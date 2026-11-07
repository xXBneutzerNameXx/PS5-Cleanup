/* PS5 Cleanup 1.0.8 - Test Mitschnitt und PC-Nachbau (Arbeitsplan 2.7, Codex-Prüfung 74)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_mitschnitt rund <wurzel> <mitschnitt>   drei Suchen mit DEMSELBEN Kern wie in der Oberfläche (dazwischen eine Kachel entfernt und ein
 *                                                vorher nicht lesbarer Ordner wieder lesbar, zuletzt das Nachschauen nach dem Löschen),
 *                                                mitgeschnitten - dann jede Suche aus dem Mitschnitt nachgebaut: muss gleich sein
 *   test_mitschnitt zeige <mitschnitt> [suche]   PC-Nachbau: Suche aus einem Mitschnitt der PS5, Ergebnis wie in der Auswahl
 *   test_mitschnitt detail <mitschnitt> [suche]  dasselbe als DETAIL-Zeilen wie die Oberfläche (Vergleich mit ihrer Ausgabe)
 * Gebaut mit -DMITSCHNITT (nur dann enthält kern/zugriff_mitschnitt.c etwas). */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void melde_log(void *c, const char *t) { (void)c; (void)t; }
static bool nie(void *c) { (void)c; return false; }
static void nichts(void *c, eintrag *e) { (void)c; (void)e; }
static void schreib_datei(void *c, const char *z) { fputs(z, (FILE *)c); fflush((FILE *)c); }

/* alles, was die Auswahl zeigt: Bereich, Art, Zeile, Pfade, Größe */
static const char *abzug(kern *k, arena *a)
{
    const char *t = "";
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        groesse *g = kern_eintrag_groesse_in(k, a, f, true);
        t = VERB(a, t, a_fmt(a, "%d|%d|%d|%d|%d|", f->Kat, f->KatHer, f->Art, f->Gesperrt ? 1 : 0, f->ToolKachel ? 1 : 0), kern_zeile(a, f),
                 g ? a_fmt(a, "|%lld/%d/%d", g->Bytes, g->Dateien, g->Ordner) : "|?", "\n");
        for (int j = 0; j < f->PfadeBasis.n; j++) t = VERB(a, t, "   ", f->PfadeBasis.e[j], "\n");
    }
    return t;
}
/* dieselben Zeilen wie die Oberfläche nach dem Nachschauen (app.c fundliste_ausgeben) - zum Vergleich mit ihrer Ausgabe */
static const char *wf(bool b) { return b ? "True" : "False"; }
static void detail_ausgeben(kern *k, const char *titel)
{
    arena *a = k->a;
    printf("PRUEF %s SPRACHE=%s ANZAHL=%d\n", titel, SP_EN ? "en" : "de", k->Funde.n);
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        printf("DETAIL %s\n", VERB(a, a_fmt(a, "%d", f->Kat), " | ", f->Text, " | basis=", f->TextBasis, " | gruppe=", f->Gruppe ? f->Gruppe : "-", " | teil=", f->Teil ? f->Teil : "-",
               " | gesperrt=", wf(f->Gesperrt), " | paket=", wf(f->Paket), " | nurleer=", wf(f->NurLeer), " | kachel=", wf(f->Kachel), " | smtipp=", wf(f->SmTipp),
               " | quelle=", f->Quelle, " :: ", sl_join(a, &f->Pfade, " | "), " || ", sl_join(a, &f->PfadeBasis, " | ")));
    }
    sliste h = r_split(a, h_trim(a, kern_speicher_hinweis(k)), "\n");
    printf("HINWEIS %s\n", sl_join(a, &h, " / "));
    printf("ANZAHL %d\n", k->Funde.n);
    printf("PRUEF ENDE %s\n", titel);
}

/* wie die Oberfläche (app.c programm und lauf_suchen) */
typedef struct { const char *modell, *ort; int db, d; bool en; sliste test; } konsole;
static const char *user_data(arena *a, const char *p) { return !strncmp(p, "/data/", 6) ? VERB(a, "/user", p) : p; }   /* /data ist /user/data */
static void einrichten(kern *k, const konsole *ko)
{
    k->melde = melde_log;
    k->MODELL_TYP = kern_modell_typ(k->a, ko->modell); k->MODELL_DIGITAL = kern_modell_digital(k->a, ko->modell);
    k->DDD_BEKANNT = ko->db != 0; k->DDD_STATUS = ko->d;
    for (int i = 0; i < ko->test.n; i++) sl_add(k->a, &k->TEST_ORDNER, a_kopie(k->a, user_data(k->a, ko->test.e[i])));
}
static void vor_suche(kern *k, const konsole *ko)   /* lauf_suchen: eigener Ort und Selbstschutz vor jeder Suche (nicht nach dem Löschen) */
{
    if (ko->ort && ko->ort[0]) kern_selbstschutz_eigener_pfad(k, user_data(k->a, ko->ort));
    kern_selbstschutz_ermitteln(k);
}
static bool suchen(kern *k, bool groessen)
{
    volatile int fe = 0;
    VERSUCHE { kern_suche_funde(k); if (groessen) kern_groessen_im_hintergrund(k, nie, NULL, nichts, NULL); }
    FANGE { printf("FAIL Suche abgebrochen: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    return !fe;
}

/* PC-Nachbau (Codex-Prüfung 74): die Suchen 1 bis n nacheinander mit DEMSELBEN Kern, jede aus ihrem eigenen Teil des Mitschnitts - so gilt
 * der Selbstschutz (z. B. nicht lesbare Ordner) über die Suchen hinweg wie in der Oberfläche. Konsole, Sprache, eigener Ort, Test-Ordner und
 * die Zeilen aus appinfo.db stehen im Mitschnitt (I, D). */
static zugriff *wg_z;
typedef struct { kern *k; void (*zeile)(struct kern *, const char *, const char *, const char *); } wg_ctx;
static void wg_cb(void *c, const char *t, const char *f, const char *w) { wg_ctx *x = c; x->zeile(x->k, t, f, w); }
static void wg_appinfo(struct kern *k, void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert)) { wg_ctx c = { k, zeile }; zugriff_wiedergabe_appinfo(wg_z, wg_cb, &c); }
typedef struct { int fehlend, kaputt; const sliste *fl, *kl; zugriff *z; } nachbau_info;
static kern *nachbau(const char *datei, int n, bool groessen, arena *a, nachbau_info *ni)
{
    kern *k = NULL;
    memset(ni, 0, sizeof *ni);
    for (int i = 1; i <= n; i++) {
        const char *fe = "";
        zugriff *z = zugriff_wiedergabe(datei, i, &fe);
        if (!z) { printf("Mitschnitt nicht lesbar: %s\n", fe); return NULL; }
        const sliste *info = zugriff_wiedergabe_info(z);
        konsole ko = { "", "", 0, 0, false, {0} };
        bool da = false, nach = false;
        const char *su = a_fmt(a, "SUCHE %d", i); size_t sl = strlen(su);
        for (int j = 0; j < info->n; j++) {
            const char *s = info->e[j], *m;
            if (!strncmp(s, "KONSOLE ", 8)) {
                if ((m = strstr(s, "modell="))) { const char *e = strstr(m, " firmware="); ko.modell = e ? a_kopie_n(a, m + 7, (size_t)(e - m - 7)) : a_kopie(a, m + 7); }
                if ((m = strstr(s, "ddd_bekannt="))) ko.db = atoi(m + 12);
                if ((m = strstr(s, " ddd="))) ko.d = atoi(m + 5);
                ko.en = strstr(s, "sprache=en") != NULL;
            } else if (!strncmp(s, "EIGENER_ORT ", 12)) ko.ort = s + 12;
            else if (!strncmp(s, "TEST_ORDNER ", 12)) sl_add(a, &ko.test, s + 12);
            else if (!strncmp(s, "SPRACHE ", 8)) ko.en = !strcmp(s + 8, "en");   /* die Sprache dieser Suche (vor der ersten umstellbar) */
            else if (!strncmp(s, su, sl) && (s[sl] == 0 || s[sl] == ' ')) { da = true; nach = strstr(s + sl, "NACH_LOESCHEN") != NULL; }
        }
        if (!da) { printf("Suche %d steht nicht im Mitschnitt\n", i); return NULL; }
        SP_EN = ko.en;
        if (!k) { k = kern_neu(z); einrichten(k, &ko); }
        else kern_verbinden(k, z);
        wg_z = z; k->appinfo_lesen = wg_appinfo;
        if (!nach) vor_suche(k, &ko);
        if (!suchen(k, groessen)) return NULL;
        ni->fehlend += zugriff_wiedergabe_fehlend(z, &ni->fl);
        ni->kaputt += zugriff_wiedergabe_kaputt(z, &ni->kl);
        ni->z = z;
    }
    return k;
}
static bool hat_pfad(kern *k, const char *p)
{
    for (int i = 0; i < k->Funde.n; i++) { eintrag *f = k->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (!strcmp(f->PfadeBasis.e[j], p)) return true; }
    return false;
}
static void vergleiche(const char *wer, const char *d1, const char *d2, arena *a, int n)
{
    pruefe(d1[0] && !strcmp(d1, d2), a_fmt(a, "%s: dieselben %d Einträge mit denselben Zeilen, Arten, Pfaden und Größen wie die Suche selbst", wer, n));
    if (strcmp(d1, d2)) {                              /* erste Abweichung zeigen */
        size_t i = 0; while (d1[i] && d1[i] == d2[i]) i++;
        size_t v = i > 300 ? i - 300 : 0;
        printf("   Suche:     ...%.600s\n   Nachbau:   ...%.600s\n", d1 + v, d2 + v);
    }
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    arena *a = arena_neu();
    if (argc >= 4 && !strcmp(argv[1], "rund")) {
        datei_optionen o = { .wurzel = argv[2], .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
        /* Codex-Prüfung 74: ein Ordner, der bei der ersten Suche nicht lesbar ist und danach wieder (der Selbstschutz merkt sich das) */
        const char *lt = a_fmt(a, "%s/user/data/Lesetest", argv[2]), *lta = a_fmt(a, "%s/a.txt", lt);
        mkdir(lt, 0755); { FILE *x = fopen(lta, "w"); if (x) { fputs("x", x); fclose(x); } }
        chmod(lt, 0);
        FILE *f = fopen(argv[3], "w");
        if (!f) { printf("FAIL %s nicht anlegbar\nFEHLERZAHL 1\n", argv[3]); return 1; }
        konsole ko = { "CFI-7021 B01Y", "/data/homebrew/PS5Cleanup", 1, 1, false, {0} };
        sl_add(a, &ko.test, "/user/data/PS5-Cleanup-Test"); sl_add(a, &ko.test, "/mnt/usb0/PS5-Cleanup-Test");
        fputs(zugriff_mitschnitt_zeile(a, 'I', "KONSOLE modell=CFI-7021 B01Y firmware=12.00 ddd_bekannt=1 ddd=1 sprache=de", NULL, NULL), f);   /* wie ms_oeffnen */
        fputs(zugriff_mitschnitt_zeile(a, 'I', "EIGENER_ORT /data/homebrew/PS5Cleanup", NULL, NULL), f);
        for (int i = 0; i < ko.test.n; i++) fputs(zugriff_mitschnitt_zeile(a, 'I', VERB(a, "TEST_ORDNER ", ko.test.e[i]), NULL, NULL), f);
        zugriff *zm = zugriff_mitschnitt(zugriff_sim(zugriff_datei(&o)), schreib_datei, f);
        /* Suche 1 */
        fputs("I SUCHE 1\n", f); fputs(zugriff_mitschnitt_zeile(a, 'I', "SPRACHE de", NULL, NULL), f);
        kern *k = kern_neu(zm); einrichten(k, &ko); vor_suche(k, &ko);
        const char *d1 = suchen(k, true) ? abzug(k, a) : ""; int n1 = k->Funde.n;
        bool lt1 = hat_pfad(k, "/user/data/Lesetest");
        /* Kachel entfernen (Simulation - auf der Platte bleibt alles): Zustand vor und nach muss im Mitschnitt stehen */
        antwort r = zm->befehl(zm, a, "SITE UNINSTALL", "/user/app/CUSA31001");
        fputs(zugriff_mitschnitt_zeile(a, 'P', "Protokoll-Zeile mit Leerzeichen und 100 %", NULL, NULL), f);
        fputs(zugriff_mitschnitt_zeile(a, 'D', "CUSA31001", "INSTALL_DIR_SAVEDATA", "CUSA31001"), f);
        /* Suche 2: der Ordner ist wieder lesbar - derselbe Kern wie in der Oberfläche */
        chmod(lt, 0755);
        fputs("I SUCHE 2\n", f); fputs(zugriff_mitschnitt_zeile(a, 'I', "SPRACHE de", NULL, NULL), f);
        kern_verbinden(k, zm); vor_suche(k, &ko);
        const char *d2 = suchen(k, true) ? abzug(k, a) : ""; int n2 = k->Funde.n;
        bool lt2 = hat_pfad(k, "/user/data/Lesetest");
        /* Suche 3: wie das erneute Nachschauen nach dem Löschen (ohne Selbstschutz-Durchgang davor) */
        fputs("I SUCHE 3 NACH_LOESCHEN\n", f); fputs(zugriff_mitschnitt_zeile(a, 'I', "SPRACHE de", NULL, NULL), f);
        kern_verbinden(k, zm);
        const char *d3 = suchen(k, true) ? abzug(k, a) : ""; int n3 = k->Funde.n;
        fclose(f);
        unlink(lta); rmdir(lt);                            /* der Testbaum wird danach noch gebraucht */
        printf("Mitschnitt: %d, %d und %d Einträge, Sony-Antwort in der Simulation %d, Lesetest bei Suche 1 %s, bei Suche 2 %s\n",
               n1, n2, n3, r.code, lt1 ? "angeboten" : "nicht angeboten", lt2 ? "angeboten" : "nicht angeboten");
        /* derselbe Ablauf aus dem Mitschnitt */
        const char *dn[3] = { d1, d2, d3 };
        for (int s = 1; s <= 3; s++) {
            nachbau_info ni; arena *b = arena_neu();
            kern *kn = nachbau(argv[3], s, true, b, &ni);
            pruefe(kn != NULL, a_fmt(a, "Suche %d aus dem Mitschnitt nachgebaut", s));
            if (!kn) continue;
            vergleiche(a_fmt(a, "PC-Nachbau Suche %d", s), dn[s - 1], abzug(kn, a), a, kn->Funde.n);
            pruefe(ni.fehlend == 0 && ni.kaputt == 0, a_fmt(a, "Suche %d: jede Frage stand im Mitschnitt, nichts kaputt (%d fehlten%s%s, %d kaputt)", s, ni.fehlend,
                                                          ni.fehlend && ni.fl && ni.fl->n ? ", z. B. " : "", ni.fehlend && ni.fl && ni.fl->n ? ni.fl->e[0] : "", ni.kaputt));
            if (s == 1) {   /* Wiedergabe verändert nie etwas */
                antwort w = ni.z->befehl(ni.z, a, "RMDA", "/user/app/CUSA31001"), w2 = ni.z->schreibe(ni.z, a, "/user/x.txt", "x", 1);
                pruefe(w.code >= 500 && w2.code >= 500, "Wiedergabe: Löschen und Schreiben werden abgelehnt");
            }
        }
        /* Inhalt des Mitschnitts */
        FILE *g = fopen(argv[3], "r"); long gr = 0; int zv = 0, zn = 0, zb = 0, zs = 0, zw2 = 0, zp = 0, zd = 0; char z[65536];
        bool elf_ganz = false;
        while (g && fgets(z, sizeof z, g)) {
            gr += (long)strlen(z);
            if (!strncmp(z, "I ZUSTAND VOR CUSA31001", 23)) zv++;
            if (!strncmp(z, "I ZUSTAND NACH CUSA31001", 24)) zn++;
            if (!strncmp(z, "B ", 2) && strstr(z, " SITE%20UNINSTALL /user/app/CUSA31001")) zb++;
            if (!strncmp(z, "S ", 2)) zs++;
            if (!strncmp(z, ". ", 2) && strlen(z) > 4000) elf_ganz = true;
            if (!strncmp(z, "P Protokoll-Zeile%20mit%20Leerzeichen%20und%20100%20%25", 52)) zp++;
            if (!strncmp(z, "D CUSA31001 INSTALL_DIR_SAVEDATA CUSA31001", 42)) zd++;
            if (!strncmp(z, "L ", 2) && strstr(z, " /user/appmeta/CUSA31001 ")) zw2++;
        }
        if (g) fclose(g);
        pruefe(zv == 1 && zn == 1 && zb == 1 && zw2 >= 2, a_fmt(a, "Kachel entfernen: Zustand vor (%d) und nach (%d), Befehl samt Antwort (%d), Ordner der Kachel-Infos gelistet (%d)", zv, zn, zb, zw2));
        pruefe(zs > 0 && !elf_ganz, a_fmt(a, "Programme der Kacheln: nur die Wege mitgeschrieben, nicht die ganze Datei (%d Stellen)", zs));
        pruefe(zp == 1 && zd == 1, "eigene Zeilen (Protokoll, appinfo.db) kodiert");
        printf("   Größe des Mitschnitts: %ld Byte\n", gr);
        /* Zustand nur für genau den App-Ordner, nicht für tiefere Wege (Codex-Prüfung 74) */
        {
            const char *zd2 = a_fmt(a, "%s.zustand", argv[3]); FILE *h = fopen(zd2, "w");
            zugriff *zz = h ? zugriff_mitschnitt(zugriff_sim(zugriff_datei(&o)), schreib_datei, h) : NULL;
            if (zz) { zz->befehl(zz, a, "RMDA", "/user/app/CUSA31001/sce_sys"); zz->befehl(zz, a, "RMDA", "/mnt/ext1/user/app/CUSA31001"); }
            if (h) fclose(h);
            int tief = 0, m2 = 0; FILE *q = fopen(zd2, "r");
            while (q && fgets(z, sizeof z, q)) { if (!strncmp(z, "I ZUSTAND VOR sce_sys", 21)) tief++; if (!strncmp(z, "I ZUSTAND VOR CUSA31001", 23)) m2++; }
            if (q) fclose(q);
            pruefe(tief == 0 && m2 == 1, a_fmt(a, "Zustand: nicht für tiefere Wege unter /user/app (%d), wohl für den App-Ordner auf der M.2 (%d)", tief, m2));
        }
        /* kaputte Mitschnitte: keine Abstürze; Unvollständiges und Kaputtes wird nicht verwendet, sondern gemeldet (Codex-Prüfung 74) */
        {
            const char *kp = a_fmt(a, "%s.kaputt", argv[3]); FILE *h = fopen(kp, "w");
            if (h) { fputs("L 226 ok /user 3\n. dx 0 a\n. f- 5\nL\nR 226 ok /x 3 ****\nA 226 ok /y -1 5 QUJD\nS 226 ok /z 5 2\n. QUJD\nK\n\n%%%%\nI SUCHE\n"
                           "L 226 ok /a 2\n. f- 1 x\nL 226 ok /a 1\n. f- 1 y\nR 226 ok /r 5 QUJD\nR 226 ok /r2 3 QUJD\n", h); fclose(h); }
            const char *fe = "";
            zugriff *zk = zugriff_wiedergabe(kp, 0, &fe);
            kdliste l = {0}, la = {0}; antwort rl = { 0, "" }, ra = { 0, "" }, rr = { 0, "" }, rr2 = { 0, "" };
            if (zk) {
                rl = zk->liste(zk, a, "/user", &l); ra = zk->liste(zk, a, "/a", &la);
                const unsigned char *d; size_t n; zk->lies(zk, a, "/x", &d, &n); rr = zk->lies(zk, a, "/r", &d, &n); rr2 = zk->lies(zk, a, "/r2", &d, &n);
                unsigned char b[8]; size_t bl; zk->lies_ab(zk, a, "/y", 0, 8, b, &bl);
            }
            int nk = zk ? zugriff_wiedergabe_kaputt(zk, NULL) : -1;
            pruefe(zk != NULL && nk >= 5 && rl.code >= 500 && rr.code >= 500 && rr2.code < 400,
                   a_fmt(a, "kaputter Mitschnitt: gelesen ohne Absturz, %d Datensätze als kaputt gemeldet, abgeschnittene Liste und falsche Länge nicht verwendet (%d, %d, %d)", nk, rl.code, rr.code, rr2.code));
            pruefe(zk && ra.code < 400 && la.n == 1 && !strcmp(la.e[0].name, "y"), "unvollständige erste Antwort verworfen, die vollständige spätere gilt");
        }
        /* Codex-Prüfung 75: kaputte Anzahl (negativ, keine Zahl), kaputter Antwortcode, kaputte Größe, ausdrücklich unvollständige Wege - nichts
         * davon wird verwendet, alles gemeldet; danach liest die Wiedergabe normal weiter */
        {
            const char *kp = a_fmt(a, "%s.kaputt2", argv[3]); FILE *h = fopen(kp, "w");
            if (h) { fputs("L 226 ok /neg -1\n. f- 1 n\nL 226 ok /txt zwei\n. f- 1 t\nS 226 ok /voll 5 1 voll\n. QUJD\nL xx ok /code 1\n. f- 1 c\n"
                           "R 226 ok /rl 3x QUJD\nL 226 ok /gr 1\n. f- abc g\nL 226 ok /gut 1\n. f- 7 g\n", h); fclose(h); }
            const char *fe = "";
            zugriff *zk = zugriff_wiedergabe(kp, 0, &fe);
            kdliste l1 = {0}, l2 = {0}, l3 = {0}, l4 = {0}, l5 = {0};
            antwort a1 = { 0, "" }, a2 = { 0, "" }, a3 = { 0, "" }, a4 = { 0, "" }, a5 = { 0, "" }, a6 = { 0, "" };
            if (zk) {
                a1 = zk->liste(zk, a, "/neg", &l1); a2 = zk->liste(zk, a, "/txt", &l2); a3 = zk->liste(zk, a, "/code", &l3);
                a4 = zk->liste(zk, a, "/gr", &l4); a5 = zk->liste(zk, a, "/gut", &l5);
                const unsigned char *d; size_t n; a6 = zk->lies(zk, a, "/rl", &d, &n);
            }
            const sliste *kl = NULL; int nk = zk ? zugriff_wiedergabe_kaputt(zk, &kl) : -1;
            int gefunden = 0; const char *soll[6] = { "Anzahl kaputt: /neg", "Anzahl kaputt: /txt", "S unvollständig", "Antwortcode kaputt: /code", "R Länge oder Daten kaputt: /rl", "L Folgezeile kaputt: /gr" };
            for (int i = 0; i < 6; i++) for (int j = 0; kl && j < kl->n; j++) if (strstr(kl->e[j], soll[i])) { gefunden++; break; }
            pruefe(zk != NULL && nk == 6 && gefunden == 6, a_fmt(a, "kaputte Zahlen und unvollständige Wege gemeldet (%d gemeldet, %d von 6 erkannt)", nk, gefunden));
            pruefe(a1.code >= 500 && a2.code >= 500 && a3.code >= 500 && a4.code >= 500 && a6.code >= 500,
                   a_fmt(a, "nichts davon verwendet - keine leere Liste aus einer kaputten Anzahl (%d %d %d %d %d)", a1.code, a2.code, a3.code, a4.code, a6.code));
            pruefe(a5.code < 400 && l5.n == 1 && !strcmp(l5.e[0].name, "g") && l5.e[0].groesse == 7, "danach liest die Wiedergabe normal weiter");
        }
        /* Codex-Prüfung 73: Simulation - eine "geschriebene" Datei ist nach dem Löschen ihres Ordners weg */
        zugriff *zsim = zugriff_sim(zugriff_datei(&o));
        antwort r0 = zsim->schreibe(zsim, a, "/user/data/tmp/neu.txt", "abc", 3);
        unsigned char bb[8]; size_t bl = 0; antwort r1 = zsim->lies_ab(zsim, a, "/user/data/tmp/neu.txt", 0, 3, bb, &bl);
        antwort r2 = zsim->befehl(zsim, a, "RMDA", "/user/data/tmp");
        antwort r3 = zsim->lies_ab(zsim, a, "/user/data/tmp/neu.txt", 0, 3, bb, &bl);
        const unsigned char *dd; size_t dnn; antwort r4 = zsim->lies(zsim, a, "/user/data/tmp/neu.txt", &dd, &dnn);
        pruefe(r0.code < 400 && r1.code < 400 && r2.code < 400 && r3.code >= 400 && r4.code >= 400,
               a_fmt(a, "Simulation: geschriebene Datei nach dem Löschen ihres Ordners weg (%d %d %d %d %d)", r0.code, r1.code, r2.code, r3.code, r4.code));
    } else if (argc >= 3 && (!strcmp(argv[1], "zeige") || !strcmp(argv[1], "detail"))) {
        bool nur_detail = !strcmp(argv[1], "detail");
        int n = argc >= 4 ? atoi(argv[3]) : 1; if (n < 1) n = 1;
        nachbau_info ni;
        kern *k = nachbau(argv[2], n, !nur_detail, a, &ni);
        if (!k) return 1;
        if (nur_detail) { detail_ausgeben(k, "MIT_SELBSTSCHUTZ"); printf("FEHLEND %d\nKAPUTT %d\n", ni.fehlend, ni.kaputt); return 0; }
        const sliste *info = zugriff_wiedergabe_info(ni.z);
        for (int i = 0; i < info->n && i < 40; i++) printf("Hinweis: %s\n", info->e[i]);
        printf("Funde: %d\n", k->Funde.n);
        for (int ki = 0; ki < KAT_ANZ; ki++) {
            int kat = ALLE_KAT[ki]; kblock *b; int nb = kern_bloecke(k, a, kat, SORT_GROESSTE, &b);
            printf("== %s (%d Blöcke)\n", kern_kat_name(kat), nb);
            for (int x = 0; x < nb; x++) for (int j = 0; j < b[x].teile.n; j++) {
                eintrag *f = b[x].teile.e[j]; groesse *g = kern_eintrag_groesse_in(k, a, f, true);
                printf("%s%s  [%s]  %s\n", j ? "      " : "", kern_zeile(a, f), kern_folge(f), g ? kern_groesse_text(a, g->Bytes) : "?");
            }
        }
        printf("Fragen, die nicht im Mitschnitt standen: %d\n", ni.fehlend);
        for (int i = 0; ni.fl && i < ni.fl->n && i < 30; i++) printf("   %s\n", ni.fl->e[i]);
        printf("Kaputte oder unvollständige Datensätze (nicht verwendet): %d\n", ni.kaputt);
        for (int i = 0; ni.kl && i < ni.kl->n && i < 30; i++) printf("   %s\n", ni.kl->e[i]);
        return 0;
    } else { printf("Aufruf: test_mitschnitt rund <wurzel> <mitschnitt> | zeige|detail <mitschnitt> [suche]\n"); return 2; }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
