/* PS5 Cleanup - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * kern.h - die Klasse Kern aus 1.0.5 (Kern.cs, Suche.cs, Ausfuehren.cs).
 * Felder und Funktionen tragen dieselben Namen wie in C#.
 */
#ifndef KERN_KERN_H
#define KERN_KERN_H

#include <string.h>
#include "basis.h"
#include "hilfen.h"
#include "sitzung.h"

/* Konstanten aus Kern.cs (als Text-Makros, damit die Muster wie in C#
 * durch Aneinanderhängen entstehen) */
#define KERN_VERSION   "1.0.6"
#define BEHALTEN_APP   "PPSA01325"                    /* Astro's Playroom - seit 1.0.6 ein normales Spiel (da_Chris86 28.10.2026), Kennung nur noch für Tests */
#define M2             "/mnt/ext1"
#define SYS_META       "/system_data/priv/appmeta"
#define SM_ORDNER      "/user/data/shadowmount"
#define USB_ERW        "/mnt/ext0"
/* NEU 1.0.9 (08.11.2026, Nachprüfung im Quellcode: ShadowMountPlus src/sm_image.c, kstuff-lite ps5-kstuff-ldr/src/image.c): Endungen der
 * Spiel-Abbilder, die eingehängt werden (die Datei muss bleiben). .ufs ist UFS wie .ffpkg (kstuff-lite) und fehlte bis 1.0.8 überall. */
#define ABBILD_ENDUNGEN "exfat|ffpfs|ffpfsc|ffpkg|pfs|img|ufs"

/* class Eintrag (Kern.cs) */
typedef struct eintrag {
    sliste Pfade, PfadeBasis;
    int Kat;
    const char *Text, *TextBasis;
    bool Wahl;
    const char *Quelle;
    bool Gesperrt;
    const char *Gruppe;        /* NULL = gehört zu keinem Spiel */
    const char *Teil;          /* "Spiel" / "Update" / "DLC" / NULL */
    bool Paket;                /* Trophäen-Liste eines noch installierten Spiels */
    bool NurLeer;              /* leerer M.2-Ordner: nur RMD */
    bool Kachel;               /* betrifft eine Kachel -> ShadowMount vorher anhalten */
    bool SmTipp;               /* ShadowMount-Spiel, dessen Kachel mitgeht */
    bool NurEinzeln;           /* NEU 1.1: nur einzeln wählbar - nie über "ganzen Bereich wählen"; in der vollständigen Bereinigung nur auf ausdrücklichen Wunsch (Jailbreak-Frage) */
    bool Jailbreak;            /* NEU 1.1 (Anforderungen 27.10.2026): möglicher Jailbreak-Einstieg - einzeln löschbar, bei der vollständigen Bereinigung wird gefragt */
    const char *SperrGrund;    /* NEU 1.1: eigener Grund, warum gesperrt (NULL = Game Compressor wie 1.0.5) */
    long long Bytes; int Dateien; int Ordner; bool HatGroesse;
    int Ort;                   /* NEU 1.0.6: wo die Daten liegen (ORT_*) - steht vorne im Text, entscheidet bei der vollständigen Bereinigung */
    const char *KachelId;      /* NEU 1.0.6 (da_Chris86 28.10.2026): Eintrag mit Kachel im PS5-Menü - Kennung für Sonys Deinstallation (NULL = keine) */
    const char *KachelOrdner;  /* NEU 1.0.6: App-Ordner dieser Kachel (/user/app/ID bzw. /mnt/extN/user/app/ID) */
    /* NEU 1.0.7 (Grundlage da_Chris86 31.10.2026 13:12, doku/PLAN_1.0.7.md): Art des Eintrags -> Ampel und kurzer Satz in der Zeile */
    int Art;                   /* ART_* (kern_arten_setzen am Ende der Suche) */
    bool Rot;                  /* endgültig weg -> nie über "ganzen Bereich wählen", nur einzeln (die vollständige Bereinigung bleibt wie sie ist) */
    const char *PlSpiel;       /* NEU 1.0.10 (da_Chris86 12.11.2026 15:12-15:27): Kachel, Starter oder Spieldateien eines Playden-Spiels - seine
                                * Kennung FAKE78xxx (kern_suche.c playden_spiele). Gewählt geht das ganze Spiel zusammen (kern_abhaengige_mitnehmen) */
    const char *PlSpst;        /* NEU 1.0.10: Spielstände eines Playden-Spiels im Bereich Spielstände - Kennung, "" = Spiel nicht mehr installiert */
    const char *PlNurListe;    /* NEU 1.0.10 (da_Chris86 12.11.2026 20:06): Playden-Spiel, von dem nur noch der Eintrag in Playdens Listen übrig ist (Kachel und
                                * Spieldateien nachweislich weg) - Kennung FAKE78xxx. Keine Datei: gelöscht wird nur der Eintrag (playden_austragen) */
    bool PlaydenStarter;       /* NEU 1.0.10: SmDatei = Starter eines Spiels, das Playden angelegt hat (kern_suche.c playden_starter_markieren) - gelb */
    bool SmDatei;              /* Spieldatei eines ShadowMount-Spiels als eigene Zeile - wird nie selbst gelöscht: ist sie gewählt,
                                * nimmt die Kachel (SmKachel) beim Löschen die Spieldatei mit ("Spiel + Kachel") */
    struct eintrag *SmKachel;  /* bei SmDatei: der Kachel-Eintrag, zu dem die Spieldatei gehört */
    const char *ToolOrdner;    /* NEU 1.0.7: Tool-Ordner, dessen Inhalt ohne eingeblendeten Spielordner (folder-source) einzeln in Pfade steht */
    struct eintrag *ToolKachel; /* NEU 1.0.7 (PS5-Test 01.11.2026): Daten-Eintrag unter seiner Tool-Kachel - ist die Kachel mit angehakt und bleibt sie, bleiben auch die Daten */
    int KatHer;                /* NEU 1.0.7 (PS5-Test 02.11.2026, "ein Block pro Kachel"): Bereich, aus dem der Eintrag unter seine Kachel
                                * gezogen wurde (0 = nicht verschoben) - die Art richtet sich weiter nach dem, was er ist */
    int ListeNr;               /* NEU 1.0.7 (PS5-Test 02.11.2026, DRIVECLUB + Bikes): Nummer der Trophäen-Liste, wenn ein Spiel mehrere hat (0 = nur eine) */
    int SidKaputt;             /* NEU 1.0.7 (PS5-Test 02.11.2026, Red Dead Redemption 2): so viele kaputte Zusatzinhalt-Angaben hat dieses Spiel */
    const char *WichtigWas;    /* NEU 1.0.7 (PS5-Test 03.11.2026): Daten-Ordner eines Tools mit Unterordnern/Dateien wie roms, keys, saves,
                                * Mcd001.ps2 - kurzer Satz ("vermutlich eigene Spiele oder Spielstände"), NULL = nichts gefunden */
    const char *WichtigFund;   /* die gefundenen Namen dazu ("roms, keys") */
    /* NEU 1.0.8 (Arbeitsplan 2.3): Tool ohne Kachel (nanoDNS, zftpd) - die Payload-Datei steht unter dem Daten-Ordner desselben Tools.
     * Nur Anzeige: anders als ToolKachel hängt beim Löschen nichts daran, jede Zeile behält ihren eigenen Haken */
    struct eintrag *Begleit;   /* bei der Payload-Datei: der Daten-Ordner, unter dem sie steht */
    bool Erkannt;              /* NEU 1.0.10 (da_Chris86 14.11.2026 01:21): Daten-Ordner, den das Rezept einer App mit Kachel zuordnet ("erkannt, nicht
                                * bewiesen") - steht nur zur Anzeige in ihrem Block (Begleit), behält seinen eigenen Haken (der Haken am Block nimmt
                                * ihn nicht mit) und hängt beim Löschen an nichts */
    const char *BlockName;     /* beim Daten-Ordner: Name des Blocks ("nanoDNS"), NULL = kern_name */
    bool OhneKachel;           /* beim Daten-Ordner: Kopf eines Blocks "Tool ohne Kachel" */
    pliste Doppelt;            /* NEU 1.0.8 (Arbeitsplan 2.4): die anderen Kopien desselben Spiels (installiert oder Spieldatei) - leer = keine */
    /* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 12, da_Chris86: jede PKG-Datei steht dort, wo das steht, was sie installiert): Angaben einer
     * Installationsdatei (PKG) - aus dem Kopf der Datei, sonst aus dem Dateinamen. PkgArt 0 = keine PKG-Datei. */
    int PkgArt;                /* PKG_SPIEL, PKG_UPDATE, PKG_DLC, PKG_APP */
    const char *PkgId;         /* Spielnummer (CUSA12345), NULL = unbekannt */
    const char *PkgTitel;      /* Name aus dem PKG bzw. aus dem Dateinamen ("" = keiner) */
    const char *PkgVer;        /* Version eines Updates bzw. Name eines Zusatzinhalts ("" = keine) */
    int PkgAnzahl;             /* so viele PKG-Dateien stehen in dieser Zeile (ab 6 gleicher Art auf einem Speicher: eine Zeile) */
    long long PkgBytes;        /* Größe laut Ordnerliste (nur für "doppelt") */
    const char *PkgNotiz;      /* 1.0.10: zweiter Pfad = ps4mounts Notiz origin.txt neben dieser app.pkg (merke_pkg_ordner), sonst NULL */
    long long *PkgGroessen;    /* zusammengefasste Zeile: Größe je Pfad (wie PfadeBasis) - für "doppelt" (Codex-Prüfung 83) */
    bool PkgOhneSpiel;         /* Kopf eines Blocks "<Spiel> · nicht installiert" */
    bool PkgSpielFehlt;        /* NEU 1.0.8 (PS5-Test 07.11.2026): steht im Block seines Spiels bzw. seiner App, davon ist aber nur ein Rest da
                                * (Kachel mit leerem App-Ordner, tote ShadowMount-Kachel) - eigener Text, rot wie jede PKG-Datei */
    bool PkgOffen;             /* Codex-Prüfung 90: Ordner, in dem PKG-Dateien nicht gesucht werden konnten (zu viele Dateien, zu tief, nicht
                                * lesbar) - er kann PKG-Dateien enthalten: rot, nur einzeln (wie eine PKG-Datei) */
    bool WichtigLeer;          /* NEU 1.0.8 (Befund 2): bekannter wichtiger Ordner (etaHEN ...), nachweislich ohne Datei mit Inhalt */
    unsigned char UsbKopie;    /* NEU 1.0.8 (PS5-Test 07.11.2026 22:53): Ordner, den die Konsole auf einem USB-Laufwerk anlegt - was darin liegt (USB_KOPIE_...) */
    unsigned char StoreTeil;   /* NEU 1.0.9 (#145): Teil einer App in den Ordnern von ProsperoStore (STORE_*), 0 = keiner (kern_suche.c merke_store) */
    bool Privat;               /* NEU 1.0.9 (da_Chris86 08.11.2026), aufgenommen 1.0.10: enthält private Daten eines Programms - nur als dessen Benutzer
                                * lesbar (kern_privat.c, zugriff_datei.c Besitzer-Wechsel); Zeile "mit privaten Daten", Erklärung im Kasten */
    const char *RepoName;      /* NEU 1.0.10 (da_Chris86 13.11.2026): Ordner eines Programms, das ein Payload-Verwalter aus einer Quelle geladen hat
                                * (/user/data/<Verwalter>/payloads/repo/<Name>@<Quelle>) - der Name; steht im Block desselben Programms (kern_suche.c) */
} eintrag;
enum { PKG_KEINE = 0, PKG_SPIEL, PKG_UPDATE, PKG_DLC, PKG_APP };
enum { USB_KOPIE_AUFNAHMEN = 1, USB_KOPIE_SPIELSTAENDE = 2, USB_KOPIE_SICHERUNG = 4 };   /* NEU 1.0.8: eintrag.UsbKopie */
/* NEU 1.0.9 (#145): eintrag.StoreTeil - aufgehobene Fassung (previous, backup), Rest einer Arbeit (trash, staging), Beleg (receipts) */
enum { STORE_FASSUNG = 1, STORE_REST = 2, STORE_BELEG = 3 };
/* NEU 1.0.7: Arten der Einträge (nur Anzeige und Auswahl-Regeln - Pfade und Schutz bleiben unberührt) */
enum {
    ART_SONST = 0,
    ART_SPIEL, ART_APP, ART_UPDATE, ART_DLC, ART_SM_KACHEL, ART_SM_DATEI, ART_SM_TOT, ART_ABBILD,
    ART_SPIELSTAND,
    ART_TR_FORTSCHRITT, ART_TR_ZAEHLER, ART_TR_LISTE, ART_TR_LISTE_REST, ART_TR_DATEN,
    ART_AUFNAHMEN, ART_AUFNAHME_UNFERTIG, ART_GALERIE_REST, ART_GALERIE_SONST,
    ART_TOOL_KACHEL, ART_TOOL_DATEN, ART_TOOL_DATEN_WICHTIG, ART_DATEI, ART_PAKET_DATEI,
    ART_REST, ART_FAKE_LIZENZ, ART_APP_DATEN, ART_UNBEKANNT, ART_SPIEL_SICHERUNG,
    ART_JAILBREAK,
    ART_ALTE_SICHERUNG,        /* NEU 1.0.7 (PS5-Test 02.11.2026): Datenbank-Kopien älterer PS5-Cleanup-Versionen (ps5cleanup_db_…) */
    ART_USB_KOPIE,             /* NEU 1.0.8 (PS5-Test 07.11.2026 22:53): eigene Daten, die die Konsole auf ein USB-Laufwerk kopiert hat (PS5/CREATE ...) */
    ART_PROTOKOLL,             /* NEU 1.0.8 (07.11.2026): Protokoll von PS5 Cleanup aus einem früheren Lauf (ps5cleanup_protokoll_…) */
    ART_STORE_FASSUNG          /* NEU 1.0.9 (#145): vorige Fassung einer App samt allem, was darin lag - von ProsperoStore aufgehoben */
};
enum { AMPEL_GRUEN = 0, AMPEL_GELB = 1, AMPEL_ROT = 2 };
/* NEU 1.0.6 (da_Chris86 28.10.2026): Speicherorte. USB = USB-Platte als erweiterter Speicher, USB-Stick = normaler Datenträger */
enum { ORT_INTERN = 0, ORT_M2 = 1, ORT_USB = 2, ORT_STICK = 3 };
int         kern_pfad_ort(const char *p);            /* Ort eines Pfads */
const char *kern_ort_vorsatz(int ort);               /* "M.2 – " usw., intern "" */
const char *kern_ort_name(int ort);                  /* "M.2-SSD" usw. */
static inline bool ist_teil(const eintrag *f, const char *t) { return f->Teil != NULL && strcmp(f->Teil, t) == 0; }

/* class Groesse (Kern.cs) */
typedef struct { long long Bytes; int Dateien; int Ordner; bool Gekappt; bool Lesefehler; } groesse;

/* Selbstschutz der ELF-Version (NEU in 1.1, siehe kern_selbstschutz.c) */
typedef struct {
    bool   aktiv;              /* aus = Kern verhält sich exakt wie 1.0.5 */
    bool   unsicher;           /* eigener Ort nicht feststellbar -> alle Homebrew-Daten gesperrt */
    bool   eigen_gefunden;
    bool   launcher_gefunden;
    sliste pfade;              /* geschützte Pfade samt Inhalt; Ordner darüber werden nie als Ganzes gelöscht */
    sliste launcher_ids;       /* Kennungen des Homebrew Launchers */
    const char *fehler;        /* NEU 1.1 (S3): Ermittlung scheiterte an einem Lesefehler -> Suche bricht ab */
    int    kopien;             /* NEU 1.1 (S2): gefundene Programmdateien mit unserer Kennung */
    sliste eigene;             /* NEU 28.10.2026: was zu PS5 Cleanup selbst gehört (Programmdateien, Daten, Kachel) - wird am Ende entfernt */
    bool   frei;               /* NEU 28.10.2026: gerade läuft die Selbstentfernung - eigene Pfade sind dann nicht gesperrt */
    sliste unlesbar;           /* NEU 1.0.6 Fix (30.10.2026, Playden): Ordner, die die PS5 nicht lesen lässt (privat) - geschützt, still übersprungen */
    sliste diag;               /* NEU 1.0.8 (Arbeitsplan 2.5): nicht lesbar oder zu tief - in DIESER Suche gefunden (fürs Protokoll, je Suche neu) */
    sliste zu_tief;            /* NEU 1.0.8: davon die Ordner, die nur zu tief liegen (SUCH_TIEFE) - je Suche neu */
    sliste zu_viele;           /* NEU 1.0.8 (3.7): davon die Ordner, die wegen der Eintragsgrenze nicht (ganz) geprüft wurden - je Suche neu */
    sliste lese_fehler, lese_code;   /* NEU 1.0.10 (Rückmeldung): nicht lesbare Ordner dieser Suche und der Grund (EACCES ...), gleicher Index - je Suche neu */
} selbstschutz;

#define RM_MAX 16 /* NEU 1.0.10: so viele verschiedene Fehler-Kürzel merkt sich die Rückmeldung */
typedef struct { char k[RM_MAX][32]; int n[RM_MAX]; int z; } rm_zaehler;   /* NEU 1.0.10: Kürzel mit Anzahl (kern_rm_zaehle) */
typedef struct kern {
    arena   *a;                         /* Speicher dieses Kerns */
    sitzung *S;                         /* die "Verbindung" (Zugriffsschicht + Sitzung) */
    void   (*melde)(void *ctx, const char *text);   /* Melde */
    void    *melde_ctx;
    void   (*warte)(void *ctx, int ms);             /* Warte */
    void    *warte_ctx;
    int    (*nach_anhalten)(void);                 /* NEU 1.0.10 (da_Chris86 14.11.2026 11:00): nur die PS5-App setzt es - dann werden ShadowMount und ps4mount vor JEDEM Löschen angehalten und danach alle übrigen nachgeladenen Programme beendet; Rückgabe: wie viele beendet wurden */
    volatile bool abbruch;              /* Abbruch */
    volatile bool anhalten;             /* NEU 1.0.8 (Arbeitsplan 3.4): Löschen nach dem laufenden Eintrag anhalten - wirkt nur zwischen zwei Einträgen */

    sliste ASTRO_IDS;                   /* { BEHALTEN_APP } */
    sliste ASTRO_NPWRS;                 /* { "NPWR20188_00" } */

    /* Schutzliste */
    mtab LINK_CACHE;                    /* ordinal: Elternordner -> dliste* (NULL = gibt es nicht) */
    int  WAR_SCHON_WEG;

    /* Suche (Suche.cs) */
    pliste Funde;                       /* eintrag* */
    bool   m2Da;
    bool   M2_OHNE_USER;
    bool   USB_SPEICHER;
    int    M2_GERAET;                   /* NEU 27.10.2026 (Durchlauf 1 auf da_Chris86s PS5): M.2-Gerät /dev/ssd1 vorhanden? 1 ja, 0 nein, -1 unbekannt (/dev nicht lesbar) */
    bool   M2_UNKLAR;                   /* NEU 1.1: M.2 nicht (richtig) erkannt - seit 27.10.2026 21:39 nur noch für Texte, sperrt nichts */
    bool   FUNDE_GUELTIG;               /* statt FUNDE_IP: erst gültig, wenn die Suche ganz durch ist */
    bool   STICK_DA;                    /* NEU 1.0.6: mindestens ein normaler USB-Stick (/mnt/usbN) lesbar */
    bool   MERKE_STICK;                 /* NEU 1.0.6: gerade wird ein USB-Stick gelistet -> Einträge nur einzeln */
    sliste KOMPRIMIERUNG;
    sliste AUFGEKLAPPT;
    mtab   LEER_DANACH;                 /* ci: Gruppe -> sliste* */
    sliste SHARES;
    bool   MERKE_OK;
    pliste bau;                         /* eintrag* */
    mtab   TITEL_CACHE;                 /* ci: id -> titelinfo* */
    const char *APP_ROH;
    int    APP_ART;                     /* NEU 1.0.7: 0 unbekannt, 1 Spiel, 2 App (Sony: applicationCategoryType bzw. CATEGORY) */
    mtab   NAMEN_CACHE;                 /* ci: id -> const char* */
    mtab   NPWR_NAMEN;                  /* ci */
    mtab   NPWR_KACHEL;                 /* NEU 1.0.7: Trophaeen-Kennung -> Kennung der Kachel (fuer das Kachelbild in der Oberflaeche) */
    mtab   NPWR_NR;                     /* NEU 1.0.7 (PS5-Test 02.11.2026): Trophäen-Kennung -> "Nr" (1, 2 ...), nur wenn das Spiel mehrere Listen hat */
    const char *EIGENES_PROTOKOLL;      /* NEU 1.0.7 (02.11.2026, aus Plan 1.0.8 4a vorgezogen): Protokoll-Datei, in die PS5 Cleanup gerade schreibt
                                         * (USB-Stick) - wird nie angeboten, sonst ließe sie sich in einem zweiten Durchgang mitlöschen */
    const char *PROTOKOLL_JETZT;        /* NEU 03.11.2026: Protokoll-Datei dieses Laufs (USB-Laufwerk, intern oder M.2) - bleibt immer */
    sliste TEST_ORDNER;                 /* NEU 1.0.8 (Arbeitsplan 2.7, Codex-Prüfung 74): Ordner der Test-Fassung (Mitschnitt, Bilder) an allen möglichen
                                         * Orten - nie angeboten, nie gelöscht, auch kein Ordner darüber (kern_test_ordner). Leer in allen anderen Fassungen */
    bool   PROTOKOLLE_FREI;             /* NEU 03.11.2026: nur während kern_alte_protokolle_weg - alte Protokolle auf der Konsole löschbar */
    mtab   NPWR_GALERIE;                /* ci */
    mtab   GALERIE_NAMEN;               /* ci */
    /* NEU 1.0.7 (PS5-Test 02.11.2026): Namen und Zuordnung von Resten - nur aus Metadaten, nie aus ähnlichen Kennungen oder Namen */
    mtab   VERKNUEPFT;                  /* ci: Kennung -> installiertes Spiel, das sie als dasselbe Spiel nennt (Zusatzinhalt-Kennung; "" = widersprüchlich) */
    mtab   VERK_SPST;                   /* ci: Kennung -> installiertes Spiel, dessen Spielstände darunter liegen (INSTALL_DIR_SAVEDATA; "" = widersprüchlich) */
    mtab   VERK_BELEG;                  /* ci: Kennung -> Beleg (z. B. "INSTALL_DIR_SAVEDATA von PPSA04264") */
    mtab   INSTALLIERT;                 /* ci: installierte Kennungen mit eigenen Angaben (während der Suche) */
    mtab   SID_KAPUTT;                  /* ci: Kennung -> Anzahl kaputter Zusatzinhalt-Angaben (SERVICE_ID_ADDCONT_ADD_n / serviceIdForSharing) */
    mtab   SID_GEZAEHLT;                /* "Kennung|Feld" schon als kaputt gezählt (param.sfo, param.json und appinfo.db nennen dieselben Felder) */
    sliste SID_WARNUNG;                 /* Warnzeilen für das Protokoll */
    mtab   REST_NAMEN;                  /* ci: Kennung -> Name aus verbliebenen Angaben (Begleitdatei der Aufnahmen, Spielstand) */
    /* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86: Spielstand "CUSA09172 (Spiel/App unbekannt)" und zwei Trophäen-Listen ohne Namen, obwohl
     * L.A. Noire und Red Dead Redemption 2 vorher installiert waren). Weitere Belege für Namen - alle nur gelesen, PS5 Cleanup speichert
     * dafür nichts (da_Chris86 13:32: "keine Daten speichern auf der PS5, außer die Logs"): */
    mtab   PROT_NAMEN;                  /* ci: Kennung -> Name aus früheren Protokollen von PS5 Cleanup (Liste der PS4-Spiele, gelöschte Spiele) */
    mtab   PROT_NPWR;                   /* ci: NEU 1.0.8 - Trophäen-Kennung -> Name aus früheren Protokollen (gelöschte Trophäen-Listen und Fortschritte) */
    mtab   SPST_TITEL;                  /* ci: Kennung -> Titel der Spielstände laut Sonys Spielstand-Liste (savedata.db), "" = uneinheitlich */
    mtab   PKG_NAMEN;                   /* ci: Kennung -> Name aus dem Kopf einer gefundenen PKG-Datei (Spiel, Update, App - nie Zusatzinhalt) */
    mtab   PKG_TRP;                     /* Kopf der Trophäen-Datei im PKG (96 Byte als Hex) -> Name des Spiels */
    mtab   NAMEN_OFFEN;                 /* ci: Kennungen, die beim Anlegen der Einträge ohne Beleg "unbekannt" blieben (nicht bei Widerspruch) */
    mtab   NPWR_OFFEN;                  /* ci: Trophäen-Kennungen, die beim Anlegen "unbekannt" blieben */
    mtab   NPWR_PKG;                    /* ci: Trophäen-Kennung -> Name über dieselbe Trophäen-Datei in einer PKG-Datei (nach der Suche) */
    mtab   PKG_TRP_NR;                  /* ci: NEU 1.0.8 - Kopf der Trophäen-Datei (Hex) -> ihre Nummer im PKG (1, 2 ...), nur bei mehreren Listen */
    /* NEU 1.0.8 (PS5-Test 07.11.2026 16:31, da_Chris86: "bei den PS4-Spielen sind die Bilder und die Beschriftung da, bei den PS5-Spielen
     * nicht"): PS5-Spiele ohne Kachel bekommen Name und Bild aus Sonys eigenen Angaben, die noch auf der Konsole liegen */
    mtab   NPWR_UCP;                    /* ci: Trophäen-Kennung -> Name aus der PS5-Trophäen-Liste selbst (TROPHY.UCP), "" = keiner */
    mtab   BILD_ORT;                    /* ci: Kennung -> "Pfad\nAnfang\nLänge" von icon0.png einer Spieldatei (.exfat) oder eines Spielordners
                                         *     ohne Kachel; Länge 0 = ganze Datei. Liest nur die Oberfläche (kachelbild_laden). NEU 1.0.9: auch aus
                                         *     einer PS4-PKG-Datei (icon0.png) */
    mtab   BILD_UPD;                    /* NEU 1.0.9: ci: Kennung -> (void *)1, wenn BILD_ORT aus einer Update-PKG stammt (die Spiel-PKG geht vor) */
    int    PRIVAT_ORTE;                 /* NEU 1.0.9: so viele private Orte von Programmen fand die letzte Suche (kern_privat.c) */
    void (*appinfo_lesen)(struct kern *k, void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert));
                                        /* Plattform: Sonys appinfo.db NUR LESEN und jede Zeile INSTALL_DIR_SAVEDATA / SERVICE_ID_ADDCONT_ADD_n
                                         * melden (NULL = nicht verfügbar - dann nur param.sfo / param.json). NEU 1.0.8: dazu aus Sonys
                                         * Spielstand-Liste (savedata.db) je Spielstand eine Zeile mit dem Feld SPIELSTAND_TITEL (Wert = Titel).
                                         * Spielstände, deren Titel gleich ihrem Untertitel ist, meldet die Plattform nicht (L.A. Noire:
                                         * "Benutzereinstellungen & Freischaltobjekte" - das ist kein Spielname) */
    sliste SpielQuellen;
    bool   BD_DISC;             /* NEU 1.0.6 (da_Chris86 29.10.2026 19:12): Blu-ray-Jailbreak-Disc im Laufwerk (nur gelesen) */
    sliste JB_QUELLEN;          /* NEU 29.10.2026 (Codex-Prüfung 38): Spieldateien, auf die eine Kachel eines Jailbreak-Einstiegs zeigt */
    mtab   QuelleInfo;                  /* Quelle -> qinfo* (seit 27.10.2026 Groß/klein genau, Codex-Prüfung 14) */
    sliste SperrQuellen;                /* NEU 27.10.2026: Spieldateien, die nicht sicher zugeordnet sind (vermutet/mehrfach) - nie löschen, auch nicht über einen Elternordner */
    mtab   ORDNER_CACHE;                /* ci: Ordner -> sliste* */

    /* Größen (Ausfuehren.cs) */
    mtab   PFAD_GROESSE;                /* ordinal: Pfad -> groesse* (nur unter groessen_sperre, Speicher in ga) */
    arena *ga;                          /* eigener Speicher für PFAD_GROESSE (auch aus dem Hintergrund-Zählen) */
    void  *groessen_sperre;             /* pthread_mutex_t* */
    bool   GROESSE_GEKAPPT;
    void (*GroesseStand)(void *ctx, long long b, int n);   /* Zwischenstand beim Zählen vor dem Löschen */
    void  *groesse_ctx;

    /* Ausführen / ShadowMount */
    bool   SM_ANGEHALTEN;
    bool   KOMPLETT_LAUF;               /* NEU 1.0.10 (da_Chris86 14.11.2026 12:18): der laufende Löschlauf ist die vollständige Bereinigung - am Ende startet die PS5 neu */
    bool   PROG_BEENDET;                /* NEU 1.0.10: in diesem Löschlauf wurden Programme im Hintergrund beendet (Hinweis am Ende) */
    bool   SM_STOP_GESENDET;
    const char *SM_GRUND;
    void (*Fortschritt)(void *ctx, int nr, int anzahl, const eintrag *f);   /* nur Anzeige */
    void  *fortschritt_ctx;
    int    verbindung_nr;               /* wechselt bei kern_verbinden: Liste gilt nur für dieselbe "Verbindung" */
    int    PKG_GELISTET;                /* NEU 1.0.8 (Codex-Prüfung 83): Einträge, die die Suche nach PKG-Dateien in Ordnern gelistet hat (Grenze je Suche) */
    int    PKG_GEFUNDEN;                /* NEU 1.0.8 (Codex-Prüfung 96): PKG-Dateien, die sie in Ordnern gefunden und einzeln angelegt hat (Grenze je Suche) */
    int    funde_verbindung_nr;         /* statt FUNDE_IP */

    selbstschutz SCHUTZ;                /* NEU 1.1 */
    /* NEU 1.0.7 (da_Chris86 03.11.2026 19:44-19:46): freiwillige QR-Rückmeldung - die Fehler des letzten Löschlaufs als Kürzel
     * "schritt.art.ort.code" (z. B. "sony.ps4.i.0x80990001"). Nur Kürzel aus festen Listen und der reine Fehlercode - keine Namen,
     * keine Kennungen, keine Pfade. RM_ERFASSEN nur während des Löschens (ablauf.c). */
    bool   RM_ERFASSEN;
    pliste *GELOESCHT;                  /* NEU 1.0.10 (Codex-Prüfung 172): während kern_fuehre_loeschung - Einträge, die dieser Lauf wirklich gelöscht hat */
    char   RM_SCHRITT[12], RM_CODE[20];
    bool SonyAbgelehnt;                 /* NEU 1.0.10 (Codex-Prüfung 171): letzter Sony-Auftrag wurde ausdrücklich abgelehnt (553), nichts angenommen */
    const char *RM_HINWEIS;             /* Schritt, der gerade fragt ("sony", "med", "ss") - NULL = aus dem Text erkennen */
    char   RM_FEHLER[RM_MAX][48];      /* NEU 1.0.10: gleiche Fehler zusammengefasst (RM_ANZ = wie oft), bis 1.0.9 die ersten 5 */
    /* NEU 1.0.10 (da_Chris86 10.11.2026 18:15, Punkte 5 und 7): zu welchem Programm die Fehler gehören (Kürzel aus fester Liste, kern_rm_programm)
     * und was geklappt hat ("weg.art", z. B. "sony.ps5") - beide zusammengefasst mit Anzahl */
    rm_zaehler RM_PROG, RM_OK;
    int    RM_ANZ[RM_MAX];
    int    RM_FEHLER_N;
    volatile int FEHLER_ZAHL;           /* NEU 1.0.8 (Codex-Prüfung 76): fehlgeschlagene Einträge dieses Löschlaufs, ohne Grenze (Seite "Löschen läuft") */
    volatile long long FREI_BISHER;     /* NEU 1.0.8 (Codex-Prüfung 76): in diesem Lauf wirklich frei geräumt, vor dem laufenden Eintrag (Byte) */
    bool   simulation;                  /* NEU 1.1: Zugriff ist eine Simulation -> ShadowMount nie wirklich anhalten */
    bool   SONY;                        /* NEU 1.0.6: Sonys Deinstallations-Dienst ist da (PS5) - Kacheln gehen über Sony */
    bool   MEDIEN;                      /* NEU 1.0.7 (01.11.2026): Sonys Löschdienst für Bilder/Videos ist da (PS5) - Aufnahmen nur darüber */
    bool   SPIELSTAENDE;                /* NEU 1.0.7 (01.11.2026): Sonys Spielstand-Liste kann ausgetragen werden (PS5) - Spielstände nur so */
    int    SYSEX; const char *SYSEX_GRUND;   /* NEU 1.0.7: /system_ex für leere Einhängeordner beschreibbar? 0 = noch nicht gefragt, 1 = ja, -1 = nein (Grund) */
    bool   SELBST_UEBERGEBEN;           /* NEU 1.0.6: Kachel + Startordner entfernt der Hintergrundteil nach dem Ende der Oberfläche */
    bool   SELBST_DIAG;                 /* NEU 1.0.6: während der Selbstentfernung jede Meldung auch ins Diagnoseprotokoll */
    sliste GEISTER;                     /* NEU 1.0.6: Namen der Kacheln, die nach dem Löschlauf im Menü stehen bleiben (Sony lehnte ab) */
    bool   DB_HINWEIS;                  /* NEU 1.0.6: am Ende "Datenbank neu aufbauen" zeigen (ohne Sony immer, mit Sony nur bei GEISTER) */
    pliste PS4;                         /* NEU 1.0.7 (da_Chris86 31.10.2026 19:56): installierte PS4-Spiele (ps4_spiel*) - kern_ps4_erfassen */
    mtab   PS4_WEG;                     /* NEU 1.0.7: "Basis|CUSAxxxxx" der PS4-Spiele, die in diesem Lauf gewählt sind (dürfen weg) */
    bool   PS4_UNKLAR;                  /* NEU 1.0.7: Inventar ließ sich nicht erstellen -> jeder PS4-Pfad eines nicht gewählten Spiels bleibt */
    int    PS4_GESCHUETZT;              /* NEU 1.0.7: so viele Einträge hat der PS4-Schutz im letzten Lauf zurückgehalten */
    /* NEU 1.0.10 (da_Chris86 12.11.2026 15:12-15:27): einzelne Playden-Spiele ganz entfernen, Playden und ps4mount vorher beenden */
    sliste PL_SPIELE, PL_SCHLUESSEL, PL_ORDNER;   /* je Suche: Kennung FAKE78xxx, Playdens Schlüssel (Steam-App-Nummer, "" = keiner), Spieldateien */
    sliste PL_DATEN;                              /* NEU 1.0.10 (Codex-Prüfung 169 (1)): dazu je Spiel der Ordner seiner Spielstände (data_root, "" = keiner) */
    int    PL_BEHALTEN;                           /* Codex-Prüfung 163 (1): Speicher, die die vollständige Bereinigung behält (Bit 1<<ORT_*) - nie mitwählen */
    mtab   PL_GESPERRT;                           /* NEU 1.0.10 (PS5-Test 12.11.2026 19:49): Löschlauf - Playden-Spiele, die sich nicht sicher austragen ließen (Kennung -> Grund): alles davon bleibt */
    int    PL_ZU; const char *PL_GRUND;           /* Löschlauf: 1 = Playden ist sicher zu, 0 = nicht sicher (Grund) - dann bleibt alles von Playden */
    int    P4M; const char *P4M_GRUND;            /* Löschlauf: 1 = ps4mount angehalten oder läuft nicht, 0 = läuft vielleicht noch (Grund) */
    sliste UNVERAENDERT;                /* NEU 1.0.7 (21:50): Einträge, die wegen unklarer Einbindung unverändert blieben (Text + Grund) */
    /* NEU 1.0.10 (da_Chris86 10.11.2026): Datenbank-Übersicht - Sonys Titel-Liste (app.db) gegen das, was auf der Konsole liegt (kern_appdb.c) */
    mtab   APPDB;                       /* ci: Kennung -> Angaben aus app.db ("u=..;l=..;t=..;h=..;x=..;g=..;c=..;n=Name"), je Suche neu */
    mtab   APPDB_KACHEL;                /* NEU 1.0.10: Kennungen der Einordnung 'k' (Kachel ohne Dateien, kern_appdb.c) - je Suche neu */
    sliste APPDB_ZEILEN;                /* Protokoll: Kopfzeile mit Zählern, dann je nicht installiertem Titel eine Zeile */
    char   APPDB_RM[48];                /* Rückmeldung: "i12.p1.x0.w0.v3.u0" (installiert, Platzhalter, Laufwerk fehlt, widersprüchlich, ohne Installation, unklar) */

    /* Konsole (Kern.cs): Modell und Laufwerks-Kopplung - nur gelesen */
    const char *MODELL_TYP;             /* "fat" / "slim" / "pro" / "" */
    bool   MODELL_DIGITAL;
    bool   DDD_BEKANNT;                 /* DDD_STATUS != null */
    int    DDD_STATUS;
} kern;

/* class Ergebnis (Ausfuehren.cs) */
typedef struct { bool Abgebrochen; int Fehler; bool SmGestoppt; groesse Frei; const char *SmText; bool Verloren;
                 int Teilweise;   /* NEU 1.0.6: Dateien gelöscht, aber die Kachel bleibt im Menü (Sony lehnte ab) - gilt nicht als ganz entfernt */
                 int Geschuetzt;  /* NEU 1.0.7: vom PS4-Schutz zurückgehalten (kein Fehler) */
                 int Unveraendert;  /* NEU 1.0.7 (21:50): wegen unklarer Einbindung unverändert gelassen (kein Fehler) */
                 bool Angehalten; int Rest;   /* NEU 1.0.8 (Arbeitsplan 3.4): auf Wunsch angehalten - so viele Einträge blieben unverändert */
               } ergebnis;
typedef enum { SM_ANGEHALTEN_Z, SM_LAEUFT_NICHT, SM_UNBEKANNT } smzustand;   /* SmZustand */

kern *kern_neu(zugriff *z);
void  kern_frei(kern *k);
void  kern_melde(kern *k, const char *text);

/* ---- NEU 1.0.7 (da_Chris86 31.10.2026 19:56): Schutz installierter PS4-Spiele (kern_ps4.c) ----
 * Hintergrund (31.10.2026, PS5 Pro FW 12.00): Sonys "Datenbank neu aufbauen" hat installierte PS4-FPKG-Spiele auf der internen SSD
 * gelöscht (app.pkg/patch.pkg weg, Ordner leer). Belegt auch in elf-arsenal (README) und ps5-db-backup (README): intern gelöscht,
 * M.2 bleibt, USB-Speicher nur, wenn abgesteckt. PS5 Cleanup darf solche Spiele nie unbeabsichtigt verlieren lassen. */
typedef struct {
    const char *id, *name, *basis;      /* basis: "/user", "/mnt/ext1/user" (M.2), "/mnt/ext0/user" (USB-Speicher) */
    int ort;                            /* ORT_INTERN / ORT_M2 / ORT_USB */
    bool unklar;                        /* Ordner nicht lesbar, oder kein app.pkg aber andere Dateien -> gilt als installiert (geschützt) */
    bool app_da; long long app_bytes;   /* installierte app.pkg (> 0 Byte) */
    bool patch_da; long long patch_bytes; const char *version;
    int dlc;                            /* Zusatzinhalte (Ordner unter addcont/<ID>) */
    sliste lizenzen;                    /* Dateinamen in /user/license mit dieser Kennung */
    bool fake;                          /* mindestens eine Lizenz "fake..." */
    int eingetragen;                    /* im PS5-Menü eingetragen: 1 ja, 0 nein, -1 unbekannt (nur die echte PS5-Fassung fragt Sony) */
} ps4_spiel;
void        kern_ps4_erfassen(kern *k);               /* nur lesen: k->PS4 neu aufbauen */
bool        kern_ps4_installiert(const ps4_spiel *s); /* app.pkg da oder unklar */
const char *kern_ps4_zuordnung(arena *a, const char *p, const char **basis);   /* Kennung, zu deren Installation p gehört (NULL = keine); *basis NULL = mehrdeutig */
void        kern_ps4_weg_setzen(kern *k);             /* PS4_WEG aus der Auswahl (gewählte Einträge "Spiel") */
bool        kern_ps4_geschuetzt(kern *k, const char *p, const char **grund);   /* true = p gehört zu einem installierten, NICHT gewählten PS4-Spiel */
const char *kern_ps4_ort_text(int ort);
const char *kern_ps4_zeile(arena *a, const ps4_spiel *s);   /* "Name (CUSAxxxxx) – intern – 79,4 GB – Update 1.32 – Fake-Lizenz" */
const char *kern_abbild_titel_test(kern *k, const char *p, size_t *gelesen);   /* NEU 1.0.8: nur für die PC-Tests (Spiel-Abbild, kern_suche.c) */
const unsigned char *kern_ufs_datei_test(kern *k, const char *p, const char *innen, size_t *n);   /* NEU 1.0.9: nur für die PC-Tests (UFS2-Leser) */
/* NEU 1.0.9 (#144, kern_entpacken.c): zlib entpacken und gepackte Dateien in PFS-Abbildern (.ffpfsc, PFSC-Behälter) stückweise lesen - nur lesen */
long kern_entpacken(const unsigned char *ein, size_t en, unsigned char *aus, size_t amax, bool zlib_rahmen);   /* -1 = kaputt */
typedef struct {
    bool (*lies)(void *c, unsigned long long ab, size_t n, unsigned char *ziel); void *c;
    unsigned long long ab, gr;                     /* Behälter in der Datei: Anfang, gespeicherte Länge */
    unsigned long long tab, daten, laenge, bloecke;  /* Tabelle, Daten (ab Behälteranfang), ungepackte Länge, Zahl der 64-KB-Blöcke */
    long long im_puffer; unsigned char *puffer, *roh;
} kern_pfsc;
bool kern_pfsc_auf(kern_pfsc *p, arena *a, bool (*lies)(void *c, unsigned long long ab, size_t n, unsigned char *ziel), void *c,
                   unsigned long long ab, unsigned long long gr);
bool kern_pfsc_lies(kern_pfsc *p, unsigned long long ab, size_t n, unsigned char *ziel);
unsigned char *kern_pfsc_datei_stueck(const char *datei, unsigned long long cab, unsigned long long cgr, unsigned long long ab, size_t n);   /* Oberfläche: malloc, NULL = nicht lesbar */
const unsigned char *kern_pfs_datei_test(kern *k, const char *p, const char *innen, size_t *n);   /* nur für die PC-Tests (PFS-Leser) */
/* NEU 1.0.9 (Codex-Prüfung 130): BILD_ORT zerlegen - "Pfad\nAnfang\nLänge", gepackt (.ffpfsc) mit der Zeile "pfsc <Anfang> <Länge>" des
 * Behälters; NEU #149: in Stücken mit der Zeile "teile <n> <Anfang> <Länge> ..." (2 bis KERN_BILD_TEILE Stücke im Abbild, das erste bei
 * Anfang, zusammen Länge). Für die Oberfläche (app.c spiel_bild), im Test geprüft. false = nicht genau in dieser Form */
#define KERN_BILD_TEILE 64
typedef struct { char pfad[1000]; unsigned long long ab, len, cab, cgr; bool gepackt; int teile_n; unsigned long long teile[KERN_BILD_TEILE][2]; } kern_bild_ort;
bool kern_bild_ort_lesen(const char *o, kern_bild_ort *b);
unsigned char *kern_bild_lesen(const char *datei, const kern_bild_ort *b);   /* NEU #149: das Bild nach b aus datei (am Stück, gepackt, in Stücken); malloc, NULL = nicht lesbar */
const char *kern_pkg_zusatz_test(kern *k, const char *p, size_t *gelesen);   /* NEU 1.0.8: nur für die PC-Tests (PKG-Prüfung, kern_suche.c) */
const char *kern_rest_text(kern *k, arena *a);              /* NEU 1.0.7: was im Menü bleibt, dazu die Bitte ums Protokoll ("" = nichts) */
extern int (*kern_ps4_sony_eingetragen)(const char *id);
/* NEU 1.0.10 (kern_appdb.c): Datenbank-Übersicht, nur lesen */
char kern_appdb_einordnen(arena *a, const char *id, const char *wert, int sony, bool installiert, bool ext_da, const char **text);   /* s p i x w v u */
void kern_appdb_uebersicht(kern *k);   /* nach der Suche: APPDB_ZEILEN und APPDB_RM aus APPDB */
/* app.db/appinfo.db nur lesen und prüfen: db/db_lesen.c (die Sicherung vom 31.10.2026 ist seit 1.0.8 ausgebaut) */

/* ---- NEU 1.0.7 (da_Chris86 31.10.2026 21:50, Punkt 4): Geisterkacheln - Sonderbehandlung nur für belegte Fälle (kern_geister.c) ----
 * GEIST_NORMAL:       /system_ex/app/<ID> ist keine Einbindung -> wie 1.0.6: ein Sony-Auftrag, Ergebnis prüfen (so lief der echte Test
 *                     auf FW 12.00 am 31.10.2026 sauber, auch für ShadowMount+-Kacheln nach dem Anhalten von ShadowMount+).
 * GEIST_PS5UPLOAD:    PSUP00001 mit der nullfs-Einbindung von ps5upload (/data/ps5upload/tile/PSUP00001) -> Einbindung lösen,
 *                     Sony-Auftrag, prüfen, höchstens zwei weitere Versuche (Vorgehen aus ps5upload register.c unregister_title).
 * GEIST_EIGENES_SPIEL: NEU 1.0.7 (da_Chris86 01.11.2026 23:19, PS5-Test 23:13): nullfs-Einbindung genau des Spielordners, den die Kachel
 *                     selbst nennt (/user/app/<ID>/mount.lnk bzw. mount_img.lnk) - so hängt kstuff Ordner-Spiele auf der M.2 ein, sobald
 *                     ShadowMount+ angehalten ist -> Einbindung lösen (nur die Ansicht, der Spielordner bleibt), ein Sony-Auftrag, prüfen.
 * GEIST_UNVERAENDERT: jede andere Einbindung (ShadowMount+, kstuff, unbekannt) oder Zustand nicht feststellbar -> der ganze Eintrag
 *                     bleibt unverändert (keine Deinstallation, keine Datei gelöscht). */
enum { GEIST_NORMAL = 0, GEIST_PS5UPLOAD = 1, GEIST_UNVERAENDERT = 2, GEIST_EIGENES_SPIEL = 3, GEIST_HINTERLASSEN = 4 };
/* NEU 1.0.10 (da_Chris86 14.11.2026 14:28, PS5-Test 14:07 Kraken Mount/Spider-Man 2): GEIST_HINTERLASSEN - die nullfs-Einbindung eines
 * Hilfsprogramms, das in diesem Löschlauf schon beendet ist (alle nachgeladenen Programme sauber beendet, kern_geist_hinterlassen(true)):
 * sie gehört keinem laufenden Programm mehr -> Einbindung lösen (nur die Ansicht /system_ex/app/<ID>), ein Sony-Auftrag. Universell,
 * ohne Programmnamen. Quellen unter /mnt/shadowmnt (ShadowMount) bleiben wie bisher unverändert. */
void kern_geist_hinterlassen(bool an);
/* eingehaengt: 1 = /system_ex/app/<ID> ist ein eigener Einhängepunkt, 0 = nein, -1 = nicht feststellbar;
 * eigene_quelle: Spielordner, den die Kachel selbst nennt (mount.lnk / mount_img.lnk), oder NULL */
int kern_geist_regel(const char *id, int eingehaengt, const char *fstyp, const char *quelle, const char *eigene_quelle, const char **grund);

/* ---- Schutzliste (Kern.cs) ---- */
bool kern_astro_pfad(kern *k, const char *p);        /* AstroPfad */
bool kern_erlaubt(kern *k, const char *p);           /* Erlaubt */
bool kern_test_ordner(const kern *k, const char *p);  /* NEU 1.0.8: p ist ein Ordner der Test-Fassung, liegt darin oder enthält ihn */
bool kern_laufende_aufnahme(const char *p);
bool kern_nie_anfassen(const char *p);                /* NEU 1.1 (S5): Systembereiche, Konten, Lizenzen, Sony-Apps */           /* NEU 1.1 (U16): sharevideo_…_hdr.webm der PS5 */
bool kern_ohne_verknuepfung(kern *k, const char *p); /* OhneVerknuepfung */

/* ---- Suche (Suche.cs) ---- */
#define KAT_ANZ 7                                     /* NEU 1.0.7 (da_Chris86 31.10.2026): 7 Bereiche - Nummern intern fest:
                                                       * 1 Spiele, 2 Spielstände, 3 Trophäen, 4 Bilder & Videos, 5 Sonstiges, 6 Apps, 7 Jailbreak;
                                                       * angezeigt in der Reihenfolge ALLE_KAT (Spiele, Apps, Spielstände, ..., Jailbreak) */
const int *kern_alle_kat(void);                       /* NEU 1.0.7: Anzeige-Reihenfolge der Bereiche */
#define ALLE_KAT (kern_alle_kat())
bool kern_bereiche_106(void);                          /* NEU 1.0.7: nur PC-Tests - Einteilung wie 1.0.6 für die Vergleiche mit 1.0.5 */
const char *kern_kat_name(int k);                    /* KatName[k] */
const char *kern_kat_info(int k);                    /* KatInfo[k] */
const char *kern_json_titel(arena *a, const char *t);/* JsonTitel */
mtab        kern_sfo_lesen(arena *a, const unsigned char *b, size_t n);   /* SfoLesen: ci-Tabelle -> sfowert* */
const char *kern_sfo_titel(arena *a, const mtab *h); /* SfoTitel */
const char *kern_sfo_wert(arena *a, const mtab *h, const char *schluessel);   /* NEU 1.0.7: Wert als Text ("" = fehlt) */
void        kern_abhaengige_mitnehmen(kern *k);      /* AbhaengigeMitnehmen */
const char *kern_speicher_hinweis(kern *k);          /* SpeicherHinweis */
void        kern_suche_funde(kern *k);               /* SucheFunde - wirft bei Fehlern wie 1.0.5 */
const char *kern_gesperrt_text(void);                /* GESPERRT_TEXT */
const char *kern_mehrfach_text(void);                /* NEU 1.1 */
const char *kern_mehrfach_grund(void);               /* NEU 1.1 */
const char *kern_nur_einzeln_text(void);
bool        kern_jb_einstieg(const char *id);   /* NEU 1.0.6/1.1 */
const char *kern_jb_grund(void);             /* NEU 1.1 */
const char *kern_jb_hinweis(arena *a, const eintrag *f);   /* NEU 1.1; 1.0.6 mit dem Namen des Jailbreaks */
const char *kern_jb_warnung(arena *a, const eintrag *f);   /* NEU 1.0.6 (da_Chris86 29.10.2026): Warnung beim Anwählen */
const char *kern_jb_weg(const char *pfad);                 /* NEU 1.0.6: "Y2JB (YouTube)", "Lua-Jailbreak" ... oder NULL */
const char *kern_jb_weg_eintrag(const eintrag *f);         /* NEU 1.0.6 */
bool        kern_jb_abbild(const char *pfad);              /* NEU 1.0.6 (Codex-Prüfung 37): Spiel-Abbild eines Jailbreak-Einstiegs (Kennung im Namen) */
/* NEU 1.0.6 (da_Chris86 28.10.2026 23:16): eingerichteter Jailbreak-Einstieg laut letzter Suche ("Y2JB (YouTube)" ...), NULL = keiner.
 * *warnung: welche Einträge man nicht löschen darf, wenn man ihn behalten will. Speicher aus a. */
const char *kern_jailbreak_erkannt(kern *k, arena *a, const char **warnung);
const char *kern_jailbreak_kuerzel(kern *k, arena *a);   /* NEU 1.0.7: "y2jb,netflix,..." für die QR-Rückmeldung (keine Namen) */
void kern_rm_zeile(kern *k, const char *t);              /* NEU 1.0.7: Fehlercode und Schritt aus einer Protokollzeile merken (QR-Rückmeldung) */
void kern_rm_fehler_dazu(kern *k, const char *schritt, const char *art);   /* NEU 1.0.7: Fehler außerhalb eines Eintrags (Selbstentfernung) */
const char *kern_fehler_kurz(const char *t);                 /* NEU 1.0.10: Systemmeldung -> Kürzel (EACCES ...), NULL = keins */
int  kern_rm_unlesbar(kern *k, char *aus, size_t n);
void kern_rm_zaehle(rm_zaehler *z, const char *k);                 /* NEU 1.0.10: Kürzel zählen (neu oder +1), höchstens RM_MAX verschiedene */
int  kern_rm_zaehler_text(const rm_zaehler *z, char *aus, size_t n);   /* NEU 1.0.10: "k*n~k*n", häufigste zuerst */
const char *kern_rm_programm(kern *k, const eintrag *f);            /* NEU 1.0.10: Programm eines Eintrags als Kürzel (playden, pstore, sm ... hb, spiel, sonst) */
const char *kern_lese_grund(kern *k, const char *p);        /* NEU 1.0.10: Grund eines nicht lesbaren Ordners der letzten Suche (EACCES ...), NULL = unbekannt */        /* NEU 1.0.10: nicht lesbare Ordner der letzten Suche als "bereich.grund*anzahl~..." (ohne Pfade) */
const char *kern_app_daten(kern *k, const char *id);   /* NEU 1.1: /user/download/<ID>, wenn er mit der App gehen darf, sonst NULL */
const char *kern_sm_ohne_m2(void);                   /* NEU 1.1 */
const char *kern_m2_unklar_grund(void);              /* NEU 1.1 */

/* ---- Auswahl (Ausfuehren.cs) ---- */
int   kern_anzahl(kern *k, int kat);                 /* Anzahl */
int   kern_anzahl_komplett(kern *k, int kat);        /* AnzahlKomplett */
pliste kern_gewaehlt(kern *k);                       /* Gewaehlt */
bool  kern_hat_pakete(kern *k);                      /* HatPakete */
void  kern_sm_zuruecksetzen(kern *k);                /* SmZuruecksetzen */
void  kern_waehle_komplett(kern *k, bool m2, bool usb);  /* WaehleKomplett - NEU 1.0.6: M.2 und USB je auf Nachfrage */
bool  kern_komplett_dabei(kern *k, const eintrag *f, bool m2, bool usb);  /* NEU 1.0.7: dieselbe Regel wie kern_waehle_komplett (ohne Trophäen-Listen) */
int   kern_komplett_ort(const eintrag *f);                /* NEU 1.0.7: Ort der Daten (ShadowMount-Kachel: intern) */
int   kern_komplett_pakete(kern *k, bool *bedingt);
bool  kern_selbst_gesperrt(const eintrag *f);            /* NEU 1.0.7: PS5 Cleanup selbst (wird zum Schluss entfernt) */       /* NEU 1.0.7: Trophäen-Listen installierter Spiele; bedingt = es gibt Spiele außerhalb der internen SSD */
int   kern_anzahl_jailbreak(kern *k, bool m2, bool usb); /* NEU 1.1 */
void  kern_jailbreak_dazu(kern *k, bool m2, bool usb);   /* NEU 1.1: Jailbreak-Einstiege zur vollständigen Bereinigung dazu */
const char *kern_sm_mit(void);                       /* SM_MIT */
const char *kern_sm_ohne(void);                      /* SM_OHNE */
pliste kern_sm_gewaehlt(kern *k);                    /* SmGewaehlt */
void  kern_setze_sm_spieldateien(kern *k, bool mit); /* SetzeSmSpieldateien */
pliste kern_ext_gewaehlt(kern *k);                   /* NEU 1.0.6: gewählte Einträge auf M.2/USB/USB-Stick (statt M2Gewaehlt) */
int   kern_anzahl_ort(kern *k, int ort);             /* NEU 1.0.6: Einträge eines Speicherorts (für die Fragen) */

/* ---- Größen (Ausfuehren.cs) ---- */
void kern_groessen_zuruecksetzen(kern *k);           /* GroessenZuruecksetzen */
bool kern_pfad_gezaehlt(kern *k, const char *p);     /* PfadGezaehlt */
groesse *kern_eintrag_groesse(kern *k, const eintrag *f, bool wieGefunden);  /* EintragGroesse (NULL = noch nicht alles gezählt) */
groesse *kern_eintrag_groesse_in(kern *k, arena *ar, const eintrag *f, bool wieGefunden);
bool     kern_bytes_je_ort(kern *k, long long aus[4]);       /* NEU 1.0.8 (4.2): Gefundenes je Speicherort, jeder Pfad einmal; false = noch nicht alles gezählt */   /* NEU 1.1: dasselbe, Speicher aus ar */
void kern_groessen_im_hintergrund(kern *k, bool (*stopp)(void *), void *stopp_ctx, void (*fertig)(void *, eintrag *), void *fertig_ctx);
groesse kern_ermittle_groessen(kern *k);             /* ErmittleGroessen */
const char *kern_groesse_text(arena *a, long long bytes);   /* GroesseText */
const char *kern_groesse_satz(arena *a, groesse g);          /* GroesseSatz */
const char *kern_freigeraeumt_text(arena *a, groesse g);     /* FreigeraeumtText */
const char *kern_zahl_n0(arena *a, long long v);           /* {0:N0} wie .NET (NEU 1.1, nur Anzeige) */

/* ---- Löschen / ShadowMount (Kern.cs, Ausfuehren.cs) ---- */
void      kern_verbinden(kern *k, zugriff *z);       /* neue "Verbindung" (Liste muss neu gesucht werden) */
smzustand kern_shadowmount_anhalten(kern *k);        /* ShadowMountAnhalten */
ergebnis  kern_fuehre_loeschung(kern *k, bool (*weiter)(void *), void *weiter_ctx);   /* FuehreLoeschung */

/* ---- Konsole (Kern.cs) ---- */
typedef enum { KOPPLUNG_BESTAETIGT, KOPPLUNG_KEINE, KOPPLUNG_UNKLAR, KOPPLUNG_NICHT_AUSLESBAR } kopplungsart;
const char  *kern_modell_typ(arena *a, const char *m);              /* ModellTyp */
bool         kern_modell_digital(arena *a, const char *m);          /* ModellDigital */
const char  *kern_modell_reihe(const char *typ, bool digital);      /* ModellReihe */
const char  *kern_modell_zeile(arena *a, const char *m);            /* NEU 1.0.6: "CFI-7021 · Europa · seit Nov. 2024" */
const char  *kern_laufwerk_text(kern *k);                           /* LaufwerkText */
kopplungsart kern_kopplungs_stufe(kern *k);                         /* KopplungsStufe */
const char  *kern_kopplungs_text(kern *k);                          /* KopplungsText */
int          kern_schluss_text(const char ***zeilen);               /* SCHLUSS_TEXT */

/* ---- Selbstschutz (NEU 1.1) ---- */
extern const char KERN_KENNUNG[];
void kern_selbstschutz_ermitteln(kern *k);                     /* Launcher + eigene Kopien suchen, Schutz einschalten */
void kern_selbstschutz_eigener_pfad(kern *k, const char *p);   /* Startordner dieses Programms */
bool kern_selbstschutz_gesperrt(kern *k, const char *p);       /* darf NICHT gelöscht werden */
bool kern_selbstschutz_anbieten(kern *k, const char *p);       /* darf in der Liste stehen */
/* NEU 1.0.9 (#146): Daten-Ordner /data/<ordner> einer bekannten App? Name der App (NULL = keine), dazu ihre Kennung und - wenn darin
 * nachweislich eigene Dateien liegen - was ("deine heruntergeladenen Filme und Serien"), sonst NULL (kern_suche.c APP_DATEN) */
bool kern_rezept_aktiv(void);
void kern_rezept_aus(void);     /* NEU 1.0.10: vergessen */
void kern_rezept_frei(kern *k);
bool kern_rezept_genannt(const char *ordner);   /* NEU 1.0.10: nennt irgendein gelesenes Programm diesen Ordner? */ /* NEU 1.0.10: vergessen, wenn es zu dieser Suche gehört (kern_frei) */   /* NEU 1.0.10: Programme ohne Kachel wurden gelesen (kern_suche.c REZ) */
const char *kern_bekannte_app(const char *ordner, const char **id, const char **wichtig);
int kern_bekannte_app_ids(const char *ordner, const char **ids, int max);   /* NEU 1.0.10: alle Kennungen eines bekannten Ordners */
bool kern_ordner_bekannt(const char *name);                       /* NEU 1.0.10: eigener Ordner eines bekannten Programms (Besitzer-Wechsel) */
bool kern_selbstschutz_enthaelt(kern *k, const char *p);       /* Geschütztes liegt darin -> aufklappen */
const char *kern_selbstschutz_hinweis(kern *k, bool komplett);   /* NEU 1.0.8 (F1): komplett = vollständige Bereinigung, sonst eigene Auswahl */
bool kern_selbstschutz_unlesbar(kern *k, const char *p);       /* NEU 1.0.6 Fix: p ist ein privater, nicht lesbarer Ordner (oder liegt darin) */
/* ---- NEU 1.0.9 (da_Chris86 08.11.2026 08:02/08:20): private Daten von Programmen (kern_privat.c, Zugriffsschicht: Besitzer-Wechsel) ---- */
void kern_privat_neu(kern *k);                                  /* Beginn einer Suche: die Zugriffsschicht fängt eine neue Liste an */
void kern_privat_markieren(kern *k);                            /* Ende der Suche: Einträge mit privaten Daten markieren, Protokoll */
void kern_privat_nachher(kern *k, const pliste *gewaehlt);      /* nach dem Löschen: was als Benutzer eines Programms gelöscht wurde */
bool kern_selbst_eigen(kern *k, const char *p);                  /* NEU 28.10.2026: p gehört zu PS5 Cleanup selbst (liegt in/ist ein eigener Pfad) */
bool kern_selbst_da(kern *k, bool m2, bool usb);                                    /* NEU 28.10.2026: liegt noch etwas von PS5 Cleanup selbst auf der PS5? */
int  kern_selbst_entfernen(kern *k, bool m2, bool usb);                             /* NEU 28.10.2026: PS5 Cleanup ganz zum Schluss selbst entfernen (Fehlerzahl) */
int kern_alte_protokolle(kern *k);                     /* NEU 03.11.2026: so viele alte Protokolle von PS5 Cleanup liegen intern (ohne das laufende) */
int kern_alte_protokolle_weg(kern *k);                 /* NEU 03.11.2026: vollständige Bereinigung - alte Protokolle intern löschen; Rückgabe Fehler */
bool kern_altes_protokoll(kern *k, const char *p);     /* NEU 03.11.2026: ist p ein altes Protokoll auf der internen SSD? */

/* ---- Arten, Ampel, kurzer Satz (NEU 1.0.7, kern_art.c) ---- */
void        kern_arten_setzen(kern *k);                     /* am Ende der Suche: Art und Rot je Eintrag */
int         kern_ampel(const eintrag *f);                   /* AMPEL_* */
const char *kern_folge(const eintrag *f);                   /* kurzer Satz neben der Zeile: was passiert danach */
const char *kern_zeile(arena *a, const eintrag *f);         /* Zeile in Alltagssprache (ohne Pfade/Marken) */
const char *kern_art_info(arena *a, const eintrag *f);      /* längerer Text für den Kasten unter der Liste (3 Sätze) */
bool        kern_nur_mit_spiel(kern *k, const eintrag *f);  /* Update/Zusatzinhalt eines noch installierten Spiels: geht nur mit dem Spiel */
const char *kern_endgueltig_satz(kern *k, arena *a);        /* "Danach endgültig weg: 12 Spielstände, ..." ("" = nichts Endgültiges gewählt) */
void        kern_sm_nach_auswahl(kern *k);                  /* gewählte ShadowMount-Kacheln: Spieldatei mit, wenn ihre Zeile gewählt ist */
const char *kern_name(arena *a, const eintrag *f);          /* NEU 1.0.8 (Arbeitsplan 2.6): nur der Name - ohne Ort, ohne "– Kachel", "– Spielstände" ... */
const char *kern_doppelt_text(arena *a, const eintrag *f);  /* NEU 1.0.8 (Arbeitsplan 2.4): " – doppelt: auch installiert auf der M.2-SSD" ("" = nur einmal da) */

/* ---- Blöcke für die Auswahl (NEU 1.0.8, kern_bloecke.c, Arbeitsplan 2.6, Entscheidung da_Chris86 04.11.2026 17:03/17:04) ----
 * Ein Block = eine Kachel (Spiel, App, Tool, Trophäen-Liste ...) mit allen Zeilen, die im selben Bereich darunter stehen. Nur Anzeige:
 * was gelöscht wird, ändert sich dadurch nicht. */
typedef struct {
    eintrag    *kopf;          /* erste Zeile des Blocks (bei einem Block aus einer Zeile: diese Zeile) */
    pliste      teile;         /* alle Zeilen des Blocks: der Kopf zuerst, dann seine Teile */
    long long   bytes;         /* Größe aller Teile zusammen - gilt nur, wenn groesse_da */
    bool        groesse_da;    /* alle Teile des Blocks sind gezählt */
    bool        gekappt;       /* eine der Größen wurde beim Zählen gekappt */
    const char *name;          /* nur der Name (A–Z und die zugeklappte Zeile) */
    bool        ohne_kachel;   /* NEU 1.0.8 (2.3): Tool ohne Kachel - Daten-Ordner mit Payload-Datei ("2 Teile · ohne Kachel") */
    int         gruppe;        /* NEU 1.0.10: 0 = Programm, 1 = einzelne Programmdatei ohne Kachel (steht unter Apps darunter) */
} kblock;
enum { SORT_GROESSTE = 0, SORT_AZ = 1 };
eintrag    *kern_kopf_von(kern *k, const eintrag *f, int kat);   /* Zeile, unter der f im Bereich kat steht (NULL = steht für sich) */
int         kern_bloecke(kern *k, arena *a, int kat, int sortierung, kblock **aus);   /* Blöcke des Bereichs kat; Rückgabe: Anzahl */
int         kern_vergleich_namen(const char *x, const char *y);  /* A–Z: Groß/klein egal, Ä wie A, ß wie ss, Zahlen nach Wert */
const char *kern_folge_kurz(const eintrag *f);               /* NEU 1.0.8 (4.3): kurzes Schildchen der Zeile ("neu installieren", "endgültig weg") */
const char *kern_laufende_gewaehlt(kern *k);   /* NEU 1.0.10 (da_Chris86 14.11.2026 12:18): sonst nie beendete Programme (pldmgr, zftpd), die selbst zum Löschen gewählt sind - sie werden dann auch beendet */
bool kern_pldm_gewaehlt(kern *k);   /* NEU 1.0.10 (da_Chris86 14.11.2026 12:10, PS5-Test: Absturz beim Start nach dem Löschen des Payload Managers): der Payload Manager (Kachel oder Daten) ist gewählt - danach Pflicht-Neustart */
bool kern_db_sicherung(const eintrag *f);   /* NEU 1.0.10 (da_Chris86 14.11.2026 10:40): Sicherungskopie von Sonys App-Datenbank, die ein Programm angelegt hat - nie automatisch gewählt, nur einzeln */
const char *kern_elf_bildschluessel(arena *a, const char *pfad);   /* NEU 1.0.10 (da_Chris86 14.11.2026 13:13: "die bekommen doch, wenn sie entpackt werden, ein Symbol"): Schlüssel in BILD_ORT für das Kachelbild in einer ELF-Datei */
bool kern_selbst_hochgeladen(const eintrag *f);   /* NEU 1.0.10: selbst in die Ablage eines Payload-Verwalters hochgeladen (payloads/local) */

#endif
