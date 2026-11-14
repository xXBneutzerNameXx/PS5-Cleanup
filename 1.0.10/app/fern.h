/* PS5 Cleanup - Bedienung im Browser
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * fern.h - kleiner Webserver in der Oberfläche (doku/alt/1.0.9/PLAN_1.0.9.md). Neu am 08.11.2026 nach da_Chris86s Test (16:09: "mit Maus und
 * Tastatur ... am Handy mit Touch", "Wieso wird es gestreamt? Das ist doch Quatsch", "ich würde den User überhaupt nicht darüber fragen"):
 * der Browser bekommt eine eigene Seite (fern_seite.html) und nur die Daten - den Zustand der App und die Liste als JSON, Bilder der
 * Kacheln als JPEG. Er baut sich die Seite selbst (Maus, Tastatur, Touch). Kein Fernsehbild, kein Steuerkreuz, keine Freigabe.
 *  - Ab dem Start an (da_Chris86 16:12: "Immer gleich an"), Adresse auf der Startseite.
 *  - Gelöscht wird nur auf der PS5, mit denselben Regeln wie am Fernseher: der Browser schickt Befehle ("dieser Haken", "Auswahl
 *    löschen", "Antwort auf Frage Nr. 7"), die Oberfläche führt sie im Hauptfaden aus wie einen Tastendruck. "Löschen" wird im Browser
 *    1,5 s gehalten, bei der vollständigen Bereinigung LÖSCHEN eingetippt; der Testmodus gilt auch hier.
 *  - Schutz (unsichtbar für den Nutzer): nur Geräte aus dem Heimnetz und die PS5 selbst; nur über die Zahlen-Adresse (gegen fremde
 *    Webseiten, die über einen Namen umleiten); Befehle nur als POST mit JSON aus der eigenen Seite (fremde Seiten können das nicht
 *    schicken, PS5 Cleanup beantwortet keine Vorab-Anfragen und gibt nichts für andere Seiten frei).
 *  - Port 9100, ist er belegt, der nächste freie bis 9109.
 * Alle Funktionen dürfen aus jedem Faden gerufen werden.
 */
#ifndef APP_FERN_H
#define APP_FERN_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* protokoll: Zeile ins Protokoll, wecken: Oberfläche neu zeichnen bzw. Befehle abholen (beides darf aus jedem Faden kommen) */
void fern_einrichten(void (*protokoll)(const char *t), void (*wecken)(void));
bool fern_starten(bool englisch, char *fehler, size_t fn);   /* true = läuft (Adresse: fern_adresse) */
void fern_stoppen(void);
bool fern_an(void);
void fern_adresse(char *a, size_t n);                        /* "http://192.168.178.78:9100", "" = aus */
void fern_sprache(bool englisch);
/* "Port 9100 offen · noch keine Anfrage vom Browser" bzw. "... Anfrage von 192.168.178.23 angekommen" oder "... abgewiesen (Grund)" - das
 * zuletzt Gemeldete; 0 = noch nichts, 1 = angekommen, 2 = abgewiesen, "" = aus. Ändert sich nur beim ersten Mal je Gerät. */
int fern_kontakt(char *z, size_t n);
bool fern_aktiv(uint32_t ms);                                /* hat in den letzten ms ein Browser etwas geholt? */
bool fern_will_daten(void);                                  /* fragt gerade ein Browser nach dem Zustand (letzte 15 s)? */
/* Daten für den Browser (aus der Oberfläche, malloc'd - gehören danach fern.c). Ein gleicher Zustand wie der letzte zählt nicht neu. */
void fern_zustand_geben(char *json);
void fern_liste_geben(char *json);
void fern_zeile(const char *t);                              /* jede Protokollzeile (Ringpuffer für die Seite "Protokoll") */
/* Befehle vom Browser, der Reihe nach (JSON-Text, z. B. {"b":"wahl","fg":3,"i":17}) */
bool fern_befehl_holen(char *j, size_t n);
/* Bilder der Kacheln: der Browser fragt nach einer Kennung, die Oberfläche lädt das Bild im Hauptfaden und gibt es zurück */
bool fern_bild_wunsch(char *id, size_t n);                   /* nächste gewünschte Kennung (die Oberfläche lädt sie) */
void fern_bild_geben(const char *id, const unsigned char *rgb, int w, int h);   /* RGB24 (w * 3 je Zeile); rgb = NULL: es gibt keins */
#endif
