#!/bin/bash
# Vergleich F: Referenz der Bedienabläufe von 1.0.5 - jedes Szenario (test/ablauf/*.txt) in de und en
# gegen eine frische Kopie des Testbaums über den Test-FTP-Server.
export MSYS_NO_PATHCONV=1
cd "$(dirname "$0")"
H="$(pwd)"; E="$H/ergebnis_ablauf"; SZ="$H/../ablauf"; mkdir -p "$E"
PY="/f/projekt/ComfyUI/python_embeded/python.exe"
exec > "$E/_lauf.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
lauf() {
  baum="$1"; s="$2"; name="$(basename "$s" .txt)"
  for sp in de en; do
    # viele Datenverbindungen beim Löschen: Windows braucht Zeit, bis die Ports wieder frei sind
    case "$name" in a3*|a4*|b1*|b4*) sleep 120;; *) sleep 20;; esac
    (echo "SPRACHE $sp"; cat "$s") > "$E/_sz.txt"
    rm -rf w; cp -r "$baum" w
    env RMDA=1 FAKESM=1 "$PY" -I miniftp4.py w 2199 ftp.log >/dev/null 2>&1 &
    pid=$!; sleep 1.5
    timeout 900 ./Ref105Ablauf.exe 2199 "$(cygpath -w "$E/_sz.txt")" "$(cygpath -w "$E/${sp}_${baum}_${name}.out")"
    rc=$?
    kill $pid 2>/dev/null; wait $pid 2>/dev/null
    (cd w && find . -printf '%y %s %P\n' | LC_ALL=C sort) > "$E/${sp}_${baum}_${name}.baum"
    echo "$baum $name $sp: rc=$rc $(grep -c '^\[..:..:..\] DIALOG' "$E/${sp}_${baum}_${name}.out") Fragen, $(tail -1 "$E/${sp}_${baum}_${name}.out" | cut -c12-)"
  done
}
for s in "$SZ"/*.txt; do lauf root9 "$s"; done
for s in "$SZ"/a3_komplett.txt "$SZ"/a4_komplett_m2.txt; do lauf gen1 "$s"; done
rm -rf w "$E/_sz.txt"
date "+%Y-%m-%d %H:%M:%S FERTIG"
