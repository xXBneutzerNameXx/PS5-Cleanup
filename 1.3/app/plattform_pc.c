/* PS5 Cleanup - Oberfläche am Fernseher
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
#ifdef MITSCHNITT
/* NEU 1.3 (21.11.2026, nur PC-Vorschau): die Liste einer echten PS5 ansehen - VORSCHAU_MITSCHNITT=<Mitschnitt vom Stick> spielt die Suche
 * VORSCHAU_MITSCHNITT_SUCHE (Standard 1) aus dem Mitschnitt nach (wie test/kern/test_mitschnitt.c "zeige"), immer als Simulation. Bilder liest
 * die Oberfläche weiter aus VORSCHAU_WURZEL. So lässt sich jede Seite am PC mit genau den Einträgen der Konsole prüfen. */
static zugriff *wg_z;
#endif
void plattform_appinfo_lesen(struct kern *k, void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert))
{
    const char *q = plattform_db_ordner();
    if (!k || !zeile) return;
    appinfo_ctx c = { k, zeile };
#ifdef MITSCHNITT
    if (wg_z) { zugriff_wiedergabe_appinfo(wg_z, appinfo_cb, &c); return; }
#endif
    const char *g = "";
    if (q) { int r = db_appinfo_angaben(k->a, q, appinfo_cb, &c, &g); (void)r; }
    if (q) { g = ""; int r = db_appdb_titel(k->a, q, appinfo_cb, &c, &g); (void)r; }   /* NEU 1.0.10: Datenbank-Übersicht */
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
/* NEU 1.3: Release-Notes in der PC-Vorschau - aus der Datei VORSCHAU_NEUESTE_TEXT (ohne: keine) */
void plattform_neueste_text(char *aus, size_t n)
{
    if (!n) return;
    aus[0] = 0;
    const char *d = getenv("VORSCHAU_NEUESTE_TEXT");
    if (!d || !d[0]) return;
    FILE *f = fopen(d, "rb");
    if (!f) return;
    size_t l = fread(aus, 1, n - 1, f); aus[l] = 0;
    fclose(f);
}
/* NEU 1.1: Netz in der PC-Vorschau - nur vorgetäuscht (VORSCHAU_NETZ=1), gesendet wird nie etwas. Was gesendet würde, steht in
 * VORSCHAU_SENDEN_DATEI (wenn gesetzt). */
static bool vorschau_netz(void) { const char *n = getenv("VORSCHAU_NETZ"); return n && n[0] == '1'; }
bool plattform_senden(const char *url, const char *eigene, const char *koerper, size_t len, char *grund, size_t gn)
{
    (void)eigene;
    const char *d = getenv("VORSCHAU_SENDEN_DATEI");
    if (d && d[0]) { FILE *f = fopen(d, "wb"); if (f) { fprintf(f, "%s\n", url); fwrite(koerper, 1, len, f); fclose(f); } }
    struct timespec ts = { 1, 0 }; nanosleep(&ts, NULL);
    if (!vorschau_netz()) { snprintf(grund, gn, "PC-Vorschau: kein Netz"); return false; }
    grund[0] = 0; return true;
}
bool plattform_holen(const char *url, const char *eigene, const char *accept, char **daten, size_t *len, size_t max, char *grund, size_t gn)
{
    (void)url; (void)eigene; (void)accept;
    *daten = NULL; *len = 0;
    const char *d = getenv("VORSCHAU_HOLEN");
    const char *e = getenv("VORSCHAU_HOLEN_ELF");   /* NEU 1.1: Update holen in der Vorschau - Antwort von GitHub und ELF getrennt */
    if (e && e[0] && accept && !strcmp(accept, "application/octet-stream")) d = e;
    if (!vorschau_netz() || !d || !d[0]) { snprintf(grund, gn, "PC-Vorschau: kein Netz"); return false; }
    FILE *f = fopen(d, "rb"); if (!f) { snprintf(grund, gn, "PC-Vorschau: %s fehlt", d); return false; }
    char *b = malloc(max + 1); size_t n = b ? fread(b, 1, max, f) : 0; fclose(f);
    if (!b) return false;
    b[n] = 0; *daten = b; *len = n; grund[0] = 0; return true;
}
void plattform_name_merken(const char *url) { (void)url; }
bool plattform_erreichbar(const char *url, const char *eigene, char *grund, size_t gn)
{
    (void)url; (void)eigene;
    struct timespec ts = { 0, 400000000L }; nanosleep(&ts, NULL);
    if (!vorschau_netz()) { snprintf(grund, gn, "PC-Vorschau: kein Netz"); return false; }
    grund[0] = 0; return true;
}
bool plattform_update_ablegen(const char *ordner, const char *name, const char *daten, size_t len, const char *alt, bool ersetzen,
                             char *ziel, size_t zn, bool *schon_da, bool *alt_geloescht, char *grund, size_t gn)
{
    (void)daten; (void)len; (void)alt; (void)ersetzen; *schon_da = false; *alt_geloescht = false; snprintf(ziel, zn, "%s/%s", ordner, name);
    snprintf(grund, gn, "PC-Vorschau: nichts geschrieben"); return true;
}
const char *plattform_eigener_ort(void) { const char *s = getenv("VORSCHAU_ORT"); return s ? s : "/data/homebrew/PS5Cleanup"; }
int plattform_upd_suchorte(const char **o, int max)   /* NEU 1.3: PC-Vorschau - Ordner aus VORSCHAU_UPD_ORTE ("a:b:c"), sonst keine */
{
    static char b[2048]; static const char *z[16]; static int n = -1;
    if (n < 0) {
        n = 0; const char *e = getenv("VORSCHAU_UPD_ORTE"); snprintf(b, sizeof b, "%s", e ? e : "");
        for (char *s = b, *k; s && *s && n < 16; s = k) { k = strchr(s, ':'); if (k) *k++ = 0; if (*s) z[n++] = s; }
    }
    int m = 0; for (int i = 0; i < n && m < max; i++) o[m++] = z[i];
    return m;
}
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
#ifdef MITSCHNITT
    const char *ms = getenv("VORSCHAU_MITSCHNITT");
    if (ms && ms[0]) {
        const char *fe = "", *nr = getenv("VORSCHAU_MITSCHNITT_SUCHE");
        zugriff *z = zugriff_wiedergabe(ms, nr && atoi(nr) > 0 ? atoi(nr) : 1, &fe);
        if (z) { wg_z = z; return zugriff_sim(z); }
        printf("PC-Vorschau: Mitschnitt nicht lesbar: %s\n", fe);
    }
#endif
    datei_optionen o = { .wurzel = w, .sortiert = true, .ordnergroesse_null = true };
    return zugriff_sim(zugriff_datei(&o));
}

/* NEU 1.0.10: am PC gibt es keine Liste der Konsolen-Programme */
int plattform_prozessliste(char *aus, size_t max) { if (aus && max) aus[0] = 0; return -1; }
int plattform_fremde_schliessen(char *aus, size_t max) { if (aus && max) aus[0] = 0; return -2; }
int plattform_hintergrund(char *aus, size_t max) { if (!aus || !max) return -1; const char *e = getenv("PC_HINTERGRUND"); snprintf(aus, max, "%s", e ? e : ""); return e && *e ? 1 : 0; }   /* NEU 1.3: am PC nur zum Testen vorgegeben */
void plattform_schliessen_auch(const char *liste) { (void)liste; }
