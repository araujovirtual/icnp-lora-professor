#include "protocolo_icnp.h"

String montarBeaconIcnp(unsigned long ciclo) {
  return "ICNP;TIPO=BEACON;PROFESSOR=1;CICLO=" + String(ciclo);
}

String montarDataIcnp(
  const String &idAluno,
  unsigned long sequencia,
  const String &ciclo,
  int frequenciaCardiaca,
  int spo2
) {
  return "ICNP;TIPO=DATA;ALUNO=" + idAluno +
         ";SEQ=" + String(sequencia) +
         ";CICLO=" + ciclo +
         ";FC=" + String(frequenciaCardiaca) +
         ";SPO2=" + String(spo2);
}

String montarAckIcnp(
  const String &idAluno,
  const String &sequencia,
  unsigned long ciclo
) {
  return "ICNP;TIPO=ACK;PROFESSOR=1;ALUNO=" + idAluno +
         ";SEQ=" + sequencia +
         ";CICLO=" + String(ciclo);
}

String extrairCampoIcnp(const String &mensagem, const String &campo) {
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

bool pacoteEhDoTipoIcnp(const String &mensagem, const String &tipoEsperado) {
  return extrairCampoIcnp(mensagem, "TIPO") == tipoEsperado;
}