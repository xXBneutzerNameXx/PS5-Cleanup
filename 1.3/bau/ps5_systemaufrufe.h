/* PS5 Cleanup 1.1.1 - Dateisystem-Aufrufe mit korrekter Fehlerbehandlung (Sicherheitsfix)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * Warum (Untersuchung 17.11.2026, Rückmeldungen #9 FW 13.20 und #13 FW 13.60, "550 Bad file descriptor"):
 * Das PS5-Payload-SDK (v0.45, libc.a, Teil syscalls.o) baut openat, fstatat, fstatfs, unlinkat, fchmodat, renameat, mkdirat, readlink
 * und statfs als rohe Systemaufrufe ohne Fehlerbehandlung: "mov $nr,%rax; mov %rcx,%r10; syscall; ret". Der PS5-Kern meldet einen
 * Fehler wie FreeBSD über das Carry-Flag, im Rückgabewert steht dann die POSITIVE Fehlernummer. Die SDK-Stubs werten das Flag nicht
 * aus: ein gescheitertes openat liefert z. B. 2 (ENOENT) - für den Aufrufer ein gültiger Datei-Griff (stderr) -, errno bleibt
 * unverändert. libc.a wird vor libkernel_sys.so gelinkt, darum gewinnen diese Stubs (nachgewiesen mit -Wl,-y).
 * Das SDK selbst macht es in mount.o (mount, nmount, unmount) richtig: "syscall; setae %cl; ... errno = rax; return -1" - genau
 * diese Auswertung steht hier. Unabhängig gemessen von anderen Projekten (sbihaiko/ariel #11 FW 11.20, MNeroba/SteamController-PS5
 * PR #8 FW 13.20). Nicht die Firmware: auf 12.00 scheitern diese Aufrufe beim Löschen nur fast nie.
 *
 * Diese Datei wird in JEDE Datei des PS5-Baus eingebunden (-include, nach bau/nurlesen.h) und ersetzt die Namen durch Fassungen, die
 * das Carry-Flag auswerten: Fehler -> -1 und errno, Erfolg -> Ergebnis unverändert. Dieselben Systemaufruf-Nummern wie die SDK-Stubs.
 * Am PC (Linux) ohne Wirkung - außer in den Tests mit PS5C_SDK_NACHBILDUNG: dann wird das Verhalten des SDK nachgestellt
 * (test/kern/test_sdk.c): PS5C_SDK_ALT=1 = wie bisher (positive Fehlernummer, errno unverändert), sonst wie diese Korrektur;
 * PS5C_FEHLER=<aufruf>:<name>:<errno>[,...] lässt einen Aufruf mit diesem Namen scheitern (z. B. openat:data:13).
 * Die Bau-Skripte prüfen, dass im fertigen Programm keiner der rohen SDK-Stubs mehr steckt.
 *
 * NEU 18.11.2026 (zweiter Weg für FW 13.x): Eine PS5-App darf auf 13.x nicht relativ zu einem geöffneten Ordner öffnen - openat und Co.
 * nehmen dort nur AT_FDCWD, jeder echte Ordner-Griff ergibt EINVAL (so dokumentiert im Quelltext von Playden, leolobato/playden-ps5
 * src/platform/directory_walk.h, Hauptplattform FW 13.60). Genau dann - Fehler EINVAL bei einem echten Ordner-Griff - geht derselbe
 * Aufruf über den ganzen Pfad (kern/ps5_pfadweg.c): der Ordner-Griff muss ein von uns geöffneter Ordner sein, dessen Pfad JETZT noch
 * genau ihn bezeichnet (Gerät/Knoten), geöffnet wird ohne Verknüpfung zu folgen, und das Geöffnete muss der Eintrag sein, der eben dort
 * stand. Jeder andere Fehler bleibt ein Fehler. Auf FW 12.x kommt EINVAL dort nie vor - dort ändert sich nichts.
 * Am PC: PS5C_NUR_PFAD=1 lässt jeden dieser Aufrufe mit echtem Ordner-Griff mit EINVAL scheitern (wie FW 13.x).
 */
#ifndef PS5C_SYSTEMAUFRUFE_H
#define PS5C_SYSTEMAUFRUFE_H
#if !defined(__linux__) || defined(PS5C_SDK_NACHBILDUNG)
/* alle Köpfe, die diese Namen deklarieren, VOR den Ersetzungen unten einbinden (sonst würde eine spätere Deklaration zu einer
 * zweiten, nicht-statischen Deklaration der ps5c_-Fassung) */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#ifndef NURLESEN_OHNE_UNISTD   /* app.c in den Fassungen ohne Löschen: unistd.h bewusst nicht (nurlesen.h) */
#include <unistd.h>
#endif
#ifdef __linux__
#include <sys/vfs.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#endif

/* zweiter Weg über den ganzen Pfad (kern/ps5_pfadweg.c) */
#define PS5C_PFADWEG 1
void ps5c_ordner_merken(int fd, const char *pfad);
int ps5c_pfad_openat(int fd, const char *p, int fl, int mode);
int ps5c_pfad_fstatat(int fd, const char *p, struct stat *st, int fl);
#ifndef PS5C_NURLESEN_H
int ps5c_pfad_unlinkat(int fd, const char *p, int fl);
int ps5c_pfad_fchmodat(int fd, const char *p, mode_t m, int fl);
int ps5c_pfad_renameat(int fa, const char *a, int fb, const char *b);
int ps5c_pfad_mkdirat(int fd, const char *p, mode_t m);
#endif
/* nur mit einem echten Ordner-Griff: bei EINVAL (FW 13.x) - und, sobald die Konsole EINMAL so abgelehnt hat, bei jeder Ablehnung.
 * NEU 18.11.2026 (Rückmeldung #22, FW 13.60, Lösch-Test von Max): Öffnen und Ansehen liefen über den zweiten Weg, das Entfernen einer Datei
 * (unlinkat) lehnte die 13.60 aber offenbar mit einer ANDEREN Fehlernummer ab - der zweite Weg sprang nie an. Wie Playden und ps5upload
 * (beide löschen auf 13.60 über den ganzen Pfad): hat die Konsole einmal gezeigt, dass sie relative Aufrufe ablehnt, geht jeder abgelehnte
 * Schritt über den Pfad - mit denselben Prüfungen (kern/ps5_pfadweg.c). Auf 12.x lehnt die Konsole nie so ab: dort bleibt alles wie bisher. */
extern _Atomic int ps5c_relativ_abgelehnt;
#define PS5C_ZWEITER_WEG(fd, r, weg) (((r) < 0 && (fd) >= 0 && (errno == EINVAL || ps5c_relativ_abgelehnt)) ? (ps5c_relativ_abgelehnt = 1, (weg)) : (r))

#ifndef __linux__
/* ---------------------------------------------------------------- PS5: Systemaufruf mit Carry-Flag (wie das SDK in mount.o) */
static inline long ps5c_roh(long nr, long a1, long a2, long a3, long a4, int *fehler)
{
    register long r10 __asm__("r10") = a4;
    long rax = nr;
    unsigned char cf;
    __asm__ volatile ("syscall\n\tsetc %1"
                      : "+a"(rax), "=r"(cf), "+D"(a1), "+S"(a2), "+d"(a3), "+r"(r10)
                      :
                      : "rcx", "r11", "r8", "r9", "memory", "cc");
    *fehler = cf;
    return rax;
}
static inline long ps5c_fertig(long r, int fehler) { if (fehler) { errno = (int)r; return -1; } return r; }
#define PS5C_RUF(nr, a1, a2, a3, a4) ({ int f_; long r_ = ps5c_roh((nr), (long)(a1), (long)(a2), (long)(a3), (long)(a4), &f_); ps5c_fertig(r_, f_); })
static inline int ps5c_openat(int fd, const char *p, int fl, ...)
{
    int mode = 0;
#ifndef PS5C_NURLESEN_H   /* ohne Löschen ist O_CREAT gesperrt - dort gibt es keinen dritten Wert */
    if (fl & O_CREAT) { va_list ap; va_start(ap, fl); mode = va_arg(ap, int); va_end(ap); }
#endif
    int r = (int)PS5C_RUF(499, fd, p, fl, mode);
    return PS5C_ZWEITER_WEG(fd, r, ps5c_pfad_openat(fd, p, fl, mode));
}
static inline int ps5c_fstatat(int fd, const char *p, struct stat *st, int fl) { int r = (int)PS5C_RUF(493, fd, p, st, fl); return PS5C_ZWEITER_WEG(fd, r, ps5c_pfad_fstatat(fd, p, st, fl)); }
static inline int ps5c_fstatfs(int fd, struct statfs *s) { return (int)PS5C_RUF(397, fd, s, 0, 0); }
static inline int ps5c_statfs(const char *p, struct statfs *s) { return (int)PS5C_RUF(396, p, s, 0, 0); }
static inline ssize_t ps5c_readlink(const char *p, char *b, size_t n) { return (ssize_t)PS5C_RUF(58, p, b, n, 0); }
#ifndef PS5C_NURLESEN_H   /* in den Fassungen ohne Löschen sind die verändernden Aufrufe gesperrt (nurlesen.h) - dort gar nicht erst nennen */
static inline int ps5c_unlinkat(int fd, const char *p, int fl) { int r = (int)PS5C_RUF(503, fd, p, fl, 0); return PS5C_ZWEITER_WEG(fd, r, ps5c_pfad_unlinkat(fd, p, fl)); }
static inline int ps5c_fchmodat(int fd, const char *p, mode_t m, int fl) { int r = (int)PS5C_RUF(490, fd, p, m, fl); return PS5C_ZWEITER_WEG(fd, r, ps5c_pfad_fchmodat(fd, p, m, fl)); }
static inline int ps5c_renameat(int fa, const char *a, int fb, const char *b) { int r = (int)PS5C_RUF(501, fa, a, fb, b); return PS5C_ZWEITER_WEG(fa, r, ps5c_pfad_renameat(fa, a, fb, b)); }
static inline int ps5c_mkdirat(int fd, const char *p, mode_t m) { int r = (int)PS5C_RUF(496, fd, p, m, 0); return PS5C_ZWEITER_WEG(fd, r, ps5c_pfad_mkdirat(fd, p, m)); }
#endif

#else
/* ---------------------------------------------------------------- PC-Test: Verhalten des SDK nachgestellt (nur PS5C_SDK_NACHBILDUNG) */
/* Ergebnis eines "rohen" Aufrufs wie auf der PS5: Fehler = positive Fehlernummer und Carry-Flag (fehler = 1) */
static inline int ps5c_nb_fehler(const char *ruf, const char *pfad)
{
    const char *f = getenv("PS5C_FEHLER");
    if (!f || !pfad) return 0;
    const char *name = strrchr(pfad, '/'); name = name ? name + 1 : pfad;
    size_t lr = strlen(ruf), ln = strlen(name);
    for (const char *s = f; *s; ) {
        const char *e = strchr(s, ','); size_t l = e ? (size_t)(e - s) : strlen(s);
        if (l > lr + 1 && !strncmp(s, ruf, lr) && s[lr] == ':' && l > lr + 1 + ln + 1 && !strncmp(s + lr + 1, name, ln) && s[lr + 1 + ln] == ':')
            return atoi(s + lr + 1 + ln + 1);
        s += l + (e ? 1 : 0);
    }
    return 0;
}
static inline long ps5c_fertig(long r, int fehler)
{
    if (!fehler) return r;
    const char *alt = getenv("PS5C_SDK_ALT");
    if (alt && !strcmp(alt, "1")) return r;            /* wie die SDK-Stubs bis 1.1: positive Fehlernummer, errno unverändert */
    errno = (int)r; return -1;                         /* wie diese Korrektur */
}
/* NEU 18.11.2026: PS5C_NUR_PFAD=1 - jeder Aufruf mit echtem Ordner-Griff scheitert mit EINVAL (wie FW 13.x).
 * PS5C_NUR_PFAD=2 (Rückmeldung #22): Öffnen/Ansehen mit EINVAL, die verändernden Aufrufe (unlinkat, fchmodat, renameat, mkdirat) mit EPERM. */
static inline int ps5c_nb_nur_pfad(const char *ruf, int fd)
{
    const char *e = getenv("PS5C_NUR_PFAD");
    if (!e || fd < 0) return 0;
    if (!strcmp(e, "1")) return EINVAL;
    if (!strcmp(e, "2")) return (!strcmp(ruf, "openat") || !strcmp(ruf, "fstatat")) ? EINVAL : EPERM;
    return 0;
}
#define PS5C_NBD(ruf, fd, pfad, ausdruck) ({ int e_ = ps5c_nb_nur_pfad((ruf), (fd)); if (!e_) e_ = ps5c_nb_fehler((ruf), (pfad)); long r_; int f_ = 0; \
    if (e_) { r_ = e_; f_ = 1; } else { int s_ = errno; errno = 0; r_ = (long)(ausdruck); if (r_ < 0) { r_ = errno ? errno : EIO; f_ = 1; } errno = s_; } \
    ps5c_fertig(r_, f_); })
#define PS5C_NB(ruf, pfad, ausdruck) ({ int e_ = ps5c_nb_fehler((ruf), (pfad)); long r_; int f_ = 0; \
    if (e_) { r_ = e_; f_ = 1; } else { int s_ = errno; errno = 0; r_ = (long)(ausdruck); if (r_ < 0) { r_ = errno ? errno : EIO; f_ = 1; } errno = s_; } \
    ps5c_fertig(r_, f_); })
static inline int ps5c_openat(int fd, const char *p, int fl, ...)
{
    int mode = 0;
#ifndef PS5C_NURLESEN_H
    if (fl & O_CREAT) { va_list ap; va_start(ap, fl); mode = va_arg(ap, int); va_end(ap); }
#endif
    int r = (int)PS5C_NBD("openat", fd, p, openat(fd, p, fl, mode));
    return PS5C_ZWEITER_WEG(fd, r, ps5c_pfad_openat(fd, p, fl, mode));
}
static inline int ps5c_fstatat(int fd, const char *p, struct stat *st, int fl) { int r = (int)PS5C_NBD("fstatat", fd, p, fstatat(fd, p, st, fl)); return PS5C_ZWEITER_WEG(fd, r, ps5c_pfad_fstatat(fd, p, st, fl)); }
static inline int ps5c_fstatfs(int fd, struct statfs *s) { return (int)PS5C_NB("fstatfs", NULL, fstatfs(fd, s)); }
static inline int ps5c_statfs(const char *p, struct statfs *s) { return (int)PS5C_NB("statfs", p, statfs(p, s)); }
static inline ssize_t ps5c_readlink(const char *p, char *b, size_t n) { return (ssize_t)PS5C_NB("readlink", p, readlink(p, b, n)); }
#ifndef PS5C_NURLESEN_H
static inline int ps5c_unlinkat(int fd, const char *p, int fl) { int r = (int)PS5C_NBD("unlinkat", fd, p, unlinkat(fd, p, fl)); return PS5C_ZWEITER_WEG(fd, r, ps5c_pfad_unlinkat(fd, p, fl)); }
static inline int ps5c_fchmodat(int fd, const char *p, mode_t m, int fl) { int r = (int)PS5C_NBD("fchmodat", fd, p, fchmodat(fd, p, m, fl)); return PS5C_ZWEITER_WEG(fd, r, ps5c_pfad_fchmodat(fd, p, m, fl)); }
static inline int ps5c_renameat(int fa, const char *a, int fb, const char *b) { int r = (int)PS5C_NBD("renameat", fa, a, renameat(fa, a, fb, b)); return PS5C_ZWEITER_WEG(fa, r, ps5c_pfad_renameat(fa, a, fb, b)); }
static inline int ps5c_mkdirat(int fd, const char *p, mode_t m) { int r = (int)PS5C_NBD("mkdirat", fd, p, mkdirat(fd, p, m)); return PS5C_ZWEITER_WEG(fd, r, ps5c_pfad_mkdirat(fd, p, m)); }
#endif
#endif

/* ab hier rufen alle diese Namen die geprüften Fassungen */
#define openat   ps5c_openat
#define fstatat  ps5c_fstatat
#define fstatfs  ps5c_fstatfs
#define statfs(p, s) ps5c_statfs((p), (s))   /* nur der Aufruf - "struct statfs" bleibt der Typ */
#define readlink ps5c_readlink
#ifndef PS5C_NURLESEN_H
#define unlinkat ps5c_unlinkat
#define fchmodat ps5c_fchmodat
#define renameat ps5c_renameat
#define mkdirat  ps5c_mkdirat
#endif
#endif
#endif
