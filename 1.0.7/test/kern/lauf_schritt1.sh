#!/bin/bash
# Schritt 1: Zugriffsschicht, Sitzung, Hilfen bauen und prüfen (läuft in WSL)
K="/pfad/zu/PS5-Cleanup/1.0.7"
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test
ln -sfn "/pfad/zu/PS5-Cleanup/1.0.7" $HOME/ps5c-k
KK=$HOME/ps5c-k   # Pfad ohne Leerzeichen für die Compiler-Aufrufe
E=$KK/test/kern/ergebnis
mkdir -p "$K/test/kern/ergebnis" "$B" "$T"
exec > "$K/test/kern/ergebnis/schritt1.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$KK/kern/basis.c $KK/kern/hilfen.c $KK/kern/zugriff_datei.c $KK/kern/zugriff_sim.c $KK/kern/sitzung.c"
echo "== PC-Testbau (mit KERN_LOESCHEN, nur für Testkopien)"
gcc -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -DKERN_LOESCHEN -I"$K/bau/pcre2-pc/include" -I"$K/bau/cjson" \
   $KERN "$K/bau/cjson/cJSON.c" "$K/test/kern/test_schritt1.c" -L"$K/bau/pcre2-pc/lib" -lpcre2-8 -lpthread -o $B/test_schritt1 || { echo BAU FEHLGESCHLAGEN; exit 1; }
echo "== PS5-Prüfbau (OHNE KERN_LOESCHEN)"
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
CC=$PS5_PAYLOAD_SDK/bin/prospero-clang
mkdir -p $B/ps5; rm -f $B/ps5/*.o
for f in basis hilfen zugriff_datei zugriff_sim sitzung; do
  $CC -c -O2 -Wall -I"$K/bau/pcre2-ps5/include" "$K/kern/$f.c" -o $B/ps5/$f.o || echo "PS5-BAU FEHLER $f"
done
echo "-- verändernde Systemaufrufe im PS5-Bau (muss leer sein):"
$PS5_PAYLOAD_SDK/bin/llvm-nm -u $B/ps5/*.o | grep -w -E "unlink|unlinkat|rmdir|chmod|fchmod|rename|renameat|mkdir|remove|truncate|ftruncate" || echo "(keine)"
echo "-- zum Vergleich im PC-Testbau:"
nm -u $B/test_schritt1 | grep -w -E "unlink|rmdir|chmod|mkdir" | tr '\n' ' '; echo
echo
echo "== 1 Muster: alle Aufrufe der Klasse H aus 1.0.5 nachrechnen"
$B/test_schritt1 muster "$K/test/ref105/spur/ergebnis/spur.jsonl"
echo
echo "== 2 Baum über Zugriffsschicht + Sitzung gegen unabhängige Liste"
for r in root9 root10w; do
  rm -rf $T/$r; cp -r "$K/test/ref105/$r" $T/$r
  if [ $r = root10w ]; then export FAKELINKS=user/data/hb; else unset FAKELINKS; fi
  $B/test_schritt1 baum $T/$r > $E/baum_${r}_c.txt
  python3 "$K/test/kern/erwartet_baum.py" $T/$r > $E/baum_${r}_soll.txt
  if cmp -s $E/baum_${r}_c.txt $E/baum_${r}_soll.txt; then echo "$r: GLEICH ($(wc -l < $E/baum_${r}_c.txt) Einträge)"; else echo "$r: UNTERSCHIED"; diff $E/baum_${r}_soll.txt $E/baum_${r}_c.txt | head -20; fi
done
unset FAKELINKS
echo
echo "== 3 Sitzung: Sonderfälle"
cd $T; $B/test_schritt1 sitzung $T/root9
echo
echo "== 4 Echt gegen Simulation"
export RMDA=1   
rm -rf $T/A $T/B; cp -r "$K/test/ref105/root9" $T/A; cp -r "$K/test/ref105/root9" $T/B
python3 "$K/test/kern/erwartet_baum.py" $T/B > $E/sim_B_vorher.txt
$B/test_schritt1 befehle $T/A "$K/test/kern/befehle1.txt" > $E/befehle_echt.txt
$B/test_schritt1 befehle $T/B "$K/test/kern/befehle1.txt" sim > $E/befehle_sim.txt
python3 "$K/test/kern/erwartet_baum.py" $T/B > $E/sim_B_nachher.txt
python3 "$K/test/kern/erwartet_baum.py" $T/A > $E/echt_A_nachher.txt
sed '/^== BUCH/,$d' $E/befehle_sim.txt > $E/befehle_sim_ohne_buch.txt
if cmp -s $E/befehle_echt.txt $E/befehle_sim_ohne_buch.txt; then echo "Antworten + Baum danach: echt und Simulation GLEICH"; else echo "UNTERSCHIED echt/Simulation:"; diff $E/befehle_echt.txt $E/befehle_sim_ohne_buch.txt | head -30; fi
if cmp -s $E/sim_B_vorher.txt $E/sim_B_nachher.txt; then echo "Simulation hat auf der Platte NICHTS verändert ($(wc -l < $E/sim_B_vorher.txt) Einträge gleich)"; else echo "FEHLER: Simulation hat etwas verändert!"; diff $E/sim_B_vorher.txt $E/sim_B_nachher.txt | head; fi
sed -n '/^== BAUM/,/^== BUCH/p' $E/befehle_echt.txt | grep -v "^==" > $E/echt_A_sitzungssicht.txt
if cmp -s $E/echt_A_sitzungssicht.txt $E/echt_A_nachher.txt; then echo "Echte Löschung: Sicht der Sitzung = tatsächlicher Plattenstand"; else echo "UNTERSCHIED Sitzungssicht/Platte"; diff $E/echt_A_nachher.txt $E/echt_A_sitzungssicht.txt | head; fi
grep -A 40 "^== BUCH" $E/befehle_sim.txt
date "+%Y-%m-%d %H:%M:%S FERTIG"
