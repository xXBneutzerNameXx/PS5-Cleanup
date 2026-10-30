/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
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
} eintrag;
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
    mtab   NAMEN_CACHE;                 /* ci: id -> const char* */
    mtab   NPWR_NAMEN;                  /* ci */
    mtab   NPWR_GALERIE;                /* ci */
    mtab   GALERIE_NAMEN;               /* ci */
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
    bool   simulation;                  /* NEU 1.1: Zugriff ist eine Simulation -> ShadowMount nie wirklich anhalten */
    bool   SONY;                        /* NEU 1.0.6: Sonys Deinstallations-Dienst ist da (PS5) - Kacheln gehen über Sony */
    bool   SELBST_UEBERGEBEN;           /* NEU 1.0.6: Kachel + Startordner entfernt der Hintergrundteil nach dem Ende der Oberfläche */
    bool   SELBST_DIAG;                 /* NEU 1.0.6: während der Selbstentfernung jede Meldung auch ins Diagnoseprotokoll */
    sliste GEISTER;                     /* NEU 1.0.6: Namen der Kacheln, die nach dem Löschlauf im Menü stehen bleiben (Sony lehnte ab) */
    bool   DB_HINWEIS;                  /* NEU 1.0.6: am Ende "Datenbank neu aufbauen" zeigen (ohne Sony immer, mit Sony nur bei GEISTER) */

    /* Konsole (Kern.cs): Modell und Laufwerks-Kopplung - nur gelesen */
    const char *MODELL_TYP;             /* "fat" / "slim" / "pro" / "" */
    bool   MODELL_DIGITAL;
    bool   DDD_BEKANNT;                 /* DDD_STATUS != null */
    int    DDD_STATUS;
} kern;

/* class Ergebnis (Ausfuehren.cs) */
typedef struct { bool Abgebrochen; int Fehler; bool SmGestoppt; groesse Frei; const char *SmText; bool Verloren;
                 int Teilweise;   /* NEU 1.0.6: Dateien gelöscht, aber die Kachel bleibt im Menü (Sony lehnte ab) - gilt nicht als ganz entfernt */
               } ergebnis;
typedef enum { SM_ANGEHALTEN_Z, SM_LAEUFT_NICHT, SM_UNBEKANNT } smzustand;   /* SmZustand */

kern *kern_neu(zugriff *z);
void  kern_frei(kern *k);
void  kern_melde(kern *k, const char *text);

/* ---- Schutzliste (Kern.cs) ---- */
bool kern_astro_pfad(kern *k, const char *p);        /* AstroPfad */
bool kern_erlaubt(kern *k, const char *p);           /* Erlaubt */
bool kern_laufende_aufnahme(const char *p);
bool kern_nie_anfassen(const char *p);                /* NEU 1.1 (S5): Systembereiche, Konten, Lizenzen, Sony-Apps */           /* NEU 1.1 (U16): sharevideo_…_hdr.webm der PS5 */
bool kern_ohne_verknuepfung(kern *k, const char *p); /* OhneVerknuepfung */

/* ---- Suche (Suche.cs) ---- */
#define KAT_ANZ 6                                     /* NEU 1.0.6: 6 Bereiche (der M.2-Bereich ist aufgeteilt) */
extern const int ALLE_KAT[KAT_ANZ];
const char *kern_kat_name(int k);                    /* KatName[k] */
const char *kern_kat_info(int k);                    /* KatInfo[k] */
const char *kern_json_titel(arena *a, const char *t);/* JsonTitel */
mtab        kern_sfo_lesen(arena *a, const unsigned char *b, size_t n);   /* SfoLesen: ci-Tabelle -> sfowert* */
const char *kern_sfo_titel(arena *a, const mtab *h); /* SfoTitel */
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

#endif
