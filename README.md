# 🌊 Monitoramento de Enchentes em Áreas de Risco com IoT

Sistema embarcado de baixo custo para **monitoramento do nível da água** em áreas urbanas de risco, baseado em **ESP32**, sensor ultrassônico **HC-SR04** e protocolo **MQTT**. O dispositivo mede o nível da água em tempo real, emite alerta sonoro local quando o nível atinge a zona de perigo e publica os dados na nuvem para visualização remota.

> Projeto acadêmico — Universidade Presbiteriana Mackenzie (UPM), Faculdade de Computação e Informática.

---

## 📑 Sumário

- [Visão geral](#-visão-geral)
- [Arquitetura](#-arquitetura)
- [Hardware](#-hardware)
- [Mapeamento de conexões](#-mapeamento-de-conexões)
- [Lógica de operação](#-lógica-de-operação)
- [Software e bibliotecas](#-software-e-bibliotecas)
- [Como executar no Wokwi](#-como-executar-no-wokwi)
- [Como testar os estados](#-como-testar-os-estados)
- [Formato das mensagens MQTT](#-formato-das-mensagens-mqtt)
- [Como visualizar os dados publicados](#-como-visualizar-os-dados-publicados)
- [Estrutura do repositório](#-estrutura-do-repositório)
- [Configurações ajustáveis](#-configurações-ajustáveis)
- [Solução de problemas](#-solução-de-problemas)
- [Autores](#-autores)
- [Licença](#-licença)

---

## 🔎 Visão geral

O crescimento urbano desordenado, somado à impermeabilização do solo e à ocupação de áreas próximas a rios e córregos, torna as enchentes um dos problemas mais recorrentes nas cidades brasileiras. Este projeto propõe uma **unidade de sensoriamento de borda (Edge Computing)** que monitora continuamente o nível da água e envia alertas antecipados, tanto localmente (sonoro) quanto remotamente (via MQTT para um dashboard).

**Principais características:**

- Medição de nível por sensor ultrassônico, com precisão de centímetros.
- Classificação automática em três zonas: **NORMAL**, **ATENÇÃO** e **PERIGO**.
- Alerta sonoro local imediato, funcionando mesmo sem internet.
- Publicação de dados em JSON via MQTT para monitoramento em larga escala.
- Totalmente simulável no **Wokwi**, sem necessidade de hardware físico.

---

## 🏗 Arquitetura

```
┌─────────────┐   pulso/eco    ┌──────────┐   Wi-Fi / MQTT   ┌────────────┐
│  HC-SR04    │ ─────────────► │  ESP32   │ ───────────────► │   Broker   │
│ (nível água)│                │ (Edge)   │                  │  (HiveMQ)  │
└─────────────┘                └────┬─────┘                  └─────┬──────┘
                                    │ GPIO 19                      │
                                    ▼                              ▼
                               ┌─────────┐                  ┌────────────┐
                               │ Buzzer  │                  │ Dashboard /│
                               │ (alerta)│                  │  Assinantes│
                               └─────────┘                  └────────────┘
```

O ESP32 atua como **Publisher** no modelo Publish/Subscribe do MQTT. As leituras são publicadas em um tópico específico e distribuídas pelo broker a todos os assinantes (por exemplo, um painel de monitoramento municipal).

---

## 🔧 Hardware

| Componente | Descrição |
|---|---|
| **ESP32 DevKit V1** | Microcontrolador dual-core 32 bits, Wi-Fi 802.11 b/g/n integrado. Cérebro do sistema. |
| **Sensor HC-SR04** | Sensor ultrassônico que mede a distância entre o ponto de instalação e a superfície da água. |
| **Buzzer Ativo 5V** | Atuador sonoro (~2,5 kHz) acionado na zona de perigo. |
| **Protoboard + Jumpers** | Interconexão sem solda (apenas na montagem física real). |

> Na simulação Wokwi, protoboard e jumpers são dispensáveis — as conexões são feitas diretamente no `diagram.json`.

---

## 🔌 Mapeamento de conexões

| Componente | Pino do componente | Pino do ESP32 | Função |
|---|---|---|---|
| HC-SR04 | VCC | 5V (VIN) | Alimentação |
| HC-SR04 | GND | GND | Aterramento |
| HC-SR04 | Trig | GPIO 5 | Gatilho de disparo (pulso 10 µs) |
| HC-SR04 | Echo | GPIO 18 | Recebimento do eco |
| Buzzer | Positivo (+) | GPIO 19 | Acionamento do alerta sonoro |
| Buzzer | Negativo (−) | GND | Aterramento |

> Em uma montagem física, o buzzer pode ser ligado por meio de um transistor NPN (ex.: BC547) para proteger o pino de I/O caso a corrente ultrapasse ~12 mA. No simulador, a ligação direta é suficiente.

---

## ⚙️ Lógica de operação

1. A cada **30 segundos**, o ESP32 envia um pulso de 10 µs ao pino **Trig** do HC-SR04.
2. O sensor emite pulsos ultrassônicos e o ESP32 mede o tempo do eco no pino **Echo**.
3. A distância é calculada pela fórmula `d = (t × v) / 2`, onde `v ≈ 343 m/s` (velocidade do som no ar a 20 °C).
4. O nível da água é obtido por `nível = altura_do_canal − distância`.
5. O sistema classifica a leitura em zonas, por **percentual da altura do canal**:

| Estado | Condição | Ação do buzzer |
|---|---|---|
| 🟢 **NORMAL** | nível < 60% | Silencioso |
| 🟡 **ATENÇÃO** | 60% ≤ nível < 80% | Bipe intermitente curto |
| 🔴 **PERIGO** | nível ≥ 80% | Sinal sonoro contínuo (~2,5 kHz) |

6. Simultaneamente, os dados são empacotados em **JSON** e publicados via MQTT.

---

## 💻 Software e bibliotecas

- **Linguagem:** C++ (framework Arduino)
- **IDE / Simulador:** Wokwi (ou Arduino IDE para hardware físico)

| Biblioteca | Finalidade |
|---|---|
| `PubSubClient` | Cliente MQTT (publicação das leituras) |
| `ArduinoJson` | Serialização das mensagens em JSON |
| `WiFi.h` | Conexão Wi-Fi (nativa do ESP32) |

> A leitura do HC-SR04 é feita com `pulseIn` nativo, aplicando a mesma fórmula `d = (t × v) / 2`. Isso garante compatibilidade total com o simulador Wokwi e dispensa bibliotecas adicionais de sensor.

---

## ▶️ Como executar no Wokwi

1. Acesse [wokwi.com](https://wokwi.com) e crie um novo projeto **ESP32**.
2. Substitua o conteúdo da aba `sketch.ino` pelo arquivo deste repositório.
3. Substitua o conteúdo da aba `diagram.json` pelo arquivo deste repositório.
4. Abra o **Library Manager** (gerenciador de bibliotecas) e adicione:
   - `PubSubClient`
   - `ArduinoJson`
5. Clique em **▶ (Play)** para iniciar a simulação.

Ao iniciar, o monitor serial mostrará a conexão ao Wi-Fi (`Wokwi-GUEST`), a conexão ao broker MQTT e as publicações periódicas.

---

## 🧪 Como testar os estados

Com a simulação rodando, **clique no sensor HC-SR04** para abrir o controle de distância e arraste o valor:

| Distância no sensor | Nível da água | Estado resultante |
|---|---|---|
| 70–100 cm | 0–30 cm | 🟢 NORMAL |
| 20–40 cm | 60–80 cm | 🟡 ATENÇÃO |
| 0–20 cm | 80–100 cm | 🔴 PERIGO |

> O `diagram.json` inicia com distância de **60 cm** (estado NORMAL), para que a simulação comece silenciosa.

---

## 📡 Formato das mensagens MQTT

- **Broker:** `broker.hivemq.com:1883`
- **Tópico:** `cidade/bairro/sensor01/nivel`

Exemplo de payload publicado:

```json
{
  "sensor": "sensor01",
  "local": "cidade/bairro",
  "distancia_cm": 40.0,
  "nivel_cm": 60.0,
  "percentual": 60.0,
  "estado": "ATENCAO",
  "timestamp_ms": 30000
}
```

---

## 👀 Como visualizar os dados publicados

1. Acesse um cliente MQTT web, como o **HiveMQ WebSocket Client**.
2. Conecte ao broker `broker.hivemq.com`.
3. Assine (Subscribe) o tópico:
   ```
   cidade/bairro/sensor01/nivel
   ```
4. As mensagens JSON aparecerão a cada ciclo de leitura.

---

## 📂 Estrutura do repositório

```
.
├── sketch.ino        # Firmware principal em C++ (ESP32)
├── diagram.json      # Diagrama de montagem do circuito (Wokwi)
├── libraries.txt     # Lista de bibliotecas usadas pelo Wokwi
└── README.md         # Esta documentação
```

---

## 🎛 Configurações ajustáveis

Edite as constantes no topo do `sketch.ino` conforme a necessidade:

```cpp
const float ALTURA_CANAL_CM   = 100.0;   // altura do canal/bueiro monitorado
const float PCT_ATENCAO       = 0.60;    // limite da zona de atenção (60%)
const float PCT_PERIGO        = 0.80;    // limite da zona de perigo (80%)
const unsigned long INTERVALO_LEITURA = 30000;  // intervalo entre leituras (ms)
```

> 💡 Para testar mais rápido no simulador, reduza `INTERVALO_LEITURA` para `5000` (5 segundos).

Configurações de rede e MQTT (também no topo do arquivo):

```cpp
const char* WIFI_SSID   = "Wokwi-GUEST";
const char* MQTT_BROKER = "broker.hivemq.com";
const char* MQTT_TOPICO = "cidade/bairro/sensor01/nivel";
```

---

## 🛠 Solução de problemas

| Problema | Causa provável | Solução |
|---|---|---|
| `PubSubClient.h: No such file or directory` | Biblioteca não adicionada ao projeto | Adicione `PubSubClient` e `ArduinoJson` pelo Library Manager do Wokwi |
| Buzzer apita assim que liga | Distância inicial do sensor está em zona de atenção/perigo | Aumente a distância no slider do sensor (acima de 60 cm para NORMAL) |
| Não conecta ao MQTT (`rc=-2`) | Broker ocupado ou instável | Aguarde alguns segundos; o código tenta reconectar automaticamente |
| Sensor retorna leitura inválida | Timeout do eco | Verifique as ligações de Trig (GPIO 5) e Echo (GPIO 18) no `diagram.json` |

---

## 👥 Autores

- Enzo Thomas Vecchione
- Guilherme Dantas
- Fabrício Hideki Nascimento Aoyagi
- André Luis de Oliveira

Universidade Presbiteriana Mackenzie (UPM) — São Paulo, SP — Brasil.

---

## 📄 Licença

Projeto desenvolvido para fins acadêmicos. Sinta-se livre para estudar, adaptar e reutilizar o código citando os autores.
