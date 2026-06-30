#include "sensor_fisiologico.h"

#include <Wire.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <Preferences.h>

#ifdef I2C_BUFFER_LENGTH
  #undef I2C_BUFFER_LENGTH
#endif
#include "MAX30105.h"

#ifndef PI
#define PI 3.14159265358979323846
#endif

#define I2C_SDA 4
#define I2C_SCL 15
#define ENDERECO_MAX3010X 0x57
#define ENDERECO_MPU6050 0x68

#define LIMIAR_DEDO_IR 10000L
#define BUFFER_PPG_API 64
#define PINO_BOOT_CALIBRACAO 0
#define NVS_NAMESPACE_MPU "icnp_mpu"
#define NVS_CHAVE_VALIDA "valida"
#define NVS_CHAVE_AX "ax"
#define NVS_CHAVE_AY "ay"
#define NVS_CHAVE_AZ "az"
#define NVS_CHAVE_MOD "mod"
#define NVS_CHAVE_ROLL "roll"
#define NVS_CHAVE_PITCH "pitch"
#define NVS_CHAVE_GX "gx"
#define NVS_CHAVE_GY "gy"
#define NVS_CHAVE_GZ "gz"
#define NVS_CHAVE_PERFIL_V15 "perfil15"
#define NVS_CHAVE_PERFIL_RESUMO "perfil_res"

#define FC_MIN_ACEITA 35
#define FC_MAX_ACEITA 180
#define INTERVALO_BATIMENTO_MIN_MS 333UL
#define INTERVALO_BATIMENTO_MAX_MS 2000UL
#define TEMPO_SEM_BATIMENTO_MS 8000UL

#define SPO2_MIN_ACEITA 88
#define SPO2_MAX_ACEITA 100
#define AC_IR_MIN_QUALIDADE 30.0f
#define AC_RED_MIN_QUALIDADE 10.0f
#define RATIO_MIN_ACEITO 0.35f
#define RATIO_MAX_ACEITO 1.30f

#define TAXA_MPU_HZ 50
#define PERIODO_MPU_US (1000000UL / TAXA_MPU_HZ)
#define JANELA_MPU 25
#define ACCEL_SCALE 8192.0f
#define GYRO_SCALE 65.5f

#define ESTADO_USO_SEM_CONTATO 0
#define ESTADO_USO_USANDO 1
#define ESTADO_PPG_SEM_CONTATO 0
#define ESTADO_PPG_SINAL_BAIXO 1
#define ESTADO_PPG_SINAL_INSTAVEL 2
#define ESTADO_PPG_ATIVO 3

static MAX30105 sensorMax;
static TaskHandle_t taskSensorHandle = NULL;
static portMUX_TYPE muxDadosSensor = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t mutexI2cCompartilhado = NULL;
static Preferences preferenciasMpu;
static bool calibracaoMpuPersistida = false;
static bool perfilAssistidoV15Persistido = false;
static volatile bool calibracaoMpuEmAndamento = false;
static volatile int calibracaoMpuProgressoPct = 0;
static volatile int calibracaoMpuSegundosRestantes = 0;

static bool sensorOk = false;
static bool mpuOk = false;
static bool sinalPresente = false;
static bool qualidadeSpo2Ok = false;
static bool ratioOk = false;
static bool paValida = false;

static long irAtual = 0;
static long redAtual = 0;

static float filtroDC_IR = 0.0f;
static float filtroDC_RED = 0.0f;
static float sinalAC_IR = 0.0f;
static float sinalAC_RED = 0.0f;
static float sinalAnteriorIR = 0.0f;
static bool filtroInicializado = false;

static float picoAcIrCiclo = 0.0f;
static float picoAcRedCiclo = 0.0f;
static float ultimoPicoAcIr = 0.0f;
static float ultimoPicoAcRed = 0.0f;

static float bpmInstantaneo = 0.0f;
static int bpmFinal = 0;
static int spo2Final = 0;
static float ratioR = 0.0f;
static int pressaoSistolica = 0;
static int pressaoDiastolica = 0;

static int fcCorrigida = 0;
static int spo2Corrigida = 0;
static int sysCorrigida = 0;
static int diaCorrigida = 0;

// V16.10: o Professor consulta por BEACON, entao ele pode perder janelas curtas
// em que o PPG estava OK. Mantemos o ultimo valor experimental valido por
// alguns segundos apenas quando o usuario continua parado/apoiado, com dedo
// detectado e sem artefato alto. Isso evita a API ficar sempre NA sem inventar
// fisiologia em movimento.
#define HOLD_FISIO_VALIDO_MS 6000UL
#define FC_VARIACAO_MAX_PUBLICACAO_BPM 5
#define FC_VARIACAO_MAX_BATIMENTO_BPM 28.0f
#define FC_SUAVIZACAO_ANTERIOR 0.75f
#define FC_SUAVIZACAO_NOVA 0.25f
static int fcHoldValido = 0;
static int spo2HoldValido = 0;
static int sysHoldValido = 0;
static int diaHoldValido = 0;
static unsigned long instanteHoldValidoMs = 0;
static int fcPublicadaEstavel = 0;

static int estadoUso = ESTADO_USO_SEM_CONTATO;
static int estadoPpg = ESTADO_PPG_SEM_CONTATO;
static unsigned long tempoUltimoBatimento = 0;

static uint32_t bufferPpgApi[BUFFER_PPG_API];
static int indicePpgApi = 0;
static int totalPpgApi = 0;

// MPU bruto/convertido
static int16_t axRaw = 0, ayRaw = 0, azRaw = 0;
static int16_t gxRaw = 0, gyRaw = 0, gzRaw = 0;
static float axG = 0, ayG = 0, azG = 0;
static float gxDps = 0, gyDps = 0, gzDps = 0;
static float rollDeg = 0, pitchDeg = 0, modG = 1.0f;
static float baseModG = 1.0f, baseRoll = 0.0f, basePitch = 0.0f;
static float baseAxG = 0.0f, baseAyG = 0.0f, baseAzG = 1.0f;
static float prevAxG = 0.0f, prevAyG = 0.0f, prevAzG = 0.0f;
static bool prevMpuValido = false;
static float offGx = 0.0f, offGy = 0.0f, offGz = 0.0f;
static unsigned long proximaAmostraMPUUs = 0;

static float histGyro[JANELA_MPU];
static float histDMod[JANELA_MPU];
static float histDAccel[JANELA_MPU];
static float histAng[JANELA_MPU];
static int histIdx = 0;
static int histCount = 0;
static float gyroMed = 0.0f, gyroMax = 0.0f, dmodMed = 0.0f, dmodMax = 0.0f, daccelMed = 0.0f, angMed = 0.0f;
static char movimentoAtual[24] = "MPU_NA";
static char movCurtoAtual[10] = "NA";
static char artefatoPpgAtual[10] = "NA";
static char qualidadeCorrigidaAtual[20] = "NA";

// V16.14: historico ainda mais curto para reduzir o atraso visual da API.
// Mantem histerese contra ruido, mas troca mais rapido entre pular/correr/
// andar/sentado/parado.
#define HIST_MOVIMENTO_ESTAVEL 9
static int histMovimentoClasse[HIST_MOVIMENTO_ESTAVEL];
static int histMovimentoIdx = 0;
static int histMovimentoCount = 0;

static void copiarTexto(char *destino, size_t tamanho, const char *origem) {
  strncpy(destino, origem, tamanho - 1);
  destino[tamanho - 1] = '\0';
}

static int codigoMovimento(const char *mov) {
  if (strcmp(mov, "PARADO_BASE") == 0) return 1;
  if (strcmp(mov, "APOIADO_POSTURA") == 0) return 2;
  if (strcmp(mov, "MICRO_MOV") == 0) return 3;
  if (strcmp(mov, "DIG_MICRO") == 0) return 4;
  if (strcmp(mov, "MOV_APOIADO") == 0) return 5;
  if (strcmp(mov, "MOV_LEVE") == 0) return 6;
  if (strcmp(mov, "ANDANDO") == 0) return 7;
  if (strcmp(mov, "IMPACTO_VERTICAL") == 0) return 8;
  if (strcmp(mov, "CORRIDA_INTENSA") == 0) return 9;
  return 0;
}

static const char *textoMovimentoPorCodigo(int cod) {
  switch (cod) {
    case 1: return "PARADO_BASE";
    case 2: return "APOIADO_POSTURA";
    case 3: return "MICRO_MOV";
    case 4: return "DIG_MICRO";
    case 5: return "MOV_APOIADO";
    case 6: return "MOV_LEVE";
    case 7: return "ANDANDO";
    case 8: return "IMPACTO_VERTICAL";
    case 9: return "CORRIDA_INTENSA";
    default: return "MPU_NA";
  }
}

static const char *curtoMovimentoPorCodigo(int cod) {
  switch (cod) {
    case 1: return "BASE";
    case 2: return "POST";
    case 3: return "MICRO";
    case 4: return "DIG";
    case 5: return "APOIO";
    case 6: return "LEVE";
    case 7: return "ANDA";
    case 8: return "PULO";
    case 9: return "CORRE";
    default: return "NA";
  }
}

static void limparFiltroMovimento() {
  for (int i = 0; i < HIST_MOVIMENTO_ESTAVEL; i++) histMovimentoClasse[i] = 0;
  histMovimentoIdx = 0;
  histMovimentoCount = 0;
}

static void definirMovimentoDireto(const char *mov, const char *curto) {
  copiarTexto(movimentoAtual, sizeof(movimentoAtual), mov);
  copiarTexto(movCurtoAtual, sizeof(movCurtoAtual), curto);
}

static void definirMovimentoFiltrado(const char *mov, const char *curto) {
  int cod = codigoMovimento(mov);
  if (cod == 0) {
    limparFiltroMovimento();
    definirMovimentoDireto(mov, curto);
    return;
  }

  histMovimentoClasse[histMovimentoIdx] = cod;
  histMovimentoIdx = (histMovimentoIdx + 1) % HIST_MOVIMENTO_ESTAVEL;
  if (histMovimentoCount < HIST_MOVIMENTO_ESTAVEL) histMovimentoCount++;

  int contagem[10];
  for (int i = 0; i < 10; i++) contagem[i] = 0;
  for (int i = 0; i < histMovimentoCount; i++) {
    int c = histMovimentoClasse[i];
    if (c >= 0 && c <= 9) contagem[c]++;
  }

  int escolhido = cod;
  int melhor = -1;
  for (int c = 1; c <= 9; c++) {
    if (contagem[c] > melhor) { melhor = contagem[c]; escolhido = c; }
  }

  int atual = codigoMovimento(movimentoAtual);

  // V16.14: a API estava demorando para sair de sentado/apoiado para movimento.
  // Se o usuario levanta ou inicia uma acao, um voto novo ja quebra o
  // estado parado/apoiado. Para voltar ao repouso, exigimos dois votos,
  // evitando piscar entre classes.
  if (atual >= 1 && atual <= 2 && cod >= 6 && contagem[cod] >= 1) {
    escolhido = cod;
  }

  if (atual >= 6 && cod >= 1 && cod <= 2 && contagem[cod] >= 2) {
    escolhido = cod;
  }

  // Picos de impacto/corrida aparecem em passos ou ajuste de luva. So troca
  // para classe forte se ela for recorrente dentro da janela curta.
  if ((escolhido == 8 || escolhido == 9) && melhor < 3 && histMovimentoCount >= 6) {
    int melhorSemForte = -1;
    int codSemForte = 6;
    for (int c = 1; c <= 7; c++) {
      if (contagem[c] > melhorSemForte) { melhorSemForte = contagem[c]; codSemForte = c; }
    }
    escolhido = codSemForte;
  }

  definirMovimentoDireto(textoMovimentoPorCodigo(escolhido), curtoMovimentoPorCodigo(escolhido));
}

static bool fcAceita(float valor) { return valor >= FC_MIN_ACEITA && valor <= FC_MAX_ACEITA; }
static bool spo2Aceita(int valor) { return valor >= SPO2_MIN_ACEITA && valor <= SPO2_MAX_ACEITA; }

static void garantirMutexI2cCompartilhado() {
  if (mutexI2cCompartilhado == NULL) mutexI2cCompartilhado = xSemaphoreCreateMutex();
}

void bloquearI2cCompartilhado() {
  garantirMutexI2cCompartilhado();
  if (mutexI2cCompartilhado != NULL) xSemaphoreTake(mutexI2cCompartilhado, portMAX_DELAY);
}

void desbloquearI2cCompartilhado() {
  if (mutexI2cCompartilhado != NULL) xSemaphoreGive(mutexI2cCompartilhado);
}

static void limparBufferPpg() {
  indicePpgApi = 0;
  totalPpgApi = 0;
  for (int i = 0; i < BUFFER_PPG_API; i++) bufferPpgApi[i] = 0;
}

static void limparValoresSemContato() {
  sinalPresente = false;
  qualidadeSpo2Ok = false;
  ratioOk = false;
  paValida = false;
  estadoUso = ESTADO_USO_SEM_CONTATO;
  estadoPpg = ESTADO_PPG_SEM_CONTATO;
  bpmInstantaneo = 0.0f;
  bpmFinal = 0;
  spo2Final = 0;
  ratioR = 0.0f;
  pressaoSistolica = 0;
  pressaoDiastolica = 0;
  fcCorrigida = 0;
  spo2Corrigida = 0;
  sysCorrigida = 0;
  diaCorrigida = 0;
  fcHoldValido = 0;
  spo2HoldValido = 0;
  sysHoldValido = 0;
  diaHoldValido = 0;
  instanteHoldValidoMs = 0;
  fcPublicadaEstavel = 0;
  filtroInicializado = false;
  sinalAnteriorIR = 0.0f;
  sinalAC_IR = 0.0f;
  sinalAC_RED = 0.0f;
  picoAcIrCiclo = 0.0f;
  picoAcRedCiclo = 0.0f;
  ultimoPicoAcIr = 0.0f;
  ultimoPicoAcRed = 0.0f;
  tempoUltimoBatimento = 0;
  copiarTexto(qualidadeCorrigidaAtual, sizeof(qualidadeCorrigidaAtual), "SEM_CONTATO");
  limparBufferPpg();
}

static void adicionarAmostraNoBufferPpgApi(long ir) {
  if (ir < 0) ir = 0;
  bufferPpgApi[indicePpgApi] = (uint32_t)ir;
  indicePpgApi++;
  if (indicePpgApi >= BUFFER_PPG_API) indicePpgApi = 0;
  if (totalPpgApi < BUFFER_PPG_API) totalPpgApi++;
}

// ---------------------- MPU6050 ----------------------
static void escreverRegistroMPU(uint8_t reg, uint8_t valor) {
  Wire.beginTransmission(ENDERECO_MPU6050);
  Wire.write(reg);
  Wire.write(valor);
  Wire.endTransmission(true);
}

static bool lerMPUBruto() {
  Wire.beginTransmission(ENDERECO_MPU6050);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  size_t n = Wire.requestFrom((uint8_t)ENDERECO_MPU6050, (size_t)14, true);
  if (n != 14) return false;
  axRaw = (Wire.read() << 8) | Wire.read();
  ayRaw = (Wire.read() << 8) | Wire.read();
  azRaw = (Wire.read() << 8) | Wire.read();
  Wire.read(); Wire.read();
  gxRaw = (Wire.read() << 8) | Wire.read();
  gyRaw = (Wire.read() << 8) | Wire.read();
  gzRaw = (Wire.read() << 8) | Wire.read();
  return true;
}

static void converterMPU() {
  axG = (float)axRaw / ACCEL_SCALE;
  ayG = (float)ayRaw / ACCEL_SCALE;
  azG = (float)azRaw / ACCEL_SCALE;
  gxDps = ((float)gxRaw / GYRO_SCALE) - offGx;
  gyDps = ((float)gyRaw / GYRO_SCALE) - offGy;
  gzDps = ((float)gzRaw / GYRO_SCALE) - offGz;
  rollDeg = atan2f(ayG, azG) * 180.0f / PI;
  pitchDeg = atan2f(-axG, sqrtf(ayG * ayG + azG * azG)) * 180.0f / PI;
  modG = sqrtf(axG * axG + ayG * ayG + azG * azG);
}

static void limparJanelaMPU() {
  for (int i = 0; i < JANELA_MPU; i++) { histGyro[i] = histDMod[i] = histDAccel[i] = histAng[i] = 0.0f; }
  histIdx = 0;
  histCount = 0;
  gyroMed = 0.0f;
  gyroMax = 0.0f;
  dmodMed = 0.0f;
  dmodMax = 0.0f;
  daccelMed = 0.0f;
  angMed = 0.0f;
  prevMpuValido = false;
  limparFiltroMovimento();
}

static void atualizarJanelaMPU() {
  // V16.3: volta ao comportamento do firmware local V15.
  // O delta de aceleracao e de angulo sao calculados contra a base calibrada
  // do proprio aluno, salva em NVS. Isso evita que a inclinacao normal da pulseira
  // seja interpretada como corrida/impacto.
  float gyroAbs = fabsf(gxDps) + fabsf(gyDps) + fabsf(gzDps);

  float dxBase = axG - baseAxG;
  float dyBase = ayG - baseAyG;
  float dzBase = azG - baseAzG;
  float daccel = sqrtf((dxBase * dxBase) + (dyBase * dyBase) + (dzBase * dzBase));

  float dmod = fabsf(modG - baseModG);
  float dRoll = rollDeg - baseRoll;
  float dPitch = pitchDeg - basePitch;
  float dang = sqrtf((dRoll * dRoll) + (dPitch * dPitch));

  histGyro[histIdx] = gyroAbs;
  histDMod[histIdx] = dmod;
  histDAccel[histIdx] = daccel;
  histAng[histIdx] = dang;
  histIdx = (histIdx + 1) % JANELA_MPU;
  if (histCount < JANELA_MPU) histCount++;

  gyroMed = dmodMed = daccelMed = angMed = 0.0f;
  gyroMax = dmodMax = 0.0f;
  for (int i = 0; i < histCount; i++) {
    gyroMed += histGyro[i];
    dmodMed += histDMod[i];
    daccelMed += histDAccel[i];
    angMed += histAng[i];
    if (histGyro[i] > gyroMax) gyroMax = histGyro[i];
    if (histDMod[i] > dmodMax) dmodMax = histDMod[i];
  }
  if (histCount > 0) {
    gyroMed /= histCount;
    dmodMed /= histCount;
    daccelMed /= histCount;
    angMed /= histCount;
  }
}

static void classificarMovimentoV8() {
  if (mpuOk && !calibracaoMpuPersistida) {
    definirMovimentoDireto("MPU_SEM_CAL", "CAL");
    limparFiltroMovimento();
    return;
  }

  if (!mpuOk || histCount < 5) {
    definirMovimentoDireto("MPU_AQUECENDO", "AQUEC");
    limparFiltroMovimento();
    return;
  }

  // V16.14: mantem separadas quatro coisas que estavam ficando misturadas:
  // repouso, apoio/postura, andando ritmico e movimento leve aleatorio de mao/braco.
  if (perfilAssistidoV15Persistido) {
    bool repousoProximoBase =
      gyroMed < 8.0f && gyroMax < 25.0f && dmodMax < 0.120f && daccelMed < 0.130f && angMed < 7.0f;

    bool repousoPosturaDiferente =
      gyroMed < 9.0f && gyroMax < 28.0f && dmodMax < 0.130f && daccelMed < 0.150f && angMed >= 7.0f;

    if (repousoProximoBase) {
      definirMovimentoFiltrado("PARADO_BASE", "BASE");
      return;
    }

    if (repousoPosturaDiferente) {
      definirMovimentoFiltrado("APOIADO_POSTURA", "POST");
      return;
    }
  }

  bool posturaDiferente = (angMed > 18.0f || daccelMed > 0.20f);

  bool impactoVertical =
    dmodMax > 0.880f ||
    dmodMed > 0.460f ||
    (dmodMax > 0.740f && daccelMed > 0.520f) ||
    (dmodMax > 0.800f && gyroMax > 145.0f && angMed > 10.0f);

  if (impactoVertical) {
    definirMovimentoFiltrado("IMPACTO_VERTICAL", "PULO");
    return;
  }

  // Corrida agora exige variacao corporal mais forte que a caminhada.
  // Isso evita que andar balancando os bracos apareca como CORRIDA_INTENSA.
  bool corridaIntensa =
    (gyroMed > 92.0f && dmodMed > 0.205f && daccelMed > 0.330f && angMed > 14.0f) ||
    (gyroMed > 115.0f && dmodMed > 0.175f && daccelMed > 0.300f && dmodMax > 0.420f) ||
    (gyroMax > 240.0f && dmodMax > 0.500f && daccelMed > 0.360f);

  if (corridaIntensa) {
    definirMovimentoFiltrado("CORRIDA_INTENSA", "CORRE");
    return;
  }

  // ANDANDO: movimento ritmico/sustentado do braco, com energia maior que
  // movimento leve, mas sem a variacao vertical/corporal de corrida ou pulo.
  bool andandoRitmico =
    (gyroMed >= 45.0f && gyroMed <= 125.0f &&
     dmodMed >= 0.045f && dmodMed <= 0.185f &&
     daccelMed >= 0.095f && daccelMed <= 0.320f &&
     angMed >= 4.0f && angMed <= 15.5f) ||
    (gyroMed >= 70.0f && gyroMed <= 130.0f &&
     dmodMed >= 0.060f && dmodMed <= 0.200f &&
     daccelMed >= 0.150f && daccelMed <= 0.330f);

  if (andandoRitmico) {
    definirMovimentoFiltrado("ANDANDO", "ANDA");
    return;
  }

  if (posturaDiferente) {
    if (gyroMed < 8.0f && gyroMax < 25.0f && dmodMax < 0.130f) {
      definirMovimentoFiltrado("APOIADO_POSTURA", "POST");
      return;
    }

    if (gyroMed < 28.0f && gyroMax < 70.0f && dmodMax < 0.220f) {
      definirMovimentoFiltrado("DIG_MICRO", "DIG");
      return;
    }

    if (gyroMed < 50.0f && gyroMax < 130.0f && dmodMax < 0.350f) {
      definirMovimentoFiltrado("MOV_APOIADO", "APOIO");
      return;
    }

    definirMovimentoFiltrado("MOV_LEVE", "LEVE");
    return;
  }

  if (gyroMed < 8.0f && gyroMax < 25.0f && dmodMax < 0.100f && daccelMed < 0.120f && angMed < 7.0f) {
    definirMovimentoFiltrado("PARADO_BASE", "BASE");
    return;
  }

  if (gyroMed < 15.0f && gyroMax < 35.0f && dmodMax < 0.130f && daccelMed < 0.150f && angMed < 10.0f) {
    definirMovimentoFiltrado(perfilAssistidoV15Persistido ? "PARADO_BASE" : "MICRO_MOV", perfilAssistidoV15Persistido ? "BASE" : "MICRO");
    return;
  }

  // Sem assinatura ritmica suficiente para ANDANDO: movimento leve/aleatorio.
  definirMovimentoFiltrado("MOV_LEVE", "LEVE");
}

static bool movimentoPermiteFisiologiaEstavel() {
  return strcmp(movimentoAtual, "PARADO_BASE") == 0 || strcmp(movimentoAtual, "APOIADO_POSTURA") == 0;
}

static bool movimentoPermiteHoldFisiologia() {
  // V16.14: hold curto so em repouso/base ou postura apoiada real.
  // MOV_APOIADO/ANDANDO/MOV_LEVE limpam a fisiologia para nao parecer travada.
  return movimentoPermiteFisiologiaEstavel();
}

static int suavizarFcParaPublicacao(int fcNovo) {
  if (fcNovo <= 0) return 0;
  if (fcPublicadaEstavel <= 0) {
    fcPublicadaEstavel = fcNovo;
    return fcPublicadaEstavel;
  }

  int delta = fcNovo - fcPublicadaEstavel;
  if (delta > FC_VARIACAO_MAX_PUBLICACAO_BPM) delta = FC_VARIACAO_MAX_PUBLICACAO_BPM;
  if (delta < -FC_VARIACAO_MAX_PUBLICACAO_BPM) delta = -FC_VARIACAO_MAX_PUBLICACAO_BPM;
  fcPublicadaEstavel += delta;
  return fcPublicadaEstavel;
}

static void classificarArtefatoPpg() {
  if (!mpuOk) { copiarTexto(artefatoPpgAtual, sizeof(artefatoPpgAtual), "MPU_NA"); return; }
  if (strcmp(movimentoAtual, "CORRIDA_INTENSA") == 0 || strcmp(movimentoAtual, "IMPACTO_VERTICAL") == 0) {
    copiarTexto(artefatoPpgAtual, sizeof(artefatoPpgAtual), "ALTO");
  } else if (strcmp(movimentoAtual, "ANDANDO") == 0 || strcmp(movimentoAtual, "MOV_LEVE") == 0 || strcmp(movimentoAtual, "MICRO_MOV") == 0 || strcmp(movimentoAtual, "DIG_MICRO") == 0 || strcmp(movimentoAtual, "MOV_APOIADO") == 0) {
    copiarTexto(artefatoPpgAtual, sizeof(artefatoPpgAtual), "MEDIO");
  } else {
    copiarTexto(artefatoPpgAtual, sizeof(artefatoPpgAtual), "BAIXO");
  }
}

static void atualizarMPU50Hz() {
  if (!mpuOk || calibracaoMpuEmAndamento) return;
  unsigned long agoraUs = micros();
  if ((int32_t)(agoraUs - proximaAmostraMPUUs) < 0) return;
  proximaAmostraMPUUs = agoraUs + PERIODO_MPU_US;

  bloquearI2cCompartilhado();
  bool ok = lerMPUBruto();
  desbloquearI2cCompartilhado();
  if (!ok) return;

  converterMPU();
  portENTER_CRITICAL(&muxDadosSensor);
  atualizarJanelaMPU();
  classificarMovimentoV8();
  classificarArtefatoPpg();
  portEXIT_CRITICAL(&muxDadosSensor);
}


static bool bootCalibracaoPressionado() {
  pinMode(PINO_BOOT_CALIBRACAO, INPUT_PULLUP);
  return digitalRead(PINO_BOOT_CALIBRACAO) == LOW;
}

bool botaoCalibracaoMpuPressionadoSensorFisiologico() {
  return bootCalibracaoPressionado();
}

bool sensorFisiologicoCalibrandoMpu() {
  return calibracaoMpuEmAndamento;
}

int progressoCalibracaoMpuSensorFisiologico() {
  return calibracaoMpuProgressoPct;
}

int segundosRestantesCalibracaoMpuSensorFisiologico() {
  return calibracaoMpuSegundosRestantes;
}

bool calibracaoMpuPersistidaSensorFisiologico() {
  return calibracaoMpuPersistida;
}

static bool carregarCalibracaoMpuNvs() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE_MPU, true)) {
    return false;
  }
  bool valida = prefs.getBool(NVS_CHAVE_VALIDA, false);
  if (valida) {
    baseAxG = prefs.getFloat(NVS_CHAVE_AX, baseAxG);
    baseAyG = prefs.getFloat(NVS_CHAVE_AY, baseAyG);
    baseAzG = prefs.getFloat(NVS_CHAVE_AZ, baseAzG);
    baseModG = prefs.getFloat(NVS_CHAVE_MOD, baseModG);
    baseRoll = prefs.getFloat(NVS_CHAVE_ROLL, baseRoll);
    basePitch = prefs.getFloat(NVS_CHAVE_PITCH, basePitch);
    offGx = prefs.getFloat(NVS_CHAVE_GX, offGx);
    offGy = prefs.getFloat(NVS_CHAVE_GY, offGy);
    offGz = prefs.getFloat(NVS_CHAVE_GZ, offGz);
  }
  perfilAssistidoV15Persistido = prefs.getBool(NVS_CHAVE_PERFIL_V15, false);
  prefs.end();
  calibracaoMpuPersistida = valida;
  return valida;
}

static void salvarCalibracaoMpuNvs() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE_MPU, false)) {
    Serial.println("EVENTO;CALIBRACAO_MPU;ERRO_NVS_ABRIR");
    return;
  }
  prefs.putFloat(NVS_CHAVE_AX, baseAxG);
  prefs.putFloat(NVS_CHAVE_AY, baseAyG);
  prefs.putFloat(NVS_CHAVE_AZ, baseAzG);
  prefs.putFloat(NVS_CHAVE_MOD, baseModG);
  prefs.putFloat(NVS_CHAVE_ROLL, baseRoll);
  prefs.putFloat(NVS_CHAVE_PITCH, basePitch);
  prefs.putFloat(NVS_CHAVE_GX, offGx);
  prefs.putFloat(NVS_CHAVE_GY, offGy);
  prefs.putFloat(NVS_CHAVE_GZ, offGz);
  prefs.putBool(NVS_CHAVE_VALIDA, true);
  prefs.end();
  calibracaoMpuPersistida = true;
  Serial.println("EVENTO;CALIBRACAO_MPU;NVS_SALVA;OK");
}


bool perfilAssistidoV15PersistidoSensorFisiologico() {
  return perfilAssistidoV15Persistido;
}

bool salvarPerfilAssistidoV15SensorFisiologico(const char *resumo) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE_MPU, false)) {
    Serial.println("EVENTO;PERFIL_V15;ERRO_NVS_ABRIR");
    return false;
  }
  prefs.putBool(NVS_CHAVE_PERFIL_V15, true);
  prefs.putString(NVS_CHAVE_PERFIL_RESUMO, resumo ? resumo : "ASSISTENTE_COMPLETO_V15_OK");
  prefs.end();
  perfilAssistidoV15Persistido = true;
  Serial.print("EVENTO;PERFIL_V15;NVS_SALVA;OK;RESUMO=");
  Serial.println(resumo ? resumo : "ASSISTENTE_COMPLETO_V15_OK");
  return true;
}

static void imprimirCalibracaoMpu(const char *origem) {
  Serial.print("EVENTO;CALIBRACAO_MPU;");
  Serial.print(origem);
  Serial.print(";AX0="); Serial.print(baseAxG, 5);
  Serial.print(";AY0="); Serial.print(baseAyG, 5);
  Serial.print(";AZ0="); Serial.print(baseAzG, 5);
  Serial.print(";MODG0="); Serial.print(baseModG, 5);
  Serial.print(";ROLL0="); Serial.print(baseRoll, 3);
  Serial.print(";PITCH0="); Serial.print(basePitch, 3);
  Serial.print(";GX_OFFSET="); Serial.print(offGx, 4);
  Serial.print(";GY_OFFSET="); Serial.print(offGy, 4);
  Serial.print(";GZ_OFFSET="); Serial.println(offGz, 4);
}

static bool calibrarMpuRepousoESalvar(const char *motivo, int amostras, int intervaloMs) {
  calibracaoMpuProgressoPct = 0;
  calibracaoMpuSegundosRestantes = (amostras * intervaloMs + 999) / 1000;
  Serial.print("EVENTO;CALIBRACAO_MPU;INICIO;");
  Serial.print(motivo);
  Serial.println(";MANTENHA_PULSEIRA_PARADA");

  double sx = 0.0, sy = 0.0, sz = 0.0, sgx = 0.0, sgy = 0.0, sgz = 0.0, sroll = 0.0, spitch = 0.0, smod = 0.0;
  int n = 0;
  for (int i = 0; i < amostras; i++) {
    bloquearI2cCompartilhado();
    bool ok = lerMPUBruto();
    desbloquearI2cCompartilhado();
    if (ok) {
      float ax = (float)axRaw / ACCEL_SCALE;
      float ay = (float)ayRaw / ACCEL_SCALE;
      float az = (float)azRaw / ACCEL_SCALE;
      float gx = (float)gxRaw / GYRO_SCALE;
      float gy = (float)gyRaw / GYRO_SCALE;
      float gz = (float)gzRaw / GYRO_SCALE;
      float roll = atan2f(ay, az) * 180.0f / PI;
      float pitch = atan2f(-ax, sqrtf((ay * ay) + (az * az))) * 180.0f / PI;
      float mod = sqrtf((ax * ax) + (ay * ay) + (az * az));
      sx += ax; sy += ay; sz += az; sgx += gx; sgy += gy; sgz += gz; sroll += roll; spitch += pitch; smod += mod;
      n++;
    }
    int pct = ((i + 1) * 100) / amostras;
    calibracaoMpuProgressoPct = pct;
    int faltam = ((amostras - (i + 1)) * intervaloMs + 999) / 1000;
    calibracaoMpuSegundosRestantes = faltam;

    if ((i % 25) == 0 || i == amostras - 1) {
      Serial.print("EVENTO;CALIBRACAO_MPU;PROGRESSO;");
      Serial.print(i + 1);
      Serial.print("/");
      Serial.print(amostras);
      Serial.print(";PCT=");
      Serial.print(pct);
      Serial.print(";FALTAM_S=");
      Serial.println(faltam);
    }
    delay(intervaloMs);
  }

  if (n < 50) {
    calibracaoMpuProgressoPct = 0;
    calibracaoMpuSegundosRestantes = 0;
    Serial.println("EVENTO;CALIBRACAO_MPU;FALHA;AMOSTRAS_INSUFICIENTES");
    return false;
  }

  offGx = sgx / n;
  offGy = sgy / n;
  offGz = sgz / n;
  baseAxG = sx / n;
  baseAyG = sy / n;
  baseAzG = sz / n;
  baseRoll = sroll / n;
  basePitch = spitch / n;
  baseModG = smod / n;
  limparJanelaMPU();
  imprimirCalibracaoMpu("FIM");
  salvarCalibracaoMpuNvs();
  calibracaoMpuProgressoPct = 100;
  calibracaoMpuSegundosRestantes = 0;
  return true;
}


bool recalibrarMpuSensorFisiologico(const char *motivo) {
  if (!mpuOk) {
    Serial.println("EVENTO;CALIBRACAO_MPU;ERRO;MPU_NAO_INICIADO");
    return false;
  }

  Serial.print("EVENTO;CALIBRACAO_MPU;MANUAL_SOLICITADA;");
  Serial.println(motivo ? motivo : "BOOT_3S");

  calibracaoMpuEmAndamento = true;
  portENTER_CRITICAL(&muxDadosSensor);
  paValida = false;
  fcCorrigida = 0;
  spo2Corrigida = 0;
  sysCorrigida = 0;
  diaCorrigida = 0;
  copiarTexto(movimentoAtual, sizeof(movimentoAtual), "CALIBRANDO_MPU");
  copiarTexto(movCurtoAtual, sizeof(movCurtoAtual), "CAL");
  copiarTexto(artefatoPpgAtual, sizeof(artefatoPpgAtual), "NA");
  copiarTexto(qualidadeCorrigidaAtual, sizeof(qualidadeCorrigidaAtual), "CALIBRANDO_MPU");
  portEXIT_CRITICAL(&muxDadosSensor);

  bool ok = calibrarMpuRepousoESalvar(motivo ? motivo : "BOOT_3S", 500, 20);
  if (!ok) {
    calibracaoMpuProgressoPct = 0;
    calibracaoMpuSegundosRestantes = 0;
  }
  limparJanelaMPU();
  proximaAmostraMPUUs = micros();
  calibracaoMpuEmAndamento = false;

  Serial.print("EVENTO;CALIBRACAO_MPU;MANUAL_FIM;");
  Serial.println(ok ? "OK" : "FALHA");
  return ok;
}

static bool inicializarMPU() {
  bloquearI2cCompartilhado();
  Wire.beginTransmission(ENDERECO_MPU6050);
  bool encontrado = (Wire.endTransmission() == 0);
  if (encontrado) {
    escreverRegistroMPU(0x6B, 0x00);
    delay(50);
    escreverRegistroMPU(0x1A, 0x03);
    escreverRegistroMPU(0x19, 0x04);
    escreverRegistroMPU(0x1C, 0x08);
    escreverRegistroMPU(0x1B, 0x08);
  }
  desbloquearI2cCompartilhado();

  if (!encontrado) {
    Serial.println("MPU6050/GY-521 nao encontrado em 0x68. Fisiologia ficara bloqueada por seguranca.");
    mpuOk = false;
    return false;
  }

  pinMode(PINO_BOOT_CALIBRACAO, INPUT_PULLUP);

  bool carregou = carregarCalibracaoMpuNvs();

  if (carregou) {
    imprimirCalibracaoMpu("NVS_CARREGADA");
    if (perfilAssistidoV15Persistido) {
      Serial.println("EVENTO;PERFIL_V15;NVS_CARREGADO;OK");
    } else {
      Serial.println("EVENTO;PERFIL_V15;NVS_AUSENTE;ASSISTENTE_COMPLETO_RECOMENDADO");
    }
  } else {
    calibracaoMpuPersistida = false;
    Serial.println("EVENTO;CALIBRACAO_MPU;NVS_AUSENTE;AGUARDANDO_BOOT_3S_ASSISTIDO");
    Serial.println("EVENTO;CALIBRACAO_MPU;ORIENTACAO;SEGURE_BOOT_3S_COM_PULSEIRA_PARADA");
  }

  limparJanelaMPU();
  mpuOk = true;
  proximaAmostraMPUUs = micros();
  Serial.print("MPU6050/GY-521 encontrado em 0x68; BASE_MODG="); Serial.print(baseModG, 4);
  Serial.print("; ROLL0="); Serial.print(baseRoll, 2);
  Serial.print("; PITCH0="); Serial.print(basePitch, 2);
  Serial.print("; CALIB_NVS="); Serial.println(calibracaoMpuPersistida ? "SIM" : "NAO");
  return true;
}

// ---------------------- PPG/fusao ----------------------
static void atualizarFusaoCorrigida() {
  fcCorrigida = 0;
  spo2Corrigida = 0;
  sysCorrigida = 0;
  diaCorrigida = 0;
  paValida = false;

  auto publicarHoldSeSeguro = [&]() -> bool {
    if (instanteHoldValidoMs == 0) return false;
    if ((millis() - instanteHoldValidoMs) > HOLD_FISIO_VALIDO_MS) return false;
    if (!sinalPresente) return false;
    if (!movimentoPermiteHoldFisiologia()) return false;
    if (strcmp(artefatoPpgAtual, "ALTO") == 0) return false;
    if (fcHoldValido <= 0 || spo2HoldValido <= 0 || sysHoldValido <= 0 || diaHoldValido <= 0) return false;

    fcCorrigida = fcHoldValido;
    spo2Corrigida = spo2HoldValido;
    sysCorrigida = sysHoldValido;
    diaCorrigida = diaHoldValido;
    paValida = true;
    copiarTexto(qualidadeCorrigidaAtual, sizeof(qualidadeCorrigidaAtual), "OK_HOLD");
    return true;
  };

  if (calibracaoMpuEmAndamento) {
    copiarTexto(qualidadeCorrigidaAtual, sizeof(qualidadeCorrigidaAtual), "CALIBRANDO_MPU");
    return;
  }

  if (mpuOk && !calibracaoMpuPersistida) {
    copiarTexto(movimentoAtual, sizeof(movimentoAtual), "MPU_SEM_CAL");
    copiarTexto(movCurtoAtual, sizeof(movCurtoAtual), "CAL");
    copiarTexto(artefatoPpgAtual, sizeof(artefatoPpgAtual), "ALTO");
    copiarTexto(qualidadeCorrigidaAtual, sizeof(qualidadeCorrigidaAtual), "MPU_SEM_CAL");
    return;
  }

  if (!sinalPresente) {
    copiarTexto(qualidadeCorrigidaAtual, sizeof(qualidadeCorrigidaAtual), "SEM_CONTATO");
    return;
  }

  if (!movimentoPermiteFisiologiaEstavel()) {
    if (!movimentoPermiteHoldFisiologia()) {
      instanteHoldValidoMs = 0;
      fcHoldValido = 0;
      spo2HoldValido = 0;
      sysHoldValido = 0;
      diaHoldValido = 0;
    }
    if (publicarHoldSeSeguro()) return;
    copiarTexto(qualidadeCorrigidaAtual, sizeof(qualidadeCorrigidaAtual), "RUIM_MOV");
    return;
  }

  if (strcmp(artefatoPpgAtual, "ALTO") == 0) {
    if (publicarHoldSeSeguro()) return;
    copiarTexto(qualidadeCorrigidaAtual, sizeof(qualidadeCorrigidaAtual), "RUIM_MOV");
    return;
  }

  bool ppgAtualOk =
    estadoPpg == ESTADO_PPG_ATIVO &&
    qualidadeSpo2Ok &&
    bpmFinal > 0 &&
    spo2Final > 0 &&
    ratioOk &&
    pressaoSistolica > 0 &&
    pressaoDiastolica > 0;

  if (!ppgAtualOk) {
    if (publicarHoldSeSeguro()) return;
    copiarTexto(qualidadeCorrigidaAtual, sizeof(qualidadeCorrigidaAtual), "RUIM_PPG");
    return;
  }

  fcCorrigida = suavizarFcParaPublicacao(bpmFinal);
  spo2Corrigida = spo2Final;
  sysCorrigida = fcCorrigida > 0 ? (int)(110.0f + ((float)fcCorrigida * 0.12f) + 0.5f) : pressaoSistolica;
  diaCorrigida = fcCorrigida > 0 ? (int)(70.0f + ((float)fcCorrigida * 0.08f) + 0.5f) : pressaoDiastolica;
  paValida = (fcCorrigida > 0 && spo2Corrigida > 0 && sysCorrigida > 0 && diaCorrigida > 0);

  if (paValida) {
    fcHoldValido = fcCorrigida;
    spo2HoldValido = spo2Corrigida;
    sysHoldValido = sysCorrigida;
    diaHoldValido = diaCorrigida;
    instanteHoldValidoMs = millis();
  }

  copiarTexto(qualidadeCorrigidaAtual, sizeof(qualidadeCorrigidaAtual), paValida ? "OK_ESTAVEL" : "RUIM_PPG");
}

static void atualizarSpO2PressaoEQualidade(float ampIr, float ampRed) {
  qualidadeSpo2Ok = false;
  ratioOk = false;

  if (filtroDC_IR <= 0.0f || filtroDC_RED <= 0.0f || ampIr <= 0.0f) {
    spo2Final = 0; ratioR = 0.0f; estadoPpg = ESTADO_PPG_SINAL_INSTAVEL; return;
  }
  if (ampIr < AC_IR_MIN_QUALIDADE || ampRed < AC_RED_MIN_QUALIDADE) {
    spo2Final = 0; ratioR = 0.0f; estadoPpg = ESTADO_PPG_SINAL_BAIXO; return;
  }

  float componenteIr = ampIr / filtroDC_IR;
  float componenteRed = ampRed / filtroDC_RED;
  if (componenteIr <= 0.0f) { spo2Final = 0; ratioR = 0.0f; estadoPpg = ESTADO_PPG_SINAL_INSTAVEL; return; }

  ratioR = componenteRed / componenteIr;
  ratioOk = (ratioR >= RATIO_MIN_ACEITO && ratioR <= RATIO_MAX_ACEITO);

  int spo2Tmp = (int)(102.0f - (7.0f * ratioR) + 0.5f);
  if (spo2Tmp > 100) spo2Tmp = 100;
  if (spo2Tmp < 0) spo2Tmp = 0;

  if (ratioOk && spo2Aceita(spo2Tmp)) {
    spo2Final = spo2Tmp;
    qualidadeSpo2Ok = (bpmFinal > 0);
    estadoPpg = qualidadeSpo2Ok ? ESTADO_PPG_ATIVO : ESTADO_PPG_SINAL_INSTAVEL;
  } else {
    spo2Final = 0;
    estadoPpg = ESTADO_PPG_SINAL_INSTAVEL;
  }

  if (bpmFinal > 0) {
    pressaoSistolica = (int)(110.0f + ((float)bpmFinal * 0.12f) + 0.5f);
    pressaoDiastolica = (int)(70.0f + ((float)bpmFinal * 0.08f) + 0.5f);
  } else {
    pressaoSistolica = 0; pressaoDiastolica = 0;
  }
}

static void processarAmostraPulseira(long ir, long red) {
  portENTER_CRITICAL(&muxDadosSensor);
  irAtual = ir; redAtual = red;

  if (ir < LIMIAR_DEDO_IR) {
    limparValoresSemContato();
    portEXIT_CRITICAL(&muxDadosSensor);
    return;
  }

  sinalPresente = true;
  estadoUso = ESTADO_USO_USANDO;

  if (!filtroInicializado) {
    filtroDC_IR = (float)ir; filtroDC_RED = (float)red; sinalAnteriorIR = 0.0f; filtroInicializado = true;
  }

  sinalAC_IR = (float)ir - filtroDC_IR;
  filtroDC_IR = (filtroDC_IR * 0.96f) + ((float)ir * 0.04f);
  sinalAC_RED = (float)red - filtroDC_RED;
  filtroDC_RED = (filtroDC_RED * 0.96f) + ((float)red * 0.04f);

  float absIr = fabsf(sinalAC_IR);
  float absRed = fabsf(sinalAC_RED);
  if (absIr > picoAcIrCiclo) picoAcIrCiclo = absIr;
  if (absRed > picoAcRedCiclo) picoAcRedCiclo = absRed;
  adicionarAmostraNoBufferPpgApi(ir);

  if (sinalAnteriorIR < 0.0f && sinalAC_IR >= 0.0f) {
    unsigned long agora = millis();
    if (tempoUltimoBatimento > 0) {
      unsigned long intervalo = agora - tempoUltimoBatimento;
      if (intervalo >= INTERVALO_BATIMENTO_MIN_MS && intervalo <= INTERVALO_BATIMENTO_MAX_MS) {
        float bpmCalculado = 60000.0f / (float)intervalo;
        if (fcAceita(bpmCalculado)) {
          bool saltoMuitoGrande = bpmFinal > 0 && fabsf(bpmCalculado - (float)bpmFinal) > FC_VARIACAO_MAX_BATIMENTO_BPM;

          // V16.12: o sinal no pulso pode gerar cruzamentos falsos e saltos como
          // 75 -> 100/140 bpm em poucos segundos. Quando o salto e muito brusco,
          // nao atualizamos a FC publicada nesta passagem; aguardamos estabilidade.
          if (!saltoMuitoGrande) {
            bpmInstantaneo = bpmCalculado;
            if (bpmFinal <= 0) bpmFinal = (int)(bpmCalculado + 0.5f);
            else bpmFinal = (int)(((float)bpmFinal * FC_SUAVIZACAO_ANTERIOR) + (bpmCalculado * FC_SUAVIZACAO_NOVA) + 0.5f);
            ultimoPicoAcIr = picoAcIrCiclo;
            ultimoPicoAcRed = picoAcRedCiclo;
            atualizarSpO2PressaoEQualidade(ultimoPicoAcIr, ultimoPicoAcRed);
            tempoUltimoBatimento = agora;
          }
        }
      } else if (intervalo > INTERVALO_BATIMENTO_MAX_MS) {
        tempoUltimoBatimento = agora;
      }
    } else {
      tempoUltimoBatimento = agora;
    }
    picoAcIrCiclo = 0.0f;
    picoAcRedCiclo = 0.0f;
  }

  if (tempoUltimoBatimento > 0 && millis() - tempoUltimoBatimento > TEMPO_SEM_BATIMENTO_MS) {
    bpmInstantaneo = 0.0f; bpmFinal = 0; spo2Final = 0; pressaoSistolica = 0; pressaoDiastolica = 0; qualidadeSpo2Ok = false; ratioOk = false; estadoPpg = ESTADO_PPG_SINAL_INSTAVEL;
  } else if (!qualidadeSpo2Ok) {
    estadoPpg = absIr < AC_IR_MIN_QUALIDADE ? ESTADO_PPG_SINAL_BAIXO : ESTADO_PPG_SINAL_INSTAVEL;
  }

  sinalAnteriorIR = sinalAC_IR;
  atualizarFusaoCorrigida();
  portEXIT_CRITICAL(&muxDadosSensor);
}

static void taskSensorPulseira(void *parametro) {
  (void)parametro;
  while (true) {
    atualizarMPU50Hz();
    bloquearI2cCompartilhado();
    long ir = sensorMax.getIR();
    long red = sensorMax.getRed();
    desbloquearI2cCompartilhado();
    processarAmostraPulseira(ir, red);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void iniciarSensorFisiologico() {
  garantirMutexI2cCompartilhado();
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  sensorOk = sensorMax.begin(Wire, I2C_SPEED_FAST, ENDERECO_MAX3010X);
  if (!sensorOk) {
    Serial.println("MAX30105/MAX30102 nao encontrado em 0x57.");
    return;
  }
  Serial.println("MAX30105/MAX30102 encontrado em 0x57.");

  byte brilhoLED = 35;
  byte sampleAverage = 4;   // V15 validada: AVG=4
  byte ledMode = 2;
  byte sampleRate = 100;
  int pulseWidth = 411;
  int adcRange = 4096;
  sensorMax.setup(brilhoLED, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  inicializarMPU();

  portENTER_CRITICAL(&muxDadosSensor);
  limparValoresSemContato();
  portEXIT_CRITICAL(&muxDadosSensor);

  Serial.println("Sensor fisiologico V16.14 iniciado: MAX30102 + MPU6050, transicoes mais rapidas e PPG pos-ACK corrigido.");
}

bool sensorFisiologicoDisponivel() { return sensorOk; }

void iniciarTasksSensorFisiologico() {
  if (!sensorOk) return;
  if (taskSensorHandle == NULL) {
    xTaskCreatePinnedToCore(taskSensorPulseira, "TaskSensorPulseira", 12288, NULL, 3, &taskSensorHandle, 1);
  }
}

long lerIrSensorFisiologico() { long v; portENTER_CRITICAL(&muxDadosSensor); v=irAtual; portEXIT_CRITICAL(&muxDadosSensor); return v; }
long lerRedSensorFisiologico() { long v; portENTER_CRITICAL(&muxDadosSensor); v=redAtual; portEXIT_CRITICAL(&muxDadosSensor); return v; }
bool dedoDetectadoSensorFisiologico() { bool v; portENTER_CRITICAL(&muxDadosSensor); v=sinalPresente; portEXIT_CRITICAL(&muxDadosSensor); return v; }
bool qualidadeSpo2OkSensorFisiologico() { bool v; portENTER_CRITICAL(&muxDadosSensor); v=paValida; portEXIT_CRITICAL(&muxDadosSensor); return v; }

int lerFrequenciaCardiacaExperimental() { int v; portENTER_CRITICAL(&muxDadosSensor); v=paValida?fcCorrigida:0; portEXIT_CRITICAL(&muxDadosSensor); return v; }
float lerBpmInstantaneoExperimental() { float v; bool s; portENTER_CRITICAL(&muxDadosSensor); v=bpmInstantaneo; s=sinalPresente; portEXIT_CRITICAL(&muxDadosSensor); return (s && fcAceita(v)) ? v : 0.0f; }
int lerSpo2Experimental() { int v; portENTER_CRITICAL(&muxDadosSensor); v=paValida?spo2Corrigida:0; portEXIT_CRITICAL(&muxDadosSensor); return v; }
float lerRatioSpo2Experimental() { float v; portENTER_CRITICAL(&muxDadosSensor); v=ratioR; portEXIT_CRITICAL(&muxDadosSensor); return v; }
int lerPressaoSistolicaExperimental() { int v; portENTER_CRITICAL(&muxDadosSensor); v=paValida?sysCorrigida:0; portEXIT_CRITICAL(&muxDadosSensor); return v; }
int lerPressaoDiastolicaExperimental() { int v; portENTER_CRITICAL(&muxDadosSensor); v=paValida?diaCorrigida:0; portEXIT_CRITICAL(&muxDadosSensor); return v; }

float lerDcIrSensorFisiologico() { float v; portENTER_CRITICAL(&muxDadosSensor); v=filtroDC_IR; portEXIT_CRITICAL(&muxDadosSensor); return v; }
float lerAcIrSensorFisiologico() { float v; portENTER_CRITICAL(&muxDadosSensor); v=ultimoPicoAcIr; portEXIT_CRITICAL(&muxDadosSensor); return v; }
float lerDcRedSensorFisiologico() { float v; portENTER_CRITICAL(&muxDadosSensor); v=filtroDC_RED; portEXIT_CRITICAL(&muxDadosSensor); return v; }
float lerAcRedSensorFisiologico() { float v; portENTER_CRITICAL(&muxDadosSensor); v=ultimoPicoAcRed; portEXIT_CRITICAL(&muxDadosSensor); return v; }
float lerSinalOndaPpgExperimental() { float v; portENTER_CRITICAL(&muxDadosSensor); v=sinalAC_IR; portEXIT_CRITICAL(&muxDadosSensor); return v; }

String lerUsoSensorFisiologico() { int v; portENTER_CRITICAL(&muxDadosSensor); v=estadoUso; portEXIT_CRITICAL(&muxDadosSensor); return v == ESTADO_USO_USANDO ? "USANDO" : "SEM_CONTATO"; }
String lerSinalPpgSensorFisiologico() { int v; portENTER_CRITICAL(&muxDadosSensor); v=estadoPpg; portEXIT_CRITICAL(&muxDadosSensor); if(v==ESTADO_PPG_ATIVO) return "PPG_ATIVO"; if(v==ESTADO_PPG_SINAL_BAIXO) return "SINAL_BAIXO"; if(v==ESTADO_PPG_SINAL_INSTAVEL) return "SINAL_INSTAVEL"; return "SEM_CONTATO"; }
bool paValidaSensorFisiologico() { bool v; portENTER_CRITICAL(&muxDadosSensor); v=paValida; portEXIT_CRITICAL(&muxDadosSensor); return v; }
String lerMovimentoSensorFisiologico() { char b[24]; portENTER_CRITICAL(&muxDadosSensor); strncpy(b,movimentoAtual,sizeof(b)); b[sizeof(b)-1]='\0'; portEXIT_CRITICAL(&muxDadosSensor); return String(b); }
String lerArtefatoPpgSensorFisiologico() { char b[10]; portENTER_CRITICAL(&muxDadosSensor); strncpy(b,artefatoPpgAtual,sizeof(b)); b[sizeof(b)-1]='\0'; portEXIT_CRITICAL(&muxDadosSensor); return String(b); }
String lerQualidadeCorrigidaSensorFisiologico() { char b[20]; portENTER_CRITICAL(&muxDadosSensor); strncpy(b,qualidadeCorrigidaAtual,sizeof(b)); b[sizeof(b)-1]='\0'; portEXIT_CRITICAL(&muxDadosSensor); return String(b); }

String lerDebugMpuSensorFisiologico() {
  float gm, gx, dm, dx, da, an;
  bool calib, perfil;
  char mov[24];
  portENTER_CRITICAL(&muxDadosSensor);
  gm = gyroMed;
  gx = gyroMax;
  dm = dmodMed;
  dx = dmodMax;
  da = daccelMed;
  an = angMed;
  calib = calibracaoMpuPersistida;
  perfil = perfilAssistidoV15Persistido;
  strncpy(mov, movimentoAtual, sizeof(mov));
  mov[sizeof(mov)-1] = '\0';
  portEXIT_CRITICAL(&muxDadosSensor);
  String s = "GM:" + String(gm, 2) + ",GX:" + String(gx, 2) + ",DM:" + String(dm, 4) + ",DX:" + String(dx, 4) + ",DA:" + String(da, 4) + ",ANG:" + String(an, 2) + ",CAL:" + String(calib ? "1" : "0") + ",PERFIL:" + String(perfil ? "1" : "0") + ",MOV:" + String(mov);
  return s;
}

String montarJanelaPpgNormalizadaApi(int quantidade) {
  if (quantidade <= 0) quantidade = 32;
  if (quantidade > BUFFER_PPG_API) quantidade = BUFFER_PPG_API;
  uint32_t local[BUFFER_PPG_API];
  int totalLocal = 0, indiceLocal = 0;
  portENTER_CRITICAL(&muxDadosSensor);
  totalLocal = totalPpgApi; indiceLocal = indicePpgApi;
  if (totalLocal >= quantidade) {
    int inicio = indiceLocal - quantidade; if (inicio < 0) inicio += BUFFER_PPG_API;
    for (int i=0;i<quantidade;i++){ int idx=(inicio+i)%BUFFER_PPG_API; local[i]=bufferPpgApi[idx]; }
  }
  portEXIT_CRITICAL(&muxDadosSensor);
  if (totalLocal < quantidade) return "";
  uint32_t minVal=local[0], maxVal=local[0];
  for(int i=1;i<quantidade;i++){ if(local[i]<minVal)minVal=local[i]; if(local[i]>maxVal)maxVal=local[i]; }
  if (maxVal <= minVal) return "";
  String saida="";
  for(int i=0;i<quantidade;i++){
    long normalizado = map((long)local[i], (long)minVal, (long)maxVal, 20, 235);
    if(normalizado<0) normalizado=0; if(normalizado>255) normalizado=255;
    if(i>0) saida += ",";
    saida += String(normalizado);
  }
  return saida;
}
