#include <Arduino.h>

#include "radio_lora.h"
#include "protocolo_icnp.h"
#include "display_oled.h"
#include "bateria.h"
#include "led_sync.h"

const unsigned long intervaloBeaconMs = 2000;
const unsigned long tempoEsperaDataMs = 1000;

const String alunos[] = {"1", "2"};
const int totalAlunos = 2;

unsigned long ultimoBeacon = 0;
unsigned long cicloAtual = 0;
int indiceAlunoAtual = 0;

void registrarCsvSucesso(
  unsigned long ciclo,
  const String &alvo,
  const String &aluno,
  unsigned long sequencia,
  int frequenciaCardiaca,
  int spo2,
  int rssi,
  float snr,
  const String &batAluno,
  const String &energiaProfessor
) {
  Serial.print("CSV;");
  Serial.print("CICLO=");
  Serial.print(ciclo);
  Serial.print(";ALVO=");
  Serial.print(alvo);
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
  Serial.print(";BAT_ALUNO=");
  Serial.print(batAluno);
  Serial.print(";ENERGIA_PROF=");
  Serial.print(energiaProfessor);
  Serial.print(";ACK=1");
  Serial.println();
}

void registrarCsvTimeout(
  unsigned long ciclo,
  const String &alvo,
  const String &energiaProfessor
) {
  Serial.print("CSV;");
  Serial.print("CICLO=");
  Serial.print(ciclo);
  Serial.print(";ALVO=");
  Serial.print(alvo);
  Serial.print(";ALUNO=NA");
  Serial.print(";SEQ=NA");
  Serial.print(";FC=NA");
  Serial.print(";SPO2=NA");
  Serial.print(";RSSI=NA");
  Serial.print(";SNR=NA");
  Serial.print(";BAT_ALUNO=NA");
  Serial.print(";ENERGIA_PROF=");
  Serial.print(energiaProfessor);
  Serial.print(";ACK=0");
  Serial.println();
}

void registrarCsvPacoteInvalido(
  unsigned long ciclo,
  const String &alvo,
  int rssi,
  float snr,
  const String &energiaProfessor
) {
  Serial.print("CSV;");
  Serial.print("CICLO=");
  Serial.print(ciclo);
  Serial.print(";ALVO=");
  Serial.print(alvo);
  Serial.print(";ALUNO=INVALIDO");
  Serial.print(";SEQ=INVALIDO");
  Serial.print(";FC=INVALIDO");
  Serial.print(";SPO2=INVALIDO");
  Serial.print(";RSSI=");
  Serial.print(rssi);
  Serial.print(";SNR=");
  Serial.print(snr, 2);
  Serial.print(";BAT_ALUNO=INVALIDO");
  Serial.print(";ENERGIA_PROF=");
  Serial.print(energiaProfessor);
  Serial.print(";ACK=0");
  Serial.println();
}

String proximoAlunoAlvo() {
  String alvo = alunos[indiceAlunoAtual];

  indiceAlunoAtual++;

  if (indiceAlunoAtual >= totalAlunos) {
    indiceAlunoAtual = 0;
  }

  return alvo;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("PROFESSOR - ICNP MULTIALUNO COM LOG CSV E LED SYNC");
  Serial.println("Placa: Heltec WiFi LoRa 32 V2");
  Serial.println("Frequencia: 915 MHz");
  Serial.println("Fluxo: BEACON(ALVO) -> DATA(BAT) -> ACK");
  Serial.println("Alunos configurados: 1 e 2");
  Serial.println("LED SYNC: indicador visual de eventos ICNP");
  Serial.println("================================");

  iniciarRadioLoRa();
  iniciarDisplayOled();
  iniciarMonitorBateria();
  iniciarLedSync();

  mostrarTelaProfessor(0, "-", "AGUARDANDO", 0, textoEnergia());

  Serial.println("LoRa iniciado com sucesso.");
  Serial.println("Display iniciado com sucesso.");
  Serial.println("Monitor de bateria iniciado com sucesso.");
  Serial.println("LED de sincronismo iniciado com sucesso.");
  Serial.println("Iniciando ciclos ICNP multialuno...");
  Serial.println();
  Serial.println("Formato CSV:");
  Serial.println("CSV;CICLO=<n>;ALVO=<id>;ALUNO=<id>;SEQ=<seq>;FC=<fc>;SPO2=<spo2>;RSSI=<dBm>;SNR=<dB>;BAT_ALUNO=<V|NA>;ENERGIA_PROF=<V|NA>;ACK=<0|1>");
}

void loop() {
  atualizarLedSync();

  unsigned long agora = millis();

  if (agora - ultimoBeacon < intervaloBeaconMs) {
    return;
  }

  ultimoBeacon = agora;
  cicloAtual++;

  String alvo = proximoAlunoAlvo();
  String energiaProfessor = textoEnergia();

  String beacon = montarBeaconIcnp(cicloAtual, alvo);

  Serial.println();
  Serial.println("===== CICLO ICNP MULTIALUNO =====");
  Serial.print("Ciclo: ");
  Serial.println(cicloAtual);
  Serial.print("Alvo: ALUNO ");
  Serial.println(alvo);
  Serial.print("Energia Professor: ");
  Serial.println(energiaProfessor);
  Serial.print("Enviando BEACON: ");
  Serial.println(beacon);

  mostrarTelaProfessor(cicloAtual, alvo, "BEACON", 0, energiaProfessor);

  enviarMensagemLoRa(beacon);
  pulsoLedSync(80);

  mostrarTelaProfessor(cicloAtual, alvo, "AGUARDANDO", 0, textoEnergia());

  PacoteRecebido pacote = receberMensagemLoRa(tempoEsperaDataMs);

  energiaProfessor = textoEnergia();

  if (!pacote.recebido) {
    pulsoLedSync(500);

    Serial.println("Timeout: nenhum DATA recebido do aluno alvo.");
    registrarCsvTimeout(cicloAtual, alvo, energiaProfessor);
    mostrarTelaProfessor(cicloAtual, alvo, "TIMEOUT", 0, energiaProfessor);
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
    pulsoLedSync(300);

    Serial.println("Pacote invalido: nao e DATA ICNP.");
    registrarCsvPacoteInvalido(cicloAtual, alvo, pacote.rssi, pacote.snr, energiaProfessor);
    mostrarTelaProfessor(cicloAtual, alvo, "INVALIDO", pacote.rssi, energiaProfessor);
    Serial.println("======================");
    return;
  }

  String aluno = extrairCampoIcnp(pacote.mensagem, "ALUNO");
  String seqTexto = extrairCampoIcnp(pacote.mensagem, "SEQ");
  String cicloTexto = extrairCampoIcnp(pacote.mensagem, "CICLO");
  String fcTexto = extrairCampoIcnp(pacote.mensagem, "FC");
  String spo2Texto = extrairCampoIcnp(pacote.mensagem, "SPO2");
  String batAluno = extrairCampoIcnp(pacote.mensagem, "BAT");

  if (
    aluno.length() == 0 ||
    seqTexto.length() == 0 ||
    cicloTexto.length() == 0 ||
    fcTexto.length() == 0 ||
    spo2Texto.length() == 0 ||
    batAluno.length() == 0
  ) {
    pulsoLedSync(300);

    Serial.println("Pacote invalido: campos obrigatorios ausentes.");
    registrarCsvPacoteInvalido(cicloAtual, alvo, pacote.rssi, pacote.snr, energiaProfessor);
    mostrarTelaProfessor(cicloAtual, alvo, "INVALIDO", pacote.rssi, energiaProfessor);
    Serial.println("======================");
    return;
  }

  if (aluno != alvo) {
    pulsoLedSync(300);

    Serial.println("Pacote ignorado: DATA recebido de aluno diferente do ALVO.");
    Serial.print("ALVO esperado: ");
    Serial.println(alvo);
    Serial.print("ALUNO recebido: ");
    Serial.println(aluno);
    registrarCsvPacoteInvalido(cicloAtual, alvo, pacote.rssi, pacote.snr, energiaProfessor);
    mostrarTelaProfessor(cicloAtual, alvo, "ALUNO INV", pacote.rssi, energiaProfessor);
    Serial.println("======================");
    return;
  }

  unsigned long sequencia = seqTexto.toInt();
  unsigned long cicloRecebido = cicloTexto.toInt();
  int frequenciaCardiaca = fcTexto.toInt();
  int spo2 = spo2Texto.toInt();

  if (cicloRecebido != cicloAtual) {
    pulsoLedSync(300);

    Serial.println("Pacote invalido: DATA pertence a outro ciclo.");
    registrarCsvPacoteInvalido(cicloAtual, alvo, pacote.rssi, pacote.snr, energiaProfessor);
    mostrarTelaProfessor(cicloAtual, alvo, "CICLO INV", pacote.rssi, energiaProfessor);
    Serial.println("======================");
    return;
  }

  String ack = montarAckIcnp(aluno, sequencia, cicloAtual);

  Serial.print("BAT Aluno recebida: ");
  Serial.println(batAluno);
  Serial.print("Energia Professor: ");
  Serial.println(energiaProfessor);
  Serial.print("Enviando ACK: ");
  Serial.println(ack);

  enviarMensagemLoRa(ack);
  pulsoLedSync(150);

  registrarCsvSucesso(
    cicloAtual,
    alvo,
    aluno,
    sequencia,
    frequenciaCardiaca,
    spo2,
    pacote.rssi,
    pacote.snr,
    batAluno,
    energiaProfessor
  );

  mostrarTelaProfessor(cicloAtual, alvo, "OK A" + aluno, pacote.rssi, energiaProfessor);

  Serial.println("Ciclo concluido com ACK.");
  Serial.println("======================");
}