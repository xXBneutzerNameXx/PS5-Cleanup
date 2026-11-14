/* PS5 Cleanup - Oberfläche am Fernseher
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * app.c - Bedienung mit dem DualSense, so einfach wie 1.0.5:
 *   Starten -> Konsole/Speicher prüfen -> Bereinigung auswählen -> Ergebnis kontrollieren -> Bestätigen.
 * Alle Abläufe, Fragen und Texte kommen unverändert aus kern/ablauf.c (= Fenster.cs von 1.0.5).
 * Diese Datei zeichnet nur und gibt die Antworten des Nutzers zurück.
 *
 * Fassungen (app/bauen.sh): schritt9/pruefen/simulation löschen NICHTS (ohne KERN_LOESCHEN, Zugriff = Simulation).
 * Nur die Fassung "release" (mit KERN_LOESCHEN) löscht wirklich - erst nach ausdrücklicher Bestätigung.
 *
 * Fäden: Der Hauptfaden zeichnet und liest den Controller. Suchen/Löschen läuft im Arbeits-Faden
 * (wie Starte() in 1.0.5); Fragen aus dem Arbeits-Faden warten, bis der Nutzer geantwortet hat.
 */
#include <SDL2/SDL.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>   /* NEU 1.0.8 (PS5-Test 07.11.2026 16:31): LONG_MAX (datei_stueck) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>     /* NEU 1.0.8 (Arbeitsplan 4.1): geglättete Ecken, Kreise und Linien */
#ifdef KERN_LOESCHEN
#include <fcntl.h>    /* NEU 02.11.2026 (Prüfung p8, Befund 3): Protokolldatei mit O_NOFOLLOW öffnen */
int close(int);       /* aus unistd.h - das ganze unistd.h nicht, weil app.c eine eigene Funktion dup() hat */
#endif
#include "../kern/ablauf.h"
#include "plattform.h"
#include "qrcodegen.h"        /* NEU 1.0.7 (03.11.2026): QR-Code für die freiwillige Rückmeldung (Project Nayuki, MIT) */
#include "fern.h"             /* NEU 1.0.9: Bedienung im Browser (doku/PLAN_1.0.9.md) */
#include "cJSON.h"            /* NEU 1.0.9: Zustand und Liste für den Browser als JSON */
#include <dirent.h>           /* NEU 1.0.8 (PS5-Test 07.11.2026): Bilder der Spielstände (spielstand_bild) - in jeder Fassung */

#define STB_TRUETYPE_IMPLEMENTATION
#include "../probelauf/fremd/stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG   /* NEU 1.0.9 (da_Chris86 08.11.2026 22:49): Kachelbild des Payload Managers ist ein JPEG namens icon0.png - die PS5 zeigt es, wir bisher nicht */
#define STBI_NO_STDIO
#include "../probelauf/fremd/stb_image.h"
#ifdef MITSCHNITT
#if !defined(KERN_LOESCHEN)
#error "MITSCHNITT nur in der Test-Fassung der echten Version (mit KERN_LOESCHEN)"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO                  /* geschrieben wird nur über eigene Dateien (O_EXCL, O_NOFOLLOW) */
#include "../probelauf/fremd/stb_image_write.h"   /* Sean Barrett, gemeinfrei bzw. MIT - nur für die Bildschirmbilder der Test-Fassung */
#include "../db/db_lesen.h"
#include <sys/types.h>
ssize_t write(int, const void *, size_t);   /* aus unistd.h - das ganze unistd.h nicht (eigene Funktion dup(), siehe oben) */
#endif

#define EINBINDEN(name, datei)                                              \
  __asm__(".section .rodata\n.global " #name "\n.balign 16\n" #name ":\n"   \
          ".incbin \"" datei "\"\n.global " #name "_ende\n" #name "_ende:\n" \
          ".byte 0\n.previous\n");                                          \
  extern const unsigned char name[], name##_ende[];
EINBINDEN(schrift_normal, "../probelauf/fremd/DejaVuSans.ttf")
EINBINDEN(schrift_fett, "../probelauf/fremd/DejaVuSans-Bold.ttf")
EINBINDEN(logo_png, "../probelauf/logo.png")

#define BREITE 1920
#define HOEHE 1080
#define EINTRAEGE(n) ((n) == 1 ? T(" Eintrag", " entry") : T(" Einträge", " entries"))   /* NEU 1.1 (U11): Einzahl */
#ifdef PRUEFLAUF
#include "pruefen.h"
#define VERSION_TEXT "1.0.6 Prüflauf (nur lesen)"
#elif defined(NUR_LESEN)
#define VERSION_TEXT "1.0.6 Test (nur lesen)"
#else
#ifdef KERN_LOESCHEN
#ifdef MITSCHNITT   /* NEU 1.0.8 (Arbeitsplan 2.7): Test-Fassung für den Abschlusstest - Bauzeit sichtbar, damit klar ist, welche läuft */
#define VERSION_TEXT "1.0.10 TESTLAUF · Bau " __DATE__ " " __TIME__
#define VERSION_TEXT_EN "1.0.10 TEST RUN · Build " __DATE__ " " __TIME__
#else
#define VERSION_TEXT "1.0.10"                /* 02.11.2026 (da_Chris86, finaler Plan 1.0.7 Punkt 7): ohne "Vorschau"; 06.11.2026: 1.0.8 (Arbeitsplan Phase 5); 08.11.2026: 1.0.9 */
#endif
#else
#define VERSION_TEXT "1.0.10 TEST · Bau " __DATE__ " " __TIME__   /* Testfassung - ohne Löschbefehle gebaut, löscht nie; 31.10.2026: Bauzeit sichtbar (welche Fassung läuft?) */
#define VERSION_TEXT_EN "1.0.10 TEST · Build " __DATE__ " " __TIME__   /* NEU 1.0.8: englische Oberfläche "Build" statt "Bau" */
#endif
#endif
#ifndef VERSION_TEXT_EN
#define VERSION_TEXT_EN VERSION_TEXT
#endif
#define VERSION_ANZEIGE (SP_EN ? VERSION_TEXT_EN : VERSION_TEXT)   /* NEU 1.0.8: Kopf, Protokoll-Datei und Absturzmeldung in der eingestellten Sprache */
#define VERSION_NUMMER "1.0.10"  /* NEU 1.0.9: nur die Nummer - damit vergleicht der Update-Hinweis (in allen Fassungen gleich) */

/* Farben wie 1.0.5 (Fenster.xaml) */
#define F_HG        0x141519
#define F_KARTE     0x1F2127
#define F_KARTE2    0x26282F
#define F_RAND      0x3A3E49
#define F_TEXT      0xE8E8EA
#define F_GRAU      0x9CA1AD
#define F_BLAU      0x2F6FDB
#define F_BLAU_HELL 0x7FB2FF
#define F_GRUEN     0x5CD08A
#define F_GRUEN_HG  0x173525
#define F_MINT      0x8FE0B5
#define F_GELB      0xF2C46B
#define F_GELB_HG   0x3A2E14
#define F_ORANGE    0xFF9F66
#define F_ROT       0x8B2A2A
#define F_ROT_RAND  0xAA3333
#define F_ROT_TEXT  0xFF8A8A
#define F_FOKUS     0x2B3A5C

/* =================================================================== Zeichnen */
static SDL_Surface *ziel;
static stbtt_fontinfo font[2];
typedef struct { int belegt, f, groesse, cp, w, h, xoff, yoff, vorschub; unsigned char *bild; } Glyph;
#define GLYPHEN 8192
static Glyph glyphen[GLYPHEN];
static Glyph *glyph(int f, int groesse, int cp)
{
    unsigned h = (unsigned)(cp * 2654435761u) ^ (unsigned)(groesse * 97) ^ (unsigned)f;
    for (int i = 0; i < GLYPHEN; i++) {
        Glyph *g = &glyphen[(h + i) % GLYPHEN];
        if (g->belegt && g->f == f && g->groesse == groesse && g->cp == cp) return g;
        if (!g->belegt) {
            float s = stbtt_ScaleForPixelHeight(&font[f], (float)groesse);
            int adv, lsb;
            g->belegt = 1; g->f = f; g->groesse = groesse; g->cp = cp;
            g->bild = stbtt_GetCodepointBitmap(&font[f], s, s, cp, &g->w, &g->h, &g->xoff, &g->yoff);
            if (!g->bild) { g->w = 0; g->h = 0; }   /* 1.0.6 Fix: ohne Bild (z. B. kein Speicher) nichts zeichnen statt abstürzen */
            stbtt_GetCodepointHMetrics(&font[f], cp, &adv, &lsb);
            g->vorschub = (int)(adv * s + 0.5f);
            return g;
        }
    }
    return NULL;
}
static int cp_naechstes(const char **s) { const unsigned char *p = (const unsigned char *)*s; uint32_t c = utf8_naechstes(&p); *s = (const char *)p; return (int)c; }
static int textbreite(int f, int gr, const char *t) { int b = 0; while (*t) { Glyph *g = glyph(f, gr, cp_naechstes(&t)); if (g) b += g->vorschub; } return b; }
static int clip_x0 = 0, clip_x1 = 1 << 30;   /* NEU 1.0.6: waagrechter Ausschnitt (Laufschrift) */
static int clip_y0 = 0, clip_y1 = 1 << 30;   /* NEU 1.0.8 (4.3): senkrechter Ausschnitt (Kasten der Auswahl läuft durch) */
static void text(int x, int y, int f, int gr, uint32_t farbe, const char *t)
{
    Uint8 fr = (farbe >> 16) & 255, fg = (farbe >> 8) & 255, fb = farbe & 255;
    SDL_PixelFormat *pf = ziel->format;
    while (*t) {
        Glyph *g = glyph(f, gr, cp_naechstes(&t));
        if (!g) continue;
        for (int j = 0; j < g->h; j++) {
            int py = y + g->yoff + j;
            if (py < 0 || py >= ziel->h || py < clip_y0 || py >= clip_y1) continue;
            Uint32 *zeile = (Uint32 *)((Uint8 *)ziel->pixels + py * ziel->pitch);
            for (int i = 0; i < g->w; i++) {
                int px = x + g->xoff + i;
                unsigned a = g->bild[j * g->w + i];
                if (!a || px < 0 || px >= ziel->w || px < clip_x0 || px >= clip_x1) continue;
                Uint8 r, gg, b; SDL_GetRGB(zeile[px], pf, &r, &gg, &b);
                zeile[px] = SDL_MapRGB(pf, (Uint8)((fr * a + r * (255 - a)) / 255), (Uint8)((fg * a + gg * (255 - a)) / 255), (Uint8)((fb * a + b * (255 - a)) / 255));
            }
        }
        x += g->vorschub;
    }
}
static void rechteck(int x, int y, int w, int h, uint32_t farbe)
{ SDL_Rect r = {x, y, w, h}; SDL_FillRect(ziel, &r, SDL_MapRGB(ziel->format, (farbe >> 16) & 255, (farbe >> 8) & 255, farbe & 255)); }
static void karte(int x, int y, int w, int h, uint32_t rand, uint32_t innen) { rechteck(x, y, w, h, rand); rechteck(x + 2, y + 2, w - 4, h - 4, innen); }

/* =================================================================== NEU 1.0.8 (Arbeitsplan 4.1): Grundbausteine der neuen Oberfläche
 * nach Entwurf 2 (Artefakt "PS5 Cleanup Oberfläche"): Farben, runde Ecken, Kreise und Linien geglättet, Symbole im Raster wie im Entwurf
 * (app/symbole.h, erzeugt von app/werkzeug/symbole_erzeugen.py). Alles zeichnet direkt in "ziel", ohne Speicher pro Bild. */
#include "symbole.h"
#define N_HG         0x121417   /* Hintergrund */
#define N_KARTE      0x1A1D22   /* Karten */
#define N_RAND       0x262A31   /* Rand der Karten */
#define N_FELD       0x22262D   /* Symbolfeld, Abzeichen, Knopf "aus" */
#define N_TEXT       0xF2F4F7
#define N_TEXT2      0xD5DAE1
#define N_GRAU       0xB4BBC6
#define N_GRAU2      0x8B93A1
#define N_LINIE      0x3A404A
#define N_BLAU       0x1C74E9   /* Hauptknopf, erledigte Schritte */
#define N_FOKUS      0x4CC9F0   /* türkise Markierung */
#define N_ROT        0xFF7B76
#define N_ROT_KNOPF  0xC8423E
#define N_GELB       0xE3BE6A
#define N_GRUEN      0x5DD39A
#define N_ORANGE     0xFF9A52
#define N_FUSS       0x0F1114
#define N_FUSS_LINIE 0x23272E
#define N_BALKEN     0x687080   /* belegt */
#define N_BALKEN_FREI 0x2A2F37
#define RAND_X 96               /* Ränder wie im Entwurf: 96 px links/rechts, 56 oben, 54 unten */
#define RAND_O 56
#define RAND_U 54
static inline void misch(int x, int y, uint32_t farbe, float a)
{
    if (a <= 0.f || x < 0 || y < 0 || x >= ziel->w || y >= ziel->h || x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1) return;
    if (a > 1.f) a = 1.f;
    Uint32 *p = (Uint32 *)((Uint8 *)ziel->pixels + y * ziel->pitch) + x;
    Uint8 r, g, b; SDL_GetRGB(*p, ziel->format, &r, &g, &b);
    unsigned ai = (unsigned)(a * 255.f + .5f), fr = (farbe >> 16) & 255, fg = (farbe >> 8) & 255, fb = farbe & 255;
    *p = SDL_MapRGB(ziel->format, (Uint8)((fr * ai + r * (255 - ai)) / 255), (Uint8)((fg * ai + g * (255 - ai)) / 255), (Uint8)((fb * ai + b * (255 - ai)) / 255));
}
static uint32_t farbmisch(uint32_t a, uint32_t b, float t)   /* t = Anteil von a (für halbdurchsichtige Flächen des Entwurfs) */
{
    unsigned r = (unsigned)(((a >> 16) & 255) * t + ((b >> 16) & 255) * (1.f - t) + .5f), g = (unsigned)(((a >> 8) & 255) * t + ((b >> 8) & 255) * (1.f - t) + .5f),
             bl = (unsigned)((a & 255) * t + (b & 255) * (1.f - t) + .5f);
    return (r << 16) | (g << 8) | bl;
}
/* gefülltes Rechteck mit runden Ecken (Radius r) */
static void rund(int x, int y, int w, int h, int r, uint32_t farbe)
{
    if (w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r <= 0) { rechteck(x, y, w, h, farbe); return; }
    rechteck(x + r, y, w - 2 * r, h, farbe);
    rechteck(x, y + r, r, h - 2 * r, farbe);
    rechteck(x + w - r, y + r, r, h - 2 * r, farbe);
    for (int dy = 0; dy < r; dy++) for (int dx = 0; dx < r; dx++) {
        float ex = (float)r - ((float)dx + .5f), ey = (float)r - ((float)dy + .5f);
        float a = (float)r - sqrtf(ex * ex + ey * ey) + .5f;
        if (a <= 0.f) continue;
        misch(x + dx, y + dy, farbe, a); misch(x + w - 1 - dx, y + dy, farbe, a);
        misch(x + dx, y + h - 1 - dy, farbe, a); misch(x + w - 1 - dx, y + h - 1 - dy, farbe, a);
    }
}
/* Karte wie im Entwurf: Rand d px, runde Ecken */
static void karte_rund(int x, int y, int w, int h, int r, int d, uint32_t rand, uint32_t innen)
{
    if (d <= 0) { rund(x, y, w, h, r, innen); return; }
    rund(x, y, w, h, r, rand); rund(x + d, y + d, w - 2 * d, h - 2 * d, r > d ? r - d : 0, innen);
}
static void kreis(float cx, float cy, float r, uint32_t farbe)   /* gefüllt */
{
    for (int y = (int)floorf(cy - r - 1); y <= (int)ceilf(cy + r + 1); y++)
        for (int x = (int)floorf(cx - r - 1); x <= (int)ceilf(cx + r + 1); x++) {
            float dx = (float)x + .5f - cx, dy = (float)y + .5f - cy;
            misch(x, y, farbe, r - sqrtf(dx * dx + dy * dy) + .5f);
        }
}
static void ring(float cx, float cy, float r, float d, uint32_t farbe)   /* Kreislinie, r = Mitte der Linie */
{
    float h = d / 2.f;
    for (int y = (int)floorf(cy - r - h - 1); y <= (int)ceilf(cy + r + h + 1); y++)
        for (int x = (int)floorf(cx - r - h - 1); x <= (int)ceilf(cx + r + h + 1); x++) {
            float dx = (float)x + .5f - cx, dy = (float)y + .5f - cy;
            misch(x, y, farbe, h - fabsf(sqrtf(dx * dx + dy * dy) - r) + .5f);
        }
}
/* Linie mit runden Enden, Dicke d */
static void strich(float ax, float ay, float bx, float by, float d, uint32_t farbe)
{
    float h = d / 2.f, vx = bx - ax, vy = by - ay, ll = vx * vx + vy * vy;
    int x0 = (int)floorf(fminf(ax, bx) - h - 1), x1 = (int)ceilf(fmaxf(ax, bx) + h + 1), y0 = (int)floorf(fminf(ay, by) - h - 1), y1 = (int)ceilf(fmaxf(ay, by) + h + 1);
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        float px = (float)x + .5f - ax, py = (float)y + .5f - ay;
        float t = ll > 0.f ? (px * vx + py * vy) / ll : 0.f;
        if (t < 0.f) t = 0.f; else if (t > 1.f) t = 1.f;
        float qx = px - t * vx, qy = py - t * vy;
        misch(x, y, farbe, h - sqrtf(qx * qx + qy * qy) + .5f);
    }
}
/* Symbol aus app/symbole.h in ein Feld der Größe gr x gr (Raster wird auf die Höhe gr gebracht) */
static void symbol(const float *s, int x, int y, int gr, uint32_t farbe)
{
    float k = (float)gr / s[1], d = s[2] * k;
    if (d < 1.2f) d = 1.2f;
    x += (int)((gr - s[0] * k) / 2.f);   /* Raster, die breiter als hoch sind, mittig */
    s += 3;
    while (*s != 0.f) {
        int typ = (int)*s++;
        if (typ == 1) {
            int n = (int)*s++;
            for (int i = 0; i + 1 < n; i++) strich(x + s[2 * i] * k, y + s[2 * i + 1] * k, x + s[2 * i + 2] * k, y + s[2 * i + 3] * k, d, farbe);
            s += 2 * n;
        } else if (typ == 2) { ring(x + s[0] * k, y + s[1] * k, s[2] * k, d, farbe); s += 3; }
        else { kreis(x + s[0] * k, y + s[1] * k, s[2] * k + d / 2.f, farbe); s += 3; }
    }
}
static void abdunkeln(void)
{
    SDL_PixelFormat *pf = ziel->format;
    for (int y = 0; y < ziel->h; y++) {
        Uint32 *z = (Uint32 *)((Uint8 *)ziel->pixels + y * ziel->pitch);
        for (int x = 0; x < ziel->w; x++) { Uint8 r, g, b; SDL_GetRGB(z[x], pf, &r, &g, &b); z[x] = SDL_MapRGB(pf, r / 3, g / 3, b / 3); }
    }
}

/* Text mit Zeilenumbruch: liefert die Zeilen (Speicher aus fa) */
static arena *fa;                    /* Speicher eines Bildes */
typedef struct { const char **z; int n; } zeilen;
static zeilen umbrechen(int f, int gr, const char *t, int breite)
{
    zeilen r = { a_hole(fa, sizeof(char *) * 4096), 0 };
    const char *absatz = t ? t : "";
    for (;;) {
        const char *ende = strchr(absatz, '\n'); size_t len = ende ? (size_t)(ende - absatz) : strlen(absatz);
        char *a = a_kopie_n(fa, absatz, len);
        /* führende Leerzeichen bleiben (Einrückung wie in 1.0.5) */
        const char *p = a;
        if (!*p && r.n < 4096) r.z[r.n++] = "";
        while (*p && r.n < 4096) {
            const char *start = p, *letzt_leer = NULL, *letzt_strich = NULL, *letzt_bind = NULL, *q = p; int b = 0;
            while (*q) {
                const char *v = q; int c = cp_naechstes(&q);
                Glyph *g = glyph(f, gr, c); b += g ? g->vorschub : 0;
                if (b > breite && v > start) { q = v; break; }
                if (c == ' ') letzt_leer = q;
                else if (c == '/') letzt_strich = q;
                else if (c == '-' && v > start && v[-1] != ' ' && *q && *q != ' ') letzt_bind = q;   /* nur mitten im Wort (Codex-Prüfung 127) */
            }
            if (*q && letzt_leer && letzt_leer > start) q = letzt_leer;
            /* NEU 1.0.9 (da_Chris86 08.11.2026, Bild aus 1.0.8: "Aufnahme-Zwischenordn" / "er"): hat die Zeile kein Leerzeichen, bricht ein Wort
             * mit Bindestrich nach dem Bindestrich um ("Aufnahme-" / "Zwischenordner") statt mitten im Wort - nur ein Bindestrich mitten im Wort
             * (Codex-Prüfung 127) und nicht in einem Pfad (dort wie in 1.0.8 nach dem "/"). Text mit Leerzeichen bricht wie bisher um
             * (Vorschau Kette 38: sonst "Jailbreak-Start: WebKit-" / "Autoloader" statt "Jailbreak-Start:" / "WebKit-Autoloader"). */
            else if (*q && letzt_bind && !letzt_strich && letzt_bind - start > 1) q = letzt_bind;
            /* NEU 1.0.8 (Arbeitsplan 4.6): ein Pfad ohne Leerzeichen bricht nach einem "/" um ("/system_data/priv/appmeta/" + "ORBT00001")
             * statt mitten im Wort - nur wenn die Zeile kein Leerzeichen hat, Text mit Leerzeichen bricht wie bisher um */
            else if (*q && letzt_strich && letzt_strich - start > 1) q = letzt_strich;
            r.z[r.n++] = a_kopie_n(fa, start, (size_t)(q - start));
            p = q; while (*p == ' ' && p != start) p++;
        }
        if (!ende) break;
        absatz = ende + 1;
    }
    return r;
}
/* NEU 1.0.8 (4.6): Zeilen ab "ab" wieder zu einem Text zusammensetzen, für die letzte Zeile, die gekürzt werden muss. umbrechen lässt
 * das Leerzeichen am Zeilenende stehen - früher kamen dadurch doppelte Leerzeichen hinein ("unbekannt  (Spiel"). Ein Leerzeichen kommt
 * nur dorthin, wo auch eins war (nicht nach einem "/" oder mitten im Wort). */
static const char *zeilen_rest(const zeilen *z, int ab)
{
    /* Codex-Prüfung 78: in einem Zug zusammensetzen - vorher wurde für jede Zeile der ganze bisherige Text neu kopiert (bei einem sehr
     * langen Namen aus param.json viele MB je Bild) */
    size_t gesamt = 1;
    for (int j = ab; j < z->n; j++) gesamt += strlen(z->z[j]) + 1;
    char *s = a_hole(fa, gesamt); size_t n = 0; bool leer = false;
    for (int j = ab; j < z->n; j++) {
        size_t l = strlen(z->z[j]), m = l;
        while (m > 0 && z->z[j][m - 1] == ' ') m--;
        if (j > ab && leer) s[n++] = ' ';
        memcpy(s + n, z->z[j], m); n += m;
        leer = m < l;
    }
    s[n] = 0;
    return s;
}
/* NEU 1.0.7 (da_Chris86 01.11.2026: "Überall, bei der ganzen App ist das so" - abgeschnittene Texte): PC-Prüfung. In der
 * PC-Vorschau (-DVORSCHAU) kommt jeder Text, der nicht in seinen Platz passt, in die Ausgabe ("ZU LANG ..."). Die PS5-Fassungen
 * sind davon unberührt. */
#ifdef VORSCHAU
static void zu_lang(const char *wie, int gr, int breite, const char *t) { printf("ZU LANG (%s, Schrift %d, Platz %d Pixel): %s\n", wie, gr, breite, t); }
#else
#define zu_lang(wie, gr, breite, t) ((void)0)
#endif
/* einzeilig, zu lang -> "…" */
static const char *kuerzen(int f, int gr, const char *t, int breite)
{
    if (textbreite(f, gr, t) <= breite) return t;
    zu_lang("gekürzt", gr, breite, t);
    int el = textbreite(f, gr, "…");
    if (el > breite) return "";   /* Codex-Prüfung 85: nicht einmal "…" passt - dann nichts, statt über den Rand */
    const char *q = t; int b = 0;
    while (*q) { const char *v = q; Glyph *g = glyph(f, gr, cp_naechstes(&q)); b += g ? g->vorschub : 0; if (b + el > breite) { q = v; break; } }
    return VERB(fa, a_kopie_n(fa, t, (size_t)(q - t)), "…");
}
/* NEU 1.0.7 (da_Chris86 01.11.2026 17:50, Foto: Text im Jailbreak-Kasten abgeschnitten): erst kleiner schreiben (in 2er-Schritten bis
 * gr_min), erst dann kürzen */
static void text_passend(int x, int y, int f, int gr, int gr_min, uint32_t farbe, const char *t, int breite)
{
    while (gr - 2 >= gr_min && textbreite(f, gr, t) > breite) gr -= 2;
    text(x, y, f, gr, farbe, kuerzen(f, gr, t, breite));
}
/* NEU 1.0.6 (da_Chris86 28.10.2026: lange Spielnamen): in der markierten Zeile läuft ein zu langer Text langsam durch -
 * 1,5 s Pause am Anfang, mit 90 Pixeln pro Sekunde bis zum Ende, 1,5 s Pause, dann wieder von vorn */
static const void *lauf_wer; static Uint32 lauf_seit; static bool lauf_aktiv;
/* NEU 1.0.8 (Arbeitsplan 4.7): die PS5 zeichnet über den Prozessor - darum nur neu zeichnen, wenn sich etwas bewegt. Laufschrift und
 * durchlaufende Kästen stehen am Anfang und am Ende still (1,5 s bzw. 3 s); in diesen Pausen wird nicht gezeichnet. lauf_naechst sagt
 * der Hauptschleife, wann sich das Nächste wieder bewegt. */
static Uint32 lauf_naechst;
static int lauf_versatz(Uint32 seit, int weg, int px_s, Uint32 halt)
{
    Uint32 jetzt = SDL_GetTicks(), lauf = (Uint32)weg * 1000 / (Uint32)px_s, zyklus = halt + lauf + halt, z = (jetzt - seit) % zyklus;
    Uint32 naechst = z < halt ? jetzt + (halt - z) : z < halt + lauf ? jetzt : jetzt + (zyklus - z);
    if (!lauf_aktiv || (Sint32)(naechst - lauf_naechst) < 0) lauf_naechst = naechst;
    lauf_aktiv = true;
    return z < halt ? 0 : z < halt + lauf ? (int)((z - halt) * (Uint32)px_s / 1000) : weg;
}
static void laufschrift(int x, int y, int f, int gr, uint32_t farbe, const char *t, int breite, const void *wer)
{
    int tw = textbreite(f, gr, t);
    if (tw <= breite) { text(x, y, f, gr, farbe, t); return; }
    zu_lang("Laufschrift", gr, breite, t);
    if (wer != lauf_wer) { lauf_wer = wer; lauf_seit = SDL_GetTicks(); }
    int off = lauf_versatz(lauf_seit, tw - breite + 20, 90, 1500);   /* 1,5 s stehen, 90 Pixel pro Sekunde, 1,5 s stehen */
    clip_x0 = x; clip_x1 = x + breite;
    text(x - off, y, f, gr, farbe, t);
    clip_x0 = 0; clip_x1 = 1 << 30;
}
static int absatz(int x, int y, int f, int gr, uint32_t farbe, const char *t, int breite, int zeilenhoehe)
{
    zeilen z = umbrechen(f, gr, t, breite);
    for (int i = 0; i < z.n; i++) text(x, y + i * zeilenhoehe, f, gr, farbe, z.z[i]);
    return z.n * zeilenhoehe;
}
/* NEU 1.0.7 (da_Chris86 01.11.2026 17:50, Foto: Warntext in der Auswahl-Leiste lief übereinander): höchstens "max" Zeilen,
 * der Rest kommt gekürzt ("…") in die letzte Zeile */
static int absatz_max(int x, int y, int f, int gr, uint32_t farbe, const char *t, int breite, int zeilenhoehe, int max)
{
    zeilen z = umbrechen(f, gr, t, breite);
    int n = z.n < max ? z.n : max;
    if (z.n > max) zu_lang("zu viele Zeilen", gr, breite, t);
    for (int i = 0; i < n; i++) {
        const char *s = z.z[i];
        if (i == n - 1 && z.n > max) s = kuerzen(f, gr, zeilen_rest(&z, i), breite);   /* 1.0.8: ohne doppelte Leerzeichen */
        text(x, y + i * zeilenhoehe, f, gr, farbe, s);
    }
    return n * zeilenhoehe;
}

/* NEU 1.0.7 (da_Chris86 01.11.2026 abends: "Um was bringen mir die ganzen Zusatztexte, wenn die Hälfte davon abgeschnitten wird?
 * Überall, bei der ganzen App ist das so."): Text ganz zeigen statt "…" - erst etwas kleiner auf einer Zeile (bis gr1), sonst ab gr1
 * auf bis zu max_z Zeilen umbrechen (bis gr_min). Passt es auch so nicht, meldet die PC-Prüfung das (zu_lang). lh = Zeilenabstand. */
typedef struct { int gr, lh; zeilen z; } passend;
static passend einpassen(int f, int gr, int gr1, int gr_min, const char *t, int breite, int max_z)
{
    passend p = {0}; t = t ? t : "";
    for (int g = gr; g >= gr1; g--) if (textbreite(f, g, t) <= breite) { p.gr = g; p.lh = g + g / 4 + 1; p.z = umbrechen(f, g, t, breite); return p; }
    for (int g = gr1; g >= gr_min; g--) { p.gr = g; p.lh = g + g / 4 + 1; p.z = umbrechen(f, g, t, breite); if (p.z.n <= max_z) return p; }
    zu_lang("passt auch umgebrochen nicht", gr_min, breite, t);
    /* NEU 1.0.7 (da_Chris86 02.11.2026 morgens, Prüfung Punkt 6): nur in diesem Ausnahmefall kürzen - höchstens max_z Zeilen, die
     * letzte mit "…", damit nichts über den Kartenrand in die Fußzeile läuft. In der PC-Prüfung kommt der Fall nicht vor. */
    if (max_z > 0 && p.z.n > max_z) {
        p.z.z[max_z - 1] = kuerzen(f, p.gr, zeilen_rest(&p.z, max_z - 1), breite);   /* 1.0.8: ohne doppelte Leerzeichen */
        p.z.n = max_z;
    }
    return p;
}
static void passend_zeichnen(int x, int y, int f, uint32_t farbe, const passend *p)   /* y = Grundlinie der ersten Zeile */
{
    for (int i = 0; i < p->z.n; i++) text(x, y + i * p->lh, f, p->gr, farbe, p->z.z[i]);
}

static SDL_Surface *logo;
static void logo_laden(void)
{
    int w, h, n;
    unsigned char *px = stbi_load_from_memory(logo_png, (int)(logo_png_ende - logo_png), &w, &h, &n, 4);
    if (!px) return;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(px, w, h, 32, w * 4, SDL_PIXELFORMAT_ABGR8888);
    if (s) { logo = SDL_ConvertSurface(s, ziel->format, 0); SDL_FreeSurface(s); }
    stbi_image_free(px);
}

/* NEU 1.0.7 (da_Chris86 01.11.2026: "Kachelbilder ... Bilder für 1.0.7"): das Bild der Kachel (icon0.png aus den Kachel-Infos)
 * statt der zwei Buchstaben. Nur lesen ("rb"). Geladen wird nur, was gerade zu sehen ist - höchstens zwei neue Bilder pro
 * gezeichnetem Bild (kein Ruckeln), verkleinert gespeichert (40 Pixel je Kennung, 150 Pixel nur für den markierten Eintrag).
 * Gibt es kein Bild, bleiben die zwei Buchstaben. */
static unsigned char *datei_lesen(const char *pfad, size_t *n)
{
    char p[1024]; int pl;
#ifdef VORSCHAU
    const char *w = getenv("VORSCHAU_WURZEL"); pl = snprintf(p, sizeof p, "%s%s", w ? w : "", pfad);
#else
    pl = snprintf(p, sizeof p, "%s", pfad);
#endif
    if (pl < 0 || (size_t)pl >= sizeof p) return NULL;          /* Codex-Prüfung 132: nie einen gekürzten Pfad öffnen */
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    unsigned char *d = NULL; long l = -1;
    if (fseek(f, 0, SEEK_END) == 0) l = ftell(f);
    if (l > 0 && l <= 4L * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0 && (d = malloc((size_t)l)) != NULL && fread(d, 1, (size_t)l, f) != (size_t)l) { free(d); d = NULL; }
    fclose(f);
    if (d) *n = (size_t)l;
    return d;
}
static SDL_Surface *bild_verkleinert(const unsigned char *px, int w, int h, int s)   /* Flächenmittel, auf den Kartengrund gelegt */
{
    SDL_Surface *o = SDL_CreateRGBSurfaceWithFormat(0, s, s, 32, SDL_PIXELFORMAT_ABGR8888);
    if (!o) return NULL;
    const unsigned hr = (F_KARTE2 >> 16) & 255, hg = (F_KARTE2 >> 8) & 255, hb = F_KARTE2 & 255;
    for (int y = 0; y < s; y++) for (int x = 0; x < s; x++) {
        int x0 = x * w / s, x1 = (x + 1) * w / s, y0 = y * h / s, y1 = (y + 1) * h / s;
        if (x1 <= x0) x1 = x0 + 1;
        if (y1 <= y0) y1 = y0 + 1;
        unsigned long r = 0, g = 0, b = 0, a = 0, n = 0;
        for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) {
            const unsigned char *q = px + ((size_t)yy * (size_t)w + (size_t)xx) * 4;
            r += (unsigned long)q[0] * q[3]; g += (unsigned long)q[1] * q[3]; b += (unsigned long)q[2] * q[3]; a += q[3]; n++;
        }
        unsigned long am = a / n;
        Uint8 *z = (Uint8 *)o->pixels + y * o->pitch + x * 4;
        z[0] = (Uint8)((r / n + hr * (255 - am)) / 255); z[1] = (Uint8)((g / n + hg * (255 - am)) / 255); z[2] = (Uint8)((b / n + hb * (255 - am)) / 255); z[3] = 255;
    }
    SDL_Surface *c = SDL_ConvertSurface(o, ziel->format, 0);
    SDL_FreeSurface(o);
    return c;
}
/* NEU 1.0.7 (Prüfung 02.11.2026): Abmessungen vor dem Entpacken prüfen - eine kaputte icon0.png mit riesiger Breite könnte sonst beim
 * Verkleinern über den Puffer hinaus lesen. icon0.png ist normal 512 x 512. Gibt d immer frei. */
static unsigned char *bild_entpacken(unsigned char *d, size_t n, int *w, int *h)
{
    int iw = 0, ih = 0, ic = 0;
    if (!d || n == 0 || n > 4u * 1024 * 1024 || !stbi_info_from_memory(d, (int)n, &iw, &ih, &ic) || iw <= 0 || ih <= 0 || iw > 4096 || ih > 4096) { free(d); return NULL; }
    /* NEU 1.0.9 (Codex-Prüfung 145, P2): JPEG (Kachelbild des Payload Managers) nur bis 1024 x 1024 Pixel und 2 MB - ein großes,
     * progressives JPEG bräuchte beim Entpacken ein Vielfaches an Speicher und Zeit im Hauptfaden. Kachelbilder sind 512 oder 1024 groß. */
    if (n >= 2 && d[0] == 0xFF && d[1] == 0xD8 && (n > 2u * 1024 * 1024 || iw > 1024 || ih > 1024)) { free(d); return NULL; }
    int c; unsigned char *px = stbi_load_from_memory(d, (int)n, w, h, &c, 4);
    free(d);
    if (px && *w > 0 && *h > 0) {
        /* NEU 1.0.9 (PS5-Test 09.11.2026 18:58, Patch Manager ILNY26228: icon0.png 128 x 128, nur Schwarz): ein Bild aus einer einzigen
         * Farbe zeigt nichts - dann wie ohne Bild (das Symbol bzw. die nächste Bildquelle), am Fernseher und im Browser */
        const uint32_t *q = (const uint32_t *)px; size_t m = (size_t)*w * (size_t)*h, i = 1;
        while (i < m && q[i] == q[0]) i++;
        if (i < m) return px;
    }
    if (px) stbi_image_free(px);
    return NULL;
}
/* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86: "wenn das Spiel nicht mehr da ist, zumindest noch das Bild"): Bilder ohne Kachel, nur gelesen.
 *  - Trophäen-Liste eines PS4-Spiels: ICON0.PNG aus /user/trophy/conf/<NPWR>/TROPHY.TRP (Kopf "DC A2 4D 00", Einträge zu 64 Byte ab 0x60:
 *    Name, Stelle, Größe, Kennzeichen - genommen wird nur ein unverschlüsselter Eintrag, Kennzeichen 0)
 *  - Spielstände: das Bild, das Sony zu jedem Spielstand ablegt (/user/home/<Benutzer>/savedata_meta/user/<Kennung>/<Name>_icon0.png) */
static uint32_t bild_be32(const unsigned char *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static unsigned long long bild_be64(const unsigned char *p) { return (unsigned long long)bild_be32(p) << 32 | bild_be32(p + 4); }
static unsigned char *datei_stueck(const char *pfad, unsigned long long ab, size_t n)
{
    char p[1024]; int pl;
#ifdef VORSCHAU
    const char *w = getenv("VORSCHAU_WURZEL"); pl = snprintf(p, sizeof p, "%s%s", w ? w : "", pfad);
#else
    pl = snprintf(p, sizeof p, "%s", pfad);
#endif
    if (pl < 0 || (size_t)pl >= sizeof p) return NULL;          /* Codex-Prüfung 132: nie einen gekürzten Pfad öffnen */
    /* NEU 1.0.8 (PS5-Test 07.11.2026 16:31): das Bild einer Spieldatei liegt oft weit hinten im Abbild - bis LONG_MAX (PS5: 64 Bit) */
    if (n == 0 || n > 4u * 1024 * 1024 || ab > (unsigned long long)LONG_MAX) return NULL;
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    unsigned char *d = malloc(n);
    if (d && (fseek(f, (long)ab, SEEK_SET) != 0 || fread(d, 1, n, f) != n)) { free(d); d = NULL; }
    fclose(f);
    return d;
}
static unsigned char *trp_bild(const char *npwr, size_t *n)
{
    char p[160]; snprintf(p, sizeof p, "/user/trophy/conf/%s/TROPHY.TRP", npwr);
    unsigned char *k = datei_stueck(p, 0, 0x60);
    if (!k) return NULL;
    uint32_t kennung = bild_be32(k), anz = bild_be32(k + 0x10), gr = bild_be32(k + 0x14);
    free(k);
    if (kennung != 0xDCA24D00u || anz == 0 || anz > 4096 || gr != 0x40) return NULL;
    unsigned char *tab = datei_stueck(p, 0x60, (size_t)anz * 0x40), *bild = NULL;
    for (uint32_t i = 0; tab && i < anz; i++) {
        const unsigned char *e = tab + (size_t)i * 0x40;
        if (memcmp(e, "ICON0.PNG", 10) != 0) continue;
        unsigned long long ab = bild_be64(e + 32), len = bild_be64(e + 40);
        if (bild_be32(e + 48) != 0 || len < 8 || len > 4u * 1024 * 1024) break;
        bild = datei_stueck(p, ab, (size_t)len);
        if (bild && memcmp(bild, "\x89PNG", 4) != 0) { free(bild); bild = NULL; }
        if (bild) *n = (size_t)len;
        break;
    }
    free(tab);
    return bild;
}
/* NEU 1.0.8 (PS5-Test 07.11.2026 16:31): Bild einer PS5-Trophäen-Liste aus /user/trophy2/nobackup/conf/<NPWR>/TROPHY.UCP - Aufbau wie
 * TROPHY.TRP (Kopf "B2 28 C6 0A", Gesamtlänge, Zahl der Einträge, Eintragsgröße 0x40; je Eintrag Name, Stelle, Größe, Kennzeichen 0), die
 * Bilder offen als PNG (auf da_Chris86s PS5 nachgesehen). Genommen wird icon0_<Sprache der Oberfläche>.png, sonst en-US, en-GB, icon0.png,
 * sonst das erste andere icon0*.png. Nur lesen. */
static unsigned char *ucp_bild(const char *npwr, size_t *n)
{
    char p[160]; snprintf(p, sizeof p, "/user/trophy2/nobackup/conf/%s/TROPHY.UCP", npwr);
    unsigned char *k = datei_stueck(p, 0, 0x60);
    if (!k) return NULL;
    uint32_t kennung = bild_be32(k), anz = bild_be32(k + 0x10), gr = bild_be32(k + 0x14);
    unsigned long long gesamt = bild_be64(k + 0x08);
    free(k);
    if (kennung != 0xB228C60Au || anz == 0 || anz > 4096 || gr != 0x40 || gesamt < 0x60 + (unsigned long long)anz * 0x40) return NULL;
    unsigned char *tab = datei_stueck(p, 0x60, (size_t)anz * 0x40), *bild = NULL;
    if (!tab) return NULL;
    const char *wunsch[4] = { T("icon0_de-DE.png", "icon0_en-US.png"), "icon0_en-US.png", "icon0_en-GB.png", "icon0.png" };
    int beste = 99; unsigned long long ab = 0, len = 0;
    for (uint32_t i = 0; i < anz; i++) {
        const unsigned char *e = tab + (size_t)i * 0x40;
        char name[33]; memcpy(name, e, 32); name[32] = 0;
        size_t nl = strlen(name);
        int rang = -1;
        for (int w = 0; w < 4; w++) if (!strcmp(name, wunsch[w])) { rang = w; break; }
        if (rang < 0 && nl > 9 && !strncmp(name, "icon0", 5) && !strcmp(name + nl - 4, ".png")) rang = 4;
        if (rang < 0 || rang >= beste || bild_be32(e + 48) != 0) continue;
        unsigned long long a = bild_be64(e + 32), l = bild_be64(e + 40);
        if (l < 8 || l > 4u * 1024 * 1024 || a < 0x60 || a > gesamt || l > gesamt - a) continue;
        beste = rang; ab = a; len = l;
    }
    free(tab);
    if (beste == 99) return NULL;
    bild = datei_stueck(p, ab, (size_t)len);
    if (bild && memcmp(bild, "\x89PNG", 4) != 0) { free(bild); bild = NULL; }
    if (bild) *n = (size_t)len;
    return bild;
}
static kern *K;
/* NEU 1.0.8 (PS5-Test 07.11.2026 16:31): Spieldatei oder Spiel-Ordner ohne Kachel - das Bild aus dem Spiel selbst. Wo es liegt, hat die
 * Suche gemerkt (BILD_ORT: "Pfad\nAnfang\nLänge", Länge 0 = die ganze Datei icon0.png eines Spiel-Ordners); gelesen wird es nur hier.
 * NEU 1.0.9 (#144): vierte Zeile "pfsc <Anfang> <Länge>" - das Bild liegt in einem gepackten Abbild (.ffpfsc), Anfang ist dann die Stelle
 * im entpackten Abbild; gelesen wird es über kern_pfsc (nur die nötigen 64-KB-Blöcke). Zerlegt wird BILD_ORT seit der Codex-Prüfung 130
 * im Kern (kern_bild_ort_lesen, dort im Test geprüft). NEU #149: Zeile "teile ..." - das Bild liegt in Stücken; gepackt oder in Stücken
 * liest es der Kern (kern_bild_lesen, ebenfalls im Test geprüft). */
static unsigned char *spiel_bild(const char *id, size_t *n)
{
    kern_bild_ort b;
    if (!K || !mt_hat(&K->BILD_ORT, id) || !kern_bild_ort_lesen(mt_hole(&K->BILD_ORT, id), &b)) return NULL;
    size_t m = 0; unsigned char *d;
    if (b.gepackt || b.teile_n) {                               /* NEU 1.0.9: im gepackten Abbild und/oder in Stücken (#149) */
        char p[1100]; int pl;
#ifdef VORSCHAU
        const char *w = getenv("VORSCHAU_WURZEL"); pl = snprintf(p, sizeof p, "%s%s", w ? w : "", b.pfad);
#else
        pl = snprintf(p, sizeof p, "%s", b.pfad);
#endif
        if (pl < 0 || (size_t)pl >= sizeof p) return NULL;          /* Codex-Prüfung 131: nie einen gekürzten Pfad öffnen */
        d = kern_bild_lesen(p, &b); m = (size_t)b.len;
    }
    else if (b.len == 0) d = datei_lesen(b.pfad, &m);
    else { if (b.len < 8 || b.len > 4u * 1024 * 1024) return NULL; d = datei_stueck(b.pfad, b.ab, (size_t)b.len); m = (size_t)b.len; }
    if (d && (m < 8 || memcmp(d, "\x89PNG", 4) != 0)) { free(d); d = NULL; }
    if (d) *n = m;
    return d;
}
static unsigned char *spielstand_bild(const char *id, size_t *n)
{
    const char *wz = "";
#ifdef VORSCHAU
    { const char *v = getenv("VORSCHAU_WURZEL"); if (v) wz = v; }
#endif
    char hp[1024]; snprintf(hp, sizeof hp, "%s/user/home", wz);
    DIR *hd = opendir(hp); if (!hd) return NULL;
    struct dirent *he; unsigned char *bild = NULL; int nh = 0;
    static const char *const SD[2] = { "savedata_meta", "savedata_prospero_meta" };
    while (!bild && (he = readdir(hd)) != NULL && nh < 16) {
        if (he->d_name[0] == '.') continue;
        nh++;
        for (int s = 0; s < 2 && !bild; s++) {
            char o[1024]; snprintf(o, sizeof o, "%s/user/home/%s/%s/user/%s", wz, he->d_name, SD[s], id);
            DIR *od = opendir(o); if (!od) continue;
            char best[256] = ""; struct dirent *oe; int no = 0;
            while ((oe = readdir(od)) != NULL && no++ < 256) {
                size_t l = strlen(oe->d_name);
                if (l < 9 || l >= sizeof best || strcmp(oe->d_name + l - 9, "icon0.png") != 0) continue;
                if (!best[0] || strcmp(oe->d_name, best) < 0) snprintf(best, sizeof best, "%s", oe->d_name);   /* immer dasselbe Bild */
            }
            closedir(od);
            if (!best[0]) continue;
            char p[1024]; snprintf(p, sizeof p, "/user/home/%s/%s/user/%s/%s", he->d_name, SD[s], id, best);
            bild = datei_lesen(p, n);
        }
    }
    closedir(hd);
    return bild;
}
static unsigned char *kachelbild_laden(const char *id, int *w, int *h)
{
    if (!strncmp(id, "NPWR", 4)) {   /* NEU 1.0.8: PS4 (TROPHY.TRP), sonst PS5 (TROPHY.UCP, PS5-Test 07.11.2026 16:31) */
        size_t n = 0; unsigned char *d = trp_bild(id, &n);
        if (!d) d = ucp_bild(id, &n);
        return d ? bild_entpacken(d, n, w, h) : NULL;
    }
    /* NEU 1.0.8 (F5, da_Chris86s Bilder vom 04.11.2026): auch Kacheln, deren App unter /system_ex/app liegt (z. B. Startordner von Tools), und
     * Apps auf M.2 oder erweitertem Speicher */
    const char *wege[6] = { "/user/appmeta/%s/icon0.png", "/system_data/priv/appmeta/%s/icon0.png", "/user/app/%s/sce_sys/icon0.png",
                            "/system_ex/app/%s/sce_sys/icon0.png", "/mnt/ext1/user/app/%s/sce_sys/icon0.png", "/mnt/ext0/user/app/%s/sce_sys/icon0.png" };
    for (int i = 0; i < 6; i++) {
        char p[256]; snprintf(p, sizeof p, wege[i], id);
        size_t n = 0; unsigned char *d = datei_lesen(p, &n);
        if (!d) continue;
        unsigned char *px = bild_entpacken(d, n, w, h);
        if (px) return px;
    }
    /* NEU 1.0.8 (PS5-Test 07.11.2026 16:31): ohne Kachel das Bild aus der Spieldatei bzw. dem Spiel-Ordner (spiel_bild) */
    { size_t n = 0; unsigned char *d = spiel_bild(id, &n); if (d) { unsigned char *px = bild_entpacken(d, n, w, h); if (px) return px; } }
    /* NEU 1.0.8 (PS5-Test 07.11.2026): ohne Kachel das Bild eines Spielstands derselben Kennung (meist das Bild des Spiels) */
    if (!strncmp(id, "CUSA", 4) || !strncmp(id, "PPSA", 4)) { size_t n = 0; unsigned char *d = spielstand_bild(id, &n); if (d) return bild_entpacken(d, n, w, h); }
    return NULL;
}
#define BILDER_MAX 512
static struct { char id[16]; SDL_Surface *b; } bilder[BILDER_MAX];
static int bilder_n = 0, bilder_neu = 0;          /* bilder_neu: in diesem Bild schon geladen */
static char gross_id[16]; static SDL_Surface *gross_b; static int gross_s;
static const char *bild_kennung(const eintrag *f)
{
    if (f->KachelId && f->KachelId[0]) return f->KachelId;
    if (f->SmDatei && f->SmKachel && f->SmKachel->KachelId) return f->SmKachel->KachelId;
    const char *tx = f->TextBasis ? f->TextBasis : (f->Text ? f->Text : "");
    /* NEU 1.0.8 (Codex-Prüfungen 111 bis 113): Spieldatei oder Spiel-Ordner ohne Kachel - die Kennung, zu der der gelesene Name gehört
     * ("[Spiel: Name (PPSA…)]", kern_suche.c spiel_zusatz), geht vor jeder Nummer im Dateinamen und vor der Kachel eines Blocks, unter dem
     * die Datei steht. Nur bei Spieldateien; genommen wird der letzte solche Zusatz (er steht hinter dem Dateinamen), er endet vor dem
     * nächsten "  [" oder am Textende - im Namen selbst steht nie "  [" (spiel_zusatz fasst Leerzeichen zusammen) */
    htreffer m;
    if (f->Art == ART_ABBILD) {
        m = h_mt(fa, tx, "^.*\\[(?:Spiel|game): .*?\\(([A-Z]{4}\\d{5})\\)\\](?=  \\[|\\s*$)");   /* PS5-Test 17:33: auch FAKE77031 (Playden) */
        if (m.ok) return m.gr[1];
        /* NEU 1.0.10 (da_Chris86 14.11.2026 07:58, PS3-Spiele als ISO ohne Bild): die Kennung aus dem Dateinamen, groß und ohne Trennzeichen - so legt die Suche das Bild ab (kern_suche.c iso_ps3) */
        /* Codex-Pruefung 262: nur PS3-ISOs, Kennung aus dem Dateinamen selbst (nicht aus einem Ordnernamen davor) */
        const char *ps3b = (strstr(tx, "[PS3-Spiel, ISO]") || strstr(tx, "[PS3 game, ISO]")) && f->PfadeBasis.n > 0 ? strrchr(f->PfadeBasis.e[0], 47) : NULL;
        m = h_mt(fa, ps3b ? ps3b : "", "(?i)(?:^|[^a-z])(BL[EUJAK][SM]|BC[EUJAK]S|NP[EUJH][AB])[-_]?(\\d{5})(?!\\d)");
        if (m.ok) {
            char *kid = (char *)a_fmt(fa, "%s%s", m.gr[1], m.gr[2]);
            for (char *c = kid; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 'a' + 'A');
            return kid;
        }
    }
    if (f->ToolKachel && f->ToolKachel->KachelId) return f->ToolKachel->KachelId;
    m = h_mt(fa, tx, "\\b((?:CUSA|PPSA)\\d{5})\\b");
    if (m.ok) return m.gr[1];
    if (h_m(tx, "\\bPSCL0000[12]\\b")) return "PSCL00001";   /* NEU 1.0.8 (F4): Aufnahmen von PS5 Cleanup - das Bild der eigenen Kachel */
    m = h_mt(fa, tx, "\\b(NPWR\\d{5}_\\d{2})\\b");   /* Trophaeen: Kachel des Spiels (npbind.dat) */
    if (m.ok && K && mt_hat(&K->NPWR_KACHEL, m.gr[1])) return (const char *)mt_hole(&K->NPWR_KACHEL, m.gr[1]);
    if (m.ok) return m.gr[1];   /* NEU 1.0.8 (PS5-Test 07.11.2026): ohne Kachel das Bild aus der Trophäen-Datei (kachelbild_laden) */
    /* NEU 1.0.8 (F5): steht die Kennung nicht im Text (z. B. "Grand Theft Auto V – Spielstände"), dann die aus dem Ordner (savedata/CUSA…) -
     * nur wenn alle Ordner dieselbe Kennung haben (ein Sammel-Eintrag mehrerer Spiele bekommt kein Bild eines davon) */
    const char *pid = NULL;
    for (int j = 0; j < f->PfadeBasis.n; j++) {
        m = h_mt(fa, f->PfadeBasis.e[j], "/((?:CUSA|PPSA)\\d{5})(?:/|$)");
        /* NEU 1.0.9 (da_Chris86 08.11.2026, Bild aus 1.0.8: "Leere Aufnahme-Zwischenordner" mit PPSA01650 und PSCL00002 zeigte das Bild
         * von Y2JB): ein Ordner mit einer anderen Kennung (PS5 Cleanup, Homebrew, Systemprogramm) gehört zu einem anderen Programm - dann
         * hier kein Bild (bis 1.0.8 wurde er übersprungen). Kachel, Spielname, Tool und Trophäen oben gehen weiter vor. */
        if (!m.ok) { if (h_m(f->PfadeBasis.e[j], "/[A-Z]{4}\\d{5}(?:/|$)")) return NULL; continue; }
        if (pid && strcmp(pid, m.gr[1])) return NULL;
        pid = m.gr[1];
    }
    /* NEU 1.0.10 (da_Chris86 14.11.2026 13:13: "die bekommen doch, wenn sie entpackt werden, ein Symbol"): Programm ohne Kachel - das Kachelbild aus seiner ELF-Datei (kern_suche.c elf_bild_merken) */
    if (!pid && K && f->PfadeBasis.n == 1) {
        const char *ks = kern_elf_bildschluessel(fa, f->PfadeBasis.e[0]);
        /* Codex-Prüfung 264: nur wenn das gemerkte Bild wirklich aus genau dieser Datei stammt (gleicher Schlüssel bei anderem Pfad) */
        const char *bo = mt_hat(&K->BILD_ORT, ks) ? (const char *)mt_hole(&K->BILD_ORT, ks) : NULL;
        size_t bl = strlen(f->PfadeBasis.e[0]);
        if (bo && !strncmp(bo, f->PfadeBasis.e[0], bl) && bo[bl] == '\n') return ks;
    }
    return pid;
}
static void neu_zeichnen(void);
static SDL_Surface *kachelbild(const eintrag *f, int s)   /* s = 44 (Zeile) oder 180 (Kasten); 1.0.8: Grenze 64 statt 40 */
{
    const char *id = bild_kennung(f);
    if (!id || strlen(id) >= sizeof gross_id) return NULL;
    if (s > 64) {
        if (!strcmp(gross_id, id) && gross_s == s) return gross_b;   /* 1.0.8: auch die Größe (der Kasten verkleinert das Bild bei langem Text) */
        /* NEU 1.0.9: läuft gerade ein Besitzer-Wechsel (private Ordner, zugriff_datei.c), erst beim nächsten Bild laden - nie warten */
        if (!zugriff_wechsel_versuchen()) { neu_zeichnen(); return NULL; }
        if (gross_b) { SDL_FreeSurface(gross_b); gross_b = NULL; }
        snprintf(gross_id, sizeof gross_id, "%s", id); gross_s = s;
        int w, h; unsigned char *px = kachelbild_laden(id, &w, &h);
        zugriff_wechsel_frei();
        if (px) { gross_b = bild_verkleinert(px, w, h, s); stbi_image_free(px); }
        return gross_b;
    }
    for (int i = 0; i < bilder_n; i++) if (!strcmp(bilder[i].id, id)) return bilder[i].b;
    if (bilder_n >= BILDER_MAX) return NULL;
    if (bilder_neu >= 2) { neu_zeichnen(); return NULL; }   /* der Rest kommt mit dem nächsten Bild */
    if (!zugriff_wechsel_versuchen()) { neu_zeichnen(); return NULL; }   /* NEU 1.0.9: wie oben */
    bilder_neu++;
    int w, h; unsigned char *px = kachelbild_laden(id, &w, &h);
    zugriff_wechsel_frei();
    snprintf(bilder[bilder_n].id, sizeof bilder[bilder_n].id, "%s", id);
    bilder[bilder_n].b = px ? bild_verkleinert(px, w, h, s) : NULL;
    if (px) stbi_image_free(px);
    return bilder[bilder_n++].b;
}

/* =================================================================== gemeinsamer Zustand (Arbeits-Faden <-> Oberfläche) */
typedef enum { D_KEINER, D_WAHL, D_TEXT, D_FERTIG, D_FEHLER } dialogart;
static struct {
    pthread_mutex_t m; pthread_cond_t c;
    dialogart art; char *titel, *text, *sicher, *gef; bool beantwortet, wahl; char eingabe[256]; bool eingabe_ok;
    bool bestaetigen;               /* NEU 1.0.8 (Codex-Prüfung 76): die Frage ist "Endgültig löschen?" - unter G.m gesetzt, ohne Text lesbar */
    char *f_kopf, *f_frei, *f_sm; bool f_ok;
    char stand[1024]; double prozent; bool unbestimmt;
    char **log; int log_n, log_cap;
    time_t *log_zeit;              /* NEU 1.0.7: Uhrzeit je Zeile (für die Protokoll-Datei) */
    volatile bool beschaeftigt;
    volatile bool neu;
    volatile int dlg_nr;            /* jede neue Frage zählt hoch -> Fokus immer zuerst auf dem sicheren Knopf */
} G = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER };

static ablauf A;                     /* (Vorab-Deklaration, steht weiter unten) */
static void rm_loeschen_merken(void);   /* NEU 1.0.7: QR-Rückmeldung (steht weiter unten) */
static char *dup(const char *s) { return strdup(s ? s : ""); }
static void neu_zeichnen(void) { __atomic_store_n(&G.neu, true, __ATOMIC_RELEASE); }   /* Codex-Prüfung 78: kommt auch aus anderen Fäden - atomar */
/* NEU 1.0.8 (Arbeitsplan 4.7): Protokollzeilen, Stand und gezählte Größen kommen aus anderen Fäden, beim Nachschauen und beim Zählen der
 * Größen oft viele pro Sekunde. Bisher wurde für jede gleich neu gezeichnet (bis zu 60 Bilder pro Sekunde, alles über den Prozessor) -
 * jetzt gesammelt, höchstens alle 100 ms. Tasten, neue Fragen und die Liste zeichnen weiter sofort. */
static bool neu_bald;                /* Codex-Prüfung 78: nur atomar lesen und schreiben (mehrere Fäden) */
static void neu_zeichnen_bald(void) { __atomic_store_n(&neu_bald, true, __ATOMIC_RELEASE); }
static Uint32 bild_zeit;             /* wann zuletzt gezeichnet wurde */

/* NEU 1.0.7 (da_Chris86 31.10.2026 19:27/19:28): Protokoll als Datei - Zeile für Zeile (kostet keinen Speicher, bei einem Absturz
 * ist alles bis dahin gesichert). Seit 02.11.2026 (da_Chris86 15:44/15:45) beginnt die Datei erst direkt vor dem ersten Löschen
 * (protokoll_beginnen): prot_stick_f auf dem USB-Stick, wenn einer steckt, sonst prot_f im Ordner /data/PS5-Cleanup-Protokolle (nicht mehr im
 * Ordner der eigenen Kachel - der geht bei der Selbstentfernung weg). □ in der Protokoll-Ansicht speichert jederzeit auf den Stick.
 * Nur die echte Fassung schreibt. Alles unter G.m. */
static FILE *prot_f, *prot_stick_f;
static char prot_intern_pfad[200], prot_stick_pfad[200];
/* NEU 1.0.7 (da_Chris86 31.10.2026 21:50, Punkt 5): Test- und Simulationsfassungen schreiben gar nichts - die schreibenden
 * Teile sind nur in der echten Fassung (KERN_LOESCHEN) eingebaut; sonst gibt es sie nicht einmal im Programm. */
static void prot_datei_zeile(FILE *f, time_t z, const char *t)
{
#ifdef KERN_LOESCHEN
    struct tm tm; localtime_r(&z, &tm);
    fprintf(f, "%02d:%02d:%02d %s\r\n", tm.tm_hour, tm.tm_min, tm.tm_sec, t ? t : "");
    fflush(f);
#else
    (void)f; (void)z; (void)t;
#endif
}
#ifdef KERN_LOESCHEN
static void prot_datei_kopf(FILE *f)   /* Kopf + alles bisherige (unter G.m) */
{
    time_t j = time(NULL); struct tm tm; localtime_r(&j, &tm);
    fprintf(f, "===== PS5 Cleanup %s (%s) - %04d-%02d-%02d %02d:%02d:%02d =====\r\n", VERSION_ANZEIGE, plattform_name(),
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    for (int i = 0; i < G.log_n; i++) prot_datei_zeile(f, G.log_zeit[i], G.log[i]);
}
#endif
#ifdef MITSCHNITT
/* NEU 1.0.8 (Arbeitsplan 2.7, Absprache da_Chris86 05.11.2026): nur die Test-Fassung für den Abschlusstest - nie in der veröffentlichten
 * Version (start/bauen_release.sh prüft das). Alles kommt in den Ordner PS5-Cleanup-Test auf dem USB-Laufwerk (sonst /data/PS5-Cleanup-Test):
 *  - mitschnitt_<Zeit>.txt: jede Frage an das Dateisystem samt Antwort, Befehle samt Antwort von Sony, Zustand vor und nach dem Entfernen
 *    einer Kachel (Ordner, app.db, appinfo.db), alle Zeilen des Protokolls, Zeilen aus appinfo.db, freier Speicher, Konsole. Format und
 *    PC-Nachbau: kern/zugriff_mitschnitt.c, test/kern/test_mitschnitt.c ("zeige").
 *  - bild_<Nr>_<Zeit>.png: ein Bild von jedem Bildschirm - nach jedem Tastendruck, bei jeder neuen Frage, sonst höchstens alle 15 s, nur
 *    wenn sich etwas verändert hat. Selbst gespeichert, ohne Sonys Screenshot-Funktion (Sonys Einblendungen sind darauf nicht zu sehen).
 * Der Ordner wird nie zum Löschen angeboten (K->TEST_ORDNER). */
static const char MS_MARKE[] = "PS5CLEANUP-MITSCHNITT-TESTFASSUNG";
static FILE *ms_f; static char ms_ordner[160];
/* Codex-Prüfung 74: Geschrieben wird nur von einem eigenen Faden. Wer eine Zeile abgibt (Oberfläche, Löschen, Bilder), legt sie nur in einen
 * Puffer (höchstens 8 MB) und wartet nie auf das Laufwerk - hängt es, gehen Zeilen verloren (gezählt), die Oberfläche läuft weiter.
 * Höchstens 2 GB Text; unter 500 MB frei oder bei einem Schreibfehler schreibt der Mitschnitt nicht weiter (letzte Zeile sagt warum). */
#define MS_PUF_MAX (8u << 20)
#define MS_TEXT_MAX (2048LL << 20)
#define MS_FREI_MIN (500LL << 20)
static pthread_mutex_t ms_m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ms_mc = PTHREAD_COND_INITIALIZER;
static char *ms_puf; static size_t ms_puf_n, ms_puf_cap;
static bool ms_schreibt, ms_aus; static long long ms_verloren;
/* Codex-Prüfung 75: eine Aufzeichnung (z. B. eine Ordnerliste) kommt mit allen Folgezeilen in einem Aufruf - gezählt werden die Zeilen */
static long long ms_zeilen(const char *z, size_t n) { long long k = 0; for (size_t i = 0; i < n; i++) if (z[i] == '\n') k++; return k ? k : 1; }
/* zwingend: die Schlusszeile (ENDE) kommt auch in einen vollen Puffer - sie ist kurz */
static void ms_schreib_x(const char *z, bool zwingend)
{
    size_t n = strlen(z);
    pthread_mutex_lock(&ms_m);
    if (ms_f && !ms_aus) {
        if (ms_puf_n + n > MS_PUF_MAX && !zwingend) ms_verloren += ms_zeilen(z, n);
        else {
            if (ms_puf_n + n > ms_puf_cap) {
                size_t nc = ms_puf_cap ? ms_puf_cap : 65536;
                while (nc < ms_puf_n + n) nc *= 2;
                char *np = realloc(ms_puf, nc);
                if (!np) { ms_verloren += ms_zeilen(z, n); pthread_mutex_unlock(&ms_m); return; }
                ms_puf = np; ms_puf_cap = nc;
            }
            memcpy(ms_puf + ms_puf_n, z, n); ms_puf_n += n;
            pthread_cond_broadcast(&ms_mc);
        }
    }
    pthread_mutex_unlock(&ms_m);
}
static void ms_schreib(void *c, const char *z) { (void)c; ms_schreib_x(z, false); }
static void *ms_schreib_faden(void *x)
{
    (void)x;
    char *b = NULL; size_t bcap = 0;
    long long gemeldet = 0, geschrieben = 0, pruefen_ab = 16LL << 20;
    pthread_mutex_lock(&ms_m);
    for (;;) {
        while (ms_puf_n == 0 && ms_verloren == gemeldet) pthread_cond_wait(&ms_mc, &ms_m);
        char *t = ms_puf; size_t n = ms_puf_n, tc = ms_puf_cap;
        ms_puf = b; ms_puf_cap = bcap; ms_puf_n = 0; b = t; bcap = tc;   /* Puffer tauschen - geschrieben wird ohne Sperre */
        long long neu_verloren = ms_verloren - gemeldet; gemeldet = ms_verloren;
        FILE *f = ms_f; bool aus = ms_aus;
        ms_schreibt = true;
        pthread_mutex_unlock(&ms_m);
        const char *grund = NULL;
        if (f && !aus) {
            bool ok = true;
            if (neu_verloren > 0) { char v[128]; snprintf(v, sizeof v, "I VERLOREN%%20%lld%%20Zeilen%%20(Schreiben%%20kam%%20nicht%%20nach)\n", neu_verloren); ok = fputs(v, f) >= 0; }
            ok = ok && (n == 0 || fwrite(b, 1, n, f) == n) && fflush(f) == 0;
            geschrieben += (long long)n;
            if (!ok) grund = "Schreibfehler";
            else if (geschrieben > MS_TEXT_MAX) grund = "2 GB Text erreicht";
            else if (geschrieben >= pruefen_ab) {
                pruefen_ab = geschrieben + (16LL << 20);
                long long fr = plattform_frei_bytes(ms_ordner);
                if (fr >= 0 && fr < MS_FREI_MIN) grund = "unter 500 MB frei";
            }
            if (grund) { arena *a = arena_neu(); fputs(zugriff_mitschnitt_zeile(a, 'I', VERB(a, "MITSCHNITT AUS: ", grund), NULL, NULL), f); fflush(f); arena_frei(a); }
        }
        pthread_mutex_lock(&ms_m);
        ms_schreibt = false;
        if (grund) ms_aus = true;
        pthread_cond_broadcast(&ms_mc);
    }
    return NULL;
}
static void ms_zeile(char typ, const char *t1, const char *t2, const char *t3)
{
    if (!ms_f) return;
    arena *a = arena_neu();
    ms_schreib(NULL, zugriff_mitschnitt_zeile(a, typ, t1, t2, t3));
    arena_frei(a);
}
static void ms_speicher(const char *wann);   /* weiter unten */
#endif
/* NEU 1.0.8 (Codex-Prüfungen 74/75): der Ordner der Test-Fassung an allen Orten, an denen plattform_test_ordner ihn anlegen kann (Wege wie im
 * Kern) - in JEDER Fassung nie angeboten und nie gelöscht, damit auch die normale Version Mitschnitt und Bilder des Tests in Ruhe lässt */
static const char *const TEST_ORDNER_ORTE[] = { "/user/data/PS5-Cleanup-Test", "/mnt/usb0/PS5-Cleanup-Test", "/mnt/usb1/PS5-Cleanup-Test", "/mnt/usb2/PS5-Cleanup-Test",
                                                "/mnt/usb3/PS5-Cleanup-Test", "/mnt/usb4/PS5-Cleanup-Test", "/mnt/usb5/PS5-Cleanup-Test", "/mnt/usb6/PS5-Cleanup-Test",
                                                "/mnt/usb7/PS5-Cleanup-Test" };
static void ui_log(void *c, const char *t)
{
    (void)c;
    pthread_mutex_lock(&G.m);
    time_t jetzt = time(NULL);
    if (G.log_n == G.log_cap) {   /* Codex-Prüfung 84: realloc kann scheitern - dann wird die Zeile nur nicht gemerkt (Datei und Ausgabe bleiben) */
        int cap = G.log_cap ? G.log_cap * 2 : 256;
        char **l = realloc(G.log, sizeof(char *) * (size_t)cap);
        if (l) G.log = l;
        time_t *z = l ? realloc(G.log_zeit, sizeof(time_t) * (size_t)cap) : NULL;
        if (z) G.log_zeit = z;
        if (l && z) G.log_cap = cap;
    }
    if (G.log_n < G.log_cap) { G.log_zeit[G.log_n] = jetzt; G.log[G.log_n++] = dup(t); }
    if (prot_f) prot_datei_zeile(prot_f, jetzt, t);
    if (prot_stick_f) prot_datei_zeile(prot_stick_f, jetzt, t);
    pthread_mutex_unlock(&G.m);
    printf("PS5 Cleanup: %s\n", t ? t : ""); fflush(stdout);
    plattform_protokoll_zeile(t);                  /* NEU 1.0.7: auch ins Kernel-Protokoll */
    fern_zeile(t);                                 /* NEU 1.0.9: auch für die Seite "Protokoll" im Browser */
#ifdef MITSCHNITT
    ms_zeile('P', t ? t : "", NULL, NULL);         /* NEU 1.0.8 (2.7): jede Zeile des Protokolls auch in den Mitschnitt */
#endif
    neu_zeichnen_bald();                           /* 1.0.8 (4.7) */
}
static void ui_meldung(const char *t) { ui_log(NULL, t); }   /* NEU 1.0.7: Zeilen der Plattform (Sony-Deinstallation) */
/* Protokoll auf den USB-Stick: neue Datei mit Datum und Uhrzeit, danach wird jede Zeile dort angehängt.
 * Rückgabe true = gespeichert (Meldung in m). Nie überschreiben: gibt es den Namen schon, wird nichts geschrieben. */
static bool protokoll_auf_stick_innen(char *m, size_t ml);
static bool protokoll_auf_stick(char *m, size_t ml)   /* Prüfung p8, Befund 7: Haupt- und Arbeitsfaden nie gleichzeitig */
{
    static pthread_mutex_t sm = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&sm);
    bool r = protokoll_auf_stick_innen(m, ml);
    pthread_mutex_unlock(&sm);
    return r;
}
static bool protokoll_auf_stick_innen(char *m, size_t ml)
{
#ifndef KERN_LOESCHEN
    snprintf(m, ml, "%s", T("Test-/Simulationsfassung: es wird nichts gespeichert - das Protokoll steht nur hier auf dem Bildschirm.", "Test/simulation version: nothing is saved - the log is only shown here on screen."));
    return false;
#else
    pthread_mutex_lock(&G.m);
    bool schon = prot_stick_f != NULL;
    pthread_mutex_unlock(&G.m);
    if (schon) { snprintf(m, ml, "%s%s", T("Schon auf dem USB-Laufwerk gespeichert: ", "Already saved on the USB drive: "), prot_stick_pfad); return true; }
    const char *o = plattform_stick_ordner();
    if (!o) { snprintf(m, ml, "%s", T("Kein USB-Laufwerk gefunden - das Protokoll bleibt auf der PS5.", "No USB drive found - the log stays on the PS5.")); return false; }
    long long fr = plattform_frei_bytes(o);       /* da_Chris86 02.11.2026 17:23: Stick voll -> interne Platte */
    if (fr >= 0 && fr < PLATTFORM_PROT_MIN_FREI) { snprintf(m, ml, "%s", T("Das USB-Laufwerk ist voll - dort wird kein Protokoll gespeichert.", "The USB drive is full - no log is saved there.")); return false; }
    time_t j = time(NULL); struct tm tm; localtime_r(&j, &tm);
    char p[200];
    snprintf(p, sizeof p, "%s/ps5cleanup_protokoll_%04d-%02d-%02d_%02d%02d%02d.txt", o, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    /* Prüfung p8, Befund 3: neu anlegen oder gar nicht (O_EXCL), nie einem Verweis folgen (O_NOFOLLOW) */
    /* Codex 02.11.2026: O_APPEND - der Hintergrundteil hängt schon an, während die Oberfläche noch schreibt; ohne würde sie seine Zeilen überschreiben */
    zugriff_wechsel_sperren();   /* NEU 1.0.9: nie mitten in einem Besitzer-Wechsel anlegen */
    int fd = open(p, O_WRONLY | O_CREAT | O_EXCL | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0644);
    { int e = errno; zugriff_wechsel_frei(); errno = e; }
    if (fd < 0 && errno == EEXIST) { snprintf(m, ml, "%s", T("Auf dem USB-Laufwerk gibt es diese Datei schon - bitte gleich nochmal versuchen.", "This file already exists on the USB drive - please try again in a moment.")); return false; }
    FILE *f = fd >= 0 ? fdopen(fd, "a") : NULL;
    if (!f) { int e = errno; if (fd >= 0) close(fd); snprintf(m, ml, "%s%s", T("Speichern auf dem USB-Laufwerk ging nicht: ", "Saving to the USB drive failed: "), strerror(e)); return false; }
    pthread_mutex_lock(&G.m);
    prot_datei_kopf(f);
    prot_stick_f = f;
    snprintf(prot_stick_pfad, sizeof prot_stick_pfad, "%s", p);
    pthread_mutex_unlock(&G.m);
    if (K) K->EIGENES_PROTOKOLL = prot_stick_pfad;   /* NEU 1.0.7 (02.11.2026): beim nächsten Nachschauen nicht zum Löschen anbieten (Codex-Prüfung 65: PROTOKOLL_JETZT bleibt das interne) */
    plattform_protokoll_stick_setzen(p);          /* der Hintergrundteil hängt bei der Selbstentfernung seine Zeilen an */
    snprintf(m, ml, "%s%s", T("Protokoll auf dem USB-Laufwerk gespeichert: ", "Log saved on the USB drive: "), p);
    return true;
#endif
}
static void ui_stand(void *c, const char *t, double p)
{
    (void)c;
    pthread_mutex_lock(&G.m);
    if (t) snprintf(G.stand, sizeof G.stand, "%s", t);
    if (p == ABLAUF_UNBESTIMMT) G.unbestimmt = true;
    else if (p >= 0) { G.unbestimmt = false; G.prozent = p; }
    pthread_mutex_unlock(&G.m);
    neu_zeichnen_bald();                           /* 1.0.8 (4.7) */
#ifdef VORSCHAU   /* NEU 1.0.8 (4.5): PC-Vorschau - Löschen sichtbar langsam, damit Bilder der Seite "Löschen läuft" möglich sind */
    static int langsam = -1; if (langsam < 0) { const char *e = getenv("VORSCHAU_LANGSAM"); langsam = e ? atoi(e) : 0; }
    if (langsam > 0 && t && (txt_beginnt(t, T("Lösche Eintrag ", "Deleting entry ")) || txt_beginnt(t, T("SIMULATION · prüfe Eintrag ", "SIMULATION · checking entry ")))) SDL_Delay((Uint32)langsam);
    if (langsam > 0 && t && p == ABLAUF_UNBESTIMMT && txt_beginnt(t, T("Schaue nach, was auf der PS5 liegt", "Scanning what is on the PS5"))) SDL_Delay((Uint32)langsam * 20);   /* Seite "Nachschauen läuft" */
#endif
}
static void warte_antwort(void)
{
    G.dlg_nr++; G.beantwortet = false; neu_zeichnen();
    while (!G.beantwortet) pthread_cond_wait(&G.c, &G.m);
    G.art = D_KEINER; neu_zeichnen();
}
static void dialog_frei(void) { free(G.titel); free(G.text); free(G.sicher); free(G.gef); G.titel = G.text = G.sicher = G.gef = NULL; }
static bool ui_wahl(void *c, const char *text, const char *titel, const char *sicher, const char *gef)
{
    (void)c;
    pthread_mutex_lock(&G.m);
    dialog_frei(); G.titel = dup(titel); G.text = dup(text); G.sicher = dup(sicher); G.gef = dup(gef);
    G.bestaetigen = titel && !strcmp(titel, T("Endgültig löschen?", "Delete permanently?"));
    G.art = D_WAHL; warte_antwort();
    bool w = G.wahl;
    pthread_mutex_unlock(&G.m);
    return w;
}
static char antwort_text[256];
static const char *ui_text(void *c, const char *text, const char *titel)
{
    (void)c;
    pthread_mutex_lock(&G.m);
    dialog_frei(); G.titel = dup(titel); G.text = dup(text); G.eingabe[0] = 0;
    G.bestaetigen = titel && !strcmp(titel, T("Endgültig löschen?", "Delete permanently?"));
    G.art = D_TEXT; warte_antwort();
    bool ok = G.eingabe_ok;
    /* te.Text.Trim() */
    const char *s = G.eingabe; while (*s == ' ') s++;
    snprintf(antwort_text, sizeof antwort_text, "%s", s);
    size_t l = strlen(antwort_text); while (l && antwort_text[l - 1] == ' ') antwort_text[--l] = 0;
    pthread_mutex_unlock(&G.m);
    return ok ? antwort_text : NULL;
}
static void ui_fertig(void *c, const char *kopf, bool ok, const char *frei, const char *sm)
{
    (void)c;
#ifdef MITSCHNITT
    ms_speicher("FERTIG");                         /* NEU 1.0.8 (2.7): freier Speicher nach der Arbeit */
#endif
    rm_loeschen_merken();   /* NEU 1.0.7: QR-Rückmeldung - der Kern ruht hier (Arbeitsfaden, vor G.m) */
    pthread_mutex_lock(&G.m);
    free(G.f_kopf); free(G.f_frei); free(G.f_sm);
    G.f_kopf = dup(kopf); G.f_frei = dup(frei); G.f_sm = dup(sm); G.f_ok = ok; G.bestaetigen = false;
    G.art = D_FERTIG; warte_antwort();
    pthread_mutex_unlock(&G.m);
}
static void ui_fehler(void *c, const char *t)
{
    (void)c;
    pthread_mutex_lock(&G.m);
    dialog_frei(); G.titel = dup("PS5 Cleanup"); G.text = dup(t); G.bestaetigen = false;
    G.art = D_FEHLER; warte_antwort();
    pthread_mutex_unlock(&G.m);
}
static void ui_liste_neu(void *c) { (void)c; neu_zeichnen(); }
/* da_Chris86 30.10.2026 09:01: keine Frage "automatisch neu starten?" mehr vor dem Löschen - am Ende wird immer ausgeschaltet (Knopf) */
static int dlg_fokus, dlg_scroll;
/* NEU 1.0.6 (da_Chris86 28.10.2026): Auswahl am Ende der Bereinigung: 0 = zurück zur Liste (○), 1 Beenden, 2 Neu starten.
 * Ausgeführt wird erst, wenn der Arbeitsfaden fertig ist (main-Schleife). */
static volatile int nach_aktion;
static volatile bool neustart_pflicht;   /* NEU 1.0.10 (da_Chris86 14.11.2026 12:10, PS5-Test: Absturz beim Start nach dem Löschen des Payload Managers): am Ende startet die PS5 neu, auch bei "Beenden" */
static int ende_hinweis;   /* NEU 1.0.7: ○ am Ende gedrückt - Hinweis (1 neu starten, 2 + Datenbank) beim Beenden als Benachrichtigung */
static void antworten(bool wahl, bool eingabe_ok)
{
    dlg_fokus = 0; dlg_scroll = 0;           /* die nächste Frage beginnt wieder beim sicheren Knopf */
    pthread_mutex_lock(&G.m);
    G.wahl = wahl; G.eingabe_ok = eingabe_ok; G.beantwortet = true;
    pthread_cond_broadcast(&G.c);
    pthread_mutex_unlock(&G.m);
}

/* =================================================================== Kern und Abläufe */
static kern *K;
static ablauf A;
static konsole_info KI;
static bool GESUCHT;                       /* Suche fertig, Liste gültig */
/* NEU 1.0.7: was die QR-Rückmeldung braucht - nur der Arbeitsfaden schreibt (wenn der Kern ruht: nach dem Nachschauen, am Ende des
 * Löschens), die Oberfläche liest eine Kopie. Beides unter rm_m (Codex-Prüfung 69: keine gleichzeitigen Zugriffe auf Kern-Daten).
 * Sperr-Reihenfolge: erst G.m, dann rm_m - nie umgekehrt. */
static pthread_mutex_t rm_m = PTHREAD_MUTEX_INITIALIZER;
static struct {
    bool gesucht; int gefunden; char jb[80]; int bereich[7], unlesbar, dauer_s; bool m2, ext, usb, sm_da;   /* letztes Nachschauen */
    bool da; int gewaehlt, fehler, teilweise, geschuetzt, unveraendert; bool ab, sm_angehalten, sm_unklar;   /* letztes Löschen */
    char fehlerliste[RM_MAX][48]; int fehler_anz[RM_MAX], fehler_n;   /* NEU 1.0.10: zusammengefasst, mit Anzahl */
    char uz[400];                                                  /* NEU 1.0.10: nicht lesbare Ordner nach Bereich und Grund (kern_rm_unlesbar) */
    char db[48];                                                   /* NEU 1.0.10: Datenbank-Übersicht als Zähler (K->APPDB_RM) */
    char prog[300], okl[300];                                      /* NEU 1.0.10: Programme der Fehler, erfolgreiche Wege ("k*n~...") */
} rm_stand;
static Uint32 rm_such_start;
/* im Arbeitsfaden aufrufen, wenn der Kern ruht (Ende des Löschens) */
static void rm_loeschen_merken(void)
{
    pthread_mutex_lock(&rm_m);
    rm_stand.da = A.rm_da; rm_stand.gewaehlt = A.rm_gewaehlt; rm_stand.fehler = A.rm_fehler; rm_stand.teilweise = A.rm_teilweise;
    rm_stand.geschuetzt = A.rm_geschuetzt; rm_stand.unveraendert = A.rm_unveraendert; rm_stand.ab = A.rm_abgebrochen;
    rm_stand.sm_angehalten = K->SM_ANGEHALTEN; rm_stand.sm_unklar = !K->SM_ANGEHALTEN && K->SM_GRUND && K->SM_GRUND[0];
    rm_stand.fehler_n = K->RM_FEHLER_N < 0 ? 0 : K->RM_FEHLER_N > RM_MAX ? RM_MAX : K->RM_FEHLER_N;
    for (int i = 0; i < rm_stand.fehler_n; i++) { memcpy(rm_stand.fehlerliste[i], K->RM_FEHLER[i], sizeof rm_stand.fehlerliste[i]); rm_stand.fehlerliste[i][47] = 0; rm_stand.fehler_anz[i] = K->RM_ANZ[i]; }
    kern_rm_zaehler_text(&K->RM_PROG, rm_stand.prog, sizeof rm_stand.prog);   /* NEU 1.0.10 */
    kern_rm_zaehler_text(&K->RM_OK, rm_stand.okl, sizeof rm_stand.okl);
    pthread_mutex_unlock(&rm_m);
}
static bool AUSWAHL_OFFEN;                 /* nach "Weiter zur Auswahl" */
static const char *eigener_ort(void)
{
    const char *o = plattform_eigener_ort();
    if (!o || !o[0]) return "";
    if (!strncmp(o, "/data/", 6)) return VERB(K->a, "/user", o);       /* /data ist /user/data */
    return o;
}
#ifdef MITSCHNITT
static zugriff *app_zugriff(void) { zugriff *z = plattform_zugriff(); return ms_f ? zugriff_mitschnitt(z, ms_schreib, NULL) : z; }
static void ms_speicher(const char *wann)
{
    static const char *const N[4] = { "intern", "M.2", "USB-Speicher", "USB-Laufwerk" };
    for (int o = ORT_INTERN; o <= ORT_STICK; o++) {
        long long fr = 0, ge = 0;
        if (!plattform_speicher(o, &fr, &ge)) continue;
        char t[160]; snprintf(t, sizeof t, "SPEICHER %s %s frei=%lld gesamt=%lld", wann, N[o], fr, ge);
        ms_zeile('I', t, NULL, NULL);
    }
}
/* appinfo.db: jede Zeile, die der Kern bekommt, auch in den Mitschnitt (D) */
static void (*ms_ai_zeile)(struct kern *k, const char *titel, const char *feld, const char *wert);
static void ms_ai_weiter(struct kern *k, const char *titel, const char *feld, const char *wert) { ms_zeile('D', titel, feld, wert); ms_ai_zeile(k, titel, feld, wert); }
static void ms_appinfo_lesen(struct kern *k, void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert))
{
    ms_ai_zeile = zeile;
    plattform_appinfo_lesen(k, ms_ai_weiter);
}
/* vor und nach dem Entfernen einer Kachel (zugriff_mitschnitt.c): steht die Kennung in app.db, welche Angaben hat appinfo.db - nur lesen */
static const char *ms_db_id; static int ms_db_n;
static void ms_db_zeile(void *c, const char *titel, const char *feld, const char *wert) { (void)c; (void)feld; (void)wert; if (titel && ms_db_id && !strcmp(titel, ms_db_id)) ms_db_n++; }
static void ms_db_zustand(const char *id, bool nachher, void (*zeile)(const char *t))
{
    const char *q = plattform_db_ordner();
    if (!q) { zeile("DB: kein Ordner mit app.db/appinfo.db"); return; }
    arena *a = arena_neu();
    const char *gr = "";
    int r = db_kennung_in_appdb(a, q, id, &gr);
    zeile(VERB(a, "DB ", nachher ? "NACH " : "VOR ", id, " app.db: ", r == 1 ? "drin" : r == 0 ? "nicht drin" : VERB(a, "nicht prüfbar (", gr ? gr : "", ")")));
    ms_db_id = id; ms_db_n = 0; gr = "";
    int z = db_appinfo_angaben(a, q, ms_db_zeile, NULL, &gr);
    zeile(VERB(a, "DB ", nachher ? "NACH " : "VOR ", id, " appinfo.db: ", z < 0 ? VERB(a, "nicht lesbar (", gr ? gr : "", ")") : a_fmt(a, "%d Angaben (Spielstand-Ordner, Zusatzinhalte) zu dieser Kennung", ms_db_n)));
    ms_db_id = NULL;
    arena_frei(a);
}
/* Bildschirmbilder: Kopie des Bilds (RGB) an einen eigenen Faden - die Oberfläche wartet nie auf das Packen; höchstens 3 warten */
typedef struct { unsigned char *rgb; int w, h, nr; } ms_bild;
static ms_bild ms_warte[3]; static int ms_warte_n = 0, ms_nr = 0, ms_verworfen = 0;
static pthread_mutex_t ms_bm = PTHREAD_MUTEX_INITIALIZER; static pthread_cond_t ms_bc = PTHREAD_COND_INITIALIZER;
static bool ms_faden_da = false, ms_arbeitet = false, ms_bilder_aus = false;
static Uint32 ms_anlass = 0, ms_geprueft = 0; static uint32_t ms_hash = 0;
typedef struct { int fd; bool fehler; } ms_png_ziel;
static int ms_png_fehler = 0;
static void ms_png_raus(void *ctx, void *d, int n)   /* Codex-Prüfung 74: stb meldet Schreibfehler nicht - darum hier merken */
{
    ms_png_ziel *z = ctx; const unsigned char *p = d;
    while (n > 0 && !z->fehler) {
        ssize_t k = write(z->fd, p, (size_t)n);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) { z->fehler = true; return; }
        p += k; n -= (int)k;
    }
}
static void *ms_bild_faden(void *x)
{
    (void)x;
    for (;;) {
        pthread_mutex_lock(&ms_bm);
        while (ms_warte_n == 0) pthread_cond_wait(&ms_bc, &ms_bm);
        ms_bild b = ms_warte[0];
        memmove(ms_warte, ms_warte + 1, sizeof(ms_bild) * (size_t)(ms_warte_n - 1)); ms_warte_n--; ms_arbeitet = true;
        pthread_mutex_unlock(&ms_bm);
        /* Platz: höchstens 2000 Bilder zu etwa 250 KB - unter 500 MB frei keine Bilder mehr. Codex-Prüfung 75: hier im Bild-Faden, nie in der
         * Oberfläche (hängt das Laufwerk, darf nur dieser Faden warten) */
        if (b.nr % 10 == 1) {
            long long fr = plattform_frei_bytes(ms_ordner);
            if (fr >= 0 && fr < 500LL * 1024 * 1024) {
                free(b.rgb);
                char t[120]; snprintf(t, sizeof t, "BILDER AUS: nur noch %lld MB frei", fr / (1024 * 1024));
                pthread_mutex_lock(&ms_bm);
                for (int i = 0; i < ms_warte_n; i++) free(ms_warte[i].rgb);   /* die noch wartenden Bilder auch nicht mehr */
                ms_warte_n = 0; ms_arbeitet = false; ms_bilder_aus = true;
                pthread_mutex_unlock(&ms_bm);
                ms_zeile('I', t, NULL, NULL);
                continue;
            }
        }
        time_t j = time(NULL); struct tm tm; localtime_r(&j, &tm);
        char p[256]; snprintf(p, sizeof p, "%s/bild_%04d_%02d%02d%02d.png", ms_ordner, b.nr, tm.tm_hour, tm.tm_min, tm.tm_sec);
        zugriff_wechsel_sperren();   /* NEU 1.0.9: nie mitten in einem Besitzer-Wechsel anlegen */
        int fd = open(p, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644);
        zugriff_wechsel_frei();
        ms_png_ziel pz = { fd, false };
        int ok = fd >= 0 ? stbi_write_png_to_func(ms_png_raus, &pz, b.w, b.h, 3, b.rgb, b.w * 3) : 0;
        if (fd >= 0 && close(fd) != 0) pz.fehler = true;
        if (pz.fehler) ok = 0;
        free(b.rgb);
        char t[300]; snprintf(t, sizeof t, "BILD %d %s%s", b.nr, strrchr(p, '/') + 1, ok ? "" : fd < 0 ? " (nicht angelegt)" : " (unvollständig - Schreibfehler)");
        ms_zeile('I', t, NULL, NULL);
        pthread_mutex_lock(&ms_bm);
        ms_arbeitet = false;
        if (!ok && ++ms_png_fehler >= 3 && !ms_bilder_aus) { ms_bilder_aus = true; ms_zeile('I', "BILDER AUS: drei Bilder ließen sich nicht speichern", NULL, NULL); }
        pthread_mutex_unlock(&ms_bm);
    }
    return NULL;
}
static void ms_taste(void) { ms_anlass = SDL_GetTicks(); }
/* ein Bild des Bildschirms, wenn er sich seit dem letzten verändert hat */
static bool ms_bilder_ende = false;               /* Codex-Prüfung 75: nach der Schlusszeile keine neuen Bilder */
static void ms_bild_jetzt(void)
{
    if (!ms_f || ms_bilder_aus || ms_bilder_ende || !ziel || ms_nr >= 2000 || ziel->format->BytesPerPixel != 4) return;
    if (SDL_MUSTLOCK(ziel)) SDL_LockSurface(ziel);
    uint32_t h = 2166136261u;
    for (int y = 0; y < ziel->h; y += 5) { const uint32_t *z = (const uint32_t *)((const unsigned char *)ziel->pixels + (size_t)y * (size_t)ziel->pitch); for (int x = 0; x < ziel->w; x += 5) { h ^= z[x]; h *= 16777619u; } }
    if (SDL_MUSTLOCK(ziel)) SDL_UnlockSurface(ziel);
    if (h == ms_hash) return;                      /* nichts verändert */
    SDL_Surface *rgb = SDL_ConvertSurfaceFormat(ziel, SDL_PIXELFORMAT_RGB24, 0);
    if (!rgb) return;
    unsigned char *d = malloc((size_t)rgb->w * (size_t)rgb->h * 3);
    if (d) for (int y = 0; y < rgb->h; y++) memcpy(d + (size_t)y * (size_t)rgb->w * 3, (const unsigned char *)rgb->pixels + (size_t)y * (size_t)rgb->pitch, (size_t)rgb->w * 3);
    int w = rgb->w, hh = rgb->h;
    SDL_FreeSurface(rgb);
    if (!d) return;
    ms_hash = h;
    pthread_mutex_lock(&ms_bm);
    if (!ms_faden_da) { pthread_t t; if (basis_faden(&t, ms_bild_faden, NULL) == 0) { pthread_detach(t); ms_faden_da = true; } }
    if (ms_faden_da && ms_warte_n < 3) { ms_warte[ms_warte_n++] = (ms_bild){ d, w, hh, ++ms_nr }; pthread_cond_signal(&ms_bc); d = NULL; }
    else ms_verworfen++;
    pthread_mutex_unlock(&ms_bm);
    free(d);
}
/* nach jedem Durchlauf der Hauptschleife: ein neues Bild, wenn es einen Anlass gibt (400 ms nach einer Taste oder neuen Frage, sonst alle 15 s) */
static void ms_bild_vielleicht(void)
{
    if (!ms_f) return;
    Uint32 jetzt = SDL_GetTicks();
    bool anlass = (ms_anlass && jetzt - ms_anlass >= 400) || jetzt - ms_geprueft >= 15000;
    if (!anlass) return;
    ms_anlass = 0; ms_geprueft = jetzt;
    ms_bild_jetzt();
}
/* vor dem Beenden oder Neustart: letztes Bild, auf die noch wartenden Bilder warten (höchstens 4 s), Schlusszeile */
static void ms_ende(const char *wann)
{
    if (!ms_f) return;
    ms_bild_jetzt();
    ms_bilder_ende = true;                         /* Codex-Prüfung 75: danach keine neuen Bilder mehr (sonst stünden BILD-Zeilen hinter ENDE) */
    bool fertig = false;
    for (int i = 0; i < 400; i++) {
        pthread_mutex_lock(&ms_bm); fertig = ms_warte_n == 0 && !ms_arbeitet; pthread_mutex_unlock(&ms_bm);
        if (fertig) break;
        SDL_Delay(10);
    }
    char t[200]; snprintf(t, sizeof t, "ENDE %s bilder=%d verworfen=%d%s", wann, ms_nr, ms_verworfen, fertig ? "" : " (ein Bild wurde noch gespeichert)");
    { arena *a = arena_neu(); ms_schreib_x(zugriff_mitschnitt_zeile(a, 'I', t, NULL, NULL), true); arena_frei(a); }   /* Codex-Prüfung 75: auch bei vollem Puffer */
    for (int i = 0; i < 300; i++) {               /* höchstens 3 s, bis der Text geschrieben ist - hängt das Laufwerk, nicht länger */
        pthread_mutex_lock(&ms_m); bool leer = (ms_puf_n == 0 && !ms_schreibt) || ms_aus; pthread_mutex_unlock(&ms_m);
        if (leer) break;
        SDL_Delay(10);
    }
}
/* NEU (Codex-Prüfung 74): jede Suche beginnt im Mitschnitt mit "I SUCHE <n>" - auch das erneute Nachschauen nach dem Löschen - und nennt die
 * Sprache (sie lässt sich vor der ersten Suche umstellen). So gibt der PC-Nachbau die Suchen nacheinander mit demselben Kern wieder. */
static int ms_suche_nr = 0;
static void ms_suche_beginnen(bool nach_loeschen)
{
    if (!ms_f) return;
    char t[64]; snprintf(t, sizeof t, "I SUCHE %d%s\n", ++ms_suche_nr, nach_loeschen ? " NACH_LOESCHEN" : ""); ms_schreib(NULL, t);
    ms_zeile('I', SP_EN ? "SPRACHE en" : "SPRACHE de", NULL, NULL);
}
static void ms_neusuche(void *c) { (void)c; ms_suche_beginnen(true); ms_speicher("NACH_LOESCHEN"); }
static void ms_oeffnen(void)
{
    const char *o = plattform_test_ordner();
    if (!o) { ui_log(NULL, "TESTLAUF: kein Ordner für Mitschnitt und Bilder (USB-Laufwerk oder /data) - es wird nichts mitgeschnitten."); return; }
    long long fr = plattform_frei_bytes(o);
    if (fr >= 0 && fr < 200LL * 1024 * 1024) { char t[300]; snprintf(t, sizeof t, "TESTLAUF: in %s ist zu wenig Platz (unter 200 MB) - es wird nichts mitgeschnitten.", o); ui_log(NULL, t); return; }
    snprintf(ms_ordner, sizeof ms_ordner, "%s", o);
    time_t j = time(NULL); struct tm tm; localtime_r(&j, &tm);
    char p[256]; snprintf(p, sizeof p, "%s/mitschnitt_%04d-%02d-%02d_%02d%02d%02d.txt", o, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    int fd = open(p, O_WRONLY | O_CREAT | O_EXCL | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0644);
    FILE *f = fd >= 0 ? fdopen(fd, "a") : NULL;
    if (!f) { int e = errno; if (fd >= 0) close(fd); char t[400]; snprintf(t, sizeof t, "TESTLAUF: Mitschnitt nicht anlegbar: %s (%s)", p, strerror(e)); ui_log(NULL, t); return; }
    stbi_write_png_compression_level = 4;          /* schneller packen - die Bilder dürfen etwas größer sein */
    pthread_t sf;
    if (basis_faden(&sf, ms_schreib_faden, NULL) != 0) { fclose(f); ui_log(NULL, "TESTLAUF: Mitschnitt nicht möglich (kein Faden zum Schreiben) - es wird nichts mitgeschnitten."); return; }
    pthread_detach(sf);
    pthread_mutex_lock(&ms_m); ms_f = f; pthread_mutex_unlock(&ms_m);
    char t[400];
    snprintf(t, sizeof t, "%s PS5 Cleanup %s (%s)", MS_MARKE, VERSION_ANZEIGE, plattform_name()); ms_zeile('I', t, NULL, NULL);
    snprintf(t, sizeof t, "KONSOLE modell=%s firmware=%s ddd_bekannt=%d ddd=%d sprache=%s", KI.modell, KI.firmware, KI.ddd_bekannt ? 1 : 0, KI.ddd, SP_EN ? "en" : "de"); ms_zeile('I', t, NULL, NULL);
    snprintf(t, sizeof t, "EIGENER_ORT %s", plattform_eigener_ort() ? plattform_eigener_ort() : ""); ms_zeile('I', t, NULL, NULL);
    for (size_t i = 0; i < sizeof TEST_ORDNER_ORTE / sizeof TEST_ORDNER_ORTE[0]; i++) { snprintf(t, sizeof t, "TEST_ORDNER %s", TEST_ORDNER_ORTE[i]); ms_zeile('I', t, NULL, NULL); }   /* für den PC-Nachbau */
    ms_speicher("START");
    snprintf(t, sizeof t, "TESTLAUF: Mitschnitt und Bildschirmbilder kommen in den Ordner %s", o); ui_log(NULL, t);
}
#else
static zugriff *app_zugriff(void) { return plattform_zugriff(); }
#endif
static zugriff *neue_verbindung(void *c) { (void)c; return app_zugriff(); }

typedef enum { AUF_SUCHEN, AUF_KOMPLETT, AUF_AUSWAHL } auftrag;
#ifdef PRUEFLAUF
static volatile Uint32 pruef_fertig_ab;   /* Zeitpunkt, an dem der Prüflauf fertig war (0 = noch nicht) */
#endif
/* Prüfausgabe für den Vergleich mit 1.0.5 (Schritt 9): dieselben DETAIL-Zeilen wie
 * test/ref105 "detail". Geht nur an die Konsolen-Ausgabe (am PC über websrv /hbldr?pipe=1),
 * nicht auf den Bildschirm und in keine Datei. */
static const char *wf(bool b) { return b ? "True" : "False"; }
static void fundliste_ausgeben(kern *k, const char *titel)
{
    arena *a = k->a;
    printf("PRUEF %s SPRACHE=%s ANZAHL=%d\n", titel, SP_EN ? "en" : "de", k->Funde.n);
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        printf("DETAIL %s\n", VERB(a, a_fmt(a, "%d", f->Kat), " | ", f->Text, " | basis=", f->TextBasis, " | gruppe=", f->Gruppe ? f->Gruppe : "-", " | teil=", f->Teil ? f->Teil : "-",
               " | gesperrt=", wf(f->Gesperrt), " | paket=", wf(f->Paket), " | nurleer=", wf(f->NurLeer), " | kachel=", wf(f->Kachel), " | smtipp=", wf(f->SmTipp),
               " | quelle=", f->Quelle, " :: ", sl_join(a, &f->Pfade, " | "), " || ", sl_join(a, &f->PfadeBasis, " | ")));
    }
    sliste h = r_split(a, h_trim(a, kern_speicher_hinweis(k)), "\n");
    printf("HINWEIS %s\n", sl_join(a, &h, " / "));
    printf("ANZAHL %d\n", k->Funde.n);
    printf("PRUEF ENDE %s\n", titel);
    fflush(stdout);
}
static void pruefausgabe(void)
{
    printf("PRUEF KONSOLE modell=%s firmware=%s ddd_bekannt=%s ddd=%d typ=%s digital=%s\n", KI.modell, KI.firmware, wf(KI.ddd_bekannt), KI.ddd, K->MODELL_TYP, wf(K->MODELL_DIGITAL));
    printf("PRUEF LAUFWERK %s\n", kern_laufwerk_text(K));
    printf("PRUEF SPEICHER m2=%s m2_ohne_user=%s usb=%s komprimierung=%d\n", wf(K->m2Da), wf(K->M2_OHNE_USER), wf(K->USB_SPEICHER), K->KOMPRIMIERUNG.n);
    printf("PRUEF EIGENER_ORT %s\n", eigener_ort());
    printf("PRUEF SCHUTZ aktiv=%s unsicher=%s eigen=%s launcher=%s\n", wf(K->SCHUTZ.aktiv), wf(K->SCHUTZ.unsicher), wf(K->SCHUTZ.eigen_gefunden), wf(K->SCHUTZ.launcher_gefunden));
    for (int i = 0; i < K->SCHUTZ.pfade.n; i++) printf("PRUEF SCHUTZPFAD %s\n", K->SCHUTZ.pfade.e[i]);
    if (ablauf_verbunden(&A)) fundliste_ausgeben(K, "MIT_SELBSTSCHUTZ");
#ifdef PRUEFLAUF
    /* Zweite, unsichtbare Suche OHNE Selbstschutz = genau wie 1.0.5 (für den 1:1-Vergleich).
     * Nur lesen; diese Liste wird nirgends angezeigt und kann nicht ausgewählt werden.
     * 1.0.6 Fix (30.10.2026): nur noch im Prüflauf-Bau - bis 1.0.6 lief sie in JEDEM Bau nach jedem Nachschauen mit
     * (doppelte Wartezeit, doppelter Speicher, gleichzeitig mit dem Größenzählen). */
    kern *k2 = kern_neu(plattform_zugriff());
    k2->simulation = true;
    k2->MODELL_TYP = K->MODELL_TYP; k2->MODELL_DIGITAL = K->MODELL_DIGITAL; k2->DDD_BEKANNT = K->DDD_BEKANNT; k2->DDD_STATUS = K->DDD_STATUS;
    VERSUCHE { kern_suche_funde(k2); fundliste_ausgeben(k2, "OHNE_SELBSTSCHUTZ_WIE_105"); }
    FANGE { printf("PRUEF FEHLER zweite Suche: %s\n", ausn_text); } ENDE_VERSUCHE;
    /* Prüflauf vor Schritt 10 (nur lesen): Schutzentscheidung je Pfad, Größen, Auswahl - für den Vergleich
     * mit 1.0.5 ohne Selbstschutz, dazu dasselbe mit Selbstschutz (eigener, unsichtbarer Kern). */
    VERSUCHE { pruef_ausgabe(k2, "OHNE_SELBSTSCHUTZ_WIE_105"); }
    FANGE { printf("PRUEF FEHLER Prüflauf ohne Selbstschutz: %s\n", ausn_text); } ENDE_VERSUCHE;
    kern *k3 = kern_neu(plattform_zugriff());
    k3->simulation = true;
    k3->MODELL_TYP = K->MODELL_TYP; k3->MODELL_DIGITAL = K->MODELL_DIGITAL; k3->DDD_BEKANNT = K->DDD_BEKANNT; k3->DDD_STATUS = K->DDD_STATUS;
    VERSUCHE { kern_selbstschutz_eigener_pfad(k3, eigener_ort()); kern_selbstschutz_ermitteln(k3); kern_suche_funde(k3); pruef_ausgabe(k3, "MIT_SELBSTSCHUTZ"); }
    FANGE { printf("PRUEF FEHLER Prüflauf mit Selbstschutz: %s\n", ausn_text); } ENDE_VERSUCHE;
    kern_frei(k3);
    printf("PRUEFLAUF FERTIG\n"); fflush(stdout);
    pruef_fertig_ab = SDL_GetTicks();    /* Prüflauf: 15 s danach schließt sich das Programm von selbst */
    kern_frei(k2);
#endif
}

/* 1.0.6 Fix (30.10.2026): Speicherverbrauch ins Protokoll - nach dem Nachschauen und wenn alle Größen gezählt sind */
static void speicher_zeile(const char *wann)
{
    char t[200];
    snprintf(t, sizeof t, "%s: %s %.0f MB, %s %.0f MB", wann, T("Arbeitsspeicher", "memory"), basis_speicher_belegt() / 1048576.0,
             T("Höchststand", "peak"), basis_speicher_spitze() / 1048576.0);
    ui_log(NULL, t);
}
static volatile int groessen_gemeldet = 0;
static void groesse_neu_app(void *c)
{
    (void)c;
    neu_zeichnen_bald();                           /* 1.0.8 (4.7): beim Zählen kommt das für jeden Eintrag */
#ifdef VORSCHAU   /* NEU 1.0.8 (PS5-Test 07.11.2026): PC-Vorschau - Zählen sichtbar langsam (VORSCHAU_ZAEHLEN = ms je Eintrag), für Bilder beim Zählen */
    static int zl = -1; if (zl < 0) { const char *e = getenv("VORSCHAU_ZAEHLEN"); zl = e ? atoi(e) : 0; }
    if (zl > 0 && A.hg_n < A.hg_gesamt) SDL_Delay((Uint32)zl);
#endif
    if (!groessen_gemeldet && A.hg_gesamt > 0 && A.hg_n >= A.hg_gesamt) { groessen_gemeldet = 1; speicher_zeile(T("Größen fertig gezählt", "sizes counted")); }
}
/* NEU 1.0.8 (Codex-Prüfung 105, P2): zählt der Hintergrund gerade? Fehlen Größen, ohne dass noch gezählt wird (Zählen nach einem Lesefehler
 * beendet oder angehalten), heißt es "mind. …" statt endlos "zählt …" */
static bool zaehlen_laeuft(void) { return A.hintergrund && A.hg_gesamt > 0 && A.hg_n < A.hg_gesamt; }
/* NEU 1.0.8 (Arbeitsplan 2.5): In den Rückmeldungen stand „Nicht lesbare Ordner: 48“ (nur die Zahl). Welche Ordner das sind, steht jetzt
 * im Protokoll - es bleibt auf der Konsole, über den QR-Code geht weiter nur die Zahl. Am Verhalten ändert sich nichts: die Ordner bleiben
 * geschützt und werden nicht angeboten. "Zu tief" = tiefer verschachtelt, als der Selbstschutz durchsucht (kern_selbstschutz.c SUCH_TIEFE). */
/* Codex-Prüfung 79: haben zwei Pfade denselben Ordner darüber (wie h_eltern, aber ohne Kopie)? "/a/b/c" und "/a/b/d" ja, "/x" und "/y" ja ("/") */
static bool gleiche_eltern(const char *a, const char *b)
{
    const char *sa = strrchr(a, '/'), *sb = strrchr(b, '/');
    return sa && sb && sa - a == sb - b && !memcmp(a, b, (size_t)(sa - a));
}
/* NEU 1.0.10: Systemkürzel in Klartext fürs Protokoll (das Kürzel bleibt dahinter stehen, für die Fehlersuche) */
static const char *lese_grund_text(const char *c)
{
    static const struct { const char *c, *de, *en; } G[] = {
        { "EACCES", "kein Zugriff - gehört vermutlich einem anderen Programm", "no access - probably owned by another program" },
        { "EPERM", "nicht erlaubt", "not permitted" }, { "ENOENT", "inzwischen weg", "gone in the meantime" },
        { "EIO", "Lesefehler des Speichers", "storage read error" }, { "ENOTDIR", "kein Ordner", "not a folder" }, { "ELOOP", "Verknüpfungsschleife", "link loop" },
    };
    for (size_t i = 0; i < sizeof G / sizeof G[0]; i++) if (!strcmp(c, G[i].c)) return VERB(K->a, T(G[i].de, G[i].en), " ", c);
    return c;
}
static void unlesbar_melden(void)
{
    int n = K->SCHUTZ.diag.n, t = K->SCHUTZ.zu_tief.n, v = K->SCHUTZ.zu_viele.n;   /* in DIESER Suche gefunden (Codex-Prüfung 71) */
    if (n <= 0) return;
    if (v > 0)   /* NEU 1.0.8 (3.7) */
        ui_log(NULL, T("HINWEIS: Manche Ordner enthalten so viele Dateien, dass PS5 Cleanup nicht ganz prüfen kann, ob darin eine Kopie von sich selbst liegt. "
                       "Sie werden zur Sicherheit nicht angeboten - Spiele daneben schon. Welche es sind, steht in der Liste darunter.",
                       "NOTE: Some folders contain so many files that PS5 Cleanup cannot fully check whether a copy of itself is in them. "
                       "To be safe they are not offered - games next to them are. They are listed below."));
    ui_log(NULL, VERB(K->a, T("Geschützt und nicht angeboten: ", "Protected and not offered: "), a_fmt(K->a, "%d", n), T(" Ordner (", " folders ("),
                      a_fmt(K->a, "%d", n - t - v), T(" nicht lesbar, ", " not readable, "), a_fmt(K->a, "%d", t), T(" zu tief verschachtelt", " nested too deep"),
                      v > 0 ? VERB(K->a, ", ", a_fmt(K->a, "%d", v), T(" mit zu vielen Dateien", " with too many files")) : "", "):"));
    sliste schon = {0};   /* NEU 1.0.8 (3.7): ab 3 gesperrten Ordnern im selben Ordner steht nur dieser, mit Anzahl (hier: je Gruppe ein Ordner daraus) */
    for (int i = 0, z = 0; i < n && z < 1000; i++) {   /* Codex-Prüfung 71: alle zeigen (bis 1000) */
        const char *p = K->SCHUTZ.diag.e[i];
        if (h_hat(&K->SCHUTZ.zu_viele, p)) {
            /* Codex-Prüfung 79: Eltern vergleichen, ohne sie jedes Mal dauerhaft zu kopieren (vorher je Ordner alle Eltern aller gesperrten
             * Ordner - bei 500 Ordnern mit langen Pfaden über 150 MB), und eine schon zusammengefasste Gruppe nicht nochmal zählen */
            bool zusammen = false;
            for (int k = 0; k < schon.n && !zusammen; k++) zusammen = gleiche_eltern(schon.e[k], p);
            if (zusammen) continue;
            int c = 0;
            for (int j = 0; j < v; j++) if (gleiche_eltern(K->SCHUTZ.zu_viele.e[j], p)) c++;
            if (c >= 3) {
                sl_add(K->a, &schon, p); z++;
                ui_log(NULL, VERB(K->a, "   ", h_eltern(K->a, p), "/...  (", a_fmt(K->a, "%d", c), T(" Ordner darin: zu viele Dateien)", " folders in it: too many files)")));
                continue;
            }
            z++; ui_log(NULL, VERB(K->a, "   ", p, T("  (zu viele Dateien)", "  (too many files)"))); continue;
        }
        const char *lg = h_hat(&K->SCHUTZ.zu_tief, p) ? NULL : kern_lese_grund(K, p);   /* NEU 1.0.10: warum nicht lesbar */
        z++; ui_log(NULL, VERB(K->a, "   ", p, h_hat(&K->SCHUTZ.zu_tief, p) ? T("  (zu tief)", "  (too deep)")
                                              : lg ? VERB(K->a, T("  (nicht lesbar: ", "  (not readable: "), lese_grund_text(lg), ")") : T("  (nicht lesbar)", "  (not readable)")));
    }
    if (n > 1000) ui_log(NULL, VERB(K->a, T("   ... und ", "   ... and "), a_fmt(K->a, "%d", n - 1000), T(" weitere", " more")));
}
static void lauf_suchen(void *x)
{
    (void)x;
    groessen_gemeldet = 0;
    ablauf_hintergrund_anhalten(&A);
#ifdef MITSCHNITT
    ms_suche_beginnen(false); ms_speicher("SUCHE");
#endif
    kern_verbinden(K, app_zugriff());
    /* Selbstschutz (NEU 1.1): eigener Ort + Homebrew Launcher, auf allen Speichern */
    kern_selbstschutz_eigener_pfad(K, eigener_ort());
    kern_selbstschutz_ermitteln(K);
    ablauf_suchen(&A, KI.modell, KI.firmware);
    unlesbar_melden();   /* NEU 1.0.8 (Arbeitsplan 2.5) */
    pruefausgabe();
    speicher_zeile(T("Nachschauen fertig", "scan finished"));
#ifdef MITSCHNITT   /* NEU 1.0.10 (da_Chris86 14.11.2026 08:25, pbc-engine2.elf legte seinen Ordner nach dem Löschen neu an): Test-Fassung - was läuft */
    {
        static char pl[32768]; int n = plattform_prozessliste(pl, sizeof pl);
        if (n >= 0 || n <= -2) {
            char t[160]; snprintf(t, sizeof t, "TESTLAUF: laufende Programme: %d%s (Nummer, Eltern, Benutzer, Name, Pfad)", n >= 0 ? n : -2 - n, n >= 0 ? "" : " - Liste gekürzt"); ui_log(NULL, t);
            for (char *z = pl; *z; ) { char *e = strchr(z, '\n'); if (e) *e = 0; ui_log(NULL, z); if (!e) break; z = e + 1; }
        }
    }
#endif
}
static volatile int such_nr = 0;   /* NEU 1.0.9 (Browser): zählt jedes fertige Nachschauen - Befehle zu einer alten Liste gelten nicht */
static volatile int nachschau_nr = 0;   /* NEU 1.0.9 (Codex-Prüfung 145, P3): zählt nur fertige Nachschauen (Auftrag "Nachschauen") - danach
                                         * zeigt der Browser erst die Übersicht, wie der Fernseher; unabhängig davon, wann er nachfragt */
static void *arbeit(void *x)
{
    auftrag a = (auftrag)(intptr_t)x;
    if (a == AUF_SUCHEN) { rm_such_start = SDL_GetTicks(); ablauf_starte(&A, lauf_suchen, NULL); }
    else if (a == AUF_KOMPLETT) ablauf_starte(&A, (void (*)(void *))ablauf_komplett, &A);
    else ablauf_starte(&A, (void (*)(void *))ablauf_auswahl, &A);
    if (a == AUF_SUCHEN) {
        GESUCHT = ablauf_verbunden(&A); AUSWAHL_OFFEN = false;
        /* NEU 1.0.7: für die QR-Rückmeldung nur Zahlen und Kürzel merken (Reihenfolge der Bereiche wie am Bildschirm) */
        int bereich[7] = {0}; bool sm_da = false; char jb[80] = "";
        if (GESUCHT) {
            static const int REIHE[7] = { 1, 6, 2, 3, 4, 5, 7 };
            for (int i = 0; i < K->Funde.n; i++) {
                eintrag *f = K->Funde.e[i];
                for (int b = 0; b < 7; b++) if (f->Kat == REIHE[b]) bereich[b]++;
                if (f->Art == ART_SM_KACHEL || f->Art == ART_SM_DATEI) sm_da = true;
            }
            arena *ra = arena_neu(); snprintf(jb, sizeof jb, "%s", kern_jailbreak_kuerzel(K, ra)); arena_frei(ra);
        }
        pthread_mutex_lock(&rm_m);
        rm_stand.gesucht = GESUCHT; rm_stand.gefunden = GESUCHT ? K->Funde.n : 0;
        memcpy(rm_stand.bereich, bereich, sizeof bereich); rm_stand.sm_da = sm_da; snprintf(rm_stand.jb, sizeof rm_stand.jb, "%s", jb);
        rm_stand.unlesbar = GESUCHT ? K->SCHUTZ.unlesbar.n : 0;
        if (GESUCHT) kern_rm_unlesbar(K, rm_stand.uz, sizeof rm_stand.uz); else rm_stand.uz[0] = 0;   /* NEU 1.0.10 */
        snprintf(rm_stand.db, sizeof rm_stand.db, "%s", GESUCHT ? K->APPDB_RM : "");   /* NEU 1.0.10: Datenbank-Übersicht */
        rm_stand.m2 = GESUCHT && K->m2Da; rm_stand.ext = GESUCHT && K->USB_SPEICHER; rm_stand.usb = GESUCHT && K->STICK_DA;
        /* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 9): in der Rückmeldung stand "Dauer 1 s" - das war das letzte, schnelle Nachschauen nach
         * dem Löschen. Jetzt zählt das längste Nachschauen dieses Starts (meist das erste) */
        if (GESUCHT) { int d = (int)((SDL_GetTicks() - rm_such_start) / 1000); if (d > rm_stand.dauer_s) rm_stand.dauer_s = d; }
        pthread_mutex_unlock(&rm_m);
    } else { K->RM_ERFASSEN = false; rm_loeschen_merken(); }   /* auch nach einem Abbruch (dann gibt es keinen Endbildschirm) */
    basis_schritt = T("Liste/Menü", "list/menu");
    if (a == AUF_SUCHEN && GESUCHT) __atomic_add_fetch(&nachschau_nr, 1, __ATOMIC_ACQ_REL);
    __atomic_add_fetch(&such_nr, 1, __ATOMIC_ACQ_REL);   /* NEU 1.0.9: neue Liste für den Browser - nach jeder Arbeit, auch nach dem Löschen (danach
                                                          * schaut der Kern neu nach; Codex-Prüfung 140, P2) */
    G.beschaeftigt = false; neu_zeichnen();
    return NULL;
}
static auftrag auftrag_jetzt = AUF_SUCHEN;   /* NEU 1.0.8 (4.5): was gerade läuft (Seite "Nachschauen läuft", Schritt oben) */
static Uint32 arbeit_seit = 0;               /* NEU 1.0.8 (4.5): seit wann ("Läuft seit") */
static bool nach_loeschen = false;           /* NEU 1.0.8 (Codex-Prüfung 76): "Fertig" war da - der Kern schaut gerade neu nach */
static void starte(auftrag a)
{
    if (G.beschaeftigt) return;
    G.beschaeftigt = true;
    auftrag_jetzt = a; arbeit_seit = SDL_GetTicks(); nach_loeschen = false;
    pthread_mutex_lock(&G.m); G.stand[0] = 0; G.prozent = 0; G.unbestimmt = false; pthread_mutex_unlock(&G.m);
    basis_schritt = a == AUF_SUCHEN ? T("Nachschauen", "scan") : a == AUF_KOMPLETT ? T("vollständige Bereinigung", "full cleanup") : T("Auswahl löschen", "delete selection");
    pthread_t t;
    if (basis_faden(&t, arbeit, (void *)(intptr_t)a) != 0) { G.beschaeftigt = false; return; }
    pthread_detach(t);
}

/* =================================================================== Bedienzustand */
static int fokus_zone = 0;      /* 0 Bereiche, 1 Einträge */
static int fokus_bereich = 0;   /* Index in sicht[] */
/* NEU 1.0.6 (da_Chris86 28.10.2026): leere Bereiche werden nicht angezeigt (solange überhaupt etwas gefunden wurde) */
static int sicht[KAT_ANZ], sicht_n = 0;
/* dlg_fokus - Dialog: 0 sicher, 1 gefährlich  |  Text: 0 Feld, 1 Abbrechen, 2 OK (oben deklariert) */
static int dlg_gesehen = 0;
static Uint32 dlg_seit = 0;    /* kurz nach dem Erscheinen einer Frage zählt ✕ nicht (kein versehentliches Doppel-Drücken) */
static bool protokoll_offen = false; static int prot_scroll = 0;
static bool tastatur_offen = false, tastatur_gesehen = false; static Uint32 tastatur_seit = 0;
static Uint32 kreis_seit = 0;   /* ○ gehalten seit */
static bool laeuft = true;
static char hinweis_kurz[1024];  static Uint32 hinweis_bis = 0;   /* kurze Meldung unten */
static SDL_Window *fenster;

/* Codex-Prüfung 80: 0 heißt "keine Meldung" - fiele das Ende beim Überlauf der Uhr genau auf 0, stünde dort 1 */
static void hinweis_dauer(Uint32 ms) { hinweis_bis = SDL_GetTicks() + ms; if (!hinweis_bis) hinweis_bis = 1; }
static int hinweis_nr = 0;   /* NEU 1.0.9: zählt die kurzen Meldungen (die Seite im Browser zeigt jede einmal) */
static void kurz(const char *t) { snprintf(hinweis_kurz, sizeof hinweis_kurz, "%s", t); hinweis_dauer(4000); hinweis_nr++; neu_zeichnen(); }
/* NEU 1.0.8 (Arbeitsplan 3.6, da_Chris86 05.11.2026 20:33/20:41): Testmodus wie der Haken der Windows-Fassung - beim Start an, dann wird nur
 * angeschaut; Löschen geht erst, wenn er im Options-Menü aus ist. Die App merkt sich das nicht (bei jedem Start wieder an). Damit gibt es
 * beim Veröffentlichen nur noch eine ELF. */
static bool testmodus = true;
static bool optionen_offen = false; static int opt_fokus = 0;   /* Options-Menü (Startseite und Auswahl) */
static Uint32 x_ruhe_bis = 0;   /* NEU 1.0.8 (PS5-Test 07.11.2026): bis dahin zählt ✕ nicht (gleich nach dem Umschalten des Testmodus) */
#ifndef NUR_LESEN
static const char *testmodus_hinweis(void) { return T("Testmodus ist an: es wird nur angeschaut. Zum Löschen mit der Options-Taste ausschalten.",
                                                      "Test mode is on: only looking. To delete, switch it off with the Options button."); }
#endif
static void testmodus_umschalten(void)
{
#ifdef NUR_LESEN
    kurz(T("Diese Testversion schaut immer nur nach.", "This test version only ever looks."));
#else
    testmodus = !testmodus;
    kurz(testmodus ? T("Testmodus an: es wird nur angeschaut.", "Test mode on: only looking.")
                   : T("Testmodus aus: Löschen ist jetzt möglich - vorher fragt die App noch einmal nach.", "Test mode off: deleting is now possible - the app asks once more first."));
    ui_log(NULL, testmodus ? T("Testmodus an - es wird nur angeschaut.", "Test mode on - only looking.")
                           : T("Testmodus aus - Löschen ist jetzt möglich (die App fragt vorher nach).", "Test mode off - deleting is now possible (the app asks first)."));
#ifdef MITSCHNITT
    ms_zeile('I', testmodus ? "TESTMODUS an" : "TESTMODUS aus", NULL, NULL);
#endif
#endif
}
/* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86 08:46: "Löschen konnte ich nichts ... auch wenn er aus war"): laut Mitschnitt ging der Testmodus
 * um 08:39:56 aus und in derselben Sekunde durch ein zweites ✕ wieder an (der Satz "erst nach dem Bestätigen" klang nach noch einmal ✕).
 * Jetzt schließt sich das Options-Menü gleich nach dem Umschalten, und ✕ zählt kurz danach nicht - sonst hakte ein doppeltes ✕ einen Eintrag an. */
static void testmodus_aus_menue(void)
{
    testmodus_umschalten();
    optionen_offen = false;
    x_ruhe_bis = SDL_GetTicks() + 700; if (!x_ruhe_bis) x_ruhe_bis = 1;
}

static int bereich_anzahl(int k) { int n = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Kat == k) n++; return n; }
static void sicht_neu(void)
{
    sicht_n = 0;
    for (int i = 0; i < KAT_ANZ; i++) if (bereich_anzahl(ALLE_KAT[i]) > 0) sicht[sicht_n++] = ALLE_KAT[i];
    if (sicht_n == 0) for (int i = 0; i < KAT_ANZ; i++) sicht[sicht_n++] = ALLE_KAT[i];
    if (fokus_bereich >= sicht_n) fokus_bereich = sicht_n - 1;
    if (fokus_bereich < 0) fokus_bereich = 0;
}
#ifndef NUR_LESEN   /* 1.0.8: nur für "Nur Auswahl löschen" gebraucht - die Testfassungen meldeten es als unbenutzt */
static int gewaehlt_anzahl(void) { int n = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Wahl) n++; return n; }
#endif

/* =================================================================== Bildschirme
 * 1.0.8 (4.5): der alte Kopf (kopf) und die alte Fußleiste (fuss) sind weg - alle Seiten nutzen kopf_neu und fuss_neu. */

/* =================================================================== NEU 1.0.8 (Arbeitsplan 4.1/4.2): Kopf, Fußleiste und Startseite
 * nach Entwurf 2 ("Start vor dem Nachschauen" und "Start nach dem Nachschauen"). Die Bedienung bleibt wie bisher. */
static int grundlinie(int oben, int lh, int gr) { return oben + (lh + gr * 7 / 10) / 2; }   /* Schrift mittig in einer Zeile der Höhe lh */
static void text_rechts(int xr, int y, int f, int gr, uint32_t farbe, const char *t) { text(xr - textbreite(f, gr, t), y, f, gr, farbe, t); }
static void text_mitte(int cx, int y, int f, int gr, uint32_t farbe, const char *t) { text(cx - textbreite(f, gr, t) / 2, y, f, gr, farbe, t); }
/* runde Ecken über ein Bild legen (Hintergrund hg) */
static void ecken_maske(int x, int y, int w, int h, int r, uint32_t hg)
{
    for (int dy = 0; dy < r; dy++) for (int dx = 0; dx < r; dx++) {
        float ex = (float)r - ((float)dx + .5f), ey = (float)r - ((float)dy + .5f);
        float a = 1.f - ((float)r - sqrtf(ex * ex + ey * ey) + .5f);
        if (a <= 0.f) continue;
        misch(x + dx, y + dy, hg, a); misch(x + w - 1 - dx, y + dy, hg, a); misch(x + dx, y + h - 1 - dy, hg, a); misch(x + w - 1 - dx, y + h - 1 - dy, hg, a);
    }
}
/* Kopf: Logo, Name, Version, Hinweis-Abzeichen der Testfassungen, Schritte 1-2-3 rechts. schritt = aktueller Schritt (1..3), 4 = alle erledigt */
/* =================================================================== NEU 1.0.9: Update-Hinweis (da_Chris86 08.11.2026 01:16: bei jedem Start)
 * Beim Start fragt ein eigener Faden GitHub nach der neuesten veröffentlichten Version (plattform_neueste_version, nur lesen). Ist sie
 * neuer, steht auf der Startseite oben "Neue Version x" und einmal kommt eine kurze Meldung, wo es sie gibt. Ohne Netz oder ohne Antwort
 * passiert nichts (nur eine Zeile im Protokoll). Gespeichert wird nichts. */
static pthread_mutex_t upd_m = PTHREAD_MUTEX_INITIALIZER;
static char upd_neu[32];                /* neuere Version laut GitHub (ohne "v"), "" = keine oder unbekannt - unter upd_m */
static bool upd_gezeigt = false;        /* kurze Meldung schon gezeigt (nur Hauptfaden) */
static const char *kopf_update = NULL;  /* setzt schirm_start für kopf_neu (nur auf der Startseite) */
static void upd_holen(char *v, size_t n) { pthread_mutex_lock(&upd_m); snprintf(v, n, "%s", upd_neu); pthread_mutex_unlock(&upd_m); }
static void *upd_faden(void *x)
{
    (void)x;
    char tag[64] = "", grund[400] = "", z[700];   /* 1.0.9 (Codex-Prüfung 145): Platz für die Rückgabewerte des zweiten Versuchs */
    bool ok = plattform_neueste_version(VERSION_NUMMER, tag, sizeof tag, grund, sizeof grund);
    const char *v = tag[0] == 'v' || tag[0] == 'V' ? tag + 1 : tag;
    /* 1.0.10 (da_Chris86 12.11.2026 09:34): auch ohne Antwort nur ein kurzer Satz - der Grund (Fehlercodes) steht im Kernel-Protokoll */
    if (!ok) snprintf(z, sizeof z, "%s", T("Update-Prüfung: GitHub gerade nicht erreichbar.", "Update check: GitHub not reachable right now."));
    else if (!kern_version_gueltig(tag) || strlen(v) >= sizeof upd_neu) snprintf(z, sizeof z, "%s", T("Update-Prüfung: unbekannte Versionsangabe von GitHub.", "Update check: unknown version from GitHub."));
    else if (kern_version_neuer(tag, VERSION_NUMMER)) {
        pthread_mutex_lock(&upd_m); snprintf(upd_neu, sizeof upd_neu, "%.31s", v); pthread_mutex_unlock(&upd_m);   /* Länge oben geprüft */
        snprintf(z, sizeof z, T("Update-Prüfung: Es gibt eine neuere Version: PS5 Cleanup %s (github.com/xXBneutzerNameXx/PS5-Cleanup).",
                                "Update check: a newer version is available: PS5 Cleanup %s (github.com/xXBneutzerNameXx/PS5-Cleanup)."), v);
    } else snprintf(z, sizeof z, "%s", T("Update-Prüfung: keine neuere Version.", "Update check: no newer version."));
    /* 1.0.10 (da_Chris86 12.11.2026 06:41 "das mit der updatzeprüfung nervt"): die Technik des zweiten Versuchs (ohne Zertifikatsprüfung,
     * Rückgabewerte) steht nicht mehr in der Zeile für den Nutzer, sondern nur noch im Kernel-Protokoll - zur Fehlersuche bleibt sie da */
    if (grund[0]) { char e[480]; snprintf(e, sizeof e, "Update-Prüfung, Einzelheiten: %s", grund); plattform_protokoll_zeile(e); }
    ui_log(NULL, z);
    neu_zeichnen();
    return NULL;
}
static void upd_starten(void)
{
    pthread_t t;
    if (basis_faden(&t, upd_faden, NULL) == 0) pthread_detach(t);
    else ui_log(NULL, T("Update-Prüfung: nicht gestartet (kein Faden).", "Update check: not started (no thread)."));
}
static void upd_anzeigen(void)   /* aus der Hauptschleife: einmal eine kurze Meldung, solange die Startseite vor dem ersten Nachschauen zu sehen ist */
{
    if (upd_gezeigt || G.beschaeftigt || G.art != D_KEINER || GESUCHT || optionen_offen || protokoll_offen) return;
    char v[32]; upd_holen(v, sizeof v);
    if (!v[0]) return;
    upd_gezeigt = true;
    char t[300];
    snprintf(t, sizeof t, T("Es gibt eine neuere Version: PS5 Cleanup %s. Zu finden auf GitHub (xXBneutzerNameXx/PS5-Cleanup).",
                            "A newer version is available: PS5 Cleanup %s. You can find it on GitHub (xXBneutzerNameXx/PS5-Cleanup)."), v);
    kurz(t); hinweis_dauer(10000);
}
static void kopf_neu(int schritt, const char *satz)
{
    int lg = satz ? 72 : 64, cy = RAND_O + lg / 2;
    if (logo) { SDL_Rect z = { RAND_X, RAND_O, lg, lg }; SDL_BlitScaled(logo, NULL, ziel, &z); ecken_maske(RAND_X, RAND_O, lg, lg, lg / 4, N_HG); }
    int x = RAND_X + lg + 24, ty = satz ? RAND_O + 38 : cy + 13;
    text(x, ty, 1, 36, N_TEXT, "PS5 Cleanup");
    int vx = x + textbreite(1, 36, "PS5 Cleanup") + 16, vb = textbreite(1, 22, VERSION_ANZEIGE) + 28;
    rund(vx, ty - 29, vb, 36, 18, N_FELD); text(vx + 14, ty - 4, 1, 22, N_GRAU, VERSION_ANZEIGE);
    int hx = vx + vb + 12;
#ifndef KERN_LOESCHEN
    {   /* Testfassungen: immer sichtbar, dass nichts gelöscht wird */
#ifdef NUR_LESEN
        const char *h = T("TEST: NUR LESEN", "TEST: READ ONLY");
#else
        const char *h = "SIMULATION";
#endif
        int hb = textbreite(1, 22, h) + 28;
        rund(hx, ty - 29, hb, 36, 18, farbmisch(N_GELB, N_HG, .16f)); text(hx + 14, ty - 4, 1, 22, N_GELB, h);
        hx += hb + 12;
    }
#endif
#ifndef NUR_LESEN
    if (testmodus) {   /* NEU 1.0.8 (3.6): solange er an ist, immer sichtbar */
        const char *h = T("TESTMODUS", "TEST MODE"); int hb = textbreite(1, 22, h) + 28;
        rund(hx, ty - 29, hb, 36, 18, farbmisch(N_GELB, N_HG, .16f)); text(hx + 14, ty - 4, 1, 22, N_GELB, h);
        hx += hb + 12;
    }
#endif
    if (kopf_update) {   /* NEU 1.0.9: neuere Version bei GitHub (nur auf der Startseite) */
        const char *h = a_fmt(fa, T("Neue Version %s", "New version %s"), kopf_update); int hb = textbreite(1, 22, h) + 28;
        rund(hx, ty - 29, hb, 36, 18, farbmisch(N_GRUEN, N_HG, .16f)); text(hx + 14, ty - 4, 1, 22, N_GRUEN, h);
        hx += hb + 12;
    }
    const char *sn[3] = { T("Nachschauen", "Scan"), T("Auswählen", "Select"), T("Bestätigen", "Confirm") };
    int ges = 0;
    for (int i = 0; i < 3; i++) ges += 44 + 12 + textbreite(i + 1 == schritt ? 1 : 0, 24, sn[i]) + (i < 2 ? 14 + 40 + 14 : 0);
    int sx = BREITE - RAND_X - ges;
    if (fern_aktiv(8000)) {   /* NEU 1.0.9: solange jemand im Browser bedient (die Seite fragt laufend nach) */
        /* da_Chris86 08.11.2026 21:39 (Bild): mit der langen Testlauf-Versionsangabe und TESTMODUS lag die Pille über "1 Nachschauen" -
         * nur so viel zeichnen, wie vor den Schritten Platz hat: ganz, kurz, nur ein Punkt oder gar nicht */
        const char *h = T("IM BROWSER", "IN BROWSER"); int hb = textbreite(1, 22, h) + 28, platz = sx - 24 - hx;
        int py = ty;
        if (hb > platz && satz) {   /* zweite Zeile, hinter dem Satz */
            int sb = textbreite(0, 24, kuerzen(0, 24, satz, BREITE - x - 860));
            hx = x + sb + 20; py = RAND_O + 76 + 3; platz = sx - 24 - hx;
        }
        if (hb > platz) { h = "BROWSER"; hb = textbreite(1, 22, h) + 28; }
        if (hb <= platz) { rund(hx, py - 29, hb, 36, 18, farbmisch(N_FOKUS, N_HG, .16f)); text(hx + 14, py - 4, 1, 22, N_FOKUS, h); }
        else if (36 <= platz) { rund(hx, py - 29, 36, 36, 18, farbmisch(N_FOKUS, N_HG, .16f)); kreis(hx + 18, py - 11, 7, N_FOKUS); }
    }
    if (satz) text(x, RAND_O + 76, 0, 24, N_GRAU, kuerzen(0, 24, satz, BREITE - x - 860));
    for (int i = 0; i < 3; i++) {
        int nr = i + 1; char z[4]; snprintf(z, sizeof z, "%d", nr);
        if (nr < schritt) { kreis(sx + 22, cy, 22, N_BLAU); symbol(SYM_HAKEN, sx + 11, cy - 11, 22, 0xFFFFFF); }
        else if (nr == schritt) { ring(sx + 22, cy, 20.5f, 3, N_FOKUS); text_mitte(sx + 22, cy + 8, 1, 22, N_FOKUS, z); }
        else { ring(sx + 22, cy, 21, 2, N_LINIE); text_mitte(sx + 22, cy + 8, 1, 22, N_GRAU2, z); }
        text(sx + 56, cy + 9, nr == schritt ? 1 : 0, 24, nr == schritt ? N_TEXT : nr < schritt ? N_GRAU : N_GRAU2, sn[i]);
        sx += 56 + textbreite(nr == schritt ? 1 : 0, 24, sn[i]);
        if (i < 2) { sx += 14; rund(sx, cy - 1, 40, 3, 1, nr < schritt ? N_BLAU : N_LINIE); sx += 54; }
    }
}
/* Tasten der Fußleiste: gezeichnetes Symbol im Ring (wie auf dem DualSense) */
typedef enum { KN_X, KN_O, KN_DREIECK, KN_QUADRAT, KN_OPT, KN_HOCHRUNTER, KN_LINKSRECHTS, KN_L1R1 } knopfart;
typedef struct { knopfart k; const char *t; bool rechts; } fussknopf;
static int knopf_breite(knopfart k) { return k == KN_OPT ? 46 : k == KN_L1R1 ? textbreite(1, 18, "L1") * 2 + 40 : k == KN_LINKSRECHTS || k == KN_HOCHRUNTER ? 44 : 36; }
static void knopf_glyph(knopfart k, int x, int cy, uint32_t ring_f, uint32_t sym_f)
{
    switch (k) {
    case KN_OPT: karte_rund(x, cy - 18, 46, 36, 18, 2, ring_f, N_FUSS); symbol(SYM_KN_OPTIONEN, x + 14, cy - 9, 18, sym_f); return;
    case KN_L1R1: {
        int w = textbreite(1, 18, "L1") + 16;
        karte_rund(x, cy - 16, w, 32, 8, 2, ring_f, N_FUSS); text(x + 8, cy + 6, 1, 18, sym_f, "L1");
        karte_rund(x + w + 8, cy - 16, w, 32, 8, 2, ring_f, N_FUSS); text(x + w + 16, cy + 6, 1, 18, sym_f, "R1");
        return; }
    case KN_HOCHRUNTER: karte_rund(x, cy - 18, 44, 36, 18, 2, ring_f, N_FUSS); symbol(SYM_KN_HOCH_RUNTER, x + 13, cy - 9, 18, sym_f); return;
    case KN_LINKSRECHTS: karte_rund(x, cy - 18, 44, 36, 10, 2, ring_f, N_FUSS); symbol(SYM_KN_LINKS_RECHTS, x + 12, cy - 7, 14, sym_f); return;
    default: break;
    }
    ring(x + 18, cy, 17, 2, ring_f);
    const float *s = k == KN_X ? SYM_KN_KREUZ : k == KN_O ? SYM_KN_KREIS : k == KN_DREIECK ? SYM_KN_DREIECK : SYM_KN_QUADRAT;
    symbol(s, x + 10, cy - 8, 16, sym_f);
}
/* Fußleiste über die ganze Breite (Entwurf 2): Tasten links, mit rechts = true rechts; kurze Meldungen und "○ halten" wie bisher.
 * hinweis_ueber: untere Kante der kurzen Meldung (0 = über der Fußleiste) - gilt für ein Bild */
static int hinweis_ueber = 0;
static void fuss_neu(const fussknopf *kn, int n)
{
    int y0 = HOEHE - RAND_U - 64, cy = y0 + 32;
    rechteck(0, y0, BREITE, HOEHE - y0, N_FUSS); rechteck(0, y0, BREITE, 1, N_FUSS_LINIE);
    int x = RAND_X, xr = BREITE - RAND_X;
    for (int i = 0; i < n; i++) if (!kn[i].rechts) {
        knopf_glyph(kn[i].k, x, cy, N_GRAU2, 0xE6EAF0); x += knopf_breite(kn[i].k) + 12;
        text(x, cy + 9, 0, 24, N_GRAU, kn[i].t); x += textbreite(0, 24, kn[i].t) + 44;
    }
    for (int i = n - 1; i >= 0; i--) if (kn[i].rechts) {
        int tw = textbreite(0, 24, kn[i].t); xr -= tw; text(xr, cy + 9, 0, 24, N_GRAU, kn[i].t);
        xr -= 12 + knopf_breite(kn[i].k); knopf_glyph(kn[i].k, xr, cy, N_GRAU2, 0xE6EAF0); xr -= 44;
    }
    if (hinweis_bis && (Sint32)(hinweis_bis - SDL_GetTicks()) > 0) {   /* kurze Meldung: gut sichtbar, notfalls zweizeilig (die Jailbreak-Warnung ist lang); Codex-Prüfung 78: auch beim Überlauf der Uhr (nach 49 Tagen) */
        int mw = BREITE - 2 * RAND_X - 48, hb = 0;
        passend p = einpassen(1, 24, 22, 18, hinweis_kurz, mw, 2); p.lh = p.gr + 8;
        for (int i = 0; i < p.z.n; i++) { int b = textbreite(1, p.gr, p.z.z[i]); if (b > hb) hb = b; }
        int hh = 24 + p.z.n * p.lh, hy = (hinweis_ueber ? hinweis_ueber : y0 - 14) - hh;
        rund(BREITE / 2 - hb / 2 - 24, hy, hb + 48, hh, 16, farbmisch(N_GELB, N_HG, .18f));
        for (int i = 0; i < p.z.n; i++) text_mitte(BREITE / 2, hy + 10 + p.gr + i * p.lh, 1, p.gr, N_GELB, p.z.z[i]);
    }
    hinweis_ueber = 0;
    if (kreis_seit && !G.beschaeftigt) {
        int anteil = (int)(SDL_GetTicks() - kreis_seit); if (anteil > 1000) anteil = 1000;
        rechteck(0, HOEHE - 6, BREITE * anteil / 1000, 6, N_FOKUS);
    }
}
/* Karte mit Symbolfeld und drei Zeilen (Konsole, Laufwerks-Kopplung, Jailbreak) */
static void info_karte(int x, int y, int w, int h, const float *sym, uint32_t sym_f, uint32_t feld, uint32_t rand_f, uint32_t innen,
                       const char *titel, uint32_t titel_f, const char *wert, uint32_t wert_f, const float *wert_sym, const char *unter, const float *unter_sym)
{
    karte_rund(x, y, w, h, 22, 1, rand_f, innen);
    int iy = y + (h - 64) / 2;
    rund(x + 24, iy, 64, 64, 16, feld); symbol(sym, x + 24 + 16, iy + 16, 32, sym_f);
    int tx = x + 24 + 64 + 20, tb = x + w - 24 - tx, ty = y + (h - 104) / 2;
    text(tx, grundlinie(ty, 30, 24), 0, 24, titel_f, kuerzen(0, 24, titel, tb));
    int wx = tx;
    if (wert_sym) { symbol(wert_sym, wx, ty + 32 + 7, 24, wert_f); wx += 34; }
    text(wx, grundlinie(ty + 32, 38, 30), 1, 30, wert_f, kuerzen(1, 30, wert, tx + tb - wx));
    int ux = tx;
    if (unter_sym) { symbol(unter_sym, ux, ty + 72 + 4, 22, N_GRAU2); ux += 30; }
    text(ux, grundlinie(ty + 72, 30, 24), 0, 24, N_GRAU, kuerzen(0, 24, unter, tx + tb - ux));
}
/* Speicher-Balken: belegt/frei bzw. gefunden/bleibt/frei, mit Legende */
static void speicher_balken(int x, int y, int w, const float *sym, const char *name, long long gesamt, long long frei, long long gefunden, bool nach)
{
    symbol(sym, x, y, 30, N_GRAU);
    text(x + 44, grundlinie(y, 32, 28), 1, 28, N_TEXT, name);
    text(x + 44 + textbreite(1, 28, name) + 14, grundlinie(y, 32, 24), 0, 24, N_GRAU2, kern_groesse_text(fa, gesamt));
    long long belegt = gesamt - frei; if (belegt < 0) belegt = 0;
    long long bleibt = nach ? belegt - gefunden : 0; if (bleibt < 0) bleibt = 0;
    long long teil[3] = { nach ? gefunden : belegt, bleibt, frei };
    uint32_t farbe[3] = { nach ? N_BLAU : N_BALKEN, N_BALKEN, N_BALKEN_FREI };
    int by = y + 44, bx = x, frei_w = w;
    int n_da = 0; for (int i = 0; i < 3; i++) if (teil[i] > 0) n_da++;
    frei_w -= (n_da > 1 ? n_da - 1 : 0) * 2;
    long long summe = teil[0] + teil[1] + teil[2]; if (summe <= 0) summe = 1;
    for (int i = 0; i < 3; i++) {
        if (teil[i] <= 0) continue;
        int bw = (int)((double)frei_w * (double)teil[i] / (double)summe); if (bw < 6) bw = 6;
        if (bx + bw > x + w) bw = x + w - bx;
        rund(bx, by, bw, 24, 4, farbe[i]); bx += bw + 2;
    }
    int ly = by + 24 + 14, lx = x;
    for (int i = 0; i < 3; i++) {
        if (teil[i] <= 0 && !(i == 2)) continue;
        if (i == 1 && !nach) continue;
        rund(lx, ly + 7, 16, 16, 4, farbe[i]);
        if (i == 2) { rund(lx, ly + 7, 16, 16, 4, 0x4A515C); rund(lx + 1, ly + 8, 14, 14, 3, N_BALKEN_FREI); }
        lx += 26;
        const char *z = kern_groesse_text(fa, teil[i]);
        const char *w2 = i == 0 ? (nach ? T(" gefunden", " found") : T(" belegt", " used")) : i == 1 ? T(" bleibt", " stays") : T(" frei", " free");
        if (i == 0 && nach) { text(lx, grundlinie(ly, 30, 24), 1, 24, N_TEXT, z); lx += textbreite(1, 24, z); text(lx, grundlinie(ly, 30, 24), 0, 24, N_GRAU, w2); lx += textbreite(0, 24, w2) + 28; }
        else { const char *s = VERB(fa, z, w2); text(lx, grundlinie(ly, 30, 24), 0, 24, N_GRAU, s); lx += textbreite(0, 24, s) + 28; }
    }
}
static const float *kat_symbol(int k)
{
    switch (k) { case 1: return SYM_SPIELE; case 2: return SYM_SPIELSTAND; case 3: return SYM_TROPHAE; case 4: return SYM_BILDER; case 6: return SYM_APPS; case 7: return SYM_JAILBREAK; default: return SYM_SONSTIGES; }
}
/* Hauptknopf (blau, türkiser Rand = Fokus) bzw. Nebenknopf (Rand) mit Tastensymbol */
static void knopf_gross(int x, int y, int w, int h, bool haupt, knopfart k, const char *t)
{
    int tw = textbreite(1, haupt ? 28 : 26, t), gw = 40, gx = x + (w - (gw + 14 + tw)) / 2, cy = y + h / 2;
    if (haupt) {
        rund(x - 9, y - 9, w + 18, h + 18, 27, farbmisch(N_FOKUS, N_HG, .18f));
        rund(x - 3, y - 3, w + 6, h + 6, 21, N_FOKUS);
        rund(x, y, w, h, 18, N_BLAU);
        ring(gx + 20, cy, 19, 2, 0xF2F6FF);
    } else {
        karte_rund(x, y, w, h, 18, 2, N_LINIE, N_KARTE);
        ring(gx + 20, cy, 17, 2, N_GRAU2);
    }
    const float *s = k == KN_X ? SYM_KN_KREUZ : k == KN_O ? SYM_KN_KREIS : k == KN_DREIECK ? SYM_KN_DREIECK : SYM_KN_QUADRAT;
    symbol(s, gx + 12, cy - 8, 16, haupt ? 0xFFFFFF : 0xE6EAF0);
    text(gx + gw + 14, grundlinie(y, h, haupt ? 28 : 26), 1, haupt ? 28 : 26, haupt ? 0xFFFFFF : N_TEXT, t);
}
static int knopf_gross_breite(bool haupt, const char *t) { return textbreite(1, haupt ? 28 : 26, t) + 40 + 14 + (haupt ? 68 : 52); }

/* NEU 1.0.8 (4.3/3.6): Options-Menü als Fenster über der Seite (Startseite und Auswahl) - Zeilen mit Symbol, Titel und einem Satz darunter */
static void optionen_zeichnen(const char *const *z, const char *const *u, const float *const *sy, int n)
{
    abdunkeln();
    int w = 840, h = 96 + n * 96 + 72, x = (BREITE - w) / 2, y = (HOEHE - h) / 2;
    karte_rund(x, y, w, h, 22, 1, N_RAND, N_KARTE);
    text(x + 32, y + 58, 1, 32, N_TEXT, T("Optionen", "Options"));
    for (int i = 0; i < n; i++) {
        int ry = y + 88 + i * 96;
        if (opt_fokus == i) karte_rund(x + 20, ry, w - 40, 84, 14, 3, N_FOKUS, 0x2C323B); else rund(x + 20, ry, w - 40, 84, 14, 0x20242A);
        symbol(sy[i], x + 44, ry + 28, 28, N_GRAU);
        text_passend(x + 92, ry + 38, 1, 26, 24, N_TEXT, z[i], w - 132);
        text_passend(x + 92, ry + 70, 0, 24, 22, N_GRAU2, u[i], w - 132);
    }
    fussknopf kn[3] = { { KN_HOCHRUNTER, T("wählen", "move"), false }, { KN_X, T("auswählen", "select"), false }, { KN_O, T("schließen", "close"), false } };
    int kx = x + 32, ky = y + h - 38;
    for (int i = 0; i < 3; i++) { knopf_glyph(kn[i].k, kx, ky, N_GRAU2, 0xE6EAF0); kx += knopf_breite(kn[i].k) + 12; text(kx, ky + 9, 0, 24, N_GRAU, kn[i].t); kx += textbreite(0, 24, kn[i].t) + 36; }
}
static const char *testmodus_zeile(void) { return testmodus ? T("Testmodus: an", "Test mode: on") : T("Testmodus: aus", "Test mode: off"); }
static const char *testmodus_satz(void)
{
#ifdef NUR_LESEN
    return T("Diese Testversion schaut immer nur nach.", "This test version only ever looks.");
#else
    return testmodus ? T("Nur anschauen, nichts wird gelöscht. ✕ schaltet ihn aus.", "Only looking, nothing is deleted. ✕ switches it off.")
                     : T("Löschen möglich, die App fragt vorher nach. ✕ schaltet ihn wieder ein.", "Deleting possible, the app asks first. ✕ switches it on again.");
#endif
}
/* NEU 1.0.9: Bedienung im Browser - Zeile im Options-Menü (Startseite und Auswahl, immer als letzte) */
static const char *fern_zeile_opt(void) { return T("Browser-Bedienung", "Browser control"); }
static const char *fern_satz(void)
{
    if (!fern_an()) return T("Nicht verfügbar - der Grund steht im Protokoll.", "Not available - the reason is in the log.");
    char a[64]; fern_adresse(a, sizeof a);
    return a_fmt(fa, T("%s · ✕ zeigt die Adresse als QR-Code", "%s · ✕ shows the address as a QR code"), a);
}
static void start_optionen(void)   /* Startseite: Testmodus, Protokoll & Rückmeldung, Browser-Bedienung (1.0.9) */
{
    const char *z[3] = { testmodus_zeile(), T("Protokoll & Rückmeldung", "Log & feedback"), fern_zeile_opt() };
    const char *u[3] = { testmodus_satz(), T("Was bisher passiert ist, dazu die freiwillige Rückmeldung", "What happened so far, plus the optional feedback"), fern_satz() };
    const float *sy[3] = { SYM_SCHILD_HAKEN, SYM_DOKUMENT, SYM_KONSOLE };
    optionen_zeichnen(z, u, sy, 3);
}
static void schirm_start(void)
{
    rechteck(0, 0, BREITE, HOEHE, N_HG);
    bool fertig = GESUCHT && ablauf_verbunden(&A);
    { char un[32]; upd_holen(un, sizeof un); kopf_update = un[0] ? a_kopie(fa, un) : NULL; }   /* NEU 1.0.9 */
    kopf_neu(fertig ? 2 : 1, T("Löscht nur, was du auswählst. System, Konto und Lizenzen bleiben.", "Deletes only what you select. System, account and licenses stay."));
    kopf_update = NULL;
    /* drei Karten: Konsole, Laufwerks-Kopplung, Jailbreak */
    int ky = RAND_O + 78 + 32, kh = 146, kw = (BREITE - 2 * RAND_X - 48) / 3;
    const char *reihe = K->MODELL_TYP[0] ? kern_modell_reihe(K->MODELL_TYP, K->MODELL_DIGITAL) : T("Unbekanntes Modell", "Unknown model");
    const char *fw = KI.firmware[0] ? VERB(fa, reihe, " · Firmware ", KI.firmware) : reihe;
    info_karte(RAND_X, ky, kw, kh, SYM_KONSOLE, N_GRAU, N_FELD, N_RAND, N_KARTE, T("Konsole", "Console"), N_GRAU2, fw, N_TEXT, NULL,
               KI.modell[0] ? kern_modell_zeile(fa, KI.modell) : T("Modell nicht erkannt", "Model not detected"), NULL);
    {
        kopplungsart ks = kern_kopplungs_stufe(K);
        const char *t = kern_laufwerk_text(K), *dp = strstr(t, ": ");
        const char *wert = dp ? a_kopie_n(fa, t, (size_t)(dp - t)) : t;   /* kurz: "Online gekoppelt", "Ab Werk gekoppelt", "Keine Kopplung", "Nicht abgefragt" ... (1.0.8: auch bei bestätigter Kopplung der Titel des Kerns) */
        uint32_t wf = ks == KOPPLUNG_BESTAETIGT ? N_GRUEN : ks == KOPPLUNG_UNKLAR ? N_GELB : N_GRAU;
        info_karte(RAND_X + kw + 24, ky, kw, kh, SYM_SCHEIBE, N_GRAU, N_FELD, N_RAND, N_KARTE, T("Laufwerks-Kopplung", "Disc drive pairing"), N_GRAU2,
                   wert, wf, ks == KOPPLUNG_BESTAETIGT ? SYM_HAKEN : NULL, T("Wird nie verändert", "Never changed"), SYM_SCHLOSS);
    }
    {
        int jx = RAND_X + 2 * (kw + 24);
        const char *warn = NULL, *jb = fertig && !G.beschaeftigt ? kern_jailbreak_erkannt(K, fa, &warn) : NULL;
        if (!fertig || G.beschaeftigt)
            info_karte(jx, ky, kw, kh, SYM_JAILBREAK, N_GRAU2, N_FELD, N_RAND, N_KARTE, "Jailbreak", N_GRAU2, T("Noch nicht geprüft", "Not checked yet"), N_GRAU, NULL,
                       T("Wird beim Nachschauen erkannt", "Detected when scanning"), NULL);
        else if (!jb)
            info_karte(jx, ky, kw, kh, SYM_JAILBREAK, N_GRAU2, N_FELD, N_RAND, N_KARTE, "Jailbreak", N_GRAU2, T("Kein Einstieg gefunden", "No entry found"), N_GRAU, NULL,
                       T("Nichts zu schützen", "Nothing to protect"), NULL);
        else {
            /* so viele Namen, wie in die Zeile passen (mindestens einer), der Rest als Zahl - alle stehen unter "Jailbreak" */
            sliste nm = r_split(fa, jb, ", ");
            int platz = kw - 24 - 64 - 20 - 24, n = 1;
            const char *zwei = nm.n ? nm.e[0] : jb;
            while (n < nm.n && textbreite(1, 30, VERB(fa, zwei, ", ", nm.e[n])) <= platz) { zwei = VERB(fa, zwei, ", ", nm.e[n]); n++; }
            int rest = nm.n - n;
            const char *unter = rest > 0 ? a_fmt(fa, T("+%d weitere · nicht vorausgewählt", "+%d more · not preselected"), rest) : T("nicht vorausgewählt", "not preselected");
            info_karte(jx, ky, kw, kh, SYM_JAILBREAK, N_ORANGE, farbmisch(N_ORANGE, N_HG, .14f), farbmisch(N_ORANGE, N_HG, .38f), farbmisch(N_ORANGE, N_HG, .07f),
                       T("Jailbreak erkannt", "Jailbreak detected"), N_ORANGE, zwei, N_TEXT, NULL, unter, NULL);
        }
    }
    /* Speicher (links) und "So geht's" bzw. das Gefundene (rechts) */
    int my = ky + kh + 28, fy = HOEHE - RAND_U - 64, uy = fy - 24 - (fertig ? 76 : 60), mh = uy - 24 - my, rw = 620, lw = BREITE - 2 * RAND_X - 24 - rw;
    karte_rund(RAND_X, my, lw, mh, 22, 1, N_RAND, N_KARTE);
    text(RAND_X + 32, grundlinie(my + 28, 34, 28), 1, 28, N_GRAU, T("Speicher", "Storage"));
    text_rechts(RAND_X + lw - 32, grundlinie(my + 28, 34, 24), 0, 24, N_GRAU2, fertig ? T("Bleibt = System und Geschütztes", "Stays = system and protected data") : T("Was weg kann, siehst du nach dem Nachschauen.", "What can go is shown after scanning."));
    {
        static Uint32 sp_zeit = 0; static bool sp_da[4]; static long long sp_frei[4], sp_ges[4]; static int usb_n = 1;
        if (!sp_zeit || SDL_GetTicks() - sp_zeit > 5000) {
            for (int o = 0; o < 4; o++) sp_da[o] = plattform_speicher(o, &sp_frei[o], &sp_ges[o]) && sp_ges[o] > 0;
            usb_n = plattform_usb_anzahl();   /* Codex-Prüfung 91: alle USB-Laufwerke zusammen */
            sp_zeit = SDL_GetTicks();
        }
        long long gef[4] = { 0, 0, 0, 0 };
        if (fertig) kern_bytes_je_ort(K, gef);
        const char *nm[4] = { T("Interne SSD", "Internal SSD"), T("M.2-SSD", "M.2 SSD"), T("Erweiterter Speicher", "Extended storage"),
                              usb_n > 1 ? a_fmt(fa, T("USB-Laufwerke (%d)", "USB drives (%d)"), usb_n) : T("USB-Laufwerk", "USB drive") };
        const float *sy[4] = { SYM_SSD, SYM_M2, SYM_SSD, SYM_USB };
        int n = 0; for (int o = 0; o < 4; o++) if (sp_da[o]) n++;
        int block = 112, platz = mh - 28 - 34 - 28, abst = n > 1 ? (platz - n * block) / (n + 1) : (platz - block) / 2;
        if (abst > 30) abst = 30;
        int y = my + 28 + 34 + (platz - n * block - (n - 1) * abst) / 2;
        for (int o = 0; o < 4; o++) if (sp_da[o]) { speicher_balken(RAND_X + 32, y, lw - 64, sy[o], nm[o], sp_ges[o], sp_frei[o], gef[o], fertig); y += block + abst; }
        if (n == 0) text(RAND_X + 32, my + mh / 2, 0, 24, N_GRAU2, T("Speicher nicht lesbar", "Storage not readable"));
    }
    int rx = RAND_X + lw + 24;
    karte_rund(rx, my, rw, mh, 22, 1, N_RAND, N_KARTE);
    if (!fertig) {
        text(rx + 32, grundlinie(my + 28, 34, 28), 1, 28, N_GRAU, T("So geht's", "How it works"));
        const char *st[3][2] = { { T("Nachschauen", "Scan"), T("Nur lesen, nichts wird verändert.", "Read only, nothing is changed.") },
                                 { T("Auswählen", "Select"), T("Du entscheidest, was weg soll.", "You decide what should go.") },
                                 { T("Bestätigen", "Confirm"), T("Erst dann wird gelöscht.", "Only then is anything deleted.") } };
        int sy = my + 28 + 34 + 24;
        for (int i = 0; i < 3; i++) {
            char z[4]; snprintf(z, sizeof z, "%d", i + 1);
            if (i == 0) { ring(rx + 32 + 24, sy + 34, 22.5f, 3, N_FOKUS); text_mitte(rx + 56, sy + 42, 1, 22, N_FOKUS, z); }
            else { kreis(rx + 56, sy + 34, 24, N_FELD); text_mitte(rx + 56, sy + 42, 1, 22, N_GRAU, z); }
            text(rx + 32 + 48 + 18, grundlinie(sy, 34, 28), 1, 28, N_TEXT, st[i][0]);
            text(rx + 32 + 48 + 18, grundlinie(sy + 36, 30, 24), 0, 24, N_GRAU, st[i][1]);
            sy += 66 + 12;   /* NEU 1.0.10: 12 statt 22 - Platz für den Hinweis darunter */
        }
        /* NEU 1.0.10 (da_Chris86 13.11.2026 12:45: "Wir müssen schon sicher gehen, dass die Programme zu sind" - Hinweis vor dem Nachschauen,
         * nicht erst vor dem Löschen: dann müsste man PS5 Cleanup beenden und neu anfangen). ShadowMount, ps4mount und Playden prüft und
         * beendet PS5 Cleanup weiter selbst. */
        absatz_max(rx + 32, sy + 4, 1, 22, N_GELB, T("Vorher alle Programme schließen, die weg sollen.", "First close all programs you want to remove."), rw - 64, 28, 1);
        absatz_max(rx + 32, sy + 4 + 30, 0, 20, N_GRAU, T("Am sichersten: PS5 neu starten und nur den Jailbreak und PS5 Cleanup laden.",
                                                         "Safest: restart the PS5 and load only the jailbreak and PS5 Cleanup."), rw - 64, 26, 2);
        knopf_gross(rx + 32, my + mh - 28 - 80, rw - 64, 80, true, KN_X, T("Nachschauen", "Scan"));
    } else {
        long long ges[8] = { 0 }; int anz[8] = { 0 }; bool alle_da = true, gekappt = false, da_k[8]; long long summe = 0;
        for (int ki = 0; ki < KAT_ANZ; ki++) {
            int k = ALLE_KAT[ki]; kblock *b = NULL;
            int nb = kern_bloecke(K, fa, k, SORT_AZ, &b);
            /* NEU 1.0.8 (PS5-Test 06.11.2026, Befunde 4 und 10): hier standen Blöcke (zugeklappte Apps), links in der Auswahl und in der
             * Rückmeldung Einträge - verschiedene Zahlen für denselben Bereich. Jetzt überall Einträge. */
            anz[k] = kern_anzahl(K, k); da_k[k] = true;
            for (int i = 0; i < nb; i++) { if (!b[i].groesse_da) alle_da = da_k[k] = false; else ges[k] += b[i].bytes; if (b[i].gekappt) gekappt = true; }
            summe += ges[k];
        }
        int lauf = 0; { long long g4[4]; kern_bytes_je_ort(K, g4); for (int o = 0; o < 4; o++) if (g4[o] > 0) lauf++; }
        /* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86 11:12 mit Foto: "wenn er zählt, das überlappt sich am Anfang"): beim Zählen stand
         * "mind. 165,5 GB" in 64 Pixeln und daneben "wird noch gezählt …" - zusammen breiter als der Kasten, der Zusatz lief über den Rand.
         * Jetzt beim Zählen ohne "mind." (das sagt schon der Zusatz); die Zahl wird kleiner, bis beides nebeneinander passt (64 bis 40), und
         * passt es auch dann nicht (z. B. "mind. 1,98 TB gefunden auf 3 Laufwerken", wenn das Zählen gekappt wurde), steht der Zusatz unter
         * der Zahl. */
        bool zl = zaehlen_laeuft();
        const char *gross = VERB(fa, (!alle_da ? !zl : gekappt) ? T("mind. ", "min. ") : "", kern_groesse_text(fa, summe));
        /* Codex-Prüfung 106 (P3): die Zahl der Laufwerke stammt aus den gezählten Größen - fehlen Größen (Zählen beendet) oder ist sie 0,
         * steht nur "gefunden" (sonst z. B. "gefunden auf 0 Laufwerken") */
        const char *auf = (!alle_da && zl) ? T("wird noch gezählt …", "still counting …")
                        : (!alle_da || lauf == 0) ? T("gefunden", "found")
                        : a_fmt(fa, lauf == 1 ? T("gefunden auf %d Laufwerk", "found on %d drive") : T("gefunden auf %d Laufwerken", "found on %d drives"), lauf);
        int frei = rw - 64, wa = textbreite(0, 24, auf), gsz = 64;
        while (gsz > 40 && textbreite(1, gsz, gross) + 16 + wa > frei) gsz -= 4;
        if (textbreite(1, gsz, gross) + 16 + wa <= frei) {
            text(rx + 32, grundlinie(my + 24, 80, gsz), 1, gsz, N_TEXT, gross);
            text(rx + 32 + textbreite(1, gsz, gross) + 16, grundlinie(my + 24, 80, 24), 0, 24, N_GRAU, auf);
        } else {
            text_passend(rx + 32, grundlinie(my + 18, 60, 52), 1, 52, 32, N_TEXT, gross, frei);
            text_passend(rx + 32, grundlinie(my + 78, 28, 22), 0, 22, 18, N_GRAU, auf, frei);
        }
        int ly = my + 24 + 80 + 12, zh = (my + mh - 20 - ly) / KAT_ANZ; if (zh > 50) zh = 50;
        for (int ki = 0; ki < KAT_ANZ; ki++) {
            int k = ALLE_KAT[ki];
            rechteck(rx + 32, ly, rw - 64, 1, N_RAND);
            symbol(kat_symbol(k), rx + 32, ly + (zh - 26) / 2, 26, k == 7 ? N_ORANGE : N_GRAU);
            text(rx + 32 + 26 + 16, grundlinie(ly, zh, 26), 0, 26, k == 7 ? N_ORANGE : N_TEXT, kern_kat_name(k));
            /* NEU 1.0.8 (PS5-Test 07.11.2026, Foto da_Chris86 11:15): beim Zählen stand bei noch nicht gezählten Bereichen "0 Byte" - als
             * wären sie leer. Jetzt "zählt …", bis der Bereich fertig gezählt ist (wie links in der Auswahl). */
            const char *gt = !anz[k] ? "–" : (da_k[k] || !zl) ? kern_groesse_text(fa, ges[k]) : T("zählt …", "counting …");
            int sw = textbreite(1, 26, gt); if (sw < 120) sw = 120;   /* 1.0.8 (4.6): "194 bytes" ist breiter als die Spalte - die Zahl rückt nach links */
            text_rechts(rx + rw - 32 - sw - 16, grundlinie(ly, zh, 24), 0, 24, N_GRAU2, a_fmt(fa, "%d", anz[k]));
            text_rechts(rx + rw - 32, grundlinie(ly, zh, 26), 1, 26, N_TEXT, gt);
            ly += zh;
        }
    }
    /* untere Zeile */
    if (!fertig) {
        int cx = RAND_X, cyy = uy + 30;
        text(cx, cyy + 9, 0, 24, N_GRAU2, T("Gesucht wird nach", "Looking for")); cx += textbreite(0, 24, T("Gesucht wird nach", "Looking for")) + 18;
        for (int ki = 0; ki < KAT_ANZ; ki++) {
            int k = ALLE_KAT[ki]; const char *nmk = kern_kat_name(k); int w = 20 + 24 + 10 + textbreite(0, 24, nmk) + 20;
            if (cx + w > BREITE - RAND_X) break;
            karte_rund(cx, cyy - 28, w, 56, 28, 1, N_RAND, N_KARTE);
            symbol(kat_symbol(k), cx + 20, cyy - 12, 24, N_GRAU);
            text(cx + 20 + 24 + 10, cyy + 9, 0, 24, N_TEXT2, nmk);
            cx += w + 12;
        }
    } else {
        text(RAND_X, grundlinie(uy, 76, 24), 0, 24, N_GRAU, K->Funde.n == 0 ? T("Nichts zu tun - die PS5 ist bereits sauber.", "Nothing to do - the PS5 is already clean.")
                                                                           : T("Als Nächstes wählst du aus, was weg soll.", "Next you choose what should go."));
        const char *t1 = T("Nochmal nachschauen", "Scan again"), *t2 = T("Weiter zur Auswahl", "Continue to selection");
        int w2 = knopf_gross_breite(true, t2), w1 = knopf_gross_breite(false, t1);
        knopf_gross(BREITE - RAND_X - w2, uy, w2, 76, true, KN_X, t2);
        knopf_gross(BREITE - RAND_X - w2 - 16 - w1, uy + 4, w1, 68, false, KN_DREIECK, t1);
    }
    fussknopf kn[3] = { { KN_OPT, T("Optionen", "Options"), false }, { KN_O, T("halten: Beenden", "hold: Quit"), false },
                        { KN_QUADRAT, SP_EN ? "Deutsch" : "English", true } };
    fuss_neu(kn, fertig ? 2 : 3);
    if (fern_an() && !optionen_offen && !(hinweis_bis && (Sint32)(hinweis_bis - SDL_GetTicks()) > 0)) {   /* NEU 1.0.9: Adresse der Browser-Bedienung */
        char ad[64]; fern_adresse(ad, sizeof ad); const char *t1 = T("Im Browser: ", "In a browser: ");
        int w1 = textbreite(0, 24, t1), w2 = textbreite(1, 24, ad), x0 = BREITE / 2 - (w1 + w2) / 2, cy = HOEHE - RAND_U - 32;
        text(x0, cy + 9, 0, 24, N_GRAU, t1); text(x0 + w1, cy + 9, 1, 24, N_FOKUS, ad);
    }
    if (optionen_offen) start_optionen();
}

static const char *groesse_von(arena *ar, long long b, bool gekappt) { return VERB(ar, gekappt ? T("mind. ", "min. ") : "", kern_groesse_text(ar, b)); }

/* =================================================================== NEU 1.0.8 (Arbeitsplan 4.3): Auswahl nach Entwurf 2
 * Links die Bereiche, in der Mitte die Einträge als Blöcke (eine Kachel mit allem, was dazugehört - zugeklappt mit Gesamtgröße; der
 * markierte Block ist aufgeklappt), rechts der Kasten zum markierten Eintrag, unten die Auswahl-Leiste. Größte zuerst oder A–Z
 * (Optionen). Was gelöscht wird, entscheidet weiter jede einzelne Zeile (ablauf_eintrag_setzen) - die Blöcke sind nur Anzeige
 * (kern_bloecke, Arbeitsplan 2.6). Gezählt werden überall Einträge (Zeilen), wie in der Zusammenfassung vor dem Löschen.
 * Abweichung vom Entwurf (eigene Entscheidung, 05.11.2026): der Kopf eines Blocks ist zweizeilig (oben der Name, darunter "3 Teile" bzw.
 * was es ist und das Schildchen) - in einer Zeile blieben für lange Spielnamen nur 175 Pixel. */
static eintrag RESTE_ZEILE;                 /* Kopf des Blocks "Reste" - wird nie gelöscht, steht nur für seine Teile */
/* Codex-Prüfung 98 (P1): in die Sammelzeile "Reste" kommt nur wirklich Harmloses - was rot ist, nur einzeln geht oder eine PKG-Datei ist
 * (z. B. ein Ordner "Temporäre Dateien", in dem PS5 Cleanup nicht nach PKG-Dateien suchen konnte), bleibt eine eigene Zeile */
static bool ist_rest(const eintrag *f) { return (f->Art == ART_REST || f->Art == ART_TR_LISTE_REST || f->Art == ART_GALERIE_REST) && !f->Rot && !f->NurEinzeln && !f->Paket && !f->PkgOffen; }
static int sortierung = SORT_GROESSTE;      /* da_Chris86 04.11.2026 17:04: Standard "größte zuerst", A–Z im Options-Menü */
static struct { int k, funde_n, hg, sort; void *funde_e; arena *a; kblock *bl; int n; } AL = { .k = -1 };
static int al_b = 0, al_t = -1, al_py = 0;  /* Markierung: Block, Teil (-1 = Kopf); Bildlauf in Pixeln */
static bool al_mehr(int b) { return b >= 0 && b < AL.n && AL.bl[b].teile.n > 1; }
static eintrag *al_markiert(void)
{
    if (AL.n <= 0 || al_b < 0 || al_b >= AL.n) return NULL;
    return al_t < 0 || !al_mehr(al_b) ? AL.bl[al_b].kopf : AL.bl[al_b].teile.e[al_t];
}
/* Blöcke des Bereichs k; Harmloses ("es geht nichts verloren") als ein Block "Reste" ganz unten (ab zwei, nie bei Spielen und
 * Jailbreak - wie bis 1.0.7). Der markierte Eintrag bleibt beim Umsortieren markiert. */
static int bloecke_holen(arena *ar, int k, int sort, kblock **aus)   /* NEU 1.0.9: aus al_ordnen (auch für den Browser) */
{
    kblock *bl = NULL; int nb = kern_bloecke(K, ar, k, sort, &bl), nr = 0;
#define ALS_REST(x) (!kern_bereiche_106() && k != 1 && k != 7 && (x)->teile.n == 1 && ist_rest((x)->kopf))
    for (int b = 0; b < nb; b++) if (ALS_REST(&bl[b])) nr++;
    if (nr >= 2) {
        kblock *neu = a_hole(ar, sizeof(kblock) * (size_t)(nb - nr + 1));
        kblock r; memset(&r, 0, sizeof r); r.kopf = &RESTE_ZEILE; r.groesse_da = true; r.name = T("Reste", "Leftovers");
        int n = 0;
        for (int b = 0; b < nb; b++) {
            if (!ALS_REST(&bl[b])) { neu[n++] = bl[b]; continue; }
            pl_add(ar, &r.teile, bl[b].kopf);
            if (!bl[b].groesse_da) r.groesse_da = false; else r.bytes += bl[b].bytes;
            if (bl[b].gekappt) r.gekappt = true;
        }
        neu[n++] = r; bl = neu; nb = n;
    }
#undef ALS_REST
    *aus = bl; return nb;
}
static void al_ordnen(int k)
{
    if (k == AL.k && AL.funde_n == K->Funde.n && AL.funde_e == (void *)K->Funde.e && AL.hg == A.hg_n && AL.sort == sortierung) return;
    const eintrag *merk = k == AL.k ? al_markiert() : NULL;
    /* Codex-Prüfung 99 (P2): der erste Teil eines Blocks ist oft derselbe Eintrag wie sein Kopf. War eine Teilzeile markiert, bleibt es eine
     * Teilzeile - bisher sprang die Markierung beim Umsortieren (Größen fertig gezählt, Reihenfolge geändert) auf den Kopf, und ✕ hakte
     * dann den ganzen Block an statt des einen Teils */
    const bool merk_teil = merk && al_t >= 0 && al_mehr(al_b);
    if (k != AL.k) { al_b = 0; al_t = -1; al_py = 0; }
    if (AL.a) arena_frei(AL.a);
    AL.a = arena_neu();
    kblock *bl = NULL; int nb = bloecke_holen(AL.a, k, sortierung, &bl);
    AL.bl = bl; AL.n = nb; AL.k = k; AL.funde_n = K->Funde.n; AL.funde_e = (void *)K->Funde.e; AL.hg = A.hg_n; AL.sort = sortierung;
    if (merk) {
        bool da = false;
        for (int b = 0; b < AL.n && !da; b++) {
            if (!merk_teil && AL.bl[b].kopf == merk) { al_b = b; al_t = -1; da = true; break; }
            for (int t = 0; t < AL.bl[b].teile.n; t++) if (AL.bl[b].teile.e[t] == merk) { al_b = b; al_t = t; da = true; break; }
        }
        for (int b = 0; b < AL.n && !da; b++) if (AL.bl[b].kopf == merk) { al_b = b; al_t = -1; da = true; }   /* als Teil nicht mehr da: sein Block */
    }
    if (al_b >= AL.n) { al_b = AL.n > 0 ? AL.n - 1 : 0; al_t = -1; }
    if (!al_mehr(al_b) || al_t >= AL.bl[al_b].teile.n) al_t = -1;
}
/* Zeilen der Liste: jeder Block mit seinem Kopf (68 Pixel), der markierte Block (nur in der Liste) dazu mit allen Teilen (je 48).
 * y/h in Pixeln, mit dem Rand des Blocks; zwischen den Blöcken 8 Pixel. */
#define AL_KOPF 74
#define AL_TEIL 52
typedef struct { int b, t, y, h; } al_zeile;
static int al_zeilen(al_zeile *z)
{
    int n = 0, y = 0;
    for (int b = 0; b < AL.n; b++) {
        bool auf = fokus_zone == 1 && b == al_b && al_mehr(b);
        int hk = 2 + AL_KOPF + (auf ? 0 : 2);
        z[n++] = (al_zeile){ b, -1, y, hk }; y += hk;
        if (auf) for (int t = 0; t < AL.bl[b].teile.n; t++) { int h = AL_TEIL + (t == AL.bl[b].teile.n - 1 ? 4 : 0); z[n++] = (al_zeile){ b, t, y, h }; y += h; }
        y += 8;
    }
    return n;
}
static int al_zustand(int b)   /* Haken des Blocks: 0 nichts, 1 teilweise, 2 alles gewählt */
{
    int w = 0, m = 0; const char *g;
    for (int i = 0; i < AL.bl[b].teile.n; i++) { eintrag *f = AL.bl[b].teile.e[i]; if (ablauf_eintrag_gesperrt(&A, f, &g)) continue; m++; if (f->Wahl) w++; }
    return w == 0 ? 0 : w == m ? 2 : 1;
}
static void eintrag_umschalten(eintrag *f)
{
    const char *g;
    if (ablauf_eintrag_gesperrt(&A, f, &g)) { kurz(g ? g : T("Nicht wählbar.", "Not selectable.")); return; }
    bool vorher = f->Wahl;
    ablauf_eintrag_setzen(&A, f, !f->Wahl);
    /* NEU 1.0.6 (da_Chris86 29.10.2026 07:28): beim Anwählen eines Jailbreak-Einstiegs deutlich warnen */
    if (!vorher && f->Wahl && f->Jailbreak) { kurz(kern_jb_warnung(fa, f)); hinweis_dauer(8000); }
    else if (vorher && !f->Wahl && f->Jailbreak) hinweis_bis = 0;   /* da_Chris86 29.10.2026 (Fernsehtest): abgewählt - Warnung sofort weg */
}
/* Teile, die ✕ auf dem Kopf nicht mit anhakt: was endgültig weg ist oder nur einzeln geht (wie bei "ganzen Bereich wählen") */
static bool nur_einzeln_teil(int b, const eintrag *f) { (void)b; return f->Rot || f->Erkannt || f->NurEinzeln || f->Paket || kern_selbst_hochgeladen(f) || kern_db_sicherung(f) || f->PlSpiel || (f->KachelId && !strncmp(f->KachelId, "FAKE78", 6)); }   /* NEU 1.0.10 (da_Chris86 14.11.2026 10:40), Antworten b und c): Sicherungskopie und Playden-Spiele nicht mit dem Block-Haken */   /* 1.0.10 (da_Chris86 13.11.2026): selbst Hochgeladenes nicht mit dem Block */   /* Paket: Codex-Prüfung 90; Codex-Prüfung 98: auch bei den Resten */
/* ✕ auf dem Kopf eines Blocks: alle anderen Teile an bzw. aus. Abwählen nimmt alles raus. Bei den Resten geht alles. */
static void block_setzen_p(const pliste *p, bool reste, bool browser)   /* NEU 1.0.9: aus al_block_setzen (auch für den Browser) */
{
    const char *g; const int b = 0;
    if (p->n == 1 && !reste) { eintrag_umschalten(p->e[0]); return; }
    int frei = 0, an = 0, alle_an = 0;
    for (int i = 0; i < p->n; i++) {
        eintrag *f = p->e[i]; if (ablauf_eintrag_gesperrt(&A, f, &g)) continue;
        if (f->Wahl) alle_an++;
        if (nur_einzeln_teil(b, f)) continue;
        frei++; if (f->Wahl) an++;
    }
    if (frei == 0 && alle_an == 0) {
        kurz(browser ? T("Diese Teile nur einzeln anhaken: den Block aufklappen und einzeln wählen.", "Tick these parts one by one: open the block and select them individually.")
                     : T("Diese Teile nur einzeln anhaken: mit ↓ in den Block, dann ✕.", "Tick these parts one by one: ↓ into the block, then ✕."));
        return;
    }
    bool ein = frei > 0 && an < frei;
    for (int i = 0; i < p->n; i++) {
        eintrag *f = p->e[i]; if (ablauf_eintrag_gesperrt(&A, f, &g)) continue;
        if (ein && nur_einzeln_teil(b, f)) continue;
        if (f->Wahl != ein) ablauf_eintrag_setzen(&A, f, ein);
    }
}
static void al_block_setzen(int b) { block_setzen_p(&AL.bl[b].teile, AL.bl[b].kopf == &RESTE_ZEILE, false); }
static void al_schritt(int d)
{
    if (AL.n <= 0) return;
    if (d > 0) {
        if (al_mehr(al_b) && al_t < AL.bl[al_b].teile.n - 1) al_t++;   /* in den Block hinein bzw. zum nächsten Teil */
        else if (al_b < AL.n - 1) { al_b++; al_t = -1; }
    } else {
        if (al_t >= 0) al_t--;                                           /* vom ersten Teil zurück auf den Kopf */
        else if (al_b > 0) { al_b--; al_t = -1; }
    }
}
/* ---- kleine Bausteine der Auswahl */
static const char *ohne_ort_vorne(const char *t)
{
    for (int o = ORT_M2; o <= ORT_STICK; o++) { const char *v = kern_ort_vorsatz(o); if (v && v[0] && txt_beginnt(t, v)) return t + strlen(v); }
    return t;
}
/* F8 (da_Chris86s Bilder vom 04.11.2026): die Kachel eines ShadowMount-Spiels liegt intern - "M.2" gehört nur zur Zeile der Spieldatei */
static int anzeige_ort(const eintrag *f) { return f->Quelle && f->Quelle[0] && !f->SmDatei ? ORT_INTERN : f->Ort; }
static const char *ort_kurz(int o) { return o == ORT_M2 ? "M.2" : o == ORT_USB ? T("USB-Platte", "USB HDD") : o == ORT_STICK ? "USB" : ""; }
static const char *al_name(const eintrag *f) { const char *n = kern_name(fa, f); return n && n[0] ? n : ohne_ort_vorne(kern_zeile(fa, f)); }
/* Was nach "Name – " in der Zeile steht ("Grand Theft Auto V – Spielstände" -> "Spielstände"); NULL = die Zeile fängt nicht so an */
static const char *nach_name(const char *z, const char *n)
{
    size_t l = n ? strlen(n) : 0, s = strlen(" – ");
    if (l && !strncmp(z, n, l) && !strncmp(z + l, " – ", s) && z[l + s]) return z + l + s;
    return NULL;
}
/* Zeile eines Teils ohne den Namen des Blocks und ohne den "Jailbreak-Start: …", der schon am Kopf steht */
static const char *kopf_kurz(const eintrag *f, const char *block);
static const char *teil_name(int b, const eintrag *f)
{
    const char *kk = f == AL.bl[b].kopf ? kopf_kurz(f, AL.bl[b].name) : NULL;   /* NEU 1.0.10 (13.11.2026): Spieldatei ohne Kachel als Kopf */
    if (kk) return kk;
    const char *z = ohne_ort_vorne(kern_zeile(fa, f)), *r = nach_name(z, AL.bl[b].name);
    if (r) z = r;
    const char *kz = ohne_ort_vorne(kern_zeile(fa, AL.bl[b].kopf)), *js = T(" – Jailbreak-Start", " – jailbreak start");
    const char *p = strstr(kz, js), *q = strstr(z, js);
    if (p && q && q > z && !strcmp(p, q)) z = a_kopie_n(fa, z, (size_t)(q - z));
    return z;
}
/* im Bereich Jailbreak steht in jeder Zeile "Jailbreak-Start: …" - dort reicht der Weg selbst ("Y2JB (YouTube)"), der Bereich sagt den Rest */
static const char *ohne_jb_start(const char *t, int k)
{
    if (k != 7) return t;
    const char *v = T("Jailbreak-Start: ", "jailbreak start: ");
    return r_r(fa, txt_beginnt(t, v) ? t + strlen(v) : t, T(" – Jailbreak-Start: ", " – jailbreak start: "), " – ");
}
static const char *schild_text(const eintrag *f, int k) { return k == 7 && f->Art == ART_JAILBREAK ? T("neu einrichten", "set up again") : kern_folge_kurz(f); }
/* Name und Zusatz einer Zeile für die zweizeilige Anzeige ("Grand Theft Auto V" / "Spielstände"):
 * - "Name – Zusatz" (der Name kommt aus dem Kern, kern_name)
 * - "Aufnahmen von Name (ID): 1 Screenshot" -> Name / "1 Screenshot"
 * - "Kurzer Vorsatz: Rest" (Fake-Lizenz, App-Daten ohne App, Kachel ohne Spiel, Spieldatei ohne Kachel ...) -> Rest / Vorsatz
 * Sonst die ganze Zeile als Name, ohne Zusatz. */
static void zeile_teilen(const eintrag *f, int k, const char **name, const char **was)
{
    const char *z = ohne_ort_vorne(kern_zeile(fa, f)), *n = al_name(f), *r = nach_name(z, n);
    *name = z; *was = "";
    if (r) { *name = n; *was = ohne_jb_start(r, k); return; }
    const char *v = T("Aufnahmen von ", "Captures of "), *e = strstr(z, "): ");
    if (txt_beginnt(z, v) && e) {
        const char *p = e; while (p > z && *p != '(') p--;
        if (p > z + strlen(v) + 1 && p[-1] == ' ') { *name = a_kopie_n(fa, z + strlen(v), (size_t)(p - 1 - (z + strlen(v)))); *was = e + 3; return; }
    }
    const char *d = strstr(z, ": ");
    if (d && d - z <= 40 && d[2]) {
        const char *s = strstr(z, " – ");
        if (!s || s > d) { *name = d + 2; *was = a_kopie_n(fa, z, (size_t)(d - z)); }
        /* NEU 1.0.10 (da_Chris86 13.11.2026 21:04: "man muss nicht nach BlackBox noch die Dateiinfo hinschreiben"): Spieldatei ohne Kachel - in
         * der Liste nur der Name; Ordner und Dateiname in Klammern stehen weiter in der Zeile des Kerns (Protokoll, Info). Sie bleiben nur, wenn
         * zwei Spieldateien desselben Spiels am selben Ort liegen (sonst wären sie nicht zu unterscheiden). */
        if (f->Art == ART_ABBILD && *name == d + 2) {
            bool zwei = false;
            for (int x = 0; x < f->Doppelt.n; x++) { const eintrag *g = f->Doppelt.e[x]; if (g != f && g->Art == ART_ABBILD && g->Ort == f->Ort) zwei = true; }
            const char *kn = kern_name(fa, f); size_t lk = strlen(kn);
            if (!zwei && lk > 0 && !strncmp(*name, kn, lk) && (*name)[lk] == ' ' && (*name)[lk + 1] == '(') {
                const char *rest = *name + lk, *zu = strstr(rest, ") – ");
                size_t lr = strlen(rest);
                if (zu) *name = VERB(fa, kn, zu + 1);                 /* dahinter steht noch etwas (z. B. "– doppelt: ...") */
                else if (lr > 0 && rest[lr - 1] == ')') *name = kn;
            }
        }
    }
}
/* NEU 1.0.10 (PS5-Test 13.11.2026 21:38, Zeile "Spieldatei ohne Kachel: Playd…" abgeschnitten): die Zeile des Kopfs im aufgeklappten Block
 * einer Spieldatei ohne Kachel - der Name steht schon in der Kopfzeile darüber, hier reicht, was es ist */
static const char *kopf_kurz(const eintrag *f, const char *block)
{
    if (!f || f->Art != ART_ABBILD || !block) return NULL;
    const char *nm, *w; zeile_teilen(f, f->Kat, &nm, &w);
    return (w[0] && !strcmp(nm, block)) ? w : NULL;
}
static uint32_t ampel_f(int a) { return a == AMPEL_ROT ? N_ROT : a == AMPEL_GELB ? N_GELB : N_GRUEN; }
static const float *ampel_sym(int a) { return a == AMPEL_ROT ? SYM_WARNUNG : a == AMPEL_GELB ? SYM_NEU : SYM_HAKEN; }
static int schild_breite(const char *t) { return 11 + 18 + 7 + textbreite(1, 22, t) + 13; }
static void schild(int x, int cy, int a, const char *t, uint32_t hg)   /* Schildchen mit Symbol: rot "endgültig weg", gelb "neu laden" ... */
{
    uint32_t f = ampel_f(a); int w = schild_breite(t);
    if (a == AMPEL_ROT) karte_rund(x, cy - 16, w, 32, 16, 2, farbmisch(f, hg, .6f), farbmisch(f, hg, .16f));
    else rund(x, cy - 16, w, 32, 16, farbmisch(f, hg, .12f));
    symbol(ampel_sym(a), x + 11, cy - 9, 18, f);
    text(x + 36, cy + 8, 1, 22, f, t);
}
static int ort_breite(const char *o) { return o[0] ? textbreite(1, 22, o) + 20 : 0; }
static void ort_schild(int x, int cy, const char *o) { rund(x, cy - 15, ort_breite(o), 30, 8, 0x2A2F37); text(x + 10, cy + 8, 1, 22, N_GRAU, o); }
static void kasten_haken(int x, int y, int z, uint32_t hg, bool gesperrt, bool jb)   /* 28 x 28: 0 leer, 1 teilweise, 2 gewählt */
{
    if (gesperrt) { karte_rund(x, y, 28, 28, 8, 2, N_LINIE, hg); symbol(SYM_SCHLOSS, x + 5, y + 5, 18, N_GRAU2); return; }
    uint32_t f = jb ? N_ORANGE : N_BLAU;
    if (z == 2) { rund(x, y, 28, 28, 8, f); symbol(SYM_HAKEN, x + 4, y + 4, 20, 0xFFFFFF); }
    else if (z == 1) { karte_rund(x, y, 28, 28, 8, 3, jb ? N_ORANGE : N_FOKUS, hg); rund(x + 7, y + 12, 14, 3, 1, jb ? N_ORANGE : N_FOKUS); }
    else karte_rund(x, y, 28, 28, 8, 3, 0x5A616D, hg);
}
static void kachel_klein(int x, int y, int gr, const eintrag *f, int k, uint32_t hg)   /* Kachelbild, sonst Symbol des Bereichs */
{
    SDL_Surface *b = (f && f != &RESTE_ZEILE) ? kachelbild(f, gr) : NULL;
    if (b) { SDL_Rect z = { x, y, gr, gr }; SDL_BlitSurface(b, NULL, ziel, &z); ecken_maske(x, y, gr, gr, gr / 4, hg); return; }
    rund(x, y, gr, gr, gr / 4, N_FELD);
    symbol(f == &RESTE_ZEILE ? SYM_SONSTIGES : kat_symbol(k), x + gr / 4, y + gr / 4, gr / 2, k == 7 ? N_ORANGE : N_GRAU);
}
static const char *block_groesse(int b)
{
    const kblock *x = &AL.bl[b];
    return x->groesse_da ? groesse_von(fa, x->bytes, x->gekappt) : "…";
}
static int block_rot(int b) { int n = 0; for (int i = 0; i < AL.bl[b].teile.n; i++) if (kern_ampel(AL.bl[b].teile.e[i]) == AMPEL_ROT) n++; return n; }
/* NEU 1.0.8 (Vorschau 06.11.2026, Block "WebKit Autoloader"): Dateinamen in der Mitte kürzen ("webkit-autolo…er_v0.6.2.elf"), damit Version
 * und Endung sichtbar bleiben - zwei Dateien desselben Tools sahen sonst gleich aus ("webkit-autoloader-…") */
static bool endet_wie_datei(const char *t)
{
    const char *p = strrchr(t, '.');
    if (!p || p == t || !p[1] || strlen(p + 1) > 6) return false;
    for (const char *q = p + 1; *q; q++) if (!((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') || (*q >= '0' && *q <= '9'))) return false;
    return true;
}
static const char *kuerzen_mitte(int f, int gr, const char *t, int breite)
{
    int tw = textbreite(f, gr, t), el = textbreite(f, gr, "…");
    if (tw <= breite) return t;
    if (breite - el < 60) return kuerzen(f, gr, t, breite);
    zu_lang("in der Mitte gekürzt", gr, breite, t);
    int platz = breite - el, hinten = platz * 45 / 100, vorne = platz - hinten, b = 0;
    const char *q = t, *kopf = t;
    while (*q) { Glyph *g = glyph(f, gr, cp_naechstes(&q)); b += g ? g->vorschub : 0; if (b > vorne) break; kopf = q; }
    q = t; b = 0;
    while (*q && tw - b > hinten) { Glyph *g = glyph(f, gr, cp_naechstes(&q)); b += g ? g->vorschub : 0; }
    if (q < kopf) q = kopf;
    return VERB(fa, a_kopie_n(fa, t, (size_t)(kopf - t)), "…", q);
}
/* Text in einer Zeile: zu lang -> erst kleiner, in der markierten Zeile dann Laufschrift, sonst "…" (ganz steht er im Kasten rechts).
 * Rückgabe: gezeichnete Breite */
static int name_zeichnen(int x, int y, int f, int gr, int gr_min, uint32_t farbe, const char *t, int breite, bool fok, const void *wer)
{
    if (breite < 20) return 0;
    int g = gr; while (g - 2 >= gr_min && textbreite(f, g, t) > breite) g -= 2;
    int tw = textbreite(f, g, t);
    if (tw <= breite) { text(x, y, f, g, farbe, t); return tw; }
    if (fok) laufschrift(x, y, f, g, farbe, t, breite, wer); else text(x, y, f, g, farbe, endet_wie_datei(t) ? kuerzen_mitte(f, g, t, breite) : kuerzen(f, g, t, breite));
    return breite;
}
/* ---- links: Bereiche */
/* 1.0.8 (4.6): breite = Platz für die Zeile (0 = egal). Passt "53 Einträge · mind. 736,0 GB" nicht, steht nur die Größe da - bis hier wurde
 * die Zeile gekürzt ("53 Einträge · wird gez…", auf der PS5 so lange, wie das Zählen dauert) */
static const char *bereich_unter(int k, int breite)
{
    int n = 0, w = 0; long long b = 0; bool alle = true, gek = false;
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i]; if (f->Kat != k) continue;
        n++; if (f->Wahl) w++;
        groesse *g = kern_eintrag_groesse_in(K, fa, f, true);
        if (!g) alle = false; else { b += g->Bytes; if (g->Gekappt) gek = true; }
    }
    if (w > 0) return a_fmt(fa, T("%d von %d gewählt", "%d of %d selected"), w, n);
    bool zl = zaehlen_laeuft();   /* Codex-Prüfung 105 (P2): nach beendetem Zählen mit fehlenden Größen "mind. …", nicht endlos "zählt …" */
    const char *gr = alle ? groesse_von(fa, b, gek) : zl ? T("zählt …", "counting …") : groesse_von(fa, b, true);
    const char *t = VERB(fa, a_fmt(fa, "%d", n), EINTRAEGE(n), " · ", gr);
    if (breite > 0 && textbreite(0, 22, t) > breite) t = (alle || !zl) ? gr : VERB(fa, a_fmt(fa, "%d", n), EINTRAEGE(n), " …");
    return t;
}
static void al_bereich_zeile(int x, int y, int w, int i, int k)
{
    bool fok = i == fokus_bereich, jb = k == 7;
    uint32_t hg = fok ? farbmisch(N_BLAU, N_KARTE, .22f) : N_KARTE;
    if (fok && fokus_zone == 0) karte_rund(x, y, w, 72, 16, 3, N_FOKUS, hg); else if (fok) rund(x, y, w, 72, 16, hg);
    int z = ablauf_bereich_zustand(&A, k);
    if (z == 2) {   /* "voll" nur, wenn wirklich alle wählbaren Einträge angehakt sind (bei "nur einzeln" sonst halb) */
        int m = 0, ww = 0; for (int j = 0; j < K->Funde.n; j++) { eintrag *f = K->Funde.e[j]; if (f->Kat == k && !f->Gesperrt) { m++; if (f->Wahl) ww++; } }
        if (ww < m) z = 1;
    }
    kasten_haken(x + 16, y + 22, z, hg, false, jb);
    symbol(kat_symbol(k), x + 60, y + 22, 28, jb ? N_ORANGE : (fok ? N_TEXT : N_GRAU));
    text_passend(x + 104, y + 33, 1, 26, 22, jb ? N_ORANGE : (fok ? 0xFFFFFF : N_TEXT), kern_kat_name(k), w - 120);
    text_passend(x + 104, y + 63, 0, 24, 22, fok ? N_TEXT2 : N_GRAU2, bereich_unter(k, w - 120), w - 120);
}
/* ---- rechts: Kasten zum markierten Eintrag. Alles ganz zeigen, Schrift mindestens 24 (Entwurf 2): passt es nicht, wird erst das Bild
 * kleiner, dann fällt es weg; reicht auch das nicht, laufen Fragen und Antworten langsam nach oben durch (wie die Laufschrift der Namen:
 * 3 s stehen, durchlaufen, 3 s stehen, wieder von vorn) - abgeschnitten wird nichts. */
typedef struct { const char *frage, *text; uint32_t farbe; const float *sym; } k_absatz;
static int kasten_absaetze(int tx, int ty, int bw, const k_absatz *ab, int n, bool zeichnen)
{
    int y = ty, letzt = ty;
    for (int i = 0; i < n; i++) {
        const k_absatz *a = &ab[i];
        if (!a->text || !a->text[0]) continue;
        if (a->frage) { if (zeichnen) text(tx, y + 24, 0, 24, N_GRAU2, a->frage); y += 36; }
        int ax = tx + (a->sym ? 34 : 0);
        zeilen z = umbrechen(0, 24, a->text, bw - (ax - tx));
        if (zeichnen) {
            if (a->sym) symbol(a->sym, tx, y + 4, 24, a->farbe);
            for (int j = 0; j < z.n; j++) text(ax, y + 24 + j * 32, 0, 24, a->farbe, z.z[j]);
        }
        y += z.n * 32; letzt = y; y += 18;
    }
    return letzt - ty;
}
static const void *kasten_wer; static Uint32 kasten_seit;
static void kasten_zeichnen(int x, int y, int w, int h, const eintrag *bild_f, int k, const char *titel, const char *unter, const k_absatz *ab, int n, const void *wer)
{
    karte_rund(x, y, w, h, 22, 1, N_RAND, N_KARTE);
    int tx = x + 28, bw = w - 56, oben = y + 26, unten = y + h - 24;
    int inhalt = kasten_absaetze(tx, 0, bw, ab, n, false);
    /* NEU 1.0.8 (Arbeitsplan 4.6): sehr lange Namen (z. B. "UP0000-SLIB00001_00 - Spiel/App unbekannt (Spiel nicht mehr installiert)")
     * dürfen bis zu 4 Zeilen haben, statt nach 2 Zeilen mit "…" zu enden. Was in 2 Zeilen passt, bleibt wie bisher. */
    int mz = umbrechen(1, 24, titel, bw).n <= 2 ? 2 : 4;
    /* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 13, da_Chris86: "rechts ist das Symbol mal kleiner und mal größer"): das Bild ist immer gleich
     * groß (120 Pixel). Bis hier wurde es je nach Textmenge 180, 140 oder 100 groß oder fiel ganz weg - passt der Text nicht, laufen die
     * Absätze darunter wie bisher langsam durch. */
    passend tp = einpassen(1, 30, 26, 24, titel, bw, mz); tp.lh = tp.gr + 8;
    int ty = oben, bg = 120;
    if (bg) {
        if (bild_f) kachel_klein(x + (w - bg) / 2, ty, bg, bild_f, k, N_KARTE);
        else { rund(x + (w - bg) / 2, ty, bg, bg, bg * 2 / 9, N_FELD); symbol(kat_symbol(k), x + (w - bg) / 2 + bg / 4, ty + bg / 4, bg / 2, k == 7 ? N_ORANGE : N_GRAU); }
        ty += bg + 22;
    }
    passend_zeichnen(tx, ty + tp.gr, 1, k == 7 && !bild_f ? N_ORANGE : N_TEXT, &tp); ty += tp.z.n * tp.lh + 4;
    if (unter[0]) { text_passend(tx, ty + 24, 0, 24, 22, N_GRAU2, unter, bw); ty += 34; }
    ty += 14;
    int platz = unten - ty, off = 0;
    if (inhalt > platz) {
        if (wer != kasten_wer) { kasten_wer = wer; kasten_seit = SDL_GetTicks(); }
        off = lauf_versatz(kasten_seit, inhalt - platz + 8, 28, 3000);   /* 1.0.8 (4.7) */
        clip_y0 = ty - 2; clip_y1 = unten + 8;
    }
    kasten_absaetze(tx, ty - off, bw, ab, n, true);
    clip_y0 = 0; clip_y1 = 1 << 30;
    if (inhalt > platz) {   /* weich ausblenden, wo es weitergeht: unten immer, oben, sobald es läuft */
        for (int i = 0; i < 30; i++) { float al = (float)(i + 1) / 30.f; for (int px = x + 2; px < x + w - 2; px++) misch(px, unten + 8 - 30 + i, N_KARTE, al); }
        if (off > 0) for (int i = 0; i < 16; i++) { float al = (float)(16 - i) / 16.f; for (int px = x + 2; px < x + w - 2; px++) misch(px, ty - 2 + i, N_KARTE, al); }
    }
}
static bool ordner_zeigen = false;   /* Options-Menü (da_Chris86 04.11.2026 16:20: "Ordner anzeigen" im Options-Menü) */
/* Codex-Prüfung 91: in einem Stück zusammengesetzt (Zeile für Zeile neu kostete bei 5000 Pfaden über 1 GB) und höchstens ORDNER_MAX
 * Pfade - danach "… und N weitere" */
#define ORDNER_MAX 100
static const char *ordner_text(const eintrag *f)
{
    int n = f->Pfade.n < ORDNER_MAX ? f->Pfade.n : ORDNER_MAX;
    const char *mehr = f->Pfade.n > n ? a_fmt(fa, T("… und %d weitere", "… and %d more"), f->Pfade.n - n) : NULL;
    size_t g = 1 + (mehr ? strlen(mehr) + 1 : 0);
    for (int i = 0; i < n; i++) g += strlen(f->Pfade.e[i]) + 1;
    char *t = a_hole(fa, g), *o = t;
    for (int i = 0; i < n; i++) { if (i) *o++ = '\n'; size_t l = strlen(f->Pfade.e[i]); memcpy(o, f->Pfade.e[i], l); o += l; }
    if (mehr) { if (n) *o++ = '\n'; size_t l = strlen(mehr); memcpy(o, mehr, l); o += l; }
    *o = 0;
    return t;
}
static void al_kasten(int x, int y, int w, int h, int k)
{
    eintrag *f = fokus_zone == 1 ? al_markiert() : NULL;
    int b = al_b;
    k_absatz ab[6]; memset(ab, 0, sizeof ab);
    if (!f) {   /* Bereich markiert */
        ab[0] = (k_absatz){ T("Was ist das?", "What is it?"), kern_kat_info(k), N_TEXT, NULL };
        ab[1] = (k_absatz){ NULL, ablauf_bereich_nur_einzeln(&A, k) ? T("Hier nur einzeln wählbar: mit → in die Liste, dann ✕.", "Individually only here: → into the list, then ✕.")
                                                                    : T("✕ wählt den ganzen Bereich, ohne was endgültig weg wäre. → geht in die Liste.", "✕ selects the whole area, without what would be gone for good. → enters the list."),
                            k == 7 ? N_ORANGE : N_GRAU, NULL };
        kasten_zeichnen(x, y, w, h, NULL, k, kern_kat_name(k), bereich_unter(k, w - 56), ab, 2, &sicht[fokus_bereich]);
        return;
    }
    const kblock *bl = &AL.bl[b];
    const char *unter_b = VERB(fa, a_fmt(fa, T("%d Teile", "%d parts"), bl->teile.n), " · ", block_groesse(b));
    if (f == &RESTE_ZEILE) {
        ab[0] = (k_absatz){ T("Was wird gelöscht?", "What is deleted?"), T("Reste von Spielen und Apps, die es nicht mehr gibt, Zwischenspeicher und leere Ordner.",
                                                                          "Leftovers of games and apps that are gone, caches and empty folders."), N_TEXT, NULL };
        ab[1] = (k_absatz){ T("Was passiert danach?", "What happens then?"), T("Es geht nichts verloren.", "Nothing is lost."), N_GRUEN, SYM_HAKEN };
        kasten_zeichnen(x, y, w, h, NULL, k, bl->name, unter_b, ab, 2, &RESTE_ZEILE);
        return;
    }
    if (al_t < 0 && al_mehr(b)) {   /* Kopf eines Blocks mit mehreren Teilen */
        const char *teile = ""; int n = bl->teile.n, rot = block_rot(b), einz = 0, frei = 0; const char *g;
        for (int i = 0; i < n && i < 6; i++) teile = VERB(fa, teile, i ? (i == n - 1 ? T(" und ", " and ") : ", ") : "", teil_name(b, bl->teile.e[i]));
        if (n > 6) teile = VERB(fa, teile, a_fmt(fa, T(" und %d weitere", " and %d more"), n - 6));
        for (int i = 0; i < n; i++) { eintrag *e = bl->teile.e[i]; if (ablauf_eintrag_gesperrt(&A, e, &g)) continue; if (nur_einzeln_teil(b, e)) einz++; else frei++; }
        const char *da = kern_art_info(fa, bl->kopf); int a = kern_ampel(bl->kopf);
        if (!da[0]) da = kern_folge(bl->kopf);
        ab[0] = (k_absatz){ T("Was wird gelöscht?", "What is deleted?"), VERB(fa, teile, "."), N_TEXT, NULL };
        ab[1] = (k_absatz){ T("Was passiert danach?", "What happens then?"), da, ampel_f(a), ampel_sym(a) };
        const char *hw = NULL;
        if (einz > 0 && frei == 0) hw = rot ? T("Was rot ist, ist danach endgültig weg. Hier hakst du jeden Teil einzeln an: mit ↓ in den Block, dann ✕.",
                                                "What is red is gone for good afterwards. Tick each part here one by one: ↓ into the block, then ✕.")
                                            : T("Hier hakst du jeden Teil einzeln an: mit ↓ in den Block, dann ✕.", "Tick each part here one by one: ↓ into the block, then ✕.");
        else if (rot > 0) hw = rot == 1 ? T("1 Teil davon ist danach endgültig weg. ✕ auf dieser Zeile lässt ihn aus - den hakst du einzeln an.",
                                            "1 part of it is gone for good afterwards. ✕ on this row leaves it out - tick it individually.")
                                        : a_fmt(fa, T("%d Teile davon sind danach endgültig weg. ✕ auf dieser Zeile lässt sie aus - die hakst du einzeln an.",
                                                      "%d parts of it are gone for good afterwards. ✕ on this row leaves them out - tick them individually."), rot);
        else if (einz > 0) hw = einz == 1 ? T("1 Teil geht nur einzeln: ✕ auf dieser Zeile lässt ihn aus.", "1 part only goes individually: ✕ on this row leaves it out.")
                                          : a_fmt(fa, T("%d Teile gehen nur einzeln: ✕ auf dieser Zeile lässt sie aus.", "%d parts only go individually: ✕ on this row leaves them out."), einz);
        if (hw) ab[2] = (k_absatz){ NULL, hw, rot ? N_ROT : N_GELB, rot ? SYM_WARNUNG : SYM_INFO };
        kasten_zeichnen(x, y, w, h, bl->kopf, k, bl->name, unter_b, ab, 3, (const char *)bl->kopf + 1);
        return;
    }
    /* eine Zeile (Block aus einer Zeile oder ein Teil) */
    const char *grund = NULL; bool gesp = ablauf_eintrag_gesperrt(&A, f, &grund);
    const char *gr = h_trim(fa, ablauf_eintrag_groesse(&A, f)); int o = anzeige_ort(f);
    const char *unter = VERB(fa, gr, o != ORT_INTERN ? VERB(fa, gr[0] ? " · " : "", kern_ort_name(o)) : "");
    const char *nm, *ws; zeile_teilen(f, k, &nm, &ws);
    int a = kern_ampel(f);
    const char *da = kern_art_info(fa, f); if (!da[0]) da = kern_folge(f);
    ab[0] = (k_absatz){ T("Was wird gelöscht?", "What is deleted?"), ohne_ort_vorne(kern_zeile(fa, f)), N_TEXT, NULL };
    if (ordner_zeigen) ab[1] = (k_absatz){ f->Pfade.n == 1 ? T("Ordner", "Folder") : T("Ordner", "Folders"), ordner_text(f), N_GRAU, NULL };
    /* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 3): eine Zeile, die sich gar nicht wählen lässt (Update eines installierten Spiels, Game
     * Compressor), braucht kein "Was passiert danach?" - dort stand derselbe Satz wie im Grund darunter ("Geht nur zusammen mit dem Spiel
     * weg"). Dann steht nur der Grund da, mit der Frage "Warum nicht wählbar?". */
    bool nie_waehlbar = gesp && grund && grund[0] && (f->Gesperrt || kern_nur_mit_spiel(K, f));
    if (nie_waehlbar) ab[2] = (k_absatz){ T("Warum nicht wählbar?", "Why can't I select it?"), grund, N_GELB, SYM_SCHLOSS };
    else {
        ab[2] = (k_absatz){ T("Was passiert danach?", "What happens then?"), da, ampel_f(a), ampel_sym(a) };
        if (gesp && grund && grund[0]) ab[3] = (k_absatz){ NULL, grund, N_GELB, SYM_SCHLOSS };
        else if (f->Jailbreak) ab[3] = (k_absatz){ NULL, kern_jb_hinweis(fa, f), N_ORANGE, SYM_JAILBREAK };
    }
    kasten_zeichnen(x, y, w, h, f, k, nm, unter, ab, 4, f);
}
/* ---- Knöpfe der Auswahl-Leiste */
static void leisten_knopf(int x, int y, int w, int h, bool an, bool rot, knopfart k, const char *t)
{
    int cy = y + h / 2;
    if (rot) karte_rund(x, y, w, h, 16, 2, an ? farbmisch(N_ROT, N_KARTE, .6f) : N_LINIE, N_KARTE);
    else if (an) rund(x, y, w, h, 16, N_BLAU); else karte_rund(x, y, w, h, 16, 2, N_LINIE, N_KARTE);
    uint32_t tf = rot ? (an ? N_ROT : N_GRAU2) : (an ? 0xFFFFFF : N_GRAU2);
    int tw = textbreite(1, 26, t), gx = x + (w - (36 + 14 + tw)) / 2;
    ring(gx + 18, cy, 17, 2, tf);
    symbol(k == KN_DREIECK ? SYM_KN_DREIECK : SYM_KN_QUADRAT, gx + 10, cy - 8, 16, tf);
    text(gx + 50, cy + 9, 1, 26, tf, t);
}
static int leisten_knopf_breite(const char *t) { return 36 + 14 + textbreite(1, 26, t) + 2 * 26; }
static void al_optionen(void)   /* Auswahl: Reihenfolge, Ordner anzeigen, Testmodus, Protokoll & Rückmeldung, Browser-Bedienung (1.0.9) */
{
    const char *z[5] = { sortierung == SORT_AZ ? T("Reihenfolge: A–Z", "Order: A–Z") : T("Reihenfolge: größte zuerst", "Order: largest first"),
                         ordner_zeigen ? T("Ordner anzeigen: an", "Show folders: on") : T("Ordner anzeigen: aus", "Show folders: off"),
                         testmodus_zeile(), T("Protokoll & Rückmeldung", "Log & feedback"), fern_zeile_opt() };
    const char *u[5] = { sortierung == SORT_AZ ? T("✕ stellt um auf: größte zuerst", "✕ switches to: largest first") : T("✕ stellt um auf: A–Z", "✕ switches to: A–Z"),
                         ordner_zeigen ? T("✕ blendet die Ordner im Kasten rechts wieder aus", "✕ hides the folders in the box on the right again")
                                       : T("✕ zeigt im Kasten rechts, in welchen Ordnern es liegt", "✕ shows in the box on the right which folders it is in"),
                         testmodus_satz(), T("Was bisher passiert ist, dazu die freiwillige Rückmeldung", "What happened so far, plus the optional feedback"), fern_satz() };
    const float *sy[5] = { SYM_SORTIEREN, SYM_SSD, SYM_SCHILD_HAKEN, SYM_DOKUMENT, SYM_KONSOLE };
    optionen_zeichnen(z, u, sy, 5);
}
/* ---- eine Zeile der Liste: Kopf eines Blocks (zweizeilig) bzw. Teil eines aufgeklappten Blocks */
/* Schildchen eines Blocks ohne Auswahl (Kopf bzw. einzige Zeile) - und ob es im ganzen Bereich gleich ist */
static const char *al_schild(int b, int k, int *a)
{
    const kblock *bl = &AL.bl[b];
    if (bl->kopf == &RESTE_ZEILE) { *a = AMPEL_GRUEN; return T("nichts geht verloren", "nothing is lost"); }
    *a = kern_ampel(bl->kopf); return schild_text(bl->kopf, k);
}
/* das Schildchen, das mehr als die Hälfte der Blöcke hat (NULL = keins); *alle: alle Blöcke haben es */
static const char *al_gleich(int k, int *a, bool *alle)
{
    if (AL.n < 2) return NULL;
    const char *best = NULL; int best_n = 0, best_a = 0;
    for (int b = 0; b < AL.n && best_n * 2 <= AL.n; b++) {
        int xa, n = 0; const char *s = al_schild(b, k, &xa);
        for (int c = 0; c < AL.n; c++) { int ya; const char *u = al_schild(c, k, &ya); if (ya == xa && !strcmp(u, s)) n++; }
        if (n > best_n) { best_n = n; best = s; best_a = xa; }
    }
    if (best_n < 2 || best_n * 2 <= AL.n) return NULL;
    *a = best_a; *alle = best_n == AL.n; return best;
}
/* der Zusatz unter dem Namen, wenn er bei allen einzelnen Zeilen gleich ist ("Spielstände") - dann steht er nicht in jeder Zeile */
static const char *al_was_gleich(int k)
{
    const char *w0 = NULL; int n = 0;
    for (int b = 0; b < AL.n; b++) {
        if (AL.bl[b].teile.n != 1 || AL.bl[b].kopf == &RESTE_ZEILE) continue;
        const char *nm, *w; zeile_teilen(AL.bl[b].kopf, k, &nm, &w);
        if (!w[0] || (w0 && strcmp(w, w0))) return NULL;
        w0 = w; n++;
    }
    return n >= 2 ? w0 : NULL;
}
static void al_kopf_zeile(int ox, int ow, int ry, int b, int k, bool fok, const char *gleich, const char *was_gleich)
{
    const kblock *bl = &AL.bl[b]; const char *g;
    int hy = ry + 2, cy = hy + AL_KOPF / 2, gx = ox + ow - 16 - 22 - 14;
    uint32_t hg = fok ? 0x2C323B : 0x20242A;
    if (fok) karte_rund(ox + 2, hy, ow - 4, AL_KOPF, 14, 3, N_FOKUS, hg);
    bool reste = bl->kopf == &RESTE_ZEILE, einzeln = bl->teile.n == 1 && !reste;
    eintrag *f0 = bl->teile.e[0];
    bool gesp = einzeln && ablauf_eintrag_gesperrt(&A, f0, &g);
    kasten_haken(ox + 16, cy - 14, einzeln ? (f0->Wahl ? 2 : 0) : al_zustand(b), hg, gesp, k == 7 || (einzeln && f0->Jailbreak));
    kachel_klein(ox + 56, cy - 24, 48, bl->kopf, k, hg);
    int nx = ox + 56 + 48 + 16;
    const char *gr = einzeln ? h_trim(fa, ablauf_eintrag_groesse(&A, f0)) : block_groesse(b);
    int gw = textbreite(1, 26, gr);
    text(gx - gw, cy + 10, 1, 26, N_TEXT, gr);
    if (!einzeln) symbol(fokus_zone == 1 && b == al_b ? SYM_RUNTER : SYM_RECHTS, ox + ow - 16 - 22, cy - 11, 22, N_GRAU);
    int rechts = gx - (gw > 96 ? gw : 96) - 18;
    /* oben der Name (bei einer Zeile mit Ort), unten was es ist bzw. "3 Teile" und rechts das Schildchen */
    const char *name, *was;
    if (einzeln) { zeile_teilen(f0, k, &name, &was); if (was_gleich && !strcmp(was, was_gleich)) was = ""; }
    else { name = bl->name ? bl->name : ""; was = reste ? a_fmt(fa, T("%d Teile", "%d parts"), bl->teile.n)
                                                      : a_fmt(fa, bl->ohne_kachel ? T("%d Teile · ohne Kachel", "%d parts · no tile") : T("%d Teile", "%d parts"), bl->teile.n); }
    const char *sch; int sa;
    if (einzeln) { sch = schild_text(f0, k); sa = kern_ampel(f0); }
    else if (reste) { sch = T("nichts geht verloren", "nothing is lost"); sa = AMPEL_GRUEN; }
    else {   /* da_Chris86 04.11.2026 17:03: "Ist ein roter Teil angehakt, steht das in der zugeklappten Zeile" - sonst die Folge des Kopfs */
        int rw = 0; for (int i = 0; i < bl->teile.n; i++) { const eintrag *e = bl->teile.e[i]; if (e->Wahl && kern_ampel(e) == AMPEL_ROT) rw++; }
        if (rw) { sch = rw == 1 ? T("1 Teil endgültig weg", "1 part gone for good") : a_fmt(fa, T("%d Teile endgültig weg", "%d parts gone for good"), rw); sa = AMPEL_ROT; }
        else { sch = schild_text(bl->kopf, k); sa = kern_ampel(bl->kopf); }
    }
    const char *ok = einzeln ? ort_kurz(anzeige_ort(f0)) : "";
    bool zeigen = !(gleich && !strcmp(sch, gleich));   /* steht schon oben über der Liste */
    int ow2 = ok[0] ? ort_breite(ok) + 12 : 0, sw = zeigen ? schild_breite(sch) : 0, b1 = rechts - nx - ow2, b2 = rechts - (zeigen ? sw + 14 : 0) - nx;
    if (zeigen) schild(rechts - sw, hy + 55, sa, sch, hg);
    uint32_t nf = gesp ? N_GRAU2 : N_TEXT;
    if (!zeigen && !was[0] && textbreite(1, 24, name) <= b1) {   /* nichts für die zweite Zeile: der Name steht in der Mitte */
        int nb = name_zeichnen(nx, cy + 10, 1, 26, 24, nf, name, b1, fok, f0);
        if (ok[0]) ort_schild(nx + nb + 12, cy, ok);
    } else if (textbreite(1, 24, name) > b1 && !was[0]) {   /* langer Name ohne Zusatz: auf beide Zeilen verteilt */
        /* 1.0.9 (PS5-Test 09.11.2026 20:15, da_Chris86: bei Vice City fehlte "1 Screenshot, 1 Video" bzw. "2 Teile"): mit Zusatz bleibt der
         * Name auf der ersten Zeile (gekürzt, im Fokus als Laufschrift ganz) und der Zusatz steht darunter - er geht nicht mehr verloren */
        zeilen zl = umbrechen(1, 24, name, b1);
        const char *rest = "";
        if (zl.n > 1) { rest = name + strlen(zl.z[0]); while (*rest == ' ') rest++; }
        text(nx, hy + 31, 1, 24, nf, zl.z[0]);
        if (ok[0]) ort_schild(nx + textbreite(1, 24, zl.z[0]) + 12, hy + 23, ok);
        if (rest[0]) name_zeichnen(nx, hy + 63, 1, 24, 24, nf, rest, b2, fok, f0);
    } else {
        int nb = name_zeichnen(nx, hy + 31, 1, 26, 24, nf, name, b1, fok, f0);
        if (ok[0]) ort_schild(nx + nb + 12, hy + 23, ok);
        if (was[0]) name_zeichnen(nx, hy + 63, 0, 24, 22, N_GRAU2, was, b2, false, NULL);   /* Laufschrift nur für den Namen */
    }
}
static void al_teil_zeile(int ox, int ow, int ry, int b, int t, int k, bool fok, const char *gleich)
{
    const kblock *bl = &AL.bl[b]; const char *g;
    eintrag *f = bl->teile.e[t]; int cy = ry + AL_TEIL / 2, gx = ox + ow - 16 - 22 - 14;
    uint32_t hg = fok ? 0x2C323B : 0x20242A;
    if (fok) karte_rund(ox + 2, ry, ow - 4, AL_TEIL, 12, 3, N_FOKUS, hg);
    bool gesp = ablauf_eintrag_gesperrt(&A, f, &g);
    kasten_haken(ox + 16, cy - 14, f->Wahl ? 2 : 0, hg, gesp, k == 7 || f->Jailbreak);
    rechteck(ox + 56 + 23, ry, 2, t == bl->teile.n - 1 ? AL_TEIL / 2 + 2 : AL_TEIL, N_LINIE);   /* Strich: gehört zum Kopf darüber */
    int nx = ox + 56 + 48 + 16;
    const char *gr = h_trim(fa, ablauf_eintrag_groesse(&A, f));
    int gw = textbreite(0, 24, gr);
    text(gx - gw, cy + 9, 0, 24, N_TEXT2, gr);
    int rechts = gx - (gw > 96 ? gw : 96) - 18;
    /* NEU 1.0.9 (da_Chris86 09.11.2026 15:55): aufgeklappt hat jeder Teil sein Schild, auch wenn es oben schon steht ("Wo nichts anderes
     * steht") - rechts im Kasten stand es, in der Liste fehlte es (ShadowMount-Kachel "kommt wieder"). Die zugeklappte Zeile bleibt ruhig. */
    const char *sch = schild_text(f, k); int sa = kern_ampel(f), sw = sch && sch[0] ? schild_breite(sch) : -14;
    (void)gleich;
    if (sw > 0) schild(rechts - sw, cy, sa, sch, hg);
    const char *ok = ort_kurz(anzeige_ort(f)); int ow2 = ok[0] ? ort_breite(ok) + 12 : 0;
    const char *name = bl->kopf == &RESTE_ZEILE ? ohne_ort_vorne(kern_zeile(fa, f)) : ohne_jb_start(teil_name(b, f), k);
    int nb = name_zeichnen(nx, cy + 9, 0, 24, 24, gesp ? N_GRAU2 : N_TEXT2, name, rechts - sw - 14 - nx - ow2, fok, f);
    if (ok[0]) ort_schild(nx + nb + 12, cy, ok);
}
/* ---- der Bildschirm */
static void schirm_liste(void)
{
    rechteck(0, 0, BREITE, HOEHE, N_HG);
    kopf_neu(2, NULL);
    ablauf_texte_frei(&A);
    sicht_neu();
    int k = sicht[fokus_bereich];
    al_ordnen(k);
    bilder_neu = 0;
    int y0 = RAND_O + 64 + 28, fy = HOEHE - RAND_U - 64, lh = 100, ly = fy - 24 - lh, ph = ly - 24 - y0;
    int nw = 400, kw = 380, lx = RAND_X + nw + 24, lw = BREITE - 2 * RAND_X - nw - kw - 48, kx = lx + lw + 24;
    /* links: Bereiche, Jailbreak unten abgesetzt */
    karte_rund(RAND_X, y0, nw, ph, 22, 1, N_RAND, N_KARTE);
    {
        int ry = y0 + 12, j7 = -1, jy = y0 + ph - 12 - 72;
        for (int i = 0; i < sicht_n; i++) { if (sicht[i] == 7 && !kern_bereiche_106()) { j7 = i; continue; } al_bereich_zeile(RAND_X + 12, ry, nw - 24, i, sicht[i]); ry += 76; }
        if (!kern_bereiche_106()) {
            rechteck(RAND_X + 20, jy - 9, nw - 40, 1, N_RAND);
            if (j7 >= 0) al_bereich_zeile(RAND_X + 12, jy, nw - 24, j7, 7);
            else {   /* nichts gefunden: trotzdem zeigen, dass danach geschaut wurde */
                symbol(SYM_JAILBREAK, RAND_X + 72, jy + 22, 28, N_GRAU2);
                text(RAND_X + 116, jy + 33, 1, 26, N_GRAU2, kern_kat_name(7));
                text(RAND_X + 116, jy + 62, 0, 22, N_GRAU2, T("nichts gefunden", "nothing found"));
            }
        }
    }
    /* Mitte: Einträge */
    karte_rund(lx, y0, lw, ph, 22, 1, N_RAND, N_KARTE);
    int gew = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == k && f->Wahl) gew++; }
    text(lx + 24, y0 + 52, 1, 32, k == 7 ? N_ORANGE : N_TEXT, kern_kat_name(k));
    int kopf_r = lx + 24 + textbreite(1, 32, kern_kat_name(k));
    if (gew) { const char *g = a_fmt(fa, T("%d ausgewählt", "%d selected"), gew); text(kopf_r + 14, y0 + 52, 0, 24, N_GRAU2, g); kopf_r += 14 + textbreite(0, 24, g); }
    int gl_a = AMPEL_GRUEN; bool gl_alle = false; const char *gleich = al_gleich(k, &gl_a, &gl_alle), *was_gleich = al_was_gleich(k);
    int ox = lx + 24, oy = y0 + (gleich ? 124 : 80), ow = lw - 24 - 38, oh = y0 + ph - 16 - oy;
    if (gleich) {   /* da_Chris86 05.11.2026 20:42: nicht in jeder Zeile dieselbe Warnung - einmal oben, in den Zeilen nur, was anders ist */
        const char *vor = gl_alle ? T("Überall hier:", "Everywhere here:") : T("Wo nichts anderes steht:", "Unless shown otherwise:");
        text(ox, y0 + 104, 0, 24, N_GRAU, vor);
        schild(ox + textbreite(0, 24, vor) + 12, y0 + 96, gl_a, gleich, N_KARTE);
    }
    al_zeile *z = a_hole(fa, sizeof(al_zeile) * (size_t)(2 * K->Funde.n + 8));
    int nz = al_zeilen(z), gesamt = nz ? z[nz - 1].y + z[nz - 1].h : 0, fz = -1;
    for (int i = 0; i < nz; i++) if (z[i].b == al_b && z[i].t == (fokus_zone == 1 && al_mehr(al_b) ? al_t : -1)) fz = i;
    if (fokus_zone == 1 && fz >= 0) {   /* markierte Zeile ganz sichtbar halten */
        if (z[fz].y < al_py) al_py = z[fz].y;
        if (z[fz].y + z[fz].h > al_py + oh) al_py = z[fz].y + z[fz].h - oh;
    }
    if (al_py > gesamt - oh) al_py = gesamt - oh;
    if (al_py < 0) al_py = 0;
    for (int i = 0; i < nz; i++) if (z[i].y >= al_py) { al_py = z[i].y; break; }   /* der Bildlauf beginnt immer mit einer ganzen Zeile */
    int erster = -1, letzter = -1;
    if (nz == 0) text(ox, oy + 40, 0, 24, N_GRAU2, T("Nichts gefunden - hier ist alles sauber.", "Nothing found - all clean here."));
    for (int i = 0; i < nz; i++) {
        if (z[i].y < al_py || z[i].y + z[i].h > al_py + oh) continue;
        int ry = oy + z[i].y - al_py, b = z[i].b;
        if (erster < 0) erster = b;
        letzter = b;
        if (z[i].t == -1 || z[i].y == al_py) {   /* Grund des Blocks: von hier bis zu seiner letzten sichtbaren Zeile */
            int bis = i; while (bis + 1 < nz && z[bis + 1].b == b && z[bis + 1].y + z[bis + 1].h <= al_py + oh) bis++;
            rund(ox, ry, ow, z[bis].y + z[bis].h - z[i].y, 16, 0x20242A);
        }
        if (z[i].t == -1) al_kopf_zeile(ox, ow, ry, b, k, fokus_zone == 1 && i == fz, gleich, was_gleich);
        else al_teil_zeile(ox, ow, ry, b, z[i].t, k, fokus_zone == 1 && i == fz, gleich);
    }
    if (gesamt > oh) {   /* Bildlaufleiste */
        int sx = lx + lw - 20, th = oh * oh / gesamt; if (th < 40) th = 40;
        rund(sx, oy, 6, oh, 3, N_FELD);
        rund(sx, oy + (oh - th) * al_py / (gesamt - oh > 0 ? gesamt - oh : 1), 6, th, 3, 0x4A515C);
    }
    {   /* rechts in der Überschrift: Reihenfolge und welche Blöcke zu sehen sind */
        const char *rf = sortierung == SORT_AZ ? "A–Z" : T("Größte zuerst", "Largest first");
        /* NEU 1.0.8 (Befunde 4 und 10): "Zeile" sagt, dass hier die Zeilen der Liste gezählt werden (eine zugeklappte App ist eine Zeile) -
         * "19–21 von 24" stand neben "31 Einträge" und sah wie ein Fehler aus */
        const char *bz = AL.n == 0 || erster < 0 ? "" : erster == 0 && letzter == AL.n - 1 ? (AL.n == 1 ? T(" · 1 Zeile", " · 1 row") : a_fmt(fa, T(" · %d Zeilen", " · %d rows"), AL.n))
                                                       : a_fmt(fa, T(" · Zeile %d–%d von %d", " · rows %d–%d of %d"), erster + 1, letzter + 1, AL.n);
        const char *t = VERB(fa, rf, bz);
        int tw = textbreite(0, 24, t), sx = lx + lw - 24 - tw;
        if (sx - 34 > kopf_r + 20) { text(sx, y0 + 52, 0, 24, N_GRAU, t); symbol(SYM_SORTIEREN, sx - 34, y0 + 33, 24, N_GRAU); }
    }
    /* rechts: Kasten */
    al_kasten(kx, y0, kw, ph, k);
    /* unten: Auswahl-Leiste */
    int gw = 0; long long gb = 0; bool galle = true, ggek = false, rot = false, jbw = false;
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i]; if (!f->Wahl) continue;
        gw++; if (kern_ampel(f) == AMPEL_ROT) rot = true; if (f->Jailbreak) jbw = true;
        groesse *g = kern_eintrag_groesse_in(K, fa, f, true); if (!g) galle = false; else { gb += g->Bytes; if (g->Gekappt) ggek = true; }
    }
    bool auswahl = gw > 0, komplett = K->Funde.n > 0 && ablauf_verbunden(&A);
#ifdef NUR_LESEN
    auswahl = komplett = false;          /* Testversion Schritt 9: nur nachschauen */
#endif
    if (testmodus) auswahl = komplett = false;   /* NEU 1.0.8 (3.6): im Testmodus wird nur angeschaut */
    karte_rund(RAND_X, ly, BREITE - 2 * RAND_X, lh, 22, 1, rot ? farbmisch(N_ROT, N_KARTE, .5f) : N_RAND, N_KARTE);
    const char *t1 = gw == 0 ? T("Nichts ausgewählt.", "Nothing selected.")
                             : VERB(fa, T("Ausgewählt: ", "Selected: "), a_fmt(fa, "%d", gw), EINTRAEGE(gw), galle ? VERB(fa, " · ", groesse_von(fa, gb, ggek)) : "");
    const char *eg = kern_bereiche_106() ? "" : kern_endgueltig_satz(K, fa), *t2 = NULL; uint32_t f2 = N_GRAU2;
    if (rot && eg[0]) { t2 = VERB(fa, T("Achtung: ", "Caution: "), eg); f2 = N_ROT; }
    else if (rot) { t2 = jbw ? T("Achtung: Jailbreak-Einstieg gewählt - dein Jailbreak startet danach nicht mehr so wie bisher.", "Caution: jailbreak entry selected - your jailbreak will no longer start the way it did.")
                             : T("Achtung: rot markierte Einträge sind danach endgültig weg.", "Caution: entries marked red are gone for good afterwards."); f2 = N_ROT; }
    else if (A.hintergrund && A.hg_gesamt > 0 && A.hg_n < A.hg_gesamt) t2 = a_fmt(fa, T("Größen werden gezählt (%d von %d) …", "Counting sizes (%d of %d) …"), A.hg_n, A.hg_gesamt);
    else { const char *sh = kern_selbstschutz_hinweis(K, false); t2 = sh[0] ? sh : T("Gelöscht wird erst nach dem Bestätigen.", "Nothing is deleted before you confirm."); }
#ifndef NUR_LESEN
    if (testmodus) { t2 = testmodus_hinweis(); f2 = N_GELB; }
#endif
    const char *b1 = T("Vollständige Bereinigung", "Full cleanup"), *b2 = T("Nur Auswahl löschen", "Delete selection only");
    int w1 = leisten_knopf_breite(b1), w2 = leisten_knopf_breite(b2), bx2 = BREITE - RAND_X - 16 - w2, bx1 = bx2 - 14 - w1;
    int tbw = bx1 - 28 - (RAND_X + 28);
    {   /* zweite Zeile: notfalls kleiner und auf zwei Zeilen - ganz, nie über den Rand. NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86 08:46:
         * "ein paar Zeilen überlappen"): bei zwei Zeilen rückt alles nach oben - die zweite Zeile des deutschen Testmodus-Hinweises lag
         * sonst auf dem unteren Rand der Leiste */
        passend p = einpassen(0, 24, 24, 22, t2, tbw, 2);
        bool zwei = p.z.n > 1;
        p.lh = p.gr + (zwei ? 3 : 4);
        text_passend(RAND_X + 28, zwei ? ly + 33 : ly + 40, 1, 28, 24, N_TEXT, t1, tbw);
        passend_zeichnen(RAND_X + 28, zwei ? ly + 59 : ly + 76, 0, f2, &p);
    }
    leisten_knopf(bx1, ly + (lh - 64) / 2, w1, 64, komplett, true, KN_DREIECK, b1);
    leisten_knopf(bx2, ly + (lh - 64) / 2, w2, 64, auswahl, false, KN_QUADRAT, b2);
    fussknopf kn[6] = { { KN_HOCHRUNTER, T("wählen", "move"), false }, { KN_LINKSRECHTS, T("Bereich / Einträge", "Area / entries"), false },
                        { KN_X, T("Haken", "Tick"), false }, { KN_L1R1, T("seitenweise", "page"), false },
                        { KN_OPT, T("Optionen", "Options"), false }, { KN_O, T("halten: Beenden", "hold: Quit"), false } };
    hinweis_ueber = ly - 12;   /* kurze Meldungen über der Auswahl-Leiste, nicht auf ihr */
    fuss_neu(kn, 6);
    if (optionen_offen) al_optionen();
}

/* =================================================================== NEU 1.0.8 (Arbeitsplan 4.4): Bestätigen nach Entwurf 2
 * Die letzte Frage vor dem Löschen bei "Nur Auswahl löschen" (Titel "Endgültig löschen?"): oben was gelöscht wird, in Gruppen nach dem, was
 * danach passiert (rot endgültig weg, gelb neu laden oder einrichten, grün es geht nichts verloren), darunter die Hinweise des Kerns, was
 * immer bleibt, und zwei Knöpfe. Die Seite startet auf "Zurück zur Auswahl"; gelöscht wird nur, wenn ✕ auf "Löschen" 1,5 s gehalten wird
 * (Loslassen bricht ab). Die Frage selbst stellt der Kern wie bisher (frage_wahl) - Mitschnitt und Nachbau bleiben gleich. */
#define HALTEN_MS 1500
static Uint32 x_seit = 0;   /* ✕ auf "Löschen" gehalten seit (0 = nicht) */
static bool tv_geschluckt[SDL_CONTROLLER_BUTTON_MAX];           /* NEU 1.0.9: Taste am Controller verbraucht (tv_vorrang) - ihr Loslassen auch */
static bool x_vom_pad = false;   /* NEU 1.0.8 (Codex-Prüfung 76): das Halten kam vom Controller (dann wird sein Zustand mitgeprüft) */
static SDL_GameController *pad;  /* (steht weiter unten bei pad_oeffnen) */
static bool bestaetigen_aktiv(void) { return (G.art == D_WAHL || G.art == D_TEXT) && G.bestaetigen; }   /* Codex-Prüfung 76: ohne G.titel (der Arbeitsfaden kann ihn freigeben) */
static bool tippen_aktiv(void) { return G.art == D_TEXT && bestaetigen_aktiv(); }   /* vollständige Bereinigung: LÖSCHEN eintippen */
static bool eingabe_passt(void)   /* genau wie der Kern prüft (ablauf_komplett) */
{ return SP_EN ? !strcmp(G.eingabe, "DELETE") : (!strcmp(G.eingabe, "LÖSCHEN") || !strcmp(G.eingabe, "LOESCHEN")); }
static bool halten_start(void)   /* ✕ gedrückt: auf "Löschen" beginnt das Halten (true), sonst zählt es als normaler Tastendruck */
{
    if (!bestaetigen_aktiv() || G.art != D_WAHL || dlg_fokus != 1 || G.dlg_nr != dlg_gesehen || SDL_GetTicks() - dlg_seit < 400) return false;
    if (protokoll_offen || optionen_offen) return false;   /* Codex-Prüfung 76: nie unter der Protokoll-Ansicht bestätigen */
    x_seit = SDL_GetTicks(); if (!x_seit) x_seit = 1;
    x_vom_pad = false;   /* wer hält, setzt der Aufrufer danach */
    neu_zeichnen();
    return true;
}
static void halten_ende(void)    /* ✕ losgelassen, bevor der Kreis voll war */
{
    if (!x_seit) return;
    x_seit = 0;
    kurz(T("Losgelassen - es wurde nichts gelöscht.", "Released - nothing was deleted."));
    neu_zeichnen();
}
static void halten_pruefen(void) /* aus der Hauptschleife: Kreis voll -> Ja */
{
    if (!x_seit) return;
    if (!bestaetigen_aktiv() || G.art != D_WAHL || dlg_fokus != 1 || protokoll_offen || optionen_offen) { x_seit = 0; return; }
    if (x_vom_pad) {   /* Codex-Prüfung 76: kommen keine Daten mehr vom Controller (Lesefehler, getrennt), zählt das als Loslassen */
        const char *se = SDL_GetError();
        if (!pad || !SDL_GameControllerGetAttached(pad) || !SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) || (se && strstr(se, "scePadReadState"))) { halten_ende(); return; }
    }
    if (SDL_GetTicks() - x_seit >= HALTEN_MS) { x_seit = 0; antworten(true, false); neu_zeichnen(); return; }
    if (SDL_GetTicks() - bild_zeit >= 33) neu_zeichnen();   /* 1.0.8 (4.7): der Kreis füllt sich mit etwa 30 Bildern pro Sekunde */
}
static void ring_teil(float cx, float cy, float r, float d, float anteil, uint32_t farbe)   /* Teil eines Rings, oben beginnend im Uhrzeigersinn */
{
    if (anteil <= 0.f) return;
    if (anteil > 1.f) anteil = 1.f;
    float h = d / 2.f, grenze = anteil * 6.2831853f;
    for (int y = (int)floorf(cy - r - h - 1); y <= (int)ceilf(cy + r + h + 1); y++)
        for (int x = (int)floorf(cx - r - h - 1); x <= (int)ceilf(cx + r + h + 1); x++) {
            float dx = (float)x + .5f - cx, dy = (float)y + .5f - cy, w = atan2f(dx, -dy);
            if (w < 0.f) w += 6.2831853f;
            if (w > grenze) continue;
            misch(x, y, farbe, h - fabsf(sqrtf(dx * dx + dy * dy) - r) + .5f);
        }
}
/* Hinweise aus der Zusammenfassung des Kerns (G.text), ohne das, was die Seite schon zeigt (Anzahl je Bereich, Größe, sichere Kopplung) */
typedef struct { const char *t; uint32_t f; const float *sym; } b_hinweis;
static int bestaetigen_hinweise(b_hinweis *h, int max)
{
    const char *kt = kern_kopplungs_text(K);
    kopplungsart ks = kern_kopplungs_stufe(K);
    pthread_mutex_lock(&G.m); const char *gt = a_kopie(fa, G.text ? G.text : ""); pthread_mutex_unlock(&G.m);   /* Codex-Prüfung 76 */
    sliste z = r_split(fa, gt, "\n");
    int n = 0;
    for (int i = 0; i < z.n && n < max; i++) {
        const char *s = z.e[i];
        if (!s[0]) continue;
        if (txt_beginnt(s, T("Wird ENDGÜLTIG gelöscht: ", "Will be deleted PERMANENTLY: ")) || txt_beginnt(s, "   - ") || txt_beginnt(s, T("Das sind ", "That is "))) continue;
        if (!strcmp(s, kt) && (ks == KOPPLUNG_BESTAETIGT || ks == KOPPLUNG_KEINE)) continue;   /* steht unten bei "Bleibt immer" */
        if (txt_beginnt(s, T("Zum Bestätigen LÖSCHEN eintippen", "Type DELETE to confirm"))) continue;   /* steht unten am Feld */
        while (*s == ' ') s++;
        uint32_t f = N_GRAU; const float *sy = SYM_INFO;
        if (txt_beginnt(s, "!!! ")) { s = r_r(fa, s + 4, " !!!$", ""); f = N_ROT; sy = SYM_WARNUNG; }
        else if (txt_beginnt(s, T("Danach endgültig weg", "Gone for good afterwards")) || txt_beginnt(s, T("Dein Jailbreak", "Your jailbreak"))) { f = N_ROT; sy = SYM_WARNUNG; }
        else if (txt_beginnt(s, T("HINWEIS", "NOTE")) || txt_beginnt(s, "SIMULATION") || !strcmp(s, kt)) f = N_GELB;
        h[n++] = (b_hinweis){ s, f, sy };
    }
    return n;
}
/* Hinweise eng untereinander; passt es nicht, laufen sie langsam durch (wie der Kasten der Auswahl) */
static Uint32 hw_seit; static int hw_nr = -1;
static int hinweise_zeichnen(int x, int y, int w, const b_hinweis *h, int n, bool zeichnen)
{
    int yy = y;
    for (int i = 0; i < n; i++) {
        zeilen z = umbrechen(0, 24, h[i].t, w - 36);
        if (zeichnen) { symbol(h[i].sym, x, yy + 6, 22, h[i].f); for (int j = 0; j < z.n; j++) text(x + 36, yy + 24 + j * 30, 0, 24, h[i].f, z.z[j]); }
        yy += z.n * 30 + 6;
    }
    return yy - y - (n ? 6 : 0);
}
static void hinweise_box(int x, int y, int w, int hoehe, const b_hinweis *h, int n)
{
    int inhalt = hinweise_zeichnen(x, 0, w, h, n, false), off = 0;
    if (inhalt > hoehe) {
        if (hw_nr != G.dlg_nr) { hw_nr = G.dlg_nr; hw_seit = SDL_GetTicks(); }
        off = lauf_versatz(hw_seit, inhalt - hoehe + 8, 28, 3000);   /* 1.0.8 (4.7) */
        clip_y0 = y - 2; clip_y1 = y + hoehe + 6;
    }
    hinweise_zeichnen(x, y - off, w, h, n, true);
    clip_y0 = 0; clip_y1 = 1 << 30;
    if (inhalt > hoehe) {
        for (int i = 0; i < 26; i++) { float al = (float)(i + 1) / 26.f; for (int px = x - 4; px < x + w; px++) misch(px, y + hoehe + 6 - 26 + i, N_HG, al); }
        if (off > 0) for (int i = 0; i < 14; i++) { float al = (float)(14 - i) / 14.f; for (int px = x - 4; px < x + w; px++) misch(px, y - 2 + i, N_HG, al); }
    }
}
/* eine Gruppe: Karte mit Kopf und den größten Einträgen */
typedef struct { eintrag *f; long long b; bool da, gek; } b_zeile;
static int b_vergleich(const void *x, const void *y) { const b_zeile *a = x, *b = y; return a->b < b->b ? 1 : a->b > b->b ? -1 : 0; }
static void b_karte(int x, int y, int w, int h, int g, const b_zeile *z, int n, long long bytes, bool alle, bool gek)
{
    int amp = g == 0 ? AMPEL_ROT : g == 1 ? AMPEL_GELB : AMPEL_GRUEN;
    uint32_t f = ampel_f(amp), hg = farbmisch(f, N_HG, g == 0 ? .07f : .04f);
    karte_rund(x, y, w, h, 22, 2, farbmisch(f, N_HG, g == 0 ? .5f : .22f), hg);
    const char *titel = g == 0 ? T("Endgültig weg", "Gone for good") : g == 1 ? T("Neu laden oder einrichten", "Download or set up again") : T("Es geht nichts verloren", "Nothing is lost");
    rund(x + 24, y + 24, 56, 56, 14, farbmisch(f, hg, .16f)); symbol(ampel_sym(amp), x + 24 + 14, y + 24 + 14, 28, f);
    text_passend(x + 96, y + 50, 1, 30, 24, f, titel, w - 96 - 24);
    text_passend(x + 96, y + 80, 0, 24, 22, N_GRAU, VERB(fa, a_fmt(fa, "%d", n), EINTRAEGE(n), alle ? VERB(fa, " · ", groesse_von(fa, bytes, gek)) : ""), w - 96 - 24);
    /* NEU 1.0.8 (Arbeitsplan 4.6): lange Namen brechen um (bis 3 Zeilen, Schrift 24) statt kleiner zu werden und mit "…" zu enden
     * ("Aufnahmen von Red Dead Redemp…"). Die Zeile wird dafür höher, dann passen weniger Einträge in die Karte - der Rest steht wie
     * bisher als "und N weitere" darunter. Nur wenn auch 3 Zeilen nicht reichen, endet die letzte mit "…" (Ausnahme, da_Chris86 02.11.).
     * "und N weitere" steht unten in der Karte - so stehen die drei Karten gleich, auch wenn die Zeilen verschieden hoch sind. */
    int ry = y + 106, unten = y + h - 16, unten_mehr = y + h - 50, nx = x + 26 + 48 + 14, zy = ry, zeig = 0;
    uint32_t zhg = farbmisch(N_KARTE, hg, .6f);
    for (int i = 0; i < n; i++) {
        const char *gr = z[i].da ? groesse_von(fa, z[i].b, z[i].gek) : "";
        int gw = textbreite(1, 24, gr);
        const char *ok = ort_kurz(anzeige_ort(z[i].f)); int ow2 = ok[0] ? ort_breite(ok) + 12 : 0;
        int nb = x + w - 30 - gw - 18 - nx - ow2; if (nb < 60) nb = 60;
        zeilen nz = umbrechen(0, 24, ohne_ort_vorne(kern_zeile(fa, z[i].f)), nb);
        if (nz.n > 3) { nz.z[2] = kuerzen(0, 24, zeilen_rest(&nz, 2), nb); nz.n = 3; }
        if (nz.n < 1) { static const char *leer[1] = { "" }; nz.z = leer; nz.n = 1; }
        int zh = 60 + (nz.n - 1) * 28;
        if (zy + zh > (i + 1 < n ? unten_mehr : unten)) break;   /* kommen noch Einträge, bleibt unten Platz für "und N weitere" */
        int cy = zy + zh / 2, ty0 = cy + 9 - (nz.n - 1) * 14;
        rund(x + 16, zy, w - 32, zh, 12, zhg);
        kachel_klein(x + 26, cy - 24, 48, z[i].f, z[i].f->Kat, zhg);
        text(x + w - 30 - gw, cy + 9, 1, 24, N_TEXT, gr);
        for (int j = 0; j < nz.n; j++) text(nx, ty0 + j * 28, 0, 24, N_TEXT, nz.z[j]);
        if (ok[0]) ort_schild(nx + textbreite(0, 24, nz.z[nz.n - 1]) + 12, ty0 + (nz.n - 1) * 28 - 9, ok);
        zy += zh + 4; zeig++;
    }
    if (zeig < n) text(x + 30, y + h - 26, 0, 24, N_GRAU, a_fmt(fa, T("und %d weitere", "and %d more"), n - zeig));
}
/* NEU 1.0.8 (4.4, da_Chris86 05.11.2026 21:33: "ne top passt gut mitgedacht"): vollständige Bereinigung - dieselbe Übersicht, unten aber
 * wie bisher LÖSCHEN eintippen (dort geht am meisten weg). Links "Zurück zur Auswahl" (Fokus 1), in der Mitte das Feld (Fokus 0, hier
 * startet die Seite), rechts "Löschen" (Fokus 2). Gelöscht wird nur mit genau dem Wort, das der Kern verlangt (ablauf_komplett). Eigene
 * Entscheidung: anderer Text bricht nicht mehr ab wie in 1.0.7, die Seite sagt stattdessen, was fehlt ("Zurück" bricht ab). */
static void bestaetigen_tippen(int kyb, int lh, int zh)
{
    const char *wort = T("LÖSCHEN", "DELETE");
    bool passt = eingabe_passt(), getippt = G.eingabe[0] != 0;
    if (passt) hinweis_bis = 0;   /* "Erst LÖSCHEN eintippen ..." gilt nicht mehr */
    const char *zt = T("Zurück zur Auswahl", "Back to selection"), *lt = T("Löschen", "Delete");
    const char *lu_ja = T("jetzt mit ✕", "now with ✕"), *lu_nein = VERB(fa, T("erst ", "type "), wort, T(" eintippen", " first")), *lu = passt ? lu_ja : lu_nein;
    int zw = 36 + 14 + textbreite(1, 26, zt) + 60, fw = 520;
    int lb = textbreite(1, 30, lt);   /* breit genug für beide Zeilen - die Knöpfe springen beim Eintippen nicht */
    if (textbreite(0, 24, lu_ja) > lb) lb = textbreite(0, 24, lu_ja);
    if (textbreite(0, 24, lu_nein) > lb) lb = textbreite(0, 24, lu_nein);
    int lw = 30 + 60 + 18 + lb + 34, x0 = (BREITE - (zw + 28 + fw + 28 + lw)) / 2, zy = kyb + (lh - zh) / 2, fx = x0 + zw + 28, lx = fx + fw + 28;
    bool fz = dlg_fokus == 1, ff = dlg_fokus == 0, fl = dlg_fokus == 2;
    /* Zurück */
    if (fz) rund(x0 - 9, zy - 9, zw + 18, zh + 18, 27, farbmisch(N_FOKUS, N_HG, .18f));
    karte_rund(x0, zy, zw, zh, 18, fz ? 3 : 2, fz ? N_FOKUS : N_LINIE, N_KARTE);
    { int gx = x0 + 30, cy = zy + zh / 2; ring(gx + 18, cy, 17, 2, N_TEXT); symbol(SYM_KN_KREIS, gx + 10, cy - 8, 16, N_TEXT); text(gx + 50, cy + 9, 1, 26, N_TEXT, zt); }
    /* Feld */
    if (ff) rund(fx - 9, kyb - 9, fw + 18, lh + 18, 27, farbmisch(N_FOKUS, N_HG, .18f));
    karte_rund(fx, kyb, fw, lh, 18, ff ? 3 : 2, ff ? N_FOKUS : passt ? N_GRUEN : getippt ? N_GELB : N_LINIE, N_KARTE);
    {
        const char *kt = tastatur_offen ? T("Tastatur ist offen ...", "Keyboard is open ...") : T("✕ = Tastatur öffnen", "✕ = open keyboard");
        text(fx + 28, kyb + 36, 0, 24, N_GRAU2, kt);
        if (getippt) text_passend(fx + 28, kyb + 78, 1, 30, 24, passt ? N_GRUEN : N_GELB, G.eingabe, fw - 56);
        else text(fx + 28, kyb + 78, 1, 30, N_GRAU, VERB(fa, T("", "Type "), wort, T(" eintippen", "")));
        if (passt) symbol(SYM_HAKEN, fx + fw - 28 - 30, kyb + lh / 2 - 15, 30, N_GRUEN);
    }
    /* Löschen */
    if (fl) { rund(lx - 9, kyb - 9, lw + 18, lh + 18, 27, farbmisch(N_FOKUS, N_HG, .18f)); rund(lx - 3, kyb - 3, lw + 6, lh + 6, 21, N_FOKUS); }
    uint32_t lf = passt ? (fl ? N_ROT_KNOPF : farbmisch(N_ROT_KNOPF, N_HG, .55f)) : farbmisch(N_ROT_KNOPF, N_HG, .2f);   /* ohne Wort: gedämpft */
    rund(lx, kyb, lw, lh, 18, lf);
    {
        int gcx = lx + 30 + 30, gcy = kyb + lh / 2;
        uint32_t wf = passt ? (fl ? 0xFFFFFF : farbmisch(0xFFFFFF, lf, .8f)) : farbmisch(0xFFFFFF, lf, .45f);
        ring(gcx, gcy, 27, 3, farbmisch(wf, lf, .5f));
        symbol(SYM_KN_KREUZ, gcx - 11, gcy - 11, 22, wf);
        text(gcx + 30 + 18, gcy - 4, 1, 30, wf, lt);
        text(gcx + 30 + 18, gcy + 26, 0, 24, farbmisch(wf, lf, .85f), lu);
    }
    const char *unten = getippt && !passt ? VERB(fa, T("Das ist nicht ", "That is not "), wort, T(" - bitte genau so eintippen, in Großbuchstaben.", " - please type it exactly like this, in capital letters."))
                      : passt ? T("Bis jetzt wurde nichts gelöscht. ✕ auf „Löschen“ löscht alles oben Gezeigte endgültig.", "Nothing has been deleted yet. ✕ on 'Delete' deletes everything shown above for good.")
                      : VERB(fa, T("Bis jetzt wurde nichts gelöscht. Zum Löschen ", "Nothing has been deleted yet. To delete, type "), wort, T(" eintippen (Großbuchstaben) und dann „Löschen“ drücken.", " (capital letters) and then press 'Delete'."));
    text_mitte(BREITE / 2, kyb + lh + 34, 0, 24, getippt && !passt ? N_GELB : N_GRAU, unten);
    fussknopf kn[3] = { { KN_X, ff ? T("Tastatur öffnen", "Open keyboard") : fl ? T("Löschen", "Delete") : T("Zurück zur Auswahl", "Back to selection"), false },
                        { KN_O, T("Zurück zur Auswahl", "Back to selection"), false }, { KN_LINKSRECHTS, T("wechseln", "switch"), false } };
    hinweis_ueber = kyb - 16;
    fuss_neu(kn, 3);
}
static void schirm_bestaetigen(void)
{
    rechteck(0, 0, BREITE, HOEHE, N_HG);
    kopf_neu(3, NULL);
    bilder_neu = 0;
    /* gewählte Einträge nach Gruppen, größte zuerst (der Kern ruht, solange er auf die Antwort wartet) */
    int nmax = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Wahl) nmax++;
    b_zeile *gz[3]; int gn[3] = { 0, 0, 0 }; long long gb[3] = { 0, 0, 0 }; bool galle[3] = { true, true, true }, ggek[3] = { false, false, false };
    for (int g = 0; g < 3; g++) gz[g] = a_hole(fa, sizeof(b_zeile) * (size_t)(nmax + 1));
    bool jb_gew = false, jb_da = false;
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i];
        if (f->Jailbreak) jb_da = true;
        if (!f->Wahl || f->Gesperrt) continue;
        if (f->Jailbreak) jb_gew = true;
        int a = kern_ampel(f), g = a == AMPEL_ROT ? 0 : a == AMPEL_GELB ? 1 : 2;
        groesse *gr = kern_eintrag_groesse_in(K, fa, f, true);
        gz[g][gn[g]++] = (b_zeile){ f, gr ? gr->Bytes : -1, gr != NULL, gr ? gr->Gekappt : false };
        if (gr) { gb[g] += gr->Bytes; if (gr->Gekappt) ggek[g] = true; } else galle[g] = false;
    }
    for (int g = 0; g < 3; g++) qsort(gz[g], (size_t)gn[g], sizeof(b_zeile), b_vergleich);
    int nges = gn[0] + gn[1] + gn[2];
    /* oben: Überschrift */
    text(RAND_X, 196, 1, 44, N_TEXT, T("Das wird gelöscht", "This will be deleted"));
    const char *sub = VERB(fa, a_fmt(fa, "%d", nges), EINTRAEGE(nges));
    if (A.GROSS.Bytes > 0) sub = VERB(fa, sub, " · ", groesse_von(fa, A.GROSS.Bytes, A.GROSS.Gekappt), T(" werden frei", " will be freed"));
    if (tippen_aktiv()) sub = VERB(fa, T("Vollständige Bereinigung · ", "Full cleanup · "), sub);
    text_passend(RAND_X, 238, 0, 28, 24, N_GRAU, sub, BREITE - 2 * RAND_X);
    /* unten nach oben: Knöpfe, "Bleibt immer", Hinweise, Karten */
    int lh = 96, zh = 80, kyb = 806, by = 720, bh = 64;
    b_hinweis hw[16]; int nh = bestaetigen_hinweise(hw, 16);
    /* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 1): die Hinweise bekommen so viel Platz, wie sie brauchen - bis hier waren es fest 90 Pixel,
     * die dritte Zeile stand halb verdeckt unter dem Ausblenden. Die Karten werden dafür kürzer (mindestens 250 Pixel, dann "und N weitere");
     * reicht auch das nicht, laufen die Hinweise wie bisher langsam durch. */
    int hinh = nh ? hinweise_zeichnen(0, 0, BREITE - 2 * RAND_X - 12, hw, nh, false) : 0;
    int ky0 = 262, ky1 = nh ? by - 14 - 16 - hinh : by - 20;
    if (ky1 > by - 20) ky1 = by - 20;
    if (ky1 < ky0 + 250) ky1 = ky0 + 250;
    int ng = 0, gi[3]; for (int g = 0; g < 3; g++) if (gn[g]) gi[ng++] = g;
    if (ng == 0) text(RAND_X, ky0 + 40, 0, 24, N_GRAU2, T("Nichts ausgewählt.", "Nothing selected."));
    else {
        int cw = (BREITE - 2 * RAND_X - (ng - 1) * 24) / ng;
        for (int i = 0; i < ng; i++) { int g = gi[i]; b_karte(RAND_X + i * (cw + 24), ky0, cw, ky1 - ky0, g, gz[g], gn[g], gb[g], galle[g], ggek[g]); }
    }
    if (nh) hinweise_box(RAND_X + 6, ky1 + 16, BREITE - 2 * RAND_X - 12, by - 14 - (ky1 + 16), hw, nh);
    /* was immer bleibt */
    karte_rund(RAND_X, by, BREITE - 2 * RAND_X, bh, 18, 1, N_RAND, N_KARTE);
    {
        int bx = RAND_X + 28, cyb = by + bh / 2;
        const char *t0 = T("Bleibt immer", "Always stays");
        text(bx, cyb + 9, 1, 24, N_TEXT2, t0); bx += textbreite(1, 24, t0) + 30;
        const char *bl[4] = { T("Systemsoftware", "System software"), T("Laufwerks-Kopplung", "Disc drive pairing"), T("Konto", "Account"), T("echte Lizenzen", "Real licenses") };
        for (int i = 0; i < 4; i++) { symbol(SYM_SCHLOSS, bx, cyb - 11, 20, N_GRAU2); bx += 28; text(bx, cyb + 9, 0, 24, N_GRAU, bl[i]); bx += textbreite(0, 24, bl[i]) + 30; }
        const char *jt = jb_gew ? T("Jailbreak-Einstieg ausgewählt", "Jailbreak entry selected") : jb_da ? T("Jailbreak bleibt (nicht ausgewählt)", "Jailbreak stays (not selected)") : "";
        if (jt[0]) {
            uint32_t jf = jb_gew ? N_ROT : N_ORANGE; int jw = textbreite(0, 24, jt), jx = BREITE - RAND_X - 28 - jw;
            if (jx - 34 > bx) { text(jx, cyb + 9, 0, 24, jf, jt); symbol(jb_gew ? SYM_WARNUNG : SYM_JAILBREAK, jx - 34, cyb - 12, 22, jf); }
        }
    }
    if (tippen_aktiv()) { bestaetigen_tippen(kyb, lh, zh); return; }
    /* Knöpfe: links "Zurück zur Auswahl" (hier startet die Seite), rechts "Löschen" (✕ halten) */
    const char *zt = T("Zurück zur Auswahl", "Back to selection"), *lt = T("Löschen", "Delete"), *lu = T("1,5 Sekunden gedrückt halten", "hold for 1.5 seconds");
    int zw = 36 + 14 + textbreite(1, 26, zt) + 60, lb = textbreite(1, 30, lt) > textbreite(0, 24, lu) ? textbreite(1, 30, lt) : textbreite(0, 24, lu);
    int lw = 30 + 60 + 18 + lb + 34, x0 = (BREITE - (zw + 28 + lw)) / 2, zy = kyb + (lh - zh) / 2, lx = x0 + zw + 28;
    bool fz = dlg_fokus == 0, fl = dlg_fokus == 1;
    if (fz) rund(x0 - 9, zy - 9, zw + 18, zh + 18, 27, farbmisch(N_FOKUS, N_HG, .18f));
    karte_rund(x0, zy, zw, zh, 18, fz ? 3 : 2, fz ? N_FOKUS : N_LINIE, N_KARTE);
    { int gx = x0 + 30, cy = zy + zh / 2; ring(gx + 18, cy, 17, 2, N_TEXT); symbol(SYM_KN_KREIS, gx + 10, cy - 8, 16, N_TEXT); text(gx + 50, cy + 9, 1, 26, N_TEXT, zt); }
    if (fl) { rund(lx - 9, kyb - 9, lw + 18, lh + 18, 27, farbmisch(N_FOKUS, N_HG, .18f)); rund(lx - 3, kyb - 3, lw + 6, lh + 6, 21, N_FOKUS); }
    rund(lx, kyb, lw, lh, 18, fl ? N_ROT_KNOPF : farbmisch(N_ROT_KNOPF, N_HG, .55f));
    {
        int gcx = lx + 30 + 30, gcy = kyb + lh / 2;
        uint32_t wf = fl ? 0xFFFFFF : farbmisch(0xFFFFFF, N_ROT_KNOPF, .7f);
        ring(gcx, gcy, 27, 3, farbmisch(0xFFFFFF, N_ROT_KNOPF, .35f));
        if (x_seit) ring_teil(gcx, gcy, 27, 5, (float)(SDL_GetTicks() - x_seit) / (float)HALTEN_MS, 0xFFFFFF);
        symbol(SYM_KN_KREUZ, gcx - 11, gcy - 11, 22, wf);
        text(gcx + 30 + 18, gcy - 4, 1, 30, wf, lt);
        text(gcx + 30 + 18, gcy + 26, 0, 24, farbmisch(wf, N_ROT_KNOPF, .85f), lu);
    }
    text_mitte(BREITE / 2, kyb + lh + 34, 0, 24, N_GRAU, fl ? T("Loslassen bricht ab. Bis jetzt wurde nichts gelöscht.", "Releasing cancels. Nothing has been deleted yet.")
                                                          : T("Bis jetzt wurde nichts gelöscht. Zum Löschen: → und dann ✕ gedrückt halten.", "Nothing has been deleted yet. To delete: → and then hold ✕."));
    fussknopf kn[3] = { { KN_X, T("halten: Löschen", "hold: Delete"), false }, { KN_O, T("Zurück zur Auswahl", "Back to selection"), false },
                        { KN_LINKSRECHTS, T("Knopf wechseln", "Switch button"), false } };
    hinweis_ueber = kyb - 16;   /* kurze Meldungen ("Losgelassen ...") über den Knöpfen */
    fuss_neu(kn, 3);
}

/* NEU 1.1: Testversion "nur lesen" - Statuszeile und Protokoll sagen das statt "SIMULATION" */
static void sim_hinweis_setzen(void)
{
#ifdef NUR_LESEN
    A.sim_hinweis = T("TEST: NUR LESEN - es wird nichts verändert.", "TEST: READ ONLY - nothing is changed.");
#endif
}

/* =================================================================== NEU 1.0.8 (Arbeitsplan 4.5): "Nachschauen läuft" nach Entwurf 2
 * wie "Löschen läuft" aufgebaut - auch für die anderen Wartezeiten ohne Löschen (Größe zählen vor der Abschlussfrage, Abbrechen).
 * Oben was läuft und seit wann, darunter der Stand des Kerns mit Balken, die letzten Zeilen des Protokolls und der Hinweis, dass
 * nichts verändert wird. ○ bricht ab, solange der Kern es erlaubt (wie bisher). */
static void schirm_arbeit(void)
{
    rechteck(0, 0, BREITE, HOEHE, N_HG);
    bool suchen = auftrag_jetzt == AUF_SUCHEN || nach_loeschen, ab = K->abbruch;   /* nach "Fertig": der Kern schaut neu nach */
    kopf_neu(suchen ? 1 : 3, NULL);
    bilder_neu = 0;
    pthread_mutex_lock(&G.m);
    char st[1024]; snprintf(st, sizeof st, "%s", G.stand); double p = G.prozent; bool unb = G.unbestimmt;
    int von = G.log_n - 40 < 0 ? 0 : G.log_n - 40;
    sliste lg = {0}; for (int i = von; i < G.log_n; i++) sl_add(fa, &lg, a_kopie(fa, G.log[i]));
    pthread_mutex_unlock(&G.m);
    int bw = BREITE - 2 * RAND_X, y = 280;   /* wie "Löschen läuft" */
    const char *titel = ab ? T("Wird abgebrochen …", "Cancelling …") : nach_loeschen ? T("Schaue nach, was übrig ist …", "Checking what is left …") : suchen ? T("Schaue nach …", "Scanning …")
                      : txt_beginnt(st, T("Zähle", "Only counting")) ? T("Zähle die Größe …", "Counting the size …") : T("Einen Moment …", "One moment …");
    text(RAND_X, y, 1, 48, N_TEXT, titel);
    int sek = arbeit_seit ? (int)((SDL_GetTicks() - arbeit_seit) / 1000) : 0;
    text_rechts(BREITE - RAND_X, y - 44, 0, 24, N_GRAU2, T("Läuft seit", "Running for"));
    text_rechts(BREITE - RAND_X, y, 1, 40, N_TEXT, a_fmt(fa, "%d:%02d", sek / 60, sek % 60));
    int ry = y + 64, pz = (int)(p + .5); if (pz < 0) pz = 0; if (pz > 100) pz = 100;
    bool mit_pz = !unb && pz > 0 && st[0];
    text_passend(RAND_X, ry, 1, 28, 22, N_TEXT2, st[0] ? st : T("Wird vorbereitet …", "Preparing …"), bw - (mit_pz ? 120 : 0));
    if (mit_pz) text_rechts(BREITE - RAND_X, ry, 1, 28, N_GRAU, a_fmt(fa, "%d %%", pz));
    int by = ry + 14;
    rund(RAND_X, by, bw, 16, 8, 0x1B2638);
    if (!mit_pz) { int w = 320, pos = (int)(SDL_GetTicks() / 3 % (Uint32)(bw + w)) - w, a = pos < 0 ? 0 : pos, b = pos + w > bw ? bw : pos + w; if (b > a) rund(RAND_X + a, by, b - a, 16, 8, N_BLAU); }
    else { int w = bw * pz / 100; rund(RAND_X, by, w < 16 ? 16 : w, 16, 8, N_BLAU); }
    /* die letzten Zeilen des Protokolls (umgebrochen, die neueste unten) */
    int ly = by + 16 + 32, kh = 440, pad = 32, tb = bw - 2 * pad - 28;
    karte_rund(RAND_X, ly, bw, kh, 22, 1, N_RAND, N_KARTE);
    symbol(SYM_DOKUMENT, RAND_X + pad, ly + 26, 24, N_GRAU2);
    text(RAND_X + pad + 36, ly + 48, 1, 24, N_GRAU, T("Protokoll - die letzten Zeilen", "Log - the latest lines"));
    int max = (kh - 92 - 16) / 36, n = 0;
    const char **zl = a_hole(fa, sizeof(char *) * (size_t)max); int *zx = a_hole(fa, sizeof(int) * (size_t)max);
    for (int i = lg.n - 1; i >= 0 && n < max; i--) {   /* von hinten: so viele Zeilen, wie in die Karte passen */
        const char *q = lg.e[i]; int ein = 0; while (q[ein] == ' ') ein++;
        if (!q[ein]) continue;
        zeilen z = umbrechen(0, 24, q + ein, tb - (ein ? 28 : 0));
        for (int j = z.n - 1; j >= 0 && n < max; j--) { memmove(zl + 1, zl, sizeof(char *) * (size_t)n); memmove(zx + 1, zx, sizeof(int) * (size_t)n); zl[0] = z.z[j]; zx[0] = ein ? 28 : 0; n++; }
    }
    for (int i = 0; i < n; i++) text(RAND_X + pad + zx[i], ly + 92 + 24 + i * 36, 0, 24, i == n - 1 ? N_TEXT : N_TEXT2, zl[i]);
    if (!n) text(RAND_X + pad, ly + 92 + 24, 0, 24, N_GRAU2, T("Noch keine Zeilen.", "No lines yet."));
    /* darunter: was garantiert ist */
    int iy = ly + kh + 24 + 26;
    symbol(SYM_SCHLOSS, RAND_X, iy - 20, 24, N_GRAU2);
    text(RAND_X + 36, iy, 0, 24, N_GRAU, nach_loeschen ? T("Das Löschen ist fertig - jetzt wird nur noch nachgeschaut.", "Deleting is done - now it only checks what is left.") : suchen ? T("Nur lesen - es wird nichts verändert.", "Read only - nothing is changed.")
                                               : T("Es wird noch nichts gelöscht - vorher kommt immer die Abschlussfrage.", "Nothing is deleted yet - the final question always comes first."));
    fussknopf kn[2]; int nk = 0;
    if (A.abbrechbar && !ab) kn[nk++] = (fussknopf){ KN_O, T("Abbrechen (es wird nichts gelöscht)", "Cancel (nothing is deleted)"), false };
    kn[nk++] = (fussknopf){ KN_OPT, T("Protokoll", "Log"), true };
    fuss_neu(kn, nk);
}

/* NEU 1.0.7 (da_Chris86 03.11.2026 19:09-19:12): freiwillige Rückmeldung per QR-Code - "wir müssen den Leuten klar machen, dass wir
 * keine inhaltsbezogenen persönlichen Daten übermitteln". Die PS5 sendet dabei NICHTS, sie zeigt nur einen Link als QR-Code. Im Link
 * stehen nur Version, Fassung, Modellnummer, Firmware und Zahlen - keine Namen von Spielen oder Apps, kein Konto, keine Pfade, keine IP.
 * Das Handy öffnet die Seite und zeigt dieselben Angaben noch einmal; gespeichert wird erst, wenn der Nutzer dort auf "Senden" tippt. */
#ifndef RUECKMELDUNG_ADRESSE
#define RUECKMELDUNG_ADRESSE "https://feedback.ps5cleanup.workers.dev/r"
#endif
#define RUECKMELDUNG_VERSION VERSION_NUMMER          /* 06.11.2026: 1.0.8 - die Rückmeldeseite vergleicht damit (NEUESTE_VERSION in rueckmeldung/wrangler.toml); 1.0.9: VERSION_NUMMER */
#ifdef KERN_LOESCHEN
#define RUECKMELDUNG_FASSUNG 'r'             /* normale ELF */
#else
#define RUECKMELDUNG_FASSUNG 't'             /* Test-ELF / Simulation (löscht nie) */
#endif
typedef struct {
    char modell[17], fw[6], bau[11], jb[80];
    char fe[RM_MAX][48]; int fa[RM_MAX], fe_n;   /* NEU 1.0.10: Fehler-Kürzel mit Anzahl, häufigste zuerst (bis 1.0.9: fehler, die ersten 5) */
    char uz[400];                                /* NEU 1.0.10: nicht lesbare Ordner "bereich.grund*anzahl~..." */
    char db[48];                                 /* NEU 1.0.10: Sonys Titel-Liste als Zähler "i12.p1.v3" (kern_appdb.c) */
    char hb[300], ek[300];                       /* NEU 1.0.10: Programme der Fehler ("playden*3~hb*1"), erfolgreiche Wege ("sony.ps5*2~datei.rest*9") */
    int s, n, g, f, p, ps, dd, sm, u, mb, t, bereich[7];
    bool ok, ab, sb, m2, ext, usb;
} rueckmeldung;
static int rm_zahl(int v) { return v < 0 ? 0 : v > 100000 ? 100000 : v; }
/* Bau-Stempel JJMMTTHHMM aus __DATE__ ("Nov  3 2026") und __TIME__ ("18:46:12") - welcher Code lief */
static const char *rm_baustempel(void)
{
    static char b[11];
    if (!b[0]) {
        static const char M[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
        const char *d = __DATE__, *z = __TIME__;
        int mon = 0; for (int i = 0; i < 12; i++) if (!strncmp(d, M + 3 * i, 3)) mon = i + 1;
        int tag = (d[4] == ' ' ? 0 : d[4] - '0') * 10 + (d[5] - '0');
        snprintf(b, sizeof b, "%c%c%02d%02d%c%c%c%c", d[9], d[10], mon, tag, z[0], z[1], z[3], z[4]);
    }
    return b;
}
/* nur erlaubte Zeichen übernehmen (Kürzel und Codes) - alles andere fällt weg */
static void rm_kopie(char *d, size_t n, const char *q, const char *erlaubt)
{
    size_t j = 0;
    for (; q && *q && j + 1 < n; q++) if ((*q >= 'a' && *q <= 'z') || (*q >= '0' && *q <= '9') || strchr(erlaubt, *q)) d[j++] = *q;
    d[j] = 0;
}
static rueckmeldung rueckmeldung_daten(void)
{
    rueckmeldung r; memset(&r, 0, sizeof r);
    /* Modellnummer: nur A-Z, 0-9 und "-" bis zum ersten anderen Zeichen ("CFI-7021 B01Y" -> "CFI-7021") */
    size_t j = 0;
    for (const char *q = KI.modell; *q && j < sizeof r.modell - 1; q++) {
        char c = *q; if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-') r.modell[j++] = c; else break;
    }
    /* Firmware nur in der Form 12.00 bzw. 9.60, sonst "unbekannt" */
    const char *w = KI.firmware; int d = 0; while (d < 3 && w[d] >= '0' && w[d] <= '9') d++;
    if (d >= 1 && d <= 2 && w[d] == '.' && w[d + 1] >= '0' && w[d + 1] <= '9' && w[d + 2] >= '0' && w[d + 2] <= '9') { memcpy(r.fw, w, (size_t)d + 3); r.fw[d + 3] = 0; }
    snprintf(r.bau, sizeof r.bau, "%s", rm_baustempel());
    r.dd = KI.ddd_bekannt ? (KI.ddd < -1 ? -1 : KI.ddd > 99 ? 99 : KI.ddd) : -1;
    pthread_mutex_lock(&rm_m);
    __typeof__(rm_stand) st = rm_stand;
    pthread_mutex_unlock(&rm_m);
#ifdef VORSCHAU
    /* nur PC-Vorschau: Fehler vorgeben, um den größten QR-Code zu prüfen (VORSCHAU_RM_FEHLER="a.b.c.d~...") */
    if (st.da && getenv("VORSCHAU_RM_FEHLER") && *getenv("VORSCHAU_RM_FEHLER")) {
        const char *q = getenv("VORSCHAU_RM_FEHLER"); st.fehler_n = 0; st.fehler = 0;
        while (*q && st.fehler_n < RM_MAX) { const char *e = strchr(q, '~'); size_t l = e ? (size_t)(e - q) : strlen(q); if (l > 47) l = 47;
            memcpy(st.fehlerliste[st.fehler_n], q, l); st.fehlerliste[st.fehler_n][l] = 0;
            char *stern = strchr(st.fehlerliste[st.fehler_n], '*'); st.fehler_anz[st.fehler_n] = stern ? atoi(stern + 1) : 1; if (stern) *stern = 0;
            st.fehler += st.fehler_anz[st.fehler_n]; st.fehler_n++; if (!e) break; q = e + 1; }
        if (getenv("VORSCHAU_RM_UZ")) snprintf(st.uz, sizeof st.uz, "%s", getenv("VORSCHAU_RM_UZ"));
        if (getenv("VORSCHAU_RM_DB")) snprintf(st.db, sizeof st.db, "%s", getenv("VORSCHAU_RM_DB"));
        if (getenv("VORSCHAU_RM_HB")) snprintf(st.prog, sizeof st.prog, "%s", getenv("VORSCHAU_RM_HB"));
        if (getenv("VORSCHAU_RM_EK")) snprintf(st.okl, sizeof st.okl, "%s", getenv("VORSCHAU_RM_EK"));
    }
#endif
    r.s = st.da ? 2 : st.gesucht ? 1 : 0;
    if (r.s >= 1) {
        r.n = rm_zahl(st.gefunden);
        rm_kopie(r.jb, sizeof r.jb, st.jb, ",");
        for (int b = 0; b < 7; b++) r.bereich[b] = rm_zahl(st.bereich[b]);
        r.u = rm_zahl(st.unlesbar); r.t = rm_zahl(st.dauer_s);
        rm_kopie(r.uz, sizeof r.uz, st.uz, ".*~-ABCDEFGHIJKLMNOPQRSTUVWXYZ");   /* NEU 1.0.10: nur Kürzel und Zahlen */
        rm_kopie(r.db, sizeof r.db, st.db, ".");                                 /* NEU 1.0.10: Datenbank-Übersicht, nur Kürzel und Zahlen */
        r.m2 = st.m2; r.ext = st.ext; r.usb = st.usb;
    }
    r.sm = st.sm_da ? 1 : 0;
    r.sb = zugriff_speicher_gebremst != 0;
    r.mb = rm_zahl((int)(basis_speicher_spitze() / 1048576));
    if (r.s == 2) {
        r.g = rm_zahl(st.gewaehlt); r.f = rm_zahl(st.fehler); r.p = rm_zahl(st.teilweise); r.ps = rm_zahl(st.geschuetzt); r.ab = st.ab;
        rm_kopie(r.hb, sizeof r.hb, st.prog, ".*~");   /* NEU 1.0.10: nur feste Kürzel und Zahlen */
        rm_kopie(r.ek, sizeof r.ek, st.okl, ".*~");
        if (st.sm_angehalten) r.sm = 2; else if (st.sm_unklar) r.sm = 3;
        for (int i = 0; i < st.fehler_n && r.fe_n < RM_MAX; i++) {
            char e[48]; rm_kopie(e, sizeof e, st.fehlerliste[i], ".-ABCDEFGHIJKLMNOPQRSTUVWXYZ");   /* EACCES & Co. in Großbuchstaben */
            if (!e[0]) continue;
            snprintf(r.fe[r.fe_n], sizeof r.fe[0], "%s", e); r.fa[r.fe_n] = st.fehler_anz[i] > 0 ? st.fehler_anz[i] : 1; r.fe_n++;
        }
        /* NEU 1.0.10: häufigste zuerst (passt der QR-Code nicht, fallen die seltensten weg) */
        for (int i = 0; i < r.fe_n; i++) for (int j = i + 1; j < r.fe_n; j++) if (r.fa[j] > r.fa[i]) {
            char x[48]; memcpy(x, r.fe[i], 48); memcpy(r.fe[i], r.fe[j], 48); memcpy(r.fe[j], x, 48); int y = r.fa[i]; r.fa[i] = r.fa[j]; r.fa[j] = y; }
    }
    r.ok = r.s == 2 ? (r.f == 0 && r.p == 0 && !r.ab && st.unveraendert == 0) : r.s == 1;   /* Codex-Prüfung 69: "unverändert" ist kein Erfolg */
    return r;
}
/* NEU 1.0.10: max_e / max_uz begrenzen die Fehler-Kürzel und Bereiche der nicht lesbaren Ordner (für den QR-Code; -1 = alle); extra = auch
 * Programme der Fehler (hb) und erfolgreiche Wege (ek) */
static const char *rueckmeldung_link_x(arena *a, const rueckmeldung *r, int max_e, int max_uz, bool extra)
{
    char e[RM_MAX * 56] = "";
    for (int i = 0; i < r->fe_n && (max_e < 0 || i < max_e); i++) { size_t l = strlen(e); snprintf(e + l, sizeof e - l, "%s%s*%d", l ? "~" : "", r->fe[i], r->fa[i]); }
    char uz[400] = "";
    for (const char *q = r->uz; *q && (max_uz != 0); ) {   /* Gruppen sind durch "~" getrennt, häufigste zuerst */
        const char *w = strchr(q, '~'); size_t l = w ? (size_t)(w - q) : strlen(q), ul = strlen(uz);
        if (ul + l + 2 >= sizeof uz) break;
        if (ul) uz[ul++] = '~';
        memcpy(uz + ul, q, l); uz[ul + l] = 0;
        if (max_uz > 0) max_uz--;
        if (!w) break;
        q = w + 1;
    }
    return a_fmt(a, "%s?v=%s&b=%c&bd=%s&s=%d&m=%s&fw=%s&l=%s&dd=%d&jb=%s&sp=%d%d%d&sm=%d&a=%d.%d.%d.%d.%d.%d.%d&n=%d&g=%d&f=%d&p=%d&ps=%d&u=%d&sb=%d&mb=%d&t=%d&ab=%d&ok=%d%s%s%s%s%s%s%s%s%s%s",
                 RUECKMELDUNG_ADRESSE, RUECKMELDUNG_VERSION, RUECKMELDUNG_FASSUNG, r->bau, r->s, r->modell, r->fw, SP_EN ? "en" : "de", r->dd, r->jb,
                 r->m2, r->ext, r->usb, r->sm, r->bereich[0], r->bereich[1], r->bereich[2], r->bereich[3], r->bereich[4], r->bereich[5], r->bereich[6],
                 r->n, r->g, r->f, r->p, r->ps, r->u, r->sb, r->mb, r->t, r->ab, r->ok ? 1 : 0, r->db[0] ? "&db=" : "", r->db, e[0] ? "&e=" : "", e, uz[0] ? "&uz=" : "", uz,
                 extra && r->hb[0] ? "&hb=" : "", extra ? r->hb : "", extra && r->ek[0] ? "&ek=" : "", extra ? r->ek : "");
}
static const char *rueckmeldung_link_n(arena *a, const rueckmeldung *r, int max_e, int max_uz) { return rueckmeldung_link_x(a, r, max_e, max_uz, true); }
static const char *rueckmeldung_link(arena *a, const rueckmeldung *r) { return rueckmeldung_link_n(a, r, -1, -1); }
/* die wichtigsten Angaben in Klartext (steht neben dem QR-Code; alles Weitere zeigt das Handy vor dem Senden) */
static const char *rueckmeldung_werte(arena *a, const rueckmeldung *r)
{
    const char *t = VERB(a, RUECKMELDUNG_VERSION, RUECKMELDUNG_FASSUNG == 't' ? T(" Test", " test") : "", " · ",
                         r->modell[0] ? r->modell : T("Modell unbekannt", "model unknown"), " · FW ", r->fw[0] ? r->fw : T("unbekannt", "unknown"));
    if (r->s >= 1) t = VERB(a, t, " · ", a_fmt(a, "%d", r->n), T(" gefunden", " found"));
    if (r->s == 2) t = VERB(a, t, " · ", a_fmt(a, "%d", r->g), T(" gewählt · ", " selected · "), a_fmt(a, "%d", r->f), T(" Fehler", " errors"),
                            r->p ? VERB(a, " · ", a_fmt(a, "%d", r->p), T(" nur teilweise", " only partly")) : "");
    return t;
}
static uint8_t qr_bild[qrcodegen_BUFFER_LEN_FOR_VERSION(20)], qr_puffer[qrcodegen_BUFFER_LEN_FOR_VERSION(20)];
/* QR-Code in qr_bild; liefert die Kantenlänge in Modulen (0 = ging nicht) */
static int qr_kodieren(const char *t)
{
    if (!qrcodegen_encodeText(t, qr_puffer, qr_bild, qrcodegen_Ecc_LOW, 1, 20, qrcodegen_Mask_AUTO, true)) return 0;   /* boostEcl: so viel Korrektur, wie in dieselbe Größe passt */
    return qrcodegen_getSize(qr_bild);
}
/* NEU 1.0.10: Rückmeldung als QR-Code - passt nicht alles hinein, zuerst weniger Bereiche, dann weniger Fehler-Kürzel (häufigste bleiben).
 * Die Browser-Bedienung schickt immer alles (rueckmeldung_link). */
static int rueckmeldung_qr(const rueckmeldung *r)
{
    /* NEU 1.0.10: zuerst fallen Bereiche und seltene Fehler weg, dann Programme und erfolgreiche Wege (hb, ek) */
    static const int STUFEN[][3] = { { -1, -1, 1 }, { -1, 6, 1 }, { 10, 4, 1 }, { 6, 3, 1 }, { 6, 3, 0 }, { 4, 2, 0 }, { 3, 0, 0 }, { 0, 0, 0 } };
    for (size_t i = 0; i < sizeof STUFEN / sizeof STUFEN[0]; i++) {
        int n = qr_kodieren(rueckmeldung_link_x(fa, r, STUFEN[i][0], STUFEN[i][1], STUFEN[i][2] != 0));
        if (n > 0) return n;
    }
    return 0;
}
static void qr_malen(int x, int y, int n, int m)   /* weißer Rand von 4 Modulen, schwarze Module - zeilenweise zusammengefasst */
{
    rechteck(x, y, (n + 8) * m, (n + 8) * m, 0xFFFFFF);
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; ) {
            if (!qrcodegen_getModule(qr_bild, i, j)) { i++; continue; }
            int b = i; while (i < n && qrcodegen_getModule(qr_bild, i, j)) i++;
            rechteck(x + (4 + b) * m, y + (4 + j) * m, (i - b) * m, m, 0x000000);
        }
}
/* Kasten "Rückmeldung (freiwillig)": QR-Code und darunter in Klartext, was drinsteht. Liefert die Höhe; 0 = passt nicht in max_h
 * (dann wird nichts gezeichnet). */
static int rueckmeldung_kasten(int x, int y, int w, int max_h)
{
    rueckmeldung r = rueckmeldung_daten();
    int n = rueckmeldung_qr(&r);
    if (n <= 0) return 0;
    const char *info = VERB(fa, T("Mit dem Handy scannen und sagen, ob es geklappt hat.\nIm Code: ", "Scan with your phone and tell us whether it worked.\nIn the code: "),
                            rueckmeldung_werte(fa, &r), T(" - dazu Jailbreak-Einstieg, Speicher, ShadowMount, Zahlen je Bereich", " - plus jailbreak entry, storage, ShadowMount, numbers per area"),
                            r.fe_n ? T(" und Fehlercodes", " and error codes") : "", T(". Das Handy zeigt alles vor dem Senden.", ". Your phone shows everything before sending."));
    const char *schutz = T("Keine Namen von Spielen oder Apps, kein Konto, keine Dateien. Gesendet wird erst mit \"Senden\" am Handy.",
                           "No game or app names, no account, no files. Nothing is sent until you tap \"Send\" on your phone.");
    /* Adresse zum Eintippen (wie bei ps5-captures): ohne Gerätedaten, nur 👍/👎 und Text */
    const char *adr = RUECKMELDUNG_ADRESSE; if (!strncmp(adr, "https://", 8)) adr += 8;
    const char *eintippen = VERB(fa, T("Ohne Scannen: ", "Without scanning: "), adr);   /* mit /r - nur diese Seite gibt es (Codex-Prüfung 69) */
    zeilen zi = umbrechen(0, 18, info, w - 40), zs = umbrechen(0, 18, schutz, w - 40), za = umbrechen(0, 17, eintippen, w - 40);
    int kopf = 54, texth = (zi.n + zs.n + za.n) * 24 + 36, platz = max_h - kopf - texth - 16;
    if (platz > w - 60) platz = w - 60;
    int m = platz / (n + 8);
    if (m < 3) return 0;                     /* zu klein zum Scannen - dann lieber gar nicht */
    int s = m * (n + 8), h = kopf + s + texth + 16;
    karte(x, y, w, h, F_RAND, F_KARTE2);
    text(x + 20, y + 36, 1, 22, F_TEXT, T("Rückmeldung (freiwillig)", "Feedback (optional)"));
    qr_malen(x + (w - s) / 2, y + kopf, n, m);
    int ty = y + kopf + s + 34;
    for (int i = 0; i < zi.n; i++, ty += 24) text(x + 20, ty, 0, 18, F_TEXT, zi.z[i]);
    ty += 6;
    for (int i = 0; i < zs.n; i++, ty += 24) text(x + 20, ty, 0, 18, F_GRAU, zs.z[i]);
    ty += 6;
    for (int i = 0; i < za.n; i++, ty += 24) text(x + 20, ty, 0, 17, F_GRAU, za.z[i]);
    return h;
}

/* =================================================================== NEU 1.0.8 (Arbeitsplan 4.5): "Löschen läuft" und "Fertig" nach Entwurf 2
 * Löschen läuft: oben "Wird gelöscht …" und "Bisher frei geräumt", darunter "Eintrag N von M" mit Balken, die letzten erledigten Einträge,
 * der laufende und "noch N Einträge"; ○ halten = nach diesem Eintrag anhalten (Arbeitsplan 3.4). Die Angaben kommen aus dem Kern (A.lf_*,
 * bei jedem Eintrag gesetzt). Ob ein Eintrag mit Fehler endete, zeigen die Zähler des Kerns - nur Anzeige, maßgeblich bleibt das Protokoll.
 * Fertig: "X frei geräumt", die Meldung des Kerns, Karten "Übrig geblieben", "Protokoll" und "Rückmeldung" (QR), Knöpfe Beenden / Neu starten. */
/* Codex-Prüfung 91: in einen festen Puffer kopieren - nie mitten in einem UTF-8-Zeichen abschneiden; gekürzt endet es mit "…" */
static void utf8_kopie(char *ziel, size_t n, const char *q)
{
    size_t l = strlen(q);
    if (l < n) { memcpy(ziel, q, l + 1); return; }
    size_t m = n - 4;                                                /* Platz für "…" (3 Byte) und das Ende */
    while (m > 0 && ((unsigned char)q[m] & 0xC0) == 0x80) m--;      /* q[m] ist das erste nicht kopierte Byte: kein Folgebyte */
    memcpy(ziel, q, m); memcpy(ziel + m, "…", 4);
}
typedef struct { const eintrag *f; char name[200]; long long b; bool hat_b; int zustand; } lz_zeile;   /* zustand: 0 läuft, 1 erledigt, 2 Fehler, 3 unverändert */
#define LZ_MAX 4   /* höchstens drei erledigte und der laufende */
static lz_zeile lz[LZ_MAX]; static int lz_n = 0, lz_nr = 0, lz_fehler = 0, lz_unv = 0;
static bool loeschseite = false, anhalten_gewuenscht = false;
/* Codex-Prüfung 76/77: alle Angaben des Kerns aus derselben Meldung (lf_seq ungerade = schreibt gerade; dann gilt der letzte Stand) -
 * die Seite liest nur diesen Stand. Fehler und Unverändertes je Meldung (lf_fehler, lf_unv) - so gehören sie zum richtigen Eintrag. */
typedef struct { int nr, n, fehler, unv; const eintrag *f; long long frei, gesamt; } lf_stand;
static lf_stand lf_jetzt;
static void lf_lesen(void)
{
    unsigned q = __atomic_load_n(&A.lf_seq, __ATOMIC_ACQUIRE);
    if (q & 1u) return;
    lf_stand s;
    s.nr = __atomic_load_n(&A.lf_nr, __ATOMIC_RELAXED); s.n = __atomic_load_n(&A.lf_n, __ATOMIC_RELAXED);
    s.fehler = __atomic_load_n(&A.lf_fehler, __ATOMIC_RELAXED); s.unv = __atomic_load_n(&A.lf_unv, __ATOMIC_RELAXED);
    s.f = __atomic_load_n(&A.lf_f, __ATOMIC_RELAXED);
    s.frei = __atomic_load_n(&A.lf_frei, __ATOMIC_RELAXED); s.gesamt = __atomic_load_n(&A.lf_gesamt, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    if (__atomic_load_n(&A.lf_seq, __ATOMIC_RELAXED) != q) return;
    lf_jetzt = s;
}
static void loeschseite_beginnen(void) { lz_n = 0; lz_nr = 0; lz_fehler = 0; lz_unv = 0; memset(&lf_jetzt, 0, sizeof lf_jetzt); anhalten_gewuenscht = false; loeschseite = true; }
static void lz_nachfuehren(void)
{
    lf_lesen();
    int nr = lf_jetzt.nr, fe = lf_jetzt.fehler, un = lf_jetzt.unv;
    const eintrag *f = lf_jetzt.f;
    if (nr != lz_nr && nr > 0 && f) {
        if (lz_n > 0 && lz[lz_n - 1].zustand == 0)   /* der vorige ist fertig - genau zuordnen lässt es sich nur, wenn keiner dazwischen lag */
            lz[lz_n - 1].zustand = nr == lz_nr + 1 && fe > lz_fehler ? 2 : nr == lz_nr + 1 && un > lz_unv ? 3 : 1;
        lz_fehler = fe; lz_unv = un;
        if (lz_n == LZ_MAX) { memmove(lz, lz + 1, sizeof lz[0] * (LZ_MAX - 1)); lz_n--; }
        lz_zeile *z = &lz[lz_n++];
        z->f = f; utf8_kopie(z->name, sizeof z->name, ohne_ort_vorne(kern_zeile(fa, f))); z->hat_b = f->HatGroesse; z->b = f->Bytes; z->zustand = 0;
        lz_nr = nr;
    }
    if (!A.anhaltbar && lz_n > 0 && lz[lz_n - 1].zustand == 0 && lf_jetzt.n > 0 && nr >= lf_jetzt.n)   /* der letzte ist durch - der Kern zählt nicht mehr */
        lz[lz_n - 1].zustand = K->FEHLER_ZAHL > lz_fehler ? 2 : K->UNVERAENDERT.n > lz_unv ? 3 : 1;
}
static const char *protokoll_wo(void)   /* "auf dem USB-Laufwerk" usw. (aus A.protokoll_datei); "" = nur in der Ansicht */
{
    const char *p = A.protokoll_datei ? A.protokoll_datei : "";
    if (txt_beginnt(p, T("auf dem USB-Laufwerk", "on the USB drive"))) return T("auf dem USB-Laufwerk", "on the USB drive");
    if (txt_beginnt(p, T("auf der M.2", "on the M.2"))) return T("auf der M.2", "on the M.2");
    if (txt_beginnt(p, T("auf der PS5", "on the PS5"))) return T("auf der PS5", "on the PS5");
    return "";
}
static void schirm_loeschen(void)
{
    rechteck(0, 0, BREITE, HOEHE, N_HG);
    kopf_neu(3, NULL);
    bilder_neu = 0;
    lz_nachfuehren();
    pthread_mutex_lock(&G.m); double p = G.prozent; bool unb = G.unbestimmt; pthread_mutex_unlock(&G.m);
    int nr = lf_jetzt.nr, n = lf_jetzt.n, bw = BREITE - 2 * RAND_X;   /* Codex-Prüfung 77: derselbe Stand wie die Liste */
    long long frei = lf_jetzt.frei, ges = lf_jetzt.gesamt;
    bool sim = K->simulation, durch = n > 0 && nr >= n && !A.anhaltbar;
    int y = 280;   /* fest (für fünf Zeilen mittig) - die Seite springt nicht, wenn die Liste wächst */
    text(RAND_X, y, 1, 48, N_TEXT, sim ? T("Simulation läuft …", "Simulation running …") : durch ? T("Fast fertig …", "Almost done …") : T("Wird gelöscht …", "Deleting …"));
    if (n > 0) {   /* sind nicht alle Größen bekannt: "mind." */
        text_rechts(BREITE - RAND_X, y - 44, 0, 24, N_GRAU2, sim ? T("Würde frei geräumt", "Would be freed") : T("Bisher frei geräumt", "Freed so far"));
        text_rechts(BREITE - RAND_X, y, 1, 40, N_TEXT, groesse_von(fa, frei, ges < 0 && frei > 0));
    }
    int ry = y + 64;
    text(RAND_X, ry, 1, 28, N_TEXT, nr > 0 ? a_fmt(fa, T("Eintrag %d von %d", "Entry %d of %d"), nr, n) : T("Wird vorbereitet …", "Preparing …"));
    int pz = (int)(p + .5); if (pz < 0) pz = 0; if (pz > 100) pz = 100;
    if (!unb && nr > 0) text_rechts(BREITE - RAND_X, ry, 1, 28, N_GRAU, a_fmt(fa, "%d %%", pz));
    int by = ry + 14;
    rund(RAND_X, by, bw, 16, 8, 0x1B2638);
    if (unb) { int w = 320, pos = (int)(SDL_GetTicks() / 3 % (Uint32)(bw + w)) - w, a = pos < 0 ? 0 : pos, b = pos + w > bw ? bw : pos + w; if (b > a) rund(RAND_X + a, by, b - a, 16, 8, N_BLAU); }
    else if (pz > 0) { int w = bw * pz / 100; rund(RAND_X, by, w < 16 ? 16 : w, 16, 8, N_BLAU); }
    /* Liste: die letzten erledigten, der laufende, "noch N Einträge" */
    int rest = n - nr; if (rest < 0) rest = 0;
    int zeilen = lz_n + (rest > 0 ? 1 : 0), ly = by + 16 + 32;
    if (zeilen > 0) {
        int lh = 24 + zeilen * 72 + (zeilen - 1) * 4;
        karte_rund(RAND_X, ly, bw, lh, 22, 1, N_RAND, N_KARTE);
        for (int i = 0; i < zeilen; i++) {
            int zy = ly + 12 + i * 76, cy = zy + 36, x = RAND_X + 12 + 18;
            bool wartet = i >= lz_n, laeuft = !wartet && lz[i].zustand == 0;
            uint32_t zhg = laeuft ? farbmisch(N_FOKUS, N_KARTE, .08f) : N_KARTE;
            if (laeuft) karte_rund(RAND_X + 12, zy, bw - 24, 72, 14, 2, farbmisch(N_FOKUS, N_KARTE, .5f), zhg);
            if (wartet) ring(x + 18, cy, 16.5f, 2.5f, N_LINIE);
            else if (laeuft) { ring(x + 18, cy, 13, 2.6f, farbmisch(N_FOKUS, N_KARTE, .25f)); ring_teil(x + 18, cy, 13, 2.6f, .25f, N_FOKUS); }
            else {
                int zu = lz[i].zustand; uint32_t f = zu == 1 ? N_GRUEN : zu == 2 ? N_ROT : N_GRAU2;
                kreis(x + 18, cy, 18, farbmisch(f, N_KARTE, .16f));
                symbol(zu == 1 ? SYM_HAKEN : zu == 2 ? SYM_WARNUNG : SYM_INFO, x + 8, cy - 10, 20, f);
            }
            x += 36 + 18;
            if (!wartet) kachel_klein(x, cy - 22, 44, lz[i].f, lz[i].f->Kat, zhg);
            x += 44 + 18;
            const char *zt = wartet ? T("wartet", "waiting") : laeuft ? T("läuft …", "running …") : lz[i].zustand == 1 ? T("erledigt", "done")
                           : lz[i].zustand == 2 ? T("Fehler", "error") : T("unverändert", "unchanged");
            uint32_t zf = wartet ? N_GRAU2 : laeuft ? N_FOKUS : lz[i].zustand == 1 ? N_GRUEN : lz[i].zustand == 2 ? N_ROT : N_GRAU2;
            int xr = BREITE - RAND_X - 12 - 18;
            if (!wartet && lz[i].hat_b) text_rechts(xr, cy + 9, 1, 26, laeuft ? N_TEXT : N_TEXT2, groesse_von(fa, lz[i].b, false));
            xr -= 150;
            text_rechts(xr, cy + 9, 1, 24, zf, zt); xr -= textbreite(1, 24, zt) + 24;
            const char *nm = wartet ? (rest == 1 ? T("noch 1 Eintrag", "1 more entry") : a_fmt(fa, T("noch %d Einträge", "%d more entries"), rest)) : lz[i].name;
            text_passend(x, cy + 9, laeuft ? 1 : 0, 26, 24, wartet ? N_GRAU2 : laeuft ? N_TEXT : N_TEXT2, nm, xr - x);
        }
        ly += lh;
    }
    /* darunter: Hinweis */
    int iy = ly + 24 + 26;
    if (anhalten_gewuenscht && A.anhaltbar) {
        symbol(SYM_INFO, RAND_X, iy - 20, 24, N_GELB);
        text(RAND_X + 36, iy, 1, 24, N_GELB, nr > 0 ? T("Hält nach diesem Eintrag an - der laufende wird noch fertig gelöscht.", "Stopping after this entry - the current one is still being finished.")
                                                      : T("Hält an, bevor etwas gelöscht wird.", "Stopping before anything is deleted."));
    } else {
        const char *wo = protokoll_wo();
        const char *t = sim ? T("Simulation: Es wird nichts gelöscht.", "Simulation: nothing is deleted.")
                            : VERB(fa, T("Bitte die Konsole nicht ausschalten.", "Please do not turn off the console."),
                                   wo[0] ? VERB(fa, T(" Das Protokoll wird ", " The log is being written "), wo, T(" mitgeschrieben.", ".")) : "");
        symbol(SYM_INFO, RAND_X, iy - 20, 24, N_GRAU2);
        text(RAND_X + 36, iy, 0, 24, N_GRAU, t);
    }
    fussknopf kn[2]; int nk = 0;
    bool anhalten = A.loescht && A.anhaltbar && !anhalten_gewuenscht;
    if (anhalten) kn[nk++] = (fussknopf){ KN_O, T("halten: nach diesem Eintrag abbrechen", "hold: stop after this entry"), false };
    kn[nk++] = (fussknopf){ KN_OPT, T("Protokoll", "Log"), true };
    fuss_neu(kn, nk);
    if (kreis_seit && anhalten) { int anteil = (int)(SDL_GetTicks() - kreis_seit); if (anteil > 1000) anteil = 1000; rechteck(0, HOEHE - 6, BREITE * anteil / 1000, 6, N_FOKUS); }
}
static const char *kuerzen_still(int f, int gr, const char *t, int breite)   /* wie kuerzen, aber gewollt (ohne "ZU LANG") */
{
    if (textbreite(f, gr, t) <= breite) return t;
    int el = textbreite(f, gr, "…");
    const char *q = t; int b = 0;
    while (*q) { const char *v = q; Glyph *g = glyph(f, gr, cp_naechstes(&q)); b += g ? g->vorschub : 0; if (b + el > breite) { q = v; break; } }
    return VERB(fa, a_kopie_n(fa, t, (size_t)(q - t)), "…");
}
static int absatz_bis(int x, int y, int f, int gr, uint32_t farbe, const char *t, int breite, int lh, int max)   /* höchstens max Zeilen, Rest "…" */
{
    zeilen z = umbrechen(f, gr, t, breite);
    int n = z.n < max ? z.n : max;
    for (int i = 0; i < n; i++) {
        const char *s = z.z[i];
        if (i == n - 1 && z.n > max) { for (int j = i + 1; j < z.n; j++) s = VERB(fa, s, " ", z.z[j]); s = kuerzen_still(f, gr, s, breite); }
        text(x, y + i * lh, f, gr, farbe, s);
    }
    return n * lh;
}
static int mitte_zeilen(int cx, int y, int f, int gr, uint32_t farbe, const char *t, int breite, int lh, int max)   /* zentriert, umbrochen */
{
    zeilen z = umbrechen(f, gr, t, breite);
    int n = z.n < max ? z.n : max;
    for (int i = 0; i < n; i++) text_mitte(cx, y + i * lh, f, gr, farbe, i == n - 1 && z.n > max ? kuerzen_still(f, gr, VERB(fa, z.z[i], " …"), breite) : z.z[i]);
    return n * lh;
}
static void fertig_feld(int x, int y, const float *sym, uint32_t f, uint32_t feld) { rund(x, y, 56, 56, 16, feld); symbol(sym, x + 14, y + 14, 28, f); }
static bool fertig_neu(void) { return !kern_bereiche_106() && !K->DB_HINWEIS; }   /* der Vergleichsmodus 1.0.6 der Tests behält das alte Fenster */
static void schirm_fertig(void)
{
    rechteck(0, 0, BREITE, HOEHE, N_HG);
    kopf_neu(4, NULL);
    bilder_neu = 0;
    pthread_mutex_lock(&G.m);
    const char *kopf = a_kopie(fa, G.f_kopf ? G.f_kopf : ""), *frei = a_kopie(fa, G.f_frei ? G.f_frei : ""), *sm = a_kopie(fa, G.f_sm ? G.f_sm : "");
    bool ok = G.f_ok;
    pthread_mutex_unlock(&G.m);
    bool sim = K->simulation;
    int cx = BREITE / 2, bw = BREITE - 2 * RAND_X;
    uint32_t af = ok ? N_GRUEN : N_ORANGE;
    kreis(cx, 214, 52, farbmisch(af, N_HG, .16f));
    symbol(ok ? SYM_HAKEN : SYM_WARNUNG, cx - 27, 214 - 27, 54, af);
    const char *gt = groesse_von(fa, A.rm_frei.Bytes, A.rm_frei.Gekappt);
    text_mitte(cx, 344, 1, 72, N_TEXT, sim ? VERB(fa, gt, T(" würden frei", " would be freed")) : VERB(fa, gt, T(" frei geräumt", " freed")));
    /* darunter die Meldung des Kerns (erste Zeile), Dateien und Ordner, dann z. B. die Selbstentfernung */
    const char *nl = strchr(kopf, '\n');
    const char *k1 = nl ? a_kopie_n(fa, kopf, (size_t)(nl - kopf)) : kopf, *k2 = nl ? nl + 1 : "";
    int yy = 394;
    yy += mitte_zeilen(cx, yy, 0, 28, ok ? N_GRAU : N_ORANGE, k1, bw, 36, 2);
    if (frei[0]) yy += mitte_zeilen(cx, yy, 0, 24, N_GRAU2, frei, bw, 32, 1);
    if (k2[0]) yy += mitte_zeilen(cx, yy, 0, 24, N_GRAU2, k2, bw, 32, 2);
    /* Karten */
    int byy = 826, smh = sm[0] ? 70 : 0, ky = yy + 4, kh = byy - 34 - smh - ky;
    if (kh > 300) { ky += (kh - 300) / 2; kh = 300; }
    int kw = (bw - 48) / 3;
    const char *rt = kern_rest_text(K, fa);
    bool nichts = ok && !rt[0];
    /* 1: Übrig geblieben */
    karte_rund(RAND_X, ky, kw, kh, 22, 1, N_RAND, N_KARTE);
    fertig_feld(RAND_X + 28, ky + 28, nichts ? SYM_HAKEN : SYM_WARNUNG, nichts ? N_GRUEN : N_ORANGE, farbmisch(nichts ? N_GRUEN : N_ORANGE, N_KARTE, .14f));
    text(RAND_X + 104, ky + 65, 0, 24, N_GRAU2, T("Übrig geblieben", "Left over"));
    absatz_bis(RAND_X + 28, ky + 124, 0, 24, nichts ? N_TEXT : N_TEXT2, nichts ? T("Nichts. Alles wurde vollständig entfernt.", "Nothing. Everything was removed completely.")
                                                                 : rt[0] ? rt : T("Nicht alles - was übrig ist, steht im Protokoll.", "Not everything - the log shows what is left."),
               kw - 56, 30, (kh - 124 + 6) / 30);
    /* 2: Protokoll */
    int x2 = RAND_X + kw + 24;
    karte_rund(x2, ky, kw, kh, 22, 1, N_RAND, N_KARTE);
    fertig_feld(x2 + 28, ky + 28, SYM_DOKUMENT, N_GRAU, N_FELD);
    text(x2 + 104, ky + 65, 0, 24, N_GRAU2, T("Protokoll", "Log"));
    {
        const char *wo = protokoll_wo(), *pd = A.protokoll_datei ? A.protokoll_datei : "", *dp = strchr(pd, ':');
        int ty = ky + 124;
        ty += absatz_bis(x2 + 28, ty, 0, 26, N_TEXT, wo[0] ? VERB(fa, T("Gespeichert ", "Saved "), wo) : T("Nur in der Protokoll-Ansicht", "Only in the log view"), kw - 56, 34, 2);
        absatz_bis(x2 + 28, ty + 4, 0, 24, N_GRAU2, wo[0] && dp ? dp + 2 : T("Mit der Options-Taste ansehen.", "View it with the Options button."), kw - 56, 30, (ky + kh - 16 - ty) / 30);
    }
    /* 3: Rückmeldung (QR-Code) */
    int x3 = x2 + kw + 24, tx = x3 + 28;
    karte_rund(x3, ky, kw, kh, 22, 1, N_RAND, N_KARTE);
    {
        rueckmeldung r = rueckmeldung_daten();
        int qn = rueckmeldung_qr(&r);
        bool qr_da = false;   /* Codex-Prüfung 91: ohne Platz für den Code kein "Mit dem Handy scannen" */
        if (qn > 0) {
            int m = (kh - 36) / (qn + 8); if (m > 5) m = 5;
            if (m >= 3) { int s = m * (qn + 8); qr_malen(x3 + 18, ky + (kh - s) / 2, qn, m); tx = x3 + 18 + s + 22; qr_da = true; }
        }
        int tw = x3 + kw - 20 - tx, ty = ky + 64;
        text(tx, ty, 0, 24, N_GRAU2, kuerzen_still(0, 24, T("Rückmeldung geben", "Give feedback"), tw)); ty += 40;
        if (qr_da) {
            ty += absatz_bis(tx, ty, 0, 26, N_TEXT, T("Mit dem Handy scannen", "Scan with your phone"), tw, 34, 2) + 8;
            absatz_bis(tx, ty, 0, 24, N_GRAU, T("Keine Namen, kein Konto, keine Dateien. Gesendet wird erst am Handy.",
                                                "No names, no account, no files. Nothing is sent until you confirm on your phone."), tw, 30, (ky + kh - 12 - ty) / 30 + 1);
        } else
            absatz_bis(tx, ty, 0, 24, N_TEXT, T("Hier ist gerade kein Platz für den QR-Code. Schick uns gern das Protokoll (GitHub oder psxtools.de).",
                                                "There is no room for the QR code here right now. Feel free to send us the log (GitHub or psxtools.de)."), tw, 30, (ky + kh - 12 - ty) / 30 + 1);
    }
    if (sm[0]) { symbol(SYM_INFO, RAND_X, ky + kh + 26, 24, N_GELB); absatz_bis(RAND_X + 36, ky + kh + 46, 0, 24, N_GELB, sm, bw - 36, 30, 2); }
    /* Knöpfe: ○ Beenden, ✕ Neu starten (wie bisher: jede Taste hat ihren Knopf) */
    const char *bt = T("Beenden", "Quit"), *nt = T("Neu starten", "Restart");
    int bh = 80, bw1 = 30 + 38 + 14 + textbreite(1, 26, bt) + 30, bw2 = 38 + 40 + 14 + textbreite(1, 28, nt) + 38, bx = (BREITE - (bw1 + 24 + bw2)) / 2, nx = bx + bw1 + 24;
    karte_rund(bx, byy, bw1, bh, 18, 2, N_LINIE, N_KARTE);
    ring(bx + 30 + 19, byy + bh / 2, 18, 2, N_GRAU2); symbol(SYM_KN_KREIS, bx + 30 + 11, byy + bh / 2 - 8, 16, N_TEXT);
    text(bx + 30 + 38 + 14, byy + bh / 2 + 10, 1, 26, N_TEXT, bt);
    rund(nx - 9, byy - 9, bw2 + 18, bh + 18, 27, farbmisch(N_FOKUS, N_HG, .18f)); rund(nx - 3, byy - 3, bw2 + 6, bh + 6, 21, N_FOKUS); rund(nx, byy, bw2, bh, 18, N_BLAU);
    ring(nx + 38 + 20, byy + bh / 2, 19, 2, 0xFFFFFF); symbol(SYM_KN_KREUZ, nx + 38 + 12, byy + bh / 2 - 8, 16, 0xFFFFFF);
    text(nx + 38 + 40 + 14, byy + bh / 2 + 10, 1, 28, 0xFFFFFF, nt);
    text_mitte(cx, byy + bh + 34, 0, 24, sim ? N_GELB : N_GRAU2, sim ? T("Simulation: Der Neustart wird nur angezeigt, nicht ausgeführt.", "Simulation: the restart is only shown, not carried out.")
                                                                     : T("Neu starten wird empfohlen.", "Restarting is recommended."));
    fussknopf kn[3] = { { KN_X, T("Neu starten", "Restart"), false }, { KN_O, T("Beenden", "Quit"), false }, { KN_OPT, T("Protokoll ansehen", "View log"), true } };
    hinweis_ueber = byy - 16;
    fuss_neu(kn, 3);
}

/* =================================================================== NEU 1.0.8 (Arbeitsplan 4.5, K5): Fragen und Fehler als eigene Seite
 * nach Entwurf 2 statt der alten Fenster (z. B. Übersicht der vollständigen Bereinigung, M.2 und erweiterter Speicher behalten/leeren,
 * Jailbreak-Warnung, ShadowMount, PS5 Cleanup selbst entfernen, Fehler). Oben Zeichen und Titel, in der Mitte der Text des Kerns in einer
 * Karte (↑↓ blättert), darunter die Knöpfe: links der sichere (○, hier startet die Seite), rechts der andere (rot) - wie auf der Seite
 * "Bestätigen". Eigene Entscheidung: die Seiten tauschen damit gegenüber dem alten Fenster die Seiten der Knöpfe (← sicher, → anders).
 * Im Text: "!!! … !!!" rot, Aufzählungen mit Punkt, Schlüsselwörter wie "BEHALTEN:" fett - grün, wenn sie zum sicheren Knopf gehören,
 * rot beim anderen. Die Fragen selbst stellt der Kern wie bisher (Mitschnitt und Nachbau unverändert). */
typedef struct { const char *t; int x, f, gr, h; uint32_t farbe; } fz_zeile;
static int frage_zeilen(fz_zeile *out, int max, const char *text, int breite, const char *sicher, const char *gef)
{
    int n = 0;
    const char *gs = a_gross(fa, sicher ? sicher : ""), *gg = a_gross(fa, gef ? gef : "");
    sliste q = r_split(fa, text ? text : "", "\n");
    for (int i = 0; i < q.n && n < max; i++) {
        const char *s = q.e[i];
        int ein = 0; while (s[ein] == ' ') ein++;
        const char *rest = s + ein;
        if (!rest[0]) { if (n > 0 && out[n - 1].h != 18) out[n++] = (fz_zeile){ "", 0, 0, 26, 18, 0 }; continue; }   /* Absatz */
        uint32_t farbe = N_TEXT2; int f = 0, x = ein > 0 ? 28 : 0;
        if (strstr(rest, "!!!")) { rest = r_r(fa, r_r(fa, rest, "^!!!\\s*", ""), "\\s*!!!$", ""); farbe = N_ROT; f = 1; }
        else if (txt_beginnt(rest, "SIMULATION") || txt_beginnt(rest, "TEST:")) { farbe = N_GELB; f = 1; }
        bool punkt = false;
        if (txt_beginnt(rest, "- ")) { rest = rest + 2; punkt = true; x = ein > 3 ? 56 : 28; }
        htreffer kw = h_mt(fa, rest, "(?-i)^([A-ZÄÖÜ0-9][A-ZÄÖÜ0-9 +/.-]{2,40}):\\s+(.+)$");
        if (kw.ok && !punkt && farbe == N_TEXT2) {   /* "BEHALTEN: …" - fett, farbig nach dem passenden Knopf */
            const char *w = kw.gr[1];
            uint32_t kf = strstr(gs, w) ? N_GRUEN : strstr(gg, w) ? N_ROT : N_TEXT;
            if (n < max) out[n++] = (fz_zeile){ w, x, 1, 24, 34, kf };
            rest = kw.gr[2];
            if (rest[0] >= 'a' && rest[0] <= 'z') { char *g = a_kopie(fa, rest); g[0] = (char)(g[0] - 'a' + 'A'); rest = g; }   /* steht jetzt am Zeilenanfang */
        }
        zeilen z = umbrechen(f, 26, rest, breite - x - (punkt ? 30 : 0));
        for (int j = 0; j < z.n && n < max; j++) {
            if (punkt && j == 0 && n < max) out[n] = (fz_zeile){ VERB(fa, "•  ", z.z[j]), x, f, 26, 36, farbe };
            else out[n] = (fz_zeile){ z.z[j], x + (punkt ? 30 : 0), f, 26, 36, farbe };
            n++;
        }
    }
    while (n > 0 && out[n - 1].h == 18) n--;
    return n;
}
static void frage_knopf(int x, int y, int w, int h, bool fok, uint32_t fuell, const float *sym, const char *t, uint32_t tf)
{
    if (fok) { rund(x - 9, y - 9, w + 18, h + 18, 27, farbmisch(N_FOKUS, N_HG, .18f)); rund(x - 3, y - 3, w + 6, h + 6, 21, N_FOKUS); }
    rund(x, y, w, h, 18, fuell);
    int cx = x + 30 + 19, cy = y + h / 2;
    ring(cx, cy, 18, 2, farbmisch(tf, fuell, .7f)); symbol(sym, cx - 8, cy - 8, 16, tf);
    text(x + 30 + 38 + 14, cy + 10, 1, 26, tf, t);
}
static void schirm_frage(void)
{
    rechteck(0, 0, BREITE, HOEHE, N_HG);
    kopf_neu(auftrag_jetzt == AUF_SUCHEN ? 1 : 3, NULL);
    bilder_neu = 0;
    pthread_mutex_lock(&G.m);
    dialogart art = G.art;
    const char *titel = a_kopie(fa, G.titel ? G.titel : ""), *txt = a_kopie(fa, G.text ? G.text : ""), *si = a_kopie(fa, G.sicher ? G.sicher : ""), *ge = a_kopie(fa, G.gef ? G.gef : "");
    pthread_mutex_unlock(&G.m);
    bool fehler = art == D_FEHLER;
    if (fehler && !strcmp(titel, "PS5 Cleanup")) {
        const char *v = T("Es ist ein Fehler aufgetreten:\n", "An error occurred:\n");
        titel = T("Es ist ein Fehler aufgetreten", "An error occurred");
        if (txt_beginnt(txt, v)) txt += strlen(v);
    }
    bool warnung = fehler || strstr(txt, "!!!") || txt_beginnt(txt, T("ACHTUNG", "WARNING"));
    uint32_t sf = fehler ? N_ROT : warnung ? N_ORANGE : N_FOKUS;
    rund(RAND_X, 150, 64, 64, 18, farbmisch(sf, N_HG, .16f));
    symbol(warnung ? SYM_WARNUNG : SYM_INFO, RAND_X + 16, 166, 32, sf);
    text_passend(RAND_X + 88, 198, 1, 44, 32, N_TEXT, titel, BREITE - 2 * RAND_X - 88);
    /* der Text in einer Karte - ↑↓ blättert */
    int kx = RAND_X, ky = 248, kw = BREITE - 2 * RAND_X, kh = 520, pad = 36;
    karte_rund(kx, ky, kw, kh, 22, 1, N_RAND, N_KARTE);
    fz_zeile *z = a_hole(fa, sizeof(fz_zeile) * 600);
    int n = frage_zeilen(z, 600, txt, kw - 2 * pad - 24, si, ge), sichtbar = kh - 2 * pad + 10, ges = 0;
    for (int i = 0; i < n; i++) ges += z[i].h;
    int max_start = n; { int h = 0; while (max_start > 0 && h + z[max_start - 1].h <= sichtbar) h += z[--max_start].h; }
    if (dlg_scroll > max_start) dlg_scroll = max_start;
    if (dlg_scroll < 0) dlg_scroll = 0;
    clip_y0 = ky + 6; clip_y1 = ky + kh - 6;
    int yy = ky + pad + 22;
    for (int i = dlg_scroll; i < n && yy - 28 < ky + kh; i++) { if (z[i].t[0]) text(kx + pad + z[i].x, yy, z[i].f, z[i].gr, z[i].farbe, z[i].t); yy += z[i].h; }
    clip_y0 = 0; clip_y1 = 1 << 30;
    bool mehr = ges > sichtbar;
    if (mehr) {
        int bh = kh - 48, sh = bh * sichtbar / ges, sy = ky + 24 + (max_start > 0 ? (bh - sh) * dlg_scroll / max_start : 0);
        rund(kx + kw - 20, ky + 24, 6, bh, 3, farbmisch(N_LINIE, N_KARTE, .5f)); rund(kx + kw - 20, sy, 6, sh, 3, N_GRAU2);
        if (dlg_scroll < max_start) for (int i = 0; i < 40; i++) { float al = (float)(i + 1) / 40.f; for (int px = kx + 2; px < kx + kw - 30; px++) misch(px, ky + kh - 42 + i, N_KARTE, al); }
    }
    /* Knöpfe */
    int byy = ky + kh + 38, bh = 80;
    if (fehler) {
        const char *ok = "OK"; int bw = 30 + 38 + 14 + textbreite(1, 26, ok) + 40, bx = (BREITE - bw) / 2;
        frage_knopf(bx, byy, bw, bh, true, N_BLAU, SYM_KN_KREUZ, ok, 0xFFFFFF);
    } else {
        int bs = 30 + 38 + 14 + textbreite(1, 26, si) + 34, bg = 30 + 38 + 14 + textbreite(1, 26, ge) + 34;
        int x0 = (BREITE - (bs + 28 + bg)) / 2, xg = x0 + bs + 28;
        bool fs = dlg_fokus == 0, fg = dlg_fokus == 1;
        frage_knopf(x0, byy, bs, bh, fs, N_BLAU, SYM_KN_KREIS, si, 0xFFFFFF);
        frage_knopf(xg, byy, bg, bh, fg, fg ? N_ROT_KNOPF : farbmisch(N_ROT_KNOPF, N_HG, .45f), SYM_KN_KREUZ, ge, fg ? 0xFFFFFF : farbmisch(0xFFFFFF, N_HG, .8f));
    }
    fussknopf kn[5]; int nk = 0;
    if (fehler) kn[nk++] = (fussknopf){ KN_X, "OK", false };
    else {
        kn[nk++] = (fussknopf){ KN_LINKSRECHTS, T("Knopf wählen", "Choose button"), false };
        kn[nk++] = (fussknopf){ KN_X, T("drücken", "Press"), false };
        kn[nk++] = (fussknopf){ KN_O, T("sicherer Knopf", "Safe button"), false };
    }
    if (mehr) kn[nk++] = (fussknopf){ KN_HOCHRUNTER, T("weiterlesen", "Read more"), false };
    kn[nk++] = (fussknopf){ KN_OPT, T("Protokoll", "Log"), true };
    hinweis_ueber = byy - 16;
    fuss_neu(kn, nk);
}

/* Fragen (FrageWahl / FrageText / FertigFenster / Meldung) */
static void schirm_dialog(void)
{
    abdunkeln();
    pthread_mutex_lock(&G.m);
    dialogart art = G.art;
    int w = 1500, x = (BREITE - w) / 2, y = 60, h = HOEHE - 120;
    karte(x, y, w, h, art == D_WAHL || art == D_TEXT ? F_BLAU : F_RAND, F_KARTE);
    if (art == D_FERTIG) {
        int yy = y + 70;
        yy += absatz(x + 40, yy, 1, 30, G.f_ok ? F_GRUEN : F_ORANGE, G.f_kopf, w - 80, 38);
        yy += absatz(x + 40, yy + 6, 0, 23, F_TEXT, G.f_frei, w - 80, 30) + 10;
        if (G.f_sm && G.f_sm[0]) yy += absatz(x + 40, yy + 16, 0, 23, F_TEXT, G.f_sm, w - 80, 30) + 16;
        /* NEU 1.0.7 (da_Chris86 03.11.2026): rechts der Kasten für die freiwillige Rückmeldung (QR-Code). Der Rest-Text rückt dafür nach
         * links; passt er schmal nicht über die Protokoll-Zeile, bleibt er breit und der Kasten entfällt (Rückmeldung dann über Options). */
        const int qr_w = 540; int tb = w - 80;
        if (!K->DB_HINWEIS) {
            const char *rt0 = !kern_bereiche_106() ? kern_rest_text(K, fa) : "";
            int schmal = w - 80 - qr_w - 30, rh = rt0[0] ? umbrechen(1, 21, rt0, schmal).n * 27 + 16 : 0;
            if (yy + 16 + rh <= y + h - 250 && rueckmeldung_kasten(x + w - 40 - qr_w, yy + 24, qr_w, y + h - 120 - (yy + 24)) > 0) tb = schmal;   /* rechts frei bis über die Knöpfe */
        }
        /* NEU 1.0.6: "Datenbank neu aufbauen" nur, wenn Kacheln im Menü bleiben (mit Namen) - ohne Sony wie bisher */
        if (!kern_bereiche_106()) {   /* NEU 1.0.7 (da_Chris86 31.10.2026 19:56): was bleibt, ohne Neuaufbau-Empfehlung; PS4-Folgen mit Namen */
            const char *rt = kern_rest_text(K, fa);
            if (rt[0]) yy += absatz(x + 40, yy + 16, 1, 21, F_ORANGE, rt, tb, 27) + 16;
        }
        else if (K->SONY && K->GEISTER.n > 0)
            yy += absatz(x + 40, yy + 16, 1, 23, F_ORANGE, VERB(fa, T("Diese Einträge bleiben im PS5-Menü sichtbar, bis die Datenbank neu aufgebaut ist: ", "These entries stay visible in the PS5 menu until the database has been rebuilt: "),
                                                                   sl_join(fa, &K->GEISTER, ", ")), tb, 30) + 16;
        if (K->DB_HINWEIS) {
        /* SCHLUSS_TEXT: Überschrift, nummerierte Schritte, Warnung */
        const char **sz; int n = kern_schluss_text(&sz);
        int bx = x + 40, by = yy + 20, bh = 44 + (K->simulation ? 34 : 0) + 40 + (n - 2) * 38 + 46 + 24;
        karte(bx, by, w - 80, bh, F_RAND, F_KARTE2);
        int ty = by + 44;
        if (K->simulation) { text(bx + 24, ty, 0, 20, F_GELB, T("SIMULATION - nach einem echten Löschen käme jetzt:", "SIMULATION - after real deleting, this would come next:")); ty += 34; }
        for (int i = 0; i < n; i++) {
            const char *s = sz[i]; int d = 0; while (s[d] >= '0' && s[d] <= '9') d++;
            if (d && s[d] == '.') { text(bx + 24, ty, 0, 23, F_TEXT, a_kopie_n(fa, s, (size_t)d + 1)); const char *r = s + d + 1; while (*r == ' ') r++; text(bx + 70, ty, 0, 23, F_TEXT, r); ty += 38; }
            else if (i == n - 1) { text(bx + 24, ty + 8, 1, 23, F_ORANGE, s); ty += 46; }
            else { text(bx + 24, ty, 1, 23, F_TEXT, s); ty += 40; }
        }
        }
        absatz_max(x + 40, y + h - 230, 0, 18, F_GRAU, VERB(fa, T("Protokoll: ", "Log: "), A.protokoll_datei), tb, 22, 2);   /* NEU 1.0.7: neben dem Rückmelde-Kasten umbrechen */
        /* da_Chris86 02.11.2026 15:41-15:54: am Ende ✕ Neu starten (vorgewählt) oder ○ Beenden */
        const char *kn[2] = { T("✕  Neu starten", "✕  Restart"), T("○  Beenden", "○  Quit") };
        int kx = x + w - 40, ky = y + h - 100;
        text(x + 40, ky - 46, 0, 20, F_GRAU, T("Empfohlen: PS5 neu starten. Danach deinen Jailbreak wie gewohnt starten.",
                                               "Recommended: restart the PS5. Then start your jailbreak as usual."));
        for (int i = 0; i < 2; i++) {   /* rechts ✕ Neu starten (vorgewählt, hell umrandet), links daneben ○ Beenden */
            int bw = textbreite(1, 24, kn[i]) + 70; kx -= bw;
            karte(kx, ky, bw, 70, i == 0 ? 0xFFFFFF : F_RAND, i == 0 ? F_BLAU : F_KARTE2);
            text(kx + 35, ky + 45, 1, 24, 0xFFFFFF, kn[i]);
            kx -= 20;
        }
        text(x + 40, ky + 45, 0, 20, F_GRAU, T("✕ = Neu starten  ·  ○ = Beenden  ·  Options: Protokoll", "✕ = restart  ·  ○ = quit  ·  Options: log"));
        if (K->simulation) text(x + 40, ky - 14, 0, 18, F_GELB, T("SIMULATION: Der Neustart wird nur angezeigt, nicht ausgeführt.", "SIMULATION: the restart is only shown, not carried out."));
        pthread_mutex_unlock(&G.m);
        return;
    }
    text(x + 40, y + 60, 1, 30, F_TEXT, G.titel);
    rechteck(x + 2, y + 84, w - 4, 2, F_RAND);
    /* Text mit Bildlauf. Bei der Texteingabe steht die Aufforderung (letzter Absatz, z.B.
     * "Zum Bestätigen LÖSCHEN eintippen ...") immer sichtbar direkt über dem Feld. */
    const char *haupt = G.text, *aufforderung = NULL;
    if (art == D_TEXT) { const char *l = NULL, *q = G.text; while ((q = strstr(q, "\n\n"))) { l = q; q += 2; } if (l) { haupt = a_kopie_n(fa, G.text, (size_t)(l - G.text)); aufforderung = l + 2; } }
    zeilen za = umbrechen(1, 24, aufforderung ? aufforderung : "", w - 80);
    int ty = y + 130, th = h - 260 - (art == D_TEXT ? 100 + (aufforderung ? za.n * 31 + 10 : 0) : 0);
    zeilen z = umbrechen(0, 24, haupt, w - 80);
    int zh = 31, max = th / zh;
    if (dlg_scroll > z.n - max) dlg_scroll = z.n - max;
    if (dlg_scroll < 0) dlg_scroll = 0;
    for (int i = dlg_scroll; i < z.n && i < dlg_scroll + max; i++) {
        const char *s = z.z[i];
        uint32_t fb = (strstr(s, "!!!") || strstr(s, "SIMULATION")) ? (strstr(s, "SIMULATION") ? F_GELB : F_ROT_TEXT) : F_TEXT;   /* 1.0.6: Warnungen rot (da_Chris86 28.10.2026) */
        text(x + 40, ty + (i - dlg_scroll) * zh, 0, 24, fb, s);
    }
    if (z.n > max) {
        int bh = th * max / z.n, by = ty - 24 + (th - bh) * dlg_scroll / (z.n - max);
        rechteck(x + w - 16, by, 6, bh, F_GRAU);
        const char *wl = T("↑↓ weiterlesen", "↑↓ read more");
        text(x + w - 40 - textbreite(0, 19, wl), y + 60, 0, 19, F_GRAU, wl);
    }
    int ky = y + h - 100;
    if (art == D_WAHL) {
        int bs = textbreite(1, 24, G.sicher) + 80, bg = textbreite(1, 24, G.gef) + 80;
        int xs = x + w - 40 - bs, xg = xs - 24 - bg;
        karte(xg, ky, bg, 70, dlg_fokus == 1 ? 0xFFFFFF : F_ROT_RAND, F_ROT);
        text(xg + 40, ky + 45, 1, 24, 0xFFFFFF, G.gef);
        karte(xs, ky, bs, 70, dlg_fokus == 0 ? 0xFFFFFF : F_BLAU_HELL, F_BLAU);
        text(xs + 40, ky + 45, 1, 24, 0xFFFFFF, G.sicher);
        text(x + 40, ky + 45, 0, 19, F_GRAU, T("←→ Knopf wählen  ·  ✕ drücken  ·  ○ = sicherer Knopf  ·  Options: Protokoll", "←→ choose button  ·  ✕ press  ·  ○ = safe button  ·  Options: log"));
    } else if (art == D_TEXT) {
        int fy = ky - 100;
        for (int i = 0; i < za.n; i++) text(x + 40, fy - 20 - (za.n - 1 - i) * 31, 1, 24, F_GELB, za.z[i]);
        karte(x + 40, fy, w - 80, 70, dlg_fokus == 0 ? 0xFFFFFF : F_RAND, F_HG);
        text(x + 64, fy + 46, 1, 30, G.eingabe[0] ? F_GELB : F_GRAU, G.eingabe[0] ? G.eingabe : (tastatur_offen ? T("Tastatur ist offen ...", "Keyboard is open ...") : T("✕ = Tastatur öffnen und eintippen", "✕ = open keyboard and type")));
        const char *ab = T("Abbrechen", "Cancel");
        int ba = textbreite(1, 24, ab) + 80, bo = textbreite(1, 24, "OK") + 100;
        int xo = x + w - 40 - bo, xa = xo - 24 - ba;
        karte(xa, ky, ba, 70, dlg_fokus == 1 ? 0xFFFFFF : F_RAND, F_KARTE2); text(xa + 40, ky + 45, 1, 24, 0xFFFFFF, ab);
        karte(xo, ky, bo, 70, dlg_fokus == 2 ? 0xFFFFFF : F_BLAU_HELL, F_BLAU); text(xo + 50, ky + 45, 1, 24, 0xFFFFFF, "OK");
        text(x + 40, ky + 45, 0, 19, F_GRAU, T("↑↓ Feld / Knöpfe  ·  ←→ Knopf  ·  ○ = Abbrechen", "↑↓ field / buttons  ·  ←→ button  ·  ○ = Cancel"));
    } else {
        karte(x + w - 240, ky, 200, 70, 0xFFFFFF, F_BLAU); text(x + w - 240 + 80, ky + 45, 1, 24, 0xFFFFFF, "✕ OK");
    }
    pthread_mutex_unlock(&G.m);
}

static char prot_meldung[300];   /* NEU 1.0.7: Ergebnis von □ (auf den USB-Stick) */
static void schirm_protokoll(void)
{
    abdunkeln();
    karte(60, 40, BREITE - 120, HOEHE - 80, F_BLAU, F_KARTE);
    text(100, 100, 1, 30, F_TEXT, T("Protokoll", "Log"));
    {   /* NEU 1.0.7: wo das Protokoll liegt */
        pthread_mutex_lock(&G.m);
        const char *wo = prot_stick_f ? VERB(fa, T("Gespeichert auf dem USB-Laufwerk: ", "Saved on the USB drive: "), prot_stick_pfad)
                       : prot_f ? VERB(fa, T("Gespeichert auf der PS5: ", "Saved on the PS5: "), prot_intern_pfad, T("  ·  □ = zusätzlich auf das USB-Laufwerk", "  ·  □ = also save to the USB drive"))
#ifdef KERN_LOESCHEN
                       : T("Nur hier in der Ansicht  ·  □ = auf das USB-Laufwerk speichern", "Only in this view  ·  □ = save to the USB drive");
#else
                       : T("Nur hier in der Ansicht - die Testfassung speichert nichts", "Only in this view - the test version saves nothing");   /* Prüfung p8, Befund 4 */
#endif
        pthread_mutex_unlock(&G.m);
        text(330, 100, 0, 20, prot_meldung[0] ? F_GELB : F_GRAU, prot_meldung[0] ? prot_meldung : wo);
    }
    pthread_mutex_lock(&G.m);
    sliste l = {0}; for (int i = 0; i < G.log_n; i++) sl_add(fa, &l, a_kopie(fa, G.log[i]));
    pthread_mutex_unlock(&G.m);
    /* NEU 1.0.7 (da_Chris86 03.11.2026): rechts die freiwillige Rückmeldung (QR-Code) - das Protokoll rückt dafür zusammen */
    const int qr_w = 540;
    int lb = rueckmeldung_kasten(BREITE - 100 - qr_w, 140, qr_w, HOEHE - 140 - 110) > 0 ? BREITE - 220 - qr_w - 30 : BREITE - 220;
    zeilen z = umbrechen(0, 20, sl_join(fa, &l, "\n"), lb);
    int max = (HOEHE - 260) / 25;
    if (prot_scroll > z.n - max) prot_scroll = z.n - max;
    if (prot_scroll < 0) prot_scroll = 0;
    for (int i = prot_scroll; i < z.n && i < prot_scroll + max; i++) text(100, 150 + (i - prot_scroll) * 25, 0, 20, F_TEXT, z.z[i]);
#ifdef KERN_LOESCHEN
    text(100, HOEHE - 70, 0, 20, F_GRAU, T("↑↓ blättern  ·  L1/R1 seitenweise  ·  □ auf USB-Laufwerk speichern  ·  ○ oder Options: schließen", "↑↓ scroll  ·  L1/R1 page  ·  □ save to USB drive  ·  ○ or Options: close"));
#else
    text(100, HOEHE - 70, 0, 20, F_GRAU, T("↑↓ blättern  ·  L1/R1 seitenweise  ·  ○ oder Options: schließen", "↑↓ scroll  ·  L1/R1 page  ·  ○ or Options: close"));
#endif
}

/* NEU 1.0.7 (da_Chris86 02.11.2026 15:44-15:56): keine Datenbank-Sicherung und kein USB-Stick als Bedingung mehr ("Wir speichern
 * auf dem USB-Stick nur das Protokoll. Fertig."). Direkt vor dem ersten Löschen beginnt nur das Protokoll als Datei: mit USB-Stick
 * dort, sonst im Ordner /data/PS5-Cleanup-Protokolle (da_Chris86 16:57: jeder Lauf eine eigene Datei mit Datum, bleibt nach der
 * Selbstentfernung). Klappt beides nicht,
 * wird trotzdem gelöscht - das Protokoll steht dann in der Ansicht (Options). Läuft im Arbeitsfaden. Test/Simulation schreiben nichts. */
static bool protokoll_beginnen(void *c);
#ifdef KERN_LOESCHEN
/* NEU 1.0.10 (da_Chris86 14.11.2026 08:25, pbc-engine2.elf legte /data/pbc-backup-center nach jedem Löschen neu an): vor dem Löschen die
 * nachgeladenen Programme im Hintergrund beenden - jedes steht mit Namen im Protokoll. */
static int programme_schliessen(void)
{
    static char b[8192];
    plattform_schliessen_auch(kern_laufende_gewaehlt(K));
    int n = plattform_fremde_schliessen(b, sizeof b);
    if (n == -2) return 0;                                  /* hier nicht vorhanden (PC) */
    ui_log(NULL, T("Schließe Programme im Hintergrund (sonst legen sie ihre Ordner sofort wieder an) ...", "Closing background programs (otherwise they recreate their folders right away) ..."));
    if (n == -1) { ui_log(NULL, T("  Laufende Programme nicht feststellbar - es wurde keines geschlossen.", "  Running programs could not be determined - none was closed.")); return 0; }
    if (!b[0]) { ui_log(NULL, T("  Es läuft kein nachgeladenes Programm.", "  No loaded program is running.")); kern_geist_hinterlassen(true); return 0; }
    bool sauber = true;   /* NEU 1.0.10 (14.11.2026 14:28): jedes aufgeführte Programm wirklich beendet - dann gehört eine übrige Einbindung keinem mehr */
    for (char *z = b; *z; ) {
        char *e = strchr(z, '\n'); if (e) *e = 0;
        int zu = -1, fe = 0, ab = 0; char t[200];
        if (sscanf(z, "%d %d %n", &zu, &fe, &ab) >= 2 && ab > 0) {
            const char *nm = z + ab;
            if (zu != 0) sauber = false;
            if (zu == 0) snprintf(t, sizeof t, T("  geschlossen   %s", "  closed   %s"), nm);
            else if (zu == 1) snprintf(t, sizeof t, T("  nicht geschlossen   %s (Fehler %d)", "  not closed   %s (error %d)"), nm, fe);
            else if (zu == 2) snprintf(t, sizeof t, T("  läuft noch   %s - nach einem Neustart der PS5 ist es beendet", "  still running   %s - it is gone after restarting the PS5"), nm);
            else if (zu == 4) snprintf(t, sizeof t, "%s", T("  weitere Programme nicht geprüft (die Liste ist zu lang)", "  further programs not checked (the list is too long)"));
            else if (zu == 5) snprintf(t, sizeof t, T("  nicht beendet   %s (ohne eigenen Namen - PS4Mount läuft noch oder PS5 Cleanup erkennt seinen eigenen Hintergrundteil nicht sicher)", "  not closed   %s (no name of its own - PS4Mount is still running or PS5 Cleanup cannot identify its own background part)"), nm);
            else snprintf(t, sizeof t, T("  nicht angefasst   %s (ohne eigenen Namen - startet vielleicht gerade erst)", "  not touched   %s (no name of its own - may just be starting)"), nm);
            ui_log(NULL, t);
        }
        if (!e) break;
        z = e + 1;
    }
    kern_geist_hinterlassen(sauber);
    return n;
}
#endif
static bool vor_dem_loeschen(void *c)
{
    bool ok = protokoll_beginnen(c);
    neustart_pflicht = ok && !K->simulation && K->KOMPLETT_LAUF;
#ifdef KERN_LOESCHEN
    /* da_Chris86 14.11.2026 11:00: nicht mehr hier, sondern im Löschlauf nach dem Anhalten von ShadowMount und ps4mount (kern_loeschen.c) */
    K->nach_anhalten = (ok && !K->simulation && plattform_fremde_schliessen(NULL, 0) != -2) ? programme_schliessen : NULL;
#endif
    return ok;
}
static bool protokoll_beginnen(void *c)
{
    (void)c;
#ifndef KERN_LOESCHEN
    ui_log(NULL, plattform_stick_ordner() ? T("SIMULATION: Hier begänne das Protokoll auf dem USB-Laufwerk.", "SIMULATION: the log would start on the USB drive here.")
                                          : T("SIMULATION: Kein USB-Laufwerk - das Protokoll käme in den Ordner /data/PS5-Cleanup-Protokolle.", "SIMULATION: no USB drive - the log would go to the folder /data/PS5-Cleanup-Protokolle."));
    return true;
#else
    char wo[220];
    pthread_mutex_lock(&G.m); bool offen = prot_f || prot_stick_f, st_ = prot_stick_f != NULL; snprintf(wo, sizeof wo, "%s", st_ ? prot_stick_pfad : prot_intern_pfad); pthread_mutex_unlock(&G.m);
    if (offen) {                                 /* schon offen (zweites Löschen oder vorher □ in der Ansicht): weiter in dieselbe Datei */
        A.protokoll_datei = VERB(K->a, st_ ? T("auf dem USB-Laufwerk: ", "on the USB drive: ") : !strncmp(wo, "/mnt/ext1/", 10) ? T("auf der M.2: ", "on the M.2: ") : T("auf der PS5: ", "on the PS5: "), wo);   /* Prüfung p8, Befund 6 */
        return true;
    }
    char m[300] = "";
    bool stick = plattform_stick_ordner() != NULL;
    if (stick && protokoll_auf_stick(m, sizeof m)) {
        ui_log(NULL, m);
        A.protokoll_datei = VERB(K->a, T("auf dem USB-Laufwerk: ", "on the USB drive: "), prot_stick_pfad);
        return true;
    }
    if (stick && m[0]) ui_log(NULL, m);          /* Prüfung p8, Befund 5: warum der Stick nicht ging */
    /* da_Chris86 02.11.2026 16:57/17:23: kein Stick, Stick voll oder nicht beschreibbar -> interne Platte (/data/PS5-Cleanup-Protokolle);
     * ist die voll oder nicht beschreibbar -> M.2 (/mnt/ext1/PS5-Cleanup-Protokolle). Jeder Lauf eine eigene Datei (Name wie auf dem
     * Stick). Neu anlegen oder gar nicht (O_EXCL), nie einem Verweis folgen (O_NOFOLLOW), immer hinten anhängen (O_APPEND, Codex). */
    for (int nr = 0; nr < 2; nr++) {
        zugriff_wechsel_sperren();   /* NEU 1.0.9: nie mitten in einem Besitzer-Wechsel anlegen (1.0.10: auch den Ordner nicht) */
        const char *po = plattform_protokoll_ordner(nr);
        if (!po) { zugriff_wechsel_frei(); continue; }
        char pi[200]; time_t j = time(NULL); struct tm tm; localtime_r(&j, &tm);
        snprintf(pi, sizeof pi, "%s/ps5cleanup_protokoll_%04d-%02d-%02d_%02d%02d%02d.txt", po, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        int fd = open(pi, O_WRONLY | O_CREAT | O_EXCL | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0644);
        { int e = errno; zugriff_wechsel_frei(); errno = e; }
        FILE *f = fd >= 0 ? fdopen(fd, "a") : NULL;
        if (!f) { int fe = errno; if (fd >= 0) close(fd); ui_log(NULL, VERB(K->a, T("Protokoll-Datei nicht anlegbar: ", "Log file could not be created: "), pi, " (", strerror(fe), ")")); continue; }
        pthread_mutex_lock(&G.m); prot_datei_kopf(f); prot_f = f; snprintf(prot_intern_pfad, sizeof prot_intern_pfad, "%s", pi); pthread_mutex_unlock(&G.m);
        if (K) K->PROTOKOLL_JETZT = prot_intern_pfad;   /* NEU 03.11.2026: dieses Protokoll bleibt bei der vollständigen Bereinigung */
        plattform_protokoll_stick_setzen(pi);   /* der Hintergrundteil hängt bei der Selbstentfernung seine Zeilen an */
        A.protokoll_datei = VERB(K->a, nr == 0 ? T("auf der PS5: ", "on the PS5: ") : T("auf der M.2: ", "on the M.2: "), pi);
        ui_log(NULL, VERB(K->a, nr == 1 ? T("Interne Platte voll oder nicht beschreibbar - das Protokoll wird auf der M.2 gespeichert: ", "Internal storage full or not writable - the log is saved on the M.2: ")
                              : stick ? T("USB-Laufwerk voll oder nicht beschreibbar - das Protokoll wird auf der PS5 gespeichert: ", "USB drive full or not writable - the log is saved on the PS5: ")
                                      : T("Kein USB-Laufwerk - das Protokoll wird auf der PS5 gespeichert: ", "No USB drive - the log is saved on the PS5: "), pi));
        return true;
    }
    ui_log(NULL, T("Das Protokoll ließ sich nirgends als Datei speichern (USB-Laufwerk, interne Platte, M.2) - es steht in der Ansicht (Options).",
                   "The log could not be saved as a file anywhere (USB drive, internal storage, M.2) - it is in the log view (Options)."));
    return true;
#endif
}
/* NEU 1.0.7 (da_Chris86 31.10.2026 19:28 / 02.11.2026): kurz vor der Selbstentfernung - wo das Protokoll bleibt (Stick oder /data).
 * Es wurde schon vor dem ersten Löschen begonnen; der Hintergrundteil hängt seine Schlusszeilen an. Läuft im Arbeitsfaden. */
static void vor_selbstweg_app(void *c)
{
    (void)c;
    if (K->simulation) {
        ui_log(NULL, plattform_stick_ordner() ? T("SIMULATION: Das Protokoll bliebe auf dem USB-Laufwerk.", "SIMULATION: the log would stay on the USB drive.")
                                              : T("SIMULATION: Das Protokoll bliebe im Ordner /data/PS5-Cleanup-Protokolle.", "SIMULATION: the log would stay in the folder /data/PS5-Cleanup-Protokolle."));
        return;
    }
    pthread_mutex_lock(&G.m);
    const char *wo = prot_stick_f ? VERB(K->a, prot_stick_pfad) : prot_f ? VERB(K->a, prot_intern_pfad) : NULL;
    pthread_mutex_unlock(&G.m);
    ui_log(NULL, wo ? VERB(K->a, T("Das Protokoll bleibt erhalten: ", "The log is kept: "), wo)
                    : T("Das Protokoll ließ sich nicht als Datei speichern - es steht nur in der Ansicht.", "The log could not be saved as a file - it is only in the log view."));
}

/* =================================================================== NEU 1.0.9: Bedienung im Browser (fern.c) - am Fernseher
 * Neu am 08.11.2026 (da_Chris86 16:09-16:12: eigene Seite mit Maus, Tastatur und Touch, kein Fernsehbild, keine Freigabe, "Immer gleich an"):
 * die Browser-Bedienung läuft ab dem Start; die Adresse steht unten auf der Startseite, im Options-Menü zeigt ✕ sie groß mit QR-Code und
 * darunter, ob vom Browser schon etwas angekommen ist (da_Chris86 15:48). Bedient jemand im Browser, steht oben "IM BROWSER". */
static bool fern_info_offen = false;
static void fern_log(const char *t) { ui_log(NULL, t); }
static void fern_info_zeigen(void)
{
    if (fern_an()) { optionen_offen = false; fern_info_offen = true; neu_zeichnen(); return; }
    kurz(T("Die Browser-Bedienung ist nicht verfügbar - der Grund steht im Protokoll.", "Browser control is not available - the reason is in the log."));
}
static void fern_karte(int w, int h, int *x, int *y) { abdunkeln(); *x = (BREITE - w) / 2; *y = (HOEHE - h) / 2; karte_rund(*x, *y, w, h, 22, 1, N_RAND, N_KARTE); }
static void schirm_fern_info(void)
{
    char a[64]; fern_adresse(a, sizeof a);
    char st[300]; int sk = fern_kontakt(st, sizeof st);   /* (da_Chris86 08.11.2026 15:48): Port offen - kommt vom Browser etwas an? */
    int w = 1280, h = 600, x, y; fern_karte(w, h, &x, &y);
    text(x + 44, y + 72, 1, 34, N_TEXT, T("Bedienung im Browser", "Control in a browser"));
    int n = qr_kodieren(a), m = n > 0 ? 360 / (n + 8) : 0, qs = n > 0 && m >= 3 ? m * (n + 8) : 0;
    if (qs) qr_malen(x + 44, y + 112, n, m);
    int tx = x + 44 + (qs ? qs + 44 : 0), tw = x + w - 44 - tx, yy = y + 146;
    text(tx, yy, 0, 26, N_GRAU, T("Im Browser am Handy oder PC öffnen:", "Open in a browser on your phone or PC:"));
    text_passend(tx, yy + 62, 1, 44, 28, N_FOKUS, a, tw);
    yy += 122;
    if (st[0]) yy += absatz(tx, yy, 0, 24, sk == 1 ? N_GRUEN : sk == 2 ? N_GELB : N_GRAU, st, tw, 34) + 18;
    yy += absatz(tx, yy, 0, 24, N_TEXT, T("Einfach öffnen - ohne Anmeldung. Bedienen mit Maus, Tastatur oder Finger. Am Handy geht es auch über den QR-Code.",
                                          "Just open it - no sign-in. Use mouse, keyboard or touch. On a phone you can also scan the QR code."), tw, 34) + 14;
    absatz(tx, yy, 0, 24, N_GRAU, T("Gelöscht wird nur nach denselben Fragen wie am Fernseher. Die Seite gibt es nur im Heimnetz und nur, solange PS5 Cleanup läuft.",
                                    "Deleting needs the same confirmations as on the TV. The page is only available in the home network and only while PS5 Cleanup is running."), tw, 34);
    int kx = x + 44, ky = y + h - 42;
    knopf_glyph(KN_O, kx, ky, N_GRAU2, 0xE6EAF0); kx += knopf_breite(KN_O) + 12; text(kx, ky + 9, 0, 24, N_GRAU, T("schließen", "close"));
}

static void zeichnen(void)
{
    if (fa) arena_frei(fa);
    fa = arena_neu();
    rechteck(0, 0, BREITE, HOEHE, F_HG);
    lauf_aktiv = false;
    if (G.beschaeftigt && A.loescht && !loeschseite) loeschseite_beginnen();   /* NEU 1.0.8 (4.5): vom ersten Löschen bis zum Ende der Arbeit */
    if (!G.beschaeftigt) loeschseite = false;
    if (G.art == D_FERTIG && fertig_neu()) schirm_fertig();   /* NEU 1.0.8 (4.5): eigene Seite statt Fenster */
    else if (bestaetigen_aktiv()) schirm_bestaetigen();        /* NEU 1.0.8 (4.4) */
    else if (G.art == D_WAHL || G.art == D_FEHLER) schirm_frage();   /* NEU 1.0.8 (4.5, K5): Fragen und Fehler als eigene Seite */
    else {   /* (die Seiten oben zeichnen alles selbst - der Hintergrund darunter entfällt) */
        if (G.beschaeftigt) { if (loeschseite) schirm_loeschen(); else schirm_arbeit(); }
        else if (GESUCHT && AUSWAHL_OFFEN && ablauf_verbunden(&A)) schirm_liste();
        else schirm_start();
        if (G.art != D_KEINER) schirm_dialog();   /* altes Fenster nur noch: Texteingabe außer der vollständigen Bereinigung, Fertig im Vergleichsmodus 1.0.6 */
    }
    if (protokoll_offen) schirm_protokoll();   /* NEU 1.0.7: auch über einer Frage / dem Abschluss (Options) */
    if (fern_info_offen && !fern_an()) fern_info_offen = false;   /* NEU 1.0.9: Adresse der Browser-Bedienung mit QR-Code */
    if (fern_info_offen) schirm_fern_info();
}

/* =================================================================== Tasten */
enum { T_X, T_KREIS, T_QUADRAT, T_DREIECK, T_HOCH, T_RUNTER, T_LINKS, T_RECHTS, T_L1, T_R1, T_OPTIONS, T_ANDERE };
static void tastatur_auf(void)
{
    if (tastatur_offen) return;
    pthread_mutex_lock(&G.m); G.eingabe[0] = 0; pthread_mutex_unlock(&G.m);
    tastatur_offen = true; tastatur_gesehen = false; tastatur_seit = SDL_GetTicks();
    SDL_StartTextInput();
}
static void taste_dialog(int t)
{
    dialogart art = G.art;
    if (G.dlg_nr != dlg_gesehen || SDL_GetTicks() - dlg_seit < 400) return;   /* Frage gerade erst erschienen */
    if (art == D_FERTIG) {   /* NEU 1.0.6 */
        /* da_Chris86 02.11.2026: ✕ = Neu starten, ○ = Beenden */
        if (G.beantwortet) return;   /* Codex-Prüfung 208: schon beantwortet - die erste Wahl gilt */
        if (t == T_X) { nach_aktion = 2; A.schluss_ohne_suche = false; antworten(false, false); }
        else if (t == T_KREIS) {
            nach_aktion = 1;
#ifndef VORSCHAU
            A.schluss_ohne_suche = true;   /* NEU 1.0.10 (PS5-Test 13.11.2026): Beenden - nicht erst noch einmal alles nachschauen */
#endif
            antworten(false, false);
        }
        return;
    }
    if (art == D_FEHLER) { if (t == T_HOCH) dlg_scroll -= 3; else if (t == T_RUNTER) dlg_scroll += 3; else if (t == T_X || t == T_KREIS) antworten(false, false); return; }
    if (tippen_aktiv()) {   /* NEU 1.0.8 (4.4): vollständige Bereinigung - links Zurück (1), Mitte Feld (0), rechts Löschen (2) */
        if (t == T_LINKS) dlg_fokus = dlg_fokus == 2 ? 0 : 1;
        else if (t == T_RECHTS) dlg_fokus = dlg_fokus == 1 ? 0 : 2;
        else if (t == T_KREIS) antworten(false, false);
        else if (t == T_X) {
            if (dlg_fokus == 0) tastatur_auf();
            else if (dlg_fokus == 1) antworten(false, false);
            else if (eingabe_passt()) antworten(false, true);
            else { kurz(G.eingabe[0] ? T("Bitte genau LÖSCHEN eintippen, in Großbuchstaben.", "Please type DELETE exactly, in capital letters.")
                                      : T("Erst LÖSCHEN eintippen: ✕ auf dem Feld öffnet die Tastatur.", "Type DELETE first: ✕ on the field opens the keyboard.")); hinweis_dauer(5000); }
        }
        return;
    }
    if (bestaetigen_aktiv()) {   /* NEU 1.0.8 (4.4): links "Zurück zur Auswahl", rechts "Löschen" (✕ halten - siehe halten_start) */
        if (t == T_LINKS) { dlg_fokus = 0; x_seit = 0; }
        else if (t == T_RECHTS) dlg_fokus = 1;
        else if (t == T_KREIS) antworten(false, false);
        else if (t == T_X) { if (dlg_fokus == 0) antworten(false, false); else kurz(T("Zum Löschen ✕ gedrückt halten, bis der Kreis voll ist.", "To delete, hold ✕ until the circle is full.")); }
        return;
    }
    if (t == T_HOCH && (art == D_WAHL || dlg_fokus == 0)) { dlg_scroll -= 3; return; }
    if (t == T_RUNTER && art == D_WAHL) { dlg_scroll += 3; return; }
    if (art == D_WAHL) {
        if (t == T_LINKS) dlg_fokus = 0; else if (t == T_RECHTS) dlg_fokus = 1;   /* 1.0.8 (K5): links der sichere Knopf (wie auf der Seite "Bestätigen") */
        else if (t == T_KREIS) antworten(false, false);
        else if (t == T_X) antworten(dlg_fokus == 1, false);
        return;
    }
    /* Texteingabe */
    if (t == T_RUNTER && dlg_fokus == 0) dlg_fokus = 2;
    else if (t == T_RUNTER) dlg_scroll += 3;
    else if (t == T_HOCH) dlg_fokus = 0;
    else if (t == T_LINKS && dlg_fokus == 2) dlg_fokus = 1;
    else if (t == T_RECHTS && dlg_fokus == 1) dlg_fokus = 2;
    else if (t == T_KREIS) antworten(false, false);
    else if (t == T_X) {
        if (dlg_fokus == 0) tastatur_auf();
        else antworten(false, dlg_fokus == 2);
    }
}
static void taste_liste(int t)
{
    sicht_neu();
    int k = sicht[fokus_bereich];
    al_ordnen(k);
    if (optionen_offen) {   /* NEU 1.0.8 (4.3): Options-Menü der Auswahl - Reihenfolge und Protokoll */
        switch (t) {
        case T_HOCH: if (opt_fokus > 0) opt_fokus--; break;
        case T_RUNTER: if (opt_fokus < 4) opt_fokus++; break;
        case T_X:
            if (opt_fokus == 0) { sortierung = sortierung == SORT_AZ ? SORT_GROESSTE : SORT_AZ; al_py = 0; }
            else if (opt_fokus == 1) ordner_zeigen = !ordner_zeigen;
            else if (opt_fokus == 2) testmodus_aus_menue();
            else if (opt_fokus == 3) { optionen_offen = false; protokoll_offen = true; prot_scroll = 1 << 20; }
            else fern_info_zeigen();   /* NEU 1.0.9 */
            break;
        case T_KREIS: case T_OPTIONS: optionen_offen = false; break;
        default: break;
        }
        return;
    }
    switch (t) {
    case T_HOCH: if (fokus_zone == 0) { if (fokus_bereich > 0) fokus_bereich--; } else al_schritt(-1); break;
    case T_RUNTER: if (fokus_zone == 0) { if (fokus_bereich < sicht_n - 1) fokus_bereich++; } else al_schritt(1); break;
    case T_L1: case T_R1:   /* seitenweise: sechs Blöcke weiter */
        if (fokus_zone == 1 && AL.n > 0) { al_b += t == T_R1 ? 6 : -6; if (al_b < 0) al_b = 0; if (al_b > AL.n - 1) al_b = AL.n - 1; al_t = -1; }
        break;
    case T_RECHTS: if (AL.n > 0) fokus_zone = 1; break;
    case T_LINKS: case T_KREIS: fokus_zone = 0; break;
    case T_X:
        if (fokus_zone == 0) {
            int n = bereich_anzahl(k);
            if (n > 0 && ablauf_bereich_nur_einzeln(&A, k) && ablauf_bereich_zustand(&A, k) == 0)   /* NEU 1.1 */
                kurz(T("Hier nur einzeln wählbar: mit → in die Liste gehen und die Einträge einzeln anhaken.", "Individually only here: press → to enter the list and tick entries one by one."));
            else if (n > 0) ablauf_bereich_setzen(&A, k, ablauf_bereich_zustand(&A, k) != 2);
        }
        else if (AL.n > 0) {
            if (al_t >= 0 && al_mehr(al_b)) eintrag_umschalten(AL.bl[al_b].teile.e[al_t]);
            else al_block_setzen(al_b);
        }
        break;
#ifdef NUR_LESEN
    case T_QUADRAT: case T_DREIECK: kurz(T("In dieser Testversion gesperrt - es wird nur nachgeschaut.", "Locked in this test version - only looking.")); break;
#else
    case T_QUADRAT: case T_DREIECK:
        if (testmodus) { kurz(testmodus_hinweis()); hinweis_dauer(6000); break; }   /* NEU 1.0.8 (3.6) */
        if (t == T_QUADRAT) { if (gewaehlt_anzahl() > 0) { dlg_fokus = 0; dlg_scroll = 0; starte(AUF_AUSWAHL); } else kurz(T("Erst etwas auswählen (✕).", "Select something first (✕).")); }
        else if (K->Funde.n > 0) { dlg_fokus = 0; dlg_scroll = 0; starte(AUF_KOMPLETT); }
        break;
#endif
    case T_OPTIONS: optionen_offen = true; opt_fokus = 0; break;
    default: break;
    }
}
static void taste(int t)
{
    if (x_ruhe_bis) {   /* NEU 1.0.8: siehe testmodus_aus_menue */
        if (t == T_X && (Sint32)(SDL_GetTicks() - x_ruhe_bis) < 0) return;
        x_ruhe_bis = 0;   /* Codex-Prüfung 102 (P3): jede andere Taste hebt die Sperre auf (z. B. □ öffnet gleich danach die Bestätigungsfrage) */
    }
    neu_zeichnen();
#ifdef MITSCHNITT
    ms_taste();                                    /* NEU 1.0.8 (2.7): nach dem Tastendruck ein Bild */
#endif
    if (protokoll_offen) {
        if (t == T_KREIS || t == T_OPTIONS) protokoll_offen = false;
        else if (t == T_HOCH) prot_scroll -= 3; else if (t == T_RUNTER) prot_scroll += 3;
        else if (t == T_L1) prot_scroll -= 30; else if (t == T_R1) prot_scroll += 30;
        else if (t == T_QUADRAT) protokoll_auf_stick(prot_meldung, sizeof prot_meldung);   /* NEU 1.0.7: Ergebnis steht in der Ansicht */
        return;
    }
    if (G.art != D_KEINER) {
        if (t == T_OPTIONS) { protokoll_offen = true; prot_scroll = 1 << 20; return; }   /* NEU 1.0.7: Protokoll auch hier */
        taste_dialog(t); return;
    }
    if (G.beschaeftigt) {
        if (t == T_KREIS && A.abbrechbar) { K->abbruch = true; ui_stand(NULL, T("Breche ab ...", "Cancelling ..."), ABLAUF_BALKEN_GLEICH); }
        else if (t == T_KREIS && A.loescht && A.anhaltbar && !anhalten_gewuenscht) kurz(T("Zum Anhalten ○ eine Sekunde gedrückt halten.", "To stop, hold ○ for one second."));   /* NEU 1.0.8 (4.5) */
        else if (t == T_OPTIONS) { protokoll_offen = true; prot_scroll = 1 << 20; }
        return;
    }
    bool fertig = GESUCHT && ablauf_verbunden(&A);
    if (fertig && AUSWAHL_OFFEN) { taste_liste(t); return; }
    if (optionen_offen) {   /* NEU 1.0.8 (3.6): Options-Menü der Startseite */
        if (t == T_HOCH && opt_fokus > 0) opt_fokus--;
        else if (t == T_RUNTER && opt_fokus < 2) opt_fokus++;
        else if (t == T_X) { if (opt_fokus == 0) testmodus_aus_menue(); else if (opt_fokus == 1) { optionen_offen = false; protokoll_offen = true; prot_scroll = 1 << 20; } else fern_info_zeigen(); }
        else if (t == T_KREIS || t == T_OPTIONS) optionen_offen = false;
        return;
    }
    /* Start / Übersicht */
    if (t == T_X) { if (fertig) { AUSWAHL_OFFEN = true; fokus_zone = 0; fokus_bereich = 0; AL.k = -1; al_b = 0; al_t = -1; al_py = 0; optionen_offen = false; } else starte(AUF_SUCHEN); }
    else if (t == T_DREIECK && fertig) starte(AUF_SUCHEN);
    else if (t == T_QUADRAT && !GESUCHT) { SP_EN = !SP_EN; sim_hinweis_setzen(); plattform_absturzmelder(SP_EN ? "PS5 Cleanup " VERSION_TEXT_EN : "PS5 Cleanup " VERSION_TEXT); }   /* Codex-Prüfung 70: auch der Absturzmelder */   /* Sprache nur vor der ersten Suche (die Texte der Liste entstehen beim Suchen) */
    else if (t == T_OPTIONS) { optionen_offen = true; opt_fokus = 0; }   /* NEU 1.0.8 (3.6): Menü mit Testmodus und Protokoll */
}
static void kreis_gehalten(void)
{
    /* ○ eine Sekunde halten = Beenden - nicht während einer Arbeit oder einer Frage */
    if (G.art == D_FERTIG) { taste_dialog(T_KREIS); return; }   /* Prüfung p8, Befund 10: am Ende wie ○ = Beenden */
    if (A.loescht && A.anhaltbar && G.art == D_KEINER) {   /* NEU 1.0.8 (4.5, Arbeitsplan 3.4): ○ halten beim Löschen = nach diesem Eintrag anhalten */
        if (!anhalten_gewuenscht && ablauf_loeschen_anhalten(&A)) { anhalten_gewuenscht = true; kurz(T("Wird nach diesem Eintrag angehalten.", "Stopping after this entry.")); }
        return;
    }
    if (G.beschaeftigt || A.loescht) { kurz(T("Bitte warten, bis die laufende Arbeit fertig ist.", "Please wait until the current task has finished.")); return; }
    if (G.art != D_KEINER) return;
    laeuft = false;
}
/* =================================================================== NEU 1.0.9: Bedienung im Browser - Daten und Befehle
 * (da_Chris86 08.11.2026 16:09-16:12: "mit Maus und Tastatur ... am Handy mit Touch", "Wieso wird es gestreamt? Das ist doch Quatsch",
 * keine Freigabe, immer an.) Der Browser bekommt den Zustand der App und die Liste als JSON (fern.c), seine Seite (fern_seite.html) baut
 * sich daraus selbst auf. Seine Befehle führt der Hauptfaden aus wie einen Tastendruck am Controller - mit denselben Prüfungen: Testmodus,
 * gesperrte Einträge, "nur einzeln", die Frage muss die gerade gestellte sein (Nummer), "Löschen" erst nach 1,5 s Halten (auch hier
 * gezählt: frühestens 1,5 s nachdem die Frage erschienen ist), bei der vollständigen Bereinigung genau das Wort LÖSCHEN.
 * Am Fernseher läuft alles mit: was im Browser passiert, sieht man dort (dieselbe App, derselbe Zustand). */
static bool tv_vorrang(int t)   /* Tasten am Controller zuerst hierher: die Info-Seite der Browser-Bedienung schließen */
{
    if (fern_info_offen) { if (t == T_KREIS || t == T_X || t == T_OPTIONS) fern_info_offen = false; neu_zeichnen(); return true; }
    return false;
}
static int fd_liste_v = 0, fd_info_i = -1, fd_info_fg = -1;
static bool fd_liste_ok(void) { return GESUCHT && ablauf_verbunden(&A) && !G.beschaeftigt; }
static void fj_s(cJSON *o, const char *k, const char *v) { cJSON_AddStringToObject(o, k, v ? v : ""); }
static void fj_n(cJSON *o, const char *k, double v) { cJSON_AddNumberToObject(o, k, v); }
static void fj_b(cJSON *o, const char *k, bool v) { cJSON_AddBoolToObject(o, k, v); }
static const char *ampel_t(int a) { return a == AMPEL_ROT ? "r" : a == AMPEL_GELB ? "y" : "g"; }
/* Zeiger -> Nummer in K->Funde (offene Adressierung, einmal je Liste) */
typedef struct { const eintrag **k; int *v; int cap; } fd_nr_t;
static void fd_nr_bauen(fd_nr_t *t, arena *a)
{
    t->cap = 16; while (t->cap < K->Funde.n * 2 + 2) t->cap *= 2;
    t->k = a_hole(a, sizeof(eintrag *) * (size_t)t->cap); t->v = a_hole(a, sizeof(int) * (size_t)t->cap);
    memset(t->k, 0, sizeof(eintrag *) * (size_t)t->cap);
    for (int i = 0; i < K->Funde.n; i++) {
        size_t h = ((uintptr_t)K->Funde.e[i] >> 4) & (size_t)(t->cap - 1);
        while (t->k[h]) h = (h + 1) & (size_t)(t->cap - 1);
        t->k[h] = K->Funde.e[i]; t->v[h] = i;
    }
}
static int fd_nr(const fd_nr_t *t, const eintrag *f)
{
    if (!f || f == &RESTE_ZEILE) return -1;
    size_t h = ((uintptr_t)f >> 4) & (size_t)(t->cap - 1);
    while (t->k[h]) { if (t->k[h] == f) return t->v[h]; h = (h + 1) & (size_t)(t->cap - 1); }
    return -1;
}
/* die Liste: alle Einträge (Nummer = Stelle in K->Funde) und je Bereich die Blöcke wie am Fernseher (größte zuerst, "Reste" unten) */
static char *fd_liste_json(void)
{
    cJSON *o = cJSON_CreateObject();
    fj_n(o, "fg", such_nr); fj_n(o, "lv", fd_liste_v);
    cJSON *e = cJSON_AddArrayToObject(o, "e");
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i]; cJSON *x = cJSON_CreateObject(); const char *g = NULL;
        const char *nm, *ws; zeile_teilen(f, f->Kat, &nm, &ws);
        fj_s(x, "n", nm); if (ws[0]) fj_s(x, "w", ws);
        fj_s(x, "z", ohne_ort_vorne(kern_zeile(fa, f)));
        const char *gr = h_trim(fa, ablauf_eintrag_groesse(&A, f)); if (gr[0]) fj_s(x, "g", gr);
        groesse *gg = kern_eintrag_groesse_in(K, fa, f, true); fj_n(x, "b", gg ? (double)gg->Bytes : -1);
        fj_s(x, "a", ampel_t(kern_ampel(f))); fj_s(x, "s", schild_text(f, f->Kat));
        const char *ok = ort_kurz(anzeige_ort(f)); if (ok[0]) fj_s(x, "o", ok);
        if (ablauf_eintrag_gesperrt(&A, f, &g)) { fj_b(x, "x", true); if (g && g[0]) fj_s(x, "xg", g); }
        if (f->Jailbreak) fj_b(x, "j", true);
        if (nur_einzeln_teil(0, f)) fj_b(x, "e", true);
        const char *bid = bild_kennung(f); if (bid && strlen(bid) < 16) fj_s(x, "i", bid);
        fj_n(x, "k", f->Kat);
        cJSON_AddItemToArray(e, x);
    }
    fd_nr_t nr; fd_nr_bauen(&nr, fa);
    cJSON *br = cJSON_AddArrayToObject(o, "bereiche");
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        int k = ALLE_KAT[ki]; cJSON *b = cJSON_CreateObject();
        fj_n(b, "k", k); fj_s(b, "name", kern_kat_name(k)); fj_s(b, "info", kern_kat_info(k)); fj_b(b, "einzeln", ablauf_bereich_nur_einzeln(&A, k));
        cJSON *bl = cJSON_AddArrayToObject(b, "bl");
        kblock *x = NULL; int nb = bloecke_holen(fa, k, SORT_GROESSTE, &x);
        for (int i = 0; i < nb; i++) {
            cJSON *y = cJSON_CreateObject(); bool reste = x[i].kopf == &RESTE_ZEILE;
            fj_n(y, "kopf", reste ? -1 : fd_nr(&nr, x[i].kopf));
            fj_s(y, "name", x[i].name ? x[i].name : "");
            fj_s(y, "g", x[i].groesse_da ? groesse_von(fa, x[i].bytes, x[i].gekappt) : "…");
            fj_n(y, "b", x[i].groesse_da ? (double)x[i].bytes : -1);
            if (x[i].ohne_kachel) fj_b(y, "ok", true);
            if (x[i].gruppe) fj_n(y, "gr", x[i].gruppe);   /* NEU 1.0.10: einzelne Programmdatei - bleibt auch bei A–Z unten */
            if (reste) { fj_s(y, "s", T("nichts geht verloren", "nothing is lost")); fj_s(y, "a", "g"); }
            else {
                fj_s(y, "s", schild_text(x[i].kopf, k)); fj_s(y, "a", ampel_t(kern_ampel(x[i].kopf)));
                const char *bid = bild_kennung(x[i].kopf); if (bid && strlen(bid) < 16) fj_s(y, "i", bid);
                const char *da = kern_art_info(fa, x[i].kopf); if (!da[0]) da = kern_folge(x[i].kopf);
                fj_s(y, "da", da);
            }
            cJSON *t = cJSON_AddArrayToObject(y, "t");
            for (int j = 0; j < x[i].teile.n; j++) {
                int n = fd_nr(&nr, x[i].teile.e[j]); if (n < 0) continue;
                cJSON *u = cJSON_CreateObject(); fj_n(u, "e", n);
                fj_s(u, "n", (!reste && x[i].teile.e[j] == x[i].kopf && kopf_kurz(x[i].teile.e[j], x[i].name)) ? kopf_kurz(x[i].teile.e[j], x[i].name)
                           : reste ? ohne_ort_vorne(kern_zeile(fa, x[i].teile.e[j])) : ohne_jb_start(nach_name(ohne_ort_vorne(kern_zeile(fa, x[i].teile.e[j])), x[i].name)
                                                                                                        ? nach_name(ohne_ort_vorne(kern_zeile(fa, x[i].teile.e[j])), x[i].name)
                                                                                                        : ohne_ort_vorne(kern_zeile(fa, x[i].teile.e[j])), k));
                cJSON_AddItemToArray(t, u);
            }
            cJSON_AddItemToArray(bl, y);
        }
        cJSON_AddItemToArray(br, b);
    }
    char *s = cJSON_PrintUnformatted(o); cJSON_Delete(o);
    return s;
}
/* der Kasten zu einem Eintrag (wie rechts in der Auswahl am Fernseher) */
static void fd_info(cJSON *o, eintrag *f)
{
    cJSON *x = cJSON_AddObjectToObject(o, "info"); const char *grund = NULL;
    int k = f->Kat; bool gesp = ablauf_eintrag_gesperrt(&A, f, &grund);
    const char *nm, *ws; zeile_teilen(f, k, &nm, &ws);
    const char *gr = h_trim(fa, ablauf_eintrag_groesse(&A, f)); int o2 = anzeige_ort(f);
    fj_s(x, "titel", nm); fj_s(x, "unter", VERB(fa, gr, o2 != ORT_INTERN ? VERB(fa, gr[0] ? " · " : "", kern_ort_name(o2)) : ""));
    const char *bid = bild_kennung(f); if (bid && strlen(bid) < 16) fj_s(x, "i", bid);
    cJSON *ab = cJSON_AddArrayToObject(x, "abs");
    #define ABS(fr, tx, fa_) do { cJSON *z_ = cJSON_CreateObject(); if (fr) fj_s(z_, "f", fr); fj_s(z_, "t", tx); fj_s(z_, "c", fa_); cJSON_AddItemToArray(ab, z_); } while (0)
    ABS(T("Was wird gelöscht?", "What is deleted?"), ohne_ort_vorne(kern_zeile(fa, f)), "t");
    int a = kern_ampel(f); const char *da = kern_art_info(fa, f); if (!da[0]) da = kern_folge(f);
    bool nie = gesp && grund && grund[0] && (f->Gesperrt || kern_nur_mit_spiel(K, f));
    if (nie) ABS(T("Warum nicht wählbar?", "Why can't I select it?"), grund, "y");
    else {
        ABS(T("Was passiert danach?", "What happens then?"), da, ampel_t(a));
        if (gesp && grund && grund[0]) ABS(NULL, grund, "y");
        else if (f->Jailbreak) ABS(NULL, kern_jb_hinweis(fa, f), "o");
    }
    ABS(f->Pfade.n == 1 ? T("Ordner", "Folder") : T("Ordner", "Folders"), ordner_text(f), "d");
    #undef ABS
    fj_n(x, "e", fd_info_i);
}
static struct { Uint32 zeit; bool da[4]; long long frei[4], ges[4]; int usb_n; } fd_sp;
static char *fd_zustand_json(void)
{
    cJSON *o = cJSON_CreateObject();
    bool fertig = GESUCHT && ablauf_verbunden(&A);
    fj_s(o, "sprache", SP_EN ? "en" : "de"); fj_s(o, "version", VERSION_ANZEIGE); fj_b(o, "testmodus", testmodus); fj_b(o, "sim", K->simulation);
#ifdef NUR_LESEN
    fj_b(o, "nur_lesen", true);
#endif
    { char a[64]; fern_adresse(a, sizeof a); fj_s(o, "adresse", a); char u[32]; upd_holen(u, sizeof u); if (u[0]) fj_s(o, "update", u); }
    /* Kopf: Konsole, Laufwerks-Kopplung, Jailbreak, Speicher (wie die Startseite) */
    {
        cJSON *x = cJSON_AddObjectToObject(o, "konsole");
        const char *reihe = K->MODELL_TYP[0] ? kern_modell_reihe(K->MODELL_TYP, K->MODELL_DIGITAL) : T("Unbekanntes Modell", "Unknown model");
        fj_s(x, "titel", KI.firmware[0] ? VERB(fa, reihe, " · Firmware ", KI.firmware) : reihe);
        fj_s(x, "unter", KI.modell[0] ? kern_modell_zeile(fa, KI.modell) : T("Modell nicht erkannt", "Model not detected"));
        kopplungsart ks = kern_kopplungs_stufe(K);
        const char *t = kern_laufwerk_text(K), *dp = strstr(t, ": ");
        cJSON *y = cJSON_AddObjectToObject(o, "kopplung");
        fj_s(y, "titel", dp ? a_kopie_n(fa, t, (size_t)(dp - t)) : t); fj_s(y, "c", ks == KOPPLUNG_BESTAETIGT ? "g" : ks == KOPPLUNG_UNKLAR ? "y" : "d");
        cJSON *z = cJSON_AddObjectToObject(o, "jailbreak");
        const char *warn = NULL, *jbt = fertig && !G.beschaeftigt ? kern_jailbreak_erkannt(K, fa, &warn) : NULL;
        fj_b(z, "geprueft", fertig && !G.beschaeftigt); if (jbt) fj_s(z, "titel", jbt);
        if (!fd_sp.zeit || SDL_GetTicks() - fd_sp.zeit > 5000) {
            for (int i = 0; i < 4; i++) fd_sp.da[i] = plattform_speicher(i, &fd_sp.frei[i], &fd_sp.ges[i]) && fd_sp.ges[i] > 0;
            fd_sp.usb_n = plattform_usb_anzahl(); fd_sp.zeit = SDL_GetTicks();
        }
        long long gef[4] = { 0, 0, 0, 0 }; if (fertig && !G.beschaeftigt) kern_bytes_je_ort(K, gef);
        const char *nm[4] = { T("Interne SSD", "Internal SSD"), T("M.2-SSD", "M.2 SSD"), T("Erweiterter Speicher", "Extended storage"),
                              fd_sp.usb_n > 1 ? a_fmt(fa, T("USB-Laufwerke (%d)", "USB drives (%d)"), fd_sp.usb_n) : T("USB-Laufwerk", "USB drive") };
        cJSON *sp = cJSON_AddArrayToObject(o, "speicher");
        for (int i = 0; i < 4; i++) if (fd_sp.da[i]) {
            cJSON *w = cJSON_CreateObject(); fj_s(w, "name", nm[i]); fj_n(w, "ges", (double)fd_sp.ges[i]); fj_n(w, "frei", (double)fd_sp.frei[i]); fj_n(w, "gef", (double)gef[i]);
            fj_s(w, "ges_t", kern_groesse_text(fa, fd_sp.ges[i])); fj_s(w, "frei_t", kern_groesse_text(fa, fd_sp.frei[i])); fj_s(w, "gef_t", kern_groesse_text(fa, gef[i]));
            cJSON_AddItemToArray(sp, w);
        }
    }
    if (hinweis_bis && (Sint32)(hinweis_bis - SDL_GetTicks()) > 0) { cJSON *h = cJSON_AddObjectToObject(o, "hinweis"); fj_s(h, "t", hinweis_kurz); fj_n(h, "nr", hinweis_nr); }
    fj_n(o, "fg", such_nr); fj_n(o, "sn", nachschau_nr);
    /* welche Seite */
    const char *seite = G.art != D_KEINER ? "frage" : G.beschaeftigt ? (loeschseite ? "loeschen" : "arbeit") : fertig ? "liste" : "start";
    fj_s(o, "seite", seite);
    if (fd_liste_ok()) {
        fj_n(o, "lv", fd_liste_v);
        char *w = a_hole(fa, (size_t)K->Funde.n + 1);
        for (int i = 0; i < K->Funde.n; i++) w[i] = ((eintrag *)K->Funde.e[i])->Wahl ? '1' : '0';
        w[K->Funde.n] = 0; fj_s(o, "wahl", w);
        cJSON *br = cJSON_AddArrayToObject(o, "bereiche");
        long long summe = 0; bool alle_da = true, gekappt = false;
        for (int ki = 0; ki < KAT_ANZ; ki++) {
            int k = ALLE_KAT[ki], z = ablauf_bereich_zustand(&A, k);
            if (z == 2) { int m = 0, ww = 0; for (int j = 0; j < K->Funde.n; j++) { eintrag *f = K->Funde.e[j]; if (f->Kat == k && !f->Gesperrt) { m++; if (f->Wahl) ww++; } } if (ww < m) z = 1; }
            int n = 0; long long b = 0; bool da = true, gek = false;
            for (int j = 0; j < K->Funde.n; j++) { eintrag *f = K->Funde.e[j]; if (f->Kat != k) continue; n++; groesse *g = kern_eintrag_groesse_in(K, fa, f, true); if (!g) da = false; else { b += g->Bytes; if (g->Gekappt) gek = true; } }
            summe += b; if (!da) alle_da = false; if (gek) gekappt = true;
            cJSON *x = cJSON_CreateObject(); fj_n(x, "k", k); fj_n(x, "z", z); fj_n(x, "n", n); fj_s(x, "unter", bereich_unter(k, 0));
            fj_s(x, "g", !n ? "–" : (da || !zaehlen_laeuft()) ? groesse_von(fa, b, gek || !da) : T("zählt …", "counting …"));
            cJSON_AddItemToArray(br, x);
        }
        bool zl = zaehlen_laeuft();
        fj_s(o, "summe", VERB(fa, (!alle_da ? !zl : gekappt) ? T("mind. ", "min. ") : "", kern_groesse_text(fa, summe)));
        if (!alle_da && zl) fj_s(o, "zaehlt", a_fmt(fa, T("Größen werden gezählt (%d von %d) …", "Counting sizes (%d of %d) …"), A.hg_n, A.hg_gesamt));
        /* Auswahl-Leiste wie am Fernseher */
        int gw = 0; long long gb = 0; bool galle = true, ggek = false, rot = false, jbw = false;
        for (int i = 0; i < K->Funde.n; i++) {
            eintrag *f = K->Funde.e[i]; if (!f->Wahl) continue;
            gw++; if (kern_ampel(f) == AMPEL_ROT) rot = true; if (f->Jailbreak) jbw = true;
            groesse *g = kern_eintrag_groesse_in(K, fa, f, true); if (!g) galle = false; else { gb += g->Bytes; if (g->Gekappt) ggek = true; }
        }
        cJSON *l = cJSON_AddObjectToObject(o, "leiste");
        fj_n(l, "n", gw);
        fj_s(l, "t1", gw == 0 ? T("Nichts ausgewählt.", "Nothing selected.") : VERB(fa, T("Ausgewählt: ", "Selected: "), a_fmt(fa, "%d", gw), EINTRAEGE(gw), galle ? VERB(fa, " · ", groesse_von(fa, gb, ggek)) : ""));
        const char *eg = kern_bereiche_106() ? "" : kern_endgueltig_satz(K, fa), *t2 = NULL, *c2 = "d";
        if (rot && eg[0]) { t2 = VERB(fa, T("Achtung: ", "Caution: "), eg); c2 = "r"; }
        else if (rot) { t2 = jbw ? T("Achtung: Jailbreak-Einstieg gewählt - dein Jailbreak startet danach nicht mehr so wie bisher.", "Caution: jailbreak entry selected - your jailbreak will no longer start the way it did.")
                                 : T("Achtung: rot markierte Einträge sind danach endgültig weg.", "Caution: entries marked red are gone for good afterwards."); c2 = "r"; }
        else { const char *sh = kern_selbstschutz_hinweis(K, false); t2 = sh[0] ? sh : T("Gelöscht wird erst nach dem Bestätigen.", "Nothing is deleted before you confirm."); }
        bool auswahl = gw > 0, komplett = K->Funde.n > 0;
#ifdef NUR_LESEN
        auswahl = komplett = false;
#else
        if (testmodus) { auswahl = komplett = false; t2 = testmodus_hinweis(); c2 = "y"; }
#endif
        fj_s(l, "t2", t2); fj_s(l, "c2", c2); fj_b(l, "auswahl", auswahl); fj_b(l, "komplett", komplett);
        if (fd_info_i >= 0 && fd_info_fg == such_nr && fd_info_i < K->Funde.n) fd_info(o, K->Funde.e[fd_info_i]);
    }
    if (G.beschaeftigt && G.art == D_KEINER) {   /* "Nachschauen läuft" bzw. "Löschen läuft" */
        cJSON *x = cJSON_AddObjectToObject(o, "arbeit");
        pthread_mutex_lock(&G.m); char st[1024]; snprintf(st, sizeof st, "%s", G.stand); double p = G.prozent; bool unb = G.unbestimmt; pthread_mutex_unlock(&G.m);
        bool suchen = auftrag_jetzt == AUF_SUCHEN || nach_loeschen, ab = K->abbruch;
        fj_b(x, "suchen", suchen); fj_n(x, "seit", arbeit_seit ? (double)((SDL_GetTicks() - arbeit_seit) / 1000) : 0);
        fj_n(x, "prozent", (unb || (!loeschseite && !st[0])) ? -1 : p); fj_b(x, "abbrechbar", A.abbrechbar && !ab);
        if (loeschseite) {
            lz_nachfuehren();
            int nr = lf_jetzt.nr, n = lf_jetzt.n; bool sim = K->simulation, durch = n > 0 && nr >= n && !A.anhaltbar;
            fj_s(x, "titel", sim ? T("Simulation läuft …", "Simulation running …") : durch ? T("Fast fertig …", "Almost done …") : T("Wird gelöscht …", "Deleting …"));
            fj_n(x, "nr", nr); fj_n(x, "n", n);
            if (n > 0) { fj_s(x, "frei_l", sim ? T("Würde frei geräumt", "Would be freed") : T("Bisher frei geräumt", "Freed so far")); fj_s(x, "frei", groesse_von(fa, lf_jetzt.frei, lf_jetzt.gesamt < 0 && lf_jetzt.frei > 0)); }
            cJSON *zl = cJSON_AddArrayToObject(x, "zeilen");
            for (int i = 0; i < lz_n; i++) {
                cJSON *z = cJSON_CreateObject(); fj_s(z, "n", lz[i].name); fj_n(z, "z", lz[i].zustand); if (lz[i].hat_b) fj_s(z, "g", groesse_von(fa, lz[i].b, false));
                const char *bid = bild_kennung(lz[i].f); if (bid && strlen(bid) < 16) fj_s(z, "i", bid);
                if (lz[i].f) fj_n(z, "k", lz[i].f->Kat);   /* 1.0.9 (09.11.2026): Symbol des Bereichs, wenn kein Bild da ist - wie am Fernseher */
                cJSON_AddItemToArray(zl, z);
            }
            int rest = n - nr; fj_n(x, "rest", rest < 0 ? 0 : rest);
            fj_b(x, "anhaltbar", A.loescht && A.anhaltbar && !anhalten_gewuenscht); fj_b(x, "haelt_an", anhalten_gewuenscht && A.anhaltbar);
            const char *wo = protokoll_wo();
            fj_s(x, "unten", sim ? T("Simulation: Es wird nichts gelöscht.", "Simulation: nothing is deleted.")
                               : VERB(fa, T("Bitte die Konsole nicht ausschalten.", "Please do not turn off the console."), wo[0] ? VERB(fa, T(" Das Protokoll wird ", " The log is being written "), wo, T(" mitgeschrieben.", ".")) : ""));
        } else {
            fj_s(x, "titel", ab ? T("Wird abgebrochen …", "Cancelling …") : nach_loeschen ? T("Schaue nach, was übrig ist …", "Checking what is left …") : suchen ? T("Schaue nach …", "Scanning …")
                         : txt_beginnt(st, T("Zähle", "Only counting")) ? T("Zähle die Größe …", "Counting the size …") : T("Einen Moment …", "One moment …"));
            fj_s(x, "stand", st[0] ? st : T("Wird vorbereitet …", "Preparing …"));
            fj_s(x, "unten", nach_loeschen ? T("Das Löschen ist fertig - jetzt wird nur noch nachgeschaut.", "Deleting is done - now it only checks what is left.") : suchen ? T("Nur lesen - es wird nichts verändert.", "Read only - nothing is changed.") : T("Es wird noch nichts gelöscht - vorher kommt immer die Abschlussfrage.", "Nothing is deleted yet - the final question always comes first."));
        }
    }
    if (G.art != D_KEINER) {   /* eine Frage: Bestätigen, LÖSCHEN eintippen, Frage, Fehler oder Fertig */
        cJSON *x = cJSON_AddObjectToObject(o, "frage");
        pthread_mutex_lock(&G.m);
        dialogart art = G.art;
        const char *titel = a_kopie(fa, G.titel ? G.titel : ""), *txt = a_kopie(fa, G.text ? G.text : ""), *si = a_kopie(fa, G.sicher ? G.sicher : ""), *ge = a_kopie(fa, G.gef ? G.gef : "");
        const char *fk = a_kopie(fa, G.f_kopf ? G.f_kopf : ""), *ff = a_kopie(fa, G.f_frei ? G.f_frei : ""), *fs = a_kopie(fa, G.f_sm ? G.f_sm : ""); bool fok = G.f_ok;
        int dnr = G.dlg_nr;   /* Codex-Prüfung 140: Nummer und Art aus demselben Augenblick */
        pthread_mutex_unlock(&G.m);
        fj_n(x, "nr", dnr);
        fj_s(x, "art", art == D_WAHL ? "wahl" : art == D_TEXT ? "text" : art == D_FEHLER ? "fehler" : "fertig");
        fj_b(x, "bestaetigen", bestaetigen_aktiv()); fj_b(x, "tippen", tippen_aktiv());
        if (art == D_FEHLER && !strcmp(titel, "PS5 Cleanup")) { const char *v = T("Es ist ein Fehler aufgetreten:\n", "An error occurred:\n"); titel = T("Es ist ein Fehler aufgetreten", "An error occurred"); if (txt_beginnt(txt, v)) txt += strlen(v); }
        fj_s(x, "titel", titel); fj_s(x, "text", txt); fj_s(x, "sicher", si); fj_s(x, "gef", ge); fj_s(x, "wort", T("LÖSCHEN", "DELETE"));
        if (bestaetigen_aktiv()) {   /* wie schirm_bestaetigen: Gruppen rot/gelb/grün, größte zuerst */
            int nges = 0; long long gbs[3] = { 0, 0, 0 }; int gn[3] = { 0, 0, 0 }; bool galle[3] = { true, true, true }, ggek[3] = { false, false, false }, jb_gew = false, jb_da = false;
            cJSON *gr = cJSON_AddArrayToObject(x, "gruppen"), *ga[3];
            for (int g = 0; g < 3; g++) ga[g] = cJSON_CreateArray();
            int nmax = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Wahl) nmax++;
            b_zeile *gz[3]; for (int g = 0; g < 3; g++) gz[g] = a_hole(fa, sizeof(b_zeile) * (size_t)(nmax + 1));
            for (int i = 0; i < K->Funde.n; i++) {
                eintrag *f = K->Funde.e[i]; if (f->Jailbreak) jb_da = true;
                if (!f->Wahl || f->Gesperrt) continue;
                if (f->Jailbreak) jb_gew = true;
                int a = kern_ampel(f), g = a == AMPEL_ROT ? 0 : a == AMPEL_GELB ? 1 : 2;
                groesse *gg = kern_eintrag_groesse_in(K, fa, f, true);
                gz[g][gn[g]++] = (b_zeile){ f, gg ? gg->Bytes : -1, gg != NULL, gg ? gg->Gekappt : false };
                if (gg) { gbs[g] += gg->Bytes; if (gg->Gekappt) ggek[g] = true; } else galle[g] = false;
            }
            for (int g = 0; g < 3; g++) {
                if (!gn[g]) { cJSON_Delete(ga[g]); continue; }
                qsort(gz[g], (size_t)gn[g], sizeof(b_zeile), b_vergleich); nges += gn[g];
                cJSON *y = cJSON_CreateObject(); fj_s(y, "a", g == 0 ? "r" : g == 1 ? "y" : "g");
                fj_s(y, "titel", g == 0 ? T("Endgültig weg", "Gone for good") : g == 1 ? T("Neu laden oder einrichten", "Download or set up again") : T("Es geht nichts verloren", "Nothing is lost"));
                fj_s(y, "unter", VERB(fa, a_fmt(fa, "%d", gn[g]), EINTRAEGE(gn[g]), galle[g] ? VERB(fa, " · ", groesse_von(fa, gbs[g], ggek[g])) : ""));
                for (int i = 0; i < gn[g] && i < 200; i++) {
                    cJSON *z = cJSON_CreateObject(); fj_s(z, "n", ohne_ort_vorne(kern_zeile(fa, gz[g][i].f))); fj_n(z, "k", gz[g][i].f->Kat);   /* 1.0.9: Symbol des Bereichs */
                    if (gz[g][i].da) fj_s(z, "g", groesse_von(fa, gz[g][i].b, gz[g][i].gek));
                    const char *ok = ort_kurz(anzeige_ort(gz[g][i].f)); if (ok[0]) fj_s(z, "o", ok);
                    const char *bid = bild_kennung(gz[g][i].f); if (bid && strlen(bid) < 16) fj_s(z, "i", bid);
                    cJSON_AddItemToArray(ga[g], z);
                }
                if (gn[g] > 200) fj_n(y, "mehr", gn[g] - 200);
                cJSON_AddItemToObject(y, "e", ga[g]); cJSON_AddItemToArray(gr, y);
            }
            const char *sub = VERB(fa, a_fmt(fa, "%d", nges), EINTRAEGE(nges));
            if (A.GROSS.Bytes > 0) sub = VERB(fa, sub, " · ", groesse_von(fa, A.GROSS.Bytes, A.GROSS.Gekappt), T(" werden frei", " will be freed"));
            if (tippen_aktiv()) sub = VERB(fa, T("Vollständige Bereinigung · ", "Full cleanup · "), sub);
            fj_s(x, "sub", sub);
            b_hinweis hw[16]; int nh = bestaetigen_hinweise(hw, 16);
            cJSON *h = cJSON_AddArrayToObject(x, "hinweise");
            for (int i = 0; i < nh; i++) { cJSON *z = cJSON_CreateObject(); fj_s(z, "t", hw[i].t); fj_s(z, "c", hw[i].f == N_ROT ? "r" : hw[i].f == N_GELB ? "y" : "d"); cJSON_AddItemToArray(h, z); }
            if (jb_gew) fj_s(x, "jb", T("Jailbreak-Einstieg ausgewählt", "Jailbreak entry selected")); else if (jb_da) fj_s(x, "jb_bleibt", T("Jailbreak bleibt (nicht ausgewählt)", "Jailbreak stays (not selected)"));
            if (tippen_aktiv()) { pthread_mutex_lock(&G.m); fj_s(x, "eingabe", G.eingabe); pthread_mutex_unlock(&G.m); }
        }
        if (art == D_FERTIG) {
            bool sim = K->simulation; const char *gt = groesse_von(fa, A.rm_frei.Bytes, A.rm_frei.Gekappt);
            fj_s(x, "gross", sim ? VERB(fa, gt, T(" würden frei", " would be freed")) : VERB(fa, gt, T(" frei geräumt", " freed")));
            fj_b(x, "ok", fok); fj_s(x, "kopf", fk); fj_s(x, "frei", ff); fj_s(x, "sm", fs);
            const char *rt = kern_rest_text(K, fa); fj_s(x, "rest", (fok && !rt[0]) ? "" : rt[0] ? rt : T("Nicht alles - was übrig ist, steht im Protokoll.", "Not everything - the log shows what is left."));
            const char *wo = protokoll_wo(), *pd = A.protokoll_datei ? A.protokoll_datei : "", *dp = strchr(pd, ':');
            fj_s(x, "prot", wo[0] ? VERB(fa, T("Gespeichert ", "Saved "), wo) : T("Nur in der Protokoll-Ansicht", "Only in the log view")); if (wo[0] && dp) fj_s(x, "prot_wo", dp + 2);
            rueckmeldung r = rueckmeldung_daten(); fj_s(x, "rueckmeldung", rueckmeldung_link(fa, &r));
            fj_b(x, "sim", sim);
        }
    }
    char *s = cJSON_PrintUnformatted(o); cJSON_Delete(o);
    return s;
}
/* ---- Befehle aus dem Browser */
static void fd_block_setzen(int k, int kopf)
{
    kblock *x = NULL; int nb = bloecke_holen(fa, k, SORT_GROESSTE, &x);
    for (int b = 0; b < nb; b++) {
        bool reste = x[b].kopf == &RESTE_ZEILE;
        if (kopf < 0 ? reste : (!reste && kopf < K->Funde.n && x[b].kopf == K->Funde.e[kopf])) { block_setzen_p(&x[b].teile, reste, true); return; }
    }
    kurz(T("Die Liste hat sich geändert - bitte noch einmal.", "The list has changed - please try again."));
}
static int jzahl(const cJSON *j, const char *k, int nein) { const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, k); return cJSON_IsNumber(v) ? (int)v->valuedouble : nein; }
static bool jja(const cJSON *j, const char *k) { return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j, k)); }
static const char *jtext(const cJSON *j, const char *k) { const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, k); return cJSON_IsString(v) && v->valuestring ? v->valuestring : NULL; }
/* Codex-Prüfung 140 (P1): Antwort nur, wenn sie zur gerade gestellten Frage gehört - Prüfen und Annehmen zusammen unter G.m, jede Frage
 * nur einmal (der Arbeitsfaden kann zwischen zwei Befehlen schon die nächste Frage stellen). eingabe: Text der Eingabe (oder NULL). */
static bool fd_antworten(int nr, dialogart art, bool wahl, bool eingabe_ok, const char *eingabe)
{
    pthread_mutex_lock(&G.m);
    bool ok = G.art == art && G.dlg_nr == nr && !G.beantwortet;
    if (ok && eingabe) snprintf(G.eingabe, sizeof G.eingabe, "%s", eingabe);
    if (ok && eingabe_ok && tippen_aktiv() && !eingabe_passt()) ok = false;
    if (ok) { G.wahl = wahl; G.eingabe_ok = eingabe_ok; G.beantwortet = true; pthread_cond_broadcast(&G.c); }
    pthread_mutex_unlock(&G.m);
    if (ok) { dlg_fokus = 0; dlg_scroll = 0; x_seit = 0; }
    return ok;
}
static void fd_befehl(const cJSON *j)
{
    const char *b = jtext(j, "b"); if (!b) return;
    int fg = jzahl(j, "fg", -1), i = jzahl(j, "i", -1), k = jzahl(j, "k", -1), nr = jzahl(j, "nr", -1);
    bool frei = !G.beschaeftigt && G.art == D_KEINER, liste = fd_liste_ok() && G.art == D_KEINER && fg == such_nr;
    bool frage = G.art != D_KEINER && nr == G.dlg_nr && G.dlg_nr == dlg_gesehen;
    if (!strcmp(b, "info")) { if (fg == such_nr && i >= 0 && i < K->Funde.n) { fd_info_i = i; fd_info_fg = fg; } else fd_info_i = -1; return; }
    if (!strcmp(b, "protokoll_stick")) { protokoll_auf_stick(prot_meldung, sizeof prot_meldung); kurz(prot_meldung); return; }
    /* was den Zustand ändert, schließt am Fernseher das Options-Menü, das Protokoll und die Info-Seite (sonst läge dort etwas Altes) */
    optionen_offen = false; protokoll_offen = false; fern_info_offen = false; neu_zeichnen();
    if (!strcmp(b, "nachschauen")) { if (frei) { AUSWAHL_OFFEN = false; starte(AUF_SUCHEN); } }
    else if (!strcmp(b, "auswahl")) { if (frei && GESUCHT && ablauf_verbunden(&A) && !AUSWAHL_OFFEN) { AUSWAHL_OFFEN = true; fokus_zone = 0; fokus_bereich = 0; AL.k = -1; al_b = 0; al_t = -1; al_py = 0; } }
    else if (!strcmp(b, "sprache")) { if (frei && !GESUCHT) { SP_EN = !SP_EN; sim_hinweis_setzen(); plattform_absturzmelder(SP_EN ? "PS5 Cleanup " VERSION_TEXT_EN : "PS5 Cleanup " VERSION_TEXT); fern_sprache(SP_EN); } }
    else if (!strcmp(b, "testmodus")) { if (frei) testmodus_umschalten(); }
    else if (liste && (!strcmp(b, "wahl") || !strcmp(b, "block") || !strcmp(b, "bereich")) && !AUSWAHL_OFFEN) {   /* der Fernseher geht mit zur Auswahl */
        AUSWAHL_OFFEN = true; fokus_zone = 0; fokus_bereich = 0; AL.k = -1; al_b = 0; al_t = -1; al_py = 0; fd_befehl(j); return;
    }
    else if (!strcmp(b, "wahl")) { if (liste && i >= 0 && i < K->Funde.n) eintrag_umschalten(K->Funde.e[i]); else if (!liste) kurz(T("Die Liste hat sich geändert - bitte noch einmal.", "The list has changed - please try again.")); }
    else if (!strcmp(b, "block")) { if (liste) fd_block_setzen(k, jzahl(j, "kopf", -2)); }
    else if (!strcmp(b, "bereich")) {
        if (liste && k >= 1 && k <= KAT_ANZ) {
            int n = bereich_anzahl(k);
            if (n > 0 && ablauf_bereich_nur_einzeln(&A, k) && ablauf_bereich_zustand(&A, k) == 0)
                kurz(T("Hier nur einzeln wählbar: die Einträge einzeln anhaken.", "Individually only here: tick the entries one by one."));
            else if (n > 0) ablauf_bereich_setzen(&A, k, ablauf_bereich_zustand(&A, k) != 2);
        }
    }
    else if (!strcmp(b, "auswahl_loeschen") || !strcmp(b, "komplett")) {
        if (!liste) return;
#ifdef NUR_LESEN
        kurz(T("In dieser Testversion gesperrt - es wird nur nachgeschaut.", "Locked in this test version - only looking."));
#else
        if (testmodus) { kurz(testmodus_hinweis()); hinweis_dauer(6000); return; }
        if (!strcmp(b, "auswahl_loeschen")) { if (gewaehlt_anzahl() > 0) { dlg_fokus = 0; dlg_scroll = 0; starte(AUF_AUSWAHL); } else kurz(T("Erst etwas auswählen.", "Select something first.")); }
        else if (K->Funde.n > 0) { dlg_fokus = 0; dlg_scroll = 0; starte(AUF_KOMPLETT); }
#endif
    }
    else if (!strcmp(b, "antwort")) {   /* Frage mit zwei Knöpfen; "Löschen" auf der Seite Bestätigen nur gehalten und frühestens 1,5 s nach dem Erscheinen */
        if (!frage || G.art != D_WAHL || SDL_GetTicks() - dlg_seit < 400) return;   /* wie am Fernseher: kurz nach dem Erscheinen zählt nichts */
        bool ja = jja(j, "ja");
        if (bestaetigen_aktiv() && ja && (!jja(j, "gehalten") || SDL_GetTicks() - dlg_seit < HALTEN_MS)) { kurz(T("Zum Löschen „Löschen“ gedrückt halten, bis der Kreis voll ist.", "To delete, hold 'Delete' until the circle is full.")); return; }
        fd_antworten(nr, D_WAHL, ja, false, NULL);
    }
    else if (!strcmp(b, "eingabe")) {   /* LÖSCHEN eintippen (vollständige Bereinigung) bzw. eine Texteingabe */
        if (!frage || G.art != D_TEXT) return;
        if (jja(j, "zurueck")) { fd_antworten(nr, D_TEXT, false, false, NULL); return; }
        const char *t = jtext(j, "t"); if (!t) return;
        if (!fd_antworten(nr, D_TEXT, false, true, t) && tippen_aktiv()) { kurz(T("Bitte genau LÖSCHEN eintippen, in Großbuchstaben.", "Please type DELETE exactly, in capital letters.")); hinweis_dauer(5000); }
    }
    else if (!strcmp(b, "ok")) { if (frage) fd_antworten(nr, D_FEHLER, false, false, NULL); }
    else if (!strcmp(b, "ende")) {
        if (frage && G.art == D_FERTIG) { const char *a = jtext(j, "a"); int na = a && !strcmp(a, "neustart") ? 2 : 1;
#ifndef VORSCHAU
            if (!G.beantwortet) A.schluss_ohne_suche = na == 1;   /* NEU 1.0.10 (PS5-Test 13.11.2026): vor der Antwort - der Arbeitsfaden läuft danach sofort weiter */
#endif
            if (fd_antworten(nr, D_FERTIG, false, false, NULL)) nach_aktion = na; }
    }
    else if (!strcmp(b, "abbrechen")) { if (G.beschaeftigt && G.art == D_KEINER && A.abbrechbar && !K->abbruch) { K->abbruch = true; ui_stand(NULL, T("Breche ab ...", "Cancelling ..."), ABLAUF_BALKEN_GLEICH); } }
    else if (!strcmp(b, "anhalten")) { if (A.loescht && A.anhaltbar && G.art == D_KEINER && !anhalten_gewuenscht && ablauf_loeschen_anhalten(&A)) { anhalten_gewuenscht = true; kurz(T("Wird nach diesem Eintrag angehalten.", "Stopping after this entry.")); } }
    else if (!strcmp(b, "beenden")) { if (frei && !A.loescht) { ui_log(NULL, T("PS5 Cleanup beendet sich (im Browser gewählt).", "PS5 Cleanup quits (chosen in the browser).")); laeuft = false; } }
}
static void fern_befehle(void)   /* aus der Hauptschleife */
{
    char t[2100]; int n = 0;
    while (n++ < 16 && fern_befehl_holen(t, sizeof t)) {
        cJSON *j = cJSON_Parse(t);
        if (j) { arena *alt = fa; fa = arena_neu(); fd_befehl(j); arena_frei(fa); fa = alt; ablauf_texte_frei(&A); }
        cJSON_Delete(j);
    }
}
/* Bilder der Kacheln für den Browser - im Hauptfaden geladen wie am Fernseher (nicht während des Nachschauens: dann ändert der Kern sie) */
static void fern_bilder(void)
{
    if (G.beschaeftigt && !A.loescht) return;
    char id[24]; int n = 0;
    while (n++ < 4) {
        /* NEU 1.0.10 (Codex-Prüfung 147): wie kachelbild() - läuft gerade ein Besitzer-Wechsel (private Ordner), erst beim nächsten Mal;
         * der Wunsch bleibt so lange stehen (erst nach der Sperre abholen) */
        if (!zugriff_wechsel_versuchen()) break;
        if (!fern_bild_wunsch(id, sizeof id)) { zugriff_wechsel_frei(); break; }
        int w = 0, h = 0; unsigned char *px = kachelbild_laden(id, &w, &h);
        zugriff_wechsel_frei();
        if (!px) { fern_bild_geben(id, NULL, 0, 0); continue; }
        const int s = 144; unsigned char *o = malloc((size_t)s * s * 3);   /* 1.0.9 (da_Chris86 22:30, 4K): 144 statt 96 - auf Handy und 4K-Bildschirm (2-fache Pixeldichte) scharf, rund 7 KB je Bild */
        const unsigned hr = (N_KARTE >> 16) & 255, hg = (N_KARTE >> 8) & 255, hb = N_KARTE & 255;
        for (int y = 0; o && y < s; y++) for (int x = 0; x < s; x++) {   /* Flächenmittel wie bild_verkleinert, auf den Kartengrund gelegt */
            int x0 = x * w / s, x1 = (x + 1) * w / s, y0 = y * h / s, y1 = (y + 1) * h / s;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            unsigned long r = 0, g = 0, b = 0, a = 0, m = 0;
            for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) { const unsigned char *q = px + ((size_t)yy * (size_t)w + (size_t)xx) * 4; r += (unsigned long)q[0] * q[3]; g += (unsigned long)q[1] * q[3]; b += (unsigned long)q[2] * q[3]; a += q[3]; m++; }
            unsigned long am = a / m; unsigned char *z = o + ((size_t)y * s + (size_t)x) * 3;
            z[0] = (unsigned char)((r / m + hr * (255 - am)) / 255); z[1] = (unsigned char)((g / m + hg * (255 - am)) / 255); z[2] = (unsigned char)((b / m + hb * (255 - am)) / 255);
        }
        stbi_image_free(px);
        fern_bild_geben(id, o, o ? s : 0, o ? s : 0); free(o);
    }
}
/* aus der Hauptschleife: Zustand und Liste für den Browser - nur wenn einer fragt; der Zustand höchstens alle 120 ms, die Liste bei einer
 * neuen Suche, beim Umschalten der Sprache und während des Zählens höchstens alle 1,5 s (dann ändern sich die Größen) */
static Uint32 fd_zeit, fd_l_zeit; static int fd_l_fg = -1, fd_l_hg = -1, fd_l_en = -1; static bool fd_l_da = false, fd_dringend = true;
static void fern_daten_raus(void)
{
    fern_bilder();
    static bool im_browser = false;   /* Codex-Prüfung 140 (P3): Pille "IM BROWSER" an bzw. aus, auch wenn sich sonst nichts ändert */
    if (fern_aktiv(8000) != im_browser) { im_browser = !im_browser; neu_zeichnen(); }
    if (!fern_will_daten()) { fd_l_da = false; return; }
    Uint32 jetzt = SDL_GetTicks();
    if (!fd_dringend && jetzt - fd_zeit < 120) return;
    arena *alt = fa; fa = arena_neu();
    if (fd_liste_ok() && (!fd_l_da || fd_l_fg != such_nr || fd_l_en != SP_EN || (fd_l_hg != A.hg_n && jetzt - fd_l_zeit >= 1500))) {
        fd_liste_v++;
        char *l = fd_liste_json();
        if (l) { fern_liste_geben(l); fd_l_da = true; fd_l_fg = such_nr; fd_l_en = SP_EN; fd_l_hg = A.hg_n; fd_l_zeit = jetzt; }
    }
    char *z = fd_zustand_json();
    arena_frei(fa); fa = alt; ablauf_texte_frei(&A);
    if (z) fern_zustand_geben(z);
    fd_zeit = jetzt; fd_dringend = false;
}
static int tastennr(int sdl)
{
    switch (sdl) {
    case SDL_CONTROLLER_BUTTON_A: return T_X; case SDL_CONTROLLER_BUTTON_B: return T_KREIS;
    case SDL_CONTROLLER_BUTTON_X: return T_QUADRAT; case SDL_CONTROLLER_BUTTON_Y: return T_DREIECK;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return T_HOCH; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return T_RUNTER;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return T_LINKS; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return T_RECHTS;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return T_L1; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return T_R1;
    case SDL_CONTROLLER_BUTTON_START: return T_OPTIONS; default: return T_ANDERE;
    }
}
static void pad_oeffnen(void)
{
    if (pad) return;
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i) && (pad = SDL_GameControllerOpen(i))) { SDL_GameControllerSetLED(pad, 0x2F, 0x6F, 0xDB); return; }
}

/* =================================================================== Vorschau am PC: Ablauf aus einer Datei */
#ifdef VORSCHAU
static FILE *skript; static Uint32 skript_warte = 0;
static void bild_speichern(const char *name)
{
    zeichnen();
    char p[512]; snprintf(p, sizeof p, "%s.bmp", name);
    SDL_SaveBMP(ziel, p);
    printf("VORSCHAU Bild %s\n", p);
}
static int tasten_name(const char *s)
{
    const char *n[] = { "X", "KREIS", "QUADRAT", "DREIECK", "HOCH", "RUNTER", "LINKS", "RECHTS", "L1", "R1", "OPTIONS" };
    for (int i = 0; i < 11; i++) if (!strcmp(s, n[i])) return i;
    return T_ANDERE;
}
static void *skript_fehler(void *p)   /* NEU 1.0.8 (K5): Fehlerseite in der PC-Vorschau ansehen - Aufbau wie in ablauf_starte, Beispiel-Fehler */
{
    (void)p;
    char t[1024];
    snprintf(t, sizeof t, "%s%s%s%s%s", T("Es ist ein Fehler aufgetreten:\n", "An error occurred:\n"),
             T("Ordner nicht lesbar: /user/app/PPSA01650 (Beispiel aus der PC-Vorschau)", "Folder not readable: /user/app/PPSA01650 (example from the PC preview)"),
             T("\n\nDas Protokoll liegt hier:\n", "\n\nThe log is here:\n"), A.protokoll_datei ? A.protokoll_datei : "",
             T("\n\nBitte diese Datei bei einer Fehlermeldung mitschicken.", "\n\nPlease attach this file when reporting the problem."));
    ui_fehler(NULL, t);
    return NULL;
}
static void skript_schritt(void)
{
    if (!skript || SDL_GetTicks() < skript_warte) return;
    char z[512];
    for (;;) {
        if (!fgets(z, sizeof z, skript)) { fclose(skript); skript = NULL; laeuft = false; return; }
        size_t l = strlen(z); while (l && (z[l - 1] == '\n' || z[l - 1] == '\r')) z[--l] = 0;
        if (!l || z[0] == '#') continue;
        if (!strncmp(z, "TASTE ", 6)) { taste(tasten_name(z + 6)); skript_warte = SDL_GetTicks() + 60; return; }
        if (!strncmp(z, "WARTELOESCHEN ", 14)) {   /* 1.0.8 (4.5): bis der Kern beim Eintrag n ist (oder nichts mehr läuft) */
            if (G.beschaeftigt && A.lf_nr < atoi(z + 14)) { fseek(skript, -(long)strlen(z) - 1, SEEK_CUR); skript_warte = SDL_GetTicks() + 50; return; }
            continue;
        }
        if (!strcmp(z, "HALTE KREIS")) { kreis_seit = SDL_GetTicks(); if (!kreis_seit) kreis_seit = 1; skript_warte = SDL_GetTicks() + 60; return; }   /* 1.0.8 (4.5): ○ halten (Anhalten) */
        if (!strncmp(z, "HALTE X", 7)) {   /* NEU 1.0.8 (4.4): ✕ halten (Bestätigen); "HALTE X 700" = nach 700 ms weiter im Skript, der Kreis läuft weiter */
            halten_start(); int ms = atoi(z + 7); skript_warte = SDL_GetTicks() + (Uint32)(ms > 0 ? ms : HALTEN_MS + 400); return;
        }
        if (!strcmp(z, "LASSE X")) { halten_ende(); skript_warte = SDL_GetTicks() + 60; return; }   /* NEU 1.0.8 (5.1a): ✕ vor 1,5 s loslassen */
        if (!strncmp(z, "TEXT ", 5)) {   /* 1.0.8: "TEXT @WORT" = LÖSCHEN/DELETE je Sprache, "TEXT @KLEIN" = dasselbe klein (falsch) */
            const char *w = !strcmp(z + 5, "@WORT") ? T("LÖSCHEN", "DELETE") : !strcmp(z + 5, "@KLEIN") ? T("löschen", "delete") : z + 5;
            pthread_mutex_lock(&G.m); snprintf(G.eingabe, sizeof G.eingabe, "%.*s", (int)sizeof G.eingabe - 1, w); pthread_mutex_unlock(&G.m); tastatur_offen = false; return; }
        if (!strncmp(z, "BILD ", 5)) { bild_speichern(z + 5); return; }
        if (!strcmp(z, "FEHLER")) { pthread_t th; if (!pthread_create(&th, NULL, skript_fehler, NULL)) pthread_detach(th); skript_warte = SDL_GetTicks() + 60; return; }   /* 1.0.8 (K5) */
        if (!strncmp(z, "WARTE ", 6)) { skript_warte = SDL_GetTicks() + (Uint32)atoi(z + 6); return; }
        if (!strcmp(z, "WARTEFREI")) {   /* bis keine Arbeit mehr läuft und die Größen gezählt sind */
            if (G.beschaeftigt || G.art != D_KEINER || (A.hintergrund && A.hg_n < A.hg_gesamt)) { fseek(skript, -(long)strlen(z) - 1, SEEK_CUR); skript_warte = SDL_GetTicks() + 100; return; }
            continue;
        }
        if (!strcmp(z, "WARTESUCHE")) {   /* NEU 1.0.8 (PS5-Test 07.11.2026): bis das Nachschauen fertig ist - die Größen dürfen noch gezählt werden */
            if (G.beschaeftigt || G.art != D_KEINER) { fseek(skript, -(long)strlen(z) - 1, SEEK_CUR); skript_warte = SDL_GetTicks() + 50; return; }
            continue;
        }
        if (!strcmp(z, "WARTEDIALOG")) {
            if (G.art == D_KEINER || G.dlg_nr != dlg_gesehen || SDL_GetTicks() - dlg_seit < 450) { fseek(skript, -(long)strlen(z) - 1, SEEK_CUR); skript_warte = SDL_GetTicks() + 100; return; }
            continue;
        }
        /* NEU 1.0.9: Bedienung im Browser in der PC-Vorschau (test/kern/lauf_fern_vorschau.sh) */
        if (!strcmp(z, "WARTEBROWSER")) { if (!fern_aktiv(3000)) { fseek(skript, -(long)strlen(z) - 1, SEEK_CUR); skript_warte = SDL_GetTicks() + 50; return; } continue; }
        if (!strncmp(z, "WARTEZEILE ", 11)) {   /* bis diese Zeile im Protokoll steht (der Browser hat etwas getan) */
            bool da = false; pthread_mutex_lock(&G.m); for (int i = 0; i < G.log_n && !da; i++) if (strstr(G.log[i], z + 11)) da = true; pthread_mutex_unlock(&G.m);
            if (!da) { fseek(skript, -(long)strlen(z) - 1, SEEK_CUR); skript_warte = SDL_GetTicks() + 50; return; }
            continue;
        }
        if (!strncmp(z, "WARTEDATEI ", 11)) {   /* bis die Gegenstelle (Test) diese Datei angelegt hat */
            struct stat st;
            if (stat(z + 11, &st) != 0) { fseek(skript, -(long)strlen(z) - 1, SEEK_CUR); skript_warte = SDL_GetTicks() + 50; return; }
            continue;
        }
        if (!strncmp(z, "CONTROLLER ", 11)) { int tc = tasten_name(z + 11); if (!tv_vorrang(tc)) taste(tc); skript_warte = SDL_GetTicks() + 60; return; }   /* wie ein Druck am Controller */
        if (!strcmp(z, "ENDE")) { laeuft = false; return; }
    }
}
#endif

/* =================================================================== Hauptschleife */
static int programm(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) { printf("PS5 Cleanup: SDL_Init: %s\n", SDL_GetError()); return 1; }
    fenster = SDL_CreateWindow("PS5 Cleanup " VERSION_TEXT, 0, 0, BREITE, HOEHE, 0);
    if (!fenster) { printf("PS5 Cleanup: SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    ziel = SDL_GetWindowSurface(fenster);
    if (!ziel) { printf("PS5 Cleanup: SDL_GetWindowSurface: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    stbtt_InitFont(&font[0], schrift_normal, stbtt_GetFontOffsetForIndex(schrift_normal, 0));
    stbtt_InitFont(&font[1], schrift_fett, stbtt_GetFontOffsetForIndex(schrift_fett, 0));
    logo_laden();
    pad_oeffnen();

    SP_EN = plattform_englisch();
    plattform_konsole(&KI);
    plattform_absturzmelder(SP_EN ? "PS5 Cleanup " VERSION_TEXT_EN : "PS5 Cleanup " VERSION_TEXT);   /* 1.0.6 Fix: ab hier jeder Absturz als Benachrichtigung + Protokoll (1.0.8: in der eingestellten Sprache) */
#ifdef MITSCHNITT
    ms_oeffnen();                                 /* NEU 1.0.8 (2.7): vor dem ersten Zugriff - so steht alles im Mitschnitt */
#endif
    K = kern_neu(app_zugriff());
    K->appinfo_lesen = plattform_appinfo_lesen;   /* NEU 1.0.7 (02.11.2026): Zuordnungen und kaputte Zusatzinhalt-Angaben aus appinfo.db (nur lesen) */
    /* Codex-Prüfungen 74/75: der Ordner der Test-Fassung an jedem möglichen Ort - in jeder Fassung, auch wenn der Mitschnitt diesmal nicht geht */
    for (size_t i = 0; i < sizeof TEST_ORDNER_ORTE / sizeof TEST_ORDNER_ORTE[0]; i++) sl_add(K->a, &K->TEST_ORDNER, TEST_ORDNER_ORTE[i]);
#ifdef MITSCHNITT
    if (ms_f) {
        K->appinfo_lesen = ms_appinfo_lesen;
        zugriff_mitschnitt_zustand = ms_db_zustand;
    }
#endif
#ifdef KERN_LOESCHEN
    K->simulation = false;                                  /* Release-Bau: löscht wirklich (nach Bestätigung) */
#else
    K->simulation = true;                                   /* dieser Bau löscht nie */
#endif
    K->MODELL_TYP = kern_modell_typ(K->a, KI.modell); K->MODELL_DIGITAL = kern_modell_digital(K->a, KI.modell);
    K->DDD_BEKANNT = KI.ddd_bekannt; K->DDD_STATUS = KI.ddd;
    ablauf_ui ui = { ui_wahl, ui_text, ui_log, ui_stand, ui_fertig, ui_fehler, ui_liste_neu, NULL };
    ablauf_init(&A, K, &ui);
    K->melde = ui_log; K->melde_ctx = NULL;
    A.groesse_neu = groesse_neu_app; A.groesse_ctx = NULL;
    A.neue_verbindung = neue_verbindung;
#ifdef KERN_LOESCHEN
    A.protokoll_datei = T("in der Protokoll-Ansicht (Options) - beim Löschen auf dem USB-Laufwerk oder im Ordner /data/PS5-Cleanup-Protokolle", "in the log view (Options) - when deleting on the USB drive or in the folder /data/PS5-Cleanup-Protokolle");
#else
    A.protokoll_datei = T("in der Protokoll-Ansicht (Options) - diese Fassung speichert es nicht als Datei", "in the log view (Options) - this version does not save it as a file");   /* Prüfung p8, Befund 6 */
#endif
#ifdef NUR_LESEN
    sim_hinweis_setzen();
    ui_log(NULL, VERB(K->a, "PS5 Cleanup ", VERSION_ANZEIGE, " (", plattform_name(), ") - ", T("TEST: NUR LESEN", "TEST: READ ONLY")));
#elif defined(KERN_LOESCHEN)
    ui_log(NULL, VERB(K->a, "PS5 Cleanup ", VERSION_ANZEIGE, " (", plattform_name(), ")"));
#else
    ui_log(NULL, VERB(K->a, "PS5 Cleanup ", VERSION_ANZEIGE, " (", plattform_name(), ") - SIMULATION"));
#endif

    /* 02.11.2026 (da_Chris86 15:44/15:45): die Protokoll-Datei beginnt erst direkt vor dem ersten Löschen (protokoll_beginnen) -
     * wer nur nachschaut, hinterlässt keine Datei. Bis dahin steht alles in der Ansicht (Options). */
    A.vor_selbstweg = vor_selbstweg_app; A.vor_selbstweg_ctx = NULL;
    upd_starten();   /* NEU 1.0.9: Update-Hinweis - fragt im Hintergrund, die Oberfläche wartet nicht darauf */
    fern_einrichten(fern_log, neu_zeichnen);   /* NEU 1.0.9: Bedienung im Browser - ab dem Start an (da_Chris86 08.11.2026: "Immer gleich an") */
    { char ff[160] = ""; if (!fern_starten(SP_EN, ff, sizeof ff)) ui_log(NULL, VERB(K->a, T("Browser-Bedienung nicht verfügbar: ", "Browser control not available: "), ff)); }
#ifdef MITSCHNITT
    if (ms_f) A.vor_neusuche = ms_neusuche;      /* NEU 1.0.8 (Codex-Prüfung 74): das Nachschauen nach dem Löschen als eigene Suche */
#endif
    A.vor_loeschen = vor_dem_loeschen; A.vor_loeschen_ctx = NULL;   /* 02.11.2026: nur noch das Protokoll - keine Datenbank-Sicherung */
    plattform_meldung = ui_meldung;   /* NEU 1.0.7: jeder Schritt der Sony-Deinstallation ins Protokoll */
#ifdef PRUEFLAUF
    starte(AUF_SUCHEN);                  /* Prüflauf: Nachschauen beginnt von selbst (nur lesen) */
#endif
    while (laeuft) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: if (!G.beschaeftigt) laeuft = false; break;
            case SDL_CONTROLLERDEVICEADDED: pad_oeffnen(); neu_zeichnen(); break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (pad && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) { SDL_GameControllerClose(pad); pad = NULL; kreis_seit = 0; }
                neu_zeichnen(); break;
            case SDL_CONTROLLERBUTTONDOWN: {
                int t = tastennr(e.cbutton.button);
                if (tv_vorrang(t)) { if (e.cbutton.button < SDL_CONTROLLER_BUTTON_MAX) tv_geschluckt[e.cbutton.button] = true; break; }   /* NEU 1.0.9 */
                if (t == T_KREIS) { kreis_seit = SDL_GetTicks(); neu_zeichnen(); break; }   /* erst beim Loslassen (kurz) */
                if (t == T_X && halten_start()) { x_vom_pad = true; SDL_ClearError(); break; }   /* NEU 1.0.8 (4.4): ✕ auf "Löschen" wird gehalten */
                taste(t); break;
            }
            case SDL_CONTROLLERBUTTONUP:
                if (e.cbutton.button < SDL_CONTROLLER_BUTTON_MAX && tv_geschluckt[e.cbutton.button]) { tv_geschluckt[e.cbutton.button] = false; break; }   /* NEU 1.0.9: gehört zur verbrauchten Taste */
                if (tastennr(e.cbutton.button) == T_KREIS && kreis_seit) { kreis_seit = 0; taste(T_KREIS); }
                if (tastennr(e.cbutton.button) == T_X) halten_ende();
                break;
#ifdef VORSCHAU   /* Codex-Prüfung 79: Tasten der PC-Tastatur nur in der PC-Vorschau. Auf der PS5 kam von einer USB-Tastatur, die während des
                  * Haltens auf "Löschen" abgezogen wird, kein Loslassen - das Halten hätte trotzdem bestätigt. Eintippen über Sonys Tastatur
                  * (SDL_TEXTINPUT, darunter) bleibt. */
            case SDL_KEYUP:     /* nur zum Testen am PC */
                if (e.key.keysym.sym == SDLK_RETURN) halten_ende();
                break;
            case SDL_KEYDOWN:   /* nur zum Testen am PC */
                if (e.key.repeat) break;
                switch (e.key.keysym.sym) {
                case SDLK_RETURN: if (!halten_start()) taste(T_X); break; case SDLK_ESCAPE: taste(T_KREIS); break;
                case SDLK_s: taste(T_QUADRAT); break; case SDLK_t: taste(T_DREIECK); break; case SDLK_o: taste(T_OPTIONS); break;
                case SDLK_UP: taste(T_HOCH); break; case SDLK_DOWN: taste(T_RUNTER); break; case SDLK_LEFT: taste(T_LINKS); break; case SDLK_RIGHT: taste(T_RECHTS); break;
                default: break;
                }
                break;
#endif
            case SDL_TEXTINPUT:
                pthread_mutex_lock(&G.m); strncat(G.eingabe, e.text.text, sizeof G.eingabe - strlen(G.eingabe) - 1); pthread_mutex_unlock(&G.m);
                neu_zeichnen(); break;
            default: break;
            }
        }
        if (tastatur_offen) {
            bool sichtbar = SDL_IsScreenKeyboardShown(fenster);
            if (sichtbar) tastatur_gesehen = true;
            if ((tastatur_gesehen && !sichtbar) || (!tastatur_gesehen && SDL_GetTicks() - tastatur_seit > 3000)) {
                tastatur_offen = false; SDL_StopTextInput();
                if (!tastatur_gesehen && !G.eingabe[0]) kurz(T("Die Tastatur ließ sich nicht öffnen.", "The keyboard could not be opened."));
                else dlg_fokus = 2;        /* nach dem Eintippen steht der Fokus auf OK */
                neu_zeichnen();
            }
        }
#ifdef PRUEFLAUF
        if (pruef_fertig_ab && !G.beschaeftigt && SDL_GetTicks() - pruef_fertig_ab > 15000) laeuft = false;   /* zurück zum Home-Menü */
#endif
        if (G.dlg_nr != dlg_gesehen) { dlg_gesehen = G.dlg_nr; dlg_fokus = 0; dlg_scroll = 0; dlg_seit = SDL_GetTicks(); neu_zeichnen();   /* neue Frage: sicherer Knopf */
            if (bestaetigen_aktiv()) { hinweis_bis = 0; x_seit = 0; }   /* NEU 1.0.8 (4.4): die Seite beginnt ohne alte Meldung */
            if (G.art == D_FERTIG) { hinweis_bis = 0; loeschseite = false; lz_n = 0; nach_loeschen = true; }   /* 4.5; Codex-Prüfung 76: danach sucht der Kern neu - keine alten Einträge mehr zeigen */
            if (G.art == D_WAHL || G.art == D_FEHLER) hinweis_bis = 0;   /* NEU 1.0.8 (K5): neue Frage - eine alte Meldung (z. B. "Testmodus aus ...") läge sonst auf der Karte */
#ifdef MITSCHNITT
            ms_taste();                            /* NEU 1.0.8 (2.7): jede neue Frage als Bild */
#endif
        }
        if (kreis_seit) { if (SDL_GetTicks() - bild_zeit >= 33) neu_zeichnen(); if (SDL_GetTicks() - kreis_seit >= 1000) { kreis_seit = 0; kreis_gehalten(); neu_zeichnen(); } }
        fern_befehle();                                                   /* NEU 1.0.9: Befehle aus dem Browser */
        halten_pruefen();                                                 /* NEU 1.0.8 (4.4): ✕ lange genug gehalten? */
        upd_anzeigen();                                                   /* NEU 1.0.9: Update-Hinweis einmal als kurze Meldung */
        /* NEU 1.0.8 (Arbeitsplan 4.7): nur neu zeichnen, wenn sich etwas ändert. Bisher lief bei jeder Frage und bei "Fertig" alle 60 ms ein
         * ganzes Bild, solange der Arbeitsfaden auf die Antwort wartete - dort bewegt sich aber nichts (Halten, ○ und Laufschrift zeichnen
         * selbst). Regelmäßig gezeichnet wird nur noch auf den Arbeits-Seiten ("Nachschauen läuft", "Löschen läuft": Balken, Zeit). */
        Uint32 jetzt = SDL_GetTicks();
        if (__atomic_load_n(&neu_bald, __ATOMIC_ACQUIRE) && jetzt - bild_zeit >= 100) neu_zeichnen();   /* gesammelte Änderungen aus den Fäden */
        if (G.beschaeftigt && G.art == D_KEINER && jetzt - bild_zeit >= 60) neu_zeichnen();
        if (hinweis_bis && (Sint32)(jetzt - hinweis_bis) >= 0) { hinweis_bis = 0; neu_zeichnen(); }   /* kurze Meldung abgelaufen: einmal ohne sie zeichnen */
        if (lauf_aktiv && (Sint32)(jetzt - lauf_naechst) >= 0 && jetzt - bild_zeit >= 33) neu_zeichnen();   /* NEU 1.0.6: Laufschrift (1.0.8: nicht in den Pausen) */
        if (nach_aktion && !G.beschaeftigt && G.art == D_KEINER) {         /* NEU 1.0.6: Auswahl am Ende der Bereinigung */
            int na = nach_aktion; nach_aktion = 0;
            if (na == 1 && neustart_pflicht) {
                na = 2;
                ui_log(NULL, T("Vollständige Bereinigung - die PS5 startet deshalb jetzt neu.",
                               "Full cleanup - the PS5 therefore restarts now."));
            }
            if (na == 1) {   /* ○ am Ende: Beenden (da_Chris86 02.11.2026) - der Hinweis "neu starten" kommt als Benachrichtigung */
                ui_log(NULL, T("PS5 Cleanup beendet sich. Empfohlen: die PS5 bei Gelegenheit neu starten.", "PS5 Cleanup quits. Recommended: restart the PS5 when convenient."));
                ende_hinweis = 1;
#ifdef VORSCHAU
                ui_log(NULL, "PC-Vorschau: Beenden (das Fenster bleibt für den Rundgang offen)");
#else
                laeuft = false;
#endif
            } else if (K->simulation) {
                const char *t = T("SIMULATION: Hier würde die PS5 jetzt neu gestartet.", "SIMULATION: the PS5 would now restart.");
                ui_log(NULL, t); kurz(t);
            } else if (K->SELBST_UEBERGEBEN && plattform_neustart_uebergeben()) {
                /* NEU 1.0.6 (da_Chris86 29.10.2026 23:08): Kachel + Startordner entfernt der Hintergrundteil erst nach dem Ende der
                 * Oberfläche - ein sofortiger Neustart käme ihm zuvor. Darum startet ER neu, nach der Entfernung. */
                ui_log(NULL, T("PS5 Cleanup beendet sich - die Kachel wird entfernt, danach startet die PS5 neu.", "PS5 Cleanup quits - the tile is removed, then the PS5 restarts."));
                laeuft = false;
            } else if (K->SELBST_UEBERGEBEN) {
                /* Prüfung p8, Befund 1: ein Neustart jetzt käme der Entfernung zuvor - dann bliebe eine Kachel ohne App stehen.
                 * Darum beenden: der Hintergrundteil entfernt Kachel und Startordner, danach selbst neu starten. */
                ui_log(NULL, T("Der Hintergrundteil hat den Neustart nicht übernommen. PS5 Cleanup beendet sich, die Kachel wird entfernt - danach die PS5 bitte selbst neu starten.",
                               "The background part did not take over the restart. PS5 Cleanup quits and the tile is removed - then please restart the PS5 yourself."));
                ende_hinweis = 1;
                laeuft = false;
            } else {
                ui_log(NULL, T("PS5 startet jetzt neu ...", "PS5 is restarting now ..."));
#ifdef MITSCHNITT
                zeichnen(); SDL_UpdateWindowSurface(fenster); ms_ende("NEUSTART");   /* NEU 1.0.8 (2.7): letzte Bilder vor dem Neustart */
#endif
                if (!plattform_neustart()) {
#ifdef MITSCHNITT
                    ms_bilder_ende = false; ms_zeile('I', "WEITER: der Neustart hat nicht geklappt", NULL, NULL);
#endif
                    const char *t = T("Das hat nicht geklappt - bitte die PS5 über das PS5-Menü neu starten.", "That did not work - please restart the PS5 via the PS5 menu."); ui_log(NULL, t); kurz(t);
                }
            }
            neu_zeichnen();
        }
#ifdef VORSCHAU
        skript_schritt();
#endif
        /* Codex-Prüfung 78: beide Merker atomar zurücksetzen, BEVOR gezeichnet wird - was ein anderer Faden währenddessen meldet, kommt ins nächste Bild */
        bool gezeichnet = false;
        if (__atomic_exchange_n(&G.neu, false, __ATOMIC_ACQ_REL)) { __atomic_store_n(&neu_bald, false, __ATOMIC_RELEASE); zeichnen(); SDL_UpdateWindowSurface(fenster); bild_zeit = SDL_GetTicks(); gezeichnet = true; }
        (void)gezeichnet; fern_daten_raus();       /* NEU 1.0.9: Zustand, Liste und Bilder für den Browser */
#ifdef MITSCHNITT
        ms_bild_vielleicht();                      /* NEU 1.0.8 (2.7) */
#endif
        SDL_Delay(16);
    }
    /* NEU 1.0.7 (Plan 1.0.7, Punkt 2): nach ○ am Ende bleibt der Hinweis auch nach dem Beenden sichtbar - als Benachrichtigung.
     * Ist die Selbstentfernung übergeben, hängt ihn der Hintergrundteil an "PS5 Cleanup wurde entfernt." an (eine Meldung,
     * richtige Reihenfolge); sonst kommt er sofort. In der Simulation keine Benachrichtigung. */
    if (ende_hinweis && !K->simulation) {
        const char *hw = T("PS5 Cleanup: Fertig. Empfohlen: die PS5 bei Gelegenheit neu starten.", "PS5 Cleanup: Done. Recommended: restart the PS5 when convenient.");
        if (!(K->SELBST_UEBERGEBEN && plattform_hinweis_uebergeben(ende_hinweis, SP_EN))) plattform_melden(hw);
    }
    fern_stoppen();                                /* NEU 1.0.9: Browser-Bedienung endet mit der App */
    ablauf_hintergrund_anhalten(&A);
#ifdef MITSCHNITT
    ms_ende("BEENDET");                            /* NEU 1.0.8 (2.7): letzte Bilder vor dem Beenden */
#endif
    if (pad) SDL_GameControllerClose(pad);
    SDL_DestroyWindow(fenster);
    SDL_Quit();
    printf("PS5 Cleanup: sauber beendet.\n");
    return 0;
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    setvbuf(stdout, NULL, _IOLBF, 0);
    plattform_start();
#ifdef VORSCHAU
    if (getenv("VORSCHAU_SKRIPT")) skript = fopen(getenv("VORSCHAU_SKRIPT"), "r");
#endif
    int r = programm();
    plattform_ende();
    return r;
}
