#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "SSD1306Wire.h"

#include "MAX30105.h"
#include "heartRate.h"

// =====================================================
// HELTEC WIFI LORA 32 V2
// =====================================================
#define I2C_SDA 4
#define I2C_SCL 15
#define OLED_RST 16
#define OLED_ADDR 0x3C

#define ENDERECO_MAX3010X 0x57

// =====================================================
// SENSOR / FC
// =====================================================
#define LIMIAR_DEDO_IR 30000L

#define RATE_SIZE 8

#define FC_MIN_ACEITA 40
#define FC_MAX_ACEITA 130

// =====================================================
// SPO2 EXPERIMENTAL
// =====================================================
#define BUFFER_SPO2 100

#define SPO2_MIN_ACEITA 85
#define SPO2_MAX_ACEITA 100

#define R_MIN_ACEITO 0.20f
#define R_MAX_ACEITO 1.30f

#define AC_IR_MIN 30.0f
#define AC_RED_MIN 10.0f

// Tempo entre cálculos de SpO2.
// A FC continua rodando direto.
#define INTERVALO_CALCULO_SPO2_MS 2500

// =====================================================
// OBJETOS
// =====================================================
SSD1306Wire displayTeste(OLED_ADDR, I2C_SDA, I2C_SCL);
MAX30105 particleSensor;

// =====================================================
// TASKS
// =====================================================
TaskHandle_t taskFcHandle = NULL;
TaskHandle_t taskSpo2Handle = NULL;
TaskHandle_t taskUiHandle = NULL;

// =====================================================
// PROTECAO DE DADOS COMPARTILHADOS
// =====================================================
portMUX_TYPE muxDados = portMUX_INITIALIZER_UNLOCKED;

// =====================================================
// VARIAVEIS COMPARTILHADAS
// =====================================================
bool sensorOk = false;
bool sinalPresente = false;
bool beatDetectado = false;

long irAtual = 0;
long redAtual = 0;

float bpmInstantaneo = 0.0f;
int bpmMedio = 0;

bool qualidadeSpo2Ok = false;
int spo2Final = -1;
float spo2Estimado = 0.0f;
float ratioR = 0.0f;

float dcIR = 0.0f;
float dcRED = 0.0f;
float acIR = 0.0f;
float acRED = 0.0f;

// =====================================================
// BUFFER CIRCULAR PARA SPO2
// A Task FC preenche.
// A Task SpO2 apenas copia e calcula.
// =====================================================
uint32_t bufferIrSpo2[BUFFER_SPO2];
uint32_t bufferRedSpo2[BUFFER_SPO2];

int indiceAmostraSpo2 = 0;
int totalAmostrasSpo2 = 0;

// =====================================================
// VARIAVEIS INTERNAS DA FC
// =====================================================
byte rates[RATE_SIZE];
byte rateSpot = 0;

long lastBeat = 0;
unsigned long ultimaBatidaDetectada = 0;

// =====================================================
// OLED
// =====================================================
void resetarOledTeste() {
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);
  delay(100);
}

void iniciarOledTeste() {
  resetarOledTeste();

  displayTeste.init();
  delay(100);

  displayTeste.clear();
  displayTeste.flipScreenVertically();
  displayTeste.setFont(ArialMT_Plain_10);
  displayTeste.setTextAlignment(TEXT_ALIGN_LEFT);

  displayTeste.drawString(0, 0, "Teste FC + SpO2");
  displayTeste.drawString(0, 14, "FC em tempo real");
  displayTeste.drawString(0, 28, "SpO2 em thread");
  displayTeste.drawString(0, 42, "Sem LoRa / ICNP");
  displayTeste.display();

  delay(1000);
}

void mostrarMensagemOled(
  const String &linha1,
  const String &linha2,
  const String &linha3,
  const String &linha4
) {
  displayTeste.clear();
  displayTeste.setFont(ArialMT_Plain_10);
  displayTeste.setTextAlignment(TEXT_ALIGN_LEFT);

  displayTeste.drawString(0, 0, linha1);
  displayTeste.drawString(0, 14, linha2);
  displayTeste.drawString(0, 28, linha3);
  displayTeste.drawString(0, 42, linha4);

  displayTeste.display();
}

// =====================================================
// FUNCOES AUXILIARES
// =====================================================
bool fcAceita(float valor) {
  return valor >= FC_MIN_ACEITA && valor <= FC_MAX_ACEITA;
}

bool fcMediaAceita(int valor) {
  return valor >= FC_MIN_ACEITA && valor <= FC_MAX_ACEITA;
}

bool spo2Aceita(int valor) {
  return valor >= SPO2_MIN_ACEITA && valor <= SPO2_MAX_ACEITA;
}

void limparBpmInterno() {
  bpmInstantaneo = 0.0f;
  bpmMedio = 0;
  rateSpot = 0;
  lastBeat = 0;
  ultimaBatidaDetectada = 0;

  for (byte i = 0; i < RATE_SIZE; i++) {
    rates[i] = 0;
  }
}

void limparSpo2Compartilhado() {
  qualidadeSpo2Ok = false;
  spo2Final = -1;
  spo2Estimado = 0.0f;
  ratioR = 0.0f;
  dcIR = 0.0f;
  dcRED = 0.0f;
  acIR = 0.0f;
  acRED = 0.0f;
}

void limparBufferSpo2Compartilhado() {
  indiceAmostraSpo2 = 0;
  totalAmostrasSpo2 = 0;

  for (int i = 0; i < BUFFER_SPO2; i++) {
    bufferIrSpo2[i] = 0;
    bufferRedSpo2[i] = 0;
  }
}

void adicionarAmostraNoBufferSpo2(long ir, long red) {
  bufferIrSpo2[indiceAmostraSpo2] = (uint32_t)ir;
  bufferRedSpo2[indiceAmostraSpo2] = (uint32_t)red;

  indiceAmostraSpo2++;

  if (indiceAmostraSpo2 >= BUFFER_SPO2) {
    indiceAmostraSpo2 = 0;
  }

  if (totalAmostrasSpo2 < BUFFER_SPO2) {
    totalAmostrasSpo2++;
  }
}

// =====================================================
// FC EM TEMPO REAL
// =====================================================
void processarFcTempoReal(long ir) {
  beatDetectado = false;

  if (ir <= LIMIAR_DEDO_IR) {
    limparBpmInterno();
    return;
  }

  if (checkForBeat(ir)) {
    beatDetectado = true;

    unsigned long agora = millis();

    if (lastBeat > 0) {
      long delta = agora - lastBeat;

      if (delta > 0) {
        float bpm = 60.0f / (delta / 1000.0f);

        if (fcAceita(bpm)) {
          bpmInstantaneo = bpm;

          rates[rateSpot++] = (byte)bpm;
          rateSpot %= RATE_SIZE;

          int soma = 0;
          int validos = 0;

          for (byte i = 0; i < RATE_SIZE; i++) {
            if (rates[i] > 0) {
              soma += rates[i];
              validos++;
            }
          }

          if (validos > 0) {
            bpmMedio = soma / validos;
          }
        }
      }
    }

    lastBeat = agora;
    ultimaBatidaDetectada = agora;
  }

  if (ultimaBatidaDetectada > 0 && millis() - ultimaBatidaDetectada > 5000) {
    limparBpmInterno();
  }
}

// =====================================================
// SPO2 EXPERIMENTAL
// =====================================================
float calcularMediaLocal(uint32_t *buffer) {
  double soma = 0.0;

  for (int i = 0; i < BUFFER_SPO2; i++) {
    soma += buffer[i];
  }

  return soma / BUFFER_SPO2;
}

float calcularRmsAcLocal(uint32_t *buffer, float media) {
  double somaQuadrados = 0.0;

  for (int i = 0; i < BUFFER_SPO2; i++) {
    float ac = (float)buffer[i] - media;
    somaQuadrados += ac * ac;
  }

  return sqrt(somaQuadrados / BUFFER_SPO2);
}

bool bufferLocalTemSinal(uint32_t *bufferIr) {
  int amostrasComSinal = 0;

  for (int i = 0; i < BUFFER_SPO2; i++) {
    if (bufferIr[i] > LIMIAR_DEDO_IR) {
      amostrasComSinal++;
    }
  }

  return amostrasComSinal >= 95;
}

void calcularSpo2ComBufferLocal(uint32_t *irLocal, uint32_t *redLocal) {
  float dcIrLocal = 0.0f;
  float dcRedLocal = 0.0f;
  float acIrLocal = 0.0f;
  float acRedLocal = 0.0f;
  float ratioLocal = 0.0f;
  float spo2EstLocal = 0.0f;
  int spo2Local = -1;
  bool qualidadeLocal = false;

  if (!bufferLocalTemSinal(irLocal)) {
    portENTER_CRITICAL(&muxDados);
    limparSpo2Compartilhado();
    portEXIT_CRITICAL(&muxDados);
    return;
  }

  dcIrLocal = calcularMediaLocal(irLocal);
  dcRedLocal = calcularMediaLocal(redLocal);

  acIrLocal = calcularRmsAcLocal(irLocal, dcIrLocal);
  acRedLocal = calcularRmsAcLocal(redLocal, dcRedLocal);

  if (dcIrLocal <= 0.0f || dcRedLocal <= 0.0f) {
    portENTER_CRITICAL(&muxDados);
    limparSpo2Compartilhado();
    portEXIT_CRITICAL(&muxDados);
    return;
  }

  if (acIrLocal < AC_IR_MIN || acRedLocal < AC_RED_MIN) {
    portENTER_CRITICAL(&muxDados);
    limparSpo2Compartilhado();
    portEXIT_CRITICAL(&muxDados);
    return;
  }

  float componenteIr = acIrLocal / dcIrLocal;
  float componenteRed = acRedLocal / dcRedLocal;

  if (componenteIr <= 0.0f) {
    portENTER_CRITICAL(&muxDados);
    limparSpo2Compartilhado();
    portEXIT_CRITICAL(&muxDados);
    return;
  }

  ratioLocal = componenteRed / componenteIr;

  if (ratioLocal < R_MIN_ACEITO || ratioLocal > R_MAX_ACEITO) {
    portENTER_CRITICAL(&muxDados);
    limparSpo2Compartilhado();
    portEXIT_CRITICAL(&muxDados);
    return;
  }

  spo2EstLocal = 104.0f - (17.0f * ratioLocal);
  spo2Local = (int)(spo2EstLocal + 0.5f);

  if (spo2Aceita(spo2Local)) {
    qualidadeLocal = true;
  } else {
    qualidadeLocal = false;
    spo2Local = -1;
  }

  portENTER_CRITICAL(&muxDados);
  dcIR = dcIrLocal;
  dcRED = dcRedLocal;
  acIR = acIrLocal;
  acRED = acRedLocal;
  ratioR = ratioLocal;
  spo2Estimado = spo2EstLocal;
  spo2Final = spo2Local;
  qualidadeSpo2Ok = qualidadeLocal;
  portEXIT_CRITICAL(&muxDados);
}

// =====================================================
// SERIAL
// =====================================================
void imprimirSerial() {
  bool sinal;
  bool beat;
  bool qualidade;
  int fc;
  float bpmInst;
  int spo2;
  float spo2Est;
  float ratio;
  float dcIrLocal;
  float acIrLocal;
  float dcRedLocal;
  float acRedLocal;
  long ir;
  long red;

  portENTER_CRITICAL(&muxDados);
  sinal = sinalPresente;
  beat = beatDetectado;
  qualidade = qualidadeSpo2Ok;
  fc = bpmMedio;
  bpmInst = bpmInstantaneo;
  spo2 = spo2Final;
  spo2Est = spo2Estimado;
  ratio = ratioR;
  dcIrLocal = dcIR;
  acIrLocal = acIR;
  dcRedLocal = dcRED;
  acRedLocal = acRED;
  ir = irAtual;
  red = redAtual;
  portEXIT_CRITICAL(&muxDados);

  Serial.print("SENSOR=");
  Serial.print(sensorOk ? "OK" : "OFF");

  Serial.print("; SINAL=");
  Serial.print(sinal ? "SIM" : "NAO");

  Serial.print("; BEAT=");
  Serial.print(beat ? "SIM" : "NAO");

  Serial.print("; FC=");
  if (sinal && fcMediaAceita(fc)) {
    Serial.print(fc);
  } else {
    Serial.print("NA");
  }

  Serial.print("; BPM_INST=");
  if (sinal && fcAceita(bpmInst)) {
    Serial.print(bpmInst, 1);
  } else {
    Serial.print("NA");
  }

  Serial.print("; SPO2=");
  if (sinal && qualidade && spo2Aceita(spo2)) {
    Serial.print(spo2);
  } else {
    Serial.print("NA");
  }

  Serial.print("; SPO2_EST=");
  Serial.print(spo2Est, 2);

  Serial.print("; QUAL_SPO2=");
  Serial.print(qualidade ? "OK" : "RUIM");

  Serial.print("; RATIO=");
  Serial.print(ratio, 4);

  Serial.print("; DC_IR=");
  Serial.print(dcIrLocal, 1);

  Serial.print("; AC_IR=");
  Serial.print(acIrLocal, 1);

  Serial.print("; DC_RED=");
  Serial.print(dcRedLocal, 1);

  Serial.print("; AC_RED=");
  Serial.print(acRedLocal, 1);

  Serial.print("; IR=");
  Serial.print(ir);

  Serial.print("; RED=");
  Serial.println(red);
}

// =====================================================
// OLED
// =====================================================
void atualizarOled() {
  bool sinal;
  bool qualidade;
  bool beat;
  int fc;
  int spo2;
  long ir;
  long red;

  portENTER_CRITICAL(&muxDados);
  sinal = sinalPresente;
  qualidade = qualidadeSpo2Ok;
  beat = beatDetectado;
  fc = bpmMedio;
  spo2 = spo2Final;
  ir = irAtual;
  red = redAtual;
  portEXIT_CRITICAL(&muxDados);

  displayTeste.clear();
  displayTeste.setFont(ArialMT_Plain_10);
  displayTeste.setTextAlignment(TEXT_ALIGN_LEFT);

  displayTeste.drawString(0, 0, "ALUNO - FC + SpO2");

  displayTeste.drawString(0, 12, sinal ? "Sinal: SIM" : "Sinal: NAO");
  displayTeste.drawString(72, 12, qualidade ? "Q:OK" : "Q:--");

  String linhaFc = "FC: ";
  if (sinal && fcMediaAceita(fc)) {
    linhaFc += String(fc);
    linhaFc += " bpm";
  } else {
    linhaFc += "--";
  }

  String linhaSpo2 = "SpO2: ";
  if (sinal && qualidade && spo2Aceita(spo2)) {
    linhaSpo2 += String(spo2);
    linhaSpo2 += "%";
  } else {
    linhaSpo2 += "--";
  }

  displayTeste.drawString(0, 25, linhaFc);
  displayTeste.drawString(0, 37, linhaSpo2);

  displayTeste.drawString(0, 49, "IR:" + String(ir / 1000) + "k");
  displayTeste.drawString(72, 49, beat ? "B:1" : "B:0");

  displayTeste.display();
}

// =====================================================
// TASK 1 - FC TEMPO REAL
// =====================================================
void taskFcTempoReal(void *parametro) {
  while (true) {
    long ir = particleSensor.getIR();
    long red = particleSensor.getRed();

    bool sinal = ir > LIMIAR_DEDO_IR;

    portENTER_CRITICAL(&muxDados);
    irAtual = ir;
    redAtual = red;
    sinalPresente = sinal;

    if (sinal) {
      adicionarAmostraNoBufferSpo2(ir, red);
    } else {
      limparBufferSpo2Compartilhado();
      limparSpo2Compartilhado();
    }
    portEXIT_CRITICAL(&muxDados);

    processarFcTempoReal(ir);

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// =====================================================
// TASK 2 - SPO2 EM TEMPO DIFERENTE
// =====================================================
void taskSpo2(void *parametro) {
  uint32_t irLocal[BUFFER_SPO2];
  uint32_t redLocal[BUFFER_SPO2];

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(INTERVALO_CALCULO_SPO2_MS));

    int totalLocal = 0;

    portENTER_CRITICAL(&muxDados);
    totalLocal = totalAmostrasSpo2;

    if (totalLocal >= BUFFER_SPO2) {
      for (int i = 0; i < BUFFER_SPO2; i++) {
        irLocal[i] = bufferIrSpo2[i];
        redLocal[i] = bufferRedSpo2[i];
      }
    }
    portEXIT_CRITICAL(&muxDados);

    if (totalLocal >= BUFFER_SPO2) {
      calcularSpo2ComBufferLocal(irLocal, redLocal);
    } else {
      portENTER_CRITICAL(&muxDados);
      qualidadeSpo2Ok = false;
      spo2Final = -1;
      portEXIT_CRITICAL(&muxDados);
    }
  }
}

// =====================================================
// TASK 3 - SERIAL + OLED
// =====================================================
void taskUi(void *parametro) {
  unsigned long ultimoSerial = 0;
  unsigned long ultimoOled = 0;

  while (true) {
    bool beatLocal = false;

    portENTER_CRITICAL(&muxDados);
    beatLocal = beatDetectado;
    portEXIT_CRITICAL(&muxDados);

    if (millis() - ultimoSerial >= 500 || beatLocal) {
      ultimoSerial = millis();
      imprimirSerial();
    }

    if (millis() - ultimoOled >= 500 || beatLocal) {
      ultimoOled = millis();
      atualizarOled();
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("TESTE THREADS - FC + SPO2");
  Serial.println("MAX30102 MH-ET LIVE + HELTEC WIFI LORA 32 V2");
  Serial.println("Sem LoRa / Sem ICNP");
  Serial.println("Task FC: leitura em tempo real");
  Serial.println("Task SpO2: calculo em tempo diferente");
  Serial.println("Task UI: Serial + OLED");
  Serial.println("Configuracao preservada da FC que funcionou");
  Serial.println("RED = 0x0A");
  Serial.println("IR  = 0x1F");
  Serial.println("SDA = GPIO4");
  Serial.println("SCL = GPIO15");
  Serial.println("OLED = 0x3C");
  Serial.println("MAX3010x = 0x57");
  Serial.println("================================");

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  iniciarOledTeste();

  sensorOk = particleSensor.begin(Wire, I2C_SPEED_FAST, ENDERECO_MAX3010X);

  if (!sensorOk) {
    Serial.println("MAX30105/MAX30102 nao encontrado em 0x57.");

    mostrarMensagemOled(
      "Sensor OFF",
      "MAX nao encontrado",
      "Verifique I2C",
      "0x57"
    );

    while (true) {
      delay(1000);
    }
  }

  Serial.println("MAX30105/MAX30102 encontrado em 0x57.");
  Serial.println("Coloque o dedo parado. Aguarde FC primeiro, depois SpO2.");

  // =====================================================
  // CONFIGURACAO QUE FUNCIONOU PARA FC
  // =====================================================
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeIR(0x1F);
  particleSensor.setPulseAmplitudeGreen(0);

  portENTER_CRITICAL(&muxDados);
  limparBpmInterno();
  limparSpo2Compartilhado();
  limparBufferSpo2Compartilhado();
  portEXIT_CRITICAL(&muxDados);

  mostrarMensagemOled(
    "Coloque o dedo",
    "FC em tempo real",
    "SpO2 em thread",
    "aguarde..."
  );

  delay(1000);

  // Core 1: leitura do sensor e FC
  xTaskCreatePinnedToCore(
    taskFcTempoReal,
    "TaskFC",
    8192,
    NULL,
    3,
    &taskFcHandle,
    1
  );

  // Core 0: calculo SpO2, sem acessar I2C
  xTaskCreatePinnedToCore(
    taskSpo2,
    "TaskSpO2",
    8192,
    NULL,
    1,
    &taskSpo2Handle,
    0
  );

  // Core 0: serial e OLED
  // OLED usa I2C, mas só ele escreve no OLED.
  // A leitura do sensor fica na TaskFC.
  xTaskCreatePinnedToCore(
    taskUi,
    "TaskUI",
    8192,
    NULL,
    1,
    &taskUiHandle,
    0
  );
}

// =====================================================
// LOOP PRINCIPAL
// =====================================================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}