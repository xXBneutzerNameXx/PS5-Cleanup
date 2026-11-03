/* PS5 Cleanup 1.0.7 - Sicherung der Spieldatenbanken (app.db, appinfo.db) vor einer Bereinigung - siehe db_sicherung.c
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 */
#ifndef DB_SICHERUNG_H
#define DB_SICHERUNG_H
#include "../kern/kern.h"

/* quelle: Ordner mit den Datenbanken (PS5: /system_data/priv/mms), ziel: Ordner auf dem USB-Stick.
 * schreiben = false: nur lesen und prüfen (Test-ELF/Simulation) - dann wird nichts geschrieben, ziel darf NULL sein.
 * true = alles gelesen, mit SQLite geprüft (und bei schreiben: auf den Stick geschrieben und zurückgelesen).
 * *bericht: Zeilen fürs Protokoll; *ablage: Anfang der Dateinamen auf dem Stick (oder ""). */
bool db_sichern(arena *a, const char *quelle, const char *ziel, const char *fw, const char *modell, bool schreiben,
                const char **bericht, const char **ablage);
/* Stick tauglich? Ordner da und Platz frei (statvfs, nur lesen). bytes = so viel wird mindestens gebraucht. */
bool db_stick_platz(const char *ziel, long long bytes, long long *frei);
/* NEU 1.0.7 (da_Chris86 01.11.2026, Reste ohne Kachel): steht die Kennung id in app.db (quelle = Ordner mit app.db)? Nur lesen:
 * app.db samt -wal/-journal zweimal gleich gelesen, mit SQLite im Speicher geprüft, dann gesucht (db_titel_suchen).
 * 1 = ja, 0 = nein, -1 = nicht prüfbar (*grund). */
int db_kennung_in_appdb(arena *a, const char *quelle, const char *id, const char **grund);
/* NEU 1.0.7 (PS5-Test 02.11.2026): appinfo.db (quelle = Ordner) samt -wal/-journal NUR LESEN, im Speicher prüfen und jede Zeile
 * INSTALL_DIR_SAVEDATA / SERVICE_ID_ADDCONT_ADD_<n> melden (db_appinfo_zeilen). Ein Versuch - die Suche soll nicht warten.
 * Anzahl Zeilen, -1 = nicht lesbar (*grund). */
int db_appinfo_angaben(arena *a, const char *quelle, void (*zeile)(void *ctx, const char *titel, const char *feld, const char *wert),
                       void *ctx, const char **grund);

#endif
