#!/bin/bash
# PS5 Cleanup - Schritt 0: Referenzergebnisse der Windows-Version 1.0.5 einfrieren.
# Läuft gegen Kopien der Testbäume (w/, w2/) - die Vorlagen root9/root10w bleiben unberührt.
export MSYS2_ENV_CONV_EXCL="FAILLIST;FAILDEL;FAKELINKS;DIEON"
export MSYS_NO_PATHCONV=1
cd "$(dirname "$0")"
H="$(pwd)"; E="$H/ergebnis"
PY="/f/projekt/ComfyUI/python_embeded/python.exe"
exec > "$E/_lauf.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
echo "-- Quellcode gegen Tag v1.0.5-beta:"; git -C "$(cygpath -w "$H/../../..")" diff --stat v1.0.5-beta -- Quellcode; echo "(leer = unverändert)"
./Ref105.exe erlaubt pfade.jsonl ergebnis/erlaubt_3004.txt; echo "erlaubt 3004: $(grep -c 1 "$E/erlaubt_3004.txt") erlaubt von $(wc -l < "$E/erlaubt_3004.txt")"
./Ref105.exe erlaubt grenzfaelle.jsonl ergebnis/erlaubt_grenzfaelle.txt; echo "erlaubt Grenzfälle: $(grep -c 1 "$E/erlaubt_grenzfaelle.txt") erlaubt von $(wc -l < "$E/erlaubt_grenzfaelle.txt")"
./Ref105.exe laufwerk > "$E/laufwerk.out" 2>&1; echo "laufwerk: $(grep -c '^OK' "$E/laufwerk.out") OK / $(grep -c '^FAIL' "$E/laufwerk.out") FAIL"
lauf() {  # lauf <name> <szenario> <baum> [ENV=...]
  name="$1"; sz="$2"; root="$3"; shift 3
  for sp in de en; do
    rm -rf w w2; cp -r "$root" w; rm -f faillist.on
    [ -n "$FLAG" ] && touch faillist.on
    env RMDA=1 FAKESM=1 "$@" "$PY" -I miniftp4.py w 2199 ftp.log >/dev/null 2>&1 &
    pid=$!; pid2=""
    if [ "$sz" = andereps5 ]; then cp -r root9 w2; "$PY" -I miniftp4.py w2 2200 ftp2.log >/dev/null 2>&1 & pid2=$!; fi
    sleep 1.5
    SPRACHE=$sp timeout 300 ./Ref105.exe "$sz" 2199 "$(cygpath -w "$H")" > "$E/${sp}_${name}.out" 2>&1
    kill $pid $pid2 2>/dev/null; wait $pid $pid2 2>/dev/null; rm -f faillist.on
    (cd w && find . -printf '%y %s %P\n' | sort) > "$E/${sp}_${name}.baum"
    echo "$name $sp: $(grep -c '^OK' "$E/${sp}_${name}.out") OK / $(grep -c '^FAIL' "$E/${sp}_${name}.out") FAIL, danach $(wc -l < "$E/${sp}_${name}.baum") Einträge"
  done
}
# Liste, Details, Auswahl
lauf detail_root9 detail root9
lauf detail_root10w detail root10w FAKELINKS=user/data/hb
lauf auswahl_root9 auswahl root9
lauf auswahl_root10w auswahl root10w FAKELINKS=user/data/hb
lauf speicher speicher root9
# alle Szenarien aus _test104/alle.sh (ohne die reinen FTP-Fälle ftpname/ftpsrvname/zftpdname/nurpasv/keindaten)
lauf astro astro root9
lauf lizenz lizenz root9
lauf gruppen gruppen root9
lauf k1 k1 root9
lauf kacheln kacheln root9
SMFALL=ok lauf sm_ok sm root9 FAKESM=1
SMFALL=aus lauf sm_aus sm root9 FAKESM=0
SMFALL=aus_mount lauf sm_aus_mount sm root9 FAKESM=0
SMFALL=stumm lauf sm_stumm sm root9 FAKESM=stumm
SMFALL=stopfail lauf sm_stopfail sm root9 FAKESM=1 STOPFAIL=1
SMFALL=trotzdem lauf sm_trotzdem sm root9 FAKESM=0
lauf verweigert verweigert root9 DENY=/user/appmeta
lauf komplett komplett root9
lauf komplettm2 komplettm2 root9
lauf m2einzeln m2einzeln root9
lauf groessen groessen root9
lauf leerordner leerordner root9
lauf abbruch abbruch root9
lauf listefehler listefehler root9 FAILLIST=/user/addcont/PPSA77700
lauf spielfehler spielfehler root9 FAILDEL=/user/app/PPSA77700
# 27.10.2026: Ausloeser verlegt - /user/download gehoert in 1.0.6 zum Spiel (nicht mehr Kat. 5); Abbruch jetzt beim 2. Kat.-5-Eintrag
lauf abriss abriss root9 "DIEON=CHMOD 777 /user/appmeta/PPSA88888"
lauf andereps5 andereps5 root9
FLAG=1 lauf m2listfehler m2listfehler root9 FAILLIST=/mnt/ext1
lauf symlink symlink root10w FAKELINKS=user/data/hb
lauf komplett_root10w komplett root10w FAKELINKS=user/data/hb
lauf komplettm2_root10w komplettm2 root10w FAKELINKS=user/data/hb
rm -rf w w2
date "+%Y-%m-%d %H:%M:%S FERTIG"
