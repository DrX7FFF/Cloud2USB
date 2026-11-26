# Cloud2USB — Instructions pour Claude Code

## Objectif
Un ESP32 branché sur le port USB d'une brodeuse se présente comme une clé USB (mass storage, lecture seule côté brodeuse).
Son contenu est le reflet d'un dossier Google Drive : on dépose les fichiers de broderie sur le Drive depuis un PC, la brodeuse les voit sur la "clé".

## Architecture retenue
- Carte SD = stockage local, copie du dossier Drive.
- Deux modes exclusifs :
  1. **Sync** : USB détaché, SD montée par l'ESP32 (FatFS), synchronisation Drive → SD.
  2. **USB** : SD démontée côté ESP32, exposée en mass storage (MSC) à la brodeuse.
- Passage Sync → USB : démontage SD, puis attache USB (la brodeuse voit un rebranchement de clé).
- Règle absolue : la SD n'est jamais montée par l'ESP32 et exposée en USB en même temps.
- Sync unidirectionnelle Drive → SD, aucune écriture vers le Drive.

## Matériel : LilyGO T8 ESP32-S2 (V1.1, écran ST7789 1.14")
- ESP32-S2, 4 Mo flash, PSRAM 8 Mo (à confirmer sur l'exemplaire), mono-cœur.
- USB OTG natif sur GPIO19 (D-) / GPIO20 (D+) : mettre les interrupteurs DIP côté USB-C en position OTG (par défaut reliés au convertisseur USB-série).
- Carte SD (SPI, pas de SDMMC sur le S2) : CS 10, MOSI 11, SCLK 12, MISO 13.
- Écran ST7789 135×240 : CS 34, MOSI 35, SCLK 36, DC 37, RST 38, BL 33.
- Référence de config PlatformIO fonctionnelle (écran) : dépôt `DrX7FFF/ESP32LilygoScreen` (`board = esp32-s2-saola-1`, framework arduino, TFT_eSPI).

## Contraintes
- USB Full Speed : ~1 Mo/s max.
- Format SD lisible par la brodeuse (FAT32, voire FAT16 ou petite partition) : dépend du modèle.
- Le basculement Sync → USB ne doit pas intervenir pendant une lecture de la brodeuse.

## Points ouverts
- Modèle de brodeuse et ses contraintes (taille max de clé, système de fichiers, formats de fichiers, détection à chaud).
- Méthode d'authentification Google Drive sur l'ESP32.
- Déclenchement de la sync (périodique, bouton, détection de changement).
- Rôle de l'écran (état, dernière sync, erreurs).

## Méthode de travail
- Toujours répondre en français.
- Avant toute fonctionnalité ou choix technique : pré-étude courte (contraintes, philosophie des bibliothèques, options d'architecture et compromis), validation, puis implémentation étape par étape.
- Ne faire que ce qui est demandé ; toute modification non demandée est proposée avant d'être faite.
- Code simple, minimal, directement exécutable. Le code fonctionnel ne parle jamais de l'interface qui l'utilise.
