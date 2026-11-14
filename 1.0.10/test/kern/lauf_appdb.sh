#!/bin/bash
# PS5 Cleanup - Test der Datenbank-Übersicht (NEU 1.0.10): app.db nur lesen, jeden Titel einordnen. Läuft in WSL.
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; mkdir -p "$K/test/kern/ergebnis" $B
exec > "$K/test/kern/ergebnis/appdb.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O1 -w -c $KK/db/db_sqlite.c -o $B/db_sqlite_appdb.o || { echo "FAIL BAU FEHLGESCHLAGEN"; exit 1; }
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/db/db_pruefen.c $KK/db/db_lesen.c $B/db_sqlite_appdb.o $KK/bau/cjson/cJSON.c $KK/test/kern/test_appdb.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -lm -o $B/test_appdb || { echo "FAIL BAU FEHLGESCHLAGEN"; exit 1; }
T=$HOME/ps5c-bau/appdb; rm -rf $T; mkdir -p $T/neu $T/alt
python3 - "$T" <<'PY'
import sqlite3, sys, os
t = sys.argv[1]
def baue(pfad, spalten, zeilen):
    c = sqlite3.connect(os.path.join(pfad, "app.db"))
    c.execute("CREATE TABLE tbl_contentinfo (titleId TEXT PRIMARY KEY, %s)" % ", ".join(spalten))
    for z in zeilen: c.execute("INSERT INTO tbl_contentinfo VALUES (%s)" % ",".join("?" * len(z)), z)
    c.commit(); c.close()
S = ["titleName TEXT", "uninstallable INTEGER", "contentLocation INTEGER", "appType INTEGER", "hddLocation INTEGER", "externalHddAppStatus INTEGER", "localConceptId TEXT"]
baue(os.path.join(t, "neu"), S, [
  ("NPXS40037", "System-App", 0, 2, 0, 0, 0, "cid:local:NPXS40037"),    # s
  ("PPSA01650", "Download", 1, 0, 5, 0, 0, "cid:scp:1"),                  # p - Platzhalter (fehlender Ordner ist normal)
  ("PPSA10000", "Gefunden", 1, 2, 0, 0, 0, "cid:local:PPSA10000"),        # i - Installation gefunden
  ("PPSA20000", "Sony kennt ihn", 1, 2, 0, 0, 0, "cid:local:PPSA20000"),  # i - Sony führt ihn
  ("PPSA20001", "Geist", 1, 2, 0, 0, 0, "cid:local:PPSA20001"),           # v - Sony nein, keine Installation
  ("TBSH00001", "TabShifter", 1, 2, 0, 0, 0, "cid:local:TBSH00001"),      # v - nur in der Datenbank angelegt
  ("CUSA40000", "PS4 extern", 1, 2, 0, 1, 0, "cid:local:CUSA40000"),      # x - erweiterter Speicher, nicht angeschlossen
  ("PPSA50000", "Spiel", 1, 2, 0, 0, 0, "cid:scp:5"),                     # w - Spielgruppe geteilt
  ("PPSA50001", "Spiel PS5", 1, 2, 0, 0, 0, "cid:scp:5"),                 # w
  ("PPSA60000", "Sony stumm", 1, 2, 0, 0, 0, "cid:local:PPSA60000"),      # u - Sony nicht fragbar
  ("PPSA30000", "Name; mit Semikolon", 1, 2, 0, 0, 0, "cid:local:PPSA30000"),   # v
  ("PPSA70000", "Komischer Ort", 1, 7, 0, 0, 0, "cid:local:PPSA70000"),   # u - unbekannter Speicherort-Wert
])
# ältere Firmware: Spalten hddLocation/externalHddAppStatus/localConceptId fehlen
baue(os.path.join(t, "alt"), S[:4], [("PPSA20001", "Geist", 1, 2, 0), ("NPXS40037", "Sys", 0, 2, 0), ("PPSA01650", "DL", 1, 0, 5)])
PY
RC=0
echo "== aktueller Aufbau, kein erweiterter Speicher angeschlossen"
$B/test_appdb $T/neu "NPXS40037=s,PPSA01650=p,PPSA10000=i,PPSA20000=k,PPSA20001=v,TBSH00001=v,CUSA40000=x,PPSA50000=w,PPSA50001=w,PPSA60000=u,PPSA30000=v,PPSA70000=u" 0 || RC=1
echo "== derselbe Aufbau, erweiterter Speicher angeschlossen: der externe Titel wird normal eingeordnet (Sony: nein -> ohne Installation)"
$B/test_appdb $T/neu "CUSA40000=v,PPSA20001=v" 1 || RC=1
echo "== ältere Firmware (Spalten fehlen): ohne Speicherort-Angabe nie \"ohne Installation\""
$B/test_appdb $T/alt "PPSA20001=u,NPXS40037=s,PPSA01650=p" 0 || RC=1
echo "== keine app.db"
mkdir -p $T/leer; $B/test_appdb $T/leer "X" 0 > $T/leer.out 2>&1; grep -q "gelesen: -1 Titel - app.db fehlt" $T/leer.out && echo "OK   fehlende app.db gemeldet, nichts eingeordnet" || { echo "FAIL fehlende app.db"; cat $T/leer.out; RC=1; }
date "+%Y-%m-%d %H:%M:%S FERTIG"; exit $RC
