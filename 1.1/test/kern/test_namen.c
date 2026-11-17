/* PS5 Cleanup - Test "Namen und Zuordnung von Resten nur aus Metadaten" (PS5-Test 02.11.2026: GTA V / Red Dead Redemption 2)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_namen <hier>
 * da_Chris86 02.11.2026 23:05: "Verknüpfungen anhand der Metadaten belegen; nichts anhand ähnlicher IDs oder Namen raten. Bei unbekannter
 * Zuordnung ausdrücklich darauf hinweisen." - "Fehlerhafte Zusatzinhalt-Service-IDs beim Nachschauen erkennen und deutlich anzeigen." */
#include "../../kern/kern.h"
#include "../../kern/ablauf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void melde_log(void *c, const char *t) { (void)c; (void)t; }
static kern *K; static arena *A0;
static eintrag *mit_pfad(const char *p)
{
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (!strcmp(f->PfadeBasis.e[j], p)) return f; }
    return NULL;
}
static const char *zeile(const eintrag *f) { return f ? kern_zeile(A0, f) : "(nicht gefunden)"; }
static void text_hat(const char *p, const char *soll, const char *was)
{
    eintrag *f = mit_pfad(p);
    pruefe(f && strstr(f->TextBasis, soll), a_fmt(A0, "%s: Text enthält \"%s\" (%s)", was, soll, f ? f->TextBasis : "nicht gefunden"));
}
static void zeile_hat(const char *p, const char *soll, const char *was)
{
    eintrag *f = mit_pfad(p);
    pruefe(f && strstr(zeile(f), soll), a_fmt(A0, "%s: Zeile enthält \"%s\" (%s)", was, soll, zeile(f)));
}
static void text_ohne(const char *p, const char *nicht, const char *was)
{
    eintrag *f = mit_pfad(p);
    pruefe(f && !strstr(f->TextBasis, nicht), a_fmt(A0, "%s: Text ohne \"%s\" (%s)", was, nicht, f ? f->TextBasis : "nicht gefunden"));
}
/* Sonys appinfo.db wie auf da_Chris86s PS5 (02.11.2026): GTA V PPSA04264 INSTALL_DIR_SAVEDATA = PPSA03420 - hier mit Test-Kennungen */
static int appinfo_aufrufe = 0;
static void test_appinfo(struct kern *k, void (*z)(struct kern *k, const char *titel, const char *feld, const char *wert))
{
    appinfo_aufrufe++;
    /* NEU 1.0.8 (PS5-Test 07.11.2026): Zeilen aus Sonys Spielstand-Liste (savedata.db) - Titel = Untertitel meldet die Plattform gar nicht */
    z(k, "CUSA72170", "SPIELSTAND_TITEL", "Kistenrennen"); z(k, "CUSA72170", "SPIELSTAND_TITEL", " Kistenrennen ");
    z(k, "CUSA72171", "SPIELSTAND_TITEL", "Kapitel 1"); z(k, "CUSA72171", "SPIELSTAND_TITEL", "Kapitel 2");   /* verschiedene Titel: nicht belegt */
    z(k, "CUSA72160", "SPIELSTAND_TITEL", "Anderer Titel");   /* das Protokoll geht vor */
    z(k, "PPSA72080", "SPIELSTAND_TITEL", "Auch falsch");     /* Widerspruch bleibt unbekannt */
    z(k, "CUSA72175", "SPIELSTAND_TITEL", "Gespeicherte Daten"); z(k, "CUSA72175", "SPIELSTAND_TITEL", "Saved Data");   /* NEU 1.0.8: allgemeine Titel */
    z(k, "PPSA72010", "INSTALL_DIR_SAVEDATA", "CUSA72190"); z(k, "PPSA72040", "INSTALL_DIR_SAVEDATA", "CUSA72190");   /* Codex 108: Widerspruch nur beim Spielstand */
    z(k, "PPSA72010", "INSTALL_DIR_SAVEDATA", "PPSA72011");              /* der GTA-Fall */
    z(k, "PPSA72010", "SERVICE_ID_ADDCONT_ADD_1", "EP1004-PPSA72080_00"); /* PPSA72001 nennt dieselbe Kennung -> widersprüchlich */
    z(k, "PPSA72010", "SERVICE_ID_ADDCONT_ADD_2", "0");                   /* unbenutzt */
    z(k, "PPSA72010", "INSTALL_DIR_SAVEDATA", "PPSA72001");              /* selbst installiert -> keine Zuordnung */
    z(k, "PPSA79990", "INSTALL_DIR_SAVEDATA", "PPSA79991");              /* Zeile eines nicht installierten Titels -> zählt nicht */
    for (int i = 1; i <= 7; i++) z(k, "CUSA72030", a_fmt(A0, "SERVICE_ID_ADDCONT_ADD_%d", i), "\r\n  ");   /* wie param.sfo - nicht doppelt zählen */
    z(k, "CUSA72020", "SERVICE_ID_ADDCONT_ADD_3", "\r\n  ");          /* Codex 42 (2): in param.sfo leer - die kaputte Angabe hier zählt trotzdem */
    z(k, "PPSA72070", "INSTALL_DIR_SAVEDATA", "PPSA72071");              /* Codex 42 (4): installiert, aber ohne lesbaren Namen */
    z(k, "PPSA72010", "INSTALL_DIR_SAVEDATA", "PPSA72005");              /* PPSA72001 nennt PPSA72005 als dasselbe Spiel -> widersprüchlich */
    z(k, "PPSA72010", "INSTALL_DIR_SAVEDATA", "PPSA72110");              /* Codex 43 (4): nicht installiert, eigene Angaben, Belege widersprechen sich */
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 2) { fprintf(stderr, "Aufruf: test_namen <hier>\n"); return 2; }
    A0 = arena_neu();
    const char *W = a_fmt(A0, "%s/w", argv[1]);
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    K = kern_neu(zugriff_datei(&o)); K->melde = melde_log;
    K->appinfo_lesen = test_appinfo;
    volatile int fe = 0;
    VERSUCHE { kern_suche_funde(K); } FANGE { printf("FAIL Suche abgebrochen: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) { printf("FEHLERZAHL 1\n"); return 1; }
    pruefe(appinfo_aufrufe == 1, a_fmt(A0, "appinfo.db wird je Suche einmal gelesen (%d)", appinfo_aufrufe));

    printf("== Zuordnung über Metadaten (installiertes Spiel)\n");
    const char *H = "/user/home/1234abcd";
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72002", H), "Spielstand PPSA72002 (Strassenkönig V)", "serviceIdForSharing (param.json)");
    zeile_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72002", H), "Strassenkönig V – Spielstände", "Zeile mit dem Spielnamen");
    text_ohne(a_fmt(A0, "%s/savedata_prospero/PPSA72002", H), "nicht mehr", "installiertes Spiel: kein \"nicht mehr\"");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72003", H), "Spielstand CUSA72003 (Strassenkönig V)", "PS4-Kennung desselben Spiels (serviceIdForSharing)");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72011", H), "Spielstand PPSA72011 (Rennfahrer)", "GTA-Fall: INSTALL_DIR_SAVEDATA aus appinfo.db");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72021", H), "Spielstand CUSA72021 (Detektivspiel)", "SERVICE_ID_ADDCONT_ADD_1 (param.sfo)");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72022", H), "Spielstand CUSA72022 (Detektivspiel)", "INSTALL_DIR_SAVEDATA (param.sfo)");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72001", H), "Spielstand PPSA72001 (Strassenkönig V)", "selbst installiert bleibt bei sich");
    {
        eintrag *d = mit_pfad("/user/download/PPSA72002");
        pruefe(!d, a_fmt(A0, "App-Daten der verknüpften Kennung werden gebraucht - nicht als Rest angeboten (%s)", d ? d->TextBasis : "nicht angeboten"));
        eintrag *u = mit_pfad("/user/patch/PPSA72002");
        pruefe(!u, a_fmt(A0, "Update-Ordner der verknüpften Kennung wird nicht als Rest angeboten (Codex 42 (1)) (%s)", u ? u->TextBasis : "nicht angeboten"));
        eintrag *u2 = mit_pfad("/user/patch/PPSA72011");
        pruefe(u2 && strstr(u2->TextBasis, "Update ohne Spiel: PPSA72011 (Spiel/App unbekannt)"),
               a_fmt(A0, "INSTALL_DIR_SAVEDATA gilt nur für Spielstände - Update-Ordner dieser Kennung bleibt ein Rest ohne Namen (Codex 42 (3)) (%s)", u2 ? u2->TextBasis : "nicht gefunden"));
    }
    text_hat("/user/av_contents/photo/NPXS40087/PPSA72002", "Aufnahmen von Strassenkönig V (PPSA72002)", "Aufnahmen der verknüpften Kennung");
    text_hat("/user/av_contents/photo/NPXS40087/PPSA72011", "Spiel/App unbekannt", "Aufnahmen unter einer Spielstand-Kennung: kein Name (Codex 42 (3))");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72071", H), "Spielstand PPSA72071 (PPSA72070)", "Besitzer ohne lesbaren Namen: seine Kennung (Codex 42 (4))");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72070", H), "Spielstand PPSA72070 (Spiel/App unbekannt)", "installiert ohne Namen: gehört nicht dem Spiel, das es nennt (Codex 42 (4))");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72120", H), "Spielstand PPSA72120 (Ohnename)", "Codex 45: installiert ohne lesbaren Namen, Name aus der Spielstand-Beschreibung");
    text_ohne(a_fmt(A0, "%s/savedata_prospero/PPSA72120", H), "nicht mehr", "Codex 45: installiert - kein \"nicht mehr auf der Konsole\"");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72042", H), "Spielstand PPSA72042 (Sternenflug)", "JSON-Escape \\u0045 wird richtig gelesen (Codex 42 (5))");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72023", H), "(Spiel/App unbekannt)", "Kennung mit Zeilenumbruch am Ende gilt nicht (Codex 42 (6))");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72005", H), "(Spiel/App unbekannt)", "Spielstand-Beleg und Zusatzinhalt-Beleg nennen verschiedene Spiele: unbekannt");
    pruefe(!mit_pfad("/user/patch/PPSA72080"), "Codex 43 (1): zwei Spiele nennen PPSA72080 - Besitzer unbekannt, der Update-Ordner wird trotzdem nicht angeboten");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72091", H), "Spielstand PPSA72091 (Zweitquelle)", "Codex 43 (2): serviceIdForSharing aus der zweiten param.json (ohne titleName)");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72110", H), "Spielstand PPSA72110 (Spiel/App unbekannt)", "Codex 43 (4): verbliebene eigene Angaben, Belege widersprechen sich, nicht installiert");
    text_ohne("/user/av_contents/photo/NPXS40087/PPSA72002", "nicht mehr", "Aufnahmen: kein \"nicht mehr\"");

    printf("== Nicht raten\n");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72003", H), "Spielstand PPSA72003 (Spiel/App unbekannt)", "ähnliche Kennung (PPSA72003) bekommt keinen Namen");
    zeile_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72003", H), "PPSA72003 (Spiel/App unbekannt) – Spielstände", "Zeile zeigt Kennung und \"unbekannt\"");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72080", H), "(Spiel/App unbekannt)", "zwei Spiele nennen dieselbe Kennung: widersprüchlich, keine Zuordnung");
    text_ohne(a_fmt(A0, "%s/savedata_prospero/PPSA72080", H), "Irgendwas", "Codex 44: bei Widerspruch auch kein Name aus der Spielstand-Beschreibung");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA79991", H), "(Spiel/App unbekannt)", "appinfo-Zeile eines nicht installierten Titels zählt nicht");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72070", H), "Spielstand CUSA72070 (Spiel/App unbekannt)", "Spielstände mit verschiedenen Titeln: Name nicht belegt");

    printf("== Deinstallierte Spiele: Name aus verbliebenen Angaben\n");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72050", H), "Spielstand CUSA72050 (Nebelstadt, nicht mehr auf der Konsole)", "Name aus der Beschreibung der Spielstände (MAINTITLE)");
    zeile_hat(a_fmt(A0, "%s/savedata/CUSA72050", H), "Nebelstadt, nicht mehr auf der Konsole – Spielstände", "Zeile");
    text_hat("/user/license/fakeEP1004-CUSA72050_00-NEBELSTADT000000.rif", "Nebelstadt (Spiel nicht mehr installiert)", "Fake-Lizenz mit Namen");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72060", H), "Spielstand CUSA72060 (Bergrennen, nicht mehr auf der Konsole)", "Name aus der Begleitdatei einer Aufnahme");
    text_hat("/user/av_contents/photo/NPXS40087/CUSA72060", "Aufnahmen von Bergrennen (CUSA72060, Spiel nicht mehr auf der Konsole)", "Aufnahmen (wie bisher)");
    text_hat("/user/av_contents/photo/NPXS40087/CUSA72090", "(Spiel nicht mehr auf der Konsole, Spiel/App unbekannt)", "Aufnahmen ohne jeden Beleg: ausdrücklich unbekannt");
    /* NEU 1.0.8 (F4, da_Chris86s Bilder vom 04.11.2026): bis 1.0.7 "Aufnahmen von 20261104175554 (PSCL00002, Spiel nicht mehr auf der Konsole)" */
    text_hat("/user/av_contents/photo/NPXS40087/PSCL00002", "Aufnahmen von PS5 Cleanup (PSCL00002)", "Aufnahmen, während PS5 Cleanup lief: heißen nach PS5 Cleanup");
    text_hat("/user/trophy/conf/NPWR72099_00", "NPWR72099_00 (Spiel/App unbekannt)", "Trophäen-Liste ohne Beleg: ausdrücklich unbekannt");
    zeile_hat("/user/trophy/conf/NPWR72099_00", "NPWR72099_00 (Spiel/App unbekannt) – Trophäen-Liste", "Zeile der Trophäen-Liste");

    /* NEU 1.0.8 (PS5-Test 07.11.2026, L.A. Noire: "CUSA09172 (Spiel/App unbekannt)" und zwei Trophäen-Listen ohne Namen) */
    printf("== NEU 1.0.8: Namen aus früheren Protokollen, Sonys Spielstand-Liste und PKG-Dateien\n");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72160", H), "Spielstand CUSA72160 (Hafenkrimi, nicht mehr auf der Konsole)", "Name aus der Liste der PS4-Spiele im neuesten Protokoll (geht vor Spielstand-Liste)");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72161", H), "Spielstand PPSA72161 (Sternwarte, nicht mehr auf der Konsole)", "Name aus einer Löschzeile (M.2, ShadowMount-Spiel)");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72162", H), "(Spiel/App unbekannt)", "Protokoll: Zeile einer Tool-Kachel zählt nicht");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72163", H), "(Spiel/App unbekannt)", "Protokoll: Platzhalter \"unbekannter Name\" zählt nicht");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72164", H), "(Spiel/App unbekannt)", "Protokoll: eingerückte Zeile außerhalb der Liste der PS4-Spiele zählt nicht");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72165", H), "(Spiel/App unbekannt)", "Protokoll: Reste-Zeile zählt nicht");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72167", H), "Spielstand PPSA72167 (Blockhafen, nicht mehr auf der Konsole)", "Codex-Prüfung 109 (P2): Löschzeile eines Teils im Block (eingerückt) - Name ohne die Einrückung");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72166", H), "Spielstand CUSA72166 (Echtname, nicht mehr auf der Konsole)", "Codex-Prüfung 109 (P3): Kennung als Name im Protokoll zählt nicht - der Name kommt dann aus der PKG-Datei");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72170", H), "Spielstand CUSA72170 (Kistenrennen, nicht mehr auf der Konsole)", "Titel aus Sonys Spielstand-Liste");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72171", H), "(Spiel/App unbekannt)", "Spielstand-Liste mit verschiedenen Titeln: nicht belegt");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72172", H), "(Spiel/App unbekannt)", "Spielstand-Beschreibung mit Titel = Untertitel zählt nicht");
    /* NEU 1.0.8 (Protokoll 174343 vom PS5-Test 07.11.2026, Spielstand von GTA V als "Gespeicherte Daten"): allgemeine Titel sind kein Spielname */
    text_hat(a_fmt(A0, "%s/savedata/CUSA72173", H), "(Spiel/App unbekannt)", "allgemeiner Spielstand-Titel \"Gespeicherte Daten\" ist kein Spielname");
    text_ohne(a_fmt(A0, "%s/savedata/CUSA72173", H), "Gespeicherte Daten", "allgemeiner Spielstand-Titel steht nicht als Name da");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72174", H), "Spielstand CUSA72174 (Kristallhöhle, nicht mehr auf der Konsole)", "allgemeiner Titel (\"Saved Data 2\") zählt nicht, der echte Name daneben schon");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72175", H), "(Spiel/App unbekannt)", "allgemeine Titel aus Sonys Spielstand-Liste zählen nicht");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72180", H), "Spielstand CUSA72180 (Kistenspiel, nicht mehr auf der Konsole)", "Name aus der PKG-Datei, nach der Suche nachgetragen");
    zeile_hat(a_fmt(A0, "%s/savedata/CUSA72180", H), "Kistenspiel, nicht mehr auf der Konsole – Spielstände", "Zeile mit dem Namen aus der PKG-Datei");
    text_hat("/user/trophy/conf/NPWR72181_00", "Trophäen-Liste: Kistenspiel (NPWR72181_00)", "Trophäen-Liste: dieselbe Trophäen-Datei steckt in der PKG-Datei");
    text_hat(a_fmt(A0, "%s/trophy/data/NPWR72181_00", H), "Kistenspiel (NPWR72181_00)", "Trophäen-Fortschritt bekommt denselben Namen");
    text_hat(a_fmt(A0, "%s/savedata/CUSA72182", H), "(Spiel/App unbekannt)", "Zusatzinhalt-PKG nennt nicht den Spielnamen: zählt nicht");
    text_ohne(a_fmt(A0, "%s/savedata_prospero/PPSA72080", H), "Falschname", "Widerspruch bleibt unbekannt, auch mit Namen in einer PKG-Datei");
    text_ohne(a_fmt(A0, "%s/savedata_prospero/PPSA72080", H), "Auch falsch", "Widerspruch bleibt unbekannt, auch mit Titel in der Spielstand-Liste");
    text_ohne(a_fmt(A0, "%s/savedata/CUSA72190", H), "Falscher", "Codex-Prüfung 108 (P2): Widerspruch beim Spielstand bleibt, auch wenn eine Aufnahme derselben Kennung ohne Beleg ist");
    text_ohne("/user/av_contents/photo/NPXS40087/CUSA72190", "Falscher", "Codex-Prüfung 108 (P2): ... und auch die Aufnahme bekommt den Namen nicht");
    text_ohne("/user/license/fakeEP1004-CUSA72191_00-CUSA72180XXXXXXX.rif", "Kistenspiel", "Codex-Prüfung 108 (P2): Fake-Lizenz einer anderen Kennung bekommt nicht den Namen aus der Inhaltskennung");
    text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72195", H), "Spielstand PPSA72195 (Echtspiel)", "Prüfer 110 (P3): installiert ohne lesbaren Namen - Name aus der PKG-Datei");
    text_ohne(a_fmt(A0, "%s/savedata_prospero/PPSA72195", H), "nicht mehr", "Prüfer 110 (P3): ... ohne \"nicht mehr auf der Konsole\"");

    /* NEU 1.0.8 (PS5-Test 07.11.2026 16:31, da_Chris86: "bei den PS4-Spielen sind die Bilder und die Beschriftung da, bei den PS5-Spielen nicht") */
    printf("== NEU 1.0.8: PS5-Trophäen-Listen und Spiele ohne Kachel mit Namen\n");
    text_hat("/user/trophy2/nobackup/conf/NPWR72196_00", "Trophäen-Liste: Sternenreise (NPWR72196_00)", "PS5-Trophäen-Liste: Name aus TROPHY.UCP (tropmeta_de-DE.json)");
    zeile_hat("/user/trophy2/nobackup/conf/NPWR72196_00", "Sternenreise – Trophäen-Liste", "Zeile der PS5-Trophäen-Liste");
    text_hat(a_fmt(A0, "%s/trophy2/nobackup/data/NPWR72196_00", H), "Trophäen-Fortschritt: Sternenreise (NPWR72196_00)", "PS5-Trophäen-Fortschritt bekommt denselben Namen");
    text_hat("/user/trophy2/nobackup/conf/NPWR72197_00", "NPWR72197_00 (Spiel/App unbekannt)", "TROPHY.UCP nennt eine andere Kennung: kein Name");
    text_ohne("/user/trophy2/nobackup/conf/NPWR72197_00", "Fremdliste", "... und auch nicht der fremde Name");
    {
        const char *og = "/mnt/usb0/homebrew/Ordnerspiel - PPSA72200 - v1.00";
        text_hat(og, "[Spiel: Ordnerspiel Echt (PPSA72200)]  [Spiel-Ordner ohne Kachel]", "Spiel-Ordner ohne Kachel: Name aus sce_sys/param.json");
        zeile_hat(og, "Spieldatei ohne Kachel: Ordnerspiel Echt (", "Zeile mit dem Spielnamen vorne");
        const char *b = mt_hat(&K->BILD_ORT, "PPSA72200") ? (const char *)mt_hole(&K->BILD_ORT, "PPSA72200") : "(keins)";
        pruefe(!strcmp(b, a_fmt(A0, "%s/sce_sys/icon0.png\n0\n0", og)), a_fmt(A0, "Bild des Spiel-Ordners: sce_sys/icon0.png (%s)", b));
        text_hat("/mnt/usb0/homebrew/PPSA72161.ffpkg", "[Spiel: Sternwarte (PPSA72161)]", "Spieldatei nicht lesbar: Name aus dem früheren Protokoll (Spielnummer im Dateinamen)");
        text_ohne("/mnt/usb0/homebrew/PPSA72201.ffpkg", "[Spiel:", "Spieldatei ohne jeden Beleg: kein Name");
        pruefe(!mt_hat(&K->BILD_ORT, "PPSA72161") && !mt_hat(&K->BILD_ORT, "PPSA72201"), "Spieldatei nicht lesbar: kein Bild gemerkt");
    }
    /* NEU 1.0.9 (08.11.2026, Nachprüfung kstuff-lite ps5-kstuff-ldr/src/image.c): .ufs ist ein UFS-Abbild wie .ffpkg. Bis 1.0.8 stand eine
     * .ufs ohne Kachel gelb als normale Datei, ein Ordner mit einer .ufs wurde nicht aufgeklappt. */
    printf("== NEU 1.0.9: .ufs ist ein Spiel-Abbild\n");
    {
        eintrag *u = mit_pfad("/mnt/usb0/homebrew/PPSA72202.ufs");
        pruefe(u && u->Kat == 1 && u->Art == ART_ABBILD && kern_ampel(u) == AMPEL_ROT, a_fmt(A0, ".ufs ohne Kachel: unter Spiele, rot (%s)", u ? u->TextBasis : "nicht gefunden"));
        text_hat("/mnt/usb0/homebrew/PPSA72202.ufs", "[ohne Kachel]", ".ufs ohne Kachel");
        eintrag *g = mit_pfad("/mnt/usb0/spiele/PPSA72203.UFS");
        pruefe(g && g->Kat == 1 && kern_ampel(g) == AMPEL_ROT, a_fmt(A0, "Ordner mit einer .UFS wird aufgeklappt, die Datei steht unter Spiele (%s)", g ? g->TextBasis : "nicht gefunden"));
        pruefe(!mit_pfad("/mnt/usb0/spiele"), "... der Ordner selbst ist kein Eintrag (sonst ginge das Abbild mit dem Ordner weg)");
    }
    /* NEU 1.0.8 (PS5-Test 07.11.2026 17:31/17:32): Kopie des Fortschritts mit Endung, zweite Trophäen-Datei im PKG */
    text_hat(a_fmt(A0, "%s/trophy/data/NPWR72181_00_BK", H), "Trophäen-Fortschritt: Kistenspiel (NPWR72181_00)", "Kopie des Fortschritts (_BK): Name der Liste ohne Endung");
    text_hat(a_fmt(A0, "%s/trophy/data/NPWR72181_00_BK", H), "[Kopie: NPWR72181_00_BK]", "... der Ordnername bleibt sichtbar");
    zeile_hat(a_fmt(A0, "%s/trophy/data/NPWR72181_00_BK", H), "Kistenspiel – dein Trophäen-Fortschritt (Kopie)", "Zeile sagt \"Kopie\"");
    zeile_hat(a_fmt(A0, "%s/trophy/data/NPWR72181_00", H), "Kistenspiel – dein Trophäen-Fortschritt", "Original ohne \"Kopie\"");
    { eintrag *o = mit_pfad(a_fmt(A0, "%s/trophy/data/NPWR72181_00", H)); pruefe(o && !strstr(zeile(o), "Kopie"), a_fmt(A0, "Original: Zeile ohne \"Kopie\" (%s)", zeile(o))); }
    {   /* Codex-Prüfung 120 (P3): die Kopie steht im Block ihrer Liste, wie das Original - kein zweiter Block für dasselbe Spiel */
        eintrag *li = mit_pfad("/user/trophy/conf/NPWR72181_00"), *og = mit_pfad(a_fmt(A0, "%s/trophy/data/NPWR72181_00", H)), *ko = mit_pfad(a_fmt(A0, "%s/trophy/data/NPWR72181_00_BK", H));
        pruefe(li && og && ko && og->ToolKachel == li && ko->ToolKachel == li,
               a_fmt(A0, "Kopie (_BK) im Block ihrer Liste wie das Original (Kopf der Kopie: %s, Kopf des Originals: %s)", ko && ko->ToolKachel ? zeile(ko->ToolKachel) : "-", og && og->ToolKachel ? zeile(og->ToolKachel) : "-"));
    }
    text_hat(a_fmt(A0, "%s/trophy/data/NPWR72189_00", H), "Trophäen-Fortschritt: Altliste (NPWR72189_00)", "Fortschritt ohne Liste: Name aus der Löschzeile der Liste im Protokoll");
    text_hat(a_fmt(A0, "%s/trophy/data/NPWR72191_00", H), "Trophäen-Fortschritt: Blockliste (NPWR72191_00)", "... auch aus einer eingerückten Blockzeile");
    text_hat(a_fmt(A0, "%s/trophy/data/NPWR72190_00", H), "NPWR72190_00 (Spiel/App unbekannt)", "\"unbekannt\" im Protokoll ist kein Name");
    text_hat("/user/trophy/conf/NPWR72187_00", "Trophäen-Liste: Doppelrennen (NPWR72187_00)", "Trophäen-Liste aus der ersten Trophäen-Datei eines PKG (0x1400)");
    text_hat("/user/trophy/conf/NPWR72188_00", "Trophäen-Liste: Doppelrennen (NPWR72188_00)", "Trophäen-Liste aus der zweiten Trophäen-Datei eines PKG (0x1401, wie DRIVECLUB BIKES)");
    zeile_hat("/user/trophy/conf/NPWR72187_00", "Doppelrennen – Trophäen-Liste 1", "zwei Listen aus einem PKG: Nummer 1");
    zeile_hat("/user/trophy/conf/NPWR72188_00", "Doppelrennen – Trophäen-Liste 2", "zwei Listen aus einem PKG: Nummer 2");
    { eintrag *e1 = mit_pfad("/user/trophy/conf/NPWR72181_00"); pruefe(e1 && e1->ListeNr == 0, a_fmt(A0, "nur eine Liste im PKG: keine Nummer (%s)", zeile(e1))); }
    zeile_hat("/user/trophy/conf/NPWR72192_00", "Protokollrennen – Trophäen-Liste 2", "Codex-Prüfung 116: Name aus dem Protokoll, Nummer aus dem PKG");

    /* NEU 1.0.8 (PS5-Test 07.11.2026 22:53): da_Chris86s Ordner "PS5" auf dem USB-Laufwerk (Screenshots, von der Konsole kopiert) stand als
     * "neu herunterladen" unter "Sonstiges" - eigene Daten, endgültig weg. Alte Protokolle von PS5 Cleanup: kein "neu herunterladen". */
    printf("== NEU 1.0.8: Ordner der Konsole auf dem USB-Laufwerk, alte Protokolle\n");
    {
        eintrag *u = mit_pfad("/mnt/usb0/PS5");
        pruefe(u && u->Kat == 5 && u->Art == ART_USB_KOPIE && u->UsbKopie == USB_KOPIE_AUFNAHMEN && kern_ampel(u) == AMPEL_ROT && u->NurEinzeln,
               a_fmt(A0, "PS5/CREATE: eigene Daten, rot, nur einzeln (%s, Art %d)", zeile(u), u ? u->Art : -1));
        zeile_hat("/mnt/usb0/PS5", "USB-Laufwerk – PS5 – deine Screenshots und Videos", "Zeile sagt, was darin liegt");
        pruefe(u && !strcmp(kern_folge(u), "deine Daten – endgültig weg") && !strcmp(kern_folge_kurz(u), "endgültig weg"),
               a_fmt(A0, "Schild: endgültig weg (%s / %s)", u ? kern_folge(u) : "-", u ? kern_folge_kurz(u) : "-"));
        const char *ui = u ? kern_art_info(A0, u) : "";
        pruefe(strstr(ui, "Darin: deine Screenshots und Videos.") && strstr(ui, "endgültig weg"), a_fmt(A0, "Erklärung (%s)", ui));
        eintrag *s = mit_pfad("/mnt/usb1/PS4");
        pruefe(s && s->Art == ART_USB_KOPIE && s->UsbKopie == (USB_KOPIE_SPIELSTAENDE | USB_KOPIE_SICHERUNG),
               a_fmt(A0, "PS4/SAVEDATA und EXPORT: eigene Daten (%s)", zeile(s)));
        zeile_hat("/mnt/usb1/PS4", "USB-Laufwerk – PS4 – deine Spielstände und Datensicherung", "Spielstände und Datensicherung");
        eintrag *m = mit_pfad("/mnt/usb1/ps5");
        pruefe(m && m->Art == ART_DATEI && !m->UsbKopie, a_fmt(A0, "eigener Ordner \"ps5\" ohne Sony-Unterordner bleibt wie bisher (%s)", zeile(m)));
        eintrag *k4 = mit_pfad("/mnt/usb0/PS4/Kistenspiel.pkg");
        pruefe(k4 && k4->Art == ART_PAKET_DATEI && !k4->UsbKopie, "Ordner \"PS4\" mit PKG-Dateien (wie bei da_Chris86): die PKG-Dateien bleiben PKG-Dateien");
        int falsch = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->UsbKopie && f != u && f != s) falsch++; }
        pruefe(falsch == 0, a_fmt(A0, "sonst nichts als Kopie der Konsole markiert (%d)", falsch));
        eintrag *p = mit_pfad("/mnt/usb0/ps5cleanup_protokoll_2026-11-07_120028.txt");
        pruefe(p && p->Art == ART_PROTOKOLL && kern_ampel(p) == AMPEL_GELB && !strcmp(kern_folge_kurz(p), "nur zum Nachlesen"),
               a_fmt(A0, "altes Protokoll: nur zum Nachlesen, gelb (%s, Art %d)", zeile(p), p ? p->Art : -1));
        pruefe(p && strstr(kern_art_info(A0, p), "Ein Protokoll von PS5 Cleanup"), "Erklärung des Protokolls");
        if (u) {
            bool w = u->Wahl; u->Wahl = true;
            const char *es = kern_endgueltig_satz(K, A0);
            u->Wahl = w;
            pruefe(strstr(es, "1 Ordner mit deinen Daten auf dem USB-Laufwerk") != NULL, a_fmt(A0, "Satz \"Danach endgültig weg\" (%s)", es));
        }
    }

    printf("== Kaputte Zusatzinhalt-Angaben\n");
    {
        eintrag *w = mit_pfad("/user/app/CUSA72030"); if (!w) { for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->KachelId && !strcmp(f->KachelId, "CUSA72030") && f->Teil && !strcmp(f->Teil, "Spiel")) w = f; } }
        pruefe(w && w->SidKaputt == 7, a_fmt(A0, "Wilder Westen 2: 7 kaputte Angaben (param.sfo + appinfo.db nicht doppelt) (%d)", w ? w->SidKaputt : -1));
        pruefe(w && strstr(zeile(w), "ACHTUNG: kaputte Zusatzinhalt-Angaben"), a_fmt(A0, "Warnung in der Zeile (%s)", zeile(w)));
        pruefe(w && strstr(kern_art_info(A0, w), "ACHTUNG: Dieses Spiel hat kaputte Angaben"), "Warnung in der Erklärung");
        bool sw = false; for (int i = 0; i < K->SID_WARNUNG.n; i++) if (strstr(K->SID_WARNUNG.e[i], "Wilder Westen 2 (CUSA72030) hat 7 kaputte")) sw = true;
        pruefe(sw, "Warnung fürs Protokoll (Wilder Westen 2, 7)");
        bool s2 = false; for (int i = 0; i < K->SID_WARNUNG.n; i++) if (strstr(K->SID_WARNUNG.e[i], "Sternenflug (PPSA72040) hat 1 kaputte")) s2 = true;
        pruefe(s2, "kaputter Eintrag in serviceIdForSharing (param.json) wird erkannt");
        bool s3 = false; for (int i = 0; i < K->SID_WARNUNG.n; i++) if (strstr(K->SID_WARNUNG.e[i], "Detektivspiel (CUSA72020) hat 2 kaputte")) s3 = true;
        pruefe(s3, "leer in param.sfo + kaputt in appinfo.db und Kennung mit Zeilenumbruch: 2 kaputte (Codex 42 (2)/(6))");
        bool s4 = false; for (int i = 0; i < K->SID_WARNUNG.n; i++) if (strstr(K->SID_WARNUNG.e[i], "Dreifach (CUSA72100) hat 1 kaputte")) s4 = true;
        pruefe(s4, "Codex 43 (3): kaputte Angabe in der zweiten param.sfo wird erkannt, auch wenn die erste nur den Titel hat");
        bool s5 = false; for (int i = 0; i < K->SID_WARNUNG.n; i++) if (strstr(K->SID_WARNUNG.e[i], "Nullfeld (CUSA72150) hat 1 kaputte")) s5 = true;
        pruefe(s5, "Codex 47 (2): \"  0\" ist kein unbenutzter Wert");
        pruefe(K->SID_WARNUNG.n == 5, a_fmt(A0, "nur die fünf kaputten Spiele werden gemeldet (%d)", K->SID_WARNUNG.n));
        bool platz = false; for (int i = 0; i < K->SID_WARNUNG.n; i++) if (strstr(K->SID_WARNUNG.e[i], "Platzhalter") || strstr(K->SID_WARNUNG.e[i], "Leerfeld")) platz = true;
        pruefe(!platz, "PS5-Test 03.11.2026: Felder aus lauter Leerzeichen (19 wie bei den GTA Definitive Editions) sind unbenutzt - keine Warnung");
        text_hat(a_fmt(A0, "%s/savedata_prospero/PPSA72041", H), "(Sternenflug)", "gültige Kennung neben einer kaputten wird trotzdem zugeordnet");
        int ohne = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->SidKaputt && !(f->KachelId && (!strcmp(f->KachelId, "CUSA72030") || !strcmp(f->KachelId, "PPSA72040") || !strcmp(f->KachelId, "CUSA72020") || !strcmp(f->KachelId, "CUSA72100") || !strcmp(f->KachelId, "CUSA72150")))) ohne++; }
        pruefe(ohne == 0, a_fmt(A0, "keine Warnung bei anderen Einträgen (%d)", ohne));
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
