#include <Arduino.h>
#include <math.h>
#include "bateria.h"

// Heltec WiFi LoRa 32 V2
// GPIO21 controla o circuito de leitura.
// Nesta placa, LOW habilita leitura coerente no ADC.
#define PINO_CONTROLE_BATERIA 21

// ADC correto encontrado no teste
#define PINO_ADC_BATERIA 37

// Fator Heltec observado no teste:
// ADC 1472 -> 3.68 V
// 1472 * 0.0025 = 3.68
#define FATOR_HELTEC_V2 0.0025f

// Faixa aceita como leitura valida.
// IMPORTANTE:
// Nao deixar BAT_MIN_VALIDA em 3.00f, senao abaixo de 3.00 V aparece "--"
// e o OLED nao consegue mostrar CRITICA.
#define BAT_MIN_VALIDA 2.80f
#define BAT_MAX_VALIDA 4.35f

// =====================================================
// PARAMETROS DE ALERTA - ALTERE SOMENTE AQUI SE PRECISAR
// =====================================================
// Para o teste atual:
// 3.20 V ainda transmite, entao o aviso fica inicialmente em 3.10 V.
#define LIMIAR_BATERIA_BAIXA 3.10f
#define LIMIAR_BATERIA_CRITICA 3.00f

// Percentual estimado por tensao operacional.
// Nao representa carga real medida por coulomb counting.
#define BAT_PERCENTUAL_100 4.20f
#define BAT_PERCENTUAL_0 3.00f

void iniciarMonitorBateria() {
  pinMode(PINO_CONTROLE_BATERIA, OUTPUT);

  digitalWrite(PINO_CONTROLE_BATERIA, LOW);
  delay(20);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  analogSetPinAttenuation(PINO_ADC_BATERIA, ADC_11db);
  adcAttachPin(PINO_ADC_BATERIA);
}

float lerTensaoBateria() {
  digitalWrite(PINO_CONTROLE_BATERIA, LOW);
  delay(5);

  const int amostras = 10;
  long soma = 0;

  for (int i = 0; i < amostras; i++) {
    soma += analogRead(PINO_ADC_BATERIA);
    delay(2);
  }

  float leituraMedia = soma / (float)amostras;
  float tensaoBateria = leituraMedia * FATOR_HELTEC_V2;

  if (tensaoBateria < BAT_MIN_VALIDA || tensaoBateria > BAT_MAX_VALIDA) {
    return NAN;
  }

  return tensaoBateria;
}

String textoEnergia() {
  float tensao = lerTensaoBateria();

  if (isnan(tensao)) {
    return "--";
  }

  return String(tensao, 2) + " V";
}

int estimarPercentualBateria(float tensao) {
  if (isnan(tensao)) {
    return -1;
  }

  if (tensao >= BAT_PERCENTUAL_100) {
    return 100;
  }

  if (tensao <= BAT_PERCENTUAL_0) {
    return 0;
  }

  int percentual = (int)(((tensao - BAT_PERCENTUAL_0) / (BAT_PERCENTUAL_100 - BAT_PERCENTUAL_0)) * 100.0f);

  if (percentual < 0) {
    percentual = 0;
  }

  if (percentual > 100) {
    percentual = 100;
  }

  return percentual;
}

String statusBateria(float tensao) {
  if (isnan(tensao)) {
    return "SEM LEITURA";
  }

  if (tensao <= LIMIAR_BATERIA_CRITICA) {
    return "CRITICA";
  }

  if (tensao <= LIMIAR_BATERIA_BAIXA) {
    return "BAIXA";
  }

  return "OK";
}