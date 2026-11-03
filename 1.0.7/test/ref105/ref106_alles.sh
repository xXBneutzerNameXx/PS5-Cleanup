#!/bin/bash
# 27.10.2026: Referenz aus der korrigierten Windows-Quelle (1.0.6) neu erzeugen. Alte 1.0.5-Ergebnisse vorher gesichert (*_105).
cd "$(dirname "$0")"
exec > ref106_lauf.log 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
for d in ergebnis ergebnis_schutz ergebnis_ablauf; do [ -d ${d}_105 ] || cp -r $d ${d}_105; done
bash bauen.sh; tail -1 ergebnis/_bauen.log | head -1; grep "csc rc" ergebnis/_bauen.log
bash bauen_ablauf.sh; grep "csc rc" ergebnis_ablauf/_bauen.log
bash dl_baum.sh
bash alles.sh;      echo "alles: $(tail -1 ergebnis/_lauf.log)"
bash gen.sh;        echo "gen: $(tail -1 ergebnis/_gen.log)"
bash gen_schutz.sh; echo "schutz: $(tail -1 ergebnis_schutz/_schutz.log)"
bash ablauf_ref.sh; echo "ablauf: $(tail -1 ergebnis_ablauf/_lauf.log)"
# gezielter Baum download_v1: detail, auswahl, komplett (de+en)
export MSYS2_ENV_CONV_EXCL="FAILLIST;FAILDEL;FAKELINKS;DIEON"; export MSYS_NO_PATHCONV=1
H="$(pwd)"; PY="/f/projekt/ComfyUI/python_embeded/python.exe"
for sz in detail auswahl komplett; do for sp in de en; do
  [ $sz = komplett ] && sleep 60
  rm -rf w; cp -r download_v1 w
  env RMDA=1 FAKESM=1 "$PY" -I miniftp4.py w 2199 ftp.log >/dev/null 2>&1 & pid=$!; sleep 1.5
  SPRACHE=$sp timeout 300 ./Ref105.exe $sz 2199 "$(cygpath -w "$H")" > ergebnis/${sp}_${sz}_download_v1.out 2>&1
  kill $pid 2>/dev/null; wait $pid 2>/dev/null
  (cd w && find . -printf '%y %s %P\n' | sort) > ergebnis/${sp}_${sz}_download_v1.baum
  echo "download_v1 $sz $sp: $(grep -c '^DETAIL' ergebnis/${sp}_${sz}_download_v1.out) Einträge, $(grep -c '^FAIL' ergebnis/${sp}_${sz}_download_v1.out) FAIL"
done; done
# 27.10.2026: verschobene ShadowMount-Spiele (Windows 1.0.6) - detail de
bash sm_baum.sh
rm -rf w; cp -r sm_v1 w
env RMDA=1 FAKESM=1 "$PY" -I miniftp4.py w 2199 ftp.log >/dev/null 2>&1 & pid=$!; sleep 1.5
SPRACHE=de timeout 300 ./Ref105.exe detail 2199 "$(cygpath -w "$H")" > ergebnis/de_detail_sm_v1.out 2>&1
kill $pid 2>/dev/null; wait $pid 2>/dev/null
echo "sm_v1 detail: $(grep -c '^DETAIL' ergebnis/de_detail_sm_v1.out) Einträge"
# Unterschied alt (1.0.5) -> neu (1.0.6): darf nur Download-Zeilen und ihre Folgen betreffen
for d in ergebnis ergebnis_schutz ergebnis_ablauf; do diff -r ${d}_105 $d > ref_diff_$d.txt; echo "$d: $(grep -c '^[<>]' ref_diff_$d.txt) geänderte Zeilen"; done
date "+%Y-%m-%d %H:%M:%S FERTIG"
