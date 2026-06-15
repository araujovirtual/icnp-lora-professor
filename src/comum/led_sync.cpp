#include "led_sync.h"

// Heltec WiFi LoRa 32 V2
// Pelo pinout enviado, o LED onboard esta associado ao GPIO25.
#define PINO_LED_SYNC 25

void iniciarLedSync() {
  pinMode(PINO_LED_SYNC, OUTPUT);
  digitalWrite(PINO_LED_SYNC, LOW);
}

void pulsoLedSync(unsigned long duracaoMs) {
  digitalWrite(PINO_LED_SYNC, HIGH);
  delay(duracaoMs);
  digitalWrite(PINO_LED_SYNC, LOW);
}

void atualizarLedSync() {
  // Mantido por compatibilidade com aluno_main.cpp e professor_main.cpp.
  // Nesta versao o pulso e executado diretamente em pulsoLedSync().
}