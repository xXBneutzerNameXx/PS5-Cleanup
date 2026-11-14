#!/bin/bash
# 1.0.7: Vergleich Eintrag für Eintrag mit 1.0.5 - Bereiche wie 1.0.6 (6 Bereiche); die neue Einteilung prüfen test_download/test_sony/test_bereiche
export PS5C_BEREICHE_106=1
# Schritt 6: Selbstschutz bauen und prüfen (läuft in WSL)
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/s6; E=$KK/test/kern/ergebnis/s6; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis/s6" $B $T $B/ps5
exec > "$K/test/kern/ergebnis/schritt6.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
# 1.0.10 (da_Chris86 12.11.2026: Rechenleistung des Mini-PCs nutzen): alle Übersetzungen gleichzeitig (der Kern selbst wird im Gesamttest
# über bau/gcc_verteiler.py auf alle Kerne verteilt und nur einmal übersetzt), danach Testkonsolen, U12 und Grenzen gleichzeitig - jede mit
# eigenem Arbeitsordner; die Ausgabe kommt danach in der bisherigen Reihenfolge
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
F="-O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN"
FG="-O2 -g -w -DKERN_LOESCHEN -DSUCH_GRENZE=40 -DLEICHT_ORDNER=10"
INC="-I$KK/bau/pcre2-pc/include -I$KK/bau/cjson"; LIB="-L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread"
GB1=$B/test_schritt6_grenze12; GB3=$B/test_schritt6_grenze3; rm -f $GB1 $GB3 $B/test_schritt6
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
gcc $F $INC $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_schritt6.c $LIB -o $B/test_schritt6 > $B/bau6.txt 2>&1 &
gcc $FG -DREST_GRENZE=2 $INC $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_schritt6.c $LIB -o $GB1 > $B/bau6_g1.txt 2>&1 || echo "BAU GRENZE FEHLGESCHLAGEN" >> $B/bau6_g1.txt &
gcc $FG -DLEICHT_GRENZE=12 -DREST_GRENZE=50 $INC $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_schritt6.c $LIB -o $GB3 > $B/bau6_g3.txt 2>&1 || echo "BAU GRENZE FEHLGESCHLAGEN" >> $B/bau6_g3.txt &
for f in kern_selbstschutz kern_suche kern_loeschen; do
  { $PS5_PAYLOAD_SDK/bin/prospero-clang -c -O2 -Wall -Wno-missing-field-initializers -I$KK/bau/pcre2-ps5/include -I$KK/bau/cjson $KK/kern/$f.c -o $B/ps5/$f.o > $B/ps5_$f.txt 2>&1 || echo "PS5-BAU FEHLER $f" >> $B/ps5_$f.txt; } &
done
wait
cat $B/bau6.txt; [ -x $B/test_schritt6 ] || { echo BAU FEHLGESCHLAGEN; exit 1; }
for f in kern_selbstschutz kern_suche kern_loeschen; do cat $B/ps5_$f.txt; done
echo "-- verändernde Systemaufrufe im PS5-Bau (muss leer sein):"
$PS5_PAYLOAD_SDK/bin/llvm-nm -u $B/ps5/*.o | grep -w -E "unlink|unlinkat|rmdir|chmod|fchmod|rename|renameat|mkdir|remove|truncate|ftruncate" || echo "(keine)"
norm_out() { tr -d '\r' < "$1" | grep -v '^FEHLERZAHL'; }
norm_baum() { tr -d '\r' < "$1" | awk '$1=="d"{$2=0} $3=="user/data/shadowmount/debug.log"{$2="X"} {print}' | LC_ALL=C sort; }
fehler=0
TS=$T
variante() {
  local v=$1 T=$TS/$v fehler=0; mkdir -p $T
  echo; echo "===== $v"
  # A: ohne Schutz = 1.0.5
  rm -rf $T/w; cp -r $RF/schutz_$v $T/w
  (cd $T; RMDA=1 FAKESM=1 $B/test_schritt6 detail $T > $E/${v}_detail_aus.out 2>&1)
  if cmp -s <(norm_out $RF/ergebnis_schutz/de_detail_$v.out) <(norm_out $E/${v}_detail_aus.out); then echo "ohne Schutz: Fundliste GLEICH wie 1.0.5 ($(grep -c '^DETAIL' $E/${v}_detail_aus.out))"; else echo "ohne Schutz: Fundliste UNTERSCHIED"; diff <(norm_out $RF/ergebnis_schutz/de_detail_$v.out) <(norm_out $E/${v}_detail_aus.out) | head -6; fehler=$((fehler+1)); fi
  rm -rf $T/w; cp -r $RF/schutz_$v $T/w
  (cd $T; RMDA=1 FAKESM=1 $B/test_schritt6 komplettm2 $T > $E/${v}_komplettm2_aus.out 2>&1)
  (cd $T/w && find . -printf '%y %s %P\n') > $E/${v}_komplettm2_aus.baum
  a=GLEICH; cmp -s <(norm_out $RF/ergebnis_schutz/de_komplettm2_$v.out) <(norm_out $E/${v}_komplettm2_aus.out) || a=UNTERSCHIED
  b=GLEICH; cmp -s <(norm_baum $RF/ergebnis_schutz/de_komplettm2_$v.baum) <(norm_baum $E/${v}_komplettm2_aus.baum) || b=UNTERSCHIED
  echo "ohne Schutz: vollständige Bereinigung Protokoll $a, Endzustand $b wie 1.0.5"
  [ $a$b = GLEICHGLEICH ] || { fehler=$((fehler+1)); diff <(norm_out $RF/ergebnis_schutz/de_komplettm2_$v.out) <(norm_out $E/${v}_komplettm2_aus.out) | head -6; }
  # B: mit Schutz
  rm -rf $T/w; cp -r $RF/schutz_$v $T/w
  (cd $T; RMDA=1 FAKESM=1 $B/test_schritt6 detail $T schutz > $E/${v}_detail_an.out 2>&1)
  grep '^SCHUTZ' $E/${v}_detail_an.out | head -3
  # 27.10.2026 (Anforderungen U24): Geschütztes bleibt SICHTBAR, aber gesperrt - geprüft wird: nichts davon wählbar
  # 1.0.6 (da_Chris86 28.10.2026): geschützt ist nur noch PS5 Cleanup selbst, der Homebrew Launcher nicht mehr
  n=$(grep '^DETAIL' $E/${v}_detail_an.out | grep -E 'PS5Cleanup|MeinCleaner' | grep -c 'gesperrt=False')
  ns=$(grep '^DETAIL' $E/${v}_detail_an.out | grep -E 'PS5Cleanup|MeinCleaner' | grep -c 'gesperrt=True')
  if [ $n = 0 ]; then echo "OK   mit Schutz: nichts von PS5 Cleanup wählbar ($ns sichtbar gesperrt, $(grep -c '^DETAIL' $E/${v}_detail_an.out) Einträge)"; else echo "FAIL mit Schutz: $n wählbare Einträge mit geschützten Teilen"; grep '^DETAIL' $E/${v}_detail_an.out | grep -E 'PS5Cleanup|MeinCleaner' | grep 'gesperrt=False' | cut -c1-200; fehler=$((fehler+1)); fi
  if [ $v != v4 ]; then
    if grep '^DETAIL' $E/${v}_detail_an.out | grep -q 'homebrew/AndereApp'; then echo "OK   fremde Homebrew-App wird einzeln angeboten (Ordner homebrew aufgeklappt)"; else echo "FAIL fremde Homebrew-App fehlt"; fehler=$((fehler+1)); fi
  else
    if grep '^DETAIL' $E/${v}_detail_an.out | grep -E ':: /user/data|homebrew' | grep -q 'gesperrt=False'; then echo "FAIL unsicher: Homebrew-Daten werden angeboten"; grep '^DETAIL' $E/${v}_detail_an.out | grep -E ':: /user/data|homebrew' | grep 'gesperrt=False' | cut -c1-200; fehler=$((fehler+1)); else echo "OK   unsicher: keine Homebrew-Daten wählbar ($(grep '^DETAIL' $E/${v}_detail_an.out | grep -E ':: /user/data|homebrew' | grep -c 'gesperrt=True') sichtbar gesperrt)"; fi
  fi
  rm -rf $T/w; cp -r $RF/schutz_$v $T/w
  (cd $T; RMDA=1 FAKESM=1 $B/test_schritt6 komplettm2 $T schutz > $E/${v}_komplettm2_an.out 2>&1)
  (cd $T/w && find . -printf '%y %s %P\n') > $E/${v}_komplettm2_an.baum
  st=$(grep -c 'SICHERHEITSSTOPP' $E/${v}_komplettm2_an.out)
  echo "mit Schutz: $(grep '^ERG' $E/${v}_komplettm2_an.out), Sicherheitsstopps: $st"
  if [ "$st" = 0 ] && grep -q '^ERG Fehler=0 ' $E/${v}_komplettm2_an.out; then echo "OK   vollständige Bereinigung mit Schutz läuft ohne Fehler und ohne Sicherheitsstopp durch"; else echo "FAIL Bereinigung mit Schutz hatte Fehler/Sicherheitsstopps"; fehler=$((fehler+1)); fi
  python3 $KK/test/kern/pruefe_schutz.py $v $E/${v}_komplettm2_aus.baum $E/${v}_komplettm2_an.baum $E/${v}_detail_an.out > $E/${v}_pruefung.txt
  cat $E/${v}_pruefung.txt; grep -q 'FEHLERZAHL 0' $E/${v}_pruefung.txt || fehler=$((fehler+1))
  # C: absichtlich erzwingen
  rm -rf $T/w; cp -r $RF/schutz_$v $T/w
  (cd $T; RMDA=1 FAKESM=1 $B/test_schritt6 erzwingen $T > $E/${v}_erzwingen.out 2>&1)
  echo "erzwungen: $(grep '^GESCHUETZT' $E/${v}_erzwingen.out), $(grep -c '^OK' $E/${v}_erzwingen.out) OK / $(grep -c '^FAIL' $E/${v}_erzwingen.out) FAIL, Sicherheitsstopps $(grep -c 'SICHERHEITSSTOPP' $E/${v}_erzwingen.out)"
  grep '^FAIL' $E/${v}_erzwingen.out | head -5
  grep -q 'FEHLERZAHL 0' $E/${v}_erzwingen.out || fehler=$((fehler+1))
  echo $fehler > $TS/_f_$v
}
# NEU 1.1 (U12): leerer Aufnahme-Zwischenordner mit der Launcher-Kennung (auf der echten PS5 gefunden: content_tmp/FAKE00000)
u12() {
  local v=$1 T=$TS/u12_$1 fehler=0; mkdir -p $T
  rm -rf $T/w; cp -r $RF/schutz_$v $T/w
  mkdir -p $T/w/user/av_contents/content_tmp/FAKE00000; ids="FAKE00000"
  [ $v = v3 ] && { mkdir -p $T/w/user/av_contents/content_tmp/HBLD00001; ids="FAKE00000|HBLD00001"; }
  (cd $T; RMDA=1 FAKESM=1 $B/test_schritt6 detail $T > $E/u12_${v}_aus.out 2>&1)
  (cd $T; RMDA=1 FAKESM=1 $B/test_schritt6 detail $T schutz > $E/u12_${v}_an.out 2>&1)
  aus=$(grep '^DETAIL' $E/u12_${v}_aus.out | grep 'Leere Aufnahme-Zwischenordner'); an=$(grep '^DETAIL' $E/u12_${v}_an.out | grep 'Leere Aufnahme-Zwischenordner')
  echo "$v ohne Schutz: ${aus%% | basis=*}"; echo "$v mit Schutz:  ${an%% | basis=*}"
  if echo "$aus" | grep -q -E "content_tmp/($ids)"; then echo "OK   ohne Schutz wie 1.0.5: Launcher-Ordner mit in der Liste"; else echo "FAIL ohne Schutz fehlt der Launcher-Ordner"; fehler=$((fehler+1)); fi
  # 1.0.6 (da_Chris86 28.10.2026): der Homebrew Launcher ist nicht mehr geschützt - auch mit Selbstschutz angeboten
  if grep '^DETAIL' $E/u12_${v}_an.out | grep -q -E "content_tmp/($ids)"; then echo "OK   mit Schutz: Launcher-Ordner angeboten wie jeder Rest (1.0.6)"; else echo "FAIL mit Schutz fehlt der Launcher-Ordner"; fehler=$((fehler+1)); fi
  if echo "$an" | grep -q 'content_tmp/PPSA12000'; then echo "OK   mit Schutz: übrige Reste wählbar"; else echo "FAIL mit Schutz: übrige Reste fehlen"; fehler=$((fehler+1)); fi
  (cd $T; RMDA=1 FAKESM=1 $B/test_schritt6 komplettm2 $T schutz > $E/u12_${v}_komplettm2_an.out 2>&1)
  ok=1; for i in ${ids//|/ }; do [ -d $T/w/user/av_contents/content_tmp/$i ] && ok=0; done
  if [ $ok = 1 ] && [ ! -e $T/w/user/av_contents/content_tmp/PPSA12000 ] && grep -q '^ERG Fehler=0 ' $E/u12_${v}_komplettm2_an.out; then echo "OK   vollständige Bereinigung mit Schutz: Launcher-Ordner und PPSA12000 weg, 0 Fehler (1.0.6)"; else echo "FAIL vollständige Bereinigung: $(grep '^ERG' $E/u12_${v}_komplettm2_an.out)"; fehler=$((fehler+1)); fi
  echo $fehler > $TS/_f_u12_$v
}
# NEU 1.0.8 (Arbeitsplan 5.1a, Codex-Prüfung 76): Grenzen der Selbstschutz-Suche (Arbeitsplan 3.7) mit kleinen Grenzen.
# 1.0.10 (PS5-Test 11.11.2026): was nach der Grenze nicht geprüft wird, ist nicht mehr gesperrt - nur gefundene Kopien sind es. Ist die Grenze
# erreicht, wird nur gesperrt, was nicht mehr geprüft wird (große Ordner); kleine Ordner und Spiel-Kopien werden weiter geprüft. Mehr als
# REST_GRENZE große Ordner danach: der ganze Ort. Baum: schutz_v1 plus Ordner, die vor den übrigen an der Reihe sind (0_, 1_, ...).
grenz_baum() {
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w; D=$T/w/user/data
  mkdir -p $D/0_sammlung $D/1_klein $D/2_spielkopie/sce_sys $D/3_gross $D/4_gross
  for i in $(seq 1 50); do echo x > $D/0_sammlung/f$i.txt; done                                   # 50 Einträge: hier wird die Grenze (40) erreicht
  for i in 1 2 3 4; do echo x > $D/1_klein/f$i.txt; done; cp $D/homebrew/PS5Cleanup/eboot.elf $D/1_klein/kopie.elf   # kleiner Ordner mit PS5-Cleanup-Kopie
  echo eboot > $D/2_spielkopie/eboot.bin; echo param > $D/2_spielkopie/sce_sys/param.json         # Spiel-Kopie (sce_sys + eboot.bin)
  # Codex-Prüfung 78: GROSSE Spiel-Kopie (15 Einträge, über LEICHT_ORDNER) mit einer PS5-Cleanup-Kopie oben drin - nach der Grenze wird sie nur
  # über die Ausnahme für Spiel-Kopien geprüft; ohne diese wäre der ganze Ordner gesperrt und die Kopie darin nicht erkannt
  mkdir -p $D/2b_spielkopie/sce_sys; echo eboot > $D/2b_spielkopie/eboot.bin; echo param > $D/2b_spielkopie/sce_sys/param.json
  for i in $(seq 1 12); do echo x > $D/2b_spielkopie/f$i.txt; done; cp $D/homebrew/PS5Cleanup/eboot.elf $D/2b_spielkopie/ps5c.elf
  for i in $(seq 1 20); do echo x > $D/3_gross/f$i.txt; echo x > $D/4_gross/f$i.txt; done        # zwei große Ordner (über LEICHT_ORDNER)
  if [ "$1" = g2 ]; then mkdir -p $D/5_gross; for i in $(seq 1 20); do echo x > $D/5_gross/f$i.txt; done; fi   # dritter großer Ordner: über REST_GRENZE
  if [ "$1" = g3 ]; then rm -rf $D/3_gross $D/4_gross; mkdir -p $D/1b_klein; for i in $(seq 1 9); do echo x > $D/1b_klein/f$i.txt; done; fi   # 5 + 9 > LEICHT_GRENZE 12
}
grenze() {
  local g=$1 T=$TS/grenze_$1; mkdir -p $T
  grenz_baum $g; bin=$GB1; [ $g = g3 ] && bin=$GB3
  if [ -x $bin ]; then (cd $T; RMDA=1 FAKESM=1 $bin detail $T schutz > $E/grenze_$g.out 2>&1); else echo "FEHLT $bin" > $E/grenze_$g.out; fi
}
# alles gleichzeitig: 4 Testkonsolen, U12 (2), die Grenzen (3, nach ihrem Bau)
rm -f $TS/_f_*
for v in v1 v2 v3 v4; do variante $v > $TS/_aus_$v.txt 2>&1 & done
for v in v1 v3; do u12 $v > $TS/_aus_u12_$v.txt 2>&1 & done
for g in g1 g2 g3; do grenze $g & done
wait
for v in v1 v2 v3 v4; do cat $TS/_aus_$v.txt; fehler=$((fehler+$(cat $TS/_f_$v 2>/dev/null || echo 1))); done
echo; echo "===== U12 Aufnahme-Zwischenordner des Launchers"
for v in v1 v3; do cat $TS/_aus_u12_$v.txt; fehler=$((fehler+$(cat $TS/_f_u12_$v 2>/dev/null || echo 1))); done
echo; echo "===== Grenzen der Selbstschutz-Suche (1.0.8, 3.7)"
cat $B/bau6_g1.txt $B/bau6_g3.txt
O1=$E/grenze_g1.out; O2=$E/grenze_g2.out; O3=$E/grenze_g3.out
pr() { if eval "$2"; then echo "OK   Grenze: $1"; else echo "FAIL Grenze: $1"; fehler=$((fehler+1)); fi; }
pr "alle drei Suchen vollständig" "grep -q '^ANZAHL' $O1 && grep -q '^ANZAHL' $O2 && grep -q '^ANZAHL' $O3"
pr "Grenze erreicht - die große Sammlung ist NICHT gesperrt (1.0.10: nicht geprüft = keine Kopie)" "! grep -q -x 'SCHUTZ /user/data/0_sammlung' $O1"
pr "danach große Ordner nicht gesperrt (3_gross, 4_gross; 1.0.10)" "! grep -q -x 'SCHUTZ /user/data/3_gross' $O1 && ! grep -q -x 'SCHUTZ /user/data/4_gross' $O1"
pr "danach nicht der ganze Ort gesperrt" "! grep -q -x 'SCHUTZ /user/data' $O1"
pr "danach PS5-Cleanup-Kopie im kleinen Ordner erkannt" "grep -q -x 'SCHUTZ /user/data/1_klein/kopie.elf' $O1"
pr "danach Spiel-Kopie weiter angeboten" "grep '^DETAIL' $O1 | grep ':: /user/data/2_spielkopie ' | grep -q 'gesperrt=False'"
pr "danach PS5-Cleanup-Kopie in einer großen Spiel-Kopie erkannt (2b_spielkopie, nicht der ganze Ordner gesperrt)" "grep -q -x 'SCHUTZ /user/data/2b_spielkopie/ps5c.elf' $O1 && ! grep -q -x 'SCHUTZ /user/data/2b_spielkopie' $O1"
pr "danach kleiner Ordner weiter angeboten (homebrew/AndereApp)" "grep '^DETAIL' $O1 | grep ':: /user/data/homebrew/AndereApp ' | grep -q 'gesperrt=False'"
pr "die nicht geprüften Ordner sind wählbar (0_sammlung, 3_gross; 1.0.10)" "grep '^DETAIL' $O1 | grep ':: /user/data/0_sammlung ' | grep -q 'gesperrt=False' && grep '^DETAIL' $O1 | grep ':: /user/data/3_gross ' | grep -q 'gesperrt=False'"
pr "zu viele große Ordner - der Ort NICHT gesperrt (1.0.10)" "! grep -q -x 'SCHUTZ /user/data' $O2"
pr "zu viele große Ordner - die Ordner bleiben wählbar (5_gross; 1.0.10)" "grep '^DETAIL' $O2 | grep ':: /user/data/5_gross ' | grep -q 'gesperrt=False'"
pr "Grenze für kleine Ordner - der nächste kleine Ordner nicht gesperrt, wählbar (1b_klein; 1.0.10)" "! grep -q -x 'SCHUTZ /user/data/1b_klein' $O3 && grep '^DETAIL' $O3 | grep ':: /user/data/1b_klein ' | grep -q 'gesperrt=False'"
pr "Grenze für kleine Ordner - die Kopie davor erkannt" "grep -q -x 'SCHUTZ /user/data/1_klein/kopie.elf' $O3"
echo; echo "ZUSAMMEN: $fehler Fehler"
date "+%Y-%m-%d %H:%M:%S FERTIG"
