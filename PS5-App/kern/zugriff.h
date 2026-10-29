/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * zugriff.h - die Zugriffsschicht.
 *
 * 1.0.5 erledigt alles über wenige FTP-Grundbefehle (CWD, LIST, RETR, STOR,
 * SITE CHMOD, DELE, RMD, RMDA). Genau diese Grundbefehle bietet die
 * Zugriffsschicht an - mit FTP-artigen Antworten (Code + Text), damit die
 * Sitzung (sitzung.c) und der Kern 1:1 wie in 1.0.5 darauf aufbauen können.
 *
 * Umsetzungen:
 *   zugriff_datei   - direkt aufs Dateisystem. Auf der PS5 ohne Wurzel und
 *                     OHNE Schreib-/Löschbefehle (die sind nur mit
 *                     KERN_LOESCHEN eingebaut). Am PC mit Wurzel (Kopie eines
 *                     Testbaums) und denselben Fehlereinspielungen wie der
 *                     Test-FTP-Server miniftp4.py von 1.0.5.
 *   zugriff_sim     - Simulation: liest über eine andere Umsetzung, löscht
 *                     und schreibt aber nur "auf dem Papier" und führt
 *                     darüber Buch.
 */
#ifndef KERN_ZUGRIFF_H
#define KERN_ZUGRIFF_H

#include "basis.h"

typedef struct { int code; const char *text; } antwort;          /* text wie FTP: "550 no" */
typedef struct { const char *name; bool ordner, link; long long groesse; } kdatei;
typedef struct { kdatei *e; int n; } kdliste;

typedef struct zugriff zugriff;
struct zugriff {
    const char *art;
    /* CWD pfad: 250 = Ordner ist da und lesbar, 550 = nicht da/kein Ordner/verweigert */
    antwort (*cwd)(zugriff *z, arena *a, const char *pfad);
    /* LIST pfad: 226 = vollständig (aus), sonst Fehler (wie Daten() == null) */
    antwort (*liste)(zugriff *z, arena *a, const char *pfad, kdliste *aus);
    /* RETR pfad: 226 = gelesen */
    antwort (*lies)(zugriff *z, arena *a, const char *pfad, const unsigned char **daten, size_t *n);
    /* STOR pfad: 226 = geschrieben */
    antwort (*schreibe)(zugriff *z, arena *a, const char *pfad, const void *daten, size_t n);
    /* NEU 1.1 (Selbstschutz S2): nur die ersten bis zu n Bytes einer Datei lesen (Verknüpfung wird
     * nicht verfolgt). 226 = gelesen. Darf NULL sein (dann gibt es diese Möglichkeit nicht). */
    antwort (*kopf)(zugriff *z, arena *a, const char *pfad, size_t n, const unsigned char **daten, size_t *len);
    /* Befehl mit Pfad: "SITE CHMOD 777", "DELE", "RMD", "RMDA"; ohne Pfad: "NOOP" */
    antwort (*befehl)(zugriff *z, arena *a, const char *befehl, const char *pfad);
    void (*schliessen)(zugriff *z);
    void *zustand;
};

/* ---------------------------------------------------------------- Dateisystem */
typedef struct {
    const char *wurzel;          /* "" auf der PS5; am PC der Ordner mit der Testbaum-Kopie */
    bool sortiert;               /* Ordnerliste nach Namen sortieren (wie miniftp4.py) */
    bool ordnergroesse_null;     /* Ordner mit Größe 0 melden (wie miniftp4.py unter Windows) */
    bool test_einspielungen;     /* Umgebungsvariablen wie miniftp4.py: DENY, FAILLIST, FAILDEL,
                                    STOPFAIL, DIEON, FAKELINKS, FAKESM, RMDA */
    const char *test_hier;       /* Ordner, in dem faillist.on liegt */
} datei_optionen;

zugriff *zugriff_datei(const datei_optionen *o);
bool zugriff_datei_kann_loeschen(void);       /* true nur mit KERN_LOESCHEN gebaut */

/* NEU 1.0.6 (da_Chris86 28.10.2026): Kacheln über Sonys Deinstallations-Dienst entfernen - wie das PS5-Menü,
 * damit keine Geisterkachel bleibt. Befehle über z->befehl:
 *   "SITE UNINSTALL?" ohne Pfad   -> 200 = den Dienst gibt es hier, 502 = gibt es nicht (PC, Windows/FTP)
 *   "SITE UNINSTALL?" Kennung     -> 200 = darf über Sony entfernt werden, 550 = nicht (Text sagt warum)
 *   "SITE UNINSTALL"  App-Ordner  -> Auftrag an Sony für die Kennung des Ordners; 200 = angenommen (Sony löscht danach)
 * Die Simulation gibt nur die Prüfungen weiter und führt den Auftrag selbst "auf dem Papier" aus.
 * Auf der PS5 setzt die Oberfläche die Funktion (app/plattform_ps5.c); NULL = kein Dienst.
 * Test am PC (test_einspielungen): Umgebungsvariable UNINSTALL=ok|ablehnen|langsam, UNINSTALL_NIE=<Kennung>. */
extern antwort (*zugriff_sony_deinstallieren)(arena *a, const char *id, bool nur_pruefen);
/* NEU 1.0.6 (Freigabe da_Chris86 29.10.2026 21:23): eigenen Startordner /system_ex/app/PSCL00002 bei der Selbstentfernung
 * löschen (nur PS5, start/startordner.h). Rückgabe 1 = entfernt, 0 = nicht da, -1 = Fehler/fremd (*text). NULL = gibt es nicht. */
extern int (*zugriff_startordner_entfernen)(arena *a, bool nur_pruefen, const char **text);
/* NEU 1.0.6 (da_Chris86 29.10.2026 23:08): vollständige Selbstentfernung über den Hintergrundteil von ps5cleanup.elf.
 * Die Oberfläche übergibt den Auftrag (eigene Kachel über Sony + eigener Startordner); der Hintergrundteil bestätigt,
 * wartet auf das Ende der Oberfläche und entfernt beides danach. Rückgabe 1 = bestätigt übernommen, 0 = kein Hintergrundteil
 * erreichbar, -1 = abgelehnt/Fehler (*text). NULL = gibt es nicht (PC, Windows, Simulation) - dann wie bisher. */
extern int (*zugriff_selbst_uebergeben)(arena *a, bool kachel, bool startordner, const char **text);
/* NEU 1.0.6: Diagnoseprotokoll (PS5: Kernel-Protokoll) - jeder Schritt der Selbstentfernung mit Grund, auch übersprungene.
 * Liegt außerhalb der zu löschenden Daten. NULL = keins. */
extern void (*zugriff_diagnose)(const char *text);

/* ---------------------------------------------------------------- Simulation */
zugriff *zugriff_sim(zugriff *lesen);
/* Buch der Simulation: jede Zeile "DELE /pfad", "RMD /pfad", "RMDA /pfad",
 * "SITE CHMOD 777 /pfad", "STOR /pfad" in der Reihenfolge, in der sie kamen */
const sliste *zugriff_sim_buch(zugriff *sim);

#endif
