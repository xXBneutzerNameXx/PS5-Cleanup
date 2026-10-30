/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * zugriff_datei.c - Zugriffsschicht direkt aufs Dateisystem.
 *
 * Verhält sich bei jedem Grundbefehl so wie der FTP-Server, mit dem 1.0.5
 * getestet wurde (miniftp4.py): gleiche Codes, gleiche Texte, gleiche
 * Reihenfolge der Ordnerliste (sortiert), Verknüpfungen werden nie verfolgt.
 *
 * SICHERHEIT: Die verändernden Befehle (STOR, SITE CHMOD, DELE, RMD, RMDA)
 * sind nur eingebaut, wenn mit -DKERN_LOESCHEN gebaut wird. Das PS5-Programm
 * wird bis zur ausdrücklichen Freigabe OHNE diesen Schalter gebaut - dann
 * antwortet jeder dieser Befehle mit "550 nicht eingebaut" und nichts passiert.
 */
#define _GNU_SOURCE
#include "zugriff.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef KERN_LOESCHEN
#ifdef __linux__
#include <sys/sysmacros.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
#endif
#endif

typedef struct {
    datei_optionen o;
    bool tot;                    /* DIEON: Verbindung "abgerissen" */
    pthread_t sm; bool sm_laeuft; volatile bool sm_ende;
} dz;

bool zugriff_datei_kann_loeschen(void)
{
#ifdef KERN_LOESCHEN
    return true;
#else
    return false;
#endif
}

static antwort A(arena *a, int code, const char *text) { antwort r = { code, a_kopie(a, text) }; return r; }

/* posixpath.normpath + "führendes // -> /" wie real() in miniftp4.py */
static const char *norm(arena *a, const char *p)
{
    size_t n = strlen(p);
    const char **teil = a_hole(a, sizeof(char *) * (n + 1));
    int k = 0;
    char *kopie = a_kopie(a, p);
    char *sp = NULL;
    for (char *s = strtok_r(kopie, "/", &sp); s; s = strtok_r(NULL, "/", &sp)) {
        if (strcmp(s, ".") == 0) continue;
        if (strcmp(s, "..") == 0) { if (k) k--; continue; }
        teil[k++] = s;
    }
    if (!k) return "/";
    size_t l = 1; for (int i = 0; i < k; i++) l += strlen(teil[i]) + 1;
    char *d = a_hole(a, l + 1), *o = d;
    for (int i = 0; i < k; i++) { *o++ = '/'; size_t x = strlen(teil[i]); memcpy(o, teil[i], x); o += x; }
    *o = 0;
    return d;
}
static const char *echt(dz *d, arena *a, const char *v) { return VERB(a, d->o.wurzel, v); }

static const char *env(dz *d, const char *name) { if (!d->o.test_einspielungen) return NULL; const char *e = getenv(name); return (e && *e) ? e : NULL; }

/* DIEON: enthält die Befehlszeile den Text, reißt die "Verbindung" ab */
static bool stirbt(dz *d, const char *zeile)
{
    if (d->tot) return true;
    const char *x = env(d, "DIEON");
    if (x && strstr(zeile, x)) d->tot = true;
    return d->tot;
}
static const antwort TOT = { -1, "" };

static bool liste_hat(const char *liste, const char *v)   /* "a,b,c" */
{
    if (!liste) return false;
    char buf[4096]; snprintf(buf, sizeof buf, "%s", liste);
    char *sp = NULL;
    for (char *s = strtok_r(buf, ",", &sp); s; s = strtok_r(NULL, ",", &sp)) if (*s && strcmp(s, v) == 0) return true;
    return false;
}
static bool fake(dz *d, const char *v)                     /* FAKELINKS: Ordner, die als Verknüpfung gemeldet werden */
{
    const char *f = env(d, "FAKELINKS");
    if (!f) return false;
    char buf[4096]; snprintf(buf, sizeof buf, "%s", f);
    char *sp = NULL;
    for (char *s = strtok_r(buf, ",", &sp); s; s = strtok_r(NULL, ",", &sp)) {
        while (*s == '/') s++;
        size_t l = strlen(s); while (l && s[l - 1] == '/') s[--l] = 0;
        if (l && strcmp(v + 1, s) == 0) return true;
    }
    return false;
}

static bool ist_ordner(const char *r) { struct stat st; return stat(r, &st) == 0 && S_ISDIR(st.st_mode); }
static bool ist_datei(const char *r) { struct stat st; return stat(r, &st) == 0 && S_ISREG(st.st_mode); }
#ifdef KERN_LOESCHEN
static bool da(const char *r) { struct stat st; return stat(r, &st) == 0; }
#endif

/* ---------------------------------------------------------------- lesen */
static antwort d_cwd(zugriff *z, arena *a, const char *pfad)
{
    dz *d = z->zustand;
    if (stirbt(d, VERB(a, "CWD ", pfad))) return TOT;
    const char *v = norm(a, pfad);
    if (liste_hat(env(d, "DENY"), v)) return A(a, 550, "550 Permission denied");
    const char *r = echt(d, a, v);
    if (ist_ordner(r) && access(r, R_OK | X_OK) == 0) return A(a, 250, "250 OK");
    return A(a, 550, "550 no");
}

static int vergleiche(const void *x, const void *y) { return strcmp(*(const char *const *)x, *(const char *const *)y); }

static antwort d_liste(zugriff *z, arena *a, const char *pfad, kdliste *aus)
{
    dz *d = z->zustand;
    aus->e = NULL; aus->n = 0;
    if (stirbt(d, "LIST")) return TOT;
    const char *v = norm(a, pfad), *r = echt(d, a, v);
    const char *fl = env(d, "FAILLIST");
    if (fl && strcmp(v, fl) == 0) {
        const char *flag = VERB(a, d->o.test_hier ? d->o.test_hier : ".", "/faillist.on");
        if (access(flag, F_OK) == 0) return A(a, 451, "451 read error");
    }
    DIR *dir = opendir(r);
    if (!dir) return A(a, 550, VERB(a, "550 ", strerror(errno)));
    sliste namen = {0};
    struct dirent *de;
    for (;;) {
        errno = 0;
        if (!(de = readdir(dir))) break;
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        sl_add(a, &namen, a_kopie(a, de->d_name));
    }
    int lesefehler = errno;                    /* NEU 1.1 (S3): Fehler beim Lesen der Liste nicht verschlucken */
    closedir(dir);
    if (lesefehler) return A(a, 451, VERB(a, "451 ", strerror(lesefehler)));
    if (d->o.sortiert && namen.n > 1) qsort(namen.e, (size_t)namen.n, sizeof(char *), vergleiche);
    aus->e = a_hole(a, sizeof(kdatei) * (size_t)(namen.n + 2));
    /* wie LIST: zuerst "." und ".." */
    const char *pkt[2] = { ".", ".." };
    for (int i = 0; i < 2; i++) { aus->e[aus->n].name = pkt[i]; aus->e[aus->n].ordner = true; aus->n++; }
    for (int i = 0; i < namen.n; i++) {
        const char *rv = VERB(a, r, "/", namen.e[i]);
        const char *vv = strcmp(v, "/") == 0 ? VERB(a, "/", namen.e[i]) : VERB(a, v, "/", namen.e[i]);
        struct stat st;
        if (lstat(rv, &st) != 0) {
            if (errno == ENOENT) continue;     /* inzwischen weg - wie vorher */
            aus->n = 0; aus->e = NULL;         /* NEU 1.1 (S3): Eintrag nicht lesbar -> Liste unvollständig = Fehler */
            return A(a, 451, VERB(a, "451 ", strerror(errno)));
        }
        kdatei *e = &aus->e[aus->n++];
        e->name = namen.e[i];
        e->link = S_ISLNK(st.st_mode) || fake(d, vv);
        e->ordner = !e->link && S_ISDIR(st.st_mode);
        e->groesse = (S_ISDIR(st.st_mode) && d->o.ordnergroesse_null) ? 0 : (long long)st.st_size;
    }
    return A(a, 226, "226 done");
}

static antwort d_lies(zugriff *z, arena *a, const char *pfad, const unsigned char **daten, size_t *n)
{
    dz *d = z->zustand;
    *daten = NULL; *n = 0;
    if (stirbt(d, VERB(a, "RETR ", pfad))) return TOT;
    const char *r = echt(d, a, norm(a, pfad));
    if (!ist_datei(r)) return A(a, 550, "550 no");
    FILE *f = fopen(r, "rb");
    if (!f) return A(a, 550, VERB(a, "550 ", strerror(errno)));
    size_t cap = 65536, len = 0; unsigned char *b = malloc(cap); size_t k;
    if (!b) { fclose(f); return A(a, 451, "451 kein Speicher"); }
    while ((k = fread(b + len, 1, cap - len, f)) > 0) {
        len += k;
        if (len == cap) { unsigned char *nb = realloc(b, cap * 2); if (!nb) { free(b); fclose(f); return A(a, 451, "451 kein Speicher"); } b = nb; cap *= 2; }
    }
    int lesefehler = ferror(f);                    /* NEU 1.1 (Codex-Prüfung 26.10.): Lesefehler nie als Teilinhalt melden */
    fclose(f);
    if (lesefehler) { free(b); return A(a, 451, "451 Lesefehler"); }
    unsigned char *ad = a_hole(a, len + 1); memcpy(ad, b, len); free(b);
    *daten = ad; *n = len;
    return A(a, 226, "226 done");
}

/* NEU 1.1 (S2): nur den Dateianfang lesen (für die Erkennung von PS5 Cleanup an jedem Ort).
 * Eine Verknüpfung wird nicht verfolgt. Nur lesen - auch im PS5-Bau. */
/* NEU 1.0.6 Fix (30.10.2026, Speicheranzeige auf da_Chris86s PS5: 151 MB nach dem Nachschauen): bis 1.0.6 wurde jede ELF für die
 * Suche nach der eigenen Kennung GANZ eingelesen und blieb im Speicher (orbit_store.elf: 65 MB). Jetzt in Stücken zu 1 MB. */
static antwort d_enthaelt(zugriff *z, arena *a, const char *pfad, const void *m, size_t ml, bool *ja)
{
    dz *d = z->zustand;
    *ja = false;
    if (stirbt(d, VERB(a, "RETR ", pfad))) return TOT;
    const char *r = echt(d, a, norm(a, pfad));
    if (!ist_datei(r)) return A(a, 550, "550 no");
    if (ml == 0 || ml > 4096) return A(a, 501, "501 Muster");
    FILE *f = fopen(r, "rb");
    if (!f) return A(a, 550, VERB(a, "550 ", strerror(errno)));
    enum { STUECK = 1 << 20 };
    unsigned char *b = malloc(STUECK + ml);
    if (!b) { fclose(f); return A(a, 451, "451 kein Speicher"); }
    size_t rest = 0, k;
    while (!*ja && (k = fread(b + rest, 1, STUECK, f)) > 0) {
        size_t n = rest + k;
        if (n >= ml && basis_suche(b, n, m, ml)) *ja = true;
        rest = n < ml - 1 ? n : ml - 1;                /* Überlappung: die Kennung kann über eine Stückgrenze gehen */
        memmove(b, b + n - rest, rest);
    }
    int lesefehler = ferror(f);
    fclose(f); free(b);
    if (lesefehler && !*ja) return A(a, 451, "451 Lesefehler");
    return A(a, 226, "226 done");
}
static antwort d_kopf(zugriff *z, arena *a, const char *pfad, size_t n, const unsigned char **daten, size_t *len)
{
    dz *d = z->zustand;
    *daten = NULL; *len = 0;
    if (stirbt(d, VERB(a, "KOPF ", pfad))) return TOT;
    const char *r = echt(d, a, norm(a, pfad));
    int fd = open(r, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return A(a, 550, VERB(a, "550 ", strerror(errno)));
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) { close(fd); return A(a, 550, "550 no"); }
    unsigned char *b = a_hole(a, n + 1);
    size_t got = 0; ssize_t k = 0;
    while (got < n && (k = read(fd, b + got, n - got)) > 0) got += (size_t)k;
    int fehler = k < 0 ? errno : 0;
    close(fd);
    if (fehler) return A(a, 451, VERB(a, "451 ", strerror(fehler)));
    *daten = b; *len = got;
    return A(a, 226, "226 done");
}

/* ---------------------------------------------------------------- schreiben / löschen */
#ifdef KERN_LOESCHEN
/* NEU 1.1 (S4): Löschen gegen veränderte Verknüpfungen und Einbindungen abgesichert.
 *  - Jeder Ordner auf dem Weg zum Ziel wird ohne Verknüpfung geöffnet (O_NOFOLLOW): wurde ein Teil
 *    des Weges inzwischen gegen eine Verknüpfung getauscht, schlägt das Öffnen fehl - nichts gelöscht.
 *  - Gelöscht wird relativ zum geöffneten Ordner (unlinkat), nie über einen Pfadtext.
 *  - Ein Ordner wird nur gelöscht, wenn weder er selbst noch irgendetwas darin eine Einbindung
 *    (anderes Dateisystem / Mount-Punkt) ist. Das wird VOR dem ersten Löschen ganz geprüft.
 *  - Verknüpfungen im Baum werden nie verfolgt, nur selbst entfernt.
 *  - Rechte (SITE CHMOD) nur für echte Dateien/Ordner, nie für eine Verknüpfung (würde ihr Ziel ändern). */
static int einbindung(int pfd, int fd);
/* NEU 1.1 (Codex-Prüfung 26.10.2026): Ein Wechsel des Dateisystems ist auf dem Weg zum Ziel nur an den
 * bekannten Speicherwurzeln erlaubt (/user, /system_data, M.2, USB-Erweiterung, USB-Sticks). Jede andere
 * Einbindung (z. B. ein anderer Ordner zusätzlich unter /mnt/ext1/... eingehängt) -> abgelehnt, nichts verändert. */
static bool speicherwurzel(const char *weg)
{
    static const char *w[] = { "/mnt", "/user", "/system_data", "/mnt/ext0", "/mnt/ext1", NULL };   /* /mnt: reiner Sammelordner */
    for (int i = 0; w[i]; i++) if (strcmp(weg, w[i]) == 0) return true;
    return strncmp(weg, "/mnt/usb", 8) == 0 && weg[8] >= '0' && weg[8] <= '9' && weg[9] == 0;
}
/* An einer Speicherwurzel nur ein echtes Dateisystem, keine Umleitung eines anderen Ordners (nullfs/unionfs). */
static bool speicherwurzel_echt(int fd, const char *weg)
{
    if (!speicherwurzel(weg)) return false;
#ifndef __linux__
    struct statfs sf;
    if (fstatfs(fd, &sf) != 0) return false;
    if (strcmp(sf.f_fstypename, "nullfs") == 0 || strcmp(sf.f_fstypename, "unionfs") == 0) return false;
#else
    (void)fd;
#endif
    return true;
}
static int eltern_oeffnen(dz *d, arena *a, const char *v, const char **blatt)
{
    *blatt = NULL;
    const char *w = d->o.wurzel[0] ? d->o.wurzel : "/";
    int fd = open(w, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return -1;
    char *kopie = a_kopie(a, v), *sp = NULL;
    char *teil = strtok_r(kopie, "/", &sp);
    if (!teil) { close(fd); errno = EINVAL; return -1; }          /* "/" selbst nie */
    char weg[4096]; weg[0] = 0;
    for (;;) {
        char *naechster = strtok_r(NULL, "/", &sp);
        if (!naechster) { *blatt = teil; return fd; }
        int nfd = openat(fd, teil, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (nfd < 0) { int e = errno; close(fd); errno = e; return -1; }
        size_t lw = strlen(weg), lt = strlen(teil);
        if (lw + lt + 2 > sizeof weg) { close(nfd); close(fd); errno = ENAMETOOLONG; return -1; }
        weg[lw] = '/'; memcpy(weg + lw + 1, teil, lt + 1);
        int ein = einbindung(fd, nfd);
        close(fd);
        if (ein < 0 || (ein > 0 && !speicherwurzel_echt(nfd, weg))) { close(nfd); errno = ein < 0 ? errno : EXDEV; return -1; }
        fd = nfd; teil = naechster;
    }
}
/* 1 = fd (Ordner) ist eine Einbindung gegenüber pfd, 0 = nein, -1 = Fehler */
static int einbindung(int pfd, int fd)
{
    struct stat a, b;
    if (fstat(pfd, &a) != 0 || fstat(fd, &b) != 0) return -1;
    if (a.st_dev != b.st_dev) return 1;
#ifdef __linux__
#ifdef STATX_ATTR_MOUNT_ROOT
    struct statx sx;
    if (statx(fd, "", AT_EMPTY_PATH, STATX_BASIC_STATS, &sx) == 0 && (sx.stx_attributes_mask & STATX_ATTR_MOUNT_ROOT) && (sx.stx_attributes & STATX_ATTR_MOUNT_ROOT)) return 1;
#endif
#else
    struct statfs fa, fb;
    if (fstatfs(pfd, &fa) != 0 || fstatfs(fd, &fb) != 0) return -1;
    if (strcmp(fa.f_mntonname, fb.f_mntonname) != 0) return 1;   /* z. B. nullfs: gleiche Geräte-Nr., anderer Mount */
#endif
    return 0;
}
/* NEU 1.1 (Codex-Prüfung 26.10.2026): Rechte eines Ordners nur über den geöffneten Ordner ändern - ohne Verknüpfung
 * geöffnet, derselbe wie geprüft, keine Einbindung. Lässt er sich nicht öffnen: abgelehnt (nie "im Zweifel ja"). */
static antwort ordner_chmod(arena *a, int pfd, const char *name, const struct stat *st)
{
    int fd = openat(pfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return A(a, 550, VERB(a, "550 ", strerror(errno)));
    struct stat s2;
    antwort erg;
    if (fstat(fd, &s2) != 0 || s2.st_ino != st->st_ino || s2.st_dev != st->st_dev) erg = A(a, 550, "550 inzwischen getauscht - nicht veraendert");
    else if (einbindung(pfd, fd) != 0) erg = A(a, 550, "550 Einbindung - nicht veraendert");
    else if (fchmod(fd, 0777) != 0) erg = A(a, 550, VERB(a, "550 ", strerror(errno)));
    else erg = A(a, 200, "200 OK");
    close(fd);
    return erg;
}
static int kind_ordner(int pfd, const char *name) { return openat(pfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); }
/* Namen eines geöffneten Ordners (ohne . und ..); NULL bei Lesefehler */
static sliste *namen_von(int fd, arena *a)
{
    int d2 = dup(fd);
    if (d2 < 0) return NULL;
    DIR *dir = fdopendir(d2);
    if (!dir) { close(d2); return NULL; }
    rewinddir(dir);                            /* dup teilt die Leseposition - immer von vorn lesen */
    sliste *l = a_hole(a, sizeof *l);
    struct dirent *de;
    for (;;) {
        errno = 0;
        if (!(de = readdir(dir))) break;
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        sl_add(a, l, a_kopie(a, de->d_name));
    }
    int e = errno; closedir(dir);
    if (e) { errno = e; return NULL; }
    return l;
}
/* ganzen Baum prüfen, bevor etwas gelöscht wird: keine Einbindung, alles lesbar */
static int baum_pruefen(int fd, arena *a, int tiefe)
{
    if (tiefe > 256) { errno = ELOOP; return -1; }
    sliste *l = namen_von(fd, a);
    if (!l) return -1;
    for (int i = 0; i < l->n; i++) {
        struct stat st;
        if (fstatat(fd, l->e[i], &st, AT_SYMLINK_NOFOLLOW) != 0) { if (errno == ENOENT) continue; return -1; }
        if (!S_ISDIR(st.st_mode)) continue;
        int c = kind_ordner(fd, l->e[i]);
        if (c < 0) return -1;
        int e = einbindung(fd, c);
        if (e != 0) { close(c); if (e > 0) errno = EXDEV; return -1; }
        int r = baum_pruefen(c, a, tiefe + 1);
        int s2 = errno; close(c); errno = s2;
        if (r) return -1;
    }
    return 0;
}
static int baum_loeschen(int fd, arena *a, int tiefe)
{
    if (tiefe > 256) { errno = ELOOP; return -1; }
    sliste *l = namen_von(fd, a);
    if (!l) return -1;
    for (int i = 0; i < l->n; i++) {
        struct stat st;
        if (fstatat(fd, l->e[i], &st, AT_SYMLINK_NOFOLLOW) != 0) { if (errno == ENOENT) continue; return -1; }
        if (S_ISDIR(st.st_mode)) {
            int c = kind_ordner(fd, l->e[i]);
            if (c < 0) return -1;
            int e = einbindung(fd, c);                 /* nochmal: inzwischen eingebunden? */
            if (e != 0) { close(c); if (e > 0) errno = EXDEV; return -1; }
            int r = baum_loeschen(c, a, tiefe + 1);
            int s2 = errno; close(c); errno = s2;
            if (r) return -1;
            if (unlinkat(fd, l->e[i], AT_REMOVEDIR) != 0) return -1;
        }
        else if (unlinkat(fd, l->e[i], 0) != 0) return -1;   /* Datei oder Verknüpfung selbst */
    }
    return 0;
}
/* Ordner name in pfd samt Inhalt entfernen (wie shutil.rmtree: auf eine Verknüpfung = Fehler) */
static int baum_weg_at(int pfd, const char *name)
{
    struct stat st;
    if (fstatat(pfd, name, &st, AT_SYMLINK_NOFOLLOW) != 0) return -1;
    if (S_ISLNK(st.st_mode)) { errno = ELOOP; return -1; }
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }
    int fd = kind_ordner(pfd, name);
    if (fd < 0) return -1;
    struct stat st2;
    if (fstat(fd, &st2) != 0 || st2.st_ino != st.st_ino || st2.st_dev != st.st_dev) { close(fd); errno = EAGAIN; return -1; }   /* inzwischen getauscht */
    int e = einbindung(pfd, fd);
    if (e != 0) { close(fd); if (e > 0) errno = EXDEV; return -1; }
    arena *a = arena_neu();
    int r = baum_pruefen(fd, a, 0);
    if (r == 0) r = baum_loeschen(fd, a, 0);
    int s2 = errno; arena_frei(a); close(fd); errno = s2;
    if (r) return -1;
    return unlinkat(pfd, name, AT_REMOVEDIR);
}
/* Pfadtext -> sicher löschen (für die Test-Nachbildung des ShadowMount) */
static int baum_weg(const char *r)
{
    char *k = strdup(r), *sl = strrchr(k, '/');
    if (!sl || sl == k) { free(k); errno = EINVAL; return -1; }
    *sl = 0;
    int pfd = open(k, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    int e = errno;
    if (pfd < 0) { free(k); errno = e; return -1; }
    int x = baum_weg_at(pfd, sl + 1);
    e = errno; close(pfd); free(k); errno = e;
    return x;
}
#endif

static antwort d_schreibe(zugriff *z, arena *a, const char *pfad, const void *daten, size_t n)
{
    dz *d = z->zustand;
    if (stirbt(d, VERB(a, "STOR ", pfad))) return TOT;
#ifdef KERN_LOESCHEN
    const char *v = norm(a, pfad);
    if (env(d, "STOPFAIL") && txt_endet(v, "/STOP")) return A(a, 550, "550 Permission denied");
    const char *bl; int pfd = eltern_oeffnen(d, a, v, &bl);              /* NEU 1.1 (S4) */
    if (pfd < 0) return A(a, 550, VERB(a, "550 ", strerror(errno)));
    /* NEU 1.1 (Codex-Prüfung 26.10.2026): nicht beim Öffnen leeren (O_TRUNC) - erst prüfen, dass es eine
     * gewöhnliche Datei ohne weitere harte Verknüpfung ist. Sonst könnte über einen harten Link eine
     * geschützte Datei geleert werden. */
    int fd = openat(pfd, bl, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0666);
    int e = errno;
    if (fd < 0 && e == EEXIST) {
        /* Datei gibt es schon: NIE abschneiden oder überschreiben (könnte ein harter Link auf etwas Wichtiges sein).
         * Eine leere Meldedatei (STOP) ist dann schon da - das reicht. Alles andere: abgelehnt. */
        struct stat sst;
        bool ok = n == 0 && fstatat(pfd, bl, &sst, AT_SYMLINK_NOFOLLOW) == 0 && S_ISREG(sst.st_mode);
        close(pfd);
        return ok ? A(a, 226, "226 schon vorhanden - unveraendert") : A(a, 550, "550 vorhanden - nicht veraendert");
    }
    close(pfd);
    if (fd < 0) return A(a, 550, VERB(a, "550 ", strerror(e)));
    size_t gesch = 0; ssize_t k = 0;
    while (gesch < n && (k = write(fd, (const char *)daten + gesch, n - gesch)) > 0) gesch += (size_t)k;
    close(fd);
    if (gesch != n) return A(a, 550, "550 write error");
    return A(a, 226, "226 done");
#else
    (void)daten; (void)n;
    return A(a, 550, "550 nicht eingebaut");
#endif
}

/* NEU 1.0.6: Sonys Deinstallations-Dienst (siehe zugriff.h) */
antwort (*zugriff_sony_deinstallieren)(arena *a, const char *id, bool nur_pruefen) = NULL;
int (*zugriff_startordner_entfernen)(arena *a, bool nur_pruefen, const char **text) = NULL;   /* NEU 1.0.6 */
int (*zugriff_selbst_uebergeben)(arena *a, bool kachel, bool startordner, const char **text) = NULL;   /* NEU 1.0.6 */
void (*zugriff_diagnose)(const char *text) = NULL;   /* NEU 1.0.6 */
static const char *kennung_aus(arena *a, const char *p) { const char *s = strrchr(p, '/'); return a_kopie(a, s ? s + 1 : p); }
static antwort d_sony(dz *d, arena *a, const char *befehl, const char *pfad)
{
    bool pruefen = strcmp(befehl, "SITE UNINSTALL?") == 0;
    if (!pruefen && strcmp(befehl, "SITE UNINSTALL") != 0) return A(a, 502, "502 no");
    const char *t = env(d, "UNINSTALL");                          /* Test am PC */
    if (t) {
        if (pfad == NULL) return A(a, 200, "200 OK");
        const char *id = kennung_aus(a, pfad), *nie = env(d, "UNINSTALL_NIE");
        if (nie && strcmp(nie, id) == 0) return A(a, 550, "550 PS5 Cleanup läuft gerade über diese App");
        if (pruefen) return A(a, 200, "200 OK");
        if (strcmp(t, "ablehnen") == 0) return A(a, 550, "550 Sony 0x80990001");
#ifdef KERN_LOESCHEN
        if (strcmp(t, "ok") == 0) {                               /* wie Sony: App-Ordner und Kachel-Infos weg */
            baum_weg(echt(d, a, norm(a, pfad)));
            baum_weg(echt(d, a, VERB(a, "/user/appmeta/", id)));
            baum_weg(echt(d, a, VERB(a, "/system_data/priv/appmeta/", id)));
        }
#endif
        return A(a, 200, "200 OK");                               /* "langsam": angenommen, aber es passiert nichts */
    }
    if (zugriff_sony_deinstallieren == NULL) return A(a, 502, "502 no");
    if (pfad == NULL) return A(a, 200, "200 OK");
    return zugriff_sony_deinstallieren(a, kennung_aus(a, pfad), pruefen);
}

static antwort d_befehl(zugriff *z, arena *a, const char *befehl, const char *pfad)
{
    dz *d = z->zustand;
    if (stirbt(d, pfad ? VERB(a, befehl, " ", pfad) : befehl)) return TOT;
    if (strcmp(befehl, "NOOP") == 0) return A(a, 200, "200 NOOP OK");
    if (strncmp(befehl, "SITE UNINSTALL", 14) == 0) return d_sony(d, a, befehl, pfad);
#ifdef KERN_LOESCHEN
    const char *v = norm(a, pfad), *r = echt(d, a, v);
    const char *bl = NULL;
    int pfd = eltern_oeffnen(d, a, v, &bl);           /* NEU 1.1 (S4): Weg ohne Verknüpfungen */
    if (pfd < 0) return A(a, 550, VERB(a, "550 ", strerror(errno)));
    antwort erg = A(a, 502, "502 no");
    struct stat st;
    bool hat = fstatat(pfd, bl, &st, AT_SYMLINK_NOFOLLOW) == 0;
    if (strcmp(befehl, "SITE CHMOD 777") == 0) {
        if (!hat || (S_ISLNK(st.st_mode) && !da(r))) erg = A(a, 550, "550 no");
        else if (S_ISLNK(st.st_mode)) erg = A(a, 550, "550 Verknuepfung - nicht veraendert");   /* NEU 1.1: nie das Ziel ändern */
        else if (S_ISDIR(st.st_mode)) erg = ordner_chmod(a, pfd, bl, &st);
        /* NEU 1.1 (Codex-Prüfung 26.10.2026): Datei mit AT_SYMLINK_NOFOLLOW - wird zwischen Prüfung und Änderung eine
         * Verknüpfung eingesetzt, wird höchstens die Verknüpfung selbst geändert (FreeBSD) bzw. abgelehnt (Linux),
         * nie ihr Ziel. Eine Datei kann keine Einbindung sein. */
        else if (fchmodat(pfd, bl, 0777, AT_SYMLINK_NOFOLLOW) != 0) erg = A(a, 550, VERB(a, "550 ", strerror(errno)));
        else erg = A(a, 200, "200 OK");
        close(pfd);
        return erg;
    }
    bool loesch = strcmp(befehl, "DELE") == 0 || strcmp(befehl, "RMD") == 0 || strcmp(befehl, "RMDA") == 0;
    const char *fd = env(d, "FAILDEL");
    if (loesch && fd && txt_beginnt(v, fd)) erg = A(a, 550, "550 Permission denied");
    else if (strcmp(befehl, "DELE") == 0) {
        if (fake(d, v)) erg = baum_weg_at(pfd, bl) != 0 ? A(a, 550, VERB(a, "550 ", strerror(errno))) : A(a, 250, "250 OK");
        else if (hat && (S_ISLNK(st.st_mode) || S_ISREG(st.st_mode)))
            erg = unlinkat(pfd, bl, 0) != 0 ? A(a, 550, VERB(a, "550 ", strerror(errno))) : A(a, 250, "250 OK");
        else erg = A(a, 550, "550 no");
    }
    else if (strcmp(befehl, "RMD") == 0) {
        erg = (hat && S_ISDIR(st.st_mode) && unlinkat(pfd, bl, AT_REMOVEDIR) == 0) ? A(a, 250, "250 OK") : A(a, 550, "550 Directory not empty");
    }
    else if (strcmp(befehl, "RMDA") == 0) {
        const char *x = getenv("RMDA");
        if (d->o.test_einspielungen && (!x || strcmp(x, "1") != 0)) erg = A(a, 502, "502 not implemented");
        else if (!hat) erg = A(a, 550, "550 no");
        else erg = baum_weg_at(pfd, bl) != 0 ? A(a, 550, VERB(a, "550 ", strerror(errno))) : A(a, 250, "250 OK");
    }
    close(pfd);
    return erg;
#else
    (void)pfad;
    return A(a, 550, "550 nicht eingebaut");
#endif
}

/* ---------------------------------------------------------------- Test: Fake-ShadowMount (wie miniftp4.py) */
#ifdef KERN_LOESCHEN
static void *fake_sm(void *arg)
{
    dz *d = arg;
    char sm[4096], stop[4200], logd[4200];
    snprintf(sm, sizeof sm, "%s/user/data/shadowmount", d->o.wurzel);
    snprintf(stop, sizeof stop, "%s/STOP", sm);
    snprintf(logd, sizeof logd, "%s/debug.log", sm);
    const char *art = getenv("FAKESM");
    while (!d->sm_ende) {
        sleep(1);
        if (d->sm_ende) break;
        if (ist_datei(stop)) {
            unlink(stop); sleep(3);
            if (art && strcmp(art, "stumm") == 0) {
                char mp[4200]; snprintf(mp, sizeof mp, "%s/mnt/shadowmnt", d->o.wurzel);
                DIR *dir = opendir(mp);
                if (dir) {
                    struct dirent *de;
                    while ((de = readdir(dir))) {
                        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
                        char p[8400]; snprintf(p, sizeof p, "%s/%s", mp, de->d_name);
                        baum_weg(p); mkdir(p, 0777);
                    }
                    closedir(dir);
                }
            } else {
                FILE *f = fopen(logd, "a");
                if (f) { fputs("[12:00:00] [SHUTDOWN] cleanup complete for STOP file /data/shadowmount/STOP\n", f); fclose(f); }
            }
        }
    }
    return NULL;
}
#endif

static void d_schliessen(zugriff *z)
{
    dz *d = z->zustand;
    if (d->sm_laeuft) { d->sm_ende = true; pthread_join(d->sm, NULL); d->sm_laeuft = false; }
}

zugriff *zugriff_datei(const datei_optionen *o)
{
    zugriff *z = calloc(1, sizeof *z);
    dz *d = calloc(1, sizeof *d);
    d->o = *o;
    d->o.wurzel = strdup(o->wurzel ? o->wurzel : "");
    d->o.test_hier = o->test_hier ? strdup(o->test_hier) : NULL;
    z->art = "datei";
    z->cwd = d_cwd; z->liste = d_liste; z->lies = d_lies; z->kopf = d_kopf; z->enthaelt = d_enthaelt; z->schreibe = d_schreibe; z->befehl = d_befehl;
    z->schliessen = d_schliessen; z->zustand = d;
#ifdef KERN_LOESCHEN
    if (o->test_einspielungen) {
        const char *f = getenv("FAKESM");
        if (f && (strcmp(f, "1") == 0 || strcmp(f, "stumm") == 0)) {
            if (pthread_create(&d->sm, NULL, fake_sm, d) == 0) d->sm_laeuft = true;
        }
    }
#endif
    return z;
}
