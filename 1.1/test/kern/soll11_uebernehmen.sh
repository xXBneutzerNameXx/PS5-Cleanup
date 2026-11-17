#!/bin/bash
# PS5 Cleanup 1.1 - feste Erwartungen (test/ref11) aus einem geprüften Lauf übernehmen. NUR für gewollte, durchgesehene Änderungen:
# danach "git diff test/ref11" lesen - jede Zeile dort ist eine Verhaltensänderung, die im Commit begründet wird.
#   bash test/kern/soll11_uebernehmen.sh <Ergebnisordner>      (z. B. test/kern/ergebnis oder test/kern/ergebnis_teil, nach einem Lauf)
# Läuft in WSL oder Git-Bash. Übernimmt nur, was der Lauf erzeugt hat; fehlende Dateien werden gemeldet.
E="$1"; P="$(cd "$(dirname "$0")/../.." && pwd)"; R="$P/test/ref11"
[ -d "$E" ] || { echo "Aufruf: soll11_uebernehmen.sh <Ergebnisordner>"; exit 2; }
n=0; fehlt=0
k() { if [ -f "$1" ]; then mkdir -p "$(dirname "$2")"; tr -d '\r' < "$1" > "$2"; n=$((n+1)); else echo "fehlt: $1"; fehlt=$((fehlt+1)); fi; }
# schritt2
k "$E/erlaubt_3004_c.txt" "$R/schritt2/erlaubt_3004.txt"; k "$E/erlaubt_grenz_c.txt" "$R/schritt2/erlaubt_grenzfaelle.txt"
for r in root9 root10w; do k "$E/verknuepfung_${r}_c.out" "$R/schritt2/verknuepfung_$r.out"; done
# schritt3
for art in detail auswahl; do for r in root9 root10w gen1 gen2 download_v1; do for sp in de en; do k "$E/${sp}_${art}_${r}_c.out" "$R/schritt3/${sp}_${art}_${r}.out"; done; done; done
# schritt5 (Namen wie in lauf_schritt5.sh)
for f in "$E"/s5/*_c.out; do [ -f "$f" ] || continue; b=$(basename "$f" _c.out); k "$f" "$R/schritt5/$b.out"; k "$E/s5/${b}_c.baum" "$R/schritt5/$b.baum"; done
# schritt6
for v in v1 v2 v3 v4; do k "$E/s6/${v}_detail_aus.out" "$R/schritt6/de_detail_$v.out"; k "$E/s6/${v}_komplettm2_aus.out" "$R/schritt6/de_komplettm2_$v.out"; k "$E/s6/${v}_komplettm2_aus.baum" "$R/schritt6/de_komplettm2_$v.baum"; done
# ablauf (Bedienabläufe)
for f in "$E"/ablauf/de_*.out "$E"/ablauf/en_*.out; do [ -f "$f" ] || continue; b=$(basename "$f" .out); k "$f" "$R/ablauf/$b.out"; k "$E/ablauf/$b.baum" "$R/ablauf/$b.baum"; done
echo "übernommen: $n Dateien, fehlend: $fehlt"
