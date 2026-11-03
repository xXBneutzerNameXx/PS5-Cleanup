/* PS5 Cleanup 1.1 - Test Download-Erkennung (Korrektur 27.10.2026, gleich wie Windows 1.0.6)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_download <hier> [schutz]   Testbaum <hier>/w = Kopie von test/ref105/download_v1 (dl_baum.sh)
 * /user/download/<ID> = dauerhafter App-Datenspeicher: nur ohne jede Installation/Registrierung anbieten,
 * Anforderungen 27.10.2026: Daten installierter Apps gehen mit der App, Jailbreak-Einstiege einzeln löschbar
 * (vollständig nur auf Nachfrage), NPXS nie. Danach echte vollständige Bereinigung der Testkopie. */
#include "../../kern/kern.h"
#include "../../kern/ablauf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void melde_log(void *c, const char *t) { (void)c; (void)t; }
static bool nein(void *c) { (void)c; return false; }
static bool da(const char *p) { struct stat st; return lstat(p, &st) == 0; }
static kern *K; static arena *A0; static const char *W;

/* Bedienablauf "vollständige Bereinigung": Fragen merken, bei der Bestätigung die Auswahl zählen und abbrechen */
static sliste titel; static bool jbAntwort; static int jbGewaehlt = -1;
static bool fw(void *c, const char *text, const char *t, const char *s, const char *g) { (void)c; (void)text; (void)s; (void)g; sl_add(A0, &titel, a_kopie(A0, t));
    if (!strcmp(t, "Jailbreak-Einstiege")) return jbAntwort; if (!strcmp(t, "M.2-SSD")) return false; return true; }
static const char *jbText;
static bool fw2(void *c, const char *text, const char *t, const char *s, const char *g) { (void)c; (void)s; (void)g; sl_add(A0, &titel, a_kopie(A0, t));
    if (!strcmp(t, "Jailbreak")) { jbText = a_kopie(A0, text); return false; } return true; }
static const char *ft(void *c, const char *text, const char *t) { (void)c; (void)text; (void)t;
    jbGewaehlt = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Jailbreak && f->Wahl) jbGewaehlt++; } return NULL; }
static eintrag *mit_pfad(const char *p)
{
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], p)) return f; }
    return NULL;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 2) { fprintf(stderr, "Aufruf: test_download <hier> [schutz]\n"); return 2; }
    bool schutz = argc >= 3 && !strcmp(argv[2], "schutz");
    A0 = arena_neu(); W = VERB(A0, argv[1], "/w");
    printf("== download %s\n", schutz ? "mit Selbstschutz" : "ohne Selbstschutz (wie Windows 1.0.6)");
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    K = kern_neu(zugriff_datei(&o)); K->melde = melde_log;
    volatile int fe = 0;
    bool lesefehler = argc >= 4 && !strcmp(argv[3], "lesefehler");
    VERSUCHE {
        if (schutz) { kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K); }
        kern_suche_funde(K);
    } FANGE { printf("%s Suche abgebrochen: %s\n", lesefehler ? "ERWARTET" : "FAIL", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (lesefehler) { pruefe(fe == 1, "/system_ex/app nicht lesbar: Suche bricht ab (nichts angeboten)"); printf("FEHLERZAHL %d\n", fehler); return fehler ? 1 : 0; }
    if (fe) { printf("FEHLERZAHL 1\n"); return 1; }

    bool m2unklar = argc >= 4 && !strcmp(argv[3], "m2unklar");
    bool usbfehler = argc >= 4 && !strcmp(argv[3], "usbfehler");
    const char *nie[] = { "PPSA55555", "PPSA70001", "PPSA70002", "PPSA70003", "PPSA70004", "HBLD70005", "NPXS40140", "CUSA70008", "PPSA70007", "PPSA01651" };
    const char *grund[] = { "installiert intern", "nur auf der M.2", "nur auf der USB-Erweiterung", "nur /user/appmeta", "nur System-Kachel-Infos", "Homebrew in /system_ex/app", "Disc-Player (NPXS)", "nur /user/app, PS4-Titel", "nur externe System-Kachel-Infos", "installierte YouTube-App (Y2JB)" };
    int NN = 10;
    for (int i = 0; i < NN; i++) {
        if (m2unklar && i == 1) continue;     /* M.2 weg: PPSA70001 ist dann eine mögliche Waise -> unten "gesperrt" */
        if (usbfehler && i == 2) {            /* USB nicht lesbar: Daten der reinen USB-App sichtbar, aber gesperrt */
            eintrag *u = mit_pfad("/user/download/PPSA70002");
            /* da_Chris86 27.10.2026 21:39: nicht lesbarer Datenträger sperrt nichts - die Daten werden normal angeboten */
            pruefe(u && !u->Gesperrt, a_fmt(A0, "USB nicht lesbar: Daten der (nicht sichtbaren) USB-App normal angeboten (%s)", u ? u->Text : "nicht angeboten"));
            continue;
        }
        eintrag *f = mit_pfad(VERB(A0, "/user/download/", nie[i]));
        /* Anforderungen 27.10.2026: die Daten einer installierten App gehen mit ihr - nie als eigener Download-Rest */
        bool ok = f == NULL || (ist_teil(f, "Spiel") && strstr(f->Text, nie[i]));
        pruefe(ok, VERB(A0, "nicht als Rest angeboten: ", nie[i], " (", grund[i], ")", f ? VERB(A0, " - gehört zur App: ", f->Text) : ""));
    }
    eintrag *g55 = mit_pfad("/user/download/PPSA55555");
    /* M.2 unklar (mit Selbstschutz): ein internes Spiel könnte eine Kopie auf der M.2 haben (Zwilling) - dann bleiben
     * gemeinsame Daten wie der App-Datenspeicher stehen und werden nicht angeboten */
    bool usbkopie = argc >= 4 && !strcmp(argv[3], "usbkopie");
    bool zw = false;   /* da_Chris86 27.10.2026 21:39: fehlende/nicht erkannte M.2 sperrt nichts mehr */
    if (zw || usbkopie) pruefe(g55 == NULL, "mögliche/echte Kopie (M.2 unklar bzw. USB): Daten der installierten App PPSA55555 bleiben");
    else pruefe(g55 && ist_teil(g55, "Spiel") && !g55->Gesperrt, a_fmt(A0, "Daten der installierten App PPSA55555 hängen an ihrem Spiel-Eintrag (%s)", g55 ? g55->Text : "-"));
    const char *jbd[] = { "PPSA01650", "PPSA01652", "PPSA01615" };
    for (int i = 0; i < 3; i++) {
        eintrag *j = mit_pfad(VERB(A0, "/user/download/", jbd[i]));
        pruefe(j && !j->Gesperrt && j->Jailbreak && j->NurEinzeln && strstr(j->Text, "Jailbreak-Einstieg"), a_fmt(A0, "Jailbreak-Daten ohne App sichtbar, einzeln löschbar: %s", j ? j->Text : jbd[i]));
    }
    eintrag *yta = mit_pfad("/user/app/PPSA01651");
    pruefe(yta && !yta->Gesperrt && yta->Jailbreak && strstr(yta->Text, "Jailbreak-Einstieg"), a_fmt(A0, "installierte YouTube-App einzeln löschbar: %s", yta ? yta->Text : "-"));
    if (zw) pruefe(mit_pfad("/user/download/PPSA01651") == NULL, "M.2 unklar: Daten der YouTube-App bleiben");
    else pruefe(yta && mit_pfad("/user/download/PPSA01651") == yta, "Daten der YouTube-App hängen an der App");
    eintrag *yt = mit_pfad("/user/download/PPSA01650");
    eintrag *w = mit_pfad("/user/download/PPSA70006");
    eintrag *ad = mit_pfad("/user/appmeta/addcont/PPSA01650");
    if (zw) pruefe(ad && ad->Gesperrt, a_fmt(A0, "M.2 unklar: Zusatzinhalt-Infos gesperrt: %s", ad ? ad->Text : "-"));
    else pruefe(ad && !ad->Gesperrt && ad->Jailbreak, a_fmt(A0, "Zusatzinhalt-Infos des Einstiegs als Jailbreak-Einstieg markiert: %s", ad ? ad->Text : "-"));
    eintrag *ext = mit_pfad("/system_data/priv/appmeta/external/PPSA70009");
    eintrag *jx = mit_pfad("/system_data/priv/appmeta/external/PPSA01614");
    pruefe(ext && !ext->Gesperrt && jx != ext, a_fmt(A0, "Sammeleintrag bleibt wählbar, der Einstieg ist herausgenommen: %s", ext ? ext->Text : "-"));
    if (!zw) pruefe(jx && jx->Jailbreak && !jx->Gesperrt, a_fmt(A0, "herausgenommener Einstieg sichtbar als eigener Jailbreak-Eintrag: %s", jx ? jx->Text : "-"));
    /* da_Chris86 27.10.2026 21:39: USB nicht lesbar / M.2 fehlt oder nicht erkannt -> nichts pauschal gesperrt, normal angeboten */
    if (m2unklar) {
        eintrag *m = mit_pfad("/user/download/PPSA70001");
        pruefe(m && !m->Gesperrt, a_fmt(A0, "M.2 fehlt/unklar: Daten des (nicht sichtbaren) M.2-Spiels normal angeboten (%s)", m ? m->Text : "nicht angeboten"));
    }
    pruefe(w && !w->Gesperrt && !w->NurEinzeln && w->Kat == 5 && strstr(w->Text, "App-Daten ohne installierte App"), a_fmt(A0, "echte Waise normal angeboten (%s)", w ? w->Text : "-"));
    int alt = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (!strncmp(f->Text, "Download: ", 10)) alt++; }
    pruefe(alt == 0, "keine Einträge mehr mit der irreführenden Bezeichnung \"Download: \"");

    /* NEU 29.10.2026 (da_Chris86: nur die Jailbreak-Einstiege schützen): Lua-Spiel + Spielstand, PS2-Spielstand, Netflix JP, Browserspeicher */
    eintrag *lua = mit_pfad("/user/app/CUSA17122"), *luasd = mit_pfad("/user/home/u1/savedata/CUSA17122"), *ps2 = mit_pfad("/user/home/u1/savedata/CUSA02199"),
            *nfjp = mit_pfad("/user/app/PPSA01616"), *br = mit_pfad("/user/home/u1/webkit/shell/local"), *tool = mit_pfad("/user/data/ps5_autoloader"), *sd = mit_pfad("/user/home/u1/savedata/CUSA70010");
    if (!lesefehler) {
        pruefe(lua && lua->Jailbreak && lua->NurEinzeln && lua->Kat == 7, a_fmt(A0, "Lua-Spiel CUSA17122: Jailbreak-Einstieg, nur einzeln, Bereich 7 Jailbreak (1.0.7) (%s)", lua ? lua->Text : "-"));
        pruefe(luasd && luasd->Jailbreak && luasd->NurEinzeln && luasd->Kat == 7, a_fmt(A0, "Spielstand des Lua-Spiels: Jailbreak-Einstieg, Bereich 7 Jailbreak (1.0.7) (%s)", luasd ? luasd->Text : "-"));
        pruefe(ps2 && ps2->Jailbreak && ps2->NurEinzeln && ps2->Kat == 7, a_fmt(A0, "Okage-Spielstand: Jailbreak-Einstieg, Bereich 7 Jailbreak (1.0.7) (%s)", ps2 ? ps2->Text : "-"));
        pruefe(nfjp && nfjp->Jailbreak && nfjp->NurEinzeln, a_fmt(A0, "Netflix JP PPSA01616: Jailbreak-Einstieg (%s)", nfjp ? nfjp->Text : "-"));
        pruefe(br && br->Jailbreak && br->NurEinzeln && br->Kat == 7, a_fmt(A0, "Browserspeicher: Jailbreak-Einstieg (WebKit), nur einzeln, Bereich 7 Jailbreak (1.0.7) (%s)", br ? br->Text : "-"));
        pruefe(tool && !tool->Jailbreak && !tool->NurEinzeln, a_fmt(A0, "Jailbreak-Tool (ps5_autoloader) bleibt wie bisher - kein Einstieg (%s)", tool ? tool->Text : "-"));
        pruefe(sd && !sd->Jailbreak && sd->Kat == 2, a_fmt(A0, "normaler Spielstand bleibt unter Spielstände (%s)", sd ? sd->Text : "-"));
        if (lua) { const char *h = kern_jb_hinweis(A0, lua), *w = kern_jb_warnung(A0, lua);
            printf("INFO Hinweis: %s\nINFO Warnung: %s\n", h, w);
            pruefe(strstr(h, "Lua-Jailbreak") && strstr(w, "Lua-Jailbreak"), "Hinweis und Warnung nennen den Jailbreak (Lua-Jailbreak)"); }
        if (ps2) pruefe(!strcmp(kern_jb_weg_eintrag(ps2), "PS2-Spielstand-Jailbreak"), "Okage gehört zum PS2-Spielstand-Jailbreak");
        if (br) pruefe(strstr(kern_jb_weg_eintrag(br), "WebKit") != NULL, "Browserspeicher gehört zum WebKit-Jailbreak");
        /* NEU 29.10.2026 19:4x (Recherche aller Jailbreak-Wege): weitere Einstiege */
        { const char *w1 = kern_jb_weg("/user/app/WKAL00001"), *w2 = kern_jb_weg("/user/appmeta/GGAU00001"), *w3 = kern_jb_weg("/user/home/u1/savedata/CUSA19555"),
                     *w4 = kern_jb_weg("/user/home/u1/savedata/CUSA07842"), *w5 = kern_jb_weg("/user/home/u1/savedata_prospero/PPSA06409"),
                     *w6 = kern_jb_weg("/user/home/u1/webkit/shell/appcache"), *w7 = kern_jb_weg("/user/app/PLDM00001"), *w8 = kern_jb_weg("/user/home/u1/webkit/shell/resourceloadstatistics");
          pruefe(w1 && !strcmp(w1, "WebKit-Autoloader") && w2 && !strcmp(w2, "WebKit-Autoloader"), "WebKit-Autoloader-Kacheln WKAL00001/GGAU00001 sind Jailbreak-Einstiege");
          pruefe(w3 && !strcmp(w3, "Lua-Jailbreak"), "Lua: CUSA19555 neu dabei");
          pruefe(w4 && !strcmp(w4, "PS2-Spielstand-Jailbreak"), "PS2: Jak X CUSA07842 neu dabei");
          pruefe(w5 && strstr(w5, "yarpe"), "Ren'Py-Jailbreak (yarpe) PPSA06409 neu dabei");
          pruefe(w6 && strstr(w6, "WebKit"), "Browser-AppCache (Offline-Jailbreak) gehört zum WebKit-Jailbreak");
          pruefe(w7 == NULL && w8 == NULL, "Payload Manager und Browser-Statistik bleiben normal (kein Einstieg)"); }
        const char *jw = NULL, *jb = kern_jailbreak_erkannt(K, A0, &jw);
        printf("INFO Jailbreak=%s | %s\n", jb ? jb : "-", jw ? jw : "-");
        pruefe(jb && strstr(jb, "Lua-Jailbreak") && strstr(jb, "PS2-Spielstand-Jailbreak") && !strstr(jb, "WebKit"), "Konsole-Karte: Lua- und PS2-Spielstand-Jailbreak erkannt, WebKit nur als Hinweis");
        pruefe(jw && strstr(jw, "(CUSA17122)") && strstr(jw, "Spielstand CUSA02199") && strstr(jw, "Browserspeicher"), "Warnung nennt Lua-Spiel, Okage-Spielstand und den Browserspeicher");
        pruefe(jw && !strstr(jw, "PPSA01616"), "Warnung nennt Netflix JP nicht (nicht eingerichtet)");
        /* Auswahl mit Jailbreak-Einstieg: deutliche Warnung, "Abbrechen" löscht nichts */
        if (luasd) {
            for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
            ablauf a; ablauf_ui ui = { .frage_wahl = fw2, .frage_text = ft }; ablauf_init(&a, K, &ui);
            titel = (sliste){0}; jbText = NULL;
            ablauf_eintrag_setzen(&a, luasd, true);
            ablauf_auswahl(&a);
            printf("INFO Fragen: %s\n", sl_join(A0, &titel, " / "));
            pruefe(h_hat(&titel, "Jailbreak") && jbText && strstr(jbText, "Lua-Jailbreak"), "Auswahl eines Jailbreak-Einstiegs: Warnung mit dem Namen des Jailbreaks");
            pruefe(!h_hat(&titel, "Endgültig löschen?"), "Warnung abgebrochen: keine Abschlussfrage");
            pruefe(da(VERB(A0, W, "/user/home/u1/savedata/CUSA17122/sdimg_SAVEDATA00")), "Warnung abgebrochen: Spielstand noch da");
            for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        }
    }

    /* Bedienablauf: Frage nach den Jailbreak-Einstiegen, "Behalten" und "Auch löschen" */
    for (int r = 0; r < 2; r++) {
        ablauf a; ablauf_ui ui = { .frage_wahl = fw, .frage_text = ft }; ablauf_init(&a, K, &ui);
        titel = (sliste){0}; jbAntwort = r == 1; jbGewaehlt = -1;
        ablauf_komplett(&a);
        printf("INFO Fragen: %s\n", sl_join(A0, &titel, " / "));
        /* 1.0.7 (da_Chris86 31.10.2026): Bereich "Jailbreak" gehört nie zur vollständigen Bereinigung - keine Frage, nichts davon gewählt */
        pruefe(!h_hat(&titel, "Jailbreak-Einstiege"), "vollständige Bereinigung fragt NICHT mehr nach den Jailbreak-Einstiegen (1.0.7)");
        pruefe(jbGewaehlt == 0, a_fmt(A0, "vor der Bestätigung kein Jailbreak-Eintrag gewählt (%d) - Antwort %s spielt keine Rolle", jbGewaehlt, r ? "auch löschen" : "behalten"));
        int rest = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Wahl) rest++;
        pruefe(rest == 0, "Abbruch bei der Bestätigung: danach nichts mehr gewählt");
    }
    /* Bereich 5 ganz wählen: Waise ja, Jailbreak-Daten nein */
    {
        ablauf a; ablauf_ui ui = {0}; ablauf_init(&a, K, &ui);
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        ablauf_bereich_setzen(&a, 5, true);
        if (w && !w->Gesperrt) pruefe(w->Wahl, "ganzen Bereich 5 wählen: Waise gewählt");
        pruefe(yt && !yt->Wahl, "ganzen Bereich 5 wählen: Jailbreak-Daten NICHT gewählt (nur einzeln)");
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
    }
    pruefe(kern_anzahl_jailbreak(K, true, true) >= (m2unklar || usbfehler ? 1 : 5), a_fmt(A0, "Jailbreak-Einstiege gezählt: %d", kern_anzahl_jailbreak(K, true, true)));
    kern_waehle_komplett(K, true, true);
    pruefe(yt && !yt->Wahl && yta && !yta->Wahl, "vollständige Bereinigung, \"behalten\": Jailbreak-Daten und YouTube-App NICHT gewählt");
    if (w && !w->Gesperrt) pruefe(w->Wahl, "vollständige Bereinigung: echte Waise gewählt");
    kern_jailbreak_dazu(K, true, true);
    pruefe(yt && (yt->Wahl || yt->Gesperrt) && yta && yta->Wahl, "vollständige Bereinigung, \"auch löschen\": Jailbreak-Einstiege gewählt (gesperrte nicht)");
    bool jbl = argc >= 4 && !strcmp(argv[3], "jbloeschen");
    kern_waehle_komplett(K, true, true);                    /* Test löscht mit "behalten" (Modus jbloeschen: "auch löschen") */
    if (jbl) kern_jailbreak_dazu(K, true, true);
    bool mitApp[16] = {0};
    for (int i = 0; i < NN; i++) { eintrag *f = mit_pfad(VERB(A0, "/user/download/", nie[i])); mitApp[i] = f && f->Wahl; }
    ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
    printf("ERG Fehler=%d\n", e.Fehler);
    pruefe(e.Fehler == 0, "vollständige Bereinigung ohne Fehler");
    for (int i = 0; i < NN; i++) {
        bool d = da(VERB(A0, W, "/user/download/", nie[i], "/download0.dat"));
        if (mitApp[i]) pruefe(!d, VERB(A0, "mit der App gelöscht: Datenspeicher ", nie[i]));
        else pruefe(d, VERB(A0, "danach noch da: Datenspeicher ", nie[i]));
    }
    if (jbl) {
        for (int i = 0; i < 3; i++) pruefe(!da(VERB(A0, W, "/user/download/", jbd[i])), VERB(A0, "\"auch löschen\": Jailbreak-Daten gelöscht ", jbd[i]));
        pruefe(!da(VERB(A0, W, "/user/app/PPSA01651")) && !da(VERB(A0, W, "/user/download/PPSA01651")), "\"auch löschen\": YouTube-App samt Daten gelöscht");
        pruefe(!da(VERB(A0, W, "/system_data/priv/appmeta/external/PPSA01614")), "\"auch löschen\": herausgenommener Einstieg aus dem Sammeleintrag gelöscht");
        if (!lesefehler) {
            pruefe(!da(VERB(A0, W, "/user/app/CUSA17122")) && !da(VERB(A0, W, "/user/home/u1/savedata/CUSA17122")), "\"auch löschen\": Lua-Spiel samt Spielstand gelöscht");
            pruefe(!da(VERB(A0, W, "/user/home/u1/savedata/CUSA02199")) && !da(VERB(A0, W, "/user/home/u1/webkit/shell/local/a")), "\"auch löschen\": Okage-Spielstand und Browserspeicher gelöscht");
        }
    } else {
        for (int i = 0; i < 3; i++) pruefe(da(VERB(A0, W, "/user/download/", jbd[i], "/download0.dat")), VERB(A0, "danach noch da: Jailbreak-Daten ", jbd[i]));
        pruefe(da(VERB(A0, W, "/user/app/PPSA01651")) && da(VERB(A0, W, "/user/download/PPSA01651/download0.dat")), "danach noch da: YouTube-App samt Daten");
        if (!lesefehler) {
            pruefe(da(VERB(A0, W, "/user/app/CUSA17122/eboot.bin")) && da(VERB(A0, W, "/user/home/u1/savedata/CUSA17122/sdimg_SAVEDATA00")), "danach noch da: Lua-Spiel samt Spielstand");
            pruefe(da(VERB(A0, W, "/user/home/u1/savedata/CUSA02199/sdimg_SAVEDATA00")) && da(VERB(A0, W, "/user/app/PPSA01616")), "danach noch da: Okage-Spielstand und Netflix JP");
            pruefe(da(VERB(A0, W, "/user/home/u1/webkit/shell/local/a")), "danach noch da: Browserspeicher (WebKit-Jailbreak)");
        }
    }
    pruefe(da(VERB(A0, W, "/user/download/NPXS40140/download0.dat")), "danach noch da: download0.dat des Blu-ray-Players");
    pruefe(!da(VERB(A0, W, "/user/download/PPSA70006")), "echte Waise PPSA70006 danach weg");
    if (!lesefehler) {
        pruefe(!da(VERB(A0, W, "/user/data/ps5_autoloader")), "vollständige Bereinigung: Jailbreak-Tool-Ordner ps5_autoloader weg (wie bisher)");
        pruefe(da(VERB(A0, W, "/user/home/u1/savedata/CUSA70010")), "vollständige Bereinigung: normaler Spielstand bleibt - am PC gibt es Sonys Spielstand-Liste nicht (1.0.7)");
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
