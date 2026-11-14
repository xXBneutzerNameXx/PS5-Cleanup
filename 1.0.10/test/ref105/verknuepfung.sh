#!/bin/bash
# Schritt 2: Referenz für OhneVerknuepfung (1.0.5) auf root9 und root10w (mit Fake-Verknüpfung user/data/hb)
export MSYS2_ENV_CONV_EXCL="FAILLIST;FAILDEL;FAKELINKS;DIEON;VPFADE"
export MSYS_NO_PATHCONV=1
cd "$(dirname "$0")"
H="$(pwd)"; E="$H/ergebnis"
PY="/f/projekt/ComfyUI/python_embeded/python.exe"
exec > "$E/_verknuepfung.log" 2>&1
bash bauen.sh; cat ergebnis/_bauen.log | head -3
export VPFADE="$(cygpath -w "$H/vpfade.jsonl")"
for root in root9 root10w; do
  rm -rf w; cp -r $root w
  if [ $root = root10w ]; then FL=user/data/hb; else FL=; fi
  env RMDA=1 FAKELINKS=$FL "$PY" -I miniftp4.py w 2199 ftp.log >/dev/null 2>&1 &
  pid=$!; sleep 1.5
  ./Ref105.exe verknuepfung 2199 "$(cygpath -w "$H")" > "$E/verknuepfung_$root.out" 2>&1
  kill $pid; wait $pid 2>/dev/null
  echo "$root: $(grep -c '^V 1' "$E/verknuepfung_$root.out") ja, $(grep -c '^V 0' "$E/verknuepfung_$root.out") nein, $(grep -c '^V AUSNAHME' "$E/verknuepfung_$root.out") Ausnahmen"
done
rm -rf w
