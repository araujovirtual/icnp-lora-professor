#include "display_oled.h"

#include <Wire.h>
#include "SSD1306Wire.h"

#define OLED_ADDR 0x3C
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16

static SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL);

static String valorOuNa(int valor) {
  if (valor <= 0) {
    return "NA";
  }

  return String(valor);
}

void iniciarDisplayOled() {
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);
  delay(100);

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  display.clear();
  display.drawString(0, 0, "ICNP LoRa");
  display.drawString(0, 12, "Inicializando...");
  display.display();
}

void mostrarTelaAluno(
  const String &idAluno,
  unsigned long ciclo,
  unsigned long sequencia,
  const String &status,
  const String &energia
) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  display.drawString(0, 0, "ALUNO " + idAluno);
  display.drawString(0, 12, "Ciclo: " + String(ciclo));
  display.drawString(0, 24, "Seq: " + String(sequencia));
  display.drawString(0, 36, "Status: " + status);
  display.drawString(0, 48, "BAT: " + energia);

  display.display();
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
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  display.drawString(0, 0, "ALUNO " + idAluno + " " + status);
  display.drawString(0, 12, "FC:" + valorOuNa(frequenciaCardiaca) + " bpm  SpO2:" + valorOuNa(spo2));
  display.drawString(0, 24, "Dedo:" + String(dedoDetectado ? "SIM" : "NAO") + " Q:" + qualidade);
  display.drawString(0, 36, "Ciclo:" + String(ciclo) + " Seq:" + String(sequencia));
  display.drawString(0, 48, "BAT:" + energia);

  display.display();
}

void mostrarTelaProfessor(
  unsigned long ciclo,
  const String &alvo,
  const String &status,
  int rssi,
  const String &energia
) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  display.drawString(0, 0, "PROFESSOR ICNP");
  display.drawString(0, 12, "Ciclo: " + String(ciclo));
  display.drawString(0, 24, "Alvo: " + alvo + " " + status);
  display.drawString(0, 36, "RSSI: " + String(rssi) + " dBm");
  display.drawString(0, 48, "BAT: " + energia);

  display.display();
}