#pragma once
// Pilote de la carte SD : natif (SD_MMC, 1 ligne) si SD_NATIVE, sinon SPI.
#ifdef SD_NATIVE
#include <SD_MMC.h>
#define CARD SD_MMC
#else
#include <SD.h>
#define CARD SD
#endif
