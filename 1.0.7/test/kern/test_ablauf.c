/* PS5 Cleanup 1.1 - Vergleich F: Bedienabläufe (ablauf.c) gegen die Oberfläche von 1.0.5
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 *   test_ablauf <szenario-datei> <hier>     (Testbaum-Kopie liegt in <hier>/w)
 *
 * Spielt dasselbe Szenario wie test/ref105/AblaufRef.cs ab (Häkchen, Ablauf, Antworten)
 * und schreibt dasselbe Protokoll-Format: jede Log-Zeile, jede Frage (Titel, Text, Knöpfe,
 * Antwort), die Fertig-Meldung und mit ZEIGE die Anzeige (Bereichsköpfe, Einträge, Statuszeile).
 * Umgebung: wie test_schritt5 (FAKESM, RMDA, ...); SIM=1 -> Simulation (K->simulation).
 */
#define _GNU_SOURCE
#include "../../kern/ablauf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *HIER;
static sliste ANTW; static int antw_i = 0;
static arena *TA;
static int fehler = 0;

static void Z(const char *t) { printf("%s\n", t); fflush(stdout); }
static void mehrzeilig(const char *t)
{
    const char *s = t ? t : "";
    for (;;) { const char *e = strchr(s, '\n'); if (!e) { printf("  | %s\n", s); break; } printf("  | %.*s\n", (int)(e - s), s); s = e + 1; }
}
static const char *naechste(void) { return antw_i < ANTW.n ? ANTW.e[antw_i++] : "N"; }

/* ---- Rückrufe wie die Dialoge in 1.0.5 */
static bool u_wahl(void *c, const char *text, const char *titel, const char *sicher, const char *gef)
{
    (void)c;
    printf("DIALOG %s\n", titel); mehrzeilig(text);
    const char *a = naechste(); bool j = !strcmp(a, "J");
    printf("  KNOPF sicher: %s\n  KNOPF gefährlich: %s\n  ANTWORT %s\n", sicher, gef, j ? "J" : "N");
    return j;
}
static const char *u_text(void *c, const char *text, const char *titel)
{
    (void)c;
    printf("DIALOG %s\n", titel); mehrzeilig(text);
    const char *a = naechste();
    printf("  EINGABE\n  ANTWORT %s\n", a);
    if (!strcmp(a, "ABBRECHEN")) return NULL;
    return h_trim(TA, a);                                     /* te.Text.Trim() */
}
static void u_log(void *c, const char *t) { (void)c; Z(t); }
static void u_stand(void *c, const char *t, double p) { (void)c; (void)t; (void)p; }
static void u_fertig(void *c, const char *kopf, bool ok, const char *frei, const char *sm)
{
    (void)c; (void)ok;
    printf("FERTIG %s\n  frei: %s\n", kopf, frei);
    if (sm && sm[0]) { printf("  sm:\n"); mehrzeilig(sm); }
    const char **z; int n = kern_schluss_text(&z);
    const char *schritt = "", *wichtig = "";
    sliste nr = {0};
    for (int i = 0; i < n; i++) {
        const char *s = z[i]; int k = 0;
        while (s[k] >= '0' && s[k] <= '9') k++;
        if (k > 0 && s[k] == '.') { const char *r = s + k + 1; while (*r == ' ') r++; sl_add(TA, &nr, VERB(TA, a_kopie_n(TA, s, (size_t)k + 1), " ", r)); }
        else if (i == n - 1) wichtig = s;
        else schritt = s;
    }
    printf("  schritt: %s\n", schritt);
    for (int i = 0; i < nr.n; i++) printf("  %s\n", nr.e[i]);
    printf("  wichtig: %s\n", wichtig);
}
static void u_fehler(void *c, const char *t) { (void)c; printf("FEHLERMELDUNG\n"); mehrzeilig(t); }

/* ---- Anzeige */
static datei_optionen OPT;
static zugriff *frische_simulation(void *c) { (void)c; return zugriff_sim(zugriff_datei(&OPT)); }   /* NEU 1.1: nach der Simulation wieder echter Stand */
static bool nie(void *c) { (void)c; return false; }
static void nichts(void *c, eintrag *e) { (void)c; (void)e; }
static void zeige_echt(ablauf *a)
{
    kern *K = a->K;
    ablauf_hintergrund_anhalten(a);
    kern_groessen_im_hintergrund(K, nie, NULL, nichts, NULL);   /* = Hintergrund-Zählung bis zum Ende */
    ablauf_texte_frei(a);
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        int k = ALLE_KAT[ki], n = 0;
        for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Kat == k) n++;
        if (n == 0) continue;   /* 1.0.6: leere Bereiche werden ausgeblendet (PS5-App sicht_neu, Windows Fenster.cs) */
        printf("KOPF %s\n", ablauf_bereich_kopf(a, k));
        for (int i = 0; i < K->Funde.n; i++) {
            eintrag *f = K->Funde.e[i];
            if (f->Kat != k) continue;
            const char *grund; bool gesp = ablauf_eintrag_gesperrt(a, f, &grund);
            printf("  %s%s%s%s%s%s\n", f->Wahl ? "[x] " : "[ ] ", f->Text, ablauf_eintrag_groesse(a, f), gesp ? "  [gesperrt]" : "",
                   grund ? VERB(TA, "  {", grund, "}") : "", f->Paket ? "  (gelb)" : "");
        }
    }
    printf("STAND %s\n", ablauf_stand_text(a));
}

static eintrag *finde_text(kern *K, const char *alternativen)
{
    sliste alt = r_split(K->a, alternativen, "\\|");
    for (int ki = 0; ki < KAT_ANZ; ki++)
        for (int i = 0; i < K->Funde.n; i++) {
            eintrag *f = K->Funde.e[i];
            if (f->Kat != ALLE_KAT[ki]) continue;
            for (int j = 0; j < alt.n; j++) if (txt_beginnt(f->Text, alt.e[j])) return f;
        }
    return NULL;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: <szenario> <hier>\n"); return 2; }
    HIER = argv[2];
    TA = arena_neu();
    /* Szenario lesen */
    FILE *fs = fopen(argv[1], "r"); if (!fs) { perror(argv[1]); return 2; }
    sliste zeilen = {0}; char b[4096];
    while (fgets(b, sizeof b, fs)) { size_t l = strlen(b); while (l && (b[l - 1] == '\n' || b[l - 1] == '\r' || b[l - 1] == ' ')) b[--l] = 0; if (l && b[0] != '#') sl_add(TA, &zeilen, a_kopie(TA, b)); }
    fclose(fs);
    const char *modell = "";
    for (int i = 0; i < zeilen.n; i++) {
        const char *z = zeilen.e[i];
        if (txt_beginnt(z, "SPRACHE ")) SP_EN = !strcmp(z + 8, "en");
        if (txt_beginnt(z, "MODELL ")) modell = z + 7;
    }
    for (int i = 0; i < zeilen.n; i++) if (txt_beginnt(zeilen.e[i], "ANTWORT ")) {
        const char *a = zeilen.e[i] + 8;
        if (!strcmp(a, "BESTAETIGUNG")) a = SP_EN ? "DELETE" : "LÖSCHEN";
        sl_add(TA, &ANTW, a);
    }

    datei_optionen o = { .wurzel = VERB(TA, HIER, "/w"), .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = HIER };
    OPT = o;
    zugriff *z = zugriff_datei(&o);
    bool sim = getenv("SIM") && !strcmp(getenv("SIM"), "1");
    kern *K = kern_neu(sim ? zugriff_sim(z) : z);
    K->simulation = sim;
    K->MODELL_TYP = kern_modell_typ(K->a, modell); K->MODELL_DIGITAL = kern_modell_digital(K->a, modell);
    K->DDD_BEKANNT = false;
    ablauf_ui ui = { u_wahl, u_text, u_log, u_stand, u_fertig, u_fehler, NULL, NULL };
    ablauf A; ablauf_init(&A, K, &ui);
    K->melde = (void (*)(void *, const char *))u_log; K->melde_ctx = NULL;
    A.protokoll_datei = "(Protokoll)";
    A.neue_verbindung = sim ? frische_simulation : NULL;

    /* "Verbinden": Suche */
    VERSUCHE { kern_suche_funde(K); } FANGE { printf("NICHT VERBUNDEN %s\n", ausn_text); return 1; } ENDE_VERSUCHE;
    Z("=== ABLAUF");
    for (int i = 0; i < zeilen.n; i++) {
        const char *zl = zeilen.e[i];
        if (!strcmp(zl, "ZEIGE")) { Z("> ZEIGE"); zeige_echt(&A); }
        else if (txt_beginnt(zl, "WAEHLE ") || txt_beginnt(zl, "ABWAEHLEN ")) {
            printf("> %s\n", zl);
            bool an = txt_beginnt(zl, "WAEHLE ");
            const char *rest = strchr(zl, ' ') + 1;
            if (txt_beginnt(rest, "BEREICH ")) ablauf_bereich_setzen(&A, atoi(rest + 8), an);
            else {
                eintrag *f = finde_text(K, rest + 5);
                if (!f) { printf("FEHLT %s\n", rest + 5); fehler++; continue; }
                const char *g; if (ablauf_eintrag_gesperrt(&A, f, &g)) { printf("NICHT WÄHLBAR %s\n", rest + 5); continue; }
                ablauf_eintrag_setzen(&A, f, an);
            }
        }
        else if (txt_beginnt(zl, "ABLAUF ") || txt_beginnt(zl, "ABLAUF_UNGEZAEHLT ")) {
            printf("> %s\n", zl);
            /* NEU 1.0.8 (Arbeitsplan 5.1a): ABLAUF_UNGEZAEHLT startet, bevor die Größen im Hintergrund gezählt sind - wie ein schneller
             * Druck auf Löschen gleich nach dem Nachschauen (die Frage "Außerhalb der internen SSD" kennt dann keine Größen) */
            bool ungezaehlt = txt_beginnt(zl, "ABLAUF_UNGEZAEHLT ");
            const char *was = strchr(zl, ' ') + 1;
            ablauf_hintergrund_anhalten(&A);
            if (!ungezaehlt) kern_groessen_im_hintergrund(K, nie, NULL, nichts, NULL);   /* wie 1.0.5: warten, bis die Hintergrund-Zählung fertig ist */
            if (!strcmp(was, "KOMPLETT")) ablauf_starte(&A, (void (*)(void *))ablauf_komplett, &A);
            else ablauf_starte(&A, (void (*)(void *))ablauf_auswahl, &A);
            ablauf_hintergrund_anhalten(&A);
            printf("> ABLAUF ENDE verbunden=%s\n", ablauf_verbunden(&A) ? "True" : "False");
        }
    }
    if (antw_i < ANTW.n) { printf("UNBENUTZTE ANTWORTEN %d\n", ANTW.n - antw_i); fehler++; }
    printf("ENDE fehler=%d\n", fehler);
    return 0;
}
