#include "config_wifi.h"

#include <Preferences.h>

static Preferences prefs;
static bool prefsIniciada = false;

void iniciarConfigWiFi() {
  if (!prefsIniciada) {
    prefs.begin("wifi_icnp", false);
    prefsIniciada = true;
  }
}

ConfigWiFi carregarConfigWiFi() {
  iniciarConfigWiFi();

  ConfigWiFi cfg;
  cfg.ssid = prefs.getString("ssid", "");
  cfg.senha = prefs.getString("senha", "");
  cfg.configurado = cfg.ssid.length() > 0;

  return cfg;
}

bool salvarConfigWiFi(const String& ssid, const String& senha) {
  iniciarConfigWiFi();

  String ssidLimpo = ssid;
  ssidLimpo.trim();

  if (ssidLimpo.length() == 0) {
    return false;
  }

  prefs.putString("ssid", ssidLimpo);
  prefs.putString("senha", senha);

  return true;
}

void apagarConfigWiFi() {
  iniciarConfigWiFi();
  prefs.remove("ssid");
  prefs.remove("senha");
}
