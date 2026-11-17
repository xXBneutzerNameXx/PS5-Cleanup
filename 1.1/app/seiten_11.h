/* PS5 Cleanup - Oberfläche am Fernseher
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * seiten_11.h - NEU 1.1 (da_Chris86 15.11.2026, Entwurf "PS5 Cleanup 1.1 – Oberfläche"): die Seiten der neuen Oberfläche.
 * Maße aus dem Entwurf (1920 x 1080): Rand 80 links/rechts, 54 oben, 48 unten (Fernseh-Vorgaben), Abstände 8/16/24/32,
 * Schrift 24 (Hinweise, Fußleiste) / 26-28 (Text) / 30-34 (Titel in Zeilen) / 40 (Karten) / 52-64 (Seitentitel).
 * Wird von app.c eingebunden (vor schirm_start); alles static. Die Bedienlogik bleibt in app.c.
 */
#define R11_X 80
#define R11_O 54
#define R11_U 48
#define FUSS11_Y (HOEHE - R11_U - 44)   /* obere Kante der Fußleiste (Höhe 44) */

static int gl11(int oben, int h, int gr) { return oben + (h + gr * 727 / 1000) / 2; }   /* Grundlinie: Schrift mittig in einer Zeile der Höhe h (Inter: Versalhöhe 0,727) */

/* ------------------------------------------------------------------ Tasten der Fußleiste (Ring mit Zeichen, wie im Entwurf) */
static int knopf11_breite(knopfart k)
{
    if (k == KN_L1R1) return textbreite(1, 22, "L1") * 2 + 24 + 8 + 4;
    if (k == KN_R1) return textbreite(1, 22, "R1") + 14;
    return 42;
}
static int kappe11(int x, int cy, const char *t, uint32_t f);
static void knopf11(knopfart k, int x, int cy, uint32_t f)
{
    if (k == KN_L1R1) {
        int w = textbreite(1, 22, "L1") + 12;
        flaeche r = { false, V1(0, 0), f, 1, 1, 2, 0, 0 };
        flaeche_zeichnen(x, cy - 17, w, 34, 9, &r); text(x + 6, gl11(cy - 17, 34, 22), 1, 22, f, "L1");
        flaeche_zeichnen(x + w + 8, cy - 17, w, 34, 9, &r); text(x + w + 14, gl11(cy - 17, 34, 22), 1, 22, f, "R1");
        return;
    }
    if (k == KN_R1) { kappe11(x, cy, "R1", f); return; }
    ring(x + 21, cy, 19.5f, 1.8f, f);
    switch (k) {
    case KN_OPT: symbol(SYM_KN_OPTIONEN, x + 12, cy - 9, 18, f); break;
    case KN_HOCHRUNTER: symbol(SYM_KN_HOCH_RUNTER, x + 12, cy - 9, 18, f); break;
    case KN_LINKSRECHTS: symbol(SYM_KN_LINKS_RECHTS, x + 11, cy - 7, 20, f); break;
    default: {
        const float *s = k == KN_X ? SYM_KN_KREUZ : k == KN_O ? SYM_KN_KREIS : k == KN_DREIECK ? SYM_KN_DREIECK : SYM_KN_QUADRAT;
        symbol(s, x + 12, cy - 9, 18, f);
    } }
}
/* "L2"/"R2" usw. als Tastenkappe */
static int kappe11(int x, int cy, const char *t, uint32_t f)
{
    int w = textbreite(1, 22, t) + 14;
    flaeche r = { false, V1(0, 0), f, 1, 1, 2, 0, 0 };
    flaeche_zeichnen(x, cy - 17, w, 34, 9, &r); text(x + 7, gl11(cy - 17, 34, 22), 1, 22, f, t);
    return w;
}
/* Versions-Pille unten rechts: grau, oder golden mit "Update x verfügbar" (da_Chris86 15.11.2026 13:29) */
static void lade_symbol(int x, int y, int s, uint32_t f)   /* Pfeil nach unten auf einem Strich (Herunterladen) */
{
    int d = s / 10 < 2 ? 2 : s / 10, cx = x + s / 2;
    rund(cx - d / 2, y + s / 12, d, s * 7 / 12, d / 2, f);
    symbol(SYM_RUNTER, x + s / 6, y + s / 3, s * 2 / 3, f);
    rund(x + s / 8, y + s - d - s / 12, s - s / 4, d, d / 2, f);
}
static int version_pille11(int xr, int cy, int xl)   /* xl: bis dahin reichen die Tasten links */
{
    char un[32]; upd_holen(un, sizeof un);
    if (un[0]) {
        const char *b = a_fmt(fa, T("Update %s verfügbar", "Update %s available"), un);
#ifdef VERSION_BAU   /* NEU 1.1 (da_Chris86 15.11.2026 23:24): Testfassungen zeigen hier die Bauzeit statt der Versionsnummer - sonst ist sie bei verfügbarem Update nirgends zu sehen */
        const char *a = VERB(fa, SP_EN ? VERSION_BAU_EN : VERSION_BAU, " · ");
        if (12 + 22 + 10 + textbreite(2, 24, a) + textbreite(1, 24, b) + 16 > xr - xl) a = VERB(fa, SP_EN ? "Build " : "Bau ", __TIME__, " · ");   /* zu eng neben den Tasten: nur die Uhrzeit */
#else
        (void)xl;
        const char *a = VERB(fa, "Version ", VERSION_NUMMER, " · ");
#endif
        int w = 12 + 22 + 10 + textbreite(2, 24, a) + textbreite(1, 24, b) + 16, x = xr - w;
        gold11(x, cy - 20, w, 40, 20, 1);
        lade_symbol(x + 12, cy - 11, 22, S_GOLD);
        text(x + 44, gl11(cy - 20, 40, 24), 2, 24, S_GOLD_T, a);
        text(x + 44 + textbreite(2, 24, a), gl11(cy - 20, 40, 24), 1, 24, S_GOLD, b);
        return w;
    }
    const char *v = VERSION_ANZEIGE; int w = textbreite(0, 24, v) + 32, x = xr - w;
    flaeche r = { false, V1(0, 0), 0x2A3B57, 1, 1, 1, 0, 0 }; flaeche_zeichnen(x, cy - 20, w, 40, 20, &r);
    text(x + 16, gl11(cy - 20, 40, 24), 0, 24, S_GRAU, v);
    return w;
}
/* NEU 1.1 (da_Chris86 16.11.2026 17:58: "unten die Tasten deutlicher hervorstechen lassen"): jede Taste der Fußleiste auf einer eigenen
 * hellen Fläche mit Rahmen, Zeichen und Text weiß statt grau. Gibt die Breite der Fläche zurück (inhalt: Taste + Abstand + Text) */
#define FUSS_PILLE_L 6
#define FUSS_PILLE_R 18
static int fuss_pille11(int x, int cy, int inhalt)
{
    int w = FUSS_PILLE_L + inhalt + FUSS_PILLE_R;
    flaeche f = { false, V1(0xFFFFFF, .07f), 0x3B4A63, 1, 1, 1, 0, 0 };
    flaeche_zeichnen(x, cy - 26, w, 52, 26, &f);
    return w;
}
/* Fußleiste: Tasten links, rechts Zusatz (Adresse) und Versions-Pille; dazu die kurze Meldung und der Halten-Balken wie bisher */
static bool fuss11_ohne_version;
static int hinweis_mx11;   /* einmalig: Mitte der kurzen Meldung (sonst Bildmitte) */   /* einmalig: rechts steht ein eigener Knopf (Auswählen: "Weiter zum Löschen") */
static void fuss11(const fussknopf *kn, int n, const char *rechts_zusatz)
{
    int cy = FUSS11_Y + 22, x = R11_X;
    for (int durch = 0; durch < 2; durch++) for (int i = 0; i < n; i++) {   /* erst die linken, dann die (früher rechts stehenden) Tasten */
        if (kn[i].rechts != (durch == 1)) continue;
        int w = fuss_pille11(x, cy, knopf11_breite(kn[i].k) + 12 + textbreite(1, 24, kn[i].t));
        knopf11(kn[i].k, x + FUSS_PILLE_L, cy, S_TEXT_W);
        text(x + FUSS_PILLE_L + knopf11_breite(kn[i].k) + 12, gl11(FUSS11_Y, 44, 24), 1, 24, S_TEXT_W, kn[i].t); x += w + 12;
    }
    int xr = BREITE - R11_X;
    if (!fuss11_ohne_version) xr -= version_pille11(xr, cy, x - 4) + 24;
    fuss11_ohne_version = false;
    if (rechts_zusatz && xr - textbreite(2, 24, rechts_zusatz) - 36 > x) {   /* nur, wenn Platz neben den Tasten ist */
        int w = textbreite(2, 24, rechts_zusatz);
        text(xr - w, gl11(FUSS11_Y, 44, 24), 2, 24, S_TUERKIS, rechts_zusatz);
        symbol(SYM_KONSOLE, xr - w - 36, cy - 13, 26, S_TEXT2);
    }
    if (hinweis_bis && (Sint32)(hinweis_bis - SDL_GetTicks()) > 0) {   /* kurze Meldung über der Fußleiste */
        int mx = hinweis_mx11 ? hinweis_mx11 : BREITE / 2, mw = (hinweis_mx11 ? 2 * (hinweis_mx11 - R11_X) : BREITE - 2 * R11_X) - 48, hb = 0;
        passend p = einpassen(2, 26, 24, 22, hinweis_kurz, mw, 2); p.lh = p.gr + 8;
        for (int i = 0; i < p.z.n; i++) { int b = textbreite(2, p.gr, p.z.z[i]); if (b > hb) hb = b; }
        int hh = 28 + p.z.n * p.lh, hy = (hinweis_ueber ? hinweis_ueber : FUSS11_Y - 16) - hh;
        /* undurchsichtig (PS5-Test 15.11.2026: die Schrift dahinter schien durch) - dunkler Grund mit Schatten, darüber der goldene Rahmen */
        leuchten(mx - hb / 2 - 28, hy, hb + 56, hh, 16, 24, 0x000000, .55f);
        rund(mx - hb / 2 - 28, hy, hb + 56, hh, 16, 0x1C170C);
        gold11(mx - hb / 2 - 28, hy, hb + 56, hh, 16, 1);
        for (int i = 0; i < p.z.n; i++) text_mitte(mx, hy + 12 + p.gr + i * p.lh, 2, p.gr, S_GOLD_T, p.z.z[i]);
    }
    hinweis_ueber = 0; hinweis_mx11 = 0;
    if (kreis_seit && !G.beschaeftigt) {   /* ○ halten: Balken am unteren Rand */
        int anteil = (int)(SDL_GetTicks() - kreis_seit); if (anteil > 1000) anteil = 1000;
        rechteck(0, HOEHE - 6, BREITE * anteil / 1000, 6, S_TUERKIS);
    }
}
/* Testfassungen und Testmodus: immer sichtbar (gelbe Pillen hinter dem Seitentitel) */
static int warn_pillen11(int x, int cy)
{
    const char *h[2] = { NULL, NULL }; int n = 0;
#ifndef KERN_LOESCHEN
#ifdef NUR_LESEN
    h[n++] = T("TEST: NUR LESEN", "TEST: READ ONLY");
#else
    h[n++] = "SIMULATION";
#endif
#endif
#ifndef NUR_LESEN
    if (testmodus) h[n++] = T("TESTMODUS", "TEST MODE");
#endif
    for (int i = 0; i < n; i++) {
        int w = textbreite(1, 22, h[i]) + 28; gold11(x, cy - 18, w, 36, 18, 1); text(x + 14, gl11(cy - 18, 36, 22), 1, 22, S_GOLD, h[i]); x += w + 12;
    }
    return x;
}
/* Kopfzeile der inneren Seiten: Schriftzug | Seitentitel ... Schritte (1 Prüfen, 2 Auswählen, 3 Löschen) */
static int schritte11_breite(int schritt)
{
    const char *sn[3] = { T("Prüfen", "Scan"), T("Auswählen", "Select"), T("Löschen", "Delete") };
    int ges = 0; for (int i = 0; i < 3; i++) ges += 40 + 10 + textbreite(i + 1 == schritt ? 1 : 0, 24, sn[i]) + (i < 2 ? 12 + 40 + 12 : 0);
    return ges;
}
static void schritte11(int xr, int cy, int schritt)
{
    const char *sn[3] = { T("Prüfen", "Scan"), T("Auswählen", "Select"), T("Löschen", "Delete") };
    int x = xr - schritte11_breite(schritt);
    for (int i = 0; i < 3; i++) {
        int nr = i + 1; char z[4]; snprintf(z, sizeof z, "%d", nr);
        if (nr < schritt) { kreis(x + 20, cy, 20, S_TEXT2); symbol(SYM_HAKEN, x + 10, cy - 10, 20, 0x0D1119); }
        else if (nr == schritt) { leuchten(x, cy - 20, 40, 40, 20, 12, S_TUERKIS, .5f); ring(x + 20, cy, 18.5f, 2.5f, S_TUERKIS); text_mitte(x + 20, gl11(cy - 20, 40, 22), 1, 22, S_TUERKIS, z); }
        else { ring(x + 20, cy, 19, 1.6f, S_GRAU3); text_mitte(x + 20, gl11(cy - 20, 40, 22), 0, 22, S_GRAU, z); }
        x += 50;
        text(x, gl11(cy - 20, 40, 24), nr == schritt ? 1 : 0, 24, nr == schritt ? S_TEXT_W : S_GRAU, sn[i]);
        x += textbreite(nr == schritt ? 1 : 0, 24, sn[i]);
        if (i < 2) { rechteck(x + 12, cy, 40, 2, S_LINIE); x += 64; }
    }
}
static void kopf11(const char *titel, int schritt)
{
    int cy = R11_O + 22, x = R11_X;
    x += logo11(x, cy - 22) + 24;
    rechteck(x, cy - 17, 2, 34, 0x222C3A); x += 26;
    text(x, gl11(cy - 22, 44, 30), 1, 30, S_TEXT, titel); x += textbreite(1, 30, titel) + 24;
    x = warn_pillen11(x, cy);
    if (schritt > 0 && x + 12 <= BREITE - R11_X - schritte11_breite(schritt)) schritte11(BREITE - R11_X, cy, schritt);   /* nur, wenn Platz ist */
}

/* ------------------------------------------------------------------ obere Leiste der Startseite: Banner und drei Zeilen */
static const bild11 *konsole_bild11(void)
{
    stil11_laden();
    const char *t = K->MODELL_TYP; int i = !strcmp(t, "slim") ? 2 : !strcmp(t, "pro") ? 4 : 0;
    bool lw = !strcmp(t, "fat") ? !K->MODELL_DIGITAL : kern_kopplungs_stufe(K) == KOPPLUNG_BESTAETIGT;   /* Slim/Pro: Laufwerk gekoppelt = dran */
    return &b11_kon[i + (lw ? 0 : 1)];
}
static void chip11(int x, int y, int w, int h, int art, const float *sym, const bild11 *bild, const char *titel, const char *unter)
{   /* art: 0 Glas, 1 grün, 2 gold, 3 gestrichelt-grau (noch offen) */
    if (art == 1) gruen11(x, y, w, h, 16);
    else if (art == 2) gold11(x, y, w, h, 16, 1);
    else { flaeche f = F_KARTE11(); if (art == 3) { f.rand = S_GRAU3; f.rand_oben = f.rand_unten = .9f; } flaeche_zeichnen(x, y, w, h, 16, &f); }
    int ix = x + 24, tx = x + 24 + 58 + 20;
    if (bild && bild->px) { int bh = h - 16 > 90 ? 90 : h - 16, bw = bild->w * bh / bild->h; bild11_zeichnen(bild, ix + (58 - bw) / 2, y + (h - bh) / 2, bw, bh, 0); }
    else if (sym) symbol(sym, ix, y + (h - 58) / 2, 58, art == 1 ? S_GRUEN : art == 2 ? S_GOLD : S_GRAU2);
    int tb = x + w - 24 - tx, mitte = y + h / 2;
    text(tx, mitte - 4, 1, 30, art == 3 ? S_TEXT2 : S_TEXT_W, kuerzen(1, 30, titel, tb));
    text(tx, mitte + 26, 0, 24, art == 3 ? S_GRAU2 : S_GRAU, kuerzen(0, 24, unter, tb));
}
/* NEU 1.1 (da_Chris86 15.11.2026: "Jailbreak über YouTube"): der Einstieg in Worten und als Kürzel */
static const char *jb_ueber11(const char *n, const char **kurz)
{
    *kurz = n;
    if (!n) return "";
    if (strstr(n, "YouTube") || strstr(n, "Y2JB")) { *kurz = "Y2JB"; return T("über YouTube", "via YouTube"); }
    if (strstr(n, "Netflix")) { *kurz = "NFNH"; return T("über Netflix", "via Netflix"); }
    if (strstr(n, "Ren'Py") || strstr(n, "yarpe")) { *kurz = "YARPE"; return T("über ein Ren'Py-Spiel", "via a Ren'Py game"); }
    if (strstr(n, "PS2")) { *kurz = "mast1c0re / Luac0re"; return T("über einen PS2-Spielstand", "via a PS2 save"); }
    if (strstr(n, "Lua")) { *kurz = "Lua"; return T("über ein Lua-Spiel", "via a Lua game"); }
    if (strstr(n, "Autoloader")) { *kurz = "WebKit-Autoloader"; return T("über den Browser", "via the browser"); }
    if (strstr(n, "WebKit") || strstr(n, "Browser")) { *kurz = "WebKit"; return strstr(n, T("möglich", "possible")) ? T("über den Browser (möglich)", "via the browser (possible)") : T("über den Browser", "via the browser"); }
    return n;
}
/* erster erkannter Jailbreak-Weg ("WebKit im Browser (möglich)"), NULL = keiner bzw. noch nicht nachgeschaut */
static const char *kopf_jb_name(void)
{
    if (!GESUCHT) return NULL;   /* während einer Frage ruht der Kern - dann darf gelesen werden */
    const char *warn = NULL, *jb = kern_jailbreak_erkannt(K, fa, &warn);
    if (!jb || !jb[0]) return NULL;
    sliste nm = r_split(fa, jb, ", ");
    return nm.n ? nm.e[0] : jb;
}
/* Jailbreak in Worten (da_Chris86 15.11.2026 13:42: der erkannte Weg, z. B. "über YouTube") */
static void start_oben11(bool fertig)
{
    int y = R11_O, h = 300, bw = 960;
    stil11_laden();
    leuchten(R11_X, y, bw, h, 24, 40, S_TUERKIS, .14f);
    if (b11_banner_b.px) bild11_zeichnen(&b11_banner_b, R11_X, y, bw, h, 24); else karte11(R11_X, y, bw, h, 24);
    { flaeche r = { false, V1(0, 0), 0x1E4A73, 1, 1, 1, 0, 0 }; flaeche_zeichnen(R11_X, y, bw, h, 24, &r); }
    int cx = R11_X + bw + 24, cw = BREITE - R11_X - cx, chh = (h - 32) / 3;
    /* Konsole: steht sofort fest (liest das System) */
    const char *reihe = K->MODELL_TYP[0] ? kern_modell_reihe(K->MODELL_TYP, K->MODELL_DIGITAL) : T("Unbekanntes Modell", "Unknown model");
    bool lw = strcmp(K->MODELL_TYP, "fat") ? kern_kopplungs_stufe(K) == KOPPLUNG_BESTAETIGT : false;
    const char *titel = lw ? VERB(fa, reihe, T(" mit Laufwerk", " with disc drive")) : reihe;
    const char *nr = KI.modell[0] ? a_kopie_n(fa, KI.modell, strcspn(KI.modell, " ")) : T("Modell nicht erkannt", "Model not detected");
    chip11(cx, y, cw, chh, 0, SYM_KONSOLE, konsole_bild11(), titel, KI.firmware[0] ? VERB(fa, nr, " · Firmware ", KI.firmware) : nr);
    /* Laufwerks-Kopplung: liest PS5 Cleanup beim Start (nur lesen) */
    {
        kopplungsart ks = kern_kopplungs_stufe(K);
        if (ks == KOPPLUNG_BESTAETIGT) chip11(cx, y + chh + 16, cw, chh, 1, SYM_SCHILD_HAKEN, NULL, T("Laufwerk gekoppelt", "Disc drive paired"), T("bleibt immer geschützt", "always stays protected"));
        else {
            const char *t = kern_laufwerk_text(K), *dp = strstr(t, ": ");
            chip11(cx, y + chh + 16, cw, chh, ks == KOPPLUNG_KEINE ? 0 : 3, SYM_SCHEIBE, NULL, dp ? a_kopie_n(fa, t, (size_t)(dp - t)) : t, T("wird nie verändert", "never changed"));
        }
    }
    /* Jailbreak: erst nach dem Nachschauen bekannt */
    {
        int jy = y + 2 * (chh + 16);
        const char *warn = NULL, *jb = fertig && !G.beschaeftigt ? kern_jailbreak_erkannt(K, fa, &warn) : NULL;
        if (!fertig || G.beschaeftigt) chip11(cx, jy, cw, chh, 3, G.beschaeftigt ? SYM_LAEDT : SYM_JAILBREAK, NULL, "Jailbreak",   /* PS5-Test 15.11.2026: der stehende Dreh-Kreis sah aus wie ein halber Balken */ G.beschaeftigt ? T("wird erkannt …", "being detected …") : T("noch nicht geprüft", "not checked yet"));
        else if (!jb) chip11(cx, jy, cw, chh, 0, SYM_JAILBREAK, NULL, "Jailbreak", T("kein Einstieg gefunden", "no entry found"));
        else {
            sliste nm = r_split(fa, jb, ", ");
            const char *erst = nm.n ? nm.e[0] : jb;
            const char *kurz, *ueber = jb_ueber11(erst, &kurz);
            const char *unter = VERB(fa, kurz, " · ", nm.n > 1 ? a_fmt(fa, T("bleibt geschützt · %d weitere erkannt", "stays protected · %d more detected"), nm.n - 1) : T("bleibt geschützt", "stays protected"));
            chip11(cx, jy, cw, chh, 2, SYM_SCHILD_HAKEN, NULL, VERB(fa, "Jailbreak ", ueber), unter);
        }
    }
}

/* ------------------------------------------------------------------ Bereichskacheln (Startseite) */
static int kat_index11(int k) { switch (k) { case 1: return 0; case 6: return 1; case 2: return 2; case 3: return 3; case 4: return 4; case 5: return 5; default: return 6; } }
static void bereichskachel11(int x, int y, int w, int h, int k, const char *gr, const char *zahl)
{
    bool jb = k == 7;
    flaeche f = { true, V2(180, 0x243140, .55f, 0x18212D, .62f), jb ? 0x4A3B16 : 0xFFFFFF, jb ? 1 : .14f, jb ? 1 : .09f, 1, .08f, 0xFFFFFF };
    leuchten(x, y + 8, w, h, 16, 20, 0x000000, .3f);
    flaeche_zeichnen(x, y, w, h, 16, &f);
    text_passend(x + 22, y + 22 + 26, 2, 26, 22, jb ? S_GOLD : S_TEXT, kern_kat_name(k), w - 40);
    const bild11 *b = &b11_sym_g[kat_index11(k)];
    int bh = h - 22 - 34 - 12 - 40 - 6 - 30 - 22; if (bh > 110) bh = 110; if (bh < 50) bh = 50;
    int bw = 132 * bh / 110;
    if (b->px) bild11_zeichnen(b, x + (w - bw) / 2, y + 22 + 34 + (h - 22 - 34 - 82 - 22 - bh) / 2, bw, bh, 0);
    text_passend(x + 22, y + h - 22 - 30 - 6, 3, 40, 26, S_TEXT_W, gr, w - 44);   /* NEU 1.1 (PS5-Test 15.11.2026: "487,8 …"): erst kleiner, dann kürzen */
    text(x + 22, y + h - 22, 0, 24, S_GRAU, kuerzen(0, 24, zahl, w - 44));
}
/* große Wege-Kachel unten (✕ Selbst auswählen / △ Alles bereinigen) */
static void weg11(int x, int y, int w, int h, bool blau, knopfart k, const char *titel, const char *satz)
{
    if (blau) { leuchten(x, y, w, h, 24, 32, S_BLAU, .40f); flaeche f = F_WEG11(); flaeche_zeichnen(x, y, w, h, 24, &f); }
    else { flaeche f = F_WEGGOLD11(); flaeche_zeichnen(x, y, w, h, 24, &f); }
    int cx = x + 28 + 29, cy = y + h / 2;
    kreis(cx, cy, 29, blau ? 0x3A5A8C : 0x2A2410);
    ring(cx, cy, 28, 2, blau ? 0xF0F4FA : S_GOLD);
    const float *s = k == KN_X ? SYM_KN_KREUZ : k == KN_DREIECK ? SYM_KN_DREIECK : k == KN_QUADRAT ? SYM_KN_QUADRAT : SYM_KN_KREIS;
    symbol(s, cx - 13, cy - 13, 26, blau ? S_TEXT_W : S_GOLD);
    int tx = x + 28 + 58 + 22, tb = x + w - 28 - tx;
    passend p = einpassen(0, 26, 24, 22, satz, tb, 2);
    int th = 34 + 6 + p.z.n * p.lh, ty = y + (h - th) / 2;
    text(tx, ty + 30, 3, 32, S_TEXT_W, kuerzen(3, 32, titel, tb));
    for (int i = 0; i < p.z.n; i++) text(tx, ty + 34 + 6 + p.gr + i * p.lh, 0, p.gr, S_TEXT2, p.z.z[i]);
}

/* ------------------------------------------------------------------ Speicher der Laufwerke (alle 5 s neu gelesen) */
typedef struct { bool da[4]; long long frei[4], ges[4]; int usb_n; } speicher11;
static speicher11 speicher11_holen(void)
{
    static Uint32 zeit = 0; static speicher11 s;
    if (!zeit || SDL_GetTicks() - zeit > 5000) {
        for (int o = 0; o < 4; o++) s.da[o] = plattform_speicher(o, &s.frei[o], &s.ges[o]) && s.ges[o] > 0;
        s.usb_n = plattform_usb_anzahl(); zeit = SDL_GetTicks();
    }
    return s;
}
static const char *ort_name11(int o, int usb_n)
{
    switch (o) { case 0: return T("Interne SSD", "Internal SSD"); case 1: return T("M.2-SSD", "M.2 SSD"); case 2: return T("Erweiterter Speicher", "Extended storage");
    default: return usb_n > 1 ? a_fmt(fa, T("USB-Laufwerke (%d)", "USB drives (%d)"), usb_n) : T("USB-Laufwerk", "USB drive"); }
}

/* ------------------------------------------------------------------ Startseite (vor dem Nachschauen, beim Nachschauen, danach) */
static void start_vorher11(int y, int h)
{
    /* links: Nachschauen; rechts: So geht's */
    int lw = 620, rx = R11_X + lw + 24, rw = BREITE - R11_X - rx;
    karte11(R11_X, y, lw, h, 20);
    text(R11_X + 32, y + 32 + 40, 3, 40, S_TEXT_W, T("Bereit zum Nachschauen", "Ready to scan"));
    absatz_max(R11_X + 32, y + 32 + 40 + 44, 0, 26, S_TEXT2, T("PS5 Cleanup schaut nach, was auf deiner PS5 liegt. Dabei wird nur gelesen – nichts wird verändert.",
                                                              "PS5 Cleanup looks at what is on your PS5. It only reads – nothing is changed."), lw - 64, 36, 3);
    { int by = y + h - 32 - 76; leuchten(R11_X + 32, by, lw - 64, 76, 18, 20, S_BLAU, .45f); flaeche f = F_HAUPT11(); flaeche_zeichnen(R11_X + 32, by, lw - 64, 76, 18, &f);
      const char *t = T("Nachschauen", "Scan"); int tw = textbreite(3, 30, t), gx = R11_X + 32 + (lw - 64 - (42 + 14 + tw)) / 2;
      knopf11(KN_X, gx, by + 38, S_TEXT_W); text(gx + 56, gl11(by, 76, 30), 3, 30, S_TEXT_W, t); }
    karte11(rx, y, rw, h, 20);
    text(rx + 32, y + 32 + 24, 1, 24, S_GRAU2, T("SO GEHT ES", "HOW IT WORKS"));
    const char *st[3][2] = { { T("Nachschauen", "Scan"), T("Nur lesen, nichts wird verändert.", "Read only, nothing is changed.") },
                             { T("Auswählen", "Select"), T("Du entscheidest, was weg soll – nichts ist vorher angehakt.", "You decide what should go – nothing is ticked beforehand.") },
                             { T("Löschen", "Delete"), T("Erst nach deiner Bestätigung.", "Only after you confirm.") } };
    int sy = y + 32 + 24 + 24;
    for (int i = 0; i < 3; i++) {
        char z[4]; snprintf(z, sizeof z, "%d", i + 1);
        if (i == 0) { ring(rx + 32 + 22, sy + 30, 20.5f, 2.5f, S_TUERKIS); text_mitte(rx + 54, gl11(sy + 8, 44, 24), 1, 24, S_TUERKIS, z); }
        else { ring(rx + 54, sy + 30, 21, 1.6f, S_GRAU3); text_mitte(rx + 54, gl11(sy + 8, 44, 24), 0, 24, S_GRAU, z); }
        text(rx + 32 + 44 + 20, sy + 26, 1, 28, S_TEXT_W, st[i][0]);
        text(rx + 32 + 44 + 20, sy + 26 + 32, 0, 24, S_GRAU, kuerzen(0, 24, st[i][1], rw - 64 - 64));
        sy += 76;
    }
    /* NEU 1.0.10 (da_Chris86 13.11.2026): vor dem Nachschauen die Programme schließen, die weg sollen */
    int gy = y + h - 32 - 72;
    gold11(rx + 32, gy, rw - 64, 72, 14, 1);
    symbol(SYM_WARNUNG, rx + 32 + 18, gy + 22, 28, S_GOLD);
    text(rx + 32 + 60, gy + 30, 1, 24, S_GOLD_T, kuerzen(1, 24, T("Vorher alle Programme schließen, die weg sollen.", "First close all programs you want to remove."), rw - 64 - 80));
    text(rx + 32 + 60, gy + 58, 0, 22, S_TEXT2, kuerzen(0, 22, T("Am sichersten: PS5 neu starten und nur Jailbreak und PS5 Cleanup laden.", "Safest: restart the PS5 and load only the jailbreak and PS5 Cleanup."), rw - 64 - 80));
}
static Uint32 suche_seit11;   /* wann das Nachschauen begann (für "noch etwa …") */
/* NEU 1.1 (da_Chris86 16.11.2026: "Das soll schon der Echtzeit entsprechen"): Prozent beim Nachschauen. Die Sicherheitsprüfung (Schritt 0)
 * meldet ihren Stand je Ordner (kern_selbstschutz.c) - wie lange ein Ordner dauert, weiß sie vorher nicht. Ab dem zweiten Nachschauen
 * kennt PS5 Cleanup die Dauer der letzten Prüfung und rechnet nach der Zeit (höchstens 74 %, bis sie fertig ist). Die Restzeit steht erst da, wenn sie sich ehrlich schätzen lässt. Nur der Fernseher (Hauptfaden) misst; der Browser liest
 * suche_pz11_jetzt. Nur Anzeige. */
static Uint32 pruef_ms_letzt, pruef_lauf, pruef_beginn; static bool pruef_fertig; static int suche_pz11_jetzt; static bool suche_rest11_ok;
static int suche_pz11(void)
{
    int schritt = K->SUCHE_SCHRITT; if (schritt < 0) schritt = 0; if (schritt > KERN_SUCHE_SCHRITTE) schritt = KERN_SUCHE_SCHRITTE;
    int pz = kern_suche_prozent(schritt, K->SUCHE_TEIL);
    Uint32 jetzt = SDL_GetTicks();
    if (pruef_lauf != arbeit_seit) { pruef_lauf = arbeit_seit; pruef_beginn = 0; pruef_fertig = false; }
    if (schritt == 0) {
        if (!pruef_beginn || pruef_fertig) { pruef_beginn = jetzt; pruef_fertig = false; }   /* auch: Nachschauen gleich nach dem Löschen */
        if (pruef_ms_letzt > 0) { int pt = (int)(75.0 * (double)(jetzt - pruef_beginn) / pruef_ms_letzt); if (pt > 74) pt = 74; pz = pt; }   /* Messung 16.11.2026: nur nach der Zeit gleichmäßig - die Ordner-Anteile laufen anfangs vor */
        suche_rest11_ok = pruef_ms_letzt > 0;
    } else {
        if (pruef_beginn && !pruef_fertig) { pruef_ms_letzt = jetzt - pruef_beginn; pruef_fertig = true; }
        suche_rest11_ok = true;
    }
    suche_pz11_jetzt = pz;
    return pz;
}
static void start_suche11(int y, int h)
{
    /* NEU 1.1 (Entwurf "0 – Nachschauen"): links der Ring mit Prozent und Restzeit, rechts die Laufwerke mit Stand, ein wechselnder
     * Hinweis und "Bisher gefunden". Prozent und Stand kommen aus den Schritten der Suche (K->SUCHE_SCHRITT, nur Anzeige). */
    bool ab = K->abbruch;
    int schritt = K->SUCHE_SCHRITT, ort = K->SUCHE_ORT;
    if (schritt < 0) schritt = 0; if (schritt > KERN_SUCHE_SCHRITTE) schritt = KERN_SUCHE_SCHRITTE;
    if (!suche_seit11 || schritt == 0) { if (!suche_seit11) suche_seit11 = SDL_GetTicks(); }
    int pz = suche_pz11();   /* NEU 1.1 (16.11.2026): nach dem echten Stand (siehe oben) */
    int lw = 560, rx = R11_X + lw + 24, rw = BREITE - R11_X - rx;
    karte11(R11_X, y, lw, h, 20);
    {
        zeilen z = umbrechen(1, 30, ab ? T("Wird abgebrochen …", "Cancelling …") : T("Ich schaue nach, was auf deiner PS5 liegt", "Looking at what is on your PS5"), lw - 64);
        for (int i = 0; i < z.n && i < 2; i++) text_mitte(R11_X + lw / 2, y + 32 + 34 + i * 38, 1, 30, S_TEXT_W, z.z[i]);
    }
    float cx = R11_X + lw / 2.f, cy = y + 32 + 76 + 30 + 135;
    leuchten((int)cx - 135, (int)cy - 135, 270, 270, 135, 40, S_TUERKIS, .2f);
    kreis(cx, cy, 135, 0x2A3644);
    if (pz > 0) ring_teil(cx, cy, 126, 18, pz / 100.f, S_TUERKIS);
    kreis(cx, cy, 117, 0x161E28);
    text_mitte((int)cx, (int)cy + 20, 3, 72, S_TEXT_W, a_fmt(fa, "%d %%", pz));
    {
        Uint32 ms = SDL_GetTicks() - arbeit_seit; const char *rz = T("wird gezählt …", "estimating …");   /* 1.1: seit diesem Nachschauen (suche_seit11 galt nur für das erste) */
        if (suche_rest11_ok && pz >= 15 && ms > 1500) { long rest = (long)((double)ms * (100 - pz) / pz / 1000.0) + 1; rz = rest >= 60 ? a_fmt(fa, T("noch etwa %ld Min.", "about %ld min left"), (rest + 30) / 60) : a_fmt(fa, T("noch etwa %ld Sek.", "about %ld s left"), rest); }
        text_mitte((int)cx, (int)cy + 62, 0, 24, S_GRAU, rz);
    }
    lauf_aktiv = true; lauf_naechst = SDL_GetTicks() + 200;
    {
        int gh = 96, gy = y + h - 32 - gh;
        gruen11(R11_X + 32, gy, lw - 64, gh, 14);
        symbol(SYM_SCHILD_HAKEN, R11_X + 32 + 20, gy + gh / 2 - 14, 28, S_GRUEN);
        zeilen z = umbrechen(0, 26, T("Es wird nur nachgeschaut – nichts wird verändert.", "Only looking – nothing is changed."), lw - 64 - 20 - 28 - 16 - 16);
        for (int i = 0; i < z.n && i < 2; i++) text(R11_X + 32 + 20 + 28 + 16, gy + gh / 2 - (z.n - 1) * 17 + 9 + i * 34, 0, 26, S_TEXT, z.z[i]);
    }
    /* rechts: Laufwerke */
    karte11(rx, y, rw, h, 20);
    text_gesperrt(rx + 28, y + 28 + 24, 1, 24, S_GRAU2, T("LAUFWERKE", "DRIVES"), .14f);
    speicher11 sp = speicher11_holen();
    int ly = y + 28 + 24 + 20;
    for (int o = 0; o < 4; o++) if (sp.da[o]) {
        /* Stand je Speicher aus den Schritten: intern bis zum Ende, M.2/erweitert in Schritt 7, USB-Sticks in Schritt 8 */
        int zu;   /* 0 wartet, 1 läuft, 2 fertig */
        float an;
        /* Codex-Prüfung 347 (P3): jedes Laufwerk "läuft" vom Anfang (die Sicherheitsprüfung liest alle) bis zu seinem Schritt; der Balken folgt
         * dem Gesamtstand und läuft nie rückwärts (vorher Schritt 0 mit Gesamtstand, danach "wartet" mit 0) */
        if (o == ORT_INTERN) { zu = schritt >= KERN_SUCHE_SCHRITTE - 1 ? 2 : 1; an = pz / 100.f; }
        else if (o == ORT_STICK) { zu = schritt > 8 ? 2 : 1; an = zu == 2 ? 1.f : pz / 100.f; }
        else { zu = schritt > 7 ? 2 : 1; an = zu == 2 ? 1.f : pz / 100.f; }
        if (ort == o && zu == 0) zu = 1;
        if (an > 1.f) an = 1.f;
        int rh = 82;
        if (zu == 1) fokus11(rx + 28, ly, rw - 56, rh, 14, 2);
        else { flaeche f = { false, V1(0xFFFFFF, .045f), 0, 0, 0, 0, 0, 0 }; flaeche_zeichnen(rx + 28, ly, rw - 56, rh, 14, &f); }
        const char *nm = ort_name11(o, sp.usb_n);
        text(rx + 28 + 20, ly + 40, 1, 28, S_TEXT_W, nm);
        text(rx + 28 + 20 + textbreite(1, 28, nm) + 16, ly + 40, 0, 24, S_GRAU, kern_groesse_text(fa, sp.ges[o]));
        const char *st = zu == 2 ? T("fertig", "done") : zu == 1 ? T("läuft …", "running …") : T("wartet", "waiting");
        text_rechts(rx + rw - 28 - 20, ly + 40, 1, 26, zu == 2 ? S_GRUEN : zu == 1 ? S_TUERKIS : S_GRAU, st);
        int bx = rx + 28 + 20, bw = rw - 56 - 40;
        { flaeche sb = { false, V1(0xFFFFFF, .08f), 0, 0, 0, 0, 0, 0 }; flaeche_zeichnen(bx, ly + 58, bw, 10, 5, &sb); }
        if (an > 0) rund(bx, ly + 58, (int)(bw * an) < 10 ? 10 : (int)(bw * an), 10, 5, S_TUERKIS);
        ly += rh + 8;
    }
    wusstest11(rx + 28, ly + 6, rw - 56, 0);
    {
        int n = K->bau.n > 0 ? K->bau.n : 0, by = y + h - 36;
        /* NEU 1.1 (16.11.2026): während der Sicherheitsprüfung steht in K->bau noch die Liste vom letzten Nachschauen - keine alte Zahl zeigen */
        if (schritt == 0) text(rx + 28, by, 0, 28, S_TEXT2, T("Zuerst die Sicherheitsprüfung: was geschützt bleiben muss …", "First the safety check: what must stay protected …"));
        else {
        const char *t1 = T("Bisher gefunden:", "Found so far:"), *t2 = a_fmt(fa, "%d", n), *t3 = EINTRAEGE(n);
        int x = rx + 28; text(x, by, 0, 28, S_TEXT2, t1); x += textbreite(0, 28, t1) + 16;
        text(x, by + 4, 3, 40, S_TEXT_W, t2); x += textbreite(3, 40, t2); text(x, by, 3, 28, S_TEXT_W, t3);
        }
    }
}
static void start_nachher11(int y, int h)
{
    karte11(R11_X, y, BREITE - 2 * R11_X, h, 20);
    long long ges[8] = { 0 }; int anz[8] = { 0 }; bool alle_da = true, da_k[8] = { 0 }; long long summe = 0;
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        int k = ALLE_KAT[ki]; kblock *b = NULL; int nb = kern_bloecke(K, fa, k, SORT_AZ, &b);
        anz[k] = kern_anzahl(K, k); da_k[k] = true;
        for (int i = 0; i < nb; i++) { if (!b[i].groesse_da) alle_da = da_k[k] = false; else ges[k] += b[i].bytes; }
        summe += ges[k];
    }
    bool zl = zaehlen_laeuft();
    int tx = R11_X + 32, ty = y + 28;
    text(tx, ty + 30, 1, 30, S_TEXT_W, T("Das liegt auf deiner PS5", "This is on your PS5"));
    speicher11 sp = speicher11_holen(); const char *orte = NULL;
    for (int o = 0; o < 4; o++) if (sp.da[o]) orte = orte ? VERB(fa, orte, o == 3 ? T(" und ", " and ") : ", ", ort_name11(o, sp.usb_n)) : ort_name11(o, sp.usb_n);
    const char *zusatz = VERB(fa, orte ? orte : "", T(" · nichts wurde verändert", " · nothing was changed"));
    text(tx + textbreite(1, 30, T("Das liegt auf deiner PS5", "This is on your PS5")) + 20, ty + 30, 0, 24, S_GRAU, kuerzen(0, 24, zusatz, 900));
    const char *gross = VERB(fa, !alle_da && !zl ? T("mind. ", "min. ") : "", kern_groesse_text(fa, summe));
    text_rechts(BREITE - R11_X - 32, ty + 36, 3, 52, S_TEXT_W, gross);
    if (!alle_da && zl) text_rechts(BREITE - R11_X - 32 - textbreite(3, 52, gross) - 16, ty + 36, 0, 24, S_GRAU, T("wird noch gezählt …", "still counting …"));
    int ky = ty + 60 + 16, kh = y + h - 24 - ky, gap = 16, kw = (BREITE - 2 * R11_X - 64 - 6 * gap) / 7;
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        int k = ALLE_KAT[ki];
        const char *gt = !anz[k] ? "–" : (da_k[k] || !zl) ? kern_groesse_text(fa, ges[k]) : T("zählt …", "counting …");
        const char *zahl = !anz[k] ? T("keine", "none") : a_fmt(fa, anz[k] == 1 ? T("%d Eintrag", "%d entry") : T("%d Einträge", "%d entries"), anz[k]);
        bereichskachel11(tx + ki * (kw + gap), ky, kw, kh, k, gt, zahl);
    }
}
static void schirm_start11(void)
{
    hintergrund11();
    bool fertig = GESUCHT && ablauf_verbunden(&A), suche = G.beschaeftigt;
    start_oben11(fertig);
    int y = R11_O + 300 + 24;
    if (!fertig || suche) {
        int h = FUSS11_Y - 24 - y;
        if (suche) start_suche11(y, h); else start_vorher11(y, h);
        fussknopf kn[4]; int n = 0;
        if (suche) { if (A.abbrechbar && !K->abbruch) kn[n++] = (fussknopf){ KN_O, T("Abbrechen", "Cancel"), false }; }   /* 1.1 (Entwurf): nur Abbrechen - ≡ zeigt weiter das Protokoll */
        else {
            suche_seit11 = 0;
            kn[n++] = (fussknopf){ KN_X, T("Nachschauen", "Scan"), false };
            kn[n++] = (fussknopf){ KN_QUADRAT, SP_EN ? "Deutsch" : "English", false };
            kn[n++] = (fussknopf){ KN_OPT, T("Optionen", "Options"), false };
            kn[n++] = (fussknopf){ KN_O, T("halten: Beenden", "hold: Quit"), false };
        }
        fuss11(kn, n, NULL);
    } else {
        int wh = 150, wy = FUSS11_Y - 24 - wh, h = wy - 24 - y;
        start_nachher11(y, h);
        int ww = (BREITE - 2 * R11_X - 24) / 2;
        weg11(R11_X, wy, ww, wh, true, KN_X, T("Selbst auswählen", "Choose yourself"), T("Du hakst an, was weg soll. Nichts ist vorher angehakt.", "You tick what should go. Nothing is ticked beforehand."));
        weg11(R11_X + ww + 24, wy, ww, wh, false, KN_DREIECK, T("Alles bereinigen", "Clean up everything"), T("Alles auf einmal – wie frisch eingerichtet, ohne Reset. Geschütztes bleibt.", "Everything at once – like freshly set up, without a reset. Protected data stays."));
        /* NEU 1.1 (da_Chris86 16.11.2026 17:58): ✕ und △ stehen schon als große Knöpfe darüber - die Leiste bietet, was sonst nur in den
         * Optionen steht: □ Sprache (schaut danach neu nach), R1 Nochmal nachschauen. Den Testmodus bewusst nicht auf eine Taste (ein
         * versehentlicher Druck schaltete den Schutz aus) */
        fussknopf kn[4] = { { KN_QUADRAT, SP_EN ? "Deutsch" : "English", false }, { KN_R1, T("Nochmal nachschauen", "Scan again"), false },
                            { KN_O, T("halten: Beenden", "hold: Quit"), false }, { KN_OPT, T("Optionen", "Options"), false } };
        char a[64]; a[0] = 0; if (fern_an()) fern_adresse(a, sizeof a);
        fuss11(kn, 4, a[0] ? a_kopie(fa, strncmp(a, "http://", 7) ? a : a + 7) : NULL);
    }
    if (optionen_offen) optionen11();
}

/* ------------------------------------------------------------------ Kopf und Fußleiste der Seiten von 1.0.8 im neuen Stil */
static const char *seite11_titel = NULL;   /* setzt die Seite vor kopf_neu (sonst nach dem Schritt) */
static void kopf_neu11(int schritt, const char *satz)
{
    (void)satz;
    const char *t = seite11_titel ? seite11_titel : schritt <= 1 ? T("Nachschauen", "Scan") : schritt == 2 ? T("Was soll weg?", "What should go?") : T("Noch einmal prüfen", "Check once more");
    kopf11(t, schritt);
    seite11_titel = NULL;
}
static void fuss_neu11(const fussknopf *kn, int n) { fuss11(kn, n, NULL); }
