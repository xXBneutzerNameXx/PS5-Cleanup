/* PS5 Cleanup 1.0.6 - Prüflauf vor Schritt 10 (nur lesen)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * pruefen.c - dieselben Zeilen wie TestKernRef.cs "pruefen" (1.0.5 unverändert):
 *   P <Erlaubt 0/1> <OhneVerknuepfung 0/1> <Pfad>      alle Pfade der Liste (+ Ordner fürs Aufräumen danach)
 *   S <Selbstschutz 0/1> <Pfad>                         nur 1.1 (mit Selbstschutz)
 *   G <Bytes> <Dateien> <Ordner> <ganz|gekappt> <Pfad>  Größe je Pfad nach der Hintergrund-Zählung
 *   E <..wie G..> <Kat> | <Text>                         Größe je Eintrag (EintragGroesse wie gefunden)
 *   WAHL ... ## <Bytes> <Dateien> <Ordner>               Auswahl der vollständigen Bereinigung
 *   SUMME <..wie G..> | <GroesseSatz>
 * Gezählt wird nur gelesen (Ordnerlisten), gelöscht wird nichts.
 */
#include "pruefen.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

static bool nie(void *c) { (void)c; return false; }
static void gezaehlt(void *c, eintrag *e) { (void)e; (*(int *)c)++; }
static const char *gz(arena *a, const groesse *g)
{
    return a_fmt(a, "%lld %d %d %s", g->Bytes, g->Dateien, g->Ordner, g->Gekappt ? "gekappt" : "ganz");
}
static void neu(arena *a, sliste *l, sliste *ges, const char *p)
{
    if (!p || !p[0]) return;
    for (int i = 0; i < ges->n; i++) if (!strcmp(ges->e[i], p)) return;
    sl_add(a, ges, p); sl_add(a, l, p);
}
static void wahl(kern *k, const char *titel, const groesse *g)
{
    arena *a = k->a;
    printf("== %s\n", titel);
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        if (!f->Wahl) continue;
        const char *gr = !g ? "" : f->HatGroesse ? a_fmt(a, " ## %lld %d %d", f->Bytes, f->Dateien, f->Ordner) : " ## -";
        printf("WAHL %d | %s :: %s%s\n", f->Kat, f->Text, sl_join(a, &f->Pfade, " | "), gr);
    }
    if (g) printf("SUMME %s | %s\n", gz(a, g), kern_groesse_satz(a, *g));
}

void pruef_ausgabe(kern *k, const char *titel)
{
    arena *a = k->a;
    printf("PRUEFLAUF %s\n", titel);
    /* 1 Schutzentscheidung je Pfad */
    sliste pfade = {0}, ges = {0};
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        for (int j = 0; j < f->Pfade.n; j++) neu(a, &pfade, &ges, f->Pfade.e[j]);
        for (int j = 0; j < f->PfadeBasis.n; j++) neu(a, &pfade, &ges, f->PfadeBasis.e[j]);
        neu(a, &pfade, &ges, f->Quelle);
    }
    for (int i = 0; i < k->AUFGEKLAPPT.n; i++) neu(a, &pfade, &ges, k->AUFGEKLAPPT.e[i]);
    for (int i = 0; i < k->LEER_DANACH.n; i++) { sliste *l = k->LEER_DANACH.e[i].v; for (int j = 0; l && j < l->n; j++) neu(a, &pfade, &ges, l->e[j]); }
    printf("== PFADE %d\n", pfade.n);
    for (int i = 0; i < pfade.n; i++) {
        const char *p = pfade.e[i];
        const char * volatile v = "?";
        VERSUCHE { v = kern_ohne_verknuepfung(k, p) ? "1" : "0"; }
        FANGE { v = VERB(a, "AUSNAHME ", ausn_text); } ENDE_VERSUCHE;
        printf("P %s %s %s\n", kern_erlaubt(k, p) ? "1" : "0", v, p);
        if (k->SCHUTZ.aktiv) printf("S %s %s\n", kern_selbstschutz_gesperrt(k, p) ? "1" : "0", p);
    }
    fflush(stdout);
    /* 2 Größen: wie die Hintergrund-Zählung der Oberfläche */
    int n = 0;
    kern_groessen_im_hintergrund(k, nie, NULL, gezaehlt, &n);
    printf("== GROESSEN gezaehlt=%d von %d\n", n, k->Funde.n);
    sliste gp = {0}, gs = {0};
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        for (int j = 0; j < f->PfadeBasis.n; j++) neu(a, &gp, &gs, f->PfadeBasis.e[j]);
        if (f->Kat == 1 && f->Quelle[0]) neu(a, &gp, &gs, f->Quelle);
    }
    pthread_mutex_lock((pthread_mutex_t *)k->groessen_sperre);
    for (int i = 0; i < gp.n; i++) {
        groesse *g = mt_hole(&k->PFAD_GROESSE, gp.e[i]);
        if (g) printf("G %s %s\n", gz(a, g), gp.e[i]); else printf("G FEHLT %s\n", gp.e[i]);
    }
    pthread_mutex_unlock((pthread_mutex_t *)k->groessen_sperre);
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        groesse *g = kern_eintrag_groesse(k, f, true);
        printf("E %s %d | %s\n", g ? gz(a, g) : "FEHLT", f->Kat, f->Text);
    }
    fflush(stdout);
    /* 3 Auswahl der vollständigen Bereinigung (nur Häkchen im Speicher) */
    groesse g;
    kern_waehle_komplett(k, false, false); g = kern_ermittle_groessen(k); wahl(k, "KOMPLETT", &g);
    kern_waehle_komplett(k, true, true); g = kern_ermittle_groessen(k); wahl(k, "KOMPLETT_M2", &g);
    kern_waehle_komplett(k, false, false); kern_setze_sm_spieldateien(k, false); g = kern_ermittle_groessen(k); wahl(k, "KOMPLETT_SM_NUR_KACHEL", &g);
    kern_setze_sm_spieldateien(k, true); g = kern_ermittle_groessen(k); wahl(k, "KOMPLETT_SM_MIT_SPIEL", &g);
    pliste spiele = {0};
    for (int i = 0; i < k->Funde.n; i++) if (ist_teil(k->Funde.e[i], "Spiel")) pl_add(a, &spiele, k->Funde.e[i]);
    for (int s = 0; s < spiele.n; s++) {
        eintrag *sp = spiele.e[s];
        kern_sm_zuruecksetzen(k);
        for (int i = 0; i < k->Funde.n; i++) ((eintrag *)k->Funde.e[i])->Wahl = false;
        sp->Wahl = true; kern_abhaengige_mitnehmen(k);
        wahl(k, VERB(a, "SPIEL ", sp->Gruppe), NULL);
    }
    for (int i = 0; i < k->Funde.n; i++) ((eintrag *)k->Funde.e[i])->Wahl = false;
    kern_sm_zuruecksetzen(k);
    printf("PRUEFLAUF ENDE %s\n", titel);
    fflush(stdout);
}
