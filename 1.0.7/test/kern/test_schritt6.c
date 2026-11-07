/* PS5 Cleanup 1.1 - Tests zu Schritt 6 (Selbstschutz)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 *   test_schritt6 detail <hier> [schutz]       Fundliste wie TestKernRef "detail" (+ geschützte Pfade)
 *   test_schritt6 komplettm2 <hier> [schutz]   vollständige Bereinigung inkl. M.2 (wie Szenario komplettm2)
 *   test_schritt6 erzwingen <hier>             geschützte Pfade und ihre Elternordner absichtlich zum
 *                                              Löschen auswählen -> jeder muss am Sicherheitsstopp scheitern
 * Der Testbaum liegt in <hier>/w.
 */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static const char *B(bool b) { return b ? "True" : "False"; }
static void melde_log(void *c, const char *t) { (void)c; printf("LOG: %s\n", t); fflush(stdout); }
static bool nein(void *c) { (void)c; return false; }
static bool da(const char *p) { struct stat st; return lstat(p, &st) == 0; }

static const char *zeile(arena *a, const eintrag *f)
{
    return VERB(a, a_fmt(a, "%d", f->Kat), " | ", f->Text, " | basis=", f->TextBasis, " | gruppe=", f->Gruppe ? f->Gruppe : "-", " | teil=", f->Teil ? f->Teil : "-",
                " | gesperrt=", B(f->Gesperrt), " | paket=", B(f->Paket), " | nurleer=", B(f->NurLeer), " | kachel=", B(f->Kachel), " | smtipp=", B(f->SmTipp),
                " | quelle=", f->Quelle, " :: ", sl_join(a, &f->Pfade, " | "), " || ", sl_join(a, &f->PfadeBasis, " | "));
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: detail|komplettm2|erzwingen <hier> [schutz]\n"); return 2; }
    const char *art = argv[1], *hier = argv[2];
    bool schutz = (argc >= 4 && !strcmp(argv[3], "schutz")) || !strcmp(art, "erzwingen");
    if (getenv("SPRACHE") && !strcmp(getenv("SPRACHE"), "en")) SP_EN = true;
    arena *ta = arena_neu();
    const char *w = VERB(ta, hier, "/w");
    datei_optionen o = { .wurzel = w, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = hier };
    zugriff *z = zugriff_datei(&o);
    kern *K = kern_neu(z);
    K->melde = melde_log;
    volatile int fe = 0;
    VERSUCHE {
        if (schutz) kern_selbstschutz_ermitteln(K);
        kern_suche_funde(K);
    } FANGE { printf("FAIL Suche: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) return 1;
    arena *A = K->a;
    if (schutz) {
        printf("SCHUTZ unsicher=%s eigen=%s launcher=%s\n", B(K->SCHUTZ.unsicher), B(K->SCHUTZ.eigen_gefunden), B(K->SCHUTZ.launcher_gefunden));
        for (int i = 0; i < K->SCHUTZ.pfade.n; i++) printf("SCHUTZ %s\n", K->SCHUTZ.pfade.e[i]);
        printf("SCHUTZ Hinweis: %s\n", kern_selbstschutz_hinweis(K, !strcmp(art, "komplettm2")));   /* 1.0.8 (F1): Hinweis je Ablauf */
    }
    if (!strcmp(art, "detail")) {
        for (int i = 0; i < K->Funde.n; i++) printf("DETAIL %s\n", zeile(A, K->Funde.e[i]));
        const char *h = h_trim(A, kern_speicher_hinweis(K));
        sliste teile = r_split(A, h, "\n");
        printf("HINWEIS %s\n", sl_join(A, &teile, " / "));
        printf("ANZAHL %d\n", K->Funde.n);
    }
    else if (!strcmp(art, "komplettm2")) {
        kern_waehle_komplett(K, true, true);
        /* NEU 26.10.2026: Mit Selbstschutz gehen M.2 und Browserspeicher nie in der vollständigen Bereinigung mit.
         * Damit der Selbstschutz auf der M.2 weiter geprüft wird, wird alles davon hier einzeln angehakt (wie von Hand). */
        if (schutz) {
            int n = 0;
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (!f->Gesperrt && !f->Wahl && !f->Jailbreak && (f->Ort != ORT_INTERN || f->Paket || f->NurEinzeln)) { f->Wahl = true; n++; } }   /* 29.10.2026: Jailbreak-Einstiege nur auf ausdrücklichen Wunsch - hier nicht */
            kern_abhaengige_mitnehmen(K);
            printf("INFO einzeln dazu angehakt: %d\n", n);
        }
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        printf("ERG Fehler=%d Abgebrochen=%s\n", e.Fehler, B(e.Abgebrochen));
    }
    else if (!strcmp(art, "erzwingen")) {
        /* alle geschützten Pfade, ihre Elternordner bis zur zweiten Ebene und Dateien darin */
        sliste ziele = {0};
        for (int i = 0; i < K->SCHUTZ.pfade.n; i++) {
            const char *q = K->SCHUTZ.pfade.e[i];
            if (!da(VERB(A, w, q))) continue;
            sl_add(A, &ziele, q);
            sl_add(A, &ziele, VERB(A, q, "/eboot.elf"));
            const char *e = h_eltern(A, q);
            if (strcmp(e, "/") && strcmp(e, "/user") && strcmp(e, "/mnt") && strcmp(e, "/user/data")) sl_add(A, &ziele, e);
        }
        /* 1.0.6: der Homebrew Launcher ist nicht mehr geschützt (da_Chris86 28.10.2026) - seine Lizenz gehört nicht mehr dazu */
        if (K->SCHUTZ.unsicher) { sl_add(A, &ziele, "/user/data/homebrew/AndereApp"); sl_add(A, &ziele, "/user/data"); }
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        sliste vorhanden = {0};
        for (int i = 0; i < ziele.n; i++) if (da(VERB(A, w, ziele.e[i])) && !h_hat(&vorhanden, ziele.e[i])) sl_add(A, &vorhanden, ziele.e[i]);
        ziele = vorhanden;
        for (int i = 0; i < ziele.n; i++) {
            const char *p = ziele.e[i];
            eintrag *f = a_hole(A, sizeof *f);
            sl_add(A, &f->Pfade, p); f->PfadeBasis = sl_kopie(A, &f->Pfade);
            f->Kat = 6; f->Text = VERB(A, "erzwungen: ", p); f->TextBasis = f->Text; f->Quelle = ""; f->Wahl = true;
            pl_add(A, &K->Funde, f);
        }
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        printf("ERG Fehler=%d Abgebrochen=%s\n", e.Fehler, B(e.Abgebrochen));
        int n = 0;
        for (int i = 0; i < ziele.n; i++) {
            const char *p = ziele.e[i];
            bool steht = da(VERB(A, w, p));
            if (steht) n++;
            pruefe(steht, VERB(A, steht ? "steht noch: " : "geschützt, aber weg: ", p));
        }
        pruefe(e.Fehler == ziele.n, a_fmt(A, "jeder erzwungene Eintrag am Sicherheitsstopp gescheitert (%d von %d)", e.Fehler, ziele.n));
        printf("GESCHUETZT %d von %d\n", n, ziele.n);
    }
    z->schliessen(z);
    printf("FEHLERZAHL %d\n", fehler);
    return 0;
}
