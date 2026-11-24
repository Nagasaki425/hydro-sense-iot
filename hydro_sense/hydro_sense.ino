/**
 * @file hydro_sense.ino
 * @brief Monitoramento de umidade do solo com WeMos D1 (ESP8266): leitura,
 *        média móvel, buffer circular (mín/máx) e telemetria JSON via MQTT.
 * @board WeMos D1 (ESP8266)
 *
 * Bibliotecas (Gerenciador de Bibliotecas da Arduino IDE):
 *   - PubSubClient (Nick O'Leary)
 * Pacote de placa: "esp8266" (Gerenciador de Placas).
 *
 * Antes de compilar: copie secrets.h.example para secrets.h e preencha.
 */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include "secrets.h"

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
const int SOIL_SENSOR_PIN = A0;

// Calibração manual em dois pontos (YL-38 / sonda resistiva)
const int DRY_SOIL_VALUE = 573;  // 0% de umidade (sonda seca ao ar)
const int WET_SOIL_VALUE = 341;  // 100% de umidade (sonda em solo saturado/água)
static_assert(DRY_SOIL_VALUE != WET_SOIL_VALUE,
              "DRY_SOIL_VALUE e WET_SOIL_VALUE devem ser diferentes");

// ---------------------------------------------------------------------------
// Amostragem e filtragem
// ---------------------------------------------------------------------------
const int NUM_SAMPLES = 5;
const unsigned long SAMPLE_INTERVAL_MS = 400;  // 400 ms * 5 amostras = 2 s por média
const int HISTORY_SIZE = 3;

// ---------------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------------
const char* MQTT_TOPIC_DATA = "robo/jackal/solo/umidade";   // telemetria (JSON)
const char* MQTT_TOPIC_STATUS = "robo/jackal/solo/status";  // "online"/"offline" (retido)
const char* MQTT_TOPIC_POINT = "robo/jackal/solo/ponto";    // assinatura: define o ponto_id atual
const char* MQTT_TOPIC_REGION = "robo/jackal/solo/regiao";  // assinatura (retida): região escolhida pelo usuário
const unsigned long MQTT_RETRY_MS = 5000;

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
unsigned long previousMillis = 0;
int sampleCount = 0;
float moistureSum = 0.0;
long rawSum = 0;

float recentAverages[HISTORY_SIZE] = {0.0, 0.0, 0.0};
int historyIndex = 0;
int recordedAverages = 0;

char pointId[8] = "P01";                 // pode ser alterado publicando em MQTT_TOPIC_POINT
char regionId[24] = "nao-definida";      // definido publicando (retido) em MQTT_TOPIC_REGION

WiFiClient plainClient;
BearSSL::WiFiClientSecure secureClient;
PubSubClient mqtt;

unsigned long lastMqttAttempt = 0;
bool firstMqttAttempt = true;

// ---------------------------------------------------------------------------
// Sensor
// ---------------------------------------------------------------------------
float readSoilMoisturePercentage(int &rawOutput) {
  rawOutput = analogRead(SOIL_SENSOR_PIN);
  float moisture = ((float)(DRY_SOIL_VALUE - rawOutput) / (DRY_SOIL_VALUE - WET_SOIL_VALUE)) * 100.0;
  return constrain(moisture, 0.0, 100.0);
}

void updateHistoryStats(float newAverage, float &maxAvg, float &minAvg) {
  recentAverages[historyIndex] = newAverage;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  if (recordedAverages < HISTORY_SIZE) {
    recordedAverages++;
  }

  maxAvg = recentAverages[0];
  minAvg = recentAverages[0];

  for (int i = 1; i < recordedAverages; i++) {
    if (recentAverages[i] > maxAvg) maxAvg = recentAverages[i];
    if (recentAverages[i] < minAvg) minAvg = recentAverages[i];
  }
}

// ---------------------------------------------------------------------------
// Rede / MQTT
// ---------------------------------------------------------------------------
// Copia o payload para dest mantendo só [A-Za-z0-9_-] (sem espaços/acentos).
// Retorna false (e não altera dest) se o resultado ficar vazio.
bool copySanitized(char* dest, size_t destSize, const byte* payload, unsigned int length) {
  char tmp[24];
  size_t limit = destSize < sizeof(tmp) ? destSize : sizeof(tmp);
  size_t n = 0;
  for (unsigned int i = 0; i < length && n < limit - 1; i++) {
    char c = (char)payload[i];
    bool valid = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                 (c >= 'a' && c <= 'z') || c == '_' || c == '-';
    if (valid) tmp[n++] = c;
  }
  if (n == 0) return false;
  tmp[n] = '\0';
  strcpy(dest, tmp);
  return true;
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, MQTT_TOPIC_POINT) == 0) {
    if (copySanitized(pointId, sizeof(pointId), payload, length)) {
      Serial.print("[mqtt] ponto_id atualizado: ");
      Serial.println(pointId);
    }
  } else if (strcmp(topic, MQTT_TOPIC_REGION) == 0) {
    if (copySanitized(regionId, sizeof(regionId), payload, length)) {
      Serial.print("[mqtt] regiao atualizada: ");
      Serial.println(regionId);
    }
  }
}

// Gera o valor JSON do campo "ts": string ISO 8601 UTC entre aspas, ou null se o NTP ainda não sincronizou.
void buildTimestampField(char* out, size_t size) {
  time_t now = time(nullptr);
  if (now < 1700000000) {  // relógio ainda não sincronizado
    snprintf(out, size, "null");
    return;
  }
  struct tm tmUtc;
  gmtime_r(&now, &tmUtc);
  char iso[24];
  strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
  snprintf(out, size, "\"%s\"", iso);
}

bool publishReading(float moisturePct, int rawValue) {
  if (!mqtt.connected()) return false;

  char ts[32];
  buildTimestampField(ts, sizeof(ts));

  // Sem GPS: a localização é a "regiao" escolhida pelo usuário. As coordenadas
  // de cada região (para a previsão de chuva) ficam no Node-RED.
  char payload[200];
  snprintf(payload, sizeof(payload),
           "{\"ponto_id\":\"%s\",\"regiao\":\"%s\",\"umidade_pct\":%.1f,"
           "\"valor_bruto\":%d,\"ts\":%s}",
           pointId, regionId, moisturePct, rawValue, ts);

  return mqtt.publish(MQTT_TOPIC_DATA, payload);
}

// Não bloqueante: se não houver rede/broker, a amostragem continua normalmente.
void maintainConnection() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }

  unsigned long now = millis();
  if (!firstMqttAttempt && now - lastMqttAttempt < MQTT_RETRY_MS) return;
  firstMqttAttempt = false;
  lastMqttAttempt = now;

  String clientId = "hydro-sense-" + String(ESP.getChipId(), HEX);
  Serial.print("[mqtt] conectando a ");
  Serial.print(MQTT_HOST);
  Serial.print("... ");

  bool ok = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                         MQTT_TOPIC_STATUS, 1, true, "offline");
  if (ok) {
    Serial.println("ok");
    mqtt.publish(MQTT_TOPIC_STATUS, "online", true);
    mqtt.subscribe(MQTT_TOPIC_POINT);
    mqtt.subscribe(MQTT_TOPIC_REGION);  // mensagem retida: chega logo após conectar
  } else {
    Serial.print("falhou (rc=");
    Serial.print(mqtt.state());
    Serial.println(")");
  }
}

// ---------------------------------------------------------------------------
// Arduino
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[hydro-sense-iot] Monitor de umidade do solo iniciado");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC

#if MQTT_USE_TLS
  // Protótipo: não valida o certificado do broker. Para produção, use
  // secureClient.setTrustAnchors(...) ou setFingerprint(...).
  secureClient.setInsecure();
  secureClient.setBufferSizes(1024, 512);
  mqtt.setClient(secureClient);
#else
  mqtt.setClient(plainClient);
#endif
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(256);
  mqtt.setSocketTimeout(5);
}

void loop() {
  maintainConnection();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= SAMPLE_INTERVAL_MS) {
    previousMillis = currentMillis;

    int rawReading = 0;
    float currentMoisture = readSoilMoisturePercentage(rawReading);

    moistureSum += currentMoisture;
    rawSum += rawReading;
    sampleCount++;

    if (sampleCount >= NUM_SAMPLES) {
      float currentAverage = moistureSum / NUM_SAMPLES;
      int rawAverage = (int)(rawSum / NUM_SAMPLES);
      float maxAverage = 0.0;
      float minAverage = 0.0;

      updateHistoryStats(currentAverage, maxAverage, minAverage);

      bool published = publishReading(currentAverage, rawAverage);

      // Telemetria local (Serial Monitor)
      Serial.print("Ponto: ");
      Serial.print(pointId);
      Serial.print(" | Regiao: ");
      Serial.print(regionId);
      Serial.print(" | Raw(med): ");
      Serial.print(rawAverage);
      Serial.print(" | Avg: ");
      Serial.print(currentAverage, 1);
      Serial.print("% | Max (Last 3): ");
      Serial.print(maxAverage, 1);
      Serial.print("% | Min (Last 3): ");
      Serial.print(minAverage, 1);
      Serial.print("% | MQTT: ");
      Serial.println(published ? "enviado" : "offline");

      moistureSum = 0.0;
      rawSum = 0;
      sampleCount = 0;
    }
  }
}
