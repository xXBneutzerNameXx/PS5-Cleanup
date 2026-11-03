/* PS5 Cleanup 1.1 - gemeinsamer C-Kern
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
} eintrag;
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
    ART_ALTE_SICHERUNG         /* NEU 1.0.7 (PS5-Test 02.11.2026): Datenbank-Kopien älterer PS5-Cleanup-Versionen (ps5cleanup_db_…) */
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
} selbstschutz;

typedef struct kern {
    arena   *a;                         /* Speicher dieses Kerns */
    sitzung *S;                         /* die "Verbindung" (Zugriffsschicht + Sitzung) */
    void   (*melde)(void *ctx, const char *text);   /* Melde */
    void    *melde_ctx;
    void   (*warte)(void *ctx, int ms);             /* Warte */
    void    *warte_ctx;
    volatile bool abbruch;              /* Abbruch */

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
    void (*appinfo_lesen)(struct kern *k, void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert));
                                        /* Plattform: Sonys appinfo.db NUR LESEN und jede Zeile INSTALL_DIR_SAVEDATA / SERVICE_ID_ADDCONT_ADD_n
                                         * melden (NULL = nicht verfügbar - dann nur param.sfo / param.json) */
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
    bool   SM_STOP_GESENDET;
    const char *SM_GRUND;
    void (*Fortschritt)(void *ctx, int nr, int anzahl, const eintrag *f);   /* nur Anzeige */
    void  *fortschritt_ctx;
    int    verbindung_nr;               /* wechselt bei kern_verbinden: Liste gilt nur für dieselbe "Verbindung" */
    int    funde_verbindung_nr;         /* statt FUNDE_IP */

    selbstschutz SCHUTZ;                /* NEU 1.1 */
    /* NEU 1.0.7 (da_Chris86 03.11.2026 19:44-19:46): freiwillige QR-Rückmeldung - die Fehler des letzten Löschlaufs als Kürzel
     * "schritt.art.ort.code" (z. B. "sony.ps4.i.0x80990001"). Nur Kürzel aus festen Listen und der reine Fehlercode - keine Namen,
     * keine Kennungen, keine Pfade. RM_ERFASSEN nur während des Löschens (ablauf.c). */
    bool   RM_ERFASSEN;
    char   RM_SCHRITT[12], RM_CODE[20];
    const char *RM_HINWEIS;             /* Schritt, der gerade fragt ("sony", "med", "ss") - NULL = aus dem Text erkennen */
    char   RM_FEHLER[5][48];
    int    RM_FEHLER_N;
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
    sliste UNVERAENDERT;                /* NEU 1.0.7 (21:50): Einträge, die wegen unklarer Einbindung unverändert blieben (Text + Grund) */

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
const char *kern_ps4_neuaufbau_text(kern *k, arena *a);     /* Folgen eines Datenbank-Neuaufbaus für die erkannten PS4-Spiele ("" = keine) */
const char *kern_rest_text(kern *k, arena *a);              /* NEU 1.0.7: was im Menü bleibt + Folgen eines Neuaufbaus ("" = nichts) */
extern int (*kern_ps4_sony_eingetragen)(const char *id);
/* NEU 1.0.7: Sicherung von app.db/appinfo.db steht seit 31.10.2026 21:50 in db/db_sicherung.c (mit SQLite-Prüfung) */

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
enum { GEIST_NORMAL = 0, GEIST_PS5UPLOAD = 1, GEIST_UNVERAENDERT = 2, GEIST_EIGENES_SPIEL = 3 };
/* eingehaengt: 1 = /system_ex/app/<ID> ist ein eigener Einhängepunkt, 0 = nein, -1 = nicht feststellbar;
 * eigene_quelle: Spielordner, den die Kachel selbst nennt (mount.lnk / mount_img.lnk), oder NULL */
int kern_geist_regel(const char *id, int eingehaengt, const char *fstyp, const char *quelle, const char *eigene_quelle, const char **grund);

/* ---- Schutzliste (Kern.cs) ---- */
bool kern_astro_pfad(kern *k, const char *p);        /* AstroPfad */
bool kern_erlaubt(kern *k, const char *p);           /* Erlaubt */
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
groesse *kern_eintrag_groesse_in(kern *k, arena *ar, const eintrag *f, bool wieGefunden);   /* NEU 1.1: dasselbe, Speicher aus ar */
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
bool kern_selbstschutz_enthaelt(kern *k, const char *p);       /* Geschütztes liegt darin -> aufklappen */
const char *kern_selbstschutz_hinweis(kern *k);
bool kern_selbstschutz_unlesbar(kern *k, const char *p);       /* NEU 1.0.6 Fix: p ist ein privater, nicht lesbarer Ordner (oder liegt darin) */
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

#endif
