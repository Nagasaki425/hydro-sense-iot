# hydro-sense-iot

[![PT-BR](https://img.shields.io/badge/Idioma-Portugu%C3%AAs-green.svg)](#português)
[![EN](https://img.shields.io/badge/Language-English-blue.svg)](#english)

---

## Português

Monitoramento da umidade do solo com IoT para apoiar decisões de irrigação e reduzir o desperdício de água. Projeto acadêmico (Universidade Presbiteriana Mackenzie) que estende uma pesquisa de Iniciação Científica em agricultura de precisão de baixo custo com um robô terrestre autônomo.

### 📋 Sobre o projeto

Um único sensor de umidade do solo, transportado por um robô móvel (Clearpath Jackal, ROS2), percorre a área e amostra vários pontos, em vez de exigir um sensor fixo por ponto. As leituras são enviadas por MQTT para um fluxo de regras que cruza a umidade com a previsão de chuva, dispara alertas e alimenta dashboards. Nesta versão o sistema **apoia a decisão** de irrigar; não há atuador.

### 🌱 ODS e Sustentabilidade

O projeto contribui para a ODS 6 – Água Potável e Saneamento, ao promover o uso mais eficiente da água na agricultura. Por meio do monitoramento da umidade do solo e da análise da previsão de chuva, o protótipo auxilia na tomada de decisões sobre irrigação, evitando o uso desnecessário de água. Nesta versão, o sistema oferece apoio à decisão, sem realizar o acionamento automático da irrigação.

### 🔄 Arquitetura

```
Sensor YL-38 → WeMos D1 (ESP8266) → Wi-Fi → Broker MQTT (EMQX Cloud)
                                                  │
                                              Node-RED ──→ OpenWeather (previsão de chuva)
                                                  │    ──→ Telegram (alerta de solo seco)
                                                  └────→ InfluxDB Cloud → Grafana
```

Neste repositório, o WeMos D1 publica o JSON diretamente no broker via Wi-Fi. A saída serial continua disponível, então o caminho alternativo (microcontrolador → nó ROS2 → MQTT) também é possível.

### ✅ Status

| Componente | Status |
|---|---|
| Leitura do sensor com calibração em dois pontos (0–100%) | ✅ Implementado |
| Média móvel (5 amostras / 2 s) e buffer circular com mín/máx das últimas 3 médias | ✅ Implementado |
| Conexão Wi-Fi, sincronização NTP e publicação MQTT (JSON, TLS opcional) | ✅ Implementado (firmware) |
| Seleção do ponto de coleta (`ponto_id`) via MQTT | ✅ Implementado |
| Região escolhida pelo usuário (substitui o GPS) | ✅ Implementado (firmware) |
| Regras no Node-RED (validação, OpenWeather, alerta Telegram) | ⏳ Previsto |
| Armazenamento no InfluxDB Cloud | ⏳ Previsto |
| Dashboards no Grafana | ⏳ Previsto |

### 📡 Formato dos dados

Tópico de telemetria: `robo/jackal/solo/umidade`

```json
{
  "ponto_id": "P03",
  "regiao": "talhao-1",
  "umidade_pct": 27.4,
  "valor_bruto": 712,
  "ts": "2026-11-23T20:15:10Z"
}
```

`regiao` é escolhida pelo usuário (não há GPS); use só letras, números, `-` e `_`. As coordenadas de cada região ficam no Node-RED e servem para consultar a previsão de chuva. `ts` é `null` até o NTP sincronizar.

| Tópico | Direção | Uso |
|---|---|---|
| `robo/jackal/solo/umidade` | publica | Telemetria (JSON) |
| `robo/jackal/solo/status` | publica (retido) | `online` / `offline` (Last Will) |
| `robo/jackal/solo/ponto` | assina | Define o `ponto_id` atual (ex.: `P05`) |
| `robo/jackal/solo/regiao` | assina (retido) | Região escolhida pelo usuário (ex.: `talhao-2`) |

### 🛠️ Tecnologias

* **Microcontrolador:** WeMos D1 (ESP8266), escolhido por ter Wi-Fi integrado
* **Sensor:** módulo de umidade do solo YL-38 (sonda resistiva, saída analógica)
* **Comunicação:** Wi-Fi, MQTT (PubSubClient), payload JSON
* **Linguagem:** C/C++ (Arduino Framework)

### 🚀 Como usar

1. Clone o repositório:
   ```bash
   git clone https://github.com/Nagasaki425/hydro-sense-iot.git
   ```
2. Na Arduino IDE, instale o pacote de placa **esp8266** e a biblioteca **PubSubClient**.
3. Entre em `hydro_sense/`, copie `secrets.h.example` para `secrets.h` e preencha Wi-Fi e credenciais do broker. O `secrets.h` está no `.gitignore`.
4. Selecione a placa **LOLIN(WEMOS) D1 R2 & mini**, compile e faça o upload.
5. Abra o Monitor Serial a 115200 baud.

### 🎯 Calibração

Ajuste em `hydro_sense.ino`:

* `DRY_SOIL_VALUE`: leitura bruta com a sonda seca ao ar (0%).
* `WET_SOIL_VALUE`: leitura bruta com a sonda em solo saturado (100%).

Anote o valor `Raw(med)` no Monitor Serial em cada condição.

### 🔒 Segurança

Chaves e senhas ficam fora do repositório (`secrets.h`). Em TLS, o firmware não valida o certificado do broker (`setInsecure()`), aceitável no protótipo; para produção, configure âncoras de confiança.

---

## English

Soil moisture monitoring with IoT to support irrigation decisions and cut water waste. Academic project (Mackenzie Presbyterian University) extending an undergraduate research project on low-cost precision agriculture with an autonomous ground robot.

### 📋 About

A single soil moisture sensor carried by a mobile robot (Clearpath Jackal, ROS2) sweeps the field and samples multiple points, instead of requiring one fixed sensor per point. Readings are sent over MQTT to a rules flow that combines moisture with the rain forecast, triggers alerts and feeds dashboards. This version provides **decision support** only; there is no actuator.

### 🌱 SDGs and Sustainability

The project contributes to SDG 6 – Clean Water and Sanitation by promoting more efficient water use in agriculture. By monitoring soil moisture and analyzing rainfall forecasts, the prototype supports irrigation decisions and helps prevent unnecessary water consumption. In this version, the system provides decision support without automatically controlling irrigation.

### 🔄 Architecture

```
YL-38 sensor → WeMos D1 (ESP8266) → Wi-Fi → MQTT broker (EMQX Cloud)
                                                  │
                                              Node-RED ──→ OpenWeather (rain forecast)
                                                  │    ──→ Telegram (dry-soil alert)
                                                  └────→ InfluxDB Cloud → Grafana
```

In this repository the WeMos D1 publishes JSON straight to the broker over Wi-Fi. Serial output is still available, so the alternative path (microcontroller → ROS2 node → MQTT) remains possible.

### ✅ Status

| Component | Status |
|---|---|
| Sensor reading with two-point calibration (0–100%) | ✅ Done |
| Moving average (5 samples / 2 s) and circular buffer with min/max of the last 3 averages | ✅ Done |
| Wi-Fi, NTP sync and MQTT publishing (JSON, optional TLS) | ✅ Done (firmware) |
| Collection point selection (`ponto_id`) via MQTT | ✅ Done |
| User-selected region (replaces GPS) | ✅ Done (firmware) |
| Node-RED rules (validation, OpenWeather, Telegram alert) | ⏳ Planned |
| InfluxDB Cloud storage | ⏳ Planned |
| Grafana dashboards | ⏳ Planned |

### 📡 Data format

Telemetry topic: `robo/jackal/solo/umidade`

```json
{
  "ponto_id": "P03",
  "regiao": "talhao-1",
  "umidade_pct": 27.4,
  "valor_bruto": 712,
  "ts": "2026-11-23T20:15:10Z"
}
```

`regiao` is chosen by the user (no GPS); use only letters, numbers, `-` and `_`. Each region's coordinates live in Node-RED and are used to fetch the rain forecast. `ts` is `null` until NTP syncs.

| Topic | Direction | Purpose |
|---|---|---|
| `robo/jackal/solo/umidade` | publish | Telemetry (JSON) |
| `robo/jackal/solo/status` | publish (retained) | `online` / `offline` (Last Will) |
| `robo/jackal/solo/ponto` | subscribe | Sets the current `ponto_id` (e.g. `P05`) |
| `robo/jackal/solo/regiao` | subscribe (retained) | User-selected region (e.g. `talhao-2`) |

### 🛠️ Tech stack

* **Microcontroller:** WeMos D1 (ESP8266), chosen for built-in Wi-Fi
* **Sensor:** YL-38 soil moisture module (resistive probe, analog output)
* **Communication:** Wi-Fi, MQTT (PubSubClient), JSON payload
* **Language:** C/C++ (Arduino Framework)

### 🚀 Getting started

1. Clone the repository:
   ```bash
   git clone https://github.com/Nagasaki425/hydro-sense-iot.git
   ```
2. In the Arduino IDE, install the **esp8266** board package and the **PubSubClient** library.
3. Go to `hydro_sense/`, copy `secrets.h.example` to `secrets.h` and fill in Wi-Fi and broker credentials. `secrets.h` is in `.gitignore`.
4. Select the **LOLIN(WEMOS) D1 R2 & mini** board, compile and upload.
5. Open the Serial Monitor at 115200 baud.

### 🎯 Calibration

Adjust in `hydro_sense.ino`:

* `DRY_SOIL_VALUE`: raw reading with the probe dry in air (0%).
* `WET_SOIL_VALUE`: raw reading with the probe in saturated soil (100%).

Note the `Raw(med)` value in the Serial Monitor under each condition.

### 🔒 Security

Keys and passwords stay out of the repository (`secrets.h`). With TLS, the firmware does not validate the broker certificate (`setInsecure()`), acceptable for a prototype; configure trust anchors for production.
