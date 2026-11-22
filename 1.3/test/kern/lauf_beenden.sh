#!/bin/bash
# NEU 1.1 (da_Chris86 15.11.2026 07:11): welche sonst weiterlaufenden Programme beim Löschen mit beendet werden - läuft in WSL
K="/pfad/zu/PS5-Cleanup/1.3"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/beenden
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/beenden.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -w -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_beenden.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_beenden || { echo BAU FEHLGESCHLAGEN; exit 1; }
(cd $T; $B/test_beenden $T)
RC=$?
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
