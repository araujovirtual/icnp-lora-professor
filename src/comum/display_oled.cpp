#include "display_oled.h"

#include <Wire.h>
#include <U8g2lib.h>
#include "sensor_fisiologico.h"

#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16

// Hardware atual do projeto: OLED SSD1306 128x64 em U8g2, rotacao correta R0.
// Importante no Heltec WiFi LoRa 32 V2: o barramento I2C do OLED usa SDA=4 e SCL=15.
// O U8g2 em HW_I2C usa a instancia Wire; por isso o Wire.begin(4,15) precisa ocorrer
// antes do display.begin(), principalmente no Professor, que nao inicializa MAX30102/MPU.
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, OLED_RST);

static String valorOuNa(int valor) {
  return valor > 0 ? String(valor) : String("NA");
}

static int textoLargura(const char *txt) {
  return display.getStrWidth(txt ? txt : "");
}

static void textoCentralizado(int y, const String &texto) {
  int x = (128 - textoLargura(texto.c_str())) / 2;
  if (x < 0) x = 0;
  display.drawStr(x, y, texto.c_str());
}

static void enviar() {
  bloquearI2cCompartilhado();
  display.sendBuffer();
  desbloquearI2cCompartilhado();
}

void iniciarDisplayOled() {
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);
  delay(100);

  bloquearI2cCompartilhado();
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);
  display.setI2CAddress(0x3C * 2);
  display.begin();
  display.setBusClock(400000);
  display.clearBuffer();
  display.setFont(u8g2_font_logisoso16_tf);
  textoCentralizado(22, "ICNP LoRa");
  display.setFont(u8g2_font_6x12_tf);
  textoCentralizado(42, "Inicializando");
  display.sendBuffer();
  desbloquearI2cCompartilhado();
}


void mostrarTelaAlunoCalibracao(
  const String &titulo,
  const String &mensagem,
  int progressoPercentual,
  int segundosRestantes
) {
  if (progressoPercentual < 0) progressoPercentual = 0;
  if (progressoPercentual > 100) progressoPercentual = 100;

  int barra = map(progressoPercentual, 0, 100, 0, 124);

  bloquearI2cCompartilhado();
  display.clearBuffer();

  display.setFont(u8g2_font_logisoso16_tf);
  textoCentralizado(17, titulo);

  display.setFont(u8g2_font_6x12_tf);
  textoCentralizado(32, mensagem);

  String pct = String(progressoPercentual) + "%";
  String tempo = segundosRestantes > 0 ? (String(segundosRestantes) + "s") : String("OK");
  display.drawStr(0, 45, pct.c_str());
  int xt = 128 - textoLargura(tempo.c_str());
  if (xt < 0) xt = 0;
  display.drawStr(xt, 45, tempo.c_str());

  display.drawFrame(0, 54, 128, 9);
  display.drawBox(2, 56, barra, 5);

  display.sendBuffer();
  desbloquearI2cCompartilhado();
}

void mostrarTelaAluno(
  const String &idAluno,
  unsigned long ciclo,
  unsigned long sequencia,
  const String &status,
  const String &energia
) {
  bloquearI2cCompartilhado();
  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0, 10, ("ALUNO " + idAluno + " ICNP").c_str());
  display.drawStr(0, 24, ("C:" + String(ciclo) + " S:" + String(sequencia)).c_str());
  display.drawStr(0, 38, ("ST:" + status).c_str());
  display.drawStr(0, 52, ("BAT:" + energia).c_str());
  display.sendBuffer();
  desbloquearI2cCompartilhado();
}

void mostrarTelaAlunoSensor(
  const String &idAluno,
  unsigned long ciclo,
  unsigned long sequencia,
  const String &status,
  const String &energia,
  int frequenciaCardiaca,
  int spo2,
  bool dedoDetectado,
  const String &qualidade
) {
  bloquearI2cCompartilhado();
  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0, 10, ("ALUNO " + idAluno + " " + status).c_str());
  display.drawStr(0, 24, ("FC:" + valorOuNa(frequenciaCardiaca) + " Sp:" + valorOuNa(spo2)).c_str());
  display.drawStr(0, 38, ("Dedo:" + String(dedoDetectado ? "SIM" : "NAO") + " Q:" + qualidade).c_str());
  display.drawStr(0, 52, ("C:" + String(ciclo) + " S:" + String(sequencia) + " B:" + energia).c_str());
  display.sendBuffer();
  desbloquearI2cCompartilhado();
}

void mostrarTelaAlunoBiometrico(
  const String &idAluno,
  unsigned long ciclo,
  unsigned long sequencia,
  const String &status,
  const String &energia,
  int frequenciaCardiaca,
  int spo2,
  int pressaoSistolica,
  int pressaoDiastolica,
  bool dedoDetectado,
  const String &qualidade,
  const String &uso,
  const String &sinalPpg,
  float sinalOnda
) {
  bool paValida = paValidaSensorFisiologico();
  String mov = lerMovimentoSensorFisiologico();
  String artefato = lerArtefatoPpgSensorFisiologico();

  String fcTxt = paValida && frequenciaCardiaca > 0 ? String(frequenciaCardiaca) : "NA";
  String spTxt = paValida && spo2 > 0 ? String(spo2) : "NA";
  String paTxt = (paValida && pressaoSistolica > 0 && pressaoDiastolica > 0) ? (String(pressaoSistolica) + "x" + String(pressaoDiastolica)) : "NA";

  bloquearI2cCompartilhado();
  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0, 9, ("ALUNO " + idAluno + " " + status).c_str());
  display.drawStr(0, 22, ("FC " + fcTxt + "  SpO2 " + spTxt).c_str());
  display.drawStr(0, 35, ("PA " + paTxt + "  V:" + String(paValida ? "1" : "0")).c_str());
  display.drawStr(0, 48, ("MOV " + mov.substring(0, 12)).c_str());
  display.drawStr(0, 61, ("ART " + artefato + " " + (dedoDetectado ? "DEDO" : "SEM")).c_str());
  display.sendBuffer();
  desbloquearI2cCompartilhado();

  (void)ciclo; (void)sequencia; (void)energia; (void)qualidade; (void)uso; (void)sinalPpg; (void)sinalOnda;
}

void mostrarTelaProfessor(
  unsigned long ciclo,
  const String &alvo,
  const String &status,
  int rssi,
  const String &energia
) {
  bloquearI2cCompartilhado();
  display.clearBuffer();
  display.setFont(u8g2_font_logisoso16_tf);
  textoCentralizado(18, "PROFESSOR");
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0, 34, ("Ciclo: " + String(ciclo)).c_str());
  display.drawStr(0, 47, ("Alvo " + alvo + " " + status).c_str());
  display.drawStr(0, 60, ("R:" + String(rssi) + " B:" + energia).c_str());
  display.sendBuffer();
  desbloquearI2cCompartilhado();
}
