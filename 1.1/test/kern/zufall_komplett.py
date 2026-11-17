#!/usr/bin/env python3
"""PS5 Cleanup - Zufallstest für das Fenster der vollständigen Bereinigung (PS5-Test 03.11.2026).
Erzeugt erfundene Konsolen wie zufall.py und ergänzt, was dort fehlte: erweiterten Speicher (USB-Festplatte für Spiele,
/mnt/ext0) mit Spielen und Ordnern sowie ShadowMount-Spiele, deren Spieldatei intern, auf der M.2, auf dem erweiterten
Speicher oder auf dem USB-Stick liegt. Je Konsole läuft test_komplett (Prüfungen dort).
Aufruf: zufall_komplett.py <test_komplett> <arbeitsordner> <anzahl> [start]

NEU 12.11.2026 (da_Chris86: größten Engpass im Gesamttest beseitigen): die Konsolen laufen auf mehreren CPU-Kernen gleichzeitig.
Jede Konsole hängt nur an ihrem Startwert (eigener Zufallsgenerator random.Random(startwert)) und bekommt ihren eigenen
Arbeitsordner (<arbeitsordner>/tNN/w statt <arbeitsordner>/w, je Kern einer) - sie sind voneinander unabhängig. Ausgabe, Zählung und
Rückgabewert sind unverändert: die Ergebnisse werden in der Reihenfolge der Startwerte ausgewertet und ausgegeben, genau wie
vorher. Bricht ein Arbeitsprozess ab, bricht der ganze Test mit Fehler ab (nichts wird still übergangen).
Anzahl der Kerne: KOMPLETT_JOBS (Standard: alle Kerne). KOMPLETT_JOBS=1 = alter Ablauf nacheinander im Ordner <arbeitsordner>/w."""
import json, os, random, shutil, subprocess, sys
import multiprocessing
from concurrent.futures import ProcessPoolExecutor

HIER = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(HIER, "zufall.py"), encoding="utf-8").read().replace("\nsys.exit(main())", "\n")
Z = {"__name__": "zufall", "__file__": os.path.join(HIER, "zufall.py")}
exec(compile(src, "zufall.py", "exec"), Z)

def datei(W, p, inhalt=b"x"):
    q = os.path.join(W, p.lstrip("/")); os.makedirs(os.path.dirname(q), exist_ok=True)
    open(q, "wb").write(inhalt if isinstance(inhalt, bytes) else inhalt.encode())

def param(W, basis, kid, titel):
    datei(W, "%s/%s/sce_sys/param.json" % (basis, kid), json.dumps({"titleId": kid, "contentVersion": "01.00",
          "localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": titel}}}))

def ergaenze(W, R, nr):
    m2 = os.path.isdir(os.path.join(W, "mnt/ext1/user/app")); stick = os.path.isdir(os.path.join(W, "mnt/usb0"))
    erw = R.random() < 0.6
    if erw:
        os.makedirs(os.path.join(W, "mnt/ext0/user/app"), exist_ok=True)
        for i in range(R.randint(0, 4)):
            kid = "PPSA8%04d" % (nr * 7 + i) if R.random() < 0.7 else "CUSA8%04d" % (nr * 7 + i)
            param(W, "/mnt/ext0/user/app", kid, "Erweitert %d" % i); datei(W, "/mnt/ext0/user/app/%s/eboot.bin" % kid)
        for i in range(R.randint(0, 3)):
            datei(W, "/mnt/ext0/Ordner%d/a.dat" % i)
    # ShadowMount-Spiele (Kachel intern, Spieldatei irgendwo)
    orte = ["/data/homebrew"] + (["/mnt/ext1/homebrew"] if m2 else []) + (["/mnt/ext0/homebrew"] if erw else []) + (["/mnt/usb0/homebrew"] if stick else [])
    for i in range(R.randint(0, 4)):
        kid = "PPSA7%04d" % (nr * 5 + i); wo = R.choice(orte); name = "Schatten %d - %s.exfat" % (i, kid)
        param(W, "/user/app", kid, "Schatten %d" % i)
        datei(W, "/user/app/%s/mount_img.lnk" % kid, "/mnt/shadowmnt/%s\n%s/%s" % (kid, wo, name))
        echt = wo.replace("/data/", "/user/data/", 1) if wo.startswith("/data/") else wo
        datei(W, "%s/%s" % (echt, name))

def eine_konsole(prog, arbeit, W, seed):
    """Eine erfundene Konsole erzeugen und prüfen (Ablauf wie bis 1.0.10 in main). Gibt das Ergebnis zurück, statt es auszugeben."""
    R = random.Random(seed)
    shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
    Z["erzeuge"](W, R)
    ergaenze(W, R, seed % 1000)
    r = subprocess.run([prog, W], capture_output=True, text=True, cwd=arbeit)
    info = [z for z in r.stdout.splitlines() if z.startswith("INFO")]
    orte = None
    if info:
        t = info[0]
        orte = (" M.2 0," not in t, "erweiterter Speicher 0," not in t, t.endswith("Stick ja"))
    fl = None
    if r.returncode != 0:
        fl = [z for z in r.stdout.splitlines() if z.startswith(("FAIL", "X ", "FEHLERMELDUNG"))] or [(r.stdout + r.stderr).strip()[:300]]
    return seed, orte, r.returncode, fl

# Arbeitsprozesse: jeder bekommt beim Start eine feste Nummer (0, 1, 2 ...) und damit seinen eigenen Ordner <arbeitsordner>/tNN
# (darin der Konsolen-Baum w; test_komplett läuft auch mit diesem Ordner als Arbeitsverzeichnis - keine gemeinsamen Dateien)
_T = None
def _start(zaehler, arbeit):
    global _T
    with zaehler.get_lock():
        nr = zaehler.value; zaehler.value += 1
    _T = os.path.join(arbeit, "t%02d" % nr)
    os.makedirs(_T, exist_ok=True)

def _konsole(a):
    prog, seed = a
    return eine_konsole(prog, _T, os.path.join(_T, "w"), seed)

def main():
    prog, arbeit, anzahl = sys.argv[1], sys.argv[2], int(sys.argv[3]); start = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    fehler = 0; zahl = {"m2": 0, "erw": 0, "stick": 0}
    jobs = max(1, min(int(os.environ.get("KOMPLETT_JOBS") or os.cpu_count() or 1), anzahl))
    seeds = range(start, start + anzahl)
    pool = None
    if jobs == 1:
        W = os.path.join(arbeit, "w")
        ergebnisse = (eine_konsole(prog, arbeit, W, seed) for seed in seeds)
    else:
        sys.stdout.flush(); sys.stderr.flush()
        ctx = multiprocessing.get_context("fork")
        zaehler = ctx.Value("i", 0)
        pool = ProcessPoolExecutor(max_workers=jobs, mp_context=ctx, initializer=_start, initargs=(zaehler, arbeit))
        # map liefert die Ergebnisse in der Reihenfolge der Startwerte; ein Fehler in einem Arbeitsprozess bricht hier mit Fehler ab
        ergebnisse = pool.map(_konsole, ((prog, seed) for seed in seeds), chunksize=4)
    erwartet = start
    for seed, orte, rc, fl in ergebnisse:
        if seed != erwartet: raise RuntimeError("Reihenfolge der Konsolen gestört: %d statt %d" % (seed, erwartet))
        erwartet += 1
        if orte:
            if orte[0]: zahl["m2"] += 1
            if orte[1]: zahl["erw"] += 1
            if orte[2]: zahl["stick"] += 1
        if rc != 0:
            fehler += 1
            print("FAIL Konsole %d: %d Abweichung(en)" % (seed, len(fl)))
            for x in fl[:8]: print("       - " + x)
    if pool: pool.shutdown()
    # Gegenprobe: wirklich jede Konsole ausgewertet
    if erwartet != start + anzahl: raise RuntimeError("nur %d von %d Konsolen ausgewertet" % (erwartet - start, anzahl))
    print("%s %d erfundene Konsolen: Fenster der vollständigen Bereinigung geprüft, %d mit Abweichungen" % ("OK  " if fehler == 0 else "FAIL", anzahl, fehler))
    print("INFO davon mit Einträgen auf M.2: %d, auf erweitertem Speicher: %d, mit USB-Stick: %d" % (zahl["m2"], zahl["erw"], zahl["stick"]))
    print("FEHLERZAHL %d" % fehler)
    return 1 if fehler else 0

sys.exit(main())
