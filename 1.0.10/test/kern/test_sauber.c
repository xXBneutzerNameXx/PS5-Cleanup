/* PS5 Cleanup - Test "sauber löschen" (da_Chris86 01.11.2026 18:27 "wir machen alles danach testlauf"):
 *  - Bilder/Videos nur über Sonys Löschdienst ("SITE MEDIUM"), danach räumt der Ablauf die Reste (Vorschaubild ohne Aufnahme) auf
 *  - Spielstände nur samt Eintrag in der Spielstand-Liste ("SITE SPIELSTAND"): erst austragen, dann die Dateien - lehnt die Liste
 *    ab oder läuft das Spiel, bleibt alles
 *  - ohne diese Dienste (PC, Windows) bleiben Aufnahmen und Spielstände unverändert (keine leeren Einträge)
 *  - Reste ohne Kachel (Sony führt die Kennung nicht, app.db auch nicht -> 226): die Dateien werden direkt gelöscht
 *  - Simulation: nur "auf dem Papier"
 *   test_sauber <hier> <fall>   fall: ok | ohne | ablehnen | belegt | teil | sim   (Umgebung setzt lauf_sauber.sh) */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static arena *A0; static kern *K; static sliste LOG;
static void melde_log(void *c, const char *t) { (void)c; sl_add(A0, &LOG, t); printf("LOG: %s\n", t); }
static bool nein(void *c) { (void)c; return false; }
static const char *H;
static bool da_w(const char *p) { struct stat st; return lstat(VERB(A0, H, "/w", p), &st) == 0; }
static int log_zeile(const char *a, const char *b) { for (int i = 0; i < LOG.n; i++) if (strstr(LOG.e[i], a) && (!b || strstr(LOG.e[i], b))) return i; return -1; }
static int buch_zeile(const sliste *bu, const char *a) { for (int i = 0; i < bu->n; i++) if (strstr(bu->e[i], a)) return i; return -1; }
static eintrag *mit_pfad(const char *p, int art)
{
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (art >= 0 && f->Art != art) continue; for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], p)) return f; }
    return NULL;
}

#define U "/user/home/1a2b3c4d"
#define FOTO "/user/av_contents/photo/NPXS40087/PPSA03530/318/20261101_174354_00830999"
#define VIDEO "/user/av_contents/video/NPXS40087/PPSA03530/0bd/20261101_174353_00523513"
#define VS_FOTO "/user/av_contents/thumbnails/photo/NPXS40087/PPSA03530/318/20261101_174354_00830999.jxr.jxr"
#define VS_VIDEO "/user/av_contents/thumbnails/video/NPXS40087/PPSA03530/0bd/20261101_174353_00523513.webm.jxr"
#define VS_REST "/user/av_contents/thumbnails/photo/NPXS40087/PPSA03530/318/alt_ohne_aufnahme.jxr.jxr"

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: test_sauber <hier> <fall>\n"); return 2; }
    H = argv[1]; const char *fall = argv[2];
    A0 = arena_neu();
    printf("== Sauber löschen: %s\n", fall);
    datei_optionen o = { .wurzel = VERB(A0, H, "/w"), .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    zugriff *z = zugriff_datei(&o);
    bool sim = !strcmp(fall, "sim");
    if (sim) z = zugriff_sim(z);
    K = kern_neu(z); K->melde = melde_log; K->simulation = sim;
    volatile int fe = 0;
    VERSUCHE { kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K); kern_suche_funde(K); }
    FANGE { printf("FAIL Suche: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) { printf("FEHLERZAHL 1\n"); return 1; }

    eintrag *aufn = mit_pfad("/user/av_contents/photo/NPXS40087/PPSA03530", ART_AUFNAHMEN);
    eintrag *sp5 = mit_pfad(U "/savedata_prospero/PPSA03530", ART_SPIELSTAND);
    eintrag *sp4 = mit_pfad(U "/savedata/CUSA09172", ART_SPIELSTAND);
    eintrag *jbsp = mit_pfad(U "/savedata/CUSA16074", -1);   /* Spielstand eines Lua-Jailbreak-Spiels - eigener Eintrag (Jailbreak) */
    eintrag *rest = NULL;
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (strstr(f->Text, "PPSA68888") && f->KachelId && !f->Gesperrt) rest = f; }
    printf("INFO Aufnahmen=%s\nINFO Spielstand PS5=%s\nINFO Spielstand PS4=%s\nINFO Reste=%s\n", aufn ? aufn->Text : "-", sp5 ? sp5->Text : "-", sp4 ? sp4->Text : "-", rest ? rest->Text : "-");
    pruefe(aufn != NULL, "Aufnahmen von PPSA03530 gefunden (Art Aufnahmen)");
    pruefe(sp5 && sp5->Pfade.n == 2, "PS5-Spielstand PPSA03530 gefunden, mit Bild/Beschreibung (_meta) - 2 Pfade");
    pruefe(sp4 != NULL, "PS4-Spielstand CUSA09172 gefunden");
    pruefe(rest && rest->KachelOrdner && !strcmp(rest->KachelOrdner, "/user/app/PPSA68888"), "App PPSA68888 hat einen App-Ordner für Sony");
    printf("INFO Jailbreak-Spielstand=%s (Art %d, Kat %d)\n", jbsp ? jbsp->Text : "-", jbsp ? jbsp->Art : -1, jbsp ? jbsp->Kat : -1);
    pruefe(jbsp && jbsp->Art != ART_SPIELSTAND, "Spielstand eines Lua-Jailbreak-Spiels ist ein anderer Eintrag (nicht Art Spielstand)");
    if (!aufn || !sp5 || !sp4 || !rest || !jbsp) { printf("FEHLERZAHL %d\n", fehler); return 1; }
    pruefe(zugriff_medium_pfad(FOTO ".jxr") && zugriff_medium_pfad(VIDEO ".webm") && !zugriff_medium_pfad(FOTO ".meta") && !zugriff_medium_pfad(FOTO ".DAT")
           && !zugriff_medium_pfad("/user/av_contents/photo/NPXS40087/../x/y/z.jxr") && !zugriff_medium_pfad(VS_FOTO) && !zugriff_medium_pfad("/user/av_contents/photo/x.jxr"),
           "Pfad-Regel Aufnahme: Bild/Video ja, .meta/.dat/Vorschaubild/.. nein");
    {
        char b[9], t[65]; bool pr = false;
        pruefe(zugriff_spielstand_pfad(U "/savedata_prospero/PPSA03530", b, &pr, t) && !strcmp(b, "1a2b3c4d") && pr && !strcmp(t, "PPSA03530")
               && zugriff_spielstand_pfad(U "/savedata/CUSA09172", NULL, &pr, NULL) && !pr
               && !zugriff_spielstand_pfad("/user/home/u2/savedata/CUSA10370", NULL, NULL, NULL) && !zugriff_spielstand_pfad(U "/savedata/..", NULL, NULL, NULL)
               && !zugriff_spielstand_pfad(U "/savedata/A/B", NULL, NULL, NULL) && !zugriff_spielstand_pfad(U "/trophy/data", NULL, NULL, NULL),
               "Pfad-Regel Spielstand-Ordner: Benutzer, PS5/PS4, Titel; fremde Formen nein");
    }

    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
    aufn->Wahl = sp5->Wahl = sp4->Wahl = rest->Wahl = jbsp->Wahl = true;
    ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
    printf("INFO Ergebnis: Fehler=%d Teilweise=%d Unveraendert=%d Geschuetzt=%d GEISTER=%d\n", e.Fehler, e.Teilweise, e.Unveraendert, e.Geschuetzt, K->GEISTER.n);
    bool foto = da_w(FOTO ".jxr"), video = da_w(VIDEO ".webm"), sdimg5 = da_w(U "/savedata_prospero/PPSA03530/sdimg_GameUserSettingsini"),
         meta5 = da_w(U "/savedata_prospero_meta/user/PPSA03530"), sdimg4 = da_w(U "/savedata/CUSA09172/sdimg_SAVEGAMEfinal1.lanoire"), app = da_w("/user/app/PPSA68888");
    bool keine_geister = true;
    for (int i = 0; i < K->GEISTER.n; i++) if (strstr(K->GEISTER.e[i], "Mediengalerie") || strstr(K->GEISTER.e[i], "Spielstand-Liste")) keine_geister = false;

    if (!strcmp(fall, "ok")) {
        pruefe(!foto && !da_w(FOTO ".meta") && !da_w(FOTO ".dat") && !da_w(VS_FOTO), "Bild samt .meta/.dat und Vorschaubild weg (über Sony)");
        pruefe(!video && !da_w(VIDEO ".meta") && !da_w(VIDEO ".dat") && !da_w(VS_VIDEO), "Video samt .meta/.dat und Vorschaubild weg (über Sony)");
        pruefe(!da_w(VS_REST) && !da_w("/user/av_contents/photo/NPXS40087/PPSA03530") && !da_w("/user/av_contents/thumbnails/photo/NPXS40087/PPSA03530"),
               "Reste danach selbst gelöscht (Vorschaubild ohne Aufnahme, leere Ordner)");
        pruefe(log_zeile("gelöscht", "über Sonys Löschdienst") >= 0, "Meldung: Aufnahmen über Sonys Löschdienst gelöscht");
        pruefe(!sdimg5 && !meta5 && !sdimg4 && !da_w(U "/savedata_prospero/PPSA03530"), "Spielstände (PS5 samt Bild/Beschreibung, PS4) weg");
        pruefe(!da_w(U "/savedata/CUSA16074"), "Jailbreak-Spielstand (anderer Eintrag) über die Spielstand-Liste gelöscht");
        pruefe(log_zeile("gelöscht", "samt Eintrag in der Spielstand-Liste") >= 0, "Meldung: Spielstand samt Eintrag in der Spielstand-Liste gelöscht");
        pruefe(!app && log_zeile("PPSA68888", "Reste ohne Kachel") >= 0, "Reste ohne Kachel (Sony 226): App-Ordner direkt gelöscht, eigene Meldung");
        pruefe(log_zeile("Sony löscht nach 60 s noch", NULL) < 0, "keine falsche Meldung \"Sony löscht noch\"");
        pruefe(e.Fehler == 0 && e.Teilweise == 0 && e.Unveraendert == 0, "keine Fehler, nichts teilweise, nichts unverändert");
        pruefe(keine_geister, "kein Hinweis auf leere Einträge (Mediengalerie/Spielstand-Liste)");
    } else if (!strcmp(fall, "ohne") || !strcmp(fall, "ablehnen") || !strcmp(fall, "belegt")) {
        bool beide = strcmp(fall, "belegt") != 0;   /* belegt: nur die Spielstände bleiben, Aufnahmen gehen über Sony */
        if (beide) pruefe(foto && video && da_w(FOTO ".meta") && da_w(VS_FOTO) && da_w(VS_REST), "Aufnahmen bleiben ganz (nichts gelöscht)");
        else pruefe(!foto && !video, "Aufnahmen über Sony gelöscht");
        pruefe(sdimg5 && meta5 && sdimg4, "Spielstände bleiben ganz (Datei, Bild/Beschreibung)");
        pruefe(da_w(U "/savedata/CUSA16074/sdimg_SAVEDATA00"), "Jailbreak-Spielstand (anderer Eintrag) bleibt ganz");
        pruefe(log_zeile("unverändert", "Spielstand") >= 0 || log_zeile("unchanged", "Saved data") >= 0, "Meldung: Spielstand unverändert");
        if (!strcmp(fall, "ohne")) pruefe(log_zeile("unverändert", "nur über Sonys Löschdienst") >= 0 && log_zeile("unverändert", "nur zusammen mit ihrem Eintrag") >= 0, "Grund genannt: nur auf der PS5");
        if (!strcmp(fall, "belegt")) pruefe(log_zeile("unverändert", "Spiel läuft") >= 0, "Grund genannt: Spiel läuft");
        if (!strcmp(fall, "ablehnen")) pruefe(log_zeile("unverändert", "abgelehnt") >= 0 && log_zeile("unverändert", "belegt") >= 0, "Gründe genannt (Sony abgelehnt, Liste belegt)");
        pruefe(e.Unveraendert == (beide ? 4 : 3) && e.Fehler == 0, "unverändert gezählt (mit Jailbreak-Spielstand), keine Fehler");
        pruefe(keine_geister, "kein Hinweis auf leere Einträge");
    } else if (!strcmp(fall, "teil")) {
        pruefe(!foto && !da_w(FOTO ".meta") && !da_w(VS_FOTO), "Bild über Sony gelöscht");
        pruefe(video && da_w(VIDEO ".meta") && da_w(VS_VIDEO) && da_w(VS_REST), "Video (Sony lehnt ab) und Reste bleiben");
        pruefe(e.Teilweise == 1 && log_zeile("TEILWEISE", NULL) >= 0 && log_zeile("übrig", "Sony hat 1 Aufnahme(n) gelöscht") >= 0, "Meldung TEILWEISE mit Grund");
    } else if (sim) {
        const sliste *bu = zugriff_sim_buch(z);
        for (int i = 0; i < bu->n; i++) printf("BUCH %s\n", bu->e[i]);
        pruefe(buch_zeile(bu, "SITE MEDIUM " FOTO ".jxr") >= 0 && buch_zeile(bu, "SITE MEDIUM " VIDEO ".webm") >= 0, "Buch: beide Aufnahmen an Sony");
        pruefe(buch_zeile(bu, "SITE SPIELSTAND " U "/savedata_prospero/PPSA03530") >= 0 && buch_zeile(bu, "SITE SPIELSTAND " U "/savedata/CUSA09172") >= 0, "Buch: beide Spielstände ausgetragen");
        pruefe(buch_zeile(bu, "SITE SPIELSTAND " U "/savedata/CUSA16074") >= 0, "Buch: Jailbreak-Spielstand ausgetragen");
        int s1 = buch_zeile(bu, "SITE SPIELSTAND " U "/savedata_prospero/PPSA03530"), d1 = buch_zeile(bu, U "/savedata_prospero/PPSA03530");
        for (int i = 0; i < bu->n; i++) if (strstr(bu->e[i], U "/savedata_prospero/PPSA03530") && !strstr(bu->e[i], "SITE SPIELSTAND")) { d1 = i; break; }
        pruefe(s1 >= 0 && d1 > s1, "Reihenfolge: erst Eintrag austragen, dann Dateien löschen");
        pruefe(foto && video && sdimg5 && sdimg4, "Simulation: nichts auf der Platte gelöscht");
        pruefe(log_zeile("würde gelöscht", "über Sonys Löschdienst") >= 0 && log_zeile("würde gelöscht", "samt Eintrag in der Spielstand-Liste") >= 0, "Meldungen der Simulation");
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
