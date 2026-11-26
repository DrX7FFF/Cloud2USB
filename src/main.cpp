#include <Arduino.h>
#include "settings.h"
#include "network.h"
#include "web.h"
#include "storage.h"
#include "ftp.h"
#include "ota.h"

void setup() {
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
