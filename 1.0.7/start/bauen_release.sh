#!/bin/bash
# PS5 Cleanup 1.0.6 - Release-ELF mit eigener Kachel bauen (WSL, ps5-payload-sdk). Nur bauen, nichts auf die PS5.
# Eingebettet: Oberfläche app/ausgabe/release/eboot.elf (löscht nach Bestätigung) + Logo probelauf/sce_sys/icon0.png.
# Ergebnis: start/ausgabe/ps5cleanup.elf
# NEU 1.0.6: "bash bauen_release.sh simulation" bettet die Simulations-Oberfläche ein (kann nichts löschen)
#            -> start/ausgabe/ps5cleanup_simulation.elf (für den Kacheltest)
set -e
V=${1:-release}
# NEU 1.0.8 (Arbeitsplan 2.7): "testlauf" bettet die Test-Fassung mit Mitschnitt ein -> ps5cleanup_testlauf.elf (nur für da_Chris86s Abschlusstest)
case "$V" in release) NAME=ps5cleanup.elf; SIMDEF= ;; simulation) NAME=ps5cleanup_simulation.elf; SIMDEF=-DSIMULATION ;; testlauf) NAME=ps5cleanup_testlauf.elf; SIMDEF= ;; *) echo "unbekannte Fassung $V"; exit 1 ;; esac
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
CC=$PS5_PAYLOAD_SDK/bin/prospero-clang; NM=$PS5_PAYLOAD_SDK/bin/llvm-nm
O=$HOME/ps5c-bau/kachel_$V; rm -rf $O; mkdir -p $O "$K/start/ausgabe"
exec > >(tee "$K/start/ausgabe/_bauen_$V.log") 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
# NEU 1.0.6 Fix (da_Chris86 30.10.2026: "wie wir solche Fehler in Zukunft vermeiden"):
# P1 Werkzeuge aktuell? Installierte SDK-Fassung (Datei .ps5cleanup_sdk_version, beim Einrichten geschrieben) gegen die neueste auf GitHub.
#    Nur die Release-Fassung bricht ab; ohne Netz nur ein deutlicher Hinweis.
SDK_IST=$(cat $PS5_PAYLOAD_SDK/.ps5cleanup_sdk_version 2>/dev/null || echo unbekannt)
SDK_NEU=$(curl -s --max-time 10 https://api.github.com/repos/ps5-payload-dev/sdk/releases/latest | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -1)
echo "SDK installiert: $SDK_IST   neueste auf GitHub: ${SDK_NEU:-nicht erreichbar}"
if [ -z "$SDK_NEU" ]; then echo "WARNUNG: neueste SDK-Fassung nicht prüfbar (kein Netz) - vor der Veröffentlichung nachholen!"
elif [ "$SDK_IST" != "$SDK_NEU" ]; then
  echo "SDK VERALTET: installiert $SDK_IST, neu $SDK_NEU - erst aktualisieren (neue Firmwares werden sonst nicht unterstützt)"
  if [ "$V" = release ]; then echo "BAU FEHLGESCHLAGEN: SDK veraltet"; exit 1; fi
fi
# P2 keine Testreste in der Release-Oberfläche (z. B. die zweite, unsichtbare Suche aus der Testzeit, bis 1.0.6)
if [ "$V" = release ] && strings "$KK/app/ausgabe/$V/eboot.elf" | grep -q -E "OHNE_SELBSTSCHUTZ_WIE_105|PRUEFLAUF FERTIG"; then
  echo "BAU FEHLGESCHLAGEN: Testcode (Prüflauf) in der Release-Oberfläche"; exit 1
fi
# NEU 1.0.8 (Arbeitsplan 2.7): Mitschnitt und Bildschirmbilder nie in der Release-Oberfläche - nur in testlauf
if [ "$V" = release ] && grep -a -q "PS5CLEANUP-MITSCHNITT-TESTFASSUNG" "$KK/app/ausgabe/$V/eboot.elf"; then
  echo "BAU FEHLGESCHLAGEN: Mitschnitt der Test-Fassung in der Release-Oberfläche"; exit 1
fi
if [ "$V" = testlauf ] && ! grep -a -q "PS5CLEANUP-MITSCHNITT-TESTFASSUNG" "$KK/app/ausgabe/$V/eboot.elf"; then
  echo "BAU FEHLGESCHLAGEN: Test-Fassung ohne Mitschnitt"; exit 1
fi
echo "Testreste in der Release-Oberfläche: keine"
# NEU 02.11.2026 (aus Plan 1.0.8 4a vorgezogen): ist der letzte Bau der Oberfläche (app/bauen.sh) nicht sauber durchgelaufen, abbrechen -
# nie still eine ältere Oberfläche einpacken
if [ ! -f "$KK/app/ausgabe/_bau_ok" ]; then echo "BAU FEHLGESCHLAGEN: der letzte Bau der Oberfläche (app/bauen.sh) ist nicht sauber durchgelaufen - erst dort den Fehler beheben"; exit 1; fi
# NEU 02.11.2026 (Codex-Prüfung): nur eine Oberfläche einbetten, die neuer ist als jeder Quelltext - sonst könnte nach einem
# gescheiterten app/bauen.sh die alte Oberfläche ins Programm rutschen
ALT=$(find "$KK/app" "$KK/kern" "$KK/db" "$KK/bau" \( -name '*.[ch]' -o -name '*.html' \) -newer "$KK/app/ausgabe/$V/eboot.elf" 2>/dev/null | head -3 | tr '\n' ' ')   # 1.0.9 (Codex-Prüfung 123): auch die eingebettete Browser-Seite
if [ ! -f "$KK/app/ausgabe/$V/eboot.elf" ] || [ -n "$ALT" ]; then echo "BAU FEHLGESCHLAGEN: Oberfläche ($V) fehlt oder ist älter als der Quelltext - erst app/bauen.sh ($ALT)"; exit 1; fi
echo "Oberfläche ($V) neuer als der Quelltext: ja"
cp "$KK/app/ausgabe/$V/eboot.elf" $O/oberflaeche.elf
cp "$KK/probelauf/sce_sys/icon0.png" $O/icon0.png
echo "eingebettete Oberfläche ($V): $(sha256sum $O/oberflaeche.elf | cut -d' ' -f1)"
echo "Kachel-Logo: $(sha256sum $O/icon0.png | cut -d' ' -f1) ($(stat -c%s $O/icon0.png) Byte)"
cd "$KK/start"
# NEU 1.0.6 (da_Chris86 29.10.2026 23:08): selbstweg.c - abschließende Selbstentfernung durch den Hintergrundteil
# NEU 1.0.7 (da_Chris86 31.10.2026 19:28): sw_protokoll.c - Schlusszeilen an die Protokoll-Datei auf dem USB-Stick anhängen
for f in ps5cleanup.c kachel.c start_hbldr.c selbstweg.c sw_protokoll.c fremd/websrv/elfldr.c fremd/websrv/pt.c; do
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
# NEU 1.0.7: sw_protokoll.o hängt nur an die Protokoll-Datei auf dem USB-Stick an (open ohne Anlegen + write). Simulation: gar nichts.
echo "-- sw_protokoll.o ($V):"
if [ "$V" = simulation ]; then
  T=$($NM -u $O/sw_protokoll.o | grep -w -E "$VERB|mkdir|mkdirat|open|openat|fopen|write|sceAppInstUtil.*" || true)
  if [ -n "$T" ]; then echo "$T"; echo "BAU FEHLGESCHLAGEN: Simulation darf in sw_protokoll.o nichts schreiben"; rm -f "$K/start/ausgabe/$NAME"; exit 1; fi
  echo "   Simulation: kein schreibender Aufruf"
else
  T=$($NM -u $O/sw_protokoll.o | grep -w -E "$VERB|mkdir|mkdirat|openat|fopen|sceAppInstUtil.*" || true)
  P=$(strings $O/sw_protokoll.o | grep -oE "/[a-z]+/[A-Za-z0-9_./]*" | sort -u | tr '\n' ' ')
  if [ -n "$T" ]; then echo "$T"; echo "BAU FEHLGESCHLAGEN: unerlaubter Aufruf in sw_protokoll.o"; rm -f "$K/start/ausgabe/$NAME"; exit 1; fi
  echo "   $($NM -u $O/sw_protokoll.o | grep -w -E "open|write|close" | tr -s ' ' | tr '\n' ' ')  Pfade: $P"
fi
echo "-- Titel-Kennungen im Start-Teil (erwartet: PSCL00001, PSCL00002, NPXS40106):"
strings $O/ps5cleanup.o $O/kachel.o $O/start_hbldr.o $O/selbstweg.o | grep -oE "[A-Z]{4}[0-9]{5}" | sort -u | tr '\n' ' '; echo
echo "-- Kachel-Adresse: $(strings $O/kachel.o | grep -c '127.0.0.1:9099') ; Port im Programm 9099"
# da_Chris86 02.11.2026: kein Ausschalten mehr - weder im Hintergrundteil noch in der eingebetteten Oberfläche
AUS=$(grep -a -c "sceSystemStateMgrTurnOff" $O/ps5cleanup.elf || true)
echo "-- Ausschalten im Programm: $AUS (muss 0 sein)"
if [ "$AUS" != 0 ]; then echo "BAU FEHLGESCHLAGEN: Ausschalten im Programm"; rm -f "$K/start/ausgabe/$NAME"; exit 1; fi
echo "-- eingebettete Oberfläche ($V):"
# 06.11.2026 (Arbeitsplan 5.2a): Texte der neuen Oberfläche 1.0.8 - die von 1.0.7 gibt es nicht mehr (zeigten deshalb 0)
printf "   Hinweis der Test-/Simulationsfassung 'diese Fassung speichert es nicht als Datei' (release: 0): %s\n" "$(grep -a -c -F 'diese Fassung speichert es nicht als Datei' $O/ps5cleanup.elf || true)"
printf "   Text der Auswahl 'Gelöscht wird erst nach dem Bestätigen.' (muss da sein): %s\n" "$(grep -a -c -F 'Gelöscht wird erst nach dem Bestätigen.' $O/ps5cleanup.elf || true)"
printf "   Version '1.0.9' im Programm (muss da sein): %s\n" "$(grep -a -c -F '1.0.9' $O/ps5cleanup.elf || true)"   # 08.11.2026: 1.0.9
# NEU 1.0.7 (03.11.2026): Adresse der freiwilligen QR-Rückmeldung (app.c RUECKMELDUNG_ADRESSE) - Platzhalter nur zum Testen, nie veröffentlichen
RM=$(grep -a -o -i -E 'https://[a-z0-9.-]+\.workers\.dev/r' $O/ps5cleanup.elf | sort -u | tr '\n' ' ')
echo "-- Rückmelde-Adresse im Programm: ${RM:-(keine)}"
# NEU 1.0.9 (Update-Hinweis): die einzige Adresse, die die Oberfläche selbst abfragt (nur lesen, beim Start)
UA=$(grep -a -o -E 'https://api\.github\.com/repos/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+/releases/latest' $O/ps5cleanup.elf | sort -u | tr '\n' ' ')
echo "-- Update-Hinweis fragt (nur lesen): ${UA:-(keine)}"
case "$RM" in *EINTRAGEN*) echo "   ACHTUNG: noch der Platzhalter - so NICHT veröffentlichen (Adresse nach dem Einrichten bei Cloudflare in app.c eintragen)";; esac
date "+%Y-%m-%d %H:%M:%S FERTIG"
