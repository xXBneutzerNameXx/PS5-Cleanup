#!/bin/bash
# PS5 Cleanup 1.0.7 - Gesamttest SCHNELL (da_Chris86 31.10.2026: "diesmal bei 1.0.7 soll es wirklich so sein").
# Gleiche Tests und gleiche Übersicht (ergebnis/alle_tests.log) wie lauf_alle.sh, aber:
#  1. Arbeitskopie im Linux-Teil von WSL statt direkt auf F: (der Weg WSL -> Windows-Laufwerk ist bei vielen kleinen Dateien langsam)
#  2. alle Testgruppen gleichzeitig, jede mit eigenem HOME (eigene Bau- und Testordner, keine Überschneidung)
#  3. Ergebnisse zurück nach F: (test/kern/ergebnis), Laufzeiten in ergebnis/_zeiten.txt
# lauf_alle.sh bleibt als langsamer Weg erhalten. Läuft in WSL.
Q="/pfad/zu/PS5-Cleanup/1.0.7"
N="$HOME/ps5c-schnell/1.0.7"; H="$HOME/ps5c-schnell/h"
EQ="${EQ:-$Q/test/kern/ergebnis}"; mkdir -p "$EQ"; rm -f "$EQ/alle_tests.fertig"
T0=$(date +%s)
mkdir -p "$N" "$H"
rsync -a --delete --exclude 'app/ausgabe/' --exclude 'start/ausgabe/' --exclude 'test/kern/ergebnis/' --exclude 'untersuchung/' --exclude 'pruefung/' "$Q/" "$N/"
grep -rlZ --include='*.sh' -F "$Q" "$N" | xargs -0 -r sed -i "s#$Q#$N#g"
# die PS5-Oberfläche bindet ../../Quellcode/ps5info_antwort.h ein (liegt neben 1.0.7)
mkdir -p "$N/../Quellcode"; cp "$Q/../Quellcode/ps5info_antwort.h" "$N/../Quellcode/"
E="$N/test/kern/ergebnis"; rm -rf "$E"; mkdir -p "$E"
T1=$(date +%s); echo "Kopie: $((T1-T0)) s" > "$E/_zeiten.txt"
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
GRUPPEN="schritt1 schritt2 schritt3 schritt5 schritt6 entscheidungen u16 sicherheit m2 download sm sony selbst bereiche bloecke zuordnung namen zufall komplett ps4 sauber selbstweg konsole ablauf appbau rueckmeldung"
printf '%s\n' $GRUPPEN | xargs -P "$(nproc)" -I{} bash -c 'lauf {}'
# Übersicht wie lauf_alle.sh
L="$E/alle_tests.log"
echo "Regression (schnell, gleichzeitig) $(date '+%Y-%m-%d %H:%M:%S')" > "$L"
auszug() { grep -a -E "GLEICH|UNTERSCHIED|FEHLERZAHL|ZUSAMMEN|^FAIL|BAU( [A-Za-z_0-9]+)? FEHLGESCHLAGEN|\(keine\)|Aufrufe=|GESCHUETZT|Szenarien|Einzahl|^OK" "$1" | grep -v -E "^OK   (  |Farbe|keine unbelegte|KopplungsText|keine deutschen)"; }   # 1.0.8 (Plan 4a): nicht mehr auf 120 Zeilen gekürzt
for s in schritt1 schritt2 schritt3 schritt5 schritt6 entscheidungen u16 sicherheit m2 download sm sony selbst bereiche bloecke zuordnung namen zufall komplett ps4 sauber rueckmeldung; do
  echo "== $s" >> "$L"; [ -f "$E/$s.log" ] && auszug "$E/$s.log" >> "$L" || echo "(kein Log $E/$s.log)" >> "$L"
done
echo "== selbstweg (Hintergrundteil, PC-Test)" >> "$L"; auszug "$E/selbstweg.log" >> "$L"
echo "== Laufwerks-Texte (gegen laufwerk_108.out: 1.0.5 mit den Kopplungs-Texten von 1.0.8)" >> "$L"; cat "$E/_konsole.erg" >> "$L"
echo "== Bedienabläufe (Vergleich F)" >> "$L"; auszug "$E/ablauf.log" >> "$L"
echo "== PS5-Bau Oberfläche" >> "$L"
grep -a -E "eboot.elf|keine|FERTIG|error|Fehler|^[0-9]+$" "$N/app/ausgabe/_bauen.log" | head -12 >> "$L"
# NEU 02.11.2026 (Codex-Prüfung 41 (4)): Testgruppen, die ihren Rückgabewert sauber weitergeben - ein Fehler dort zählt als FAIL
for s in bloecke zuordnung namen zufall komplett rueckmeldung; do r=$(cat "$E/_$s.rc" 2>/dev/null || echo fehlt); [ "$r" = "0" ] || echo "FAIL Rückgabewert von lauf_$s.sh: $r" >> "$L"; done
# NEU 02.11.2026: Gegenprobe über alle Einzel-Logs und den PS5-Bau (bis 1.0.7 kürzte die Übersicht oben je Gruppe auf 120 Zeilen;
# seit 1.0.8 kürzt sie nicht mehr, die Gegenprobe bleibt als zweite Sicherung)
# NEU 1.0.8 (Codex-Prüfung 76): ein Test, der sich nicht übersetzen lässt, zählt mit ("Test-Bau gescheitert") - bis hier stand das nur oben
TB=$(cat "$E"/*.log "$E/_konsole.erg" 2>/dev/null | grep -a -c -E 'BAU( [A-Za-z_0-9]+)? FEHLGESCHLAGEN')   # Codex-Prüfung 77: auch "BAU SIM FEHLGESCHLAGEN"
echo "== Gegenprobe über alle Einzel-Logs: FAIL $(cat "$E"/*.log 2>/dev/null | grep -a -c '^FAIL'), Test-Bau gescheitert $TB, PS5-Bau gescheitert $(cat "$E/_appbau.konsole" 2>/dev/null | grep -a -c 'BAU FEHLGESCHLAGEN')" >> "$L"
T2=$(date +%s); echo "Gesamt: $((T2-T0)) s" >> "$E/_zeiten.txt"
date "+%Y-%m-%d %H:%M:%S FERTIG" >> "$L"
rsync -a "$E/" "$EQ/"
touch "$EQ/alle_tests.fertig"
# Codex-Prüfung 41 (4): Gesamtlauf endet mit Fehlerstatus, wenn irgendwo ein FAIL steht
[ "$(cat "$E"/*.log 2>/dev/null | grep -a -c '^FAIL')" = "0" ] && [ "$TB" = "0" ]
