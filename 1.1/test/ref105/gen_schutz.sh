#!/bin/bash
# Schritt 6: Referenz von 1.0.5 auf den Bäumen mit Launcher/PS5 Cleanup (v1..v4): detail + komplettm2 (Deutsch)
export MSYS2_ENV_CONV_EXCL="FAILLIST;FAILDEL;FAKELINKS;DIEON"
export MSYS_NO_PATHCONV=1
cd "$(dirname "$0")"
H="$(pwd)"; E="$H/ergebnis_schutz"; mkdir -p "$E"
PY="/f/projekt/ComfyUI/python_embeded/python.exe"
exec > "$E/_schutz.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
for v in v1 v2 v3 v4; do rm -rf schutz_$v; "$PY" -I erzeuge_baum.py schutz_$v 1 $v; done
for v in v1 v2 v3 v4; do
  for sz in detail komplettm2; do
    [ $sz = komplettm2 ] && sleep 120
    rm -rf w; cp -r schutz_$v w
    env RMDA=1 FAKESM=1 "$PY" -I miniftp4.py w 2199 ftp.log >/dev/null 2>&1 &
    pid=$!; sleep 1.5
    SPRACHE=de timeout 300 ./Ref105.exe $sz 2199 "$(cygpath -w "$H")" > "$E/de_${sz}_$v.out" 2>&1
    kill $pid 2>/dev/null; wait $pid 2>/dev/null
    (cd w && find . -printf '%y %s %P\n' | sort) > "$E/de_${sz}_$v.baum"
    echo "$v $sz: $(grep -c '^DETAIL' "$E/de_${sz}_$v.out") Einträge, danach $(wc -l < "$E/de_${sz}_$v.baum")"
  done
done
rm -rf w
date "+%Y-%m-%d %H:%M:%S FERTIG"
