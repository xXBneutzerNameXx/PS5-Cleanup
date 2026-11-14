/* PS5 Cleanup - Spielstand-Einträge aus Sonys Spielstand-Liste (savedata.db) austragen - siehe spielstand_db.c
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 */
#ifndef SPIELSTAND_DB_H
#define SPIELSTAND_DB_H
#include <stddef.h>

/* db: Pfad der savedata.db, titel: title_id (= Name des Spielstand-Ordners). Alles oder nichts, vorher Prüfung (quick_check,
 * Aufbau, Anzahl); seit 02.11.2026 ohne Sicherung (da_Chris86: auf dem USB-Stick nur das Protokoll).
 * 0 = ausgetragen (*zeilen = so viele Einträge, auch 0 - dann wurde nichts geändert),
 * -1 = nichts geändert (*grund sagt warum). */
int spielstand_db_austragen(const char *db, const char *titel, int *zeilen, char *grund, size_t gl);

/* NEU 1.0.7 (01.11.2026, Red Dead Redemption 2): kaputte Zusatzinhalt-Angaben (SERVICE_ID_ADDCONT_ADD_n) eines Titels in Sonys
 * appinfo.db auf 0 setzen - nur wenn welche kaputt sind; alles oder nichts, ohne Sicherung (seit 02.11.2026).
 * 0 = ok (*zeilen = repariert, 0 = nichts zu tun), -1 = kaputt, aber nichts geändert, -2 = Prüfung nicht möglich (*grund). */
int appinfo_zusatz_reparieren(const char *db, const char *titel, int *zeilen, char *grund, size_t gl);

/* NEU 1.0.10 (12.11.2026 21:40): direkt eingetragene, verwaiste Kachel <titel> aus app.db und appinfo.db im Ordner <ordner> austragen -
 * alles oder nichts über beide Dateien, nur bekannte Stellen, vorher und nachher geprüft (siehe spielstand_db.c). nur_pruefen = 1: nichts
 * ändern, nur sagen, ob es ginge. 0 = ausgetragen/austragbar (*zeilen), -1 = nichts geändert (*grund), -2 = Prüfung nicht möglich. */
int kachel_db_austragen(const char *ordner, const char *titel, int nur_pruefen, int *zeilen, char *grund, size_t gl, char *verweis, size_t vl);

#endif
