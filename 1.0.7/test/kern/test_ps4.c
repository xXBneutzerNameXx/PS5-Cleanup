/* PS5 Cleanup 1.0.7 - Test PS4-Spieleschutz, Datenbank-Sicherung, Ende ohne Neuaufbau-Empfehlung
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_ps4 <hier>   Testbaum <hier>/w (lauf_ps4.sh), Datenbanken <hier>/mms*, Stick <hier>/stick
 * Auftrag da_Chris86 31.10.2026 19:56 (Punkte 1, 2, 4, 6). Alles auf nachgebildeten Dateisystemen - nie auf einer PS5.
 * Jede Prüfung druckt OK/FAIL, am Ende FEHLERZAHL. */
#include "../../kern/kern.h"
#include "../../kern/ablauf.h"
#include "../../db/db_lesen.h"
#include <stdio.h>
#include <stdint.h>
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
/* NEU 1.0.9 (Codex-Prüfung 130): Testdaten ganz lesen (malloc) und eine Zahl aus zlib_faelle.bin */
static unsigned char *ganz_lesen(const char *p, size_t *n)
{
    *n = 0; FILE *f = fopen(p, "rb"); if (!f) return NULL;
    unsigned char *d = NULL; size_t g = 0, cap = 0, r;
    do { if (g == cap) { cap = cap ? cap * 2 : 65536; unsigned char *x = realloc(d, cap); if (!x) { free(d); fclose(f); return NULL; } d = x; }
         r = fread(d + g, 1, cap - g, f); g += r; } while (r > 0);
    fclose(f); *n = g; return d;
}
/* NEU 1.0.9 (#149): das Bild nach BILD_ORT über kern_bild_lesen lesen (wie die Oberfläche) und mit der Kopie des Bildes vergleichen */
static bool bild_gleich(kern *k, const char *id, int teile, bool gepackt, const char *kopie, const char **ort)
{
    *ort = mt_hat(&k->BILD_ORT, id) ? mt_hole(&k->BILD_ORT, id) : "(keins)";
    kern_bild_ort b; size_t rn = 0;
    if (!kern_bild_ort_lesen(*ort, &b) || b.teile_n != teile || b.gepackt != gepackt) return false;
    unsigned char *d = kern_bild_lesen(VERB(A0, W, b.pfad), &b), *r = ganz_lesen(VERB(A0, HIER, kopie), &rn);
    bool ok = d && r && rn == b.len && !memcmp(d, r, rn);
    free(d); free(r);
    return ok;
}
static bool hole32(const unsigned char *d, size_t n, size_t *i, uint32_t *v)
{
    if (n - *i < 4) return false;
    *v = (uint32_t)d[*i] | (uint32_t)d[*i + 1] << 8 | (uint32_t)d[*i + 2] << 16 | (uint32_t)d[*i + 3] << 24; *i += 4; return true;
}

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
        {   /* NEU 1.0.9 (da_Chris86 08.11.2026 01:19): Bild aus der PKG-Datei (icon0.png, Eintrag 0x1200 bei 0x3800 = 14336, 1024 Byte) */
            struct { const char *id, *soll, *was; } B[] = {
                { "CUSA30040", "/mnt/usb0/Bild_B_Spiel.pkg\n14336\n1024", "die Spiel-PKG geht vor dem Update (das Update kam zuerst)" },
                { "CUSA30041", "/mnt/usb0/Bild_Nur_Update.pkg\n14336\n1024", "nur ein Update da: dessen Bild" },
                { "CUSA30042", NULL, "Bild verschlüsselt: kein Bild" },
                { "CUSA30043", NULL, "Bild ohne PNG-Anfang: kein Bild" },
                { "CUSA30044", NULL, "nur ein Zusatzinhalt: sein eigenes Bild zählt nicht" } };
            for (size_t i = 0; i < sizeof B / sizeof *B; i++) {
                const char *b = mt_hat(&k->BILD_ORT, B[i].id) ? mt_hole(&k->BILD_ORT, B[i].id) : NULL;
                pruefe(B[i].soll ? b && !strcmp(b, B[i].soll) : !b, a_fmt(A0, "PKG-Bild: %s (%s)", B[i].was, b ? b : "keins"));
            }
            const char *b = mt_hat(&k->BILD_ORT, "CUSA30001") ? mt_hole(&k->BILD_ORT, "CUSA30001") : NULL;
            pruefe(b && (strstr(b, "/Spiel_Eins.pkg\n") || strstr(b, "/grenze.pkg\n")) && !strstr(b, "Update") && !strstr(b, "Zusatz"),
                   a_fmt(A0, "PKG-Bild: Spiel, Update und Zusatzinhalt - das Bild einer Spiel-PKG (%s)", b ? b : "keins"));
            size_t g = 0; kern_pkg_zusatz_test(k, "/mnt/usb0/Bild_B_Spiel.pkg", &g);
            pruefe(g > 0 && g <= 4096, a_fmt(A0, "PKG-Bild: vom Bild nur der Anfang gelesen (%zu Byte insgesamt)", g));
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
          const char *o = "";   /* NEU 1.0.9 (#149): Bilder in Stücken - gelesen wie von der Oberfläche, Byte für Byte wie das Bild */
          bool g = bild_gleich(k, "PPSA30023", 2, false, "/bild_PPSA30023.png", &o);
          pruefe(g, a_fmt(A0, "Bild über verstreute Cluster: in 2 Stücken gemerkt und gelesen (%s)", o));
          g = bild_gleich(k, "PPSA30024", 3, false, "/bild_PPSA30024.png", &o);
          pruefe(g, a_fmt(A0, "Bild über 4 Cluster in 3 Stücken (FAT-Kette): gemerkt und gelesen (%s)", o));
          g = bild_gleich(k, "PPSA30028", 64, false, "/bild_PPSA30028.png", &o);   /* Codex-Prüfung 134: die Grenzen */
          pruefe(g, "Bild in 64 Stücken (die Grenze): gemerkt und gelesen");
          g = bild_gleich(k, "PPSA30080", 2, false, "/bild_PPSA30080.png", &o);
          pruefe(g, a_fmt(A0, "1024 Cluster zu 512 Byte in 2 Stücken (die Grenze): gemerkt und gelesen (%s)", o));
          pruefe(!mt_hat(&k->BILD_ORT, "PPSA30025") && !mt_hat(&k->BILD_ORT, "PPSA30026") && !mt_hat(&k->BILD_ORT, "PPSA30027")
                 && !mt_hat(&k->BILD_ORT, "PPSA30029") && !mt_hat(&k->BILD_ORT, "PPSA30081"),
                 "kein Bild bei Kreis in der FAT-Kette, zu kurzer Kette, ungültigem Folgecluster, 65 Stücken und 1025 Clustern (Codex-Prüfung 134)");
          eintrag *q2 = mit_pfad(k, "/mnt/usb0/PPSA30025.exfat");
          pruefe(q2 && strstr(q2->Text, "[Spiel: Bild Kreis (PPSA30025)]"), "Kreis in der Bildkette: der Name bleibt"); }
        size_t g = 0; const char *z = kern_abbild_titel_test(k, "/mnt/usb0/PPSA30010.exfat", &g);
        pruefe(!strcmp(z, "Spiel Abbild") && g > 0 && g <= 32768, a_fmt(A0, "exFAT-Abbild (knapp 300 KB): nur kleine Stücke gelesen (%zu Byte)", g));
        z = kern_abbild_titel_test(k, "/mnt/usb0/PPSA30015.exfat", &g);
        pruefe(g <= 300000, a_fmt(A0, "FAT-Kette im Kreis: Lesen begrenzt (%zu Byte)", g));
        SP_EN = true; k = neu(); SP_EN = false;
        eintrag *q = mit_pfad(k, "/mnt/usb0/PPSA30010.exfat");
        pruefe(q && strstr(q->Text, "[game: Image Game (PPSA30010)]") && strstr(q->Text, "[no tile]"), a_fmt(A0, "EN: englischer Name (%s)", q ? q->Text : "nicht angeboten"));
    }

    /* =============================================================== 16c. UFS2-Abbilder (.ffpkg, .ufs, .img): Name und Bild (NEU 1.0.9) */
    printf("== 16c. UFS2-Abbilder (.ffpkg, .ufs, .img): Spielname und Bild aus dem Abbild\n");
    {
        kern *k = neu();
        struct { const char *pfad, *soll, *was; } B[] = {
            { "/mnt/usb0/PPSA30030.ffpkg", "[Spiel: Ufs Spiel (PPSA30030)]", "UFS2 wie mkufs2.sh (Blöcke und Fragmente 64 KB)" },
            { "/mnt/usb0/PPSA30031.ufs", "[Spiel: Klein Spiel (PPSA30031)]", ".ufs mit 4-KB-Blöcken, 512-Byte-Fragmenten, sce_sys in Zylindergruppe 1" },
            { "/mnt/usb0/PPSA30032.ffpkg", "[Spiel: Ufs Unterordner (PPSA30032)]", "Spiel in einem einzigen Unterordner" },
            { "/mnt/usb0/CUSA30033.ffpkg", "[Spiel: PS4 Ufs (CUSA30033)]", "PS4-Spiel im UFS2-Abbild (param.sfo)" },
            { "/mnt/usb0/PPSA30034.ffpkg", "[Spiel: Ufs Verstreut (PPSA30034)]", "Bild über verstreute Blöcke: Name ja" },
            { "/mnt/usb0/PPSA30036.ffpkg", "[Spiel: Ufs Kaputt Verz (PPSA30036)]", "kaputter Verzeichniseintrag: Rest des Stücks übersprungen, sce_sys im nächsten Stück" },
            { "/mnt/usb0/PPSA30037.img", "[Spiel: Ufs Img (PPSA30037)]", ".img mit UFS2 (UFS2Tool)" },
            { "/mnt/usb0/PPSA30039.ffpkg", "[Spiel: Ufs Ohne Png (PPSA30039)]", "icon0.png ohne PNG-Kennung: Name ja" },
            { "/mnt/usb0/PPSA30035.ffpkg", NULL, "kaputter Superblock (Blockgröße 3000): nur der Dateiname" },
            { "/mnt/usb0/PPSA30038.ffpkg", NULL, "abgeschnittenes Abbild: nur der Dateiname" },
            { "/mnt/usb0/PPSA30041.ffpkg", NULL, "Inode-Nummer hinter der letzten Zylindergruppe: nur der Dateiname" } };
        for (size_t i = 0; i < sizeof B / sizeof *B; i++) {
            eintrag *q = mit_pfad(k, B[i].pfad);
            pruefe(q && q->Kat == 1 && strstr(q->Text, "[ohne Kachel]") && (B[i].soll ? strstr(q->Text, B[i].soll) != NULL : !strstr(q->Text, "[Spiel:")),
                   a_fmt(A0, "%s (%s)", B[i].was, q ? q->Text : "nicht angeboten"));
        }
        struct { const char *id, *soll, *was; } C[] = {
            { "PPSA30030", "/mnt/usb0/PPSA30030.ffpkg\n458752\n100000", "Bild in zwei Blöcken hintereinander (ab Fragment 7 x 64 KB)" },
            { "PPSA30031", "/mnt/usb0/PPSA30031.ufs\n102400\n80000", "Bild über 20 Blöcke mit Indirekt-Block (ab Fragment 200 x 512)" },
            { "PPSA30037", "/mnt/usb0/PPSA30037.img\n458752\n100000", ".img: Bild gemerkt" } };
        for (size_t i = 0; i < sizeof C / sizeof *C; i++) {
            const char *b = mt_hat(&k->BILD_ORT, C[i].id) ? mt_hole(&k->BILD_ORT, C[i].id) : "(keins)";
            pruefe(!strcmp(b, C[i].soll), a_fmt(A0, "%s (%s)", C[i].was, b));
        }
        pruefe(!mt_hat(&k->BILD_ORT, "PPSA30039") && !mt_hat(&k->BILD_ORT, "PPSA30035"), "kein Bild ohne PNG-Kennung und ohne lesbaren Namen");
        { const char *o = "";   /* NEU 1.0.9 (#149): Bilder in Stücken */
          bool g = bild_gleich(k, "PPSA30034", 2, false, "/bild_PPSA30034.png", &o);
          pruefe(g, a_fmt(A0, "verstreute Blöcke: in 2 Stücken gemerkt und gelesen (%s)", o));
          g = bild_gleich(k, "PPSA30042", 4, false, "/bild_PPSA30042.png", &o);
          pruefe(g, a_fmt(A0, "4 Stücke (eines über den Indirekt-Block, ein halber letzter Block): gemerkt und gelesen (%s)", o));
          kern_bild_ort b; const char *c = mt_hat(&k->BILD_ORT, "PPSA30031") ? mt_hole(&k->BILD_ORT, "PPSA30031") : "";
          unsigned char *d = kern_bild_ort_lesen(c, &b) ? kern_bild_lesen(VERB(A0, W, b.pfad), &b) : NULL;
          pruefe(d && b.teile_n == 0 && !memcmp(d, "\x89PNG", 4), "Bild am Stück: auch über kern_bild_lesen gelesen"); free(d);
          pruefe(!mt_hat(&k->BILD_ORT, "PPSA30043") && !mt_hat(&k->BILD_ORT, "PPSA30044") && !mt_hat(&k->BILD_ORT, "PPSA30045") && !mt_hat(&k->BILD_ORT, "PPSA30046"),
                 "kein Bild bei Loch, Block außerhalb, Block über den Rand und doppeltem Block (Codex-Prüfung 134)");
          eintrag *q3 = mit_pfad(k, "/mnt/usb0/PPSA30047.ufs");
          pruefe(!mt_hat(&k->BILD_ORT, "PPSA30047") && q3 && strstr(q3->Text, "[Spiel: Ufs Ueberlappt (PPSA30047)]"),
                 "Blöcke, die sich zur Hälfte überschneiden: kein Bild, der Name bleibt (Codex-Prüfung 135)"); }
        size_t g = 0; const char *z = kern_abbild_titel_test(k, "/mnt/usb0/PPSA30030.ffpkg", &g);
        pruefe(!strcmp(z, "Ufs Spiel") && g > 0 && g <= 16384, a_fmt(A0, "UFS2-Abbild (1 MB): nur kleine Stücke gelesen (%zu Byte)", g));
        z = kern_abbild_titel_test(k, "/mnt/usb0/PPSA30031.ufs", &g);
        pruefe(!strcmp(z, "Klein Spiel") && g > 0 && g <= 16384, a_fmt(A0, "kleine Blöcke mit Indirekt-Block: nur kleine Stücke gelesen (%zu Byte)", g));
        /* Codex-Prüfung 125 (P2): Gegenprobe mit einem fremden, echten UFS2-Abbild (mit FreeBSD newfs angelegt; aus log2timeline/dfvfs
         * test_data/ufs2.raw, Apache-2.0, ab Stelle 8192): 32-KB-Blöcke, 4-KB-Fragmente, 4 Zylindergruppen, .snap im Hauptverzeichnis,
         * a_directory in Gruppe 1. Inhalte wie in dfvfs (passwords.txt, test_data/another_file). */
        { const char *ref = "/mnt/usb0/ufs2_dfvfs.img"; size_t m = 0;
          const unsigned char *pw = kern_ufs_datei_test(k, ref, "passwords.txt", &m);
          pruefe(pw && m == 116 && !memcmp(pw, "place,user,password\nbank,joesmith,superrich\nalarm system,-,1234\ntreasure chest,-,1111\nuber secret laire,admin,admin\n", 116),
                 a_fmt(A0, "echtes UFS2 (newfs): passwords.txt im Hauptverzeichnis gelesen (%zu Byte)", m));
          const unsigned char *af = kern_ufs_datei_test(k, ref, "a_directory/another_file", &m);
          pruefe(af && m == 22 && !memcmp(af, "This is another file.\n", 22), a_fmt(A0, "echtes UFS2: Datei in einem Ordner der Zylindergruppe 1 gelesen (%zu Byte)", m));
          pruefe(!kern_ufs_datei_test(k, ref, "a_link", &m), "echtes UFS2: eine Verknüpfung wird nicht als Datei gelesen");
          pruefe(!kern_ufs_datei_test(k, ref, "a_directory", &m) && !kern_ufs_datei_test(k, ref, "fehlt.txt", &m), "echtes UFS2: Ordner und fehlende Datei: nichts gelesen");
          eintrag *q = mit_pfad(k, ref);
          pruefe(q && q->Kat == 1 && !strstr(q->Text, "[Spiel:"), a_fmt(A0, "echtes UFS2 ohne sce_sys: Spiel-Abbild ohne Namen (%s)", q ? q->Text : "nicht angeboten")); }
        SP_EN = true; k = neu(); SP_EN = false;
        eintrag *q = mit_pfad(k, "/mnt/usb0/PPSA30030.ffpkg");
        pruefe(q && strstr(q->Text, "[game: Ufs Game (PPSA30030)]"), a_fmt(A0, "EN: englischer Name (%s)", q ? q->Text : "nicht angeboten"));
    }

    /* =============================================================== 16d. PFS-Abbilder (.ffpfs, .ffpfsc): Name und Bild (NEU 1.0.9, #144) */
    printf("== 16d. PFS-Abbilder (.ffpfs, .ffpfsc): Spielname und Bild aus dem Abbild\n");
    {
        kern *k = neu();
        struct { const char *pfad, *soll, *was; } B[] = {
            { "/mnt/usb0/PPSA30050.ffpfs", "[Spiel: Pfs Spiel (PPSA30050)]", "PFS wie MkPFS (64-KB-Blöcke, sce_sys in uroot)" },
            { "/mnt/usb0/CUSA30051.ffpfs", "[Spiel: PS4 Pfs (CUSA30051)]", "PS4-Spiel im PFS-Abbild (param.sfo)" },
            { "/mnt/usb0/PPSA30052.ffpfs", "[Spiel: Pfs Ohne Png (PPSA30052)]", "icon0.png ohne PNG-Kennung: Name ja" },
            { "/mnt/usb0/PPSA30053.ffpfsc", "[Spiel: Pfsc Spiel (PPSA30053)]", ".ffpfsc: exFAT gepackt im PFS (zlib-Blöcke, Block 1 ungepackt, letzter Block halb)" },
            { "/mnt/usb0/PPSA30054.ffpfsc", "[Spiel: Pfs Roh (PPSA30054)]", "Abbild im Abbild ungepackt" },
            { "/mnt/usb0/PPSA30055.ffpfs", NULL, "verschlüsseltes PFS: nur der Dateiname" },
            { "/mnt/usb0/PPSA30056.ffpfs", NULL, "param.json nicht am Stück: nur der Dateiname" },
            { "/mnt/usb0/PPSA30057.ffpfs", NULL, "falsche Kennung: nur der Dateiname" },
            { "/mnt/usb0/PPSA30058.ffpfs", NULL, "abgeschnittenes Abbild: nur der Dateiname" },
            { "/mnt/usb0/PPSA30059.ffpfsc", "[Spiel: Pfs Lang (PPSA30059)]", "inneres Abbild mit langem Dateinamen (über 64 Zeichen): erkannt (Codex-Prüfung 130)" },
            { "/mnt/usb0/PPSA30060.ffpfsc", NULL, "zwei Dateien in uroot: kein Abbild im Abbild" },
            { "/mnt/usb0/PPSA30061.ffpfsc", NULL, "zwei Dateien, eine mit langem Namen: kein Abbild im Abbild (Codex-Prüfung 130)" },
            { "/mnt/usb0/PPSA30062.ffpfsc", NULL, "eine Datei und ein Eintrag fremder Art: kein Abbild im Abbild (Codex-Prüfung 130)" },
            { "/mnt/usb0/PPSA30063.ffpfs", NULL, "param.json ist eine Verknüpfung (Art 0xA000): nicht gelesen (Codex-Prüfung 130)" },
            { "/mnt/usb0/PPSA30064.ffpfs", NULL, "sce_sys mit Art 0xC000: kein Ordner, nicht gelesen (Codex-Prüfung 130)" },
            { "/mnt/usb0/PPSA30065.ffpfsc", NULL, "zwei Dateien, eine mit einer 0 im Namen: kein Abbild im Abbild (Codex-Prüfung 131)" },
            { "/mnt/usb0/PPSA30066.ffpfsc", NULL, "eine Datei, danach ein kaputter Eintrag: kein Abbild im Abbild (Codex-Prüfung 131)" },
            { "/mnt/usb0/PPSA30067.ffpfsc", NULL, "uroot endet mit einem angefangenen Eintrag: kein Abbild im Abbild (Codex-Prüfung 132)" },
            { "/mnt/usb0/PPSA30068.ffpfs", NULL, "sce_sys mit kaputtem Eintrag (davor param.sfo, dahinter param.json): kein Name aus einer kaputten Liste (Codex-Prüfung 132)" },
            { "/mnt/usb0/PPSA30069.ffpfs", NULL, "Über-Wurzel mit kaputtem Eintrag hinter uroot: nicht gelesen (Codex-Prüfung 132)" },
            { "/mnt/usb0/PPSA30072.ffpfs", "[Spiel: Pfs Minus (PPSA30072)]", "Indirekt-Felder alle -1 statt 0: gelesen (PS5-Test 09.11.2026, Spider-Man 2)" },
            { "/mnt/usb0/PPSA30073.ffpfs", NULL, "Indirekt-Felder gemischt (-1 und 0): nicht gelesen" } };
        for (size_t i = 0; i < sizeof B / sizeof *B; i++) {
            eintrag *q = mit_pfad(k, B[i].pfad);
            pruefe(q && q->Kat == 1 && strstr(q->Text, "[ohne Kachel]") && (B[i].soll ? strstr(q->Text, B[i].soll) != NULL : !strstr(q->Text, "[Spiel:")),
                   a_fmt(A0, "%s (%s)", B[i].was, q ? q->Text : "nicht angeboten"));
        }
        struct { const char *id, *soll, *was; } C[] = {
            { "PPSA30050", "/mnt/usb0/PPSA30050.ffpfs\n458752\n5000", "Bild am Stück (Block 7)" },
            { "PPSA30054", "/mnt/usb0/PPSA30054.ffpfsc\n397312\n50", "Bild im ungepackten inneren Abbild (Stelle der Datei + Stelle im Abbild)" } };
        for (size_t i = 0; i < sizeof C / sizeof *C; i++) {
            const char *b = mt_hat(&k->BILD_ORT, C[i].id) ? mt_hole(&k->BILD_ORT, C[i].id) : "(keins)";
            pruefe(!strcmp(b, C[i].soll), a_fmt(A0, "%s (%s)", C[i].was, b));
        }
        pruefe(!mt_hat(&k->BILD_ORT, "PPSA30052") && !mt_hat(&k->BILD_ORT, "PPSA30055") && !mt_hat(&k->BILD_ORT, "PPSA30056"), "kein Bild ohne PNG-Kennung und ohne lesbaren Namen");
        pruefe(mt_hat(&k->BILD_ORT, "PPSA30072") && !mt_hat(&k->BILD_ORT, "PPSA30073"), "Indirekt-Felder alle -1: Bild da, gemischt: keins (PS5-Test 09.11.2026)");
        /* gepackt: vierte Zeile "pfsc <Anfang> <Länge>"; die Oberfläche liest das Bild über kern_pfsc_datei_stueck */
        const char *b = mt_hat(&k->BILD_ORT, "PPSA30053") ? mt_hole(&k->BILD_ORT, "PPSA30053") : "";
        unsigned long long ab = 0, len = 0, cab = 0, cgr = 0; char pf[200] = "";
        bool form = sscanf(b, "%199[^\n]\n%llu\n%llu\npfsc %llu %llu", pf, &ab, &len, &cab, &cgr) == 5 && !strcmp(pf, "/mnt/usb0/PPSA30053.ffpfsc") && ab == 69632 && len == 5000 && cab == 327680;
        pruefe(form, a_fmt(A0, ".ffpfsc: Bild mit vierter Zeile pfsc (%s)", b));
        kern_bild_ort bo;
        pruefe(kern_bild_ort_lesen(b, &bo) && bo.gepackt && !strcmp(bo.pfad, "/mnt/usb0/PPSA30053.ffpfsc") && bo.ab == 69632 && bo.len == 5000 && bo.cab == 327680 && bo.cgr == cgr,
               "Oberfläche: BILD_ORT der .ffpfsc mit kern_bild_ort_lesen zerlegt (wie app.c spiel_bild)");
        { const char *o = "";   /* NEU 1.0.9 (#149): gepackt und in Stücken */
          bool g = bild_gleich(k, "PPSA30070", 2, true, "/bild_PPSA30070.png", &o);
          pruefe(g, a_fmt(A0, ".ffpfsc, Bild in 2 Stücken in gepackten Blöcken, eines über die Blockgrenze: gelesen (%s)", o));
          g = bild_gleich(k, "PPSA30071", 3, false, "/bild_PPSA30071.png", &o);
          pruefe(g, a_fmt(A0, "Abbild im Abbild ungepackt (Versatz), Bild in 3 Stücken: gelesen (%s)", o));
          size_t n53 = 0; unsigned char *b53 = kern_bild_lesen(VERB(A0, W, bo.pfad), &bo), *i53 = ganz_lesen(VERB(A0, HIER, "/innen_PPSA30053.exfat"), &n53);
          pruefe(b53 && i53 && n53 >= 69632 + 5000 && !memcmp(b53, i53 + 69632, 5000), "Oberfläche: Bild aus dem gepackten Abbild über kern_bild_lesen - Byte für Byte wie im inneren Abbild");
          free(b53); free(i53); }
        unsigned char *bild = form ? kern_pfsc_datei_stueck(VERB(A0, W, pf), cab, cgr, ab, (size_t)len) : NULL;
        unsigned char soll[5000]; bool gleich = false;
        FILE *fi = fopen(VERB(A0, HIER, "/innen_PPSA30053.exfat"), "rb");
        if (bild && fi && fseek(fi, 69632, SEEK_SET) == 0 && fread(soll, 1, sizeof soll, fi) == sizeof soll) gleich = !memcmp(bild, soll, sizeof soll) && !memcmp(bild, "\x89PNG", 4);
        if (fi) fclose(fi);
        free(bild);
        pruefe(gleich, "Oberfläche: Bild aus dem gepackten Abbild gelesen - Byte für Byte wie im inneren Abbild");
        unsigned char *x1 = form ? kern_pfsc_datei_stueck(VERB(A0, W, pf), cab + 8, cgr, ab, 8) : NULL, *x2 = form ? kern_pfsc_datei_stueck(VERB(A0, W, pf), cab, cgr, 294912 - 4, 8) : NULL;
        pruefe(form && !x1 && !x2, "Oberfläche: falscher Behälter oder hinter dem Ende - nichts gelesen");
        free(x1); free(x2);
        size_t g = 0; const char *z = kern_abbild_titel_test(k, "/mnt/usb0/PPSA30053.ffpfsc", &g);
        pruefe(!strcmp(z, "Pfsc Spiel") && g > 0 && g <= 300000, a_fmt(A0, ".ffpfsc: nur kleine Stücke gelesen (%zu Byte)", g));
        z = kern_abbild_titel_test(k, "/mnt/usb0/PPSA30050.ffpfs", &g);
        pruefe(!strcmp(z, "Pfs Spiel") && g > 0 && g <= 300000, a_fmt(A0, ".ffpfs: nur kleine Stücke gelesen (%zu Byte)", g));
        size_t m = 0; const unsigned char *pj = kern_pfs_datei_test(k, "/mnt/usb0/PPSA30050.ffpfs", "sce_sys/param.json", &m);
        pruefe(pj && m > 20 && strstr((const char *)pj, "\"Pfs Spiel\""), a_fmt(A0, "Datei im PFS über den Pfad gelesen (%zu Byte)", m));
        pruefe(!kern_pfs_datei_test(k, "/mnt/usb0/PPSA30050.ffpfs", "sce_sys", &m) && !kern_pfs_datei_test(k, "/mnt/usb0/PPSA30050.ffpfs", "fehlt.txt", &m), "Ordner und fehlende Datei: nichts gelesen");
        pruefe(!kern_pfs_datei_test(k, "/mnt/usb0/PPSA30063.ffpfs", "sce_sys/param.json", &m) && kern_pfs_datei_test(k, "/mnt/usb0/PPSA30063.ffpfs", "sce_sys/icon0.png", &m)
               && !kern_pfs_datei_test(k, "/mnt/usb0/PPSA30064.ffpfs", "sce_sys/icon0.png", &m) && !kern_pfs_datei_test(k, "/mnt/usb0/PPSA30069.ffpfs", "sce_sys/icon0.png", &m),
               "Pfad-Testhilfe: Verknüpfung, Ordner fremder Art und kaputte Über-Wurzel werden nicht gelesen, die Datei daneben schon (Codex-Prüfungen 131, 132)");
        SP_EN = true; k = neu(); SP_EN = false;
        eintrag *q = mit_pfad(k, "/mnt/usb0/PPSA30050.ffpfs");
        pruefe(q && strstr(q->Text, "[game: Pfs Game (PPSA30050)]"), a_fmt(A0, "EN: englischer Name (%s)", q ? q->Text : "nicht angeboten"));
    }

    /* =============================================================== 16e. Entpacken, PFSC-Behälter, BILD_ORT (NEU 1.0.9, Codex-Prüfung 130) */
    printf("== 16e. Entpacken (zlib) wie Pythons zlib, PFSC-Behälter, Bild-Ort zerlegen\n");
    {   /* Fälle aus entpacken_faelle.py (lauf_ps4.sh): Entscheidung und Inhalt von Pythons zlib */
        size_t fn = 0, i = 0; unsigned char *fa = ganz_lesen(VERB(A0, HIER, "/zlib_faelle.bin"), &fn);
        int gut[128] = {0}, schlecht[128] = {0}, gezeigt = 0; bool form = fa != NULL;
        while (form && i < fn) {
            uint32_t nl = 0, platz = 0, en = 0, rn = 0;
            if (!hole32(fa, fn, &i, &nl) || nl == 0 || nl > 100 || fn - i < (size_t)nl + 2) { form = false; break; }
            char name[128]; memcpy(name, fa + i, nl); name[nl] = 0; i += nl;
            char art = (char)fa[i++]; bool rahmen = fa[i++] != 0;
            if (!hole32(fa, fn, &i, &platz) || !hole32(fa, fn, &i, &en) || fn - i < en) { form = false; break; }
            const unsigned char *ein = fa + i; i += en;
            if (!hole32(fa, fn, &i, &rn) || fn - i < rn) { form = false; break; }
            const unsigned char *soll = fa + i; i += rn;
            unsigned char *aus = malloc(platz ? platz : 1);
            long r = aus ? kern_entpacken(ein, en, aus, platz, rahmen) : -2;
            bool ok = art == 'g' ? r == (long)rn && !memcmp(aus, soll, rn) : r == -1;
            unsigned char g = (unsigned char)name[0] & 127;
            if (ok) gut[g]++; else { schlecht[g]++; if (gezeigt++ < 12) printf("     anders als zlib: %s (%s, Ergebnis %ld, erwartet %ld)\n", name, art == 'g' ? "gültig" : "kaputt", r, art == 'g' ? (long)rn : -1L); }
            free(aus);
        }
        free(fa);
        pruefe(form && gut['z'] >= 200 && !schlecht['z'], a_fmt(A0, "Entpacken: von zlib gepackt (Stufen 0, 1, 6 und 9, alle Strategien, kleines Fenster, ohne Rahmen, mehrere Blöcke, genau passender Platz): %d wie zlib, %d anders", gut['z'], schlecht['z']));
        pruefe(form && gut['h'] >= 12 && !schlecht['h'], a_fmt(A0, "Entpacken: von Hand gebaut, gültig (Länge 258, Abstand 32768, alle Längen und Abstände, ein einziger und kein Abstandscode, nur Ende, viele Blöcke): %d wie zlib, %d anders", gut['h'], schlecht['h']));
        pruefe(form && gut['k'] >= 220 && !schlecht['k'], a_fmt(A0, "Entpacken: kaputt (Blockart 3, Tabellen über- und unterbelegt, Wiederholungen, zu weite Rückverweise, Codes 30/31/286/287, 287/288 Längen und 31/32 Abstände wie zlib, Kopf, Adler-32, abgeschnitten, zu wenig Platz): %d abgelehnt, %d nicht", gut['k'], schlecht['k']));
        pruefe(form && gut['m'] >= 4000 && !schlecht['m'], a_fmt(A0, "Entpacken: zufällig veränderte Ströme mit und ohne Rahmen: %d wie zlib entschieden (gültige gleich entpackt), %d anders", gut['m'], schlecht['m']));
        FILE *li = fopen(VERB(A0, HIER, "/pfsc/liste.txt"), "r");
        int ok_gut = 0, ka_gut = 0, falsch = 0; bool codex = false, grenzen = false, letzt_roh = false, letzt_zlib = false;
        char beh[64], inh[64], erw[16]; unsigned long long ab, n;
        while (li && fscanf(li, "%63s %63s %15s %llu %llu", beh, inh, erw, &ab, &n) == 5) {
            const char *bp = VERB(A0, HIER, "/pfsc/", beh); struct stat st;
            unsigned char *d = stat(bp, &st) == 0 ? kern_pfsc_datei_stueck(bp, 0, (unsigned long long)st.st_size, ab, (size_t)n) : NULL;
            bool ok;
            if (!strcmp(erw, "ok")) {
                size_t rn = 0; unsigned char *r = ganz_lesen(VERB(A0, HIER, "/pfsc/", inh), &rn);
                ok = d && r && ab + n <= rn && !memcmp(d, r + ab, (size_t)n); free(r);
                if (ok) ok_gut++;
                if (!strcmp(beh, "codex_ende.bin") && ab == 3 * 65536ULL) codex = ok;
                if (!strcmp(beh, "gepackt_ende.bin") && ab == 65536 - 100) grenzen = ok;
                if (!strcmp(beh, "roh_ende.bin") && ab == 3 * 65536ULL) letzt_roh = ok;
                if (!strcmp(beh, "gepackt_ende.bin") && ab == 3 * 65536ULL) letzt_zlib = ok;
            } else { ok = !d; if (ok) ka_gut++; }
            if (!ok) { falsch++; printf("     PFSC anders: %s ab %llu, %llu Byte (erwartet: %s)\n", beh, ab, n, erw); }
            free(d);
        }
        if (li) fclose(li);
        pruefe(codex, "PFSC: letzter Block ungepackt, dessen Bytes zufällig ein gültiger zlib-Strom sind - so gelesen, wie gespeichert (Codex-Prüfung 130)");
        pruefe(grenzen && letzt_roh && letzt_zlib, "PFSC: über Blockgrenzen (gepackt/ungepackt) und kurzer letzter Block ungepackt und gepackt - Byte für Byte wie der Inhalt");
        pruefe(li && ok_gut >= 35 && ka_gut >= 25 && !falsch, a_fmt(A0, "PFSC: %d Lesefälle richtig gelesen, %d kaputte Behälter, Tabellen und Blöcke abgelehnt, %d anders", ok_gut, ka_gut, falsch));
        /* BILD_ORT zerlegen (app.c spiel_bild) */
        struct { const char *o; bool ok, gepackt; const char *pfad; unsigned long long ab, len, cab, cgr; } O[] = {
            { "/mnt/usb0/a.exfat\n69632\n5000", true, false, "/mnt/usb0/a.exfat", 69632, 5000, 0, 0 },
            { "/a\n0\n0", true, false, "/a", 0, 0, 0, 0 },
            { "/mnt/usb0/a.ffpfsc\n69632\n5000\npfsc 327680 99999", true, true, "/mnt/usb0/a.ffpfsc", 69632, 5000, 327680, 99999 },
            { "/a\n1\n4194304\npfsc 3 4", true, true, "/a", 1, 4194304, 3, 4 },
            { "/a\n1\n8\npfsc 3 4x", false, false, NULL, 0, 0, 0, 0 }, { "/a\n1\n8\npfsc 3 4 ", false, false, NULL, 0, 0, 0, 0 },
            { "/a\n1\n8\npfsc 3 4\n", false, false, NULL, 0, 0, 0, 0 }, { "/a\n1\n8\npfsc 3", false, false, NULL, 0, 0, 0, 0 },
            { "/a\n1\n8\nPFSC 3 4", false, false, NULL, 0, 0, 0, 0 }, { "/a\n1\n8\npfsc  3 4", false, false, NULL, 0, 0, 0, 0 },
            { "/a\n1\n8\npfsc -3 4", false, false, NULL, 0, 0, 0, 0 }, { "/a\n1\n7\npfsc 3 4", false, false, NULL, 0, 0, 0, 0 },
            { "/a\n1\n4194305\npfsc 3 4", false, false, NULL, 0, 0, 0, 0 }, { "/a\n\n2", false, false, NULL, 0, 0, 0, 0 },
            { "/a\n 1\n2", false, false, NULL, 0, 0, 0, 0 }, { "/a\n1x\n2", false, false, NULL, 0, 0, 0, 0 }, { "/a\n+1\n2", false, false, NULL, 0, 0, 0, 0 },
            { "\n1\n2", false, false, NULL, 0, 0, 0, 0 }, { "/a\n1", false, false, NULL, 0, 0, 0, 0 }, { "/a\n1\n2\n", false, false, NULL, 0, 0, 0, 0 },
            { "/a\n1\n18446744073709551616", false, false, NULL, 0, 0, 0, 0 }, { "", false, false, NULL, 0, 0, 0, 0 },
            { "/a\n18446744073709551615\n0", true, false, "/a", 18446744073709551615ULL, 0, 0, 0 },
            { "/a\n18446744073709551610\n0", true, false, "/a", 18446744073709551610ULL, 0, 0, 0 } };
        /* NEU 1.0.9 (#149): Zeile "teile" */
        struct { const char *o; bool ok; int n; bool gepackt; } T2[] = {
            { "/a\n10\n30\nteile 2 10 20 100 10", true, 2, false }, { "/a\n10\n30\npfsc 5 6\nteile 3 10 10 50 10 90 10", true, 3, true },
            { "/a\n10\n30\nteile 1 10 30", false, 0, false }, { "/a\n10\n30\nteile 2 10 20", false, 0, false },
            { "/a\n10\n30\nteile 2 10 20 100 11", false, 0, false }, { "/a\n10\n30\nteile 2 11 20 100 10", false, 0, false },
            { "/a\n10\n30\nteile 2 10 30 100 0", false, 0, false }, { "/a\n10\n30\nteile 2 10 20 100 10 ", false, 0, false },
            { "/a\n10\n30\nteile 2 10 20  100 10", false, 0, false }, { "/a\n10\n30\nteile 2 10 20 100 10\n", false, 0, false },
            { "/a\n10\n30\nteile 2 10 20 100 10\npfsc 5 6", false, 0, false }, { "/a\n10\n7\nteile 2 10 3 100 4", false, 0, false },
            { "/a\n10\n30\nTEILE 2 10 20 100 10", false, 0, false }, { "/a\n10\n30\nteile 65 10 30", false, 0, false },
            { "/a\n10\n30\nteile 2 10 20 18446744073709551615 10", false, 0, false } };
        int t_falsch = 0;
        for (size_t j = 0; j < sizeof T2 / sizeof *T2; j++) {
            kern_bild_ort tb; bool r = kern_bild_ort_lesen(T2[j].o, &tb);
            if (r != T2[j].ok || (r && (tb.teile_n != T2[j].n || tb.gepackt != T2[j].gepackt))) { t_falsch++; printf("     teile anders: \"%s\"\n", T2[j].o); }
        }
        { kern_bild_ort tb; char s[4096] = "/a\n10\n640\nteile 64"; for (int j = 0; j < 64; j++) snprintf(s + strlen(s), sizeof s - strlen(s), " %d 10", 10 + 20 * j);
          if (!kern_bild_ort_lesen(s, &tb) || tb.teile_n != 64 || tb.teile[63][0] != 1270) { t_falsch++; printf("     teile anders: 64 Stücke\n"); } }
        pruefe(!t_falsch, a_fmt(A0, "Bild-Ort mit Stücken zerlegen: %zu Formen (2 bis 64 Stücke, mit pfsc, Summe, erstes Stück, leeres Stück, Reste, Reihenfolge) - %d anders",
                                sizeof T2 / sizeof *T2 + 1, t_falsch));
        { kern_bild_ort tb; bool f1 = kern_bild_ort_lesen("/mnt/usb0/PPSA30024.exfat\n81920\n16284\nteile 2 81920 8192 9999999 8092", &tb);
          unsigned char *d1 = f1 ? kern_bild_lesen(VERB(A0, W, tb.pfad), &tb) : NULL, *d2 = kern_bild_lesen(VERB(A0, W, "/mnt/usb0/fehlt.exfat"), &tb);
          pruefe(f1 && !d1 && !d2, "kern_bild_lesen: Stück hinter dem Dateiende oder Datei fehlt - nichts gelesen"); free(d1); free(d2); }
        int o_falsch = 0; kern_bild_ort b;
        for (size_t j = 0; j < sizeof O / sizeof *O; j++) {
            bool r = kern_bild_ort_lesen(O[j].o, &b);
            bool ok = r == O[j].ok && (!r || (b.gepackt == O[j].gepackt && !strcmp(b.pfad, O[j].pfad) && b.ab == O[j].ab && b.len == O[j].len && b.cab == O[j].cab && b.cgr == O[j].cgr));
            if (!ok) { o_falsch++; printf("     Bild-Ort anders: \"%s\"\n", O[j].o); }
        }
        char lang[1100]; memset(lang, 'p', sizeof lang); snprintf(lang + 999, sizeof lang - 999, "\n1\n8");
        bool l999 = kern_bild_ort_lesen(lang, &b) && strlen(b.pfad) == 999;
        memset(lang, 'p', sizeof lang); snprintf(lang + 1000, sizeof lang - 1000, "\n1\n8");
        bool l1000 = !kern_bild_ort_lesen(lang, &b);
        pruefe(!o_falsch && l999 && l1000 && !kern_bild_ort_lesen(NULL, &b), a_fmt(A0, "Bild-Ort zerlegen: %zu Formen (drei und vier Zeilen, Grenzen 8 Byte und 4 MB, Reste, Leerzeichen, Vorzeichen, leere Zahlen, Pfad bis 999 Zeichen, Zahlen bis zum Höchstwert) - %d anders", sizeof O / sizeof *O + 3, o_falsch));
    }

    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
