#ifndef CONFIGURACAO_LORA_H
#define CONFIGURACAO_LORA_H

#define FREQUENCIA_LORA 915E6

#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_SS 18
#define LORA_RST 14
#define LORA_DIO0 26

#define LORA_SPREADING_FACTOR 7
#define LORA_SIGNAL_BANDWIDTH 125E3
#define LORA_CODING_RATE 5

// V16.2: potencia reduzida para diminuir pico de corrente no TX LoRa e evitar brownout.
#define LORA_TX_POWER_DBM 14

#endif