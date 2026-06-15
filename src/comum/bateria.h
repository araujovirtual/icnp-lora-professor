#ifndef BATERIA_H
#define BATERIA_H

#include <Arduino.h>

void iniciarMonitorBateria();

float lerTensaoBateria();

String textoEnergia();

int estimarPercentualBateria(float tensao);

String statusBateria(float tensao);

#endif