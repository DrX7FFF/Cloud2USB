#include "web.h"
#include "settings.h"
#include "storage.h"
#include "emb.h"
#include <WebServer.h>
#include <WiFi.h>

static WebServer server(80);

static String esc(String v) {
  v.replace("&", "&amp;");
  v.replace("\"", "&quot;");
  v.replace("<", "&lt;");
  return v;
}

static String page(const String &tab, const String &body, bool refresh = false) {
  String h = "<!doctype html><meta name=viewport content='width=device-width'><title>Cloud2USB</title>";
  if (refresh) h += "<meta http-equiv=refresh content=2>";
  h += "<style>nav{display:flex;gap:4px}nav a{flex:1;text-align:center;padding:8px;background:#ddd;color:#000;text-decoration:none;border-radius:6px}"
       "nav a.on{background:#1565c0;color:#fff}</style><h1>Cloud2USB</h1><nav>";
  for (auto t : {std::make_pair("/", "Accueil"), std::make_pair("/reglage", "Réglage"), std::make_pair("/wifi", "WiFi")})
    h += String("<a href=") + t.first + (tab == t.first ? " class=on>" : ">") + t.second + "</a>";
  return h + "</nav>" + body;
}

static void pageHome() {
  bool idle = embState() == EmbState::Idle, usb = mode() == Mode::USB;
  String h = "<p>" + String(usb ? "Mode USB (Téléchargement et FTP inactif)" : "Mode WIFI (USB inactif / Téléchargement et FTP actif)") + "</p>";
  if (idle) h += "<form method=post action=/mode><p><button>Passer en mode " + String(usb ? "WiFi" : "USB") + "</button></p></form>";
  h += "<p>FTP (mode WiFi) : ftp://cloud2usb.local, anonyme</p>";
  if (idle) h += "<form method=post action=/emb/start><p><button" + String(usb ? " disabled" : "") + ">Télécharger les nouveaux achats</button></p></form>";
  if (embState() == EmbState::WaitCode)
    h += "<form method=post action=/emb/code><p>Code reçu<br><input name=code inputmode=numeric autocomplete=one-time-code></p>"
         "<p><button>Valider</button></p></form>";
  String st = embStatus(), rep = embReport();
  if (st.length()) h += "<p>" + esc(st) + "</p>";
  if (rep.length()) h += "<pre>" + esc(rep) + "</pre>";
  server.send(200, "text/html; charset=utf-8", page("/", h, embState() == EmbState::Running));
}

static void pageSettings() {
  Settings s = loadSettings();
  String h = embState() != EmbState::Idle ? String("<p>Téléchargement en cours.</p>") :
    "<h2>EmbLibrary</h2><form method=post action=/emb/save>"
    "<p>E-mail<br><input name=email type=email value=\"" + esc(s.email) + "\"></p>"
    "<p>Adresse du site (vide = " + String(EMB_SITE) + ")<br><input name=site placeholder=\"" + EMB_SITE + "\" value=\"" + esc(s.site) + "\"></p>"
    "<p>Depuis (AAAA-MM-JJ, avancé à chaque téléchargement)<br><input name=since value=\"" + esc(s.since) + "\"></p>"
    "<p><button>Enregistrer</button></p></form>"
    "<h2>Expert</h2><form method=post action=/emb/journal><p>Journal détaillé (Cloud2USB/journal.txt) : " + String(embJournal() ? "activé" : "désactivé") +
    " <button>" + (embJournal() ? "Désactiver" : "Activer") + "</button></p></form>";
  server.send(200, "text/html; charset=utf-8", page("/reglage", h));
}

static void pageWifi() {
  Settings s = loadSettings();
  server.send(200, "text/html; charset=utf-8", page("/wifi",
    "<form method=post action=/save>"
    "<p>Nom du réseau<br><input name=ssid value=\"" + esc(s.ssid) + "\"></p>"
    "<p>Mot de passe (vide = inchangé)<br><input name=pass type=password></p>"
    "<p><label><input type=radio name=ipmode value=dhcp onchange=f()" + (s.fixedIp ? "" : " checked") + "> DHCP (IP automatique)</label><br>"
    "<label><input type=radio name=ipmode value=static id=st onchange=f()" + (s.fixedIp ? " checked" : "") + "> IP statique</label></p>"
    "<p>IP<br><input class=ip name=ip value=" + WiFi.localIP().toString() + "></p>"
    "<p>Masque<br><input class=ip name=mask value=" + WiFi.subnetMask().toString() + "></p>"
    "<p>Passerelle (et DNS)<br><input class=ip name=gw value=" + WiFi.gatewayIP().toString() + "></p>"
    "<p><button>Enregistrer et redémarrer</button></p></form>"
    "<script>function f(){for(const e of document.querySelectorAll('.ip'))e.disabled=!st.checked}f()</script>"));
}

static void back(const char *tab) {
  server.sendHeader("Location", tab);
  server.send(303);
}

static void home() { back("/"); }

static void embSave() {
  Settings s = loadSettings();
  s.email = server.arg("email");
  s.site = server.arg("site");
  s.since = server.arg("since");
  saveSettings(s);
  back("/reglage");
}

static void embStartPage() {
  if (mode() == Mode::Network) embStart();
  home();
}

static void embJournalPage() {
  embSetJournal(!embJournal());
  back("/reglage");
}

static void embCodePage() {
  embCode(server.arg("code"));
  home();
}

static void saveAndRestart() {
  Settings s = loadSettings();
  s.ssid = server.arg("ssid");
  if (server.arg("pass").length()) s.pass = server.arg("pass");
  s.fixedIp = server.arg("ipmode") == "static";
  if (s.fixedIp) s.ip = server.arg("ip"), s.mask = server.arg("mask"), s.gw = server.arg("gw");
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
  server.on("/", HTTP_GET, pageHome);
  server.on("/reglage", HTTP_GET, pageSettings);
  server.on("/wifi", HTTP_GET, pageWifi);
  server.on("/save", HTTP_POST, saveAndRestart);
  server.on("/emb/save", HTTP_POST, embSave);
  server.on("/emb/start", HTTP_POST, embStartPage);
  server.on("/emb/code", HTTP_POST, embCodePage);
  server.on("/emb/journal", HTTP_POST, embJournalPage);
  server.begin();
}

void handleWeb() { server.handleClient(); }
