/* PS5 Cleanup 1.0.6 - abschließende Selbstentfernung durch den Hintergrundteil von ps5cleanup.elf
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * da_Chris86 29.10.2026 23:08: Die Oberfläche läuft aus dem eigenen Startordner /system_ex/app/PSCL00002 und kann ihn darum
 * nicht selbst entfernen. Sie übergibt den Auftrag an den Hintergrundteil (127.0.0.1:9099); der wartet auf ihr Ende und
 * entfernt dann die eigene Kachel PSCL00001 über Sonys Deinstallation und den eigenen Startordner, stellt den Einhängezustand
 * von /system_ex wieder her, prüft das Ergebnis und beendet sich. Jeder Schritt steht im Kernel-Protokoll.
 * Angefasst wird nur, was eindeutig zu PS5 Cleanup gehört (Markierungsdateien, feste Namen) - nie Sony-Dateien.
 */
#pragma once
#include <stddef.h>

#define SW_KACHEL_ID   "PSCL00001"
/* Kachel PSCL00001 über Sony austragen und prüfen. 1 = entfernt (geprüft), 0 = war nicht da, -1 = nicht/nicht ganz (Grund in g) */
int selbstweg_kachel(char *g, size_t gl);
/* eigenen Startordner entfernen, Einhängezustand prüfen. 1 = entfernt (geprüft), 0 = war nicht da, -1 = Fehler (Grund in g),
 * -2 = /system_ex in dieser Sitzung nicht mehr beschreibbar (nichts verändert; ohne Hinweis, da_Chris86 30.10.2026 00:26) */
int selbstweg_startordner(char *g, size_t gl);
/* ganzer Auftrag. 1 = vollständig entfernt (geprüft); 0 = unvollständig. Text für die Benachrichtigung in erg. */
int selbstweg_ausfuehren(int kachel, int startordner, char *erg, size_t el);
/* Protokollzeile ins Kernel-Protokoll ("PS5 Cleanup Selbstentfernung: ...") */
void selbstweg_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
