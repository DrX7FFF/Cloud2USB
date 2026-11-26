#include "settings.h"
#include <Preferences.h>

static Preferences prefs;

Settings loadSettings() {
  prefs.begin("cloud2usb", true);
  Settings s{prefs.getString("ssid"), prefs.getString("pass"), prefs.getString("drive")};
  prefs.end();
  return s;
}

void saveSettings(const Settings &s) {
  prefs.begin("cloud2usb", false);
  prefs.putString("ssid", s.ssid);
  prefs.putString("pass", s.pass);
  prefs.putString("drive", s.driveFolder);
  prefs.end();
}
