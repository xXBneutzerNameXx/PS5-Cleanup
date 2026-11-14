#!/bin/bash
# Entscheidungen 26.10.2026 zur Sicherheitsprüfung (Punkte 6 und 7): M.2 und Browserspeicher nur einzeln,
# Spieldatei mit mehreren Kacheln gesperrt, M.2 nicht erkannt -> Reste möglicher M.2-Spiele gesperrt (läuft in WSL)
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/m2; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/m2.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_m2.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_m2 || { echo BAU FEHLGESCHLAGEN; exit 1; }
for art in schutz ohne; do
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  rm -f "$T/w/mnt/ext1/homebrew/.Zwei - PPSA90002.exfat.gc-compress.tmp"
  (cd $T; RMDA=1 FAKESM=1 $B/test_m2 m2 $T $art)
done
for art in schutz ohne; do
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  rm -f "$T/w/mnt/ext1/homebrew/.Zwei - PPSA90002.exfat.gc-compress.tmp"
  cp -r $T/w/user/app/PPSA90002 $T/w/user/app/PPSA90012
  (cd $T; RMDA=1 FAKESM=1 $B/test_m2 mehrfach $T $art)
done
for art in schutz ohne; do
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w; rm -rf $T/w/mnt/ext1
  echo "-- M.2 fehlt ganz"
  (cd $T; RMDA=1 FAKESM=1 $B/test_m2 m2weg $T $art)
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w; rm -rf $T/w/mnt/ext1/user
  echo "-- M.2 ohne user-Ordner"
  (cd $T; RMDA=1 FAKESM=1 $B/test_m2 m2weg $T $art)
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w; rm -rf $T/w/mnt/ext1/user/app
  echo "-- M.2 nur teilweise sichtbar (user/ da, Spieleliste user/app fehlt) - NEU Codex-Prüfung 26.10."
  (cd $T; RMDA=1 FAKESM=1 $B/test_m2 m2weg $T $art)
done | grep -v "^LOG:"
date "+%Y-%m-%d %H:%M:%S FERTIG"
