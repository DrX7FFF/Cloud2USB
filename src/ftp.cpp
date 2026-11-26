#include "ftp.h"
#include "storage.h"
#include <SimpleFTPServer.h>

static FtpServer ftp;
static bool running = false;

void handleFtp() {
  bool wanted = mode() == Mode::Network;
  if (wanted != running) {
    if (wanted) ftp.begin();
    else ftp.end();
    running = wanted;
  }
  if (running) ftp.handleFTP();
}
