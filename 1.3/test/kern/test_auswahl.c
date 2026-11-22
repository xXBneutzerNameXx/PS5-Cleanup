/* PS5 Cleanup - Test der Löschliste je Auswahlweg (NEU 1.3, da_Chris86 20.11.2026 17:50 "Den Test machen wir später", 21.11.2026 09:29)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_auswahl <hier>   Testbaum <hier>/w (root9 + zuordnung_baum.py, aus lauf_auswahl.sh)
 * Frage: wird bei jedem Weg zu wählen genau das gelöscht, was gewählt ist - und nichts anderes?
 *   1. jeder Eintrag einzeln (Haken auf der Zeile)
 *   2. jeder Block mit dem Haken auf dem Kopf (kern/ablauf.c ablauf_block_setzen - dieselbe Regel wie Fernseher und Browser)
 *   3. jeder Bereich ("ganzen Bereich wählen", ablauf_bereich_setzen)
 *   4. "Alles bereinigen" (kern_waehle_komplett) ohne und mit M.2/USB
 * Je Weg ein frischer Kern auf einer Simulation (zugriff_sim): gelöscht wird nur "auf dem Papier", das Buch der Simulation sagt, was
 * gelöscht worden wäre. Geprüft wird:
 *   - jeder gelöschte Pfad (DELE, RMDA) liegt in einem gewählten Eintrag - oder trägt die Kennung einer gewählten Kachel (deren Reste,
 *     z. B. Web-Zwischenspeicher oder leere Aufnahme-Zwischenordner, gehen bewusst mit)
 *   - RMD (nur leere Ordner) zusätzlich auch für Ordner über einem gewählten Pfad (leer gewordene Eltern)
 *   - der Block-Haken und "ganzen Bereich wählen" haken nie an, was nur einzeln gehen soll (Erkanntes, Sicherungen von Sonys Datenbank,
 *     selbst Hochgeladenes, Playden-Spiele, endgültig Weggehendes)
 *   - gewählt ist danach auch, was der Löschlauf mitnimmt (kern_abhaengige_mitnehmen) - die Liste vor dem Löschen zeigt es */
#include "../../kern/kern.h"
#include "../../kern/ablauf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fehler = 0, laeufe = 0, buchungen = 0;
static void melde_log(void *c, const char *t) { (void)c; (void)t; }
static bool nein(void *c) { (void)c; return false; }
static const char *W; static arena *A0;
static zugriff *Z;
static bool GEGENPROBE;   /* lauf_auswahl.sh: zweiter Lauf mit GEGENPROBE=1 muss Fehler finden (prüft den Test selbst) */

static kern *neu(void)
{
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    Z = zugriff_sim(zugriff_datei(&o));
    kern *k = kern_neu(Z); k->melde = melde_log; k->simulation = true;
    volatile int fe = 0;
    VERSUCHE { kern_selbstschutz_eigener_pfad(k, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(k); kern_suche_funde(k); }
    FANGE { printf("FAIL Suche abgebrochen: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) { fehler++; return NULL; }
    for (int i = 0; i < k->Funde.n; i++) ((eintrag *)k->Funde.e[i])->Wahl = false;
    return k;
}
static bool unter(const char *p, const char *b)
{
    size_t l = strlen(b);
    if (l > 1 && b[l - 1] == '/') l--;
    return !strncmp(p, b, l) && (p[l] == 0 || p[l] == '/');
}
static bool kennung_in(const char *p, const char *id)
{
    const char *s = p; size_t l = strlen(id);
    while ((s = strstr(s, id)) != NULL) {
        bool vorn = s == p || !((s[-1] >= 'A' && s[-1] <= 'Z') || (s[-1] >= '0' && s[-1] <= '9'));
        char h = s[l];
        if (vorn && !((h >= 'A' && h <= 'Z') || (h >= '0' && h <= '9'))) return true;
        s += l;
    }
    return false;
}
/* nach der Auswahl: mitnehmen, auf dem Papier löschen, Buch gegen das Gewählte prüfen */
static void loeschen_pruefen(kern *k, const char *was)
{
    kern_abhaengige_mitnehmen(k);
    sliste gp = {0}, ids = {0};
    int gew = 0;
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i]; if (!f->Wahl) continue;
        gew++;
        if (GEGENPROBE && gew == 1) continue;   /* Gegenprobe: der erste gewählte Eintrag "fehlt" - dann muss der Test anschlagen */
        for (int j = 0; j < f->Pfade.n; j++) sl_add(A0, &gp, f->Pfade.e[j]);
        for (int j = 0; j < f->PfadeBasis.n; j++) sl_add(A0, &gp, f->PfadeBasis.e[j]);
        if (f->KachelId && strlen(f->KachelId) == 9) sl_add(A0, &ids, f->KachelId);
    }
    if (gew == 0) return;
    laeufe++;
    volatile int fe = 0;
    VERSUCHE { kern_fuehre_loeschung(k, nein, NULL); } FANGE { printf("FAIL %s: Löschlauf abgebrochen: %s\n", was, ausn_text); fehler++; fe = 1; } ENDE_VERSUCHE;
    if (fe) return;
    const sliste *b = zugriff_sim_buch(Z);
    for (int i = 0; i < b->n; i++) {
        const char *z = b->e[i], *p = NULL; int art = 0;   /* 1 DELE/RMDA, 2 RMD, 3 CHMOD, 4 STOR */
        if (!strncmp(z, "DELE ", 5)) { p = z + 5; art = 1; }
        else if (!strncmp(z, "RMDA ", 5)) { p = z + 5; art = 1; }
        else if (!strncmp(z, "RMD ", 4)) { p = z + 4; art = 2; }
        else if (!strncmp(z, "SITE CHMOD ", 11)) { p = strchr(z + 11, ' '); p = p ? p + 1 : z; art = 3; }
        else if (!strncmp(z, "STOR ", 5)) { p = z + 5; art = 4; }
        else continue;
        buchungen++;
        bool ok = false;
        for (int j = 0; j < gp.n && !ok; j++) if (unter(p, gp.e[j]) || (art >= 2 && unter(gp.e[j], p))) ok = true;
        for (int j = 0; j < ids.n && !ok; j++) if (kennung_in(p, ids.e[j])) ok = true;
        if (!ok && art == 4) { printf("INFO %s: geschrieben %s\n", was, p); continue; }   /* z. B. Playdens Liste: nur melden */
        if (!ok) printf("FAIL %s: %s liegt in keinem gewählten Eintrag\n", was, z), fehler++;
    }
}
static void gesperrte_regel(kern *k, const char *was, bool (*nur_einzeln)(kern *, const eintrag *), const pliste *nur_diese, const eintrag *ausser)
{
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i]; if (!f->Wahl || f == ausser) continue;
        if (nur_diese) { bool drin = false; for (int j = 0; j < nur_diese->n; j++) if (nur_diese->e[j] == f) drin = true; if (!drin) continue; }
        if (nur_einzeln(k, f)) printf("FAIL %s: \"%s\" ist mit angehakt, geht aber nur einzeln\n", was, f->Text), fehler++;
    }
}
static bool block_nur_einzeln(kern *k, const eintrag *f) { (void)k; return ablauf_block_nur_einzeln(f); }
static bool bereich_nur_einzeln(kern *k, const eintrag *f) { (void)k; return f->Erkannt || kern_db_sicherung(f) || f->Rot || f->Paket || f->NurEinzeln; }
/* NEU 1.3 (da_Chris86 22.11.2026: "liegen geblieben muss auch weg"): vermutete Ordner, Datenbank-Sicherungen und unklarer Inhalt gehen mit -
 * "Alles bereinigen" nimmt nie den Bereich Jailbreak (was nur mit seinem Spiel geht - z. B. die Trophäen-Liste eines Spiels auf der M.2 -, geht mit, wenn das Spiel geht) */
static bool komplett_nie(kern *k, const eintrag *f) { (void)k; return f->Kat == 7; }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 2) { fprintf(stderr, "Aufruf: test_auswahl <hier>\n"); return 2; }
    A0 = arena_neu();
    W = VERB(A0, argv[1], "/w");
    GEGENPROBE = getenv("GEGENPROBE") != NULL;
    kern *k = neu();
    if (!k) { printf("FEHLERZAHL 1\n"); return 1; }
    int n = k->Funde.n;
    printf("INFO %d Einträge im Testbaum\n", n);
    ablauf_ui ui = {0};

    /* 1. jeder Eintrag einzeln */
    int f0 = fehler, n1 = 0;
    for (int i = 0; i < n; i++) {
        kern *x = neu(); if (!x || x->Funde.n != n) { printf("FAIL einzeln: Suche nicht gleich (%d statt %d)\n", x ? x->Funde.n : -1, n); fehler++; break; }
        static ablauf AB; ablauf_init(&AB, x, &ui);
        eintrag *f = x->Funde.e[i]; const char *g;
        if (ablauf_eintrag_gesperrt(&AB, f, &g)) { kern_frei(x); continue; }
        ablauf_eintrag_setzen(&AB, f, true);
        n1++;
        loeschen_pruefen(x, VERB(A0, "einzeln \"", f->Text, "\""));
        kern_frei(x);
    }
    printf("%s jeder Eintrag einzeln: %d Läufe, nur Gewähltes gelöscht\n", fehler == f0 ? "OK  " : "FAIL", n1);

    /* 2. jeder Block mit dem Haken auf dem Kopf */
    f0 = fehler; int n2 = 0;
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        int kat = ALLE_KAT[ki];
        kblock *bl = NULL; int nb = kern_bloecke(k, A0, kat, SORT_GROESSTE, &bl);
        for (int b = 0; b < nb; b++) {
            if (bl[b].teile.n < 2) continue;   /* ein Block aus einer Zeile = Haken auf der Zeile (Weg 1) */
            kern *x = neu(); if (!x) break;
            kblock *xb = NULL; int xn = kern_bloecke(x, A0, kat, SORT_GROESSTE, &xb);
            if (xn != nb || xb[b].kopf == NULL || strcmp(xb[b].kopf->Text, bl[b].kopf->Text)) { printf("FAIL Block %s: Blöcke nicht gleich\n", bl[b].kopf->Text); fehler++; kern_frei(x); continue; }
            static ablauf AB; ablauf_init(&AB, x, &ui);
            if (!ablauf_block_setzen(&AB, &xb[b].teile)) { kern_frei(x); continue; }   /* alles nur einzeln: der Haken ändert nichts (Hinweis am Fernseher) */
            n2++;
            const char *was = VERB(A0, "Block \"", bl[b].kopf->Text, "\"");
            gesperrte_regel(x, was, block_nur_einzeln, &xb[b].teile, NULL);
            /* zweiter Haken: alles wieder aus */
            {   kern *y = neu(); kblock *yb = NULL; int yn = y ? kern_bloecke(y, A0, kat, SORT_GROESSTE, &yb) : 0;
                if (y && yn == nb) { static ablauf AY; ablauf_init(&AY, y, &ui); ablauf_block_setzen(&AY, &yb[b].teile); ablauf_block_setzen(&AY, &yb[b].teile);
                    int noch = 0; for (int t = 0; t < yb[b].teile.n; t++) if (((eintrag *)yb[b].teile.e[t])->Wahl) noch++;
                    if (noch) printf("FAIL %s: zweiter Haken lässt %d Teil(e) gewählt\n", was, noch), fehler++; }
                if (y) kern_frei(y); }
            loeschen_pruefen(x, was);
            kern_frei(x);
        }
    }
    printf("%s jeder Block mit dem Haken auf dem Kopf: %d Läufe, nichts nur-einzeln mit angehakt, nur Gewähltes gelöscht, zweiter Haken wählt ab\n", fehler == f0 ? "OK  " : "FAIL", n2);

    /* 3. jeder Bereich */
    f0 = fehler; int n3 = 0;
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        kern *x = neu(); if (!x) break;
        static ablauf AB; ablauf_init(&AB, x, &ui);
        ablauf_bereich_setzen(&AB, ALLE_KAT[ki], true);
        const char *was = VERB(A0, "Bereich ", kern_kat_name(ALLE_KAT[ki]));
        /* was der Bereich selbst anhakt: nur Einträge dieses Bereichs; mitgenommen wird erst beim Löschen */
        for (int i = 0; i < x->Funde.n; i++) { eintrag *f = x->Funde.e[i]; if (f->Wahl && f->Kat == ALLE_KAT[ki] && bereich_nur_einzeln(x, f)) printf("FAIL %s: \"%s\" ist mit angehakt, geht aber nur einzeln\n", was, f->Text), fehler++; }
        n3++;
        loeschen_pruefen(x, was);
        kern_frei(x);
    }
    printf("%s jeder Bereich (\"ganzen Bereich wählen\"): %d Läufe, nichts nur-einzeln mit angehakt, nur Gewähltes gelöscht\n", fehler == f0 ? "OK  " : "FAIL", n3);

    /* 4. Alles bereinigen */
    f0 = fehler;
    for (int v = 0; v < 4; v++) {
        bool m2 = v & 1, usb = v & 2;
        kern *x = neu(); if (!x) break;
        kern_waehle_komplett(x, m2, usb);
        const char *was = a_fmt(A0, "Alles bereinigen (M.2 %s, USB %s)", m2 ? "mit" : "ohne", usb ? "mit" : "ohne");
        gesperrte_regel(x, was, komplett_nie, NULL, NULL);
        loeschen_pruefen(x, was);
        kern_frei(x);
    }
    printf("%s Alles bereinigen (ohne/mit M.2, ohne/mit USB): nie Bereich Jailbreak, nur Gewähltes gelöscht\n", fehler == f0 ? "OK  " : "FAIL");

    printf("INFO %d Löschläufe, %d Buchungen geprüft\n", laeufe, buchungen);
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
