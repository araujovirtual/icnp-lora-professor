#ifndef RADIO_LORA_H
#define RADIO_LORA_H

#include <Arduino.h>

struct PacoteRecebido {
  String mensagem;
  int rssi;
  float snr;
  bool recebido;
};

void iniciarRadioLoRa();
void enviarMensagemLoRa(const String &mensagem);
PacoteRecebido receberMensagemLoRa(unsigned long tempoLimiteMs);

#endif