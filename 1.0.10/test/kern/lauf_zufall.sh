#!/bin/bash
# Zufallstest: 1000 erfundene Konsolen (da_Chris86 02.11.2026) - läuft in WSL
export PLAYDEN_WEB=zu   # NEU 1.0.10 (12.11.2026): Playden gilt im Test als geschlossen (zugriff_datei.c d_playden_web)
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/zuf
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/zufall.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -w -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_zufall.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_zufall || { echo BAU FEHLGESCHLAGEN; exit 1; }
RMDA=1 python3 $KK/test/kern/zufall.py $B/test_zufall $T ${ZUFALL_ANZAHL:-1000} ${ZUFALL_START:-1}
RC=$?   # Codex-Prüfung 40 (6): Rückgabewert weitergeben
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
