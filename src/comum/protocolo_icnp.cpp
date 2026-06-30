#include "protocolo_icnp.h"

String montarBeaconIcnp(unsigned long ciclo, const String &alvo) {
  String mensagem = "";

  mensagem += ICNP_ASSINATURA;
  mensagem += ";TIPO=";
  mensagem += ICNP_TIPO_BEACON;
  mensagem += ";PROFESSOR=1";
  mensagem += ";CICLO=";
  mensagem += String(ciclo);
  mensagem += ";ALVO=";
  mensagem += alvo;

  return mensagem;
}

String montarDataIcnp(
  const String &idAluno,
  unsigned long sequencia,
  unsigned long ciclo,
  int frequenciaCardiaca,
  int spo2,
  const String &batAluno,
  long ir,
  long red,
  bool dedoDetectado,
  const String &qualidade,
  int pressaoSistolica,
  int pressaoDiastolica,
  const String &uso,
  const String &sinalPpg,
  bool paValida,
  const String &movimento,
  const String &artefatoPpg
) {
  String mensagem = "";

  mensagem += ICNP_ASSINATURA;
  mensagem += ";TIPO=";
  mensagem += ICNP_TIPO_DATA;
  mensagem += ";ALUNO=";
  mensagem += idAluno;
  mensagem += ";SEQ=";
  mensagem += String(sequencia);
  mensagem += ";CICLO=";
  mensagem += String(ciclo);
  mensagem += ";FC=";
  mensagem += String(frequenciaCardiaca);
  mensagem += ";SPO2=";
  mensagem += String(spo2);
  mensagem += ";BAT=";
  mensagem += batAluno;
  mensagem += ";IR=";
  mensagem += String(ir);
  mensagem += ";RED=";
  mensagem += String(red);
  mensagem += ";DEDO=";
  mensagem += dedoDetectado ? "1" : "0";
  mensagem += ";QUAL=";
  mensagem += qualidade;
  mensagem += ";SYS=";
  mensagem += String(pressaoSistolica);
  mensagem += ";DIA=";
  mensagem += String(pressaoDiastolica);
  mensagem += ";USO=";
  mensagem += uso;
  mensagem += ";SINAL_PPG=";
  mensagem += sinalPpg;
  mensagem += ";PA_VALIDA=";
  mensagem += paValida ? "1" : "0";
  mensagem += ";MOV=";
  mensagem += movimento;
  mensagem += ";ARTEFATO=";
  mensagem += artefatoPpg;

  return mensagem;
}

String montarAckIcnp(
  const String &idAluno,
  unsigned long sequencia,
  unsigned long ciclo
) {
  String mensagem = "";

  mensagem += ICNP_ASSINATURA;
  mensagem += ";TIPO=";
  mensagem += ICNP_TIPO_ACK;
  mensagem += ";PROFESSOR=1";
  mensagem += ";ALUNO=";
  mensagem += idAluno;
  mensagem += ";SEQ=";
  mensagem += String(sequencia);
  mensagem += ";CICLO=";
  mensagem += String(ciclo);

  return mensagem;
}

String extrairCampoIcnp(const String &mensagem, const String &campo) {
  String chave = campo + "=";

  int inicio = mensagem.indexOf(chave);

  if (inicio < 0) {
    return "";
  }

  inicio += chave.length();

  int fim = mensagem.indexOf(";", inicio);

  if (fim < 0) {
    fim = mensagem.length();
  }

  return mensagem.substring(inicio, fim);
}

bool pacoteIcnpValido(const String &mensagem) {
  return mensagem.startsWith(ICNP_ASSINATURA);
}

bool pacoteEhDoTipoIcnp(const String &mensagem, const String &tipo) {
  if (!pacoteIcnpValido(mensagem)) {
    return false;
  }

  String tipoRecebido = extrairCampoIcnp(mensagem, "TIPO");

  return tipoRecebido == tipo;
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
  String seqRecebida = extrairCampoIcnp(mensagem, "SEQ");
  String cicloRecebido = extrairCampoIcnp(mensagem, "CICLO");

  if (alunoRecebido != idAluno) {
    return false;
  }

  if (seqRecebida.toInt() != sequencia) {
    return false;
  }

  if (cicloRecebido.toInt() != ciclo) {
    return false;
  }

  return true;
}
