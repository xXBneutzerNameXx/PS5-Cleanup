#!/bin/bash
# Test "PKG- und ELF-Dateien bei ihrem Spiel bzw. Programm" (PS5-Test 06.11.2026, Befunde 2, 12, 14; da_Chris86 17:47) - läuft in WSL
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/zuo; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/zuordnung.log" 2>&1
set -o pipefail; RC=0
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
# NEU 1.0.10: die echte Bereinigung von Sonys Datenbank (app/spielstand_db.c) - gleiche Schalter wie in lauf_sauber.sh (nur einmal übersetzt)
gcc -O2 -g -Wall -std=gnu11 -D_DEFAULT_SOURCE -c $KK/app/spielstand_db.c -o $B/spielstand_db_zuo.o || { echo BAU FEHLGESCHLAGEN spielstand_db; exit 1; }
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_zuordnung.c $B/spielstand_db_zuo.o -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -lm -o $B/test_zuordnung || { echo BAU FEHLGESCHLAGEN; exit 1; }
# Lauf 0: Fälle auf dem Nachbau (root9) - danach löscht der Test auf diesem Baum
rm -rf $T/w; cp -r $RF/root9 $T/w; python3 -I $KK/test/kern/zuordnung_baum.py $T/w || { echo "FAIL Testbaum"; exit 1; }
rm -rf $T/kz $T/kz_vorher; python3 -I $KK/test/kern/kachel_db_baum.py $T/kz normal || { echo "FAIL Test-Datenbank"; exit 1; }; cp -r $T/kz $T/kz_vorher
(cd $T; RMDA=1 FAKESM=1 PLAYDEN_WEB=zu $B/test_zuordnung $T 0) | grep -v "^LOG:" || RC=1
V=$(python3 -I $KK/test/kern/kachel_db_vergleich.py $T/kz_vorher $T/kz KOPI0000); echo "$V" | sed 's/^/INFO   /'
[ "$(echo "$V" | grep -c '^GLEICH')" = 2 ] && echo "OK   ganzer Ablauf: Sonys Test-Datenbank danach - KOPI00001/KOPI00002 überall weg, jede andere Zeile gleich, beide heil" || { echo "FAIL ganzer Ablauf: Sonys Test-Datenbank"; RC=1; }
# Lauf 2: 2000 PKG-Dateien
rm -rf $T/w2; cp -r $RF/root9 $T/w2; python3 -I $KK/test/kern/zuordnung_baum.py $T/w2 viele || { echo "FAIL Testbaum 2000"; exit 1; }
(cd $T; RMDA=1 FAKESM=1 $B/test_zuordnung $T 2) | grep -v "^LOG:" || RC=1
# Lauf 3 (Codex-Prüfung 96): dieselben 2000 PKG-Dateien, aber höchstens 1000 PKG-Dateien aus Ordnern je Suche
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -DPKG_DATEIEN_MAX=1000 -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_zuordnung.c $B/spielstand_db_zuo.o -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -lm -o $B/test_zuordnung_1000 || { echo BAU FEHLGESCHLAGEN; exit 1; }
(cd $T; RMDA=1 FAKESM=1 $B/test_zuordnung_1000 $T 3) | grep -v "^LOG:" || RC=1
# Lauf 4 (NEU 1.0.9, #145): ProsperoStore mit unlesbarem journal.json - nichts wird herausgelöst (wie bis 1.0.8)
# (Codex-Prüfung 138: gültiger Inhalt hinter einer Verknüpfung, die PS5 Cleanup nie öffnet - ein Lesefehler auch unter root)
rm -rf $T/w2; cp -r $RF/root9 $T/w2; python3 -I $KK/test/kern/zuordnung_baum.py $T/w2 store_kaputt || { echo "FAIL Testbaum Lauf 4"; exit 1; }
(cd $T; RMDA=1 FAKESM=1 $B/test_zuordnung $T 4) | grep -v "^LOG:" || RC=1
# Lauf 5 (Codex-Prüfung 137): ProsperoStore ohne journal.json - auch die Teile von PPSA99713 werden herausgelöst
rm -rf $T/w2; cp -r $RF/root9 $T/w2; python3 -I $KK/test/kern/zuordnung_baum.py $T/w2 store_ohne_journal || { echo "FAIL Testbaum Lauf 5"; exit 1; }
(cd $T; RMDA=1 FAKESM=1 $B/test_zuordnung $T 5) | grep -v "^LOG:" || RC=1
# Lauf 6 (Codex-Prüfung 138): ProsperoStore mit journal.json, das kein JSON ist - ebenfalls nichts herausgelöst
rm -rf $T/w2; cp -r $RF/root9 $T/w2; python3 -I $KK/test/kern/zuordnung_baum.py $T/w2 store_json_kaputt || { echo "FAIL Testbaum Lauf 6"; exit 1; }
(cd $T; RMDA=1 FAKESM=1 $B/test_zuordnung $T 6) | grep -v "^LOG:" || RC=1
rm -rf $T/w2
date "+%Y-%m-%d %H:%M:%S FERTIG"
[ $RC -ne 0 ] && echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"
exit $RC
