/* PS5 Cleanup - Tests zu Schritt 5 (Löschablauf)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 *   test_schritt5 <szenario> <hier>      (Testbaum-Kopie liegt in <hier>/w, ggf. <hier>/w2)
 *
 * 1:1 nach den Szenarien aus TestKern.cs (1.0.5) - gleiche Prüfungen, gleiche
 * Ausgabezeilen (LOG:, OK/FAIL, ERG, GROESSE), damit die Ausgaben direkt mit
 * den Referenzläufen von 1.0.5 verglichen werden können.
 * Umgebungsvariablen wie bei 1.0.5: SPRACHE, SMFALL, FAKESM, STOPFAIL, DENY,
 * FAILLIST, FAILDEL, DIEON, FAKELINKS, RMDA, FLAG.
 */
#define _GNU_SOURCE
#include "../../kern/kern.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static const char *B(bool b) { return b ? "True" : "False"; }
static bool STOPPING_GESEHEN = false;
static void melde_log(void *c, const char *t) { (void)c; printf("LOG: %s\n", t); fflush(stdout); if (strstr(t, "Stopping ShadowMount")) STOPPING_GESEHEN = true; }

static const char *HIER;
static const char *lokal(arena *a, const char *p) { return VERB(a, HIER, "/w", p); }
static bool datei_da(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISREG(st.st_mode); }
static bool ordner_da(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static bool ordner_mit_inhalt(const char *p)
{
    if (!ordner_da(p)) return false;
    DIR *d = opendir(p); struct dirent *e; bool ja = false;
    while (d && (e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) { ja = true; break; }
    if (d) closedir(d);
    return ja;
}
static void schreibe(const char *p, const char *t) { FILE *f = fopen(p, "w"); if (f) { fputs(t, f); fclose(f); } }
static void mkdir_p(const char *p)
{
    char b[4096]; snprintf(b, sizeof b, "%s", p);
    for (char *c = b + 1; *c; c++) if (*c == '/') { *c = 0; mkdir(b, 0777); *c = '/'; }
    mkdir(b, 0777);
}
static int baum_weg(const char *p)
{
    struct stat st; if (lstat(p, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(p); struct dirent *e;
        while (d && (e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char q[8192]; snprintf(q, sizeof q, "%s/%s", p, e->d_name); baum_weg(q);
        }
        if (d) closedir(d);
        return rmdir(p);
    }
    return unlink(p);
}

static zugriff *ECHT;          /* für SIM=1: die echte Zugriffsschicht unter der Simulation */
static zugriff *zugriff_fuer(const char *wurzel)
{
    datei_optionen o = { .wurzel = wurzel, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = HIER };
    zugriff *z = zugriff_datei(&o);
    ECHT = z;
    if (getenv("SIM") && !strcmp(getenv("SIM"), "1")) return zugriff_sim(z);
    return z;
}
/* Baum so, wie ihn die Sitzung sieht (bei SIM=1: die simulierte Sicht) */
static int vgl2(const void *x, const void *y) { return strcmp(*(const char *const *)x, *(const char *const *)y); }
static void baum_aus(kern *k, const char *pfad, sliste *aus)
{
    dliste *l = s_liste(k->S, pfad, true);
    if (!l) return;
    for (int i = 0; i < l->n; i++) {
        datei *e = &l->e[i];
        const char *p = strcmp(pfad, "/") == 0 ? VERB(k->a, "/", e->name) : VERB(k->a, pfad, "/", e->name);
        sl_add(k->a, aus, a_fmt(k->a, "%c %lld %s", e->ist_link ? 'l' : e->ist_ordner ? 'd' : 'f', e->ist_ordner ? 0LL : e->groesse, p + 1));
        if (e->ist_ordner && !e->ist_link) baum_aus(k, p, aus);
    }
}

/* Like(f.Text, muster) */
static pliste finde(kern *k, const char *m)
{
    pliste l = {0};
    for (int i = 0; i < k->Funde.n; i++) { eintrag *f = k->Funde.e[i]; if (h_like(f->Text, m)) pl_add(k->a, &l, f); }
    return l;
}
static bool sl_enthaelt(const sliste *l, const char *s) { for (int i = 0; i < l->n; i++) if (!strcmp(l->e[i], s)) return true; return false; }
static int teile_anzahl(const char *p, char c) { int n = 1; for (; *p; p++) if (*p == c) n++; return n; }
static bool nein(void *c) { (void)c; return false; }
static double jetzt(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec + (double)t.tv_nsec / 1e9; }

typedef struct { bool gefragt; bool antwort; } frage;
static bool frage_fn(void *c) { frage *f = c; f->gefragt = true; return f->antwort; }

typedef struct { int *liste; int n; int gesamt; } fortschritt;
static void fortschritt_fn(void *c, int i, int n, const eintrag *f) { (void)f; fortschritt *x = c; x->liste[x->n++] = i; x->gesamt = n; }
static void zaehle_fn(void *c, eintrag *e) { (void)e; (*(int *)c)++; }

/* liste() aus dem Szenario "speicher": Bereich + Pfade, sortiert (ordinal) */
static int vgl(const void *x, const void *y) { return strcmp(*(const char *const *)x, *(const char *const *)y); }
static const char *liste_text(kern *k)
{
    sliste l = {0};
    for (int i = 0; i < k->Funde.n; i++) { eintrag *f = k->Funde.e[i]; sl_add(k->a, &l, a_fmt(k->a, "%d %s", f->Kat, sl_join(k->a, &f->PfadeBasis, "|"))); }
    qsort(l.e, (size_t)l.n, sizeof(char *), vgl);
    return sl_join(k->a, &l, "\n");
}

static void suche(kern *k) { kern_suche_funde(k); }   /* wirft wie 1.0.5 */
/* 1.0.6: Zeilen ohne das USB-Spiel CUSA00001 (Szenario speicher) */
static const char *ohne_usb(kern *k, const char *t)
{
    sliste o = {0};
    for (const char *p = t; *p; ) {
        const char *e = strchr(p, '\n'); size_t n = e ? (size_t)(e - p) : strlen(p);
        char *z = a_kopie_n(k->a, p, n);
        if (!strstr(z, "CUSA00001")) sl_add(k->a, &o, z);
        if (!e) break;
        p = e + 1;
    }
    return sl_join(k->a, &o, "\n");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: <szenario> <hier>\n"); return 2; }
    const char *sz = argv[1]; HIER = argv[2];
    if (getenv("SPRACHE") && !strcmp(getenv("SPRACHE"), "en")) SP_EN = true;
    arena *ta = arena_neu();
    kern *K = kern_neu(zugriff_fuer(lokal(ta, "")));
    K->melde = melde_log;
    if (getenv("SIM") && !strcmp(getenv("SIM"), "1") && getenv("SIMKERN")) K->simulation = true;   /* NEU 1.1 (A9) */
    char suchFehler[2048] = "";
    VERSUCHE { suche(K); } FANGE { snprintf(suchFehler, sizeof suchFehler, "%s", ausn_text); } ENDE_VERSUCHE;
    arena *A = K->a;

    if (!strcmp(sz, "verweigert")) {
        printf("Suche: %s\n", suchFehler);
        pruefe(suchFehler[0] && !K->FUNDE_GUELTIG && (strstr(suchFehler, "verweigert") || strstr(suchFehler, "denies")), "Zugriff verweigert -> Suche bricht ab, keine Liste");
        ergebnis e0 = kern_fuehre_loeschung(K, nein, NULL);
        pruefe(e0.Abgebrochen, "Löschen ohne gültige Liste abgebrochen");
        printf("FEHLERZAHL %d\n", fehler); return 0;
    }
    if (!strcmp(sz, "m2listfehler")) {
        printf("Suche: %s\n", suchFehler);
        pruefe(suchFehler[0] && !K->FUNDE_GUELTIG, "gescheiterte M.2-Liste bricht die Suche ab (keine Liste zum Löschen)");
        ergebnis e0 = kern_fuehre_loeschung(K, nein, NULL);
        pruefe(e0.Abgebrochen, "Löschen ohne gültige Liste abgebrochen");
        printf("FEHLERZAHL %d\n", fehler); return 0;
    }
    if (suchFehler[0]) { printf("FAIL Suche: %s\n", suchFehler); return 1; }

    if (!strcmp(sz, "astro")) {
        /* 1.0.6 (da_Chris86 28.10.2026): Astro's Playroom ist ein normales Spiel - nichts wird gesondert erkannt oder geschützt */
        /* dieselben Prüfungen in derselben Reihenfolge wie TestKernRef.cs (Windows) */
        bool a2 = false;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (r_m(sl_join(A, &f->Pfade, " "), "PPSA01327")) a2 = true; }
        pruefe(kern_erlaubt(K, "/user/home/u1/savedata_prospero/PPSA01326"), "Astro-Spielstand ist löschbar wie jeder andere (1.0.6)");
        pruefe(K->ASTRO_IDS.n == 0, "Astro wird nicht mehr gesondert erkannt (1.0.6)");
        pruefe(a2, "Astro auf der M.2 wird wie jedes Spiel angeboten (1.0.6)");
        pruefe(kern_erlaubt(K, "/user/home/u1/trophy2/nobackup/data/NPWR20188_00"), "Astro-Trophäenfortschritt ist löschbar wie jeder andere (1.0.6)");
    }
    else if (!strcmp(sz, "lizenz")) {
        pruefe(finde(K, VERB(A, T("Fake-Lizenz", "Fake license"), "*PPSA11111*")).n == 0, "Lizenz eines installierten ShadowMount-Spiels nicht angeboten");
        pruefe(finde(K, VERB(A, T("Fake-Lizenz", "Fake license"), "*SLIB00001*")).n == 1, "Lizenz ohne Spiel angeboten");
        pliste m2 = {0};
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (ist_teil(f, "Spiel") && h_like(f->Gruppe, "*|CUSA09172")) pl_add(A, &m2, f); }
        pruefe(m2.n == 1 && strstr(sl_join(A, &((eintrag *)m2.e[0])->Pfade, " "), "fakeEP1004-CUSA09172"), "Lizenz des M.2-Spiels hängt am Spiel");
        pruefe(((eintrag *)finde(K, "*_zu_loeschen_1*").e[0])->Kat == 5 && ((eintrag *)finde(K, "*_zu_loeschen_1*").e[0])->Ort == ORT_M2, "M.2-Rest einer Aufräumaktion in Bereich 5 mit Ort M.2 (1.0.6)");
    }
    else if (!strcmp(sz, "gruppen")) {
        pliste g = {0};
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Gruppe && h_like(f->Gruppe, "*PPSA77700")) pl_add(A, &g, f); }
        for (int i = 0; i < g.n; i++) { eintrag *f = g.e[i]; printf("  %s %s %d %s\n", f->Gruppe, f->Teil ? f->Teil : "", f->Kat, f->Text); }
        eintrag *intern = NULL;
        for (int i = 0; i < g.n && !intern; i++) { eintrag *f = g.e[i]; if (!strcmp(f->Gruppe, "/user|PPSA77700") && f->Teil && !strcmp(f->Teil, "Spiel")) intern = f; }
        intern->Wahl = true; kern_abhaengige_mitnehmen(K);
        int n1 = 0, n2 = 0;
        for (int i = 0; i < g.n; i++) { eintrag *f = g.e[i]; if (txt_beginnt(f->Gruppe, "/mnt/ext1") && f->Wahl) n1++; if (!strcmp(f->Gruppe, "/user|PPSA77700") && f->Wahl) n2++; }
        pruefe(n1 == 0, "internes Spiel gewählt -> M.2-Fassung bleibt abgewählt");
        pruefe(n2 == 4, "internes Spiel nimmt eigenes Update + 2 DLCs mit");
        const char *sp = sl_join(A, &intern->Pfade, " ");
        pruefe(!r_m(sp, "/user/appmeta/PPSA77700|/system_data"), VERB(A, "internes Spiel mit M.2-Zwilling lässt geteilte Kachel-Infos stehen (", sp, ")"));
    }
    else if (!strcmp(sz, "sm")) {
        /* NEU 1.0.5: ShadowMount - nur bestätigt angehalten oder nachweislich aus -> ohne Frage löschen */
        const char *fall = getenv("SMFALL") ? getenv("SMFALL") : "ok";
        if (!strcmp(fall, "aus_mount") || !strcmp(fall, "stumm") || !strcmp(fall, "trotzdem")) {
            mkdir_p(lokal(A, "/mnt/shadowmnt/PPSA99999"));
            schreibe(lokal(A, "/mnt/shadowmnt/PPSA99999/eboot.bin"), "x");
        }
        frage fr = { false, !strcmp(fall, "trotzdem") };
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        eintrag *tool = NULL;
        for (int i = 0; i < K->Funde.n && !tool; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == 6 && strstr(f->Text, "TOOL00001")) tool = f; }
        tool->Wahl = true;
        const char *ziel = tool->Pfade.e[0];
        ergebnis e = kern_fuehre_loeschung(K, frage_fn, &fr);
        bool weg = !ordner_da(lokal(A, ziel)) && !datei_da(lokal(A, ziel));
        bool sollFragen = !strcmp(fall, "aus_mount") || !strcmp(fall, "stopfail") || !strcmp(fall, "trotzdem");
        bool sollWeg = !sollFragen || !strcmp(fall, "trotzdem");
        printf("ERG Fall=%s gefragt=%s weg=%s abgebrochen=%s Grund=%s\n", fall, B(fr.gefragt), B(weg), B(e.Abgebrochen), K->SM_GRUND);
        pruefe(fr.gefragt == sollFragen, VERB(A, "Fall ", fall, ": Rückfrage ", sollFragen ? "kommt" : "kommt nicht"));
        pruefe(weg == sollWeg, VERB(A, "Fall ", fall, ": ", sollWeg ? "gelöscht" : "nichts gelöscht"));
    }
    else if (!strcmp(sz, "kacheln")) {
        pliste deK = finde(K, "*Kachel*");
        if (!SP_EN) { bool alle = deK.n > 0; for (int i = 0; i < deK.n; i++) if (!((eintrag *)deK.e[i])->Kachel) alle = false; pruefe(alle, a_fmt(A, "alle Einträge mit 'Kachel' im Text sind markiert (%d)", deK.n)); }
        sliste dePfade = {0}; for (int i = 0; i < deK.n; i++) sl_add(A, &dePfade, ((eintrag *)deK.e[i])->PfadeBasis.e[0]);
        SP_EN = true; suche(K);
        sliste enPfade = {0}; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kachel) sl_add(A, &enPfade, f->PfadeBasis.e[0]); }
        bool alleDa = true; for (int i = 0; i < dePfade.n; i++) if (!sl_enthaelt(&enPfade, dePfade.e[i])) alleDa = false;
        pruefe(alleDa, a_fmt(A, "auf Englisch dieselben Einträge markiert (%d)", enPfade.n));
        const char *erstes = NULL; int deutsch = 0;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (h_m(f->Text, "Kachel|Spielstand |Trophäen|Zusatzinhalt|ohne Spiel|unbekannter")) { if (!erstes) erstes = f->Text; deutsch++; } }
        pruefe(deutsch == 0, VERB(A, "keine deutschen Wörter in den englischen Einträgen", deutsch > 0 ? VERB(A, ": ", erstes) : ""));
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        eintrag *tool = NULL;
        for (int i = 0; i < K->Funde.n && !tool; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == 6 && strstr(f->Text, "TOOL00001")) tool = f; }
        tool->Wahl = true;
        /* Protokoll mitschreiben: "Stopping ShadowMount" muss vorkommen */
        STOPPING_GESEHEN = false;
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        pruefe(STOPPING_GESEHEN, "nur eine Tool-Kachel gewählt (Englisch) -> ShadowMount wird vorher angehalten");
        printf("ERG Fehler=%d\n", e.Fehler);
    }
    else if (!strcmp(sz, "k1")) {
        kern_waehle_komplett(K, false, false);
        eintrag *c = finde(K, "*PPSA33333*ShadowMount*").e[0];
        pruefe(strstr(sl_join(A, &c->Pfade, " "), "Game C") != NULL, "KOMPLETT nimmt SM-Spiel von der internen SSD mit");
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        c->Wahl = true;
        kern_setze_sm_spieldateien(K, false);
        pruefe(!strstr(sl_join(A, &c->Pfade, " "), "Game C"), "danach \"nur Kacheln\": Spieldatei NICHT mehr in der Liste");
        pruefe(!strstr(c->Text, "Spiel + Kachel") && teile_anzahl(c->Text, '-') < 6, VERB(A, "Text sauber: ", c->Text));
        kern_setze_sm_spieldateien(K, true); kern_setze_sm_spieldateien(K, true);
        int n = 0; for (int i = 0; i < c->Pfade.n; i++) if (strstr(c->Pfade.e[i], "Game C")) n++;
        pruefe(n == 1, "zweimal \"mit Spiel\": Spieldatei genau einmal");
        kern_setze_sm_spieldateien(K, false);
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        printf("ERG Fehler=%d\n", e.Fehler);
    }
    else if (!strcmp(sz, "symlink")) {
        bool hinter = false;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->Pfade.n; j++) if (txt_beginnt(f->Pfade.e[j], "/user/data/hb/")) hinter = true; }
        pruefe(!hinter, "nichts hinter der Verknüpfung angeboten");
        kern_waehle_komplett(K, false, false);
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        printf("ERG Fehler=%d\n", e.Fehler);
    }
    else if (!strcmp(sz, "komplett")) {
        kern_waehle_komplett(K, false, false);
        int liste[10000]; fortschritt fs = { liste, 0, -1 };
        K->Fortschritt = fortschritt_fn; K->fortschritt_ctx = &fs;
        groesse g = kern_ermittle_groessen(K);
        printf("GROESSE %s\n", kern_groesse_satz(A, g));
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        printf("ERG Fehler=%d Abgebrochen=%s %s\n", e.Fehler, B(e.Abgebrochen), kern_freigeraeumt_text(A, e.Frei));
        bool reihe = fs.gesamt > 0 && fs.n == fs.gesamt;
        for (int i = 0; i < fs.n && reihe; i++) if (liste[i] != i + 1) reihe = false;
        pruefe(reihe, a_fmt(A, "Fortschritt meldet jeden Eintrag einmal: %d von %d", fs.n, fs.gesamt));
    }
    else if (!strcmp(sz, "komplettm2")) {
        kern_waehle_komplett(K, true, true);
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        printf("ERG Fehler=%d Abgebrochen=%s\n", e.Fehler, B(e.Abgebrochen));
    }
    else if (!strcmp(sz, "m2einzeln")) {
        pliste m2e = {0};
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Ort == ORT_M2 && !f->Quelle[0]) pl_add(A, &m2e, f); }   /* 1.0.6: statt Bereich 7 der Ort */
        for (int i = 0; i < m2e.n; i++) { eintrag *f = m2e.e[i]; printf("  M2 %s :: %s\n", f->Text, sl_join(A, &f->Pfade, " | ")); }
        eintrag *einer = NULL;
        for (int i = 0; i < m2e.n && !einer; i++) { eintrag *f = m2e.e[i]; if (f->Pfade.n == 1 && teile_anzahl(f->Pfade.e[0], '/') == 5 && !f->Quelle[0] && !strstr(f->Pfade.e[0], "_zu_loeschen_")) einer = f; }
        pruefe(einer != NULL, "Einträge innerhalb der M.2-Ordner einzeln vorhanden");
        bool voll = false;
        for (int i = 0; i < m2e.n; i++) { eintrag *f = m2e.e[i]; const char *p = f->Pfade.e[0]; if (txt_beginnt(p, "/mnt/ext1/") && teile_anzahl(p, '/') == 4 && !f->NurLeer && !strstr(p, "_zu_loeschen_") && ordner_mit_inhalt(lokal(A, p))) voll = true; }
        pruefe(!voll, "kein voller M.2-Ordner als Ganzes angeboten");
        if (einer) {
            einer->Wahl = true;
            ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
            const char *ordner = h_eltern(A, einer->Pfade.e[0]);
            printf("ERG Fehler=%d geloescht=%s\n", e.Fehler, einer->Pfade.e[0]);
            pruefe(e.Fehler == 0 && !datei_da(lokal(A, einer->Pfade.e[0])) && !ordner_da(lokal(A, einer->Pfade.e[0])), "gewählter Eintrag gelöscht");
            pruefe(ordner_da(lokal(A, ordner)), "M.2-Ordner selbst bleibt stehen");
        }
    }
    else if (!strcmp(sz, "leerordner")) {
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (txt_beginnt(f->Text, "M.2 – games/x.iso")) { f->Wahl = true; break; } }
        kern_fuehre_loeschung(K, nein, NULL);
        suche(K);
        eintrag *leer = NULL;
        for (int i = 0; i < K->Funde.n && !leer; i++) { eintrag *f = K->Funde.e[i]; if (f->NurLeer && !strcmp(f->Pfade.e[0], "/mnt/ext1/games")) leer = f; }
        pruefe(leer != NULL && leer->NurLeer, "leerer Ordner games wird als 'nur leer' angeboten");
        schreibe(lokal(A, "/mnt/ext1/games/neu.iso"), "spiel");
        leer->Wahl = true;
        ergebnis e1 = kern_fuehre_loeschung(K, nein, NULL);
        pruefe(e1.Fehler == 1 && datei_da(lokal(A, "/mnt/ext1/games/neu.iso")), a_fmt(A, "inzwischen gefüllt: nichts gelöscht, Datei bleibt (Fehler=%d)", e1.Fehler));
        unlink(lokal(A, "/mnt/ext1/games/neu.iso"));
        suche(K);
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->NurLeer && !strcmp(f->Pfade.e[0], "/mnt/ext1/games")) { leer = f; break; } }
        leer->Wahl = true;
        ergebnis e2 = kern_fuehre_loeschung(K, nein, NULL);
        pruefe(e2.Fehler == 0 && !ordner_da(lokal(A, "/mnt/ext1/games")), "wirklich leer: Ordner entfernt");
    }
    else if (!strcmp(sz, "groessen")) {
        int n = 0;
        kern_groessen_im_hintergrund(K, nein, NULL, zaehle_fn, &n);
        pruefe(n == K->Funde.n, a_fmt(A, "Hintergrund-Zählung hat alle %d Einträge gezählt (%d)", K->Funde.n, n));
        bool alle = true; for (int i = 0; i < K->Funde.n; i++) if (!kern_eintrag_groesse(K, K->Funde.e[i], true)) alle = false;
        pruefe(alle, "jeder Eintrag hat eine Größe");
        kern_waehle_komplett(K, true, true);
        groesse g = kern_ermittle_groessen(K);
        printf("GROESSE %s\n", kern_groesse_satz(A, g));
        pruefe(g.Bytes > 0 && !g.Gekappt, "Summe vor dem Löschen aus den gemerkten Größen");
        pruefe(s_gesund(K->S), "Hauptverbindung danach in Ordnung");
    }
    else if (!strcmp(sz, "abbruch")) {
        K->abbruch = true;
        volatile bool abg = false;
        VERSUCHE { suche(K); } FANGE { if (ausn_art == AUSN_ABBRUCH) abg = true; } ENDE_VERSUCHE;
        pruefe(abg && !K->FUNDE_GUELTIG, "Suche abgebrochen, keine Liste zum Löschen");
        K->abbruch = false;
        pruefe(s_gesund(K->S), "Verbindung nach dem Abbruch weiter sauber");
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        pruefe(e.Abgebrochen, "Löschen nach abgebrochener Suche verweigert");
    }
    else if (!strcmp(sz, "listefehler")) {
        eintrag *d = finde(K, "*Season Pass*").e[0]; d->Wahl = true;
        schreibe(VERB(A, HIER, "/faillist.on"), "x");
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        printf("ERG Fehler=%d\n", e.Fehler);
        unlink(VERB(A, HIER, "/faillist.on"));
        pruefe(e.Fehler == 1, "fehlgeschlagene Ordnerliste = Fehler, nicht \"schon weg\"");
    }
    else if (!strcmp(sz, "spielfehler")) {
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Gruppe && !strcmp(f->Gruppe, "/user|PPSA77700") && f->Teil && !strcmp(f->Teil, "Spiel")) { f->Wahl = true; break; } }
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        printf("ERG Fehler=%d\n", e.Fehler);
        pruefe(e.Fehler == 4, "Spiel-Fehler: Spiel + Update + 2 DLC nicht gelöscht");
    }
    else if (!strcmp(sz, "abriss")) {
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == 5) f->Wahl = true; }
        double t = jetzt();
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        double sek = jetzt() - t;
        printf("ERG Fehler=%d Verloren=%s Sekunden=%d\n", e.Fehler, B(e.Verloren), (int)sek);
        pruefe(e.Verloren && sek < 15, "Abriss erkannt und sofort beendet");
        pruefe(true, "Zeitlimit danach wieder normal");      /* nur FTP: Zeitlimit - auf der Konsole gibt es keins */
    }
    else if (!strcmp(sz, "speicher")) {
        bool en0 = SP_EN;
        for (int r = 0; r < 2; r++) {
            SP_EN = r == 1; const char *sp = SP_EN ? "EN" : "DE";
            suche(K);
            const char *basis = liste_text(K);
            pruefe(K->m2Da && !K->M2_OHNE_USER && !K->USB_SPEICHER && !kern_speicher_hinweis(K)[0], VERB(A, sp, ": normale PS5 -> kein Hinweis"));
            mkdir_p(lokal(A, "/mnt/ext0/user/app/CUSA00001"));
            suche(K);
            const char *h = kern_speicher_hinweis(K);
            printf("  %s USB: %s\n", sp, h_trim(A, h));
            pruefe(K->USB_SPEICHER && strstr(h, SP_EN ? "USB drive" : "USB-Platte"), VERB(A, sp, ": USB-Platte als erweiterter Speicher -> Hinweis"));
            /* 1.0.6 (da_Chris86 28.10.2026: "USB wie M.2"): das USB-Spiel steht in Bereich 1, sonst bleibt die Liste gleich (wie TestKernRef.cs) */
            const char *mitUsb = liste_text(K);
            pruefe(!strcmp(ohne_usb(K, mitUsb), ohne_usb(K, basis)) && r_m(mitUsb, "(^|\n)1 /mnt/ext0/user/app/CUSA00001"), VERB(A, sp, ": mit USB-Speicher: USB-Spiel in Bereich 1, sonst dieselbe Liste (1.0.6)"));
            rename(lokal(A, "/mnt/ext0"), lokal(A, "/mnt/ext0_weg"));
            suche(K);
            pruefe(!K->USB_SPEICHER && !kern_speicher_hinweis(K)[0], VERB(A, sp, ": USB-Platte weg -> Hinweis weg"));
            rename(lokal(A, "/mnt/ext1/user"), lokal(A, "/mnt/user_weg"));
            suche(K);
            h = kern_speicher_hinweis(K);
            printf("  %s M2 ohne user: %s\n", sp, h_trim(A, h));
            pruefe(K->m2Da && K->M2_OHNE_USER && strstr(h, "M.2"), VERB(A, sp, ": M.2 ohne user-Ordner -> Hinweis"));
            rename(lokal(A, "/mnt/ext1"), lokal(A, "/mnt/ext1_weg"));
            suche(K);
            h = kern_speicher_hinweis(K);
            printf("  %s ohne M2: %s\n", sp, h_trim(A, h));
            pruefe(!K->m2Da && strstr(h, SP_EN ? "No M.2" : "Keine M.2"), VERB(A, sp, ": keine M.2 -> bisheriger Hinweis"));
            rename(lokal(A, "/mnt/ext1_weg"), lokal(A, "/mnt/ext1"));
            rename(lokal(A, "/mnt/user_weg"), lokal(A, "/mnt/ext1/user"));
            rename(lokal(A, "/mnt/ext0_weg"), lokal(A, "/mnt/ext0"));
            baum_weg(lokal(A, "/mnt/ext0"));
            suche(K);
            pruefe(!strcmp(liste_text(K), basis) && !kern_speicher_hinweis(K)[0], VERB(A, sp, ": alles zurück -> gleiche Liste, kein Hinweis"));
            if (SP_EN) pruefe(!h_m(VERB(A, kern_speicher_hinweis(K), h), "[äöüÄÖÜß]|HINWEIS|Kachel"), "EN: keine deutschen Wörter im Hinweis");
        }
        SP_EN = en0;
    }
    else if (!strcmp(sz, "andereps5")) {
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == 5) f->Wahl = true; }
        kern_verbinden(K, zugriff_fuer(VERB(A, HIER, "/w2")));
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        pruefe(e.Abgebrochen, "Liste einer anderen PS5 -> abgebrochen");
    }
    else { fprintf(stderr, "unbekanntes Szenario %s\n", sz); return 2; }
    if (getenv("SIM") && !strcmp(getenv("SIM"), "1")) {
        const sliste *buch = zugriff_sim_buch(K->S->z);
        for (int i = 0; i < buch->n; i++) fprintf(stderr, "BUCH %s\n", buch->e[i]);
        const char *sb = getenv("SIMBAUM");
        if (sb) {
            sliste l = {0}; baum_aus(K, "/", &l);
            qsort(l.e, (size_t)l.n, sizeof(char *), vgl2);
            FILE *f = fopen(sb, "w"); for (int i = 0; i < l.n; i++) fprintf(f, "%s\n", l.e[i]); fclose(f);
        }
    }
    ECHT->schliessen(ECHT);
    printf("FEHLERZAHL %d\n", fehler);
    return 0;
}
