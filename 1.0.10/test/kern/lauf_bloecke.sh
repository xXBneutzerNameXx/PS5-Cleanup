#!/bin/bash
# Test "ein Block pro Kachel", Trophäen-Listen, Aufnahmen-Kopien, alte Sicherungen, gesperrte Einzeldateien (PS5-Test 02.11.2026) - läuft in WSL
export PLAYDEN_WEB=zu   # NEU 1.0.10 (12.11.2026): Playden gilt im Test als geschlossen (zugriff_datei.c d_playden_web)
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/blo; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/bloecke.log" 2>&1
set -o pipefail; RC=0   # Codex-Prüfung 40 (6): Rückgabewerte der Tests nicht verschlucken
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_bloecke.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_bloecke || { echo BAU FEHLGESCHLAGEN; exit 1; }
W=$T/w
rm -rf $T/ref; mkdir -p $T/ref; cp -r $RF/root9 $T/ref/root9; cp $RF/dl_baum.sh $T/ref/; (cd $T/ref; bash dl_baum.sh > /dev/null); rm -rf $W; cp -r $T/ref/download_v1 $W
# Kachel mit param.json: pj <ID> <Kategorie> <Titel> [deeplink]
pj() { mkdir -p $W/user/app/$1/sce_sys $W/user/appmeta/$1 $W/system_data/priv/appmeta/$1
  if [ -n "$4" ]; then dl=",\"deeplinkUri\":\"$4\""; else dl=""; fi
  printf '{"titleId":"%s","applicationCategoryType":%s%s,"localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"%s"}}}' "$1" "$2" "$dl" "$3" > $W/user/appmeta/$1/param.json
  echo x > $W/user/app/$1/eboot.bin; }
# PS4-Spiel mit param.sfo
sfo() { mkdir -p $W/user/app/$1/sce_sys $W/user/appmeta/$1 $W/system_data/priv/appmeta/$1; echo x > $W/user/app/$1/eboot.bin; python3 - "$W/user/appmeta/$1/param.sfo" "$2" "$3" <<'PY'
import struct, sys
ziel, cat, title = sys.argv[1], sys.argv[2], sys.argv[3]
eintr = [("CATEGORY", cat), ("TITLE", title)]
keys = b""; offs = []
for k, _ in eintr: offs.append(len(keys)); keys += k.encode() + b"\0"
while len(keys) % 4: keys += b"\0"
daten = b""; doffs = []; laengen = []
for _, v in eintr:
    d = v.encode() + b"\0"; doffs.append(len(daten)); laengen.append(len(d)); d += b"\0" * ((-len(d)) % 4); daten += d
kt = 20 + 16 * len(eintr); dt = kt + len(keys)
kopf = b"\0PSF" + struct.pack("<IIII", 0x0101, kt, dt, len(eintr))
tab = b"".join(struct.pack("<HHIII", offs[i], 0x0204, laengen[i], laengen[i] + ((-laengen[i]) % 4), doffs[i]) for i in range(len(eintr)))
open(ziel, "wb").write(kopf + tab + keys + daten)
PY
}
D=$W/user/data
sfo CUSA31001 gd "Rennspiel"
printf 'xx\001NPWR31001_00\000yy\001NPWR31002_00\000zz' > $W/system_data/priv/appmeta/CUSA31001/npbind.dat
pj FAKE71001 0 "Playden"
pj PPSA71002 0 "BlackBox"
pj PPSA71003 0 "ProsperoStore"
pj PPSA71004 0 "ProsperoEden Switch UI"
pj SLIB71005 65536 "Spectrum Library"
pj ORBT71006 65536 "Orbit Store" "http://127.0.0.1:12801"   # 1.0.8 (07.11.2026): wie das echte Orbit Store (ORBT00001) eine App (65536)
printf 'xx\001NPWR31009_00\000' > $W/system_data/priv/appmeta/PPSA71002/npbind.dat
# Daten, Ordner, Dateien
for o in playden playden-games playden-import blackbox-v2 prosperostore prosperoeden SpectrumLibrary orbit-store play prospero tmp MeinOrdner; do mkdir -p $D/$o; echo inhalt > $D/$o/datei.txt; done
head -c 300000 /dev/urandom > $D/orbit-store/gross.dat   # NEU 1.0.8 (2.6): kleine Kachel, große Daten - der Block zählt
mkdir -p $D/homebrew/FAKE71001 $D/homebrew/PPSA71003; echo x > $D/homebrew/FAKE71001/eboot.bin; echo x > $D/homebrew/PPSA71003/eboot.bin
mkdir -p $W/mnt/ext1/playden/games/Spiel1; echo x > $W/mnt/ext1/playden/games/Spiel1/a.exe
U=$W/mnt/usb0; mkdir -p $U/playden; echo x > $U/playden/b.txt
echo pkg > "$U/Rennspiel - CUSA31001 - v1.00 - EU.pkg"; echo pkg > "$U/Rennspiel - CUSA31001 - v1.04 - EU - Update.pkg"
echo pkg > "$U/Anderes Spiel - CUSA39999 - v1.00 - EU.pkg"
echo elf > $U/orbit_store_v1.2.elf
echo db > $U/ps5cleanup_db_2026-11-01_233441_FW12.00_CFI-7021_B01Y_appinfo.db; echo db > $U/ps5cleanup_db_2026-11-01_233441_FW12.00_CFI-7021_B01Y_app.db
# Trophäen: zwei Listen für CUSA31001 (Fortschritt nur für die erste), eine für PPSA71002
for n in NPWR31001_00 NPWR31002_00; do mkdir -p $W/user/trophy/conf/$n; echo t > $W/user/trophy/conf/$n/TROPHY.TRP; done
mkdir -p $W/user/home/1234abcd/trophy/data/NPWR31001_00; echo t > $W/user/home/1234abcd/trophy/data/NPWR31001_00/TROPUSR.DAT
mkdir -p $W/user/trophy2/nobackup/conf/NPWR31009_00; echo t > $W/user/trophy2/nobackup/conf/NPWR31009_00/TROPHY.UCP
# Aufnahmen samt zweiter Fassung (sdr)
mkdir -p $W/user/av_contents/photo/NPXS40087/CUSA31001/aaa $W/user/av_contents/sdr/photo/NPXS40087/CUSA31001/aaa
echo j > $W/user/av_contents/photo/NPXS40087/CUSA31001/aaa/bild.jpg; echo j > $W/user/av_contents/sdr/photo/NPXS40087/CUSA31001/aaa/bild.jpg
mkdir -p $W/user/av_contents/sdr/photo/NPXS40087/PPSA79999/bbb; echo j > $W/user/av_contents/sdr/photo/NPXS40087/PPSA79999/bbb/nur_sdr.jpg
# Codex-Prüfung 40: PKG mit Spielnummer in einem Ordner, der wie eine App heißt (Ordner wird aufgeklappt, weil ein Spiel-Abbild darin liegt)
mkdir -p "$W/mnt/ext1/OrbitStore"; echo pkg > "$W/mnt/ext1/OrbitStore/Rennspiel - CUSA31001 - v1.00 - EU.pkg"; echo img > "$W/mnt/ext1/OrbitStore/Fremd - PPSA31555.exfat"
# Codex-Prüfung 40: anderes Produkt, dessen Name mit dem einer App beginnt (mit Leerzeichen bzw. ohne Trenner)
mkdir -p "$D/Playden Dungeons" "$D/PlaydenDungeons"; echo x > "$D/Playden Dungeons/a.txt"; echo x > "$D/PlaydenDungeons/a.txt"
# Codex-Prüfung 40: zweites Konto hat nur Liste 2 gespielt - sein Fortschritt steht nicht unter dem des ersten Kontos
mkdir -p $W/user/home/5678efgh/trophy/data/NPWR31002_00; echo t > $W/user/home/5678efgh/trophy/data/NPWR31002_00/TROPUSR.DAT
# 03.11.2026: Fortschritt eines Spiels mit nur einer Liste (BlackBox), Fortschritt ohne Liste bei zwei Konten
for u in 1234abcd; do mkdir -p $W/user/home/$u/trophy2/nobackup/data/NPWR31009_00; echo t > $W/user/home/$u/trophy2/nobackup/data/NPWR31009_00/TRPTITLE.DAT; done
for u in 1234abcd 5678efgh; do mkdir -p $W/user/home/$u/trophy2/nobackup/data/NPWR39998_00; echo t > $W/user/home/$u/trophy2/nobackup/data/NPWR39998_00/TRPTITLE.DAT; done
# Codex-Prüfung 62: dieselbe Liste an zwei Orten (trophy/conf und trophy2/nobackup/conf) - ein Eintrag mit zwei Pfaden, trotzdem Kopf
mkdir -p $W/user/trophy/conf/NPWR39997_00 $W/user/trophy2/nobackup/conf/NPWR39997_00 $W/user/home/1234abcd/trophy2/nobackup/data/NPWR39997_00
echo t > $W/user/trophy/conf/NPWR39997_00/TROPHY.TRP; echo t > $W/user/trophy2/nobackup/conf/NPWR39997_00/TROPHY.UCP; echo t > $W/user/home/1234abcd/trophy2/nobackup/data/NPWR39997_00/TRPTITLE.DAT
# Codex-Prüfung 41: Payload eines anderen Tools im App-Ordner, App-Ordner mit Spielnummer im Namen, Titel mit ™, zweite Fassung ohne Original
pj PLDM00001 65536 "Payload Manager" "http://127.0.0.1:12800"   # 1.0.8 (07.11.2026): wie der echte Payload Manager eine App (65536)
# NEU 1.0.8 (PS5-Test 07.11.2026, Spielekompressor PSGC50001): eine Tool-Kachel, die sich bei Sony als Spiel meldet (applicationCategoryType 0)
# steht wie im PS5-Menü unter Spiele - mit ihrem ganzen Block (Daten-Ordner /data/GameCompressor, Programmdatei auf dem USB-Laufwerk)
pj PSGC50001 0 "Spielekompressor" "http://127.0.0.1:5910/"; mkdir -p $D/GameCompressor; echo x > $D/GameCompressor/einstellungen.json; printf '\177ELF' > $U/game-compressor_v1.0.4.elf
echo p > $U/ps5cleanup_protokoll_2026-11-02_210000.txt; echo p > $U/ps5cleanup_protokoll_2026-11-01_120000.txt   # laufendes und altes Protokoll
echo elf > "$W/mnt/ext1/OrbitStore/pldmgr_v0.6.3.elf"
mkdir -p "$W/mnt/ext1/playden/cache-CUSA31001"; echo x > "$W/mnt/ext1/playden/cache-CUSA31001/c.dat"
pj PPSA71007 0 "Lumen™ Forge"; mkdir -p $D/lumen; echo x > $D/lumen/a.txt
mkdir -p $W/user/av_contents/photo/NPXS40087/PPSA71002/bbb $W/user/av_contents/sdr/photo/NPXS40087/PPSA71002/bbb $W/user/av_contents/sdr/photo/NPXS40087/PPSA71002/ccc
echo j > $W/user/av_contents/photo/NPXS40087/PPSA71002/bbb/b.jpg; echo j > $W/user/av_contents/sdr/photo/NPXS40087/PPSA71002/bbb/b.jpg; echo j > $W/user/av_contents/sdr/photo/NPXS40087/PPSA71002/ccc/c.jpg
# NEU 03.11.2026 (da_Chris86 16:15/16:22: jede Kachel bekommt alles, was sie selbst anlegt - allgemein, nicht nur für seine Programme):
# erfundene Apps, damit die Regeln für jede Konsole gelten
pjm() { mkdir -p $W/user/appmeta/$1 $W/system_data/priv/appmeta/$1; if [ -n "$4" ]; then dl=",\"deeplinkUri\":\"$4\""; else dl=""; fi
  printf '{"titleId":"%s","applicationCategoryType":%s%s,"localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"%s"}}}' "$1" "$2" "$dl" "$3" > $W/user/appmeta/$1/param.json; }
pjm LUMI71008 65536 "Lumina Library" "http://127.0.0.1:7577/"      # Kachel ohne App-Ordner mit Verweis auf ein Programm
pjm REST71016 0 "Alter Rest"                                        # Kachel-Infos ohne App-Ordner, ohne Verweis: bleibt Rest
mkdir -p $D/LuminaLibrary; echo x > $D/LuminaLibrary/tile.version; echo elf > $U/LuminaLibrary_2.0.elf
pj SPMP71009 65536 "Spiegel Mount+" "http://127.0.0.1:10109"; echo elf > $U/SpiegelMountPlus_2.0-beta1.elf; mkdir -p $D/spiegelmount; echo x > $D/spiegelmount/config.ini
head -c 5000 /dev/urandom > $W/user/app/SPMP71009/medien.dat   # NEU 1.0.8 (2.6): diese Kachel allein ist größer als die von Orbit Store
pj KIST71010 65536 "Kiste Manager" "http://127.0.0.1:8890"; echo elf > $U/ps5-kiste-manager_v1.0.elf
pj ABLG71011 65536 "Ablage" "http://127.0.0.1:8891"; mkdir -p $D/ablage; echo x > $D/ablage/a.txt; echo log > $D/ablage_startup.log
pj STRN71012 65536 "Stern Laden" "http://127.0.0.1:8892"; pj PPSA71013 0 "Stern Laden"; mkdir -p $D/sternladen
elf() { python3 - "$1" "$2" <<'PY'
import sys
inhalt = b"\x7fELF\x02\x01\x01" + b"\0" * 57 + b"".join(x.encode() + b"\0" for x in sys.argv[2].split("|")) + b"\0" * 64
open(sys.argv[1], "wb").write(inhalt)
PY
}
elf $D/sternladen/stern.elf ".stern-drive|/data/sternladen/state.json"; echo m > $W/mnt/ext1/.stern-drive
pj PPSA71014 0 "Emu Neun"; pj PPSA71015 0 "Disc Neun"; mkdir -p $D/homebrew/PPSA71014 $D/homebrew/PPSA71015
elf $D/homebrew/PPSA71014/eboot.bin "/data/EMU9DATA/bios|/data/emu9.lock|/data/homebrew/PPSA71015|/data/ZWEIDATA/x|/data/garlic/backups"
cp $D/homebrew/PPSA71014/eboot.bin $D/homebrew/PPSA71015/eboot.bin
elf $D/homebrew/PPSA71014/helper.elf "/data/EMU9DATA/games|/data/emu9.lock"
mkdir -p $D/EMU9DATA $D/garlic $D/ZWEIDATA; head -c 300 /dev/urandom > $D/EMU9DATA/Mcd001.ps2; echo x > $D/garlic/a.txt; echo x > $D/ZWEIDATA/a.txt; echo x > $D/emu9.lock
# Codex-Prüfung 56: linke Grenze, "+" beim Namensanfang, einzelne Spielstand-Datei, zwei verschiedene Apps gleichen Namens, Verknüpfung
elf $D/homebrew/PPSA71014/links.elf "/mnt/usb0/data/LINKSDATA/x|/mnt/usb0/user/data/LINKSDATA/y|/data/emu9.sav"; mkdir -p $D/LINKSDATA; echo x > $D/LINKSDATA/a.txt; head -c 50 /dev/urandom > $D/emu9.sav
mkdir -p "$D/SpiegelMount+-cache"; echo x > "$D/SpiegelMount+-cache/c.bin"
pj FTPA71017 65536 "Datei Server" "http://127.0.0.1:2122"; pj FTPB71018 65536 "Datei Server" "http://127.0.0.1:2123"; mkdir -p $D/dateiserver; echo x > $D/dateiserver/a.ini
mkdir -p $D/kisteziel; elf $D/kisteziel/k.elf "/data/SYMDATA/x"; ln -sfn kisteziel $D/kiste-manager; mkdir -p $D/SYMDATA; echo x > $D/SYMDATA/a.txt
# Codex-Prüfung 59: Weg genau über der Grenze zweier Lese-Stücke (1 MB) und eine Datei voller "/data/"-Stellen (Laufzeit)
pj PPSA71019 0 "Grenz App"; mkdir -p $D/homebrew/PPSA71019 $D/GRENZDATA; echo x > $D/GRENZDATA/a.txt
python3 - "$D/homebrew/PPSA71019/grenz.elf" "$D/homebrew/PPSA71019/viel.elf" <<'PY'
import sys
k = b"\x7fELF\x02\x01\x01"
open(sys.argv[1], "wb").write(k + b"\0" * (1048576 - len(k) - 7) + b"\0/data/GRENZDATA/x\0" + b"\0" * 64)
open(sys.argv[2], "wb").write(k + (b"\0/data/" + b"a/" * 300 + b"\0" + b"\0/user/data/x\0/.y\0") * 12000)
PY
# NEU 1.0.8 (Arbeitsplan 2.3): Tools ohne Kachel - Payload-Datei und Daten-Ordner (wie nanoDNS und zftpd auf da_Chris86s PS5), dazu
# ein unbekannter Ordner mit passender Datei, "ps5-" vorne, zwei gleich gut passende Ordner und ein zu kurzer Name
for o in nanodns zftpd Werkbank Werkbank-Archiv Spielwiese alpha alphabeta abc kompass; do mkdir -p $D/$o; echo x > $D/$o/einstellungen.ini; done
for e in nanoDNS_0.4.elf zftpd-ps5-v1.6.0.elf garlic-savemgr_v1.14.elf Werkbank_v2.elf Spielwiese-ps5_v1.elf alpha-beta-tool_v1.elf abc.elf ps5-kompass_v1.0.elf; do printf '\177ELF' > "$U/$e"; done
# NEU 1.0.8 (Arbeitsplan 2.4): das installierte Rennspiel (CUSA31001) liegt zusätzlich als Spieldatei auf dem Stick, ein fremdes Spiel zweimal als Spieldatei
echo img > "$U/CUSA31001.exfat"; mkdir -p $W/mnt/ext1/games; echo img > "$U/Zweimal - PPSA31777.exfat"; echo img > "$W/mnt/ext1/Zweimal - PPSA31777.exfat"
(cd $T; RMDA=1 FAKESM=1 $B/test_bloecke $T 0) | grep -v "^LOG:" || RC=1
# NEU 1.0.8 (Arbeitsplan 2.7): Mitschnitt und PC-Nachbau - dieselbe Suche auf demselben Testbaum, einmal mitgeschnitten, einmal wiedergegeben
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -DMITSCHNITT -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_mitschnitt.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_mitschnitt || { echo BAU FEHLGESCHLAGEN; exit 1; }
echo "== Mitschnitt und PC-Nachbau (Testbaum der Blöcke)"
(cd $T; RMDA=1 FAKESM=1 $B/test_mitschnitt rund $T/w $T/mitschnitt.txt) | grep -v "^LOG:" || RC=1
# NEU 03.11.2026 (da_Chris86 17:51): alte Protokolle auf der Konsole (intern) - Lauf 2 löscht sie, das laufende und fremde Dateien bleiben
P=$D/PS5-Cleanup-Protokolle; mkdir -p $P; for z in 2026-11-03_063508 2026-11-03_145321 2026-11-03_174254; do echo p > $P/ps5cleanup_protokoll_$z.txt; done; echo f > $P/andere.txt
ln -sfn ps5cleanup_protokoll_2026-11-03_063508.txt $P/ps5cleanup_protokoll_2026-11-03_000000.txt
# Lauf 2 mit Selbstschutz: nicht lesbare Einzeldateien in einem Daten-Ordner
mkdir -p $D/prosperoeden/user/cache/native-opengl; head -c 200 /dev/urandom > $D/prosperoeden/user/cache/native-opengl/1570.bin
head -c 200 /dev/urandom > $D/prosperoeden/user/cache/native-opengl/lesbar.bin; chmod 000 $D/prosperoeden/user/cache/native-opengl/1570.bin
(cd $T; RMDA=1 FAKESM=1 $B/test_bloecke $T 1) | grep -v "^LOG:" || RC=1
chmod 644 $D/prosperoeden/user/cache/native-opengl/1570.bin
date "+%Y-%m-%d %H:%M:%S FERTIG"
[ $RC -ne 0 ] && echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"
exit $RC
