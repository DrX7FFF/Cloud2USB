#include <Arduino.h>
#include <esp_heap_caps.h>
#include "settings.h"
#include "network.h"
#include "web.h"
#include "storage.h"
#include "ftp.h"
#include "ota.h"

void setup() {
#ifdef TLS_EXTMEM
  heap_caps_malloc_extmem_enable(256);  // réservations dès 256 o en PSRAM : mémoire interne gardée pour la connexion chiffrée (TLS)
#endif
  Serial.begin(115200);
  Serial.println(startStorage() ? "SD OK" : "SD absente");
  Serial.println(startNetwork(loadSettings()));
  startWeb();
  startOta();
}

void loop() {
  handleWeb();
  handleOta();
  handleFtp();
}
