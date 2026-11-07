/* PS5 Cleanup 1.0.8 - Spieldatenbanken (app.db, appinfo.db) nur lesen und prüfen - siehe db_lesen.c
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 */
#ifndef DB_LESEN_H
#define DB_LESEN_H
#include "../kern/kern.h"

/* quelle: Ordner mit den Datenbanken (PS5: /system_data/priv/mms). Nur lesen: app.db und appinfo.db samt -wal/-journal je
 * zweimal gleich gelesen und mit SQLite im Speicher geprüft (integrity_check, Kopie über die Backup-Schnittstelle).
 * Es wird nichts geschrieben. true = beide ok. *bericht: Zeilen fürs Protokoll (bei false am Ende
 * "Datenbank-Prüfung FEHLGESCHLAGEN: <Grund>"). */
bool db_beide_pruefen(arena *a, const char *quelle, const char **bericht);
/* NEU 1.0.7 (da_Chris86 01.11.2026, Reste ohne Kachel): steht die Kennung id in app.db (quelle = Ordner mit app.db)? Nur lesen:
 * app.db samt -wal/-journal zweimal gleich gelesen, mit SQLite im Speicher geprüft, dann gesucht (db_titel_suchen).
 * 1 = ja, 0 = nein, -1 = nicht prüfbar (*grund). */
int db_kennung_in_appdb(arena *a, const char *quelle, const char *id, const char **grund);
/* NEU 1.0.7 (PS5-Test 02.11.2026): appinfo.db (quelle = Ordner) samt -wal/-journal NUR LESEN, im Speicher prüfen und jede Zeile
 * INSTALL_DIR_SAVEDATA / SERVICE_ID_ADDCONT_ADD_<n> melden (db_appinfo_zeilen). Ein Versuch - die Suche soll nicht warten.
 * Anzahl Zeilen, -1 = nicht lesbar (*grund). */
int db_appinfo_angaben(arena *a, const char *quelle, void (*zeile)(void *ctx, const char *titel, const char *feld, const char *wert),
                       void *ctx, const char **grund);
/* NEU 1.0.8 (PS5-Test 07.11.2026): Sonys Spielstand-Liste (datei = Pfad der savedata.db, PS5: /system_data/savedata/<Benutzer>/db/user/
 * savedata.db) samt -wal/-journal NUR LESEN, im Speicher prüfen und je Spielstand (Kennung, "SPIELSTAND_TITEL", Titel) melden
 * (db_spielstand_zeilen). Ein Versuch. Anzahl Zeilen, -1 = nicht lesbar (*grund). */
int db_spielstand_angaben(arena *a, const char *datei, void (*zeile)(void *ctx, const char *titel, const char *feld, const char *wert),
                          void *ctx, const char **grund);

#endif
