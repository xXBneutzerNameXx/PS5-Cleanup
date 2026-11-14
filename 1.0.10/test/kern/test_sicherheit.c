/* PS5 Cleanup - Sicherheitsprüfung (unabhängiger Prüfbericht 25.10.2026): Tests S2, S3, S5, U17
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_sicherheit s2|s3|s5|u17 <hier> [schutz]      Testbaum in <hier>/w (Kopie; bei s2/u17 wird gelöscht)
 */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void melde_log(void *c, const char *t) { (void)c; (void)t; }
static bool nein(void *c) { (void)c; return false; }
static int hook_n; static int hook_test(void) { hook_n++; return 2; }   /* NEU 1.0.10 (14.11.2026): Programme schließen (nur PS5-App) */
static bool da(const char *p) { struct stat st; return lstat(p, &st) == 0; }
static kern *K; static arena *A0; static const char *W;

static volatile int such_fehler;
static const char *such_text = "";
static void neu(bool schutz)
{
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    K = kern_neu(zugriff_datei(&o)); K->melde = melde_log;
    such_fehler = 0; such_text = "";
    VERSUCHE {
        if (schutz) { kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K); }
        kern_suche_funde(K);
    } FANGE { such_fehler = 1; such_text = a_kopie(A0, ausn_text); } ENDE_VERSUCHE;
}
/* steckt p (oder etwas darunter / darüber) in irgendeinem angebotenen Eintrag? */
static bool angeboten(const char *p)
{
    size_t l = strlen(p);
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i];
        for (int j = 0; j < f->Pfade.n; j++) { const char *q = f->Pfade.e[j]; size_t m = strlen(q);
            if (!strcmp(q, p) || (m < l && !strncmp(p, q, m) && p[m] == '/') || (l < m && !strncmp(q, p, l) && q[l] == '/')) return true; } }
    return false;
}
/* NEU 27.10.2026: steckt p in einem WÄHLBAREN (nicht gesperrten) Eintrag? Geschütztes steht seitdem sichtbar gesperrt da. */
/* 1.0.6 Fix: steht genau dieser Pfad (als Ganzes) in einem Eintrag? */
static bool genau_angeboten(const char *p)
{
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], p)) return true; }
    return false;
}
static bool angeboten_frei(const char *p)
{
    size_t l = strlen(p);
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Gesperrt) continue;
        for (int j = 0; j < f->Pfade.n; j++) { const char *q = f->Pfade.e[j]; size_t m = strlen(q);
            if (!strcmp(q, p) || (m < l && !strncmp(p, q, m) && p[m] == '/') || (l < m && !strncmp(q, p, l) && q[l] == '/')) return true; } }
    return false;
}
/* NEU 1.0.8 (Arbeitsplan 3.5): Meldungen sammeln; "warten" legt einen gelöschten Tool-Ordner wieder an (wie ein laufendes Tool) */
static sliste MELD; static const char *WIEDER = NULL;
static void melde_sammeln(void *c, const char *t) { (void)c; sl_add(A0, &MELD, a_kopie(A0, t ? t : "")); }
static void warte_und_anlegen(void *c, int ms) { (void)c; (void)ms; if (WIEDER) mkdir(WIEDER, 0755); }
static bool gemeldet(const char *was) { for (int i = 0; i < MELD.n; i++) if (strstr(MELD.e[i], was)) return true; return false; }
static bool geschuetzt(const char *p) { for (int i = 0; i < K->SCHUTZ.pfade.n; i++) if (!strcmp(K->SCHUTZ.pfade.e[i], p)) return true; return false; }
static const char *w(const char *p) { return VERB(A0, W, p); }
static void komplett(void)
{
    kern_waehle_komplett(K, true, true);
    ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
    printf("ERG Fehler=%d Abgebrochen=%s\n", e.Fehler, e.Abgebrochen ? "True" : "False");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: test_sicherheit s2|s3|s5|u17 <hier> [schutz]\n"); return 2; }
    const char *art = argv[1]; bool schutz = argc >= 4 && !strcmp(argv[3], "schutz");
    A0 = arena_neu(); W = VERB(A0, argv[2], "/w");
    printf("== %s %s\n", art, schutz ? "mit Selbstschutz" : "ohne Selbstschutz");
    if (!strcmp(art, "s2")) {                 /* PS5 Cleanup unter anderem Namen / an anderem Ort */
        neu(true);
        pruefe(!such_fehler, "Suche läuft");
        pruefe(geschuetzt("/user/data/pldmgr/local/ps5c/ps5c.elf"), "umbenannte Kopie im Payload-Manager-Ordner erkannt");
        pruefe(geschuetzt("/mnt/ext1/meinordner/ohne_endung"), "Kopie ohne Dateiendung auf der M.2 erkannt");
        pruefe(!geschuetzt("/user/data/payloads/fremd.elf"), "fremde ELF ohne Kennung nicht geschützt");
        pruefe(!geschuetzt("/user/data/notiz.txt"), "Textdatei, die die Kennung nur erwähnt, nicht geschützt");
        pruefe(K->SCHUTZ.kopien >= 3, a_fmt(A0, "Kopien gefunden: %d (homebrew/PS5Cleanup, pldmgr, M.2)", K->SCHUTZ.kopien));
        pruefe(!angeboten_frei("/user/data/pldmgr/local/ps5c/ps5c.elf") && !angeboten_frei("/mnt/ext1/meinordner/ohne_endung"), "kein wählbarer Eintrag enthält eine Kopie (Anforderungen 27.10.2026: sichtbar gesperrt erlaubt)");
        pruefe(angeboten("/user/data/payloads/fremd.elf"), "Ordner mit fremder ELF wird wie bisher angeboten");
        komplett();
        pruefe(da(w("/user/data/pldmgr/local/ps5c/ps5c.elf")) && da(w("/mnt/ext1/meinordner/ohne_endung")), "vollständige Bereinigung inkl. M.2: alle Kopien stehen noch");
        pruefe(!da(w("/user/data/payloads/fremd.elf")), "fremde ELF wurde wie bisher gelöscht");
    }
    else if (!strcmp(art, "s3")) {           /* Lesefehler: nie raten */
        neu(schutz);
        if (getenv("FAILLIST")) {
            printf("SUCHE %s\n", such_fehler ? such_text : "lief durch");
            pruefe(such_fehler && !K->FUNDE_GUELTIG, "Ordnerliste nicht lesbar: Suche bricht ab, keine Liste");
            /* 1.0.6: der Homebrew Launcher wird nicht mehr gesucht - den Lesefehler meldet jetzt die Suche selbst (Abbruch bleibt) */
            if (schutz) pruefe(such_text && (strstr(such_text, "Sicherheitsprüfung nicht möglich") || strstr(such_text, "nicht vollständig gelesen")), a_fmt(A0, "Grund wird genannt: %s", such_text ? such_text : "-"));
        } else {
            pruefe(!such_fehler, "Suche läuft");
            bool drin = false;
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->Pfade.n; j++) if (strstr(f->Pfade.e[j], "content_tmp/GESPERRT00")) drin = true; }
            if (schutz) pruefe(!drin, "nicht lesbarer Aufnahme-Zwischenordner wird nicht als leer angeboten");
            else printf("INFO ohne Selbstschutz (wie 1.0.5) angeboten: %s\n", drin ? "ja" : "nein");
        }
    }
    else if (!strcmp(art, "s5")) {           /* Systembereiche: nie */
        kern *k0 = kern_neu(NULL);
        const char *nie[] = {
            "/system/vsh/app/NPXS20001/eboot.bin", "/system_ex/app/FAKE00000", "/system_ex/app/NPXS40106/eboot.bin", "/preinst/x", "/update/PS5UPDATE.PUP", "/dev/ddd",
            "/system_data/settings", "/system_data/settings/system.dat", "/system_data/priv/mms/app.db", "/system_data/priv/license", "/system_data/priv/drm",
            "/system_data/priv/home/1a2b3c4d", "/system_data/priv/appmeta", "/system_data/priv/appmeta/NPXS40028", "/system_data/priv/updater", "/system_data/game",
            "/user/home", "/user/home/1a2b3c4d", "/user/home/1a2b3c4d/", "/user/home/1a2b3c4d/userinfo.dat", "/user/home/1a2b3c4d/savedata", "/user/home/1a2b3c4d/webkit",
            "/user/license", "/user/license/act.dat", "/user/license/nobackup", "/user/license/nobackup/x.rif", "/user/license/EP9000-PPSA01325_00-XXX.rif", "/user/license/ep9000.rif",
            "/user/system", "/user/system/share", "/user/system/share/trophy", "/user/system/share/share_settings.mst", "/user/system/notification", "/user/system/webkit",
            "/user/app", "/user/app/NPXS40140", "/user/appmeta/NPXS40028", "/user/patch/NPXS40001", "/user/addcont/NPXS40001", "/user/download/NPXS40001",
            "/user/priv", "/user/common", "/user/sshared", "/user/devlog", "/user/lost+found", "/user/playgo", "/user/savedata", "/user/trophy", "/user/trophy2", "/user/bgft", "/user/temp",
            "/user/data", "/user/av_contents", "/user/catalog_downloader",
            "/mnt/ext1", "/mnt/ext1/user", "/mnt/ext1/user/app", "/mnt/ext1/system_data", "/mnt/ext1/lost+found", "/mnt/ext0/system_data/x",
            "/mnt/usb0", "/mnt/shadowmnt/PPSA12345", "/mnt/shadowmnt/PPSA12345/eboot.bin", "/mnt/sandbox/x", "/mnt/pfs/x", "/mnt/disc/x",
            "/user/home/1a2b3c4d/savedata_prospero/sce_sdmemory", "/mnt/ext0", "/mnt/ext0/user", "/mnt/ext0/user/app", "/mnt/ext0/lost+found",
            "/data/homebrew", "/", "",
            /* NEU 27.10.2026 (Codex-Prüfung 7): Sony-Apps in Unterordnern der Kachel-Infos, Schrägstrich am Ende */
            "/user/appmeta/external/NPXS40140", "/user/appmeta/addcont/NPXS40140", "/user/appmeta/external/addcont/NPXS40140",
            "/user/appmeta/external/", "/user/appmeta/addcont/", "/user/data/etaHEN/", "/mnt/ext1/homebrew/",
            /* NEU 02.11.2026 (Prüfung p8, Befund 8): das Protokoll ohne Stick wird nie angeboten */
            "/user/data/PS5-Cleanup-Protokolle", "/user/data/PS5-Cleanup-Protokolle/", "/user/data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_2026-11-02_165700.txt",
            "/data/PS5-Cleanup-Protokolle", "/mnt/ext1/PS5-Cleanup-Protokolle", "/mnt/ext1/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_2026-11-02_172300.txt", NULL };
        int n = 0, frei = 0;
        for (int i = 0; nie[i]; i++) { n++; if (kern_erlaubt(k0, nie[i])) { frei++; printf("FAIL erlaubt: %s\n", nie[i]); } }
        pruefe(frei == 0, a_fmt(A0, "%d geschützte Pfade (System, Sony-Apps, Einstellungen, Konten, Aktivierung, echte Lizenzen, Wechselmedien): alle gesperrt", n));
        const char *ja[] = { "/user/app/PPSA12345", "/user/home/1a2b3c4d/savedata_prospero/PPSA12345", "/user/license/fakeEP0000-PPSA12345.rif",
            "/user/system/share/PPSA12345", "/system_data/priv/appmeta/PPSA12345", "/system_data/priv/error/history/1.json", "/user/data/etaHEN", "/mnt/ext1/homebrew",
            /* 1.0.6 (da_Chris86 28.10.2026): Astro wie jedes Spiel, USB wie M.2, USB-Stick (nur einzeln angeboten) */
            "/user/app/PPSA01325", "/user/home/1a2b3c4d/savedata_prospero/PPSA01325", "/mnt/ext0/user/app/PPSA12345", "/mnt/ext0/homebrew/x.exfat", "/mnt/usb0/websrv-ps5.elf",
            "/user/data/PS5-Cleanup-Protokolle2", "/mnt/ext1/PS5-Cleanup-Protokolle2", NULL };   /* 02.11.2026: nur genau die Protokoll-Ordner sind gesperrt */
        int m = 0, gesperrt = 0;
        for (int i = 0; ja[i]; i++) { m++; if (!kern_erlaubt(k0, ja[i])) { gesperrt++; printf("FAIL gesperrt: %s\n", ja[i]); } }
        pruefe(gesperrt == 0, a_fmt(A0, "%d Pfade sind erlaubt (1.0.5 + 1.0.6: Astro, USB, USB-Stick)", m));
    }
    else if (!strcmp(art, "baum")) {         /* NEU 27.10.2026: Geschütztes UNTER einem erlaubten Ordner - Ordner bleibt ganz */
        neu(true);
        pruefe(!such_fehler, "Suche läuft");
        eintrag *gs = NULL, *fr = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; f->Wahl = false;
            for (int j = 0; j < f->Pfade.n; j++) { if (!strcmp(f->Pfade.e[j], "/mnt/ext1/test/sicherung")) gs = f; if (!strcmp(f->Pfade.e[j], "/mnt/ext1/test/frei")) fr = f; } }
        pruefe(gs && fr && !gs->Gesperrt, "Testordner auf der M.2 angeboten");
        if (gs && fr) {
            gs->Wahl = true; fr->Wahl = true;
            ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
            printf("ERG Fehler=%d\n", e.Fehler);
            pruefe(e.Fehler >= 1, "Sicherheitsstopp gemeldet");
            struct stat st; bool rechte = lstat(w("/mnt/ext1/test/sicherung/NPXS40140/param.json"), &st) == 0 && (st.st_mode & 0777) == 0644
                && lstat(w("/mnt/ext1/test/sicherung"), &st) == 0 && (st.st_mode & 0777) == 0750;
            pruefe(da(w("/mnt/ext1/test/sicherung/NPXS40140/param.json")) && da(w("/mnt/ext1/test/sicherung/eigenes.txt")), "Ordner mit Sony-App-Daten darin: komplett stehen geblieben");
            pruefe(rechte, "auch keine Rechte geändert (kein CHMOD)");
            pruefe(!da(w("/mnt/ext1/test/frei")), "anderer Ordner normal gelöscht");
        }
    }
    else if (!strcmp(art, "c27")) {          /* NEU 1.0.6 (Codex-Prüfung 27) */
        neu(true);
        pruefe(!such_fehler, "Suche läuft trotz nicht lesbarem Ordner auf einem USB-Stick");
        eintrag *pl = NULL, *s0 = NULL, *el = NULL;
        const char *ZT = "/mnt/usb1/sehrtief"; for (int i = 0; i < 63; i++) ZT = a_fmt(A0, "%s/x", ZT);   /* NEU 1.0.10: die 64. Ebene */
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->Pfade.n; j++) {
            if (!strcmp(f->Pfade.e[j], "/mnt/ext1/payloads/liste.txt")) pl = f;
            if (!strcmp(f->Pfade.e[j], "/mnt/usb0/notiz.txt")) s0 = f;
            if (!strcmp(f->Pfade.e[j], "/mnt/usb0/ps5c.elf")) el = f; } }
        pruefe(pl && pl->Kat == 6 && pl->Ort == ORT_M2, "Homebrew-Ordner auf der M.2 (payloads): Bereich 6, Ort M.2");
        pruefe(s0 && !s0->Gesperrt && s0->NurEinzeln && s0->Ort == ORT_STICK, "lesbarer USB-Stick: Eintrag nur einzeln wählbar");
        /* 1.0.6 Fix (da_Chris86 30.10.2026, Playden): ein nicht lesbarer (privater) Ordner sperrt nicht mehr den ganzen Stick -
         * er wird still übersprungen und bleibt geschützt (auch der Stick nie als Ganzes), der Rest ist einzeln wählbar */
        pruefe(angeboten_frei("/mnt/usb1/datei.txt") && !angeboten("/mnt/usb1/kaputt") && !genau_angeboten("/mnt/usb1"),
               "USB-Stick mit privatem Ordner: übrige Dateien wählbar, privater Ordner still übersprungen, Stick nie als Ganzes");
        /* NEU 1.0.8 (Arbeitsplan 2.5): beide Arten werden gezählt und fürs Protokoll unterschieden; zu tief Verschachteltes bleibt geschützt */
        pruefe(h_hat(&K->SCHUTZ.unlesbar, "/mnt/usb1/kaputt") && !h_hat(&K->SCHUTZ.zu_tief, "/mnt/usb1/kaputt"), "privater Ordner: als nicht lesbar gezählt, nicht als zu tief");
        /* NEU 1.0.10 (PS5-Test 11.11.2026, Playden/ProsperoEden): was die Suche nach Kopien nicht prüft (zu tief), wird nicht mehr gesperrt */
        pruefe(!h_hat(&K->SCHUTZ.zu_tief, "/mnt/usb1/tief/a/b/c/d/e/f/g") && !geschuetzt("/mnt/usb1/tief/a/b/c/d/e/f/g") && angeboten_frei("/mnt/usb1/tief/a/b/c/d/e/f/g/h/t.txt"),
               "8 Ebenen tief (wie Playdens Windows-Ordner): nicht gesperrt, die Datei darin wählbar");
        pruefe(K->SCHUTZ.zu_tief.n == 0 && !geschuetzt(ZT) && angeboten_frei(ZT) && angeboten_frei(a_fmt(A0, "%s/x/x/t.txt", ZT)),
               a_fmt(A0, "66 Ebenen tief (über der Suchtiefe 64): nicht gesperrt, nicht als zu tief vermerkt, wählbar (%d zu tief)", K->SCHUTZ.zu_tief.n));
        pruefe(h_hat(&K->SCHUTZ.diag, "/mnt/usb1/kaputt") && K->SCHUTZ.diag.n == 1, a_fmt(A0, "Protokoll-Liste (diag): nur der private Ordner (%d)", K->SCHUTZ.diag.n));
        {   /* Codex-Prüfung 71: zweite Suche, der private Ordner ist inzwischen lesbar - die Protokoll-Liste zeigt ihn nicht mehr */
            chmod(w("/mnt/usb1/kaputt"), 0755);
            kern_selbstschutz_ermitteln(K);
            pruefe(!h_hat(&K->SCHUTZ.diag, "/mnt/usb1/kaputt") && K->SCHUTZ.zu_tief.n == 0 && K->SCHUTZ.diag.n == 0,   /* Codex-Prüfung 72: sonst wäre nicht zurückgesetzt */
                   a_fmt(A0, "zweite Suche: Protokoll-Liste neu (%d Ordner, davon %d zu tief), alter Fund nicht mehr drin", K->SCHUTZ.diag.n, K->SCHUTZ.zu_tief.n));
            chmod(w("/mnt/usb1/kaputt"), 0);
        }
        pruefe(el && el->Gesperrt && strstr(el->Text, "USB-Laufwerk, bleibt") != NULL, "Kopie von PS5 Cleanup auf dem USB-Stick: gesperrt mit eigenem Hinweis");
        if (el) printf("  Text: %s\n", el->Text);
        pruefe(!angeboten("/user/appmeta/push_resource") && !angeboten("/system_data/priv/appmeta/push_resource") && !angeboten("/user/download/push_resource"),
               "Sony-Ordner ohne Spielnummer (push_resource) wird nicht angeboten (Nutzer-Protokoll 28.10.2026)");
        pruefe(!angeboten("/user/app/backup") && !angeboten("/user/patch/backup") && !angeboten("/user/license/fakeBackup.rif"),
               "Ordner ohne Spielnummer in app/patch und Lizenz-Ordner werden nicht angeboten (Codex-Prüfung 31)");
        pruefe(!kern_erlaubt(K, "/user/appmeta/push_resource") && !kern_erlaubt(K, "/user/app/backup") && kern_erlaubt(K, "/user/appmeta/addcont/PPSA12345/x"),
               "Schutzliste: unter app/appmeta nur Spielnummern erlaubt");
        {   /* NEU 1.0.6 (PS5-Test 28.10.2026): ISO mit Spielnummer = Spiel (Bereich 1), ohne Spielnummer = Rest (Bereich 5) */
            eintrag *iso1 = NULL, *iso2 = NULL;
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->Pfade.n; j++) {
                if (!strcmp(f->Pfade.e[j], "/mnt/ext1/isos/DuckTales [BLES01969].iso")) iso1 = f;
                if (!strcmp(f->Pfade.e[j], "/mnt/ext1/isos/linux.iso")) iso2 = f; } }
            pruefe(iso1 && iso1->Kat == 1 && strstr(iso1->Text, "ISO") && iso2 && iso2->Kat == 5, "ISO mit Spielnummer in Bereich 1, Linux-ISO in Bereich 5");
        }
        pruefe(angeboten("/mnt/usb0/user") && !angeboten_frei("/mnt/usb0/user/k.elf"), "Kopie von PS5 Cleanup im Ordner user eines USB-Sticks erkannt (Codex-Prüfung 30)");
    }
    else if (!strcmp(art, "selbst")) {       /* NEU 1.0.6 (da_Chris86 28.10.2026): PS5 Cleanup entfernt sich am Ende selbst */
        neu(true);
        pruefe(!such_fehler, "Suche läuft");
        eintrag *ka = NULL; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], "/user/app/PSCL00001")) ka = f; }
        pruefe(!ka || ka->Gesperrt, "eigene Kachel PSCL00001 wird während des Laufs nicht zum Löschen angeboten");
        pruefe(kern_selbst_da(K, false, false), "eigene Dateien auf der internen SSD gefunden");
        int f1 = kern_selbst_entfernen(K, false, false);
        pruefe(f1 == 0 && !da(w("/user/data/pldmgr/local/ps5c/ps5c.elf")) && !da(w("/user/app/PSCL00001")), "intern: eigene Kopie und eigene Kachel entfernt");
        pruefe(da(w("/mnt/ext1/meinordner/ohne_endung")), "M.2 behalten: eigene Kopie auf der M.2 bleibt");
        pruefe(da(w("/user/data/homebrew")) && da(w("/user/app")), "nur Eigenes entfernt (Elternordner bleiben)");
        int f2 = kern_selbst_entfernen(K, true, false);
        pruefe(f2 == 0 && !da(w("/mnt/ext1/meinordner/ohne_endung")), "M.2 mit geleert: eigene Kopie auf der M.2 entfernt");
    }
    else if (!strcmp(art, "testordner")) {   /* NEU 1.0.8 (Codex-Prüfung 74): Ordner der Test-Fassung - nie angeboten, nie gelöscht, auch nicht von der Selbstentfernung */
        datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
        K = kern_neu(zugriff_datei(&o)); K->melde = melde_log;
        sl_add(A0, &K->TEST_ORDNER, "/user/data/PS5-Cleanup-Test"); sl_add(A0, &K->TEST_ORDNER, "/mnt/usb0/PS5-Cleanup-Test"); sl_add(A0, &K->TEST_ORDNER, "/mnt/usb1/PS5-Cleanup-Test");
        such_fehler = 0;
        VERSUCHE { kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K); kern_suche_funde(K); }
        FANGE { such_fehler = 1; such_text = a_kopie(A0, ausn_text); } ENDE_VERSUCHE;
        pruefe(!such_fehler, "Suche läuft");
        pruefe(!angeboten("/user/data/PS5-Cleanup-Test") && !angeboten("/mnt/usb0/PS5-Cleanup-Test"), "Ordner der Test-Fassung nicht angeboten (intern und USB-Laufwerk)");
        pruefe(!kern_erlaubt(K, "/user/data/PS5-Cleanup-Test") && !kern_erlaubt(K, "/user/data/PS5-Cleanup-Test/bild_0001.png") && !kern_erlaubt(K, "/mnt/usb0"),
               "nie löschbar: der Ordner, alles darin und ein Ordner darüber");
        pruefe(kern_erlaubt(K, "/user/data/irgendwas") && kern_erlaubt(K, "/user/data/PS5-Cleanup-Testx"), "andere Ordner wie bisher (auch mit ähnlichem Namen)");
        /* Codex-Prüfung 75: auf dem USB-Laufwerk (exFAT) ist groß/klein egal - "ps5-cleanup-test" ist derselbe Ordner */
        pruefe(!angeboten("/mnt/usb1/ps5-cleanup-test") && !kern_erlaubt(K, "/mnt/usb1/ps5-cleanup-test/bild_0002.png") && !kern_erlaubt(K, "/mnt/usb1/PS5-CLEANUP-TEST"),
               "auch in anderer Schreibweise (ps5-cleanup-test) nicht angeboten und nie löschbar");
        komplett();
        pruefe(da(w("/user/data/PS5-Cleanup-Test/mitschnitt.txt")) && da(w("/user/data/PS5-Cleanup-Test/ps5c.elf")) && da(w("/mnt/usb0/PS5-Cleanup-Test/bild_0001.png"))
               && da(w("/mnt/usb1/ps5-cleanup-test/bild_0002.png")),
               "vollständige Bereinigung: alles darin steht noch");
        int f1 = kern_selbst_entfernen(K, true, true);
        pruefe(da(w("/user/data/PS5-Cleanup-Test/ps5c.elf")) && !da(w("/user/data/pldmgr/local/ps5c/ps5c.elf")),
               a_fmt(A0, "Selbstentfernung: die Kopie im Test-Ordner bleibt, die andere eigene Kopie ist weg (%d Fehler gemeldet)", f1));
    }
    else if (!strcmp(art, "teilweise")) {    /* NEU 1.0.8 (Arbeitsplan 3.5): Freigeräumt bei teilweise gelöschtem Ordner, Hinweis "Tool läuft gerade" */
        neu(true);
        pruefe(!such_fehler, "Suche läuft");
        eintrag *z = NULL, *n = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->PfadeBasis.n == 1 && !strcmp(f->PfadeBasis.e[0], "/user/data/zftpd")) z = f; if (f->PfadeBasis.n == 1 && !strcmp(f->PfadeBasis.e[0], "/user/data/nanodns")) n = f; }
        pruefe(z && n && z->Kat == 6 && n->Kat == 6, "Daten-Ordner der Tools zftpd und nanoDNS gefunden (Apps)");
        if (z && n) {
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; f->Wahl = f == z || f == n; }
            kern_ermittle_groessen(K);                /* wie der Ablauf vor dem Löschen (groesse_ermitteln) */
            K->melde = melde_sammeln; K->warte = warte_und_anlegen; WIEDER = w("/user/data/nanodns"); hook_n = 0; K->nach_anhalten = hook_test;
            ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
            printf("ERG Fehler=%d Frei=%lld Byte (zftpd vorher %lld, nanodns %lld)\n", e.Fehler, e.Frei.Bytes, z->Bytes, n->Bytes);
            pruefe(e.Fehler == 1 && da(w("/user/data/zftpd/fest/x.bin")) && !da(w("/user/data/zftpd/a.bin")), "zftpd nur teilweise gelöscht (eine Datei ließ sich nicht löschen)");
            pruefe(e.Frei.Bytes == 1000 + n->Bytes, a_fmt(A0, "Freigeräumt zählt das wirklich Gelöschte mit (%lld = 1000 von zftpd + %lld von nanoDNS)", e.Frei.Bytes, n->Bytes));
            /* NEU 1.0.10 (da_Chris86 14.11.2026 11:00): mit nach_anhalten wird ShadowMount auch dann angehalten, wenn nichts von ShadowMount gewählt ist */
            pruefe(hook_n == 1 && !gemeldet("Halte ShadowMount an"), a_fmt(A0, "vor jedem Löschen einmal Programme schließen (%d-mal); ShadowMount bleibt, wenn nichts von ihm gewählt ist", hook_n));
            pruefe(e.SmText && strstr(e.SmText, "neu laden"), "Hinweis am Ende: beendete Programme neu laden");
            K->nach_anhalten = NULL;
            pruefe(gemeldet("    davon gelöscht: "), "Protokoll: wie viel vom teilweise gelöschten Ordner weg ist");
            pruefe(gemeldet("Läuft dieses Tool gerade?"), "Protokoll: Hinweis, dass das Tool laufen könnte");
            pruefe(gemeldet(" ist gleich wieder da - das Tool läuft vermutlich gerade"), "Protokoll: gelöschter Tool-Ordner gleich wieder da - Hinweis");
        }
    }
    else if (!strcmp(art, "u17")) {          /* Spiel auf der USB-Erweiterung gilt als installiert */
        neu(schutz);
        pruefe(!such_fehler, "Suche läuft");
        const char *teile[] = { "/user/appmeta/PPSA88888", "/system_data/priv/appmeta/PPSA88888", "/user/system/share/PPSA88888", "/user/license/fakeEP0000-PPSA88888.rif", NULL };
        int an = 0; for (int i = 0; teile[i]; i++) if (angeboten(teile[i])) { an++; printf("INFO angeboten: %s\n", teile[i]); }
        /* 1.0.6 (da_Chris86 28.10.2026): USB wie M.2 - das Spiel steht in Bereich 1 mit Ort USB, Kachel-Infos/Share/Lizenz gehen mit ihm */
        eintrag *g = NULL; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Ort == ORT_USB && ist_teil(f, "Spiel")) g = f; }
        printf("INFO %s\n", g ? g->Text : "-");
        pruefe(g && g->Kat == 1 && !strncmp(g->Text, "Erweiterter Speicher – ", strlen("Erweiterter Speicher – ")) && angeboten("/mnt/ext0/user/app/PPSA88888"), "USB-Spiel steht in Bereich 1, Ort USB vorne (1.0.6)");
        pruefe(an == 4, a_fmt(A0, "Kachel, Infos, Share-Einstellung und Lizenz gehören zum USB-Spiel (%d von 4)", an));
        if (schutz) {
            kern_waehle_komplett(K, true, false);
            ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
            printf("ERG Fehler=%d\n", e.Fehler);
            bool alle = da(w("/mnt/ext0/user/app/PPSA88888")); for (int i = 0; teile[i]; i++) if (!da(w(teile[i]))) alle = false;
            pruefe(alle, "vollständige Bereinigung \"USB behalten\": alles vom USB-Spiel steht noch");
            neu(schutz);
            komplett();
            bool weg = !da(w("/mnt/ext0/user/app/PPSA88888")); for (int i = 0; teile[i]; i++) if (da(w(teile[i]))) weg = false;
            pruefe(weg, "vollständige Bereinigung \"USB auch leeren\": das USB-Spiel samt Kachel-Infos, Share und Lizenz ist weg");
        }
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
