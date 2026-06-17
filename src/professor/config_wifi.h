#ifndef CONFIG_WIFI_H
#define CONFIG_WIFI_H

#include <Arduino.h>

struct ConfigWiFi {
  String ssid;
  String senha;
  bool configurado = false;
};

void iniciarConfigWiFi();
ConfigWiFi carregarConfigWiFi();
bool salvarConfigWiFi(const String& ssid, const String& senha);
void apagarConfigWiFi();

#endif
