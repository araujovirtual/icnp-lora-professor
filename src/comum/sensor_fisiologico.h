#pragma once

#include <Arduino.h>

void iniciarSensorFisiologico();
bool sensorFisiologicoDisponivel();

long lerIrSensorFisiologico();
long lerRedSensorFisiologico();

bool dedoDetectadoSensorFisiologico();

int lerFrequenciaCardiacaExperimental();
int lerSpo2Experimental();