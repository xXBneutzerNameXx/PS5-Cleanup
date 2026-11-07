/* PS5 Cleanup 1.0.8 - Test "nach dem laufenden Eintrag anhalten" (Arbeitsplan 3.4)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_anhalten <hier> <nach>   vollständige Bereinigung über den Ablauf wie in der Oberfläche (Testbaum <hier>/w, wird wirklich gelöscht).
 *                                 <nach> > 0: während Eintrag <nach> wird "anhalten" gewählt - dieser Eintrag wird fertig, alle übrigen bleiben,
 *                                 PS5 Cleanup entfernt sich nicht, die Meldung sagt es. <nach> = 0: Gegenprobe ohne Anhalten.
 *                                 <nach> = -1 (Codex-Prüfung 75): "anhalten" erst während des LETZTEN Eintrags - alles ist gelöscht, aber
 *                                 PS5 Cleanup entfernt sich trotzdem nicht selbst, die Meldung sagt "angehalten", die Rückmeldung "abgebrochen".
 */
#include "../../kern/ablauf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static bool da(const char *p) { struct stat st; return lstat(p, &st) == 0; }
static ablauf A; static arena *A0; static int NACH = 0;
static const char *kopf_ist = ""; static bool ok_ist = true, fertig_da = false, anhalten_ok = false;
static sliste LOG;
static bool u_wahl(void *c, const char *text, const char *titel, const char *sicher, const char *gef) { (void)c; (void)text; (void)titel; (void)sicher; (void)gef; return true; }
static const char *u_text(void *c, const char *text, const char *titel) { (void)c; (void)text; (void)titel; return "LÖSCHEN"; }
static void u_log(void *c, const char *t) { (void)c; sl_add(A0, &LOG, a_kopie(A0, t ? t : "")); }
static void u_stand(void *c, const char *t, double p)
{
    (void)c; (void)p;
    const char *v = "Lösche Eintrag ";
    if (!t || strncmp(t, v, strlen(v))) return;
    int nr = atoi(t + strlen(v)); const char *von = strstr(t, " von "); int gesamt = von ? atoi(von + 5) : 0;
    if ((NACH > 0 && nr == NACH) || (NACH < 0 && gesamt > 0 && nr == gesamt)) anhalten_ok = ablauf_loeschen_anhalten(&A) || anhalten_ok;
}
static void u_fertig(void *c, const char *kopf, bool ok, const char *frei, const char *sm) { (void)c; (void)frei; (void)sm; kopf_ist = a_kopie(A0, kopf ? kopf : ""); ok_ist = ok; fertig_da = true; }
static void u_fehler(void *c, const char *t) { (void)c; printf("FEHLERMELDUNG %s\n", t); }
static void u_liste(void *c) { (void)c; }
static int log_zaehle(const char *was) { int n = 0; for (int i = 0; i < LOG.n; i++) if (strstr(LOG.e[i], was)) n++; return n; }
static bool nie(void *c) { (void)c; return false; }
static void nichts(void *c, eintrag *e) { (void)c; (void)e; }
static void suchen(void *x) { ablauf_suchen((ablauf *)x, "CFI-7021 B01Y", "12.00"); }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: test_anhalten <hier> <nach>\n"); return 2; }
    A0 = arena_neu(); NACH = atoi(argv[2]);
    const char *W = VERB(A0, argv[1], "/w");
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    kern *K = kern_neu(zugriff_datei(&o)); K->simulation = false;
    K->MODELL_TYP = kern_modell_typ(K->a, "CFI-7021 B01Y"); K->MODELL_DIGITAL = kern_modell_digital(K->a, "CFI-7021 B01Y"); K->DDD_BEKANNT = false;
    ablauf_ui ui = { u_wahl, u_text, u_log, u_stand, u_fertig, u_fehler, u_liste, NULL };
    ablauf_init(&A, K, &ui);
    K->melde = u_log; K->melde_ctx = NULL;
    A.protokoll_datei = "(Protokoll)";
    /* wie die Oberfläche (lauf_suchen): Selbstschutz, Suche, Größen - jeder Ablauf über ablauf_starte (fängt Ausnahmen) */
    kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K);
    ablauf_starte(&A, suchen, &A);
    ablauf_hintergrund_anhalten(&A); kern_groessen_im_hintergrund(K, nie, NULL, nichts, NULL);
    /* vorher festhalten: Zeile -> Pfade (nach dem Löschen sucht der Ablauf neu) */
    int n0 = K->Funde.n; const char **txt = a_hole(A0, sizeof(char *) * (size_t)(n0 + 1)), **basis = a_hole(A0, sizeof(char *) * (size_t)(n0 + 1));
    sliste *pf = a_hole(A0, sizeof(sliste) * (size_t)(n0 + 1));
    for (int i = 0; i < n0; i++) { eintrag *f = K->Funde.e[i]; txt[i] = a_kopie(A0, f->Text); basis[i] = a_kopie(A0, f->TextBasis); pf[i] = sl_kopie(A0, &f->PfadeBasis); }
    const char *eigen = VERB(A0, W, "/user/data/pldmgr/local/ps5c/ps5c.elf");
    pruefe(n0 > 5 && da(eigen) && kern_selbst_da(K, true, true), a_fmt(A0, "Testbaum: %d Einträge, eigene Kopie von PS5 Cleanup da", n0));
    ablauf_starte(&A, (void (*)(void *))ablauf_komplett, &A);
    ablauf_hintergrund_anhalten(&A);
    printf("Meldung am Ende: %s\n", kopf_ist);
    /* Codex-Prüfung 89: nach dem Anhalten darf nur noch "gelöscht ... (mit dem Spiel über Sony)" kommen (Update/Zusatzinhalt eines Spiels,
     * das Sony schon samt diesen Teilen entfernt hat) - gezählt wird "gelöscht" darum nur bis zur Zeile "Angehalten auf deinen Wunsch" */
    int stopp = LOG.n; for (int i = 0; i < LOG.n; i++) if (strstr(LOG.e[i], "Angehalten auf deinen Wunsch")) { stopp = i; break; }
    int gel = 0, sony_nach = 0, falsch_nach = 0;
    for (int i = 0; i < LOG.n; i++) if (!strncmp(LOG.e[i], "  gelöscht   ", 13)) { if (i < stopp) gel++; else if (strstr(LOG.e[i], "(mit dem Spiel über Sony)")) sony_nach++; else falsch_nach++; }
    int ang = log_zaehle("   (angehalten");
    if (NACH < 0) {
        pruefe(anhalten_ok, "Anhalten während des letzten Eintrags angenommen");
        pruefe(fertig_da && !ok_ist && !strncmp(kopf_ist, "Angehalten - es war schon der letzte Eintrag", 44), "Meldung am Ende: angehalten beim letzten Eintrag");
        pruefe(ang == 0 && log_zaehle("da lief schon der letzte Eintrag, übrig ist nichts") == 1, "Protokoll: angehalten, kein Eintrag übrig");
        pruefe(da(eigen) && log_zaehle("PS5 Cleanup bleibt, weil du angehalten hast.") == 1, "PS5 Cleanup entfernt sich auch dann nicht selbst");
        pruefe(A.rm_abgebrochen, "Rückmeldung: zählt als abgebrochen");
        pruefe(!ablauf_loeschen_anhalten(&A), "nach dem Löschen: Anhalten nicht mehr möglich");
    } else if (NACH > 0) {
        pruefe(anhalten_ok, "Anhalten während des Löschens angenommen");
        pruefe(fertig_da && !ok_ist && !strncmp(kopf_ist, "Angehalten - ", 13), "Meldung am Ende: angehalten, nicht \"fertig\"");
        pruefe(log_zaehle(a_fmt(A0, "Angehalten auf deinen Wunsch nach Eintrag %d von ", NACH)) == 1, "Protokoll: nach welchem Eintrag angehalten wurde");
        int rest = atoi(kopf_ist + 13);
        pruefe(ang > 0 && ang == rest, a_fmt(A0, "jeder übrige Eintrag steht im Protokoll als unverändert (%d, Meldung %d)", ang, rest));
        pruefe(gel <= NACH && falsch_nach == 0, a_fmt(A0, "höchstens %d Einträge gelöscht (%d) - der laufende wurde fertig, danach keiner mehr (nur mit dem Spiel über Sony: %d)", NACH, gel, sony_nach));
        /* die übrigen Einträge stehen noch ganz */
        int geprueft = 0; const char *weg = "";
        for (int i = 0; i < LOG.n; i++) {
            const char *z = LOG.e[i], *v = "  unverändert   ", *e = strstr(z, "   (angehalten");
            if (strncmp(z, v, strlen(v)) || !e) continue;
            const char *t = a_kopie_n(A0, z + strlen(v), (size_t)(e - z - strlen(v)));
            /* die Zeile beim Löschen = Text nach der Auswahl (bei ShadowMount mit Zusatz) - sonst über den Grundtext (längster passender) */
            int j0 = -1; size_t bl = 0;
            for (int j = 0; j < n0 && j0 < 0; j++) if (!strcmp(txt[j], t)) j0 = j;
            if (j0 < 0) for (int j = 0; j < n0; j++) { size_t l = strlen(basis[j]); if (l > bl && !strncmp(t, basis[j], l)) { bl = l; j0 = j; } }
            if (j0 >= 0) { geprueft++; for (int q = 0; q < pf[j0].n; q++) if (!da(VERB(A0, W, pf[j0].e[q]))) weg = pf[j0].e[q]; }
            else printf("   nicht zugeordnet: %s\n", t);
        }
        pruefe(geprueft == ang && !weg[0], a_fmt(A0, "die übrigen %d Einträge sind unverändert auf der Platte%s%s", geprueft, weg[0] ? " - FEHLT: " : "", weg));
        pruefe(da(eigen) && log_zaehle("PS5 Cleanup bleibt, weil du angehalten hast") == 1, "PS5 Cleanup entfernt sich nach dem Anhalten nicht selbst");
        pruefe(A.rm_abgebrochen, "Rückmeldung: zählt als abgebrochen");
        pruefe(!ablauf_loeschen_anhalten(&A), "nach dem Löschen: Anhalten nicht mehr möglich");
    } else {
        pruefe(fertig_da && strncmp(kopf_ist, "Angehalten", 10) && ang == 0, "Gegenprobe ohne Anhalten: normal fertig");
        pruefe(!da(eigen), "Gegenprobe: ohne Anhalten entfernt sich PS5 Cleanup am Ende selbst");
        pruefe(!A.rm_abgebrochen, "Gegenprobe: Rückmeldung nicht abgebrochen");
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
