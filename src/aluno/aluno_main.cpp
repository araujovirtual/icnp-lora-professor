#include <Arduino.h>
#include <math.h>

#include "radio_lora.h"
#include "protocolo_icnp.h"
#include "display_oled.h"
#include "bateria.h"
#include "led_sync.h"
#include "sensor_fisiologico.h"

#ifndef ID_ALUNO_CONFIG
#define ID_ALUNO_CONFIG "2"
#endif

// 0 = apenas imprime PPG no Serial.
// 1 = envia pacote ICNP TIPO=PPG após ACK válido.
// Recomendo deixar 0 no primeiro teste.
#define ENVIAR_PPG_DEBUG_LORA 1


#define AMOSTRAS_PPG_API 32
#define INTERVALO_MINIMO_PPG_MS 2000

const String ID_ALUNO = ID_ALUNO_CONFIG;

const unsigned long tempoEsperaAckMs = 2000;
const unsigned long intervaloMinimoEntreDadosMs = 300;
const unsigned long intervaloStatusSensorMs = 2000;

unsigned long contadorSeq = 0;
unsigned long ultimoEnvioData = 0;
unsigned long ultimoStatusSensor = 0;
unsigned long ultimoEnvioPpg = 0;

unsigned long ultimoCicloTela = 0;
unsigned long ultimaSeqTela = 0;
String ultimoStatusTela = "AGUARDANDO";

String montarLinhaEnergia(float tensao) {
  String status = statusBateria(tensao);

  if (isnan(tensao)) {
    return "--";
  }

  String tensaoTexto = String(tensao, 2) + " V";

  if (status == "OK") {
    return tensaoTexto;
  }

  return status + " " + tensaoTexto;
}

String montarBatParaPacote(float tensao) {
  if (isnan(tensao)) {
    return "NA";
  }

  return String(tensao, 2);
}

String textoQualidadeSensor(
  bool sensorOk,
  bool dedoDetectado,
  bool qualidadeSpo2Ok,
  int fc,
  int spo2
) {
  if (!sensorOk) {
    return "NA";
  }

  if (!dedoDetectado) {
    return "NA";
  }

  if (fc <= 0 || spo2 <= 0) {
    return "RUIM";
  }

  if (!qualidadeSpo2Ok) {
    return "RUIM";
  }

  return "OK";
}

void atualizarTelaAlunoAtual(const String &status) {
  bool sensorOk = sensorFisiologicoDisponivel();
  bool dedo = dedoDetectadoSensorFisiologico();
  bool qualidadeSpo2Ok = qualidadeSpo2OkSensorFisiologico();

  int fc = lerFrequenciaCardiacaExperimental();
  int spo2 = lerSpo2Experimental();

  float tensaoAluno = lerTensaoBateria();
  String energiaAluno = montarLinhaEnergia(tensaoAluno);

  String qualidade = textoQualidadeSensor(
    sensorOk,
    dedo,
    qualidadeSpo2Ok,
    fc,
    spo2
  );

  ultimoStatusTela = status;

  mostrarTelaAlunoSensor(
    ID_ALUNO,
    ultimoCicloTela,
    ultimaSeqTela,
    ultimoStatusTela,
    energiaAluno,
    fc,
    spo2,
    dedo,
    qualidade
  );
}

void imprimirStatusSensor() {
  bool sensorOk = sensorFisiologicoDisponivel();
  bool dedo = dedoDetectadoSensorFisiologico();
  bool qualidadeSpo2Ok = qualidadeSpo2OkSensorFisiologico();

  int fc = lerFrequenciaCardiacaExperimental();
  int spo2 = lerSpo2Experimental();

  long ir = lerIrSensorFisiologico();
  long red = lerRedSensorFisiologico();

  String qualidade = textoQualidadeSensor(
    sensorOk,
    dedo,
    qualidadeSpo2Ok,
    fc,
    spo2
  );

  String ppg = montarJanelaPpgNormalizadaApi(AMOSTRAS_PPG_API);

  Serial.print("STATUS_SENSOR;");
  Serial.print("ALUNO=");
  Serial.print(ID_ALUNO);
  Serial.print(";SENSOR=");
  Serial.print(sensorOk ? "OK" : "NA");
  Serial.print(";DEDO=");
  Serial.print(dedo ? "1" : "0");
  Serial.print(";QUAL=");
  Serial.print(qualidade);
  Serial.print(";FC=");
  Serial.print(fc > 0 ? String(fc) : "NA");
  Serial.print(";SPO2=");
  Serial.print(spo2 > 0 ? String(spo2) : "NA");
  Serial.print(";IR=");
  Serial.print(ir);
  Serial.print(";RED=");
  Serial.print(red);
  Serial.print(";PPG=");
  Serial.print(ppg.length() > 0 ? ppg : "NA");
  Serial.println();
}

String montarPacotePpgDebug(unsigned long seq, unsigned long ciclo) {
  String ppg = montarJanelaPpgNormalizadaApi(AMOSTRAS_PPG_API);

  if (ppg.length() == 0) {
    return "";
  }

  String pacote = "ICNP;TIPO=PPG;ALUNO=" + ID_ALUNO +
                  ";SEQ=" + String(seq) +
                  ";CICLO=" + String(ciclo) +
                  ";N=" + String(AMOSTRAS_PPG_API) +
                  ";PPG=" + ppg;

  return pacote;
}

void tentarEnviarPpgDebug(unsigned long seq, unsigned long ciclo, bool dedo, const String &qualidade) {
#if ENVIAR_PPG_DEBUG_LORA
  if (!dedo) {
    return;
  }

  if (qualidade != "OK") {
    return;
  }

  unsigned long agora = millis();

  if (agora - ultimoEnvioPpg < INTERVALO_MINIMO_PPG_MS) {
    return;
  }

  ultimoEnvioPpg = agora;

  String pacotePpg = montarPacotePpgDebug(seq, ciclo);

  if (pacotePpg.length() == 0) {
    Serial.println("PPG debug nao enviado: janela PPG insuficiente.");
    return;
  }

  Serial.print("Enviando PPG debug: ");
  Serial.println(pacotePpg);

  delay(80);
  enviarMensagemLoRa(pacotePpg);
  pulsoLedSync(30);
#else
  (void)seq;
  (void)ciclo;
  (void)dedo;
  (void)qualidade;
#endif
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ALUNO - ICNP COM SENSOR PPG REAL");
  Serial.println("Placa: Heltec WiFi LoRa 32 V2");
  Serial.println("Frequencia: 915 MHz");
  Serial.println("ID_ALUNO: " + ID_ALUNO);
  Serial.println("Fluxo: BEACON(ALVO) -> DATA(FC/SPO2/BAT/IR/RED/DEDO/QUAL) -> ACK");
  Serial.println("Base: fluxo ICNP valido anterior + sensor fisiologico em tasks");
  Serial.println("Debug PPG API: buffer normalizado disponivel no STATUS_SENSOR");
#if ENVIAR_PPG_DEBUG_LORA
  Serial.println("Envio LoRa TIPO=PPG: ATIVADO");
#else
  Serial.println("Envio LoRa TIPO=PPG: DESATIVADO");
#endif
  Serial.println("================================");

  iniciarRadioLoRa();
  iniciarDisplayOled();
  iniciarMonitorBateria();
  iniciarLedSync();

  mostrarTelaAluno(ID_ALUNO, 0, 0, "SENSOR", "--");

  Serial.println("Iniciando sensor fisiologico...");
  iniciarSensorFisiologico();

  if (sensorFisiologicoDisponivel()) {
    iniciarTasksSensorFisiologico();
    Serial.println("Sensor fisiologico iniciado com tasks.");
  } else {
    Serial.println("Sensor fisiologico NAO encontrado. DATA sera enviado com campos NA/RUIM.");
  }

  atualizarTelaAlunoAtual("AGUARDANDO");

  Serial.println("LoRa iniciado com sucesso.");
  Serial.println("Display iniciado com sucesso.");
  Serial.println("Monitor de bateria iniciado com sucesso.");
  Serial.println("LED de sincronismo iniciado com sucesso.");
  Serial.println("Aguardando BEACON do Professor...");
}

void loop() {
  atualizarLedSync();

  unsigned long agoraLoop = millis();

  if (agoraLoop - ultimoStatusSensor >= intervaloStatusSensorMs) {
    ultimoStatusSensor = agoraLoop;
    imprimirStatusSensor();
    atualizarTelaAlunoAtual(ultimoStatusTela);
  }

  PacoteRecebido beacon = receberMensagemLoRa(200);

  if (!beacon.recebido) {
    return;
  }

  if (!pacoteEhDoTipoIcnp(beacon.mensagem, ICNP_TIPO_BEACON)) {
    return;
  }

  String cicloTexto = extrairCampoIcnp(beacon.mensagem, "CICLO");
  String alvo = extrairCampoIcnp(beacon.mensagem, "ALVO");

  float tensaoAluno = lerTensaoBateria();
  String batAluno = montarBatParaPacote(tensaoAluno);
  String energiaAluno = montarLinhaEnergia(tensaoAluno);
  String statusEnergia = statusBateria(tensaoAluno);

  if (cicloTexto.length() == 0 || alvo.length() == 0) {
    pulsoLedSync(300);

    Serial.println("BEACON invalido: campo CICLO ou ALVO ausente.");
    ultimoStatusTela = "BEACON INV";
    atualizarTelaAlunoAtual(ultimoStatusTela);
    return;
  }

  unsigned long ciclo = cicloTexto.toInt();

  if (alvo != ID_ALUNO) {
    pulsoLedSync(30);

    Serial.println();
    Serial.println("BEACON recebido, mas nao e para este aluno.");
    Serial.print("Meu ID: ");
    Serial.println(ID_ALUNO);
    Serial.print("ALVO: ");
    Serial.println(alvo);
    Serial.print("Energia Aluno: ");
    Serial.println(energiaAluno);
    Serial.print("Status Energia: ");
    Serial.println(statusEnergia);

    ultimoCicloTela = ciclo;
    ultimoStatusTela = "IGNORADO";
    atualizarTelaAlunoAtual(ultimoStatusTela);
    return;
  }

  pulsoLedSync(80);

  Serial.println();
  Serial.println("===== BEACON RECEBIDO PARA ESTE ALUNO =====");
  Serial.print("Mensagem: ");
  Serial.println(beacon.mensagem);
  Serial.print("Ciclo: ");
  Serial.println(ciclo);
  Serial.print("Alvo: ");
  Serial.println(alvo);
  Serial.print("RSSI BEACON: ");
  Serial.print(beacon.rssi);
  Serial.println(" dBm");
  Serial.print("SNR BEACON: ");
  Serial.print(beacon.snr);
  Serial.println(" dB");

  Serial.print("Energia Aluno: ");
  Serial.println(energiaAluno);
  Serial.print("Status Energia: ");
  Serial.println(statusEnergia);
  Serial.print("BAT enviada no DATA: ");
  Serial.println(batAluno);

  unsigned long agora = millis();

  if (agora - ultimoEnvioData < intervaloMinimoEntreDadosMs) {
    Serial.println("DATA ignorado: intervalo minimo ainda nao atingido.");
    Serial.println("===========================");

    ultimoCicloTela = ciclo;
    ultimoStatusTela = "AGUARDANDO";
    atualizarTelaAlunoAtual(ultimoStatusTela);
    return;
  }

  ultimoEnvioData = agora;

  bool sensorOk = sensorFisiologicoDisponivel();
  bool dedo = dedoDetectadoSensorFisiologico();
  bool qualidadeSpo2Ok = qualidadeSpo2OkSensorFisiologico();

  int frequenciaCardiaca = lerFrequenciaCardiacaExperimental();
  int spo2 = lerSpo2Experimental();

  long ir = lerIrSensorFisiologico();
  long red = lerRedSensorFisiologico();

  String qualidade = textoQualidadeSensor(
    sensorOk,
    dedo,
    qualidadeSpo2Ok,
    frequenciaCardiaca,
    spo2
  );

  ultimoCicloTela = ciclo;
  ultimaSeqTela = contadorSeq;
  ultimoStatusTela = "ENVIANDO";

  mostrarTelaAlunoSensor(
    ID_ALUNO,
    ultimoCicloTela,
    ultimaSeqTela,
    ultimoStatusTela,
    energiaAluno,
    frequenciaCardiaca,
    spo2,
    dedo,
    qualidade
  );

  String data = montarDataIcnp(
    ID_ALUNO,
    contadorSeq,
    ciclo,
    frequenciaCardiaca,
    spo2,
    batAluno,
    ir,
    red,
    dedo,
    qualidade
  );

  Serial.print("FC enviada: ");
  Serial.println(frequenciaCardiaca > 0 ? String(frequenciaCardiaca) : "NA");
  Serial.print("SpO2 enviado: ");
  Serial.println(spo2 > 0 ? String(spo2) : "NA");
  Serial.print("IR enviado: ");
  Serial.println(ir);
  Serial.print("RED enviado: ");
  Serial.println(red);
  Serial.print("Dedo: ");
  Serial.println(dedo ? "SIM" : "NAO");
  Serial.print("Qualidade: ");
  Serial.println(qualidade);

  String ppgSerial = montarJanelaPpgNormalizadaApi(AMOSTRAS_PPG_API);
  Serial.print("PPG janela normalizada: ");
  Serial.println(ppgSerial.length() > 0 ? ppgSerial : "NA");

  Serial.print("Enviando DATA: ");
  Serial.println(data);

  delay(250);

  enviarMensagemLoRa(data);
  pulsoLedSync(40);

  PacoteRecebido ack = receberMensagemLoRa(tempoEsperaAckMs);

  if (!ack.recebido) {
    pulsoLedSync(500);

    Serial.println("Timeout: ACK nao recebido.");
    Serial.println("===========================");

    ultimoStatusTela = "TIMEOUT";
    atualizarTelaAlunoAtual(ultimoStatusTela);

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
    pulsoLedSync(150);

    Serial.println("ACK valido. Ciclo ICNP concluido.");

    ultimoStatusTela = "OK";
    atualizarTelaAlunoAtual(ultimoStatusTela);

    tentarEnviarPpgDebug(contadorSeq, ciclo, dedo, qualidade);
  } else {
    pulsoLedSync(300);

    Serial.println("ACK invalido: aluno, sequencia ou ciclo nao conferem.");

    ultimoStatusTela = "ACK INV";
    atualizarTelaAlunoAtual(ultimoStatusTela);
  }

  Serial.println("===========================");

  contadorSeq++;
}