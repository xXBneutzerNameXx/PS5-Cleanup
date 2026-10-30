/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * basis.c - Speicher, Texte, Listen, Wörterbuch, Ausnahmen.
 */
#include "basis.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ Arena */
typedef struct block { struct block *vor; size_t frei, groesse; unsigned char daten[]; } block;
struct arena { block *b; };

/* NEU 1.0.6.1: Speicherzähler (alle Fäden) und Grund vor abort() - siehe basis.h */
static size_t speicher_belegt = 0, speicher_spitze = 0;
size_t basis_speicher_belegt(void) { return __atomic_load_n(&speicher_belegt, __ATOMIC_RELAXED); }
size_t basis_speicher_spitze(void) { return __atomic_load_n(&speicher_spitze, __ATOMIC_RELAXED); }
void (*basis_kein_speicher)(size_t n) = NULL;
const char *volatile basis_grund = NULL;
const char *volatile basis_schritt = "Start";
volatile int basis_zaehlt = 0;
static void speicher_plus(size_t n)
{
    size_t v = __atomic_add_fetch(&speicher_belegt, n, __ATOMIC_RELAXED);
    size_t s = __atomic_load_n(&speicher_spitze, __ATOMIC_RELAXED);
    while (v > s && !__atomic_compare_exchange_n(&speicher_spitze, &s, v, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) { }
}
int basis_faden(pthread_t *t, void *(*f)(void *), void *x)
{
    pthread_attr_t at;
    if (pthread_attr_init(&at)) return pthread_create(t, NULL, f, x);
    size_t g = 0;
    if (!pthread_attr_getstacksize(&at, &g) && g < 1024 * 1024) pthread_attr_setstacksize(&at, 1024 * 1024);
    int r = pthread_create(t, &at, f, x);
    pthread_attr_destroy(&at);
    return r;
}
const void *basis_suche(const void *d, size_t n, const void *m, size_t ml)
{
    const unsigned char *a = d, *b = m;
    if (ml == 0) return d;
    if (n < ml) return NULL;
    for (size_t i = 0; i + ml <= n; i++) {
        const unsigned char *t = memchr(a + i, b[0], n - ml - i + 1);
        if (!t) return NULL;
        i = (size_t)(t - a);
        if (!memcmp(t, b, ml)) return t;
    }
    return NULL;
}
static void kein_speicher(size_t n)
{
    basis_grund = "kein Arbeitsspeicher mehr";
    if (basis_kein_speicher) basis_kein_speicher(n);
    abort();
}

arena *arena_neu(void) { arena *a = calloc(1, sizeof *a); if (!a) kein_speicher(sizeof *a); return a; }

void arena_frei(arena *a)
{
    if (!a) return;
    block *b = a->b;
    while (b) { block *v = b->vor; __atomic_sub_fetch(&speicher_belegt, sizeof(block) + b->groesse, __ATOMIC_RELAXED); free(b); b = v; }
    free(a);
}

void *a_hole(arena *a, size_t n)
{
    n = (n + 15) & ~(size_t)15;
    if (!a->b || a->b->frei < n) {
        size_t g = n > 65536 ? n : 65536;
        block *b = malloc(sizeof(block) + g);
        if (!b) kein_speicher(sizeof(block) + g);
        speicher_plus(sizeof(block) + g);
        b->vor = a->b; b->frei = g; b->groesse = g; a->b = b;
    }
    void *p = a->b->daten + (a->b->groesse - a->b->frei);
    a->b->frei -= n;
    memset(p, 0, n);
    return p;
}

char *a_kopie_n(arena *a, const char *s, size_t n)
{
    char *d = a_hole(a, n + 1);
    if (s && n) memcpy(d, s, n);
    d[n] = 0;
    return d;
}
char *a_kopie(arena *a, const char *s) { return a_kopie_n(a, s ? s : "", s ? strlen(s) : 0); }

char *a_fmt(arena *a, const char *fmt, ...)
{
    va_list ap, ap2;
    va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    char *d = a_hole(a, (size_t)n + 1);
    vsnprintf(d, (size_t)n + 1, fmt, ap2); va_end(ap2);
    return d;
}

char *a_verb(arena *a, ...)
{
    va_list ap; size_t n = 0; const char *s;
    va_start(ap, a); while ((s = va_arg(ap, const char *))) n += strlen(s); va_end(ap);
    char *d = a_hole(a, n + 1), *p = d;
    va_start(ap, a); while ((s = va_arg(ap, const char *))) { size_t l = strlen(s); memcpy(p, s, l); p += l; } va_end(ap);
    *p = 0;
    return d;
}

/* ------------------------------------------------------------------ Listen */
static void *wachse(arena *a, void *alt, int n, int *cap, size_t el)
{
    if (n < *cap) return alt;
    int neu = *cap ? *cap * 2 : 8;
    void *p = a_hole(a, (size_t)neu * el);
    if (alt && n) memcpy(p, alt, (size_t)n * el);
    *cap = neu;
    return p;
}
void sl_add(arena *a, sliste *l, const char *s) { l->e = wachse(a, l->e, l->n, &l->cap, sizeof(char *)); l->e[l->n++] = s; }
void sl_add_alle(arena *a, sliste *l, const sliste *von) { for (int i = 0; i < von->n; i++) sl_add(a, l, von->e[i]); }
sliste sl_kopie(arena *a, const sliste *l) { sliste k = {0}; sl_add_alle(a, &k, l); return k; }
const char *sl_join(arena *a, const sliste *l, const char *t)
{
    size_t n = 0, lt = strlen(t);
    for (int i = 0; i < l->n; i++) n += strlen(l->e[i]) + (i ? lt : 0);
    char *d = a_hole(a, n + 1), *p = d;
    for (int i = 0; i < l->n; i++) {
        if (i) { memcpy(p, t, lt); p += lt; }
        size_t x = strlen(l->e[i]); memcpy(p, l->e[i], x); p += x;
    }
    *p = 0;
    return d;
}
void pl_add(arena *a, pliste *l, void *p) { l->e = wachse(a, l->e, l->n, &l->cap, sizeof(void *)); l->e[l->n++] = p; }

/* ------------------------------------------------------------------ Wörterbuch */
mtab mt_neu(bool ci) { mtab t = {0}; t.ci = ci; return t; }
int mt_index(const mtab *t, const char *k)
{
    for (int i = 0; i < t->n; i++)
        if (t->ci ? txt_gleich_ci(t->e[i].k, k) : txt_gleich(t->e[i].k, k)) return i;
    return -1;
}
bool  mt_hat(const mtab *t, const char *k) { return mt_index(t, k) >= 0; }
void *mt_hole(const mtab *t, const char *k) { int i = mt_index(t, k); return i < 0 ? NULL : t->e[i].v; }
void mt_setze(arena *a, mtab *t, const char *k, void *v)
{
    int i = mt_index(t, k);
    if (i >= 0) { t->e[i].v = v; return; }       /* .NET: vorhandener Schlüssel behält seinen Platz */
    t->e = wachse(a, t->e, t->n, &t->cap, sizeof(mpaar));
    t->e[t->n].k = a_kopie(a, k); t->e[t->n].v = v; t->n++;
}

/* ------------------------------------------------------------------ Ausnahmen */
_Thread_local fangpunkt *ausn_oben;
_Thread_local ausn_t ausn_art;
_Thread_local char ausn_text[2048];

void wirf(ausn_t art, const char *text)
{
    ausn_art = art;
    snprintf(ausn_text, sizeof ausn_text, "%s", text ? text : "");
    if (!ausn_oben) { fprintf(stderr, "UNBEHANDELTE AUSNAHME: %s\n", ausn_text); basis_grund = "unbehandelte Ausnahme"; abort(); }
    longjmp(ausn_oben->j, 1);
}
void weiterwerfen(void)
{
    if (!ausn_oben) { fprintf(stderr, "UNBEHANDELTE AUSNAHME: %s\n", ausn_text); basis_grund = "unbehandelte Ausnahme"; abort(); }
    longjmp(ausn_oben->j, 1);
}

bool SP_EN = false;

/* ------------------------------------------------------------------ Texte */
bool txt_gleich(const char *a, const char *b) { return strcmp(a ? a : "", b ? b : "") == 0; }
bool txt_beginnt(const char *s, const char *x) { return strncmp(s, x, strlen(x)) == 0; }
bool txt_endet(const char *s, const char *x) { size_t a = strlen(s), b = strlen(x); return a >= b && memcmp(s + a - b, x, b) == 0; }
bool txt_enthaelt(const char *s, const char *t) { return strstr(s, t) != NULL; }
int  txt_index(const char *s, const char *t) { const char *p = strstr(s, t); return p ? (int)(p - s) : -1; }
int  txt_letzter(const char *s, char c) { const char *p = strrchr(s, c); return p ? (int)(p - s) : -1; }

uint32_t utf8_naechstes(const unsigned char **pp)
{
    const unsigned char *p = *pp;
    uint32_t c = p[0];
    if (c < 0x80) { *pp = p + 1; return c; }
    int n; uint32_t min;
    if ((c & 0xE0) == 0xC0) { n = 1; c &= 0x1F; min = 0x80; }
    else if ((c & 0xF0) == 0xE0) { n = 2; c &= 0x0F; min = 0x800; }
    else if ((c & 0xF8) == 0xF0) { n = 3; c &= 0x07; min = 0x10000; }
    else { *pp = p + 1; return 0xFFFD; }
    for (int i = 1; i <= n; i++) {
        if ((p[i] & 0xC0) != 0x80) { *pp = p + i; return 0xFFFD; }
        c = (c << 6) | (p[i] & 0x3F);
    }
    *pp = p + n + 1;
    if (c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return 0xFFFD;
    return c;
}

uint32_t zeichen_gross(uint32_t c)
{
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c < 0x80) return c;
    if (c == 0xB5) return 0x39C;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
    if (c == 0xFF) return 0x178;
    if (c >= 0x100 && c <= 0x137) { if (c == 0x131) return 'I'; return (c & 1) ? c - 1 : c; }
    if (c >= 0x139 && c <= 0x148) return (c & 1) ? c : c - 1;
    if (c >= 0x14A && c <= 0x177) return (c & 1) ? c - 1 : c;
    if (c >= 0x17A && c <= 0x17E) return (c & 1) ? c : c - 1;
    if (c == 0x17F) return 'S';
    if (c >= 0x3B1 && c <= 0x3C1) return c - 32;
    if (c == 0x3C2) return 0x3A3;
    if (c >= 0x3C3 && c <= 0x3CB) return c - 32;
    if (c == 0x3AC) return 0x386;
    if (c >= 0x3AD && c <= 0x3AF) return c - 37;
    if (c == 0x3CC) return 0x38C;
    if (c == 0x3CD) return 0x38E;
    if (c == 0x3CE) return 0x38F;
    if (c >= 0x430 && c <= 0x44F) return c - 32;
    if (c >= 0x450 && c <= 0x45F) return c - 80;
    return c;
}

bool txt_gleich_ci(const char *a, const char *b)
{
    const unsigned char *p = (const unsigned char *)(a ? a : ""), *q = (const unsigned char *)(b ? b : "");
    while (*p && *q) {
        if (zeichen_gross(utf8_naechstes(&p)) != zeichen_gross(utf8_naechstes(&q))) return false;
    }
    return *p == 0 && *q == 0;
}

static int utf8_schreibe(char *d, uint32_t c)
{
    if (c < 0x80) { d[0] = (char)c; return 1; }
    if (c < 0x800) { d[0] = (char)(0xC0 | (c >> 6)); d[1] = (char)(0x80 | (c & 0x3F)); return 2; }
    if (c < 0x10000) { d[0] = (char)(0xE0 | (c >> 12)); d[1] = (char)(0x80 | ((c >> 6) & 0x3F)); d[2] = (char)(0x80 | (c & 0x3F)); return 3; }
    d[0] = (char)(0xF0 | (c >> 18)); d[1] = (char)(0x80 | ((c >> 12) & 0x3F)); d[2] = (char)(0x80 | ((c >> 6) & 0x3F)); d[3] = (char)(0x80 | (c & 0x3F)); return 4;
}
char *a_gross(arena *a, const char *s)
{
    size_t n = strlen(s ? s : "");
    char *d = a_hole(a, n * 3 + 4), *o = d;            /* U+FFFD braucht 3 Bytes für 1 Eingabebyte */
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) o += utf8_schreibe(o, zeichen_gross(utf8_naechstes(&p)));
    *o = 0;
    return d;
}
