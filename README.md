# hydro-sense-iot

[![PT-BR](https://img.shields.io/badge/Idioma-Portugu%C3%AAs-green.svg)](#portugu%C3%AAs)
[![EN](https://img.shields.io/badge/Language-English-blue.svg)](#english)

---

## Português

Sistema IoT de monitoramento agrícola para detecção de chuvas e umidade do solo, otimizando a irrigação e reduzindo o desperdício de água.

### 📋 Sobre o Projeto

O **hydro-sense-iot** é uma solução de Internet das Coisas (IoT) desenvolvida para o monitoramento em tempo real de áreas de plantio. O sistema coleta, filtra e processa dados ambientais de umidade do solo e precipitação para fornecer telemetria precisa e alertas, evitando irrigações desnecessárias e promovendo o uso sustentável dos recursos hídricos.

### ✨ Funcionalidades

* **Monitoramento de Umidade do Solo:** Leitura contínua da umidade volumétrica da terra com filtragem de ruído (média móvel) para maior estabilidade dos dados.
* **Detecção de Chuvas:** Identificação imediata de precipitação na área monitorada para suspensão automática ou manual da irrigação.
* **Telemetria em Tempo Real:** Formatação e envio dos dados estruturados (JSON) para integração com dashboards online e bancos de dados.
* **Alertas Inteligentes:** Notificações baseadas em limiares calibrados (solo seco vs. solo saturado) para auxiliar na tomada de decisão do produtor.

### 🛠️ Tecnologias e Arquitetura

* **Microcontroladores:** Arduino Mega / WeMos D1 (ESP8266)
* **Sensores:** Sensor de Umidade do Solo (capacitivo/resistivo) e Sensor de Chuva
* **Comunicação e Dados:** Wi-Fi, Serial e payload em formato JSON
* **Linguagem:** C/C++ (Arduino Framework)

### 🚀 Como Utilizar

1. Clone este repositório:
   ```bash
   git clone [https://github.com/seu-usuario/hydro-sense-iot.git](https://github.com/seu-usuario/hydro-sense-iot.git)
   ```
2. Abra o código-fonte na IDE de sua preferência (Arduino IDE ou PlatformIO).
3. Ajuste as credenciais de rede Wi-Fi e os valores de calibração dos sensores conforme o tipo de solo.
4. Faça o upload do firmware para a placa e acompanhe as leituras via Monitor Serial ou dashboard integrado.

---

## English

IoT agricultural monitoring system for rain detection and soil moisture tracking, designed to optimize irrigation and save water.

### 📋 About the Project

**hydro-sense-iot** is an Internet of Things (IoT) solution designed for real-time monitoring of crop fields. The system collects, filters, and processes environmental data on soil moisture and rainfall to deliver accurate telemetry and alerts, preventing unnecessary irrigation and promoting sustainable water management.

### ✨ Features

* **Soil Moisture Monitoring:** Continuous reading of soil moisture levels with noise filtering (moving average) for stable measurements.
* **Rain Detection:** Immediate rainfall identification in the monitored area to halt scheduled or manual irrigation.
* **Real-Time Telemetry:** Structured data formatting (JSON) ready for integration with online dashboards and databases.
* **Smart Alerts:** Threshold-based notifications (calibrated for dry vs. saturated soil) to support data-driven irrigation decisions.

### 🛠️ Tech Stack & Architecture

* **Microcontrollers:** Arduino Mega / WeMos D1 (ESP8266)
* **Sensors:** Soil Moisture Sensor (capacitive/resistive) and Rain Drop Sensor
* **Communication & Data:** Wi-Fi, Serial, and JSON payload formatting
* **Language:** C/C++ (Arduino Framework)

### 🚀 Getting Started

1. Clone this repository:
   ```bash
   git clone [https://github.com/your-username/hydro-sense-iot.git](https://github.com/your-username/hydro-sense-iot.git)
   ```
2. Open the source code in your preferred IDE (Arduino IDE or PlatformIO).
3. Configure your Wi-Fi credentials and adjust sensor calibration thresholds according to your soil type.
4. Flash the firmware to your microcontroller and monitor the telemetry via Serial Monitor or your connected dashboard.
