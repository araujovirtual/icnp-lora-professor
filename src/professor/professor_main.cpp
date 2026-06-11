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
  unsigned long sequencia,
  int frequenciaCardiaca,
  int spo2,
  int rssi,
  float snr
) {
  Serial.print("CSV;");
  Serial.print("CICLO=");
  Serial.print(ciclo);
  Serial.print(";ALUNO=");
  Serial.print(aluno);
  Serial.print(";SEQ=");
  Serial.print(sequencia);
  Serial.print(";FC=");
  Serial.print(frequenciaCardiaca);
  Serial.print(";SPO2=");
  Serial.print(spo2);
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
  Serial.print(";FC=NA");
  Serial.print(";SPO2=NA");
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
  Serial.print(";FC=INVALIDO");
  Serial.print(";SPO2=INVALIDO");
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
  Serial.println("PROFESSOR - ICNP COM LOG CSV");
  Serial.println("Placa: Heltec WiFi LoRa 32 V2");
  Serial.println("Frequencia: 915 MHz");
  Serial.println("Fluxo: BEACON -> DATA -> ACK");
  Serial.println("================================");

  iniciarRadioLoRa();

  Serial.println("LoRa iniciado com sucesso.");
  Serial.println("Iniciando ciclos ICNP...");
  Serial.println();
  Serial.println("Formato CSV:");
  Serial.println("CSV;CICLO=<n>;ALUNO=<id>;SEQ=<seq>;FC=<fc>;SPO2=<spo2>;RSSI=<dBm>;SNR=<dB>;ACK=<0|1>");
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
  Serial.print("Ciclo: ");
  Serial.println(cicloAtual);
  Serial.print("Enviando BEACON: ");
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
  Serial.print("RSSI DATA: ");
  Serial.print(pacote.rssi);
  Serial.println(" dBm");
  Serial.print("SNR DATA: ");
  Serial.print(pacote.snr);
  Serial.println(" dB");

  if (!pacoteEhDoTipoIcnp(pacote.mensagem, ICNP_TIPO_DATA)) {
    Serial.println("Pacote invalido: nao e DATA ICNP.");
    registrarCsvPacoteInvalido(cicloAtual, pacote.rssi, pacote.snr);
    Serial.println("======================");
    return;
  }

  String aluno = extrairCampoIcnp(pacote.mensagem, "ALUNO");
  String seqTexto = extrairCampoIcnp(pacote.mensagem, "SEQ");
  String cicloTexto = extrairCampoIcnp(pacote.mensagem, "CICLO");
  String fcTexto = extrairCampoIcnp(pacote.mensagem, "FC");
  String spo2Texto = extrairCampoIcnp(pacote.mensagem, "SPO2");

  if (
    aluno.length() == 0 ||
    seqTexto.length() == 0 ||
    cicloTexto.length() == 0 ||
    fcTexto.length() == 0 ||
    spo2Texto.length() == 0
  ) {
    Serial.println("Pacote invalido: campos obrigatorios ausentes.");
    registrarCsvPacoteInvalido(cicloAtual, pacote.rssi, pacote.snr);
    Serial.println("======================");
    return;
  }

  unsigned long sequencia = seqTexto.toInt();
  unsigned long cicloRecebido = cicloTexto.toInt();
  int frequenciaCardiaca = fcTexto.toInt();
  int spo2 = spo2Texto.toInt();

  if (cicloRecebido != cicloAtual) {
    Serial.println("Pacote invalido: DATA pertence a outro ciclo.");
    registrarCsvPacoteInvalido(cicloAtual, pacote.rssi, pacote.snr);
    Serial.println("======================");
    return;
  }

  String ack = montarAckIcnp(aluno, sequencia, cicloAtual);

  Serial.print("Enviando ACK: ");
  Serial.println(ack);

  enviarMensagemLoRa(ack);

  registrarCsvSucesso(
    cicloAtual,
    aluno,
    sequencia,
    frequenciaCardiaca,
    spo2,
    pacote.rssi,
    pacote.snr
  );

  Serial.println("Ciclo concluido com ACK.");
  Serial.println("======================");
}