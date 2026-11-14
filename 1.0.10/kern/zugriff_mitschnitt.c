/* PS5 Cleanup - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * zugriff_mitschnitt.c - NEU 1.0.8 (Arbeitsplan 2.7, Absprache da_Chris86 05.11.2026): Mitschnitt für den Abschlusstest an der PS5.
 *  - zugriff_mitschnitt: legt sich um eine andere Zugriffsschicht und schreibt jede Frage samt Antwort als Textzeile mit (Ordnerlisten,
 *    gelesene Dateien, Befehle samt Antwort von Sony, vor und nach dem Entfernen einer Kachel der Zustand ihrer Ordner). Verändert selbst
 *    nichts. Nur die Test-Fassung (MITSCHNITT) benutzt ihn - die veröffentlichte Version nicht.
 *  - zugriff_wiedergabe: liest einen Mitschnitt und beantwortet dieselben Fragen genauso. Damit läuft die Suche am PC mit den echten Daten
 *    der PS5 (PC-Nachbau). Verändert nie etwas: alle verändernden Befehle bekommen "550 Wiedergabe".
 * Format: eine Zeile je Frage, Felder durch ein Leerzeichen getrennt. Texte, Pfade und Namen mit %XX für Leer- und Steuerzeichen und %
 * ("%-" = leer), Daten base64 ("-" = leer):
 *   C <code> <text> <pfad>                       Ordner da und lesbar?
 *   L <code> <text> <pfad> <n>                   Ordnerliste, danach n Zeilen ". <d|f><l|-> <größe> <name>"
 *   R <code> <text> <pfad> <n> <daten|*>         ganze Datei (* = größer als 1 MB, nicht mitgeschrieben)
 *   K <code> <text> <pfad> <daten>               Anfang einer Datei
 *   A <code> <text> <pfad> <ab> <n> <daten|*>    n Bytes ab Stelle (* = mehr als 1 MB, nicht mitgeschrieben)
 *   E <code> <text> <pfad> <0|1> <muster>        enthält die Datei das Muster?
 *   S <code> <text> <pfad> <bytes> <n>           Datei stückweise gelesen, danach n Zeilen ". <daten>": nur die Wege (/data/..., /user/data/...)
 *                                                und Punkt-Namen, nach denen die Suche im Programm einer Kachel sucht (kern_suche.c beleg_bereich)
 *   W <code> <text> <pfad> <n>                   Datei geschrieben (nur die Größe)
 *   B <code> <text> <befehl> <pfad>              Befehl (löschen, Sony-Deinstallation, ...) samt Antwort
 *   I <text>  P <text>  D <titel> <feld> <wert>  Hinweise, Zeilen des Protokolls, Zeilen aus appinfo.db (schreibt die Oberfläche).
 *   "I SUCHE <n>" beginnt die n-te Suche - die Wiedergabe kann eine davon auswählen.
 */
#include "zugriff.h"
#ifdef MITSCHNITT   /* Codex-Prüfung 74: nur die Test-Fassung (und die PC-Tests) enthalten den Mitschnitt - app/bauen.sh prüft das */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MS_DATEI_MAX (1024 * 1024)      /* ganze Dateien bis 1 MB mitschreiben */
#define MS_STUECK_MAX (1024 * 1024)     /* Stücke (lies_ab) bis 1 MB mitschreiben */
#define MS_WEG_MAX 1000                 /* wie BELEG_WEG_MAX in kern_suche.c: so lang darf ein Weg hinter /data/ sein (Codex-Prüfung 74) */
#define MS_WEGE_MAX 20000               /* höchstens so viele Wege/Punkt-Namen je Datei */

void (*zugriff_mitschnitt_zustand)(const char *id, bool nachher, void (*zeile)(const char *t)) = NULL;

/* ------------------------------------------------------------------ gemeinsame Helfer */
static const char *norm(arena *a, const char *p)   /* wie zugriff_sim.c: ".", "..", doppelte "/" auflösen */
{
    size_t n = strlen(p);
    const char **teil = a_hole(a, sizeof(char *) * (n + 1));
    int k = 0; char *kopie = a_kopie(a, p), *sp = NULL;
    for (char *s = strtok_r(kopie, "/", &sp); s; s = strtok_r(NULL, "/", &sp)) {
        if (strcmp(s, ".") == 0) continue;
        if (strcmp(s, "..") == 0) { if (k) k--; continue; }
        teil[k++] = s;
    }
    if (!k) return "/";
    size_t l = 1; for (int i = 0; i < k; i++) l += strlen(teil[i]) + 1;
    char *d = a_hole(a, l + 1), *o = d;
    for (int i = 0; i < k; i++) { *o++ = '/'; size_t x = strlen(teil[i]); memcpy(o, teil[i], x); o += x; }
    *o = 0;
    return d;
}
static const char *pct(arena *a, const char *s)
{
    if (!s || !*s) return "%-";
    size_t n = strlen(s); char *o = a_hole(a, 3 * n + 1), *p = o;
    for (const unsigned char *q = (const unsigned char *)s; *q; q++) {
        if (*q <= 0x20 || *q == '%' || *q == 0x7F) { *p++ = '%'; *p++ = "0123456789ABCDEF"[*q >> 4]; *p++ = "0123456789ABCDEF"[*q & 15]; }
        else *p++ = (char)*q;
    }
    *p = 0;
    return o;
}
static int hexwert(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
static const char *unpct(arena *a, const char *s)
{
    if (!strcmp(s, "%-")) return "";
    size_t n = strlen(s); char *o = a_hole(a, n + 1), *p = o;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '%' && i + 2 < n + 1 && hexwert(s[i + 1]) >= 0 && hexwert(s[i + 2]) >= 0) { *p++ = (char)(hexwert(s[i + 1]) * 16 + hexwert(s[i + 2])); i += 2; }
        else *p++ = s[i];
    }
    *p = 0;
    return o;
}
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char *b64(arena *a, const unsigned char *d, size_t n)
{
    if (!d || !n) return "-";
    char *o = a_hole(a, (n + 2) / 3 * 4 + 1), *p = o;
    for (size_t i = 0; i < n; i += 3) {
        unsigned v = (unsigned)d[i] << 16 | (i + 1 < n ? (unsigned)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        *p++ = B64[v >> 18 & 63]; *p++ = B64[v >> 12 & 63];
        *p++ = i + 1 < n ? B64[v >> 6 & 63] : '='; *p++ = i + 2 < n ? B64[v & 63] : '=';
    }
    *p = 0;
    return o;
}
static int b64wert(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    return c == '+' ? 62 : c == '/' ? 63 : -1;
}
static unsigned char *unb64(arena *a, const char *s, size_t *n)
{
    *n = 0;
    if (!strcmp(s, "-")) return a_hole(a, 1);
    size_t l = strlen(s);
    unsigned char *o = a_hole(a, l / 4 * 3 + 3);
    unsigned v = 0; int bits = 0;
    for (size_t i = 0; i < l; i++) {
        int w = b64wert(s[i]);
        if (w < 0) continue;                                   /* "=" und Fremdes */
        v = v << 6 | (unsigned)w; bits += 6;
        if (bits >= 8) { bits -= 8; o[(*n)++] = (unsigned char)(v >> bits & 0xFF); }
    }
    return o;
}
static const char *zahl(arena *a, long long v) { return a_fmt(a, "%lld", v); }

/* ------------------------------------------------------------------ Mitschnitt */
typedef struct {
    zugriff *innen;
    void (*schreib)(void *c, const char *zeile);
    void *c;
    pthread_mutex_t m;
} mz;
static void raus(mz *s, const char *zeilen) { pthread_mutex_lock(&s->m); s->schreib(s->c, zeilen); pthread_mutex_unlock(&s->m); }
/* Codex-Prüfung 92: die Texte des Mitschnitts (base64, Listen) entstehen nur in eigenem, kurzlebigem Speicher (t) - nie im Speicher des
 * Aufrufers, der bei der Suche bis zum Ende bleibt (3000 PKG-Dateien mit param.sfo hätten sonst Hunderte MB belegt). */
#define MIT_T(...) do { arena *t = arena_neu(); __VA_ARGS__; arena_frei(t); } while (0)
static const char *kopf_text(arena *a, char typ, antwort r, const char *pfad)
{
    char t[2] = { typ, 0 };
    return VERB(a, t, " ", zahl(a, r.code), " ", pct(a, r.text), " ", pct(a, pfad));
}
static antwort m_cwd(zugriff *z, arena *a, const char *pfad)
{
    mz *s = z->zustand;
    antwort r = s->innen->cwd(s->innen, a, pfad);
    MIT_T(raus(s, VERB(t, kopf_text(t, 'C', r, norm(t, pfad)), "\n")));
    return r;
}
static const char *liste_text(arena *a, antwort r, const char *pfad, const kdliste *l)
{
    int n = r.code < 400 && l ? l->n : 0;
    const char *k = kopf_text(a, 'L', r, pfad);
    const char **nm = a_hole(a, sizeof(char *) * (size_t)(n + 1));
    size_t g = strlen(k) + 32;
    for (int i = 0; i < n; i++) { nm[i] = pct(a, l->e[i].name); g += strlen(nm[i]) + 40; }
    char *o = a_hole(a, g + 1), *p = o;
    p += snprintf(p, g + 1 - (size_t)(p - o), "%s %d\n", k, n);
    for (int i = 0; i < n; i++)
        p += snprintf(p, g + 1 - (size_t)(p - o), ". %c%c %lld %s\n", l->e[i].ordner ? 'd' : 'f', l->e[i].link ? 'l' : '-', l->e[i].groesse, nm[i]);
    return o;
}
static antwort m_liste(zugriff *z, arena *a, const char *pfad, kdliste *aus)
{
    mz *s = z->zustand;
    antwort r = s->innen->liste(s->innen, a, pfad, aus);
    MIT_T(raus(s, liste_text(t, r, norm(t, pfad), aus)));
    return r;
}
static antwort m_lies(zugriff *z, arena *a, const char *pfad, const unsigned char **d, size_t *n)
{
    mz *s = z->zustand;
    antwort r = s->innen->lies(s->innen, a, pfad, d, n);
    size_t m = r.code < 400 ? *n : 0;
    MIT_T(raus(s, VERB(t, kopf_text(t, 'R', r, norm(t, pfad)), " ", zahl(t, (long long)m), " ", m > MS_DATEI_MAX ? "*" : b64(t, *d, m), "\n")));
    return r;
}
static antwort m_kopf(zugriff *z, arena *a, const char *pfad, size_t n, const unsigned char **d, size_t *len)
{
    mz *s = z->zustand;
    antwort r = s->innen->kopf(s->innen, a, pfad, n, d, len);
    MIT_T(raus(s, VERB(t, kopf_text(t, 'K', r, norm(t, pfad)), " ", r.code < 400 ? b64(t, *d, *len) : "-", "\n")));
    return r;
}
static antwort m_enthaelt(zugriff *z, arena *a, const char *pfad, const void *mu, size_t ml, bool *ja)
{
    mz *s = z->zustand;
    antwort r = s->innen->enthaelt(s->innen, a, pfad, mu, ml, ja);
    MIT_T(raus(s, VERB(t, kopf_text(t, 'E', r, norm(t, pfad)), " ", r.code < 400 && *ja ? "1" : "0", " ", b64(t, mu, ml), "\n")));
    return r;
}
static antwort m_lies_ab(zugriff *z, arena *a, const char *pfad, long long ab, size_t n, unsigned char *ziel, size_t *len)
{
    mz *s = z->zustand;
    antwort r = s->innen->lies_ab(s->innen, a, pfad, ab, n, ziel, len);
    size_t m = r.code < 400 ? *len : 0;
    MIT_T(raus(s, VERB(t, kopf_text(t, 'A', r, norm(t, pfad)), " ", zahl(t, ab), " ", zahl(t, (long long)n), " ", m > MS_STUECK_MAX ? "*" : b64(t, ziel, m), "\n")));
    return r;
}
static antwort m_schreibe(zugriff *z, arena *a, const char *pfad, const void *d, size_t n)
{
    mz *s = z->zustand;
    antwort r = s->innen->schreibe(s->innen, a, pfad, d, n);
    MIT_T(raus(s, VERB(t, kopf_text(t, 'W', r, norm(t, pfad)), " ", zahl(t, (long long)n), "\n")));
    return r;
}
/* stückweise gelesen: die Stücke gehen unverändert weiter; mitgeschrieben werden nur die Wege und Punkt-Namen nach den Regeln von
 * kern_suche.c beleg_bereich (links frei: Anfang, \0 oder "; Weg bis \0 oder "; Punkt-Name nach \0, / oder " bis \0, / oder ") */
#define MS_REST 1200                    /* mehr als BELEG_REST (1100): ein Weg am Ende eines Stücks ist immer ganz da (Codex-Prüfung 74) */
typedef struct { const unsigned char *d; size_t n; } mweg;
typedef struct {
    bool (*weiter)(void *c, const unsigned char *d, size_t n); void *c;
    arena *a; unsigned char *b; size_t rn, schon; long long bytes;
    mweg *tab; size_t cap; pliste wege;
    bool voll;                  /* mehr als MS_WEGE_MAX verschiedene Wege - in der S-Zeile gekennzeichnet (Codex-Prüfung 74) */
} mst;
static uint32_t fnv(const unsigned char *d, size_t n) { uint32_t h = 2166136261u; for (size_t i = 0; i < n; i++) { h ^= d[i]; h *= 16777619u; } return h; }
static void weg_merken(mst *st, const unsigned char *d, size_t n)
{
    if (n == 0) return;
    if (st->cap == 0) { st->cap = 65536; st->tab = a_hole(st->a, sizeof(mweg) * st->cap); }   /* 65536 > 2 x MS_WEGE_MAX: immer ein freier Platz */
    for (size_t i = fnv(d, n) & (st->cap - 1); ; i = (i + 1) & (st->cap - 1)) {
        if (!st->tab[i].d) {
            if (st->wege.n >= MS_WEGE_MAX) { st->voll = true; return; }
            unsigned char *k = a_hole(st->a, n); memcpy(k, d, n); st->tab[i] = (mweg){ k, n }; pl_add(st->a, &st->wege, &st->tab[i]); return;
        }
        if (st->tab[i].n == n && !memcmp(st->tab[i].d, d, n)) return;
    }
}
static bool frei_links(const unsigned char *b, size_t q) { return q == 0 || b[q - 1] == 0 || b[q - 1] == '"'; }
static void wege_suchen(mst *st, const unsigned char *b, size_t len, size_t von, size_t bis)
{
    for (size_t p = von; p < bis; ) {
        const unsigned char *t = basis_suche(b + p, len - p, "/data/", 6);
        if (!t) break;
        size_t q = (size_t)(t - b);
        if (q >= bis) break;
        p = q + 1;
        size_t a0 = (q >= 5 && !memcmp(b + q - 5, "/user", 5) && frei_links(b, q - 5)) ? q - 5 : q;
        if (!frei_links(b, a0)) continue;
        size_t i = q + 6;
        while (i < len && i - (q + 6) < MS_WEG_MAX && b[i] != 0 && b[i] != '"') i++;
        /* Codex-Prüfung 74: ein zu langer Weg wird nicht verworfen - die Suche wertet unterwegs jeden Teil-Weg bis zu einem "/" aus.
         * Mitgeschrieben werden die ersten MS_WEG_MAX Zeichen: der Nachbau stößt an dieselbe Grenze und prüft den ganzen Weg ebenso nicht. */
        weg_merken(st, b + a0, i - a0);
        p = i;
    }
    for (size_t p = von; p < bis; ) {
        const unsigned char *t = memchr(b + p, '.', bis - p);
        if (!t) break;
        size_t q = (size_t)(t - b);
        p = q + 1;
        if (!(q == 0 || b[q - 1] == 0 || b[q - 1] == '/' || b[q - 1] == '"')) continue;
        size_t i = q; bool druckbar = true;
        while (i < len && i - q < 256 && b[i] != 0 && b[i] != '/' && b[i] != '"') { if (b[i] < 0x20 || b[i] == 0x7F) druckbar = false; i++; }
        if (i - q < 2 || i - q >= 256 || !druckbar) { p = i > q ? i : q + 1; continue; }
        weg_merken(st, b + q, i - q);
        p = i;
    }
}
static bool m_stueck_weiter(void *c, const unsigned char *d, size_t n)
{
    mst *st = c;
    bool ende = d == NULL;
    if (!ende) {
        st->bytes += (long long)n;
        for (size_t o = 0; o < n; ) {                            /* in den eigenen Puffer (höchstens 64 KB je Durchgang) */
            size_t m = n - o < 65536 ? n - o : 65536;
            memcpy(st->b + st->rn, d + o, m);
            size_t len = st->rn + m, bis = len > MS_REST ? len - MS_REST : 0;
            if (bis > st->schon) wege_suchen(st, st->b, len, st->schon, bis);
            size_t neu = bis > st->schon ? bis : st->schon, ab = neu > 8 ? neu - 8 : 0;
            memmove(st->b, st->b + ab, len - ab);
            st->rn = len - ab; st->schon = neu - ab;
            o += m;
        }
    } else if (st->rn > st->schon) wege_suchen(st, st->b, st->rn, st->schon, st->rn);
    return st->weiter(st->c, d, n);
}
static antwort m_stuecke(zugriff *z, arena *a, const char *pfad, size_t stueck, bool (*weiter)(void *c, const unsigned char *d, size_t n), void *c)
{
    mz *s = z->zustand;
    mst st = { weiter, c, arena_neu(), NULL, 0, 0, 0, NULL, 0, {0}, false };
    st.b = a_hole(st.a, 65536 + MS_REST + 16);
    antwort r = s->innen->stuecke(s->innen, a, pfad, stueck, m_stueck_weiter, &st);
    const char *k = VERB(st.a, kopf_text(st.a, 'S', r, norm(st.a, pfad)), " ", zahl(st.a, st.bytes), " ", zahl(st.a, st.wege.n), st.voll ? " voll" : "", "\n");
    const char **w = a_hole(st.a, sizeof(char *) * (size_t)(st.wege.n + 1));
    size_t g = strlen(k) + 1;
    for (int i = 0; i < st.wege.n; i++) { mweg *x = st.wege.e[i]; w[i] = b64(st.a, x->d, x->n); g += strlen(w[i]) + 3; }
    char *o = a_hole(st.a, g + 1), *p = o;
    p += snprintf(p, g + 1, "%s", k);
    for (int i = 0; i < st.wege.n; i++) p += snprintf(p, g + 1 - (size_t)(p - o), ". %s\n", w[i]);
    raus(s, o);
    arena_frei(st.a);
    return r;
}
/* vor und nach dem Entfernen einer Kachel: der Zustand ihrer Ordner (und über zugriff_mitschnitt_zustand die Zeilen in den Datenbanken) */
static mz *zustand_mz;
static void zustand_zeile(const char *t) { if (zustand_mz) { arena *a = arena_neu(); raus(zustand_mz, VERB(a, "I ", pct(a, t), "\n")); arena_frei(a); } }
/* Kennung aus /user/app/<Kennung> oder /mnt/extN/user/app/<Kennung> - nur genau dieser Ordner, nicht tiefer (Codex-Prüfung 74) */
static const char *app_kennung(arena *a, const char *v)
{
    const char *m = strstr(v, "/user/app/");
    if (!m || (m != v && strncmp(v, "/mnt/ext", 8))) return NULL;
    const char *id = m + 10;
    if (!*id || strchr(id, '/')) return NULL;
    return a_kopie(a, id);
}
static void zustand(mz *s, arena *a, const char *v, const char *id, bool nachher)
{
    raus(s, VERB(a, "I ZUSTAND ", nachher ? "NACH " : "VOR ", pct(a, id), "\n"));
    const char *o[4] = { v, VERB(a, "/user/appmeta/", id), VERB(a, "/system_data/priv/appmeta/", id), VERB(a, "/user/app/", id) };
    for (int i = 0; i < 4; i++) {
        if (i == 3 && !strcmp(o[3], v)) continue;
        kdliste l = {0};
        antwort r = s->innen->liste(s->innen, a, o[i], &l);
        raus(s, liste_text(a, r, norm(a, o[i]), &l));
    }
    if (zugriff_mitschnitt_zustand) {
        static pthread_mutex_t zm = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&zm); zustand_mz = s; zugriff_mitschnitt_zustand(id, nachher, zustand_zeile); zustand_mz = NULL; pthread_mutex_unlock(&zm);
    }
}
static antwort m_befehl(zugriff *z, arena *a, const char *befehl, const char *pfad)
{
    mz *s = z->zustand;
    arena *t = arena_neu();   /* Codex-Prüfung 92: alles für den Mitschnitt (auch die Ordnerlisten vor und nach) in eigenem Speicher */
    const char *v = pfad ? norm(t, pfad) : NULL;
    const char *id = (befehl && v && (!strcmp(befehl, "SITE UNINSTALL") || !strcmp(befehl, "RMDA"))) ? app_kennung(t, v) : NULL;
    if (id) zustand(s, t, v, id, false);
    antwort r = s->innen->befehl(s->innen, a, befehl, pfad);
    raus(s, VERB(t, "B ", zahl(t, r.code), " ", pct(t, r.text), " ", pct(t, befehl), " ", pct(t, v ? v : ""), "\n"));
    if (id) zustand(s, t, v, id, true);
    arena_frei(t);
    return r;
}
static void m_schliessen(zugriff *z)
{
    mz *s = z->zustand;
    if (s->innen && s->innen->schliessen) s->innen->schliessen(s->innen);
    pthread_mutex_destroy(&s->m);
    free(s); free(z);
}
zugriff *zugriff_mitschnitt(zugriff *innen, void (*schreib)(void *c, const char *zeilen), void *c)
{
    zugriff *z = calloc(1, sizeof *z);
    mz *s = calloc(1, sizeof *s);
    if (!z || !s) { free(z); free(s); return innen; }
    s->innen = innen; s->schreib = schreib; s->c = c;
    pthread_mutex_init(&s->m, NULL);
    z->art = "mitschnitt";
    z->cwd = m_cwd; z->liste = m_liste; z->lies = m_lies; z->schreibe = m_schreibe; z->befehl = m_befehl;
    z->kopf = innen->kopf ? m_kopf : NULL;
    z->enthaelt = innen->enthaelt ? m_enthaelt : NULL;
    z->stuecke = innen->stuecke ? m_stuecke : NULL;
    z->lies_ab = innen->lies_ab ? m_lies_ab : NULL;
    z->schliessen = m_schliessen; z->zustand = s;
    return z;
}

/* ------------------------------------------------------------------ Wiedergabe (PC-Nachbau) */
typedef struct { const char *k; void *v; } wpaar;
typedef struct { wpaar *e; size_t cap, n; } wtab;
static uint32_t wfnv(const char *s) { return fnv((const unsigned char *)s, strlen(s)); }
static void *w_hole(const wtab *t, const char *k)
{
    if (!t->cap) return NULL;
    for (size_t i = wfnv(k) & (t->cap - 1), s = 0; s < t->cap && t->e[i].k; s++, i = (i + 1) & (t->cap - 1))
        if (!strcmp(t->e[i].k, k)) return t->e[i].v;
    return NULL;
}
static void w_neu(arena *a, wtab *t, const char *k, void *v)   /* nur die erste Antwort zählt */
{
    if (t->n * 2 >= t->cap) {
        wtab n = { a_hole(a, sizeof(wpaar) * (t->cap ? t->cap * 2 : 1024)), t->cap ? t->cap * 2 : 1024, 0 };
        for (size_t i = 0; i < t->cap; i++) if (t->e[i].k) w_neu(a, &n, t->e[i].k, t->e[i].v);
        *t = n;
    }
    size_t i = wfnv(k) & (t->cap - 1);
    while (t->e[i].k) { if (!strcmp(t->e[i].k, k)) return; i = (i + 1) & (t->cap - 1); }
    t->e[i].k = a_kopie(a, k); t->e[i].v = v; t->n++;
}
typedef struct { antwort r; kdliste l; } w_liste;
typedef struct { antwort r; const unsigned char *d; size_t n; bool fehlt; } w_daten;   /* fehlt: Datei größer als 1 MB, nicht mitgeschrieben */
typedef struct { long long ab; size_t nreq; const unsigned char *d; size_t n; } w_stueck;
typedef struct { antwort r; pliste stuecke; } w_ab;
typedef struct { antwort r; bool ja; } w_ja;
typedef struct { const char *titel, *feld, *wert; } w_info;
typedef struct {
    arena *a;
    wtab c, l, r, k, ab, e, s, b;
    pliste d;                 /* Zeilen aus appinfo.db (D) */
    sliste info;              /* Hinweise (I) */
    pthread_mutex_t m;
    int fehlend;              /* Fragen, die nicht im Mitschnitt stehen */
    sliste fehlt;             /* die ersten davon (für den Bericht) */
    int nkaputt;              /* Codex-Prüfung 74: unvollständige oder kaputte Datensätze - nicht verwendet, der Nachbau ist dort ungenau */
    sliste kaputt;            /* die ersten davon (für den Bericht) */
    long long budget;         /* so viel Speicher belegen Daten und Listen schon */
} wz;
static antwort WA(arena *a, int code, const char *text) { antwort r = { code, a_kopie(a, text) }; return r; }
static void fehlt(wz *s, const char *was, const char *pfad)
{
    pthread_mutex_lock(&s->m);
    s->fehlend++;
    if (s->fehlt.n < 200) sl_add(s->a, &s->fehlt, VERB(s->a, was, " ", pfad));
    pthread_mutex_unlock(&s->m);
}
static antwort w_cwd(zugriff *z, arena *a, const char *pfad)
{
    wz *s = z->zustand; const char *v = norm(a, pfad);
    antwort *r = w_hole(&s->c, v);
    if (r) return WA(a, r->code, r->text);
    w_liste *l = w_hole(&s->l, v);
    if (l) return WA(a, l->r.code < 400 ? 250 : l->r.code, l->r.code < 400 ? "250 ok" : l->r.text);
    fehlt(s, "CWD", v);
    return WA(a, 550, "550 nicht im Mitschnitt");
}
static antwort w_liste_f(zugriff *z, arena *a, const char *pfad, kdliste *aus)
{
    wz *s = z->zustand; const char *v = norm(a, pfad);
    w_liste *l = w_hole(&s->l, v);
    aus->e = NULL; aus->n = 0;
    if (!l) { fehlt(s, "LIST", v); return WA(a, 550, "550 nicht im Mitschnitt"); }
    if (l->r.code < 400) {
        aus->e = a_hole(a, sizeof(kdatei) * (size_t)(l->l.n + 1)); aus->n = l->l.n;
        for (int i = 0; i < l->l.n; i++) { aus->e[i] = l->l.e[i]; aus->e[i].name = a_kopie(a, l->l.e[i].name); }
    }
    return WA(a, l->r.code, l->r.text);
}
static antwort w_lies(zugriff *z, arena *a, const char *pfad, const unsigned char **d, size_t *n)
{
    wz *s = z->zustand; const char *v = norm(a, pfad);
    w_daten *x = w_hole(&s->r, v);
    *d = NULL; *n = 0;
    if (!x || x->fehlt) { fehlt(s, "RETR", v); return WA(a, 550, "550 nicht im Mitschnitt"); }
    if (x->r.code < 400) { unsigned char *k = a_hole(a, x->n + 1); memcpy(k, x->d, x->n); *d = k; *n = x->n; }
    return WA(a, x->r.code, x->r.text);
}
static antwort w_kopf(zugriff *z, arena *a, const char *pfad, size_t n, const unsigned char **d, size_t *len)
{
    wz *s = z->zustand; const char *v = norm(a, pfad);
    w_daten *x = w_hole(&s->k, v), *g = w_hole(&s->r, v);
    *d = NULL; *len = 0;
    if (g && !g->fehlt && (!x || g->n >= x->n)) x = g;     /* die ganze Datei, wenn sie da ist */
    if (!x || x->fehlt) { fehlt(s, "KOPF", v); return WA(a, 550, "550 nicht im Mitschnitt"); }
    if (x->r.code < 400) { size_t m = x->n < n ? x->n : n; unsigned char *k = a_hole(a, m + 1); memcpy(k, x->d, m); *d = k; *len = m; }
    return WA(a, x->r.code, x->r.text);
}
static antwort w_enthaelt(zugriff *z, arena *a, const char *pfad, const void *mu, size_t ml, bool *ja)
{
    wz *s = z->zustand; const char *v = norm(a, pfad);
    *ja = false;
    w_ja *x = w_hole(&s->e, VERB(a, v, "\001", b64(a, mu, ml)));
    if (x) { *ja = x->ja; return WA(a, x->r.code, x->r.text); }
    w_daten *g = w_hole(&s->r, v);
    if (g && !g->fehlt) { if (g->r.code < 400) *ja = g->n >= ml && basis_suche(g->d, g->n, mu, ml) != NULL; return WA(a, g->r.code, g->r.text); }
    fehlt(s, "ENTHAELT", v);
    return WA(a, 550, "550 nicht im Mitschnitt");
}
static antwort w_stuecke(zugriff *z, arena *a, const char *pfad, size_t stueck, bool (*weiter)(void *c, const unsigned char *d, size_t n), void *c)
{
    wz *s = z->zustand; const char *v = norm(a, pfad);
    if (stueck == 0) return WA(a, 501, "501 Stück");
    w_daten *x = w_hole(&s->s, v);
    if (!x) { x = w_hole(&s->r, v); if (x && x->fehlt) x = NULL; }
    if (!x) { fehlt(s, "STUECKE", v); return WA(a, 550, "550 nicht im Mitschnitt"); }
    if (x->r.code < 400) {
        bool los = true;
        for (size_t o = 0; los && o < x->n; o += stueck) los = weiter(c, x->d + o, x->n - o < stueck ? x->n - o : stueck);
        if (los) weiter(c, NULL, 0);
    }
    return WA(a, x->r.code, x->r.text);
}
static antwort w_lies_ab(zugriff *z, arena *a, const char *pfad, long long ab, size_t n, unsigned char *ziel, size_t *len)
{
    wz *s = z->zustand; const char *v = norm(a, pfad);
    *len = 0;
    if (ab < 0) return WA(a, 501, "501 Stelle");
    w_ab *x = w_hole(&s->ab, v);
    if (x) for (int i = 0; i < x->stuecke.n; i++) {
        w_stueck *t = x->stuecke.e[i];
        if (t->ab == ab && t->nreq == n) { memcpy(ziel, t->d, t->n < n ? t->n : n); *len = t->n < n ? t->n : n; return WA(a, 226, "226 done"); }   /* dieselbe Frage */
        if (t->ab <= ab && (unsigned long long)(ab - t->ab) + n <= t->n) { memcpy(ziel, t->d + (ab - t->ab), n); *len = n; return WA(a, 226, "226 done"); }
    }
    w_daten *g = w_hole(&s->r, v);
    if (g && !g->fehlt && g->r.code < 400) {
        if ((unsigned long long)ab < g->n) { size_t m = g->n - (size_t)ab; if (m > n) m = n; memcpy(ziel, g->d + ab, m); *len = m; }
        return WA(a, 226, "226 done");
    }
    if (x && x->r.code >= 400) return WA(a, x->r.code, x->r.text);
    fehlt(s, "LIES_AB", v);
    return WA(a, 550, "550 nicht im Mitschnitt");
}
static antwort w_schreibe(zugriff *z, arena *a, const char *pfad, const void *d, size_t n) { (void)z; (void)pfad; (void)d; (void)n; return WA(a, 550, "550 Wiedergabe"); }
static antwort w_befehl(zugriff *z, arena *a, const char *befehl, const char *pfad)
{
    wz *s = z->zustand;
    if (!befehl) return WA(a, 550, "550 Wiedergabe");
    if (!strcmp(befehl, "NOOP")) return WA(a, 200, "200 ok");
    size_t l = strlen(befehl);
    if (l && befehl[l - 1] == '?') {                       /* nur Fragen ("SITE UNINSTALL?" ...) wie aufgezeichnet beantworten */
        antwort *x = w_hole(&s->b, VERB(a, befehl, "\001", pfad ? norm(a, pfad) : ""));
        if (x) return WA(a, x->code, x->text);
        return WA(a, 502, "502 nicht im Mitschnitt");
    }
    return WA(a, 550, "550 Wiedergabe");                  /* nie etwas verändern */
}
static void w_schliessen(zugriff *z) { wz *s = z->zustand; pthread_mutex_destroy(&s->m); arena_frei(s->a); free(s); free(z); }

/* eine Zeile in Felder zerlegen (Leerzeichen) */
static int felder(char *z, char **f, int max)
{
    int n = 0;
    for (char *p = z; *p && n < max; ) {
        while (*p == ' ') p++;
        if (!*p) break;
        f[n++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    return n;
}
/* streng (Codex-Prüfung 74): nur gültiges base64 - Länge durch 4 teilbar, "=" höchstens zweimal und nur am Ende. "-" = leer. Sonst NULL */
static unsigned char *unb64_streng(arena *a, const char *s, size_t *n)
{
    *n = 0;
    if (!strcmp(s, "-")) return a_hole(a, 1);
    size_t l = strlen(s), gl = 0;
    if (l == 0 || l % 4) return NULL;
    while (gl < 2 && gl < l && s[l - 1 - gl] == '=') gl++;
    for (size_t i = 0; i < l - gl; i++) if (b64wert(s[i]) < 0) return NULL;
    return unb64(a, s, n);
}
#define MS_ZEILE_MAX (16u << 20)            /* längste Zeile: ganze Dateien bis 1 MB stehen base64 mit etwa 1,4 MB da */
#define MS_BUDGET (1536LL << 20)            /* Wiedergabe: höchstens so viel Speicher für Daten und Listen (Codex-Prüfung 74) */
#define MS_LISTE_MAX 10000000               /* mehr Einträge in einem Ordner gibt es nicht - kaputte Zahl */
/* Codex-Prüfung 75: Zahlen streng lesen - nur Ziffern (mit Vorzeichen), sonst gilt die Zeile als kaputt */
static bool zahl_streng(const char *s, long long *v)
{
    if (!s || !*s) return false;
    char *e = NULL; errno = 0;
    long long x = strtoll(s, &e, 10);
    if (errno || !e || *e) return false;
    *v = x; return true;
}
static void kaputt(wz *s, const char *was, const char *pfad)
{
    s->nkaputt++;
    if (s->kaputt.n < 200) sl_add(s->a, &s->kaputt, VERB(s->a, was, " ", pfad ? pfad : ""));
}
zugriff *zugriff_wiedergabe(const char *datei, int suche_nr, const char **fehler)
{
    *fehler = "";
    FILE *f = fopen(datei, "r");
    if (!f) { *fehler = "Mitschnitt nicht lesbar"; return NULL; }
    zugriff *z = calloc(1, sizeof *z);
    wz *s = calloc(1, sizeof *s);
    if (!z || !s) { fclose(f); free(z); free(s); *fehler = "kein Speicher"; return NULL; }
    s->a = arena_neu(); pthread_mutex_init(&s->m, NULL);
    arena *a = s->a;
    size_t cap = 1 << 20; char *z1 = malloc(cap);
    int suche = 0; bool mit = suche_nr <= 0;
    /* L/S mit Folgezeilen (Codex-Prüfung 74): erst gespeichert, wenn alle angekündigten Folgezeilen da und lesbar sind - ein
     * abgeschnittener oder kaputter Datensatz wird nicht verwendet (die Frage gilt dann als "nicht im Mitschnitt") und gemeldet */
    char o_typ = 0; long o_rest = 0; bool o_nimm = false, o_voll = false; const char *o_pfad = NULL; antwort o_r = { 0, "" };
    kdatei *ol = NULL; long ol_n = 0, ol_cap = 0;            /* L: Einträge (wächst mit den Folgezeilen - nichts vorab belegt) */
    pliste o_wege = {0};                                     /* S: Wege */
    const char *abbruch = NULL;
    for (;;) {
        if (!z1) { abbruch = "kein Speicher"; break; }
        size_t n = 0; int ch;
        while ((ch = fgetc(f)) != EOF && ch != '\n') {
            if (n + 1 >= cap) {
                if (cap >= MS_ZEILE_MAX) { abbruch = "eine Zeile ist zu lang - Mitschnitt kaputt?"; break; }
                char *nz = realloc(z1, cap * 2); if (!nz) { abbruch = "kein Speicher"; break; }
                z1 = nz; cap *= 2;
            }
            z1[n++] = (char)ch;
        }
        if (abbruch) break;
        if (ch == EOF && n == 0) break;
        z1[n] = 0;
        if (n && z1[n - 1] == '\r') z1[--n] = 0;
        char *fe[8]; int nf = felder(z1, fe, 8);
        if (nf == 0) continue;
        /* Codex-Prüfung 75: Speichergrenze VOR dem Entpacken - jede Zeile, die gemerkt werden kann, zählt mit ihrer ganzen Länge (mehr wird
         * daraus nie), auch wenn sie danach als kaputt verworfen wird */
        {
            bool folge = o_rest > 0 && !strcmp(fe[0], ".");
            bool merkbar = folge ? o_nimm : (mit || !strcmp(fe[0], "I"));
            if (merkbar) { s->budget += (long long)n + 96; if (s->budget > MS_BUDGET) { abbruch = "Mitschnitt zu groß für die Wiedergabe"; break; } }
        }
        if (o_rest > 0 && !strcmp(fe[0], ".")) {             /* Folgezeile von L oder S */
            o_rest--;
            if (o_typ == 'L' && o_nimm) {
                long long gr = 0;
                if (nf < 4 || strlen(fe[1]) < 2 || !zahl_streng(fe[2], &gr) || gr < 0) { kaputt(s, "L Folgezeile kaputt:", o_pfad); o_nimm = false; }
                else {
                    if (ol_n == ol_cap) {
                        long nc = ol_cap ? ol_cap * 2 : 256; kdatei *x = realloc(ol, sizeof(kdatei) * (size_t)nc);
                        if (!x) { abbruch = "kein Speicher"; break; }
                        ol = x; ol_cap = nc;
                    }
                    kdatei *e = &ol[ol_n++];
                    e->ordner = fe[1][0] == 'd'; e->link = fe[1][1] == 'l'; e->groesse = gr; e->name = unpct(a, fe[3]);
                }
            } else if (o_typ == 'S' && o_nimm) {
                size_t wn = 0; unsigned char *w = nf >= 2 ? unb64_streng(a, fe[1], &wn) : NULL;
                if (!w) { kaputt(s, "S Folgezeile kaputt:", o_pfad); o_nimm = false; }
                else { w_stueck *t = a_hole(a, sizeof *t); t->d = w; t->n = wn; pl_add(a, &o_wege, t); }
            }
            if (s->budget > MS_BUDGET) { abbruch = "Mitschnitt zu groß für die Wiedergabe"; break; }
            if (o_rest == 0 && o_nimm) {                     /* vollständig: jetzt speichern */
                if (o_typ == 'L' && !w_hole(&s->l, o_pfad)) {
                    w_liste *x = a_hole(a, sizeof *x); x->r = o_r;
                    x->l.e = a_hole(a, sizeof(kdatei) * (size_t)(ol_n + 1)); if (ol_n) memcpy(x->l.e, ol, sizeof(kdatei) * (size_t)ol_n); x->l.n = (int)ol_n;
                    w_neu(a, &s->l, o_pfad, x);
                } else if (o_typ == 'S' && !w_hole(&s->s, o_pfad)) {   /* Inhalt wie ein Programm: ELF-Kennung, dann jeder Weg mit \0 davor und danach */
                    size_t g = 5; for (int i = 0; i < o_wege.n; i++) g += ((w_stueck *)o_wege.e[i])->n + 1;
                    unsigned char *d = a_hole(a, g + 1); memcpy(d, "\177ELF", 4); size_t o = 5;
                    for (int i = 0; i < o_wege.n; i++) { w_stueck *t = o_wege.e[i]; memcpy(d + o, t->d, t->n); o += t->n + 1; }
                    w_daten *x = a_hole(a, sizeof *x); x->r = o_r; x->d = d; x->n = o;
                    w_neu(a, &s->s, o_pfad, x);
                }
                o_typ = 0; ol_n = 0; o_wege = (pliste){0};
            }
            continue;
        }
        if (o_rest > 0) {                                    /* angekündigte Folgezeilen fehlen: nicht verwenden */
            if (o_nimm) kaputt(s, o_typ == 'L' ? "L unvollständig:" : "S unvollständig:", o_pfad);
            o_rest = 0; o_typ = 0; o_nimm = false; ol_n = 0; o_wege = (pliste){0};
        }
        if (!strcmp(fe[0], ".")) continue;                   /* Folgezeile ohne Kopf */
        if (!strcmp(fe[0], "I")) {
            const char *t = nf >= 2 ? unpct(a, fe[1]) : "";
            /* "I SUCHE <n> [NACH_LOESCHEN]": Felder sind einzeln kodiert - den Rest der Zeile wieder zusammensetzen */
            if (nf >= 3 && !strcmp(fe[1], "SUCHE")) { suche = atoi(fe[2]); mit = suche_nr <= 0 || suche == suche_nr; }
            /* die Kopfzeilen vor der ersten Suche (Konsole, eigener Ort, Test-Ordner) gelten für jede Suche */
            if (mit || suche == 0) { const char *g = t; for (int i = 2; i < nf; i++) g = VERB(a, g, " ", unpct(a, fe[i])); sl_add(a, &s->info, g); }
            continue;
        }
        bool hat_folge = (!strcmp(fe[0], "L") && nf >= 5) || (!strcmp(fe[0], "S") && nf >= 6);
        if (hat_folge) {                                     /* auch außerhalb der gewählten Suche: Folgezeilen überspringen */
            long long k = -1, code = 0;
            ol_n = 0; o_wege = (pliste){0};
            /* Codex-Prüfung 75: kaputte Anzahl - wie viele Folgezeilen kommen, ist unbekannt; nichts verwenden (sie werden als Folgezeilen
             * ohne Kopf übersprungen), statt eine leere Antwort daraus zu machen */
            if (!zahl_streng(fe[0][0] == 'L' ? fe[4] : fe[5], &k) || k < 0 || k > MS_LISTE_MAX) {
                if (mit) kaputt(s, "Anzahl kaputt:", unpct(a, fe[3]));
                o_typ = 0; o_rest = 0; o_nimm = false;
                continue;
            }
            o_typ = fe[0][0]; o_rest = k;
            if (!mit) { o_nimm = false; continue; }          /* andere Suche: Folgezeilen nur überspringen, nichts merken */
            o_pfad = unpct(a, fe[3]);
            o_voll = o_typ == 'S' && nf >= 7 && !strcmp(fe[6], "voll");
            o_nimm = !(o_typ == 'L' ? w_hole(&s->l, o_pfad) : w_hole(&s->s, o_pfad));   /* nur die erste Antwort zählt */
            if (o_nimm && !zahl_streng(fe[1], &code)) { kaputt(s, "Antwortcode kaputt:", o_pfad); o_nimm = false; }
            /* Codex-Prüfung 75: ausdrücklich unvollständige Wege (mehr als mitgeschrieben) nicht verwenden - der Nachbau fragt dann "fehlt" */
            if (o_nimm && o_voll) { kaputt(s, "S unvollständig (mehr Wege, als mitgeschrieben wurden) - nicht verwendet:", o_pfad); o_nimm = false; }
            o_r = (antwort){ (int)code, unpct(a, fe[2]) };
            if (o_rest == 0 && o_nimm) {                     /* ohne Folgezeilen: gleich speichern */
                if (o_typ == 'L') { w_liste *x = a_hole(a, sizeof *x); x->r = o_r; x->l.e = a_hole(a, sizeof(kdatei)); x->l.n = 0; w_neu(a, &s->l, o_pfad, x); }
                else { w_daten *x = a_hole(a, sizeof *x); x->r = o_r; x->d = (const unsigned char *)"\177ELF"; x->n = 4; w_neu(a, &s->s, o_pfad, x); }
                o_typ = 0;
            }
            continue;
        }
        if (!mit) continue;
        if (!strcmp(fe[0], "D") && nf >= 4) { w_info *d = a_hole(a, sizeof *d); d->titel = unpct(a, fe[1]); d->feld = unpct(a, fe[2]); d->wert = unpct(a, fe[3]); pl_add(a, &s->d, d); continue; }
        if (nf < 4 || strlen(fe[0]) != 1) continue;
        long long code = 0;
        const char *pfad = unpct(a, fe[3]);
        if (strchr("CRKABE", fe[0][0]) && !zahl_streng(fe[1], &code)) { kaputt(s, "Antwortcode kaputt:", pfad); continue; }   /* Codex-Prüfung 75 */
        antwort r = { (int)code, unpct(a, fe[2]) };
        switch (fe[0][0]) {
        case 'C': { antwort *x = a_hole(a, sizeof *x); *x = r; w_neu(a, &s->c, pfad, x); break; }
        case 'R': {
            if (nf < 6) { kaputt(s, "R kaputt:", pfad); break; }
            w_daten *x = a_hole(a, sizeof *x); x->r = r;
            if (!strcmp(fe[5], "*")) x->fehlt = true;
            else {
                long long laenge = -1;
                x->d = unb64_streng(a, fe[5], &x->n);
                if (!x->d || !zahl_streng(fe[4], &laenge) || (long long)x->n != laenge) { kaputt(s, "R Länge oder Daten kaputt:", pfad); break; }   /* Codex-Prüfungen 74/75 */
            }
            w_neu(a, &s->r, pfad, x);
            break;
        }
        case 'K': {
            if (nf < 5) { kaputt(s, "K kaputt:", pfad); break; }
            w_daten *x = a_hole(a, sizeof *x); x->r = r; x->d = unb64_streng(a, fe[4], &x->n);
            if (!x->d) { kaputt(s, "K Daten kaputt:", pfad); break; }
            w_daten *alt = w_hole(&s->k, pfad);
            if (alt) { if (x->n > alt->n && r.code < 400) *alt = *x; } else w_neu(a, &s->k, pfad, x);   /* der längste Anfang */
            break;
        }
        case 'A': {
            if (nf < 7) { kaputt(s, "A kaputt:", pfad); break; }
            w_ab *x = w_hole(&s->ab, pfad);
            if (!x) { x = a_hole(a, sizeof *x); x->r = r; w_neu(a, &s->ab, pfad, x); }
            if (r.code < 400 && strcmp(fe[6], "*")) {
                long long ab = -1, nreq = -1;
                w_stueck *t = a_hole(a, sizeof *t); t->d = unb64_streng(a, fe[6], &t->n);
                if (!t->d || !zahl_streng(fe[4], &ab) || !zahl_streng(fe[5], &nreq) || ab < 0 || nreq < 0 || (long long)t->n > nreq) { kaputt(s, "A Daten kaputt:", pfad); break; }
                t->ab = ab; t->nreq = (size_t)nreq;
                pl_add(a, &x->stuecke, t);
            }
            break;
        }
        case 'B': {
            if (nf < 5) break;                             /* Zeile: B code text befehl pfad - fe[3] ist hier der Befehl */
            antwort *x = a_hole(a, sizeof *x); *x = r;
            w_neu(a, &s->b, VERB(a, unpct(a, fe[3]), "\001", unpct(a, fe[4])), x);
            break;
        }
        case 'E': {
            if (nf < 6) break;
            w_ja *x = a_hole(a, sizeof *x); x->r = r; x->ja = fe[4][0] == '1';
            w_neu(a, &s->e, VERB(a, pfad, "\001", fe[5]), x);
            break;
        }
        default: break;                                    /* W, P: nichts zu beantworten */
        }
        if (s->budget > MS_BUDGET) { abbruch = "Mitschnitt zu groß für die Wiedergabe"; break; }
    }
    if (!abbruch && o_rest > 0 && o_nimm) kaputt(s, o_typ == 'L' ? "L unvollständig (Dateiende):" : "S unvollständig (Dateiende):", o_pfad);
    free(z1); free(ol);
    fclose(f);
    if (abbruch) { *fehler = abbruch; pthread_mutex_destroy(&s->m); arena_frei(s->a); free(s); free(z); return NULL; }
    z->art = "wiedergabe";
    z->cwd = w_cwd; z->liste = w_liste_f; z->lies = w_lies; z->kopf = w_kopf; z->enthaelt = w_enthaelt; z->stuecke = w_stuecke;
    z->lies_ab = w_lies_ab; z->schreibe = w_schreibe; z->befehl = w_befehl; z->schliessen = w_schliessen; z->zustand = s;
    return z;
}
int zugriff_wiedergabe_kaputt(zugriff *z, const sliste **welche) { wz *s = z->zustand; if (welche) *welche = &s->kaputt; return s->nkaputt; }
int zugriff_wiedergabe_fehlend(zugriff *z, const sliste **welche) { wz *s = z->zustand; if (welche) *welche = &s->fehlt; return s->fehlend; }
const sliste *zugriff_wiedergabe_info(zugriff *z) { return &((wz *)z->zustand)->info; }
void zugriff_wiedergabe_appinfo(zugriff *z, void (*zeile)(void *c, const char *titel, const char *feld, const char *wert), void *c)
{
    wz *s = z->zustand;
    for (int i = 0; i < s->d.n; i++) { w_info *d = s->d.e[i]; zeile(c, d->titel, d->feld, d->wert); }
}
/* Zeilen, die die Oberfläche selbst schreibt (I, P, D) - Texte kodiert wie oben */
const char *zugriff_mitschnitt_zeile(arena *a, char typ, const char *t1, const char *t2, const char *t3)
{
    char t[2] = { typ, 0 };
    if (typ == 'D') return VERB(a, t, " ", pct(a, t1), " ", pct(a, t2), " ", pct(a, t3), "\n");
    return VERB(a, t, " ", pct(a, t1), "\n");
}
#else
typedef int zugriff_mitschnitt_nicht_gebaut;   /* Fassungen ohne Mitschnitt: von alldem nichts im Programm */
#endif
