#!/bin/bash
# Test "PKG- und ELF-Dateien bei ihrem Spiel bzw. Programm" (PS5-Test 06.11.2026, Befunde 2, 12, 14; da_Chris86 17:47) - läuft in WSL
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/zuo; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/zuordnung.log" 2>&1
set -o pipefail; RC=0
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_zuordnung.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_zuordnung || { echo BAU FEHLGESCHLAGEN; exit 1; }
# Lauf 0: Fälle auf dem Nachbau (root9) - danach löscht der Test auf diesem Baum
rm -rf $T/w; cp -r $RF/root9 $T/w; python3 -I $KK/test/kern/zuordnung_baum.py $T/w || { echo "FAIL Testbaum"; exit 1; }
(cd $T; RMDA=1 FAKESM=1 $B/test_zuordnung $T 0) | grep -v "^LOG:" || RC=1
# Lauf 2: 2000 PKG-Dateien
rm -rf $T/w2; cp -r $RF/root9 $T/w2; python3 -I $KK/test/kern/zuordnung_baum.py $T/w2 viele || { echo "FAIL Testbaum 2000"; exit 1; }
(cd $T; RMDA=1 FAKESM=1 $B/test_zuordnung $T 2) | grep -v "^LOG:" || RC=1
# Lauf 3 (Codex-Prüfung 96): dieselben 2000 PKG-Dateien, aber höchstens 1000 PKG-Dateien aus Ordnern je Suche
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -DPKG_DATEIEN_MAX=1000 -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_zuordnung.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_zuordnung_1000 || { echo BAU FEHLGESCHLAGEN; exit 1; }
(cd $T; RMDA=1 FAKESM=1 $B/test_zuordnung_1000 $T 3) | grep -v "^LOG:" || RC=1
rm -rf $T/w2
date "+%Y-%m-%d %H:%M:%S FERTIG"
[ $RC -ne 0 ] && echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"
exit $RC
