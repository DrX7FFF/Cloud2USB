# Algorithme de synchro emblibrary validé sur PC (2026-11-26) — `emb_sync.py` + connexion `emb_test.py` (dans ce dossier)

Décisions de l'utilisatrice :
- Un dossier par commande dans `EMB/Embf/`, nommé `AAAA MM JJ NN` (jour de la commande, heure de Paris ; NN = rang de la commande dans le jour, 00 pour la première). La brodeuse lit les sous-dossiers.
- Ce qui est déjà téléchargé = un repère sauvegardé (date `processedAt` de la dernière commande traitée), pas les fichiers présents (l'utilisatrice peut trier/effacer). 1er lancement : date de départ saisie.
- On copie tous les `.jef` du ZIP (un par taille pour les motifs « All Sizes ») + l'image du motif (`<sku>.jpg`, URL publique `lineItems.image.url`, pleine taille ~270 Ko).
- Motif en échec (carte cadeau, erreur) : signalé « à télécharger à la main », ignoré, et le repère avance quand même.

Séquence (session déjà ouverte) :
1. GET `https://account.emblibrary.com/oauth/token` (cookie de session) → `access_token` (~1 h).
2. GraphQL `orders(first:50,after,sortKey:PROCESSED_AT,query:"processed_at:>='<début du jour du repère>'")` → commandes + `lineItems{name sku quantity variantId image{url}}` ; tri croissant ; numérotation NN par jour ; on saute celles `<= repère`.
3. Par motif (variantId non nul, quantité > 0) : jeton extension (240 s) → `downloadReq/<shop>/<client>/<commande>/<variante>/` → lien signé 15 min → ZIP → extraire `*.jef` à plat dans le dossier de la commande ; + image.
4. Fin de commande → repère = `processedAt` de la commande, sauvegardé.

Résultat du test : 2 commandes téléchargées (21 .jef, 10 images, 2,4 Mo) ; relance immédiate : rien de nouveau. ZIP par motif jusqu'à 2,8 Mo (décompression Deflate nécessaire sur l'ESP).
Connexion (une fois, puis session réutilisée) : voir `emblibrary-etude.md` § « Chaîne complète validée ». Ne pas multiplier les demandes de code (blocage 429 constaté après 4 demandes en ~20 min).
