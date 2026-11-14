/* PS5 Cleanup - gemeinsamer C-Kern
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
    /* NEU 1.0.8 (Arbeitsplan 2.1, Spielname im exFAT-Abbild): n Bytes ab Stelle ab nach ziel lesen (Verknüpfung am Ende des Pfads wird
     * nicht verfolgt). *len = wirklich gelesen (am Dateiende weniger). 226 = gelesen. Darf NULL sein (dann gibt es das nicht). */
    antwort (*lies_ab)(zugriff *z, arena *a, const char *pfad, long long ab, size_t n, unsigned char *ziel, size_t *len);
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
 *                                    553 = Sony hat den ersten Auftrag ausdrücklich abgelehnt, nichts angenommen (NEU 1.0.10, Codex-Prüfung 171)
 *                                    226 = NEU 1.0.7: Sony führt die Kennung nicht im Menü und app.db kennt sie auch nicht
 *                                    (Reste ohne Kachel) - nichts beauftragt, die Dateien darf der Kern selbst löschen
 * Die Simulation gibt nur die Prüfungen weiter und führt den Auftrag selbst "auf dem Papier" aus.
 * Auf der PS5 setzt die Oberfläche die Funktion (app/plattform_ps5.c); NULL = kein Dienst.
 * Test am PC (test_einspielungen): Umgebungsvariable UNINSTALL=ok|ablehnen|langsam, UNINSTALL_NIE=<Kennung>. */
extern antwort (*zugriff_sony_deinstallieren)(arena *a, const char *id, bool nur_pruefen);
/* NEU 1.0.10 (da_Chris86 12.11.2026 21:56, TabShifter TBSH00001): Homebrew-Kachel, die ein Programm direkt in Sonys Datenbanken eingetragen hat
 * und deren Entfernen Sony ablehnt, gezielt aus app.db und appinfo.db austragen (app/spielstand_db.c kachel_db_austragen: alles oder nichts,
 * nur bekannte Stellen, ohne Sony-Paket, vorher und nachher geprüft). Befehle über z->befehl:
 *   "SITE KACHEL-DB?" Kennung -> 200 = austragbar (ändert nichts), 550 = nicht (Text sagt warum), 502 = in dieser Fassung nicht eingebaut
 *   "SITE KACHEL-DB"  Kennung -> 200 = ausgetragen, sonst nichts geändert (nur Fassungen mit Löschen)
 * Die Plattform setzt zugriff_kachel_db (nur die echte Fassung der PS5; Tests setzen ihn selbst). Die Simulation gibt nur "?" weiter. */
extern antwort (*zugriff_kachel_db)(arena *a, const char *id, bool nur_pruefen);
/* NEU 1.0.10 (da_Chris86 12.11.2026 22:40, Zusatzinhalte): Sony führt installierte Zusatzinhalte in addcont.db. Werden nur ihre Ordner gelöscht,
 * bleibt Sonys Eintrag (ps4mount docs/superpowers/specs/2026-10-23-dlc-internal-install-design.md: sceAppInstUtilAppUnInstallAddcont(Kennung,
 * Name) entfernt Eintrag UND Dateien, auf der PS5 bestätigt). Darum wie bei Kacheln zuerst Sony, nachweisen, dann die Dateien. Arg = "<Kennung>"
 * (alle Zusatzinhalte des Spiels) oder "<Kennung>/<Name>" (Name = dir_name in addcont.db = Ordnername unter addcont/<Kennung>):
 *   "SITE ZUSATZ?" Arg -> 200 = Sony führt so viele (Text), 226 = Sony führt keinen, 550 = Liste nicht lesbar, 502 = nicht eingebaut. Ändert nichts.
 *   "SITE ZUSATZ"  Arg -> jeden davon über Sony austragen, danach in addcont.db nachsehen: 200 = alle weg, 226 = Sony führte keinen,
 *                         551 = ein Teil ist schon ausgetragen (teilweise), sonst 550 (Text sagt, was blieb). Nur Fassungen mit Löschen.
 *                         Der Kern beauftragt nur Ordner auf dem internen Speicher und je Zusatzinhalt (Name) einzeln (Codex-Prüfung 172).
 * Die Plattform setzt zugriff_zusatz (nur die echte Fassung der PS5). Test am PC: ZUSATZ=ok|keins|ablehnen|teil. Die Simulation gibt nur "?" weiter. */
extern antwort (*zugriff_zusatz)(arena *a, const char *arg, bool nur_pruefen);
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

/* NEU 1.0.9 (da_Chris86 08.11.2026 08:02/08:20, Playden & Co.): Programme legen Ordner an, in die nur sie selbst dürfen - root darf auf
 * der PS5 nicht hinein ("Root can't bypass permissions on the console", Playden). Scheitert ein Schritt daran, wiederholt zugriff_datei
 * ihn EINMAL als eingetragener Besitzer dessen, was den Weg versperrt - nur unter /user/data und auf M.2 / erweitertem Speicher
 * außerhalb von <Laufwerk>/user, nie für root, nie über eine Verknüpfung (Einzelheiten: zugriff_datei.c, "Besitzer-Wechsel").
 * zugriff_als_besitzer: tu(c) als Benutzer uid ausführen und dessen Ergebnis liefern; dfd/name = das, was den Weg versperrt (name NULL =
 * dfd selbst). false = kein Wechsel möglich (dann ist nichts passiert). PS5: seteuid und sofort zurück (app/plattform_ps5.c). NULL = gibt
 * es nicht (PC, Simulation ohne PS5) - dann wie bisher: nicht lesbar = geschützt, nie gelöscht.
 * zugriff_rechte_kaputt: das Zurückwechseln ging nicht - ab da verändert die Zugriffsschicht nichts mehr und der Kern hört auf.
 * Befehle über z->befehl (alle ohne Pfad):
 *   "SITE PRIVAT?"        -> 200 = der Wechsel ist eingerichtet, 502 = nicht
 *   "SITE PRIVAT-LISTE?"  -> 200, erste Zeile "200 <Anzahl> <nicht aufgezählt>", dann je Ort "<L|W> <Benutzer> <Schritte> <Pfad>"
 *                            (L = als Besitzer gelesen, W = als Besitzer gelöscht, Pfad = Ziel des Löschbefehls) seit "SITE PRIVAT-NEU"
 *   "SITE PRIVAT-NEU"     -> Liste leeren (Beginn einer Suche), 200
 * Der Wechsel gilt auf der PS5 für den ganzen Prozess: Datei-Arbeit außerhalb der Zugriffsschicht, die nicht hineinfallen darf (Bilder der
 * Oberfläche, Protokoll-Dateien), nimmt dieselbe Sperre: sperren/frei (wartet) bzw. versuchen (false = gerade läuft ein Wechsel). */
extern bool (*zugriff_als_besitzer)(int dfd, const char *name, unsigned uid, bool (*tu)(void *c), void *c);
extern volatile bool zugriff_rechte_kaputt;
/* NEU 1.0.10 (da_Chris86 10.11.2026 17:30: Nutzerdaten, Spielstände und gemeinsam genutzte Ordner schützen; aus dem Entwurf 1.0.9):
 * gewechselt wird nur im Ordner eines BEKANNTEN Programms (Playden, ProsperoStore ...). name = der Ordner des Programms (/user/data/<name>,
 * /user/data/homebrew/<name>, /mnt/extN/<name>, /mnt/extN/homebrew/<name>). Setzt der Kern (kern_neu: kern_ordner_bekannt). NULL = nie
 * wechseln - unbekannte Ordner bleiben wie bis 1.0.9 geschützt. */
extern bool (*zugriff_besitzer_bekannt)(const char *name);
void zugriff_wechsel_sperren(void);
bool zugriff_wechsel_versuchen(void);
void zugriff_wechsel_frei(void);

/* NEU 1.0.10 (da_Chris86 12.11.2026 15:12-15:27): Playden beim Löschen eines seiner Spiele sauber beenden und das Spiel danach genau so aus
 * seinen Listen austragen, wie Playdens eigenes "Uninstall" es tut (Quellcode leolobato/playden-ps5). Befehle über z->befehl:
 *   "SITE PLAYDEN?"          ohne Pfad -> 200 = Playden läuft (seine Webseite auf 127.0.0.1:7031-7039 antwortet), 250 = keine Antwort,
 *                                         502 = nicht feststellbar (kein Netz-Zugang: PC, Simulation ohne PS5)
 *   "SITE PLAYDEN-ENDE"      ohne Pfad -> Playdens eigener Befehl "beenden" (POST /api/quit), dann warten, bis keine Antwort mehr kommt;
 *                                         200 = beendet, sonst nicht (Text sagt warum)
 *   "SITE PLAYDEN-AUSTRAGEN" Kennung   -> das Spiel FAKE78xxx aus /data/playden/runtime-state/installs.json (Eintrag und Reservierung) und aus
 *                                         /data/playden-games/.playden-games (seine Zeile) austragen - nur genau diese Einträge, alles andere
 *                                         Byte für Byte gleich, vorher und nachher geprüft wie Playden selbst liest; als Besitzer der Dateien
 *                                         geschrieben (neue Datei daneben, dann in einem Schritt getauscht). 200 = ausgetragen, sonst nichts geändert.
 *   "SITE PLAYDEN-PRUEFEN"   Kennung   -> NEU 1.0.10 (PS5-Test 12.11.2026 19:49): alles wie AUSTRAGEN - lassen sich beide Listen lesen, ist der
 *                                         Eintrag eindeutig und ohne laufende Arbeit? Seit 22:40 (Fallout-Test) wird jede Liste, die das Austragen
 *                                         ändern würde, mit ihrem BISHERIGEN Inhalt auf genau demselben Weg neu geschrieben - so ist das Schreiben
 *                                         selbst vor dem Löschen bewiesen, inhaltlich ändert sich nichts. 200 = austragbar, 226 = steht in keiner
 *                                         Liste, sonst nicht (Text sagt warum). Vor dem Löschen eines Playden-Spiels: geht es nicht, bleibt das
 *                                         ganze Spiel unverändert.
 *   "SITE PLAYDEN-PRUEFEN?"  Kennung   -> wie PRUEFEN, aber ohne zu schreiben (nur die Bedingungen) - für den Testmodus.
 *   Lesen, Prüfen und Schreiben gehen seit 1.0.10 denselben Weg wie das Nachschauen (über den Pfad, in einem Durchgang als derselbe Benutzer).
 * Die Simulation gibt "SITE PLAYDEN?" und "SITE PLAYDEN-PROBE" weiter, "SITE PLAYDEN-PRUEFEN" als "SITE PLAYDEN-PRUEFEN?" (ändern nichts). Test am PC: PLAYDEN_WEB=laeuft|zu|bleibt (bleibt: beenden klappt nicht). */
extern int (*zugriff_playden)(arena *a, bool beenden, const char **text);
/* NEU 1.0.10 (Codex-Prüfung 163): "SITE LAEUFT?" Kennung - läuft dieses Spiel gerade (Sandbox eingehängt)? "SITE PS4MOUNT?" - läuft ps4mount?
 * 200 = ja, 250 = nein, 502 = nicht feststellbar. Test am PC: LAEUFT=<Kennung,...>, PS4MOUNT=laeuft|aus. PS5: app/plattform_ps5.c. */
extern int (*zugriff_titel_laeuft)(const char *id);   /* 1 = läuft, 0 = nein, -1 = nicht feststellbar */
extern int (*zugriff_port_belegt)(int port);          /* 1 = 127.0.0.1:port nimmt an, 0 = abgewiesen, -1 = nicht feststellbar */   /* PS5: 1 = läuft / beendet, 0 = keine Antwort / nicht beendet, -1 = Fehler */

/* ---------------------------------------------------------------- Simulation */
zugriff *zugriff_sim(zugriff *lesen);
/* Buch der Simulation: jede Zeile "DELE /pfad", "RMD /pfad", "RMDA /pfad",
 * "SITE CHMOD 777 /pfad", "STOR /pfad" in der Reihenfolge, in der sie kamen */
const sliste *zugriff_sim_buch(zugriff *sim);

/* ---------------------------------------------------------------- Mitschnitt (NEU 1.0.8, Arbeitsplan 2.7, zugriff_mitschnitt.c) */
/* legt sich um innen und schreibt jede Frage samt Antwort als Textzeile(n) über schreib() mit (aus jedem Faden, nie gleichzeitig).
 * Verändert selbst nichts. Nur die Test-Fassung (MITSCHNITT) benutzt ihn. */
zugriff *zugriff_mitschnitt(zugriff *innen, void (*schreib)(void *c, const char *zeilen), void *c);
/* vor und nach dem Entfernen einer Kachel ruft der Mitschnitt das auf (z. B. Zeilen der Datenbanken zu dieser Kennung); NULL = nichts */
extern void (*zugriff_mitschnitt_zustand)(const char *id, bool nachher, void (*zeile)(const char *t));
/* eine eigene Zeile im Format des Mitschnitts: typ 'I' (Hinweis) oder 'P' (Protokoll) mit t1, 'D' (appinfo.db) mit t1..t3 */
const char *zugriff_mitschnitt_zeile(arena *a, char typ, const char *t1, const char *t2, const char *t3);
/* PC-Nachbau: beantwortet die Fragen aus einem Mitschnitt (suche_nr > 0: nur die n-te Suche). Verändert nie etwas. NULL = *fehler */
zugriff *zugriff_wiedergabe(const char *datei, int suche_nr, const char **fehler);
int zugriff_wiedergabe_fehlend(zugriff *z, const sliste **welche);      /* Fragen, die nicht im Mitschnitt standen */
int zugriff_wiedergabe_kaputt(zugriff *z, const sliste **welche);       /* unvollständige oder kaputte Datensätze (nicht verwendet) */
const sliste *zugriff_wiedergabe_info(zugriff *z);                      /* die Hinweise (I) */
void zugriff_wiedergabe_appinfo(zugriff *z, void (*zeile)(void *c, const char *titel, const char *feld, const char *wert), void *c);

#endif
