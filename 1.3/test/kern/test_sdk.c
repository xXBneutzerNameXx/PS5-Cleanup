/* PS5 Cleanup 1.1.1 - Test: Dateisystem-Aufrufe des PS5-SDK (Sicherheitsfix 17.11.2026, Rückmeldungen #9 FW 13.20, #13/#15 FW 13.60)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * Übersetzt mit -DPS5C_SDK_NACHBILDUNG -include bau/ps5_systemaufrufe.h: die Aufrufe openat, fstatat, unlinkat ... verhalten sich wie auf
 * der PS5. PS5C_SDK_ALT=1 = wie die SDK-Stubs bis 1.1 (Fehler als positive Zahl, errno unverändert), sonst wie die Korrektur.
 * PS5C_FEHLER=<aufruf>:<name>:<errno> lässt einen Aufruf scheitern (wie die PS5 auf FW 13.x beim Weg zum Ziel).
 *   test_sdk <wurzel> <fall>
 *   fall: fix_ebadf  - Korrektur, openat(user) scheitert mit 13: sauberer Fehler mit echtem Grund und Aufruf, nichts gelöscht
 *         alt_ebadf  - SDK wie bisher, openat(user) scheitert mit 78: neuer Code lehnt ab (Griff 78 gibt es nicht), nichts gelöscht
 *         fix_fremd  - Korrektur, Griff 13 ist ein fremder offener Ordner (b), openat(a) scheitert mit 13: nichts gelöscht, Griff 13 bleibt offen
 *         alt_fremd  - SDK wie bisher, dasselbe: der neue Code erkennt den fremden Ordner (Gerät/Knoten) - b/x.txt bleibt, Griff 13 bleibt offen
 *         fix_stderr - Korrektur, openat(a) scheitert mit 2 (ENOENT = Griff von stderr): nichts gelöscht
 *         alt_stderr - SDK wie bisher, dasselbe
 *         fix_weg    - Korrektur, unlinkat(c.txt) scheitert mit 1: Fehler mit echtem Grund, c.txt bleibt
 *         fix_normal - Korrektur ohne Fehler: Datei und Ordner werden normal gelöscht
 *   NEU 18.11.2026 - zweiter Weg (PS5C_NUR_PFAD=1: jeder Aufruf mit echtem Ordner-Griff scheitert mit EINVAL, wie FW 13.x):
 *         p13_normal - Datei und Ordner samt Inhalt (mit Verknüpfung nach draußen) werden über den Pfad gelöscht, draußen bleibt alles
 *         p13_link   - Teil des Weges ist eine Verknüpfung: abgelehnt, das Ziel draußen bleibt
 */
#include "../../kern/zugriff.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static char buf[4096];
static const char *w(const char *wurzel, const char *p) { snprintf(buf, sizeof buf, "%s%s", wurzel, p); return buf; }
static bool da(const char *wurzel, const char *p) { struct stat st; return lstat(w(wurzel, p), &st) == 0; }
static bool offen(int fd) { return fcntl(fd, F_GETFD) != -1; }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: test_sdk <wurzel> <fall>\n"); return 2; }
    const char *W = argv[1], *fall = argv[2];
    printf("== SDK-Aufrufe: %s (PS5C_SDK_ALT=%s, PS5C_FEHLER=%s)\n", fall, getenv("PS5C_SDK_ALT") ? getenv("PS5C_SDK_ALT") : "-", getenv("PS5C_FEHLER") ? getenv("PS5C_FEHLER") : "-");
    datei_optionen o = { .wurzel = W, .sortiert = true };
    zugriff *z = zugriff_datei(&o);
    arena *a = arena_neu();
    antwort r;
    bool fremd = strstr(fall, "_fremd") != NULL;
    if (fremd) {   /* Griff 13 = ein fremder offener Ordner (wie ein Ordner, den ein anderer Teil des Programms gerade liest) */
        int fb = open(w(W, "/user/data/b"), O_RDONLY | O_DIRECTORY);
        if (fb < 0 || dup2(fb, 13) != 13) { printf("FAIL Vorbereitung (Griff 13)\n"); return 1; }
        close(fb);
    }
    if (!strcmp(fall, "p13_normal")) {
        r = z->befehl(z, a, "DELE", "/user/data/c.txt");
        printf("DELE /user/data/c.txt -> %s\n", r.text);
        pruefe(r.code == 250 && !da(W, "/user/data/c.txt"), "FW 13.x: Datei über den Pfad gelöscht");
        r = z->befehl(z, a, "RMDA", "/user/data/a");
        printf("RMDA /user/data/a -> %s\n", r.text);
        pruefe(r.code == 250 && !da(W, "/user/data/a"), "FW 13.x: Ordner samt Inhalt über den Pfad gelöscht");
        pruefe(da(W, "/user/data/b/x.txt") && da(W, "/draussen/wichtig.txt"), "Nachbarordner und das Ziel der Verknüpfung (draußen) unberührt");
    } else if (!strcmp(fall, "p13_link")) {
        r = z->befehl(z, a, "DELE", "/user/data/umweg/wichtig.txt");
        printf("DELE /user/data/umweg/wichtig.txt -> %s\n", r.text);
        pruefe(r.code >= 500 && da(W, "/draussen/wichtig.txt"), "FW 13.x: Verknüpfung im Weg - abgelehnt, Datei draußen bleibt");
        r = z->befehl(z, a, "RMDA", "/user/data/umweg");
        printf("RMDA /user/data/umweg -> %s\n", r.text);
        pruefe(r.code >= 500 && da(W, "/draussen/wichtig.txt"), "FW 13.x: RMDA auf eine Verknüpfung zu einem Ordner - abgelehnt");
    } else if (!strcmp(fall, "fix_normal")) {
        r = z->befehl(z, a, "DELE", "/user/data/c.txt");
        printf("DELE /user/data/c.txt -> %s\n", r.text);
        pruefe(r.code == 250 && !da(W, "/user/data/c.txt"), "normale Datei gelöscht");
        r = z->befehl(z, a, "RMDA", "/user/data/a");
        printf("RMDA /user/data/a -> %s\n", r.text);
        pruefe(r.code == 250 && !da(W, "/user/data/a"), "normaler Ordner samt Inhalt gelöscht");
        pruefe(da(W, "/user/data/b/x.txt"), "Nachbarordner unberührt");
    } else if (!strcmp(fall, "fix_weg")) {
        r = z->befehl(z, a, "DELE", "/user/data/c.txt");
        printf("DELE /user/data/c.txt -> %s\n", r.text);
        pruefe(r.code >= 500 && da(W, "/user/data/c.txt"), "Entfernen scheitert: Datei bleibt");
        pruefe(strstr(r.text, "Operation not permitted") != NULL, "echter Grund (errno 1) in der Antwort, kein veralteter");
    } else {
        const char *ziel = "/user/data/a/x.txt";
        r = z->befehl(z, a, "DELE", ziel);
        printf("DELE %s -> %s\n", ziel, r.text);
        pruefe(r.code >= 500, "abgelehnt");
        pruefe(da(W, "/user/data/a/x.txt") && da(W, "/user/data/b/x.txt") && da(W, "/user/data/c.txt"), "nichts gelöscht - weder das Ziel noch im fremden Ordner");
        pruefe(strstr(r.text, "Weg zum Ziel öffnen") != NULL && strstr(r.text, "[Aufruf: ") != NULL, "Schritt und Aufruf im Protokoll");
        if (!strcmp(fall, "fix_ebadf")) pruefe(strstr(r.text, "openat(/, user) = -1, errno 13") != NULL && strstr(r.text, "Bad file descriptor") == NULL,
                                               "Korrektur: der echte Grund (openat errno 13) statt \"Bad file descriptor\"");
        if (!strcmp(fall, "fix_fremd") || !strcmp(fall, "fix_stderr"))
            pruefe(strstr(r.text, "openat(/user/data, a) = -1") != NULL, "Korrektur: openat meldet -1 mit echtem Grund");
        if (!strcmp(fall, "alt_fremd")) pruefe(strstr(r.text, "openat(/user/data, a) = 13") != NULL, "SDK wie bisher: openat lieferte 13 (fremder Griff) - erkannt und abgelehnt");
        if (fremd) pruefe(offen(13), "der fremde Griff 13 wurde nicht geschlossen");
        if (strstr(fall, "_stderr")) pruefe(offen(2), "stderr (Griff 2) wurde nicht geschlossen");
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
