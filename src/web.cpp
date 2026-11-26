#include "web.h"
#include "settings.h"
#include "storage.h"
#include <WebServer.h>

static WebServer server(80);

static String esc(String v) {
  v.replace("&", "&amp;");
  v.replace("\"", "&quot;");
  v.replace("<", "&lt;");
  return v;
}

static void pageSettings() {
  Settings s = loadSettings();
  server.send(200, "text/html; charset=utf-8",
    "<!doctype html><meta name=viewport content='width=device-width'><title>Cloud2USB</title>"
    "<h1>Cloud2USB</h1><form method=post action=/mode>"
    "<p>Mode : " + String(mode() == Mode::USB ? "USB (brodeuse)" : "Réseau (USB coupé, FTP actif)") + "</p>"
    "<p><button>Passer en mode " + String(mode() == Mode::USB ? "Réseau" : "USB") + "</button></p></form>"
    "<p>FTP (mode Réseau) : ftp://cloud2usb.local, anonyme</p>"
    "<h2>Réglages</h2><form method=post action=/save>"
    "<p>Wi-Fi<br><input name=ssid value=\"" + esc(s.ssid) + "\"></p>"
    "<p>Mot de passe (vide = inchangé)<br><input name=pass type=password></p>"
    "<p>Dossier Drive<br><input name=drive value=\"" + esc(s.driveFolder) + "\"></p>"
    "<p><button>Enregistrer et redémarrer</button></p></form>");
}

static void saveAndRestart() {
  Settings s = loadSettings();
  s.ssid = server.arg("ssid");
  if (server.arg("pass").length()) s.pass = server.arg("pass");
  s.driveFolder = server.arg("drive");
  saveSettings(s);
  server.send(200, "text/html; charset=utf-8", "Enregistré, redémarrage…");
  delay(500);
  ESP.restart();
}

static void toggleMode() {
  setMode(mode() == Mode::USB ? Mode::Network : Mode::USB);
  server.sendHeader("Location", "/");
  server.send(303);
}

void startWeb() {
  server.on("/mode", HTTP_POST, toggleMode);
  server.on("/", HTTP_GET, pageSettings);
  server.on("/save", HTTP_POST, saveAndRestart);
  server.begin();
}

void handleWeb() { server.handleClient(); }
