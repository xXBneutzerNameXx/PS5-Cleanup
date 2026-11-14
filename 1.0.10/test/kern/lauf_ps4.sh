#!/bin/bash
# PS5 Cleanup - Test PS4-Spieleschutz + Datenbank-Prüfung (seit 1.0.8 nur lesen) + Ende ohne Neuaufbau-Empfehlung (da_Chris86 31.10.2026 19:56)
# Nur nachgebildete Dateisysteme und Datenbanken - nie eine PS5. Läuft in WSL (K überschreibbar für andere Orte).
K="${K:-/pfad/zu/PS5-Cleanup/1.0.10}"
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
# 1.0.10 (da_Chris86 12.11.2026: Rechenleistung des Mini-PCs nutzen): jede Datei gleichzeitig, ein Fehler bei irgendeiner zählt wie bisher
export KK B
SP=0; printf '%s\n' $KERN $KK/db/db_pruefen.c $KK/db/db_lesen.c | xargs -P "$(nproc)" -I{} bash -c 'gcc -c -O1 -w -include $KK/bau/nurlesen.h -DNUR_LESEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson "$1" -o $B/nl_$(basename "$1" .c).o' _ {} || SP=1
[ $SP = 0 ] && echo "OK   Schreibsperre: Kern und Datenbank-Prüfung lassen sich mit bau/nurlesen.h übersetzen" || echo "FAIL Schreibsperre: Übersetzen mit bau/nurlesen.h scheitert"
NLT=$(nm -u $B/nl_*.o | grep -w -E "fwrite|fputs|fputc|fprintf|write|pwrite|unlink|unlinkat|rmdir|mkdir|mkdirat|rename|renameat|truncate|ftruncate|chmod|fchmod|fchmodat|link|linkat|symlink|nmount|mount|unmount|openat" | sort -u)
[ -z "$NLT" ] && echo "OK   Schreibsperre: keine schreibende Funktion in den Objektdateien ohne KERN_LOESCHEN" || echo "FAIL Schreibsperre: $NLT"
FO=0; printf '%s\n' $KERN $KK/db/db_pruefen.c $KK/db/db_lesen.c | xargs -P "$(nproc)" -I{} bash -c 'gcc -E -include $KK/bau/nurlesen.h -DNUR_LESEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson "$1" 2>/dev/null | python3 -I $KK/bau/fopen_pruefen.py "$(basename $1)" > /dev/null' _ {} || FO=1
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
for i, (eid, off, gr) in enumerate(liste): struct.pack_into(">IIIIII", h, tab + 32 * i, eid, 0, 0x80000000 if eid == 0x1200 and art == "bild_verschl" else 0, 0, off, gr)
if art != "bild_kaputt": h[0x3800:0x3808] = b"\x89PNG\r\n\x1a\n"   # NEU 1.0.9: icon0.png (Eintrag 0x1200) beginnt wie eine PNG-Datei
if art != "sfo_weg": h[so:so + len(sfo)] = sfo
if art == "viele": struct.pack_into(">I", h, 0x10, 9000)
open(ziel, "wb").write(bytes(h)[:so + 16] if art == "abgeschnitten" else bytes(h) + b"x" * 200000)
PY
}
exfatbild() {   # exfatbild <datei> <art> <titel_de> <titel_en> <nummer> [<Kopie des Bildes>] - NEU 1.0.8 (Arbeitsplan 2.1): kleines exFAT-Abbild nach Spezifikation
               # NEU 1.0.9 (#149): bild_stuecke (Bild über 4 Cluster in 3 Stücken, jedes Byte anders) - die Kopie liegt außerhalb des Baums;
               #   Codex-Prüfung 134: bild_stuecke_hinten (über die Grenze zweier PFSC-Blöcke) | bild_kreis | bild_kurz | bild_ungueltig
               #   | bild_64, bild_65 (Stücke) | bild_1024, bild_1025 (Cluster zu 512 Byte)
               # art: normal (param.json über 2 Cluster per FAT-Kette) | am_stueck (ohne FAT-Kette) | unterordner | sfo (PS4) | kein_exfat
               #   | kaputt_kopf | schleife (FAT-Kette der Wurzel im Kreis) | riesig (param.json mit 4 GB Länge) | ohne_sys | kurz (abgeschnitten)
  mkdir -p "$(dirname "$1")"; python3 - "$@" <<'PY'
import struct, sys, json
ziel, art, de, en, nummer = sys.argv[1:6]
kopie = sys.argv[6] if len(sys.argv) > 6 else ""
CL = 512 if art in ("bild_1024", "bild_1025") else 4096                     # Codex-Prüfung 134: kleine Cluster für die Grenze von 1024
ANZ = 1400 if CL == 512 else 160 if art in ("bild_64", "bild_65") else 64
SEK, FAT_OFF, HEAP_OFF, ENDE = 512, 24, 64, 0xFFFFFFFF
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
        # NEU 07.11.2026 (PS5-Test 16:31): Bild über zwei Cluster - hintereinander (bild_kette) oder verstreut (bild_verstreut); NEU #149 und
        # Codex-Prüfung 134: die Ketten der übrigen Arten (Stücke, Kreis FAT[11] = 11, zu kurz, ungültig FAT[11] = 9999, Grenzen)
        ketten = {"bild_kette": [11, 12], "bild_verstreut": [11, 13], "bild_stuecke": [14, 15, 18, 20], "bild_stuecke_hinten": [40, 41, 42, 45],
                  "bild_kreis": [11], "bild_kurz": [11, 13], "bild_ungueltig": [11],
                  "bild_64": [20 + 2 * i for i in range(64)], "bild_65": [20 + 2 * i for i in range(65)],
                  "bild_1024": list(range(100, 612)) + list(range(700, 1212)), "bild_1025": list(range(100, 612)) + list(range(700, 1212)) + [1300]}
        kl = ketten.get(art)
        bl = (5000 if art in ("bild_kette", "bild_verstreut", "bild_kreis", "bild_ungueltig") else 3 * CL if art == "bild_kurz" else len(kl) * CL - 100) if kl else 50
        verz(sys_c, [eintrag("icon0.png", False, kl[0] if kl else 11, bl), eintrag("param.json", False, 5, laenge)])
        ablegen([5, 7] if CL == 4096 else list(range(5, 5 + (len(pj) + CL - 1) // CL)), pj)
        if kl:
            bild = b"\x89PNG\r\n\x1a\n" + bytes((i * 7 + i // CL * 13) % 256 for i in range(bl - 8))
            ablegen(kl, bild)
            if art == "bild_kreis": fat[11] = 11
            if art == "bild_ungueltig": fat[11] = 9999
            if kopie: open(kopie, "wb").write(bild)
if art == "schleife": fat[2] = 2
b = bytearray(512); b[0:3] = b"\xEB\x76\x90"; b[3:11] = b"EXFAT   "
struct.pack_into("<QQIIIII", b, 64, 0, len(img) // SEK, FAT_OFF, 1, HEAP_OFF, ANZ, 2)
b[108] = 20 if art == "kaputt_kopf" else 9; b[109] = (CL // SEK).bit_length() - 1; b[110] = 1; b[510] = 0x55; b[511] = 0xAA
img[0:512] = b; struct.pack_into("<%dI" % len(fat), img, FAT_OFF * SEK, *fat)
if art == "kein_exfat": img = bytearray(65536)
if art == "kurz": img = img[:40960]
open(ziel, "wb").write(bytes(img))
PY
}
pfsbild() {   # pfsbild <datei> <art> <titel_de> <titel_en> <nummer> [<inneres Abbild>] - NEU 1.0.9 (#144): kleines PFS-Abbild, Aufbau wie MkPFS
              # (quer3q/ps5-dump-forge): Blöcke zu 64 KB, Kopf in Block 0, Inodes (0xA8 Byte) in Block 1, jede Datei am Stück.
              # art: normal | sfo (PS4) | bild_kein_png | gepackt (uroot/<nummer>.exfat = inneres Abbild als PFSC: zlib-Blöcke, Block 1 ungepackt)
              #   | innen_roh (dieselbe Datei ungepackt) | verschluesselt (Art 0x4) | verstreut (param.json nicht am Stück) | kaputt_kennung | kurz
              #   Codex-Prüfung 130: | lang, zwei, zwei_lang, fremde_art (in uroot: Name über 64 Zeichen, zwei Dateien, zwei mit einem langen
              #   Namen, Datei und ein Eintrag der Art 7) | link_json (param.json mit Art 0xA000) | ordner_c000 (sce_sys mit Art 0xC000)
              #   Codex-Prüfung 131: | zwei_null (zweite Datei mit einer 0 im Namen) | kaputt_eintrag (nach der Datei ein kaputter Eintrag)
              #   Codex-Prüfung 132: | rest_kopf (uroot endet mit 8 Byte eines Eintrags) | sys_kaputt (sce_sys: param.sfo, kaputter Eintrag,
              #   param.json) | wurzel_kaputt (Über-Wurzel: hinter uroot ein kaputter Eintrag)
  mkdir -p "$(dirname "$1")"; python3 - "$@" <<'PY'
import struct, sys, json, zlib
ziel, art, de, en, nummer = sys.argv[1:6]
innen = open(sys.argv[6], "rb").read() if len(sys.argv) > 6 else b""
BS, IN = 65536, 0xA8
def dirent(ino, typ, name):
    n = name.encode(); el = (16 + len(n) + 7) // 8 * 8
    return struct.pack("<IiiI", ino, typ, len(n), el) + n + b"\0" * (el - 16 - len(n))
def ordner(ich, eltern, kinder):
    return dirent(ich, 4, ".") + dirent(eltern, 5, "..") + b"".join(dirent(i, t, n) for i, t, n in kinder) + b"\0" * 16
def sfo(paare):
    keys = b""; offs = []
    for k, _ in paare: offs.append(len(keys)); keys += k.encode() + b"\0"
    while len(keys) % 4: keys += b"\0"
    daten = b""; doffs = []; laengen = []
    for _, v in paare: d = v.encode() + b"\0"; doffs.append(len(daten)); laengen.append(len(d)); d += b"\0" * ((-len(d)) % 4); daten += d
    kt = 20 + 16 * len(paare); dt = kt + len(keys)
    return b"\0PSF" + struct.pack("<IIII", 0x0101, kt, dt, len(paare)) + b"".join(struct.pack("<HHIII", offs[i], 0x0204, laengen[i], laengen[i] + ((-laengen[i]) % 4), doffs[i]) for i in range(len(paare))) + keys + daten
inodes = []   # (ist_ordner, daten, flags, gr_echt)
wurzel = ordner(0, 0, [(1, 2, "flat_path_table"), (2, 3, "uroot")])
if art == "wurzel_kaputt": wurzel = wurzel[:-16] + struct.pack("<IiiI", 9, 2, 1, 16) + b"\0" * 16
inodes.append((True, wurzel, 0x10, 0))
inodes.append((False, b"\0" * 64, 0x10, 64))
if art in ("gepackt", "innen_roh", "lang", "zwei", "zwei_lang", "fremde_art", "zwei_null", "kaputt_eintrag", "rest_kopf"):
    kinder = [(3, 2, ("Inneres Abbild mit einem sehr langen Namen, laenger als 64 Zeichen - " + nummer if art == "lang" else nummer) + ".exfat")]
    if art == "zwei": kinder.append((4, 2, nummer + "-kopie.exfat"))
    if art == "zwei_lang": kinder.append((4, 2, "Zweite Datei mit einem sehr langen Namen, ebenfalls laenger als 64 Zeichen.exfat"))
    if art == "fremde_art": kinder.append((4, 7, "fremd"))
    if art == "zwei_null": kinder.append((4, 2, "kopie\0.exfat"))
    d_ur = ordner(2, 2, kinder)
    if art == "kaputt_eintrag": d_ur = d_ur[:-16] + struct.pack("<IiiI", 4, 2, 1, 16) + b"\0" * 16   # Name 1 Byte, Eintrag nur 16 Byte: kaputt
    if art == "rest_kopf": d_ur = dirent(3, 2, nummer + ".exfat") + struct.pack("<Ii", 4, 2)   # ohne Ende, Ordner nur so lang (siehe gr unten)
    inodes.append((True, d_ur, 0x10, 0))
    if art == "gepackt":
        bl = (len(innen) + BS - 1) // BS; tab, daten = 0x400, 0x10000; bloecke = []
        for i in range(bl):
            roh = innen[i * BS:(i + 1) * BS]; z = zlib.compress(roh, 6)
            bloecke.append(roh if (i == 1 and len(roh) == BS) or len(z) >= len(roh) else z)
        offs = []; o = daten
        for b in bloecke: offs.append(o); o += len(b)
        offs.append(o)
        c = bytearray(daten); c[0:0x30] = b"PFSC" + struct.pack("<IIIQQQQ", 0, 6, BS, BS, tab, daten, len(innen))
        struct.pack_into("<%dQ" % len(offs), c, tab, *offs)
        inodes.append((False, bytes(c) + b"".join(bloecke), 0x11, len(innen)))
    else:
        inodes.append((False, innen, 0x10, len(innen)))
    if len(kinder) > 1: inodes.append((False, innen, 0x10, len(innen)))   # Inode 4: die zweite Datei bzw. der fremde Eintrag
else:
    p = sfo([("TITLE", de), ("TITLE_ID", nummer), ("CATEGORY", "gd")]) if art == "sfo" else json.dumps({"titleId": nummer, "localizedParameters": {"defaultLanguage": "en-US", "de-DE": {"titleName": de}, "en-US": {"titleName": en}}}).encode()
    icon = (b"KEIN PNG" if art == "bild_kein_png" else b"\x89PNG\r\n\x1a\n") + b"i" * 4992
    inodes.append((True, ordner(2, 2, [(3, 3, "sce_sys"), (6, 2, "eboot.bin")]), 0x10, 0))
    sysd = ordner(3, 2, [(4, 2, "param.sfo" if art == "sfo" else "param.json"), (5, 2, "icon0.png")])
    if art == "sys_kaputt": sysd = dirent(3, 4, ".") + dirent(2, 5, "..") + dirent(7, 2, "param.sfo") + struct.pack("<IiiI", 8, 2, 1, 16) + dirent(4, 2, "param.json") + dirent(5, 2, "icon0.png") + b"\0" * 16
    inodes.append((True, sysd, 0x10, 0))
    inodes.append((False, p, 0x10, len(p))); inodes.append((False, icon, 0x10, len(icon))); inodes.append((False, b"e" * 100, 0x10, 100))
    if art == "sys_kaputt": s7 = sfo([("TITLE", "Pfs Sfo"), ("TITLE_ID", nummer), ("CATEGORY", "gd")]); inodes.append((False, s7, 0x10, len(s7)))   # Inode 7
blk = 2; tabelle = bytearray(BS); daten = []
for nr, (ist_ordner, d, flags, echt) in enumerate(inodes):
    gr = BS if ist_ordner and not (art == "rest_kopf" and nr == 2) else len(d); nb = (gr + BS - 1) // BS
    ino = bytearray(IN)
    mode = 0x41ED if ist_ordner else 0x81A4
    if art == "link_json" and nr == 4: mode = 0xA1FF                        # param.json als Verknüpfung
    if art == "ordner_c000" and nr == 3: mode = 0xC1ED                      # sce_sys mit fremder Art
    struct.pack_into("<HHI", ino, 0, mode, 0, flags)
    struct.pack_into("<QQ", ino, 8, gr, gr if ist_ordner else echt)
    struct.pack_into("<I", ino, 0x60, nb)
    struct.pack_into("<12i", ino, 0x64, blk, *([0 if nr == 0 else -1] * 11))   # Über-Wurzel wie in echten Abbildern: Rest 0
    if art == "verstreut" and nr == 4: struct.pack_into("<i", ino, 0x68, blk + 3)
    # NEU 1.0.9 (PS5-Test 09.11.2026, Spider-Man 2 .ffpfsc): Indirekt-Felder alle -1 statt 0 - heißt ebenso "keiner"; gemischt: nicht gelesen
    if art == "ind_minus" and nr > 0: struct.pack_into("<5I", ino, 0x94, *([0xFFFFFFFF] * 5))
    if art == "ind_gemischt" and nr > 0: struct.pack_into("<5I", ino, 0x94, 0xFFFFFFFF, 0, 0, 0, 0)
    tabelle[nr * IN:(nr + 1) * IN] = ino
    daten.append((blk, d.ljust(nb * BS, b"\0"))); blk += nb
kopf = bytearray(BS)
struct.pack_into("<QQ", kopf, 0, 2, 20130316 if art == "kaputt_kennung" else 20130315)
struct.pack_into("<H", kopf, 0x1C, 0xC if art == "verschluesselt" else 0x8)
struct.pack_into("<I", kopf, 0x20, BS)
struct.pack_into("<QQQ", kopf, 0x30, len(inodes), blk, 1)
bild = bytearray(blk * BS); bild[0:BS] = kopf; bild[BS:2 * BS] = tabelle
for b, d in daten: bild[b * BS:b * BS + len(d)] = d
if art == "kurz": bild = bild[:3 * BS]
open(ziel, "wb").write(bytes(bild))
PY
}
ufsbild() {   # ufsbild <datei> <art> <titel_de> <titel_en> <nummer> [<Kopie des Bildes>] - NEU 1.0.9 (08.11.2026): kleines UFS2-Abbild nach FreeBSD fs.h/dinode.h/dir.h
              # NEU #149: bild_stuecke (wie klein, das Bild in 4 Stücken: 5 + 7 Blöcke, 7 über den Indirekt-Block, ein halber letzter Block)
              #   Codex-Prüfung 134 (wie klein): bild_loch | bild_ausserhalb (Block hinter dem Dateisystem) | bild_rand (letzter Block über
              #   den Rand) | bild_doppelt (ein Block zweimal) | bild_ueberlappt (Codex-Prüfung 135: Block 1 bei Fragment 196, Block 0 bei 200)
              # art: normal (Profil wie ShadowMountPlus mkufs2.sh: Blöcke und Fragmente 64 KB) | klein (4-KB-Blöcke, 512-Byte-Fragmente, 2 Zylinder-
              #   gruppen, sce_sys in Gruppe 1, Bild über 20 Blöcke mit Indirekt-Block) | unterordner | sfo (PS4) | bild_verstreut | bild_kein_png
              #   | kaputt_sb (Blockgröße 3000) | kaputt_verz (kaputter Eintrag im ersten 512-Byte-Stück) | inode_weit (Eintrag hinter der letzten Gruppe) | kurz
  mkdir -p "$(dirname "$1")"; python3 - "$@" <<'PY'
import struct, sys, json
ziel, art, de, en, nummer = sys.argv[1:6]
kopie = sys.argv[6] if len(sys.argv) > 6 else ""
KLEIN = ("klein", "bild_stuecke", "bild_loch", "bild_ausserhalb", "bild_rand", "bild_doppelt", "bild_ueberlappt")
if art in KLEIN: BS, FS, IPG, FPG, NCG, IBLKNO = 4096, 512, 32, 512, 2, 152
else: BS, FS, IPG, FPG, NCG, IBLKNO = 65536, 65536, 256, 16, 1, 3
FRAG = BS // FS; INOPB = BS // 256; SIZE = FPG * NCG
img = bytearray(SIZE * FS)
def ino_off(ino):
    cg, rest = divmod(ino, IPG)
    return (FPG * cg + IBLKNO + (rest // INOPB) * FRAG) * FS + (ino % INOPB) * 256
def inode(ino, mode, laenge, bloecke, ind=0):
    b = bytearray(256); struct.pack_into("<HH", b, 0, mode, 1); struct.pack_into("<Q", b, 16, laenge)
    for j, a in enumerate(bloecke[:12]): struct.pack_into("<q", b, 112 + 8 * j, a)
    struct.pack_into("<q", b, 208, ind); o = ino_off(ino); img[o:o + 256] = b
def schreibe(frag, daten): o = frag * FS; img[o:o + len(daten)] = daten
def datei(ino, daten, frags, ind=0):
    for i, a in enumerate(frags):
        if a and a * FS < len(img): schreibe(a, daten[i * BS:(i + 1) * BS])   # Loch (0) und Blöcke außerhalb: nichts schreiben
    if len(frags) > 12: schreibe(ind, b"".join(struct.pack("<q", a) for a in frags[12:]))
    inode(ino, 0o100644, len(daten), frags, ind)
def eintrag(ino, name, typ, rl=None):
    n = name.encode(); rl = rl or ((8 + len(n) + 1 + 3) & ~3)
    e = struct.pack("<IHBB", ino, rl, typ, len(n)) + n
    return e + b"\0" * (rl - len(e))
def stueck(liste):   # ein 512-Byte-Stück, der letzte Eintrag reicht bis zum Ende
    out = b""
    for i, (ino, name, typ) in enumerate(liste): out += eintrag(ino, name, typ, 512 - len(out) if i == len(liste) - 1 else None)
    return out
def ordner(ino, frag, daten): schreibe(frag, daten); inode(ino, 0o040755, len(daten), [frag])
def sfo(paare):
    keys = b""; offs = []
    for k, _ in paare: offs.append(len(keys)); keys += k.encode() + b"\0"
    while len(keys) % 4: keys += b"\0"
    daten = b""; doffs = []; laengen = []
    for _, v in paare: d = v.encode() + b"\0"; doffs.append(len(daten)); laengen.append(len(d)); d += b"\0" * ((-len(d)) % 4); daten += d
    kt = 20 + 16 * len(paare); dt = kt + len(keys)
    return b"\0PSF" + struct.pack("<IIII", 0x0101, kt, dt, len(paare)) + b"".join(struct.pack("<HHIII", offs[i], 0x0204, laengen[i], laengen[i] + ((-laengen[i]) % 4), doffs[i]) for i in range(len(paare))) + keys + daten
pj = json.dumps({"titleId": nummer, "localizedParameters": {"defaultLanguage": "en-US", "de-DE": {"titleName": de}, "en-US": {"titleName": en}}, "fueller": "x" * 5000}).encode()
D, R = 4, 8   # d_type: Ordner, Datei
if art in KLEIN:
    ordner(2, 168, stueck([(2, ".", D), (2, "..", D), (3, "eboot.bin", R), (35, "sce_sys", D)]))
    datei(3, b"e" * 100, [169])
    ordner(35, 680, stueck([(35, ".", D), (2, "..", D), (36, "param.json", R), (37, "icon0.png", R)]))
    datei(36, pj, [688, 696])
    if art == "klein": datei(37, b"\x89PNG\r\n\x1a\n" + b"p" * (80000 - 8), [200 + 8 * i for i in range(20)], ind=360)
    else:
        bild = b"\x89PNG\r\n\x1a\n" + bytes((i * 11 + i // 4096 * 29) % 256 for i in range(80000 - 8))
        fr = [200 + 8 * i for i in range(5)] + [400 + 8 * i for i in range(7)] + [248 + 8 * i for i in range(7)] + [320] if art == "bild_stuecke" \
             else [200 + 8 * i for i in range(19)] + [1020] if art == "bild_rand" else [200 + 8 * i for i in range(20)]
        if art == "bild_loch": fr[3] = 0
        if art == "bild_ausserhalb": fr[3] = 5000
        if art == "bild_doppelt": fr[7] = fr[6]
        if art == "bild_ueberlappt": fr[1] = 196   # überschneidet Block 0 (Fragmente 200 bis 207) zur Hälfte
        datei(37, bild, fr, ind=360)
        if art == "bild_ueberlappt": schreibe(200, bild[:BS])   # Block 0 zuletzt: die PNG-Kennung bleibt, nur die Überschneidung ist falsch
        if kopie: open(kopie, "wb").write(bild)
else:
    if art == "unterordner":
        ordner(2, 4, stueck([(2, ".", D), (2, "..", D), (7, nummer + "-app", D)]))
        ordner(7, 10, stueck([(7, ".", D), (2, "..", D), (3, "eboot.bin", R), (4, "sce_sys", D)]))
    elif art == "kaputt_verz":
        s1 = eintrag(2, ".", D) + eintrag(2, "..", D) + struct.pack("<IHBB", 3, 600, R, 9) + b"eboot.bin"; s1 += b"\0" * (512 - len(s1))
        ordner(2, 4, s1 + stueck([(4, "sce_sys", D)]))
    elif art == "inode_weit":
        ordner(2, 4, stueck([(2, ".", D), (2, "..", D), (3, "eboot.bin", R), (9999, "sce_sys", D)]))
    else:
        ordner(2, 4, stueck([(2, ".", D), (2, "..", D), (3, "eboot.bin", R), (4, "sce_sys", D)]))
    datei(3, b"e" * 100, [9])
    ordner(4, 5, stueck([(4, ".", D), (2, "..", D), (5, "param.sfo" if art == "sfo" else "param.json", R), (6, "icon0.png", R)]))
    datei(5, sfo([("TITLE", de), ("TITLE_ID", nummer), ("CATEGORY", "gd")]) if art == "sfo" else pj, [6])
    bild = b"" if art == "bild_kein_png" else b"\x89PNG\r\n\x1a\n"
    datei(6, bild + b"p" * (100000 - len(bild)), [7, 11] if art == "bild_verstreut" else [7, 8])
    if kopie: open(kopie, "wb").write(bild + b"p" * (100000 - len(bild)))
sb = bytearray(1376)
struct.pack_into("<iiii", sb, 8, 65536 // FS, 65536 // FS + max(1, 8192 // FS), IBLKNO, IBLKNO + IPG // INOPB * FRAG)
struct.pack_into("<I", sb, 44, NCG); struct.pack_into("<iii", sb, 48, 3000 if art == "kaputt_sb" else BS, FS, FRAG)
struct.pack_into("<iii", sb, 96, FRAG.bit_length() - 1, (FS // 512).bit_length() - 1, 4096)
struct.pack_into("<iI", sb, 116, BS // 8, INOPB); struct.pack_into("<Ii", sb, 184, IPG, FPG)
struct.pack_into("<qq", sb, 992, 65536, 65536); struct.pack_into("<qq", sb, 1080, SIZE, SIZE - 8)
struct.pack_into("<i", sb, 1324, 2); struct.pack_into("<I", sb, 1372, 0x19540119)
# Codex-Prüfung 125 (P2): die übrigen Rechenwerte wie newfs (fs_bmask/fmask/bshift/fshift, maxcontig, maxbpg, sbsize, cssize, cgsize,
# maxbsize, csaddr, maxsymlinklen, maxfilesize, qbmask/qfmask) und je Zylindergruppe ein Kopf mit CG_MAGIC 0x090255 - der Leser braucht
# sie nicht; die Gegenprobe mit einem echten UFS2 ist ufs2_dfvfs.img (test/kern/daten)
struct.pack_into("<iiiiii", sb, 72, -BS, -FS, BS.bit_length() - 1, FS.bit_length() - 1, 1, BS // 4)
struct.pack_into("<i", sb, 104, -(-1376 // FS) * FS); struct.pack_into("<ii", sb, 156, FS, FS); struct.pack_into("<ii", sb, 856, 0, BS)
struct.pack_into("<q", sb, 1096, IBLKNO + IPG // INOPB * FRAG); struct.pack_into("<i", sb, 1320, 120)
struct.pack_into("<Qqq", sb, 1328, 1 << 40, BS - 1, FS - 1)
for c in range(NCG): struct.pack_into("<I", img, (FPG * c + 65536 // FS + max(1, 8192 // FS)) * FS + 4, 0x090255)
img[65536:65536 + 1376] = sb
if len(img) > SIZE * FS: del img[SIZE * FS:]   # bild_rand: nur bis zum Ende des Dateisystems
if art == "kurz": img = img[:200000]
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
# NEU 1.0.9 (da_Chris86 08.11.2026 01:19): Bild aus der PKG-Datei - Spiel geht vor Update, Zusatzinhalt zählt nicht, verschlüsselt oder ohne PNG kein Bild
pkgdatei $W/mnt/usb0/Bild_A_Update.pkg "Bild Spiel" CUSA30040 gp 01.05
pkgdatei $W/mnt/usb0/Bild_B_Spiel.pkg "Bild Spiel" CUSA30040 gd 01.00
pkgdatei $W/mnt/usb0/Bild_Nur_Update.pkg "Nur Update" CUSA30041 gp 01.02
pkgdatei $W/mnt/usb0/Bild_Verschl.pkg "Bild Verschl" CUSA30042 gd 01.00 bild_verschl
pkgdatei $W/mnt/usb0/Bild_Kaputt.pkg "Bild Kaputt" CUSA30043 gd 01.00 bild_kaputt
pkgdatei $W/mnt/usb0/Bild_Zusatz.pkg "Bonus" CUSA30044 ac 01.00
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
exfatbild $W/mnt/usb0/PPSA30023.exfat bild_verstreut "Bild Verstreut" "Picture Scattered" PPSA30023 $T/bild_PPSA30023.png
exfatbild $W/mnt/usb0/PPSA30024.exfat bild_stuecke "Bild Stuecke" "Picture Pieces" PPSA30024 $T/bild_PPSA30024.png   # NEU #149
# Codex-Prüfung 134: kaputte Bildketten und die Grenzen
exfatbild $W/mnt/usb0/PPSA30025.exfat bild_kreis "Bild Kreis" "Picture Loop" PPSA30025
exfatbild $W/mnt/usb0/PPSA30026.exfat bild_kurz "Bild Kurz" "Picture Short" PPSA30026
exfatbild $W/mnt/usb0/PPSA30027.exfat bild_ungueltig "Bild Ungueltig" "Picture Invalid" PPSA30027
exfatbild $W/mnt/usb0/PPSA30028.exfat bild_64 "Bild 64" "Picture 64" PPSA30028 $T/bild_PPSA30028.png
exfatbild $W/mnt/usb0/PPSA30029.exfat bild_65 "Bild 65" "Picture 65" PPSA30029
exfatbild $W/mnt/usb0/PPSA30080.exfat bild_1024 "Bild 1024" "Picture 1024" PPSA30080 $T/bild_PPSA30080.png
exfatbild $W/mnt/usb0/PPSA30081.exfat bild_1025 "Bild 1025" "Picture 1025" PPSA30081
# NEU 1.0.9 (08.11.2026): UFS2-Abbilder (.ffpkg wie ShadowMountPlus mkufs2.sh, .ufs von kstuff-lite, .img von UFS2Tool)
ufsbild $W/mnt/usb0/PPSA30030.ffpkg normal "Ufs Spiel" "Ufs Game" PPSA30030
ufsbild $W/mnt/usb0/PPSA30031.ufs klein "Klein Spiel" "Small Game" PPSA30031
ufsbild $W/mnt/usb0/PPSA30032.ffpkg unterordner "Ufs Unterordner" "Ufs Subfolder" PPSA30032
ufsbild $W/mnt/usb0/CUSA30033.ffpkg sfo "PS4 Ufs" "PS4 Ufs" CUSA30033
ufsbild $W/mnt/usb0/PPSA30034.ffpkg bild_verstreut "Ufs Verstreut" "Ufs Scattered" PPSA30034 $T/bild_PPSA30034.png
ufsbild $W/mnt/usb0/PPSA30042.ufs bild_stuecke "Ufs Stuecke" "Ufs Pieces" PPSA30042 $T/bild_PPSA30042.png   # NEU #149
ufsbild $W/mnt/usb0/PPSA30043.ufs bild_loch "Ufs Loch" "Ufs Hole" PPSA30043   # Codex-Prüfung 134
ufsbild $W/mnt/usb0/PPSA30044.ufs bild_ausserhalb "Ufs Ausserhalb" "Ufs Outside" PPSA30044
ufsbild $W/mnt/usb0/PPSA30045.ufs bild_rand "Ufs Rand" "Ufs Edge" PPSA30045
ufsbild $W/mnt/usb0/PPSA30046.ufs bild_doppelt "Ufs Doppelt" "Ufs Twice" PPSA30046
ufsbild $W/mnt/usb0/PPSA30047.ufs bild_ueberlappt "Ufs Ueberlappt" "Ufs Overlap" PPSA30047   # Codex-Prüfung 135
ufsbild $W/mnt/usb0/PPSA30035.ffpkg kaputt_sb "X" "X" PPSA30035
ufsbild $W/mnt/usb0/PPSA30036.ffpkg kaputt_verz "Ufs Kaputt Verz" "Ufs Broken Dir" PPSA30036
ufsbild $W/mnt/usb0/PPSA30037.img normal "Ufs Img" "Ufs Img" PPSA30037
ufsbild $W/mnt/usb0/PPSA30038.ffpkg kurz "X" "X" PPSA30038
ufsbild $W/mnt/usb0/PPSA30039.ffpkg bild_kein_png "Ufs Ohne Png" "Ufs No Png" PPSA30039
ufsbild $W/mnt/usb0/PPSA30041.ffpkg inode_weit "X" "X" PPSA30041
# NEU 1.0.9 (#144): PFS-Abbilder (.ffpfs, .ffpfsc von ShadowMountPlus) - das innere Abbild der .ffpfsc liegt außerhalb des Baums (zum Vergleich)
pfsbild $W/mnt/usb0/PPSA30050.ffpfs normal "Pfs Spiel" "Pfs Game" PPSA30050
pfsbild $W/mnt/usb0/CUSA30051.ffpfs sfo "PS4 Pfs" "PS4 Pfs" CUSA30051
pfsbild $W/mnt/usb0/PPSA30052.ffpfs bild_kein_png "Pfs Ohne Png" "Pfs No Png" PPSA30052
exfatbild $T/innen_PPSA30053.exfat bild_kette "Pfsc Spiel" "Pfsc Game" PPSA30053
pfsbild $W/mnt/usb0/PPSA30053.ffpfsc gepackt "X" "X" PPSA30053 $T/innen_PPSA30053.exfat
exfatbild $T/innen_PPSA30054.exfat normal "Pfs Roh" "Pfs Raw" PPSA30054
pfsbild $W/mnt/usb0/PPSA30054.ffpfsc innen_roh "X" "X" PPSA30054 $T/innen_PPSA30054.exfat
pfsbild $W/mnt/usb0/PPSA30055.ffpfs verschluesselt "X" "X" PPSA30055
pfsbild $W/mnt/usb0/PPSA30056.ffpfs verstreut "X" "X" PPSA30056
pfsbild $W/mnt/usb0/PPSA30057.ffpfs kaputt_kennung "X" "X" PPSA30057
pfsbild $W/mnt/usb0/PPSA30058.ffpfs kurz "X" "X" PPSA30058
# Codex-Prüfung 130: langer Name, mehrere Dateien, fremder Eintrag, Inode-Arten; Entpacken gegen Pythons zlib und PFSC-Behälter
exfatbild $T/innen_PPSA30059.exfat normal "Pfs Lang" "Pfs Long" PPSA30059
pfsbild $W/mnt/usb0/PPSA30059.ffpfsc lang "X" "X" PPSA30059 $T/innen_PPSA30059.exfat
exfatbild $T/innen_falsch.exfat normal "Pfs Falsch" "Pfs Wrong" PPSA30060
pfsbild $W/mnt/usb0/PPSA30060.ffpfsc zwei "X" "X" PPSA30060 $T/innen_falsch.exfat
pfsbild $W/mnt/usb0/PPSA30061.ffpfsc zwei_lang "X" "X" PPSA30061 $T/innen_falsch.exfat
pfsbild $W/mnt/usb0/PPSA30062.ffpfsc fremde_art "X" "X" PPSA30062 $T/innen_falsch.exfat
pfsbild $W/mnt/usb0/PPSA30063.ffpfs link_json "Pfs Link" "Pfs Link" PPSA30063
pfsbild $W/mnt/usb0/PPSA30064.ffpfs ordner_c000 "Pfs C000" "Pfs C000" PPSA30064
pfsbild $W/mnt/usb0/PPSA30065.ffpfsc zwei_null "X" "X" PPSA30065 $T/innen_falsch.exfat
pfsbild $W/mnt/usb0/PPSA30066.ffpfsc kaputt_eintrag "X" "X" PPSA30066 $T/innen_falsch.exfat
pfsbild $W/mnt/usb0/PPSA30067.ffpfsc rest_kopf "X" "X" PPSA30067 $T/innen_falsch.exfat
pfsbild $W/mnt/usb0/PPSA30068.ffpfs sys_kaputt "Pfs Json" "Pfs Json" PPSA30068
pfsbild $W/mnt/usb0/PPSA30069.ffpfs wurzel_kaputt "Pfs Wurzel" "Pfs Root" PPSA30069
pfsbild $W/mnt/usb0/PPSA30072.ffpfs ind_minus "Pfs Minus" "Pfs Minus" PPSA30072
pfsbild $W/mnt/usb0/PPSA30073.ffpfs ind_gemischt "Pfs Gemischt" "Pfs Mixed" PPSA30073
# NEU #149: Bild in Stücken im gepackten inneren Abbild
exfatbild $T/innen_PPSA30070.exfat bild_stuecke_hinten "Pfsc Stuecke" "Pfsc Pieces" PPSA30070 $T/bild_PPSA30070.png   # gepackte PFSC-Blöcke 2 und 3
pfsbild $W/mnt/usb0/PPSA30070.ffpfsc gepackt "X" "X" PPSA30070 $T/innen_PPSA30070.exfat
exfatbild $T/innen_PPSA30071.exfat bild_stuecke "Pfs Roh Stuecke" "Pfs Raw Pieces" PPSA30071 $T/bild_PPSA30071.png   # Codex-Prüfung 134: mit Versatz
pfsbild $W/mnt/usb0/PPSA30071.ffpfsc innen_roh "X" "X" PPSA30071 $T/innen_PPSA30071.exfat
python3 -I $KK/test/kern/entpacken_faelle.py $T || echo "FAIL Testfälle zum Entpacken (entpacken_faelle.py) nicht erzeugt"
# Codex-Prüfung 125 (P2): Gegenprobe mit einem echten UFS2 (FreeBSD newfs, aus log2timeline/dfvfs - test/kern/daten/LIESMICH_ufs2_dfvfs.txt)
gzip -dc $KK/test/kern/daten/ufs2_dfvfs.raw.gz > $W/mnt/usb0/ufs2_dfvfs.img
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
