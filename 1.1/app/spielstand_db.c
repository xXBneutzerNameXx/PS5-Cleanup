/* PS5 Cleanup - Spielstand-Einträge aus Sonys Spielstand-Liste (savedata.db) austragen
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
        int tc_vor = sqlite3_total_changes(h);   /* Codex-Prüfung 206 (1): zählt auch, was Trigger ändern */
        long long ges_vor = -1, ges_nach = -1;   /* Codex-Prüfung 204 (1): ändert das Austragen mehr als die gezählten Zeilen (z. B. über einen Trigger), wird zurückgenommen */
        if (sqlite3_prepare_v2(h, "SELECT count(*) FROM savedata", -1, &s, NULL) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW) ges_vor = sqlite3_column_int64(s, 0);
        sqlite3_finalize(s); s = NULL;
        if (sqlite3_prepare_v2(h, "DELETE FROM savedata WHERE title_id = ?1", -1, &s, NULL) != SQLITE_OK) {
            snprintf(grund, gl, "Austragen nicht möglich (%s) - nichts geändert", sqlite3_errmsg(h)); goto zurueck;
        }
        sqlite3_bind_text(s, 1, titel, -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(s);
        sqlite3_finalize(s); s = NULL;
        if (rc != SQLITE_DONE || sqlite3_changes(h) != n) {
            snprintf(grund, gl, "Austragen unvollständig (%d von %d, %s) - zurückgenommen, nichts geändert", sqlite3_changes(h), n, sqlite3_errmsg(h)); goto zurueck;
        }
        if (sqlite3_prepare_v2(h, "SELECT count(*) FROM savedata", -1, &s, NULL) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW) ges_nach = sqlite3_column_int64(s, 0);
        sqlite3_finalize(s); s = NULL;
        if (ges_vor < 0 || ges_nach != ges_vor - n || sqlite3_total_changes(h) - tc_vor != n) {
            snprintf(grund, gl, "Beim Austragen hätten sich auch andere Einträge der Spielstand-Liste geändert - zurückgenommen, nichts geändert"); goto zurueck;
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

/* =====================================================================================================================
 * NEU 1.0.10 (da_Chris86 12.11.2026 21:40, TabShifter TBSH00001): Kachel, die ein Programm direkt in Sonys Datenbanken eingetragen hat
 * (ohne Sonys Installation) und von der nachweislich keine Datei mehr da ist, gezielt aus app.db und appinfo.db austragen.
 * Grundlage: TabShifter (github xvvulcanvx/PS5-TabShifter main.c install_tabshifter_launcher) kopiert die Zeilen einer anderen App in
 * tbl_contentinfo, tbl_iconinfo_<Benutzer>, tbl_concepticoninfo_<Benutzer> und appinfo.db tbl_appinfo und ändert die Kennung; Sony lehnt
 * das Entfernen ab (PS5-Test 12.11.2026 12:46, 0x80F50005). Die Deinstallation der Gemeinschaft (PR #21) löscht dieselben Stellen
 * ungeprüft (Rückgabewerte nicht ausgewertet, zwei getrennte Schreibvorgänge). Hier stattdessen - für jede Kennung gleich, ohne Namen:
 *  1. app.db öffnen, appinfo.db dazu (ATTACH) - beide in EINEM Schreibvorgang: alles oder nichts über beide Dateien
 *  2. BEGIN IMMEDIATE (Sonys Dienste warten, höchstens 3 s, sonst nichts geändert), quick_check beider Dateien
 *  3. Bedingungen: genau eine Zeile in tbl_contentinfo, uninstallable = 1, keine Systemkennung, KEIN Sony-Paket (contentId leer - so
 *     stehen auf da_Chris86s PS5 genau die Homebrew-Kacheln mit Webseite auf 127.0.0.1 da; Spiele, Apps und PKG-Homebrew haben eine
 *     contentId und gehen nur über Sony), die Spielgruppe (localConceptId) gehört keinem anderen Titel, kein Auslöser (Trigger), der beim
 *     Löschen etwas anderes ändert
 *  4. jede Zeile in JEDER Tabelle beider Dateien, in der die Kennung vorkommt, muss zu den bekannten Stellen gehören - sonst nichts
 *  5. zählen, löschen (genau so viele Zeilen wie gezählt), danach: die Kennung steht nirgends mehr, quick_check, Fremdschlüssel-Prüfung
 *     nicht schlechter als vorher - dann COMMIT, sonst ROLLBACK
 * nur_pruefen: alles bis auf das Löschen (am Ende ROLLBACK). Dateien werden hier nicht angefasst. *verweis (darf NULL sein): Adresse der
 * Kachel (pprDeeplinkUri) - der Aufrufer prüft damit, ob das Programm gerade läuft.
 * 0 = ausgetragen bzw. (nur_pruefen) austragbar, *zeilen = Anzahl Zeilen; -1 = nichts geändert (*grund); -2 = Prüfung nicht möglich. */
typedef struct { const char *schema, *tabelle, *wo; int art; long long n; int frei; } kz_stelle;   /* frei: tabelle mit sqlite3_mprintf */   /* art 0: ?1 = Kennung, 1: ?1 = Spielgruppe, 2: beides */
static int kz_spalte_da(sqlite3 *h, const char *schema, const char *tab, const char *sp)
{
    char *q = sqlite3_mprintf("SELECT count(*) FROM pragma_table_info(%Q, %Q) WHERE name = %Q", tab, schema, sp);
    sqlite3_stmt *s = NULL; int n = -1;
    if (q && sqlite3_prepare_v2(h, q, -1, &s, NULL) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
    sqlite3_finalize(s); sqlite3_free(q);
    return n;
}
static long long kz_zahl(sqlite3 *h, const char *sql, const char *a1, const char *a2)
{
    sqlite3_stmt *s = NULL; long long n = -1;
    if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) != SQLITE_OK) return -1;
    if (a1) sqlite3_bind_text(s, 1, a1, -1, SQLITE_TRANSIENT);
    if (a2) sqlite3_bind_text(s, 2, a2, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
    return n;
}
/* in wie vielen Zeilen der Tabelle kommt die Kennung irgendwo vor (jede Spalte als Text)? -1 = Fehler */
static long long kz_vorkommen(sqlite3 *h, const char *schema, const char *tab, const char *titel)
{
    char *q = sqlite3_mprintf("SELECT name FROM pragma_table_info(%Q, %Q)", tab, schema);
    sqlite3_stmt *s = NULL;
    if (!q || sqlite3_prepare_v2(h, q, -1, &s, NULL) != SQLITE_OK) { sqlite3_free(q); return -1; }
    sqlite3_free(q);
    char *bed = NULL;
    while (sqlite3_step(s) == SQLITE_ROW) {
        const char *sp = (const char *)sqlite3_column_text(s, 0);
        if (!sp) continue;
        char *t = sqlite3_mprintf("%s%sinstr(CAST(\"%w\" AS TEXT), ?1) > 0", bed ? bed : "", bed ? " OR " : "", sp);
        sqlite3_free(bed); bed = t;
        if (!bed) break;
    }
    sqlite3_finalize(s);
    if (!bed) return 0;
    char *z = sqlite3_mprintf("SELECT count(*) FROM \"%w\".\"%w\" WHERE %s", schema, tab, bed);
    sqlite3_free(bed);
    long long n = z ? kz_zahl(h, z, titel, NULL) : -1;
    sqlite3_free(z);
    return n;
}
/* PRAGMA ai.quick_check: 1 = genau eine Zeile "ok" (wie sd_heil, für die dazu geöffnete appinfo.db) */
static int kz_heil_ai(sqlite3 *h, char *grund, size_t gl)
{
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(h, "PRAGMA ai.quick_check", -1, &st, NULL) != SQLITE_OK) { snprintf(grund, gl, "Prüfung von appinfo.db nicht möglich (%s) - nichts geändert", sqlite3_errmsg(h)); return 0; }
    int zeilen = 0, ok = 0, rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) { const char *z = (const char *)sqlite3_column_text(st, 0); if (++zeilen == 1 && z && !strcmp(z, "ok")) ok = 1; }
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE || zeilen != 1 || !ok) { snprintf(grund, gl, "appinfo.db ist beschädigt (quick_check) - nichts geändert"); return 0; }
    return 1;
}
static int kz_name_ok(const char *t)   /* Tabellennamen, die hier zusammengesetzt werden: nur Buchstaben, Ziffern, _ */
{
    if (!t || !t[0]) return 0;
    for (const char *p = t; *p; p++) if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) return 0;
    return 1;
}
int kachel_db_austragen(const char *ordner, const char *titel, int nur_pruefen, int *zeilen, char *grund, size_t gl, char *verweis, size_t vl)
{
    *zeilen = 0;
    if (gl) grund[0] = 0;
    if (verweis && vl) verweis[0] = 0;
    if (!ordner || !titel || strlen(titel) != 9) { snprintf(grund, gl, "Aufruf unvollständig - nichts geändert"); return -2; }
    for (int i = 0; i < 9; i++) if (i < 4 ? !(titel[i] >= 'A' && titel[i] <= 'Z') : !(titel[i] >= '0' && titel[i] <= '9')) { snprintf(grund, gl, "keine Titel-Kennung - nichts geändert"); return -2; }
    if (!strncmp(titel, "NPXS", 4)) { snprintf(grund, gl, "Systemkennung - nichts geändert"); return -1; }
    char pa[600], pi[600];
    snprintf(pa, sizeof pa, "%s/app.db", ordner); snprintf(pi, sizeof pi, "%s/appinfo.db", ordner);
    struct stat st;
    if (stat(pa, &st) != 0 || !S_ISREG(st.st_mode) || stat(pi, &st) != 0 || !S_ISREG(st.st_mode)) { snprintf(grund, gl, "app.db oder appinfo.db nicht gefunden - nichts geändert"); return -2; }
    sqlite3 *h = NULL;
    if (sqlite3_open_v2(pa, &h, nur_pruefen ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "app.db nicht zu öffnen (%s)", h ? sqlite3_errmsg(h) : "kein Speicher"); sqlite3_close(h); return -2; }
    sqlite3_busy_timeout(h, 3000);
    {   sqlite3_stmt *s = NULL;
        int ok = sqlite3_prepare_v2(h, "ATTACH DATABASE ?1 AS ai", -1, &s, NULL) == SQLITE_OK;
        if (ok) { sqlite3_bind_text(s, 1, pi, -1, SQLITE_TRANSIENT); ok = sqlite3_step(s) == SQLITE_DONE; }
        sqlite3_finalize(s);
        if (!ok) { snprintf(grund, gl, "appinfo.db nicht dazu zu öffnen (%s)", sqlite3_errmsg(h)); sqlite3_close(h); return -2; }
    }
    if (sqlite3_exec(h, nur_pruefen ? "BEGIN" : "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "Sonys Datenbank ist gerade belegt (%s) - nichts geändert", sqlite3_errmsg(h)); sqlite3_close(h); return -1; }
    int erg = -1;
    kz_stelle stellen[64]; int ns = 0;
    char *lc = NULL;
    long long fk_vor = -1;
    if (!sd_heil(h, "app.db", grund, gl)) goto zurueck;
    if (!kz_heil_ai(h, grund, gl)) goto zurueck;
    /* 3. Bedingungen */
    if (kz_spalte_da(h, "main", "tbl_contentinfo", "titleId") != 1 || kz_spalte_da(h, "main", "tbl_contentinfo", "uninstallable") != 1
        || kz_spalte_da(h, "main", "tbl_contentinfo", "localConceptId") != 1 || kz_spalte_da(h, "ai", "tbl_appinfo", "titleId") != 1) {
        snprintf(grund, gl, "Aufbau der Datenbank unbekannt - nichts geändert"); goto zurueck; }
    if (kz_zahl(h, "SELECT count(*) FROM main.tbl_contentinfo WHERE titleId = ?1", titel, NULL) != 1) {
        snprintf(grund, gl, "%s steht nicht genau einmal in Sonys Titel-Liste - nichts geändert", titel); goto zurueck; }
    if (kz_zahl(h, "SELECT count(*) FROM main.tbl_contentinfo WHERE titleId = ?1 AND uninstallable = 1", titel, NULL) != 1) {
        snprintf(grund, gl, "%s ist laut Sony nicht deinstallierbar - nichts geändert", titel); goto zurueck; }
    if (kz_spalte_da(h, "main", "tbl_contentinfo", "contentId") != 1
        || kz_zahl(h, "SELECT count(*) FROM main.tbl_contentinfo WHERE titleId = ?1 AND (contentId IS NULL OR CAST(contentId AS TEXT) = '')", titel, NULL) != 1) {
        snprintf(grund, gl, "%s ist mit einem Sony-Paket installiert (contentId) - das entfernt nur Sony, nichts geändert", titel); goto zurueck; }
    if (verweis && vl && kz_spalte_da(h, "main", "tbl_contentinfo", "pprDeeplinkUri") == 1) {
        sqlite3_stmt *s = NULL;
        if (sqlite3_prepare_v2(h, "SELECT CAST(pprDeeplinkUri AS TEXT) FROM main.tbl_contentinfo WHERE titleId = ?1", -1, &s, NULL) == SQLITE_OK) {
            sqlite3_bind_text(s, 1, titel, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0)) snprintf(verweis, vl, "%s", (const char *)sqlite3_column_text(s, 0));
        }
        sqlite3_finalize(s);
    }
    {   sqlite3_stmt *s = NULL;
        if (sqlite3_prepare_v2(h, "SELECT CAST(localConceptId AS TEXT) FROM main.tbl_contentinfo WHERE titleId = ?1", -1, &s, NULL) == SQLITE_OK) {
            sqlite3_bind_text(s, 1, titel, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0)) lc = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(s, 0));
        }
        sqlite3_finalize(s);
    }
    if (!lc || !lc[0] || kz_zahl(h, "SELECT count(*) FROM main.tbl_contentinfo WHERE localConceptId = ?1 AND titleId <> ?2", lc, titel) != 0) {
        snprintf(grund, gl, "die Spielgruppe von %s gehört auch anderen Titeln (oder fehlt) - nichts geändert", titel); goto zurueck; }
    if (kz_zahl(h, "SELECT (SELECT count(*) FROM main.sqlite_master WHERE type = 'trigger' AND upper(sql) LIKE '%DELETE%')"
                   " + (SELECT count(*) FROM ai.sqlite_master WHERE type = 'trigger' AND upper(sql) LIKE '%DELETE%')", NULL, NULL) != 0) {
        snprintf(grund, gl, "Sonys Datenbank hat Auslöser beim Löschen - Auswirkung unklar, nichts geändert"); goto zurueck; }
    fk_vor = kz_zahl(h, "SELECT (SELECT count(*) FROM pragma_foreign_key_check(NULL,'main')) + (SELECT count(*) FROM pragma_foreign_key_check(NULL,'ai'))", NULL, NULL);
    if (fk_vor < 0) {   /* Codex-Prüfung 206 (2): ohne Verweis-Prüfung vorher gibt es nachher keinen Vergleich */
        snprintf(grund, gl, "Verweise zwischen Tabellen ließen sich nicht prüfen - nichts geändert"); goto zurueck; }
    /* bekannte Stellen: wo eine Kachel steht (Sony bzw. so wie TabShifter sie anlegt und PR #21 sie entfernt) */
    stellen[ns++] = (kz_stelle){ "main", "tbl_contentinfo", "titleId = ?1", 0, 0, 0 };
    if (kz_spalte_da(h, "main", "tbl_conceptmetadata", "localConceptId") == 1) stellen[ns++] = (kz_stelle){ "main", "tbl_conceptmetadata", "localConceptId = ?1", 1, 0, 0 };
    stellen[ns++] = (kz_stelle){ "ai", "tbl_appinfo", "titleId = ?1", 0, 0, 0 };
    if (kz_spalte_da(h, "ai", "tbl_conceptinfo", "local_concept_id") == 1) stellen[ns++] = (kz_stelle){ "ai", "tbl_conceptinfo", "local_concept_id = ?1", 1, 0, 0 };
    {   sqlite3_stmt *s = NULL;
        if (sqlite3_prepare_v2(h, "SELECT name FROM main.sqlite_master WHERE type = 'table' AND (name GLOB 'tbl_iconinfo_*' OR name GLOB 'tbl_concepticoninfo_*'"
                                  " OR name GLOB 'tbl_group_*' OR name GLOB 'tbl_info_*') ORDER BY name", -1, &s, NULL) != SQLITE_OK) {
            snprintf(grund, gl, "Tabellen nicht lesbar (%s) - nichts geändert", sqlite3_errmsg(h)); goto zurueck; }
        int ok = 1;
        while (ok && sqlite3_step(s) == SQLITE_ROW && ns < 60) {
            const char *t = (const char *)sqlite3_column_text(s, 0);
            if (!kz_name_ok(t)) { ok = 0; break; }
            const char *wo = NULL; int art = 0;
            if (!strncmp(t, "tbl_iconinfo_", 13)) { if (kz_spalte_da(h, "main", t, "titleId") != 1) ok = 0; else wo = "titleId = ?1"; }
            else if (!strncmp(t, "tbl_concepticoninfo_", 20)) { if (kz_spalte_da(h, "main", t, "primaryTitleId") != 1 || kz_spalte_da(h, "main", t, "localConceptId") != 1) ok = 0;
                                                                else { wo = "primaryTitleId = ?1 OR localConceptId = ?2"; art = 2; } }
            else if (!strncmp(t, "tbl_group_", 10)) { if (kz_spalte_da(h, "main", t, "itemId") == 1) wo = "itemId = ?1"; }
            else if (!strncmp(t, "tbl_info_", 9)) { if (kz_spalte_da(h, "main", t, "key") == 1) wo = "key = 'nsx_content_flag_' || ?1"; }
            if (ok && wo) stellen[ns++] = (kz_stelle){ "main", sqlite3_mprintf("%s", t), wo, art, 0, 1 };
        }
        sqlite3_finalize(s);
        if (!ok) { snprintf(grund, gl, "Aufbau einer Benutzertabelle unbekannt - nichts geändert"); goto zurueck; }
    }
    /* zählen */
    long long summe = 0;
    for (int i = 0; i < ns; i++) {
        char *q = sqlite3_mprintf("SELECT count(*) FROM \"%w\".\"%w\" WHERE %s", stellen[i].schema, stellen[i].tabelle, stellen[i].wo);
        stellen[i].n = q ? kz_zahl(h, q, stellen[i].art == 1 ? lc : titel, stellen[i].art == 2 ? lc : NULL) : -1;
        sqlite3_free(q);
        if (stellen[i].n < 0) { snprintf(grund, gl, "%s ließ sich nicht zählen (%s) - nichts geändert", stellen[i].tabelle, sqlite3_errmsg(h)); goto zurueck; }
        summe += stellen[i].n;
    }
    /* 4. jede Zeile mit der Kennung muss zu einer bekannten Stelle gehören */
    for (int sc = 0; sc < 2; sc++) {
        const char *schema = sc ? "ai" : "main";
        char *q = sqlite3_mprintf("SELECT name FROM \"%w\".sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%%'", schema);
        sqlite3_stmt *s = NULL;
        if (!q || sqlite3_prepare_v2(h, q, -1, &s, NULL) != SQLITE_OK) { sqlite3_free(q); snprintf(grund, gl, "Tabellen nicht lesbar - nichts geändert"); goto zurueck; }
        sqlite3_free(q);
        int ok = 1;
        while (ok && sqlite3_step(s) == SQLITE_ROW) {
            const char *t = (const char *)sqlite3_column_text(s, 0);
            if (!t) continue;
            long long v = kz_vorkommen(h, schema, t, titel);
            if (v < 0) { snprintf(grund, gl, "%s nicht lesbar - nichts geändert", t); ok = 0; break; }
            if (v == 0) continue;
            long long bekannt = -1;
            for (int i = 0; i < ns; i++) if (!strcmp(stellen[i].schema, schema) && !strcmp(stellen[i].tabelle, t)) bekannt = stellen[i].n;
            if (bekannt != v) { snprintf(grund, gl, "%s steht auch an einer unbekannten Stelle (%s.%s: %lld Zeilen) - nichts geändert", titel, schema, t, v); ok = 0; }
        }
        sqlite3_finalize(s);
        if (!ok) goto zurueck;
    }
    if (summe == 0) { snprintf(grund, gl, "%s steht in keiner Tabelle - nichts geändert", titel); goto zurueck; }
    if (nur_pruefen) { erg = 0; *zeilen = (int)summe; snprintf(grund, gl, "austragbar: %lld Zeilen", summe); goto zurueck; }
    /* 5. löschen */
    for (int i = 0; i < ns; i++) {
        if (stellen[i].n == 0) continue;
        char *q = sqlite3_mprintf("DELETE FROM \"%w\".\"%w\" WHERE %s", stellen[i].schema, stellen[i].tabelle, stellen[i].wo);
        sqlite3_stmt *s = NULL;
        int ok = q && sqlite3_prepare_v2(h, q, -1, &s, NULL) == SQLITE_OK;
        sqlite3_free(q);
        if (ok) { sqlite3_bind_text(s, 1, stellen[i].art == 1 ? lc : titel, -1, SQLITE_TRANSIENT); if (stellen[i].art == 2) sqlite3_bind_text(s, 2, lc, -1, SQLITE_TRANSIENT);
                  ok = sqlite3_step(s) == SQLITE_DONE && sqlite3_changes(h) == stellen[i].n; }
        sqlite3_finalize(s);
        if (!ok) { snprintf(grund, gl, "Austragen aus %s unvollständig (%s) - zurückgenommen, nichts geändert", stellen[i].tabelle, sqlite3_errmsg(h)); goto zurueck; }
    }
    /* danach: nirgends mehr, heil, Fremdschlüssel nicht schlechter */
    for (int sc = 0; sc < 2; sc++) {
        const char *schema = sc ? "ai" : "main";
        char *q = sqlite3_mprintf("SELECT name FROM \"%w\".sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%%'", schema);
        sqlite3_stmt *s = NULL; int ok = 1;
        if (!q || sqlite3_prepare_v2(h, q, -1, &s, NULL) != SQLITE_OK) ok = 0;
        sqlite3_free(q);
        while (ok && sqlite3_step(s) == SQLITE_ROW) { const char *t = (const char *)sqlite3_column_text(s, 0); if (t && kz_vorkommen(h, schema, t, titel) != 0) ok = 0; }
        sqlite3_finalize(s);
        if (!ok) { snprintf(grund, gl, "nach dem Austragen steht %s noch in Sonys Datenbank - zurückgenommen, nichts geändert", titel); goto zurueck; }
    }
    if (!sd_heil(h, "app.db", grund, gl)) goto zurueck;
    if (!kz_heil_ai(h, grund, gl)) goto zurueck;
    {   /* Codex-Prüfung 204 (2): ließen sich die Verweise vorher prüfen und nachher nicht mehr, wird ebenfalls zurückgenommen */
        long long fk_nach = kz_zahl(h, "SELECT (SELECT count(*) FROM pragma_foreign_key_check(NULL,'main')) + (SELECT count(*) FROM pragma_foreign_key_check(NULL,'ai'))", NULL, NULL);
        if (fk_nach > fk_vor || (fk_vor >= 0 && fk_nach < 0)) {
            snprintf(grund, gl, "Verweise zwischen Tabellen wären danach kaputt - zurückgenommen, nichts geändert"); goto zurueck; }
    }
    if (sqlite3_exec(h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        snprintf(grund, gl, "Sonys Datenbank ließ sich nicht abschließen (%s) - zurückgenommen, nichts geändert", sqlite3_errmsg(h)); goto zurueck; }
    erg = 0; *zeilen = (int)summe;
    {   size_t l = (size_t)snprintf(grund, gl, "ausgetragen:");
        for (int i = 0; i < ns && l < gl; i++) if (stellen[i].n) l += (size_t)snprintf(grund + l, gl - l, " %s.%s %lld", stellen[i].schema, stellen[i].tabelle, stellen[i].n); }
    goto ende;
zurueck:
    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
ende:
    for (int i = 0; i < ns; i++) if (stellen[i].frei) sqlite3_free((void *)stellen[i].tabelle);
    sqlite3_free(lc);
    sqlite3_close(h);
    return erg;
}
