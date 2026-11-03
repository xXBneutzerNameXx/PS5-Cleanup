/* PS5 Cleanup 1.0.7 - Test appinfo.db NUR LESEN für die Zuordnung (db_appinfo_angaben / db_appinfo_zeilen) - PS5-Test 02.11.2026
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_namen_db <hier> <echte_kopie_ordner|->
 * 1. nachgebaute appinfo.db (Text, Zahl 0, NULL, kaputter Wert)  2. fehlende appinfo.db  3. da_Chris86s Kopie vom 02.11.2026 18:25
 * (falls vorhanden): GTA V PPSA04264 -> PPSA03420 und 7 kaputte Werte bei CUSA08519, dann durch den Kern bis zur Zeile. */
#include "../../kern/kern.h"
#include "../../db/db_sicherung.h"
#include "../../db/db_pruefen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void melde_log(void *c, const char *t) { (void)c; (void)t; }
static arena *A0;
typedef struct { const char *t, *f, *w; } zeile_t;
static zeile_t Z[4000]; static int NZ = 0;
static void sammle(void *c, const char *t, const char *f, const char *w) { (void)c; if (NZ < 4000) { Z[NZ].t = a_kopie(A0, t); Z[NZ].f = a_kopie(A0, f); Z[NZ].w = a_kopie(A0, w); NZ++; } }
static int finde(const char *t, const char *f, const char *w) { int n = 0; for (int i = 0; i < NZ; i++) if (!strcmp(Z[i].t, t) && (!f || !strcmp(Z[i].f, f)) && (!w || !strcmp(Z[i].w, w))) n++; return n; }
static void nachspielen(struct kern *k, void (*z)(struct kern *k, const char *titel, const char *feld, const char *wert)) { for (int i = 0; i < NZ; i++) z(k, Z[i].t, Z[i].f, Z[i].w); }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: test_namen_db <hier> <echt|->\n"); return 2; }
    A0 = arena_neu();
    const char *g = "";
    printf("== nachgebaute appinfo.db\n");
    NZ = 0;
    int r = db_appinfo_angaben(A0, a_fmt(A0, "%s/db1", argv[1]), sammle, NULL, &g);
    pruefe(r == 5, a_fmt(A0, "5 passende Zeilen (%d, %s)", r, g));
    pruefe(finde("PPSA70001", "INSTALL_DIR_SAVEDATA", "PPSA70002") == 1, "INSTALL_DIR_SAVEDATA als Text");
    pruefe(finde("PPSA70001", "SERVICE_ID_ADDCONT_ADD_1", "EP1004-PPSA70002_00") == 1, "SERVICE_ID_ADDCONT_ADD_1 als Text");
    pruefe(finde("PPSA70001", "SERVICE_ID_ADDCONT_ADD_2", "0") == 1, "Zahl 0 kommt als \"0\"");
    pruefe(finde("PPSA70001", "SERVICE_ID_ADDCONT_ADD_3", "") == 1, "NULL kommt als \"\"");
    pruefe(finde("CUSA70003", "SERVICE_ID_ADDCONT_ADD_1", "\r\n  ") == 1, "kaputter Wert unverändert");
    pruefe(finde("PPSA70001", "TITLE", NULL) == 0, "andere Felder werden nicht gemeldet");
    printf("== fehlende appinfo.db\n");
    NZ = 0; g = "";
    r = db_appinfo_angaben(A0, a_fmt(A0, "%s/gibtsnicht", argv[1]), sammle, NULL, &g);
    pruefe(r == -1 && strstr(g, "fehlt"), a_fmt(A0, "-1 mit Grund (%d, %s)", r, g));
    if (strcmp(argv[2], "-") != 0) {
        printf("== da_Chris86s appinfo.db (Kopie 02.11.2026 18:25, nur gelesen)\n");
        NZ = 0; g = "";
        r = db_appinfo_angaben(A0, argv[2], sammle, NULL, &g);
        pruefe(r > 0, a_fmt(A0, "gelesen: %d Zeilen (%s)", r, g));
        pruefe(finde("PPSA04264", "INSTALL_DIR_SAVEDATA", "PPSA03420") == 1, "GTA V PPSA04264: INSTALL_DIR_SAVEDATA = PPSA03420");
        pruefe(finde("PPSA04264", "SERVICE_ID_ADDCONT_ADD_1", "EP1004-PPSA03420_00") == 1, "GTA V PPSA04264: SERVICE_ID_ADDCONT_ADD_1 = EP1004-PPSA03420_00");
        pruefe(finde("CUSA08519", NULL, "\r\n  ") == 7, a_fmt(A0, "Red Dead Redemption 2 CUSA08519: 7 Werte Zeilenumbruch + 2 Leerzeichen (%d)", finde("CUSA08519", NULL, "\r\n  ")));
        printf("== dieselben Zeilen durch den Kern (nachgebildete Konsole mit GTA V und RDR2)\n");
        datei_optionen o = { .wurzel = a_fmt(A0, "%s/w", argv[1]), .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
        kern *K = kern_neu(zugriff_datei(&o)); K->melde = melde_log; K->appinfo_lesen = nachspielen;
        volatile int fe = 0;
        VERSUCHE { kern_suche_funde(K); } FANGE { printf("FAIL Suche abgebrochen: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
        if (!fe) {
            eintrag *s = NULL, *rd = NULL;
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i];
                for (int j = 0; j < f->PfadeBasis.n; j++) if (!strcmp(f->PfadeBasis.e[j], "/user/home/1234abcd/savedata_prospero/PPSA03420")) s = f;
                if (f->KachelId && !strcmp(f->KachelId, "CUSA08519") && f->Teil && !strcmp(f->Teil, "Spiel")) rd = f; }
            pruefe(s && strstr(s->TextBasis, "Spielstand PPSA03420 (Grand Theft Auto V)"), a_fmt(A0, "GTA-Spielstand mit Namen: %s", s ? kern_zeile(A0, s) : "nicht gefunden"));
            pruefe(rd && rd->SidKaputt == 7 && strstr(kern_zeile(A0, rd), "ACHTUNG"), a_fmt(A0, "RDR2 mit Warnung: %s", rd ? kern_zeile(A0, rd) : "nicht gefunden"));
        }
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
