#include "ota.h"
#include <ArduinoOTA.h>

void startOta() {
  ArduinoOTA.setHostname("cloud2usb");
  ArduinoOTA.setMdnsEnabled(false);  // mDNS déjà lancé par le réseau
  ArduinoOTA.begin();
}

void handleOta() { ArduinoOTA.handle(); }
