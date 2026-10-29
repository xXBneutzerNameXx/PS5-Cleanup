/* PS5 Cleanup 1.0.6 - start_hbldr.h (abgeleitet aus websrv src/ps5/hbldr.h, John Törnblom, GPL-3.0-or-later) */
#pragma once
#include <stdint.h>
#include <unistd.h>
#include <stddef.h>
/* NEU 1.0.6: eigenen Startordner /system_ex/app/PSCL00002 bereitstellen (startordner.h). 0 = bereit */
int start_startordner(char *text, size_t text_len, int *umgehaengt);
/* Startet die Platzhalter-App als Vordergrund-App und lädt die ELF aus dem Speicher hinein.
 * Beendet vorher eine laufende App/ein laufendes Spiel (wie websrv). -1 = Fehler. */
pid_t start_hbldr_launch(const char* cwd, const char* progname, uint8_t* elf, int stdio,
			 char** argv, char** envp);
/* NEU 1.0.6: Prozess mit diesem Namen suchen (nur lesen). -1 = keiner, -2 = unbekannt (Prozessliste nicht lesbar) */
pid_t start_hbldr_find_pid(const char* name);
