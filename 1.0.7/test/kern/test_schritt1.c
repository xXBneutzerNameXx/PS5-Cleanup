/* PS5 Cleanup 1.1 - Tests zu Schritt 1 (Zugriffsschicht, Sitzung, Hilfen)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 *   test_schritt1 muster <spur.jsonl>
 *       jeder Aufruf der Klasse H aus 1.0.5 (Muster, Text, Ergebnis) wird mit
 *       dem C-Kern nachgerechnet - Ergebnis muss gleich sein
 *   test_schritt1 baum <wurzel>
 *       ganzer Baum über Zugriffsschicht + Sitzung: "Typ Größe Pfad"
 *   test_schritt1 befehle <wurzel> <befehle.txt> [sim]
 *       Befehle ausführen (echt oder Simulation), danach den Baum ausgeben,
 *       so wie ihn die Sitzung sieht; mit "sim" zusätzlich das Buch
 *   test_schritt1 sitzung <wurzel>
 *       Sonderfälle der Sitzung (streng, verweigert, Existiert, Steuerzeichen)
 */
#include "../../kern/basis.h"
#include "../../kern/hilfen.h"
#include "../../kern/sitzung.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *lies_datei(const char *p, size_t *n)
{
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); }
    fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)l + 1); if (fread(b, 1, (size_t)l, f) != (size_t)l) { perror("fread"); exit(2); }
    b[l] = 0; fclose(f); if (n) *n = (size_t)l; return b;
}

/* ------------------------------------------------------------------ muster */
static int muster(const char *datei)
{
    char *alles = lies_datei(datei, NULL);
    long n = 0, gleich = 0, anders = 0, fehler = 0;
    long je[5][2] = {{0}};
    const char *namen[5] = { "M", "MT", "R", "LIKE", "GLEICH" };
    for (char *z = strtok(alles, "\n"); z; z = strtok(NULL, "\n")) {
        size_t lz = strlen(z); if (lz && z[lz - 1] == '\r') z[lz - 1] = 0;
        cJSON *j = cJSON_Parse(z);
        if (!j || cJSON_GetArraySize(j) != 4) { fprintf(stderr, "kaputte Zeile: %.80s\n", z); fehler++; cJSON_Delete(j); continue; }
        const char *f = cJSON_GetArrayItem(j, 0)->valuestring, *m = cJSON_GetArrayItem(j, 1)->valuestring,
                   *s = cJSON_GetArrayItem(j, 2)->valuestring, *soll = cJSON_GetArrayItem(j, 3)->valuestring;
        arena *a = arena_neu();
        const char *volatile ist = "?"; volatile int k = -1;
        volatile int ausn = 0;
        VERSUCHE {
            if (!strcmp(f, "M")) { k = 0; ist = h_m(s, m) ? "1" : "0"; }
            else if (!strcmp(f, "MT")) {
                k = 1;
                htreffer t = h_mt(a, s, m);
                /* wie die Spur in 1.0.5: ein fehlgeschlagener Match (Match.Empty) hat in .NET
                   genau eine (leere) Gruppe - Groups[i] liefert dann für jedes i "" */
                sliste l = {0}; sl_add(a, &l, t.ok ? "1" : "0");
                for (int i = 0; i < (t.ok ? t.ngr : 1); i++) sl_add(a, &l, t.gr[i]);
                ist = sl_join(a, &l, "\x01");
            }
            else if (!strcmp(f, "R")) {
                k = 2;
                const char *x = strstr(m, "\x02");
                ist = h_r(a, s, a_kopie_n(a, m, (size_t)(x - m)), x + 1);
            }
            else if (!strcmp(f, "LIKE")) { k = 3; ist = h_like(s, m) ? "1" : "0"; }
            else if (!strcmp(f, "GLEICH")) { k = 4; ist = txt_gleich_ci(m, s) ? "1" : "0"; }
        } FANGE {
            ausn = 1; ist = a_fmt(a, "AUSNAHME %s", ausn_text);
        } ENDE_VERSUCHE;
        n++;
        if (k >= 0) je[k][0]++;
        if (strcmp(ist, soll) == 0) gleich++;
        else {
            anders++; if (k >= 0) je[k][1]++;
            if (anders <= 40) printf("ANDERS %s muster=%s text=%s soll=%s ist=%s\n", f, m, s, soll, ist);
        }
        (void)ausn;
        arena_frei(a); cJSON_Delete(j);
    }
    printf("MUSTER Aufrufe=%ld gleich=%ld anders=%ld kaputt=%ld\n", n, gleich, anders, fehler);
    for (int k = 0; k < 5; k++) printf("  %-6s %8ld Aufrufe, %ld anders\n", namen[k], je[k][0], je[k][1]);
    free(alles);
    return anders || fehler ? 1 : 0;
}

/* ------------------------------------------------------------------ Baum ausgeben */
static zugriff *datei_zugriff(const char *wurzel)
{
    datei_optionen o = { .wurzel = wurzel, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    return zugriff_datei(&o);
}

static void baum_aus(sitzung *s, const char *pfad, sliste *aus)
{
    dliste *l = s_liste(s, pfad, true);
    if (!l) return;
    for (int i = 0; i < l->n; i++) {
        datei *e = &l->e[i];
        const char *p = strcmp(pfad, "/") == 0 ? VERB(s->a, "/", e->name) : VERB(s->a, pfad, "/", e->name);
        char typ = e->ist_link ? 'l' : e->ist_ordner ? 'd' : 'f';
        sl_add(s->a, aus, a_fmt(s->a, "%c %lld %s", typ, e->ist_ordner ? 0LL : e->groesse, p + 1));
        if (e->ist_ordner && !e->ist_link) baum_aus(s, p, aus);
    }
}
static int sortiere(const void *x, const void *y) { return strcmp(*(const char *const *)x, *(const char *const *)y); }
static void drucke_baum(sitzung *s)
{
    sliste l = {0};
    baum_aus(s, "/", &l);
    qsort(l.e, (size_t)l.n, sizeof(char *), sortiere);
    for (int i = 0; i < l.n; i++) printf("%s\n", l.e[i]);
}

static int baum(const char *wurzel)
{
    arena *a = arena_neu();
    zugriff *z = datei_zugriff(wurzel);
    sitzung *s = sitzung_neu(z, a);
    volatile int rc = 0;
    VERSUCHE { drucke_baum(s); } FANGE { printf("AUSNAHME %d %s\n", ausn_art, ausn_text); rc = 1; } ENDE_VERSUCHE;
    z->schliessen(z); arena_frei(a);
    return rc;
}

/* ------------------------------------------------------------------ Befehle */
static int befehle(const char *wurzel, const char *datei, bool sim)
{
    arena *a = arena_neu();
    zugriff *echt = datei_zugriff(wurzel);
    zugriff *z = sim ? zugriff_sim(echt) : echt;
    sitzung *s = sitzung_neu(z, a);
    char *alles = lies_datei(datei, NULL);
    char *naechste = NULL;
    for (char *z0 = strtok_r(alles, "\n", &naechste); z0; z0 = strtok_r(NULL, "\n", &naechste)) {
        char *volatile zeile = z0;
        size_t lz = strlen(zeile); if (lz && zeile[lz - 1] == '\r') zeile[lz - 1] = 0;
        if (!*zeile || *zeile == '#') continue;
        /* "DELE /pfad", "RMD /pfad", "RMDA /pfad", "SITE CHMOD 777 /pfad", "STOR /pfad", "EXISTIERT /pfad" */
        char *p = strrchr(zeile, ' ');
        if (!p) continue;
        *p = 0; const char *bef = zeile, *pfad = p + 1;
        volatile int ok = 1;
        VERSUCHE {
            if (!strcmp(bef, "STOR")) printf("STOR %s -> %d\n", pfad, s_schreiben(s, pfad, "x", 1));
            else if (!strcmp(bef, "EXISTIERT")) printf("EXISTIERT %s -> %d\n", pfad, s_existiert(s, pfad));
            else { antwort r = s_befehl(s, bef, pfad); printf("%s %s -> %s\n", bef, pfad, r.text); }
        } FANGE { ok = 0; printf("%s %s -> AUSNAHME %s\n", bef, pfad, ausn_text); } ENDE_VERSUCHE;
        (void)ok;
    }
    printf("== BAUM\n");
    VERSUCHE { drucke_baum(s); } FANGE { printf("AUSNAHME %s\n", ausn_text); } ENDE_VERSUCHE;
    if (sim) {
        const sliste *b = zugriff_sim_buch(z);
        printf("== BUCH (%d)\n", b->n);
        for (int i = 0; i < b->n; i++) printf("%s\n", b->e[i]);
    }
    echt->schliessen(echt);
    free(alles); arena_frei(a);
    return 0;
}

/* ------------------------------------------------------------------ Sitzung: Sonderfälle */
#define PRUEFE(bed, text) do { bool _b = (bed); printf("%s %s\n", _b ? "OK  " : "FAIL", text); if (!_b) fehler++; } while (0)
static int sitzung_tests(const char *wurzel)
{
    volatile int fehler = 0;
    arena *a = arena_neu();
    zugriff *z = datei_zugriff(wurzel);
    sitzung *s = sitzung_neu(z, a);

    PRUEFE(s_liste(s, "/gibtsnicht", false) == NULL, "Liste: fehlender Ordner -> null");
    {
        volatile int geworfen = 0;
        VERSUCHE { s_liste(s, "/gibtsnicht", true); } FANGE { geworfen = 1; } ENDE_VERSUCHE;
        PRUEFE(!geworfen, "Liste streng: wirklich fehlender Ordner -> null, keine Ausnahme");
    }
    PRUEFE(s_liste(s, "/user", false) != NULL, "Liste /user gelesen");
    PRUEFE(s_existiert(s, "/user/app"), "Existiert /user/app");
    PRUEFE(!s_existiert(s, "/user/gibtsnicht"), "Existiert: fehlt -> false");
    PRUEFE(!s_existiert(s, "/gibtsnicht/auch/nicht"), "Existiert: Ordner darüber fehlt -> false");
    PRUEFE(strcmp(s_text(s, "/gibtsnicht.txt"), "") == 0, "Text: fehlende Datei -> leer");
    {
        volatile int geworfen = 0; char text[300] = "";
        VERSUCHE { s_liste(s, "/user/a\nb", false); } FANGE { geworfen = ausn_art == AUSN_IO; snprintf(text, sizeof text, "%s", ausn_text); } ENDE_VERSUCHE;
        PRUEFE(geworfen && strstr(text, "Ungültiges Zeichen im FTP-Befehl: CWD /user") == text, "Steuerzeichen im Pfad -> IOException, nichts gesendet");
    }
    /* DENY: vorhandener, aber gesperrter Ordner (wie miniftp4.py DENY=/user/appmeta) */
    setenv("DENY", "/user/appmeta", 1);
    {
        volatile int geworfen = 0; char text[400] = "";
        VERSUCHE { s_liste(s, "/user/appmeta", true); } FANGE { geworfen = 1; snprintf(text, sizeof text, "%s", ausn_text); } ENDE_VERSUCHE;
        printf("     Text: %s\n", text);
        PRUEFE(geworfen && !strcmp(text, "PS5 verweigert den Zugriff auf /user/appmeta (550 Permission denied) - zur Sicherheit wird abgebrochen."),
               "Liste streng: verweigerter Ordner -> Abbruch mit dem Text von 1.0.5");
        PRUEFE(s_liste(s, "/user/appmeta", false) == NULL, "Liste nicht streng: verweigerter Ordner -> null");
    }
    unsetenv("DENY");
    /* FAILLIST: Liste bricht mittendrin ab */
    setenv("FAILLIST", "/user/app", 1);
    { FILE *f = fopen("faillist.on", "w"); if (f) fclose(f); }
    {
        volatile int geworfen = 0; char text[400] = "";
        VERSUCHE { s_liste(s, "/user/app", true); } FANGE { geworfen = ausn_art == AUSN_IO; snprintf(text, sizeof text, "%s", ausn_text); } ENDE_VERSUCHE;
        printf("     Text: %s\n", text);
        PRUEFE(geworfen && !strcmp(text, "Ordner /user/app konnte nicht vollständig gelesen werden - bitte nochmal verbinden."), "Liste streng: unvollständige Liste -> IOException wie 1.0.5");
        PRUEFE(s_liste(s, "/user/app", false) == NULL, "Liste nicht streng: unvollständige Liste -> null");
        PRUEFE(!strcmp(s->letzter_fehler, "LIST: 451 read error"), "LetzterFehler wie 1.0.5");
        geworfen = 0;
        VERSUCHE { s_existiert(s, "/user/app/X"); } FANGE { geworfen = 1; snprintf(text, sizeof text, "%s", ausn_text); } ENDE_VERSUCHE;
        PRUEFE(geworfen && !strcmp(text, "Ordner /user/app konnte nicht gelesen werden"), "Existiert: Liste nicht lesbar -> Abbruch statt \"gibt es nicht\"");
    }
    remove("faillist.on"); unsetenv("FAILLIST");
    /* Englisch */
    SP_EN = true;
    setenv("DENY", "/user/appmeta", 1);
    {
        volatile int geworfen = 0; char text[400] = "";
        VERSUCHE { s_liste(s, "/user/appmeta", true); } FANGE { geworfen = 1; snprintf(text, sizeof text, "%s", ausn_text); } ENDE_VERSUCHE;
        PRUEFE(geworfen && !strcmp(text, "PS5 denies access to /user/appmeta (550 Permission denied) - stopping to be safe."), "Englisch: Text wie 1.0.5");
    }
    unsetenv("DENY"); SP_EN = false;
    /* Gesund */
    PRUEFE(s_gesund(s), "Gesund");
    z->schliessen(z); arena_frei(a);
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && !strcmp(argv[1], "muster")) return muster(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "baum")) return baum(argv[2]);
    if (argc >= 4 && !strcmp(argv[1], "befehle")) return befehle(argv[2], argv[3], argc >= 5 && !strcmp(argv[4], "sim"));
    if (argc >= 3 && !strcmp(argv[1], "sitzung")) return sitzung_tests(argv[2]);
    fprintf(stderr, "Aufruf: muster <spur.jsonl> | baum <wurzel> | befehle <wurzel> <datei> [sim] | sitzung <wurzel>\n");
    return 2;
}
