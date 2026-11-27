#pragma once
#include <Arduino.h>

// Téléchargement des achats emblibrary.com vers EMB/Embf/ (en mode Réseau), en tâche de fond.
enum class EmbState { Idle, Running, WaitCode };

void embStart();                  // lance le téléchargement des commandes postérieures au repère
void embCode(const String &code); // code reçu par e-mail, quand embState() == WaitCode
EmbState embState();
String embStatus();               // avancement, ou résultat de la dernière exécution
String embReport();               // dossiers créés et motifs à télécharger à la main
void embSetJournal(bool on);      // journal détaillé des échanges sur la SD (Cloud2USB/journal.txt), non mémorisé
bool embJournal();
