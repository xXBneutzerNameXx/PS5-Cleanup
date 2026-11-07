/* PS5 Cleanup 1.0.7 - Test PS4-Spieleschutz, Datenbank-Sicherung, Ende ohne Neuaufbau-Empfehlung
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_ps4 <hier>   Testbaum <hier>/w (lauf_ps4.sh), Datenbanken <hier>/mms*, Stick <hier>/stick
 * Auftrag da_Chris86 31.10.2026 19:56 (Punkte 1, 2, 4, 6). Alles auf nachgebildeten Dateisystemen - nie auf einer PS5.
 * Jede Prüfung druckt OK/FAIL, am Ende FEHLERZAHL. */
#include "../../kern/kern.h"
#include "../../kern/ablauf.h"
#include "../../db/db_lesen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static sliste LOG; static arena *A0;
static void melde_log(void *c, const char *t) { (void)c; sl_add(A0, &LOG, a_kopie(A0, t ? t : "")); }
static bool log_hat(const char *s) { for (int i = 0; i < LOG.n; i++) if (strstr(LOG.e[i], s)) return true; return false; }
static const char *W, *HIER;
static bool da(const char *v) { struct stat st; return stat(VERB(A0, W, v), &st) == 0; }
static void tdatei(const char *v, size_t n)
{
    char *r = (char *)VERB(A0, W, v);
    for (char *q = r + strlen(W) + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(r, 0755); *q = '/'; }
    FILE *f = fopen(VERB(A0, W, v), "wb"); if (!f) { printf("FAIL Testdatei %s\n", v); fehler++; return; } for (size_t i = 0; i < n; i++) fputc('x', f); fclose(f); }
static bool ja(void *c) { (void)c; return true; }

static kern *neu(void)
{
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = HIER };
    kern *k = kern_neu(zugriff_datei(&o)); k->melde = melde_log;
    LOG = (sliste){0};
    volatile int fe = 0;
    VERSUCHE { kern_suche_funde(k); } FANGE { printf("FAIL Suche abgebrochen: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) { printf("FEHLERZAHL %d\n", fehler + 1); exit(1); }
    return k;
}
static const ps4_spiel *spiel(kern *k, const char *id, int ort)
{
    for (int i = 0; i < k->PS4.n; i++) { const ps4_spiel *s = k->PS4.e[i]; if (!strcmp(s->id, id) && s->ort == ort) return s; }
    return NULL;
}
static eintrag *mit_pfad(kern *k, const char *p)
{
    for (int i = 0; i < k->Funde.n; i++) { eintrag *f = k->Funde.e[i]; for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], p)) return f; }
    return NULL;
}
static eintrag *spiel_eintrag(kern *k, const char *gruppe)
{
    for (int i = 0; i < k->Funde.n; i++) { eintrag *f = k->Funde.e[i]; if (ist_teil(f, "Spiel") && f->Gruppe && !strcmp(f->Gruppe, gruppe)) return f; }
    return NULL;
}
static void keine_wahl(kern *k) { for (int i = 0; i < k->Funde.n; i++) ((eintrag *)k->Funde.e[i])->Wahl = false; }
static bool geschuetzt(kern *k, const char *p) { const char *g; return kern_ps4_geschuetzt(k, p, &g); }

/* ---- Bedienablauf (für "Vorbereitung direkt vor dem Löschen schlägt fehl -> nichts löschen") */
static int FRAGEN_Z = 0;
static bool u_wahl(void *c, const char *t, const char *ti, const char *s, const char *g) { (void)c; (void)t; (void)ti; (void)s; (void)g; FRAGEN_Z++; return true; }
static const char *u_text(void *c, const char *t, const char *ti) { (void)c; (void)t; (void)ti; FRAGEN_Z++; return SP_EN ? "DELETE" : "LÖSCHEN"; }
static void u_log(void *c, const char *t) { melde_log(c, t); }
static void u_stand(void *c, const char *t, double p) { (void)c; (void)t; (void)p; }
static void u_fertig(void *c, const char *k, bool ok, const char *f, const char *s) { (void)c; (void)k; (void)ok; (void)f; (void)s; }
static void u_fehler(void *c, const char *t) { (void)c; melde_log(NULL, VERB(A0, "FEHLERMELDUNG ", t)); }
static int vor_loeschen_gerufen = 0;
static const char *EINB_ID = "", *EINB_QUELLE = "";
static int test_einbindung(arena *a, const char *id, const char **fs, const char **q)
{
    (void)a;
    if (!strcmp(id, EINB_ID)) { *fs = "nullfs"; *q = EINB_QUELLE; return 1; }
    *fs = NULL; *q = NULL; return 0;
}
static bool vorbereitung_scheitert(void *c) { (void)c; vor_loeschen_gerufen++; return false; }
static bool nie(void *c) { (void)c; return false; }
static void nichts(void *c, eintrag *e) { (void)c; (void)e; }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 2) { fprintf(stderr, "Aufruf: test_ps4 <hier>\n"); return 2; }
    A0 = arena_neu();
    HIER = argv[1]; W = VERB(A0, HIER, "/w");

    /* =============================================================== 1. Inventar */
    printf("== 1. Inventar der PS4-Spiele (nur lesen)\n");
    kern *k = neu();
    const ps4_spiel *s1 = spiel(k, "CUSA30001", ORT_INTERN), *s2 = spiel(k, "CUSA30002", ORT_INTERN), *s3 = spiel(k, "CUSA30003", ORT_M2),
                    *s4 = spiel(k, "CUSA30004", ORT_INTERN), *s5 = spiel(k, "CUSA30005", ORT_INTERN), *s6i = spiel(k, "CUSA30006", ORT_INTERN),
                    *s6m = spiel(k, "CUSA30006", ORT_M2), *s7 = spiel(k, "CUSA30007", ORT_USB);
    pruefe(k->PS4.n == 10, a_fmt(A0, "10 PS4-Ordner erfasst (intern, M.2, USB-Speicher - seit 05.11.2026 mit CUSA30009 intern leer und auf der M.2; ist %d)", k->PS4.n));
    pruefe(s1 && s1->app_da && s1->app_bytes == 4096 && kern_ps4_installiert(s1), "CUSA30001 intern: installiert, app.pkg 4096 Byte");
    pruefe(s1 && s1->patch_da && s1->patch_bytes == 2048 && !strcmp(s1->version, "01.05"), a_fmt(A0, "CUSA30001: Update 2048 Byte, Version aus dem Update 01.05 (ist %s)", s1 ? s1->version : "-"));
    pruefe(s1 && s1->dlc == 1, "CUSA30001: 1 Zusatzinhalt");
    pruefe(s1 && s1->lizenzen.n == 2 && s1->fake, "CUSA30001: 2 Lizenzdateien (rif/idx), Fake-Lizenz erkannt");
    pruefe(s1 && !strcmp(s1->name, "Spiel Eins"), a_fmt(A0, "CUSA30001: Name aus param.sfo (ist %s)", s1 ? s1->name : "-"));
    pruefe(s1 && s1->eingetragen == -1, "CUSA30001: Menüeintrag am PC nicht abfragbar (-1, nur die PS5-Fassung fragt Sony)");
    pruefe(s2 && kern_ps4_installiert(s2) && s2->lizenzen.n == 1 && !s2->fake, "CUSA30002 intern: installiert, echte Lizenzdatei (kein Fake)");
    pruefe(s3 && kern_ps4_installiert(s3) && s3->basis && !strcmp(s3->basis, M2 "/user"), "CUSA30003 auf der M.2: installiert, Ort M.2");
    pruefe(s4 && !kern_ps4_installiert(s4), "CUSA30004: leerer Ordner (wie nach einem Neuaufbau) = NICHT installiert");
    pruefe(s5 && s5->unklar && kern_ps4_installiert(s5), "CUSA30005: kein app.pkg, Ordner aber nicht leer = unklar = gilt als installiert");
    pruefe(s6i && s6m && kern_ps4_installiert(s6i) && kern_ps4_installiert(s6m), "CUSA30006: zwei Kopien (intern + M.2) getrennt erfasst");
    pruefe(s7 && kern_ps4_installiert(s7), "CUSA30007 auf dem USB-Speicher: installiert");
    if (s1) { const char *z = kern_ps4_zeile(A0, s1); pruefe(strstr(z, "Spiel Eins") && strstr(z, "CUSA30001") && strstr(z, "intern") && strstr(z, "Update 01.05") && strstr(z, "Fake-Lizenz"), a_fmt(A0, "Zeile DE: %s", z)); }
    if (s3) { SP_EN = true; const char *z = kern_ps4_zeile(A0, s3); SP_EN = false; pruefe(strstr(z, "CUSA30003") && strstr(z, "M.2 SSD"), a_fmt(A0, "Zeile EN: %s", z)); }
    if (s4) { const char *z = kern_ps4_zeile(A0, s4); pruefe(strstr(z, "FEHLEN") != NULL, a_fmt(A0, "Zeile leerer Ordner: %s", z)); }

    /* =============================================================== 2. Zuordnung */
    printf("== 2. Zuordnung von Pfaden zu PS4-Installationen\n");
    struct { const char *p, *id, *basis; } Z[] = {
        { "/user/app/CUSA30001", "CUSA30001", "/user" }, { "/user/app/CUSA30001/app.pkg", "CUSA30001", "/user" },
        { "/user/patch/CUSA30001", "CUSA30001", "/user" }, { "/user/addcont/CUSA30001/DLC0000000000001", "CUSA30001", "/user" },
        { M2 "/user/app/CUSA30003", "CUSA30003", M2 "/user" }, { USB_ERW "/user/patch/CUSA30007", "CUSA30007", USB_ERW "/user" },
        { M2 "/user/appmeta/CUSA30003", "CUSA30003", M2 "/user" },
        { "/user/appmeta/CUSA30001", "CUSA30001", NULL }, { SYS_META "/CUSA30001", "CUSA30001", NULL },
        { "/user/license/fakeEP1004-CUSA30001_00.rif", "CUSA30001", NULL }, { "/user/playgo/app/EP1004-CUSA30001_00-AAAAAAAAAAAAAAAA", "CUSA30001", NULL },
        { "/user/download/CUSA30001", "CUSA30001", NULL }, { "/user/system/share/CUSA30001", "CUSA30001", NULL },
        { "/user/app/PPSA10001", NULL, NULL }, { "/user/data/MeinOrdner", NULL, NULL }, { "/user/home/1234abcd/savedata/CUSA30001", NULL, NULL },
        { "/user/app/CUSA3000", NULL, NULL }, { "/user/app/XCUSA30001", NULL, NULL } };
    for (size_t i = 0; i < sizeof Z / sizeof Z[0]; i++) {
        const char *b; const char *id = kern_ps4_zuordnung(A0, Z[i].p, &b);
        bool ok = (!id && !Z[i].id) || (id && Z[i].id && !strcmp(id, Z[i].id) && ((!b && !Z[i].basis) || (b && Z[i].basis && !strcmp(b, Z[i].basis))));
        pruefe(ok, a_fmt(A0, "%s -> %s%s%s", Z[i].p, Z[i].id ? Z[i].id : "keine PS4-Installation", Z[i].id ? (Z[i].basis ? " auf " : " (gemeinsam)") : "", Z[i].basis ? Z[i].basis : ""));
    }
    pruefe(true, "Spielstände (savedata) gehören nicht zur Installation - eigener Bereich, eigene Wahl");

    /* =============================================================== 3. Schutz ohne Auswahl */
    printf("== 3. Schutzregeln - nichts gewählt\n");
    keine_wahl(k); kern_ps4_weg_setzen(k);
    const char *G1[] = { "/user/app/CUSA30001", "/user/patch/CUSA30001", "/user/addcont/CUSA30001", "/user/appmeta/CUSA30001", "/user/license/fakeEP1004-CUSA30001_00.rif",
                         "/user/playgo/app/EP1004-CUSA30001_00-AAAAAAAAAAAAAAAA", "/user/download/CUSA30001", M2 "/user/app/CUSA30003", USB_ERW "/user/app/CUSA30007",
                         "/user/app/CUSA30005", "/user/license/fakeEP1004-CUSA30005_00.rif", NULL };
    for (int i = 0; G1[i]; i++) pruefe(geschuetzt(k, G1[i]), a_fmt(A0, "geschützt: %s", G1[i]));
    const char *F1[] = { "/user/app/CUSA30004", "/user/appmeta/CUSA30004", "/user/license/fakeEP1004-CUSA30004_00.rif", "/user/data/MeinOrdner", "/user/app/CUSA39999", NULL };
    for (int i = 0; F1[i]; i++) pruefe(!geschuetzt(k, F1[i]), a_fmt(A0, "nicht geschützt (kein installiertes PS4-Spiel): %s", F1[i]));
    pruefe(geschuetzt(k, M2 "/user") && geschuetzt(k, "/user/license") && geschuetzt(k, USB_ERW), "Ordner, die installierte PS4-Spiele enthalten (/mnt/ext1/user, /user/license, /mnt/ext0), sind geschützt");
    pruefe(!geschuetzt(k, "/user/data") && !geschuetzt(k, "/user/home"), "Ordner ohne PS4-Spiele (/user/data, /user/home) nicht");
    { const char *g; kern_ps4_geschuetzt(k, "/user/app/CUSA30002", &g); pruefe(g && strstr(g, "Spiel Zwei") && strstr(g, "CUSA30002") && strstr(g, "intern"), a_fmt(A0, "Grund nennt Name, Kennung, Ort: %s", g ? g : "-")); }

    /* =============================================================== 4. Schutz mit Auswahl */
    printf("== 4. Schutzregeln - Spiel gewählt\n");
    eintrag *e1 = spiel_eintrag(k, "/user|CUSA30001");
    pruefe(e1 != NULL, "Eintrag \"Spiel Eins - Spiel\" gefunden");
    if (e1) {
        e1->Wahl = true; kern_ps4_weg_setzen(k);
        pruefe(!geschuetzt(k, "/user/app/CUSA30001") && !geschuetzt(k, "/user/patch/CUSA30001") && !geschuetzt(k, "/user/license/fakeEP1004-CUSA30001_00.rif") && !geschuetzt(k, "/user/appmeta/CUSA30001"),
               "Spiel Eins gewählt: seine Pfade (App, Update, Lizenz, Kachel-Infos) dürfen weg");
        pruefe(geschuetzt(k, "/user/app/CUSA30002") && geschuetzt(k, "/user/license/CUSA30002.rif"), "Spiel Zwei (nicht gewählt) bleibt geschützt");
        e1->Wahl = false;
    }
    eintrag *e6 = spiel_eintrag(k, "/user|CUSA30006"), *e6m = spiel_eintrag(k, M2 "/user|CUSA30006");
    pruefe(e6 && e6m, "CUSA30006: je ein Eintrag für die interne und die M.2-Kopie");
    /* NEU 1.0.8 (Arbeitsplan 2.4): doppelte Spiele deutlich markieren */
    pruefe(e6 && e6m && e6->Doppelt.n == 1 && e6m->Doppelt.n == 1 && strstr(kern_zeile(A0, e6), " – doppelt: auch installiert auf der M.2-SSD")
           && strstr(kern_zeile(A0, e6m), " – doppelt: auch installiert auf der internen SSD") && strstr(kern_art_info(A0, e6), "Doppelt: Dieses Spiel liegt mehr als einmal"),
           a_fmt(A0, "2.4: beide Kopien als doppelt markiert (%s | %s)", e6 ? kern_zeile(A0, e6) : "-", e6m ? kern_zeile(A0, e6m) : "-"));
    pruefe(e1 && e1->Doppelt.n == 0 && !strstr(kern_zeile(A0, e1), "doppelt") && !strstr(kern_art_info(A0, e1), "Doppelt"), a_fmt(A0, "2.4: Spiel nur einmal da - kein Vermerk (%s)", e1 ? kern_zeile(A0, e1) : "-"));
    { int n9 = 0, d9 = 0; for (int i = 0; i < k->Funde.n; i++) { eintrag *f = k->Funde.e[i]; if (f->Gruppe && strstr(f->Gruppe, "CUSA30009") && ist_teil(f, "Spiel")) { n9++; if (f->Doppelt.n) d9++; } }
      pruefe(n9 >= 1 && d9 == 0, a_fmt(A0, "2.4 (Codex-Prüfung 73): leerer App-Ordner neben der M.2-Installation ist keine zweite Kopie (%d Einträge, %d doppelt)", n9, d9)); }
    if (e6 && e6m) {
        keine_wahl(k); e6->Wahl = true; kern_ps4_weg_setzen(k);
        pruefe(!geschuetzt(k, "/user/app/CUSA30006"), "nur interne Kopie gewählt: /user/app/CUSA30006 darf weg");
        pruefe(geschuetzt(k, M2 "/user/app/CUSA30006"), "... die M.2-Kopie bleibt geschützt");
        pruefe(geschuetzt(k, "/user/license/fakeEP1004-CUSA30006_00.rif") && geschuetzt(k, "/user/appmeta/CUSA30006"), "... gemeinsame Lizenz und Kachel-Infos bleiben (die M.2-Kopie braucht sie)");
        bool ohne = true; for (int j = 0; j < e6->Pfade.n; j++) if (geschuetzt(k, e6->Pfade.e[j])) ohne = false;
        pruefe(ohne, a_fmt(A0, "... der Eintrag der internen Kopie enthält nichts Geschütztes (wird also wirklich gelöscht: %s)", sl_join(A0, &e6->Pfade, ", ")));
        e6m->Wahl = true; kern_ps4_weg_setzen(k);
        pruefe(!geschuetzt(k, "/user/license/fakeEP1004-CUSA30006_00.rif"), "beide Kopien gewählt: gemeinsame Lizenz darf weg");
    }
    keine_wahl(k);
    {   /* Ein gewählter Eintrag, der genau den App-Ordner löscht (z. B. ShadowMount-/Jailbreak-Kachel), IST das Spiel */
        eintrag *x = mit_pfad(k, "/user/data/MeinOrdner");
        if (x) {
            sliste alt = x->Pfade; x->Pfade = (sliste){0}; sl_add(k->a, &x->Pfade, "/user/app/CUSA30002"); x->Wahl = true; kern_ps4_weg_setzen(k);
            pruefe(!geschuetzt(k, "/user/license/CUSA30002.rif"), "gewählter Eintrag mit genau /user/app/CUSA30002 zählt als \"Spiel gewählt\"");
            x->Pfade = alt; x->Wahl = false;
        }
    }
    keine_wahl(k);

    /* =============================================================== 5. Zusammenfassung vor dem Löschen */
    printf("== 5. Zusammenfassung nennt die PS4-Spiele\n");
    {
        static ablauf AB; ablauf_ui ui = { u_wahl, u_text, u_log, u_stand, u_fertig, u_fehler, NULL, NULL };
        ablauf_init(&AB, k, &ui);
        if (e1) e1->Wahl = true;
        const char *z = ablauf_zusammenfassung(&AB, false);
        pruefe(strstr(z, "GELÖSCHT") && strstr(z, "Spiel Eins") && strstr(z, "CUSA30001"), "Zusammenfassung DE: Spiel Eins wird gelöscht (Name + Kennung)");
        pruefe(strstr(z, "geschützt bleiben") != NULL, "Zusammenfassung DE: Zahl der geschützt bleibenden PS4-Spiele");
        SP_EN = true; z = ablauf_zusammenfassung(&AB, false); SP_EN = false;
        pruefe(strstr(z, "DELETED") && strstr(z, "stay protected"), "Zusammenfassung EN");
        keine_wahl(k);
        /* NEU 1.0.8 (F2, da_Chris86s Bilder vom 04.11.2026): nur einzelne Dateien auf dem USB-Laufwerk gewählt - nur die Dateien nennen, keine
         * "!!! ACHTUNG ... was darin liegt"-Warnung (klang nach dem ganzen Laufwerk) */
        eintrag *d1 = mit_pfad(k, "/mnt/usb0/Spiel_Eins.pkg"), *d2 = mit_pfad(k, "/mnt/usb0/kaputt.pkg");
        if (d1 && d2) { d1->Wahl = d2->Wahl = true; kern_ermittle_groessen(k); }   /* wie der Ablauf vor dem Bestätigen: Größen der gewählten */
        pruefe(d1 && d2 && d1 != d2 && d1->HatGroesse && d1->Ordner == 0 && d2->HatGroesse && d2->Ordner == 0,
               a_fmt(A0, "zwei Dateien auf dem USB-Laufwerk angeboten (mit Größe: %d/%d Ordner)", d1 ? d1->Ordner : -1, d2 ? d2->Ordner : -1));
        if (d1 && d2) {
            z = ablauf_zusammenfassung(&AB, false);
            pruefe(strstr(z, "Auf dem USB-Laufwerk werden nur einzelne Dateien gelöscht (2): ") && strstr(z, "Spiel_Eins.pkg") && strstr(z, "kaputt.pkg") && !strstr(z, "ACHTUNG"),
                   "nur Dateien gewählt: die Zusammenfassung nennt die zwei Dateien, keine Warnung vor dem ganzen Laufwerk");
            keine_wahl(k);
        }
    }

    /* =============================================================== 6. Löschen: Schutz greift unmittelbar vor dem Löschen */
    printf("== 6. Löschlauf: falsch zugeordneter Pfad eines nicht gewählten Spiels\n");
    {
        eintrag *mo = mit_pfad(k, "/user/data/MeinOrdner"), *l4 = mit_pfad(k, "/user/license/fakeEP1004-CUSA30004_00.rif");
        pruefe(mo && l4, "Einträge MeinOrdner und Fake-Lizenz CUSA30004 (Spiel nicht installiert) angeboten");
        eintrag *x = mit_pfad(k, "/user/data/Zuordnung");
        pruefe(x != NULL, "Testeintrag /user/data/Zuordnung angeboten");
        if (mo && l4 && x) {
            /* Nachbildung einer Fehlzuordnung: ein gewählter Eintrag enthält zusätzlich die Lizenz von Spiel Zwei */
            sl_add(k->a, &x->Pfade, "/user/license/CUSA30002.rif");
            mo->Wahl = l4->Wahl = x->Wahl = true;
            ergebnis r = kern_fuehre_loeschung(k, ja, NULL);
            pruefe(!da("/user/data/MeinOrdner"), "MeinOrdner gelöscht (gehört zu keinem PS4-Spiel)");
            pruefe(!da("/user/license/fakeEP1004-CUSA30004_00.rif"), "Fake-Lizenz des NICHT installierten CUSA30004 gelöscht");
            pruefe(da("/user/app/CUSA30002/app.pkg") && da("/user/license/CUSA30002.rif"), "Spiel Zwei unberührt (app.pkg und Lizenz da)");
            pruefe(da("/user/data/Zuordnung/a.txt"), "der ganze Eintrag mit dem fremden Pfad blieb (im Zweifel nicht löschen)");
            pruefe(r.Geschuetzt == 1 && k->PS4_GESCHUETZT == 1 && r.Fehler == 0, a_fmt(A0, "Ergebnis: 1 geschützt, 0 Fehler (ist %d/%d)", r.Geschuetzt, r.Fehler));
            pruefe(log_hat("geschützt") && log_hat("Spiel Zwei (CUSA30002"), "Protokoll: \"geschützt ... Spiel Zwei (CUSA30002, intern)\"");
        }
    }

    /* =============================================================== 7. Neu erfasst direkt vor dem Löschen */
    printf("== 7. Inventar wird direkt vor dem Löschen neu erstellt\n");
    tdatei("/user/license/fakeEP1004-CUSA30008_00.rif", 10);
    k = neu();
    {
        eintrag *l8 = mit_pfad(k, "/user/license/fakeEP1004-CUSA30008_00.rif");
        pruefe(l8 != NULL && !spiel(k, "CUSA30008", ORT_INTERN), "Lizenz CUSA30008 angeboten - Spiel zur Suche nicht installiert");
        mkdir(VERB(A0, W, "/user/app/CUSA30008"), 0755); tdatei("/user/app/CUSA30008/app.pkg", 100);   /* inzwischen installiert */
        if (l8) {
            l8->Wahl = true;
            ergebnis r = kern_fuehre_loeschung(k, ja, NULL);
            pruefe(da("/user/license/fakeEP1004-CUSA30008_00.rif"), "Lizenz bleibt: beim Löschen ist CUSA30008 installiert (Neuerfassung greift)");
            pruefe(r.Geschuetzt == 1, a_fmt(A0, "Ergebnis: 1 geschützt (ist %d)", r.Geschuetzt));
        }
    }

    /* =============================================================== 8. Inventar scheitert -> im Zweifel nichts PS4-Bezogenes */
    printf("== 8. Inventar nicht möglich (Ordnerliste /user/app scheitert beim Löschen)\n");
    tdatei("/user/data/Zweiter/a.txt", 1);
    k = neu();
    {
        eintrag *l4 = NULL, *zw = mit_pfad(k, "/user/data/Zweiter");
        tdatei("/user/license/fakeEP1004-CUSA30004_00.rif", 10);   /* wieder da (Abschnitt 6 hat sie gelöscht) */
        k = neu(); zw = mit_pfad(k, "/user/data/Zweiter"); l4 = mit_pfad(k, "/user/license/fakeEP1004-CUSA30004_00.rif");
        pruefe(l4 && zw, "Fake-Lizenz CUSA30004 und Ordner Zweiter angeboten");
        if (l4 && zw) {
            l4->Wahl = zw->Wahl = true;
            setenv("FAILLIST", "/user/app", 1);   /* Merkdatei faillist.on liegt in <hier>, nicht im Testbaum */
            FILE *f = fopen(VERB(A0, HIER, "/faillist.on"), "w"); if (f) fclose(f);
            ergebnis r = kern_fuehre_loeschung(k, ja, NULL);
            unlink(VERB(A0, HIER, "/faillist.on")); unsetenv("FAILLIST");
            pruefe(k->PS4_UNKLAR, "Inventar gescheitert -> PS4_UNKLAR");
            pruefe(da("/user/license/fakeEP1004-CUSA30004_00.rif"), "Fake-Lizenz bleibt (könnte zu einem PS4-Spiel gehören)");
            pruefe(!da("/user/data/Zweiter"), "Ordner ohne PS4-Bezug wird trotzdem gelöscht");
            pruefe(r.Geschuetzt == 1, a_fmt(A0, "Ergebnis: 1 geschützt (ist %d)", r.Geschuetzt));
        }
    }

    /* =============================================================== 9. nicht lesbarer Spielordner */
    printf("== 9. Spielordner nicht lesbar\n");
    k = neu();
    {
        setenv("FAILLIST", "/user/app/CUSA30002", 1);
        FILE *f = fopen(VERB(A0, HIER, "/faillist.on"), "w"); if (f) fclose(f);
        kern_ps4_erfassen(k);
        unlink(VERB(A0, HIER, "/faillist.on")); unsetenv("FAILLIST");
        const ps4_spiel *u = spiel(k, "CUSA30002", ORT_INTERN);
        pruefe(u && u->unklar && kern_ps4_installiert(u), "CUSA30002: Ordner nicht lesbar = unklar = gilt als installiert");
        keine_wahl(k); kern_ps4_weg_setzen(k);
        pruefe(geschuetzt(k, "/user/license/CUSA30002.rif"), "... seine Lizenz bleibt geschützt");
    }

    /* =============================================================== 10. Simulation */
    printf("== 10. Simulation: gleiche Schutzregeln, nichts gelöscht\n");
    {
        datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = HIER };
        kern *ks = kern_neu(zugriff_sim(zugriff_datei(&o))); ks->melde = melde_log; ks->simulation = true; LOG = (sliste){0};
        VERSUCHE { kern_suche_funde(ks); } FANGE { printf("FAIL Suche (Simulation): %s\n", ausn_text); fehler++; } ENDE_VERSUCHE;
        eintrag *x = mit_pfad(ks, "/user/data/Zuordnung");
        if (x) {
            sl_add(ks->a, &x->Pfade, "/user/patch/CUSA30001");
            x->Wahl = true;
            ergebnis r = kern_fuehre_loeschung(ks, ja, NULL);
            pruefe(r.Geschuetzt == 1 && log_hat("geschützt"), "Simulation meldet den Eintrag als geschützt");
            pruefe(da("/user/data/Zuordnung/a.txt") && da("/user/patch/CUSA30001/patch.pkg"), "Simulation: nichts gelöscht");
        } else pruefe(false, "Testeintrag in der Simulation gefunden");
    }

    /* =============================================================== 11. Ende ohne Neuaufbau-Empfehlung */
    printf("== 11. Hinweis am Ende: kein Datenbank-Neuaufbau, statt dessen Protokoll schicken\n");
    k = neu();
    {
        k->SONY = true; k->GEISTER = (sliste){0}; sl_add(k->a, &k->GEISTER, "PS5Upload Minion (PSUP00001)");
        const char *t = kern_rest_text(k, A0);
        pruefe(strstr(t, "PSUP00001") && strstr(t, "unverändert"), "nennt den verbliebenen Eintrag, lässt ihn unverändert");
        /* da_Chris86 02.11.2026 15:54: kein Wort mehr zum Datenbank-Neuaufbau - statt dessen das Protokoll an uns schicken */
        pruefe(strstr(t, "schick uns das Protokoll") != NULL, "bittet darum, uns das Protokoll zu schicken");
        pruefe(!strstr(t, "Datenbank") && !strstr(t, "Neuaufbau") && !strstr(t, "abgesicherten Modus"), "kein Wort zum Datenbank-Neuaufbau");
        pruefe(strstr(t, "CUSA30001") == NULL, "keine PS4-Liste mehr (die gehörte zum Neuaufbau)");
        SP_EN = true; const char *te = kern_rest_text(k, A0); SP_EN = false;
        pruefe(strstr(te, "send us the log") && strstr(te, "PSUP00001") && !strstr(te, "database"), "EN: gleicher Inhalt");
        k->GEISTER = (sliste){0}; k->SONY = true;
        pruefe(kern_rest_text(k, A0)[0] == 0, "alles sauber entfernt: kein Hinweis");
        k->SONY = false;
        pruefe(strstr(kern_rest_text(k, A0), "schick uns das Protokoll") != NULL, "ohne Sony-Dienst (PC): Hinweis mit Bitte ums Protokoll");
        pruefe(!k->DB_HINWEIS, "DB_HINWEIS (alte Empfehlung) im 1.0.7-Modus aus");
    }

    /* =============================================================== 12. Vorbereitung vor dem Löschen schlägt fehl -> nichts gelöscht */
    printf("== 12. Bedienablauf: Vorbereitung direkt vor dem Löschen (vor_loeschen) schlägt fehl -> NICHTS gelöscht\n");
    tdatei("/user/data/Dritter/a.txt", 1);
    k = neu();
    {
        static ablauf AB; ablauf_ui ui = { u_wahl, u_text, u_log, u_stand, u_fertig, u_fehler, NULL, NULL };
        ablauf_init(&AB, k, &ui);
        k->melde = melde_log; AB.protokoll_datei = "(Protokoll)";
        AB.vor_loeschen = vorbereitung_scheitert;
        eintrag *dr = mit_pfad(k, "/user/data/Dritter");
        pruefe(dr != NULL, "Ordner Dritter angeboten");
        if (dr) {
            ablauf_eintrag_setzen(&AB, dr, true);
            ablauf_hintergrund_anhalten(&AB);
            kern_groessen_im_hintergrund(k, nie, NULL, nichts, NULL);
            ablauf_starte(&AB, (void (*)(void *))ablauf_auswahl, &AB);
            ablauf_hintergrund_anhalten(&AB);
            pruefe(vor_loeschen_gerufen == 1, a_fmt(A0, "vor_loeschen wurde direkt vor dem Löschen aufgerufen (%d x)", vor_loeschen_gerufen));
            pruefe(da("/user/data/Dritter/a.txt"), "Ordner Dritter NICHT gelöscht");
            pruefe(log_hat("NICHTS gelöscht"), "Protokoll: \"Es wurde NICHTS gelöscht.\"");
        }
    }

    /* =============================================================== 13. Datenbanken nur lesen und mit SQLite prüfen */
    printf("== 13. Datenbanken (app.db, appinfo.db) nur lesen und mit echter SQLite-Prüfung (seit 1.0.8 ohne Sicherung)\n");
    {
        const char *mms = VERB(A0, HIER, "/mms");
        const char *ber;
        struct stat st0, st1; stat(VERB(A0, mms, "/app.db"), &st0);
        bool ok = db_beide_pruefen(A0, mms, &ber);
        pruefe(ok && strstr(ber, "integrity_check) ok") && strstr(ber, "Backup-Schnittstelle ok"), a_fmt(A0, "lesen und prüfen: ok\n%s", ber));
        pruefe(strstr(ber, "WAL-Modus") && strstr(ber, "Einträge eingerechnet"), "app.db im WAL-Modus: abgeschlossene WAL-Einträge eingerechnet");
        stat(VERB(A0, mms, "/app.db"), &st1);
        pruefe(st0.st_size == st1.st_size && st0.st_mtime == st1.st_mtime, "Original app.db unverändert (Größe, Zeitstempel)");
        char cmd[900];
        snprintf(cmd, sizeof cmd, "cd '%s' && sha1sum app.db app.db-wal appinfo.db appinfo.db-journal | cmp -s - '%s/mms.sha1'", mms, HIER);
        pruefe(system(cmd) == 0, "Originale samt WAL/Journal unverändert (Prüfsumme wie vor dem Test)");
        struct { const char *ordner, *soll, *was; } F[] = {
            { "/mms_kaputt", "FEHLGESCHLAGEN", "abgeschnittene app.db" },
            { "/mms_fehlt", "appinfo.db - fehlt", "fehlende appinfo.db" },
            { "/mms_fremd", "kein SQLite-Dateikopf", "keine SQLite-Datei" },
            { "/mms_journal", "schreibt gerade", "heißes Rollback-Journal (Konsole schreibt gerade)" },
            { "/mms_defekt", "integrity_check", "beschädigter Index (integrity_check)" },
            { "/mms_walkaputt", "WAL", "WAL-Datei mit falscher Prüfsumme im Kopf" } };
        for (size_t i = 0; i < sizeof F / sizeof F[0]; i++) {
            ok = db_beide_pruefen(A0, VERB(A0, HIER, F[i].ordner), &ber);
            pruefe(!ok && strstr(ber, F[i].soll) && strstr(ber, "Datenbank-Prüfung FEHLGESCHLAGEN"), a_fmt(A0, "%s erkannt: %s", F[i].was, ber));
        }
        ok = db_beide_pruefen(A0, VERB(A0, HIER, "/mms_walmuell"), &ber);
        pruefe(ok && strstr(ber, "1 nicht abgeschlossen"), a_fmt(A0, "WAL mit abgebrochenem letzten Eintrag: Eintrag verworfen, Rest ok (%s)", ber));
        SP_EN = true; ok = db_beide_pruefen(A0, mms, &ber); SP_EN = false;
        pruefe(ok && strstr(ber, "SQLite check (integrity_check) ok"), "Bericht auf Englisch");
        SP_EN = true; ok = db_beide_pruefen(A0, VERB(A0, HIER, "/mms_kaputt"), &ber); SP_EN = false;
        pruefe(!ok && strstr(ber, "Database check FAILED"), a_fmt(A0, "Fehlermeldung auf Englisch: %s", ber));
    }

    /* =============================================================== 14. Geisterkacheln: Regel */
    printf("== 14. Geisterkacheln - Sonderbehandlung nur für belegte Fälle\n");
    {
        const char *g;
        pruefe(kern_geist_regel("PPSA01234", 0, NULL, NULL, NULL, &g) == GEIST_NORMAL, "keine Einbindung: normal (ein Sony-Auftrag wie im echten Test)");
        pruefe(kern_geist_regel("PSUP00001", 1, "nullfs", "/data/ps5upload/tile/PSUP00001", NULL, &g) == GEIST_PS5UPLOAD, "PSUP00001 + nullfs von /data/ps5upload/tile/PSUP00001: ps5upload-Behandlung");
        pruefe(kern_geist_regel("PSUP00001", 1, "nullfs", "/user/data/ps5upload/tile/PSUP00001/", NULL, &g) == GEIST_PS5UPLOAD, "... auch als /user/data/... mit Schrägstrich am Ende");
        pruefe(kern_geist_regel("PSUP00001", 1, "nullfs", "/mnt/shadowmnt/PSUP00001", NULL, &g) == GEIST_UNVERAENDERT, "PSUP00001 aber andere Quelle: unverändert");
        pruefe(kern_geist_regel("PSUP00001", 1, "unionfs", "/data/ps5upload/tile/PSUP00001", NULL, &g) == GEIST_UNVERAENDERT, "PSUP00001 aber anderes Dateisystem: unverändert");
        pruefe(kern_geist_regel("PPSA01234", 1, "nullfs", "/mnt/shadowmnt/x.exfat", NULL, &g) == GEIST_UNVERAENDERT && strstr(g, "ShadowMount+"), "ShadowMount+-Einbindung noch aktiv: unverändert");
        pruefe(kern_geist_regel("PPSA01234", 1, "nullfs", "/data/ps5upload/tile/PSUP00001", NULL, &g) == GEIST_UNVERAENDERT, "ps5upload-Quelle unter fremder Kennung: unverändert");
        pruefe(kern_geist_regel("PPSA01234", -1, NULL, NULL, NULL, &g) == GEIST_UNVERAENDERT, "Zustand nicht feststellbar: unverändert");
        pruefe(kern_geist_regel("", 0, NULL, NULL, NULL, &g) == GEIST_UNVERAENDERT, "Kennung fehlt: unverändert");
        /* NEU 1.0.7 (PS5-Test 01.11.2026 23:13, da_Chris86 23:19): Ordner-Spiel, von kstuff eingebunden */
        const char *f1 = "/mnt/ext1/homebrew/F1® 25 - PPSA24662 - v1.26.0 - US";
        pruefe(kern_geist_regel("PPSA24662", 1, "nullfs", f1, f1, &g) == GEIST_EIGENES_SPIEL && strstr(g, "Spielordner"), "Ordner-Spiel: Einbindung genau des eigenen Spielordners -> lösen, dann Sony");
        pruefe(kern_geist_regel("PPSA24662", 1, "nullfs", "/mnt/ext1/homebrew/F1® 25 - PPSA24662 - v1.26.0 - US/", f1, &g) == GEIST_EIGENES_SPIEL, "... auch mit Schrägstrich am Ende");
        pruefe(kern_geist_regel("PPSA12345", 1, "nullfs", "/data/homebrew/Spiel - PPSA12345", "/user/data/homebrew/Spiel - PPSA12345", &g) == GEIST_EIGENES_SPIEL, "... /data und /user/data sind derselbe Ordner");
        pruefe(kern_geist_regel("PPSA12345", 1, "nullfs", "/mnt/usb0/Spiel - PPSA12345", "/mnt/usb0/Spiel - PPSA12345", &g) == GEIST_EIGENES_SPIEL, "... auch auf dem USB-Stick");
        pruefe(kern_geist_regel("PPSA24662", 1, "nullfs", "/mnt/ext1/homebrew/Anderes Spiel - PPSA99999", f1, &g) == GEIST_UNVERAENDERT, "anderer Ordner als der der Kachel: unverändert");
        pruefe(kern_geist_regel("PPSA24662", 1, "nullfs", f1, NULL, &g) == GEIST_UNVERAENDERT, "Kachel nennt keinen Spielordner: unverändert");
        pruefe(kern_geist_regel("PPSA24662", 1, "unionfs", f1, f1, &g) == GEIST_UNVERAENDERT, "anderes Dateisystem: unverändert");
        pruefe(kern_geist_regel("PPSA24662", 1, "nullfs", "/mnt/shadowmnt/PPSA24662", "/mnt/shadowmnt/PPSA24662", &g) == GEIST_UNVERAENDERT, "Abbild-Einhängepunkt von ShadowMount: unverändert");
        pruefe(kern_geist_regel("PPSA24662", 1, "nullfs", "/system/vsh/app", "/system/vsh/app", &g) == GEIST_UNVERAENDERT, "Systemordner: unverändert");
        pruefe(kern_geist_regel("PPSA24662", 1, "nullfs", "/mnt/ext1/homebrew/../user", "/mnt/ext1/homebrew/../user", &g) == GEIST_UNVERAENDERT, "ungewöhnliche Schreibweise (..): unverändert");
        pruefe(kern_geist_regel("PPSA24662", 1, "nullfs", "/mnt/ext1/", "/mnt/ext1/", &g) == GEIST_UNVERAENDERT, "ganze M.2 statt Spielordner: unverändert");
        pruefe(kern_geist_regel("PPSA24662", -1, NULL, NULL, f1, &g) == GEIST_UNVERAENDERT, "Zustand nicht feststellbar: weiterhin unverändert");
    }

    /* =============================================================== 15. Löschlauf: Kachel mit unklarer Einbindung bleibt ganz */
    printf("== 15. Löschlauf: Eintrag mit unklarer Einbindung bleibt unverändert\n");
    tdatei("/user/data/Vierter/a.txt", 1);
    k = neu();
    {
        eintrag *ps5 = mit_pfad(k, "/user/app/PPSA10001");
        pruefe(ps5 && ps5->KachelId && !strcmp(ps5->KachelId, "PPSA10001"), "PS5-Spiel PPSA10001 mit Kachel-Kennung angeboten");
        if (ps5) {
            EINB_ID = "PPSA10001"; EINB_QUELLE = "/mnt/shadowmnt/Spiel.exfat";
            zugriff_einbindung = test_einbindung;
            ps5->Wahl = true;
            eintrag *mo = mit_pfad(k, "/user/data/Vierter"); if (mo) mo->Wahl = true;
            ergebnis r = kern_fuehre_loeschung(k, ja, NULL);
            zugriff_einbindung = NULL;
            pruefe(da("/user/app/PPSA10001") && da("/user/appmeta/PPSA10001/param.json"), "PPSA10001 komplett unverändert (App-Ordner und Kachel-Infos da)");
            pruefe(r.Unveraendert == 1 && k->UNVERAENDERT.n == 1, a_fmt(A0, "Ergebnis: 1 unverändert (ist %d)", r.Unveraendert));
            pruefe(log_hat("unverändert") && log_hat("ShadowMount+"), "Protokoll: \"unverändert ... ShadowMount+ ...\"");
            pruefe(!mo || !da("/user/data/Vierter"), "anderer gewählter Eintrag ohne Einbindung wurde normal gelöscht");
            const char *t = kern_rest_text(k, A0);
            pruefe(strstr(t, "Unverändert gelassen") && strstr(t, "PPSA10001") && strstr(t, "schick uns das Protokoll"), a_fmt(A0, "Hinweis am Ende nennt den unveränderten Eintrag: %s", t));
        }
    }

    /* =============================================================== 16. PKG-Dateien: Spielname aus dem PKG (NEU 1.0.8, Arbeitsplan 2.2) */
    printf("== 16. PKG-Dateien auf dem USB-Stick: Spielname und Spielnummer aus dem PKG\n");
    k = neu();
    {
        eintrag *p1 = mit_pfad(k, "/mnt/usb0/Spiel_Eins.pkg"), *p2 = mit_pfad(k, "/mnt/usb0/Update_Eins.pkg"), *p3 = mit_pfad(k, "/mnt/usb0/kaputt.pkg");
        pruefe(p1 && strstr(p1->Text, "Spiel_Eins.pkg") && strstr(p1->Text, "[PKG: Spiel Eins (CUSA30001)]"), a_fmt(A0, "Spiel-PKG: Name und Nummer aus dem PKG (%s)", p1 ? p1->Text : "nicht angeboten"));
        pruefe(p2 && strstr(p2->Text, "[PKG: Spiel Eins (CUSA30001), Update 01.05]"), a_fmt(A0, "Update-PKG: als Update erkannt (%s)", p2 ? p2->Text : "nicht angeboten"));
        pruefe(p3 && strstr(p3->Text, "kaputt.pkg") && !strstr(p3->Text, "[PKG"), a_fmt(A0, "kein echtes PKG: nur der Dateiname wie bisher (%s)", p3 ? p3->Text : "nicht angeboten"));
        /* Codex-Prüfung 73: der Name aus dem PKG steht auch in der Zeile der Auswahl. 1.0.8 (PS5-Test 06.11.2026, Befund 12): CUSA30001 ist
         * installiert - die PKG-Dateien stehen in seinem Block ("Spiel Eins – Installationsdatei", "Spiel Eins – Update-Datei 01.05"), rot */
        pruefe(p1 && strstr(kern_zeile(A0, p1), "USB-Laufwerk – Spiel Eins – Installationsdatei") && !strcmp(kern_name(A0, p1), "Spiel Eins") && p1->Kat == 1
               && p1->ToolKachel && kern_ampel(p1) == AMPEL_ROT,
               a_fmt(A0, "Zeile der Auswahl: im Block des installierten Spiels (%s, Name: %s, Bereich %d)", p1 ? kern_zeile(A0, p1) : "-", p1 ? kern_name(A0, p1) : "-", p1 ? p1->Kat : -1));
        pruefe(p2 && strstr(kern_zeile(A0, p2), "Spiel Eins – Update-Datei 01.05") && !strcmp(kern_name(A0, p2), "Spiel Eins") && p2->ToolKachel == p1->ToolKachel,
               a_fmt(A0, "Update-PKG in der Zeile: %s", p2 ? kern_zeile(A0, p2) : "-"));
        eintrag *p4 = mit_pfad(k, "/mnt/usb0/Zusatz_Eins.pkg");
        pruefe(p4 && strstr(kern_zeile(A0, p4), "Spiel Eins – Zusatzinhalt-Datei: Bonuspaket") && p4->ToolKachel == (p1 ? p1->ToolKachel : NULL),
               a_fmt(A0, "Zusatzinhalt-PKG mit seinem eigenen Namen im Block des Spiels: %s", p4 ? kern_zeile(A0, p4) : "-"));
        {   /* Codex-Prüfung 71: Sonderfälle und Lesegrenzen */
            struct { const char *p, *soll, *was; } P[] = {
                { "/mnt/usb0/Zusatz_Eins.pkg", "[PKG: Bonuspaket (CUSA30001), Zusatzinhalt]", "Zusatzinhalt erkannt" },
                { "/mnt/usb0/grenze.pkg", "[PKG: Spiel Grenze (CUSA30001)]", "param.sfo über der 64-KB-Grenze gelesen" },
                { "/mnt/usb0/ohne_id.pkg", "[PKG: Spiel Ohne (CUSA30002)]", "Spielnummer aus der Inhaltskennung im Kopf" },
                { "/mnt/usb0/ohne_sfo.pkg", NULL, "PKG ohne param.sfo: nur der Dateiname" },
                { "/mnt/usb0/sfo_gross.pkg", NULL, "param.sfo zu groß: nur der Dateiname" },
                { "/mnt/usb0/abgeschnitten.pkg", NULL, "abgeschnittene Datei: nur der Dateiname" },
                { "/mnt/usb0/weit.pkg", "[PKG: X (CUSA30001)]", "param.sfo hinter 1 MB (wie bei großen echten PKGs, 05.11.2026): gezielt gelesen" },
                { "/mnt/usb0/viele.pkg", NULL, "Eintragstabelle mit 9000 Einträgen (mehr als 8192): nur der Dateiname" },
                { "/mnt/usb0/sfo_weg.pkg", NULL, "param.sfo zeigt hinter das Dateiende: nur der Dateiname" } };
            for (size_t i = 0; i < sizeof P / sizeof P[0]; i++) {
                eintrag *q = mit_pfad(k, P[i].p);
                pruefe(q && (P[i].soll ? strstr(q->Text, P[i].soll) != NULL : !strstr(q->Text, "[PKG")), a_fmt(A0, "%s (%s)", P[i].was, q ? q->Text : "nicht angeboten"));
            }
        }
        {   /* Codex-Prüfung 72: gelesen wird nur der Anfang, ein manipuliertes param.sfo bleibt klein */
            size_t g = 0; const char *z = kern_pkg_zusatz_test(k, "/mnt/usb0/Spiel_Eins.pkg", &g);
            pruefe(z[0] && g > 0 && g <= 4096, a_fmt(A0, "Spiel-PKG (gut 200 KB): nur Kopf, Eintragstabelle und param.sfo gelesen (%zu Byte)", g));
            z = kern_pkg_zusatz_test(k, "/mnt/usb0/weit.pkg", &g);
            pruefe(z[0] && g > 0 && g <= 4096, a_fmt(A0, "param.sfo hinter 1 MB (Datei gut 2 MB): direkt hingesprungen (%zu Byte gelesen)", g));
            z = kern_pkg_zusatz_test(k, "/mnt/usb0/viele.pkg", &g);
            pruefe(!z[0] && g <= 0x60, a_fmt(A0, "9000 Einträge: nach dem Kopf aufgehört (%zu Byte)", g));
            z = kern_pkg_zusatz_test(k, "/mnt/usb0/boese.pkg", &g);
            pruefe(z[0] && strlen(z) < 200 && g <= 2 * 65536, a_fmt(A0, "manipuliertes param.sfo (300 Einträge auf 60 KB Text): Anzeige begrenzt (%zu Zeichen, %zu Byte gelesen)", strlen(z), g));
        }
        SP_EN = true; k = neu(); SP_EN = false;
        p2 = mit_pfad(k, "/mnt/usb0/Update_Eins.pkg");
        pruefe(p2 && strstr(p2->Text, "[PKG: Spiel Eins (CUSA30001), update 01.05]"), a_fmt(A0, "EN: als update erkannt (%s)", p2 ? p2->Text : "nicht angeboten"));
    }

    /* =============================================================== 16b. Spiel-Abbilder: Spielname aus dem exFAT-Abbild (NEU 1.0.8, Arbeitsplan 2.1) */
    printf("== 16b. Spiel-Abbilder mit nur der Spielnummer im Namen: Spielname aus dem Abbild\n");
    {
        kern *k = neu();
        struct { const char *pfad, *soll, *was; } B[] = {
            { "/mnt/usb0/PPSA30010.exfat", "[Spiel: Spiel Abbild (PPSA30010)]", "exFAT-Abbild, param.json über zwei Cluster (FAT-Kette)" },
            { "/mnt/usb0/PPSA30011-app.exfat", "[Spiel: Am Stueck (PPSA30011)]", "zusammenhängend gespeichert (ohne FAT-Kette)" },
            { "/mnt/usb0/PPSA30012.exfat", "[Spiel: Im Unterordner (PPSA30012)]", "Spiel in einem einzigen Unterordner" },
            { "/mnt/usb0/CUSA30020.exfat", "[Spiel: PS4 Abbild (CUSA30020)]", "PS4-Spiel im Abbild (param.sfo)" },
            { "/mnt/usb0/PPSA30015.exfat", "[Spiel: Schleife (PPSA30015)]", "FAT-Kette im Kreis: Verzeichnis nur begrenzt gelesen" },
            { "/mnt/usb0/PPSA30013.img", NULL, "kein exFAT: nur der Dateiname" },
            { "/mnt/usb0/PPSA30014.exfat", NULL, "kaputter Kopf: nur der Dateiname" },
            { "/mnt/usb0/PPSA30016.exfat", NULL, "param.json mit 4 GB Länge: nicht gelesen" },
            { "/mnt/usb0/PPSA30017.exfat", NULL, "kein sce_sys: nur der Dateiname" },
            { "/mnt/usb0/PPSA30019.exfat", NULL, "abgeschnittenes Abbild: nur der Dateiname" },
            /* geändert 07.11.2026 (PS5-Test 16:31): das Abbild wird immer gelesen - Name und Bild kommen aus dem Spiel selbst */
            { "/mnt/usb0/Spiel Mit Namen - PPSA30018.exfat", "[Spiel: Anderer Name (PPSA30018)]", "Name im Dateinamen: das Abbild wird trotzdem gelesen" },
            { "/mnt/usb0/Mein Spiel.exfat", "[Spiel: Ohne Nummer (PPSA30021)]", "Dateiname ohne Spielnummer: Name und Kennung aus dem Abbild" },
            { "/mnt/usb0/PPSA30022.exfat", "[Spiel: Bild Kette (PPSA30022)]", "Bild über zwei Cluster hintereinander" },
            { "/mnt/usb0/PPSA30023.exfat", "[Spiel: Bild Verstreut (PPSA30023)]", "Bild über zwei verstreute Cluster" } };
        for (size_t i = 0; i < sizeof B / sizeof *B; i++) {
            eintrag *q = mit_pfad(k, B[i].pfad);
            pruefe(q && strstr(q->Text, "[ohne Kachel]") && (B[i].soll ? strstr(q->Text, B[i].soll) != NULL : !strstr(q->Text, "[Spiel:")),
                   a_fmt(A0, "%s (%s)", B[i].was, q ? q->Text : "nicht angeboten"));
        }
        /* Codex-Prüfung 73: der gelesene Name muss auch in der Zeile der Auswahl stehen (bis dahin schnitt kern_zeile ihn ab) */
        { eintrag *q = mit_pfad(k, "/mnt/usb0/PPSA30010.exfat"), *q2 = mit_pfad(k, "/mnt/usb0/PPSA30013.img");
          pruefe(q && strstr(kern_zeile(A0, q), "Spieldatei ohne Kachel: Spiel Abbild (PPSA30010.exfat)") && !strcmp(kern_name(A0, q), "Spiel Abbild"),
                 a_fmt(A0, "Zeile der Auswahl zeigt den Spielnamen: %s (Name: %s)", q ? kern_zeile(A0, q) : "-", q ? kern_name(A0, q) : "-"));
          pruefe(q2 && strstr(kern_zeile(A0, q2), "PPSA30013.img") && !strstr(kern_zeile(A0, q2), "("), a_fmt(A0, "ohne gelesenen Namen bleibt der Dateiname: %s", q2 ? kern_zeile(A0, q2) : "-")); }
        /* NEU 1.0.8 (PS5-Test 07.11.2026 16:31): die Kennung aus dem Abbild steht nicht in der Zeile, nur der Name; das Bild (icon0.png am Stück)
         * merkt sich die Suche für die Oberfläche: Abbild, Anfang (Heap 64 x 512 + 9 Cluster x 4096 = 69632), Länge 50 */
        { eintrag *q = mit_pfad(k, "/mnt/usb0/Mein Spiel.exfat");
          pruefe(q && strstr(kern_zeile(A0, q), "Spieldatei ohne Kachel: Ohne Nummer (Mein Spiel.exfat)") && !strcmp(kern_name(A0, q), "Ohne Nummer"),
                 a_fmt(A0, "Zeile ohne die angehängte Kennung: %s (Name: %s)", q ? kern_zeile(A0, q) : "-", q ? kern_name(A0, q) : "-"));
          const char *b1 = mt_hat(&k->BILD_ORT, "PPSA30010") ? mt_hole(&k->BILD_ORT, "PPSA30010") : "(keins)";
          const char *b2 = mt_hat(&k->BILD_ORT, "PPSA30021") ? mt_hole(&k->BILD_ORT, "PPSA30021") : "(keins)";
          pruefe(!strcmp(b1, "/mnt/usb0/PPSA30010.exfat\n69632\n50"), a_fmt(A0, "Bild der Spieldatei: Stelle im Abbild gemerkt (%s)", b1));
          pruefe(!strcmp(b2, "/mnt/usb0/Mein Spiel.exfat\n69632\n50"), a_fmt(A0, "Bild der Spieldatei ohne Nummer im Namen (%s)", b2));
          pruefe(!mt_hat(&k->BILD_ORT, "PPSA30013") && !mt_hat(&k->BILD_ORT, "PPSA30017"), "ohne lesbaren Namen kein Bild");
          const char *b3 = mt_hat(&k->BILD_ORT, "PPSA30022") ? mt_hole(&k->BILD_ORT, "PPSA30022") : "(keins)";
          pruefe(!strcmp(b3, "/mnt/usb0/PPSA30022.exfat\n69632\n5000"), a_fmt(A0, "Bild über zwei Cluster hintereinander (FAT-Kette geprüft): gemerkt (%s)", b3));
          pruefe(!mt_hat(&k->BILD_ORT, "PPSA30023"), "Bild über verstreute Cluster: nicht gemerkt (die Oberfläche liest nur ein Stück am Stück)"); }
        size_t g = 0; const char *z = kern_abbild_titel_test(k, "/mnt/usb0/PPSA30010.exfat", &g);
        pruefe(!strcmp(z, "Spiel Abbild") && g > 0 && g <= 32768, a_fmt(A0, "exFAT-Abbild (knapp 300 KB): nur kleine Stücke gelesen (%zu Byte)", g));
        z = kern_abbild_titel_test(k, "/mnt/usb0/PPSA30015.exfat", &g);
        pruefe(g <= 300000, a_fmt(A0, "FAT-Kette im Kreis: Lesen begrenzt (%zu Byte)", g));
        SP_EN = true; k = neu(); SP_EN = false;
        eintrag *q = mit_pfad(k, "/mnt/usb0/PPSA30010.exfat");
        pruefe(q && strstr(q->Text, "[game: Image Game (PPSA30010)]") && strstr(q->Text, "[no tile]"), a_fmt(A0, "EN: englischer Name (%s)", q ? q->Text : "nicht angeboten"));
    }

    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
