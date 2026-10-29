/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * hilfen.c - Klasse H aus Sitzung.cs (1.0.5) und direkte Regex-Aufrufe,
 * ausgewertet mit PCRE2.
 */
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hilfen.h"

/* ------------------------------------------------------------ Muster-Speicher */
#define OPT_H    (PCRE2_CASELESS | PCRE2_UTF | PCRE2_UCP)
#define OPT_LIKE (PCRE2_CASELESS | PCRE2_UTF | PCRE2_UCP | PCRE2_DOTALL)
#define OPT_ROH  (PCRE2_UTF | PCRE2_UCP)

typedef struct muster { struct muster *nach; uint32_t opt; char *text; pcre2_code *code; } muster;
#define FAECHER 1024
static muster *fach[FAECHER];
static pthread_mutex_t sperre = PTHREAD_MUTEX_INITIALIZER;
static char musterfehler[512];
const char *h_musterfehler(void) { return musterfehler; }

static unsigned streu(const char *s, uint32_t opt)
{
    unsigned h = 2166136261u ^ opt;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h % FAECHER;
}

static pcre2_code *hole(const char *text, uint32_t opt)
{
    unsigned i = streu(text, opt);
    pthread_mutex_lock(&sperre);
    for (muster *m = fach[i]; m; m = m->nach)
        if (m->opt == opt && strcmp(m->text, text) == 0) { pthread_mutex_unlock(&sperre); return m->code; }
    int fe; PCRE2_SIZE fo;
    pcre2_code *c = pcre2_compile((PCRE2_SPTR)text, PCRE2_ZERO_TERMINATED, opt, &fe, &fo, NULL);
    if (!c) {
        PCRE2_UCHAR b[256]; pcre2_get_error_message(fe, b, sizeof b);
        snprintf(musterfehler, sizeof musterfehler, "Muster '%s' bei %d: %s", text, (int)fo, (char *)b);
        pthread_mutex_unlock(&sperre);
        wirf(AUSN_FEHLER, musterfehler);             /* wie ArgumentException in .NET */
    }
    muster *m = calloc(1, sizeof *m);
    m->opt = opt; m->text = strdup(text); m->code = c; m->nach = fach[i]; fach[i] = m;
    pthread_mutex_unlock(&sperre);
    return c;
}

/* ------------------------------------------------------------ .NET-Text */
static bool gueltig_utf8(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        const unsigned char *v = p;
        uint32_t c = utf8_naechstes(&p);
        if (c == 0xFFFD && !(p - v == 3 && v[0] == 0xEF && v[1] == 0xBF && v[2] == 0xBD)) return false;
    }
    return true;
}
/* malloc'te Kopie mit U+FFFD statt ungültiger Bytes, oder NULL wenn alles gültig ist */
static char *net_kopie(const char *s)
{
    if (gueltig_utf8(s)) return NULL;
    size_t n = strlen(s);
    char *d = malloc(n * 3 + 1), *o = d;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        const unsigned char *v = p;
        uint32_t c = utf8_naechstes(&p);
        if (c == 0xFFFD) { *o++ = (char)0xEF; *o++ = (char)0xBF; *o++ = (char)0xBD; }
        else { memcpy(o, v, (size_t)(p - v)); o += p - v; }
    }
    *o = 0;
    return d;
}
const char *h_net_text(arena *a, const char *s)
{
    char *k = net_kopie(s ? s : "");
    if (!k) return s ? s : "";
    const char *r = a_kopie(a, k); free(k); return r;
}

/* ------------------------------------------------------------ Grundfunktionen */
static bool passt(const char *s, const char *text, uint32_t opt)
{
    pcre2_code *c = hole(text, opt);
    char *k = net_kopie(s ? s : "");
    const char *t = k ? k : (s ? s : "");
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(c, NULL);
    int rc = pcre2_match(c, (PCRE2_SPTR)t, strlen(t), 0, 0, md, NULL);
    pcre2_match_data_free(md);
    free(k);
    return rc >= 0;
}

static htreffer treffer(arena *a, const char *s, const char *text, uint32_t opt)
{
    htreffer r = { false, "", 0, NULL };
    pcre2_code *c = hole(text, opt);
    uint32_t ngr = 0; pcre2_pattern_info(c, PCRE2_INFO_CAPTURECOUNT, &ngr);
    r.ngr = (int)ngr + 1;
    r.gr = a_hole(a, sizeof(char *) * (size_t)r.ngr);
    for (int i = 0; i < r.ngr; i++) r.gr[i] = "";
    char *k = net_kopie(s ? s : "");
    const char *t = k ? k : (s ? s : "");
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(c, NULL);
    int rc = pcre2_match(c, (PCRE2_SPTR)t, strlen(t), 0, 0, md, NULL);
    if (rc >= 0) {
        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
        r.ok = true;
        for (int i = 0; i < r.ngr && i < rc; i++)
            if (ov[2 * i] != PCRE2_UNSET) r.gr[i] = a_kopie_n(a, t + ov[2 * i], ov[2 * i + 1] - ov[2 * i]);
        r.wert = r.gr[0];
    }
    pcre2_match_data_free(md);
    free(k);
    return r;
}

/* .NET-Ersetzung: $1..$99, ${n}, $$, $0/$& (mehr wird im Kern nicht benutzt) */
static const char *ersetze(arena *a, const char *s, const char *text, const char *ersatz, uint32_t opt)
{
    pcre2_code *c = hole(text, opt);
    char *k = net_kopie(s ? s : "");
    const char *t = k ? k : (s ? s : "");
    size_t n = strlen(t);
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(c, NULL);
    size_t cap = n + 64, len = 0; char *aus = malloc(cap);
#define DAZU(p, l) do { size_t _l = (l); while (len + _l + 1 > cap) { cap *= 2; aus = realloc(aus, cap); } memcpy(aus + len, (p), _l); len += _l; } while (0)
    size_t pos = 0, letzt = 0;
    while (pos <= n) {
        int rc = pcre2_match(c, (PCRE2_SPTR)t, n, pos, 0, md, NULL);
        if (rc < 0) break;
        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
        DAZU(t + letzt, ov[0] - letzt);
        for (const char *e = ersatz; *e; e++) {
            if (*e == '$' && e[1] == '$') { DAZU("$", 1); e++; continue; }
            if (*e == '$' && (e[1] == '0' || e[1] == '&')) { DAZU(t + ov[0], ov[1] - ov[0]); e++; continue; }
            if (*e == '$' && e[1] >= '1' && e[1] <= '9') {
                int g = e[1] - '0'; const char *z = e + 2;
                if (*z >= '0' && *z <= '9' && g * 10 + (*z - '0') < rc) { g = g * 10 + (*z - '0'); z++; }
                if (g < rc && ov[2 * g] != PCRE2_UNSET) DAZU(t + ov[2 * g], ov[2 * g + 1] - ov[2 * g]);
                e = z - 1; continue;
            }
            if (*e == '$' && e[1] == '{') {
                const char *z = strchr(e, '}');
                if (z) { int g = atoi(e + 2); if (g < rc && ov[2 * g] != PCRE2_UNSET) DAZU(t + ov[2 * g], ov[2 * g + 1] - ov[2 * g]); e = z; continue; }
            }
            DAZU(e, 1);
        }
        letzt = ov[1];
        if (ov[1] == ov[0]) {                        /* leerer Treffer: ein Zeichen weiter */
            if (ov[1] >= n) { pos = n + 1; break; }
            const unsigned char *q = (const unsigned char *)t + ov[1]; utf8_naechstes(&q);
            DAZU(t + ov[1], (size_t)((const char *)q - (t + ov[1])));
            letzt = pos = (size_t)((const char *)q - t);
        } else pos = ov[1];
    }
    if (letzt < n) DAZU(t + letzt, n - letzt);
    aus[len] = 0;
#undef DAZU
    const char *r = a_kopie(a, aus);
    free(aus); free(k); pcre2_match_data_free(md);
    return r;
}

static sliste alle(arena *a, const char *s, const char *text, uint32_t opt, bool teile)
{
    sliste l = {0};
    pcre2_code *c = hole(text, opt);
    char *k = net_kopie(s ? s : "");
    const char *t = k ? k : (s ? s : "");
    size_t n = strlen(t), pos = 0, letzt = 0;
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(c, NULL);
    while (pos <= n) {
        int rc = pcre2_match(c, (PCRE2_SPTR)t, n, pos, 0, md, NULL);
        if (rc < 0) break;
        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
        if (teile) { sl_add(a, &l, a_kopie_n(a, t + letzt, ov[0] - letzt)); letzt = ov[1]; }
        else sl_add(a, &l, a_kopie_n(a, t + ov[0], ov[1] - ov[0]));
        if (ov[1] == ov[0]) {
            if (ov[1] >= n) break;
            const unsigned char *q = (const unsigned char *)t + ov[1]; utf8_naechstes(&q);
            pos = (size_t)((const char *)q - t);
        } else pos = ov[1];
    }
    if (teile) sl_add(a, &l, a_kopie_n(a, t + letzt, n - letzt));
    pcre2_match_data_free(md); free(k);
    return l;
}

/* ------------------------------------------------------------ Klasse H */
bool h_m(const char *s, const char *m) { return passt(s, m, OPT_H); }
htreffer h_mt(arena *a, const char *s, const char *m) { return treffer(a, s, m, OPT_H); }
const char *h_r(arena *a, const char *s, const char *m, const char *e) { return ersetze(a, s, m, e, OPT_H); }

const char *h_escape(arena *a, const char *s)
{
    size_t n = strlen(s ? s : "");
    char *d = a_hole(a, n * 2 + 1), *o = d;
    for (const char *p = s ? s : ""; *p; p++) {
        switch (*p) {
        case '\\': case '*': case '+': case '?': case '|': case '{': case '[':
        case '(': case ')': case '^': case '$': case '.': case '#': case ' ':
            *o++ = '\\'; *o++ = *p; break;
        case '\t': *o++ = '\\'; *o++ = 't'; break;
        case '\n': *o++ = '\\'; *o++ = 'n'; break;
        case '\r': *o++ = '\\'; *o++ = 'r'; break;
        case '\f': *o++ = '\\'; *o++ = 'f'; break;
        default: *o++ = *p;
        }
    }
    *o = 0;
    return d;
}

bool h_like(const char *s, const char *wild)
{
    arena *a = arena_neu();
    const char *e = h_escape(a, wild);
    /* "^" + Escape(wild).Replace("\\*", ".*").Replace("\\?", ".") + "$" */
    size_t n = strlen(e);
    char *re = a_hole(a, n * 2 + 3), *o = re;
    *o++ = '^';
    for (const char *p = e; *p; p++) {
        if (p[0] == '\\' && p[1] == '*') { *o++ = '.'; *o++ = '*'; p++; }
        else if (p[0] == '\\' && p[1] == '?') { *o++ = '.'; p++; }
        else if (p[0] == '\\' && p[1]) { *o++ = p[0]; *o++ = p[1]; p++; }
        else *o++ = *p;
    }
    *o++ = '$'; *o = 0;
    char kopie[4096];
    snprintf(kopie, sizeof kopie, "%s", re);
    arena_frei(a);
    return passt(s, kopie, OPT_LIKE);
}

bool h_hat(const sliste *l, const char *s)
{
    if (!l) return false;
    for (int i = 0; i < l->n; i++) if (txt_gleich_ci(l->e[i], s)) return true;
    return false;
}
bool h_gleich(const char *a, const char *b) { return txt_gleich_ci(a, b); }
bool h_steuerzeichen(const char *s)
{
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) if (*p < 0x20 || *p == 0x7F) return true;
    return false;
}
const char *h_eltern(arena *a, const char *p)
{
    int i = txt_letzter(p, '/');
    if (i < 0) wirf(AUSN_FEHLER, "Die Länge darf nicht kleiner als 0 sein.");   /* Substring(0, -1) */
    if (i == 0) return "/";
    return a_kopie_n(a, p, (size_t)i);
}
const char *h_blatt(arena *a, const char *p) { int i = txt_letzter(p, '/'); return a_kopie(a, p + i + 1); }

/* ------------------------------------------------------------ direkte Regex-Aufrufe */
bool r_m(const char *s, const char *m) { return passt(s, m, OPT_ROH); }
int r_anzahl(const char *s, const char *m) { arena *a = arena_neu(); int n = alle(a, s, m, OPT_ROH, false).n; arena_frei(a); return n; }
sliste r_alle(arena *a, const char *s, const char *m) { return alle(a, s, m, OPT_ROH, false); }
const char *r_r(arena *a, const char *s, const char *m, const char *e) { return ersetze(a, s, m, e, OPT_ROH); }
sliste r_split(arena *a, const char *s, const char *m) { return alle(a, s, m, OPT_ROH, true); }

/* ------------------------------------------------------------ Trim */
static bool weiss(uint32_t c)
{
    return c == ' ' || (c >= 0x09 && c <= 0x0D) || c == 0x85 || c == 0xA0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}
const char *h_trim(arena *a, const char *s)
{
    const char *t = h_net_text(a, s);
    const unsigned char *p = (const unsigned char *)t, *anf = NULL, *ende = p;
    while (*p) {
        const unsigned char *v = p;
        uint32_t c = utf8_naechstes(&p);
        if (!weiss(c)) { if (!anf) anf = v; ende = p; }
    }
    if (!anf) return "";
    return a_kopie_n(a, (const char *)anf, (size_t)(ende - anf));
}
const char *h_ohne_bom(const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) p += 3;
    return (const char *)p;
}
