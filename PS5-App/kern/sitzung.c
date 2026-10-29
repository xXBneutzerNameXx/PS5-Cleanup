/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * sitzung.c - 1:1 nach der Klasse Sitzung (Sitzung.cs, 1.0.5).
 * Jede Funktion trägt den Namen der C#-Methode, aus der sie stammt.
 */
#include "sitzung.h"
#include "hilfen.h"
#include <stdlib.h>
#include <string.h>

sitzung *sitzung_neu(zugriff *z, arena *a)
{
    sitzung *s = a_hole(a, sizeof *s);
    s->z = z; s->a = a; s->letzter_fehler = "";
    return s;
}

/* Ftp.Befehl: Steuerzeichen im Befehl -> IOException, nie senden.
 * Abgerissene Verbindung (Code -1) -> IOException "FTP-Verbindung unterbrochen". */
static void pruefe_zeile(sitzung *s, const char *zeile)
{
    if (h_steuerzeichen(zeile)) {
        const char *ersetzt = r_r(s->a, zeile, "[\\x00-\\x1f\\x7f]", "?");
        wirf(AUSN_IO, VERB(s->a, T("Ungültiges Zeichen im FTP-Befehl: ", "Invalid character in FTP command: "), ersetzt));
    }
}
static antwort verbindung(antwort a)
{
    if (a.code < 0) wirf(AUSN_IO, T("FTP-Verbindung unterbrochen", "FTP connection interrupted"));
    return a;
}

antwort s_befehl(sitzung *s, const char *befehl, const char *pfad)
{
    pruefe_zeile(s, pfad ? VERB(s->a, befehl, " ", pfad) : befehl);
    return verbindung(s->z->befehl(s->z, s->a, befehl, pfad));
}
static antwort cwd(sitzung *s, const char *pfad)
{
    pruefe_zeile(s, VERB(s->a, "CWD ", pfad));
    return verbindung(s->z->cwd(s->z, s->a, pfad));
}
/* Ftp.Daten("LIST") nach CWD pfad: NULL wie null */
static kdliste *daten_liste(sitzung *s, const char *pfad)
{
    kdliste *l = a_hole(s->a, sizeof *l);
    antwort a = verbindung(s->z->liste(s->z, s->a, pfad, l));
    if (a.code >= 400) { s->letzter_fehler = VERB(s->a, "LIST: ", a.text); return NULL; }
    return l;
}

/* Zerlege: ".", "..", Namen mit Steuerzeichen fallen weg */
static dliste *zerlege(sitzung *s, const kdliste *l)
{
    dliste *d = a_hole(s->a, sizeof *d);
    d->e = a_hole(s->a, sizeof(datei) * (size_t)(l->n + 1));
    for (int i = 0; i < l->n; i++) {
        const char *name = l->e[i].name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        if (h_steuerzeichen(name)) continue;          /* Steuerzeichen im Namen: nie anfassen */
        datei *e = &d->e[d->n++];
        e->name = name; e->ist_ordner = l->e[i].ordner; e->ist_link = l->e[i].link; e->groesse = l->e[i].groesse;
    }
    return d;
}

/* KeinOrdner: true = es gibt hier wirklich keinen Ordner */
static bool kein_ordner(sitzung *s, const char *pfad)
{
    if (strcmp(pfad, "/") == 0 || pfad[0] == 0) return false;
    const char *eltern = h_eltern(s->a, pfad), *name = h_blatt(s->a, pfad);
    antwort a = cwd(s, eltern);
    if (a.code >= 400) return a.code == 550 && kein_ordner(s, eltern);
    kdliste *l = daten_liste(s, eltern);
    if (!l) return false;                             /* nicht lesbar: nicht "gibt es nicht" annehmen */
    dliste *d = zerlege(s, l);
    for (int i = 0; i < d->n; i++) if (strcmp(d->e[i].name, name) == 0) return !d->e[i].ist_ordner;
    return true;
}

const char *s_verweigert_text(sitzung *s, const char *pfad, antwort a)
{
    return VERB(s->a, T("PS5 verweigert den Zugriff auf ", "PS5 denies access to "), pfad, " (", a.text, ")",
                T(" - zur Sicherheit wird abgebrochen.", " - stopping to be safe."));
}

/* Liste: Ordnerinhalt; NULL wenn es den Ordner nicht gibt.
 * streng: nur ein wirklich fehlender Ordner liefert NULL - ein vorhandener, aber
 * nicht lesbarer Ordner und jeder andere Fehler brechen ab. */
dliste *s_liste(sitzung *s, const char *pfad, bool streng)
{
    if (s->abbruch && s->abbruch(s->abbruch_ctx)) wirf(AUSN_ABBRUCH, "");
    antwort a = cwd(s, pfad);
    if (a.code >= 400) {
        s->letzter_fehler = VERB(s->a, "CWD ", pfad, ": ", a.text);
        if (streng && a.code != 550)
            wirf(AUSN_FEHLER, VERB(s->a, T("PS5 meldet beim Lesen von ", "PS5 reports when reading "), pfad, " : ", a.text));
        if (streng && !kein_ordner(s, pfad)) wirf(AUSN_FEHLER, s_verweigert_text(s, pfad, a));
        return NULL;
    }
    kdliste *l = daten_liste(s, pfad);
    if (!l) {
        if (streng) wirf(AUSN_IO, VERB(s->a, T("Ordner ", "Folder "), pfad,
                     T(" konnte nicht vollständig gelesen werden - bitte nochmal verbinden.", " could not be read completely - please connect again.")));
        return NULL;
    }
    return zerlege(s, l);
}

sliste s_namen(sitzung *s, const char *pfad, bool streng)
{
    sliste n = {0};
    dliste *l = s_liste(s, pfad, streng);
    if (!l) return n;
    for (int i = 0; i < l->n; i++) sl_add(s->a, &n, l->e[i].name);
    return n;
}

/* Existiert: nur ein klares "gibt es nicht" zählt als nein -
 * jeder andere Fehler bricht ab, statt fälschlich "schon weg" zu melden. */
bool s_existiert(sitzung *s, const char *pfad)
{
    const char *eltern = h_eltern(s->a, pfad), *name = h_blatt(s->a, pfad);
    antwort a = cwd(s, eltern);
    if (a.code == 550) {
        if (kein_ordner(s, eltern)) return false;     /* der Ordner darüber fehlt wirklich */
        wirf(AUSN_FEHLER, s_verweigert_text(s, eltern, a));
    }
    if (a.code >= 400) wirf(AUSN_FEHLER, VERB(s->a, T("PS5 meldet beim Prüfen von ", "PS5 reports when checking "), pfad, " : ", a.text));
    kdliste *l = daten_liste(s, eltern);
    if (!l) wirf(AUSN_FEHLER, VERB(s->a, T("Ordner ", "Folder "), eltern, T(" konnte nicht gelesen werden", " could not be read")));
    for (int i = 0; i < l->n; i++) if (strcmp(l->e[i].name, name) == 0) return true;   /* alle Zeilen, auch "." und Steuerzeichen */
    return false;
}

bool s_bytes(sitzung *s, const char *pfad, const unsigned char **d, size_t *n)
{
    pruefe_zeile(s, VERB(s->a, "RETR ", pfad));
    antwort a = verbindung(s->z->lies(s->z, s->a, pfad, d, n));
    if (a.code >= 400) { s->letzter_fehler = VERB(s->a, "RETR ", pfad, ": ", a.text); *d = NULL; *n = 0; return false; }
    return true;
}

/* NEU 1.1 (Selbstschutz S2): nur den Anfang einer Datei lesen - false bei Fehler oder wenn es das nicht gibt */
bool s_kopf(sitzung *s, const char *pfad, size_t n, const unsigned char **d, size_t *len)
{
    *d = NULL; *len = 0;
    if (!s->z->kopf) return false;
    antwort a = verbindung(s->z->kopf(s->z, s->a, pfad, n, d, len));
    if (a.code >= 400) { s->letzter_fehler = VERB(s->a, "KOPF ", pfad, ": ", a.text); *d = NULL; *len = 0; return false; }
    return true;
}

static const char *als_text(sitzung *s, const unsigned char *d, size_t n);
const char *s_text(sitzung *s, const char *pfad)
{
    const unsigned char *d; size_t n;
    if (!s_bytes(s, pfad, &d, &n)) return "";
    return als_text(s, d, n);
}
static const char *als_text(sitzung *s, const unsigned char *d, size_t n)
{
    /* UTF8.GetString liest bis zum Ende, auch über Nullbytes hinweg (z. B. in npbind.dat).
     * Ein C-Text endet am Nullbyte - darum wird jedes Nullbyte durch \x01 ersetzt: für alle
     * Muster des Kerns verhält sich \x01 wie \0 (kein Leerraum, keine Ziffer, ein
     * Steuerzeichen, passt auf "." und [^"]). */
    char *k = a_kopie_n(s->a, (const char *)d, n);
    for (size_t i = 0; i < n; i++) if (k[i] == 0) k[i] = 1;
    return h_net_text(s->a, k);
}

/* NEU 1.1 (Codex-Prüfung 26.10.2026): wie s_text, aber eine vorhandene und trotzdem nicht lesbare Datei
 * bricht ab (statt "" zu liefern). Nur eine wirklich fehlende Datei ergibt "". */
const char *s_text_streng(sitzung *s, const char *pfad)
{
    const unsigned char *d; size_t n;
    if (s_bytes(s, pfad, &d, &n)) return als_text(s, d, n);   /* genau einmal gelesen */
    if (!s_existiert(s, pfad)) return "";
    wirf(AUSN_FEHLER, VERB(s->a, T("Datei ", "File "), pfad, T(" ist da, ließ sich aber nicht lesen", " exists but could not be read")));
    return "";
}

bool s_schreiben(sitzung *s, const char *pfad, const void *d, size_t n)
{
    pruefe_zeile(s, VERB(s->a, "STOR ", pfad));
    antwort a = verbindung(s->z->schreibe(s->z, s->a, pfad, d, n));
    return a.code == 226 || a.code == 250;
}

/* Gesund: antwortet die Zugriffsschicht sauber? */
bool s_gesund(sitzung *s)
{
    volatile bool ok = false;
    VERSUCHE {
        if (s_befehl(s, "NOOP", NULL).code == 200) ok = s_liste(s, "/user", false) != NULL;
    } FANGE {
        if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN();
        ok = false;
    } ENDE_VERSUCHE;
    return ok;
}
