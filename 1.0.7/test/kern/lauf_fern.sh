#!/bin/bash
# NEU 1.0.9: Test der Bedienung im Browser (app/fern.c) - Schutz, Zustand, Liste, Protokoll, Befehle, Bilder, Grenzen. Läuft in WSL, nur über 127.x.
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; mkdir -p "$K/test/kern/ergebnis" $B
exec > "$K/test/kern/ergebnis/fern.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
cd "$KK/app" || exit 1   # fern.c bindet fern_seite.html über den Assembler ein (Pfad relativ zu app/)
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -I$KK/bau/cjson \
   $KK/test/kern/test_fern.c $KK/bau/cjson/cJSON.c -lpthread -o $B/test_fern || { echo "FAIL BAU FEHLGESCHLAGEN"; exit 1; }
timeout 120 $B/test_fern; RC=$?
date "+%Y-%m-%d %H:%M:%S FERTIG"; exit $RC
