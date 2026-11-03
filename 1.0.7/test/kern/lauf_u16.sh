#!/bin/bash
# Test U16 (laufende Aufnahme der PS5 schützen) - läuft in WSL
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/u16; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/u16.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_u16.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_u16 || { echo BAU FEHLGESCHLAGEN; exit 1; }
for art in ohne schutz; do
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  for f in sharevideo_1a2b3c4d_hdr.webm sharevideo_1a2b3c4d.webm meinclip.webm; do printf 'webm' > $T/w/user/av_contents/content_tmp/$f; done
  (cd $T; RMDA=1 FAKESM=1 $B/test_u16 $T $art)
done | grep -v "^LOG:"
date "+%Y-%m-%d %H:%M:%S FERTIG"
