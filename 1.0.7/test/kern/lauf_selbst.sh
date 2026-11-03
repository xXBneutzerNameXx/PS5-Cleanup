#!/bin/bash
# Test vollständige Selbstentfernung inkl. Startordner (29.10.2026) - Baum wie PS5 nach Schritt 3 - läuft in WSL
K="${K:-/pfad/zu/PS5-Cleanup/1.0.7}"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/selbst; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/selbst.log" 2>&1
RC=0
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_selbst.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_selbst || { echo BAU FEHLGESCHLAGEN; exit 1; }
baum() {
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  rm -rf $T/w/user/data/homebrew/PS5Cleanup $T/w/mnt/ext1/homebrew/MeinCleaner          # wie PS5: keine Kopie intern/M.2
  mkdir -p $T/w/mnt/usb0; cp $RF/schutz_v1/user/data/homebrew/PS5Cleanup/eboot.elf $T/w/mnt/usb0/ps5cleanup.elf   # ELF auf dem Stick
  mkdir -p $T/w/user/app/PSCL00001/sce_sys $T/w/user/appmeta/PSCL00001 $T/w/system_data/priv/appmeta/PSCL00001
  echo "PS5CLEANUP-KACHEL" > $T/w/user/app/PSCL00001/.ps5cleanup_kachel; echo 1 > $T/w/user/app/PSCL00001/.ps5cleanup_eingetragen
  echo '{"titleId":"PSCL00001","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"PS5 Cleanup"}}}' > $T/w/user/app/PSCL00001/sce_sys/param.json
  echo '{}' > $T/w/user/appmeta/PSCL00001/param.json; echo '{}' > $T/w/system_data/priv/appmeta/PSCL00001/param.json
  [ "$1" = sysmeta_fehlt ] && rm -rf $T/w/system_data/priv/appmeta/PSCL00001
  mkdir -p $T/w/system_ex/app/PSCL00002/sce_sys; echo "PS5CLEANUP-STARTORDNER" > $T/w/system_ex/app/PSCL00002/.ps5cleanup_start
  echo x > $T/w/system_ex/app/PSCL00002/eboot.bin; echo '{}' > $T/w/system_ex/app/PSCL00002/sce_sys/param.json
  printf 'Testeintrag fuer den Selbstentfernungstest\n' > $T/w/user/data/ps5cleanup_austragen.txt
  [ "$1" = geist ] && { mkdir -p $T/w/mnt/ext1/user/app/PSCL00001/sce_sys; echo x > $T/w/mnt/ext1/user/app/PSCL00001/eboot.bin; }   # Kopie auf M.2 -> nie über Sony
  [ "$1" = sandbox ] && { mkdir -p $T/w/mnt/sandbox/PSCL00002_000/system; echo x > $T/w/mnt/sandbox/PSCL00002_000/system/x; }   # Laufzeitbereich der App (PS5-Test 23:45)
}
for fall in ${FAELLE:-sofort appmeta_bleibt appmeta_spaet sysmeta_fehlt uebergabe kein_hintergrund fehler_vorher sandbox kein_hg_ro unbestaetigt geist kein_hg_ablehnen}; do
  baum $fall
  FD=; [ $fall = fehler_vorher ] && FD=/user/data/PS5Cleanup
  (cd $T; FAILDEL=$FD RMDA=1 $B/test_selbst $T $fall) | grep -v "^LOG:" || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"; RC=1; }
done
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
