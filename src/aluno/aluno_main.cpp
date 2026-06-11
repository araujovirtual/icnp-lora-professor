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
  Serial.println("ALUNO - ICNP COM VALIDACAO DE ACK");
  Serial.println("Placa: Heltec WiFi LoRa 32 V2");
  Serial.println("Frequencia: 915 MHz");
  Serial.println("ID_ALUNO: " + ID_ALUNO);
  Serial.println("Fluxo: aguarda BEACON -> envia DATA -> valida ACK");
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

  if (!pacoteEhDoTipoIcnp(beacon.mensagem, ICNP_TIPO_BEACON)) {
    return;
  }

  String cicloTexto = extrairCampoIcnp(beacon.mensagem, "CICLO");

  if (cicloTexto.length() == 0) {
    Serial.println("BEACON invalido: campo CICLO ausente.");
    return;
  }

  unsigned long ciclo = cicloTexto.toInt();

  Serial.println();
  Serial.println("===== BEACON RECEBIDO =====");
  Serial.print("Mensagem: ");
  Serial.println(beacon.mensagem);
  Serial.print("Ciclo: ");
  Serial.println(ciclo);
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

  int frequenciaCardiacaSimulada = 72;
  int spo2Simulado = 98;

  String data = montarDataIcnp(
    ID_ALUNO,
    contadorSeq,
    ciclo,
    frequenciaCardiacaSimulada,
    spo2Simulado
  );

  Serial.print("Enviando DATA: ");
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

  if (ackIcnpConfere(ack.mensagem, ID_ALUNO, contadorSeq, ciclo)) {
    Serial.println("ACK valido. Ciclo ICNP concluido.");
  } else {
    Serial.println("ACK invalido: aluno, sequencia ou ciclo nao conferem.");
  }

  Serial.println("===========================");

  contadorSeq++;
}