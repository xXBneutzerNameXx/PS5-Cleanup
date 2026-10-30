/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * kern_selbstschutz.c - NEU in 1.1 (gibt es in 1.0.5 nicht):
 * PS5 Cleanup darf während einer Bereinigung weder sich selbst noch den
 * seine eigenen Dateien entfernen - egal, wo sie liegen. Ganz zum Schluss entfernt es sich selbst
 * (vollständige Bereinigung: immer; Auswahl: auf Nachfrage) - da_Chris86 28.10.2026. Der Homebrew Launcher
 * gehört nicht dazu und hat keinen Sonderschutz mehr.
 *
 * Grundsatz: Der Selbstschutz kann nur ZUSÄTZLICH sperren. Er wird vor der
 * unveränderten Schutzliste aus 1.0.5 geprüft und erlaubt nie etwas, was
 * 1.0.5 sperrt. Ist er aus (k->SCHUTZ.aktiv == false), verhält sich der Kern
 * exakt wie 1.0.5 (darauf beruhen alle Vergleiche der Schritte 1-5).
 *
 * Erkennung (zur Laufzeit, nichts fest eingetragen):
 *  - PS5 Cleanup selbst: jede ELF-Datei mit der Kennung KERN_KENNUNG - unter jedem Namen und
 *    an jedem Ort, den PS5 Cleanup löschen könnte (NEU 1.1, S2); liegt sie in <Speicher>/homebrew/<Name>,
 *    der ganze Programmordner. Dazu der Ordner, aus dem das Programm gerade gestartet wurde.
 *  - Lesefehler bei der Ermittlung: nichts raten, die Suche bricht ab (NEU 1.1, S3).
 *  - Lässt sich nicht feststellen, wo PS5 Cleanup liegt: "unsicher" - dann werden
 *    alle Homebrew-Daten (/user/data, jeder Ordner "homebrew") gesperrt statt zu raten.
 */
#define _GNU_SOURCE
#include "kern.h"
#include <stdlib.h>
#include <string.h>

/* Diese Kennung steckt in jedem Programm, das den Kern enthält - daran wird
 * eine eboot.elf als "PS5 Cleanup" erkannt. */
__attribute__((used)) const char KERN_KENNUNG[] = "PS5CLEANUP-ELF-KENNUNG:7c1e5b2a-selbstschutz";

#define A (k->a)

static void schuetze(kern *k, const char *p) { if (!h_hat(&k->SCHUTZ.pfade, p)) sl_add(A, &k->SCHUTZ.pfade, p); }
/* NEU 28.10.2026 (da_Chris86): gehört zu PS5 Cleanup selbst - während des Laufs geschützt, am Ende entfernbar */
static void eigen(kern *k, const char *p) { schuetze(k, p); if (!h_hat(&k->SCHUTZ.eigene, p)) sl_add(A, &k->SCHUTZ.eigene, p); }

void kern_selbstschutz_eigener_pfad(kern *k, const char *p)
{
    /* NEU 1.0.6 (PS5-Test 29.10.2026 23:45, Kernel-Protokoll): Über die eigene Kachel läuft die Oberfläche als App PSCL00002 -
     * ihr Arbeitsordner ist dann der Laufzeitbereich /mnt/sandbox/PSCL00002_000 (eine Sicht des Systems auf die App, kein
     * Speicherort). Als "eigener Ordner" geführt, scheiterte die Selbstentfernung daran ("SICHERHEITSSTOPP ... ist gesperrt",
     * 1 Fehler -> Kachel und Startordner blieben; ebenso in Schritt 4 um 22:53). Er wird darum nur geschützt, nie gelöscht.
     * Bekannt ist der eigene Ort trotzdem (die eigene App) - wie bisher wird darum nicht "unsicher" gesperrt. */
    if (p && (!strcmp(p, "/mnt/sandbox") || !strncmp(p, "/mnt/sandbox/", 13))) { schuetze(k, p); k->SCHUTZ.eigen_gefunden = true; return; }
    if (p && p[0] == '/') { eigen(k, p); k->SCHUTZ.eigen_gefunden = true; }
}


/* NEU 1.1 (S2): PS5 Cleanup an jedem Ort und unter jedem Namen erkennen.
 * Gesucht wird in allem, was PS5 Cleanup überhaupt löschen könnte (/user/data, M.2 und USB-Erweiterung
 * außerhalb von user/ und system_data/). Eine Datei gilt als PS5 Cleanup, wenn sie eine ELF ist
 * (erste 4 Bytes 7F 45 4C 46) und die Kennung enthält - egal wie sie heißt und wo sie liegt
 * (z. B. eboot.elf, ps5cleanup.elf, starttest.elf, Kopie im Payload Manager). Gelesen wird nur.
 * Grenzen: Tiefe 8, 300000 Einträge; wird eine Grenze erreicht -> "unsicher" (Homebrew-Daten gesperrt).
 * Ordner von Spiel-Kopien (sce_sys/ + eboot.bin) werden nicht durchsucht. */
#define SUCH_TIEFE 8
#define SUCH_GRENZE 300000
typedef struct { int eintraege; bool voll; bool zaehlt; } suchstand;   /* zaehlt: Fund gilt als "eigener Ort gefunden" */
static bool ist_elf_mit_kennung(kern *k, const char *p, bool *unlesbar)
{
    const unsigned char *d; size_t n;
    *unlesbar = false;
    if (!s_kopf(k->S, p, 4, &d, &n)) { *unlesbar = true; return false; }
    if (n < 4 || d[0] != 0x7F || d[1] != 'E' || d[2] != 'L' || d[3] != 'F') return false;
    /* 1.0.6 Fix: stückweise suchen statt die ganze Datei dauerhaft in den Speicher zu laden */
    int r = s_enthaelt(k->S, p, KERN_KENNUNG, strlen(KERN_KENNUNG));
    if (r < 0) { *unlesbar = true; return false; }
    return r == 1;
}
static bool spiel_kopie(const dliste *l)
{
    bool sce = false, eboot = false;
    for (int i = 0; i < l->n; i++) {
        if (l->e[i].ist_ordner && !l->e[i].ist_link && txt_gleich_ci(l->e[i].name, "sce_sys")) sce = true;
        if (!l->e[i].ist_ordner && txt_gleich_ci(l->e[i].name, "eboot.bin")) eboot = true;
    }
    return sce && eboot;
}
static void kopien_suchen(kern *k, const char *ordner, int tiefe, suchstand *st)
{
    if (st->voll) return;
    /* 1.0.6 Fix (30.10.2026, da_Chris86: Playden legt private Ordner an, z. B. /user/data/playden/runtime-state, die die PS5
     * niemandem zeigt): bis 1.0.6 brach EIN solcher Ordner das ganze Nachschauen ab. Jetzt: die oberste Ebene weiter streng;
     * ein nicht lesbarer Unterordner wird geschützt (nie gelöscht, auch kein Ordner darüber als Ganzes) und still übersprungen. */
    dliste *l = s_liste(k->S, ordner, tiefe == 0);   /* oberste Ebene streng: Lesefehler -> Ausnahme */
    if (!l) {
        if (tiefe > 0) { schuetze(k, ordner); if (!h_hat(&k->SCHUTZ.unlesbar, ordner)) sl_add(A, &k->SCHUTZ.unlesbar, ordner); }
        return;
    }
    /* NEU 1.1 (Codex-Prüfung 26.10.2026): Spiel-Kopie (sce_sys/ + eboot.bin) wird nicht ganz durchsucht,
     * aber ihre Dateien der obersten Ebene werden geprüft (eine dort abgelegte PS5-Cleanup-Kopie wird erkannt). */
    bool nur_dateien = tiefe > 0 && spiel_kopie(l);
    for (int i = 0; i < l->n; i++) {
        const datei *e = &l->e[i];
        if (nur_dateien && (e->ist_ordner || e->ist_link)) continue;
        if (!strcmp(e->name, ".") || !strcmp(e->name, "..")) continue;
        if (++st->eintraege > SUCH_GRENZE) { st->voll = true; return; }
        const char *p = strcmp(ordner, "/") ? VERB(A, ordner, "/", e->name) : VERB(A, "/", e->name);
        if (e->ist_link) continue;                    /* Verknüpfungen nie verfolgen */
        if (e->ist_ordner) {
            /* nur auf M.2 und USB-Speicher: deren Systemordner nicht durchsuchen (1.0.6, Codex-Prüfung 30: auf USB-Sticks schon) */
            if (tiefe == 0 && (h_gleich(e->name, "user") || h_gleich(e->name, "system_data") || h_gleich(e->name, "lost+found")) && (!strcmp(ordner, M2) || !strcmp(ordner, USB_ERW))) continue;
            /* 1.0.6 Fix (PS5-Test da_Chris86 30.10.2026 15:50, Playden: Windows-Spiele mit sehr tiefen Ordnern): bis hier wurden bei
             * erreichter Tiefe ALLE Homebrew-Daten gesperrt. Jetzt: nur dieser Ordner wird geschützt (nie gelöscht, auch kein Ordner
             * darüber als Ganzes) und still übersprungen - wie ein privater, nicht lesbarer Ordner. */
            if (tiefe + 1 >= SUCH_TIEFE) { schuetze(k, p); if (!h_hat(&k->SCHUTZ.unlesbar, p)) sl_add(A, &k->SCHUTZ.unlesbar, p); continue; }
            kopien_suchen(k, p, tiefe + 1, st);
            continue;
        }
        if (e->groesse < 64) continue;
        bool unlesbar;
        bool unser = ist_elf_mit_kennung(k, p, &unlesbar);
        if (unlesbar) { schuetze(k, p); continue; }   /* nicht prüfbar: vorsichtshalber nie anbieten */
        if (!unser) continue;
        k->SCHUTZ.kopien++; if (st->zaehlt) k->SCHUTZ.eigen_gefunden = true;   /* Zusatzorte heben die Unsicherheit nie auf */
        /* homebrew/<Name>/<Datei>: der ganze Programmordner (mit Bild usw.) gehört dazu */
        const char *o = h_eltern(A, p);
        if (h_m(o, "/homebrew/[^/]+$")) eigen(k, o); else eigen(k, p);
    }
}

static void ermitteln_innen(kern *k)
{
    /* da_Chris86 28.10.2026 13:29: Der Homebrew Launcher gehört nicht zu PS5 Cleanup (Start über ELF + eigene Kachel) -
     * kein Sonderschutz mehr, er wird wie jedes Homebrew-Programm angeboten. */
    /* PS5 Cleanup selbst - an jedem Ort, unter jedem Namen (S2) */
    /* NEU 1.1 (Codex-Prüfung 26.10.2026): auch Downloads und Zwischenspeicher durchsuchen.
     * 1.0.6 Fix (30.10.2026): die Eintragsgrenze gilt je Ort; wird sie erreicht, wird NUR dieser Ort ganz gesperrt
     * (bis hier: dieser und jeder weitere Ort, dazu alle Homebrew-Daten der PS5). */
    const char *orte[6] = { "/user/data", M2, USB_ERW, "/user/download", "/user/temp", "/user/bgft" };
    for (int w = 0; w < 6; w++) {
        suchstand st = {0};
        st.zaehlt = w < 3;                               /* wie bisher: nur diese drei Orte zählen als "gefunden" */
        kopien_suchen(k, orte[w], 0, &st);
        if (st.voll) schuetze(k, orte[w]);
    }
    /* NEU 1.0.6 (Codex-Prüfung 27): auch normale USB-Sticks /mnt/usb0..7 durchsuchen. Lässt sich ein Stick nicht ganz
     * lesen oder ist er zu groß, wird der ganze Stick gesperrt (nichts darauf angeboten) - die übrige Prüfung läuft weiter. */
    for (int u = 0; u < 8; u++) {
        const char *b = a_fmt(A, "/mnt/usb%d", u);
        suchstand su = {0};
        volatile bool kaputt = false;
        VERSUCHE { kopien_suchen(k, b, 0, &su); }
        FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); kaputt = true; } ENDE_VERSUCHE;
        if (kaputt || su.voll) schuetze(k, b);
    }
}

void kern_selbstschutz_ermitteln(kern *k)
{
    k->SCHUTZ.aktiv = true;
    k->SCHUTZ.fehler = NULL;
    VERSUCHE { ermitteln_innen(k); }
    FANGE {
        if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN();
        /* NEU 1.1 (S3): Lesefehler -> nicht raten. Die Suche bricht ab (kern_suche_funde). */
        k->SCHUTZ.fehler = VERB(A, T("Sicherheitsprüfung nicht möglich - ein Ordner ließ sich nicht lesen: ", "Safety check not possible - a folder could not be read: "), ausn_text);
    } ENDE_VERSUCHE;
    /* eigene Daten (Einstellungen, Protokolle) */
    eigen(k, "/user/data/PS5Cleanup");
    /* NEU 1.0.6: die eigene Startbildschirm-Kachel PSCL00001 (start/kachel.c) - nur, wenn unsere Markierung
     * darin liegt (sonst gehört die Kennung einer anderen App und wird ganz normal behandelt) */
    if (s_existiert(k->S, "/user/app/PSCL00001/.ps5cleanup_kachel")) {
        eigen(k, "/user/app/PSCL00001");
        eigen(k, "/user/appmeta/PSCL00001");
        eigen(k, "/system_data/priv/appmeta/PSCL00001");
    }
    k->SCHUTZ.unsicher = k->SCHUTZ.unsicher || !k->SCHUTZ.eigen_gefunden || k->SCHUTZ.fehler != NULL;
}

/* Liegt a in b (oder ist a == b)?  Pfade wie in 1.0.5 ohne Groß/klein verglichen. */
static bool in_oder_gleich(const char *a, const char *b)
{
    size_t lb = strlen(b);
    if (strlen(a) < lb) return false;
    char *kopf = strndup(a, lb);
    bool g = txt_gleich_ci(kopf, b);
    free(kopf);
    return g && (a[lb] == 0 || a[lb] == '/');
}
static bool hat_homebrew_teil(const char *p)
{
    return h_m(p, "(^|/)homebrew(/|$)");
}

/* Darf p NICHT gelöscht werden?  p selbst geschützt, in etwas Geschütztem,
 * oder ein Ordner, der etwas Geschütztes enthält. */
bool kern_selbstschutz_gesperrt(kern *k, const char *p)
{
    if (!k->SCHUTZ.aktiv || !p) return false;
    if (k->SCHUTZ.frei)        /* Selbstentfernung am Ende: eigene Pfade (und ihr Inhalt) sind frei, alles andere bleibt geschützt */
        for (int i = 0; i < k->SCHUTZ.eigene.n; i++) if (in_oder_gleich(p, k->SCHUTZ.eigene.e[i])) return false;
    for (int i = 0; i < k->SCHUTZ.pfade.n; i++) {
        const char *q = k->SCHUTZ.pfade.e[i];
        if (in_oder_gleich(p, q) || in_oder_gleich(q, p)) return true;
    }
    /* Fake-Lizenzen des Launchers */
    if (h_m(p, "^/user/license/"))
        for (int i = 0; i < k->SCHUTZ.launcher_ids.n; i++) if (h_m(p, VERB(A, h_escape(A, k->SCHUTZ.launcher_ids.e[i]), "[^/]*$"))) return true;
    if (k->SCHUTZ.unsicher) {
        if (in_oder_gleich(p, "/user/data") || in_oder_gleich("/user/data", p)) return true;
        if (hat_homebrew_teil(p)) return true;
    }
    return false;
}

bool kern_selbst_eigen(kern *k, const char *p)
{
    if (!k->SCHUTZ.aktiv || !p) return false;
    for (int i = 0; i < k->SCHUTZ.eigene.n; i++) if (in_oder_gleich(p, k->SCHUTZ.eigene.e[i])) return true;
    return false;
}

bool kern_selbstschutz_unlesbar(kern *k, const char *p)
{
    if (!k->SCHUTZ.aktiv || !p) return false;
    for (int i = 0; i < k->SCHUTZ.unlesbar.n; i++) if (in_oder_gleich(p, k->SCHUTZ.unlesbar.e[i])) return true;
    return false;
}
/* Darf p in der Liste angeboten werden?  (geschützt oder darin: nein) */
bool kern_selbstschutz_anbieten(kern *k, const char *p) { return !kern_selbstschutz_gesperrt(k, p); }

/* Liegt etwas Geschütztes echt unterhalb von p?  -> Ordner wird aufgeklappt */
bool kern_selbstschutz_enthaelt(kern *k, const char *p)
{
    if (!k->SCHUTZ.aktiv) return false;
    for (int i = 0; i < k->SCHUTZ.pfade.n; i++) {
        const char *q = k->SCHUTZ.pfade.e[i];
        if (!txt_gleich_ci(q, p) && in_oder_gleich(q, p)) return true;
    }
    return false;
}

const char *kern_selbstschutz_hinweis(kern *k)
{
    if (!k->SCHUTZ.aktiv) return "";
    if (k->SCHUTZ.unsicher)
        return T("PS5 Cleanup bleibt während der Bereinigung erhalten und wird ganz zum Schluss entfernt. Weil nicht sicher feststellbar war, wo PS5 Cleanup liegt, sind alle Homebrew-Daten gesperrt.",
                 "PS5 Cleanup stays during the cleanup and is removed at the very end. Because it could not be determined for sure where PS5 Cleanup is located, all homebrew data is locked.");
    return T("PS5 Cleanup bleibt während der Bereinigung erhalten und wird ganz zum Schluss entfernt.", "PS5 Cleanup stays during the cleanup and is removed at the very end.");
}
