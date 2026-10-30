/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * basis.h - Speicher, Texte, Listen, Wörterbuch, Ausnahmen.
 * Grundlage, damit der Kern 1:1 wie der C#-Kern von 1.0.5 geschrieben
 * werden kann (Listen, Dictionary mit Einfüge-Reihenfolge, try/catch).
 */
#ifndef KERN_BASIS_H
#define KERN_BASIS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>

/* ---------------------------------------------------------------
 * Arena: aller Speicher einer Suche/Löschung liegt in einer Arena und
 * wird am Ende in einem Stück freigegeben. Dadurch sind Ausnahmen
 * (longjmp) ohne Speicherlecks möglich.
 * --------------------------------------------------------------- */
typedef struct arena arena;
arena *arena_neu(void);
void   arena_frei(arena *a);
void  *a_hole(arena *a, size_t n);                 /* mit 0 gefüllt */
/* NEU 1.0.6.1 (Absturz auf FW 13.60, 30.10.2026): Speicherverbrauch des Kerns (alle Arenen) und letzter Grund vor abort().
 * basis_kein_speicher: wird aufgerufen, bevor das Programm mangels Arbeitsspeicher abbricht (PS5: Meldung + Protokoll). */
size_t basis_speicher_belegt(void);                 /* Byte, gerade belegt */
size_t basis_speicher_spitze(void);                 /* Byte, Höchststand */
extern void (*basis_kein_speicher)(size_t n);
extern const char *volatile basis_grund;            /* NULL oder Grund für einen Abbruch (abort) */
extern const char *volatile basis_schritt;          /* was die Oberfläche gerade macht (für die Absturzmeldung) */
extern volatile int basis_zaehlt;                   /* 1 = das Größenzählen im Hintergrund läuft */
/* NEU 1.0.6 Fix: Faden starten wie pthread_create, aber mit mindestens 1 MB Stapel (nie weniger als die Voreinstellung) */
#include <pthread.h>
int basis_faden(pthread_t *t, void *(*f)(void *), void *x);
/* NEU 1.0.6 Fix: Byte-Folge suchen (memmem gibt es in der PS5-Bibliothek nicht) */
const void *basis_suche(const void *d, size_t n, const void *m, size_t ml);
char  *a_kopie(arena *a, const char *s);           /* NULL -> "" */
char  *a_kopie_n(arena *a, const char *s, size_t n);
char  *a_fmt(arena *a, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
char  *a_verb(arena *a, ...);                      /* Texte aneinander, Ende mit NULL */
#define VERB(a, ...) a_verb((a), __VA_ARGS__, (const char *)NULL)

/* ---------------------------------------------------------------
 * Listen (wie List<string> / List<T>)
 * --------------------------------------------------------------- */
typedef struct { const char **e; int n, cap; } sliste;
void sl_add(arena *a, sliste *l, const char *s);
void sl_add_alle(arena *a, sliste *l, const sliste *von);
sliste sl_kopie(arena *a, const sliste *l);
const char *sl_join(arena *a, const sliste *l, const char *trenner);

typedef struct { void **e; int n, cap; } pliste;
void pl_add(arena *a, pliste *l, void *p);

/* ---------------------------------------------------------------
 * Wörterbuch Text -> Wert mit Einfüge-Reihenfolge (wie Dictionary in .NET
 * ohne Entfernen). ci = Schlüssel ohne Groß/klein (wie H.Tab<T>()).
 * --------------------------------------------------------------- */
typedef struct { const char *k; void *v; } mpaar;
typedef struct { mpaar *e; int n, cap; bool ci; } mtab;
mtab  mt_neu(bool ci);
int   mt_index(const mtab *t, const char *k);      /* -1 = fehlt */
bool  mt_hat(const mtab *t, const char *k);
void *mt_hole(const mtab *t, const char *k);       /* NULL wenn fehlt */
void  mt_setze(arena *a, mtab *t, const char *k, void *v);

/* ---------------------------------------------------------------
 * Ausnahmen (wie throw/catch in C#)
 *
 *   VERSUCHE {
 *       ...                      (kein return/break/goto aus dem Block!)
 *   } FANGE {
 *       if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN();
 *   } ENDE_VERSUCHE;
 *
 * Variablen, die im VERSUCHE-Block geändert und danach benutzt werden,
 * müssen volatile sein.
 * --------------------------------------------------------------- */
typedef enum {
    AUSN_KEINE = 0,
    AUSN_FEHLER,      /* Exception */
    AUSN_IO,          /* IOException */
    AUSN_ABBRUCH,     /* AbbruchAusnahme (Nutzer hat abgebrochen) */
} ausn_t;

typedef struct fangpunkt { jmp_buf j; struct fangpunkt *vor; } fangpunkt;
extern _Thread_local fangpunkt *ausn_oben;
extern _Thread_local ausn_t ausn_art;
extern _Thread_local char ausn_text[2048];

void wirf(ausn_t art, const char *text) __attribute__((noreturn));
void weiterwerfen(void) __attribute__((noreturn));

#define VERSUCHE { fangpunkt _fp; _fp.vor = ausn_oben; ausn_oben = &_fp; \
                   if (setjmp(_fp.j) == 0) {
#define FANGE        ausn_oben = _fp.vor; } else { ausn_oben = _fp.vor;
#define ENDE_VERSUCHE } }
#define WEITERWERFEN() weiterwerfen()

/* Sprache (wie Sp.EN / Sp.T in Sprache.cs) */
extern bool SP_EN;
static inline const char *T(const char *de, const char *en) { return SP_EN ? en : de; }

/* Kleinigkeiten */
bool  txt_gleich(const char *a, const char *b);                 /* ordinal */
bool  txt_beginnt(const char *s, const char *anfang);           /* ordinal */
bool  txt_endet(const char *s, const char *ende);               /* ordinal */
bool  txt_enthaelt(const char *s, const char *teil);            /* ordinal */
int   txt_index(const char *s, const char *teil);               /* ordinal, -1 */
int   txt_letzter(const char *s, char c);                       /* -1 */

/* Groß/klein wie .NET (OrdinalIgnoreCase / ToUpperInvariant): einfache
 * Zuordnung je Zeichen für Latein (inkl. Latin-1, Latin Erweitert-A),
 * Griechisch und Kyrillisch; alle anderen Zeichen bleiben wie sie sind. */
uint32_t zeichen_gross(uint32_t c);
bool  txt_gleich_ci(const char *a, const char *b);
char *a_gross(arena *a, const char *s);
/* UTF-8 lesen: ungültige Bytes zählen als U+FFFD (wie UTF8Encoding in .NET) */
uint32_t utf8_naechstes(const unsigned char **p);

#endif
