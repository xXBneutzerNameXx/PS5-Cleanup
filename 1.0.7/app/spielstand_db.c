/* PS5 Cleanup 1.0.7 - Spielstand-Einträge aus Sonys Spielstand-Liste (savedata.db) austragen
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (SQLite selbst: gemeinfrei, db/sqlite/sqlite3.c)
 *
 * Auftrag da_Chris86 01.11.2026: Spielstände ohne leeren Eintrag in der Spielstand-Liste löschen, ohne Datenbank-Neuaufbau
 * (17:31 "Neuaufbau der Datenbank ist inakzeptabel", 18:09 "wenn wir einmalig die Datenbank anrühren müssen für die Safe Games,
 * dann ist es ja okay", 18:27 "wir machen alles").
 * PS5-Test 01.11.2026 18:16 und 18:24 (PS5 Pro, FW 12.00, Test-ELF start/test_spielstand.c und test_spielstand2.c): Eintrag in
 * savedata.db gelöscht, danach die Dateien -> der Spielstand verschwand aus der Liste, beim letzten Spielstand eines Spiels die
 * ganze Gruppe; kein Neustart, kein Neuaufbau. Sonys eigene Löschfunktion (sceSaveDataDelete) lehnte im Test ab.
 * Öffentlicher Weg wie MacheteSVG (src/core/save_manager.cpp, sqlite_adapter.hpp): Zeile in savedata.db löschen, busy_timeout 3 s.
 *
 * Nur in der echten Fassung eingebaut (app/bauen.sh, Fassung release). Dieser Programmteil bringt eine EIGENE SQLite-Kopie MIT
 * Dateizugriff mit - alle SQLite-Funktionen sind hier "static" (nur in dieser Datei sichtbar, der Bau prüft das). Die übrige
 * Datenbank-Prüfung (db/) bleibt ohne Dateizugriff.
 *
 * Ablauf (alles oder nichts):
 *  1. savedata.db öffnen - nur eine vorhandene Datei, nichts anlegen
 *  2. BEGIN IMMEDIATE - ab jetzt kann niemand sonst in die Liste schreiben (Sonys Dienst wartet, höchstens 3 s, sonst Abbruch)
 *  3. PRAGMA quick_check muss "ok" sein, die Tabelle savedata muss da sein
 *  4. Einträge des Titels zählen - keine: nichts ändern
 *  5. DELETE FROM savedata WHERE title_id = ? - es müssen genau so viele Zeilen sein wie gezählt, sonst ROLLBACK
 *  6. COMMIT (klappt das nicht, ROLLBACK - die Liste bleibt wie sie war)
 * Bis 02.11.2026 stand vor Schritt 5 eine Sicherung der Liste auf dem USB-Stick. Entfernt auf Wunsch von da_Chris86 (02.11.2026
 * 15:44: "Wir speichern auf dem USB-Laufwerk nur das Protokoll") - die Prüfungen 1-4 und das "alles oder nichts" bleiben.
 * SQLite legt dabei kurz savedata.db-journal an (nötig für "alles oder nichts") und entfernt es nach dem COMMIT wieder.
 */
#define SQLITE_API static
#define SQLITE_EXTERN
#define SQLITE_THREADSAFE 0
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_OMIT_WAL 1
#define SQLITE_OMIT_SHARED_CACHE 1
#define SQLITE_OMIT_DEPRECATED 1
#define SQLITE_DEFAULT_MEMSTATUS 0
#define SQLITE_TEMP_STORE 3
#define SQLITE_MAX_MMAP_SIZE 0
#define SQLITE_DQS 0
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma clang diagnostic ignored "-Wunused-const-variable"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "../db/sqlite/sqlite3.c"

#include "spielstand_db.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* PRAGMA quick_check: 1 = genau eine Zeile "ok" */
static int sd_heil(sqlite3 *h, const char *name, char *grund, size_t gl)
{
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(h, "PRAGMA quick_check", -1, &st, NULL) != SQLITE_OK) { snprintf(grund, gl, "Prüfung nicht möglich (%s)", sqlite3_errmsg(h)); return 0; }
    int zeilen = 0, ok = 0, rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const char *z = (const char *)sqlite3_column_text(st, 0);
        if (++zeilen == 1 && z && !strcmp(z, "ok")) ok = 1;
    }
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE || zeilen != 1 || !ok) { snprintf(grund, gl, "%s ist beschädigt (quick_check) - nichts geändert", name); return 0; }
    return 1;
}

int spielstand_db_austragen(const char *db, const char *titel, int *zeilen, char *grund, size_t gl)
{
    *zeilen = 0;
    if (gl) grund[0] = 0;
    if (!db || !titel || !titel[0]) { snprintf(grund, gl, "Aufruf unvollständig - nichts geändert"); return -1; }
    struct stat st;
    if (stat(db, &st) != 0 || !S_ISREG(st.st_mode)) { snprintf(grund, gl, "Spielstand-Liste %s nicht gefunden", db); return -1; }
    sqlite3 *h = NULL;
    if (sqlite3_open_v2(db, &h, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "Spielstand-Liste nicht zu öffnen (%s)", h ? sqlite3_errmsg(h) : "kein Speicher");
        sqlite3_close(h); return -1;
    }
    sqlite3_busy_timeout(h, 3000);   /* wie MacheteSVG (PRAGMA busy_timeout=3000) */
    if (sqlite3_exec(h, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "Spielstand-Liste ist gerade belegt (%s) - nichts geändert", sqlite3_errmsg(h));
        sqlite3_close(h); return -1;
    }
    int erg = -1, n = -1;
    sqlite3_stmt *s = NULL;
    if (!sd_heil(h, "Spielstand-Liste", grund, gl)) goto zurueck;
    if (sqlite3_prepare_v2(h, "SELECT count(*) FROM savedata WHERE title_id = ?1", -1, &s, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "Spielstand-Liste hat nicht den erwarteten Aufbau (%s) - nichts geändert", sqlite3_errmsg(h)); goto zurueck;
    }
    sqlite3_bind_text(s, 1, titel, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
    sqlite3_finalize(s); s = NULL;
    if (n < 0) { snprintf(grund, gl, "Einträge ließen sich nicht zählen (%s) - nichts geändert", sqlite3_errmsg(h)); goto zurueck; }
    if (n > 0) {
        if (sqlite3_prepare_v2(h, "DELETE FROM savedata WHERE title_id = ?1", -1, &s, NULL) != SQLITE_OK) {
            snprintf(grund, gl, "Austragen nicht möglich (%s) - nichts geändert", sqlite3_errmsg(h)); goto zurueck;
        }
        sqlite3_bind_text(s, 1, titel, -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(s);
        sqlite3_finalize(s); s = NULL;
        if (rc != SQLITE_DONE || sqlite3_changes(h) != n) {
            snprintf(grund, gl, "Austragen unvollständig (%d von %d, %s) - zurückgenommen, nichts geändert", sqlite3_changes(h), n, sqlite3_errmsg(h)); goto zurueck;
        }
    }
    if (sqlite3_exec(h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "Spielstand-Liste ließ sich nicht abschließen (%s) - zurückgenommen, nichts geändert", sqlite3_errmsg(h)); goto zurueck;
    }
    erg = 0; *zeilen = n;
    goto ende;
zurueck:
    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
ende:
    sqlite3_close(h);
    return erg;
}

/* NEU 1.0.7 (da_Chris86 01.11.2026 22:03 "löschen, wenn normale Sony- oder Kachellöschungen nicht funktionieren"; PS5-Test
 * start/test_rdr2b.c 22:08): kaputte Zusatzinhalt-Angaben eines Titels in Sonys App-Datenbank (appinfo.db) reparieren.
 * Befund Red Dead Redemption 2 (CUSA08519, umgebauter Patch): SERVICE_ID_ADDCONT_ADD_1..7 = "\r\n  " - Sonys Deinstallation brach daran
 * ab (0x80B21B02, leere Kennung beim Austragen der Zusatzinhalte), im PS5-Menü stürzte ShellCore ab. Gültig ist eine Kennung wie
 * EP1004-CUSA15490_00 oder 0/leer (= unbenutzt, so bei L.A. Noire). Repariert wird nur, was keins von beidem ist: Wert 0.
 * Ablauf: 1. nur lesen und zählen (keine Sperre) - nichts kaputt: fertig, nichts geändert, nichts gesichert (der Normalfall).
 *         2. sonst wie spielstand_db_austragen: BEGIN IMMEDIATE, quick_check, neu zählen, UPDATE (genau so viele Zeilen wie
 *            gezählt, sonst ROLLBACK), COMMIT. Ohne Sicherung (da_Chris86 02.11.2026 15:44 - bis dahin vorher eine auf dem Stick).
 * 0 = ok (*zeilen = so viele Werte repariert, 0 = nichts zu tun), -1 = kaputt, aber nichts geändert, -2 = Prüfung nicht möglich. */
/* Codex-Prüfung 47 (1): nur Leerzeichen = unbenutzter Platzhalter (PS5-Test 03.11.2026, GTA Definitive Editions: 19 Leerzeichen) - nicht ändern */
#define AZ_KAPUTT "titleId = ?1 AND key GLOB 'SERVICE_ID_ADDCONT_ADD_[0-9]*' AND NOT (val IS NULL OR val = 0 OR val = '' OR val = '0' OR " \
                  "(typeof(val) = 'text' AND trim(val, ' ') = '') OR " \
                  "(length(val) = 19 AND val GLOB '[A-Z][A-Z][0-9][0-9][0-9][0-9]-[A-Z][A-Z][A-Z][A-Z][0-9][0-9][0-9][0-9][0-9]_[0-9][0-9]'))"
static int az_zaehlen(sqlite3 *h, const char *titel)
{
    sqlite3_stmt *s = NULL; int n = -1;
    if (sqlite3_prepare_v2(h, "SELECT count(*) FROM tbl_appinfo WHERE " AZ_KAPUTT, -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s, 1, titel, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    return n;
}
int appinfo_zusatz_reparieren(const char *db, const char *titel, int *zeilen, char *grund, size_t gl)
{
    *zeilen = 0;
    if (gl) grund[0] = 0;
    if (!db || !titel || !titel[0]) { snprintf(grund, gl, "Aufruf unvollständig"); return -2; }
    struct stat st;
    if (stat(db, &st) != 0 || !S_ISREG(st.st_mode)) { snprintf(grund, gl, "App-Datenbank %s nicht gefunden", db); return -2; }
    sqlite3 *h = NULL;
    if (sqlite3_open_v2(db, &h, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "App-Datenbank nicht zu öffnen (%s)", h ? sqlite3_errmsg(h) : "kein Speicher");
        sqlite3_close(h); return -2;
    }
    sqlite3_busy_timeout(h, 3000);
    int n = az_zaehlen(h, titel);
    if (n < 0) snprintf(grund, gl, "Zusatzinhalt-Angaben ließen sich nicht lesen (%s)", sqlite3_errmsg(h));
    sqlite3_close(h); h = NULL;
    if (n < 0) return -2;
    if (n == 0) return 0;
    if (sqlite3_open_v2(db, &h, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "App-Datenbank nicht zum Ändern zu öffnen (%s)", h ? sqlite3_errmsg(h) : "kein Speicher");
        sqlite3_close(h); return -1;
    }
    sqlite3_busy_timeout(h, 3000);
    if (sqlite3_exec(h, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "App-Datenbank ist gerade belegt (%s) - nichts geändert", sqlite3_errmsg(h));
        sqlite3_close(h); return -1;
    }
    int erg = -1, n2 = -1;
    sqlite3_stmt *s = NULL;
    if (!sd_heil(h, "App-Datenbank", grund, gl)) goto zurueck;
    n2 = az_zaehlen(h, titel);
    if (n2 < 0) { snprintf(grund, gl, "Zusatzinhalt-Angaben ließen sich nicht zählen (%s) - nichts geändert", sqlite3_errmsg(h)); goto zurueck; }
    if (n2 > 0) {
        if (sqlite3_prepare_v2(h, "UPDATE tbl_appinfo SET val = 0 WHERE " AZ_KAPUTT, -1, &s, NULL) != SQLITE_OK) {
            snprintf(grund, gl, "Reparatur nicht möglich (%s) - nichts geändert", sqlite3_errmsg(h)); goto zurueck;
        }
        sqlite3_bind_text(s, 1, titel, -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(s);
        sqlite3_finalize(s); s = NULL;
        if (rc != SQLITE_DONE || sqlite3_changes(h) != n2) {
            snprintf(grund, gl, "Reparatur unvollständig (%d von %d, %s) - zurückgenommen, nichts geändert", sqlite3_changes(h), n2, sqlite3_errmsg(h)); goto zurueck;
        }
    }
    if (sqlite3_exec(h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "App-Datenbank ließ sich nicht abschließen (%s) - zurückgenommen, nichts geändert", sqlite3_errmsg(h)); goto zurueck;
    }
    erg = 0; *zeilen = n2;
    goto ende;
zurueck:
    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
ende:
    sqlite3_close(h);
    return erg;
}
