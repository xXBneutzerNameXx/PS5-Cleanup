#!/bin/bash
# PS5 Cleanup 1.3 - Test "Update ersetzen" (app/upd_suche.c: alte Programmdatei auch ohne Nachschauen finden). Läuft in WSL.
K="/pfad/zu/PS5-Cleanup/1.3"
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/update
mkdir -p "$K/test/kern/ergebnis" $B "${T%/*}"
exec > "$K/test/kern/ergebnis/update.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
gcc -O2 -g -Wall -Wextra -DUPD_SUCHE_MAX_BYTE=300000 "$K/app/upd_suche.c" "$K/test/kern/test_upd_suche.c" -o $B/test_upd_suche || { echo "FAIL BAU FEHLGESCHLAGEN"; exit 1; }
rm -rf $T; $B/test_upd_suche $T; RC=$?
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
