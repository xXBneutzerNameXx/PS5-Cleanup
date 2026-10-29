/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * ablauf.h - die Bedien-Abläufe aus Fenster.cs (1.0.5), ohne Oberfläche:
 * Suchen, vollständige Bereinigung, "Nur Auswahl löschen", Zusammenfassung,
 * Abschlussfrage mit LÖSCHEN/DELETE, Größen zählen, Löschen, Fertig.
 * Die Fragen und Texte sind WORTGLEICH aus 1.0.5. Die Oberfläche (Fernseher,
 * später Browser) liefert nur die Antworten über die Rückrufe.
 * NEU 1.1: In der Simulation (K->simulation) steht überall deutlich "SIMULATION".
 */
#ifndef KERN_ABLAUF_H
#define KERN_ABLAUF_H

#include "kern.h"

/* Wert für ui.stand(): Balken */
#define ABLAUF_UNBESTIMMT   (-1.0)     /* Balken läuft (IsIndeterminate) */
#define ABLAUF_BALKEN_GLEICH (-2.0)    /* nur Text ändern */

typedef struct ablauf_ui {
    /* FrageWahl(text, titel, sicher, gefaehrlich): true = gefährlicher Knopf. Vorbelegt ist immer "sicher". */
    bool (*frage_wahl)(void *ctx, const char *text, const char *titel, const char *sicher, const char *gefaehrlich);
    /* FrageText(text, titel): eingegebener Text, NULL = abgebrochen */
    const char *(*frage_text)(void *ctx, const char *text, const char *titel);
    void (*log)(void *ctx, const char *zeile);                          /* Log(...) ins Protokoll */
    void (*stand)(void *ctx, const char *text, double prozent);        /* Statuszeile (NULL = Text bleibt) + Balken (0..100 oder ABLAUF_*) */
    /* FertigMeldung(kopf, ok, frei, smText); die Anleitung danach liefert kern_schluss_text() */
    void (*fertig)(void *ctx, const char *kopf, bool ok, const char *frei, const char *smtext);
    void (*fehler)(void *ctx, const char *text);                       /* FehlerMelden: Meldung an den Nutzer */
    void (*liste_neu)(void *ctx);                                       /* ZeigeBereiche/ZeigeSpeicher neu */
    void *ctx;
} ablauf_ui;

typedef struct ablauf {
    kern *K;
    ablauf_ui ui;
    groesse GROSS;                       /* zuletzt gezählte Größe der Auswahl */
    /* Größen im Hintergrund */
    void *hintergrund;                   /* interner Zustand (Faden) */
    volatile int hg_n, hg_gesamt;        /* "Größen werden ermittelt (n von gesamt)" */
    void (*groesse_neu)(void *ctx);      /* Oberfläche neu zeichnen, wenn eine Größe dazukam (aus dem Hintergrund-Faden!) */
    void *groesse_ctx;
    /* NEU 1.1: neue "Verbindung" nach einer Simulation (frische Simulationsschicht),
     * damit die Liste danach wieder den echten Stand zeigt. NULL = keine. */
    zugriff *(*neue_verbindung)(void *ctx);
    void *verbindung_ctx;
    volatile bool abbrechbar;            /* gerade läuft Suchen/Zählen: Abbrechen erlaubt (btAbbrechen in 1.0.5) */
    volatile bool loescht;               /* LAEUFT: gerade wird gelöscht - nicht beenden */
    const char *protokoll_datei;         /* für FehlerMelden ("Das Protokoll liegt hier: ...") */
    const char *sim_hinweis;             /* NEU 1.1: statt "SIMULATION - es wird nichts gelöscht." in Statuszeile/Protokoll (NULL = so lassen) */
    arena *ta;                           /* Speicher der Anzeige-Texte (Oberflächen-Faden), siehe ablauf_texte_frei */
    bool leeren_m2, leeren_usb;          /* NEU 1.0.6: vollständige Bereinigung - wurden M.2/USB mit geleert? (für das Selbst-Entfernen) */
    int selbst_modus;                    /* NEU 28.10.2026: 1 = vollständige Bereinigung (PS5 Cleanup am Ende ohne Frage entfernen), 2 = Auswahl (am Ende fragen) */
    /* NEU 1.0.6 (da_Chris86 28.10.2026 23:19): nach der letzten Bestätigung fragen, ob die PS5 am Ende automatisch neu starten soll.
     * NULL = keine Frage (Windows-Vergleich, Tests). Das Ergebnis steht in auto_neustart (die Oberfläche führt es aus). */
    bool (*neustart_frage)(void *ctx);
    void *neustart_ctx;
    bool auto_neustart;
} ablauf;

void ablauf_init(ablauf *a, kern *k, const ablauf_ui *ui);
bool ablauf_bereich_nur_einzeln(ablauf *a, int kat);   /* NEU 1.1 */
bool ablauf_verbunden(ablauf *a);                                    /* VERBUNDEN: gültige Liste da */
void ablauf_texte_frei(ablauf *a);                                   /* Anzeige-Texte freigeben (z.B. einmal je Bild) */

/* Die folgenden Anzeige-Funktionen sind für den Oberflächen-Faden. Ihr Speicher kommt aus
 * a->ta (nie aus dem Kern-Speicher). Sie dürfen NICHT aufgerufen werden, während ein Ablauf
 * läuft (dann zeigt die Oberfläche nur Statuszeile und Protokoll) - wie in 1.0.5, wo die
 * Liste während der Arbeit gesperrt ist. */

/* Auswahl wie die Kästchen in 1.0.5 */
void ablauf_eintrag_setzen(ablauf *a, eintrag *f, bool an);          /* Kästchen eines Eintrags */
void ablauf_bereich_setzen(ablauf *a, int kat, bool an);             /* Kästchen eines Bereichs */
int  ablauf_bereich_zustand(ablauf *a, int kat);                     /* 0 = nichts, 1 = teilweise, 2 = alles */
bool ablauf_eintrag_gesperrt(ablauf *a, const eintrag *f, const char **grund);   /* nicht anklickbar? + Hinweis (ToolTip) */
const char *ablauf_bereich_kopf(ablauf *a, int kat);                 /* "[1] Spiele & Apps   (12 Einträge · 3,4 GB)" */
const char *ablauf_eintrag_groesse(ablauf *a, const eintrag *f);     /* "   1,2 GB" oder "" */
const char *ablauf_stand_text(ablauf *a);                            /* AktualisiereStand */
const char *ablauf_zusammenfassung(ablauf *a, bool komplett);        /* Zusammenfassung(komplett) */

/* Größen im Hintergrund (HintergrundStarten/-Anhalten) */
void ablauf_hintergrund_starten(ablauf *a);
void ablauf_hintergrund_anhalten(ablauf *a);

/* Abläufe - laufen im Arbeits-Faden, fragen über ui.frage_*.
 * Immer über ablauf_starte() aufrufen (wie Starte() in 1.0.5: Abbruch und Fehler). */
void ablauf_suchen(ablauf *a, const char *modell, const char *firmware);   /* Teil "Suchen" aus Verbinde() */
void ablauf_komplett(ablauf *a);                                      /* KomplettAblauf */
void ablauf_auswahl(ablauf *a);                                       /* AuswahlAblauf */
typedef struct { ablauf *a; const char *modell, *firmware; } ablauf_suchauftrag;
void ablauf_starte(ablauf *a, void (*fn)(void *), void *arg);         /* Starte(ablauf) */

#endif
