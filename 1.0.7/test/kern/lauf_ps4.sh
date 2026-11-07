#!/bin/bash
# PS5 Cleanup 1.0.7 - Test PS4-Spieleschutz + Datenbank-Prüfung (seit 1.0.8 nur lesen) + Ende ohne Neuaufbau-Empfehlung (da_Chris86 31.10.2026 19:56)
# Nur nachgebildete Dateisysteme und Datenbanken - nie eine PS5. Läuft in WSL (K überschreibbar für andere Orte).
K="${K:-/pfad/zu/PS5-Cleanup/1.0.7}"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/ps4
mkdir -p "$K/test/kern/ergebnis" $B
exec > "$K/test/kern/ergebnis/ps4.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
# NEU 1.0.7 (21:50): SQLite (db/db_sqlite.c, ohne Dateizugriff) einmal übersetzen - dauert etwa eine halbe Minute
gcc -O1 -w -c $KK/db/db_sqlite.c -o $B/db_sqlite.o || { echo BAU FEHLGESCHLAGEN; exit 1; }
echo "SQLite-Objekt, Aufrufe nach außen: $(nm -u $B/db_sqlite.o | tr -s ' ' | tr '\n' ' ')"
nm -u $B/db_sqlite.o | grep -w -E "open|openat|fopen|read|write|unlink|fsync|mmap|mkdir|rmdir|rename|ftruncate|fcntl|lstat|stat|access" && { echo "FAIL SQLite enthält Dateizugriffe"; } || echo "OK   SQLite ohne jeden Dateizugriff (nur Speicher-, Text- und Zeitfunktionen)"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/db/db_pruefen.c $KK/db/db_lesen.c $B/db_sqlite.o $KK/bau/cjson/cJSON.c $KK/test/kern/test_ps4.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_ps4 || { echo BAU FEHLGESCHLAGEN; exit 1; }
# NEU 1.0.7 (21:50, Punkt 5): Schreibsperre der Test-/Prüffassungen am PC nachweisen
SP=0; for q in $KERN $KK/db/db_pruefen.c $KK/db/db_lesen.c; do gcc -c -O1 -w -include $KK/bau/nurlesen.h -DNUR_LESEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson "$q" -o $B/nl_$(basename "$q" .c).o || SP=1; done
[ $SP = 0 ] && echo "OK   Schreibsperre: Kern und Datenbank-Prüfung lassen sich mit bau/nurlesen.h übersetzen" || echo "FAIL Schreibsperre: Übersetzen mit bau/nurlesen.h scheitert"
NLT=$(nm -u $B/nl_*.o | grep -w -E "fwrite|fputs|fputc|fprintf|write|pwrite|unlink|unlinkat|rmdir|mkdir|mkdirat|rename|renameat|truncate|ftruncate|chmod|fchmod|fchmodat|link|linkat|symlink|nmount|mount|unmount|openat" | sort -u)
[ -z "$NLT" ] && echo "OK   Schreibsperre: keine schreibende Funktion in den Objektdateien ohne KERN_LOESCHEN" || echo "FAIL Schreibsperre: $NLT"
FO=0; for q in $KERN $KK/db/db_pruefen.c $KK/db/db_lesen.c; do gcc -E -include $KK/bau/nurlesen.h -DNUR_LESEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson "$q" 2>/dev/null | python3 -I $KK/bau/fopen_pruefen.py "$(basename $q)" > /dev/null || FO=1; done
[ $FO = 0 ] && echo "OK   Schreibsperre: jedes fopen ohne KERN_LOESCHEN öffnet nur lesend" || echo "FAIL Schreibsperre: fopen schreibend"
printf '#include <stdio.h>\nvoid x(FILE *f) { fwrite("a", 1, 1, f); }\n' > $B/gift.c
gcc -c -include $KK/bau/nurlesen.h $B/gift.c -o $B/gift.o 2>/dev/null && echo "FAIL Gegenprobe: fwrite ließ sich trotz Sperre übersetzen" || echo "OK   Gegenprobe: ein fwrite bricht das Übersetzen ab (Sperre wirkt)"
printf '#include <stdio.h>\nFILE *x(void) { return fopen("x.txt", "w"); }\n' > $B/gift2.c
gcc -E -include $KK/bau/nurlesen.h $B/gift2.c | python3 -I $KK/bau/fopen_pruefen.py gift2.c > /dev/null && echo "FAIL Gegenprobe: fopen(\"w\") nicht erkannt" || echo "OK   Gegenprobe: fopen(\"w\") wird erkannt"
rm -rf $T; mkdir -p $T; W=$T/w
sfo() {   # sfo <datei> KEY=WERT ...
  mkdir -p "$(dirname "$1")"; python3 - "$@" <<'PY'
import struct, sys
ziel = sys.argv[1]; eintr = [a.split("=", 1) for a in sys.argv[2:]]
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
gr() { mkdir -p "$(dirname "$1")"; head -c "$2" /dev/zero | tr '\0' 'x' > "$1"; }
pkgdatei() {   # pkgdatei <datei> <TITEL> <TITLE_ID> <CATEGORY> <APP_VER> [art] - NEU 1.0.8 (Arbeitsplan 2.2): PS4-PKG-Kopf mit param.sfo
              # art (Codex-Prüfung 71): normal | ohne_sfo | sfo_gross | abgeschnitten | grenze (param.sfo über der 64-KB-Grenze) | weit (hinter 1 MB) | ohne_id
              #   | boese (Codex-Prüfung 72: param.sfo mit 300 Einträgen, die alle auf denselben 60-KB-Text zeigen)
              #   | viele (05.11.2026: Kopf nennt 9000 Einträge) | sfo_weg (05.11.2026: param.sfo zeigt hinter das Dateiende)
  mkdir -p "$(dirname "$1")"; python3 - "$@" <<'PY'
import struct, sys
ziel, titel, tid, kat, ver = sys.argv[1:6]; art = sys.argv[6] if len(sys.argv) > 6 else "normal"
eintr = [("APP_VER", ver), ("CATEGORY", kat), ("TITLE", titel)] + ([] if art == "ohne_id" else [("TITLE_ID", tid)])
keys = b""; offs = []
for k, _ in eintr: offs.append(len(keys)); keys += k.encode() + b"\0"
while len(keys) % 4: keys += b"\0"
daten = b""; doffs = []; laengen = []
for _, v in eintr:
    d = v.encode() + b"\0"; doffs.append(len(daten)); laengen.append(len(d)); d += b"\0" * ((-len(d)) % 4); daten += d
kt = 20 + 16 * len(eintr); dt = kt + len(keys)
sfo = b"\0PSF" + struct.pack("<IIII", 0x0101, kt, dt, len(eintr)) + b"".join(struct.pack("<HHIII", offs[i], 0x0204, laengen[i], laengen[i] + ((-laengen[i]) % 4), doffs[i]) for i in range(len(eintr))) + keys + daten
if art == "boese":
    n = 300; keys = b"TITLE\0\0\0"; daten = b"A" * 60000 + b"\0" * 4
    kt = 20 + 16 * n; dt = kt + len(keys)
    sfo = b"\0PSF" + struct.pack("<IIII", 0x0101, kt, dt, n) + b"".join(struct.pack("<HHIII", 0, 0x0204, 60001, 60004, 0) for _ in range(n)) + keys + daten
so = {"grenze": 0xFFF0, "weit": 0x200000, "sfo_weg": 0x7FFF0000}.get(art, 0x3000)
tab = 0x2A80; liste = [(0x0001, 0x2B00, 64), (0x0400, 0x2B40, 0x100)] + ([] if art == "ohne_sfo" else [(0x1000, so, 70000 if art == "sfo_gross" else len(sfo))]) + [(0x1200, 0x3800, 0x400)]
h = bytearray(max(0x4000, (0 if art == "sfo_weg" else so) + len(sfo) + 16))
h[0:4] = b"\x7FCNT"; struct.pack_into(">I", h, 0x10, len(liste)); struct.pack_into(">I", h, 0x18, tab)
cid = ("UP0001-" + tid + "_00-PS5CLEANUPTEST01").encode(); h[0x40:0x40 + len(cid)] = cid
for i, (eid, off, gr) in enumerate(liste): struct.pack_into(">IIIIII", h, tab + 32 * i, eid, 0, 0, 0, off, gr)
if art != "sfo_weg": h[so:so + len(sfo)] = sfo
if art == "viele": struct.pack_into(">I", h, 0x10, 9000)
open(ziel, "wb").write(bytes(h)[:so + 16] if art == "abgeschnitten" else bytes(h) + b"x" * 200000)
PY
}
exfatbild() {   # exfatbild <datei> <art> <titel_de> <titel_en> <nummer> - NEU 1.0.8 (Arbeitsplan 2.1): kleines exFAT-Abbild nach Spezifikation
               # art: normal (param.json über 2 Cluster per FAT-Kette) | am_stueck (ohne FAT-Kette) | unterordner | sfo (PS4) | kein_exfat
               #   | kaputt_kopf | schleife (FAT-Kette der Wurzel im Kreis) | riesig (param.json mit 4 GB Länge) | ohne_sys | kurz (abgeschnitten)
  mkdir -p "$(dirname "$1")"; python3 - "$@" <<'PY'
import struct, sys, json
ziel, art, de, en, nummer = sys.argv[1:6]
SEK, CL, ANZ, FAT_OFF, HEAP_OFF, ENDE = 512, 4096, 64, 24, 64, 0xFFFFFFFF
img = bytearray(HEAP_OFF * SEK + ANZ * CL)
fat = [0] * (ANZ + 2); fat[0] = 0xFFFFFFF8; fat[1] = ENDE
def off(c): return HEAP_OFF * SEK + (c - 2) * CL
def eintrag(name, ordner, c, laenge, am_stueck=False):
    n = name.encode("utf-16-le"); nl = len(name); k = (nl + 14) // 15
    e = bytearray(32 * (2 + k)); e[0] = 0x85; e[1] = 1 + k; struct.pack_into("<H", e, 4, 0x10 if ordner else 0x20)
    e[32] = 0xC0; e[33] = 0x01 | (0x02 if am_stueck else 0); e[35] = nl
    struct.pack_into("<Q", e, 40, laenge); struct.pack_into("<I", e, 52, c); struct.pack_into("<Q", e, 56, laenge)
    for j in range(k): o = 64 + 32 * j; e[o] = 0xC1; t = n[30 * j:30 * j + 30]; e[o + 2:o + 2 + len(t)] = t
    return bytes(e)
def ablegen(clusters, daten, kette=True):
    for i, c in enumerate(clusters): s = daten[i * CL:(i + 1) * CL]; img[off(c):off(c) + len(s)] = s
    if kette:
        for a, b in zip(clusters, clusters[1:]): fat[a] = b
        fat[clusters[-1]] = ENDE
def verz(c, eintraege): ablegen([c], b"".join(eintraege))
def sfo(paare):
    keys = b""; offs = []
    for k, _ in paare: offs.append(len(keys)); keys += k.encode() + b"\0"
    while len(keys) % 4: keys += b"\0"
    daten = b""; doffs = []; laengen = []
    for _, v in paare: d = v.encode() + b"\0"; doffs.append(len(daten)); laengen.append(len(d)); d += b"\0" * ((-len(d)) % 4); daten += d
    kt = 20 + 16 * len(paare); dt = kt + len(keys)
    return b"\0PSF" + struct.pack("<IIII", 0x0101, kt, dt, len(paare)) + b"".join(struct.pack("<HHIII", offs[i], 0x0204, laengen[i], laengen[i] + ((-laengen[i]) % 4), doffs[i]) for i in range(len(paare))) + keys + daten
pj = json.dumps({"titleId": nummer, "localizedParameters": {"defaultLanguage": "en-US", "de-DE": {"titleName": de}, "en-US": {"titleName": en}}, "fueller": "x" * 5000}).encode()
eboot = eintrag("eboot.bin", False, 10, 100)
if art == "unterordner":
    verz(2, [eintrag(nummer + "-app", True, 3, CL)]); verz(3, [eboot, eintrag("sce_sys", True, 4, CL)]); sys_c = 4
elif art == "ohne_sys":
    verz(2, [eboot, eintrag("a", True, 3, CL), eintrag("b", True, 4, CL)]); verz(3, []); verz(4, []); sys_c = None
else:
    verz(2, [eboot, eintrag("sce_sys", True, 3, CL)]); sys_c = 3
if sys_c:
    if art == "sfo":
        s = sfo([("TITLE", de), ("TITLE_ID", nummer), ("CATEGORY", "gd")]); verz(sys_c, [eintrag("icon0.png", False, 11, 50), eintrag("param.sfo", False, 5, len(s))]); ablegen([5], s)
    elif art == "am_stueck":
        verz(sys_c, [eintrag("icon0.png", False, 11, 50), eintrag("param.json", False, 5, len(pj), True)]); ablegen([5, 6], pj, kette=False)
    else:
        laenge = 4 * 1024 ** 3 if art == "riesig" else len(pj)
        # NEU 07.11.2026 (PS5-Test 16:31): Bild über zwei Cluster - hintereinander (bild_kette) oder verstreut (bild_verstreut, kein Bild)
        bl = 5000 if art in ("bild_kette", "bild_verstreut") else 50
        verz(sys_c, [eintrag("icon0.png", False, 11, bl), eintrag("param.json", False, 5, laenge)]); ablegen([5, 7], pj)
        if bl == 5000: ablegen([11, 12] if art == "bild_kette" else [11, 13], b"\x89PNG\r\n\x1a\n" + b"p" * (bl - 8))
if art == "schleife": fat[2] = 2
b = bytearray(512); b[0:3] = b"\xEB\x76\x90"; b[3:11] = b"EXFAT   "
struct.pack_into("<QQIIIII", b, 64, 0, len(img) // SEK, FAT_OFF, 1, HEAP_OFF, ANZ, 2)
b[108] = 20 if art == "kaputt_kopf" else 9; b[109] = 3; b[110] = 1; b[510] = 0x55; b[511] = 0xAA
img[0:512] = b; struct.pack_into("<%dI" % len(fat), img, FAT_OFF * SEK, *fat)
if art == "kein_exfat": img = bytearray(65536)
if art == "kurz": img = img[:40960]
open(ziel, "wb").write(bytes(img))
PY
}
spiel() { sfo $W/user/appmeta/$1/param.sfo "TITLE=$2" CATEGORY=gd APP_VER=01.00 TITLE_ID=$1; }
# CUSA30001 intern: Spiel + Update + Zusatzinhalt + Fake-Lizenz + PlayGo + App-Daten
gr $W/user/app/CUSA30001/app.pkg 4096; spiel CUSA30001 "Spiel Eins"
gr $W/user/patch/CUSA30001/patch.pkg 2048; sfo $W/user/patch/CUSA30001/sce_sys/param.sfo "TITLE=Spiel Eins" APP_VER=01.05
gr $W/user/addcont/CUSA30001/DLC0000000000001/ac.pkg 10
gr $W/user/license/fakeEP1004-CUSA30001_00.rif 10; gr $W/user/license/fakeEP1004-CUSA30001_00.idx 10
gr $W/user/playgo/app/EP1004-CUSA30001_00-AAAAAAAAAAAAAAAA/00/d0.pdb 10
gr $W/user/download/CUSA30001/x.dat 10
# NEU 1.0.8 (Arbeitsplan 2.2): PKG-Dateien auf dem USB-Stick - Spiel, Update, kein echtes PKG (Name ohne Spielnummer)
pkgdatei $W/mnt/usb0/Spiel_Eins.pkg "Spiel Eins" CUSA30001 gd 01.00
pkgdatei $W/mnt/usb0/Update_Eins.pkg "Spiel Eins" CUSA30001 gp 01.05
gr $W/mnt/usb0/kaputt.pkg 5000
pkgdatei $W/mnt/usb0/Zusatz_Eins.pkg "Bonuspaket" CUSA30001 ac 01.00   # Codex-Prüfung 71: Lesegrenzen und Sonderfälle
pkgdatei $W/mnt/usb0/ohne_sfo.pkg "X" CUSA30001 gd 01.00 ohne_sfo
pkgdatei $W/mnt/usb0/sfo_gross.pkg "X" CUSA30001 gd 01.00 sfo_gross
pkgdatei $W/mnt/usb0/abgeschnitten.pkg "X" CUSA30001 gd 01.00 abgeschnitten
pkgdatei $W/mnt/usb0/grenze.pkg "Spiel Grenze" CUSA30001 gd 01.00 grenze
pkgdatei $W/mnt/usb0/weit.pkg "X" CUSA30001 gd 01.00 weit
pkgdatei $W/mnt/usb0/ohne_id.pkg "Spiel Ohne" CUSA30002 gd 01.00 ohne_id
pkgdatei $W/mnt/usb0/boese.pkg "X" CUSA30001 gd 01.00 boese
pkgdatei $W/mnt/usb0/viele.pkg "X" CUSA30001 gd 01.00 viele; pkgdatei $W/mnt/usb0/sfo_weg.pkg "X" CUSA30001 gd 01.00 sfo_weg   # 05.11.2026
# NEU 1.0.8 (Arbeitsplan 2.1): Spiel-Abbilder, deren Dateiname nur die Spielnummer hat - Spielname aus dem Abbild
exfatbild $W/mnt/usb0/PPSA30010.exfat normal "Spiel Abbild" "Image Game" PPSA30010
exfatbild $W/mnt/usb0/PPSA30011-app.exfat am_stueck "Am Stueck" "In One Piece" PPSA30011
exfatbild $W/mnt/usb0/PPSA30012.exfat unterordner "Im Unterordner" "In Subfolder" PPSA30012
exfatbild $W/mnt/usb0/CUSA30020.exfat sfo "PS4 Abbild" "PS4 Image" CUSA30020
exfatbild $W/mnt/usb0/PPSA30013.img kein_exfat "X" "X" PPSA30013
exfatbild $W/mnt/usb0/PPSA30014.exfat kaputt_kopf "X" "X" PPSA30014
exfatbild $W/mnt/usb0/PPSA30015.exfat schleife "Schleife" "Loop" PPSA30015
exfatbild $W/mnt/usb0/PPSA30016.exfat riesig "X" "X" PPSA30016
exfatbild $W/mnt/usb0/PPSA30017.exfat ohne_sys "X" "X" PPSA30017
exfatbild $W/mnt/usb0/PPSA30019.exfat kurz "X" "X" PPSA30019
exfatbild "$W/mnt/usb0/Spiel Mit Namen - PPSA30018.exfat" normal "Anderer Name" "Other Name" PPSA30018
exfatbild "$W/mnt/usb0/Mein Spiel.exfat" normal "Ohne Nummer" "No Number" PPSA30021   # NEU 07.11.2026 (PS5-Test 16:31): ohne Nummer im Namen
exfatbild $W/mnt/usb0/PPSA30022.exfat bild_kette "Bild Kette" "Picture Chain" PPSA30022
exfatbild $W/mnt/usb0/PPSA30023.exfat bild_verstreut "Bild Verstreut" "Picture Scattered" PPSA30023
# CUSA30002 intern, echte Lizenzdatei
gr $W/user/app/CUSA30002/app.pkg 1000; spiel CUSA30002 "Spiel Zwei"; gr $W/user/license/CUSA30002.rif 10
# CUSA30003 nur auf der M.2
gr $W/mnt/ext1/user/app/CUSA30003/app.pkg 1000; spiel CUSA30003 "Spiel Drei"
# CUSA30004: leerer Ordner (wie nach "Datenbank neu aufbauen" beobachtet), Kachel-Infos und Fake-Lizenz blieben
mkdir -p $W/user/app/CUSA30004; spiel CUSA30004 "Spiel Vier"; gr $W/user/license/fakeEP1004-CUSA30004_00.rif 10
# CUSA30005: kein app.pkg, aber andere Datei (unklar)
gr $W/user/app/CUSA30005/app.pkg.part 10; spiel CUSA30005 "Spiel Fuenf"; gr $W/user/license/fakeEP1004-CUSA30005_00.rif 10
# CUSA30006: zwei Kopien (intern + M.2)
gr $W/user/app/CUSA30006/app.pkg 500; gr $W/mnt/ext1/user/app/CUSA30006/app.pkg 500; spiel CUSA30006 "Spiel Sechs"; gr $W/user/license/fakeEP1004-CUSA30006_00.rif 10
# NEU 1.0.8 (Codex-Prüfung 73): CUSA30009 auf der M.2 installiert, intern nur ein leerer App-Ordner - keine zweite Kopie
mkdir -p $W/user/app/CUSA30009; gr $W/mnt/ext1/user/app/CUSA30009/app.pkg 900; spiel CUSA30009 "Spiel Neun"
# CUSA30007 auf dem USB-Speicher (erweiterter Speicher) mit Update
gr $W/mnt/ext0/user/app/CUSA30007/app.pkg 700; gr $W/mnt/ext0/user/patch/CUSA30007/patch.pkg 70; spiel CUSA30007 "Spiel Sieben"
# PS5-Spiel und Ordner ohne PS4-Bezug
mkdir -p $W/user/app/PPSA10001 $W/user/appmeta/PPSA10001
printf '{"titleId":"PPSA10001","applicationCategoryType":0,"localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"PS5 Spiel"}}}' > $W/user/appmeta/PPSA10001/param.json
gr $W/user/data/MeinOrdner/a.txt 1; gr $W/user/data/Zuordnung/a.txt 1
# Datenbanken (nachgebildet: gleiche Tabellennamen wie auf der PS5 gelesen, Inhalt erfunden). NEU 21:50: app.db im WAL-Modus mit
# NICHT zurückgeschriebenen Einträgen (wie eine laufende Konsole), appinfo.db im Rollback-Modus (so wie auf der PS5 FW 12.00 gelesen)
python3 - "$T" <<'PY'
import sqlite3, sys, os, shutil
t = sys.argv[1]
for o in ("mms", "mms_kaputt", "mms_fehlt", "mms_fremd", "mms_journal", "mms_defekt", "mms_walkaputt", "mms_walmuell", "arbeit"): os.makedirs(os.path.join(t, o), exist_ok=True)
w = os.path.join(t, "arbeit", "app.db")
c = sqlite3.connect(w); c.execute("PRAGMA journal_mode=WAL"); c.execute("PRAGMA wal_autocheckpoint=0")
c.execute("CREATE TABLE t (a INTEGER PRIMARY KEY, b TEXT)"); c.execute("CREATE INDEX i ON t(b)")
c.executemany("INSERT INTO t(b) VALUES (?)", [("CUSA%05d" % i * 10,) for i in range(3000)]); c.commit()
c.execute("DELETE FROM t WHERE a % 3 = 0"); c.commit()
shutil.copy(w, os.path.join(t, "mms", "app.db")); shutil.copy(w + "-wal", os.path.join(t, "mms", "app.db-wal"))   # Kopie bei offener Verbindung
c.close()
r = os.path.join(t, "arbeit", "appinfo.db")
b = sqlite3.connect(r); b.execute("CREATE TABLE tbl_appinfo (titleId TEXT, metaDataId INTEGER, key TEXT, val TEXT)"); b.execute("CREATE INDEX ix ON tbl_appinfo(titleId, key)")
b.executemany("INSERT INTO tbl_appinfo VALUES (?, 0, 'TITLE', ?)", [("CUSA%05d" % i, "Spiel %d" % i) for i in range(800)]); b.commit(); b.close()
shutil.copy(r, os.path.join(t, "mms", "appinfo.db")); open(os.path.join(t, "mms", "appinfo.db-journal"), "wb").close()   # leeres Journal (abgeschlossen)
d = open(r, "rb").read()
for o in ("mms_kaputt", "mms_fehlt", "mms_fremd", "mms_journal", "mms_defekt", "mms_walkaputt", "mms_walmuell"): shutil.copy(r, os.path.join(t, o, "appinfo.db"))
open(os.path.join(t, "mms_kaputt", "app.db"), "wb").write(d[:-4096])
os.remove(os.path.join(t, "mms_fehlt", "appinfo.db")); shutil.copy(r, os.path.join(t, "mms_fehlt", "app.db"))
open(os.path.join(t, "mms_fremd", "app.db"), "wb").write(b"kein SQLite\n" * 400)
shutil.copy(r, os.path.join(t, "mms_journal", "app.db")); open(os.path.join(t, "mms_journal", "app.db-journal"), "wb").write(bytes.fromhex("d9d505f920a163d7") + b"\0" * 504)
# beschädigter Index: Zeilen der Tabelle verändern, ohne den Index anzupassen (Seite mitten in der Tabelle)
k = bytearray(d); seite = int.from_bytes(d[16:18], "big"); n = len(d) // seite
k[seite * 3 - 120: seite * 3 - 20] = b"\xff" * 100; open(os.path.join(t, "mms_defekt", "app.db"), "wb").write(bytes(k))
mw = open(os.path.join(t, "mms", "app.db-wal"), "rb").read()
shutil.copy(os.path.join(t, "mms", "app.db"), os.path.join(t, "mms_walkaputt", "app.db")); wk = bytearray(mw); wk[24] ^= 0xff; open(os.path.join(t, "mms_walkaputt", "app.db-wal"), "wb").write(bytes(wk))
shutil.copy(os.path.join(t, "mms", "app.db"), os.path.join(t, "mms_walmuell", "app.db"))
open(os.path.join(t, "mms_walmuell", "app.db-wal"), "wb").write(mw + mw[32:32 + 24 + seite][:8] + b"\x00" * 16 + b"Z" * seite)   # abgebrochener letzter Eintrag
PY
(cd "$T/mms"; sha1sum app.db app.db-wal appinfo.db appinfo.db-journal) > "$T/mms.sha1"
(cd $T; $B/test_ps4 $T)
date "+%Y-%m-%d %H:%M:%S FERTIG"
