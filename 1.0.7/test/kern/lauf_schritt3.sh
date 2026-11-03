#!/bin/bash
# 1.0.7: Vergleich Eintrag für Eintrag mit 1.0.5 - Bereiche wie 1.0.6 (6 Bereiche); die neue Einteilung prüft test_bereiche
export PS5C_BEREICHE_106=1
# Schritt 3/4: Suche + Auswahl bauen und gegen 1.0.5 vergleichen (läuft in WSL)
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test; E=$KK/test/kern/ergebnis; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T $B/ps5
exec > "$K/test/kern/ergebnis/schritt3.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_schritt3.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_schritt3 || { echo BAU FEHLGESCHLAGEN; exit 1; }
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
for f in kern_suche kern_auswahl; do $PS5_PAYLOAD_SDK/bin/prospero-clang -c -O2 -Wall -Wno-missing-field-initializers -I$KK/bau/pcre2-ps5/include -I$KK/bau/cjson $KK/kern/$f.c -o $B/ps5/$f.o && echo "PS5-Bau $f: ok"; done
$PS5_PAYLOAD_SDK/bin/prospero-clang -c -O2 $KK/bau/cjson/cJSON.c -o $B/ps5/cJSON.o && echo "PS5-Bau cJSON: ok"
echo
for art in detail auswahl; do
 for r in root9 root10w gen1 gen2 download_v1; do
  for sp in de en; do
   rm -rf $T/$r; cp -r $RF/$r $T/$r
   case $r in root10w) export FAKELINKS=user/data/hb;; gen2) export FAKELINKS=user/data/etaHEN;; *) unset FAKELINKS;; esac
   (cd $T; $B/test_schritt3 $art $T/$r $sp > $E/${sp}_${art}_${r}_c.out)
   tr -d '\r' < $RF/ergebnis/${sp}_${art}_${r}.out | grep -v '^FEHLERZAHL' > $E/${sp}_${art}_${r}_soll.out
   if cmp -s $E/${sp}_${art}_${r}_soll.out $E/${sp}_${art}_${r}_c.out; then echo "$art $r $sp: GLEICH ($(wc -l < $E/${sp}_${art}_${r}_c.out) Zeilen)"; else echo "$art $r $sp: UNTERSCHIED"; diff $E/${sp}_${art}_${r}_soll.out $E/${sp}_${art}_${r}_c.out | head -12; fi
  done
 done
done
date "+%Y-%m-%d %H:%M:%S FERTIG"
