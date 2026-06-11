#include "radio_lora.h"

#include <SPI.h>
#include <LoRa.h>

#include "configuracao_lora.h"

void iniciarRadioLoRa() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(FREQUENCIA_LORA)) {
    Serial.println("ERRO: Falha ao iniciar LoRa.");
    while (true) {
      delay(1000);
    }
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
}

void enviarMensagemLoRa(const String &mensagem) {
  LoRa.beginPacket();
  LoRa.print(mensagem);
  LoRa.endPacket();
}

PacoteRecebido receberMensagemLoRa(unsigned long tempoLimiteMs) {
  PacoteRecebido pacote;
  pacote.mensagem = "";
  pacote.rssi = 0;
  pacote.snr = 0.0;
  pacote.recebido = false;

  unsigned long inicio = millis();

  while (millis() - inicio < tempoLimiteMs) {
    int tamanhoPacote = LoRa.parsePacket();

    if (tamanhoPacote > 0) {
      while (LoRa.available()) {
        pacote.mensagem += (char)LoRa.read();
      }

      pacote.rssi = LoRa.packetRssi();
      pacote.snr = LoRa.packetSnr();
      pacote.recebido = true;

      return pacote;
    }
  }

  return pacote;
}