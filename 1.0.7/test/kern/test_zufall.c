/* PS5 Cleanup 1.0.7 - Zufallstest-Helfer (da_Chris86 02.11.2026 19:58: "100 oder 1000 andere Leute haben andere Trophäen, Bilder,
 * Spiele und Apps"). Durchsucht EINEN erfundenen PS5-Baum und schreibt jeden Eintrag maschinenlesbar heraus; geprüft wird in
 * zufall.py gegen das, was der Erzeuger weiß.  Aufruf: test_zufall <wurzel>
 * Ausgabe: K\t<Name>\t<Wert> (Konstanten), E\t<nr>\t<Bereich>\t<KatHer>\t<Art>\t<ListeNr>\t<gesperrt>\t<Kopf-nr>\t<KachelId>\t<Zeile>, P\t<nr>\t<Pfad> */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void melde_log(void *c, const char *t) { (void)c; (void)t; }
static void sauber(const char *s) { for (; s && *s; s++) putchar(*s == '\t' || *s == '\n' ? ' ' : *s); }

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "Aufruf: test_zufall <wurzel>\n"); return 2; }
    datei_optionen o = { .wurzel = argv[1], .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    kern *K = kern_neu(zugriff_datei(&o)); K->melde = melde_log;
    arena *a = arena_neu();
    volatile int fe = 0;
    VERSUCHE { kern_suche_funde(K); } FANGE { printf("X\t%s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) return 1;
    printf("K\tART_ALTE_SICHERUNG\t%d\nK\tART_PAKET_DATEI\t%d\nK\tART_AUFNAHMEN\t%d\nK\tART_UNBEKANNT\t%d\nK\tART_TR_LISTE\t%d\nK\tART_TR_LISTE_REST\t%d\n",
           ART_ALTE_SICHERUNG, ART_PAKET_DATEI, ART_AUFNAHMEN, ART_UNBEKANNT, ART_TR_LISTE, ART_TR_LISTE_REST);
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i];
        int kopf = -1; for (int j = 0; j < K->Funde.n; j++) if (K->Funde.e[j] == f->ToolKachel) kopf = j;
        printf("E\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t", i, f->Kat, f->KatHer, f->Art, f->ListeNr, f->Gesperrt ? 1 : 0, kopf, f->KachelId && f->KachelId[0] ? f->KachelId : "-");
        sauber(kern_zeile(a, f)); printf(" | "); sauber(f->TextBasis); printf("\n");
        for (int j = 0; j < f->PfadeBasis.n; j++) { printf("P\t%d\t", i); sauber(f->PfadeBasis.e[j]); printf("\n"); }
    }
    return 0;
}
