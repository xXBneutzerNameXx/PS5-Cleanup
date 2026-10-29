#!/bin/bash
# PS5 Cleanup 1.0.6 – Oberfläche am Fernseher für die PS5 bauen (in WSL mit ps5-payload-sdk)
# Aufruf aus Git-Bash:  wsl.exe -e bash -lc 'bash "<PS5-App>/app/bauen.sh"'
# Baut drei Fassungen OHNE Löschbefehle (ohne KERN_LOESCHEN, Zugriff immer Simulation) und die Release-Fassung MIT (27.10.2026):
#   ausgabe/release/eboot.elf     Release 1.1: löscht wirklich nach Bestätigung (erst nach Praxistest-Freigabe auf die PS5)
#   ausgabe/schritt9/eboot.elf    Testversion Schritt 9: nur nachschauen (Löschen auch als Simulation gesperrt)
#   ausgabe/simulation/eboot.elf  Schritt 10: Simulation der Bereinigung
#   ausgabe/pruefen/eboot.elf     Prüflauf (auf der PS5 als pruefen.elf) vor Schritt 10: wie schritt9, dazu Schutzentscheidungen, Größen, Auswahl (nur lesen)
set -e
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
K="$(cd "$(dirname "$0")/.." && pwd)"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
CC=$PS5_PAYLOAD_SDK/bin/prospero-clang
NM=$PS5_PAYLOAD_SDK/bin/llvm-nm
mkdir -p "$K/app/ausgabe"
exec > >(tee "$K/app/ausgabe/_bauen.log") 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
cd "$KK/app"
for fassung in schritt9 simulation pruefen release; do
  O=$HOME/ps5c-bau/app_$fassung; rm -rf $O; mkdir -p $O "$K/app/ausgabe/$fassung"
  D=""; X=""; [ $fassung = schritt9 ] && D="-DNUR_LESEN"
  [ $fassung = pruefen ] && { D="-DNUR_LESEN -DPRUEFLAUF"; X="pruefen.c"; }
  [ $fassung = release ] && D="-DKERN_LOESCHEN"   # 27.10.2026: Release - löscht wirklich (nach Bestätigung); erst nach da_Chris86s Praxistest-Freigabe auf die PS5
  for f in $KK/kern/*.c $KK/bau/cjson/cJSON.c app.c plattform_ps5.c $X; do
    $CC -c -O2 -Wall -Wno-missing-field-initializers $D -I$KK/bau/sdl2/include -I$KK/bau/pcre2-ps5/include -I$KK/bau/cjson "$f" -o $O/$(basename "$f" .c).o
  done
  $CC $O/*.o -L$KK/bau/sdl2/lib -L$KK/bau/pcre2-ps5/lib -lSDL2 -lpcre2-8 \
    -lSceVideoOut -lSceAudioOut -lScePad -lSceUserService -lSceSystemService \
    -lSceImeDialog -lSceKeyboard -lSceIpmi -lSceAppInstUtil -lkernel_sys -o $O/eboot.elf
  cp $O/eboot.elf "$K/app/ausgabe/$fassung/eboot.elf"
  echo "== $fassung"
  ls -la "$K/app/ausgabe/$fassung/eboot.elf" | awk '{print $5" Byte"}'
  sha256sum "$K/app/ausgabe/$fassung/eboot.elf" | awk '{print "SHA-256 "$1}'
  echo "-- verändernde Systemaufrufe in Kern und Oberfläche (muss leer sein):"
  VERB="unlink|unlinkat|rmdir|chmod|fchmod|fchmodat|rename|renameat|mkdir|mkdirat|remove|truncate|ftruncate|link|symlink|utimes|futimes"
  T1=$($NM -u $(ls $O/*.o) | grep -w -E "$VERB" || true)
  if [ $fassung = release ]; then echo "${T1:-(keine)}"; echo "(Release: Löschaufrufe gewollt - nur aus kern/zugriff_datei.o erlaubt)"
    T1b=$($NM -u $(ls $O/*.o | grep -v -E "zugriff_datei.o|plattform_ps5.o") | grep -w -E "$VERB" || true)
    # NEU 1.0.6 (Freigabe da_Chris86 29.10.2026 21:23): plattform_ps5.o löscht bei der Selbstentfernung den EIGENEN Startordner
    # /system_ex/app/PSCL00002 (start/startordner.h) - dort erlaubt sind nur unlink und rmdir (dazu nmount)
    T1c=$($NM -u $O/plattform_ps5.o | grep -w -E "$VERB" | grep -v -w -E "unlink|rmdir" || true)
    echo "   plattform_ps5.o (eigener Startordner): $($NM -u $O/plattform_ps5.o | grep -w -E "$VERB|nmount" | tr -s ' ' | tr '\n' ' ')"
    T1b="$T1b$T1c"
    if [ -n "$T1b" ]; then echo "$T1b"; echo "BAU FEHLGESCHLAGEN: verändernder Aufruf außerhalb der Zugriffsschicht ($fassung)"; rm -f "$K/app/ausgabe/$fassung/eboot.elf"; exit 1; fi
  elif [ -n "$T1" ]; then echo "$T1"; echo "BAU FEHLGESCHLAGEN: verändernder Aufruf in Kern/Oberfläche ($fassung)"; rm -f "$K/app/ausgabe/$fassung/eboot.elf"; exit 1; else
  echo "(keine)"; fi
  echo "-- dieselben Aufrufe im fertigen Programm (inkl. SDL2-Bibliothek; erlaubt nur mkdir aus SDL2, siehe Hinweis):"
  T2=$($NM -u $O/eboot.elf | grep -w -E "$VERB" || true)
  echo "${T2:-(keine)}"
  # NEU 26.10.2026 (Codex-Prüfung): Treffer brechen den Bau ab. Einzige Ausnahme: mkdir - stammt aus der SDL2-Bibliothek
  # (SDL_GetPrefPath), unser Code ruft es nicht auf (Prüfung der .o-Dateien oben ist leer).
  T3=$(echo "$T2" | grep -v -w "mkdir" | grep -v "^$" || true)
  if [ $fassung != release ] && [ -n "$T3" ]; then echo "BAU FEHLGESCHLAGEN: unerwarteter verändernder Aufruf im fertigen Programm ($fassung)"; rm -f "$K/app/ausgabe/$fassung/eboot.elf"; exit 1; fi
  # NEU 1.0.6: Sonys Deinstallation, Neustart, Ruhemodus, Ausschalten nur im Release-Bau
  T4=$($NM -u $O/eboot.elf | grep -w -E "sceAppInstUtilAppUnInstall|sceSystemServiceRequestReboot|sceSystemStateMgrTurnOff|sceSystemStateMgrEnterStandby" || true)
  echo "-- Sony-Deinstallation/Neustart/Ruhemodus/Ausschalten im Programm:"; echo "${T4:-(keine)}"
  if [ $fassung != release ] && [ -n "$T4" ]; then echo "BAU FEHLGESCHLAGEN: Sony-Dienst in einer Fassung, die nichts verändern darf ($fassung)"; rm -f "$K/app/ausgabe/$fassung/eboot.elf"; exit 1; fi
  # NEU 30.10.2026: Ausschalten läuft über dlsym (nm zeigt es nicht) - darum zusätzlich nach dem Namen im Programm suchen
  T5=$(grep -a -c "sceSystemStateMgrTurnOff" $O/eboot.elf || true)
  echo "-- Ausschalten (sceSystemStateMgrTurnOff über dlsym) im Programm: $T5"
  if [ $fassung != release ] && [ "$T5" != 0 ]; then echo "BAU FEHLGESCHLAGEN: Ausschalten in einer Fassung, die nichts verändern darf ($fassung)"; rm -f "$K/app/ausgabe/$fassung/eboot.elf"; exit 1; fi
  if [ $fassung = release ] && [ "$T5" = 0 ]; then echo "BAU FEHLGESCHLAGEN: Ausschalten fehlt im Release ($fassung)"; rm -f "$K/app/ausgabe/$fassung/eboot.elf"; exit 1; fi
  echo "-- Löschbefehle der Zugriffsschicht einkompiliert? (1 = nein, nur Fehlerantwort)"
  grep -a -c "nicht eingebaut" $O/zugriff_datei.o || true
  echo "-- Kennung für den Selbstschutz enthalten:"
  grep -c "PS5CLEANUP-ELF-KENNUNG:7c1e5b2a-selbstschutz" $O/eboot.elf
  echo "-- Testversion nur lesen (Text im Programm):"
  grep -c "In dieser Testversion gesperrt" $O/eboot.elf || true
  echo "-- Sicherheitsregeln im fertigen Programm (je 1 = enthalten):"
  for m in "U12 Launcher-Zwischenordner|/user/av_contents/content_tmp/" "U16 laufende Aufnahme|sharevideo_[0-9A-Fa-f]+_hdr" "S3 Lesefehler-Abbruch|Sicherheitsprüfung nicht möglich" \
           "S5 Systembereiche|preinst2|update|dev|proc|app0" "U17 USB-Erweiterung|Spiele darauf gelten als installiert" "Selbstschutz-Kennung|PS5CLEANUP-ELF-KENNUNG:7c1e5b2a" \
           "Jailbreak-Einstieg nur einzeln|Jailbreak-Einstieg - nur einzeln" "Zuordnung eindeutig|mehrere Kacheln" \
           "Spiel-Abbild als Spiel (1.0.6)|[ohne Kachel]" "USB-Stick nur einzeln (1.0.6)|USB-Stick: nur einzeln" "Selbstentfernung (1.0.6)|PS5 Cleanup jetzt auch entfernen"; do
    n="${m%%|*}"; t="${m#*|}"; c=$(grep -a -c -F "$t" $O/eboot.elf || true); echo "   $n: $([ "$c" -ge 1 ] && echo 1 || echo FEHLT)"
  done
done
echo "== Probebau Zugriffsschicht MIT Löschbefehlen für die PS5 (nur übersetzen, in KEINE ELF eingebaut)"
$CC -c -O2 -Wall -Wno-missing-field-initializers -DKERN_LOESCHEN -I$KK/bau/pcre2-ps5/include -I$KK/bau/cjson $KK/kern/zugriff_datei.c -o $HOME/ps5c-bau/probe_zugriff_loeschen.o && echo "   übersetzt" || echo "   FEHLER"
$NM -u $HOME/ps5c-bau/probe_zugriff_loeschen.o | grep -w -E "openat|unlinkat|fstatat|fchmodat|fstatfs" | tr -s ' ' | tr '\n' ' '; echo
date "+%Y-%m-%d %H:%M:%S FERTIG"
