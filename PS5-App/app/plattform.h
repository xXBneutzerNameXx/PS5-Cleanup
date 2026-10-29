/* PS5 Cleanup 1.0.6 - Oberfläche am Fernseher
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * plattform.h - was die Oberfläche von der Konsole braucht. Auf der PS5:
 * plattform_ps5.c, für die Vorschau am PC: plattform_pc.c.
 */
#ifndef APP_PLATTFORM_H
#define APP_PLATTFORM_H
#include <stdbool.h>
#include "../kern/zugriff.h"

typedef struct {
    char modell[64];        /* "" = unbekannt */
    char firmware[32];      /* "" = unbekannt */
    bool ddd_bekannt;       /* Laufwerks-Kopplung abgefragt? */
    int  ddd;               /* Statuswert 0..6, -1 = nicht auslesbar */
} konsole_info;

void plattform_start(void);                     /* vor SDL_Init */
void plattform_ende(void);                      /* zurück zum Home-Menü */
void plattform_konsole(konsole_info *k);        /* nur lesen */
bool plattform_englisch(void);                  /* Systemsprache nicht Deutsch */
const char *plattform_eigener_ort(void);        /* Ordner, aus dem PS5 Cleanup gestartet wurde ("" = unbekannt) */
zugriff *plattform_zugriff(void);               /* NEU für jede Suche: Simulation über das Dateisystem (nur lesen) */
const char *plattform_name(void);               /* "PS5" oder "PC-Vorschau" */
/* NEU 1.0.6 (da_Chris86 28.10.2026 23:19): Neustart nach der Bereinigung. true = Auftrag angenommen.
 * Nur im Release-Bau wirksam - die Simulation ruft ihn nie auf. */
bool plattform_neustart(void);
/* NEU 1.0.6: Kachel + Startordner entfernt der Hintergrundteil - er übernimmt auch den Neustart (erst nach der Entfernung).
 * true = bestätigt übernommen (dann beendet sich die Oberfläche nur), false = nicht übernommen */
bool plattform_neustart_uebergeben(void);
/* NEU 1.0.6 (da_Chris86 30.10.2026 07:29): muss die Datenbank neu aufgebaut werden, gibt es "Ausschalten" statt "Neu starten".
 * Ganz aus, kein Ruhemodus. Gleiche Regeln wie beim Neustart (Release-Bau, Übergabe an den Hintergrundteil). */
bool plattform_ausschalten(void);
bool plattform_ausschalten_uebergeben(void);

#endif
