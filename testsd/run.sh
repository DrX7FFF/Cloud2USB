#!/bin/sh
# Flashe l'étape $1 et enregistre la sortie série dans logs/$2.txt
cd "$(dirname "$0")" && mkdir -p logs
~/.platformio/penv/bin/pio run -e step$1 -t upload > logs/$2.flash.txt 2>&1 || { echo "FLASH KO"; exit 1; }
~/.platformio/penv/bin/python - > logs/$2.txt <<'PY'
import serial, time
s = serial.Serial(); s.port='/dev/ttyUSB0'; s.baudrate=115200; s.dtr=False; s.rts=False; s.timeout=1; s.open()
s.rts=True; time.sleep(0.1); s.rts=False
t=time.time()
while time.time()-t < 900:
    l=s.readline().decode(errors='replace')
    if l: print(l, end='', flush=True)
    if 'FIN' in l or 'SD absente' in l: break
PY
sed -n '/^STEP/,$p' logs/$2.txt
