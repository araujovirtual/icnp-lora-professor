#ifndef PROTOCOLO_ICNP_H
#define PROTOCOLO_ICNP_H

#include <Arduino.h>

String montarBeaconIcnp(unsigned long ciclo);
String montarDataIcnp(
  const String &idAluno,
  unsigned long sequencia,
  const String &ciclo,
  int frequenciaCardiaca,
  int spo2
);
String montarAckIcnp(
  const String &idAluno,
  const String &sequencia,
  unsigned long ciclo
);

String extrairCampoIcnp(const String &mensagem, const String &campo);
bool pacoteEhDoTipoIcnp(const String &mensagem, const String &tipoEsperado);

#endif