#!/usr/bin/env python3
# NEU 1.0.10 (da_Chris86 12.11.2026 21:40): Sonys app.db und appinfo.db im Aufbau der PS5 Pro FW 12.00 (nur die Tabellen, die eine Kachel
# betreffen, Spalten wie dort) mit einer Kachel, die ein Programm direkt eingetragen hat - so wie TabShifter es macht (Zeilen einer anderen
# App kopiert, Kennung geändert). Aufruf: kachel_db_baum.py <ordner> [fall]   fall: normal | teilt | fremd | ausloeser | system | verweis
import sqlite3, sys, os
o = sys.argv[1]; fall = sys.argv[2] if len(sys.argv) > 2 else "normal"
os.makedirs(o, exist_ok=True)
for f in ("app.db", "appinfo.db"):
    if os.path.exists(os.path.join(o, f)): os.unlink(os.path.join(o, f))
U = "0284599307"
a = sqlite3.connect(os.path.join(o, "app.db"))
a.executescript(f"""
CREATE TABLE tbl_version(category TEXT NOT NULL,status INT,PRIMARY KEY(category));
CREATE TABLE tbl_contentinfo(titleId TEXT NOT NULL PRIMARY KEY,contentId TEXT,titleName TEXT,metaDataPath TEXT,installTime TEXT,mTime TEXT,promoteTime TEXT,
  size INT,uninstallable INT,contentLocation INT,hddLocation INT,externalHddAppStatus INT,localConceptId TEXT,pprDeeplinkUri TEXT,AppInfoJson TEXT);
CREATE TABLE tbl_conceptmetadata(localConceptId TEXT NOT NULL PRIMARY KEY,meta TEXT);
CREATE TABLE tbl_iconinfo_{U}(titleId TEXT NOT NULL PRIMARY KEY,titleName TEXT,localConceptId TEXT,installedDate TEXT,appDrmType INT,dispLocation INT);
CREATE TABLE tbl_concepticoninfo_{U}(localConceptId TEXT NOT NULL PRIMARY KEY,primaryTitleId TEXT,conceptName TEXT);
CREATE TABLE tbl_group_{U}(groupId INT,itemId TEXT);
CREATE TABLE tbl_info_{U}(key TEXT NOT NULL PRIMARY KEY,value TEXT);
CREATE TABLE tbl_playlist_{U}(id INT,item TEXT);
CREATE TRIGGER trig_insert_drm_tbl_iconinfo_{U} after insert on tbl_iconinfo_{U} when new.appDrmType = 1 begin update tbl_iconinfo_{U} set appDrmType = 5 where titleId = new.titleId; end;
INSERT INTO tbl_version VALUES ('appinfo_sync_index', 17905);
""")
i = sqlite3.connect(os.path.join(o, "appinfo.db"))
i.executescript("""
CREATE TABLE tbl_version(category TEXT NOT NULL,status INT,PRIMARY KEY(category));
CREATE TABLE tbl_appinfo(titleId TEXT NOT NULL,metaDataId TEXT,key INT NOT NULL,val,PRIMARY KEY(titleId, metaDataId, key));
CREATE TABLE tbl_conceptinfo(local_concept_id TEXT NOT NULL,metadata_id TEXT,key INT NOT NULL,val,PRIMARY KEY(local_concept_id, key));
INSERT INTO tbl_version VALUES ('titles_have_no_catalog_content_version', 'NPXS40037,ORIG00001,');
""")
def titel(t, n, inst, u=1, lc=None, cid=None):
    lc = lc or f"cid:local:{t}"
    a.execute("INSERT INTO tbl_contentinfo VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)", (t, cid, n, f"/user/app/{t}/sce_sys", inst, inst, inst, 458752, u, 2, 0, 0, lc,
              "http://127.0.0.1:9090/", '{"TITLE_ID":"%s","_org_path":"/user/app/%s"}' % (t, t)))
    a.execute(f"INSERT INTO tbl_iconinfo_{U} VALUES (?,?,?,?,?,?)", (t, n, lc, inst, 0, 188))
    a.execute(f"INSERT OR IGNORE INTO tbl_concepticoninfo_{U} VALUES (?,?,?)", (lc, t, n))
    for k, v in (("TITLE", n), ("TITLE_ID", t), ("_org_path", f"/user/app/{t}"), ("DEEPLINK_URI", "http://127.0.0.1:9090/")):
        i.execute("INSERT INTO tbl_appinfo VALUES (?,?,?,?)", (t, "prior:internal:0", k, v))
titel("ORIG00001", "Ladeprogramm", "2026-11-09 20:56:39.764")                 # das Original (wie der Payload Manager)
titel("KOPI00001", "Kopie", "2026-11-09 20:56:39.764")                        # direkt eingetragen: Zeitpunkte der Kopie gleich
titel("KOPI00002", "Kopie mit Ordner", "2026-11-09 20:56:39.764")             # dasselbe, ihre Dateien sind noch da (aktive Kachel)
titel("GRUP00001", "Spiel", "2026-10-01 10:00:00.000", lc="cid:local:GEMEINSAM")
titel("GRUP00002", "Spiel PS4", "2026-10-01 11:00:00.000", lc="cid:local:GEMEINSAM")
titel("NPXS40037", "System", "2026-01-01 00:00:00.000", u=0)
titel("FEST00001", "nicht deinstallierbar", "2026-02-01 00:00:00.000", u=0)
titel("PPSA99177", "PKG-Homebrew", "2026-03-01 00:00:00.000", cid="UP9000-PPSA99177_00-ORBITSTORETV0001")   # mit Sony-Paket installiert
a.execute(f"INSERT INTO tbl_group_{U} VALUES (1, 'KOPI00001')"); a.execute(f"INSERT INTO tbl_group_{U} VALUES (1, 'ORIG00001')")
a.execute(f"INSERT INTO tbl_info_{U} VALUES ('nsx_content_flag_KOPI00001', '{{\"deleted\":0}}')"); a.execute(f"INSERT INTO tbl_info_{U} VALUES ('nsx_content_flag_ORIG00001', '{{}}')")
a.execute("INSERT INTO tbl_conceptmetadata VALUES ('cid:local:ORIG00001', 'x')")
i.execute("INSERT INTO tbl_conceptinfo VALUES ('cid:local:KOPI00001', 'm', 'NAME', 'Kopie')"); i.execute("INSERT INTO tbl_conceptinfo VALUES ('cid:local:ORIG00001', 'm', 'NAME', 'Ladeprogramm')")
if fall == "fremd":      # die Kennung steht noch an einer Stelle, die keine bekannte Stelle einer Kachel ist
    a.execute(f"INSERT INTO tbl_playlist_{U} VALUES (1, 'KOPI00001')")
if fall == "ausloeser":  # ein Auslöser beim Löschen hätte unbekannte Folgen
    a.execute(f"CREATE TRIGGER t_weg AFTER DELETE ON tbl_contentinfo BEGIN DELETE FROM tbl_version; END")
if fall == "teilt":      # die Kopie teilt die Spielgruppe mit dem Original
    a.execute("UPDATE tbl_contentinfo SET localConceptId='cid:local:ORIG00001' WHERE titleId='KOPI00001'")
if fall == "verweis":    # Codex-Prüfung 171 (2): eine Tabelle in appinfo.db verweist über die Spielgruppe (ohne die Kennung im Text) auf die
    # Zeile, die beim Austragen wegfiele - danach wäre der Verweis kaputt, das muss erkannt und zurückgenommen werden
    for t in ("tbl_contentinfo", f"tbl_iconinfo_{U}", f"tbl_concepticoninfo_{U}"):
        a.execute(f"UPDATE {t} SET localConceptId='cid:local:K1' WHERE localConceptId='cid:local:KOPI00001'")
    i.execute("UPDATE tbl_conceptinfo SET local_concept_id='cid:local:K1' WHERE local_concept_id='cid:local:KOPI00001'")
    i.execute("CREATE TABLE tbl_verweis(lc TEXT, k INT, FOREIGN KEY(lc, k) REFERENCES tbl_conceptinfo(local_concept_id, key))")
    i.execute("INSERT INTO tbl_verweis VALUES ('cid:local:K1', 'NAME')")
a.commit(); a.close(); i.commit(); i.close()
