import io, json, os, sys, zipfile, urllib.request, urllib.parse, http.cookiejar
from datetime import datetime
from zoneinfo import ZoneInfo

DIR = os.path.dirname(os.path.abspath(__file__))
ACC = "https://account.emblibrary.com"
UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0 Safari/537.36"
SD = os.path.join(DIR, "sd", "EMB", "Embf")
ETAT = os.path.join(DIR, "etat.json")
TZ = ZoneInfo("Europe/Paris")
APP = 'appId:"gid://shopify/App/249361858561",configurationRecordId:"gid://shopify/CheckoutProfile/4835508524",registrationUuid:"d3552f83-ade7-4a6a-985a-5baa436664e2",extensionType:ACTIVATED'

cj = http.cookiejar.MozillaCookieJar(os.path.join(DIR, "cookies.txt"))
cj.load(ignore_discard=True, ignore_expires=True)
op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cj))


def get(url, data=None, headers=None):
    r = urllib.request.Request(url, data=data, headers={"User-Agent": UA, "Accept": "application/json", **(headers or {})})
    with op.open(r) as resp:
        return resp.read()


def gql(query, variables=None):
    d = json.loads(get(ACC + "/customer/api/unstable/graphql", json.dumps({"query": query, "variables": variables or {}}).encode(),
                       {"Content-Type": "application/json", "Authorization": token}))
    if d.get("errors"):
        raise SystemExit("Erreur GraphQL : %s" % d["errors"])
    return d["data"]


def num(gid):
    return gid.rsplit("/", 1)[-1]


# Repère : date de la dernière commande entièrement téléchargée (ou date de départ au 1er lancement)
repere = json.load(open(ETAT))["repere"] if os.path.exists(ETAT) else sys.argv[1] + "T00:00:00Z"
jour_repere = datetime.fromisoformat(repere.replace("Z", "+00:00")).astimezone(TZ).replace(hour=0, minute=0, second=0)
print("Repère :", repere)

token = json.loads(get(ACC + "/oauth/token"))["access_token"]

# Commandes depuis le début du jour du repère (pour numéroter les commandes du jour), de la plus ancienne à la plus récente
commandes, apres = [], None
while True:
    d = gql("""query($q:String,$after:String){customer{id orders(first:50,after:$after,sortKey:PROCESSED_AT,query:$q){
               pageInfo{hasNextPage endCursor}
               nodes{id name processedAt lineItems(first:100){nodes{name sku quantity variantId image{url}}}}}}}""",
            {"q": "processed_at:>='%s'" % jour_repere.isoformat(), "after": apres})
    client = num(d["customer"]["id"])
    commandes += d["customer"]["orders"]["nodes"]
    if not d["customer"]["orders"]["pageInfo"]["hasNextPage"]:
        break
    apres = d["customer"]["orders"]["pageInfo"]["endCursor"]
commandes.sort(key=lambda o: o["processedAt"])

rang = {}
for o in commandes:
    jour = datetime.fromisoformat(o["processedAt"].replace("Z", "+00:00")).astimezone(TZ).strftime("%Y %m %d")
    rang[jour] = rang.get(jour, -1) + 1
    if o["processedAt"] <= repere:
        continue
    dossier = os.path.join(SD, "%s %02d" % (jour, rang[jour]))
    os.makedirs(dossier, exist_ok=True)
    print("Commande", o["name"], "->", os.path.relpath(dossier, SD))
    for li in o["lineItems"]["nodes"]:
        if not li["variantId"] or li["quantity"] <= 0:
            print("   ignoré :", li["name"])
            continue
        try:
            ext = gql("{uiExtensionSessionToken(%s){value}}" % APP)["uiExtensionSessionToken"]["value"]
            rep = json.loads(get("https://assets.emblibrary.com/downloadReq/embroidery-library-store.myshopify.com/%s/%s/%s/"
                                 % (client, num(o["id"]), num(li["variantId"])), headers={"Authorization": "Bearer " + ext}))
            z = zipfile.ZipFile(io.BytesIO(get(rep["downloadUrl"], headers={"Accept": "*/*"})))
        except Exception as e:
            print("   À TÉLÉCHARGER À LA MAIN :", li["name"], "(%s)" % e)
            continue
        jefs = [n for n in z.namelist() if n.lower().endswith(".jef")]
        for n in jefs:
            open(os.path.join(dossier, os.path.basename(n)), "wb").write(z.read(n))
        if li["image"]:
            url = li["image"]["url"]
            open(os.path.join(dossier, li["sku"] + os.path.splitext(urllib.parse.urlparse(url).path)[1]), "wb").write(get(url, headers={"Accept": "*/*"}))
        print("   %s : %d .jef%s" % (li["name"], len(jefs), " + image" if li["image"] else ""))
    repere = o["processedAt"]
    json.dump({"repere": repere}, open(ETAT, "w"))

print("Terminé. Repère :", repere)
