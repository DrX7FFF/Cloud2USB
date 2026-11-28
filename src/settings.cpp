#include "settings.h"
#include <Preferences.h>
#include <esp_mac.h>

Settings settings;

void loadSettings() {
  Preferences prefs;
  prefs.begin("cloud2usb", true);
  settings = {prefs.getBool("fixedip"), prefs.getUInt("ip"), prefs.getUInt("mask"), prefs.getUInt("gw"),
             prefs.getString("email"), prefs.getString("since"), prefs.getString("site"), prefs.getString("name")};
  prefs.end();
}

void saveSettings() {
  Preferences prefs;
  prefs.begin("cloud2usb", false);
  prefs.putString("email", settings.email);
  prefs.putString("since", settings.since);
  prefs.putString("site", settings.site);
  prefs.putBool("fixedip", settings.fixedIp);
  prefs.putUInt("ip", settings.ip);
  prefs.putUInt("mask", settings.mask);
  prefs.putUInt("gw", settings.gw);
  prefs.putString("name", settings.name);
  prefs.end();
}

String deviceName() {
  bool ok = settings.name.length() && settings.name.length() <= 31;
  for (char c : settings.name) ok &= isalnum(c) || c == '-';
  if (ok) return settings.name;
  uint8_t m[6];
  esp_base_mac_addr_get(m);
  char n[17];
  snprintf(n, sizeof n, "Cloud2USB-%02X%02X%02X", m[3], m[4], m[5]);
  return n;
}
