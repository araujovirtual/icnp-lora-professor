#include "sensor_fisiologico.h"

#include <Wire.h>
#include <math.h>
#include "MAX30105.h"
#include "heartRate.h"

#define I2C_SDA 4
#define I2C_SCL 15
#define ENDERECO_MAX3010X 0x57

// Ajustado com base nos testes reais.
// Sem dedo: centenas a poucos milhares.
// Com dedo: ~22000 a 24000.
#define LIMIAR_DEDO_IR 10000L

#define RATE_SIZE 8

#define FC_MIN_ACEITA 40
#define FC_MAX_ACEITA 130

#define BUFFER_SPO2 100
#define BUFFER_PPG_API 64

#define SPO2_MIN_ACEITA 85
#define SPO2_MAX_ACEITA 100

#define R_MIN_ACEITO 0.20f
#define R_MAX_ACEITO 1.30f

#define AC_IR_MIN 30.0f
#define AC_RED_MIN 10.0f

#define INTERVALO_CALCULO_SPO2_MS 2500

static MAX30105 sensorMax;

static TaskHandle_t taskFcHandle = NULL;
static TaskHandle_t taskSpo2Handle = NULL;

static portMUX_TYPE muxDadosSensor = portMUX_INITIALIZER_UNLOCKED;

static bool sensorOk = false;
static bool sinalPresente = false;

static long irAtual = 0;
static long redAtual = 0;

static float bpmInstantaneo = 0.0f;
static int bpmMedio = 0;

static bool qualidadeSpo2Ok = false;
static int spo2Final = -1;
static float ratioR = 0.0f;

static float dcIR = 0.0f;
static float dcRED = 0.0f;
static float acIR = 0.0f;
static float acRED = 0.0f;

static uint32_t bufferIrSpo2[BUFFER_SPO2];
static uint32_t bufferRedSpo2[BUFFER_SPO2];

static int indiceAmostraSpo2 = 0;
static int totalAmostrasSpo2 = 0;

static uint32_t bufferPpgApi[BUFFER_PPG_API];
static int indicePpgApi = 0;
static int totalPpgApi = 0;

static byte rates[RATE_SIZE];
static byte rateSpot = 0;

static long lastBeat = 0;
static unsigned long ultimaBatidaDetectada = 0;

static bool fcAceita(float valor) {
  return valor >= FC_MIN_ACEITA && valor <= FC_MAX_ACEITA;
}

static bool fcMediaAceita(int valor) {
  return valor >= FC_MIN_ACEITA && valor <= FC_MAX_ACEITA;
}

static bool spo2Aceita(int valor) {
  return valor >= SPO2_MIN_ACEITA && valor <= SPO2_MAX_ACEITA;
}

static void limparBpmInterno() {
  bpmInstantaneo = 0.0f;
  bpmMedio = 0;
  rateSpot = 0;
  lastBeat = 0;
  ultimaBatidaDetectada = 0;

  for (byte i = 0; i < RATE_SIZE; i++) {
    rates[i] = 0;
  }
}

static void limparSpo2Compartilhado() {
  qualidadeSpo2Ok = false;
  spo2Final = -1;
  ratioR = 0.0f;
  dcIR = 0.0f;
  dcRED = 0.0f;
  acIR = 0.0f;
  acRED = 0.0f;
}

static void limparBufferSpo2Compartilhado() {
  indiceAmostraSpo2 = 0;
  totalAmostrasSpo2 = 0;

  for (int i = 0; i < BUFFER_SPO2; i++) {
    bufferIrSpo2[i] = 0;
    bufferRedSpo2[i] = 0;
  }
}

static void limparBufferPpgApiCompartilhado() {
  indicePpgApi = 0;
  totalPpgApi = 0;

  for (int i = 0; i < BUFFER_PPG_API; i++) {
    bufferPpgApi[i] = 0;
  }
}

static void adicionarAmostraNoBufferSpo2(long ir, long red) {
  if (ir < 0) {
    ir = 0;
  }

  if (red < 0) {
    red = 0;
  }

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

static void adicionarAmostraNoBufferPpgApi(long ir) {
  if (ir < 0) {
    ir = 0;
  }

  bufferPpgApi[indicePpgApi] = (uint32_t)ir;

  indicePpgApi++;

  if (indicePpgApi >= BUFFER_PPG_API) {
    indicePpgApi = 0;
  }

  if (totalPpgApi < BUFFER_PPG_API) {
    totalPpgApi++;
  }
}

static void processarFcTempoReal(long ir) {
  if (ir <= LIMIAR_DEDO_IR) {
    limparBpmInterno();
    return;
  }

  if (checkForBeat(ir)) {
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

static float calcularMediaLocal(uint32_t *buffer) {
  double soma = 0.0;

  for (int i = 0; i < BUFFER_SPO2; i++) {
    soma += buffer[i];
  }

  return soma / BUFFER_SPO2;
}

static float calcularRmsAcLocal(uint32_t *buffer, float media) {
  double somaQuadrados = 0.0;

  for (int i = 0; i < BUFFER_SPO2; i++) {
    float ac = (float)buffer[i] - media;
    somaQuadrados += ac * ac;
  }

  return sqrt(somaQuadrados / BUFFER_SPO2);
}

static bool bufferLocalTemSinal(uint32_t *bufferIr) {
  int amostrasComSinal = 0;

  for (int i = 0; i < BUFFER_SPO2; i++) {
    if (bufferIr[i] > LIMIAR_DEDO_IR) {
      amostrasComSinal++;
    }
  }

  return amostrasComSinal >= 95;
}

static void calcularSpo2ComBufferLocal(uint32_t *irLocal, uint32_t *redLocal) {
  if (!bufferLocalTemSinal(irLocal)) {
    portENTER_CRITICAL(&muxDadosSensor);
    limparSpo2Compartilhado();
    portEXIT_CRITICAL(&muxDadosSensor);
    return;
  }

  float dcIrLocal = calcularMediaLocal(irLocal);
  float dcRedLocal = calcularMediaLocal(redLocal);

  float acIrLocal = calcularRmsAcLocal(irLocal, dcIrLocal);
  float acRedLocal = calcularRmsAcLocal(redLocal, dcRedLocal);

  if (dcIrLocal <= 0.0f || dcRedLocal <= 0.0f) {
    portENTER_CRITICAL(&muxDadosSensor);
    limparSpo2Compartilhado();
    portEXIT_CRITICAL(&muxDadosSensor);
    return;
  }

  if (acIrLocal < AC_IR_MIN || acRedLocal < AC_RED_MIN) {
    portENTER_CRITICAL(&muxDadosSensor);
    limparSpo2Compartilhado();
    portEXIT_CRITICAL(&muxDadosSensor);
    return;
  }

  float componenteIr = acIrLocal / dcIrLocal;
  float componenteRed = acRedLocal / dcRedLocal;

  if (componenteIr <= 0.0f) {
    portENTER_CRITICAL(&muxDadosSensor);
    limparSpo2Compartilhado();
    portEXIT_CRITICAL(&muxDadosSensor);
    return;
  }

  float ratioLocal = componenteRed / componenteIr;

  if (ratioLocal < R_MIN_ACEITO || ratioLocal > R_MAX_ACEITO) {
    portENTER_CRITICAL(&muxDadosSensor);
    limparSpo2Compartilhado();
    portEXIT_CRITICAL(&muxDadosSensor);
    return;
  }

  float spo2Estimado = 104.0f - (17.0f * ratioLocal);
  int spo2Local = (int)(spo2Estimado + 0.5f);

  bool qualidadeLocal = spo2Aceita(spo2Local);

  if (!qualidadeLocal) {
    spo2Local = -1;
  }

  portENTER_CRITICAL(&muxDadosSensor);
  dcIR = dcIrLocal;
  dcRED = dcRedLocal;
  acIR = acIrLocal;
  acRED = acRedLocal;
  ratioR = ratioLocal;
  spo2Final = spo2Local;
  qualidadeSpo2Ok = qualidadeLocal;
  portEXIT_CRITICAL(&muxDadosSensor);
}

static void taskFcTempoReal(void *parametro) {
  while (true) {
    long ir = sensorMax.getIR();
    long red = sensorMax.getRed();

    bool sinal = ir > LIMIAR_DEDO_IR;

    portENTER_CRITICAL(&muxDadosSensor);

    irAtual = ir;
    redAtual = red;
    sinalPresente = sinal;

    if (sinal) {
      adicionarAmostraNoBufferPpgApi(ir);
      adicionarAmostraNoBufferSpo2(ir, red);
    } else {
      limparBufferPpgApiCompartilhado();
      limparBufferSpo2Compartilhado();
      limparSpo2Compartilhado();
    }

    portEXIT_CRITICAL(&muxDadosSensor);

    processarFcTempoReal(ir);

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

static void taskSpo2(void *parametro) {
  uint32_t irLocal[BUFFER_SPO2];
  uint32_t redLocal[BUFFER_SPO2];

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(INTERVALO_CALCULO_SPO2_MS));

    int totalLocal = 0;

    portENTER_CRITICAL(&muxDadosSensor);

    totalLocal = totalAmostrasSpo2;

    if (totalLocal >= BUFFER_SPO2) {
      for (int i = 0; i < BUFFER_SPO2; i++) {
        irLocal[i] = bufferIrSpo2[i];
        redLocal[i] = bufferRedSpo2[i];
      }
    }

    portEXIT_CRITICAL(&muxDadosSensor);

    if (totalLocal >= BUFFER_SPO2) {
      calcularSpo2ComBufferLocal(irLocal, redLocal);
    } else {
      portENTER_CRITICAL(&muxDadosSensor);
      qualidadeSpo2Ok = false;
      spo2Final = -1;
      portEXIT_CRITICAL(&muxDadosSensor);
    }
  }
}

void iniciarSensorFisiologico() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  sensorOk = sensorMax.begin(Wire, I2C_SPEED_FAST, ENDERECO_MAX3010X);

  if (!sensorOk) {
    Serial.println("MAX30105/MAX30102 nao encontrado em 0x57.");
    return;
  }

  Serial.println("MAX30105/MAX30102 encontrado em 0x57.");

  sensorMax.setup();
  sensorMax.setPulseAmplitudeRed(0x0A);
  sensorMax.setPulseAmplitudeIR(0x1F);
  sensorMax.setPulseAmplitudeGreen(0);

  portENTER_CRITICAL(&muxDadosSensor);
  limparBpmInterno();
  limparSpo2Compartilhado();
  limparBufferSpo2Compartilhado();
  limparBufferPpgApiCompartilhado();
  portEXIT_CRITICAL(&muxDadosSensor);

  Serial.println("Sensor fisiologico iniciado.");
}

bool sensorFisiologicoDisponivel() {
  return sensorOk;
}

void iniciarTasksSensorFisiologico() {
  if (!sensorOk) {
    return;
  }

  if (taskFcHandle == NULL) {
    xTaskCreatePinnedToCore(
      taskFcTempoReal,
      "TaskSensorFC",
      8192,
      NULL,
      3,
      &taskFcHandle,
      1
    );
  }

  if (taskSpo2Handle == NULL) {
    xTaskCreatePinnedToCore(
      taskSpo2,
      "TaskSensorSpO2",
      8192,
      NULL,
      1,
      &taskSpo2Handle,
      0
    );
  }
}

long lerIrSensorFisiologico() {
  long valor;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = irAtual;
  portEXIT_CRITICAL(&muxDadosSensor);

  return valor;
}

long lerRedSensorFisiologico() {
  long valor;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = redAtual;
  portEXIT_CRITICAL(&muxDadosSensor);

  return valor;
}

bool dedoDetectadoSensorFisiologico() {
  bool valor;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = sinalPresente;
  portEXIT_CRITICAL(&muxDadosSensor);

  return valor;
}

bool qualidadeSpo2OkSensorFisiologico() {
  bool valor;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = qualidadeSpo2Ok;
  portEXIT_CRITICAL(&muxDadosSensor);

  return valor;
}

int lerFrequenciaCardiacaExperimental() {
  int valor;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = bpmMedio;
  portEXIT_CRITICAL(&muxDadosSensor);

  if (!fcMediaAceita(valor)) {
    return 0;
  }

  return valor;
}

float lerBpmInstantaneoExperimental() {
  float valor;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = bpmInstantaneo;
  portEXIT_CRITICAL(&muxDadosSensor);

  if (!fcAceita(valor)) {
    return 0.0f;
  }

  return valor;
}

int lerSpo2Experimental() {
  int valor;
  bool qualidade;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = spo2Final;
  qualidade = qualidadeSpo2Ok;
  portEXIT_CRITICAL(&muxDadosSensor);

  if (!qualidade || !spo2Aceita(valor)) {
    return 0;
  }

  return valor;
}

float lerRatioSpo2Experimental() {
  float valor;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = ratioR;
  portEXIT_CRITICAL(&muxDadosSensor);

  return valor;
}

float lerDcIrSensorFisiologico() {
  float valor;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = dcIR;
  portEXIT_CRITICAL(&muxDadosSensor);

  return valor;
}

float lerAcIrSensorFisiologico() {
  float valor;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = acIR;
  portEXIT_CRITICAL(&muxDadosSensor);

  return valor;
}

float lerDcRedSensorFisiologico() {
  float valor;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = dcRED;
  portEXIT_CRITICAL(&muxDadosSensor);

  return valor;
}

float lerAcRedSensorFisiologico() {
  float valor;

  portENTER_CRITICAL(&muxDadosSensor);
  valor = acRED;
  portEXIT_CRITICAL(&muxDadosSensor);

  return valor;
}

String montarJanelaPpgNormalizadaApi(int quantidade) {
  if (quantidade <= 0) {
    quantidade = 32;
  }

  if (quantidade > BUFFER_PPG_API) {
    quantidade = BUFFER_PPG_API;
  }

  uint32_t local[BUFFER_PPG_API];
  int totalLocal = 0;
  int indiceLocal = 0;

  portENTER_CRITICAL(&muxDadosSensor);

  totalLocal = totalPpgApi;
  indiceLocal = indicePpgApi;

  if (totalLocal >= quantidade) {
    int inicio = indiceLocal - quantidade;

    if (inicio < 0) {
      inicio += BUFFER_PPG_API;
    }

    for (int i = 0; i < quantidade; i++) {
      int idx = (inicio + i) % BUFFER_PPG_API;
      local[i] = bufferPpgApi[idx];
    }
  }

  portEXIT_CRITICAL(&muxDadosSensor);

  if (totalLocal < quantidade) {
    return "";
  }

  uint32_t minVal = local[0];
  uint32_t maxVal = local[0];

  for (int i = 1; i < quantidade; i++) {
    if (local[i] < minVal) {
      minVal = local[i];
    }

    if (local[i] > maxVal) {
      maxVal = local[i];
    }
  }

  if (maxVal <= minVal) {
    return "";
  }

  String saida = "";

  for (int i = 0; i < quantidade; i++) {
    long normalizado = map(
      (long)local[i],
      (long)minVal,
      (long)maxVal,
      20,
      235
    );

    if (normalizado < 0) {
      normalizado = 0;
    }

    if (normalizado > 255) {
      normalizado = 255;
    }

    if (i > 0) {
      saida += ",";
    }

    saida += String(normalizado);
  }

  return saida;
}