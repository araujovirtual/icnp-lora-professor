#include <Arduino.h>

#include "radio_lora.h"
#include "protocolo_icnp.h"

const String ID_ALUNO = "1";

const unsigned long tempoEsperaAckMs = 1200;
const unsigned long intervaloMinimoEntreDadosMs = 300;

unsigned long contadorSeq = 0;
unsigned long ultimoEnvioData = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ALUNO - ICNP MINIMO");
  Serial.println("Placa: Heltec WiFi LoRa 32 V2");
  Serial.println("Frequencia: 915 MHz");
  Serial.println("ID_ALUNO: " + ID_ALUNO);
  Serial.println("================================");

  iniciarRadioLoRa();

  Serial.println("LoRa iniciado com sucesso.");
  Serial.println("Aguardando BEACON do Professor...");
}

void loop() {
  PacoteRecebido beacon = receberMensagemLoRa(200);

  if (!beacon.recebido) {
    return;
  }

  if (!pacoteEhDoTipoIcnp(beacon.mensagem, "BEACON")) {
    return;
  }

  String ciclo = extrairCampoIcnp(beacon.mensagem, "CICLO");

  Serial.println();
  Serial.println("===== BEACON RECEBIDO =====");
  Serial.print("Mensagem: ");
  Serial.println(beacon.mensagem);
  Serial.print("RSSI BEACON: ");
  Serial.print(beacon.rssi);
  Serial.println(" dBm");
  Serial.print("SNR BEACON: ");
  Serial.print(beacon.snr);
  Serial.println(" dB");

  unsigned long agora = millis();

  if (agora - ultimoEnvioData < intervaloMinimoEntreDadosMs) {
    Serial.println("DATA ignorado: intervalo minimo ainda nao atingido.");
    Serial.println("===========================");
    return;
  }

  ultimoEnvioData = agora;

  String data = montarDataIcnp(ID_ALUNO, contadorSeq, ciclo, 72, 98);

  Serial.print("Enviando: ");
  Serial.println(data);

  enviarMensagemLoRa(data);

  PacoteRecebido ack = receberMensagemLoRa(tempoEsperaAckMs);

  if (!ack.recebido) {
    Serial.println("Timeout: ACK nao recebido.");
    Serial.println("===========================");
    contadorSeq++;
    return;
  }

  Serial.print("Recebido: ");
  Serial.println(ack.mensagem);
  Serial.print("RSSI ACK: ");
  Serial.print(ack.rssi);
  Serial.println(" dBm");
  Serial.print("SNR ACK: ");
  Serial.print(ack.snr);
  Serial.println(" dB");

  String alunoAck = extrairCampoIcnp(ack.mensagem, "ALUNO");
  String seqAck = extrairCampoIcnp(ack.mensagem, "SEQ");

  if (pacoteEhDoTipoIcnp(ack.mensagem, "ACK") && alunoAck == ID_ALUNO && seqAck == String(contadorSeq)) {
    Serial.println("ACK valido. Ciclo ICNP concluido.");
  } else {
    Serial.println("ACK invalido ou pertencente a outro pacote.");
  }

  Serial.println("===========================");

  contadorSeq++;
}