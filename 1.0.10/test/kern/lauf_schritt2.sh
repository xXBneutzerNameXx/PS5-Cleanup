#!/bin/bash
# 1.0.7: Vergleich Eintrag für Eintrag mit 1.0.5 - Bereiche wie 1.0.6 (6 Bereiche); die neue Einteilung prüfen test_download/test_sony/test_bereiche
export PS5C_BEREICHE_106=1
# Schritt 2: Schutzliste bauen und gegen 1.0.5 vergleichen (läuft in WSL)
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test; E=$KK/test/kern/ergebnis; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/schritt2.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_schritt2.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_schritt2 || { echo BAU FEHLGESCHLAGEN; exit 1; }
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
$PS5_PAYLOAD_SDK/bin/prospero-clang -c -O2 -Wall -I$KK/bau/pcre2-ps5/include $KK/kern/kern_schutz.c -o $B/ps5/kern_schutz.o && echo "PS5-Bau kern_schutz: ok"
echo
echo "== A1 Schutzliste: 3004 Prüfpfade"
$B/test_schritt2 erlaubt $RF/pfade.jsonl $E/erlaubt_3004_c.txt
if cmp -s <(tr -d '\r' < $RF/ergebnis/erlaubt_3004.txt) $E/erlaubt_3004_c.txt; then echo "GLEICH wie 1.0.5"; else echo "UNTERSCHIED:"; diff <(tr -d '\r' < $RF/ergebnis/erlaubt_3004.txt) $E/erlaubt_3004_c.txt | head; fi
echo "== A2 Schutzliste: 42430 Grenzfälle"
$B/test_schritt2 erlaubt $RF/grenzfaelle.jsonl $E/erlaubt_grenz_c.txt
if cmp -s <(tr -d '\r' < $RF/ergebnis/erlaubt_grenzfaelle.txt) $E/erlaubt_grenz_c.txt; then echo "GLEICH wie 1.0.5"; else
  echo "UNTERSCHIED:"; paste -d' ' <(tr -d '\r' < $RF/ergebnis/erlaubt_grenzfaelle.txt) $E/erlaubt_grenz_c.txt $RF/grenzfaelle.jsonl | awk '$1 != $2' | head -20; fi
echo "== A3 Verknüpfungsprüfung (OhneVerknuepfung), 968 Pfade je Baum"
for r in root9 root10w; do
  rm -rf $T/$r; cp -r $RF/$r $T/$r
  if [ $r = root10w ]; then export FAKELINKS=user/data/hb; else unset FAKELINKS; fi
  (cd $T; $B/test_schritt2 verknuepfung $T/$r $RF/vpfade.jsonl > $E/verknuepfung_${r}_c.out)
  if cmp -s <(tr -d '\r' < $RF/ergebnis/verknuepfung_$r.out | grep '^V ') $E/verknuepfung_${r}_c.out; then echo "$r: GLEICH wie 1.0.5 ($(grep -c '^V 1' $E/verknuepfung_${r}_c.out) ja / $(grep -c '^V 0' $E/verknuepfung_${r}_c.out) nein)"; else echo "$r: UNTERSCHIED"; diff <(tr -d '\r' < $RF/ergebnis/verknuepfung_$r.out | grep '^V ') $E/verknuepfung_${r}_c.out | head; fi
done
date "+%Y-%m-%d %H:%M:%S FERTIG"
