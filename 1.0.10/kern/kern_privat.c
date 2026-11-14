/* PS5 Cleanup 1.0.10 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * kern_privat.c - Entwurf 1.0.9 (da_Chris86 08.11.2026 08:02/08:20), aufgenommen 1.0.10 (da_Chris86 10.11.2026 17:30, Punkt 3): private Daten
 * von Programmen (Playden & Co.) sichtbar machen. Seit 1.0.10 nur im Ordner eines bekannten Programms (kern_ordner_bekannt), nie über
 * eine Verknüpfung, Sicherheitsstopp auch für Sonys Befehle (Codex-Prüfung 126).
 * "Es muss benutzerfreundlich werden und der User muss auch sehen, was passiert."
 * Die Zugriffsschicht liest und löscht Ordner, in die nur das Programm selbst darf, als dessen Benutzer (zugriff_datei.c, Besitzer-Wechsel)
 * und merkt sich jeden Ort ("SITE PRIVAT-LISTE?"). Hier:
 *  - nach der Suche: Einträge markieren, die solche Daten enthalten (Zeile "mit privaten Daten", Erklärung im Kasten) und im Protokoll
 *    nennen, welche Orte das sind und wem sie gehören;
 *  - nach dem Löschen: im Protokoll, was als Benutzer eines Programms gelöscht wurde (Testmodus: was so gelöscht würde).
 * Nur Anzeige - was gelöscht wird, entscheiden wie immer Auswahl und Schutzregeln.
 */
#include "kern.h"
#include <stdlib.h>
#include <string.h>

#define A (k->a)

typedef struct { char art; unsigned uid; int n; const char *pfad; } privat_ort;

void kern_privat_neu(kern *k)
{
    if (!k->S) return;
    VERSUCHE { s_befehl(k->S, "SITE PRIVAT-NEU", NULL); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); } ENDE_VERSUCHE;
    k->PRIVAT_ORTE = 0;
}

/* "SITE PRIVAT-LISTE?" lesen. NULL = nichts (kein Wechsel eingerichtet, Wiedergabe ohne diese Frage, ...) */
static privat_ort *liste_holen(kern *k, int *n, int *mehr)
{
    *n = 0; *mehr = 0;
    if (!k->S) return NULL;
    volatile int code = 502;
    const char *volatile text = NULL;
    VERSUCHE { antwort r = s_befehl(k->S, "SITE PRIVAT-LISTE?", NULL); code = r.code; text = r.text; }
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); code = 502; } ENDE_VERSUCHE;
    if (code != 200 || !text) return NULL;
    sliste z = r_split(A, text, "\n");
    if (z.n < 1) return NULL;
    htreffer kopf = h_mt(A, z.e[0], "^200 (\\d+) (\\d+)$");
    if (!kopf.ok) return NULL;
    *mehr = atoi(kopf.gr[2]);
    privat_ort *l = a_hole(A, sizeof *l * (size_t)z.n);
    for (int i = 1; i < z.n; i++) {
        htreffer m = h_mt(A, z.e[i], "^([LW]) (\\d+) (\\d+) (/.*)$");
        if (!m.ok) continue;
        l[*n].art = m.gr[1][0]; l[*n].uid = (unsigned)strtoul(m.gr[2], NULL, 10); l[*n].n = atoi(m.gr[3]); l[*n].pfad = m.gr[4];
        (*n)++;
    }
    return l;
}
static bool liegt_in(const char *p, const char *q) { size_t l = strlen(q); return strncmp(p, q, l) == 0 && (p[l] == 0 || p[l] == '/'); }
/* liegt ein privater Ort (L) in einem Pfad des Eintrags - oder der Pfad in einem privaten Ort? */
static bool hat_privat(const eintrag *f, const privat_ort *l, int n)
{
    for (int j = 0; j < f->Pfade.n; j++)
        for (int i = 0; i < n; i++) if (l[i].art == 'L' && (liegt_in(l[i].pfad, f->Pfade.e[j]) || liegt_in(f->Pfade.e[j], l[i].pfad))) return true;
    return false;
}

void kern_privat_markieren(kern *k)
{
    k->PRIVAT_ORTE = 0;
    int n, mehr;
    privat_ort *l = liste_holen(k, &n, &mehr);
    int orte = 0;
    for (int i = 0; l && i < n; i++) if (l[i].art == 'L') orte++;
    if (orte == 0) return;
    k->PRIVAT_ORTE = orte + mehr;
    const char *mark = T("  [mit privaten Daten]", "  [with private data]");
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        if (f->Privat || !hat_privat(f, l, n)) continue;
        f->Privat = true;
        f->Text = VERB(A, f->Text ? f->Text : "", mark);
        f->TextBasis = VERB(A, f->TextBasis ? f->TextBasis : "", mark);
    }
    kern_melde(k, VERB(A, T("Private Daten von Programmen gefunden (nur das Programm selbst darf hinein) - PS5 Cleanup liest sie kurz als dessen Benutzer, so wie das Programm selbst (",
                            "Private data of apps found (only the app itself may open it) - PS5 Cleanup reads it briefly as the app's user, like the app itself ("),
                       a_fmt(A, "%d", orte + mehr), "):"));
    for (int i = 0; i < n; i++) if (l[i].art == 'L') kern_melde(k, VERB(A, "   ", l[i].pfad, a_fmt(A, T("  (Besitzer-Kennung %u)", "  (owner id %u)"), l[i].uid)));
    if (mehr > 0) kern_melde(k, VERB(A, T("   ... und ", "   ... and "), a_fmt(A, "%d", mehr), T(" weitere", " more")));
}

void kern_privat_nachher(kern *k, const pliste *gewaehlt)
{
    bool dabei = false;
    for (int i = 0; i < gewaehlt->n && !dabei; i++) if (((eintrag *)gewaehlt->e[i])->Privat) dabei = true;
    if (!dabei) return;
    int n, mehr;
    privat_ort *l = liste_holen(k, &n, &mehr);
    if (!l) return;
    if (k->simulation) {   /* Testmodus: gelöscht wird nichts - was als Benutzer des Programms gelöscht würde */
        bool kopf = false;
        for (int i = 0; i < n; i++) {
            if (l[i].art != 'L') continue;
            bool gew = false;
            for (int g = 0; g < gewaehlt->n && !gew; g++) { const eintrag *f = gewaehlt->e[g]; if (f->Privat && hat_privat(f, &l[i], 1)) gew = true; }
            if (!gew) continue;
            if (!kopf) { kern_melde(k, T("SIMULATION: Diese privaten Daten würden als Benutzer des Programms gelöscht:", "SIMULATION: this private data would be deleted as the app's user:")); kopf = true; }
            kern_melde(k, VERB(A, "   ", l[i].pfad, a_fmt(A, T("  (Besitzer-Kennung %u)", "  (owner id %u)"), l[i].uid)));
        }
        return;
    }
    bool kopf = false;
    for (int i = 0; i < n; i++) {
        if (l[i].art != 'W') continue;
        if (!kopf) { kern_melde(k, T("Als Benutzer des Programms gelöscht (private Daten):", "Deleted as the app's user (private data):")); kopf = true; }
        kern_melde(k, VERB(A, "   ", l[i].pfad, a_fmt(A, T("  (Besitzer-Kennung %u, %d Schritte)", "  (owner id %u, %d steps)"), l[i].uid, l[i].n)));
    }
    kern_privat_neu(k);   /* gemeldet - ein weiterer Lauf ohne neue Suche nennt nur seine eigenen Schritte */
}
