#include "web.h"
#include "settings.h"
#include "storage.h"
#include "emb.h"
#include <WebServer.h>

static WebServer server(80);

static String esc(String v) {
  v.replace("&", "&amp;");
  v.replace("\"", "&quot;");
  v.replace("<", "&lt;");
  return v;
}

static String embSection() {
  Settings s = loadSettings();
  String h = "<h2>Emblibrary</h2>", st = embStatus(), rep = embReport();
  if (st.length()) h += "<p>" + esc(st) + "</p>";
  if (rep.length()) h += "<pre>" + esc(rep) + "</pre>";
  if (embState() == EmbState::WaitCode)
    h += "<form method=post action=/emb/code><p>Code reçu<br><input name=code inputmode=numeric autocomplete=one-time-code></p>"
         "<p><button>Valider</button></p></form>";
  if (embState() != EmbState::Idle) return h;
  h += "<form method=post action=/emb/journal><p>Journal détaillé (Cloud2USB/journal.txt) : " + String(embJournal() ? "activé" : "désactivé") +
       " <button>" + (embJournal() ? "Désactiver" : "Activer") + "</button></p></form>";
  h += mode() == Mode::Network ? "<form method=post action=/emb/start><p><button>Télécharger les nouveaux achats</button></p></form>"
                               : "<p>Téléchargement possible en mode Réseau.</p>";
  return h + "<form method=post action=/emb/save>"
    "<p>E-mail<br><input name=email type=email value=\"" + esc(s.email) + "\"></p>"
    "<p>Depuis (AAAA-MM-JJ, avancé à chaque téléchargement)<br><input name=since value=\"" + esc(s.since) + "\"></p>"
    "<p><button>Enregistrer</button></p></form>";
}

static void pageSettings() {
  Settings s = loadSettings();
  bool idle = embState() == EmbState::Idle;
  String page = "<!doctype html><meta name=viewport content='width=device-width'><title>Cloud2USB</title>";
  if (embState() == EmbState::Running) page += "<meta http-equiv=refresh content=2>";
  page += "<h1>Cloud2USB</h1><p>Mode : " + String(mode() == Mode::USB ? "USB (brodeuse)" : "Réseau (USB coupé, FTP actif)") + "</p>";
  if (idle) page += "<form method=post action=/mode><p><button>Passer en mode " + String(mode() == Mode::USB ? "Réseau" : "USB") + "</button></p></form>";
  page += "<p>FTP (mode Réseau) : ftp://cloud2usb.local, anonyme</p>" + embSection() +
    "<h2>Réglages</h2><form method=post action=/save>"
    "<p>Wi-Fi<br><input name=ssid value=\"" + esc(s.ssid) + "\"></p>"
    "<p>Mot de passe (vide = inchangé)<br><input name=pass type=password></p>"
    "<p>Dossier Drive<br><input name=drive value=\"" + esc(s.driveFolder) + "\"></p>"
    "<p><button>Enregistrer et redémarrer</button></p></form>";
  server.send(200, "text/html; charset=utf-8", page);
}

static void home() {
  server.sendHeader("Location", "/");
  server.send(303);
}

static void embSave() {
  Settings s = loadSettings();
  s.email = server.arg("email");
  s.since = server.arg("since");
  saveSettings(s);
  home();
}

static void embStartPage() {
  if (mode() == Mode::Network) embStart();
  home();
}

static void embJournalPage() {
  embSetJournal(!embJournal());
  home();
}

static void embCodePage() {
  embCode(server.arg("code"));
  home();
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
  if (embState() == EmbState::Idle) setMode(mode() == Mode::USB ? Mode::Network : Mode::USB);  // jamais pendant un téléchargement
  home();
}

void startWeb() {
  server.on("/mode", HTTP_POST, toggleMode);
  server.on("/", HTTP_GET, pageSettings);
  server.on("/save", HTTP_POST, saveAndRestart);
  server.on("/emb/save", HTTP_POST, embSave);
  server.on("/emb/start", HTTP_POST, embStartPage);
  server.on("/emb/code", HTTP_POST, embCodePage);
  server.on("/emb/journal", HTTP_POST, embJournalPage);
  server.begin();
}

void handleWeb() { server.handleClient(); }
