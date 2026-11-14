#!/bin/bash
# PS5 Cleanup - Gesamttest SCHNELL (da_Chris86 31.10.2026: "diesmal bei 1.0.7 soll es wirklich so sein").
export PLAYDEN_WEB=zu   # NEU 1.0.10 (12.11.2026): Playden gilt im Test als geschlossen (zugriff_datei.c d_playden_web)
# Gleiche Tests und gleiche Übersicht (ergebnis/alle_tests.log) wie lauf_alle.sh, aber:
#  1. Arbeitskopie im Linux-Teil von WSL statt direkt auf F: (der Weg WSL -> Windows-Laufwerk ist bei vielen kleinen Dateien langsam)
#  2. alle Testgruppen gleichzeitig, jede mit eigenem HOME (eigene Bau- und Testordner, keine Überschneidung)
#  3. Ergebnisse zurück nach F: (test/kern/ergebnis), Laufzeiten in ergebnis/_zeiten.txt
# lauf_alle.sh bleibt als langsamer Weg erhalten. Läuft in WSL.
Q="/pfad/zu/PS5-Cleanup/1.0.10"
N="$HOME/ps5c-schnell/1.0.10"; H="$HOME/ps5c-schnell/h"
# 1.0.10 (da_Chris86 12.11.2026): nie zwei Gesamttests gleichzeitig in denselben Ordnern - ein zweiter wartet, bis der erste fertig ist
mkdir -p "$HOME/ps5c-schnell"; exec 9> "$HOME/ps5c-schnell/.gesamttest.lock"
flock -n 9 || { echo "Ein anderer Gesamttest läuft gerade - warte, bis er fertig ist"; flock 9; }
# NEU 13.11.2026 (da_Chris86: gezielt nachprüfen statt jedes Mal alles): NUR_GRUPPEN="zuordnung privat" startet nur diese Testgruppen
# (Teiltest nach einer Behebung). Ergebnis dann in test/kern/ergebnis_teil, nie in ergebnis - der letzte Gesamttest bleibt unberührt.
# Ohne NUR_GRUPPEN: unverändert alle Gruppen.
if [ -n "$NUR_GRUPPEN" ]; then EQ="${EQ:-$Q/test/kern/ergebnis_teil}"; fi
EQ="${EQ:-$Q/test/kern/ergebnis}"; mkdir -p "$EQ"; rm -f "$EQ/alle_tests.fertig"
T0=$(date +%s)
cpu() { awk '/^cpu /{print $2+$3+$4+$7+$8+$9, $2+$3+$4+$5+$6+$7+$8+$9}' /proc/stat; }   # belegt, gesamt (Takte aller Threads)
C0=$(cpu)
mkdir -p "$N" "$H"
# 1.0.10 (12.11.2026): der Abgleich über den Weg WSL -> Windows-Laufwerk wartet vor allem auf Antworten (rund 13000 Einträge, fast alle in
# test/ und referenz/). Darum gleichzeitig: das Gerüst ohne die Inhalte von test/, test/ref105/ und referenz/, dazu jeder Unterordner dort
# einzeln. Gleiche Ausschlüsse wie bisher; was es im Projekt nicht mehr gibt, wird auch in der Arbeitskopie gelöscht.
RX=(--exclude 'app/ausgabe/' --exclude 'start/ausgabe/' --exclude 'test/kern/ergebnis/' --exclude 'test/kern/ergebnis_teil/' --exclude 'untersuchung/' --exclude 'pruefung/')
mkdir -p "$N/test/ref105" "$N/referenz"
teil() {   # $1 = Pfad relativ zum Projekt, GERUEST = alles außer den einzeln abgeglichenen Inhalten
  if [ "$1" = GERUEST ]; then rsync -a --delete "${RX[@]}" --exclude '/test/*' --exclude '/referenz/*' "$Q/" "$N/"
  elif [ -d "$Q/$1" ]; then x=(); [ "$1" = test/kern ] && x=(--exclude '/ergebnis/' --exclude '/ergebnis_teil/'); rsync -a --delete "${RX[@]}" "${x[@]}" "$Q/$1/" "$N/$1/"
  else rsync -a "$Q/$1" "$N/$1"; fi
}
export Q N; export -f teil; export RXS="${RX[*]}"
{ echo GERUEST; for o in test referenz test/ref105; do ls -A "$Q/$o" | while IFS= read -r x; do [ "$o/$x" = test/ref105 ] || echo "$o/$x"; done; done; } \
  | xargs -d '\n' -P 16 -I{} bash -c 'RX=($RXS); teil "$1"' _ {}
for o in test referenz test/ref105; do   # im Projekt gelöscht -> auch hier weg
  comm -13 <(ls -A "$Q/$o" | LC_ALL=C sort) <(ls -A "$N/$o" | LC_ALL=C sort) | while IFS= read -r x; do rm -rf "$N/$o/$x"; done
done
grep -rlZ --include='*.sh' -F "$Q" "$N" | xargs -0 -r sed -i "s#$Q#$N#g"
# die PS5-Oberfläche bindet ../../Quellcode/ps5info_antwort.h ein (liegt neben dem Versionsordner)
mkdir -p "$N/../Quellcode"; cp "$Q/../Quellcode/ps5info_antwort.h" "$N/../Quellcode/"
E="$N/test/kern/ergebnis"; rm -rf "$E"; mkdir -p "$E"
T1=$(date +%s); echo "Kopie: $((T1-T0)) s" > "$E/_zeiten.txt"
# 1.0.10 (da_Chris86 12.11.2026: Rechenleistung nutzen, keine Mehrfachkompilierungen): "gcc" ist im Gesamttest test/kern/gcc_verteiler.py -
# jede Kern-Datei wird einmal übersetzt (gemeinsamer Zwischenspeicher für alle Gruppen) und alle Übersetzungen teilen sich die Threads
export PS5C_GCC="$(command -v gcc)" PS5C_OBJCACHE="$H/_obj"
rm -rf "$H/_obj" "$H/_bin"; mkdir -p "$H/_obj" "$H/_bin"
printf '#!/bin/sh\nexec python3 -I "%s/test/kern/gcc_verteiler.py" "$@"\n' "$N" > "$H/_bin/gcc"; chmod +x "$H/_bin/gcc"
export PATH="$H/_bin:$PATH"
export N H E
lauf() {
  s=$1; a=$(date +%s); mkdir -p "$H/$s"
  case $s in
    selbstweg) HOME="$H/$s" bash "$N/test/start/lauf_selbstweg.sh" > "$E/_$s.konsole" 2>&1 ;;
    appbau)    HOME="$H/$s" bash "$N/app/bauen.sh" > "$E/_$s.konsole" 2>&1 ;;
    konsole)
      B="$H/$s/bau"; mkdir -p "$B"
      if gcc -O2 -w -I"$N/bau/pcre2-pc/include" -I"$N/bau/cjson" $(ls "$N"/kern/*.c | tr "\n" " ") "$N/bau/cjson/cJSON.c" "$N/test/kern/test_konsole.c" -L"$N/bau/pcre2-pc/lib" -lpcre2-8 -lpthread -o "$B/test_konsole"; then
        "$B/test_konsole" > "$E/konsole.out"
        # 1.0.8 (06.11.2026): gegen test/kern/laufwerk_108.out = Ausgabe von 1.0.5, nur die Kopplungs-Zeilen der Werte 0, 1, 3 bis 6 (Slim/Pro) neu
        if cmp -s <(tr -d '\r' < "$N/test/kern/laufwerk_108.out" | head -n -4) <(head -n -1 "$E/konsole.out"); then echo "GLEICH wie laufwerk_108.out ($(head -n -1 "$E/konsole.out" | wc -l) Zeilen)" > "$E/_konsole.erg"; else echo "UNTERSCHIED" > "$E/_konsole.erg"; fi
        tail -1 "$E/konsole.out" >> "$E/_konsole.erg"
      else echo "BAU FEHLGESCHLAGEN" > "$E/_konsole.erg"; fi ;;
    *) HOME="$H/$s" bash "$N/test/kern/lauf_$s.sh" > "$E/_$s.konsole" 2>&1; echo $? > "$E/_$s.rc" ;;   # Rückgabewert merken (Codex-Prüfung 41)
  esac
  echo "$s: $(( $(date +%s) - a )) s" >> "$E/_zeiten.txt"
}
export -f lauf
GRUPPEN="schritt1 schritt2 schritt3 schritt5 schritt6 entscheidungen u16 sicherheit m2 download sm sony selbst bereiche bloecke zuordnung namen zufall komplett ps4 sauber selbstweg konsole ablauf appbau rueckmeldung fern appdb privat rezept"   # NEU 1.0.9: fern (Bedienung im Browser); NEU 1.0.10: appdb (Datenbank-Übersicht), privat (private Ordner von Programmen), rezept (Rezept lesen, universelle Erkennung)
# 1.0.10 (12.11.2026): die längsten Gruppen des letzten Laufs zuerst starten (neue/unbekannte vorne), damit am Ende keine lange allein läuft
if [ -n "$NUR_GRUPPEN" ]; then for g in $NUR_GRUPPEN; do case " $GRUPPEN " in *" $g "*) ;; *) echo "Unbekannte Testgruppe: $g"; exit 2;; esac; done; GRUPPEN="$NUR_GRUPPEN"; fi
REIHE=$(for g in $GRUPPEN; do d=$(sed -n "s/^$g: \([0-9]*\) s$/\1/p" "$EQ/_zeiten.txt" 2>/dev/null | tail -1); echo "${d:-99999} $g"; done | sort -rn | awk '{print $2}')
printf '%s\n' $REIHE | xargs -P "$(nproc)" -I{} bash -c 'lauf {}' 9>&-   # Sperre nicht an Testprogramme vererben
# Übersicht wie lauf_alle.sh
L="$E/alle_tests.log"
echo "Regression (schnell, gleichzeitig) $(date '+%Y-%m-%d %H:%M:%S')" > "$L"
[ -n "$NUR_GRUPPEN" ] && echo "TEILTEST - nur: $NUR_GRUPPEN (kein Gesamttest)" >> "$L"
auszug() { grep -a -E "GLEICH|UNTERSCHIED|FEHLERZAHL|ZUSAMMEN|^FAIL|BAU( [A-Za-z_0-9]+)? FEHLGESCHLAGEN|\(keine\)|Aufrufe=|GESCHUETZT|Szenarien|Einzahl|^OK" "$1" | grep -v -E "^OK   (  |Farbe|keine unbelegte|KopplungsText|keine deutschen)"; }   # 1.0.8 (Plan 4a): nicht mehr auf 120 Zeilen gekürzt
drin() { [ -z "$NUR_GRUPPEN" ] || case " $NUR_GRUPPEN " in *" $1 "*) return 0;; *) return 1;; esac; }   # Teiltest: nur gewählte Gruppen
for s in schritt1 schritt2 schritt3 schritt5 schritt6 entscheidungen u16 sicherheit m2 download sm sony selbst bereiche bloecke zuordnung namen zufall komplett ps4 sauber rueckmeldung fern appdb privat rezept; do
  drin $s || continue
  echo "== $s" >> "$L"; [ -f "$E/$s.log" ] && auszug "$E/$s.log" >> "$L" || echo "(kein Log $E/$s.log)" >> "$L"
done
drin selbstweg && { echo "== selbstweg (Hintergrundteil, PC-Test)" >> "$L"; auszug "$E/selbstweg.log" >> "$L"; }
drin konsole && { echo "== Laufwerks-Texte (gegen laufwerk_108.out: 1.0.5 mit den Kopplungs-Texten von 1.0.8)" >> "$L"; cat "$E/_konsole.erg" >> "$L"; }
drin ablauf && { echo "== Bedienabläufe (Vergleich F)" >> "$L"; auszug "$E/ablauf.log" >> "$L"; }
drin appbau && { echo "== PS5-Bau Oberfläche" >> "$L"
grep -a -E "eboot.elf|keine|FERTIG|error|Fehler|^[0-9]+$" "$N/app/ausgabe/_bauen.log" | head -12 >> "$L"; }
# NEU 02.11.2026 (Codex-Prüfung 41 (4)): Testgruppen, die ihren Rückgabewert sauber weitergeben - ein Fehler dort zählt als FAIL
for s in bloecke zuordnung namen zufall komplett rueckmeldung fern appdb privat rezept; do drin $s || continue; r=$(cat "$E/_$s.rc" 2>/dev/null || echo fehlt); [ "$r" = "0" ] || echo "FAIL Rückgabewert von lauf_$s.sh: $r" >> "$L"; done
# NEU 02.11.2026: Gegenprobe über alle Einzel-Logs und den PS5-Bau (bis 1.0.7 kürzte die Übersicht oben je Gruppe auf 120 Zeilen;
# seit 1.0.8 kürzt sie nicht mehr, die Gegenprobe bleibt als zweite Sicherung)
# NEU 1.0.8 (Codex-Prüfung 76): ein Test, der sich nicht übersetzen lässt, zählt mit ("Test-Bau gescheitert") - bis hier stand das nur oben
TB=$(cat "$E"/*.log "$E/_konsole.erg" 2>/dev/null | grep -a -c -E 'BAU( [A-Za-z_0-9]+)? FEHLGESCHLAGEN')   # Codex-Prüfung 77: auch "BAU SIM FEHLGESCHLAGEN"
echo "== Gegenprobe über alle Einzel-Logs: FAIL $(cat "$E"/*.log 2>/dev/null | grep -a -c '^FAIL'), Test-Bau gescheitert $TB, PS5-Bau gescheitert $(cat "$E/_appbau.konsole" 2>/dev/null | grep -a -c 'BAU FEHLGESCHLAGEN')" >> "$L"
T2=$(date +%s); echo "Gesamt: $((T2-T0)) s" >> "$E/_zeiten.txt"
# 1.0.10 (12.11.2026): wie gut wurde der Rechner genutzt - belegte Takte aller Threads im Verhältnis zu allen Takten der Laufzeit
read b0 g0 <<< "$C0"; read b1 g1 <<< "$(cpu)"
echo "CPU-Auslastung: $(( (b1-b0)*100 / ((g1-g0) > 0 ? (g1-g0) : 1) )) % von $(nproc) Threads im Mittel über die ganze Laufzeit" >> "$E/_zeiten.txt"
date "+%Y-%m-%d %H:%M:%S FERTIG" >> "$L"
rsync -a "$E/" "$EQ/"
touch "$EQ/alle_tests.fertig"
# Codex-Prüfung 41 (4): Gesamtlauf endet mit Fehlerstatus, wenn irgendwo ein FAIL steht
[ "$(cat "$E"/*.log 2>/dev/null | grep -a -c '^FAIL')" = "0" ] && [ "$TB" = "0" ]
