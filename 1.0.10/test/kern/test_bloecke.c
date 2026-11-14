/* PS5 Cleanup - Test "ein Block pro Kachel" (da_Chris86, PS5-Test 02.11.2026, finaler Plan 1.0.7 Punkte B1-B6)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_bloecke <hier> 0   Blöcke, Sonstiges, alte Sicherungen, Trophäen-Listen, Aufnahmen-Kopien
 *   test_bloecke <hier> 1   mit Selbstschutz: nicht lesbare Einzeldateien bleiben still stehen, kein falscher Zusatz */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void melde_log(void *c, const char *t) { (void)c; (void)t; }
static kern *K; static arena *A0;
static bool nie_stopp(void *c) { (void)c; return false; }          /* NEU 1.0.8 (2.6): alles zählen wie die App im Hintergrund */
static void nichts_tun(void *c, eintrag *e) { (void)c; (void)e; }
static eintrag *mit_pfad(const char *p)
{
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (!strcmp(f->PfadeBasis.e[j], p)) return f; }
    return NULL;
}
static eintrag *mit_pfad_anfang(const char *p)
{
    size_t l = strlen(p);
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (!strncmp(f->PfadeBasis.e[j], p, l) && (f->PfadeBasis.e[j][l] == 0 || f->PfadeBasis.e[j][l] == '/')) return f; }
    return NULL;
}
static const char *kopf(const eintrag *f) { return f && f->ToolKachel && f->ToolKachel->KachelId ? f->ToolKachel->KachelId : "-"; }
static void unter(const char *p, const char *id, int kat, const char *was)
{
    eintrag *f = mit_pfad(p); if (!f) f = mit_pfad_anfang(p);
    pruefe(f && !strcmp(kopf(f), id) && f->Kat == kat, a_fmt(A0, "%s: unter %s, Bereich %d (%s, Kopf %s, Bereich %d)", was, id, kat, f ? kern_zeile(A0, f) : "nicht gefunden", kopf(f), f ? f->Kat : -1));
}
static void frei_stehend(const char *p, int kat, const char *was)
{
    eintrag *f = mit_pfad(p); if (!f) f = mit_pfad_anfang(p);
    pruefe(f && !f->ToolKachel && f->Kat == kat, a_fmt(A0, "%s: steht für sich, Bereich %d (%s, Kopf %s, Bereich %d)", was, kat, f ? kern_zeile(A0, f) : "nicht gefunden", kopf(f), f ? f->Kat : -1));
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: test_bloecke <hier> 0|1\n"); return 2; }
    A0 = arena_neu();
    const char *W = a_fmt(A0, "%s/w", argv[1]);
    bool schutz = atoi(argv[2]) == 1;
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    K = kern_neu(zugriff_datei(&o)); K->melde = melde_log;
    K->EIGENES_PROTOKOLL = "/mnt/usb0/ps5cleanup_protokoll_2026-11-02_210000.txt";   /* NEU 1.0.7: in diese Datei schreibt PS5 Cleanup gerade */
    volatile int fe = 0;
    VERSUCHE {
        if (schutz) { kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K); }
        kern_suche_funde(K);
    } FANGE { printf("FAIL Suche abgebrochen: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) { printf("FEHLERZAHL 1\n"); return 1; }

    /* immer: kein Pfad steht in zwei Einträgen */
    {
        mtab gesehen = mt_neu(true); bool doppelt = false; const char *dp = "";
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) {
            if (mt_hat(&gesehen, f->PfadeBasis.e[j])) { doppelt = true; dp = f->PfadeBasis.e[j]; } else mt_setze(A0, &gesehen, f->PfadeBasis.e[j], (void *)1); } }
        pruefe(!doppelt, a_fmt(A0, "kein Pfad in zwei Einträgen (%s)", dp));
    }

    if (!schutz) {
        printf("== Ein Block pro Kachel: über die Spielnummer\n");
        unter("/mnt/usb0/Rennspiel - CUSA31001 - v1.00 - EU.pkg", "CUSA31001", 1, "PKG des PS4-Spiels auf dem Stick");
        unter("/mnt/usb0/Rennspiel - CUSA31001 - v1.04 - EU - Update.pkg", "CUSA31001", 1, "Update-PKG des PS4-Spiels auf dem Stick");
        eintrag *pk = mit_pfad("/mnt/usb0/Rennspiel - CUSA31001 - v1.00 - EU.pkg");
        pruefe(pk && pk->Art == ART_PAKET_DATEI && pk->KatHer == 5, a_fmt(A0, "PKG bleibt Installationsdatei (Art %d, vorher Bereich %d)", pk ? pk->Art : -1, pk ? pk->KatHer : -1));
        {   /* 1.0.8 (PS5-Test 06.11.2026, Befund 12): PKG eines nicht installierten Spiels - eigener Block "<Spiel> · nicht installiert" unter Spiele, rot */
            eintrag *an = mit_pfad("/mnt/usb0/Anderes Spiel - CUSA39999 - v1.00 - EU.pkg");
            pruefe(an && an->Kat == 1 && an->KatHer == 5 && an->PkgOhneSpiel && an->BlockName && !strcmp(an->BlockName, "Anderes Spiel · nicht installiert")
                   && !an->ToolKachel && kern_ampel(an) == AMPEL_ROT && !strcmp(kern_folge_kurz(an), "endgültig weg"),
                   a_fmt(A0, "PKG eines nicht installierten Spiels: Block \"%s\" unter Spiele, rot (%s, Bereich %d)", an && an->BlockName ? an->BlockName : "-", an ? kern_zeile(A0, an) : "-", an ? an->Kat : -1));
        }
        unter("/user/data/homebrew/FAKE71001", "FAKE71001", 1, "homebrew/<Kennung> unter ihrer Kachel");
        unter("/user/data/homebrew/PPSA71003", "PPSA71003", 1, "homebrew/<Kennung> unter ihrer Kachel");
        printf("== Ein Block pro Kachel: über den Namen der App\n");
        unter("/user/data/playden", "FAKE71001", 1, "playden (App unter Spiele, wie die PS5 sie zeigt)");
        {   /* NEU 1.0.10 (da_Chris86 14.11.2026 19:51): Playdens eigene Daten - rot, Playden startet danach nicht mehr */
            eintrag *pd = mit_pfad("/user/data/playden");
            pruefe(pd && pd->Art == ART_TOOL_DATEN_WICHTIG && kern_ampel(pd) == AMPEL_ROT && !strcmp(kern_folge_kurz(pd), "Playden startet nicht mehr")
                   && strstr(kern_art_info(A0, pd), "startet Playden nicht mehr"),
                   a_fmt(A0, "Playdens eigene Daten: rot, \"Playden startet nicht mehr\" (Art %d, %s)", pd ? pd->Art : -1, pd ? kern_folge_kurz(pd) : "-"));
        }
        unter("/user/data/playden-games", "FAKE71001", 1, "playden-games");
        unter("/user/data/playden-import", "FAKE71001", 1, "playden-import");
        unter("/mnt/ext1/playden", "FAKE71001", 1, "playden auf der M.2");
        unter("/mnt/usb0/playden", "FAKE71001", 1, "playden auf dem USB-Stick");
        unter("/user/data/blackbox-v2", "PPSA71002", 1, "blackbox-v2");
        unter("/user/data/prosperostore", "PPSA71003", 1, "prosperostore (gleicher Name zählt vor ähnlichem)");
        unter("/user/data/prosperoeden", "PPSA71004", 1, "prosperoeden (ProsperoEden Switch UI)");
        unter("/user/data/SpectrumLibrary", "SLIB71005", 6, "SpectrumLibrary unter der App (Apps)");
        unter("/user/data/orbit-store", "ORBT71006", 6, "orbit-store unter der Tool-Kachel");
        unter("/mnt/usb0/orbit_store_v1.2.elf", "ORBT71006", 6, "ELF auf dem Stick unter der Tool-Kachel");
        printf("== Tool-Kachel, die sich bei Sony als Spiel meldet (Spielekompressor)\n");
        {   /* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86 17:35/19:13): wie im PS5-Menü unter Spiele, bleibt aber eine Tool-Kachel */
            eintrag *sk = mit_pfad("/user/app/PSGC50001");
            pruefe(sk && sk->Kat == 1 && sk->KatHer == 6 && sk->Art == ART_TOOL_KACHEL && !sk->ToolKachel,
                   a_fmt(A0, "Spielekompressor: Kachel unter Spiele, Art Tool-Kachel (%s, Bereich %d, vorher %d, Art %d)", sk ? kern_zeile(A0, sk) : "nicht gefunden", sk ? sk->Kat : -1, sk ? sk->KatHer : -1, sk ? sk->Art : -1));
            unter("/user/data/GameCompressor", "PSGC50001", 1, "Spielekompressor: Daten-Ordner unter seiner Kachel (Spiele)");
            unter("/mnt/usb0/game-compressor_v1.0.4.elf", "PSGC50001", 1, "Spielekompressor: Programmdatei unter seiner Kachel (Spiele)");
            eintrag *gd = mit_pfad("/user/data/GameCompressor");
            pruefe(gd && gd->Art != ART_UNBEKANNT && gd->Art != ART_ABBILD && gd->Art != ART_SPIEL, a_fmt(A0, "Spielekompressor: Daten bleiben Tool-Daten (Art %d)", gd ? gd->Art : -1));
            eintrag *pm = mit_pfad("/user/app/PLDM00001");
            pruefe(pm && pm->Kat == 6 && !pm->KatHer && pm->Art == ART_TOOL_KACHEL, a_fmt(A0, "Payload Manager (meldet sich als App) bleibt unter Apps (Bereich %d)", pm ? pm->Kat : -1));
        }
        eintrag *pl = mit_pfad("/user/data/playden-games");
        pruefe(pl && pl->Art != ART_UNBEKANNT, a_fmt(A0, "Daten unter ihrer Kachel sind nicht mehr \"Inhalt unbekannt\" (Art %d)", pl ? pl->Art : -1));
        printf("== Nicht raten\n");
        eintrag *p4 = mit_pfad("/user/data/play"); pruefe(p4 && !p4->ToolKachel, "\"play\" (zu kurz) gehört zu keiner Kachel");
        eintrag *pr = mit_pfad("/user/data/prospero"); pruefe(pr && !pr->ToolKachel, "\"prospero\" passt zu zwei Kacheln gleich gut - bleibt für sich");
        frei_stehend("/user/data/MeinOrdner", 5, "unbekannter Ordner bleibt unter Sonstiges");
        frei_stehend("/user/data/tmp", 5, "tmp bleibt unter Sonstiges");
        printf("== Sonstiges nur echter Rest\n");
        bool rest_ok = true; const char *rb = "";
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat != 5) continue;
            for (int j = 0; j < f->PfadeBasis.n; j++) if (strstr(f->PfadeBasis.e[j], "playden") || strstr(f->PfadeBasis.e[j], "CUSA31001") || strstr(f->PfadeBasis.e[j], "ps5cleanup_db_")) { rest_ok = false; rb = f->PfadeBasis.e[j]; } }
        pruefe(rest_ok, a_fmt(A0, "nichts unter Sonstiges, das zu einer Kachel gehört (%s)", rb));
        printf("== Reihenfolge: Teile direkt unter ihrer Kachel\n");
        bool reihe = true; const char *rf = "";
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (!f->ToolKachel) continue;
            int h = -1; for (int j = 0; j < i; j++) if (K->Funde.e[j] == f->ToolKachel) h = j;
            if (h < 0) { reihe = false; rf = f->TextBasis; continue; }
            for (int j = h + 1; j < i; j++) if (((eintrag *)K->Funde.e[j])->ToolKachel != f->ToolKachel) { reihe = false; rf = f->TextBasis; } }
        pruefe(reihe, a_fmt(A0, "jeder Teil steht direkt nach seiner Kachel (%s)", rf));
        printf("== Alte Sicherungen von PS5 Cleanup\n");
        eintrag *as = mit_pfad("/mnt/usb0/ps5cleanup_db_2026-11-01_233441_FW12.00_CFI-7021_B01Y_appinfo.db");
        eintrag *as2 = mit_pfad("/mnt/usb0/ps5cleanup_db_2026-11-01_233441_FW12.00_CFI-7021_B01Y_app.db");
        pruefe(as && as == as2 && as->Kat == 6 && as->Art == ART_ALTE_SICHERUNG && strstr(kern_zeile(A0, as), "PS5 Cleanup – alte Sicherungen (2 Dateien)"),
               a_fmt(A0, "ein Eintrag unter Apps (%s – %s)", as ? kern_zeile(A0, as) : "nicht gefunden", as ? kern_folge(as) : ""));
        printf("== Trophäen: mehrere Listen eines Spiels\n");
        eintrag *l1 = mit_pfad("/user/trophy/conf/NPWR31001_00"), *l2 = mit_pfad("/user/trophy/conf/NPWR31002_00"), *l9 = mit_pfad("/user/trophy2/nobackup/conf/NPWR31009_00");
        pruefe(l1 && l1->ListeNr == 1 && strstr(kern_zeile(A0, l1), "Trophäen-Liste 1"), a_fmt(A0, "erste Liste: %s", l1 ? kern_zeile(A0, l1) : "nicht gefunden"));
        pruefe(l2 && l2->ListeNr == 2 && strstr(kern_zeile(A0, l2), "Rennspiel – Trophäen-Liste 2") && l2->ToolKachel == l1, a_fmt(A0, "zweite Liste unter der ersten: %s", l2 ? kern_zeile(A0, l2) : "nicht gefunden"));
        pruefe(l2 && l2->Kat == 3 && l2->Art == ART_TR_LISTE, a_fmt(A0, "zweite Liste bleibt Trophäen-Liste unter Trophäen (Art %d)", l2 ? l2->Art : -1));
        pruefe(l9 && l9->ListeNr == 0 && strstr(kern_zeile(A0, l9), "BlackBox – Trophäen-Liste") && !strstr(kern_zeile(A0, l9), "Liste 1"), a_fmt(A0, "nur eine Liste: ohne Nummer (%s)", l9 ? kern_zeile(A0, l9) : "nicht gefunden"));
        eintrag *fo = mit_pfad("/user/home/1234abcd/trophy/data/NPWR31001_00");
        pruefe(fo && fo->ListeNr == 1 && strstr(kern_zeile(A0, fo), "(Liste 1)"), a_fmt(A0, "Fortschritt nennt die Liste: %s", fo ? kern_zeile(A0, fo) : "nicht gefunden"));
        printf("== Aufnahmen: zweite Fassung (sdr) gehört zum Spiel\n");
        eintrag *au = mit_pfad("/user/av_contents/photo/NPXS40087/CUSA31001");
        pruefe(au && au == mit_pfad("/user/av_contents/sdr/photo/NPXS40087/CUSA31001") && strstr(au->TextBasis, "1 Screenshot") && !strstr(au->TextBasis, "2 Screenshots"),
               a_fmt(A0, "ein Eintrag, nicht doppelt gezählt (%s)", au ? au->TextBasis : "nicht gefunden"));
        /* Codex-Prüfung 40 (3/4): ohne Original-Aufnahme bleibt die zweite Fassung "Weitere Galerie-Daten" (Inhalt unklar) - nicht belegt,
         * dass ihr Löschen allein keine Leiche in der Mediengalerie hinterlässt */
        eintrag *ns = mit_pfad("/user/av_contents/sdr/photo/NPXS40087/PPSA79999");
        pruefe(ns && ns->Kat == 4 && ns->Art != ART_AUFNAHMEN && strstr(ns->TextBasis, "Weitere Galerie-Daten"), a_fmt(A0, "nur sdr vorhanden: bleibt \"Weitere Galerie-Daten\" (%s)", ns ? ns->TextBasis : "nicht gefunden"));
        eintrag *sr = mit_pfad("/user/av_contents/sdr/photo/NPXS40087/CUSA31001");
        pruefe(sr && sr->Art == ART_AUFNAHMEN, "sdr mit Original: kein eigener \"Weitere Galerie-Daten\"-Eintrag");
        printf("== Codex-Prüfung 40\n");
        unter("/mnt/ext1/OrbitStore/Rennspiel - CUSA31001 - v1.00 - EU.pkg", "CUSA31001", 1, "PKG mit Spielnummer im Ordner \"OrbitStore\": zählt die Nummer, nicht der Ordner");
        frei_stehend("/user/data/Playden Dungeons", 5, "anderes Produkt mit Leerzeichen (\"Playden Dungeons\")");
        frei_stehend("/user/data/PlaydenDungeons", 5, "anderer Name ohne Trenner (\"PlaydenDungeons\")");
        printf("== Protokoll-Datei, in die gerade geschrieben wird\n");
        pruefe(!mit_pfad("/mnt/usb0/ps5cleanup_protokoll_2026-11-02_210000.txt"), "das laufende Protokoll auf dem Stick wird nicht angeboten");
        pruefe(mit_pfad("/mnt/usb0/ps5cleanup_protokoll_2026-11-01_120000.txt") != NULL, "ein altes Protokoll auf dem Stick wird weiter angeboten");
        printf("== Codex-Prüfung 41\n");
        unter("/mnt/ext1/OrbitStore/pldmgr_v0.6.3.elf", "PLDM00001", 6, "Payload eines anderen Tools im Ordner \"OrbitStore\": unter seiner eigenen Kachel");
        unter("/mnt/ext1/playden/cache-CUSA31001", "FAKE71001", 1, "App-Ordner mit Spielnummer im Namen (playden/cache-CUSA31001): bleibt bei Playden");
        unter("/user/data/lumen", "PPSA71007", 1, "Titel mit Sonderzeichen (\"Lumen™ Forge\")");
        eintrag *sd = mit_pfad("/user/av_contents/sdr/photo/NPXS40087/PPSA71002");
        pruefe(sd && sd->Art != ART_AUFNAHMEN && strstr(sd->TextBasis, "Weitere Galerie-Daten"), a_fmt(A0, "zweite Fassung mit einer Aufnahme ohne Original bleibt \"Weitere Galerie-Daten\" (%s)", sd ? sd->TextBasis : "nicht gefunden"));
        eintrag *f2 = mit_pfad("/user/home/5678efgh/trophy/data/NPWR31002_00");
        /* 03.11.2026 (da_Chris86 17:12): der Fortschritt steht unter der Trophäen-Liste seines Spiels - nie unter dem Fortschritt eines anderen Kontos */
        pruefe(f2 && f2->ToolKachel == l1, a_fmt(A0, "Fortschritt eines zweiten Kontos steht unter der Liste, nicht unter dem des ersten (%s)", f2 ? f2->TextBasis : "nicht gefunden"));
        printf("== 03.11.2026: Trophäen - Liste und Fortschritt eines Spiels untereinander\n");
        pruefe(fo && fo->ToolKachel == l1, a_fmt(A0, "Fortschritt (Liste 1) unter der ersten Liste (%s)", fo ? fo->TextBasis : "nicht gefunden"));
        eintrag *p9 = mit_pfad("/user/home/1234abcd/trophy2/nobackup/data/NPWR31009_00");
        pruefe(p9 && p9->ToolKachel == l9 && p9->Kat == 3 && p9->Art == ART_TR_FORTSCHRITT, a_fmt(A0, "nur eine Liste: Fortschritt darunter, bleibt Fortschritt (%s)", p9 ? kern_zeile(A0, p9) : "nicht gefunden"));
        { int il1 = -1, il2 = -1, ifo = -1; for (int i = 0; i < K->Funde.n; i++) { if (K->Funde.e[i] == l1) il1 = i; if (K->Funde.e[i] == l2) il2 = i; if (K->Funde.e[i] == fo) ifo = i; }
          pruefe(il1 >= 0 && il1 < il2 && il2 < ifo, a_fmt(A0, "Reihenfolge: Liste 1, Liste 2, dann Fortschritt (%d, %d, %d)", il1, il2, ifo)); }
        eintrag *oa = mit_pfad("/user/home/1234abcd/trophy2/nobackup/data/NPWR39998_00"), *ob = mit_pfad("/user/home/5678efgh/trophy2/nobackup/data/NPWR39998_00");
        pruefe(oa && ob && !oa->ToolKachel && !ob->ToolKachel, "ohne Liste: Fortschritt zweier Konten steht nicht untereinander");
        eintrag *lz = mit_pfad("/user/trophy/conf/NPWR39997_00"), *pz = mit_pfad("/user/home/1234abcd/trophy2/nobackup/data/NPWR39997_00");
        pruefe(lz && pz && pz->ToolKachel == lz, a_fmt(A0, "Liste an zwei Orten (%d Pfade): Fortschritt trotzdem darunter", lz ? lz->PfadeBasis.n : -1));
        printf("== 03.11.2026: jede Kachel bekommt, was sie selbst anlegt - allgemeine Regeln (erfundene Apps)\n");
        eintrag *lk = mit_pfad("/user/appmeta/LUMI71008");
        pruefe(lk && lk->Kat == 6 && lk->Art == ART_TOOL_KACHEL && lk->KachelId && !strcmp(lk->KachelId, "LUMI71008") && !strcmp(kern_zeile(A0, lk), "Lumina Library – Kachel"),
               a_fmt(A0, "Kachel ohne App-Ordner mit Verweis auf ein Programm: Kachel unter Apps (%s, Bereich %d)", lk ? kern_zeile(A0, lk) : "nicht gefunden", lk ? lk->Kat : -1));
        unter("/user/data/LuminaLibrary", "LUMI71008", 6, "... ihre Einstellungen darunter");
        unter("/mnt/usb0/LuminaLibrary_2.0.elf", "LUMI71008", 6, "... ihre ELF darunter");
        eintrag *rk = mit_pfad("/user/appmeta/REST71016");
        pruefe(rk && rk->Kat == 5 && strstr(rk->TextBasis, "Reste: "), "Kachel-Infos ohne App-Ordner und ohne Verweis bleiben \"Reste\"");
        unter("/mnt/usb0/SpiegelMountPlus_2.0-beta1.elf", "SPMP71009", 6, "\"+\" im Namen = \"Plus\" (SpiegelMountPlus unter \"Spiegel Mount+\")");
        unter("/user/data/spiegelmount", "SPMP71009", 6, "Daten-Ordner ohne \"+\" auch");
        unter("/mnt/usb0/ps5-kiste-manager_v1.0.elf", "KIST71010", 6, "\"ps5-\" vorne zählt nicht (ps5-kiste-manager unter \"Kiste Manager\")");
        unter("/user/data/ablage_startup.log", "ABLG71011", 6, "Datei neben dem Daten-Ordner (ablage_startup.log neben ablage)");
        eintrag *al = mit_pfad("/user/data/ablage_startup.log");
        pruefe(al && !strcmp(kern_zeile(A0, al), "ablage_startup.log"), a_fmt(A0, "Einzeldatei heißt, wie sie heißt (%s)", al ? kern_zeile(A0, al) : ""));
        unter("/user/data/sternladen", "STRN71012", 6, "zwei Kacheln mit gleichem Namen: Daten unter der Tool-Kachel");
        unter("/mnt/ext1/.stern-drive", "STRN71012", 6, "Punkt-Datei auf der M.2: Beleg im Programm (stern.elf)");
        eintrag *sd2 = mit_pfad("/mnt/ext1/.stern-drive");
        pruefe(sd2 && sd2->Art == ART_TOOL_DATEN && strstr(kern_zeile(A0, sd2), ".stern-drive"), a_fmt(A0, "... Markierung des Tools, kein \"neu herunterladen\" (%s – %s)", sd2 ? kern_zeile(A0, sd2) : "", sd2 ? kern_folge(sd2) : ""));
        unter("/user/data/EMU9DATA", "PPSA71014", 1, "Beleg im Programm: /data/EMU9DATA in eboot.bin und helper.elf von \"Emu Neun\" (Disc-Kachel nur eboot.bin)");
        unter("/user/data/emu9.lock", "PPSA71014", 1, "Datei aus dem Programm (/data/emu9.lock)");
        eintrag *em = mit_pfad("/user/data/EMU9DATA");
        pruefe(em && em->Art == ART_TOOL_DATEN_WICHTIG && em->Rot && em->WichtigFund && strstr(em->WichtigFund, "Mcd001.ps2"),
               a_fmt(A0, "Speicherkarte darin: rot, vermutlich eigene Spielstände (%s)", em ? kern_zeile(A0, em) : ""));
        frei_stehend("/user/data/ZWEIDATA", 5, "Gleichstand (zwei Kacheln nennen den Pfad gleich oft): nicht raten");
        eintrag *ga = mit_pfad("/user/data/garlic");
        pruefe(ga && !ga->ToolKachel, "Daten-Ordner eines bekannten Tools ohne Kachel (garlic) kommt nicht unter eine fremde Kachel");
        printf("== Codex-Prüfung 56\n");
        frei_stehend("/user/data/LINKSDATA", 5, "Pfad nur als /mnt/usb0/data/LINKSDATA im Programm: kein Beleg für /user/data/LINKSDATA (linke Grenze)");
        unter("/user/data/emu9.sav", "PPSA71014", 1, "einzelne Spielstand-Datei aus dem Programm");
        eintrag *sv = mit_pfad("/user/data/emu9.sav");
        pruefe(sv && sv->Art == ART_TOOL_DATEN_WICHTIG && sv->Rot, a_fmt(A0, "... ist rot, nur einzeln (%s)", sv ? kern_zeile(A0, sv) : ""));
        unter("/user/data/SpiegelMount+-cache", "SPMP71009", 6, "\"+\" auch beim Namensanfang (SpiegelMount+-cache unter \"Spiegel Mount+\")");
        eintrag *ds = mit_pfad("/user/data/dateiserver");
        pruefe(ds && !ds->ToolKachel, "zwei verschiedene Apps \"Datei Server\" im selben Bereich: nicht raten");
        frei_stehend("/user/data/SYMDATA", 5, "Programm hinter einer Verknüpfung (kiste-manager -> kisteziel) zählt nicht");
        unter("/user/data/GRENZDATA", "PPSA71019", 1, "Codex-Prüfung 59: Weg über der Grenze zweier Lese-Stücke wird gefunden");
        printf("== 03.11.2026: Satz vor dem Löschen\n");
        { eintrag *eh = NULL; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; f->Wahl = (f == em); if (f == em) eh = f; }
          const char *satz = kern_endgueltig_satz(K, A0);
          pruefe(eh && strstr(satz, "vermutlich eigene Spiele, Spielstände oder Sicherungen (EMU9DATA)") && !strstr(satz, "deine vermutlich"), a_fmt(A0, "%s", satz));
          for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false; }
        printf("== 1.0.8 (Arbeitsplan 2.3): Tools ohne Kachel - Payload-Datei unter dem Daten-Ordner\n");
        {
            struct { const char *elf, *ordner, *name; } F[] = {
                { "/mnt/usb0/nanoDNS_0.4.elf", "/user/data/nanodns", "nanoDNS" },
                { "/mnt/usb0/zftpd-ps5-v1.6.0.elf", "/user/data/zftpd", "zftpd" },
                { "/mnt/usb0/garlic-savemgr_v1.14.elf", "/user/data/garlic", "garlic" },
                { "/mnt/usb0/Werkbank_v2.elf", "/user/data/Werkbank", "Werkbank" },
                { "/mnt/usb0/ps5-kompass_v1.0.elf", "/user/data/kompass", "kompass" } };
            for (size_t i = 0; i < sizeof F / sizeof F[0]; i++) {
                eintrag *e = mit_pfad(F[i].elf), *o = mit_pfad(F[i].ordner);
                kblock *bb = NULL; { kblock *b; int nb = kern_bloecke(K, A0, 6, SORT_GROESSTE, &b); for (int x = 0; x < nb; x++) if (o && b[x].kopf == o) bb = &b[x]; }
                pruefe(e && o && e->Begleit == o && !e->ToolKachel && !o->ToolKachel && e->Kat == 6 && o->Kat == 6 && bb && bb->ohne_kachel && bb->teile.n == 2 && bb->teile.e[1] == e && !strcmp(bb->name, F[i].name),
                       a_fmt(A0, "%s unter %s: Block \"%s\", ohne Kachel, %d Zeilen (Bereich %d und %d)", F[i].elf, F[i].ordner, bb ? bb->name : "-", bb ? bb->teile.n : 0, e ? e->Kat : -1, o ? o->Kat : -1));
            }
            eintrag *wb = mit_pfad("/user/data/Werkbank"), *we = mit_pfad("/mnt/usb0/Werkbank_v2.elf");
            pruefe(wb && wb->KatHer == 5 && wb->Art != ART_UNBEKANNT && we && we->Kat == 6,
                   a_fmt(A0, "unbekannter Ordner mit passender Payload-Datei: beide unter Apps, nicht mehr \"Inhalt unbekannt\" (%s; Ordner vorher Bereich %d, Datei vorher %d, Art %d)",
                         wb ? kern_zeile(A0, wb) : "-", wb ? wb->KatHer : -1, we ? we->KatHer : -1, wb ? wb->Art : -1));
            eintrag *ab = mit_pfad("/mnt/usb0/alpha-beta-tool_v1.elf"), *ac = mit_pfad("/mnt/usb0/abc.elf"), *oa1 = mit_pfad("/user/data/alpha"), *oa3 = mit_pfad("/user/data/abc");
            pruefe(ab && !ab->Begleit && !ab->KatHer && oa1 && !oa1->OhneKachel && !oa1->KatHer,
                   a_fmt(A0, "Datei beginnt mit zwei unbekannten Ordnernamen (alpha, alphabeta): nicht raten, nichts verschoben (Datei Bereich %d, Ordner Bereich %d)", ab ? ab->Kat : -1, oa1 ? oa1->Kat : -1));
            /* Codex-Prüfung 73: nie umgekehrt (Ordner länger als die Datei), und ein unbekannter Ordner nur bei gleichem Namen */
            eintrag *wa = mit_pfad("/user/data/Werkbank-Archiv"), *sw = mit_pfad("/user/data/Spielwiese"), *se = mit_pfad("/mnt/usb0/Spielwiese-ps5_v1.elf");
            pruefe(wa && !wa->OhneKachel && !wa->KatHer && wa->Kat == 5, a_fmt(A0, "Werkbank-Archiv bleibt unter Sonstiges, gehört nicht zu Werkbank_v2.elf (Bereich %d)", wa ? wa->Kat : -1));
            pruefe(sw && se && !se->Begleit && !sw->OhneKachel && sw->Kat == 5, "unbekannter Ordner, Datei beginnt nur mit seinem Namen (Spielwiese-ps5_v1.elf): nicht zugeordnet");
            pruefe(ac && !ac->Begleit && !ac->KatHer && oa3 && !oa3->OhneKachel && !oa3->KatHer,
                   a_fmt(A0, "zu kurzer Ordnername (abc): nicht raten (Datei Bereich %d, Ordner Bereich %d)", ac ? ac->Kat : -1, oa3 ? oa3->Kat : -1));
            eintrag *nd = mit_pfad("/user/data/nanodns"), *ne = mit_pfad("/mnt/usb0/nanoDNS_0.4.elf");
            pruefe(nd && ne && kern_kopf_von(K, ne, 6) == nd && kern_kopf_von(K, nd, 6) == NULL, "kern_kopf_von: Payload-Datei steht unter dem Daten-Ordner");
            pruefe(ne && strstr(kern_zeile(A0, ne), "nanoDNS_0.4.elf") && !strstr(kern_zeile(A0, ne), "   – "), a_fmt(A0, "Zeile der Payload-Datei: %s", ne ? kern_zeile(A0, ne) : "-"));
        }
        printf("== 1.0.8 (Arbeitsplan 2.4): doppelte Spiele\n");
        {
            eintrag *rs = mit_pfad_anfang("/user/app/CUSA31001"), *ra = mit_pfad("/mnt/usb0/CUSA31001.exfat");
            eintrag *z1 = mit_pfad("/mnt/usb0/Zweimal - PPSA31777.exfat"), *z2 = mit_pfad("/mnt/ext1/Zweimal - PPSA31777.exfat");
            pruefe(rs && ra && rs->Doppelt.n == 1 && rs->Doppelt.e[0] == ra && strstr(kern_zeile(A0, rs), " – doppelt: auch als Spieldatei auf dem USB-Laufwerk"),
                   a_fmt(A0, "installiertes Spiel mit Spieldatei auf dem Stick: %s", rs ? kern_zeile(A0, rs) : "-"));
            pruefe(ra && ra->Art == ART_ABBILD && strstr(kern_zeile(A0, ra), " – doppelt: auch installiert auf der internen SSD"),
                   a_fmt(A0, "die Spieldatei sagt, dass das Spiel installiert ist: %s (Art %d)", ra ? kern_zeile(A0, ra) : "-", ra ? ra->Art : -1));
            pruefe(z1 && z2 && z1->Doppelt.n == 1 && z2->Doppelt.n == 1 && strstr(kern_zeile(A0, z1), "auch als Spieldatei auf der M.2-SSD") && strstr(kern_zeile(A0, z2), "auch als Spieldatei auf dem USB-Laufwerk"),
                   a_fmt(A0, "zwei Spieldateien desselben Spiels: %s | %s", z1 ? kern_zeile(A0, z1) : "-", z2 ? kern_zeile(A0, z2) : "-"));
            eintrag *pk2 = mit_pfad("/mnt/usb0/Rennspiel - CUSA31001 - v1.00 - EU.pkg"), *pk3 = mit_pfad("/mnt/ext1/OrbitStore/Rennspiel - CUSA31001 - v1.00 - EU.pkg");
            bool pk_kopie = false; for (int i = 0; rs && i < rs->Doppelt.n; i++) if (((eintrag *)rs->Doppelt.e[i])->PkgArt) pk_kopie = true;
            pruefe(rs && !pk_kopie, "Installationsdatei (PKG) zählt nicht als Kopie des installierten Spiels");
            /* 1.0.8 (Befund 12): dieselbe PKG-Datei (gleicher Name, gleiche Größe) auf USB-Laufwerk und M.2 - beide Zeilen sagen "doppelt" */
            pruefe(pk2 && pk3 && pk2->Doppelt.n == 1 && pk2->Doppelt.e[0] == pk3 && pk3->Doppelt.n == 1 && strstr(kern_zeile(A0, pk2), "Rennspiel – Installationsdatei – doppelt: auch auf der M.2-SSD")
                   && strstr(kern_zeile(A0, pk3), "doppelt: auch auf dem USB-Laufwerk"),
                   a_fmt(A0, "dieselbe PKG-Datei auf zwei Speichern: %s | %s", pk2 ? kern_zeile(A0, pk2) : "-", pk3 ? kern_zeile(A0, pk3) : "-"));
            /* dazu aus dem Nachbau (ref105 download_v1): "Testspiel Fünf" ist intern UND auf der M.2 installiert - auch doppelt */
            int nd = 0, n5 = 0; const char *wel = "";
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (!f->Doppelt.n) continue; nd++;
                if (strstr(kern_zeile(A0, f), "Testspiel Fünf – Spiel – doppelt: auch installiert auf der ")) n5++;
                else if (f != rs && f != ra && f != z1 && f != z2 && f != pk2 && f != pk3) wel = a_fmt(A0, "%s; %s", wel, kern_zeile(A0, f)); }
            pruefe(nd == 8 && n5 == 2 && !wel[0], a_fmt(A0, "genau diese 4 Zeilen, die beiden PKG-Dateien und die beiden Kopien von \"Testspiel Fünf\" (intern und M.2) als doppelt markiert (%d, %d%s)", nd, n5, wel));
        }
        printf("== 1.0.8 (Arbeitsplan 2.6): Blöcke je Kachel, Gesamtgröße, größte zuerst und A–Z\n");
        {
            kern_groessen_zuruecksetzen(K);
            bool einmal = true, kopf_vorn = true, fundreihe = true; const char *wo = "";
            for (int ki = 0; ki < KAT_ANZ; ki++) {
                int kat = ALLE_KAT[ki]; kblock *b; int nb = kern_bloecke(K, A0, kat, SORT_GROESSTE, &b);
                for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat != kat) continue;
                    int c = 0; for (int x = 0; x < nb; x++) for (int j = 0; j < b[x].teile.n; j++) if (b[x].teile.e[j] == f) c++;
                    if (c != 1) { einmal = false; wo = f->TextBasis; } }
                int vor = -1;
                for (int x = 0; x < nb; x++) {
                    if (b[x].teile.n < 1 || b[x].teile.e[0] != b[x].kopf || kern_kopf_von(K, b[x].kopf, kat)) { kopf_vorn = false; wo = b[x].kopf ? b[x].kopf->TextBasis : "?"; }
                    int ix = -1; for (int i = 0; i < K->Funde.n; i++) if (K->Funde.e[i] == b[x].kopf) ix = i;
                    if (x > 0 && b[x - 1].gruppe != b[x].gruppe) { if (b[x - 1].gruppe > b[x].gruppe) fundreihe = false; vor = -1; }   /* 1.0.10: je Gruppe */
                    if (ix < vor || b[x].groesse_da) fundreihe = false;
                    vor = ix;
                }
            }
            pruefe(einmal, a_fmt(A0, "jede Zeile steht in genau einem Block (%s)", wo));
            pruefe(kopf_vorn, a_fmt(A0, "jeder Block beginnt mit seinem Kopf, der selbst unter keinem anderen steht (%s)", wo));
            pruefe(fundreihe, "solange nicht alles gezählt ist: Blöcke in Fundreihenfolge und ohne Größe (die Liste springt nur einmal)");
            eintrag *ob = mit_pfad("/user/data/orbit-store"), *oe = mit_pfad("/mnt/usb0/orbit_store_v1.2.elf");
            kblock *bo = NULL; { kblock *b; int nb = kern_bloecke(K, A0, 6, SORT_GROESSTE, &b); for (int x = 0; x < nb; x++) if (ob && b[x].kopf == ob->ToolKachel) bo = &b[x]; }
            bool hat_o = false, hat_e = false; if (bo) for (int j = 1; j < bo->teile.n; j++) { if (bo->teile.e[j] == ob) hat_o = true; if (bo->teile.e[j] == oe) hat_e = true; }
            pruefe(bo && hat_o && hat_e && !strcmp(bo->name, "Orbit Store"), a_fmt(A0, "Block \"%s\": Kachel, Daten-Ordner und ELF zusammen (%d Zeilen)", bo ? bo->name : "-", bo ? bo->teile.n : 0));
            eintrag *t1 = mit_pfad("/user/trophy/conf/NPWR31001_00"), *t2 = mit_pfad("/user/trophy/conf/NPWR31002_00"), *tf = mit_pfad("/user/home/1234abcd/trophy/data/NPWR31001_00");
            kblock *bt = NULL; { kblock *b; int nb = kern_bloecke(K, A0, 3, SORT_GROESSTE, &b); for (int x = 0; x < nb; x++) if (b[x].kopf == t1) bt = &b[x]; }
            int i2 = -1, ifo = -1; if (bt) for (int j = 0; j < bt->teile.n; j++) { if (bt->teile.e[j] == t2) i2 = j; if (bt->teile.e[j] == tf) ifo = j; }
            pruefe(bt && i2 == 1 && ifo > i2 && !strcmp(bt->name, "Rennspiel"), a_fmt(A0, "Trophäen \"%s\": Liste 1 oben, dann Liste 2, dann der Fortschritt (%d, %d)", bt ? bt->name : "-", i2, ifo));
            /* jetzt alles zählen (wie im Hintergrund der App) */
            kern_groessen_im_hintergrund(K, nie_stopp, NULL, nichts_tun, NULL);
            bool summe = true, groesste = true, teile_ok = true, az = true, gleich_viele = true; const char *wo2 = "";
            for (int ki = 0; ki < KAT_ANZ; ki++) {
                int kat = ALLE_KAT[ki]; kblock *b, *z; int nb = kern_bloecke(K, A0, kat, SORT_GROESSTE, &b), nz = kern_bloecke(K, A0, kat, SORT_AZ, &z);
                if (nb != nz) gleich_viele = false;
                for (int x = 0; x < nb; x++) {
                    long long su = 0; bool da = true;
                    for (int j = 0; j < b[x].teile.n; j++) { groesse *g = kern_eintrag_groesse_in(K, A0, b[x].teile.e[j], true); if (!g) da = false; else su += g->Bytes; }
                    if (!da || !b[x].groesse_da || su != b[x].bytes) { summe = false; wo2 = b[x].name; }
                    if (x > 0 && (b[x - 1].gruppe > b[x].gruppe || (b[x - 1].gruppe == b[x].gruppe && b[x - 1].bytes < b[x].bytes))) { groesste = false; wo2 = b[x].name; }
                    for (int j = 2; j < b[x].teile.n; j++) {
                        eintrag *v = b[x].teile.e[j - 1], *w = b[x].teile.e[j];
                        bool lv = v->Art == ART_TR_LISTE || v->Art == ART_TR_LISTE_REST, lw = w->Art == ART_TR_LISTE || w->Art == ART_TR_LISTE_REST;
                        groesse *gv = kern_eintrag_groesse_in(K, A0, v, true), *gw = kern_eintrag_groesse_in(K, A0, w, true);
                        if ((!lv && lw) || (lv == lw && gv && gw && gv->Bytes < gw->Bytes)) { teile_ok = false; wo2 = b[x].name; }
                    }
                }
                for (int x = 1; x < nz; x++) if (z[x - 1].gruppe > z[x].gruppe || (z[x - 1].gruppe == z[x].gruppe && kern_vergleich_namen(z[x - 1].name, z[x].name) > 0)) { az = false; wo2 = z[x].name; }
            }
            pruefe(summe, a_fmt(A0, "Gesamtgröße je Block = Summe seiner Zeilen (%s)", wo2));
            pruefe(groesste, a_fmt(A0, "größte zuerst nach der Größe des ganzen Blocks (%s)", wo2));
            pruefe(teile_ok, a_fmt(A0, "im Block: weitere Trophäen-Listen zuerst, sonst größte zuerst (%s)", wo2));
            pruefe(az, a_fmt(A0, "A–Z: jeder Bereich nach Namen (%s)", wo2));
            pruefe(gleich_viele, "A–Z und größte zuerst zeigen dieselben Blöcke");
            {
                kblock *b, *z; int nb = kern_bloecke(K, A0, 6, SORT_GROESSTE, &b), nz = kern_bloecke(K, A0, 6, SORT_AZ, &z);
                long long ko = -1, kmax = -1;
                for (int x = 0; x < nb; x++) { groesse *g = kern_eintrag_groesse_in(K, A0, b[x].kopf, true); long long v = g ? g->Bytes : -1;
                    if (!strcmp(b[x].name, "Orbit Store")) ko = v; else if (v > kmax) kmax = v; }
                pruefe(nb > 0 && !strcmp(b[0].name, "Orbit Store") && kmax > ko && ko >= 0,
                       a_fmt(A0, "oben steht \"%s\": die Kachel allein ist kleiner als eine andere (%lld zu %lld Byte), mit ihren Daten ist sie die größte", nb > 0 ? b[0].name : "-", ko, kmax));
                int pa = -1, po = -1, pp = -1;
                for (int x = 0; x < nz; x++) { if (!strcmp(z[x].name, "Ablage")) pa = x; if (!strcmp(z[x].name, "Orbit Store")) po = x; if (!strcmp(z[x].name, "Payload Manager")) pp = x; }
                pruefe(pa >= 0 && pa < po && po < pp, a_fmt(A0, "A–Z: Ablage (%d) vor Orbit Store (%d) vor Payload Manager (%d)", pa, po, pp));
                printf("   Apps, größte zuerst:"); for (int x = 0; x < nb; x++) printf(" %s (%lld)%s", b[x].name, b[x].bytes, x + 1 < nb ? "," : "\n");
                printf("   Apps, A–Z:"); for (int x = 0; x < nz; x++) printf(" %s%s", z[x].name, x + 1 < nz ? "," : "\n");
            }
            eintrag *lu = mit_pfad("/user/appmeta/LUMI71008"), *sp = mit_pfad_anfang("/user/app/CUSA31001");
            pruefe(lu && !strcmp(kern_name(A0, lu), "Lumina Library"), a_fmt(A0, "Name ohne \"– Kachel\": %s", lu ? kern_name(A0, lu) : "-"));
            pruefe(sp && !strcmp(kern_name(A0, sp), "Rennspiel"), a_fmt(A0, "Name eines Spiels: %s (Zeile: %s)", sp ? kern_name(A0, sp) : "-", sp ? kern_zeile(A0, sp) : "-"));
            pruefe(kern_vergleich_namen("Ägypten", "Apfel") < 0 && kern_vergleich_namen("apfel", "Birne") < 0 && kern_vergleich_namen("Zeta", "alpha") > 0
                   && kern_vergleich_namen("Ölfass", "Orange") < 0 && kern_vergleich_namen("Élan", "Ende") < 0, "A–Z: Groß/klein egal, Ä wie A, Akzente wie der Grundbuchstabe");
            pruefe(kern_vergleich_namen("Teil 2", "Teil 10") < 0 && kern_vergleich_namen("Teil 007", "Teil 10") < 0 && kern_vergleich_namen("Teil 10", "Teil 9") > 0, "A–Z: Zahlen nach Wert");
            pruefe(kern_vergleich_namen("Straße A", "Strasse B") < 0 && kern_vergleich_namen("Strasse A", "Straße B") < 0, "A–Z: ß wie ss");
            pruefe(kern_vergleich_namen("'Splosion Man", "Rayman") > 0 && kern_vergleich_namen("[Beta] Zoo", "Apfel") > 0, "A–Z: Satzzeichen ganz vorne zählen nicht");
            pruefe(kern_vergleich_namen("Mortal Kombat™ 1", "Mortal Kombat 2") < 0 && kern_vergleich_namen("Assassin’s Creed A", "Assassin's Creed B") < 0, "A–Z: ™ zählt nicht, ’ wie '");
            pruefe(kern_vergleich_namen("Rennspiel", "Rennspiel") == 0 && kern_vergleich_namen("Zoo", "Apfel") == -kern_vergleich_namen("Apfel", "Zoo")
                   && kern_vergleich_namen("Straße", "Strasse") == -kern_vergleich_namen("Strasse", "Straße") && kern_vergleich_namen("Straße", "Strasse") != 0, "A–Z: gleiche Namen 0, sonst immer dieselbe Reihenfolge");
            pruefe(kern_vergleich_namen(NULL, "a") < 0 && kern_vergleich_namen("\xC3", "a") > 0 && kern_vergleich_namen("a\xE2\x80", "a") > 0, "A–Z: leer und kaputtes UTF-8 ohne Absturz");
            {   /* Codex-Prüfung 73: eine echte Ordnung - antisymmetrisch und transitiv, auch mit ®, Zahlen, ß, Œ und führenden Nullen */
                const char *W[] = { "A®2", "A3", "A10", "A2", "A02", "a2", "A®10", "A™3", "Straße", "Strasse", "Strasse2", "Straße10", "Ä1", "A1", "'A1", "™A1",
                                    "a 1", "a1b", "a10b", "ß", "ss", "s", "Œuvre", "Oeuvre", "Ouvre", "", "1", "01", "001", "10", "x®", "x", "Zoo", "zoo", "ä", "a" };
                int nw = (int)(sizeof W / sizeof *W), bad = 0; const char *wo3 = "";
                for (int i = 0; i < nw; i++) for (int j = 0; j < nw; j++) {
                    int c1 = kern_vergleich_namen(W[i], W[j]), c2 = kern_vergleich_namen(W[j], W[i]);
                    if ((c1 > 0) - (c1 < 0) != -((c2 > 0) - (c2 < 0)) || (i == j && c1 != 0)) { bad++; wo3 = a_fmt(A0, "%s / %s", W[i], W[j]); }
                    for (int l = 0; l < nw; l++) if (c1 < 0 && kern_vergleich_namen(W[j], W[l]) < 0 && kern_vergleich_namen(W[i], W[l]) >= 0) { bad++; wo3 = a_fmt(A0, "%s < %s < %s", W[i], W[j], W[l]); }
                }
                pruefe(bad == 0 && kern_vergleich_namen("A®2", "A10") < 0, a_fmt(A0, "A–Z ist eine echte Ordnung (%d Widersprüche%s%s)", bad, bad ? ", z. B. " : "", wo3));
            }
        }
    } else {
        printf("== Selbstschutz: nicht lesbare Einzeldatei\n");
        pruefe(K->SCHUTZ.aktiv && !K->SCHUTZ.unsicher, a_fmt(A0, "Selbstschutz an und sicher (aktiv %d, unsicher %d)", K->SCHUTZ.aktiv, K->SCHUTZ.unsicher));
        eintrag *pe = mit_pfad_anfang("/user/data/prosperoeden");
        pruefe(pe && !strstr(pe->TextBasis, "PS5 Cleanup"), a_fmt(A0, "prosperoeden ohne \"(alles außer PS5 Cleanup)\" (%s)", pe ? pe->TextBasis : "nicht gefunden"));
        bool gelistet = false; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (strstr(f->PfadeBasis.e[j], "1570.bin")) gelistet = true; }
        /* NEU 1.0.10 (PS5-Test 12.11.2026, Playden): eine nicht lesbare Datei sperrt nicht mehr - sie gehört zum Ordner wie jede andere */
        bool gesperrt = false; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (strstr(f->PfadeBasis.e[j], "/user/data/prosperoeden") && f->Gesperrt) gesperrt = true; }
        pruefe(!gesperrt && !kern_selbstschutz_unlesbar(K, "/user/data/prosperoeden/user/cache/native-opengl/1570.bin") && kern_selbstschutz_anbieten(K, "/user/data/prosperoeden/user/cache/native-opengl/1570.bin"),
               a_fmt(A0, "die nicht lesbare Datei sperrt nichts mehr (1.0.10, Playden) - gelistet %s", gelistet ? "ja" : "über ihren Ordner"));
        pruefe(mit_pfad("/user/data/prosperoeden/user/cache/native-opengl/lesbar.bin") || mit_pfad_anfang("/user/data/prosperoeden/user") || (pe && !pe->Gesperrt),
               "der lesbare Rest wird angeboten (seit 1.0.10 meist der ganze Ordner, frei)");
        printf("== 03.11.2026: alte Protokolle auf der Konsole (vollständige Bereinigung)\n");
        const char *PR = "/user/data/PS5-Cleanup-Protokolle";
        K->PROTOKOLL_JETZT = "/data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_2026-11-03_174254.txt";   /* Codex-Prüfung 65: so, wie die App ihn setzt */
        bool angeboten = false; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (strstr(f->PfadeBasis.e[j], "PS5-Cleanup-Protokolle")) angeboten = true; }
        pruefe(!angeboten, "Protokolle stehen in keinem Eintrag");
        pruefe(kern_alte_protokolle(K) == 2, a_fmt(A0, "zwei alte Protokolle (ohne das laufende, ohne Verknüpfung, ohne fremde Datei): %d", kern_alte_protokolle(K)));
        int fe = kern_alte_protokolle_weg(K);
        const char *W2 = a_fmt(A0, "%s/w", argv[1]);
        #define DA(x) (access(a_fmt(A0, "%s%s/%s", W2, PR, x), F_OK) == 0 || faccessat(AT_FDCWD, a_fmt(A0, "%s%s/%s", W2, PR, x), F_OK, AT_SYMLINK_NOFOLLOW) == 0)
        pruefe(fe == 0 && !DA("ps5cleanup_protokoll_2026-11-03_063508.txt") && !DA("ps5cleanup_protokoll_2026-11-03_145321.txt"), a_fmt(A0, "alte Protokolle gelöscht (Fehler %d)", fe));
        pruefe(DA("ps5cleanup_protokoll_2026-11-03_174254.txt"), "das laufende Protokoll bleibt");
        pruefe(DA("andere.txt") && DA("ps5cleanup_protokoll_2026-11-03_000000.txt"), "fremde Datei und Verknüpfung bleiben");
        pruefe(!K->PROTOKOLLE_FREI, "Freigabe danach wieder zu");
        #undef DA
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
