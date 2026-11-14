/* PS5 Cleanup - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * kern_bloecke.c - NEU 1.0.8 (Arbeitsplan 2.6, Entscheidung da_Chris86 04.11.2026 17:03 und 17:04, doku/alt/1.0.8/PLAN_1.0.8.md Punkt 6):
 * Blöcke für die Auswahl. Ein Block ist eine Kachel (Spiel, App, Tool, Trophäen-Liste ...) mit allen Zeilen, die im selben Bereich
 * darunter stehen, und der Größe aller Teile zusammen. Standard ist "größte zuerst", im Options-Menü A–Z.
 * Bis 1.0.7 stand diese Ordnung in der Oberfläche (app.c ordnung/kopf_von, PS5-Tests 01.11. und 02.11.2026). Jetzt steht sie hier,
 * damit sie am PC getestet wird und die neue Auswahl (Phase 4.3: zugeklappte Apps mit Gesamtgröße, "1–6 von 39") darauf aufbaut.
 * Nur Anzeige: was gelöscht wird, ändert sich dadurch nicht.
 */
#include "kern.h"
#include <string.h>

/* Spielnummer (CUSA/PPSA und 5 Ziffern, frei stehend) im Text oder im ersten Pfad. Ohne Regex und ohne Arena: läuft auch beim
 * Tastendruck in der Oberfläche. Wie bis 1.0.7 in app.c. */
static bool alnum(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }
static bool kennung_von(const eintrag *f, char out[10])
{
    const char *q[2] = { f->TextBasis ? f->TextBasis : "", f->PfadeBasis.n > 0 ? f->PfadeBasis.e[0] : "" };
    for (int s = 0; s < 2; s++) for (const char *p = q[s]; *p; p++) {
        if (strncmp(p, "CUSA", 4) && strncmp(p, "PPSA", 4)) continue;
        if (p > q[s] && alnum(p[-1])) continue;
        int d = 0; while (d < 5 && p[4 + d] >= '0' && p[4 + d] <= '9') d++;
        if (d == 5 && !alnum(p[9])) { memcpy(out, p, 9); out[9] = 0; return true; }
    }
    return false;
}

/* Kopf eines Teils im selben Bereich (wie bis 1.0.7 kopf_von in app.c):
 *   Spieldatei -> ihre ShadowMount-Kachel; Daten und Payload-Dateien -> ihre Tool-Kachel (seit 02.11.2026 auch Trophäen-Liste -> erste Liste,
 *   Fortschritt -> Liste); Update/Zusatzinhalt -> ihr Spiel (gleiche Gruppe); im Bereich Jailbreak: Spielstand, App-Daten,
 *   Zusatzinhalt-Infos -> die Kachel mit derselben Kennung (da_Chris86 01.11.2026 23:38, YouTube und YouTube-Spielstand).
 *   NEU 1.0.8 (2.3): Payload-Datei eines Tools ohne Kachel -> sein Daten-Ordner (Begleit, kern_suche.c tools_ohne_kachel).
 * Nur wenn genau EIN Kopf passt. */
eintrag *kern_kopf_von(kern *k, const eintrag *f, int kat)
{
    if (f->SmDatei && f->SmKachel && f->SmKachel->Kat == kat) return f->SmKachel;
    if (kern_bereiche_106()) return NULL;
    if (f->ToolKachel && f->ToolKachel->Kat == kat) return f->ToolKachel;
    if (f->Begleit && f->Begleit->Kat == kat) return f->Begleit;   /* NEU 1.0.8 (2.3): Payload-Datei unter dem Daten-Ordner (Tool ohne Kachel) */
    eintrag *h = NULL; int n = 0;
    if (f->Teil && strcmp(f->Teil, "Spiel") && f->Gruppe) {   /* Update / Zusatzinhalt */
        for (int i = 0; i < k->Funde.n; i++) { eintrag *g = k->Funde.e[i];
            if (g != f && g->Kat == kat && ist_teil(g, "Spiel") && g->Gruppe && !strcmp(g->Gruppe, f->Gruppe)) { h = g; n++; } }
        return n == 1 ? h : NULL;
    }
    char id[10];
    if (kat == 7 && !(f->KachelId && f->KachelId[0]) && !f->SmDatei && !ist_teil(f, "Spiel") && kennung_von(f, id)) {
        for (int i = 0; i < k->Funde.n; i++) { eintrag *g = k->Funde.e[i];
            if (g != f && g->Kat == 7 && g->KachelId && !strcmp(g->KachelId, id)) { h = g; n++; } }
        return n == 1 ? h : NULL;
    }
    return NULL;
}

/* ---- A–Z ----
 * Wie man es von einem Namensverzeichnis kennt (DIN 5007): Groß/klein egal, Ä wie A, ß wie ss, Akzente wie der Grundbuchstabe,
 * Zahlen nach ihrem Wert ("Teil 2" vor "Teil 10"). ®, © und ™ zählen nicht, ’ wie ', – und — wie -. Satzzeichen ganz vorne
 * ("'Splosion Man", "[Beta] Tool") zählen nicht. Sind zwei Namen danach gleich, entscheiden die Bytes (immer dieselbe Reihenfolge). */
static const char AZ_LATIN1[] = "aaaaaaaceeeeiiiidnooooo*ouuuuytsaaaaaaaceeeeiiiidnooooo*ouuuuyty";   /* U+00C0 bis U+00FF */
static unsigned az_dekodieren(const unsigned char **p)
{
    const unsigned char *s = *p;
    unsigned c = *s++, u = c;
    if (c >= 0xC2 && c <= 0xDF && (s[0] & 0xC0) == 0x80) { u = (c & 0x1Fu) << 6 | (s[0] & 0x3Fu); s += 1; }
    else if (c >= 0xE0 && c <= 0xEF && (s[0] & 0xC0) == 0x80 && (s[1] & 0xC0) == 0x80) { u = (c & 0x0Fu) << 12 | (s[0] & 0x3Fu) << 6 | (s[1] & 0x3Fu); s += 2; }
    else if (c >= 0xF0 && c <= 0xF4 && (s[0] & 0xC0) == 0x80 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        u = (c & 0x07u) << 18 | (s[0] & 0x3Fu) << 12 | (s[1] & 0x3Fu) << 6 | (s[2] & 0x3Fu); s += 3; }
    *p = s;                                                    /* kaputtes UTF-8: Byte für Byte */
    return u;
}
static bool az_leise(unsigned u) { return u == 0xAE || u == 0xA9 || u == 0x2122 || u == 0xAD || u == 0x200B || u == 0xFEFF; }   /* ® © ™, unsichtbar */
/* Codex-Prüfung 73: ® © ™ an der Stelle überspringen, BEVOR auf eine Zahl geprüft wird - sonst war "A®2" < "A3" < "A10" < "A®2" */
static void az_leise_weg(const unsigned char **p)
{
    for (;;) { const unsigned char *q = *p; if (!*q) return; if (!az_leise(az_dekodieren(&q))) return; *p = q; }
}
/* nächstes Zeichen für A–Z (0 = Ende); *zweites bekommt den zweiten Buchstaben von ß, Æ, Œ, Þ */
static unsigned az_zeichen(const unsigned char **p, unsigned *zweites)
{
    *zweites = 0;
    for (;;) {
        if (!**p) return 0;
        unsigned u = az_dekodieren(p);
        if (az_leise(u)) continue;
        if (u >= 'A' && u <= 'Z') return u + 32;
        if (u >= 0xC0 && u <= 0xFF && u != 0xD7 && u != 0xF7) {
            if (u == 0xDF) *zweites = 's';
            else if (u == 0xC6 || u == 0xE6) *zweites = 'e';
            else if (u == 0xDE || u == 0xFE) *zweites = 'h';
            return (unsigned char)AZ_LATIN1[u - 0xC0];
        }
        switch (u) {
        case 0x152: case 0x153: *zweites = 'e'; return 'o';  /* Œ œ */
        case 0x160: case 0x161: return 's';                   /* Š š */
        case 0x17D: case 0x17E: return 'z';                   /* Ž ž */
        case 0x141: case 0x142: return 'l';                   /* Ł ł */
        case 0x178: return 'y';                               /* Ÿ */
        case 0x2018: case 0x2019: case 0x2032: return '\'';   /* ‘ ’ ′ */
        case 0x201C: case 0x201D: return '"';                 /* “ ” */
        case 0x2013: case 0x2014: return '-';                 /* – — */
        case 0xA0: return ' ';                                /* geschütztes Leerzeichen */
        default: return u;
        }
    }
}
static bool az_ziffer(unsigned char c) { return c >= '0' && c <= '9'; }
static bool az_vorne_weg(unsigned char c) { return c < 0x80 && !alnum((char)c); }   /* Satzzeichen und Leerzeichen ganz vorne */
int kern_vergleich_namen(const char *x, const char *y)
{
    const char *x0 = x ? x : "", *y0 = y ? y : "";
    const unsigned char *p = (const unsigned char *)x0, *q = (const unsigned char *)y0;
    while (*p && az_vorne_weg(*p)) p++;
    while (*q && az_vorne_weg(*q)) q++;
    unsigned p2 = 0, q2 = 0;
    for (;;) {
        if (!p2) az_leise_weg(&p);
        if (!q2) az_leise_weg(&q);
        if (!p2 && !q2 && az_ziffer(*p) && az_ziffer(*q)) {   /* Zahlen nach Wert, führende Nullen zählen nicht */
            while (*p == '0' && az_ziffer(p[1])) p++;
            while (*q == '0' && az_ziffer(q[1])) q++;
            const unsigned char *pe = p, *qe = q;
            while (az_ziffer(*pe)) pe++;
            while (az_ziffer(*qe)) qe++;
            if (pe - p != qe - q) return pe - p < qe - q ? -1 : 1;
            int c = memcmp(p, q, (size_t)(pe - p));
            if (c) return c < 0 ? -1 : 1;
            p = pe; q = qe;
            continue;
        }
        unsigned a, b;
        if (p2) { a = p2; p2 = 0; } else a = az_zeichen(&p, &p2);
        if (q2) { b = q2; q2 = 0; } else b = az_zeichen(&q, &q2);
        if (a != b) return a < b ? -1 : 1;
        if (!a) break;
    }
    int c = strcmp(x0, y0);
    return c < 0 ? -1 : c > 0;
}

/* ---- Blöcke ---- */
/* Reihenfolge der Teile in einem Block: unter einer Trophäen-Liste zuerst die weiteren Listen, dann der Fortschritt (Codex-Prüfung 62,
 * 03.11.2026); innerhalb davon größte zuerst, sobald der ganze Bereich gezählt ist - vorher in Fundreihenfolge */
/* Codex-Prüfung 83 (P3): PKG-Dateien nach den übrigen Teilen, in fester Reihenfolge Installationsdatei, Update, Zusatzinhalt (auch wenn
 * danach nach Größe sortiert wird) */
static int teil_rang(const eintrag *f)
{
    if (f->Art == ART_TR_LISTE || f->Art == ART_TR_LISTE_REST) return 0;
    if (!f->PkgArt || kern_bereiche_106()) return 1;
    return f->PkgArt == PKG_UPDATE ? 3 : f->PkgArt == PKG_DLC ? 4 : 2;
}
/* Teil v vor Teil w? Rang, dann größte zuerst (nur wenn alles gezählt ist), sonst Fundreihenfolge. Codex-Prüfung 109 (P3): eine
 * Spieldatei-Zeile, deren ShadowMount-Kachel im selben Block ein Teil ist (Orbit Store: Tool-Kachel und App über ShadowMount), wird mit
 * ihrer Kachel einsortiert (anker) und steht direkt dahinter - nicht nach eigener Größe */
static bool teil_vor(eintrag *const *e, const long long *by, const int *anker, bool alle, int v, int w)
{
    int av = anker[v], aw = anker[w];
    if (av == aw) return v == av && w != aw;
    int rv = teil_rang(e[av]), rw = teil_rang(e[aw]);
    if (rv != rw) return rv < rw;
    if (alle && by[av] != by[aw]) return by[av] > by[aw];
    return av < aw;
}
/* NEU 1.0.10 (da_Chris86 14.11.2026 07:46, Bild der Apps-Liste: "die ELF-Dateien liegen einfach so zwischen den Programmen"): unter Apps
 * (Bereich 6) stehen zuerst die Programme (Kachel oder Block mit Teilen), danach die einzelnen Programmdateien ohne Kachel (eine Zeile:
 * .elf, .self, .bin, .plugin). Innerhalb jeder Gruppe die gewählte Reihenfolge. Nur Anzeige. */
/* NEU 1.0.10 (da_Chris86 14.11.2026 08:03: "dann liste das doch gleich so: PS5 PS4 PS3"): unter Spiele (Bereich 1) zuerst PS5, dann PS4,
 * dann PS3, zuletzt was sich nicht einordnen lässt. Erkannt an der Spielkennung (PPSA = PS5, CUSA = PS4, BLES/BLUS/... = PS3): zuerst die
 * Kennung der Kachel, dann der Text der Zeile, dann der Pfad. Nur Anzeige. */
static int zeile_plattform(const eintrag *f)
{
    const char *q[3] = { f->KachelId, f->TextBasis ? f->TextBasis : f->Text, f->PfadeBasis.n > 0 ? f->PfadeBasis.e[0] : NULL };
    for (int i = 0; i < 3; i++) {
        const char *s = q[i];
        if (!s || !s[0]) continue;
        if (h_m(s, "(?i)(^|[^a-z])PPSA[-_]?\\d{5}(?!\\d)")) return 0;
        if (h_m(s, "(?i)(^|[^a-z])CUSA[-_]?\\d{5}(?!\\d)")) return 1;
        if (h_m(s, "(?i)(^|[^a-z])(BL[EUJAK][SM]|BC[EUJAK]S|NP[EUJH][AB])[-_]?\\d{5}(?!\\d)")) return 2;
    }
    return 3;
}
static int block_gruppe(const kblock *x)
{
    const eintrag *f = x->kopf;
    if (f && !kern_bereiche_106() && f->Kat == 1) {
        for (int j = 0; j < x->teile.n; j++) { int p = zeile_plattform(x->teile.e[j]); if (p < 3) return p; }
        return 3;
    }
    if (!f || kern_bereiche_106() || f->Kat != 6 || x->teile.n != 1 || f->KachelId || f->Kachel || f->PfadeBasis.n != 1) return 0;
    return h_m(f->PfadeBasis.e[0], "(?i)\\.(elf|self|bin|plugin)$") ? 1 : 0;
}
static bool block_vor(const kblock *x, const kblock *y, int sortierung, bool alle)
{
    int gx = x->gruppe, gy = y->gruppe;
    if (gx != gy) return gx < gy;
    if (sortierung == SORT_AZ) {
        int c = kern_vergleich_namen(x->name, y->name);
        if (c) return c < 0;
    }
    return alle && x->bytes > y->bytes;
}
int kern_bloecke(kern *k, arena *a, int kat, int sortierung, kblock **aus)
{
    int n = 0;
    for (int i = 0; i < k->Funde.n; i++) if (((eintrag *)k->Funde.e[i])->Kat == kat) n++;
    eintrag **e = a_hole(a, sizeof(eintrag *) * (size_t)(n + 1));
    long long *by = a_hole(a, sizeof(long long) * (size_t)(n + 1));
    bool *da = a_hole(a, sizeof(bool) * (size_t)(n + 1)), *gk = a_hole(a, sizeof(bool) * (size_t)(n + 1));
    bool alle = true;
    int m = 0;
    for (int i = 0; i < k->Funde.n && m < n; i++) {
        eintrag *f = k->Funde.e[i];
        if (f->Kat != kat) continue;
        groesse *g = kern_eintrag_groesse_in(k, a, f, true);
        e[m] = f; da[m] = g != NULL; by[m] = g ? g->Bytes : 0; gk[m] = g && g->Gekappt;
        if (!g) alle = false;
        m++;
    }
    n = m;
    /* Kopf je Zeile - bei einer Kette (Teil eines Teils) bis zum obersten Kopf; ein Kreis zählt als "steht für sich" */
    int *hk = a_hole(a, sizeof(int) * (size_t)(n + 1)), *kopf = a_hole(a, sizeof(int) * (size_t)(n + 1));
    for (int i = 0; i < n; i++) {
        eintrag *h = kern_kopf_von(k, e[i], kat);
        hk[i] = -1;
        if (h) for (int j = 0; j < n; j++) if (e[j] == h) { hk[i] = j; break; }
    }
    for (int i = 0; i < n; i++) {
        int j = hk[i], s = 0;
        while (j >= 0 && hk[j] >= 0 && s++ <= n) j = hk[j];
        kopf[i] = (j >= 0 && hk[j] < 0 && j != i) ? j : -1;
    }
    /* ein Block je Kopf, in Fundreihenfolge */
    kblock *b = a_hole(a, sizeof(kblock) * (size_t)(n + 1));
    int nb = 0;
    int *teil = a_hole(a, sizeof(int) * (size_t)(n + 1)), *anker = a_hole(a, sizeof(int) * (size_t)(n + 1));
    for (int i = 0; i < n; i++) {
        if (kopf[i] >= 0) continue;
        kblock *x = &b[nb++];
        memset(x, 0, sizeof *x);
        x->kopf = e[i]; x->groesse_da = da[i]; x->bytes = by[i]; x->gekappt = gk[i];
        pl_add(a, &x->teile, e[i]);
        int nt = 0;
        for (int j = 0; j < n; j++) if (kopf[j] == i) teil[nt++] = j;
        for (int t = 0; t < nt; t++) {                         /* Codex-Prüfung 109 (P3): Spieldatei mit ihrer Kachel (teil_vor) */
            int j = teil[t]; anker[j] = j;
            if (e[j]->SmDatei && e[j]->SmKachel) for (int u = 0; u < nt; u++) if (e[teil[u]] == e[j]->SmKachel) { anker[j] = teil[u]; break; }
        }
        for (int t = 1; t < nt; t++) {                         /* stabil: Rang, dann größte zuerst (nur wenn alles gezählt ist) */
            int v = teil[t], u = t - 1;
            while (u >= 0 && teil_vor(e, by, anker, alle, v, teil[u])) {
                teil[u + 1] = teil[u]; u--;
            }
            teil[u + 1] = v;
        }
        for (int t = 0; t < nt; t++) {
            int j = teil[t];
            pl_add(a, &x->teile, e[j]);
            x->bytes += by[j];
            if (!da[j]) x->groesse_da = false;
            if (gk[j]) x->gekappt = true;
        }
        x->name = x->kopf->BlockName ? x->kopf->BlockName : kern_name(a, x->kopf);
        x->ohne_kachel = x->kopf->OhneKachel && x->teile.n > 1;
        x->gruppe = block_gruppe(x);
    }
    /* größte zuerst erst, wenn der ganze Bereich gezählt ist (sonst springt die Liste bei jeder neuen Größe); A–Z sofort. Stabil. */
    /* 14.11.2026: immer - die Gruppen (block_gruppe) gelten auch, solange noch gezählt wird; ohne Gruppe ändert sich dann nichts (stabil) */
        for (int i = 1; i < nb; i++) {
            kblock t = b[i];
            int j = i - 1;
            while (j >= 0 && block_vor(&t, &b[j], sortierung, alle)) { b[j + 1] = b[j]; j--; }
            b[j + 1] = t;
        }
    *aus = b;
    return nb;
}
