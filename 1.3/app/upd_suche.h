/* PS5 Cleanup - Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * upd_suche.h - NEU 1.3 (Update-Probe 1.1 -> 1.2 von da_Chris86, 18.11.2026): "Ja, ersetzen" hat die alte Programmdatei nicht gelöscht,
 * wenn das Update vor dem ersten Nachschauen geholt wurde - die alte Datei kannte die App erst nach dem Nachschauen. Hier wird sie auch
 * ohne Nachschauen gefunden, und für beide Wege gilt dieselbe Regel, welche Datei als "die alte Version" ersetzt werden darf.
 * Nur lesen. Kein Kern nötig (am PC einzeln prüfbar: test/kern/test_upd_suche.c).
 */
#ifndef UPD_SUCHE_H
#define UPD_SUCHE_H
#include <stddef.h>

/* Welche der Kandidaten (Pfade von Dateien, die schon als PS5 Cleanup erkannt sind) ist die alte Version, die "Ja, ersetzen" löschen darf?
 * - nur Dateien auf .elf; nie etwas unter einem Ordner "pldmgr" (diese Kopien verwaltet der Payload Manager selbst)
 * - zuerst der Name ps5cleanup_v<version>.elf (Groß-/Kleinschreibung egal), dabei ein USB-Laufwerk (/mnt/usb...) vor allem anderen
 * - sonst: gibt es genau eine Datei ohne Versionsnummer im Namen (z. B. ps5cleanup.elf), diese
 * - eine Datei mit einer ANDEREN Versionsnummer im Namen wird nie gewählt (sie könnte neuer sein); im Zweifel keine
 * Rückgabe: Index in pfade oder -1 (keine eindeutige). */
int upd_alte_waehlen(const char *const *pfade, int n, const char *version);

/* Sucht PS5 Cleanup in den Ordnern (nur oberste Ebene, nur lesen): normale Datei (keine Verknüpfung), Name endet auf .elf und enthält
 * "cleanup" (Groß-/Kleinschreibung egal), beginnt mit 7F 'E' 'L' 'F' und enthält die Kennung, höchstens UPD_SUCHE_MAX_BYTE groß.
 * Doppelt genannte Ordner zählen einmal. Aus den Treffern wählt upd_alte_waehlen. Rückgabe: Zahl der Treffer;
 * aus = gewählte Datei oder "" (keine eindeutige). */
int upd_eigene_elf_suchen(const char *const *ordner, int n, const char *kennung, const char *version, char *aus, size_t an);

/* NEU 1.3 (da_Chris86 20.11.2026: "Bekommt der Nutzer eine Info, was in 1.2 neu ist?" - bis 1.2 stand nur "Update x verfügbar" da):
 * die wichtigsten Punkte aus dem Text der Release-Notes bei GitHub (Feld "body", Markdown). Nur Text, kein Netz, kein Zeichnen.
 * art UPD_NEU_TITEL: die Punkte der ersten Aufzählung ("- ..." oder "* ...") - je Punkt seine fette Überschrift am Anfang
 *                    ("- **Behoben - Firmware 13.x:** ..." -> "Behoben - Firmware 13.x"), ohne Überschrift der ganze Punkt
 * art UPD_NEU_GANZ:  dieselben Punkte, aber ganz (für das Protokoll)
 * art UPD_NEU_EN:    der englische Kurztext ("**In short:** ...") in Sätzen - ohne ihn keine Punkte
 * Markdown-Zeichen (**, __, `), Links [Text](Adresse) -> Text und Emojis (eine Schrift hat sie nicht) fallen weg.
 * Rückgabe: Zahl der Punkte in aus (höchstens max, jeder höchstens UPD_NEU_LAENGE - 1 Bytes, an einer Zeichengrenze gekürzt). */
#define UPD_NEU_LAENGE 400
enum { UPD_NEU_TITEL = 0, UPD_NEU_EN = 1, UPD_NEU_GANZ = 2 };
int upd_neu_punkte(const char *text, int art, char (*aus)[UPD_NEU_LAENGE], int max);

#endif
