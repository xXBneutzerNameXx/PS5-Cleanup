#!/bin/bash
# Test ShadowMount: verschobene Spieldateien (27.10.2026) - läuft in WSL
export PLAYDEN_WEB=zu   # NEU 1.0.10 (12.11.2026): Playden gilt im Test als geschlossen (zugriff_datei.c d_playden_web)
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/sm; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/sm.log" 2>&1
set -o pipefail
RC=0
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_sm.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_sm || { echo BAU FEHLGESCHLAGEN; exit 1; }
kachel() {  # id titel quelle-in-mount_img.lnk
  mkdir -p "$T/w/user/app/$1/sce_sys"
  printf '{"titleId":"%s","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"%s"}},"contentVersion":"01.00"}' "$1" "$2" > "$T/w/user/app/$1/sce_sys/param.json"
  printf '/mnt/shadowmnt/%s\n%s' "$1" "$3" > "$T/w/user/app/$1/mount_img.lnk"
}
for art in schutz ohne; do
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  rm -f "$T/w/mnt/ext1/homebrew/.Zwei - PPSA90002.exfat.gc-compress.tmp"
  kachel PPSA30528 "Red Dead Redemption" "/mnt/ext1/homebrew/Red Dead Redemption - PPSA30528 - v1.2.0 - US.exfat"
  echo x > "$T/w/user/data/homebrew/Red Dead Redemption - PPSA30528 - v1.2.0 - US.exfat"
  kachel PPSA99058 "Overdrive" "/mnt/ext1/homebrew/Overdrive.exfat"
  echo x > "$T/w/user/data/homebrew/Overdrive.exfat"
  kachel PPSA77771 "Usb Spiel" "/mnt/ext1/homebrew/Usb Spiel - PPSA77771.exfat"
  mkdir -p $T/w/mnt/usb0/homebrew; echo x > "$T/w/mnt/usb0/homebrew/Usb Spiel - PPSA77771.exfat"
  kachel PPSA77772 "Umbenannt" "/mnt/ext1/homebrew/Alt - PPSA77772.exfat"
  echo x > "$T/w/user/data/homebrew/Neu PPSA77772.exfat"
  kachel PPSA77773 "Eigener Ort" "/data/homebrew/Eigen.exfat"
  mkdir -p $T/w/user/data/meinespiele; echo x > $T/w/user/data/meinespiele/Eigen.exfat
  # NEU 1.0.9 (08.11.2026, ShadowMountPlus 1.7): Einstellungen im Format 2 - eigener Suchort als "Path" unter "[ Scan ]", in Anführungszeichen;
  # "path" unter [emulators] ist kein Suchort
  kachel PPSA77782 "Eigener Ort neu" "/data/homebrew/Eigen2.exfat"
  mkdir -p "$T/w/user/data/meine spiele2"; echo x > "$T/w/user/data/meine spiele2/Eigen2.exfat"
  kachel PPSA77783 "Falscher Abschnitt" "/data/homebrew/Eigen3.exfat"
  mkdir -p $T/w/user/data/nichtscan; echo x > $T/w/user/data/nichtscan/Eigen3.exfat
  printf '# Beispiel\n# scanpath=/data/falsch\nscanpath=/data/meinespiele/   # eigene Spiele\n[config]\nversion = 2\n[emulators]\npath = /data/nichtscan\n[ Scan ]   ; Abschnitt mit Leerzeichen\nPath = "/data/meine spiele2/"  ; Format 2 (ShadowMountPlus 1.7)\n' > $T/w/user/data/shadowmount/config.ini
  kachel PPSA77774 "Abgezogen" "/mnt/usb3/homebrew/Abgezogen - PPSA77774.exfat"
  kachel PPSA77775 "Geteilt A" "/data/homebrew/Geteilt - PPSA77775.exfat"
  kachel PPSA77776 "Geteilt B" "/data/homebrew/Geteilt - PPSA77775.exfat"
  echo x > "$T/w/user/data/homebrew/Geteilt - PPSA77775.exfat"
  kachel PPSA77777 "Fremd am alten Ort" "/data/homebrew/Fremd - PPSA88888.exfat"; echo x > "$T/w/user/data/homebrew/Fremd - PPSA88888.exfat"
  kachel PPSA77778 "Doppel vermutet" "/mnt/ext1/homebrew/Doppel.exfat"
  kachel PPSA77779 "Doppel echt" "/data/homebrew/Doppel.exfat"; echo x > "$T/w/user/data/homebrew/Doppel.exfat"
  kachel PPSA77770 "Rueck echt" "/data/homebrew/Rueck.exfat"; echo x > "$T/w/user/data/homebrew/Rueck.exfat"
  kachel PPSA77780 "Rueck vermutet" "/mnt/ext1/homebrew/Rueck.exfat"
  kachel PPSA77781 "Zwei Kandidaten" "/mnt/ext1/homebrew/Zwei.exfat"
  mkdir -p $T/w/user/data/homebrew/backports; echo x > $T/w/user/data/homebrew/Zwei.exfat; echo x > $T/w/user/data/homebrew/backports/Zwei.exfat
  (cd $T; RMDA=1 FAKESM=1 $B/test_sm $T $art) | grep -v "^LOG:" || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"; RC=1; }
  echo "-- keine M.2 eingebaut (Geräteliste ohne ssd1)"
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w; rm -rf $T/w/mnt/ext1; mkdir -p $T/w/dev; touch $T/w/dev/ssd0 $T/w/dev/ssd0.user $T/w/dev/da1
  mkdir -p $T/w/system_data/priv/appmeta/PPSA90002; echo '{}' > $T/w/system_data/priv/appmeta/PPSA90002/param.json   # Codex-Prüfung 19 (P2)
  (cd $T; RMDA=1 FAKESM=1 $B/test_sm $T $art m2weg) | grep -v "^LOG:" || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"; RC=1; }
  echo "-- Einhängeordner gelöschter ShadowMount-Spiele (03.11.2026)"
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  rm -f "$T/w/user/data/homebrew/.Spiel Eins - PPSA90001.exfat.gc-compress.tmp"
  mkdir -p $T/w/system_ex/app/PPSA90001 $T/w/system_ex/app/PPSA90002 $T/w/system_ex/app/PPSA90003 $T/w/system_ex/app/FAKE78001 $T/w/system_ex/app/FAKE77031
  echo x > $T/w/system_ex/app/PPSA90003/x
  (cd $T; RMDA=1 FAKESM=1 $B/test_sm $T $art einhaenge) | grep -v "^LOG:" || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"; RC=1; }
  echo "-- M.2 eingebaut, aber nicht eingehängt (Geräteliste mit ssd1)"
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w; rm -rf $T/w/mnt/ext1; mkdir -p $T/w/dev; touch $T/w/dev/ssd0 $T/w/dev/ssd1 $T/w/dev/ssd1.user
  rm -f "$T/w/user/data/homebrew/.Spiel Eins - PPSA90001.exfat.gc-compress.tmp"   # sonst sperrt die laufende Komprimierung (richtig, aber nicht Thema dieses Falls)
  (cd $T; RMDA=1 FAKESM=1 $B/test_sm $T $art m2unerkannt) | grep -v "^LOG:" || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"; RC=1; }
  echo "-- Starter eines Playden-Spiels (NEU 1.0.10, Codex-Prüfung 161)"
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  rm -f "$T/w/user/data/homebrew/.Spiel Eins - PPSA90001.exfat.gc-compress.tmp"
  kachel FAKE78011 "Playden-Spiel" "/data/homebrew/FAKE78011"; kachel FAKE78012 "Fremd" "/data/homebrew/FAKE78012"
  mkdir -p $T/w/user/data/homebrew/FAKE78011 $T/w/user/data/homebrew/FAKE78012
  printf '{\n  "any_title": true,\n  "managed_publication_guard": "playden-publication-guard-v1",\n  "title_rule": {\n    "allow_list": "/data/playden/title-rule/allow-list",\n    "prefix": "FAKE78"\n  }\n}\n' > $T/w/user/data/homebrew/FAKE78011/native-title-build.json
  echo x > $T/w/user/data/homebrew/FAKE78011/eboot.bin; echo x > $T/w/user/data/homebrew/FAKE78012/eboot.bin; echo '{"guard":"playden-publication-guard-v1","rule":"/data/playden/title-rule"}' > $T/w/user/data/homebrew/FAKE78012/native-title-build.json   # Codex-Prüfung 162: nachgemacht, nicht in Playdens Liste
  mkdir -p $T/w/user/data/playden/runtime-state; echo '{"schema_version":1,"reservations":[],"installs":[{"title_id":"FAKE78011","app_id":11,"install_path":"/data/playden-games/11","data_root":"/data/playden/games/steam-11"}]}' > $T/w/user/data/playden/runtime-state/installs.json
  (cd $T; RMDA=1 FAKESM=1 $B/test_sm $T $art playden) | grep -v "^LOG:" || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"; RC=1; }
  echo "-- gesperrter Suchordner (NEU 1.0.10, wie ShadowMountPlus 1.7beta12)"
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  rm -f "$T/w/user/data/homebrew/.Spiel Eins - PPSA90001.exfat.gc-compress.tmp"
  kachel PPSA77799 "Gesperrt Spiel" "/mnt/ext1/homebrew/Weg - PPSA77799.exfat"
  mkdir -p $T/w/user/data/etaHEN/games; echo x > "$T/w/user/data/etaHEN/games/Weg - PPSA77799.exfat"
  kachel PPSA77798 "Gefunden trotz Sperre" "/mnt/ext1/homebrew/Weg2 - PPSA77798.exfat"; echo x > "$T/w/user/data/homebrew/Weg2 - PPSA77798.exfat"
  kachel PPSA77797 "Ort gesperrt" "/data/geheim/Drin - PPSA77797.exfat"
  mkdir -p $T/w/user/data/geheim; echo x > "$T/w/user/data/geheim/Drin - PPSA77797.exfat"
  chmod 000 $T/w/user/data/etaHEN/games $T/w/user/data/geheim
  (cd $T; RMDA=1 FAKESM=1 $B/test_sm $T $art gesperrt) | grep -v "^LOG:" || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"; RC=1; }
  chmod 755 $T/w/user/data/etaHEN/games $T/w/user/data/geheim
done
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
