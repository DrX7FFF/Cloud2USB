#pragma once

enum class Mode { USB, Network };

// Monte la SD et l'expose en clé USB lecture seule. Renvoie false si pas de SD.
bool startStorage();
void setMode(Mode m);
Mode mode();
