#!/bin/bash
# Fenster der vollständigen Bereinigung auf erfundenen Konsolen (PS5-Test 03.11.2026) - läuft in WSL
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/kompl
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/komplett.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_komplett.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_komplett || { echo BAU FEHLGESCHLAGEN; exit 1; }
RMDA=1 python3 $KK/test/kern/zufall_komplett.py $B/test_komplett $T ${KOMPLETT_ANZAHL:-6000} ${KOMPLETT_START:-1}
RC=$?
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
