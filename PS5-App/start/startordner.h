/* PS5 Cleanup 1.0.6 - eigener Startordner /system_ex/app/PSCL00002 (ersetzt die fremde Platzhalter-App FAKE00000)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * Vorgehen übernommen aus websrv src/ps5/hbldr.c (ps5-payload-dev/websrv, John Törnblom, GPL-3.0-or-later):
 * remount_system_ex() und fakeapp_create_if_missing() - dort für FAKE00000, hier für die EIGENE Kennung PSCL00002.
 * Sony startet eine solche App nur aus /system_ex/app/<Kennung>/ (websrv hbldr.c, SDK samples/install_app/Makefile).
 *
 * Freigabe da_Chris86 29.10.2026 21:23 (Ausnahme zur Regel "/system_ex nie anfassen", klar begrenzt):
 *  - nur der eigene Ordner /system_ex/app/PSCL00002 wird angelegt; bei der Selbstentfernung werden NUR seine eigenen
 *    Dateien wieder gelöscht;
 *  - bestehende Sony-Dateien werden nur gelesen und kopiert (NPXS40106/eboot.bin -> eigene eboot.bin);
 *  - fremde Dateien und FAKE00000 bleiben unangetastet;
 *  - enthält der Zielordner fremde oder nicht eindeutig zu PS5 Cleanup gehörende Inhalte: nichts überschreiben;
 *  - der vorherige Einhängezustand von /system_ex wird danach wiederhergestellt.
 * Eindeutig "unser" ist der Ordner nur mit der Markierungsdatei und ausschließlich den eigenen Dateien darin.
 */
#pragma once
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <ps5/klog.h>

#ifndef SO_WURZEL
#define SO_WURZEL   ""            /* nur der PC-Test setzt hier einen Testordner davor (test/start) */
#endif
#define SO_ID       "PSCL00002"
#define SO_DIR      SO_WURZEL "/system_ex/app/" SO_ID
#define SO_SYS      SO_DIR "/sce_sys"
#define SO_PARAMP   SO_SYS "/param.json"
#define SO_EBOOT    SO_DIR "/eboot.bin"
#define SO_MARKE    SO_DIR "/.ps5cleanup_start"
#define SO_PSNOW    SO_WURZEL "/system_ex/app/NPXS40106/eboot.bin"
static const char SO_MARKE_TEXT[] = "PS5CLEANUP-STARTORDNER\n";
static const char SO_PARAM[] =
"{\n"
"  \"applicationCategoryType\": 0,\n"
"  \"attribute\": 1,\n"
"  \"attribute2\": 0,\n"
"  \"attribute3\": 4,\n"
"  \"titleId\": \"" SO_ID "\",\n"
"  \"contentId\": \"IV9999-" SO_ID "_00-PS5CLEANUPSTART0\",\n"
"  \"localizedParameters\": {\n"
"    \"defaultLanguage\": \"en-US\",\n"
"    \"en-US\": { \"titleName\": \"PS5 Cleanup\" },\n"
"    \"de-DE\": { \"titleName\": \"PS5 Cleanup\" }\n"
"  }\n"
"}\n";

#define SO_IOV(x) { (void *)(x), (x) ? strlen(x) + 1 : 0 }
/* 1 = schreibgeschützt eingehängt, 0 = beschreibbar, -1 = unbekannt */
static int so_ist_ro(void)
{
    struct statfs s;
    if (statfs(SO_WURZEL "/system_ex", &s)) return -1;
    return (s.f_flags & MNT_RDONLY) ? 1 : 0;
}
/* wie websrv remount_system_ex(); ro=1 stellt "schreibgeschützt" wieder her */
static int so_umhaengen(int ro)
{
    struct iovec iov[] = {
        SO_IOV("from"),      SO_IOV("/dev/ssd0.system_ex"),
        SO_IOV("fspath"),    SO_IOV("/system_ex"),
        SO_IOV("fstype"),    SO_IOV("exfatfs"),
        SO_IOV("large"),     SO_IOV("yes"),
        SO_IOV("timezone"),  SO_IOV("static"),
        SO_IOV("async"),     SO_IOV(NULL),
        SO_IOV("ignoreacl"), SO_IOV(NULL),
        /* PS5-Tests 30.10.2026 00:14/00:21 (mount_varianten.elf): Nach dem Einschalten lässt sich /system_ex genau EINMAL
         * beschreibbar machen; nach dem Zurückstellen meldet nmount bis zum nächsten Neustart "Erfolg", es bleibt aber
         * schreibgeschützt - mit und ohne "rw"/"noro". Darum die ursprüngliche, auf der PS5 bestätigte Optionsliste (wie websrv,
         * Rückstellung: MNT_RDONLY, Codex 29.10. 23:57 bestätigt) und IMMER den tatsächlichen Zustand per statfs prüfen. */
    };
    int r = nmount(iov, sizeof iov / sizeof iov[0], MNT_UPDATE | (ro ? MNT_RDONLY : 0));
    int e = errno;
    klog_printf("PS5 Cleanup: /system_ex umhängen -> %s: nmount %d (errno %d), danach schreibgeschützt=%d\n", ro ? "ro" : "rw", r, r ? e : 0, so_ist_ro());
    errno = e;
    return r;
}
/* Schreibschutz wiederherstellen und bestätigen. Hängt Sony die beendete App noch ab, meldet nmount "Operation already in
 * progress" (PS5-Test Codex 29.10. 23:50) - dann bis zu 60 s alle 2 s erneut. 0 = schreibgeschützt bestätigt */
static int so_schuetzen(void)
{
    for (int i = 0; i < 30; i++) {
        if (so_ist_ro() == 1) return 0;
        so_umhaengen(1);
        if (so_ist_ro() == 1) return 0;
        sleep(2);
    }
    return so_ist_ro() == 1 ? 0 : -1;
}
static int so_datei(const char *p, off_t groesse)   /* echte Datei (keine Verknüpfung), -1 = beliebige Größe */
{
    struct stat st;
    return !lstat(p, &st) && S_ISREG(st.st_mode) && (groesse < 0 || st.st_size == groesse);
}
static int so_nur(const char *ordner, const char *const *erlaubt)   /* 1 = nur diese Namen im Ordner */
{
    DIR *d = opendir(ordner);
    if (!d) return 0;
    struct dirent *e; int ok = 1;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        int gefunden = 0;
        for (int i = 0; erlaubt[i]; i++) if (!strcmp(e->d_name, erlaubt[i])) gefunden = 1;
        if (!gefunden) { ok = 0; break; }
    }
    closedir(d);
    return ok;
}
/* 1 = eindeutig der eigene Ordner (Markierung + nur eigene Inhalte), 0 = gibt es nicht, -1 = fremd/unklar */
static int so_zustand(void)
{
    struct stat st;
    if (lstat(SO_DIR, &st)) return errno == ENOENT ? 0 : -1;
    if (!S_ISDIR(st.st_mode)) return -1;
    if (!so_datei(SO_MARKE, (off_t)(sizeof SO_MARKE_TEXT - 1))) return -1;
    char puf[64] = {0};
    int fd = open(SO_MARKE, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return -1;
    ssize_t n = read(fd, puf, sizeof puf - 1); close(fd);
    if (n != (ssize_t)(sizeof SO_MARKE_TEXT - 1) || memcmp(puf, SO_MARKE_TEXT, (size_t)n)) return -1;
    static const char *const oben[] = { ".ps5cleanup_start", "eboot.bin", "sce_sys", NULL };
    static const char *const unten[] = { "param.json", NULL };
    if (!so_nur(SO_DIR, oben)) return -1;
    if (!lstat(SO_EBOOT, &st) && !S_ISREG(st.st_mode)) return -1;
    if (!lstat(SO_SYS, &st)) { if (!S_ISDIR(st.st_mode) || !so_nur(SO_SYS, unten)) return -1; if (!lstat(SO_PARAMP, &st) && !S_ISREG(st.st_mode)) return -1; }
    return 1;
}
static int so_schreiben(const char *p, const void *d, size_t n, mode_t m)
{
    int fd = open(p, O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW, m);
    if (fd < 0) return -1;
    size_t ges = 0;
    while (ges < n) { ssize_t w = write(fd, (const char *)d + ges, n - ges); if (w < 0 && errno == EINTR) continue; if (w <= 0) { close(fd); return -1; } ges += (size_t)w; }
    return close(fd) ? -1 : 0;
}
static unsigned char *so_lesen(const char *p, size_t *n)
{
    struct stat st; *n = 0;
    if (lstat(p, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0) return NULL;
    int fd = open(p, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return NULL;
    unsigned char *b = malloc((size_t)st.st_size);
    size_t ges = 0;
    while (b && ges < (size_t)st.st_size) { ssize_t r = read(fd, b + ges, (size_t)st.st_size - ges); if (r < 0 && errno == EINTR) continue; if (r <= 0) { free(b); b = NULL; break; } ges += (size_t)r; }
    close(fd);
    if (b) *n = ges;
    return b;
}
/* Startordner bereitstellen. 0 = bereit; sonst Fehler (Text in t). *umgehaengt: 1 = /system_ex wurde dafür kurz beschreibbar gemacht */
static int so_einrichten(char *t, size_t tl, int *umgehaengt)
{
    *umgehaengt = 0; t[0] = 0;
    int z = so_zustand();
    if (z < 0) { snprintf(t, tl, "Der Ordner %s gehört nicht eindeutig zu PS5 Cleanup - es wird nichts überschrieben.", SO_DIR); return -1; }
    struct stat st;
    if (lstat(SO_PSNOW, &st) || !S_ISREG(st.st_mode)) { snprintf(t, tl, "Die Sony-Datei %s fehlt - Start nicht möglich.", SO_PSNOW); return -1; }
    if (z == 1 && so_datei(SO_EBOOT, st.st_size) && so_datei(SO_PARAMP, (off_t)(sizeof SO_PARAM - 1))) return 0;   /* schon bereit */
    size_t n = 0; unsigned char *eboot = so_lesen(SO_PSNOW, &n);                     /* Sony-Datei: nur lesen */
    if (!eboot) { snprintf(t, tl, "%s ließ sich nicht lesen - Start nicht möglich.", SO_PSNOW); return -1; }
    int ro = so_ist_ro();
    if (ro < 0) { free(eboot); snprintf(t, tl, "Der Einhängezustand von /system_ex ist unbekannt - nichts verändert."); return -1; }
    if (ro) {
        if (so_umhaengen(0)) { free(eboot); snprintf(t, tl, "/system_ex ließ sich nicht beschreibbar machen (%s).", strerror(errno)); return -1; }
        if (so_ist_ro() != 0) {   /* PS5: nur einmal pro Einschalten möglich (Tests 30.10.2026) - nichts verändert */
            free(eboot);
            snprintf(t, tl, "PS5 Cleanup lässt sich erst nach einem Neustart der PS5 wieder starten.");
            return -1;
        }
        *umgehaengt = 1;
    }
    int f = 0;
    if (z == 0) {   /* neu: Ordner, dann SOFORT die Markierung (so bleibt nie ein unmarkierter eigener Ordner) */
        if (mkdir(SO_DIR, 0755)) f = 1;
        else if (so_schreiben(SO_MARKE, SO_MARKE_TEXT, sizeof SO_MARKE_TEXT - 1, 0644)) f = 1;
    }
    if (!f && lstat(SO_SYS, &st) && mkdir(SO_SYS, 0755)) f = 1;
    if (!f && so_schreiben(SO_PARAMP, SO_PARAM, sizeof SO_PARAM - 1, 0644)) f = 1;
    if (!f && so_schreiben(SO_EBOOT, eboot, n, 0755)) f = 1;
    int fe = errno;
    free(eboot);
    int zurueck = ro ? so_schuetzen() : 0;                                            /* vorherigen Zustand wiederherstellen */
    if (f) { snprintf(t, tl, "Der Startordner %s ließ sich nicht anlegen (%s).%s", SO_DIR, strerror(fe), zurueck ? " /system_ex ließ sich nicht wieder schreibgeschützt einhängen." : ""); return -1; }
    if (zurueck) snprintf(t, tl, "Startordner angelegt, aber /system_ex ließ sich nicht wieder schreibgeschützt einhängen (%s).", strerror(errno));
    return so_zustand() == 1 ? 0 : -1;
}
/* Eigene Startdateien entfernen (Selbstentfernung). 1 = entfernt, 0 = war nicht da, -1 = Fehler/fremd (Text in t),
 * -2 = /system_ex ist in dieser Sitzung nicht mehr beschreibbar (nichts verändert; da_Chris86 30.10.2026 00:26: dann ohne Hinweis lassen) */
static int so_entfernen(char *t, size_t tl)
{
    t[0] = 0;
    int z = so_zustand();
    if (z == 0) return 0;
    if (z < 0) { snprintf(t, tl, "%s gehört nicht eindeutig zu PS5 Cleanup - nicht gelöscht.", SO_DIR); return -1; }
    int ro = so_ist_ro();
    if (ro < 0) { snprintf(t, tl, "Einhängezustand von /system_ex unbekannt - Startordner nicht gelöscht."); return -1; }
    if (ro && so_umhaengen(0)) { snprintf(t, tl, "/system_ex ließ sich nicht beschreibbar machen (%s) - Startordner bleibt.", strerror(errno)); return -1; }
    if (ro) {
        int z2 = so_ist_ro();
        if (z2 < 0) { snprintf(t, tl, "Einhängezustand von /system_ex nach dem Umhängen unbekannt - Startordner nicht gelöscht."); return -1; }   /* Review 30.10.2026 */
        if (z2 == 1) { snprintf(t, tl, "/system_ex ist in dieser Sitzung nicht mehr beschreibbar - Startordner bleibt (nichts verändert)."); return -2; }
    }
    int f = 0;
    if (!access(SO_EBOOT, F_OK) && unlink(SO_EBOOT)) f = 1;
    if (!f && !access(SO_PARAMP, F_OK) && unlink(SO_PARAMP)) f = 1;
    if (!f && !access(SO_SYS, F_OK) && rmdir(SO_SYS)) f = 1;
    if (!f && unlink(SO_MARKE)) f = 1;
    if (!f && rmdir(SO_DIR)) f = 1;
    int fe = errno;
    int zurueck = ro ? so_schuetzen() : 0;
    if (f) { snprintf(t, tl, "Startordner %s nicht ganz gelöscht (%s).%s", SO_DIR, strerror(fe), zurueck ? " /system_ex ließ sich nicht wieder schreibgeschützt einhängen." : ""); return -1; }
    if (zurueck) { snprintf(t, tl, "Startordner gelöscht, aber /system_ex ließ sich nicht wieder schreibgeschützt einhängen (%s).", strerror(errno)); return -1; }
    return 1;
}
