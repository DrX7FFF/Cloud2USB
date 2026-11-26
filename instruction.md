# Cloud2USB — Instructions pour Claude Code

## Objectif
Un ESP32 branché sur le port USB d'une brodeuse se présente comme une clé USB (mass storage, en lecture/écriture pour le proto ; règles à affiner).
Son contenu est le reflet d'un dossier Google Drive : on dépose les fichiers de broderie sur le Drive depuis un PC, la brodeuse les voit sur la "clé".

## Architecture retenue
- Carte SD = stockage local, copie du dossier Drive.
- Commutateur à deux modes sur la page web :
  1. **USB** (par défaut) : la SD est exposée en clé USB (mass storage, MSC) à la brodeuse ; en lecture/écriture pour l'appareil branché (brodeuse ou PC) ; FTP coupé ; aucune écriture par l'ESP32 lui-même.
  2. **Réseau** : USB détaché (la brodeuse voit la clé débranchée, le câble reste branché pour l'alimentation), SD relue (remontée) au passage dans ce mode puis en lecture/écriture par l'ESP32, exposée en FTP ; c'est dans ce mode que se feront l'éventuelle synchro et le reste.
- Règle absolue : jamais d'écriture sur la SD par l'ESP32 pendant qu'elle est exposée en USB.
- Un seul maître à la fois sur la SD (USB ou FTP) : ni lecture seule USB pendant le FTP, ni FTP en lecture seule pendant l'USB pour le POC (cache de l'appareil branché, écritures différées du PC, bibliothèque FTP sans lecture seule).
- Synchro Drive → SD : peut-être abandonnée ; si elle existe, unidirectionnelle, en mode Réseau.
- Réglages (Wi-Fi, dossier Drive) depuis une page web servie par l'ESP32 (page maison : WebServer Arduino + Preferences, pas de WiFiManager). Sans Wi-Fi configuré ou joignable : point d'accès « Cloud2USB » ; sinon connexion à la box, page toujours accessible (cloud2usb.local).
- Accès réseau aux fichiers : FTP (plutôt que WebDAV), pour organiser dossiers et fichiers et renommer depuis Android (« Gestionnaire de fichiers + »), Nautilus ou l'explorateur Windows.
- POC : au plus simple et au plus fiable. Framework Arduino. FTP = `xreef/SimpleFTPServer` (anonyme, port 21), actif uniquement en mode Réseau. Page de réglages et commutateur de mode : WebServer Arduino, port 80. (WebDAV abandonné : ESPWebDAV ne compile pas sur S2 / Arduino 3, `zonque/webdav-server` sans verrouillage et non maintenue.)
- Clé USB : classe Arduino `USBMSC`, lecture/écriture (`isWritable(true)`) ; mode Réseau = `tud_disconnect()`, retour USB = `tud_connect()`. Mode USB au démarrage. Au démarrage, on attend que l'appareil branché ait reconnu la clé (événement USB « démarré », 5 s max pour le cas d'un simple chargeur) avant de lancer Wi-Fi, web, FTP et OTA : la reconnaissance se fait sans concurrence. (La tâche USB a déjà la priorité la plus haute ; l'événement d'éjection est ignoré car la brodeuse ne l'envoie peut-être pas.)
- Partitions : `min_spiffs.csv` (2 × 1,9 Mo pour le programme ; SPIFFS 128 Ko non utilisé).
- OTA : ArduinoOTA depuis PlatformIO (`pio run -e ota -t upload`, cible cloud2usb.local) ; autre solution (sans PC) à trouver plus tard.
- Heure : NTP à prévoir (sans heure, les fichiers écrits en FTP sont datés du 1er janvier).
- Écran : affiche l'état de l'appareil (seule source d'information : le port USB est pris par la brodeuse, pas de journal série).

## Matériel : LilyGO T8 ESP32-S2 (V1.1, écran ST7789 1.14")
- ESP32-S2, 4 Mo flash, PSRAM 8 Mo, mono-cœur.
- USB OTG natif sur GPIO19 (D-) / GPIO20 (D+) : mettre les interrupteurs DIP côté USB-C en position OTG (par défaut reliés au convertisseur USB-série).
- Carte SD (SPI, pas de SDMMC sur le S2) : CS 10, MOSI 11, SCLK 12, MISO 13.
- Écran ST7789 135×240 : CS 34, MOSI 35, SCLK 36, DC 37, RST 38, BL 33.
- Référence de config PlatformIO fonctionnelle (écran) : dépôt `DrX7FFF/ESP32LilygoScreen` (`board = esp32-s2-saola-1`, framework arduino, TFT_eSPI).

## Brodeuse : Elna eXpressive 830 (équivalent Janome MC500E / elna 830E)
- Fichiers lus : `.jef` uniquement → la sync ne copie que les `.jef`.
- Système de fichiers : FAT32.
- Taille max du support : 16 Go → carte SD ≤ 16 Go, ou partition exposée ≤ 16 Go.
- Emplacement des fichiers : `EMB/Embf/` à la racine (`EMB` optionnel).

## Contraintes
- USB Full Speed : ~1 Mo/s max.
- Le basculement Sync → USB ne doit pas intervenir pendant une lecture de la brodeuse.

## Points ouverts
- Authentification Google Drive : reportée ; d'abord une simulation du Drive.
- Bouton de synchro : sur la page web pour le proto (toutes les commandes sont sur la page web).

## Étapes
1. Wi-Fi + page web de réglages.
2. Carte SD + modes USB / Réseau (commutateur) + FTP.
3. Synchro éventuelle (Drive simulé, puis vrai Drive).
4. Écran d'état.

## Méthode de travail
- Toujours répondre en français.
- Avant toute fonctionnalité ou choix technique : pré-étude courte (contraintes, philosophie des bibliothèques, options d'architecture et compromis), validation, puis implémentation étape par étape.
- Ne faire que ce qui est demandé ; toute modification non demandée est proposée avant d'être faite.
- Code simple, minimal, directement exécutable. Le code fonctionnel ne parle jamais de l'interface qui l'utilise.
