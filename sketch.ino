/*
 * ==================================================================
 *  Monitoramento de Enchentes em Areas de Risco com IoT
 *  Universidade Presbiteriana Mackenzie - FCI
 * ==================================================================
 *  ESP32 + HC-SR04 + Buzzer Ativo 5V + MQTT
 *
 *  Le o nivel da agua com sensor ultrassonico, classifica em
 *  NORMAL / ATENCAO / PERIGO conforme o percentual da altura do
 *  canal, aciona o buzzer e publica os dados em JSON via MQTT.
 *
 *  Conexoes (Tabela 1 do artigo):
 *    HC-SR04 VCC  -> 5V (VIN)
 *    HC-SR04 GND  -> GND
 *    HC-SR04 Trig -> GPIO 5
 *    HC-SR04 Echo -> GPIO 18
 *    Buzzer (sinal, pino 2) -> GPIO 19
 *    Buzzer (GND, pino 1)   -> GND
 *
 *  Bibliotecas (Library Manager do Wokwi):
 *    - PubSubClient (Nick O'Leary)
 *    - ArduinoJson  (Benoit Blanchon)
 * ==================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ----------------- CONFIGURACAO DE PINOS -----------------
#define PINO_TRIG   5     // Trigger do HC-SR04
#define PINO_ECHO   18    // Echo do HC-SR04
#define PINO_BUZZER 19    // Buzzer ativo (conforme artigo)
#define PINO_LED    2     // LED onboard (indicador visual)

// ----------------- PARAMETROS DO CANAL -------------------
const float ALTURA_CANAL_CM = 100.0;   // altura do canal monitorado

// Zonas de risco por PERCENTUAL da altura (vide artigo):
const float PCT_ATENCAO = 0.60;   // 60% -> zona de atencao
const float PCT_PERIGO  = 0.80;   // 80% -> zona de perigo

// ----------------- CONFIGURACAO WIFI ---------------------
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// ----------------- CONFIGURACAO MQTT ---------------------
const char* MQTT_BROKER = "broker.hivemq.com";
const int   MQTT_PORTA  = 1883;
const char* MQTT_TOPICO = "cidade/bairro/sensor01/nivel";
const char* MQTT_CLIENT_ID = "esp32-enchente-mackenzie-01";

// Intervalo entre leituras/publicacoes.
// Artigo: 30 s. Reduzido para 5 s para facilitar a demonstracao.
const unsigned long INTERVALO_LEITURA = 5000;

WiFiClient espClient;
PubSubClient mqtt(espClient);
unsigned long ultimaLeitura = 0;
String estadoAtual = "NORMAL";   // estado vigente (atualizado a cada leitura)

// ==================================================================
//  Mede a distancia em cm usando o HC-SR04 (formula d = t*v/2)
// ==================================================================
float medirDistanciaCM() {
  digitalWrite(PINO_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PINO_TRIG, HIGH);     // pulso de disparo de 10 us
  delayMicroseconds(10);
  digitalWrite(PINO_TRIG, LOW);

  long duracao = pulseIn(PINO_ECHO, HIGH, 30000);  // timeout 30 ms
  if (duracao == 0) {
    return -1.0;
  }
  return (duracao * 0.0343) / 2.0;   // v = 343 m/s; ida e volta /2
}

// ==================================================================
//  Conexao WiFi
// ==================================================================
void conectarWiFi() {
  Serial.print("Conectando ao WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi conectado. IP: ");
  Serial.println(WiFi.localIP());
}

// ==================================================================
//  Conexao / reconexao ao broker MQTT
// ==================================================================
void conectarMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Conectando ao broker MQTT...");
    if (mqtt.connect(MQTT_CLIENT_ID)) {
      Serial.println(" conectado!");
    } else {
      Serial.print(" falhou (rc=");
      Serial.print(mqtt.state());
      Serial.println("). Nova tentativa em 2s.");
      delay(2000);
    }
  }
}

// ==================================================================
//  Padrao sonoro do buzzer conforme o estado.
//  Chamado a CADA ciclo do loop, para que o alarme seja
//  continuo enquanto o nivel permanecer critico.
// ==================================================================
void tocarAlertaSonoro(const String& estado) {
  if (estado == "PERIGO") {
    // Sirene: bipes rapidos e repetidos (~2,5 kHz)
    for (int i = 0; i < 4; i++) {
      tone(PINO_BUZZER, 2500);
      delay(150);
      noTone(PINO_BUZZER);
      delay(80);
    }
  } else if (estado == "ATENCAO") {
    // Bipe unico mais grave
    tone(PINO_BUZZER, 1500);
    delay(300);
    noTone(PINO_BUZZER);
  } else {
    noTone(PINO_BUZZER);   // NORMAL = silencio
  }
}

// ==================================================================
//  "Aquecimento" do buzzer: o primeiro tom no Wokwi costuma nao
//  ser ouvido, entao disparamos um tom curto e inaudivel no setup.
// ==================================================================
void aquecerBuzzer() {
  tone(PINO_BUZZER, 100);
  delay(60);
  noTone(PINO_BUZZER);
  delay(40);
}

// ==================================================================
//  SETUP
// ==================================================================
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(PINO_TRIG, OUTPUT);
  pinMode(PINO_ECHO, INPUT);
  pinMode(PINO_BUZZER, OUTPUT);
  pinMode(PINO_LED, OUTPUT);

  aquecerBuzzer();

  Serial.println("\n=== Monitoramento de Enchentes IoT - Mackenzie ===");

  conectarWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORTA);
  conectarMQTT();
}

// ==================================================================
//  Faz uma nova leitura, classifica e publica (a cada intervalo)
// ==================================================================
void processarLeitura() {
  float distancia = medirDistanciaCM();
  if (distancia < 0) {
    Serial.println("Falha na leitura do sensor HC-SR04.");
    return;
  }

  float nivelAgua = ALTURA_CANAL_CM - distancia;
  if (nivelAgua < 0) nivelAgua = 0;
  if (nivelAgua > ALTURA_CANAL_CM) nivelAgua = ALTURA_CANAL_CM;

  float percentual = nivelAgua / ALTURA_CANAL_CM;

  if (percentual >= PCT_PERIGO) {
    estadoAtual = "PERIGO";
  } else if (percentual >= PCT_ATENCAO) {
    estadoAtual = "ATENCAO";
  } else {
    estadoAtual = "NORMAL";
  }

  // LED aceso quando ha qualquer alerta
  digitalWrite(PINO_LED, estadoAtual == "NORMAL" ? LOW : HIGH);

  // Monta e publica o JSON
  StaticJsonDocument<256> doc;
  doc["sensor"]       = "sensor01";
  doc["local"]        = "cidade/bairro";
  doc["distancia_cm"] = round(distancia * 10) / 10.0;
  doc["nivel_cm"]     = round(nivelAgua * 10) / 10.0;
  doc["percentual"]   = round(percentual * 1000) / 10.0;
  doc["estado"]       = estadoAtual;
  doc["timestamp_ms"] = millis();

  char payload[256];
  serializeJson(doc, payload);

  if (mqtt.publish(MQTT_TOPICO, payload)) {
    Serial.print("Publicado -> ");
    Serial.println(payload);
  } else {
    Serial.println("Falha ao publicar no MQTT.");
  }
}

// ==================================================================
//  LOOP
// ==================================================================
void loop() {
  if (!mqtt.connected()) {
    conectarMQTT();
  }
  mqtt.loop();

  // Atualiza a leitura/publicacao no intervalo definido
  unsigned long agora = millis();
  if (agora - ultimaLeitura >= INTERVALO_LEITURA) {
    ultimaLeitura = agora;
    processarLeitura();
  }

  // O alarme sonoro toca continuamente (a cada ciclo) enquanto
  // o estado for critico, refletindo o nivel atual em tempo real.
  tocarAlertaSonoro(estadoAtual);
}
