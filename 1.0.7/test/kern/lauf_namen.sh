#!/bin/bash
# Test "Namen und Zuordnung von Resten nur aus Metadaten" + "kaputte Zusatzinhalt-Angaben" (PS5-Test 02.11.2026, GTA V / RDR2) - läuft in WSL
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/nam; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/namen.log" 2>&1
set -o pipefail; RC=0
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_namen.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_namen || { echo BAU FEHLGESCHLAGEN; exit 1; }
W=$T/w
rm -rf $T/ref; mkdir -p $T/ref; cp -r $RF/root9 $T/ref/root9; cp $RF/dl_baum.sh $T/ref/; (cd $T/ref; bash dl_baum.sh > /dev/null); rm -rf $W; cp -r $T/ref/download_v1 $W
# PS5-Spiel mit param.json: pj <ID> <Titel> [Zusatz-JSON]
pj() { mkdir -p $W/user/app/$1/sce_sys $W/user/appmeta/$1 $W/system_data/priv/appmeta/$1
  printf '{"titleId":"%s","applicationCategoryType":0%s,"localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"%s"}}}' "$1" "$3" "$2" > $W/user/appmeta/$1/param.json
  echo x > $W/user/app/$1/eboot.bin; }
# param.sfo schreiben: sfo_datei <Ziel> <JSON-Liste [[Schlüssel, Wert], ...]> (Zahl -> 0x0404, Text -> 0x0204)
sfo_datei() { mkdir -p "$(dirname "$1")"; python3 - "$1" "$2" <<'PY'
import json, struct, sys
ziel, eintr = sys.argv[1], json.loads(sys.argv[2])
keys = b""; offs = []
for k, _ in eintr: offs.append(len(keys)); keys += k.encode() + b"\0"
while len(keys) % 4: keys += b"\0"
daten = b""; doffs = []; laengen = []; fmts = []; maxl = []
for _, v in eintr:
    if isinstance(v, int): d = struct.pack("<I", v); fmts.append(0x0404); laengen.append(4); maxl.append(4)
    else: d = v.encode() + b"\0"; fmts.append(0x0204); laengen.append(len(d)); d += b"\0" * ((-len(d)) % 4); maxl.append(len(d))
    doffs.append(len(daten)); daten += d
kt = 20 + 16 * len(eintr); dt = kt + len(keys)
kopf = b"\0PSF" + struct.pack("<IIII", 0x0101, kt, dt, len(eintr))
tab = b"".join(struct.pack("<HHIII", offs[i], fmts[i], laengen[i], maxl[i], doffs[i]) for i in range(len(eintr)))
open(ziel, "wb").write(kopf + tab + keys + daten)
PY
}
# PS4-Spiel: ps4 <ID> <JSON-Liste der SFO-Felder>
ps4() { mkdir -p $W/user/app/$1/sce_sys $W/user/appmeta/$1 $W/system_data/priv/appmeta/$1; echo x > $W/user/app/$1/eboot.bin; sfo_datei $W/system_data/priv/appmeta/$1/param.sfo "$2"; }
H=$W/user/home/1234abcd
spst() { mkdir -p $H/$1/$2; echo s > $H/$1/$2/sdimg; }
# 1. PS5-Spiel nennt andere Kennungen in serviceIdForSharing (auch eine, die ein zweites Spiel nennt -> widersprüchlich)
pj PPSA72001 "Strassenkönig V" ',"addcont":{"serviceIdForSharing":["EP1004-PPSA72001_00","EP1004-PPSA72002_00","EP1004-CUSA72003_00","EP1004-PPSA72080_00","EP1004-PPSA72070_00","EP1004-PPSA72005_00","EP1004-PPSA72110_00"]}'
spst savedata_prospero PPSA72002; spst savedata CUSA72003; spst savedata_prospero PPSA72001; spst savedata_prospero PPSA72003; spst savedata_prospero PPSA72080
mkdir -p $W/user/download/PPSA72002; echo d > $W/user/download/PPSA72002/download0.dat
mkdir -p $W/user/patch/PPSA72002; echo p > $W/user/patch/PPSA72002/patch.pkg
mkdir -p $W/user/av_contents/photo/NPXS40087/PPSA72002/aaa; echo j > $W/user/av_contents/photo/NPXS40087/PPSA72002/aaa/b.jpg
# 2. GTA-Fall: die Verknüpfung steht nur in appinfo.db (test_namen.c liefert die Zeilen)
pj PPSA72010 "Rennfahrer" ""
spst savedata_prospero PPSA72011; spst savedata_prospero PPSA79991; spst savedata_prospero PPSA72005
mkdir -p $W/user/patch/PPSA72011; echo p > $W/user/patch/PPSA72011/patch.pkg
mkdir -p $W/user/av_contents/photo/NPXS40087/PPSA72011/aaa; echo j > $W/user/av_contents/photo/NPXS40087/PPSA72011/aaa/b.jpg
# Codex 42 (4): installiert, aber ohne lesbaren Namen (keine param.json)
mkdir -p $W/user/app/PPSA72070/sce_sys; echo x > $W/user/app/PPSA72070/eboot.bin
spst savedata_prospero PPSA72070; spst savedata_prospero PPSA72071
mkdir -p $W/user/app/PPSA72120/sce_sys; echo x > $W/user/app/PPSA72120/eboot.bin; spst savedata_prospero PPSA72120   # Codex 45
sfo_datei $H/savedata_prospero_meta/user/PPSA72120/S.sfo '[["MAINTITLE","Ohnename"]]'
# 3. PS4-Spiel mit gültigen Angaben in param.sfo
ps4 CUSA72020 '[["CATEGORY","gd"],["TITLE","Detektivspiel"],["INSTALL_DIR_SAVEDATA","CUSA72022"],["SERVICE_ID_ADDCONT_ADD_1","EP1004-CUSA72021_00"],["SERVICE_ID_ADDCONT_ADD_2",0],["SERVICE_ID_ADDCONT_ADD_3",""],["SERVICE_ID_ADDCONT_ADD_4","EP1004-CUSA72023_00\n"]]'
spst savedata CUSA72021; spst savedata CUSA72022; spst savedata CUSA72023
# 4. PS4-Spiel mit kaputten Angaben wie das umgebaute RDR2-Update (7 x Zeilenumbruch + 2 Leerzeichen)
ps4 CUSA72030 '[["CATEGORY","gd"],["TITLE","Wilder Westen 2"],["SERVICE_ID_ADDCONT_ADD_1","\r\n  "],["SERVICE_ID_ADDCONT_ADD_2","\r\n  "],["SERVICE_ID_ADDCONT_ADD_3","\r\n  "],["SERVICE_ID_ADDCONT_ADD_4","\r\n  "],["SERVICE_ID_ADDCONT_ADD_5","\r\n  "],["SERVICE_ID_ADDCONT_ADD_6","\r\n  "],["SERVICE_ID_ADDCONT_ADD_7","\r\n  "]]'
# 5. PS5-Spiel mit einem kaputten und einem gültigen Eintrag in serviceIdForSharing
# PS5-Test 03.11.2026: unbenutzte Felder als 19 Leerzeichen (GTA Definitive Editions) - keine Warnung
pj PPSA72130 "Platzhalter" ',"addcont":{"serviceIdForSharing":["                   ","                   ","                   ","                   ","                   ","                   ","                   "]}'
ps4 CUSA72140 '[["CATEGORY","gd"],["TITLE","Leerfeld"],["SERVICE_ID_ADDCONT_ADD_1","                   "],["SERVICE_ID_ADDCONT_ADD_2","  "]]'
ps4 CUSA72150 '[["CATEGORY","gd"],["TITLE","Nullfeld"],["SERVICE_ID_ADDCONT_ADD_1","  0"]]'   # Codex 47 (2): kein genaues "0" -> kaputt
pj PPSA72040 "Sternenflug" ',"addcont":{"serviceIdForSharing":["\\r\\n  ","EP9000-PPSA72041_00","\u0045P9000-PPSA72042_00"]}'
spst savedata_prospero PPSA72041; spst savedata_prospero PPSA72042
# 6. deinstalliert: Name aus der Beschreibung der Spielstände (MAINTITLE, auch in der Sicherung sce_bu_) + Fake-Lizenz
spst savedata CUSA72050
sfo_datei $H/savedata_meta/user/CUSA72050/SAVE0.sfo '[["MAINTITLE","Nebelstadt"],["TITLE_ID","CUSA72050"]]'
sfo_datei $H/savedata_meta/user/CUSA72050/sce_bu_SAVE0.sfo '[["MAINTITLE","Nebelstadt"],["TITLE_ID","CUSA72050"]]'
mkdir -p $W/user/license; echo r > $W/user/license/fakeEP1004-CUSA72050_00-NEBELSTADT000000.rif
# 7. deinstalliert: Name aus der Begleitdatei einer Aufnahme
spst savedata CUSA72060
mkdir -p $W/user/av_contents/photo/NPXS40087/CUSA72060/aaa; echo j > $W/user/av_contents/photo/NPXS40087/CUSA72060/aaa/b.jpg
printf '\x01\x00\x00\x00\x10\x00\x00\x00Bergrennen_20261102181119\x00rest' > $W/user/av_contents/photo/NPXS40087/CUSA72060/aaa/b.dat
# 8. deinstalliert: Spielstände mit verschiedenen Titeln -> Name nicht belegt
spst savedata CUSA72070
sfo_datei $H/savedata_meta/user/CUSA72070/A.sfo '[["MAINTITLE","Kapitel 1"]]'
sfo_datei $H/savedata_meta/user/CUSA72070/B.sfo '[["MAINTITLE","Kapitel 2"]]'
# Codex 43: (1) PPSA72080 wird von zwei Spielen genannt - sein Update-Ordner bleibt geschützt; (2) zweite param.json ohne titleName nennt eine
# Kennung; (3) erste param.sfo nur mit Titel, die zweite mit kaputter Angabe; (4) nicht installiert, eigene Angaben, widersprüchliche Belege
mkdir -p $W/user/patch/PPSA72080; echo p > $W/user/patch/PPSA72080/patch.pkg
sfo_datei $H/savedata_prospero_meta/user/PPSA72080/S.sfo '[["MAINTITLE","Irgendwas"]]'   # Codex 44: Widerspruch -> trotzdem unbekannt
pj PPSA72090 "Zweitquelle" ""
printf '{"titleId":"PPSA72090","addcont":{"serviceIdForSharing":["EP1004-PPSA72091_00"]}}' > $W/system_data/priv/appmeta/PPSA72090/param.json
spst savedata_prospero PPSA72091
ps4 CUSA72100 '[["CATEGORY","gd"],["TITLE","Dreifach"],["SERVICE_ID_ADDCONT_ADD_1","\r\n  "]]'
sfo_datei $W/user/appmeta/CUSA72100/param.sfo '[["CATEGORY","gd"],["TITLE","Dreifach"]]'
mkdir -p $W/system_data/priv/appmeta/PPSA72110; printf '{"titleId":"PPSA72110","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"Altspiel"}}}' > $W/system_data/priv/appmeta/PPSA72110/param.json
spst savedata_prospero PPSA72110
# 9. Aufnahmen und Trophäen-Liste ohne jeden Beleg
mkdir -p $W/user/av_contents/photo/NPXS40087/CUSA72090/aaa; echo j > $W/user/av_contents/photo/NPXS40087/CUSA72090/aaa/b.jpg
mkdir -p $W/user/trophy/conf/NPWR72099_00; echo t > $W/user/trophy/conf/NPWR72099_00/TROPHY.TRP
(cd $T; RMDA=1 FAKESM=1 $B/test_namen $T) | grep -v "^LOG:" || RC=1
# Teil 2: appinfo.db nur lesen (SQLite im Speicher, ohne Dateizugriff) - nachgebaut, fehlend, da_Chris86s Kopie vom 02.11.2026 18:25
gcc -O1 -w -c $KK/db/db_sqlite.c -o $B/db_sqlite_nam.o || { echo BAU FEHLGESCHLAGEN; exit 1; }
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/db/db_pruefen.c $KK/db/db_sicherung.c $B/db_sqlite_nam.o $KK/bau/cjson/cJSON.c $KK/test/kern/test_namen_db.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_namen_db || { echo BAU FEHLGESCHLAGEN; exit 1; }
T2=$HOME/ps5c-test/nam2; rm -rf $T2; mkdir -p $T2/db1
python3 - $T2/db1/appinfo.db <<'PY2'
import sqlite3, sys
c = sqlite3.connect(sys.argv[1]); c.execute("CREATE TABLE tbl_appinfo (titleId TEXT, metaDataId TEXT, key TEXT, val)")
rows = [("PPSA70001","m","TITLE","Testspiel"),("PPSA70001","m","INSTALL_DIR_SAVEDATA","PPSA70002"),("PPSA70001","m","SERVICE_ID_ADDCONT_ADD_1","EP1004-PPSA70002_00"),
        ("PPSA70001","m","SERVICE_ID_ADDCONT_ADD_2",0),("PPSA70001","m","SERVICE_ID_ADDCONT_ADD_3",None),("CUSA70003","m","SERVICE_ID_ADDCONT_ADD_1","\r\n  ")]
c.executemany("INSERT INTO tbl_appinfo VALUES (?,?,?,?)", rows); c.commit(); c.close()
PY2
ECHT="/pfad/zu/arbeitsordner"; EO=-
W2=$T2/w; mkdir -p $W2/user/app $W2/user/home/1234abcd/savedata_prospero/PPSA03420; echo s > $W2/user/home/1234abcd/savedata_prospero/PPSA03420/sdimg
mkdir -p $W2/user/app/PPSA04264 $W2/user/appmeta/PPSA04264; echo x > $W2/user/app/PPSA04264/eboot.bin
printf '{"titleId":"PPSA04264","applicationCategoryType":0,"localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"Grand Theft Auto V"}}}' > $W2/user/appmeta/PPSA04264/param.json
mkdir -p $W2/user/app/CUSA08519 $W2/user/appmeta/CUSA08519 $W2/system_data/priv/appmeta/CUSA08519; echo x > $W2/user/app/CUSA08519/app.pkg
python3 - $W2/system_data/priv/appmeta/CUSA08519/param.sfo <<'PY3'
import struct, sys
eintr = [("CATEGORY","gd"),("TITLE","Red Dead Redemption 2")]
keys = b""; offs = []
for k, _ in eintr: offs.append(len(keys)); keys += k.encode() + b"\0"
while len(keys) % 4: keys += b"\0"
daten = b""; doffs = []; laengen = []
for _, v in eintr:
    d = v.encode() + b"\0"; doffs.append(len(daten)); laengen.append(len(d)); d += b"\0" * ((-len(d)) % 4); daten += d
kt = 20 + 16 * len(eintr); dt = kt + len(keys)
open(sys.argv[1], "wb").write(b"\0PSF" + struct.pack("<IIII", 0x0101, kt, dt, len(eintr)) + b"".join(struct.pack("<HHIII", offs[i], 0x0204, laengen[i], laengen[i] + ((-laengen[i]) % 4), doffs[i]) for i in range(len(eintr))) + keys + daten)
PY3
if [ -f "$ECHT" ]; then mkdir -p $T2/echt; cp "$ECHT" $T2/echt/appinfo.db; EO=$T2/echt; fi
(cd $T2; RMDA=1 FAKESM=1 $B/test_namen_db $T2 $EO) | grep -v "^LOG:" || RC=1
rm -rf $T2/echt   # Kopie der Konsolen-Datenbank nicht liegen lassen
date "+%Y-%m-%d %H:%M:%S FERTIG"
[ $RC -ne 0 ] && echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"
exit $RC
