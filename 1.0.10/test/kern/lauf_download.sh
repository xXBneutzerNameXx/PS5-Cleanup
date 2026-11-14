#!/bin/bash
# Test Download-Erkennung (27.10.2026) - läuft in WSL
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/dl; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/download.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_download.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_download || { echo BAU FEHLGESCHLAGEN; exit 1; }
bash $RF/dl_baum.sh
# 1.0.10 (da_Chris86 12.11.2026: Rechenleistung des Mini-PCs nutzen): jeder Fall gleichzeitig mit eigenem Arbeitsordner ($TS/<art>_<fall>),
# Ausgabe danach in der bisherigen Reihenfolge. Inhalt der Fälle unverändert.
TS=$T
fall_dl() {
  local art=$1 fall=$2 T=$TS/$1_$2; mkdir -p $T
  case $fall in
    normal) rm -rf $T/w; cp -r $RF/download_v1 $T/w
      (cd $T; RMDA=1 FAKESM=1 $B/test_download $T $art) | grep -v "^LOG:" ;;
    m2teil) echo "-- M.2 nur teilweise sichtbar (user/app fehlt)"
      rm -rf $T/w; cp -r $RF/download_v1 $T/w; rm -rf $T/w/mnt/ext1/user/app
      (cd $T; RMDA=1 FAKESM=1 $B/test_download $T $art m2unklar) | grep -v "^LOG:" ;;
    usbfehler) echo "-- USB-Erweiterung nicht lesbar"
      rm -rf $T/w; cp -r $RF/download_v1 $T/w; touch $T/faillist.on
      (cd $T; RMDA=1 FAKESM=1 FAILLIST=/mnt/ext0/user $B/test_download $T $art usbfehler) | grep -v "^LOG:"; rm -f $T/faillist.on ;;
    lesefehler) echo "-- /system_ex/app nicht lesbar"
      rm -rf $T/w; cp -r $RF/download_v1 $T/w; touch $T/faillist.on
      (cd $T; RMDA=1 FAKESM=1 FAILLIST=/system_ex/app $B/test_download $T $art lesefehler) | grep -v "^LOG:"; rm -f $T/faillist.on ;;
    usbkopie) echo "-- Spiel PPSA55555 zusätzlich auf der USB-Erweiterung (Zwilling)"
      rm -rf $T/w; cp -r $RF/download_v1 $T/w; mkdir -p $T/w/mnt/ext0/user/app; cp -r $T/w/user/app/PPSA55555 $T/w/mnt/ext0/user/app/
      (cd $T; RMDA=1 FAKESM=1 $B/test_download $T $art usbkopie) | grep -v "^LOG:" ;;
    jb) echo "-- Jailbreak-Einstiege: \"Auch löschen\""
      rm -rf $T/w; cp -r $RF/download_v1 $T/w
      (cd $T; RMDA=1 FAKESM=1 $B/test_download $T $art jbloeschen) | grep -v "^LOG:" ;;
    m2fehlt) echo "-- M.2 fehlt ganz"
      rm -rf $T/w; cp -r $RF/download_v1 $T/w; rm -rf $T/w/mnt/ext1
      (cd $T; RMDA=1 FAKESM=1 $B/test_download $T $art m2unklar) | grep -v "^LOG:" ;;
  esac
}
FAELLE="normal m2teil usbfehler lesefehler usbkopie jb m2fehlt"
for art in schutz ohne; do for fall in $FAELLE; do fall_dl $art $fall > $TS/_aus_${art}_$fall.txt 2>&1 & done; done
wait
for art in schutz ohne; do for fall in $FAELLE; do cat $TS/_aus_${art}_$fall.txt; done; done
date "+%Y-%m-%d %H:%M:%S FERTIG"
