#ifndef PROTOCOLO_ICNP_H
#define PROTOCOLO_ICNP_H

#include <Arduino.h>

// Assinatura e tipos oficiais do protocolo ICNP
#define ICNP_ASSINATURA "ICNP"
#define ICNP_TIPO_BEACON "BEACON"
#define ICNP_TIPO_DATA   "DATA"
#define ICNP_TIPO_ACK    "ACK"

// Monta BEACON enviado pelo Professor para abrir o ciclo
String montarBeaconIcnp(unsigned long ciclo);

// Monta DATA enviado pelo Aluno com telemetria
String montarDataIcnp(
  const String &idAluno,
  unsigned long sequencia,
  unsigned long ciclo,
  int frequenciaCardiaca,
  int spo2
);

// Monta ACK enviado pelo Professor confirmando DATA recebido
String montarAckIcnp(
  const String &idAluno,
  unsigned long sequencia,
  unsigned long ciclo
);

// Extrai um campo textual de uma mensagem ICNP
String extrairCampoIcnp(const String &mensagem, const String &campo);

// Verifica se a mensagem é do tipo esperado
bool pacoteEhDoTipoIcnp(const String &mensagem, const String &tipoEsperado);

// Verifica se a mensagem possui assinatura ICNP e campo TIPO
bool pacoteIcnpValido(const String &mensagem);

// Verifica se o ACK recebido corresponde ao DATA enviado
bool ackIcnpConfere(
  const String &mensagem,
  const String &idAluno,
  unsigned long sequencia,
  unsigned long ciclo
);

#endif