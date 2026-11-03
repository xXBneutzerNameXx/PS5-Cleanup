/* PS5 Cleanup 1.0.6 - eigene Startbildschirm-Kachel
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * Legt die Web-Kachel PSCL00001 an (wie Payload Manager PLDM00001 / ShadowMountPlus FAKE10101):
 *   /user/app/PSCL00001/sce_sys/{param.json,icon0.png} + Markierung, Eintrag durch die PS5 selbst
 *   (Sonys Installationsdienst - die Systemdatenbank fasst PS5 Cleanup nicht an). Ein Druck auf die Kachel
 *   öffnet http://127.0.0.1:9099/ - darauf antwortet der Hintergrundteil von ps5cleanup.elf und startet die
 *   Vollbild-Oberfläche (Weg A).
 * Eintragen: Einzel-Eintrag (AppInstallTitleDir), wenn es ihn gibt, sonst AppInstallAll (FW 12.00).
 *   da_Chris86 28.10.2026: auch wenn AppInstallAll dabei andere App-Ordner ohne Kachel einträgt.
 *   Kein Eingriff in Systemprozesse (die ShellCore-Brücke von ShadowMountPlus wird nicht übernommen).
 * Entfernen: wie jede Homebrew-Kachel durch den Kern (vollständige Bereinigung: zum Schluss; Auswahl: auf Nachfrage).
 */
#pragma once
#include <stddef.h>

#define KACHEL_ID   "PSCL00001"
#define KACHEL_PORT 9099

enum {
    KACHEL_SCHON_DA       = 1,    /* bereits eingetragen - nichts getan */
    KACHEL_OK             = 0,    /* eben eingetragen */
    KACHEL_FREMD          = -1,   /* Kennung belegt, aber nicht von PS5 Cleanup - nichts angefasst */
    KACHEL_SCHREIBFEHLER  = -4,   /* Kachel-Dateien ließen sich nicht anlegen */
    KACHEL_KEIN_DIENST    = -5,   /* Eintragsfunktion nicht verfügbar */
    KACHEL_SONY_FEHLER    = -6,   /* Sonys Dienst meldet einen Fehler (Code in *sony) */
};

/* Richtet die Kachel ein, falls nötig. text: kurze Meldung (DE) für Benachrichtigung/Protokoll. */
int kachel_einrichten(const unsigned char *icon, size_t icon_len, int *sony, char *text, size_t text_len);

/* Firmware als Dezimalzahl, z. B. 1200 für 12.00; 0 = unbekannt */
unsigned kachel_firmware(void);
