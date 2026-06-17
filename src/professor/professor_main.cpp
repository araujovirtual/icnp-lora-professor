#include <Arduino.h>

#include "radio_lora.h"
#include "protocolo_icnp.h"
#include "display_oled.h"
#include "bateria.h"
#include "led_sync.h"
#include "api_professor.h"

// ============================================================
// CONFIGURACAO DA API
// ============================================================
// 1 = API ligada em Wi-Fi STA.
// 0 = API desligada.
// ============================================================

#define API_PROFESSOR_ATIVA 1

// ============================================================
// CONFIGURACAO ICNP
// ============================================================

const unsigned long intervaloBeaconMs = 2000;
const unsigned long tempoEsperaDataMs = 1200;

const String alunos[] = {"1", "2"};
const int totalAlunos = 2;

unsigned long ultimoBeacon = 0;
unsigned long cicloAtual = 0;
int indiceAlunoAtual = 0;

// ============================================================
// ESTADO LOCAL DOS ALUNOS NO PROFESSOR
// ============================================================

struct EstadoAluno {
  String id;
  bool atualizado;
  unsigned long ultimoCiclo;
  unsigned long ultimaSequencia;
  int fc;
  int spo2;
  long ir;
  long red;
  bool dedo;
  String qualidade;
  String batAluno;
  int rssi;
  float snr;
  unsigned long instanteMs;
};

EstadoAluno estadoAlunos[totalAlunos];

// ============================================================
// FUNCOES AUXILIARES
// ============================================================

void inicializarEstadoAlunos() {
  for (int i = 0; i < totalAlunos; i++) {
    estadoAlunos[i].id = alunos[i];
    estadoAlunos[i].atualizado = false;
    estadoAlunos[i].ultimoCiclo = 0;
    estadoAlunos[i].ultimaSequencia = 0;
    estadoAlunos[i].fc = 0;
    estadoAlunos[i].spo2 = 0;
    estadoAlunos[i].ir = 0;
    estadoAlunos[i].red = 0;
    estadoAlunos[i].dedo = false;
    estadoAlunos[i].qualidade = "NA";
    estadoAlunos[i].batAluno = "NA";
    estadoAlunos[i].rssi = 0;
    estadoAlunos[i].snr = 0.0f;
    estadoAlunos[i].instanteMs = 0;
  }
}

void atualizarEstadoAluno(
  const String &aluno,
  unsigned long ciclo,
  unsigned long sequencia,
  int frequenciaCardiaca,
  int spo2,
  long ir,
  long red,
  bool dedo,
  const String &qualidade,
  const String &batAluno,
  int rssi,
  float snr
) {
  for (int i = 0; i < totalAlunos; i++) {
    if (estadoAlunos[i].id == aluno) {
      estadoAlunos[i].atualizado = true;
      estadoAlunos[i].ultimoCiclo = ciclo;
      estadoAlunos[i].ultimaSequencia = sequencia;
      estadoAlunos[i].fc = frequenciaCardiaca;
      estadoAlunos[i].spo2 = spo2;
      estadoAlunos[i].ir = ir;
      estadoAlunos[i].red = red;
      estadoAlunos[i].dedo = dedo;
      estadoAlunos[i].qualidade = qualidade;
      estadoAlunos[i].batAluno = batAluno;
      estadoAlunos[i].rssi = rssi;
      estadoAlunos[i].snr = snr;
      estadoAlunos[i].instanteMs = millis();
      return;
    }
  }
}

void registrarCsvSucesso(
  unsigned long ciclo,
  const String &alvo,
  const String &aluno,
  unsigned long sequencia,
  int frequenciaCardiaca,
  int spo2,
  long ir,
  long red,
  bool dedo,
  const String &qualidade,
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
  Serial.print(frequenciaCardiaca > 0 ? String(frequenciaCardiaca) : "NA");
  Serial.print(";SPO2=");
  Serial.print(spo2 > 0 ? String(spo2) : "NA");
  Serial.print(";IR=");
  Serial.print(ir);
  Serial.print(";RED=");
  Serial.print(red);
  Serial.print(";DEDO=");
  Serial.print(dedo ? "1" : "0");
  Serial.print(";QUAL=");
  Serial.print(qualidade);
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
  Serial.print(";IR=NA");
  Serial.print(";RED=NA");
  Serial.print(";DEDO=NA");
  Serial.print(";QUAL=NA");
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
  Serial.print(";IR=INVALIDO");
  Serial.print(";RED=INVALIDO");
  Serial.print(";DEDO=INVALIDO");
  Serial.print(";QUAL=INVALIDO");
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

bool camposObrigatoriosPresentes(
  const String &aluno,
  const String &seqTexto,
  const String &cicloTexto,
  const String &fcTexto,
  const String &spo2Texto,
  const String &batAluno,
  const String &irTexto,
  const String &redTexto,
  const String &dedoTexto,
  const String &qualidade
) {
  return (
    aluno.length() > 0 &&
    seqTexto.length() > 0 &&
    cicloTexto.length() > 0 &&
    fcTexto.length() > 0 &&
    spo2Texto.length() > 0 &&
    batAluno.length() > 0 &&
    irTexto.length() > 0 &&
    redTexto.length() > 0 &&
    dedoTexto.length() > 0 &&
    qualidade.length() > 0
  );
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("PROFESSOR - ICNP COM PPG REAL + API WIFI STA");
  Serial.println("Placa: Heltec WiFi LoRa 32 V2");
  Serial.println("Fluxo: BEACON(ALVO) -> DATA(FC/SPO2/IR/RED/DEDO/QUAL/BAT) -> ACK");
  Serial.println("API no Professor via Wi-Fi STA");
  Serial.println("================================");

  inicializarEstadoAlunos();

  iniciarDisplayOled();
  iniciarMonitorBateria();
  iniciarLedSync();

#if API_PROFESSOR_ATIVA
  iniciarApiProfessor();
#else
  Serial.println("API Professor: DESLIGADA");
#endif

  iniciarRadioLoRa();

  mostrarTelaProfessor(0, "-", "AGUARDANDO", 0, textoEnergia());

  Serial.println("Formato CSV:");
  Serial.println("CSV;CICLO=<n>;ALVO=<id>;ALUNO=<id>;SEQ=<seq>;FC=<fc|NA>;SPO2=<spo2|NA>;IR=<ir>;RED=<red>;DEDO=<0|1>;QUAL=<OK|RUIM|NA>;RSSI=<dBm>;SNR=<dB>;BAT_ALUNO=<V|NA>;ENERGIA_PROF=<V|NA>;ACK=<0|1>");
}

// ============================================================
// LOOP PRINCIPAL ICNP / LORA
// ============================================================

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
  Serial.println("===== CICLO ICNP =====");
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
  String irTexto = extrairCampoIcnp(pacote.mensagem, "IR");
  String redTexto = extrairCampoIcnp(pacote.mensagem, "RED");
  String dedoTexto = extrairCampoIcnp(pacote.mensagem, "DEDO");
  String qualidade = extrairCampoIcnp(pacote.mensagem, "QUAL");

  if (!camposObrigatoriosPresentes(
    aluno,
    seqTexto,
    cicloTexto,
    fcTexto,
    spo2Texto,
    batAluno,
    irTexto,
    redTexto,
    dedoTexto,
    qualidade
  )) {
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
    registrarCsvPacoteInvalido(cicloAtual, alvo, pacote.rssi, pacote.snr, energiaProfessor);

    mostrarTelaProfessor(cicloAtual, alvo, "ALUNO INV", pacote.rssi, energiaProfessor);

    Serial.println("======================");
    return;
  }

  unsigned long sequencia = seqTexto.toInt();
  unsigned long cicloRecebido = cicloTexto.toInt();
  int frequenciaCardiaca = fcTexto.toInt();
  int spo2 = spo2Texto.toInt();
  long ir = irTexto.toInt();
  long red = redTexto.toInt();
  bool dedo = dedoTexto == "1";

  if (cicloRecebido != cicloAtual) {
    pulsoLedSync(300);

    Serial.println("Pacote invalido: DATA pertence a outro ciclo.");
    registrarCsvPacoteInvalido(cicloAtual, alvo, pacote.rssi, pacote.snr, energiaProfessor);

    mostrarTelaProfessor(cicloAtual, alvo, "CICLO INV", pacote.rssi, energiaProfessor);

    Serial.println("======================");
    return;
  }

  String ack = montarAckIcnp(aluno, sequencia, cicloAtual);

  Serial.print("FC recebida: ");
  Serial.println(frequenciaCardiaca);
  Serial.print("SpO2 recebido: ");
  Serial.println(spo2);
  Serial.print("IR recebido: ");
  Serial.println(ir);
  Serial.print("RED recebido: ");
  Serial.println(red);
  Serial.print("Dedo detectado: ");
  Serial.println(dedo ? "SIM" : "NAO");
  Serial.print("Qualidade recebida: ");
  Serial.println(qualidade);
  Serial.print("BAT Aluno recebida: ");
  Serial.println(batAluno);
  Serial.print("Enviando ACK: ");
  Serial.println(ack);

  enviarMensagemLoRa(ack);
  pulsoLedSync(150);

  atualizarEstadoAluno(
    aluno,
    cicloAtual,
    sequencia,
    frequenciaCardiaca,
    spo2,
    ir,
    red,
    dedo,
    qualidade,
    batAluno,
    pacote.rssi,
    pacote.snr
  );

  registrarCsvSucesso(
    cicloAtual,
    alvo,
    aluno,
    sequencia,
    frequenciaCardiaca,
    spo2,
    ir,
    red,
    dedo,
    qualidade,
    pacote.rssi,
    pacote.snr,
    batAluno,
    energiaProfessor
  );

#if API_PROFESSOR_ATIVA
  atualizarEstadoAlunoAPI(
    aluno.toInt(),
    (int)sequencia,
    (int)cicloAtual,
    frequenciaCardiaca > 0 ? frequenciaCardiaca : -1,
    spo2 > 0 ? spo2 : -1,
    ir,
    red,
    dedo ? 1 : 0,
    qualidade,
    pacote.rssi,
    pacote.snr,
    batAluno.toFloat(),
    energiaProfessor.toFloat(),
    1
  );
#endif

  mostrarTelaProfessor(cicloAtual, alvo, "OK A" + aluno, pacote.rssi, energiaProfessor);

  Serial.println("Ciclo concluido com ACK.");
  Serial.println("======================");
}