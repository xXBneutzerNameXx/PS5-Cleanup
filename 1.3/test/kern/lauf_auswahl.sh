#!/bin/bash
# NEU 1.3 (21.11.2026): Löschliste je Auswahlweg - einzeln, Block-Haken, ganzer Bereich, Alles bereinigen (test_auswahl.c) - läuft in WSL
K="${K:-/pfad/zu/PS5-Cleanup/1.3}"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/auswahl; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/auswahl.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_auswahl.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_auswahl || { echo BAU FEHLGESCHLAGEN; exit 1; }
# Baum wie test_zuordnung (Lauf 0): Nachbau root9 + viele Programme, Kacheln, Spieldateien, Playden, erkannte Ordner, Sicherungen
rm -rf $T/w; cp -r $RF/root9 $T/w; python3 -I $KK/test/kern/zuordnung_baum.py $T/w || { echo "FAIL Testbaum"; exit 1; }
(cd $T; RMDA=1 FAKESM=1 PLAYDEN_WEB=zu $B/test_auswahl $T) | grep -v "^LOG:"
RC=${PIPESTATUS[0]}
# Gegenprobe: fehlt der Prüfung ein gewählter Eintrag, muss sie anschlagen - sonst prüft der Test nichts
G=$(cd $T; GEGENPROBE=1 RMDA=1 FAKESM=1 PLAYDEN_WEB=zu $B/test_auswahl $T 2>&1 | grep -c "^FAIL .*liegt in keinem gewählten Eintrag")
[ "$G" -gt 100 ] && echo "OK   Gegenprobe: ohne den ersten gewählten Eintrag schlägt die Prüfung $G Mal an" || { echo "FAIL Gegenprobe: Prüfung schlägt nur $G Mal an"; RC=1; }
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
