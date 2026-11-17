#!/bin/bash
export MSYS_NO_PATHCONV=1
cd "$(dirname "$0")"
H="$(pwd)"; E="$H/ergebnis"
exec > "$E/_nachtrag.log" 2>&1
echo "-- Quellcode gegen Tag v1.0.5-beta:"; git -C "$(cygpath -w "$H/../../..")" diff --stat v1.0.5-beta -- Quellcode; echo "(leer = unverändert, rc=$?)"
git -C "$(cygpath -w "$H/../../..")" rev-parse --abbrev-ref HEAD
./Ref105.exe erlaubt pfade.jsonl ergebnis/erlaubt_3004.txt; echo "erlaubt 3004: $(grep -c 1 "$E/erlaubt_3004.txt") erlaubt von $(wc -l < "$E/erlaubt_3004.txt")"
./Ref105.exe erlaubt grenzfaelle.jsonl ergebnis/erlaubt_grenzfaelle.txt; echo "erlaubt Grenzfälle: $(grep -c 1 "$E/erlaubt_grenzfaelle.txt") erlaubt von $(wc -l < "$E/erlaubt_grenzfaelle.txt")"
if cmp -s <(tr -d '\r' < "$E/erlaubt_3004.txt") <(tr -d '\r' < ../../../_test104/erlaubt_cs.txt); then echo "3004 gleich wie _test104/erlaubt_cs.txt"; else echo "3004 ABWEICHUNG zu erlaubt_cs.txt"; fi
