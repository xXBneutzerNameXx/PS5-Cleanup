/* PS5 Cleanup 1.1 - Test S4: Löschen gegen veränderte Verknüpfungen und Einbindungen abgesichert
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *   test_s4 <wurzel> [einbindung]   (Baum wird von lauf_sicherheit.sh vorbereitet; "einbindung" nur in unshare -rm)
 */
#include "../../kern/zugriff.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static bool da(const char *p) { struct stat st; return lstat(p, &st) == 0; }
static char buf[4096];
static const char *w(const char *wurzel, const char *p) { snprintf(buf, sizeof buf, "%s%s", wurzel, p); return buf; }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    const char *W = argv[1];
    bool einb = argc >= 3 && !strcmp(argv[2], "einbindung");
    datei_optionen o = { .wurzel = W, .sortiert = true };
    zugriff *z = zugriff_datei(&o);
    arena *a = arena_neu();
    antwort r;
    if (!einb) {
        /* 1 Ordner mit Verknüpfung nach draußen: Verknüpfung weg, Ziel bleibt */
        r = z->befehl(z, a, "RMDA", "/user/data/a");
        printf("RMDA /user/data/a -> %s\n", r.text);
        pruefe(r.code == 250 && !da(w(W, "/user/data/a")), "Ordner samt Verknüpfung entfernt");
        pruefe(da(w(W, "/aussen/wichtig.txt")), "Ziel der Verknüpfung (außerhalb) steht noch");
        /* 2 Teil des Weges ist eine Verknüpfung (getauschter Ordner): nichts löschen */
        r = z->befehl(z, a, "DELE", "/user/data/getauscht/wichtig2.txt");
        printf("DELE /user/data/getauscht/wichtig2.txt -> %s\n", r.text);
        pruefe(r.code >= 400 && da(w(W, "/aussen2/wichtig2.txt")), "Weg über Verknüpfung: abgelehnt, Datei außerhalb steht noch");
        r = z->befehl(z, a, "RMDA", "/user/data/getauscht/unter");
        pruefe(r.code >= 400 && da(w(W, "/aussen2/unter/x.txt")), "RMDA über Verknüpfung im Weg: abgelehnt");
        /* 3 Rechte nie über eine Verknüpfung ändern */
        struct stat vorher; stat(w(W, "/aussen/wichtig.txt"), &vorher);
        r = z->befehl(z, a, "SITE CHMOD 777", "/user/data/link_auf_datei");
        struct stat nachher; stat(w(W, "/aussen/wichtig.txt"), &nachher);
        printf("SITE CHMOD 777 /user/data/link_auf_datei -> %s\n", r.text);
        pruefe(r.code >= 400 && vorher.st_mode == nachher.st_mode, "CHMOD auf Verknüpfung: abgelehnt, Ziel unverändert");
        /* 4 DELE auf Verknüpfung entfernt nur die Verknüpfung */
        r = z->befehl(z, a, "DELE", "/user/data/link_auf_datei");
        pruefe(r.code == 250 && !da(w(W, "/user/data/link_auf_datei")) && da(w(W, "/aussen/wichtig.txt")), "DELE auf Verknüpfung: nur sie weg, Ziel steht");
        /* 5 RMDA auf eine Verknüpfung (Ordner): Fehler, Ziel bleibt */
        r = z->befehl(z, a, "RMDA", "/user/data/link_auf_ordner");
        pruefe(r.code >= 400 && da(w(W, "/aussen2/unter/x.txt")), "RMDA auf Verknüpfung zu Ordner: abgelehnt");
        /* 6 Schreiben über Verknüpfung im Weg: abgelehnt */
        r = z->schreibe(z, a, "/user/data/getauscht/neu.txt", "x", 1);
        pruefe(r.code >= 400 && !da(w(W, "/aussen2/neu.txt")), "Schreiben über Verknüpfung im Weg: abgelehnt");
        /* 6b NEU (Codex-Prüfung 26.10.): STOP-Datei als harter Link auf eine wichtige Datei -> nicht geleert */
        r = z->schreibe(z, a, "/user/data/shadowmount/STOP", "x", 1);
        struct stat hs; stat(w(W, "/aussen/act.dat"), &hs);
        printf("STOR STOP (harter Link) -> %s\n", r.text);
        pruefe(r.code >= 400 && hs.st_size == 8, "Schreiben auf harten Link: abgelehnt, wichtige Datei unverändert");
        r = z->schreibe(z, a, "/user/data/neu.txt", "abc", 3);
        pruefe(r.code == 226 && da(w(W, "/user/data/neu.txt")), "normales Schreiben geht weiter");
        r = z->befehl(z, a, "SITE CHMOD 777", "/user/data/normal.txt");
        pruefe(r.code == 200, "CHMOD auf normale Datei geht weiter");
        /* 7 normale Fälle gehen weiter */
        r = z->befehl(z, a, "DELE", "/user/data/normal.txt");
        pruefe(r.code == 250 && !da(w(W, "/user/data/normal.txt")), "normale Datei: gelöscht");
        r = z->befehl(z, a, "RMDA", "/user/data/normal_ordner");
        pruefe(r.code == 250 && !da(w(W, "/user/data/normal_ordner")), "normaler Ordner samt Inhalt: gelöscht");
    } else {
        /* 8 Einbindung im Baum: gar nichts löschen */
        r = z->befehl(z, a, "RMDA", "/user/data/m");
        printf("RMDA /user/data/m (enthält Einbindung) -> %s\n", r.text);
        pruefe(r.code >= 400, "Ordner mit Einbindung darin: abgelehnt");
        pruefe(da(w(W, "/user/data/m/vorher.txt")) && da(w(W, "/user/data/m/innen/im_mount.txt")), "nichts gelöscht - weder davor noch in der Einbindung");
        r = z->befehl(z, a, "RMDA", "/user/data/m/innen");
        pruefe(r.code >= 400 && da(w(W, "/user/data/m/innen/im_mount.txt")), "Einbindung selbst: abgelehnt");
        /* NEU (Codex-Prüfung 26.10.): auch Einzelbefehle nie in eine Einbindung hinein (Ausweichweg Datei für Datei) */
        r = z->befehl(z, a, "DELE", "/user/data/m/innen/im_mount.txt");
        printf("DELE /user/data/m/innen/im_mount.txt -> %s\n", r.text);
        pruefe(r.code >= 400 && da(w(W, "/user/data/m/innen/im_mount.txt")), "DELE in einer Einbindung: abgelehnt");
        r = z->befehl(z, a, "SITE CHMOD 777", "/user/data/m/innen");
        pruefe(r.code >= 400, "CHMOD auf eine Einbindung: abgelehnt");
        r = z->befehl(z, a, "SITE CHMOD 777", "/user/data/m/innen/im_mount.txt");
        pruefe(r.code >= 400, "CHMOD in einer Einbindung: abgelehnt");
        r = z->schreibe(z, a, "/user/data/m/innen/neu.txt", "x", 1);
        pruefe(r.code >= 400 && !da(w(W, "/user/data/m/innen/neu.txt")), "Schreiben in eine Einbindung: abgelehnt");
        r = z->befehl(z, a, "DELE", "/user/data/m/vorher.txt");
        pruefe(r.code == 250 && !da(w(W, "/user/data/m/vorher.txt")), "Datei neben der Einbindung: normal gelöscht");
    }
    printf("FEHLERZAHL %d\n", fehler);
    return 0;
}
