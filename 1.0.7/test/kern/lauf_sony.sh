#!/bin/bash
# Test Sonys Deinstallations-Dienst (28.10.2026) - läuft in WSL
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/sony; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/sony.log" 2>&1
set -o pipefail
RC=0
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_sony.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_sony || { echo BAU FEHLGESCHLAGEN; exit 1; }
app() {  # id titel [deeplink]
  mkdir -p "$T/w/user/app/$1/sce_sys" "$T/w/user/appmeta/$1" "$T/w/system_data/priv/appmeta/$1"
  dl=""; [ -n "$3" ] && dl=",\"deeplinkUri\":\"$3\""
  printf '{"titleId":"%s","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"%s"}},"contentVersion":"01.00"%s}' "$1" "$2" "$dl" > "$T/w/user/app/$1/sce_sys/param.json"
  echo x > "$T/w/user/app/$1/eboot.bin"; echo '{}' > "$T/w/user/appmeta/$1/param.json"; echo '{}' > "$T/w/system_data/priv/appmeta/$1/param.json"
}
kachel() {  # id titel quelle
  mkdir -p "$T/w/user/app/$1/sce_sys" "$T/w/user/appmeta/$1"
  printf '{"titleId":"%s","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"%s"}},"contentVersion":"01.00"}' "$1" "$2" > "$T/w/user/app/$1/sce_sys/param.json"
  printf '/mnt/shadowmnt/%s\n%s' "$1" "$3" > "$T/w/user/app/$1/mount_img.lnk"; echo '{}' > "$T/w/user/appmeta/$1/param.json"
}
baum() {
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  rm -f "$T/w/mnt/ext1/homebrew/.Zwei - PPSA90002.exfat.gc-compress.tmp" "$T/w/user/data/homebrew/.Spiel Eins - PPSA90001.exfat.gc-compress.tmp"
  app PPSA61111 "Sony Testspiel"; mkdir -p $T/w/user/patch/PPSA61111/sce_sys; echo x > $T/w/user/patch/PPSA61111/x.bin
  app TOOL61111 "Testtool" "http://127.0.0.1:7777/"
  mkdir -p $T/w/user/data/Testtool; echo '{}' > $T/w/user/data/Testtool/einstellungen.json   # NEU 1.0.7: Daten des Tools (unter seiner Kachel)
  mkdir -p $T/w/mnt/usb0; echo x > $T/w/mnt/usb0/Testtool_v1.2.elf   # NEU 1.0.7 (da_Chris86 01.11.2026): ELF des Tools auf dem USB-Stick - auch unter der Kachel
  # NEU 1.0.7 (da_Chris86 01.11.2026 23:22): Game Compressor und LZ4-Game-Compressor nicht verwechseln (Namen wie auf seiner PS5)
  app LZGC50001 "LZ4 Game Compressor" "http://127.0.0.1:7071/"; app PSGC50001 "Spielekompressor" "http://127.0.0.1:7072/"
  mkdir -p $T/w/user/data/GameCompressor $T/w/user/data/AMPRStudio; echo x > $T/w/user/data/GameCompressor/launcher.ok; echo x > $T/w/user/data/AMPRStudio/launcher-install.log
  echo x > $T/w/mnt/usb0/game-compressor_v1.0.4.elf; echo x > $T/w/mnt/usb0/LZ4-Game-Compressor.elf
  rm -rf $T/w/mnt/ext1/user/app/FAKE00000   # nur die interne Kopie (sonst zweite Kopie -> nie über Sony)
  app FAKE00000 "Homebrew Launcher"
  mkdir -p $T/w/system_data/priv/appmeta/PPSA64444; echo '{"titleId":"PPSA64444","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"Schloss Spiel"}}}' > $T/w/system_data/priv/appmeta/PPSA64444/param.json   # leere Kachel (Schloss)
  app PPSA65555 "Geschuetzt drin"; mkdir -p $T/w/user/app/PPSA65555/NPXS20001; echo x > $T/w/user/app/PPSA65555/NPXS20001/x   # Sony darf hier nie ran (Codex 32)
  app PPSA01650 "YouTube"; mkdir -p $T/w/user/download/PPSA01650; echo x > $T/w/user/download/PPSA01650/download0.dat   # Y2JB-Einstieg (Jailbreak-Anzeige)
  mkdir -p $T/w/user/data/homebrew
  kachel PPSA62222 "SM Mit" "/data/homebrew/SM Mit - PPSA62222.exfat"; echo x > "$T/w/user/data/homebrew/SM Mit - PPSA62222.exfat"
  kachel PPSA63333 "SM Ohne" "/data/homebrew/SM Ohne - PPSA63333.exfat"; echo x > "$T/w/user/data/homebrew/SM Ohne - PPSA63333.exfat"
  # NEU 30.10.2026: ShadowMount-Spiel mit Spieldatei auf einem normalen USB-Stick + eine normale Datei auf dem Stick
  mkdir -p $T/w/mnt/usb0; kachel PPSA63530 "Vice City" "/mnt/usb0/Vice City - PPSA63530.exfat"; echo x > "$T/w/mnt/usb0/Vice City - PPSA63530.exfat"; echo x > $T/w/mnt/usb0/notiz.txt
  # NEU 30.10.2026 (da_Chris86 08:10): Spiel-Abbild OHNE Kachel im Ordner homebrew auf dem Stick + ELF daneben -> Ordner wird aufgeklappt
  mkdir -p $T/w/mnt/usb0/homebrew; echo x > $T/w/mnt/usb0/homebrew/PPSA03530.exfat; echo x > $T/w/mnt/usb0/homebrew/tool.elf
  # NEU 29.10.2026: Lua-Jailbreak-Spiel über ShadowMount (mit Spielstand) - bleibt unter "Spiele", geschützt
  kachel CUSA16074 "Raspberry Cube" "/data/homebrew/Raspberry Cube - CUSA16074.exfat"; echo x > "$T/w/user/data/homebrew/Raspberry Cube - CUSA16074.exfat"
  mkdir -p $T/w/user/home/1a2b3c4d/savedata/CUSA16074; echo s > $T/w/user/home/1a2b3c4d/savedata/CUSA16074/sdimg_SAVEDATA00
  # Codex-Prüfung 36: Spiel-Abbild eines Lua-Spiels ohne Kachel - eigener Eintrag, geschützt, bleibt unter "Spiele"
  echo x > "$T/w/user/data/homebrew/Aibeya - CUSA17068.exfat"
  # Codex-Prüfung 37: Abbild eines Einstiegs in einem Unterordner; gemeinsame Spieldatei (ohne Kennung) einer Einstiegs- und einer normalen Kachel
  mkdir -p "$T/w/user/data/homebrew/Backups"; echo x > "$T/w/user/data/homebrew/Backups/Jinki Resurrection - CUSA25179.exfat"; echo x > "$T/w/user/data/homebrew/Backups/notiz.txt"
  # NEU 30.10.2026: Backups (Abbild direkt darin) wird seit 30.10. aufgeklappt - der Sicherheitsstopp für einen Ordner als Ganzes
  # wird darum mit "Archiv" geprüft (Abbild eine Ebene tiefer, Ordner bleibt ein Eintrag)
  mkdir -p "$T/w/user/data/homebrew/Archiv/alt"; echo x > "$T/w/user/data/homebrew/Archiv/alt/Jinki Resurrection - CUSA25179.exfat"; echo x > "$T/w/user/data/homebrew/Archiv/notiz.txt"
  kachel CUSA19556 "Aikagi 2" "/data/homebrew/gemeinsam.exfat"; kachel PPSA66666 "Andere Kachel" "/data/homebrew/gemeinsam.exfat"; echo x > "$T/w/user/data/homebrew/gemeinsam.exfat"
  mkdir -p $T/w/user/app/PSCL00001/sce_sys; echo x > $T/w/user/app/PSCL00001/.ps5cleanup_kachel; echo '{"titleId":"PSCL00001"}' > $T/w/user/app/PSCL00001/sce_sys/param.json
  # NEU 03.11.2026 (da_Chris86 17:50): leere Einhängeordner - auch nach "nur Kachel" weg, wenn die Kachel über Sony ging
  mkdir -p $T/w/system_ex/app/PPSA63333 $T/w/system_ex/app/PPSA62222
}
for fall in ok sim ablehnen ohne langsam selbstjb selbstjb2 jbordner einbindung eigenes fremd; do
  baum
  # Codex-Prüfung 36: Spiel-Abbild eines Lua-Spiels im Ordner von PS5 Cleanup - die Selbstentfernung lässt den Ordner stehen
  [ $fall = selbstjb ] && { mkdir -p $T/w/user/data/homebrew/PS5Cleanup; echo x > "$T/w/user/data/homebrew/PS5Cleanup/Fuyu Kiss - CUSA29745.exfat"; }
  # Codex-Prüfung 37: Spieldatei OHNE Kennung im Namen im Ordner von PS5 Cleanup (Kachel eines Lua-Spiels); Abbild im eigenen App-Ordner (Sony nie)
  [ $fall = selbstjb2 ] && { mkdir -p $T/w/user/data/homebrew/PS5Cleanup; kachel CUSA11481 "Mikagami" "/data/homebrew/PS5Cleanup/mika.exfat"; kachel PPSA67777 "Zweite Kachel" "/data/homebrew/PS5Cleanup/mika.exfat"; echo x > "$T/w/user/data/homebrew/PS5Cleanup/mika.exfat"; echo x > "$T/w/user/app/PSCL00001/Raspberry - CUSA16074.exfat"; }
  case $fall in ok|sim|selbstjb|selbstjb2|jbordner|einbindung|eigenes|fremd) U=ok;; ablehnen) U=ablehnen;; langsam) U=langsam;; ohne) U=;; esac
  (cd $T; UNINSTALL=$U UNINSTALL_NIE=PSCL00002 RMDA=1 FAKESM=1 $B/test_sony $T $fall) | grep -v "^LOG:" || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"; RC=1; }
done
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
