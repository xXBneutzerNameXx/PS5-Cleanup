/* PS5 Cleanup 1.0.7 - SQLite für die Prüfung der Spieldatenbanken: OHNE Dateizugriff übersetzt
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (SQLite selbst: gemeinfrei, db/sqlite/sqlite3.c)
 *
 * SQLITE_OS_OTHER: SQLite bringt keinen eigenen Dateisystem-Teil mit (kein open/write/unlink/fsync ...).
 * Der einzige "Dateisystem"-Ersatz unten lehnt jedes Öffnen und Löschen ab; geprüft wird nur im Speicher
 * (sqlite3_deserialize / Speicher-Datenbanken). Temporäre Daten bleiben ebenfalls im Speicher (SQLITE_TEMP_STORE=3).
 */
#define SQLITE_OS_OTHER 1
#define SQLITE_THREADSAFE 0
#define SQLITE_TEMP_STORE 3
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_OMIT_SHARED_CACHE 1
#define SQLITE_OMIT_DEPRECATED 1
#define SQLITE_OMIT_LOCALTIME 1
#define SQLITE_OMIT_PROGRESS_CALLBACK 1
#define SQLITE_DEFAULT_MEMSTATUS 0
#define SQLITE_DQS 0
#define SQLITE_MAX_MMAP_SIZE 0
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "sqlite/sqlite3.c"

#include <time.h>

/* Dateisystem-Ersatz "nurlesen": öffnet nichts, löscht nichts. Zufall/Zeit nur für SQLites interne Zwecke. */
static int nl_oeffnen(sqlite3_vfs *v, sqlite3_filename z, sqlite3_file *f, int fl, int *aus) { (void)v; (void)z; (void)fl; (void)aus; f->pMethods = 0; return SQLITE_CANTOPEN; }
static int nl_loeschen(sqlite3_vfs *v, const char *z, int sync) { (void)v; (void)z; (void)sync; return SQLITE_IOERR_DELETE; }
static int nl_da(sqlite3_vfs *v, const char *z, int fl, int *r) { (void)v; (void)z; (void)fl; *r = 0; return SQLITE_OK; }
static int nl_voll(sqlite3_vfs *v, const char *z, int n, char *aus) { (void)v; sqlite3_snprintf(n, aus, "%s", z); return SQLITE_OK; }
static int nl_zufall(sqlite3_vfs *v, int n, char *aus)
{
    (void)v;
    static unsigned long x = 2463534242UL;
    x ^= (unsigned long)time(NULL);
    for (int i = 0; i < n; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; aus[i] = (char)(x & 0xFF); }
    return n;
}
static int nl_schlafen(sqlite3_vfs *v, int us) { (void)v; return us; }
static int nl_zeit(sqlite3_vfs *v, double *t) { (void)v; *t = 2440587.5 + (double)time(NULL) / 86400.0; return SQLITE_OK; }
static int nl_letzter_fehler(sqlite3_vfs *v, int n, char *z) { (void)v; (void)n; (void)z; return 0; }
static sqlite3_vfs nl_vfs = { 1, sizeof(sqlite3_file), 512, 0, "nurlesen", 0,
                              nl_oeffnen, nl_loeschen, nl_da, nl_voll, 0, 0, 0, 0, nl_zufall, nl_schlafen, nl_zeit, nl_letzter_fehler,
                              0, 0, 0, 0 };
int sqlite3_os_init(void) { return sqlite3_vfs_register(&nl_vfs, 1); }
int sqlite3_os_end(void) { return SQLITE_OK; }
