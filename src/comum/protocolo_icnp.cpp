#include "protocolo_icnp.h"

String montarBeaconIcnp(unsigned long ciclo) {
  return String(ICNP_ASSINATURA) +
         ";TIPO=" + String(ICNP_TIPO_BEACON) +
         ";PROFESSOR=1" +
         ";CICLO=" + String(ciclo);
}

String montarDataIcnp(
  const String &idAluno,
  unsigned long sequencia,
  unsigned long ciclo,
  int frequenciaCardiaca,
  int spo2
) {
  return String(ICNP_ASSINATURA) +
         ";TIPO=" + String(ICNP_TIPO_DATA) +
         ";ALUNO=" + idAluno +
         ";SEQ=" + String(sequencia) +
         ";CICLO=" + String(ciclo) +
         ";FC=" + String(frequenciaCardiaca) +
         ";SPO2=" + String(spo2);
}

String montarAckIcnp(
  const String &idAluno,
  unsigned long sequencia,
  unsigned long ciclo
) {
  return String(ICNP_ASSINATURA) +
         ";TIPO=" + String(ICNP_TIPO_ACK) +
         ";PROFESSOR=1" +
         ";ALUNO=" + idAluno +
         ";SEQ=" + String(sequencia) +
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
  if (!pacoteIcnpValido(mensagem)) {
    return false;
  }

  return extrairCampoIcnp(mensagem, "TIPO") == tipoEsperado;
}

bool pacoteIcnpValido(const String &mensagem) {
  if (!mensagem.startsWith(ICNP_ASSINATURA)) {
    return false;
  }

  if (extrairCampoIcnp(mensagem, "TIPO") == "") {
    return false;
  }

  return true;
}

bool ackIcnpConfere(
  const String &mensagem,
  const String &idAluno,
  unsigned long sequencia,
  unsigned long ciclo
) {
  if (!pacoteEhDoTipoIcnp(mensagem, ICNP_TIPO_ACK)) {
    return false;
  }

  String alunoRecebido = extrairCampoIcnp(mensagem, "ALUNO");
  String sequenciaRecebida = extrairCampoIcnp(mensagem, "SEQ");
  String cicloRecebido = extrairCampoIcnp(mensagem, "CICLO");

  if (alunoRecebido != idAluno) {
    return false;
  }

  if (sequenciaRecebida.toInt() != (long)sequencia) {
    return false;
  }

  if (cicloRecebido.toInt() != (long)ciclo) {
    return false;
  }

  return true;
}