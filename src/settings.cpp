#include "settings.h"
#include <Preferences.h>

Settings loadSettings() {
  Preferences prefs;  // locale : appelée depuis plusieurs tâches
  prefs.begin("cloud2usb", true);
  Settings s{prefs.getString("ssid"), prefs.getString("pass"), prefs.getString("drive"), prefs.getString("email"), prefs.getString("since")};
  prefs.end();
  return s;
}

void saveSettings(const Settings &s) {
  Preferences prefs;
  prefs.begin("cloud2usb", false);
  prefs.putString("ssid", s.ssid);
  prefs.putString("pass", s.pass);
  prefs.putString("drive", s.driveFolder);
  prefs.putString("email", s.email);
  prefs.putString("since", s.since);
  prefs.end();
}
