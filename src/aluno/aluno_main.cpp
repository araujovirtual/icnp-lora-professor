#include <Arduino.h>
#include <math.h>
#include <esp_system.h>

#include "radio_lora.h"
#include "protocolo_icnp.h"
#include "display_oled.h"
#include "bateria.h"
#include "led_sync.h"
#include "sensor_fisiologico.h"

#ifndef ID_ALUNO_CONFIG
#define ID_ALUNO_CONFIG "2"
#endif

// 0 = apenas mostra a janela PPG no Serial.
// 1 = envia pacote ICNP TIPO=PPG apos ACK valido.
#define ENVIAR_PPG_DEBUG_LORA 1

#define AMOSTRAS_PPG_API 24
#define INTERVALO_MINIMO_PPG_MS 1000

const String ID_ALUNO = ID_ALUNO_CONFIG;

// ACK curto para evitar que o Aluno fique escutando pacotes de outro ciclo/aluno.
const unsigned long tempoEsperaAckMs = 650;
const unsigned long intervaloStatusSensorMs = 2000;
const unsigned long intervaloTelaTempoRealMs = 250;
// O LoRa envia snapshot quando o Professor chama por BEACON.
// Para envio cravado a cada 5 s, ajuste o intervalo do Professor.
const unsigned long intervaloMinimoTelemetriaMs = 120;

unsigned long contadorSeq = 0;
unsigned long ultimoEnvioData = 0;
unsigned long ultimoStatusSensor = 0;
unsigned long ultimoEnvioPpg = 0;
unsigned long ultimaAtualizacaoTela = 0;

unsigned long ultimoCicloTela = 0;
unsigned long ultimaSeqTela = 0;
String ultimoStatusTela = "AGUARDANDO";
static volatile bool telaCalibracaoManualAtiva = false;

String montarLinhaEnergia(float tensao) {
  String status = statusBateria(tensao);

  if (isnan(tensao)) {
    return "--";
  }

  String tensaoTexto = String(tensao, 2) + " V";

  if (status == "OK") {
    return tensaoTexto;
  }

  return status + " " + tensaoTexto;
}

String montarBatParaPacote(float tensao) {
  if (isnan(tensao)) {
    return "NA";
  }

  return String(tensao, 2);
}

String movimentoParaApi(const String &movimento) {
  // V16.10: a API deve mostrar a acao compreensivel para o usuario.
  // Internamente o sensor ainda usa IMPACTO_VERTICAL para bloquear PPG,
  // mas no painel isso representa a etapa operacional de pular/impacto.
  if (movimento == "IMPACTO_VERTICAL") {
    return "PULANDO_IMPACTO";
  }
  return movimento;
}

String textoQualidadeSensor(
  bool sensorOk,
  bool dedoDetectado,
  bool paValida,
  int fc,
  int spo2,
  const String &sinalPpg
) {
  if (!sensorOk) {
    return "NA";
  }

  if (!dedoDetectado) {
    return "NA";
  }

  if (!paValida) {
    return "RUIM";
  }

  if (fc <= 0 || spo2 <= 0) {
    return "RUIM";
  }

  // V16.11: quando a fisiologia vem do hold seguro em repouso, o sinal pode
  // aparecer como SINAL_INSTAVEL por poucos segundos, mas PA_VALIDA=1 garante
  // que os valores ainda sao a ultima janela estavel publicada.
  return "OK";
}

void atualizarTelaAlunoAtual(const String &status) {
  // Durante o assistente V15, somente a tarefa de calibracao controla o OLED.
  // Isso evita a tela piscar/alternar entre "CALIBRANDO" e a etapa atual.
  if (telaCalibracaoManualAtiva) {
    return;
  }

  if (sensorFisiologicoCalibrandoMpu()) {
    mostrarTelaAlunoCalibracao(
      "CAL PARADO",
      "NAO MEXA",
      progressoCalibracaoMpuSensorFisiologico(),
      segundosRestantesCalibracaoMpuSensorFisiologico()
    );
    return;
  }

  bool sensorOk = sensorFisiologicoDisponivel();
  bool dedo = dedoDetectadoSensorFisiologico();
  bool paValida = paValidaSensorFisiologico();

  int fc = lerFrequenciaCardiacaExperimental();
  int spo2 = lerSpo2Experimental();
  int sys = lerPressaoSistolicaExperimental();
  int dia = lerPressaoDiastolicaExperimental();

  String uso = lerUsoSensorFisiologico();
  String sinalPpg = lerSinalPpgSensorFisiologico();

  float sinalOnda = lerSinalOndaPpgExperimental();

  float tensaoAluno = lerTensaoBateria();
  String energiaAluno = montarLinhaEnergia(tensaoAluno);

  String qualidade = textoQualidadeSensor(
    sensorOk,
    dedo,
    paValida,
    fc,
    spo2,
    sinalPpg
  );

  ultimoStatusTela = status;

  mostrarTelaAlunoBiometrico(
    ID_ALUNO,
    ultimoCicloTela,
    ultimaSeqTela,
    ultimoStatusTela,
    energiaAluno,
    fc,
    spo2,
    sys,
    dia,
    dedo,
    qualidade,
    uso,
    sinalPpg,
    sinalOnda
  );
}

void imprimirStatusSensor() {
  bool sensorOk = sensorFisiologicoDisponivel();
  bool dedo = dedoDetectadoSensorFisiologico();
  bool paValida = paValidaSensorFisiologico();

  int fc = lerFrequenciaCardiacaExperimental();
  int spo2 = lerSpo2Experimental();
  int sys = lerPressaoSistolicaExperimental();
  int dia = lerPressaoDiastolicaExperimental();

  long ir = lerIrSensorFisiologico();
  long red = lerRedSensorFisiologico();

  float acIr = lerAcIrSensorFisiologico();
  float acRed = lerAcRedSensorFisiologico();
  float ratio = lerRatioSpo2Experimental();

  String uso = lerUsoSensorFisiologico();
  String sinalPpg = lerSinalPpgSensorFisiologico();
  String movimento = lerMovimentoSensorFisiologico();
  String movimentoApi = movimentoParaApi(movimento);
  String artefato = lerArtefatoPpgSensorFisiologico();
  String qualCorr = lerQualidadeCorrigidaSensorFisiologico();
  String debugMpu = lerDebugMpuSensorFisiologico();

  String qualidade = textoQualidadeSensor(
    sensorOk,
    dedo,
    paValida,
    fc,
    spo2,
    sinalPpg
  );

  String ppg = montarJanelaPpgNormalizadaApi(AMOSTRAS_PPG_API);

  Serial.print("STATUS_SENSOR;");
  Serial.print("ALUNO=");
  Serial.print(ID_ALUNO);
  Serial.print(";SENSOR=");
  Serial.print(sensorOk ? "OK" : "NA");
  Serial.print(";USO=");
  Serial.print(uso);
  Serial.print(";SINAL_PPG=");
  Serial.print(sinalPpg);
  Serial.print(";PA_VALIDA=");
  Serial.print(paValida ? "1" : "0");
  Serial.print(";MOV=");
  Serial.print(movimento);
  Serial.print(";ARTEFATO=");
  Serial.print(artefato);
  Serial.print(";QUAL_CORR=");
  Serial.print(qualCorr);
  Serial.print(";DEDO=");
  Serial.print(dedo ? "1" : "0");
  Serial.print(";QUAL=");
  Serial.print(qualidade);
  Serial.print(";FC=");
  Serial.print(fc > 0 ? String(fc) : "NA");
  Serial.print(";SPO2=");
  Serial.print(spo2 > 0 ? String(spo2) : "NA");
  Serial.print(";PA_EXP=");
  if (sys > 0 && dia > 0) {
    Serial.print(sys);
    Serial.print("x");
    Serial.print(dia);
  } else {
    Serial.print("NA");
  }
  Serial.print(";IR=");
  Serial.print(ir);
  Serial.print(";RED=");
  Serial.print(red);
  Serial.print(";AC_IR=");
  Serial.print(acIr, 1);
  Serial.print(";AC_RED=");
  Serial.print(acRed, 1);
  Serial.print(";RATIO=");
  Serial.print(ratio, 3);
  Serial.print(";PPG=");
  Serial.print(ppg.length() > 0 ? ppg : "NA");
  Serial.print(";MPU_DBG=");
  Serial.print(debugMpu);
  Serial.println();
}

String montarPacotePpgDebug(unsigned long seq, unsigned long ciclo) {
  String ppg = montarJanelaPpgNormalizadaApi(AMOSTRAS_PPG_API);

  if (ppg.length() == 0) {
    return "";
  }

  String pacote = "ICNP;TIPO=PPG;ALUNO=" + ID_ALUNO +
                  ";SEQ=" + String(seq) +
                  ";CICLO=" + String(ciclo) +
                  ";N=" + String(AMOSTRAS_PPG_API) +
                  ";PPG=" + ppg;

  return pacote;
}

void tentarEnviarPpgDebug(unsigned long seq, unsigned long ciclo, bool dedo, const String &qualidade, bool paValida) {
#if ENVIAR_PPG_DEBUG_LORA
  if (!dedo) {
    return;
  }

  if (qualidade != "OK" || !paValida) {
    return;
  }

  unsigned long agora = millis();

  if (agora - ultimoEnvioPpg < INTERVALO_MINIMO_PPG_MS) {
    return;
  }

  ultimoEnvioPpg = agora;

  String pacotePpg = montarPacotePpgDebug(seq, ciclo);

  if (pacotePpg.length() == 0) {
    Serial.println("PPG debug nao enviado: janela PPG insuficiente.");
    return;
  }

  Serial.print("Enviando PPG debug: ");
  Serial.println(pacotePpg);

  delay(25);
  enviarMensagemLoRa(pacotePpg);
  pulsoLedSync(30);
#else
  (void)seq;
  (void)ciclo;
  (void)dedo;
  (void)qualidade;
  (void)paValida;
#endif
}



struct ContextoRecalibracaoMpu {
  const char *motivo;
  volatile bool concluido;
  volatile bool ok;
};

static ContextoRecalibracaoMpu contextoRecalibracao;

static void taskExecutarRecalibracaoMpu(void *parametro) {
  ContextoRecalibracaoMpu *ctx = (ContextoRecalibracaoMpu *)parametro;
  ctx->ok = recalibrarMpuSensorFisiologico(ctx->motivo);
  ctx->concluido = true;
  vTaskDelete(NULL);
}

static bool executarRecalibracaoComTela(const char *motivo, const String &titulo, const String &mensagem) {
  contextoRecalibracao.motivo = motivo;
  contextoRecalibracao.concluido = false;
  contextoRecalibracao.ok = false;

  BaseType_t criada = xTaskCreatePinnedToCore(
    taskExecutarRecalibracaoMpu,
    "TaskRecalMpu",
    8192,
    &contextoRecalibracao,
    4,
    NULL,
    1
  );

  if (criada != pdPASS) {
    Serial.println("EVENTO;CALIBRACAO_MPU;ERRO;NAO_CRIOU_TASK_RECALIBRACAO");
    return false;
  }

  while (!contextoRecalibracao.concluido) {
    mostrarTelaAlunoCalibracao(
      titulo,
      mensagem,
      progressoCalibracaoMpuSensorFisiologico(),
      segundosRestantesCalibracaoMpuSensorFisiologico()
    );
    vTaskDelay(pdMS_TO_TICKS(160));
  }

  mostrarTelaAlunoCalibracao(
    contextoRecalibracao.ok ? "CAL OK" : "FALHA",
    contextoRecalibracao.ok ? "NVS SALVA" : "TENTE NOVO",
    contextoRecalibracao.ok ? 100 : 0,
    0
  );
  vTaskDelay(pdMS_TO_TICKS(900));
  return contextoRecalibracao.ok;
}

static void registrarSnapshotAssistenteV15(const char *etapa, int segundo) {
  Serial.print("CAL_V15;");
  Serial.print("ETAPA="); Serial.print(etapa);
  Serial.print(";T="); Serial.print(segundo);
  Serial.print(";USO="); Serial.print(lerUsoSensorFisiologico());
  Serial.print(";SINAL_PPG="); Serial.print(lerSinalPpgSensorFisiologico());
  Serial.print(";PA_VALIDA="); Serial.print(paValidaSensorFisiologico() ? "1" : "0");
  Serial.print(";MOV="); Serial.print(lerMovimentoSensorFisiologico());
  Serial.print(";ARTEFATO="); Serial.print(lerArtefatoPpgSensorFisiologico());
  Serial.print(";QUAL_CORR="); Serial.print(lerQualidadeCorrigidaSensorFisiologico());
  Serial.print(";FC="); Serial.print(lerFrequenciaCardiacaExperimental());
  Serial.print(";SPO2="); Serial.print(lerSpo2Experimental());
  Serial.print(";SYS="); Serial.print(lerPressaoSistolicaExperimental());
  Serial.print(";DIA="); Serial.print(lerPressaoDiastolicaExperimental());
  Serial.print(";IR="); Serial.print(lerIrSensorFisiologico());
  Serial.print(";RED="); Serial.print(lerRedSensorFisiologico());
  Serial.print(";AC_IR="); Serial.print(lerAcIrSensorFisiologico(), 1);
  Serial.print(";AC_RED="); Serial.print(lerAcRedSensorFisiologico(), 1);
  Serial.print(";RATIO="); Serial.print(lerRatioSpo2Experimental(), 3);
  Serial.print(";MPU_DBG="); Serial.println(lerDebugMpuSensorFisiologico());
}

static bool contatoPpgAssistenteV15Ok() {
  bool dedo = dedoDetectadoSensorFisiologico();
  String sinal = lerSinalPpgSensorFisiologico();
  long ir = lerIrSensorFisiologico();
  long red = lerRedSensorFisiologico();

  bool irOk = ir >= 50000L && ir <= 255000L;
  bool redOk = red >= 12000L && red <= 255000L;
  bool sinalContato = sinal != "SEM_CONTATO";

  return dedo && irOk && redOk && sinalContato;
}

static bool ppgAssistenteV15Ok() {
  bool dedo = dedoDetectadoSensorFisiologico();
  String sinal = lerSinalPpgSensorFisiologico();
  String movimento = lerMovimentoSensorFisiologico();
  String qualCorr = lerQualidadeCorrigidaSensorFisiologico();
  long ir = lerIrSensorFisiologico();
  long red = lerRedSensorFisiologico();
  float acIr = lerAcIrSensorFisiologico();
  float acRed = lerAcRedSensorFisiologico();
  float ratio = lerRatioSpo2Experimental();

  bool irOk = ir >= 30000L && ir <= 260000L;
  bool redOk = red >= 10000L && red <= 260000L;
  bool acOk = acIr >= 30.0f && acRed >= 10.0f;
  bool ratioOk = ratio >= 0.35f && ratio <= 1.35f;
  bool sinalOk = sinal == "PPG_ATIVO";
  bool movOk = movimento != "CORRIDA_INTENSA" && movimento != "IMPACTO_VERTICAL";
  bool qualOk = qualCorr != "SEM_CONTATO" && qualCorr != "RUIM_PPG";

  return dedo && irOk && redOk && acOk && ratioOk && sinalOk && movOk && qualOk;
}

static void telaDiagnosticoPpgAssistenteV15(int pct, int faltam) {
  bool dedo = dedoDetectadoSensorFisiologico();
  String sinal = lerSinalPpgSensorFisiologico();
  String movimento = lerMovimentoSensorFisiologico();
  long ir = lerIrSensorFisiologico();
  float acIr = lerAcIrSensorFisiologico();
  float acRed = lerAcRedSensorFisiologico();
  float ratio = lerRatioSpo2Experimental();

  if (!dedo || ir < 30000L) {
    mostrarTelaAlunoCalibracao("COLOQUE", "LUVA/SENSOR", pct, faltam);
    return;
  }

  if (movimento == "CORRIDA_INTENSA" || movimento == "IMPACTO_VERTICAL") {
    mostrarTelaAlunoCalibracao("NAO MEXA", "PPG AJUSTE", pct, faltam);
    return;
  }

  if (sinal != "PPG_ATIVO" || acIr < 30.0f || acRed < 10.0f || ratio < 0.35f || ratio > 1.35f) {
    mostrarTelaAlunoCalibracao("AJUSTE", "PPG BAIXO", pct, faltam);
    return;
  }

  mostrarTelaAlunoCalibracao("PPG OK", "SEGURE PARADO", pct, faltam);
}

static bool prepararEtapaAssistidaV15(const char *etapa, const String &titulo, const String &mensagem, int segundosPreparo = 5) {
  Serial.print("EVENTO;ASSISTENTE_V15;ETAPA;");
  Serial.print(etapa);
  Serial.print(";PREPARO;");
  Serial.print(segundosPreparo);
  Serial.println("s");

  unsigned long inicio = millis();
  unsigned long duracaoMs = (unsigned long)segundosPreparo * 1000UL;
  int ultimoSegundoLog = -1;

  while ((millis() - inicio) < duracaoMs) {
    unsigned long decorrido = millis() - inicio;
    int segundo = (int)(decorrido / 1000UL);
    int pct = (int)((decorrido * 100UL) / duracaoMs);
    if (pct > 100) pct = 100;
    int faltam = (int)((duracaoMs - decorrido + 999UL) / 1000UL);

    mostrarTelaAlunoCalibracao("PREPARE", titulo, pct, faltam);

    if (segundo != ultimoSegundoLog) {
      ultimoSegundoLog = segundo;
      Serial.print("CAL_V15;ETAPA=");
      Serial.print(etapa);
      Serial.print(";FASE=PREPARO;T=");
      Serial.print(segundo);
      Serial.print(";MSG=");
      Serial.println(mensagem);
    }
    vTaskDelay(pdMS_TO_TICKS(120));
  }

  mostrarTelaAlunoCalibracao("AGORA", titulo, 100, 0);
  vTaskDelay(pdMS_TO_TICKS(850));
  return true;
}

static bool prevalidarPpgAssistenteV15() {
  Serial.println("EVENTO;ASSISTENTE_V15;ETAPA;AJUSTE_SENSOR_PPG;INICIO");
  Serial.println("EVENTO;ASSISTENTE_V15;ETAPA;AJUSTE_SENSOR_PPG;REGRA;SO_COMECA_VALIDACAO_APOS_CONTATO_REAL");

  // Fase 1: nao existe contagem de PPG enquanto nao houver contato real.
  // Assim o OLED nao mostra "dedo OK" sem a luva/sensor estar no corpo.
  unsigned long inicioEsperaContato = millis();
  unsigned long contatoDesde = 0;
  int ultimoLogContato = -1;
  const unsigned long timeoutContatoMs = 90000UL;
  const unsigned long contatoEstavelMs = 1800UL;

  while ((millis() - inicioEsperaContato) < timeoutContatoMs) {
    bool contato = contatoPpgAssistenteV15Ok();

    if (contato) {
      if (contatoDesde == 0) contatoDesde = millis();
      unsigned long contatoMs = millis() - contatoDesde;
      int pctContato = (int)((contatoMs * 100UL) / contatoEstavelMs);
      if (pctContato > 100) pctContato = 100;
      int faltamContato = (int)((contatoEstavelMs - (contatoMs > contatoEstavelMs ? contatoEstavelMs : contatoMs) + 999UL) / 1000UL);
      mostrarTelaAlunoCalibracao("DEDO", "DETECTADO", pctContato, faltamContato);
      if (contatoMs >= contatoEstavelMs) {
        Serial.println("EVENTO;ASSISTENTE_V15;ETAPA;AJUSTE_SENSOR_PPG;CONTATO_REAL_CONFIRMADO");
        mostrarTelaAlunoCalibracao("SENSOR", "CONTATO OK", 100, 0);
        vTaskDelay(pdMS_TO_TICKS(900));
        break;
      }
    } else {
      contatoDesde = 0;
      mostrarTelaAlunoCalibracao("COLOQUE", "SENSOR PPG", 0, 0);
    }

    int segundo = (int)((millis() - inicioEsperaContato) / 1000UL);
    if (segundo != ultimoLogContato) {
      ultimoLogContato = segundo;
      Serial.print("CAL_V15;ETAPA=AJUSTE_SENSOR_PPG;FASE=AGUARDANDO_CONTATO;T=");
      Serial.print(segundo);
      Serial.print(";CONTATO=");
      Serial.print(contato ? 1 : 0);
      Serial.print(";DEDO=");
      Serial.print(dedoDetectadoSensorFisiologico() ? 1 : 0);
      Serial.print(";SINAL_PPG=");
      Serial.print(lerSinalPpgSensorFisiologico());
      Serial.print(";IR=");
      Serial.print(lerIrSensorFisiologico());
      Serial.print(";RED=");
      Serial.print(lerRedSensorFisiologico());
      Serial.print(";AC_IR=");
      Serial.print(lerAcIrSensorFisiologico(), 1);
      Serial.print(";AC_RED=");
      Serial.print(lerAcRedSensorFisiologico(), 1);
      Serial.print(";RATIO=");
      Serial.println(lerRatioSpo2Experimental(), 3);
    }

    vTaskDelay(pdMS_TO_TICKS(180));
  }

  if (!contatoPpgAssistenteV15Ok()) {
    Serial.println("EVENTO;ASSISTENTE_V15;ETAPA;AJUSTE_SENSOR_PPG;FALHA;CONTATO_REAL_NAO_CONFIRMADO");
    mostrarTelaAlunoCalibracao("FALHA", "SEM SENSOR", 0, 0);
    vTaskDelay(pdMS_TO_TICKS(2200));
    return false;
  }

  // Fase 2: somente agora comeca a validacao de PPG real.
  Serial.println("EVENTO;ASSISTENTE_V15;ETAPA;AJUSTE_SENSOR_PPG;VALIDACAO_PPG_INICIO");
  unsigned long inicioValidacao = millis();
  unsigned long ppgOkDesde = 0;
  int ultimoLogValidacao = -1;
  const unsigned long timeoutValidacaoMs = 60000UL;
  const unsigned long ppgOkNecessarioMs = 5000UL;

  while ((millis() - inicioValidacao) < timeoutValidacaoMs) {
    bool contato = contatoPpgAssistenteV15Ok();
    bool ok = contato && ppgAssistenteV15Ok();

    if (!contato) {
      ppgOkDesde = 0;
      mostrarTelaAlunoCalibracao("COLOQUE", "SENSOR PPG", 0, 0);
    } else if (ok) {
      if (ppgOkDesde == 0) ppgOkDesde = millis();
      unsigned long okMs = millis() - ppgOkDesde;
      int pctOk = (int)((okMs * 100UL) / ppgOkNecessarioMs);
      if (pctOk > 100) pctOk = 100;
      int faltaOk = (int)((ppgOkNecessarioMs - (okMs > ppgOkNecessarioMs ? ppgOkNecessarioMs : okMs) + 999UL) / 1000UL);
      mostrarTelaAlunoCalibracao("VALIDANDO", "PPG PARADO", pctOk, faltaOk);
      if (okMs >= ppgOkNecessarioMs) {
        Serial.println("EVENTO;ASSISTENTE_V15;ETAPA;AJUSTE_SENSOR_PPG;OK_PPG_REAL");
        mostrarTelaAlunoCalibracao("SENSOR", "PPG OK", 100, 0);
        vTaskDelay(pdMS_TO_TICKS(1100));
        return true;
      }
    } else {
      ppgOkDesde = 0;
      telaDiagnosticoPpgAssistenteV15(0, 0);
    }

    int segundo = (int)((millis() - inicioValidacao) / 1000UL);
    if (segundo != ultimoLogValidacao) {
      ultimoLogValidacao = segundo;
      Serial.print("CAL_V15;ETAPA=AJUSTE_SENSOR_PPG;FASE=VALIDANDO_PPG;T=");
      Serial.print(segundo);
      Serial.print(";PPG_OK=");
      Serial.print(ok ? 1 : 0);
      Serial.print(";CONTATO=");
      Serial.print(contato ? 1 : 0);
      Serial.print(";DEDO=");
      Serial.print(dedoDetectadoSensorFisiologico() ? 1 : 0);
      Serial.print(";SINAL_PPG=");
      Serial.print(lerSinalPpgSensorFisiologico());
      Serial.print(";MOV=");
      Serial.print(lerMovimentoSensorFisiologico());
      Serial.print(";AC_IR=");
      Serial.print(lerAcIrSensorFisiologico(), 1);
      Serial.print(";AC_RED=");
      Serial.print(lerAcRedSensorFisiologico(), 1);
      Serial.print(";RATIO=");
      Serial.println(lerRatioSpo2Experimental(), 3);
    }

    vTaskDelay(pdMS_TO_TICKS(180));
  }

  Serial.println("EVENTO;ASSISTENTE_V15;ETAPA;AJUSTE_SENSOR_PPG;FALHA;PPG_REAL_NAO_CONFIRMADO");
  mostrarTelaAlunoCalibracao("FALHA", "AJUSTE PPG", 0, 0);
  vTaskDelay(pdMS_TO_TICKS(2200));
  return false;
}

static void executarEtapaAssistidaV15(const char *etapa, const String &titulo, const String &mensagem, int segundos) {
  prepararEtapaAssistidaV15(etapa, titulo, mensagem, 5);

  Serial.print("EVENTO;ASSISTENTE_V15;ETAPA;");
  Serial.print(etapa);
  Serial.println(";COLETA_INICIO");

  unsigned long inicio = millis();
  unsigned long duracaoMs = (unsigned long)segundos * 1000UL;
  int ultimoSegundoLog = -1;

  while ((millis() - inicio) < duracaoMs) {
    unsigned long decorrido = millis() - inicio;
    int segundo = (int)(decorrido / 1000UL);
    int pct = (int)((decorrido * 100UL) / duracaoMs);
    if (pct > 100) pct = 100;
    int faltam = (int)((duracaoMs - decorrido + 999UL) / 1000UL);

    mostrarTelaAlunoCalibracao(titulo, mensagem, pct, faltam);

    if (segundo != ultimoSegundoLog) {
      ultimoSegundoLog = segundo;
      registrarSnapshotAssistenteV15(etapa, segundo);
    }
    vTaskDelay(pdMS_TO_TICKS(140));
  }

  registrarSnapshotAssistenteV15(etapa, segundos);
  Serial.print("EVENTO;ASSISTENTE_V15;ETAPA;");
  Serial.print(etapa);
  Serial.println(";COLETA_FIM");
  mostrarTelaAlunoCalibracao("ETAPA OK", titulo, 100, 0);
  vTaskDelay(pdMS_TO_TICKS(900));
}

static bool executarAssistenteCompletoV15() {
  telaCalibracaoManualAtiva = true;
  ultimoStatusTela = "ASSIST V15";

  Serial.println("EVENTO;ASSISTENTE_V15;INICIO;PROTOCOLO_COMPLETO_UNICO_BOOT_5S_SENSOR_SO_COM_DEDO_REAL");
  mostrarTelaAlunoCalibracao("MODO", "V15 FULL", 0, 0);
  vTaskDelay(pdMS_TO_TICKS(1000));

  if (!prevalidarPpgAssistenteV15()) {
    Serial.println("EVENTO;ASSISTENTE_V15;FALHA;PPG_NAO_VALIDADO");
    return false;
  }

  // V16.11: antes aparecia como uma segunda etapa "PARADO", confundindo o uso.
  // Agora a base do MPU e salva com nome/tela propria, e a etapa PARADO_BASE
  // aparece uma unica vez no assistente.
  prepararEtapaAssistidaV15("ZERAR_MPU_BASE", "ZERAR MPU", "SALVAR POSICAO BASE", 5);
  bool okCal = executarRecalibracaoComTela("ASSISTENTE_V15_ZERAR_MPU", "ZERANDO MPU", "NAO MEXA");
  if (!okCal) {
    Serial.println("EVENTO;ASSISTENTE_V15;FALHA;ZERAR_MPU_BASE");
    return false;
  }

  executarEtapaAssistidaV15("PARADO_BASE", "PARADO", "NAO MEXA", 8);
  executarEtapaAssistidaV15("ANDANDO_MOV_LEVE", "ANDAR", "PASSOS LEVES", 10);
  executarEtapaAssistidaV15("DIGITANDO_MICRO", "MOVIMENTE", "DEDOS/MAO", 10);
  executarEtapaAssistidaV15("CORRENDO_LOCAL", "CORRER", "NO LUGAR", 10);
  executarEtapaAssistidaV15("PULANDO_IMPACTO", "PULAR", "LEVE", 10);
  executarEtapaAssistidaV15("PARADO_2_RECUPERACAO", "SENTAR", "MAOS JOELHO", 12);

  bool salvouPerfil = salvarPerfilAssistidoV15SensorFisiologico("PPG_OK;ZERAR_MPU;PARADO;ANDANDO;DIGITANDO;CORRENDO;PULANDO;SENTADO;BOOT_5S_UNICO;PREPARO_5S;PPG_SO_COM_DEDO_REAL;TELA_SEM_ALTERNANCIA;V16_14");
  if (!salvouPerfil) {
    Serial.println("EVENTO;ASSISTENTE_V15;FALHA;PERFIL_NAO_SALVO");
    return false;
  }

  Serial.println("EVENTO;ASSISTENTE_V15;FIM;CALIBRACAO_EFETUADA_COM_SUCESSO");
  return true;
}

static void taskCalibracaoBootAluno(void *parametro) {
  (void)parametro;

  unsigned long inicioPressionado = 0;
  unsigned long ultimaTelaBoot = 0;
  bool aviso5sImpresso = false;
  bool aguardandoSoltarParaIniciar = false;

  while (true) {
    if (!sensorFisiologicoDisponivel()) {
      inicioPressionado = 0;
      aviso5sImpresso = false;
      aguardandoSoltarParaIniciar = false;
      telaCalibracaoManualAtiva = false;
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    bool pressionado = botaoCalibracaoMpuPressionadoSensorFisiologico();

    if (!pressionado) {
      if (aguardandoSoltarParaIniciar) {
        Serial.println("EVENTO;ASSISTENTE_V15;BOOT_5S_SOLTO;INICIANDO_PROTOCOLO_COMPLETO");
        telaCalibracaoManualAtiva = true;
        bool ok = executarAssistenteCompletoV15();

        if (ok) {
          Serial.println("EVENTO;ASSISTENTE_V15;CALIBRACAO_EFETUADA_COM_SUCESSO;REINICIANDO_RADIO");
          mostrarTelaAlunoCalibracao("CALIBRACAO", "EFETUADA", 100, 0);
          delay(1400);
          mostrarTelaAlunoCalibracao("COM SUCESSO", "REINICIANDO", 100, 0);
          delay(1400);
          mostrarTelaAlunoCalibracao("REINICIA", "RADIO", 100, 0);
          delay(1000);
          ESP.restart();
        } else {
          Serial.println("EVENTO;ASSISTENTE_V15;FALHA;TENTE_NOVAMENTE");
          mostrarTelaAlunoCalibracao("FALHA", "TENTE NOVO", 0, 0);
          delay(2500);
        }
      }

      inicioPressionado = 0;
      aviso5sImpresso = false;
      aguardandoSoltarParaIniciar = false;
      if (!sensorFisiologicoCalibrandoMpu()) {
        telaCalibracaoManualAtiva = false;
      }
      vTaskDelay(pdMS_TO_TICKS(80));
      continue;
    }

    if (inicioPressionado == 0) {
      inicioPressionado = millis();
      ultimaTelaBoot = 0;
      aviso5sImpresso = false;
      aguardandoSoltarParaIniciar = false;
      telaCalibracaoManualAtiva = true;
      Serial.println("EVENTO;ASSISTENTE_V15;BOOT_PRESSIONADO;SEGURE_5S_PARA_CALIBRACAO_COMPLETA");
    }

    unsigned long segurandoMs = millis() - inicioPressionado;
    unsigned long limite = segurandoMs > 5000UL ? 5000UL : segurandoMs;
    int progressoBoot = (int)((limite * 100UL) / 5000UL);
    if (progressoBoot > 100) progressoBoot = 100;
    int faltam5s = (int)((5000UL - limite + 999UL) / 1000UL);

    if (millis() - ultimaTelaBoot >= 120UL) {
      ultimaTelaBoot = millis();
      if (segurandoMs < 5000UL) {
        mostrarTelaAlunoCalibracao("SEGURE", "BOOT 5S", progressoBoot, faltam5s);
      } else {
        mostrarTelaAlunoCalibracao("SOLTE", "INICIAR V15", 100, 0);
      }
    }

    if (!aviso5sImpresso && segurandoMs >= 5000UL) {
      aviso5sImpresso = true;
      aguardandoSoltarParaIniciar = true;
      Serial.println("EVENTO;ASSISTENTE_V15;BOOT_5S_CONFIRMADO;SOLTE_PARA_INICIAR");
    }

    vTaskDelay(pdMS_TO_TICKS(60));
  }
}

static void taskTelaAluno(void *parametro) {
  (void)parametro;

  while (true) {
    atualizarLedSync();
    atualizarTelaAlunoAtual("LOCAL");
    vTaskDelay(pdMS_TO_TICKS(intervaloTelaTempoRealMs));
  }
}

static void taskLoRaIcnp(void *parametro) {
  (void)parametro;

  while (true) {

  atualizarLedSync();

  unsigned long agoraLoop = millis();

  if (agoraLoop - ultimoStatusSensor >= intervaloStatusSensorMs) {
    ultimoStatusSensor = agoraLoop;
    imprimirStatusSensor();
  }

  if (sensorFisiologicoCalibrandoMpu() || telaCalibracaoManualAtiva) {
    ultimoStatusTela = sensorFisiologicoCalibrandoMpu() ? "CALIBRANDO" : "ASSIST V15";
    vTaskDelay(pdMS_TO_TICKS(50));
    continue;
  }

  PacoteRecebido beacon = receberMensagemLoRa(20);

  if (!beacon.recebido) {
    continue;
  }

  if (!pacoteEhDoTipoIcnp(beacon.mensagem, ICNP_TIPO_BEACON)) {
    continue;
  }

  String cicloTexto = extrairCampoIcnp(beacon.mensagem, "CICLO");
  String alvo = extrairCampoIcnp(beacon.mensagem, "ALVO");

  float tensaoAluno = lerTensaoBateria();
  String batAluno = montarBatParaPacote(tensaoAluno);
  String energiaAluno = montarLinhaEnergia(tensaoAluno);
  String statusEnergia = statusBateria(tensaoAluno);

  if (cicloTexto.length() == 0 || alvo.length() == 0) {
    pulsoLedSync(300);

    Serial.println("BEACON invalido: campo CICLO ou ALVO ausente.");
    ultimoStatusTela = "BEACON INV";
    // Tela atualizada pela TaskTelaAluno em tempo real.
    continue;
  }

  unsigned long ciclo = cicloTexto.toInt();

  if (alvo != ID_ALUNO) {
    pulsoLedSync(30);

    Serial.println();
    Serial.println("BEACON recebido, mas nao e para este aluno.");
    Serial.print("Meu ID: ");
    Serial.println(ID_ALUNO);
    Serial.print("ALVO: ");
    Serial.println(alvo);
    Serial.print("Energia Aluno: ");
    Serial.println(energiaAluno);
    Serial.print("Status Energia: ");
    Serial.println(statusEnergia);

    ultimoCicloTela = ciclo;
    ultimoStatusTela = "IGNORADO";
    // Tela atualizada pela TaskTelaAluno em tempo real.
    continue;
  }

  pulsoLedSync(80);

  Serial.println();
  Serial.println("===== BEACON RECEBIDO PARA ESTE ALUNO =====");
  Serial.print("Mensagem: ");
  Serial.println(beacon.mensagem);
  Serial.print("Ciclo: ");
  Serial.println(ciclo);
  Serial.print("Alvo: ");
  Serial.println(alvo);
  Serial.print("RSSI BEACON: ");
  Serial.print(beacon.rssi);
  Serial.println(" dBm");
  Serial.print("SNR BEACON: ");
  Serial.print(beacon.snr);
  Serial.println(" dB");

  Serial.print("Energia Aluno: ");
  Serial.println(energiaAluno);
  Serial.print("Status Energia: ");
  Serial.println(statusEnergia);
  Serial.print("BAT enviada no DATA: ");
  Serial.println(batAluno);

  unsigned long agora = millis();

  if (agora - ultimoEnvioData < intervaloMinimoTelemetriaMs) {
    Serial.println("DATA ignorado: intervalo minimo ainda nao atingido.");
    Serial.println("===========================");

    ultimoCicloTela = ciclo;
    ultimoStatusTela = "AGUARDANDO";
    // Tela atualizada pela TaskTelaAluno em tempo real.
    continue;
  }

  ultimoEnvioData = agora;

  bool sensorOk = sensorFisiologicoDisponivel();
  bool dedo = dedoDetectadoSensorFisiologico();
  bool paValida = paValidaSensorFisiologico();

  int frequenciaCardiaca = lerFrequenciaCardiacaExperimental();
  int spo2 = lerSpo2Experimental();
  int pressaoSistolica = lerPressaoSistolicaExperimental();
  int pressaoDiastolica = lerPressaoDiastolicaExperimental();

  long ir = lerIrSensorFisiologico();
  long red = lerRedSensorFisiologico();

  String uso = lerUsoSensorFisiologico();
  String sinalPpg = lerSinalPpgSensorFisiologico();
  String movimento = lerMovimentoSensorFisiologico();
  String movimentoApi = movimentoParaApi(movimento);
  String artefato = lerArtefatoPpgSensorFisiologico();

  String qualidade = textoQualidadeSensor(
    sensorOk,
    dedo,
    paValida,
    frequenciaCardiaca,
    spo2,
    sinalPpg
  );

  ultimoCicloTela = ciclo;
  ultimaSeqTela = contadorSeq;
  ultimoStatusTela = "ENVIANDO";

  // Tela atualizada pela TaskTelaAluno em tempo real.

  String data = montarDataIcnp(
    ID_ALUNO,
    contadorSeq,
    ciclo,
    frequenciaCardiaca,
    spo2,
    batAluno,
    ir,
    red,
    dedo,
    qualidade,
    pressaoSistolica,
    pressaoDiastolica,
    uso,
    sinalPpg,
    paValida,
    movimentoApi,
    artefato
  );

  Serial.print("FC enviada: ");
  Serial.println(frequenciaCardiaca > 0 ? String(frequenciaCardiaca) : "NA");
  Serial.print("SpO2 enviado: ");
  Serial.println(spo2 > 0 ? String(spo2) : "NA");
  Serial.print("PA experimental enviada: ");
  if (pressaoSistolica > 0 && pressaoDiastolica > 0) {
    Serial.print(pressaoSistolica);
    Serial.print("x");
    Serial.println(pressaoDiastolica);
  } else {
    Serial.println("NA");
  }
  Serial.print("IR enviado: ");
  Serial.println(ir);
  Serial.print("RED enviado: ");
  Serial.println(red);
  Serial.print("Dedo: ");
  Serial.println(dedo ? "SIM" : "NAO");
  Serial.print("Uso: ");
  Serial.println(uso);
  Serial.print("Sinal PPG: ");
  Serial.println(sinalPpg);
  Serial.print("PA valida: ");
  Serial.println(paValida ? "SIM" : "NAO");
  Serial.print("Movimento: ");
  Serial.println(movimentoApi);
  Serial.print("Artefato PPG: ");
  Serial.println(artefato);
  Serial.print("Qualidade: ");
  Serial.println(qualidade);

  String ppgSerial = montarJanelaPpgNormalizadaApi(AMOSTRAS_PPG_API);
  Serial.print("PPG janela normalizada: ");
  Serial.println(ppgSerial.length() > 0 ? ppgSerial : "NA");

  Serial.print("Enviando DATA: ");
  Serial.println(data);

  delay(80);

  enviarMensagemLoRa(data);
  pulsoLedSync(40);

  PacoteRecebido ack = receberMensagemLoRa(tempoEsperaAckMs);

  if (!ack.recebido) {
    pulsoLedSync(500);

    Serial.println("Timeout: ACK nao recebido.");
    Serial.println("===========================");

    ultimoStatusTela = "TIMEOUT";
    // Tela atualizada pela TaskTelaAluno em tempo real.

    contadorSeq++;
    continue;
  }

  Serial.print("Recebido: ");
  Serial.println(ack.mensagem);
  Serial.print("RSSI ACK: ");
  Serial.print(ack.rssi);
  Serial.println(" dBm");
  Serial.print("SNR ACK: ");
  Serial.print(ack.snr);
  Serial.println(" dB");

  if (ackIcnpConfere(ack.mensagem, ID_ALUNO, contadorSeq, ciclo)) {
    pulsoLedSync(150);

    Serial.println("ACK valido. Ciclo ICNP concluido.");

    ultimoStatusTela = "OK";
    // Tela atualizada pela TaskTelaAluno em tempo real.

    tentarEnviarPpgDebug(contadorSeq, ciclo, dedo, qualidade, paValida);
  } else {
    pulsoLedSync(300);

    Serial.println("ACK invalido: aluno, sequencia ou ciclo nao conferem.");

    ultimoStatusTela = "ACK INV";
    // Tela atualizada pela TaskTelaAluno em tempo real.
  }

  Serial.println("===========================");

  contadorSeq++;

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  Serial.begin(230400);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ALUNO - ICNP V16.14 COM PPG + MPU6050 + OLED U8G2 R0");
  Serial.println("Placa: Heltec WiFi LoRa 32 V2");
  Serial.println("Frequencia: 915 MHz");
  Serial.println("ID_ALUNO: " + ID_ALUNO);
  Serial.println("Fluxo: BEACON(ALVO) -> DATA(FC/SPO2/PA_EXP/PA_VALIDA/MOV/ARTEFATO) -> ACK");
  Serial.println("Sensor: MAX30105/MAX30102 com filtro DC exponencial e cruzamento de zero do IR");
  Serial.println("PA_EXP: estimativa empirica/experimental, nao medicao clinica");
  Serial.println("V16.14: PPG pos-ACK corrigido; ANDANDO separado de MOV_LEVE; transicoes mais rapidas.");
  Serial.println("OLED: U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, OLED_RST)");
#if ENVIAR_PPG_DEBUG_LORA
  Serial.println("Envio LoRa TIPO=PPG: ATIVADO");
#else
  Serial.println("Envio LoRa TIPO=PPG: DESATIVADO");
#endif
  Serial.println("================================");

  iniciarRadioLoRa();
  iniciarDisplayOled();
  iniciarMonitorBateria();
  iniciarLedSync();

  mostrarTelaAluno(ID_ALUNO, 0, 0, "SENSOR", "--");

  Serial.println("Iniciando sensor fisiologico...");
  iniciarSensorFisiologico();

  if (sensorFisiologicoDisponivel()) {
    iniciarTasksSensorFisiologico();
    Serial.println("Sensor fisiologico iniciado em task dedicada.");
  } else {
    Serial.println("Sensor fisiologico NAO encontrado. DATA sera enviado com campos NA/RUIM.");
  }

  if (sensorFisiologicoDisponivel() && calibracaoMpuPersistidaSensorFisiologico()) {
    Serial.println("EVENTO;CALIBRACAO_MPU;CONFIRMACAO_BOOT;CALIBRADO_NVS=SIM");
    Serial.print("EVENTO;PERFIL_V15;CONFIRMACAO_BOOT;PERFIL_NVS=");
    Serial.println(perfilAssistidoV15PersistidoSensorFisiologico() ? "SIM" : "NAO");
    mostrarTelaAlunoCalibracao(perfilAssistidoV15PersistidoSensorFisiologico() ? "V15 OK" : "CAL OK", perfilAssistidoV15PersistidoSensorFisiologico() ? "PERFIL SALVO" : "NVS SALVA", 100, 0);
    delay(1500);
  } else if (sensorFisiologicoDisponivel()) {
    Serial.println("EVENTO;CALIBRACAO_MPU;CONFIRMACAO_BOOT;CALIBRADO_NVS=NAO");
    mostrarTelaAlunoCalibracao("CAL NAO", "SEGURE BOOT", 0, 0);
    delay(1500);
  }

  atualizarTelaAlunoAtual("AGUARDANDO");

  Serial.println("Calibracao: BOOT 5s. V16.14 corrige PPG na API e reduz delay de movimento.");
  mostrarTelaAlunoCalibracao("BOOT", "5S CAL", 0, 5);
  delay(1200);

  xTaskCreatePinnedToCore(
    taskCalibracaoBootAluno,
    "TaskCalibracaoBootAluno",
    6144,
    NULL,
    3,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    taskTelaAluno,
    "TaskTelaAluno",
    6144,
    NULL,
    2,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    taskLoRaIcnp,
    "TaskLoRaIcnp",
    16384,
    NULL,
    2,
    NULL,
    0
  );

  Serial.println("LoRa iniciado com sucesso.");
  Serial.println("Display iniciado com sucesso.");
  Serial.println("Monitor de bateria iniciado com sucesso.");
  Serial.println("LED de sincronismo iniciado com sucesso.");
  Serial.println("Aguardando BEACON do Professor...");
}



void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
