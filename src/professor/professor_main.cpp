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

const unsigned long intervaloBeaconMs = 3000;
const unsigned long tempoEsperaDataMs = 1200;

unsigned long ultimoBeacon = 0;
unsigned long cicloAtual = 0;

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
  Serial.println("PROFESSOR - ICNP MINIMO");
  Serial.println("Placa: Heltec WiFi LoRa 32 V2");
  Serial.println("Frequencia: 915 MHz");
  Serial.println("================================");

  iniciarLoRa();

  Serial.println("LoRa iniciado com sucesso.");
  Serial.println("Iniciando ciclos ICNP...");
}

void loop() {
  unsigned long agora = millis();

  if (agora - ultimoBeacon < intervaloBeaconMs) {
    return;
  }

  ultimoBeacon = agora;
  cicloAtual++;

  String beacon = "ICNP;TIPO=BEACON;PROFESSOR=1;CICLO=" + String(cicloAtual);

  Serial.println();
  Serial.println("===== CICLO ICNP =====");
  Serial.print("Enviando: ");
  Serial.println(beacon);

  enviarMensagem(beacon);

  int rssi = 0;
  float snr = 0.0;

  String data = receberMensagem(tempoEsperaDataMs, rssi, snr);

  if (data.length() == 0) {
    Serial.println("Timeout: nenhum DATA recebido.");
    Serial.println("======================");
    return;
  }

  Serial.print("Recebido: ");
  Serial.println(data);
  Serial.print("RSSI: ");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.print("SNR: ");
  Serial.print(snr);
  Serial.println(" dB");

  String tipo = extrairCampo(data, "TIPO");
  String aluno = extrairCampo(data, "ALUNO");
  String seq = extrairCampo(data, "SEQ");

  if (tipo != "DATA" || aluno.length() == 0 || seq.length() == 0) {
    Serial.println("Pacote invalido para este ciclo.");
    Serial.println("======================");
    return;
  }

  String ack = "ICNP;TIPO=ACK;PROFESSOR=1;ALUNO=" + aluno + ";SEQ=" + seq + ";CICLO=" + String(cicloAtual);

  Serial.print("Enviando: ");
  Serial.println(ack);

  enviarMensagem(ack);

  Serial.println("Ciclo concluido com ACK.");
  Serial.println("======================");
}