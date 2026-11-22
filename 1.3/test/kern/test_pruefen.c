/* PS5 Cleanup - Prüflauf vor Schritt 10 (nur lesen), Gegenstück am PC
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 *   test_pruefen <wurzel> [schutz]
 * Sucht im Testbaum und gibt dieselben Zeilen aus wie die Prüf-ELF auf der PS5 (app/pruefen.c)
 * und wie TestKernRef.cs "pruefen" (1.0.5). Löscht nichts.
 */
#include "../../kern/kern.h"
#include "../../app/pruefen.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "Aufruf: test_pruefen <wurzel> [schutz]\n"); return 2; }
    setvbuf(stdout, NULL, _IOLBF, 0);
    bool schutz = argc >= 3 && !strcmp(argv[2], "schutz");
    datei_optionen o = { .wurzel = argv[1], .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    kern *k = kern_neu(zugriff_sim(zugriff_datei(&o)));
    k->simulation = true;
    volatile int fehler = 0;
    VERSUCHE {
        if (schutz) { kern_selbstschutz_eigener_pfad(k, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(k); }
        kern_suche_funde(k);
    } FANGE { printf("FAIL Suche: %s\n", ausn_text); fehler = 1; } ENDE_VERSUCHE;
    if (fehler) return 1;
    pruef_ausgabe(k, schutz ? "MIT_SELBSTSCHUTZ" : "OHNE_SELBSTSCHUTZ_WIE_105");
    return 0;
}
