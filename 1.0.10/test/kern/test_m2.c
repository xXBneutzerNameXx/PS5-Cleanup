/* PS5 Cleanup - Entscheidungen 26.10.2026 zur Sicherheitsprüfung (Punkte 6 und 7): Tests
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 *   test_m2 m2 <hier> [schutz]        M.2 und Browserspeicher wie 1.0.5 (Anforderungen 27.10.2026)
 *   test_m2 mehrfach <hier> [schutz]  Spieldatei auf der M.2, auf die zwei Kacheln zeigen -> gesperrt
 *   test_m2 m2weg <hier> [schutz]     M.2 nicht (richtig) erkannt -> Reste möglicher M.2-Spiele gesperrt
 * Der Testbaum liegt in <hier>/w (Kopie; bei "m2" und "m2weg" wird darin gelöscht).
 */
#include "../../kern/kern.h"
#include "../../kern/ablauf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void melde_log(void *c, const char *t) { (void)c; (void)t; }
static bool nein(void *c) { (void)c; return false; }
static bool da(const char *p) { struct stat st; return lstat(p, &st) == 0; }
static kern *K; static arena *A0; static const char *W;

static void neu(bool schutz)
{
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    K = kern_neu(zugriff_datei(&o)); K->melde = melde_log;
    volatile int fe = 0;
    VERSUCHE {
        if (schutz) { kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K); }
        kern_suche_funde(K);
    } FANGE { printf("FAIL Suche: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) { printf("FEHLERZAHL 1\n"); exit(0); }
}
static int zaehle(int kat, bool nurGewaehlt) { int n = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if ((kat == 0 || f->Kat == kat) && (!nurGewaehlt || f->Wahl)) n++; } return n; }
/* 1.0.6: statt Bereich 7 der Ort - Einträge mit Daten auf der M.2 (ohne ShadowMount-Kacheln) */
static int zaehle_m2(bool nurGewaehlt) { int n = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Ort == ORT_M2 && !f->Quelle[0] && (!nurGewaehlt || f->Wahl)) n++; } return n; }
static eintrag *finde(const char *teil) { for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (strstr(f->Text, teil)) return f; } return NULL; }
/* 27.10.2026: Text muss so ANFANGEN - der Kachel-Text "vermutlich verschoben nach /mnt/ext1/homebrew/<Datei>" enthaelt den Dateinamen auch */
static eintrag *finde_anfang(const char *anf) { size_t n = strlen(anf); for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (!strncmp(f->Text, anf, n)) return f; } return NULL; }
static bool hat_pfad(const eintrag *f, const char *p) { for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], p)) return true; return false; }
static void alle_ab(void) { for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false; kern_sm_zuruecksetzen(K); }

/* Inhalt eines Ordners (rekursiv) als Zeilen "Pfad Größe" - für "vorher = nachher" */
static void baum(const char *wurzel, const char *rel, sliste *l)
{
    const char *p = VERB(A0, wurzel, rel);
    DIR *d = opendir(p); if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        const char *r = VERB(A0, rel, "/", e->d_name);
        struct stat st; if (lstat(VERB(A0, wurzel, r), &st) != 0) continue;
        sl_add(A0, l, a_fmt(A0, "%s %lld", r, S_ISDIR(st.st_mode) ? 0LL : (long long)st.st_size));
        if (S_ISDIR(st.st_mode)) baum(wurzel, r, l);
    }
    closedir(d);
}
static int vgl(const void *x, const void *y) { return strcmp(*(const char *const *)x, *(const char *const *)y); }
static const char *baum_text(const char *ordner) { sliste l = {0}; baum(VERB(A0, W, ordner), "", &l); if (l.n > 1) qsort(l.e, (size_t)l.n, sizeof(char *), vgl); return sl_join(A0, &l, "\n"); }

/* Bedienablauf "vollständige Bereinigung": merkt sich die Fragen, bricht bei der Texteingabe ab */
static sliste titel; static const char *erster_text;
static bool fw(void *c, const char *text, const char *t, const char *s, const char *g) { (void)c; (void)s; (void)g; if (!erster_text) erster_text = a_kopie(A0, text); sl_add(A0, &titel, a_kopie(A0, t)); return true; }
static const char *ft(void *c, const char *text, const char *t) { (void)c; (void)text; sl_add(A0, &titel, a_kopie(A0, t)); return NULL; }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: test_m2 m2|mehrfach|m2weg <hier> [schutz]\n"); return 2; }
    const char *art = argv[1]; bool schutz = argc >= 4 && !strcmp(argv[3], "schutz");
    A0 = arena_neu(); W = VERB(A0, argv[2], "/w");
    printf("== %s %s\n", art, schutz ? "mit Selbstschutz" : "ohne Selbstschutz (wie 1.0.5)");
    if (!strcmp(art, "m2")) {
        /* Anforderungen 27.10.2026: M.2, Browserspeicher und ShadowMount-Spiele auf der M.2 wieder wie 1.0.5 -
         * mit und ohne Selbstschutz gleich (die Entscheidungen vom 26.10.2026, U18-U20, sind ersetzt). */
        neu(schutz);
        int n7 = zaehle_m2(false), e7 = 0, g7 = 0;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Ort == ORT_M2 && !f->Quelle[0]) { if (f->NurEinzeln) e7++; if (f->Gesperrt) g7++; } }
        eintrag *br = finde("Browserspeicher");
        printf("INFO M.2-Einträge %d (davon gesperrt %d), Browserspeicher %s\n", n7, g7, br ? br->Text : "-");
        pruefe(n7 > 2 && br != NULL, "Testbaum: M.2-Einträge und Browserspeicher vorhanden");
        /* da_Chris86 29.10.2026: der Browserspeicher ist ein Jailbreak-Einstieg (offline gespeicherter WebKit-Jailbreak) - nur einzeln */
        pruefe(e7 == 0 && br && br->NurEinzeln && br->Jailbreak, "nichts auf der M.2 ist \"nur einzeln\"; der Browserspeicher ist Jailbreak-Einstieg (nur einzeln, 29.10.2026)");
        /* 1 vollständige Bereinigung mit "M.2 auch leeren" */
        kern_waehle_komplett(K, true, true);
        int w7 = zaehle_m2(true); bool brw = br && br->Wahl;
        pruefe(w7 == n7 - g7 && !brw, a_fmt(A0, "vollständige Bereinigung mit \"M.2 auch leeren\": alle %d freien M.2-Einträge gewählt, der Browserspeicher nicht (Jailbreak-Einstieg)", w7));
        kern_waehle_komplett(K, false, false);
        pruefe(zaehle_m2(true) == 0 && br && !br->Wahl, "vollständige Bereinigung mit \"M.2 behalten\": nichts von der M.2, Browserspeicher nicht (Jailbreak-Einstieg)");
        /* 2 "ganzen Bereich wählen" */
        ablauf a; ablauf_ui ui = {0}; ablauf_init(&a, K, &ui);
        /* 1.0.6: kein eigener M.2-Bereich mehr - die M.2-Reste stehen in Bereich 5 und werden mit ihm gewählt */
        alle_ab(); ablauf_bereich_setzen(&a, 5, true);
        { int r = 0, rw = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == 5 && f->Ort == ORT_M2 && !f->Gesperrt) { r++; if (f->Wahl) rw++; } }
          pruefe(r > 0 && rw == r, a_fmt(A0, "\"ganzen Bereich 5 wählen\" nimmt alle %d M.2-Reste mit (1.0.6)", r)); }
        alle_ab(); ablauf_bereich_setzen(&a, 6, true);
        pruefe(br && !br->Wahl, "Bereich 6 ganz wählen nimmt den Browserspeicher NICHT mit (Jailbreak-Einstieg, 29.10.2026)");
        /* 3 ShadowMount-Spiel auf der M.2 über Bereich 1 mit "Spiele + Kacheln" */
        alle_ab();
        eintrag *sm = NULL; for (int i = 0; i < K->Funde.n && !sm; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == 1 && f->Quelle[0] && h_like(f->Quelle, M2 "/*") && !f->Gesperrt) sm = f; }
        if (sm) {
            sm->Wahl = true; kern_setze_sm_spieldateien(K, true);
            printf("INFO %s\n", sm->Text);
            pruefe(hat_pfad(sm, sm->Quelle), "Bereich 1, \"Spiele + Kacheln\": die Spieldatei auf der M.2 geht mit (wie 1.0.5)");
        } else printf("INFO kein ShadowMount-Spiel auf der M.2 im Testbaum\n");
        /* 4 Bedienablauf: Frage "M.2 auch leeren?" */
        alle_ab();
        ui.frage_wahl = fw; ui.frage_text = ft; ablauf_init(&a, K, &ui);
        ablauf_komplett(&a);
        printf("INFO Fragen: %s\n", sl_join(A0, &titel, " / "));
        pruefe(h_hat(&titel, "M.2-SSD"), "vollständige Bereinigung: Frage \"M.2 auch leeren\" wie 1.0.5");
        pruefe(erster_text && !strstr(erster_text, "nur einzeln wählbar"), "erste Frage nennt keine \"nur einzeln\"-Ausnahmen mehr");
        /* 5 wirklich löschen (Testkopie) mit "M.2 behalten": die M.2 bleibt, der Browserspeicher bleibt (seit 29.10.2026 Jailbreak-Einstieg) */
        if (schutz) {
            const char *m2v = baum_text("/mnt/ext1");
            kern_waehle_komplett(K, false, false);
            ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
            printf("ERG Fehler=%d Abgebrochen=%s\n", e.Fehler, e.Abgebrochen ? "True" : "False");
            pruefe(e.Fehler == 0, "vollständige Bereinigung ohne Fehler");
            int zl = 1; for (const char *c = m2v; *c; c++) if (*c == '\n') zl++;
            pruefe(!strcmp(m2v, baum_text("/mnt/ext1")) && strlen(m2v) > 0, a_fmt(A0, "\"M.2 behalten\": M.2 danach unverändert (%d Dateien und Ordner verglichen)", zl));
            pruefe(da(VERB(A0, W, "/user/home/1a2b3c4d/webkit/shell/local")), "Browserspeicher bleibt (Jailbreak-Einstieg, \"Behalten\", 29.10.2026)");
            pruefe(da(VERB(A0, W, "/user/home/1a2b3c4d/savedata/PPSA11036")), "Spielstand bleibt unverändert - am PC gibt es Sonys Spielstand-Liste nicht (1.0.7: keine leeren Einträge)");
        }
    }
    else if (!strcmp(art, "mehrfach")) {
        neu(schutz);
        eintrag *f = finde("homebrew/Zwei - PPSA90002.exfat  [Spiel-Abbild");
        printf("INFO %s\n", f ? f->Text : "-");
        eintrag *k2 = finde("(PPSA90012)");
        printf("INFO %s\n", k2 ? k2->Text : "-");
        pruefe(k2 && !k2->Gesperrt && (!k2->Quelle || !k2->Quelle[0]), "zweite Kachel: nur Kachel, ohne Spieldatei");
        const char *g = NULL;
        if (f) { ablauf a; ablauf_ui ui = {0}; ablauf_init(&a, K, &ui); ablauf_eintrag_gesperrt(&a, f, &g); }
        /* da_Chris86 28.10.2026 13:41: Spieldatei, auf die zwei Kacheln zeigen = eigenes Spiel-Abbild in Bereich 1, löschbar, nie an einer Kachel */
        pruefe(f && !f->Gesperrt && f->Kat == 1 && f->Ort == ORT_M2 && strstr(f->Text, "mehrere Kacheln"), a_fmt(A0, "Spieldatei mit zwei Kacheln: eigenes Spiel-Abbild auf der M.2 (%s)", f ? f->Text : "-"));
        if (schutz) {
            kern_waehle_komplett(K, true, true); kern_abhaengige_mitnehmen(K);
            pruefe(f && f->Wahl, "\"M.2 auch leeren\": das Spiel-Abbild wird mit gewählt");
            kern_waehle_komplett(K, false, false);
            pruefe(f && !f->Wahl, "\"M.2 behalten\": das Spiel-Abbild bleibt");
        }
        (void)g;
    }
    else if (!strcmp(art, "m2weg")) {
        neu(schutz);
        printf("INFO M2_UNKLAR=%s m2Da=%s ohneUser=%s\n", K->M2_UNKLAR ? "ja" : "nein", K->m2Da ? "ja" : "nein", K->M2_OHNE_USER ? "ja" : "nein");
        int gesp = 0, frei = 0; sliste gl = {0};
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i];
            if (f->Gesperrt && f->SperrGrund && strstr(f->SperrGrund, "M.2-SSD wird gerade nicht erkannt")) { gesp++; sl_add(A0, &gl, f->Text); }
            else if (!f->Gesperrt) frei++; }
        printf("INFO gesperrt: %s\n", sl_join(A0, &gl, " / "));
        const char *h = kern_speicher_hinweis(K);
        /* da_Chris86 27.10.2026 21:39: fehlt die M.2 oder wird sie nicht erkannt, werden die internen Kacheln und Reste ganz normal angeboten */
        pruefe(gesp == 0, a_fmt(A0, "M.2 fehlt/unklar: nichts pauschal gesperrt (%d)", gesp));
        pruefe(frei > 10, a_fmt(A0, "alles normal wählbar (%d Einträge)", frei));
        pruefe(strstr(h, "gesperrt") == NULL, "Hinweis spricht von keiner Sperre");
        if (schutz) {
            kern_waehle_komplett(K, true, true);
            int gw = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Gesperrt && f->Wahl) gw++; }
            pruefe(gw == 0 && zaehle(0, true) > 0, a_fmt(A0, "vollständige Bereinigung: nichts Gesperrtes gewählt, %d Einträge gewählt", zaehle(0, true)));
            ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
            printf("ERG Fehler=%d\n", e.Fehler);
            pruefe(e.Fehler == 0, "vollständige Bereinigung ohne Fehler");
            pruefe(!da(VERB(A0, W, "/user/license/fakeEP0000-PPSA10481.rif")) && !da(VERB(A0, W, "/system_data/priv/appmeta/PPSA10481")), "Fake-Lizenz und Kachel-Reste des nicht sichtbaren M.2-Spiels PPSA10481 sind bereinigt");
            pruefe(da(VERB(A0, W, "/user/license/act.dat")), "Aktivierung (act.dat) bleibt");
        }
    }
    printf("FEHLERZAHL %d\n", fehler);
    return 0;
}
