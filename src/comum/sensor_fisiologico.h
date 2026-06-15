#pragma once

#include <Arduino.h>

void iniciarSensorFisiologico();
bool sensorFisiologicoDisponivel();

void iniciarTasksSensorFisiologico();

long lerIrSensorFisiologico();
long lerRedSensorFisiologico();

bool dedoDetectadoSensorFisiologico();
bool qualidadeSpo2OkSensorFisiologico();

int lerFrequenciaCardiacaExperimental();
float lerBpmInstantaneoExperimental();

int lerSpo2Experimental();
float lerRatioSpo2Experimental();

float lerDcIrSensorFisiologico();
float lerAcIrSensorFisiologico();
float lerDcRedSensorFisiologico();
float lerAcRedSensorFisiologico();