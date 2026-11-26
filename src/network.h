#pragma once
#include "settings.h"

// Connexion à la box, sinon point d'accès "Cloud2USB". Renvoie l'adresse IP.
IPAddress startNetwork(const Settings &s);
