#include "settings.h"
#include <Preferences.h>
#include <esp_mac.h>

Settings loadSettings() {
  Preferences prefs;  // locale : appelée depuis plusieurs tâches
  prefs.begin("cloud2usb", true);
  Settings s{prefs.getBool("fixedip"), prefs.getString("ssid"), prefs.getString("pass"), prefs.getString("email"), prefs.getString("since"), prefs.getString("site"),
             prefs.getString("ip"), prefs.getString("mask"), prefs.getString("gw"), prefs.getString("name")};
  prefs.end();
  return s;
}

void saveSettings(const Settings &s) {
  Preferences prefs;
  prefs.begin("cloud2usb", false);
  prefs.putString("ssid", s.ssid);
  prefs.putString("pass", s.pass);
  prefs.putString("email", s.email);
  prefs.putString("since", s.since);
  prefs.putString("site", s.site);
  prefs.putBool("fixedip", s.fixedIp);
  prefs.putString("ip", s.ip);
  prefs.putString("mask", s.mask);
  prefs.putString("gw", s.gw);
  prefs.putString("name", s.name);
  prefs.end();
}

String deviceName(const Settings &s) {
  bool ok = s.name.length() && s.name.length() <= 31;
  for (char c : s.name) ok &= isalnum(c) || c == '-';
  if (ok) return s.name;
  uint8_t m[6];
  esp_base_mac_addr_get(m);
  char n[17];
  snprintf(n, sizeof n, "Cloud2USB-%02X%02X%02X", m[3], m[4], m[5]);
  return n;
}
