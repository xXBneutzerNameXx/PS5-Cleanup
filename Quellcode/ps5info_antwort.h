/* ps5info_antwort.h - prueft die Antwort des Laufwerks-Treibers /dev/ddd.
 * Eigene Datei, damit die Pruefung auch am PC getestet werden kann.
 * Lizenz: GPL-3.0 (siehe LICENSE_GPL-3.0_ps5info.txt). */
#ifndef PS5INFO_ANTWORT_H
#define PS5INFO_ANTWORT_H
#include <stdint.h>

/* Startwert des Ergebnisses: kommt nicht als echter Status vor. Bleibt er
 * nach der Abfrage stehen, hat der Treiber nichts zurueckgeschrieben. */
#define DDD_UNGUELTIG 0xFFFFFFFFu
/* hoechster bekannter Statuswert (0..6 nach earthonion/ddd_status) */
#define DDD_HOECHSTER 6u

/* Liefert den Statuswert 0..6 nur bei einer vollstaendigen, gueltigen Antwort:
 * ioctl erfolgreich (r == 0), Statusfeld der Antwort 0, Ergebnis wirklich
 * zurueckgeschrieben und im bekannten Bereich. Sonst -1 = nicht auslesbar.
 * Keine Abhaengigkeit von Modell oder Firmware. */
static int ddd_antwort_pruefen(int r, uint32_t feld, uint32_t aus) {
  if (r != 0) {
    return -1;
  }
  if (feld != 0) {
    return -1;
  }
  if (aus == DDD_UNGUELTIG || aus > DDD_HOECHSTER) {
    return -1;
  }
  return (int)aus;
}
#endif
