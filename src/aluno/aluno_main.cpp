#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#define FREQUENCIA_LORA 915E6

#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_SS 18
#define LORA_RST 14
#define LORA_DIO0 26

const String ID_ALUNO = "1";

const unsigned long tempoEsperaAckMs = 1200;
const unsigned long intervaloMinimoEntreDadosMs = 300;

unsigned long contadorSeq = 0;
unsigned long ultimoEnvioData = 0;

void iniciarLoRa() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(FREQUENCIA_LORA)) {
    Serial.println("ERRO: Falha ao iniciar LoRa.");
    while (true) {
      delay(1000);
    }
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
}

void enviarMensagem(const String &mensagem) {
  LoRa.beginPacket();
  LoRa.print(mensagem);
  LoRa.endPacket();
}

String receberMensagem(unsigned long tempoLimiteMs, int &rssi, float &snr) {
  unsigned long inicio = millis();

  while (millis() - inicio < tempoLimiteMs) {
    int tamanhoPacote = LoRa.parsePacket();

    if (tamanhoPacote > 0) {
      String mensagem = "";

      while (LoRa.available()) {
        mensagem += (char)LoRa.read();
      }

      rssi = LoRa.packetRssi();
      snr = LoRa.packetSnr();

      return mensagem;
    }
  }

  return "";
}

String extrairCampo(const String &mensagem, const String &campo) {
  int inicio = mensagem.indexOf(campo + "=");

  if (inicio < 0) {
    return "";
  }

  inicio += campo.length() + 1;
  int fim = mensagem.indexOf(";", inicio);

  if (fim < 0) {
    fim = mensagem.length();
  }

  return mensagem.substring(inicio, fim);
}

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

  iniciarLoRa();

  Serial.println("LoRa iniciado com sucesso.");
  Serial.println("Aguardando BEACON do Professor...");
}

void loop() {
  int rssiBeacon = 0;
  float snrBeacon = 0.0;

  String beacon = receberMensagem(200, rssiBeacon, snrBeacon);

  if (beacon.length() == 0) {
    return;
  }

  String tipo = extrairCampo(beacon, "TIPO");

  if (tipo != "BEACON") {
    return;
  }

  String ciclo = extrairCampo(beacon, "CICLO");

  Serial.println();
  Serial.println("===== BEACON RECEBIDO =====");
  Serial.print("Mensagem: ");
  Serial.println(beacon);
  Serial.print("RSSI BEACON: ");
  Serial.print(rssiBeacon);
  Serial.println(" dBm");
  Serial.print("SNR BEACON: ");
  Serial.print(snrBeacon);
  Serial.println(" dB");

  unsigned long agora = millis();

  if (agora - ultimoEnvioData < intervaloMinimoEntreDadosMs) {
    Serial.println("DATA ignorado: intervalo minimo ainda nao atingido.");
    Serial.println("===========================");
    return;
  }

  ultimoEnvioData = agora;

  String data = "ICNP;TIPO=DATA;ALUNO=" + ID_ALUNO +
                ";SEQ=" + String(contadorSeq) +
                ";CICLO=" + ciclo +
                ";FC=72;SPO2=98";

  Serial.print("Enviando: ");
  Serial.println(data);

  enviarMensagem(data);

  int rssiAck = 0;
  float snrAck = 0.0;

  String ack = receberMensagem(tempoEsperaAckMs, rssiAck, snrAck);

  if (ack.length() == 0) {
    Serial.println("Timeout: ACK nao recebido.");
    Serial.println("===========================");
    contadorSeq++;
    return;
  }

  Serial.print("Recebido: ");
  Serial.println(ack);
  Serial.print("RSSI ACK: ");
  Serial.print(rssiAck);
  Serial.println(" dBm");
  Serial.print("SNR ACK: ");
  Serial.print(snrAck);
  Serial.println(" dB");

  String tipoAck = extrairCampo(ack, "TIPO");
  String alunoAck = extrairCampo(ack, "ALUNO");
  String seqAck = extrairCampo(ack, "SEQ");

  if (tipoAck == "ACK" && alunoAck == ID_ALUNO && seqAck == String(contadorSeq)) {
    Serial.println("ACK valido. Ciclo ICNP concluido.");
  } else {
    Serial.println("ACK invalido ou pertencente a outro pacote.");
  }

  Serial.println("===========================");

  contadorSeq++;
}