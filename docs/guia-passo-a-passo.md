# Guia passo a passo — hydro-sense-iot

Fluxo completo: **WeMos D1 → EMQX Cloud (MQTT) → Node-RED → InfluxDB Cloud → Grafana**, com alertas no Telegram e previsão de chuva do OpenWeather.

## Decisão: qual banco de dados?

**InfluxDB Cloud (Serverless, plano gratuito).** É o banco de séries temporais previsto no artigo, tem nó oficial no Node-RED e fonte de dados nativa no Grafana. Os limites do plano gratuito (retenção curta, taxa de escrita) mudam com o tempo; confira no site, mas servem para o protótipo.

**Modelo de dados**

| Item | Valor |
|---|---|
| Bucket | `hydro_sense` |
| Measurement | `umidade_solo` |
| Tags | `regiao`, `ponto_id` |
| Fields | `umidade_pct` (float), `valor_bruto` (int) |
| Measurement extra | `previsao_chuva` (tag `regiao`; fields `mm_24h`, `prob_max`) |

## Estrutura final do repositório

```
hydro-sense-iot/
├── README.md
├── .gitignore
├── hydro_sense/
│   ├── hydro_sense.ino
│   └── secrets.h.example
├── node-red/
│   └── flows.json
└── docs/
    └── guia-passo-a-passo.md
```

---

## Passo 1 — Subir os arquivos para o repositório

```bash
git clone https://github.com/Nagasaki425/hydro-sense-iot.git
cd hydro-sense-iot
# copie para esta pasta: README.md, .gitignore, hydro_sense/, docs/
git status          # confira que secrets.h NÃO aparece (só secrets.h.example)
git add .
git commit -m "Firmware Wi-Fi/MQTT, README e guia"
git push
```

## Passo 2 — Broker MQTT (EMQX Cloud)

1. No console do EMQX Cloud, abra o seu deployment (*Deployment Overview*) e anote o **Address** (algo como `xxxxxxxx.ala.us-east-1.emqxsl.com`). A porta **MQTT over TLS/SSL** é **8883**.
2. Em *Access Control → Client Authentication*, crie um usuário e senha. Se o seu deployment tiver regras de autorização (ACL), permita publicar e assinar em `robo/jackal/solo/#`. (Os nomes dos menus podem variar um pouco.)
3. Baixe o **CA Certificate** (ícone de download ao lado do certificado). Ele pode ser necessário no Node-RED.
4. Teste em *Diagnostics → Online Test*: conecte com o usuário criado e assine `robo/jackal/solo/#`.
5. Confira a cota do plano no *Deployment Overview*. O firmware publica a cada 2 s; se a cota apertar, aumente o intervalo de publicação.

## Passo 3 — Gravar o firmware no WeMos

1. Arduino IDE: instale o pacote de placa **esp8266** e a biblioteca **PubSubClient**.
2. Em `hydro_sense/`, copie `secrets.h.example` para `secrets.h` e preencha Wi-Fi e broker.
3. Placa: **LOLIN(WEMOS) D1 R2 & mini**. Compile e faça o upload.
4. Monitor Serial a 115200: procure `[mqtt] conectando a ... ok`.
5. No Online Test do EMQX deve aparecer o JSON a cada ~2 s em `robo/jackal/solo/umidade` e `online` em `robo/jackal/solo/status`.

> O firmware publica a cada 2 s. O Node-RED abaixo usa cache da previsão e limite de alertas para não sobrecarregar as APIs.

## Passo 4 — Regiões (no lugar do GPS)

1. Defina os nomes das regiões da sua área, só com letras, números, `-` e `_`: `talhao-1`, `talhao-2`...
2. Pegue a latitude/longitude aproximada de cada uma (Google Maps → clique direito no mapa). Elas só servem para a previsão de chuva.
3. A região é escolhida publicando o nome, **com a opção Retain ligada**, em `robo/jackal/solo/regiao`:
   * no Online Test do EMQX (teste), ou
   * pelo dropdown do dashboard do Node-RED (Passo 6, é o que o usuário vai usar).
4. Por ser retida, o WeMos recebe a região de volta toda vez que reconecta.

## Passo 5 — InfluxDB Cloud

1. Crie a conta em cloud2.influxdata.com (plano gratuito) e anote a **URL da região** e o nome da **organização**.
2. Crie o bucket `hydro_sense`.
3. Crie **dois tokens** em *API Tokens*: um **somente escrita** no bucket (Node-RED) e um **somente leitura** (Grafana).
4. Guarde os tokens em local seguro; nunca no repositório.

## Passo 6 — Node-RED

> **Atalho:** o arquivo `node-red/flows.json` já traz todos os nós abaixo. Importe-o (*Menu → Import*), instale só `node-red-contrib-influxdb`, e configure o servidor MQTT (Address do EMQX, porta 8883, TLS, usuário e senha) e o InfluxDB. Nele, a escolha de região é uma página simples em `http://localhost:1880/regiao`, no lugar do dropdown do dashboard dos itens 10 a 12. No TLS do Node-RED, se der erro de certificado, carregue o CA Certificate baixado no passo 2 ou, no protótipo, desmarque *Verify server certificate*.

### 6.1 Instalar e iniciar

Instale o Node.js LTS e depois:

```bash
npm install -g --unsafe-perm node-red
```

Defina as chaves como variáveis de ambiente e inicie (nada de chaves dentro do fluxo):

```bash
# Linux/macOS
export OPENWEATHER_KEY="..." TELEGRAM_TOKEN="..." TELEGRAM_CHAT_ID="..."
node-red
```

```powershell
# Windows PowerShell
$env:OPENWEATHER_KEY="..."; $env:TELEGRAM_TOKEN="..."; $env:TELEGRAM_CHAT_ID="..."
node-red
```

Abra http://localhost:1880. Em *Menu → Manage palette → Install*, instale:

* `node-red-contrib-influxdb`
* `@flowfuse/node-red-dashboard` (para o dropdown de região)

### 6.2 Chaves das APIs

* **OpenWeather:** crie conta em openweathermap.org e gere uma API key (pode levar um tempo para ativar). Usaremos o endpoint gratuito `data/2.5/forecast`.
* **Telegram:** no app, converse com `@BotFather`, envie `/newbot` e guarde o token. Mande uma mensagem qualquer ao seu bot e abra `https://api.telegram.org/bot<TOKEN>/getUpdates`: o `chat.id` é o seu `TELEGRAM_CHAT_ID`.

### 6.3 Nós do fluxo (na ordem)

**Entrada**
1. `mqtt in`: servidor = o Address do EMQX, porta 8883, TLS ligado (crie uma configuração TLS padrão), usuário/senha; tópico `robo/jackal/solo/umidade`; saída *parsed JSON*.
2. `function` **Validar e enriquecer**:

```js
// Edite com as regiões reais e suas coordenadas
const REGIOES = {
  "talhao-1": { lat: -23.5500, lon: -46.6300 },
  "talhao-2": { lat: -23.5600, lon: -46.6400 }
};

const d = msg.payload;
if (typeof d !== "object" || d === null) return null;

const u = Number(d.umidade_pct);
if (!isFinite(u) || u < 0 || u > 100) { node.warn("umidade inválida"); return null; }

const reg = REGIOES[d.regiao];
if (!reg) { node.warn("região desconhecida: " + d.regiao); return null; }

msg.leitura = {
  ponto_id: d.ponto_id, regiao: d.regiao,
  umidade_pct: u, valor_bruto: d.valor_bruto,
  lat: reg.lat, lon: reg.lon
};
return msg;
```

A saída deste nó vai para **dois ramos**.

**Ramo A — gravar no InfluxDB**

3. `function` **Preparar Influx**:

```js
const l = msg.leitura;
msg.measurement = "umidade_solo";
msg.payload = [
  { umidade_pct: l.umidade_pct, valor_bruto: l.valor_bruto },  // fields
  { regiao: l.regiao, ponto_id: l.ponto_id }                   // tags
];
return msg;
```
4. `influxdb out`: versão 2.0, URL, organização, bucket `hydro_sense`, token de **escrita**.

**Ramo B — previsão de chuva e alerta**

5. `function` **Previsão (com cache)**, com **2 saídas**:

```js
const l = msg.leitura;
const TTL = 30 * 60 * 1000;               // 30 min
const cache = flow.get("chuva_" + l.regiao);
if (cache && Date.now() - cache.t < TTL) {
  msg.chuva = cache;
  return [null, msg];                      // saída 2: pula a chamada HTTP
}
msg.url = "https://api.openweathermap.org/data/2.5/forecast"
        + "?lat=" + l.lat + "&lon=" + l.lon
        + "&units=metric&cnt=8&appid=" + env.get("OPENWEATHER_KEY");
return [msg, null];                        // saída 1: consulta a API
```
6. Saída 1 → `http request`: método GET, URL vazia (usa `msg.url`), retorno *parsed JSON*.
7. `function` **Processar previsão**, com **2 saídas**:

```js
const l = msg.leitura;
const lista = (msg.payload && msg.payload.list) || [];
const mm  = lista.reduce((s, p) => s + ((p.rain && p.rain["3h"]) || 0), 0);
const pop = Math.max(0, ...lista.map(p => p.pop || 0));
const chuva = { t: Date.now(), mm: mm, pop: pop };
flow.set("chuva_" + l.regiao, chuva);
msg.chuva = chuva;

// saída 2: grava a previsão no Influx (para o Dashboard 2)
const f = {
  measurement: "previsao_chuva",
  payload: [{ mm_24h: mm, prob_max: pop }, { regiao: l.regiao }]
};
return [msg, f];
```
   * Saída 2 → `influxdb out` (o mesmo nó do passo 4).
   * Saída 1 → nó **Decidir alerta** (abaixo), junto com a saída 2 do nó 5.
8. `function` **Decidir alerta**:

```js
const l = msg.leitura, c = msg.chuva;
const LIMIAR = 30;                  // % de umidade (ajuste por cultura/solo)
const CHUVA_MM = 2;                 // mm previstos em 24 h = "chuva relevante"
const COOLDOWN = 60 * 60 * 1000;    // no máx. 1 alerta por hora por ponto

if (l.umidade_pct >= LIMIAR) return null;
if (c.mm >= CHUVA_MM) return null;

const key = "alerta_" + l.regiao + "_" + l.ponto_id;
if (Date.now() - (flow.get(key) || 0) < COOLDOWN) return null;
flow.set(key, Date.now());

msg.url = "https://api.telegram.org/bot" + env.get("TELEGRAM_TOKEN") + "/sendMessage";
msg.payload = {
  chat_id: env.get("TELEGRAM_CHAT_ID"),
  text: "⚠️ Solo seco\nRegião: " + l.regiao + " | Ponto: " + l.ponto_id +
        "\nUmidade: " + l.umidade_pct.toFixed(1) + "%" +
        "\nChuva prevista (24 h): " + c.mm.toFixed(1) + " mm\nSugestão: irrigar."
};
return msg;
```
9. `http request`: método POST, URL vazia (usa `msg.url`), retorno *parsed JSON*. Quando o payload é um objeto, o nó envia como JSON.

**Seleção da região (dashboard)**

10. `ui-dropdown` (Dashboard): opções = os mesmos nomes de `REGIOES`; ao escolher, envia o texto.
11. `mqtt out`: tópico `robo/jackal/solo/regiao`, **Retain = true**, mesmo servidor MQTT.
12. O dashboard fica em http://localhost:1880/dashboard.

### 6.4 Exportar o fluxo para o repositório

*Menu → Export → all flows → Download* e salve como `node-red/flows.json`. O export normal não inclui credenciais, mas abra o arquivo e confira que não há chave escrita em nenhum nó.

## Passo 7 — Teste ponta a ponta (sem o robô)

No Online Test do EMQX, publique (retain desligado) em `robo/jackal/solo/umidade`:

```json
{"ponto_id":"P03","regiao":"talhao-1","umidade_pct":22.5,"valor_bruto":520,"ts":null}
```

Confira, em ordem:

1. O Node-RED (aba *debug*) mostra o dado passando pelos nós.
2. O InfluxDB (*Data Explorer*) mostra o ponto em `umidade_solo`.
3. Se não houver chuva prevista, chega o alerta no Telegram (só 1 por hora por ponto).

## Passo 8 — Grafana

1. Use Grafana Cloud (plano gratuito) ou um Grafana local.
2. *Connections → Data sources → InfluxDB*: linguagem **Flux**; URL do Influx; organização; token de **leitura**; bucket padrão `hydro_sense`. (Os nomes dos menus podem variar um pouco.)

### Dashboard 1 — Monitoramento

**Gauge: umidade atual (média)**
```flux
from(bucket: "hydro_sense")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "umidade_solo" and r._field == "umidade_pct")
  |> group()
  |> mean()
```

**Stat: pontos abaixo do limiar**
```flux
from(bucket: "hydro_sense")
  |> range(start: -6h)
  |> filter(fn: (r) => r._measurement == "umidade_solo" and r._field == "umidade_pct")
  |> group(columns: ["regiao", "ponto_id"])
  |> last()
  |> filter(fn: (r) => r._value < 30.0)
  |> group()
  |> count()
```

**Time series: umidade por ponto**
```flux
from(bucket: "hydro_sense")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r._measurement == "umidade_solo" and r._field == "umidade_pct")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
```
Adicione uma linha de limiar em 30 nas opções do painel (*Thresholds*).

### Dashboard 2 — Apoio à decisão

Sem GPS, o "mapa de pontos" vira **umidade média por região**.

**Bar gauge: umidade média por região**
```flux
from(bucket: "hydro_sense")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "umidade_solo" and r._field == "umidade_pct")
  |> group(columns: ["regiao"])
  |> mean()
```

**Bar chart: chuva prevista (24 h) por região**
```flux
from(bucket: "hydro_sense")
  |> range(start: -6h)
  |> filter(fn: (r) => r._measurement == "previsao_chuva" and r._field == "mm_24h")
  |> group(columns: ["regiao"])
  |> last()
```

**Table: pontos críticos**
```flux
from(bucket: "hydro_sense")
  |> range(start: -6h)
  |> filter(fn: (r) => r._measurement == "umidade_solo" and r._field == "umidade_pct")
  |> group(columns: ["regiao", "ponto_id"])
  |> last()
  |> filter(fn: (r) => r._value < 30.0)
  |> keep(columns: ["regiao", "ponto_id", "_value"])
```

Exporte cada dashboard em JSON (*Share → Export*) e salve em `grafana/` no repositório.

## Passo 9 — Ajustes no artigo

A troca de GPS por região muda estes trechos do texto:

* Seção 1.2 e 1.4: "georreferenciados" → "regionalizados".
* Tabela 1: remover a linha do GPS.
* Seção 2.2: exemplo de JSON (sem `lat`/`lon`, com `regiao`) e a regra do Node-RED (região → coordenadas para o OpenWeather).
* Figura 4 e Tabela 2: o "mapa de pontos" vira "umidade por região".
* Seção 2.1: o WeMos publica direto no MQTT (sem o nó ROS2 intermediário).
