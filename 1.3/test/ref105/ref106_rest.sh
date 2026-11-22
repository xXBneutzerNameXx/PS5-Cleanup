#!/bin/bash
# 27.10.2026 ~23:25: Fortsetzung von ref106_alles.sh ab den Bedienablaeufen. Der erste Lauf wurde um 22:59 abgebrochen, weil beim
# Neuverbinden der Claude-App alle ueber die Kommandozeile gestarteten Hintergrundprozesse beendet wurden (auch der Waechter).
# alles/gen/schutz waren bis dahin fertig (0 FAIL). Neustart ueber die Kommandozeile (Aufgabenplanung wurde nicht eingerichtet - braucht da_Chris86s Freigabe).
cd "$(dirname "$0")"
exec >> ref106_lauf.log 2>&1
date "+%Y-%m-%d %H:%M:%S Fortsetzung"
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
