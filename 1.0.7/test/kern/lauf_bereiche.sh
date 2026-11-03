#!/bin/bash
# Test der 7 Bereiche (1.0.7, 31.10.2026) - läuft in WSL
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/ber; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/bereiche.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_bereiche.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_bereiche || { echo BAU FEHLGESCHLAGEN; exit 1; }
W=$T/w
# Basis: download_v1 (dl_baum.sh) wird in eine eigene Kopie gebaut, damit parallele Läufe sich nicht stören
rm -rf $T/ref; mkdir -p $T/ref; cp -r $RF/root9 $T/ref/root9; cp $RF/dl_baum.sh $T/ref/; (cd $T/ref; bash dl_baum.sh > /dev/null); rm -rf $W; cp -r $T/ref/download_v1 $W
pj() { mkdir -p $W/user/app/$1/sce_sys $W/user/appmeta/$1; printf '{"titleId":"%s","applicationCategoryType":%s,"localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"%s"}}}' "$1" "$2" "$3" > $W/user/appmeta/$1/param.json; }
pj PPSA10001 0 "Test Game"
pj PPSA10002 65536 "Media Player Pro"
sfo() { mkdir -p $W/user/app/$1/sce_sys $W/user/appmeta/$1; python3 - "$W/user/appmeta/$1/param.sfo" "$2" "$3" <<'PY'
import struct, sys
ziel, cat, title = sys.argv[1], sys.argv[2], sys.argv[3]
eintr = [("CATEGORY", cat), ("TITLE", title)]
keys = b""; offs = []
for k, _ in eintr: offs.append(len(keys)); keys += k.encode() + b"\0"
while len(keys) % 4: keys += b"\0"
daten = b""; doffs = []; laengen = []
for _, v in eintr:
    d = v.encode() + b"\0"; doffs.append(len(daten)); laengen.append(len(d)); d += b"\0" * ((-len(d)) % 4); daten += d
kt = 20 + 16 * len(eintr); dt = kt + len(keys)
kopf = b"\0PSF" + struct.pack("<IIII", 0x0101, kt, dt, len(eintr))
tab = b"".join(struct.pack("<HHIII", offs[i], 0x0204, laengen[i], laengen[i] + ((-laengen[i]) % 4), doffs[i]) for i in range(len(eintr)))
open(ziel, "wb").write(kopf + tab + keys + daten)
PY
}
sfo CUSA20001 gd "Old Game"
sfo CUSA20002 gde "Video App"
mkdir -p $W/user/data/nanodns $W/user/data/AMPRStudio; echo x > $W/user/data/nanodns/config.ini; echo x > $W/user/data/AMPRStudio/a.bin
# NEU 1.0.7 (Recherche 31.10.2026): Tool mit Unersetzlichem, Spiel-Sicherungen, unbekannter Ordner, Einhängeordner, Merkdatei
mkdir -p $W/user/data/garlic/backups $W/user/data/game-backups $W/user/data/MeinOrdner $W/user/data/imgmnt/ufsmnt $W/user/data/save_mnt
echo x > $W/user/data/garlic/backups/s.zip; echo x > $W/user/data/game-backups/Spiel.exfat; echo x > $W/user/data/MeinOrdner/a.txt; echo x > $W/user/data/.kstuff_noautomount
mkdir -p $W/user/license; echo x > $W/user/license/fakeCUSA99999.rif; echo x > $W/user/license/fakeCUSA99999.idx
# NEU 1.0.7 (31.10.2026 17:00): LZ4 Game Compressor blendet das Spiel-Original ein (folder-source), PlayGo-Reste, Spielstand-Meta
mkdir -p $W/user/data/AMPRStudio/folder-source/sce_sys; echo x > $W/user/data/AMPRStudio/folder-source/eboot.bin; echo '{"source":"/mnt/ext1/homebrew/Spiel"}' > $W/user/data/AMPRStudio/folder-source.json
# 31.10.2026 17:25: LEERES folder-source (nichts eingeblendet, z. B. nach Neustart) geht mit dem Tool-Ordner mit
mkdir -p $W/user/data/AMPRLeer/folder-source; echo x > $W/user/data/AMPRLeer/b.bin
mkdir -p $W/user/playgo/app/EP1004-CUSA77701_00-ABCDEFGHIJKLMNOP/00 $W/user/playgo/patch/EP1004-CUSA77701_00-ABCDEFGHIJKLMNOP/00/00.00 $W/user/playgo/app/EP0000-CUSA20001_00-OLDGAME000000000/00
echo x > $W/user/playgo/app/EP1004-CUSA77701_00-ABCDEFGHIJKLMNOP/00/d0.pdb; echo x > $W/user/playgo/patch/EP1004-CUSA77701_00-ABCDEFGHIJKLMNOP/00/00.00/d0.pdb; echo x > $W/user/playgo/app/EP0000-CUSA20001_00-OLDGAME000000000/00/d0.pdb
mkdir -p $W/user/home/1234abcd/savedata_prospero/PPSA77702 $W/user/home/1234abcd/savedata_prospero_meta/user/PPSA77702
echo x > $W/user/home/1234abcd/savedata_prospero/PPSA77702/sdimg_SAVE; echo x > $W/user/home/1234abcd/savedata_prospero_meta/user/PPSA77702/SAVE_icon0.png
# NEU 1.0.7 (17:00): leere Einhängeordner von ShadowMount in /system_ex/app
mkdir -p $W/system_ex/app/PPSA88801 $W/system_ex/app/PPSA10001 $W/system_ex/app/PPSA88802 $W/system_ex/app/FAKE78001 $W/system_ex/app/FAKE77031 $W/system_ex/app/NPXS40000 $W/system_ex/app/PSCL00002
echo x > $W/system_ex/app/PPSA88802/eboot.bin
# NEU 1.0.7 (PS5-Test 01.11.2026): PS4-Spiel - npbind.dat liegt direkt im Kachel-Info-Ordner (wie bei Red Dead Redemption 2, CUSA08519)
mkdir -p $W/system_data/priv/appmeta/CUSA20001 $W/user/home/1234abcd/trophy/data/NPWR09412_00 $W/user/trophy/conf/NPWR09412_00
printf 'xx\001NPWR09412_00\000yy' > $W/system_data/priv/appmeta/CUSA20001/npbind.dat
echo x > $W/user/home/1234abcd/trophy/data/NPWR09412_00/TROP.SFM; echo x > $W/user/trophy/conf/NPWR09412_00/TROP.SFM
(cd $T; RMDA=1 FAKESM=1 $B/test_bereiche $T) | grep -v "^LOG:"
date "+%Y-%m-%d %H:%M:%S FERTIG"
