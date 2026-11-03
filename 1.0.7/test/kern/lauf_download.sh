#!/bin/bash
# Test Download-Erkennung (27.10.2026) - läuft in WSL
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/dl; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/download.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_download.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_download || { echo BAU FEHLGESCHLAGEN; exit 1; }
bash $RF/dl_baum.sh
for art in schutz ohne; do
  rm -rf $T/w; cp -r $RF/download_v1 $T/w
  (cd $T; RMDA=1 FAKESM=1 $B/test_download $T $art) | grep -v "^LOG:"
  echo "-- M.2 nur teilweise sichtbar (user/app fehlt)"
  rm -rf $T/w; cp -r $RF/download_v1 $T/w; rm -rf $T/w/mnt/ext1/user/app
  (cd $T; RMDA=1 FAKESM=1 $B/test_download $T $art m2unklar) | grep -v "^LOG:"
  echo "-- USB-Erweiterung nicht lesbar"
  rm -rf $T/w; cp -r $RF/download_v1 $T/w; touch $T/faillist.on
  (cd $T; RMDA=1 FAKESM=1 FAILLIST=/mnt/ext0/user $B/test_download $T $art usbfehler) | grep -v "^LOG:"; rm -f $T/faillist.on
  echo "-- /system_ex/app nicht lesbar"
  rm -rf $T/w; cp -r $RF/download_v1 $T/w; touch $T/faillist.on
  (cd $T; RMDA=1 FAKESM=1 FAILLIST=/system_ex/app $B/test_download $T $art lesefehler) | grep -v "^LOG:"; rm -f $T/faillist.on
  echo "-- Spiel PPSA55555 zusätzlich auf der USB-Erweiterung (Zwilling)"
  rm -rf $T/w; cp -r $RF/download_v1 $T/w; mkdir -p $T/w/mnt/ext0/user/app; cp -r $T/w/user/app/PPSA55555 $T/w/mnt/ext0/user/app/
  (cd $T; RMDA=1 FAKESM=1 $B/test_download $T $art usbkopie) | grep -v "^LOG:"
  echo "-- Jailbreak-Einstiege: \"Auch löschen\""
  rm -rf $T/w; cp -r $RF/download_v1 $T/w
  (cd $T; RMDA=1 FAKESM=1 $B/test_download $T $art jbloeschen) | grep -v "^LOG:"
  echo "-- M.2 fehlt ganz"
  rm -rf $T/w; cp -r $RF/download_v1 $T/w; rm -rf $T/w/mnt/ext1
  (cd $T; RMDA=1 FAKESM=1 $B/test_download $T $art m2unklar) | grep -v "^LOG:"
done
date "+%Y-%m-%d %H:%M:%S FERTIG"
