#pragma once
#include <Arduino.h>

struct Settings {
  String ssid, pass, driveFolder;
};

Settings loadSettings();
void saveSettings(const Settings &s);
