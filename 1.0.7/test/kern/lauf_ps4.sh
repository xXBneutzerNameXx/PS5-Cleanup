#!/bin/bash
# PS5 Cleanup 1.0.7 - Test PS4-Spieleschutz + Datenbank-Sicherung + Ende ohne Neuaufbau-Empfehlung (da_Chris86 31.10.2026 19:56)
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
   $KERN $KK/db/db_pruefen.c $KK/db/db_sicherung.c $B/db_sqlite.o $KK/bau/cjson/cJSON.c $KK/test/kern/test_ps4.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_ps4 || { echo BAU FEHLGESCHLAGEN; exit 1; }
# NEU 1.0.7 (21:50, Punkt 5): Schreibsperre der Test-/Prüffassungen am PC nachweisen
SP=0; for q in $KERN $KK/db/db_pruefen.c $KK/db/db_sicherung.c; do gcc -c -O1 -w -include $KK/bau/nurlesen.h -DNUR_LESEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson "$q" -o $B/nl_$(basename "$q" .c).o || SP=1; done
[ $SP = 0 ] && echo "OK   Schreibsperre: Kern und Datenbank-Prüfung lassen sich mit bau/nurlesen.h übersetzen" || echo "FAIL Schreibsperre: Übersetzen mit bau/nurlesen.h scheitert"
NLT=$(nm -u $B/nl_*.o | grep -w -E "fwrite|fputs|fputc|fprintf|write|pwrite|unlink|unlinkat|rmdir|mkdir|mkdirat|rename|renameat|truncate|ftruncate|chmod|fchmod|fchmodat|link|linkat|symlink|nmount|mount|unmount|openat" | sort -u)
[ -z "$NLT" ] && echo "OK   Schreibsperre: keine schreibende Funktion in den Objektdateien ohne KERN_LOESCHEN" || echo "FAIL Schreibsperre: $NLT"
FO=0; for q in $KERN $KK/db/db_pruefen.c $KK/db/db_sicherung.c; do gcc -E -include $KK/bau/nurlesen.h -DNUR_LESEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson "$q" 2>/dev/null | python3 -I $KK/bau/fopen_pruefen.py "$(basename $q)" > /dev/null || FO=1; done
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
spiel() { sfo $W/user/appmeta/$1/param.sfo "TITLE=$2" CATEGORY=gd APP_VER=01.00 TITLE_ID=$1; }
# CUSA30001 intern: Spiel + Update + Zusatzinhalt + Fake-Lizenz + PlayGo + App-Daten
gr $W/user/app/CUSA30001/app.pkg 4096; spiel CUSA30001 "Spiel Eins"
gr $W/user/patch/CUSA30001/patch.pkg 2048; sfo $W/user/patch/CUSA30001/sce_sys/param.sfo "TITLE=Spiel Eins" APP_VER=01.05
gr $W/user/addcont/CUSA30001/DLC0000000000001/ac.pkg 10
gr $W/user/license/fakeEP1004-CUSA30001_00.rif 10; gr $W/user/license/fakeEP1004-CUSA30001_00.idx 10
gr $W/user/playgo/app/EP1004-CUSA30001_00-AAAAAAAAAAAAAAAA/00/d0.pdb 10
gr $W/user/download/CUSA30001/x.dat 10
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
for o in ("mms", "mms_kaputt", "mms_fehlt", "mms_fremd", "mms_journal", "mms_defekt", "mms_walkaputt", "mms_walmuell", "stick", "arbeit"): os.makedirs(os.path.join(t, o), exist_ok=True)
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
