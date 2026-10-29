#!/bin/bash
# PS5 Cleanup 1.0.6 - Release-ELF mit eigener Kachel bauen (WSL, ps5-payload-sdk). Nur bauen, nichts auf die PS5.
# Eingebettet: Oberfläche app/ausgabe/release/eboot.elf (löscht nach Bestätigung) + Logo probelauf/sce_sys/icon0.png.
# Ergebnis: start/ausgabe/ps5cleanup.elf
# NEU 1.0.6: "bash bauen_release.sh simulation" bettet die Simulations-Oberfläche ein (kann nichts löschen)
#            -> start/ausgabe/ps5cleanup_simulation.elf (für den Kacheltest)
set -e
V=${1:-release}
case "$V" in release) NAME=ps5cleanup.elf; SIMDEF= ;; simulation) NAME=ps5cleanup_simulation.elf; SIMDEF=-DSIMULATION ;; *) echo "unbekannte Fassung $V"; exit 1 ;; esac
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
K="$(cd "$(dirname "$0")/.." && pwd)"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
CC=$PS5_PAYLOAD_SDK/bin/prospero-clang; NM=$PS5_PAYLOAD_SDK/bin/llvm-nm
O=$HOME/ps5c-bau/kachel_$V; rm -rf $O; mkdir -p $O "$K/start/ausgabe"
exec > >(tee "$K/start/ausgabe/_bauen_$V.log") 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
cp "$KK/app/ausgabe/$V/eboot.elf" $O/oberflaeche.elf
cp "$KK/probelauf/sce_sys/icon0.png" $O/icon0.png
echo "eingebettete Oberfläche ($V): $(sha256sum $O/oberflaeche.elf | cut -d' ' -f1)"
echo "Kachel-Logo: $(sha256sum $O/icon0.png | cut -d' ' -f1) ($(stat -c%s $O/icon0.png) Byte)"
cd "$KK/start"
# NEU 1.0.6 (da_Chris86 29.10.2026 23:08): selbstweg.c - abschließende Selbstentfernung durch den Hintergrundteil
for f in ps5cleanup.c kachel.c start_hbldr.c selbstweg.c fremd/websrv/elfldr.c fremd/websrv/pt.c; do
  $CC -c -O2 -Wall $SIMDEF -Ifremd/websrv -I. -DUI_ELF="\"$O/oberflaeche.elf\"" -DICON_PNG="\"$O/icon0.png\"" "$f" -o $O/$(basename "$f" .c).o
done
# SDK-Issue ps5-payload-dev/sdk#26: -lSceIpmi ausdrücklich VOR -lSceAppInstUtil (Fehler in Sonys rtld)
$CC $O/*.o -lkernel_sys -lSceSystemService -lSceUserService -lSceIpmi -lSceAppInstUtil -o $O/ps5cleanup.elf
cp $O/ps5cleanup.elf "$K/start/ausgabe/$NAME"
echo "== $NAME $(stat -c%s $O/ps5cleanup.elf) Byte  SHA-256 $(sha256sum $O/ps5cleanup.elf | cut -d' ' -f1)"
VERB="nmount|mount|unmount|unlink|unlinkat|rmdir|rename|renameat|truncate|ftruncate|chmod|fchmod|fchmodat|symlink|link|sceAppInstUtilAppUnInstall"
echo "-- verändernde Aufrufe im Start-Teil ohne kachel.o (muss leer sein):"
T=$($NM -u $O/ps5cleanup.o $O/pt.o | grep -w -E "$VERB|mkdir|mkdirat|open|openat|fopen|sceAppInstUtil.*" || true)
# NEU 1.0.6 (Freigabe da_Chris86 29.10.2026 21:23): start_hbldr.o legt den EIGENEN Startordner /system_ex/app/PSCL00002 an
# (startordner.h) - erlaubt sind dort nur nmount (beschreibbar/zurück), mkdir, open, write; nie Löschen/Umbenennen/Deinstallieren
T3=$($NM -u $O/start_hbldr.o | grep -w -E "mount|unmount|unlink|unlinkat|rmdir|rename|renameat|truncate|ftruncate|chmod|fchmod|fchmodat|symlink|link|mkdirat|openat|fopen|sceAppInstUtil.*" || true)
T="$T$T3"
echo "   start_hbldr.o (eigener Startordner): $($NM -u $O/start_hbldr.o | grep -w -E "nmount|mkdir|open|write" | tr -s ' ' | tr '\n' ' ')"
echo "   Startordner-Kennung: $(strings $O/start_hbldr.o | grep -c 'system_ex/app/PSCL00002') x /system_ex/app/PSCL00002, FAKE00000: $(strings $O/start_hbldr.o $O/ps5cleanup.o | grep -c FAKE00000 || true)"
# elfldr.o (websrv) braucht open nur für /dev/deci_std* (Ausgabe-Umleitung, von uns mit stdio=-1 nicht benutzt) - sonst gleich streng
T2=$($NM -u $O/elfldr.o | grep -w -E "$VERB|mkdir|mkdirat|openat|fopen|sceAppInstUtil.*" || true)
T="$T$T2"
echo "   elfldr.o open-Stellen: $(strings $O/elfldr.o | grep -c '/dev/deci_std' || true) x /dev/deci_std"
if [ -n "$T" ]; then echo "$T"; echo "BAU FEHLGESCHLAGEN: verändernder Aufruf im Start-Teil"; rm -f "$K/start/ausgabe/$NAME"; exit 1; fi
echo "(keine)"
echo "-- kachel.o: erlaubt sind nur mkdir/open/write (Kachel-Ordner) und Sonys Installationsdienst; nie Löschen/Umbenennen/Einhängen/Deinstallieren:"
T=$($NM -u $O/kachel.o | grep -w -E "$VERB" || true)
if [ -n "$T" ]; then echo "$T"; echo "BAU FEHLGESCHLAGEN: unerlaubter Aufruf in kachel.o"; rm -f "$K/start/ausgabe/$NAME"; exit 1; fi
$NM -u $O/kachel.o | grep -w -E "mkdir|open|write|sceAppInstUtil[A-Za-z]*|kernel_dynlib_resolve|kernel_dynlib_handle" | tr -s ' ' | tr '\n' ' '; echo
# NEU 1.0.6 (da_Chris86 29.10.2026 23:08): selbstweg.o entfernt NUR die eigene Kachel (Sony) und den eigenen Startordner -
# erlaubt: nmount (beschreibbar/zurück), unlink, rmdir, open (Markierung lesen), Sonys Dienst; nie Anlegen/Schreiben/Umbenennen
echo "-- selbstweg.o ($V):"
if [ "$V" = simulation ]; then
  T=$($NM -u $O/selbstweg.o | grep -w -E "$VERB|mkdir|mkdirat|open|openat|fopen|write|sceAppInstUtil.*" || true)
  if [ -n "$T" ]; then echo "$T"; echo "BAU FEHLGESCHLAGEN: Simulation darf in selbstweg.o nichts verändern"; rm -f "$K/start/ausgabe/$NAME"; exit 1; fi
  echo "   Simulation: kein verändernder Aufruf"
else
  T=$($NM -u $O/selbstweg.o | grep -w -E "$VERB|mkdir|mkdirat|openat|fopen|write" | grep -v -w -E "nmount|unlink|rmdir|sceAppInstUtilAppUnInstall" || true)
  if [ -n "$T" ]; then echo "$T"; echo "BAU FEHLGESCHLAGEN: unerlaubter Aufruf in selbstweg.o"; rm -f "$K/start/ausgabe/$NAME"; exit 1; fi
  echo "   $($NM -u $O/selbstweg.o | grep -w -E "nmount|unlink|rmdir|open|sceAppInstUtil[A-Za-z]*" | tr -s ' ' | tr '\n' ' ')"
  echo "   Pfade: $(strings $O/selbstweg.o | grep -oE "/(system_ex|user|system_data)/[A-Za-z0-9_./]*" | sort -u | tr '\n' ' ')"
fi
echo "-- Titel-Kennungen im Start-Teil (erwartet: PSCL00001, PSCL00002, NPXS40106):"
strings $O/ps5cleanup.o $O/kachel.o $O/start_hbldr.o $O/selbstweg.o | grep -oE "[A-Z]{4}[0-9]{5}" | sort -u | tr '\n' ' '; echo
echo "-- Kachel-Adresse: $(strings $O/kachel.o | grep -c '127.0.0.1:9099') ; Port im Programm 9099"
echo "-- eingebettete Oberfläche ($V):"
printf "   Simulation-Hinweisfeld 'Es wird nichts gelöscht - nur angezeigt.': %s\n" "$(grep -a -c -F 'Es wird nichts gelöscht - nur angezeigt.' $O/ps5cleanup.elf || true)"
printf "   Release-Text 'Gelöscht wird erst, wenn du am Ende ausdrücklich bestätigst.': %s\n" "$(grep -a -c -F 'Gelöscht wird erst, wenn du am Ende ausdrücklich bestätigst.' $O/ps5cleanup.elf || true)"
date "+%Y-%m-%d %H:%M:%S FERTIG"
