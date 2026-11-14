#!/bin/bash
# 27.10.2026: nur die betroffenen Tests (WSL) - Gesamttest danach mit lauf_alle.sh
K="/pfad/zu/PS5-Cleanup/1.0.10"; E="$K/test/kern/ergebnis"; L="$E/ziel.log"
echo "Zieltests $(date '+%H:%M:%S')" > "$L"
for s in ${ZIEL:-sm m2 download sicherheit schritt6}; do
  echo "== $s" >> "$L"; bash "$K/test/kern/lauf_$s.sh" > /dev/null 2>&1
  grep -a -E "^FAIL|FEHLERZAHL|BAU FEHLGESCHLAGEN|UNTERSCHIED" "$E/$s.log" | sort | uniq -c >> "$L"
done
date "+%H:%M:%S FERTIG" >> "$L"
