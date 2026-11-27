# Étude téléchargement emblibrary.com (2026-11-26, relevé dans le navigateur intégré, compte de l'utilisatrice)

## Constats vérifiés
- Boutique Shopify (`embroidery-library-store.myshopify.com`), comptes clients « nouvelle génération » sur `account.emblibrary.com`.
- Connexion : e-mail → code à 6 chiffres reçu par e-mail → page « Enter code ». Pas de mot de passe.
  - Départ : `https://emblibrary.com/customer_authentication/login?return_to=/` → redirige vers `account.emblibrary.com/authentication/login?client_id=afbf7c15-2ce4-4674-8cc8-f75f78b02b23&redirect_uri=/authentication/oauth/authorize?...response_type=code&scope=openid+email+customer-account-api&redirect_uri=https://emblibrary.com/customer_authentication/callback...`
  - E-mail : POST `/authentication/login` (200) → page `/authentication/code?...` ; code : formulaire POST vers `/authentication/code?...` (même requête de paramètres). Corps exacts NON relevés (perdus du tampon).
  - Ensuite OAuth `authorize` → `emblibrary.com/customer_authentication/callback` → retour sur `account.emblibrary.com` avec cookies de session (HttpOnly, non lisibles).
- Jeton d'accès : GET `https://account.emblibrary.com/oauth/token` (avec les cookies) → `{"access_token":"shcat_…"(564 car.), "expires_in":3459}` ≈ 1 h. Durée de vie des cookies de session : NON relevée.
- Liste des achats : POST `https://account.emblibrary.com/customer/api/unstable/graphql`, en-tête `authorization: <access_token>` (sans « Bearer »), ex. :
  `{customer{id orders(first:50,reverse:true,sortKey:PROCESSED_AT){nodes{id name lineItems(first:20){nodes{name sku variantId productId}}}}}}` → testé OK.
- Boutons de téléchargement = extension d'une appli tierce (« emb-digital-assets », appId `gid://shopify/App/249361858561`), serveur `https://assets.emblibrary.com/` (secours `https://assets-fallback.emblibrary.com/`), derrière Cloudflare.
  - Jeton d'extension (4 min, `expiresIn:240`) : même GraphQL, `{uiExtensionSessionToken(appId:"gid://shopify/App/249361858561",configurationRecordId:"gid://shopify/CheckoutProfile/4835508524",registrationUuid:"d3552f83-ade7-4a6a-985a-5baa436664e2",extensionType:ACTIVATED){value expiresIn}}` → testé OK (valeurs lues dans la page « Orders », peuvent changer à chaque version de l'appli).
  - Commande entière : GET `https://assets.emblibrary.com/downloadReq/<shop>/<customerId>/<orderId>/` ; un motif : GET `…/downloadReq/<shop>/<customerId>/<orderId>/<variantId>/` ; en-têtes `Authorization: Bearer <jeton extension>`, `Accept: application/json` → `{"status":"success","downloadUrl":"…"}` (d'après le script de l'extension ; appel direct NON testé : CORS bloque depuis la page, et la copie du jeton vers le PC a été refusée).
  - Puis GET `downloadUrl` (lien direct, le navigateur l'a téléchargé sans autre requête visible ; forme et durée du lien NON relevées).
- Contenu : « Download All Designs » d’une commande → `<commande>.zip` (861 Ko, stocké) contenant `ESPxxxxx-1_<Nom>.zip` (866 Ko, compressé Deflate) contenant 11 formats + PDF, dont `ESPxxxxx-1_<Nom>_JEF.jef` (67 Ko). Donc : ZIP dans ZIP pour la commande ; par motif, probablement le ZIP intérieur seul (non testé).

## Points ouverts à vérifier avant portage
1. Corps exacts des deux envois (e-mail, code) et éventuelle protection anti-robot (Cloudflare / captcha) hors navigateur.
2. Durée de validité de la session (cookies) : combien de temps sans ressaisir de code.
3. Appel `downloadReq` hors navigateur (avec jeton) et forme/durée du `downloadUrl`.
4. Décompression ZIP (Deflate) sur l'ESP32 pour n'extraire que le `.jef`.

## Test « Download Design » (un motif), 2026-11-26 20:35
- Bouton du motif sur la page d’une commande → `ESPxxxxx-1_<Nom>.zip` (866 005 o) = exactement le ZIP intérieur de la commande (11 formats + PDF, `.jef` identique). Le téléchargement motif par motif donne donc toujours un ZIP à décompresser (un niveau au lieu de deux).
- Confirmé sur une commande à 3 motifs : « Download Design » → `ESPxxxxx-1_<Nom>.zip` (840 899 o) = ce seul motif, 11 formats + PDF, dont `_JEF.jef` (83 726 o). Un ZIP par motif, jamais de `.jef` seul.

## Filtrer les nouvelles commandes (testé 2026-11-26)
- `orders(first:50,sortKey:PROCESSED_AT,reverse:true,query:"processed_at:>AAAA-MM-JJ"){nodes{name processedAt lineItems(first:50){nodes{sku variantId}}}}` → renvoie seulement les commandes postérieures à la date. Le filtre par date fonctionne côté site. Pagination par `pageInfo{hasNextPage endCursor}`.

## Image du motif (testé 2026-11-26)
- Pas d'image dans le ZIP (11 formats + PDF des couleurs). L'image est donnée par la liste des commandes (`lineItems…image{url}` ; la page utilise des tailles 128, 308, 407, 634, 643, 831, 1296). Lien public sur cdn.shopify.com, sans connexion : `ESPxxxxx-ALL_643x643.jpg` → 200 image/jpeg 136 276 o ; `_128x128.jpg` → 7 914 o.

## Connexion hors navigateur (script PC, 2026-11-26 ~20:45-21:00)
- Envoi e-mail / code : POST `https://account.emblibrary.com/authentication/login`, JSON `{"email","step":"email"|"code","code"?, "client_id","redirect_uri","region_country"?,"request_token"?}`, en-têtes `X-CSRF-Token` (meta csrf-token de la page) + cookies + `Accept: application/json`. Paramètres = ceux de l'adresse « canonical-login-url » de la page (pas ceux de l'URL authorize). Réponses : `{"nextStep":"code"}` puis `{"nextStep":"redirect","data":{"redirect_uri":"…callback?code=shcac_…"}}`. Avec de mauvais paramètres : 400 `{"server_error":{"id":"default"}}`.
- La page exige `Accept: text/html…` (sinon 406) ; elle me classe `"bot":true` mais répond.
- Deux entrées distinctes : connexion par la boutique (client `afbf7c15…`, retour `emblibrary.com/customer_authentication/callback`) → session boutique seulement (cookie `_shopify_essential` de emblibrary.com), `account.emblibrary.com/oauth/token` répond 401. Il faut se connecter par `https://account.emblibrary.com/orders` (client `3d58bb62-4750-47c7-a9c4-d905db69ac04`, retour `https://account.emblibrary.com/callback`, scope `customer-account-api:full`) — NON encore validé jusqu'au bout.
- Après 4 demandes de connexion en ~20 min : 429 « Verifying your connection… » (page anti-robot), pas de code envoyé. → Sur l'ESP : connexion rare, session réutilisée.
- Durée du blocage : aucune valeur officielle Shopify trouvée (recherche web 2026-11-26). Témoignages : page « Unavailable » avec 59 s de compte à rebours après demandes de code répétées (community.shopify.dev/t/…/34968) ; blocages de plusieurs heures après échecs répétés (Shop Pay). Pas de réglage côté marchand.

## Chaîne complète validée hors navigateur (script PC `emb_test.py`, 2026-11-26 21:15)
- Connexion par `https://account.emblibrary.com/orders` → authorize → POST login email puis code → `https://account.emblibrary.com/callback?code=…` → `/orders`. Cookie de session : `_shopify_essential` sur account.emblibrary.com (expiration annoncée 2027-11-26 ; durée réelle de la session côté serveur NON mesurée).
- `/oauth/token` 200 → GraphQL commandes OK → jeton extension (240 s) OK → `downloadReq/<shop>/<client>/<commande>/<variante>/` 200 `{"status":"success","downloadUrl":"https://assets.emblibrary.com/secure-download/<id>?expires=<epoch>&signature=<sha256 hex>"}` → GET du lien sans cookie ni jeton : 200, ZIP.
- Lien signé valable 15 min (expires = émission + 900 s).
- Relance immédiate avec les cookies enregistrés : aucun e-mail ni code, tout refonctionne.
- Motif « All Sizes » : ZIP de 2,77 Mo, 33 fichiers, un sous-dossier par taille (ESPxxxxx-1/-2/-3), un `.jef` par taille (138 Ko, 97 Ko, 79 Ko).
- Journal complet de chaque échange : `log_<date>/NN.txt` (contient cookies et jetons).
