/* PS5 Cleanup 1.1 - Tests zu Schritt 3/4 (Suche, Auswahl)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 *   test_schritt3 detail <wurzel> [en]
 *   test_schritt3 auswahl <wurzel> [en]
 * Ausgabe im selben Format wie TestKernRef.cs ("detail" / "auswahl") von 1.0.5,
 * damit die Dateien direkt verglichen werden können.
 */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *B(bool b) { return b ? "True" : "False"; }
static const char *zeile(arena *a, const eintrag *f)
{
    return VERB(a, a_fmt(a, "%d", f->Kat), " | ", f->Text, " | basis=", f->TextBasis, " | gruppe=", f->Gruppe ? f->Gruppe : "-", " | teil=", f->Teil ? f->Teil : "-",
                " | gesperrt=", B(f->Gesperrt), " | paket=", B(f->Paket), " | nurleer=", B(f->NurLeer), " | kachel=", B(f->Kachel), " | smtipp=", B(f->SmTipp),
                " | quelle=", f->Quelle, " :: ", sl_join(a, &f->Pfade, " | "), " || ", sl_join(a, &f->PfadeBasis, " | "));
}
static void dump(kern *k, const char *titel)
{
    printf("== %s\n", titel);
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        if (f->Wahl) printf("WAHL %d | %s :: %s\n", f->Kat, f->Text, sl_join(k->a, &f->Pfade, " | "));
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "Aufruf: detail|auswahl <wurzel> [en]\n"); return 2; }
    SP_EN = argc >= 4 && !strcmp(argv[3], "en");
    datei_optionen o = { .wurzel = argv[2], .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    kern *k = kern_neu(zugriff_datei(&o));
    volatile int fehler = 0;
    VERSUCHE { kern_suche_funde(k); } FANGE { printf("FAIL Suche: %s\n", ausn_text); fehler = 1; } ENDE_VERSUCHE;
    if (fehler) return 1;
    if (!strcmp(argv[1], "detail")) {
        for (int i = 0; i < k->Funde.n; i++) printf("DETAIL %s\n", zeile(k->a, k->Funde.e[i]));
        /* SpeicherHinweis().Trim().Replace("\n", " / ") */
        const char *h = h_trim(k->a, kern_speicher_hinweis(k));
        sliste teile = r_split(k->a, h, "\n");
        printf("HINWEIS %s\n", sl_join(k->a, &teile, " / "));
        printf("ANZAHL %d\n", k->Funde.n);
    }
    else if (!strcmp(argv[1], "auswahl")) {
        kern_waehle_komplett(k, false, false); dump(k, "KOMPLETT");
        kern_waehle_komplett(k, true, true); dump(k, "KOMPLETT_M2");
        kern_waehle_komplett(k, false, false); kern_setze_sm_spieldateien(k, false); dump(k, "KOMPLETT_SM_NUR_KACHEL");
        kern_setze_sm_spieldateien(k, true); dump(k, "KOMPLETT_SM_MIT_SPIEL");
        pliste spiele = {0};
        for (int i = 0; i < k->Funde.n; i++) if (ist_teil(k->Funde.e[i], "Spiel")) pl_add(k->a, &spiele, k->Funde.e[i]);
        for (int s = 0; s < spiele.n; s++) {
            eintrag *sp = spiele.e[s];
            kern_sm_zuruecksetzen(k);
            for (int i = 0; i < k->Funde.n; i++) ((eintrag *)k->Funde.e[i])->Wahl = false;
            sp->Wahl = true; kern_abhaengige_mitnehmen(k);
            dump(k, VERB(k->a, "SPIEL ", sp->Gruppe));
        }
    }
    return 0;
}
