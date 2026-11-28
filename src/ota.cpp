#include "ota.h"
#include <ArduinoOTA.h>
#include <WiFi.h>

void startOta() {
  ArduinoOTA.setHostname(WiFi.getHostname());
  ArduinoOTA.setMdnsEnabled(false);  // mDNS déjà lancé par le réseau
  ArduinoOTA.begin();
}

void handleOta() { ArduinoOTA.handle(); }
