/* PS5 Cleanup 1.1 - Test U16: laufende Aufnahme der PS5 (content_tmp/sharevideo_<hex>_hdr.webm) wird
 * nie angeboten und nie gelöscht; andere WebM-Dateien bleiben wie bisher (wie 1.0.5).
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_u16 <hier> [schutz]      Testbaum in <hier>/w (wird bei "loeschen" verändert - nur eine Kopie benutzen)
 */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void melde_log(void *c, const char *t) { (void)c; printf("LOG: %s\n", t); }
static bool nein(void *c) { (void)c; return false; }
static bool da(const char *p) { struct stat st; return lstat(p, &st) == 0; }
#define HDR "/user/av_contents/content_tmp/sharevideo_1a2b3c4d_hdr.webm"
#define HDR2 "/user/av_contents/content_tmp/sharevideo_F6754CDB_hdr.webm"
#define SDR "/user/av_contents/content_tmp/sharevideo_1a2b3c4d.webm"
#define CLIP "/user/av_contents/content_tmp/meinclip.webm"

static kern *neu(const char *w, bool schutz)
{
    datei_optionen o = { .wurzel = w, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    kern *K = kern_neu(zugriff_datei(&o));
    K->melde = melde_log;
    if (schutz) { kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K); }
    kern_suche_funde(K);
    return K;
}
static eintrag *leere(kern *K)
{
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (strstr(f->Text, "Leere Aufnahme-Zwischenordner")) return f; }
    return NULL;
}
static bool hat(const eintrag *f, const char *p) { if (!f) return false; for (int i = 0; i < f->Pfade.n; i++) if (!strcmp(f->Pfade.e[i], p)) return true; return false; }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 2) { fprintf(stderr, "Aufruf: test_u16 <hier> [schutz]\n"); return 2; }
    bool schutz = argc >= 3 && !strcmp(argv[2], "schutz");
    arena *a = arena_neu();
    const char *w = VERB(a, argv[1], "/w");
    printf("== %s\n", schutz ? "mit Selbstschutz" : "ohne Selbstschutz");
    /* 1 Schutzregel selbst */
    kern *K0 = kern_neu(NULL);
    pruefe(!kern_erlaubt(K0, HDR) && !kern_erlaubt(K0, HDR2), "sharevideo_<hex>_hdr.webm: nie löschen (Erlaubt = nein)");
    pruefe(kern_erlaubt(K0, SDR), "sharevideo_<hex>.webm (ohne _hdr): wie bisher erlaubt");
    pruefe(kern_erlaubt(K0, CLIP), "andere WebM in content_tmp: wie bisher erlaubt");
    pruefe(kern_erlaubt(K0, "/user/av_contents/video/PPSA55555/clip.webm"), "WebM in der Mediengalerie: wie bisher erlaubt");
    pruefe(kern_erlaubt(K0, "/user/av_contents/content_tmp/PPSA12000"), "Aufnahme-Zwischenordner eines Spiels: wie bisher erlaubt");
    pruefe(kern_erlaubt(K0, "/user/av_contents/content_tmp/sharevideo_1a2b3c4d_hdr.webm.alt") , "nur genau der Name ist gesperrt (Anhang .alt erlaubt)");
    /* 2 Suche */
    kern *K = neu(w, schutz);
    eintrag *f = leere(K);
    printf("LEERE %s :: %s\n", f ? f->Text : "-", f ? sl_join(a, &f->Pfade, " | ") : "-");
    pruefe(f && !hat(f, HDR), "Suche: laufende Aufnahme wird nicht angeboten");
    pruefe(f && hat(f, SDR) && hat(f, CLIP), "Suche: andere WebM-Dateien werden wie bisher angeboten");
    bool irgendwo = false;
    for (int i = 0; i < K->Funde.n; i++) if (hat(K->Funde.e[i], HDR)) irgendwo = true;
    pruefe(!irgendwo, "Suche: in keinem Eintrag enthalten");
    /* 3 vollständige Bereinigung inkl. M.2 */
    kern_waehle_komplett(K, true, true);
    ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
    printf("ERG Fehler=%d\n", e.Fehler);
    pruefe(e.Fehler == 0, "vollständige Bereinigung ohne Fehler");
    pruefe(da(VERB(a, w, HDR)), "laufende Aufnahme steht danach noch");
    pruefe(!da(VERB(a, w, SDR)) && !da(VERB(a, w, CLIP)), "andere WebM-Dateien wurden wie bisher gelöscht");
    /* 4 absichtlich erzwungen: Sicherheitsstopp */
    kern *K2 = neu(w, schutz);
    for (int i = 0; i < K2->Funde.n; i++) ((eintrag *)K2->Funde.e[i])->Wahl = false;
    eintrag *x = a_hole(K2->a, sizeof *x);
    sl_add(K2->a, &x->Pfade, HDR); x->PfadeBasis = sl_kopie(K2->a, &x->Pfade);
    x->Kat = 4; x->Text = "erzwungen: laufende Aufnahme"; x->TextBasis = x->Text; x->Quelle = ""; x->Wahl = true;
    pl_add(K2->a, &K2->Funde, x);
    ergebnis e2 = kern_fuehre_loeschung(K2, nein, NULL);
    pruefe(e2.Fehler == 1 && da(VERB(a, w, HDR)), "erzwungen: Sicherheitsstopp, Datei steht noch");
    printf("FEHLERZAHL %d\n", fehler);
    return 0;
}
