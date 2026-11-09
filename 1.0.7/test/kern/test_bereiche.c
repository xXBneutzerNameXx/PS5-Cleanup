/* PS5 Cleanup 1.0.7 - Test der 7 Bereiche (da_Chris86 31.10.2026)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_bereiche <hier>   Testbaum <hier>/w = download_v1 + Zusätze aus lauf_bereiche.sh
 * Prüft: Reihenfolge und Namen der Bereiche; Spiel/App nach Sony-Kategorie (param.json applicationCategoryType,
 * param.sfo CATEGORY); bekannte Tool-Daten unter Apps, unbekannte Ordner und Fake-Lizenzen unter Sonstiges;
 * alle Jailbreak-Einstiege unter Jailbreak, nur einzeln, nie in der vollständigen Bereinigung. */
#include "../../kern/kern.h"
#include "../../kern/ablauf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static int zw_weg = 0, zw_wuerde = 0;   /* 1.0.9: Zeilen zum mitgelöschten Aufnahme-Zwischenordner */
static void melde_log(void *c, const char *t)
{
    (void)c;
    if (!t || !strstr(t, "Aufnahme-Zwischenordner")) return;
    printf("PROT %s\n", t);
    if (strstr(t, "  gelöscht      – leerer Aufnahme-Zwischenordner /user/av_contents/content_tmp/PPSA10001")) zw_weg++;
    if (strstr(t, "  würde gelöscht – leerer Aufnahme-Zwischenordner /user/av_contents/content_tmp/PPSA10002")) zw_wuerde++;
}
static bool nein(void *c) { (void)c; return false; }
static kern *K; static arena *A0;
static eintrag *mit_pfad(const char *p)
{
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (!strcmp(f->PfadeBasis.e[j], p)) return f; }
    return NULL;
}
static void kat(const char *p, int soll, const char *was)
{
    eintrag *f = mit_pfad(p);
    pruefe(f && f->Kat == soll, a_fmt(A0, "%s: Bereich %d %s (%s, ist %d)", was, soll, kern_kat_name(soll), f ? f->Text : "nicht gefunden", f ? f->Kat : -1));
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 2) { fprintf(stderr, "Aufruf: test_bereiche <hier>\n"); return 2; }
    A0 = arena_neu();
    const char *W = VERB(A0, argv[1], "/w");
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    K = kern_neu(zugriff_datei(&o)); K->melde = melde_log;
    volatile int fe = 0;
    VERSUCHE { kern_suche_funde(K); } FANGE { printf("FAIL Suche abgebrochen: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) { printf("FEHLERZAHL 1\n"); return 1; }

    printf("== Reihenfolge und Namen\n");
    const int soll[KAT_ANZ] = { 1, 6, 2, 3, 4, 5, 7 };
    bool reihe = true; for (int i = 0; i < KAT_ANZ; i++) if (ALLE_KAT[i] != soll[i]) reihe = false;
    pruefe(reihe, "Reihenfolge: Spiele, Apps, Spielstände, Trophäen, Bilder & Videos, Sonstiges, Jailbreak");
    pruefe(!strcmp(kern_kat_name(1), "Spiele") && !strcmp(kern_kat_name(6), "Apps") && !strcmp(kern_kat_name(5), "Sonstiges") && !strcmp(kern_kat_name(7), "Jailbreak"),
           "Namen: Spiele / Apps / Sonstiges / Jailbreak");
    bool gueltig = true; for (int i = 0; i < K->Funde.n; i++) { int k = ((eintrag *)K->Funde.e[i])->Kat; if (k < 1 || k > 7) gueltig = false; }
    pruefe(gueltig, "jeder Eintrag hat einen der 7 Bereiche");

    printf("== Spiel oder App (Sony-Kategorie)\n");
    kat("/user/app/PPSA10001", 1, "PS5-Spiel (applicationCategoryType 0)");
    kat("/user/app/PPSA10002", 6, "PS5-Medien-App (applicationCategoryType 65536)");
    kat("/user/app/CUSA20001", 1, "PS4-Spiel (CATEGORY gd)");
    kat("/user/app/CUSA20002", 6, "PS4-App (CATEGORY gde)");
    kat("/user/app/PPSA55555", 1, "Spiel ohne Kategorie-Angabe bleibt Spiel");

    printf("== Daten-Ordner, Fake-Lizenzen\n");
    kat("/user/data/nanodns", 6, "Daten eines bekannten Tools ohne Kachel (nanoDNS)");
    kat("/user/data/ps5_autoloader", 6, "Autoloader-Einstellungen (Homebrew)");
    kat("/user/data/AMPRStudio/a.bin", 6, "AMPRStudio = LZ4-Game-Compressor (Recherche 31.10.2026: bekanntes Tool; 17:00: Inhalt ohne folder-source)");
    kat("/user/data/MeinOrdner", 5, "unbekannter Ordner");
    kat("/user/license/fakeCUSA99999.rif", 5, "Fake-Lizenz eines nicht mehr installierten Spiels");

    printf("== Jailbreak\n");
    kat("/user/home/u1/webkit/shell/local", 7, "Browserspeicher");
    kat("/user/app/CUSA17122", 7, "Lua-Jailbreak-Spiel");
    kat("/user/home/u1/savedata/CUSA17122", 7, "Spielstand des Lua-Spiels");
    kat("/user/home/u1/savedata/CUSA02199", 7, "PS2-Spielstand-Jailbreak (Okage)");
    bool alle_einzeln = true; int jb = 0;
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat != 7) continue; jb++; if (!f->NurEinzeln && !f->Gesperrt) alle_einzeln = false; }
    pruefe(jb > 0 && alle_einzeln, a_fmt(A0, "alle %d Einträge unter Jailbreak: nur einzeln oder gesperrt", jb));
    pruefe(kern_anzahl_komplett(K, 7) == 0, "vollständige Bereinigung zählt aus Jailbreak nichts");
    bool jb_woanders = false;
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Jailbreak && f->Kat != 7) jb_woanders = true; }
    pruefe(!jb_woanders, "kein Jailbreak-Einstieg außerhalb von Jailbreak");

    /* NEU 1.0.7 (Grundlage da_Chris86 31.10.2026 13:12, Freigabe Textliste 13:16) */
    printf("== 1.0.7: Arten, Ampel, Rotes nur einzeln\n");
    static ablauf AB; ablauf_ui ui = {0}; ablauf_init(&AB, K, &ui);
    eintrag *nd = mit_pfad("/user/data/nanodns"), *ga = mit_pfad("/user/data/garlic"), *gb = mit_pfad("/user/data/game-backups/Spiel.exfat"), *fl = mit_pfad("/user/license/fakeCUSA99999.rif");
    pruefe(nd && nd->Art == ART_TOOL_DATEN && kern_ampel(nd) == AMPEL_GELB && !nd->Rot, "nanoDNS-Daten: gelb, \"startet mit Grundeinstellungen\"");
    pruefe(ga && ga->Kat == 6 && ga->Art == ART_TOOL_DATEN_WICHTIG && ga->Rot, "garlic-Daten (Spielstand-Backups): Apps, rot");
    pruefe(gb && gb->Art == ART_SPIEL_SICHERUNG && gb->Rot, a_fmt(A0, "game-backups (LZ4-Game-Compressor): rot (%s, Art %d)", gb ? gb->Text : "nicht gefunden", gb ? gb->Art : -1));
    pruefe(fl && fl->Art == ART_FAKE_LIZENZ && kern_ampel(fl) == AMPEL_GELB, "Fake-Lizenz: gelb, \"kommt mit dem Spiel wieder\"");
    pruefe(ga && strstr(kern_zeile(A0, ga), "Spielstand-Backups") != NULL, a_fmt(A0, "Zeile garlic nennt die Backups (%s)", ga ? kern_zeile(A0, ga) : "-"));
    bool alle_art = true; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (!kern_folge(f)[0]) alle_art = false; }
    pruefe(alle_art, "jede Zeile hat einen kurzen Satz (kern_folge)");
    ablauf_bereich_setzen(&AB, 6, true);
    pruefe(nd && nd->Wahl, "ganzen Bereich Apps wählen: nanoDNS-Daten gewählt");
    pruefe(ga && !ga->Wahl, "ganzen Bereich Apps wählen: garlic (rot) NICHT gewählt - nur einzeln");
    const char *gr; pruefe(ga && !ablauf_eintrag_gesperrt(&AB, ga, &gr) && gr && strstr(gr, "endgültig"), "garlic einzeln wählbar, Hinweis \"danach endgültig weg\"");
    ablauf_bereich_setzen(&AB, 6, false);
    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
    ablauf_eintrag_setzen(&AB, ga, true);
    eintrag *sp = NULL; for (int i = 0; i < K->Funde.n && !sp; i++) { eintrag *f = K->Funde.e[i]; if (f->Art == ART_SPIELSTAND && !f->Gesperrt) sp = f; }
    if (sp) ablauf_eintrag_setzen(&AB, sp, true);
    const char *satz = kern_endgueltig_satz(K, A0);
    pruefe(strstr(satz, "garlic") && (!sp || strstr(satz, "Spielst")), a_fmt(A0, "Satz vor dem Löschen konkret: %s", satz));
    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;

    printf("== 1.0.7: Einhängeordner nie angeboten\n");
    pruefe(!mit_pfad("/user/data/imgmnt"), "kstuff-lite-Einhängeordner /data/imgmnt nicht in der Liste");
    pruefe(!mit_pfad("/user/data/save_mnt"), "garlic-Einhängeordner /data/save_mnt nicht in der Liste");
    pruefe(!mit_pfad("/user/data/.kstuff_noautomount"), "Merkdatei .kstuff_noautomount nicht in der Liste");
    pruefe(!kern_erlaubt(K, "/user/data/imgmnt/ufsmnt/x") && !kern_erlaubt(K, "/mnt/ps5upload/x"), "Einhängeordner: Löschen nie erlaubt");

    printf("== 1.0.7: Update/Zusatzinhalt nur mit dem Spiel\n");
    eintrag *up = NULL; for (int i = 0; i < K->Funde.n && !up; i++) { eintrag *f = K->Funde.e[i]; if (f->Teil && strcmp(f->Teil, "Spiel") && kern_nur_mit_spiel(K, f)) up = f; }
    if (!up) printf("(kein Update/Zusatzinhalt eines installierten Spiels im Testbaum)\n");
    else {
        bool gs = ablauf_eintrag_gesperrt(&AB, up, &gr);
        pruefe(gs && gr && strstr(gr, "zusammen mit dem Spiel"), a_fmt(A0, "%s: einzeln nicht wählbar (%s)", up->Text, gr ? gr : "-"));
        eintrag *spiel = NULL; for (int i = 0; i < K->Funde.n && !spiel; i++) { eintrag *f = K->Funde.e[i]; if (ist_teil(f, "Spiel") && f->Gruppe && txt_gleich_ci(f->Gruppe, up->Gruppe)) spiel = f; }
        if (spiel) { ablauf_eintrag_setzen(&AB, spiel, true); pruefe(up->Wahl, "Spiel gewählt: Update/Zusatzinhalt geht mit"); ablauf_eintrag_setzen(&AB, spiel, false); pruefe(!up->Wahl, "Spiel abgewählt: Update/Zusatzinhalt auch"); }
    }

    printf("== 1.0.7: ShadowMount - Kachel und Spieldatei als zwei Zeilen\n");
    int smk = 0, smd = 0; bool paar = true;
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i];
        if (f->SmDatei) { smd++; if (!f->SmKachel || i == 0 || K->Funde.e[i - 1] != f->SmKachel || f->Pfade.n != 1 || strcmp(f->Pfade.e[0], f->SmKachel->Quelle)) paar = false; }
        if (f->Quelle && f->Quelle[0]) smk++;
    }
    pruefe(smk == smd && paar, a_fmt(A0, "jede Kachel mit Spieldatei hat direkt danach ihre Zeile \"Spieldatei\" (%d Kacheln, %d Zeilen)", smk, smd));
    eintrag *d = NULL; for (int i = 0; i < K->Funde.n && !d; i++) { eintrag *f = K->Funde.e[i]; if (f->SmDatei && !f->Gesperrt && !f->Jailbreak) d = f; }
    if (!d) printf("(kein ShadowMount-Spiel im Testbaum)\n");
    else {
        eintrag *kt = d->SmKachel;
        pruefe(kern_ampel(kt) == AMPEL_GRUEN && d->Rot, "Kachel grün (kommt wieder), Spieldatei rot");
        ablauf_bereich_setzen(&AB, d->Kat, true);
        pruefe(kt->Wahl && !d->Wahl, "ganzen Bereich wählen: Kachel ja, Spieldatei nein");
        kern_sm_nach_auswahl(K);
        bool ohne = true; for (int j = 0; j < kt->Pfade.n; j++) if (!strcmp(kt->Pfade.e[j], kt->Quelle)) ohne = false;
        pruefe(ohne, "nur Kachel: Spieldatei nicht unter den Löschpfaden");
        ablauf_bereich_setzen(&AB, d->Kat, false);
        ablauf_eintrag_setzen(&AB, d, true);
        pruefe(kt->Wahl, "Spieldatei gewählt: Kachel geht mit");
        pruefe(ablauf_eintrag_gesperrt(&AB, kt, &gr), "Kachel dann nicht einzeln abwählbar (\"Geht mit der Spieldatei weg\")");
        kern_sm_nach_auswahl(K);
        bool mit = false; for (int j = 0; j < kt->Pfade.n; j++) if (!strcmp(kt->Pfade.e[j], kt->Quelle)) mit = true;
        pruefe(mit, "Spiel + Kachel: Spieldatei unter den Löschpfaden der Kachel");
        pliste g = kern_gewaehlt(K); bool zeile_dabei = false; for (int j = 0; j < g.n; j++) if (((eintrag *)g.e[j])->SmDatei) zeile_dabei = true;
        pruefe(!zeile_dabei, "die Zeile \"Spieldatei\" selbst wird nie gelöscht (nicht in kern_gewaehlt)");
        pruefe(strstr(kern_endgueltig_satz(K, A0), "Spieldatei") != NULL, "Satz vor dem Löschen nennt die Spieldatei");
        ablauf_eintrag_setzen(&AB, d, false); kern_sm_zuruecksetzen(K);
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        kern_waehle_komplett(K, true, true);
        pruefe(kt->Wahl && d->Wahl == (d->Ort != ORT_STICK), "vollständige Bereinigung (Speicher wird geleert): Kachel und Spieldatei gewählt");
        int n1 = kern_anzahl_komplett(K, d->Kat), n2 = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == d->Kat && !f->Gesperrt && !f->Paket && !f->NurEinzeln) n2++; }
        pruefe(n1 < n2, "Zählung der vollständigen Bereinigung zählt die Zeile \"Spieldatei\" nicht doppelt");
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        kern_sm_zuruecksetzen(K);
    }
    printf("== 1.0.7 (31.10.2026 17:00): folder-source, PlayGo-Reste, Spielstand-Meta\n");
    {
        eintrag *am = mit_pfad("/user/data/AMPRStudio/a.bin");
        bool fs = false; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (strstr(f->PfadeBasis.e[j], "/folder-source") && !strstr(f->PfadeBasis.e[j], "folder-source.json")) fs = true; }
        pruefe(am && mit_pfad("/user/data/AMPRStudio/folder-source.json") == am && !mit_pfad("/user/data/AMPRStudio"), a_fmt(A0, "AMPRStudio: Inhalt ohne folder-source (%s)", am ? sl_join(A0, &am->PfadeBasis, ", ") : "nicht gefunden"));
        pruefe(!fs, "der eingeblendete Spielordner folder-source steht in keinem Eintrag");
        {   /* 31.10.2026 17:25: leeres folder-source = nichts eingeblendet -> ganzer Tool-Ordner samt leerem folder-source */
            eintrag *hb = mit_pfad("/user/data/AMPRLeer");
            pruefe(hb && !hb->ToolOrdner && !mit_pfad("/user/data/AMPRLeer/b.bin"), a_fmt(A0, "leeres folder-source: Tool-Ordner ganz (%s)", hb ? sl_join(A0, &hb->PfadeBasis, ", ") : "nicht gefunden"));
        }
        pruefe(am && am->ToolOrdner && am->Art == ART_TOOL_DATEN, a_fmt(A0, "AMPRStudio bleibt Tool-Daten (Art %d, %s)", am ? am->Art : -1, am ? kern_zeile(A0, am) : "-"));
        pruefe(!kern_erlaubt(K, "/user/data/AMPRStudio/folder-source") && !kern_erlaubt(K, "/user/data/AMPRStudio/folder-source/eboot.bin"), "folder-source: Löschen nie erlaubt");
        const char *pg = "/user/playgo/app/EP1004-CUSA77701_00-ABCDEFGHIJKLMNOP", *pgp = "/user/playgo/patch/EP1004-CUSA77701_00-ABCDEFGHIJKLMNOP";
        eintrag *p1 = mit_pfad(pg);
        pruefe(p1 && mit_pfad(pgp) == p1 && p1->Kat == 5 && p1->Art == ART_REST && kern_ampel(p1) == AMPEL_GRUEN, a_fmt(A0, "PlayGo-Rest (Spiel nicht installiert): ein Eintrag, Sonstiges, grün (%s)", p1 ? p1->Text : "nicht gefunden"));
        pruefe(!mit_pfad("/user/playgo/app/EP0000-CUSA20001_00-OLDGAME000000000"), "PlayGo eines installierten Spiels wird nicht angeboten");
        pruefe(kern_erlaubt(K, pg) && !kern_erlaubt(K, "/user/playgo/app") && !kern_erlaubt(K, "/user/playgo/app/EP1004-NPXS40001_00-ABCDEFGHIJKLMNOP"), "PlayGo: nur einzelne Content-ID-Ordner erlaubt, nie der Ordner, nie NPXS");
        const char *sd = "/user/home/1234abcd/savedata_prospero/PPSA77702", *sm = "/user/home/1234abcd/savedata_prospero_meta/user/PPSA77702";
        eintrag *s1 = mit_pfad(sd);
        pruefe(s1 && mit_pfad(sm) == s1 && s1->Art == ART_SPIELSTAND, a_fmt(A0, "Spielstand nimmt seine Meta-Daten mit (%s)", s1 ? sl_join(A0, &s1->PfadeBasis, ", ") : "nicht gefunden"));
        pruefe(kern_erlaubt(K, sm) && !kern_erlaubt(K, "/user/home/1234abcd/savedata_prospero_meta/user/NPXS40001") && !kern_erlaubt(K, "/user/home/1234abcd/savedata_prospero_meta"), "Spielstand-Meta: nur je Kennung erlaubt, nie NPXS, nie der Ordner");
        pruefe(!kern_erlaubt(K, "/user/temp/share/PSCL00002"), "eigene PS5-Ordner nur während der Selbstentfernung erlaubt");
        pruefe(!kern_erlaubt(K, "/user/av_contents/content_tmp/PSCL00002") && !kern_erlaubt(K, "/user/av_contents/content_tmp/PSCL00001") && kern_erlaubt(K, "/user/av_contents/content_tmp/PPSA12000"),
               "1.0.9: eigener Aufnahme-Zwischenordner nur während der Selbstentfernung, der anderer Apps wie bisher");
        eintrag *sx = mit_pfad("/system_ex/app/PPSA88801");
        /* 03.11.2026 (da_Chris86 15:11): Playden selbst (FAKE77031) ist nicht installiert - sein leerer Ordner geht jetzt mit */
        pruefe(sx && sx->NurLeer && sx->Kat == 5 && sx->Art == ART_REST && kern_ampel(sx) == AMPEL_GRUEN && sx->PfadeBasis.n == 2 && mit_pfad("/system_ex/app/FAKE77031") == sx,
               a_fmt(A0, "leere Einhängeordner ohne Spiel (auch Playden selbst, nicht installiert): ein Eintrag, Sonstiges, grün, nur rmdir (%s)", sx ? sl_join(A0, &sx->PfadeBasis, ", ") : "nicht gefunden"));
        pruefe(!mit_pfad("/system_ex/app/PPSA10001") && !mit_pfad("/system_ex/app/PPSA88802"), "nicht angeboten: Spiel noch installiert / Ordner nicht leer");
        pruefe(!mit_pfad("/system_ex/app/FAKE78001") && !mit_pfad("/system_ex/app/NPXS40000") && !mit_pfad("/system_ex/app/PSCL00002"),
               "nie angeboten: von Playden verwaltete Spiele, Sony, PS5 Cleanup");
        pruefe(kern_erlaubt(K, "/system_ex/app/PPSA88801") && !kern_erlaubt(K, "/system_ex/app") && !kern_erlaubt(K, "/system_ex/app/PPSA88801/x") && !kern_erlaubt(K, "/system_ex/app/NPXS40000")
               && !kern_erlaubt(K, "/system_ex/app/FAKE00000") && !kern_erlaubt(K, "/system_ex/common_ex"), "/system_ex: nur einzelne Einhängeordner erlaubt, sonst nichts");
    }
    printf("== 1.0.7 (PS5-Test 01.11.2026): Trophaeen eines PS4-Spiels - npbind.dat direkt im Kachel-Info-Ordner\n");
    {
        eintrag *t1 = mit_pfad("/user/home/1234abcd/trophy/data/NPWR09412_00"), *t2 = mit_pfad("/user/trophy/conf/NPWR09412_00");
        pruefe(t1 && strstr(t1->Text, "Old Game (NPWR09412_00)") && !strstr(t1->Text, "nicht mehr"), a_fmt(A0, "Trophaeen-Fortschritt gehoert zum PS4-Spiel (%s)", t1 ? t1->Text : "nicht gefunden"));
        pruefe(t2 && strstr(t2->Text, "Old Game (NPWR09412_00)") && strstr(t2->Text, "Spiel noch da"), a_fmt(A0, "Trophaeen-Liste: Spiel noch da (%s)", t2 ? t2->Text : "nicht gefunden"));
    }
    printf("== 1.0.9 (da_Chris86 08.11.2026, Bild aus 1.0.8): leere Aufnahme-Zwischenordner nur von Apps, die nicht mehr da sind\n");
    {
        eintrag *z = NULL;
        for (int i = 0; i < K->Funde.n && !z; i++) { eintrag *f = K->Funde.e[i]; if (strstr(f->Text, "Leere Aufnahme-Zwischenordner")) z = f; }
        pruefe(z && z->PfadeBasis.n == 1 && !strcmp(z->PfadeBasis.e[0], "/user/av_contents/content_tmp/PPSA12345"),
               a_fmt(A0, "angeboten nur der Ordner einer App, die nicht mehr da ist (%s)", z ? sl_join(A0, &z->PfadeBasis, ", ") : "nicht gefunden"));
        pruefe(!mit_pfad("/user/av_contents/content_tmp/PPSA10001") && !mit_pfad("/user/av_contents/content_tmp/PSCL00002") && !mit_pfad("/user/av_contents/content_tmp/PSCL00001")
               && !mit_pfad("/user/av_contents/content_tmp/NPXS40001") && !mit_pfad("/user/av_contents/content_tmp/PPSA10002"),
               "nicht angeboten: installierte Apps, PS5 Cleanup (auch nicht leer), Systemprogramm");
        pruefe(!mit_pfad("/user/av_contents/content_tmp/PPSA55501") && !mit_pfad("/user/av_contents/content_tmp/PPSA55502"), "nicht angeboten: nur Kachel-Infos (appmeta bzw. system_data) zählen als noch da");
        {   /* Testmodus: würde mitgelöscht, bleibt aber */
            datei_optionen o2 = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
            kern *K0 = K; K = kern_neu(zugriff_sim(zugriff_datei(&o2))); K->melde = melde_log;
            volatile int fe2 = 0;
            VERSUCHE { kern_suche_funde(K); } FANGE { printf("FAIL Suche (Testmodus) abgebrochen: %s\n", ausn_text); fe2 = 1; } ENDE_VERSUCHE;
            eintrag *v = NULL;
            for (int i = 0; !fe2 && i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; f->Wahl = false; if (f->KachelId && !strcmp(f->KachelId, "PPSA10002")) v = f; }
            pruefe(v != NULL, "Testmodus: Eintrag der App PPSA10002 mit Kachel");
            if (v) {
                K->simulation = true; v->Wahl = true; kern_abhaengige_mitnehmen(K);
                ergebnis es = kern_fuehre_loeschung(K, nein, NULL);
                struct stat s2;
                pruefe(es.Frei.Ordner == (v->HatGroesse ? v->Ordner : 0) + 1, a_fmt(A0, "Testmodus: würde frei zählt den Ordner mit (%lld)", (long long)es.Frei.Ordner));
                pruefe(es.Fehler == 0 && zw_wuerde == 1 && lstat(VERB(A0, W, "/user/av_contents/content_tmp/PPSA10002"), &s2) == 0 && lstat(VERB(A0, W, "/user/app/PPSA10002"), &s2) == 0,
                       a_fmt(A0, "Testmodus: \"würde gelöscht – leerer Aufnahme-Zwischenordner\", nichts gelöscht (Zeilen %d, Fehler %d)", zw_wuerde, es.Fehler));
            }
            K = K0;
        }
        eintrag *g = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; f->Wahl = false; if (f->KachelId && !strcmp(f->KachelId, "PPSA10001")) g = f; }
        pruefe(g != NULL, a_fmt(A0, "Eintrag der App PPSA10001 mit Kachel (%s)", g ? g->Text : "nicht gefunden"));
        struct stat st;
        if (g) {
            g->Wahl = true;
            kern_abhaengige_mitnehmen(K);
            ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
            pruefe(e.Fehler == 0 && lstat(VERB(A0, W, "/user/app/PPSA10001"), &st) != 0, a_fmt(A0, "App PPSA10001 gelöscht (Fehler %d)", e.Fehler));
            pruefe(lstat(VERB(A0, W, "/user/av_contents/content_tmp/PPSA10001"), &st) != 0 && zw_weg == 1, a_fmt(A0, "ihr leerer Aufnahme-Zwischenordner ist im selben Durchgang mit weg, im Protokoll (%d)", zw_weg));
            pruefe(e.Frei.Ordner == (g->HatGroesse ? g->Ordner : 0) + 1, a_fmt(A0, "Freigeräumt zählt den Ordner mit (%lld Ordner, Eintrag %s %lld)", (long long)e.Frei.Ordner,
                   g->HatGroesse ? "mit Größe" : "ohne Größe", (long long)g->Ordner));
            pruefe(lstat(VERB(A0, W, "/user/av_contents/content_tmp/PSCL00002"), &st) == 0 && lstat(VERB(A0, W, "/user/av_contents/content_tmp/PPSA12345"), &st) == 0
                   && lstat(VERB(A0, W, "/user/av_contents/content_tmp/NPXS40001"), &st) == 0, "die anderen Ordner bleiben (nicht gewählt, PS5 Cleanup, Systemprogramm)");
        }
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
