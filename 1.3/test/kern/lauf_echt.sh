#!/bin/bash
# NEU 1.3 (21.11.2026, Prüfstand): echte PS5-Mitschnitte (test/echt/*.txt) mit dem aktuellen Kern nachbauen und die Blöcke mit der
# geprüften Aufstellung (<mitschnitt>.erwartet) vergleichen. Jede Abweichung ist ein FAIL und steht als Unterschied im Log - so fällt sofort
# auf, wenn eine Änderung an der Erkennung auf der echten Konsole etwas anders zuordnet. Mitschnitte ohne .erwartet zählen nicht. Läuft in WSL.
K="${K:-/pfad/zu/PS5-Cleanup/1.3}"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/echt; T=$HOME/ps5c-test/echt
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/echt.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -DMITSCHNITT -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_mitschnitt.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_mitschnitt || { echo BAU FEHLGESCHLAGEN; exit 1; }
RC=0; N=0
for m in "$K"/test/echt/*.txt; do
    [ -f "$m" ] || continue
    e="${m%.txt}.erwartet"; [ -f "$e" ] || { echo "--   $(basename "$m"): keine geprüfte Aufstellung, zählt nicht"; continue; }
    N=$((N+1)); n=$(basename "$m" .txt)
    $B/test_mitschnitt zeige "$m" 1 > "$T/$n.zeige" 2>&1 || { echo "FAIL $n: Nachbau gescheitert"; RC=1; continue; }
    python3 -I "$KK/test/kern/echt_bloecke.py" "$T/$n.zeige" > "$T/$n.ist"
    if diff -u "$e" "$T/$n.ist" > "$T/$n.diff"; then echo "OK   $n: alle $(wc -l < "$e") Blöcke wie auf der Konsole"
    else echo "FAIL $n: Blöcke anders als auf der Konsole"; grep "^[-+][^-+]" "$T/$n.diff"; RC=1; fi
done
[ $N -gt 0 ] || echo "--   kein echter Mitschnitt mit geprüfter Aufstellung vorhanden"
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
