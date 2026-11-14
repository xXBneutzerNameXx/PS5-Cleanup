#!/bin/bash
# Test der freiwilligen QR-Rückmeldung (03.11.2026) - Fehlercodes aus Protokollzeilen, Jailbreak-Kürzel. Läuft in WSL.
export PLAYDEN_WEB=zu   # NEU 1.0.10 (12.11.2026): Playden gilt im Test als geschlossen (zugriff_datei.c d_playden_web)
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; mkdir -p "$K/test/kern/ergebnis" $B
exec > "$K/test/kern/ergebnis/rueckmeldung.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_rueckmeldung.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_rueckmeldung || { echo "FAIL BAU FEHLGESCHLAGEN"; exit 1; }
$B/test_rueckmeldung; RC=$?
date "+%Y-%m-%d %H:%M:%S FERTIG"; exit $RC
