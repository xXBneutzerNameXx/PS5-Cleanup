/* PS5 Cleanup 1.0.6 - Oberfläche am Fernseher
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * plattform_pc.c - nur für die Vorschau am PC: eine Test-PS5 als Ordner
 * (Umgebung VORSCHAU_WURZEL), feste Konsolen-Angaben. Immer Simulation.
 */
#include "plattform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void plattform_start(void) { }
void plattform_ende(void) { }
const char *plattform_name(void) { return "PC-Vorschau"; }
bool plattform_neustart(void) { printf("PC-Vorschau: Neustart\n"); return true; }
bool plattform_neustart_uebergeben(void) { return false; }   /* NEU 1.0.6: am PC gibt es keinen Hintergrundteil */
bool plattform_ausschalten(void) { printf("PC-Vorschau: Ausschalten\n"); return true; }   /* NEU 1.0.6 */
bool plattform_ausschalten_uebergeben(void) { return false; }
bool plattform_englisch(void) { const char *s = getenv("VORSCHAU_SPRACHE"); return s && !strcmp(s, "en"); }
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
