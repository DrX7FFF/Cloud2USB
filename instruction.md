# Cloud2USB — Instructions pour Claude Code

## Objectif
Un ESP32 branché sur le port USB d'une brodeuse se présente comme une clé USB (mass storage, en lecture/écriture pour le proto ; règles à affiner).
Les fichiers de broderie sont déposés en FTP ou téléchargés depuis emblibrary.com ; la brodeuse les voit sur la "clé".

## Architecture retenue
- Carte SD = stockage local.
- Commutateur à deux modes sur la page web :
  1. **USB** (par défaut) : la SD est exposée en clé USB (mass storage, MSC) à la brodeuse ; en lecture/écriture pour l'appareil branché (brodeuse ou PC) ; FTP coupé ; aucune écriture par l'ESP32 lui-même.
  2. **WiFi** : USB détaché (la brodeuse voit la clé débranchée, le câble reste branché pour l'alimentation), SD relue (remontée) au passage dans ce mode puis en lecture/écriture par l'ESP32, exposée en FTP.
- Règle absolue : jamais d'écriture sur la SD par l'ESP32 pendant qu'elle est exposée en USB.
- Un seul maître à la fois sur la SD (USB ou FTP) : ni lecture seule USB pendant le FTP, ni FTP en lecture seule pendant l'USB pour le POC (cache de l'appareil branché, écritures différées du PC, bibliothèque FTP sans lecture seule).
- Page web servie par l'ESP32, en onglets (une adresse par onglet) : Accueil (mode, adresse FTP, téléchargement emblibrary), Réglage (EmbLibrary : e-mail, adresse du site, « Depuis » ; Expert : journal détaillé), WiFi (adresse MAC ; nom de l'appareil (vide ou invalide = Cloud2USB-XXXXXX, fin de l'adresse MAC) : nom vu par la box et adresse <nom>.local (page, FTP, OTA), pour plusieurs appareils sur le même réseau ; le point d'accès reste « Cloud2USB », il ne sert qu'à la première configuration, un appareil à la fois ; nom du réseau, mot de passe, DHCP ou IP statique : IP, masque, passerelle servant aussi de DNS ; valeurs en cours toujours affichées, champs grisés en DHCP ; IP statique invalide → DHCP) (page maison : WebServer Arduino + Preferences, pas de WiFiManager). Sans Wi-Fi configuré ou joignable : point d'accès « Cloud2USB » ; sinon connexion à la box, page toujours accessible (<nom>.local).
- Accès réseau aux fichiers : FTP (plutôt que WebDAV), pour organiser dossiers et fichiers et renommer depuis Android (« Gestionnaire de fichiers + »), Nautilus ou l'explorateur Windows.
- POC : au plus simple et au plus fiable. Framework Arduino. FTP = `xreef/SimpleFTPServer` (anonyme, port 21), actif uniquement en mode WiFi. Page de réglages et commutateur de mode : WebServer Arduino, port 80. (WebDAV abandonné : ESPWebDAV ne compile pas sur S2 / Arduino 3, `zonque/webdav-server` sans verrouillage et non maintenue.)
- Clé USB : classe Arduino `USBMSC`, lecture/écriture (`isWritable(true)`) ; mode WiFi = `tud_disconnect()`, retour USB = `tud_connect()`. Mode USB au démarrage. Au démarrage, on attend que l'appareil branché ait reconnu la clé (événement USB « démarré », 5 s max pour le cas d'un simple chargeur) avant de lancer Wi-Fi, web, FTP et OTA : la reconnaissance se fait sans concurrence. (La tâche USB a déjà la priorité la plus haute ; l'événement d'éjection est ignoré car la brodeuse ne l'envoie peut-être pas.)
- Partitions : T8 `min_spiffs.csv` (2 × 1,9 Mo pour le programme ; SPIFFS 128 Ko non utilisé) ; XIAO `default_8MB.csv` (par défaut de la carte).
- OTA : ArduinoOTA depuis PlatformIO (`pio run -e ota -t upload`, cible <nom>.local, à reporter dans platformio.ini) ; autre solution (sans PC) à trouver plus tard.
- Heure : NTP à prévoir (sans heure, les fichiers écrits en FTP sont datés du 1er janvier).
- Téléchargement des achats emblibrary.com (étude validée sur PC le 2026-11-26, détail dans `docs/emblibrary/`), en mode WiFi, réglages sur la page web :
  - Connexion : e-mail puis code à 6 chiffres reçu, une seule fois ; la session est ensuite réutilisée (durée réelle de validité non mesurée). Ne pas multiplier les demandes de code (blocage constaté après 4 demandes en ~20 min).
  - Un dossier par commande dans `EMB/Embf/`, nommé `AAAA MM JJ NN` (jour de la commande, heure de Paris ; NN = rang de la commande dans le jour, 00 pour la première).
  - Déjà téléchargé = repère sauvegardé (date de la dernière commande traitée), pas les fichiers présents (tri et effacement libres). Premier lancement : date de départ saisie.
  - Par motif : tous les `.jef` du ZIP (un par taille pour « All Sizes ») + l'image du motif (`<sku>.jpg`).
  - Motif en échec (carte cadeau, erreur) : signalé « à télécharger à la main », ignoré ; le repère avance quand même.
  - ESP32 : ZIP (4,5 Mo et plus constatés pour « All Sizes ») reçu au fil de l'eau dans `Cloud2USB/tmp.zip` sur la SD (pas de limite de taille liée à la mémoire), puis sommaire lu à la fin du fichier, chaque `.jef` chargé seul en mémoire, décompressé avec le décompresseur intégré à la puce (ROM) et écrit ; ZIP temporaire effacé.
  - Mise en œuvre (`emb.cpp`) : tâche de fond ; adresse du site réglable (vide = `https://account.emblibrary.com`) ; à chaque exécution, identifiants de l'extension de téléchargement (`appId`, `configurationRecordId`, `registrationUuid` = `extension.id`), adresse (`url_domain`) et boutique (`shop_name`) lus dans la page `/orders` (balise `extensions-data`, objet `order-oneclick-download`), car ils changent avec les versions de l'appli ; absents → arrêt « page des commandes inattendue » ; certificat du site non vérifié (POC) ; e-mail et repère « Depuis » mémorisés dans les réglages, le repère avance après chaque commande ; cookies de session gardés en mémoire de réglages.
  - Page web : bouton « Télécharger les nouveaux achats » (grisé en mode USB) ; si le site n'accepte plus la session, l'e-mail est envoyé et la page affiche le champ code + « Valider » (10 min max) ; avancement rafraîchi toutes les 2 s ; changement de mode bloqué pendant le téléchargement ; compte rendu (dossiers créés, motifs à télécharger à la main).
  - Journal détaillé des échanges (envoyé / reçu) : activable depuis la page web, non mémorisé (désactivé au démarrage), écrit dans `Cloud2USB/journal.txt` sur la SD, refait à chaque téléchargement ; lisible en FTP. Contient e-mail, code et cookies.
- Écran : affiche l'état de l'appareil (seule source d'information : le port USB est pris par la brodeuse, pas de journal série).

## Matériel : Seeed XIAO ESP32S3 (environnements `xiaos3`, `xiaos3_ota`), carte retenue
- ESP32-S3R8 : 512 Ko SRAM, PSRAM 8 Mo, flash 8 Mo, double cœur. Antenne externe U.FL.
- USB-C relié directement à l'USB natif (GPIO19/20). Réglé en clé USB (TinyUSB), pas en port série : flash par câble en maintenant BOOT au branchement, sinon OTA.
- Carte SD en natif 1 ligne (SD_MMC), module LC Studio : CLK = D8 (GPIO7, broche SCK du module), CMD = D10 (GPIO9, MOSI), DAT0 = D9 (GPIO8, MISO), DAT3 (CS) relié au 3,3 V ; module alimenté en 3V3.
- Choix du S3 : sur le T8 S2, la SD en SPI n'est pas viable avec le Wi-Fi connecté, même sans aucun trafic : écritures arrêtées avant la fin (9 fichiers sur 20 ; 0 sur 20 sans Wi-Fi ; données écrites justes), tests du 2026-11-27 avec `testsd/` (`./run.sh <étape> <nom>`). À refaire sur la XIAO. Mémoire interne du S2 trop juste aussi pour la connexion chiffrée.
- À venir : LilyGO T-Dongle-S3-Plus (format clé, écran).

## Matériel : LilyGO T8 ESP32-S2 (V1.1, écran ST7789 1.14"), environnements `t8s2`, `ota`
- ESP32-S2, 4 Mo flash, PSRAM 8 Mo, mono-cœur.
- USB OTG natif sur GPIO19 (D-) / GPIO20 (D+) : mettre les interrupteurs DIP côté USB-C en position OTG (par défaut reliés au convertisseur USB-série).
- Carte SD (SPI, pas de SDMMC sur le S2) : CS 10, MOSI 11, SCLK 12, MISO 13.
- Écran ST7789 135×240 : CS 34, MOSI 35, SCLK 36, DC 37, RST 38, BL 33.
- Référence de config PlatformIO fonctionnelle (écran) : dépôt `DrX7FFF/ESP32LilygoScreen` (`board = esp32-s2-saola-1`, framework arduino, TFT_eSPI).

## Brodeuse : Elna eXpressive 830 (équivalent Janome MC500E / elna 830E)
- Fichiers lus : `.jef` uniquement.
- Système de fichiers : FAT32.
- Taille max du support : 16 Go → carte SD ≤ 16 Go, ou partition exposée ≤ 16 Go.
- Emplacement des fichiers : `EMB/Embf/` à la racine (`EMB` optionnel).

## Contraintes
- USB Full Speed : ~1 Mo/s max.
- Pilote SPI de la SD (cœur Arduino, T8) : il range les données par mots de 4 octets ; une lecture en milieu de fichier vers une adresse non multiple de 4 fait planter la puce (LoadStoreAlignment, constaté le 2026-11-26). Lectures à position quelconque : depuis un début de secteur, par secteurs de 512 o, via un tampon aligné.
- Le basculement WiFi → USB ne doit pas intervenir pendant une lecture de la brodeuse.

## Acquis (ne pas régresser)
- Journal de debug du cœur : `-DCORE_DEBUG_LEVEL=2` ne suffit pas, la plateforme ajoute `-DCORE_DEBUG_LEVEL=0` après nos options → `build_unflags = -DCORE_DEBUG_LEVEL=0`.

## Points ouverts
- Page emblibrary, à adapter plus tard : après « Valider » le code, la page s'affiche avant que l'ESP ait pris le code et reste sur la saisie (pas de rafraîchissement dans cet état) ; il faut recharger à la main.

## À faire
- Page web : afficher l'état de la carte SD au démarrage (présente / absente), jusqu'ici seulement envoyé au port série (retiré).

## Étapes
1. Wi-Fi + page web de réglages.
2. Carte SD + modes USB / WiFi (commutateur) + FTP.
3. Téléchargement des achats emblibrary.com.
4. Écran d'état.

## Méthode de travail
- Toujours répondre en français.
- Avant toute fonctionnalité ou choix technique : pré-étude courte (contraintes, philosophie des bibliothèques, options d'architecture et compromis), validation, puis implémentation étape par étape.
- Ne faire que ce qui est demandé ; toute modification non demandée est proposée avant d'être faite.
- Code simple, minimal, directement exécutable. Le code fonctionnel ne parle jamais de l'interface qui l'utilise.
