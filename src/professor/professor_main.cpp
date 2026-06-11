#include <Arduino.h>

#include "radio_lora.h"
#include "protocolo_icnp.h"

const unsigned long intervaloBeaconMs = 3000;
const unsigned long tempoEsperaDataMs = 1200;

unsigned long ultimoBeacon = 0;
unsigned long cicloAtual = 0;

void registrarCsvSucesso(
  unsigned long ciclo,
  const String &aluno,
  const String &seq,
  int rssi,
  float snr
) {
  Serial.print("CSV;");
  Serial.print("CICLO=");
  Serial.print(ciclo);
  Serial.print(";ALUNO=");
  Serial.print(aluno);
  Serial.print(";SEQ=");
  Serial.print(seq);
  Serial.print(";RSSI=");
  Serial.print(rssi);
  Serial.print(";SNR=");
  Serial.print(snr, 2);
  Serial.print(";ACK=1");
  Serial.println();
}

void registrarCsvTimeout(unsigned long ciclo) {
  Serial.print("CSV;");
  Serial.print("CICLO=");
  Serial.print(ciclo);
  Serial.print(";ALUNO=NA");
  Serial.print(";SEQ=NA");
  Serial.print(";RSSI=NA");
  Serial.print(";SNR=NA");
  Serial.print(";ACK=0");
  Serial.println();
}

void registrarCsvPacoteInvalido(unsigned long ciclo, int rssi, float snr) {
  Serial.print("CSV;");
  Serial.print("CICLO=");
  Serial.print(ciclo);
  Serial.print(";ALUNO=INVALIDO");
  Serial.print(";SEQ=INVALIDO");
  Serial.print(";RSSI=");
  Serial.print(rssi);
  Serial.print(";SNR=");
  Serial.print(snr, 2);
  Serial.print(";ACK=0");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("PROFESSOR - ICNP MINIMO COM LOG CSV");
  Serial.println("Placa: Heltec WiFi LoRa 32 V2");
  Serial.println("Frequencia: 915 MHz");
  Serial.println("================================");

  iniciarRadioLoRa();

  Serial.println("LoRa iniciado com sucesso.");
  Serial.println("Iniciando ciclos ICNP...");
  Serial.println();
  Serial.println("Formato CSV:");
  Serial.println("CSV;CICLO=<n>;ALUNO=<id>;SEQ=<seq>;RSSI=<dBm>;SNR=<dB>;ACK=<0|1>");
}

void loop() {
  unsigned long agora = millis();

  if (agora - ultimoBeacon < intervaloBeaconMs) {
    return;
  }

  ultimoBeacon = agora;
  cicloAtual++;

  String beacon = montarBeaconIcnp(cicloAtual);

  Serial.println();
  Serial.println("===== CICLO ICNP =====");
  Serial.print("Enviando: ");
  Serial.println(beacon);

  enviarMensagemLoRa(beacon);

  PacoteRecebido pacote = receberMensagemLoRa(tempoEsperaDataMs);

  if (!pacote.recebido) {
    Serial.println("Timeout: nenhum DATA recebido.");
    registrarCsvTimeout(cicloAtual);
    Serial.println("======================");
    return;
  }

  Serial.print("Recebido: ");
  Serial.println(pacote.mensagem);
  Serial.print("RSSI: ");
  Serial.print(pacote.rssi);
  Serial.println(" dBm");
  Serial.print("SNR: ");
  Serial.print(pacote.snr);
  Serial.println(" dB");

  String aluno = extrairCampoIcnp(pacote.mensagem, "ALUNO");
  String seq = extrairCampoIcnp(pacote.mensagem, "SEQ");

  if (!pacoteEhDoTipoIcnp(pacote.mensagem, "DATA") || aluno.length() == 0 || seq.length() == 0) {
    Serial.println("Pacote invalido para este ciclo.");
    registrarCsvPacoteInvalido(cicloAtual, pacote.rssi, pacote.snr);
    Serial.println("======================");
    return;
  }

  String ack = montarAckIcnp(aluno, seq, cicloAtual);

  Serial.print("Enviando: ");
  Serial.println(ack);

  enviarMensagemLoRa(ack);

  registrarCsvSucesso(cicloAtual, aluno, seq, pacote.rssi, pacote.snr);

  Serial.println("Ciclo concluido com ACK.");
  Serial.println("======================");
}