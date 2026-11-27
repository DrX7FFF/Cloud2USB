#include "emb.h"
#include "settings.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "card.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <vector>
#include "miniz.h"
#include <esp_heap_caps.h>

static const char *ACC = "https://account.emblibrary.com";
static const char *UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0 Safari/537.36";
static const std::vector<std::pair<String, String>> HTML = {{"Accept", "text/html,application/xhtml+xml,*/*;q=0.8"}};
static const char *TMP_ZIP = "/Cloud2USB/tmp.zip";
static const char *ASSETS = "https://assets.emblibrary.com/downloadReq/embroidery-library-store.myshopify.com/";
static const char *EXT_TOKEN = "{uiExtensionSessionToken(appId:\"gid://shopify/App/249361858561\","
  "configurationRecordId:\"gid://shopify/CheckoutProfile/4835508524\","
  "registrationUuid:\"d3552f83-ade7-4a6a-985a-5baa436664e2\",extensionType:ACTIVATED){value}}";
static const char *ORDERS = "query($q:String,$after:String){customer{id orders(first:50,after:$after,sortKey:PROCESSED_AT,query:$q){"
  "pageInfo{hasNextPage endCursor} nodes{id processedAt lineItems(first:100){nodes{name sku quantity variantId image{url}}}}}}}";

// État partagé avec les appelants
static std::mutex mtx;
static EmbState state = EmbState::Idle;
static String status, report, code;
static volatile size_t received;

static void setStatus(const String &s) { std::lock_guard<std::mutex> l(mtx); status = s; }
static void addReport(const String &s) { std::lock_guard<std::mutex> l(mtx); report += s + "\n"; }

// Cookies de session, par hôte, gardés en mémoire de réglages
struct SavedCookie { String host, name, value; };
static std::vector<SavedCookie> jar;

static void loadJar() {
  Preferences p;
  p.begin("emb", true);
  String txt;
  size_t n = p.getBytesLength("jar");
  if (n) {
    char *b = (char *)calloc(n + 1, 1);
    p.getBytes("jar", b, n);
    txt = b;
    free(b);
  }
  p.end();
  jar.clear();
  for (int i = 0, j; i < (int)txt.length(); i = j + 1) {
    j = txt.indexOf('\n', i);
    if (j < 0) j = txt.length();
    String line = txt.substring(i, j);
    int a = line.indexOf(' '), b = line.indexOf('=');
    if (a > 0 && b > a) jar.push_back({line.substring(0, a), line.substring(a + 1, b), line.substring(b + 1)});
  }
}

static void saveJar() {
  String txt;
  for (auto &c : jar) txt += c.host + " " + c.name + "=" + c.value + "\n";
  Preferences p;
  p.begin("emb", false);
  p.putBytes("jar", txt.c_str(), txt.length());
  p.end();
}

static void storeCookie(const String &host, const String &h) {
  int eq = h.indexOf('='), sc = h.indexOf(';');
  if (sc < 0) sc = h.length();
  if (eq <= 0 || eq > sc) return;
  String name = h.substring(0, eq), value = h.substring(eq + 1, sc), low = h;
  name.trim();
  low.toLowerCase();
  for (auto it = jar.begin(); it != jar.end(); ++it)
    if (it->host == host && it->name == name) { jar.erase(it); break; }
  if (value.length() && low.indexOf("max-age=0") < 0 && low.indexOf("01 jan 1970") < 0) jar.push_back({host, name, value});
}

static String cookiesFor(const String &host) {
  String s;
  for (auto &c : jar)
    if (c.host == host) s += (s.length() ? "; " : "") + c.name + "=" + c.value;
  return s;
}

// HTTP(S), redirections suivies, cookies gérés ; corps dans body, ou dans out si fourni (réponse 200)
struct Resp { int code; String body; };

// Écrit la réponse dans un fichier en comptant les octets reçus (avancement)
struct CountStream : Stream {
  File &f;
  CountStream(File &f) : f(f) {}
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t *p, size_t n) override {
    size_t w = f.write(p, n);
    received += w;
    return w;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
};

static String hostOf(const String &url) {
  int a = url.indexOf("://") + 3, b = url.indexOf('/', a);
  return url.substring(a, b < 0 ? url.length() : b);
}

static String join(const String &base, const String &loc) {
  if (loc.startsWith("http")) return loc;
  if (loc.startsWith("/")) return base.substring(0, base.indexOf("://") + 3) + hostOf(base) + loc;
  return base.substring(0, base.lastIndexOf('/') + 1) + loc;
}

// Journal détaillé des échanges (activé depuis l'appelant, non mémorisé) : /Cloud2USB/journal.txt, refait à chaque exécution
static volatile bool journalOn = false;
static File journal;

static void jlog(const String &s) {
  if (journal) journal.print(s);
}

static String memInfo() {
  return "mémoire interne libre " + String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) + " o, plus grand bloc "
       + String(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)) + " o";
}

static Resp request(String url, String method = "GET", String payload = "",
                    const std::vector<std::pair<String, String>> &headers = {}, Stream *out = nullptr) {
  for (int hop = 0; hop < 10; hop++) {
    NetworkClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setReuse(false);
    if (!http.begin(client, url)) return {-1, ""};
    http.setUserAgent(UA);
    http.addHeader("Accept-Language", "en-US,en;q=0.9");  // exigé par le site, comme un navigateur
    http.collectAllHeaders();
    String host = hostOf(url), c = cookiesFor(host);
    if (c.length()) http.addHeader("Cookie", c);
    for (auto &h : headers) http.addHeader(h.first, h.second);
    jlog("\n===== " + method + " " + url + "\n(" + memInfo() + ")\n> User-Agent: " + UA + "\n> Accept-Language: en-US,en;q=0.9\n");
    if (c.length()) jlog("> Cookie: " + c + "\n");
    for (auto &h : headers) jlog("> " + h.first + ": " + h.second + "\n");
    if (payload.length()) jlog("> (corps) " + payload + "\n");
    int code = http.sendRequest(method.c_str(), payload);
    jlog("< " + String(code) + (code < 0 ? " " + http.errorToString(code) : "") + "\n");
    if (code < 0) {
      char e[128];
      client.lastError(e, sizeof e);
      jlog("< TLS : " + String(e) + "\n");
    }
    for (int i = 0; i < http.headers(); i++) {
      jlog("< " + http.headerName(i) + ": " + http.header(i) + "\n");
      if (http.headerName(i).equalsIgnoreCase("Set-Cookie")) storeCookie(host, http.header(i));
    }
    String loc = http.getLocation();
    if (code >= 301 && code <= 308 && loc.length()) {
      url = join(url, loc);
      if (code <= 303) method = "GET", payload = "";
      continue;
    }
    Resp r{code, ""};
    if (code == 200 && out) {
      int w = http.writeToStream(out);
      if (w < 0) r.code = w;
    } else if (code > 0) r.body = http.getString();
    jlog("< (corps, " + String(r.body.length()) + " o)\n" + r.body + "\n");
    return r;
  }
  return {-1, ""};
}

// Compte client
static String token;

static String accessToken() {
  Resp r = request(String(ACC) + "/oauth/token", "GET", "", {{"Accept", "application/json"}});
  JsonDocument d;
  if (r.code != 200 || deserializeJson(d, r.body)) return "";
  return d["access_token"] | "";
}

static bool gql(const String &query, const String &vars, JsonDocument &out) {
  JsonDocument q;
  q["query"] = query;
  q["variables"] = serialized(vars);
  String body;
  serializeJson(q, body);
  Resp r = request(String(ACC) + "/customer/api/unstable/graphql", "POST", body,
                   {{"Content-Type", "application/json"}, {"Accept", "application/json"}, {"Authorization", token}});
  return r.code == 200 && !deserializeJson(out, r.body) && out["errors"].isNull();
}

static String between(const String &s, const String &a, char end) {
  int i = s.indexOf(a);
  if (i < 0) return "";
  i += a.length();
  int j = s.indexOf(end, i);
  return j < 0 ? "" : s.substring(i, j);
}

static String urlDecode(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    if (s[i] == '+') o += ' ';
    else if (s[i] == '%' && i + 2 < s.length()) o += (char)strtol(s.substring(i + 1, i + 3).c_str(), nullptr, 16), i += 2;
    else o += s[i];
  }
  return o;
}

// Connexion : e-mail, puis code reçu par e-mail. Renvoie une erreur, ou "".
static String login(const String &email) {
  setStatus("Connexion au site…");
  jar.clear();
  Resp r = request(String(ACC) + "/orders", "GET", "", HTML);
  String canon = between(r.body, "data-serialized-id=\"canonical-login-url\">", '<');
  String csrf = between(r.body, "name=\"csrf-token\" content=\"", '"');
  JsonDocument d;
  if (canon.isEmpty() || csrf.isEmpty() || deserializeJson(d, canon)) return "page de connexion inattendue";
  String url = join(ACC, d.as<String>());

  JsonDocument p;
  p["email"] = email;
  p["step"] = "email";
  String qs = url.substring(url.indexOf('?') + 1);
  for (int i = 0, j; i < (int)qs.length(); i = j + 1) {
    j = qs.indexOf('&', i);
    if (j < 0) j = qs.length();
    String kv = qs.substring(i, j);
    int eq = kv.indexOf('=');
    String k = urlDecode(kv.substring(0, eq));
    if (eq > 0 && (k == "client_id" || k == "redirect_uri" || k == "region_country" || k == "request_token"))
      p[k] = urlDecode(kv.substring(eq + 1));
  }
  std::vector<std::pair<String, String>> h = {{"Content-Type", "application/json; charset=UTF-8"}, {"Accept", "application/json"},
                                              {"X-CSRF-Token", csrf}, {"Origin", ACC}, {"Referer", url}};
  String body;
  serializeJson(p, body);
  r = request(String(ACC) + "/authentication/login", "POST", body, h);
  if (r.code != 200 || r.body.indexOf("\"nextStep\":\"code\"") < 0) return "e-mail refusé (réponse " + String(r.code) + ")";

  {
    std::lock_guard<std::mutex> l(mtx);
    state = EmbState::WaitCode;
    code = "";
    status = "Code envoyé à " + email;
  }
  String c;
  for (uint32_t t = millis(); c.isEmpty(); delay(200)) {
    if (millis() - t > 600000) return "pas de code saisi en 10 min";
    std::lock_guard<std::mutex> l(mtx);
    c = code;
  }
  {
    std::lock_guard<std::mutex> l(mtx);
    state = EmbState::Running;
    status = "Vérification du code…";
  }

  p["step"] = "code";
  p["code"] = c;
  body = "";
  serializeJson(p, body);
  r = request(String(ACC) + "/authentication/login", "POST", body, h);
  JsonDocument a;
  String redir = r.code == 200 && !deserializeJson(a, r.body) ? (a["data"]["redirect_uri"] | "") : "";
  if (redir.isEmpty()) return "code refusé (réponse " + String(r.code) + ")";
  request(join(ACC, redir), "GET", "", HTML);
  saveJar();
  return "";
}

// Heure de Paris (règles d'heure d'été)
static long days(int y, int m, int d) {
  y -= m <= 2;
  int era = y / 400, yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  return era * 146097L + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

static time_t parseUtc(const String &s) {  // "2026-11-11T19:36:54Z"
  return days(s.substring(0, 4).toInt(), s.substring(5, 7).toInt(), s.substring(8, 10).toInt()) * 86400L
       + s.substring(11, 13).toInt() * 3600 + s.substring(14, 16).toInt() * 60 + s.substring(17, 19).toInt();
}

// ZIP : extrait à plat les .jef dans dir. Renvoie leur nombre, ou -1.
static uint16_t rd16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | (uint32_t)rd16(p + 2) << 16; }

static bool unpack(uint16_t method, const uint8_t *in, size_t cs, uint8_t *out, size_t us) {
  if (method == 0) return cs == us && memcpy(out, in, us);
  if (method != 8) return false;
  tinfl_decompressor *d = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
  if (!d) return false;
  tinfl_init(d);
  size_t n = us;
  tinfl_status s = tinfl_decompress(d, in, &cs, out, out, &n, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  free(d);
  return s == TINFL_STATUS_DONE && n == us;
}

static String why;  // étape du dernier échec d'un motif ou d'une image, pour le compte rendu

static String codeText(int c) { return c < 0 ? HTTPClient::errorToString(c) : "réponse " + String(c); }

// Lecture à une position quelconque. Le pilote SPI de la SD exige des adresses multiples de 4 : on lit depuis un début
// de secteur, par secteurs, dans un tampon aligné, et on ne recopie que la partie utile.
static bool readAt(File &f, size_t pos, uint8_t *buf, size_t len) {
  static uint8_t blk[512] __attribute__((aligned(4)));
  size_t start = pos & ~(size_t)511;
  if (!f.seek(start)) return false;
  for (size_t p = start; p < pos + len; p += 512) {
    size_t n = f.read(blk, 512);
    size_t a = std::max(p, pos), b = std::min(p + n, pos + len);
    if (b > a) memcpy(buf + (a - pos), blk + (a - p), b - a);
    if (n < 512 && p + n < pos + len) return false;
  }
  return true;
}

static int extractJef(File &z, const String &dir) {
  why = "lecture du ZIP";
  size_t n = z.size(), tail = std::min(n, (size_t)65557);
  uint8_t *t = (uint8_t *)ps_malloc(tail);
  long e = -1;
  if (t && readAt(z, n - tail, t, tail))
    for (long i = (long)tail - 22; i >= 0; i--)
      if (rd32(t + i) == 0x06054b50) { e = i; break; }
  uint16_t count = e < 0 ? 0 : rd16(t + e + 10);
  uint32_t cdSize = e < 0 ? 0 : rd32(t + e + 12), cdOff = e < 0 ? 0 : rd32(t + e + 16);
  free(t);
  if (e < 0 || cdOff + cdSize > n) return -1;
  uint8_t *cd = (uint8_t *)ps_malloc(cdSize);
  if (!cd || !readAt(z, cdOff, cd, cdSize)) {
    free(cd);
    return -1;
  }
  int jefs = 0;
  for (uint32_t p = 0; count > 0; count--) {
    if (p + 46 > cdSize || rd32(cd + p) != 0x02014b50) { jefs = -1; break; }
    const uint8_t *c = cd + p;
    uint16_t method = rd16(c + 10), nl = rd16(c + 28);
    uint32_t cs = rd32(c + 20), us = rd32(c + 24), lh = rd32(c + 42);
    String name((const char *)c + 46, nl), low = name;
    p += 46 + nl + rd16(c + 30) + rd16(c + 32);
    low.toLowerCase();
    if (!low.endsWith(".jef")) continue;
    uint8_t h[30];
    if (!readAt(z, lh, h, 30) || rd32(h) != 0x04034b50) { jefs = -1; break; }
    size_t at = lh + 30 + rd16(h + 26) + rd16(h + 28);
    if (at + cs > n || cs == 0 || us == 0) { jefs = -1; break; }
    uint8_t *in = (uint8_t *)ps_malloc(cs), *out = (uint8_t *)ps_malloc(us);
    bool ok = in && out && readAt(z, at, in, cs) && unpack(method, in, cs, out, us);
    free(in);
    if (!ok) {
      free(out);
      why = "décompression";
      jefs = -1;
      break;
    }
    File f = CARD.open(dir + "/" + name.substring(name.lastIndexOf('/') + 1), FILE_WRITE);
    bool opened = f;
    size_t w = opened ? f.write(out, us) : 0;
    f.close();
    free(out);
    if (w != us) {
      why = "écriture SD " + String(w) + "/" + String(us) + " o" + (opened ? "" : " (ouverture)");
      jefs = -1;
      break;
    }
    jefs++;
  }
  free(cd);
  return jefs;
}

// Un motif : lien signé, ZIP, .jef. Renvoie le nombre de .jef, ou -1.
static String num(const String &gid) { return gid.substring(gid.lastIndexOf('/') + 1); }

static int downloadJef(const String &client, const String &order, const String &variant, const String &dir) {
  JsonDocument d;
  why = "jeton";
  if (!gql(EXT_TOKEN, "{}", d)) return -1;
  String ext = d["data"]["uiExtensionSessionToken"]["value"] | "";
  Resp r = request(String(ASSETS) + client + "/" + num(order) + "/" + num(variant) + "/", "GET", "",
                   {{"Authorization", "Bearer " + ext}, {"Accept", "application/json"}});
  JsonDocument j;
  why = "lien (" + codeText(r.code) + ")";
  if (r.code != 200 || deserializeJson(j, r.body)) return -1;
  String url = j["downloadUrl"] | "";
  if (url.isEmpty()) return -1;
  // ZIP reçu dans un fichier temporaire : pas de limite de taille liée à la mémoire
  CARD.mkdir("/Cloud2USB");
  File f = CARD.open(TMP_ZIP, FILE_WRITE);
  if (!f) {
    why = "ouverture du ZIP temporaire";
    return -1;
  }
  CountStream cnt(f);
  received = 0;
  int c = request(url, "GET", "", {{"Accept", "*/*"}}, &cnt).code;
  f.close();
  why = "réception du ZIP (" + codeText(c) + ", " + String(received) + " o)";
  int n = -1;
  if (c == 200) {
    File z = CARD.open(TMP_ZIP, FILE_READ);
    if (z) n = extractJef(z, dir);
    z.close();
  }
  CARD.remove(TMP_ZIP);
  return n;
}

static bool saveImage(const String &url, const String &sku, const String &dir) {
  String path = url.substring(0, url.indexOf('?') < 0 ? url.length() : url.indexOf('?'));
  String file = dir + "/" + sku + path.substring(path.lastIndexOf('.'));
  File f = CARD.open(file, FILE_WRITE);
  int c = f ? request(url, "GET", "", {{"Accept", "*/*"}}, &f).code : 0;
  bool ok = c == 200;
  why = f ? codeText(c) + ", " + String(f.size()) + " o écrits" : String("ouverture du fichier");
  f.close();
  if (!ok) CARD.remove(file);
  return ok;
}

// Synchro : commandes postérieures au repère, un dossier par commande. Renvoie une erreur, ou "".
struct Item { String name, sku, variant, image; int qty; };
struct Order { String id, at; std::vector<Item> items; };

static String syncOrders() {
  Settings s = loadSettings();
  if (s.email.isEmpty() || s.since.isEmpty()) return "e-mail et date de début à renseigner";
  String since = s.since.length() == 10 ? s.since + "T00:00:00Z" : s.since;

  setStatus("Connexion…");
  loadJar();
  token = accessToken();
  if (token.isEmpty()) {
    String e = login(s.email);
    if (e.length()) return e;
    token = accessToken();
    if (token.isEmpty()) return "connexion refusée";
  }

  // Commandes depuis le début du jour du repère (heure de Paris), pour numéroter celles du jour
  time_t t = parseUtc(since);
  struct tm tm;
  localtime_r(&t, &tm);
  int y = tm.tm_year + 1900, m = tm.tm_mon + 1, dd = tm.tm_mday;
  tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
  tm.tm_isdst = -1;
  long offset = (days(y, m, dd) * 86400L - mktime(&tm)) / 3600;
  char q[64];
  snprintf(q, sizeof q, "processed_at:>='%04d-%02d-%02dT00:00:00+%02ld:00'", y, m, dd, offset);

  setStatus("Liste des commandes…");
  std::vector<Order> orders;
  String client, after;
  do {
    JsonDocument v, d;
    v["q"] = q;
    if (after.length()) v["after"] = after;
    String vs;
    serializeJson(v, vs);
    if (!gql(ORDERS, vs, d)) return "liste des commandes illisible";
    JsonObject cu = d["data"]["customer"];
    client = num(cu["id"] | "");
    JsonArray nodes = cu["orders"]["nodes"];
    for (JsonObject o : nodes) {
      Order od{o["id"] | "", o["processedAt"] | "", {}};
      JsonArray items = o["lineItems"]["nodes"];
      for (JsonObject li : items)
        od.items.push_back({li["name"] | "", li["sku"] | "", li["variantId"] | "", li["image"]["url"] | "", li["quantity"] | 0});
      orders.push_back(od);
    }
    after = (cu["orders"]["pageInfo"]["hasNextPage"] | false) ? String(cu["orders"]["pageInfo"]["endCursor"] | "") : String();
  } while (after.length());
  std::sort(orders.begin(), orders.end(), [](const Order &a, const Order &b) { return a.at < b.at; });

  std::map<String, int> rank;
  int newOrders = 0;
  for (auto &o : orders) {
    t = parseUtc(o.at);
    localtime_r(&t, &tm);
    char day[16];
    strftime(day, sizeof day, "%Y %m %d", &tm);
    int r = rank.count(day) ? rank[day] + 1 : 0;
    rank[day] = r;
    if (o.at <= since) continue;
    char name[32];
    snprintf(name, sizeof name, "%s %02d", day, r);
    String dir = String("/EMB/Embf/") + name, manual;
    CARD.mkdir("/EMB");
    CARD.mkdir("/EMB/Embf");
    CARD.mkdir(dir);
    int jefs = 0, imgs = 0;
    for (size_t i = 0; i < o.items.size(); i++) {
      Item &it = o.items[i];
      setStatus(String(name) + " : motif " + (i + 1) + "/" + o.items.size() + " (" + it.name + ")");
      why = "pas de motif à télécharger";
      int n = it.variant.length() && it.qty > 0 ? downloadJef(client, o.id, it.variant, dir) : -1;
      if (n < 0) { manual += "  à télécharger à la main : " + it.name + " [" + why + "]\n"; continue; }
      jefs += n;
      if (it.image.length() && saveImage(it.image, it.sku, dir)) imgs++;
      else if (it.image.length()) manual += "  image non téléchargée : " + it.name + " [" + why + "]\n";
    }
    addReport(String(name) + " : " + jefs + " .jef, " + imgs + " images\n" + manual);
    since = o.at;
    Settings cur = loadSettings();
    cur.since = since;
    saveSettings(cur);
    newOrders++;
  }
  if (!newOrders) addReport("Aucune nouvelle commande.");
  return "";
}

static void run(void *) {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  if (journalOn) {
    CARD.mkdir("/Cloud2USB");
    journal = CARD.open("/Cloud2USB/journal.txt", FILE_WRITE);
    jlog("Début (" + memInfo() + ")\n");
  }
  {  // verrou et texte libérés avant la fin de la tâche
    String e = syncOrders();
    jlog("\n===== Fin : " + (e.isEmpty() ? String("OK") : e) + "\n");
    journal.close();
    std::lock_guard<std::mutex> l(mtx);
    state = EmbState::Idle;
    status = e.isEmpty() ? String("Téléchargement terminé.") : String("Erreur : " + e);
  }
  vTaskDelete(nullptr);
}

void embStart() {
  std::lock_guard<std::mutex> l(mtx);
  if (state != EmbState::Idle) return;
  state = EmbState::Running;
  status = report = "";
  received = 0;
  xTaskCreate(run, "emb", 20480, nullptr, 1, nullptr);
}

void embSetJournal(bool on) { journalOn = on; }
bool embJournal() { return journalOn; }

void embCode(const String &c) {
  std::lock_guard<std::mutex> l(mtx);
  if (state == EmbState::WaitCode) code = c;
}

EmbState embState() {
  std::lock_guard<std::mutex> l(mtx);
  return state;
}

String embStatus() {
  std::lock_guard<std::mutex> l(mtx);
  return state == EmbState::Running && received ? String(status + " — " + String(received / 1024) + " Ko") : status;
}

String embReport() {
  std::lock_guard<std::mutex> l(mtx);
  return report;
}
