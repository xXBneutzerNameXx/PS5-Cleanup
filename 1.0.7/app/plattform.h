/* PS5 Cleanup 1.1 - Oberfläche am Fernseher
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * plattform.h - was die Oberfläche von der Konsole braucht. Auf der PS5:
 * plattform_ps5.c, für die Vorschau am PC: plattform_pc.c.
 */
#ifndef APP_PLATTFORM_H
#define APP_PLATTFORM_H
#include <stdbool.h>
#include "../kern/zugriff.h"

typedef struct {
    char modell[64];        /* "" = unbekannt */
    char firmware[32];      /* "" = unbekannt */
    bool ddd_bekannt;       /* Laufwerks-Kopplung abgefragt? */
    int  ddd;               /* Statuswert 0..6, -1 = nicht auslesbar */
} konsole_info;

void plattform_start(void);                     /* vor SDL_Init */
void plattform_ende(void);                      /* zurück zum Home-Menü */
void plattform_konsole(konsole_info *k);        /* nur lesen */
bool plattform_englisch(void);                  /* Systemsprache nicht Deutsch */
const char *plattform_eigener_ort(void);        /* Ordner, aus dem PS5 Cleanup gestartet wurde ("" = unbekannt) */
zugriff *plattform_zugriff(void);               /* NEU für jede Suche: Simulation über das Dateisystem (nur lesen) */
const char *plattform_name(void);               /* "PS5" oder "PC-Vorschau" */
/* NEU 1.0.6 (da_Chris86 28.10.2026 23:19): Neustart nach der Bereinigung. true = Auftrag angenommen.
 * Nur im Release-Bau wirksam - die Simulation ruft ihn nie auf. */
bool plattform_neustart(void);
/* NEU 1.0.6: Kachel + Startordner entfernt der Hintergrundteil - er übernimmt auch den Neustart (erst nach der Entfernung).
 * true = bestätigt übernommen (dann beendet sich die Oberfläche nur), false = nicht übernommen */
bool plattform_neustart_uebergeben(void);
/* NEU 1.0.6 Fix (30.10.2026, Absturz auf FW 13.60 ohne jede Meldung): ab jetzt meldet sich jeder Absturz (Speicherfehler, kein
 * Arbeitsspeicher, unbehandelter Fehler) als Benachrichtigung und im Kernel-Protokoll - mit Schritt, Grund und Speicherverbrauch. */
void plattform_absturzmelder(const char *name);

/* NEU 1.0.7 (Plan 1.0.7, Punkt 2): ○ Beenden am Ende - Hinweis "bei Gelegenheit neu starten" als Benachrichtigung.
 * art: seit 02.11.2026 nur noch 1 (kein Ausschalten, kein Datenbank-Neuaufbau mehr).
 * hinweis_uebergeben: der Hintergrundteil hängt den Hinweis an seine Abschlussmeldung ("PS5 Cleanup wurde entfernt.") an.
 * melden: Benachrichtigung sofort (wenn nichts übergeben ist). */
bool plattform_hinweis_uebergeben(int art, bool englisch);
void plattform_melden(const char *text);
/* NEU 1.0.7 (Infoleiste "Speicher frei"): freier und gesamter Platz eines Speicherorts (ORT_* aus kern.h).
 * false = nicht angeschlossen oder nicht lesbar. Nur lesen. */
bool plattform_speicher(int ort, long long *frei, long long *gesamt);
/* NEU 1.0.7 (da_Chris86 31.10.2026 19:27/19:28): Protokoll als Datei.
 * protokoll_ordner(nr): Ordner für das Protokoll, wenn der USB-Stick fehlt, voll oder nicht beschreibbar ist (da_Chris86 02.11.2026
 *   16:57/17:23): nr 0 = interne Platte /data/PS5-Cleanup-Protokolle, nr 1 = M.2 /mnt/ext1/PS5-Cleanup-Protokolle (nur wenn die
 *   interne Platte voll ist). Jeder Lauf eine eigene Datei wie auf dem Stick, bleibt nach der Selbstentfernung. Die echte Fassung legt
 *   den Ordner bei Bedarf an. NULL = geht nicht (nicht eingehängt, voll, kein Ordner).
 * frei_bytes: freier Platz an einem Ort in Byte, -1 = nicht lesbar (nur lesen).
 * stick_ordner: Wurzel eines angesteckten USB-Sticks (/mnt/usbN), NULL = keiner.
 * protokoll_zeile: jede Zeile zusätzlich ins Kernel-Protokoll (für Tests).
 * protokoll_stick_setzen: Protokoll-Datei (Stick oder intern) - der Hintergrundteil hängt bei der Selbstentfernung seine Schlusszeilen an. */
#define PLATTFORM_PROT_MIN_FREI (2LL * 1024 * 1024)   /* weniger frei = voll (für das Protokoll) */
const char *plattform_protokoll_ordner(int nr);
long long plattform_frei_bytes(const char *pfad);
const char *plattform_stick_ordner(void);
void plattform_protokoll_zeile(const char *t);
void plattform_protokoll_stick_setzen(const char *pfad);
/* NEU 1.0.7 (Punkt 2): Ordner mit app.db/appinfo.db (PS5: /system_data/priv/mms), NULL = keiner (nur lesen) */
const char *plattform_db_ordner(void);
/* NEU 1.0.7 (PS5-Test 02.11.2026, GTA V / Red Dead Redemption 2): Sonys appinfo.db NUR LESEN und dem Kern jede Zeile
 * INSTALL_DIR_SAVEDATA / SERVICE_ID_ADDCONT_ADD_<n> melden (Zuordnung von Resten, kaputte Zusatzinhalt-Angaben). Setzt app.c als
 * K->appinfo_lesen. */
struct kern;
void plattform_appinfo_lesen(struct kern *k, void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert));
/* NEU 1.0.7 (Punkt 3): Zeilen der Plattform (z. B. jeder Schritt einer Sony-Deinstallation) ins Protokoll der Oberfläche.
 * Setzt app.c; darf aus jedem Faden gerufen werden. NULL = nur Kernel-Protokoll. */
extern void (*plattform_meldung)(const char *t);

#endif
