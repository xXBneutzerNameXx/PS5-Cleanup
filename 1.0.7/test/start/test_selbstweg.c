/* PS5 Cleanup 1.0.6 - PC-Test der abschließenden Selbstentfernung durch den Hintergrundteil (start/selbstweg.c)
 * Sonys Dienst und das Einhängen von /system_ex sind nachgebildet; gelöscht wird nur im Testordner (SO_WURZEL).
 *   test_selbstweg <fall>   fall: ok | ablehnen | fremd_start | fremd_kachel | ohne_kachel | ro_zurueck | haengt */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../../start/selbstweg.h"
#include <fcntl.h>
#include <ps5/klog.h>
#include "../../start/startordner.h"   /* für den Startfall (so_einrichten) */

int TEST_RO = 0, TEST_NMOUNT = 0;
/* wie auf der PS5 (Tests 30.10.2026 00:14/00:21): nach dem Einschalten genau EIN Wechsel auf beschreibbar; nach einer Rückstellung
 * meldet nmount Erfolg, es bleibt aber schreibgeschützt. Fall "ealready": Rückstellung scheitert zweimal mit errno 37. */
static int GESPERRT = 0, EALREADY_REST = 0;
int nmount(struct iovec *iov, unsigned n, int flags)
{
    (void)iov; (void)n; TEST_NMOUNT++;
    if (flags & MNT_RDONLY) {
        if (EALREADY_REST > 0) { EALREADY_REST--; errno = 37; return -1; }
        TEST_RO = 1; GESPERRT = 1; return 0;
    }
    if (!GESPERRT) TEST_RO = 0;
    return 0;
}
static int MOUNTS = 0;
int sw_eigene_mounts(void) { return MOUNTS > 0 ? MOUNTS-- : 0; }
#undef statfs
int test_statfs(const char *p, struct statfs *s) { (void)p; memset(s, 0, sizeof *s); s->f_flags = TEST_RO ? MNT_RDONLY : 0; return 0; }

static const char *FALL;
static int SONY_DA = 1, UNINST = 0;
static void weg(const char *p) { char c[2000]; snprintf(c, sizeof c, "rm -rf '%s'", p); if (system(c)) {} }
int sceAppInstUtilInitialize(void) { if (!strcmp(FALL, "haengt")) sleep(30); return 0; }
int sceAppInstUtilAppExists(const char *id, int *d) { (void)id; *d = (int)(0xFFFFFF00u | (unsigned)SONY_DA); return 0; }   /* nur ein Byte wie auf der PS5 */
int sceAppInstUtilAppUnInstall(const char *id)
{
    UNINST++;
    if (strcmp(id, "PSCL00001")) { printf("FAIL Sony-Auftrag für fremde Kennung %s\n", id); return 1; }
    if (!strcmp(FALL, "ablehnen")) return (int)0x80990001;
    if (!strcmp(FALL, "zweimal") && UNINST == 1) return 0;   /* NEU 01.11.2026: angenommen, Kachel bleibt erst noch eingetragen (wie PS5Upload) */
    if (!strcmp(FALL, "nie_weg")) return 0;                   /* NEU 01.11.2026: angenommen, Kachel bleibt immer eingetragen */
    weg(SO_WURZEL "/user/app/PSCL00001"); SONY_DA = 0; return 0;
}
static int fehler = 0;
static void pruefe(int ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static int da(const char *p) { struct stat st; return lstat(p, &st) == 0; }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    FALL = argc > 1 ? argv[1] : "ok";
    printf("== Hintergrundteil-Selbstentfernung: %s\n", FALL);
    if (!strcmp(FALL, "fremd_start")) { FILE *f = fopen(SO_WURZEL "/system_ex/app/PSCL00002/fremd.txt", "w"); if (f) fclose(f); }
    if (!strcmp(FALL, "fremd_kachel")) unlink(SO_WURZEL "/user/app/PSCL00001/.ps5cleanup_kachel");
    if (!strcmp(FALL, "ro_zurueck") || !strcmp(FALL, "ealready")) TEST_RO = 1;
    if (!strcmp(FALL, "zweiter_wechsel")) { TEST_RO = 1; GESPERRT = 1; }      /* in dieser Sitzung schon einmal zurückgestellt */
    /* NEU 03.11.2026 (PS5-Test): die Oberfläche hat /system_ex in dieser Sitzung selbst beschreibbar gemacht (Marke) - vor der Sitzung war es
     * schreibgeschützt: danach wieder schreibgeschützt ist RICHTIG, keine Meldung "nicht wie vorher" */
    if (!strcmp(FALL, "sitzung_offen")) { TEST_RO = 0; so_offen_merken(); }
    if (!strcmp(FALL, "ealready")) { EALREADY_REST = 2; MOUNTS = 3; }
    if (!strcmp(FALL, "start_gesperrt") || !strcmp(FALL, "start_frei")) {   /* Start: Startordner fehlt, anlegen */
        weg(SO_WURZEL "/system_ex/app/PSCL00002"); TEST_RO = 1; if (!strcmp(FALL, "start_gesperrt")) GESPERRT = 1;
        char t[400]; int u = 0;
        int r = so_einrichten(t, sizeof t, &u);
        printf("INFO so_einrichten -> %d (umgehängt %d) %s | nmount %d, schreibgeschützt %d\n", r, u, t, TEST_NMOUNT, TEST_RO);
        if (!strcmp(FALL, "start_gesperrt")) {
            pruefe(r == -1 && strstr(t, "erst nach einem Neustart") && !da(SO_WURZEL "/system_ex/app/PSCL00002"), "Start nach Rückstellung in derselben Sitzung: klare Meldung, nichts angelegt");
            pruefe(TEST_NMOUNT == 1 && TEST_RO == 1, "Start gesperrt: ein Umhängeversuch, Schreibschutz unverändert");
        } else {
            /* NEU 1.0.7 (da_Chris86 31.10.2026 17:00): /system_ex bleibt bis zum Ende der Sitzung beschreibbar (Marke), zurück erst am Ende */
            pruefe(r == 0 && so_zustand() == 1 && TEST_RO == 0 && TEST_NMOUNT == 1 && so_offen_von_uns() == 1, "erster Start nach dem Einschalten: Startordner angelegt, /system_ex bleibt bis zum Ende beschreibbar (Marke gesetzt)");
            pruefe(so_zurueck() == 1 && TEST_RO == 1 && !da(SO_OFFEN), "am Ende der Sitzung: Schreibschutz zurück, Marke weg");
            pruefe(so_zurueck() == 0 && TEST_RO == 1, "zweites Zurückstellen: nichts zu tun");
        }
        pruefe(da(SO_WURZEL "/system_ex/app/FAKE00000/eboot.bin") && da(SO_WURZEL "/system_ex/app/NPXS40106/eboot.bin"), "fremde/Sony-Ordner unangetastet (FAKE00000, NPXS40106)");
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    if (!strcmp(FALL, "protokoll")) {   /* NEU 1.0.7: Schlusszeilen an die Protokoll-Datei auf dem Stick (nur anhängen, nie anlegen) */
        const char *gut = "/mnt/usb0/ps5cleanup_protokoll_2026-10-31_203000.txt";
        pruefe(!selbstweg_protokoll_setzen("/mnt/usb8/ps5cleanup_protokoll_1.txt", strlen("/mnt/usb8/ps5cleanup_protokoll_1.txt")), "abgelehnt: /mnt/usb8");
        pruefe(!selbstweg_protokoll_setzen("/user/app/ps5cleanup_protokoll_1.txt", strlen("/user/app/ps5cleanup_protokoll_1.txt")), "abgelehnt: anderer Ort als /mnt/usbN");
        pruefe(!selbstweg_protokoll_setzen("/mnt/usb0/ps5cleanup_protokoll_../x.txt", strlen("/mnt/usb0/ps5cleanup_protokoll_../x.txt")), "abgelehnt: .. im Namen");
        pruefe(!selbstweg_protokoll_setzen("/mnt/usb0/andere_datei.txt", strlen("/mnt/usb0/andere_datei.txt")), "abgelehnt: anderer Dateiname");
        mkdir(SO_WURZEL "/mnt", 0755); mkdir(SO_WURZEL "/mnt/usb0", 0755);
        {   /* da_Chris86 02.11.2026 16:57: ohne Stick Ordner /data/PS5-Cleanup-Protokolle, je Lauf eine Datei */
            const char *ja = "/data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_2026-11-02_165700.txt";
            const char *nein[] = { "/data/PS5-Cleanup-Protokoll.txt", "/data/PS5-Cleanup-Protokolle/andere.txt", "/data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_../x.txt",
                                   "/data/x/ps5cleanup_protokoll_2026-11-02_165700.txt", "/data/PS5-Cleanup-Protokolle2/ps5cleanup_protokoll_1.txt", "/data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_1.txt/x.txt", NULL };
            pruefe(selbstweg_protokoll_setzen(ja, strlen(ja)), "angenommen: /data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_<Datum>.txt (ohne USB-Stick)");
            const char *ja2 = "/mnt/ext1/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_2026-11-02_172300.txt";   /* 17:23: interne Platte voll -> M.2 */
            pruefe(selbstweg_protokoll_setzen(ja2, strlen(ja2)), "angenommen: /mnt/ext1/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_<Datum>.txt (M.2)");
            const char *nein2[] = { "/mnt/ext1/ps5cleanup_protokoll_1.txt", "/mnt/ext0/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_1.txt", "/mnt/ext1/PS5-Cleanup-Protokolle/x.txt", NULL };
            int z2 = 0; for (int i = 0; nein2[i]; i++) if (selbstweg_protokoll_setzen(nein2[i], strlen(nein2[i]))) { z2++; printf("  angenommen, obwohl falsch: %s\n", nein2[i]); }
            pruefe(z2 == 0, "abgelehnt: M.2 ohne Protokoll-Ordner, USB-Speicher, anderer Name");
            int z = 0; for (int i = 0; nein[i]; i++) if (selbstweg_protokoll_setzen(nein[i], strlen(nein[i]))) { z++; printf("  angenommen, obwohl falsch: %s\n", nein[i]); }
            pruefe(z == 0, "abgelehnt: alte Einzeldatei, anderer Name, .., anderer Ordner, Ordner mit ähnlichem Namen, Unterordner");
        }
        pruefe(selbstweg_protokoll_setzen(gut, strlen(gut)), "angenommen: /mnt/usb0/ps5cleanup_protokoll_<Datum>.txt");
        selbstweg_log("Zeile ohne Datei");
        pruefe(!da(SO_WURZEL "/mnt/usb0/ps5cleanup_protokoll_2026-10-31_203000.txt"), "Datei fehlt: wird NICHT angelegt");
        FILE *f = fopen(SO_WURZEL "/mnt/usb0/ps5cleanup_protokoll_2026-10-31_203000.txt", "w"); if (f) { fputs("Kopf\r\n", f); fclose(f); }
        selbstweg_log("Kachel %s - Test", "PSCL00001");
        char b[400] = {0}; f = fopen(SO_WURZEL "/mnt/usb0/ps5cleanup_protokoll_2026-10-31_203000.txt", "r"); if (f) { size_t n = fread(b, 1, sizeof b - 1, f); b[n] = 0; fclose(f); }
        pruefe(!strncmp(b, "Kopf\r\n", 6) && strstr(b, "Selbstentfernung: Kachel PSCL00001 - Test\r\n") != NULL, "Zeile angehängt, Kopf bleibt");
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    if (!strcmp(FALL, "protokoll_intern")) {   /* NEU 1.0.8 (Plan 1.0.8 4a): ohne USB-Stick /data/PS5-Cleanup-Protokolle - nur anhängen, nie anlegen */
        const char *p = "/data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_2026-11-04_220000.txt";
        const char *datei = SO_WURZEL "/data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_2026-11-04_220000.txt";
        pruefe(selbstweg_protokoll_setzen(p, strlen(p)), "angenommen: /data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_<Datum>.txt");
        selbstweg_log("Zeile ohne Ordner");
        pruefe(!da(SO_WURZEL "/data/PS5-Cleanup-Protokolle"), "Ordner fehlt: der Hintergrundteil legt weder Ordner noch Datei an");
        mkdir(SO_WURZEL "/data", 0755); mkdir(SO_WURZEL "/data/PS5-Cleanup-Protokolle", 0755);
        selbstweg_log("Zeile ohne Datei");
        pruefe(!da(datei), "Datei fehlt: wird NICHT angelegt");
        FILE *f = fopen(datei, "w"); if (f) { fputs("Kopf\r\nZeile der Oberfläche\r\n", f); fclose(f); }
        selbstweg_log("Kachel %s - Test", "PSCL00001");
        selbstweg_log("zweite Zeile");
        char b[800] = {0}; f = fopen(datei, "r"); if (f) { size_t n = fread(b, 1, sizeof b - 1, f); b[n] = 0; fclose(f); }
        const char *z1 = strstr(b, "Selbstentfernung: Kachel PSCL00001 - Test\r\n"), *z2 = strstr(b, "Selbstentfernung: zweite Zeile\r\n");
        pruefe(!strncmp(b, "Kopf\r\nZeile der Oberfläche\r\n", strlen("Kopf\r\nZeile der Oberfläche\r\n")) && z1 && z2 && z1 < z2 && !strstr(b, "ohne Datei"),
               "an die Datei der Oberfläche angehängt: ihr Inhalt bleibt, beide Zeilen in Reihenfolge, nichts aus der Zeit davor");
        /* eine Verknüpfung statt der Datei: nicht folgen (O_NOFOLLOW), die fremde Datei bleibt unverändert */
        const char *p2 = "/data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_2026-11-04_220100.txt";
        f = fopen(SO_WURZEL "/data/fremd.txt", "w"); if (f) { fputs("fremd\n", f); fclose(f); }
        pruefe(symlink(SO_WURZEL "/data/fremd.txt", SO_WURZEL "/data/PS5-Cleanup-Protokolle/ps5cleanup_protokoll_2026-11-04_220100.txt") == 0, "Verknüpfung angelegt (Voraussetzung für die nächste Prüfung, Codex-Prüfung 70)");
        pruefe(selbstweg_protokoll_setzen(p2, strlen(p2)), "angenommen: zweiter Pfad (ist eine Verknüpfung)");
        selbstweg_log("darf nicht ankommen");
        char c[100] = {0}; f = fopen(SO_WURZEL "/data/fremd.txt", "r"); if (f) { size_t n = fread(c, 1, sizeof c - 1, f); c[n] = 0; fclose(f); }
        pruefe(!strcmp(c, "fremd\n"), "Verknüpfung: nicht gefolgt, die fremde Datei ist unverändert");
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    if (!strcmp(FALL, "sitzung_offen")) {   /* Codex 67: direkt die Startordner-Funktion prüfen - selbstweg_ausfuehren() verschluckt ihren Fehlertext */
        char g[400] = "x";
        int s = selbstweg_startordner(g, sizeof g);
        printf("INFO Startordner %d: '%s' | nmount %d, schreibgeschützt %d\n", s, g, TEST_NMOUNT, TEST_RO);
        pruefe(s == 1 && g[0] == 0, "von uns geöffnet: Startordner-Funktion meldet Erfolg ohne Fehlertext (keine Meldung \"nicht wie vor der Sitzung\")");
        pruefe(!da(SO_WURZEL "/system_ex/app/PSCL00002") && da(SO_WURZEL "/user/app/PSCL00001/.ps5cleanup_kachel"), "von uns geöffnet: Startordner weg, Kachel nicht angefasst");
        pruefe(TEST_RO == 1 && !da(SO_OFFEN), "von uns geöffnet: danach wieder schreibgeschützt (wie vor der Sitzung), Marke weg");
        pruefe(da(SO_WURZEL "/system_ex/app/FAKE00000/eboot.bin") && da(SO_WURZEL "/system_ex/app/NPXS40106/eboot.bin"), "fremde/Sony-Ordner unangetastet (FAKE00000, NPXS40106)");
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    int kachel = strcmp(FALL, "ohne_kachel") != 0;
    if (!kachel) { weg(SO_WURZEL "/user/app/PSCL00001"); SONY_DA = 0; }
    char erg[1200];
    int r = selbstweg_ausfuehren(kachel, 1, erg, sizeof erg);
    printf("INFO Ergebnis %d: %s | Sony-Aufträge %d, nmount %d, schreibgeschützt %d\n", r, erg, UNINST, TEST_NMOUNT, TEST_RO);
    pruefe(da(SO_WURZEL "/system_ex/app/FAKE00000/eboot.bin") && da(SO_WURZEL "/system_ex/app/NPXS40106/eboot.bin"), "fremde/Sony-Ordner unangetastet (FAKE00000, NPXS40106)");
    if (!strcmp(FALL, "ok") || !strcmp(FALL, "ohne_kachel") || !strcmp(FALL, "ro_zurueck") || !strcmp(FALL, "ealready")) {
        pruefe(r == 1 && !strcmp(erg, "PS5 Cleanup wurde entfernt."), "Ergebnis: \"PS5 Cleanup wurde entfernt.\" (ohne Hinweis zum Startordner)");
        pruefe(!da(SO_WURZEL "/system_ex/app/PSCL00002") && !da(SO_WURZEL "/user/app/PSCL00001"), "Startordner und Kachel weg");
        pruefe(UNINST == (kachel ? 1 : 0), kachel ? "genau ein Sony-Auftrag (PSCL00001)" : "ohne Kachel kein Sony-Auftrag");
        if (!strcmp(FALL, "ro_zurueck")) pruefe(TEST_RO == 1 && TEST_NMOUNT == 2, "schreibgeschützt vorher -> beschreibbar -> wieder schreibgeschützt (ursprüngliche Optionsliste)");
        else if (!strcmp(FALL, "ealready")) pruefe(TEST_RO == 1 && TEST_NMOUNT == 4 && MOUNTS == 0, "Abbau abgewartet, Rückstellung nach 2x \"Operation already in progress\" wiederholt und bestätigt");
        else pruefe(TEST_RO == 0 && TEST_NMOUNT == 0, "beschreibbar vorher -> kein Umhängen");
    } else if (!strcmp(FALL, "zweiter_wechsel")) {
        pruefe(r == 1 && !strcmp(erg, "PS5 Cleanup wurde entfernt."), "zweiter Wechsel in derselben Sitzung: Kachel entfernt, Meldung ohne Hinweis");
        pruefe(da(SO_WURZEL "/system_ex/app/PSCL00002/.ps5cleanup_start") && da(SO_WURZEL "/system_ex/app/PSCL00002/eboot.bin"), "Startordner bleibt unverändert (nichts gelöscht)");
        pruefe(TEST_RO == 1 && TEST_NMOUNT == 1, "genau ein Umhängeversuch, Schreibschutz unverändert");
    } else if (!strcmp(FALL, "ablehnen")) {
        pruefe(r == 0 && strstr(erg, "Sony hat abgelehnt") && !strstr(erg, "Startordner"), "Sony lehnt ab: Meldung nennt die Kachel, nicht den Startordner");
        pruefe(da(SO_WURZEL "/system_ex/app/PSCL00002/.ps5cleanup_start") && da(SO_WURZEL "/user/app/PSCL00001/.ps5cleanup_kachel"), "Startordner und Kachel-Dateien noch da");
    } else if (!strcmp(FALL, "fremd_start")) {
        pruefe(r == 1 && !strstr(erg, "Startordner"), "fremder Inhalt im Startordner: Kachel entfernt, ohne Hinweis zum Startordner");
        pruefe(da(SO_WURZEL "/system_ex/app/PSCL00002/fremd.txt") && da(SO_WURZEL "/system_ex/app/PSCL00002/eboot.bin"), "Startordner unverändert (nichts gelöscht)");
    } else if (!strcmp(FALL, "fremd_kachel")) {
        pruefe(r == 0 && UNINST == 0 && strstr(erg, "nicht eindeutig zu PS5 Cleanup"), "Kachel ohne Markierung: Sony wird nicht beauftragt");
        pruefe(da(SO_WURZEL "/user/app/PSCL00001") && da(SO_WURZEL "/system_ex/app/PSCL00002/.ps5cleanup_start"), "Kachel und Startordner bleiben");
    } else if (!strcmp(FALL, "zweimal")) {   /* NEU 01.11.2026 */
        pruefe(r == 1 && UNINST == 2 && !da(SO_WURZEL "/user/app/PSCL00001"), "Sony nimmt an, Kachel bleibt erst eingetragen: der zweite Auftrag entfernt sie (wie PS5Upload am 01.11.2026)");
    } else if (!strcmp(FALL, "nie_weg")) {   /* NEU 01.11.2026 */
        pruefe(r == 0 && UNINST == 3 && strstr(erg, "nach 3 Versuchen"), "Sony nimmt an, Kachel bleibt eingetragen: höchstens 3 Aufträge, dann Abbruch mit Grund");
        pruefe(da(SO_WURZEL "/user/app/PSCL00001/.ps5cleanup_kachel") && da(SO_WURZEL "/system_ex/app/PSCL00002/.ps5cleanup_start"), "Kachel-Dateien und Startordner bleiben (keine Geisterkachel)");
    } else if (!strcmp(FALL, "haengt")) {
        pruefe(r == 0 && strstr(erg, "antwortet nicht") && UNINST == 0, "Sonys Dienst hängt: nach 10 s abgebrochen, nichts entfernt");
        pruefe(da(SO_WURZEL "/system_ex/app/PSCL00002/.ps5cleanup_start"), "Startordner bleibt");
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
