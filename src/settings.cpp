#include "settings.h"
#include <Preferences.h>

Settings loadSettings() {
  Preferences prefs;  // locale : appelée depuis plusieurs tâches
  prefs.begin("cloud2usb", true);
  Settings s{prefs.getBool("fixedip"), prefs.getString("ssid"), prefs.getString("pass"), prefs.getString("email"), prefs.getString("since"), prefs.getString("site"),
             prefs.getString("ip"), prefs.getString("mask"), prefs.getString("gw")};
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
  prefs.end();
}
