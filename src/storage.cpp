#include "storage.h"
#include "card.h"
#include <SPI.h>
#include <USB.h>
#include <USBMSC.h>
#include "tusb.h"
#include <SimpleFTPServer.h>

static USBMSC msc;
static Mode current = Mode::USB;
static FtpServer ftp;

static bool mountCard() {
#ifdef SD_NATIVE
  CARD.setPins(7, 9, 8);  // CLK D8, CMD D10, DAT0 D9 (XIAO) ; DAT3 relié au 3,3 V
  return CARD.begin("/sd", true);
#else
  return SD.begin(10, SPI);
#endif
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  for (uint32_t i = 0; i < bufsize / 512; i++)
    if (!CARD.readRAW((uint8_t *)buffer + i * 512, lba + i)) return -1;
  return bufsize;
}

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  for (uint32_t i = 0; i < bufsize / 512; i++)
    if (!CARD.writeRAW(buffer + i * 512, lba + i)) return -1;
  return bufsize;
}

bool startStorage() {
#ifndef SD_NATIVE
  SPI.begin(12, 13, 11, 10);
#endif
  bool ok = mountCard();
  msc.vendorID("Cloud2US");
  msc.productID("SD");
  msc.productRevision("1.0");
  msc.onRead(onRead);
  msc.onWrite(onWrite);
  msc.isWritable(true);
  msc.mediaPresent(ok);
  msc.begin(ok ? CARD.numSectors() : 0, 512);
  USB.begin();
  // Laisse l'appareil branché reconnaître la clé avant de lancer le reste (5 s max : simple chargeur).
  for (uint32_t t = millis(); !USB && millis() - t < 5000;) delay(10);
  return ok;
}

void setMode(Mode m) {
  current = m;
  if (m == Mode::Network) {
    tud_disconnect();
    CARD.end();  // relit la carte modifiée par l'appareil USB
    mountCard();
    ftp.begin();
  } else {
    ftp.end();
    tud_connect();
  }
}

Mode mode() { return current; }

void handleFtp() { if (current == Mode::Network) ftp.handleFTP(); }
