#ifndef PROTOCOLO_ICNP_H
#define PROTOCOLO_ICNP_H

#include <Arduino.h>

#define ICNP_ASSINATURA "ICNP"
#define ICNP_TIPO_BEACON "BEACON"
#define ICNP_TIPO_DATA "DATA"
#define ICNP_TIPO_ACK "ACK"

String montarBeaconIcnp(unsigned long ciclo, const String &alvo);

String montarDataIcnp(
  const String &idAluno,
  unsigned long sequencia,
  unsigned long ciclo,
  int frequenciaCardiaca,
  int spo2,
  const String &batAluno,
  long ir,
  long red,
  bool dedoDetectado
);

String montarAckIcnp(
  const String &idAluno,
  unsigned long sequencia,
  unsigned long ciclo
);

String extrairCampoIcnp(const String &mensagem, const String &campo);

bool pacoteEhDoTipoIcnp(const String &mensagem, const String &tipo);

bool pacoteIcnpValido(const String &mensagem);

bool ackIcnpConfere(
  const String &mensagem,
  const String &idAluno,
  unsigned long sequencia,
  unsigned long ciclo
);

#endif