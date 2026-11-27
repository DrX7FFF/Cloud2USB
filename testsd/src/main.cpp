#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>
#if STEP >= 1
#include <WiFi.h>
#include <Preferences.h>
#endif
#if STEP >= 4
#include <ESPmDNS.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <SimpleFTPServer.h>
static WebServer server(80);
static FtpServer ftp;
#endif

static uint32_t buf[1024], lu[1024];  // 4 Ko, alignés sur 4 octets

static void remplir(uint32_t n, uint32_t pos) {
  for (int i = 0; i < 1024; i++) buf[i] = (pos / 4 + i) * 2654435761u ^ (n << 24);
}

static bool monter() {
  for (int i = 1; i <= 5; i++) {
    if (SD.begin(10, SPI, 4000000)) { Serial.printf("SD montee (essai %d)\n", i); return true; }
    SD.end();
    delay(500);
  }
  return false;
}

static void test() {
#if STEP >= 3
  SD.end();
  Serial.println(SD.begin(10, SPI, 4000000) ? "SD.end/begin OK" : "SD.end/begin KO");
#endif
  Serial.println("fichier | demande | ecrit | ecriture ms | relecture");
  for (uint32_t n = 1; n <= 10; n++) {
    uint32_t taille = 1048576 + esp_random() % (7 * 1048576 + 1);
    char nom[16];
    snprintf(nom, sizeof nom, "/t%02u.bin", (unsigned)n);
    File f = SD.open(nom, FILE_WRITE);
    uint32_t ecrit = 0, t0 = millis();
    while (f && ecrit < taille) {
      remplir(n, ecrit);
      size_t k = min<uint32_t>(sizeof buf, taille - ecrit);
      size_t w = f.write((uint8_t *)buf, k);
      ecrit += w;
      if (w != k) break;
    }
    if (f) f.close();
    uint32_t duree = millis() - t0;

    const char *res = "OK";
    f = SD.open(nom, FILE_READ);
    if (!f) res = "ouverture KO";
    else if (f.size() != ecrit) res = "taille KO";
    else {
      for (uint32_t pos = 0; pos < ecrit; pos += sizeof lu) {
        size_t k = min<uint32_t>(sizeof lu, ecrit - pos);
        remplir(n, pos);
        if (f.read((uint8_t *)lu, k) != k) { res = "lecture KO"; break; }
        if (memcmp(lu, buf, k)) { res = "contenu KO"; break; }
      }
      f.close();
    }
    Serial.printf("%s | %u | %u | %u | %s\n", nom, (unsigned)taille, (unsigned)ecrit, (unsigned)duree, res);
    SD.remove(nom);
  }
  Serial.println("FIN");
}

void setup() {
#if STEP >= 2
  heap_caps_malloc_extmem_enable(256);
#endif
  Serial.begin(115200);
  delay(2000);
  Serial.printf("STEP %d\n", STEP);
  SPI.begin(12, 13, 11, 10);
  if (!monter()) { Serial.println("SD absente"); return; }
#if STEP >= 1
  Preferences p;
  p.begin("cloud2usb", true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(p.getString("ssid").c_str(), p.getString("pass").c_str());
  p.end();
  Serial.println(WiFi.waitForConnectResult(15000) == WL_CONNECTED ? WiFi.localIP().toString() : "WiFi KO");
#endif
#if STEP >= 4
  MDNS.begin("cloud2usb");
  server.on("/", [] { server.send(200, "text/plain", "sdtest"); });
  server.begin();
  ArduinoOTA.setHostname("cloud2usb");
  ArduinoOTA.setMdnsEnabled(false);
  ArduinoOTA.begin();
  ftp.begin();
#endif
#if STEP >= 5
  xTaskCreate([](void *) { test(); vTaskDelete(nullptr); }, "test", 20480, nullptr, 1, nullptr);
#else
  test();
#endif
}

void loop() {
#if STEP >= 4
  server.handleClient();
  ArduinoOTA.handle();
  ftp.handleFTP();
#endif
}
