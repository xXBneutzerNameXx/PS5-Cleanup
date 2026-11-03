/* PS5 Cleanup 1.0.7 - Test "ein Block pro Kachel" (da_Chris86, PS5-Test 02.11.2026, finaler Plan 1.0.7 Punkte B1-B6)
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
        frei_stehend("/mnt/usb0/Anderes Spiel - CUSA39999 - v1.00 - EU.pkg", 5, "PKG eines nicht installierten Spiels bleibt unter Sonstiges");
        unter("/user/data/homebrew/FAKE71001", "FAKE71001", 1, "homebrew/<Kennung> unter ihrer Kachel");
        unter("/user/data/homebrew/PPSA71003", "PPSA71003", 1, "homebrew/<Kennung> unter ihrer Kachel");
        printf("== Ein Block pro Kachel: über den Namen der App\n");
        unter("/user/data/playden", "FAKE71001", 1, "playden (App unter Spiele, wie die PS5 sie zeigt)");
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
    } else {
        printf("== Selbstschutz: nicht lesbare Einzeldatei\n");
        pruefe(K->SCHUTZ.aktiv && !K->SCHUTZ.unsicher, a_fmt(A0, "Selbstschutz an und sicher (aktiv %d, unsicher %d)", K->SCHUTZ.aktiv, K->SCHUTZ.unsicher));
        eintrag *pe = mit_pfad_anfang("/user/data/prosperoeden");
        pruefe(pe && !strstr(pe->TextBasis, "PS5 Cleanup"), a_fmt(A0, "prosperoeden ohne \"(alles außer PS5 Cleanup)\" (%s)", pe ? pe->TextBasis : "nicht gefunden"));
        bool gelistet = false; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (strstr(f->PfadeBasis.e[j], "1570.bin")) gelistet = true; }
        pruefe(!gelistet, "die nicht lesbare Datei steht in keinem Eintrag (bleibt still stehen)");
        pruefe(mit_pfad("/user/data/prosperoeden/user/cache/native-opengl/lesbar.bin") || mit_pfad_anfang("/user/data/prosperoeden/user"), "der lesbare Rest wird angeboten");
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
