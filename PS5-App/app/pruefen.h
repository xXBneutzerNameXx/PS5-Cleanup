/* PS5 Cleanup 1.0.6 - Prüflauf vor Schritt 10 (nur lesen)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later */
#ifndef APP_PRUEFEN_H
#define APP_PRUEFEN_H
#include "../kern/kern.h"
/* Gibt für einen fertig gesuchten Kern aus (stdout, gleiches Format wie test/ref105 "pruefen"):
 * Schutzentscheidung je Pfad, Größen je Pfad und Eintrag, Auswahl der vollständigen Bereinigung
 * ohne/mit M.2 samt Größen. Löscht nichts, schreibt nichts - ändert nur die Häkchen im Speicher. */
void pruef_ausgabe(kern *k, const char *titel);
#endif
