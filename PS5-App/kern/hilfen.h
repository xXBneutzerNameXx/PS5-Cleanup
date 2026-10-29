/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * hilfen.h - 1:1 nach der Klasse H in Sitzung.cs (1.0.5) plus die direkten
 * Regex-Aufrufe des C#-Kerns. Die Suchmuster werden WÖRTLICH aus 1.0.5
 * übernommen und mit PCRE2 ausgewertet (gleiche Mustersprache wie .NET).
 *
 * Optionen wie in 1.0.5:
 *   H.M / H.Mt / H.R   -> IgnoreCase | CultureInvariant  (hier: CASELESS, UTF, UCP)
 *   H.Like             -> zusätzlich Singleline           (hier: DOTALL)
 *   Regex.* direkt     -> ohne Optionen                   (hier: UTF, UCP)
 * Texte mit ungültigem UTF-8 werden vor dem Vergleich so umgewandelt, wie
 * .NET sie gelesen hätte (ungültige Bytes -> U+FFFD).
 */
#ifndef KERN_HILFEN_H
#define KERN_HILFEN_H

#include "basis.h"

typedef struct {
    bool ok;               /* Match.Success */
    const char *wert;      /* Match.Value  ("" wenn kein Treffer) */
    int ngr;               /* Anzahl Gruppen inkl. Gruppe 0 */
    const char **gr;       /* Groups[i].Value ("" wenn die Gruppe nicht beteiligt ist) */
} htreffer;

/* Klasse H (Sitzung.cs) */
bool        h_m(const char *s, const char *muster);
htreffer    h_mt(arena *a, const char *s, const char *muster);
const char *h_r(arena *a, const char *s, const char *muster, const char *ersatz);
bool        h_like(const char *s, const char *wild);
bool        h_hat(const sliste *l, const char *s);
bool        h_gleich(const char *a, const char *b);
bool        h_steuerzeichen(const char *s);
const char *h_eltern(arena *a, const char *p);     /* wirft wie .NET, wenn kein '/' drin ist */
const char *h_blatt(arena *a, const char *p);

/* Regex.Escape */
const char *h_escape(arena *a, const char *s);

/* direkte Regex-Aufrufe (ohne IgnoreCase) */
bool        r_m(const char *s, const char *muster);                         /* Regex.IsMatch */
int         r_anzahl(const char *s, const char *muster);                    /* Regex.Matches(..).Count */
sliste      r_alle(arena *a, const char *s, const char *muster);            /* Regex.Matches(..) -> Value */
const char *r_r(arena *a, const char *s, const char *muster, const char *ersatz); /* Regex.Replace */
sliste      r_split(arena *a, const char *s, const char *muster);           /* Regex.Split */

/* Text so, wie .NET ihn aus UTF-8 gelesen hätte (ungültig -> U+FFFD) */
const char *h_net_text(arena *a, const char *s);
/* string.Trim() / TrimStart('﻿') */
const char *h_trim(arena *a, const char *s);
const char *h_ohne_bom(const char *s);

/* Fehlertext der letzten Muster-Übersetzung (für die Tests) */
const char *h_musterfehler(void);

#endif
