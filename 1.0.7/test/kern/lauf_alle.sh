#!/bin/bash
# Alle Kern-Tests nacheinander (Regression) + Laufwerks-Texte + Bedienabläufe + PS5-Bau der Oberfläche.
# Läuft in WSL. Ergebnis: test/kern/ergebnis/alle_tests.log (Übersicht), Einzelheiten in den einzelnen Logs.
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
E="$K/test/kern/ergebnis"; mkdir -p "$E"; rm -f "$E/alle_tests.fertig"
L="$E/alle_tests.log"
echo "Regression $(date '+%Y-%m-%d %H:%M:%S')" > "$L"
auszug() { grep -a -E "GLEICH|UNTERSCHIED|FEHLERZAHL|ZUSAMMEN|^FAIL|BAU FEHLGESCHLAGEN|\(keine\)|Aufrufe=|GESCHUETZT|Szenarien|Einzahl|^OK" "$1" | grep -v -E "^OK   (  |Farbe|keine unbelegte|KopplungsText|keine deutschen)" | head -120; }
for s in schritt1 schritt2 schritt3 schritt5 schritt6 entscheidungen u16 sicherheit m2 download sm sony selbst bereiche ps4 sauber; do   # ps4: NEU 31.10.2026 (PS4-Schutz, DB-Sicherung); sauber: NEU 01.11.2026 (Bilder/Videos, Spielstände, Reste ohne Kachel); sony: NEU 28.10.2026; selbst: NEU 29.10.2026 (Selbstentfernung inkl. Startordner/Übergabe)
  echo "== $s" >> "$L"
  bash "$KK/test/kern/lauf_$s.sh"
  case $s in schritt3) f="$E/schritt3.log";; *) f="$E/$s.log";; esac
  [ -f "$f" ] && auszug "$f" >> "$L" || echo "(kein Log $f)" >> "$L"
done
echo "== selbstweg (Hintergrundteil, PC-Test)" >> "$L"
bash "$KK/test/start/lauf_selbstweg.sh"; auszug "$E/selbstweg.log" >> "$L"
echo "== Laufwerks-Texte (gegen 1.0.5 laufwerk.out)" >> "$L"
B=$HOME/ps5c-bau/test; mkdir -p $B
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
if gcc -O2 -w -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_konsole.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_konsole; then
  $B/test_konsole > "$E/konsole.out"
  if cmp -s <(tr -d '\r' < $KK/test/ref105/ergebnis/laufwerk.out | head -n -4) <(head -n -1 "$E/konsole.out"); then echo "GLEICH wie 1.0.5 ($(head -n -1 "$E/konsole.out" | wc -l) Zeilen)" >> "$L"; else echo "UNTERSCHIED" >> "$L"; fi
  tail -1 "$E/konsole.out" >> "$L"
else echo "BAU FEHLGESCHLAGEN" >> "$L"; fi
if [ -f "$KK/test/ref105/ergebnis_ablauf/_lauf.log" ] && grep -q FERTIG "$KK/test/ref105/ergebnis_ablauf/_lauf.log"; then
  echo "== Bedienabläufe (Vergleich F)" >> "$L"
  bash "$KK/test/kern/lauf_ablauf.sh"; auszug "$E/ablauf.log" >> "$L"
fi
echo "== PS5-Bau Oberfläche" >> "$L"
bash "$KK/app/bauen.sh" > /dev/null 2>&1
grep -a -E "eboot.elf|keine|FERTIG|error|Fehler|^[0-9]+$" "$K/app/ausgabe/_bauen.log" | head -12 >> "$L"
date "+%Y-%m-%d %H:%M:%S FERTIG" >> "$L"
touch "$E/alle_tests.fertig"
