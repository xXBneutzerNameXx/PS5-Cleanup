#!/usr/bin/env python3
# NEU 1.0.10: vorher/nachher von app.db und appinfo.db vergleichen - jede Zeile ohne die Kennung gleich (samt rowid), Aufbau gleich, heil.
# Aufruf: kachel_db_vergleich.py <ordner vorher> <ordner nachher> <kennung>   Ausgabe je Datei eine Zeile "GLEICH ..." oder "ANDERS ..."
import sqlite3,sys
def dump(p, titel):
    c=sqlite3.connect(f"file:{p}?mode=ro",uri=True)
    out={}; ref={}
    for (t,) in c.execute("select name from sqlite_master where type='table' and name not like 'sqlite_%'"):
        cols=[r[1] for r in c.execute(f"pragma table_info('{t}')")]
        rows=list(c.execute(f'select rowid,* from "{t}"')) if c.execute(f"select count(*) from sqlite_master where name=? and sql like '%WITHOUT ROWID%'",(t,)).fetchone()[0]==0 else list(c.execute(f'select * from "{t}"'))
        out[t]=[r for r in rows if not any(isinstance(v,str) and titel in v for v in r)]
        ref[t]=sum(1 for r in rows if any(isinstance(v,str) and titel in v for v in r))
    schema=sorted(c.execute("select type,name,sql from sqlite_master").fetchall(), key=lambda x:(x[0],x[1]))
    ic=c.execute("pragma integrity_check").fetchall(); fk=c.execute("pragma foreign_key_check").fetchall()
    return out,ref,schema,ic,fk
titel=sys.argv[3]
for f in ["app.db","appinfo.db"]:
    a,ra,sa,_,_=dump(f"{sys.argv[1]}/{f}",titel); b,rb,sb,ic,fk=dump(f"{sys.argv[2]}/{f}",titel)
    diff=[t for t in a if a[t]!=b.get(t)]
    gut = not diff and sa == sb and ic == [('ok',)] and not fk and not any(rb.values())
    print("GLEICH" if gut else "ANDERS", f, "Zeilen ohne", titel, "gleich:", not diff, diff, "| Schema gleich:", sa == sb, "| integrity:", ic, "| fk:", len(fk))
    print("   vorher mit",titel,":",{k:v for k,v in ra.items() if v}, " nachher:",{k:v for k,v in rb.items() if v})
    print("   Zeilen gesamt vorher/nachher:", sum(len(x) for x in a.values())+sum(ra.values()), sum(len(x) for x in b.values())+sum(rb.values()))
