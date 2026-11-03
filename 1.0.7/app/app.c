/* PS5 Cleanup 1.1 - Oberfläche am Fernseher
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef KERN_LOESCHEN
#include <fcntl.h>    /* NEU 02.11.2026 (Prüfung p8, Befund 3): Protokolldatei mit O_NOFOLLOW öffnen */
int close(int);       /* aus unistd.h - das ganze unistd.h nicht, weil app.c eine eigene Funktion dup() hat */
#endif
#include "../kern/ablauf.h"
#include "../db/db_sicherung.h"   /* NEU 1.0.7: Datenbank-Sicherung mit SQLite-Prüfung */
#include "plattform.h"
#include "qrcodegen.h"        /* NEU 1.0.7 (03.11.2026): QR-Code für die freiwillige Rückmeldung (Project Nayuki, MIT) */

#define STB_TRUETYPE_IMPLEMENTATION
#include "../probelauf/fremd/stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../probelauf/fremd/stb_image.h"

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
#define VERSION_TEXT "1.0.7"                 /* 02.11.2026 (da_Chris86, finaler Plan 1.0.7 Punkt 7): ohne "Vorschau" */
#else
#define VERSION_TEXT "1.0.7 TEST · Bau " __DATE__ " " __TIME__   /* Testfassung - ohne Löschbefehle gebaut, löscht nie; 31.10.2026: Bauzeit sichtbar (welche Fassung läuft?) */
#endif
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
static void text(int x, int y, int f, int gr, uint32_t farbe, const char *t)
{
    Uint8 fr = (farbe >> 16) & 255, fg = (farbe >> 8) & 255, fb = farbe & 255;
    SDL_PixelFormat *pf = ziel->format;
    while (*t) {
        Glyph *g = glyph(f, gr, cp_naechstes(&t));
        if (!g) continue;
        for (int j = 0; j < g->h; j++) {
            int py = y + g->yoff + j;
            if (py < 0 || py >= ziel->h) continue;
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
            const char *start = p, *letzt_leer = NULL, *q = p; int b = 0;
            while (*q) {
                const char *v = q; int c = cp_naechstes(&q);
                Glyph *g = glyph(f, gr, c); b += g ? g->vorschub : 0;
                if (b > breite && v > start) { q = v; break; }
                if (c == ' ') letzt_leer = q;
            }
            if (*q && letzt_leer && letzt_leer > start) q = letzt_leer;
            r.z[r.n++] = a_kopie_n(fa, start, (size_t)(q - start));
            p = q; while (*p == ' ' && p != start) p++;
        }
        if (!ende) break;
        absatz = ende + 1;
    }
    return r;
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
static void laufschrift(int x, int y, int f, int gr, uint32_t farbe, const char *t, int breite, const void *wer)
{
    int tw = textbreite(f, gr, t);
    if (tw <= breite) { text(x, y, f, gr, farbe, t); return; }
    zu_lang("Laufschrift", gr, breite, t);
    Uint32 jetzt = SDL_GetTicks();
    if (wer != lauf_wer) { lauf_wer = wer; lauf_seit = jetzt; }
    int weg = tw - breite + 20;
    Uint32 halt = 1500, lauf = (Uint32)weg * 1000 / 90, z = (jetzt - lauf_seit) % (halt + lauf + halt);
    int off = z < halt ? 0 : z < halt + lauf ? (int)((z - halt) * 90 / 1000) : weg;
    clip_x0 = x; clip_x1 = x + breite;
    text(x - off, y, f, gr, farbe, t);
    clip_x0 = 0; clip_x1 = 1 << 30;
    lauf_aktiv = true;
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
        if (i == n - 1 && z.n > max) { for (int j = i + 1; j < z.n; j++) s = VERB(fa, s, " ", z.z[j]); s = kuerzen(f, gr, s, breite); }
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
        const char *s = p.z.z[max_z - 1];
        for (int j = max_z; j < p.z.n; j++) s = VERB(fa, s, " ", p.z.z[j]);
        p.z.z[max_z - 1] = kuerzen(f, p.gr, s, breite);
        p.z.n = max_z;
    }
    return p;
}
static void passend_zeichnen(int x, int y, int f, uint32_t farbe, const passend *p)   /* y = Grundlinie der ersten Zeile */
{
    for (int i = 0; i < p->z.n; i++) text(x, y + i * p->lh, f, p->gr, farbe, p->z.z[i]);
}
/* eine Listenzeile (46 Pixel hoch): ein oder zwei Zeilen, senkrecht mittig */
static void zeile_zeichnen(int x, int ry, int f, uint32_t farbe, const passend *p)
{
    int n = p->z.n < 2 ? p->z.n : 2, lh = p->gr + 3, y0 = ry + 23 - (n - 1) * lh / 2 + p->gr * 36 / 100;
    for (int i = 0; i < n; i++) text(x, y0 + i * lh, f, p->gr, farbe, p->z.z[i]);
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
    char p[1024];
#ifdef VORSCHAU
    const char *w = getenv("VORSCHAU_WURZEL"); snprintf(p, sizeof p, "%s%s", w ? w : "", pfad);
#else
    snprintf(p, sizeof p, "%s", pfad);
#endif
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
        if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1;
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
static unsigned char *kachelbild_laden(const char *id, int *w, int *h)
{
    const char *wege[3] = { "/user/appmeta/%s/icon0.png", "/system_data/priv/appmeta/%s/icon0.png", "/user/app/%s/sce_sys/icon0.png" };
    for (int i = 0; i < 3; i++) {
        char p[256]; snprintf(p, sizeof p, wege[i], id);
        size_t n = 0; unsigned char *d = datei_lesen(p, &n);
        if (!d) continue;
        /* NEU 1.0.7 (Prüfung 02.11.2026): Abmessungen vor dem Entpacken prüfen - eine kaputte icon0.png mit riesiger Breite
         * könnte sonst beim Verkleinern über den Puffer hinaus lesen. icon0.png ist normal 512 x 512. */
        int iw = 0, ih = 0, ic = 0;
        if (!stbi_info_from_memory(d, (int)n, &iw, &ih, &ic) || iw <= 0 || ih <= 0 || iw > 4096 || ih > 4096) { free(d); continue; }
        int c; unsigned char *px = stbi_load_from_memory(d, (int)n, w, h, &c, 4);
        free(d);
        if (px && *w > 0 && *h > 0) return px;
        if (px) stbi_image_free(px);
    }
    return NULL;
}
#define BILDER_MAX 512
static struct { char id[16]; SDL_Surface *b; } bilder[BILDER_MAX];
static int bilder_n = 0, bilder_neu = 0;          /* bilder_neu: in diesem Bild schon geladen */
static char gross_id[16]; static SDL_Surface *gross_b;
static kern *K;
static const char *bild_kennung(const eintrag *f)
{
    if (f->KachelId && f->KachelId[0]) return f->KachelId;
    if (f->SmDatei && f->SmKachel && f->SmKachel->KachelId) return f->SmKachel->KachelId;
    if (f->ToolKachel && f->ToolKachel->KachelId) return f->ToolKachel->KachelId;
    const char *tx = f->TextBasis ? f->TextBasis : (f->Text ? f->Text : "");
    htreffer m = h_mt(fa, tx, "\\b((?:CUSA|PPSA)\\d{5})\\b");
    if (m.ok) return m.gr[1];
    m = h_mt(fa, tx, "\\b(NPWR\\d{5}_\\d{2})\\b");   /* Trophaeen: Kachel des Spiels (npbind.dat) */
    if (m.ok && K && mt_hat(&K->NPWR_KACHEL, m.gr[1])) return (const char *)mt_hole(&K->NPWR_KACHEL, m.gr[1]);
    return NULL;
}
static void neu_zeichnen(void);
static SDL_Surface *kachelbild(const eintrag *f, int s)   /* s = 40 (Zeile) oder 150 (Kasten) */
{
    const char *id = bild_kennung(f);
    if (!id || strlen(id) >= sizeof gross_id) return NULL;
    if (s > 40) {
        if (!strcmp(gross_id, id)) return gross_b;
        if (gross_b) { SDL_FreeSurface(gross_b); gross_b = NULL; }
        snprintf(gross_id, sizeof gross_id, "%s", id);
        int w, h; unsigned char *px = kachelbild_laden(id, &w, &h);
        if (px) { gross_b = bild_verkleinert(px, w, h, s); stbi_image_free(px); }
        return gross_b;
    }
    for (int i = 0; i < bilder_n; i++) if (!strcmp(bilder[i].id, id)) return bilder[i].b;
    if (bilder_n >= BILDER_MAX) return NULL;
    if (bilder_neu >= 2) { neu_zeichnen(); return NULL; }   /* der Rest kommt mit dem nächsten Bild */
    bilder_neu++;
    int w, h; unsigned char *px = kachelbild_laden(id, &w, &h);
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
    char *f_kopf, *f_frei, *f_sm; bool f_ok;
    volatile bool f_auto; Uint32 f_seit;     /* NEU 1.0.6: automatischer Neustart nach 10 s (abbrechbar) */
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
static void neu_zeichnen(void) { G.neu = true; }

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
    fprintf(f, "===== PS5 Cleanup %s (%s) - %04d-%02d-%02d %02d:%02d:%02d =====\r\n", VERSION_TEXT, plattform_name(),
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    for (int i = 0; i < G.log_n; i++) prot_datei_zeile(f, G.log_zeit[i], G.log[i]);
}
#endif
static void ui_log(void *c, const char *t)
{
    (void)c;
    pthread_mutex_lock(&G.m);
    if (G.log_n == G.log_cap) {
        G.log_cap = G.log_cap ? G.log_cap * 2 : 256;
        G.log = realloc(G.log, sizeof(char *) * (size_t)G.log_cap);
        G.log_zeit = realloc(G.log_zeit, sizeof(time_t) * (size_t)G.log_cap);
    }
    G.log_zeit[G.log_n] = time(NULL);
    G.log[G.log_n++] = dup(t);
    if (prot_f) prot_datei_zeile(prot_f, G.log_zeit[G.log_n - 1], t);
    if (prot_stick_f) prot_datei_zeile(prot_stick_f, G.log_zeit[G.log_n - 1], t);
    pthread_mutex_unlock(&G.m);
    printf("PS5 Cleanup: %s\n", t ? t : ""); fflush(stdout);
    plattform_protokoll_zeile(t);                  /* NEU 1.0.7: auch ins Kernel-Protokoll */
    neu_zeichnen();
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
    int fd = open(p, O_WRONLY | O_CREAT | O_EXCL | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0644);
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
    neu_zeichnen();
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
    rm_loeschen_merken();   /* NEU 1.0.7: QR-Rückmeldung - der Kern ruht hier (Arbeitsfaden, vor G.m) */
    pthread_mutex_lock(&G.m);
    free(G.f_kopf); free(G.f_frei); free(G.f_sm);
    G.f_kopf = dup(kopf); G.f_frei = dup(frei); G.f_sm = dup(sm); G.f_ok = ok;
    G.f_auto = false; G.f_seit = SDL_GetTicks();   /* da_Chris86 30.10.2026 09:01: kein Countdown - ausgeschaltet wird erst mit ✕ oder ○ */
    (void)A.auto_neustart;   /* NEU 1.0.6; da_Chris86 29.10.2026 20:03: kein Neustart, wenn die Datenbank neu aufgebaut werden muss */
    G.art = D_FERTIG; warte_antwort();
    G.f_auto = false;
    pthread_mutex_unlock(&G.m);
}
static void ui_fehler(void *c, const char *t)
{
    (void)c;
    pthread_mutex_lock(&G.m);
    dialog_frei(); G.titel = dup("PS5 Cleanup"); G.text = dup(t);
    G.art = D_FEHLER; warte_antwort();
    pthread_mutex_unlock(&G.m);
}
static void ui_liste_neu(void *c) { (void)c; neu_zeichnen(); }
/* da_Chris86 30.10.2026 09:01: keine Frage "automatisch neu starten?" mehr vor dem Löschen - am Ende wird immer ausgeschaltet (Knopf) */
static int dlg_fokus, dlg_scroll;
/* NEU 1.0.6 (da_Chris86 28.10.2026): Auswahl am Ende der Bereinigung: 0 = zurück zur Liste (○), 1 Beenden, 2 Neu starten.
 * Ausgeführt wird erst, wenn der Arbeitsfaden fertig ist (main-Schleife). */
static volatile int nach_aktion;
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
    char fehlerliste[5][48]; int fehler_n;
} rm_stand;
static Uint32 rm_such_start;
/* im Arbeitsfaden aufrufen, wenn der Kern ruht (Ende des Löschens) */
static void rm_loeschen_merken(void)
{
    pthread_mutex_lock(&rm_m);
    rm_stand.da = A.rm_da; rm_stand.gewaehlt = A.rm_gewaehlt; rm_stand.fehler = A.rm_fehler; rm_stand.teilweise = A.rm_teilweise;
    rm_stand.geschuetzt = A.rm_geschuetzt; rm_stand.unveraendert = A.rm_unveraendert; rm_stand.ab = A.rm_abgebrochen;
    rm_stand.sm_angehalten = K->SM_ANGEHALTEN; rm_stand.sm_unklar = !K->SM_ANGEHALTEN && K->SM_GRUND && K->SM_GRUND[0];
    rm_stand.fehler_n = K->RM_FEHLER_N < 0 ? 0 : K->RM_FEHLER_N > 5 ? 5 : K->RM_FEHLER_N;
    for (int i = 0; i < rm_stand.fehler_n; i++) { memcpy(rm_stand.fehlerliste[i], K->RM_FEHLER[i], sizeof rm_stand.fehlerliste[i]); rm_stand.fehlerliste[i][47] = 0; }
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
static zugriff *neue_verbindung(void *c) { (void)c; return plattform_zugriff(); }

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
    neu_zeichnen();
    if (!groessen_gemeldet && A.hg_gesamt > 0 && A.hg_n >= A.hg_gesamt) { groessen_gemeldet = 1; speicher_zeile(T("Größen fertig gezählt", "sizes counted")); }
}
static void lauf_suchen(void *x)
{
    (void)x;
    groessen_gemeldet = 0;
    ablauf_hintergrund_anhalten(&A);
    kern_verbinden(K, plattform_zugriff());
    /* Selbstschutz (NEU 1.1): eigener Ort + Homebrew Launcher, auf allen Speichern */
    kern_selbstschutz_eigener_pfad(K, eigener_ort());
    kern_selbstschutz_ermitteln(K);
    ablauf_suchen(&A, KI.modell, KI.firmware);
    pruefausgabe();
    speicher_zeile(T("Nachschauen fertig", "scan finished"));
}
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
        rm_stand.m2 = GESUCHT && K->m2Da; rm_stand.ext = GESUCHT && K->USB_SPEICHER; rm_stand.usb = GESUCHT && K->STICK_DA;
        rm_stand.dauer_s = GESUCHT ? (int)((SDL_GetTicks() - rm_such_start) / 1000) : 0;
        pthread_mutex_unlock(&rm_m);
    } else { K->RM_ERFASSEN = false; rm_loeschen_merken(); }   /* auch nach einem Abbruch (dann gibt es keinen Endbildschirm) */
    basis_schritt = T("Liste/Menü", "list/menu");
    G.beschaeftigt = false; neu_zeichnen();
    return NULL;
}
static void starte(auftrag a)
{
    if (G.beschaeftigt) return;
    G.beschaeftigt = true;
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
static int fokus_eintrag = 0;   /* Index innerhalb des Bereichs */
/* dlg_fokus - Dialog: 0 sicher, 1 gefährlich  |  Text: 0 Feld, 1 Abbrechen, 2 OK (oben deklariert) */
static int dlg_gesehen = 0;
static Uint32 dlg_seit = 0;    /* kurz nach dem Erscheinen einer Frage zählt ✕ nicht (kein versehentliches Doppel-Drücken) */
static bool protokoll_offen = false; static int prot_scroll = 0;
static bool tastatur_offen = false, tastatur_gesehen = false; static Uint32 tastatur_seit = 0;
static Uint32 kreis_seit = 0;   /* ○ gehalten seit */
static bool laeuft = true;
static char hinweis_kurz[1024];  static Uint32 hinweis_bis = 0;   /* kurze Meldung unten */
static SDL_Window *fenster;

static void kurz(const char *t) { snprintf(hinweis_kurz, sizeof hinweis_kurz, "%s", t); hinweis_bis = SDL_GetTicks() + 4000; }

static int bereich_anzahl(int k) { int n = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Kat == k) n++; return n; }
static void sicht_neu(void)
{
    sicht_n = 0;
    for (int i = 0; i < KAT_ANZ; i++) if (bereich_anzahl(ALLE_KAT[i]) > 0) sicht[sicht_n++] = ALLE_KAT[i];
    if (sicht_n == 0) for (int i = 0; i < KAT_ANZ; i++) sicht[sicht_n++] = ALLE_KAT[i];
    if (fokus_bereich >= sicht_n) fokus_bereich = sicht_n - 1;
    if (fokus_bereich < 0) fokus_bereich = 0;
}
/* NEU 1.0.7 (da_Chris86 31.10.2026: "größte zuerst"): Einträge eines Bereichs nach Größe, größte oben. Sortiert wird erst, wenn
 * alle Größen des Bereichs gezählt sind - vorher bleibt die Fundreihenfolge (die Liste springt so nur einmal). Der markierte
 * Eintrag bleibt beim Umsortieren markiert. */
static arena *oa;
static eintrag **ord_e; static int ord_n = 0, ord_cap = 0, ord_k = -1, ord_funde = -1, ord_hg = -1;
static void *ord_fe;                        /* NEU 1.0.7 (Prüfung 02.11.2026): welche Fundliste - nach neuem Nachschauen neu ordnen */
static bool *ord_ein;                       /* NEU 1.0.7 (Prüfung 02.11.2026): Zeile steht direkt unter ihrem Kopf (eingerückt) */
/* NEU 1.0.7: Sammelzeile "Reste" je Bereich */
static eintrag RESTE_ZEILE;                 /* Platzhalter in ord_e - wird nie gelöscht, steht nur für die Teile in reste_e */
static eintrag **reste_e; static int reste_n = 0; static bool reste_offen = false, ord_reste_offen = false; static int reste_offen_k = -1;
static bool ist_rest(const eintrag *f) { return f->Art == ART_REST || f->Art == ART_TR_LISTE_REST || f->Art == ART_GALERIE_REST; }
/* NEU 1.0.7: Kopf eines Teils im selben Bereich - nur für die Anzeige, was gelöscht wird, ändert sich dadurch nicht:
 *   Spieldatei -> ShadowMount-Kachel; Daten/Payload-Datei -> Tool-Kachel; Update/Zusatzinhalt -> ihr Spiel (gleiche Gruppe);
 *   im Bereich Jailbreak: Spielstand, App-Daten, Zusatzinhalt-Infos -> die Kachel mit derselben Kennung (da_Chris86 01.11.2026 23:38:
 *   "der YouTube Jailbreak und der YouTube Speicherstand sind nicht in einer Kachel, obwohl es die gleiche PPSA hat, 1650").
 * Nur wenn genau EIN Kopf passt. Ohne Regex/Arena (wird auch außerhalb des Zeichnens aufgerufen). */
static bool kennung_von(const eintrag *f, char out[10])
{
    const char *q[2] = { f->TextBasis ? f->TextBasis : "", f->PfadeBasis.n > 0 ? f->PfadeBasis.e[0] : "" };
    for (int s = 0; s < 2; s++) for (const char *p = q[s]; *p; p++) {
        if (strncmp(p, "CUSA", 4) && strncmp(p, "PPSA", 4)) continue;
        char v = p > q[s] ? p[-1] : ' ';
        if ((v >= 'A' && v <= 'Z') || (v >= 'a' && v <= 'z') || (v >= '0' && v <= '9')) continue;
        int d = 0; while (d < 5 && p[4 + d] >= '0' && p[4 + d] <= '9') d++;
        char n = d == 5 ? p[9] : 'x';
        if (d == 5 && !((n >= 'A' && n <= 'Z') || (n >= 'a' && n <= 'z') || (n >= '0' && n <= '9'))) { memcpy(out, p, 9); out[9] = 0; return true; }
    }
    return false;
}
static eintrag *kopf_von(const eintrag *f, int k)
{
    if (f->SmDatei && f->SmKachel && f->SmKachel->Kat == k) return f->SmKachel;
    if (kern_bereiche_106()) return NULL;
    if (f->ToolKachel && f->ToolKachel->Kat == k) return f->ToolKachel;
    eintrag *h = NULL; int n = 0;
    if (f->Teil && strcmp(f->Teil, "Spiel") && f->Gruppe) {   /* Update / Zusatzinhalt */
        for (int i = 0; i < K->Funde.n; i++) { eintrag *g = K->Funde.e[i];
            if (g != f && g->Kat == k && ist_teil(g, "Spiel") && g->Gruppe && !strcmp(g->Gruppe, f->Gruppe)) { h = g; n++; } }
        return n == 1 ? h : NULL;
    }
    char id[10];
    if (k == 7 && !(f->KachelId && f->KachelId[0]) && !f->SmDatei && !ist_teil(f, "Spiel") && kennung_von(f, id)) {
        for (int i = 0; i < K->Funde.n; i++) { eintrag *g = K->Funde.e[i];
            if (g != f && g->Kat == 7 && g->KachelId && !strcmp(g->KachelId, id)) { h = g; n++; } }
        return n == 1 ? h : NULL;
    }
    return NULL;
}
static void ordnung(int k)
{
    if (reste_offen_k != k) { reste_offen = false; reste_offen_k = k; }
    if (k == ord_k && K->Funde.n == ord_funde && (void *)K->Funde.e == ord_fe && A.hg_n == ord_hg && ord_reste_offen == reste_offen) return;
    ord_reste_offen = reste_offen;
    eintrag *alt = (k == ord_k && fokus_eintrag >= 0 && fokus_eintrag < ord_n) ? ord_e[fokus_eintrag] : NULL;
    ord_k = k; ord_funde = K->Funde.n; ord_fe = (void *)K->Funde.e; ord_hg = A.hg_n;
    if (ord_cap < K->Funde.n + 1) { ord_cap = K->Funde.n + 64; ord_e = realloc(ord_e, sizeof(eintrag *) * (size_t)ord_cap); }
    if (oa) arena_frei(oa);
    oa = arena_neu();
    long long *by = a_hole(oa, sizeof(long long) * (size_t)(K->Funde.n + 1));
    bool alle = true; ord_n = 0;
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i]; if (f->Kat != k) continue;
        groesse *g = kern_eintrag_groesse_in(K, oa, f, true);
        if (!g) alle = false; else by[ord_n] = g->Bytes;
        ord_e[ord_n++] = f;
    }
    /* NEU 1.0.7 (da_Chris86 01.11.2026 abends: "wie bei Spielen sollen die Apps auch zusammengehören ... die Kachel war an Position 3,
     * die Elf-Datei an Position 12"): Teile stehen direkt unter ihrem Kopf - die Spieldatei unter ihrer ShadowMount-Kachel, Daten und
     * Payload-Dateien unter ihrer Tool-Kachel. Sortiert wird nach der Größe der ganzen Gruppe (größte oben, stabil). */
    eintrag **kh = a_hole(oa, sizeof(eintrag *) * (size_t)(ord_n + 1));   /* Kopf je Eintrag, einmal bestimmt */
    for (int i = 0; i < ord_n; i++) kh[i] = kopf_von(ord_e[i], k);
    if (alle) {
        long long *gb = a_hole(oa, sizeof(long long) * (size_t)(ord_n + 1));
        for (int i = 0; i < ord_n; i++) gb[i] = by[i];
        for (int i = 0; i < ord_n; i++) if (kh[i]) for (int j = 0; j < ord_n; j++) if (ord_e[j] == kh[i]) { gb[j] += by[i]; break; }
        for (int i = 1; i < ord_n; i++) {
            eintrag *e = ord_e[i], *h = kh[i]; long long b = gb[i]; int j = i - 1;
            while (j >= 0 && gb[j] < b) { ord_e[j + 1] = ord_e[j]; gb[j + 1] = gb[j]; kh[j + 1] = kh[j]; j--; }
            ord_e[j + 1] = e; gb[j + 1] = b; kh[j + 1] = h;
        }
    }
    {
        int m = 0; eintrag **tmp = a_hole(oa, sizeof(eintrag *) * (size_t)(ord_n + 1));
        for (int i = 0; i < ord_n; i++) {
            if (kh[i]) continue;
            tmp[m++] = ord_e[i];
            /* NEU 1.0.7 (03.11.2026, Codex-Prüfung 62): unter einer Trophäen-Liste zuerst die weiteren Listen, dann der Fortschritt -
             * innerhalb davon nach Größe */
            for (int dg = 0; dg < 2; dg++)
                for (int j = 0; j < ord_n; j++) if (kh[j] == ord_e[i] && (dg == 0) == (ord_e[j]->Art == ART_TR_LISTE || ord_e[j]->Art == ART_TR_LISTE_REST)) tmp[m++] = ord_e[j];
        }
        if (m == ord_n) memcpy(ord_e, tmp, sizeof(eintrag *) * (size_t)ord_n);
    }
    /* NEU 1.0.7 (Grundlage 31.10.2026, Punkt 4): Harmloses (grün, "es geht nichts verloren") als EINE Zeile "Reste" ganz unten -
     * die Einzelteile erst beim Aufklappen (→ auf der Zeile). Ab zwei Teilen; in Spiele und Jailbreak nie. */
    reste_n = 0;
    if (!kern_bereiche_106() && k != 1 && k != 7) {
        int m = 0; eintrag **rest = a_hole(oa, sizeof(eintrag *) * (size_t)(ord_n + 1));
        /* Codex-Prüfung 62: ein Kopf mit Teilen darunter (Trophäen-Liste eines entfernten Spiels mit Fortschritt) bleibt bei seinem Block */
        bool *kinder = a_hole(oa, sizeof(bool) * (size_t)(ord_n + 1));
        for (int i = 0; i < ord_n; i++) { kinder[i] = false; for (int j = 0; j < ord_n && !kinder[i]; j++) if (j != i && kopf_von(ord_e[j], k) == ord_e[i]) kinder[i] = true; }
        #define ALS_REST(i) (ist_rest(ord_e[i]) && !kopf_von(ord_e[i], k) && !kinder[i])
        for (int i = 0; i < ord_n; i++) if (ALS_REST(i)) rest[reste_n++] = ord_e[i];
        if (reste_n >= 2) {
            for (int i = 0; i < ord_n; i++) if (!ALS_REST(i)) ord_e[m++] = ord_e[i];
            ord_e[m++] = &RESTE_ZEILE;
            if (reste_offen) for (int i = 0; i < reste_n; i++) ord_e[m++] = rest[i];
            ord_n = m;
            reste_e = rest;
        } else reste_n = 0;
        #undef ALS_REST
    }
    /* NEU 1.0.7 (Prüfung 02.11.2026): eingerückt mit Strich ("gehört zur Zeile darüber") nur, was wirklich direkt unter seinem Kopf
     * steht - klappt die Gruppierung nicht (Kopf in einem anderen Bereich oder bei den Resten), steht die Zeile für sich */
    ord_ein = a_hole(oa, sizeof(bool) * (size_t)(ord_n + 1));
    {
        eintrag *kopf = NULL;
        for (int i = 0; i < ord_n; i++) {
            eintrag *f = ord_e[i];
            if (f == &RESTE_ZEILE) { ord_ein[i] = false; kopf = NULL; continue; }
            eintrag *h = kopf_von(f, k);
            ord_ein[i] = h != NULL && h == kopf;
            if (!h) kopf = f;
        }
    }
    if (alt) for (int i = 0; i < ord_n; i++) if (ord_e[i] == alt) fokus_eintrag = i;
}
static bool unter_kopf(int i) { return ord_ein && i >= 0 && i < ord_n && ord_ein[i]; }
static int bereich_zeilen(int k) { ordnung(k); return ord_n; }
static eintrag *bereich_eintrag(int k, int idx)
{
    ordnung(k);
    return idx >= 0 && idx < ord_n ? ord_e[idx] : NULL;
}
static int gewaehlt_anzahl(void) { int n = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Wahl) n++; return n; }

/* =================================================================== Bildschirme */
static void kopf(void)
{
    /* NEU 1.0.7 (Vorlage doku/ui_vorlage_1.0.7.png): flacher Kopf ohne Karte, Version als Abzeichen - Platz für die Infoleiste */
    if (logo) { SDL_Rect z = {48, 26, 92, 92}; SDL_BlitScaled(logo, NULL, ziel, &z); }
    text(160, 74, 1, 46, F_TEXT, "PS5 Cleanup");
    int vx = 160 + textbreite(1, 46, "PS5 Cleanup") + 18, vb = textbreite(1, 20, VERSION_TEXT) + 24;
    rechteck(vx, 44, vb, 34, F_BLAU);
    text(vx + 12, 69, 1, 20, 0xFFFFFF, VERSION_TEXT);
    const char *k = T("Löscht nur, was du auswählst. Kein Zurücksetzen, keine Neuinstallation: Systemsoftware, Laufwerks-Kopplung, Konto und echte Lizenzen bleiben. Benutzung auf eigene Gefahr - Gelöschtes ist endgültig weg.",
                      "Deletes only what you select - single games, saved data, trophies or everything at once. No reset, no reinstall: system software, disc drive pairing, account and real licenses stay. Use at your own risk - deleted data is gone for good.");
    absatz(162, 104, 0, 17, F_GRAU, k, 1220, 21);
    /* dauerhaft: SIMULATION (Release-Bau: kein Hinweisfeld) */
#ifndef KERN_LOESCHEN
    karte(1420, 28, 460, 92, F_GELB, F_GELB_HG);
#endif
#ifdef KERN_LOESCHEN
#elif defined(NUR_LESEN)
    text(1444, 68, 1, 30, F_GELB, T("TEST: NUR LESEN", "TEST: READ ONLY"));
    text(1444, 102, 0, 19, F_GELB, T("Es wird nur nachgeschaut - nichts verändert.", "Only looking - nothing is changed."));
#else
    text(1444, 68, 1, 30, F_GELB, "SIMULATION");
    text(1444, 102, 0, 19, F_GELB, T("Es wird nichts gelöscht - nur angezeigt.", "Nothing is deleted - display only."));
#endif
}

static uint32_t kopplung_farbe(void)
{
    switch (kern_kopplungs_stufe(K)) { case KOPPLUNG_BESTAETIGT: return F_MINT; case KOPPLUNG_UNKLAR: return F_GELB; default: return 0xC9CCD4; }
}
/* NEU 1.0.7 (da_Chris86 01.11.2026 abends, abgeschnittene Texte): statt der langen Liste der Einträge, die zum Jailbreak gehören
 * (stehen alle unter "Jailbreak", die volle Warnung kommt beim Anhaken), ein kurzer Satz mit der Anzahl */
static const char *jb_kurz(const char *warn)
{
    if (!warn || kern_bereiche_106()) return warn;
    int nj = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Jailbreak) nj++;
    if (nj == 0) return warn;
    return a_fmt(fa, T("Nicht löschen, wenn du ihn behalten willst - die %d Einträge dafür stehen unter „Jailbreak“.", "Do not delete if you want to keep it - its %d entries are under “Jailbreak”."), nj);
}
/* ZeigeKonsole/ZeigeKopplung/ZeigeSpeicher aus 1.0.5 */
static void konsole_karte(int x, int y, int w, int h, bool kompakt)
{
    karte(x, y, w, h, F_RAND, F_KARTE);
    int yy = y + 40;
    text(x + 24, yy, 1, 20, F_GRAU, T("KONSOLE", "CONSOLE")); yy += 44;
    const char *reihe = kern_modell_reihe(K->MODELL_TYP, K->MODELL_DIGITAL);
    text(x + 24, yy, 1, 30, F_TEXT, K->MODELL_TYP[0] ? reihe : T("Unbekanntes Modell", "Unknown model")); yy += 34;
    /* NEU 1.0.6 (da_Chris86 28.10.2026): "CFI-7021 · Europa · seit Nov. 2024"; "Erkannt" nur noch, wenn das Modell fehlt */
    text(x + 24, yy, 0, 21, F_GRAU, KI.modell[0] ? kern_modell_zeile(fa, KI.modell) : T("Modell nicht erkannt - Angaben ohne Gewähr", "Model not detected - no guarantee for the details")); yy += 38;
    text(x + 24, yy, 0, 21, F_GRAU, "Firmware"); text(x + 200, yy, 1, 21, F_TEXT, KI.firmware[0] ? KI.firmware : T("unbekannt", "unknown")); yy += 50;
    /* NEU 1.0.6 (da_Chris86 28.10.2026 23:16): welcher Jailbreak-Einstieg liegt drauf - und was man dafür nicht löschen darf */
    /* da_Chris86 29.10.2026 19:12: zeigt, was EINGERICHTET ist - nicht, womit gerade gestartet wurde */
    text(x + 24, yy, 1, 20, F_GRAU, T("JAILBREAK - EINGERICHTET", "JAILBREAK - SET UP")); yy += 34;
    if (!GESUCHT || !ablauf_verbunden(&A) || G.beschaeftigt) { text(x + 24, yy, 0, 21, F_GRAU, T("Wird beim Nachschauen erkannt.", "Detected when scanning.")); yy += 46; }
    else {
        const char *warn = NULL, *jb = kern_jailbreak_erkannt(K, fa, &warn);
        if (!jb) {
            text(x + 24, yy, 0, 21, F_GRAU, T("Kein Jailbreak-Einstieg gefunden.", "No jailbreak entry found.")); yy += 30;
            if (warn) yy += absatz(x + 24, yy, 0, 19, F_ORANGE, warn, w - 48, 24);   /* NEU 29.10.2026: Browserspeicher */
            yy += 16;
        }
        else { yy += absatz(x + 24, yy, 1, 22, F_MINT, VERB(fa, T("Eingerichtet: ", "Set up: "), jb), w - 48, 28); yy += 2; if (warn) yy += absatz(x + 24, yy, 0, 19, F_ORANGE, jb_kurz(warn), w - 48, 24); yy += 16; }
    }
    /* Laufwerks-Kopplung */
    text(x + 24, yy, 1, 20, F_GRAU, T("LAUFWERKS-KOPPLUNG", "DISC DRIVE PAIRING")); yy += 34;
    const char *t = kern_laufwerk_text(K); const char *dp = strstr(t, ": ");
    const char *titel = dp ? a_kopie_n(fa, t, (size_t)(dp - t)) : t, *rest = dp ? dp + 2 : "";
    if (rest[0] >= 'a' && rest[0] <= 'z') { char *r2 = (char *)a_kopie_n(fa, rest, strlen(rest)); r2[0] = (char)(r2[0] - 'a' + 'A'); rest = r2; }   /* Satzanfang groß */
    text(x + 24, yy, 1, 24, kopplung_farbe(), VERB(fa, kern_kopplungs_stufe(K) == KOPPLUNG_BESTAETIGT ? "✓ " : "", titel)); yy += 30;
    if (!kompakt) { yy += absatz(x + 24, yy, 0, 19, F_GRAU, rest, w - 48, 24); }
    text(x + 24, yy + 4, 0, 18, F_GRAU, T("Wird nur gelesen, nie verändert.", "Only read, never changed."));
    if (yy + 10 > y + h) zu_lang("unter den Kartenrand (Konsole, Startbildschirm)", 18, h, T("Wird nur gelesen, nie verändert.", "Only read, never changed."));   /* NEU 1.0.7: PC-Prüfung */
}
static void speicher_karte(int x, int y, int w, int h)
{
    karte(x, y, w, h, F_RAND, F_KARTE);
    int yy = y + 40;
    text(x + 24, yy, 1, 20, F_GRAU, T("SPEICHER", "STORAGE")); yy += 42;
    /* NEU 1.0.6 (da_Chris86 28.10.2026): nur die Anzahl; weiß = gefunden, grau = nicht angeschlossen, gelb = unklar */
    const char *nl[4] = { T("Intern", "Internal"), T("M.2-SSD", "M.2 SSD"), T("USB-Geräte", "USB devices"), "ShadowMount" };   /* Codex-Prüfung 48: nicht nur "USB" */
    const char *wt[4]; uint32_t fb[4] = { F_TEXT, F_TEXT, F_TEXT, F_TEXT };
    if (!GESUCHT || !ablauf_verbunden(&A)) { for (int i = 0; i < 4; i++) wt[i] = "–"; }
    else {
        int n[4] = { 0, 0, 0, 0 }, sm = 0;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Ort >= 0 && f->Ort <= 3) n[f->Ort]++; if (f->Quelle[0]) sm++; }
        wt[0] = VERB(fa, a_fmt(fa, "%d", n[ORT_INTERN]), EINTRAEGE(n[ORT_INTERN]));
        if (!K->m2Da) { wt[1] = T("nicht eingebaut", "not installed"); fb[1] = F_GRAU; }
        else if (K->M2_OHNE_USER) { wt[1] = T("leer oder nicht richtig erkannt", "empty or not detected properly"); fb[1] = F_GELB; }
        else wt[1] = VERB(fa, a_fmt(fa, "%d", n[ORT_M2]), EINTRAEGE(n[ORT_M2]));
        const char *u = "";
        if (K->USB_SPEICHER) u = VERB(fa, T("erweiterter Speicher · ", "extended storage · "), a_fmt(fa, "%d", n[ORT_USB]), EINTRAEGE(n[ORT_USB]));
        if (K->STICK_DA) u = VERB(fa, u, u[0] ? "  ·  " : "", T("USB-Laufwerk · ", "USB drive · "), a_fmt(fa, "%d", n[ORT_STICK]), n[ORT_STICK] == 1 ? T(" Datei", " file") : T(" Dateien", " files"));
        if (!u[0]) { u = T("nicht angeschlossen", "not connected"); fb[2] = F_GRAU; }
        wt[2] = u;
        if (sm > 0) wt[3] = VERB(fa, a_fmt(fa, "%d", sm), sm == 1 ? T(" Spiel", " game") : T(" Spiele", " games"));
        else { wt[3] = T("keine Spiele", "no games"); fb[3] = F_GRAU; }
    }
    for (int i = 0; i < 4; i++) { text(x + 24, yy, 0, 21, F_GRAU, nl[i]); text(x + 230, yy, 1, 21, fb[i], kuerzen(1, 21, wt[i], w - 254)); yy += 32; }
    if (GESUCHT && ablauf_verbunden(&A)) {
        const char *hw = h_trim(fa, kern_speicher_hinweis(K));
        if (hw[0]) { yy += 8; absatz(x + 24, yy + 4, 0, 18, F_GELB, hw, w - 48, 23); }
    }
}

static void fuss(const char **knoepfe, int n)
{
    rechteck(0, HOEHE - 64, BREITE, 64, F_KARTE);
    rechteck(0, HOEHE - 64, BREITE, 2, F_RAND);
    int x = 60, ende = 60;
    for (int i = 0; i < n; i++) ende += textbreite(0, 22, knoepfe[i]) + 56;
    bool hw = hinweis_bis > SDL_GetTicks();
    int hb = hw ? textbreite(1, 22, hinweis_kurz) : 0;
    /* da_Chris86 29.10.2026 (Fernsehtest): eine lange Meldung (z. B. die Jailbreak-Warnung) überlagerte die Tastenhilfe -
     * passt sie nicht daneben, steht sie allein in der Zeile (links beginnend, notfalls gekürzt) */
    bool allein = hw && ende - 56 + 40 > BREITE - 60 - hb;
    if (!allein) for (int i = 0; i < n; i++) { text(x, HOEHE - 22, 0, 22, F_GRAU, knoepfe[i]); x += textbreite(0, 22, knoepfe[i]) + 56; }
    if (allein) { passend hp = einpassen(1, 22, 18, 16, hinweis_kurz, BREITE - 120, 2); hp.lh = hp.gr + 4; passend_zeichnen(60, hp.z.n > 1 ? HOEHE - 38 : HOEHE - 22, 1, F_GELB, &hp); }   /* NEU 1.0.7: ganz, notfalls zweizeilig */
    else if (hw) text(BREITE - 60 - hb, HOEHE - 22, 1, 22, F_GELB, hinweis_kurz);
    if (kreis_seit && !G.beschaeftigt) {
        int anteil = (int)(SDL_GetTicks() - kreis_seit); if (anteil > 1000) anteil = 1000;
        rechteck(0, HOEHE - 6, BREITE * anteil / 1000, 6, F_GRUEN);
    }
}

static void schirm_start(void)
{
    kopf();
    konsole_karte(40, 176, 900, 520, false);
    speicher_karte(980, 176, 900, 300);
    bool fertig = GESUCHT && ablauf_verbunden(&A);
    karte(980, 500, 900, 196, F_BLAU, F_KARTE);
    if (!fertig) {
        text(1010, 552, 1, 30, F_TEXT, T("✕  Nachschauen, was drauf ist", "✕  Scan what is on it"));
        absatz(1010, 594, 0, 21, F_GRAU, T("Es wird nur gelesen - dabei wird nichts verändert. Danach kannst du auswählen, was weg soll.",
                                           "This only reads - nothing is changed. Afterwards you can choose what should go."), 840, 27);
    } else {
        text(1010, 552, 1, 30, F_TEXT, T("✕  Weiter zur Auswahl", "✕  Continue to selection"));
        const char *z = K->Funde.n == 0 ? T("Nichts zu tun - die PS5 ist bereits sauber.", "Nothing to do - the PS5 is already clean.")
                                        : VERB(fa, a_fmt(fa, "%d", K->Funde.n), EINTRAEGE(K->Funde.n), T(" gefunden · nichts verändert", " found · nothing changed"));
        text(1010, 600, 0, 22, F_GRAU, z);
        text(1010, 644, 0, 20, F_GRAU, T("△  Nochmal nachschauen", "△  Scan again"));
    }
    karte(40, 720, 1840, 270, F_RAND, F_KARTE);
    text(70, 764, 1, 20, F_GRAU, T("SO GEHT ES", "HOW IT WORKS"));
    const char *schritte[5] = { T("1. Starten", "1. Start"), T("2. Konsole und Speicher prüfen", "2. Check console and storage"), T("3. Bereinigung auswählen", "3. Choose what to clean up"),
                                T("4. Ergebnis kontrollieren", "4. Review the result"), T("5. Bestätigen", "5. Confirm") };
    for (int i = 0; i < 5; i++) text(70 + i * 360, 812, 1, 22, (fertig ? i == 1 : i == 0) ? F_BLAU_HELL : F_GRAU, schritte[i]);
#ifdef NUR_LESEN
    absatz(70, 860, 0, 20, F_GELB, T("Testversion: Es wird nur nachgeschaut, was auf der PS5 liegt. Löschen ist gesperrt - auch als Simulation.",
                                     "Test version: it only looks at what is on the PS5. Deleting is locked - even as a simulation."), 1780, 26);
    if (0)
#endif
#ifdef KERN_LOESCHEN
    absatz(70, 860, 0, 20, F_GRAU, T("PS5 Cleanup bleibt während der Bereinigung erhalten und geht ganz zum Schluss mit. Gelöscht wird erst, wenn du am Ende ausdrücklich bestätigst.",
                                     "PS5 Cleanup stays during the cleanup and is removed at the very end. Nothing is deleted until you explicitly confirm at the end."), 1780, 26);
#else
    absatz(70, 860, 0, 20, F_GRAU, T("In dieser Version ist alles eine Simulation: Du siehst genau, was passieren würde - gelöscht wird nichts.",
                                     "In this version everything is a simulation: you see exactly what would happen - nothing is deleted."), 1780, 26);
#endif
    if (!fertig) { const char *kn[4] = { T("✕ Nachschauen", "✕ Scan"), SP_EN ? "□ Deutsch" : "□ English", T("Options: Protokoll & Rückmeldung", "Options: log & feedback"), T("○ halten: Beenden", "hold ○: Quit") }; fuss(kn, 4); }
    else { const char *kn[4] = { T("✕ Weiter zur Auswahl", "✕ Continue"), T("△ Nochmal nachschauen", "△ Scan again"), T("Options: Protokoll & Rückmeldung", "Options: log & feedback"), T("○ halten: Beenden", "hold ○: Quit") }; fuss(kn, 4); }
}

/* =================================================================== 1.0.7: Auswahl-Bildschirm nach Vorlage
 * (doku/ui_vorlage_1.0.7.png + HTML-Vorschau, Farben von 1.0.6): Infoleiste, links Bereiche + Legende + Jailbreak getrennt,
 * rechts Einträge mit Ampel/Art/Größe und Kasten mit drei Fragen, unten die Auswahl-Leiste. */
static void punkt(int cx, int cy, int r, uint32_t farbe)
{
    for (int dy = -r; dy <= r; dy++) { int dx = 0; while ((dx + 1) * (dx + 1) + dy * dy <= r * r) dx++; rechteck(cx - dx, cy + dy, 2 * dx + 1, 1, farbe); }
}
/* Ampel "Was passiert danach?" - VORLÄUFIG (Plan 1.0.7 Schritt 4): wird mit der abgestimmten Textliste je Eintragsart ersetzt.
 * Bis dahin vorsichtig: grün nur, wo PS5 Cleanup selbst schon "kann weg" sagt. */
enum { AMP_GRUEN = AMPEL_GRUEN, AMP_GELB = AMPEL_GELB, AMP_ROT = AMPEL_ROT };
/* NEU 1.0.7: Ampel aus dem Kern (Art des Eintrags, kern_art.c) - laut abgestimmter Textliste (Freigabe da_Chris86 31.10.2026 13:16) */
static int ampel(const eintrag *f) { return kern_ampel(f); }
static uint32_t ampel_farbe(int a) { return a == AMP_GRUEN ? F_GRUEN : a == AMP_GELB ? F_GELB : 0xE5484D; }
static const char *ampel_text(int a)
{
    return a == AMP_GRUEN ? T("harmlos, es geht nichts verloren", "harmless, nothing is lost")
         : a == AMP_GELB  ? T("neu einrichten oder neu laden", "set up again or download again")
                          : T("endgültig weg", "gone for good");
}
/* Name und Art aus dem Text: "Altes Spiel (CUSA12345) - Update (Version 01.09)" -> Name bis zur Klammer, Art dahinter.
 * Ohne dieses Muster bleibt der ganze Text der Name (keine erfundene Art). */
static void name_art(const eintrag *f, const char **name, const char **art)
{
    *name = f->Text; *art = "";
    const char *p = strstr(f->Text, ") - ");
    if (p && p[4]) { *name = a_kopie_n(fa, f->Text, (size_t)(p + 1 - f->Text)); *art = p + 4; }
    /* Marken wie "  [GESPERRT: …]" / "  [Jailbreak-Einstieg - nur einzeln]" nur im Kasten unten (voller Text), nicht in der Zeile */
    const char *m = strstr(*name, "  ["); if (m) *name = a_kopie_n(fa, *name, (size_t)(m - *name));
    m = strstr(*art, "  ["); if (m) *art = a_kopie_n(fa, *art, (size_t)(m - *art));
}
/* Platzhalter-Bild (bis die Kachelbilder kommen): zwei Buchstaben des Namens */
static void bildchen(int x, int y, int gr, const eintrag *f, int schrift)
{
    SDL_Surface *b = kachelbild(f, gr);   /* NEU 1.0.7: Kachelbild, wenn es eins gibt */
    if (b) { SDL_Rect z = { x, y, gr, gr }; SDL_BlitSurface(b, NULL, ziel, &z); return; }
    karte(x, y, gr, gr, F_RAND, F_KARTE2);
    const char *n = f->Text, *v = kern_ort_vorsatz(f->Ort);
    if (v && v[0] && strncmp(n, v, strlen(v)) == 0) n += strlen(v);
    char kz[3] = {0}; int i = 0;
    for (const char *q = n; *q && i < 2; q++) { char c = *q; if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) kz[i++] = (char)((c >= 'a' && c <= 'z') ? c - 32 : c); else if (i == 1) break; }
    if (!kz[0]) { kz[0] = '?'; }
    int tw = textbreite(1, schrift, kz);
    text(x + (gr - tw) / 2, y + gr / 2 + schrift * 7 / 20, 1, schrift, F_GRAU, kz);
}
static const char *groesse_von(arena *ar, long long b, bool gekappt) { return VERB(ar, gekappt ? T("mind. ", "min. ") : "", kern_groesse_text(ar, b)); }
static const char *bereich_unterzeile(int k)
{
    int n = 0; long long b = 0; bool alle = true, gek = false;
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i]; if (f->Kat != k) continue;
        n++; groesse *g = kern_eintrag_groesse_in(K, fa, f, true);
        if (!g) alle = false; else { b += g->Bytes; if (g->Gekappt) gek = true; }
    }
    const char *t = VERB(fa, a_fmt(fa, "%d", n), EINTRAEGE(n));
    if (n > 0) t = VERB(fa, t, "  ·  ", alle ? groesse_von(fa, b, gek) : T("Größe wird gezählt …", "counting size …"));
    return t;
}

/* ---- Infoleiste */
static void info_leiste(int y, int h)
{
    bool da = GESUCHT && ablauf_verbunden(&A) && !G.beschaeftigt;
    /* Konsole - NEU 1.0.7 (da_Chris86 01.11.2026 abends: "die halbe Kachel ist leer - entweder größer oder besser verteilt"):
     * links die Reihe groß, rechts die Firmware groß, darunter Modell · Region · seit über die ganze Breite. Nichts wird gekürzt. */
    const char *reihe = K->MODELL_TYP[0] ? kern_modell_reihe(K->MODELL_TYP, K->MODELL_DIGITAL) : T("Unbekanntes Modell", "Unknown model");
    const char *mz = KI.modell[0] ? kern_modell_zeile(fa, KI.modell) : T("Modell nicht erkannt", "Model not detected");
    const char *fw = KI.firmware[0] ? KI.firmware : T("unbekannt", "unknown");
    int bf = textbreite(1, 28, fw); if (textbreite(1, 17, "FIRMWARE") > bf) bf = textbreite(1, 17, "FIRMWARE");
    int w = textbreite(1, 28, reihe) + 48 + bf, w2 = textbreite(0, 19, mz);
    if (w2 > w) w = w2;
    w += 44; if (w < 400) w = 400; if (w > 560) w = 560;
    int x = 40;
    karte(x, y, w, h, F_RAND, F_KARTE);
    text(x + 22, y + 32, 1, 17, F_GRAU, T("KONSOLE", "CONSOLE"));
    text(x + w - 22 - textbreite(1, 17, "FIRMWARE"), y + 32, 1, 17, F_GRAU, "FIRMWARE");
    text_passend(x + 22, y + 70, 1, 28, 18, F_TEXT, reihe, w - 44 - bf - 24);
    text(x + w - 22 - textbreite(1, 28, fw), y + 70, 1, 28, F_TEXT, fw);
    text_passend(x + 22, y + 98, 0, 19, 14, F_GRAU, mz, w - 44);
    /* Laufwerk */
    x = x + w + 16; w = 360;
    karte(x, y, w, h, F_RAND, F_KARTE);
    text(x + 22, y + 32, 1, 17, F_GRAU, T("LAUFWERK", "DISC DRIVE"));
    const char *t = kern_laufwerk_text(K); const char *dp = strstr(t, ": ");
    const char *titel = dp ? a_kopie_n(fa, t, (size_t)(dp - t)) : t;
    text_passend(x + 22, y + 66, 1, 24, 16, kopplung_farbe(), VERB(fa, kern_kopplungs_stufe(K) == KOPPLUNG_BESTAETIGT ? "✓ " : "", titel), w - 44);
    text_passend(x + 22, y + 94, 0, 18, 14, F_GRAU, T("wird nur gelesen, nie verändert", "only read, never changed"), w - 44);
    /* Jailbreak: was EINGERICHTET ist (nicht, womit gestartet wurde). NEU 1.0.7 (da_Chris86 01.11.2026, Foto: abgeschnitten): die
     * Namen notfalls auf zwei Zeilen; statt der langen Liste der Einträge (steht unter "Jailbreak" und in der Warnung beim Anhaken)
     * ein kurzer Satz mit der Anzahl */
    x = x + w + 16; w = 1508 - 16 - x;
    const char *warn = NULL, *jb = da ? kern_jailbreak_erkannt(K, fa, &warn) : NULL;
    karte(x, y, w, h, jb ? F_ORANGE : F_RAND, F_KARTE);
    text(x + 22, y + 32, 1, 17, jb ? F_ORANGE : F_GRAU, jb ? T("JAILBREAK ERKANNT", "JAILBREAK DETECTED") : "JAILBREAK");
    if (!da) text(x + 22, y + 66, 0, 21, F_GRAU, T("wird beim Nachschauen erkannt", "detected when scanning"));
    else if (!jb) {
        text(x + 22, y + 62, 0, 20, F_GRAU, T("kein Einstieg gefunden", "no entry found"));
        if (warn) { passend wp = einpassen(0, 16, 15, 14, warn, w - 44, 2); wp.lh = wp.gr + 3; passend_zeichnen(x + 22, y + 84, 0, F_ORANGE, &wp); }
    } else {
        passend jp = einpassen(1, 22, 18, 16, jb, w - 44, 2); jp.lh = jp.gr + 4;
        passend_zeichnen(x + 22, jp.z.n > 1 ? y + 58 : y + 66, 1, F_TEXT, &jp);
        const char *hw = warn ? jb_kurz(warn) : T("geschützt - nur einzeln löschbar", "protected - delete individually only");
        if (warn && kern_bereiche_106()) { passend wp = einpassen(0, 17, 15, 14, hw, w - 44, 1); passend_zeichnen(x + 22, y + 100, 0, F_ORANGE, &wp); }
        else text_passend(x + 22, y + 100, 0, 17, 14, warn ? F_ORANGE : F_GRAU, hw, w - 44);
    }
    /* Speicher frei (alle 5 s neu gelesen) */
    x = 1508; w = 372;
    karte(x, y, w, h, F_RAND, F_KARTE);
    text(x + 22, y + 32, 1, 17, F_GRAU, T("SPEICHER FREI", "FREE STORAGE"));
    static Uint32 sp_zeit = 0; static bool sp_da[4]; static long long sp_frei[4], sp_ges[4];
    if (!sp_zeit || SDL_GetTicks() - sp_zeit > 5000) { sp_zeit = SDL_GetTicks() | 1; for (int i = 0; i < 4; i++) sp_da[i] = plattform_speicher(i, &sp_frei[i], &sp_ges[i]); }
    const char *nl[4] = { T("Intern", "Internal"), "M.2", T("Erw. Speicher", "Ext. storage"), T("USB-Laufwerk", "USB drive") };   /* 03.11.2026: nicht nur "USB" */
    int zy = y + 56, gezeigt = 0;
    for (int i = 0; i < 4 && gezeigt < 3; i++) {
        if (!sp_da[i]) continue;
        text(x + 22, zy + 6, 0, 17, F_GRAU, nl[i]);
        const char *fw = kern_groesse_text(fa, sp_frei[i]);
        if (sp_frei[i] >= 1024LL * 1024 * 1024 * 1024) {   /* ab 1 TB als TB (sonst "1.024,5 GB") */
            char *t = (char *)a_fmt(fa, "%.1f TB", (double)sp_frei[i] / (1024.0 * 1024 * 1024 * 1024));
            for (char *c = t; *c; c++) if (*c == '.' && !SP_EN) *c = ',';
            fw = t;
        }
        text(x + 200 - textbreite(1, 17, fw), zy + 6, 1, 17, F_TEXT, fw);
        int bx = x + 214, bw = w - 22 - 214, an = sp_ges[i] > 0 ? (int)((sp_ges[i] - sp_frei[i]) * bw / sp_ges[i]) : 0;
        rechteck(bx, zy - 4, bw, 10, F_KARTE2);
        rechteck(bx, zy - 4, an, 10, sp_frei[i] * 10 < sp_ges[i] ? F_ORANGE : F_BLAU);
        zy += 22; gezeigt++;
    }
    if (!gezeigt) text(x + 22, y + 66, 0, 20, F_GRAU, T("nicht lesbar", "not readable"));
}

/* ---- links */
static void bereich_zeile(int x, int yy, int w, int hz, int k, bool fok, bool jbk)
{
    int n = bereich_anzahl(k);
    if (fok) rechteck(x + 8, yy, w - 16, hz - 6, fokus_zone == 0 ? (jbk ? 0x4A3020 : F_FOKUS) : F_KARTE2);
    int z = ablauf_bereich_zustand(&A, k), cy = yy + (hz - 6 - 30) / 2;
    if (z == 2) {   /* Anzeige: "voll" nur, wenn wirklich alle wählbaren Einträge angehakt sind (bei "nur einzeln" sonst halb) */
        int m = 0, w = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == k && !f->Gesperrt) { m++; if (f->Wahl) w++; } }
        if (w < m) z = 1;
    }
    karte(x + 22, cy, 30, 30, n ? F_GRAU : F_RAND, F_HG);
    uint32_t hk = jbk ? F_ORANGE : F_BLAU_HELL;
    if (z == 2) rechteck(x + 28, cy + 6, 18, 18, hk); else if (z == 1) rechteck(x + 28, cy + 12, 18, 6, hk);
    text(x + 68, yy + 30, 1, 24, !n ? F_GRAU : jbk ? F_ORANGE : F_TEXT, kern_kat_name(k));
    text_passend(x + 68, yy + 56, 0, 18, 14, F_GRAU, bereich_unterzeile(k), w - 90);
}
static void legende(int x, int y, int w, int h)
{
    karte(x, y, w, h, F_RAND, F_KARTE);
    text(x + 22, y + 30, 1, 17, F_GRAU, T("WAS PASSIERT DANACH?", "WHAT HAPPENS AFTERWARDS?"));
    for (int a = 0; a < 3; a++) { int ly = y + 58 + a * 26; punkt(x + 30, ly - 6, 7, ampel_farbe(a)); text(x + 48, ly, 0, 18, F_TEXT, ampel_text(a)); }
}

/* NEU 1.0.7 (da_Chris86 01.11.2026 abends, Foto vom Kasten: "Um was bringen mir die ganzen Zusatztexte, wenn die Hälfte davon
 * abgeschnitten wird?"): Kasten unter der Liste - jeder Text ganz, notfalls umgebrochen. detail_mass rechnet, detail_zeichnen zeichnet
 * mit denselben Werten; der Kasten ist so hoch wie der längste Eintrag des Bereichs (die Liste springt beim Blättern nicht). */
#define DETAIL_BILD 130
typedef struct { passend titel, was, weg, danach, grund; int a, lw, h; } detail_masse;
static void detail_mass(eintrag *fe, int wr, detail_masse *d)
{
    memset(d, 0, sizeof *d);
    const char *grund = NULL; bool gesp = ablauf_eintrag_gesperrt(&A, fe, &grund);
    const char *name, *art; name_art(fe, &name, &art);
    if (!kern_bereiche_106()) name = kern_zeile(fa, fe);
    d->a = ampel(fe);
    const char *weg = fe->Pfade.n > 0 ? fe->Pfade.e[0] : "";
    if (fe->Pfade.n > 1) weg = VERB(fa, weg, a_fmt(fa, T("  (+%d weitere)", "  (+%d more)"), fe->Pfade.n - 1));
    const char *danach = kern_bereiche_106() ? ampel_text(d->a) : kern_art_info(fa, fe);
    if (!danach[0]) danach = kern_folge(fe);
    if (fe->Jailbreak && kern_bereiche_106()) danach = kern_jb_hinweis(fa, fe);
    const char *l[3] = { T("Was ist das?", "What is it?"), T("Was wird gelöscht?", "What is deleted?"), T("Was passiert danach?", "What happens then?") };
    for (int i = 0; i < 3; i++) { int b = textbreite(1, 19, l[i]); if (b > d->lw) d->lw = b; }
    d->lw += 24;
    int tw = wr - (DETAIL_BILD + 46) - 24, vw = tw - d->lw;
    d->titel = einpassen(1, 26, 22, 20, name, tw, 2); d->titel.lh = d->titel.gr + 7;
    const char *was = fe->Text ? fe->Text : "";
    if (!strncmp(was, "   – ", strlen("   – "))) was += strlen("   – ");   /* Teil unter seiner Kachel: ohne Strich */
    d->was = einpassen(0, 19, 17, 16, was, vw, 3); d->was.lh = d->was.gr + 5;
    d->weg = einpassen(0, 19, 17, 16, weg, vw, 3); d->weg.lh = d->weg.gr + 5;
    d->danach = einpassen(0, 19, 17, 16, danach, vw - 22, 5); d->danach.lh = d->danach.gr + 5;
    int zy = 44 + (d->titel.z.n - 1) * d->titel.lh + 36;
    zy += d->was.z.n * d->was.lh + 6 + d->weg.z.n * d->weg.lh + 6 + d->danach.z.n * d->danach.lh;
    if (gesp && grund && grund[0]) { d->grund = einpassen(0, 18, 16, 15, grund, tw, 2); d->grund.lh = d->grund.gr + 5; zy += 4 + d->grund.z.n * d->grund.lh; }
    d->h = zy + 2;
    if (d->h < DETAIL_BILD + 46) d->h = DETAIL_BILD + 46;
}
static void detail_zeichnen(eintrag *fe, int xr, int dy, int wr)
{
    detail_masse d; detail_mass(fe, wr, &d);
    bildchen(xr + 24, dy + 22, DETAIL_BILD, fe, 40);
    int dx = xr + DETAIL_BILD + 46;
    passend_zeichnen(dx, dy + 44, 1, F_TEXT, &d.titel);
    int zy = dy + 44 + (d.titel.z.n - 1) * d.titel.lh + 36;
    text(dx, zy, 1, 19, F_GRAU, T("Was ist das?", "What is it?"));            passend_zeichnen(dx + d.lw, zy, 0, F_TEXT, &d.was); zy += d.was.z.n * d.was.lh + 6;
    text(dx, zy, 1, 19, F_GRAU, T("Was wird gelöscht?", "What is deleted?"));  passend_zeichnen(dx + d.lw, zy, 0, F_TEXT, &d.weg); zy += d.weg.z.n * d.weg.lh + 6;
    text(dx, zy, 1, 19, F_GRAU, T("Was passiert danach?", "What happens then?"));
    punkt(dx + d.lw + 7, zy - 6, 7, ampel_farbe(d.a));
    passend_zeichnen(dx + d.lw + 22, zy, 0, ampel_farbe(d.a), &d.danach); zy += d.danach.z.n * d.danach.lh;
    if (d.grund.z.n) passend_zeichnen(dx, zy + 4, 0, F_GELB, &d.grund);
}
static int bereich_zeilen(int k);
static eintrag *bereich_eintrag(int k, int idx);
static int detail_hoehe(int k, int wr, eintrag *fe)
{
    static int c_k = -1, c_n = -1, c_h = 0, c_r = -1; static bool c_en; static void *c_fe;
    if (k != c_k || K->Funde.n != c_n || (void *)K->Funde.e != c_fe || SP_EN != c_en || reste_n != c_r) {
        int h = DETAIL_BILD + 46, n = bereich_zeilen(k);
        for (int i = 0; i < n; i++) { eintrag *f = bereich_eintrag(k, i); if (!f || f == &RESTE_ZEILE) continue; detail_masse d; detail_mass(f, wr, &d); if (d.h > h) h = d.h; }
        for (int i = 0; i < reste_n; i++) { detail_masse d; detail_mass(reste_e[i], wr, &d); if (d.h > h) h = d.h; }
        c_k = k; c_n = K->Funde.n; c_fe = (void *)K->Funde.e; c_en = SP_EN; c_r = reste_n; c_h = h < 250 ? h : 250;   /* höchstens 250 - sonst zu wenig Liste */
    }
    int h = c_h;
    if (fe && fe != &RESTE_ZEILE) { detail_masse d; detail_mass(fe, wr, &d); if (d.h > h) h = d.h; }   /* z. B. ein Sperrgrund kam dazu */
    return h;
}

/* Auswahl: links Bereiche, rechts Einträge (ZeigeBereiche) */
#define ZEILE 50
static int eintrag_scroll = 0;
static void schirm_liste(void)
{
    kopf();
    ablauf_texte_frei(&A);
    info_leiste(138, 112);
    sicht_neu();
    bool jb_getrennt = !kern_bereiche_106();
    /* links: Bereiche (Spiele bis Sonstiges), darunter Legende, ganz unten Jailbreak - getrennt und orange */
    int xl = 40, wl = 560, yl = 266, hz = 76;
    karte(xl, yl, wl, 16 + 6 * hz, fokus_zone == 0 && !(jb_getrennt && sicht[fokus_bereich] == 7) ? F_BLAU : F_RAND, F_KARTE);
    int r = 0; bool jb_da = false;
    for (int i = 0; i < sicht_n; i++) {
        int k = sicht[i];
        if (jb_getrennt && k == 7) { jb_da = true; continue; }
        if (r < 7) bereich_zeile(xl, yl + 10 + r * hz, wl, hz, k, i == fokus_bereich, false);
        r++;
    }
    legende(xl, yl + 16 + 6 * hz + 16, wl, 884 - (yl + 16 + 6 * hz + 16));
    if (jb_getrennt) {
        bool fj = sicht[fokus_bereich] == 7;
        karte(xl, 900, wl, 96, fj && fokus_zone == 0 ? F_ORANGE : (jb_da ? 0x7A4A2E : F_RAND), F_KARTE);
        if (jb_da) bereich_zeile(xl, 910, wl, hz + 6, 7, fj, true);
        else { text(xl + 68, 940, 1, 24, F_GRAU, kern_kat_name(7)); text(xl + 68, 966, 0, 18, F_GRAU, T("nichts gefunden", "nothing found")); }
    }
    /* rechts: Einträge des Bereichs */
    int k = sicht[fokus_bereich];
    bool jbk = jb_getrennt && k == 7;
    int xr = 616, yr = 266, wr = BREITE - 40 - xr, hr = 618;
    karte(xr, yr, wr, hr, fokus_zone == 1 ? (jbk ? F_ORANGE : F_BLAU) : F_RAND, F_KARTE);
    text(xr + 24, yr + 42, 1, 28, jbk ? F_ORANGE : F_TEXT, kern_kat_name(k));
    /* NEU 1.0.7 (da_Chris86 01.11.2026 abends): nichts abschneiden - die Erklärung des Bereichs notfalls auf zwei Zeilen */
    passend ki = einpassen(0, 19, 17, 16, kern_kat_info(k), wr - 48, 2); ki.lh = ki.gr + 4;
    passend_zeichnen(xr + 24, yr + 72, 0, F_GRAU, &ki);
    int yy = yr + 84 + (ki.z.n - 1) * ki.lh;
    bool sm = false; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == k && f->Quelle[0]) sm = true; }   /* 1.0.7: auch unter "Jailbreak" */
    /* NEU 1.0.7 (da_Chris86 31.10.2026 12:49): keine Abfrage mehr - Kachel und Spieldatei sind eigene Zeilen */
    if (sm) {
        passend sp = einpassen(0, 18, 16, 15, kern_bereiche_106() ? T("ShadowMount-Spiele: Bei \"Nur Auswahl löschen\" wirst du gefragt, ob nur die Kachel oder auch die Spieldatei weg soll.",
                                                                      "ShadowMount games: with \"Delete selection only\" you will be asked whether only the tile or the game file too should go.")
                                                                    : T("ShadowMount-Spiele: Kachel und Spieldatei stehen getrennt - die Spieldatei geht nur weg, wenn du sie selbst anhakst.",
                                                                        "ShadowMount games: tile and game file are separate rows - the game file only goes if you tick it yourself."), wr - 48, 2);
        sp.lh = sp.gr + 4;
        passend_zeichnen(xr + 24, yy + 16, 0, F_GELB, &sp);
        yy += 26 + (sp.z.n - 1) * sp.lh;
    }
    int n = bereich_zeilen(k);
    bilder_neu = 0;
    eintrag *fe = (fokus_zone == 1) ? bereich_eintrag(k, fokus_eintrag) : NULL;
    int detail_y = yr + hr - detail_hoehe(k, wr, fe);
    int liste_y = yy + 8, platz = detail_y - 6 - liste_y, sichtbar = platz / ZEILE; if (sichtbar < 1) sichtbar = 1;
    if (fokus_eintrag < eintrag_scroll) eintrag_scroll = fokus_eintrag;
    if (fokus_eintrag >= eintrag_scroll + sichtbar) eintrag_scroll = fokus_eintrag - sichtbar + 1;
    if (n == 0) text(xr + 24, liste_y + 34, 0, 22, F_GRAU, T("Nichts gefunden - hier ist alles sauber.", "Nothing found - all clean here."));
    /* NEU 1.0.7: Spalte "Was passiert danach?" schmaler und notfalls zweizeilig - mehr Platz für die Namen (auch zweizeilig) */
    int gr_w = 140, art_w = 300;
    for (int i = eintrag_scroll; i < n && i < eintrag_scroll + sichtbar; i++) {
        eintrag *f = bereich_eintrag(k, i);
        if (!f) break;
        int ry = liste_y + (i - eintrag_scroll) * ZEILE;
        bool fok = fokus_zone == 1 && i == fokus_eintrag;
        if (f == &RESTE_ZEILE) {   /* NEU 1.0.7: Sammelzeile "Reste" */
            int w = 0, m = 0; long long b = 0; bool alle = true, gek = false;
            for (int j = 0; j < reste_n; j++) { eintrag *r = reste_e[j]; if (r->Gesperrt) continue; m++; if (r->Wahl) w++;
                groesse *g = kern_eintrag_groesse_in(K, fa, r, true); if (!g) alle = false; else { b += g->Bytes; if (g->Gekappt) gek = true; } }
            if (fok) rechteck(xr + 8, ry, wr - 16, ZEILE - 4, F_FOKUS);
            karte(xr + 20, ry + 9, 28, 28, F_GRAU, F_HG);
            if (w > 0 && w == m) rechteck(xr + 26, ry + 15, 16, 16, F_BLAU_HELL); else if (w > 0) rechteck(xr + 26, ry + 21, 16, 4, F_BLAU_HELL);
            punkt(xr + 124, ry + 23, 7, ampel_farbe(AMP_GRUEN));
            const char *nm = VERB(fa, reste_offen ? "▾ " : "▸ ", T("Reste (", "Leftovers ("), a_fmt(fa, "%d", reste_n), T(" Teile)", " items)"));
            int art_x = xr + wr - 24 - gr_w - art_w;
            text_passend(xr + 144, ry + 31, 1, 22, 16, F_TEXT, nm, art_x - 20 - (xr + 144));
            rechteck(art_x - 10, ry + 12, 2, 24, F_RAND);
            passend ap = einpassen(0, 19, 17, 16, T("es geht nichts verloren", "nothing is lost"), art_w - 10, 2);
            zeile_zeichnen(art_x + 6, ry, 0, ampel_farbe(AMP_GRUEN), &ap);
            if (alle) { const char *gr = groesse_von(fa, b, gek); text(xr + wr - 28 - textbreite(0, 20, gr), ry + 31, 0, 20, F_GRAU, gr); }
            continue;
        }
        bool einger = false;   /* aufgeklappte Teile der Reste eingerückt - Codex-Prüfung 63: nur, was wirklich in den Resten steht */
        if (reste_offen && reste_n >= 2) for (int j = 0; j < reste_n && !einger; j++) if (reste_e[j] == f) einger = true;
        const char *grund; bool gesp = ablauf_eintrag_gesperrt(&A, f, &grund);
        if (fok) rechteck(xr + 8, ry, wr - 16, ZEILE - 4, F_FOKUS);
        karte(xr + 20, ry + 9, 28, 28, gesp ? F_RAND : F_GRAU, F_HG);
        if (f->Wahl) rechteck(xr + 26, ry + 15, 16, 16, gesp ? F_GRAU : (jbk ? F_ORANGE : F_BLAU_HELL));
        /* NEU 1.0.7: Teile unter ihrem Kopf (Spieldatei, Daten und Payload-Dateien einer Tool-Kachel) und aufgeklappte Reste eingerückt */
        int ein = einger || unter_kopf(i) ? 34 : 0;   /* Prüfung 02.11.2026: nur direkt unter dem Kopf */
        if (ein) rechteck(xr + 78, ry + 8, 2, 30, F_RAND);   /* senkrechter Strich: gehört zur Zeile darüber */
        else bildchen(xr + 62, ry + 3, 40, f, 15);
        punkt(xr + 124 + (ein ? 10 : 0), ry + 23, 7, ampel_farbe(ampel(f)));
        /* NEU 1.0.7 (Grundlage 31.10.2026, Punkt 1): die Zeile sagt selbst, was weggeht und ob es wiederkommt */
        const char *name = kern_bereiche_106() ? NULL : kern_zeile(fa, f), *art = kern_bereiche_106() ? NULL : kern_folge(f);
        if (kern_bereiche_106()) name_art(f, &name, &art);
        const char *gr = h_trim(fa, ablauf_eintrag_groesse(&A, f));
        int art_x = xr + wr - 24 - gr_w - art_w, name_x = xr + 144 + ein;
        int name_w = (art[0] ? art_x - 20 : xr + wr - 24 - gr_w) - name_x;
        uint32_t fb = gesp ? F_GRAU : F_TEXT;
        /* NEU 1.0.7 (da_Chris86 01.11.2026 abends): Name ganz - etwas kleiner oder auf zwei Zeilen; nur wenn auch das nicht reicht, läuft
         * er in der markierten Zeile durch (Laufschrift) */
        passend np = einpassen(0, 22, 18, 17, name, name_w, 2);
        if (np.z.n <= 2) zeile_zeichnen(name_x, ry, 0, fb, &np);
        else if (fok) laufschrift(name_x, ry + 31, 0, 22, fb, name, name_w, f);
        else text(name_x, ry + 31, 0, 22, fb, kuerzen(0, 22, name, name_w));
        if (art[0]) {
            rechteck(art_x - 10, ry + 12, 2, 24, F_RAND);
            passend ap = einpassen(0, 19, 17, 15, art, art_w - 10, 2);
            zeile_zeichnen(art_x + 6, ry, 0, kern_bereiche_106() ? F_GRAU : ampel_farbe(ampel(f)), &ap);
        }
        if (gr[0]) text(xr + wr - 28 - textbreite(0, 20, gr), ry + 31, 0, 20, F_GRAU, gr);
    }
    if (n > sichtbar) {   /* Bildlaufleiste */
        int bh = platz * sichtbar / n, by = liste_y + (platz - bh) * eintrag_scroll / (n - sichtbar > 0 ? n - sichtbar : 1);
        rechteck(xr + wr - 10, by, 4, bh, F_RAND);
    }
    /* Kasten unten: drei Fragen zum markierten Eintrag */
    rechteck(xr + 2, detail_y, wr - 4, 2, F_RAND);
    if (fe == &RESTE_ZEILE) {   /* NEU 1.0.7: Sammelzeile "Reste" */
        int hy = detail_y + 46;
        text(xr + 24, hy, 1, 26, F_TEXT, VERB(fa, T("Reste (", "Leftovers ("), a_fmt(fa, "%d", reste_n), T(" Teile)", " items)"))); hy += 38;
        hy += absatz(xr + 24, hy, 0, 19, F_GRUEN, T("Reste von Spielen und Apps, die es nicht mehr gibt, Zwischenspeicher und leere Ordner - es geht nichts verloren.",
                                                    "Leftovers of games and apps that are gone, caches and empty folders - nothing is lost."), wr - 48, 25);
        text(xr + 24, hy + 14, 0, 19, F_GRAU, reste_offen ? T("✕ alle Reste wählen  ·  → zuklappen", "✕ select all leftovers  ·  → collapse")
                                                          : T("✕ alle Reste wählen  ·  → aufklappen und einzeln wählen", "✕ select all leftovers  ·  → expand and choose one by one"));
        fe = NULL;
    }
    else if (fe) detail_zeichnen(fe, xr, detail_y, wr);
    else {
        int hy = detail_y + 40;
        hy += absatz(xr + 24, hy, 0, 19, F_GRAU, kern_kat_info(k), wr - 48, 25);
        if (ablauf_bereich_nur_einzeln(&A, k)) text(xr + 24, hy + 14, 0, 19, jbk ? F_ORANGE : F_GELB, T("→ Einträge dieses Bereichs einzeln wählen  ·  hier nur einzeln wählbar", "→ choose entries of this area one by one  ·  individually only here"));
        else text(xr + 24, hy + 14, 0, 19, F_GRAU, T("→ Einträge dieses Bereichs einzeln wählen  ·  ✕ ganzen Bereich wählen", "→ choose entries of this area one by one  ·  ✕ select the whole area"));
    }
    /* unten: Auswahl-Leiste mit Warnung */
    int gw = 0; long long gb = 0; bool galle = true, ggek = false, rot = false, jbw = false;
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i]; if (!f->Wahl) continue;
        gw++; if (ampel(f) == AMP_ROT) rot = true; if (f->Jailbreak) jbw = true;
        groesse *g = kern_eintrag_groesse_in(K, fa, f, true); if (!g) galle = false; else { gb += g->Bytes; if (g->Gekappt) ggek = true; }
    }
    bool auswahl = gw > 0, komplett = K->Funde.n > 0 && ablauf_verbunden(&A);
#ifdef NUR_LESEN
    auswahl = komplett = false;          /* Testversion Schritt 9: nur nachschauen */
#endif
    karte(xr, 900, wr, 96, rot ? F_ROT_RAND : F_RAND, F_KARTE);
    /* NEU 1.0.7 (da_Chris86 01.11.2026 abends: "Und unten ausgewählt 65 Einträge. Ist es immer noch so eingedrückt? Man kann ja gar
     * nicht alles lesen."): die beiden Knöpfe stehen übereinander - der Text links hat doppelt so viel Platz und steht ganz da
     * (Warnung bis zu drei Zeilen) */
    int bw = 360, bx = xr + wr - 16 - bw, tw = bx - xr - 40;
    const char *st = gw == 0 ? T("Nichts ausgewählt.", "Nothing selected.")
                             : VERB(fa, T("Ausgewählt: ", "Selected: "), a_fmt(fa, "%d", gw), EINTRAEGE(gw), galle ? VERB(fa, "  ·  ", groesse_von(fa, gb, ggek)) : "");
    text_passend(xr + 24, 930, 1, 22, 16, F_TEXT, st, tw);
    const char *eg = kern_bereiche_106() ? "" : kern_endgueltig_satz(K, fa);
    const char *wt = NULL; uint32_t wf = F_GRAU;
    if (rot && eg[0]) { wt = VERB(fa, T("Achtung: ", "Caution: "), eg); wf = F_ROT_TEXT; }
    else if (rot) { wt = jbw ? T("Achtung: Jailbreak-Einstieg ausgewählt - dein Jailbreak startet danach nicht mehr so wie bisher.", "Caution: jailbreak entry selected - your jailbreak will no longer start the way it did.")
                             : T("Achtung: rot markierte Einträge sind danach endgültig weg.", "Caution: entries marked red are gone for good afterwards."); wf = F_ROT_TEXT; }
    else if (A.hintergrund && A.hg_gesamt > 0 && A.hg_n < A.hg_gesamt) wt = a_fmt(fa, T("Größen werden gezählt (%d von %d) …", "Counting sizes (%d of %d) …"), A.hg_n, A.hg_gesamt);
    else { const char *sh = kern_selbstschutz_hinweis(K); if (sh[0]) wt = sh; }
    if (wt) { passend wp = einpassen(0, 17, 16, 14, wt, tw, 3); wp.lh = wp.gr + 3; passend_zeichnen(xr + 24, 954, 0, wf, &wp); }
    karte(bx, 908, bw, 40, auswahl ? F_BLAU_HELL : F_RAND, auswahl ? F_BLAU : F_KARTE2);
    text_passend(bx + 18, 936, 1, 21, 16, auswahl ? 0xFFFFFF : F_GRAU, T("□  Nur Auswahl löschen", "□  Delete selection only"), bw - 36);
    karte(bx, 952, bw, 40, komplett ? F_ROT_RAND : F_RAND, komplett ? F_ROT : F_KARTE2);
    text_passend(bx + 18, 980, 1, 21, 16, komplett ? 0xFFFFFF : F_GRAU, T("△  Vollständige Bereinigung", "△  Full cleanup"), bw - 36);
    const char *kn[5] = { T("↑↓ wählen", "↑↓ move"), T("←→ Bereich / Einträge", "←→ area / entries"), T("✕ Haken", "✕ tick"), T("Options: Protokoll", "Options: log"), T("○ halten: Beenden", "hold ○: Quit") };
    fuss(kn, 5);
}

/* NEU 1.1: Testversion "nur lesen" - Statuszeile und Protokoll sagen das statt "SIMULATION" */
static void sim_hinweis_setzen(void)
{
#ifdef NUR_LESEN
    A.sim_hinweis = T("TEST: NUR LESEN - es wird nichts verändert.", "TEST: READ ONLY - nothing is changed.");
#endif
}

static void schirm_arbeit(void)
{
    kopf();
    pthread_mutex_lock(&G.m);
    char st[1024]; snprintf(st, sizeof st, "%s", G.stand); double p = G.prozent; bool unb = G.unbestimmt;
    pthread_mutex_unlock(&G.m);
    karte(40, 176, 1840, 200, A.loescht ? F_ROT_RAND : F_RAND, F_KARTE);
    const char *was = A.loescht ? (K->simulation ? T("SIMULATION läuft - es wird nichts gelöscht.", "SIMULATION running - nothing is deleted.")
                                                 : T("Löschen läuft - bitte warten, bis „Fertig“ erscheint.", "Deleting - please wait until \"Done\" appears."))
                                : T("Bitte warten ...", "Please wait ...");
    text(70, 228, 1, 30, A.loescht ? F_ROT_TEXT : F_TEXT, was);
    text(70, 272, 0, 22, F_GRAU, kuerzen(0, 22, st[0] ? st : " ", 1780));
    karte(70, 300, 1780, 40, F_RAND, F_HG);
    if (unb) { int w = 300, pos = (int)(SDL_GetTicks() / 3 % (1776 + w)) - w; int a = pos < 0 ? 0 : pos, b = pos + w > 1776 ? 1776 : pos + w; if (b > a) rechteck(72 + a, 302, b - a, 36, F_BLAU); }
    else if (p > 0) rechteck(72, 302, (int)(1776 * (p > 100 ? 100 : p) / 100), 36, F_BLAU);
    /* Protokoll (letzte Zeilen) */
    karte(40, 396, 1840, 600, F_RAND, F_KARTE);
    text(70, 436, 1, 20, F_GRAU, T("PROTOKOLL", "LOG"));
    pthread_mutex_lock(&G.m);
    int von = G.log_n - 400 < 0 ? 0 : G.log_n - 400;
    const char *alles = "";
    sliste l = {0}; for (int i = von; i < G.log_n; i++) sl_add(fa, &l, a_kopie(fa, G.log[i]));
    pthread_mutex_unlock(&G.m);
    alles = sl_join(fa, &l, "\n");
    zeilen z = umbrechen(0, 20, alles, 1780);
    int max = 21, start = z.n > max ? z.n - max : 0;
    for (int i = start; i < z.n; i++) text(70, 476 + (i - start) * 25, 0, 20, F_TEXT, z.z[i]);
    const char *kn[2] = { A.abbrechbar ? T("○ Abbrechen (es wird nichts gelöscht)", "○ Cancel (nothing is deleted)") : T("Bitte warten", "Please wait"), T("Options: Protokoll", "Options: log") };
    fuss(kn, 2);
}

/* NEU 1.0.7 (da_Chris86 03.11.2026 19:09-19:12): freiwillige Rückmeldung per QR-Code - "wir müssen den Leuten klar machen, dass wir
 * keine inhaltsbezogenen persönlichen Daten übermitteln". Die PS5 sendet dabei NICHTS, sie zeigt nur einen Link als QR-Code. Im Link
 * stehen nur Version, Fassung, Modellnummer, Firmware und Zahlen - keine Namen von Spielen oder Apps, kein Konto, keine Pfade, keine IP.
 * Das Handy öffnet die Seite und zeigt dieselben Angaben noch einmal; gespeichert wird erst, wenn der Nutzer dort auf "Senden" tippt. */
#ifndef RUECKMELDUNG_ADRESSE
#define RUECKMELDUNG_ADRESSE "https://feedback.ps5cleanup.workers.dev/r"
#endif
#define RUECKMELDUNG_VERSION "1.0.7"
#ifdef KERN_LOESCHEN
#define RUECKMELDUNG_FASSUNG 'r'             /* normale ELF */
#else
#define RUECKMELDUNG_FASSUNG 't'             /* Test-ELF / Simulation (löscht nie) */
#endif
typedef struct {
    char modell[17], fw[6], bau[11], jb[80], fehler[5 * 48 + 8];
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
        const char *q = getenv("VORSCHAU_RM_FEHLER"); st.fehler_n = 0;
        while (*q && st.fehler_n < 5) { const char *e = strchr(q, '~'); size_t l = e ? (size_t)(e - q) : strlen(q); if (l > 47) l = 47;
            memcpy(st.fehlerliste[st.fehler_n], q, l); st.fehlerliste[st.fehler_n][l] = 0; st.fehler_n++; if (!e) break; q = e + 1; }
        st.fehler = st.fehler_n;
    }
#endif
    r.s = st.da ? 2 : st.gesucht ? 1 : 0;
    if (r.s >= 1) {
        r.n = rm_zahl(st.gefunden);
        rm_kopie(r.jb, sizeof r.jb, st.jb, ",");
        for (int b = 0; b < 7; b++) r.bereich[b] = rm_zahl(st.bereich[b]);
        r.u = rm_zahl(st.unlesbar); r.t = rm_zahl(st.dauer_s);
        r.m2 = st.m2; r.ext = st.ext; r.usb = st.usb;
    }
    r.sm = st.sm_da ? 1 : 0;
    r.sb = zugriff_speicher_gebremst != 0;
    r.mb = rm_zahl((int)(basis_speicher_spitze() / 1048576));
    if (r.s == 2) {
        r.g = rm_zahl(st.gewaehlt); r.f = rm_zahl(st.fehler); r.p = rm_zahl(st.teilweise); r.ps = rm_zahl(st.geschuetzt); r.ab = st.ab;
        if (st.sm_angehalten) r.sm = 2; else if (st.sm_unklar) r.sm = 3;
        for (int i = 0; i < st.fehler_n; i++) {
            char e[48]; rm_kopie(e, sizeof e, st.fehlerliste[i], ".-ABCDEFGHIJKLMNOPQRSTUVWXYZ");   /* EACCES & Co. in Großbuchstaben */
            if (!e[0]) continue;
            size_t l = strlen(r.fehler);
            snprintf(r.fehler + l, sizeof r.fehler - l, "%s%s", l ? "~" : "", e);
        }
    }
    r.ok = r.s == 2 ? (r.f == 0 && r.p == 0 && !r.ab && st.unveraendert == 0) : r.s == 1;   /* Codex-Prüfung 69: "unverändert" ist kein Erfolg */
    return r;
}
static const char *rueckmeldung_link(arena *a, const rueckmeldung *r)
{
    return a_fmt(a, "%s?v=%s&b=%c&bd=%s&s=%d&m=%s&fw=%s&l=%s&dd=%d&jb=%s&sp=%d%d%d&sm=%d&a=%d.%d.%d.%d.%d.%d.%d&n=%d&g=%d&f=%d&p=%d&ps=%d&u=%d&sb=%d&mb=%d&t=%d&ab=%d&ok=%d%s%s",
                 RUECKMELDUNG_ADRESSE, RUECKMELDUNG_VERSION, RUECKMELDUNG_FASSUNG, r->bau, r->s, r->modell, r->fw, SP_EN ? "en" : "de", r->dd, r->jb,
                 r->m2, r->ext, r->usb, r->sm, r->bereich[0], r->bereich[1], r->bereich[2], r->bereich[3], r->bereich[4], r->bereich[5], r->bereich[6],
                 r->n, r->g, r->f, r->p, r->ps, r->u, r->sb, r->mb, r->t, r->ab, r->ok ? 1 : 0, r->fehler[0] ? "&e=" : "", r->fehler);
}
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
    int n = qr_kodieren(rueckmeldung_link(fa, &r));
    if (n <= 0) return 0;
    const char *info = VERB(fa, T("Mit dem Handy scannen und sagen, ob es geklappt hat.\nIm Code: ", "Scan with your phone and tell us whether it worked.\nIn the code: "),
                            rueckmeldung_werte(fa, &r), T(" - dazu Jailbreak-Einstieg, Speicher, ShadowMount, Zahlen je Bereich", " - plus jailbreak entry, storage, ShadowMount, numbers per area"),
                            r.fehler[0] ? T(" und Fehlercodes", " and error codes") : "", T(". Das Handy zeigt alles vor dem Senden.", ". Your phone shows everything before sending."));
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
        /* da_Chris86 02.11.2026 15:41-15:54: am Ende ✕ Neu starten (vorgewählt) oder ○ Beenden - kein Ausschalten mehr (das gab es seit
         * 30.10.2026 nur für den Datenbank-Neuaufbau, den PS5 Cleanup nie mehr empfiehlt). */
        const char *kn[2] = { T("✕  Neu starten", "✕  Restart"), T("○  Beenden", "○  Quit") };
        int kx = x + w - 40, ky = y + h - 100;
        text(x + 40, ky - 46, 0, 20, F_GRAU, T("Empfohlen: PS5 neu starten. Danach deinen Jailbreak wie gewohnt starten.",
                                               "Recommended: restart the PS5. Then start your jailbreak as usual."));
        if (G.f_auto) {
            int rest = 10 - (int)((SDL_GetTicks() - G.f_seit) / 1000); if (rest < 0) rest = 0;
            text(x + 40, ky - 84, 1, 24, F_ORANGE, VERB(fa, T("Automatischer Neustart in ", "Automatic restart in "), a_fmt(fa, "%d", rest), T(" s  ·  ○ = abbrechen", " s  ·  ○ = cancel")));
            lauf_aktiv = true;                     /* laufend neu zeichnen (Countdown) */
        }
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
        const char *po = plattform_protokoll_ordner(nr);
        if (!po) continue;
        char pi[200]; time_t j = time(NULL); struct tm tm; localtime_r(&j, &tm);
        snprintf(pi, sizeof pi, "%s/ps5cleanup_protokoll_%04d-%02d-%02d_%02d%02d%02d.txt", po, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        int fd = open(pi, O_WRONLY | O_CREAT | O_EXCL | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0644);
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

static void zeichnen(void)
{
    if (fa) arena_frei(fa);
    fa = arena_neu();
    rechteck(0, 0, BREITE, HOEHE, F_HG);
    lauf_aktiv = false;
    if (G.beschaeftigt) schirm_arbeit();
    else if (GESUCHT && AUSWAHL_OFFEN && ablauf_verbunden(&A)) schirm_liste();
    else schirm_start();
    if (G.art != D_KEINER) schirm_dialog();
    if (protokoll_offen) schirm_protokoll();   /* NEU 1.0.7: auch über einer Frage / dem Abschluss (Options) */
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
        pthread_mutex_lock(&G.m); bool cd = G.f_auto; if (cd && t == T_KREIS) G.f_auto = false; pthread_mutex_unlock(&G.m);   /* Codex-Prüfung 33 */
        if (cd) return;                                  /* Countdown läuft: nur ○ (abbrechen) */
        /* da_Chris86 02.11.2026: ✕ = Neu starten, ○ = Beenden (vorher 30.10.2026: ✕ = ausschalten, ○ = zurück zur Liste) */
        if (t == T_X) { nach_aktion = 2; antworten(false, false); }
        else if (t == T_KREIS) { nach_aktion = 1; antworten(false, false); }
        return;
    }
    if (art == D_FEHLER) { if (t == T_X || t == T_KREIS) antworten(false, false); return; }
    if (t == T_HOCH && (art == D_WAHL || dlg_fokus == 0)) { dlg_scroll -= 3; return; }
    if (t == T_RUNTER && art == D_WAHL) { dlg_scroll += 3; return; }
    if (art == D_WAHL) {
        if (t == T_LINKS) dlg_fokus = 1; else if (t == T_RECHTS) dlg_fokus = 0;
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
    int k = sicht[fokus_bereich], n = bereich_zeilen(k);
    switch (t) {
    case T_HOCH: if (fokus_zone == 0) { if (fokus_bereich > 0) { fokus_bereich--; fokus_eintrag = 0; eintrag_scroll = 0; } } else if (fokus_eintrag > 0) fokus_eintrag--; break;
    case T_RUNTER: if (fokus_zone == 0) { if (fokus_bereich < sicht_n - 1) { fokus_bereich++; fokus_eintrag = 0; eintrag_scroll = 0; } } else if (fokus_eintrag < n - 1) fokus_eintrag++; break;
    case T_L1: if (fokus_zone == 1) { fokus_eintrag -= 8; if (fokus_eintrag < 0) fokus_eintrag = 0; } break;
    case T_R1: if (fokus_zone == 1) { fokus_eintrag += 8; if (fokus_eintrag > n - 1) fokus_eintrag = n - 1 < 0 ? 0 : n - 1; } break;
    case T_RECHTS:
        if (fokus_zone == 1 && bereich_eintrag(k, fokus_eintrag) == &RESTE_ZEILE) { reste_offen = !reste_offen; break; }   /* NEU 1.0.7: Reste auf-/zuklappen */
        if (n > 0) fokus_zone = 1;
        break;
    case T_LINKS: fokus_zone = 0; break;
    case T_X:
        if (fokus_zone == 0) {
            if (n > 0 && ablauf_bereich_nur_einzeln(&A, k) && ablauf_bereich_zustand(&A, k) == 0)   /* NEU 1.1 */
                kurz(T("Hier nur einzeln wählbar: mit → in die Liste gehen und die Einträge einzeln anhaken.", "Individually only here: press → to enter the list and tick entries one by one."));
            else if (n > 0) ablauf_bereich_setzen(&A, k, ablauf_bereich_zustand(&A, k) != 2);
        }
        else {
            eintrag *f = bereich_eintrag(k, fokus_eintrag);
            const char *g;
            if (f == &RESTE_ZEILE) {   /* NEU 1.0.7: alle Reste an/aus */
                bool alle = true; for (int j = 0; j < reste_n; j++) if (!reste_e[j]->Gesperrt && !reste_e[j]->Wahl) alle = false;
                for (int j = 0; j < reste_n; j++) if (!ablauf_eintrag_gesperrt(&A, reste_e[j], &g)) ablauf_eintrag_setzen(&A, reste_e[j], !alle);
            }
            else if (f && ablauf_eintrag_gesperrt(&A, f, &g)) kurz(g ? g : T("Nicht wählbar.", "Not selectable."));
            else if (f) {
                bool vorher = f->Wahl;
                ablauf_eintrag_setzen(&A, f, !f->Wahl);
                /* NEU 1.0.6 (da_Chris86 29.10.2026 07:28): beim Anwählen eines Jailbreak-Einstiegs deutlich warnen */
                if (!vorher && f->Wahl && f->Jailbreak) { kurz(kern_jb_warnung(fa, f)); hinweis_bis = SDL_GetTicks() + 8000; }
                else if (vorher && !f->Wahl && f->Jailbreak) hinweis_bis = 0;   /* da_Chris86 29.10.2026 (Fernsehtest): abgewählt - Warnung sofort weg */
            }
        }
        break;
#ifdef NUR_LESEN
    case T_QUADRAT: case T_DREIECK: kurz(T("In dieser Testversion gesperrt - es wird nur nachgeschaut.", "Locked in this test version - only looking.")); break;
#else
    case T_QUADRAT: if (gewaehlt_anzahl() > 0) { dlg_fokus = 0; dlg_scroll = 0; starte(AUF_AUSWAHL); } else kurz(T("Erst etwas auswählen (✕).", "Select something first (✕).")); break;
    case T_DREIECK: if (K->Funde.n > 0) { dlg_fokus = 0; dlg_scroll = 0; starte(AUF_KOMPLETT); } break;
#endif
    case T_OPTIONS: protokoll_offen = true; prot_scroll = 1 << 20; break;
    default: break;
    }
}
static void taste(int t)
{
    neu_zeichnen();
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
        else if (t == T_OPTIONS) { protokoll_offen = true; prot_scroll = 1 << 20; }
        return;
    }
    bool fertig = GESUCHT && ablauf_verbunden(&A);
    if (fertig && AUSWAHL_OFFEN) { taste_liste(t); return; }
    /* Start / Übersicht */
    if (t == T_X) { if (fertig) { AUSWAHL_OFFEN = true; fokus_zone = 0; fokus_bereich = 0; fokus_eintrag = 0; eintrag_scroll = 0; } else starte(AUF_SUCHEN); }
    else if (t == T_DREIECK && fertig) starte(AUF_SUCHEN);
    else if (t == T_QUADRAT && !GESUCHT) { SP_EN = !SP_EN; sim_hinweis_setzen(); }   /* Sprache nur vor der ersten Suche (die Texte der Liste entstehen beim Suchen) */
    else if (t == T_OPTIONS) { protokoll_offen = true; prot_scroll = 1 << 20; }
}
static void kreis_gehalten(void)
{
    /* ○ eine Sekunde halten = Beenden - nicht während einer Arbeit oder einer Frage */
    if (G.art == D_FERTIG) { taste_dialog(T_KREIS); return; }   /* Prüfung p8, Befund 10: am Ende wie ○ = Beenden */
    if (G.beschaeftigt || A.loescht) { kurz(T("Bitte warten, bis die laufende Arbeit fertig ist.", "Please wait until the current task has finished.")); return; }
    if (G.art != D_KEINER) return;
    laeuft = false;
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
static SDL_GameController *pad;
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
static void skript_schritt(void)
{
    if (!skript || SDL_GetTicks() < skript_warte) return;
    char z[512];
    for (;;) {
        if (!fgets(z, sizeof z, skript)) { fclose(skript); skript = NULL; laeuft = false; return; }
        size_t l = strlen(z); while (l && (z[l - 1] == '\n' || z[l - 1] == '\r')) z[--l] = 0;
        if (!l || z[0] == '#') continue;
        if (!strncmp(z, "TASTE ", 6)) { taste(tasten_name(z + 6)); skript_warte = SDL_GetTicks() + 60; return; }
        if (!strncmp(z, "TEXT ", 5)) { pthread_mutex_lock(&G.m); snprintf(G.eingabe, sizeof G.eingabe, "%s", z + 5); pthread_mutex_unlock(&G.m); tastatur_offen = false; return; }
        if (!strncmp(z, "BILD ", 5)) { bild_speichern(z + 5); return; }
        if (!strncmp(z, "WARTE ", 6)) { skript_warte = SDL_GetTicks() + (Uint32)atoi(z + 6); return; }
        if (!strcmp(z, "WARTEFREI")) {   /* bis keine Arbeit mehr läuft und die Größen gezählt sind */
            if (G.beschaeftigt || G.art != D_KEINER || (A.hintergrund && A.hg_n < A.hg_gesamt)) { fseek(skript, -(long)strlen(z) - 1, SEEK_CUR); skript_warte = SDL_GetTicks() + 100; return; }
            continue;
        }
        if (!strcmp(z, "WARTEDIALOG")) {
            if (G.art == D_KEINER || G.dlg_nr != dlg_gesehen || SDL_GetTicks() - dlg_seit < 450) { fseek(skript, -(long)strlen(z) - 1, SEEK_CUR); skript_warte = SDL_GetTicks() + 100; return; }
            continue;
        }
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
    plattform_absturzmelder("PS5 Cleanup " VERSION_TEXT);   /* 1.0.6 Fix: ab hier jeder Absturz als Benachrichtigung + Protokoll */
    K = kern_neu(plattform_zugriff());
    K->appinfo_lesen = plattform_appinfo_lesen;   /* NEU 1.0.7 (02.11.2026): Zuordnungen und kaputte Zusatzinhalt-Angaben aus appinfo.db (nur lesen) */
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
    A.neustart_frage = NULL;   /* da_Chris86 30.10.2026 09:01: keine Neustart-Frage mehr */
#ifdef KERN_LOESCHEN
    A.protokoll_datei = T("in der Protokoll-Ansicht (Options) - beim Löschen auf dem USB-Laufwerk oder im Ordner /data/PS5-Cleanup-Protokolle", "in the log view (Options) - when deleting on the USB drive or in the folder /data/PS5-Cleanup-Protokolle");
#else
    A.protokoll_datei = T("in der Protokoll-Ansicht (Options) - diese Fassung speichert es nicht als Datei", "in the log view (Options) - this version does not save it as a file");   /* Prüfung p8, Befund 6 */
#endif
#ifdef NUR_LESEN
    sim_hinweis_setzen();
    ui_log(NULL, VERB(K->a, "PS5 Cleanup " VERSION_TEXT " (", plattform_name(), ") - ", T("TEST: NUR LESEN", "TEST: READ ONLY")));
#elif defined(KERN_LOESCHEN)
    ui_log(NULL, VERB(K->a, "PS5 Cleanup " VERSION_TEXT " (", plattform_name(), ")"));
#else
    ui_log(NULL, VERB(K->a, "PS5 Cleanup " VERSION_TEXT " (", plattform_name(), ") - SIMULATION"));
#endif

    /* 02.11.2026 (da_Chris86 15:44/15:45): die Protokoll-Datei beginnt erst direkt vor dem ersten Löschen (protokoll_beginnen) -
     * wer nur nachschaut, hinterlässt keine Datei. Bis dahin steht alles in der Ansicht (Options). */
    A.vor_selbstweg = vor_selbstweg_app; A.vor_selbstweg_ctx = NULL;
    A.vor_loeschen = protokoll_beginnen; A.vor_loeschen_ctx = NULL;   /* 02.11.2026: nur noch das Protokoll - keine Datenbank-Sicherung */
    A.loeschen_moeglich = NULL;   /* 02.11.2026 (da_Chris86 15:44): kein USB-Stick, keine Sicherung als Bedingung */
    plattform_meldung = ui_meldung;   /* NEU 1.0.7: jeder Schritt der Sony-Deinstallation ins Protokoll */
#ifdef PRUEFLAUF
    starte(AUF_SUCHEN);                  /* Prüflauf: Nachschauen beginnt von selbst (nur lesen) */
#endif
    Uint32 letztes = 0;
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
                /* NEU 1.0.6 (Codex-Prüfung 32): ○ bricht den Neustart-Countdown sofort beim Drücken ab */
                if (t == T_KREIS) { pthread_mutex_lock(&G.m); bool cd = G.art == D_FERTIG && G.f_auto; if (cd) G.f_auto = false; pthread_mutex_unlock(&G.m); if (cd) { neu_zeichnen(); break; } }
                if (t == T_KREIS) { kreis_seit = SDL_GetTicks(); neu_zeichnen(); break; }   /* erst beim Loslassen (kurz) */
                taste(t); break;
            }
            case SDL_CONTROLLERBUTTONUP:
                if (tastennr(e.cbutton.button) == T_KREIS && kreis_seit) { kreis_seit = 0; taste(T_KREIS); }
                break;
            case SDL_KEYDOWN:   /* nur zum Testen am PC */
                if (e.key.repeat) break;
                switch (e.key.keysym.sym) {
                case SDLK_RETURN: taste(T_X); break; case SDLK_ESCAPE: taste(T_KREIS); break;
                case SDLK_s: taste(T_QUADRAT); break; case SDLK_t: taste(T_DREIECK); break; case SDLK_o: taste(T_OPTIONS); break;
                case SDLK_UP: taste(T_HOCH); break; case SDLK_DOWN: taste(T_RUNTER); break; case SDLK_LEFT: taste(T_LINKS); break; case SDLK_RIGHT: taste(T_RECHTS); break;
                default: break;
                }
                break;
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
        if (G.dlg_nr != dlg_gesehen) { dlg_gesehen = G.dlg_nr; dlg_fokus = 0; dlg_scroll = 0; dlg_seit = SDL_GetTicks(); neu_zeichnen(); }   /* neue Frage: sicherer Knopf */
        if (kreis_seit) { neu_zeichnen(); if (SDL_GetTicks() - kreis_seit >= 1000) { kreis_seit = 0; kreis_gehalten(); } }
        if (G.beschaeftigt || hinweis_bis > SDL_GetTicks()) { if (SDL_GetTicks() - letztes > 60) neu_zeichnen(); }
        if (lauf_aktiv && SDL_GetTicks() - letztes > 33) neu_zeichnen();   /* NEU 1.0.6: Laufschrift */
        {   /* NEU 1.0.6: Countdown abgelaufen -> Neustart (Codex-Prüfung 32: unter der Sperre lesen) */
            pthread_mutex_lock(&G.m);
            bool ab = G.art == D_FERTIG && G.f_auto && SDL_GetTicks() - G.f_seit >= 10000;
            if (ab) G.f_auto = false;
            pthread_mutex_unlock(&G.m);
            if (ab) { nach_aktion = 2; antworten(false, false); }
        }
        if (nach_aktion && !G.beschaeftigt && G.art == D_KEINER) {         /* NEU 1.0.6: Auswahl am Ende der Bereinigung */
            int na = nach_aktion; nach_aktion = 0;
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
                if (!plattform_neustart()) { const char *t = T("Das hat nicht geklappt - bitte die PS5 über das PS5-Menü neu starten.", "That did not work - please restart the PS5 via the PS5 menu."); ui_log(NULL, t); kurz(t); }
            }
            neu_zeichnen();
        }
#ifdef VORSCHAU
        skript_schritt();
#endif
        if (G.neu) { G.neu = false; zeichnen(); SDL_UpdateWindowSurface(fenster); letztes = SDL_GetTicks(); }
        SDL_Delay(16);
    }
    /* NEU 1.0.7 (Plan 1.0.7, Punkt 2): nach ○ am Ende bleibt der Hinweis auch nach dem Beenden sichtbar - als Benachrichtigung.
     * Ist die Selbstentfernung übergeben, hängt ihn der Hintergrundteil an "PS5 Cleanup wurde entfernt." an (eine Meldung,
     * richtige Reihenfolge); sonst kommt er sofort. In der Simulation keine Benachrichtigung. */
    if (ende_hinweis && !K->simulation) {
        const char *hw = T("PS5 Cleanup: Fertig. Empfohlen: die PS5 bei Gelegenheit neu starten.", "PS5 Cleanup: Done. Recommended: restart the PS5 when convenient.");
        if (!(K->SELBST_UEBERGEBEN && plattform_hinweis_uebergeben(ende_hinweis, SP_EN))) plattform_melden(hw);
    }
    ablauf_hintergrund_anhalten(&A);
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
