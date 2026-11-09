# PS5 Cleanup 1.0.9 - Testfälle für kern/kern_entpacken.c (Codex-Prüfung 130, 08.11.2026). Nur für die PC-Tests (lauf_ps4.sh).
#   python3 -I entpacken_faelle.py <ziel>
# <ziel>/zlib_faelle.bin - zlib- und Deflate-Ströme, jeweils mit der Entscheidung von Pythons zlib: gültig (mit dem entpackten Inhalt) oder
#   kaputt. Gruppen (erster Buchstabe des Namens): z = von zlib gepackt (Stufen, Strategien, Fenster, Blöcke), h = von Hand gebaut, gültig
#   (Länge 258, Abstand 32768, ein einziger Abstandscode ...), k = von Hand gebaut, kaputt (Tabellen, Wiederholungen, Rückverweise, Kopf,
#   Adler-32, abgeschnitten, zu wenig Platz), m = zufällig veränderte Ströme (zlib entscheidet).
#   Aufbau je Fall: u32 Namenslänge, Name, u8 Art ('g' gültig, 'k' kaputt), u8 zlib-Rahmen (1/0), u32 Platz im Ziel, u32 Länge der
#   Eingabe, Eingabe, u32 Länge des Inhalts, Inhalt (bei 'k' leer). Zahlen little endian.
# <ziel>/pfsc/ - PFSC-Behälter (*.bin) mit ihrem Inhalt (*.roh) und liste.txt: "<Behälter> <Inhalt|-> <ok|kaputt> <Anfang> <Länge>".
import heapq, os, random, struct, sys, zlib

ziel = sys.argv[1]
rng = random.Random(130)
FAELLE = []
def fall(name, art, rahmen, platz, ein, inhalt=b""):
    FAELLE.append((name, art, rahmen, platz, bytes(ein), bytes(inhalt) if art == "g" else b""))
def urteil(ein, rahmen, platz):           # Entscheidung von zlib; mehr als platz Byte gilt als kaputt (kern_entpacken hat dann keinen Platz)
    try:
        d = zlib.decompressobj(15 if rahmen else -15)
        out = d.decompress(ein, platz + 1)
        if len(out) > platz or not d.eof: return "k", b""
        return "g", out
    except zlib.error:
        return "k", b""
def nach_zlib(name, ein, rahmen=1, platz=None, soll=None):   # Fall mit der Entscheidung von zlib; soll = was erwartet wird (Prüfung des Generators)
    p = platz if platz is not None else 1 << 20
    a, out = urteil(ein, rahmen, p)
    if soll is not None and a != soll: raise SystemExit("Generator: %s ergibt bei zlib %s statt %s" % (name, a, soll))
    fall(name, a, rahmen, p if platz is not None or a == "k" else max(len(out), 1), ein, out)   # gültig: genau passender Platz; kaputt: viel Platz

# ---------------------------------------------------------------- Daten
WOERTER = ("der die das und ist nicht ein eine zu mit von auf den im fuer sich es dem PS5 Cleanup Spiel Abbild Datei Ordner Block "
           "Kachel Bild Name Kennung gepackt entpackt Tabelle").split()
def text(n):
    t = b""
    while len(t) < n: t += (" ".join(rng.choice(WOERTER) for _ in range(200)) + ".\n").encode()
    return t[:n]
def zufall(n): return rng.randbytes(n)
R800 = zufall(800)
DATEN = [("leer", b""), ("ein", b"a"), ("kurz", b"Hallo PS5"), ("zufall100", zufall(100)), ("zufall5000", zufall(5000)),
         ("nullen", b"\0" * 65536), ("text", text(20000)), ("gemischt", R800 + b"\0" * 30000 + R800 + text(3000)),
         ("wiederholt", b"abc" * 5000), ("lauf", b"x" * 1000 + b"y" * 3 + b"x" * 600)]
STRAT = [("std", zlib.Z_DEFAULT_STRATEGY), ("filter", zlib.Z_FILTERED), ("huff", zlib.Z_HUFFMAN_ONLY), ("rle", zlib.Z_RLE), ("fest", zlib.Z_FIXED)]
def packen(d, stufe=6, strat=zlib.Z_DEFAULT_STRATEGY, wbits=15, mem=8):
    c = zlib.compressobj(stufe, zlib.DEFLATED, wbits, mem, strat); return c.compress(d) + c.flush()

# ---------------------------------------------------------------- z: von zlib gepackt
for dn, d in DATEN:
    for stufe in (0, 1, 6, 9):
        for sn, s in STRAT:
            nach_zlib("z_%s_%d_%s" % (dn, stufe, sn), packen(d, stufe, s), soll="g")
for dn, d in DATEN[3:8]:
    nach_zlib("z_%s_fenster9" % dn, packen(d, 9, wbits=9), soll="g")
    nach_zlib("z_%s_speicher1" % dn, packen(d, 9, mem=1), soll="g")
    nach_zlib("z_%s_ohne_rahmen" % dn, packen(d, 6, wbits=-15), rahmen=0, soll="g")
for dn, d in (("text", DATEN[6][1]), ("gemischt", DATEN[7][1])):
    c = zlib.compressobj(6); s = b""; teile = [d[i:i + 2500] for i in range(0, len(d), 2500)]
    for j, t in enumerate(teile): s += c.compress(t) + c.flush([zlib.Z_SYNC_FLUSH, zlib.Z_FULL_FLUSH, zlib.Z_PARTIAL_FLUSH, zlib.Z_BLOCK][j % 4])
    nach_zlib("z_%s_bloecke" % dn, s + c.flush(), soll="g")
nach_zlib("z_rest_danach", zlib.compress(b"Hallo PS5 Cleanup", 9) + b"MUELL", soll="g")   # wie zlib: Daten hinter dem Strom stören nicht
t = DATEN[6][1]; z = zlib.compress(t, 9)
fall("z_platz_genau", "g", 1, len(t), z, t)
fall("k_platz_zu_klein", "k", 1, len(t) - 1, z)                                             # eigene Regel: kein Platz -> kaputt

# ---------------------------------------------------------------- Bauteile für von Hand gebaute Ströme
class Bits:
    def __init__(s): s.v = 0; s.n = 0; s.b = bytearray()
    def roh(s, wert, n):                                   # n Bits, niedrigstes zuerst
        s.v |= wert << s.n; s.n += n
        while s.n >= 8: s.b.append(s.v & 255); s.v >>= 8; s.n -= 8
    def code(s, c, l):                                     # Huffman-Code: höchstes Bit zuerst (Länge 0: kein Code, nichts)
        if l: s.roh(int(format(c, "0%db" % l)[::-1], 2) & ((1 << l) - 1), l)
    def grenze(s):
        if s.n: s.b.append(s.v & 255); s.v = 0; s.n = 0
    def fertig(s): s.grenze(); return bytes(s.b)
def kanon(laengen):                                        # kanonische Codes (RFC 1951 3.2.2)
    anz = [0] * 16
    for l in laengen: anz[l] += 1
    anz[0] = 0; c = 0; naechst = [0] * 16
    for b in range(1, 16): c = (c + anz[b - 1]) << 1; naechst[b] = c
    out = []
    for l in laengen:
        out.append(naechst[l] if l else None)
        if l: naechst[l] += 1
    return out
def huff_laengen(freq, maxl):                              # vollständiger Code aus Häufigkeiten (ein einziges Zeichen: Länge 1)
    sym = [i for i, f in enumerate(freq) if f > 0]; l = [0] * len(freq)
    if len(sym) == 1: l[sym[0]] = 1
    if len(sym) <= 1: return l
    while True:
        h = [(freq[i], k, [i]) for k, i in enumerate(sym)]; heapq.heapify(h); z = len(h); tiefe = dict.fromkeys(sym, 0)
        while len(h) > 1:
            a = heapq.heappop(h); b = heapq.heappop(h)
            for i in a[2] + b[2]: tiefe[i] += 1
            z += 1; heapq.heappush(h, (a[0] + b[0], z, a[2] + b[2]))
        if max(tiefe.values()) <= maxl:
            for i in sym: l[i] = tiefe[i]
            return l
        freq = [(f + 1) // 2 + 1 if f else 0 for f in freq]
LB = [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258]
LE = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0]
DB = [1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577]
DE = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13]
def l_sym(n):
    i = 28 if n == 258 else max(j for j in range(28) if LB[j] <= n)
    return 257 + i, n - LB[i], LE[i]
def d_sym(d):
    i = max(j for j in range(30) if DB[j] <= d)
    return i, d - DB[i], DE[i]
FEST_L = [8] * 144 + [9] * 112 + [7] * 24 + [8] * 8
FEST_D = [5] * 32
REIHE = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]
def symbole(bw, folge, ll, dl):   # folge: ("z", Zeichen) | ("r", Länge, Abstand) | ("lsym", Code) | ("dsym", Code) (Länge 3, roher Abstandscode)
    ll = (ll + [0] * 300)[:288]; dl = (dl + [0] * 40)[:32]; lc, dc = kanon(ll), kanon(dl)
    for s in folge:
        if s[0] == "z": bw.code(lc[s[1]], ll[s[1]])
        elif s[0] == "r":
            a, x, n = l_sym(s[1]); bw.code(lc[a], ll[a]); bw.roh(x, n)
            a, x, n = d_sym(s[2]); bw.code(dc[a], dl[a]); bw.roh(x, n)
        elif s[0] == "lsym": bw.code(lc[s[1]], ll[s[1]])
        elif s[0] == "dsym": bw.code(lc[257], ll[257]); bw.code(dc[s[1]], dl[s[1]])
    bw.code(lc[256], ll[256])
def fest(bw, letzt, folge): bw.roh(letzt, 1); bw.roh(1, 2); symbole(bw, folge, FEST_L, FEST_D)
def roh_block(bw, letzt, daten, nlen=None, kuerzen=0):
    bw.roh(letzt, 1); bw.roh(0, 2); bw.grenze(); n = len(daten)
    bw.b += struct.pack("<HH", n, (n ^ 0xFFFF) if nlen is None else nlen) + daten[:n - kuerzen]
def cl_kodieren(l):                                        # Code-Längen mit 16/17/18 wie zlib
    out = []; i = 0
    while i < len(l):
        w = l[i]; j = i
        while j < len(l) and l[j] == w: j += 1
        n = j - i
        if w == 0:
            while n >= 11: k = min(n, 138); out.append((18, k - 11)); n -= k
            if n >= 3: out.append((17, n - 3)); n = 0
            out += [(0, None)] * n
        else:
            out.append((w, None)); n -= 1
            while n >= 3: k = min(n, 6); out.append((16, k - 3)); n -= k
            out += [(w, None)] * n
        i = j
    return out
def dyn(bw, letzt, folge, ll=None, dl=None, cl_folge=None, cl_l=None, hlit=None, hdist=None):
    fl = [0] * 286; fd = [0] * 30; fl[256] = 1
    for s in folge:
        if s[0] == "z": fl[s[1]] += 1
        elif s[0] == "r": fl[l_sym(s[1])[0]] += 1; fd[d_sym(s[2])[0]] += 1
    if ll is None: ll = huff_laengen(fl, 15)
    if dl is None: dl = huff_laengen(fd, 15)
    nl = hlit if hlit is not None else max(257, max(i for i, x in enumerate(ll) if x) + 1)
    nd = hdist if hdist is not None else max([1] + [i + 1 for i, x in enumerate(dl) if x])
    ll = (ll + [0] * 300)[:nl]; dl = (dl + [0] * 40)[:nd]
    if cl_folge is None: cl_folge = cl_kodieren(ll + dl)
    if cl_l is None:
        f = [0] * 19
        for s, _ in cl_folge: f[s] += 1
        if sum(1 for x in f if x) == 1: f[18 if f[18] == 0 else 0] += 1          # ein einziger Code wäre unvollständig
        cl_l = huff_laengen(f, 7)
    hc = 19
    while hc > 4 and cl_l[REIHE[hc - 1]] == 0: hc -= 1
    bw.roh(letzt, 1); bw.roh(2, 2); bw.roh(nl - 257, 5); bw.roh(nd - 1, 5); bw.roh(hc - 4, 4)
    for i in range(hc): bw.roh(cl_l[REIHE[i]], 3)
    cc = kanon(cl_l)
    for s, x in cl_folge:
        bw.code(cc[s], cl_l[s])
        if s == 16: bw.roh(x, 2)
        elif s == 17: bw.roh(x, 3)
        elif s == 18: bw.roh(x, 7)
    symbole(bw, folge, ll, dl)
def rahmen(deflate, inhalt, cinfo=7, cm=8, fdict=False, fcheck_falsch=False, adler_xor=0, dictid=b""):
    cmf = cinfo << 4 | cm; flg = 0x20 if fdict else 0
    flg += (31 - (cmf * 256 + flg) % 31) % 31
    if fcheck_falsch: flg ^= 1
    return bytes([cmf, flg]) + dictid + deflate + struct.pack(">I", zlib.adler32(inhalt) ^ adler_xor)
def strom(bauen, inhalt=None):                             # Strom mit zlib-Rahmen; Inhalt für die Prüfsumme (sonst von zlib ohne Rahmen)
    bw = Bits(); bauen(bw); d = bw.fertig()
    if inhalt is None: _, inhalt = urteil(d, 0, 1 << 20)
    return d, inhalt

# ---------------------------------------------------------------- h: von Hand gebaut, gültig (zlib muss zustimmen)
def h(name, bauen):
    d, inhalt = strom(bauen); nach_zlib(name, rahmen(d, inhalt), soll="g")
h("h_laenge258", lambda b: fest(b, 1, [("z", 120), ("r", 258, 1), ("r", 258, 1)]))
R32 = zufall(32768)
h("h_abstand32768", lambda b: (roh_block(b, 0, R32), fest(b, 1, [("r", 258, 32768), ("r", 3, 32768), ("r", 4, 1)])))
h("h_ueberlappend", lambda b: fest(b, 1, [("z", 97), ("z", 98), ("r", 7, 2)]))
h("h_alle_laengen", lambda b: fest(b, 1, [("z", 1)] + [("r", n, 1) for n in range(3, 259)]))
h("h_alle_abstaende", lambda b: (roh_block(b, 0, zufall(32768)), fest(b, 1, [("r", 3, d) for d in DB] + [("r", 3, d + (1 << e) - 1) for d, e in zip(DB, DE)])))
h("h_ein_abstand", lambda b: dyn(b, 1, [("z", 97), ("z", 98)] + [("r", 10, 2)] * 3))
h("h_kein_abstand", lambda b: dyn(b, 1, [("z", c) for c in b"nur Zeichen"]))
h("h_nur_ende", lambda b: dyn(b, 1, []))
h("h_alle_zeichen", lambda b: dyn(b, 1, [("z", c) for c in range(256)] * 3 + [("r", 258, 768)]))
h("h_viele_bloecke", lambda b: (roh_block(b, 0, b"abc"), roh_block(b, 0, b""), fest(b, 0, [("z", 100)]), dyn(b, 0, [("r", 3, 4)]), roh_block(b, 1, b"def")))
h("h_roh_leer", lambda b: roh_block(b, 1, b""))

# ---------------------------------------------------------------- k: von Hand gebaut, kaputt (zlib muss ablehnen)
def k(name, bauen, inhalt=b"x", **r):         # mit Rahmen und ohne (ohne Prüfsumme: kaputt nur wegen des Fehlers selbst)
    bw = Bits(); bauen(bw); d = bw.fertig()
    nach_zlib(name, rahmen(d, inhalt, **r), soll="k"); nach_zlib(name + "_ohne_rahmen", d, rahmen=0, soll="k")
k("k_blockart3", lambda b: (b.roh(1, 1), b.roh(3, 2), b.roh(0, 16)))
k("k_roh_nlen", lambda b: roh_block(b, 1, b"abc", nlen=0x1234))
k("k_roh_kurz", lambda b: roh_block(b, 1, b"abcdefgh" * 20, kuerzen=60))
k("k_zu_weit", lambda b: fest(b, 1, [("z", 97), ("r", 3, 2)]))
k("k_zu_weit_32768", lambda b: (roh_block(b, 0, zufall(1000)), fest(b, 1, [("r", 3, 32768)])))
k("k_zu_weit_leer", lambda b: fest(b, 1, [("r", 3, 1)]))
k("k_abstandscode30", lambda b: fest(b, 1, [("z", 97), ("z", 98), ("z", 99), ("dsym", 30)]))
k("k_abstandscode31", lambda b: fest(b, 1, [("z", 97), ("z", 98), ("z", 99), ("dsym", 31)]))
k("k_laengencode286", lambda b: fest(b, 1, [("z", 97), ("lsym", 286)]))
k("k_laengencode287", lambda b: fest(b, 1, [("z", 97), ("lsym", 287)]))
k("k_hlit287", lambda b: dyn(b, 1, [("z", 97)], hlit=287))
k("k_hlit288", lambda b: dyn(b, 1, [("z", 97)], hlit=288))
k("k_hdist31", lambda b: dyn(b, 1, [("z", 97), ("z", 98), ("r", 3, 2)], hdist=31))
k("k_hdist32", lambda b: dyn(b, 1, [("z", 97), ("z", 98), ("r", 3, 2)], hdist=32))
VOLL = [("z", c) for c in b"Hallo PS5 Cleanup"] + [("r", 5, 6)]
def cl_mit(laengen): l = [0] * 19; [l.__setitem__(s, v) for s, v in laengen.items()]; return l
k("k_cl_ueberbelegt", lambda b: dyn(b, 1, VOLL, cl_l=cl_mit({0: 1, 8: 1, 18: 1})))
k("k_cl_unvollstaendig", lambda b: dyn(b, 1, VOLL, cl_l=cl_mit({0: 2, 8: 2})))
k("k_cl_einzeln", lambda b: dyn(b, 1, VOLL, ll=[8] * 286, dl=[8], cl_folge=[(8, None)] * 287, cl_l=cl_mit({8: 1})))
k("k_16_am_anfang", lambda b: dyn(b, 1, VOLL, cl_folge=[(16, 0), (18, 127), (18, 127)]))
def wdh_ueberlauf(sym, ex):                                # Wiederholung über das Ende der Längen hinaus
    ll = huff_laengen([1] * 3 + [0] * 253 + [1], 15); cf = cl_kodieren((ll + [0] * 300)[:257] + [1])
    return cf[:-1] + [(sym, ex)]
k("k_16_ueberlauf", lambda b: dyn(b, 1, [("z", 0)], ll=huff_laengen([1] * 3 + [0] * 253 + [1], 15), dl=[1], cl_folge=wdh_ueberlauf(16, 3)))
k("k_17_ueberlauf", lambda b: dyn(b, 1, [("z", 0)], ll=huff_laengen([1] * 3 + [0] * 253 + [1], 15), dl=[1], cl_folge=wdh_ueberlauf(17, 7)))
k("k_18_ueberlauf", lambda b: dyn(b, 1, [("z", 0)], ll=huff_laengen([1] * 3 + [0] * 253 + [1], 15), dl=[1], cl_folge=wdh_ueberlauf(18, 127)))
k("k_ll_ueberbelegt", lambda b: dyn(b, 1, [("z", 97)], ll=[0] * 97 + [1, 1] + [0] * 157 + [1, 1], dl=[0]))
k("k_ll_unvollstaendig", lambda b: dyn(b, 1, [("z", 97)], ll=[0] * 97 + [2] + [0] * 158 + [2], dl=[0]))
k("k_dl_unvollstaendig", lambda b: dyn(b, 1, [("z", 97), ("z", 98), ("r", 3, 2)], dl=[0, 2, 2]))
k("k_dl_ueberbelegt", lambda b: dyn(b, 1, [("z", 97), ("z", 98), ("r", 3, 2)], dl=[1, 1, 1]))
k("k_ohne_ende", lambda b: dyn(b, 1, [("z", 97)], ll=[0] * 97 + [1] + [0] * 159 + [1], dl=[0]))
d0, i0 = strom(lambda b: fest(b, 1, VOLL))
nach_zlib("h_bezug", rahmen(d0, i0), soll="g")
nach_zlib("k_kopf_cm7", rahmen(d0, i0, cm=7), soll="k")
nach_zlib("k_kopf_fenster8", rahmen(d0, i0, cinfo=8), soll="k")
nach_zlib("k_kopf_pruefwert", rahmen(d0, i0, fcheck_falsch=True), soll="k")
nach_zlib("k_kopf_woerterbuch", rahmen(d0, i0, fdict=True, dictid=b"\0\0\0\1"), soll="k")
for j in range(4): nach_zlib("k_adler_%d" % j, rahmen(d0, i0, adler_xor=0xFF << (8 * j)), soll="k")
s0 = rahmen(d0, i0)
for j in range(1, 5): nach_zlib("k_ohne_adler_%d" % j, s0[:-j], soll="k")
kz = zlib.compress(b"Hallo Hallo Hallo PS5 Cleanup " * 3, 9)
for j in range(len(kz)): nach_zlib("k_abgeschnitten_%d" % j, kz[:j], soll="k")
kd = packen(DATEN[6][1][:3000], 6)
for j in range(0, len(kd), 7): nach_zlib("k_abgeschnitten_text_%d" % j, kd[:j], soll="k")
nach_zlib("k_leer", b"", soll="k")

# ---------------------------------------------------------------- m: zufällig verändert (zlib entscheidet)
basis = [packen(d, stufe, s) for d in (b"Hallo PS5 Cleanup " * 20, text(1500), zufall(300), DATEN[7][1][:4000], b"abc" * 300)
         for stufe, s in ((1, zlib.Z_DEFAULT_STRATEGY), (9, zlib.Z_DEFAULT_STRATEGY), (6, zlib.Z_FIXED))]
basis += [rahmen(*strom(lambda b: dyn(b, 1, [("z", c) for c in b"PS5 Cleanup liest gepackte Abbilder"] + [("r", 12, 9)]))),
          rahmen(*strom(lambda b: (roh_block(b, 0, b"roh" * 30), fest(b, 0, [("z", 65), ("r", 40, 1)]), dyn(b, 1, [("z", 66), ("r", 30, 50)]))))]
def veraendern(b, kopf):
    m = bytearray(b); w = rng.randrange(7)
    if w == 0: m[rng.randrange(len(m))] ^= 1 << rng.randrange(8)
    elif w == 1:
        for _ in range(rng.randint(2, 4)): m[rng.randrange(len(m))] ^= 1 << rng.randrange(8)
    elif w == 2: m[rng.randrange(len(m))] = rng.randrange(256)
    elif w == 3: del m[rng.randrange(len(m))]
    elif w == 4: m.insert(rng.randrange(len(m) + 1), rng.randrange(256))
    elif w == 5: m = m[:rng.randrange(len(m))]
    else: p = rng.randrange(2, len(m)); m[p:p] = m[rng.randrange(2, len(m)):][:rng.randint(1, 16)]
    if rng.randrange(3) == 0 and len(m) > kopf: m[kopf] ^= 1 << rng.randrange(8)   # oft im ersten Block-Kopf
    return bytes(m)
for bi, b in enumerate(basis):
    for j in range(110): nach_zlib("m_%d_%d" % (bi, j), veraendern(b, 2), platz=200000)
# ohne zlib-Rahmen (ohne Prüfsumme): veränderte Ströme ergeben oft etwas anderes - das muss Byte für Byte wie bei zlib sein
basis_roh = [packen(d, stufe, s, wbits=-15) for d in (b"Hallo PS5 Cleanup " * 20, text(1500), zufall(300), DATEN[7][1][:4000], b"abc" * 300)
             for stufe, s in ((1, zlib.Z_DEFAULT_STRATEGY), (9, zlib.Z_DEFAULT_STRATEGY), (6, zlib.Z_FIXED), (6, zlib.Z_HUFFMAN_ONLY))]
basis_roh += [strom(lambda b: dyn(b, 1, [("z", c) for c in range(256)] + [("r", 258, 256), ("r", 3, 1)]))[0],
              strom(lambda b: (roh_block(b, 0, b"roh" * 30), fest(b, 0, [("z", 65), ("r", 40, 1)]), dyn(b, 1, [("z", 66), ("r", 30, 50)])))[0]]
for bi, b in enumerate(basis_roh):
    for j in range(110): nach_zlib("m_roh_%d_%d" % (bi, j), veraendern(b, 0), rahmen=0, platz=200000)

with open(os.path.join(ziel, "zlib_faelle.bin"), "wb") as f:
    for name, art, rahmen_ja, platz, ein, inhalt in FAELLE:
        n = name.encode()
        f.write(struct.pack("<I", len(n)) + n + art.encode() + bytes([rahmen_ja]) + struct.pack("<II", platz, len(ein)) + ein + struct.pack("<I", len(inhalt)) + inhalt)
zaehl = {}
for name, art, *_ in FAELLE: zaehl[name[0] + art] = zaehl.get(name[0] + art, 0) + 1
print("zlib_faelle.bin: %d Fälle %s" % (len(FAELLE), " ".join("%s=%d" % kv for kv in sorted(zaehl.items()))))

# ---------------------------------------------------------------- PFSC-Behälter (Aufbau wie pfsbild in lauf_ps4.sh)
BS = 65536
P = os.path.join(ziel, "pfsc"); os.makedirs(P, exist_ok=True)
LISTE = []
def pfsc(inhalt, ersatz=None):            # Block ungepackt, wenn zlib nicht kürzer ist; ersatz: Block-Nr. -> gespeicherte Bytes
    bl = (len(inhalt) + BS - 1) // BS; tab, daten = 0x400, 0x10000; bloecke = []
    for i in range(bl):
        roh = inhalt[i * BS:(i + 1) * BS]; z = zlib.compress(roh, 6)
        bloecke.append((ersatz or {}).get(i, roh if len(z) >= len(roh) else z))
    offs = []; o = daten
    for b in bloecke: offs.append(o); o += len(b)
    offs.append(o)
    c = bytearray(daten); c[0:0x30] = b"PFSC" + struct.pack("<IIIQQQQ", 0, 6, BS, BS, tab, daten, len(inhalt))
    struct.pack_into("<%dQ" % len(offs), c, tab, *offs)
    return bytearray(bytes(c) + b"".join(bloecke)), offs
def ablegen(name, behaelter, inhalt=None):
    open(os.path.join(P, name + ".bin"), "wb").write(bytes(behaelter))
    if inhalt is not None: open(os.path.join(P, name + ".roh"), "wb").write(bytes(inhalt))
def lesen(name, quelle, erwartung, ab, n): LISTE.append("%s.bin %s %s %d %d" % (name, quelle + ".roh" if quelle else "-", erwartung, ab, n))
CODEX = bytes.fromhex("789c4b4c840300190d042c")
assert zlib.decompress(CODEX) == b"a" * 11                       # die Falle aus Codex-Prüfung 130: ungepackt, aber ein gültiger zlib-Strom
VARIANTEN = {"gepackt_ende": text(1000), "roh_ende": zufall(1000), "codex_ende": CODEX, "voll": b""}
for name, letzt in VARIANTEN.items():
    inhalt = text(BS) + zufall(BS) + text(BS) + letzt
    c, offs = pfsc(inhalt)
    arten = ["roh" if offs[i + 1] - offs[i] == min(BS, len(inhalt) - i * BS) else "zlib" for i in range(len(offs) - 1)]
    assert arten[:3] == ["zlib", "roh", "zlib"] and (not letzt or arten[3] == ("zlib" if name == "gepackt_ende" else "roh")), (name, arten)
    ablegen(name, c, inhalt)
    for ab, n in ((100, 5000), (BS - 100, 300), (2 * BS - 50, 100), (3 * BS - 10, 10 + len(letzt)), (0, len(inhalt)), (len(inhalt) - 1, 1)) + (((3 * BS, len(letzt)),) if letzt else ()):
        lesen(name, name, "ok", ab, n)
    lesen(name, None, "kaputt", len(inhalt) - 4, 8)              # hinter dem Ende
    lesen(name, None, "kaputt", len(inhalt), 1)
basis_inhalt = text(BS) + zufall(BS) + text(BS) + text(1000)
c0, o0 = pfsc(basis_inhalt)
ablegen("basis", c0, basis_inhalt)
for ab, n in ((0, 10), (BS, 10), (2 * BS, 10), (3 * BS, 1000)): lesen("basis", "basis", "ok", ab, n)
def tabelle(name, nr, wert, kaputt_ab, gut_ab):
    c = bytearray(c0); struct.pack_into("<Q", c, 0x400 + 8 * nr, wert); ablegen(name, c)
    lesen(name, None, "kaputt", kaputt_ab, 10); lesen(name, "basis", "ok", gut_ab, 10)
tabelle("t_von_vor_daten", 0, 0x10000 - 8, 0, 2 * BS)
tabelle("t_bis_vor_von", 1, o0[0] - 1, 0, 2 * BS)
tabelle("t_bis_hinter_ende", 4, len(c0) + 1, 3 * BS, 0)
tabelle("t_laenge_null", 1, o0[0], 0, 2 * BS)
tabelle("t_zu_lang", 2, o0[1] + BS + 8, BS, 0)
def block_ersatz(name, nr, b, kaputt_ab, gut_ab):
    c, _ = pfsc(basis_inhalt, {nr: b}); ablegen(name, c)
    lesen(name, None, "kaputt", kaputt_ab, 10); lesen(name, "basis", "ok", gut_ab, 10)
block_ersatz("z_ergibt_zu_wenig", 0, zlib.compress(basis_inhalt[:1000], 6), 0, 2 * BS)
block_ersatz("z_ergibt_zu_viel", 3, zlib.compress(text(2000), 6), 3 * BS, 0)
zk = bytearray(zlib.compress(basis_inhalt[:BS], 6)); zk[len(zk) // 2] ^= 0x55
try: zlib.decompress(bytes(zk)); raise SystemExit("Generator: z_kaputt ist noch gültig")
except zlib.error: pass
block_ersatz("z_kaputt", 0, bytes(zk), 0, 2 * BS)
block_ersatz("r_letzter_aufgefuellt", 3, basis_inhalt[3 * BS:] + b"\0" * (BS - 1000), 3 * BS, 0)   # 64 KB ungepackt, Inhalt aber nur 1000 Byte
def kopf(name, ab, fmt, wert):
    c = bytearray(c0); struct.pack_into(fmt, c, ab, wert); ablegen(name, c); lesen(name, None, "kaputt", 0, 10)
kopf("k_kennung", 0, "<4s", b"PFSX")
kopf("k_blockgroesse_0c", 0x0C, "<I", 0x8000)
kopf("k_blockgroesse_10", 0x10, "<Q", 0x20000)
kopf("k_tabelle_vor_kopf", 0x18, "<Q", 0x20)
kopf("k_tabelle_hinter_ende", 0x18, "<Q", len(c0) + 8)
kopf("k_daten_in_tabelle", 0x20, "<Q", 0x400 + 8)
kopf("k_daten_hinter_ende", 0x20, "<Q", len(c0) + 1)
kopf("k_laenge_null", 0x28, "<Q", 0)
kopf("k_laenge_riesig", 0x28, "<Q", (1 << 40) + 1)
c = bytearray(c0); ablegen("k_kurz", c[:0x2F]); lesen("k_kurz", None, "kaputt", 0, 10)
open(os.path.join(P, "liste.txt"), "w").write("\n".join(LISTE) + "\n")
print("pfsc: %d Behälter, %d Lesefälle" % (sum(1 for x in os.listdir(P) if x.endswith(".bin")), len(LISTE)))
