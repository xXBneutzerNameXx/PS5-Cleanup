#!/bin/bash
# NEU 1.0.7 (da_Chris86 01.11.2026): Bilder/Videos und Spielstände ohne leere Einträge, Reste ohne Kachel - läuft in WSL
#  Teil 1: Ablauf im Kern (test_sauber.c) mit nachgestellten Diensten (MEDIEN, SPIELSTAND, UNINSTALL_FEHLT)
#  Teil 2: app/spielstand_db.c gegen echte SQLite-Dateien mit dem Aufbau von Sonys savedata.db (PS5 Pro FW 12.00)
#  Teil 3: appinfo_zusatz_reparieren gegen eine SQLite-Datei im Aufbau von Sonys appinfo.db (Befund Red Dead 01.11.2026)
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/sauber; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/sauber.log" 2>&1
set -o pipefail
RC=0
date "+%Y-%m-%d %H:%M:%S Start"
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_sauber.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_sauber || { echo BAU FEHLGESCHLAGEN; exit 1; }
gcc -O2 -g -Wall -std=gnu11 -D_DEFAULT_SOURCE -c $KK/app/spielstand_db.c -o $B/spielstand_db.o || { echo BAU FEHLGESCHLAGEN spielstand_db; exit 1; }
gcc -O2 -g -Wall $B/spielstand_db.o $KK/test/kern/test_spielstand_db.c -lm -o $B/test_spielstand_db || { echo BAU FEHLGESCHLAGEN test_spielstand_db; exit 1; }
gcc -O2 -g -Wall $B/spielstand_db.o $KK/test/kern/test_appinfo_db.c -lm -o $B/test_appinfo_db || { echo BAU FEHLGESCHLAGEN test_appinfo_db; exit 1; }
gcc -O2 -g -Wall $B/spielstand_db.o $KK/test/kern/test_kachel_db.c -lm -o $B/test_kachel_db || { echo BAU FEHLGESCHLAGEN test_kachel_db; exit 1; }
G=$(nm -g --defined-only $B/spielstand_db.o | awk '{print $3}' | tr '\n' ' ')
if [ "$G" = "appinfo_zusatz_reparieren kachel_db_austragen spielstand_db_austragen " ]; then echo "OK   spielstand_db.o: nach außen nur appinfo_zusatz_reparieren, kachel_db_austragen und spielstand_db_austragen (eigene SQLite-Kopie ganz intern)"; else echo "FAIL spielstand_db.o zeigt nach außen: $G"; RC=1; fi

app() {  # id titel
  mkdir -p "$T/w/user/app/$1/sce_sys" "$T/w/user/appmeta/$1" "$T/w/system_data/priv/appmeta/$1"
  printf '{"titleId":"%s","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"%s"}},"contentVersion":"01.00"}' "$1" "$2" > "$T/w/user/app/$1/sce_sys/param.json"
  echo x > "$T/w/user/app/$1/eboot.bin"; echo '{}' > "$T/w/user/appmeta/$1/param.json"; echo '{}' > "$T/w/system_data/priv/appmeta/$1/param.json"
}
baum() {
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  U=$T/w/user/home/1a2b3c4d
  mkdir -p $U/savedata_prospero/PPSA03530 $U/savedata_prospero_meta/user/PPSA03530 $U/savedata/CUSA09172
  echo s > $U/savedata_prospero/PPSA03530/sdimg_GameUserSettingsini; echo b > $U/savedata_prospero/PPSA03530/GameUserSettingsini.bin
  echo p > $U/savedata_prospero_meta/user/PPSA03530/GameUserSettingsini_icon0.png
  echo s > $U/savedata/CUSA09172/sdimg_SAVEGAMEfinal1.lanoire
  mkdir -p $U/savedata/CUSA16074; echo s > $U/savedata/CUSA16074/sdimg_SAVEDATA00   # Lua-Jailbreak-Spiel (Raspberry Cube): Spielstand ist ein Jailbreak-Eintrag
  AV=$T/w/user/av_contents
  mkdir -p $AV/photo/NPXS40087/PPSA03530/318 $AV/video/NPXS40087/PPSA03530/0bd $AV/thumbnails/photo/NPXS40087/PPSA03530/318 $AV/thumbnails/video/NPXS40087/PPSA03530/0bd
  F=$AV/photo/NPXS40087/PPSA03530/318/20261101_174354_00830999; echo j > $F.jxr; echo m > $F.meta; echo d > $F.dat
  V=$AV/video/NPXS40087/PPSA03530/0bd/20261101_174353_00523513; echo w > $V.webm; echo m > $V.meta; echo d > $V.dat
  echo t > $AV/thumbnails/photo/NPXS40087/PPSA03530/318/20261101_174354_00830999.jxr.jxr
  echo t > $AV/thumbnails/video/NPXS40087/PPSA03530/0bd/20261101_174353_00523513.webm.jxr
  echo t > $AV/thumbnails/photo/NPXS40087/PPSA03530/318/alt_ohne_aufnahme.jxr.jxr   # Vorschaubild ohne Aufnahme (Rest)
  app PPSA68888 "Reste Spiel"   # App-Ordner, aber nicht (mehr) im PS5-Menü (UNINSTALL_FEHLT)
}
# 1.0.10 (da_Chris86 12.11.2026: Rechenleistung des Mini-PCs nutzen): die drei Teile sind voneinander unabhängig (eigene Ordner
# $T/f_*, $T/db, $T/ai) und laufen gleichzeitig; Ausgabe danach in der bisherigen Reihenfolge, Fehler zählen wie bisher.
teil1() {
echo "== Teil 1: Ablauf im Kern"
# 1.0.10 (da_Chris86 12.11.2026: Rechenleistung des Mini-PCs nutzen): die Fälle laufen gleichzeitig, jeder mit eigenem Arbeitsordner;
# Ausgabe danach in der bisherigen Reihenfolge, ein Fehler eines Falls zählt wie bisher
TS=$T
fall_s() {
  local fall=$1 T=$TS/f_$1; mkdir -p $T
  baum
  local M= S= N= UN=ok
  case $fall in ok|sim) M=ok; S=ok;; ohne) UN=;; ablehnen) M=ablehnen; S=ablehnen;; belegt) M=ok; S=belegt;; teil) M=ok; S=ok; N=20261101_174353_00523513.webm;; esac
  (cd $T; MEDIEN=$M MEDIEN_NIE=$N SPIELSTAND=$S UNINSTALL=$UN UNINSTALL_FEHLT=PPSA68888 UNINSTALL_NIE=PSCL00002 RMDA=1 $B/test_sauber $T $fall) | grep -v "^LOG:" || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert, Fall $fall)"; return 1; }
}
for fall in ok ohne ablehnen belegt teil sim; do { fall_s $fall > $TS/_aus_$fall.txt 2>&1; echo $? > $TS/_rc_$fall; } & done
wait
for fall in ok ohne ablehnen belegt teil sim; do cat $TS/_aus_$fall.txt; [ "$(cat $TS/_rc_$fall 2>/dev/null)" = 0 ] || RC=1; done

echo $RC > $T/_rc_teil1
}
teil2() {
echo "== Teil 2: app/spielstand_db.c gegen SQLite-Dateien im Aufbau von Sonys savedata.db"
D=$T/db; rm -rf $D; mkdir -p $D   # seit 02.11.2026 ohne Sicherung (da_Chris86: auf dem USB-Stick nur das Protokoll)
mkdb() {  # datei - Aufbau wie /system_data/savedata_prospero/<Benutzer>/db/user/savedata.db (PS5 Pro FW 12.00, Rollback-Journal)
python3 -I - "$1" <<'EOF'
import sqlite3, sys
c = sqlite3.connect(sys.argv[1])
c.executescript("""
CREATE TABLE savedata (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,title_id NOT NULL, dir_name NOT NULL, main_title NOT NULL, sub_title , detail , tmp_dir_name , is_broken , user_param , blocks , free_blocks , size_kib , mtime NOT NULL, fake_broken , account_id , user_id , faked_owner , cloud_icon_url , cloud_revision , game_title_id NOT NULL, system_blocks , convert_performance , account_id_in_usb_path );
CREATE INDEX savedata_index on savedata(title_id, dir_name, user_param, size_kib, mtime, game_title_id);
""")
for t, d in [("PPSA03530", "GameUserSettingsini"), ("PPSA03530", "Zweiter"), ("CUSA09172", "SAVEGAMEfinal1.lanoire"), ("PPSA01650", "STARBOARD")]:
    c.execute("INSERT INTO savedata (title_id, dir_name, main_title, mtime, game_title_id) VALUES (?,?,?,?,?)", (t, d, "Titel " + t, 1761980000, t))
c.commit(); c.close()
EOF
}
zeilen() { python3 -I -c "import sqlite3,sys; c=sqlite3.connect('file:'+sys.argv[1]+'?mode=ro',uri=True); print(c.execute('select count(*) from savedata').fetchone()[0], c.execute('select count(*) from savedata where title_id=?',(sys.argv[2],)).fetchone()[0], c.execute('pragma integrity_check').fetchone()[0])" "$1" "$2"; }
fall() {  # name erwartung(0|-1) db titel [gleich]  - gleich: auch bei 0 muss die Liste Byte für Byte gleich bleiben
  local name=$1 soll=$2 gleich=$5; shift 2
  local vorher=$(sha256sum "$1" 2>/dev/null | cut -c1-16)
  local aus=$($B/test_spielstand_db "$1" "$2")
  local nachher=$(sha256sum "$1" 2>/dev/null | cut -c1-16)
  echo "INFO $name: $aus"
  case "$aus" in "ERGEBNIS r=$soll "*) echo "OK   $name: Rückgabe $soll";; *) echo "FAIL $name: Rückgabe nicht $soll"; RC=1;; esac
  if [ "$soll" = "-1" ] || [ "$gleich" = "gleich" ]; then
    if [ "$vorher" = "$nachher" ]; then echo "OK   $name: Spielstand-Liste unverändert"; else echo "FAIL $name: Spielstand-Liste verändert"; RC=1; fi
  fi
}
# 1. normaler Fall: zwei Einträge eines Titels
mkdb $D/a.db
fall "zwei Einträge austragen" 0 $D/a.db PPSA03530
z=$(zeilen $D/a.db PPSA03530); [ "$z" = "2 0 ok" ] && echo "OK   danach 2 Einträge übrig, keiner von PPSA03530, integrity ok" || { echo "FAIL danach: $z"; RC=1; }
[ ! -e $D/a.db-journal ] && echo "OK   kein Journal übrig" || { echo "FAIL Journal übrig"; RC=1; }
[ "$(ls $D)" = "a.db" ] && echo "OK   keine weitere Datei angelegt (keine Sicherung)" || { echo "FAIL weitere Dateien: $(ls $D)"; RC=1; }
# 2. Titel ohne Einträge: nichts ändern
fall "keine Einträge" 0 $D/a.db XXXX00000 gleich
mkdb $D/b.db
# 5. Liste gerade belegt (anderer Prozess schreibt) -> nach 3 s aufgeben, nichts ändern
python3 -I -c "import sqlite3,time,sys; c=sqlite3.connect(sys.argv[1],isolation_level=None); c.execute('BEGIN EXCLUSIVE'); open(sys.argv[2],'w').close(); time.sleep(8)" $D/b.db $D/belegt.flag &
BP=$!; for i in $(seq 50); do [ -e $D/belegt.flag ] && break; sleep 0.1; done
t0=$(date +%s); fall "Liste belegt" -1 $D/b.db PPSA03530; t1=$(date +%s)
[ $((t1 - t0)) -ge 2 ] && [ $((t1 - t0)) -le 6 ] && echo "OK   etwa 3 s gewartet ($((t1 - t0)) s)" || { echo "FAIL Wartezeit $((t1 - t0)) s"; RC=1; }
wait $BP
# 6. keine Datenbank / fehlt / falscher Aufbau
head -c 16384 /dev/urandom > $D/kaputt.db
fall "keine SQLite-Datei" -1 $D/kaputt.db PPSA03530
fall "Liste fehlt" -1 $D/fehlt.db PPSA03530
[ ! -e $D/fehlt.db ] && echo "OK   fehlende Liste nicht angelegt" || { echo "FAIL fehlende Liste wurde angelegt"; RC=1; }
python3 -I -c "import sqlite3,sys; c=sqlite3.connect(sys.argv[1]); c.execute('create table andere (x)'); c.commit()" $D/fremd.db
fall "falscher Aufbau (keine Tabelle savedata)" -1 $D/fremd.db PPSA03530
# 7. beschädigte Liste (Seite überschrieben) -> quick_check, nichts ändern
mkdb $D/d.db; python3 -I -c "import sys; f=open(sys.argv[1],'r+b'); f.seek(4096+8); f.write(b'\xff'*200); f.close()" $D/d.db
fall "beschädigte Liste" -1 $D/d.db PPSA03530

echo $RC > $T/_rc_teil2
}
teil3() {
echo "== Teil 3: appinfo_zusatz_reparieren gegen SQLite-Dateien im Aufbau von Sonys appinfo.db (Befund Red Dead 01.11.2026)"
E=$T/ai; rm -rf $E; mkdir -p $E   # seit 02.11.2026 ohne Sicherung
mkai() {  # datei - Aufbau wie /system_data/priv/mms/appinfo.db (PS5 Pro FW 12.00, Rollback-Journal), Werte wie dort gefunden
python3 -I - "$1" <<'EOF'
import sqlite3, sys
c = sqlite3.connect(sys.argv[1])
c.executescript("""
CREATE TABLE tbl_version(category TEXT NOT NULL,status INT,PRIMARY KEY(category));
CREATE TABLE tbl_appinfo(titleId TEXT NOT NULL,metaDataId TEXT,key INT NOT NULL,val,PRIMARY KEY(titleId, metaDataId, key));
""")
m = 'prior:internal:0'
rows = []
for i in range(1, 8): rows.append(('CUSA08519', m, 'SERVICE_ID_ADDCONT_ADD_%d' % i, '\r\n  '))        # Red Dead mit umgebautem Patch
rows.append(('CUSA09172', m, 'SERVICE_ID_ADDCONT_ADD_1', 'EP1004-CUSA15490_00'))                     # L.A. Noire: echte Kennung
for i in range(2, 8): rows.append(('CUSA09172', m, 'SERVICE_ID_ADDCONT_ADD_%d' % i, 0))             # unbenutzt = 0
for i in range(1, 8): rows.append(('PPSA01650', m, 'SERVICE_ID_ADDCONT_ADD_%d' % i, ''))             # leer
for i in range(1, 8): rows.append(('PPSA03524', m, 'SERVICE_ID_ADDCONT_ADD_%d' % i, ' ' * 19))       # 03.11.2026: Platzhalter aus Leerzeichen (unbenutzt)
rows.append(('CUSA77778', m, 'SERVICE_ID_ADDCONT_ADD_1', '  0'))                                      # Codex 47 (2): kein genaues "0" -> kaputt
rows += [('CUSA08519', m, 'TITLE', 'Red Dead Redemption 2'), ('CUSA08519', m, 'APP_VER', '01.32'), ('CUSA09172', m, 'TITLE', 'L.A. Noire'),
         ('CUSA77777', m, 'SERVICE_ID_ADDCONT_ADD_1', 'EP1004-CUSA1549'), ('CUSA77777', m, 'SERVICE_ID_ADDCONT_ADD_2', 'ep1004-cusa15490_00')]
c.executemany("INSERT INTO tbl_appinfo VALUES (?,?,?,?)", rows)
c.commit(); c.close()
EOF
}
werte() { python3 -I -c "import sqlite3,sys; c=sqlite3.connect('file:'+sys.argv[1]+'?mode=ro',uri=True); print(repr(sorted(set((r[0],r[1]) for r in c.execute(\"select typeof(val), val from tbl_appinfo where titleId=? and key glob 'SERVICE_ID_ADDCONT_ADD_*'\",(sys.argv[2],))))), c.execute('pragma integrity_check').fetchone()[0])" "$1" "$2"; }
afall() {  # name erwartung(0|-1|-2) zeilen db titel
  local name=$1 soll=$2 sz=$3; shift 3
  local vorher=$(sha256sum "$1" 2>/dev/null | cut -c1-16)
  local aus=$($B/test_appinfo_db "$1" "$2")
  local nachher=$(sha256sum "$1" 2>/dev/null | cut -c1-16)
  echo "INFO $name: $aus"
  case "$aus" in "ERGEBNIS r=$soll zeilen=$sz "*) echo "OK   $name: Rückgabe $soll, $sz Werte";; *) echo "FAIL $name: nicht r=$soll zeilen=$sz"; RC=1;; esac
  if [ "$soll" != "0" ] || [ "$sz" = "0" ]; then
    if [ "$vorher" = "$nachher" ]; then echo "OK   $name: App-Datenbank unverändert"; else echo "FAIL $name: App-Datenbank verändert"; RC=1; fi
  fi
}
# 1. Red Dead: 7 kaputte Werte -> 0 (ohne Sicherung, seit 02.11.2026)
mkai $E/a.db
afall "Red Dead: 7 kaputte Werte reparieren" 0 7 $E/a.db CUSA08519
w=$(werte $E/a.db CUSA08519); [ "$w" = "[('integer', 0)] ok" ] && echo "OK   danach alle 7 Werte = 0 (Zahl), integrity ok" || { echo "FAIL danach: $w"; RC=1; }
w=$(werte $E/a.db CUSA09172); [ "$w" = "[('integer', 0), ('text', 'EP1004-CUSA15490_00')] ok" ] && echo "OK   L.A. Noire unverändert" || { echo "FAIL L.A. Noire: $w"; RC=1; }
w=$(werte $E/a.db PPSA01650); [ "$w" = "[('text', '')] ok" ] && echo "OK   YouTube (leer) unverändert" || { echo "FAIL YouTube: $w"; RC=1; }
[ ! -e $E/a.db-journal ] && echo "OK   kein Journal übrig" || { echo "FAIL Journal übrig"; RC=1; }
[ "$(ls $E)" = "a.db" ] && echo "OK   keine weitere Datei angelegt (keine Sicherung)" || { echo "FAIL weitere Dateien: $(ls $E)"; RC=1; }
# 2. nochmal: nichts mehr kaputt -> nichts ändern
afall "Red Dead nochmal" 0 0 $E/a.db CUSA08519
# 3. gültige Werte (Kennung, 0, leer) bleiben
afall "L.A. Noire (gültig)" 0 0 $E/a.db CUSA09172
afall "YouTube (leer)" 0 0 $E/a.db PPSA01650
# 3b. NEU 03.11.2026 (Codex-Prüfung 47): Platzhalter aus Leerzeichen bleiben unverändert, "  0" ist kaputt
afall "Platzhalter aus 19 Leerzeichen (unbenutzt)" 0 0 $E/a.db PPSA03524
w=$(werte $E/a.db PPSA03524); [ "$w" = "[('text', '                   ')] ok" ] && echo "OK   Platzhalter unverändert" || { echo "FAIL Platzhalter: $w"; RC=1; }
afall "\"  0\" (Leerzeichen vor der 0) ist kaputt" 0 1 $E/a.db CUSA77778
# 4. kaputt (zu kurz, Kleinbuchstaben) -> repariert, wie bei Red Dead (da_Chris86 02.11.2026: gilt für alle Spiele)
afall "kaputt (zu kurz, Kleinbuchstaben)" 0 2 $E/a.db CUSA77777
w=$(werte $E/a.db CUSA77777); [ "$w" = "[('integer', 0)] ok" ] && echo "OK   danach beide Werte = 0, integrity ok" || { echo "FAIL danach: $w"; RC=1; }
# 5. kaputte Werte da, aber die Datenbank ist beschädigt (Prüfung unter der Sperre schlägt fehl) -> -1, nichts geändert (Prüfung p8, Befund 8)
mkai $E/c.db
python3 -I - $E/c.db <<'EOF'
import sqlite3, sys
p = sys.argv[1]; c = sqlite3.connect(p)
c.execute("CREATE TABLE fueller (x)"); c.executemany("INSERT INTO fueller VALUES (?)", [(b'\x00' * 3000,) for _ in range(20)]); c.commit()
ps = c.execute("pragma page_size").fetchone()[0]; root = c.execute("select rootpage from sqlite_master where name='fueller'").fetchone()[0]; c.close()
f = open(p, 'r+b'); f.seek((root - 1) * ps + 8); f.write(b'\xff' * 200); f.close()
EOF
afall "beschädigte Datenbank (kaputte Werte da)" -1 0 $E/c.db CUSA08519
mkai $E/b.db
# 6. Datenbank belegt (anderer Prozess schreibt) -> nach ~3 s aufgeben, nichts ändern
python3 -I -c "import sqlite3,time,sys; c=sqlite3.connect(sys.argv[1],isolation_level=None); c.execute('BEGIN EXCLUSIVE'); open(sys.argv[2],'w').close(); time.sleep(8)" $E/b.db $E/belegt.flag &
BP=$!; for i in $(seq 50); do [ -e $E/belegt.flag ] && break; sleep 0.1; done
afall "Datenbank belegt" -2 0 $E/b.db CUSA08519
wait $BP
# 7. fehlt / keine SQLite-Datei / falscher Aufbau -> nicht prüfbar, nichts anlegen
afall "Datenbank fehlt" -2 0 $E/fehlt.db CUSA08519
[ ! -e $E/fehlt.db ] && echo "OK   fehlende Datenbank nicht angelegt" || { echo "FAIL fehlende Datenbank angelegt"; RC=1; }
head -c 16384 /dev/urandom > $E/kaputt.db
afall "keine SQLite-Datei" -2 0 $E/kaputt.db CUSA08519
python3 -I -c "import sqlite3,sys; c=sqlite3.connect(sys.argv[1]); c.execute('create table andere (x)'); c.commit()" $E/fremd.db
afall "falscher Aufbau (keine Tabelle tbl_appinfo)" -2 0 $E/fremd.db CUSA08519
echo $RC > $T/_rc_teil3
}
teil4() {
echo "== Teil 4: kachel_db_austragen - direkt eingetragene Kachel aus app.db und appinfo.db (NEU 1.0.10, TabShifter TBSH00001)"
local RC=0 Q=$T/kz; rm -rf $Q; mkdir -p $Q
kfall() {  # name fall kennung erwartung(0|-1|-2) nur_pruefen [grund-text]
  local name=$1 fall=$2 id=$3 soll=$4 np=$5 gt=$6
  rm -rf $Q/v $Q/n; python3 -I $KK/test/kern/kachel_db_baum.py $Q/v $fall; cp -r $Q/v $Q/n
  local aus=$($B/test_kachel_db $Q/n $id $np)
  echo "INFO $name: $aus"
  case "$aus" in "ERGEBNIS r=$soll "*) echo "OK   $name: Rückgabe $soll";; *) echo "FAIL $name: nicht r=$soll"; RC=1;; esac
  [ -n "$gt" ] && { case "$aus" in *"$gt"*) echo "OK   $name: Grund nennt \"$gt\"";; *) echo "FAIL $name: Grund ohne \"$gt\""; RC=1;; esac; }
  if [ "$soll" != 0 ] || [ "$np" = 1 ]; then
    if [ "$(cd $Q/v; sha256sum app.db appinfo.db)" = "$(cd $Q/n; sha256sum app.db appinfo.db)" ]; then echo "OK   $name: beide Dateien unverändert"; else echo "FAIL $name: Datei verändert"; RC=1; fi
  else
    local v=$(python3 -I $KK/test/kern/kachel_db_vergleich.py $Q/v $Q/n $id)
    echo "$v" | sed 's/^/INFO   /'
    [ "$(echo "$v" | grep -c '^GLEICH')" = 2 ] && echo "OK   $name: $id überall weg, jede andere Zeile gleich (samt rowid), Aufbau gleich, beide heil" || { echo "FAIL $name: Vergleich"; RC=1; }
  fi
  ls $Q/n | grep -q -- '-journal' && { echo "FAIL $name: Journal-Datei übrig"; RC=1; }
}
kfall "direkt eingetragene Kachel: nur prüfen" normal KOPI00001 0 1 "austragbar"
kfall "direkt eingetragene Kachel: austragen" normal KOPI00001 0 0 "ausgetragen"
kfall "Spielgruppe mit anderem Titel geteilt" teilt KOPI00001 -1 0 "Spielgruppe"
kfall "Kennung an unbekannter Stelle (Playlist)" fremd KOPI00001 -1 0 "unbekannten Stelle"
kfall "Auslöser beim Löschen in der Datenbank" ausloeser KOPI00001 -1 0 "Auslöser"
kfall "Verweis aus appinfo.db wäre danach kaputt (Codex-Prüfung 171)" verweis KOPI00001 -1 0 "Verweise zwischen Tabellen"
kfall "Systemkennung" normal NPXS40037 -1 0 "Systemkennung"
kfall "nicht deinstallierbar" normal FEST00001 -1 0 "nicht deinstallierbar"
kfall "Original des Kopierten (steht in Sonys Katalog-Liste)" normal ORIG00001 -1 0 "unbekannten Stelle"
kfall "Kennung nicht in Sonys Liste" normal NICH00001 -1 0 "nicht genau einmal"
kfall "mit Sony-Paket installiert (contentId)" normal PPSA99177 -1 0 "Sony-Paket"
# Sony schreibt gerade (hält appinfo.db): nichts geändert, auch app.db nicht
rm -rf $Q/v $Q/n; python3 -I $KK/test/kern/kachel_db_baum.py $Q/v normal; cp -r $Q/v $Q/n
python3 -I -c "import sqlite3,time,sys; c=sqlite3.connect(sys.argv[1],isolation_level=None); c.execute('BEGIN EXCLUSIVE'); open(sys.argv[2],'w').close(); time.sleep(6)" $Q/n/appinfo.db $Q/belegt.flag &
BP=$!; for i in $(seq 1 50); do [ -f $Q/belegt.flag ] && break; sleep 0.1; done
aus=$($B/test_kachel_db $Q/n KOPI00001 0); wait $BP; echo "INFO belegt: $aus"
case "$aus" in "ERGEBNIS r=-1 "*|"ERGEBNIS r=-2 "*) echo "OK   appinfo.db belegt: nichts geändert (Rückgabe negativ)";; *) echo "FAIL appinfo.db belegt: $aus"; RC=1;; esac
[ "$(cd $Q/v; sha256sum app.db appinfo.db)" = "$(cd $Q/n; sha256sum app.db appinfo.db)" ] && echo "OK   appinfo.db belegt: beide Dateien unverändert" || { echo "FAIL appinfo.db belegt: verändert"; RC=1; }
echo $RC > $T/_rc_teil4
}
for t in teil1 teil2 teil3 teil4; do rm -f $T/_rc_$t; $t > $T/_aus_$t.txt 2>&1 & done
wait
for t in teil1 teil2 teil3 teil4; do cat $T/_aus_$t.txt; [ "$(cat $T/_rc_$t 2>/dev/null)" = 0 ] || RC=1; done
date "+%Y-%m-%d %H:%M:%S FERTIG"
grep -q "^FAIL" "$K/test/kern/ergebnis/sauber.log" && RC=1
echo "FEHLERZAHL $(grep -c '^FAIL' "$K/test/kern/ergebnis/sauber.log")"
exit $RC
