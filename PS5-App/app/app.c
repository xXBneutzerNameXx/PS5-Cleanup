/* PS5 Cleanup 1.0.6 - Oberfläche am Fernseher
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kern/ablauf.h"
#include "plattform.h"

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
#define VERSION_TEXT "1.0.6 Fix"        /* 30.10.2026: korrigierte 1.0.6 (Speicher, Playden, Absturzmelder) */
#else
#define VERSION_TEXT "1.0.6 Fix TEST"   /* da_Chris86 30.10.2026: Testfassung PS5-Cleanup-Test.elf - ohne Löschbefehle gebaut, löscht nie */
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
/* einzeilig, zu lang -> "…" */
static const char *kuerzen(int f, int gr, const char *t, int breite)
{
    if (textbreite(f, gr, t) <= breite) return t;
    int el = textbreite(f, gr, "…");
    const char *q = t; int b = 0;
    while (*q) { const char *v = q; Glyph *g = glyph(f, gr, cp_naechstes(&q)); b += g ? g->vorschub : 0; if (b + el > breite) { q = v; break; } }
    return VERB(fa, a_kopie_n(fa, t, (size_t)(q - t)), "…");
}
/* NEU 1.0.6 (da_Chris86 28.10.2026: lange Spielnamen): in der markierten Zeile läuft ein zu langer Text langsam durch -
 * 1,5 s Pause am Anfang, mit 90 Pixeln pro Sekunde bis zum Ende, 1,5 s Pause, dann wieder von vorn */
static const void *lauf_wer; static Uint32 lauf_seit; static bool lauf_aktiv;
static void laufschrift(int x, int y, int f, int gr, uint32_t farbe, const char *t, int breite, const void *wer)
{
    int tw = textbreite(f, gr, t);
    if (tw <= breite) { text(x, y, f, gr, farbe, t); return; }
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

/* =================================================================== gemeinsamer Zustand (Arbeits-Faden <-> Oberfläche) */
typedef enum { D_KEINER, D_WAHL, D_TEXT, D_FERTIG, D_FEHLER } dialogart;
static struct {
    pthread_mutex_t m; pthread_cond_t c;
    dialogart art; char *titel, *text, *sicher, *gef; bool beantwortet, wahl; char eingabe[256]; bool eingabe_ok;
    char *f_kopf, *f_frei, *f_sm; bool f_ok;
    volatile bool f_auto; Uint32 f_seit;     /* NEU 1.0.6: automatischer Neustart nach 10 s (abbrechbar) */
    char stand[1024]; double prozent; bool unbestimmt;
    char **log; int log_n, log_cap;
    volatile bool beschaeftigt;
    volatile bool neu;
    volatile int dlg_nr;            /* jede neue Frage zählt hoch -> Fokus immer zuerst auf dem sicheren Knopf */
} G = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER };

static ablauf A;                     /* (Vorab-Deklaration, steht weiter unten) */
static char *dup(const char *s) { return strdup(s ? s : ""); }
static void neu_zeichnen(void) { G.neu = true; }

static void ui_log(void *c, const char *t)
{
    (void)c;
    pthread_mutex_lock(&G.m);
    if (G.log_n == G.log_cap) { G.log_cap = G.log_cap ? G.log_cap * 2 : 256; G.log = realloc(G.log, sizeof(char *) * (size_t)G.log_cap); }
    G.log[G.log_n++] = dup(t);
    pthread_mutex_unlock(&G.m);
    printf("PS5 Cleanup: %s\n", t ? t : ""); fflush(stdout);
    neu_zeichnen();
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
    if (a == AUF_SUCHEN) ablauf_starte(&A, lauf_suchen, NULL);
    else if (a == AUF_KOMPLETT) ablauf_starte(&A, (void (*)(void *))ablauf_komplett, &A);
    else ablauf_starte(&A, (void (*)(void *))ablauf_auswahl, &A);
    if (a == AUF_SUCHEN) { GESUCHT = ablauf_verbunden(&A); AUSWAHL_OFFEN = false; }
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
static char hinweis_kurz[256];  static Uint32 hinweis_bis = 0;   /* kurze Meldung unten */
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
static eintrag *bereich_eintrag(int k, int idx)
{
    int n = 0;
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == k) { if (n == idx) return f; n++; } }
    return NULL;
}
static int gewaehlt_anzahl(void) { int n = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Wahl) n++; return n; }

/* =================================================================== Bildschirme */
static void kopf(void)
{
    karte(40, 24, BREITE - 80, 128, F_RAND, F_KARTE);
    if (logo) { SDL_Rect z = {60, 36, 104, 104}; SDL_BlitScaled(logo, NULL, ziel, &z); }
    text(186, 92, 1, 48, F_TEXT, "PS5 Cleanup");
    text(186 + textbreite(1, 48, "PS5 Cleanup") + 18, 92, 1, 26, F_BLAU_HELL, VERSION_TEXT);
    const char *k = T("Löscht nur, was du auswählst. Kein Zurücksetzen, keine Neuinstallation: Systemsoftware, Laufwerks-Kopplung, Konto und echte Lizenzen bleiben. Benutzung auf eigene Gefahr - Gelöschtes ist endgültig weg.",
                      "Deletes only what you select - single games, saved data, trophies or everything at once. No reset, no reinstall: system software, disc drive pairing, account and real licenses stay. Use at your own risk - deleted data is gone for good.");
    absatz(188, 124, 0, 17, F_GRAU, k, 1180, 21);
    /* dauerhaft: SIMULATION (Release-Bau: kein Hinweisfeld) */
#ifndef KERN_LOESCHEN
    karte(1420, 40, 460, 96, F_GELB, F_GELB_HG);
#endif
#ifdef KERN_LOESCHEN
#elif defined(NUR_LESEN)
    text(1444, 82, 1, 30, F_GELB, T("TEST: NUR LESEN", "TEST: READ ONLY"));
    text(1444, 116, 0, 19, F_GELB, T("Es wird nur nachgeschaut - nichts verändert.", "Only looking - nothing is changed."));
#else
    text(1444, 82, 1, 30, F_GELB, "SIMULATION");
    text(1444, 116, 0, 19, F_GELB, T("Es wird nichts gelöscht - nur angezeigt.", "Nothing is deleted - display only."));
#endif
}

static uint32_t kopplung_farbe(void)
{
    switch (kern_kopplungs_stufe(K)) { case KOPPLUNG_BESTAETIGT: return F_MINT; case KOPPLUNG_UNKLAR: return F_GELB; default: return 0xC9CCD4; }
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
        else { yy += absatz(x + 24, yy, 1, 22, F_MINT, VERB(fa, T("Eingerichtet: ", "Set up: "), jb), w - 48, 28); yy += 2; if (warn) yy += absatz(x + 24, yy, 0, 19, F_ORANGE, warn, w - 48, 24); yy += 16; }
    }
    /* Laufwerks-Kopplung */
    text(x + 24, yy, 1, 20, F_GRAU, T("LAUFWERKS-KOPPLUNG", "DISC DRIVE PAIRING")); yy += 34;
    const char *t = kern_laufwerk_text(K); const char *dp = strstr(t, ": ");
    const char *titel = dp ? a_kopie_n(fa, t, (size_t)(dp - t)) : t, *rest = dp ? dp + 2 : "";
    if (rest[0] >= 'a' && rest[0] <= 'z') { char *r2 = (char *)a_kopie_n(fa, rest, strlen(rest)); r2[0] = (char)(r2[0] - 'a' + 'A'); rest = r2; }   /* Satzanfang groß */
    text(x + 24, yy, 1, 24, kopplung_farbe(), VERB(fa, kern_kopplungs_stufe(K) == KOPPLUNG_BESTAETIGT ? "✓ " : "", titel)); yy += 30;
    if (!kompakt) { yy += absatz(x + 24, yy, 0, 19, F_GRAU, rest, w - 48, 24); }
    text(x + 24, yy + 4, 0, 18, F_GRAU, T("Wird nur gelesen, nie verändert.", "Only read, never changed."));
}
static void speicher_karte(int x, int y, int w, int h)
{
    karte(x, y, w, h, F_RAND, F_KARTE);
    int yy = y + 40;
    text(x + 24, yy, 1, 20, F_GRAU, T("SPEICHER", "STORAGE")); yy += 42;
    /* NEU 1.0.6 (da_Chris86 28.10.2026): nur die Anzahl; weiß = gefunden, grau = nicht angeschlossen, gelb = unklar */
    const char *nl[4] = { T("Intern", "Internal"), T("M.2-SSD", "M.2 SSD"), "USB", "ShadowMount" };
    const char *wt[4]; uint32_t fb[4] = { F_TEXT, F_TEXT, F_TEXT, F_TEXT };
    if (!GESUCHT || !ablauf_verbunden(&A)) { for (int i = 0; i < 4; i++) wt[i] = "–"; }
    else {
        int n[4] = { 0, 0, 0, 0 }, sm = 0;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Ort >= 0 && f->Ort <= 3) n[f->Ort]++; if (f->Kat == 1 && f->Quelle[0]) sm++; }
        wt[0] = VERB(fa, a_fmt(fa, "%d", n[ORT_INTERN]), EINTRAEGE(n[ORT_INTERN]));
        if (!K->m2Da) { wt[1] = T("nicht eingebaut", "not installed"); fb[1] = F_GRAU; }
        else if (K->M2_OHNE_USER) { wt[1] = T("leer oder nicht richtig erkannt", "empty or not detected properly"); fb[1] = F_GELB; }
        else wt[1] = VERB(fa, a_fmt(fa, "%d", n[ORT_M2]), EINTRAEGE(n[ORT_M2]));
        const char *u = "";
        if (K->USB_SPEICHER) u = VERB(fa, T("erweiterter Speicher · ", "extended storage · "), a_fmt(fa, "%d", n[ORT_USB]), EINTRAEGE(n[ORT_USB]));
        if (K->STICK_DA) u = VERB(fa, u, u[0] ? "  ·  " : "", T("USB-Stick · ", "USB stick · "), a_fmt(fa, "%d", n[ORT_STICK]), n[ORT_STICK] == 1 ? T(" Datei", " file") : T(" Dateien", " files"));
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
    if (allein) text(60, HOEHE - 22, 1, 22, F_GELB, kuerzen(1, 22, hinweis_kurz, BREITE - 120));
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
    if (!fertig) { const char *kn[4] = { T("✕ Nachschauen", "✕ Scan"), SP_EN ? "□ Deutsch" : "□ English", T("Options: Protokoll", "Options: log"), T("○ halten: Beenden", "hold ○: Quit") }; fuss(kn, 4); }
    else { const char *kn[4] = { T("✕ Weiter zur Auswahl", "✕ Continue"), T("△ Nochmal nachschauen", "△ Scan again"), T("Options: Protokoll", "Options: log"), T("○ halten: Beenden", "hold ○: Quit") }; fuss(kn, 4); }
}

/* Auswahl: links Bereiche, rechts Einträge (ZeigeBereiche) */
#define ZEILE 46
static int eintrag_scroll = 0;
static void schirm_liste(void)
{
    kopf();
    ablauf_texte_frei(&A);
    /* links: Bereiche */
    int x0 = 40, y0 = 176, w0 = 640;
    karte(x0, y0, w0, 620, fokus_zone == 0 ? F_BLAU : F_RAND, F_KARTE);
    text(x0 + 24, y0 + 40, 1, 20, F_GRAU, T("BEREINIGUNG", "CLEANUP"));
    sicht_neu();
    for (int i = 0; i < sicht_n; i++) {
        int k = sicht[i], yy = y0 + 64 + i * 86;
        int n = bereich_anzahl(k);
        if (i == fokus_bereich) rechteck(x0 + 8, yy, w0 - 16, 70, fokus_zone == 0 ? F_FOKUS : F_KARTE2);
        int z = ablauf_bereich_zustand(&A, k);
        /* Kästchen */
        karte(x0 + 22, yy + 18, 34, 34, n ? F_GRAU : F_RAND, F_HG);
        if (z == 2) rechteck(x0 + 29, yy + 25, 20, 20, F_BLAU_HELL);
        else if (z == 1) rechteck(x0 + 29, yy + 32, 20, 6, F_BLAU_HELL);
        const char *kp = ablauf_bereich_kopf(&A, k);
        /* "[k] Name   (n Einträge · Größe)" in zwei Zeilen */
        const char *kl = strstr(kp, "   (");
        const char *name = kl ? a_kopie_n(fa, kp, (size_t)(kl - kp)) : kp;
        text(x0 + 72, yy + 32, 1, 23, n ? F_TEXT : F_GRAU, kuerzen(1, 23, name, w0 - 100));
        if (kl) text(x0 + 72, yy + 60, 0, 19, F_GRAU, kuerzen(0, 19, kl + 3, w0 - 100));
    }
    /* rechts: Einträge des Bereichs */
    int k = sicht[fokus_bereich];
    int x1 = 700, y1 = 176, w1 = BREITE - 40 - x1, h1 = 620;
    karte(x1, y1, w1, h1, fokus_zone == 1 ? F_BLAU : F_RAND, F_KARTE);
    int yy = y1 + 40;
    text(x1 + 24, yy, 1, 22, F_TEXT, kern_kat_name(k)); yy += 10;
    yy += absatz(x1 + 24, yy + 26, 0, 19, F_GRAU, kern_kat_info(k), w1 - 48, 24) + 4;
    bool sm = false; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == 1 && f->Quelle[0]) sm = true; }
    if (k == 1 && sm) yy += absatz(x1 + 24, yy + 26, 0, 19, F_GELB, T("ShadowMount-Spiele: Bei \"Nur Auswahl löschen\" wirst du gefragt, ob nur die Kachel oder auch die Spieldatei weg soll.",
                                                                     "ShadowMount games: with \"Delete selection only\" you will be asked whether only the tile or the game file too should go."), w1 - 48, 24);
    int n = bereich_anzahl(k);
    int liste_y = yy + 12, platz = (y1 + h1 - 120) - liste_y, sichtbar = platz / ZEILE; if (sichtbar < 1) sichtbar = 1;
    if (fokus_eintrag < eintrag_scroll) eintrag_scroll = fokus_eintrag;
    if (fokus_eintrag >= eintrag_scroll + sichtbar) eintrag_scroll = fokus_eintrag - sichtbar + 1;
    if (n == 0) text(x1 + 24, liste_y + 34, 0, 22, F_GRAU, T("Nichts gefunden - hier ist alles sauber.", "Nothing found - all clean here."));
    for (int i = eintrag_scroll; i < n && i < eintrag_scroll + sichtbar; i++) {
        eintrag *f = bereich_eintrag(k, i);
        int ry = liste_y + (i - eintrag_scroll) * ZEILE;
        const char *grund; bool gesp = ablauf_eintrag_gesperrt(&A, f, &grund);
        if (fokus_zone == 1 && i == fokus_eintrag) rechteck(x1 + 8, ry, w1 - 16, ZEILE - 4, F_FOKUS);
        karte(x1 + 22, ry + 6, 30, 30, gesp ? F_RAND : F_GRAU, F_HG);
        if (f->Wahl) rechteck(x1 + 28, ry + 12, 18, 18, gesp ? F_GRAU : F_BLAU_HELL);
        const char *gr = ablauf_eintrag_groesse(&A, f);
        int gb = gr[0] ? textbreite(0, 20, gr) : 0;
        uint32_t fb = gesp ? F_GRAU : (f->Paket ? F_GELB : F_TEXT);
        if (fokus_zone == 1 && i == fokus_eintrag) laufschrift(x1 + 66, ry + 29, 0, 22, fb, f->Text, w1 - 110 - gb, f);   /* NEU 1.0.6 */
        else text(x1 + 66, ry + 29, 0, 22, fb, kuerzen(0, 22, f->Text, w1 - 110 - gb));
        if (gr[0]) text(x1 + w1 - 24 - gb, ry + 29, 0, 20, F_GRAU, gr);
    }
    if (n > sichtbar) {   /* Bildlaufleiste */
        int bh = platz * sichtbar / n, by = liste_y + (platz - bh) * eintrag_scroll / (n - sichtbar > 0 ? n - sichtbar : 1);
        rechteck(x1 + w1 - 10, by, 4, bh, F_RAND);
    }
    /* Infofeld (InfoFeld in 1.0.5): voller Name + Hinweis */
    eintrag *fe = (fokus_zone == 1) ? bereich_eintrag(k, fokus_eintrag) : NULL;
    rechteck(x1 + 2, y1 + h1 - 110, w1 - 4, 2, F_RAND);
    if (fe) {
        const char *grund; ablauf_eintrag_gesperrt(&A, fe, &grund);
        int hy = y1 + h1 - 80;
        hy += absatz(x1 + 24, hy, 0, 19, F_TEXT, kuerzen(0, 19, fe->Text, (w1 - 48) * 2), w1 - 48, 24);
        if (grund) text(x1 + 24, hy, 0, 19, F_GELB, kuerzen(0, 19, grund, w1 - 48));
    } else if (ablauf_bereich_nur_einzeln(&A, k)) text(x1 + 24, y1 + h1 - 64, 0, 19, F_GELB, T("→ Einträge dieses Bereichs einzeln wählen  ·  hier nur einzeln wählbar", "→ choose entries of this area one by one  ·  individually only here"));   /* NEU 1.1 */
    else text(x1 + 24, y1 + h1 - 64, 0, 19, F_GRAU, T("→ Einträge dieses Bereichs einzeln wählen  ·  ✕ ganzen Bereich wählen", "→ choose entries of this area one by one  ·  ✕ select the whole area"));
    /* unten: Stand + Knöpfe (wie AktualisiereStand / Knoepfe) */
    karte(40, 812, 1840, 190, F_RAND, F_KARTE);
    text(70, 856, 1, 24, F_TEXT, kuerzen(1, 24, ablauf_stand_text(&A), 1780));
    const char *sh = kern_selbstschutz_hinweis(K);
    if (sh[0]) text(70, 892, 0, 19, F_GRAU, kuerzen(0, 19, sh, 1780));
    bool auswahl = gewaehlt_anzahl() > 0, komplett = K->Funde.n > 0 && ablauf_verbunden(&A);
#ifdef NUR_LESEN
    auswahl = komplett = false;          /* Testversion Schritt 9: nur nachschauen */
#endif
    karte(70, 914, 520, 70, auswahl ? F_BLAU_HELL : F_RAND, auswahl ? F_BLAU : F_KARTE2);
    text(96, 960, 1, 26, auswahl ? 0xFFFFFF : F_GRAU, T("□  Nur Auswahl löschen", "□  Delete selection only"));
    karte(620, 914, 560, 70, komplett ? F_ROT_RAND : F_RAND, komplett ? F_ROT : F_KARTE2);
    text(646, 960, 1, 26, komplett ? 0xFFFFFF : F_GRAU, T("△  Vollständige Bereinigung", "△  Full cleanup"));
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
        /* NEU 1.0.6: "Datenbank neu aufbauen" nur, wenn Kacheln im Menü bleiben (mit Namen) - ohne Sony wie bisher */
        if (K->SONY && K->GEISTER.n > 0)
            yy += absatz(x + 40, yy + 16, 1, 23, F_ORANGE, VERB(fa, T("Diese Einträge bleiben im PS5-Menü sichtbar, bis die Datenbank neu aufgebaut ist: ", "These entries stay visible in the PS5 menu until the database has been rebuilt: "),
                                                                   sl_join(fa, &K->GEISTER, ", ")), w - 80, 30) + 16;
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
        text(x + 40, y + h - 230, 0, 18, F_GRAU, VERB(fa, T("Protokoll: ", "Log: "), A.protokoll_datei));
        /* da_Chris86 30.10.2026 09:01: am Ende IMMER ausschalten (ganz aus, kein Ruhemodus) - ein Knopf, ohne Countdown, erst mit ✕ oder ○.
         * Vorher (28./29.10.): Beenden · Neu starten / Ausschalten. So muss niemand LIESMICH lesen, um das Richtige zu wählen. */
        const char *kn[1] = { T("PS5 ausschalten", "Turn off PS5") };
        int kx = x + w - 40, ky = y + h - 100;
        if (K->DB_HINWEIS)
            text(x + 40, ky - 46, 0, 20, F_ORANGE, T("Die PS5 wird ganz ausgeschaltet (kein Ruhemodus). Danach die Datenbank neu aufbauen (Schritte oben).",
                                                     "The PS5 turns off completely (not rest mode). Then rebuild the database (steps above)."));
        else
            text(x + 40, ky - 46, 0, 20, F_GRAU, T("Die PS5 wird ganz ausgeschaltet. Danach einschalten und deinen Jailbreak neu starten.",
                                                   "The PS5 turns off completely. Then turn it on and start your jailbreak again."));
        if (G.f_auto) {
            int rest = 10 - (int)((SDL_GetTicks() - G.f_seit) / 1000); if (rest < 0) rest = 0;
            text(x + 40, ky - 84, 1, 24, F_ORANGE, VERB(fa, T("Automatischer Neustart in ", "Automatic restart in "), a_fmt(fa, "%d", rest), T(" s  ·  ○ = abbrechen", " s  ·  ○ = cancel")));
            lauf_aktiv = true;                     /* laufend neu zeichnen (Countdown) */
        }
        for (int i = 0; i < 1; i++) {
            int bw = textbreite(1, 24, kn[i]) + 70; kx -= bw;
            karte(kx, ky, bw, 70, dlg_fokus == i ? 0xFFFFFF : F_BLAU_HELL, i == 0 ? F_BLAU : F_KARTE2);
            text(kx + 35, ky + 45, 1, 24, 0xFFFFFF, kn[i]);
            kx -= 20;
        }
        text(x + 40, ky + 45, 0, 20, F_GRAU, T("✕ = PS5 ausschalten  ·  ○ = abbrechen", "✕ = turn off PS5  ·  ○ = cancel"));
        text(x + 40, ky + 75, 0, 18, F_ORANGE, K->DB_HINWEIS ? T("Mit ○ beendet sich PS5 Cleanup nur - dann musst du die PS5 selbst ganz ausschalten (kein Ruhemodus) und die Datenbank neu aufbauen.",
                                                                  "With ○ PS5 Cleanup only quits - then you have to turn the PS5 off completely yourself (not rest mode) and rebuild the database.")
                                                             : T("Mit ○ beendet sich PS5 Cleanup nur - dann musst du die PS5 selbst neu starten oder ausschalten.",
                                                                  "With ○ PS5 Cleanup only quits - then you have to restart or turn off the PS5 yourself."));
        if (K->simulation) text(x + 40, ky - 14, 0, 18, F_GELB, T("SIMULATION: Das Ausschalten wird nur angezeigt, nicht ausgeführt.", "SIMULATION: turning off is only shown, not carried out."));
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
        text(x + 40, ky + 45, 0, 19, F_GRAU, T("←→ Knopf wählen  ·  ✕ drücken  ·  ○ = sicherer Knopf", "←→ choose button  ·  ✕ press  ·  ○ = safe button"));
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

static void schirm_protokoll(void)
{
    abdunkeln();
    karte(60, 40, BREITE - 120, HOEHE - 80, F_BLAU, F_KARTE);
    text(100, 100, 1, 30, F_TEXT, T("Protokoll", "Log"));
    pthread_mutex_lock(&G.m);
    sliste l = {0}; for (int i = 0; i < G.log_n; i++) sl_add(fa, &l, a_kopie(fa, G.log[i]));
    pthread_mutex_unlock(&G.m);
    zeilen z = umbrechen(0, 20, sl_join(fa, &l, "\n"), BREITE - 220);
    int max = (HOEHE - 260) / 25;
    if (prot_scroll > z.n - max) prot_scroll = z.n - max;
    if (prot_scroll < 0) prot_scroll = 0;
    for (int i = prot_scroll; i < z.n && i < prot_scroll + max; i++) text(100, 150 + (i - prot_scroll) * 25, 0, 20, F_TEXT, z.z[i]);
    text(100, HOEHE - 70, 0, 20, F_GRAU, T("↑↓ blättern  ·  L1/R1 seitenweise  ·  ○ oder Options: schließen", "↑↓ scroll  ·  L1/R1 page  ·  ○ or Options: close"));
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
    else if (protokoll_offen) schirm_protokoll();
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
        /* da_Chris86 30.10.2026 09:03: ✕ = ausschalten, ○ = nicht ausschalten (PS5 Cleanup beendet sich, Hinweis steht auf dem Bildschirm) */
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
    int k = sicht[fokus_bereich], n = bereich_anzahl(k);
    switch (t) {
    case T_HOCH: if (fokus_zone == 0) { if (fokus_bereich > 0) { fokus_bereich--; fokus_eintrag = 0; eintrag_scroll = 0; } } else if (fokus_eintrag > 0) fokus_eintrag--; break;
    case T_RUNTER: if (fokus_zone == 0) { if (fokus_bereich < sicht_n - 1) { fokus_bereich++; fokus_eintrag = 0; eintrag_scroll = 0; } } else if (fokus_eintrag < n - 1) fokus_eintrag++; break;
    case T_L1: if (fokus_zone == 1) { fokus_eintrag -= 8; if (fokus_eintrag < 0) fokus_eintrag = 0; } break;
    case T_R1: if (fokus_zone == 1) { fokus_eintrag += 8; if (fokus_eintrag > n - 1) fokus_eintrag = n - 1 < 0 ? 0 : n - 1; } break;
    case T_RECHTS: if (n > 0) fokus_zone = 1; break;
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
            if (f && ablauf_eintrag_gesperrt(&A, f, &g)) kurz(g ? g : T("Nicht wählbar.", "Not selectable."));
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
    if (G.art != D_KEINER) { taste_dialog(t); return; }
    if (protokoll_offen) {
        if (t == T_KREIS || t == T_OPTIONS) protokoll_offen = false;
        else if (t == T_HOCH) prot_scroll -= 3; else if (t == T_RUNTER) prot_scroll += 3;
        else if (t == T_L1) prot_scroll -= 30; else if (t == T_R1) prot_scroll += 30;
        return;
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
    A.protokoll_datei = T("nur in der Protokoll-Ansicht (Options) - es wird keine Datei geschrieben", "only in the log view (Options) - no file is written");
#ifdef NUR_LESEN
    sim_hinweis_setzen();
    ui_log(NULL, VERB(K->a, "PS5 Cleanup " VERSION_TEXT " (", plattform_name(), ") - ", T("TEST: NUR LESEN", "TEST: READ ONLY")));
#elif defined(KERN_LOESCHEN)
    ui_log(NULL, VERB(K->a, "PS5 Cleanup " VERSION_TEXT " (", plattform_name(), ")"));
#else
    ui_log(NULL, VERB(K->a, "PS5 Cleanup " VERSION_TEXT " (", plattform_name(), ") - SIMULATION"));
#endif

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
            if (na == 1) {   /* ○ am Ende: nicht ausschalten, PS5 Cleanup beendet sich (übergebene Selbstentfernung läuft danach) */
                ui_log(NULL, K->DB_HINWEIS ? T("Nicht ausgeschaltet - bitte die PS5 selbst ganz ausschalten und die Datenbank neu aufbauen.", "Not turned off - please turn the PS5 off completely yourself and rebuild the database.")
                                           : T("Nicht ausgeschaltet - bitte die PS5 selbst neu starten oder ausschalten.", "Not turned off - please restart or turn off the PS5 yourself."));
#ifdef VORSCHAU
                ui_log(NULL, "PC-Vorschau: Beenden");
#else
                laeuft = false;
#endif
            } else if (K->simulation) {
                const char *t = T("SIMULATION: Hier würde die PS5 jetzt ausgeschaltet.", "SIMULATION: the PS5 would now turn off.");
                ui_log(NULL, t); kurz(t);
            } else if (K->SELBST_UEBERGEBEN && plattform_ausschalten_uebergeben()) {
                /* NEU 1.0.6 (da_Chris86 29.10.2026 23:08 / 30.10.2026 09:01): Kachel + Startordner entfernt der Hintergrundteil erst nach
                 * dem Ende der Oberfläche - ein sofortiges Ausschalten käme ihm zuvor. Darum schaltet ER aus, nach der Entfernung. */
                ui_log(NULL, T("PS5 Cleanup beendet sich - die Kachel wird entfernt, danach schaltet sich die PS5 aus.", "PS5 Cleanup quits - the tile is removed, then the PS5 turns off."));
                laeuft = false;
            } else {
                if (K->SELBST_UEBERGEBEN) ui_log(NULL, T("Der Hintergrundteil hat das Ausschalten nicht übernommen - die Kachel bleibt dabei stehen.", "The background part did not take over turning off - the tile stays."));
                ui_log(NULL, T("PS5 schaltet sich jetzt aus ...", "PS5 is turning off now ..."));
                if (!plattform_ausschalten()) { const char *t = T("Das hat nicht geklappt - bitte die PS5 über das PS5-Menü ganz ausschalten (nicht Ruhemodus).", "That did not work - please turn the PS5 off completely via the PS5 menu (not rest mode)."); ui_log(NULL, t); kurz(t); }
            }
            neu_zeichnen();
        }
#ifdef VORSCHAU
        skript_schritt();
#endif
        if (G.neu) { G.neu = false; zeichnen(); SDL_UpdateWindowSurface(fenster); letztes = SDL_GetTicks(); }
        SDL_Delay(16);
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
