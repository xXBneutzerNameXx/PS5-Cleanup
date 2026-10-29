/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * kern_loeschen.c - Löschen, ShadowMount, Größen und der Löschablauf,
 * 1:1 aus Kern.cs und Ausfuehren.cs (1.0.5):
 *   Freigeben, LoescheBaum, Loeschen, LeerenOrdnerLoeschen, LeerenOrdnerEntfernen,
 *   KeineSpieleEingebunden, ShadowMountAnhalten, BaumGroesse, EintragGroesse,
 *   GroessenImHintergrund, ErmittleGroessen, GroesseText, GroesseSatz,
 *   FreigeraeumtText, FuehreLoeschung, SmSchlusstext.
 *
 * Was es nur bei FTP gibt (Zeitlimits, Verbindung erneuern), ist hier eine
 * Stelle ohne Wirkung - die Abfolge der Prüfungen bleibt dieselbe.
 */
#include "kern.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define A (k->a)
static bool sl_enthaelt(const sliste *l, const char *s) { for (int i = 0; i < l->n; i++) if (strcmp(l->e[i], s) == 0) return true; return false; }  /* NEU 1.0.6: wie in kern_suche.c */
static sliste L1(arena *a, const char *x) { sliste l = {0}; sl_add(a, &l, x); return l; }
static void melde(kern *k, const char *t) { k->melde(k->melde_ctx, t); if (k->SELBST_DIAG && zugriff_diagnose) zugriff_diagnose(VERB(A, "Selbstentfernung: ", t ? t : "")); }   /* NEU 1.0.6: Diagnose */
static void warte(kern *k, int ms) { k->warte(k->warte_ctx, ms); }
static double jetzt(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec + (double)t.tv_nsec / 1e9; }
static void sperren(kern *k) { pthread_mutex_lock((pthread_mutex_t *)k->groessen_sperre); }
static void entsperren(kern *k) { pthread_mutex_unlock((pthread_mutex_t *)k->groessen_sperre); }

void kern_verbinden(kern *k, zugriff *z)
{
    k->S = sitzung_neu(z, A);
    k->verbindung_nr++;
}

/* -------------------------------------------------------------
 * Löschen (Kern.cs)
 * ------------------------------------------------------------- */
static void freigeben(kern *k, const char *d)
{
    s_befehl(k->S, "SITE CHMOD 777", d);
    dliste *l = s_liste(k->S, d, false);
    if (!l) return;
    for (int i = 0; i < l->n; i++) if (l->e[i].ist_ordner) freigeben(k, VERB(A, d, "/", l->e[i].name));
}

static void loesche_baum(kern *k, const char *d)
{
    dliste *l = s_liste(k->S, d, false);
    if (l) {
        for (int i = 0; i < l->n; i++) {
            datei *e = &l->e[i];
            const char *p = VERB(A, d, "/", e->name);
            if (e->ist_ordner) loesche_baum(k, p);
            else if (e->ist_link) s_befehl(k->S, "DELE", p);         /* nur die Verknüpfung, kein CHMOD (würde dem Ziel gelten) */
            else { s_befehl(k->S, "SITE CHMOD 777", p); s_befehl(k->S, "DELE", p); }
        }
    }
    s_befehl(k->S, "RMD", d);
}

static const char *stopp_text(void) { return T("  SICHERHEITSSTOPP: ", "  SAFETY STOP: "); }
/* NEU 1.1, Selbstschutz: zusätzliche Sperre VOR der Schutzliste aus 1.0.5 */
static bool selbst_gesperrt(kern *k, const char *p, bool melden)
{
    if (!kern_selbstschutz_gesperrt(k, p)) return false;
    if (melden) melde(k, VERB(A, stopp_text(), p, T(" gehört zu PS5 Cleanup und wird erst ganz zum Schluss entfernt - jetzt nichts gelöscht.", " belongs to PS5 Cleanup and is only removed at the very end - nothing deleted now.")));
    return true;
}

/* NEU 1.1 (Entscheidung da_Chris86 25.10.2026, A1/A2): Namen mit beschädigten Zeichen (ungültiges
 * UTF-8) oder mit Leerzeichen am Anfang konnte 1.0.5 über FTP nicht löschen. 1.1 bleibt dabei:
 * nie automatisch löschen, sondern Sicherheitsstopp mit Eintrag im Protokoll. */
static bool beschaedigt(kern *k, const char *p, bool melden)
{
    bool b = strcmp(h_net_text(A, p), p) != 0;
    for (const char *c = p; *c && !b; c++) if (c[0] == '/' && c[1] == ' ') b = true;
    if (b && melden) melde(k, VERB(A, stopp_text(), h_net_text(A, p), T(" hat beschädigte Zeichen im Namen - nichts gelöscht.", " has damaged characters in its name - nothing deleted.")));
    return b;
}

/* NEU 1.1 (Anforderungen 27.10.2026, Codex-Prüfung 7): Liegt in einem Ordner irgendetwas Geschütztes
 * (Sony-System, Konten, Lizenzen, Sony-Apps, Astro, PS5 Cleanup/Launcher, laufende Aufnahme), wird der
 * Ordner gar nicht angefasst - auch kein CHMOD. Nicht lesbar = geschützt. Verknüpfungen werden nicht verfolgt. */
/* Was als Nachkomme eines erlaubten Ordners geschützt ist. (kern_nie_anfassen gilt nur für den gewählten Pfad
 * selbst - seine Ausnahmen sind genau gewählte Pfade, deren Inhalt dann mitgeht.) */
static bool nachkomme_geschuetzt(kern *k, const char *c)
{
    /* Sony-App-Ordner NPXS<5 Ziffern> irgendwo im Pfad (auch im gewählten Ordner selbst) - nicht in der Galerie,
     * dort heißen auch normale Aufnahmen von Sony-Apps so (Codex-Prüfung 9) */
    return (!h_m(c, "^/user/av_contents/") && h_m(c, "/NPXS\\d{5}(/|$)")) || h_m(c, "^/user/home/[^/]+/savedata[^/]*/sce_") || h_m(c, "^/user/license/(act\\.dat|nobackup)")
        || kern_laufende_aufnahme(c) || kern_astro_pfad(k, c) || kern_selbstschutz_gesperrt(k, c);
}
static bool baum_hat_geschuetztes(kern *k, const char *d, const char **fund, int tiefe)
{
    if (tiefe > 64) { *fund = d; return true; }
    dliste *volatile l = NULL;
    volatile bool fehl = false;
    VERSUCHE { l = s_liste(k->S, d, true); }
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); fehl = true; } ENDE_VERSUCHE;
    if (fehl || l == NULL) { *fund = d; return true; }
    for (int i = 0; i < l->n; i++) {
        const char *c = VERB(A, d, "/", l->e[i].name);
        /* Codex-Prüfung 37: ein Spiel-Abbild eines Jailbreak-Einstiegs IN einem gewählten Ordner - nur einzeln löschbar */
        if (nachkomme_geschuetzt(k, c) || kern_jb_abbild(c)) { *fund = c; return true; }
        if (l->e[i].ist_ordner && !l->e[i].ist_link && baum_hat_geschuetztes(k, c, fund, tiefe + 1)) return true;
    }
    return false;
}

/* Codex-Prüfung 37: nicht gewählter Eintrag eines Jailbreak-Einstiegs (auch gesperrt), dessen Pfad oder Spieldatei in d liegt */
static const char *funde_jb(kern *k, const char *d)
{
    size_t ld = strlen(d);
    /* Codex-Prüfung 38: Spieldatei einer Kachel eines Einstiegs (auch gemeinsam genutzt, gesperrt oder ohne Kennung im Namen) */
    for (int x = 0; x < k->JB_QUELLEN.n; x++) {
        const char *q = k->JB_QUELLEN.e[x];
        if ((strcmp(q, d) == 0 || (strncmp(q, d, ld) == 0 && q[ld] == '/')) && s_existiert(k->S, q)) return T("Spieldatei eines Jailbreak-Einstiegs", "game file of a jailbreak entry");
    }
    for (int x = 0; x < k->Funde.n; x++) {
        eintrag *f = k->Funde.e[x];
        if (f->Wahl) continue;
        bool jb = f->Jailbreak;
        for (int j = 0; j < f->PfadeBasis.n && !jb; j++) if (kern_jb_weg(f->PfadeBasis.e[j])) jb = true;
        if (!jb) continue;
        for (int j = -1; j < f->PfadeBasis.n; j++) {
            const char *q = j < 0 ? f->Quelle : f->PfadeBasis.e[j];
            if (q && q[0] && (strcmp(q, d) == 0 || (strncmp(q, d, ld) == 0 && q[ld] == '/'))) return kern_jb_weg_eintrag(f);
        }
    }
    return NULL;
}
/* Codex-Prüfung 36: Name des Jailbreaks, wenn in d (oder d selbst) etwas von einem Jailbreak-Einstieg liegt - sonst NULL */
static const char *baum_jb(kern *k, const char *d, int tiefe)
{
    if (tiefe == 0) { const char *f = funde_jb(k, d); if (f) return f; }
    const char *w = kern_jb_weg(d);
    if (w || tiefe > 64) return w;
    dliste *volatile l = NULL;
    VERSUCHE { l = s_liste(k->S, d, true); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); l = NULL; } ENDE_VERSUCHE;
    if (l == NULL) return NULL;
    for (int i = 0; i < l->n; i++) {
        const char *c = VERB(A, d, "/", l->e[i].name);
        if ((w = kern_jb_weg(c)) != NULL) return w;
        if (l->e[i].ist_ordner && !l->e[i].ist_link && (w = baum_jb(k, c, tiefe + 1)) != NULL) return w;
    }
    return NULL;
}

static bool loeschen(kern *k, const char *p)
{
    if (selbst_gesperrt(k, p, true)) return false;
    /* NEU 27.10.2026 (Codex-Prüfung 14): nicht sicher zugeordnete Spieldatei - weder sie noch ein Ordner darüber */
    for (int i = 0; i < k->SperrQuellen.n; i++) {
        const char *sq = k->SperrQuellen.e[i]; size_t l = strlen(p);
        size_t lq = strlen(sq);
        if (strcmp(sq, p) == 0 || (strncmp(sq, p, l) == 0 && sq[l] == '/') || (strncmp(p, sq, lq) == 0 && p[lq] == '/')) {   /* auch alles darin */
            melde(k, VERB(A, stopp_text(), p, T(" enthält eine nicht sicher zugeordnete Spieldatei (", " contains a game file that is not assigned for certain ("), sq, T(") - nichts gelöscht.", ") - nothing deleted.")));
            return false;
        }
    }
    if (beschaedigt(k, p, true)) return false;
    if (!kern_erlaubt(k, p)) { melde(k, VERB(A, stopp_text(), p, T(" ist gesperrt - nichts gelöscht.", " is protected - nothing deleted."))); return false; }
    if (!kern_ohne_verknuepfung(k, p)) { melde(k, VERB(A, stopp_text(), p, T(" liegt hinter einer Verknüpfung - nichts gelöscht.", " is behind a symlink - nothing deleted."))); return false; }
    if (!s_existiert(k->S, p)) { k->WAR_SCHON_WEG++; return true; }
    /* Ist der Eintrag selbst eine Verknüpfung: nur sie entfernen, nie ihr Ziel */
    dliste *el = s_liste(k->S, h_eltern(A, p), true);
    datei *selbst = NULL;
    if (el) { const char *b = h_blatt(A, p); for (int i = 0; i < el->n; i++) if (strcmp(el->e[i].name, b) == 0) { selbst = &el->e[i]; break; } }
    if (selbst != NULL && selbst->ist_link) {
        s_befehl(k->S, "DELE", p);
        return !s_existiert(k->S, p);
    }
    if (selbst == NULL) { melde(k, VERB(A, stopp_text(), p, T(" steht nicht in der Ordnerliste - nichts gelöscht.", " is not in the folder listing - nothing deleted."))); return false; }
    /* 27.10.2026 (Codex-Prüfung 24): auch für einzeln gewählte Dateien und unabhängig vom Selbstschutz (gleich wie Windows 1.0.6) */
    {
        const char *fund = NULL;
        if (nachkomme_geschuetzt(k, p)) fund = p;
        if (fund || (selbst->ist_ordner && baum_hat_geschuetztes(k, p, &fund, 0))) {
            melde(k, VERB(A, stopp_text(), p, T(" enthält Geschütztes (", " contains something protected ("), fund, T(") - nichts gelöscht.", ") - nothing deleted.")));
            return false;
        }
    }
    s_befehl(k->S, "SITE CHMOD 777", p);
    s_befehl(k->S, "DELE", p);                           /* falls Datei */
    if (!s_existiert(k->S, p)) return true;
    freigeben(k, p);                                      /* Ordner */
    antwort ra = s_befehl(k->S, "RMDA", p);
    if (s_existiert(k->S, p)) {
        /* NEU 1.1 (Codex-Prüfung 26.10.2026): Hat die abgesicherte Löschung abgelehnt (Einbindung, Verknüpfung
         * im Weg, Lesefehler ...), wird NICHT über den Ausweichweg Datei für Datei weitergelöscht. Der
         * Ausweichweg bleibt nur, wenn RMDA gar nicht vorhanden ist (502), wie in 1.0.5. */
        /* 30.10.2026: Windows erkennt zusätzlich "500 Unknown command" (zftpd). Hier nicht nötig - die PS5-App löscht direkt, nicht
         * über FTP (RMDA kommt aus zugriff_datei.c, nie 500). Bewusst unverändert, damit die freigegebene ELF genau diesem Quelltext entspricht. */
        if (ra.code != 502) {   /* seit 27.10.2026 auch ohne Selbstschutz, gleich wie Windows 1.0.6 (Codex-Prüfung 24) */
            melde(k, VERB(A, stopp_text(), p, T(" ließ sich nicht sicher als Ganzes löschen - der Rest bleibt stehen.", " could not be deleted safely as a whole - the rest is kept.")));
            return false;
        }
        loesche_baum(k, p);
    }
    return !s_existiert(k->S, p);
}

/* Leeren Ordner entfernen, der als eigener Eintrag gewählt wurde: nur RMD -
 * RMD löscht nie Inhalt mit. Ist inzwischen etwas drin, schlägt es fehl. */
static bool leeren_ordner_loeschen(kern *k, const char *p)
{
    if (selbst_gesperrt(k, p, true)) return false;
    if (beschaedigt(k, p, true)) return false;
    if (!kern_erlaubt(k, p)) { melde(k, VERB(A, stopp_text(), p, T(" ist gesperrt - nichts gelöscht.", " is protected - nothing deleted."))); return false; }
    if (!kern_ohne_verknuepfung(k, p)) { melde(k, VERB(A, stopp_text(), p, T(" liegt hinter einer Verknüpfung - nichts gelöscht.", " is behind a symlink - nothing deleted."))); return false; }
    if (!s_existiert(k->S, p)) { k->WAR_SCHON_WEG++; return true; }
    dliste *l = s_liste(k->S, p, true);
    if (l == NULL || l->n != 0) { melde(k, VERB(A, "  ", p, T(" ist nicht mehr leer - bleibt stehen.", " is no longer empty - kept."))); return false; }
    s_befehl(k->S, "RMD", p);
    return !s_existiert(k->S, p);
}

/* Nur einen wirklich leeren Ordner entfernen (RMD löscht nie Inhalt mit) */
static void leeren_ordner_entfernen(kern *k, const char *o)
{
    VERSUCHE {
        if (!selbst_gesperrt(k, o, false) && !beschaedigt(k, o, false) && kern_erlaubt(k, o)) {
            dliste *l = s_liste(k->S, o, false);
            if (l != NULL && l->n == 0) {
                antwort a = s_befehl(k->S, "RMD", o);
                if (a.code < 400) melde(k, VERB(A, k->simulation ? T("  würde gelöscht   leerer Ordner ", "  would be deleted   empty folder ") : T("  gelöscht   leerer Ordner ", "  deleted   empty folder "), o));
            }
        }
    } FANGE { } ENDE_VERSUCHE;                              /* catch { } - auch Abbruch */
}

/* -------------------------------------------------------------
 * ShadowMount anhalten (Kern.cs)
 * ------------------------------------------------------------- */
#define SM_MOUNTS "/mnt/shadowmnt"
static int abmeldungen(const char *log) { return r_anzahl(log, "\\[SHUTDOWN\\] cleanup complete"); }

/* Sind gerade Spiele eingebunden? 1 = keine, 0 = ja, -1 = nicht feststellbar */
static int keine_spiele_eingebunden(kern *k)
{
    volatile int erg = 1;
    VERSUCHE {
        dliste *l = s_liste(k->S, SM_MOUNTS, true);
        if (l) {
            for (int i = 0; i < l->n && erg == 1; i++) {
                if (!l->e[i].ist_ordner) continue;
                volatile int fehl = 0;
                dliste *volatile u = NULL;
                VERSUCHE { u = s_liste(k->S, VERB(A, SM_MOUNTS "/", l->e[i].name), true); }
                FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); fehl = 1; } ENDE_VERSUCHE;
                if (fehl) erg = -1;
                else if (u != NULL && u->n > 0) erg = 0;
            }
        }
    } FANGE {
        if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN();
        erg = -1;
    } ENDE_VERSUCHE;
    return erg;
}
static smzustand unbekannt(kern *k, const char *grund)
{
    k->SM_GRUND = grund;
    melde(k, VERB(A, "  ", grund));
    return SM_UNBEKANNT;
}

static smzustand sm_innen(kern *k)
{
    dliste *ordner = s_liste(k->S, SM_ORDNER, true);
    if (ordner == NULL) {
        int frei = keine_spiele_eingebunden(k);
        if (frei == 1) { melde(k, T("  ShadowMount ist nicht eingerichtet und es sind keine Spiele eingebunden.", "  ShadowMount is not set up and no games are mounted.")); return SM_LAEUFT_NICHT; }
        return unbekannt(k, frei == 0 ? T("Es sind Spiele eingebunden, aber ShadowMount ist nicht erreichbar.", "Games are mounted, but ShadowMount cannot be reached.")
                                      : T("Ob Spiele eingebunden sind, ließ sich nicht prüfen.", "Could not check whether games are mounted."));
    }
    /* nur eine Abmeldung NACH dem Anhalten zählt (alte Zeilen im Protokoll nicht) */
    int vorher = abmeldungen(s_text(k->S, SM_ORDNER "/debug.log"));
    if (!s_schreiben(k->S, SM_ORDNER "/STOP", NULL, 0))
        return unbekannt(k, T("Die Stopp-Datei für ShadowMount ließ sich nicht anlegen.", "Could not create the stop file for ShadowMount."));
    /* Läuft es, holt es sich die STOP-Datei innerhalb von ~3 s selbst ab. */
    bool abgeholt = false;
    double ende = jetzt() + 8;
    while (jetzt() < ende) {
        warte(k, 500);
        if (!s_existiert(k->S, SM_ORDNER "/STOP")) { abgeholt = true; break; }
    }
    if (!abgeholt) {
        s_befehl(k->S, "DELE", SM_ORDNER "/STOP");      /* niemand hat sie abgeholt */
        int frei = keine_spiele_eingebunden(k);
        if (frei == 1) { melde(k, T("  ShadowMount läuft nicht (keine Reaktion, keine Spiele eingebunden).", "  ShadowMount is not running (no response, no games mounted).")); return SM_LAEUFT_NICHT; }
        return unbekannt(k, frei == 0 ? T("ShadowMount reagiert nicht (evtl. älter als 1.7), aber Spiele sind eingebunden.", "ShadowMount does not respond (maybe older than 1.7), but games are mounted.")
                                      : T("ShadowMount reagiert nicht und ob Spiele eingebunden sind, ließ sich nicht prüfen.", "ShadowMount does not respond and it could not be checked whether games are mounted."));
    }
    k->SM_STOP_GESENDET = true;
    /* Bestätigung: Abmeldung im Protokoll oder alle Spiele ausgehängt (falls das Protokoll abgeschaltet ist) */
    ende = jetzt() + 90;
    while (jetzt() < ende) {
        warte(k, 2000);
        bool abgemeldet = abmeldungen(s_text(k->S, SM_ORDNER "/debug.log")) > vorher;
        if (!abgemeldet && keine_spiele_eingebunden(k) == 1) { warte(k, 3000); abgemeldet = keine_spiele_eingebunden(k) == 1; }
        if (abgemeldet) { melde(k, T("  ShadowMount ist beendet.", "  ShadowMount has stopped.")); k->SM_ANGEHALTEN = true; return SM_ANGEHALTEN_Z; }
    }
    return unbekannt(k, T("ShadowMount hat sich nach 90 s nicht bestätigt beendet.", "ShadowMount did not confirm it stopped within 90 s."));
}

smzustand kern_shadowmount_anhalten(kern *k)
{
    k->SM_ANGEHALTEN = false; k->SM_STOP_GESENDET = false; k->SM_GRUND = "";
    melde(k, T("Halte ShadowMount an (sonst legt es die Kacheln sofort wieder an) ...", "Stopping ShadowMount (otherwise it recreates the tiles right away) ..."));
    /* NEU 1.1 (Entscheidung da_Chris86 25.10.2026, A9): In der Simulation wird ShadowMount NIE
     * angefasst. Die Simulation nimmt einen erfolgreichen Stopp an und sagt das deutlich. */
    if (k->simulation) {
        melde(k, T("  SIMULATION: Hier würde ShadowMount angehalten - es wird nichts angehalten.", "  SIMULATION: ShadowMount would be stopped here - nothing is stopped."));
        return SM_ANGEHALTEN_Z;
    }
    volatile smzustand z = SM_UNBEKANNT;
    VERSUCHE { z = sm_innen(k); }
    FANGE {
        if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN();
        z = unbekannt(k, VERB(A, T("ShadowMount-Zustand nicht feststellbar: ", "ShadowMount state unknown: "), ausn_text));
    } ENDE_VERSUCHE;
    return z;
}

/* -------------------------------------------------------------
 * Größen (Ausfuehren.cs)
 * ------------------------------------------------------------- */
void kern_groessen_zuruecksetzen(kern *k) { sperren(k); k->PFAD_GROESSE = mt_neu(false); entsperren(k); }
bool kern_pfad_gezaehlt(kern *k, const char *p) { sperren(k); bool r = mt_hat(&k->PFAD_GROESSE, p); entsperren(k); return r; }
static void groesse_merken(kern *k, const char *p, const groesse *g, bool nur_wenn_neu)
{
    sperren(k);
    if (!nur_wenn_neu || !mt_hat(&k->PFAD_GROESSE, p)) {
        groesse *x = a_hole(k->ga, sizeof *x); *x = *g;
        mt_setze(k->ga, &k->PFAD_GROESSE, p, x);
    }
    entsperren(k);
}
static void plus(groesse *a, const groesse *g) { a->Bytes += g->Bytes; a->Dateien += g->Dateien; a->Ordner += g->Ordner; if (g->Gekappt) a->Gekappt = true; if (g->Lesefehler) a->Lesefehler = true; }

typedef void (*lauf_fn)(void *ctx, long long b, int n);
/* BaumGroesse: Größe eines Pfads (Datei oder Ordner samt Inhalt). Gekappt = nicht ganz durchgezählt. */
static groesse baum_groesse(sitzung *s, arena *a, const char *p, double deadline, lauf_fn lauf, void *lctx)
{
    groesse erg = {0};
    dliste *volatile l = NULL;
    volatile int fall = 0;
    VERSUCHE { l = s_liste(s, h_eltern(a, p), true); }       /* streng: nur "gibt es nicht" ist null */
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); fall = ausn_art == AUSN_IO ? 1 : 2; } ENDE_VERSUCHE;
    if (fall == 1) { erg.Gekappt = true; erg.Lesefehler = true; return erg; }   /* Verbindung evtl. aus dem Takt */
    if (fall == 2) { erg.Gekappt = true; return erg; }                            /* saubere Fehlerantwort */
    if (l == NULL) return erg;
    const char *b = h_blatt(a, p);
    datei *e = NULL;
    for (int i = 0; i < l->n; i++) if (strcmp(l->e[i].name, b) == 0) { e = &l->e[i]; break; }
    if (e == NULL) return erg;
    if (!e->ist_ordner) { erg.Bytes = e->groesse; erg.Dateien = 1; if (lauf) lauf(lctx, e->groesse, 1); return erg; }
    erg.Ordner = 1;
    sliste stapel = {0}; sl_add(a, &stapel, p);
    while (stapel.n > 0) {
        if (jetzt() > deadline || erg.Dateien + erg.Ordner > 200000) { erg.Gekappt = true; break; }
        const char *d = stapel.e[--stapel.n];
        dliste *volatile kl = NULL;
        volatile int f2 = 0;
        VERSUCHE { kl = s_liste(s, d, true); }
        FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); f2 = ausn_art == AUSN_IO ? 1 : 2; } ENDE_VERSUCHE;
        if (f2 == 1) { erg.Gekappt = true; erg.Lesefehler = true; break; }
        if (f2 == 2) { erg.Gekappt = true; continue; }            /* dieser Ordner nicht lesbar: Rest weiterzählen */
        if (kl == NULL) { erg.Gekappt = true; continue; }         /* im Ordner darüber gelistet, aber nicht lesbar */
        long long bb = 0; int n = 0;
        for (int i = 0; i < kl->n; i++) {
            datei *x = &kl->e[i];
            if (x->ist_ordner) { erg.Ordner++; sl_add(a, &stapel, VERB(a, d, "/", x->name)); }
            else if (!x->ist_link) { erg.Dateien++; erg.Bytes += x->groesse; bb += x->groesse; n++; }
            else { erg.Dateien++; n++; }
        }
        if (lauf) lauf(lctx, bb, n);
    }
    return erg;
}

/* EintragGroesse: aus den gemerkten Pfad-Größen (NULL = noch nicht alles gezählt) */
groesse *kern_eintrag_groesse(kern *k, const eintrag *f, bool wieGefunden) { return kern_eintrag_groesse_in(k, A, f, wieGefunden); }
/* NEU 1.1: dasselbe mit eigenem Speicher (für die Anzeige aus dem Oberflächen-Faden) */
groesse *kern_eintrag_groesse_in(kern *k, arena *ar, const eintrag *f, bool wieGefunden)
{
    groesse *g = a_hole(ar, sizeof *g);
    const sliste *pf = wieGefunden ? &f->PfadeBasis : &f->Pfade;
    sliste gesehen = {0};
    sperren(k);
    for (int i = 0; i < pf->n; i++) {
        bool doppelt = false; for (int j = 0; j < gesehen.n; j++) if (strcmp(gesehen.e[j], pf->e[i]) == 0) doppelt = true;   /* Distinct() */
        if (doppelt) continue;
        sl_add(ar, &gesehen, pf->e[i]);
        groesse *x = mt_hole(&k->PFAD_GROESSE, pf->e[i]);
        if (!x) { entsperren(k); return NULL; }
        plus(g, x);
    }
    entsperren(k);
    return g;
}

/* GroessenImHintergrund: über eine eigene "Verbindung" (eigene Sitzung, eigener Speicher)
 * alle Einträge durchzählen, bis alles gezählt ist oder stopp() true liefert. */
void kern_groessen_im_hintergrund(kern *k, bool (*stopp)(void *), void *sc, void (*fertig)(void *, eintrag *), void *fc)
{
    arena *a = arena_neu();
    sitzung *s = sitzung_neu(k->S->z, a);
    s->abbruch = stopp; s->abbruch_ctx = sc;
    pliste liste = {0}; for (int i = 0; i < k->Funde.n; i++) pl_add(a, &liste, k->Funde.e[i]);
    VERSUCHE {
        for (int i = 0; i < liste.n; i++) {
            eintrag *e = liste.e[i];
            if (stopp(sc)) break;
            sliste pfade = sl_kopie(a, &e->PfadeBasis);
            if (e->Kat == 1 && e->Quelle[0]) sl_add(a, &pfade, e->Quelle);
            bool aufhoeren = false;
            for (int j = 0; j < pfade.n && !aufhoeren; j++) {
                bool doppelt = false; for (int m = 0; m < j; m++) if (strcmp(pfade.e[m], pfade.e[j]) == 0) doppelt = true;
                if (doppelt || kern_pfad_gezaehlt(k, pfade.e[j])) continue;
                groesse g = baum_groesse(s, a, pfade.e[j], jetzt() + 120, NULL, NULL);
                if (g.Lesefehler) { aufhoeren = true; break; }    /* nichts merken, aufhören */
                groesse_merken(k, pfade.e[j], &g, true);
            }
            if (aufhoeren) break;
            fertig(fc, e);
        }
    } FANGE { } ENDE_VERSUCHE;                                   /* catch { } */
    arena_frei(a);
}

typedef struct { kern *k; long long b; int n; double zuletzt; } laufstand;
static void lauf_zaehlen(void *c, long long b, int n)
{
    laufstand *l = c;
    l->b += b; l->n += n;
    if (jetzt() - l->zuletzt > 0.4) { l->zuletzt = jetzt(); if (l->k->GroesseStand) l->k->GroesseStand(l->k->groesse_ctx, l->b, l->n); }
}

/* ErmittleGroessen: Größe aller gewählten Einträge (vor dem Löschen). */
groesse kern_ermittle_groessen(kern *k)
{
    kern_abhaengige_mitnehmen(k);
    groesse sum = {0};
    sliste gesehen = {0};
    double deadline = jetzt() + 60;
    laufstand ls = { k, 0, 0, 0 };
    bool gekappt = false;
    /* (FTP: Verbindung auffrischen / Zeitlimit 4 s - auf der Konsole ohne Wirkung) */
    s_gesund(k->S);
    pliste gw = kern_gewaehlt(k);
    for (int i = 0; i < gw.n; i++) {
        eintrag *f = gw.e[i];
        groesse g = {0};
        for (int j = 0; j < f->Pfade.n; j++) {
            const char *p = f->Pfade.e[j];
            bool neu = true; for (int m = 0; m < gesehen.n; m++) if (strcmp(gesehen.e[m], p) == 0) neu = false;   /* HashSet.Add */
            if (!neu) continue;
            sl_add(A, &gesehen, p);
            sperren(k); groesse *xp = mt_hole(&k->PFAD_GROESSE, p); groesse x = xp ? *xp : (groesse){0}; entsperren(k);
            bool da = xp != NULL;
            if (da && x.Gekappt) da = false;                  /* nicht ganz gezählt: nochmal versuchen */
            if (!da) {
                x = baum_groesse(k->S, A, p, deadline, lauf_zaehlen, &ls);
                if (!x.Lesefehler && !x.Gekappt) groesse_merken(k, p, &x, false);
            }
            else lauf_zaehlen(&ls, x.Bytes, x.Dateien);
            plus(&g, &x);
        }
        f->Bytes = g.Bytes; f->Dateien = g.Dateien; f->Ordner = g.Ordner; f->HatGroesse = true;
        if (g.Gekappt) gekappt = true;
        sum.Bytes += g.Bytes; sum.Dateien += g.Dateien; sum.Ordner += g.Ordner;
    }
    sum.Gekappt = gekappt;
    k->GROESSE_GEKAPPT = gekappt;
    return sum;
}

/* Zahlen wie .NET mit "N" und der Kultur de-DE bzw. en-US:
 * 15 gültige Stellen, dann ab 5 aufrunden (weg von null), Tausendertrenner. */
static const char *zahl_n(arena *a, double v, int stellen)
{
    if (v < 0) v = -v;                                   /* kommt hier nicht vor */
    char ex[64]; snprintf(ex, sizeof ex, "%.14e", v);   /* d.dddddddddddddde+XX */
    char ziff[16]; int nz = 0;
    for (char *c = ex; *c && *c != 'e'; c++) if (*c >= '0' && *c <= '9') ziff[nz++] = *c;
    int e10 = atoi(strchr(ex, 'e') + 1);
    /* alle Ziffern mit Komma-Position: "all" = Ganzteil + Nachkommastellen */
    char all[128]; int n = 0, komma;
    if (e10 >= 0) { for (int i = 0; i < nz; i++) all[n++] = ziff[i]; komma = e10 + 1; }
    else { all[n++] = '0'; for (int i = 0; i < -e10 - 1; i++) all[n++] = '0'; for (int i = 0; i < nz; i++) all[n++] = ziff[i]; komma = 1; }
    while (n < komma + stellen + 1) all[n++] = '0';
    /* runden: Ziffer hinter der letzten Nachkommastelle */
    int lae = komma + stellen;
    char r[130]; r[0] = '0'; memcpy(r + 1, all, (size_t)lae);
    if (all[lae] >= '5') { int i = lae; while (i >= 0) { if (r[i] == '9') { r[i] = '0'; i--; } else { r[i]++; break; } } }
    /* führende Nullen im Ganzteil weg (eine bleibt) */
    int anf = 0, g = komma + 1;
    while (g > 1 && r[anf] == '0') { anf++; g--; }
    char tsep = SP_EN ? ',' : '.', dsep = SP_EN ? '.' : ',';
    char o[200]; int w = 0;
    for (int i = 0; i < g; i++) { if (i && (g - i) % 3 == 0) o[w++] = tsep; o[w++] = r[anf + i]; }
    if (stellen) { o[w++] = dsep; for (int i = 0; i < stellen; i++) o[w++] = r[anf + g + i]; }
    o[w] = 0;
    return a_kopie(a, o);
}

/* NEU 1.1: {0:N0} für die Oberfläche (z.B. "bisher ... in 12.345 Dateien") */
const char *kern_zahl_n0(arena *a, long long v) { return zahl_n(a, (double)v, 0); }

const char *kern_groesse_text(arena *a, long long bytes)
{
    const double KB = 1024, MB = KB * 1024, GB = MB * 1024;
    if (bytes >= GB) return VERB(a, zahl_n(a, (double)bytes / GB, 1), " GB");
    if (bytes >= MB) return VERB(a, zahl_n(a, (double)bytes / MB, 1), " MB");
    if (bytes >= KB) return VERB(a, zahl_n(a, (double)bytes / KB, 0), " KB");
    return VERB(a, a_fmt(a, "%lld", bytes), SP_EN ? " bytes" : " Byte");
}
const char *kern_groesse_satz(arena *a, groesse g)
{
    const char *t = VERB(a, g.Gekappt ? T("mindestens", "at least") : T("etwa", "about"), " ", kern_groesse_text(a, g.Bytes),
                         T(" in ", " in "), zahl_n(a, g.Dateien, 0), T(" Dateien und ", " files and "), zahl_n(a, g.Ordner, 0), T(" Ordnern", " folders"));
    if (g.Gekappt) t = VERB(a, t, T(" (große Ordner nicht ganz durchgezählt)", " (large folders not fully counted)"));
    return t;
}
const char *kern_freigeraeumt_text(arena *a, groesse g)
{
    return VERB(a, T("Freigeräumt: ", "Freed: "), g.Gekappt ? T("mindestens ", "at least ") : "", kern_groesse_text(a, g.Bytes),
                T(" in ", " in "), zahl_n(a, g.Dateien, 0), T(" Dateien und ", " files and "), zahl_n(a, g.Ordner, 0), T(" Ordnern", " folders"));
}

/* -------------------------------------------------------------
 * Ausführen (Jailbreak-Tools zuletzt). weiter() wird gefragt, wenn
 * ShadowMount nicht sauber angehalten werden konnte; true = trotzdem löschen.
 * ------------------------------------------------------------- */
/* NEU 1.0.6 (da_Chris86 28.10.2026): Kachel über Sonys Deinstallations-Dienst entfernen - wie das PS5-Menü, damit
 * kein Eintrag im Menü stehen bleibt. 1 = Sony hat sie entfernt (App-Ordner ist weg); 0 = nicht (*grund sagt warum).
 * Sony arbeitet nach dem Auftrag im Hintergrund weiter - darum wird gewartet, bis der App-Ordner weg ist (höchstens 60 s). */
static const char *antwort_grund(arena *a, antwort r) { const char *t = r.text ? r.text : ""; return a_kopie(a, strlen(t) > 4 ? t + 4 : t); }
static int sony_entfernen(kern *k, const char *ordner, const char *id, const char **grund)
{
    antwort r = s_befehl(k->S, "SITE UNINSTALL?", id);
    if (r.code != 200) { *grund = antwort_grund(A, r); return 0; }
    r = s_befehl(k->S, "SITE UNINSTALL", ordner);
    if (r.code != 200) { *grund = VERB(A, T("Entfernung unvollständig - Sony hat abgelehnt (", "Removal incomplete - Sony refused ("), antwort_grund(A, r), ")"); return 0; }
    double bis = jetzt() + 60;
    while (s_existiert(k->S, ordner)) {
        /* Codex-Prüfung 32: dann NICHT selbst weiterlöschen (Sony arbeitet evtl. noch - Wettlauf) */
        if (jetzt() > bis) { *grund = T("Sony löscht nach 60 s noch im Hintergrund - der Rest bleibt vorerst stehen, später nochmal nachschauen", "Sony is still deleting in the background after 60 s - the rest stays for now, scan again later"); return 2; }
        warte(k, 500);
    }
    return 1;
}
/* NEU 1.0.6 (Codex-Prüfung 32): dieselben Schutzprüfungen wie loeschen(), ohne etwas zu verändern und ohne Meldung -
 * vor Sonys Deinstallation (Sony löscht den ganzen App-Ordner, darum muss vorher feststehen, dass loeschen() ihn auch dürfte). */
static bool schutz_vorab(kern *k, const char *p)
{
    if (selbst_gesperrt(k, p, false) || beschaedigt(k, p, false)) return false;
    for (int i = 0; i < k->SperrQuellen.n; i++) {
        const char *sq = k->SperrQuellen.e[i]; size_t l = strlen(p), lq = strlen(sq);
        if (strcmp(sq, p) == 0 || (strncmp(sq, p, l) == 0 && sq[l] == '/') || (strncmp(p, sq, lq) == 0 && p[lq] == '/')) return false;
    }
    if (!kern_erlaubt(k, p) || !kern_ohne_verknuepfung(k, p)) return false;
    if (!s_existiert(k->S, p)) return true;
    dliste *el = s_liste(k->S, h_eltern(A, p), true);
    datei *selbst = NULL;
    if (el) { const char *b = h_blatt(A, p); for (int i = 0; i < el->n; i++) if (strcmp(el->e[i].name, b) == 0) { selbst = &el->e[i]; break; } }
    if (selbst == NULL) return false;
    if (selbst->ist_link) return true;
    const char *fund = NULL;
    if (nachkomme_geschuetzt(k, p)) return false;
    if (selbst->ist_ordner && baum_hat_geschuetztes(k, p, &fund, 0)) return false;
    return true;
}
/* NEU 1.0.6 (Codex-Prüfung 32/33): Ändert sich ein Ordner noch (Sony löscht gerade)? Kennwert über den ganzen Baum
 * (Dateien, Ordner, Bytes - höchstens 5 s Zählzeit). -1 = nicht feststellbar. */
static long long ordner_kennwert(kern *k, const char *p)
{
    volatile groesse g = {0};
    volatile bool fehl = false;
    VERSUCHE { g = baum_groesse(k->S, A, p, jetzt() + 5, NULL, NULL); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); fehl = true; } ENDE_VERSUCHE;
    if (fehl || g.Gekappt) return -1;
    return ((long long)g.Dateien * 1000003LL + g.Ordner) * 1000033LL + g.Bytes;
}
/* NEU 1.0.6 (Codex-Prüfung 33): Ordner, die Sony mit dem Spiel vermutlich mitlöscht (Update, Zusatzinhalte, deren Infos) */
static sliste sony_mit(kern *k, const eintrag *f)
{
    sliste l = {0};
    if (!f->KachelOrdner || !f->KachelId) return l;
    const char *o = f->KachelOrdner, *e = VERB(A, "/app/", f->KachelId);
    size_t lo = strlen(o), le = strlen(e);
    if (lo <= le || strcmp(o + lo - le, e) != 0) return l;
    const char *b = a_kopie_n(A, o, lo - le);               /* "/user" bzw. "/mnt/extN/user" */
    sl_add(A, &l, VERB(A, b, "/patch/", f->KachelId));
    sl_add(A, &l, VERB(A, b, "/addcont/", f->KachelId));
    sl_add(A, &l, VERB(A, "/user/appmeta/addcont/", f->KachelId));
    return l;
}
/* Name für die Liste der Kacheln, die im Menü bleiben: Text ohne Ort und ohne Zusatz in eckigen Klammern */
static const char *geist_name(kern *k, const eintrag *f)
{
    const char *t = f->TextBasis ? f->TextBasis : f->Text;
    const char *v = kern_ort_vorsatz(f->Ort);
    if (v[0] && txt_beginnt(t, v)) t += strlen(v);
    const char *tk = T("Tool-Kachel: ", "Tool tile: ");
    if (txt_beginnt(t, tk)) t += strlen(tk);
    const char *lk = T("Leere Kachel (Schloss): ", "Empty tile (lock icon): ");
    if (txt_beginnt(t, lk)) t += strlen(lk);
    htreffer m = h_mt(A, t, "^([^()]*\\([A-Z]{4}\\d{5}\\))");   /* nur "Name (Kennung)" */
    return m.ok ? m.gr[1] : t;
}
/* Eintrag, dessen Dateien gelöscht sind, dessen Kachel aber im Menü bleibt: genau sagen, was weg ist und was nicht */
static void teilweise_melden(kern *k, const eintrag *f, const sliste *geloescht, const char *uebrig, const char *grund)
{
    melde(k, VERB(A, k->simulation ? T("  würde TEILWEISE entfernt   ", "  would be PARTLY removed   ") : T("  TEILWEISE entfernt   ", "  PARTLY removed   "), f->Text));
    for (int j = 0; j < geloescht->n; j++)
        melde(k, VERB(A, k->simulation ? T("      würde gelöscht: ", "      would be deleted: ") : T("      gelöscht: ", "      deleted: "), geloescht->e[j]));
    melde(k, VERB(A, T("      übrig: ", "      left: "), uebrig, " - ", grund));
}

static const char *sm_schlusstext(kern *k, const pliste *gewaehlt)
{
    bool smDatenWeg = false, kachelnWeg = false;
    for (int i = 0; i < gewaehlt->n; i++) {
        eintrag *f = gewaehlt->e[i];
        for (int j = 0; j < f->Pfade.n; j++) if (h_gleich(f->Pfade.e[j], SM_ORDNER) || h_gleich(f->Pfade.e[j], "/user/data")) smDatenWeg = true;
        if (f->Kat == 1 && f->Quelle[0]) kachelnWeg = true;
    }
    if (smDatenWeg) return T("ShadowMount wurde mitgelöscht - richte es neu ein, dann erscheinen die Spiele auf der M.2 wieder als Kacheln.", "ShadowMount was deleted too - set it up again and the games on the M.2 will show up as tiles again.");
    if (k->SM_ANGEHALTEN) return T("ShadowMount wurde angehalten - zum Weiterspielen einfach neu laden.", "ShadowMount was stopped - just load it again to keep playing.");
    if (kachelnWeg) return T("Die Spiele auf der Platte erscheinen wieder als Kacheln, sobald ShadowMount läuft.", "The games on the SSD/M.2 will show up as tiles again once ShadowMount is running.");
    return "";
}

ergebnis kern_fuehre_loeschung(kern *k, bool (*weiter)(void *), void *wc)
{
    ergebnis erg = {0}; erg.SmText = "";
    /* Merker vom letzten Löschlauf zurücksetzen */
    k->SM_ANGEHALTEN = false; k->SM_STOP_GESENDET = false; k->SM_GRUND = "";
    /* Abbruch angefordert: gar nicht erst anfangen */
    if (k->abbruch) { erg.Abgebrochen = true; return erg; }
    kern_abhaengige_mitnehmen(k);
    k->LINK_CACHE = mt_neu(false);
    pliste gewaehlt = kern_gewaehlt(k);
    erg.Frei.Gekappt = k->GROESSE_GEKAPPT;
    /* Die Liste muss von genau der "Verbindung" stammen, die gerade besteht. */
    if (k->S == NULL || !k->FUNDE_GUELTIG || k->funde_verbindung_nr != k->verbindung_nr) {
        melde(k, T("FEHLER: Die Liste stammt nicht von der gerade verbundenen PS5 - nichts gelöscht. Bitte neu verbinden.", "ERROR: The list is not from the PS5 currently connected - nothing deleted. Please connect again."));
        erg.Abgebrochen = true; erg.Fehler = 1; return erg;
    }
    /* Erst sicherstellen, dass die Verbindung sauber antwortet. */
    if (!s_gesund(k->S)) {
        melde(k, T("Verbindung zur PS5 antwortet nicht sauber - verbinde neu ...", "Connection to the PS5 is not responding cleanly - reconnecting ..."));
        bool ok = s_gesund(k->S);                       /* (FTP: VerbindungErneuern - auf der Konsole: nochmal prüfen) */
        if (!ok) {
            melde(k, T("FEHLER: Keine saubere Verbindung zur PS5 - nichts gelöscht. Bitte neu verbinden.", "ERROR: No clean connection to the PS5 - nothing deleted. Please connect again."));
            erg.Abgebrochen = true; erg.Fehler = 1; return erg;
        }
    }
    /* NEU 1.0.6: Gibt es Sonys Deinstallations-Dienst (nur PS5-App)? Ohne ihn (PC/Windows) alles wie bisher. */
    k->GEISTER = (sliste){0}; k->DB_HINWEIS = false;
    {
        volatile bool sony = false;
        VERSUCHE { sony = s_befehl(k->S, "SITE UNINSTALL?", NULL).code == 200; }
        FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); sony = false; } ENDE_VERSUCHE;
        k->SONY = sony;
    }
    /* Sobald ShadowMount-Kacheln, -Spiele, -Daten oder die M.2 dran sind: erst anhalten. */
    int smBetroffen = 0;
    for (int i = 0; i < gewaehlt.n; i++) {
        eintrag *f = gewaehlt.e[i];
        bool b = f->Quelle[0] || f->Ort != ORT_INTERN || f->Kachel;   /* 1.0.6: statt Bereich 7 der Ort (M.2, USB, USB-Stick) */
        for (int j = 0; j < f->Pfade.n && !b; j++) if (h_like(f->Pfade.e[j], SM_ORDNER "*") || h_gleich(f->Pfade.e[j], "/user/data")
                                                      || h_m(f->Pfade.e[j], "(?i)\\.(exfat|ffpfs|ffpfsc|ffpkg|pfs|img)$")) b = true;   /* Codex-Prüfung 28: Spiel-Abbilder */
        if (b) smBetroffen++;
    }
    if (smBetroffen > 0) {
        smzustand z = kern_shadowmount_anhalten(k);
        erg.SmGestoppt = z != SM_UNBEKANNT;
        if (z == SM_UNBEKANNT) {
            melde(k, "");
            melde(k, T(" ShadowMount konnte nicht sicher angehalten werden. Läuft es noch, können", " ShadowMount could not be stopped safely. If it is still running,"));
            melde(k, T(" eingebundene Spieldaten beim Löschen beschädigt werden.", " mounted game data can be damaged while deleting."));
            if (!weiter(wc)) { erg.Abgebrochen = true; return erg; }
            melde(k, T(" Der Nutzer hat 'Trotzdem löschen' gewählt.", " The user chose 'Delete anyway'."));
        }
        melde(k, "");
    }
    if (k->simulation) melde(k, T("SIMULATION: Prüfe, was gelöscht würde ... (es wird nichts gelöscht)", "SIMULATION: checking what would be deleted ... (nothing is deleted)"));   /* NEU 1.1 */
    else melde(k, T("Lösche ... (große Spiele können etwas dauern)", "Deleting ... (large games can take a while)"));
    mtab spielFehlt = mt_neu(true);
    mtab sonyGruppe = mt_neu(true);      /* NEU 1.0.6: Spiele, die Sony entfernt hat -> ihre Updates/Zusatzinhalte nimmt Sony evtl. mit */
    mtab sonyLaeuft = mt_neu(true);      /* NEU 1.0.6 (Codex-Prüfung 33): Sony nach 60 s nicht fertig -> Teile dieses Spiels nicht selbst anfassen */
    volatile bool verloren = false;
    static const int REIHE[KAT_ANZ] = { 1, 2, 3, 4, 5, 6 };   /* 1.0.6: ohne Bereich 7; Homebrew weiter zuletzt */
    int nr = 0, anzahl = 0;
    for (int i = 0; i < gewaehlt.n; i++) { int kt = ((eintrag *)gewaehlt.e[i])->Kat; for (int r = 0; r < KAT_ANZ; r++) if (kt == REIHE[r]) { anzahl++; break; } }
    for (int r = 0; r < KAT_ANZ; r++) {
        for (int i = 0; i < gewaehlt.n; i++) {
            eintrag *f = gewaehlt.e[i];
            if (f->Kat != REIHE[r]) continue;
            nr++;
            if (k->Fortschritt) k->Fortschritt(k->fortschritt_ctx, nr, anzahl, f);
            if (verloren) { erg.Fehler++; continue; }
            /* Update/DLC eines Spiels, das selbst nicht gelöscht werden konnte: bleiben */
            if (f->Teil != NULL && strcmp(f->Teil, "Spiel") != 0 && f->Gruppe != NULL && mt_hat(&spielFehlt, f->Gruppe)) {
                melde(k, VERB(A, T("  übersprungen   ", "  skipped   "), f->Text, T(" (das Spiel selbst konnte nicht gelöscht werden)", " (the game itself could not be deleted)")));
                erg.Fehler++; continue;
            }
            volatile bool ok = true;
            k->WAR_SCHON_WEG = 0;
            /* NEU 1.0.6 (da_Chris86 28.10.2026): Kachel über Sony. Bei "Spiel + Kachel" (ShadowMount) steht die Spieldatei vorne
             * in den Pfaden und wird ZUERST gelöscht, dann die Kachel bei Sony ausgetragen (sonst trägt ShadowMount sie wieder ein).
             * Bei "nur Kachel" bleibt die Spieldatei. Lehnt Sony ab: Dateien wie bisher löschen - der Eintrag gilt dann
             * nicht als ganz entfernt (die Kachel bleibt im Menü). */
            volatile bool sonyOffen = k->SONY && f->KachelId && f->KachelOrdner && !f->Gesperrt;
            volatile bool sonyWeg = false, sonyTeil = false, mitSony = false, sonyHaengt = false;
            const char *volatile sonyGrund = "";
            sliste geloescht = {0};
            /* Codex-Prüfung 32: bei unsicherem ShadowMount-Stopp nie Sony für ShadowMount-/Tool-Kacheln (auch tote/unklare) */
            if (sonyOffen && (f->Quelle[0] || f->Kachel) && smBetroffen > 0 && !erg.SmGestoppt && !k->simulation) {
                sonyOffen = false; sonyTeil = true;
                sonyGrund = T("ShadowMount war nicht sicher angehalten, darum nicht über Sony", "ShadowMount was not stopped safely, so not via Sony");
            }
            /* NEU 1.0.6 (da_Chris86 29.10.2026 21:25): Kachel ohne App-Ordner (leere Kachel/Schloss, Kachel-Bilder ohne App) zuerst
             * über Sony mit der geprüften Kennung austragen - der Dienst prüft danach selbst, ob der Menüeintrag weg ist
             * (plattform_ps5.c). Lehnt Sony ab oder antwortet nicht: "Entfernung unvollständig", die Dateien werden wie
             * bisher gelöscht, der Eintrag gilt NICHT als ganz entfernt. */
            if (k->SONY && f->KachelId && !f->KachelOrdner && !f->Gesperrt) {
                const char *g = ""; volatile int r = 0;
                VERSUCHE { r = sony_entfernen(k, VERB(A, "/user/app/", f->KachelId), f->KachelId, &g); }
                FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); r = 0; g = ausn_text; } ENDE_VERSUCHE;
                if (r == 1) { sonyWeg = true; mitSony = true; }
                else { sonyTeil = true; sonyGrund = g; }
            }
            if (sonyOffen && !s_existiert(k->S, f->KachelOrdner)) {   /* App-Ordner schon weg (Codex-Prüfung 33): Kachel bleibt evtl. im Menü */
                sonyOffen = false; sonyTeil = true;
                sonyGrund = T("der App-Ordner war schon weg - Sony konnte die Kachel nicht austragen", "the app folder was already gone - Sony could not remove the tile");
            }
            /* Codex-Prüfung 33: Sony nimmt evtl. Update/Zusatzinhalte mit - auch die müssen löschbar sein, und kein Teil des Spiels gesperrt */
            if (sonyOffen && f->Gruppe != NULL)
                for (int g = 0; g < k->Funde.n && sonyOffen; g++) { eintrag *t = k->Funde.e[g]; if (t != f && t->Gruppe != NULL && h_gleich(t->Gruppe, f->Gruppe) && (t->Gesperrt || !t->Wahl)) sonyOffen = false; }
            if (sonyOffen) {
                sliste mit = sony_mit(k, f);
                for (int j = 0; j < mit.n && sonyOffen; j++) {
                    volatile bool frei = false;
                    VERSUCHE { frei = !s_existiert(k->S, mit.e[j]) || schutz_vorab(k, mit.e[j]); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); frei = false; } ENDE_VERSUCHE;
                    if (!frei) sonyOffen = false;
                }
            }
            /* Codex-Prüfung 34: wegen Update/Zusatzinhalt nicht über Sony - dann bleibt die Kachel im Menü (TEILWEISE + Hinweis) */
            if (!sonyOffen && !sonyTeil && k->SONY && f->KachelId && f->KachelOrdner && !f->Gesperrt && s_existiert(k->S, f->KachelOrdner)) {
                bool eigen_frei = true;
                for (int j = 0; j < f->Pfade.n && eigen_frei; j++) if (!(f->Quelle[0] && strcmp(f->Pfade.e[j], f->Quelle) == 0)) {
                    volatile bool fr = false;
                    VERSUCHE { fr = schutz_vorab(k, f->Pfade.e[j]); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); fr = false; } ENDE_VERSUCHE;
                    if (!fr) eigen_frei = false;
                }
                if (eigen_frei) {   /* (sonst meldet das eigene Löschen den Sicherheitsstopp als Fehler) */
                    sonyTeil = true;
                    sonyGrund = T("ein Update oder Zusatzinhalt ist gesperrt oder geschützt - darum nicht über Sony", "an update or add-on is locked or protected - so not via Sony");
                }
            }
            /* Codex-Prüfung 32: Sony löscht den ganzen App-Ordner - vorher dieselben Schutzprüfungen wie beim eigenen Löschen.
             * Schlägt eine fehl: nicht über Sony; das eigene Löschen meldet dann den Sicherheitsstopp wie bisher. */
            for (int j = 0; j < f->Pfade.n && sonyOffen; j++) {
                const char *p = f->Pfade.e[j];
                if (f->Quelle[0] && strcmp(p, f->Quelle) == 0) continue;
                volatile bool frei = false;
                VERSUCHE { frei = schutz_vorab(k, p); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); frei = false; } ENDE_VERSUCHE;
                if (!frei) sonyOffen = false;
            }
            if (f->Gruppe != NULL && !ist_teil(f, "Spiel") && mt_hat(&sonyLaeuft, f->Gruppe)) {
                sliste nichts = {0};
                teilweise_melden(k, f, &nichts, sl_join(A, &f->Pfade, ", "), T("Sony löscht das Spiel noch im Hintergrund - später nochmal nachschauen", "Sony is still deleting the game in the background - scan again later"));
                erg.Teilweise++;
                continue;
            }
            /* Update/Zusatzinhalt eines Spiels, das Sony gerade entfernt: warten, bis die Ordner weg sind oder sich nicht mehr
             * ändern (Codex-Prüfung 32: sonst Wettlauf mit Sonys Hintergrund-Löschen), höchstens 30 s - dann den Rest selbst */
            if (f->Gruppe != NULL && !ist_teil(f, "Spiel") && mt_hat(&sonyGruppe, f->Gruppe)) {
                mitSony = true;
                double bis = jetzt() + 30;
                long long vorher = -2; int gleich = 0;
                bool ruhig = false;
                for (;;) {
                    bool alle = true, unklar = false; long long w = 0;
                    for (int j = 0; j < f->Pfade.n; j++) if (s_existiert(k->S, f->Pfade.e[j])) { alle = false; long long kw = ordner_kennwert(k, f->Pfade.e[j]); if (kw < 0) unklar = true; w = w * 131 + kw; }
                    if (alle) { ruhig = true; break; }
                    if (!unklar && w == vorher) { if (++gleich >= 2) { ruhig = true; break; } } else gleich = 0;   /* Codex-Prüfung 34: nicht feststellbar zählt nie als Stillstand */
                    vorher = unklar ? -2 : w;
                    if (jetzt() > bis) break;
                    warte(k, 1000);
                }
                if (!ruhig) {   /* Sony arbeitet offenbar noch - nicht selbst anfassen (kein Wettlauf) */
                    sliste nichts = {0};
                    teilweise_melden(k, f, &nichts, sl_join(A, &f->Pfade, ", "), T("Sony löscht das Spiel noch im Hintergrund - später nochmal nachschauen", "Sony is still deleting the game in the background - scan again later"));
                    erg.Teilweise++;
                    continue;
                }
            }
            for (int j = 0; j < f->Pfade.n; j++) {
                const char *p = f->Pfade.e[j];
                if (sonyOffen && !(f->Quelle[0] && strcmp(p, f->Quelle) == 0)) {
                    sonyOffen = false;
                    const char *g = "";
                    volatile int r = 0;
                    VERSUCHE { r = sony_entfernen(k, f->KachelOrdner, f->KachelId, &g); }
                    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); r = 0; g = ausn_text; } ENDE_VERSUCHE;
                    if (r == 1) sonyWeg = true;
                    else if (r == 2) { sonyHaengt = true; sonyGrund = g; break; }   /* nicht selbst weiterlöschen */
                    else { sonyTeil = true; sonyGrund = g; }
                }
                VERSUCHE {
                    if (!(f->NurLeer ? leeren_ordner_loeschen(k, p) : loeschen(k, p))) ok = false;
                } FANGE {
                    if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN();
                    ok = false;
                    melde(k, VERB(A, "    (", p, " : ", ausn_text, ")"));
                    /* Steht die Verbindung noch? Kurz nachfragen. */
                    VERSUCHE { if (s_befehl(k->S, "NOOP", NULL).code != 200) verloren = true; } FANGE { verloren = true; } ENDE_VERSUCHE;
                } ENDE_VERSUCHE;
                /* Der Rest des Eintrags nur, wenn das Vorherige wirklich weg ist */
                if (!ok) break;
                sl_add(A, &geloescht, p);
            }
            if (ok && sonyHaengt) {                     /* NEU 1.0.6: Sony arbeitet noch - der Rest bleibt vorerst */
                if (f->Gruppe != NULL) mt_setze(A, &sonyLaeuft, f->Gruppe, (void *)1);
                sliste rest = {0};
                for (int j = 0; j < f->Pfade.n; j++) if (!sl_enthaelt(&geloescht, f->Pfade.e[j])) sl_add(A, &rest, f->Pfade.e[j]);
                teilweise_melden(k, f, &geloescht, sl_join(A, &rest, ", "), sonyGrund);
                erg.Teilweise++;
                continue;
            }
            if (ok && sonyTeil) {                       /* NEU 1.0.6: Dateien weg, Kachel bleibt */
                teilweise_melden(k, f, &geloescht, T("der Eintrag im PS5-Menü (Kachel)", "the entry in the PS5 menu (tile)"), sonyGrund);
                erg.Teilweise++;
                sl_add(A, &k->GEISTER, geist_name(k, f));
                if (f->HatGroesse) { erg.Frei.Bytes += f->Bytes; erg.Frei.Dateien += f->Dateien; erg.Frei.Ordner += f->Ordner; }
                continue;
            }
            if (ok && (sonyWeg || (mitSony && k->WAR_SCHON_WEG >= f->Pfade.n))) {   /* NEU 1.0.6: über Sony entfernt */
                if (sonyWeg && f->Gruppe != NULL) mt_setze(A, &sonyGruppe, f->Gruppe, (void *)1);
                melde(k, VERB(A, k->simulation ? T("  würde gelöscht   ", "  would be deleted   ") : T("  gelöscht   ", "  deleted   "), f->Text,
                              sonyWeg ? T("   (Kachel über Sony entfernt)", "   (tile removed via Sony)") : T("   (mit dem Spiel über Sony)", "   (with the game via Sony)")));
                if (f->HatGroesse) { erg.Frei.Bytes += f->Bytes; erg.Frei.Dateien += f->Dateien; erg.Frei.Ordner += f->Ordner; }
                continue;
            }
            if (ok && k->WAR_SCHON_WEG >= f->Pfade.n) { melde(k, VERB(A, T("  war schon weg   ", "  already gone   "), f->Text)); continue; }
            if (ok) {
                melde(k, VERB(A, k->simulation ? T("  würde gelöscht   ", "  would be deleted   ") : T("  gelöscht   ", "  deleted   "), f->Text));
                if (f->HatGroesse) { erg.Frei.Bytes += f->Bytes; erg.Frei.Dateien += f->Dateien; erg.Frei.Ordner += f->Ordner; }
                /* NEU 1.0.6 (PS5-Test da_Chris86 29.10.2026 19:54): die Mediengalerie führt eine eigene Liste - gelöschte Bilder und
                 * Videos stehen dort als leere Einträge, bis die Datenbank neu aufgebaut ist. Darum am Ende der Hinweis (mit Sony) */
                if (k->SONY) {   /* genauso die Spielstand-Liste in den Einstellungen (wie bis 1.0.5: dort räumte der Neuaufbau beides auf) */
                    const char *gal = T("Mediengalerie (gelöschte Bilder und Videos)", "Media Gallery (deleted screenshots and videos)");
                    const char *sav = T("Spielstand-Liste (gelöschte Spielstände)", "Saved data list (deleted saves)");
                    bool av = false, sd = false;
                    for (int j = 0; j < f->Pfade.n; j++) {
                        if (h_m(f->Pfade.e[j], "^/user/av_contents/(photo|video)/")) av = true;
                        if (h_m(f->Pfade.e[j], "^/user/home/[^/]+/savedata(_prospero)?/")) sd = true;
                    }
                    if (av && !sl_enthaelt(&k->GEISTER, gal)) sl_add(A, &k->GEISTER, gal);
                    if (sd && !sl_enthaelt(&k->GEISTER, sav)) sl_add(A, &k->GEISTER, sav);
                }
            } else {
                melde(k, VERB(A, T("  FEHLER      ", "  ERROR      "), f->Text, T(" (konnte nicht ganz gelöscht werden)", " (could not be deleted completely)"))); erg.Fehler++;
                if (ist_teil(f, "Spiel")) mt_setze(A, &spielFehlt, f->Gruppe, (void *)1);
            }
        }
    }
    if (verloren)
        melde(k, T("FEHLER: Die Verbindung zur PS5 ist abgerissen - der Rest wurde nicht gelöscht. Bitte neu verbinden.", "ERROR: The connection to the PS5 dropped - the rest was not deleted. Please connect again."));
    else {
        /* Ordner, die nur wegen ShadowMount-Spielen einzeln gelistet waren und
         * jetzt leer sind, auch entfernen (innerste zuerst). */
        for (int i = k->AUFGEKLAPPT.n - 1; i >= 0; i--) {
            const char *o = k->AUFGEKLAPPT.e[i];
            const char *oz = VERB(A, o, "/");
            bool betroffen = false;
            for (int g = 0; g < gewaehlt.n && !betroffen; g++) { eintrag *f = gewaehlt.e[g]; for (int j = 0; j < f->Pfade.n; j++) if (txt_beginnt(f->Pfade.e[j], oz)) { betroffen = true; break; } }
            if (betroffen) leeren_ordner_entfernen(k, o);
        }
        /* Ordner von Spielen, deren Teile gelöscht wurden und die jetzt leer sind */
        for (int g = 0; g < k->LEER_DANACH.n; g++) {
            const char *gr = k->LEER_DANACH.e[g].k;
            bool dabei = false;
            for (int i = 0; i < gewaehlt.n; i++) { eintrag *f = gewaehlt.e[i]; if (f->Gruppe != NULL && h_gleich(f->Gruppe, gr)) { dabei = true; break; } }
            if (!dabei) continue;
            sliste *os = k->LEER_DANACH.e[g].v;
            for (int i = 0; i < os->n; i++) leeren_ordner_entfernen(k, os->e[i]);
        }
    }
    erg.SmText = sm_schlusstext(k, &gewaehlt);
    erg.Verloren = verloren;
    return erg;
}

/* NEU 28.10.2026 (da_Chris86): PS5 Cleanup entfernt sich ganz zum Schluss selbst (Programmdateien, eigene Daten, eigene Kachel).
 * Das Programm läuft dann schon im Arbeitsspeicher. Alle übrigen Schutzregeln gelten unverändert (loeschen()). */
/* NEU 1.0.6 (Codex-Prüfung 28): eigene Dateien auf "behaltener" M.2/USB-Speicher und auf USB-Sticks bleiben -
 * intern immer, M.2/USB nur, wenn sie mit geleert wurden. */
static bool selbst_weg_erlaubt(const char *p, bool m2, bool usb)
{
    int o = kern_pfad_ort(p);
    return o == ORT_INTERN || (o == ORT_M2 && m2) || (o == ORT_USB && usb);
}
bool kern_selbst_da(kern *k, bool m2, bool usb)
{
    if (!k->SCHUTZ.aktiv) return false;
    for (int i = 0; i < k->SCHUTZ.eigene.n; i++) if (selbst_weg_erlaubt(k->SCHUTZ.eigene.e[i], m2, usb) && s_existiert(k->S, k->SCHUTZ.eigene.e[i])) return true;
    return false;
}
/* NEU 1.0.6 (da_Chris86 29.10.2026 22:58): jeder Schritt der Selbstentfernung mit Grund ins Diagnoseprotokoll (PS5: Kernel-
 * Protokoll) - auch übersprungene. Das liegt außerhalb der zu löschenden Daten. */
static void sdiag(kern *k, const char *t) { if (zugriff_diagnose) zugriff_diagnose(VERB(A, "Selbstentfernung: ", t)); }
/* eigene Kachel über Sony austragen (wie bisher); r==2: Sony löscht noch - die Kachel-Ordner nicht selbst anfassen */
/* Rückgabe wie sony_entfernen: 1 = über Sony entfernt, 0 = nicht (Kachel bleibt im Menü, GEISTER), 2 = Sony löscht noch */
static int kachel_ueber_sony(kern *k, const char *p, const char *id, sliste *nicht_anfassen, int *fehler)
{
    const char *volatile g = "";
    volatile int r = 0;
    VERSUCHE { const char *gg = ""; r = sony_entfernen(k, p, id, &gg); g = gg; }
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); r = 0; g = ausn_text; } ENDE_VERSUCHE;
    sdiag(k, a_fmt(A, "Kachel %s über Sony -> %d %s", id, (int)r, g ? g : ""));
    if (r == 2) {                                 /* Sony arbeitet noch - diese Dateien nicht selbst anfassen */
        melde(k, VERB(A, "  PS5 Cleanup: ", g));
        (*fehler)++;                              /* Codex-Prüfung 33: dann nicht "ganz entfernt" melden */
        sl_add(A, nicht_anfassen, p); sl_add(A, nicht_anfassen, VERB(A, "/user/appmeta/", id)); sl_add(A, nicht_anfassen, VERB(A, SYS_META "/", id));
        return 2;
    }
    if (r) melde(k, VERB(A, k->simulation ? T("  würde gelöscht   ", "  would be deleted   ") : T("  gelöscht   ", "  deleted   "), T("PS5 Cleanup: Kachel ", "PS5 Cleanup: tile "), id, T("   (über Sony)", "   (via Sony)")));
    else {
        melde(k, VERB(A, T("  PS5 Cleanup: Sony hat die eigene Kachel nicht entfernt - ", "  PS5 Cleanup: Sony did not remove its own tile - "), g, T(". Die Dateien werden gelöscht, die Kachel bleibt im Menü.", ". The files are deleted, the tile stays in the menu.")));
        sl_add(A, &k->GEISTER, "PS5 Cleanup");
    }
    return r;
}
/* eine eigene Datei/einen eigenen Ordner löschen (wie bisher) - mit Grund ins Diagnoseprotokoll */
static void eigenes_loeschen(kern *k, const char *p, int *fehler)
{
    const char *volatile jbDrin = NULL;
    VERSUCHE { jbDrin = baum_jb(k, p, 0); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); jbDrin = NULL; } ENDE_VERSUCHE;
    if (jbDrin) {
        (*fehler)++;
        melde(k, VERB(A, T("  BLEIBT      PS5 Cleanup: ", "  KEPT        PS5 Cleanup: "), p, T(" - darin liegt ein Jailbreak-Einstieg (", " - it contains a jailbreak entry ("), jbDrin, ")"));
        return;
    }
    /* 28.10.2026 (PS5-Test Simulation): in der Simulation "würde gelöscht" wie bei allen anderen Einträgen */
    if (loeschen(k, p)) melde(k, VERB(A, k->simulation ? T("  würde gelöscht   ", "  would be deleted   ") : T("  gelöscht   ", "  deleted   "), T("PS5 Cleanup: ", "PS5 Cleanup: "), p));
    else { (*fehler)++; melde(k, VERB(A, T("  FEHLER      ", "  ERROR      "), T("PS5 Cleanup: ", "PS5 Cleanup: "), p)); }
}
/* bisheriger Weg (PC-Simulation, kein Hintergrundteil erreichbar): die Oberfläche entfernt den Startordner selbst */
static void startordner_selbst(kern *k, int *fehler)
{
    if (!zugriff_startordner_entfernen) { sdiag(k, "Startordner: übersprungen - auf diesem Gerät nicht eingebaut"); return; }
    if (!k->SONY) { sdiag(k, "Startordner: übersprungen - ohne Sonys Dienst (kein PS5-Lauf)"); return; }
    /* da_Chris86 30.10.2026 00:26: zum Startordner keinen Hinweis auf dem Bildschirm - nur im Diagnoseprotokoll */
    if (*fehler) { sdiag(k, a_fmt(A, "Startordner: bleibt - vorher %d Fehler/BLEIBT (siehe Zeilen davor)", *fehler)); return; }
    const char *t = NULL;
    int r = zugriff_startordner_entfernen(A, k->simulation, &t);
    sdiag(k, a_fmt(A, "Startordner durch die Oberfläche -> %d %s", r, t ? t : ""));
    /* r: 1 entfernt, 0 nicht da, -1 Fehler/fremd, -2 /system_ex in dieser Sitzung nicht mehr beschreibbar (PS5: nur einmal pro
     * Einschalten, Tests 30.10.2026) - in allen Fällen ohne Bildschirmhinweis; ein Fehler am eigenen Startordner zählt nicht */
    sdiag(k, r == 1 ? (k->simulation ? "Startordner: würde gelöscht" : "Startordner: gelöscht") : r == 0 ? "Startordner: war nicht da"
                    : r == -2 ? "Startordner: bleibt - /system_ex ist in dieser Sitzung nicht mehr beschreibbar" : "Startordner: bleibt - Fehler (siehe Zeile davor)");
}
int kern_selbst_entfernen(kern *k, bool m2, bool usb)
{
    k->SELBST_UEBERGEBEN = false;
    if (!k->SCHUTZ.aktiv) { sdiag(k, "übersprungen - Selbstschutz ist aus"); return 0; }
    int fehler = 0;
    k->LINK_CACHE = mt_neu(false);                 /* frisch: nach dem Löschlauf hat sich viel geändert */
    k->SCHUTZ.frei = true;
    k->SELBST_DIAG = true;                         /* ab hier jede Meldung auch ins Diagnoseprotokoll */
    /* NEU 1.0.6 (da_Chris86 29.10.2026 23:08): Kachel und Startordner entfernt der Hintergrundteil von ps5cleanup.elf, nachdem
     * sich die Oberfläche beendet hat (die Oberfläche läuft selbst aus dem Startordner). Ohne ihn: wie bisher. */
    bool uebergabe = zugriff_selbst_uebergeben && k->SONY && !k->simulation;
    sdiag(k, a_fmt(A, "Beginn - Sonys Dienst %s, Übergabe an den Hintergrundteil %s, M.2 mitgeleert %s, USB mitgeleert %s, %d eigene Pfade",
                   k->SONY ? "ja" : "nein", uebergabe ? "ja" : (!zugriff_selbst_uebergeben ? "nicht eingebaut" : (k->simulation ? "nein (Simulation)" : "nein (ohne Sonys Dienst)")),
                   m2 ? "ja" : "nein", usb ? "ja" : "nein", k->SCHUTZ.eigene.n));
    /* 1. eigene Kachel: dieselben Prüfungen wie bisher (Codex-Prüfung 32/33/37). da_Chris86 28.10.2026: über Sony, damit
     *    keine Geisterkachel bleibt. Mit Hintergrundteil: hier nur prüfen, Sony beauftragt er nach dem Ende der Oberfläche. */
    sliste nicht_anfassen = {0};
    const char *kachel_p = NULL, *kachel_id = NULL;
    bool kachel_geist = false;                     /* eigene Kachel bleibt im Menü (nicht über Sony möglich) - Review 30.10.2026 */
    for (int i = 0; i < k->SCHUTZ.eigene.n && k->SONY; i++) {
        const char *p = k->SCHUTZ.eigene.e[i];
        htreffer m = h_mt(A, p, "^/user/app/([A-Z]{4}\\d{5})$");
        if (!m.ok) continue;
        if (!selbst_weg_erlaubt(p, m2, usb)) { sdiag(k, VERB(A, "Kachel ", p, " - übersprungen: Ort wird nicht mitgeleert")); continue; }
        if (!s_existiert(k->S, p)) { sdiag(k, VERB(A, "Kachel ", p, " - nicht vorhanden")); continue; }
        volatile bool frei = false;
        VERSUCHE { frei = schutz_vorab(k, p); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); frei = false; } ENDE_VERSUCHE;   /* Codex-Prüfung 33 */
        const char *volatile jbS = NULL;   /* Codex-Prüfung 37: liegt darin etwas von einem Jailbreak-Einstieg - nie über Sony */
        VERSUCHE { jbS = baum_jb(k, p, 0); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); jbS = NULL; } ENDE_VERSUCHE;
        if (jbS) { sl_add(A, &nicht_anfassen, p); fehler++;
            melde(k, VERB(A, T("  BLEIBT      PS5 Cleanup: ", "  KEPT        PS5 Cleanup: "), p, T(" - darin liegt ein Jailbreak-Einstieg (", " - it contains a jailbreak entry ("), jbS, ")"));
            continue; }
        const char *g = "";
        if (s_existiert(k->S, VERB(A, M2 "/user/app/", m.gr[1])) || s_existiert(k->S, VERB(A, USB_ERW "/user/app/", m.gr[1])))
            g = T("es gibt noch eine Kopie auf M.2/USB", "there is another copy on M.2/USB");   /* Codex-Prüfung 32 */
        else if (!frei)
            g = T("die Schutzprüfung hat etwas gefunden - nicht über Sony", "the protection check found something - not via Sony");
        if (g[0]) {
            melde(k, VERB(A, T("  PS5 Cleanup: Sony hat die eigene Kachel nicht entfernt - ", "  PS5 Cleanup: Sony did not remove its own tile - "), g, T(". Die Dateien werden gelöscht, die Kachel bleibt im Menü.", ". The files are deleted, the tile stays in the menu.")));
            sl_add(A, &k->GEISTER, "PS5 Cleanup");
            kachel_geist = true;
            continue;
        }
        kachel_p = p; kachel_id = m.gr[1];
        if (uebergabe) {
            sl_add(A, &nicht_anfassen, p); sl_add(A, &nicht_anfassen, VERB(A, "/user/appmeta/", kachel_id)); sl_add(A, &nicht_anfassen, VERB(A, SYS_META "/", kachel_id));
            sdiag(k, VERB(A, "Kachel ", kachel_id, " - Prüfung bestanden, Sonys Deinstallation übernimmt der Hintergrundteil"));
        } else kachel_ueber_sony(k, p, kachel_id, &nicht_anfassen, &fehler);
    }
    /* 2. übrige eigene Dateien (Programmkopien, eigene Daten) - wie bisher */
    for (int i = 0; i < k->SCHUTZ.eigene.n; i++) {
        const char *p = k->SCHUTZ.eigene.e[i];
        if (!selbst_weg_erlaubt(p, m2, usb)) { sdiag(k, VERB(A, p, " - übersprungen: liegt auf ", kern_ort_name(kern_pfad_ort(p)), ", der nicht mitgeleert wird")); continue; }
        if (sl_enthaelt(&nicht_anfassen, p)) { sdiag(k, VERB(A, p, uebergabe && kachel_p ? " - übersprungen: übernimmt der Hintergrundteil (Sony)" : " - übersprungen: Sony arbeitet noch bzw. Jailbreak-Einstieg darin")); continue; }
        if (!s_existiert(k->S, p)) { sdiag(k, VERB(A, p, " - nicht (mehr) vorhanden")); continue; }
        /* Codex-Prüfung 36 (da_Chris86 29.10.2026: Jailbreak-Einstiege nur auf ausdrücklichen Wunsch): liegt in diesem Ordner noch
         * etwas von einem Jailbreak-Einstieg, bleibt der Ordner (eigenes_loeschen) */
        eigenes_loeschen(k, p, &fehler);
    }
    /* 3. Kachel + eigener Startordner /system_ex/app/PSCL00002 (Freigabe da_Chris86 29.10.2026 21:23) - nur, wenn PS5 Cleanup
     *    sonst ganz entfernt ist (sonst bleibt es startbar, um es nochmal zu versuchen). */
    if (uebergabe) {
        if (fehler) {
            if (kachel_p) melde(k, T("  BLEIBT      PS5 Cleanup: Kachel - es sind Fehler aufgetreten (siehe oben), PS5 Cleanup bleibt startbar",
                                     "  KEPT        PS5 Cleanup: tile - errors occurred (see above), PS5 Cleanup stays startable"));
            sdiag(k, a_fmt(A, "Übergabe nicht ausgelöst - vorher %d Fehler/BLEIBT (siehe Zeilen davor)", fehler));
        } else if (kachel_geist) {
            /* Review 30.10.2026: bleibt die eigene Kachel im Menü, wird nichts übergeben - sonst käme "wurde entfernt" */
            sdiag(k, "Übergabe nicht ausgelöst - die eigene Kachel bleibt im Menü (siehe oben); Startordner bleibt");
        } else {
            const char *t = NULL;
            int r = zugriff_selbst_uebergeben(A, kachel_p != NULL, true, &t);
            sdiag(k, a_fmt(A, "Übergabe an den Hintergrundteil (Kachel %s, Startordner ja) -> %d %s", kachel_p ? "ja" : "nein", r, t ? t : ""));
            if (r == 1) {
                k->SELBST_UEBERGEBEN = true;
                melde(k, kachel_p ? T("  PS5 Cleanup: Die Kachel wird entfernt, sobald PS5 Cleanup beendet ist - danach kommt eine Benachrichtigung.",
                                      "  PS5 Cleanup: the tile is removed as soon as PS5 Cleanup has quit - a notification follows.")
                                  : T("  PS5 Cleanup: Der Abschluss folgt, sobald PS5 Cleanup beendet ist - danach kommt eine Benachrichtigung.",
                                      "  PS5 Cleanup: the last step follows as soon as PS5 Cleanup has quit - a notification follows."));
            } else if (r == 0) {
                /* nur, wenn der Hintergrundteil gar nicht erreichbar ist (keine Verbindung): die Oberfläche macht es selbst */
                melde(k, VERB(A, T("  PS5 Cleanup: Hintergrundteil nicht erreichbar (", "  PS5 Cleanup: background part not reachable ("), t && t[0] ? t : "?",
                              T(") - die Kachel wird jetzt direkt entfernt.", ") - the tile is removed directly now.")));
                if (kachel_p) {
                    sliste reste = {0};
                    int kr = kachel_ueber_sony(k, kachel_p, kachel_id, &reste, &fehler);
                    if (kr == 0) eigenes_loeschen(k, kachel_p, &fehler);   /* wie bisher: Dateien weg, Kachel bleibt im Menü (Review 30.10.2026) */
                    const char *rp[2] = { VERB(A, "/user/appmeta/", kachel_id), VERB(A, SYS_META "/", kachel_id) };
                    for (int j = 0; j < 2; j++) {
                        if (sl_enthaelt(&reste, rp[j])) { sdiag(k, VERB(A, rp[j], " - übersprungen: Sony arbeitet noch")); continue; }
                        if (!s_existiert(k->S, rp[j])) { sdiag(k, VERB(A, rp[j], " - nicht (mehr) vorhanden")); continue; }
                        eigenes_loeschen(k, rp[j], &fehler);
                    }
                }
                startordner_selbst(k, &fehler);
            } else {
                /* Review 30.10.2026: erreichbar, aber nicht bestätigt (z. B. Zeitüberschreitung, Simulations-Hintergrundteil) - dann
                 * NICHT selbst weitermachen (sonst liefe die Entfernung evtl. doppelt); PS5 Cleanup bleibt startbar */
                fehler++;
                melde(k, VERB(A, T("  FEHLER      PS5 Cleanup: Der Hintergrundteil hat den Auftrag nicht bestätigt (", "  ERROR      PS5 Cleanup: the background part did not confirm the job ("),
                              t && t[0] ? t : "?", T(") - die Kachel bleibt, PS5 Cleanup bleibt startbar.", ") - the tile stays, PS5 Cleanup stays startable.")));
            }
        }
    } else startordner_selbst(k, &fehler);
    sdiag(k, a_fmt(A, "Ende - %d Fehler/BLEIBT%s", fehler, k->SELBST_UEBERGEBEN ? ", Kachel und Startordner an den Hintergrundteil übergeben" : ""));
    k->SELBST_DIAG = false;
    k->SCHUTZ.frei = false;
    return fehler;
}
