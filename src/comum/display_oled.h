#ifndef DISPLAY_OLED_H
#define DISPLAY_OLED_H

#include <Arduino.h>

void iniciarDisplayOled();

void mostrarTelaProfessor(
  unsigned long ciclo,
  const String &alvo,
  const String &statusAck,
  int rssi,
  const String &energia
);

void mostrarTelaAluno(
  const String &idAluno,
  unsigned long ciclo,
  unsigned long sequencia,
  const String &statusAck,
  const String &energia
);

#endif