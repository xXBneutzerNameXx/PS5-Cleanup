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
# NEU 1.0.8 (F4): Aufnahmen, die während PS5 Cleanup lief (Startordner PSCL00002) - Begleitdatei nennt nur die Uhrzeit
mkdir -p $W/user/av_contents/photo/NPXS40087/PSCL00002/aaa; echo j > $W/user/av_contents/photo/NPXS40087/PSCL00002/aaa/20261104175554.jpg
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
# 10. NEU 1.0.8 (PS5-Test 07.11.2026, L.A. Noire: Spielstand und Trophäen-Listen "unbekannt"): Namen aus früheren Protokollen von PS5 Cleanup,
#     aus Sonys Spielstand-Liste (Zeilen SPIELSTAND_TITEL, test_namen.c) und aus PKG-Dateien (nach der Suche nachgetragen)
mkdir -p $W/mnt/usb0
printf '%s\r\n' "===== PS5 Cleanup 1.0.8 (PS5) - 2026-11-07 12:00:28 =====" \
  "11:57:21 PS4-Spiele (geschützt, solange du das Spiel selbst nicht anhakst): 2" \
  "11:57:21    Hafenkrimi (CUSA72160) – intern – Installationsdaten FEHLEN (nur leerer Ordner) – Version 01.04 – Fake-Lizenz" \
  "11:57:21    unbekannter Name (CUSA72163) – intern – Version 01.00" \
  "11:57:21    CUSA72166 (CUSA72166) – intern – Version 01.00" \
  "11:57:21 Geschützt und nicht angeboten: 1 Ordner (1 nicht lesbar, 0 zu tief verschachtelt):" \
  "11:57:21    Falsch (CUSA72164) – nicht aus der Liste der PS4-Spiele" \
  "12:00:33   gelöscht   M.2 – Sternwarte (PPSA72161) - ShadowMount-Spiel - nur Kachel, Spiel bleibt   (Kachel über Sony entfernt)" \
  "12:00:34   gelöscht      – M.2 – Blockhafen (PPSA72167) - ShadowMount-Spiel - nur Kachel, Spiel bleibt   (Kachel über Sony entfernt)" \
  "12:00:35   gelöscht   Tool-Kachel: Werkzeug (CUSA72162)   (Kachel über Sony entfernt)" \
  "12:00:36   gelöscht   Reste: Rest (CUSA72165)" \
  "12:00:37   gelöscht   Trophäen-Liste: Altliste (NPWR72189_00) - Spiel nicht mehr da, kann weg (Namen/Bilder, nicht dein Fortschritt)" \
  "12:00:37   gelöscht      – Trophäen-Fortschritt: Blockliste (NPWR72191_00) (PS4-Spiel) - Spiel nicht mehr auf der Konsole" \
  "12:00:38   gelöscht   Trophäen-Liste: NPWR72190_00 (Spiel/App unbekannt) - Spiel nicht mehr da, kann weg (Namen/Bilder, nicht dein Fortschritt)" \
  "12:00:39   unverändert   Trophäen-Liste: Protokollrennen (NPWR72192_00) - Spiel nicht mehr da, kann weg (Namen/Bilder, nicht dein Fortschritt)" > "$W/mnt/usb0/ps5cleanup_protokoll_2026-11-07_120028.txt"
printf '%s\r\n' "09:00:33   gelöscht   Alter Name (CUSA72160) - Spiel   (Kachel über Sony entfernt)" > "$W/mnt/usb0/ps5cleanup_protokoll_2026-11-01_090000.txt"
for x in CUSA72160 CUSA72162 CUSA72163 CUSA72164 CUSA72165 CUSA72166 CUSA72170 CUSA72171 CUSA72172 CUSA72180 CUSA72182; do spst savedata $x; done
spst savedata_prospero PPSA72161; spst savedata_prospero PPSA72167   # PPSA72167: Codex-Prüfung 109 (P2), Zeile im Block
sfo_datei $H/savedata_meta/user/CUSA72172/S.sfo '[["MAINTITLE","Einstellungen"],["SUBTITLE","Einstellungen"]]'   # Titel = Untertitel: kein Spielname
# NEU 1.0.8 (Protokoll 174343 vom PS5-Test 07.11.2026: "Spielstand PPSA03420 (Gespeicherte Daten, ...)"): allgemeine Titel sind kein Spielname
spst savedata CUSA72173; sfo_datei $H/savedata_meta/user/CUSA72173/S.sfo '[["MAINTITLE","Gespeicherte Daten"]]'
spst savedata CUSA72174; sfo_datei $H/savedata_meta/user/CUSA72174/A.sfo '[["MAINTITLE","Saved Data 2"]]'; sfo_datei $H/savedata_meta/user/CUSA72174/B.sfo '[["MAINTITLE","Kristallhöhle"]]'
spst savedata CUSA72175   # Titel nur aus Sonys Spielstand-Liste (test_namen.c): "Gespeicherte Daten" und "Saved Data"
# TROPHY.TRP wie von Sony (Kopf DC A2 4D 00, Einträge zu 64 Byte, ICON0.PNG unverschlüsselt) - dieselbe Datei steckt in der PKG-Datei
trp_datei() { mkdir -p "$(dirname "$1")"; python3 - "$@" <<'PY'
import struct, sys, hashlib
ziel, kz = sys.argv[1:3]
png = b"\x89PNG\r\n\x1a\n" + b"x" * 64
eintr = [("TROPCONF.ESFM", b"E" * 32, 3), ("ICON0.PNG", png, 0)]
off = 0x60 + 0x40 * len(eintr); daten = b""; tab = b""
for name, d, fl in eintr:
    tab += name.encode().ljust(32, b"\0") + struct.pack(">QQI", off + len(daten), len(d), fl) + b"\0" * 12; daten += d
kopf = bytearray(0x60); kopf[0:4] = b"\xDC\xA2\x4D\x00"; struct.pack_into(">I", kopf, 4, 3); struct.pack_into(">Q", kopf, 8, off + len(daten))
struct.pack_into(">II", kopf, 0x10, len(eintr), 0x40); kopf[0x1C:0x30] = hashlib.sha1(kz.encode()).digest()
open(ziel, "wb").write(bytes(kopf) + tab + daten)
PY
}
# PKG-Datei mit param.sfo (0x1000) und optional Trophäen-Dateien (0x1400, 0x1401): pkg_datei <ziel> <TITLE_ID> <TITLE> <CATEGORY> <trp|-> [trp2]
pkg_datei() { mkdir -p "$(dirname "$1")"; python3 - "$@" <<'PY'
import struct, sys
ziel, tid, titel, kat, trp = sys.argv[1:6]
trp2 = sys.argv[6] if len(sys.argv) > 6 else "-"
eintr = [("CATEGORY", kat), ("TITLE", titel), ("TITLE_ID", tid)]
keys = b""; offs = []
for k, _ in eintr: offs.append(len(keys)); keys += k.encode() + b"\0"
while len(keys) % 4: keys += b"\0"
daten = b""; doffs = []; laengen = []
for _, v in eintr:
    d = v.encode() + b"\0"; doffs.append(len(daten)); laengen.append(len(d)); d += b"\0" * ((-len(d)) % 4); daten += d
kt = 20 + 16 * len(eintr); dt = kt + len(keys)
sfo = b"\0PSF" + struct.pack("<IIII", 0x0101, kt, dt, len(eintr)) + b"".join(struct.pack("<HHIII", offs[i], 0x0204, laengen[i], laengen[i] + ((-laengen[i]) % 4), doffs[i]) for i in range(len(eintr))) + keys + daten
trpd = open(trp, "rb").read() if trp != "-" else b""
trpd2 = open(trp2, "rb").read() if trp2 != "-" else b""
tab, so, to = 0x2A80, 0x3000, 0x4000
to2 = to + len(trpd) + 64
liste = [(0x1000, so, len(sfo))] + ([(0x1400, to, len(trpd))] if trpd else []) + ([(0x1401, to2, len(trpd2))] if trpd2 else [])
h = bytearray(max(to2 + len(trpd2), so + len(sfo)) + 16)
h[0:4] = b"\x7FCNT"; struct.pack_into(">I", h, 0x10, len(liste)); struct.pack_into(">I", h, 0x18, tab)
for i, (eid, off, gr) in enumerate(liste): struct.pack_into(">IIIIII", h, tab + 32 * i, eid, 0, 0, 0, off, gr)
h[so:so + len(sfo)] = sfo
if trpd: h[to:to + len(trpd)] = trpd
if trpd2: h[to2:to2 + len(trpd2)] = trpd2
open(ziel, "wb").write(bytes(h))
PY
}
trp_datei $W/user/trophy/conf/NPWR72181_00/TROPHY.TRP kistenspiel
mkdir -p $H/trophy/data/NPWR72181_00; echo x > $H/trophy/data/NPWR72181_00/trophy.img
pkg_datei $W/mnt/usb0/PS4/Kistenspiel.pkg CUSA72180 "Kistenspiel" gd $W/user/trophy/conf/NPWR72181_00/TROPHY.TRP
pkg_datei $W/mnt/usb0/PS4/Bonus.pkg CUSA72182 "Bonuspaket" ac -        # Zusatzinhalt: sein eigener Name, nicht der des Spiels
pkg_datei $W/mnt/usb0/PS4/Echt.pkg CUSA72166 "Echtname" gd -     # Codex-Prüfung 109 (P3): im Protokoll nur die Kennung als Name - der echte Name kommt aus der PKG-Datei
pkg_datei $W/mnt/usb0/Falsch.pkg PPSA72080 "Falschname" gd -           # Widerspruch (zwei Spiele nennen PPSA72080) bleibt unbekannt
# Codex-Prüfung 108 (P2): Widerspruch nur beim Spielstand (zwei INSTALL_DIR_SAVEDATA-Belege, test_namen.c), dazu eine Aufnahme derselben
# Kennung ohne Beleg - auch dann wird nichts nachgetragen; Fake-Lizenz einer anderen Kennung, deren Inhaltskennung CUSA72180 enthält
spst savedata CUSA72190
mkdir -p $W/user/av_contents/photo/NPXS40087/CUSA72190/aaa; echo j > $W/user/av_contents/photo/NPXS40087/CUSA72190/aaa/b.jpg
pkg_datei $W/mnt/usb0/Falsch2.pkg CUSA72190 "Falscher Spielstand" gd -
mkdir -p $W/user/license; echo r > $W/user/license/fakeEP1004-CUSA72191_00-CUSA72180XXXXXXX.rif
# Prüfer 110 (P3): installiert, aber ohne lesbaren Namen - der Name aus einer PKG-Datei ohne "nicht mehr auf der Konsole"
mkdir -p $W/user/app/PPSA72195/sce_sys; echo x > $W/user/app/PPSA72195/eboot.bin; spst savedata_prospero PPSA72195
pkg_datei $W/mnt/usb0/Echtspiel.pkg PPSA72195 "Echtspiel" gd -
# NEU 1.0.8 (PS5-Test 07.11.2026 16:31): PS5-Trophäen-Liste (TROPHY.UCP) trägt den Spielnamen selbst (tropmeta_<Sprache>.json) und das Bild
# (icon0_<Sprache>.png) - Aufbau wie auf da_Chris86s PS5 (Kopf B2 28 C6 0A, Einträge zu 64 Byte), Inhalte erfunden
ucp_datei() { mkdir -p "$(dirname "$1")"; python3 - "$@" <<'PY'
import struct, sys, json
ziel, kennung, de, en = sys.argv[1:5]
png = b"\x89PNG\r\n\x1a\n" + b"u" * 64
def meta(n): return json.dumps({"schemaVersion": "0.90", "trophyNpCommId": kennung, "metadata": {"titleMetadata": {"name": n}, "trophyMetadata": [{"id": "0000", "name": "Alles", "detail": "x"}]}}, ensure_ascii=False).encode()
eintr = [("icon0_de-DE.png", png), ("icon0_en-US.png", png), ("tropconf.json", b"{}"), ("tropmeta_de-DE.json", meta(de)), ("tropmeta_en-US.json", meta(en))]
off = 0x60 + 0x40 * len(eintr); daten = b""; tab = b""
for name, d in eintr:
    tab += name.encode().ljust(32, b"\0") + struct.pack(">QQ", off + len(daten), len(d)) + b"\0" * 16; daten += d
kopf = bytearray(0x60); kopf[0:4] = b"\xB2\x28\xC6\x0A"; struct.pack_into(">Q", kopf, 8, off + len(daten)); struct.pack_into(">II", kopf, 0x10, len(eintr), 0x40)
open(ziel, "wb").write(bytes(kopf) + tab + daten)
PY
}
ucp_datei $W/user/trophy2/nobackup/conf/NPWR72196_00/TROPHY.UCP NPWR72196_00 "Sternenreise" "Star Voyage"
mkdir -p $H/trophy2/nobackup/data/NPWR72196_00; echo x > $H/trophy2/nobackup/data/NPWR72196_00/trophy.img
ucp_datei $W/user/trophy2/nobackup/conf/NPWR72197_00/TROPHY.UCP NPWR72198_00 "Fremdliste" "Other List"   # andere Kennung in der Datei: kein Name
# NEU 1.0.8 (PS5-Test 07.11.2026 16:31): Spiel-Ordner und Spieldateien ohne Kachel bekommen ihren Namen - aus dem Spiel, sonst aus einem Beleg
OG="$W/mnt/usb0/homebrew/Ordnerspiel - PPSA72200 - v1.00"; mkdir -p "$OG/sce_sys"; echo x > "$OG/eboot.bin"
printf '{"titleId":"PPSA72200","localizedParameters":{"defaultLanguage":"en-US","de-DE":{"titleName":"Ordnerspiel Echt"},"en-US":{"titleName":"Folder Game"}}}' > "$OG/sce_sys/param.json"
printf '\x89PNG\r\n\x1a\nbild' > "$OG/sce_sys/icon0.png"
echo f > $W/mnt/usb0/homebrew/PPSA72161.ffpkg   # nicht lesbar (kein exFAT): Name aus dem früheren Protokoll (Sternwarte)
echo f > $W/mnt/usb0/homebrew/PPSA72201.ffpkg   # ohne jeden Beleg: kein Name
# NEU 1.0.9 (08.11.2026, kstuff-lite image.c): .ufs ist ein UFS-Abbild wie .ffpkg - Spiel, auch großgeschrieben in einem eigenen Ordner
echo f > $W/mnt/usb0/homebrew/PPSA72202.ufs
mkdir -p $W/mnt/usb0/spiele; echo f > $W/mnt/usb0/spiele/PPSA72203.UFS
# NEU 1.0.8 (PS5-Test 07.11.2026 17:43): Fortschritt, dessen Liste schon gelöscht ist - Name aus dem Protokoll (auch eingerückte Blockzeile)
for x in NPWR72189_00 NPWR72190_00 NPWR72191_00; do mkdir -p $H/trophy/data/$x; echo x > $H/trophy/data/$x/trophy.img; done
# NEU 1.0.8 (PS5-Test 07.11.2026 17:31): Kopie des Trophäen-Fortschritts mit Endung (wie NPWR09412_00_BK auf da_Chris86s PS5)
mkdir -p $H/trophy/data/NPWR72181_00_BK; echo x > $H/trophy/data/NPWR72181_00_BK/trophy.img
# NEU 1.0.8 (PS5-Test 07.11.2026 17:32, DRIVECLUB BIKES): ein Update-PKG mit zwei Trophäen-Dateien (0x1400 und 0x1401)
trp_datei $W/user/trophy/conf/NPWR72187_00/TROPHY.TRP doppelrennen-eins
trp_datei $W/user/trophy/conf/NPWR72188_00/TROPHY.TRP doppelrennen-zwei
pkg_datei $W/mnt/usb0/PS4/Doppelrennen.pkg CUSA72186 "Doppelrennen" gp $W/user/trophy/conf/NPWR72187_00/TROPHY.TRP $W/user/trophy/conf/NPWR72188_00/TROPHY.TRP
# Codex-Prüfung 116: Name schon aus dem Protokoll, Nummer trotzdem aus dem PKG (zweite Trophäen-Datei)
trp_datei $W/user/trophy/conf/NPWR72192_00/TROPHY.TRP protokollrennen-zwei; trp_datei $W/mnt/usb0/PS4/trp_eins.bin protokollrennen-eins
pkg_datei $W/mnt/usb0/PS4/Protokollrennen.pkg CUSA72193 "Protokollrennen" gp $W/mnt/usb0/PS4/trp_eins.bin $W/user/trophy/conf/NPWR72192_00/TROPHY.TRP
# NEU 1.0.8 (PS5-Test 07.11.2026 22:53): Ordner, die die Konsole beim Kopieren auf ein USB-Laufwerk anlegt (eigene Daten) - und ein
# eigener Ordner "ps5" ohne Sony-Unterordner (bleibt wie bisher). Der Ordner "PS4" auf usb0 mit PKG-Dateien bleibt ebenfalls wie bisher.
mkdir -p "$W/mnt/usb0/PS5/CREATE/Screenshots/Testspiel" "$W/mnt/usb0/PS5/CREATE/Video Clips/Testspiel"
echo bild > "$W/mnt/usb0/PS5/CREATE/Screenshots/Testspiel/Testspiel_20261107.jpg"; echo film > "$W/mnt/usb0/PS5/CREATE/Video Clips/Testspiel/Testspiel_20261107.webm"
mkdir -p $W/mnt/usb1/PS4/SAVEDATA/1234abcd/CUSA72160 $W/mnt/usb1/PS4/EXPORT/BACKUP $W/mnt/usb1/ps5/Musik
echo s > $W/mnt/usb1/PS4/SAVEDATA/1234abcd/CUSA72160/sdimg_SAVEDATA00; echo b > $W/mnt/usb1/PS4/EXPORT/BACKUP/sicherung.bin
echo m > $W/mnt/usb1/ps5/Musik/lied.mp3
(cd $T; RMDA=1 FAKESM=1 $B/test_namen $T) | grep -v "^LOG:" || RC=1
# Teil 2: appinfo.db nur lesen (SQLite im Speicher, ohne Dateizugriff) - nachgebaut, fehlend, da_Chris86s Kopie vom 02.11.2026 18:25
gcc -O1 -w -c $KK/db/db_sqlite.c -o $B/db_sqlite_nam.o || { echo BAU FEHLGESCHLAGEN; exit 1; }
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/db/db_pruefen.c $KK/db/db_lesen.c $B/db_sqlite_nam.o $KK/bau/cjson/cJSON.c $KK/test/kern/test_namen_db.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_namen_db || { echo BAU FEHLGESCHLAGEN; exit 1; }
T2=$HOME/ps5c-test/nam2; rm -rf $T2; mkdir -p $T2/db1
python3 - $T2/db1/appinfo.db <<'PY2'
import sqlite3, sys
c = sqlite3.connect(sys.argv[1]); c.execute("CREATE TABLE tbl_appinfo (titleId TEXT, metaDataId TEXT, key TEXT, val)")
rows = [("PPSA70001","m","TITLE","Testspiel"),("PPSA70001","m","INSTALL_DIR_SAVEDATA","PPSA70002"),("PPSA70001","m","SERVICE_ID_ADDCONT_ADD_1","EP1004-PPSA70002_00"),
        ("PPSA70001","m","SERVICE_ID_ADDCONT_ADD_2",0),("PPSA70001","m","SERVICE_ID_ADDCONT_ADD_3",None),("CUSA70003","m","SERVICE_ID_ADDCONT_ADD_1","\r\n  ")]
c.executemany("INSERT INTO tbl_appinfo VALUES (?,?,?,?)", rows); c.commit(); c.close()
PY2
# NEU 1.0.8 (PS5-Test 07.11.2026): Spielstand-Liste wie Sonys savedata.db (Spalten wie auf da_Chris86s PS5, Werte erfunden)
mkdir -p $T2/sd
python3 - $T2/sd/savedata.db <<'PY4'
import sqlite3, sys
c = sqlite3.connect(sys.argv[1])
c.execute("CREATE TABLE savedata (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,title_id NOT NULL, dir_name NOT NULL, main_title NOT NULL, sub_title , detail , mtime NOT NULL, game_title_id NOT NULL)")
rows = [("CUSA70010","SAVE1","Testspiel","Spieler",1),("CUSA70010","SAVE2","Testspiel","Automatisch",2),("CUSA70011","PREFS","Einstellungen","Einstellungen",3),
        ("CUSA70012","S","Ohne Untertitel",None,4)]
c.executemany("INSERT INTO savedata (title_id, dir_name, main_title, sub_title, mtime, game_title_id) VALUES (?,?,?,?,?,?)", [(r[0], r[1], r[2], r[3], r[4], r[0]) for r in rows])
c.commit(); c.close()
PY4
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
