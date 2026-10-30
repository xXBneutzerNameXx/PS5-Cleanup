/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * zugriff_sim.c - Simulation: liest über eine andere Zugriffsschicht,
 * verändert aber NICHTS. Jeder verändernde Befehl wird nur ins Buch
 * geschrieben und danach so behandelt, als wäre er ausgeführt worden
 * (gelöschte Pfade sind ab dann "nicht mehr da"), damit der Ablauf genauso
 * weiterläuft wie beim echten Löschen.
 */
#include "zugriff.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    zugriff *innen;
    arena *a;
    mtab weg;          /* gelöschte Pfade (genau so geschrieben) */
    mtab virtuell;     /* "geschriebene" Dateien: Pfad -> sdaten* */
    sliste buch;
} sz;
typedef struct { const unsigned char *d; size_t n; } sdaten;

static antwort A(arena *a, int code, const char *text) { antwort r = { code, a_kopie(a, text) }; return r; }

static const char *norm(arena *a, const char *p)
{
    /* wie zugriff_datei: ".", "..", doppelte "/" auflösen */
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

/* ist v selbst oder ein Ordner darüber gelöscht? */
static bool ist_weg(sz *s, const char *v)
{
    for (int i = 0; i < s->weg.n; i++) {
        const char *w = s->weg.e[i].k; size_t l = strlen(w);
        if (strncmp(v, w, l) == 0 && (v[l] == 0 || v[l] == '/')) return true;
    }
    return false;
}
static const char *kind(arena *a, const char *v, const char *name) { return strcmp(v, "/") == 0 ? VERB(a, "/", name) : VERB(a, v, "/", name); }
static const char *eltern(arena *a, const char *v) { int i = txt_letzter(v, '/'); return i <= 0 ? "/" : a_kopie_n(a, v, (size_t)i); }
static const char *blatt(const char *v) { return v + txt_letzter(v, '/') + 1; }

static antwort s_cwd(zugriff *z, arena *a, const char *pfad)
{
    sz *s = z->zustand;
    const char *v = norm(a, pfad);
    if (ist_weg(s, v)) return A(a, 550, "550 no");
    return s->innen->cwd(s->innen, a, v);
}

static antwort s_liste(zugriff *z, arena *a, const char *pfad, kdliste *aus)
{
    sz *s = z->zustand;
    const char *v = norm(a, pfad);
    aus->e = NULL; aus->n = 0;
    if (ist_weg(s, v)) return A(a, 550, "550 no");
    kdliste l; antwort r = s->innen->liste(s->innen, a, v, &l);
    if (r.code >= 400 || r.code < 0) return r;
    int virt = 0;
    for (int i = 0; i < s->virtuell.n; i++) if (strcmp(eltern(a, s->virtuell.e[i].k), v) == 0) virt++;
    aus->e = a_hole(a, sizeof(kdatei) * (size_t)(l.n + virt + 1));
    for (int i = 0; i < l.n; i++) {
        const char *nm = l.e[i].name;
        if (strcmp(nm, ".") && strcmp(nm, "..") && ist_weg(s, kind(a, v, nm))) continue;
        aus->e[aus->n++] = l.e[i];
    }
    for (int i = 0; i < s->virtuell.n; i++) {
        const char *k = s->virtuell.e[i].k;
        if (strcmp(eltern(a, k), v) != 0) continue;
        bool schon = false;
        for (int j = 0; j < aus->n; j++) if (strcmp(aus->e[j].name, blatt(k)) == 0) schon = true;
        if (schon) continue;
        kdatei *e = &aus->e[aus->n++];
        e->name = a_kopie(a, blatt(k)); e->groesse = (long long)((sdaten *)s->virtuell.e[i].v)->n;
    }
    return r;
}

static antwort s_lies(zugriff *z, arena *a, const char *pfad, const unsigned char **d, size_t *n)
{
    sz *s = z->zustand;
    const char *v = norm(a, pfad);
    *d = NULL; *n = 0;
    sdaten *vd = mt_hole(&s->virtuell, v);
    if (vd) { *d = vd->d; *n = vd->n; return A(a, 226, "226 done"); }
    if (ist_weg(s, v)) return A(a, 550, "550 no");
    return s->innen->lies(s->innen, a, v, d, n);
}

/* NEU 1.0.6 Fix: Suche nach einer Byte-Folge - an den inneren Zugriff weiterreichen (sonst ganz lesen) */
static antwort s_enthaelt(zugriff *z, arena *a, const char *pfad, const void *m, size_t ml, bool *ja)
{
    sz *s = z->zustand;
    const char *v = norm(a, pfad);
    *ja = false;
    sdaten *vd = mt_hole(&s->virtuell, v);
    if (vd) { *ja = vd->n >= ml && basis_suche(vd->d, vd->n, m, ml) != NULL; return A(a, 226, "226 done"); }
    if (ist_weg(s, v)) return A(a, 550, "550 no");
    if (s->innen->enthaelt) return s->innen->enthaelt(s->innen, a, v, m, ml, ja);
    const unsigned char *d; size_t n;
    antwort r = s->innen->lies(s->innen, a, v, &d, &n);
    if (r.code < 400) *ja = n >= ml && basis_suche(d, n, m, ml) != NULL;
    return r;
}
static antwort s_kopf(zugriff *z, arena *a, const char *pfad, size_t n, const unsigned char **d, size_t *len)
{
    sz *s = z->zustand;
    const char *v = norm(a, pfad);
    *d = NULL; *len = 0;
    sdaten *vd = mt_hole(&s->virtuell, v);
    if (vd) { *d = vd->d; *len = vd->n < n ? vd->n : n; return A(a, 226, "226 done"); }
    if (ist_weg(s, v)) return A(a, 550, "550 no");
    if (!s->innen->kopf) return A(a, 502, "502 no");
    return s->innen->kopf(s->innen, a, v, n, d, len);
}

static antwort s_schreibe(zugriff *z, arena *a, const char *pfad, const void *daten, size_t n)
{
    sz *s = z->zustand;
    const char *v = norm(s->a, pfad);
    sl_add(s->a, &s->buch, VERB(s->a, "STOR ", v));
    sdaten *vd = a_hole(s->a, sizeof *vd);
    unsigned char *k = a_hole(s->a, n + 1); if (n) memcpy(k, daten, n);
    vd->d = k; vd->n = n;
    mt_setze(s->a, &s->virtuell, v, vd);
    return A(a, 226, "226 done");
}

/* Eintrag in der (simulierten) Ordnerliste des Elternordners */
static const kdatei *finde(zugriff *z, arena *a, const char *v)
{
    if (strcmp(v, "/") == 0) return NULL;
    kdliste l; antwort r = s_liste(z, a, eltern(a, v), &l);
    if (r.code >= 400 || r.code < 0) return NULL;
    for (int i = 0; i < l.n; i++) if (strcmp(l.e[i].name, blatt(v)) == 0) return &l.e[i];
    return NULL;
}

static antwort s_befehl(zugriff *z, arena *a, const char *befehl, const char *pfad)
{
    sz *s = z->zustand;
    if (strcmp(befehl, "NOOP") == 0) return A(a, 200, "200 NOOP OK");
    /* NEU 1.0.6: Sonys Deinstallation - Prüfungen gehen an die echte Zugriffsschicht, der Auftrag selbst nur "auf dem Papier"
     * (App-Ordner und Kachel-Infos sind danach "weg", wie nach Sonys Deinstallation) */
    if (strcmp(befehl, "SITE UNINSTALL?") == 0) return s->innen->befehl(s->innen, a, befehl, pfad);
    if (strcmp(befehl, "SITE UNINSTALL") == 0) {
        if (pfad == NULL) return A(a, 550, "550 no");
        const char *v = norm(a, pfad), *id = blatt(v);
        antwort r = s->innen->befehl(s->innen, a, "SITE UNINSTALL?", v);
        if (r.code != 200) return r;
        sl_add(s->a, &s->buch, VERB(s->a, "SITE UNINSTALL ", v));
        mt_setze(s->a, &s->weg, v, (void *)1);
        mt_setze(s->a, &s->weg, VERB(a, "/user/appmeta/", id), (void *)1);
        mt_setze(s->a, &s->weg, VERB(a, "/system_data/priv/appmeta/", id), (void *)1);
        return A(a, 200, "200 OK");
    }
    const char *v = norm(a, pfad);
    const kdatei *e = finde(z, a, v);
    int vi = mt_index(&s->virtuell, v);
    if (strcmp(befehl, "SITE CHMOD 777") == 0) {
        if (!e && vi < 0) return A(a, 550, "550 no");
        sl_add(s->a, &s->buch, VERB(s->a, "SITE CHMOD 777 ", v));
        return A(a, 200, "200 OK");
    }
    if (strcmp(befehl, "DELE") == 0) {
        if (!e || (e->ordner && !e->link)) return A(a, 550, "550 no");
        sl_add(s->a, &s->buch, VERB(s->a, "DELE ", v));
        if (vi >= 0) {               /* virtuelle Datei: aus der Tabelle nehmen */
            for (int i = vi; i < s->virtuell.n - 1; i++) s->virtuell.e[i] = s->virtuell.e[i + 1];
            s->virtuell.n--;
        } else mt_setze(s->a, &s->weg, v, (void *)1);
        return A(a, 250, "250 OK");
    }
    if (strcmp(befehl, "RMD") == 0) {
        if (!e || !e->ordner) return A(a, 550, "550 Directory not empty");
        kdliste l; s_liste(z, a, v, &l);
        int echt = 0; for (int i = 0; i < l.n; i++) if (strcmp(l.e[i].name, ".") && strcmp(l.e[i].name, "..")) echt++;
        if (echt) return A(a, 550, "550 Directory not empty");
        sl_add(s->a, &s->buch, VERB(s->a, "RMD ", v));
        mt_setze(s->a, &s->weg, v, (void *)1);
        return A(a, 250, "250 OK");
    }
    if (strcmp(befehl, "RMDA") == 0) {
        if (!e || !e->ordner || e->link) return A(a, 550, "550 no");
        sl_add(s->a, &s->buch, VERB(s->a, "RMDA ", v));
        mt_setze(s->a, &s->weg, v, (void *)1);
        return A(a, 250, "250 OK");
    }
    return A(a, 502, "502 no");
}

static void s_schliessen(zugriff *z) { (void)z; }

zugriff *zugriff_sim(zugriff *lesen)
{
    zugriff *z = calloc(1, sizeof *z);
    sz *s = calloc(1, sizeof *s);
    s->innen = lesen; s->a = arena_neu();
    s->weg = mt_neu(false); s->virtuell = mt_neu(false);
    z->art = "sim";
    z->cwd = s_cwd; z->liste = s_liste; z->lies = s_lies; z->kopf = s_kopf; z->enthaelt = s_enthaelt; z->schreibe = s_schreibe; z->befehl = s_befehl;
    z->schliessen = s_schliessen; z->zustand = s;
    return z;
}

const sliste *zugriff_sim_buch(zugriff *sim) { return &((sz *)sim->zustand)->buch; }
