#!/usr/bin/env python3
# 1.0.10 (da_Chris86 12.11.2026: Rechenleistung des Mini-PCs nutzen, keine Mehrfachkompilierungen) - nur Test-/Bauumgebung.
# Steht im Gesamttest (lauf_alle_schnell.sh) als "gcc" vorne im PATH. Ein Aufruf mit mehreren .c-Dateien (Kern + Test, linken) wird zerlegt:
#   1. jede .c-Datei einzeln übersetzt, alle gleichzeitig - verteilt auf gemeinsame Rechenplätze (so viele wie Threads) für ALLE
#      Testgruppen zusammen (dynamisch: wer frei ist, nimmt die nächste Datei)
#   2. das Ergebnis je Datei landet im gemeinsamen Zwischenspeicher. Schlüssel = Compiler, Schalter und der vollständig vorverarbeitete
#      Quelltext (gcc -E, wie ccache) - jede Änderung an Quelle oder eingebundenen Dateien ergibt einen neuen Schlüssel. Übersetzen dieselbe
#      Datei mit denselben Schaltern mehrere Gruppen, übersetzt nur die erste, die anderen warten und nehmen ihr Ergebnis
#   3. danach wird wie bisher gelinkt (gleiche Reihenfolge der Argumente, .c durch .o ersetzt)
# Alles andere (gcc -c, -E, eine einzelne Datei, ...) geht unverändert an den echten gcc. Ohne PS5C_OBJCACHE: immer der echte gcc.
# Ein Übersetzungsfehler wird wie bei gcc ausgegeben und endet mit Rückgabewert 1 (es wird nicht gelinkt).
import fcntl, hashlib, os, subprocess, sys, time
from concurrent.futures import ThreadPoolExecutor

GCC = os.environ.get("PS5C_GCC", "/usr/bin/gcc")
CACHE = os.environ.get("PS5C_OBJCACHE", "")
args = sys.argv[1:]

def echt():
    os.execv(GCC, [GCC] + args)

if not CACHE or any(a in ("-E", "-S", "-M", "-MM", "-") or a.startswith("-x") or a.startswith("-fdump") for a in args):
    echt()
NUR_C = "-c" in args   # 1.0.10: auch "gcc -c datei.c -o datei.o" (z. B. SQLite, rund 30 s, in mehreren Gruppen) nur einmal übersetzen

MIT_WERT = {"-o", "-include", "-I", "-L", "-MF", "-MT", "-MQ", "-x", "-isystem", "-iquote", "-imacros", "-D", "-U"}
quellen = []
for i, a in enumerate(args):
    if a.endswith(".c") and (i == 0 or args[i - 1] not in MIT_WERT) and os.path.isfile(a):
        quellen.append(i)
if NUR_C:
    if len(quellen) != 1 or "-o" not in args: echt()
elif len(quellen) < 2:
    echt()

# Schalter fürs Übersetzen: alles außer Quellen, -o, Bibliotheken und Objektdateien; -I und -include auf echte Pfade (die Testgruppen
# sehen das Projekt über eigene Verknüpfungen - so treffen sich gleiche Dateien im Zwischenspeicher)
def echt_pfad(p):
    return os.path.realpath(p)
cf = []
i = 0
while i < len(args):
    a = args[i]
    if i in quellen:
        i += 1; continue
    if a == "-o" or a == "-c":
        i += 2 if a == "-o" else 1; continue
    if a.startswith("-L") or a.startswith("-l") or a.endswith((".o", ".a", ".so")):
        if a in ("-L", "-l"):
            i += 2; continue
        i += 1; continue
    if a in ("-I", "-include", "-isystem", "-iquote", "-imacros") and i + 1 < len(args):
        cf += [a, echt_pfad(args[i + 1])]; i += 2; continue
    if a.startswith("-I") and len(a) > 2:
        cf.append("-I" + echt_pfad(a[2:])); i += 1; continue
    cf.append(a); i += 1
if "-pthread" in args and "-pthread" not in cf:
    cf.append("-pthread")

os.makedirs(CACHE, exist_ok=True)
PLAETZE = int(os.environ.get("PS5C_PLAETZE", "") or os.cpu_count() or 4)

class Platz:
    """einer von PLAETZE Rechenplätzen, gemeinsam für alle gcc-Aufrufe aller Testgruppen"""
    def __enter__(self):
        while True:
            for n in range(PLAETZE):
                f = open(os.path.join(CACHE, "platz_%02d" % n), "a")
                try:
                    fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    self.f = f
                    return self
                except OSError:
                    f.close()
            time.sleep(0.02)
    def __exit__(self, *x):
        fcntl.flock(self.f, fcntl.LOCK_UN); self.f.close()

try:
    ver = subprocess.run([GCC, "-dumpfullversion"], capture_output=True, text=True).stdout.strip()
except Exception:
    ver = "?"

def uebersetzen(i):
    q = echt_pfad(args[i])
    with Platz():
        pp = subprocess.run([GCC] + cf + ["-E", q], capture_output=True)
    if pp.returncode != 0:
        return i, None, pp.stderr
    h = hashlib.sha256()
    for t in (ver, "\0".join(cf), q):
        h.update(t.encode()); h.update(b"\0")
    h.update(pp.stdout)
    ziel = os.path.join(CACHE, h.hexdigest() + ".o")
    if os.path.exists(ziel):
        return i, ziel, b""
    with open(ziel + ".lock", "a") as l:
        fcntl.flock(l, fcntl.LOCK_EX)          # dieselbe Datei übersetzt nur einer, die anderen warten hier
        if os.path.exists(ziel):
            return i, ziel, b""
        tmp = "%s.%d.%d.tmp" % (ziel, os.getpid(), i)
        with Platz():
            r = subprocess.run([GCC] + cf + ["-c", q, "-o", tmp], capture_output=True)
        if r.returncode != 0:
            try: os.unlink(tmp)
            except OSError: pass
            return i, None, r.stderr
        os.replace(tmp, ziel)
        return i, ziel, r.stderr

with ThreadPoolExecutor(max_workers=len(quellen)) as ex:
    erg = list(ex.map(uebersetzen, quellen))
fehler = False
for i, ziel, err in erg:                         # Meldungen in der Reihenfolge der Quellen, wie gcc
    if err:
        sys.stderr.buffer.write(err)
    if ziel is None:
        fehler = True
sys.stderr.flush()
if fehler:
    sys.exit(1)
obj = {i: z for i, z, _ in erg}
if NUR_C:                                        # nur übersetzen: das Ergebnis an die gewünschte Stelle kopieren
    import shutil
    ziel = args[args.index("-o") + 1]
    tmp = "%s.%d.tmp" % (ziel, os.getpid()); shutil.copyfile(obj[quellen[0]], tmp); os.replace(tmp, ziel)
    sys.exit(0)
neu = [obj.get(i, a) for i, a in enumerate(args)]
os.execv(GCC, [GCC] + neu)
