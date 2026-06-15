#pragma once

#include <Arduino.h>

void iniciarDisplayOled();

void mostrarTelaAluno(
  const String &idAluno,
  unsigned long ciclo,
  unsigned long sequencia,
  const String &status,
  const String &energia
);

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
);

void mostrarTelaProfessor(
  unsigned long ciclo,
  const String &alvo,
  const String &status,
  int rssi,
  const String &energia
);