#pragma once
#include <Arduino.h>
#include <IPAddress.h>

struct Settings {
  bool fixedIp;                             // IP statique (ip, mask, gw ; DNS = passerelle), sinon DHCP
  IPAddress ip, mask, gw;
  String email, since, site, name;  // name : nom de l'appareil (box, <name>.local), vide = Cloud2USB-XXXXXX ; since : repère emblibrary (AAAA-MM-JJ ou date de la dernière commande) ; site : adresse emblibrary, vide = EMB_SITE
};

extern Settings settings;  // chargés au démarrage, modifiés directement puis enregistrés par saveSettings()

void loadSettings();
void saveSettings();
String deviceName();  // name s'il est valide, sinon Cloud2USB- + fin de l'adresse MAC
