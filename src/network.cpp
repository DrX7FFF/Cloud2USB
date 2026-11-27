#include "network.h"
#include <WiFi.h>
#include <ESPmDNS.h>

IPAddress startNetwork(const Settings &s) {
  IPAddress ip;
  if (s.ssid.length()) {
    WiFi.mode(WIFI_STA);
    IPAddress a, m, g;
    if (s.fixedIp && a.fromString(s.ip) && m.fromString(s.mask) && g.fromString(s.gw)) WiFi.config(a, g, m, g);
    WiFi.begin(s.ssid.c_str(), s.pass.c_str());
    if (WiFi.waitForConnectResult(15000) == WL_CONNECTED) ip = WiFi.localIP();
  }
  if (!ip) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Cloud2USB");
    ip = WiFi.softAPIP();
  }
  MDNS.begin("cloud2usb");
  return ip;
}
