/* PS5 Cleanup 1.1 - gemeinsamer C-Kern
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
/* NEU 1.0.7 (03.11.2026): freiwillige QR-Rückmeldung - aus einer Protokollzeile nur den Fehlercode (0x8XXXXXXX bzw. den errno-Namen
 * zur Systemmeldung) und den Schritt als Kürzel merken. Der Text selbst wird nie übernommen (er kann Namen und Pfade enthalten). */
static void rm_zeile_mit(kern *k, const char *t, const char *hinweis);
void kern_rm_zeile(kern *k, const char *t) { rm_zeile_mit(k, t, NULL); }
static void rm_zeile_mit(kern *k, const char *t, const char *hinweis)
{
    if (!t || k->RM_CODE[0]) return;             /* je Eintrag zählt der erste Fehlercode */
    /* Codex-Prüfung 69: nur die Antwort der Zugriffsschicht bzw. Sonys ansehen - in Protokollzeilen stehen Namen und Pfade, die selbst
     * wie ein Code aussehen können (z. B. /user/data/0x81234567.pkg) */
    /* nur Antworttexte: sie beginnen mit dem Code ("550 ..."); Protokollzeilen mit Namen werden nie gelesen (Gegenprüfung 03.11.2026) */
    if (!((t[0] == '4' || t[0] == '5') && t[1] >= '0' && t[1] <= '9' && t[2] >= '0' && t[2] <= '9' && t[3] == ' ')) return;
    const char *ant = t + 4;
    char code[20] = "";
    for (const char *h = strstr(ant, "0x"); h && !code[0]; h = strstr(h + 2, "0x")) {
        int n = 0; while (n < 9 && ((h[2 + n] >= '0' && h[2 + n] <= '9') || (h[2 + n] >= 'a' && h[2 + n] <= 'f') || (h[2 + n] >= 'A' && h[2 + n] <= 'F'))) n++;
        if (n == 8 && h[2] == '8') {             /* Fehlercodes von Sony und vom System beginnen mit 0x8 */
            code[0] = '0'; code[1] = 'x';
            for (int i = 0; i < 8; i++) { char c = h[2 + i]; code[2 + i] = (c >= 'A' && c <= 'F') ? (char)(c - 'A' + 'a') : c; }
            code[10] = 0;
        }
    }
    static const struct { const char *text, *name; } SYS[] = {
        { "ermission denied", "EACCES" }, { "peration not permitted", "EPERM" }, { "o such file or directory", "ENOENT" },
        { "busy", "EBUSY" }, { "ead-only file system", "EROFS" }, { "o space left on device", "ENOSPC" },
        { "nput/output error", "EIO" }, { "irectory not empty", "ENOTEMPTY" }, { "evels of symbolic links", "ELOOP" },
        { "ross-device link", "EXDEV" }, { "timed out", "ETIMEDOUT" }, { "Not a directory", "ENOTDIR" }, { "Is a directory", "EISDIR" },
        { "ile name too long", "ENAMETOOLONG" }, { "nvalid argument", "EINVAL" }, { "llocate memory", "ENOMEM" },
        { "lready in progress", "EALREADY" },
    };
    for (size_t i = 0; !code[0] && i < sizeof SYS / sizeof SYS[0]; i++) if (strstr(ant, SYS[i].text)) snprintf(code, sizeof code, "%s", SYS[i].name);
    if (!code[0]) return;                        /* Zeile ohne Fehlercode - nichts merken */
    const char *s = strstr(t, "Zusatzinhalt") || strstr(t, "add-on") ? "appinfo"
                  : hinweis ? hinweis
                  : strstr(t, "Spielstand-Liste") || strstr(t, "saved data list") ? "ss"
                  : strstr(t, "Mediengalerie") || strstr(t, "Löschdienst") || strstr(t, "Media Gallery") || strstr(t, "delete service") ? "med"
                  : strstr(t, "Zusatzinhalt") || strstr(t, "add-on") ? "appinfo"
                  : strstr(t, "Einbindung") || strstr(t, "ShadowMount") || strstr(t, "mount") ? "sm"
                  : strstr(t, "Sony") ? "sony"
                  : strstr(t, "Selbstentfernung") ? "selbst" : "datei";
    snprintf(k->RM_SCHRITT, sizeof k->RM_SCHRITT, "%s", s);
    snprintf(k->RM_CODE, sizeof k->RM_CODE, "%s", code);
}
/* NEU 1.0.7 (Codex-Prüfung 69): Fehler außerhalb eines Eintrags (Selbstentfernung, alte Protokolle) - Schritt und Art fest */
void kern_rm_fehler_dazu(kern *k, const char *schritt, const char *art)
{
    if (k->RM_FEHLER_N < 0 || k->RM_FEHLER_N >= 5) return;
    snprintf(k->RM_FEHLER[k->RM_FEHLER_N], sizeof k->RM_FEHLER[0], "%s.%s.i.%s", schritt, art, k->RM_CODE[0] ? k->RM_CODE : "-");
    k->RM_FEHLER_N++;
}
/* NEU 1.0.7: einen Fehler für die QR-Rückmeldung festhalten (höchstens 5) - Art und Ort nur als Kürzel */
static void rm_fehler(kern *k, const eintrag *f)
{
    k->FEHLER_ZAHL++;   /* NEU 1.0.8 (Codex-Prüfung 76): ohne Grenze, für die Anzeige */
    if (k->RM_FEHLER_N < 0 || k->RM_FEHLER_N >= 5) return;
    const char *id = f->KachelId ? f->KachelId : f->Gruppe ? f->Gruppe : "";
    bool ps4 = !strncmp(id, "CUSA", 4) || (!id[0] && f->Text && strstr(f->Text, "(CUSA"));
    const char *art;
    switch (f->Art) {
    case ART_SPIEL: case ART_UPDATE: case ART_DLC: case ART_ABBILD: case ART_SM_KACHEL: case ART_SM_DATEI: case ART_SM_TOT: case ART_SPIEL_SICHERUNG:
        art = ps4 ? "ps4" : "ps5"; break;
    case ART_APP: case ART_APP_DATEN: art = "app"; break;
    case ART_TOOL_KACHEL: case ART_TOOL_DATEN: case ART_TOOL_DATEN_WICHTIG: art = "tool"; break;
    case ART_SPIELSTAND: art = "ss"; break;
    case ART_TR_FORTSCHRITT: case ART_TR_ZAEHLER: case ART_TR_LISTE: case ART_TR_LISTE_REST: case ART_TR_DATEN: art = "tr"; break;
    case ART_AUFNAHMEN: case ART_AUFNAHME_UNFERTIG: case ART_GALERIE_REST: case ART_GALERIE_SONST: art = "med"; break;
    case ART_JAILBREAK: art = "jb"; break;
    case ART_REST: case ART_FAKE_LIZENZ: case ART_UNBEKANNT: case ART_DATEI: case ART_PAKET_DATEI: case ART_ALTE_SICHERUNG: art = "rest"; break;
    default: art = f->Kat == 2 ? "ss" : f->Kat == 3 ? "tr" : f->Kat == 4 ? "med" : f->Kat == 7 ? "jb" : f->Kat == 6 ? "app" : "sonst"; break;
    }
    const char *ort = f->Ort == ORT_M2 ? "m2" : f->Ort == ORT_USB ? "ext" : f->Ort == ORT_STICK ? "usb" : "i";
    snprintf(k->RM_FEHLER[k->RM_FEHLER_N], sizeof k->RM_FEHLER[0], "%s.%s.%s.%s", k->RM_SCHRITT[0] ? k->RM_SCHRITT : "sonst", art, ort, k->RM_CODE[0] ? k->RM_CODE : "-");
    k->RM_FEHLER_N++;
}
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

/* NEU 1.0.7 (Hänger 31.10.2026 auf der PS5 Pro, FW 12.00): Jede Ordnerliste beim Durchgehen eines Ordnerbaums (Zählen,
 * Schutzprüfung, Freigeben, Löschen) bekommt eigenen Speicher, der gleich danach wieder frei wird. Bis 1.0.6 Fix blieben alle
 * Listen bis zum Programmende liegen (51.676 Dateien: 281 MB) - beim zweiten Lauf ohne Neustart war der Speicher der Konsole
 * voll und sie fror ein. letzter_fehler wird bei Bedarf in den Speicher darüber kopiert (sonst zeigte er ins Leere). */
static void liste_zu(sitzung *s, arena *alt, arena *t, const char *vorher)
{
    if (s->letzter_fehler != vorher) s->letzter_fehler = a_kopie(alt, s->letzter_fehler ? s->letzter_fehler : "");
    s->a = alt; arena_frei(t);
}
#define LISTE_AUF(sz) sitzung *const li_s = (sz); arena *const li_alt = li_s->a; const char *const li_vorher = li_s->letzter_fehler; \
                      arena *const li_t = arena_neu(); li_s->a = li_t
#define LISTE_ZU() liste_zu(li_s, li_alt, li_t, li_vorher)

/* -------------------------------------------------------------
 * Löschen (Kern.cs)
 * ------------------------------------------------------------- */
static void freigeben(kern *k, const char *d);
static void freigeben_ebene(kern *k, const char *d)
{
    s_befehl(k->S, "SITE CHMOD 777", d);
    dliste *l = s_liste(k->S, d, false);
    if (!l) return;
    for (int i = 0; i < l->n; i++) if (l->e[i].ist_ordner) freigeben(k, VERB(k->S->a, d, "/", l->e[i].name));
}
static void freigeben(kern *k, const char *d)   /* 1.0.7: je Ordner eigener Speicher */
{
    LISTE_AUF(k->S);
    VERSUCHE { freigeben_ebene(k, d); } FANGE { LISTE_ZU(); WEITERWERFEN(); } ENDE_VERSUCHE;
    LISTE_ZU();
}

static void loesche_baum(kern *k, const char *d);
static void loesche_baum_ebene(kern *k, const char *d)
{
    dliste *l = s_liste(k->S, d, false);
    if (l) {
        for (int i = 0; i < l->n; i++) {
            datei *e = &l->e[i];
            const char *p = VERB(k->S->a, d, "/", e->name);
            if (e->ist_ordner) loesche_baum(k, p);
            else if (e->ist_link) s_befehl(k->S, "DELE", p);         /* nur die Verknüpfung, kein CHMOD (würde dem Ziel gelten) */
            else { s_befehl(k->S, "SITE CHMOD 777", p); s_befehl(k->S, "DELE", p); }
        }
    }
    s_befehl(k->S, "RMD", d);
}
static void loesche_baum(kern *k, const char *d)   /* 1.0.7: je Ordner eigener Speicher */
{
    LISTE_AUF(k->S);
    VERSUCHE { loesche_baum_ebene(k, d); } FANGE { LISTE_ZU(); WEITERWERFEN(); } ENDE_VERSUCHE;
    LISTE_ZU();
}

static const char *stopp_text(void) { return T("  SICHERHEITSSTOPP: ", "  SAFETY STOP: "); }
/* NEU 1.1, Selbstschutz: zusätzliche Sperre VOR der Schutzliste aus 1.0.5 */
static bool selbst_gesperrt(kern *k, const char *p, bool melden)
{
    if (!kern_selbstschutz_gesperrt(k, p)) return false;
    if (melden) melde(k, VERB(A, stopp_text(), p, kern_bereiche_106() ? T(" gehört zu PS5 Cleanup und wird erst ganz zum Schluss entfernt - jetzt nichts gelöscht.", " belongs to PS5 Cleanup and is only removed at the very end - nothing deleted now.")
                                                                     : T(" gehört zu PS5 Cleanup selbst - jetzt nichts gelöscht (PS5 Cleanup selbst kommt erst ganz am Ende dran).", " belongs to PS5 Cleanup itself - nothing deleted now (PS5 Cleanup itself only comes at the very end).")));   /* NEU 1.0.8 (F1) */
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
static bool baum_hat_geschuetztes(kern *k, const char *d, const char **fund, int tiefe);
static bool baum_geschuetzt_ebene(kern *k, const char *d, const char **fund, int tiefe)
{
    dliste *volatile l = NULL;
    volatile bool fehl = false;
    VERSUCHE { l = s_liste(k->S, d, true); }
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); fehl = true; } ENDE_VERSUCHE;
    if (fehl || l == NULL) { *fund = a_kopie(A, d); return true; }   /* 1.0.7: Fund in den bleibenden Speicher */
    for (int i = 0; i < l->n; i++) {
        const char *c = VERB(k->S->a, d, "/", l->e[i].name);
        /* Codex-Prüfung 37: ein Spiel-Abbild eines Jailbreak-Einstiegs IN einem gewählten Ordner - nur einzeln löschbar */
        if (nachkomme_geschuetzt(k, c) || kern_jb_abbild(c)) { *fund = a_kopie(A, c); return true; }
        if (l->e[i].ist_ordner && !l->e[i].ist_link && baum_hat_geschuetztes(k, c, fund, tiefe + 1)) return true;
    }
    return false;
}
static bool baum_hat_geschuetztes(kern *k, const char *d, const char **fund, int tiefe)   /* 1.0.7: je Ordner eigener Speicher */
{
    if (tiefe > 64) { *fund = a_kopie(A, d); return true; }
    volatile bool r = false;
    LISTE_AUF(k->S);
    VERSUCHE { r = baum_geschuetzt_ebene(k, d, fund, tiefe); } FANGE { LISTE_ZU(); WEITERWERFEN(); } ENDE_VERSUCHE;
    LISTE_ZU();
    return r;
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
static const char *baum_jb(kern *k, const char *d, int tiefe);
static const char *baum_jb_ebene(kern *k, const char *d, int tiefe)
{
    const char *w;
    dliste *volatile l = NULL;
    VERSUCHE { l = s_liste(k->S, d, true); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); l = NULL; } ENDE_VERSUCHE;
    if (l == NULL) return NULL;
    for (int i = 0; i < l->n; i++) {
        const char *c = VERB(k->S->a, d, "/", l->e[i].name);
        if ((w = kern_jb_weg(c)) != NULL) return w;        /* feste Texte - bleiben gültig */
        if (l->e[i].ist_ordner && !l->e[i].ist_link && (w = baum_jb(k, c, tiefe + 1)) != NULL) return w;
    }
    return NULL;
}
static const char *baum_jb(kern *k, const char *d, int tiefe)   /* 1.0.7: je Ordner eigener Speicher */
{
    if (tiefe == 0) { const char *f = funde_jb(k, d); if (f) return f; }
    const char *w = kern_jb_weg(d);
    if (w || tiefe > 64) return w;
    const char *volatile r = NULL;
    LISTE_AUF(k->S);
    VERSUCHE { r = baum_jb_ebene(k, d, tiefe); } FANGE { LISTE_ZU(); WEITERWERFEN(); } ENDE_VERSUCHE;
    LISTE_ZU();
    return r;
}

static bool loeschen_ausfuehren(kern *k, const char *p);
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
    /* Codex-Prüfung 98/100 (P2): ab hier jede Ordnerliste (Existenzprüfungen, Liste des Ordners darüber) in eigenem Speicher, der gleich
     * danach wieder frei wird - bisher blieben sie je Datei im Speicher des Kerns liegen; beim Löschen tausender Dateien aus EINEM Ordner
     * wuchs er quadratisch, bis die Speicherbremse den Lauf anhielt. Erst NACH kern_ohne_verknuepfung: dessen LINK_CACHE muss im
     * bleibenden Speicher liegen. Meldungen gehen weiter über VERB(A, ...) in den bleibenden Speicher. */
    volatile bool r = false;
    LISTE_AUF(k->S);
    VERSUCHE { r = loeschen_ausfuehren(k, p); } FANGE { LISTE_ZU(); WEITERWERFEN(); } ENDE_VERSUCHE;
    LISTE_ZU();
    return r;
}
static bool loeschen_ausfuehren(kern *k, const char *p)
{
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
            if (k->RM_ERFASSEN && ra.code >= 400) rm_zeile_mit(k, ra.text, "datei");   /* NEU 1.0.7: QR-Rückmeldung */
            melde(k, VERB(A, stopp_text(), p, T(" ließ sich nicht sicher als Ganzes löschen - der Rest bleibt stehen.", " could not be deleted safely as a whole - the rest is kept.")));
            return false;
        }
        loesche_baum(k, p);
    }
    return !s_existiert(k->S, p);
}

/* Leeren Ordner entfernen, der als eigener Eintrag gewählt wurde: nur RMD -
 * RMD löscht nie Inhalt mit. Ist inzwischen etwas drin, schlägt es fehl. */
/* NEU 1.0.7: /system_ex für diese Sitzung beschreibbar machen (PS5: nur einmal pro Einschalten; zurückgestellt wird am Ende).
 * k->SYSEX: 1 = geht, -1 = geht nicht (Grund in k->SYSEX_GRUND). Ausgelagert am 03.11.2026 (auch für Einhängeordner gelöschter Spiele). */
static bool sysex_bereit(kern *k)
{
    if (k->SYSEX == 0) {
        const char *t = NULL;
        /* ohne Haken (Windows über FTP, PC-Tests): einfach versuchen - ist /system_ex schreibgeschützt, scheitert rmdir mit Meldung */
        int r = zugriff_system_ex_oeffnen ? zugriff_system_ex_oeffnen(A, &t) : 1;
        k->SYSEX = r == 1 ? 1 : -1;
        k->SYSEX_GRUND = r == 1 ? NULL : (r == -2 ? T("/system_ex lässt sich in dieser Sitzung nicht mehr beschreibbar machen - nach einem Neustart der PS5 nochmal versuchen",
                                                       "/system_ex cannot be made writable again in this session - try again after restarting the PS5")
                                                  : (t && t[0] ? t : (zugriff_system_ex_oeffnen ? T("/system_ex ließ sich nicht beschreibbar machen", "/system_ex could not be made writable")
                                                                                                : T("auf diesem Gerät nicht möglich", "not possible on this device"))));
        if (zugriff_diagnose) zugriff_diagnose(a_fmt(A, "/system_ex öffnen -> %d %s", r, t ? t : ""));
    }
    return k->SYSEX > 0;
}
/* NEU 1.0.7 (PS5-Test 03.11.2026, ProsperoStore/Playden): ShadowMount-Spiel samt Spieldatei gelöscht - sein Einhängeordner
 * /system_ex/app/<Kennung> war beim Nachschauen noch belegt und stand darum nicht bei "leere Einhängeordner". Jetzt ist er leer:
 * mitnehmen, nur wenn erlaubt (kern_schutz.c sm_leerer_ordner), nur ganz leer, nur mit rmdir. Geht das nicht, bleibt er still
 * (unsichtbar, ohne Kachel) - beim nächsten Nachschauen steht er bei "leere Einhängeordner". */
static bool leeren_ordner_loeschen(kern *k, const char *p);
/* kachel_weg: die Kachel wurde in diesem Lauf nachweislich ausgetragen (über Sony bzw. stand gar nicht im Menü). NEU 03.11.2026 (da_Chris86
 * 17:50, PS5-Test: nach der vollständigen Bereinigung blieben 9 leere Einhängeordner von Spielen, bei denen nur die Kachel ging): dann geht
 * auch der leere Einhängeordner - ShadowMount legt ihn beim nächsten Einbinden der Spieldatei selbst wieder an. Nur leer, nur rmdir. */
static void sm_einhaengeordner_weg(kern *k, const eintrag *f, const sliste *geloescht, bool kachel_weg)
{
    if (kern_bereiche_106() || k->simulation || !f->KachelId || !f->Quelle || !f->Quelle[0] || (!kachel_weg && !sl_enthaelt(geloescht, f->Quelle))) return;
    const char *p = VERB(A, "/system_ex/app/", f->KachelId);
    volatile bool leer = false;
    VERSUCHE { dliste *in = kern_erlaubt(k, p) && s_existiert(k->S, p) ? s_liste(k->S, p, false) : NULL; leer = in && in->n == 0; }
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); leer = false; } ENDE_VERSUCHE;
    if (!leer || !sysex_bereit(k)) return;
    volatile bool weg = false;
    VERSUCHE { weg = leeren_ordner_loeschen(k, p); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); weg = false; } ENDE_VERSUCHE;
    if (weg) melde(k, VERB(A, T("  gelöscht      – leerer Einhängeordner ", "  deleted      – empty mount folder "), p));
    else if (zugriff_diagnose) zugriff_diagnose(VERB(A, "Einhängeordner bleibt: ", p));
}
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

/* NEU 1.0.9 (da_Chris86 08.11.2026, Bild aus 1.0.8): den Aufnahme-Zwischenordner /user/av_contents/content_tmp/<Kennung> legt die PS5 beim
 * Start einer App selbst an. Solange die App da ist, bietet die Suche ihn nicht an (kern_suche.c) - ist ihre Kachel in diesem Lauf
 * gelöscht worden, geht er hier mit, nur wenn er leer ist. Nie der von PS5 Cleanup (Selbstentfernung 2b), nie der eines Systemprogramms. */
/* Codex-Prüfung 127: bleibt er (inzwischen nicht leer, abgelehnt), steht das im Protokoll; der Ordner zählt beim Freigeräumten mit;
 * true = die Verbindung ist weg (wie im Löschlauf mit NOOP geprüft) - der Aufrufer setzt dann "verloren". */
static bool zwischenordner_weg(kern *k, const eintrag *f, ergebnis *erg)
{
    if (kern_bereiche_106() || !f->KachelId || !h_m(f->KachelId, "^[A-Z]{4}\\d{5}$") || h_m(f->KachelId, "^(PSCL0000[12]|NPXS\\d{5})$")) return false;
    const char *p = VERB(A, "/user/av_contents/content_tmp/", f->KachelId);
    if (h_hat(&f->Pfade, p)) return false;
    /* 0 nicht da (nichts zu tun), 1 entfernt, 2 bleibt (Grund), 3 Fehler beim Zugriff - Codex-Prüfung 128: ist er da und bleibt, steht
     * das immer im Protokoll (geschützt, nicht leer, nicht lesbar, abgelehnt) */
    volatile int was = 0;
    const char *volatile grund = "";
    VERSUCHE {
        if (s_existiert(k->S, p)) {
            if (selbst_gesperrt(k, p, false) || beschaedigt(k, p, false) || !kern_erlaubt(k, p) || !kern_ohne_verknuepfung(k, p)) { was = 2; grund = T("geschützt", "protected"); }
            else {
                dliste *in = s_liste(k->S, p, false);
                if (!in) was = 3;
                else if (in->n > 0) { was = 2; grund = T("nicht leer", "not empty"); }
                else {   /* nur RMD: nie mit Inhalt */
                    s_befehl(k->S, "RMD", p);
                    if (s_existiert(k->S, p)) { was = 2; grund = T("ließ sich nicht entfernen", "could not be removed"); } else was = 1;
                }
            }
        }
    } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); was = 3; } ENDE_VERSUCHE;
    if (was == 3) grund = T("nicht lesbar", "not readable");
    if (was == 1) {
        melde(k, VERB(A, k->simulation ? T("  würde gelöscht – leerer Aufnahme-Zwischenordner ", "  would be deleted – empty capture temp folder ")
                                       : T("  gelöscht      – leerer Aufnahme-Zwischenordner ", "  deleted      – empty capture temp folder "), p));
        erg->Frei.Ordner++;
    }
    else if (was >= 2) melde(k, VERB(A, T("  bleibt        – Aufnahme-Zwischenordner ", "  kept         – capture temp folder "), p, " (", grund, ")"));
    if (was != 3) return false;
    volatile bool da = false;
    VERSUCHE { da = s_befehl(k->S, "NOOP", NULL).code == 200; } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); da = false; } ENDE_VERSUCHE;
    return !da;
}
/* Nur einen wirklich leeren Ordner entfernen (RMD löscht nie Inhalt mit) */
static void leeren_ordner_entfernen(kern *k, const char *o)
{
    VERSUCHE {
        const char *pg = NULL;   /* NEU 1.0.7: auch leere Ordner installierter, nicht gewählter PS4-Spiele bleiben */
        bool ps4 = !kern_bereiche_106() && kern_ps4_geschuetzt(k, o, &pg);
        if (!ps4 && !selbst_gesperrt(k, o, false) && !beschaedigt(k, o, false) && kern_erlaubt(k, o)) {
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
    volatile int fall = 0;
    volatile bool gefunden = false, ordner = false; volatile long long dgr = 0;
    {   /* Codex-Prüfung 75: auch die Liste des Ordners darüber nur in eigenem Speicher (sie kann groß sein und wird oft gebraucht) */
        const char *b = h_blatt(a, p), *el = h_eltern(a, p);
        LISTE_AUF(s);
        dliste *volatile l = NULL;
        VERSUCHE { l = s_liste(s, el, true); }               /* streng: nur "gibt es nicht" ist null */
        FANGE { if (ausn_art == AUSN_ABBRUCH) { LISTE_ZU(); WEITERWERFEN(); } fall = ausn_art == AUSN_IO ? 1 : 2; } ENDE_VERSUCHE;
        if (fall == 0 && l) for (int i = 0; i < l->n; i++) if (strcmp(l->e[i].name, b) == 0) { gefunden = true; ordner = l->e[i].ist_ordner; dgr = l->e[i].groesse; break; }
        LISTE_ZU();
    }
    if (fall == 1) { erg.Gekappt = true; erg.Lesefehler = true; return erg; }   /* Verbindung evtl. aus dem Takt */
    if (fall == 2) { erg.Gekappt = true; return erg; }                            /* saubere Fehlerantwort */
    if (!gefunden) return erg;
    if (!ordner) { erg.Bytes = dgr; erg.Dateien = 1; if (lauf) lauf(lctx, dgr, 1); return erg; }
    erg.Ordner = 1;
    sliste stapel = {0}; sl_add(a, &stapel, p);
    while (stapel.n > 0) {
        if (jetzt() > deadline || erg.Dateien + erg.Ordner > 200000) { erg.Gekappt = true; break; }
        const char *d = stapel.e[--stapel.n];
        LISTE_AUF(s);                                              /* 1.0.7: Ordnerliste in eigenem Speicher */
        dliste *volatile kl = NULL;
        volatile int f2 = 0;
        VERSUCHE { kl = s_liste(s, d, true); }
        FANGE { if (ausn_art == AUSN_ABBRUCH) { LISTE_ZU(); WEITERWERFEN(); } f2 = ausn_art == AUSN_IO ? 1 : 2; } ENDE_VERSUCHE;
        if (f2 == 1) { erg.Gekappt = true; erg.Lesefehler = true; LISTE_ZU(); break; }
        if (f2 == 2) { erg.Gekappt = true; LISTE_ZU(); continue; }            /* dieser Ordner nicht lesbar: Rest weiterzählen */
        if (kl == NULL) { erg.Gekappt = true; LISTE_ZU(); continue; }         /* im Ordner darüber gelistet, aber nicht lesbar */
        long long bb = 0; int n = 0;
        for (int i = 0; i < kl->n; i++) {
            datei *x = &kl->e[i];
            if (x->ist_ordner) { erg.Ordner++; sl_add(a, &stapel, VERB(a, d, "/", x->name)); }
            else if (!x->ist_link) { erg.Dateien++; erg.Bytes += x->groesse; bb += x->groesse; n++; }
            else { erg.Dateien++; n++; }
        }
        LISTE_ZU();
        if (lauf) lauf(lctx, bb, n);
    }
    return erg;
}

/* EintragGroesse: aus den gemerkten Pfad-Größen (NULL = noch nicht alles gezählt) */
groesse *kern_eintrag_groesse(kern *k, const eintrag *f, bool wieGefunden) { return kern_eintrag_groesse_in(k, A, f, wieGefunden); }
/* NEU 1.1: dasselbe mit eigenem Speicher (für die Anzeige aus dem Oberflächen-Faden) */
/* NEU 1.0.8 (Arbeitsplan 4.2): was die Suche gefunden hat, je Speicherort (jeder Pfad nur einmal) - für die Balken der Startseite.
 * false = noch nicht alles gezählt (die Summen sind dann zu klein). */
bool kern_bytes_je_ort(kern *k, long long aus[4])
{
    for (int o = 0; o < 4; o++) aus[o] = 0;
    arena *a = arena_neu();
    mtab gesehen = mt_neu(false);
    bool alle = true;
    sperren(k);
    for (int i = 0; i < k->Funde.n; i++) {
        const eintrag *f = k->Funde.e[i];
        for (int j = 0; j < f->PfadeBasis.n; j++) {
            const char *p = f->PfadeBasis.e[j];
            if (mt_hat(&gesehen, p)) continue;
            mt_setze(a, &gesehen, p, (void *)1);
            groesse *x = mt_hole(&k->PFAD_GROESSE, p);
            if (!x) { alle = false; continue; }
            int o = kern_pfad_ort(p);
            if (o >= 0 && o < 4) aus[o] += x->Bytes;
        }
    }
    entsperren(k);
    arena_frei(a);
    return alle;
}
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
    /* NEU 1.0.6.1 (Absturz auf FW 13.60, 30.10.2026): bis 1.0.6 blieben ALLE Ordnerlisten bis zum Ende des Zählens im Speicher
     * (bei vielen Dateien Hunderte MB). Jetzt eigener Speicher je Eintrag, danach sofort frei. Gemerkt wird nur die Summe
     * (groesse_merken kopiert den Pfad in k->ga). */
    arena *volatile t = NULL;
    VERSUCHE {
        for (int i = 0; i < liste.n; i++) {
            eintrag *e = liste.e[i];
            if (stopp(sc)) break;
            t = arena_neu(); s->a = t;
            sliste pfade = sl_kopie(t, &e->PfadeBasis);
            if (e->Quelle[0]) sl_add(t, &pfade, e->Quelle);   /* 1.0.7: auch Jailbreak-Spiel-Abbilder (Bereich 7) */
            bool aufhoeren = false;
            for (int j = 0; j < pfade.n && !aufhoeren; j++) {
                bool doppelt = false; for (int m = 0; m < j; m++) if (strcmp(pfade.e[m], pfade.e[j]) == 0) doppelt = true;
                if (doppelt || kern_pfad_gezaehlt(k, pfade.e[j])) continue;
                groesse g = baum_groesse(s, t, pfade.e[j], jetzt() + 120, NULL, NULL);
                if (g.Lesefehler) { aufhoeren = true; break; }    /* nichts merken, aufhören */
                groesse_merken(k, pfade.e[j], &g, true);
            }
            s->a = a; s->letzter_fehler = ""; arena_frei(t); t = NULL;
            if (aufhoeren) break;
            fertig(fc, e);
        }
    } FANGE { } ENDE_VERSUCHE;                                   /* catch { } */
    s->a = a;
    if (t) arena_frei(t);
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
                {   /* 1.0.7: Zählen in eigenem Speicher, danach frei (nur die Summe bleibt) */
                    LISTE_AUF(k->S);
                    VERSUCHE { x = baum_groesse(k->S, li_t, p, deadline, lauf_zaehlen, &ls); } FANGE { LISTE_ZU(); WEITERWERFEN(); } ENDE_VERSUCHE;
                    LISTE_ZU();
                }
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
    const double KB = 1024, MB = KB * 1024, GB = MB * 1024, TB = GB * 1024;
    /* NEU 1.0.8 (K4, da_Chris86s Bilder vom 04.11.2026): "2,01 TB" statt "2.062,6 GB" (Texte bis 1.0.6 für die Vergleichstests unverändert) */
    if (bytes >= TB && !kern_bereiche_106()) return VERB(a, zahl_n(a, (double)bytes / TB, (double)bytes / TB < 10 ? 2 : 1), " TB");
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
static const char *antwort_grund_text(arena *a, antwort r) { const char *t = r.text ? r.text : ""; return a_kopie(a, strlen(t) > 4 ? t + 4 : t); }
/* NEU 1.0.7 (Codex-Prüfung 69, QR-Rückmeldung): Fehlercodes direkt aus der Antwort der Zugriffsschicht bzw. Sonys nehmen - dort stehen
 * keine Namen und Pfade ("550 Sony 0x80990001", "550 Permission denied"). Der Schritt kommt aus RM_HINWEIS (wer gerade fragt). */
static void rm_zeile_mit(kern *k, const char *t, const char *hinweis);
static const char *antwort_grund_rm(kern *k, arena *a, antwort r) { if (k->RM_ERFASSEN && r.code >= 400) rm_zeile_mit(k, r.text, k->RM_HINWEIS); return antwort_grund_text(a, r); }
#define antwort_grund(a, r) antwort_grund_rm(k, (a), (r))
/* NEU 1.0.7 (QR-Rückmeldung): Schritt "sony" nur während dieses Aufrufs */
static int sony_entfernen_innen(kern *k, const char *ordner, const char *id, const char **grund);
static int sony_entfernen(kern *k, const char *ordner, const char *id, const char **grund)
{ k->RM_HINWEIS = "sony"; int r = sony_entfernen_innen(k, ordner, id, grund); k->RM_HINWEIS = NULL; return r; }
static int sony_entfernen_innen(kern *k, const char *ordner, const char *id, const char **grund)
{
    antwort r = s_befehl(k->S, "SITE UNINSTALL?", id);
    if (r.code == 451) { *grund = antwort_grund(A, r); return 3; }   /* NEU 1.0.7: Einbindung unklar - unverändert lassen */
    if (r.code != 200) { *grund = antwort_grund(A, r); return 0; }
    r = s_befehl(k->S, "SITE UNINSTALL", ordner);
    /* NEU 1.0.7 (da_Chris86 01.11.2026, Reste ohne Kachel): Sony führt die Kennung nicht im Menü und app.db kennt sie auch nicht -
     * es gibt keine Kachel, die stehen bleiben könnte: 4 = die Dateien darf der Ablauf selbst löschen (bis 1.0.7: 60 s gewartet,
     * dann fälschlich "Sony löscht noch" - und nichts gelöscht) */
    if (r.code == 226) { *grund = antwort_grund(A, r); return 4; }
    if (r.code == 451) { *grund = antwort_grund(A, r); return 3; }
    if (r.code != 200) { *grund = VERB(A, T("Entfernung unvollständig - ", "Removal incomplete - "), antwort_grund(A, r)); return 0; }   /* 01.11.2026: den Grund nennt die Plattform selbst */
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
    {   /* 1.0.7: in eigenem Speicher */
        LISTE_AUF(k->S);
        VERSUCHE { g = baum_groesse(k->S, li_t, p, jetzt() + 5, NULL, NULL); } FANGE { LISTE_ZU(); if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); fehl = true; } ENDE_VERSUCHE;
        if (!fehl) LISTE_ZU();
    }
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
/* NEU 1.0.7 (da_Chris86 31.10.2026 23:32: "ohne Geisterkacheln, damit überhaupt kein Datenbank-Neuaufbau notwendig wird"):
 * Eine App mit Kachel wird nur entfernt, wenn Sony die Kachel nachweislich austrägt. Geht das nicht, bleibt sie VOLLSTÄNDIG
 * erhalten (keine Datei gelöscht) - dann bleibt sie startbar und lässt sich später im PS5-Menü oder mit PS5 Cleanup entfernen.
 * Bis 1.0.6 wurden die Dateien trotzdem gelöscht ("Dateien weg, Kachel bleibt") - genau so entstand die Geisterkachel von
 * PS5Upload (PSUP00001, Test 30.10.2026), die einen Datenbank-Neuaufbau nötig machte. */
static void kachel_bleibt(kern *k, const eintrag *f, const char *grund, mtab *spielBleibt)
{
    if (ist_teil(f, "Spiel") && f->Gruppe) mt_setze(A, spielBleibt, f->Gruppe, (void *)1);   /* Update/Zusatzinhalt bleiben dann auch */
    const char *g = grund && grund[0] ? grund : T("die Kachel ließ sich nicht sicher über Sony austragen", "the tile could not be removed safely via Sony");
    /* NEU 1.0.8 (PS5-Test 07.11.2026 17:40, PS5Upload Minion): hat Sony den Auftrag angenommen, kann Sony die App noch im Hintergrund
     * entfernen (die Dateien waren Minuten später weg, die Kachel blieb) - dann stimmt "bleibt vollständig erhalten" nicht */
    bool angenommen = strstr(g, "angenommen") != NULL || strstr(g, "accepted") != NULL;
    melde(k, VERB(A, T("  unverändert   ", "  unchanged   "), f->Text, "   (", g,
                  angenommen ? T(" - PS5 Cleanup hat selbst nichts gelöscht; Sony kann die App aber noch im Hintergrund entfernen - nach einem Neustart nochmal nachschauen)",
                                 " - PS5 Cleanup deleted nothing itself; Sony may still remove the app in the background - check again after a restart)")
                             : T(" - die App bleibt vollständig erhalten, es wurde nichts gelöscht)", " - the app stays complete, nothing was deleted)")));
    sl_add(A, &k->UNVERAENDERT, f->Text);
}
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
        if (f->Quelle[0]) kachelnWeg = true;
    }
    if (smDatenWeg) return T("ShadowMount wurde mitgelöscht - richte es neu ein, dann erscheinen die Spiele auf der M.2 wieder als Kacheln.", "ShadowMount was deleted too - set it up again and the games on the M.2 will show up as tiles again.");
    if (k->SM_ANGEHALTEN) return T("ShadowMount wurde angehalten - zum Weiterspielen einfach neu laden.", "ShadowMount was stopped - just load it again to keep playing.");
    if (kachelnWeg) return T("Die Spiele auf der Platte erscheinen wieder als Kacheln, sobald ShadowMount läuft.", "The games on the SSD/M.2 will show up as tiles again once ShadowMount is running.");
    return "";
}

/* NEU 1.0.7 (da_Chris86 01.11.2026 17:31 "Neuaufbau der Datenbank ist inakzeptabel", 18:11 "alles in einem sauberen Rutsch";
 * PS5-Tests 01.11.2026 17:53-18:24): Bilder/Videos nur über Sonys Löschdienst (wie "Löschen" in der Mediengalerie), Spielstände nur
 * zusammen mit ihrem Eintrag in Sonys Spielstand-Liste. Bis 1.0.6 wurden nur die Dateien gelöscht - Mediengalerie und
 * Spielstand-Liste zeigten danach leere Einträge, bis die Datenbank neu aufgebaut wurde (der Neuaufbau löscht PS4-FPKG-Spiele). */
static void medien_sammeln(kern *k, const char *p, int tiefe, sliste *aus, sliste *fremd)
{
    if (tiefe > 2) { sl_add(A, fremd, p); return; }   /* tiefer legt die PS5 keine Aufnahmen ab (Prüfung 01.11.2026: nicht still übergehen) */
    dliste *l = s_liste(k->S, p, true);
    if (!l) return;
    for (int i = 0; i < l->n; i++) {
        const datei *d = &l->e[i];
        if (!strcmp(d->name, ".") || !strcmp(d->name, "..") || d->ist_link) continue;   /* Verknüpfungen nie an Sony (löscht danach der Ablauf selbst) */
        const char *c = VERB(A, p, "/", d->name);
        if (d->ist_ordner) medien_sammeln(k, c, tiefe + 1, aus, fremd);
        else if (zugriff_medium_pfad(c)) sl_add(A, aus, c);
        else if (!h_m(d->name, "(?i)\\.(meta|dat)$")) sl_add(A, fremd, c);   /* unbekannte Datei - nie selbst löschen */
    }
}
/* true = die Ordner sind weg oder ändern sich nicht mehr (Sony räumt nach dem Löschen noch kurz auf), false = nach "sekunden" noch in Bewegung */
static bool warten_bis_ruhig(kern *k, const sliste *pfade, double sekunden)
{
    double bis = jetzt() + sekunden;
    long long vorher = -2; int gleich = 0;
    for (;;) {
        bool alle = true, unklar = false; long long w = 0;
        for (int j = 0; j < pfade->n; j++) if (s_existiert(k->S, pfade->e[j])) { alle = false; long long kw = ordner_kennwert(k, pfade->e[j]); if (kw < 0) unklar = true; w = w * 131 + kw; }
        if (alle) return true;
        if (!unklar && w == vorher) { if (++gleich >= 2) return true; } else gleich = 0;
        vorher = unklar ? -2 : w;
        if (jetzt() > bis) return false;
        warte(k, 1000);
    }
}
/* Aufnahmen eines Spiels über Sonys Löschdienst. 1 = alle Bilder/Videos von Sony gelöscht (*n Stück; was danach noch da ist -
 * Vorschaubilder ohne Aufnahme, leere Ordner - löscht der Ablauf selbst), 0 = nichts gelöscht, 2 = nur ein Teil. *grund: warum. */
/* NEU 1.0.7 (QR-Rückmeldung): Schritt "med" nur während dieses Aufrufs */
static int medien_ueber_sony_innen(kern *k, const eintrag *f, const char **grund, volatile int *n);
static int medien_ueber_sony(kern *k, const eintrag *f, const char **grund, volatile int *n)
{ k->RM_HINWEIS = "med"; int r = medien_ueber_sony_innen(k, f, grund, n); k->RM_HINWEIS = NULL; return r; }
static int medien_ueber_sony_innen(kern *k, const eintrag *f, const char **grund, volatile int *n)
{
    *n = 0;
    if (!k->MEDIEN) {
        *grund = T("Bilder und Videos löscht PS5 Cleanup nur über Sonys Löschdienst (wie die Mediengalerie) - den gibt es nur in PS5 Cleanup auf der PS5; sonst blieben leere Einträge in der Mediengalerie",
                   "PS5 Cleanup deletes screenshots and videos only via Sony's delete service (like the Media Gallery) - only PS5 Cleanup on the PS5 has it; otherwise empty entries would remain in the Media Gallery");
        return 0;
    }
    /* Prüfung 01.11.2026: vorher dieselben Schutzprüfungen wie beim eigenen Löschen - sonst löschte Sony die Aufnahmen, der Rest bliebe
     * wegen der Schutzprüfung halb stehen */
    for (int j = 0; j < f->Pfade.n; j++) {
        volatile bool frei = false;
        VERSUCHE { frei = schutz_vorab(k, f->Pfade.e[j]); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); frei = false; } ENDE_VERSUCHE;
        if (!frei) { *grund = VERB(A, T("die Schutzprüfung lässt das Löschen nicht zu: ", "the protection check does not allow deleting: "), f->Pfade.e[j]); return 0; }
    }
    sliste m = {0}, fremd = {0};
    for (int j = 0; j < f->Pfade.n; j++)
        if (h_m(f->Pfade.e[j], "^/user/av_contents/(photo|video)/") && s_existiert(k->S, f->Pfade.e[j])) medien_sammeln(k, f->Pfade.e[j], 0, &m, &fremd);
    if (fremd.n > 0) {
        *grund = VERB(A, T("unbekannte Datei im Aufnahme-Ordner (Sony kann sie nicht zuordnen) - darum bleibt alles: ", "unknown file in the capture folder (Sony cannot assign it) - so everything stays: "), fremd.e[0]);
        return 0;
    }
    for (int i = 0; i < m.n; i++) {                       /* erst alle prüfen, dann löschen */
        antwort r = s_befehl(k->S, "SITE MEDIUM?", m.e[i]);
        if (r.code != 200) { *grund = VERB(A, h_blatt(A, m.e[i]), ": ", antwort_grund(A, r)); return 0; }
    }
    for (int i = 0; i < m.n; i++) {
        if (!s_existiert(k->S, m.e[i])) continue;
        antwort r = s_befehl(k->S, "SITE MEDIUM", m.e[i]);
        if (zugriff_diagnose) zugriff_diagnose(a_fmt(A, "Mediengalerie: %s -> %d %s", m.e[i], r.code, r.text ? r.text : ""));
        if (r.code != 200) { *grund = VERB(A, h_blatt(A, m.e[i]), ": ", antwort_grund(A, r)); return (*n || r.code == 551) ? 2 : 0; }   /* 551: Sony evtl. schon dabei */
        (*n)++;
    }
    if (*n && !warten_bis_ruhig(k, &f->Pfade, 15)) {
        *grund = T("Sony räumt nach dem Löschen noch auf - der Rest (Ordner, Vorschaubilder) bleibt vorerst, später nochmal nachschauen",
                   "Sony is still tidying up after deleting - the rest (folders, thumbnails) stays for now, scan again later");
        return 2;
    }
    return 1;
}
/* Spielstand: erst die Einträge in Sonys Spielstand-Liste austragen (vorher gesichert), die Dateien löscht danach der Ablauf.
 * 1 = ausgetragen, 0 = nichts geändert (*grund). Vorher müssen alle Pfade löschbar sein - sonst wären die Einträge weg, die Dateien
 * aber nicht löschbar. */
/* NEU 1.0.7 (QR-Rückmeldung): Schritt "ss" nur während dieses Aufrufs */
static int spielstand_austragen_innen(kern *k, const eintrag *f, const char **grund);
static int spielstand_austragen(kern *k, const eintrag *f, const char **grund)
{ k->RM_HINWEIS = "ss"; int r = spielstand_austragen_innen(k, f, grund); k->RM_HINWEIS = NULL; return r; }
static int spielstand_austragen_innen(kern *k, const eintrag *f, const char **grund)
{
    if (!k->SPIELSTAENDE) {
        *grund = T("Spielstände löscht PS5 Cleanup nur zusammen mit ihrem Eintrag in Sonys Spielstand-Liste - das geht nur in PS5 Cleanup auf der PS5; sonst blieben leere Einträge in der Spielstand-Liste",
                   "PS5 Cleanup deletes saved data only together with its entry in Sony's saved data list - only PS5 Cleanup on the PS5 can do that; otherwise empty entries would remain in the saved data list");
        return 0;
    }
    const char *o = f->Pfade.n > 0 ? f->Pfade.e[0] : NULL;
    if (!o || !zugriff_spielstand_pfad(o, NULL, NULL, NULL)) { *grund = T("kein Spielstand-Ordner", "not a saved data folder"); return 0; }
    for (int j = 0; j < f->Pfade.n; j++) {
        volatile bool frei = false;
        VERSUCHE { frei = schutz_vorab(k, f->Pfade.e[j]); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); frei = false; } ENDE_VERSUCHE;
        if (!frei) { *grund = VERB(A, T("die Schutzprüfung lässt das Löschen nicht zu: ", "the protection check does not allow deleting: "), f->Pfade.e[j]); return 0; }
    }
    antwort r = s_befehl(k->S, "SITE SPIELSTAND?", o);
    if (r.code != 200) { *grund = antwort_grund(A, r); return 0; }
    r = s_befehl(k->S, "SITE SPIELSTAND", o);
    if (zugriff_diagnose) zugriff_diagnose(a_fmt(A, "Spielstand-Liste: %s -> %d %s", o, r.code, r.text ? r.text : ""));
    if (r.code != 200) { *grund = antwort_grund(A, r); return 0; }
    return 1;
}
/* NEU 1.0.7: Spielstand-Ordner in einem ANDEREN Eintrag (z. B. Jailbreak-Spiel samt Spielstand) - vor dem Löschen seiner Dateien
 * austragen (Sonys Kachel-Auftrag kommt vorher). 1 = ausgetragen, 0 = nicht (*grund) - dann bleiben er und alles danach. */
/* NEU 1.0.7 (QR-Rückmeldung): Schritt "ss" nur während dieses Aufrufs */
static int spielstand_ordner_austragen_innen(kern *k, const char *p, const char **grund);
static int spielstand_ordner_austragen(kern *k, const char *p, const char **grund)
{ k->RM_HINWEIS = "ss"; int r = spielstand_ordner_austragen_innen(k, p, grund); k->RM_HINWEIS = NULL; return r; }
static int spielstand_ordner_austragen_innen(kern *k, const char *p, const char **grund)
{
    if (!k->SPIELSTAENDE) {
        *grund = T("der Spielstand lässt sich hier nicht aus Sonys Spielstand-Liste austragen (nur PS5 Cleanup auf der PS5) - sonst bliebe ein leerer Eintrag",
                   "the saved data cannot be removed from Sony's saved data list here (only PS5 Cleanup on the PS5) - otherwise an empty entry would remain");
        return 0;
    }
    antwort r = s_befehl(k->S, "SITE SPIELSTAND?", p);
    if (r.code != 200) { *grund = antwort_grund(A, r); return 0; }
    r = s_befehl(k->S, "SITE SPIELSTAND", p);
    if (zugriff_diagnose) zugriff_diagnose(a_fmt(A, "Spielstand-Liste: %s -> %d %s", p, r.code, r.text ? r.text : ""));
    if (r.code != 200) { *grund = antwort_grund(A, r); return 0; }
    return 1;
}

/* NEU 1.0.8 (Arbeitsplan 3.5, aus PLAN Punkt 3): bei einem nur teilweise gelöschten Eintrag zählt das wirklich Gelöschte zum
 * "Freigeräumt" - bis 1.0.7 gar nichts (ein halb gelöschter großer Ordner ergab "Freigeräumt: 0 Byte"). Was noch da ist, wird nachgezählt
 * (höchstens 5 s je Eintrag); ist das nicht sicher möglich (Lesefehler, Zeit um), kommt nichts dazu - lieber zu wenig als zu viel. */
static long long frei_teilweise(kern *k, const eintrag *f, ergebnis *erg)
{
    if (!f->HatGroesse || k->simulation) return 0;
    groesse rest = {0};
    volatile bool sicher = true;
    /* Codex-Prüfung 75: eigener Speicher (wird gleich wieder frei) und höchstens 5 s für den ganzen Eintrag, nicht je Pfad */
    arena *volatile ta = arena_neu();
    const double bis = jetzt() + 5;
    for (int j = 0; j < f->Pfade.n && sicher; j++) {
        const char *p = f->Pfade.e[j];
        if (jetzt() > bis) { sicher = false; break; }
        VERSUCHE { groesse g = baum_groesse(k->S, ta, p, bis, NULL, NULL); if (g.Gekappt || g.Lesefehler) sicher = false; else plus(&rest, &g); }
        FANGE { if (ausn_art == AUSN_ABBRUCH) { arena_frei(ta); WEITERWERFEN(); } sicher = false; } ENDE_VERSUCHE;
    }
    arena_frei(ta);
    if (!sicher || f->Bytes <= rest.Bytes) return 0;
    erg->Frei.Bytes += f->Bytes - rest.Bytes;
    if (f->Dateien > rest.Dateien) erg->Frei.Dateien += f->Dateien - rest.Dateien;
    if (f->Ordner > rest.Ordner) erg->Frei.Ordner += f->Ordner - rest.Ordner;
    return f->Bytes - rest.Bytes;
}
static bool tool_eintrag(const eintrag *f) { return f->Art == ART_TOOL_DATEN || f->Art == ART_TOOL_DATEN_WICHTIG || f->Art == ART_TOOL_KACHEL; }
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
    /* NEU 1.0.7 (da_Chris86 31.10.2026 19:56): PS4-Spiele direkt vor dem Löschen neu erfassen (nur lesen) und festhalten, welche
     * gewählt sind - alles andere, was zu einem installierten PS4-Spiel gehört, bleibt (kern_ps4.c). Geht das nicht: im Zweifel nicht löschen. */
    k->PS4_GESCHUETZT = 0; k->PS4_UNKLAR = false; k->UNVERAENDERT = (sliste){0};
    if (!kern_bereiche_106()) {
        VERSUCHE { kern_ps4_erfassen(k); }
        FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); k->PS4 = (pliste){0}; k->PS4_UNKLAR = true; } ENDE_VERSUCHE;
        kern_ps4_weg_setzen(k);
    }
    {
        volatile bool sony = false;
        VERSUCHE { sony = s_befehl(k->S, "SITE UNINSTALL?", NULL).code == 200; }
        FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); sony = false; } ENDE_VERSUCHE;
        k->SONY = sony;
    }
    {   /* NEU 1.0.7 (01.11.2026): Sonys Löschdienst für Bilder/Videos und die Spielstand-Liste (nur PS5 Cleanup auf der PS5) - nur
         * gefragt, wenn solche Einträge gewählt sind (dabei wird der Löschdienst vorbereitet). Ohne sie bleiben solche Einträge. */
        bool med = false, sav = false;
        for (int i = 0; i < gewaehlt.n; i++) {
            eintrag *f = gewaehlt.e[i]; if (f->Art == ART_AUFNAHMEN) med = true; if (f->Art == ART_SPIELSTAND) sav = true;
            for (int j = 0; j < f->Pfade.n && !sav; j++) if (zugriff_spielstand_pfad(f->Pfade.e[j], NULL, NULL, NULL)) sav = true;   /* auch Spielstände in anderen Einträgen */
        }
        volatile bool m = false, s = false;
        if (!kern_bereiche_106() && (med || sav)) {
            VERSUCHE {
                if (med) m = s_befehl(k->S, "SITE MEDIUM?", NULL).code == 200;
                if (sav) s = s_befehl(k->S, "SITE SPIELSTAND?", NULL).code == 200;
            } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); } ENDE_VERSUCHE;
        }
        k->MEDIEN = m; k->SPIELSTAENDE = s;
    }
    /* Sobald ShadowMount-Kacheln, -Spiele, -Daten oder die M.2 dran sind: erst anhalten. */
    int smBetroffen = 0;
    for (int i = 0; i < gewaehlt.n; i++) {
        eintrag *f = gewaehlt.e[i];
        bool b = f->Quelle[0] || f->Ort != ORT_INTERN || f->Kachel;   /* 1.0.6: statt Bereich 7 der Ort (M.2, USB, USB-Stick) */
        for (int j = 0; j < f->Pfade.n && !b; j++) if (h_like(f->Pfade.e[j], SM_ORDNER "*") || h_gleich(f->Pfade.e[j], "/user/data")
                                                      || h_m(f->Pfade.e[j], "(?i)\\.(" ABBILD_ENDUNGEN ")$")) b = true;   /* Codex-Prüfung 28: Spiel-Abbilder (1.0.9: ABBILD_ENDUNGEN, mit .ufs) */
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
    mtab spielBleibt = mt_neu(true);     /* NEU 1.0.7: Spiele, deren Kachel Sony nicht ausgetragen hat - sie bleiben ganz, samt Update/Zusatzinhalt */
    mtab spielGeschuetzt = mt_neu(true); /* NEU 1.0.8 (Codex-Prüfung 81): Spiele, die der PS4-Schutz beim Löschen stehen ließ - nur für den Nachlauf */
    mtab kachelWeg = mt_neu(true);       /* NEU 1.0.7 (PS5-Test 01.11.2026): Kacheln, die in diesem Lauf wirklich entfernt wurden - für die Daten darunter */
    mtab sonyGruppe = mt_neu(true);      /* NEU 1.0.6: Spiele, die Sony entfernt hat -> ihre Updates/Zusatzinhalte nimmt Sony evtl. mit */
    mtab sonyLaeuft = mt_neu(true);      /* NEU 1.0.6 (Codex-Prüfung 33): Sony nach 60 s nicht fertig -> Teile dieses Spiels nicht selbst anfassen */
    volatile bool verloren = false;
    pliste tool_weg = {0};               /* NEU 1.0.8 (3.5): gelöschte Daten-Ordner von Tools - am Ende: legt das Tool sie gleich wieder an? */
    pliste bearbeitet = {0};             /* NEU 1.0.8 (Codex-Prüfung 79): Einträge, die an der Reihe waren - nach "Anhalten" räumt der Nachlauf nur bei ihnen auf */
    static const int REIHE[KAT_ANZ] = { 1, 2, 3, 4, 5, 6, 7 };   /* 1.0.7: 6 = Apps (Homebrew weiter fast zuletzt), 7 = Jailbreak ganz zuletzt */
    int nr = 0, anzahl = 0;
    for (int i = 0; i < gewaehlt.n; i++) { int kt = ((eintrag *)gewaehlt.e[i])->Kat; for (int r = 0; r < KAT_ANZ; r++) if (kt == REIHE[r]) { anzahl++; break; } }
    for (int r = 0; r < KAT_ANZ; r++) {
        for (int i = 0; i < gewaehlt.n; i++) {
            eintrag *f = gewaehlt.e[i];
            if (f->Kat != REIHE[r]) continue;
            /* NEU 1.0.8 (Arbeitsplan 3.4): auf Wunsch nach dem laufenden Eintrag anhalten - nur hier, zwischen zwei Einträgen. Ein Eintrag
             * (Sony-Deinstallation, Spielstand-Liste, Dateien) läuft immer ganz zu Ende; alle übrigen bleiben unverändert. Was zu einem schon
             * gelöschten Eintrag gehört und nach ihm käme (Update, Daten einer Tool-Kachel), bleibt als Rest stehen - wie bei einem Fehler. */
            if (k->anhalten && !verloren) {
                if (!erg.Angehalten) {
                    erg.Angehalten = true;
                    melde(k, "");
                    melde(k, nr == 0 ? T("Angehalten auf deinen Wunsch, bevor etwas gelöscht wurde. Diese Einträge bleiben unverändert:",
                                         "Stopped at your request before anything was deleted. These entries stay unchanged:")
                                     : VERB(A, T("Angehalten auf deinen Wunsch nach Eintrag ", "Stopped at your request after entry "), a_fmt(A, "%d", nr),
                                            T(" von ", " of "), a_fmt(A, "%d", anzahl), T(". Diese Einträge bleiben unverändert:", ". These entries stay unchanged:")));
                }
                /* Codex-Prüfung 89: Update/Zusatzinhalt eines Spiels, das Sony schon entfernt hat - Sony nimmt sie mit dem Spiel mit. Sind sie
                 * schon weg, ist das kein "unverändert" (und ihr Platz ist frei geräumt) */
                if (f->Gruppe != NULL && !ist_teil(f, "Spiel") && (mt_hat(&sonyGruppe, f->Gruppe) || mt_hat(&sonyLaeuft, f->Gruppe))) {   /* Codex-Prüfung 93: auch nach 60 s */
                    volatile bool da = false;
                    for (int j = 0; j < f->Pfade.n && !da; j++) { VERSUCHE { da = s_existiert(k->S, f->Pfade.e[j]); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); da = true; } ENDE_VERSUCHE; }
                    if (!da) {
                        melde(k, VERB(A, k->simulation ? T("  würde gelöscht   ", "  would be deleted   ") : T("  gelöscht   ", "  deleted   "), f->Text, T("   (mit dem Spiel über Sony)", "   (with the game via Sony)")));
                        if (f->HatGroesse) { erg.Frei.Bytes += f->Bytes; erg.Frei.Dateien += f->Dateien; erg.Frei.Ordner += f->Ordner; }
                        continue;
                    }
                    erg.Rest++;
                    melde(k, VERB(A, T("  unverändert   ", "  unchanged   "), f->Text, T("   (angehalten - Sony entfernt es eventuell noch mit dem Spiel)", "   (stopped - Sony may still remove it with the game)")));
                    continue;
                }
                erg.Rest++;
                melde(k, VERB(A, T("  unverändert   ", "  unchanged   "), f->Text, T("   (angehalten)", "   (stopped)")));
                continue;
            }
            nr++;
            pl_add(A, &bearbeitet, f);   /* Codex-Prüfung 79 */
            k->RM_SCHRITT[0] = 0; k->RM_CODE[0] = 0; k->RM_HINWEIS = NULL;   /* NEU 1.0.7: QR-Rückmeldung - Fehlercode je Eintrag */
            k->FREI_BISHER = erg.Frei.Bytes;   /* NEU 1.0.8 (Codex-Prüfung 76): "Bisher frei geräumt" - nur, was wirklich weg ist */
            if (k->Fortschritt) k->Fortschritt(k->fortschritt_ctx, nr, anzahl, f);
            if (verloren) { erg.Fehler++; rm_fehler(k, f); continue; }
            /* NEU 1.0.7: Update/Zusatzinhalt eines Spiels, das unverändert bleibt (Kachel nicht über Sony ausgetragen) - bleiben auch */
            if (f->Teil != NULL && strcmp(f->Teil, "Spiel") != 0 && f->Gruppe != NULL && mt_hat(&spielBleibt, f->Gruppe)) {
                melde(k, VERB(A, T("  unverändert   ", "  unchanged   "), f->Text, T("   (das Spiel bleibt - darum bleibt auch dieser Teil)", "   (the game stays - so this part stays too)")));
                sl_add(A, &k->UNVERAENDERT, f->Text); erg.Unveraendert++; continue;
            }
            /* NEU 1.0.7 (PS5-Test 01.11.2026 20:14: bei 6 Tools waren die Daten weg, die Kachel blieb): Daten unter einer Tool-Kachel,
             * die mit angehakt ist, gehen nur, wenn die Kachel in diesem Lauf wirklich entfernt wurde - sonst bleiben sie mit ihr */
            if (!kern_bereiche_106() && f->ToolKachel && f->ToolKachel->Wahl && f->ToolKachel->KachelId && !mt_hat(&kachelWeg, f->ToolKachel->KachelId)) {
                melde(k, VERB(A, T("  unverändert   ", "  unchanged   "), f->Text, T("   (die Kachel bleibt - darum bleiben auch ihre Daten)", "   (the tile stays - so its data stays too)")));
                sl_add(A, &k->UNVERAENDERT, f->Text); erg.Unveraendert++; continue;
            }
            /* Update/DLC eines Spiels, das selbst nicht gelöscht werden konnte: bleiben */
            if (f->Teil != NULL && strcmp(f->Teil, "Spiel") != 0 && f->Gruppe != NULL && mt_hat(&spielFehlt, f->Gruppe)) {
                melde(k, VERB(A, T("  übersprungen   ", "  skipped   "), f->Text, T(" (das Spiel selbst konnte nicht gelöscht werden)", " (the game itself could not be deleted)")));
                erg.Fehler++; rm_fehler(k, f); continue;
            }
            volatile bool ok = true;
            k->WAR_SCHON_WEG = 0;
            /* NEU 1.0.7: PS4-Schutz - vor dem eigentlichen Löschen jeden Pfad (und den App-Ordner der Kachel) erneut prüfen */
            if (!kern_bereiche_106()) {
                const char *pg = NULL;
                for (int j = 0; j < f->Pfade.n && !pg; j++) { const char *g; if (kern_ps4_geschuetzt(k, f->Pfade.e[j], &g)) pg = g; }
                if (!pg && f->KachelOrdner) { const char *g; if (kern_ps4_geschuetzt(k, f->KachelOrdner, &g)) pg = g; }
                if (!pg && f->KachelId && h_m(f->KachelId, "^CUSA\\d{5}$")) { const char *g; if (kern_ps4_geschuetzt(k, VERB(A, "/user/app/", f->KachelId), &g)) pg = g; }
                if (pg) {
                    melde(k, VERB(A, T("  geschützt   ", "  protected   "), f->Text, "   (", pg, ")"));
                    k->PS4_GESCHUETZT++; erg.Geschuetzt++;
                    if (ist_teil(f, "Spiel") && f->Gruppe) mt_setze(A, &spielGeschuetzt, f->Gruppe, (void *)1);   /* Codex-Prüfung 81 */
                    continue;
                }
            }
            /* NEU 1.0.7 (da_Chris86 31.10.2026 21:50, Punkt 4): hängt im Systemordner der Kachel eine Einbindung, die nicht
             * nachweislich zu einer unterstützten Anwendung gehört - der ganze Eintrag bleibt unverändert (kern_geister.c) */
            if (!kern_bereiche_106() && f->KachelId && zugriff_einbindung) {
                const char *fs = NULL, *q = NULL, *g = "";
                volatile int m = -1;
                VERSUCHE { m = zugriff_einbindung(A, f->KachelId, &fs, &q); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); m = -1; } ENDE_VERSUCHE;
                const char *eq = (f->Quelle && f->Quelle[0] && f->Quelle[0] != '?') ? f->Quelle : NULL;   /* NEU 1.0.7: Spielordner der Kachel */
                int regel = kern_geist_regel(f->KachelId, m, fs, q, eq, &g);
                if (regel == GEIST_UNVERAENDERT) {
                    if (ist_teil(f, "Spiel") && f->Gruppe) mt_setze(A, &spielBleibt, f->Gruppe, (void *)1);   /* 01.11.2026 (Prüfung): bleibt das Spiel, bleiben Update/Zusatzinhalt auch */
                    melde(k, VERB(A, T("  unverändert   ", "  unchanged   "), f->Text, "   (", g, (m == 1 && q) ? VERB(A, " - ", fs ? fs : "?", " ", q) : "", ")"));
                    sl_add(A, &k->UNVERAENDERT, f->Text); erg.Unveraendert++; if (k->RM_CODE[0]) rm_fehler(k, f);
                    continue;
                }
                if (regel == GEIST_PS5UPLOAD || regel == GEIST_EIGENES_SPIEL) melde(k, VERB(A, "  ", f->Text, ": ", g));
            }
            /* NEU 1.0.7 (da_Chris86 01.11.2026): Bilder/Videos nur über Sonys Löschdienst, Spielstände nur samt Eintrag in der
             * Spielstand-Liste - geht das nicht, bleibt der Eintrag unverändert (keine leeren Einträge, kein Datenbank-Neuaufbau) */
            volatile int sauber = 0;   /* 1 = Aufnahmen über Sony gelöscht, 2 = Spielstand-Einträge ausgetragen, 3 = Aufnahmen-Eintrag ohne Aufnahme (nur Reste) */
            if (!kern_bereiche_106() && !f->Gesperrt && (f->Art == ART_AUFNAHMEN || f->Art == ART_SPIELSTAND)) {
                const char *volatile sg = ""; volatile int sr = 0, sn = 0;
                VERSUCHE {
                    const char *g2 = "";
                    sr = f->Art == ART_AUFNAHMEN ? medien_ueber_sony(k, f, &g2, &sn) : spielstand_austragen(k, f, &g2);
                    sg = g2;
                } FANGE { k->RM_HINWEIS = NULL; if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); sr = sn > 0 ? 2 : 0; sg = ausn_text; } ENDE_VERSUCHE;   /* Prüfung 01.11.: schon gelöschte Aufnahmen = TEILWEISE */
                if (sr == 0) {
                    melde(k, VERB(A, T("  unverändert   ", "  unchanged   "), f->Text, "   (", sg, T(" - nichts gelöscht)", " - nothing deleted)")));
                    sl_add(A, &k->UNVERAENDERT, f->Text); erg.Unveraendert++; rm_fehler(k, f);
                    continue;
                }
                if (sr == 2) {
                    sliste nichts = {0};
                    teilweise_melden(k, f, &nichts, sl_join(A, &f->Pfade, ", "), VERB(A, a_fmt(A, T("Sony hat %d Aufnahme(n) gelöscht - ", "Sony deleted %d capture(s) - "), (int)sn), sg));
                    frei_teilweise(k, f, &erg);   /* NEU 1.0.8 (3.5): die gelöschten Aufnahmen zählen zum Freigeräumt */
                    erg.Teilweise++; rm_fehler(k, f);
                    continue;
                }
                sauber = f->Art == ART_AUFNAHMEN ? (sn > 0 ? 1 : 3) : 2;
            }
            /* NEU 1.0.6 (da_Chris86 28.10.2026): Kachel über Sony. Bei "Spiel + Kachel" (ShadowMount) steht die Spieldatei vorne
             * in den Pfaden, wird seit 1.0.7 aber ZULETZT gelöscht - erst die Kachel über Sony (da_Chris86 01.11.2026, siehe unten).
             * Bei "nur Kachel" bleibt die Spieldatei. Lehnt Sony ab: Dateien wie bisher löschen - der Eintrag gilt dann
             * nicht als ganz entfernt (die Kachel bleibt im Menü). */
            volatile bool sonyOffen = k->SONY && f->KachelId && f->KachelOrdner && !f->Gesperrt;
            volatile bool sonyWeg = false, sonyTeil = false, mitSony = false, sonyHaengt = false, sonyUnveraendert = false, resteOhneKachel = false;
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
                FANGE { k->RM_HINWEIS = NULL; if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); r = 0; g = ausn_text; } ENDE_VERSUCHE;
                if (r == 3) {   /* NEU 1.0.7: Einbindung unklar - nichts anfassen */
                    if (ist_teil(f, "Spiel") && f->Gruppe) mt_setze(A, &spielBleibt, f->Gruppe, (void *)1);   /* 01.11.2026 (Prüfung): bleibt das Spiel, bleiben Update/Zusatzinhalt auch */
                    melde(k, VERB(A, T("  unverändert   ", "  unchanged   "), f->Text, "   (", g, ")"));
                    sl_add(A, &k->UNVERAENDERT, f->Text); erg.Unveraendert++; if (k->RM_CODE[0]) rm_fehler(k, f);
                    continue;
                }
                if (r == 1) { sonyWeg = true; mitSony = true; }
                else if (r == 4) resteOhneKachel = true;   /* NEU 1.0.7: keine Kachel im Menü - die Reste löscht der Ablauf selbst */
                else if (!kern_bereiche_106()) { kachel_bleibt(k, f, g, &spielBleibt); erg.Unveraendert++; rm_fehler(k, f); continue; }   /* NEU 1.0.7: keine Datei ohne ausgetragene Kachel */
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
                if (f->Quelle[0] && strcmp(p, f->Quelle) == 0 && kern_bereiche_106()) continue;   /* 01.11.2026 (Prüfung): die Spieldatei geht jetzt NACH Sony - darum auch sie VOR Sony prüfen */
                volatile bool frei = false;
                VERSUCHE { frei = schutz_vorab(k, p); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); frei = false; } ENDE_VERSUCHE;
                if (!frei) sonyOffen = false;
            }
            if (f->Gruppe != NULL && !ist_teil(f, "Spiel") && mt_hat(&sonyLaeuft, f->Gruppe)) {
                sliste nichts = {0};
                teilweise_melden(k, f, &nichts, sl_join(A, &f->Pfade, ", "), T("Sony löscht das Spiel noch im Hintergrund - später nochmal nachschauen", "Sony is still deleting the game in the background - scan again later"));
                erg.Teilweise++; rm_fehler(k, f);
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
                    erg.Teilweise++; rm_fehler(k, f);
                    continue;
                }
            }
            /* NEU 1.0.7: leere Einhängeordner in /system_ex - vorher /system_ex für diese Sitzung beschreibbar machen (PS5: nur einmal
             * pro Einschalten; zurückgestellt wird am Ende). Geht das nicht, bleibt der Eintrag mit Grund stehen. */
            if (!kern_bereiche_106() && !k->simulation && f->Pfade.n > 0 && txt_beginnt(f->Pfade.e[0], "/system_ex/")) {
                sysex_bereit(k);
                if (k->SYSEX < 0) {
                    sliste nichts = {0};
                    teilweise_melden(k, f, &nichts, sl_join(A, &f->Pfade, ", "), k->SYSEX_GRUND);
                    erg.Teilweise++; rm_fehler(k, f);
                    continue;
                }
            }
            /* NEU 1.0.7 (da_Chris86 31.10.2026 23:32): Eintrag mit Kachel im Menü, der nicht über Sony gehen kann (ShadowMount nicht
             * sicher angehalten, App-Ordner schon weg, Update/Zusatzinhalt gesperrt, Geschütztes im App-Ordner ...) - nichts löschen.
             * Bis 1.0.6 wurden die Dateien trotzdem gelöscht und die Kachel blieb als Geisterkachel. */
            if (!kern_bereiche_106() && k->SONY && f->KachelId && !f->Gesperrt && !sonyOffen && !sonyWeg && !resteOhneKachel) {
                kachel_bleibt(k, f, sonyGrund[0] ? sonyGrund : T("die Schutzprüfung lässt Sonys Deinstallation hier nicht zu", "the protection check does not allow Sony's uninstall here"), &spielBleibt);
                erg.Unveraendert++; if (k->RM_CODE[0]) rm_fehler(k, f);
                continue;
            }
            volatile bool sonyNein = false;
            volatile bool spielstandBleibt = false; const char *volatile spielstandGrund = "";   /* NEU 1.0.7: Spielstand in diesem Eintrag ließ sich nicht austragen */   /* NEU 1.0.7: Sony hat die Kachel nicht (nachweislich) ausgetragen - ab da nichts mehr löschen */
            /* NEU 1.0.7 (da_Chris86 01.11.2026): bei "Spiel + Kachel" (ShadowMount) erst die Kachel über Sony, die Spieldatei ZULETZT.
             * Lehnt Sony ab, wird so gar nichts gelöscht (bis 1.0.6: Spieldatei schon weg, Kachel blieb als Geisterkachel). Scheitert
             * danach das Löschen der Spieldatei, ist die Kachel weg und das Spiel noch da - ShadowMount legt die Kachel beim nächsten
             * Start wieder an (keine Geisterkachel). Über Sony geht es nur, wenn ShadowMount angehalten ist (siehe oben). */
            sliste reihe = f->Pfade;
            if (!kern_bereiche_106() && k->SONY && f->Quelle[0]) {   /* ohne Sonys Dienst (PC, Windows) bleibt die alte Reihenfolge */
                sliste r = {0}; int nq = 0;
                for (int j = 0; j < f->Pfade.n; j++) { if (strcmp(f->Pfade.e[j], f->Quelle) == 0) nq++; else sl_add(A, &r, f->Pfade.e[j]); }
                for (int j = 0; j < nq; j++) sl_add(A, &r, f->Quelle);
                reihe = r;
            }
            for (int j = 0; j < reihe.n; j++) {
                const char *p = reihe.e[j];
                if (sonyOffen && !(f->Quelle[0] && strcmp(p, f->Quelle) == 0)) {
                    sonyOffen = false;
                    const char *g = "";
                    volatile int r = 0;
                    VERSUCHE { r = sony_entfernen(k, f->KachelOrdner, f->KachelId, &g); }
                    FANGE { k->RM_HINWEIS = NULL; if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); r = 0; g = ausn_text; } ENDE_VERSUCHE;
                    if (r == 1) sonyWeg = true;
                    else if (r == 4) resteOhneKachel = true;   /* NEU 1.0.7: keine Kachel im Menü - die Reste löscht der Ablauf selbst */
                    else if (r == 2) { sonyHaengt = true; sonyGrund = g; break; }   /* nicht selbst weiterlöschen */
                    else if (r == 3) { sonyUnveraendert = true; sonyGrund = g; break; }   /* NEU 1.0.7: Einbindung unklar - Rest unverändert */
                    else if (!kern_bereiche_106()) { sonyNein = true; sonyGrund = g; break; }   /* NEU 1.0.7: ohne ausgetragene Kachel keine Datei mehr */
                    else { sonyTeil = true; sonyGrund = g; }
                }
                /* NEU 1.0.7 (da_Chris86 01.11.2026): Spielstand-Ordner in einem anderen Eintrag (z. B. Jailbreak-Spiel samt Spielstand) -
                 * erst aus Sonys Spielstand-Liste austragen (nach Sonys Kachel-Auftrag), sonst bliebe ein leerer Eintrag. Geht das nicht,
                 * bleiben der Spielstand und alles danach. */
                if (!kern_bereiche_106() && f->Art != ART_SPIELSTAND && zugriff_spielstand_pfad(p, NULL, NULL, NULL) && s_existiert(k->S, p)) {
                    const char *volatile sg2 = ""; volatile int sr2 = 0;
                    VERSUCHE { const char *g2 = ""; sr2 = spielstand_ordner_austragen(k, p, &g2); sg2 = g2; }
                    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); sr2 = 0; sg2 = ausn_text; } ENDE_VERSUCHE;
                    if (!sr2) { spielstandBleibt = true; spielstandGrund = sg2; break; }
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
            if (ok && spielstandBleibt) {   /* NEU 1.0.7: Spielstand ließ sich nicht austragen - er und der Rest bleiben */
                if (geloescht.n == 0 && !sonyWeg) {
                    if (ist_teil(f, "Spiel") && f->Gruppe) mt_setze(A, &spielBleibt, f->Gruppe, (void *)1);
                    melde(k, VERB(A, T("  unverändert   ", "  unchanged   "), f->Text, "   (", spielstandGrund, T(" - nichts gelöscht)", " - nothing deleted)")));
                    sl_add(A, &k->UNVERAENDERT, f->Text); erg.Unveraendert++; rm_fehler(k, f);
                    continue;
                }
                sliste rest = {0};
                for (int j = 0; j < f->Pfade.n; j++) if (!sl_enthaelt(&geloescht, f->Pfade.e[j])) sl_add(A, &rest, f->Pfade.e[j]);
                teilweise_melden(k, f, &geloescht, sl_join(A, &rest, ", "), VERB(A, T("der Spielstand bleibt: ", "the saved data stays: "), spielstandGrund));
                erg.Teilweise++; rm_fehler(k, f);
                continue;
            }
            if (ok && sonyNein) {                       /* NEU 1.0.7: Sony hat die Kachel nicht ausgetragen */
                if (geloescht.n == 0) { kachel_bleibt(k, f, sonyGrund, &spielBleibt); erg.Unveraendert++; rm_fehler(k, f); continue; }
                /* Absicherung - kommt seit 1.0.7 nicht mehr vor: Sony wird vor der ersten Datei beauftragt, die Spieldatei geht zuletzt */
                sliste rest = {0};
                for (int j = 0; j < f->Pfade.n; j++) if (!sl_enthaelt(&geloescht, f->Pfade.e[j])) sl_add(A, &rest, f->Pfade.e[j]);
                teilweise_melden(k, f, &geloescht, sl_join(A, &rest, ", "), VERB(A, sonyGrund, T(" - die Kachel und ihre Dateien bleiben", " - the tile and its files stay")));
                erg.Teilweise++; rm_fehler(k, f);
                sl_add(A, &k->GEISTER, geist_name(k, f));
                continue;
            }
            if (ok && sonyUnveraendert) {               /* NEU 1.0.7: Sony-Teil meldet unklare Einbindung - der Rest bleibt */
                if (ist_teil(f, "Spiel") && f->Gruppe) mt_setze(A, &spielBleibt, f->Gruppe, (void *)1);   /* 01.11.2026 (Prüfung): bleibt das Spiel, bleiben Update/Zusatzinhalt auch */
                sliste rest = {0};
                for (int j = 0; j < f->Pfade.n; j++) if (!sl_enthaelt(&geloescht, f->Pfade.e[j])) sl_add(A, &rest, f->Pfade.e[j]);
                melde(k, VERB(A, T("  unverändert   ", "  unchanged   "), f->Text, "   (", sonyGrund, ")"));
                if (geloescht.n) melde(k, VERB(A, T("    schon gelöscht: ", "    already deleted: "), sl_join(A, &geloescht, ", ")));
                sl_add(A, &k->UNVERAENDERT, f->Text); erg.Unveraendert++; rm_fehler(k, f);
                continue;
            }
            if (ok && sonyHaengt) {                     /* NEU 1.0.6: Sony arbeitet noch - der Rest bleibt vorerst */
                if (f->Gruppe != NULL) mt_setze(A, &sonyLaeuft, f->Gruppe, (void *)1);
                sliste rest = {0};
                for (int j = 0; j < f->Pfade.n; j++) if (!sl_enthaelt(&geloescht, f->Pfade.e[j])) sl_add(A, &rest, f->Pfade.e[j]);
                teilweise_melden(k, f, &geloescht, sl_join(A, &rest, ", "), sonyGrund);
                erg.Teilweise++; rm_fehler(k, f);
                continue;
            }
            if (ok && sonyTeil) {                       /* NEU 1.0.6: Dateien weg, Kachel bleibt */
                teilweise_melden(k, f, &geloescht, T("der Eintrag im PS5-Menü (Kachel)", "the entry in the PS5 menu (tile)"), sonyGrund);
                erg.Teilweise++; rm_fehler(k, f);
                sl_add(A, &k->GEISTER, geist_name(k, f));
                if (f->HatGroesse) { erg.Frei.Bytes += f->Bytes; erg.Frei.Dateien += f->Dateien; erg.Frei.Ordner += f->Ordner; }
                continue;
            }
            if (ok && sauber) {   /* NEU 1.0.7: Aufnahmen über Sonys Löschdienst / Spielstand samt Eintrag in der Spielstand-Liste */
                melde(k, VERB(A, k->simulation ? T("  würde gelöscht   ", "  would be deleted   ") : T("  gelöscht   ", "  deleted   "), f->Text,
                              sauber == 1 ? T("   (über Sonys Löschdienst - kein leerer Eintrag in der Mediengalerie)", "   (via Sony's delete service - no empty entry in the Media Gallery)")
                            : sauber == 3 ? T("   (keine Aufnahme mehr darin - nur Reste)", "   (no capture left in it - leftovers only)")
                                          : T("   (samt Eintrag in der Spielstand-Liste - kein leerer Eintrag)", "   (including its entry in the saved data list - no empty entry)")));
                if (f->HatGroesse) { erg.Frei.Bytes += f->Bytes; erg.Frei.Dateien += f->Dateien; erg.Frei.Ordner += f->Ordner; }
                continue;
            }
            if (ok && resteOhneKachel) {   /* NEU 1.0.7 (da_Chris86 01.11.2026): nicht im PS5-Menü - die Reste direkt gelöscht */
                if (f->KachelId) mt_setze(A, &kachelWeg, f->KachelId, (void *)1);
                melde(k, VERB(A, k->simulation ? T("  würde gelöscht   ", "  would be deleted   ") : T("  gelöscht   ", "  deleted   "), f->Text,
                              T("   (war nicht im PS5-Menü eingetragen - Reste ohne Kachel)", "   (was not listed in the PS5 menu - leftovers without a tile)")));
                if (f->HatGroesse) { erg.Frei.Bytes += f->Bytes; erg.Frei.Dateien += f->Dateien; erg.Frei.Ordner += f->Ordner; }
                sm_einhaengeordner_weg(k, f, &geloescht, true);   /* Codex-Prüfung 54 */
                if (zwischenordner_weg(k, f, &erg)) verloren = true;   /* NEU 1.0.9 */
                continue;
            }
            if (ok && (sonyWeg || (mitSony && k->WAR_SCHON_WEG >= f->Pfade.n))) {   /* NEU 1.0.6: über Sony entfernt */
                if (f->KachelId) mt_setze(A, &kachelWeg, f->KachelId, (void *)1);
                if (sonyWeg && f->Gruppe != NULL) mt_setze(A, &sonyGruppe, f->Gruppe, (void *)1);
                melde(k, VERB(A, k->simulation ? T("  würde gelöscht   ", "  would be deleted   ") : T("  gelöscht   ", "  deleted   "), f->Text,
                              sonyWeg ? T("   (Kachel über Sony entfernt)", "   (tile removed via Sony)") : T("   (mit dem Spiel über Sony)", "   (with the game via Sony)")));
                if (f->HatGroesse) { erg.Frei.Bytes += f->Bytes; erg.Frei.Dateien += f->Dateien; erg.Frei.Ordner += f->Ordner; }
                sm_einhaengeordner_weg(k, f, &geloescht, sonyWeg);   /* NEU 03.11.2026 (17:50: auch "nur Kachel") */
                if (zwischenordner_weg(k, f, &erg)) verloren = true;   /* NEU 1.0.9 */
                continue;
            }
            if (ok && k->WAR_SCHON_WEG >= f->Pfade.n) { if (f->KachelId) mt_setze(A, &kachelWeg, f->KachelId, (void *)1); melde(k, VERB(A, T("  war schon weg   ", "  already gone   "), f->Text)); if (zwischenordner_weg(k, f, &erg)) verloren = true; continue; }   /* 1.0.9: zwischenordner_weg */
            if (ok) {
                melde(k, VERB(A, k->simulation ? T("  würde gelöscht   ", "  would be deleted   ") : T("  gelöscht   ", "  deleted   "), f->Text));
                if (f->Art == ART_TOOL_DATEN || f->Art == ART_TOOL_DATEN_WICHTIG) pl_add(A, &tool_weg, f);   /* NEU 1.0.8 (3.5) */
                if (f->KachelId) mt_setze(A, &kachelWeg, f->KachelId, (void *)1);
                sm_einhaengeordner_weg(k, f, &geloescht, false);   /* NEU 03.11.2026 */
                if (zwischenordner_weg(k, f, &erg)) verloren = true;   /* NEU 1.0.9 */
                if (f->HatGroesse) { erg.Frei.Bytes += f->Bytes; erg.Frei.Dateien += f->Dateien; erg.Frei.Ordner += f->Ordner; }
                /* NEU 1.0.6 (PS5-Test da_Chris86 29.10.2026 19:54): die Mediengalerie führt eine eigene Liste - gelöschte Bilder und
                 * Videos stehen dort als leere Einträge, bis die Datenbank neu aufgebaut ist. Darum am Ende der Hinweis (mit Sony) */
                if (k->SONY && kern_bereiche_106()) {   /* NEU 1.0.7: nur noch in der 1.0.6-Einteilung - sonst gehen Aufnahmen und Spielstände nur über Sony bzw. die Spielstand-Liste. Genauso die Spielstand-Liste in den Einstellungen (wie bis 1.0.5: dort räumte der Neuaufbau beides auf) */
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
                melde(k, VERB(A, T("  FEHLER      ", "  ERROR      "), f->Text, T(" (konnte nicht ganz gelöscht werden)", " (could not be deleted completely)"))); erg.Fehler++; rm_fehler(k, f);
                /* NEU 1.0.8 (Arbeitsplan 3.5): was davon doch gelöscht ist, zählt zum Freigeräumt; bei einem Tool der Hinweis, dass es laufen könnte */
                long long teil = frei_teilweise(k, f, &erg);
                if (teil > 0) melde(k, VERB(A, T("    davon gelöscht: ", "    of which deleted: "), kern_groesse_text(A, teil)));
                if (tool_eintrag(f)) melde(k, T("    Läuft dieses Tool gerade? Dann zuerst beenden - am einfachsten die PS5 neu starten und das Tool nicht laden - und danach nochmal löschen.",
                                                "    Is this tool running right now? Then stop it first - easiest: restart the PS5 without loading the tool - and delete again afterwards."));
                if (sauber == 2) melde(k, T("    (die Einträge in der Spielstand-Liste sind schon ausgetragen - die übrigen Dateien zeigt die PS5 nirgends an)",
                                            "    (the entries in the saved data list are already removed - the PS5 does not show the remaining files anywhere)"));
                if (ist_teil(f, "Spiel")) mt_setze(A, &spielFehlt, f->Gruppe, (void *)1);
            }
        }
    }
    /* NEU 1.0.8 (Arbeitsplan 3.5, Hinweis "Tool läuft gerade"): ist ein gelöschter Daten-Ordner eines Tools kurz darauf wieder da, läuft das
     * Tool vermutlich und legt ihn neu an - dann sagen, was zu tun ist (nur ein Hinweis, nichts wird nochmal gelöscht) */
    if (!verloren && tool_weg.n > 0 && !k->simulation) {
        warte(k, 1500);
        for (int i = 0; i < tool_weg.n; i++) {
            eintrag *f = tool_weg.e[i];
            volatile bool wieder = false;
            for (int j = 0; j < f->Pfade.n && !wieder; j++) {
                VERSUCHE { wieder = s_existiert(k->S, f->Pfade.e[j]); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); } ENDE_VERSUCHE;
            }
            if (wieder) melde(k, VERB(A, T("  HINWEIS     ", "  NOTE        "), f->Text,
                                     T(" ist gleich wieder da - das Tool läuft vermutlich gerade und legt seinen Ordner neu an. Beende es (am einfachsten: PS5 neu starten und das Tool nicht laden) und lösche dann nochmal.",
                                       " is back right away - the tool is probably running and recreates its folder. Stop it (easiest: restart the PS5 without loading the tool) and delete again.")));
        }
    }
    if (verloren)
        melde(k, T("FEHLER: Die Verbindung zur PS5 ist abgerissen - der Rest wurde nicht gelöscht. Bitte neu verbinden.", "ERROR: The connection to the PS5 dropped - the rest was not deleted. Please connect again."));
    else {
        /* Codex-Prüfung 79: nach "Anhalten" nur für Einträge, die an der Reihe waren - die übrigen bleiben ganz unverändert, auch ein
         * leerer Ordner ihres Spiels (z. B. /user/addcont/<Spiel>). Ohne Anhalten wie bisher alle gewählten. */
        const pliste *nachlauf = erg.Angehalten ? &bearbeitet : &gewaehlt;
        /* Ordner, die nur wegen ShadowMount-Spielen einzeln gelistet waren und
         * jetzt leer sind, auch entfernen (innerste zuerst). */
        for (int i = k->AUFGEKLAPPT.n - 1; i >= 0; i--) {
            const char *o = k->AUFGEKLAPPT.e[i];
            const char *oz = VERB(A, o, "/");
            bool betroffen = false;
            for (int g = 0; g < nachlauf->n && !betroffen; g++) { eintrag *f = nachlauf->e[g]; for (int j = 0; j < f->Pfade.n; j++) if (txt_beginnt(f->Pfade.e[j], oz)) { betroffen = true; break; } }
            if (betroffen) leeren_ordner_entfernen(k, o);
        }
        /* Ordner von Spielen, deren Teile gelöscht wurden und die jetzt leer sind */
        for (int g = 0; g < k->LEER_DANACH.n; g++) {
            const char *gr = k->LEER_DANACH.e[g].k;
            bool dabei = false;
            for (int i = 0; i < nachlauf->n; i++) { eintrag *f = nachlauf->e[i]; if (f->Gruppe != NULL && h_gleich(f->Gruppe, gr)) { dabei = true; break; } }
            if (!dabei) continue;
            /* Codex-Prüfungen 80/81: das Spiel bleibt (Sony hat abgelehnt, unklare Einbindung, PS4-Schutz) oder ließ sich nicht löschen -
             * dann bleiben auch seine Ordner */
            if (mt_hat(&spielBleibt, gr) || mt_hat(&spielFehlt, gr) || mt_hat(&spielGeschuetzt, gr)) continue;
            sliste *os = k->LEER_DANACH.e[g].v;
            for (int i = 0; i < os->n; i++) leeren_ordner_entfernen(k, os->e[i]);
        }
    }
    erg.SmText = sm_schlusstext(k, erg.Angehalten ? &bearbeitet : &gewaehlt);   /* Codex-Prüfung 89: nach "Anhalten" nur, was an der Reihe war */
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
    else if (!kern_bereiche_106()) {   /* NEU 1.0.7: keine eigene Geisterkachel - Dateien der Kachel bleiben */
        melde(k, VERB(A, T("  BLEIBT      PS5 Cleanup: Kachel - Sony hat sie nicht entfernt (", "  KEPT        PS5 Cleanup: tile - Sony did not remove it ("), g,
                      T(") - die Kachel bleibt samt Dateien, PS5 Cleanup bleibt startbar", ") - the tile stays with its files, PS5 Cleanup stays startable")));
        (*fehler)++;
        sl_add(A, nicht_anfassen, p); sl_add(A, nicht_anfassen, VERB(A, "/user/appmeta/", id)); sl_add(A, nicht_anfassen, VERB(A, SYS_META "/", id));
    } else {
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
/* NEU 03.11.2026 (da_Chris86 17:50/17:51, PS5-Test: nach der vollständigen Bereinigung lagen in /data/PS5-Cleanup-Protokolle noch die
 * Protokolle früherer Läufe): bei der vollständigen Bereinigung gehen die alten Protokolle von PS5 Cleanup auf der internen SSD mit - nie
 * die auf dem USB-Laufwerk, nie das Protokoll dieses Laufs. Nur Dateien mit dem eigenen Namen (ps5cleanup_protokoll_JJJJ-MM-TT_hhmmss.txt),
 * keine Verknüpfungen; der Ordner nur, wenn er danach leer ist. Im ersten Fenster steht es als eigene Zeile. */
/* Codex-Prüfung 65: die App schreibt nach /data/..., die Suche sieht /user/data/... - beides ist derselbe Ort */
static bool gleiches_protokoll(const char *a, const char *p)
{
    if (!a || !a[0]) return false;
    if (!strncmp(a, "/data/", 6)) return !strncmp(p, "/user", 5) && !strcmp(a, p + 5);
    return !strcmp(a, p);
}
bool kern_altes_protokoll(kern *k, const char *p)
{
    if (!p || !h_m(p, "(?-i)^/user/data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_\\d{4}-\\d{2}-\\d{2}_\\d{6}\\.txt$")) return false;
    return !gleiches_protokoll(k->PROTOKOLL_JETZT, p) && !gleiches_protokoll(k->EIGENES_PROTOKOLL, p);   /* beide offenen Protokolle bleiben */
}
static sliste alte_protokolle_liste(kern *k)
{
    sliste l = {0};
    dliste *dl = NULL;
    VERSUCHE { dl = s_liste(k->S, "/user/data/PS5-Cleanup-Protokolle", false); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); dl = NULL; } ENDE_VERSUCHE;
    for (int i = 0; dl && i < dl->n; i++) {
        if (dl->e[i].ist_ordner || dl->e[i].ist_link) continue;
        const char *p = VERB(A, "/user/data/PS5-Cleanup-Protokolle/", dl->e[i].name);
        if (kern_altes_protokoll(k, p)) sl_add(A, &l, p);
    }
    return l;
}
int kern_alte_protokolle(kern *k) { if (kern_bereiche_106()) return 0; return alte_protokolle_liste(k).n; }
int kern_alte_protokolle_weg(kern *k)
{
    if (kern_bereiche_106()) return 0;
    sliste l = alte_protokolle_liste(k);
    if (l.n == 0) return 0;
    volatile int weg = 0, fehler = 0;   /* Codex-Prüfung 65: in VERSUCHE verändert */
    k->PROTOKOLLE_FREI = true;
    VERSUCHE {
        for (int i = 0; i < l.n; i++) { if (k->simulation) weg++; else if (loeschen(k, l.e[i]) && !s_existiert(k->S, l.e[i])) weg++; else fehler++; }
        if (!k->simulation && fehler == 0) { const char *o = "/user/data/PS5-Cleanup-Protokolle"; dliste *r = s_liste(k->S, o, false); if (r && r->n == 0) leeren_ordner_loeschen(k, o); }
    } FANGE { k->PROTOKOLLE_FREI = false; if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); fehler++; } ENDE_VERSUCHE;
    k->PROTOKOLLE_FREI = false;
    if (weg > 0) melde(k, VERB(A, k->simulation ? T("  würde gelöscht   ", "  would be deleted   ") : T("  gelöscht   ", "  deleted   "),
                             T("alte Protokolle von PS5 Cleanup auf der Konsole (", "old PS5 Cleanup logs on the console ("), a_fmt(A, "%d", weg), ")"));
    if (fehler > 0) melde(k, VERB(A, T("  BLEIBT      alte Protokolle von PS5 Cleanup auf der Konsole: ", "  KEPT        old PS5 Cleanup logs on the console: "), a_fmt(A, "%d", fehler)));
    return fehler;
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
            g = kern_bereiche_106() ? T("es gibt noch eine Kopie auf M.2/USB", "there is another copy on M.2/USB")   /* Codex-Prüfung 32 */
                                    : T("es gibt noch eine Kopie auf der M.2 oder dem erweiterten Speicher", "there is another copy on the M.2 or the extended storage");
        else if (!frei)
            g = T("die Schutzprüfung hat etwas gefunden - nicht über Sony", "the protection check found something - not via Sony");
        if (g[0]) {
            kachel_geist = true;
            if (!kern_bereiche_106()) {   /* NEU 1.0.7: keine eigene Geisterkachel - Kachel samt Dateien bleibt, PS5 Cleanup bleibt startbar */
                melde(k, VERB(A, T("  BLEIBT      PS5 Cleanup: Kachel - ", "  KEPT        PS5 Cleanup: tile - "), g, T(" - die Kachel bleibt samt Dateien, PS5 Cleanup bleibt startbar", " - the tile stays with its files, PS5 Cleanup stays startable")));
                sl_add(A, &nicht_anfassen, p); sl_add(A, &nicht_anfassen, VERB(A, "/user/appmeta/", m.gr[1])); sl_add(A, &nicht_anfassen, VERB(A, SYS_META "/", m.gr[1]));
                fehler++;
                continue;
            }
            melde(k, VERB(A, T("  PS5 Cleanup: Sony hat die eigene Kachel nicht entfernt - ", "  PS5 Cleanup: Sony did not remove its own tile - "), g, T(". Die Dateien werden gelöscht, die Kachel bleibt im Menü.", ". The files are deleted, the tile stays in the menu.")));
            sl_add(A, &k->GEISTER, "PS5 Cleanup");
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
        if (!selbst_weg_erlaubt(p, m2, usb)) { sdiag(k, VERB(A, p, " - übersprungen: liegt auf ", kern_ort_name(kern_pfad_ort(p)), " - wird nicht mitgeleert")); continue; }
        if (sl_enthaelt(&nicht_anfassen, p)) { sdiag(k, VERB(A, p, uebergabe && kachel_p ? " - übersprungen: übernimmt der Hintergrundteil (Sony)" : " - übersprungen: Kachel bleibt (nicht über Sony ausgetragen), Sony arbeitet noch oder Jailbreak-Einstieg darin")); continue; }
        if (!s_existiert(k->S, p)) { sdiag(k, VERB(A, p, " - nicht (mehr) vorhanden")); continue; }
        /* Codex-Prüfung 36 (da_Chris86 29.10.2026: Jailbreak-Einstiege nur auf ausdrücklichen Wunsch): liegt in diesem Ordner noch
         * etwas von einem Jailbreak-Einstieg, bleibt der Ordner (eigenes_loeschen) */
        eigenes_loeschen(k, p, &fehler);
    }
    /* 2b. NEU 1.0.7 (da_Chris86 31.10.2026 16:51): Beim Start einer App legt die PS5 selbst leere Arbeitsordner mit ihrer Kennung an
     *     (belegt an PSCL00002: unser Code schreibt dort nie hin, die Ordner entstehen trotzdem). Die eigenen gehen mit - nur wenn
     *     sie leer sind; ein Fehler hier hält die Selbstentfernung nicht auf (nur Diagnose). */
    if (!kern_bereiche_106() && k->SONY)
        for (int j = 0; j < 3; j++) {   /* 1.0.9: auch content_tmp/PSCL00001 (die Suche bietet beide eigenen nicht mehr an, kern_suche.c) */
            static const char *const EIGENE_ARBEIT[3] = { "/user/temp/share/PSCL00002", "/user/av_contents/content_tmp/PSCL00002", "/user/av_contents/content_tmp/PSCL00001" };
            const char *p = EIGENE_ARBEIT[j];
            if (!s_existiert(k->S, p)) { sdiag(k, VERB(A, p, " - nicht vorhanden")); continue; }
            dliste *l = s_liste(k->S, p, false);
            if (!l || l->n > 0) { sdiag(k, VERB(A, p, " - bleibt: nicht leer")); continue; }
            /* Codex-Prüfung 127 (P1): nur RMD - ist der Ordner inzwischen nicht mehr leer, bleibt er (loeschen() hätte ihn samt Inhalt entfernt) */
            bool ok = kern_erlaubt(k, p) && kern_ohne_verknuepfung(k, p) && s_befehl(k->S, "RMD", p).code < 400 && !s_existiert(k->S, p);
            if (ok) melde(k, VERB(A, k->simulation ? T("  würde gelöscht   ", "  would be deleted   ") : T("  gelöscht   ", "  deleted   "), T("PS5 Cleanup: ", "PS5 Cleanup: "), p));
            sdiag(k, VERB(A, p, ok ? " - entfernt" : " - bleibt: ließ sich nicht entfernen"));
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
                    if (kr == 0 && kern_bereiche_106()) eigenes_loeschen(k, kachel_p, &fehler);   /* 1.0.6: Dateien weg, Kachel bleibt im Menü; 1.0.7: Dateien bleiben */
                    const char *rp[2] = { VERB(A, "/user/appmeta/", kachel_id), VERB(A, SYS_META "/", kachel_id) };
                    for (int j = 0; j < 2; j++) {
                        if (sl_enthaelt(&reste, rp[j])) { sdiag(k, VERB(A, rp[j], kr == 0 ? " - bleibt: die Kachel ist nicht ausgetragen" : " - übersprungen: Sony arbeitet noch")); continue; }
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
