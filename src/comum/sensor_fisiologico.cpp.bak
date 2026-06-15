#include "sensor_fisiologico.h"

#include <Wire.h>
#include "MAX30105.h"

// Mesmo barramento I2C do OLED onboard da Heltec WiFi LoRa 32 V2
#define I2C_SDA 4
#define I2C_SCL 15
#define OLED_RST 16

// Endereco I2C esperado do MAX30105/MAX30102
#define ENDERECO_MAX3010X 0x57

// Limiar simples apenas para detectar presenca do dedo.
// Este valor e experimental e pode ser ajustado depois.
#define LIMIAR_DEDO_IR 50000L

static MAX30105 sensorMax;
static bool sensorOk = false;

void resetarOledParaLiberarBarramento() {
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);
  delay(50);
}

void iniciarSensorFisiologico() {
  resetarOledParaLiberarBarramento();

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  sensorOk = sensorMax.begin(Wire, I2C_SPEED_FAST, ENDERECO_MAX3010X);

  if (!sensorOk) {
    Serial.println("MAX30105/MAX30102 nao encontrado em 0x57.");
    return;
  }

  Serial.println("MAX30105/MAX30102 encontrado em 0x57.");

  byte ledBrightness = 0x3F; // brilho moderado
  byte sampleAverage = 4;
  byte ledMode = 2;          // Red + IR
  int sampleRate = 100;
  int pulseWidth = 411;
  int adcRange = 4096;

  sensorMax.setup(
    ledBrightness,
    sampleAverage,
    ledMode,
    sampleRate,
    pulseWidth,
    adcRange
  );

  sensorMax.setPulseAmplitudeRed(0x1F);
  sensorMax.setPulseAmplitudeIR(0x1F);
  sensorMax.setPulseAmplitudeGreen(0);

  Serial.println("Sensor fisiologico iniciado.");
}

bool sensorFisiologicoDisponivel() {
  return sensorOk;
}

long lerIrSensorFisiologico() {
  if (!sensorOk) {
    return 0;
  }

  return sensorMax.getIR();
}

long lerRedSensorFisiologico() {
  if (!sensorOk) {
    return 0;
  }

  return sensorMax.getRed();
}

bool dedoDetectadoSensorFisiologico() {
  long ir = lerIrSensorFisiologico();

  return ir > LIMIAR_DEDO_IR;
}

int lerFrequenciaCardiacaExperimental() {
  if (!sensorOk) {
    return 0;
  }

  if (!dedoDetectadoSensorFisiologico()) {
    return 0;
  }

  // Placeholder temporario.
  // Nesta etapa, primeiro validamos leitura real IR/RED e presenca do dedo.
  // O algoritmo real de FC entra depois.
  return 72;
}

int lerSpo2Experimental() {
  if (!sensorOk) {
    return 0;
  }

  if (!dedoDetectadoSensorFisiologico()) {
    return 0;
  }

  // Placeholder temporario.
  // Nesta etapa, primeiro validamos leitura real IR/RED e presenca do dedo.
  // O algoritmo real de SpO2 entra depois.
  return 98;
}