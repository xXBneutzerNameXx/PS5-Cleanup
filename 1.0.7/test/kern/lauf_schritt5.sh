#!/bin/bash
# 1.0.7: Vergleich Eintrag für Eintrag mit 1.0.5 - Bereiche wie 1.0.6 (6 Bereiche); die neue Einteilung prüft test_bereiche
export PS5C_BEREICHE_106=1
# Schritt 5: Löschablauf bauen und alle Lösch-Szenarien gegen 1.0.5 vergleichen (läuft in WSL)
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/s5; E=$KK/test/kern/ergebnis/s5; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis/s5" $B $T $B/ps5
exec > "$K/test/kern/ergebnis/schritt5.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_schritt5.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_schritt5 || { echo BAU FEHLGESCHLAGEN; exit 1; }
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
$PS5_PAYLOAD_SDK/bin/prospero-clang -c -O2 -Wall -Wno-missing-field-initializers -I$KK/bau/pcre2-ps5/include -I$KK/bau/cjson $KK/kern/kern_loeschen.c -o $B/ps5/kern_loeschen.o && echo "PS5-Bau kern_loeschen: ok"
echo "-- verändernde Systemaufrufe im PS5-Bau (muss leer sein):"
$PS5_PAYLOAD_SDK/bin/llvm-nm -u $B/ps5/*.o | grep -w -E "unlink|unlinkat|rmdir|chmod|fchmod|rename|renameat|mkdir|remove|truncate|ftruncate" || echo "(keine)"
# Sekunden hängen von der Geschwindigkeit ab. Bei Bäumen mit Fake-Verknüpfung (FAKELINKS) meldet der
# Test-FTP-Server unter Windows für den als Verknüpfung getarnten Ordner dessen NTFS-Größe - die gibt es
# auf Linux nicht; dort werden nur die Byte-Zahlen in GROESSE/Freigeräumt ausgeblendet (Anzahlen bleiben).
norm_out() { tr -d '\r' < "$1" | sed -E 's/Sekunden=[0-9]+/Sekunden=X/' | if [ -n "$FL_NORM" ]; then sed -E 's/(etwa|about|mindestens|at least|Freigeräumt:|Freed:) [0-9.,]+ (KB|MB|GB|Byte|bytes)/\1 N \2/'; else cat; fi; }
# Testumgebung: Ordnergrößen sind unter Windows 0, unter Linux 4096 -> 0. Die Fake-ShadowMount-Zeile im
# debug.log schreibt Python unter Windows mit \r\n, unter Linux mit \n -> Größe dieser einen Datei nicht vergleichen.
norm_baum() { tr -d '\r' < "$1" | awk '$1=="d"{$2=0} $3=="user/data/shadowmount/debug.log"{$2="X"} {print}' | LC_ALL=C sort; }
gleich=0; anders=0
# 1.0.7 (31.10.2026, schneller Gesamttest): jedes Szenario in eigenem Ordner $T/p_<name> und GLEICHZEITIG; Ausgabe je Szenario in
# $T/p_<name>.erg, danach in fester Reihenfolge zusammengesetzt - Ergebnis und Text wie bisher.
NAMEN=""
lauf() {  # lauf <name> <szenario> <baum> <referenzordner> [ENV=...]
  name="$1"; sz="$2"; root="$3"; ref="$4"; shift 4
  TT=$T/p_$name; rm -rf $TT; mkdir -p $TT
  for sp in de en; do
    rm -rf $TT/w $TT/w2 $TT/faillist.on; cp -r $RF/$root $TT/w
    [ -n "$FLAG" ] && touch $TT/faillist.on
    [ "$sz" = andereps5 ] && cp -r $RF/root9 $TT/w2
    (cd $TT; env -u FAKELINKS -u DENY -u FAILLIST -u FAILDEL -u DIEON -u STOPFAIL RMDA=1 FAKESM=1 "$@" SPRACHE=$sp timeout 300 $B/test_schritt5 "$sz" $TT > $E/${sp}_${name}_c.out 2>&1)
    (cd $TT/w && find . -printf '%y %s %P\n') > $E/${sp}_${name}_c.baum
    rm -f $TT/faillist.on
    norm_out $RF/$ref/${sp}_${name}.out > $E/${sp}_${name}_soll.n; norm_out $E/${sp}_${name}_c.out > $E/${sp}_${name}_ist.n
    norm_baum $RF/$ref/${sp}_${name}.baum > $E/${sp}_${name}_sollbaum.n; norm_baum $E/${sp}_${name}_c.baum > $E/${sp}_${name}_istbaum.n
    a="Ausgabe "; if cmp -s $E/${sp}_${name}_soll.n $E/${sp}_${name}_ist.n; then a="${a}GLEICH"; else a="${a}ANDERS"; fi
    b="Endzustand "; if cmp -s $E/${sp}_${name}_sollbaum.n $E/${sp}_${name}_istbaum.n; then b="${b}GLEICH"; else b="${b}ANDERS"; fi
    case "$a$b" in *ANDERS*) echo x >> $TT.anders;; *) echo x >> $TT.gleich;; esac
    echo "$name $sp: $a ($(grep -c '^OK' $E/${sp}_${name}_c.out) OK / $(grep -c '^FAIL' $E/${sp}_${name}_c.out) FAIL), $b ($(wc -l < $E/${sp}_${name}_istbaum.n))"
    case "$a" in *ANDERS) diff $E/${sp}_${name}_soll.n $E/${sp}_${name}_ist.n | head -10;; esac
    case "$b" in *ANDERS) diff $E/${sp}_${name}_sollbaum.n $E/${sp}_${name}_istbaum.n | head -10;; esac
  done > $TT.erg 2>&1
}
starte() { NAMEN="$NAMEN $1"; rm -f $T/p_$1.gleich $T/p_$1.anders; }
starte astro; lauf astro astro root9 ergebnis &
starte lizenz; lauf lizenz lizenz root9 ergebnis &
starte gruppen; lauf gruppen gruppen root9 ergebnis &
starte k1; lauf k1 k1 root9 ergebnis &
starte kacheln; lauf kacheln kacheln root9 ergebnis &
starte sm_ok; SMFALL=ok lauf sm_ok sm root9 ergebnis FAKESM=1 SMFALL=ok &
starte sm_aus; SMFALL=aus lauf sm_aus sm root9 ergebnis FAKESM=0 SMFALL=aus &
starte sm_aus_mount; lauf sm_aus_mount sm root9 ergebnis FAKESM=0 SMFALL=aus_mount &
starte sm_stumm; lauf sm_stumm sm root9 ergebnis FAKESM=stumm SMFALL=stumm &
starte sm_stopfail; lauf sm_stopfail sm root9 ergebnis FAKESM=1 STOPFAIL=1 SMFALL=stopfail &
starte sm_trotzdem; lauf sm_trotzdem sm root9 ergebnis FAKESM=0 SMFALL=trotzdem &
starte verweigert; lauf verweigert verweigert root9 ergebnis DENY=/user/appmeta &
starte komplett; lauf komplett komplett root9 ergebnis &
starte komplettm2; lauf komplettm2 komplettm2 root9 ergebnis &
starte m2einzeln; lauf m2einzeln m2einzeln root9 ergebnis &
starte groessen; lauf groessen groessen root9 ergebnis &
starte leerordner; lauf leerordner leerordner root9 ergebnis &
starte abbruch; lauf abbruch abbruch root9 ergebnis &
starte listefehler; lauf listefehler listefehler root9 ergebnis FAILLIST=/user/addcont/PPSA77700 &
starte spielfehler; lauf spielfehler spielfehler root9 ergebnis FAILDEL=/user/app/PPSA77700 &
# 27.10.2026: Ausloeser wie test/ref105/alles.sh verlegt (/user/download gehoert in 1.0.6 zum Spiel)
starte abriss; lauf abriss abriss root9 ergebnis "DIEON=CHMOD 777 /user/appmeta/PPSA88888" &
starte andereps5; lauf andereps5 andereps5 root9 ergebnis &
starte m2listfehler; FLAG=1 lauf m2listfehler m2listfehler root9 ergebnis FAILLIST=/mnt/ext1 &
starte symlink; lauf symlink symlink root10w ergebnis FAKELINKS=user/data/hb &
starte komplett_root10w; FL_NORM=1 lauf komplett_root10w komplett root10w ergebnis FAKELINKS=user/data/hb &
starte komplettm2_root10w; lauf komplettm2_root10w komplettm2 root10w ergebnis FAKELINKS=user/data/hb &
starte speicher; lauf speicher speicher root9 ergebnis &
# 27.10.2026: Referenz 1.0.6 liegt in ergebnis/ (gen.sh), ergebnis_gen/ ist der alte Stand 1.0.5
starte komplett_gen1; lauf komplett_gen1 komplett gen1 ergebnis &
starte komplettm2_gen1; lauf komplettm2_gen1 komplettm2 gen1 ergebnis &
starte komplett_gen2; FL_NORM=1 lauf komplett_gen2 komplett gen2 ergebnis FAKELINKS=user/data/etaHEN &
starte komplettm2_gen2; lauf komplettm2_gen2 komplettm2 gen2 ergebnis FAKELINKS=user/data/etaHEN &
wait
for n in $NAMEN; do cat $T/p_$n.erg; [ -f $T/p_$n.gleich ] && gleich=$((gleich+$(wc -l < $T/p_$n.gleich))); [ -f $T/p_$n.anders ] && anders=$((anders+$(wc -l < $T/p_$n.anders))); done
echo
echo "== Echt gegen Simulation (Bäume ohne ShadowMount, damit nichts angehalten werden muss)"
for r in root9 gen1; do
  rm -rf $T/basis_$r; cp -r $RF/$r $T/basis_$r; rm -rf $T/basis_$r/user/data/shadowmount $T/basis_$r/mnt/shadowmnt
  (cd $T/basis_$r && find . -printf '%y %s %P\n') | awk '$3!=""' > $E/sim_${r}_vorher.baum
  for sz in komplett komplettm2; do
    rm -rf $T/w; cp -r $T/basis_$r $T/w
    (cd $T; env -u FAKELINKS RMDA=1 FAKESM=0 SPRACHE=de $B/test_schritt5 $sz $T > $E/sim_${r}_${sz}_echt.out 2>&1)
    (cd $T/w && find . -printf '%y %s %P\n') | awk '$3!=""' > $E/sim_${r}_${sz}_echt.baum
    rm -rf $T/w; cp -r $T/basis_$r $T/w
    (cd $T; env -u FAKELINKS RMDA=1 FAKESM=0 SPRACHE=de SIM=1 SIMBAUM=$E/sim_${r}_${sz}_sicht.baum $B/test_schritt5 $sz $T > $E/sim_${r}_${sz}_sim.out 2> $E/sim_${r}_${sz}_sim.buch)
    (cd $T/w && find . -printf '%y %s %P\n') | awk '$3!=""' > $E/sim_${r}_${sz}_simplatte.baum
    x1=ANDERS; cmp -s $E/sim_${r}_${sz}_echt.out $E/sim_${r}_${sz}_sim.out && x1=GLEICH
    x2=ANDERS; cmp -s <(norm_baum $E/sim_${r}_${sz}_echt.baum) <(norm_baum $E/sim_${r}_${sz}_sicht.baum) && x2=GLEICH
    x3=VERAENDERT; cmp -s <(norm_baum $E/sim_${r}_vorher.baum) <(norm_baum $E/sim_${r}_${sz}_simplatte.baum) && x3=UNVERAENDERT
    echo "$r $sz: Protokoll echt/Simulation $x1, Endzustand echt = Sicht der Simulation $x2, Platte nach Simulation $x3, Buch $(grep -c '^BUCH' $E/sim_${r}_${sz}_sim.buch) Befehle"
    [ $x1 = ANDERS ] && diff $E/sim_${r}_${sz}_echt.out $E/sim_${r}_${sz}_sim.out | head -8
    [ $x2 = ANDERS ] && diff <(norm_baum $E/sim_${r}_${sz}_echt.baum) <(norm_baum $E/sim_${r}_${sz}_sicht.baum) | head -8
    case "$x1$x2$x3" in *ANDERS*|*GLEICHVERAENDERT|*ANDERSVERAENDERT) anders=$((anders+1));; *) gleich=$((gleich+1));; esac
  done
done
echo
echo "ZUSAMMEN: $gleich Läufe gleich, $anders Läufe mit Unterschied"
date "+%Y-%m-%d %H:%M:%S FERTIG"
