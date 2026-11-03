#!/bin/bash
# Entscheidungen vom 25.10.2026 prüfen: A1/A2 beschädigte Namen -> Sicherheitsstopp, A9 ShadowMount in der Simulation
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/ent; E=$KK/test/kern/ergebnis/ent; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis/ent" $B $T
exec > "$K/test/kern/ergebnis/entscheidungen.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_schritt5.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_schritt5 || { echo BAU FEHLGESCHLAGEN; exit 1; }
f=0
echo "== A1/A2 beschädigte Namen"
rm -rf $T/w; cp -r $RF/root9 $T/w
# 1.0.6: unter /user/download werden nur noch Ordner mit Spielnummer angeboten - die beschädigten Namen liegen darum jetzt
# unter /user/data (dort wird weiter jeder Ordner angeboten, der Sicherheitsstopp im Löschcode muss greifen)
mkdir -p "$T/w/user/data/"$'\xff\xfe'"KAPUTT" "$T/w/user/data/ LEERZEICHEN"
echo x > "$T/w/user/data/"$'\xff\xfe'"KAPUTT/a.pkg"; echo x > "$T/w/user/data/ LEERZEICHEN/b.pkg"
(cd $T; RMDA=1 FAKESM=1 SPRACHE=de $B/test_schritt5 komplett $T > $E/beschaedigt.out 2>&1)
grep -a "beschädigte" $E/beschaedigt.out
n=$(grep -a -c "hat beschädigte Zeichen im Namen - nichts gelöscht" $E/beschaedigt.out)
if [ "$n" = 2 ] && [ -f "$T/w/user/data/"$'\xff\xfe'"KAPUTT/a.pkg" ] && [ -f "$T/w/user/data/ LEERZEICHEN/b.pkg" ]; then echo "OK   beide beschädigten Einträge: Sicherheitsstopp im Protokoll, Dateien stehen noch"; else echo "FAIL beschädigte Namen ($n Meldungen)"; f=$((f+1)); fi
if [ -d "$T/w/user/download/PPSA55555" ]; then echo "FAIL normaler Download nicht gelöscht"; f=$((f+1)); else echo "OK   normale Downloads daneben wurden wie bisher gelöscht"; fi
echo "== A9 ShadowMount in der Simulation"
rm -rf $T/w; cp -r $RF/root9 $T/w
(cd $T/w && find . -printf '%y %s %P\n' | LC_ALL=C sort) > $E/sim_vorher.baum
(cd $T; RMDA=1 FAKESM=1 SPRACHE=de SIM=1 SIMKERN=1 $B/test_schritt5 komplett $T > $E/sim_sm.out 2> $E/sim_sm.buch)
(cd $T/w && find . -printf '%y %s %P\n' | LC_ALL=C sort) > $E/sim_nachher.baum
grep "ShadowMount" $E/sim_sm.out | head -4
grep -q "SIMULATION: Hier würde ShadowMount angehalten - es wird nichts angehalten." $E/sim_sm.out && echo "OK   Simulation meldet deutlich: ShadowMount würde angehalten" || { echo "FAIL Hinweis fehlt"; f=$((f+1)); }
grep -q "STOP" $E/sim_sm.buch && { echo "FAIL Stopp-Datei im Buch"; f=$((f+1)); } || echo "OK   keine Stopp-Datei angelegt (auch nicht im Buch)"
cmp -s $E/sim_vorher.baum $E/sim_nachher.baum && echo "OK   Platte nach der Simulation unverändert" || { echo "FAIL Platte verändert"; f=$((f+1)); }
grep -q "ShadowMount ist beendet\|Stopping\|Rückfrage" $E/sim_sm.out && { echo "FAIL echter Stopp-Ablauf gelaufen"; f=$((f+1)); } || echo "OK   kein echter Stopp-Ablauf, keine Rückfrage"
echo "Simulation: $(grep '^ERG' $E/sim_sm.out), Buch $(grep -c '^BUCH' $E/sim_sm.buch) Befehle"
echo "FEHLERZAHL $f"
