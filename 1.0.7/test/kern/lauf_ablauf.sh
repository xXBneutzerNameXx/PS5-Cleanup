#!/bin/bash
# 1.0.7: Vergleich Eintrag für Eintrag mit 1.0.5 - Bereiche wie 1.0.6 (6 Bereiche); die neue Einteilung prüfen test_download/test_sony/test_bereiche
export PS5C_BEREICHE_106=1
# Vergleich F: Bedienabläufe (kern/ablauf.c) gegen die Oberfläche von 1.0.5 (läuft in WSL)
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/ab; E=$KK/test/kern/ergebnis/ablauf; RF=$KK/test/ref105; RE=$RF/ergebnis_ablauf
mkdir -p "$K/test/kern/ergebnis/ablauf" $B $T
exec > "$K/test/kern/ergebnis/ablauf.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_ablauf.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_ablauf || { echo BAU FEHLGESCHLAGEN; exit 1; }
# 1.0.5-Protokoll: Uhrzeit weg, nur ab "=== ABLAUF"
norm_ref() { tr -d '\r' < "$1" | sed -E 's/^\[[0-9]{2}:[0-9]{2}:[0-9]{2}\] //' | sed -n '/^=== ABLAUF$/,$p' | grep -v -x -E 'Ende\.|End\.'; }
norm_c() { tr -d '\r' < "$1" | sed -n '/^=== ABLAUF$/,$p'; }
# NEU 1.1 (U11, gewollt): Einzahl "1 Eintrag"/"1 entry" statt "1 Einträge"/"1 entries" wie in 1.0.5.
# Nur diese Stellen werden in der 1.0.5-Ausgabe auf die Einzahl gebracht, alles andere bleibt Zeichen für Zeichen verglichen.
EZ='s/(^|[^0-9.,])1 Einträge/\11 Eintrag/g; s/(^|[^0-9.,])1 entries/\11 entry/g; s/(^|[^0-9.,])1 M\.2 SSD entries/\11 M.2 SSD entry/g'
# 1.0.6 Fix (30.10.2026, gewollt): Schritt 3 der Schluss-Anleitung nennt jetzt genau die Menünamen von Sony
DBT="s/3\\. Controller per USB anstecken und 'Datenbank neu erstellen' wählen\\./3. Controller per USB anstecken: „Cache leeren und Datenbank neu erstellen“ → „Datenbank neu aufbauen“./; s/3\\. Connect the controller via USB and choose 'Rebuild Database'\\./3. Connect the controller via USB: 'Clear Cache and Rebuild Database' → 'Rebuild Database'./"
norm_ez() { sed -E "$EZ" | sed -E "$DBT"; }
ez_zahl=0
# Testumgebung (wie Schritt 5): Das nachgestellte ShadowMount schreibt nach dem Stopp seine debug.log - unter
# Windows mit \r\n, unter Linux mit \n (1 Byte je Zeile). Nach dem Löschen werden darum nur die Byte-Zahlen
# von "shadowmount" und der Kopf von Bereich 6 ausgeblendet (Anzahl der Einträge bleibt verglichen).
norm_sm() { awk '/^FERTIG /{nach=1} nach && /^KOPF \[6\]/{sub(/ · [0-9.,]+ [A-Za-z]+\)/," · N)")} nach && /^  \[.\] shadowmount   /{sub(/   [0-9.,]+ [A-Za-z]+$/,"   N")} {print}'; }
norm_baum() { tr -d '\r' < "$1" | awk '$1=="d"{$2=0} $3=="user/data/shadowmount/debug.log"{$2="X"} {print}' | LC_ALL=C sort; }
f=0; n=0
for s in $KK/test/ablauf/*.txt; do
  name=$(basename $s .txt)
  for baum in root9 gen1; do
    for sp in de en; do
      [ -f "$RE/${sp}_${baum}_${name}.out" ] || continue
      n=$((n+1))
      (echo "SPRACHE $sp"; cat $s) > $T/sz.txt
      rm -rf $T/w; cp -r $RF/$baum $T/w
      (cd $T; RMDA=1 FAKESM=1 timeout 600 $B/test_ablauf $T/sz.txt $T > $E/${sp}_${baum}_${name}.out 2>&1)
      (cd $T/w && find . -printf '%y %s %P\n') > $E/${sp}_${baum}_${name}.baum
      ez_zahl=$((ez_zahl + $(norm_ref "$RE/${sp}_${baum}_${name}.out" | diff - <(norm_ref "$RE/${sp}_${baum}_${name}.out" | norm_ez) | grep -c "^>")))
      a=GLEICH; cmp -s <(norm_ref "$RE/${sp}_${baum}_${name}.out" | norm_ez | norm_sm) <(norm_c $E/${sp}_${baum}_${name}.out | norm_sm) || a=UNTERSCHIED
      b=GLEICH; cmp -s <(norm_baum "$RE/${sp}_${baum}_${name}.baum") <(norm_baum $E/${sp}_${baum}_${name}.baum) || b=UNTERSCHIED
      echo "$baum $name $sp: Ablauf $a, Endzustand $b  ($(grep -c '^DIALOG' $E/${sp}_${baum}_${name}.out) Fragen)"
      if [ $a != GLEICH ]; then f=$((f+1)); diff <(norm_ref "$RE/${sp}_${baum}_${name}.out" | norm_ez | norm_sm) <(norm_c $E/${sp}_${baum}_${name}.out | norm_sm) | head -30; fi
      if [ $b != GLEICH ]; then f=$((f+1)); diff <(norm_baum "$RE/${sp}_${baum}_${name}.baum") <(norm_baum $E/${sp}_${baum}_${name}.baum) | head -10; fi
    done
  done
done
echo "Szenarien: $n"
echo "davon Einzahl-Stellen (U11) in den 1.0.5-Ausgaben angeglichen: $ez_zahl"
# Simulation: dieselben Abläufe mit SIM=1 -> Platte unverändert, überall SIMULATION
echo "== Simulation"
for name in a3_komplett a4_komplett_m2 b1_auswahl_kacheln; do
  for sp in de en; do
    (echo "SPRACHE $sp"; cat $KK/test/ablauf/$name.txt) > $T/sz.txt
    rm -rf $T/w; cp -r $RF/root9 $T/w
    (cd $T/w && find . -printf '%y %s %P\n' | LC_ALL=C sort) > $T/vorher.baum
    (cd $T; RMDA=1 FAKESM=1 SIM=1 timeout 600 $B/test_ablauf $T/sz.txt $T > $E/sim_${sp}_${name}.out 2>&1)
    (cd $T/w && find . -printf '%y %s %P\n' | LC_ALL=C sort) > $T/nachher.baum
    ok=1
    cmp -s $T/vorher.baum $T/nachher.baum || { echo "FAIL $name $sp: Platte verändert"; ok=0; }
    grep -q "^  | SIMULATION" $E/sim_${sp}_${name}.out || { echo "FAIL $name $sp: Abschlussfrage ohne SIMULATION"; ok=0; }
    grep -q "^FERTIG SIMULATION" $E/sim_${sp}_${name}.out || { echo "FAIL $name $sp: Fertig-Meldung ohne SIMULATION"; ok=0; }
    if grep -E "^  (gelöscht|deleted) " $E/sim_${sp}_${name}.out | head -1 | grep -q .; then echo "FAIL $name $sp: Protokoll sagt 'gelöscht'"; ok=0; fi
    grep -q "^STAND SIMULATION" $E/sim_${sp}_${name}.out || { echo "FAIL $name $sp: Statuszeile ohne SIMULATION"; ok=0; }
    grep -q "^> ABLAUF ENDE verbunden=True" $E/sim_${sp}_${name}.out || { echo "FAIL $name $sp: nach der Simulation keine gültige Liste (neue Verbindung + Suche)"; ok=0; }
    if [ $name = b1_auswahl_kacheln ]; then
      # nach der Simulation zeigt die Liste wieder den echten Stand (dieselben Einträge wie vorher)
      awk '/^> ZEIGE/{z++} z==1 && /^  \[/' $E/sim_${sp}_${name}.out | cut -c7- | sed -E 's/  \[gesperrt\]  \{(Geht mit dem Spiel weg.|Goes with the game.)\}//' > $T/l1; awk '/^> ZEIGE/{z++} z==2 && /^  \[/' $E/sim_${sp}_${name}.out | cut -c7- | sed -E 's/  \[gesperrt\]  \{(Geht mit dem Spiel weg.|Goes with the game.)\}//' > $T/l2
      cmp -s $T/l1 $T/l2 && [ -s $T/l1 ] || { echo "FAIL $name $sp: Liste nach der Simulation nicht wie vorher"; ok=0; }
    fi
    [ $ok = 1 ] && echo "OK   $name $sp: Platte unverändert, SIMULATION in Frage/Statuszeile/Fertig, kein 'gelöscht' ($(grep -c -E '^  (würde gelöscht|would be deleted)' $E/sim_${sp}_${name}.out) x 'würde gelöscht')" || f=$((f+1))
  done
done
echo "FEHLERZAHL $f"
date "+%Y-%m-%d %H:%M:%S FERTIG"
