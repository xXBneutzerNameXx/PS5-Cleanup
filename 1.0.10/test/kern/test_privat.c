/* PS5 Cleanup 1.0.10 - Test: private Ordner von Programmen (Playden & Co., da_Chris86 08.11.2026 08:02/08:20; Entwurf 1.0.9, aufgenommen 1.0.10).
 * NEU 1.0.10: nur im Ordner eines bekannten Programms (privatapp unbekannt, snes9x bekannt), nie über eine Verknüpfung im privaten Ordner
 * (Codex-Prüfung 126), Sicherheitsstopp sperrt auch Sonys Befehle und zählt auch nach dem letzten Schritt als Fehler.
 * Am PC nachgestellt: "privat" = Ordner (oder Datei) mit Rechten 000, die uns selbst gehören - ohne root darf dann auch der Besitzer nicht
 * hinein, so wie root auf der PS5 nicht in Ordner anderer Benutzer mit 0700. Der Wechsel (zugriff_als_besitzer) macht hier genau das, was
 * den Weg versperrt, für den einen Schritt auf 0700 und stellt danach die alten Rechte wieder her. Auf der PS5 ist es seteuid (echt
 * prüfbar nur dort). Am PC geht darum kein privater Ordner IN einem privaten Ordner - der kommt auf der PS5 bei Playden nicht vor.
 *   test_privat <hier> zugriff|ohne|kern|m2|sim|kaputt|kaputt_ende|kaputt_nachlauf|kaputt_selbst     Testbaum <hier>/w (siehe lauf_privat.sh) */
#define _GNU_SOURCE
#include "../../kern/kern.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static arena *A0; static const char *W; static kern *K;
static sliste prot;
static bool kaputt_nach_geloescht = false;   /* NEU 1.0.10: "Zurückwechseln ging nicht" gleich nach dem letzten gelöschten Eintrag */
static bool kaputt_beim_warten = false;      /* NEU 1.0.10 (Codex-Prüfung 147): ... erst im Nachlauf (das Warten vor "Tool wieder da?") */
static int warte_n = 0;
static void warte_test(void *c, int ms) { (void)c; (void)ms; warte_n++; if (kaputt_beim_warten) zugriff_rechte_kaputt = true; }
static void melde_merken(void *c, const char *t)
{
    (void)c; sl_add(A0, &prot, a_kopie(A0, t ? t : ""));
    if (kaputt_nach_geloescht && t && !strncmp(t, "  gelöscht   ", 14)) zugriff_rechte_kaputt = true;
}
static bool prot_hat(const char *s) { for (int i = 0; i < prot.n; i++) if (strstr(prot.e[i], s)) return true; return false; }
static bool nein(void *c) { (void)c; return false; }

/* ---- der nachgestellte Wechsel */
static int wechsel_n = 0, abgelehnt_n = 0, kaputt_ab = -1;
static char wo[256][1024]; static int wo_n = 0;
static void wo_merken(int dfd, const char *name)
{
    char p[900], l[64]; snprintf(l, sizeof l, "/proc/self/fd/%d", dfd);
    ssize_t n = readlink(l, p, sizeof p - 1); if (n < 0) return; p[n] = 0;
    if (wo_n < 256) snprintf(wo[wo_n++], sizeof wo[0], "%s%s%s", p, name ? "/" : "", name ? name : "");
}
static const char *tausch_ziel = NULL, *tausch_von = NULL;   /* Codex-Prüfung 184/185: beim Wechsel Nr. tausch_bei das Ziel gegen tausch_von tauschen */
static int tausch_bei = 0;
static const char *ablehnen_name = NULL; static int ablehnen_nr = 0, ablehnen_treffer = 0;   /* Codex-Prüfung 194: ab dem ablehnen_nr-ten Wechsel bei ablehnen_name jeden Wechsel ablehnen */
static bool pc_als_besitzer(int dfd, const char *name, unsigned uid, bool (*tu)(void *c), void *c)
{
    wechsel_n++; wo_merken(dfd, name);
    if (tausch_ziel && wechsel_n >= tausch_bei) {
        const char *t = tausch_ziel; tausch_ziel = NULL;
        if (rename(VERB(A0, W, t), VERB(A0, W, t, ".alt")) != 0 || rename(VERB(A0, W, tausch_von), VERB(A0, W, t)) != 0) printf("INFO Tausch nicht möglich\n");
    }
    if (ablehnen_name && name && !strcmp(name, ablehnen_name)) ablehnen_treffer++;
    if (ablehnen_name && ablehnen_treffer >= ablehnen_nr) { abgelehnt_n++; errno = EPERM; return false; }
    if (uid != (unsigned)getuid()) { abgelehnt_n++; return false; }
    struct stat st;
    if ((name ? fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW) : fstat(dfd, &st)) != 0 || S_ISLNK(st.st_mode) || st.st_uid != uid) { abgelehnt_n++; return false; }
    mode_t alt = st.st_mode & 07777;
    if ((name ? fchmodat(dfd, name, alt | 0700, 0) : fchmod(dfd, alt | 0700)) != 0) { abgelehnt_n++; return false; }
    bool ok = tu(c);
    int e = errno;
    if (name) { if (fchmodat(dfd, name, alt, 0)) {} } else if (fchmod(dfd, alt)) {}   /* (ist der Eintrag weg, gibt es nichts zurückzustellen) */
    if (kaputt_ab >= 0 && wechsel_n > kaputt_ab) zugriff_rechte_kaputt = true;   /* "Zurückwechseln ging nicht" nachgestellt */
    errno = e;
    return ok;
}

static int modus(const char *p) { struct stat st; if (lstat(VERB(A0, W, p), &st) != 0) return -1; return (int)(st.st_mode & 07777); }
static bool da(const char *p) { return modus(p) >= 0; }
static bool hat_name(const kdliste *l, const char *n) { for (int i = 0; i < l->n; i++) if (!strcmp(l->e[i].name, n)) return true; return false; }
static bool hat(const eintrag *f, const char *p) { for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], p)) return true; return false; }
static eintrag *mit_pfad(const char *p) { for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (hat(f, p)) return f; } return NULL; }
/* Eintrag, dessen Pfad p ist oder enthält */
static eintrag *um_pfad(const char *p)
{
    size_t lp = strlen(p);
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->Pfade.n; j++) { const char *q = f->Pfade.e[j]; size_t l = strlen(q); if (!strncmp(p, q, l) && (p[l] == 0 || p[l] == '/') && l <= lp) return f; } }
    return NULL;
}
static void funde_zeigen(const char *wo_)
{
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i];
        for (int j = 0; j < f->Pfade.n; j++) if (strstr(f->Pfade.e[j], wo_)) { printf("INFO Kat=%d Gesperrt=%d Privat=%d %s :: %s\n", f->Kat, f->Gesperrt, f->Privat, f->Text, f->Pfade.e[j]); break; }
    }
}
static bool alle_wechsel_erlaubt(void)
{
    for (int i = 0; i < wo_n; i++) {
        const char *p = strstr(wo[i], "/w/");
        if (!p || (strncmp(p, "/w/user/data/", 13) && strncmp(p, "/w/mnt/ext1/homebrew/", 21))) { printf("INFO Wechsel außerhalb: %s\n", wo[i]); return false; }
    }
    return true;
}

static int test_zugriff(void)
{
    printf("== Zugriffsschicht: private Ordner als Besitzer lesen und löschen\n");
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    zugriff *z = zugriff_datei(&o);
    arena *a = A0; kdliste l; antwort r; const unsigned char *dd; size_t dn;
    const char *RS = "/user/data/playden/runtime-state";
    zugriff_besitzer_bekannt = kern_ordner_bekannt;   /* sonst setzt das kern_neu */
    zugriff_als_besitzer = NULL;
    r = z->liste(z, a, RS, &l);
    pruefe(r.code >= 400, a_fmt(A0, "ohne Wechsel: privater Ordner nicht lesbar wie bis 1.0.8 (%s)", r.text));
    pruefe(z->befehl(z, a, "SITE PRIVAT?", NULL).code == 502, "ohne Wechsel: SITE PRIVAT? = 502");
    zugriff_als_besitzer = pc_als_besitzer;
    pruefe(z->befehl(z, a, "SITE PRIVAT?", NULL).code == 200, "mit Wechsel: SITE PRIVAT? = 200");
    r = z->cwd(z, a, RS);
    pruefe(r.code == 250 && modus(RS) == 0, a_fmt(A0, "CWD privater Ordner: als Besitzer da (%s), Rechte danach wieder 000 (%o)", r.text, modus(RS)));
    int vorher = wechsel_n;
    r = z->liste(z, a, RS, &l);
    pruefe(r.code == 226 && hat_name(&l, "state.json") && hat_name(&l, "requests") && hat_name(&l, "trash"), a_fmt(A0, "LIST privater Ordner: als Besitzer vollständig (%s, %d Einträge)", r.text, l.n));
    pruefe(wechsel_n == vorher + 1 && modus(RS) == 0, a_fmt(A0, "LIST: genau ein Wechsel (%d), Rechte danach wieder 000", wechsel_n - vorher));
    r = z->liste(z, a, "/user/data/playden/runtime-state/requests", &l);
    pruefe(r.code == 226 && hat_name(&l, "r1.json"), "LIST eines normalen Ordners im privaten Ordner: als Besitzer des privaten Ordners");
    r = z->kopf(z, a, "/user/data/playden/user/steam.vdf", 12, &dd, &dn);
    pruefe(r.code == 226 && dn == 12 && !memcmp(dd, "\"SteamLogin\"", 12) && modus("/user/data/playden/user/steam.vdf") == 0, "KOPF private Datei (000) in einem Ordner mit Sticky-Bit: als Besitzer gelesen, Rechte wieder 000");
    r = z->lies(z, a, "/user/data/playden/runtime-state/state.json", &dd, &dn);
    pruefe(r.code == 226 && dn == 300 && dd[0] == 'x', a_fmt(A0, "RETR Datei im privaten Ordner: ganz gelesen (%zu Byte)", dn));
    bool ja = false;
    r = z->enthaelt(z, a, "/user/data/playden/art/a.png", "PNG", 3, &ja);
    pruefe(r.code == 226 && ja, "ENTHAELT Datei im privaten Ordner art: als Besitzer gelesen");
    unsigned char puf[8]; size_t pn = 0;
    r = z->lies_ab(z, a, "/user/data/playden/art/a.png", 1, 3, puf, &pn);
    pruefe(r.code == 226 && pn == 3 && !memcmp(puf, "PNG", 3), "LIES_AB Datei im privaten Ordner: als Besitzer gelesen");
    /* nur im erlaubten Bereich */
    vorher = wechsel_n;
    r = z->liste(z, a, "/user/home/10000000/privat", &l);
    pruefe(r.code >= 400 && wechsel_n == vorher, "/user/home (Sony-Bereich): kein Wechsel - bleibt nicht lesbar");
    r = z->liste(z, a, "/mnt/ext1/user/privat", &l);
    pruefe(r.code >= 400 && wechsel_n == vorher, "/mnt/ext1/user (Sony-Bereich der M.2): kein Wechsel - bleibt nicht lesbar");
    r = z->lies(z, a, "/user/home/10000000/privat/x", &dd, &dn);
    pruefe(r.code >= 400 && wechsel_n == vorher, "Datei im Sony-Bereich: kein Wechsel");
    r = z->liste(z, a, "/user/data/playden/zeiger", &l);
    pruefe(r.code >= 400 && wechsel_n == vorher, "Verknüpfung auf den privaten Ordner: nie verfolgt, kein Wechsel");
    r = z->liste(z, a, "/mnt/ext1/homebrew/privatapp/cache", &l);
    pruefe(r.code >= 400 && wechsel_n == vorher, "NEU 1.0.10: unbekanntes Programm (privatapp): kein Wechsel - bleibt nicht lesbar");
    r = z->liste(z, a, "/mnt/ext1/homebrew/snes9x/cache", &l);
    pruefe(r.code == 226 && hat_name(&l, "s.bin") && wechsel_n == vorher + 1, "M.2 außerhalb von user, bekanntes Programm (snes9x): privater Ordner als Besitzer lesbar");
    /* NEU 1.0.10 (Codex-Prüfung 126, P1): Verknüpfungen IM privaten Ordner nie verfolgen - auch nicht als Besitzer */
    r = z->liste(z, a, RS, &l);
    pruefe(r.code == 226 && hat_name(&l, "aussen") && hat_name(&l, "g.lnk"), "LIST zeigt die Verknüpfungen im privaten Ordner (als Verknüpfung)");
    r = z->liste(z, a, "/user/data/playden/runtime-state/aussen", &l);
    pruefe(r.code >= 400 && !hat_name(&l, "g.txt"), a_fmt(A0, "LIST über eine Verknüpfung im privaten Ordner nach draußen: abgelehnt (%s)", r.text));
    r = z->cwd(z, a, "/user/data/playden/runtime-state/aussen");
    pruefe(r.code >= 400, "CWD über eine Verknüpfung im privaten Ordner: abgelehnt");
    r = z->lies(z, a, "/user/data/playden/runtime-state/g.lnk", &dd, &dn);
    pruefe(r.code >= 400 && !(dn >= 6 && !memcmp(dd, "geheim", 6)), "RETR einer Verknüpfung im privaten Ordner: abgelehnt, nichts von draußen gelesen");
    r = z->lies(z, a, "/user/data/playden/runtime-state/aussen/g.txt", &dd, &dn);
    pruefe(r.code >= 400 && !(dn >= 6 && !memcmp(dd, "geheim", 6)), "RETR durch eine Verknüpfung im privaten Ordner hindurch: abgelehnt");
    r = z->kopf(z, a, "/user/data/playden/runtime-state/aussen/g.txt", 6, &dd, &dn);
    pruefe(r.code >= 400, "KOPF durch eine Verknüpfung im privaten Ordner hindurch: abgelehnt");
    /* nicht der eingetragene Besitzer -> abgelehnt, nichts passiert */
    r = z->befehl(z, a, "SITE PRIVAT-LISTE?", NULL);
    printf("INFO Liste:\n%s", r.text);
    pruefe(r.code == 200 && strstr(r.text, " /user/data/playden/runtime-state\n") && strstr(r.text, " /user/data/playden/user/steam.vdf\n") && strstr(r.text, " /user/data/playden/art\n")
           && strstr(r.text, " /mnt/ext1/homebrew/snes9x/cache\n") && !strstr(r.text, "privatapp"), "SITE PRIVAT-LISTE?: alle gelesenen privaten Orte (nichts vom unbekannten Programm)");
    pruefe(!strstr(r.text, "runtime-state/requests") && !strstr(r.text, "/user/home") && !strstr(r.text, "/mnt/ext1/user"), "Liste: Ordner im privaten Ordner zählt beim privaten Ordner, nichts aus Sony-Bereichen");
    pruefe(alle_wechsel_erlaubt(), "jeder Wechsel lag unter /user/data bzw. auf der M.2 außerhalb von user");
    r = z->befehl(z, a, "SITE PRIVAT-NEU", NULL);
    antwort r2 = z->befehl(z, a, "SITE PRIVAT-LISTE?", NULL);
    pruefe(r.code == 200 && !strcmp(r2.text, "200 0 0\n"), "SITE PRIVAT-NEU: Liste leer");
#ifdef KERN_LOESCHEN
    {   /* NEU 1.0.10 (PS5-Test 12.11.2026 19:49): Playdens Installationsliste liegt im privaten Ordner - prüfen und austragen gehen denselben
         * Weg wie das Lesen (über den Pfad, in einem Durchgang als Besitzer); die neue Datei gehört wieder dem Besitzer, Rechte bleiben */
        const char *IJ = "/user/data/playden/runtime-state/installs.json";
        r = z->lies(z, a, IJ, &dd, &dn); const char *vorher_ij = a_kopie_n(A0, (const char *)dd, dn);
        setenv("PL_KEIN_SCHREIBEN", "1", 1);
        antwort rk = z->befehl(z, a, "SITE PLAYDEN-PRUEFEN", "FAKE78050:5050");
        unsetenv("PL_KEIN_SCHREIBEN");
        r = z->lies(z, a, IJ, &dd, &dn);
        pruefe(rk.code == 550 && strstr(rk.text, "ließe sich nicht ändern") && dn == strlen(vorher_ij) && !memcmp(dd, vorher_ij, dn) && modus(RS) == 0,
               a_fmt(A0, "Vorprüfung beweist das Schreiben: geht es nicht, 550 vor dem Löschen, Liste unverändert (%s)", rk.text));
        /* NEU 1.0.10 (PS5-Test 13.11.2026 11:56, Hitman: "kein freier Name für die Zwischendatei"): belegte Namen der Zwischendatei - einer
         * belegt: der nächste wird genommen, der belegte bleibt unberührt; alle 16 belegt: 550 mit dem echten Grund, Liste unverändert */
        const char *Z0 = a_fmt(A0, "%s/runtime-state/.installs.json.ps5cleanup-%ld-", "/user/data/playden", (long)getpid());
        bool belegt_ok = chmod(VERB(A0, W, RS), 0700) == 0;
        for (int v = 0; v < 16 && belegt_ok; v++) { FILE *bf = fopen(VERB(A0, W, Z0, a_fmt(A0, "%d", v)), "w"); if (!bf) belegt_ok = false; else { fputs("fremd", bf); fclose(bf); } }
        chmod(VERB(A0, W, RS), 0);
        antwort rb = z->befehl(z, a, "SITE PLAYDEN-PRUEFEN", "FAKE78050:5050");
        r = z->lies(z, a, IJ, &dd, &dn);
        pruefe(belegt_ok && rb.code == 550 && strstr(rb.text, "kein freier Name") && strstr(rb.text, "exists") && dn == strlen(vorher_ij) && !memcmp(dd, vorher_ij, dn) && modus(RS) == 0,
               a_fmt(A0, "alle 16 Namen der Zwischendatei belegt: 550 mit echtem Grund, Liste unverändert (%s)", rb.text));
        chmod(VERB(A0, W, RS), 0700);
        for (int v = 1; v < 16; v++) unlink(VERB(A0, W, Z0, a_fmt(A0, "%d", v)));
        chmod(VERB(A0, W, RS), 0);
        antwort rp = z->befehl(z, a, "SITE PLAYDEN-PRUEFEN", "FAKE78050:5050");
        r = z->lies(z, a, IJ, &dd, &dn);
        chmod(VERB(A0, W, RS), 0700);
        const unsigned char *zd = NULL; size_t zn = 0; FILE *zf = fopen(VERB(A0, W, Z0, "0"), "r"); char zb[16] = {0};
        if (zf) { zn = fread(zb, 1, sizeof zb - 1, zf); fclose(zf); zd = (const unsigned char *)zb; }
        bool z1_weg = access(VERB(A0, W, Z0, "1"), F_OK) != 0;
        unlink(VERB(A0, W, Z0, "0"));
        chmod(VERB(A0, W, RS), 0);
        pruefe(rp.code == 200 && zd && zn == 5 && !memcmp(zd, "fremd", 5) && z1_weg && modus(RS) == 0 && dn == strlen(vorher_ij) && !memcmp(dd, vorher_ij, dn),
               a_fmt(A0, "erster Name belegt: der nächste wird genommen, der belegte bleibt unberührt, keine Zwischendatei übrig (%s)", rp.text));
        pruefe(rp.code == 200 && modus(RS) == 0 && dn == strlen(vorher_ij) && !memcmp(dd, vorher_ij, dn),
               a_fmt(A0, "SITE PLAYDEN-PRUEFEN im privaten Ordner: austragbar, nichts verändert, Ordner wieder 000 (%s)", rp.text));
        antwort ra = z->befehl(z, a, "SITE PLAYDEN-AUSTRAGEN", "FAKE78050:5050");
        r = z->lies(z, a, IJ, &dd, &dn);
        const char *soll = "{\"schema_version\":1,\"reservations\":[],\"installs\":[]}";
        bool gleich = r.code == 226 && dn == strlen(soll) && !memcmp(dd, soll, dn);
        r = z->liste(z, a, RS, &l);
        bool rest = false; for (int i = 0; i < l.n; i++) if (strstr(l.e[i].name, "ps5cleanup")) rest = true;
        r = z->lies(z, a, "/user/data/playden-games/.playden-games", &dd, &dn);
        bool zeile = r.code == 226 && dn == 13 && !memcmp(dd, "7\tinternal\t5\n", 13);
        struct stat sj; bool modus_ok = false;
        if (chmod(VERB(A0, W, RS), 0700) == 0) { modus_ok = stat(VERB(A0, W, IJ), &sj) == 0 && (sj.st_mode & 07777) == 0600 && sj.st_uid == getuid(); chmod(VERB(A0, W, RS), 0); }
        if (!(gleich && zeile && !rest && modus_ok)) printf("INFO gleich %d zeile %d rest %d modus %d\n", gleich, zeile, rest, modus_ok);
        pruefe(ra.code == 200 && gleich && zeile && !rest && modus(RS) == 0 && modus_ok,
               a_fmt(A0, "SITE PLAYDEN-AUSTRAGEN im privaten Ordner: Eintrag und Zeile weg, Rest gleich, keine Zwischendatei, Rechte 0600 wie vorher (%s)", ra.text));
    }
    {   /* NEU 1.0.10 (da_Chris86 22:40; PS5-Tests 12:46 und 19:49): auf der PS5 gelingt der Wechsel je Einzelschritt nicht (hier nachgestellt mit
         * PRIVAT_NUR_DURCHGANG) - DELE, RMD und RMDA im privaten Ordner gehen dann in EINEM Durchgang als Besitzer (privat_weg), wie das Lesen */
        setenv("PRIVAT_NUR_DURCHGANG", "1", 1);
        const char *KP = "/user/data/playden/games/steam-223850.kept";
        int w0 = wechsel_n;
        r = z->befehl(z, a, "RMDA", KP);
        pruefe(r.code == 250 && !da(KP) && wechsel_n == w0 + 1, a_fmt(A0, "nur Durchgang: RMDA aufgehobene Spielstände (privat, verschachtelt) ganz weg in EINEM Wechsel (%s, %d Wechsel)", r.text, wechsel_n - w0));
        r = z->befehl(z, a, "DELE", "/user/data/playden/runtime-state/requests/r1.json");
        pruefe(r.code == 250 && !da("/user/data/playden/runtime-state/requests/r1.json") && modus(RS) == 0, a_fmt(A0, "nur Durchgang: DELE Datei hinter dem privaten Ordner weg, Ordner wieder 000 (%s)", r.text));
        r = z->befehl(z, a, "RMD", "/user/data/playden/runtime-state/requests");
        pruefe(r.code == 250 && !da("/user/data/playden/runtime-state/requests") && modus(RS) == 0, a_fmt(A0, "nur Durchgang: RMD leerer Ordner hinter dem privaten Ordner weg (%s)", r.text));
        w0 = wechsel_n;
        r = z->befehl(z, a, "DELE", "/user/data/playden/runtime-state/aussen/g.txt");
        pruefe(r.code >= 400 && access(VERB(A0, W, "/../aussen/g.txt"), F_OK) == 0, "nur Durchgang: durch eine Verknüpfung im privaten Ordner nie gelöscht, das Ziel draußen bleibt");
        r = z->befehl(z, a, "RMDA", "/user/home/10000000/privat");
        pruefe(r.code >= 400 && modus("/user/home/10000000/privat") == 0, "nur Durchgang: Sony-Bereich bleibt (kein Wechsel, nichts gelöscht)");
        w0 = wechsel_n;
        r = z->befehl(z, a, "RMDA", "/user/data/gamecompressor");
        pruefe(r.code == 250 && !da("/user/data/gamecompressor") && wechsel_n == w0 + 1, a_fmt(A0, "nur Durchgang: Ordner des Programms ist selbst privat - ganz weg in EINEM Wechsel (Codex-Prüfung 172 (3); %s)", r.text));
        r = z->befehl(z, a, "RMDA", "/mnt/ext1/homebrew/privatapp/cache");
        pruefe(r.code >= 400 && da("/mnt/ext1/homebrew/privatapp/cache"), "nur Durchgang: Ordner eines unbekannten Programms bleibt");
        unsetenv("PRIVAT_NUR_DURCHGANG");
    }
    {   /* NEU 1.0.10 (PS5-Test 13.11.2026 08:19): der Ordner lässt sich öffnen, das Geöffnete ist aber unbrauchbar (fstat: EBADF) -
         * gilt als fehlende Rechte: als sein Besitzer in EINEM Durchgang (vorher: Sicherheitsstopp, 0 Byte gelöscht) */
        const char *K3 = "/user/data/playden/games/steam-77777.kept", *K4 = "/user/data/playden/games/steam-88888.kept";
        setenv("OFFEN_UNBRAUCHBAR", "steam-77777.kept", 1);
        int w0 = wechsel_n;
        r = z->befehl(z, a, "RMDA", K3);
        pruefe(r.code == 250 && !da(K3) && wechsel_n == w0 + 1, a_fmt(A0, "unbrauchbar geöffnet (Ziel selbst): RMDA ganz weg in EINEM Wechsel (%s, %d Wechsel)", r.text, wechsel_n - w0));
        setenv("OFFEN_UNBRAUCHBAR", "innen", 1);
        w0 = wechsel_n;
        r = z->befehl(z, a, "RMDA", K4);
        pruefe(r.code == 250 && !da(K4) && wechsel_n == w0 + 2, a_fmt(A0, "unbrauchbar geöffnet (Teil darin): als Besitzer geprüft und geleert, dann alles weg (%s, %d Wechsel)", r.text, wechsel_n - w0));
        pruefe(da("/user/data/playden/games/PPSA12345") && da("/user/data/playden/config.json"), "unbrauchbar geöffnet: Nachbarn im Ordner games und Playdens Einstellungen bleiben");
        /* Codex-Prüfung 184 (P1): zwischen dem unbrauchbaren Öffnen und dem Durchgang als Besitzer wird ein anderer Ordner unter den Namen
         * geschoben - er darf nicht gelöscht werden (Identität wie gesehen) und es gibt keinen zweiten Versuch ohne sie */
        const char *K5 = "/user/data/playden/games/steam-55555.kept";
        setenv("OFFEN_UNBRAUCHBAR", "steam-55555.kept", 1);
        tausch_ziel = K5; tausch_von = "/user/data/playden/games/tausch-b"; tausch_bei = 0; w0 = wechsel_n;
        r = z->befehl(z, a, "RMDA", K5);
        pruefe(r.code >= 400 && da(VERB(A0, K5, "/b.txt")) && da(VERB(A0, K5, ".alt/a.txt")) && wechsel_n == w0 + 1,
               a_fmt(A0, "unbrauchbar geöffnet, dann getauscht: der fremde Ordner bleibt, der gewählte auch, nur EIN Versuch (%s, %d Wechsel)", r.text, wechsel_n - w0));
        tausch_ziel = NULL;
        /* Codex-Prüfung 185 (P1): ein Teil darin (unbrauchbar geöffnet) lässt sich auch als Besitzer nicht leeren (Unterordner 0555) -
         * der zweite Versuch für den ganzen Befehl darf nur das gewählte Ziel löschen; wird es vorher getauscht, bleibt der fremde Ordner */
        const char *K6 = "/user/data/playden/games/steam-66666.kept";
        setenv("OFFEN_UNBRAUCHBAR", "innen", 1);
        w0 = wechsel_n; tausch_ziel = K6; tausch_von = "/user/data/playden/games/tausch-c"; tausch_bei = w0 + 3;
        r = z->befehl(z, a, "RMDA", K6);
        pruefe(r.code >= 400 && da(VERB(A0, K6, "/c.txt")) && da(VERB(A0, K6, ".alt/innen/ro/r.txt")) && wechsel_n == w0 + 3,
               a_fmt(A0, "Teil darin scheitert auch als Besitzer, dann Ziel getauscht: zweiter Versuch nur mit der gesehenen Identität - der fremde Ordner bleibt (%s, %d Wechsel)", r.text, wechsel_n - w0));
        tausch_ziel = NULL;
        /* NEU 1.0.10 (PS5-Test 13.11.2026 12:25, Playden ganz löschen): privater Ordner lässt sich öffnen, das Geöffnete zeigt aber ANDERE
         * Angaben als eben angesehen - wie "unbrauchbar" als sein Besitzer (mit dem angesehenen Ziel), statt Sicherheitsstopp */
        const char *K8 = "/user/data/playden/games/steam-33333.kept", *K9 = "/user/data/playden/games/steam-33334.kept", *K10 = "/user/data/playden/games/steam-33335.kept";
        unsetenv("OFFEN_UNBRAUCHBAR");
        setenv("OFFEN_ANDERS", "innen3", 1);
        w0 = wechsel_n;
        r = z->befehl(z, a, "RMDA", K8);
        pruefe(r.code == 250 && !da(K8) && wechsel_n == w0 + 2, a_fmt(A0, "geöffnet mit anderen Angaben (Teil darin): als Besitzer geprüft und geleert, dann alles weg (%s, %d Wechsel)", r.text, wechsel_n - w0));
        setenv("OFFEN_ANDERS", "steam-33334.kept", 1);
        w0 = wechsel_n;
        r = z->befehl(z, a, "RMDA", K9);
        pruefe(r.code == 250 && !da(K9) && wechsel_n == w0 + 1, a_fmt(A0, "geöffnet mit anderen Angaben (Ziel selbst): als Besitzer in EINEM Wechsel weg (%s, %d Wechsel)", r.text, wechsel_n - w0));
        setenv("OFFEN_ANDERS", "steam-33335.kept", 1);
        w0 = wechsel_n; tausch_ziel = K10; tausch_von = "/user/data/playden/games/tausch-e"; tausch_bei = 0;
        r = z->befehl(z, a, "RMDA", K10);
        pruefe(r.code >= 400 && da(VERB(A0, K10, "/e.txt")) && da(VERB(A0, K10, ".alt/k.txt")) && wechsel_n == w0 + 1,
               a_fmt(A0, "geöffnet mit anderen Angaben, dann wirklich getauscht: der fremde Ordner bleibt, der gewählte auch (%s, %d Wechsel)", r.text, wechsel_n - w0));
        tausch_ziel = NULL;
        unsetenv("OFFEN_ANDERS");
        {   /* NEU 1.0.10 (PS5-Test 13.11.2026 23:35, Codex-Prüfung 217): ein fremder privater Ordner wird nie als root betreten - Prüfen und
             * Leeren gleich als sein Besitzer (privat_direkt), als Teil darin und als Ziel selbst */
            const char *K21 = "/user/data/playden/games/steam-21211.kept", *K22 = "/user/data/playden/games/steam-21212.kept";
            setenv("PRIVAT_DIREKT", "innen8", 1);
            w0 = wechsel_n;
            r = z->befehl(z, a, "RMDA", K21);
            pruefe(r.code == 250 && !da(K21) && wechsel_n > w0, a_fmt(A0, "fremder privater Ordner (Teil darin): gleich als Besitzer geprüft und geleert, alles weg (%s, %d Wechsel)", r.text, wechsel_n - w0));
            setenv("PRIVAT_DIREKT", "steam-21212.kept", 1);
            w0 = wechsel_n;
            r = z->befehl(z, a, "RMDA", K22);
            pruefe(r.code == 250 && !da(K22) && wechsel_n > w0, a_fmt(A0, "fremder privater Ordner (Ziel selbst): gleich als Besitzer weg (%s, %d Wechsel)", r.text, wechsel_n - w0));
            unsetenv("PRIVAT_DIREKT");
        }
        /* NEU 1.0.10 (PS5-Test 13.11.2026 12:50, Rest von /data/playden): eine Datei im Ordner lässt sich mit vollen Rechten nicht ansehen,
         * ohne Grund (errno 0) - der Ordner darüber wird als sein Besitzer geprüft und gelöscht, statt Sicherheitsstopp */
        const char *K11 = "/user/data/playden/games/steam-11111.kept";
        setenv("STAT_OHNE_GRUND", "sid", 1);
        w0 = wechsel_n;
        r = z->befehl(z, a, "RMDA", K11);
        pruefe(r.code == 250 && !da(K11) && wechsel_n == w0 + 2, a_fmt(A0, "Datei ohne Grund nicht ansehbar: Ordner darüber als Besitzer geprüft und gelöscht, alles weg (%s, %d Wechsel)", r.text, wechsel_n - w0));
        unsetenv("STAT_OHNE_GRUND");
        /* NEU 1.0.10 (PS5-Test 13.11.2026 12:58): nach dem Leeren als Besitzer lässt sich der private Ordner nicht mehr ansehen - der Besitzer
         * des Ordners darüber entfernt den leeren Ordner (mit der gesehenen Identität), statt Sicherheitsstopp */
        const char *K12 = "/user/data/playden/games/steam-12121.kept";
        setenv("OFFEN_UNBRAUCHBAR", "innen5", 1); setenv("LEER_OHNE_GRUND", "innen5", 1);
        setenv("BLOCK_OHNE_GRUND", "innen5", 1);   /* Codex-Prüfung 192 (2): auch der Weg dorthin sieht das Ziel als root nicht */
        w0 = wechsel_n;
        r = z->befehl(z, a, "RMDA", K12);
        pruefe(r.code == 250 && !da(K12) && wechsel_n == w0 + 3, a_fmt(A0, "geleerter privater Ordner nicht mehr ansehbar: als Besitzer des Ordners darüber entfernt, alles weg (%s, %d Wechsel)", r.text, wechsel_n - w0));
        unsetenv("LEER_OHNE_GRUND"); unsetenv("OFFEN_UNBRAUCHBAR"); unsetenv("BLOCK_OHNE_GRUND");
        /* Codex-Prüfung 193 (1): wie K12, aber der Ordner darüber gehört root - der Besitzer des Kinds darf den leeren Ordner nicht entfernen,
         * root entfernt ihn dann ohne Wechsel */
        const char *K13 = "/user/data/playden/games/steam-13131.kept";
        setenv("OFFEN_UNBRAUCHBAR", "innen6", 1); setenv("LEER_OHNE_GRUND", "innen6", 1);
        setenv("BLOCK_OHNE_GRUND", "innen6", 1); setenv("ELTERN_ROOT", "innen6", 1);
        w0 = wechsel_n;
        r = z->befehl(z, a, "RMDA", K13);
        pruefe(r.code == 250 && !da(K13) && wechsel_n == w0 + 3, a_fmt(A0, "geleerter privater Ordner, Ordner darüber gehört root: root entfernt den leeren Ordner, alles weg (%s, %d Wechsel)", r.text, wechsel_n - w0));
        unsetenv("LEER_OHNE_GRUND"); unsetenv("OFFEN_UNBRAUCHBAR"); unsetenv("BLOCK_OHNE_GRUND"); unsetenv("ELTERN_ROOT");
        /* Codex-Prüfung 194: wie K13, aber der Wechsel zum Entfernen (art 1) wird abgelehnt, bevor der Besitzer die Identität prüfen kann (z. B.
         * eingetauschter Ordner eines anderen Besitzers) - root entfernt dann NICHTS, Stopp; der geleerte Ordner bleibt */
        const char *K14 = "/user/data/playden/games/steam-14141.kept";
        setenv("OFFEN_UNBRAUCHBAR", "innen7", 1); setenv("LEER_OHNE_GRUND", "innen7", 1); setenv("BLOCK_OHNE_GRUND", "innen7", 1);
        ablehnen_name = "innen7"; ablehnen_nr = 3;   /* 1. Prüfen (art 3), 2. Leeren (art 4), 3. Entfernen (art 1) */ ablehnen_treffer = 0;
        r = z->befehl(z, a, "RMDA", K14);
        pruefe(r.code != 250 && da(VERB(A0, K14, "/innen7")) && !da(VERB(A0, K14, "/innen7/t")), a_fmt(A0, "Wechsel zum Entfernen abgelehnt (Identität ungeprüft): root entfernt nichts, geleerter Ordner bleibt (%d %s)", r.code, r.text));
        ablehnen_name = NULL;
        unsetenv("LEER_OHNE_GRUND"); unsetenv("OFFEN_UNBRAUCHBAR"); unsetenv("BLOCK_OHNE_GRUND");
        setenv("OFFEN_UNBRAUCHBAR", "innen", 1);
        unsetenv("OFFEN_UNBRAUCHBAR");
    }
    /* löschen */
    r = z->befehl(z, a, "SITE CHMOD 777", RS);
    pruefe(r.code >= 400 && modus(RS) == 0, "SITE CHMOD auf den privaten Ordner: nie Rechte ändern (bleibt 000)");
    r = z->befehl(z, a, "DELE", "/user/data/playden/runtime-state/state.json");
    pruefe(r.code == 250 && !da("/user/data/playden/runtime-state/state.json") && modus(RS) == 0, a_fmt(A0, "DELE Datei im privaten Ordner: weg (%s), Ordner wieder 000", r.text));
    r = z->befehl(z, a, "RMD", "/user/data/playden/runtime-state/trash");
    pruefe(r.code == 250 && !da("/user/data/playden/runtime-state/trash"), "RMD leerer Ordner im privaten Ordner: weg");
    r = z->befehl(z, a, "RMDA", "/user/data/playden/art");
    pruefe(r.code == 250 && !da("/user/data/playden/art"), a_fmt(A0, "RMDA privater Ordner samt Inhalt: weg (%s)", r.text));
    vorher = wechsel_n;
    r = z->befehl(z, a, "DELE", "/user/home/10000000/privat/x");
    pruefe(r.code >= 400 && wechsel_n == vorher && modus("/user/home/10000000/privat") == 0, "DELE im Sony-Bereich: kein Wechsel, nichts gelöscht");
    r = z->befehl(z, a, "RMDA", "/mnt/ext1/user/privat");
    pruefe(r.code >= 400 && wechsel_n == vorher, "RMDA im Sony-Bereich der M.2: kein Wechsel, nichts gelöscht");
    r = z->befehl(z, a, "RMDA", "/user/data/playden");
    pruefe(r.code == 250 && !da("/user/data/playden"), a_fmt(A0, "RMDA ganzer Ordner des Programms mit privaten Ordnern und privater Datei: ganz weg (%s)", r.text));
    r = z->befehl(z, a, "SITE PRIVAT-LISTE?", NULL);
    printf("INFO Liste nach dem Löschen:\n%s", r.text);
    pruefe(strstr(r.text, "W ") && strstr(r.text, " /user/data/playden\n") && strstr(r.text, " /user/data/playden/art\n"), "Liste: gelöscht als Besitzer (W) je Ziel des Befehls");
    pruefe(access(VERB(A0, W, "/../aussen/g.txt"), F_OK) == 0, "Verknüpfungen im privaten Ordner: nur die Verknüpfung weg, das Ziel draußen bleibt");
    /* NEU 1.0.10 (Codex-Prüfung 126, P2): bloßes Öffnen ist kein Löschen - ein gescheitertes RMD meldet kein W */
    z->befehl(z, a, "SITE PRIVAT-NEU", NULL);
    vorher = wechsel_n;
    r = z->befehl(z, a, "RMD", "/mnt/ext1/homebrew/snes9x/cache/s.bin");   /* der Weg führt durch den privaten Ordner (als Besitzer geöffnet), RMD auf eine Datei scheitert */
    r2 = z->befehl(z, a, "SITE PRIVAT-LISTE?", NULL);
    pruefe(r.code >= 400 && wechsel_n > vorher && da("/mnt/ext1/homebrew/snes9x/cache") && !strstr(r2.text, "W "), a_fmt(A0, "RMD auf eine Datei im privaten Ordner: scheitert, Ordner als Besitzer nur geöffnet - kein \"gelöscht\" in der Liste (%s)", r2.text));
    pruefe(alle_wechsel_erlaubt(), "auch beim Löschen jeder Wechsel im erlaubten Bereich");
    /* Rechte kaputt: danach verändert die Schicht nichts mehr */
    zugriff_rechte_kaputt = true;
    r = z->befehl(z, a, "RMDA", "/mnt/ext1/homebrew/privatapp");
    pruefe(r.code >= 400 && da("/mnt/ext1/homebrew/privatapp/cache") && modus("/mnt/ext1/homebrew/privatapp/cache") == 0, "Zurückwechseln ging nicht: kein Löschbefehl mehr");
    pruefe(z->befehl(z, a, "SITE PRIVAT?", NULL).code == 502, "Zurückwechseln ging nicht: SITE PRIVAT? = 502");
    /* NEU 1.0.10 (Codex-Prüfung 126, P1): auch Sonys verändernde Befehle nicht mehr - Fragen ("?") schon */
    r = z->befehl(z, a, "SITE UNINSTALL", "PPSA12345");
    pruefe(r.code == 550 && strstr(r.text, "Rechte nicht zurueckgestellt"), a_fmt(A0, "Zurückwechseln ging nicht: SITE UNINSTALL abgelehnt (%s)", r.text));
    r = z->befehl(z, a, "SITE MEDIUM", "/user/av_contents/x.mp4");
    pruefe(r.code == 550 && strstr(r.text, "Rechte nicht zurueckgestellt"), "Zurückwechseln ging nicht: SITE MEDIUM abgelehnt");
    r = z->befehl(z, a, "SITE SPIELSTAND", "/user/home/10000000/savedata_prospero/PPSA12345");
    pruefe(r.code == 550 && strstr(r.text, "Rechte nicht zurueckgestellt"), "Zurückwechseln ging nicht: SITE SPIELSTAND abgelehnt");
    r = z->befehl(z, a, "SITE UNINSTALL?", "PPSA12345");
    pruefe(!strstr(r.text, "Rechte nicht zurueckgestellt"), a_fmt(A0, "Zurückwechseln ging nicht: Fragen gehen weiter (SITE UNINSTALL? -> %s)", r.text));
    zugriff_rechte_kaputt = false;
#endif
    printf("INFO Wechsel %d, abgelehnt %d\n", wechsel_n, abgelehnt_n);
    return 0;
}

static void suchen(zugriff *z)
{
    K = kern_neu(z); K->melde = melde_merken;
    volatile int fe = 0;
    VERSUCHE { kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K); kern_suche_funde(K); }
    FANGE { printf("FAIL Suche: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) { printf("FEHLERZAHL 1\n"); exit(1); }
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: test_privat <hier> zugriff|ohne|kern|m2|sim|kaputt\n"); return 2; }
    A0 = arena_neu();
    W = VERB(A0, argv[1], "/w");
    const char *art = argv[2];
    if (!strcmp(art, "zugriff")) { test_zugriff(); printf("FEHLERZAHL %d\n", fehler); return fehler ? 1 : 0; }
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    const char *PL = "/user/data/playden", *RS = "/user/data/playden/runtime-state";
    if (!strcmp(art, "ohne")) {   /* wie bis 1.0.8: ohne Wechsel bleiben die privaten Ordner geschützt */
        printf("== Kern ohne Wechsel (wie bis 1.0.8)\n");
        zugriff_als_besitzer = NULL;
        suchen(zugriff_datei(&o));
        funde_zeigen("/user/data/playden");
        eintrag *p = mit_pfad(PL);
        pruefe(!p || p->Gesperrt, "ohne Wechsel: der Ordner des Programms wird nicht als Ganzes angeboten");
        pruefe(kern_selbstschutz_unlesbar(K, RS), "ohne Wechsel: runtime-state gilt als nicht lesbar (geschützt)");
        pruefe(!prot_hat("Private Daten"), "ohne Wechsel: keine Zeile zu privaten Daten");
        bool privat = false; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Privat) privat = true;
        pruefe(!privat, "ohne Wechsel: kein Eintrag \"mit privaten Daten\"");
        printf("FEHLERZAHL %d\n", fehler); return fehler ? 1 : 0;
    }
    zugriff_als_besitzer = pc_als_besitzer;
    bool sim = !strcmp(art, "sim");
    zugriff *z = zugriff_datei(&o);
    suchen(sim ? zugriff_sim(z) : z);
    if (sim) K->simulation = true;
    printf("== Kern %s\n", art);
    funde_zeigen("/user/data/playden"); funde_zeigen("/mnt/ext1/homebrew/privatapp"); funde_zeigen("/mnt/ext1/homebrew/snes9x");
    for (int i = 0; i < prot.n; i++) if (strstr(prot.e[i], "rivat") || strstr(prot.e[i], "Benutzer")) printf("PROT %s\n", prot.e[i]);
    eintrag *p = mit_pfad(PL);
    pruefe(p && !p->Gesperrt, "der Ordner des Programms steht als Ganzes da (nicht mehr aufgeklappt, nichts darin geschützt)");
    pruefe(!kern_selbstschutz_unlesbar(K, RS) && !kern_selbstschutz_gesperrt(K, PL), "runtime-state gilt nicht mehr als nicht lesbar, nichts gesperrt");
    pruefe(p && p->Privat && strstr(p->Text, "[mit privaten Daten]"), a_fmt(A0, "Eintrag markiert: %s", p ? p->Text : "-"));
    pruefe(p && strstr(kern_zeile(A0, p), " – mit privaten Daten"), a_fmt(A0, "Zeile: %s", p ? kern_zeile(A0, p) : "-"));
    pruefe(p && strstr(kern_art_info(A0, p), "Private Daten: Das Programm hat hier Ordner angelegt"), "Kasten erklärt, was PS5 Cleanup damit macht");
    pruefe(prot_hat("Private Daten von Programmen gefunden") && prot_hat("   /user/data/playden/runtime-state  (Besitzer-Kennung ") && prot_hat("   /user/data/playden/user/game-settings-private  (Besitzer-Kennung ")
           && prot_hat("   /user/data/playden/art  (Besitzer-Kennung ") && prot_hat("   /user/data/playden/games/PPSA12345  (Besitzer-Kennung "), "Protokoll nennt jeden privaten Ordner mit Besitzer");
    pruefe(K->PRIVAT_ORTE >= 5, a_fmt(A0, "PRIVAT_ORTE = %d", K->PRIVAT_ORTE));
    pruefe(alle_wechsel_erlaubt(), "Suche: jeder Wechsel im erlaubten Bereich");
    eintrag *m = um_pfad("/mnt/ext1/homebrew/snes9x/cache");
    pruefe(m && !m->Gesperrt && m->Privat, a_fmt(A0, "M.2: Eintrag mit dem privaten Ordner markiert (%s)", m ? m->Text : "-"));
    eintrag *u = um_pfad("/mnt/ext1/homebrew/privatapp/cache");   /* NEU 1.0.10: unbekanntes Programm */
    pruefe(kern_selbstschutz_unlesbar(K, "/mnt/ext1/homebrew/privatapp/cache") && !(u && u->Privat), a_fmt(A0, "unbekanntes Programm: privater Ordner bleibt nicht lesbar und geschützt (%s)", u ? u->Text : "-"));
    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
    if (!strcmp(art, "kern") || sim) {
        if (p) p->Wahl = true;
        prot = (sliste){0};
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        for (int i = 0; i < prot.n; i++) printf("PROT %s\n", prot.e[i]);
        printf("ERG Fehler=%d Teilweise=%d Unveraendert=%d\n", e.Fehler, e.Teilweise, e.Unveraendert);
        if (sim) {
            pruefe(e.Fehler == 0 && da(RS) && modus(RS) == 0 && modus("/user/data/playden/art") == 0 && da("/user/data/playden/config.json"), "Testmodus: nichts gelöscht, Rechte unverändert 000");
            pruefe(prot_hat("SIMULATION: Diese privaten Daten würden als Benutzer des Programms gelöscht:") && prot_hat("   /user/data/playden/runtime-state  (Besitzer-Kennung "), "Testmodus: Protokoll sagt, was so gelöscht würde");
            pruefe(prot_hat("würde gelöscht") && prot_hat("[mit privaten Daten]"), "Testmodus: Zeile \"würde gelöscht ... [mit privaten Daten]\"");
        } else {
            pruefe(e.Fehler == 0 && !da(PL), "Löschen: der Ordner des Programms ist ganz weg - samt privater Ordner und privater Datei");
            pruefe(prot_hat("gelöscht") && prot_hat("[mit privaten Daten]"), "Protokoll: \"gelöscht ... [mit privaten Daten]\"");
            pruefe(prot_hat("Als Benutzer des Programms gelöscht (private Daten):") && prot_hat("   /user/data/playden  (Besitzer-Kennung "), "Protokoll: was als Benutzer des Programms gelöscht wurde");
            pruefe(da("/mnt/ext1/homebrew/snes9x/cache") && modus("/mnt/ext1/homebrew/snes9x/cache") == 0, "nicht gewählt (M.2) bleibt, Rechte unverändert 000");
            pruefe(access(VERB(A0, W, "/../aussen/g.txt"), F_OK) == 0, "Ziel der Verknüpfungen draußen bleibt");
            pruefe(alle_wechsel_erlaubt(), "Löschen: jeder Wechsel im erlaubten Bereich");
        }
    } else if (!strcmp(art, "m2")) {
        if (m) m->Wahl = true;
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        printf("ERG Fehler=%d\n", e.Fehler);
        pruefe(e.Fehler == 0 && !da("/mnt/ext1/homebrew/snes9x/cache"), "M.2: privater Ordner mit gelöscht");
        pruefe(da("/mnt/ext1/homebrew/privatapp/cache") && modus("/mnt/ext1/homebrew/privatapp/cache") == 0, "unbekanntes Programm bleibt unverändert");
        pruefe(da(RS) && modus(RS) == 0, "nicht gewählt (Playden) bleibt unverändert");
    } else if (!strcmp(art, "kaputt")) {   /* der erste Wechsel beim Löschen "kommt nicht zurück" */
        if (p) p->Wahl = true;
        if (m) m->Wahl = true;
        kaputt_ab = wechsel_n;
        prot = (sliste){0};
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        for (int i = 0; i < prot.n; i++) printf("PROT %s\n", prot.e[i]);
        printf("ERG Fehler=%d\n", e.Fehler);
        pruefe(e.Fehler >= 1 && prot_hat("SICHERHEITSSTOPP: Nach dem Wechsel auf den Benutzer eines Programms"), "Zurückwechseln ging nicht: Sicherheitsstopp im Protokoll, als Fehler gezählt");
        pruefe(wechsel_n == kaputt_ab + 1, a_fmt(A0, "danach kein einziger Wechsel mehr (%d)", wechsel_n - kaputt_ab));
        pruefe(da(RS) || da("/mnt/ext1/homebrew/snes9x/cache"), "danach nichts mehr gelöscht: mindestens einer der beiden privaten Ordner ist noch da");
        zugriff_rechte_kaputt = false;
    } else if (!strcmp(art, "kaputt_ende")) {   /* NEU 1.0.10 (Codex-Prüfung 126, P2): "kommt nicht zurück" erst nach dem letzten Eintrag */
        if (p) p->Wahl = true;
        prot = (sliste){0};
        kaputt_nach_geloescht = true;
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        kaputt_nach_geloescht = false;
        for (int i = 0; i < prot.n; i++) printf("PROT %s\n", prot.e[i]);
        printf("ERG Fehler=%d RM=%s\n", e.Fehler, K->RM_CODE);
        pruefe(!da(PL), "der Eintrag selbst wurde noch gelöscht");
        pruefe(e.Fehler == 1 && prot_hat("SICHERHEITSSTOPP: Nach dem Wechsel auf den Benutzer eines Programms"), a_fmt(A0, "Sicherheitsstopp nach dem letzten Eintrag: im Protokoll und genau einmal als Fehler gezählt (%d) - kein \"alles gelöscht\", keine Selbstentfernung", e.Fehler));
        pruefe(!strcmp(K->RM_CODE, "g-rechte"), a_fmt(A0, "Rückmeldung: Grund g-rechte (%s)", K->RM_CODE));
        zugriff_rechte_kaputt = false;
    } else if (!strcmp(art, "kaputt_selbst")) {   /* NEU 1.0.10 (Codex-Prüfung 148, P2): Rechte gehen beim Entfernen der eigenen Dateien verloren */
        prot = (sliste){0};
        kaputt_nach_geloescht = true;
        int f = kern_selbst_entfernen(K, true, false);
        kaputt_nach_geloescht = false;
        for (int i = 0; i < prot.n; i++) printf("PROT %s\n", prot.e[i]);
        printf("ERG selbst Fehler=%d kaputt=%d uebergeben=%d\n", f, (int)zugriff_rechte_kaputt, (int)K->SELBST_UEBERGEBEN);
        if (!zugriff_rechte_kaputt) printf("INFO keine eigene Datei gelöscht - Fall nicht erreicht\n");
        pruefe(zugriff_rechte_kaputt && f >= 1 && !K->SELBST_UEBERGEBEN && prot_hat("PS5 Cleanup entfernt sich nicht weiter"),
               a_fmt(A0, "Rechte bei der Selbstentfernung verloren: Fehler gezählt (%d), keine Übergabe, Sicherheitsstopp im Protokoll", f));
        zugriff_rechte_kaputt = false;
    } else if (!strcmp(art, "kaputt_nachlauf")) {   /* NEU 1.0.10 (Codex-Prüfung 147, P2): erst nach der ersten Kontrolle, im Nachlauf */
        if (p) p->Wahl = true;
        prot = (sliste){0};
        K->warte = warte_test;
        kaputt_beim_warten = true;
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        kaputt_beim_warten = false;
        for (int i = 0; i < prot.n; i++) printf("PROT %s\n", prot.e[i]);
        printf("ERG Fehler=%d warte=%d RM=%s\n", e.Fehler, warte_n, K->RM_CODE);
        pruefe(warte_n > 0, "Nachlauf erreicht (Warten vor \"Tool wieder da?\")");
        pruefe(e.Fehler == 1 && prot_hat("SICHERHEITSSTOPP: Nach dem Wechsel auf den Benutzer eines Programms") && !strcmp(K->RM_CODE, "g-rechte"),
               a_fmt(A0, "Sicherheitsstopp im Nachlauf: im Protokoll, genau einmal als Fehler, Grund g-rechte (%d, %s)", e.Fehler, K->RM_CODE));
        zugriff_rechte_kaputt = false;
    }
    printf("INFO Wechsel %d, abgelehnt %d\n", wechsel_n, abgelehnt_n);
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
