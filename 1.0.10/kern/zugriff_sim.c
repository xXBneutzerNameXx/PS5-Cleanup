/* PS5 Cleanup - gemeinsamer C-Kern
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
/* Codex-Prüfung 73: wird ein Pfad "gelöscht", gehen auch die "geschriebenen" Dateien darin mit (vorher blieben sie lesbar) */
static void virtuell_weg(sz *s, const char *v)
{
    size_t l = strlen(v);
    int m = 0;
    for (int i = 0; i < s->virtuell.n; i++) {
        const char *k = s->virtuell.e[i].k;
        if (strncmp(k, v, l) == 0 && (k[l] == 0 || k[l] == '/' || (l == 1 && v[0] == '/'))) continue;
        s->virtuell.e[m++] = s->virtuell.e[i];
    }
    s->virtuell.n = m;
}
static void weg_setzen(sz *s, arena *a, const char *v) { mt_setze(s->a, &s->weg, v, (void *)1); virtuell_weg(s, v); (void)a; }
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
/* NEU 1.0.7 (03.11.2026): stückweise lesen - an den inneren Zugriff weiterreichen (sonst ganz lesen, ein Stück) */
static antwort s_stuecke(zugriff *z, arena *a, const char *pfad, size_t stueck, bool (*weiter)(void *c, const unsigned char *d, size_t n), void *c)
{
    sz *s = z->zustand;
    const char *v = norm(a, pfad);
    sdaten *vd = mt_hole(&s->virtuell, v);
    if (stueck == 0) return A(a, 501, "501 Stück");
    if (vd) { bool los = true; for (size_t o = 0; los && o < vd->n; o += stueck) los = weiter(c, vd->d + o, vd->n - o < stueck ? vd->n - o : stueck); if (los) weiter(c, NULL, 0); return A(a, 226, "226 done"); }   /* Codex-Prüfung 60 */
    if (ist_weg(s, v)) return A(a, 550, "550 no");
    if (s->innen->stuecke) return s->innen->stuecke(s->innen, a, v, stueck, weiter, c);
    const unsigned char *d; size_t n;
    antwort r = s->innen->lies(s->innen, a, v, &d, &n);
    if (r.code < 400) { bool los = true; for (size_t o = 0; los && o < n; o += stueck) los = weiter(c, d + o, n - o < stueck ? n - o : stueck); if (los) weiter(c, NULL, 0); }
    return r;
}
/* NEU 1.0.8 (Arbeitsplan 2.1): n Bytes ab Stelle ab - an den inneren Zugriff weiterreichen */
static antwort s_lies_ab(zugriff *z, arena *a, const char *pfad, long long ab, size_t n, unsigned char *ziel, size_t *len)
{
    sz *s = z->zustand;
    const char *v = norm(a, pfad);
    *len = 0;
    if (ab < 0) return A(a, 501, "501 Stelle");
    sdaten *vd = mt_hole(&s->virtuell, v);
    if (vd) {
        if ((unsigned long long)ab < vd->n) { size_t m = vd->n - (size_t)ab; if (m > n) m = n; memcpy(ziel, vd->d + ab, m); *len = m; }
        return A(a, 226, "226 done");
    }
    if (ist_weg(s, v)) return A(a, 550, "550 no");
    if (!s->innen->lies_ab) return A(a, 502, "502 no");
    return s->innen->lies_ab(s->innen, a, v, ab, n, ziel, len);
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
        weg_setzen(s, a, v);
        weg_setzen(s, a, VERB(a, "/user/appmeta/", id));
        weg_setzen(s, a, VERB(a, "/system_data/priv/appmeta/", id));
        return A(a, 200, "200 OK");
    }
    /* NEU 1.0.7: Bilder/Videos (Sonys Löschdienst) und Spielstand-Liste - Prüfungen gehen an die echte Zugriffsschicht, der Auftrag
     * selbst nur "auf dem Papier": die Aufnahme samt .meta/.dat und Vorschaubild ist danach "weg"; beim Spielstand ändert sich an
     * den Dateien nichts (die löscht danach der Kern) */
    if (strcmp(befehl, "SITE MEDIUM?") == 0 || strcmp(befehl, "SITE SPIELSTAND?") == 0) return s->innen->befehl(s->innen, a, befehl, pfad);
    /* NEU 1.0.9: private Orte (Liste, neue Liste) - verändern keine Datei, gehen an die echte Schicht (gelesen wird auch im Testmodus als
     * Besitzer, gelöscht wird hier nur auf dem Papier) */
    if (strncmp(befehl, "SITE PRIVAT", 11) == 0) return s->innen->befehl(s->innen, a, befehl, pfad);
    /* NEU 1.0.10: nur fragen geht an die echte Schicht - beenden und austragen nie (der Kern sagt "würde") */
    if (strcmp(befehl, "SITE PLAYDEN?") == 0 || strcmp(befehl, "SITE LAEUFT?") == 0 || strcmp(befehl, "SITE PS4MOUNT?") == 0 || strcmp(befehl, "SITE PLAYDEN-PROBE") == 0
        || strcmp(befehl, "SITE KACHEL-DB?") == 0 || strcmp(befehl, "SITE ZUSATZ?") == 0) return s->innen->befehl(s->innen, a, befehl, pfad);
    /* NEU 1.0.10 (22:40): die echte Vorprüfung beweist das Schreiben mit gleichem Inhalt - im Testmodus nur die Prüfung ohne zu schreiben */
    if (strcmp(befehl, "SITE PLAYDEN-PRUEFEN") == 0 || strcmp(befehl, "SITE PLAYDEN-PRUEFEN?") == 0) return s->innen->befehl(s->innen, a, "SITE PLAYDEN-PRUEFEN?", pfad);
    if (strcmp(befehl, "SITE MEDIUM") == 0 || strcmp(befehl, "SITE SPIELSTAND") == 0) {
        if (pfad == NULL) return A(a, 550, "550 no");
        bool medium = strcmp(befehl, "SITE MEDIUM") == 0;
        const char *v = norm(a, pfad);
        antwort r = s->innen->befehl(s->innen, a, medium ? "SITE MEDIUM?" : "SITE SPIELSTAND?", v);
        if (r.code != 200) return r;
        if (!finde(z, a, v)) return A(a, 550, "550 no");
        sl_add(s->a, &s->buch, VERB(s->a, befehl, " ", v));
        if (medium) {
            const char *bl = blatt(v), *pk = strrchr(bl, '.');
            const char *stamm = pk && pk > bl ? a_kopie_n(a, v, (size_t)(pk - v)) : v;
            weg_setzen(s, a, v);
            weg_setzen(s, a, VERB(a, stamm, ".meta"));
            weg_setzen(s, a, VERB(a, stamm, ".dat"));
            weg_setzen(s, a, VERB(a, "/user/av_contents/thumbnails/", v + 18, ".jxr"));
        }
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
        } else weg_setzen(s, a, v);
        return A(a, 250, "250 OK");
    }
    if (strcmp(befehl, "RMD") == 0) {
        if (!e || !e->ordner) return A(a, 550, "550 Directory not empty");
        kdliste l; s_liste(z, a, v, &l);
        int echt = 0; for (int i = 0; i < l.n; i++) if (strcmp(l.e[i].name, ".") && strcmp(l.e[i].name, "..")) echt++;
        if (echt) return A(a, 550, "550 Directory not empty");
        sl_add(s->a, &s->buch, VERB(s->a, "RMD ", v));
        weg_setzen(s, a, v);
        return A(a, 250, "250 OK");
    }
    if (strcmp(befehl, "RMDA") == 0) {
        if (!e || !e->ordner || e->link) return A(a, 550, "550 no");
        sl_add(s->a, &s->buch, VERB(s->a, "RMDA ", v));
        weg_setzen(s, a, v);
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
    z->stuecke = s_stuecke;
    z->lies_ab = s_lies_ab;   /* NEU 1.0.8 */
    z->schliessen = s_schliessen; z->zustand = s;
    return z;
}

const sliste *zugriff_sim_buch(zugriff *sim) { return &((sz *)sim->zustand)->buch; }
