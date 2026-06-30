#pragma once

#include <Arduino.h>

void iniciarSensorFisiologico();
bool sensorFisiologicoDisponivel();

void iniciarTasksSensorFisiologico();

// Mutex simples para proteger o barramento I2C compartilhado entre MAX3010x e OLED.
// O objetivo e evitar disputa entre a task do sensor e a task de atualizacao da tela.
void bloquearI2cCompartilhado();
void desbloquearI2cCompartilhado();

long lerIrSensorFisiologico();
long lerRedSensorFisiologico();

bool dedoDetectadoSensorFisiologico();
bool qualidadeSpo2OkSensorFisiologico();

int lerFrequenciaCardiacaExperimental();
float lerBpmInstantaneoExperimental();

int lerSpo2Experimental();
float lerRatioSpo2Experimental();

int lerPressaoSistolicaExperimental();
int lerPressaoDiastolicaExperimental();

float lerDcIrSensorFisiologico();
float lerAcIrSensorFisiologico();
float lerDcRedSensorFisiologico();
float lerAcRedSensorFisiologico();
float lerSinalOndaPpgExperimental();

String lerUsoSensorFisiologico();
String lerSinalPpgSensorFisiologico();

// V16 - fusao PPG + MPU6050 baseada no ensaio local V15.
// Valores fisiologicos so devem ser usados quando pa_valida=1.
bool paValidaSensorFisiologico();
String lerMovimentoSensorFisiologico();
String lerArtefatoPpgSensorFisiologico();
String lerQualidadeCorrigidaSensorFisiologico();
String lerDebugMpuSensorFisiologico();

// V16.7: assistente completo V15 unico por BOOT 5s,
// disponiveis a qualquer momento depois que o Aluno iniciou.
bool botaoCalibracaoMpuPressionadoSensorFisiologico();
bool sensorFisiologicoCalibrandoMpu();
int progressoCalibracaoMpuSensorFisiologico();
int segundosRestantesCalibracaoMpuSensorFisiologico();
bool calibracaoMpuPersistidaSensorFisiologico();
bool perfilAssistidoV15PersistidoSensorFisiologico();
bool salvarPerfilAssistidoV15SensorFisiologico(const char *resumo);
bool recalibrarMpuSensorFisiologico(const char *motivo);

String montarJanelaPpgNormalizadaApi(int quantidade);
