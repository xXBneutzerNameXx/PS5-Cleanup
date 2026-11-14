#!/bin/bash
# PS5 Cleanup 1.0.10 - Test "Rezept lesen" (universelle Erkennung, da_Chris86 11.11.2026). Läuft in WSL.
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/rezept
mkdir -p "$K/test/kern/ergebnis" $B
exec > "$K/test/kern/ergebnis/rezept.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_rezept.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_rezept || { echo "FAIL BAU FEHLGESCHLAGEN"; exit 1; }
rm -rf $T; mkdir -p $T; cp -r $KK/test/ref105/root9 $T/w
python3 -I $KK/test/kern/rezept_baum.py $T/w || { echo "FAIL Testbaum"; exit 1; }
(cd $T; $B/test_rezept $T); RC=$?
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
