/* PS5 Cleanup - Oberfläche am Fernseher
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * stil_11.h - NEU 1.1 (da_Chris86 15.11.2026, Entwurf "PS5 Cleanup 1.1 – Oberfläche"): Grundbausteine der neuen Oberfläche.
 * Farben aus da_Chris86s Palette (Hintergrund #0D1119, Karte #19232E, Aktion Blau #2D73D6, Akzent Türkis #36D4DC, Text #E8EDF5),
 * dazu Grün (geschützt) und Gold (Jailbreak). Milchglas: Der Hintergrund (bilder_11/hintergrund.png) wird einmal beim Start geladen
 * und einmal verschwommen in einem Viertel der Größe abgelegt - Karten mischen ihre Farbe mit diesem verschwommenen Bild. Die PS5
 * zeichnet ohne Grafikchip; so kostet der Effekt beim Bedienen nichts. Reicht der Arbeitsspeicher nicht (Bild nicht geladen), zeichnen
 * alle Bausteine dieselben Flächen ohne Glas (schlichte Fassung) - nie ein Absturz.
 * Wird von app.c eingebunden (nach den Zeichen-Grundfunktionen); alles static.
 */

/* ------------------------------------------------------------------ Farben (0xRRGGBB) */
#define S_HG        0x0D1119
#define S_KARTE     0x19232E
#define S_BLAU      0x2D73D6
#define S_BLAU_H    0x5B9BF0   /* Rand des Hauptknopfs */
#define S_TUERKIS   0x36D4DC
#define S_TEXT      0xE8EDF5
#define S_TEXT_W    0xFFFFFF
#define S_TEXT2     0xC7D2E3   /* Nebentext */
#define S_GRAU      0xA3AEC2   /* Unterzeilen */
#define S_GRAU2     0x8B9AB3   /* Überschriften in Großbuchstaben, ruhige Symbole */
#define S_GRAU3     0x5B6B82
#define S_LINIE     0x3B4A63
#define S_GRUEN     0x34D399
#define S_GRUEN_H   0x6EE7B7
#define S_GRUEN_R   0x2E9B74
#define S_GOLD      0xF5B942
#define S_GOLD_R    0xB8862E
#define S_GOLD_T    0xF2E3BF
#define S_ROT       0xFF7B76
#define S_SYM       0xD5DEEB   /* Linien-Symbole */

/* ------------------------------------------------------------------ Pixel schnell lesen/schreiben (32 Bit, 8 Bit je Farbe) */
static int px_ok = -1; static Uint32 px_rs, px_gs, px_bs, px_am;
static bool px_bereit(void)
{
    if (px_ok < 0) {
        SDL_PixelFormat *f = ziel->format;
        px_ok = f->BytesPerPixel == 4 && f->Rloss == 0 && f->Gloss == 0 && f->Bloss == 0;
        px_rs = f->Rshift; px_gs = f->Gshift; px_bs = f->Bshift; px_am = f->Amask;
    }
    return px_ok == 1;
}
#define PX_R(p) (((p) >> px_rs) & 255)
#define PX_G(p) (((p) >> px_gs) & 255)
#define PX_B(p) (((p) >> px_bs) & 255)
#define PX(r, g, b) ((((Uint32)(r)) << px_rs) | (((Uint32)(g)) << px_gs) | (((Uint32)(b)) << px_bs) | px_am)
static inline Uint32 *px_zeile(int y) { return (Uint32 *)((Uint8 *)ziel->pixels + y * ziel->pitch); }
/* Farbe c (0xRRGGBB) mit Deckkraft a (0..1) über das Pixel mischen */
static inline void px_misch(Uint32 *p, unsigned r, unsigned g, unsigned b, float a)
{
    if (a <= 0.f) return; if (a >= 1.f) { *p = PX(r, g, b); return; }
    unsigned ai = (unsigned)(a * 256.f), bi = 256 - ai, q = *p;
    *p = PX((r * ai + PX_R(q) * bi) >> 8, (g * ai + PX_G(q) * bi) >> 8, (b * ai + PX_B(q) * bi) >> 8);
}
static inline bool im_ausschnitt(int x, int y) { return x >= clip_x0 && x < clip_x1 && y >= clip_y0 && y < clip_y1 && x >= 0 && y >= 0 && x < ziel->w && y < ziel->h; }

/* ------------------------------------------------------------------ Bilder (RGBA aus bilder_11, beim Start dekodiert) */
EINBINDEN(b11_hintergrund, "bilder_11/hintergrund.png")
EINBINDEN(b11_banner, "bilder_11/banner.jpg")
EINBINDEN(b11_logo, "bilder_11/logo_schrift_36.png")
EINBINDEN(b11_k_ps5_lw, "bilder_11/konsole_ps5_lw.png")
EINBINDEN(b11_k_ps5_dig, "bilder_11/konsole_ps5_dig.png")
EINBINDEN(b11_k_slim_lw, "bilder_11/konsole_slim_lw.png")
EINBINDEN(b11_k_slim_dig, "bilder_11/konsole_slim_dig.png")
EINBINDEN(b11_k_pro_lw, "bilder_11/konsole_pro_lw.png")
EINBINDEN(b11_k_pro_dig, "bilder_11/konsole_pro_dig.png")
EINBINDEN(b11_s_spiele_g, "bilder_11/sym_spiele_132.png")
EINBINDEN(b11_s_apps_g, "bilder_11/sym_apps_132.png")
EINBINDEN(b11_s_spielst_g, "bilder_11/sym_spielstaende_132.png")
EINBINDEN(b11_s_troph_g, "bilder_11/sym_trophaeen_132.png")
EINBINDEN(b11_s_bilder_g, "bilder_11/sym_bilder_132.png")
EINBINDEN(b11_s_sonst_g, "bilder_11/sym_sonstiges_132.png")
EINBINDEN(b11_s_jb_g, "bilder_11/sym_jailbreak_132.png")
EINBINDEN(b11_s_spiele_k, "bilder_11/sym_spiele_40.png")
EINBINDEN(b11_s_apps_k, "bilder_11/sym_apps_40.png")
EINBINDEN(b11_s_spielst_k, "bilder_11/sym_spielstaende_40.png")
EINBINDEN(b11_s_troph_k, "bilder_11/sym_trophaeen_40.png")
EINBINDEN(b11_s_bilder_k, "bilder_11/sym_bilder_40.png")
EINBINDEN(b11_s_sonst_k, "bilder_11/sym_sonstiges_40.png")
EINBINDEN(b11_s_jb_k, "bilder_11/sym_jailbreak_40.png")

typedef struct { int w, h; unsigned char *px; } bild11;   /* RGBA, Zeile für Zeile */
static bild11 bild11_laden(const unsigned char *d, const unsigned char *e)
{
    bild11 b = {0}; int n = 0;
    b.px = stbi_load_from_memory(d, (int)(e - d), &b.w, &b.h, &n, 4);
    if (!b.px) { b.w = b.h = 0; }
    return b;
}
#define B11(name) bild11_laden(name, name##_ende)
static bild11 b11_banner_b, b11_logo_b, b11_kon[6], b11_sym_g[7], b11_sym_k[7];
static bool b11_geladen = false;
/* Hintergrund in voller Größe (im Format von ziel, zum schnellen Kopieren) und verschwommen in 1/4 (RGB, 480 x 270) */
static SDL_Surface *b11_hg;
#define UNSCH_W (BREITE / 4)
#define UNSCH_H (HOEHE / 4)
static unsigned char *b11_unscharf;   /* UNSCH_W * UNSCH_H * 3 */

static void unscharf_bauen(const unsigned char *rgba, int w, int h)
{
    b11_unscharf = malloc((size_t)UNSCH_W * UNSCH_H * 3);
    float *t = malloc(sizeof(float) * UNSCH_W * UNSCH_H * 3), *u = malloc(sizeof(float) * UNSCH_W * UNSCH_H * 3);
    if (!b11_unscharf || !t || !u) { free(b11_unscharf); b11_unscharf = NULL; free(t); free(u); return; }
    /* verkleinern (Mittel aus 4 x 4) */
    for (int y = 0; y < UNSCH_H; y++) for (int x = 0; x < UNSCH_W; x++) for (int c = 0; c < 3; c++) {
        unsigned s = 0;
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {
            int sx = x * 4 + i, sy = y * 4 + j; if (sx >= w) sx = w - 1; if (sy >= h) sy = h - 1;
            s += rgba[((size_t)sy * w + sx) * 4 + c];
        }
        t[((size_t)y * UNSCH_W + x) * 3 + c] = s / 16.f;
    }
    /* dreimal Kastenfilter Radius 4 (in voller Größe etwa 16 px, wie blur(16px) im Entwurf) - waagrecht, dann senkrecht */
    const int R = 4;
    for (int durch = 0; durch < 3; durch++) {
        for (int y = 0; y < UNSCH_H; y++) for (int x = 0; x < UNSCH_W; x++) for (int c = 0; c < 3; c++) {
            float s = 0; int n = 0;
            for (int k = -R; k <= R; k++) { int xx = x + k; if (xx < 0 || xx >= UNSCH_W) continue; s += t[((size_t)y * UNSCH_W + xx) * 3 + c]; n++; }
            u[((size_t)y * UNSCH_W + x) * 3 + c] = s / n;
        }
        for (int y = 0; y < UNSCH_H; y++) for (int x = 0; x < UNSCH_W; x++) for (int c = 0; c < 3; c++) {
            float s = 0; int n = 0;
            for (int k = -R; k <= R; k++) { int yy = y + k; if (yy < 0 || yy >= UNSCH_H) continue; s += u[((size_t)yy * UNSCH_W + x) * 3 + c]; n++; }
            t[((size_t)y * UNSCH_W + x) * 3 + c] = s / n;
        }
    }
    for (size_t i = 0; i < (size_t)UNSCH_W * UNSCH_H * 3; i++) { float v = t[i] * 1.12f; b11_unscharf[i] = (unsigned char)(v > 255.f ? 255.f : v); }   /* saturate(140%) grob: etwas heller */
    free(t); free(u);
}
static void stil11_laden(void)
{
    if (b11_geladen) return;
    b11_geladen = true;
    bild11 hg = B11(b11_hintergrund);
    if (hg.px && hg.w == BREITE && hg.h == HOEHE) {
        unscharf_bauen(hg.px, hg.w, hg.h);
        SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(hg.px, hg.w, hg.h, 32, hg.w * 4, SDL_PIXELFORMAT_RGBA32);
        if (s) { b11_hg = SDL_ConvertSurface(s, ziel->format, 0); SDL_FreeSurface(s); }
    }
    if (hg.px) stbi_image_free(hg.px);
    b11_banner_b = B11(b11_banner); b11_logo_b = B11(b11_logo);
    b11_kon[0] = B11(b11_k_ps5_lw); b11_kon[1] = B11(b11_k_ps5_dig); b11_kon[2] = B11(b11_k_slim_lw);
    b11_kon[3] = B11(b11_k_slim_dig); b11_kon[4] = B11(b11_k_pro_lw); b11_kon[5] = B11(b11_k_pro_dig);
    b11_sym_g[0] = B11(b11_s_spiele_g); b11_sym_g[1] = B11(b11_s_apps_g); b11_sym_g[2] = B11(b11_s_spielst_g); b11_sym_g[3] = B11(b11_s_troph_g);
    b11_sym_g[4] = B11(b11_s_bilder_g); b11_sym_g[5] = B11(b11_s_sonst_g); b11_sym_g[6] = B11(b11_s_jb_g);
    b11_sym_k[0] = B11(b11_s_spiele_k); b11_sym_k[1] = B11(b11_s_apps_k); b11_sym_k[2] = B11(b11_s_spielst_k); b11_sym_k[3] = B11(b11_s_troph_k);
    b11_sym_k[4] = B11(b11_s_bilder_k); b11_sym_k[5] = B11(b11_s_sonst_k); b11_sym_k[6] = B11(b11_s_jb_k);
}
static bool glas_da(void) { return b11_hg && b11_unscharf && px_bereit(); }

/* ganzer Hintergrund (schlichte Fassung: einfarbig) */
static void hintergrund11(void)
{
    stil11_laden();
    if (b11_hg) SDL_BlitSurface(b11_hg, NULL, ziel, NULL);
    else rechteck(0, 0, BREITE, HOEHE, S_HG);
}
/* Pixel (x, y) mit Anteil a zum Hintergrundbild zurückmischen (weiches Ausblenden laufender Texte auf dem Hintergrund) */
static void hg_misch(int x, int y, float a)
{
    if (!im_ausschnitt(x, y)) return;
    if (!b11_hg || !px_bereit() || b11_hg->format->BytesPerPixel != 4) { misch(x, y, S_HG, a); return; }
    Uint32 q = ((Uint32 *)((Uint8 *)b11_hg->pixels + y * b11_hg->pitch))[x];
    px_misch(&px_zeile(y)[x], PX_R(q), PX_G(q), PX_B(q), a);
}
/* verschwommener Hintergrund an (x, y) - bilinear aus dem Viertel-Bild */
static inline void unscharf_an(int x, int y, unsigned *r, unsigned *g, unsigned *b)
{
    float fx = x / 4.f - .5f, fy = y / 4.f - .5f; if (fx < 0) fx = 0; if (fy < 0) fy = 0;
    int x0 = (int)fx, y0 = (int)fy; if (x0 > UNSCH_W - 2) x0 = UNSCH_W - 2; if (y0 > UNSCH_H - 2) y0 = UNSCH_H - 2;
    float ax = fx - x0, ay = fy - y0; if (ax > 1) ax = 1; if (ay > 1) ay = 1;
    const unsigned char *p = b11_unscharf + ((size_t)y0 * UNSCH_W + x0) * 3, *q = p + UNSCH_W * 3;
    float w00 = (1 - ax) * (1 - ay), w10 = ax * (1 - ay), w01 = (1 - ax) * ay, w11 = ax * ay;
    *r = (unsigned)(p[0] * w00 + p[3] * w10 + q[0] * w01 + q[3] * w11);
    *g = (unsigned)(p[1] * w00 + p[4] * w10 + q[1] * w01 + q[4] * w11);
    *b = (unsigned)(p[2] * w00 + p[5] * w10 + q[2] * w01 + q[5] * w11);
}

/* ------------------------------------------------------------------ Flächen mit runden Ecken
 * Abstand eines Punktes (Pixelmitte) zum Rand eines Rechtecks mit runden Ecken: negativ = innen */
static inline float rand_abstand(float px, float py, float x, float y, float w, float h, float r)
{
    float hx = w / 2.f, hy = h / 2.f, qx = fabsf(px - (x + hx)) - (hx - r), qy = fabsf(py - (y + hy)) - (hy - r);
    float ax = qx > 0 ? qx : 0, ay = qy > 0 ? qy : 0, innen = qx > qy ? qx : qy; if (innen > 0) innen = 0;
    return (ax > 0 || ay > 0 ? sqrtf(ax * ax + ay * ay) : 0) + innen - r;
}
/* Farbverlauf: bis zu 3 Stufen (Farbe, Deckkraft, Lage 0..1), Richtung in Grad wie CSS (180 = oben nach unten, 135 = links oben nach rechts unten) */
typedef struct { int n; uint32_t f[3]; float a[3], pos[3]; float grad; } verlauf;
static verlauf V1(uint32_t f, float a) { verlauf v = { 1, { f }, { a }, { 0 }, 180 }; return v; }
static verlauf V2(float grad, uint32_t f0, float a0, uint32_t f1, float a1) { verlauf v = { 2, { f0, f1 }, { a0, a1 }, { 0, 1 }, grad }; return v; }
static verlauf V3(float grad, uint32_t f0, float a0, float p1, uint32_t f1, float a1, uint32_t f2, float a2) { verlauf v = { 3, { f0, f1, f2 }, { a0, a1, a2 }, { 0, p1, 1 }, grad }; return v; }
static inline void verlauf_an(const verlauf *v, float t, unsigned *r, unsigned *g, unsigned *b, float *a)
{
    int i = 0; if (v->n == 1) t = 0;
    while (i < v->n - 2 && t > v->pos[i + 1]) i++;
    float s = v->n == 1 ? 0 : (t - v->pos[i]) / (v->pos[i + 1] - v->pos[i]); if (s < 0) s = 0; if (s > 1) s = 1;
    uint32_t c0 = v->f[i], c1 = v->f[v->n == 1 ? 0 : i + 1];
    *r = (unsigned)(((c0 >> 16) & 255) * (1 - s) + ((c1 >> 16) & 255) * s);
    *g = (unsigned)(((c0 >> 8) & 255) * (1 - s) + ((c1 >> 8) & 255) * s);
    *b = (unsigned)((c0 & 255) * (1 - s) + (c1 & 255) * s);
    *a = v->a[i] * (1 - s) + v->a[v->n == 1 ? 0 : i + 1] * s;
}
/* Fläche: glas = Milchglas darunter (der Verlauf liegt darüber), rand = Randfarbe mit Deckkraft oben/unten (Licht von oben),
 * glanz = feine helle Linie innen oben (inset 0 1px). d = Randbreite. */
typedef struct { bool glas; verlauf fuell; uint32_t rand; float rand_oben, rand_unten; int d; float glanz; uint32_t glanz_f; } flaeche;
static void flaeche_zeichnen(int x, int y, int w, int h, int r, const flaeche *s)
{
    if (w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2; if (r * 2 > h) r = h / 2;
    bool schnell = px_bereit(), glas = s->glas && glas_da();
    float rad = (float)(s->fuell.grad * 3.14159265f / 180.f), dx = sinf(rad), dy = -cosf(rad);
    float halb = fabsf(dx) * w / 2.f + fabsf(dy) * h / 2.f; if (halb < 1) halb = 1;
    float cx = x + w / 2.f, cy = y + h / 2.f;
    int y0 = y < clip_y0 ? clip_y0 : y, y1 = y + h > clip_y1 ? clip_y1 : y + h, x0 = x < clip_x0 ? clip_x0 : x, x1 = x + w > clip_x1 ? clip_x1 : x + w;
    if (y0 < 0) y0 = 0; if (x0 < 0) x0 = 0; if (y1 > ziel->h) y1 = ziel->h; if (x1 > ziel->w) x1 = ziel->w;
    unsigned rr = (s->rand >> 16) & 255, rg = (s->rand >> 8) & 255, rb = s->rand & 255;
    unsigned gr_ = (s->glanz_f >> 16) & 255, gg = (s->glanz_f >> 8) & 255, gb = s->glanz_f & 255;
    for (int py = y0; py < y1; py++) {
        Uint32 *z = schnell ? px_zeile(py) : NULL;
        float fy = py + .5f, ty = (fy - y) / (float)h;
        float randa = s->rand_oben + (s->rand_unten - s->rand_oben) * (ty < .35f ? ty / .35f : 1.f);
        for (int px = x0; px < x1; px++) {
            float fx = px + .5f, dist = rand_abstand(fx, fy, (float)x, (float)y, (float)w, (float)h, (float)r);
            float bedeckt = .5f - dist; if (bedeckt <= 0) continue; if (bedeckt > 1) bedeckt = 1;
            float t = ((fx - cx) * dx + (fy - cy) * dy) / (2 * halb) + .5f;
            unsigned fr, fg, fb; float fa; verlauf_an(&s->fuell, t, &fr, &fg, &fb, &fa);
            if (!schnell) {   /* langsame Fassung: nur Füllung und Rand */
                misch(px, py, (fr << 16) | (fg << 8) | fb, fa * bedeckt);
                if (s->d > 0 && dist > -s->d) misch(px, py, s->rand, randa * bedeckt);
                continue;
            }
            Uint32 *p = z + px;
            if (glas) {   /* Milchglas: verschwommener Hintergrund statt des Pixels darunter */
                unsigned ur, ug, ub; unscharf_an(px, py, &ur, &ug, &ub);
                unsigned nr = (unsigned)(fr * fa + ur * (1 - fa)), ng = (unsigned)(fg * fa + ug * (1 - fa)), nb = (unsigned)(fb * fa + ub * (1 - fa));
                px_misch(p, nr, ng, nb, bedeckt);
            } else px_misch(p, fr, fg, fb, fa * bedeckt);
            if (s->glanz > 0 && py - y >= s->d && py - y < s->d + 1 + (r > 6 ? 0 : 0) && dist < -s->d) px_misch(p, gr_, gg, gb, s->glanz * bedeckt);
            if (s->d > 0 && dist > -s->d) { float k = dist > -s->d + 1 ? 1 : (dist + s->d); if (k > 1) k = 1; px_misch(p, rr, rg, rb, randa * bedeckt * (k > 0 ? 1 : 0)); }
        }
    }
}
/* Leuchten außen (box-shadow 0 0 R): Farbe mit Deckkraft a, nimmt nach außen ab */
static void leuchten(int x, int y, int w, int h, int r, int R, uint32_t farbe, float a)
{
    if (!px_bereit() || R <= 0) return;
    unsigned fr = (farbe >> 16) & 255, fg = (farbe >> 8) & 255, fb = farbe & 255;
    for (int py = y - R; py < y + h + R; py++) {
        if (py < 0 || py >= ziel->h || py < clip_y0 || py >= clip_y1) continue;
        Uint32 *z = px_zeile(py);
        bool mitte = py >= y + r && py < y + h - r;
        for (int px = x - R; px < x + w + R; px++) {
            if (mitte && px >= x && px < x + w) { px = x + w - 1; continue; }
            if (px < 0 || px >= ziel->w || px < clip_x0 || px >= clip_x1) continue;
            float d = rand_abstand(px + .5f, py + .5f, (float)x, (float)y, (float)w, (float)h, (float)r);
            if (d <= 0 || d >= R) continue;
            float k = 1.f - d / R; px_misch(z + px, fr, fg, fb, a * k * k);
        }
    }
}

/* ------------------------------------------------------------------ fertige Flächen des Entwurfs */
static flaeche F_KARTE11(void)   /* Milchglas-Karte */
{ flaeche f = { true, V2(180, 0x1E2A38, .58f, 0x141D28, .66f), 0xFFFFFF, .20f, .09f, 1, .07f, 0xFFFFFF }; return f; }
static flaeche F_FELD11(void)    /* Zeile/Feld in einer Karte */
{ flaeche f = { false, V1(0xFFFFFF, .045f), 0xFFFFFF, .09f, .09f, 1, 0, 0 }; return f; }
static flaeche F_FELD11_OHNE(void) { flaeche f = { false, V1(0xFFFFFF, .045f), 0, 0, 0, 0, 0, 0 }; return f; }
static flaeche F_FOKUS11(int d)   /* gewählte Zeile/Kachel: weicher blauer Verlauf, Rand Blau, Glanz oben */
{ flaeche f = { false, V2(180, 0x284E82, .75f, 0x1C3862, .75f), S_BLAU, 1, 1, d, .22f, 0xAACDFF }; return f; }
static flaeche F_GRUEN11(void)
{ flaeche f = { false, V3(135, S_GRUEN, .20f, .4f, 0x08201A, .55f, 0x06130F, .5f), S_GRUEN_R, 1, 1, 1, .22f, S_GRUEN_H }; return f; }
static flaeche F_GOLD11(int d)
{ flaeche f = { false, V3(135, S_GOLD, .22f, .4f, 0x241B08, .55f, 0x150F05, .5f), d > 1 ? S_GOLD : S_GOLD_R, 1, 1, d, .22f, 0xFCD378 }; return f; }
static flaeche F_HAUPT11(void)    /* Hauptknopf */
{ flaeche f = { false, V2(180, 0x3580E6, 1, 0x2563C0, 1), S_BLAU_H, 1, 1, 2, .25f, 0xAACDFF }; return f; }
static flaeche F_NEBEN11(void)    /* Nebenknopf */
{ flaeche f = { false, V1(0xFFFFFF, .045f), S_LINIE, 1, 1, 2, 0, 0 }; return f; }
static flaeche F_WEG11(void)      /* große Wege-Kachel (blau) */
{ flaeche f = { true, V2(160, 0x2D64B4, .55f, 0x183056, .6f), S_BLAU, 1, 1, 2, .25f, 0xAACDFF }; return f; }
static flaeche F_WEGGOLD11(void)  /* große Wege-Kachel (golden, nicht gewählt) */
{ flaeche f = { true, V3(160, S_GOLD, .12f, .45f, 0x14110A, .5f, 0x0C0D12, .45f), 0xA37E33, 1, .75f, 2, .15f, 0xFCD378 }; return f; }
static void karte11(int x, int y, int w, int h, int r) { flaeche f = F_KARTE11(); leuchten(x, y + 10, w, h, r, 30, 0x000000, .35f); flaeche_zeichnen(x, y, w, h, r, &f); }
static void fokus11(int x, int y, int w, int h, int r, int d) { flaeche f = F_FOKUS11(d); leuchten(x, y, w, h, r, 22, S_BLAU, .40f); flaeche_zeichnen(x, y, w, h, r, &f); }
static void gruen11(int x, int y, int w, int h, int r) { flaeche f = F_GRUEN11(); leuchten(x, y, w, h, r, 20, S_GRUEN, .20f); flaeche_zeichnen(x, y, w, h, r, &f); }
static void gold11(int x, int y, int w, int h, int r, int d) { flaeche f = F_GOLD11(d); leuchten(x, y, w, h, r, 20, S_GOLD, d > 1 ? .25f : .18f); flaeche_zeichnen(x, y, w, h, r, &f); }
static void feld11(int x, int y, int w, int h, int r) { flaeche f = F_FELD11(); flaeche_zeichnen(x, y, w, h, r, &f); }

/* ------------------------------------------------------------------ RGBA-Bild zeichnen (skaliert, bilinear, mit runden Ecken r) */
static void bild11_zeichnen(const bild11 *b, int x, int y, int w, int h, int r)
{
    if (!b->px || w <= 0 || h <= 0 || !px_bereit()) return;
    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= ziel->h || py < clip_y0 || py >= clip_y1) continue;
        Uint32 *z = px_zeile(py);
        float sy = (py - y + .5f) * b->h / (float)h - .5f; if (sy < 0) sy = 0; int y0 = (int)sy; float ay = sy - y0; int y1 = y0 + 1 < b->h ? y0 + 1 : y0;
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= ziel->w || px < clip_x0 || px >= clip_x1) continue;
            float bed = 1;
            if (r > 0) { float d = rand_abstand(px + .5f, py + .5f, (float)x, (float)y, (float)w, (float)h, (float)r); bed = .5f - d; if (bed <= 0) continue; if (bed > 1) bed = 1; }
            float sx = (px - x + .5f) * b->w / (float)w - .5f; if (sx < 0) sx = 0; int x0 = (int)sx; float ax = sx - x0; int x1 = x0 + 1 < b->w ? x0 + 1 : x0;
            const unsigned char *p00 = b->px + ((size_t)y0 * b->w + x0) * 4, *p10 = b->px + ((size_t)y0 * b->w + x1) * 4,
                                *p01 = b->px + ((size_t)y1 * b->w + x0) * 4, *p11 = b->px + ((size_t)y1 * b->w + x1) * 4;
            float w00 = (1 - ax) * (1 - ay), w10 = ax * (1 - ay), w01 = (1 - ax) * ay, w11 = ax * ay;
            float a = (p00[3] * w00 + p10[3] * w10 + p01[3] * w01 + p11[3] * w11) / 255.f;
            if (a <= 0.003f) continue;
            /* vormultipliziert mischen, damit die Ränder durchsichtiger Bilder keinen dunklen Saum bekommen */
            float cr = (p00[0] * p00[3] * w00 + p10[0] * p10[3] * w10 + p01[0] * p01[3] * w01 + p11[0] * p11[3] * w11) / 255.f / a;
            float cg = (p00[1] * p00[3] * w00 + p10[1] * p10[3] * w10 + p01[1] * p01[3] * w01 + p11[1] * p11[3] * w11) / 255.f / a;
            float cb = (p00[2] * p00[3] * w00 + p10[2] * p10[3] * w10 + p01[2] * p01[3] * w01 + p11[2] * p11[3] * w11) / 255.f / a;
            px_misch(z + px, (unsigned)cr, (unsigned)cg, (unsigned)cb, a * bed);
        }
    }
}
/* Schriftzug "PS5 Cleanup" oben links (Höhe des Bildes 45 px) */
static int logo11(int x, int y)
{
    stil11_laden();
    if (b11_logo_b.px) { bild11_zeichnen(&b11_logo_b, x, y, b11_logo_b.w, b11_logo_b.h, 0); return b11_logo_b.w; }
    text(x, y + 36, 3, 36, S_TEXT_W, "PS5 Cleanup"); return textbreite(3, 36, "PS5 Cleanup");
}
/* SDL-Bild (beliebiges Format, ohne Durchsichtigkeit) mit runden Ecken zeichnen - statt ecken_maske, die eine feste Hintergrundfarbe brauchte */
static void surf_rund(SDL_Surface *b, int x, int y, int w, int h, int r)
{
    if (!b || !px_bereit()) { if (b) { SDL_Rect z = { x, y, w, h }; SDL_BlitScaled(b, NULL, ziel, &z); } return; }
    if (SDL_MUSTLOCK(b)) SDL_LockSurface(b);
    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= ziel->h || py < clip_y0 || py >= clip_y1) continue;
        Uint32 *z = px_zeile(py); int sy = (py - y) * b->h / h;
        const Uint8 *q = (const Uint8 *)b->pixels + sy * b->pitch;
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= ziel->w || px < clip_x0 || px >= clip_x1) continue;
            float d = rand_abstand(px + .5f, py + .5f, (float)x, (float)y, (float)w, (float)h, (float)r), bed = .5f - d;
            if (bed <= 0) continue; if (bed > 1) bed = 1;
            int sx = (px - x) * b->w / w; Uint32 v = 0; memcpy(&v, q + sx * b->format->BytesPerPixel, b->format->BytesPerPixel > 4 ? 4 : b->format->BytesPerPixel);
            Uint8 cr, cg, cb; SDL_GetRGB(v, b->format, &cr, &cg, &cb);
            px_misch(z + px, cr, cg, cb, bed);
        }
    }
    if (SDL_MUSTLOCK(b)) SDL_UnlockSurface(b);
}
