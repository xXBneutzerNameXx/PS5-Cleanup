#!/bin/bash
# PS5 Cleanup 1.0.6 – Oberfläche am Fernseher für die PS5 bauen (in WSL mit ps5-payload-sdk)
# Aufruf aus Git-Bash:  wsl.exe -e bash -lc 'bash "/pfad/zu/PS5-Cleanup/1.0.7/app/bauen.sh"'
# Baut drei Fassungen OHNE Löschbefehle (ohne KERN_LOESCHEN, Zugriff immer Simulation) und die Release-Fassung MIT (27.10.2026):
#   ausgabe/release/eboot.elf     Release 1.1: löscht wirklich nach Bestätigung (erst nach Praxistest-Freigabe auf die PS5)
#   ausgabe/schritt9/eboot.elf    Testversion Schritt 9: nur nachschauen (Löschen auch als Simulation gesperrt)
#   ausgabe/simulation/eboot.elf  Schritt 10: Simulation der Bereinigung
#   ausgabe/pruefen/eboot.elf     Prüflauf (auf der PS5 als pruefen.elf) vor Schritt 10: wie schritt9, dazu Schutzentscheidungen, Größen, Auswahl (nur lesen)
set -e
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
CC=$PS5_PAYLOAD_SDK/bin/prospero-clang
NM=$PS5_PAYLOAD_SDK/bin/llvm-nm
mkdir -p "$K/app/ausgabe"
# NEU 02.11.2026 (aus Plan 1.0.8 4a vorgezogen): Marke "sauber durchgelaufen" erst ganz am Ende - start/bauen_release.sh baut nur damit
rm -f "$K/app/ausgabe/_bau_ok"
exec > >(tee "$K/app/ausgabe/_bauen.log") 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
cd "$KK/app"
# NEU 1.0.7 (da_Chris86 31.10.2026 23:57 "wieder nur 1 cpu ausgelastet?"): die vier Fassungen gleichzeitig bauen. Die Schritte je
# Fassung sind unverändert (gleiche Prüfungen, gleicher Abbruch bei Fehlern); jede schreibt ihr Protokoll erst in eine eigene
# Datei, am Ende stehen alle vier in fester Reihenfolge in _bauen.log. Vorher liefen sie nacheinander (der SQLite-Teil je Fassung auf einem Kern).
bau() {
  fassung=$1
  O=$HOME/ps5c-bau/app_$fassung; rm -rf $O; mkdir -p $O "$K/app/ausgabe/$fassung"
  D=""; X=""; [ $fassung = schritt9 ] && D="-DNUR_LESEN"
  [ $fassung = pruefen ] && { D="-DNUR_LESEN -DPRUEFLAUF"; X="pruefen.c"; }
  [ $fassung = release ] && D="-DKERN_LOESCHEN"   # 27.10.2026: Release - löscht wirklich (nach Bestätigung); erst nach da_Chris86s Praxistest-Freigabe auf die PS5
  SD=""; [ $fassung = release ] && SD="spielstand_db.c"   # NEU 1.0.7 (01.11.2026): Spielstand-Liste austragen - eigene SQLite MIT Dateizugriff, nur in der echten Fassung
  # 1.0.7 (31.10.2026): alle Programmteile gleichzeitig übersetzen (alle Kerne) - Ergebnis wie vorher, bricht bei einem Fehler ab
  # NEU 1.0.7 (da_Chris86 31.10.2026 21:50, Punkt 5): Fassungen ohne Löschen mit Schreibsperre (bau/nurlesen.h) - außer plattform_ps5.c
  # (Startordner-Teil, wird über nm unten geprüft). Datenbank-Prüfung (db/) mit SQLite ohne Dateizugriff in allen Fassungen.
  NL=""; [ $fassung != release ] && NL="-include $KK/bau/nurlesen.h"
  export CC D O KK NL
  printf '%s\n' $KK/kern/*.c $KK/db/db_pruefen.c $KK/db/db_sicherung.c $KK/db/db_sqlite.c $KK/bau/cjson/cJSON.c app.c plattform_ps5.c qrcodegen.c $X $SD | xargs -P "$(nproc)" -I{} bash -c \
    'F="$NL"; case "$(basename "{}")" in plattform_ps5.c) F="";; spielstand_db.c) F="-D_BSD_SOURCE -std=gnu11";; app.c|pruefen.c) [ -n "$NL" ] && F="$NL -DNURLESEN_MIT_SDL -DNURLESEN_OHNE_UNISTD";; esac; $CC -c -O2 -Wall -Wno-missing-field-initializers $D $F -I$KK/bau/sdl2/include -I$KK/bau/pcre2-ps5/include -I$KK/bau/cjson "{}" -o $O/$(basename "{}" .c).o'
  if [ $fassung != release ]; then   # NEU 1.0.7: jedes fopen lesend (außer plattform_ps5.c - siehe nm-Prüfung)
    FF=0; for q in $KK/kern/*.c $KK/db/db_pruefen.c $KK/db/db_sicherung.c app.c $X; do
      FX="$NL"; case "$(basename $q)" in app.c|pruefen.c) FX="$NL -DNURLESEN_MIT_SDL -DNURLESEN_OHNE_UNISTD";; esac
      $CC -E $D $FX -I$KK/bau/sdl2/include -I$KK/bau/pcre2-ps5/include -I$KK/bau/cjson "$q" 2>/dev/null | python3 -I $KK/bau/fopen_pruefen.py "$(basename $q)" > /dev/null || { echo "BAU FEHLGESCHLAGEN: fopen nicht lesend in $q ($fassung)"; FF=1; }
    done
    [ $FF = 0 ] && echo "-- fopen in Kern, Datenbank-Prüfung und Oberfläche: nur lesend ($fassung)" || exit 1
  fi
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
    T1b=$($NM -u $(ls $O/*.o | grep -v -E "zugriff_datei.o|plattform_ps5.o|spielstand_db.o") | grep -w -E "$VERB" || true)
    # NEU 1.0.6 (Freigabe da_Chris86 29.10.2026 21:23): plattform_ps5.o löscht bei der Selbstentfernung den EIGENEN Startordner
    # /system_ex/app/PSCL00002 (start/startordner.h) - dort erlaubt sind nur unlink und rmdir (dazu nmount)
    # NEU 02.11.2026 (da_Chris86 16:57/17:23): dazu mkdir - nur die Protokoll-Ordner /data/PS5-Cleanup-Protokolle und /mnt/ext1/PS5-Cleanup-Protokolle
    T1c=$($NM -u $O/plattform_ps5.o | grep -w -E "$VERB" | grep -v -w -E "unlink|rmdir|mkdir" || true)
    echo "   plattform_ps5.o (eigener Startordner, Protokoll-Ordner): $($NM -u $O/plattform_ps5.o | grep -w -E "$VERB|nmount" | tr -s ' ' | tr '\n' ' ')"
    # NEU 1.0.7 (da_Chris86 01.11.2026): spielstand_db.o trägt Spielstände aus Sonys Spielstand-Liste aus (eigene SQLite MIT Dateizugriff:
    # Journal anlegen/löschen; seit 02.11.2026 ohne Sicherung) - nach außen nur spielstand_db_austragen und appinfo_zusatz_reparieren
    echo "   spielstand_db.o (Spielstand-Liste, Red-Dead-Reparatur): $($NM -u $O/spielstand_db.o | grep -w -E "$VERB|open|write|pwrite|fsync" | tr -s ' ' | tr '\n' ' ')"
    SDG=$($NM -g --defined-only $O/spielstand_db.o | awk '{print $3}' | tr '\n' ' ')
    if [ "$SDG" != "appinfo_zusatz_reparieren spielstand_db_austragen " ]; then echo "BAU FEHLGESCHLAGEN: spielstand_db.o zeigt nach außen: $SDG"; rm -f "$K/app/ausgabe/$fassung/eboot.elf"; exit 1; fi
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
  # da_Chris86 02.11.2026: kein Ausschalten mehr (am Ende nur Neu starten / Beenden) - darf in keiner Fassung mehr vorkommen
  # (lief früher über dlsym, nm zeigt es nicht - darum nach dem Namen im Programm suchen)
  T5=$(grep -a -c "sceSystemStateMgrTurnOff" $O/eboot.elf || true)
  echo "-- Ausschalten (sceSystemStateMgrTurnOff) im Programm: $T5 (muss 0 sein)"
  if [ "$T5" != 0 ]; then echo "BAU FEHLGESCHLAGEN: Ausschalten im Programm ($fassung)"; rm -f "$K/app/ausgabe/$fassung/eboot.elf"; exit 1; fi
  # NEU 1.0.7 (01.11.2026): Sonys Löschdienst für Bilder/Videos (über die NID - nm zeigt ihn nicht) und das Austragen aus der Spielstand-Liste
  # (Merkmal: Protokollzeile Eintrag/Einträge ausgetragen - seit 02.11.2026 ohne Sicherung) nur im Release
  T6=$(grep -a -c "qC3QW+AltHs" $O/eboot.elf || true); T7=$(grep -a -c "Eintrag/Eintr" $O/eboot.elf || true)
  echo "-- Löschdienst Mediengalerie / Spielstand-Liste austragen im Programm: $T6 / $T7"
  if [ $fassung != release ] && { [ "$T6" != 0 ] || [ "$T7" != 0 ]; }; then echo "BAU FEHLGESCHLAGEN: Löschdienst/Spielstand-Liste in einer Fassung, die nichts verändern darf ($fassung)"; rm -f "$K/app/ausgabe/$fassung/eboot.elf"; exit 1; fi
  if [ $fassung = release ] && { [ "$T6" = 0 ] || [ "$T7" = 0 ]; }; then echo "BAU FEHLGESCHLAGEN: Löschdienst/Spielstand-Liste fehlt im Release ($fassung)"; rm -f "$K/app/ausgabe/$fassung/eboot.elf"; exit 1; fi
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
           "Spiel-Abbild als Spiel (1.0.6)|[ohne Kachel]" "USB-Stick nur einzeln (1.0.6)|USB-Stick: nur einzeln" "Selbstentfernung (1.0.6)|PS5 Cleanup jetzt auch entfernen" \
           "Bilder/Videos nur über Sony (1.0.7)|nur über Sonys Löschdienst" "Spielstände nur samt Listeneintrag (1.0.7)|nur zusammen mit ihrem Eintrag"; do
    n="${m%%|*}"; t="${m#*|}"; c=$(grep -a -c -F "$t" $O/eboot.elf || true); echo "   $n: $([ "$c" -ge 1 ] && echo 1 || echo FEHLT)"
  done
}
PIDS=""; mkdir -p $HOME/ps5c-bau
for fassung in schritt9 simulation pruefen release; do ( bau $fassung ) > $HOME/ps5c-bau/app_$fassung.bauprotokoll 2>&1 & PIDS="$PIDS $!"; done
RCS=""; for p in $PIDS; do if wait $p; then RCS="$RCS 0"; else RCS="$RCS 1"; fi; done
for fassung in schritt9 simulation pruefen release; do cat $HOME/ps5c-bau/app_$fassung.bauprotokoll; done
case "$RCS" in *1*) echo "BAU FEHLGESCHLAGEN (mindestens eine Fassung, siehe oben):$RCS"; exit 1;; esac
echo "== Probebau Zugriffsschicht MIT Löschbefehlen für die PS5 (nur übersetzen, in KEINE ELF eingebaut)"
$CC -c -O2 -Wall -Wno-missing-field-initializers -DKERN_LOESCHEN -I$KK/bau/pcre2-ps5/include -I$KK/bau/cjson $KK/kern/zugriff_datei.c -o $HOME/ps5c-bau/probe_zugriff_loeschen.o && echo "   übersetzt" || echo "   FEHLER"
$NM -u $HOME/ps5c-bau/probe_zugriff_loeschen.o | grep -w -E "openat|unlinkat|fstatat|fchmodat|fstatfs" | tr -s ' ' | tr '\n' ' '; echo
date "+%Y-%m-%d %H:%M:%S FERTIG"
date "+%Y-%m-%d %H:%M:%S" > "$K/app/ausgabe/_bau_ok"   # NEU 02.11.2026: alle Fassungen sauber gebaut
