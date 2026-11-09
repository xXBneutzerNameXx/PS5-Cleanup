/* PS5 Cleanup 1.1 - Oberfläche am Fernseher
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * plattform_pc.c - nur für die Vorschau am PC: eine Test-PS5 als Ordner
 * (Umgebung VORSCHAU_WURZEL), feste Konsolen-Angaben. Immer Simulation.
 */
#include "plattform.h"
#include "../db/db_lesen.h"   /* NEU 1.0.7: appinfo.db nur lesen */
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
int plattform_usb_anzahl(void) { return 1; }   /* Codex-Prüfung 91: in der Vorschau ein USB-Laufwerk */
bool plattform_englisch(void) { const char *s = getenv("VORSCHAU_SPRACHE"); return s && !strcmp(s, "en"); }
/* NEU 1.0.7: Protokoll-Datei in der PC-Vorschau nur über Umgebungsvariablen (sonst keine Datei) */
const char *plattform_protokoll_ordner(int nr) { return nr == 0 ? getenv("VORSCHAU_PROTOKOLL") : NULL; }
long long plattform_frei_bytes(const char *pfad) { (void)pfad; return -1; }
const char *plattform_stick_ordner(void) { return getenv("VORSCHAU_STICK"); }
void plattform_protokoll_zeile(const char *t) { (void)t; }
void plattform_protokoll_stick_setzen(const char *pfad) { (void)pfad; }
const char *plattform_db_ordner(void) { return getenv("VORSCHAU_MMS"); }
#ifdef MITSCHNITT
#include <sys/stat.h>
/* NEU 1.0.8 (Arbeitsplan 2.7): Vorschau mit Mitschnitt - Ordner aus VORSCHAU_TEST (sonst ./PS5-Cleanup-Test) */
const char *plattform_test_ordner(void)
{
    const char *o = getenv("VORSCHAU_TEST"); static char p[512];
    snprintf(p, sizeof p, "%s", o && o[0] ? o : "PS5-Cleanup-Test");
    mkdir(p, 0755);
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode) ? p : NULL;
}
#endif
/* NEU 1.0.7 (PS5-Test 02.11.2026): appinfo.db nur lesen und die Zeilen an den Kern geben (siehe plattform.h) */
typedef struct { kern *k; void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert); } appinfo_ctx;
static void appinfo_cb(void *c, const char *titel, const char *feld, const char *wert) { appinfo_ctx *x = c; x->zeile(x->k, titel, feld, wert); }
void plattform_appinfo_lesen(struct kern *k, void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert))
{
    const char *q = plattform_db_ordner();
    if (!k || !zeile) return;
    appinfo_ctx c = { k, zeile };
    const char *g = "";
    if (q) { int r = db_appinfo_angaben(k->a, q, appinfo_cb, &c, &g); (void)r; }
    /* NEU 1.0.8 (PS5-Test 07.11.2026): Sonys Spielstand-Liste - in der Vorschau eine Datei aus VORSCHAU_SAVEDATA */
    const char *s = getenv("VORSCHAU_SAVEDATA");
    if (s && s[0]) { g = ""; int r = db_spielstand_angaben(k->a, s, appinfo_cb, &c, &g); (void)r; }
}
void (*plattform_meldung)(const char *t) = NULL;
/* NEU 1.0.9 (Update-Hinweis): die PC-Vorschau fragt nie im Netz nach. VORSCHAU_NEUESTE gibt die Antwort vor (z. B. "v1.0.10"),
 * VORSCHAU_NEUESTE_MS wartet vorher so viele Millisekunden (wie eine langsame Antwort). Ohne VORSCHAU_NEUESTE: wie ohne Netz. */
#include <time.h>
bool plattform_neueste_version(const char *eigene, char *tag, size_t n, char *grund, size_t gn)
{
    (void)eigene;
    const char *w = getenv("VORSCHAU_NEUESTE_MS");
    if (w && atoi(w) > 0) { struct timespec ts = { atoi(w) / 1000, (long)(atoi(w) % 1000) * 1000000L }; nanosleep(&ts, NULL); }
    const char *v = getenv("VORSCHAU_NEUESTE");
    if (!v || !v[0]) { snprintf(grund, gn, "PC-Vorschau: keine Abfrage im Netz"); return false; }
    snprintf(tag, n, "%s", v);
    return true;
}
const char *plattform_eigener_ort(void) { const char *s = getenv("VORSCHAU_ORT"); return s ? s : "/data/homebrew/PS5Cleanup"; }
void plattform_konsole(konsole_info *k)
{
    memset(k, 0, sizeof *k);
    const char *m = getenv("VORSCHAU_MODELL");
    snprintf(k->modell, sizeof k->modell, "%s", m ? m : "CFI-2016");
    snprintf(k->firmware, sizeof k->firmware, "12.00");
    const char *d = getenv("VORSCHAU_DDD");   /* NEU 1.0.8: andere Kopplungs-Werte in der Vorschau prüfen (Standard 1 = online gekoppelt) */
    k->ddd_bekannt = true; k->ddd = d ? atoi(d) : 1;
}
zugriff *plattform_zugriff(void)
{
    static char w[1024];
    const char *e = getenv("VORSCHAU_WURZEL");
    snprintf(w, sizeof w, "%s", e ? e : ".");
    datei_optionen o = { .wurzel = w, .sortiert = true, .ordnergroesse_null = true };
    return zugriff_sim(zugriff_datei(&o));
}
