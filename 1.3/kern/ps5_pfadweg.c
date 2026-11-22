/* PS5 Cleanup 1.2 - zweiter Weg über den ganzen Pfad (FW 13.x)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * Warum (18.11.2026, Rückmeldungen #9 FW 13.20, #13 und #15 FW 13.60): Eine PS5-App darf auf FW 13.x nicht relativ zu einem geöffneten
 * Ordner öffnen - openat/mkdirat nehmen dort nur AT_FDCWD, jeder echte Ordner-Griff ergibt EINVAL (dokumentiert im Quelltext von Playden,
 * leolobato/playden-ps5 src/platform/directory_walk.h, Hauptplattform FW 13.60; Playden geht dort deshalb jeden Schritt über den Pfad).
 * Unser Löschen öffnet den Weg Ordner für Ordner (kern/zugriff_datei.c eltern_oeffnen) - auf 13.x scheiterte darum jedes Löschen.
 *
 * Der bisherige Weg bleibt der erste: bau/ps5_systemaufrufe.h ruft diese Fassungen NUR, wenn der Aufruf mit einem echten Ordner-Griff
 * mit EINVAL abgelehnt wurde. Dann gilt für jeden Schritt:
 *   - Der Ordner-Griff muss ein Ordner sein, den PS5 Cleanup selbst über seinen Pfad geöffnet und hier gemerkt hat (ps5c_ordner_merken),
 *     er muss noch derselbe sein (Gerät/Knoten) UND sein Pfad muss JETZT noch genau ihn bezeichnen (lstat) - sonst ESTALE, nichts getan.
 *   - Nur ein einzelner Name (kein "/", kein "." oder "..") - sonst EINVAL.
 *   - Öffnen: mit denselben Kennzeichen (O_NOFOLLOW wie beim ersten Weg), das Geöffnete muss genau der Eintrag sein, der unmittelbar davor
 *     unter dem Namen stand; danach wird der Ordner darüber noch einmal geprüft. Sonst geschlossen, ESTALE.
 *   - Ansehen: lstat/stat. Entfernen: unlink/rmdir (beide folgen einer Verknüpfung am Ende nie). Rechte: nie für eine Verknüpfung.
 * Alle Prüfungen von zugriff_datei.c (keine Verknüpfung, keine Einbindung, Ziel derselbe wie gesehen) laufen unverändert darüber.
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef NURLESEN_OHNE_UNISTD
#include <unistd.h>
#endif

#ifdef PS5C_PFADWEG
_Atomic int ps5c_relativ_abgelehnt = 0;   /* NEU 18.11.2026: einmal EINVAL mit Ordner-Griff gesehen = FW 13.x (bau/ps5_systemaufrufe.h); atomar (Codex) - nur 0->1 */
#define PW_MAX_FD 4096
typedef struct { char *pfad; dev_t dev; ino_t ino; } pw_eintrag;
static pw_eintrag g_pw[PW_MAX_FD];
static pthread_mutex_t g_pw_sperre = PTHREAD_MUTEX_INITIALIZER;

void ps5c_ordner_merken(int fd, const char *pfad)
{
    struct stat st;
    if (fd < 0 || fd >= PW_MAX_FD || !pfad || pfad[0] != '/' || fstat(fd, &st) != 0 || !S_ISDIR(st.st_mode)) return;
    char *k = strdup(pfad);
    if (!k) return;
    pthread_mutex_lock(&g_pw_sperre);
    free(g_pw[fd].pfad);
    g_pw[fd].pfad = k; g_pw[fd].dev = st.st_dev; g_pw[fd].ino = st.st_ino;
    pthread_mutex_unlock(&g_pw_sperre);
}
/* Pfad des gehaltenen Ordners fd - nur wenn fd noch der gemerkte Ordner ist UND der Pfad jetzt genau ihn bezeichnet */
static int pw_pfad_von(int fd, char *aus, size_t n)
{
    struct stat a, b;
    if (fd < 0 || fd >= PW_MAX_FD || fstat(fd, &a) != 0) { errno = EBADF; return -1; }
    int da = 0;
    pthread_mutex_lock(&g_pw_sperre);
    if (g_pw[fd].pfad && g_pw[fd].dev == a.st_dev && g_pw[fd].ino == a.st_ino && strlen(g_pw[fd].pfad) < n) { strcpy(aus, g_pw[fd].pfad); da = 1; }
    pthread_mutex_unlock(&g_pw_sperre);
    if (!da) { errno = EINVAL; return -1; }                       /* nicht von uns über den Pfad geöffnet: wie die PS5 ablehnen */
    if (lstat(aus, &b) != 0 || !S_ISDIR(b.st_mode) || b.st_dev != a.st_dev || b.st_ino != a.st_ino) { errno = ESTALE; return -1; }
    return 0;
}
static int pw_voll(int fd, const char *name, char *aus, size_t n)
{
    if (!name || !name[0] || strchr(name, '/') || !strcmp(name, ".") || !strcmp(name, "..")) { errno = EINVAL; return -1; }
    char p[PATH_MAX];
    if (pw_pfad_von(fd, p, sizeof p) != 0) return -1;
    int r = snprintf(aus, n, "%s%s%s", p, strcmp(p, "/") ? "/" : "", name);
    if (r < 0 || (size_t)r >= n) { errno = ENAMETOOLONG; return -1; }
    return 0;
}
static int pw_darueber_gleich(int fd) { char q[PATH_MAX]; return pw_pfad_von(fd, q, sizeof q) == 0; }

int ps5c_pfad_openat(int fd, const char *name, int fl, int mode)
{
    char p[PATH_MAX];
    if (pw_voll(fd, name, p, sizeof p) != 0) return -1;
    struct stat vor, nach;
    int da = lstat(p, &vor) == 0;
    int r = open(p, fl, mode);
    if (r < 0) return -1;
    int neu = 0;
#ifndef PS5C_NURLESEN_H
    neu = !da && (fl & O_CREAT);                                   /* neu angelegt (STOR mit O_EXCL) - davor gab es nichts zu vergleichen */
#endif
    if (fstat(r, &nach) != 0 || (!neu && (!da || nach.st_dev != vor.st_dev || nach.st_ino != vor.st_ino)) || !pw_darueber_gleich(fd)) {
        close(r); errno = ESTALE; return -1;
    }
    if (S_ISDIR(nach.st_mode)) ps5c_ordner_merken(r, p);
    return r;
}
int ps5c_pfad_fstatat(int fd, const char *name, struct stat *st, int fl)
{
    char p[PATH_MAX];
    if (pw_voll(fd, name, p, sizeof p) != 0) return -1;
    return (fl & AT_SYMLINK_NOFOLLOW) ? lstat(p, st) : stat(p, st);
}
#ifndef PS5C_NURLESEN_H
int ps5c_pfad_unlinkat(int fd, const char *name, int fl)
{
    char p[PATH_MAX];
    if (pw_voll(fd, name, p, sizeof p) != 0) return -1;
    return (fl & AT_REMOVEDIR) ? rmdir(p) : unlink(p);              /* beide folgen einer Verknüpfung am Ende nie */
}
int ps5c_pfad_fchmodat(int fd, const char *name, mode_t m, int fl)
{
    char p[PATH_MAX];
    if (pw_voll(fd, name, p, sizeof p) != 0) return -1;
    if (fl & AT_SYMLINK_NOFOLLOW) { struct stat s; if (lstat(p, &s) != 0) return -1; if (S_ISLNK(s.st_mode)) { errno = EPERM; return -1; } }
    return chmod(p, m);
}
int ps5c_pfad_renameat(int fa, const char *a, int fb, const char *b)
{
    char pa[PATH_MAX], pb[PATH_MAX];
    if (pw_voll(fa, a, pa, sizeof pa) != 0 || pw_voll(fb, b, pb, sizeof pb) != 0) return -1;
    return rename(pa, pb);
}
int ps5c_pfad_mkdirat(int fd, const char *name, mode_t m)
{
    char p[PATH_MAX];
    if (pw_voll(fd, name, p, sizeof p) != 0) return -1;
    return mkdir(p, m);
}
#endif
#endif
