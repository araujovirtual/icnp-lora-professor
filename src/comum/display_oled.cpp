#include "display_oled.h"

#include <Wire.h>
#include "SSD1306Wire.h"

// Heltec WiFi LoRa 32 V2
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16
#define OLED_ADDR 0x3C

SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL);

void resetarDisplayOled() {
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);
  delay(50);
}

void iniciarDisplayOled() {
  resetarDisplayOled();

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();

  display.drawString(0, 0, "ICNP LoRa");
  display.drawString(0, 16, "Inicializando...");
  display.display();
}

void mostrarTelaProfessor(
  unsigned long ciclo,
  const String &alvo,
  const String &statusAck,
  int rssi,
  const String &energia
) {
  display.clear();
  display.setFont(ArialMT_Plain_10);

  display.drawString(0, 0, "ICNP LoRa - PROF");
  display.drawString(0, 12, "Ciclo: " + String(ciclo) + " Alvo:" + alvo);
  display.drawString(0, 24, "ACK: " + statusAck);

  if (rssi == 0) {
    display.drawString(0, 36, "RSSI: --");
  } else {
    display.drawString(0, 36, "RSSI: " + String(rssi) + " dBm");
  }

  display.drawString(0, 48, "Energia: " + energia);

  display.display();
}

void mostrarTelaAluno(
  const String &idAluno,
  unsigned long ciclo,
  unsigned long sequencia,
  const String &statusAck,
  const String &energia
) {
  display.clear();
  display.setFont(ArialMT_Plain_10);

  display.drawString(0, 0, "ICNP LoRa - ALUNO " + idAluno);
  display.drawString(0, 12, "Ciclo: " + String(ciclo));
  display.drawString(0, 24, "SEQ: " + String(sequencia));
  display.drawString(0, 36, "ACK: " + statusAck);
  display.drawString(0, 48, "Energia: " + energia);

  display.display();
}