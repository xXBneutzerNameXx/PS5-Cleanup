/* PS5 Cleanup - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * kern_entpacken.c - NEU 1.0.9 (08.11.2026, Formate von ShadowMountPlus: .ffpfsc): nur lesen.
 *  - kern_entpacken: zlib-Daten entpacken (RFC 1950 und 1951). Eigene, kleine Umsetzung (kanonische Huffman-Codes, Bit für Bit gelesen) -
 *    langsam, aber gebraucht werden nur wenige Blöcke je Abbild. Jeder Wert aus den Daten wird geprüft: nie über die Eingabe hinaus lesen,
 *    nie über den Platz im Ziel hinaus schreiben, Rückverweise nur in schon Geschriebenes, Prüfsumme (Adler-32) am Ende. Kaputte Daten
 *    ergeben -1, nie einen Absturz. Was gültig ist, entscheidet es wie zlib in der üblichen Einstellung (inflate.c, inftrees.c; ohne
 *    Wörterbuch, ohne PKZIP_BUG_WORKAROUND): dieselben Ströme gelten als gültig
 *    bzw. kaputt (Codex-Prüfung 130; im Test gegen Pythons zlib geprüft, auch mit zufällig veränderten Strömen).
 *  - kern_pfsc_*: eine gepackte Datei in einem PFS-Abbild (.ffpfsc: darin ein exFAT-Abbild) stückweise lesen. Aufbau nach
 *    quer3q/ps5-dump-forge (crates/ps5-dump-forge-pfs, GPL-3.0, nur gelesen) und an echten Dateien nachgeprüft: Kopf "PFSC", Blöcke zu
 *    64 KB, eine Tabelle mit der Stelle jedes Blocks. Ein Block ist ungepackt, wenn er genau so lang ist wie sein Inhalt (64 KB, der letzte
 *    Block auch kürzer), sonst ein zlib-Strom, der genau seinen Inhalt ergibt (in den echten Dateien ist jeder kürzere Block so gepackt).
 *    Die Suche liest so Name, Kennung und Bild aus dem inneren Abbild, die Oberfläche das Bild - nie das ganze Abbild.
 */
#include "kern.h"
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const unsigned char *e; size_t en, ep;      /* Eingabe, Länge, Stelle */
    unsigned bits, bn;                          /* noch nicht gelesene Bits (niedrigste zuerst) und ihre Anzahl */
    unsigned char *a; size_t amax, ap;          /* Ausgabe */
    bool kaputt;
} ent;
typedef struct { short anz[16], sym[320]; } huff;   /* Anzahl Codes je Länge, Zeichen nach Code geordnet */

static unsigned bits_holen(ent *s, unsigned n)   /* n <= 16 Bits, niedrigstes zuerst */
{
    while (s->bn < n) {
        if (s->ep >= s->en) { s->kaputt = true; return 0; }
        s->bits |= (unsigned)s->e[s->ep++] << s->bn;
        s->bn += 8;
    }
    unsigned v = s->bits & ((1u << n) - 1);
    s->bits >>= n; s->bn -= n;
    return v;
}
/* Tabelle aus den Code-Längen bauen - so streng wie zlib (inftrees.c; Codex-Prüfung 130): zu viele Codes einer Länge oder ein unvollständiger
 * Code sind kaputt. Erlaubt sind nur gar kein Code (beim Lesen dann -1) und - nicht bei der Tabelle der Code-Längen (einzeln_ok) - ein
 * einziger Code der Länge 1 (RFC 1951 3.2.7, z. B. nur ein Abstand); dessen freie Bitfolge ergibt beim Lesen -1. */
static bool huff_bauen(huff *h, const unsigned char *laengen, int n, bool einzeln_ok)
{
    short stelle[16];
    memset(h->anz, 0, sizeof h->anz);
    for (int i = 0; i < n; i++) h->anz[laengen[i]]++;
    if (h->anz[0] == n) return true;                 /* gar keine Codes (z. B. keine Abstände): beim Lesen -1 */
    int frei = 1, max = 0;
    for (int l = 1; l < 16; l++) { frei <<= 1; frei -= h->anz[l]; if (frei < 0) return false; if (h->anz[l]) max = l; }
    if (frei > 0 && !(einzeln_ok && max == 1)) return false;   /* unvollständig */
    stelle[1] = 0;
    for (int l = 1; l < 15; l++) stelle[l + 1] = (short)(stelle[l] + h->anz[l]);
    for (int i = 0; i < n; i++) if (laengen[i]) h->sym[stelle[laengen[i]]++] = (short)i;
    return true;
}
static int huff_lesen(ent *s, const huff *h)
{
    int code = 0, erst = 0, index = 0;
    for (int l = 1; l < 16; l++) {
        code |= (int)bits_holen(s, 1);
        if (s->kaputt) return -1;
        int anz = h->anz[l];
        if (code - anz < erst) return h->sym[index + (code - erst)];
        index += anz; erst += anz; erst <<= 1; code <<= 1;
    }
    return -1;                                       /* kein gültiger Code */
}
static const short L_BASIS[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const short L_EXTRA[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const short D_BASIS[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const short D_EXTRA[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
static bool codes_lesen(ent *s, const huff *lz, const huff *ab)
{
    for (;;) {
        int z = huff_lesen(s, lz);
        if (z < 0) return false;
        if (z < 256) { if (s->ap >= s->amax) return false; s->a[s->ap++] = (unsigned char)z; continue; }
        if (z == 256) return true;
        z -= 257;
        if (z >= 29) return false;                   /* 286, 287 */
        size_t len = (size_t)L_BASIS[z] + bits_holen(s, (unsigned)L_EXTRA[z]);
        int d = huff_lesen(s, ab);
        if (d < 0 || d >= 30) return false;          /* 30, 31 */
        size_t abst = (size_t)D_BASIS[d] + bits_holen(s, (unsigned)D_EXTRA[d]);
        if (s->kaputt || abst > s->ap || len > s->amax - s->ap) return false;
        for (size_t i = 0; i < len; i++, s->ap++) s->a[s->ap] = s->a[s->ap - abst];   /* überlappend erlaubt (byteweise) */
    }
}
static huff fest_lz, fest_ab;
static pthread_once_t fest_einmal = PTHREAD_ONCE_INIT;
static void fest_bauen(void)                         /* feste Codes (RFC 1951 3.2.6) - einmal für alle Fäden */
{
    unsigned char l[288];
    int i = 0;
    for (; i < 144; i++) l[i] = 8;
    for (; i < 256; i++) l[i] = 9;
    for (; i < 280; i++) l[i] = 7;
    for (; i < 288; i++) l[i] = 8;
    huff_bauen(&fest_lz, l, 288, false);
    for (i = 0; i < 32; i++) l[i] = 5;               /* 32 Abstandscodes wie zlib (vollständig) - 30 und 31 sind beim Lesen kaputt */
    huff_bauen(&fest_ab, l, 32, false);
}
static bool block_fest(ent *s)
{
    pthread_once(&fest_einmal, fest_bauen);
    return codes_lesen(s, &fest_lz, &fest_ab);
}
static bool block_dynamisch(ent *s)
{
    static const unsigned char REIHE[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    unsigned nlen = bits_holen(s, 5) + 257, ndist = bits_holen(s, 5) + 1, ncode = bits_holen(s, 4) + 4;
    /* wie zlib (inflate.c) und puff.c: mehr als 286 Längen- oder 30 Abstandscodes sind kaputt. RFC 1951 lässt 32 Abstandscodes zu, die
     * zwei letzten dürfen aber nie vorkommen und kein Packer schreibt sie (Codex-Prüfung 130: bewusst wie zlib gelassen) */
    if (s->kaputt || nlen > 286 || ndist > 30) return false;
    unsigned char l[320];
    memset(l, 0, sizeof l);
    for (unsigned i = 0; i < ncode; i++) l[REIHE[i]] = (unsigned char)bits_holen(s, 3);
    huff cl, lz, ab;
    if (s->kaputt || !huff_bauen(&cl, l, 19, false)) return false;
    unsigned i = 0;
    while (i < nlen + ndist) {
        int z = huff_lesen(s, &cl);
        if (z < 0) return false;
        if (z < 16) { l[i++] = (unsigned char)z; continue; }
        unsigned wdh; unsigned char w = 0;
        if (z == 16) { if (i == 0) return false; w = l[i - 1]; wdh = 3 + bits_holen(s, 2); }
        else if (z == 17) wdh = 3 + bits_holen(s, 3);
        else wdh = 11 + bits_holen(s, 7);
        if (s->kaputt || i + wdh > nlen + ndist) return false;
        while (wdh--) l[i++] = w;
    }
    if (l[256] == 0) return false;                  /* ohne Ende-Zeichen geht es nicht */
    unsigned char la[30];
    memcpy(la, l + nlen, ndist);
    if (!huff_bauen(&lz, l, (int)nlen, true) || !huff_bauen(&ab, la, (int)ndist, true)) return false;
    return codes_lesen(s, &lz, &ab);
}
static bool block_roh(ent *s)
{
    s->bits = 0; s->bn = 0;                         /* auf das nächste Byte */
    if (s->ep + 4 > s->en) return false;
    unsigned len = s->e[s->ep] | (unsigned)s->e[s->ep + 1] << 8, nlen = s->e[s->ep + 2] | (unsigned)s->e[s->ep + 3] << 8;
    s->ep += 4;
    if ((len ^ 0xFFFFu) != nlen || len > s->en - s->ep || len > s->amax - s->ap) return false;
    memcpy(s->a + s->ap, s->e + s->ep, len);
    s->ep += len; s->ap += len;
    return true;
}
long kern_entpacken(const unsigned char *ein, size_t en, unsigned char *aus, size_t amax, bool zlib_rahmen)
{
    ent s = { ein, en, 0, 0, 0, aus, amax, 0, false };
    if (zlib_rahmen) {
        if (en < 6) return -1;
        unsigned cmf = ein[0], flg = ein[1];
        if ((cmf & 15) != 8 || (cmf >> 4) > 7 || (cmf * 256 + flg) % 31 != 0 || (flg & 0x20)) return -1;   /* deflate, Fenster <= 32 KB, kein Wörterbuch */
        s.ep = 2;
    }
    for (;;) {
        unsigned letzt = bits_holen(&s, 1), art = bits_holen(&s, 2);
        if (s.kaputt) return -1;
        bool ok = art == 0 ? block_roh(&s) : art == 1 ? block_fest(&s) : art == 2 ? block_dynamisch(&s) : false;
        if (!ok || s.kaputt) return -1;
        if (letzt) break;
    }
    if (zlib_rahmen) {
        s.bits = 0; s.bn = 0;
        if (s.ep + 4 > s.en) return -1;
        unsigned long a1 = 1, a2 = 0;
        for (size_t i = 0; i < s.ap; i++) { a1 = (a1 + s.a[i]) % 65521; a2 = (a2 + a1) % 65521; }
        unsigned long soll = (unsigned long)s.e[s.ep] << 24 | (unsigned long)s.e[s.ep + 1] << 16 | (unsigned long)s.e[s.ep + 2] << 8 | s.e[s.ep + 3];
        if (soll != (a2 << 16 | a1)) return -1;
    }
    return (long)s.ap;
}

/* ---------------------------------------------------------------- PFSC (gepackte Datei in einem PFS-Abbild) */
#define PFSC_BLOCK 65536ULL
static unsigned long long le64(const unsigned char *b) { unsigned long long v = 0; for (int i = 7; i >= 0; i--) v = v << 8 | b[i]; return v; }
static unsigned le32(const unsigned char *b) { return (unsigned)b[0] | (unsigned)b[1] << 8 | (unsigned)b[2] << 16 | (unsigned)b[3] << 24; }
/* Behälter ab Stelle ab (gespeicherte Länge gr) öffnen; Puffer aus a. false = kein gültiger PFSC-Kopf */
bool kern_pfsc_auf(kern_pfsc *p, arena *a, bool (*lies)(void *c, unsigned long long ab, size_t n, unsigned char *ziel), void *c,
                   unsigned long long ab, unsigned long long gr)
{
    memset(p, 0, sizeof *p);
    unsigned char k[0x30];
    if (gr < 0x30 || ab > ~0ULL - gr || !lies(c, ab, sizeof k, k)) return false;
    if (memcmp(k, "PFSC", 4) != 0 || le32(k + 0x0C) != PFSC_BLOCK || le64(k + 0x10) != PFSC_BLOCK) return false;
    unsigned long long tab = le64(k + 0x18), daten = le64(k + 0x20), laenge = le64(k + 0x28);
    if (laenge == 0 || laenge > (1ULL << 40)) return false;                     /* höchstens 1 TB ungepackt */
    unsigned long long bl = (laenge + PFSC_BLOCK - 1) / PFSC_BLOCK;
    if (tab < 0x30 || tab > gr || bl + 1 > (gr - tab) / 8 || daten < tab + (bl + 1) * 8 || daten > gr) return false;
    p->lies = lies; p->c = c; p->ab = ab; p->gr = gr; p->tab = tab; p->daten = daten; p->laenge = laenge; p->bloecke = bl;
    p->im_puffer = -1;
    p->puffer = a_hole(a, (size_t)PFSC_BLOCK); p->roh = a_hole(a, (size_t)PFSC_BLOCK);
    return true;
}
static bool pfsc_block(kern_pfsc *p, unsigned long long nr)
{
    if ((long long)nr == p->im_puffer) return true;
    p->im_puffer = -1;
    if (nr >= p->bloecke) return false;
    unsigned char t[16];
    if (!p->lies(p->c, p->ab + p->tab + nr * 8, sizeof t, t)) return false;
    unsigned long long von = le64(t), bis = le64(t + 8);
    unsigned long long soll = p->laenge - nr * PFSC_BLOCK < PFSC_BLOCK ? p->laenge - nr * PFSC_BLOCK : PFSC_BLOCK;
    if (von < p->daten || bis < von || bis > p->gr || bis - von == 0 || bis - von > PFSC_BLOCK) return false;
    size_t n = (size_t)(bis - von);
    if (!p->lies(p->c, p->ab + von, n, p->roh)) return false;
    /* Codex-Prüfung 130: ungepackt ist ein Block genau dann, wenn er so lang ist wie sein Inhalt - der wird nie probeweise entpackt (auch
     * wenn seine Bytes zufällig ein gültiger zlib-Strom sind); jeder andere muss ein zlib-Strom sein, der genau den Inhalt ergibt */
    if (n == soll) memcpy(p->puffer, p->roh, n);
    else if (kern_entpacken(p->roh, n, p->puffer, (size_t)soll, true) != (long)soll) return false;
    p->im_puffer = (long long)nr;
    return true;
}
/* n Byte ab Stelle ab der entpackten Datei; false = außerhalb oder nicht lesbar */
bool kern_pfsc_lies(kern_pfsc *p, unsigned long long ab, size_t n, unsigned char *ziel)
{
    if (!p->lies || ab > p->laenge || n > p->laenge - ab) return false;
    while (n > 0) {
        unsigned long long nr = ab / PFSC_BLOCK, in = ab % PFSC_BLOCK;
        if (!pfsc_block(p, nr)) return false;
        size_t k = (size_t)(PFSC_BLOCK - in) < n ? (size_t)(PFSC_BLOCK - in) : n;
        memcpy(ziel, p->puffer + in, k);
        ziel += k; ab += k; n -= k;
    }
    return true;
}
/* Für die Oberfläche und die Tests: n Byte ab Stelle ab der gepackten Datei, deren Behälter in der Datei datei bei cab liegt (Länge
 * cgr) - direkt aus der Datei, nur lesen. Speicher mit malloc (der Aufrufer gibt ihn frei); NULL = nicht lesbar. */
static bool datei_lies(void *c, unsigned long long ab, size_t n, unsigned char *ziel)
{
    FILE *f = c;
    if (ab > (unsigned long long)LONG_MAX) return false;
    return fseek(f, (long)ab, SEEK_SET) == 0 && fread(ziel, 1, n, f) == n;
}
unsigned char *kern_pfsc_datei_stueck(const char *datei, unsigned long long cab, unsigned long long cgr, unsigned long long ab, size_t n)
{
    if (!datei || n == 0 || n > 4u * 1024 * 1024) return NULL;
    FILE *f = fopen(datei, "rb");
    if (!f) return NULL;
    arena *a = arena_neu();
    kern_pfsc p;
    unsigned char *d = malloc(n);
    if (d && !(kern_pfsc_auf(&p, a, datei_lies, f, cab, cgr) && kern_pfsc_lies(&p, ab, n, d))) { free(d); d = NULL; }
    arena_frei(a);
    fclose(f);
    return d;
}
/* NEU 1.0.9 (#149): ein Bild nach BILD_ORT (kern_bild_ort_lesen) aus der Datei datei lesen - am Stück, in Stücken (teile) und/oder im
 * gepackten Abbild (pfsc), die Datei einmal geöffnet, nur lesen. Für die Oberfläche (app.c spiel_bild) und den Test. Speicher mit malloc
 * (der Aufrufer gibt ihn frei); NULL = nicht lesbar. */
unsigned char *kern_bild_lesen(const char *datei, const kern_bild_ort *b)
{
    if (!datei || !b || b->len < 8 || b->len > 4u * 1024 * 1024 || b->teile_n < 0 || b->teile_n > KERN_BILD_TEILE) return NULL;
    FILE *f = fopen(datei, "rb");
    if (!f) return NULL;
    arena *a = arena_neu();
    kern_pfsc p;
    unsigned char *d = malloc((size_t)b->len);
    bool ok = d && (!b->gepackt || kern_pfsc_auf(&p, a, datei_lies, f, b->cab, b->cgr));
    size_t got = 0;
    for (int i = 0, n = b->teile_n ? b->teile_n : 1; ok && i < n; i++) {
        unsigned long long ab = b->teile_n ? b->teile[i][0] : b->ab, l = b->teile_n ? b->teile[i][1] : b->len;
        ok = l <= b->len - got && (b->gepackt ? kern_pfsc_lies(&p, ab, (size_t)l, d + got) : datei_lies(f, ab, (size_t)l, d + got));
        got += (size_t)l;
    }
    ok = ok && got == b->len;
    arena_frei(a);
    fclose(f);
    if (!ok) { free(d); return NULL; }
    return d;
}
