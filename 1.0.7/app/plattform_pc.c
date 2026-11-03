/* PS5 Cleanup 1.1 - Oberfläche am Fernseher
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * plattform_pc.c - nur für die Vorschau am PC: eine Test-PS5 als Ordner
 * (Umgebung VORSCHAU_WURZEL), feste Konsolen-Angaben. Immer Simulation.
 */
#include "plattform.h"
#include "../db/db_sicherung.h"   /* NEU 1.0.7: appinfo.db nur lesen */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void plattform_absturzmelder(const char *name) { (void)name; }   /* 1.0.6 Fix: nur PS5 */

void plattform_start(void) { }
void plattform_ende(void) { }
const char *plattform_name(void) { return "PC-Vorschau"; }
bool plattform_neustart(void) { printf("PC-Vorschau: Neustart\n"); return true; }
bool plattform_neustart_uebergeben(void) { return false; }   /* NEU 1.0.6: am PC gibt es keinen Hintergrundteil */
bool plattform_hinweis_uebergeben(int art, bool englisch) { (void)art; (void)englisch; return false; }   /* NEU 1.0.7 */
void plattform_melden(const char *text) { printf("PC-Vorschau: Benachrichtigung: %s\n", text); }
/* NEU 1.0.7: feste Beispielwerte (Vorschau) - intern, M.2, USB-Stick */
bool plattform_speicher(int ort, long long *frei, long long *gesamt)
{
    const long long G = 1000LL * 1000 * 1000;
    switch (ort) {
    case 0: *frei = 312 * G; *gesamt = 1860 * G; return true;
    case 1: *frei = 1100 * G; *gesamt = 2000 * G; return true;
    case 3: *frei = 48 * G; *gesamt = 64 * G; return true;
    default: return false;
    }
}
bool plattform_englisch(void) { const char *s = getenv("VORSCHAU_SPRACHE"); return s && !strcmp(s, "en"); }
/* NEU 1.0.7: Protokoll-Datei in der PC-Vorschau nur über Umgebungsvariablen (sonst keine Datei) */
const char *plattform_protokoll_ordner(int nr) { return nr == 0 ? getenv("VORSCHAU_PROTOKOLL") : NULL; }
long long plattform_frei_bytes(const char *pfad) { (void)pfad; return -1; }
const char *plattform_stick_ordner(void) { return getenv("VORSCHAU_STICK"); }
void plattform_protokoll_zeile(const char *t) { (void)t; }
void plattform_protokoll_stick_setzen(const char *pfad) { (void)pfad; }
const char *plattform_db_ordner(void) { return getenv("VORSCHAU_MMS"); }
/* NEU 1.0.7 (PS5-Test 02.11.2026): appinfo.db nur lesen und die Zeilen an den Kern geben (siehe plattform.h) */
typedef struct { kern *k; void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert); } appinfo_ctx;
static void appinfo_cb(void *c, const char *titel, const char *feld, const char *wert) { appinfo_ctx *x = c; x->zeile(x->k, titel, feld, wert); }
void plattform_appinfo_lesen(struct kern *k, void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert))
{
    const char *q = plattform_db_ordner();
    if (!k || !zeile || !q) return;
    appinfo_ctx c = { k, zeile };
    const char *g = "";
    int r = db_appinfo_angaben(k->a, q, appinfo_cb, &c, &g);
    (void)r;
}
void (*plattform_meldung)(const char *t) = NULL;
const char *plattform_eigener_ort(void) { const char *s = getenv("VORSCHAU_ORT"); return s ? s : "/data/homebrew/PS5Cleanup"; }
void plattform_konsole(konsole_info *k)
{
    memset(k, 0, sizeof *k);
    const char *m = getenv("VORSCHAU_MODELL");
    snprintf(k->modell, sizeof k->modell, "%s", m ? m : "CFI-2016");
    snprintf(k->firmware, sizeof k->firmware, "12.00");
    k->ddd_bekannt = true; k->ddd = 1;
}
zugriff *plattform_zugriff(void)
{
    static char w[1024];
    const char *e = getenv("VORSCHAU_WURZEL");
    snprintf(w, sizeof w, "%s", e ? e : ".");
    datei_optionen o = { .wurzel = w, .sortiert = true, .ordnergroesse_null = true };
    return zugriff_sim(zugriff_datei(&o));
}
