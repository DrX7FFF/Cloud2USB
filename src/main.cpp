#include <Arduino.h>
#include <esp_heap_caps.h>
#include "settings.h"
#include <ESPmDNS.h>
#include "web.h"
#include "storage.h"
#include <ArduinoOTA.h>
#include <WiFi.h>

void setup() {
#ifdef TLS_EXTMEM
  heap_caps_malloc_extmem_enable(256);  // réservations dès 256 o en PSRAM : mémoire interne gardée pour la connexion chiffrée (TLS)
#endif
  startStorage();
  loadSettings();
  // Connexion à la box (réseau mémorisé par le pilote Wi-Fi), sinon point d'accès "Cloud2USB"
  WiFi.setHostname(deviceName().c_str());
  WiFi.mode(WIFI_STA);
  if (settings.fixedIp) WiFi.config(settings.ip, settings.gw, settings.mask, settings.gw);
  if (WiFi.begin() == WL_CONNECT_FAILED || WiFi.waitForConnectResult(15000) != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Cloud2USB");
  }
  MDNS.begin(WiFi.getHostname());
  startWeb();
  ArduinoOTA.setMdnsEnabled(false);  // mDNS déjà lancé par le réseau
  ArduinoOTA.begin();  // mise à jour par Wi-Fi depuis PlatformIO (espota)
}

void loop() {
  handleWeb();
  ArduinoOTA.handle();
  handleFtp();
}
