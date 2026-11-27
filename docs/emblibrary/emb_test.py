import json, re, time, urllib.request, urllib.parse, http.cookiejar, os

OUT = os.path.dirname(os.path.abspath(__file__))
UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0 Safari/537.36"
ACC = "https://account.emblibrary.com"
CK = os.path.join(OUT, "cookies.txt")
cj = http.cookiejar.MozillaCookieJar(CK)
if os.path.exists(CK):
    cj.load(ignore_discard=True, ignore_expires=True)
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *a):
        return None


op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cj), NoRedirect)
LOG = os.path.join(OUT, "log_" + time.strftime("%Y%m%d_%H%M%S"))
os.makedirs(LOG)
n = 0


def req(url, data=None, headers=None, method=None):
    global n
    h = {"User-Agent": UA, "Accept": "text/html,application/xhtml+xml,*/*;q=0.8", "Accept-Language": "en-US,en;q=0.9"}
    h.update(headers or {})
    while True:
        r = urllib.request.Request(url, data=data, headers=h, method=method)
        try:
            resp = op.open(r)
        except urllib.error.HTTPError as e:
            resp = e
        body = resp.read()
        n += 1
        with open(os.path.join(LOG, "%02d.txt" % n), "wb") as f:
            f.write(("%s %s\n%s\n%s\n" % (method or ("POST" if data else "GET"), url,
                     "\n".join("> %s: %s" % kv for kv in r.header_items()), data.decode() if data else "")).encode())
            f.write(("\n< %s\n%s\n\n" % (resp.status, "\n".join("< %s: %s" % kv for kv in resp.headers.items()))).encode())
            f.write(body)
        loc = resp.headers.get("Location")
        if resp.status in (301, 302, 303, 307, 308) and loc:
            url = urllib.parse.urljoin(url, loc)
            if resp.status in (301, 302, 303):
                data, method = None, None
            continue
        return resp.status, url, body


if not os.path.exists(CK):
    # 1. Page de connexion
    st, url, body = req(ACC + "/orders")
    print("1 page connexion:", st, url.split("?")[0])
    url = urllib.parse.urljoin(ACC, json.loads(re.search(rb'data-serialized-id="canonical-login-url">([^<]+)<', body).group(1)))
    print("  page Sign in:", url.split("?")[0])
    q = dict(urllib.parse.parse_qsl(urllib.parse.urlparse(url).query))
    csrf = re.search(rb'name="csrf-token" content="([^"]+)"', body).group(1).decode()
    base = {k: q[k] for k in ("client_id", "redirect_uri", "region_country", "request_token") if k in q}


    def post(payload):
        st, _, b = req(ACC + "/authentication/login", json.dumps(payload).encode(),
                       {"Content-Type": "application/json; charset=UTF-8", "Accept": "application/json",
                        "X-CSRF-Token": csrf, "Origin": ACC, "Referer": url}, "POST")
        return st, b.decode(errors="replace")


    # 2. E-mail
    email = input("E-mail : ").strip()
    st, b = post({"email": email, "step": "email", **base})
    print("2 envoi e-mail:", st, b[:300])
    if st != 200 or '"nextStep":"code"' not in b:
        raise SystemExit("BLOQUÉ : e-mail refusé (%s), pas de code envoyé. Détail dans %s" % (st, LOG))

    # 3. Code
    code = input("Code reçu : ").strip()
    st, b = post({"email": email, "step": "code", "code": code, **base})
    print("3 envoi code:", st, b[:300])
    redir = json.loads(b)["data"]["redirect_uri"]
    st, url, _ = req(urllib.parse.urljoin(ACC, redir))
    print("4 suite connexion:", st, url.split("?")[0])
    cj.save(ignore_discard=True, ignore_expires=True)

# 5. Cookies de session (nom, domaine, expiration)
for c in cj:
    exp = time.strftime("%Y-%m-%d %H:%M", time.localtime(c.expires)) if c.expires else "fin de session"
    print("   cookie", c.domain, c.name, exp)

# 6. Jeton d'accès
st, _, b = req(ACC + "/oauth/token", headers={"Accept": "application/json"})
print("6 jeton:", st, b[:60])
tok = json.loads(b)["access_token"]


def gql(query):
    st, _, b = req(ACC + "/customer/api/unstable/graphql", json.dumps({"query": query}).encode(),
                   {"Content-Type": "application/json", "Accept": "application/json", "Authorization": tok}, "POST")
    return json.loads(b)


# 7. Dernière commande
d = gql('{customer{id orders(first:1,sortKey:PROCESSED_AT,reverse:true){nodes{id name lineItems(first:1){nodes{name variantId}}}}}}')
cust = d["data"]["customer"]["id"].split("/")[-1]
o = d["data"]["customer"]["orders"]["nodes"][0]
li = o["lineItems"]["nodes"][0]
print("7 commande:", o["name"], "|", li["name"])

# 8. Jeton de l'extension de téléchargement
d = gql('{uiExtensionSessionToken(appId:"gid://shopify/App/249361858561",configurationRecordId:"gid://shopify/CheckoutProfile/4835508524",registrationUuid:"d3552f83-ade7-4a6a-985a-5baa436664e2",extensionType:ACTIVATED){value expiresIn}}')
ext = d["data"]["uiExtensionSessionToken"]["value"]
print("8 jeton extension: ok, expire dans", d["data"]["uiExtensionSessionToken"]["expiresIn"], "s")

# 9. Demande du lien du motif
u = "https://assets.emblibrary.com/downloadReq/embroidery-library-store.myshopify.com/%s/%s/%s/" % (
    cust, o["id"].split("/")[-1], li["variantId"].split("/")[-1])
st, _, b = req(u, headers={"Authorization": "Bearer " + ext, "Accept": "application/json"})
print("9 downloadReq:", st, b[:200])
dl = json.loads(b)["downloadUrl"]
print("   lien:", dl.split("?")[0], "| paramètres:", list(urllib.parse.parse_qs(urllib.parse.urlparse(dl).query)))

# 10. Téléchargement du ZIP (sans cookies ni jeton)
st, _, data = req(dl, headers={"Accept": "*/*"})
f = os.path.join(OUT, "test_motif.zip")
open(f, "wb").write(data)
print("10 ZIP:", st, len(data), "octets ->", f)
