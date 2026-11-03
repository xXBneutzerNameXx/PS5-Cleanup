#!/bin/bash
# Schritt 1: alle Aufrufe der Klasse H (Muster, Text, Ergebnis) aus 1.0.5 mitschreiben.
export MSYS2_ENV_CONV_EXCL="FAILLIST;FAILDEL;FAKELINKS;DIEON"
export MSYS_NO_PATHCONV=1
cd "$(dirname "$0")/.."
H="$(pwd)"; E="$H/spur/ergebnis"; mkdir -p "$E"
PY="/f/projekt/ComfyUI/python_embeded/python.exe"
CSC="/c/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe"
FW='C:\Windows\Microsoft.NET\Framework64\v4.0.30319'
Q='C:\pfad\zu\PS5-Cleanup\Quellcode'
exec > "$E/_spur.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
"$CSC" -nologo -target:exe -optimize+ -out:RefSpur.exe -r:"$FW\\System.Web.Extensions.dll" -r:"$FW\\System.Core.dll" \
  "$Q\\Sprache.cs" "$Q\\Ftp.cs" spur\\Sitzung_spur.cs "$Q\\Kern.cs" "$Q\\Suche.cs" "$Q\\Ausfuehren.cs" "$Q\\Feedback.cs" TestKernRef.cs
echo "csc rc=$?"
rm -f "$E/spur.jsonl"
export SPUR="$(cygpath -w "$E/spur.jsonl")"
lauf() {
  name="$1"; sz="$2"; root="$3"; shift 3
  for sp in de en; do
    rm -rf w w2; cp -r "$root" w; rm -f faillist.on
    [ -n "$FLAG" ] && touch faillist.on
    env RMDA=1 FAKESM=1 "$@" "$PY" -I miniftp4.py w 2199 ftp.log >/dev/null 2>&1 &
    pid=$!
    sleep 1.5
    SPRACHE=$sp timeout 300 ./RefSpur.exe "$sz" 2199 "$(cygpath -w "$H")" > "$E/${sp}_${name}.out" 2>&1
    kill $pid 2>/dev/null; wait $pid 2>/dev/null; rm -f faillist.on
    if cmp -s "$E/${sp}_${name}.out" "ergebnis/${sp}_${name}.out"; then g=gleich; else g=ANDERS; fi
    echo "$name $sp: Ausgabe $g wie Referenz, Spur $(wc -l < "$E/spur.jsonl") Zeilen"
  done
}
lauf detail_root9 detail root9
lauf detail_root10w detail root10w FAKELINKS=user/data/hb
lauf auswahl_root9 auswahl root9
lauf auswahl_root10w auswahl root10w FAKELINKS=user/data/hb
lauf speicher speicher root9
lauf astro astro root9
lauf lizenz lizenz root9
lauf gruppen gruppen root9
lauf k1 k1 root9
lauf kacheln kacheln root9
SMFALL=ok lauf sm_ok sm root9 FAKESM=1
lauf komplett komplett root9
lauf komplettm2 komplettm2 root9
lauf m2einzeln m2einzeln root9
lauf leerordner leerordner root9
lauf symlink symlink root10w FAKELINKS=user/data/hb
lauf komplettm2_root10w komplettm2 root10w FAKELINKS=user/data/hb
rm -rf w w2
date "+%Y-%m-%d %H:%M:%S FERTIG"
