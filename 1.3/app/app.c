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
#include <strings.h>   /* NEU 1.1: strcasecmp (Update holen) */
#include <sys/stat.h>
#include <time.h>
#include <math.h>     /* NEU 1.0.8 (Arbeitsplan 4.1): geglättete Ecken, Kreise und Linien */
#ifdef KERN_LOESCHEN
#include <fcntl.h>    /* NEU 02.11.2026 (Prüfung p8, Befund 3): Protokolldatei mit O_NOFOLLOW öffnen */
int close(int);       /* aus unistd.h - das ganze unistd.h nicht, weil app.c eine eigene Funktion dup() hat */
#endif
#include "../kern/ablauf.h"
#include "plattform.h"
#include "upd_suche.h"          /* NEU 1.3: alte Programmdatei für "Update holen - Ja, ersetzen" auch ohne Nachschauen finden */
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
/* NEU 1.1 (Entwurf 15.11.2026): Schrift Inter (OFL-1.1, Teilmenge lateinisch, app/bilder_11) - Zeichen, die Inter nicht hat (✕ ○ △ □,
 * fremde Schriften in Spielnamen), kommen weiter aus DejaVu */
EINBINDEN(schrift_inter, "bilder_11/Inter-Regular.otf")
EINBINDEN(schrift_inter_fett, "bilder_11/Inter-Bold.otf")
EINBINDEN(schrift_inter_halb, "bilder_11/Inter-SemiBold.otf")
EINBINDEN(schrift_inter_extra, "bilder_11/Inter-ExtraBold.otf")
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
#if defined(MITSCHNITT) && defined(BILDER_FASSUNG)   /* NEU 1.1 (da_Chris86 16.11.2026 18:37): Bilder-Fassung - Anzeige wie die Veröffentlichung */
#define VERSION_TEXT "1.3"
#define FENSTER_TITEL "PS5 Cleanup 1.1 Bilder-Fassung · Bau " __DATE__ " " __TIME__   /* nicht sichtbar (Vollbild) - Build-Kennung für die Stick-Prüfung */
#elif defined(MITSCHNITT)   /* NEU 1.0.8 (Arbeitsplan 2.7): Test-Fassung für den Abschlusstest - Bauzeit sichtbar, damit klar ist, welche läuft */
#define VERSION_TEXT "1.3 TESTLAUF · Bau " __DATE__ " " __TIME__
#define VERSION_TEXT_EN "1.3 TEST RUN · Build " __DATE__ " " __TIME__
#define VERSION_BAU "Bau " __DATE__ " " __TIME__          /* NEU 1.1 (da_Chris86 15.11.2026 23:24): Bauzeit auch neben dem Update-Hinweis sichtbar */
#define VERSION_BAU_EN "Build " __DATE__ " " __TIME__
#else
#define VERSION_TEXT "1.3"                /* 02.11.2026 (da_Chris86, finaler Plan 1.0.7 Punkt 7): ohne "Vorschau"; 06.11.2026: 1.0.8 (Arbeitsplan Phase 5); 08.11.2026: 1.0.9; 14.11.2026: 1.1 (da_Chris86: "Die nächste Version heißt 1.1") */
#endif
#else
#define VERSION_TEXT "1.3 TEST · Bau " __DATE__ " " __TIME__   /* Testfassung - ohne Löschbefehle gebaut, löscht nie; 31.10.2026: Bauzeit sichtbar (welche Fassung läuft?) */
#define VERSION_TEXT_EN "1.3 TEST · Build " __DATE__ " " __TIME__   /* NEU 1.0.8: englische Oberfläche "Build" statt "Bau" */
#define VERSION_BAU "Bau " __DATE__ " " __TIME__
#define VERSION_BAU_EN "Build " __DATE__ " " __TIME__
#endif
#endif
#ifndef VERSION_TEXT_EN
#define VERSION_TEXT_EN VERSION_TEXT
#endif
#ifndef FENSTER_TITEL
#define FENSTER_TITEL "PS5 Cleanup " VERSION_TEXT
#endif
#define VERSION_ANZEIGE (SP_EN ? VERSION_TEXT_EN : VERSION_TEXT)   /* NEU 1.0.8: Kopf, Protokoll-Datei und Absturzmeldung in der eingestellten Sprache */
#ifdef VERSION_TEST   /* NEU 1.1: nur für die Update-Test-ELF (bauen.sh mit VERSION_TEST=..., nur testlauf) - gibt sich als ältere Version aus */
#define VERSION_NUMMER VERSION_TEST
#else
#define VERSION_NUMMER "1.3"  /* NEU 1.0.9: nur die Nummer - damit vergleicht der Update-Hinweis (in allen Fassungen gleich) */
#endif

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
static stbtt_fontinfo font[6];   /* NEU 1.1: 0 Inter, 1 Inter fett, 2 Inter halbfett, 3 Inter extrafett, 4/5 DejaVu (Ersatz für fehlende Zeichen) */
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
            /* NEU 1.1: Größe wie im Entwurf (CSS font-size = Höhe des Gevierts); fehlt das Zeichen in Inter, aus DejaVu */
            int q = f;
            if (q < 4 && cp != ' ' && stbtt_FindGlyphIndex(&font[q], cp) == 0) q = (f == 0 || f == 2) ? 4 : 5;
            stbtt_fontinfo *fi = &font[q];
            float s = stbtt_ScaleForMappingEmToPixels(fi, (float)groesse);
            int adv, lsb;
            g->belegt = 1; g->f = f; g->groesse = groesse; g->cp = cp;
            g->bild = stbtt_GetCodepointBitmap(fi, s, s, cp, &g->w, &g->h, &g->xoff, &g->yoff);
            if (!g->bild) { g->w = 0; g->h = 0; }   /* 1.0.6 Fix: ohne Bild (z. B. kein Speicher) nichts zeichnen statt abstürzen */
            stbtt_GetCodepointHMetrics(fi, cp, &adv, &lsb);
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
#define N_HG         0x0D1119   /* Hintergrund - NEU 1.1: alle N_-Farben nach da_Chris86s Palette (Entwurf 15.11.2026) */
#define N_KARTE      0x19232E   /* Karten */
#define N_RAND       0x2B3746   /* Rand der Karten */
#define N_FELD       0x222C38   /* Symbolfeld, Abzeichen, Knopf "aus" */
#define N_TEXT       0xE8EDF5
#define N_TEXT2      0xC7D2E3
#define N_GRAU       0xA3AEC2
#define N_GRAU2      0x8B9AB3
#define N_LINIE      0x3B4A63
#define N_BLAU       0x2D73D6   /* Hauptknopf, erledigte Schritte */
#define N_FOKUS      0x36D4DC   /* türkise Markierung */
#define N_ROT        0xFF7B76
#define N_ROT_KNOPF  0xC8423E
#define N_GELB       0xF5B942
#define N_GRUEN      0x34D399
#define N_ORANGE     0xF5B942
#define N_FUSS       0x0D1119
#define N_FUSS_LINIE 0x0D1119
#define N_BALKEN     0x5B6B82   /* belegt */
#define N_BALKEN_FREI 0x2A3644
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
#include "stil_11.h"   /* NEU 1.1: Bausteine der neuen Oberfläche (Farben, Milchglas, Verläufe, Bilder) */
/* NEU 1.1: die Karten der Seiten von 1.0.8 bekommen den neuen Stil - Karte = Milchglas, Markierung = weicher blauer Verlauf */
static void karte_rund_neu(int x, int y, int w, int h, int r, int d, uint32_t rand, uint32_t innen)
{
    if (rand == N_FOKUS && d >= 2) { fokus11(x, y, w, h, r, 2); return; }
    if (innen == N_KARTE && (rand == N_RAND || rand == N_LINIE)) { if (w > 240 && h > 120) karte11(x, y, w, h, r); else { flaeche f = F_NEBEN11(); f.d = d; flaeche_zeichnen(x, y, w, h, r, &f); } return; }
    karte_rund(x, y, w, h, r, d, rand, innen);
}
#define karte_rund karte_rund_neu

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
    if (b.b64) {                                                /* NEU 1.1: Bild aus dem Katalog eines Payload-Verwalters (base64, PNG oder JPEG) */
        unsigned char *t = datei_stueck(b.pfad, b.ab, (size_t)b.len);
        if (t) {   /* Codex-Prüfung 335: hat sich der Katalog seit der Suche geändert, kein Bild */
            unsigned char sh[32]; char hx[65];
            kern_sha256(t, (size_t)b.len, sh); for (int i = 0; i < 32; i++) snprintf(hx + 2 * i, 3, "%02x", sh[i]);
            if (strcmp(hx, b.b64sum)) { free(t); t = NULL; }
        }
        d = t ? kern_bild_b64(t, (size_t)b.len, &m) : NULL;
        free(t);
        if (d && !(m >= 8 && (!memcmp(d, "\x89PNG", 4) || (d[0] == 0xFF && d[1] == 0xD8)))) { free(d); d = NULL; }
        if (d) *n = m;
        return d;
    }
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
/* NEU 1.1 (PS5-Test 16.11.2026 18:15, Patch Manager: Kachelbild nur Schwarz, sein Download hat ein Bild): gemerkt, zu welcher Kennung
 * sich KEIN Bild laden ließ - dann nimmt block_bild das Bild eines Teils. Nur Hauptfaden (Fernseher und fern_bilder). */
#define BILD_FEHLT_MAX 256
static char bild_fehlt_id[BILD_FEHLT_MAX][16]; static int bild_fehlt_n;
/* NEU 1.3 ("Erst messen, dann beschleunigen"): Zeiten fürs Protokoll - Tastendruck bis Bild, Zeichnen, Browser-Daten, Kachelbilder.
 * Nur gezählt; die Zusammenfassung schreibt mess_bericht (nach dem Nachschauen, beim Speichern des Protokolls und am Ende). */
static uint64_t mess_ms(void) { struct timespec t; if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) return 0; return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u; }
typedef struct { unsigned n, langsam; uint64_t summe, max; } mess_wert;
static mess_wert MW_TASTE, MW_BILD, MW_FERN, MW_KACHEL;
static pthread_mutex_t mess_m = PTHREAD_MUTEX_INITIALIZER;
static uint64_t mess_start = 0, mess_taste_ab = 0; static bool mess_erstes_bild = false;
static void mess_dazu(mess_wert *w, uint64_t d, uint64_t grenze)
{
    pthread_mutex_lock(&mess_m);
    w->n++; w->summe += d; if (d > w->max) w->max = d; if (d > grenze) w->langsam++;
    pthread_mutex_unlock(&mess_m);
}
static bool bild_fehlt(const char *id) { for (int i = 0; id && i < bild_fehlt_n; i++) if (!strcmp(bild_fehlt_id[i], id)) return true; return false; }
static unsigned char *kachelbild_laden_innen(const char *id, int *w, int *h);
static unsigned char *kachelbild_laden(const char *id, int *w, int *h)
{
    uint64_t mess_a = mess_ms();
    unsigned char *px = kachelbild_laden_innen(id, w, h);
    mess_dazu(&MW_KACHEL, mess_ms() - mess_a, 100);   /* NEU 1.3 */
#ifdef VORSCHAU   /* NEU 1.3 (nur PC-Vorschau): welches Bild sich nicht laden ließ - für die Bildprüfung am PC */
    if (!px && !bild_fehlt(id)) { const char *bo = K && mt_hat(&K->BILD_ORT, id) ? (const char *)mt_hole(&K->BILD_ORT, id) : ""; char kz[200]; snprintf(kz, sizeof kz, "%s", bo); for (char *c = kz; *c; c++) if (*c == '\n') *c = '|'; printf("VORSCHAU kein Bild: %s [%s]\n", id, kz); }
#endif
    if (!px && strlen(id) < 16 && !bild_fehlt(id) && bild_fehlt_n < BILD_FEHLT_MAX) snprintf(bild_fehlt_id[bild_fehlt_n++], 16, "%s", id);
    return px;
}
static unsigned char *kachelbild_laden_innen(const char *id, int *w, int *h)
{
    /* NEU 1.3 (Punkt 5): Playdens Bilderspeicher /user/data/playden/art (= /data/playden/art) - Steam-Spiele als <Nummer>_600x900.jpg (Titelbild), <Nummer>_logo.png
     * (Quelle: playden-ps5 src/homebrew/artwork_backend.cpp CacheFileName, src/library/store.cpp ToKey). Nur lesen. */
    if (!strncmp(id, "PLD", 3) && id[3] >= '0' && id[3] <= '9') {   /* PS5-Test 19.11.2026: nicht PLDM00001 (Payload Manager) */
        static const char *const NAMEN[2] = { "%s_600x900.jpg", "%s_logo.png" };
        for (int i = 0; i < 2; i++) {
            char nm[40], p[96]; snprintf(nm, sizeof nm, NAMEN[i], id + 3); snprintf(p, sizeof p, "/user/data/playden/art/%s", nm);
            size_t n = 0; unsigned char *d = datei_lesen(p, &n);
            if (!d) continue;
            unsigned char *px = bild_entpacken(d, n, w, h);
            if (px) return px;
        }
        return NULL;
    }
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
/* 1.3: mit eigener Arena - auch aus dem Arbeitsfaden (erg_erfassen); bild_kennung(f) nimmt wie bisher die Arena der Oberfläche */
static const char *bild_kennung_in(arena *ba, const eintrag *f)
{
    if (f->KachelId && f->KachelId[0]) return f->KachelId;
    if (f->SmDatei && f->SmKachel && f->SmKachel->KachelId) return f->SmKachel->KachelId;
    /* NEU 1.1 (da_Chris86 16.11.2026 15:54: "die Kacheln könnte man auch mit Bild machen, bei Save Games zumindest"): Spielstände eines
     * Playden-Spiels zeigen das Bild seiner Kachel (FAKE78xxx, gesetzt in kern_suche.c playden_spiele_innen) */
    if (f->PlSpst && f->PlSpst[0]) return f->PlSpst;
    /* NEU 1.3 (Punkt 5, da_Chris86 17.11.2026): Spielstände eines gelöschten Playden-Spiels (PlSpst "", Ordner games/steam-<Nummer>.kept bzw.
     * .saves) - das Bild aus Playdens eigenem Bilderspeicher (kachelbild_laden_innen "PLD<Steam-Nummer>") */
    if (f->PlSpst) for (int j = 0; j < f->PfadeBasis.n; j++) {
        htreffer mp = h_mt(ba, f->PfadeBasis.e[j], "/playden/games/steam-(\\d{1,8})(?:\\.kept|\\.saves)?(?:/|$)");
        if (mp.ok) return a_fmt(ba, "PLD%s", mp.gr[1]);
    }
    const char *tx = f->TextBasis ? f->TextBasis : (f->Text ? f->Text : "");
    /* NEU 1.0.8 (Codex-Prüfungen 111 bis 113): Spieldatei oder Spiel-Ordner ohne Kachel - die Kennung, zu der der gelesene Name gehört
     * ("[Spiel: Name (PPSA…)]", kern_suche.c spiel_zusatz), geht vor jeder Nummer im Dateinamen und vor der Kachel eines Blocks, unter dem
     * die Datei steht. Nur bei Spieldateien; genommen wird der letzte solche Zusatz (er steht hinter dem Dateinamen), er endet vor dem
     * nächsten "  [" oder am Textende - im Namen selbst steht nie "  [" (spiel_zusatz fasst Leerzeichen zusammen) */
    htreffer m;
    if (f->Art == ART_ABBILD) {
        m = h_mt(ba, tx, "^.*\\[(?:Spiel|game): .*?\\(([A-Z]{4}\\d{5})\\)\\](?=  \\[|\\s*$)");   /* PS5-Test 17:33: auch FAKE77031 (Playden) */
        if (m.ok) return m.gr[1];
        /* NEU 1.0.10 (da_Chris86 14.11.2026 07:58, PS3-Spiele als ISO ohne Bild): die Kennung aus dem Dateinamen, groß und ohne Trennzeichen - so legt die Suche das Bild ab (kern_suche.c iso_ps3) */
        /* Codex-Pruefung 262: nur PS3-ISOs, Kennung aus dem Dateinamen selbst (nicht aus einem Ordnernamen davor) */
        const char *ps3b = (strstr(tx, "[PS3-Spiel, ISO]") || strstr(tx, "[PS3 game, ISO]")) && f->PfadeBasis.n > 0 ? strrchr(f->PfadeBasis.e[0], 47) : NULL;
        m = h_mt(ba, ps3b ? ps3b : "", "(?i)(?:^|[^a-z])(BL[EUJAK][SM]|BC[EUJAK]S|NP[EUJH][AB])[-_]?(\\d{5})(?!\\d)");
        if (m.ok) {
            char *kid = (char *)a_fmt(ba, "%s%s", m.gr[1], m.gr[2]);
            for (char *c = kid; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 'a' + 'A');
            return kid;
        }
    }
    if (f->ToolKachel && f->ToolKachel->KachelId) return f->ToolKachel->KachelId;
    m = h_mt(ba, tx, "\\b((?:CUSA|PPSA)\\d{5})\\b");
    if (m.ok) return m.gr[1];
    if (h_m(tx, "\\bPSCL0000[12]\\b")) return "PSCL00001";   /* NEU 1.0.8 (F4): Aufnahmen von PS5 Cleanup - das Bild der eigenen Kachel */
    m = h_mt(ba, tx, "\\b(NPWR\\d{5}_\\d{2})\\b");   /* Trophaeen: Kachel des Spiels (npbind.dat) */
    if (m.ok && K && mt_hat(&K->NPWR_KACHEL, m.gr[1])) return (const char *)mt_hole(&K->NPWR_KACHEL, m.gr[1]);
    if (m.ok) return m.gr[1];   /* NEU 1.0.8 (PS5-Test 07.11.2026): ohne Kachel das Bild aus der Trophäen-Datei (kachelbild_laden) */
    /* NEU 1.0.8 (F5): steht die Kennung nicht im Text (z. B. "Grand Theft Auto V – Spielstände"), dann die aus dem Ordner (savedata/CUSA…) -
     * nur wenn alle Ordner dieselbe Kennung haben (ein Sammel-Eintrag mehrerer Spiele bekommt kein Bild eines davon) */
    const char *pid = NULL;
    for (int j = 0; j < f->PfadeBasis.n; j++) {
        m = h_mt(ba, f->PfadeBasis.e[j], "/((?:CUSA|PPSA)\\d{5})(?:/|$)");
        /* NEU 1.0.9 (da_Chris86 08.11.2026, Bild aus 1.0.8: "Leere Aufnahme-Zwischenordner" mit PPSA01650 und PSCL00002 zeigte das Bild
         * von Y2JB): ein Ordner mit einer anderen Kennung (PS5 Cleanup, Homebrew, Systemprogramm) gehört zu einem anderen Programm - dann
         * hier kein Bild (bis 1.0.8 wurde er übersprungen). Kachel, Spielname, Tool und Trophäen oben gehen weiter vor. */
        if (!m.ok) { if (h_m(f->PfadeBasis.e[j], "/[A-Z]{4}\\d{5}(?:/|$)")) return NULL; continue; }
        if (pid && strcmp(pid, m.gr[1])) return NULL;
        pid = m.gr[1];
    }
    /* NEU 1.0.10 (da_Chris86 14.11.2026 13:13: "die bekommen doch, wenn sie entpackt werden, ein Symbol"): Programm ohne Kachel - das Kachelbild aus seiner ELF-Datei (kern_suche.c elf_bild_merken) */
    if (!pid && K && f->PfadeBasis.n == 1) {
        const char *ks = kern_elf_bildschluessel(ba, f->PfadeBasis.e[0]);
        /* Codex-Prüfung 264: nur wenn das gemerkte Bild wirklich zu genau dieser Datei gehört (gleicher Schlüssel bei anderem Pfad);
         * NEU 1.1: das Bild kann auch aus dem Katalog eines Payload-Verwalters stammen - wem es gehört, steht in ELF_BILD_VON */
        const char *von = mt_hat(&K->ELF_BILD_VON, ks) ? (const char *)mt_hole(&K->ELF_BILD_VON, ks) : NULL;
        if (von && mt_hat(&K->BILD_ORT, ks) && !strcmp(von, f->PfadeBasis.e[0])) return ks;
    }
    /* NEU 1.3 (da_Chris86 21.11.2026 16:00: "shadowmount – Einstellungen", "gt7p" und Hochgeladenes im Payload Manager ohne Bild): das Bild der
     * Programmdatei, zu der der Kern den Eintrag zählt (BildDatei) - für jeden Daten-Ordner gleich, keine Liste */
    if (!pid && K && f->BildDatei) {
        const char *ks = kern_elf_bildschluessel(ba, f->BildDatei);
        const char *von = mt_hat(&K->ELF_BILD_VON, ks) ? (const char *)mt_hole(&K->ELF_BILD_VON, ks) : NULL;
        if (von && mt_hat(&K->BILD_ORT, ks) && !strcmp(von, f->BildDatei)) return ks;
    }
    return pid;
}
/* NEU 1.3 (da_Chris86 21.11.2026 15:24, PS5-Test: Spielstand von ASTRO's PLAYROOM ohne Bild - das Spiel ist weg, Sony hat beim Spielstand kein
 * Bild abgelegt, aber die Trophäen desselben Spiels haben eins): der Spielname eines Eintrags ohne den Zusatz "nicht mehr auf der Konsole" */
static const char *bild_spielname(const eintrag *f)
{
    const char *n = kern_name(fa, f), *z = T(", nicht mehr auf der Konsole", ", no longer on the console");
    if (!n) return "";
    size_t ln = strlen(n), lz = strlen(z);
    return (ln > lz && !strcmp(n + ln - lz, z)) ? a_kopie_n(fa, n, ln - lz) : n;
}
/* Ließ sich das eigene Bild nicht laden (bild_fehlt), dann das Bild eines anderen Eintrags mit genau demselben Spielnamen - in beide
 * Richtungen (Spielstand, Trophäen, Aufnahmen ...), für jedes Spiel gleich. Gibt es auf der Konsole kein Bild mehr, bleibt es bei den
 * Anfangsbuchstaben. */
static const char *bild_kennung(const eintrag *f)
{
    const char *id = bild_kennung_in(fa, f);
    if (!K || (id && !bild_fehlt(id))) return id;
    if (f->Kat != 2 && f->Kat != 3 && f->Kat != 4) return id;   /* Spielstände, Trophäen, Bilder & Videos: Daten eines Spiels ohne eigene Kachel */
    const char *n = bild_spielname(f);
    if (strlen(n) < 2) return id;
    for (int i = 0; i < K->Funde.n; i++) {
        const eintrag *g = K->Funde.e[i]; if (!g || g == f) continue;
        const char *gid = bild_kennung_in(fa, g);
        if (!gid || bild_fehlt(gid) || (id && !strcmp(gid, id))) continue;
        if (!strcmp(bild_spielname(g), n)) return gid;
    }
    return id;
}
/* NEU 1.1 (PS5-Test 15.11.2026 22:21, Orbit Store: Block ohne Kachel, aber mit Programmdatei): das Bild eines Blocks - das seines Kopfs,
 * hat der keins, das erste Bild eines seiner Teile (z. B. aus der ELF-Datei). Für alle Blöcke gleich, keine Sonderregel je App. */
static const eintrag *block_bild(const kblock *bl)
{
    if (!bl || !bl->kopf) return NULL;
    const char *kb = bild_kennung(bl->kopf);
    if (kb && !bild_fehlt(kb)) return bl->kopf;   /* NEU 1.1: ließ sich das Bild des Kopfs nicht laden, das erste Bild eines Teils */
    for (int j = 0; j < bl->teile.n; j++) { const char *tb = bl->teile.e[j] ? bild_kennung(bl->teile.e[j]) : NULL; if (tb && !bild_fehlt(tb)) return bl->teile.e[j]; }
    return bl->kopf;
}
static void neu_zeichnen(void);
static SDL_Surface *kachelbild_id(const char *id, int s);
static SDL_Surface *kachelbild(const eintrag *f, int s)   /* s = 44 (Zeile) oder 180 (Kasten); 1.0.8: Grenze 64 statt 40 */
{
    return kachelbild_id(bild_kennung(f), s);
}
/* 1.3: dasselbe nach Kennung (Ergebnisseite - die Einträge gibt es dort nicht mehr) */
static SDL_Surface *kachelbild_id(const char *id, int s)
{
    if (!id || strlen(id) >= sizeof gross_id) return NULL;
    if (s > 96) {   /* 1.1: Zeilenbilder sind 76 groß (Entwurf) - bis 96 im kleinen Speicher */
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
/* NEU 1.3 (PS5-Test 21.11.2026: beim Suchen gingen 28 Lesezugriffe verloren, der PC-Nachbau stimmte darum nicht): ist der Puffer voll,
 * wartet der Aufrufer bis zu 10 s, bis der Schreibfaden Platz gemacht hat - erst wenn das Laufwerk wirklich hängt, gehen Zeilen verloren.
 * Danach wird nicht bei jeder Zeile wieder 10 s gewartet (ms_stockt), bis wieder ein Schreiben geklappt hat. */
static bool ms_stockt;
/* Codex-Prüfung 75: eine Aufzeichnung (z. B. eine Ordnerliste) kommt mit allen Folgezeilen in einem Aufruf - gezählt werden die Zeilen */
static long long ms_zeilen(const char *z, size_t n) { long long k = 0; for (size_t i = 0; i < n; i++) if (z[i] == '\n') k++; return k ? k : 1; }
/* zwingend: die Schlusszeile (ENDE) kommt auch in einen vollen Puffer - sie ist kurz */
static void ms_schreib_x(const char *z, bool zwingend)
{
    size_t n = strlen(z);
    pthread_mutex_lock(&ms_m);
    if (ms_f && !ms_aus) {
        if (ms_puf_n + n > MS_PUF_MAX && !zwingend && !ms_stockt) {
            struct timespec bis; clock_gettime(CLOCK_REALTIME, &bis); bis.tv_sec += 10;
            while (ms_f && !ms_aus && ms_puf_n > 0 && ms_puf_n + n > MS_PUF_MAX)
                if (pthread_cond_timedwait(&ms_mc, &ms_m, &bis) == ETIMEDOUT) { ms_stockt = true; break; }
        }
        if (!ms_f || ms_aus) { pthread_mutex_unlock(&ms_m); return; }
        if (ms_puf_n > 0 && ms_puf_n + n > MS_PUF_MAX && !zwingend) ms_verloren += ms_zeilen(z, n);
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
        else if (f && !aus && n > 0) ms_stockt = false;
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
static void erg_grund_merken(const char *t);
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
    fern_zeile(t);                                 /* NEU 1.0.9: auch für die Seite "Protokoll" im Browser (NEU 1.3: unter G.m - dieselbe Reihenfolge wie G.log, auch wenn zwei Fäden gleichzeitig schreiben) */
    pthread_mutex_unlock(&G.m);
    printf("PS5 Cleanup: %s\n", t ? t : ""); fflush(stdout);
    plattform_protokoll_zeile(t);                  /* NEU 1.0.7: auch ins Kernel-Protokoll */
    erg_grund_merken(t);                           /* NEU 1.1: Grund eines unveränderten oder nicht ganz gelöschten Eintrags für die Ergebnisseite */
#ifdef MITSCHNITT
    ms_zeile('P', t ? t : "", NULL, NULL);         /* NEU 1.0.8 (2.7): jede Zeile des Protokolls auch in den Mitschnitt */
#endif
    neu_zeichnen_bald();                           /* 1.0.8 (4.7) */
}
static void ui_meldung(const char *t) { ui_log(NULL, t); }   /* NEU 1.0.7: Zeilen der Plattform (Sony-Deinstallation) */
/* Protokoll auf den USB-Stick: neue Datei mit Datum und Uhrzeit, danach wird jede Zeile dort angehängt.
 * Rückgabe true = gespeichert (Meldung in m). Nie überschreiben: gibt es den Namen schon, wird nichts geschrieben. */
/* NEU 1.3: Zusammenfassung der Messwerte seit dem Start ins Protokoll */
static void mess_bericht(void)
{
    mess_wert t, b, f, k;
    pthread_mutex_lock(&mess_m); t = MW_TASTE; b = MW_BILD; f = MW_FERN; k = MW_KACHEL; pthread_mutex_unlock(&mess_m);
    char z[300];
    #define MW_MITTEL(w) ((unsigned long long)((w).n ? (w).summe / (w).n : 0))
    if (t.n) { snprintf(z, sizeof z, T("Zeit: Tastendruck bis Bild %u mal, Mittel %llu ms, längstens %llu ms, über 100 ms: %u mal",
                                       "Time: key press to frame %u times, average %llu ms, longest %llu ms, over 100 ms: %u times"),
                        t.n, MW_MITTEL(t), (unsigned long long)t.max, t.langsam); ui_log(NULL, z); }
    if (b.n) { snprintf(z, sizeof z, T("Zeit: Bild zeichnen %u mal, Mittel %llu ms, längstens %llu ms, über 50 ms: %u mal",
                                       "Time: drawing a frame %u times, average %llu ms, longest %llu ms, over 50 ms: %u times"),
                        b.n, MW_MITTEL(b), (unsigned long long)b.max, b.langsam); ui_log(NULL, z); }
    if (f.n) { snprintf(z, sizeof z, T("Zeit: Daten für den Browser %u mal, Mittel %llu ms, längstens %llu ms, über 50 ms: %u mal",
                                       "Time: data for the browser %u times, average %llu ms, longest %llu ms, over 50 ms: %u times"),
                        f.n, MW_MITTEL(f), (unsigned long long)f.max, f.langsam); ui_log(NULL, z); }
    if (k.n) { snprintf(z, sizeof z, T("Zeit: Kachelbilder geladen %u mal, zusammen %llu ms, längstens %llu ms, über 100 ms: %u mal",
                                       "Time: tile images loaded %u times, total %llu ms, longest %llu ms, over 100 ms: %u times"),
                        k.n, (unsigned long long)k.summe, (unsigned long long)k.max, k.langsam); ui_log(NULL, z); }
    #undef MW_MITTEL
}
static bool protokoll_auf_stick_innen(char *m, size_t ml);
static bool protokoll_auf_stick(char *m, size_t ml)   /* Prüfung p8, Befund 7: Haupt- und Arbeitsfaden nie gleichzeitig */
{
    mess_bericht();   /* NEU 1.3 */
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
static const char *ohne_ort_vorne(const char *t);
/* NEU 1.1 (Entwurf "5 – Fertig"): Ergebnis je Eintrag - erfasst im Arbeitsfaden bei jeder Fortschrittsmeldung (fortschritt() setzt
 * A.lf_* direkt davor), so fehlt keiner. Ob der vorige mit Fehler endete bzw. unverändert blieb, zeigen die Zähler des Kerns. */
/* zustand: 0 läuft, 1 erledigt, 2 Fehler, 3 unverändert (auch: nach "Anhalten" nicht mehr drangekommen), 4 vom PS4-Schutz zurückgehalten.
 * schl: woran der Eintrag nach dem nächsten Nachschauen wiederzuerkennen ist (erster Pfad, sonst die Kachel) - nicht der Anzeigename, der bei
 * Spielständen verschiedener Benutzer gleich ist (Codex-Prüfung 339, P1) */
/* NEU 1.1 (PS5-Test da_Chris86 17.11.2026: auf der Ergebnisseite stand nur "unverändert", der Grund nur im Protokoll): text = Zeile des Kerns
 * (f->Text, so steht sie in seiner Meldung), grund = was der Kern in Klammern dahinter nennt */
/* 1.3 (PS5-Test da_Chris86 20.11.2026 21:33: Ergebnis zeigte Kacheln nur mit Buchstaben): bild = Kennung des Kachelbilds (bild_kennung_in) */
typedef struct { char name[160], schl[320], text[400], grund[240], bild[24]; long long b; bool hat_b; int kat, zustand; } erg_zeile;
#define ERG_MAX 4000
static erg_zeile *erg; static int erg_n = 0, erg_nr = 0, erg_fe = 0, erg_un = 0, erg_gs = 0;
static pthread_mutex_t erg_m = PTHREAD_MUTEX_INITIALIZER;
static const char *erg_schluessel(arena *x, const eintrag *f)
{
    if (f->Pfade.n > 0 && f->Pfade.e[0] && f->Pfade.e[0][0]) return f->Pfade.e[0];
    if (f->KachelId && f->KachelId[0]) return VERB(x, "kachel:", f->KachelId);
    return VERB(x, "name:", ohne_ort_vorne(kern_zeile(x, f)));
}
static void erg_abschliessen(int fe, int un, int gs)   /* unter erg_m; gs: vom PS4-Schutz zurückgehalten (Codex-Prüfung 339, P2) */
{
    if (erg_n > 0 && erg[erg_n - 1].zustand == 0) erg[erg_n - 1].zustand = fe > erg_fe ? 2 : gs > erg_gs ? 4 : un > erg_un ? 3 : 1;
#ifdef VORSCHAU   /* PC-Vorschau: Eintrag Nr. VORSCHAU_ERG_FEHLER als Fehler zeigen (Seite "Nicht alles gelöscht") */
    { const char *e = getenv("VORSCHAU_ERG_FEHLER"); if (e && erg_n == atoi(e)) erg[erg_n - 1].zustand = 2; }
#endif
    erg_fe = fe; erg_un = un; erg_gs = gs;
}
/* NEU 1.1: Meldet der Kern einen Eintrag als unverändert, teilweise oder mit Fehler ("  unverändert   – <Zeile>   (Grund)"), kommt der Grund in
 * die Ergebniszeile, deren Kern-Zeile in der Meldung steht. Nur Anzeige - Zählung und Zustand bleiben, wie der Kern sie meldet. */
static int erg_wartet = -1;   /* Codex-Prüfung 357 (P3): Zeile, deren Grund erst in der folgenden "übrig:"-Zeile kommt */
static void erg_grund_merken(const char *t)
{
    if (!t) return;
    const char *v = t; while (*v == ' ') v++;
    if (!strncmp(v, "übrig: ", 8) || !strncmp(v, "left: ", 6)) {
        pthread_mutex_lock(&erg_m);
        if (erg && erg_wartet >= 0 && erg_wartet < erg_n && !erg[erg_wartet].grund[0]) {
            const char *r = v + (v[0] == 'l' ? 6 : 8); size_t n = strlen(r); while (n && (r[n - 1] == ' ' || r[n - 1] == '\n')) n--;
            if (n >= sizeof erg[0].grund) { n = sizeof erg[0].grund - 1; while (n && ((unsigned char)r[n] & 0xC0) == 0x80) n--; }
            if (n) { memcpy(erg[erg_wartet].grund, r, n); erg[erg_wartet].grund[n] = 0; }
        }
        erg_wartet = -1;
        pthread_mutex_unlock(&erg_m);
        return;
    }
    static const char *const WORT[] = { "unverändert", "unchanged", "TEILWEISE", "PARTLY", "FEHLER", "ERROR" };
    bool passt = false; for (size_t w = 0; w < sizeof WORT / sizeof WORT[0] && !passt; w++) passt = !strncmp(v, WORT[w], strlen(WORT[w]));
    if (!passt) return;
    pthread_mutex_lock(&erg_m);
    erg_wartet = -1;
    for (int i = erg_n - 1; erg && i >= 0 && i >= erg_n - 400; i--) {
        /* Codex-Prüfung 354: ein schon gemerkter Grund wird nie überschrieben (gleich benannte Spielstände, "Anhalten"), und der Grund ist alles
         * hinter dem Namen - mit oder ohne Klammer, mit einem oder drei Leerzeichen davor */
        if (!erg[i].text[0] || erg[i].grund[0]) continue;
        const char *p = strstr(v, erg[i].text); if (!p) continue;
        const char *r = p + strlen(erg[i].text); while (*r == ' ') r++;
        size_t n = strlen(r); while (n && (r[n - 1] == ' ' || r[n - 1] == '\n')) n--;
        if (n >= 2 && r[0] == '(' && r[n - 1] == ')') {   /* äußere Klammer nur weg, wenn sie wirklich den ganzen Grund umschließt */
            int tiefe = 0; size_t j = 0; for (; j < n; j++) { if (r[j] == '(') tiefe++; else if (r[j] == ')' && --tiefe == 0) break; }
            if (j == n - 1) { r++; n -= 2; }
        }
        if (n >= sizeof erg[i].grund) { n = sizeof erg[i].grund - 1; while (n && ((unsigned char)r[n] & 0xC0) == 0x80) n--; }
        if (n) { memcpy(erg[i].grund, r, n); erg[i].grund[n] = 0; } else erg_wartet = i;
        break;
    }
    pthread_mutex_unlock(&erg_m);
}
static void erg_erfassen(void)   /* Arbeitsfaden */
{
    int nr = A.lf_nr; const eintrag *f = A.lf_f;
    if (!f || nr <= 0 || nr == erg_nr) return;
    pthread_mutex_lock(&erg_m);
    if (nr == 1 || nr < erg_nr) { erg_n = 0; erg_fe = A.lf_fehler; erg_un = A.lf_unv; erg_gs = K->PS4_GESCHUETZT; }   /* neuer Löschlauf */
    else erg_abschliessen(A.lf_fehler, A.lf_unv, K->PS4_GESCHUETZT);
    if (!erg) erg = calloc(ERG_MAX, sizeof *erg);
    if (erg && erg_n < ERG_MAX) {
        erg_zeile *e = &erg[erg_n++];
        arena *x = arena_neu(); snprintf(e->name, sizeof e->name, "%s", ohne_ort_vorne(kern_zeile(x, f))); snprintf(e->schl, sizeof e->schl, "%s", erg_schluessel(x, f));
        /* Codex-Prüfung 357 (P3): passt der Name nicht ganz hinein, wird nicht abgeglichen - sonst begänne der Grund mitten im Namen */
        if (f->Text && strlen(f->Text) < sizeof e->text) snprintf(e->text, sizeof e->text, "%s", f->Text); else e->text[0] = 0;
        e->grund[0] = 0;
        { const char *bk = bild_kennung_in(x, f); snprintf(e->bild, sizeof e->bild, "%s", bk ? bk : ""); }   /* 1.3 */
        arena_frei(x);
        e->b = f->Bytes; e->hat_b = f->HatGroesse; e->kat = f->Kat; e->zustand = 0;
    }
    erg_nr = nr;
    pthread_mutex_unlock(&erg_m);
}
static void ui_stand(void *c, const char *t, double p)
{
    (void)c;
    if (A.loescht) erg_erfassen();   /* NEU 1.1 */
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
    pthread_mutex_lock(&erg_m);   /* NEU 1.1 */
    erg_abschliessen((int)K->FEHLER_ZAHL, K->UNVERAENDERT.n, K->PS4_GESCHUETZT); erg_nr = 0;
    if (K->anhalten) {   /* Codex-Prüfung 339 (P2): nach "Anhalten" kommen die übrigen gewählten Einträge nicht mehr dran - sie stehen als "unverändert" in der Liste */
        if (!erg) erg = calloc(ERG_MAX, sizeof *erg);
        arena *x = arena_neu();
        for (int i = 0; erg && i < K->Funde.n && erg_n < ERG_MAX; i++) {
            const eintrag *f = K->Funde.e[i]; if (!f->Wahl || f->SmDatei) continue;
            const char *sl = erg_schluessel(x, f); bool da = false;
            for (int j = 0; j < erg_n && !da; j++) da = !strcmp(erg[j].schl, sl);
            if (da) continue;
            erg_zeile *e = &erg[erg_n++];
            snprintf(e->name, sizeof e->name, "%s", ohne_ort_vorne(kern_zeile(x, f))); snprintf(e->schl, sizeof e->schl, "%s", sl);
            { const char *bk = bild_kennung_in(x, f); snprintf(e->bild, sizeof e->bild, "%s", bk ? bk : ""); }   /* 1.3 */
            e->b = f->Bytes; e->hat_b = f->HatGroesse; e->kat = f->Kat; e->zustand = 3;
            e->text[0] = 0; snprintf(e->grund, sizeof e->grund, "%s", T("nach „Anhalten“ nicht mehr drangekommen", "not reached after \"Stop\""));
        }
        arena_frei(x);
    }
    pthread_mutex_unlock(&erg_m);
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
    int n = K->SCHUTZ.diag.n, t = K->SCHUTZ.zu_tief.n;   /* in DIESER Suche gefunden (Codex-Prüfung 71); NEU 1.1: "zu viele Dateien" gibt es seit 1.0.10 nicht mehr */
    if (n <= 0) return;
    ui_log(NULL, VERB(K->a, T("Geschützt und nicht angeboten: ", "Protected and not offered: "), a_fmt(K->a, "%d", n), T(" Ordner (", " folders ("),
                      a_fmt(K->a, "%d", n - t), T(" nicht lesbar, ", " not readable, "), a_fmt(K->a, "%d", t), T(" zu tief verschachtelt", " nested too deep"), "):"));
    for (int i = 0, z = 0; i < n && z < 1000; i++) {   /* Codex-Prüfung 71: alle zeigen (bis 1000) */
        const char *p = K->SCHUTZ.diag.e[i];
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
    uint64_t m0 = mess_ms();   /* NEU 1.3: Teilzeiten des Nachschauens */
    kern_verbinden(K, app_zugriff());
    uint64_t m1 = mess_ms();
    /* Selbstschutz (NEU 1.1): eigener Ort + Homebrew Launcher, auf allen Speichern */
    kern_selbstschutz_eigener_pfad(K, eigener_ort());
    kern_selbstschutz_ermitteln(K);
    uint64_t m2 = mess_ms();
    ablauf_suchen(&A, KI.modell, KI.firmware);
    {
        uint64_t m3 = mess_ms(); char z[240];
        snprintf(z, sizeof z, T("Zeit: Nachschauen %llu ms (Verbinden %llu ms, Selbstschutz %llu ms, Suchen %llu ms)",
                                "Time: scan %llu ms (connect %llu ms, self-protection %llu ms, search %llu ms)"),
                 (unsigned long long)(m3 - m0), (unsigned long long)(m1 - m0), (unsigned long long)(m2 - m1), (unsigned long long)(m3 - m2));
        ui_log(NULL, z);
    }
    unlesbar_melden();   /* NEU 1.0.8 (Arbeitsplan 2.5) */
    pruefausgabe();
    speicher_zeile(T("Nachschauen fertig", "scan finished"));
    mess_bericht();   /* NEU 1.3 */
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
/* NEU 1.3 (da_Chris86 20.11.2026: "Bekommt der Nutzer eine Info, was in 1.2 neu ist?" - bis 1.2 nur "Update x verfügbar"): die
 * wichtigsten Punkte aus den Release-Notes der neuen Version (app/upd_suche.c upd_neu_punkte), für beide Sprachen gemerkt - die Sprache
 * lässt sich jederzeit umschalten. Deutsch: die Überschriften der Punkte; Englisch: der Kurztext in Sätzen. Auf der Startseite (vor dem
 * Nachschauen) unter "Neu in x", ganz im Protokoll. */
#define UPD_PUNKTE 6
static char upd_p_de[UPD_PUNKTE][UPD_NEU_LAENGE], upd_p_en[UPD_PUNKTE][UPD_NEU_LAENGE];   /* unter upd_m */
static int upd_pn_de, upd_pn_en;                                                          /* unter upd_m */
static int upd_punkte_holen(char (*p)[UPD_NEU_LAENGE])   /* Punkte in der eingestellten Sprache (höchstens UPD_PUNKTE) */
{
    pthread_mutex_lock(&upd_m);
    int n = SP_EN ? upd_pn_en : upd_pn_de;
    memcpy(p, SP_EN ? upd_p_en : upd_p_de, sizeof(char[UPD_NEU_LAENGE]) * (size_t)n);
    pthread_mutex_unlock(&upd_m);
    return n;
}
/* NEU 1.3 (Update-Probe 1.1 -> 1.2, da_Chris86 18.11.2026: "Ja, ersetzen" ließ die alte Datei liegen, weil das Update vor dem ersten
 * Nachschauen geholt wurde): die alte Programmdatei auch ohne Nachschauen kennen. Gesucht wird in einem eigenen Faden (nur lesen,
 * app/upd_suche.c) - bei der Update-Prüfung und noch einmal direkt vor dem Ablegen; angezeigt wird das gemerkte Ergebnis. */
static char upd_alt_such[1024];          /* unter upd_m: die gefundene alte Programmdatei, "" = keine eindeutige */
static void upd_alt_suchen(void)          /* Dateizugriffe - nur aus einem eigenen Faden, nie aus der Zeichenschleife */
{
    const char *o[16]; int n = plattform_upd_suchorte(o, 16);
    char p[1024] = ""; int t = upd_eigene_elf_suchen(o, n, KERN_KENNUNG, VERSION_NUMMER, p, sizeof p);
    pthread_mutex_lock(&upd_m); snprintf(upd_alt_such, sizeof upd_alt_such, "%s", p); pthread_mutex_unlock(&upd_m);
    char z[1300]; snprintf(z, sizeof z, "Update: alte Programmdatei gesucht (ohne Nachschauen) - %d Kopie(n) von PS5 Cleanup gefunden, %s%s",
                           t, p[0] ? "ersetzt würde: " : "keine ist eindeutig die laufende Version", p);
    plattform_protokoll_zeile(z);
}
static void rm_name_merken(void);
static void *upd_faden(void *x)
{
    (void)x;
    rm_name_merken();   /* NEU 1.1 (PS5-Test 17.11.2026 17:31): Rückmeldung auch nach "Alles bereinigen" (nanoDNS gelöscht) */
    char tag[64] = "", grund[400] = "", z[700];   /* 1.0.9 (Codex-Prüfung 145): Platz für die Rückgabewerte des zweiten Versuchs */
    char upd_zeilen[UPD_PUNKTE][UPD_NEU_LAENGE]; int upd_zeilen_n = 0;   /* NEU 1.3: was neu ist, fürs Protokoll (nur dieser Faden) */
    bool ok = plattform_neueste_version(VERSION_NUMMER, tag, sizeof tag, grund, sizeof grund);
    const char *v = tag[0] == 'v' || tag[0] == 'V' ? tag + 1 : tag;
    /* 1.0.10 (da_Chris86 12.11.2026 09:34): auch ohne Antwort nur ein kurzer Satz - der Grund (Fehlercodes) steht im Kernel-Protokoll */
    if (!ok) snprintf(z, sizeof z, "%s", T("Update-Prüfung: GitHub gerade nicht erreichbar.", "Update check: GitHub not reachable right now."));
    else if (!kern_version_gueltig(tag) || strlen(v) >= sizeof upd_neu) snprintf(z, sizeof z, "%s", T("Update-Prüfung: unbekannte Versionsangabe von GitHub.", "Update check: unknown version from GitHub."));
    else if (kern_version_neuer(tag, VERSION_NUMMER)) {
        pthread_mutex_lock(&upd_m); snprintf(upd_neu, sizeof upd_neu, "%.31s", v); pthread_mutex_unlock(&upd_m);   /* Länge oben geprüft */
        upd_alt_suchen();   /* NEU 1.3 */
        snprintf(z, sizeof z, T("Update-Prüfung: Es gibt eine neuere Version: PS5 Cleanup %s (github.com/xXBneutzerNameXx/PS5-Cleanup).",
                                "Update check: a newer version is available: PS5 Cleanup %s (github.com/xXBneutzerNameXx/PS5-Cleanup)."), v);
        {   /* NEU 1.3: was neu ist - für die Startseite merken, ganz ins Protokoll (nach der Zeile oben) */
            char *rn = malloc(8192);
            char (*de)[UPD_NEU_LAENGE] = malloc(sizeof(char[UPD_PUNKTE][UPD_NEU_LAENGE])), (*en)[UPD_NEU_LAENGE] = malloc(sizeof(char[UPD_PUNKTE][UPD_NEU_LAENGE]));
            if (rn && de && en) {
                plattform_neueste_text(rn, 8192);
                int nd = upd_neu_punkte(rn, UPD_NEU_TITEL, de, UPD_PUNKTE), ne = upd_neu_punkte(rn, UPD_NEU_EN, en, UPD_PUNKTE);
                pthread_mutex_lock(&upd_m);
                memcpy(upd_p_de, de, sizeof upd_p_de); upd_pn_de = nd; memcpy(upd_p_en, en, sizeof upd_p_en); upd_pn_en = ne;
                pthread_mutex_unlock(&upd_m);
                int nz = SP_EN ? ne : upd_neu_punkte(rn, UPD_NEU_GANZ, de, UPD_PUNKTE);   /* Deutsch: die ganzen Punkte */
                for (int i = 0; i < nz; i++) snprintf(upd_zeilen[i], sizeof upd_zeilen[i], "%s", SP_EN ? en[i] : de[i]);
                upd_zeilen_n = nz;
            }
            free(rn); free(de); free(en);
        }
    } else snprintf(z, sizeof z, "%s", T("Update-Prüfung: keine neuere Version.", "Update check: no newer version."));
    /* 1.0.10 (da_Chris86 12.11.2026 06:41 "das mit der updatzeprüfung nervt"): die Technik des zweiten Versuchs (ohne Zertifikatsprüfung,
     * Rückgabewerte) steht nicht mehr in der Zeile für den Nutzer, sondern nur noch im Kernel-Protokoll - zur Fehlersuche bleibt sie da */
    if (grund[0]) { char e[480]; snprintf(e, sizeof e, "Update-Prüfung, Einzelheiten: %s", grund); plattform_protokoll_zeile(e); }
    ui_log(NULL, z);
    for (int i = 0; i < upd_zeilen_n && i < UPD_PUNKTE; i++) {   /* NEU 1.3 */
        char w[UPD_NEU_LAENGE + 64]; snprintf(w, sizeof w, T("Neu in %s: %s", "New in %s: %s"), v, upd_zeilen[i]); ui_log(NULL, w);
    }
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
    /* NEU 1.1 (Entwurf): keine eingeblendete Meldung mehr - das Update steht in der Versions-Pille unten und unter Optionen › Info */
    neu_zeichnen();
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
typedef enum { KN_X, KN_O, KN_DREIECK, KN_QUADRAT, KN_OPT, KN_HOCHRUNTER, KN_LINKSRECHTS, KN_L1R1, KN_R1 } knopfart;   /* KN_R1: NEU 1.1 */
typedef struct { knopfart k; const char *t; bool rechts; } fussknopf;
/* NEU 1.3: die zuletzt gedrückte Taste leuchtet kurz auf (Rückmeldung "Taste ist angekommen"); ○ leuchtet, solange es gehalten wird */
static knopfart leucht_k = KN_X; static int leucht_lr = -1; static Uint32 leucht_bis = 0;
static bool leucht_ohne_pause = false;   /* PC-Vorschau mit Skript (Tests): ohne die kurze Pause */   /* leucht_lr: bei L1/R1 0 = links, 1 = rechts */
static bool knopf_leuchtet(knopfart k)
{
    if (k == KN_O && kreis_seit) return true;   /* ○ wirkt erst beim Loslassen */
    if (!leucht_bis || (Sint32)(leucht_bis - SDL_GetTicks()) <= 0) return false;
    return k == leucht_k || (leucht_k == KN_R1 && k == KN_L1R1);
}
#define LEUCHT_GRUND 0xFFFFFF
#define LEUCHT_ZEICHEN 0x0D1119
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
    if (knopf_leuchtet(k)) { rund(x + 1, cy - 17, 34, 34, 17, LEUCHT_GRUND); sym_f = LEUCHT_ZEICHEN; }   /* NEU 1.3 */
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
    bool an = knopf_leuchtet(k);   /* NEU 1.3 */
    if (an) rund(gx + 2, cy - 18, 36, 36, 18, LEUCHT_GRUND);
    const float *s = k == KN_X ? SYM_KN_KREUZ : k == KN_O ? SYM_KN_KREIS : k == KN_DREIECK ? SYM_KN_DREIECK : SYM_KN_QUADRAT;
    symbol(s, gx + 12, cy - 8, 16, an ? LEUCHT_ZEICHEN : haupt ? 0xFFFFFF : 0xE6EAF0);
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
        if (opt_fokus == i) karte_rund(x + 20, ry, w - 40, 84, 14, 3, N_FOKUS, 0x2C323B); else { flaeche fl = F_FELD11_OHNE(); flaeche_zeichnen(x + 20, ry, w - 40, 84, 14, &fl); }
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
/* Startseite: (Nochmal nachschauen), (Sprache), Testmodus, Protokoll & Rückmeldung, Browser-Bedienung (1.0.9) */
enum { SO_NOCHMAL, SO_SPRACHE, SO_TEST, SO_PROT, SO_FERN };
static void sim_hinweis_setzen(void);
static void sprache_umschalten(void)
{
    SP_EN = !SP_EN; sim_hinweis_setzen();
    plattform_absturzmelder(SP_EN ? "PS5 Cleanup " VERSION_TEXT_EN : "PS5 Cleanup " VERSION_TEXT);   /* Codex-Prüfung 70: auch der Absturzmelder */
}
static void starte(auftrag a);
/* NEU 1.1 (da_Chris86 16.11.2026 17:58: "Sprache auf Viereck ... sofort"): die Sprache lässt sich jetzt auch nach dem Nachschauen umstellen -
 * die Texte der Liste entstehen beim Suchen, darum schaut PS5 Cleanup danach von selbst neu nach (wie "Nochmal nachschauen"). Vorher
 * wie bisher nur umstellen. Für Fernseher, Optionen und Browser gleich. */
static void sprache_wechseln(void)
{
    sprache_umschalten(); fern_sprache(SP_EN);
    if (GESUCHT && ablauf_verbunden(&A)) {
        AUSWAHL_OFFEN = false; optionen_offen = false;
        starte(AUF_SUCHEN);
        kurz(T("Sprache umgestellt – PS5 Cleanup schaut neu nach.", "Language changed – PS5 Cleanup is scanning again."));
    }
}
static int start_opt_ids(int *ids)
{
    int n = 0;
    if (GESUCHT && ablauf_verbunden(&A)) ids[n++] = SO_NOCHMAL;   /* NEU 1.1: früher △ auf der Übersicht */
    if (!GESUCHT) ids[n++] = SO_SPRACHE;                          /* NEU 1.1: wie □ - nur vor der ersten Suche */
    ids[n++] = SO_TEST; ids[n++] = SO_PROT; ids[n++] = SO_FERN;
    return n;
}
static void start_optionen(void)
{
    int ids[6], n = start_opt_ids(ids);
    const char *z[6], *u[6]; const float *sy[6];
    for (int i = 0; i < n; i++) switch (ids[i]) {
        case SO_NOCHMAL: z[i] = T("Nochmal nachschauen", "Scan again"); u[i] = T("Sucht alles noch einmal.", "Searches everything again."); sy[i] = SYM_NEU; break;
        case SO_SPRACHE: z[i] = T("Sprache: Deutsch", "Language: English"); u[i] = T("✕ stellt um auf: English", "✕ switches to: Deutsch"); sy[i] = SYM_INFO; break;
        case SO_TEST: z[i] = testmodus_zeile(); u[i] = testmodus_satz(); sy[i] = SYM_SCHILD_HAKEN; break;
        case SO_PROT: z[i] = T("Protokoll & Rückmeldung", "Log & feedback"); u[i] = T("Was bisher passiert ist, dazu die freiwillige Rückmeldung", "What happened so far, plus the optional feedback"); sy[i] = SYM_DOKUMENT; break;
        default: z[i] = fern_zeile_opt(); u[i] = fern_satz(); sy[i] = SYM_KONSOLE; break;
    }
    optionen_zeichnen(z, u, sy, n);
}
static void optionen11(void);
static void wusstest11(int x, int y, int w, int art);
static void text_gesperrt(int x, int y, int f, int gr, uint32_t farbe, const char *t, float abst);
static void ring_teil(float cx, float cy, float r, float d, float anteil, uint32_t farbe);
#include "seiten_11.h"   /* NEU 1.1: Seiten der neuen Oberfläche */
#define kopf_neu kopf_neu11
#define fuss_neu fuss_neu11
static void schirm_start(void)
{
    hintergrund11();
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
    if (optionen_offen) optionen11();
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
static struct { int k, funde_n, hg, sort, fnr; void *funde_e; arena *a; kblock *bl; int n; } AL = { .k = -1 };   /* NEU 1.2: fnr = K->FUNDE_NR */
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
#define ALS_REST(x) (k != 1 && k != 7 && (x)->teile.n == 1 && ist_rest((x)->kopf))
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
    if (k == AL.k && AL.funde_n == K->Funde.n && AL.funde_e == (void *)K->Funde.e && AL.fnr == K->FUNDE_NR && AL.hg == A.hg_n && AL.sort == sortierung) return;
    const eintrag *merk = k == AL.k ? al_markiert() : NULL;
    /* Codex-Prüfung 99 (P2): der erste Teil eines Blocks ist oft derselbe Eintrag wie sein Kopf. War eine Teilzeile markiert, bleibt es eine
     * Teilzeile - bisher sprang die Markierung beim Umsortieren (Größen fertig gezählt, Reihenfolge geändert) auf den Kopf, und ✕ hakte
     * dann den ganzen Block an statt des einen Teils */
    const bool merk_teil = merk && al_t >= 0 && al_mehr(al_b);
    if (k != AL.k) { al_b = 0; al_t = -1; al_py = 0; }
    if (AL.a) arena_frei(AL.a);
    AL.a = arena_neu();
    kblock *bl = NULL; int nb = bloecke_holen(AL.a, k, sortierung, &bl);
    AL.bl = bl; AL.n = nb; AL.k = k; AL.funde_n = K->Funde.n; AL.funde_e = (void *)K->Funde.e; AL.fnr = K->FUNDE_NR; AL.hg = A.hg_n; AL.sort = sortierung;
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
#define AL_KOPF 86   /* 1.1 (Entwurf): Bild 76 + 2 x 5 */
#define AL_TEIL 46   /* 1.1 (Entwurf): Haken 42 + 2 x 2 */
#define AL_WARN 74   /* 1.1 (Entwurf 2b): Warnung unter einer angehakten Zeile */
static const char *al_warnung(int b);
typedef struct { int b, t, y, h; } al_zeile;
static int al_zeilen(al_zeile *z)
{
    int n = 0, y = 0;
    for (int b = 0; b < AL.n; b++) {
        bool auf = fokus_zone == 1 && b == al_b && al_mehr(b);
        int hk = AL_KOPF + (al_warnung(b) ? AL_WARN : 0);
        z[n++] = (al_zeile){ b, -1, y, hk }; y += hk;
        if (auf) for (int t = 0; t < AL.bl[b].teile.n; t++) { z[n++] = (al_zeile){ b, t, y, AL_TEIL }; y += AL_TEIL; }
        y += 3;
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
    else if (!vorher && f->Wahl && f->BezugWarnung) { kurz(f->BezugWarnung); hinweis_dauer(8000); }   /* NEU 1.3: Elf Launcher - gehört zu WK Store */
    else if (vorher && !f->Wahl && f->BezugWarnung) hinweis_bis = 0;
    else if (vorher && !f->Wahl && f->Jailbreak) hinweis_bis = 0;   /* da_Chris86 29.10.2026 (Fernsehtest): abgewählt - Warnung sofort weg */
}
/* Teile, die ✕ auf dem Kopf nicht mit anhakt: was endgültig weg ist oder nur einzeln geht (wie bei "ganzen Bereich wählen").
 * NEU 1.3 (21.11.2026): die Regel steht jetzt im Kern (kern/ablauf.c ablauf_block_nur_einzeln, ablauf_block_setzen) - derselbe Wortlaut,
 * damit der Test der Löschliste je Auswahlweg genau das prüft, was Fernseher und Browser tun */
static bool nur_einzeln_teil(int b, const eintrag *f) { (void)b; return ablauf_block_nur_einzeln(f); }
/* ✕ auf dem Kopf eines Blocks: alle anderen Teile an bzw. aus. Abwählen nimmt alles raus. Bei den Resten geht alles. */
static void block_setzen_p(const pliste *p, bool reste, bool browser)   /* NEU 1.0.9: aus al_block_setzen (auch für den Browser) */
{
    if (p->n == 1 && !reste) { eintrag_umschalten(p->e[0]); return; }
    if (!ablauf_block_setzen(&A, p))
        kurz(browser ? T("Diese Teile nur einzeln anhaken: den Block aufklappen und einzeln wählen.", "Tick these parts one by one: open the block and select them individually.")
                     : T("Diese Teile nur einzeln anhaken: mit ↓ in den Block, dann ✕.", "Tick these parts one by one: ↓ into the block, then ✕."));
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
static const char *schild_text(const eintrag *f, int k)
{
    if (k != 7 || f->Art != ART_JAILBREAK) return kern_folge_kurz(f);
    /* NEU 1.3: hat der Kern für diesen Teil eines Einstiegs einen eigenen kurzen Satz (die Ordner von WK Store: "gehört zu WK Store",
     * "endgültig weg"), steht der da - sonst wie bisher "neu einrichten" */
    const char *kz = kern_folge_kurz(f);
    return strcmp(kz, T("Jailbreak neu einrichten", "set up jailbreak again")) != 0 ? kz : T("neu einrichten", "set up again");
}
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
    /* NEU 1.1 (PS5-Test da_Chris86 17.11.2026: "Fallout: New Vegas – Spieldateien (Playden)" wurde zu "New Vegas – Spieldateien" / "Fallout"):
     * steht vor dem ersten " – " genau der Name eines anderen Eintrags mit derselben Kennung (Kachel, Spiel), ist es "Name – Zusatz" und kein
     * "Vorsatz: Rest" - für jeden Namen mit Doppelpunkt, ohne Sonderregel. Nur hier gesucht (Doppelpunkt vor dem " – "), das ist selten. */
    if (d && d - z <= 40 && d[2]) {
        const char *s0 = strstr(z, " – "), *bid = s0 && s0 > d ? bild_kennung(f) : NULL;
        if (bid && bid[0]) {
            size_t l = (size_t)(s0 - z);
            for (int i = 0; i < K->Funde.n; i++) {
                const eintrag *g = K->Funde.e[i]; if (g == f) continue;
                const char *gb = bild_kennung(g); if (!gb || strcmp(gb, bid) != 0) continue;
                const char *gn = kern_name(fa, g);
                if (gn && strlen(gn) == l && !strncmp(gn, z, l) && s0[strlen(" – ")]) { *name = gn; *was = s0 + strlen(" – "); return; }
            }
        }
    }
    if (d && d - z <= 40 && d[2]) {
        const char *s = strstr(z, " – ");
        if (!s || s > d) { *name = d + 2; *was = a_kopie_n(fa, z, (size_t)(d - z)); }
        /* NEU 1.3 (da_Chris86 21.11.2026 15:23, PS5-Test: "CUSA08519 (Red Dead Redemption 2)" / "ps4mount-Daten" neben "Red Dead Redemption 2" /
         * "Spiel"): steht hinter dem Vorsatz "KENNUNG (Name)", ist der Name der Name - wie bei allen anderen Teilen, ohne Kennung. Gilt für jeden
         * Vorsatz (ps4mount-Daten, Web-Zwischenspeicher, Fake-Lizenz ...). Ohne bekannten Namen bleibt die Kennung stehen. */
        if (*name == d + 2) {
            htreffer mk = h_mt(fa, *name, "^[A-Z]{4}\\d{5} \\((.+)\\)$");
            if (mk.ok && strcmp(mk.gr[1], T("Spiel/App unbekannt", "game/app unknown")) != 0) *name = mk.gr[1];
        }
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
static void ort_schild(int x, int cy, const char *o) { flaeche f = { false, V1(0, 0), 0x2A3B57, 1, 1, 1, 0, 0 }; flaeche_zeichnen(x, cy - 16, ort_breite(o), 32, 8, &f); text(x + 10, cy + 8, 2, 22, N_TEXT2, o); }   /* NEU 1.1: Rahmen statt Fläche */
static void kasten_haken(int x, int y, int z, uint32_t hg, bool gesperrt, bool jb)   /* 28 x 28: 0 leer, 1 teilweise, 2 gewählt */
{
    /* NEU 1.1: Haken-Kästchen 34 x 34 nach Entwurf (Blau, beim Jailbreak Gold); x/y bleiben die Ecke des alten 28er-Kästchens */
    (void)hg; x -= 3; y -= 3;
    flaeche rand_f = { false, V1(0, 0), 0x4A5B78, 1, 1, 2, 0, 0 };
    if (gesperrt) { rand_f.rand = N_LINIE; flaeche_zeichnen(x, y, 34, 34, 9, &rand_f); symbol(SYM_SCHLOSS, x + 7, y + 7, 20, N_GRAU2); return; }
    uint32_t f = jb ? S_GOLD : S_BLAU;
    if (z == 2) { rund(x, y, 34, 34, 9, f); symbol(SYM_HAKEN, x + 6, y + 6, 22, jb ? 0x241A05 : 0xFFFFFF); }
    else if (z == 1) { rand_f.rand = f; flaeche_zeichnen(x, y, 34, 34, 9, &rand_f); rund(x + 9, y + 15, 16, 4, 2, f); }
    else flaeche_zeichnen(x, y, 34, 34, 9, &rand_f);
}
static void kachel_klein(int x, int y, int gr, const eintrag *f, int k, uint32_t hg)   /* Kachelbild, sonst Symbol des Bereichs */
{
    SDL_Surface *b = (f && f != &RESTE_ZEILE) ? kachelbild(f, gr <= 96 ? 76 : gr) : NULL;
    if (b) { surf_rund(b, x, y, gr, gr, gr * 22 / 100); return; }   /* NEU 1.1: runde Ecken ohne feste Hintergrundfarbe */
    { flaeche fl = F_FELD11(); flaeche_zeichnen(x, y, gr, gr, gr * 22 / 100, &fl); }
    symbol(f == &RESTE_ZEILE ? SYM_SONSTIGES : kat_symbol(k), x + gr / 4, y + gr / 4, gr / 2, k == 7 ? N_ORANGE : N_GRAU);
}
/* NEU 1.1 (Entwurf): Bild eines Eintrags - das Kachelbild, sonst ein Kürzel aus dem Namen auf einer Farbe ("PS5 Cleanup" -> "PC") */
static const char *kuerzel11(const char *n)
{
    static char k[8]; int j = 0; bool neu = true;
    for (const unsigned char *p = (const unsigned char *)(n ? n : ""); *p && j < 2; p++) {
        unsigned char c = *p;
        bool trenn = c == ' ' || c == '_' || c == '-' || c == '.' || c == '(' || c == '[' || c == '/';
        if (trenn) { neu = true; continue; }
        if (neu && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) { k[j++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c); }
        neu = false;
    }
    if (j == 1) {   /* ein Wort: die ersten beiden Buchstaben */
        for (const unsigned char *p = (const unsigned char *)n; *p; p++) if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) {
            const unsigned char *q = p + 1;
            if ((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z') || (*q >= '0' && *q <= '9')) { k[1] = (char)(*q >= 'a' && *q <= 'z' ? *q - 32 : *q); j = 2; }
            break;
        }
    }
    k[j] = 0;
    return j ? k : "?";
}
static uint32_t kuerzel_farbe11(const char *n, int k)
{
    static const uint32_t F[6] = { 0x1F4FA8, 0x1E3A5F, 0x2A3346, 0x4B3A8F, 0x1F5F57, 0x5A2E6E };
    unsigned h = 5381; for (const char *p = n ? n : ""; *p; p++) h = h * 33 + (unsigned char)*p;
    if (k == 7) return (h & 1) ? 0x7A1F1F : 0x2A3346;
    return F[h % 6];
}
static void bild_kachel11(int x, int y, int s, int r, const eintrag *f, int k, const char *name, float deck)
{
    SDL_Surface *b = f ? kachelbild(f, s <= 96 ? 76 : s) : NULL;
    if (b) {
        surf_rund(b, x, y, s, s, r);
        if (deck < 1.f) { flaeche d = { false, V1(S_HG, 1.f - deck), 0, 0, 0, 0, 0, 0 }; flaeche_zeichnen(x, y, s, s, r, &d); }   /* gesperrt: abgedunkelt */
        return;
    }
    uint32_t c = kuerzel_farbe11(name, k);
    if (deck < 1.f) c = farbmisch(c, S_HG, deck);
    rund(x, y, s, s, r, c);
    const char *kz = kuerzel11(name); int gr = s * 30 / 76; if (gr < 18) gr = 18;
    text_mitte(x + s / 2, y + s / 2 + gr * 36 / 100, 3, gr, deck < 1.f ? farbmisch(0xFFFFFF, S_HG, deck) : 0xFFFFFF, kz);
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
/* NEU 1.1 (Entwurf "2 – Auswählen"): Überschriften in Großbuchstaben, gesperrt (24, grau; "ACHTUNG" gold), Text 26 in Weiß;
 * Absätze ohne Überschrift (Hinweise) mit Symbol in ihrer Farbe */
static const char *gross11(const char *t)   /* Großbuchstaben, auch äöü */
{
    size_t l = strlen(t ? t : ""); char *o = a_hole(fa, l + 1); size_t j = 0;
    for (size_t i = 0; i < l; i++) {
        unsigned char c = (unsigned char)t[i];
        if (c >= 'a' && c <= 'z') o[j++] = (char)(c - 32);
        else if (c == 0xC3 && i + 1 < l && ((unsigned char)t[i + 1] == 0xA4 || (unsigned char)t[i + 1] == 0xB6 || (unsigned char)t[i + 1] == 0xBC)) { o[j++] = (char)c; o[j++] = (char)((unsigned char)t[i + 1] - 0x20); i++; }
        else o[j++] = (char)c;
    }
    o[j] = 0; return o;
}
static void text_gesperrt(int x, int y, int f, int gr, uint32_t farbe, const char *t, float abst)   /* letter-spacing (em) */
{
    char z[8]; int sp = (int)(gr * abst + .5f);
    for (const char *p = t; *p; ) {
        int n = 1; unsigned char c = (unsigned char)*p; if (c >= 0xF0) n = 4; else if (c >= 0xE0) n = 3; else if (c >= 0xC0) n = 2;
        memcpy(z, p, (size_t)n); z[n] = 0; text(x, y, f, gr, farbe, z); x += textbreite(f, gr, z) + sp; p += n;
    }
}
static int kasten_absaetze(int tx, int ty, int bw, const k_absatz *ab, int n, bool zeichnen)
{
    int y = ty, letzt = ty;
    for (int i = 0; i < n; i++) {
        const k_absatz *a = &ab[i];
        if (!a->text || !a->text[0]) continue;
        bool achtung = a->frage && (a->farbe == N_ORANGE || a->farbe == N_GELB) && a->sym == SYM_JAILBREAK;
        if (a->frage) { if (zeichnen) text_gesperrt(tx, y + 24, 1, 24, achtung ? S_GOLD : S_GRAU2, gross11(a->frage), .14f); y += 24 + 10; }
        bool hinweis = !a->frage && a->sym;
        int ax = tx + (hinweis ? 38 : 0);
        zeilen z = umbrechen(0, 26, a->text, bw - (ax - tx));
        uint32_t tf = hinweis ? (a->farbe == N_ROT ? 0xFFB4AE : a->farbe == N_GRUEN ? S_GRUEN : a->farbe == N_GRAU || a->farbe == N_GRAU2 ? S_TEXT2 : S_GOLD_T) : S_TEXT_W;
        if (zeichnen) {
            if (hinweis) symbol(a->sym, tx, y + 6, 26, a->farbe == N_GELB || a->farbe == N_ORANGE ? S_GOLD : a->farbe);
            for (int j = 0; j < z.n; j++) text(ax, y + 27 + j * 38, 0, 26, tf, z.z[j]);
        }
        y += z.n * 38; letzt = y; y += 24;
    }
    return letzt - ty;
}
static const void *kasten_wer; static Uint32 kasten_seit;
static int kasten_status_art; static const char *kasten_status;   /* unten im Kasten: 1 grün, 2 gold, 3 rot, 4 gesperrt (setzt al_kasten) */
static void kasten_zeichnen(int x, int y, int w, int h, const eintrag *bild_f, int k, const char *titel, const char *unter, const k_absatz *ab, int n, const void *wer)
{
    karte11(x, y, w, h, 20);
    if (k == 7) { flaeche r = { false, V1(0, 0), 0x6B5520, 1, 1, 1, 0, 0 }; flaeche_zeichnen(x, y, w, h, 20, &r); }   /* Entwurf 2b: Jailbreak mit goldenem Rand */
    int tx = x + 28, bw = w - 56, oben = y + 28, unten = y + h - 28;
    /* Statuskasten unten */
    if (kasten_status_art && kasten_status && kasten_status[0]) {
        passend sp = einpassen(0, 24, 24, 20, kasten_status, bw - 32 - 40, 2); sp.lh = sp.gr + 6;
        int sh = 32 + sp.z.n * sp.lh, sy = unten - sh;
        uint32_t sf = S_TEXT2; const float *sy_ = SYM_HAKEN; uint32_t ic = S_GRUEN;
        if (kasten_status_art == 1) gruen11(tx, sy, bw, sh, 14);
        else if (kasten_status_art == 2) { gold11(tx, sy, bw, sh, 14, 1); sy_ = SYM_NEU; ic = S_GOLD; sf = S_GOLD_T; }
        else if (kasten_status_art == 3) { flaeche f = { false, V2(135, N_ROT, .22f, 0x2A0D0D, .55f), N_ROT, 1, 1, 1, 0, 0 }; flaeche_zeichnen(tx, sy, bw, sh, 14, &f); sy_ = SYM_WARNUNG; ic = N_ROT; sf = 0xFFD0CE; }
        else { flaeche f = { false, V1(0xFFFFFF, .045f), 0x2A3B57, 1, 1, 1, 0, 0 }; flaeche_zeichnen(tx, sy, bw, sh, 14, &f); sy_ = SYM_SCHLOSS; ic = S_GRAU; }
        symbol(sy_, tx + 16, sy + sh / 2 - 14, 28, ic);
        for (int i = 0; i < sp.z.n; i++) text(tx + 16 + 28 + 12, sy + 16 + sp.gr - 2 + i * sp.lh, 0, sp.gr, sf, sp.z.z[i]);
        unten = sy - 24;
    }
    kasten_status_art = 0; kasten_status = NULL;
    /* Kopf: Bild 140 links, daneben Name (34, bis 3 Zeilen) und Unterzeile */
    int bg = 140, nx = tx + bg + 20, nb = x + w - 28 - nx;
    if (k == 7) leuchten(tx, oben, bg, bg, 30, 26, S_GOLD, .25f); else leuchten(tx, oben, bg, bg, 30, 26, S_TUERKIS, .3f);
    if (bild_f) bild_kachel11(tx, oben, bg, 30, bild_f, k, titel, 1.f);
    else { rund(tx, oben, bg, bg, 30, k == 7 ? 0x3A2A08 : 0x1F4FA8); symbol(kat_symbol(k), tx + bg / 4, oben + bg / 4, bg / 2, k == 7 ? S_GOLD : 0xFFFFFF); }
    passend tp = einpassen(3, 34, 34, 24, titel, nb, 3); tp.lh = tp.gr + 6;
    int th = tp.z.n * tp.lh + (unter[0] ? 8 + 30 : 0), ty0 = oben + (bg - th) / 2;
    if (ty0 < oben) ty0 = oben;
    for (int i = 0; i < tp.z.n; i++) text(nx, ty0 + tp.gr - 4 + i * tp.lh, 3, tp.gr, S_TEXT_W, tp.z.z[i]);
    if (unter[0]) text_passend(nx, ty0 + tp.z.n * tp.lh + 8 + 22, 0, 24, 20, S_GRAU, unter, nb);
    int ty = oben + bg + 24;
    int inhalt = kasten_absaetze(tx, 0, bw, ab, n, false);
    int platz = unten - ty, off = 0;
    if (inhalt > platz) {
        if (wer != kasten_wer) { kasten_wer = wer; kasten_seit = SDL_GetTicks(); }
        off = lauf_versatz(kasten_seit, inhalt - platz + 8, 28, 3000);   /* 1.0.8 (4.7): langsam durchlaufen */
        clip_y0 = ty - 2; clip_y1 = unten + 6;
    }
    kasten_absaetze(tx, ty - off, bw, ab, n, true);
    clip_y0 = 0; clip_y1 = 1 << 30;
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
/* NEU 1.1 (Entwurf): Überschriften "Das wird gelöscht", "Danach" (beim Jailbreak zuerst "Achtung"); was danach passiert, steht als
 * farbiger Kasten unten (grün nichts verloren, gold neu laden/einrichten, rot endgültig weg, grau gesperrt) */
static void al_status(int a, const char *kurz)
{
    kasten_status_art = a == AMPEL_ROT ? 3 : a == AMPEL_GELB ? 2 : 1;
    kasten_status = a == AMPEL_ROT ? T("Danach endgültig weg", "Gone for good afterwards")
                  : a == AMPEL_GELB ? (kurz && kurz[0] ? VERB(fa, T("Danach: ", "Afterwards: "), kurz) : T("Danach neu laden oder einrichten", "Download or set up again afterwards"))
                  : T("Es geht nichts verloren", "Nothing is lost");
}
/* NEU 1.1: Inhalt des Kastens (auch für die Browserseite, fern_tv.h) - gezeichnet wird in al_kasten */
typedef struct { const eintrag *bild; int k; const char *titel, *unter; k_absatz ab[6]; int n; int st_art; const char *st; const void *wer; } kasten_d;
#define KD(bild_, k_, ti_, un_, ab_, n_, wer_) do { d->bild = (bild_); d->k = (k_); d->titel = (ti_); d->unter = (un_); memcpy(d->ab, (ab_), sizeof(k_absatz) * (size_t)(n_)); \
    d->n = (n_); d->wer = (wer_); d->st_art = kasten_status_art; d->st = kasten_status; kasten_status_art = 0; kasten_status = NULL; } while (0)
static void al_kasten_daten(int w, int k, kasten_d *d)
{
    eintrag *f = al_markiert();
    int b = al_b;
    k_absatz ab[6]; memset(ab, 0, sizeof ab);
    if (!f) {   /* leerer Bereich */
        ab[0] = (k_absatz){ T("Was ist das", "What is it"), kern_kat_info(k), N_TEXT, NULL };
        KD(NULL, k, kern_kat_name(k), bereich_unter(k, w - 56 - 160), ab, 1, &sicht[fokus_bereich]);
        return;
    }
    const kblock *bl = &AL.bl[b];
    const char *unter_b = VERB(fa, a_fmt(fa, T("%d Teile", "%d parts"), bl->teile.n), " · ", block_groesse(b));
    if (f == &RESTE_ZEILE) {
        ab[0] = (k_absatz){ T("Das wird gelöscht", "This will be deleted"), T("Reste von Spielen und Apps, die es nicht mehr gibt, Zwischenspeicher und leere Ordner.",
                                                                          "Leftovers of games and apps that are gone, caches and empty folders."), N_TEXT, NULL };
        al_status(AMPEL_GRUEN, NULL);
        KD(NULL, k, bl->name, unter_b, ab, 1, &RESTE_ZEILE);
        return;
    }
    if (al_t < 0 && al_mehr(b)) {   /* Kopf eines Blocks mit mehreren Teilen */
        const char *teile = ""; int n = bl->teile.n, rot = block_rot(b), einz = 0, frei = 0; const char *g;
        for (int i = 0; i < n; i++) { eintrag *e = bl->teile.e[i]; if (ablauf_eintrag_gesperrt(&A, e, &g)) continue; if (nur_einzeln_teil(b, e)) einz++; else frei++; }
        /* NEU 1.1 (PS5-Test 16.11.2026, Playden-Block): gehen manche Teile nur einzeln, nennt "Das wird gelöscht" nur die Teile, die ✕ auf
         * dem Kopf wirklich nimmt - sonst stand dort auch, was ✕ auslässt (z. B. Spielstände), und der Hinweis darunter war abgeschnitten */
        bool nur_frei = einz > 0 && frei > 0; int m = n, z = 0;
        if (nur_frei) { m = 0; for (int i = 0; i < n; i++) { eintrag *e = bl->teile.e[i]; if (!((ablauf_eintrag_gesperrt(&A, e, &g) && (e->Gesperrt || !kern_nur_mit_spiel(K, e))) || nur_einzeln_teil(b, e))) m++; } }
        for (int i = 0; i < n && z < 6; i++) {
            eintrag *e = bl->teile.e[i];
            /* Codex-Prüfung 343 (5): ein Update oder Zusatzinhalt, das nur mit dem Spiel geht, kommt über das Spiel mit - es steht dabei;
             * Codex-Prüfung 344: nicht, wenn es selbst gesperrt ist (Selbstschutz) - das nimmt kern_abhaengige_mitnehmen nie mit */
            if (nur_frei && ((ablauf_eintrag_gesperrt(&A, e, &g) && (e->Gesperrt || !kern_nur_mit_spiel(K, e))) || nur_einzeln_teil(b, e))) continue;
            teile = VERB(fa, teile, z ? (z == m - 1 ? T(" und ", " and ") : ", ") : "", teil_name(b, e)); z++;
        }
        if (m > 6) teile = VERB(fa, teile, a_fmt(fa, T(" und %d weitere", " and %d more"), m - 6));
        const char *da = kern_art_info(fa, bl->kopf); int a = kern_ampel(bl->kopf);
        if (!da[0]) da = kern_folge(bl->kopf);
        int i = 0;
        if (bl->kopf->Bezug) ab[i++] = (k_absatz){ T("Hinweis", "Note"), bl->kopf->Bezug, N_ORANGE, SYM_JAILBREAK };   /* NEU 1.3: Elf Launcher <-> WK Store - zuerst, sonst abgeschnitten */
        /* NEU 1.3 (da_Chris86 21.11.2026 19:25, WK Store: "nicht dass ich beim Antippen die Sicherung mit lösche"): nimmt ✕ auf dem Kopf gar
         * nichts mit (alle Teile nur einzeln), stand hier trotzdem jeder Teil unter "Das wird gelöscht" - jetzt steht da, dass es nichts ist */
        if (einz > 0 && frei == 0) {
            ab[i++] = (k_absatz){ T("Das wird gelöscht", "This will be deleted"), T("Mit ✕ auf dieser Zeile nichts - jeder Teil geht nur einzeln: mit → in den Block, dann ✕ auf dem Teil.",
                                                                              "Nothing with ✕ on this row - each part goes individually only: → into the block, then ✕ on the part."), N_TEXT, NULL };
        } else ab[i++] = (k_absatz){ T("Das wird gelöscht", "This will be deleted"), VERB(fa, teile, "."), N_TEXT, NULL };
        ab[i++] = (k_absatz){ T("Danach", "Afterwards"), da, N_TEXT, NULL };
        const char *hw = NULL;
        if (einz > 0 && frei == 0) hw = rot ? T("Was rot ist, ist danach endgültig weg.", "What is red is gone for good afterwards.") : NULL;
        else if (rot > 0) hw = rot == 1 ? T("1 Teil davon ist danach endgültig weg. ✕ auf dieser Zeile lässt ihn aus - den hakst du einzeln an.",
                                            "1 part of it is gone for good afterwards. ✕ on this row leaves it out - tick it individually.")
                                        : a_fmt(fa, T("%d Teile davon sind danach endgültig weg. ✕ auf dieser Zeile lässt sie aus - die hakst du einzeln an.",
                                                      "%d parts of it are gone for good afterwards. ✕ on this row leaves them out - tick them individually."), rot);
        else if (einz > 0) hw = einz == 1 ? T("1 Teil geht nur einzeln: ✕ auf dieser Zeile lässt ihn aus.", "1 part only goes individually: ✕ on this row leaves it out.")
                                          : a_fmt(fa, T("%d Teile gehen nur einzeln: ✕ auf dieser Zeile lässt sie aus.", "%d parts only go individually: ✕ on this row leaves them out."), einz);
        if (hw) ab[i++] = (k_absatz){ NULL, hw, rot ? N_ROT : N_GELB, rot ? SYM_WARNUNG : SYM_INFO };
        al_status(rot ? AMPEL_ROT : a, schild_text(bl->kopf, k));
        KD(block_bild(bl), k, bl->name, unter_b, ab, i, (const char *)bl->kopf + 1);
        return;
    }
    /* eine Zeile (Block aus einer Zeile oder ein Teil) */
    const char *grund = NULL; bool gesp = ablauf_eintrag_gesperrt(&A, f, &grund);
    const char *gr = h_trim(fa, ablauf_eintrag_groesse(&A, f)); int o = anzeige_ort(f);
    const char *unter = VERB(fa, gr, o != ORT_INTERN ? VERB(fa, gr[0] ? " · " : "", kern_ort_name(o)) : "");
    const char *nm, *ws; zeile_teilen(f, k, &nm, &ws);
    int a = kern_ampel(f);
    const char *da = kern_art_info(fa, f); if (!da[0]) da = kern_folge(f);
    int i = 0;
    /* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 3): eine Zeile, die sich gar nicht wählen lässt, braucht kein "Danach" - dann steht der Grund da */
    bool nie_waehlbar = gesp && grund && grund[0] && (f->Gesperrt || kern_nur_mit_spiel(K, f));
    if (f->Jailbreak && !gesp) ab[i++] = (k_absatz){ T("Achtung", "Caution"), kern_jb_hinweis(fa, f), N_ORANGE, SYM_JAILBREAK };
    else if (f->Bezug && !gesp) ab[i++] = (k_absatz){ T("Hinweis", "Note"), f->Bezug, N_ORANGE, SYM_JAILBREAK };   /* NEU 1.3 */
    ab[i++] = (k_absatz){ T("Das wird gelöscht", "This will be deleted"), ohne_ort_vorne(kern_zeile(fa, f)), N_TEXT, NULL };
    if (ordner_zeigen) ab[i++] = (k_absatz){ T("Ordner", "Folders"), ordner_text(f), N_TEXT, NULL };
    if (nie_waehlbar) ab[i++] = (k_absatz){ T("Warum gesperrt", "Why locked"), grund, N_TEXT, NULL };
    else {
        ab[i++] = (k_absatz){ T("Danach", "Afterwards"), da, N_TEXT, NULL };
        if (gesp && grund && grund[0]) ab[i++] = (k_absatz){ NULL, grund, N_GELB, SYM_SCHLOSS };
    }
    if (gesp) { kasten_status_art = 4; kasten_status = T("Gesperrte Einträge bleiben immer - der Grund steht in der Zeile.", "Locked entries always stay - the reason is in the row."); }
    else al_status(a, schild_text(f, k));
    /* NEU 1.3 (PS5-Test 21.11.2026: "nanodns – Einstellungen" zeigte nur "NE", der Block nanoDNS hat ein Bild): ein Teil ohne eigenes Bild
     * zeigt das Bild seines Blocks - gilt für jeden Block */
    const eintrag *rb = f;
    { const char *kb = bild_kennung(f); if ((!kb || bild_fehlt(kb)) && al_mehr(b)) { const eintrag *bb = block_bild(bl); if (bb) rb = bb; } }
    KD(rb, k, nm, unter, ab, i, f);
}
#undef KD
static void al_kasten(int x, int y, int w, int h, int k)
{
    kasten_d d; memset(&d, 0, sizeof d); al_kasten_daten(w, k, &d);
    kasten_status_art = d.st_art; kasten_status = d.st;
    kasten_zeichnen(x, y, w, h, d.bild, d.k, d.titel, d.unter, d.ab, d.n, d.wer);
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
/* NEU 1.1 (Entwurf "2 – Auswählen", da_Chris86 15.11.2026 "Die Vorlage ist das neue Design"): Zeile mit Haken (42), Bild (76, sonst Kürzel
 * auf Farbe), Name (28) und darunter grau, was es ist und was danach passiert; rechts der Ort als Kästchen und die Größe. Angehakt und
 * danach endgültig weg: goldener (Jailbreak) bzw. roter Rahmen mit Warnung unter der Zeile (Entwurf 2b). Gesperrt: ausgegraut mit Schloss. */
static const char *ort_lang(int o) { return o == ORT_M2 ? "M.2" : o == ORT_USB ? T("USB-Platte", "USB HDD") : o == ORT_STICK ? "USB" : T("Intern", "Internal"); }
static const char *al_orte(int b)   /* "Intern + USB" */
{
    bool da[4] = { false, false, false, false }; const char *t = "";
    for (int i = 0; i < AL.bl[b].teile.n; i++) { int o = anzeige_ort(AL.bl[b].teile.e[i]); if (o >= 0 && o < 4) da[o] = true; }
    for (int o = 0; o < 4; o++) if (da[o]) t = t[0] ? VERB(fa, t, " + ", ort_lang(o)) : ort_lang(o);
    return t;
}
static const char *al_warnung(int b)   /* Warnung unter der Zeile (NULL = keine) */
{
    if (b < 0 || b >= AL.n || AL.bl[b].kopf == &RESTE_ZEILE) return NULL;
    const kblock *bl = &AL.bl[b]; int rw = 0; bool jb = false;
    for (int i = 0; i < bl->teile.n; i++) { const eintrag *e = bl->teile.e[i]; if (e->Wahl && kern_ampel(e) == AMPEL_ROT) { rw++; if (e->Jailbreak) jb = true; } }
    if (!rw) return NULL;
    if (jb) return T("Danach startet dein Jailbreak über diesen Weg nicht mehr. Du brauchst einen anderen Einstieg.", "Afterwards your jailbreak no longer starts this way. You need another entry point.");
    if (bl->teile.n > 1) return rw == 1 ? T("1 Teil davon ist danach endgültig weg.", "1 part of it is gone for good afterwards.")
                                        : a_fmt(fa, T("%d Teile davon sind danach endgültig weg.", "%d parts of it are gone for good afterwards."), rw);
    return T("Danach endgültig weg - das lässt sich nicht zurückholen.", "Gone for good afterwards - this cannot be brought back.");
}
static bool al_warnung_jb(int b) { const kblock *bl = &AL.bl[b]; for (int i = 0; i < bl->teile.n; i++) { const eintrag *e = bl->teile.e[i]; if (e->Wahl && e->Jailbreak && kern_ampel(e) == AMPEL_ROT) return true; } return false; }
static void haken42(int x, int y, int z, bool gesperrt, bool jb)   /* 0 leer, 1 teilweise, 2 gewählt */
{
    flaeche rf = { false, V1(0, 0), 0x4A5B78, 1, 1, 2, 0, 0 };
    if (gesperrt) {   /* gestrichelt wirkt im Entwurf - hier gedämpfter Rand mit Schloss */
        rf.rand = 0x3B4A63; flaeche_zeichnen(x, y, 42, 42, 11, &rf); symbol(SYM_SCHLOSS, x + 9, y + 9, 24, S_GRAU); return;
    }
    uint32_t f = jb ? S_GOLD : S_BLAU;
    if (z == 2) { rund(x, y, 42, 42, 11, f); symbol(SYM_HAKEN, x + 7, y + 7, 28, jb ? 0x241A05 : 0xFFFFFF); }
    else if (z == 1) { rf.rand = f; flaeche_zeichnen(x, y, 42, 42, 11, &rf); rund(x + 11, y + 19, 20, 4, 2, f); }
    else flaeche_zeichnen(x, y, 42, 42, 11, &rf);
}
static void ort_kaestchen(int xr, int cy, const char *o)   /* rechtsbündig bis xr */
{
    if (!o || !o[0]) return;
    int w = textbreite(0, 24, o) + 16, x = xr - w;
    flaeche f = { false, V1(0, 0), 0x2A3B57, 1, 1, 1, 0, 0 }; flaeche_zeichnen(x, cy - 18, w, 36, 8, &f);
    text(x + 8, cy + 8, 0, 24, S_TEXT2, o);
}
static int ort_kaestchen_breite(const char *o) { return o && o[0] ? textbreite(0, 24, o) + 16 : 0; }
/* NEU 1.1 (da_Chris86 16.11.2026 15:55): Ebene eines Teils im Block (kern_bloecke) - 1 Teil, 2 Kopf einer Untergruppe, 3 Teil darin */
static int teil_ebene(const kblock *bl, int t) { return t == 0 ? 0 : bl->ebene ? bl->ebene[t] : 1; }
/* Name eines Teils in einer Untergruppe ohne den Namen ihres Kopfs ("Fallout Shelter – Starter (Playden)" -> "Starter (Playden)") */
static const char *unter_name(const kblock *bl, int t, const char *name)
{
    if (teil_ebene(bl, t) != 3) return name;
    for (int u = t - 1; u > 0; u--) if (teil_ebene(bl, u) == 2) { const char *r = nach_name(name, al_name(bl->teile.e[u])); return r ? r : name; }
    return name;
}
static void al_kopf_zeile(int ox, int ow, int ry, int b, int k, bool fok, const char *gleich, const char *was_gleich)
{
    (void)gleich; (void)was_gleich;
    const kblock *bl = &AL.bl[b]; const char *g;
    bool reste = bl->kopf == &RESTE_ZEILE, einzeln = bl->teile.n == 1 && !reste;
    eintrag *f0 = bl->teile.e[0];
    bool gesp = einzeln && ablauf_eintrag_gesperrt(&A, f0, &g);
    const char *warn = al_warnung(b); bool wjb = warn && al_warnung_jb(b);
    int h = AL_KOPF + (warn ? AL_WARN : 0), cy = ry + AL_KOPF / 2;
    if (warn) {   /* Entwurf 2b: Rahmen um Zeile und Warnung */
        uint32_t wf = wjb ? S_GOLD : N_ROT;
        flaeche fl = { false, V1(wjb ? 0x1A1608 : 0x200C0C, .9f), wf, 1, 1, 2, 0, 0 };
        leuchten(ox, ry, ow, h, 14, 16, wf, .2f); flaeche_zeichnen(ox, ry, ow, h, 14, &fl);
    } else if (fok) fokus11(ox, ry, ow, AL_KOPF, 14, 2);
    if (warn && fok) { flaeche r = { false, V1(0, 0), S_BLAU_H, 1, 1, 3, 0, 0 }; flaeche_zeichnen(ox - 2, ry - 2, ow + 4, h + 4, 16, &r); }
    haken42(ox + 16, cy - 21, einzeln ? (f0->Wahl ? 2 : 0) : al_zustand(b), gesp, k == 7 || (einzeln && f0->Jailbreak));
    bild_kachel11(ox + 74, ry + 5, 76, 17, reste ? NULL : block_bild(bl), k, einzeln ? al_name(f0) : (bl->name ? bl->name : ""), gesp ? .55f : 1.f);
    int nx = ox + 74 + 76 + 16, xr = ox + ow - 16;
    const char *gr = gesp ? "–" : einzeln ? h_trim(fa, ablauf_eintrag_groesse(&A, f0)) : block_groesse(b);
    text_rechts(xr, cy + 10, 1, 26, S_TEXT_W, gr);
    const char *ok = reste ? "" : einzeln ? ort_lang(anzeige_ort(f0)) : al_orte(b);
    int gw = textbreite(1, 26, gr); if (gw < 116) gw = 116;
    int okx = xr - gw - 16; ort_kaestchen(okx, cy, ok);
    int rechts = okx - ort_kaestchen_breite(ok) - 16;
    /* Name und Unterzeile */
    const char *name, *was;
    if (einzeln) zeile_teilen(f0, k, &name, &was);
    else { name = bl->name ? bl->name : ""; was = a_fmt(fa, bl->ohne_kachel ? T("%d Teile · ohne Kachel", "%d parts · no tile") : T("%d Teile", "%d parts"), bl->teile.n); }
    const char *folge = reste ? T("nichts geht verloren", "nothing is lost") : schild_text(einzeln ? f0 : bl->kopf, k);
    const char *unter = gesp && g && g[0] ? VERB(fa, T("Gesperrt: ", "Locked: "), g)
                      : was[0] && folge && folge[0] && strcmp(was, folge) ? VERB(fa, was, " · ", folge) : was[0] ? was : folge ? folge : "";
    uint32_t nf = gesp ? S_GRAU : S_TEXT_W;
    if (unter[0]) {
        name_zeichnen(nx, ry + 39, 2, 28, 24, nf, name, rechts - nx, fok, einzeln ? (const void *)f0 : (const void *)bl->kopf);
        text(nx, ry + 72, 0, 24, gesp ? S_TEXT2 : S_GRAU, kuerzen(0, 24, unter, rechts - nx));
    } else name_zeichnen(nx, cy + 10, 2, 28, 24, nf, name, rechts - nx, fok, einzeln ? (const void *)f0 : (const void *)bl->kopf);
    if (warn) {   /* Warnung unter der Zeile (Entwurf 2b) */
        int wx = ox + 16 + 58, wy = ry + AL_KOPF, ww = ow - 16 - 58 - 16, wh = AL_WARN - 12;
        if (wjb) gold11(wx, wy, ww, wh, 12, 1);
        else { flaeche f = { false, V2(135, N_ROT, .22f, 0x2A0D0D, .55f), N_ROT, 1, 1, 1, 0, 0 }; flaeche_zeichnen(wx, wy, ww, wh, 12, &f); }
        symbol(SYM_WARNUNG, wx + 16, wy + wh / 2 - 16, 32, wjb ? S_GOLD : N_ROT);
        passend p = einpassen(0, 26, 26, 20, warn, ww - 16 - 32 - 16 - 16, 2); p.lh = p.gr + 4;
        int ty = wy + (wh - p.z.n * p.lh) / 2 + p.gr - 4;
        for (int i = 0; i < p.z.n; i++) text(wx + 16 + 32 + 16, ty + i * p.lh, 0, p.gr, wjb ? S_GOLD_T : 0xFFD0CE, p.z.z[i]);
    }
}
static void al_teil_zeile(int ox, int ow, int ry, int b, int t, int k, bool fok, const char *gleich)
{
    (void)gleich;
    const kblock *bl = &AL.bl[b]; const char *g;
    eintrag *f = bl->teile.e[t]; int cy = ry + AL_TEIL / 2;
    if (fok) fokus11(ox, ry, ow, AL_TEIL, 12, 2);
    bool gesp = ablauf_eintrag_gesperrt(&A, f, &g);
    int eb = teil_ebene(bl, t), ein = eb == 3 ? 44 : 0;   /* NEU 1.1: Untergruppe eingerückt, ihr Kopf mit Bild */
    haken42(ox + 74 + ein, cy - 21, f->Wahl ? 2 : 0, gesp, k == 7 || f->Jailbreak);
    int nx = ox + 74 + ein + 42 + 16, xr = ox + ow - 16;
    if (eb == 2) { bild_kachel11(nx, cy - 18, 36, 8, f, k, al_name(f), gesp ? .55f : 1.f); nx += 36 + 12; }
    const char *gr = gesp ? "–" : h_trim(fa, ablauf_eintrag_groesse(&A, f));
    text_rechts(xr, cy + 10, 1, 26, S_TEXT_W, gr);
    const char *ok = ort_lang(anzeige_ort(f));
    int gw = textbreite(1, 26, gr); if (gw < 116) gw = 116;
    int okx = xr - gw - 16; ort_kaestchen(okx, cy, ok);
    int rechts = okx - ort_kaestchen_breite(ok) - 16;
    const char *name = bl->kopf == &RESTE_ZEILE ? ohne_ort_vorne(kern_zeile(fa, f)) : unter_name(bl, t, ohne_jb_start(teil_name(b, f), k));
    const char *zu = gesp && g && g[0] ? VERB(fa, T("Gesperrt: ", "Locked: "), g) : schild_text(f, k);
    int zb = zu && zu[0] ? textbreite(0, 24, zu) : 0;
    /* NEU 1.3 (auf der PS5 gesehen 21.11.2026 18:22: "Gesperrt: Geht nur zusammen mit dem Spiel we…", bei langen Namen "Geht nur zusammen m…"):
     * passt der Sperrgrund "nur mit dem Spiel" nicht neben den Namen, steht er in der Zeile kurz da - der ganze Satz steht im Kasten rechts. */
    if (gesp && zb && kern_nur_mit_spiel(K, f) && textbreite(2, 26, name) + 14 + zb > rechts - nx) {
        zu = T("Gesperrt: nur mit dem Spiel", "Locked: only with the game"); zb = textbreite(0, 24, zu);
    }
    int nmax = rechts - nx - (zb ? zb + 14 : 0); if (nmax < (rechts - nx) / 2) nmax = (rechts - nx) / 2;
    /* NEU 1.3 (PS5-Test da_Chris86 21.11.2026 13:3x: "PS5-Cooling-Center – Einstellungen – erkannt, nicht be…"): der Name geht vor - passt er
     * neben dem grauen Zusatz ("neu einrichten") auch in der kleinsten Schrift nicht, entfällt der Zusatz in dieser Zeile (er steht im Kasten
     * rechts). Ein Sperrgrund bleibt immer stehen. */
    if (zb && !gesp && textbreite(2, 22, name) > nmax) { zb = 0; nmax = rechts - nx; }
    int nb = name_zeichnen(nx, cy + 10, 2, 26, 22, gesp ? S_GRAU : S_TEXT_W, name, nmax, fok, f);
    if (zb) text(nx + nb + 14, cy + 10, 0, 24, gesp ? S_TEXT2 : S_GRAU, kuerzen(0, 24, zu, rechts - nx - nb - 14));
}
/* ---- der Bildschirm */
static void schirm_liste(void)
{
    /* NEU 1.1 (Entwurf 15.11.2026, Seite "2 – Auswählen"): Bereiche als Reiter oben (L1/R1), die Einträge in der Mitte, rechts der
     * Kasten zum markierten Eintrag; die Auswahl steht oben rechts, "Weiter zum Löschen" (□) unten rechts. Die Bedienung der Liste
     * (↑↓, ✕, Blöcke mit Teilen) ist wie bisher. */
    hintergrund11();
    ablauf_texte_frei(&A);
    sicht_neu();
    fokus_zone = 1;
    int k = sicht[fokus_bereich];
    al_ordnen(k);
    bilder_neu = 0;
    /* Auswahl insgesamt */
    int gw = 0; long long gb = 0; bool galle = true, ggek = false, rot = false, jbw = false;
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i]; if (!f->Wahl) continue;
        gw++; if (kern_ampel(f) == AMPEL_ROT) rot = true; if (f->Jailbreak) jbw = true;
        groesse *g = kern_eintrag_groesse_in(K, fa, f, true); if (!g) galle = false; else { gb += g->Bytes; if (g->Gekappt) ggek = true; }
    }
    /* Kopf: Schriftzug | Was soll weg? ... Schritte, Auswahl-Pille */
    {
        int cy = R11_O + 22, x = R11_X;
        x += logo11(x, cy - 22) + 24; rechteck(x, cy - 17, 2, 34, 0x222C3A); x += 26;
        text(x, gl11(cy - 22, 44, 30), 1, 30, S_TEXT, T("Was soll weg?", "What should go?")); x += textbreite(1, 30, T("Was soll weg?", "What should go?")) + 24;
        x = warn_pillen11(x, cy);
        const char *pt = gw == 0 ? T("Nichts ausgewählt", "Nothing selected")
                                 : VERB(fa, a_fmt(fa, T("%d ausgewählt", "%d selected"), gw), galle ? VERB(fa, " · ", groesse_von(fa, gb, ggek)) : "");
        int pw = 24 + 34 + 12 + textbreite(1, 26, pt) + 24, px = BREITE - R11_X - pw;
        bool jbp = jbw && k == 7;
        if (jbp) gold11(px, cy - 25, pw, 50, 25, 2);
        else { flaeche f = { false, V1(0xFFFFFF, .05f), 0x3B4A63, 1, 1, 1, 0, 0 }; flaeche_zeichnen(px, cy - 25, pw, 50, 25, &f); }
        symbol(SYM_HAKEN, px + 24, cy - 17, 34, gw ? (jbp ? S_GOLD : S_SYM) : S_GRAU2);
        text(px + 24 + 34 + 12, gl11(cy - 25, 50, 26), 1, 26, gw ? S_TEXT_W : S_GRAU, pt);
        /* NEU 1.3 (da_Chris86 20.11.2026: App und Browser gleich): Optionen-Knopf neben der Auswahl-Pille wie im Browser (fern_seite.html zListe) -
         * die Options-Taste öffnet hier das Menü, die Fußleiste ist voll. Die Schritte passen daneben (sonst in Kurzform). */
        const char *ot = T("Optionen", "Options");
        int ow = 22 + 24 + 12 + textbreite(1, 26, ot) + 22, ox = px - 16 - ow;
        { flaeche f = { false, V1(0xFFFFFF, .07f), 0x3B4A63, 1, 1, 1, 0, 0 }; flaeche_zeichnen(ox, cy - 25, ow, 50, 25, &f); }
        symbol(SYM_KN_OPTIONEN, ox + 22, cy - 25 + 13, 24, S_TEXT_W);
        text(ox + 58, gl11(cy - 25, 50, 26), 1, 26, S_TEXT_W, ot);
        schritte11_passend(x, ox - 12, cy, 2);   /* nur, wenn Platz ist (1.3: sonst in Kurzform) */
    }
    /* Reiter */
    int ty = R11_O + 50 + 20, th = 46;
    {
        int gws[8] = { 0 }; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Wahl && f->Kat >= 0 && f->Kat < 8) gws[f->Kat]++; }
        const char *lab[8]; int lb[8], gesamt = 0;
        for (int i = 0; i < sicht_n; i++) {
            int kk = sicht[i], n = bereich_anzahl(kk);
            char z[48]; if (gws[kk]) { if (n > 999) snprintf(z, sizeof z, T("%d von 999+", "%d of 999+"), gws[kk] > 999 ? 999 : gws[kk]); else snprintf(z, sizeof z, T("%d von %d", "%d of %d"), gws[kk], n); }
            else snprintf(z, sizeof z, n > 999 ? "999+" : "%d", n);
            lab[i] = a_kopie(fa, z);
            lb[i] = 6 + 44 + 8 + textbreite(1, 24, kern_kat_name(kk)) + 8 + textbreite(2, 24, lab[i]) + 12;
            gesamt += lb[i] + 6;
        }
        int l1w = kappe11(0, -100, "L1", 0) , frei = BREITE - 2 * R11_X - 2 * (l1w + 8);
        bool kompakt = gesamt > frei;   /* passt die Leiste nicht: nicht gewählte Reiter nur mit Symbol und Zahl (da_Chris86 15.11.2026 13:43) */
        int x = R11_X, cy = ty + th / 2;
        x += kappe11(x, cy, "L1", S_TEXT2) + 8;
        for (int i = 0; i < sicht_n; i++) {
            int kk = sicht[i]; bool akt = i == fokus_bereich, jb = kk == 7;
            bool name = !kompakt || akt;
            int w = 6 + 44 + 8 + (name ? textbreite(1, 24, kern_kat_name(kk)) + 8 : 0) + textbreite(2, 24, lab[i]) + 12;
            if (akt) {
                if (jb) { leuchten(x, ty, w, th, th / 2, 18, S_GOLD, .45f); rund(x, ty, w, th, th / 2, S_GOLD); }
                else { leuchten(x, ty, w, th, th / 2, 16, S_BLAU, .4f); rund(x, ty, w, th, th / 2, S_BLAU); }
                { flaeche d = { false, V1(0x0D1119, .6f), 0, 0, 0, 0, 0, 0 }; flaeche_zeichnen(x + 6, cy - 17, 44, 34, 12, &d); }
            } else { flaeche f = { true, V1(0x19232E, .62f), 0xFFFFFF, .09f, .09f, 1, 0, 0 }; flaeche_zeichnen(x, ty, w, th, th / 2, &f); }
            const bild11 *b = &b11_sym_k[kat_index11(kk)]; if (b->px) bild11_zeichnen(b, x + 10, cy - 15, 36, 30, 0);
            int tx = x + 6 + 44 + 8; uint32_t nf = akt ? (jb ? 0x241A05 : S_TEXT_W) : (jb ? S_GOLD : S_TEXT);
            if (name) { text(tx, gl11(ty, th, 24), 1, 24, nf, kern_kat_name(kk)); tx += textbreite(1, 24, kern_kat_name(kk)) + 8; }
            text(tx, gl11(ty, th, 24), 2, 24, akt ? nf : S_GRAU, lab[i]);
            x += w + 6;
        }
        kappe11(x + 2, cy, "R1", S_TEXT2);
    }
    int y0 = ty + th + 20, ph = FUSS11_Y - 20 - y0, kw = 520, kx = BREITE - R11_X - kw, lx = R11_X, lw = kx - 24 - lx;
    /* Mitte: Einträge */
    karte11(lx, y0, lw, ph, 20);
    const char *gleich = NULL, *was_gleich = NULL;   /* 1.1: jede Zeile sagt selbst, was danach passiert (Entwurf) */
    int ox = lx + 16, ow = lw - 16 - 30, oy = y0 + 56, oh = y0 + ph - 8 - oy;
    {
        int n = bereich_anzahl(k);
        const char *rf = k == 7 ? T("was der Jailbreak zum Starten braucht", "what the jailbreak needs to start") : sortierung == SORT_AZ ? "A–Z" : T("größte zuerst", "largest first");
        const char *zt = VERB(fa, a_fmt(fa, "%d", n), EINTRAEGE(n), " · ", rf);
        if (A.hintergrund && A.hg_gesamt > 0 && A.hg_n < A.hg_gesamt) zt = VERB(fa, zt, a_fmt(fa, T(" · Größen werden gezählt (%d von %d)", " · counting sizes (%d of %d)"), A.hg_n, A.hg_gesamt));
        text_passend(lx + 24, y0 + 40, 0, 24, 20, S_GRAU, zt, lw - 48 - 260);
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
        if (z[i].t == -1) al_kopf_zeile(ox, ow, ry, b, k, fokus_zone == 1 && i == fz, gleich, was_gleich);
        else al_teil_zeile(ox, ow, ry, b, z[i].t, k, fokus_zone == 1 && i == fz, gleich);
    }
    if (gesamt > oh) {   /* Bildlaufleiste */
        int sx = lx + lw - 16, sy0 = y0 + 56, sh0 = y0 + ph - 14 - sy0, th = sh0 * oh / gesamt; if (th < 40) th = 40;
        { flaeche sb = { false, V1(0xFFFFFF, .08f), 0, 0, 0, 0, 0, 0 }; flaeche_zeichnen(sx, sy0, 6, sh0, 3, &sb); }
        rund(sx, sy0 + (sh0 - th) * al_py / (gesamt - oh > 0 ? gesamt - oh : 1), 6, th, 3, S_GRAU2);
    }
    {   /* oben rechts in der Karte: welche Zeilen zu sehen sind */
        const char *bz = AL.n == 0 || erster < 0 ? "" : erster == 0 && letzter == AL.n - 1 ? (AL.n == 1 ? T("1 Zeile", "1 row") : a_fmt(fa, T("%d Zeilen", "%d rows"), AL.n))
                                                       : a_fmt(fa, T("Zeile %d–%d von %d", "rows %d–%d of %d"), erster + 1, letzter + 1, AL.n);
        text_rechts(lx + lw - 38, y0 + 40, 0, 24, S_GRAU, bz);
    }
    /* rechts: Kasten zum markierten Eintrag (Entwurf: ganze Höhe; was danach passiert, steht unten im Kasten) */
    al_kasten(kx, y0, kw, ph, k);
    (void)rot;
    /* Fußleiste und "Weiter zum Löschen" */
    bool auswahl = gw > 0;
#ifdef NUR_LESEN
    auswahl = false;
#endif
    if (testmodus) auswahl = false;
    {
        const char *wt = T("Weiter zum Löschen", "Continue to delete");
        int bw = 24 + 32 + 16 + textbreite(1, 26, wt) + 24, bh = 60, bx = BREITE - R11_X - bw, by = FUSS11_Y + 22 - bh / 2;
        if (auswahl) { leuchten(bx, by, bw, bh, 16, 20, S_BLAU, .45f); flaeche f = F_HAUPT11(); flaeche_zeichnen(bx, by, bw, bh, 16, &f); }
        else { flaeche f = F_NEBEN11(); flaeche_zeichnen(bx, by, bw, bh, 16, &f); }
        ring(bx + 24 + 16, by + bh / 2, 14.5f, 1.8f, auswahl ? S_TEXT_W : S_GRAU2); symbol(SYM_KN_QUADRAT, bx + 24 + 9, by + bh / 2 - 7, 14, auswahl ? S_TEXT_W : S_GRAU2);
        text(bx + 24 + 32 + 16, gl11(by, bh, 26), 1, 26, auswahl ? S_TEXT_W : S_GRAU, wt);
    }
    {   /* Tasten links */
        int cy = FUSS11_Y + 22, x = R11_X;
        /* NEU 1.1 (da_Chris86 16.11.2026 17:55: "Teile zeigen ... springt er nur auf die Kachel. Sonst macht er gar nichts"): die Teile eines
         * Blocks stehen schon offen da, sobald er markiert ist - → führt hinein, ← wieder hinaus. Die Leiste sagt jetzt, was die Taste dort
         * wirklich tut: auf dem Kopf eines Blocks mit Teilen "Teile wählen", in einem Block "Zurück zum Block", sonst nichts */
        bool im_block = AL.n > 0 && al_t >= 0, teile = AL.n > 0 && al_t < 0 && al_mehr(al_b);
        struct { int art; const char *k, *t; } tn[6] = {
            { KN_X, NULL, T("Haken setzen", "Tick") }, { im_block ? -3 : teile ? -2 : 0, NULL, im_block ? T("Zurück zum Block", "Back to block") : teile ? T("Teile wählen", "Choose parts") : NULL },
            { KN_DREIECK, NULL, T("Alle an/aus", "All on/off") },
            { -1, "L1R1", T("Bereich", "Area") }, { -1, "L2R2", T("Seite", "Page") }, { KN_O, NULL, T("Zurück", "Back") } };
        for (int i = 0; i < 6; i++) {
            if (!tn[i].t) continue;
            /* NEU 1.1 (17:58): jede Taste auf eigener heller Fläche, weiß (wie fuss11) - erst die Breite, dann zeichnen */
            int kw = tn[i].art >= 0 || tn[i].art <= -2 ? 42 : 0;
            char a[3] = { 0 }, b[3] = { 0 };
            if (tn[i].art == -1) { a[0] = tn[i].k[0]; a[1] = tn[i].k[1]; b[0] = tn[i].k[2]; b[1] = tn[i].k[3]; kw = textbreite(1, 22, a) + 14 + 12 + textbreite(1, 22, b) + 14; }
            int w = fuss_pille11(x, cy, kw + 12 + textbreite(1, 24, tn[i].t)), xi = x + FUSS_PILLE_L;
            if (tn[i].art == -2 || tn[i].art == -3) { ring(xi + 21, cy, 19.5f, 1.6f, S_TEXT_W); symbol(tn[i].art == -2 ? SYM_RECHTS : SYM_LINKS, xi + 11, cy - 10, 20, S_TEXT_W); }
            else if (tn[i].art >= 0) knopf11((knopfart)tn[i].art, xi, cy, S_TEXT_W);
            else { int wa = kappe11(xi, cy, a, S_TEXT_W); kappe11(xi + wa + 12, cy, b, S_TEXT_W); }
            text(xi + kw + 12, gl11(FUSS11_Y, 44, 24), 1, 24, S_TEXT_W, tn[i].t); x += w + 12;
        }
    }
    hinweis_ueber = FUSS11_Y - 16;
    fuss11_ohne_version = true; hinweis_mx11 = lx + lw / 2;   /* Meldung über der Liste, nicht über dem Hinweis rechts */
    fuss11(NULL, 0, NULL);   /* nur kurze Meldung und Halten-Balken */
    if (optionen_offen) optionen11();
}

/* =================================================================== NEU 1.0.8 (Arbeitsplan 4.4): Bestätigen nach Entwurf 2
 * Die letzte Frage vor dem Löschen bei "Nur Auswahl löschen" (Titel "Endgültig löschen?"): oben was gelöscht wird, in Gruppen nach dem, was
 * danach passiert (rot endgültig weg, gelb neu laden oder einrichten, grün es geht nichts verloren), darunter die Hinweise des Kerns, was
 * immer bleibt, und zwei Knöpfe. Die Seite startet auf "Zurück zur Auswahl"; gelöscht wird nur, wenn ✕ auf "Löschen" 1,5 s gehalten wird
 * (Loslassen bricht ab). Die Frage selbst stellt der Kern wie bisher (frage_wahl) - Mitschnitt und Nachbau bleiben gleich. */
#define HALTEN_MS 2000   /* 1.1 (Entwurf "3 – Bestätigen"): 2 Sekunden */
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
    if (!bestaetigen_aktiv() || G.art != D_WAHL || G.dlg_nr != dlg_gesehen || SDL_GetTicks() - dlg_seit < 400) return false;   /* 1.1: ohne → (Entwurf) */
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
    if (!bestaetigen_aktiv() || G.art != D_WAHL || protokoll_offen || optionen_offen) { x_seit = 0; return; }
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
        for (int i = 0; i < 26; i++) { float al = (float)(i + 1) / 26.f; for (int px = x - 4; px < x + w; px++) hg_misch(px, y + hoehe + 6 - 26 + i, al); }   /* 1.1: zum Hintergrundbild hin */
        if (off > 0) for (int i = 0; i < 14; i++) { float al = (float)(14 - i) / 14.f; for (int px = x - 4; px < x + w; px++) hg_misch(px, y - 2 + i, al); }
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
/* NEU 1.1 (Entwurf "3 – Bestätigen"): Knopf "Zurück zur Auswahl" und die Halte-Karte "Löschen" nebeneinander (rechte Spalte) */
static void zurueck_knopf11(int x, int y, int w, int h, bool fokus)
{
    const char *zt = T("Zurück zur Auswahl", "Back to selection");
    if (fokus) fokus11(x, y, w, h, 18, 2); else { flaeche f = F_NEBEN11(); flaeche_zeichnen(x, y, w, h, 18, &f); }
    knopf11(KN_O, x + 22, y + h / 2, fokus ? S_TEXT_W : S_TEXT2);
    text_passend(x + 22 + 42 + 14, gl11(y, h, 26), 1, 26, 22, fokus ? S_TEXT_W : S_TEXT, zt, w - 22 - 42 - 14 - 18);
}
static void halte_karte11(int x, int y, int w, int h, bool fokus, bool bereit, const char *titel, const char *satz, float anteil)
{
    if (fokus && bereit) fokus11(x, y, w, h, 20, 2);
    else { flaeche f = F_NEBEN11(); flaeche_zeichnen(x, y, w, h, 20, &f); }
    int cx = x + 24 + 34, cy = y + h / 2;
    uint32_t rf = bereit ? (fokus ? S_TUERKIS : S_GRAU2) : S_GRAU3;
    kreis(cx, cy, 33, 0x0F1826);
    ring(cx, cy, 30, 3, bereit ? 0x2A3B57 : 0x1E2836);
    if (anteil > 0.f) ring_teil(cx, cy, 30, 6, anteil, S_TUERKIS);
    else if (bereit && fokus) ring_teil(cx, cy, 30, 3, .3f, rf);
    symbol(SYM_KN_KREUZ, cx - 12, cy - 12, 24, bereit ? S_TEXT_W : S_GRAU2);
    int tx = cx + 34 + 22, tb = x + w - 20 - tx;
    text_passend(tx, cy - 6, 3, 30, 24, bereit ? S_TEXT_W : S_GRAU, titel, tb);
    text_passend(tx, cy + 28, 0, 24, 20, bereit ? S_TEXT2 : S_GRAU2, satz, tb);
}
/* NEU 1.0.8 (4.4, da_Chris86 05.11.2026 21:33: "ne top passt gut mitgedacht"): vollständige Bereinigung - dieselbe Übersicht, unten aber
 * wie bisher LÖSCHEN eintippen (dort geht am meisten weg). "Zurück zur Auswahl" (Fokus 1), das Feld (Fokus 0, hier startet die Seite),
 * "Löschen" (Fokus 2). Gelöscht wird nur mit genau dem Wort, das der Kern verlangt (ablauf_komplett). Anderer Text bricht nicht ab, die
 * Seite sagt stattdessen, was fehlt ("Zurück" bricht ab). 1.1: das Feld steht über den beiden Knöpfen (rechte Spalte). */
static void bestaetigen_tippen(int rx, int rw, int kyb, int kh)
{
    const char *wort = T("LÖSCHEN", "DELETE");
    bool passt = eingabe_passt(), getippt = G.eingabe[0] != 0;
    if (passt) hinweis_bis = 0;   /* "Erst LÖSCHEN eintippen ..." gilt nicht mehr */
    bool fz = dlg_fokus == 1, ff = dlg_fokus == 0, fl = dlg_fokus == 2;
    int fh = 96, fy = kyb - 16 - fh, zw = 360;
    /* Feld */
    if (ff) fokus11(rx, fy, rw, fh, 18, 2);
    else { flaeche f = F_FELD11(); if (passt) { f.rand = S_GRUEN; f.rand_oben = f.rand_unten = .8f; } else if (getippt) { f.rand = S_GOLD; f.rand_oben = f.rand_unten = .8f; } flaeche_zeichnen(rx, fy, rw, fh, 18, &f); }
    {
        const char *kt = tastatur_offen ? T("Tastatur ist offen ...", "Keyboard is open ...") : T("✕ = Tastatur öffnen", "✕ = open keyboard");
        text(rx + 28, fy + 36, 0, 24, ff ? S_TEXT2 : S_GRAU2, kt);
        if (getippt) text_passend(rx + 28, fy + 78, 1, 30, 24, passt ? S_GRUEN : S_GOLD, G.eingabe, rw - 56 - 40);
        else text(rx + 28, fy + 78, 1, 30, S_GRAU, VERB(fa, T("", "Type "), wort, T(" eintippen", "")));
        if (passt) symbol(SYM_HAKEN, rx + rw - 28 - 30, fy + fh / 2 - 15, 30, S_GRUEN);
    }
    zurueck_knopf11(rx, kyb, zw, kh, fz);
    halte_karte11(rx + zw + 16, kyb, rw - zw - 16, kh, fl, passt, T("Löschen", "Delete"),
                  passt ? T("jetzt mit ✕ - löscht alles hier Gezeigte", "now with ✕ - deletes everything shown here") : VERB(fa, T("erst ", "type "), wort, T(" eintippen", " first")), 0.f);
    const char *unten = getippt && !passt ? VERB(fa, T("Das ist nicht ", "That is not "), wort, T(" - bitte genau so eintippen, in Großbuchstaben.", " - please type it exactly like this, in capital letters."))
                      : passt ? T("Bis jetzt wurde nichts gelöscht. ✕ auf „Löschen“ löscht alles Gezeigte endgültig.", "Nothing has been deleted yet. ✕ on 'Delete' deletes everything shown for good.")
                      : VERB(fa, T("Bis jetzt wurde nichts gelöscht. ", "Nothing has been deleted yet. Type "), wort, T(" eintippen, dann „Löschen“.", ", then 'Delete'."));
    text_passend(rx + 4, fy - 14, 0, 24, 20, getippt && !passt ? S_GOLD : S_GRAU, unten, rw - 8);
    fussknopf kn[3] = { { KN_X, ff ? T("Tastatur öffnen", "Open keyboard") : fl ? T("Löschen", "Delete") : T("Zurück zur Auswahl", "Back to selection"), false },
                        { KN_O, T("Zurück zur Auswahl", "Back to selection"), false }, { KN_LINKSRECHTS, T("wechseln", "switch"), false } };
    hinweis_ueber = fy - 50;
    fuss_neu(kn, 3);
}
/* NEU 1.1: rechte Spalte - was immer bleibt (grüne Karte) */
static int bleibt_karte11(int x, int y, int w, bool jb_gew, bool jb_da, bool zeichnen)
{
    const char *p[6]; int n = 0;
    p[n++] = T("Laufwerkskopplung", "Disc drive pairing"); p[n++] = T("Lizenzen und Konten", "Licenses and accounts");
    p[n++] = T("Systemsoftware", "System software");
    if (jb_da && !jb_gew) { const char *jn = kopf_jb_name(); p[n++] = jn && jn[0] ? VERB(fa, "Jailbreak ", jn) : T("Jailbreak", "Jailbreak"); }
    p[n++] = T("Alles nicht Angehakte", "Everything not ticked");
    int sw = (w - 56 - 20) / 2, zh = 0;
    for (int i = 0; i < n; i += 2) {   /* Zeilenhöhe nach dem längeren der beiden Punkte (bis 2 Zeilen) */
        int z1 = umbrechen(0, 26, p[i], sw - 38).n, z2 = i + 1 < n ? umbrechen(0, 26, p[i + 1], sw - 38).n : 1;
        int m = z1 > z2 ? z1 : z2; if (m > 2) m = 2; zh += m * 32 + 12;
    }
    int h = 24 + 44 + 16 + zh + 12;
    if (!zeichnen) return h;
    gruen11(x, y, w, h, 20);
    symbol(SYM_SCHILD_HAKEN, x + 28, y + 24, 44, S_GRUEN);
    text(x + 28 + 44 + 16, gl11(y + 24, 44, 32), 3, 32, S_TEXT_W, T("Bleibt unangetastet", "Stays untouched"));
    int yy = y + 24 + 44 + 16;
    for (int i = 0; i < n; i += 2) {
        int m = 1;
        for (int c = 0; c < 2 && i + c < n; c++) {
            int sx = x + 28 + c * (sw + 20);
            zeilen z = umbrechen(0, 26, p[i + c], sw - 38); if (z.n > 2) z.n = 2; if (z.n > m) m = z.n;
            symbol(SYM_HAKEN, sx, yy + (z.n > 1 ? 18 : 3), 26, S_GRUEN);
            for (int j = 0; j < z.n; j++) text(sx + 38, yy + 25 + j * 32 - (z.n > 1 ? 0 : 0), 0, 26, S_TEXT_W, z.z[j]);
        }
        yy += m * 32 + 12;
    }
    return h;
}
/* NEU 1.1: rechte Spalte - Speicher danach (Balken je Laufwerk: belegt, was frei wird, frei) */
/* PS5-Test 17.11.2026 17:57/17:58: "3,64 TB frei · +1,04 TB" las sich wie "noch 1,04 TB dazu" - jetzt "jetzt → danach frei"; und mit M.2 fehlte
 * die Karte im Browser (zu wenig Platz) - max_h > 0: die Zeilen rücken zusammen (höchstens bis 40 Pixel je Laufwerk), statt die Karte wegzulassen */
static const char *speicher_frei_text(long long jetzt, long long danach)
{
    const char *a = kern_groesse_text(fa, jetzt), *b = kern_groesse_text(fa, danach);
    return strcmp(a, b) ? VERB(fa, a, " → ", b, T(" frei", " free")) : VERB(fa, b, T(" frei", " free"));
}
static int speicher_danach11(int x, int y, int w, const long long *weg, bool zeichnen, int max_h)
{
    speicher11 sp = speicher11_holen();
    int o_l[4], n = 0;
    for (int o = 0; o < 4; o++) if (sp.da[o] && (o == 0 || weg[o] > 0)) o_l[n++] = o;
    if (!n) return 0;
    int p = 66, h = 70 + n * p;
    if (max_h > 0 && h > max_h) { p = (max_h - 70) / n; if (p < 40) return 0; h = 70 + n * p; }
    if (!zeichnen) return h;
    karte11(x, y, w, h, 20);
    text_gesperrt(x + 24, y + 20 + 24, 1, 24, S_GRAU2, T("SPEICHER DANACH", "STORAGE AFTERWARDS"), .14f);
    {   /* Legende */
        const char *l1 = T("bleibt", "stays"), *l2 = T("wird frei", "freed");
        int lx = x + w - 24 - textbreite(0, 24, l2); text(lx, y + 20 + 24, 0, 24, S_GRAU, l2); rund(lx - 24, y + 20 + 7, 16, 16, 4, S_TUERKIS);
        lx -= 24 + 20 + textbreite(0, 24, l1); text(lx, y + 20 + 24, 0, 24, S_GRAU, l1); rund(lx - 24, y + 20 + 7, 16, 16, 4, S_GRAU3);
    }
    int gn = p >= 52 ? 26 : 22, gr = p >= 52 ? 24 : 20, bh = p * 14 / 66 < 8 ? 8 : p * 14 / 66;
    for (int i = 0; i < n; i++) {
        int o = o_l[i], ry = y + 58 + i * p, ty = ry + 32 * p / 66;
        long long ges = sp.ges[o], jetzt = sp.frei[o] < ges ? sp.frei[o] : ges, frei = sp.frei[o] + weg[o]; if (frei > ges) frei = ges;
        text(x + 24, ty, 1, gn, S_TEXT, ort_name11(o, sp.usb_n));
        const char *rt = speicher_frei_text(jetzt, frei);
        const char *pt = weg[o] > 0 ? VERB(fa, " · +", kern_groesse_text(fa, weg[o])) : "";
        int pw = textbreite(1, gr, pt);
        text_rechts(x + w - 24 - pw, ty, 0, gr, S_TEXT2, rt);
        if (pt[0]) text(x + w - 24 - pw, ty, 1, gr, S_TUERKIS, pt);
        int bx = x + 24, bw = w - 48, by = ry + 44 * p / 66;
        { flaeche sb = { false, V1(0xFFFFFF, .08f), 0, 0, 0, 0, 0, 0 }; flaeche_zeichnen(bx, by, bw, bh, bh / 2, &sb); }
        double belegt = ges > 0 ? (double)(ges - frei) / (double)ges : 0, wf = ges > 0 ? (double)weg[o] / (double)ges : 0;
        int b1 = (int)(belegt * bw + .5), b2 = (int)(wf * bw + .5); if (b2 < 4 && weg[o] > 0) b2 = 4; if (b1 + b2 > bw) b1 = bw - b2;
        if (b1 > 0) rund(bx, by, b1 < bh ? bh : b1, bh, bh / 2, S_GRAU3);
        if (b2 > 0) rund(bx + b1, by, b2, bh, 3, S_TUERKIS);
    }
    return h;
}
/* NEU 1.1 (Entwurf "3 – Bestätigen"): Halte-Karte - Ring mit Fortschritt, "2 Sekunden gedrückt halten" */
static void halte_karte_entwurf(int x, int y, int w, int h, float anteil, bool gold, const char *titel, const char *satz)
{
    uint32_t rf = gold ? S_GOLD : S_BLAU;
    leuchten(x, y, w, h, 24, 32, rf, .4f);
    flaeche f = { true, gold ? V2(160, 0x5A4310, .55f, 0x2A1F08, .6f) : V2(160, 0x2D64B4, .55f, 0x183056, .6f), gold ? S_GOLD : S_BLAU, 1, 1, 2, .25f, gold ? 0xFCD378 : 0xAACDFF };
    flaeche_zeichnen(x, y, w, h, 24, &f);
    int cx = x + 24 + 34, cy = y + h / 2;
    kreis(cx, cy, 34, 0x1C2635);
    ring_teil(cx, cy, 30.5f, 7, 1.f, 0x2A3443);
    if (anteil > 0.f) ring_teil(cx, cy, 30.5f, 7, anteil, gold ? S_GOLD : S_TUERKIS);
    kreis(cx, cy, 27, gold ? 0x1E1606 : 0x08192A);
    symbol(gold ? SYM_KN_DREIECK : SYM_KN_KREUZ, cx - 15, cy - 15, 30, 0xFFFFFF);
    int tx = cx + 34 + 24, tb = x + w - 24 - tx;
    text_passend(tx, cy - 4, 3, 30, 24, S_TEXT_W, titel, tb);
    text_passend(tx, cy + 32, 0, 24, 20, S_TEXT2, satz, tb);
}
static void schirm_bestaetigen(void)
{
    /* NEU 1.1 (Entwurf "3 – Bestätigen", da_Chris86 15.11.2026 "Die Vorlage ist das neue Design"): links was gelöscht wird, nach Bereichen,
     * rechts was unangetastet bleibt, der Speicher danach und die Halte-Karte. ✕ 2 Sekunden halten löscht, ○ geht zurück zur Auswahl. */
    hintergrund11();
    kopf_neu(3, NULL);
    bilder_neu = 0;
    int nmax = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Wahl) nmax++;
    static const int KO[8] = { 1, 6, 2, 3, 4, 5, 7, 0 };
    b_zeile *gz[8]; int gn[8] = { 0 }; long long weg[4] = { 0, 0, 0, 0 };
    for (int g = 0; g < 8; g++) gz[g] = a_hole(fa, sizeof(b_zeile) * (size_t)(nmax + 1));
    bool jb_gew = false, jb_da = false;
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i];
        if (f->Jailbreak) jb_da = true;
        if (!f->Wahl || f->Gesperrt) continue;
        if (f->Jailbreak) jb_gew = true;
        int g = 7; for (int j = 0; j < 8; j++) if (KO[j] == f->Kat) g = j;
        groesse *gr = kern_eintrag_groesse_in(K, fa, f, true);
        gz[g][gn[g]++] = (b_zeile){ f, gr ? gr->Bytes : -1, gr != NULL, gr ? gr->Gekappt : false };
        if (gr) { int o = anzeige_ort(f); if (o >= 0 && o < 4) weg[o] += gr->Bytes; }
    }
    for (int g = 0; g < 8; g++) qsort(gz[g], (size_t)gn[g], sizeof(b_zeile), b_vergleich);
    int y0 = R11_O + 50 + 20, unten = FUSS11_Y - 20, lw = (BREITE - 2 * R11_X - 24) / 2, lx = R11_X, rx = lx + lw + 24, rw = BREITE - R11_X - rx;
    /* links: Das wird gelöscht */
    karte11(lx, y0, lw, unten - y0, 20);
    {
        const char *gt = A.GROSS.Bytes > 0 ? groesse_von(fa, A.GROSS.Bytes, A.GROSS.Gekappt) : "";
        int gw = gt[0] ? textbreite(3, 52, gt) : 0;
        text_passend(lx + 32, y0 + 32 + 46, 3, 44, 30, S_TEXT_W, tippen_aktiv() ? T("Alles wird gelöscht", "Everything will be deleted") : T("Das wird gelöscht", "This will be deleted"), lw - 64 - gw - 16);
        if (gt[0]) text_rechts(lx + lw - 32, y0 + 32 + 48, 3, 52, S_TEXT_W, gt);
    }
    /* 1.3 FIX (PS5-Test da_Chris86 20.11.2026 21:13: vier Gruppen und sieben Hinweise - es standen nur "und N weitere" da, ohne einen
     * Eintrag mit Bild, und die Gruppe JAILBREAK fehlte ganz). Neue Reihenfolge: zuerst die rechte Spalte ausmessen. Ist zwischen
     * "Speicher danach" und der Halte-Karte Platz, kommen die Hinweise dorthin und links bleibt der ganze Platz für die Gruppen.
     * Sonst stehen sie wie bisher links unten, aber höchstens so hoch, dass alle Gruppen passen (läuft durch, wenn es mehr ist).
     * Passen die Gruppen auch ohne Zeilen nicht, steht jede als eine Zeile mit Anzahl da (Kurzform) - eine Gruppe fällt nie weg.
     * Gleiche Regeln im Browser (fern_seite.html zBest). */
    b_hinweis hw[16]; int nh = bestaetigen_hinweise(hw, 16);
    int ry = y0 + bleibt_karte11(rx, y0, rw, jb_gew, jb_da, false) + 24;
    int kh = 112, kyb = unten - kh;
    int grenze = unten - 112 - (tippen_aktiv() ? 16 + 96 + 44 : 24), sh = speicher_danach11(rx, ry, rw, weg, false, grenze - ry);
    int ry_hw = ry + (sh ? sh + 24 : 0), r_frei = grenze - ry_hw;   /* Platz rechts zwischen "Speicher danach" und Halte-Karte bzw. Feld */
    int r_inh = nh ? hinweise_zeichnen(0, 0, rw - 64, hw, nh, false) : 0;
    bool hw_rechts = nh && r_frei >= 64 + 90;   /* mindestens drei Zeilen sichtbar */
    int hb = lw - 64, hinh = 0;
    int gy0 = y0 + 32 + 66 + 16, gy1 = unten - 32;
    int gi[8], ng = 0; for (int g = 0; g < 8; g++) if (gn[g]) gi[ng++] = g;
    int zh = 62, zg = 12, fest = 0, mindest = 0;
    for (int i = 0; i < ng; i++) { fest += 16 + 28 + 12 + 16; mindest += 16 + 28 + 12 + 34 + 16; }
    if (ng > 1) mindest += (ng - 1) * 16;
    if (nh && !hw_rechts) {
        hinh = hinweise_zeichnen(0, 0, hb - 12, hw, nh, false);
        if (hinh > 220) hinh = 220;
        int luft = gy1 - gy0 - mindest - 20;          /* was nach den Gruppen (ohne Zeilen) für die Hinweise bleibt */
        if (hinh > luft) hinh = luft < 66 ? 66 : luft;  /* höchstens so hoch, dass alle Gruppen passen; zwei Zeilen bleiben immer */
        gy1 -= hinh + 20;
        hinweise_box(lx + 32 + 4, unten - 32 - hinh, hb - 12, hinh, hw, nh);
    }
    /* Gruppen: Kopf + so viele Zeilen, wie passen (reihum); der Rest als "und N weitere" */
    if (ng == 0) text(lx + 32, gy0 + 40, 0, 24, S_GRAU2, T("Nichts ausgewählt.", "Nothing selected."));
    else if (mindest > gy1 - gy0) {   /* Kurzform: je Gruppe eine Zeile mit Anzahl - alle Gruppen bleiben sichtbar */
        int kz = 56, kg = 10, yy = gy0;
        if (ng * kz + (ng - 1) * kg > gy1 - gy0) { kz = (gy1 - gy0 - (ng - 1) * kg) / ng; if (kz < 40) kz = 40; }
        for (int i = 0; i < ng; i++) {
            int g = gi[i];
            flaeche fl = { false, V1(0xFFFFFF, .045f), 0xFFFFFF, .09f, .09f, 1, 0, 0 }; flaeche_zeichnen(lx + 32, yy, lw - 64, kz, 14, &fl);
            text_gesperrt(lx + 32 + 24, gl11(yy, kz, 24), 1, 24, KO[g] == 7 ? S_GOLD : S_GRAU2, gross11(kern_kat_name(KO[g])), .14f);
            text_rechts(lx + lw - 32 - 24, gl11(yy, kz, 24), 0, 24, S_TEXT_W, VERB(fa, a_fmt(fa, "%d", gn[g]), EINTRAEGE(gn[g])));
            yy += kz + kg;
        }
    }
    else {
        int zeig[8] = { 0 }, platz = gy1 - gy0 - (ng - 1) * 16;
        bool mehr = true;
        while (mehr) {   /* reihum je eine Zeile dazu, solange Platz ist */
            mehr = false;
            for (int i = 0; i < ng; i++) {
                int g = gi[i]; if (zeig[g] >= gn[g]) continue;
                int h = fest; for (int j = 0; j < ng; j++) { int gg = gi[j], z = zeig[gg] + (j == i ? 1 : 0); h += z * (zh + zg) - (z ? zg : 0) + (z < gn[gg] ? 34 : 0); }
                if (h <= platz) { zeig[g]++; mehr = true; }
            }
        }
        int yy = gy0;
        for (int i = 0; i < ng; i++) {
            int g = gi[i], z = zeig[g], gh = 16 + 28 + 12 + z * (zh + zg) - (z ? zg : 0) + (z < gn[g] ? 34 : 0) + 16;
            flaeche fl = { false, V1(0xFFFFFF, .045f), 0xFFFFFF, .09f, .09f, 1, 0, 0 }; flaeche_zeichnen(lx + 32, yy, lw - 64, gh, 16, &fl);
            text_gesperrt(lx + 32 + 24, yy + 16 + 22, 1, 24, KO[g] == 7 ? S_GOLD : S_GRAU2, gross11(kern_kat_name(KO[g])), .14f);
            int ry2 = yy + 16 + 28 + 12;
            for (int j = 0; j < z; j++) {
                eintrag *f = gz[g][j].f;
                const char *nm, *ws; zeile_teilen(f, f->Kat, &nm, &ws);
                const char *gr = gz[g][j].da ? groesse_von(fa, gz[g][j].b, gz[g][j].gek) : "";
                int gw = textbreite(1, 26, gr), tx = lx + 32 + 24 + 48 + 16, tb = lx + lw - 32 - 24 - gw - 16 - tx;
                bild_kachel11(lx + 32 + 24, ry2 + (zh - 48) / 2, 48, 11, f, f->Kat, nm, 1.f);
                text(tx, ry2 + 26, 1, 28, S_TEXT_W, kuerzen(1, 28, nm, tb));
                const char *u = ws[0] ? VERB(fa, ws, " · ", schild_text(f, f->Kat)) : schild_text(f, f->Kat);
                text(tx, ry2 + 56, 0, 24, kern_ampel(f) == AMPEL_ROT ? 0xFFB4AE : S_GRAU, kuerzen(0, 24, u, tb));
                text_rechts(lx + lw - 32 - 24, ry2 + zh / 2 + 10, 1, 26, S_TEXT_W, gr);
                ry2 += zh + zg;
            }
            if (z < gn[g]) text(lx + 32 + 24, ry2 + 22 - (z ? zg : 0), 0, 24, S_GRAU, a_fmt(fa, T("und %d weitere", "and %d more"), gn[g] - z));
            yy += gh + 16;
        }
    }
    /* rechts: Bleibt unangetastet, Speicher danach, (1.3) die Hinweise, unten die Halte-Karte bzw. das Feld */
    bleibt_karte11(rx, y0, rw, jb_gew, jb_da, true);
    if (sh) speicher_danach11(rx, ry, rw, weg, true, grenze - ry);
    if (hw_rechts) {
        int hh = r_inh < r_frei - 64 ? r_inh : r_frei - 64;
        karte11(rx, ry_hw, rw, hh + 64, 20);
        hinweise_box(rx + 32, ry_hw + 32, rw - 64, hh, hw, nh);
    }
    if (tippen_aktiv()) { bestaetigen_tippen(rx, rw, kyb, kh); return; }
    float anteil = x_seit ? (float)(SDL_GetTicks() - x_seit) / (float)HALTEN_MS : 0.f;
    halte_karte_entwurf(rx, kyb, rw, kh, anteil, false, T("2 Sekunden gedrückt halten", "Hold for 2 seconds"), T("zum Löschen – loslassen bricht ab", "to delete – releasing cancels"));
    fussknopf kn[2] = { { KN_X, T("Halten: Löschen", "Hold: Delete"), false }, { KN_O, T("Zurück zur Auswahl", "Back to selection"), false } };
    hinweis_ueber = kyb - 16;
    fuss_neu(kn, 2);
}

/* =================================================================== NEU 1.1 (Entwurf "3b – Alles bereinigen"): eine Seite statt der Fragen
 * nacheinander. Links die Laufwerke (interne SSD immer, M.2 und erweiterter Speicher je mit Schalter), rechts was frei wird, was
 * unangetastet bleibt und der Speicher danach; unten LÖSCHEN eintippen (da_Chris86 15.11.2026 14:52: "USER muss LÖSCHEN EINGEBEN").
 * Der Kern fragt wie bisher nacheinander (ablauf_komplett) - die Antworten gibt die Seite: erst "Weiter", dann je Speicher die Schalter,
 * zuletzt das eingetippte Wort (der Kern prüft es selbst noch einmal). Kommt eine andere Frage dazwischen, wird sie normal gezeigt. */
static bool komplett_frage(void)   /* die erste Frage der vollständigen Bereinigung */
{
    if (G.art != D_WAHL || G.bestaetigen) return false;
    pthread_mutex_lock(&G.m); bool ja = G.titel && !strcmp(G.titel, T("Vollständige Bereinigung", "Full cleanup")); pthread_mutex_unlock(&G.m);
    return ja;
}
static struct { int nr, fokus; bool leeren[3], da[3]; bool auto_an, auto_leeren[3], auto_voll; long long auto_gezeigt; char wort[32]; int auto_nr; } KB = { -1 };
static void komplett_vorbereiten(void)
{
    if (KB.nr == G.dlg_nr) return;
    KB.nr = G.dlg_nr; KB.fokus = 0; memset(KB.leeren, 0, sizeof KB.leeren);
    pthread_mutex_lock(&G.m); const char *t = a_kopie(fa, G.text ? G.text : ""); G.eingabe[0] = 0; pthread_mutex_unlock(&G.m);
    KB.da[ORT_M2] = strstr(t, T("Auf der M.2-SSD - gleich kommt die Frage", "On the M.2 SSD - you will be asked next")) != NULL;
    KB.da[ORT_USB] = strstr(t, T("Auf dem erweiterten Speicher (USB-Festplatte für Spiele) - gleich kommt", "On the extended storage (USB drive for games) - you will be asked")) != NULL;
}
/* Was die vollständige Bereinigung auf diesem Speicher nimmt (Größe, Einträge, davon Spiele).
 * 1.1 BLOCKER-FIX (PS5-Test da_Chris86 16.11.2026 20:49: angezeigt "etwa 40,1 GB · 118 Einträge", M.2 "5 Einträge · 27,1 GB" - gelöscht
 * wurden 140 Einträge mit 1,62 TB, darunter 13 ShadowMount-Spiele auf der M.2). Seit Codex-Prüfung 345 rechnet der Kern selbst
 * (kern_komplett_vorschau): dieselbe Auswahl und dieselben Pfade wie beim Löschen, je Speicher. Die Zeile eines Speichers zeigt, was er mit
 * Schalter AN hergibt (m2/usb an); oben rechts, "Speicher danach" und die Warnung rechnen mit der aktuellen Wahl. Zwischengespeichert
 * (höchstens zweimal je Sekunde neu - die Pfad-Tabelle ist groß). kv_voll = alle Größen gezählt (sonst "mindestens"). */
static kern_kv kv_alle[4], kv_wahl[4]; static bool kv_voll = true; static Uint32 kv_zeit; static bool kv_m2 = false, kv_usb = false, kv_da = false;
static void komplett_vorschau_holen(void)
{
    bool m2 = KB.leeren[ORT_M2], usb = KB.leeren[ORT_USB];
    if (kv_da && m2 == kv_m2 && usb == kv_usb && SDL_GetTicks() - kv_zeit < 500) return;
    bool v1 = kern_komplett_vorschau(K, true, true, kv_alle);
    bool v2 = kern_komplett_vorschau(K, m2, usb, kv_wahl);
    kv_voll = v1 && v2; kv_m2 = m2; kv_usb = usb; kv_da = true; kv_zeit = SDL_GetTicks();
}
static long long komplett_bytes(int ort, int *anz, int *spiele)   /* eine Speicher-Zeile: was er mit Schalter AN hergibt */
{
    komplett_vorschau_holen();
    if (anz) *anz = kv_alle[ort].eintraege;
    if (spiele) *spiele = kv_alle[ort].spiele;
    return kv_alle[ort].bytes;
}
static long long komplett_bytes_wahl(int ort, int *anz, int *spiele)   /* mit der aktuellen Wahl */
{
    komplett_vorschau_holen();
    if (anz) *anz = kv_wahl[ort].eintraege;
    if (spiele) *spiele = kv_wahl[ort].spiele;
    return kv_wahl[ort].bytes;
}
/* Zeile unter dem Speicher: "13 Spiele · 18 Einträge · 1,5 TB" (NEU 1.1: die Spiele zuerst - darum geht es) */
static const char *komplett_unter(int ort, int an_n, int sp, long long b)
{
    const char *g = kern_groesse_text(fa, b);
    if (ort == ORT_INTERN) return sp > 0 ? VERB(fa, T("immer dabei · ", "always included · "), a_fmt(fa, "%d", sp), sp == 1 ? T(" Spiel", " game") : T(" Spiele", " games"), " · ", g)
                                         : VERB(fa, T("immer dabei · ", "always included · "), g);
    return sp > 0 ? VERB(fa, a_fmt(fa, "%d", sp), sp == 1 ? T(" Spiel", " game") : T(" Spiele", " games"), ", ", a_fmt(fa, "%d", an_n), EINTRAEGE(an_n), " · ", g)
                  : VERB(fa, a_fmt(fa, "%d", an_n), EINTRAEGE(an_n), " · ", g);
}
/* Warnung unten links: wird ein weiterer Speicher mit Spielen geleert, steht es ausdrücklich da (NEU 1.1, siehe komplett_bytes) */
static const char *komplett_warnung(void)
{
    const char *w = "", *wo = ""; int sp = 0; long long b = 0;
    for (int o = ORT_M2; o <= ORT_USB; o++) {
        if (!KB.da[o] || !KB.leeren[o]) continue;
        int s = 0; long long x = komplett_bytes_wahl(o, NULL, &s);
        if (s <= 0) continue;
        sp += s; b += x; wo = wo[0] ? VERB(fa, wo, T(" und ", " and "), ort_name11(o, 0)) : ort_name11(o, 0);
    }
    if (sp > 0) w = VERB(fa, T("Auch die Spiele auf ", "The games on "), wo, T(" werden gelöscht: ", " are deleted too: "), a_fmt(fa, "%d", sp), sp == 1 ? T(" Spiel", " game") : T(" Spiele", " games"),
                          " · ", kv_voll ? "" : T("mindestens ", "at least "), kern_groesse_text(fa, b), ". ");
    return VERB(fa, w, T("Das lässt sich nicht rückgängig machen. Danach startet die PS5 neu.", "This cannot be undone. The PS5 restarts afterwards."));
}
static void schalter11(int x, int cy, bool an, bool fokus)
{
    int w = 72, h = 40, y = cy - h / 2;
    if (an) { leuchten(x, y, w, h, 20, 12, S_TUERKIS, .35f); rund(x, y, w, h, 20, S_TUERKIS); kreis(x + w - 20, cy, 15, 0xFFFFFF); }
    else { flaeche f = { false, V1(0xFFFFFF, .10f), fokus ? S_BLAU_H : 0xFFFFFF, fokus ? 1 : .12f, fokus ? 1 : .12f, 1, 0, 0 }; flaeche_zeichnen(x, y, w, h, 20, &f); kreis(x + 20, cy, 15, 0xE8EDF5); }
}
static void schirm_komplett11(void)
{
    komplett_vorbereiten();
    hintergrund11();
    seite11_titel = T("Alles bereinigen", "Clean up everything");
    kopf_neu(3, NULL);
    bilder_neu = 0;
    speicher11 sp = speicher11_holen();
    int y0 = R11_O + 50 + 20, unten = FUSS11_Y - 20, lw = (BREITE - 2 * R11_X - 24) / 2, lx = R11_X, rx = lx + lw + 24, rw = BREITE - R11_X - rx;
    /* links: Wo soll bereinigt werden? */
    karte11(lx, y0, lw, unten - y0, 20);
    text_passend(lx + 32, y0 + 32 + 40, 3, 44, 30, S_TEXT_W, T("Wo soll bereinigt werden?", "Where should it clean up?"), lw - 64);
    {
        zeilen z = umbrechen(0, 26, T("Alles Löschbare auf den gewählten Laufwerken – auch Spiele, Spielstände, Trophäen, Bilder und Videos.",
                                      "Everything deletable on the chosen drives – including games, saves, trophies, pictures and videos."), lw - 64);
        for (int i = 0; i < z.n && i < 3; i++) text(lx + 32, y0 + 32 + 40 + 44 + i * 38, 0, 26, S_TEXT2, z.z[i]);
    }
    int orte[3] = { ORT_INTERN, ORT_M2, ORT_USB }, n = 0, ry = y0 + 32 + 40 + 44 + 2 * 38 + 24, rh = 110;
    long long frei_ges = 0; int anz_ges = 0; const char *wo = T("Interne SSD", "Internal SSD");
    for (int i = 0; i < 3; i++) {
        int o = orte[i]; if (o != ORT_INTERN && !KB.da[o]) continue;
        /* 1.3 FIX (PS5-Test da_Chris86 20.11.2026: "Interne SSD · 11 Spiele · 19,4 GB", frei aber "etwa 9,0 GB" bei M.2 aus): die interne SSD ist
         * immer dabei - ihre Zeile rechnet mit der aktuellen Wahl wie "Das wird frei" (vorher wie mit allen Schaltern AN, also mit den
         * internen Teilen der M.2-Spiele). M.2/erweiterter Speicher zeigen weiter, was sie mit Schalter AN hergeben. */
        int an_n = 0, sp_n = 0; long long b = o == ORT_INTERN ? komplett_bytes_wahl(o, &an_n, &sp_n) : komplett_bytes(o, &an_n, &sp_n);
        bool an = o == ORT_INTERN || KB.leeren[o], fok = KB.fokus == n;
        if (an) { int wn = 0; frei_ges += komplett_bytes_wahl(o, &wn, NULL); anz_ges += wn; if (o != ORT_INTERN) wo = VERB(fa, wo, T(" und ", " and "), ort_name11(o, sp.usb_n)); }
        if (fok && o != ORT_INTERN) fokus11(lx + 32, ry, lw - 64, rh, 16, 2);
        else { flaeche f = F_FELD11(); flaeche_zeichnen(lx + 32, ry, lw - 64, rh, 16, &f); }
        text(lx + 32 + 24, ry + 46, 1, 30, S_TEXT_W, ort_name11(o, sp.usb_n));
        const char *u = komplett_unter(o, an_n, sp_n, b);
        text_passend(lx + 32 + 24, ry + 84, 0, 26, 20, S_GRAU, u, lw - 64 - 48 - 100);
        if (o == ORT_INTERN) symbol(SYM_SCHLOSS, lx + lw - 32 - 24 - 28, ry + rh / 2 - 14, 28, S_GRAU);
        else schalter11(lx + lw - 32 - 24 - 72, ry + rh / 2, KB.leeren[o], fok);
        ry += rh + 16; n++;
    }
    {   /* Warnung unten links */
        const char *wt = komplett_warnung();
        passend p = einpassen(0, 26, 26, 20, wt, lw - 64 - 32 - 60, 3); p.lh = p.gr + 10;
        int wh = 32 + p.z.n * p.lh, wy = unten - 32 - wh;
        gold11(lx + 32, wy, lw - 64, wh, 14, 1);
        symbol(SYM_WARNUNG, lx + 32 + 20, wy + wh / 2 - 15, 30, S_GOLD);
        for (int i = 0; i < p.z.n; i++) text(lx + 32 + 20 + 30 + 16, wy + 16 + p.gr - 2 + i * p.lh, 0, p.gr, S_GOLD_T, p.z.z[i]);
    }
    /* rechts: Das wird frei */
    int y = y0;
    {
        int h = 200; karte11(rx, y, rw, h, 20);
        text_gesperrt(rx + 32, y + 32 + 22, 1, 24, S_GRAU2, T("DAS WIRD FREI", "THIS WILL BE FREED"), .14f);
        text_passend(rx + 32, y + 32 + 22 + 80, 3, 72, 48, S_TEXT_W, VERB(fa, kv_voll ? T("etwa ", "about ") : T("mindestens ", "at least "), kern_groesse_text(fa, frei_ges)), rw - 64);   /* 1.1: noch nicht alles gezählt */
        text_passend(rx + 32, y + h - 28, 0, 26, 22, S_TEXT2, VERB(fa, wo, " · ", a_fmt(fa, "%d", anz_ges), EINTRAEGE(anz_ges)), rw - 64);
        y += h + 24;
    }
    {   /* Bleibt unangetastet (als Satz) */
        const char *jn = kopf_jb_name();
        const char *bt = VERB(fa, T("Laufwerkskopplung, Lizenzen und Konten, Systemsoftware", "Disc drive pairing, licenses and accounts, system software"),
                              jn ? VERB(fa, T(", Jailbreak ", ", jailbreak "), jn) : "", T(", der Bereich Jailbreak und alles auf dem USB-Laufwerk.", ", the jailbreak area and everything on the USB drive."));
        zeilen z = umbrechen(0, 26, bt, rw - 56); if (z.n > 3) z.n = 3;
        int h = 24 + 44 + 12 + z.n * 38 + 20;
        gruen11(rx, y, rw, h, 20);
        symbol(SYM_SCHILD_HAKEN, rx + 28, y + 24, 44, S_GRUEN);
        text(rx + 28 + 44 + 16, gl11(y + 24, 44, 32), 3, 32, S_TEXT_W, T("Bleibt unangetastet", "Stays untouched"));
        for (int i = 0; i < z.n; i++) text(rx + 28, y + 24 + 44 + 12 + 27 + i * 38, 0, 26, S_TEXT_W, z.z[i]);
        y += h + 24;
    }
    {   /* Speicher danach */
        long long weg[4] = { 0, 0, 0, 0 };
        for (int o = ORT_INTERN; o <= ORT_USB; o++) if (o == ORT_INTERN || KB.da[o]) weg[o] = komplett_bytes_wahl(o, NULL, NULL);   /* 1.1: mit der Wahl (auch App-Daten intern) */
        int mh = unten - 112 - 24 - y, sh = speicher_danach11(rx, y, rw, weg, false, mh);
        if (sh) speicher_danach11(rx, y, rw, weg, true, mh);
    }
    /* unten rechts: LÖSCHEN eintippen (gold) */
    {
        bool passt = eingabe_passt(), getippt = G.eingabe[0] != 0;
        int kh = 112, ky = unten - kh;
        const char *t1 = passt ? T("Bereit: △ drücken zum Bereinigen", "Ready: press △ to clean up") : getippt ? VERB(fa, T("Das ist nicht ", "That is not "), T("LÖSCHEN", "DELETE")) : T("LÖSCHEN eintippen", "Type DELETE");
        const char *t2 = passt ? T("eingetippt: ", "typed: ") : T("△ öffnet die Tastatur – erst dann wird bereinigt", "△ opens the keyboard – only then it cleans up");
        if (passt) t2 = VERB(fa, t2, G.eingabe);
        if (tastatur_offen) t2 = T("Tastatur ist offen …", "Keyboard is open …");
        halte_karte_entwurf(rx, ky, rw, kh, passt ? 1.f : 0.f, true, t1, t2);
    }
    fussknopf kn[3] = { { KN_X, T("Laufwerk an/aus", "Drive on/off"), false }, { KN_DREIECK, eingabe_passt() ? T("Bereinigen", "Clean up") : T("LÖSCHEN eintippen", "Type DELETE"), false },
                        { KN_O, T("Zurück", "Back"), false } };
    hinweis_ueber = unten - 112 - 16;
    fuss_neu(kn, 3);
}
/* aus der Hauptschleife, wenn eine neue Frage da ist: die Antworten der Seite "Alles bereinigen" geben */
/* Codex-Prüfung 347 (P2): endet die Automatik, muss die Frage sofort gezeichnet werden - sonst bliebe die Arbeitsseite stehen, während der
 * Kern auf eine unsichtbare Bestätigung wartet (die regelmäßige Aktualisierung greift bei einer Frage nicht) */
static void komplett_auto_aus(void) { KB.auto_an = false; neu_zeichnen(); }
static void komplett_auto(void)
{
    if (!KB.auto_an || G.beantwortet || G.art == D_KEINER) return;
    /* Codex-Prüfung 339 (P1): automatisch beantwortet wird nur die Frage, die DIREKT auf die zuletzt beantwortete folgt (Nummer + 1) - kommt
     * irgendetwas dazwischen, ist die Kette zu Ende und alles Weitere wird normal gezeigt. Die Zustimmung ist das eingetippte LÖSCHEN auf der
     * Seite "Alles bereinigen" (da_Chris86 15.11.2026: "USER muss LÖSCHEN EINGEBEN"); der Kern prüft das Wort selbst noch einmal. */
    pthread_mutex_lock(&G.m); int dnr = G.dlg_nr; pthread_mutex_unlock(&G.m);
    if (dnr == KB.auto_nr) return;
    if (dnr != KB.auto_nr + 1) { komplett_auto_aus(); return; }
    KB.auto_nr = dnr;
    if (G.art == D_WAHL && !G.bestaetigen) {
        pthread_mutex_lock(&G.m); const char *ti = a_kopie(fa, G.titel ? G.titel : ""); pthread_mutex_unlock(&G.m);
        if (!strcmp(ti, T("M.2-SSD", "M.2 SSD"))) { antworten(KB.auto_leeren[ORT_M2], false); return; }
        if (!strcmp(ti, T("Erweiterter Speicher (USB-Festplatte)", "Extended storage (USB drive)"))) { antworten(KB.auto_leeren[ORT_USB], false); return; }
        komplett_auto_aus();   /* eine andere Frage: normal zeigen */
        return;
    }
    if (G.art == D_TEXT && G.bestaetigen) {
        /* 1.1 (Codex-Prüfung 345 P1): waren beim Eintippen noch nicht alle Größen gezählt, stand auf der Seite nur "mindestens ..." - dann die
         * Abschlussfrage des Kerns (mit der gezählten Größe) zeigen statt sie selbst zu beantworten */
        if (!KB.auto_voll) { komplett_auto_aus(); return; }
        /* Codex-Prüfung 346 (P1): Gegenprobe mit dem, was der Kern jetzt wirklich gewählt und gezählt hat (z. B. App-Daten, die nach dem
         * Nachschauen entstanden sind) - ist es mehr als auf der Seite stand (Rundung: 1 % + 16 MB), wird die Frage normal gezeigt */
        { long long ist = 0; bool voll = kern_gewaehlt_bytes(K, &ist);
          if (!voll || ist > KB.auto_gezeigt + KB.auto_gezeigt / 100 + 16LL * 1024 * 1024) {
              ui_log(NULL, T("Alles bereinigen: es wird mehr gelöscht als angezeigt - bitte die Frage lesen.", "Clean up everything: more will be deleted than shown - please read the question."));
              komplett_auto_aus(); return; } }
        pthread_mutex_lock(&G.m); snprintf(G.eingabe, sizeof G.eingabe, "%s", KB.wort); pthread_mutex_unlock(&G.m);
        komplett_auto_aus();
        antworten(false, true);
        return;
    }
    if (G.art != D_KEINER) komplett_auto_aus();
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
    hintergrund11();
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
    char hg[400];                                /* NEU 1.3: nachgeladene Programme, die liefen ("nanodns,pldmgr") - nur Namen */
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
/* NEU 1.3 (21.11.2026): die laufenden nachgeladenen Programme - einmal beim Öffnen der Rückmeldung gelesen (nicht bei jedem Bild) */
static char rm_hg_puf[400];
static void rm_hg_lesen(void)
{
    char t[400] = ""; if (plattform_hintergrund(t, sizeof t) < 0) t[0] = 0;
    pthread_mutex_lock(&rm_m); snprintf(rm_hg_puf, sizeof rm_hg_puf, "%s", t); pthread_mutex_unlock(&rm_m);
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
    char hg[400]; memcpy(hg, rm_hg_puf, sizeof hg); hg[sizeof hg - 1] = 0;
    pthread_mutex_unlock(&rm_m);
    rm_kopie(r.hg, sizeof r.hg, hg, ",._-");   /* NEU 1.3: nur Namen (Kleinbuchstaben, Ziffern, . _ -) */
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
/* Nachtprüfung 16.11.2026 (Codex-Prüfung 341, P2): wie viele der gewählten Einträge wirklich gelöscht sind - Fehler, teilweise entfernte,
 * vom PS4-Schutz zurückgehaltene und unveränderte zählen nicht; wurde angehalten oder abgebrochen, zählen nur die Zeilen, die als
 * gelöscht abgeschlossen sind (nicht bearbeitete kommen in keiner Zahl des Kerns vor) */
static int rm_geloescht(const rueckmeldung *r)
{
    pthread_mutex_lock(&rm_m); int un = rm_stand.unveraendert; bool ab = rm_stand.ab; pthread_mutex_unlock(&rm_m);
    int d = r->g - r->f - r->p - r->ps - un;
    if (ab) {
        int z = 0; pthread_mutex_lock(&erg_m); for (int i = 0; i < erg_n; i++) if (erg[i].zustand == 1) z++; pthread_mutex_unlock(&erg_m);
        if (z < d) d = z;
    }
    return d < 0 ? 0 : d;
}
static const char *rm_satz_selbst(const rueckmeldung *r)
{
    int d = rm_geloescht(r);
    /* 1.3 (da_Chris86 21.11.2026: 47 angehakt, hier stand "48 selbst ausgewählt" - der 48. gehörte zu einem gewählten Spiel) */
    if (d >= r->g) return a_fmt(fa, T("Du hast selbst ausgewählt – %d Einträge sind gelöscht.", "You chose yourself – %d entries are deleted."), r->g);
    return a_fmt(fa, T("Du hast selbst ausgewählt – %d von %d Einträgen sind gelöscht.", "You chose yourself – %d of %d entries are deleted."), d, r->g);
}
/* NEU 1.0.10: max_e / max_uz begrenzen die Fehler-Kürzel und Bereiche der nicht lesbaren Ordner (für den QR-Code; -1 = alle); extra = auch
 * Programme der Fehler (hb) und erfolgreiche Wege (ek) */
/* Codex-Prüfung 524 (Befund 5): mit_hg - die Namen der Hintergrundprogramme nie im QR-Code (dort steht "Keine Namen von Spielen oder Apps") */
static const char *rueckmeldung_link_x(arena *a, const rueckmeldung *r, int max_e, int max_uz, bool extra, bool mit_hg)
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
    return a_fmt(a, "%s?v=%s&b=%c&bd=%s&s=%d&m=%s&fw=%s&l=%s&dd=%d&jb=%s&sp=%d%d%d&sm=%d&a=%d.%d.%d.%d.%d.%d.%d&n=%d&g=%d&f=%d&p=%d&ps=%d&u=%d&sb=%d&mb=%d&t=%d&ab=%d&ok=%d%s%s%s%s%s%s%s%s%s%s%s%s",
                 RUECKMELDUNG_ADRESSE, RUECKMELDUNG_VERSION, RUECKMELDUNG_FASSUNG, r->bau, r->s, r->modell, r->fw, SP_EN ? "en" : "de", r->dd, r->jb,
                 r->m2, r->ext, r->usb, r->sm, r->bereich[0], r->bereich[1], r->bereich[2], r->bereich[3], r->bereich[4], r->bereich[5], r->bereich[6],
                 r->n, r->g, r->f, r->p, r->ps, r->u, r->sb, r->mb, r->t, r->ab, r->ok ? 1 : 0, r->db[0] ? "&db=" : "", r->db, e[0] ? "&e=" : "", e, uz[0] ? "&uz=" : "", uz,
                 extra && r->hb[0] ? "&hb=" : "", extra ? r->hb : "", extra && r->ek[0] ? "&ek=" : "", extra ? r->ek : "",
                 mit_hg && r->hg[0] ? "&hg=" : "", mit_hg ? r->hg : "");   /* NEU 1.3: hg - Programme im Hintergrund */
}
static const char *rueckmeldung_link_n(arena *a, const rueckmeldung *r, int max_e, int max_uz) { return rueckmeldung_link_x(a, r, max_e, max_uz, true, false); }   /* Links im Browser stehen neben "Keine Namen" - ohne hg */
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
        int n = qr_kodieren(rueckmeldung_link_x(fa, r, STUFEN[i][0], STUFEN[i][1], STUFEN[i][2] != 0, false));
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
typedef struct { const eintrag *f; char name[200]; long long b; bool hat_b; int zustand; bool haupt; } lz_zeile;   /* haupt: NEU 1.1 - Kachel/Spiel, mit Bild */   /* zustand: 0 läuft, 1 erledigt, 2 Fehler, 3 unverändert */
#define LZ_MAX 4   /* höchstens drei erledigte und der laufende */
static lz_zeile lz[LZ_MAX]; static int lz_n = 0, lz_nr = 0, lz_fehler = 0, lz_unv = 0;
/* NEU 1.1 (Entwurf "5 – Fertig"): das Ergebnis je Eintrag (gleiche Zuordnung wie oben, nur ohne Grenze von vier Zeilen) */

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
static void loeschseite_beginnen(void) { lz_n = 0; lz_nr = 0; lz_fehler = 0; lz_unv = 0; memset(&lf_jetzt, 0, sizeof lf_jetzt); anhalten_gewuenscht = false; loeschseite = true;
    pthread_mutex_lock(&erg_m); if (erg_nr == 0) erg_n = 0; pthread_mutex_unlock(&erg_m); }   /* Codex-Prüfung 339: keine Zeilen eines früheren Laufs; Codex-Prüfung 340: hat der Arbeitsfaden den laufenden schon erfasst (erg_nr > 0), bleibt er */
static void lz_nachfuehren(void)
{
    lf_lesen();
    int nr = lf_jetzt.nr, fe = lf_jetzt.fehler, un = lf_jetzt.unv;
    const eintrag *f = lf_jetzt.f;
    if (nr != lz_nr && nr > 0 && f) {
        if (lz_n > 0 && lz[lz_n - 1].zustand == 0) {   /* der vorige ist fertig - genau zuordnen lässt es sich nur, wenn keiner dazwischen lag */
            lz[lz_n - 1].zustand = nr == lz_nr + 1 && fe > lz_fehler ? 2 : nr == lz_nr + 1 && un > lz_unv ? 3 : 1;
        }
        lz_fehler = fe; lz_unv = un;
        if (lz_n == LZ_MAX) { memmove(lz, lz + 1, sizeof lz[0] * (LZ_MAX - 1)); lz_n--; }
        lz_zeile *z = &lz[lz_n++];
        z->f = f; utf8_kopie(z->name, sizeof z->name, ohne_ort_vorne(kern_zeile(fa, f))); z->hat_b = f->HatGroesse; z->b = f->Bytes; z->zustand = 0;
        z->haupt = f->Kachel || ist_teil(f, "Spiel");   /* NEU 1.1 (da_Chris86 16.11.2026: "beim Hauptprogramm die Kachel anzeigen" - wie bis 1.0.10) */
        /* 1.3 (PS5-Test da_Chris86 20.11.2026 21:56: bei "Alles bereinigen" standen Spieldateien ohne Kachel ohne Bild): jeder Eintrag mit Bild
         * zeigt es - wie die Ergebnisseite; ohne Bild bleibt der Kreis */
        if (!z->haupt) { const char *bk = bild_kennung(f); z->haupt = bk && bk[0] && !bild_fehlt(bk); }
        lz_nr = nr;
    }
    if (!A.anhaltbar && lz_n > 0 && lz[lz_n - 1].zustand == 0 && lf_jetzt.n > 0 && nr >= lf_jetzt.n)   /* der letzte ist durch - der Kern zählt nicht mehr */
    {
        lz[lz_n - 1].zustand = K->FEHLER_ZAHL > lz_fehler ? 2 : K->UNVERAENDERT.n > lz_unv ? 3 : 1;
    }
}
static const char *protokoll_wo(void)   /* "auf dem USB-Laufwerk" usw. (aus A.protokoll_datei); "" = nur in der Ansicht */
{
    const char *p = A.protokoll_datei ? A.protokoll_datei : "";
    if (txt_beginnt(p, T("auf dem USB-Laufwerk", "on the USB drive"))) return T("auf dem USB-Laufwerk", "on the USB drive");
    if (txt_beginnt(p, T("auf der M.2", "on the M.2"))) return T("auf der M.2", "on the M.2");
    if (txt_beginnt(p, T("auf der PS5", "on the PS5"))) return T("auf der PS5", "on the PS5");
    return "";
}
static const char *kuerzen_still(int f, int gr, const char *t, int breite)   /* wie kuerzen, aber gewollt (ohne "ZU LANG") */
{
    if (textbreite(f, gr, t) <= breite) return t;
    int el = textbreite(f, gr, "…");
    const char *q = t; int b = 0;
    while (*q) { const char *v = q; Glyph *g = glyph(f, gr, cp_naechstes(&q)); b += g ? g->vorschub : 0; if (b + el > breite) { q = v; break; } }
    return VERB(fa, a_kopie_n(fa, t, (size_t)(q - t)), "…");
}
/* NEU 1.1 (Entwurf, "Wusstest du?"): wechselnde Hinweise beim Warten - nur aus Angaben, die PS5 Cleanup wirklich hat */
static void wusstest_wahl(int art, const char **w, const char **s)   /* NEU 1.1: auch für die Browserseite (fern_tv.h) */
{
    const char *wert[6], *satz[6]; int n = 0;
    struct timespec ts; if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0 && ts.tv_sec > 120) {
        long m = (long)(ts.tv_sec / 60);
        wert[n] = m >= 60 ? a_fmt(fa, T("%ld Std.", "%ld h"), m / 60) : a_fmt(fa, T("%ld Min.", "%ld min"), m);
        satz[n++] = T("läuft deine PS5 schon seit dem letzten Einschalten.", "your PS5 has been running since it was last switched on.");
    }
    if (art == 1 && lf_jetzt.frei > 0) { wert[n] = groesse_von(fa, lf_jetzt.frei, false); satz[n++] = K->simulation ? T("würden bei diesem Durchgang schon frei.", "would already be freed in this run.") : T("hat PS5 Cleanup bei diesem Durchgang schon freigeräumt.", "PS5 Cleanup has already freed in this run."); }
    if (GESUCHT && K->FUNDE_GUELTIG) {
        const eintrag *gross = NULL; int spiele = 0; long long sum = 0;
        for (int i = 0; i < K->Funde.n; i++) { const eintrag *f = K->Funde.e[i]; if (f->Kat != 1 || f->Teil == NULL || strcmp(f->Teil, "Spiel")) continue; spiele++; if (f->HatGroesse) { sum += f->Bytes; if (!gross || f->Bytes > gross->Bytes) gross = f; } }
        if (gross && gross->Bytes > 0) { wert[n] = groesse_von(fa, gross->Bytes, false); satz[n++] = VERB(fa, T("belegt ", "is used by "), al_name(gross), T(", dein größtes Spiel.", ", your biggest game.")); }
        if (spiele > 1) { wert[n] = a_fmt(fa, "%d", spiele); satz[n++] = VERB(fa, T("Spiele liegen auf deiner PS5", "games are on your PS5"), sum > 0 ? VERB(fa, T(", zusammen ", ", together "), groesse_von(fa, sum, false)) : "", "."); }
    }
    if (!n) { wert[n] = T("Tipp:", "Tip:"); satz[n++] = T("Geschützte Daten fasst PS5 Cleanup nie an.", "PS5 Cleanup never touches protected data."); }
    int i = (int)(SDL_GetTicks() / 4500) % n;
    *w = wert[i]; *s = satz[i];
}
static void wusstest11(int x, int y, int w, int art)   /* art 0 Nachschauen, 1 Löschen */
{
    const char *wert1, *satz1; wusstest_wahl(art, &wert1, &satz1);
    const char *wert[1] = { wert1 }, *satz[1] = { satz1 }; int i = 0;
    flaeche f = { false, V1(0xFFFFFF, .045f), 0xFFFFFF, .09f, .09f, 1, 0, 0 }; flaeche_zeichnen(x, y, w, 60, 14, &f);
    symbol(SYM_INFO, x + 18, y + 16, 28, S_GRAU2);
    int tx = x + 18 + 28 + 14;
    text(tx, y + 38, 1, 22, S_TEXT_W, wert[i]); tx += textbreite(1, 22, wert[i]) + 8;
    text(tx, y + 38, 0, 22, S_TEXT2, kuerzen_still(0, 22, satz[i], x + w - 18 - tx));
    lauf_aktiv = true;   /* die Seite zeichnet regelmäßig neu (Wechsel) */
}
static void schirm_loeschen(void)
{
    /* NEU 1.1 (Entwurf "4 – Löschen läuft"): links der Ring mit Prozent und "Eintrag x von y", darunter was gerade gelöscht wird; rechts der
     * Ablauf (die letzten erledigten, der laufende, die wartenden), ein wechselnder Hinweis und "Bitte die PS5 jetzt nicht ausschalten." */
    hintergrund11();
    seite11_titel = K->simulation ? T("Simulation läuft", "Simulation running") : T("Wird gelöscht", "Deleting");
    kopf_neu(3, NULL);
    bilder_neu = 0;
    lz_nachfuehren();
    pthread_mutex_lock(&G.m); double p = G.prozent; bool unb = G.unbestimmt; pthread_mutex_unlock(&G.m);
    int nr = lf_jetzt.nr, n = lf_jetzt.n;
    bool sim = K->simulation;
    int y0 = R11_O + 50 + 20, unten = FUSS11_Y - 20, lw = 720, lx = R11_X, rx = lx + lw + 24, rw = BREITE - R11_X - rx;
    /* links: Ring */
    karte11(lx, y0, lw, unten - y0, 20);
    {
        /* Prozent über alle Schritte: die fertigen ganz, der laufende anteilig (G.prozent gilt nur für den laufenden Schritt) */
        /* NEU 1.1 (da_Chris86 16.11.2026: "1 von 4", aber schon 90 % - "Das soll schon der Echtzeit entsprechen"): G.prozent war der Anteil
         * der Größe über ALLE Einträge und wurde hier noch einmal zu den fertigen Einträgen gezählt. Jetzt zählen nur die fertigen Einträge -
         * passend zu "Eintrag x von y"; am Ende 100. */
        double ges = (!unb && p >= 100) ? 100 : n > 0 && nr > 0 ? (nr - 1) * 100.0 / n : unb ? 0 : p;
        int cx = lx + lw / 2, cy = y0 + (unten - y0) / 2 - 80, pz = (int)(ges + .5); if (pz < 0) pz = 0; if (pz > 100) pz = 100;
        leuchten(cx - 190, cy - 190, 380, 380, 190, 60, S_TUERKIS, .25f);
        kreis(cx, cy, 190, 0x2A3644);
        if (nr == 0) { float a = (float)(SDL_GetTicks() % 1600) / 1600.f; ring_teil(cx, cy, 177.5f, 25, 1.f, 0x2A3644); (void)a; }
        else ring_teil(cx, cy, 177.5f, 25, pz / 100.f, S_TUERKIS);
        kreis(cx, cy, 165, 0x161E28);
        text_mitte(cx, cy + 30, 3, 96, S_TEXT_W, nr == 0 ? "…" : a_fmt(fa, "%d %%", pz));
        text_mitte(cx, cy + 78, 0, 28, S_GRAU, nr > 0 ? a_fmt(fa, T("%d von %d Schritten", "%d of %d steps"), nr, n) : T("wird vorbereitet …", "preparing …"));
        const char *jetzt = lz_n > 0 && lz[lz_n - 1].zustand == 0 ? lz[lz_n - 1].name : "";
        if (jetzt[0]) {
            text_mitte(cx, cy + 190 + 32 + 40, 3, 34, S_TEXT_W, kuerzen_still(3, 34, jetzt, lw - 80));
            text_mitte(cx, cy + 190 + 32 + 40 + 40, 0, 26, S_GRAU, sim ? T("wird geprüft …", "being checked …") : T("wird gelöscht …", "being deleted …"));
        }
        lauf_aktiv = true;
    }
    /* rechts: Ablauf */
    karte11(rx, y0, rw, unten - y0, 20);
    text_gesperrt(rx + 32, y0 + 32 + 22, 1, 24, S_GRAU2, T("ABLAUF", "PROGRESS"), .14f);
    int rest = n - nr; if (rest < 0) rest = 0;
    int zeilen = lz_n + (rest > 0 ? 1 : 0), zy = y0 + 32 + 22 + 24, zh = 90;
    for (int i = 0; i < zeilen; i++) {
        bool wartet = i >= lz_n, laeuft = !wartet && lz[i].zustand == 0;
        int x = rx + 32, w = rw - 64, cy = zy + zh / 2;
        if (laeuft) fokus11(x, zy, w, zh, 16, 2);
        else { flaeche f = { false, V1(0xFFFFFF, .045f), 0, 0, 0, 0, 0, 0 }; flaeche_zeichnen(x, zy, w, zh, 16, &f); }
        int sx = x + 20 + 19;
        bool bild = !wartet && lz[i].haupt && lz[i].f;
        if (bild) {   /* NEU 1.1 (da_Chris86 16.11.2026): Hauptteil (Kachel, Spiel) mit seinem Bild, der Stand als kleines Zeichen in der Ecke */
            int bs = 60, bx = x + 20, by = cy - bs / 2;
            bild_kachel11(bx, by, bs, 14, lz[i].f, lz[i].f->Kat, lz[i].name, 1.f);
            int ex = bx + bs - 4, ey = by + bs - 4;
            kreis(ex, ey, 15, 0x0E1622);
            if (laeuft) { ring(ex, ey, 11, 3, 0x13233A); ring_teil(ex, ey, 11, 3, .75f, S_TUERKIS); }
            else { int zu = lz[i].zustand; kreis(ex, ey, 12, zu == 1 ? S_GRUEN : zu == 2 ? N_ROT : S_GRAU2); symbol(zu == 1 ? SYM_HAKEN : zu == 2 ? SYM_WARNUNG : SYM_INFO, ex - 8, ey - 8, 16, 0x06261C); }
            sx = bx + bs - 19;   /* Text rechts neben dem Bild */
        }
        else if (wartet) ring(sx, cy, 17.5f, 3, 0x3B4A63);
        else if (laeuft) { ring(sx, cy, 17, 4, 0x13233A); ring_teil(sx, cy, 17, 4, .75f, S_TUERKIS); }
        else {
            int zu = lz[i].zustand; uint32_t f = zu == 1 ? S_GRUEN : zu == 2 ? N_ROT : S_GRAU2;
            kreis(sx, cy, 19, f); symbol(zu == 1 ? SYM_HAKEN : zu == 2 ? SYM_WARNUNG : SYM_INFO, sx - 13, cy - 13, 26, 0x06261C);
        }
        const char *zt = wartet ? T("wartet", "waiting") : laeuft ? T("läuft …", "running …") : lz[i].zustand == 1 ? (sim ? T("geprüft", "checked") : T("gelöscht", "deleted"))
                       : lz[i].zustand == 2 ? T("Fehler", "error") : T("unverändert", "unchanged");
        uint32_t zf = wartet ? S_GRAU : laeuft ? S_TUERKIS : lz[i].zustand == 1 ? S_GRUEN : lz[i].zustand == 2 ? N_ROT : S_GRAU;
        text_rechts(x + w - 20, cy + 9, 1, 24, zf, zt);
        int tx = sx + 19 + 20, tb = x + w - 20 - textbreite(1, 24, zt) - 20 - tx;
        const char *nm = wartet ? (rest == 1 ? T("Noch 1 Eintrag", "1 more entry") : a_fmt(fa, T("Noch %d Einträge", "%d more entries"), rest)) : lz[i].name;
        const char *u = wartet ? T("kommt danach", "comes next") : lz[i].hat_b ? groesse_von(fa, lz[i].b, false) : "";
        text(tx, cy - (u[0] ? 4 : -10), 1, 28, wartet ? S_TEXT2 : S_TEXT_W, kuerzen_still(1, 28, nm, tb));
        if (u[0]) text(tx, cy + 28, 0, 24, S_GRAU, u);
        zy += zh + 8;
    }
    wusstest11(rx + 32, zy + 4, rw - 64, 1);
    {   /* unten: nicht ausschalten bzw. Anhalten */
        const char *t = anhalten_gewuenscht && A.anhaltbar ? (nr > 0 ? T("Hält nach diesem Eintrag an - der laufende wird noch fertig gelöscht.", "Stopping after this entry - the current one is still being finished.")
                                                                     : T("Hält an, bevor etwas gelöscht wird.", "Stopping before anything is deleted."))
                      : sim ? T("Simulation: Es wird nichts gelöscht.", "Simulation: nothing is deleted.") : T("Bitte die PS5 jetzt nicht ausschalten.", "Please do not switch off the PS5 now.");
        int hy = unten - 32 - 62;
        gold11(rx + 32, hy, rw - 64, 62, 14, 1);
        symbol(SYM_WARNUNG, rx + 32 + 20, hy + 16, 30, S_GOLD);
        text_passend(rx + 32 + 20 + 30 + 16, hy + 40, 0, 24, 20, S_GOLD_T, t, rw - 64 - 20 - 30 - 16 - 20);
    }
    /* Fußleiste: links der Satz aus dem Entwurf, dazu ○ halten (Anhalten) */
    bool anhalten = A.loescht && A.anhaltbar && !anhalten_gewuenscht;
    {
        int x = R11_X, cy = FUSS11_Y + 22;
        text(x, gl11(FUSS11_Y, 44, 24), 0, 24, S_GRAU, T("Geschützte Daten werden nicht angefasst.", "Protected data is never touched.")); x += textbreite(0, 24, T("Geschützte Daten werden nicht angefasst.", "Protected data is never touched.")) + 32;
        if (anhalten) { knopf11(KN_O, x, cy, S_TEXT2); x += 42 + 12; text(x, gl11(FUSS11_Y, 44, 24), 0, 24, S_TEXT2, T("halten: nach diesem Eintrag anhalten", "hold: stop after this entry")); }
    }
    fuss11(NULL, 0, NULL);
    if (kreis_seit && anhalten) { int anteil = (int)(SDL_GetTicks() - kreis_seit); if (anteil > 1000) anteil = 1000; rechteck(0, HOEHE - 6, BREITE * anteil / 1000, 6, S_TUERKIS); }
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
enum { T_X, T_KREIS, T_QUADRAT, T_DREIECK, T_HOCH, T_RUNTER, T_LINKS, T_RECHTS, T_L1, T_R1, T_OPTIONS, T_L2, T_R2, T_ANDERE };   /* L2/R2 neu in 1.1 */
static void tastatur_auf(void);
/* =================================================================== NEU 1.1 (Entwurf "5 – Fertig", "5b – Nicht alles gelöscht", "6 – Rückmeldung")
 * Fertig: links das Ergebnis je Eintrag, rechts was als Nächstes kommt. Neu starten / Beenden öffnen zuerst die Rückmeldung (da_Chris86
 * 15.11.2026: "Bei Rückmeldung, bevor neu gestartet oder beendet wird"); mit Internet sendet die PS5 direkt (bereinigtes Protokoll, ohne
 * Namen, Konten, IP/MAC, Seriennummer), ohne Internet nur der QR-Code fürs Handy. "Noch einmal versuchen" (5b): nach dem Nachschauen sind die
 * Einträge angehakt, die übrig sind. */
static struct {
    bool offen; int aktion;            /* 1 Beenden, 2 Neu starten, 0 zurück zur Fertig-Seite */
    int fokus, wertung;                /* fokus: 0 Ja, 1 Nein, 2 Textfeld, 3 Senden, 4 Ohne, 5 Grund (NEU 1.3); wertung 0 gut, 1 Problem */
    int grund, gfokus;                 /* NEU 1.3 (21.11.2026): bei "Nein" ein Tipp, was nicht geklappt hat - 0 keiner, 1..4 RM_GRUND; gfokus 0..3 */
    char text[1024]; bool tippt;
    volatile int netz;                 /* 0 wird geprüft, 1 Internet, 2 kein Internet */
    volatile int senden;               /* 0 nichts, 1 läuft, 2 gesendet, 3 ging nicht */
    Uint32 zeit; bool offline;         /* offline = QR-Code (kein Internet oder Senden ging nicht) */
    size_t prot_bytes;
} RM;
/* NEU 1.3: die festen Gründe (gleiche Kürzel wie die Rückmeldeseite, rueckmeldung/src/index.js GRUENDE) */
static const char *const RM_GRUND[4][3] = { { "fehlt", "Etwas fehlt", "Something missing" }, { "zuordnung", "Falsch zugeordnet", "Wrongly grouped" },
                                            { "loeschen", "Löschen ging nicht", "Deleting failed" }, { "haengt", "Hängt / stürzt ab", "Hangs / crashes" } };
static const char *rm_grund_name(int g) { return g >= 1 && g <= 4 ? T(RM_GRUND[g - 1][1], RM_GRUND[g - 1][2]) : ""; }
static char *rm_koerper; static size_t rm_koerper_n;   /* Formular-Daten für den Faden (gehören ihm, bis er fertig ist) */
static void rm_name_merken(void) { plattform_name_merken(RUECKMELDUNG_ADRESSE); }
static const char *rm_url_basis(void) { static char b[160]; if (!b[0]) { snprintf(b, sizeof b, "%s", RUECKMELDUNG_ADRESSE); char *r = strrchr(b, '/'); if (r && r > b + 8) *r = 0; } return b; }
static void *rm_netz_faden(void *x)
{
    (void)x; char g[400] = "";
    char u[200]; snprintf(u, sizeof u, "%s/bereit", rm_url_basis());
    bool ok = plattform_erreichbar(u, VERSION_NUMMER, g, sizeof g);
    __atomic_store_n(&RM.netz, ok ? 1 : 2, __ATOMIC_RELEASE);
    if (!ok && g[0]) { char z[480]; snprintf(z, sizeof z, "Rückmeldung, Internet-Prüfung: %s", g); plattform_protokoll_zeile(z); }
    neu_zeichnen();
    return NULL;
}
static void *rm_sende_faden(void *x)
{
    (void)x; char g[400] = "";
    char u[200]; snprintf(u, sizeof u, "%s/direkt", rm_url_basis());
    bool ok = plattform_senden(u, VERSION_NUMMER, rm_koerper, rm_koerper_n, g, sizeof g);
    free(rm_koerper); rm_koerper = NULL;
    if (g[0]) { char z[480]; snprintf(z, sizeof z, "Rückmeldung senden: %s", g); plattform_protokoll_zeile(z); }
    __atomic_store_n(&RM.senden, ok ? 2 : 3, __ATOMIC_RELEASE);
    neu_zeichnen();
    return NULL;
}
/* das Protokoll ohne persönliche Angaben (wie die Browser-Bedienung, fern_seite.html protokollBereinigt) */
static const char *protokoll_bereinigt(arena *a, size_t max)
{
    pthread_mutex_lock(&G.m);
    size_t g = 1; for (int i = 0; i < G.log_n; i++) g += strlen(G.log[i]) + 1;
    char *t = a_hole(a, g), *o = t;
    for (int i = 0; i < G.log_n; i++) { size_t l = strlen(G.log[i]); memcpy(o, G.log[i], l); o += l; *o++ = '\n'; }
    *o = 0;
    pthread_mutex_unlock(&G.m);
    const char *x = t;
    x = r_r(a, x, "[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(\\.[A-Za-z0-9-]+)+", "[E-Mail]");
    x = r_r(a, x, "(?<![0-9A-Fa-f:-])[0-9A-Fa-f]{2}([:-])(?:[0-9A-Fa-f]{2}\\1){4}[0-9A-Fa-f]{2}(?![0-9A-Fa-f:-])", "[MAC]");
    x = r_r(a, x, "(?<![0-9.])(?:[0-9]{1,3}\\.){3}[0-9]{1,3}(?![0-9]|\\.[0-9])", "[IP]");
    /* Codex-Prüfung 339 (P2): auch IPv6 (mit "::" oder voll ausgeschrieben - Uhrzeiten wie 16:31:20 bleiben), Seriennummern und die
     * Benutzer-Kennung vor ":" */
    x = r_r(a, x, "(?<![0-9A-Za-z:.])(?=[0-9A-Fa-f:]*[0-9A-Fa-f])(?:(?:[0-9A-Fa-f]{1,4}:){7}[0-9A-Fa-f]{1,4}|(?:[0-9A-Fa-f]{0,4}:){1,7}:(?:[0-9A-Fa-f]{0,4}:){0,6}[0-9A-Fa-f]{0,4})(?![0-9A-Za-z:])", "[IP]");
    x = r_r(a, x, "(?i)(seriennummer|serien-nr\\.?|serial(?: number)?|s/n)(\\s*[:=]?\\s*)[A-Z0-9-]{6,}", "$1$2[entfernt]");
    x = r_r(a, x, "(/user/home/)[0-9A-Fa-f]{4,16}(?![0-9A-Fa-f])", "$1[Benutzer]");
    size_t l = strlen(x);
    if (l > max) { const char *p = x + (l - max); while (*p && ((unsigned char)*p & 0xC0) == 0x80) p++; x = VERB(a, "…\n", p); }   /* die neuesten Zeilen bleiben */
    return x;
}
/* Codex-Prüfung 357 (P2): der Freitext ohne IP-, E-Mail- und MAC-Adressen, Seriennummern und lange Nummern (wie der Worker beim Speichern) -
 * "Nie dabei" gilt damit auch für das Textfeld, soweit es sich erkennen lässt */
static const char *text_geschwaerzt(arena *a, const char *x)
{
    return h_text_schwaerzen(a, x);   /* NEU 1.2: die Regeln stehen im Kern (kern/hilfen.c) - dort am PC getestet */
}
static const char *url_kodiert(arena *a, const char *t)
{
    size_t l = strlen(t); char *o = a_hole(a, l * 3 + 1), *p = o;
    for (const unsigned char *q = (const unsigned char *)t; *q; q++) {
        if ((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') || (*q >= '0' && *q <= '9') || *q == '-' || *q == '_' || *q == '.' || *q == '~') *p++ = (char)*q;
        else if (*q == ' ') *p++ = '+';
        else { static const char H[] = "0123456789ABCDEF"; *p++ = '%'; *p++ = H[*q >> 4]; *p++ = H[*q & 15]; }
    }
    *p = 0; return o;
}
static void rm_oeffnen(int aktion, int wertung)
{
    memset(&RM, 0, sizeof RM); RM.offen = true; RM.aktion = aktion; RM.wertung = wertung; RM.fokus = wertung ? 1 : 0;
    rm_hg_lesen();
    RM.prot_bytes = strlen(protokoll_bereinigt(fa, 100000));
    pthread_t t; if (basis_faden(&t, rm_netz_faden, NULL) == 0) pthread_detach(t); else RM.netz = 2;
}
static void rm_aktion_ausfuehren(void)   /* Neu starten / Beenden wie bisher (D_FERTIG beantworten) */
{
    int a = RM.aktion; RM.offen = false;
    if (a == 0 || G.art != D_FERTIG || G.beantwortet) return;
    nach_aktion = a;
#ifndef VORSCHAU
    A.schluss_ohne_suche = a == 1;   /* NEU 1.0.10 (PS5-Test 13.11.2026): Beenden - nicht erst noch einmal alles nachschauen */
#endif
    antworten(false, false);
}
static void rm_senden(void)
{
    rueckmeldung r = rueckmeldung_daten();
    const char *l = rueckmeldung_link_x(fa, &r, -1, -1, true, true), *q = strchr(l, '?');   /* NEU 1.3: nur das direkte Senden nimmt hg mit (steht unter "Das wird gesendet") */
    char fid[40]; { unsigned x = (unsigned)SDL_GetTicks() ^ (unsigned)time(NULL) * 2654435761u; snprintf(fid, sizeof fid, "ps5-%08x%08x%02x", x, (unsigned)rand(), (unsigned)rand() & 0xFF); }   /* 22 Zeichen wie beim Worker (RE_FID) */
    const char *k = VERB(fa, q ? q + 1 : "", "&wertung=", RM.wertung ? "problem" : "gut", "&quelle=ps5&art=", K->KOMPLETT_LAUF ? "komplett" : "auswahl",
                         "&fid=", fid, RM.wertung && RM.grund >= 1 && RM.grund <= 4 ? VERB(fa, "&gr=", RM_GRUND[RM.grund - 1][0]) : "", RM.text[0] ? VERB(fa, "&text=", url_kodiert(fa, text_geschwaerzt(fa, RM.text))) : "",
                         "&mit_protokoll=1&protokoll=", url_kodiert(fa, protokoll_bereinigt(fa, 100000)));
    free(rm_koerper); rm_koerper_n = strlen(k); rm_koerper = malloc(rm_koerper_n + 1);
    if (!rm_koerper) { RM.senden = 3; RM.zeit = SDL_GetTicks(); return; }
    memcpy(rm_koerper, k, rm_koerper_n + 1);
    RM.senden = 1; RM.zeit = SDL_GetTicks();
    pthread_t t; if (basis_faden(&t, rm_sende_faden, NULL) == 0) pthread_detach(t); else { free(rm_koerper); rm_koerper = NULL; RM.senden = 3; }
}
static void rm_nachfuehren(void)   /* aus dem Zeichnen: nach dem Senden weiter */
{
    if (RM.tippt && !tastatur_offen) {   /* Text übernehmen, höchstens 200 Zeichen */
        RM.tippt = false;
        pthread_mutex_lock(&G.m); const char *e = G.eingabe; int z = 0; size_t j = 0;
        for (const unsigned char *p = (const unsigned char *)e; *p && j + 4 < sizeof RM.text; ) {
            int n = *p >= 0xF0 ? 4 : *p >= 0xE0 ? 3 : *p >= 0xC0 ? 2 : 1;
            if (z >= 200) break;
            memcpy(RM.text + j, p, (size_t)n); j += (size_t)n; p += n; z++;
        }
        RM.text[j] = 0; G.eingabe[0] = 0; pthread_mutex_unlock(&G.m);
    }
    int sd = __atomic_load_n(&RM.senden, __ATOMIC_ACQUIRE);
    if ((sd == 2 || sd == 3) && !RM.zeit) RM.zeit = SDL_GetTicks();
    if (sd == 2 && SDL_GetTicks() - RM.zeit > 3000) { ui_log(NULL, T("Rückmeldung gesendet - danke!", "Feedback sent - thank you!")); rm_aktion_ausfuehren(); }
    if (sd == 3 && SDL_GetTicks() - RM.zeit > 3000) { RM.senden = 0; RM.offline = true; RM.zeit = 0; ui_log(NULL, T("Rückmeldung: Senden ging nicht - QR-Code fürs Handy.", "Feedback: sending failed - QR code for the phone.")); }
}
static const char *dauer_text(int s)
{
    if (s < 60) return a_fmt(fa, T("%d Sekunden", "%d seconds"), s);
    int m = s / 60, r = s % 60;
    return r ? a_fmt(fa, T("%d Minuten %d Sekunden", "%d minutes %d seconds"), m, r) : a_fmt(fa, T("%d Minuten", "%d minutes"), m);
}
static void wahl_karte(int x, int y, int w, int h, bool an, bool fokus, bool warn, const char *titel, const char *satz)
{
    if (fokus) fokus11(x, y, w, h, 18, 2);
    else if (an) { flaeche f = F_FOKUS11(1); flaeche_zeichnen(x, y, w, h, 18, &f); }
    else { flaeche f = F_FELD11(); flaeche_zeichnen(x, y, w, h, 18, &f); }
    int cx = x + 24 + 23, cy = y + 24 + 23;
    ring(cx, cy, 22, 2, warn ? S_GOLD : S_TUERKIS);
    if (warn) text_mitte(cx, cy + 9, 1, 26, S_GOLD, "!"); else symbol(SYM_HAKEN, cx - 13, cy - 13, 26, S_TUERKIS);
    text_passend(x + 24, y + h - 58, 3, 28, 22, S_TEXT_W, titel, w - 48);
    text_passend(x + 24, y + h - 22, 0, 24, 20, S_TEXT2, satz, w - 48);
}
static void knopf_breit(int x, int y, int w, int h, bool haupt, bool fokus, knopfart k, const char *t)
{
    if (haupt) { leuchten(x, y, w, h, 14, 20, S_BLAU, fokus ? .5f : .3f); flaeche f = F_HAUPT11(); flaeche_zeichnen(x, y, w, h, 14, &f); }
    else if (fokus) fokus11(x, y, w, h, 14, 2);
    else { flaeche f = F_NEBEN11(); flaeche_zeichnen(x, y, w, h, 14, &f); }
    if (fokus && haupt) { flaeche r = { false, V1(0, 0), S_TUERKIS, 1, 1, 3, 0, 0 }; flaeche_zeichnen(x - 4, y - 4, w + 8, h + 8, 18, &r); }
    int tw = textbreite(3, 28, t), gx = x + (w - (42 + 16 + tw)) / 2;
    knopf11(k, gx, y + h / 2, S_TEXT_W);
    text(gx + 42 + 16, gl11(y, h, 28), 3, 28, S_TEXT_W, kuerzen_still(3, 28, t, w - 42 - 40));
}
/* NEU 1.1 (Entwurf UpdateDialog): Knopf mit Unterzeile - Symbol links, fett der Text, darunter klein der Zusatz */
static void knopf_breit2(int x, int y, int w, int h, bool haupt, bool fokus, knopfart k, const char *t, const char *u)
{
    if (haupt) { leuchten(x, y, w, h, 14, 20, S_BLAU, fokus ? .5f : .3f); flaeche f = F_HAUPT11(); flaeche_zeichnen(x, y, w, h, 14, &f); }
    else if (fokus) fokus11(x, y, w, h, 14, 2);
    else { flaeche f = F_NEBEN11(); flaeche_zeichnen(x, y, w, h, 14, &f); }
    if (fokus && haupt) { flaeche r = { false, V1(0, 0), S_TUERKIS, 1, 1, 3, 0, 0 }; flaeche_zeichnen(x - 4, y - 4, w + 8, h + 8, 18, &r); }
    int gx = x + 26, tx = gx + 42 + 18, tb = x + w - 20 - tx;
    if (fokus) knopf11(k, gx, y + h / 2, S_TEXT_W);   /* NEU 1.1 (PS5-Test 15.11.2026): das Tastenzeichen nur auf dem Knopf, den ✕ gerade auslöst */
    text(tx, y + h / 2 - 6, 3, 28, S_TEXT_W, kuerzen_still(3, 28, t, tb));
    text(tx, y + h / 2 + 28, 0, 22, haupt ? 0xD6E4FF : S_GRAU, kuerzen_still(0, 22, u, tb));
}
static void schirm_rueckmeldung(void)
{
    rm_nachfuehren();
    if (!RM.offen) return;
    hintergrund11();
    bilder_neu = 0;
    rueckmeldung r = rueckmeldung_daten();
    bool komplett = K->KOMPLETT_LAUF, neustart = RM.aktion == 2 || (RM.aktion == 1 && neustart_pflicht);
    int netz = __atomic_load_n(&RM.netz, __ATOMIC_ACQUIRE), sd = __atomic_load_n(&RM.senden, __ATOMIC_ACQUIRE);
    bool offline = RM.offline || netz == 2;
    /* Kopf: Schriftzug | Kurz bevor du gehst | (nach „…“ · Neu starten) ... Internet */
    {
        int cy = R11_O + 25, x = R11_X;
        x += logo11(x, cy - 22) + 24; rechteck(x, cy - 17, 2, 34, 0x222C3A); x += 26;
        const char *ti = T("Kurz bevor du gehst", "Before you go");
        text(x, gl11(cy - 22, 44, 30), 1, 30, S_TEXT, ti); x += textbreite(1, 30, ti) + 24;
        if (RM.aktion) {
            const char *c = VERB(fa, komplett ? T("nach „Alles bereinigen“", "after \"Clean up everything\"") : T("nach „Selbst auswählen“", "after \"Choose yourself\""), " · ",
                                 neustart ? T("Neu starten", "Restart") : T("Beenden", "Quit"));
            int cw = textbreite(1, 24, c) + 32;
            flaeche f = { false, V1(S_TUERKIS, .08f), S_TUERKIS, .7f, .7f, 1, 0, 0 }; flaeche_zeichnen(x, cy - 23, cw, 46, 23, &f);
            text(x + 16, gl11(cy - 23, 46, 24), 1, 24, S_TUERKIS, c);
        }
        const char *nt = netz == 0 ? T("Internet wird geprüft …", "Checking internet …") : offline && netz == 1 ? T("Senden ging nicht", "Sending failed") : netz == 1 ? T("Internet verbunden", "Internet connected") : T("Kein Internet", "No internet");
        int nw = textbreite(2, 24, nt) + 32 + 22, nx = BREITE - R11_X - nw;
        uint32_t nf = netz == 1 && !offline ? S_GRUEN : S_GRAU;
        flaeche f = { false, V1(netz == 1 && !offline ? S_GRUEN : 0xFFFFFF, .06f), netz == 1 && !offline ? S_GRUEN_R : 0x3B4A63, 1, 1, 1, 0, 0 }; flaeche_zeichnen(nx, cy - 23, nw, 46, 23, &f);
        kreis(nx + 22, cy, 6, nf); text(nx + 38, gl11(cy - 23, 46, 24), 2, 24, netz == 1 && !offline ? S_GRUEN : S_TEXT2, nt);
    }
    int y0 = R11_O + 50 + 20, unten = FUSS11_Y + 44, lw = 1094, lx = R11_X, rx = lx + lw + 24, rw = BREITE - R11_X - rx;
    karte11(lx, y0, lw, unten - y0, 20);
    const char *aktt = RM.aktion == 0 ? T("zurück", "back") : neustart ? T("neu starten", "restart") : T("beenden", "quit");
    if (!offline) {
        /* 6 / 6b: Hat alles geklappt? */
        bool geloescht = r.s == 2;
        text_passend(lx + 40, y0 + 40 + 50, 3, 56, 36, S_TEXT_W, !geloescht ? T("Wie läuft PS5 Cleanup bei dir?", "How is PS5 Cleanup doing for you?") : komplett ? T("Ist die PS5 jetzt sauber?", "Is the PS5 clean now?") : T("Hat alles geklappt?", "Did everything work?"), lw - 80);
        const char *sz = !geloescht ? T("Bis jetzt wurde nichts gelöscht.", "Nothing has been deleted so far.") : komplett ? VERB(fa, T("Du hast die interne SSD", "You cleaned the internal SSD"), A.leeren_m2 ? T(" und die M.2-SSD", " and the M.2 SSD") : "", T(" komplett bereinigt.", " completely."))
                                  : rm_satz_selbst(&r);
        sz = VERB(fa, sz, T(" Deine Rückmeldung zeigt, wie PS5 Cleanup auf anderen Konsolen läuft – auch wenn alles gut ging.", " Your feedback shows how PS5 Cleanup runs on other consoles – even if everything went well."));
        zeilen z = umbrechen(0, 28, sz, lw - 80); if (z.n > 3) z.n = 3;
        for (int i = 0; i < z.n; i++) text(lx + 40, y0 + 40 + 50 + 40 + 30 + i * 42, 0, 28, S_TEXT2, z.z[i]);
        int cy = y0 + 264, cw = (lw - 80 - 18) / 2, chh = 200;
        if (!geloescht) komplett = false;
        wahl_karte(lx + 40, cy, cw, chh, RM.wertung == 0, RM.fokus == 0, false, komplett ? T("Ja, alles sauber", "Yes, all clean") : T("Ja, alles gut", "Yes, all good"),
                   komplett ? T("Auf den gewählten Laufwerken ist alles weg", "Everything on the chosen drives is gone") : T("Alles Ausgewählte ist weg", "Everything selected is gone"));
        wahl_karte(lx + 40 + cw + 18, cy, cw, chh, RM.wertung == 1, RM.fokus == 1, true, komplett ? T("Nein, etwas ist noch da", "No, something is still there") : T("Nein, etwas hat nicht geklappt", "No, something did not work"),
                   T("Das Protokoll wird mitgeschickt", "The log is sent along"));
        /* NEU 1.3 (21.11.2026): bei "Nein" ein Tipp, was nicht geklappt hat (freiwillig) - darunter das Textfeld */
        int ty = cy + chh + 24, th = 98;
        if (RM.wertung == 1) {
            int gh = 64, gw = (lw - 80 - 3 * 12) / 4;
            for (int i = 0; i < 4; i++) {
                int gx = lx + 40 + i * (gw + 12); bool an = RM.grund == i + 1, fk = RM.fokus == 5 && RM.gfokus == i;
                if (fk) fokus11(gx, ty, gw, gh, 32, 2);
                else if (an) { flaeche f = F_FOKUS11(1); flaeche_zeichnen(gx, ty, gw, gh, 32, &f); }
                else { flaeche f = F_FELD11(); flaeche_zeichnen(gx, ty, gw, gh, 32, &f); }
                int gs = 24; while (gs > 18 && textbreite(2, gs, rm_grund_name(i + 1)) > gw - 24) gs--;   /* PS5-Test 21.11.2026: ganz lesbar statt "…" */
                text_mitte(gx + gw / 2, ty + gh / 2 + 9, 2, gs, an || fk ? S_TEXT_W : S_TEXT2, kuerzen_still(2, gs, rm_grund_name(i + 1), gw - 24));
            }
            ty += gh + 16;
        }
        /* Textfeld */
        if (RM.fokus == 2) fokus11(lx + 40, ty, lw - 80, th, 16, 2);
        else { flaeche f = { false, V1(0xFFFFFF, .035f), 0xFFFFFF, .18f, .18f, 1, 0, 0 }; flaeche_zeichnen(lx + 40, ty, lw - 80, th, 16, &f); }
        symbol(SYM_DOKUMENT, lx + 40 + 24, ty + th / 2 - 15, 30, S_TEXT2);
        if (RM.text[0]) { text_passend(lx + 40 + 74, ty + 42, 1, 28, 22, S_TEXT_W, RM.text, lw - 80 - 98); text(lx + 40 + 74, ty + 76, 0, 24, S_GRAU, T("✕ ändert den Text", "✕ changes the text")); }
        else { text(lx + 40 + 74, ty + 42, 1, 28, S_TEXT_W, T("Kurz etwas dazuschreiben", "Add a short note")); text(lx + 40 + 74, ty + 76, 0, 24, S_GRAU, tastatur_offen ? T("Tastatur ist offen …", "Keyboard is open …") : T("freiwillig · bis 200 Zeichen · ✕ öffnet die Tastatur der PS5", "optional · up to 200 characters · ✕ opens the PS5 keyboard")); }
        /* Hinweis und Knöpfe */
        const char *hw = RM.aktion == 0 ? T("Danach geht es zurück zum Ergebnis.", "Afterwards you return to the result.")
                       : neustart ? T("Du hast „Neu starten“ gewählt – die PS5 startet danach neu, mit oder ohne Rückmeldung.", "You chose \"Restart\" – the PS5 restarts afterwards, with or without feedback.")
                                  : T("Du hast „Beenden“ gewählt – PS5 Cleanup beendet sich danach, mit oder ohne Rückmeldung.", "You chose \"Quit\" – PS5 Cleanup quits afterwards, with or without feedback.");
        int by = unten - 40 - 76;
        symbol(SYM_INFO, lx + 40, by - 70, 28, S_GRAU2);
        { zeilen hz = umbrechen(0, 26, hw, lw - 80 - 46); for (int i = 0; i < hz.n && i < 2; i++) text(lx + 40 + 46, by - 70 + 22 + i * 32, 0, 26, S_TEXT2, hz.z[i]); }
        int b1 = (lw - 80 - 18) * 47 / 100, b2 = lw - 80 - 18 - b1;
        /* NEU 1.1 (PS5-Test 15.11.2026): Blau folgt dem Fokus (steht er oben, ist "Senden" blau) */
        knopf_breit(lx + 40, by, b1, 76, RM.fokus != 4, RM.fokus == 3, KN_X, VERB(fa, T("Senden und ", "Send and "), aktt));
        knopf_breit(lx + 40 + b1 + 18, by, b2, 76, RM.fokus == 4, RM.fokus == 4, KN_O, VERB(fa, T("Ohne Rückmeldung ", "Without feedback: "), aktt));
    } else {
        /* 6a: ohne Internet - QR-Code fürs Handy */
        text_passend(lx + 40, y0 + 40 + 50, 3, 56, 36, S_TEXT_W, T("Rückmeldung per Handy", "Feedback via phone"), lw - 80);
        const char *sz = RM.offline && netz == 1 ? T("Direkt senden hat gerade nicht geklappt. Deine Rückmeldung geht trotzdem – über dein Handy.", "Sending directly did not work just now. Your feedback still works – via your phone.")
                                                 : T("Die PS5 ist gerade nicht mit dem Internet verbunden. Deine Rückmeldung geht trotzdem – über dein Handy.", "The PS5 is not connected to the internet right now. Your feedback still works – via your phone.");
        zeilen z = umbrechen(0, 28, sz, lw - 80); for (int i = 0; i < z.n && i < 3; i++) text(lx + 40, y0 + 40 + 50 + 40 + 30 + i * 42, 0, 28, S_TEXT2, z.z[i]);
        int qn = rueckmeldung_qr(&r), qy = y0 + 300, qs = 340;
        if (qn > 0) { int m = qs / (qn + 8); if (m < 3) m = 3; int s = m * (qn + 8); rund(lx + 120, qy + (qs - s) / 2 - 10, s + 20, s + 20, 18, 0xFFFFFF); qr_malen(lx + 130, qy + (qs - s) / 2, qn, m); }
        int sx = lx + 120 + qs + 60, sw = lx + lw - 40 - sx, yy = qy + 10;
        text(sx, yy + 30, 3, 32, S_TEXT_W, T("So geht es", "How it works")); yy += 64;
        const char *st[3] = { T("1. Code mit dem Handy scannen", "1. Scan the code with your phone"), T("2. „Hat geklappt“ oder „Hat nicht geklappt“ tippen", "2. Tap \"Worked\" or \"Did not work\""), T("3. Senden – erst dann geht etwas raus", "3. Send – only then is anything sent") };
        for (int i = 0; i < 3; i++) { zeilen q = umbrechen(0, 28, st[i], sw); for (int j = 0; j < q.n && j < 2; j++) text(sx, yy + 30 + j * 38, 0, 28, S_TEXT, q.z[j]); yy += q.n * 38 + 16; }
        { zeilen q = umbrechen(0, 24, neustart ? T("Das Protokoll ist zu groß für den Code – es bleibt auf dem USB-Laufwerk. Freiwillig – du kannst auch einfach neu starten.", "The log is too big for the code – it stays on the USB drive. Optional – you can simply restart.") : T("Das Protokoll ist zu groß für den Code – es bleibt auf dem USB-Laufwerk. Freiwillig – du kannst auch einfach weitermachen.", "The log is too big for the code – it stays on the USB drive. Optional – you can simply carry on."), sw);
          for (int j = 0; j < q.n && j < 4; j++) text(sx, yy + 26 + j * 30, 0, 24, S_GRAU, q.z[j]); }
        int by = unten - 40 - 76;
        knopf_breit(lx + 40, by, lw - 80, 76, true, true, KN_X, RM.aktion == 0 ? T("Zurück", "Back") : neustart ? T("Neu starten", "Restart") : T("Beenden", "Quit"));
    }
    /* rechts: Das wird gesendet / Das steckt im QR-Code */
    {
        const char *k[14], *v[14]; int n = 0;   /* Codex-Prüfung 339 (P1): bis zu 11 Zeilen (Alles bereinigen, online, mit Text); NEU 1.3: 12 mit Hintergrund */
        const char *reihe = K->MODELL_TYP[0] ? kern_modell_reihe(K->MODELL_TYP, K->MODELL_DIGITAL) : "PS5";
        k[n] = T("Konsole", "Console"); v[n++] = VERB(fa, reihe, r.modell[0] ? VERB(fa, " · ", r.modell) : "");
        k[n] = "Firmware"; v[n++] = r.fw[0] ? r.fw : T("unbekannt", "unknown");
        k[n] = "PS5 Cleanup"; v[n++] = VERB(fa, "Version ", VERSION_NUMMER);
        k[n] = T("Art", "Type"); v[n++] = komplett ? T("Alles bereinigen", "Clean up everything") : T("Selbst ausgewählt", "Selected yourself");
        if (komplett) { k[n] = T("Laufwerke", "Drives"); v[n++] = VERB(fa, T("Interne SSD", "Internal SSD"), A.leeren_m2 ? ", M.2-SSD" : "", A.leeren_usb ? T(", erweiterter Speicher", ", extended storage") : ""); }
        k[n] = T("Ergebnis", "Result"); v[n++] = a_fmt(fa, T("%d gelöscht · %d Fehler", "%d deleted · %d errors"), rm_geloescht(&r), r.f);
        k[n] = T("Freigeräumt", "Freed"); v[n++] = groesse_von(fa, A.rm_frei.Bytes, A.rm_frei.Gekappt);
        k[n] = T("Nachschauen", "Scan"); v[n++] = dauer_text(r.t);   /* 1.3 (da_Chris86 21.11.2026): es ist die Dauer des Nachschauens, nicht des Löschens */
        if (!offline) { k[n] = T("Protokoll", "Log"); v[n++] = VERB(fa, T("mit Spiel-/App-Namen · ", "with game/app names · "), kern_groesse_text(fa, (long long)RM.prot_bytes)); }   /* ehrlich: das bereinigte Protokoll nennt Spiele und Apps */
        if (!offline) { k[n] = T("Deine Antwort", "Your answer"); v[n++] = RM.wertung ? (RM.grund ? VERB(fa, T("Nein · ", "No · "), rm_grund_name(RM.grund)) : komplett ? T("Nein, etwas ist noch da", "No, something is still there") : T("Nein, etwas hat nicht geklappt", "No, something did not work")) : (komplett ? T("Ja, alles sauber", "Yes, all clean") : T("Ja, alles gut", "Yes, all good")); }
        if (!offline && RM.text[0]) { k[n] = T("Dein Text", "Your note"); v[n++] = text_geschwaerzt(fa, RM.text); }
        if (!offline && r.hg[0]) { k[n] = T("Im Hintergrund", "In background"); v[n++] = r_r(fa, r.hg, ",", ", "); }   /* NEU 1.3: Namen der laufenden Programme */
        int zh = n > 11 ? 52 : 56;   /* NEU 1.3: mit 12 Zeilen etwas enger, damit alles auf den Schirm passt */
        /* NEU 1.3 (PS5-Test da_Chris86 21.11.2026: die Namen der Hintergrundprogramme waren hinten mit "…" abgeschnitten): die letzte Zeile
         * "Im Hintergrund" bricht auf bis zu drei Zeilen um, die Karte wächst mit */
        bool hg_zeile = !offline && r.hg[0]; passend hgp = {0}; int hg_mehr = 0;
        if (hg_zeile) { hgp = einpassen(2, 26, 22, 20, v[n - 1], rw - 64 - 246, 3); hg_mehr = hgp.z.n > 1 ? (hgp.z.n - 1) * (hgp.gr + 6) : 0; }
        int h = 28 + 34 + n * zh + 12 + hg_mehr;
        karte11(rx, y0, rw, h, 20);
        text_gesperrt(rx + 32, y0 + 28 + 24, 1, 24, S_GRAU2, offline ? T("DAS STECKT IM QR-CODE", "WHAT THE QR CODE HOLDS") : T("DAS WIRD GESENDET", "THIS WILL BE SENT"), .14f);
        for (int i = 0; i < n; i++) {
            int yy = y0 + 28 + 34 + i * zh;
            text(rx + 32, yy + 38, 0, 26, S_GRAU, k[i]);
            if (hg_zeile && i == n - 1 && hgp.z.n > 1) { for (int q = 0; q < hgp.z.n && q < 3; q++) text(rx + 32 + 246, yy + 38 + q * (hgp.gr + 6), 2, hgp.gr, S_TEXT_W, hgp.z.z[q]); continue; }
            text_passend(rx + 32 + 246, yy + 38, 2, 26, 20, S_TEXT_W, v[i], rw - 64 - 246);
            rechteck(rx + 32, yy + 55, rw - 64, 1, 0x1E2A3A);
        }
        int gy = y0 + h + 24, gh = 140;
        gruen11(rx, gy, rw, gh, 20);
        symbol(SYM_SCHILD_HAKEN, rx + 32, gy + gh / 2 - 15, 30, S_GRUEN);
        text(rx + 32 + 44, gy + 44, 3, 28, S_TEXT_W, T("Nie dabei", "Never included"));
        zeilen z = umbrechen(0, 24, T("Konten, Passwörter, IP- und MAC-Adressen, Seriennummer", "Accounts, passwords, IP and MAC addresses, serial number"), rw - 64 - 44);
        for (int i = 0; i < z.n && i < 2; i++) text(rx + 32 + 44, gy + 80 + i * 30, 0, 24, S_TEXT, z.z[i]);
    }
    /* kurze Rückmeldung nach dem Senden (Entwurf "Gesendet") */
    if (sd == 1 || sd == 2 || sd == 3) {
        abdunkeln();
        int w = 900, h = 150, x = (BREITE - w) / 2, y = (HOEHE - h) / 2;
        if (sd == 3) gold11(x, y, w, h, 22, 2); else if (sd == 2) gruen11(x, y, w, h, 22); else karte11(x, y, w, h, 22);
        int cx = x + 36 + 38, cy = y + h / 2;
        if (sd == 2) { kreis(cx, cy, 38, S_GRUEN); symbol(SYM_HAKEN, cx - 20, cy - 20, 40, 0x06261C); }
        else if (sd == 3) { ring(cx, cy, 36, 3, S_GOLD); text_mitte(cx, cy + 14, 3, 40, S_GOLD, "!"); }
        else { ring(cx, cy, 30, 5, 0x2A3644); ring_teil(cx, cy, 30, 5, (float)(SDL_GetTicks() % 1200) / 1200.f, S_TUERKIS); lauf_aktiv = true; }
        const char *t1 = sd == 2 ? T("Rückmeldung gesendet – danke!", "Feedback sent – thank you!") : sd == 3 ? T("Senden hat nicht geklappt", "Sending did not work") : T("Wird gesendet …", "Sending …");
        const char *t2 = sd == 2 ? (RM.aktion == 2 || neustart ? T("Die PS5 startet in 3 Sekunden neu …", "The PS5 restarts in 3 seconds …") : RM.aktion == 1 ? T("PS5 Cleanup beendet sich gleich …", "PS5 Cleanup quits in a moment …") : T("Gleich geht es zurück.", "Returning in a moment."))
                       : sd == 3 ? T("Kein Problem – gleich erscheint der QR-Code fürs Handy.", "No problem – the QR code for your phone appears in a moment.") : T("Bitte kurz warten.", "Please wait a moment.");
        text(cx + 38 + 28, cy - 6, 3, 34, S_TEXT_W, t1);
        text_passend(cx + 38 + 28, cy + 32, 0, 26, 22, S_TEXT2, t2, x + w - 36 - (cx + 38 + 28));
        if (sd != 1) lauf_aktiv = true;
    }
}
static void taste_rueckmeldung(int t)
{
    int sd = __atomic_load_n(&RM.senden, __ATOMIC_ACQUIRE), netz = __atomic_load_n(&RM.netz, __ATOMIC_ACQUIRE);
    if (sd) return;   /* während des Sendens und der kurzen Meldung nichts */
    bool offline = RM.offline || netz == 2;
    if (offline) { if (t == T_X || t == T_KREIS) rm_aktion_ausfuehren(); return; }
    switch (t) {
    /* NEU 1.3: Fokus 5 = Zeile mit den Gründen (nur bei "Nein") */
    case T_HOCH: RM.fokus = RM.fokus == 5 ? RM.wertung : (RM.fokus == 3 || RM.fokus == 4) ? 2 : RM.fokus == 2 ? (RM.wertung == 1 ? 5 : RM.wertung) : RM.fokus; break;
    case T_RUNTER: RM.fokus = RM.fokus <= 1 ? (RM.wertung == 1 ? 5 : 2) : RM.fokus == 5 ? 2 : RM.fokus == 2 ? 3 : RM.fokus; break;
    case T_LINKS: if (RM.fokus == 1) RM.fokus = 0; else if (RM.fokus == 4) RM.fokus = 3; else if (RM.fokus == 5 && RM.gfokus > 0) RM.gfokus--; break;
    case T_RECHTS: if (RM.fokus == 0) RM.fokus = 1; else if (RM.fokus == 3) RM.fokus = 4; else if (RM.fokus == 5 && RM.gfokus < 3) RM.gfokus++; break;
    case T_X:
        if (RM.fokus <= 1) RM.wertung = RM.fokus;
        else if (RM.fokus == 5) { if (RM.wertung == 1) RM.grund = RM.grund == RM.gfokus + 1 ? 0 : RM.gfokus + 1; }
        else if (RM.fokus == 2) { tastatur_auf(); RM.tippt = true; }
        else if (RM.fokus == 3) {
            if (netz == 0) { kurz(T("Einen Moment - das Internet wird noch geprüft.", "One moment - the internet is still being checked.")); break; }
            rm_senden();
        }
        else rm_aktion_ausfuehren();
        break;
    case T_KREIS: rm_aktion_ausfuehren(); break;
    default: break;
    }
}
/* ---------------------------------------------------------------- Fertig (5) und Nicht alles gelöscht (5b) */
static bool fertig_fehler(void)
{
    bool ok; pthread_mutex_lock(&G.m); ok = G.f_ok; pthread_mutex_unlock(&G.m);
    int nf = 0; pthread_mutex_lock(&erg_m); for (int i = 0; i < erg_n; i++) if (erg[i].zustand >= 2) nf++; pthread_mutex_unlock(&erg_m);
    return !ok || nf > 0;
}
static bool nochmal_warten;                    /* "Noch einmal versuchen": nach dem Nachschauen die übrigen anhaken */
static char **nochmal_namen; static int nochmal_n;
static void schirm_fertig(void)
{
    if (RM.offen) { schirm_rueckmeldung(); if (RM.offen) return; }
    hintergrund11();
    seite11_titel = fertig_fehler() ? T("Ergebnis", "Result") : "";
    kopf_neu(4, NULL);
    bilder_neu = 0;
    pthread_mutex_lock(&G.m);
    const char *kopf = a_kopie(fa, G.f_kopf ? G.f_kopf : ""), *frei = a_kopie(fa, G.f_frei ? G.f_frei : ""), *sm = a_kopie(fa, G.f_sm ? G.f_sm : "");
    pthread_mutex_unlock(&G.m);
    (void)frei;
    bool sim = K->simulation, fehler = fertig_fehler();
    pthread_mutex_lock(&erg_m);
    int en = erg_n, nf = 0; erg_zeile *ez = a_hole(fa, sizeof(erg_zeile) * (size_t)(en + 1));
    for (int i = 0; i < en; i++) if (erg[i].zustand >= 2) ez[nf++] = erg[i];
    int j = nf; for (int i = 0; i < en; i++) if (erg[i].zustand < 2) ez[j++] = erg[i];
    pthread_mutex_unlock(&erg_m);
    /* oben: großer Haken bzw. Ausrufezeichen, Überschrift */
    {
        int cx = R11_X + 75, cy = R11_O + 50 + 20 + 75;
        uint32_t f = fehler ? S_GOLD : S_GRUEN;
        leuchten(cx - 75, cy - 75, 150, 150, 75, 40, f, .45f); kreis(cx, cy, 75, f);
        if (fehler) text_mitte(cx, cy + 30, 3, 84, 0x241A05, "!"); else symbol(SYM_HAKEN, cx - 42, cy - 42, 84, 0x0D1119);
        const char *gt = groesse_von(fa, A.rm_frei.Bytes, A.rm_frei.Gekappt);
        const char *ti = fehler ? (nf == 1 ? T("Eins ist noch nicht weg.", "One thing is not gone yet.") : nf > 1 ? a_fmt(fa, T("%d sind noch nicht weg.", "%d are not gone yet."), nf) : T("Nicht alles hat geklappt.", "Not everything worked."))
                            : sim ? T("Simulation fertig.", "Simulation done.") : T("Alles gelöscht.", "All deleted.");
        int tx = R11_X + 150 + 40;
        text_passend(tx, cy + 4, 3, 72, 44, S_TEXT_W, ti, BREITE - R11_X - tx);
        const char *s1 = fehler ? T("Alles andere ist gelöscht · ", "Everything else is deleted · ") : "";
        const char *s3 = fehler ? T(" freigeräumt", " freed") : sim ? T(" würden frei · es wurde nichts gelöscht", " would be freed · nothing was deleted") : T(" freigeräumt · nichts ist übrig geblieben", " freed · nothing is left over");
        int x = tx; text(x, cy + 56, 0, 30, S_TEXT2, s1); x += textbreite(0, 30, s1);
        text(x, cy + 56, 1, 30, fehler ? S_TEXT_W : S_GRUEN, gt); x += textbreite(1, 30, gt);
        text_passend(x, cy + 56, 0, 30, 22, S_TEXT2, s3, BREITE - R11_X - x);
    }
    int y0 = R11_O + 50 + 20 + 150 + 30, unten = FUSS11_Y - 20, kw = fehler ? 560 : 520, lx = R11_X, rx = BREITE - R11_X - kw, lw = rx - 24 - lx;
    /* links: Ergebnis */
    karte11(lx, y0, lw, unten - y0, 20);
    text_gesperrt(lx + 32, y0 + 24 + 24, 1, 24, S_GRAU2, T("ERGEBNIS", "RESULT"), .14f);
    const char *wo = protokoll_wo(), *rt = kern_rest_text(K, fa);
    b_hinweis hw[6]; int nh = 0;
    hw[nh++] = (b_hinweis){ wo[0] ? VERB(fa, T("Das Protokoll liegt ", "The log is "), wo, ".") : T("Das Protokoll siehst du mit der Options-Taste.", "View the log with the Options button."), N_GRAU, SYM_INFO };
    { const char *nl = strchr(kopf, '\n'); if (nl && nl[1]) hw[nh++] = (b_hinweis){ nl + 1, N_GRAU, SYM_INFO }; }
    if (sm[0]) hw[nh++] = (b_hinweis){ sm, N_GELB, SYM_INFO };
    if (rt[0]) hw[nh++] = (b_hinweis){ rt, fehler ? N_GELB : N_GRAU, fehler ? SYM_WARNUNG : SYM_INFO };
    int hinh = hinweise_zeichnen(0, 0, lw - 64 - 12, hw, nh, false); if (hinh > 190) hinh = 190;
    int ly = y0 + 24 + 24 + 22, lu = unten - 24 - hinh - 18, zh = 84;
    hinweise_box(lx + 32 + 4, unten - 24 - hinh, lw - 64 - 12, hinh, hw, nh);
    int platz = (lu - ly + 8) / (zh + 8), zeig = en < platz ? en : (platz > 1 ? platz - 1 : 0);
    if (en == 0) text(lx + 32, ly + 40, 0, 26, S_GRAU, sim ? T("In der Simulation wurde nichts gelöscht.", "Nothing was deleted in the simulation.") : T("Es wurde nichts gelöscht.", "Nothing was deleted."));
    for (int i = 0; i < zeig; i++) {
        const erg_zeile *e = &ez[i]; int yy = ly + i * (zh + 8), cy = yy + zh / 2;
        bool fz = e->zustand >= 2;
        if (fz) { flaeche f = { false, V1(0x1A1608, .9f), S_GOLD, 1, 1, 2, 0, 0 }; flaeche_zeichnen(lx + 32, yy, lw - 64, zh, 14, &f); }
        else { flaeche f = { false, V1(0xFFFFFF, .045f), 0, 0, 0, 0, 0, 0 }; flaeche_zeichnen(lx + 32, yy, lw - 64, zh, 14, &f); }
        const char *nm = e->name, *wt = strstr(nm, " – ");
        const char *n1 = wt ? a_kopie_n(fa, nm, (size_t)(wt - nm)) : nm, *n2 = wt ? wt + strlen(" – ") : "";
        { SDL_Surface *eb = e->bild[0] ? kachelbild_id(e->bild, 76) : NULL;   /* NEU 1.3: das vorher geladene Bild der Kachel */
          if (eb) surf_rund(eb, lx + 32 + 16, cy - 24, 48, 48, 11); else bild_kachel11(lx + 32 + 16, cy - 24, 48, 11, NULL, e->kat, n1, 1.f); }
        const char *st = e->zustand == 2 ? T("nicht gelöscht", "not deleted") : e->zustand == 3 ? T("unverändert", "unchanged") : e->zustand == 4 ? T("geschützt", "protected")
                       : sim ? T("geprüft", "checked") : T("gelöscht", "deleted");
        uint32_t sf = e->zustand == 2 || e->zustand == 4 ? S_GOLD : e->zustand == 3 ? S_GRAU : S_GRUEN;
        int sw = textbreite(1, 26, st) + (e->zustand <= 1 ? 36 : 0), sx = lx + lw - 32 - 20 - sw;
        if (e->zustand <= 1) symbol(SYM_HAKEN, sx, cy - 14, 28, sf);
        text(sx + (e->zustand <= 1 ? 36 : 0), cy + 9, 1, 26, sf, st);
        int tx = lx + 32 + 16 + 48 + 16, tb = sx - 20 - tx;
        const char *unter = VERB(fa, n2, n2[0] && e->hat_b ? " · " : "", e->hat_b ? groesse_von(fa, e->b, false) : "");
        if (e->zustand >= 2 && e->grund[0]) unter = e->grund;   /* NEU 1.1: bei "unverändert"/"nicht gelöscht" steht der Grund darunter */
        text(tx, cy - (unter[0] ? 4 : -10), 1, 28, S_TEXT_W, kuerzen_still(1, 28, n1, tb));
        if (unter[0]) text(tx, cy + 28, 0, 24, S_GRAU, kuerzen_still(0, 24, unter, tb));
    }
    if (zeig < en) text(lx + 32 + 16, ly + zeig * (zh + 8) + 30, 0, 24, S_GRAU, a_fmt(fa, T("und %d weitere erledigt", "and %d more done"), en - zeig));
    /* rechts */
    if (!fehler) {
        int h = 220; karte11(rx, y0, kw, h, 20);
        int cx = rx + 24 + 32; ring(cx, y0 + h / 2, 31, 2, S_GRAU3); symbol(SYM_DOKUMENT, cx - 18, y0 + h / 2 - 18, 36, S_SYM);
        int tx = cx + 32 + 24, tb = rx + kw - 24 - tx;
        text(tx, y0 + 52, 3, 28, S_TEXT_W, T("Danach kurz Rückmeldung", "Then a quick feedback"));
        zeilen z = umbrechen(0, 24, T("Nach Neu starten oder Beenden fragt PS5 Cleanup kurz nach – Senden mit einem Knopfdruck, freiwillig. Kein Handy nötig.",
                                      "After Restart or Quit, PS5 Cleanup asks briefly – send with one press, optional. No phone needed."), tb);
        for (int i = 0; i < z.n && i < 4; i++) text(tx, y0 + 94 + i * 34, 0, 24, S_TEXT2, z.z[i]);
        int bh = 130, by2 = unten - bh, by1 = by2 - 24 - bh;
        halte_karte_entwurf(rx, by1, kw, bh, -1.f, false, T("PS5 neu starten", "Restart PS5"), sim ? T("Simulation: wird nur angezeigt", "Simulation: only shown") : T("empfohlen – lädt alles frisch", "recommended – loads everything fresh"));
        { flaeche f = F_NEBEN11(); f.d = 2; f.rand = 0x3B4A63; f.rand_oben = f.rand_unten = 1; flaeche_zeichnen(rx, by2, kw, bh, 24, &f); }
        ring(rx + 24 + 26, by2 + bh / 2, 25, 2, S_GRAU); ring(rx + 24 + 26, by2 + bh / 2, 9, 2, S_TEXT2);
        text_passend(rx + 24 + 52 + 24, gl11(by2, bh, 30), 3, 30, 22, S_TEXT_W, neustart_pflicht ? T("Beenden (startet trotzdem neu)", "Quit (restarts anyway)") : T("Beenden", "Quit"), kw - (24 + 52 + 24) - 24);   /* PS5-Test 17:27: lief über den Rand */
        fussknopf kn[2] = { { KN_X, T("Neu starten", "Restart"), false }, { KN_O, T("Beenden", "Quit"), false } };
        fuss_neu(kn, 2);
    } else {
        int h = 230; karte11(rx, y0, kw, h, 20);
        text_gesperrt(rx + 32, y0 + 32 + 22, 1, 24, S_GRAU2, T("WENN ES WIEDER NICHT KLAPPT", "IF IT FAILS AGAIN"), .14f);
        zeilen z = umbrechen(0, 26, VERB(fa, T("PS5 neu starten und PS5 Cleanup noch einmal laufen lassen.", "Restart the PS5 and run PS5 Cleanup again."),
                                         wo[0] ? VERB(fa, T(" Das Protokoll liegt ", " The log is "), wo, ".") : ""), kw - 64);
        for (int i = 0; i < z.n && i < 4; i++) text(rx + 32, y0 + 32 + 22 + 48 + i * 38, 0, 26, S_TEXT, z.z[i]);
        int y2 = y0 + h + 24, h2 = 230; karte11(rx, y2, kw, h2, 20);
        int cx = rx + 24 + 32; ring(cx, y2 + h2 / 2, 31, 2, S_GRAU3); symbol(SYM_DOKUMENT, cx - 18, y2 + h2 / 2 - 18, 36, S_SYM);
        int tx = cx + 32 + 24, tb = rx + kw - 24 - tx;
        text(tx, y2 + 52, 3, 28, S_TEXT_W, T("Fehler melden", "Report a problem"));
        zeilen z2 = umbrechen(0, 24, T("Mit Internet direkt senden, ohne Internet per QR-Code – wie bei der Rückmeldung. Das Protokoll kommt mit, keine persönlichen Daten.",
                                       "With internet send directly, without per QR code – like the feedback. The log is included, no personal data."), tb);
        for (int i = 0; i < z2.n && i < 4; i++) text(tx, y2 + 94 + i * 34, 0, 24, S_TEXT2, z2.z[i]);
        int bh = 130;
        halte_karte_entwurf(rx, unten - bh, kw, bh, -1.f, false, T("Noch einmal versuchen", "Try again"), T("nur das, was übrig ist", "only what is left"));
        fussknopf kn[3] = { { KN_X, T("Noch einmal versuchen", "Try again"), false }, { KN_DREIECK, T("Fehler melden", "Report a problem"), false }, { KN_O, T("Beenden", "Quit"), false } };
        fuss_neu(kn, 3);
    }
}
static void nochmal_versuchen(void)   /* 5b: die Namen merken, beantworten - nach dem Nachschauen anhaken (siehe Hauptschleife) */
{
    for (int i = 0; i < nochmal_n; i++) free(nochmal_namen[i]);
    free(nochmal_namen); nochmal_namen = NULL; nochmal_n = 0;
    pthread_mutex_lock(&erg_m);
    nochmal_namen = calloc((size_t)erg_n + 1, sizeof(char *));
    for (int i = 0; nochmal_namen && i < erg_n; i++) if (erg[i].zustand == 2 || erg[i].zustand == 3) nochmal_namen[nochmal_n++] = strdup(erg[i].schl);   /* nicht der PS4-Schutz (4) */
    pthread_mutex_unlock(&erg_m);
    nochmal_warten = true;
    nach_aktion = 3; A.schluss_ohne_suche = false;
    antworten(false, false);
}
static void nochmal_anhaken(void)   /* aus der Hauptschleife, wenn nach "Noch einmal versuchen" fertig nachgeschaut ist */
{
    nochmal_warten = false;
    int n = 0;
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i];
        const char *z = erg_schluessel(fa, f);   /* Codex-Prüfung 339 (P1): am Pfad wiedererkennen, nicht am Anzeigenamen */
        for (int j = 0; j < nochmal_n; j++) if (!strcmp(z, nochmal_namen[j]) && !f->Gesperrt) { if (!f->Wahl) eintrag_umschalten(f); n++; break; }
    }
    AUSWAHL_OFFEN = true; fokus_zone = 1; fokus_bereich = 0; AL.k = -1; al_b = 0; al_t = -1; al_py = 0;
    kurz(n == 1 ? T("Der übrige Eintrag ist angehakt - □ löscht ihn noch einmal.", "The remaining entry is ticked - □ deletes it again.")
         : n ? a_fmt(fa, T("%d übrige Einträge sind angehakt - □ löscht sie noch einmal.", "%d remaining entries are ticked - □ deletes them again."), n)
           : T("Nach dem Nachschauen ist nichts mehr davon da.", "After checking again, none of it is left."));
    hinweis_dauer(8000);
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
    hintergrund11();
    /* NEU 1.1 (PS5-Test da_Chris86 17.11.2026): über einer Frage stand "Noch einmal prüfen" (die Überschrift der Bestätigungsseite) - auch
     * bei "PS5 Cleanup jetzt auch entfernen?" nach dem Löschen. Fragen haben eine eigene, neutrale Überschrift. */
    if (auftrag_jetzt != AUF_SUCHEN) seite11_titel = T("Rückfrage", "Question");
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
    /* 1.1 (Stil des Entwurfs): Symbol im Kreis, Überschrift, Text in einer Glaskarte, Knöpfe als Karten */
    uint32_t sf = fehler ? N_ROT : warnung ? S_GOLD : S_TUERKIS;
    int y0 = R11_O + 50 + 24;
    leuchten(R11_X, y0, 76, 76, 38, 24, sf, .3f); kreis(R11_X + 38, y0 + 38, 38, farbmisch(sf, S_HG, .18f)); ring(R11_X + 38, y0 + 38, 37, 2, sf);
    symbol(warnung ? SYM_WARNUNG : SYM_INFO, R11_X + 38 - 19, y0 + 38 - 19, 38, sf);
    text_passend(R11_X + 76 + 28, y0 + 38 + 18, 3, 48, 32, S_TEXT_W, titel, BREITE - 2 * R11_X - 104);
    /* der Text in einer Karte - ↑↓ blättert */
    int kx = R11_X, ky = y0 + 76 + 28, kw = BREITE - 2 * R11_X, kh = 500, pad = 36;
    karte11(kx, ky, kw, kh, 20);
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
        if (dlg_scroll < max_start) { symbol(SYM_RUNTER, kx + kw / 2 - 14, ky + kh - 34, 28, S_GRAU2); }
    }
    /* Knöpfe */
    int byy = ky + kh + 32, bh = 92;
    if (fehler) knopf_breit((BREITE - 520) / 2, byy, 520, bh, true, true, KN_X, "OK");
    else {
        int bw = (kw - 24) / 2;
        bool fs = dlg_fokus == 0, fg = dlg_fokus == 1;
        knopf_breit(kx, byy, bw, bh, fs, fs, KN_O, si);
        /* der andere Knopf: rötlich (Entwurf: Warnfarbe), im Fokus deutlich */
        int xg = kx + bw + 24;
        if (fg) { leuchten(xg, byy, bw, bh, 14, 20, N_ROT, .4f); flaeche f = { false, V2(160, 0xC8423E, .75f, 0x6E1E1C, .8f), 0xFF8A85, 1, 1, 2, .2f, 0xFFC4C0 }; flaeche_zeichnen(xg, byy, bw, bh, 14, &f); }
        else { flaeche f = { false, V2(160, 0x5A1E1C, .5f, 0x2A0D0D, .55f), N_ROT, .6f, .6f, 1, 0, 0 }; flaeche_zeichnen(xg, byy, bw, bh, 14, &f); }
        int tw = textbreite(3, 28, ge), gx = xg + (bw - (42 + 16 + tw)) / 2; if (gx < xg + 20) gx = xg + 20;
        knopf11(KN_X, gx, byy + bh / 2, S_TEXT_W); text(gx + 42 + 16, gl11(byy, bh, 28), 3, 28, S_TEXT_W, kuerzen_still(3, 28, ge, bw - 42 - 56));
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
    char auch[160]; snprintf(auch, sizeof auch, "%s", kern_laufende_gewaehlt(K));
    plattform_schliessen_auch(auch);
    int n = plattform_fremde_schliessen(b, sizeof b);
    if (n == -2) return 0;                                  /* hier nicht vorhanden (PC) */
    ui_log(NULL, T("Schließe Programme im Hintergrund (sonst legen sie ihre Ordner sofort wieder an) ...", "Closing background programs (otherwise they recreate their folders right away) ..."));
    /* NEU 1.1 (Claude 15.11.2026, da_Chris86 07:09 "nach dem Löschen wird ohnehin der Neustart empfohlen"): ein Teil des Jailbreaks ist zum
     * Löschen gewählt und wird darum mit beendet */
    /* 1.3 (da_Chris86 22.11.2026: "der Jailbreak wird ja nicht beendet, es ist nur ein Tool für den Jailbreak"): das Werkzeug beim Namen nennen */
    if (kern_jailbreak_in(auch)) {
        char jb[96] = ""; size_t jl = 0;
        for (const char *z = auch; *z; ) {
            const char *e = strchr(z, ','); size_t l = e ? (size_t)(e - z) : strlen(z);
            char t[32]; snprintf(t, sizeof t, "%.*s,", (int)(l < 24 ? l : 24), z);
            if (l && kern_jailbreak_in(t) && jl + l + 3 < sizeof jb) jl += (size_t)snprintf(jb + jl, sizeof jb - jl, "%s%.*s", jl ? ", " : "", (int)l, z);
            if (!e) break;
            z = e + 1;
        }
        char h[300];
        snprintf(h, sizeof h, T("  Hinweis: Ein Jailbreak-Werkzeug ist zum Löschen gewählt (%s) und wird beendet - was es bereitstellt, fehlt bis zum nächsten Neustart.",
                                "  Note: a jailbreak tool is selected for deletion (%s) and is closed - what it provides is missing until the next restart."), jb);
        ui_log(NULL, h);
    }
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
    if (RM.offen && G.art != D_FERTIG && !G.beschaeftigt) schirm_rueckmeldung();   /* NEU 1.1: Rückmeldung aus den Optionen */
    else if (G.art == D_FERTIG) schirm_fertig();   /* NEU 1.0.8 (4.5): eigene Seite statt Fenster (das alte Fenster gab es bis 1.0.10 nur im Vergleichsmodus der Tests) */
    else if (KB.auto_an && G.art != D_KEINER && !G.beantwortet && (G.art == D_TEXT || G.art == D_WAHL)) schirm_arbeit();   /* 1.1: gleich beantwortet */
    else if (komplett_frage()) schirm_komplett11();             /* NEU 1.1 (Entwurf 3b) */
    else if (bestaetigen_aktiv()) schirm_bestaetigen();        /* NEU 1.0.8 (4.4) */
    else if (G.art == D_WAHL || G.art == D_FEHLER) schirm_frage();   /* NEU 1.0.8 (4.5, K5): Fragen und Fehler als eigene Seite */
    else {   /* (die Seiten oben zeichnen alles selbst - der Hintergrund darunter entfällt) */
        if (G.beschaeftigt) { if (loeschseite) schirm_loeschen(); else if (auftrag_jetzt == AUF_SUCHEN && !nach_loeschen) schirm_start11(); else schirm_arbeit(); }
        else if (GESUCHT && AUSWAHL_OFFEN && ablauf_verbunden(&A)) schirm_liste();
        else schirm_start11();
        if (G.art != D_KEINER) schirm_dialog();   /* altes Fenster nur noch: Texteingabe außer der vollständigen Bereinigung, Fertig im Vergleichsmodus 1.0.6 */
    }
    if (protokoll_offen) schirm_protokoll();   /* NEU 1.0.7: auch über einer Frage / dem Abschluss (Options) */
    if (fern_info_offen && !fern_an()) fern_info_offen = false;   /* NEU 1.0.9: Adresse der Browser-Bedienung mit QR-Code */
    if (fern_info_offen) schirm_fern_info();
}

/* =================================================================== Tasten */
static void tastatur_auf(void)
{
    if (tastatur_offen) return;
    pthread_mutex_lock(&G.m); G.eingabe[0] = 0; pthread_mutex_unlock(&G.m);
    tastatur_offen = true; tastatur_gesehen = false; tastatur_seit = SDL_GetTicks();
    SDL_StartTextInput();
}
static void taste_komplett11(int t)
{
    komplett_vorbereiten();
    int orte[3] = { ORT_INTERN, ORT_M2, ORT_USB }, liste[3], n = 0;
    for (int i = 0; i < 3; i++) if (orte[i] == ORT_INTERN || KB.da[orte[i]]) liste[n++] = orte[i];
    if (t == T_HOCH && KB.fokus > 0) KB.fokus--;
    else if (t == T_RUNTER && KB.fokus < n - 1) KB.fokus++;
    else if (t == T_X) {
        int o = liste[KB.fokus < n ? KB.fokus : 0];
        if (o == ORT_INTERN) kurz(T("Die interne SSD ist immer dabei.", "The internal SSD is always included."));
        else KB.leeren[o] = !KB.leeren[o];
    }
    else if (t == T_DREIECK) {
        if (!eingabe_passt()) tastatur_auf();
        else {
            pthread_mutex_lock(&G.m); snprintf(KB.wort, sizeof KB.wort, "%s", G.eingabe); pthread_mutex_unlock(&G.m);
            KB.auto_an = true; memcpy(KB.auto_leeren, KB.leeren, sizeof KB.leeren);
            kv_da = false; komplett_vorschau_holen(); KB.auto_voll = kv_voll;
            KB.auto_gezeigt = 0; for (int o = ORT_INTERN; o <= ORT_USB; o++) if (o == ORT_INTERN || KB.leeren[o]) KB.auto_gezeigt += kv_wahl[o].bytes;   /* 1.1 (Codex-Prüfung 345): Größen nicht alle bekannt - die Abschlussfrage wird gezeigt */
            pthread_mutex_lock(&G.m); KB.auto_nr = G.dlg_nr; pthread_mutex_unlock(&G.m);   /* die Kette beginnt bei dieser Frage */
            ui_log(NULL, VERB(fa, T("Alles bereinigen: interne SSD", "Clean up everything: internal SSD"), KB.leeren[ORT_M2] ? T(", M.2-SSD", ", M.2 SSD") : "",
                              KB.leeren[ORT_USB] ? T(", erweiterter Speicher", ", extended storage") : "", T(" - LÖSCHEN eingetippt.", " - DELETE typed.")));
            antworten(true, false);   /* "Weiter" */
        }
    }
    else if (t == T_KREIS) antworten(false, false);
}
static struct { bool offen; int fokus; volatile int stand; char ziel[600], grund[300], datei[256], altinfo[1300]; bool schon_da; } UPD;   /* Update-Dialog; stand 0 Frage, 1 lädt, 2 fertig, 3 Fehler */
/* =================================================================== NEU 1.1 (da_Chris86 15.11.2026): Update holen
 * Fragt GitHub nach der neuesten Version (wie der Update-Hinweis), lädt die ELF-Datei und prüft sie (Größe, ELF-Kopf, Kennung von PS5
 * Cleanup, SHA-256 aus GitHubs Angabe). Erst dann wird sie neben die laufende gelegt (eigener Name) bzw. ersetzt die alte ("Ja"). Geht
 * etwas schief, bleibt die alte Version unverändert. Testfassungen (ohne KERN_LOESCHEN) laden nur und schreiben nichts. */
typedef struct { uint32_t h[8]; uint64_t n; uint8_t b[64]; size_t bl; } sha256_t;
static const uint32_t SHA_K[64] = { 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,
    0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,
    0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,
    0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define SHA_R(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(sha256_t *c, const uint8_t *p)
{
    uint32_t w[64], a, b, d, e, f, g, h, cc, t1, t2;
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) w[i] = (SHA_R(w[i - 2], 17) ^ SHA_R(w[i - 2], 19) ^ (w[i - 2] >> 10)) + w[i - 7] + (SHA_R(w[i - 15], 7) ^ SHA_R(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 16];
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3]; e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];
    for (int i = 0; i < 64; i++) {
        t1 = h + (SHA_R(e, 6) ^ SHA_R(e, 11) ^ SHA_R(e, 25)) + ((e & f) ^ (~e & g)) + SHA_K[i] + w[i];
        t2 = (SHA_R(a, 2) ^ SHA_R(a, 13) ^ SHA_R(a, 22)) + ((a & b) ^ (a & cc) ^ (b & cc));
        h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}
static void sha256(const uint8_t *d, size_t n, char hex[65])
{
    sha256_t c = { { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 }, 0, { 0 }, 0 };
    size_t i = 0; for (; i + 64 <= n; i += 64) sha_block(&c, d + i);
    uint8_t b[128] = { 0 }; size_t r = n - i; memcpy(b, d + i, r); b[r] = 0x80;
    size_t tot = r + 1 + 8 <= 64 ? 64 : 128; uint64_t bits = (uint64_t)n * 8;
    for (int k = 0; k < 8; k++) b[tot - 1 - k] = (uint8_t)(bits >> (8 * k));
    sha_block(&c, b); if (tot == 128) sha_block(&c, b + 64);
    for (int k = 0; k < 8; k++) snprintf(hex + 8 * k, 9, "%08x", c.h[k]);
}
static bool speicher_enthaelt(const char *d, size_t n, const char *s, size_t l)   /* memmem gibt es auf der PS5 nicht */
{
    if (!l || l > n) return false;
    for (size_t i = 0; i + l <= n; i++) if (d[i] == s[0] && !memcmp(d + i, s, l)) return true;
    return false;
}
static const char *upd_alte_elf(arena *a)   /* die Programmdatei von PS5 Cleanup, die "Ja, ersetzen" löscht ("" = keine eindeutige) */
{
    /* NEU 1.3: nach dem Nachschauen aus den gefundenen Kopien, sonst aus der Suche ohne Nachschauen - beide nach derselben Regel
     * (app/upd_suche.c upd_alte_waehlen: ps5cleanup_v<laufende Version>.elf, USB zuerst; sonst eine einzige ohne Versionsnummer; nie eine
     * andere Version, nie eine Kopie des Payload Managers). Bis 1.2: ohne Nachschauen keine (die alte blieb liegen, die Meldung sagte
     * nichts dazu); nach dem Nachschauen die erste .elf-Kopie, am liebsten auf USB - auch eine mit anderer Versionsnummer. */
    int n = K->SCHUTZ.eigene.n;
    if (n > 0) {
        const char **l = malloc(sizeof *l * (size_t)n); int w = -1;
        if (l) { for (int i = 0; i < n; i++) l[i] = K->SCHUTZ.eigene.e[i]; w = upd_alte_waehlen(l, n, VERSION_NUMMER); }
        const char *r = a_kopie(a, l && w >= 0 ? l[w] : "");
        free(l);
        return r;
    }
    char p[1024]; pthread_mutex_lock(&upd_m); snprintf(p, sizeof p, "%s", upd_alt_such); pthread_mutex_unlock(&upd_m);
    return a_kopie(a, p);
}
static const char *upd_ort_name(void)
{
    const char *alt = upd_alte_elf(fa);
    if (txt_beginnt(alt, "/mnt/usb")) return T("USB-Laufwerk", "USB drive");
    if (txt_beginnt(alt, "/mnt/ext")) return T("M.2 bzw. erweiterter Speicher", "M.2 or extended storage");
    if (alt[0]) return T("interne SSD", "internal SSD");
    return T("USB-Laufwerk (wenn eins steckt), sonst intern", "USB drive (if one is plugged in), otherwise internal");
}
static bool upd_ersetzen;
static void *upd_lade_faden(void *x)
{
    (void)x;
    char g[400] = "", *json = NULL, *elf = NULL; size_t jl = 0, el = 0;
    arena *a = arena_neu();
    const char *fehler = NULL, *ziel = "";
    UPD.altinfo[0] = 0;
    if (!plattform_holen("https://api.github.com/repos/xXBneutzerNameXx/PS5-Cleanup/releases/latest", VERSION_NUMMER, "application/vnd.github+json", &json, &jl, 512 * 1024, g, sizeof g))
        fehler = VERB(a, T("GitHub nicht erreichbar", "GitHub not reachable"), g[0] ? VERB(a, " (", g, ")") : "");
    const char *url = NULL, *digest = NULL, *name = NULL; long long groesse = -1;
    cJSON *j = fehler ? NULL : cJSON_ParseWithLength(json, jl);
    if (!fehler) {
        const cJSON *as = j ? cJSON_GetObjectItemCaseSensitive(j, "assets") : NULL, *e;
        cJSON_ArrayForEach(e, as) {
            const cJSON *n = cJSON_GetObjectItemCaseSensitive(e, "name"), *u = cJSON_GetObjectItemCaseSensitive(e, "browser_download_url");
            if (!cJSON_IsString(n) || !cJSON_IsString(u)) continue;
            const char *nn = n->valuestring; size_t l = strlen(nn);
            if (l < 5 || strcasecmp(nn + l - 4, ".elf") || !strcasestr(nn, "cleanup") || strcasestr(nn, "test") || strcasestr(nn, "simul")) continue;
            name = a_kopie(a, nn); url = a_kopie(a, u->valuestring);
            const cJSON *d = cJSON_GetObjectItemCaseSensitive(e, "digest"), *s = cJSON_GetObjectItemCaseSensitive(e, "size");
            if (cJSON_IsString(d) && txt_beginnt(d->valuestring, "sha256:")) digest = a_kopie(a, d->valuestring + 7);
            if (cJSON_IsNumber(s)) groesse = (long long)s->valuedouble;
            break;
        }
        if (!url) fehler = T("In der neuesten Version ist keine ELF-Datei zu finden.", "No ELF file found in the latest release.");
    }
    if (j) cJSON_Delete(j);
    free(json);
    if (!fehler && !plattform_holen(url, VERSION_NUMMER, "application/octet-stream", &elf, &el, 64u * 1024 * 1024, g, sizeof g))
        fehler = VERB(a, T("Herunterladen ging nicht", "Download failed"), g[0] ? VERB(a, " (", g, ")") : "");
    if (!fehler) {   /* prüfen */
        char hex[65];
        if (el < 1024 || memcmp(elf, "\x7f" "ELF", 4)) fehler = T("Die Datei ist keine ELF-Datei.", "The file is not an ELF file.");
        else if (!digest || groesse < 0) fehler = T("Zur Datei fehlen Prüfsumme oder Größe - ohne sie wird nichts abgelegt.", "Checksum or size missing for the file - nothing is saved without them.");   /* Codex-Prüfung 339 (P2) */
        else if ((long long)el != groesse) fehler = T("Die Datei ist unvollständig.", "The file is incomplete.");
        else if (!speicher_enthaelt(elf, el, KERN_KENNUNG, strlen(KERN_KENNUNG))) fehler = T("Die Datei ist nicht PS5 Cleanup.", "The file is not PS5 Cleanup.");
        else { sha256((const uint8_t *)elf, el, hex); if (strlen(digest) != 64 || strcasecmp(hex, digest)) fehler = T("Prüfsumme stimmt nicht - die Datei ist beschädigt.", "Checksum mismatch - the file is damaged."); }
    }
    if (!fehler) {   /* ablegen */
        if (K->SCHUTZ.eigene.n == 0) upd_alt_suchen();   /* NEU 1.3: frisch suchen - seit der Update-Prüfung kann sich etwas geändert haben */
        const char *alt = upd_alte_elf(a), *ordner;
        bool ersetzen = upd_ersetzen && alt[0];   /* NEU 1.3: ohne eindeutige alte Datei wird nie etwas gelöscht */
        if (alt[0]) ordner = a_kopie_n(a, alt, (size_t)(strrchr(alt, '/') - alt));
        else { struct stat st; ordner = stat("/mnt/usb0", &st) == 0 ? "/mnt/usb0" : plattform_eigener_ort(); }
        char g2[300] = "", wo[1024] = ""; bool da = false, weg = false;
        if (!plattform_update_ablegen(ordner, name, elf, el, alt, ersetzen, wo, sizeof wo, &da, &weg, g2, sizeof g2)) fehler = a_kopie(a, g2);
        else {   /* NEU 1.1 (PS5-Test 15.11.2026): der wirkliche Name (auch _2 ..) und ob sie schon da lag */
            ziel = VERB(a, wo, g2[0] ? VERB(a, " (", g2, ")") : "");
            UPD.schon_da = da;
            const char *sl = strrchr(wo, '/'); snprintf(UPD.datei, sizeof UPD.datei, "%s", sl ? sl + 1 : wo);
            /* NEU 1.3: ehrlich sagen, was mit der alten Version ist - "gelöscht" nur, wenn das Löschen wirklich geklappt hat */
            if (weg) snprintf(UPD.altinfo, sizeof UPD.altinfo, T(" Die alte Version ist gelöscht: %s.", " The old version has been deleted: %s."), alt);
            else if (alt[0] && strcmp(alt, wo)) snprintf(UPD.altinfo, sizeof UPD.altinfo, T(" Die alte Version bleibt daneben: %s.", " The old version stays next to it: %s."), alt);
            else snprintf(UPD.altinfo, sizeof UPD.altinfo, "%s", T(" Die bisherige Programmdatei hat PS5 Cleanup nicht sicher gefunden - liegt noch eine alte Version da, bitte selbst löschen.",
                                                                  " PS5 Cleanup could not reliably find its current program file - if an old version is still there, please delete it yourself."));
        }
    }
    free(elf);
    snprintf(UPD.ziel, sizeof UPD.ziel, "%s", ziel); snprintf(UPD.grund, sizeof UPD.grund, "%s", fehler ? fehler : "");
    { char z[2300]; snprintf(z, sizeof z, "Update holen: %s%s%s%s", fehler ? "nicht geklappt - " : "fertig - ", fehler ? fehler : ziel, !fehler && UPD.schon_da ? " (lag schon da, nichts doppelt gespeichert)" : "", fehler ? "" : UPD.altinfo); plattform_protokoll_zeile(z); }
    arena_frei(a);
    __atomic_store_n(&UPD.stand, fehler ? 3 : 2, __ATOMIC_RELEASE);
    neu_zeichnen();
    return NULL;
}
static bool satzende(const char *t) { size_t l = t ? strlen(t) : 0; return l && strchr(".!?", t[l - 1]); }
/* NEU 1.1: graue Hinweiszeile unter den Knöpfen, rechts "○ <was Kreis tut>" */
static void upd_hinweiszeile(int x, int y, int w, const char *hinweis, const char *kreis)
{
    int kw2 = textbreite(0, 24, kreis), rx = x + w - kw2, gx = rx - 12 - 42;
    knopf11(KN_O, gx, y - 9, S_GRAU);
    text(rx, y, 0, 24, S_GRAU, kreis);
    if (hinweis[0]) text(x, y, 0, 24, S_GRAU, kuerzen_still(0, 24, hinweis, gx - 24 - x));
}
static void upd_dialog11(void)
{
    abdunkeln();
    char un[32]; upd_holen(un, sizeof un);
    bool ab = upd_alte_elf(fa)[0] != 0;   /* NEU 1.3: alte Programmdatei eindeutig bekannt - nur dann gibt es "Ja, ersetzen" */
    int w = 1080, h = 654, x = (BREITE - w) / 2, y = (HOEHE - h) / 2, st = __atomic_load_n(&UPD.stand, __ATOMIC_ACQUIRE);
    gold11(x, y, w, h, 24, 2);
    { flaeche f = { true, V2(180, 0x1E2A38, .8f, 0x141D28, .85f), 0, 0, 0, 0, 0, 0 }; flaeche_zeichnen(x + 2, y + 2, w - 4, h - 4, 22, &f); }
    int cx = x + 48 + 38, cy = y + 48 + 38;
    kreis(cx, cy, 38, 0x3A2A08); ring(cx, cy, 37, 2, S_GOLD); lade_symbol(cx - 18, cy - 18, 36, S_GOLD);
    text(cx + 38 + 22, cy - 2, 3, 44, S_TEXT_W, a_fmt(fa, T("Update auf Version %s", "Update to version %s"), un));
    text(cx + 38 + 22, cy + 40, 0, 28, S_TEXT2, st == 1 ? T("Wird geladen und geprüft …", "Downloading and checking …") : st == 2 ? T("Fertig.", "Done.") : st == 3 ? T("Hat nicht geklappt.", "Did not work.")
                                                     : ab ? VERB(fa, T("Soll die alte Version ", "Replace the old version "), VERSION_NUMMER, T(" ersetzt werden?", "?"))
                                                          : T("Die neue Version kommt neben die alte.", "The new version is placed next to the old one."));
    int bx = x + 48, by = y + 160, bw = w - 96, bh = 276;
    { flaeche f = { false, V1(0xFFFFFF, .05f), 0, 0, 0, 0, 0, 0 }; flaeche_zeichnen(bx, by, bw, bh, 16, &f); }
    if (st == 0) {
        const char *z1 = VERB(fa, upd_ort_name(), T(" – dort, wo PS5 Cleanup jetzt liegt", " – where PS5 Cleanup is now"));
        const char *k[3] = { T("Ort: ", "Location: "), T("Ersetzen: ", "Replace: "), T("Zusätzlich: ", "Additionally: ") };
        const char *v[3] = { z1, T("Die neue Version wird geladen und geprüft, erst dann wird die alte gelöscht.", "The new version is downloaded and checked, only then is the old one deleted."),
                             T("Die neue Version kommt als eigene Datei daneben, beide bleiben.", "The new version is placed next to it as its own file, both stay.") };
        if (!ab) {   /* NEU 1.3: alte Programmdatei nicht sicher gefunden - nichts versprechen, was nicht passiert */
            k[1] = T("Alte Version: ", "Old version: "); v[1] = T("PS5 Cleanup hat seine bisherige Programmdatei nicht sicher gefunden - darum wird nichts gelöscht.", "PS5 Cleanup could not reliably find its current program file - so nothing is deleted.");
            k[2] = T("Danach: ", "Afterwards: "); v[2] = T("Die neue Version kommt als eigene Datei daneben. Die alte bitte selbst löschen.", "The new version is placed next to it as its own file. Please delete the old one yourself.");
        }
        int yy = by + 24;
        for (int i = 0; i < 3; i++) {
            int kw2 = textbreite(1, 26, k[i]); text(bx + 24, yy + 26, 1, 26, S_TEXT_W, k[i]);
            zeilen z = umbrechen(0, 26, v[i], bw - 48 - kw2); for (int q = 0; q < z.n && q < 2; q++) text(bx + 24 + (q ? 0 : kw2), yy + 26 + q * 38, 0, 26, S_TEXT, z.z[q]);
            yy += (z.n > 1 ? 2 : 1) * 38 + 10;
        }
    } else {
        const char *t = st == 1 ? T("Bitte warten – gleich fertig.", "Please wait – almost done.") : st == 2 ? VERB(fa, UPD.schon_da ? T("Die neue Version lag schon da: ", "The new version was already there: ") : T("Abgelegt: ", "Saved: "), UPD.ziel,
                                                                                                         T(". Zum Starten künftig diese Datei nehmen: ", ". From now on, start this file: "), UPD.datei, ".", UPD.altinfo)
                                                                                                : VERB(fa, UPD.grund, satzende(UPD.grund) ? "" : ".", T(" Die alte Version bleibt unverändert.", " The old version stays unchanged."));
        zeilen z = umbrechen(0, 26, t, bw - 48); for (int q = 0; q < z.n && q < 5; q++) text(bx + 24, by + 24 + 26 + q * 38, 0, 26, st == 3 ? S_GOLD_T : S_TEXT, z.z[q]);
        if (st == 1) { ring(bx + bw - 60, by + bh / 2, 26, 5, 0x2A3644); ring_teil(bx + bw - 60, by + bh / 2, 26, 5, (float)(SDL_GetTicks() % 1200) / 1200.f, S_TUERKIS); lauf_aktiv = true; }
    }
    int ky = by + bh + 24, kh = 100, kw = (bw - 18) / 2;
    if (st == 0) {
        /* NEU 1.1 (PS5-Test 15.11.2026): Blau und ✕ folgen dem Fokus, beide mit ✕ bestätigen; ○ = zurück, nichts tun */
        if (ab) {
            knopf_breit2(bx, ky, kw, kh, UPD.fokus == 0, UPD.fokus == 0, KN_X, T("Ja, ersetzen", "Yes, replace"), T("empfohlen", "recommended"));
            knopf_breit2(bx + kw + 18, ky, kw, kh, UPD.fokus == 1, UPD.fokus == 1, KN_X, T("Nein, zusätzlich ablegen", "No, keep both"),
                         a_fmt(fa, T("ps5cleanup_v%s.elf daneben", "ps5cleanup_v%s.elf next to it"), un));
        } else knopf_breit2(bx, ky, bw, kh, true, true, KN_X, T("Update holen", "Get update"), a_fmt(fa, T("ps5cleanup_v%s.elf kommt daneben", "ps5cleanup_v%s.elf goes next to it"), un));   /* NEU 1.3 */
        upd_hinweiszeile(bx, ky + kh + 40, bw, T("Geht beim Laden etwas schief, bleibt die alte Version unverändert.", "If the download fails, the old version stays unchanged."), T("zurück", "back"));
    } else if (st == 2) {   /* NEU 1.1 (PS5-Test 15.11.2026): gleich beenden, um mit der neuen Version weiterzumachen */
        knopf_breit2(bx, ky, kw, kh, UPD.fokus == 0, UPD.fokus == 0, KN_X, T("PS5 Cleanup beenden", "Quit PS5 Cleanup"), T("dann die neue Datei starten", "then start the new file"));
        knopf_breit2(bx + kw + 18, ky, kw, kh, UPD.fokus == 1, UPD.fokus == 1, KN_X, T("Weiter mit dieser Version", "Continue with this version"), VERB(fa, T("Version ", "Version "), VERSION_NUMMER));
        upd_hinweiszeile(bx, ky + kh + 40, bw, "", T("weiter mit dieser Version", "continue with this version"));
    } else if (st == 3) knopf_breit(bx, ky, bw, kh, true, true, KN_X, T("Schließen", "Close"));
}
static void taste_upd11(int t)
{
    int st = __atomic_load_n(&UPD.stand, __ATOMIC_ACQUIRE);
    if (st == 1) return;
    if (st == 3) { if (t == T_X || t == T_KREIS) UPD.offen = false; return; }
    if (t == T_LINKS) { UPD.fokus = 0; return; } else if (t == T_RECHTS) { UPD.fokus = 1; return; }
    if (t == T_KREIS) { UPD.offen = false; return; }   /* NEU 1.1 (PS5-Test 15.11.2026): ○ immer = zurück, nichts tun */
    if (t != T_X) return;
    if (st == 2) {   /* ✕ auf "PS5 Cleanup beenden" bzw. "Weiter mit dieser Version" */
        UPD.offen = false;
        if (UPD.fokus == 0) {
            if (G.beschaeftigt || A.loescht || G.art != D_KEINER) { kurz(T("Bitte warten, bis die laufende Arbeit fertig ist.", "Please wait until the current task has finished.")); return; }
            ui_log(NULL, T("PS5 Cleanup beendet sich nach dem Update - beim nächsten Mal die neue Datei starten.", "PS5 Cleanup quits after the update - start the new file next time."));
            laeuft = false;
        }
        return;
    }
    {
        upd_ersetzen = UPD.fokus == 0 && upd_alte_elf(fa)[0];   /* NEU 1.3: ohne eindeutige alte Datei nie "ersetzen" (es gibt dann nur "Update holen") */
        UPD.fokus = 0;   /* nach dem Laden steht der Fokus auf "PS5 Cleanup beenden" */
        UPD.stand = 1;
        pthread_t th; if (basis_faden(&th, upd_lade_faden, NULL) == 0) pthread_detach(th); else { UPD.stand = 3; snprintf(UPD.grund, sizeof UPD.grund, "%s", T("Kein Faden frei.", "No thread available.")); }
    }
}
/* =================================================================== NEU 1.1 (Entwurf "Optionen"): ein Menü für Startseite und Auswahl
 * rechts als Tafel: Sprache, Testmodus, (Nochmal nachschauen / Reihenfolge / Ordner), Bedienung im Browser, Protokoll speichern,
 * Rückmeldung geben; darunter Info mit Version und - wenn es eins gibt - "Update holen" (△). */
enum { OP_SPRACHE, OP_TEST, OP_NOCHMAL, OP_SORT, OP_ORDNER, OP_BROWSER, OP_PROT, OP_RM };
static int opt_ids11(int *ids)
{
    int n = 0; bool liste = GESUCHT && AUSWAHL_OFFEN && ablauf_verbunden(&A);
    ids[n++] = OP_SPRACHE; ids[n++] = OP_TEST;
    if (!liste && GESUCHT && ablauf_verbunden(&A)) ids[n++] = OP_NOCHMAL;
    if (liste) { ids[n++] = OP_SORT; ids[n++] = OP_ORDNER; }
    ids[n++] = OP_BROWSER; ids[n++] = OP_PROT; ids[n++] = OP_RM;
    return n;
}
static void optionen11(void)
{
    abdunkeln();
    int ids[10], n = opt_ids11(ids);
    if (opt_fokus >= n) opt_fokus = n - 1;
    int x = 1080, w = BREITE - R11_X - x, y = R11_O, h = HOEHE - 48 - y;
    karte11(x, y, w, h, 24);
    { flaeche r = { false, V1(0, 0), 0x2A4A7A, 1, 1, 1, 0, 0 }; flaeche_zeichnen(x, y, w, h, 24, &r); }
    ring(x + 34 + 20, y + 54, 19, 2, S_TEXT2); symbol(SYM_KN_OPTIONEN, x + 34 + 8, y + 54 - 12, 24, S_TEXT2);
    text(x + 34 + 40 + 20, y + 54 + 16, 3, 40, S_TEXT_W, T("Optionen", "Options"));
    char un[32]; upd_holen(un, sizeof un);
    int fy = y + h - 34 - 22, ry = y + 100, rg = 10;
    bool voll = un[0] && (fy - 34 - 256 - 20 - ry - (n - 1) * rg) / n >= 80;   /* passt die große Update-Karte noch? */
    int ih = !un[0] ? 110 : voll ? 256 : 170, iy = fy - 34 - ih;
    int rh = (iy - 20 - ry - (n - 1) * rg) / n; if (rh > 96) rh = 96;
    for (int i = 0; i < n; i++) {
        int yy = ry + i * (rh + rg);
        if (opt_fokus == i) fokus11(x + 32, yy, w - 64, rh, 14, 2); else { flaeche f = F_FELD11(); flaeche_zeichnen(x + 32, yy, w - 64, rh, 14, &f); }
        const char *t1 = "", *t2 = "";
        char a[64];
        switch (ids[i]) {
        case OP_SPRACHE: t1 = T("Sprache", "Language"); t2 = SP_EN ? "English" : "Deutsch"; break;
        case OP_TEST: t1 = T("Testmodus", "Test mode");
#ifdef NUR_LESEN
            t2 = T("diese Testversion schaut immer nur nach", "this test version only ever looks");
#else
            t2 = testmodus ? T("an – es wird nur angeschaut", "on – only looking") : T("aus – Löschen ist möglich", "off – deleting is possible");
#endif
            break;
        case OP_NOCHMAL: t1 = T("Nochmal nachschauen", "Scan again"); t2 = T("sucht alles noch einmal", "searches everything again"); break;
        case OP_SORT: t1 = T("Reihenfolge", "Order"); t2 = sortierung == SORT_AZ ? "A–Z" : T("größte zuerst", "largest first"); break;
        case OP_ORDNER: t1 = T("Ordner anzeigen", "Show folders"); t2 = ordner_zeigen ? T("an – im Kasten rechts", "on – in the box on the right") : T("aus", "off"); break;
        case OP_BROWSER: t1 = T("Bedienung im Browser", "Browser control"); if (fern_an()) { fern_adresse(a, sizeof a); t2 = VERB(fa, T("an · ", "on · "), txt_beginnt(a, "http://") ? a + 7 : a); } else t2 = T("nicht verfügbar", "not available"); break;
        case OP_PROT: t1 = T("Protokoll speichern", "Save log"); t2 = T("auf das USB-Laufwerk · □ ansehen", "to the USB drive · □ view"); break;
        default: t1 = T("Rückmeldung geben", "Give feedback"); t2 = T("Direkt senden · ohne Internet per QR-Code", "Send directly · without internet via QR code"); break;
        }
        bool zwei = rh >= 76;
        text(x + 32 + 24, zwei ? yy + rh / 2 - 4 : gl11(yy, rh, 28), 3, 28, S_TEXT_W, t1);
        if (zwei) text_passend(x + 32 + 24, yy + rh / 2 + 28, 0, 24, 20, S_TEXT2, t2, w - 64 - 48 - 40);
        else text_rechts(x + w - 32 - 56, gl11(yy, rh, 24), 0, 24, S_TEXT2, kuerzen_still(0, 24, t2, w / 2 - 60));
        symbol(SYM_RECHTS, x + w - 32 - 24 - 22, yy + rh / 2 - 11, 22, S_TEXT2);
    }
    /* Info */
    if (un[0]) gold11(x + 32, iy, w - 64, ih, 18, 2); else { flaeche f = F_FELD11(); flaeche_zeichnen(x + 32, iy, w - 64, ih, 18, &f); }
    symbol(SYM_INFO, x + 32 + 24, iy + 36, 34, un[0] ? S_GOLD : S_GRAU);
    text(x + 32 + 24 + 34 + 22, iy + 52, 3, 28, S_TEXT_W, "Info");
    text(x + 32 + 24 + 34 + 22, iy + 86, 0, 24, S_TEXT2, VERB(fa, "Version ", VERSION_NUMMER, T(" · von da_Chris86", " · by da_Chris86")));
    if (un[0] && !voll) {   /* kompakt: nur der Hinweis auf △ */
        const char *bt = VERB(fa, T("Neues Update: ", "New update: "), un); int bw2 = textbreite(1, 24, bt) + 32, bx2 = x + w - 32 - 24 - bw2;
        rund(bx2, iy + 18, bw2, 40, 20, S_GOLD); text(bx2 + 16, gl11(iy + 18, 40, 24), 1, 24, 0x241A05, bt);   /* NEU 1.1 (PS5-Test 15.11.2026): über der Versionszeile, nicht darauf */
        lade_symbol(x + 32 + 24 + 2, iy + 112, 28, S_GOLD);
        text_passend(x + 32 + 24 + 46, iy + 136, 0, 24, 20, S_TEXT2, VERB(fa, T("△ Update holen – kommt dorthin, wo PS5 Cleanup liegt: ", "△ Get update – goes where PS5 Cleanup is: "), upd_ort_name()), w - 64 - 48 - 46);
    }
    if (un[0] && voll) {
        const char *bt = VERB(fa, T("Neues Update: ", "New update: "), un); int bw2 = textbreite(1, 24, bt) + 32, bx2 = x + w - 32 - 24 - bw2;
        rund(bx2, iy + 18, bw2, 40, 20, S_GOLD); text(bx2 + 16, gl11(iy + 18, 40, 24), 1, 24, 0x241A05, bt);   /* NEU 1.1 (PS5-Test 15.11.2026): über der Versionszeile, nicht darauf */
        int ux = x + 32 + 24, uy = iy + 120, uw = w - 64 - 48, uh = ih - 120 - 24;
        { flaeche f = { false, V1(0xFFFFFF, .05f), 0, 0, 0, 0, 0, 0 }; flaeche_zeichnen(ux, uy, uw, uh, 14, &f); }
        lade_symbol(ux + 20, uy + uh / 2 - 14, 28, S_GOLD);
        text(ux + 20 + 28 + 18, uy + 40, 3, 28, S_TEXT_W, T("Update holen", "Get update"));
        text_passend(ux + 20 + 28 + 18, uy + 76, 0, 24, 20, S_TEXT2, VERB(fa, T("Kommt dorthin, wo PS5 Cleanup jetzt liegt: ", "Goes where PS5 Cleanup is now: "), upd_ort_name()), uw - 20 - 28 - 18 - 80);
        int kx = ux + uw - 20 - 52; flaeche k = { false, V1(0, 0), S_GOLD, 1, 1, 2, 0, 0 }; flaeche_zeichnen(kx, uy + uh / 2 - 21, 52, 42, 10, &k);
        symbol(SYM_KN_DREIECK, kx + 14, uy + uh / 2 - 12, 24, S_GOLD);
    }
    /* Fußleiste der Tafel */
    int kx = x + 32, cy = fy;
    knopf11(KN_X, kx, cy, S_TEXT2); kx += 42 + 12; text(kx, gl11(cy - 22, 44, 24), 0, 24, S_TEXT2, T("Öffnen", "Open")); kx += textbreite(0, 24, T("Öffnen", "Open")) + 32;
    knopf11(KN_O, kx, cy, S_TEXT2); kx += 42 + 12; text(kx, gl11(cy - 22, 44, 24), 0, 24, S_TEXT2, T("Schließen", "Close")); kx += textbreite(0, 24, T("Schließen", "Close")) + 32;
    if (un[0]) { knopf11(KN_DREIECK, kx, cy, S_TEXT2); kx += 42 + 12; text(kx, gl11(cy - 22, 44, 24), 0, 24, S_TEXT2, T("Update holen", "Get update")); }
    if (UPD.offen) upd_dialog11();
}
static void taste_optionen11(int t)
{
    if (UPD.offen) { taste_upd11(t); return; }
    int ids[10], n = opt_ids11(ids);
    if (t == T_HOCH && opt_fokus > 0) opt_fokus--;
    else if (t == T_RUNTER && opt_fokus < n - 1) opt_fokus++;
    else if (t == T_KREIS || t == T_OPTIONS) optionen_offen = false;
    else if (t == T_DREIECK) { char un[32]; upd_holen(un, sizeof un); if (un[0]) { UPD.offen = true; UPD.fokus = 0; UPD.stand = 0; } }
    else if (t == T_QUADRAT && ids[opt_fokus < n ? opt_fokus : 0] == OP_PROT) { optionen_offen = false; protokoll_offen = true; prot_scroll = 1 << 20; }
    else if (t == T_X) switch (ids[opt_fokus < n ? opt_fokus : 0]) {
        case OP_SPRACHE: sprache_wechseln(); break;   /* NEU 1.1: auch nach dem Nachschauen (schaut neu nach) */
        case OP_TEST: testmodus_aus_menue(); break;
        case OP_NOCHMAL: optionen_offen = false; starte(AUF_SUCHEN); break;
        case OP_SORT: sortierung = sortierung == SORT_AZ ? SORT_GROESSTE : SORT_AZ; al_py = 0; AL.k = -1; break;
        case OP_ORDNER: ordner_zeigen = !ordner_zeigen; break;
        case OP_BROWSER: fern_info_zeigen(); break;
        case OP_PROT: protokoll_auf_stick(prot_meldung, sizeof prot_meldung); kurz(prot_meldung); hinweis_dauer(6000); break;
        default: optionen_offen = false; rm_oeffnen(0, 0); break;
    }
}
static void taste_dialog(int t)
{
    dialogart art = G.art;
    if (G.dlg_nr != dlg_gesehen || SDL_GetTicks() - dlg_seit < 400) return;   /* Frage gerade erst erschienen */
    if (art == D_FERTIG) {   /* NEU 1.0.6 */
        /* da_Chris86 02.11.2026: ✕ = Neu starten, ○ = Beenden */
        if (G.beantwortet) return;   /* Codex-Prüfung 208: schon beantwortet - die erste Wahl gilt */
        /* NEU 1.1 (Entwurf 5/5b/6): erst die Rückmeldung, dann Neu starten / Beenden (rm_aktion_ausfuehren) */
        if (RM.offen) { taste_rueckmeldung(t); return; }
        if (fertig_fehler()) {
            if (t == T_X) nochmal_versuchen();
            else if (t == T_DREIECK) rm_oeffnen(0, 1);
            else if (t == T_KREIS) rm_oeffnen(1, 1);
        } else {
            if (t == T_X) rm_oeffnen(2, 0);
            else if (t == T_KREIS) rm_oeffnen(1, 0);
        }
        return;
    }
    if (komplett_frage()) { taste_komplett11(t); return; }   /* NEU 1.1 (Entwurf 3b) */
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
        /* 1.1 (Entwurf "3 – Bestätigen"): nur die Halte-Karte - ✕ halten löscht, ○ zurück zur Auswahl; ein kurzes ✕ erklärt das Halten */
        if (t == T_KREIS) antworten(false, false);
        else if (t == T_X) { kurz(T("Zum Löschen ✕ gedrückt halten, bis der Kreis voll ist.", "To delete, hold ✕ until the circle is full.")); hinweis_dauer(4000); }
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
    if (optionen_offen) { taste_optionen11(t); return; }   /* NEU 1.1: Tafel "Optionen" (Entwurf) */
    /* NEU 1.1 (Entwurf "2 – Auswählen", da_Chris86 15.11.2026): Bereiche sind Reiter oben - L1/R1 (auch ←/→) wechseln den Bereich,
     * L2/R2 blättern seitenweise, △ setzt bzw. nimmt alle Haken des Bereichs, ○ geht zurück zur Übersicht. "Alles bereinigen" steht
     * jetzt auf der Übersicht (△ dort). */
    fokus_zone = 1;
    switch (t) {
    case T_HOCH: al_schritt(-1); break;
    case T_RUNTER: al_schritt(1); break;
    case T_RECHTS: if (AL.n > 0 && al_mehr(al_b) && al_t < 0) al_t = 0; break;   /* Entwurf: → Teile zeigen */
    case T_LINKS: if (al_t >= 0) al_t = -1; break;
    case T_L1: case T_R1: {
        int nb = fokus_bereich + (t == T_R1 ? 1 : -1);
        if (nb >= 0 && nb < sicht_n && nb != fokus_bereich) { fokus_bereich = nb; AL.k = -1; al_b = 0; al_t = -1; al_py = 0; }
        break;
    }
    case T_L2: case T_R2:   /* seitenweise: sechs Blöcke weiter (bis 1.0.10 auf L1/R1) */
        if (AL.n > 0) { al_b += t == T_R2 ? 6 : -6; if (al_b < 0) al_b = 0; if (al_b > AL.n - 1) al_b = AL.n - 1; al_t = -1; }
        break;
    case T_KREIS: AUSWAHL_OFFEN = false; optionen_offen = false; hinweis_bis = 0; break;   /* zurück zur Übersicht: Hinweise der Liste (z. B. Jailbreak-Warnung) gehören nicht dorthin */
    case T_DREIECK: {   /* alle an/aus im Bereich - wie bisher ✕ auf dem Bereich links */
        int n = bereich_anzahl(k);
        if (n > 0 && ablauf_bereich_nur_einzeln(&A, k) && ablauf_bereich_zustand(&A, k) == 0)
            kurz(T("Hier nur einzeln wählbar: die Einträge mit ✕ einzeln anhaken.", "Individually only here: tick the entries one by one with ✕."));
        else if (n > 0) ablauf_bereich_setzen(&A, k, ablauf_bereich_zustand(&A, k) != 2);
        break;
    }
    case T_X:
        if (AL.n > 0) {
            if (al_t >= 0 && al_mehr(al_b)) eintrag_umschalten(AL.bl[al_b].teile.e[al_t]);
            else al_block_setzen(al_b);
        }
        break;
#ifdef NUR_LESEN
    case T_QUADRAT: kurz(T("In dieser Testversion gesperrt - es wird nur nachgeschaut.", "Locked in this test version - only looking.")); break;
#else
    case T_QUADRAT:
        if (testmodus) { kurz(testmodus_hinweis()); hinweis_dauer(6000); break; }   /* NEU 1.0.8 (3.6) */
        if (gewaehlt_anzahl() > 0) { dlg_fokus = 0; dlg_scroll = 0; starte(AUF_AUSWAHL); } else kurz(T("Erst etwas auswählen (✕).", "Select something first (✕)."));
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
    {   /* NEU 1.3: gedrückte Taste kurz aufleuchten lassen */
        int k = -1; leucht_lr = -1;
        switch (t) {
        case T_X: k = KN_X; break; case T_KREIS: k = KN_O; break; case T_DREIECK: k = KN_DREIECK; break; case T_QUADRAT: k = KN_QUADRAT; break;
        case T_OPTIONS: k = KN_OPT; break; case T_HOCH: case T_RUNTER: k = KN_HOCHRUNTER; break; case T_LINKS: case T_RECHTS: k = KN_LINKSRECHTS; break;
        case T_L1: k = KN_L1R1; leucht_lr = 0; break; case T_R1: k = KN_R1; leucht_lr = 1; break;
        default: break;
        }
        if (k >= 0) { leucht_k = (knopfart)k; leucht_bis = SDL_GetTicks() + 250; if (!leucht_bis) leucht_bis = 1; }
        /* PS5-Test 18.11.2026 18:12: ✕/△ auf den Wege-Kacheln wechseln sofort die Seite - das Aufleuchten war nie zu sehen. Bei den
         * Aktionstasten erst ein Bild mit der leuchtenden Taste, kurz stehen lassen, dann die Aktion (Steuerkreuz ohne Pause) */
        if (k >= 0 && (t == T_X || t == T_KREIS || t == T_DREIECK || t == T_QUADRAT) && !leucht_ohne_pause && fenster) {
            zeichnen(); SDL_UpdateWindowSurface(fenster); bild_zeit = SDL_GetTicks(); SDL_Delay(110);
        }
    }
    if (!mess_taste_ab) mess_taste_ab = mess_ms();   /* NEU 1.3: Zeit bis zum nächsten Bild */
    neu_zeichnen();
#ifdef MITSCHNITT
    ms_taste();                                    /* NEU 1.0.8 (2.7): nach dem Tastendruck ein Bild */
#endif
    if (protokoll_offen) {
        if (t == T_KREIS || t == T_OPTIONS) protokoll_offen = false;
        else if (t == T_HOCH) prot_scroll -= 3; else if (t == T_RUNTER) prot_scroll += 3;
        else if (t == T_L1 || t == T_L2) prot_scroll -= 30; else if (t == T_R1 || t == T_R2) prot_scroll += 30;
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
    if (RM.offen && G.art == D_KEINER) { taste_rueckmeldung(t); return; }   /* NEU 1.1: Rückmeldung aus den Optionen */
    bool fertig = GESUCHT && ablauf_verbunden(&A);
    if (fertig && AUSWAHL_OFFEN) { taste_liste(t); return; }
    if (optionen_offen) { taste_optionen11(t); return; }   /* NEU 1.1: Tafel "Optionen" (Entwurf) */
    /* Start / Übersicht */
    if (t == T_X) { if (fertig) { AUSWAHL_OFFEN = true; fokus_zone = 1; fokus_bereich = 0; AL.k = -1; al_b = 0; al_t = -1; al_py = 0; optionen_offen = false; } else starte(AUF_SUCHEN); }
    else if (t == T_DREIECK && fertig) {   /* NEU 1.1 (da_Chris86 15.11.2026): △ = Alles bereinigen; "Nochmal nachschauen" steht in den Optionen */
#ifdef NUR_LESEN
        kurz(T("In dieser Testversion gesperrt - es wird nur nachgeschaut.", "Locked in this test version - only looking."));
#else
        if (testmodus) { kurz(testmodus_hinweis()); hinweis_dauer(6000); }
        else if (K->Funde.n > 0) { dlg_fokus = 0; dlg_scroll = 0; starte(AUF_KOMPLETT); }
        else kurz(T("Es wurde nichts gefunden.", "Nothing was found."));
#endif
    }
    else if (t == T_QUADRAT) sprache_wechseln();   /* NEU 1.1 (17:58): auch nach dem Nachschauen - dann wird neu nachgeschaut */
    else if (t == T_R1 && fertig) { optionen_offen = false; starte(AUF_SUCHEN); }   /* NEU 1.1 (17:58): R1 = Nochmal nachschauen (Fußleiste) */
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
        kblock *x = NULL; int nb = bloecke_holen(fa, k, sortierung, &x);   /* NEU 1.1: dieselbe Reihenfolge wie am Fernseher */
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
                const char *bid = bild_kennung(block_bild(&x[i])); if (bid && strlen(bid) < 16) fj_s(y, "i", bid);
                const char *da = kern_art_info(fa, x[i].kopf); if (!da[0]) da = kern_folge(x[i].kopf);
                fj_s(y, "da", da);
            }
            cJSON *t = cJSON_AddArrayToObject(y, "t");
            for (int j = 0; j < x[i].teile.n; j++) {
                int n = fd_nr(&nr, x[i].teile.e[j]); if (n < 0) continue;
                cJSON *u = cJSON_CreateObject(); fj_n(u, "e", n);
                fj_s(u, "n", (!reste && x[i].teile.e[j] == x[i].kopf && kopf_kurz(x[i].teile.e[j], x[i].name)) ? kopf_kurz(x[i].teile.e[j], x[i].name)
                           : reste ? ohne_ort_vorne(kern_zeile(fa, x[i].teile.e[j])) : unter_name(&x[i], j, ohne_jb_start(nach_name(ohne_ort_vorne(kern_zeile(fa, x[i].teile.e[j])), x[i].name)
                                                                                                        ? nach_name(ohne_ort_vorne(kern_zeile(fa, x[i].teile.e[j])), x[i].name)
                                                                                                        : ohne_ort_vorne(kern_zeile(fa, x[i].teile.e[j])), k)));
                if (!reste && teil_ebene(&x[i], j) > 1) fj_n(u, "eb", teil_ebene(&x[i], j));   /* NEU 1.1: Untergruppe */
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
        else if (f->Bezug) ABS(NULL, f->Bezug, "o");
    }
    ABS(f->Pfade.n == 1 ? T("Ordner", "Folder") : T("Ordner", "Folders"), ordner_text(f), "d");
    #undef ABS
    fj_n(x, "e", fd_info_i);
}
static struct { Uint32 zeit; bool da[4]; long long frei[4], ges[4]; int usb_n; } fd_sp;
#include "fern_tv.h"   /* NEU 1.1: was der Fernseher zeigt (Browserseite als Abbild der App) */
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
        {   /* NEU 1.1 (Browser als Abbild): was gerade gesperrt ist, ändert sich mit der Auswahl (z. B. die Kachel, die mit der Spieldatei geht) */
            char *gs = a_hole(fa, (size_t)K->Funde.n + 1); cJSON *gg = cJSON_AddObjectToObject(o, "sperr_g");
            for (int i = 0; i < K->Funde.n; i++) {
                const char *gr = NULL; bool x = ablauf_eintrag_gesperrt(&A, K->Funde.e[i], &gr); gs[i] = x ? '1' : '0';
                if (x && gr && gr[0]) { char nr[16]; snprintf(nr, sizeof nr, "%d", i); fj_s(gg, nr, gr); }
            }
            gs[K->Funde.n] = 0; fj_s(o, "sperr", gs);
        }
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
        fj_s(l, "kurz", gw == 0 ? T("Nichts ausgewählt", "Nothing selected") : VERB(fa, a_fmt(fa, "%d", gw), T(" ausgewählt", " selected"), galle ? VERB(fa, " · ", groesse_von(fa, gb, ggek)) : ""));   /* NEU 1.1: Pille oben (Entwurf Browser) */
        fj_s(l, "t1", gw == 0 ? T("Nichts ausgewählt.", "Nothing selected.") : VERB(fa, T("Ausgewählt: ", "Selected: "), a_fmt(fa, "%d", gw), EINTRAEGE(gw), galle ? VERB(fa, " · ", groesse_von(fa, gb, ggek)) : ""));
        const char *eg = kern_endgueltig_satz(K, fa), *t2 = NULL, *c2 = "d";
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
                fj_b(z, "h", lz[i].haupt && lz[i].f);   /* NEU 1.1: Hauptteil mit Bild */
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
            fj_s(x, "sub", sub); if (A.GROSS.Bytes > 0) fj_s(x, "gross_t", groesse_von(fa, A.GROSS.Bytes, A.GROSS.Gekappt));
            b_hinweis hw[16]; int nh = bestaetigen_hinweise(hw, 16);
            cJSON *h = cJSON_AddArrayToObject(x, "hinweise");
            for (int i = 0; i < nh; i++) { cJSON *z = cJSON_CreateObject(); fj_s(z, "t", hw[i].t); fj_s(z, "c", hw[i].f == N_ROT ? "r" : hw[i].f == N_GELB ? "y" : "d"); cJSON_AddItemToArray(h, z); }
            if (jb_gew) fj_s(x, "jb", T("Jailbreak-Einstieg ausgewählt", "Jailbreak entry selected")); else if (jb_da) fj_s(x, "jb_bleibt", T("Jailbreak bleibt (nicht ausgewählt)", "Jailbreak stays (not selected)"));
            {   /* NEU 1.1 (Entwurf BrowserBestaetigen): "Bleibt unangetastet" und "Speicher danach" wie am Fernseher */
                const char *bl = VERB(fa, T("Laufwerkskopplung, Lizenzen und Konten, Systemsoftware", "Disc drive pairing, licenses and accounts, system software"));
                if (jb_da && !jb_gew) { const char *jn = kopf_jb_name(); bl = VERB(fa, bl, ", ", jn && jn[0] ? VERB(fa, "Jailbreak ", jn) : "Jailbreak"); }
                fj_s(x, "bleibt", VERB(fa, bl, T(" und alles nicht Angehakte.", " and everything not ticked.")));
                long long weg[4] = { 0, 0, 0, 0 };
                for (int i = 0; i < K->Funde.n; i++) {
                    eintrag *f = K->Funde.e[i]; if (!f->Wahl || f->Gesperrt) continue;
                    groesse *gg = kern_eintrag_groesse_in(K, fa, f, true); int o = anzeige_ort(f);
                    if (gg && o >= 0 && o < 4) weg[o] += gg->Bytes;
                }
                speicher11 sp = speicher11_holen();
                cJSON *dn = cJSON_AddArrayToObject(x, "danach");
                for (int o = 0; o < 4; o++) if (sp.da[o] && (o == 0 || weg[o] > 0)) {
                    long long fr = sp.frei[o] + weg[o]; if (fr > sp.ges[o]) fr = sp.ges[o];
                    cJSON *z = cJSON_CreateObject(); fj_s(z, "name", ort_name11(o, sp.usb_n)); fj_s(z, "frei", VERB(fa, kern_groesse_text(fa, fr), T(" frei", " free")));
                    if (weg[o] > 0) fj_s(z, "plus", VERB(fa, "+", kern_groesse_text(fa, weg[o])));
                    cJSON_AddItemToArray(dn, z);
                }
            }
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
            {   /* NEU 1.1 (Entwurf BrowserFertig): dieselbe Ergebnisliste und Überschrift wie am Fernseher (schirm_fertig) */
                bool fe = fertig_fehler(); int nf = 0;
                cJSON *el = cJSON_AddArrayToObject(x, "erg");
                pthread_mutex_lock(&erg_m);
                for (int pass = 0; pass < 2; pass++) for (int i = 0; i < erg_n; i++) {
                    if ((erg[i].zustand >= 2) != (pass == 0)) continue;
                    if (erg[i].zustand >= 2) nf++;
                    if (cJSON_GetArraySize(el) >= 300) continue;
                    cJSON *z = cJSON_CreateObject(); const char *nm = erg[i].name, *wt = strstr(nm, " – ");
                    fj_s(z, "n", wt ? a_kopie_n(fa, nm, (size_t)(wt - nm)) : nm);
                    if (erg[i].zustand >= 2 && erg[i].grund[0]) fj_s(z, "w", erg[i].grund); else if (wt) fj_s(z, "w", wt + strlen(" – "));   /* NEU 1.1: Grund wie am Fernseher */
                    if (erg[i].hat_b) fj_s(z, "g", groesse_von(fa, erg[i].b, false));
                    fj_n(z, "z", erg[i].zustand); fj_n(z, "k", erg[i].kat); if (erg[i].bild[0]) fj_s(z, "i", erg[i].bild);   /* NEU 1.3: Bild */
                    cJSON_AddItemToArray(el, z);
                }
                pthread_mutex_unlock(&erg_m);
                fj_b(x, "fehler", fe); fj_b(x, "komplett", K->KOMPLETT_LAUF);
                fj_s(x, "titel2", fe ? (nf == 1 ? T("Eins ist noch nicht weg.", "One thing is not gone yet.") : nf > 1 ? a_fmt(fa, T("%d sind noch nicht weg.", "%d are not gone yet."), nf) : T("Nicht alles hat geklappt.", "Not everything worked."))
                                     : sim ? T("Simulation fertig.", "Simulation done.") : T("Alles gelöscht.", "All deleted."));
                fj_s(x, "s1", fe ? T("Alles andere ist gelöscht · ", "Everything else is deleted · ") : "");
                fj_s(x, "gt", gt);
                fj_s(x, "s3", fe ? T(" freigeräumt", " freed") : sim ? T(" würden frei · es wurde nichts gelöscht", " would be freed · nothing was deleted") : T(" freigeräumt · nichts ist übrig geblieben", " freed · nothing is left over"));
            }
        }
    }
    fd_tv(o);
    char *s = cJSON_PrintUnformatted(o); cJSON_Delete(o);
    return s;
}
/* ---- Befehle aus dem Browser */
static int tasten_name(const char *s);
/* NEU 1.1: Text aus dem Browser übernehmen - nur ganze UTF-8-Zeichen, keine Steuerzeichen, höchstens max Zeichen */
static void fd_text_kopie(char *d, size_t n, const char *q, int max)
{
    size_t j = 0; int z = 0;
    for (const unsigned char *p = (const unsigned char *)q; *p && z < max; ) {
        int l = *p >= 0xF0 ? 4 : *p >= 0xE0 ? 3 : *p >= 0xC0 ? 2 : 1, ok = 1;
        for (int i = 1; i < l && ok; i++) if ((p[i] & 0xC0) != 0x80) ok = 0;   /* bricht am Ende (0) ab */
        if (!ok || *p < 0x20 || (*p >= 0x80 && *p < 0xC0)) { p++; continue; }
        if (j + (size_t)l + 1 > n) break;
        memcpy(d + j, p, (size_t)l); j += (size_t)l; p += l; z++;
    }
    d[j] = 0;
}
static void fd_block_setzen(int k, int kopf)
{
    kblock *x = NULL; int nb = bloecke_holen(fa, k, sortierung, &x);   /* NEU 1.1: dieselbe Reihenfolge wie am Fernseher */
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
    /* NEU 1.1 (da_Chris86 15.11.2026 20:17: "genauso funktioniert und genauso ausschaut wie auf der App"): die Browserseite ist ein Abbild
     * der App - Tasten wie am Controller, Klicks = Fokus auf diesen Knopf und ✕. Dieselben Prüfungen wie am Fernseher (taste_*). */
    if (!strcmp(b, "taste")) {
        const char *tn = jtext(j, "t"); int t = tn ? tasten_name(tn) : T_ANDERE;
        if (t == T_ANDERE) return;
        if (G.art != D_KEINER && nr != G.dlg_nr) return;              /* zu einer anderen (alten) Frage */
        if (t == T_X && bestaetigen_aktiv() && G.art == D_WAHL) return; /* gelöscht wird im Browser nur mit dem Haken "geprüft" (antwort) */
        taste(t); neu_zeichnen(); return;
    }
    if (!strcmp(b, "klick")) {
        const char *wo = jtext(j, "wo"); if (!wo) return;
        if (!strcmp(wo, "opt")) {
            if (!optionen_offen || G.beschaeftigt || G.art != D_KEINER || UPD.offen) return;
            int ids[10], n = opt_ids11(ids); if (i < 0 || i >= n || ids[i] == OP_BROWSER) return;
            opt_fokus = i; taste_optionen11(T_X);
        }
        else if (!strcmp(wo, "upd")) { if (!UPD.offen || !optionen_offen) return; UPD.fokus = i == 1 ? 1 : 0; taste_upd11(T_X); }
        else if (!strcmp(wo, "rm")) {
            if (RM.offen && i >= 10 && i <= 13 && RM.wertung == 1) { RM.fokus = 5; RM.gfokus = i - 10; taste_rueckmeldung(T_X); return; }   /* NEU 1.3: Grund */
            if (!RM.offen || i < 0 || i > 4 || i == 2) return; RM.fokus = i; taste_rueckmeldung(T_X); }
        else if (!strcmp(wo, "kb")) { if (!komplett_frage() || nr != G.dlg_nr) return; KB.fokus = i < 0 ? 0 : i; taste_komplett11(T_X); }
        else if (!strcmp(wo, "dlg")) { if (!frage || bestaetigen_aktiv() || (G.art != D_WAHL && G.art != D_FEHLER)) return; dlg_fokus = i ? 1 : 0; taste_dialog(T_X); }
        neu_zeichnen(); return;
    }
    if (!strcmp(b, "rm_text")) { const char *tx = jtext(j, "t"); if (!RM.offen || !tx) return; fd_text_kopie(RM.text, sizeof RM.text, tx, 200); RM.fokus = 3; neu_zeichnen(); return; }
    if (!strcmp(b, "kb_wort")) {   /* Alles bereinigen: das eingetippte Wort (der Kern prüft es selbst noch einmal) */
        const char *tx = jtext(j, "t"); if (!tx || !komplett_frage() || nr != G.dlg_nr) return;
        pthread_mutex_lock(&G.m); fd_text_kopie(G.eingabe, sizeof G.eingabe, tx, 30); pthread_mutex_unlock(&G.m);
        if (jja(j, "los")) taste_komplett11(T_DREIECK);   /* wie △ am Controller: passt das Wort, geht es los */
        neu_zeichnen(); return;
    }
    if (!strcmp(b, "reiter")) {
        if (!(fd_liste_ok() && G.art == D_KEINER)) return;
        if (!AUSWAHL_OFFEN) { AUSWAHL_OFFEN = true; fokus_zone = 1; optionen_offen = false; }
        sicht_neu();
        { int kz = jzahl(j, "k", -1); if (kz >= 0) { i = -1; for (int q = 0; q < sicht_n; q++) if (sicht[q] == kz) { i = q; break; } } }   /* Codex-Prüfung 341 (P3): Kachel der Übersicht = ihr Bereich */
        if (i >= 0 && i < sicht_n && i != fokus_bereich) { fokus_bereich = i; AL.k = -1; al_b = 0; al_t = -1; al_py = 0; }
        neu_zeichnen(); return;
    }
    if (!strcmp(b, "mark")) {   /* Zeile der Liste markieren (Block bi, Teil ti oder -1) - der Kasten rechts zeigt sie, wie am Fernseher */
        if (!(fd_liste_ok() && G.art == D_KEINER && AUSWAHL_OFFEN && fg == such_nr)) return;
        sicht_neu(); int kk = sicht[fokus_bereich]; al_ordnen(kk);
        int bi = jzahl(j, "bi", -1), ti = jzahl(j, "ti", -1), ke = jzahl(j, "ke", -3);   /* ke: Eintrag des Blockkopfs, -1 = Reste */
        if (bi < 0 || bi >= AL.n) return;
        bool passt = ke == -1 ? AL.bl[bi].kopf == &RESTE_ZEILE : (ke >= 0 && ke < K->Funde.n && AL.bl[bi].kopf == K->Funde.e[ke]);
        if (!passt) return;   /* die Liste hat sich inzwischen geändert */
        if (ti >= 0) {   /* Codex-Prüfung 340 (P2): der Teil zählt nach seinem Eintrag, nicht nach seiner Stelle - Teile können sich umsortieren */
            int te = jzahl(j, "te", -1); ti = -1;
            if (te >= 0 && te < K->Funde.n) for (int q = 0; q < AL.bl[bi].teile.n; q++) if (AL.bl[bi].teile.e[q] == K->Funde.e[te]) { ti = q; break; }
            if (ti < 0) return;
        }
        al_b = bi; al_t = ti >= 0 && ti < AL.bl[bi].teile.n && al_mehr(bi) ? ti : -1; fokus_zone = 1;
        neu_zeichnen(); return;
    }
    if (!strcmp(b, "upd_auf")) {   /* Pille "Update verfügbar" geklickt: Optionen mit dem Update-Fenster */
        char un[32]; upd_holen(un, sizeof un);
        if (!un[0] || G.beschaeftigt || G.art != D_KEINER || RM.offen) return;
        optionen_offen = true; opt_fokus = 0;
        if (__atomic_load_n(&UPD.stand, __ATOMIC_ACQUIRE) != 1) { UPD.offen = true; UPD.fokus = 0; UPD.stand = 0; }
        neu_zeichnen(); return;
    }
    if (!strcmp(b, "prot")) { protokoll_offen = jja(j, "an"); if (protokoll_offen) { optionen_offen = false; prot_scroll = 1 << 20; } neu_zeichnen(); return; }
    /* was den Zustand ändert, schließt am Fernseher das Options-Menü, das Protokoll und die Info-Seite (sonst läge dort etwas Altes) */
    optionen_offen = false; protokoll_offen = false; fern_info_offen = false; neu_zeichnen();
    if (!strcmp(b, "nachschauen")) { if (frei) { AUSWAHL_OFFEN = false; starte(AUF_SUCHEN); } }
    else if (!strcmp(b, "auswahl")) { if (frei && GESUCHT && ablauf_verbunden(&A) && !AUSWAHL_OFFEN) { AUSWAHL_OFFEN = true; fokus_zone = 0; fokus_bereich = 0; AL.k = -1; al_b = 0; al_t = -1; al_py = 0; } }
    else if (!strcmp(b, "sprache")) { if (frei) sprache_wechseln(); }   /* NEU 1.1: auch nach dem Nachschauen (schaut neu nach) */
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
        /* NEU 1.1 (Entwurf BrowserBestaetigen): im Browser bestätigt der Haken "Ich habe geprüft, was gelöscht wird" statt des Haltens */
        bool geprueft = jja(j, "geprueft");
        if (bestaetigen_aktiv() && ja && (!(jja(j, "gehalten") || geprueft) || SDL_GetTicks() - dlg_seit < HALTEN_MS)) {
            kurz(geprueft ? T("Bitte erst in Ruhe prüfen, was gelöscht wird - dann noch einmal „Jetzt löschen“.", "Please check what will be deleted first - then press 'Delete now' again.")
                          : T("Zum Löschen „Löschen“ gedrückt halten, bis der Kreis voll ist.", "To delete, hold 'Delete' until the circle is full.")); return; }
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
        if (frage && G.art == D_FERTIG && jtext(j, "a") && !strcmp(jtext(j, "a"), "nochmal")) {   /* NEU 1.1: 5b "Noch einmal versuchen" auch im Browser */
            pthread_mutex_lock(&G.m); bool passt = G.dlg_nr == nr && !G.beantwortet; pthread_mutex_unlock(&G.m);
            if (passt && fertig_fehler()) nochmal_versuchen();
            return;
        }
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
static void fern_bild_senden(const char *id, unsigned char *px, int w, int h);
static void fern_bilder(void)
{
    /* 1.3: auch auf der Prüfseite und bei "Alles bereinigen" - dort wartet der Kern auf die Antwort (offene Frage, G.art); nie während des
     * Nachschauens (G.bestaetigen allein bleibt nach der Antwort stehen - darum bestaetigen_aktiv, Abschlussprüfung 20.11.2026) */
    if (G.beschaeftigt && !A.loescht && !bestaetigen_aktiv() && !komplett_frage()) return;
    char id[24]; int n = 0;
    while (n++ < 4) {
        /* NEU 1.0.10 (Codex-Prüfung 147): wie kachelbild() - läuft gerade ein Besitzer-Wechsel (private Ordner), erst beim nächsten Mal;
         * der Wunsch bleibt so lange stehen (erst nach der Sperre abholen) */
        if (!zugriff_wechsel_versuchen()) break;
        if (!fern_bild_wunsch(id, sizeof id)) { zugriff_wechsel_frei(); break; }
        int w = 0, h = 0; unsigned char *px = kachelbild_laden(id, &w, &h);
        zugriff_wechsel_frei();
        fern_bild_senden(id, px, w, h);
    }
}
/* das geladene Bild (RGBA, wird freigegeben) für den Browser verkleinern und abgeben; px = NULL: es gibt keins */
static void fern_bild_senden(const char *id, unsigned char *px, int w, int h)
{
    {
        if (!px) { fern_bild_geben(id, NULL, 0, 0); return; }
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
/* NEU 1.3 (PS5-Test da_Chris86 20.11.2026 21:33: auf der Ergebnisseite standen gelöschte Kacheln nur mit Buchstaben - ihr Bild liegt in der
 * Kachel, die gerade gelöscht wurde). Auf der Prüfseite, bei "Alles bereinigen" und während des Löschens die Bilder aller gewählten Einträge
 * vorher in den Speicher holen (am Fernseher je Bild höchstens 2, für den Browser höchstens 2 - nur wenn einer zuschaut). Nur lesen. */
static bool bild_im_speicher(const char *id) { for (int i = 0; i < bilder_n; i++) if (!strcmp(bilder[i].id, id)) return true; return false; }
static void bilder_vorladen(void)
{
    /* PS5-Test 20.11.2026 21:52: im Browser blieben die Bilder aus - auf der Prüfseite gilt PS5 Cleanup als beschäftigt (der Arbeitsfaden wartet
     * auf die Antwort, die Einträge ändern sich dabei nicht). Darum nur noch: Prüfseite, "Alles bereinigen" oder Löschen läuft. */
    if (!K || !(A.loescht || bestaetigen_aktiv() || komplett_frage())) return;   /* bestaetigen_aktiv: nur solange die Frage offen ist */
    bool browser = fern_aktiv(15000); int nb = 0;
    for (int i = 0; i < K->Funde.n; i++) {
        const eintrag *f = K->Funde.e[i];
        if (!f->Wahl && !(f->Kachel && komplett_frage())) continue;   /* bei "Alles bereinigen" wählt der Kern erst beim Löschen - die Kacheln vorher */
        const char *id = bild_kennung(f); if (!id || !id[0] || strlen(id) >= 16 || bild_fehlt(id)) continue;
        if (bilder_neu < 2 && !bild_im_speicher(id)) kachelbild(f, 76);
        if (browser && nb < 2 && !fern_bild_bekannt(id)) {
            if (!zugriff_wechsel_versuchen()) break;
            int w = 0, h = 0; unsigned char *px = kachelbild_laden(id, &w, &h);
            zugriff_wechsel_frei();
            fern_bild_senden(id, px, w, h); nb++;
        }
        if (bilder_neu >= 2 && (!browser || nb >= 2)) break;
    }
}
/* aus der Hauptschleife: Zustand und Liste für den Browser - nur wenn einer fragt; der Zustand höchstens alle 120 ms, die Liste bei einer
 * neuen Suche, beim Umschalten der Sprache und während des Zählens höchstens alle 1,5 s (dann ändern sich die Größen) */
static Uint32 fd_zeit, fd_l_zeit; static int fd_l_fg = -1, fd_l_hg = -1, fd_l_en = -1, fd_l_sort = -1, fd_l_bf = -1; static bool fd_l_da = false, fd_dringend = true;
static void fern_daten_raus(void)
{
    fern_bilder();
    bilder_vorladen();   /* NEU 1.3 */
    static bool im_browser = false;   /* Codex-Prüfung 140 (P3): Pille "IM BROWSER" an bzw. aus, auch wenn sich sonst nichts ändert */
    if (fern_aktiv(8000) != im_browser) { im_browser = !im_browser; neu_zeichnen(); }
    if (!fern_will_daten()) { fd_l_da = false; return; }
    Uint32 jetzt = SDL_GetTicks();
    if (!fd_dringend && jetzt - fd_zeit < 120) return;
    uint64_t mess_a = mess_ms();   /* NEU 1.3 */
    arena *alt = fa; fa = arena_neu();
    /* NEU 1.1: auch, wenn sich ein Kachelbild als nicht ladbar erwiesen hat (block_bild nimmt dann das Bild eines Teils) - höchstens alle 1,5 s */
    if (fd_liste_ok() && (!fd_l_da || fd_l_fg != such_nr || fd_l_en != SP_EN || fd_l_sort != sortierung || ((fd_l_hg != A.hg_n || fd_l_bf != bild_fehlt_n) && jetzt - fd_l_zeit >= 1500))) {
        fd_liste_v++;
        char *l = fd_liste_json();
        if (l) { fern_liste_geben(l); fd_l_da = true; fd_l_fg = such_nr; fd_l_en = SP_EN; fd_l_sort = sortierung; fd_l_hg = A.hg_n; fd_l_bf = bild_fehlt_n; fd_l_zeit = jetzt; }
    }
    char *z = fd_zustand_json();
    arena_frei(fa); fa = alt; ablauf_texte_frei(&A);
    if (z) fern_zustand_geben(z);
    fd_zeit = jetzt; fd_dringend = false;
    mess_dazu(&MW_FERN, mess_ms() - mess_a, 50);
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

/* Name einer Taste -> T_... (Browser-Befehl "taste" und Skript der PC-Vorschau) - NEU 1.1: auch in der PS5-Fassung, der Browser braucht ihn */
static int tasten_name(const char *s)
{
    const char *n[] = { "X", "KREIS", "QUADRAT", "DREIECK", "HOCH", "RUNTER", "LINKS", "RECHTS", "L1", "R1", "OPTIONS", "L2", "R2" };
    for (int i = 0; i < 13; i++) if (!strcmp(s, n[i])) return i;
    return T_ANDERE;
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
    fenster = SDL_CreateWindow(FENSTER_TITEL, 0, 0, BREITE, HOEHE, 0);
    if (!fenster) { printf("PS5 Cleanup: SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    ziel = SDL_GetWindowSurface(fenster);
    if (!ziel) { printf("PS5 Cleanup: SDL_GetWindowSurface: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    stbtt_InitFont(&font[4], schrift_normal, stbtt_GetFontOffsetForIndex(schrift_normal, 0));
    stbtt_InitFont(&font[5], schrift_fett, stbtt_GetFontOffsetForIndex(schrift_fett, 0));
    /* NEU 1.1: Inter; lässt sie sich nicht öffnen, DejaVu an ihrer Stelle (nie ohne Schrift) */
    if (!stbtt_InitFont(&font[0], schrift_inter, 0)) font[0] = font[4];
    if (!stbtt_InitFont(&font[1], schrift_inter_fett, 0)) font[1] = font[5];
    if (!stbtt_InitFont(&font[2], schrift_inter_halb, 0)) font[2] = font[0];
    if (!stbtt_InitFont(&font[3], schrift_inter_extra, 0)) font[3] = font[1];
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
    {   /* NEU 1.1: Schriften und Bilder der App für die Browserseite (Abbild der App) */
        static const fern_datei_t D[] = {
            { "inter0.otf", "font/otf", schrift_inter, schrift_inter_ende }, { "inter1.otf", "font/otf", schrift_inter_fett, schrift_inter_fett_ende },
            { "inter2.otf", "font/otf", schrift_inter_halb, schrift_inter_halb_ende }, { "inter3.otf", "font/otf", schrift_inter_extra, schrift_inter_extra_ende },
            { "hg.png", "image/png", b11_hintergrund, b11_hintergrund_ende }, { "banner.jpg", "image/jpeg", b11_banner, b11_banner_ende },
            { "logo.png", "image/png", b11_logo, b11_logo_ende },
            { "k0.png", "image/png", b11_k_ps5_lw, b11_k_ps5_lw_ende }, { "k1.png", "image/png", b11_k_ps5_dig, b11_k_ps5_dig_ende },
            { "k2.png", "image/png", b11_k_slim_lw, b11_k_slim_lw_ende }, { "k3.png", "image/png", b11_k_slim_dig, b11_k_slim_dig_ende },
            { "k4.png", "image/png", b11_k_pro_lw, b11_k_pro_lw_ende }, { "k5.png", "image/png", b11_k_pro_dig, b11_k_pro_dig_ende },
            { "g0.png", "image/png", b11_s_spiele_g, b11_s_spiele_g_ende }, { "g1.png", "image/png", b11_s_apps_g, b11_s_apps_g_ende },
            { "g2.png", "image/png", b11_s_spielst_g, b11_s_spielst_g_ende }, { "g3.png", "image/png", b11_s_troph_g, b11_s_troph_g_ende },
            { "g4.png", "image/png", b11_s_bilder_g, b11_s_bilder_g_ende }, { "g5.png", "image/png", b11_s_sonst_g, b11_s_sonst_g_ende },
            { "g6.png", "image/png", b11_s_jb_g, b11_s_jb_g_ende },
            { "s0.png", "image/png", b11_s_spiele_k, b11_s_spiele_k_ende }, { "s1.png", "image/png", b11_s_apps_k, b11_s_apps_k_ende },
            { "s2.png", "image/png", b11_s_spielst_k, b11_s_spielst_k_ende }, { "s3.png", "image/png", b11_s_troph_k, b11_s_troph_k_ende },
            { "s4.png", "image/png", b11_s_bilder_k, b11_s_bilder_k_ende }, { "s5.png", "image/png", b11_s_sonst_k, b11_s_sonst_k_ende },
            { "s6.png", "image/png", b11_s_jb_k, b11_s_jb_k_ende } };
        fern_dateien(D, (int)(sizeof D / sizeof D[0]));
    }
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
            case SDL_CONTROLLERAXISMOTION: {   /* NEU 1.1: L2/R2 sind bei SDL Achsen - beim Durchdrücken einmal auslösen */
                static bool gedrueckt[2];
                int i = e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? 0 : e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT ? 1 : -1;
                if (i < 0) break;
                if (!gedrueckt[i] && e.caxis.value > 20000) { gedrueckt[i] = true; int t = i ? T_R2 : T_L2; if (!tv_vorrang(t)) taste(t); }
                else if (gedrueckt[i] && e.caxis.value < 8000) gedrueckt[i] = false;
                break;
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
        if (KB.auto_an && G.art != D_KEINER && !G.beantwortet && SDL_GetTicks() - dlg_seit >= 150) komplett_auto();   /* NEU 1.1 (Entwurf 3b) */
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
        if (leucht_bis && (Sint32)(jetzt - leucht_bis) >= 0) { leucht_bis = 0; neu_zeichnen(); }   /* NEU 1.3: Aufleuchten vorbei */
        if (lauf_aktiv && (Sint32)(jetzt - lauf_naechst) >= 0 && jetzt - bild_zeit >= 33) neu_zeichnen();   /* NEU 1.0.6: Laufschrift (1.0.8: nicht in den Pausen) */
        if (nach_aktion && !G.beschaeftigt && G.art == D_KEINER) {         /* NEU 1.0.6: Auswahl am Ende der Bereinigung */
            int na = nach_aktion; nach_aktion = 0;
            if (na == 3) { if (GESUCHT && nochmal_warten) nochmal_anhaken(); else nochmal_warten = false; na = 0; }   /* NEU 1.1: "Noch einmal versuchen" - danach weder Beenden noch Neustart */
            else if (na == 1 && neustart_pflicht) {
                na = 2;
                ui_log(NULL, T("Vollständige Bereinigung - die PS5 startet deshalb jetzt neu.",
                               "Full cleanup - the PS5 therefore restarts now."));
            }
            if (na == 0) { }
            else if (na == 1) {   /* ○ am Ende: Beenden (da_Chris86 02.11.2026) - der Hinweis "neu starten" kommt als Benachrichtigung */
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
        if (__atomic_exchange_n(&G.neu, false, __ATOMIC_ACQ_REL)) {
            __atomic_store_n(&neu_bald, false, __ATOMIC_RELEASE);
            uint64_t mess_a = mess_ms();   /* NEU 1.3 */
            zeichnen(); SDL_UpdateWindowSurface(fenster); bild_zeit = SDL_GetTicks(); gezeichnet = true;
            uint64_t mess_b = mess_ms(); mess_dazu(&MW_BILD, mess_b - mess_a, 50);
            if (mess_taste_ab) { mess_dazu(&MW_TASTE, mess_b - mess_taste_ab, 100); mess_taste_ab = 0; }
            if (!mess_erstes_bild) {
                mess_erstes_bild = true; char z[160];
                snprintf(z, sizeof z, T("Zeit: Start bis erstes Bild %llu ms", "Time: start to first frame %llu ms"), (unsigned long long)(mess_b - mess_start));
                ui_log(NULL, z);
            }
        }
        (void)gezeichnet; fern_daten_raus();       /* NEU 1.0.9: Zustand, Liste und Bilder für den Browser */
#ifdef MITSCHNITT
        ms_bild_vielleicht();                      /* NEU 1.0.8 (2.7) */
#endif
        SDL_Delay(16);
    }
    mess_bericht();   /* NEU 1.3 */
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
    mess_start = mess_ms();   /* NEU 1.3 */
#ifdef VORSCHAU
    leucht_ohne_pause = getenv("VORSCHAU_SKRIPT") != NULL;
#endif
    setvbuf(stdout, NULL, _IOLBF, 0);
    plattform_start();
#ifdef VORSCHAU
    if (getenv("VORSCHAU_SKRIPT")) skript = fopen(getenv("VORSCHAU_SKRIPT"), "r");
#endif
    int r = programm();
    plattform_ende();
    return r;
}
