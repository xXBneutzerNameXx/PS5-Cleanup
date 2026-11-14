/* PS5 Cleanup - Test "PKG- und ELF-Dateien bei ihrem Spiel bzw. Programm" (PS5-Test 06.11.2026, Befunde 2, 12 und 14;
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
#include "../../app/spielstand_db.h"   /* NEU 1.0.10: kachel_db_austragen (Sonys Datenbank im Test) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void melde_log(void *c, const char *t) { (void)c; if (getenv("ZEIGE")) printf("LOG: %s\n", t); }
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

/* NEU 1.0.10: steht das Spiel (Kennung, Steam-App-Nummer) in Playdens Listen? */
static const char *lies_datei(const char *p)
{
    static char b[1 << 16]; FILE *f = fopen(VERB(A0, W, p), "rb"); if (!f) return NULL;
    size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0; return b;
}
/* NEU 1.0.10 (da_Chris86 12.11.2026 15:19): ps4mount nachgestellt - holt die Stopp-Datei ab und schreibt wie das echte (gasb07/ps4mount src/main.c) */
#include <pthread.h>
static volatile int p4m_art = 0;   /* 0 = läuft nicht, 1 = beendet sich, 2 = weigert sich (Spiel offen) */
static volatile int p4m_aus = 0;
static void *p4m_faden(void *x)
{
    (void)x;
    while (!p4m_aus) {
        const char *st = VERB(A0, W, "/user/data/ps4mount/stop");
        if (p4m_art && access(st, F_OK) == 0) {
            usleep(300000); unlink(st);
            FILE *f = fopen(VERB(A0, W, "/user/data/ps4mount/log.txt"), "a");
            if (f) { fputs(p4m_art == 1 ? "[notify] PS4Mount v1.2 stopped: the games are unmounted\n" : "[main] not stopped: Close Wolkenreise before stopping PS4Mount\n[notify] PS4Mount v1.2 not stopped: Close Wolkenreise before stopping PS4Mount\n", f); fclose(f); }
        }
        usleep(100000);
    }
    return NULL;
}
static bool pl_hat(const char *id, const char *app)
{
    const char *j = lies_datei("/user/data/playden/runtime-state/installs.json"); bool in = j && strstr(j, VERB(A0, "\"", id, "\""));
    const char *g = lies_datei("/user/data/playden-games/.playden-games");
    bool zeile = g && (!strncmp(g, VERB(A0, app, "\t"), strlen(app) + 1) || strstr(g, VERB(A0, "\n", app, "\t")));
    return in && zeile;
}
static bool pl_liste_gut(void)   /* nach dem Austragen: genau ein Eintrag und eine Reservierung weniger, alles andere Byte für Byte gleich */
{
    static const char soll[] = "{\"schema_version\":1,\"next_runtime_number\":3000000,\"reservations\":[{\"install_id\":\"steam-223850\",\"title_id\":\"FAKE78001\"},{\"install_id\":\"steam-1010\",\"title_id\":\"FAKE78010\"},{\"install_id\":\"steam-1012\",\"title_id\":\"FAKE78012\"},{\"install_id\":\"steam-1013\",\"title_id\":\"FAKE78013\"}],\"installs\":["
        "{\"install_id\":\"steam-223850\",\"title_id\":\"FAKE78001\",\"app_id\":223850,\"install_path\":\"/data/playden-games/223850\",\"data_root\":\"/data/playden/games/steam-223850\",\"state\":\"ready\",\"last_failure\":null,\"journal\":null,\"size\":12345678901234567890},"
        "{\"title_id\":\"FAKE78003\"";
    const char *j = lies_datei("/user/data/playden/runtime-state/installs.json");
    if (!j || strncmp(j, soll, strlen(soll)) != 0) { printf("  installs.json danach: %.400s\n", j ? j : "(fehlt)"); return false; }
    const char *g = lies_datei("/user/data/playden-games/.playden-games");
    if (!g || strcmp(g, "223850\tinternal\t10224767053\n1010\tinternal\t5000\n1011\tinternal\t0\n1012\tinternal\t5000\n1013\tinternal\t5000\n1013\tinternal\t5000\n") != 0) { printf("  .playden-games danach: %s\n", g ? g : "(fehlt)"); return false; }
    return true;
}
/* NEU 1.0.10: steht das Spiel noch irgendwo in Playdens Listen (Eintrag, Reservierung bzw. Zeile mit seinem Schlüssel)? */
static bool pl_irgendwo(const char *id, const char *app)
{
    const char *j = lies_datei("/user/data/playden/runtime-state/installs.json"), *g = lies_datei("/user/data/playden-games/.playden-games");
    return (j && strstr(j, VERB(A0, "\"", id, "\""))) || (g && (!strncmp(g, VERB(A0, app, "\t"), strlen(app) + 1) || strstr(g, VERB(A0, "\n", app, "\t"))));
}
/* NEU 1.0.10 (20:06): Sonys Titel-Liste im Test - nur die Kacheln ohne Dateien (sonst liest der Test keine app.db) */
static void test_appinfo(kern *k, void (*zeile)(kern *k, const char *titel, const char *feld, const char *wert))
{
    zeile(k, "VERW71060", "APPDB", "u=1;l=2;t=0;h=0;x=0;g=0;c=cid:local:VERW71060;n=Verwaiste Kachel");
    zeile(k, "VERW71061", "APPDB", "u=1;l=2;t=0;h=0;x=0;g=0;c=cid:local:VERW71061;n=Mit Einhaengeordner");
    zeile(k, "VERW71062", "APPDB", "u=1;l=2;t=0;h=0;x=0;g=0;c=cid:local:VERW71062;n=Normal installiert");
    zeile(k, "VERW71063", "APPDB", "u=0;l=2;t=0;h=0;x=0;g=0;c=cid:local:VERW71063;n=System");
    zeile(k, "KOPI00001", "APPDB", "u=1;l=2;t=0;h=0;x=0;g=0;c=cid:local:KOPI00001;n=Kopie");   /* steht auch in der Test-Datenbank (kachel_db_baum.py) */
}
static int test_sony(const char *id) { return !strncmp(id, "VERW", 4) || !strcmp(id, "FAKE78012") || !strncmp(id, "KOPI0000", 8) ? 1 : !strncmp(id, "FAKE78", 6) ? 0 : -1; }   /* Playden-Spiele: nach dem Löschen nicht mehr bei Sony */
/* NEU 1.0.10 (21:56): Sonys Datenbank im Test - die echte Bereinigung (app/spielstand_db.c) auf einer nachgebauten app.db/appinfo.db */
static const char *KZ_ORDNER = NULL;
static antwort test_kachel_db(arena *a, const char *id, bool np)
{
    char g[1200] = "", v[300] = ""; int z = 0;
    int r = KZ_ORDNER ? kachel_db_austragen(KZ_ORDNER, id, np, &z, g, sizeof g, v, sizeof v) : -2;
    antwort x = { r == 0 ? 200 : 550, a_fmt(a, "%d %s", r == 0 ? 200 : 550, g) };
    return x;
}
static eintrag *nur_liste(const char *id) { for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->PlNurListe && !strcmp(f->PlNurListe, id)) return f; } return NULL; }
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
    if (lauf == 0) { K->appinfo_lesen = test_appinfo; kern_ps4_sony_eingetragen = test_sony; zugriff_kachel_db = test_kachel_db; KZ_ORDNER = a_fmt(A0, "%s/kz", argv[1]); }   /* NEU 1.0.10: Kachel ohne Dateien */
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
    {   /* 1.0.10 (PS5-Test da_Chris86 12.11.2026, Bild von 1.0.9): ps4mounts Notiz origin.txt geht mit ihrer PKG-Datei, ohne PKG ein Rest */
        eintrag *bp = mit_pfad("/mnt/usb0/ps4/CUSA40021/app.pkg"), *bn = mit_pfad("/mnt/usb0/ps4/CUSA40021/origin.txt"), *bw = mit_pfad("/mnt/usb0/ps4/CUSA40022/origin.txt");
        pruefe(bp && bp == bn && bp->PfadeBasis.n == 2 && bp->Art == ART_PAKET_DATEI, a_fmt(A0, "ps4mount-Notiz im Eintrag ihrer PKG-Datei (%s)", bp ? bp->Text : "-"));
        pruefe(!mit_pfad("/mnt/usb0/ps4/CUSA40022") && !mit_pfad("/mnt/usb1/ps4/CUSA40023"), "Notiz ohne PKG: nur die Datei, nie der ganze Ordner (Codex-Prüfung 168)");
        pruefe(bw && bw != bp && bw->PfadeBasis.n == 1 && bw->Art == ART_REST && strstr(bw->Text, "Notiz von ps4mount ohne PKG-Datei"),
               a_fmt(A0, "ps4mount-Notiz ohne PKG-Datei: ein Rest (%s, Art %d)", bw ? bw->Text : "-", bw ? bw->Art : -1));
        eintrag *oz = mit_pfad("/mnt/usb0/ps4/CUSA40024/origin.txt"), *fp = mit_pfad("/mnt/usb0/ps4/CUSA40024/app.pkg"), *eb = mit_pfad("/mnt/usb1/ps4/CUSA40023/origin.txt");
        pruefe(oz && fp && oz != fp && fp->PfadeBasis.n == 1 && !fp->PkgNotiz && oz->Art != ART_REST && strstr(oz->Text, "ps4  (ohne PKG-Dateien)") && oz->PfadeBasis.n == 1,
               a_fmt(A0, "Ordner \"origin.txt\" ist keine Notiz: bleibt im Rest, nicht bei der PKG-Datei (%s, Art %d)", oz ? oz->Text : "-", oz ? oz->Art : -1));
        pruefe(eb && eb->PfadeBasis.n == 1 && eb->Art == ART_REST && strstr(eb->Text, "Notiz von ps4mount ohne PKG-Datei"),
               a_fmt(A0, "Bibliothek nur mit einer Notiz ohne PKG: ein Rest (%s)", eb ? eb->Text : "-"));
        pruefe(bp && bp->PkgNotiz && !strcmp(bp->PkgNotiz, "/mnt/usb0/ps4/CUSA40021/origin.txt"), "PKG-Eintrag kennt seine Notiz (PkgNotiz)");
        eintrag *mp = mit_pfad("/mnt/ext1/ps4/CUSA40025/app.pkg"), *mn = mit_pfad("/mnt/ext1/ps4/CUSA40025/origin.txt"), *mw = mit_pfad("/mnt/ext1/ps4/CUSA40026/origin.txt");
        pruefe(mp && mp == mn && mp->PkgNotiz, a_fmt(A0, "M.2: Notiz im Eintrag ihrer PKG-Datei (Codex-Prüfung 167) (%s)", mp ? mp->Text : "-"));
        pruefe(mw && mw->Art == ART_REST && strstr(mw->Text, "Notiz von ps4mount ohne PKG-Datei"), a_fmt(A0, "M.2: Notiz ohne PKG ein Rest (%s)", mw ? mw->Text : "-"));
    }
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
    {   /* NEU 1.0.10 (PS5-Test 11.11.2026, Playden): 9 Ebenen werden geprüft (Grenze 64 wie die Löschprüfung) - beide PKG-Dateien stehen bei
         * ihrem Spiel. Erst unter 64 Ebenen bleibt ein Ordner ungeprüft: EIN Eintrag, rot, nie "(ohne PKG-Dateien)" (Codex-Prüfung 83) */
        eintrag *fl = mit_pfad("/mnt/usb0/Tief/1/Flach - CUSA40008.pkg"), *gr = mit_pfad("/mnt/usb0/Tief/1/2/3/4/5/6/7/8/9/Grund - CUSA40007.pkg");
        pruefe(fl && fl->PkgArt && gr && gr->PkgArt, a_fmt(A0, "9 Ebenen tief: beide PKG-Dateien gefunden und bei ihrem Spiel (%s | %s)", z(fl), z(gr)));
        eintrag *gt = mit_pfad("/mnt/usb0/Ganztief"); const char *ohne = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f == gt) continue;
            for (int j = 0; j < f->PfadeBasis.n; j++) if (!strncmp(f->PfadeBasis.e[j], "/mnt/usb0/Ganztief/", 19)) ohne = f->PfadeBasis.e[j]; }
        pruefe(gt && gt->PkgOffen && gt->PfadeBasis.n == 1 && !strcmp(gt->PfadeBasis.e[0], "/mnt/usb0/Ganztief") && !ohne,
               a_fmt(A0, "tiefer als 64 Ebenen: der Ordner bleibt EIN Eintrag, rot (%s%s%s)", z(gt), ohne ? " | einzeln: " : "", ohne ? ohne : ""));
    }
    eintrag *tf = mit_pfad("/mnt/usb0/Viele");
    pruefe(tf && !tf->PkgArt && !mit_pfad("/mnt/usb0/Viele/Paket - CUSA40011.pkg") && tf->PkgOffen && tf->Rot && kern_ampel(tf) == AMPEL_ROT
           && strstr(kern_art_info(A0, tf), "nicht nach PKG-Dateien suchen") && strstr(kern_folge(tf), "kann PKG-Dateien enthalten"),
           a_fmt(A0, "Ordner mit über 10000 Dateien (PKG-Suche nicht möglich): EIN Eintrag, rot, nur einzeln, mit Grund (Codex-Prüfung 90) (%s)", z(tf)));
    {   /* Gegenprobe: ein geprüfter Ordner ohne PKG-Datei bleibt, wie er war */
        int offen = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->PkgOffen) offen++;
        pruefe(offen == 2, a_fmt(A0, "nur die ungeprüften Ordner sind so markiert (Viele, Ganztief: %d)", offen));
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
    {   /* NEU 1.0.10 (da_Chris86 12.11.2026, Durchsicht seiner PS5): Web-Zwischenspeicher und ps4mount-Daten je Kennung, Sicherungen eines Tools
         * neben Sonys App-Datenbank */
        eintrag *vk = kachel("VOLL71034"), *vw = mit_pfad("/system_data/httpcache/nobackup/VOLL71034"), *vs = mit_pfad("/system_data/ps4mount/stage/VOLL71034");
        pruefe(vk && vw && vs && vw != vs && vw->ToolKachel == vk && vs->ToolKachel == vk && vw->Kat == vk->Kat && strstr(z(vw), "Web-Zwischenspeicher") && strstr(z(vs), "ps4mount-Daten")
               && !strstr(z(vw), "nicht mehr installiert") && vw->Art == ART_REST && vs->Art == ART_TOOL_DATEN,
               a_fmt(A0, "Web-Zwischenspeicher und ps4mount-Daten einer installierten App: zwei Einträge unter ihrer Kachel, der Zwischenspeicher grün (da_Chris86 12.11.2026 06:48) (%s [%s] | %s [%s])",
                     z(vw), kern_folge_kurz(vw), z(vs), kern_folge_kurz(vs)));
        eintrag *yt = kachel("PPSA01650"), *yw = mit_pfad("/system_data/httpcache/nobackup/PPSA01650");
        pruefe(yt && yw && yw->ToolKachel == yt && yw->Art == ART_REST, a_fmt(A0, "Zwischenspeicher eines Jailbreak-Einstiegs (YouTube, Y2JB): bei YouTube, nicht unter Sonstiges (%s)", z(yw)));
        {   /* da_Chris86 12.11.2026 06:44-06:59: Playden und seine Spiele */
            eintrag *pk = kachel("FAKE77031"), *k1 = kachel("FAKE78001"), *k2 = kachel("FAKE78002");
            eintrag *s1 = mit_pfad("/user/data/playden-games/223850"), *d1 = mit_pfad("/user/data/playden/games/steam-223850");
            eintrag *s2 = mit_pfad("/mnt/ext1/playden/games/22380"), *d2 = mit_pfad("/user/data/playden/games/steam-22380");
            eintrag *rest = mit_pfad("/user/data/playden-games/999");
            pruefe(pk && k1 && k2 && kern_kopf_von(K, k1, k1->Kat) == pk && kern_kopf_von(K, k2, k2->Kat) == pk,
                   a_fmt(A0, "die Kacheln der Playden-Spiele stehen im Block von Playden (%s | %s)", z(k1), z(k2)));
            /* NEU 1.0.10 (da_Chris86 12.11.2026 15:25): die Spielstände stehen im Bereich Spielstände, nicht im Block */
            pruefe(s1 && s1->ToolKachel == k1 && strstr(z(s1), "3DMark – Spieldateien") && s1->PfadeBasis.n == 1 && s1->PlSpiel && !strcmp(s1->PlSpiel, "FAKE78001")
                   && d1 && d1->Kat == 2 && !d1->ToolKachel && d1->PlSpst && !strcmp(d1->PlSpst, "FAKE78001") && d1 != s1 && strstr(z(d1), "3DMark – Spielstände und Einstellungen") && d1->Art == ART_TOOL_DATEN_WICHTIG && d1->Rot,
                   a_fmt(A0, "3DMark: Spieldateien unter seiner Kachel, Spielstände (rot) im Bereich Spielstände (%s [%s] | %s [%s])", z(s1), s1 ? kern_folge_kurz(s1) : "-", z(d1), d1 ? kern_folge_kurz(d1) : "-"));
            pruefe(s2 && s2->ToolKachel == k2 && d2 && d2->Kat == 2 && !d2->ToolKachel && k1->PlSpiel && k2->PlSpiel && !strcmp(k2->PlSpiel, "FAKE78002"),
                   a_fmt(A0, "Fallout: Spieldateien auf der M.2 unter seiner Kachel, Spielstände im Bereich Spielstände (%s | %s)", z(s2), z(d2)));
            {   eintrag *sv = mit_pfad("/user/data/playden/games/steam-223850.saves"), *kp = mit_pfad("/user/data/playden/games/steam-400.kept");
                pruefe(sv && sv->Kat == 2 && sv->PlSpst && strstr(z(sv), "3DMark – Sicherung der Spielstände") && kp && kp->Kat == 2 && kp->PlSpst && !kp->PlSpst[0]
                       && strstr(z(kp), "Portal – Spielstände und Einstellungen (Playden, Spiel nicht mehr installiert)") && kp->Rot,
                       a_fmt(A0, "Sicherung der Spielstände und aufgehobene Spielstände eines deinstallierten Spiels: im Bereich Spielstände (%s | %s)", z(sv), z(kp))); }
            pruefe(rest && rest->ToolKachel == pk && !strstr(z(rest), "Spieldateien (Playden)"), a_fmt(A0, "ein Ordner, den die Liste keinem Playden-Spiel zuordnet, bleibt bei Playden (%s)", z(rest)));
            {   /* Codex-Prüfung 160 */
                eintrag *g7 = mit_pfad("/user/data/playden-games/777"), *g6 = mit_pfad("/user/data/playden/games/steam-6");
                pruefe(g7 && g7->ToolKachel == pk && g6 && g6->Kat == 2 && g6->PlSpst && !strcmp(g6->PlSpst, "FAKE78006"),
                       a_fmt(A0, "derselbe Ordner bei zwei Spielen: bleibt bei Playden, die eigenen Spielstände je Spiel (Codex-Prüfung 160 (1)) (%s | %s)", z(g7), z(g6)));
                eintrag *m8 = mit_pfad("/user/data/playden/games/steam-888/save.dat"), *c8 = mit_pfad("/user/data/playden/games/steam-888/ps5c.elf");
                pruefe(m8 && m8->Kat == 2 && m8->PlSpst && c8 && c8->Gesperrt,
                       a_fmt(A0, "Spielstände mit Kopie von PS5 Cleanup: der freie Teil bleibt beim Spiel, die Kopie gesperrt (Codex-Prüfung 160 (3)) (%s | %s)", z(m8), z(c8)));
                eintrag *o5 = mit_pfad("/user/data/playden-games/555"), *o5d = mit_pfad("/user/data/playden/games/steam-555");
                pruefe((!o5 || o5->ToolKachel == pk) && o5d && o5d->Kat == 2 && o5d->PlSpst && !o5d->PlSpst[0] && (o5 || mit_pfad("/user/data/playden-games")) ,   /* Codex-Prüfung 163 (9): seine Spielstände unter Spielstände */
                       a_fmt(A0, "Spiel in der Liste ohne Kachel: seine Ordner bleiben bei Playden (Codex-Prüfung 160 (4)) (%s | %s)", z(o5), z(o5d)));
                eintrag *st1 = mit_pfad("/user/data/homebrew/FAKE78001");
                pruefe(st1 && !st1->Rot,   /* hier keine ShadowMount-Spieldatei - gelb/rot nach Beleg prüft test_sm (Modus playden) */
                       a_fmt(A0, "Starter eines Playden-Spiels: gelb, nicht rot (da_Chris86 12.11.2026 07:49) (%s [%s])", z(st1), st1 ? kern_folge_kurz(st1) : "-"));
            }
            bool unsinn = false;
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; const char *t = f->TextBasis ? f->TextBasis : "";
                if (strstr(t, "FAKE78003") || strstr(t, "FAKE78004") || (strstr(t, "(Playden)") && !f->PlSpst && !(f->ToolKachel && f->ToolKachel->KachelId && !strncmp(f->ToolKachel->KachelId, "FAKE78", 6)))) unsinn = true; }
            pruefe(!unsinn && !mit_pfad("/user/data/homebrew") && !mit_pfad("/user/data/pldmgr/payloads"),
                   "Pfade außerhalb von Playdens Spiele-Ordnern und fremde Kennungen in der Liste: nicht beachtet");
            /* der ganze Block Playden: alle Zeilen; die Kachel eines Spiels: nur das Spiel */
            kblock *b = NULL, *bp = NULL; int nb = pk ? kern_bloecke(K, A0, pk->Kat, SORT_AZ, &b) : 0;
            for (int x = 0; x < nb; x++) if (b[x].kopf == pk) bp = &b[x];
            bool alle = bp != NULL; const eintrag *soll[] = { k1, k2, s1, s2, rest };
            for (int t = 0; bp && t < bp->teile.n; t++) if (bp->teile.e[t] == d1 || bp->teile.e[t] == d2) alle = false;   /* Spielstände nicht im Block */
            for (int q = 0; bp && q < 5; q++) { bool drin = false; for (int t = 0; t < bp->teile.n; t++) if (bp->teile.e[t] == soll[q]) drin = true; if (!drin) alle = false; }
            pruefe(alle, a_fmt(A0, "Block Playden enthält beide Spiele mit Spieldateien, ohne Spielstände (%d Zeilen)", bp ? bp->teile.n : 0));
            if (pk && k1 && k2 && s1 && s2 && d1 && d2 && bp) {
                /* NEU 1.0.10 (da_Chris86 12.11.2026 15:12): Playden läuft und lässt sich nicht beenden - alles von Playden bleibt diesmal */
                unlink("playden_web.zu");
                setenv("PLAYDEN_WEB", "bleibt", 1);
                for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                k2->Wahl = true; kern_abhaengige_mitnehmen(K);
                ergebnis r = kern_fuehre_loeschung(K, ja, NULL);
                pruefe(r.Unveraendert >= 2 && da("/user/app/FAKE78002") && da("/mnt/ext1/playden/games/22380/FalloutNV.exe") && pl_hat("FAKE78002", "22380"),
                       a_fmt(A0, "Playden lässt sich nicht beenden: Fallout bleibt ganz, auch in Playdens Listen (unverändert %d)", r.Unveraendert));
                /* nur die Kachel gewählt: das ganze Spiel geht mit (wie Playdens Uninstall) - Playden läuft, PS5 Cleanup beendet es vorher */
                setenv("PLAYDEN_WEB", "laeuft", 1);
                for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                k2->Wahl = true; kern_abhaengige_mitnehmen(K);
                pruefe(s2->Wahl && !d2->Wahl, "nur die Kachel gewählt: die Spieldateien gehen mit, die Spielstände nicht");
                r = kern_fuehre_loeschung(K, ja, NULL);
                setenv("PLAYDEN_WEB", "zu", 1);
                pruefe(r.Fehler == 0 && r.Teilweise == 0 && access("playden_web.zu", F_OK) == 0 && !da("/mnt/ext1/playden/games/22380") && da("/user/data/playden/games/steam-22380/save.dat") && !da("/user/app/FAKE78002")
                       && da("/user/data/playden-games/223850/3DMark.exe") && da("/user/data/playden/games/steam-223850/save.dat") && da("/user/app/FAKE78001")
                       && da("/user/data/playden/kits/playden-kit/wine.bin") && da("/user/app/FAKE77031"),
                       a_fmt(A0, "nur Fallout gewählt: Playden beendet, Fallout weg, seine Spielstände bleiben, 3DMark und Playden bleiben (Fehler %d, teilweise %d)", r.Fehler, r.Teilweise));
                pruefe(!pl_hat("FAKE78002", "22380") && pl_hat("FAKE78001", "223850") && pl_liste_gut(),
                       "Fallout aus Playdens Listen ausgetragen (Eintrag, Reservierung, Zeile der Spieleliste), 3DMark und alles andere unverändert");
                /* Codex-Prüfung 163 (4): läuft 3DMark gerade, bleiben Spieldateien und Spielstände - auch wenn nur die Spielstände gewählt sind */
                setenv("LAEUFT", "FAKE78001", 1);
                for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                d1->Wahl = true; s1->Wahl = true; kern_abhaengige_mitnehmen(K);
                r = kern_fuehre_loeschung(K, ja, NULL);
                unsetenv("LAEUFT");
                pruefe(da("/user/data/playden/games/steam-223850/save.dat") && da("/user/data/playden-games/223850/3DMark.exe") && da("/user/app/FAKE78001") && pl_hat("FAKE78001", "223850") && r.Unveraendert >= 2,
                       a_fmt(A0, "das Spiel läuft gerade: Spielstände, Spieldateien, Kachel und Playdens Listen bleiben (unverändert %d)", r.Unveraendert));
                /* da_Chris86 12.11.2026 16:00: läuft das Spiel gerade (Playden ist dann zu), lehnt Sony die Kachel ab - dann bleibt alles */
                setenv("UNINSTALL", "ok", 1); setenv("UNINSTALL_NIE", "FAKE78001", 1);
                for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                k1->Wahl = true; kern_abhaengige_mitnehmen(K);
                r = kern_fuehre_loeschung(K, ja, NULL);
                unsetenv("UNINSTALL"); unsetenv("UNINSTALL_NIE");
                pruefe(da("/user/app/FAKE78001") && da("/user/data/playden-games/223850/3DMark.exe") && pl_hat("FAKE78001", "223850") && r.Unveraendert >= 1,
                       a_fmt(A0, "Sony lehnt die Kachel ab (z. B. das Spiel läuft): Kachel, Spieldateien und Playdens Listen bleiben (unverändert %d, Fehler %d)", r.Unveraendert, r.Fehler));
                /* NEU 1.0.10 (da_Chris86 12.11.2026 20:06, PS5-Test 19:49): ein Spiel, das nur noch in Playdens Listen steht - angeboten (ohne Datei,
                 * in Playdens Block, grün), gelöscht wird nur der Eintrag; arbeitet Playden gerade daran, wird es nicht angeboten */
                eintrag *nl = nur_liste("FAKE78010");
                pruefe(nl && nl->Pfade.n == 0 && kern_kopf_von(K, nl, nl->Kat) == pk && nl->Art == ART_REST && !nl->Rot && strstr(z(nl), "Nur noch in Playdens Liste") && !nur_liste("FAKE78011"),
                       a_fmt(A0, "nur noch in Playdens Liste: angeboten ohne Datei, in Playdens Block, grün - nicht, wenn Playden gerade daran arbeitet (%s [%s])", z(nl), nl ? kern_folge_kurz(nl) : "-"));
                if (nl) {
                    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                    nl->Wahl = true; kern_abhaengige_mitnehmen(K);
                    r = kern_fuehre_loeschung(K, ja, NULL);
                    pruefe(r.Fehler == 0 && r.Teilweise == 0 && !pl_irgendwo("FAKE78010", "1010") && pl_irgendwo("FAKE78011", "1011") && pl_hat("FAKE78001", "223850")
                           && da("/user/app/FAKE78001") && da("/user/data/playden-games/223850/3DMark.exe") && da("/user/app/FAKE77031"),
                           a_fmt(A0, "nur noch in Playdens Liste: ausgetragen (Eintrag, Reservierung, Zeile), alles andere bleibt (Fehler %d, teilweise %d)", r.Fehler, r.Teilweise));
                }
                /* NEU 1.0.10 (20:06): lässt sich Playdens Liste vor dem Löschen nicht als änderbar prüfen (hier: installs.json hat zwei Namen -
                 * so ersetzt PS5 Cleanup sie nie), bleibt das ganze Spiel unverändert - keine halbe Deinstallation (19:49: Kachel und Dateien weg,
                 * Playden zeigte das Spiel weiter) */
                {
                    const char *ij = VERB(A0, W, "/user/data/playden/runtime-state/installs.json"), *hart = VERB(A0, W, "/user/data/playden/runtime-state/zweiter-name");
                    pruefe(link(ij, hart) == 0, "Vorbereitung: zweiter Name für installs.json");
                    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                    k1->Wahl = true; kern_abhaengige_mitnehmen(K);
                    r = kern_fuehre_loeschung(K, ja, NULL);
                    unlink(hart);
                    pruefe(da("/user/app/FAKE78001") && da("/user/data/playden-games/223850/3DMark.exe") && da("/user/data/homebrew/FAKE78001") && pl_hat("FAKE78001", "223850")
                           && r.Teilweise == 0 && r.Fehler == 0 && r.Unveraendert >= 2,
                           a_fmt(A0, "Playdens Liste vorher nicht änderbar: 3DMark bleibt ganz - Kachel, Starter, Spieldateien, Listen (unverändert %d, teilweise %d, Fehler %d)", r.Unveraendert, r.Teilweise, r.Fehler));
                }
                {   /* NEU 1.0.10 (da_Chris86 22:40, Fallout-Test 19:49): Rechte-Bits sehen gut aus, das Schreiben selbst geht aber nicht - bis hier
                     * zeigte das erst der Versuch NACH dem Löschen (10,3 GB weg, Playden zeigte das Spiel weiter). Jetzt schreibt die Vorprüfung
                     * die Liste mit gleichem Inhalt neu - scheitert das, bleibt das ganze Spiel */
                    setenv("PL_KEIN_SCHREIBEN", "1", 1);
                    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                    k1->Wahl = true; kern_abhaengige_mitnehmen(K);
                    r = kern_fuehre_loeschung(K, ja, NULL);
                    unsetenv("PL_KEIN_SCHREIBEN");
                    pruefe(da("/user/app/FAKE78001") && da("/user/data/playden-games/223850/3DMark.exe") && da("/user/data/homebrew/FAKE78001") && pl_hat("FAKE78001", "223850")
                           && r.Teilweise == 0 && r.Fehler == 0 && r.Unveraendert >= 2,
                           a_fmt(A0, "Schreiben in Playdens Liste geht nicht (Rechte sehen gut aus): 3DMark bleibt ganz, nichts gelöscht (unverändert %d, teilweise %d, Fehler %d)", r.Unveraendert, r.Teilweise, r.Fehler));
                }
                {   /* Codex-Prüfung 169 (2): Sony führt die Kachel noch - der Listeneintrag bleibt, unverändert (nicht "teilweise") */
                    eintrag *n12 = nur_liste("FAKE78012");
                    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                    if (n12) n12->Wahl = true;
                    r = kern_fuehre_loeschung(K, ja, NULL);
                    pruefe(n12 && pl_hat("FAKE78012", "1012") && r.Unveraendert == 1 && r.Teilweise == 0 && r.Fehler == 0,
                           a_fmt(A0, "nur noch in Playdens Liste, Sony führt die Kachel aber noch: Eintrag bleibt (unverändert %d, teilweise %d)", r.Unveraendert, r.Teilweise));
                    /* Codex-Prüfung 169 (1): scheitert die Vorprüfung, bleiben auch die aufgehobenen Spielstände dieses Spiels */
                    eintrag *n13 = nur_liste("FAKE78013"), *s13 = mit_pfad("/user/data/playden/games/steam-1013");
                    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                    if (n13) n13->Wahl = true;
                    if (s13) s13->Wahl = true;
                    r = kern_fuehre_loeschung(K, ja, NULL);
                    pruefe(n13 && s13 && s13->PlSpst && da("/user/data/playden/games/steam-1013/drive_c/save.sav") && pl_irgendwo("FAKE78013", "1013") && r.Unveraendert == 2 && r.Teilweise == 0 && r.Fehler == 0,
                           a_fmt(A0, "Vorprüfung scheitert (zwei Zeilen): Listeneintrag und aufgehobene Spielstände bleiben (unverändert %d, teilweise %d)", r.Unveraendert, r.Teilweise));
                }
            }
        }
        if (lauf == 0) {   /* NEU 1.0.10 (da_Chris86 12.11.2026 20:06, TabShifter TBSH00001): Kachel ohne jede Datei */
            eintrag *v0 = kachel("VERW71060"), *v1 = kachel("VERW71061"), *v2 = kachel("VERW71062"), *v3 = kachel("VERW71063");
            pruefe(v0 && v0->Pfade.n == 0 && v0->Kat == 6 && v0->Art == ART_REST && !v0->KachelOrdner && strstr(z(v0), "Kachel ohne Dateien: Verwaiste Kachel (VERW71060)")
                   && !v1 && v2 && v2->Pfade.n > 0 && !strstr(z(v2), "ohne Dateien") && !v3,
                   a_fmt(A0, "Kachel ohne Dateien: angeboten (über Sony, grün); nicht bei Einhängeordner eines Tools, installiert oder System (%s [%s])", z(v0), v0 ? kern_folge_kurz(v0) : "-"));
            if (v0) {
                setenv("UNINSTALL", "ablehnen", 1);
                for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                v0->Wahl = true;
                ergebnis r = kern_fuehre_loeschung(K, ja, NULL);
                pruefe(r.Unveraendert == 1 && r.Fehler == 0 && r.Teilweise == 0, a_fmt(A0, "Kachel ohne Dateien, Sony lehnt ab, steht auch nicht in Sonys Datenbank: unverändert, sonst nichts (unverändert %d)", r.Unveraendert));
                setenv("UNINSTALL", "ok", 1);
                r = kern_fuehre_loeschung(K, ja, NULL);
                unsetenv("UNINSTALL");
                pruefe(r.Unveraendert == 0 && r.Fehler == 0 && r.Teilweise == 0, a_fmt(A0, "Kachel ohne Dateien, Sony entfernt sie: erledigt (Fehler %d)", r.Fehler));
                v0->Wahl = false;
            }
            /* NEU 1.0.10 (da_Chris86 21:56): direkt eingetragene Homebrew-Kachel (wie TabShifter) - Sony lehnt ab, dann gezielt aus Sonys
             * Datenbank: nur ihre Zeilen, alles andere gleich (kachel_db_vergleich.py im Lauf-Skript prüft die Dateien danach) */
            eintrag *kp = kachel("KOPI00001");
            pruefe(kp && kp->Pfade.n == 0 && strstr(z(kp), "Kachel ohne Dateien: Kopie (KOPI00001)"), a_fmt(A0, "direkt eingetragene Kachel ohne Dateien erkannt (%s)", z(kp)));
            {   /* NEU 1.0.10 (da_Chris86 23:43): ihr Programm (beim Payload Manager hochgeladen) steht unter ihr, wie bei jeder Kachel - beide unter Apps */
                eintrag *ke = mit_pfad("/user/data/pldmgr/payloads/local/kopie");
                pruefe(kp && ke && ke->ToolKachel == kp && kp->Kat == 6, a_fmt(A0, "Kachel ohne Dateien ist Kopf ihres Blocks: ihr Programm steht unter ihr (%s | %s, Bereich %d)", ke ? z(ke) : "-", kp ? z(kp) : "-", kp ? kp->Kat : -1));
            }
            if (kp) {
                char g[1200] = "", v[300] = ""; int zz = 0;
                pruefe(kachel_db_austragen(KZ_ORDNER, "KOPI00001", 1, &zz, g, sizeof g, v, sizeof v) == 0, a_fmt(A0, "vorher: KOPI00001 in Sonys Test-Datenbank austragbar (%s)", g));
                {   /* Codex-Prüfung 171 (1): Sony hat den Auftrag ANGENOMMEN, die Kachel steht aber noch da - Sony arbeitet vielleicht noch daran:
                     * dann nie an die Datenbank, die Kachel bleibt (nicht als gelöscht gemeldet) */
                    setenv("UNINSTALL", "angenommen", 1);
                    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                    kp->Wahl = true;
                    ergebnis r0 = kern_fuehre_loeschung(K, ja, NULL);
                    unsetenv("UNINSTALL");
                    int r1 = kachel_db_austragen(KZ_ORDNER, "KOPI00001", 1, &zz, g, sizeof g, v, sizeof v);
                    pruefe(r1 == 0 && r0.Unveraendert + r0.Fehler + r0.Teilweise > 0,
                           a_fmt(A0, "Sony nimmt an, Kachel bleibt: Sonys Datenbank unberührt, nicht als gelöscht gemeldet (unverändert %d, Fehler %d, teilweise %d; %s)", r0.Unveraendert, r0.Fehler, r0.Teilweise, g));
                }
                setenv("UNINSTALL", "ablehnen", 1);
                for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                kp->Wahl = true;
                ergebnis r = kern_fuehre_loeschung(K, ja, NULL);
                unsetenv("UNINSTALL");
                int rr = kachel_db_austragen(KZ_ORDNER, "KOPI00001", 1, &zz, g, sizeof g, v, sizeof v);
                pruefe(r.Fehler == 0 && r.Teilweise == 0 && r.Unveraendert == 0 && rr == -1 && strstr(g, "nicht genau einmal"),
                       a_fmt(A0, "Sony lehnt ab: aus Sonys Datenbank ausgetragen, als gelöscht gemeldet (Fehler %d, unverändert %d; danach: %s)", r.Fehler, r.Unveraendert, g));
                kp->Wahl = false;
            }
            /* dasselbe mit Dateien (aktive Kachel, wie TabShifter vor dem Löschen): Sony lehnt ab -> aus Sonys Datenbank, dann die Dateien */
            eintrag *k2 = kachel("KOPI00002");
            pruefe(k2 && k2->KachelOrdner && da("/user/app/KOPI00002"), a_fmt(A0, "aktive direkt eingetragene Kachel mit App-Ordner (%s)", z(k2)));
            if (k2) {
                setenv("UNINSTALL", "ablehnen", 1);
                for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                k2->Wahl = true; kern_abhaengige_mitnehmen(K);
                ergebnis r = kern_fuehre_loeschung(K, ja, NULL);
                unsetenv("UNINSTALL");
                char g[1200] = "", v[300] = ""; int zz = 0;
                int rr = kachel_db_austragen(KZ_ORDNER, "KOPI00002", 1, &zz, g, sizeof g, v, sizeof v);
                pruefe(r.Fehler == 0 && r.Teilweise == 0 && r.Unveraendert == 0 && rr == -1 && !da("/user/app/KOPI00002") && !da("/user/appmeta/KOPI00002") && !da("/system_data/priv/appmeta/KOPI00002"),
                       a_fmt(A0, "aktive Kachel, Sony lehnt ab: aus Sonys Datenbank ausgetragen, Dateien gelöscht (Fehler %d, teilweise %d, unverändert %d)", r.Fehler, r.Teilweise, r.Unveraendert));
                k2->Wahl = false;
            }
        }
        {   /* NEU 1.0.10 (da_Chris86 22:40): Zusatzinhalt allein gewählt - zuerst Sonys Eintrag (addcont.db) über Sony, nachgewiesen, dann die Dateien.
             * Lehnt Sony ab, bleiben die Dateien (sonst stünde ein Zusatzinhalt ohne Dateien in Sonys Liste) */
            eintrag *dl = mit_pfad("/user/addcont/PPSA55555/dlc1");
            pruefe(dl != NULL && !dl->KachelId, a_fmt(A0, "Zusatzinhalt PPSA55555/dlc1 ist ein eigener Eintrag (%s)", dl ? z(dl) : "-"));
            if (dl) {
                setenv("ZUSATZ", "ablehnen", 1);
                for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                dl->Wahl = true;
                ergebnis r = kern_fuehre_loeschung(K, ja, NULL);
                pruefe(da("/user/addcont/PPSA55555/dlc1") && r.Unveraendert == 1 && r.Fehler == 0 && r.Teilweise == 0,
                       a_fmt(A0, "Sony trägt den Zusatzinhalt nicht aus: Dateien bleiben, unverändert (unverändert %d, Fehler %d)", r.Unveraendert, r.Fehler));
                setenv("ZUSATZ", "ok", 1);
                r = kern_fuehre_loeschung(K, ja, NULL);
                unsetenv("ZUSATZ");
                pruefe(!da("/user/addcont/PPSA55555/dlc1") && da("/user/app/PPSA55555") && r.Unveraendert == 0 && r.Fehler == 0 && r.Teilweise == 0,
                       a_fmt(A0, "Sony trägt den Zusatzinhalt aus: weg, das Spiel bleibt (unverändert %d, Fehler %d, teilweise %d)", r.Unveraendert, r.Fehler, r.Teilweise));
                dl->Wahl = false;
            }
            eintrag *d2 = mit_pfad("/user/addcont/PPSA77700/DLC1");
            if (d2) {   /* Codex-Prüfung 172 (2): Sony hat schon einen Teil ausgetragen (551) - das ist "teilweise", nie "unverändert" */
                setenv("ZUSATZ", "teil", 1);
                for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                d2->Wahl = true;
                ergebnis r = kern_fuehre_loeschung(K, ja, NULL);
                unsetenv("ZUSATZ");
                pruefe(r.Teilweise == 1 && r.Unveraendert == 0 && r.Fehler == 0, a_fmt(A0, "Sony trägt nur einen Teil aus: teilweise, nicht unverändert (teilweise %d, unverändert %d)", r.Teilweise, r.Unveraendert));
                d2->Wahl = false;
            } else pruefe(false, "Zusatzinhalt PPSA77700/DLC1 fehlt");
        }
        {   /* NEU 1.0.10 (da_Chris86 13.11.2026): ein Download allein gewählt - nur sein Ordner geht, die Programmdatei auf dem USB-Laufwerk
             * und der übrige Verwalter bleiben */
            eintrag *sr = mit_pfad("/user/data/pldmgr/payloads/repo/schattenwerk@e8cce5d1");
            if (sr) {
                for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                sr->Wahl = true;
                ergebnis r = kern_fuehre_loeschung(K, ja, NULL);
                pruefe(!da("/user/data/pldmgr/payloads/repo/schattenwerk@e8cce5d1") && da("/mnt/usb0/Schattenwerk_1.7-beta11.elf") && da("/user/data/pldmgr/payloads/repo/klogsrv@e8cce5d1")
                       && da("/user/data/pldmgr/payloads/repo/kachelwerk@e8cce5d1") && da("/user/data/pldmgr/payloads/local/tabshifter") && da("/user/data/pldmgr/payloads/repo/schutzwerk@e8cce5d1/ps5c.elf")
                       && r.Fehler == 0 && r.Teilweise == 0 && r.Unveraendert == 0,
                       a_fmt(A0, "Download allein gelöscht: nur sein Ordner, sonst nichts (Fehler %d, teilweise %d, unverändert %d)", r.Fehler, r.Teilweise, r.Unveraendert));
                sr->Wahl = false;
            } else pruefe(false, "Download schattenwerk fehlt");
        }
        eintrag *ge = mit_pfad("/mnt/usb0/garlic-savemgr_v1.15-beta1.elf"), *gl = mit_pfad("/user/data/garlic-savemgr.instance.lock");
        pruefe(ge && gl && gl->Begleit && gl->Begleit == (ge->Begleit ? ge->Begleit : ge) && gl->Kat == 6 && strcmp(kern_folge_kurz(gl), "Inhalt unbekannt") != 0,
               a_fmt(A0, "Sperrdatei eines Payloads ohne Kachel (garlic-savemgr.instance.lock): bei seiner ELF-Datei, nicht \"Inhalt unbekannt\" (%s [%s] | %s)",
                     z(gl), gl ? kern_folge_kurz(gl) : "-", z(ge)));
        eintrag *rw = mit_pfad("/system_data/httpcache/nobackup/PPSA79999"), *rp = mit_pfad("/system_data/ps4mount/app/CUSA79998");
        pruefe(rw && !rw->ToolKachel && rw->Kat == 5 && !rw->Gesperrt && strstr(z(rw), "nicht mehr installierten App") && rp && !rp->ToolKachel && rp->Kat == 5 && !rp->Gesperrt
               && rw->Art == ART_REST && rp->Art == ART_REST
               && strstr(z(rp), "ps4mount-Daten einer nicht mehr installierten App"),
               a_fmt(A0, "dasselbe ohne App: Rest unter Sonstiges, wählbar (%s | %s)", z(rw), z(rp)));
        /* NEU 1.0.10 (da_Chris86 14.11.2026 19:36, Spider-Man 2) */
        eintrag *sk = mit_pfad("/mnt/ext1/PKG/Sternfall - CUSA40071 - v1.00 - EU.pkg"), *sr = mit_pfad("/system_data/ps4mount/app/CUSA40071");
        pruefe(sk && sr && sk->PkgOhneSpiel && sr->Begleit == sk && sr->Kat == sk->Kat && sk->Kat == 1 && sr->KatHer == 5 && !sr->Gesperrt && sr->Art == ART_REST
               && strstr(z(sr), "ps4mount-Daten einer nicht mehr installierten App"),
               a_fmt(A0, "ps4mount-Daten eines nicht installierten Spiels: im Block seiner PKG-Datei unter Spiele, bleibt ein Rest (%s [Bereich %d, Art %d] | %s)",
                     z(sr), sr ? sr->Kat : -1, sr ? sr->Art : -1, z(sk)));
        pruefe(!mit_pfad("/system_data/httpcache/nobackup/NPXS40002") && !kern_erlaubt(K, "/system_data/httpcache/nobackup/NPXS40002"),
               "Web-Zwischenspeicher einer Sony-App (NPXS): nie angeboten, nie erlaubt");
        eintrag *tk = kachel("TBSH71050"), *tb = mit_pfad("/system_data/priv/mms/app.db.tabshifter.bak"), *fb = mit_pfad("/system_data/priv/mms/app.db.fremdtool.bak");
        pruefe(tb && tb->Art == ART_ALTE_SICHERUNG && kern_ampel(tb) != AMPEL_ROT && !strcmp(kern_folge_kurz(tb), "nicht mehr gebraucht"),
               a_fmt(A0, "Sicherung unter der Kachel ihres Programms: \"nicht mehr gebraucht\", nicht rot - der Haken auf den Block nimmt sie mit (da_Chris86 13.11.2026) (%s)", tb ? kern_folge_kurz(tb) : "-"));
        pruefe(tk && tb && tb == mit_pfad("/system_data/priv/mms/appinfo.db.tabshifter.bak") && tb->ToolKachel == tk && strstr(z(tb), "angelegt von tabshifter"),
               a_fmt(A0, "TabShifters Sicherungen von Sonys App-Datenbank: EIN Eintrag unter der Kachel TabShifter (%s)", z(tb)));
        pruefe(fb && !fb->ToolKachel && fb->Kat == 5 && !fb->Gesperrt && fb->Art == ART_TOOL_DATEN_WICHTIG && kern_ampel(fb) == AMPEL_ROT,
               a_fmt(A0, "Sicherung eines Tools ohne Kachel: Rest unter Sonstiges, rot (%s)", z(fb)));
        bool sony = false; const char *sn[4] = { "app.db.bak", "app.db", "app.bckdb", "addcont_backup.db" };
        for (int i = 0; i < 4; i++) { const char *sp = VERB(A0, "/system_data/priv/mms/", sn[i]); if (mit_pfad(sp) || kern_erlaubt(K, sp)) sony = true; }
        pruefe(!sony, "Sonys eigene Dateien (app.db, app.bckdb, addcont_backup.db) und ein namenloses app.db.bak: nie angeboten, nie erlaubt");
        /* selbst Hochgeladenes beim Payload Manager (da_Chris86 12.11.2026 05:59) */
        eintrag *pt = mit_pfad("/user/data/pldmgr/payloads/local/tabshifter"), *pe = mit_pfad("/user/data/pldmgr/payloads/local/einzelwerk");
        eintrag *pr = mit_pfad("/user/data/pldmgr/payloads/repo"), *pc = mit_pfad("/user/data/pldmgr/local/ps5c/ps5c.elf");
        if (!pr) pr = mit_pfad("/user/data/pldmgr/payloads/repo/klogsrv@e8cce5d1");   /* 1.0.10 (13.11.2026): repo aufgeteilt - klogsrv bleibt beim Verwalter */
        bool doppelt = false;   /* kein anderer Eintrag enthält die herausgelösten Ordner noch (als Ganzes oder als Ordner darüber) */
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f == pt || f == pe) continue;
            for (int j = 0; j < f->PfadeBasis.n; j++) { const char *q = f->PfadeBasis.e[j];
                if (!strcmp(q, "/user/data/pldmgr") || !strcmp(q, "/user/data/pldmgr/payloads") || !strcmp(q, "/user/data/pldmgr/payloads/local") || (strstr(q, "/pldmgr/payloads/local/") && !strstr(q, "/pldmgr/payloads/local/mitkopie/") && !strstr(q, "/pldmgr/payloads/local/kopie") && !strstr(q, "/pldmgr/payloads/local/upkg"))) { doppelt = true; fprintf(stderr, "doppelt: %s\n", q); } } }   /* upkg: PKG darin, bleibt beim Verwalter (Codex-Prüfung 174 (P2)) */   /* mitkopie: eigener Fall (Codex-Prüfung 159 (3)); kopie: Programm der Kachel ohne Dateien (23:43) */
        pruefe(tk && pt && pt->ToolKachel == tk && pt->PfadeBasis.n == 1 && !pt->Gesperrt && strstr(z(pt), "von dir hochgeladen"),
               a_fmt(A0, "selbst hochgeladenes TabShifter.elf (payloads/local) steht unter der Kachel TabShifter (%s)", z(pt)));
        pruefe(pe && !pe->ToolKachel && pe->PfadeBasis.n == 1 && !pe->Gesperrt && pe != pr, a_fmt(A0, "selbst hochgeladen ohne passende Kachel: eigener Eintrag (%s)", z(pe)));
        pruefe(pr && pr != pt && !doppelt, a_fmt(A0, "Downloads aus einer Quelle (payloads/repo) bleiben beim Payload-Manager-Ordner, nichts doppelt (%s)", z(pr)));
        pruefe(pc && pc->Gesperrt && pc != pr && pc != pt, a_fmt(A0, "Kopie von PS5 Cleanup daneben bleibt geschützt (%s)", z(pc)));
        {   /* NEU 1.0.10 (da_Chris86 13.11.2026): Downloads des Payload-Verwalters (payloads/repo) beim selben Programm - nur Anzeige */
            const char *R = "/user/data/pldmgr/payloads/repo/";
            eintrag *su = mit_pfad("/mnt/usb0/Schattenwerk_1.7-beta11.elf"), *sr = mit_pfad(VERB(A0, R, "schattenwerk@e8cce5d1"));
            eintrag *kw = kachel("KWRK71052"), *kr = mit_pfad(VERB(A0, R, "kachelwerk@e8cce5d1"));
            eintrag *dw = mit_pfad("/user/data/datenwerk"), *dr = mit_pfad(VERB(A0, R, "ps5-datenwerk@e8cce5d1"));
            eintrag *sc = mit_pfad(VERB(A0, R, "schutzwerk@e8cce5d1/ps5c.elf")), *kl = mit_pfad(VERB(A0, R, "klogsrv@e8cce5d1"));
            int drin = 0;   /* jeder Download-Ordner steht in genau EINEM Eintrag, nirgends zusätzlich ein Ordner darüber */
            const char *ns[3] = { "schattenwerk@e8cce5d1", "kachelwerk@e8cce5d1", "ps5-datenwerk@e8cce5d1" };
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i];
                for (int j = 0; j < f->PfadeBasis.n; j++) { const char *q = f->PfadeBasis.e[j];
                    for (int x = 0; x < 3; x++) { const char *c = VERB(A0, R, ns[x]); if (!strcmp(q, c) || txt_beginnt(c, VERB(A0, q, "/")) || txt_beginnt(q, VERB(A0, c, "/"))) drin++; } } }
            pruefe(su && sr && sr != su && sr->RepoName && sr->Begleit == su && sr->PfadeBasis.n == 1 && strstr(z(sr), "(heruntergeladen)") && sr->Art == ART_DATEI
                   && !strcmp(kern_folge_kurz(sr), "neu herunterladen") && kern_kopf_von(K, sr, sr->Kat) == su,
                   a_fmt(A0, "Download neben der Programmdatei auf dem USB-Laufwerk: im selben Block, eigene Zeile, \"neu herunterladen\" (%s | %s)", z(sr), z(su)));
            pruefe(kw && kr && kr->RepoName && kr->Begleit == kw && kern_kopf_von(K, kr, kr->Kat) == kw,
                   a_fmt(A0, "Download unter der Kachel desselben Programms (%s | %s)", z(kr), z(kw)));
            pruefe(dw && dr && dr->RepoName && dr->Begleit == dw && dw->OhneKachel && kern_kopf_von(K, dr, dr->Kat) == dw,
                   a_fmt(A0, "Download bei seinem Daten-Ordner (\"ps5-\" vorne zählt nicht) (%s | %s)", z(dr), z(dw)));
            pruefe(drin == 3, a_fmt(A0, "jeder zugeordnete Download steht genau einmal, nicht zusätzlich beim Verwalter (%d)", drin));
            bool sw = false, lk = false;
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->RepoName && strstr(f->RepoName, "schutzwerk")) sw = true;
                for (int j = 0; j < f->PfadeBasis.n; j++) if (strstr(f->PfadeBasis.e[j], "linkwerk-ziel") || strstr(f->PfadeBasis.e[j], "/linkwerk@e8cce5d1/")) lk = true; }
            pruefe(sc && sc->Gesperrt && kl && kl->RepoName && !sw, a_fmt(A0, "mit Kopie von PS5 Cleanup darin bleibt er geschützt, ohne passendes Programm bekommt der Download seine eigene Zeile (%s | %s)", z(sc), z(kl)));
            pruefe(!lk, "verknüpfter Download-Ordner: nie dahinter (nur die Verknüpfung selbst)");
            /* Codex-Prüfung 174 (P3): Programm und Download zählen als EIN Block (Zusammenfassung, vollständige Bereinigung) */
            pruefe(sr && su && kr && kw && dr && dw && !strcmp(ablauf_block_schluessel(A0, sr), ablauf_block_schluessel(A0, su))
                   && !strcmp(ablauf_block_schluessel(A0, kr), ablauf_block_schluessel(A0, kw)) && !strcmp(ablauf_block_schluessel(A0, dr), ablauf_block_schluessel(A0, dw)),
                   "Download und sein Programm zählen als ein Block (Codex-Prüfung 174 (P3))");
            /* Codex-Prüfung 174 (P2) / 175: eine PKG-Datei darin bleibt eine eigene rote Zeile mit Warnung; Download bzw. Upload werden
             * trotzdem herausgelöst - ohne die PKG-Datei (kein Pfad des neuen Eintrags enthält sie) */
            const char *PK = VERB(A0, R, "pkgwerk@e8cce5d1/Pkgwerk - CUSA40061 - v1.00.pkg"), *UK = "/user/data/pldmgr/payloads/local/upkg/Upkg - CUSA40062 - v1.00.pkg";
            eintrag *pp = mit_pfad(PK), *up = mit_pfad(UK), *pw = NULL, *uw = NULL;
            bool ueber = false;   /* enthält irgendein ANDERER Eintrag die PKG-Datei (als sie selbst oder über einen Ordner darüber)? */
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i];
                if (f->RepoName && !strcmp(f->RepoName, "pkgwerk")) pw = f;
                if (kern_selbst_hochgeladen(f) && !f->PkgArt && f->PfadeBasis.n > 0 && strstr(f->PfadeBasis.e[0], "/payloads/local/upkg")) uw = f;
                for (int j = 0; j < f->PfadeBasis.n; j++) { const char *q = f->PfadeBasis.e[j];
                    if ((f != pp && (!strcmp(q, PK) || txt_beginnt(PK, VERB(A0, q, "/")))) || (f != up && (!strcmp(q, UK) || txt_beginnt(UK, VERB(A0, q, "/"))))) ueber = true; } }
            pruefe(pp && pp->PkgArt && kern_ampel(pp) == AMPEL_ROT && up && up->PkgArt && kern_ampel(up) == AMPEL_ROT && pw && pw != pp && uw && uw != up && !ueber,
                   a_fmt(A0, "PKG-Datei im Download- bzw. Upload-Ordner: eigene rote Zeile; Download und Upload ohne sie herausgelöst (Codex-Prüfung 174/175) (%s | %s | %s | %s)", z(pp), z(up), z(pw), z(uw)));
            /* selbst Hochgeladenes heißt "neu hochladen", die Programmdatei vom USB-Laufwerk weiter "neu herunterladen" */
            pruefe(pt && kern_selbst_hochgeladen(pt) && !strcmp(kern_folge_kurz(pt), "neu hochladen") && su && !kern_selbst_hochgeladen(su) && !strcmp(kern_folge_kurz(su), "neu herunterladen"),
                   a_fmt(A0, "selbst hochgeladen: \"neu hochladen\", USB-Datei: \"neu herunterladen\" (%s | %s)", pt ? kern_folge_kurz(pt) : "-", su ? kern_folge_kurz(su) : "-"));
        }
        /* Codex-Prüfung 159 */
        eintrag *nt = mit_pfad("/user/data/nurlader/payloads/local/tabshifter");
        pruefe(nt && nt->ToolKachel == tk && nt->PfadeBasis.n == 1 && !mit_pfad("/user/data/nurlader"),
               a_fmt(A0, "Verwalter, in dem NUR das Hochgeladene liegt: herausgelöst, unter der Kachel, kein leerer Eintrag (Codex-Prüfung 159 (1)) (%s)", z(nt)));
        eintrag *lv = mit_pfad("/user/data/linklader"); bool hinter = false;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (strstr(f->PfadeBasis.e[j], "/linklader/payloads/") || strstr(f->PfadeBasis.e[j], "/extern/")) hinter = true; }
        pruefe(lv && !hinter, a_fmt(A0, "payloads als Verknüpfung: nicht dahinter aufgeteilt, der Verwalter bleibt EIN Eintrag (Codex-Prüfung 159 (2)) (%s)", z(lv)));
        eintrag *mk = mit_pfad("/user/data/pldmgr/payloads/local/mitkopie/werk.elf"), *mc = mit_pfad("/user/data/pldmgr/payloads/local/mitkopie/ps5c.elf");
        pruefe(mk && mk == mit_pfad("/user/data/pldmgr/payloads/local/mitkopie/meta.json") && !mk->Gesperrt && strstr(z(mk), "von dir hochgeladen") && mc && mc->Gesperrt && mc != mk,
               a_fmt(A0, "Upload-Ordner mit Kopie von PS5 Cleanup: die übrigen Dateien als eigener Eintrag, die Kopie bleibt geschützt (Codex-Prüfung 159 (3)) (%s | %s)", z(mk), z(mc)));
        eintrag *mw = kachel("MISC71051"), *mb = mit_pfad("/system_data/priv/mms/app.db.MischWerk.bak");
        pruefe(mw && mb && mb == mit_pfad("/system_data/priv/mms/appinfo.db.mischwerk.bak") && mb->ToolKachel == mw,
               a_fmt(A0, "Sicherungen in gemischter Schreibweise: EIN Eintrag unter der Kachel (Codex-Prüfung 159 (4)) (%s)", z(mb)));
        eintrag *zt = mit_pfad("/user/data/zweitlader/payloads/local/tabshifter"), *zr = mit_pfad("/user/data/zweitlader/einstellungen.txt");
        bool zganz = mit_pfad("/user/data/zweitlader") || mit_pfad("/user/data/zweitlader/payloads") || mit_pfad("/user/data/zweitlader/payloads/local");
        pruefe(zt && zt->ToolKachel == tk && zr && zr != zt && !zganz && h_hat(&K->AUFGEKLAPPT, "/user/data/zweitlader") && h_hat(&K->AUFGEKLAPPT, "/user/data/zweitlader/payloads/local"),
               a_fmt(A0, "anderer Verwalter, bisher EIN Eintrag: aufgeteilt, das Hochgeladene unter seiner Kachel, der Rest für sich (%s | %s)", z(zt), z(zr)));
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
        /* NEU 1.0.10 (Erkennungsliste): ein Ordner mit zwei Kennungen geht fest zu der Kachel, die wirklich da ist; neue App ohne Kachel */
        eintrag *ek = kachel("PPSA99016"), *ed = mit_pfad("/user/data/prosperoeden"), *mu = mit_pfad("/user/data/mupen64plus");
        pruefe(ek && ed && ed->ToolKachel == ek, a_fmt(A0, "prosperoeden steht bei der Spiegel-Kachel PPSA99016 (zweite Kennung) (%s)", z(ed)));
        pruefe(mu && !mu->ToolKachel && mu->Kat == 6 && kern_ampel(mu) == AMPEL_ROT && strstr(z(mu), "gehört zu Mupen64Plus PS5"),
               a_fmt(A0, "mupen64plus ohne Kachel: unter Apps, rot (deine ROMs), gehört zu Mupen64Plus PS5 (%s)", z(mu)));
    }

    printf("== Löschlauf: 8 Zusatzinhalte (eine Zeile) und der Rest des Ordners PS4\n");
    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
    if (ndl && rest) {
        ndl->Wahl = rest->Wahl = true;
        if (st_mo) st_mo->Wahl = true;   /* NEU 1.0.9 (#145) */
        /* NEU 1.0.10 (Codex-Prüfung 172 (5)): eine gelöschte Quelle steht in ShadowMounts Liste, das Austragen dort scheitert - "teilweise" */
        const char *ML = VERB(A0, W, "/user/data/shadowmount/manual.lst"), *ML_INHALT = "/mnt/usb0/PS4/liesmich.txt\n/mnt/usb0/PS4/unbekannt.pkg\n";
        { FILE *mf = fopen(ML, "w"); if (mf) { fputs(ML_INHALT, mf); fclose(mf); } }
        setenv("SM_KEIN_SCHREIBEN", "1", 1);
        ergebnis r = kern_fuehre_loeschung(K, ja, NULL);
        unsetenv("SM_KEIN_SCHREIBEN");
        {   char ml[512] = ""; FILE *mf = fopen(ML, "r"); size_t mn = mf ? fread(ml, 1, sizeof ml - 1, mf) : 0; if (mf) fclose(mf); ml[mn] = 0; unlink(ML);
            pruefe(r.Teilweise == 1 && !strcmp(ml, ML_INHALT), a_fmt(A0, "ShadowMounts Liste ließ sich nicht ändern: als teilweise gemeldet, Liste unverändert (teilweise %d)", r.Teilweise)); }
        int weg = 0; for (int i = 0; i < 8; i++) if (!da(a_fmt(A0, "/mnt/usb0/DLC/EP0001-CUSA40002_00-NEBELDLC0000000%d.pkg", i))) weg++;
        pruefe(weg == 8 && r.Fehler == 0, a_fmt(A0, "alle 8 Dateien der Zeile gelöscht (%d, Fehler %d)", weg, r.Fehler));
        pruefe(da("/mnt/ext1/PKG/EP0001-CUSA40002_00-NEBELDLC00000010.pkg") && da("/mnt/ext1/PKG/Nebelpfad - CUSA40002 - v1.00 - EU.pkg"), "Zusatzinhalte und Installationsdatei auf der M.2 bleiben");
        pruefe(!da("/mnt/usb0/PS4/Cover/bild.jpg") && !da("/mnt/usb0/PS4/liesmich.txt"), "Rest des Ordners PS4 gelöscht");
        pruefe(da("/mnt/usb0/PS4/Wolkenreise - CUSA40001 - v1.00 - EU.pkg") && da("/mnt/usb0/PS4/Nebelpfad - CUSA40002 - v1.05 - EU - Update.pkg") && da("/mnt/usb0/PS4/unbekannt.pkg"),
               "die PKG-Dateien im Ordner PS4 bleiben (nicht gewählt)");
        pruefe(st_mo && !da("/mnt/ext1/prosperostore/previous/PPSA99709") && da("/mnt/ext1/prosperostore/staging/PPSA99709.zip") && da("/user/data/prosperostore/receipts/PPSA99708.json"),
               "aufgehobene Fassung einer gelöschten App gelöscht - ihr angefangener Download und die Belege bleiben (nicht gewählt) (NEU 1.0.9, #145)");
    } else pruefe(false, "Löschlauf: Einträge fehlen");
    {   /* NEU 1.0.10 (da_Chris86 12.11.2026): Reste in /system_data wirklich löschen - Verknüpfung nur selbst (nie ihr Ziel), Sonys Dateien bleiben */
        printf("== Löschlauf: Web-Zwischenspeicher, ps4mount-Rest, Sicherungen von TabShifter und eines Tools ohne Kachel\n");
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        eintrag *rw = mit_pfad("/system_data/httpcache/nobackup/PPSA79999"), *rp = mit_pfad("/system_data/ps4mount/app/CUSA79998");
        eintrag *tb = mit_pfad("/system_data/priv/mms/app.db.tabshifter.bak"), *fb = mit_pfad("/system_data/priv/mms/app.db.fremdtool.bak");
        char ziel[4096] = {0}; ssize_t zl = readlink(VERB(A0, W, "/system_data/ps4mount/app/CUSA79998/app.pkg"), ziel, sizeof ziel - 1);   /* Codex-Prüfung 159 (5) */
        pruefe(zl > 0 && !strcmp(ziel, VERB(A0, W, "/mnt/usb0/ps4/CUSA79998/app.pkg")) && da("/mnt/usb0/ps4/CUSA79998/app.pkg"), "Vorprüfung: die Verknüpfung zeigt auf die geprüfte Datei im Testbaum");
        if (rw && rp && tb && fb) {
            /* NEU 1.0.10: ps4mount läuft - erst weigert es sich (Spiel offen): der ps4mount-Rest bleibt; dann beendet es sich */
            mkdir(VERB(A0, W, "/user/data/ps4mount"), 0777);
            FILE *lf = fopen(VERB(A0, W, "/user/data/ps4mount/log.txt"), "w"); if (lf) { fputs("[notify] PS4Mount v1.2 running\n", lf); fclose(lf); }
            /* Codex-Prüfung 163 (3): läuft es nicht (Sperr-Port frei), wird gar nicht erst gewartet */
            setenv("PS4MOUNT", "aus", 1);
            for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
            eintrag *pk0 = mit_pfad("/mnt/usb0/PS4/unbekannt.pkg"); if (pk0) pk0->Wahl = true;
            ergebnis r0 = kern_fuehre_loeschung(K, ja, NULL);
            pruefe(pk0 && K->P4M == 1 && !da("/user/data/ps4mount/stop") && r0.Unveraendert == 0, "ps4mount läuft nicht: nichts angehalten, PKG-Datei gelöscht");
            for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
            setenv("PS4MOUNT", "laeuft", 1);
            pthread_t pt; p4m_aus = 0; p4m_art = 2; pthread_create(&pt, NULL, p4m_faden, NULL);
            rp->Wahl = true;
            ergebnis r = kern_fuehre_loeschung(K, ja, NULL);
            pruefe(da("/system_data/ps4mount/app/CUSA79998") && r.Unveraendert == 1 && !da("/user/data/ps4mount/stop") && strstr(K->P4M_GRUND, "Close Wolkenreise"),
                   a_fmt(A0, "ps4mount weigert sich (Spiel offen): der ps4mount-Rest bleibt unverändert (%s)", K->P4M_GRUND));
            p4m_art = 1;
            for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
            rw->Wahl = rp->Wahl = tb->Wahl = fb->Wahl = true;
            r = kern_fuehre_loeschung(K, ja, NULL);
            pruefe(K->P4M == 1 && !da("/user/data/ps4mount/stop"), "ps4mount beendet sich auf die Stopp-Datei - danach wird gelöscht");
            p4m_aus = 1; pthread_join(pt, NULL);
            unsetenv("PS4MOUNT");
            pruefe(r.Fehler == 0 && !da("/system_data/httpcache/nobackup/PPSA79999") && !da("/system_data/ps4mount/app/CUSA79998"),
                   a_fmt(A0, "Web-Zwischenspeicher und ps4mount-Rest gelöscht (Fehler %d)", r.Fehler));
            pruefe(!da("/system_data/priv/mms/app.db.tabshifter.bak") && !da("/system_data/priv/mms/appinfo.db.tabshifter.bak") && !da("/system_data/priv/mms/app.db.fremdtool.bak"),
                   "Sicherungen von TabShifter und des Tools ohne Kachel gelöscht");
            pruefe(da("/system_data/priv/mms/app.db") && da("/system_data/priv/mms/app.bckdb") && da("/system_data/priv/mms/app.db.bak") && da("/system_data/priv/mms/addcont_backup.db")
                   && da("/system_data/httpcache/nobackup/NPXS40002") && da("/system_data/httpcache/nobackup/VOLL71034") && da("/system_data/ps4mount/stage/VOLL71034/mount.lnk"),
                   "Sonys Dateien, der Zwischenspeicher der Sony-App und die Daten der installierten App bleiben");
            pruefe(da("/mnt/usb0/ps4/CUSA79998/app.pkg"), "ps4mount-Rest: nur die Verknüpfung gelöscht, nie ihr Ziel (die PKG auf dem Stick bleibt)");
        } else pruefe(false, "Löschlauf /system_data: Einträge fehlen");
    }
    {   /* da_Chris86 12.11.2026 06:55: "wenn Playden angehakt ist, die Kachel, dann soll alles weg" */
        printf("== Löschlauf: Playden ganz (der Block)\n");
        eintrag *pk = kachel("FAKE77031");
        kblock *b = NULL, *bp = NULL; int nb = pk ? kern_bloecke(K, A0, pk->Kat, SORT_AZ, &b) : 0;
        for (int x = 0; x < nb; x++) if (b[x].kopf == pk) bp = &b[x];
        if (bp) {
            /* Codex-Prüfung 160 (2): wie der Haken auf dem Block in der App (app.c block_setzen_p) - ohne Gesperrtes, Rotes, nur einzeln Wählbares */
            for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
            for (int t = 0; t < bp->teile.n; t++) { eintrag *f = bp->teile.e[t]; if (!f->Gesperrt && !f->Rot && !f->NurEinzeln && !f->Paket) f->Wahl = true; }
            /* NEU 1.0.10 (22:40): ShadowMounts Liste manual.lst nennt den Starter von 3DMark (wie Playden ihn einträgt) und eine fremde Quelle */
            const char *ML = VERB(A0, W, "/user/data/shadowmount/manual.lst");
            { FILE *mf = fopen(ML, "w"); if (mf) { fputs("/mnt/usb0/games/BLEIBT00001\n/data/homebrew/FAKE78001/\r\n/data/homebrew/FAKE78099\n", mf); fclose(mf); } }
            ergebnis r = kern_fuehre_loeschung(K, ja, NULL);
            {   char ml[512] = ""; FILE *mf = fopen(ML, "r"); size_t mn = mf ? fread(ml, 1, sizeof ml - 1, mf) : 0; if (mf) fclose(mf); ml[mn] = 0;
                pruefe(!strcmp(ml, "/mnt/usb0/games/BLEIBT00001\n/data/homebrew/FAKE78099\n"),
                       a_fmt(A0, "ShadowMounts Liste manual.lst: nur die Zeile der gelöschten Quelle (FAKE78001) ausgetragen, alles andere gleich (%s)", ml)); }
            pruefe(r.Fehler == 0 && r.Teilweise == 0 && !da("/user/app/FAKE77031") && !da("/user/app/FAKE78001") && !da("/user/data/homebrew/FAKE78001") && !da("/user/data/playden-games/223850")
                   && da("/user/data/playden/games/steam-223850/save.dat"),
                   a_fmt(A0, "Haken auf den Block Playden: Kacheln, Starter und Spieldateien weg, die Spielstände (rot) bleiben (Fehler %d)", r.Fehler));
            for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
            for (int t = 0; t < bp->teile.n; t++) { eintrag *f = bp->teile.e[t]; if (!f->Gesperrt) f->Wahl = true; }   /* das Rote einzeln dazu */
            for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->PlSpst && !f->Gesperrt) f->Wahl = true; }   /* NEU 1.0.10: und die Spielstände (Bereich Spielstände) */
            r = kern_fuehre_loeschung(K, ja, NULL);
            pruefe(!da("/user/data/playden/games/steam-223850") && !da("/user/data/playden-games/777") && !da("/user/data/playden/kits")
                   && da("/user/data/playden/games/steam-888/ps5c.elf"),
                   a_fmt(A0, "dazu das Rote einzeln und die Spielstände: Playden ganz weg - nur die gesperrte Kopie von PS5 Cleanup bleibt (Fehler %d, %d Zeilen)", r.Fehler, bp->teile.n));
            pruefe(!da("/mnt/ext1/playden"), "NEU 1.0.10 (PS5-Test 12:25): der leere Ordner playden auf der M.2 geht mit");   /* Spieldateien von Fallout lagen darin */
            pruefe(da("/user/data/pldmgr") && da("/user/app/TBSH71050"), "anderes bleibt");
        } else pruefe(false, "Block Playden fehlt");
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
