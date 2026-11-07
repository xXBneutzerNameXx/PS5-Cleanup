/* PS5 Cleanup 1.1 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * sitzung.h - 1:1 nach der Klasse Sitzung in Sitzung.cs (1.0.5):
 * Liste (mit "streng"), Namen, Existiert, Text, Bytes, Schreiben, Befehl,
 * Gesund - aufgebaut auf der Zugriffsschicht statt auf FTP.
 */
#ifndef KERN_SITZUNG_H
#define KERN_SITZUNG_H

#include "zugriff.h"

/* class Datei */
typedef struct { const char *name; bool ist_ordner, ist_link; long long groesse; } datei;
/* List<Datei> - als Zeiger: NULL entspricht null in C# */
typedef struct { datei *e; int n; } dliste;

typedef struct sitzung {
    zugriff *z;
    arena *a;                            /* hier landen alle Ergebnisse */
    bool (*abbruch)(void *ctx);          /* Abbruch durch den Nutzer: vor jeder Ordnerliste geprüft */
    void *abbruch_ctx;
    const char *letzter_fehler;          /* Ftp.LetzterFehler */
} sitzung;

sitzung    *sitzung_neu(zugriff *z, arena *a);
antwort     s_befehl(sitzung *s, const char *befehl, const char *pfad);   /* pfad darf NULL sein */
dliste     *s_liste(sitzung *s, const char *pfad, bool streng);
sliste      s_namen(sitzung *s, const char *pfad, bool streng);
bool        s_existiert(sitzung *s, const char *pfad);
bool        s_kopf(sitzung *s, const char *pfad, size_t n, const unsigned char **d, size_t *len);   /* NEU 1.1: Dateianfang */
bool        s_lies_ab(sitzung *s, const char *pfad, long long ab, size_t n, unsigned char *ziel, size_t *len);   /* NEU 1.0.8: n Bytes ab Stelle ab */
int         s_enthaelt(sitzung *s, const char *pfad, const void *m, size_t ml);
int         s_stuecke(sitzung *s, const char *pfad, size_t stueck, bool (*weiter)(void *c, const unsigned char *d, size_t n), void *c);   /* NEU 1.0.7: stückweise, 1 gelesen, -1 nicht lesbar */   /* NEU 1.0.6 Fix: 1 ja, 0 nein, -1 nicht lesbar */
const char *s_text(sitzung *s, const char *pfad);
const char *s_text_streng(sitzung *s, const char *pfad);   /* NEU 1.1: Lesefehler = Abbruch */
bool        s_bytes(sitzung *s, const char *pfad, const unsigned char **d, size_t *n);  /* false = null */
bool        s_schreiben(sitzung *s, const char *pfad, const void *d, size_t n);
bool        s_gesund(sitzung *s);

/* Text eines Fehlers wie in 1.0.5 */
const char *s_verweigert_text(sitzung *s, const char *pfad, antwort a);

#endif
