#pragma once
#include <Arduino.h>

struct Settings {
  String ssid, pass, driveFolder, email, since;  // since : repère emblibrary (AAAA-MM-JJ ou date de la dernière commande)
};

Settings loadSettings();
void saveSettings(const Settings &s);
