/* PS5 Cleanup 1.0.8 - Test "PKG- und ELF-Dateien bei ihrem Spiel bzw. Programm" (PS5-Test 06.11.2026, Befunde 2, 12 und 14;
 * da_Chris86 17:47: "Jede ELF-Datei ... das muss ja überall sein. Also, das soll schon sauber geordnet sein, unabhängig, ob es
 * installiert ist oder nicht. Aber vor allem, wenn installiert ist, dann soll die dazugehörige ELF auch mit aufgelistet werden.")
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_zuordnung <hier> 0   Fälle (Testbaum zuordnung_baum.py), danach ein Löschlauf auf dem Testbaum
 *   test_zuordnung <hier> 2   2000 PKG-Dateien: alle gefunden, zusammengefasst, Laufzeit
 *   test_zuordnung <hier> 4   NEU 1.0.9 (#145): ProsperoStore mit unlesbarem journal.json (Testbaum zuordnung_baum.py store_kaputt)
 *   test_zuordnung <hier> 5   NEU 1.0.9 (Codex-Prüfung 137): ProsperoStore ohne journal.json (store_ohne_journal)
 *   test_zuordnung <hier> 6   NEU 1.0.9 (Codex-Prüfung 138): ProsperoStore mit journal.json ohne gültiges JSON (store_json_kaputt)
 * Nur nachgebildete Dateisysteme - nie eine PS5. Jede Prüfung druckt OK/FAIL, am Ende FEHLERZAHL. */
#include "../../kern/ablauf.h"   /* kern.h und ablauf_block_schluessel */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void melde_log(void *c, const char *t) { (void)c; (void)t; }
static kern *K; static arena *A0; static const char *W;
static bool ja(void *c) { (void)c; return true; }
static bool nie(void *c) { (void)c; return false; }
static void nichts(void *c, eintrag *f) { (void)c; (void)f; }
static bool da(const char *v) { struct stat st; return stat(VERB(A0, W, v), &st) == 0; }
static eintrag *mit_pfad(const char *p)
{
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (!strcmp(f->PfadeBasis.e[j], p)) return f; }
    return NULL;
}
static eintrag *kachel(const char *id)
{
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->KachelId && !strcmp(f->KachelId, id)) return f; }
    return NULL;
}
static const char *z(const eintrag *f) { return f ? kern_zeile(A0, f) : "nicht gefunden"; }
static kblock *block_mit(int kat, const eintrag *f, int *n_teile)
{
    kblock *b; int nb = kern_bloecke(K, A0, kat, SORT_AZ, &b);
    for (int i = 0; i < nb; i++) for (int j = 0; j < b[i].teile.n; j++) if (b[i].teile.e[j] == f) { *n_teile = b[i].teile.n; return &b[i]; }
    *n_teile = 0; return NULL;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: test_zuordnung <hier> 0|2|3\n"); return 2; }
    A0 = arena_neu();
    int lauf = atoi(argv[2]);
    W = a_fmt(A0, "%s/%s", argv[1], lauf >= 2 ? "w2" : "w");
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    K = kern_neu(zugriff_datei(&o)); K->melde = melde_log;
    volatile int fe = 0;
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    /* wie in der Oberfläche: erst den Selbstschutz ermitteln (Codex-Prüfung 93 - eine Kopie von PS5 Cleanup liegt im Testbaum) */
    VERSUCHE { kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K); kern_suche_funde(K); }
    FANGE { printf("FAIL Suche abgebrochen: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if (fe) { printf("FEHLERZAHL 1\n"); return 1; }
    double sek = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;

    {   /* immer: kein Pfad steht in zwei Einträgen */
        mtab gesehen = mt_neu(false); const char *dp = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) {
            if (mt_hat(&gesehen, f->PfadeBasis.e[j])) dp = f->PfadeBasis.e[j]; else mt_setze(A0, &gesehen, f->PfadeBasis.e[j], (void *)1); } }
        pruefe(!dp, a_fmt(A0, "kein Pfad in zwei Einträgen (%s)", dp ? dp : "-"));
    }

    if (lauf == 5) {   /* Codex-Prüfung 137: ProsperoStore ohne journal.json - keine angefangene Arbeit, auch PPSA99713 wird herausgelöst */
        printf("== ProsperoStore ohne journal.json\n");
        eintrag *z1 = mit_pfad("/user/data/prosperostore/staging/PPSA99713.zip"), *z2 = mit_pfad("/user/data/prosperostore/staging/PPSA99713");
        eintrag *fa = mit_pfad("/user/data/prosperostore/previous/PPSA99708");
        pruefe(z1 && z1->StoreTeil == STORE_REST && !z1->ToolKachel && z2 && z2 != z1 && z2->StoreTeil == STORE_REST && fa && fa->StoreTeil == STORE_FASSUNG,
               a_fmt(A0, "ohne journal.json werden auch die Teile von PPSA99713 herausgelöst (%s | %s)", z(z1), z(z2)));
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    if (lauf == 4 || lauf == 6) {   /* NEU 1.0.9 (#145): journal.json von ProsperoStore nicht lesbar (4) oder kein JSON (6, Codex-Prüfung 138) - nichts herausgelöst, wie bis 1.0.8 */
        printf("%s\n", lauf == 4 ? "== ProsperoStore mit unlesbarem journal.json (gültiger Inhalt hinter einer Verknüpfung)" : "== ProsperoStore mit journal.json ohne gültiges JSON");
        if (lauf == 4) {   /* Codex-Prüfung 138: der Lesefehler tritt wirklich ein (auch unter root) - gelesen nennte der Inhalt eine gültige Kennung */
            const unsigned char *d = NULL; size_t n = 0;
            bool lesbar = s_kopf(K->S, "/user/data/prosperostore/journal.json", 64, &d, &n);
            bool gueltig = s_kopf(K->S, "/user/data/prosperostore/journal_daten.json", 4096, &d, &n) && d && strstr(a_kopie_n(A0, (const char *)d, n), "\"titleId\":\"PPSA99713\"") != NULL;
            pruefe(!lesbar && gueltig, a_fmt(A0, "Voraussetzung: journal.json lässt sich nicht lesen (%s), dahinter steht gültig PPSA99713 (%s)", lesbar ? "lesbar!" : "Lesefehler", gueltig ? "ja" : "nein"));
        }
        int teile = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->StoreTeil) teile++;
        eintrag *ps = kachel("PPSA99000"), *g = mit_pfad("/user/data/prosperostore"), *m = mit_pfad("/mnt/ext1/prosperostore/previous/PPSA99709");
        eintrag *ms = mit_pfad("/mnt/ext1/prosperostore/staging");
        pruefe(teile == 0 && g && g->ToolKachel == ps && g->PfadeBasis.n == 1 && m && ms && ms->PfadeBasis.n == 1,
               a_fmt(A0, "nichts herausgelöst: der Ordner ist EIN Eintrag bei ProsperoStore, auf der M.2 wie bisher aufgeklappt (%d Teile | %s | %s | %s)", teile, z(g), z(m), z(ms)));
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    if (lauf == 3) {   /* Codex-Prüfung 96: übersetzt mit -DPKG_DATEIEN_MAX=1000 - Ordner, deren PKG-Dateien nicht mehr hineinpassen, bleiben rot */
        printf("== Grenze der PKG-Dateien je Suche (Test: 1000) bei 2000 PKG-Dateien in 4 Ordnern\n");
        static const char *const ORD[4] = { "/mnt/usb0/PS4", "/mnt/usb0/DLC", "/mnt/ext1/PKG", "/user/data/pkg" };
        int geteilt = 0, offen = 0, falsch = 0;
        for (int o = 0; o < 4; o++) {
            size_t l = strlen(ORD[o]); int n_pkg = 0;
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (!f->PkgArt) continue;
                for (int j = 0; j < f->PfadeBasis.n; j++) if (!strncmp(f->PfadeBasis.e[j], ORD[o], l) && f->PfadeBasis.e[j][l] == '/') { n_pkg++; break; } }
            eintrag *g = mit_pfad(ORD[o]); bool off = g && g->PkgOffen && g->Rot;
            if (n_pkg > 0 && !g) geteilt++; else if (n_pkg == 0 && off) offen++; else falsch++;
            printf("   %s: %d PKG-Zeilen, %s\n", ORD[o], n_pkg, off ? "rot (PkgOffen)" : g ? "ein Eintrag, NICHT rot" : "aufgeteilt");
        }
        pruefe(K->PKG_GEFUNDEN > 0 && K->PKG_GEFUNDEN <= 1000, a_fmt(A0, "höchstens 1000 PKG-Dateien aus Ordnern angelegt (%d)", K->PKG_GEFUNDEN));
        pruefe(geteilt > 0 && offen > 0 && falsch == 0, a_fmt(A0, "jeder Ordner ist entweder aufgeteilt (%d) oder rot, nur einzeln (%d) - sonst nichts (%d)", geteilt, offen, falsch));
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    if (lauf == 2) {
        printf("== 2000 PKG-Dateien auf USB-Laufwerk, M.2 und intern\n");
        int summe = 0, zeilen = 0, ohne = 0, unter = 0, zusammen = 0; bool rot = true;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (!f->PkgArt) continue;
            zeilen++; summe += f->PkgAnzahl; if (f->PkgAnzahl > 1) zusammen++;
            if (f->ToolKachel) unter++; else if (f->PkgOhneSpiel || (f->Begleit && f->Begleit->PkgOhneSpiel)) ohne++;
            if (kern_ampel(f) != AMPEL_ROT) rot = false; }
        pruefe(summe == 2000, a_fmt(A0, "alle 2000 PKG-Dateien gefunden (%d, in %d Zeilen, davon %d zusammengefasst)", summe, zeilen, zusammen));
        pruefe(zeilen < 400 && zusammen > 0, a_fmt(A0, "ab 6 gleicher Art je Spiel und Speicher eine Zeile: %d Zeilen statt 2000", zeilen));
        pruefe(unter + ohne == zeilen, a_fmt(A0, "jede Zeile bei ihrem Spiel: %d unter installierten Spielen, %d in Blöcken \"nicht installiert\"", unter, ohne));
        pruefe(rot, "alle PKG-Zeilen rot (endgültig weg)");
        kblock *b; int nb = kern_bloecke(K, A0, 1, SORT_AZ, &b), nni = 0;
        for (int i = 0; i < nb; i++) if (strstr(b[i].name, " · nicht installiert")) nni++;
        pruefe(nni == 30, a_fmt(A0, "30 Blöcke \"nicht installiert\" (10 von 40 Spielen installiert): %d", nni));
        pruefe(sek < 20.0, a_fmt(A0, "Suche mit 2000 PKG-Dateien in %.1f s (Grenze 20 s am PC)", sek));
        pruefe(K->PKG_GELISTET > 0 && K->PKG_GELISTET <= 100000, a_fmt(A0, "Grenze der PKG-Suche je Suche eingehalten: %d gelistete Einträge (höchstens 100000)", K->PKG_GELISTET));
        /* aus Ordnern: USB PS4 600 + DLC 600 + intern 300 (die 500 auf der M.2 stehen direkt in einem aufgeklappten Ordner) */
        pruefe(K->PKG_GEFUNDEN == 1500, a_fmt(A0, "PKG-Dateien aus Ordnern unter der Grenze von 20000: %d (erwartet 1500)", K->PKG_GEFUNDEN));
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }

    printf("== Befund 12: PKG-Dateien beim installierten Spiel (USB-Laufwerk und intern, auch in Ordnern)\n");
    eintrag *sp = kachel("CUSA40001");
    eintrag *p1 = mit_pfad("/mnt/usb0/PS4/Wolkenreise - CUSA40001 - v1.00 - EU.pkg"), *p2 = mit_pfad("/mnt/usb0/PS4/Wolkenreise - CUSA40001 - v1.02 - EU - Update.pkg");
    eintrag *p3 = mit_pfad("/user/data/pkg/Wolkenreise - CUSA40001 - v1.03 - EU - Update.pkg");
    pruefe(sp && p1 && p1->ToolKachel == sp && p1->Kat == sp->Kat && p1->KatHer == 5 && p1->Art == ART_PAKET_DATEI && kern_ampel(p1) == AMPEL_ROT
           && !strcmp(kern_folge_kurz(p1), "endgültig weg") && strstr(z(p1), "USB-Laufwerk – Wolkenreise – Installationsdatei"),
           a_fmt(A0, "Installationsdatei im Ordner PS4: im Block des Spiels, rot (%s, Bereich %d)", z(p1), p1 ? p1->Kat : -1));
    pruefe(p2 && p2->ToolKachel == sp && strstr(z(p2), "Wolkenreise – Update-Datei 01.02"), a_fmt(A0, "Update-Datei beim Spiel: %s", z(p2)));
    pruefe(p3 && p3->ToolKachel == sp && strstr(z(p3), "Wolkenreise – Update-Datei 01.03") && p3->Ort == ORT_INTERN, a_fmt(A0, "Update-Datei intern (/data/pkg) beim Spiel: %s", z(p3)));
    pruefe(p1 && strstr(kern_art_info(A0, p1), "Was installiert ist, bleibt"), a_fmt(A0, "Kasten: %s", p1 ? kern_art_info(A0, p1) : "-"));
    {
        int nt = 0; kblock *b = block_mit(sp ? sp->Kat : 1, p1, &nt);
        pruefe(b && b->kopf == sp && nt >= 4, a_fmt(A0, "Block \"%s\": Spiel und seine PKG-Dateien (%d Zeilen)", b ? b->name : "-", nt));
    }
    eintrag *ap = kachel("CUSA40003"), *a1 = mit_pfad("/mnt/usb0/PS4/Notizbuch - CUSA40003 - v1.00.pkg");
    pruefe(ap && ap->Kat == 6 && a1 && a1->ToolKachel == ap && a1->Kat == 6, a_fmt(A0, "PKG einer installierten App unter Apps bei der App: %s (Bereich %d)", z(a1), a1 ? a1->Kat : -1));

    printf("== Befund 12: nicht installiert - eigener Block mit Installationsdatei, Update und Zusatzinhalten\n");
    eintrag *nu = mit_pfad("/mnt/usb0/PS4/Nebelpfad - CUSA40002 - v1.00 - EU.pkg"), *nm = mit_pfad("/mnt/ext1/PKG/Nebelpfad - CUSA40002 - v1.00 - EU.pkg");
    eintrag *nup = mit_pfad("/mnt/usb0/PS4/Nebelpfad - CUSA40002 - v1.05 - EU - Update.pkg"), *ndl = mit_pfad("/mnt/usb0/DLC/EP0001-CUSA40002_00-NEBELDLC00000000.pkg");
    eintrag *nm10 = mit_pfad("/mnt/ext1/PKG/EP0001-CUSA40002_00-NEBELDLC00000010.pkg");
    eintrag *kopf = nu && nu->PkgOhneSpiel ? nu : nm && nm->PkgOhneSpiel ? nm : NULL;
    pruefe(kopf && kopf->Kat == 1 && kopf->BlockName && !strcmp(kopf->BlockName, "Nebelpfad · nicht installiert") && !kopf->ToolKachel,
           a_fmt(A0, "Kopf: eine Installationsdatei, Block \"%s\" unter Spiele", kopf && kopf->BlockName ? kopf->BlockName : "-"));
    eintrag *andere = kopf == nu ? nm : nu;
    pruefe(andere && andere->Begleit == kopf && nup && nup->Begleit == kopf && ndl && ndl->Begleit == kopf && nm10 && nm10->Begleit == kopf,
           "Installationsdatei, Update und Zusatzinhalte im selben Block");
    pruefe(ndl && ndl->PkgAnzahl == 8 && ndl->Pfade.n == 8 && strstr(z(ndl), "Nebelpfad · nicht installiert – Zusatzinhalte (8 PKG-Dateien)"),
           a_fmt(A0, "8 Zusatzinhalte auf dem USB-Laufwerk: eine Zeile (%s, %d Pfade)", z(ndl), ndl ? ndl->Pfade.n : 0));
    pruefe(nm10 && nm10->PkgAnzahl == 1 && strstr(z(nm10), "Zusatzinhalt-Datei: Nebelpfad Paket 10"), a_fmt(A0, "2 Zusatzinhalte auf der M.2: einzeln (%s)", z(nm10)));
    pruefe(nup && strstr(z(nup), "Nebelpfad · nicht installiert – Update-Datei 01.05"), a_fmt(A0, "Update: %s", z(nup)));
    pruefe(kopf && strstr(kern_art_info(A0, kopf), "gerade nicht installiert") && kern_ampel(kopf) == AMPEL_ROT, a_fmt(A0, "Kasten: %s", kopf ? kern_art_info(A0, kopf) : "-"));
    {
        int nt = 0; kblock *b = block_mit(1, kopf, &nt);
        bool reihe = b != NULL;
        for (int i = 1; b && i < b->teile.n; i++) { const eintrag *x = b->teile.e[i - 1], *y = b->teile.e[i];
            int rx = x->PkgArt == PKG_UPDATE ? 1 : x->PkgArt == PKG_DLC ? 2 : 0, ry = y->PkgArt == PKG_UPDATE ? 1 : y->PkgArt == PKG_DLC ? 2 : 0;
            if (i > 1 && rx > ry) reihe = false; }
        pruefe(b && nt == 10 && b->kopf == kopf && reihe && !strcmp(b->name, "Nebelpfad · nicht installiert"),
               a_fmt(A0, "Block \"%s\": 10 Zeilen, oben die Installationsdateien, dann Update, dann Zusatzinhalte (%d)", b ? b->name : "-", nt));
        /* Codex-Prüfung 83 (P3): die Reihenfolge bleibt, auch wenn die Größen bekannt sind (kern_bloecke sortiert dann nach Größe) */
        kern_groessen_im_hintergrund(K, nie, NULL, nichts, NULL);   /* alles zählen - die Zeile mit 8 Zusatzinhalten ist dann die größte */
        b = block_mit(1, kopf, &nt); reihe = b != NULL;
        for (int i = 2; b && i < b->teile.n; i++) { const eintrag *x = b->teile.e[i - 1], *y = b->teile.e[i];
            int rx = x->PkgArt == PKG_UPDATE ? 1 : x->PkgArt == PKG_DLC ? 2 : 0, ry = y->PkgArt == PKG_UPDATE ? 1 : y->PkgArt == PKG_DLC ? 2 : 0;
            if (rx > ry) reihe = false; }
        pruefe(reihe && kern_eintrag_groesse(K, ndl, true), "Reihenfolge Installationsdatei, Update, Zusatzinhalte auch nach dem Zählen (Zusatzinhalte größer)");
    }
    printf("== Befund 12: doppelt, App nicht installiert, ohne Spielnummer, Rest des Ordners, Tiefe\n");
    pruefe(nu && nm && nu->Doppelt.n == 1 && nu->Doppelt.e[0] == nm && strstr(z(nu), "doppelt: auch auf der M.2-SSD") && strstr(z(nm), "doppelt: auch auf dem USB-Laufwerk"),
           a_fmt(A0, "gleiche Datei (Name und Größe) auf USB-Laufwerk und M.2: %s | %s", z(nu), z(nm)));
    eintrag *zb = mit_pfad("/mnt/usb0/PS4/Zeichenbrett - CUSA40004.pkg");
    pruefe(zb && zb->Kat == 6 && zb->PkgOhneSpiel && !strcmp(zb->BlockName, "Zeichenbrett · nicht installiert"), a_fmt(A0, "App nicht installiert: Block unter Apps (%s, Bereich %d)", z(zb), zb ? zb->Kat : -1));
    eintrag *un = mit_pfad("/mnt/usb0/PS4/unbekannt.pkg");
    pruefe(un && un->Kat == 5 && !un->PkgId && !un->Begleit && !un->ToolKachel && kern_ampel(un) == AMPEL_ROT && strstr(kern_art_info(A0, un), "ließ sich nicht lesen"),
           a_fmt(A0, "PKG ohne lesbare Spielnummer: unter Sonstiges, rot (%s)", z(un)));
    eintrag *rest = mit_pfad("/mnt/usb0/PS4/Cover");
    bool pkg_drin = false; for (int j = 0; rest && j < rest->PfadeBasis.n; j++) if (strstr(rest->PfadeBasis.e[j], ".pkg")) pkg_drin = true;
    pruefe(rest && rest == mit_pfad("/mnt/usb0/PS4/liesmich.txt") && rest->PfadeBasis.n == 2 && !pkg_drin && strstr(rest->Text, "PS4  (ohne PKG-Dateien)"),
           a_fmt(A0, "der Rest des Ordners PS4 ist EIN Eintrag ohne die PKG-Dateien (%s)", rest ? rest->Text : "-"));
    eintrag *ts = mit_pfad("/mnt/usb0/Sammlung/A/B/C/Tiefsee - CUSA40005 - v1.00.pkg"), *zt = mit_pfad("/mnt/usb0/Sammlung/A/B/C/D/Zu tief - CUSA40006.pkg");
    eintrag *sa = mit_pfad("/mnt/usb0/Sammlung/A/notiz.txt");
    pruefe(ts && ts->PkgOhneSpiel && zt && zt->PkgOhneSpiel && sa && sa->PfadeBasis.n == 1 && strstr(sa->Text, "Sammlung  (ohne PKG-Dateien)") && !mit_pfad("/mnt/usb0/Sammlung/A/B/C/D"),
           a_fmt(A0, "PKG vier und fünf Ordner tief gefunden, der Rest ohne PKG-Datei (%s)", sa ? sa->Text : "-"));

    printf("== Codex-Prüfung 83\n");
    {   /* P1: kein Rest-Eintrag "(ohne PKG-Dateien)" darf eine PKG-Datei enthalten */
        const char *pk = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (!strstr(f->TextBasis, "ohne PKG-Dateien")) continue;
            for (int j = 0; j < f->PfadeBasis.n; j++) if (strstr(f->PfadeBasis.e[j], ".pkg")) pk = f->PfadeBasis.e[j]; }
        pruefe(!pk, a_fmt(A0, "kein Eintrag \"(ohne PKG-Dateien)\" mit einer PKG-Datei (%s)", pk ? pk : "-"));
    }
    {   /* tiefer als 8 Ebenen: der Selbstschutz sperrt, was er nicht prüfen konnte (wie in der Oberfläche) - angeboten wird nur, was
         * geprüft ist, die flache PKG-Datei steht bei ihrem Spiel; nichts Ungeprüftes kommt als "(ohne PKG-Dateien)" mit */
        eintrag *fl = mit_pfad("/mnt/usb0/Tief/1/Flach - CUSA40008.pkg"); const char *tiefer = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i];
            for (int j = 0; j < f->PfadeBasis.n; j++) if (!strncmp(f->PfadeBasis.e[j], "/mnt/usb0/Tief/1/2", 18)) tiefer = f->PfadeBasis.e[j]; }
        pruefe(fl && fl->PkgArt && !tiefer, a_fmt(A0, "tiefer als 8 Ebenen: das Ungeprüfte bleibt gesperrt, die flache PKG-Datei steht bei ihrem Spiel (%s%s%s)",
                                                   z(fl), tiefer ? " | angeboten: " : "", tiefer ? tiefer : ""));
    }
    eintrag *tf = mit_pfad("/mnt/usb0/Viele");
    pruefe(tf && !tf->PkgArt && !mit_pfad("/mnt/usb0/Viele/Paket - CUSA40011.pkg") && tf->PkgOffen && tf->Rot && kern_ampel(tf) == AMPEL_ROT
           && strstr(kern_art_info(A0, tf), "nicht nach PKG-Dateien suchen") && strstr(kern_folge(tf), "kann PKG-Dateien enthalten"),
           a_fmt(A0, "Ordner mit über 10000 Dateien (PKG-Suche nicht möglich): EIN Eintrag, rot, nur einzeln, mit Grund (Codex-Prüfung 90) (%s)", z(tf)));
    {   /* Gegenprobe: ein geprüfter Ordner ohne PKG-Datei bleibt, wie er war */
        int offen = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->PkgOffen) offen++;
        pruefe(offen == 1, a_fmt(A0, "nur der ungeprüfte Ordner ist so markiert (%d)", offen));
    }
    eintrag *ur = mit_pfad("/user/data/ps5upload/backups"), *up = mit_pfad("/user/data/ps5upload/Wolkenreise - CUSA40001 - v1.04 - EU - Update.pkg");
    pruefe(ur && ur->ToolOrdner && !strcmp(ur->ToolOrdner, "/user/data/ps5upload") && ur->Art == ART_TOOL_DATEN_WICHTIG && kern_ampel(ur) == AMPEL_ROT && ur->Kat == 6
           && up && up->ToolKachel == sp, a_fmt(A0, "Rest eines Tool-Ordners mit Sicherung bleibt rot unter Apps (%s, Bereich %d), seine PKG-Datei beim Spiel (%s)", z(ur), ur ? ur->Kat : -1, z(up)));
    eintrag *u1 = mit_pfad("/mnt/usb1/DLC/EP0001-CUSA40002_00-NEBELDLC00000020.pkg");
    pruefe(ndl && ndl->PkgAnzahl == 8 && u1 && u1->PkgAnzahl == 1 && u1->Begleit == kopf,
           a_fmt(A0, "zweites USB-Laufwerk: seine 3 Zusatzinhalte bleiben einzeln, nicht in der Zeile des ersten (%s)", z(u1)));
    eintrag *w0 = p2, *w1u = mit_pfad("/mnt/usb1/PS4/Wolkenreise - CUSA40001 - v1.02 - EU - Update.pkg");
    pruefe(w0 && w1u && w0->Doppelt.n == 1 && w0->Doppelt.e[0] == w1u && strstr(z(w0), "doppelt: auch auf einem anderen USB-Laufwerk"),
           a_fmt(A0, "dieselbe Datei auf zwei USB-Laufwerken: doppelt (%s)", z(w0)));
    eintrag *ll = mit_pfad("/mnt/usb0/PS4/Leerlauf - CUSA40009 - v1.00.pkg"), *lk = kachel("CUSA40009");
    /* PS5-Test 07.11.2026 (da_Chris86 08:46 "Doppelteinträge"): bis dahin eigener Block "· nicht installiert" (Codex-Prüfung 83) - jetzt im
     * Block des Spiels, mit eigenem Text */
    pruefe(lk && ll && ll->ToolKachel == lk && ll->PkgSpielFehlt && !ll->PkgOhneSpiel && strstr(kern_art_info(A0, ll), "nur ein Rest") && kern_ampel(ll) == AMPEL_ROT,
           a_fmt(A0, "leerer Rest /user/app/CUSA40009: PKG im Block des Spiels, Text \"nur ein Rest\" (PS5-Test 07.11.2026) (%s | Kopf: %s)", z(ll), z(lk)));
    eintrag *m0 = mit_pfad("/mnt/ext1/PKG/EP0001-CUSA40002_00-NEBELDLC00000000.pkg");
    pruefe(m0 && m0->PkgAnzahl == 1 && m0->Doppelt.n == 1 && m0->Doppelt.e[0] == ndl && ndl && ndl->Doppelt.n == 0 && strstr(z(m0), "doppelt: auch auf dem USB-Laufwerk") && !strstr(z(ndl), "doppelt"),
           a_fmt(A0, "nur 1 von 8 Dateien auch auf der M.2: die einzelne Datei sagt doppelt, die Zeile mit 8 nicht (Codex-Prüfung 85) (%s | %s)", z(m0), z(ndl)));
    eintrag *te = kachel("TEMU71030"), *tr = mit_pfad("/user/data/TestEmulator/backups");
    pruefe(te && tr && tr->ToolKachel == te && tr->Art == ART_TOOL_DATEN_WICHTIG && kern_ampel(tr) == AMPEL_ROT,
           a_fmt(A0, "unbekanntes Tool mit Kachel: Rest mit Sicherungen bleibt rot bei seiner Kachel (Codex-Prüfung 85) (%s, Art %d)", z(tr), tr ? tr->Art : -1));
    eintrag *tk = kachel("PPSA40010"), *tp = mit_pfad("/mnt/usb0/PS5/Totenreich - PPSA40010 - v1.00.pkg");
    pruefe(tk && strstr(tk->TextBasis, " - ShadowMount: ") && tp && tp->ToolKachel == tk && tp->PkgSpielFehlt && !tp->PkgOhneSpiel && strstr(kern_art_info(A0, tp), "nur ein Rest"),
           a_fmt(A0, "tote ShadowMount-Kachel: PKG im Block der Kachel, Text \"nur ein Rest\" (Codex-Prüfung 85, PS5-Test 07.11.2026) (%s | Kachel: %s)", z(tp), z(tk)));
    eintrag *lw = kachel("LEER71033"), *lp = mit_pfad("/mnt/usb0/Leerwerkzeug-LEER71033.pkg"), *vw = kachel("VOLL71034"), *vp = mit_pfad("/mnt/usb0/Vollwerkzeug-VOLL71034.pkg");
    pruefe(lw && lp && lp->ToolKachel == lw && lp->PkgSpielFehlt && !lp->PkgOhneSpiel && strstr(kern_art_info(A0, lp), "nur ein Rest"),
           a_fmt(A0, "Tool-Kachel mit leerem App-Ordner: PKG bei ihr, Text \"nur ein Rest\" (Codex-Prüfung 90, PS5-Test 07.11.2026) (%s)", z(lp)));
    pruefe(vw && vp && vp->ToolKachel == vw && !vp->PkgOhneSpiel && !vp->PkgSpielFehlt && !strstr(kern_art_info(A0, vp), "nur ein Rest"),
           a_fmt(A0, "Gegenprobe: Tool-Kachel mit Inhalt - PKG steht bei ihr (%s)", z(vp)));
    {   /* Codex-Prüfung 93: PKG-Datei neben einer Kopie von PS5 Cleanup steht bei ihrem Spiel, nicht in "alles außer PS5 Cleanup" */
        eintrag *pp = mit_pfad("/user/data/pldmgr/Pakete/Wolkenreise - CUSA40001 - v1.06 - EU - Update.pkg"), *pr = mit_pfad("/user/data/pldmgr/local/notiz.txt");
        const char *drin = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f == pp) continue;
            for (int j = 0; j < f->PfadeBasis.n; j++) if (!strncmp(f->PfadeBasis.e[j], "/user/data/pldmgr/Pakete", 24)) drin = f->PfadeBasis.e[j]; }
        pruefe(pp && pp->PkgArt == PKG_UPDATE && pp->ToolKachel == sp && pr && !pr->PkgOffen && !drin,
               a_fmt(A0, "PKG-Datei neben PS5 Cleanup steht bei ihrem Spiel (Codex-Prüfung 93) (%s | Rest: %s%s%s)", z(pp), z(pr), drin ? " | noch drin: " : "", drin ? drin : ""));
    }
    {   /* Codex-Prüfung 98 (P1): die Sammelzeile "alles außer PS5 Cleanup" eines Tool-Ordners mit Spielstand-Sicherung bleibt Daten des
         * Tool-Ordners (rot, nicht "neu herunterladen"), ebenso der Rest eines dort aufgeteilten Unterordners; die PKG-Dateien stehen einzeln */
        eintrag *sr = mit_pfad("/user/data/ps5save/backups"), *sa = mit_pfad("/user/data/ps5save/ablage/notiz.txt"), *su = mit_pfad("/user/data/sicherer/backups");
        eintrag *s1 = mit_pfad("/user/data/ps5save/Sternfall - CUSA40041 - v1.00.pkg"), *s2 = mit_pfad("/user/data/ps5save/ablage/Sternfall - CUSA40041 - v1.01 - Update.pkg");
        pruefe(sr && sr->ToolOrdner && !strcmp(sr->ToolOrdner, "/user/data/ps5save") && sr->Art == ART_TOOL_DATEN_WICHTIG && kern_ampel(sr) == AMPEL_ROT && sr->Rot,
               a_fmt(A0, "Sammelzeile neben PS5 Cleanup mit Spielstand-Sicherung bleibt rot (Codex-Prüfung 98) (%s, Art %d)", z(sr), sr ? sr->Art : -1));
        pruefe(sa && sa != sr && sa->ToolOrdner && !strcmp(sa->ToolOrdner, "/user/data/ps5save") && kern_ampel(sa) == AMPEL_ROT && s1 && s1->PkgArt && s2 && s2->PkgArt,
               a_fmt(A0, "Rest eines dort aufgeteilten Unterordners gehört zum Tool-Ordner, PKG-Dateien einzeln (Codex-Prüfung 98) (%s | %s | %s)", z(sa), z(s1), z(s2)));
        pruefe(su && su->ToolOrdner && !strcmp(su->ToolOrdner, "/user/data/sicherer") && su->Art == ART_UNBEKANNT,
               a_fmt(A0, "Gegenprobe unbekannter Ordner: Sammelzeile wie der ganze Ordner \"Inhalt unbekannt\", nicht \"neu herunterladen\" (Codex-Prüfung 98) (%s, Art %d)", z(su), su ? su->Art : -1));
    }
    {   /* PS5-Test 07.11.2026 (da_Chris86 08:46): der Entpack-Ordner von Orbit Store gehört zu dem Spiel darin (Gegenprobe: zwei verschiedene
         * Spielnummern - bleibt für sich); eine PKG-Datei im PS5-Format (Nummer nicht lesbar) im Daten-Ordner eines Tools steht beim Tool */
        eintrag *sw = kachel("PPSA40042"), *ox = mit_pfad("/mnt/ext1/homebrew/.orbit-extract-a1b2c3d4"), *oy = mit_pfad("/mnt/ext1/homebrew/.orbit-extract-e5f6a7b8");
        pruefe(sw && ox && ox->ToolKachel == sw && ox->Kat == sw->Kat && oy && !oy->ToolKachel,
               a_fmt(A0, "Entpack-Ordner von Orbit steht beim Spiel darin, ein gemischter bleibt für sich (PS5-Test 07.11.2026) (%s | %s)", z(ox), z(oy)));
        eintrag *fp = mit_pfad("/user/data/TestEmulator/TestEmulator_2.0.pkg");
        pruefe(fp && te && fp->ToolKachel == te && !fp->PkgId && fp->Kat == te->Kat && kern_ampel(fp) == AMPEL_ROT && strstr(kern_art_info(A0, fp), "im Ordner von TestEmulator")
               && strstr(z(fp), "TestEmulator_2.0.pkg"),
               a_fmt(A0, "PKG ohne lesbare Nummer im Daten-Ordner eines Tools steht beim Tool, mit Dateinamen (PS5-Test 07.11.2026) (%s, Bereich %d)", z(fp), fp ? fp->Kat : -1));
        eintrag *nk = kachel("NURP71035"), *np = mit_pfad("/user/data/Nurpaket/Nurpaket_1.0.pkg"), *fr = mit_pfad("/user/data/Fremdablage/fremd.pkg");
        pruefe(nk && np && np->ToolKachel == nk && np->Kat == nk->Kat && strstr(z(np), "Nurpaket_1.0.pkg") && fr && !fr->ToolKachel && fr->Kat == 5,
               a_fmt(A0, "nur eine PKG-Datei im Ordner (kein Rest): steht trotzdem beim Tool; Ordner ohne Kachel bleibt unter Sonstiges (Codex-Prüfung 102) (%s | %s)", z(np), z(fr)));
        eintrag *wk = kachel("WEBD71036"), *wp = mit_pfad("/user/data/ps5-webdateien/setup.pkg");
        pruefe(wk && wp && wp->ToolKachel == wk, a_fmt(A0, "dieselbe Namensregel wie für den Ordner, auch \"ps5-\" vorne (Codex-Prüfung 103) (%s)", z(wp)));
        eintrag *oz = mit_pfad("/mnt/ext1/homebrew/.orbit-extract-c9c9c9c9");
        pruefe(oz && !oz->ToolKachel, a_fmt(A0, "Verknüpfung .orbit-extract-... gehört zu keinem Spiel (Codex-Prüfung 102) (%s)", z(oz)));
    }
    {   /* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86: Orbit Store "in einem Block unter Apps"): Tool-Kachel und App über ShadowMount mit
         * GENAU demselben Namen sind ein Block unter Apps - mit der Spieldatei-Zeile; Gegenprobe: zwei gleichnamige Kacheln unter Apps */
        eintrag *tk = kachel("ORBT71040"), *sm = kachel("PPSA71041"), *sd = mit_pfad("/mnt/ext1/homebrew/PPSA71041.ffpkg");
        int nt = 0; kblock *b = tk ? block_mit(6, tk, &nt) : NULL;
        bool alle = b && b->kopf == tk;
        bool sm_drin = false, sd_drin = false;
        for (int j = 0; b && j < b->teile.n; j++) { if (b->teile.e[j] == sm) sm_drin = true; if (b->teile.e[j] == sd) sd_drin = true; }
        pruefe(tk && sm && sd && sm->Begleit == tk && !sm->ToolKachel && sm->Kat == 6 && sm->KatHer == 1 && sd->SmDatei && sd->Kat == 6 && alle && sm_drin && sd_drin,
               a_fmt(A0, "Sternhafen: Tool-Kachel, ShadowMount-Kachel und Spieldatei in EINEM Block unter Apps, die ShadowMount-Kachel nur zur Anzeige (Begleit, Codex-Prüfung 108) (%d Teile: %s | %s | %s)", nt, z(tk), z(sm), z(sd)));
        eintrag *zw = kachel("PPSA71044");
        pruefe(zw && zw->Kat == 1 && !zw->ToolKachel, a_fmt(A0, "Gegenprobe: zwei gleichnamige Kacheln unter Apps - ShadowMount-Kachel bleibt unter Spiele (%s, Bereich %d)", z(zw), zw ? zw->Kat : -1));
        eintrag *lf = kachel("PPSA71045"), *lfd = mit_pfad("/mnt/ext1/homebrew/PPSA71045.ffpkg");
        pruefe(lf && lf->Kat == 6 && lf->KatHer == 1 && lfd && lfd->Kat == 6 && lfd->KatHer == 1, a_fmt(A0, "App über ShadowMount (laut Angaben eine App) steht unter Apps, samt Spieldatei, und wird gelöscht wie unter Spiele (Codex-Prüfung 108) (%s, Bereich %d/%d | %s, Bereich %d/%d)",
               z(lf), lf ? lf->Kat : -1, lf ? lf->KatHer : -1, z(lfd), lfd ? lfd->Kat : -1, lfd ? lfd->KatHer : -1));
        /* Codex-Prüfung 109 (P3): Reihenfolge im Block und Zählen als EIN Block; tote ShadowMount-App */
        int ism = -1, isd = -1;
        for (int j = 0; b && j < b->teile.n; j++) { if (b->teile.e[j] == sm) ism = j; if (b->teile.e[j] == sd) isd = j; }
        groesse *gsm = sm ? kern_eintrag_groesse_in(K, A0, sm, true) : NULL, *gsd = sd ? kern_eintrag_groesse_in(K, A0, sd, true) : NULL;
        pruefe(gsm && gsd && gsd->Bytes > gsm->Bytes && ism > 0 && isd == ism + 1,
               a_fmt(A0, "Sternhafen: Spieldatei (%lld Byte) steht direkt hinter ihrer Kachel (%lld Byte), obwohl sie größer ist (Codex-Prüfung 109) (Platz %d und %d)",
                     gsd ? gsd->Bytes : -1LL, gsm ? gsm->Bytes : -1LL, ism, isd));
        const char *btk = tk ? ablauf_block_schluessel(A0, tk) : "-";
        pruefe(tk && sm && sd && !strcmp(ablauf_block_schluessel(A0, sm), btk) && !strcmp(ablauf_block_schluessel(A0, sd), btk),
               "Sternhafen zählt beim Bestätigen als EIN Block, wie in der Auswahl (Codex-Prüfung 109)");
        eintrag *dapp = kachel("APPN71047"), *dsm = kachel("PPSA71048");
        int ndt = 0; kblock *db = dapp ? block_mit(6, dapp, &ndt) : NULL; bool dsm_drin = false;
        for (int j = 0; db && j < db->teile.n; j++) if (db->teile.e[j] == dsm) dsm_drin = true;
        pruefe(dapp && dsm && dsm->Begleit == dapp && db && db->kopf == dapp && dsm_drin && !strcmp(ablauf_block_schluessel(A0, dsm), ablauf_block_schluessel(A0, dapp)),
               a_fmt(A0, "Doppelapp: installierte App und dieselbe App über ShadowMount - ein Block, auch beim Zählen (Prüfer 110) (%s | %s)", z(dapp), z(dsm)));
        eintrag *ll = kachel("PPSA71046");
        pruefe(ll && ll->Kat == 6 && ll->KatHer == 1 && ll->Art == ART_SM_TOT && kern_ampel(ll) != AMPEL_GELB,
               a_fmt(A0, "App über ShadowMount ohne Spieldatei (tote Kachel): unter Apps, Art wie unter Spiele (Codex-Prüfung 109) (%s, Bereich %d/%d, Art %d)",
                     z(ll), ll ? ll->Kat : -1, ll ? ll->KatHer : -1, ll ? ll->Art : -1));
    }
    {   /* Codex-Prüfung 94: aufgeteilte Ordner stehen in AUFGEKLAPPT - leer nach dem Löschen werden sie mit entfernt (äußere vor inneren) */
        int i1 = -1, i2 = -1, i3 = -1, i4 = -1;
        for (int i = 0; i < K->AUFGEKLAPPT.n; i++) { const char *o = K->AUFGEKLAPPT.e[i];
            if (!strcmp(o, "/user/data/pldmgr/Pakete")) i1 = i; if (!strcmp(o, "/mnt/usb0/PS4")) i2 = i;
            if (!strcmp(o, "/mnt/usb0/Sammlung")) i3 = i; if (!strcmp(o, "/mnt/usb0/Sammlung/A/B/C/D")) i4 = i; }
        pruefe(i1 >= 0 && i2 >= 0 && i3 >= 0 && i4 > i3, a_fmt(A0, "aufgeteilte Ordner werden nach dem Löschen mit entfernt, wenn sie leer sind (Codex-Prüfung 94) (%d %d %d %d)", i1, i2, i3, i4));
    }
    eintrag *bk = kachel("BACK71031");
    pruefe(bk && tr && ur && tr->ToolKachel == te && ur->ToolKachel != bk && kern_ampel(ur) == AMPEL_ROT,
           a_fmt(A0, "Kachel \"Backups\" bekommt keinen Rest eines Tool-Ordners, der Rest bleibt bei seinem Besitzer (Codex-Prüfung 86) (%s | %s)", z(tr), z(ur)));
    {
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        if (ndl) ndl->Wahl = true;
        const char *s = kern_endgueltig_satz(K, A0);
        pruefe(strstr(s, "8 Installationsdateien (PKG)") != NULL, a_fmt(A0, "Satz vor dem Löschen: %s", s));
        if (ndl) ndl->Wahl = false;
    }

    printf("== Befund 14 und 17:47: ELF-Dateien bei ihrem Programm\n");
    eintrag *wk = kachel("WKAL00001"), *w1 = mit_pfad("/mnt/usb0/webkit-autoloader-installer_v0.6.2.elf"), *w2 = mit_pfad("/mnt/usb0/webkit-autoloader-installer_v0.7.0-dev-3a34c3b.elf");
    eintrag *wa = mit_pfad("/user/data/ps5_autoloader");
    pruefe(wk && wk->Jailbreak && wk->Kat == 7, a_fmt(A0, "WebKit Autoloader ist ein Jailbreak-Einstieg (Bereich %d)", wk ? wk->Kat : -1));
    pruefe(w1 && w2 && w1->ToolKachel == wk && w2->ToolKachel == wk && w1->Kat == 7 && w1->KatHer == 6 && !w1->Jailbreak && w1->Art != ART_JAILBREAK,
           a_fmt(A0, "Installer-ELFs (zwei Versionen) unter der Kachel im Bereich Jailbreak, selbst kein Einstieg (%s, Art %d)", z(w1), w1 ? w1->Art : -1));
    pruefe(wa && wa->ToolKachel == wk && wa->Kat == 7, a_fmt(A0, "Ordner ps5_autoloader (autoload.txt) unter der Kachel (%s, Bereich %d)", z(wa), wa ? wa->Kat : -1));
    {
        int nt = 0; kblock *b = block_mit(7, wk, &nt);
        pruefe(b && b->kopf == wk && nt == 4 && !strcmp(b->name, "WebKit Autoloader v0.4.0") && strstr(z(wk), "WebKit Autoloader v0.4.0 – Kachel – Jailbreak-Start: WebKit-Autoloader"),
               a_fmt(A0, "Block \"%s\" im Bereich Jailbreak: Kachel, 2 ELF-Dateien, ps5_autoloader (%d Zeilen; Kachel: %s)", b ? b->name : "-", nt, z(wk)));
    }
    eintrag *wz = kachel("WERK71020"), *e1 = mit_pfad("/mnt/usb0/werkzeugkiste_v2.0.elf"), *e2 = mit_pfad("/mnt/usb0/Werkzeugkiste-2.1.elf");
    pruefe(wz && e1 && e2 && e1->ToolKachel == wz && e2->ToolKachel == wz, a_fmt(A0, "Version im Namen der Kachel (\"Werkzeugkiste v2.1\") stört nicht: %s | %s", z(e1), z(e2)));
    eintrag *sz = kachel("SPUR71021"), *s1 = mit_pfad("/mnt/usb0/spurensuche_v1.elf");
    pruefe(sz && s1 && s1->ToolKachel == sz, a_fmt(A0, "Kachel \"Spurensuche 3\" (Zahl ohne Version): ELF darunter (%s)", z(s1)));
    eintrag *g1 = mit_pfad("/mnt/usb0/Zangenwerk_v0.4.4.elf"), *g2 = mit_pfad("/mnt/usb0/Zangenwerk_v0.4.9-pre.elf");
    eintrag *gk = g1 && g1->OhneKachel ? g1 : g2 && g2->OhneKachel ? g2 : NULL, *go = gk == g1 ? g2 : g1;
    {
        int nt = 0; kblock *b = block_mit(6, gk, &nt);
        pruefe(gk && go && go->Begleit == gk && gk->BlockName && !strcmp(gk->BlockName, "Zangenwerk") && b && b->ohne_kachel && nt == 2,
               a_fmt(A0, "zwei Versionen ohne Kachel: ein Block \"%s\" (%d Zeilen, ohne Kachel)", b ? b->name : "-", nt));
    }
    eintrag *h1 = mit_pfad("/mnt/usb0/ps5-hammer_v1.elf"), *h2 = mit_pfad("/mnt/usb0/hammer-ps5-v2.elf");
    eintrag *hk = h1 && h1->OhneKachel ? h1 : h2 && h2->OhneKachel ? h2 : NULL;
    pruefe(hk && (hk == h1 ? h2 : h1)->Begleit == hk && !strcmp(hk->BlockName, "hammer"), a_fmt(A0, "\"ps5-\" vorne und \"-ps5\" hinten zählen nicht: Block \"%s\"", hk ? hk->BlockName : "-"));
    eintrag *ez = mit_pfad("/mnt/usb0/einzeln_v1.elf");
    pruefe(ez && !ez->Begleit && !ez->OhneKachel && !ez->ToolKachel && ez->Kat == 6, "eine einzelne ELF ohne Programm bleibt für sich unter Apps");

    printf("== Befund 2: bekannter wichtiger Ordner ohne Inhalt\n");
    eintrag *et = mit_pfad("/user/data/etaHEN"), *ga = mit_pfad("/user/data/garlic");
    pruefe(et && et->Art == ART_TOOL_DATEN && et->WichtigLeer && kern_ampel(et) == AMPEL_GELB && !strstr(z(et), "Cheats") && strstr(kern_art_info(A0, et), "keine Datei mit Inhalt"),
           a_fmt(A0, "etaHEN ohne Datei mit Inhalt: gelb, nur Einstellungen (%s)", z(et)));
    pruefe(ga && ga->Art == ART_TOOL_DATEN_WICHTIG && kern_ampel(ga) == AMPEL_ROT, a_fmt(A0, "garlic mit Spielstand-Sicherung bleibt rot (%s)", z(ga)));

    printf("== NEU 1.0.9 (#145): ProsperoStore - Beleg, aufgehobene Fassung und Reste bei ihrer App, der Rest bei ProsperoStore\n");
    eintrag *st_mo = NULL;
    {
        const char *S = "/user/data/prosperostore", *M = "/mnt/ext1/prosperostore";
        eintrag *ps = kachel("PPSA99000"), *eh = kachel("PPSA99708"), *ki = kachel("PPSA99716");
        eintrag *bl = mit_pfad(a_fmt(A0, "%s/receipts/PPSA99708.json", S)), *fa = mit_pfad(a_fmt(A0, "%s/previous/PPSA99708", S)), *tr = mit_pfad(a_fmt(A0, "%s/trash/PPSA99708", S));
        pruefe(eh && bl && bl->StoreTeil == STORE_BELEG && bl->ToolKachel == eh && bl->Art == ART_REST && kern_ampel(bl) == AMPEL_GRUEN && strstr(z(bl), "Beleg von ProsperoStore")
               && strstr(kern_art_info(A0, bl), "Beleg"), a_fmt(A0, "Beleg steht bei seiner App, grün (%s)", z(bl)));
        pruefe(fa && fa->StoreTeil == STORE_FASSUNG && fa->ToolKachel == eh && fa->Art == ART_STORE_FASSUNG && kern_ampel(fa) == AMPEL_ROT && fa->Rot
               && strstr(z(fa), "ältere Fassung, von ProsperoStore aufgehoben") && !strstr(z(fa), "Eichenhain") && strstr(kern_art_info(A0, fa), "vorige Fassung")
               && !strcmp(kern_folge_kurz(fa), "endgültig weg"), a_fmt(A0, "aufgehobene Fassung samt selbst hineingelegter Dateien steht bei ihrer App, rot (%s)", z(fa)));
        pruefe(tr && tr->StoreTeil == STORE_REST && tr->ToolKachel == eh && tr->Art == ART_REST && strstr(z(tr), "Rest einer Deinstallation durch ProsperoStore"),
               a_fmt(A0, "nicht fertig gelöschte App (trash) steht bei ihrer App, grün (%s)", z(tr)));
        eintrag *kp = mit_pfad(a_fmt(A0, "%s/previous/PPSA99716/pakete/Wolkenreise - CUSA40001 - v1.07 - EU - Update.pkg", S)), *kn = mit_pfad(a_fmt(A0, "%s/previous/PPSA99716/notiz.txt", S));
        eintrag *kz = mit_pfad(a_fmt(A0, "%s/staging/PPSA99716.zip", S));
        pruefe(ki && kp && kp->PkgArt == PKG_UPDATE && kp->ToolKachel == sp && !kp->StoreTeil && kn && kn->StoreTeil == STORE_FASSUNG && kn->ToolKachel == ki
               && kz && kz->StoreTeil == STORE_REST && kz->ToolKachel == ki && strstr(z(kz), "angefangener Download von ProsperoStore"),
               a_fmt(A0, "PKG-Datei in einer aufgehobenen Fassung steht bei ihrem Spiel, der Rest der Fassung und der Download bei der App (%s | %s | %s)", z(kp), z(kn), z(kz)));
        eintrag *mo = mit_pfad(a_fmt(A0, "%s/previous/PPSA99709", M)), *mz = mit_pfad(a_fmt(A0, "%s/staging/PPSA99709.zip", M));
        pruefe(mo && mo->StoreTeil == STORE_FASSUNG && !mo->ToolKachel && mo->Art == ART_STORE_FASSUNG && kern_ampel(mo) == AMPEL_ROT && strstr(z(mo), "M.2") && strstr(z(mo), "Moosgarten"),
               a_fmt(A0, "App gelöscht: ihre aufgehobene Fassung steht für sich, rot, mit dem Namen der App (%s)", z(mo)));
        pruefe(mz && mz->StoreTeil == STORE_REST && !mz->ToolKachel && mz->Art == ART_REST && strstr(z(mz), "angefangener Download"),
               a_fmt(A0, "angefangener Download einer gelöschten App steht für sich (%s)", z(mz)));
        st_mo = mo;
        eintrag *ko = mit_pfad(a_fmt(A0, "%s/settings.txt", S));
        static const char *const IM_REST[] = { "journal.json", "logs", "staging/PPSA99713.zip", "staging/PPSA99713", "receipts/PPSA99710.json",
                                               "receipts/PPSA99712.json", "previous/notiz.txt", "previous/ppsa99715", "previous/PPSA99714", NULL };
        const char *fehlt = NULL;
        for (int i = 0; IM_REST[i]; i++) if (!ko || mit_pfad(a_fmt(A0, "%s/%s", S, IM_REST[i])) != ko) fehlt = IM_REST[i];
        pruefe(ko && !ko->StoreTeil && ko->ToolOrdner && !strcmp(ko->ToolOrdner, S) && ko->ToolKachel == ps && !fehlt,
               a_fmt(A0, "der Rest (Einstellungen, falscher und kaputter Beleg, Verknüpfung, Kleinschreibung, angefangene Arbeit laut journal.json) ist EIN Eintrag bei ProsperoStore (%s%s%s)",
                     z(ko), fehlt ? " | nicht darin: " : "", fehlt ? fehlt : ""));
        eintrag *cp = mit_pfad(a_fmt(A0, "%s/cache/pakete/Notizbuch - CUSA40003 - v1.01 - Update.pkg", S)), *cr = mit_pfad(a_fmt(A0, "%s/cache/bild.png", S));
        pruefe(cp && cp->PkgArt == PKG_UPDATE && cp->ToolKachel == ap && cr && cr != ko && cr == mit_pfad(a_fmt(A0, "%s/cache/pakete/info.txt", S)) && !cr->StoreTeil
               && cr->ToolOrdner && !strcmp(cr->ToolOrdner, S) && cr->ToolKachel == ps,
               a_fmt(A0, "PKG-Datei im Rest steht bei ihrem Spiel, der Rest ihres Unterordners bleibt bei ProsperoStore (Codex-Prüfung 137) (%s | %s)", z(cp), z(cr)));
        {   /* Codex-Prüfung 137: kein Pfad eines Eintrags liegt in einem Pfad eines anderen (bei den Ordnern von ProsperoStore) */
            const char *ueber = NULL;
            for (int i = 0; i < K->Funde.n && !ueber; i++) { eintrag *f = K->Funde.e[i];
                for (int j = 0; j < f->PfadeBasis.n && !ueber; j++) { const char *a = f->PfadeBasis.e[j]; size_t la = strlen(a);
                    if (!strstr(a, "/prosperostore")) continue;
                    for (int i2 = 0; i2 < K->Funde.n && !ueber; i2++) { eintrag *g = K->Funde.e[i2]; if (g == f) continue;
                        for (int j2 = 0; j2 < g->PfadeBasis.n; j2++) if (!strncmp(g->PfadeBasis.e[j2], a, la) && g->PfadeBasis.e[j2][la] == '/') { ueber = g->PfadeBasis.e[j2]; break; } } } }
            pruefe(!ueber, a_fmt(A0, "kein Teil von ProsperoStore liegt in einem anderen Eintrag (Codex-Prüfung 137) (%s)", ueber ? ueber : "-"));
        }
        eintrag *mk = mit_pfad(a_fmt(A0, "%s/staging/teil.tmp", M));
        pruefe(mk && mk == mit_pfad(a_fmt(A0, "%s/backup", M)) && !mk->StoreTeil && mk->ToolKachel == ps, a_fmt(A0, "der Rest auf der M.2 ist EIN Eintrag bei ProsperoStore (%s)", z(mk)));
        int ia = -1, ib = -1, ic = -1;
        for (int i = 0; i < K->AUFGEKLAPPT.n; i++) { const char *o = K->AUFGEKLAPPT.e[i];
            if (!strcmp(o, S)) ia = i; if (!strcmp(o, a_fmt(A0, "%s/previous", S))) ib = i; if (!strcmp(o, a_fmt(A0, "%s/receipts", S))) ic = i; }
        pruefe(ia >= 0 && ib > ia && ic > ia, a_fmt(A0, "aufgeteilte Ordner von ProsperoStore stehen in AUFGEKLAPPT, äußere vor inneren (%d %d %d)", ia, ib, ic));
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        if (fa) fa->Wahl = true;
        const char *s = kern_endgueltig_satz(K, A0);
        pruefe(strstr(s, "1 ältere Fassung einer App (ProsperoStore)") != NULL, a_fmt(A0, "Satz vor dem Löschen: %s", s));
        if (fa) fa->Wahl = false;
    }

    printf("== NEU 1.0.9 (#146): Daten-Ordner bekannter Apps (aus ihrem Quellcode)\n");
    {
        eintrag *pc = kachel("PPSA11018"), *pw = mit_pfad("/user/data/prospero-win"), *sw = mit_pfad("/user/data/savagewind");
        eintrag *vl = mit_pfad("/user/data/vlc"), *vx = mit_pfad("/user/data/vlcx");
        pruefe(pc && pw && pw->ToolKachel == pc && kern_ampel(pw) == AMPEL_ROT && strstr(z(pw), "deine Windows-Spiele") && !strstr(z(pw), "gehört zu"),
               a_fmt(A0, "prospero-win steht bei PC5, obwohl es anders heißt - rot, eigene Spiele und Schlüssel (%s)", z(pw)));
        pruefe(sw && !sw->ToolKachel && sw->Kat == 6 && sw->Art == ART_TOOL_DATEN_WICHTIG && kern_ampel(sw) == AMPEL_ROT && strstr(z(sw), "gehört zu Breath of the Wild")
               && strstr(kern_art_info(A0, sw), "Spieldateien aus deinem eigenen Dump"),
               a_fmt(A0, "App nicht installiert: savagewind bleibt unter Apps, rot, mit dem Namen der App (%s)", z(sw)));
        pruefe(vl && !vl->ToolKachel && vl->Kat == 6 && vl->Art == ART_TOOL_DATEN && strstr(z(vl), "vlc – Einstellungen – gehört zu VLC"),
               a_fmt(A0, "App nicht installiert, nur Einstellungen: gelb, mit dem Namen der App (%s)", z(vl)));
        pruefe(vx && vx->Kat == 5 && !strstr(z(vx), "gehört zu"), a_fmt(A0, "Gegenprobe: unbekannter Ordner bleibt unter Sonstiges (%s, Bereich %d)", z(vx), vx ? vx->Kat : -1));
        eintrag *hp = mit_pfad("/user/data/homebrew/prospero-win");
        pruefe(hp && hp->ToolKachel != pc && !strstr(z(hp), "gehört zu"),
               a_fmt(A0, "gleichnamiger Ordner in homebrew ist nicht belegt - nicht fest bei PC5, kein \"gehört zu\" (Codex-Prüfung 137) (%s)", z(hp)));
        eintrag *pmk = kachel("ILNY26228"), *pmu = mit_pfad("/user/data/user");   /* PS5-Test 09.11.2026: Patch Manager (ps-patch-system) */
        pruefe(pmk && pmu && pmu->ToolKachel == pmk && kern_ampel(pmu) == AMPEL_ROT && strstr(z(pmu), "Deine Spiel-Patches und ihre Sicherungen") && !strstr(z(pmu), "ps-patch-system") && !strstr(z(pmu), "user –"),
               a_fmt(A0, "Ordner \"user\" steht beim Patch Manager - rot, deine Spiel-Patches (%s)", z(pmu)));
    }

    printf("== Löschlauf: 8 Zusatzinhalte (eine Zeile) und der Rest des Ordners PS4\n");
    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
    if (ndl && rest) {
        ndl->Wahl = rest->Wahl = true;
        if (st_mo) st_mo->Wahl = true;   /* NEU 1.0.9 (#145) */
        ergebnis r = kern_fuehre_loeschung(K, ja, NULL);
        int weg = 0; for (int i = 0; i < 8; i++) if (!da(a_fmt(A0, "/mnt/usb0/DLC/EP0001-CUSA40002_00-NEBELDLC0000000%d.pkg", i))) weg++;
        pruefe(weg == 8 && r.Fehler == 0, a_fmt(A0, "alle 8 Dateien der Zeile gelöscht (%d, Fehler %d)", weg, r.Fehler));
        pruefe(da("/mnt/ext1/PKG/EP0001-CUSA40002_00-NEBELDLC00000010.pkg") && da("/mnt/ext1/PKG/Nebelpfad - CUSA40002 - v1.00 - EU.pkg"), "Zusatzinhalte und Installationsdatei auf der M.2 bleiben");
        pruefe(!da("/mnt/usb0/PS4/Cover/bild.jpg") && !da("/mnt/usb0/PS4/liesmich.txt"), "Rest des Ordners PS4 gelöscht");
        pruefe(da("/mnt/usb0/PS4/Wolkenreise - CUSA40001 - v1.00 - EU.pkg") && da("/mnt/usb0/PS4/Nebelpfad - CUSA40002 - v1.05 - EU - Update.pkg") && da("/mnt/usb0/PS4/unbekannt.pkg"),
               "die PKG-Dateien im Ordner PS4 bleiben (nicht gewählt)");
        pruefe(st_mo && !da("/mnt/ext1/prosperostore/previous/PPSA99709") && da("/mnt/ext1/prosperostore/staging/PPSA99709.zip") && da("/user/data/prosperostore/receipts/PPSA99708.json"),
               "aufgehobene Fassung einer gelöschten App gelöscht - ihr angefangener Download und die Belege bleiben (nicht gewählt) (NEU 1.0.9, #145)");
    } else pruefe(false, "Löschlauf: Einträge fehlen");
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
