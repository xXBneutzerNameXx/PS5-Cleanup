/* PS5 Cleanup - Protokoll-Datei auf dem USB-Stick für den Hintergrundteil (Selbstentfernung)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * da_Chris86 31.10.2026 19:28: Die Oberfläche speichert das Protokoll kurz vor der Selbstentfernung auf den USB-Stick;
 * der Hintergrundteil hängt seine Schlusszeilen dort an. Eigene, kleine Datei, damit die Bau-Prüfung genau sehen kann:
 * hier steht der EINZIGE schreibende Aufruf des Hintergrundteils (open ohne Anlegen + write), sonst nichts.
 */
#pragma once
#include <stddef.h>
/* Nur ein Pfad der Form /mnt/usbN/ps5cleanup_protokoll_<Ziffern _ ->.txt wird angenommen. 1 = angenommen, 0 = abgelehnt. */
int  sw_protokoll_setzen(const char *pfad, size_t n);
/* Zeile anhängen ("HH:MM:SS Selbstentfernung: <t>"). Nur anhängen, nie anlegen, keiner Verknüpfung folgen.
 * Simulation (-DSIMULATION): tut nichts. */
void sw_protokoll_zeile(const char *t);
