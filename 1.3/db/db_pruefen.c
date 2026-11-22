/* PS5 Cleanup - SQLite-Prüfung der Spieldatenbanken, ganz im Arbeitsspeicher - siehe db_pruefen.h
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * Dateiformat nach https://sqlite.org/fileformat2.html (Abschnitte "The Database Header", "The Rollback Journal",
 * "The WAL File Format", "Checksum Algorithm").
 */
#include "db_pruefen.h"
#include "sqlite/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char JOURNAL_KENNUNG[8] = { 0xd9, 0xd5, 0x05, 0xf9, 0x20, 0xa1, 0x63, 0xd7 };

const char *db_sqlite_version(void) { return sqlite3_libversion(); }

static unsigned long be32(const unsigned char *p) { return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3]; }
static unsigned long le32(const unsigned char *p) { return ((unsigned long)p[3] << 24) | ((unsigned long)p[2] << 16) | ((unsigned long)p[1] << 8) | p[0]; }

/* Prüfsumme des WAL (fileformat2 "Checksum Algorithm"): über 32-Bit-Wörter, paarweise, fortlaufend */
static void wal_summe(const unsigned char *d, size_t n, int gross, unsigned long *s1, unsigned long *s2)
{
    for (size_t i = 0; i + 8 <= n; i += 8) {
        unsigned long x0 = gross ? be32(d + i) : le32(d + i), x1 = gross ? be32(d + i + 4) : le32(d + i + 4);
        *s1 = (*s1 + x0 + *s2) & 0xFFFFFFFFUL;
        *s2 = (*s2 + x1 + *s1) & 0xFFFFFFFFUL;
    }
}

static int fehler(db_ergebnis *e, int art, const char *t)
{
    e->ergebnis = art;
    snprintf(e->grund, sizeof e->grund, "%s", t);
    for (char *q = e->grund; *q; q++) if (*q == '\n' || *q == '\r') *q = ' ';
    return art;
}

/* PRAGMA integrity_check: 1 = genau eine Zeile "ok" */
static int integritaet(sqlite3 *db, char *grund, size_t gl)
{
    sqlite3_stmt *st = NULL;
    grund[0] = 0;
    if (sqlite3_prepare_v2(db, "PRAGMA integrity_check", -1, &st, NULL) != SQLITE_OK) { snprintf(grund, gl, "integrity_check: %s", sqlite3_errmsg(db)); return 0; }
    int zeilen = 0, ok = 0, rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const char *z = (const char *)sqlite3_column_text(st, 0);
        zeilen++;
        if (zeilen == 1 && z && !strcmp(z, "ok")) ok = 1;
        else if (!grund[0]) snprintf(grund, gl, "integrity_check: %s", z ? z : "?");
    }
    if (rc != SQLITE_DONE && !grund[0]) snprintf(grund, gl, "integrity_check: %s", sqlite3_errmsg(db));
    sqlite3_finalize(st);
    return rc == SQLITE_DONE && zeilen == 1 && ok;
}

/* Speicher-Datenbank aus den Bytes b/bn (b wird übernommen, mit sqlite3_malloc64 angelegt) */
static sqlite3 *oeffne_speicher(unsigned char *b, size_t bn, char *grund, size_t gl)
{
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(":memory:", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "SQLite: %s", db ? sqlite3_errmsg(db) : "kein Speicher"); sqlite3_close(db); sqlite3_free(b); return NULL;
    }
    int rc = sqlite3_deserialize(db, "main", b, (sqlite3_int64)bn, (sqlite3_int64)bn, SQLITE_DESERIALIZE_FREEONCLOSE | SQLITE_DESERIALIZE_RESIZEABLE);
    if (rc != SQLITE_OK) { snprintf(grund, gl, "SQLite deserialize: %s", sqlite3_errmsg(db)); sqlite3_close(db); return NULL; }
    return db;
}

int db_pruefen(const unsigned char *d, size_t n, const unsigned char *wal, size_t wn,
               const unsigned char *jr, size_t jn, int mit_bild, db_ergebnis *e)
{
    memset(e, 0, sizeof *e);
    if (!d || n < 100 || memcmp(d, "SQLite format 3", 16) != 0) return fehler(e, DB_FEHLER, "kein SQLite-Dateikopf");
    unsigned ps = ((unsigned)d[16] << 8) | d[17];
    unsigned seite = ps == 1 ? 65536u : ps;
    if (seite < 512 || seite > 65536 || (seite & (seite - 1))) return fehler(e, DB_FEHLER, "Seitengröße ungültig");
    if (n % seite) return fehler(e, DB_FEHLER, "Dateigröße kein Vielfaches der Seitengröße");
    e->seitengroesse = seite;
    e->modus = d[18];

    /* 1. Rollback-Journal: Kennung am Anfang = Schreibvorgang lief gerade (heißes Journal) */
    if (jr && jn >= 8 && memcmp(jr, JOURNAL_KENNUNG, 8) == 0)
        return fehler(e, DB_SCHREIBT, "Rollback-Journal mit Kennung vorhanden - die Konsole schreibt gerade in diese Datenbank");

    /* 2. Arbeitskopie im Speicher (SQLite gibt sie beim Schließen frei) */
    size_t bn = n;
    unsigned char *b = sqlite3_malloc64(n);
    if (!b) return fehler(e, DB_FEHLER, "kein Arbeitsspeicher");
    memcpy(b, d, n);

    if (wal && wn > 0) {
        if (wn < 32) { sqlite3_free(b); return fehler(e, DB_FEHLER, "WAL-Datei kürzer als ihr Kopf"); }
        unsigned long magie = be32(wal);
        if (magie != 0x377f0682UL && magie != 0x377f0683UL) { sqlite3_free(b); return fehler(e, DB_FEHLER, "WAL-Datei ohne gültige Kennung"); }
        int gross = magie & 1;
        unsigned long wps = be32(wal + 8);
        if (wps != seite) { sqlite3_free(b); return fehler(e, DB_FEHLER, "WAL-Seitengröße passt nicht zur Datenbank"); }
        unsigned long s1 = 0, s2 = 0;
        wal_summe(wal, 24, gross, &s1, &s2);
        if (s1 != be32(wal + 24) || s2 != be32(wal + 28)) { sqlite3_free(b); return fehler(e, DB_FEHLER, "WAL-Kopf: Prüfsumme falsch"); }
        unsigned long salz1 = be32(wal + 16), salz2 = be32(wal + 20);
        size_t rahmen = 24 + (size_t)seite;
        /* erst nur prüfen: bis zu welchem Eintrag ist alles gültig, wo liegt der letzte Abschluss */
        int gueltig = 0, letzter_abschluss = -1; unsigned long db_seiten = 0;
        for (size_t o = 32; o + rahmen <= wn; o += rahmen) {
            const unsigned char *f = wal + o;
            if (be32(f + 8) != salz1 || be32(f + 12) != salz2) break;
            wal_summe(f, 8, gross, &s1, &s2);
            wal_summe(f + 24, seite, gross, &s1, &s2);
            if (s1 != be32(f + 16) || s2 != be32(f + 20)) break;
            if (be32(f) == 0) break;
            if (be32(f + 4) != 0) { letzter_abschluss = gueltig; db_seiten = be32(f + 4); }
            gueltig++;
        }
        int gesamt = (int)((wn - 32) / rahmen);
        e->wal_eintraege = letzter_abschluss + 1;
        e->wal_verworfen = gesamt - e->wal_eintraege;
        if (letzter_abschluss >= 0) {
            size_t neu = (size_t)db_seiten * seite;
            unsigned char *nb = sqlite3_realloc64(b, neu > bn ? neu : bn);
            if (!nb) { sqlite3_free(b); return fehler(e, DB_FEHLER, "kein Arbeitsspeicher"); }
            b = nb;
            if (neu > bn) memset(b + bn, 0, neu - bn);
            for (int i = 0; i <= letzter_abschluss; i++) {
                const unsigned char *f = wal + 32 + (size_t)i * rahmen;
                unsigned long pg = be32(f);
                if (pg > db_seiten) continue;   /* Seite hinter dem Ende der abgeschlossenen Datenbank */
                memcpy(b + (size_t)(pg - 1) * seite, f + 24, seite);
            }
            bn = neu;
        }
    }
    /* SQLite kann eine Speicher-Datenbank nicht im WAL-Modus öffnen - in der ARBEITSKOPIE auf Rollback umstellen
     * (Byte 18/19; die WAL-Einträge sind oben schon eingerechnet). Die Rohkopie auf dem Stick bleibt unverändert. */
    if (b[18] == 2) b[18] = 1;
    if (b[19] == 2) b[19] = 1;

    /* 3. integrity_check der Arbeitskopie */
    char g[300];
    sqlite3 *quelle = oeffne_speicher(b, bn, g, sizeof g);
    if (!quelle) return fehler(e, DB_FEHLER, g);
    if (!integritaet(quelle, g, sizeof g)) { sqlite3_close(quelle); return fehler(e, DB_FEHLER, g); }

    /* 4. Kopie über die SQLite-Backup-Schnittstelle, ebenfalls prüfen */
    sqlite3 *ziel = NULL;
    if (sqlite3_open_v2(":memory:", &ziel, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        snprintf(g, sizeof g, "SQLite: %s", ziel ? sqlite3_errmsg(ziel) : "kein Speicher"); sqlite3_close(ziel); sqlite3_close(quelle); return fehler(e, DB_FEHLER, g);
    }
    char ps_sql[64]; snprintf(ps_sql, sizeof ps_sql, "PRAGMA page_size=%u", seite);
    sqlite3_exec(ziel, ps_sql, NULL, NULL, NULL);
    sqlite3_backup *bk = sqlite3_backup_init(ziel, "main", quelle, "main");
    int rc = bk ? sqlite3_backup_step(bk, -1) : SQLITE_ERROR;
    int rc2 = bk ? sqlite3_backup_finish(bk) : SQLITE_ERROR;
    if (rc != SQLITE_DONE || rc2 != SQLITE_OK) {
        snprintf(g, sizeof g, "SQLite-Backup-Schnittstelle: %s", sqlite3_errmsg(ziel));
        sqlite3_close(ziel); sqlite3_close(quelle); return fehler(e, DB_FEHLER, g);
    }
    sqlite3_close(quelle);
    if (!integritaet(ziel, g, sizeof g)) { sqlite3_close(ziel); char g2[340]; snprintf(g2, sizeof g2, "Kopie: %s", g); return fehler(e, DB_FEHLER, g2); }
    sqlite3_int64 sz = 0;
    unsigned char *ser = sqlite3_serialize(ziel, "main", &sz, 0);
    sqlite3_close(ziel);
    if (!ser || sz <= 0) { sqlite3_free(ser); return fehler(e, DB_FEHLER, "Kopie ließ sich nicht aus SQLite holen"); }
    e->seiten = (unsigned long)((size_t)sz / seite);
    if (mit_bild) {
        e->bild = malloc((size_t)sz);
        if (!e->bild) { sqlite3_free(ser); return fehler(e, DB_FEHLER, "kein Arbeitsspeicher"); }
        memcpy(e->bild, ser, (size_t)sz); e->bild_n = (size_t)sz;
    }
    sqlite3_free(ser);
    e->ergebnis = DB_OK;
    return DB_OK;
}

void db_ergebnis_frei(db_ergebnis *e) { free(e->bild); e->bild = NULL; e->bild_n = 0; }

/* NEU 1.0.7 (da_Chris86 01.11.2026, Reste ohne Kachel): Kennung in app.db suchen - jede Tabelle mit einer Spalte "titleId"
 * (app.db der PS5 Pro FW 12.00: tbl_contentinfo, tbl_iconinfo_<Benutzer>, tbl_addon_<Benutzer>, tbl_playlist_<Benutzer>).
 * Ganz im Speicher auf einer Kopie der geprüften Bytes - keine Datei wird geöffnet. */
int db_titel_suchen(const unsigned char *bild, size_t n, const char *id, char *grund, size_t gl)
{
    grund[0] = 0;
    if (!bild || !n || !id || !id[0]) { snprintf(grund, gl, "keine Daten"); return -1; }
    unsigned char *b = sqlite3_malloc64(n);
    if (!b) { snprintf(grund, gl, "kein Speicher"); return -1; }
    memcpy(b, bild, n);
    sqlite3 *db = oeffne_speicher(b, n, grund, gl);
    if (!db) return -1;
    sqlite3_stmt *t = NULL;
    int erg = 0, rt = SQLITE_DONE, tabellen = 0;
    if (sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table'", -1, &t, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "Tabellen: %s", sqlite3_errmsg(db)); sqlite3_close(db); return -1;
    }
    while (erg == 0 && (rt = sqlite3_step(t)) == SQLITE_ROW) {
        const char *tab = (const char *)sqlite3_column_text(t, 0);
        if (!tab) continue;
        sqlite3_stmt *c = NULL; char *spalte = NULL; int rc;
        if (sqlite3_prepare_v2(db, "SELECT name FROM pragma_table_info(?1)", -1, &c, NULL) != SQLITE_OK) { snprintf(grund, gl, "Spalten: %s", sqlite3_errmsg(db)); erg = -1; break; }
        sqlite3_bind_text(c, 1, tab, -1, SQLITE_TRANSIENT);
        while ((rc = sqlite3_step(c)) == SQLITE_ROW) {
            const char *s = (const char *)sqlite3_column_text(c, 0);
            if (s && sqlite3_stricmp(s, "titleId") == 0) { spalte = sqlite3_mprintf("%s", s); break; }
        }
        sqlite3_finalize(c);
        if (!spalte && rc != SQLITE_DONE) { snprintf(grund, gl, "Spalten von %s: %s", tab, sqlite3_errmsg(db)); erg = -1; break; }
        if (!spalte) continue;
        tabellen++;
        char *q = sqlite3_mprintf("SELECT 1 FROM \"%w\" WHERE \"%w\" = ?1 LIMIT 1", tab, spalte);
        sqlite3_free(spalte);
        sqlite3_stmt *s = NULL;
        if (!q || sqlite3_prepare_v2(db, q, -1, &s, NULL) != SQLITE_OK) { snprintf(grund, gl, "Suche in %s: %s", tab, sqlite3_errmsg(db)); sqlite3_free(q); erg = -1; break; }
        sqlite3_free(q);
        sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(s);
        if (rc == SQLITE_ROW) erg = 1;
        else if (rc != SQLITE_DONE) { snprintf(grund, gl, "Suche in %s: %s", tab, sqlite3_errmsg(db)); erg = -1; }
        sqlite3_finalize(s);
    }
    /* Prüfung 01.11.2026: Lesefehler oder keine einzige Tabelle mit titleId (anderer Aufbau) -> unklar, nie "nicht in app.db" */
    if (erg == 0 && rt != SQLITE_DONE) { snprintf(grund, gl, "Tabellen: %s", sqlite3_errmsg(db)); erg = -1; }
    if (erg == 0 && tabellen == 0) { snprintf(grund, gl, "keine Tabelle mit titleId - Aufbau unbekannt"); erg = -1; }
    sqlite3_finalize(t);
    sqlite3_close(db);
    return erg;
}

int db_appinfo_zeilen(const unsigned char *bild, size_t n, void (*zeile)(void *ctx, const char *titel, const char *feld, const char *wert),
                      void *ctx, char *grund, size_t gl)
{
    grund[0] = 0;
    if (!bild || !n || !zeile) { snprintf(grund, gl, "keine Daten"); return -1; }
    unsigned char *b = sqlite3_malloc64(n);
    if (!b) { snprintf(grund, gl, "kein Speicher"); return -1; }
    memcpy(b, bild, n);
    sqlite3 *db = oeffne_speicher(b, n, grund, gl);
    if (!db) return -1;
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "SELECT titleId, key, CAST(val AS TEXT) FROM tbl_appinfo WHERE key = 'INSTALL_DIR_SAVEDATA' OR key GLOB 'SERVICE_ID_ADDCONT_ADD_[0-9]*'",
                           -1, &s, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "appinfo.db: %s", sqlite3_errmsg(db)); sqlite3_close(db); return -1;
    }
    int anz = 0, rc;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        const char *t = (const char *)sqlite3_column_text(s, 0), *f = (const char *)sqlite3_column_text(s, 1), *w = (const char *)sqlite3_column_text(s, 2);
        if (!t || !f) continue;
        zeile(ctx, t, f, w ? w : "");
        anz++;
    }
    if (rc != SQLITE_DONE) { snprintf(grund, gl, "appinfo.db: %s", sqlite3_errmsg(db)); anz = -1; }
    sqlite3_finalize(s);
    sqlite3_close(db);
    return anz;
}

/* NEU 1.0.8 (PS5-Test 07.11.2026): Sonys Spielstand-Liste savedata.db (Tabelle savedata) - je Spielstand Kennung und Titel. Nur lesen. */
int db_spielstand_zeilen(const unsigned char *bild, size_t n, void (*zeile)(void *ctx, const char *titel, const char *feld, const char *wert),
                         void *ctx, char *grund, size_t gl)
{
    grund[0] = 0;
    if (!bild || !n || !zeile) { snprintf(grund, gl, "keine Daten"); return -1; }
    unsigned char *b = sqlite3_malloc64(n);
    if (!b) { snprintf(grund, gl, "kein Speicher"); return -1; }
    memcpy(b, bild, n);
    sqlite3 *db = oeffne_speicher(b, n, grund, gl);
    if (!db) return -1;
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "SELECT CAST(title_id AS TEXT), CAST(main_title AS TEXT), CAST(sub_title AS TEXT) FROM savedata", -1, &s, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "savedata.db: %s", sqlite3_errmsg(db)); sqlite3_close(db); return -1;
    }
    int anz = 0, rc;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        const char *t = (const char *)sqlite3_column_text(s, 0), *h = (const char *)sqlite3_column_text(s, 1), *u = (const char *)sqlite3_column_text(s, 2);
        if (!t || !h || !h[0]) continue;
        /* Titel gleich Untertitel (L.A. Noire: zweimal "Benutzereinstellungen & Freischaltobjekte"): der Spielstand beschreibt nur sich selbst */
        if (u && !strcmp(h, u)) continue;
        zeile(ctx, t, "SPIELSTAND_TITEL", h);
        anz++;
    }
    if (rc != SQLITE_DONE) { snprintf(grund, gl, "savedata.db: %s", sqlite3_errmsg(db)); anz = -1; }
    sqlite3_finalize(s);
    sqlite3_close(db);
    return anz;
}

/* NEU 1.0.10 (da_Chris86 10.11.2026: Datenbank-Übersicht im Protokoll, nur lesen): je Zeile von tbl_contentinfo in app.db eine Meldung
 * (Kennung, "APPDB", Wert). Wert: "u=<uninstallable>;l=<contentLocation>;t=<appType>;h=<hddLocation>;x=<externalHddAppStatus>;
 * g=<Anzahl anderer Titel mit demselben localConceptId>;c=<localConceptId>;n=<titleName>" - der Name steht zuletzt (darf ';' enthalten).
 * Fehlt eine Spalte (andere Firmware), steht dort "?". Ganz im Speicher, nur lesen. Anzahl Zeilen, -1 = Fehler (grund). */
int db_appdb_zeilen(const unsigned char *bild, size_t n, void (*zeile)(void *ctx, const char *titel, const char *feld, const char *wert),
                    void *ctx, char *grund, size_t gl)
{
    grund[0] = 0;
    if (!bild || !n || !zeile) { snprintf(grund, gl, "keine Daten"); return -1; }
    unsigned char *b = sqlite3_malloc64(n);
    if (!b) { snprintf(grund, gl, "kein Speicher"); return -1; }
    memcpy(b, bild, n);
    sqlite3 *db = oeffne_speicher(b, n, grund, gl);
    if (!db) return -1;
    static const char *const SP[7] = { "titleName", "uninstallable", "contentLocation", "appType", "hddLocation", "externalHddAppStatus", "localConceptId" };
    int da[7] = { 0 }, titel_da = 0, rc;
    sqlite3_stmt *c = NULL;
    if (sqlite3_prepare_v2(db, "SELECT name FROM pragma_table_info('tbl_contentinfo')", -1, &c, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "app.db: %s", sqlite3_errmsg(db)); sqlite3_close(db); return -1;
    }
    while ((rc = sqlite3_step(c)) == SQLITE_ROW) {
        const char *s = (const char *)sqlite3_column_text(c, 0);
        if (!s) continue;
        if (!sqlite3_stricmp(s, "titleId")) titel_da = 1;
        for (int i = 0; i < 7; i++) if (!sqlite3_stricmp(s, SP[i])) da[i] = 1;
    }
    sqlite3_finalize(c);
    if (rc != SQLITE_DONE || !titel_da) { snprintf(grund, gl, "app.db: tbl_contentinfo ohne titleId - Aufbau unbekannt"); sqlite3_close(db); return -1; }
    char *q = sqlite3_mprintf("SELECT CAST(t.titleId AS TEXT), %s, %s, %s, %s, %s, %s, %s, %s FROM tbl_contentinfo t",
        da[0] ? "CAST(t.titleName AS TEXT)" : "NULL", da[1] ? "CAST(t.uninstallable AS TEXT)" : "NULL", da[2] ? "CAST(t.contentLocation AS TEXT)" : "NULL",
        da[3] ? "CAST(t.appType AS TEXT)" : "NULL", da[4] ? "CAST(t.hddLocation AS TEXT)" : "NULL", da[5] ? "CAST(t.externalHddAppStatus AS TEXT)" : "NULL",
        da[6] ? "CAST(t.localConceptId AS TEXT)" : "NULL",
        da[6] ? "(SELECT count(*) FROM tbl_contentinfo o WHERE o.localConceptId = t.localConceptId AND o.titleId <> t.titleId)" : "NULL");
    sqlite3_stmt *s = NULL;
    if (!q || sqlite3_prepare_v2(db, q, -1, &s, NULL) != SQLITE_OK) { snprintf(grund, gl, "app.db: %s", sqlite3_errmsg(db)); sqlite3_free(q); sqlite3_close(db); return -1; }
    sqlite3_free(q);
    int anz = 0;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        const char *t = (const char *)sqlite3_column_text(s, 0);
        if (!t || !t[0]) continue;
        const char *v[9];
        for (int i = 1; i <= 8; i++) { v[i] = (const char *)sqlite3_column_text(s, i); }
        char *w = sqlite3_mprintf("u=%s;l=%s;t=%s;h=%s;x=%s;g=%s;c=%s;n=%s", v[2] ? v[2] : "?", v[3] ? v[3] : "?", v[4] ? v[4] : "?", v[5] ? v[5] : "?",
                                  v[6] ? v[6] : "?", v[8] ? v[8] : "?", v[7] ? v[7] : "", v[1] ? v[1] : "");
        if (!w) { snprintf(grund, gl, "kein Speicher"); anz = -1; break; }
        zeile(ctx, t, "APPDB", w);
        sqlite3_free(w);
        anz++;
    }
    if (anz >= 0 && rc != SQLITE_DONE) { snprintf(grund, gl, "app.db: %s", sqlite3_errmsg(db)); anz = -1; }
    sqlite3_finalize(s);
    sqlite3_close(db);
    return anz;
}

/* NEU 1.0.10 (da_Chris86 12.11.2026 22:40, Zusatzinhalte): Sonys Liste der installierten Zusatzinhalte (addcont.db, Tabelle addcont:
 * title_id, dir_name, content_id - auf da_Chris86s PS5 12.11.2026 22:49 so gelesen). Je Zeile (Kennung, "ZUSATZ", "<dir_name>\t<content_id>"). */
int db_addcont_zeilen(const unsigned char *bild, size_t n, void (*zeile)(void *ctx, const char *titel, const char *feld, const char *wert),
                      void *ctx, char *grund, size_t gl)
{
    grund[0] = 0;
    if (!bild || !n || !zeile) { snprintf(grund, gl, "keine Daten"); return -1; }
    unsigned char *b = sqlite3_malloc64(n);
    if (!b) { snprintf(grund, gl, "kein Speicher"); return -1; }
    memcpy(b, bild, n);
    sqlite3 *db = oeffne_speicher(b, n, grund, gl);
    if (!db) return -1;
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "SELECT CAST(title_id AS TEXT), CAST(dir_name AS TEXT), CAST(content_id AS TEXT) FROM addcont", -1, &s, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "addcont.db: %s", sqlite3_errmsg(db)); sqlite3_close(db); return -1;
    }
    int anz = 0, rc;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        const char *t = (const char *)sqlite3_column_text(s, 0), *d = (const char *)sqlite3_column_text(s, 1), *c = (const char *)sqlite3_column_text(s, 2);
        if (!t || !t[0]) continue;
        char *w = sqlite3_mprintf("%s\t%s", d ? d : "", c ? c : "");
        if (!w) { snprintf(grund, gl, "kein Speicher"); anz = -1; break; }
        zeile(ctx, t, "ZUSATZ", w);
        sqlite3_free(w);
        anz++;
    }
    if (anz >= 0 && rc != SQLITE_DONE) { snprintf(grund, gl, "addcont.db: %s", sqlite3_errmsg(db)); anz = -1; }
    sqlite3_finalize(s);
    sqlite3_close(db);
    return anz;
}
