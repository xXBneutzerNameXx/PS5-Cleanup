/* PS5 Cleanup 1.0.8 - Spieldatenbanken (app.db, appinfo.db) nur lesen und prüfen
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * Bis 02.11.2026 stand hier die Sicherung beider Datenbanken auf den USB-Stick (Auftrag da_Chris86 31.10.2026). Seitdem sichert
 * PS5 Cleanup nichts mehr (da_Chris86 02.11.2026: "Wir speichern auf dem USB-Stick nur das Protokoll."), in 1.0.8 ist der
 * schreibende Teil ganz entfernt (Plan 1.0.8 Punkt 4). Geblieben ist das Lesen - hier wird nie geschrieben:
 *  - db_beide_pruefen: app.db und appinfo.db lesen und prüfen (Prüf-ELF)
 *  - db_kennung_in_appdb: steht eine Kennung in app.db? (Reste ohne Kachel)
 *  - db_appinfo_angaben: Angaben aus appinfo.db (Zuordnungen, kaputte Zusatzinhalt-Angaben)
 * Ablauf je Datenbank: Hauptdatei, -wal und -journal je zweimal lesen (müssen gleich sein), dann im Speicher prüfen
 * (db_pruefen.c). Schreibt die Konsole gerade (Journal-Kennung) oder stimmt etwas nicht: bis zu dreimal neu lesen.
 */
#include "db_lesen.h"
#include "db_pruefen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DB_MAX (64L * 1024 * 1024)   /* mehr ist für diese Datenbanken nicht plausibel */

/* ganze Datei lesen; NULL = fehlt/nicht lesbar/zu groß (*fehlt = 1 bei "nicht da") */
static unsigned char *lies(const char *p, size_t *n, int *fehlt)
{
    *n = 0; if (fehlt) *fehlt = 0;
    FILE *f = fopen(p, "rb");
    if (!f) { if (fehlt) *fehlt = 1; return NULL; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long l = ftell(f);
    if (l < 0 || l > DB_MAX) { fclose(f); return NULL; }
    rewind(f);
    unsigned char *d = malloc((size_t)l + 1);
    if (!d) { fclose(f); return NULL; }
    size_t r = l > 0 ? fread(d, 1, (size_t)l, f) : 0;
    int fehler = ferror(f); fclose(f);
    if (fehler || r != (size_t)l) { free(d); return NULL; }
    *n = (size_t)l;
    return d;
}
/* zweimal lesen, beide Male gleich. 1 = ok (d gesetzt oder bei "fehlt" NULL), 0 = ändert sich / nicht lesbar */
static int lies2(const char *p, unsigned char **d, size_t *n, int *fehlt)
{
    size_t n2 = 0; int f2 = 0;
    *d = lies(p, n, fehlt);
    if (!*d) return *fehlt ? 1 : 0;
    unsigned char *d2 = lies(p, &n2, &f2);
    int gleich = d2 && n2 == *n && memcmp(*d, d2, n2) == 0;
    free(d2);
    if (!gleich) { free(*d); *d = NULL; }
    return gleich;
}
static void warte_ms(int ms) { struct timespec z = { ms / 1000, (long)(ms % 1000) * 1000000L }; nanosleep(&z, NULL); }

typedef struct { unsigned char *d, *wal, *jr; size_t n, wn, jn; db_ergebnis e; } db_teil;

bool db_beide_pruefen(arena *a, const char *quelle, const char **bericht)
{
    static const char *const DB[2] = { "app.db", "appinfo.db" };
    db_teil t[2]; memset(t, 0, sizeof t);
    const char *fehler = NULL, *b = "";

    /* lesen und im Speicher prüfen (mit_bild = 0: die geprüfte Kopie wird nicht gebraucht) */
    for (int i = 0; i < 2 && !fehler; i++) {
        const char *p = VERB(a, quelle, "/", DB[i]);
        const char *grund = NULL;
        for (int v = 0; v < 3; v++) {
            if (v > 0) warte_ms(1500);
            free(t[i].d); free(t[i].wal); free(t[i].jr); db_ergebnis_frei(&t[i].e);
            t[i].d = t[i].wal = t[i].jr = NULL;
            int fehlt = 0;
            if (!lies2(p, &t[i].d, &t[i].n, &fehlt)) { grund = T("ändert sich gerade oder ist nicht lesbar", "is changing right now or not readable"); continue; }
            if (!t[i].d) { grund = T("fehlt", "missing"); break; }
            int fw2 = 0, fj = 0;
            if (!lies2(VERB(a, p, "-wal"), &t[i].wal, &t[i].wn, &fw2) || !lies2(VERB(a, p, "-journal"), &t[i].jr, &t[i].jn, &fj)) {
                grund = T("Begleitdatei (-wal/-journal) ändert sich gerade oder ist nicht lesbar", "companion file (-wal/-journal) is changing or not readable"); continue;
            }
            int r = db_pruefen(t[i].d, t[i].n, t[i].wal, t[i].wn, t[i].jr, t[i].jn, 0, &t[i].e);
            if (r == DB_OK) { grund = NULL; break; }
            grund = a_kopie(a, t[i].e.grund);
            if (r == DB_FEHLER && v == 2) break;
        }
        if (grund) fehler = VERB(a, p, " - ", grund);
    }
    for (int i = 0; i < 2 && !fehler; i++) {
        const db_ergebnis *e = &t[i].e;
        const char *z = a_fmt(a, "%s: %zu Byte, %s, SQLite-Prüfung (integrity_check) ok, Kopie über die SQLite-Backup-Schnittstelle ok (%lu Seiten)",
                              DB[i], t[i].n, e->modus == 2 ? "WAL-Modus" : "Rollback-Journal-Modus", e->seiten);
        if (SP_EN) z = a_fmt(a, "%s: %zu bytes, %s, SQLite check (integrity_check) ok, copy via the SQLite backup API ok (%lu pages)",
                             DB[i], t[i].n, e->modus == 2 ? "WAL mode" : "rollback journal mode", e->seiten);
        if (t[i].wn) z = VERB(a, z, a_fmt(a, T(", WAL %zu Byte (%d Einträge eingerechnet, %d nicht abgeschlossen)", ", WAL %zu bytes (%d entries applied, %d not committed)"), t[i].wn, e->wal_eintraege, e->wal_verworfen));
        if (t[i].jn) z = VERB(a, z, a_fmt(a, T(", -journal %zu Byte ohne Kennung (abgeschlossen)", ", -journal %zu bytes without signature (finished)"), t[i].jn));
        b = VERB(a, b, "   ", z, "\n");
    }

    for (int i = 0; i < 2; i++) { free(t[i].d); free(t[i].wal); free(t[i].jr); db_ergebnis_frei(&t[i].e); }
    if (fehler) {
        for (char *q = (char *)(fehler = a_kopie(a, fehler)); *q; q++) if (*q == '\n') *q = ' ';
        *bericht = VERB(a, b, T("Datenbank-Prüfung FEHLGESCHLAGEN: ", "Database check FAILED: "), fehler);
        return false;
    }
    *bericht = b;
    return true;
}

/* NEU 1.0.7 (da_Chris86 01.11.2026, Reste ohne Kachel - siehe db_lesen.h). Nur lesen, bis zu drei Versuche (die Konsole kann
 * gerade schreiben). */
int db_kennung_in_appdb(arena *a, const char *quelle, const char *id, const char **grund)
{
    *grund = "";
    if (!quelle || !id || !id[0]) { *grund = T("app.db unbekannt", "app.db unknown"); return -1; }
    const char *p = VERB(a, quelle, "/app.db");
    const char *g = T("app.db nicht lesbar", "app.db not readable");
    for (int v = 0; v < 3; v++) {
        if (v > 0) warte_ms(1500);
        unsigned char *d = NULL, *wal = NULL, *jr = NULL; size_t n = 0, wn = 0, jn = 0; int fehlt = 0, f2 = 0, f3 = 0;
        if (!lies2(p, &d, &n, &fehlt) || !d) {
            free(d);
            g = fehlt ? T("app.db fehlt", "app.db missing") : T("app.db ändert sich gerade oder ist nicht lesbar", "app.db is changing right now or not readable");
            if (fehlt) break;
            continue;
        }
        if (!lies2(VERB(a, p, "-wal"), &wal, &wn, &f2) || !lies2(VERB(a, p, "-journal"), &jr, &jn, &f3)) {
            free(d); free(wal); free(jr);
            g = T("Begleitdatei von app.db ändert sich gerade", "companion file of app.db is changing right now");
            continue;
        }
        db_ergebnis e; memset(&e, 0, sizeof e);
        int r = db_pruefen(d, n, wal, wn, jr, jn, 1, &e);
        free(d); free(wal); free(jr);
        if (r != DB_OK || !e.bild) { g = a_kopie(a, e.grund[0] ? e.grund : "app.db: Prüfung fehlgeschlagen"); db_ergebnis_frei(&e); continue; }
        char gr[300];
        int t = db_titel_suchen(e.bild, e.bild_n, id, gr, sizeof gr);
        db_ergebnis_frei(&e);
        if (t >= 0) return t;
        g = a_kopie(a, gr);
    }
    *grund = g;
    return -1;
}

int db_appinfo_angaben(arena *a, const char *quelle, void (*zeile)(void *ctx, const char *titel, const char *feld, const char *wert),
                       void *ctx, const char **grund)
{
    *grund = "";
    if (!quelle || !zeile) { *grund = T("appinfo.db unbekannt", "appinfo.db unknown"); return -1; }
    const char *p = VERB(a, quelle, "/appinfo.db");
    unsigned char *d = NULL, *wal = NULL, *jr = NULL; size_t n = 0, wn = 0, jn = 0; int fehlt = 0, f2 = 0, f3 = 0;
    if (!lies2(p, &d, &n, &fehlt) || !d) {
        free(d);
        *grund = fehlt ? T("appinfo.db fehlt", "appinfo.db missing") : T("appinfo.db ändert sich gerade oder ist nicht lesbar", "appinfo.db is changing right now or not readable");
        return -1;
    }
    if (!lies2(VERB(a, p, "-wal"), &wal, &wn, &f2) || !lies2(VERB(a, p, "-journal"), &jr, &jn, &f3)) {
        free(d); free(wal); free(jr);
        *grund = T("Begleitdatei von appinfo.db ändert sich gerade", "companion file of appinfo.db is changing right now");
        return -1;
    }
    db_ergebnis e; memset(&e, 0, sizeof e);
    int r = db_pruefen(d, n, wal, wn, jr, jn, 1, &e);
    free(d); free(wal); free(jr);
    if (r != DB_OK || !e.bild) { *grund = a_kopie(a, e.grund[0] ? e.grund : "appinfo.db: Prüfung fehlgeschlagen"); db_ergebnis_frei(&e); return -1; }
    char gr[300];
    int z = db_appinfo_zeilen(e.bild, e.bild_n, zeile, ctx, gr, sizeof gr);
    db_ergebnis_frei(&e);
    if (z < 0) *grund = a_kopie(a, gr);
    return z;
}

/* NEU 1.0.8 (PS5-Test 07.11.2026): Sonys Spielstand-Liste - genauso nur gelesen und geprüft wie appinfo.db (ein Versuch) */
int db_spielstand_angaben(arena *a, const char *datei, void (*zeile)(void *ctx, const char *titel, const char *feld, const char *wert),
                          void *ctx, const char **grund)
{
    *grund = "";
    if (!datei || !zeile) { *grund = T("savedata.db unbekannt", "savedata.db unknown"); return -1; }
    unsigned char *d = NULL, *wal = NULL, *jr = NULL; size_t n = 0, wn = 0, jn = 0; int fehlt = 0, f2 = 0, f3 = 0;
    if (!lies2(datei, &d, &n, &fehlt) || !d) {
        free(d);
        *grund = fehlt ? T("savedata.db fehlt", "savedata.db missing") : T("savedata.db ändert sich gerade oder ist nicht lesbar", "savedata.db is changing right now or not readable");
        return -1;
    }
    if (!lies2(VERB(a, datei, "-wal"), &wal, &wn, &f2) || !lies2(VERB(a, datei, "-journal"), &jr, &jn, &f3)) {
        free(d); free(wal); free(jr);
        *grund = T("Begleitdatei von savedata.db ändert sich gerade", "companion file of savedata.db is changing right now");
        return -1;
    }
    db_ergebnis e; memset(&e, 0, sizeof e);
    int r = db_pruefen(d, n, wal, wn, jr, jn, 1, &e);
    free(d); free(wal); free(jr);
    if (r != DB_OK || !e.bild) { *grund = a_kopie(a, e.grund[0] ? e.grund : "savedata.db: Prüfung fehlgeschlagen"); db_ergebnis_frei(&e); return -1; }
    char gr[300];
    int z = db_spielstand_zeilen(e.bild, e.bild_n, zeile, ctx, gr, sizeof gr);
    db_ergebnis_frei(&e);
    if (z < 0) *grund = a_kopie(a, gr);
    return z;
}
