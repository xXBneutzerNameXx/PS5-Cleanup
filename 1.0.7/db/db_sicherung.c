/* PS5 Cleanup 1.0.7 - Sicherung der Spieldatenbanken (app.db, appinfo.db) vor einer Bereinigung
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * Auftrag da_Chris86 31.10.2026 19:56 (Punkt 2) und 21:50 (Punkte 1 und 3):
 *  - vor einer Bereinigung app.db und appinfo.db auf den USB-Stick sichern, Originale nie verändern
 *  - ohne Stick oder wenn die Sicherung nicht gelingt: KEINE Löschung
 *  - echte SQLite-Prüfung (integrity_check) samt Journal/WAL, Kopie über die SQLite-Backup-Schnittstelle
 *  - eine einfache Dateikopie gilt nicht als garantiert wiederherstellbar; keine automatische Rücksicherung
 * Ablauf je Datenbank: Hauptdatei, -wal und -journal je zweimal lesen (müssen gleich sein), dann im Speicher prüfen
 * (db_pruefen.c). Schreibt die Konsole gerade (Journal-Kennung) oder stimmt etwas nicht: bis zu dreimal neu lesen.
 * Erst wenn BEIDE Datenbanken geprüft sind, wird geschrieben: Rohkopien (Bytes wie auf der PS5), geprüfte Kopien
 * (<name>_geprueft.db, aus der SQLite-Backup-Schnittstelle) und info.txt - jede Datei zurückgelesen und verglichen.
 * Der schreibende Teil ist nur in der echten Fassung (KERN_LOESCHEN) eingebaut; Test-ELF/Simulation lesen nur.
 */
#include "db_sicherung.h"
#include "db_pruefen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/statvfs.h>

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
#ifdef KERN_LOESCHEN
static bool schreibe(const char *p, const unsigned char *d, size_t n)
{
    FILE *f = fopen(p, "rb");
    if (f) { fclose(f); return false; }   /* nie überschreiben */
    f = fopen(p, "wb");
    if (!f) return false;
    bool ok = (n == 0 || fwrite(d, 1, n, f) == n);
    if (fclose(f) != 0) ok = false;
    if (!ok) return false;
    size_t nz = 0; unsigned char *dz = lies(p, &nz, NULL);   /* zurücklesen und vergleichen */
    ok = dz && nz == n && memcmp(dz, d, n) == 0;
    free(dz);
    return ok;
}
#endif
static const char *sauber(arena *a, const char *t)   /* für Dateinamen: nur Buchstaben, Ziffern, Punkt, Minus */
{
    char *c = a_kopie(a, t && t[0] ? t : "unbekannt");
    for (char *q = c; *q; q++) if (!((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z') || (*q >= '0' && *q <= '9') || *q == '.' || *q == '-')) *q = '_';
    return c;
}
static void warte_ms(int ms) { struct timespec z = { ms / 1000, (long)(ms % 1000) * 1000000L }; nanosleep(&z, NULL); }

bool db_stick_platz(const char *ziel, long long bytes, long long *frei)
{
    struct statvfs s;
    *frei = -1;
    if (!ziel || statvfs(ziel, &s) != 0) return false;
    *frei = (long long)s.f_bavail * (long long)s.f_frsize;
    return *frei >= bytes;
}

typedef struct { unsigned char *d, *wal, *jr; size_t n, wn, jn; db_ergebnis e; } db_teil;

bool db_sichern(arena *a, const char *quelle, const char *ziel, const char *fw, const char *modell, bool schreiben,
                const char **bericht, const char **ablage)
{
    static const char *const DB[2] = { "app.db", "appinfo.db" };
#ifdef KERN_LOESCHEN
    static const char *const NAME[2] = { "app", "appinfo" };
#endif
    time_t j = time(NULL); struct tm tm; localtime_r(&j, &tm);
    const char *vor = ziel ? a_fmt(a, "%s/ps5cleanup_db_%04d-%02d-%02d_%02d%02d%02d_FW%s_%s_", ziel, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                   tm.tm_hour, tm.tm_min, tm.tm_sec, sauber(a, fw), sauber(a, modell)) : "";
    *ablage = vor;
    db_teil t[2]; memset(t, 0, sizeof t);
    const char *fehler = NULL, *b = "";

    /* Schritt 1: lesen und im Speicher prüfen (nichts wird geschrieben) */
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
            int r = db_pruefen(t[i].d, t[i].n, t[i].wal, t[i].wn, t[i].jr, t[i].jn, schreiben ? 1 : 0, &t[i].e);
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

#ifdef KERN_LOESCHEN
    /* Schritt 2: erst jetzt schreiben - Rohkopien, geprüfte Kopien, info.txt (jede Datei zurückgelesen) */
    if (!fehler && schreiben) {
        long long frei = 0, brauche = 4096; bool platz = true;
        for (int i = 0; i < 2; i++) brauche += (long long)(t[i].n + t[i].wn + t[i].jn + t[i].e.bild_n);
        if (!db_stick_platz(ziel, brauche * 2, &frei)) {
            platz = false;
            fehler = a_fmt(a, T("auf dem USB-Laufwerk ist zu wenig Platz frei (%lld Byte frei, %lld Byte nötig) oder es ist nicht lesbar", "not enough free space on the USB drive (%lld bytes free, %lld bytes needed) or it is not readable"), frei, brauche * 2);
        }
        for (int i = 0; i < 2 && !fehler; i++) {
            const char *roh = VERB(a, vor, DB[i]);
            if (!schreibe(roh, t[i].d, t[i].n)) fehler = roh;
            else if (t[i].wn && !schreibe(VERB(a, roh, "-wal"), t[i].wal, t[i].wn)) fehler = VERB(a, roh, "-wal");
            else if (t[i].jn && !schreibe(VERB(a, roh, "-journal"), t[i].jr, t[i].jn)) fehler = VERB(a, roh, "-journal");
            else if (!schreibe(VERB(a, vor, NAME[i], "_geprueft.db"), t[i].e.bild, t[i].e.bild_n)) fehler = VERB(a, vor, NAME[i], "_geprueft.db");
        }
        if (!fehler) {
            const char *info = VERB(a,
                "PS5 Cleanup - Sicherung der Spieldatenbanken / backup of the game databases\r\n",
                a_fmt(a, "%04d-%02d-%02d %02d:%02d:%02d  Firmware %s  Modell %s  SQLite %s\r\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                      tm.tm_hour, tm.tm_min, tm.tm_sec, fw && fw[0] ? fw : "?", modell && modell[0] ? modell : "?", db_sqlite_version()),
                "Quelle / source: ", quelle, "\r\n\r\n",
                "Rohkopien (Bytes genau wie auf der PS5, zweimal gleich gelesen): app.db, appinfo.db (und -wal/-journal, falls vorhanden).\r\n",
                "Gepruefte Kopien: app_geprueft.db, appinfo_geprueft.db - SQLite hat die Rohkopie samt WAL im Arbeitsspeicher geoeffnet,\r\n",
                "PRAGMA integrity_check meldete 'ok', die Kopie entstand mit der SQLite-Backup-Schnittstelle und wurde nochmals mit\r\n",
                "integrity_check geprueft. Jede Datei auf dem Stick wurde nach dem Schreiben zurueckgelesen und verglichen.\r\n\r\n",
                "WICHTIG: Das ist eine gepruefte Kopie - keine Garantie, dass eine Ruecksicherung funktioniert. PS5 Cleanup spielt nichts\r\n",
                "zurueck und hat keine Ruecksicherung eingebaut; eine Ruecksicherung wurde nie getestet.\r\n\r\n");
            for (int i = 0; i < 2; i++) {
                const char *z = a_fmt(a, "%s: %zu Byte, Modus %s, Seitengroesse %u, %lu Seiten in der geprueften Kopie", DB[i], t[i].n, t[i].e.modus == 2 ? "WAL" : "Rollback-Journal", t[i].e.seitengroesse, t[i].e.seiten);
                if (t[i].wn) z = VERB(a, z, a_fmt(a, ", WAL %zu Byte (%d Eintraege eingerechnet, %d nicht abgeschlossen)", t[i].wn, t[i].e.wal_eintraege, t[i].e.wal_verworfen));
                info = VERB(a, info, z, "\r\n");
            }
            if (!schreibe(VERB(a, vor, "info.txt"), (const unsigned char *)info, strlen(info))) fehler = VERB(a, vor, "info.txt");
        }
        if (fehler && platz) fehler = VERB(a, fehler, T(" ließ sich nicht schreiben, gibt es schon oder stimmt beim Zurücklesen nicht", " could not be written, already exists or differs when read back"));
        if (!fehler) b = VERB(a, b, T("   Alle Dateien auf dem USB-Laufwerk zurückgelesen und gleich.\n", "   All files on the USB drive read back and identical.\n"));
    }
#else
    if (schreiben && !fehler) fehler = T("diese Fassung schreibt nichts (Test/Simulation)", "this version writes nothing (test/simulation)");
#endif
    for (int i = 0; i < 2; i++) { free(t[i].d); free(t[i].wal); free(t[i].jr); db_ergebnis_frei(&t[i].e); }
    if (fehler) {
        for (char *q = (char *)(fehler = a_kopie(a, fehler)); *q; q++) if (*q == '\n') *q = ' ';
        *bericht = VERB(a, b, T("Datenbank-Sicherung FEHLGESCHLAGEN: ", "Database backup FAILED: "), fehler);
        return false;
    }
    *bericht = b;
    return true;
}

/* NEU 1.0.7 (da_Chris86 01.11.2026, Reste ohne Kachel - siehe db_sicherung.h). Nur lesen, bis zu drei Versuche (die Konsole kann
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
