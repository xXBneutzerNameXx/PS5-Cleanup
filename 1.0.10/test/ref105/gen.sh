#!/bin/bash
# Schritt 3: große erzeugte Testbäume gen1/gen2 -> Referenz von 1.0.5 (detail, auswahl, komplett, komplettm2)
export MSYS2_ENV_CONV_EXCL="FAILLIST;FAILDEL;FAKELINKS;DIEON"
export MSYS_NO_PATHCONV=1
cd "$(dirname "$0")"
H="$(pwd)"; E="$H/${GEN_ZIEL:-ergebnis}"; mkdir -p "$E"
PY="/f/projekt/ComfyUI/python_embeded/python.exe"
exec > "$E/_gen.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
for g in 1 2; do rm -rf gen$g; "$PY" -I erzeuge_baum.py gen$g $g; done
lauf() {
  name="$1"; sz="$2"; root="$3"; shift 3
  for sp in de en; do
    # viele Datenverbindungen: Windows braucht Zeit, bis die Ports wieder frei sind (sonst "Socketadresse ... nur einmal")
    case "$name" in komplett*) sleep 120;; esac
    rm -rf w; cp -r "$root" w
    env RMDA=1 FAKESM=1 "$@" "$PY" -I miniftp4.py w 2199 ftp.log >/dev/null 2>&1 &
    pid=$!; sleep 1.5
    SPRACHE=$sp timeout 300 ./Ref105.exe "$sz" 2199 "$(cygpath -w "$H")" > "$E/${sp}_${name}.out" 2>&1
    kill $pid 2>/dev/null; wait $pid 2>/dev/null
    (cd w && find . -printf '%y %s %P\n' | sort) > "$E/${sp}_${name}.baum"
    echo "$name $sp: $(grep -c '^DETAIL' "$E/${sp}_${name}.out") Einträge, $(grep -c '^OK' "$E/${sp}_${name}.out") OK / $(grep -c '^FAIL' "$E/${sp}_${name}.out") FAIL, danach $(wc -l < "$E/${sp}_${name}.baum")"
  done
}
lauf detail_gen1 detail gen1
lauf auswahl_gen1 auswahl gen1
lauf komplett_gen1 komplett gen1
lauf komplettm2_gen1 komplettm2 gen1
lauf detail_gen2 detail gen2 FAKELINKS=user/data/etaHEN
lauf auswahl_gen2 auswahl gen2 FAKELINKS=user/data/etaHEN
lauf komplett_gen2 komplett gen2 FAKELINKS=user/data/etaHEN
lauf komplettm2_gen2 komplettm2 gen2 FAKELINKS=user/data/etaHEN
rm -rf w
date "+%Y-%m-%d %H:%M:%S FERTIG"
