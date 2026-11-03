/* PS5 Cleanup 1.1 - gemeinsamer C-Kern
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
    /* NEU 1.0.6 Fix (30.10.2026): enthält die Datei die Byte-Folge m? Liest stückweise (nie die ganze Datei in den Speicher).
     * 226 = gelesen (*ja), sonst Fehler. Darf NULL sein (dann liest die Sitzung die Datei ganz und gibt den Speicher gleich frei). */
    antwort (*enthaelt)(zugriff *z, arena *a, const char *pfad, const void *m, size_t ml, bool *ja);
    /* NEU 1.0.7 (03.11.2026, Beleg aus dem Programm einer Kachel - kern_suche.c beleg_aus_programmen): Datei stückweise lesen (je Stück
     * höchstens stueck Bytes) und jedes Stück an weiter() geben, am Ende einmal weiter(c, NULL, 0). Liefert weiter() false, wird sofort
     * aufgehört (Abbrechen). Nur lesen; eine Verknüpfung am Ende des Pfads wird nicht verfolgt. 226 = gelesen. Darf NULL sein. */
    antwort (*stuecke)(zugriff *z, arena *a, const char *pfad, size_t stueck, bool (*weiter)(void *c, const unsigned char *d, size_t n), void *c);
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
 *                                    226 = NEU 1.0.7: Sony führt die Kennung nicht im Menü und app.db kennt sie auch nicht
 *                                    (Reste ohne Kachel) - nichts beauftragt, die Dateien darf der Kern selbst löschen
 * Die Simulation gibt nur die Prüfungen weiter und führt den Auftrag selbst "auf dem Papier" aus.
 * Auf der PS5 setzt die Oberfläche die Funktion (app/plattform_ps5.c); NULL = kein Dienst.
 * Test am PC (test_einspielungen): Umgebungsvariable UNINSTALL=ok|ablehnen|langsam, UNINSTALL_NIE=<Kennung>. */
extern antwort (*zugriff_sony_deinstallieren)(arena *a, const char *id, bool nur_pruefen);
/* NEU 1.0.7 (da_Chris86 01.11.2026: Bilder, Videos und Spielstände ohne leere Einträge entfernen, kein Datenbank-Neuaufbau;
 * PS5-Tests 01.11.2026 17:53-18:24 bestanden). Befehle über z->befehl:
 *   "SITE MEDIUM?"     ohne Pfad -> 200 = Sonys Löschdienst für Bilder/Videos ist da (nur PS5 Cleanup auf der PS5), 502 = nicht
 *   "SITE MEDIUM?"     Datei     -> 200 = diese Aufnahme darf über Sony gelöscht werden, 550 = nicht (Text sagt warum)
 *   "SITE MEDIUM"      Datei     -> Sony löscht die Aufnahme samt .meta/.dat und Vorschaubild und trägt sie aus der Mediengalerie
 *                                   aus; 200 erst, wenn die Dateien nachweislich weg sind; 551 = Sony hat (vielleicht)
 *                                   angenommen, ist aber nicht fertig - der Rest bleibt
 *   "SITE SPIELSTAND?" ohne Pfad -> 200 = Sonys Spielstand-Liste (savedata.db) kann ausgetragen werden (nur PS5), 502 = nicht
 *   "SITE SPIELSTAND?" Ordner    -> 200 = darf ausgetragen werden (Spiel läuft nicht, Liste da), 550 = nicht (Text sagt warum)
 *   "SITE SPIELSTAND"  Ordner    -> Liste auf den USB-Stick sichern, dann alle Einträge dieses Spielstand-Ordners austragen;
 *                                   200 = erledigt. Die Dateien löscht danach der Kern (erst Eintrag, dann Dateien - nie umgekehrt)
 * Ordner = /user/home/<Benutzer>/savedata[_prospero]/<Titel>. NULL = kein Dienst (PC, Windows) - dann bleiben solche Einträge
 * unverändert. Die Simulation gibt nur die Prüfungen weiter. Test am PC: MEDIEN=ok|ablehnen|langsam (MEDIEN_NIE=<Dateiname>),
 * SPIELSTAND=ok|ablehnen|belegt. */
extern antwort (*zugriff_medium_loeschen)(arena *a, const char *pfad, bool nur_pruefen);
extern antwort (*zugriff_spielstand_austragen)(arena *a, const char *ordner, bool nur_pruefen);
/* Aufnahme der Mediengalerie? /user/av_contents/(photo|video)/<App>/<Spiel>[/<Unterordner>]/<Datei> - nicht .meta/.dat */
bool zugriff_medium_pfad(const char *p);
/* Spielstand-Ordner /user/home/<8 Hex>/savedata[_prospero]/<Name>? Setzt (wenn nicht NULL) benutzer[9], *prospero, titel[65] */
bool zugriff_spielstand_pfad(const char *p, char *benutzer, bool *prospero, char *titel);
/* NEU 1.0.7 (da_Chris86 31.10.2026 21:50, Punkt 4): ist /system_ex/app/<id> ein eigener Einhängepunkt? Nur lesen (statfs).
 * 1 = ja (*fstyp, *quelle gesetzt), 0 = nein, -1 = nicht feststellbar. NULL = gibt es nicht (PC, Windows) - dann wie bisher. */
extern int (*zugriff_einbindung)(arena *a, const char *id, const char **fstyp, const char **quelle);
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
/* NEU 1.0.7 (da_Chris86 31.10.2026 17:00: Ausnahme für leere Einhängeordner von ShadowMount): /system_ex für die Dauer dieser
 * Sitzung beschreibbar machen (PS5: nur einmal pro Einschalten möglich; zurückgestellt wird am Ende). 1 = beschreibbar,
 * -2 = in dieser Sitzung nicht mehr möglich, -1 = Fehler (*text). NULL = gibt es nicht (PC, Windows, Simulation). */
extern int (*zugriff_system_ex_oeffnen)(arena *a, const char **text);
/* NEU 1.0.7 (Hänger 31.10.2026, da_Chris86: "entscheide du"): Sicherheitsbremse. Wird vor jeder Ordnerliste gefragt; true = der
 * Arbeitsspeicher wird knapp (PS5: Sonys freier Speicher unter 100 MB oder PS5 Cleanup selbst über 150 MB) -> sauber anhalten
 * (gleicher Weg wie Abbrechen). grund: Text fürs Protokoll. NULL = keine Bremse (PC-Tests, Windows). */
extern bool (*zugriff_speicher_knapp)(const char **grund);
extern volatile int zugriff_speicher_gebremst;          /* 1 = die Bremse hat ausgelöst (für die Meldung) */
extern const char *volatile zugriff_speicher_grund;
/* Die Bremse greift nur in Fäden, in denen ein Anhalten sicher aufgefangen wird (Ablauf, Zählen im Hintergrund) - sonst wäre
 * ein Anhalten ohne Auffänger ein Absturz (PC-Test 31.10.2026). 1 = hier darf gebremst werden. */
extern _Thread_local int zugriff_bremse_hier;

/* ---------------------------------------------------------------- Simulation */
zugriff *zugriff_sim(zugriff *lesen);
/* Buch der Simulation: jede Zeile "DELE /pfad", "RMD /pfad", "RMDA /pfad",
 * "SITE CHMOD 777 /pfad", "STOR /pfad" in der Reihenfolge, in der sie kamen */
const sliste *zugriff_sim_buch(zugriff *sim);

#endif
