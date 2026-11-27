#pragma once
#include <Arduino.h>

struct Settings {
  bool fixedIp;                             // IP statique (ip, mask, gw ; DNS = passerelle), sinon DHCP
  String ssid, pass, email, since, site, ip, mask, gw;  // since : repère emblibrary (AAAA-MM-JJ ou date de la dernière commande) ; site : adresse emblibrary, vide = EMB_SITE
};

Settings loadSettings();
void saveSettings(const Settings &s);
