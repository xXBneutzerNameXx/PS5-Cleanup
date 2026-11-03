#!/bin/bash
# 1.0.7: Vergleich Eintrag für Eintrag mit 1.0.5 - Bereiche wie 1.0.6 (6 Bereiche); die neue Einteilung prüfen test_download/test_sony/test_bereiche
export PS5C_BEREICHE_106=1
# Schritt 6: Selbstschutz bauen und prüfen (läuft in WSL)
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/s6; E=$KK/test/kern/ergebnis/s6; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis/s6" $B $T $B/ps5
exec > "$K/test/kern/ergebnis/schritt6.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_schritt6.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_schritt6 || { echo BAU FEHLGESCHLAGEN; exit 1; }
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
for f in kern_selbstschutz kern_suche kern_loeschen; do $PS5_PAYLOAD_SDK/bin/prospero-clang -c -O2 -Wall -Wno-missing-field-initializers -I$KK/bau/pcre2-ps5/include -I$KK/bau/cjson $KK/kern/$f.c -o $B/ps5/$f.o || echo "PS5-BAU FEHLER $f"; done
echo "-- verändernde Systemaufrufe im PS5-Bau (muss leer sein):"
$PS5_PAYLOAD_SDK/bin/llvm-nm -u $B/ps5/*.o | grep -w -E "unlink|unlinkat|rmdir|chmod|fchmod|rename|renameat|mkdir|remove|truncate|ftruncate" || echo "(keine)"
norm_out() { tr -d '\r' < "$1" | grep -v '^FEHLERZAHL'; }
norm_baum() { tr -d '\r' < "$1" | awk '$1=="d"{$2=0} $3=="user/data/shadowmount/debug.log"{$2="X"} {print}' | LC_ALL=C sort; }
fehler=0
for v in v1 v2 v3 v4; do
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
done
# NEU 1.1 (U12): leerer Aufnahme-Zwischenordner mit der Launcher-Kennung (auf der echten PS5 gefunden: content_tmp/FAKE00000)
echo; echo "===== U12 Aufnahme-Zwischenordner des Launchers"
for v in v1 v3; do
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
done
echo; echo "ZUSAMMEN: $fehler Fehler"
date "+%Y-%m-%d %H:%M:%S FERTIG"
