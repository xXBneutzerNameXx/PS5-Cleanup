/* PS5 Cleanup 1.0.7 - Test "Namen und Zuordnung von Resten nur aus Metadaten" (PS5-Test 02.11.2026: GTA V / Red Dead Redemption 2)
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
    text_hat("/user/trophy/conf/NPWR72099_00", "NPWR72099_00 (Spiel/App unbekannt)", "Trophäen-Liste ohne Beleg: ausdrücklich unbekannt");
    zeile_hat("/user/trophy/conf/NPWR72099_00", "NPWR72099_00 (Spiel/App unbekannt) – Trophäen-Liste", "Zeile der Trophäen-Liste");

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
