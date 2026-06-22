#ifndef API_PROFESSOR_H
#define API_PROFESSOR_H

#include <Arduino.h>

struct EstadoAlunoAPI {
  bool ativo = false;

  int aluno = 0;
  int seq = -1;
  int ciclo = -1;

  int fc = -1;
  int spo2 = -1;

  long ir = -1;
  long red = -1;

  String ppg = "";
  int ppgN = 0;
  unsigned long ppgMs = 0;

  int dedo = -1;
  String qual = "NA";

  int rssi = 0;
  float snr = 0.0f;

  float batAluno = -1.0f;
  float energiaProfessor = -1.0f;

  int ack = 0;

  unsigned long ultimoMs = 0;
};

void iniciarApiProfessor();

void atualizarEstadoAlunoAPI(
  int aluno,
  int seq,
  int ciclo,
  int fc,
  int spo2,
  long ir,
  long red,
  int dedo,
  const String& qual,
  int rssi,
  float snr,
  float batAluno,
  float energiaProfessor,
  int ack
);

void atualizarPpgAlunoAPI(
  int aluno,
  const String& ppg,
  int ppgN
);

#endif
