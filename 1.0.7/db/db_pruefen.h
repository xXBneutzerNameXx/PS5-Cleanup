/* PS5 Cleanup 1.0.7 - SQLite-Prüfung der Spieldatenbanken, ganz im Arbeitsspeicher
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (SQLite selbst: gemeinfrei, siehe db/LIESMICH.txt)
 *
 * Auftrag da_Chris86 31.10.2026 21:50 (Punkt 3): die tatsächliche SQLite-Konsistenz prüfen, Journal- und WAL-Dateien
 * berücksichtigen, eine konsistente Sicherung über die SQLite-Backup-Schnittstelle untersuchen. Eine einfache Dateikopie
 * gilt NICHT als garantiert wiederherstellbar.
 *
 * Vorgehen (keine Datei wird geöffnet, gelesen oder geschrieben - nur die übergebenen Bytes im Speicher):
 *  1. Rollback-Journal: beginnt eine nicht leere -journal-Datei mit der Journal-Kennung, war die Konsole mitten im
 *     Schreiben ("heißes Journal") -> Ergebnis DB_SCHREIBT (später nochmal versuchen), die Kopie wird nicht verwendet.
 *  2. WAL: gültige, abgeschlossene Einträge (Prüfsummen und Salz wie im SQLite-Dateiformat) werden auf die Seiten der
 *     Hauptdatei gelegt; unvollständige oder fremde Einträge am Ende fallen weg - wie SQLite es selbst macht.
 *  3. SQLite öffnet das Ergebnis als Speicher-Datenbank (sqlite3_deserialize) und führt PRAGMA integrity_check aus.
 *  4. Über die SQLite-Backup-Schnittstelle (sqlite3_backup_*) entsteht eine zweite Speicher-Datenbank; auch sie muss
 *     integrity_check "ok" liefern. Ihr Inhalt (bild) ist die geprüfte Kopie.
 * SQLite ist so übersetzt (db_sqlite.c), dass es gar keine Dateien kennt: SQLITE_OS_OTHER mit einem eigenen Dateisystem-
 * Ersatz, der jedes Öffnen ablehnt.
 */
#ifndef DB_PRUEFEN_H
#define DB_PRUEFEN_H
#include <stddef.h>

enum { DB_OK = 0, DB_FEHLER = 1, DB_SCHREIBT = 2 };

typedef struct {
    int ergebnis;                 /* DB_OK / DB_FEHLER / DB_SCHREIBT */
    char grund[400];              /* bei Fehler: was nicht stimmt */
    int modus;                    /* 1 = Rollback-Journal, 2 = WAL (Kopf der Hauptdatei, Byte 18) */
    unsigned seitengroesse;
    unsigned long seiten;         /* Seiten der geprüften Kopie */
    int wal_eintraege;            /* gültige WAL-Einträge, die eingerechnet wurden */
    int wal_verworfen;            /* WAL-Einträge nach dem letzten Abschluss oder ungültig (nicht eingerechnet) */
    unsigned char *bild;          /* geprüfte Kopie (malloc) oder NULL - freigeben mit db_ergebnis_frei */
    size_t bild_n;
} db_ergebnis;

/* d/n: Hauptdatei; wal/wn und jr/jn: Begleitdateien oder NULL/0. mit_bild: geprüfte Kopie zurückgeben. */
int  db_pruefen(const unsigned char *d, size_t n, const unsigned char *wal, size_t wn,
                const unsigned char *jr, size_t jn, int mit_bild, db_ergebnis *e);
void db_ergebnis_frei(db_ergebnis *e);
const char *db_sqlite_version(void);   /* z. B. "3.54.1" */
/* NEU 1.0.7 (da_Chris86 01.11.2026, Reste ohne Kachel): kommt die Kennung id in einer Tabelle mit einer Spalte "titleId" vor?
 * Ganz im Speicher (bild = geprüfte Kopie aus db_pruefen). 1 = ja, 0 = nein, -1 = Fehler (grund). */
int db_titel_suchen(const unsigned char *bild, size_t n, const char *id, char *grund, size_t gl);
/* NEU 1.0.7 (PS5-Test 02.11.2026, GTA V / Red Dead Redemption 2): aus appinfo.db (bild = geprüfte Kopie) jede Zeile der Tabelle
 * tbl_appinfo mit dem Feld INSTALL_DIR_SAVEDATA oder SERVICE_ID_ADDCONT_ADD_<n> melden (Wert als Text, NULL -> ""). Ganz im Speicher,
 * nur lesen. Rückgabe: Anzahl Zeilen, -1 = Fehler (grund). */
int db_appinfo_zeilen(const unsigned char *bild, size_t n, void (*zeile)(void *ctx, const char *titel, const char *feld, const char *wert),
                      void *ctx, char *grund, size_t gl);

#endif
