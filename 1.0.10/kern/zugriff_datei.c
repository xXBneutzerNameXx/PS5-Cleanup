/* PS5 Cleanup - gemeinsamer C-Kern
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
 *
 * NEU 1.0.9 (da_Chris86 08.11.2026): private Ordner von Programmen (Playden & Co.) - siehe "Besitzer-Wechsel".
 */
#define _GNU_SOURCE
#include "zugriff.h"
#include "cJSON.h"
#include <dirent.h>
#include <stdarg.h>
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
#include <sys/statvfs.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
#endif
#endif

typedef struct pv_ort { char art; unsigned uid; int n; const char *pfad; } pv_ort;   /* NEU 1.0.9: ein privater Ort (L gelesen, W gelöscht) */
typedef struct {
    datei_optionen o;
    bool tot;                    /* DIEON: Verbindung "abgerissen" */
    pthread_t sm; bool sm_laeuft; volatile bool sm_ende;
    pthread_mutex_t pv_m;        /* NEU 1.0.9: private Orte (Besitzer-Wechsel) - alles darunter nur mit pv_m */
    arena *pv_a; pv_ort *pv; int pv_n, pv_cap, pv_mehr;
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

static bool ist_datei(const char *r) { struct stat st; return stat(r, &st) == 0 && S_ISREG(st.st_mode); }
#ifdef KERN_LOESCHEN
static bool da(const char *r) { struct stat st; return stat(r, &st) == 0; }
#endif

/* NEU 1.0.7 (Prüf-ELF auf der PS5, 31.10.2026 23:06): access() prüft mit der "echten" Benutzerkennung des Prozesses. Ein Payload
 * ohne angehobene Rechte (Prüf-ELF) bekam so "550" für /user/appmeta, obwohl er den Ordner tatsächlich öffnen kann. Darum zusätzlich
 * wirklich öffnen (nur lesen) - verhält sich in der App (Rechte angehoben, access() klappt schon) genau wie bisher. (1.0.9: ordner_pruefen) */

/* ---------------------------------------------------------------- Besitzer-Wechsel (NEU 1.0.9)
 * da_Chris86 08.11.2026 08:02: Programme wie Playden schreiben Daten, die man danach nicht mehr loswird - "Man müsste halt nur rausfinden,
 * wie diese Programme ihre Ordner anlegen". Playden (leolobato/playden-ps5): runtime-state, user/game-settings-private, art und games/<Kennung>
 * mit Rechten 0700 (Dateien 0600) - "Root can't bypass permissions on the console". Auf da_Chris86s PS5 gehörten sie Benutzer 1. Bis 1.0.8
 * galten solche Ordner als nicht lesbar: geschützt, still übersprungen, nach dem Löschen blieb der Ordner des Programms stehen.
 * 08:20 ("du kennst den Programmcode besser ... der User muss auch sehen, was passiert"), entschieden:
 *  - Scheitert ein Schritt an fehlenden Rechten (EACCES/EPERM), wird er EINMAL als Besitzer dessen wiederholt, was den Weg versperrt -
 *    so wie Playden selbst (seteuid). Gewechselt wird nur auf den eingetragenen Besitzer (lstat/fstat), nie auf root.
 *  - nur unter /user/data (= /data) und auf M.2 / erweitertem Speicher außer <Laufwerk>/user, system_data, lost+found (besitzer_erlaubt);
 *    Sony-Bereiche nie. Verknüpfungen werden nie verfolgt (jeder Teil des Weges ohne Verknüpfung geöffnet).
 *  - lesen: "ist der Ordner da" (CWD), Ordnerliste, Datei öffnen. Löschen: nur in DELE, RMD und RMDA - nie Rechte ändern, nie schreiben.
 *  - was auch als Besitzer nicht geht, bleibt wie bisher: nicht lesbar = geschützt, nie gelöscht.
 *  - klappt das Zurückwechseln nicht (zugriff_rechte_kaputt), verändert diese Schicht nichts mehr (der Kern hört auf).
 *  - jeder Ort, an dem gewechselt wurde, wird gemerkt ("SITE PRIVAT-LISTE?") - Liste, Erklärung und Protokoll zeigen ihn.
 * Der Wechsel gilt auf der PS5 für den ganzen Prozess (alle Fäden). Darum hält jede Frage an diese Schicht die Lesesperre und der Wechsel
 * die Schreibsperre: solange gewechselt ist, wartet jede andere Datei-Arbeit. Die Oberfläche nimmt dieselbe Sperre für ihre Bilder. */
bool (*zugriff_als_besitzer)(int dfd, const char *name, unsigned uid, bool (*tu)(void *c), void *c) = NULL;
volatile bool zugriff_rechte_kaputt = false;
static pthread_rwlock_t g_wechsel = PTHREAD_RWLOCK_INITIALIZER;
static _Thread_local int g_lesen = 0;                 /* Lesesperre dieses Fadens - gezählt, verschachtelt erlaubt */
void zugriff_wechsel_sperren(void) { if (g_lesen++ == 0) pthread_rwlock_rdlock(&g_wechsel); }
bool zugriff_wechsel_versuchen(void)
{
    if (g_lesen > 0) { g_lesen++; return true; }
    if (pthread_rwlock_tryrdlock(&g_wechsel) != 0) return false;
    g_lesen = 1;
    return true;
}
void zugriff_wechsel_frei(void) { if (g_lesen > 0 && --g_lesen == 0) pthread_rwlock_unlock(&g_wechsel); }
/* tu(c) als Besitzer: die eigene Lesesperre kurz abgeben, mit der Schreibsperre allein wechseln, danach wie vorher */
static bool wechseln(int dfd, const char *name, unsigned uid, bool (*tu)(void *c), void *c)
{
    if (!zugriff_als_besitzer || zugriff_rechte_kaputt) return false;
    int t = g_lesen;
    if (t) { g_lesen = 0; pthread_rwlock_unlock(&g_wechsel); }
    pthread_rwlock_wrlock(&g_wechsel);
    bool ok = !zugriff_rechte_kaputt && zugriff_als_besitzer(dfd, name, uid, tu, c);
    int e = errno;
    pthread_rwlock_unlock(&g_wechsel);
    if (t) { pthread_rwlock_rdlock(&g_wechsel); g_lesen = t; }
    errno = e;
    return ok;
}
static bool rechte_fehler(int e) { return e == EACCES || e == EPERM; }
static bool teil_ci(const char *s, size_t l, const char *w)
{
    if (strlen(w) != l) return false;
    for (size_t i = 0; i < l; i++) { char c = s[i]; if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); if (c != w[i]) return false; }
    return true;
}
/* Darf hier als Besitzer gelesen bzw. gelöscht werden? v: Pfad wie in den Befehlen (norm), mindestens eine Ebene unter /user/data bzw.
 * unter /mnt/ext0 oder /mnt/ext1 - dort nicht in user, system_data, lost+found (wie der Selbstschutz: Systemordner der Laufwerke).
 * NEU 1.0.10: und nur im Ordner eines bekannten Programms (zugriff_besitzer_bekannt). *plen = Länge des Programmordners in v (für die
 * Prüfung ohne Verknüpfungen, siehe weg_ansehen). */
bool (*zugriff_besitzer_bekannt)(const char *name) = NULL;
static bool besitzer_ort(const char *v, size_t *plen)
{
    if (!v || !zugriff_besitzer_bekannt) return false;
    const char *r;
    if (!strncmp(v, "/user/data/", 11)) r = v + 11;
    else if (!strncmp(v, "/mnt/ext0/", 10) || !strncmp(v, "/mnt/ext1/", 10)) r = v + 10;
    else return false;
    const char *e = strchr(r, '/');
    size_t l = e ? (size_t)(e - r) : strlen(r);
    if (l == 0 || l > 200) return false;
    if (r == v + 10 && (teil_ci(r, l, "user") || teil_ci(r, l, "system_data") || teil_ci(r, l, "lost+found"))) return false;
    if (teil_ci(r, l, "homebrew")) {                     /* Sammelordner: es zählt der Ordner darin */
        if (!e) return false;
        r = e + 1; e = strchr(r, '/'); l = e ? (size_t)(e - r) : strlen(r);
        if (l == 0 || l > 200) return false;
    }
    char name[208]; memcpy(name, r, l); name[l] = 0;
    if (teil_ci(r, l, "homebrew") || teil_ci(r, l, "user") || !zugriff_besitzer_bekannt(name)) return false;
    if (plen) *plen = (size_t)(r - v) + l;
    return true;
}
static bool besitzer_erlaubt(const char *v) { return besitzer_ort(v, NULL); }
/* NEU 1.0.10 (Codex-Prüfung 126, P1): als Besitzer nie über eine Verknüpfung. Vom Ordner des Programms an bis v jeden Teil mit lstat
 * ansehen (als Besitzer, darum innerhalb des Wechsels): keine Verknüpfung, unterwegs nur Ordner. Vor und nach dem Schritt - beide Male
 * dieselben Teile (Gerät, Knoten), sonst wurde inzwischen etwas getauscht und das Ergebnis gilt nicht. Ohne openat (Fassungen ohne
 * Löschen, Schreibsperre): nur lstat auf Pfade; die Gegenprobe danach schließt den Tausch zwischen den beiden Blicken aus. */
#define WEG_MAX 48
typedef struct { int n; dev_t dev[WEG_MAX]; ino_t ino[WEG_MAX]; mode_t art; } weg_kette;
static _Thread_local bool g_link_am_ende = false;   /* NEU 13.11.2026 (Codex-Prüfung 180 (4)): nur für DELE einer Verknüpfung selbst */
/* NEU 1.0.10 (Codex-Prüfung 192 (2)): der Besitzer des Ziels, wie es vorher gesehen wurde (privat_weg_x mit soll) - lässt sich das Ziel
 * selbst als root gerade nicht ansehen (PS5: ohne Grund, EBADF), nimmt blockade_finden diesen Besitzer; ob es wirklich dasselbe Ziel ist,
 * prüft der Durchgang als Besitzer mit soll. -1 = keiner. */
static _Thread_local long g_soll_uid = -1;
/* Codex-Prüfung 194: der letzte privat_weg_x (art 1) hat als Besitzer das Ziel mit soll bestätigt, nur das Entfernen scheiterte */
static _Thread_local bool g_art1_geprueft = false;
static bool weg_ansehen(dz *d, arena *a, const char *v, weg_kette *k)
{
    k->n = 0; k->art = 0;
    size_t p;
    if (!besitzer_ort(v, &p)) { errno = EPERM; return false; }
    for (;;) {
        const char *teil = a_kopie(a, v); ((char *)teil)[p] = 0;
        struct stat st;
        if (k->n >= WEG_MAX) { errno = ENAMETOOLONG; return false; }
        if (lstat(echt(d, a, teil), &st) != 0) return false;
        if (S_ISLNK(st.st_mode) && !(v[p] == 0 && g_link_am_ende)) { errno = ELOOP; return false; }
        k->dev[k->n] = st.st_dev; k->ino[k->n] = st.st_ino; k->n++; k->art = st.st_mode;
        if (v[p] == 0) return true;
        if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return false; }
        const char *e = strchr(v + p + 1, '/');
        p = e ? (size_t)(e - v) : strlen(v);
    }
}
static bool weg_gleich(const weg_kette *x, const weg_kette *y)
{
    if (x->n != y->n) return false;
    for (int i = 0; i < x->n; i++) if (x->dev[i] != y->dev[i] || x->ino[i] != y->ino[i]) return false;
    return true;
}
/* tu als Besitzer, eingerahmt von weg_ansehen (siehe oben). *kette = Blick davor (für den Vergleich mit einer geöffneten Datei) */
typedef struct { dz *d; arena *a; const char *v; bool (*tu)(void *c); void *c; weg_kette vor; } weg_auftrag;
static bool weg_tu(void *c)
{
    weg_auftrag *w = c;
    if (!weg_ansehen(w->d, w->a, w->v, &w->vor)) return false;
    bool ok = w->tu(w->c);
    int e = errno;
    weg_kette nach;
    if (!weg_ansehen(w->d, w->a, w->v, &nach) || !weg_gleich(&w->vor, &nach)) { errno = EAGAIN; return false; }   /* (ein offenes fd schließt der Aufrufer) */
    errno = e;
    return ok;
}
static bool liegt_in(const char *p, const char *q) { size_t l = strlen(q); return !strncmp(p, q, l) && (p[l] == 0 || p[l] == '/'); }
#define PV_MAX 20000
/* einen Ort merken: L (gelesen) - liegt er in einem schon gemerkten Ort desselben Besitzers, zählt er dort mit; W (gelöscht) je Ziel */
static void privat_merken(dz *d, char art, unsigned uid, const char *pfad, int n)
{
    if (!pfad || strchr(pfad, '\n')) return;
    pthread_mutex_lock(&d->pv_m);
    for (int i = 0; i < d->pv_n; i++) {
        pv_ort *o = &d->pv[i];
        if (o->art != art || o->uid != uid) continue;
        if (art == 'L' ? liegt_in(pfad, o->pfad) : !strcmp(pfad, o->pfad)) { o->n += n; pthread_mutex_unlock(&d->pv_m); return; }
    }
    if (d->pv_n >= PV_MAX) { d->pv_mehr++; pthread_mutex_unlock(&d->pv_m); return; }
    if (d->pv_n == d->pv_cap) {
        int nc = d->pv_cap ? d->pv_cap * 2 : 16;
        pv_ort *x = realloc(d->pv, sizeof *x * (size_t)nc);
        if (!x) { d->pv_mehr++; pthread_mutex_unlock(&d->pv_m); return; }
        d->pv = x; d->pv_cap = nc;
    }
    d->pv[d->pv_n].art = art; d->pv[d->pv_n].uid = uid; d->pv[d->pv_n].n = n; d->pv[d->pv_n].pfad = a_kopie(d->pv_a, pfad);
    d->pv_n++;
    pthread_mutex_unlock(&d->pv_m);
}
/* Was versperrt den Weg zu v? Von der Wurzel Ordner für Ordner (ohne Verknüpfung) bis zum ersten Teil, den diese Rechte nicht öffnen,
 * oder bis v selbst. Liefert den offenen Ordner darüber (*pfd), den Namen darin, den eingetragenen Besitzer und den Pfad des Teils.
 * false = nicht feststellbar (fehlt, Verknüpfung, anderer Fehler) - dann wird nicht gewechselt.
 * Nur open() (lesend) und fstatat: die Fassungen ohne Löschen enthalten kein openat (Schreibsperre, test/kern/lauf_ps4.sh). Jeder Ordner
 * wird ohne Verknüpfung geöffnet und muss derselbe sein, der eben angesehen wurde (sonst wurde er inzwischen getauscht - kein Wechsel). */
static bool blockade_finden(dz *d, arena *a, const char *v, int *pfd, const char **name, unsigned *uid, const char **ort)
{
    const char *w = d->o.wurzel[0] ? d->o.wurzel : "/";
    int fd = open(w, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return false;
    int vfd = -1; const char *vname = NULL, *vweg = NULL;      /* der Ordner über fd und der Name von fd darin */
    char *kopie = a_kopie(a, v), *sp = NULL;
    const char *weg = "";
    for (char *teil = strtok_r(kopie, "/", &sp); teil; ) {
        char *naechster = strtok_r(NULL, "/", &sp);
        const char *hier = VERB(a, weg, "/", teil);
        struct stat st;
        const char *tb = (!naechster && g_soll_uid > 0) ? env(d, "BLOCK_OHNE_GRUND") : NULL;   /* Test: wie auf der PS5 (nur Testfassung) */
        if ((tb && !strcmp(tb, teil)) ? (errno = 0, 1) : fstatat(fd, teil, &st, AT_SYMLINK_NOFOLLOW) != 0) {
            int fe = errno;
            if (!naechster && g_soll_uid > 0 && (fe == 0 || fe == EBADF)) {   /* Codex-Prüfung 192 (2): das Ziel selbst, ohne Grund nicht ansehbar */
                if (vfd >= 0) close(vfd);
                *pfd = fd; *name = a_kopie(a, teil); *uid = (unsigned)g_soll_uid; *ort = hier; return true;
            }
            errno = fe;
            /* fd ließ sich öffnen, aber nicht durchsuchen: dann versperrt fd selbst */
            struct stat sf;
            if (rechte_fehler(errno) && vfd >= 0 && fstat(fd, &sf) == 0) { close(fd); *pfd = vfd; *name = vname; *uid = (unsigned)sf.st_uid; *ort = vweg; return true; }
            break;
        }
        if (S_ISLNK(st.st_mode)) {                              /* nie über eine Verknüpfung */
            if (!naechster && g_link_am_ende) { if (vfd >= 0) close(vfd); *pfd = fd; *name = a_kopie(a, teil); *uid = (unsigned)st.st_uid; *ort = hier; return true; }   /* nur DELE: die Verknüpfung selbst (Codex-Prüfung 180 (4)) */
            break;
        }
        if (!naechster) { if (vfd >= 0) close(vfd); *pfd = fd; *name = a_kopie(a, teil); *uid = (unsigned)st.st_uid; *ort = hier; return true; }
        int nfd = open(echt(d, a, hier), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (nfd < 0) {
            if (!rechte_fehler(errno)) break;
            if (vfd >= 0) close(vfd);
            *pfd = fd; *name = a_kopie(a, teil); *uid = (unsigned)st.st_uid; *ort = hier; return true;
        }
        struct stat sn;
        if (fstat(nfd, &sn) != 0 || sn.st_ino != st.st_ino || sn.st_dev != st.st_dev) { close(nfd); break; }   /* inzwischen getauscht */
        if (vfd >= 0) close(vfd);
        vfd = fd; vname = a_kopie(a, teil); vweg = hier;
        fd = nfd; weg = hier; teil = naechster;
    }
    close(fd); if (vfd >= 0) close(vfd);
    return false;
}
/* Ein Lese-Schritt (tu) ist an fehlenden Rechten gescheitert: einmal als Besitzer dessen wiederholen, was den Weg zu v versperrt.
 * NEU 1.0.10: eingerahmt von weg_ansehen (nie über eine Verknüpfung); *kette (darf NULL sein) = der Weg zu v, wie er davor aussah */
static bool als_besitzer_nochmal(dz *d, arena *a, const char *v, bool (*tu)(void *c), void *c, weg_kette *kette)
{
    if (!zugriff_als_besitzer || zugriff_rechte_kaputt || !besitzer_erlaubt(v)) return false;
    int pfd; const char *name, *ort; unsigned uid;
    if (!blockade_finden(d, a, v, &pfd, &name, &uid, &ort)) return false;
    weg_auftrag w = { d, a, v, tu, c, { 0 } };
    bool ok = besitzer_erlaubt(ort) && wechseln(pfd, name, uid, weg_tu, &w);
    int e = errno; close(pfd); errno = e;
    if (ok) privat_merken(d, 'L', uid, ort, 1);
    if (kette) *kette = w.vor;
    return ok;
}
/* Datei lesend öffnen, nachdem der normale Weg an fehlenden Rechten scheiterte: als Besitzer, nur gewöhnliche Dateien, nie über eine
 * Verknüpfung (1.0.10: O_NOFOLLOW und die geöffnete Datei muss die sein, die weg_ansehen sah) */
typedef struct { const char *r; int flags; int fd; } oeffnen_auftrag;
static bool oeffnen_tu(void *c) { oeffnen_auftrag *x = c; x->fd = open(x->r, x->flags | O_NOFOLLOW); return x->fd >= 0; }
static int als_besitzer_oeffnen(dz *d, arena *a, const char *v, const char *r, int flags)
{
    oeffnen_auftrag x = { r, flags, -1 };
    weg_kette k;
    bool ok = als_besitzer_nochmal(d, a, v, oeffnen_tu, &x, &k);
    if (!ok) { if (x.fd >= 0) { int e = errno; close(x.fd); errno = e; } return -1; }
    struct stat st;
    if (fstat(x.fd, &st) != 0 || !S_ISREG(st.st_mode) || k.n < 1 || st.st_dev != k.dev[k.n - 1] || st.st_ino != k.ino[k.n - 1]) { close(x.fd); errno = EINVAL; return -1; }
    return x.fd;
}
/* open() wie bisher; scheitert es an fehlenden Rechten, einmal als Besitzer */
static int datei_oeffnen(dz *d, arena *a, const char *v, const char *r, int flags)
{
    int fd = open(r, flags);
    if (fd >= 0 || !rechte_fehler(errno)) return fd;
    int e = errno;
    fd = als_besitzer_oeffnen(d, a, v, r, flags);
    if (fd < 0) errno = e;
    return fd;
}
/* fopen(r, "rb") wie bisher (nur gewöhnliche Dateien); bei fehlenden Rechten einmal als Besitzer. NULL = *nein gesetzt */
static FILE *lesend_auf(dz *d, arena *a, const char *v, const char *r, antwort *nein)
{
    errno = 0;
    if (ist_datei(r)) {
        FILE *f = fopen(r, "rb");
        if (f) return f;
        if (!rechte_fehler(errno)) { *nein = A(a, 550, VERB(a, "550 ", strerror(errno))); return NULL; }
    } else if (!rechte_fehler(errno)) { *nein = A(a, 550, "550 no"); return NULL; }
    int fd = als_besitzer_oeffnen(d, a, v, r, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    FILE *f = fd >= 0 ? fdopen(fd, "rb") : NULL;
    if (!f) { if (fd >= 0) close(fd); *nein = A(a, 550, "550 no"); }
    return f;
}
/* "SITE PRIVAT?", "SITE PRIVAT-LISTE?", "SITE PRIVAT-NEU" (zugriff.h) */
static antwort d_privat(dz *d, arena *a, const char *befehl)
{
    if (!strcmp(befehl, "SITE PRIVAT?")) return zugriff_als_besitzer && !zugriff_rechte_kaputt ? A(a, 200, "200 OK") : A(a, 502, "502 no");
    if (!strcmp(befehl, "SITE PRIVAT-NEU")) {
        pthread_mutex_lock(&d->pv_m);
        arena_frei(d->pv_a); d->pv_a = arena_neu(); d->pv_n = 0; d->pv_mehr = 0;
        pthread_mutex_unlock(&d->pv_m);
        return A(a, 200, "200 OK");
    }
    if (!strcmp(befehl, "SITE PRIVAT-LISTE?")) {
        pthread_mutex_lock(&d->pv_m);
        size_t cap = 256, n = 0;
        char *b = malloc(cap);
        if (!b) { pthread_mutex_unlock(&d->pv_m); return A(a, 451, "451 kein Speicher"); }
        n = (size_t)snprintf(b, cap, "200 %d %d\n", d->pv_n, d->pv_mehr);
        for (int i = 0; i < d->pv_n; i++) {
            size_t brauche = strlen(d->pv[i].pfad) + 48;
            if (n + brauche > cap) {
                while (n + brauche > cap) cap *= 2;
                char *x = realloc(b, cap);
                if (!x) { free(b); pthread_mutex_unlock(&d->pv_m); return A(a, 451, "451 kein Speicher"); }
                b = x;
            }
            n += (size_t)snprintf(b + n, cap - n, "%c %u %d %s\n", d->pv[i].art, d->pv[i].uid, d->pv[i].n, d->pv[i].pfad);
        }
        pthread_mutex_unlock(&d->pv_m);
        antwort r = A(a, 200, b);
        free(b);
        return r;
    }
    return A(a, 502, "502 no");
}

/* ---------------------------------------------------------------- lesen */
/* 0 = Ordner ist da und lesbar, sonst errno (wie bisher: stat, dann access, dann wirklich öffnen) */
static int ordner_pruefen(const char *r)
{
    struct stat st;
    errno = 0;
    if (stat(r, &st) != 0) return errno ? errno : EIO;
    if (!S_ISDIR(st.st_mode)) return ENOTDIR;
    if (access(r, R_OK | X_OK) == 0) return 0;
    DIR *o = opendir(r);
    if (!o) return errno ? errno : EACCES;
    closedir(o);
    return 0;
}
typedef struct { const char *r; } cwd_auftrag;
static bool cwd_tu(void *c) { return ordner_pruefen(((cwd_auftrag *)c)->r) == 0; }
static antwort d_cwd(zugriff *z, arena *a, const char *pfad)
{
    dz *d = z->zustand;
    if (stirbt(d, VERB(a, "CWD ", pfad))) return TOT;
    const char *v = norm(a, pfad);
    if (liste_hat(env(d, "DENY"), v)) return A(a, 550, "550 Permission denied");
    const char *r = echt(d, a, v);
    int f = ordner_pruefen(r);
    if (f == 0) return A(a, 250, "250 OK");
    if (rechte_fehler(f)) { cwd_auftrag x = { r }; if (als_besitzer_nochmal(d, a, v, cwd_tu, &x, NULL)) return A(a, 250, "250 OK"); }   /* NEU 1.0.9 */
    return A(a, 550, "550 no");
}

static int vergleiche(const void *x, const void *y) { return strcmp(*(const char *const *)x, *(const char *const *)y); }

static antwort liste_lesen(dz *d, arena *a, const char *v, const char *r, kdliste *aus, int *fehler);
typedef struct { dz *d; arena *a; const char *v, *r; kdliste *aus; antwort erg; int fehler; } liste_auftrag;
static bool liste_tu(void *c) { liste_auftrag *x = c; x->erg = liste_lesen(x->d, x->a, x->v, x->r, x->aus, &x->fehler); return x->erg.code < 400; }
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
    int f = 0;
    antwort erg = liste_lesen(d, a, v, r, aus, &f);
    if (erg.code >= 400 && rechte_fehler(f)) {           /* NEU 1.0.9: privater Ordner - einmal als Besitzer */
        liste_auftrag x = { d, a, v, r, aus, erg, f };
        if (als_besitzer_nochmal(d, a, v, liste_tu, &x, NULL)) return x.erg;
        aus->e = NULL; aus->n = 0;
    }
    return erg;
}
/* die Ordnerliste selbst (bis 1.0.8 in d_liste); *fehler = errno, wenn es an einem Systemaufruf scheiterte */
static antwort liste_lesen(dz *d, arena *a, const char *v, const char *r, kdliste *aus, int *fehler)
{
    *fehler = 0;
    aus->e = NULL; aus->n = 0;
    DIR *dir = opendir(r);
    if (!dir) { *fehler = errno; return A(a, 550, VERB(a, "550 ", strerror(errno))); }
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
    if (lesefehler) { *fehler = lesefehler; return A(a, 451, VERB(a, "451 ", strerror(lesefehler))); }
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
            *fehler = errno;
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
    const char *v = norm(a, pfad), *r = echt(d, a, v);
    antwort nein;
    FILE *f = lesend_auf(d, a, v, r, &nein);             /* 1.0.9: bei fehlenden Rechten einmal als Besitzer */
    if (!f) return nein;
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
    const char *v = norm(a, pfad), *r = echt(d, a, v);
    errno = 0;
    if (!ist_datei(r) && !rechte_fehler(errno)) return A(a, 550, "550 no");
    if (ml == 0 || ml > 4096) return A(a, 501, "501 Muster");
    antwort nein;
    FILE *f = lesend_auf(d, a, v, r, &nein);             /* 1.0.9: bei fehlenden Rechten einmal als Besitzer */
    if (!f) return nein;
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
/* NEU 1.0.7 (03.11.2026): stückweise lesen mit Rückruf (Codex-Prüfung 59: abbrechbar zwischen zwei Stücken). Ohne Verknüpfung am Ende. */
static antwort d_stuecke(zugriff *z, arena *a, const char *pfad, size_t stueck, bool (*weiter)(void *c, const unsigned char *d, size_t n), void *c)
{
    dz *d = z->zustand;
    if (stirbt(d, VERB(a, "RETR ", pfad))) return TOT;
    if (stueck == 0) return A(a, 501, "501 Stück");
    const char *v = norm(a, pfad), *r = echt(d, a, v);
    int fd = datei_oeffnen(d, a, v, r, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);   /* Codex-Prüfung 73: eine FIFO blockiert sonst schon beim Öffnen */
    if (fd < 0) return A(a, 550, VERB(a, "550 ", strerror(errno)));
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) { close(fd); return A(a, 550, "550 no"); }
    unsigned char *b = malloc(stueck);
    if (!b) { close(fd); return A(a, 451, "451 kein Speicher"); }
    ssize_t k; bool los = true;
    while (los && (k = read(fd, b, stueck)) != 0) {
        if (k < 0) { if (errno == EINTR) continue; int f = errno; free(b); close(fd); return A(a, 451, VERB(a, "451 ", strerror(f))); }
        los = weiter(c, b, (size_t)k);
    }
    free(b); close(fd);
    if (los) weiter(c, NULL, 0);
    return A(a, 226, "226 done");
}
/* NEU 1.0.8 (Arbeitsplan 2.1): n Bytes ab Stelle ab lesen (Spielname im exFAT-Abbild). Nur lesen, Verknüpfung am Ende nicht verfolgen. */
static antwort d_lies_ab(zugriff *z, arena *a, const char *pfad, long long ab, size_t n, unsigned char *ziel, size_t *len)
{
    dz *d = z->zustand;
    *len = 0;
    if (stirbt(d, VERB(a, "RETR ", pfad))) return TOT;
    if (ab < 0) return A(a, 501, "501 Stelle");
    const char *v = norm(a, pfad), *r = echt(d, a, v);
    int fd = datei_oeffnen(d, a, v, r, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);   /* Codex-Prüfung 73: eine FIFO blockiert sonst schon beim Öffnen */
    if (fd < 0) return A(a, 550, VERB(a, "550 ", strerror(errno)));
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) { close(fd); return A(a, 550, "550 no"); }
    if (lseek(fd, (off_t)ab, SEEK_SET) == (off_t)-1) { int f = errno; close(fd); return A(a, 451, VERB(a, "451 ", strerror(f))); }
    size_t got = 0; ssize_t k = 0;
    while (got < n) {
        k = read(fd, ziel + got, n - got);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) break;
        got += (size_t)k;
    }
    int fehler = k < 0 ? errno : 0;
    close(fd);
    if (fehler) return A(a, 451, VERB(a, "451 ", strerror(fehler)));
    *len = got;
    return A(a, 226, "226 done");
}
static antwort d_kopf(zugriff *z, arena *a, const char *pfad, size_t n, const unsigned char **daten, size_t *len)
{
    dz *d = z->zustand;
    *daten = NULL; *len = 0;
    if (stirbt(d, VERB(a, "KOPF ", pfad))) return TOT;
    const char *v = norm(a, pfad), *r = echt(d, a, v);
    int fd = datei_oeffnen(d, a, v, r, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);   /* Codex-Prüfung 73: eine FIFO blockiert sonst schon beim Öffnen */
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
    /* /system_ex NEU 1.0.7: nur für leere Einhängeordner von ShadowMount (kern_schutz.c sm_leerer_ordner) - eingehängte lehnt einbindung() ab */
    static const char *w[] = { "/mnt", "/user", "/system_data", "/mnt/ext0", "/mnt/ext1", "/system_ex", NULL };   /* /mnt: reiner Sammelordner */
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
/* NEU 1.0.9: Besitzer-Wechsel beim Löschen (siehe oben "Besitzer-Wechsel"). Nur während DELE, RMD und RMDA mit einem Ziel im erlaubten
 * Bereich (d_befehl setzt g_besitzer_frei) - nie bei SITE CHMOD, STOR oder den Test-Nachbildungen. Jeder Schritt (Ordner öffnen, Eintrag
 * ansehen, Eintrag entfernen) erst wie bisher; scheitert er an fehlenden Rechten, einmal als Besitzer des Ordners fd, dann als Besitzer
 * des Eintrags (z. B. Datei in einem Ordner mit Sticky-Bit). Jeder Wechsel zählt beim Ziel des Befehls (SITE PRIVAT-LISTE?, W). */
static _Thread_local bool g_besitzer_frei = false;
static _Thread_local dz *g_dz = NULL;
static _Thread_local const char *g_ziel = NULL;
enum { B_OEFFNEN, B_STAT, B_WEG };
typedef struct { int art, fd; const char *name; int flags; struct stat *st; int erg, fehler; } bauftrag;
static int b_einmal(bauftrag *b)
{
    int r;
    if (b->art == B_OEFFNEN) r = openat(b->fd, b->name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);   /* wie kind_ordner */
    else if (b->art == B_STAT) r = fstatat(b->fd, b->name, b->st, AT_SYMLINK_NOFOLLOW);
    else r = unlinkat(b->fd, b->name, b->flags);
    b->erg = r; b->fehler = r < 0 ? errno : 0;
    return r;
}
/* NEU 1.0.10 (PS5-Test 13.11.2026 07:44, Diagnose-Fassung): der Wechsel je Einzelschritt öffnet auf der PS5 zwar den privaten Ordner,
 * der so geöffnete Ordner ist danach aber unbrauchbar ("Ziel geöffnet, Angaben nicht lesbar: Bad file descriptor") - wie schon am
 * 12.11.2026 12:46 und 19:49. Darum gibt es ihn nicht mehr: ein Schritt, der an fehlenden Rechten scheitert, meldet das einfach, und
 * DELE/RMD/RMDA gehen dann in EINEM Durchgang als Besitzer (privat_weg), wo jede Datei innerhalb des Wechsels geöffnet UND benutzt wird
 * (bis jetzt nur im Test erzwungen: PRIVAT_NUR_DURCHGANG). */
static int bschritt(int art, int fd, const char *name, int flags, struct stat *st)
{
    if (zugriff_rechte_kaputt) { errno = EPERM; return -1; }   /* Zurückwechseln ging nicht: nichts mehr anfassen */
    bauftrag b = { art, fd, name, flags, st, -1, 0 };
    if (b_einmal(&b) >= 0) return b.erg;
    errno = b.fehler;
    return -1;
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
        size_t lw = strlen(weg), lt = strlen(teil);
        if (lw + lt + 2 > sizeof weg) { close(fd); errno = ENAMETOOLONG; return -1; }
        weg[lw] = '/'; memcpy(weg + lw + 1, teil, lt + 1);
        /* NEU 1.0.9: ein Teil des Weges im erlaubten Bereich darf als Besitzer geöffnet werden (privater Ordner) */
        int nfd = (g_besitzer_frei && besitzer_erlaubt(weg)) ? bschritt(B_OEFFNEN, fd, teil, 0, NULL) : openat(fd, teil, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (nfd < 0) { int e = errno; close(fd); errno = e; return -1; }
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
/* (kind_ordner seit 1.0.9 in bschritt, B_OEFFNEN - dieselben Kennzeichen) */
/* Namen eines geöffneten Ordners (ohne . und ..); NULL bei Lesefehler */
/* NEU 1.0.10 (PS5-Test 13.11.2026 07:18: RMDA steam-223850.kept scheiterte mit "Resource temporarily unavailable", ohne dass zu sehen
 * war, an welchem Schritt): beim Löschen wird der ERSTE gescheiterte Schritt mit Name und Grund gemerkt und an die Antwort 550 gehängt
 * ("[Schritt: ...]"). Nur Text - am Ablauf ändert sich nichts. */
static _Thread_local char g_schritt[400];
static void schritt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void schritt(const char *fmt, ...)
{
    if (g_schritt[0]) return;   /* der erste Fehler zählt (der tiefste - die äußeren Ebenen melden danach nur noch weiter) */
    int e = errno;
    va_list ap; va_start(ap, fmt); vsnprintf(g_schritt, sizeof g_schritt, fmt, ap); va_end(ap);
    errno = e;
}
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
#define SCHRITT_FEHLER(was, name) schritt("%s %s: %s", (was), (name), strerror(errno))
/* NEU 1.0.10 (PS5-Test 13.11.2026 13:36, RMDA /user/data/playden: nur "550 Input/output error" ohne Schritt): einen späteren Schritt an den
 * ersten anhängen ("...; dann: ...") - so steht im Protokoll auch, woran der letzte Versuch scheiterte. Nur Text. */
static void schritt_dazu(const char *was, const char *name)
{
    int e = errno;
    size_t n = strlen(g_schritt);
    if (n == 0) snprintf(g_schritt, sizeof g_schritt, "%s %s: %s", was, name ? name : "-", strerror(e ? e : EIO));
    else if (n + 8 < sizeof g_schritt) snprintf(g_schritt + n, sizeof g_schritt - n, "; dann: %s %s: %s", was, name ? name : "-", strerror(e ? e : EIO));
    errno = e;
}
/* NEU 1.0.10 (PS5-Test 13.11.2026 07:44): ein privater Teil IM zu löschenden Baum (z. B. Playdens games/<Spiel>.kept im Ordner von
 * Playden, der selbst root gehört) - dieser Teil wird als Ganzes in EINEM Durchgang als sein Besitzer geprüft (art 3) bzw. gelöscht
 * (art 2: Ordner samt Inhalt, 1: leerer Ordner, 0: Datei) - privat_weg, über den Pfad, ohne Verknüpfung, nur im Ordner eines bekannten
 * Programms. Nur, wenn der Schritt an fehlenden Rechten scheiterte und der Pfad bekannt ist (pfad != NULL: Befehl mit Besitzer-Wechsel,
 * nicht schon innerhalb eines Durchgangs). 0 = erledigt. */
static int privat_weg_x(dz *d, arena *a, const char *v, int art, const struct stat *soll, int eltern_fd);
/* soll: das Kind, wie es im Ordner fd beim Ansehen war; fd: der Ordner darüber (Codex-Prüfung 180 (1)) */
static int als_besitzer_teil(arena *a, const char *pfad, const char *name, int art, int e, const struct stat *soll, int fd)
{
    if (!pfad || !g_besitzer_frei || !g_dz || !rechte_fehler(e)) { errno = e; return -1; }
    const char *v = VERB(a, pfad, "/", name);
    const char *vorher = g_schritt[0] ? a_kopie(a, g_schritt) : NULL;
    g_schritt[0] = 0;
    int r = privat_weg_x(g_dz, a, v, art, soll, fd);
    int e2 = errno;
    if (r != 0 && !g_schritt[0] && vorher) snprintf(g_schritt, sizeof g_schritt, "%s", vorher);
    if (r == 0 && vorher) snprintf(g_schritt, sizeof g_schritt, "%s", "");   /* erledigt: der frühere Hinweis gilt nicht mehr */
    errno = r == 0 ? 0 : e2;
    return r;
}
/* Ordner name in fd als seinen Besitzer leeren, dann ohne Wechsel entfernen (der Ordner darüber gehört meist root); geht das nicht an
 * Rechten, als Besitzer des Ordners darüber (privat_weg_x art 1). Codex-Prüfung 180 (2). */
static int kind_ansehen(int fd, const char *name, struct stat *st);
static int ansehen_fehler(int e);
static bool fremd_privat(const struct stat *st, const char *name);
/* Codex-Prüfung 193 (1): den geleerten privaten Ordner name in fd entfernen, wenn root ihn nicht ansehen kann bzw. andere Angaben sieht.
 * Erst als Besitzer des Ordners darüber (art 1: prüft dort die gesehene Identität st; wurde getauscht, Stopp). Scheitert das nur an Rechten
 * (der Ordner darüber gehört root, der Besitzer des Kinds darf darin nichts entfernen; ein Wechsel auf root geht auf der PS5 nicht),
 * entfernt root den Ordner ohne Wechsel - das geht nur, solange er LEER ist. */
static int leer_weg_als_besitzer(arena *a, const char *pfad, const char *name, int e, const struct stat *st, int fd)
{
    g_art1_geprueft = false;
    if (als_besitzer_teil(a, pfad, name, 1, e, st, fd) == 0) return 0;
    int e1 = errno; bool gp = g_art1_geprueft; g_art1_geprueft = false;
    /* Codex-Prüfung 194: root nur, wenn der Besitzer eben genau diesen Ordner (st) bestätigt hat und nur das Entfernen an Rechten scheiterte -
     * nie, wenn schon der Wechsel abgelehnt wurde (z. B. ein eingetauschter Ordner eines anderen Besitzers) oder getauscht war (EAGAIN) */
    if (!rechte_fehler(e1) || !gp) { errno = e1; return -1; }
    if (bschritt(B_WEG, fd, name, AT_REMOVEDIR, NULL) == 0) { g_schritt[0] = 0; return 0; }
    int e2 = errno;
    SCHRITT_FEHLER("Löschen: geleerten Ordner ohne Wechsel entfernen", name);
    errno = e2 ? e2 : e1;
    return -1;
}
static int ordner_als_besitzer_weg(arena *a, const char *pfad, const char *name, int e, const struct stat *st, int fd)
{
    if (als_besitzer_teil(a, pfad, name, 4, e, st, fd) != 0) return -1;
    struct stat z;
    errno = 0;
    const char *tl = (g_besitzer_frei && g_dz) ? env(g_dz, "LEER_OHNE_GRUND") : NULL;   /* Test: wie auf der PS5 12:58 (nur Testfassung) */
    if ((tl && !strcmp(tl, name)) ? (errno = 0, 1) : kind_ansehen(fd, name, &z) != 0) {
        /* NEU 1.0.10 (PS5-Test 13.11.2026 12:58, /data/playden/user/game-settings-private): nach dem Leeren als Besitzer ließ sich der
         * private Ordner mit vollen Rechten nicht mehr ansehen bzw. zeigte andere Angaben - bisher Sicherheitsstopp. Jetzt entfernt ihn
         * der Besitzer des Ordners darüber (art 1: nur ein LEERER Ordner, mit der gesehenen Identität st geprüft - wurde getauscht, stoppt er). */
        int fe = errno; errno = fe; SCHRITT_FEHLER("Löschen: geleerten Ordner ansehen", name);
        return leer_weg_als_besitzer(a, pfad, name, ansehen_fehler(fe), st, fd);
    }
    if (!S_ISDIR(z.st_mode) || z.st_dev != st->st_dev || z.st_ino != st->st_ino) {
        if (S_ISDIR(z.st_mode) && fremd_privat(st, name)) {   /* privat: Angaben weichen ab - als Besitzer mit st prüfen und entfernen */
            schritt("Löschen: geleerter Ordner %s zeigt andere Angaben (privat)", name);
            return leer_weg_als_besitzer(a, pfad, name, EACCES, st, fd);
        }
        errno = EAGAIN;
        schritt("Löschen: geleerter Ordner %s anders als gesehen", name); return -1; }
    if (bschritt(B_WEG, fd, name, AT_REMOVEDIR, NULL) == 0) return 0;
    int e2 = errno;
    SCHRITT_FEHLER("Löschen: geleerten Ordner entfernen", name);
    return als_besitzer_teil(a, pfad, name, 1, e2, st, fd);
}
/* NEU 1.0.10 (PS5-Test 13.11.2026 08:19, RMDA steam-223850.kept): auch OHNE Wechsel lässt sich der private Ordner auf der PS5 öffnen,
 * das Geöffnete ist aber unbrauchbar (fstat: "Bad file descriptor") - der Wechsel war also nicht die (einzige) Ursache. Ein so
 * unbrauchbar geöffneter Ordner gilt jetzt als "fehlende Rechte": er geht dann wie jeder private Teil in EINEM Durchgang als sein
 * Besitzer (privat_weg - mit allen Prüfungen dort). Vorher: EAGAIN, Sicherheitsstopp ohne zweiten Versuch.
 * Das Ziel selbst (baum_weg_at) geht dabei gleich mit seiner gesehenen Identität und dem Ordner darüber (Codex-Prüfung 184).
 * Test: OFFEN_UNBRAUCHBAR=<Name> stellt das am PC nach (nur Testfassung, nur außerhalb des Durchgangs). */
static int offen_ansehen(int fd, const char *name, struct stat *st)
{
    const char *t = (g_besitzer_frei && g_dz) ? env(g_dz, "OFFEN_UNBRAUCHBAR") : NULL;
    if (t && name && !strcmp(t, name)) { errno = EBADF; return -1; }
    int r = fstat(fd, st);
    const char *t2 = (g_besitzer_frei && g_dz) ? env(g_dz, "OFFEN_ANDERS") : NULL;   /* Test: geöffnet mit anderen Angaben (PS5 12:25) */
    if (r == 0 && t2 && name && !strcmp(t2, name)) st->st_ino += 1000003;
    return r;
}
/* Fehler von offen_ansehen -> errno für den Aufrufer: unbrauchbar oder ohne Rechte = EACCES (dann als Besitzer), sonst EAGAIN wie bisher */
static int unbrauchbar(int e) { return (e == EBADF || rechte_fehler(e)) ? EACCES : EAGAIN; }
/* NEU 1.0.10 (PS5-Test 13.11.2026 12:25, Playden ganz löschen): auch die zweite Form desselben PS5-Verhaltens - ein privater Ordner (gehört
 * einem anderen Benutzer, z. B. Playden) lässt sich mit vollen Rechten öffnen, das Geöffnete zeigt aber ANDERE Angaben (Gerät/Knoten) als
 * eben angesehen ("Prüfen: game-settings-private inzwischen getauscht", der ganze Rest von /data/playden blieb). Bei einem solchen Ordner
 * geht es wie bei "unbrauchbar" als sein Besitzer weiter - der prüft dort mit dem eben angesehenen Ziel (soll), ob es wirklich derselbe
 * Ordner ist; wurde wirklich getauscht, stoppt er. Ordner, die uns selbst gehören: Stopp wie bisher. */
/* NEU 1.0.10 (PS5-Test 13.11.2026 12:50, Rest von /data/playden): dritte Form - im Ordner eines Programms ließ sich eine private Datei
 * (user/steam-device-id) mit vollen Rechten nicht einmal ansehen, die PS5 nannte keinen Grund (fstatat -1, errno 0). Ohne Grund, EBADF oder
 * fehlende Rechte gelten als "fehlende Rechte": der Ordner darüber wird dann als sein Besitzer geprüft bzw. gelöscht (die Aufrufer weiter
 * oben, mit seiner gesehenen Identität). Test: STAT_OHNE_GRUND=<Name> (nur Testfassung, nur außerhalb des Durchgangs). */
static int kind_ansehen(int fd, const char *name, struct stat *st)
{
    const char *t = (g_besitzer_frei && g_dz) ? env(g_dz, "STAT_OHNE_GRUND") : NULL;
    if (t && name && !strcmp(t, name)) { errno = 0; return -1; }
    errno = 0;
    return bschritt(B_STAT, fd, name, 0, st);
}
static int ansehen_fehler(int e) { return (e == 0 || e == EBADF || rechte_fehler(e)) ? EACCES : e; }
static bool fremd_privat(const struct stat *st, const char *name)
{
    if (!g_besitzer_frei || !S_ISDIR(st->st_mode)) return false;
    const char *t = g_dz ? env(g_dz, "OFFEN_ANDERS") : NULL;   /* Test: am PC gehört alles uns - so nachgestellt (nur Testfassung) */
    return st->st_uid != geteuid() || (t && name && !strcmp(t, name));
}
/* NEU 1.0.10 (PS5-Test 13.11.2026 23:35, RMDA /user/data/playden mit games/steam-205930, Rechte 0700, Besitzer 1): root kam in den privaten
 * Ordner scheinbar hinein (öffnen und Angaben stimmten), die Namensliste war dann aber die eines anderen Ordners ("ansehen pldmgr: No
 * error") - die vierte Form nach "nicht zu öffnen", "geöffnet, aber unbrauchbar" und "geöffnet mit anderen Angaben". Darum EINE Regel für
 * jede Tiefe und jedes Programm: ein Ordner, der einem anderen Benutzer gehört und in den Fremde nicht hineindürfen, wird nie als root
 * betreten - Prüfen und Leeren gehen gleich als sein Besitzer (dieselben Wege und Prüfungen wie bisher nach einem gescheiterten Versuch).
 * Scheitert das, bleibt der Ordner stehen (kein Versuch als root). Test: PRIVAT_DIREKT=<Name> (nur Testfassung). */
static bool privat_direkt(const struct stat *st, const char *name, const char *pfad)
{
    if (!g_besitzer_frei || !g_dz || !pfad || !S_ISDIR(st->st_mode)) return false;
    const char *t = env(g_dz, "PRIVAT_DIREKT");
    if (t && name && !strcmp(t, name)) return true;
    if (st->st_uid == 0 || st->st_uid == geteuid()) return false;
    return (st->st_mode & 0005) != 0005;
}
/* ganzen Baum prüfen, bevor etwas gelöscht wird: keine Einbindung, alles lesbar. pfad: Pfad von fd wie in den Befehlen (NULL = unbekannt).
 * Ein Teil, der an fehlenden Rechten scheitert (nicht zu öffnen, nicht zu lesen, Einträge nicht anzusehen), wird als sein Besitzer
 * geprüft (als_besitzer_teil art 3) - mit dem Kind und dem Ordner darüber, wie sie hier gesehen wurden. */
static int baum_pruefen(int fd, arena *a, int tiefe, const char *pfad)
{
    if (tiefe > 256) { errno = ELOOP; schritt("mehr als 256 Ordner ineinander (Tiefe %d)", tiefe); return -1; }   /* Codex-Prüfung 197 (3) */
    sliste *l = namen_von(fd, a);
    if (!l) { SCHRITT_FEHLER("Prüfen: Ordner lesen in Tiefe", a_fmt(a, "%d", tiefe)); return -1; }
    for (int i = 0; i < l->n; i++) {
        struct stat st;
        {   /* Codex-Prüfung 202 (2), 203 (4): Namen mit Steuerzeichen sieht der Kern nicht (sitzung.c zerlege: "nie anfassen") - dann
             * darf auch die abgesicherte Löschung sie nicht mitnehmen: der ganze Ordner bleibt, mit Schritt-Angabe */
            bool sz = false;
            for (const unsigned char *q = (const unsigned char *)l->e[i]; *q; q++) if (*q < 0x20 || *q == 0x7F) sz = true;
            if (sz) { errno = EINVAL; schritt("Prüfen: Name mit Steuerzeichen in Tiefe %d - nichts gelöscht", tiefe); return -1; }
        }
        if (kind_ansehen(fd, l->e[i], &st) != 0) {   /* 1.0.9: bschritt; Rechte: der Aufrufer prüft diesen Ordner als Besitzer */
            int e = errno; if (e == ENOENT) continue;
            errno = e; schritt("Prüfen: ansehen %s: %s (Tiefe %d, %d Namen, in %s)", l->e[i], strerror(e), tiefe, l->n, pfad ? pfad : "privatem Ordner"); errno = ansehen_fehler(e); return -1; }   /* PS5 12:50: auch ohne Grund */
        if (!S_ISDIR(st.st_mode)) continue;
        if (privat_direkt(&st, l->e[i], pfad)) {   /* fremder privater Ordner: gleich als sein Besitzer prüfen */
            if (als_besitzer_teil(a, pfad, l->e[i], 3, EACCES, &st, fd) == 0) continue;
            schritt("Prüfen: privater Ordner %s als Besitzer %u: %s", l->e[i], (unsigned)st.st_uid, errno ? strerror(errno) : "ohne Grund");
            return -1;
        }
        int c = bschritt(B_OEFFNEN, fd, l->e[i], 0, NULL);
        if (c < 0) {
            int e = errno;
            SCHRITT_FEHLER("Prüfen: öffnen", l->e[i]);
            if (als_besitzer_teil(a, pfad, l->e[i], 3, e, &st, fd) == 0) continue;   /* privater Teil: als sein Besitzer geprüft */
            return -1;
        }
        struct stat so;
        if (offen_ansehen(c, l->e[i], &so) != 0) {   /* PS5 08:19: geöffnet, aber unbrauchbar - als sein Besitzer prüfen */
            int e = errno; close(c); errno = e;
            SCHRITT_FEHLER("Prüfen: geöffnet, Angaben nicht lesbar", l->e[i]);
            if (als_besitzer_teil(a, pfad, l->e[i], 3, unbrauchbar(e), &st, fd) == 0) continue;
            return -1;
        }
        if (so.st_dev != st.st_dev || so.st_ino != st.st_ino) {
            close(c);
            if (fremd_privat(&st, l->e[i])) {   /* PS5 12:25: privater Ordner, geöffnet mit anderen Angaben - als sein Besitzer prüfen (mit st) */
                schritt("Prüfen: %s geöffnet, aber anders als angesehen (privat)", l->e[i]);
                if (als_besitzer_teil(a, pfad, l->e[i], 3, EACCES, &st, fd) == 0) continue;
                return -1;
            }
            errno = EAGAIN; schritt("Prüfen: %s inzwischen getauscht", l->e[i]); return -1; }
        int e = einbindung(fd, c);
        if (e != 0) { close(c); if (e > 0) errno = EXDEV; SCHRITT_FEHLER("Prüfen: Einbindung", l->e[i]); return -1; }
        int r = baum_pruefen(c, a, tiefe + 1, pfad ? VERB(a, pfad, "/", l->e[i]) : NULL);
        int s2 = errno; close(c); errno = s2;
        if (r) {
            if (als_besitzer_teil(a, pfad, l->e[i], 3, s2, &st, fd) == 0) continue;   /* darin an Rechten gescheitert: als Besitzer prüfen */
            return -1;
        }
    }
    return 0;
}
static _Thread_local int g_weg_n = 0;   /* Codex-Prüfung 182: wie viel baum_loeschen wirklich entfernt hat (für "als Besitzer gelöscht") */
static int baum_loeschen(int fd, arena *a, int tiefe, const char *pfad)
{
    if (tiefe > 256) { errno = ELOOP; schritt("mehr als 256 Ordner ineinander (Tiefe %d)", tiefe); return -1; }   /* Codex-Prüfung 197 (3) */
    sliste *l = namen_von(fd, a);
    if (!l) { SCHRITT_FEHLER("Löschen: Ordner lesen in Tiefe", a_fmt(a, "%d", tiefe)); return -1; }
    for (int i = 0; i < l->n; i++) {
        struct stat st;
        if (kind_ansehen(fd, l->e[i], &st) != 0) {   /* 1.0.9: bschritt */
            int e = errno; if (e == ENOENT) continue;
            errno = e; schritt("Löschen: ansehen %s: %s (Tiefe %d, %d Namen, in %s)", l->e[i], strerror(e), tiefe, l->n, pfad ? pfad : "privatem Ordner"); errno = ansehen_fehler(e); return -1; }   /* PS5 12:50: auch ohne Grund */
        if (S_ISDIR(st.st_mode)) {
            if (privat_direkt(&st, l->e[i], pfad)) {   /* fremder privater Ordner: gleich als sein Besitzer leeren, dann entfernen */
                if (ordner_als_besitzer_weg(a, pfad, l->e[i], EACCES, &st, fd) == 0) continue;
                schritt("Löschen: privater Ordner %s als Besitzer %u: %s", l->e[i], (unsigned)st.st_uid, errno ? strerror(errno) : "ohne Grund");
                return -1;
            }
            int c = bschritt(B_OEFFNEN, fd, l->e[i], 0, NULL);
            if (c < 0) {
                int e = errno;
                SCHRITT_FEHLER("Löschen: öffnen", l->e[i]);
                if (ordner_als_besitzer_weg(a, pfad, l->e[i], e, &st, fd) == 0) continue;   /* privater Teil: als sein Besitzer geleert, dann entfernt */
                return -1;
            }
            struct stat so;
            if (offen_ansehen(c, l->e[i], &so) != 0) {   /* PS5 08:19: geöffnet, aber unbrauchbar - als sein Besitzer leeren, dann entfernen */
                int e = errno; close(c); errno = e;
                SCHRITT_FEHLER("Löschen: geöffnet, Angaben nicht lesbar", l->e[i]);
                if (ordner_als_besitzer_weg(a, pfad, l->e[i], unbrauchbar(e), &st, fd) == 0) continue;
                return -1;
            }
            if (so.st_dev != st.st_dev || so.st_ino != st.st_ino) {
                close(c);
                if (fremd_privat(&st, l->e[i])) {   /* PS5 12:25: wie beim Prüfen - als sein Besitzer leeren, dann entfernen (mit st) */
                    schritt("Löschen: %s geöffnet, aber anders als angesehen (privat)", l->e[i]);
                    if (ordner_als_besitzer_weg(a, pfad, l->e[i], EACCES, &st, fd) == 0) continue;
                    return -1;
                }
                errno = EAGAIN; schritt("Löschen: %s inzwischen getauscht", l->e[i]); return -1; }
            int e = einbindung(fd, c);                 /* nochmal: inzwischen eingebunden? */
            if (e != 0) { close(c); if (e > 0) errno = EXDEV; SCHRITT_FEHLER("Löschen: Einbindung", l->e[i]); return -1; }
            int r = baum_loeschen(c, a, tiefe + 1, pfad ? VERB(a, pfad, "/", l->e[i]) : NULL);
            int s2 = errno; close(c); errno = s2;
            if (r) {
                if (ordner_als_besitzer_weg(a, pfad, l->e[i], s2, &st, fd) == 0) continue;   /* darin an Rechten gescheitert: als Besitzer weiter */
                return -1;
            }
            if (bschritt(B_WEG, fd, l->e[i], AT_REMOVEDIR, NULL) != 0) {
                int e2 = errno;
                SCHRITT_FEHLER("Löschen: Ordner entfernen", l->e[i]);
                if (als_besitzer_teil(a, pfad, l->e[i], 1, e2, &st, fd) == 0) continue;
                return -1;
            }
            g_weg_n++;
        }
        else if (bschritt(B_WEG, fd, l->e[i], 0, NULL) != 0) {   /* Datei oder Verknüpfung selbst */
            int e2 = errno;
            SCHRITT_FEHLER("Löschen: Datei entfernen", l->e[i]);
            if (als_besitzer_teil(a, pfad, l->e[i], 0, e2, &st, fd) == 0) continue;
            return -1;
        }
        else g_weg_n++;
    }
    return 0;
}
/* Ordner name in pfd samt Inhalt entfernen (wie shutil.rmtree: auf eine Verknüpfung = Fehler) */
static int baum_weg_at(int pfd, const char *name, const char *pfad)   /* pfad: Pfad von name wie im Befehl (NULL = unbekannt) */
{
    struct stat st;
    if (bschritt(B_STAT, pfd, name, 0, &st) != 0) { SCHRITT_FEHLER("Ziel ansehen", name); return -1; }   /* 1.0.9: bschritt */
    if (S_ISLNK(st.st_mode)) { errno = ELOOP; SCHRITT_FEHLER("Ziel ist eine Verknüpfung", name); return -1; }
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; SCHRITT_FEHLER("Ziel ist kein Ordner", name); return -1; }
    /* Codex-Prüfung 217: auch das Ziel selbst - ein fremder privater Ordner wird nie als root geöffnet (privat_direkt), gleich als Besitzer */
    const bool direkt = privat_direkt(&st, name, pfad);
    int fd = direkt ? -1 : bschritt(B_OEFFNEN, pfd, name, 0, NULL);
    if (!direkt && fd < 0) { SCHRITT_FEHLER("Ziel öffnen", name); return -1; }
    struct stat st2;
    int u = direkt ? EACCES : 0;
    if (direkt) schritt("Ziel %s ist ein fremder privater Ordner (Besitzer %u)", name, (unsigned)st.st_uid);
    else if (offen_ansehen(fd, name, &st2) != 0) {   /* PS5 08:19: unbrauchbar = fehlende Rechte -> gleich hier als Besitzer (privat_weg_x) */
        int e = errno; SCHRITT_FEHLER("Ziel geöffnet, Angaben nicht lesbar", name); close(fd);
        u = unbrauchbar(e);
    }
    else if ((st2.st_ino != st.st_ino || st2.st_dev != st.st_dev) && fremd_privat(&st, name)) {   /* PS5 12:25: privat, geöffnet mit anderen Angaben */
        schritt("Ziel geöffnet, aber anders als angesehen (privat): Gerät %lu/%lu, Knoten %lu/%lu", (unsigned long)st.st_dev, (unsigned long)st2.st_dev, (unsigned long)st.st_ino, (unsigned long)st2.st_ino);
        close(fd); u = EACCES;
    }
    if (u != 0) {
        if (u != EACCES || !pfad || !g_besitzer_frei || !g_dz) { errno = u == EACCES ? EAGAIN : u; return -1; }   /* ohne Pfad/Freigabe: Stopp wie bisher */
        /* Codex-Prüfung 184 (P1): mit dem Ziel (st) und dem Ordner darüber (pfd), wie eben gesehen - nicht über weg_antwort/privat_weg, das
         * beides nicht kennt (dort könnte inzwischen ein anderer Ordner unter dem Namen liegen). Scheitert es, nie nochmal ohne sie. */
        char zuerst[200]; snprintf(zuerst, sizeof zuerst, "%s", g_schritt); g_schritt[0] = 0;
        arena *pa = arena_neu();
        int r = privat_weg_x(g_dz, pa, pfad, 2, &st, pfd);
        int e2 = errno;
        arena_frei(pa);
        if (r != 0) {
            char dann[180]; snprintf(dann, sizeof dann, "%s", g_schritt[0] ? g_schritt : strerror(e2 ? e2 : EIO));
            snprintf(g_schritt, sizeof g_schritt, "%.190s; dann als Besitzer: %.180s", zuerst, dann);
            errno = (e2 == 0 || rechte_fehler(e2)) ? EAGAIN : e2;   /* kein zweiter Versuch in weg_antwort */
            return -1;
        }
        g_schritt[0] = 0;
        return 0;
    }
    if (st2.st_ino != st.st_ino || st2.st_dev != st.st_dev) {   /* inzwischen getauscht */
        schritt("Ziel geöffnet, aber anders als angesehen: Gerät %lu/%lu, Knoten %lu/%lu", (unsigned long)st.st_dev, (unsigned long)st2.st_dev, (unsigned long)st.st_ino, (unsigned long)st2.st_ino);
        close(fd); errno = EAGAIN; return -1; }
    int e = einbindung(pfd, fd);
    if (e != 0) { close(fd); if (e > 0) errno = EXDEV; SCHRITT_FEHLER("Ziel Einbindung", name); return -1; }
    arena *a = arena_neu();
    int r = baum_pruefen(fd, a, 0, pfad);
    if (r == 0) r = baum_loeschen(fd, a, 0, pfad);
    int s2 = errno; arena_frei(a); close(fd); errno = s2;
    if (r) return -1;
    if (bschritt(B_WEG, pfd, name, AT_REMOVEDIR, NULL) == 0) return 0;
    /* NEU 1.0.10 (PS5 13.11.2026 13:36, 16:44 und 16:55, /user/data/playden): direkt nach dem Leeren (Teile davon als Besitzer) lehnt die PS5
     * das Entfernen des leeren Ziels ab - OHNE Grund (errno 0, im Protokoll stand dafür "Input/output error"); in einem neuen Lauf geht es
     * sofort (15:07). Der bisherige Nachversuch sprang nur bei EIO an und nahm denselben, vor dem Wechsel geöffneten Ordner darüber.
     * Jetzt bis zu 5 Nachversuche (je 0,3 s) bei EIO, EBADF oder ohne Grund - mit NEU geöffnetem Ordner darüber (wie privat_weg_x beim
     * geleerten Ordner) und nur für genau denselben Ordner (Gerät, Knoten); inzwischen weg = erreicht (Codex 200). */
    int e1 = errno;
    if (e1 == EIO || e1 == EBADF || e1 == 0) {
        int e3 = e1; bool fremd = false;
        for (int nv = 0; nv < 5 && !fremd && !zugriff_rechte_kaputt; nv++) {
            usleep(300000);
            arena *ra = (pfad && g_dz) ? arena_neu() : NULL;
            const char *bl2 = NULL;
            int p2 = ra ? eltern_oeffnen(g_dz, ra, pfad, &bl2) : -1;
            int q = p2 >= 0 ? p2 : pfd; const char *qn = p2 >= 0 ? bl2 : name;
            struct stat st3; int r3 = -1;
            if (bschritt(B_STAT, q, qn, 0, &st3) != 0) { e3 = errno; if (e3 == ENOENT) r3 = 0; }
            else if (!S_ISDIR(st3.st_mode) || st3.st_ino != st.st_ino || st3.st_dev != st.st_dev) { e3 = EIO; fremd = true; }   /* etwas anderes unter dem Namen: nicht anfassen */
            else if (bschritt(B_WEG, q, qn, AT_REMOVEDIR, NULL) == 0) r3 = 0;
            else { e3 = errno; if (e3 == ENOENT) r3 = 0; }
            if (p2 >= 0) close(p2);
            if (ra) arena_frei(ra);
            if (r3 == 0) return 0;
        }
        schritt("Löschen: geleertes Ziel selbst entfernen %s: zuerst %s, nach den Nachversuchen %s", name, e1 ? strerror(e1) : "ohne Grund", e3 ? strerror(e3) : "ohne Grund");
        errno = e3 ? e3 : EIO;
        return -1;
    }
    errno = e1;
    SCHRITT_FEHLER("Löschen: geleertes Ziel selbst entfernen", name);   /* NEU 1.0.10 (PS5 13:36): bisher ohne Schritt-Angabe */
    return -1;
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
    int x = baum_weg_at(pfd, sl + 1, NULL);
    e = errno; close(pfd); free(k); errno = e;
    return x;
}
#endif

static antwort d_schreibe(zugriff *z, arena *a, const char *pfad, const void *daten, size_t n)
{
    dz *d = z->zustand;
    if (stirbt(d, VERB(a, "STOR ", pfad))) return TOT;
#ifdef KERN_LOESCHEN
    if (zugriff_rechte_kaputt) return A(a, 550, "550 Rechte nicht zurueckgestellt - nichts veraendert");   /* NEU 1.0.9 */
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
antwort (*zugriff_kachel_db)(arena *a, const char *id, bool nur_pruefen) = NULL;
antwort (*zugriff_zusatz)(arena *a, const char *arg, bool nur_pruefen) = NULL;   /* NEU 1.0.10 (22:40) */
static bool kennung_gueltig(const char *id);
/* "<Kennung>" oder "<Kennung>/<Name>" (Name: 1-32 Zeichen A-Z a-z 0-9 _ -) */
static bool zusatz_arg_ok(const char *p)
{
    if (!p || strlen(p) < 9) return false;
    char id[10]; memcpy(id, p, 9); id[9] = 0;
    if (!kennung_gueltig(id)) return false;
    if (p[9] == 0) return true;
    if (p[9] != '/' || !p[10] || strlen(p + 10) > 32) return false;
    for (const char *q = p + 10; *q; q++) if (!((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z') || (*q >= '0' && *q <= '9') || *q == '_' || *q == '-')) return false;
    return true;
}
/* Test am PC (ZUSATZ=ok|keins|ablehnen): wie Sony - "ok" entfernt den Ordner des Zusatzinhalts (bzw. aller des Spiels) samt Infos */
static antwort zusatz_test(dz *d, arena *a, const char *t, const char *arg, bool nur_pruefen)
{
    if (!strcmp(t, "keins")) return A(a, 226, "226 Sony führt keinen Zusatzinhalt dazu");
    if (nur_pruefen) return A(a, 200, "200 Sony führt 1 Zusatzinhalt");
    if (!strcmp(t, "ablehnen")) return A(a, 550, "550 Sony hat den Zusatzinhalt nicht ausgetragen (0x80990001)");
    if (!strcmp(t, "teil")) return A(a, 551, "551 Sony hat angenommen, 1 von 2 Zusatzinhalt(en) stehen nach 30 s aber noch in Sonys Liste");
#ifdef KERN_LOESCHEN
    char id[10]; memcpy(id, arg, 9); id[9] = 0;
    const char *rest = arg[9] ? arg + 9 : "";
    baum_weg(echt(d, a, VERB(a, "/user/addcont/", id, rest)));
    baum_weg(echt(d, a, VERB(a, "/user/appmeta/addcont/", id, rest)));
#else
    (void)d; (void)arg;
#endif
    return A(a, 200, "200 ausgetragen: 1");
}   /* NEU 1.0.10 (zugriff.h) */
static bool kennung_gueltig(const char *id)
{
    if (!id || strlen(id) != 9) return false;
    for (int i = 0; i < 9; i++) if (i < 4 ? !(id[i] >= 'A' && id[i] <= 'Z') : !(id[i] >= '0' && id[i] <= '9')) return false;
    return true;
}
int (*zugriff_einbindung)(arena *a, const char *id, const char **fstyp, const char **quelle) = NULL;   /* NEU 1.0.7 */
int (*zugriff_startordner_entfernen)(arena *a, bool nur_pruefen, const char **text) = NULL;   /* NEU 1.0.6 */
int (*zugriff_selbst_uebergeben)(arena *a, bool kachel, bool startordner, const char **text) = NULL;   /* NEU 1.0.6 */
int (*zugriff_system_ex_oeffnen)(arena *a, const char **text) = NULL;   /* NEU 1.0.7 */
void (*zugriff_diagnose)(const char *text) = NULL;   /* NEU 1.0.6 */
antwort (*zugriff_medium_loeschen)(arena *a, const char *pfad, bool nur_pruefen) = NULL;          /* NEU 1.0.7 */
antwort (*zugriff_spielstand_austragen)(arena *a, const char *ordner, bool nur_pruefen) = NULL;   /* NEU 1.0.7 */
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
        {   /* NEU 1.0.7: Sony führt die Kennung nicht und app.db kennt sie nicht - Reste ohne Kachel (UNINSTALL_FEHLT: Liste mit Komma) */
            const char *fe = env(d, "UNINSTALL_FEHLT");
            if (strcmp(t, "fehlt") == 0 || (fe && strstr(VERB(a, ",", fe, ","), VERB(a, ",", id, ","))))
                return A(a, 226, "226 nicht im PS5-Menü eingetragen und nicht in app.db - Reste ohne Kachel");
        }
        if (strcmp(t, "ablehnen") == 0) return A(a, 553, "553 Sony 0x80990001");   /* 553: erster Auftrag ausdrücklich abgelehnt (zugriff.h) */
        /* NEU 1.0.10 (Codex-Prüfung 171, 1): Sony hat angenommen, die Kachel bleibt aber eingetragen (wie plattform_ps5.c nach 60 s) */
        if (strcmp(t, "angenommen") == 0) return A(a, 550, "550 Sony hat angenommen, die Kachel ist nach 60 s aber noch eingetragen");
#ifdef KERN_LOESCHEN
        if (strcmp(t, "halb") == 0) {                             /* NEU 1.0.10 (PS5-Löschtest 12.11.2026, TabShifter): Ordner weg, Kachel bleibt, Ablehnung */
            baum_weg(echt(d, a, norm(a, pfad)));
            baum_weg(echt(d, a, VERB(a, "/user/appmeta/", id)));
            baum_weg(echt(d, a, VERB(a, "/system_data/priv/appmeta/", id)));
            return A(a, 553, "553 Sony 0x80F50005");
        }
#endif
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

/* NEU 1.0.7 (da_Chris86 01.11.2026): Bilder/Videos über Sonys Löschdienst, Spielstände samt Eintrag in der Spielstand-Liste
 * (siehe zugriff.h). Die Pfad-Regeln gelten für Kern, Simulation und PS5-Oberfläche gleich. */
static bool teil_ok(const char *s, size_t l) { return l > 0 && !(l == 1 && s[0] == '.') && !(l == 2 && s[0] == '.' && s[1] == '.'); }
static bool endet_ci(const char *s, const char *e)
{
    size_t n = strlen(s), m = strlen(e);
    if (n < m) return false;
    for (size_t i = 0; i < m; i++) { char c = s[n - m + i]; if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); if (c != e[i]) return false; }
    return true;
}
bool zugriff_medium_pfad(const char *p)
{
    static const char *const W[2] = { "/user/av_contents/photo/", "/user/av_contents/video/" };
    if (!p) return false;
    const char *r = NULL;
    for (int i = 0; i < 2; i++) if (strncmp(p, W[i], strlen(W[i])) == 0) r = p + strlen(W[i]);
    if (!r) return false;
    int teile = 0; const char *letzt = r;
    for (const char *s = r;;) {
        const char *e = strchr(s, '/'); size_t l = e ? (size_t)(e - s) : strlen(s);
        if (!teil_ok(s, l)) return false;
        teile++; letzt = s;
        if (!e) break;
        s = e + 1;
    }
    return teile >= 3 && teile <= 4 && !endet_ci(letzt, ".meta") && !endet_ci(letzt, ".dat");
}
bool zugriff_spielstand_pfad(const char *p, char *benutzer, bool *prospero, char *titel)
{
    if (!p || strncmp(p, "/user/home/", 11) != 0) return false;
    const char *b = p + 11;
    for (int i = 0; i < 8; i++) if (!((b[i] >= '0' && b[i] <= '9') || (b[i] >= 'a' && b[i] <= 'f') || (b[i] >= 'A' && b[i] <= 'F'))) return false;
    if (b[8] != '/') return false;
    const char *s = b + 9; bool pr;
    if (strncmp(s, "savedata_prospero/", 18) == 0) { pr = true; s += 18; }
    else if (strncmp(s, "savedata/", 9) == 0) { pr = false; s += 9; }
    else return false;
    size_t l = strlen(s);
    if (l == 0 || l > 64 || strchr(s, '/') || !teil_ok(s, l)) return false;
    for (size_t i = 0; i < l; i++) { char c = s[i]; if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-' || c == '.')) return false; }
    if (benutzer) { memcpy(benutzer, b, 8); benutzer[8] = 0; }
    if (prospero) *prospero = pr;
    if (titel) { memcpy(titel, s, l); titel[l] = 0; }
    return true;
}
static antwort d_medien(dz *d, arena *a, const char *befehl, const char *pfad)
{
    bool pruefen = strcmp(befehl, "SITE MEDIUM?") == 0;
    if (!pruefen && strcmp(befehl, "SITE MEDIUM") != 0) return A(a, 502, "502 no");
    const char *v = pfad ? norm(a, pfad) : NULL;
    const char *t = env(d, "MEDIEN");                                 /* Test am PC */
    if (t) {
        if (v == NULL) return A(a, 200, "200 OK");
        if (!zugriff_medium_pfad(v)) return A(a, 550, "550 keine Aufnahme der Mediengalerie");
        if (pruefen) return A(a, 200, "200 OK");
        const char *nie = env(d, "MEDIEN_NIE"), *bl = strrchr(v, '/') + 1;
        if (strcmp(t, "ablehnen") == 0 || (nie && strcmp(nie, bl) == 0)) return A(a, 550, "550 Sony hat abgelehnt (0x809D5001)");
        if (strcmp(t, "langsam") == 0) return A(a, 551, "551 Sony hat angenommen, nach 30 s ist die Aufnahme aber noch nicht ganz weg");
#ifdef KERN_LOESCHEN
        if (strcmp(t, "ok") == 0) {                                   /* wie Sony: Aufnahme, .meta/.dat, Vorschaubild, leere Ordner */
            const char *pk = strrchr(bl, '.');
            const char *stamm = pk && pk > bl ? a_kopie_n(a, v, (size_t)(pk - v)) : v;
            const char *weg[4] = { v, VERB(a, stamm, ".meta"), VERB(a, stamm, ".dat"), VERB(a, "/user/av_contents/thumbnails/", v + 18, ".jxr") };
            for (int i = 0; i < 4; i++) unlink(echt(d, a, weg[i]));
            for (int i = 0; i < 4; i += 3) {                          /* leere Ordner bis einschließlich Spiel-Ordner */
                const char *o = weg[i];
                int mind = i == 3 ? 6 : 5;                            /* /user/av_contents/[thumbnails/]<art>/<App>/<Spiel> */
                for (;;) {
                    int x = txt_letzter(o, '/'); if (x <= 0) break;
                    o = a_kopie_n(a, o, (size_t)x);
                    int st = 0; for (const char *q = o; *q; q++) if (*q == '/') st++;
                    if (st < mind || rmdir(echt(d, a, o)) != 0) break;
                }
            }
        }
#endif
        return A(a, 200, "200 OK");
    }
    if (zugriff_medium_loeschen == NULL) return A(a, 502, "502 no");
    return zugriff_medium_loeschen(a, v, v ? pruefen : true);
}
static antwort d_spielstand(dz *d, arena *a, const char *befehl, const char *pfad)
{
    bool pruefen = strcmp(befehl, "SITE SPIELSTAND?") == 0;
    if (!pruefen && strcmp(befehl, "SITE SPIELSTAND") != 0) return A(a, 502, "502 no");
    const char *v = pfad ? norm(a, pfad) : NULL;
    const char *t = env(d, "SPIELSTAND");                             /* Test am PC */
    if (t) {
        if (v == NULL) return A(a, 200, "200 OK");
        if (!zugriff_spielstand_pfad(v, NULL, NULL, NULL)) return A(a, 550, "550 kein Spielstand-Ordner");
        if (strcmp(t, "belegt") == 0) return A(a, 550, "550 der Spielstand ist gerade in Benutzung (Spiel läuft) - erst das Spiel schließen");
        if (pruefen) return A(a, 200, "200 OK");
        if (strcmp(t, "ablehnen") == 0) return A(a, 550, "550 Spielstand-Liste ist gerade belegt - nichts geändert");
        return A(a, 200, "200 2 Einträge ausgetragen");
    }
    if (zugriff_spielstand_austragen == NULL) return A(a, 502, "502 no");
    return zugriff_spielstand_austragen(a, v, v ? pruefen : true);
}

/* ---------------------------------------------------------------- Playden (NEU 1.0.10, siehe zugriff.h) */
int (*zugriff_playden)(arena *a, bool beenden, const char **text) = NULL;
int (*zugriff_titel_laeuft)(const char *id) = NULL;
int (*zugriff_port_belegt)(int port) = NULL;
/* "SITE LAEUFT?" Kennung: 200 = das Spiel läuft (seine Sandbox ist eingehängt), 250 = nein, 502 = nicht feststellbar bzw. gibt es nicht.
 * "SITE PS4MOUNT?": 200 = ps4mount läuft (hält 127.0.0.1:9032 - gasb07/ps4mount src/main.c LOCK_PORT), 250 = nein, 502 = nicht feststellbar */
static antwort d_frage(dz *d, arena *a, const char *befehl, const char *pfad)
{
    if (!strcmp(befehl, "SITE LAEUFT?")) {
        const char *t = env(d, "LAEUFT");
        if (t) return pfad && liste_hat(t, pfad) ? A(a, 200, "200 läuft") : A(a, 250, "250 läuft nicht");
        if (!zugriff_titel_laeuft || !pfad) return A(a, 502, "502 no");
        int r = zugriff_titel_laeuft(pfad);
        return r == 1 ? A(a, 200, "200 läuft") : r == 0 ? A(a, 250, "250 läuft nicht") : A(a, 502, "502 nicht feststellbar");
    }
    const char *t = env(d, "PS4MOUNT");
    if (t) return !strcmp(t, "laeuft") ? A(a, 200, "200 läuft") : A(a, 250, "250 läuft nicht");
    if (!zugriff_port_belegt) return A(a, 502, "502 no");
    int r = zugriff_port_belegt(9032);
    return r == 1 ? A(a, 200, "200 läuft") : r == 0 ? A(a, 250, "250 läuft nicht") : A(a, 502, "502 nicht feststellbar");
}
static antwort d_playden_web(dz *d, arena *a, bool beenden)
{
    const char *t = env(d, "PLAYDEN_WEB");                            /* Test am PC: Zustand in der Datei playden_web.zu im Testordner */
    if (t) {
        const char *zf = VERB(a, d->o.test_hier ? d->o.test_hier : ".", "/playden_web.zu");
        struct stat zst;
        bool zu = strcmp(t, "zu") == 0 || stat(zf, &zst) == 0;
        if (!beenden) return zu ? A(a, 250, "250 keine Antwort") : A(a, 200, "200 Playden 0.10.0 antwortet");
        if (zu) return A(a, 200, "200 beendet");
        if (strcmp(t, "bleibt") == 0) return A(a, 551, "551 Playden hat sich nach 120 s nicht beendet");
#ifdef KERN_LOESCHEN
        int fd = open(zf, O_WRONLY | O_CREAT | O_CLOEXEC, 0666); if (fd >= 0) close(fd);
        return A(a, 200, "200 beendet");
#else
        return A(a, 550, "550 nicht eingebaut");
#endif
    }
    if (!zugriff_playden) return A(a, 502, "502 no");
    const char *tx = "";
    int r = zugriff_playden(a, beenden, &tx);
    if (!beenden) return r == 1 ? A(a, 200, VERB(a, "200 ", tx)) : r == 0 ? A(a, 250, VERB(a, "250 ", tx)) : A(a, 502, VERB(a, "502 ", tx));
    return r == 1 ? A(a, 200, VERB(a, "200 ", tx)) : A(a, 551, VERB(a, "551 ", tx));
}
#ifdef KERN_LOESCHEN
/* Ende des JSON-Werts ab i (Leerraum davor wird übersprungen); -1 = kaputt. Nur Aufbau, keine Bedeutung. */
static long js_ws(const char *s, size_t n, size_t i) { while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++; return (long)i; }
static long js_ende(const char *s, size_t n, size_t i)
{
    i = (size_t)js_ws(s, n, i);
    if (i >= n) return -1;
    if (s[i] == '"') { for (i++; i < n; i++) { if (s[i] == '\\') { i++; continue; } if (s[i] == '"') return (long)i + 1; } return -1; }
    if (s[i] == '{' || s[i] == '[') {
        int tiefe = 0;
        for (; i < n; i++) {
            char c = s[i];
            if (c == '"') { for (i++; i < n && s[i] != '"'; i++) if (s[i] == '\\') i++; if (i >= n) return -1; continue; }
            if (c == '{' || c == '[') { if (++tiefe > 64) return -1; }
            else if (c == '}' || c == ']') { if (--tiefe == 0) return (long)i + 1; }
        }
        return -1;
    }
    size_t a = i;
    while (i < n && s[i] != ',' && s[i] != '}' && s[i] != ']' && s[i] != ' ' && s[i] != '\t' && s[i] != '\r' && s[i] != '\n') i++;
    return i > a ? (long)i : -1;
}
typedef struct { long anf, ende; } js_spanne;
/* Elemente des Arrays "name" im obersten Objekt: Anzahl (höchstens max), -1 = nicht da oder kaputt */
static int js_feld_elemente(const char *s, size_t n, const char *name, js_spanne *el, int max, long *arr_anf)
{
    long i = js_ws(s, n, 0);
    if ((size_t)i >= n || s[i] != '{') return -1;
    i++;
    for (;;) {
        i = js_ws(s, n, (size_t)i);
        if ((size_t)i < n && s[i] == '}') return -1;
        long ke = js_ende(s, n, (size_t)i);
        if (ke < 0 || s[i] != '"') return -1;
        bool treffer = (size_t)(ke - i - 2) == strlen(name) && !strncmp(s + i + 1, name, strlen(name));
        i = js_ws(s, n, (size_t)ke);
        if ((size_t)i >= n || s[i] != ':') return -1;
        long va = js_ws(s, n, (size_t)i + 1), ve = js_ende(s, n, (size_t)va);
        if (ve < 0) return -1;
        if (treffer) {
            if (s[va] != '[') return -1;
            if (arr_anf) *arr_anf = va;
            int z = 0; long j = js_ws(s, n, (size_t)va + 1);
            if (s[j] == ']') return 0;
            for (;;) {
                long ea = js_ws(s, n, (size_t)j), ee = js_ende(s, n, (size_t)ea);
                if (ee < 0 || z >= max) return -1;
                el[z].anf = ea; el[z].ende = ee; z++;
                j = js_ws(s, n, (size_t)ee);
                if ((size_t)j < n && s[j] == ',') { j++; continue; }
                if ((size_t)j < n && s[j] == ']') return z;
                return -1;
            }
        }
        i = js_ws(s, n, (size_t)ve);
        if ((size_t)i < n && s[i] == ',') { i++; continue; }
        return -1;
    }
}
/* Text ohne das Element e eines Arrays (samt dem Komma davor bzw. danach) */
static char *js_ohne(arena *a, const char *s, size_t n, js_spanne e, size_t *neu_n)
{
    long von = e.anf, bis = e.ende;
    long nach = js_ws(s, n, (size_t)bis);
    if ((size_t)nach < n && s[nach] == ',') bis = nach + 1;
    else { long v = von - 1; while (v >= 0 && (s[v] == ' ' || s[v] == '\t' || s[v] == '\r' || s[v] == '\n')) v--; if (v >= 0 && s[v] == ',') von = v; }
    char *r = a_hole(a, n + 1);
    memcpy(r, s, (size_t)von); memcpy(r + von, s + bis, n - (size_t)bis); *neu_n = n - (size_t)(bis - von); r[*neu_n] = 0;
    return r;
}
static const char *js_text(const cJSON *o, const char *k) { const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k); return cJSON_IsString(v) && v->valuestring ? v->valuestring : NULL; }

/* ---- Playdens zwei Listen lesen, prüfen und ersetzen
 * NEU 1.0.10 (PS5-Test da_Chris86 12.11.2026, Mitschnitt 08:46 und Löschprotokoll 19:49): Beim Nachschauen liest PS5 Cleanup installs.json auf
 * der PS5 zuverlässig (Mitschnitt 08:46: vollständiger Inhalt) - über den Pfad, in EINEM Durchgang als Benutzer des versperrten Ordners,
 * eingerahmt von weg_ansehen (lesend_auf -> als_besitzer_oeffnen). Das Austragen ging bis dahin einen anderen Weg: eltern_oeffnen und bschritt
 * mit Ordner-Verweisen, die als PS5 Cleanup geöffnet wurden, Wechsel je Einzelschritt. Genau dieser Weg scheiterte auf der PS5 beim Lesen
 * (19:49, "Installationsliste ist nicht lesbar") wie beim Löschen der privaten Spielstände (12:46, RMDA steam-223850) - auf der PS5 hat er
 * noch nie funktioniert, am PC (test_privat: Nachbildung über Rechte des Ordners) schon. Darum hier für alles (lesen, prüfen, ersetzen) nur
 * noch der Weg des Nachschauens: Ordner und Datei über den Pfad geöffnet, ohne Verknüpfung, alles in einem Durchgang als derselbe Benutzer -
 * erst als PS5 Cleanup selbst; scheitert das an fehlenden Rechten, als Besitzer des Ordners, der den Weg versperrt (wie beim Nachschauen). */
enum { PLJ_LESEN, PLJ_PRUEFEN, PLJ_ERSETZEN };
typedef struct {
    int art; const char *ordner, *name; size_t max; arena *a;
    char *buf; size_t bn; struct stat st, sd;            /* gelesen: Inhalt, die Datei, ihr Ordner */
    const char *alt; size_t altn;                         /* PRUEFEN/ERSETZEN: so muss die Datei noch aussehen (wie gelesen) */
    struct stat alt_st, alt_sd;                           /* ... und es müssen noch dieselbe Datei und derselbe Ordner sein (Codex-Prüfung 169 (3)/(4)) */
    const char *neu; size_t neun;                         /* ERSETZEN: der neue Inhalt */
    int fehler; const char *schritt; bool geschrieben;
    bool test_kein_schreiben;                             /* nur Test (PL_KEIN_SCHREIBEN): Anlegen der neuen Datei scheitert wie ohne Recht */
} pl_job;
static bool pl_job_fehler(pl_job *j, const char *schritt, int e) { j->schritt = schritt; j->fehler = e ? e : EIO; return false; }
/* ganze Datei lesen: gewöhnliche Datei, nur ein Name, höchstens max Bytes, beim Lesen nicht gewachsen */
static bool pl_lesen_fd(pl_job *j, int fd, char **buf, size_t *bn, struct stat *st)
{
    if (fstat(fd, st) != 0) return false;
    if (!S_ISREG(st->st_mode) || st->st_nlink != 1 || st->st_size < 0 || (size_t)st->st_size > j->max) { errno = EINVAL; return false; }
    size_t soviel = (size_t)st->st_size; char *b = a_hole(j->a, soviel + 1); size_t n = 0;
    while (n < soviel) { ssize_t k = read(fd, b + n, soviel - n); if (k < 0) return false; if (k == 0) break; n += (size_t)k; }
    char x; ssize_t mehr = n == soviel ? read(fd, &x, 1) : 0;
    if (n != soviel || mehr != 0) { errno = EAGAIN; return false; }
    b[n] = 0; *buf = b; *bn = n;
    return true;
}
static bool pl_nur_lesbar(int dfd)
{
#ifdef __linux__
    struct statvfs sv; return fstatvfs(dfd, &sv) != 0 || (sv.f_flag & ST_RDONLY);
#else
    struct statfs sf; return fstatfs(dfd, &sf) != 0 || (sf.f_flags & MNT_RDONLY);
#endif
}
static bool pl_job_tu(void *c)
{
    pl_job *j = c;
    j->fehler = 0; j->schritt = NULL;
    int dfd = open(j->ordner, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (dfd < 0) return pl_job_fehler(j, "Ordner öffnen", errno);
    bool ok = false;
    if (fstat(dfd, &j->sd) != 0) { pl_job_fehler(j, "Ordner ansehen", errno); goto zu; }
    /* Codex-Prüfung 169 (3)/(4): beim Prüfen und Ersetzen muss es genau der Ordner sein, der gelesen wurde - wurde unterwegs etwas gegen
     * eine Verknüpfung oder einen anderen Ordner getauscht, wird hier abgebrochen, bevor etwas geschrieben wird */
    if (j->art != PLJ_LESEN && (j->sd.st_dev != j->alt_sd.st_dev || j->sd.st_ino != j->alt_sd.st_ino || j->sd.st_uid != j->alt_sd.st_uid)) {
        pl_job_fehler(j, "Ordner seit dem Lesen getauscht", EAGAIN); goto zu; }
    int fd = openat(dfd, j->name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) { pl_job_fehler(j, "Datei öffnen", errno); goto zu; }
    bool gelesen = pl_lesen_fd(j, fd, &j->buf, &j->bn, &j->st);
    int e = errno; close(fd);
    if (!gelesen) { pl_job_fehler(j, "Datei lesen", e); goto zu; }
    if (j->art == PLJ_LESEN) { ok = true; goto zu; }
    /* prüfen und ersetzen: dieselbe Datei, unverändert seit dem Lesen? */
    if (j->st.st_dev != j->alt_st.st_dev || j->st.st_ino != j->alt_st.st_ino || j->st.st_uid != j->alt_st.st_uid
        || j->bn != j->altn || memcmp(j->buf, j->alt, j->bn) != 0) { pl_job_fehler(j, "Datei hat sich seit dem Lesen geändert", EAGAIN); goto zu; }
    /* ersetzen nur so, dass die neue Datei dem gehört, dem die alte gehört: als ihr Besitzer, in seinem Ordner mit Schreibrecht */
    uid_t ich = geteuid();
    if (j->st.st_uid != ich || j->sd.st_uid != ich) { pl_job_fehler(j, "Datei bzw. Ordner gehört einem anderen Benutzer", EACCES); goto zu; }
    if ((j->sd.st_mode & (S_IWUSR | S_IXUSR)) != (S_IWUSR | S_IXUSR)) { pl_job_fehler(j, "Ordner ohne Schreibrecht", EACCES); goto zu; }
    if (pl_nur_lesbar(dfd)) { pl_job_fehler(j, "Speicher ist nur lesbar", EROFS); goto zu; }
    /* Codex-Prüfung 169 (5) / 170 (5): Name der Zwischendatei - der erste freie von 16; Prüfen verlangt, dass einer frei ist.
     * NEU 1.0.10 (PS5-Test 13.11.2026 11:56, Hitman): auf der PS5 galten in Playdens privatem Ordner alle 16 Namen als belegt ("kein freier
     * Name für die Zwischendatei", 0 Byte gelöscht) - die Vorab-Prüfung mit fstatat zählte JEDEN Fehler außer ENOENT als "belegt" und meldete
     * dann EEXIST statt des echten Grundes. Jetzt: beim Ersetzen entscheidet das Anlegen selbst (O_CREAT|O_EXCL - nur EEXIST heißt belegt,
     * jeder andere Fehler bricht mit seinem echten Grund ab); beim reinen Prüfen (ohne Schreiben) wird der echte Grund gemeldet. */
    mode_t modus = j->st.st_mode & 07777;
    const char *tmp = NULL; int wfd = -1, te = EEXIST;
    for (int v = 0; v < 16 && !tmp; v++) {
        const char *t = VERB(j->a, ".", j->name, ".ps5cleanup-", a_fmt(j->a, "%ld-%d", (long)getpid(), v));
        if (j->art == PLJ_PRUEFEN) {
            struct stat sx;
            if (fstatat(dfd, t, &sx, AT_SYMLINK_NOFOLLOW) == 0) continue;
            if (errno == ENOENT) { tmp = t; break; }
            te = errno;
            continue;
        }
        if (j->test_kein_schreiben) { pl_job_fehler(j, "neue Datei anlegen", EACCES); goto zu; }
        wfd = openat(dfd, t, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, modus);
        if (wfd >= 0) { tmp = t; break; }
        if (errno != EEXIST) { pl_job_fehler(j, "neue Datei anlegen", errno); goto zu; }
    }
    if (!tmp) { pl_job_fehler(j, "kein freier Name für die Zwischendatei", te); goto zu; }
    if (j->art == PLJ_PRUEFEN) { ok = true; goto zu; }
    {
        size_t g = 0; ssize_t k = 0; errno = 0;
        while (g < j->neun && (k = write(wfd, j->neu + g, j->neun - g)) > 0) g += (size_t)k;
        bool w = g == j->neun && fchmod(wfd, modus) == 0 && fsync(wfd) == 0;
        int we = errno;
        if (close(wfd) != 0 && w) { w = false; we = errno; }
        if (!w) { unlinkat(dfd, tmp, 0); pl_job_fehler(j, "neue Datei schreiben", we); goto zu; }
        /* unmittelbar davor: ist es noch dieselbe Datei wie gelesen? */
        struct stat jetzt;
        if (fstatat(dfd, j->name, &jetzt, AT_SYMLINK_NOFOLLOW) != 0) {   /* NEU 1.0.10 (PS5-Test 13.11.): echter Grund statt "getauscht" */
            int fe = errno; unlinkat(dfd, tmp, 0); pl_job_fehler(j, "Datei vor dem Einsetzen ansehen", fe); goto zu; }
        if (jetzt.st_dev != j->st.st_dev || jetzt.st_ino != j->st.st_ino || jetzt.st_size != j->st.st_size) {
            unlinkat(dfd, tmp, 0); pl_job_fehler(j, "Datei inzwischen getauscht", EAGAIN); goto zu; }
        if (renameat(dfd, tmp, dfd, j->name) != 0) { int re = errno; unlinkat(dfd, tmp, 0); pl_job_fehler(j, "neue Datei einsetzen", re); goto zu; }
        j->geschrieben = true;
        fsync(dfd);
        /* gegenlesen: steht genau das Neue drin? */
        int rfd = openat(dfd, j->name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
        char *nb = NULL; size_t nn = 0; struct stat ns;
        bool gl = rfd >= 0 && pl_lesen_fd(j, rfd, &nb, &nn, &ns);
        if (rfd >= 0) close(rfd);
        if (!gl || nn != j->neun || memcmp(nb, j->neu, nn) != 0) { pl_job_fehler(j, "Gegenlesen nach dem Schreiben", EIO); goto zu; }
        j->st = ns; j->buf = nb; j->bn = nn;
        ok = true;
    }
zu:
    close(dfd);
    return ok;
}
static _Thread_local const char *g_pl_grund = NULL;
/* v = Datei. 1 = gemacht, 0 = (nur beim Lesen) Datei bzw. Ordner nicht da, -1 = nicht gegangen (Grund in g_pl_grund, j->geschrieben sagt,
 * ob die Datei trotzdem ersetzt wurde) */
static int pl_machen(dz *d, arena *a, const char *v, pl_job *j)
{
    g_pl_grund = NULL;
    const char *sl = strrchr(v, '/');
    if (!sl || sl == v) { g_pl_grund = "Weg: ungültig"; return -1; }
    const char *vo = a_kopie_n(a, v, (size_t)(sl - v));
    j->ordner = echt(d, a, vo); j->name = sl + 1; j->a = a; j->geschrieben = false; j->fehler = 0; j->schritt = NULL;
    j->test_kein_schreiben = env(d, "PL_KEIN_SCHREIBEN") != NULL;
    /* 1. als PS5 Cleanup selbst - eingerahmt wie beim Wechsel (Weg ohne Verknüpfung, vorher und nachher derselbe) */
    weg_auftrag w = { d, a, vo, pl_job_tu, j, { 0 } };
    errno = 0;
    bool ok = !zugriff_rechte_kaputt && weg_tu(&w);
    int e = j->fehler ? j->fehler : errno;
    weg_kette k = w.vor;
    bool selbst_weg_fehlt = !ok && !j->fehler && e == ENOENT;
    /* 2. an fehlenden Rechten gescheitert: genau wie beim Nachschauen als Besitzer des Ordners, der den Weg versperrt */
    if (!ok && !j->geschrieben && rechte_fehler(e)) {
        j->fehler = 0; j->schritt = NULL; errno = 0;
        ok = als_besitzer_nochmal(d, a, vo, pl_job_tu, j, &k);
        e = j->fehler ? j->fehler : errno;
        if (!ok && !j->fehler) j->schritt = "als Besitzer";
    }
    if (ok && (k.n < 1 || j->sd.st_dev != k.dev[k.n - 1] || j->sd.st_ino != k.ino[k.n - 1])) { ok = false; j->schritt = "Ordner inzwischen getauscht"; e = EAGAIN; }
    if (ok) return 1;
    if (j->art == PLJ_LESEN && e == ENOENT && (selbst_weg_fehlt || (j->schritt && !strcmp(j->schritt, "Datei öffnen")))) return 0;
    g_pl_grund = VERB(a, j->schritt ? j->schritt : "Weg ansehen", ": ", strerror(e ? e : EIO));
    return -1;
}
static int pl_lies(dz *d, arena *a, const char *v, size_t max, pl_job *j)
{
    memset(j, 0, sizeof *j); j->art = PLJ_LESEN; j->max = max;
    return pl_machen(d, a, v, j);
}
/* gelesene Datei (j aus pl_lies) prüfen bzw. ersetzen - nur, wenn es noch dieselbe Datei im selben Ordner ist und sie genau so aussieht.
 * *aus (darf NULL sein): der Stand danach (beim Ersetzen die neue Datei) */
static int pl_aendern(dz *d, arena *a, const char *v, const pl_job *gelesen, int art, const char *neu, size_t neun, pl_job *aus)
{
    pl_job j; memset(&j, 0, sizeof j);
    j.art = art; j.max = gelesen->max; j.alt = gelesen->buf; j.altn = gelesen->bn; j.alt_st = gelesen->st; j.alt_sd = gelesen->sd; j.neu = neu; j.neun = neun;
    int r = pl_machen(d, a, v, &j);
    if (aus) *aus = j;
    if (r < 0 && j.geschrieben) return -2;   /* ersetzt, aber nicht bestätigt */
    return r;
}
#define PL_INSTALLS "/user/data/playden/runtime-state/installs.json"
/* NEU 1.0.10 (da_Chris86 12.11.2026 22:40, ShadowMount): "SITE SM-QUELLE-WEG" Zeile - die Zeile (eine Spielquelle) aus ShadowMounts Liste
 * /data/shadowmount/manual.lst austragen (ShadowMountPlus sm_paths.h MANUAL_LIST_FILE; docs/api.md "/api/v1/manual/remove": "Remove matching
 * source lines from manual.lst"). Wie Playden (runtime_host.cpp RetireTitle) soll ShadowMount eine entfernte Quelle nicht mehr suchen.
 * Nur genau gleiche Zeilen (ohne \r und abschließendes /), alles andere Byte für Byte gleich; derselbe sichere Weg wie Playdens Listen
 * (pl_lies/pl_aendern: Zwischendatei, Besitzer und Rechte wie vorher, in einem Schritt getauscht, gegengelesen).
 * 200 = ausgetragen, 226 = stand nicht drin (oder keine Liste), sonst nichts geändert. Der Kern ruft es nur, wenn ShadowMount angehalten ist. */
#define SM_MANUELL "/user/data/shadowmount/manual.lst"
static size_t sm_zeile_kern(const char *z, size_t l)   /* Länge ohne \r, Leerzeichen und abschließendes / */
{
    while (l && (z[l - 1] == '\r' || z[l - 1] == ' ' || z[l - 1] == '\t')) l--;
    while (l > 1 && z[l - 1] == '/') l--;
    return l;
}
static antwort d_sm_quelle_weg(dz *d, arena *a, const char *zeile)
{
    if (!zeile || zeile[0] != '/' || strchr(zeile, '\n') || strstr(zeile, "/../") || strlen(zeile) > 1000) return A(a, 550, "550 keine Spielquelle");
    size_t zl = sm_zeile_kern(zeile, strlen(zeile));
    pl_job j; int r = pl_lies(d, a, SM_MANUELL, 1u << 20, &j);
    if (r == 0) return A(a, 226, "226 ShadowMount hat keine Liste manual.lst");
    if (r < 0) return A(a, 550, VERB(a, "550 ShadowMounts Liste ist nicht lesbar (", g_pl_grund ? g_pl_grund : "?", ") - nichts geändert"));
    if (memchr(j.buf, 0, j.bn)) return A(a, 550, "550 ShadowMounts Liste sieht anders aus als erwartet - nichts geändert");
    char *neu = a_hole(a, j.bn + 1); size_t nn = 0; int weg = 0;
    for (size_t i = 0; i < j.bn; ) {
        const char *e = memchr(j.buf + i, '\n', j.bn - i); size_t l = e ? (size_t)(e - (j.buf + i)) + 1 : j.bn - i;
        size_t kl = sm_zeile_kern(j.buf + i, l - (e ? 1 : 0));
        if (kl == zl && !memcmp(j.buf + i, zeile, zl)) weg++;
        else { memcpy(neu + nn, j.buf + i, l); nn += l; }
        i += l;
    }
    if (!weg) return A(a, 226, "226 stand nicht in ShadowMounts Liste");
    if (env(d, "SM_KEIN_SCHREIBEN")) return A(a, 550, "550 ShadowMounts Liste ließ sich nicht schreiben (Test) - nichts geändert");   /* nur Test */
    int w = pl_aendern(d, a, SM_MANUELL, &j, PLJ_ERSETZEN, neu, nn, NULL);
    if (w == 1) return A(a, 200, a_fmt(a, "200 aus ShadowMounts Liste ausgetragen: %d Zeile(n)", weg));
    if (w == -2) return A(a, 551, VERB(a, "551 ShadowMounts Liste geschrieben, aber nicht bestätigt (", g_pl_grund ? g_pl_grund : "?", ")"));
    return A(a, 550, VERB(a, "550 ShadowMounts Liste ließ sich nicht schreiben (", g_pl_grund ? g_pl_grund : "?", ") - nichts geändert"));
}
#define PL_REGISTER "/user/data/playden-games/.playden-games"
/* "SITE PLAYDEN-AUSTRAGEN" bzw. (nur_pruefen) "SITE PLAYDEN-PRUEFEN": arg = "<Kennung>[:<Schlüssel>]". Prüfen macht alles wie das Austragen
 * (lesen, Aufbau, Eintrag eindeutig, keine laufende Arbeit, neue Listen gebaut und geprüft) und schreibt dann jede Liste, die das Austragen
 * ändern würde, MIT IHREM BISHERIGEN INHALT auf genau demselben Weg neu (Zwischendatei, Besitzer, Rechte, Einsetzen, Gegenlesen).
 * NEU 1.0.10 (da_Chris86 12.11.2026 22:40, Fallout-Test 19:49: 10,3 GB gelöscht, dann scheiterte Playdens Liste): bis dahin prüfte die
 * Vorprüfung nur Rechte-Bits - ob das Schreiben auf der Konsole wirklich geht, zeigte erst der echte Versuch NACH dem Löschen. Jetzt ist
 * das Schreiben selbst vorher bewiesen; inhaltlich ändert die Vorprüfung nichts (gleicher Inhalt, gleiche Rechte, gleicher Besitzer). */
/* pruefart: 0 = austragen, 1 = prüfen samt Schreibbeweis (gleicher Inhalt), 2 = nur prüfen ohne zu schreiben ("SITE PLAYDEN-PRUEFEN?", Testmodus) */
static antwort d_playden_austragen(dz *d, arena *a, const char *arg, int pruefart)
{
    bool nur_pruefen = pruefart != 0;
    /* der Schlüssel (Steam-App-Nummer) aus der Suche gilt nur, wenn das Spiel nicht mehr in der Installationsliste steht (Codex-Prüfung 163 (6):
     * ein früherer Versuch hat sie schon geändert, die Spieleliste aber nicht); steht es dort, muss er passen */
    const char *id = arg, *kern_schl = NULL;
    if (arg && strchr(arg, ':')) { id = a_kopie_n(a, arg, (size_t)(strchr(arg, ':') - arg)); kern_schl = strchr(arg, ':') + 1; }
    if (kern_schl) { bool ziff = kern_schl[0] != 0 && strlen(kern_schl) <= 10; for (const char *q = kern_schl; *q; q++) if (*q < '0' || *q > '9') ziff = false; if (!ziff) kern_schl = NULL; }
    if (!id || strlen(id) != 9 || strncmp(id, "FAKE78", 6) != 0 || id[6] < '0' || id[6] > '9' || id[7] < '0' || id[7] > '9' || id[8] < '0' || id[8] > '9')
        return A(a, 550, "550 keine Kennung eines Playden-Spiels");
    pl_job jl; int r = pl_lies(d, a, PL_INSTALLS, 4u << 20, &jl);
    char *j = jl.buf; size_t jn = jl.bn; mode_t jmod = jl.st.st_mode & 07777;
    if (r == 0 && !kern_schl) return A(a, 226, "226 Playdens Installationsliste gibt es nicht (Playden ist entfernt) - nichts auszutragen");
    if (r < 0) return A(a, 550, VERB(a, "550 Playdens Installationsliste ist nicht lesbar (", g_pl_grund ? g_pl_grund : "?", ") - nichts geändert"));
    if (r == 0) { j = a_kopie(a, "{\"schema_version\":1,\"reservations\":[],\"installs\":[]}"); jn = strlen(j); jmod = 0600; }   /* nur noch die Spieleliste */
    if (memchr(j, 0, jn) || (jmod != 0600 && jmod != 0644)) return A(a, 550, "550 Playdens Installationsliste sieht anders aus als erwartet - nichts geändert");
    {
        cJSON *o = cJSON_ParseWithOpts(j, NULL, true);
        const cJSON *sv = o ? cJSON_GetObjectItemCaseSensitive(o, "schema_version") : NULL;
        bool schema = cJSON_IsNumber(sv) && sv->valuedouble == 1;
        cJSON_Delete(o);
        if (!schema) return A(a, 550, "550 Playdens Installationsliste hat eine unbekannte Fassung - nichts geändert");
    }
    js_spanne in[512], rs[512];
    int ni = js_feld_elemente(j, jn, "installs", in, 512, NULL), nr = js_feld_elemente(j, jn, "reservations", rs, 512, NULL);
    if (ni < 0 || nr < 0) return A(a, 550, "550 Playdens Installationsliste ist nicht so aufgebaut wie erwartet - nichts geändert");
    int wi = -1, wr = -1, treffer_i = 0, treffer_r = 0; const char *inst_id = NULL, *schluessel = NULL;
    for (int i = 0; i < ni; i++) {
        cJSON *e = cJSON_ParseWithOpts(a_kopie_n(a, j + in[i].anf, (size_t)(in[i].ende - in[i].anf)), NULL, true);
        const char *t = e ? js_text(e, "title_id") : NULL;
        if (t && !strcmp(t, id)) {
            treffer_i++; wi = i;
            const char *ii = js_text(e, "install_id"), *zs = js_text(e, "state");
            const cJSON *jr = cJSON_GetObjectItemCaseSensitive(e, "journal"), *ap = cJSON_GetObjectItemCaseSensitive(e, "app_id"), *st = cJSON_GetObjectItem(e, "store");
            inst_id = ii ? a_kopie(a, ii) : NULL;
            bool ruhig = zs && (!strcmp(zs, "ready") || !strcmp(zs, "failed") || !strcmp(zs, "unavailable") || !strcmp(zs, "downloaded"));
            if (!ii || !ruhig || !cJSON_IsNull(jr)) { cJSON_Delete(e); return A(a, 550, "550 Playden ist bei diesem Spiel mitten in einer Arbeit (einrichten oder entfernen) - nichts geändert"); }
            if (st || !cJSON_IsNumber(ap) || ap->valuedouble < 1 || ap->valuedouble > 4294967295.0 || ap->valuedouble != (double)(unsigned long long)ap->valuedouble) {
                cJSON_Delete(e); return A(a, 550, "550 kein Steam-Spiel - das kann nur Playden selbst austragen, nichts geändert"); }
            schluessel = a_fmt(a, "%llu", (unsigned long long)ap->valuedouble);
        }
        cJSON_Delete(e);
    }
    for (int i = 0; i < nr; i++) {
        cJSON *e = cJSON_ParseWithOpts(a_kopie_n(a, j + rs[i].anf, (size_t)(rs[i].ende - rs[i].anf)), NULL, true);
        const char *t = e ? js_text(e, "title_id") : NULL, *ii = e ? js_text(e, "install_id") : NULL;
        if (t && !strcmp(t, id)) { treffer_r++; wr = i; if (!inst_id || !ii || strcmp(ii, inst_id) != 0) wr = -2; }
        cJSON_Delete(e);
    }
    bool in_liste = true;
    if (treffer_i == 0 && treffer_r == 0) { in_liste = false; schluessel = kern_schl; }
    else if (kern_schl && schluessel && strcmp(kern_schl, schluessel) != 0) return A(a, 550, "550 die Steam-Nummer des Spiels hat sich seit der Suche geändert - nichts geändert");
    else if (treffer_i != 1 || treffer_r != 1 || wi < 0 || wr < 0) return A(a, 550, "550 das Spiel steht in Playdens Liste nicht eindeutig - nichts geändert");
    /* Spieleliste: genau eine Zeile mit diesem Schlüssel, ohne laufende Arbeit (Download, Update, Verschieben, Entfernen) */
    pl_job jg; memset(&jg, 0, sizeof jg);
    int rg = schluessel ? pl_lies(d, a, PL_REGISTER, 1u << 20, &jg) : 0;
    if (rg < 0) return A(a, 550, VERB(a, "550 Playdens Spieleliste ist nicht lesbar (", g_pl_grund ? g_pl_grund : "?", ") - nichts geändert"));
    char *g = jg.buf; size_t gn = jg.bn;
    char *gneu = NULL; size_t gneu_n = 0; int gzeilen = 0;
    if (rg > 0) {
        if (memchr(g, 0, gn)) return A(a, 550, "550 Playdens Spieleliste sieht anders aus als erwartet - nichts geändert");
        gneu = a_hole(a, gn + 1);
        size_t sl = strlen(schluessel);
        for (size_t i = 0; i < gn; ) {
            const char *e = memchr(g + i, '\n', gn - i); size_t l = e ? (size_t)(e - (g + i)) + 1 : gn - i;
            bool diese = l > sl && !strncmp(g + i, schluessel, sl) && (g[i + sl] == '\t' || g[i + sl] == '\n' || g[i + sl] == '\r');
            if (diese) {
                const char *z = a_kopie_n(a, g + i, l);
                const char *f3 = strchr(z, '\t'); f3 = f3 ? strchr(f3 + 1, '\t') : NULL;   /* drittes Feld: Größe - 0 = Download in der Warteschlange */
                bool wartet = !f3 || (f3[1] == '0' && (f3[2] == '\t' || f3[2] == '\n' || f3[2] == '\r' || f3[2] == 0));
                if (strstr(z, "\tremoving") || strstr(z, "\tupdate") || strstr(z, "\tmove=") || strstr(z, "\tpaused") || wartet)
                    return A(a, 550, "550 Playden hat bei diesem Spiel noch eine Arbeit offen (Download, Update, Verschieben) - nichts geändert");
                gzeilen++;
            } else { memcpy(gneu + gneu_n, g + i, l); gneu_n += l; }
            i += l;
        }
        if (gzeilen > 1) return A(a, 550, "550 das Spiel steht zweimal in Playdens Spieleliste - nichts geändert");
    }
    if (!in_liste && gzeilen == 0) return A(a, 226, "226 Playden führt das Spiel in keiner Liste");
    /* neue Installationsliste: genau die beiden Einträge weg, sonst alles gleich - und so geprüft, wie Playden sie liest */
    char *t2 = NULL; size_t n2 = 0;
    if (in_liste) {
        js_spanne a1 = in[wi], a2 = rs[wr];
        size_t n1 = 0; char *t1;
        if (a1.anf > a2.anf) { t1 = js_ohne(a, j, jn, a1, &n1); js_spanne e2[512]; int z = js_feld_elemente(t1, n1, "reservations", e2, 512, NULL); if (z != nr) return A(a, 550, "550 Prüfung der neuen Liste fehlgeschlagen - nichts geändert"); t2 = js_ohne(a, t1, n1, e2[wr], &n2); }
        else { t1 = js_ohne(a, j, jn, a2, &n1); js_spanne e1[512]; int z = js_feld_elemente(t1, n1, "installs", e1, 512, NULL); if (z != ni) return A(a, 550, "550 Prüfung der neuen Liste fehlgeschlagen - nichts geändert"); t2 = js_ohne(a, t1, n1, e1[wi], &n2); }
        js_spanne c1[512], c2[512];
        int ki = js_feld_elemente(t2, n2, "installs", c1, 512, NULL), kr = js_feld_elemente(t2, n2, "reservations", c2, 512, NULL);
        cJSON *o = cJSON_ParseWithOpts(t2, NULL, true);
        bool gut = o && cJSON_IsObject(o) && ki == ni - 1 && kr == nr - 1 && !strstr(t2, VERB(a, "\"", id, "\""));
        const cJSON *sv = o ? cJSON_GetObjectItemCaseSensitive(o, "schema_version") : NULL;
        if (!cJSON_IsNumber(sv) || sv->valuedouble != 1) gut = false;
        cJSON_Delete(o);
        for (int i = 0, q = 0; gut && i < ni; i++) { if (i == wi) continue; size_t l = (size_t)(in[i].ende - in[i].anf); if ((size_t)(c1[q].ende - c1[q].anf) != l || memcmp(j + in[i].anf, t2 + c1[q].anf, l)) gut = false; q++; }
        for (int i = 0, q = 0; gut && i < nr; i++) { if (i == wr) continue; size_t l = (size_t)(rs[i].ende - rs[i].anf); if ((size_t)(c2[q].ende - c2[q].anf) != l || memcmp(j + rs[i].anf, t2 + c2[q].anf, l)) gut = false; q++; }
        if (!gut) return A(a, 550, "550 Prüfung der neuen Installationsliste fehlgeschlagen - nichts geändert");
    }
    const char *was = VERB(a, "Installationsliste ", in_liste ? "ja" : "war nicht drin", ", Spieleliste ", gzeilen ? "ja" : "war nicht drin");
    if (nur_pruefen) {
        /* lassen sich beide so ersetzen, wie es das Austragen gleich täte? Bewiesen durch Ersetzen mit dem bisherigen Inhalt (siehe oben).
         * Im Testmodus (pruefart 2) wie bis 22:40 nur die Bedingungen, ohne zu schreiben - der Testmodus verändert auf der Konsole nichts */
        int pa = pruefart == 2 ? PLJ_PRUEFEN : PLJ_ERSETZEN;
        if (in_liste && pl_aendern(d, a, PL_INSTALLS, &jl, pa, pa == PLJ_ERSETZEN ? j : NULL, pa == PLJ_ERSETZEN ? jn : 0, NULL) != 1)
            return A(a, 550, VERB(a, "550 Playdens Installationsliste ließe sich nicht ändern (", g_pl_grund ? g_pl_grund : "?", ") - nichts geändert"));
        if (gzeilen == 1 && pl_aendern(d, a, PL_REGISTER, &jg, pa, pa == PLJ_ERSETZEN ? g : NULL, pa == PLJ_ERSETZEN ? gn : 0, NULL) != 1)
            return A(a, 550, VERB(a, "550 Playdens Spieleliste ließe sich nicht ändern (", g_pl_grund ? g_pl_grund : "?", ") - nichts geändert"));
        return A(a, 200, VERB(a, "200 austragbar: ", was));
    }
    /* Reihenfolge wie Playdens Uninstall: erst der Eintrag (runtime_host.cpp EraseAfterUnregister), dann die Spieleliste (SaveRegistry) */
    pl_job jneu; memset(&jneu, 0, sizeof jneu);
    if (in_liste) {
        int w1 = pl_aendern(d, a, PL_INSTALLS, &jl, PLJ_ERSETZEN, t2, n2, &jneu);
        if (w1 == -2) return A(a, 551, VERB(a, "551 Playdens Installationsliste geschrieben, aber nicht bestätigt (", g_pl_grund ? g_pl_grund : "?", ")"));
        if (w1 != 1) return A(a, 550, VERB(a, "550 Playdens Installationsliste ließ sich nicht schreiben (", g_pl_grund ? g_pl_grund : "?", ") - nichts geändert"));
    }
    if (gzeilen == 1) {
        int w2 = pl_aendern(d, a, PL_REGISTER, &jg, PLJ_ERSETZEN, gneu, gneu_n, NULL);
        if (w2 != 1) {
            const char *e = g_pl_grund ? g_pl_grund : "?";
            if (w2 == -2) return A(a, 551, VERB(a, "551 Playdens Spieleliste geschrieben, aber nicht bestätigt (", e, ")"));
            /* Codex-Prüfung 163 (6): die Installationsliste wieder so, wie sie war - kein halber Zustand */
            bool zurueck = !in_liste;
            if (in_liste) zurueck = pl_aendern(d, a, PL_INSTALLS, &jneu, PLJ_ERSETZEN, j, jn, NULL) == 1;   /* jneu: die eben geschriebene Datei */
            return zurueck ? A(a, 550, VERB(a, "550 Playdens Spieleliste ließ sich nicht schreiben (", e, ") - nichts geändert"))
                           : A(a, 551, VERB(a, "551 aus der Installationsliste ausgetragen, die Spieleliste ließ sich aber nicht schreiben (", e, ")"));
        }
    }
    /* nachher: steht das Spiel wirklich nirgends mehr? */
    pl_job j1, j2; memset(&j2, 0, sizeof j2);
    int r1 = pl_lies(d, a, PL_INSTALLS, 4u << 20, &j1);
    int r2 = schluessel ? pl_lies(d, a, PL_REGISTER, 1u << 20, &j2) : 0;
    bool noch = r1 < 0 || r2 < 0;
    for (size_t i = 0; r2 > 0 && i < j2.bn && !noch; ) {
        size_t sl = strlen(schluessel);
        if (!strncmp(j2.buf + i, schluessel, sl) && (j2.buf[i + sl] == '\t' || j2.buf[i + sl] == '\n' || j2.buf[i + sl] == '\r' || j2.buf[i + sl] == 0)) noch = true;
        const char *e = memchr(j2.buf + i, '\n', j2.bn - i); i = e ? (size_t)(e - j2.buf) + 1 : j2.bn;
    }
    if (r1 > 0 && strstr(j1.buf, VERB(a, "\"", id, "\""))) noch = true;
    return !noch ? A(a, 200, VERB(a, "200 ausgetragen: ", was)) : A(a, 551, "551 nach dem Austragen steht das Spiel noch in einer Liste von Playden");
}
/* NEU 1.0.10 (PS5-Test 12.11.2026): "SITE PLAYDEN-PROBE" - nur lesen: lassen sich Playdens zwei Listen auf dem Weg des Austragens lesen?
 * Beim Nachschauen aufgerufen, damit das Protokoll es schon vor dem Löschen zeigt. Verändert nichts. */
static antwort d_playden_probe(dz *d, arena *a)
{
    pl_job j1, j2;
    int r1 = pl_lies(d, a, PL_INSTALLS, 4u << 20, &j1); const char *g1 = g_pl_grund;
    int r2 = pl_lies(d, a, PL_REGISTER, 1u << 20, &j2); const char *g2 = g_pl_grund;
    const char *e1 = r1 > 0 ? a_fmt(a, "lesbar (%zu Byte)", j1.bn) : r1 == 0 ? "nicht vorhanden" : VERB(a, "NICHT lesbar (", g1 ? g1 : "?", ")");
    const char *e2 = r2 > 0 ? a_fmt(a, "lesbar (%zu Byte)", j2.bn) : r2 == 0 ? "nicht vorhanden" : VERB(a, "NICHT lesbar (", g2 ? g2 : "?", ")");
    return A(a, r1 >= 0 && r2 >= 0 ? 200 : 550, VERB(a, r1 >= 0 && r2 >= 0 ? "200 " : "550 ", "Installationsliste ", e1, ", Spieleliste ", e2));
}
#endif

#ifdef KERN_LOESCHEN
static antwort d_befehl_loeschen(dz *d, arena *a, const char *befehl, const char *pfad);
#endif
static antwort d_befehl(zugriff *z, arena *a, const char *befehl, const char *pfad)
{
    dz *d = z->zustand;
    if (stirbt(d, pfad ? VERB(a, befehl, " ", pfad) : befehl)) return TOT;
    if (strcmp(befehl, "NOOP") == 0) return A(a, 200, "200 NOOP OK");
    /* Codex-Prüfung 126 (P1), behoben 1.0.10: nach dem Sicherheitsstopp auch keine verändernden Sony-Befehle mehr (Deinstallieren,
     * Medien, Spielstände) - nur noch Fragen ("...?") und die Liste der privaten Orte */
    if (zugriff_rechte_kaputt && befehl[0] && befehl[strlen(befehl) - 1] != '?' && strncmp(befehl, "SITE PRIVAT", 11) != 0)
        return A(a, 550, "550 Rechte nicht zurueckgestellt - nichts veraendert");
    if (strncmp(befehl, "SITE UNINSTALL", 14) == 0) return d_sony(d, a, befehl, pfad);
    if (strncmp(befehl, "SITE MEDIUM", 11) == 0) return d_medien(d, a, befehl, pfad);           /* NEU 1.0.7 */
    if (strncmp(befehl, "SITE SPIELSTAND", 15) == 0) return d_spielstand(d, a, befehl, pfad);   /* NEU 1.0.7 */
    if (strncmp(befehl, "SITE PRIVAT", 11) == 0) return d_privat(d, a, befehl);                 /* NEU 1.0.9 */
    if (strcmp(befehl, "SITE PLAYDEN?") == 0) return d_playden_web(d, a, false);                 /* NEU 1.0.10 */
    if (strcmp(befehl, "SITE KACHEL-DB?") == 0) {                                                /* NEU 1.0.10, ändert nichts */
        if (!kennung_gueltig(pfad)) return A(a, 550, "550 keine Titel-Kennung");
        return zugriff_kachel_db ? zugriff_kachel_db(a, pfad, true) : A(a, 502, "502 in dieser Fassung nicht eingebaut");
    }
    if (strcmp(befehl, "SITE LAEUFT?") == 0 || strcmp(befehl, "SITE PS4MOUNT?") == 0) return d_frage(d, a, befehl, pfad);   /* NEU 1.0.10 */
    if (strcmp(befehl, "SITE ZUSATZ?") == 0) {                                                   /* NEU 1.0.10 (22:40), ändert nichts */
        if (!zusatz_arg_ok(pfad)) return A(a, 550, "550 keine Kennung eines Zusatzinhalts");
        const char *t = env(d, "ZUSATZ");
        if (t) return zusatz_test(d, a, t, pfad, true);
        return zugriff_zusatz ? zugriff_zusatz(a, pfad, true) : A(a, 502, "502 in dieser Fassung nicht eingebaut");
    }
#ifdef KERN_LOESCHEN
    if (strcmp(befehl, "SITE PLAYDEN-ENDE") == 0) return d_playden_web(d, a, true);              /* NEU 1.0.10 */
    if (strcmp(befehl, "SITE PLAYDEN-AUSTRAGEN") == 0) return d_playden_austragen(d, a, pfad, 0);   /* NEU 1.0.10 */
    if (strcmp(befehl, "SITE PLAYDEN-PRUEFEN") == 0) return d_playden_austragen(d, a, pfad, 1);   /* NEU 1.0.10: vor dem Löschen, ändert inhaltlich nichts (Schreibbeweis) */
    if (strcmp(befehl, "SITE PLAYDEN-PRUEFEN?") == 0) return d_playden_austragen(d, a, pfad, 2);  /* NEU 1.0.10 (22:40): Testmodus - schreibt nichts */
    if (strcmp(befehl, "SITE SM-QUELLE-WEG") == 0) return d_sm_quelle_weg(d, a, pfad);            /* NEU 1.0.10 (22:40) */
    if (strcmp(befehl, "SITE PLAYDEN-PROBE") == 0) return d_playden_probe(d, a);                 /* NEU 1.0.10, nur lesen */
    if (strcmp(befehl, "SITE KACHEL-DB") == 0) {                                                 /* NEU 1.0.10 */
        if (!kennung_gueltig(pfad)) return A(a, 550, "550 keine Titel-Kennung");
        return zugriff_kachel_db ? zugriff_kachel_db(a, pfad, false) : A(a, 502, "502 in dieser Fassung nicht eingebaut");
    }
    if (strcmp(befehl, "SITE ZUSATZ") == 0) {                                                    /* NEU 1.0.10 (22:40) */
        if (!zusatz_arg_ok(pfad)) return A(a, 550, "550 keine Kennung eines Zusatzinhalts");
        const char *t = env(d, "ZUSATZ");
        if (t) return zusatz_test(d, a, t, pfad, false);
        return zugriff_zusatz ? zugriff_zusatz(a, pfad, false) : A(a, 502, "502 in dieser Fassung nicht eingebaut");
    }
#endif
#ifdef KERN_LOESCHEN
    if (!pfad) return A(a, 502, "502 no");
    if (zugriff_rechte_kaputt) return A(a, 550, "550 Rechte nicht zurueckgestellt - nichts veraendert");   /* NEU 1.0.9 */
    /* NEU 1.0.9: Besitzer-Wechsel nur für DELE, RMD und RMDA mit einem Ziel im erlaubten Bereich - für die Dauer dieses Befehls */
    const char *v = norm(a, pfad);
    bool loesch = strcmp(befehl, "DELE") == 0 || strcmp(befehl, "RMD") == 0 || strcmp(befehl, "RMDA") == 0;
    g_besitzer_frei = loesch && besitzer_erlaubt(v); g_dz = d; g_ziel = v;
    antwort erg = d_befehl_loeschen(d, a, befehl, pfad);
    g_besitzer_frei = false; g_dz = NULL; g_ziel = NULL;
    return erg;
#else
    (void)pfad;
    return A(a, 550, "550 nicht eingebaut");
#endif
}
#ifdef KERN_LOESCHEN
/* NEU 1.0.10 (da_Chris86 12.11.2026 22:40): private Ordner von Programmen auf der PS5 vollständig löschen. Der Wechsel je Einzelschritt
 * (bschritt mit Ordner-Verweisen, die als PS5 Cleanup geöffnet wurden) hat auf der PS5 nie funktioniert (12:46 RMDA steam-223850.kept,
 * 19:49 Lesen der Installationsliste) - zuverlässig ist dort nur der Weg des Nachschauens: alles in EINEM Durchgang als Besitzer dessen,
 * was den Weg versperrt, über den Pfad und ohne Verknüpfung geöffnet (wie pl_machen für Playdens Listen). Scheitert DELE, RMD oder RMDA
 * an fehlenden Rechten, wird der ganze Befehl genau so einmal wiederholt: Weg zum Ziel (eltern_oeffnen: ohne Verknüpfung, keine Einbindung),
 * dann wie bisher (RMDA: erst den ganzen Baum prüfen, dann löschen) - innerhalb des Durchgangs ohne weiteren Wechsel. Vorher (als Besitzer)
 * der Blick auf den Weg vom Ordner des Programms bis zum Ziel (weg_ansehen: keine Verknüpfung), und das geöffnete Ziel muss genau das dort
 * gesehene sein (Gerät, Knoten) - so geht es auch für den Ordner des Programms selbst (Codex-Prüfung 172 (3): der Rahmen auf den Ordner
 * darüber lag außerhalb des erlaubten Bereichs). Gilt nur, wo der Wechsel ohnehin erlaubt ist (besitzer_erlaubt). */
/* art: 0 DELE, 1 RMD, 2 RMDA, 3 nur prüfen (wie RMDA ohne Löschen), 4 nur leeren (wie RMDA, der Ordner selbst bleibt) - 3 und 4 NEU 13.11.2026.
 * soll (darf NULL sein): so muss das Ziel aussehen (Gerät, Knoten - wie beim Prüfen gesehen); e_soll: so der Ordner darüber (Codex-Prüfung
 * 180 (1): ein Pfad allein reicht nicht, zwischen Prüfen und Löschen kann getauscht werden). leer: Inhalt weg, nur der Ordner selbst nicht
 * (fehlende Rechte im Ordner darüber) - ziel_dev/ino: das gelöschte Ziel (für das Entfernen danach ohne Wechsel). */
typedef struct { dz *d; arena *a; const char *v; int art; int fehler; const struct stat *soll; bool e_soll; dev_t e_dev; ino_t e_ino;
                 bool leer; dev_t ziel_dev; ino_t ziel_ino; int weg_n; bool art1_geprueft; } privat_weg_job;
static bool privat_weg_tu(void *c)
{
    privat_weg_job *j = c;
    bool frei = g_besitzer_frei; g_besitzer_frei = false;   /* schon als Besitzer: kein Wechsel im Wechsel */
    const char *bl = NULL; int r = -1;
    errno = 0;
    weg_kette kv;
    g_link_am_ende = j->art == 0;
    bool gesehen = weg_ansehen(j->d, j->a, j->v, &kv);
    g_link_am_ende = false;
    if (!gesehen) schritt("als Besitzer: Weg ansehen (%d Teile): %s", kv.n, strerror(errno));
    int pfd = gesehen ? eltern_oeffnen(j->d, j->a, j->v, &bl) : -1;
    if (gesehen && pfd < 0) schritt("als Besitzer: Weg öffnen: %s", strerror(errno));
    if (pfd >= 0) {
        struct stat st;
        if (fstatat(pfd, bl, &st, AT_SYMLINK_NOFOLLOW) != 0) { SCHRITT_FEHLER("als Besitzer: Ziel ansehen", bl); }
        else if (kv.n < 1 || st.st_dev != kv.dev[kv.n - 1] || st.st_ino != kv.ino[kv.n - 1]) {   /* inzwischen getauscht */
            schritt("als Besitzer: Ziel anders als auf dem Weg gesehen: Gerät %lu/%lu, Knoten %lu/%lu (%d Teile)", (unsigned long)(kv.n ? kv.dev[kv.n - 1] : 0), (unsigned long)st.st_dev,
                    (unsigned long)(kv.n ? kv.ino[kv.n - 1] : 0), (unsigned long)st.st_ino, kv.n);
            errno = EAGAIN; }
        else if (j->soll && (st.st_dev != j->soll->st_dev || st.st_ino != j->soll->st_ino)) {   /* Codex-Prüfung 180 (1) */
            schritt("als Besitzer: Ziel anders als beim Prüfen gesehen: Gerät %lu/%lu, Knoten %lu/%lu", (unsigned long)j->soll->st_dev, (unsigned long)st.st_dev,
                    (unsigned long)j->soll->st_ino, (unsigned long)st.st_ino);
            errno = EAGAIN; }
        else if (j->e_soll && (fstat(pfd, &st) != 0 || st.st_dev != j->e_dev || st.st_ino != j->e_ino)) {   /* Codex-Prüfung 180 (1) */
            schritt("als Besitzer: Ordner darüber anders als beim Prüfen gesehen");
            errno = EAGAIN; }
        else {
            j->ziel_dev = kv.dev[kv.n - 1]; j->ziel_ino = kv.ino[kv.n - 1];
            if (fstatat(pfd, bl, &st, AT_SYMLINK_NOFOLLOW) != 0) { SCHRITT_FEHLER("als Besitzer: Ziel ansehen", bl); }   /* st wieder das Ziel */
            else if (st.st_dev != j->ziel_dev || st.st_ino != j->ziel_ino) { schritt("als Besitzer: Ziel inzwischen getauscht"); errno = EAGAIN; }   /* Codex-Prüfung 181 (1) */
            else if (j->art == 2 || j->art == 3 || j->art == 4) {   /* Codex-Prüfung 173 (3): genau das gesehene Ziel öffnen und löschen - nicht erneut nach dem Namen suchen */
                int fd = openat(pfd, bl, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
                struct stat so;
                if (fd < 0) { SCHRITT_FEHLER("als Besitzer: Ziel öffnen", bl); }
                else if (fstat(fd, &so) != 0) { SCHRITT_FEHLER("als Besitzer: Ziel geöffnet, Angaben nicht lesbar", bl); close(fd); errno = EAGAIN; }   /* wie bisher: EAGAIN */
                else if (so.st_dev != kv.dev[kv.n - 1] || so.st_ino != kv.ino[kv.n - 1]) {
                    schritt("als Besitzer: Ziel geöffnet, aber anders als gesehen: Gerät %lu/%lu, Knoten %lu/%lu", (unsigned long)kv.dev[kv.n - 1], (unsigned long)so.st_dev,
                            (unsigned long)kv.ino[kv.n - 1], (unsigned long)so.st_ino);
                    close(fd); errno = EAGAIN; }
                else {
                    int ein = einbindung(pfd, fd);
                    if (ein != 0) { close(fd); errno = ein > 0 ? EXDEV : errno; SCHRITT_FEHLER("als Besitzer: Einbindung", bl); }
                    else {
                        arena *ba = arena_neu();
                        int b = baum_pruefen(fd, ba, 0, NULL);
                        g_weg_n = 0;
                        if (b == 0 && j->art != 3) b = baum_loeschen(fd, ba, 0, NULL);
                        j->weg_n = g_weg_n;
                        int e2 = errno; arena_frei(ba); close(fd); errno = e2;
                        struct stat sz;   /* Codex-Prüfung 181 (2): unmittelbar vor dem Entfernen nochmal: noch derselbe Ordner? */
                        if (b == 0 && j->art == 2 && fstatat(pfd, bl, &sz, AT_SYMLINK_NOFOLLOW) != 0) {   /* NEU 1.0.10 (PS5 12:25, Batman): echter Grund */
                            int fe = errno; SCHRITT_FEHLER("als Besitzer: geleerten Ordner ansehen", bl); b = -1; errno = fe ? fe : EIO; }
                        else if (b == 0 && j->art == 2 && (sz.st_dev != j->ziel_dev || sz.st_ino != j->ziel_ino)) {
                            schritt("als Besitzer: geleerter Ordner inzwischen getauscht: Gerät %lu/%lu, Knoten %lu/%lu", (unsigned long)j->ziel_dev, (unsigned long)sz.st_dev,
                                    (unsigned long)j->ziel_ino, (unsigned long)sz.st_ino);
                            b = -1; errno = EAGAIN; }
                        r = j->art == 3 || j->art == 4 ? b : b == 0 ? unlinkat(pfd, bl, AT_REMOVEDIR) : -1;   /* art 3: nur prüfen, 4: nur leeren */
                        if (j->art == 2 && b == 0 && r != 0) {
                            if (rechte_fehler(errno)) j->leer = true;   /* Inhalt weg - den leeren Ordner entfernt danach der Aufrufer ohne Wechsel */
                            SCHRITT_FEHLER("als Besitzer: leeren Ordner entfernen", bl);
                        }
                    }
                }
            }
            else if (j->art == 1) {
                j->art1_geprueft = true;   /* Codex-Prüfung 194: bis hier sind Weg, Ziel (soll) und Ordner darüber als Besitzer bestätigt */
                const char *tr = env(j->d, "ELTERN_ROOT");   /* Test (nur Testfassung): Ordner darüber gehört root - der Besitzer des Kinds darf nichts entfernen (Codex-Prüfung 193 (1)) */
                if (tr && !strcmp(tr, bl)) errno = EACCES;
                else if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) r = unlinkat(pfd, bl, AT_REMOVEDIR); else errno = ENOTDIR; }
            else { if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) r = unlinkat(pfd, bl, 0); else errno = EISDIR; }
        }
        int e = errno; close(pfd); errno = e;
    }
    j->fehler = r == 0 ? 0 : (errno ? errno : EIO);
    g_besitzer_frei = frei;
    errno = j->fehler;
    return r == 0;
}
/* 0 = gelöscht (art 3: geprüft, 4: geleert), -1 = nicht (errno). soll/eltern_fd (dürfen NULL/-1 sein): so sahen Ziel und Ordner darüber
 * beim Prüfen aus (Codex-Prüfung 180 (1)). */
static int privat_weg_x(dz *d, arena *a, const char *v, int art, const struct stat *soll, int eltern_fd)
{
    if (!zugriff_als_besitzer || zugriff_rechte_kaputt || !besitzer_erlaubt(v)) { errno = EACCES; return -1; }
    const char *sl = strrchr(v, '/');
    if (!sl || sl == v) { errno = EINVAL; return -1; }
    int pfd; const char *name, *ort; unsigned uid;
    g_link_am_ende = art == 0;
    g_soll_uid = soll && soll->st_uid != 0 ? (long)soll->st_uid : -1;
    bool bf = blockade_finden(d, a, v, &pfd, &name, &uid, &ort);
    g_soll_uid = -1;
    g_link_am_ende = false;
    if (!bf) { errno = EACCES; schritt("als Besitzer: keine Sperre auf dem Weg gefunden"); return -1; }
    privat_weg_job j = { d, a, v, art, -1, soll, false, 0, 0, false, 0, 0, 0 };   /* fehler -1: tu lief nicht (Wechsel nicht möglich) */
    struct stat es;
    if (eltern_fd >= 0 && fstat(eltern_fd, &es) == 0) { j.e_soll = true; j.e_dev = es.st_dev; j.e_ino = es.st_ino; }
    errno = 0;
    bool ok = false;
    /* Codex-Prüfung 180 (3)/(4): einen Eintrag (Datei, Verknüpfung, leeren Ordner) entfernen darf, wer den Ordner darüber besitzt - erst
     * als dieser, dann (Ordner mit Sticky-Bit) als Besitzer des Eintrags; wie bisher je Einzelschritt, jetzt in einem Durchgang */
    struct stat ps;
    if ((art == 0 || art == 1) && !strcmp(ort, v) && fstat(pfd, &ps) == 0 && ps.st_uid != 0 && ps.st_uid != uid) {
        unsigned u2 = (unsigned)ps.st_uid; const char *o2 = a_kopie_n(a, v, (size_t)(sl - v));
        if (besitzer_erlaubt(o2) && wechseln(pfd, name, u2, privat_weg_tu, &j)) { ok = true; uid = u2; ort = o2; }
        else if (j.fehler >= 0 && !rechte_fehler(j.fehler)) { int e = j.fehler; close(pfd); errno = e; return -1; }   /* gescheitert, aber nicht an Rechten */
        else { j.fehler = -1; }
    }
    if (!ok) ok = besitzer_erlaubt(ort) && wechseln(pfd, name, uid, privat_weg_tu, &j);
    g_art1_geprueft = j.art1_geprueft;
    if (!ok && j.fehler < 0) { schritt("als Besitzer: Wechsel auf Benutzer %u bei %s nicht möglich (%s)", uid, ort, errno ? strerror(errno) : "ohne Grund"); j.fehler = errno ? errno : EACCES; }
    int e = j.fehler ? j.fehler : errno;
    close(pfd);
    if (!ok && art == 2 && j.leer) {   /* Inhalt als Besitzer gelöscht, den leeren Ordner darf nur der Besitzer des Ordners darüber entfernen (Codex-Prüfung 180 (2)) */
        const char *bl2 = NULL; struct stat z;
        int p2 = eltern_oeffnen(d, a, v, &bl2);
        if (p2 >= 0) {
            /* NEU 1.0.10 (PS5 13:36): jeder Fehlschlag hier mit Schritt im Protokoll - nur Text, die Prüfungen sind unverändert */
            if (fstatat(p2, bl2, &z, AT_SYMLINK_NOFOLLOW) != 0) { e = errno ? errno : e; schritt_dazu("geleerten Ordner ansehen", bl2); }
            else if (!S_ISDIR(z.st_mode) || z.st_dev != j.ziel_dev || z.st_ino != j.ziel_ino) { e = errno ? errno : e; int e0 = errno; errno = EAGAIN; schritt_dazu("geleerter Ordner anders als gesehen", bl2); errno = e0; }
            else if (unlinkat(p2, bl2, AT_REMOVEDIR) == 0) ok = true;
            else { e = errno ? errno : e; schritt_dazu("geleerten Ordner entfernen", bl2); }
            close(p2);
        }
        else schritt_dazu("Ordner darüber öffnen für", v);
        if (!ok && rechte_fehler(e)) {   /* Codex-Prüfung 181 (3): Ordner darüber gehört einem anderen Benutzer - als dieser (art 1, nur derselbe Ordner) */
            struct stat zs; memset(&zs, 0, sizeof zs); zs.st_dev = j.ziel_dev; zs.st_ino = j.ziel_ino;
            if (privat_weg_x(d, a, v, 1, &zs, -1) == 0) { ok = true; uid = 0; }
            else { e = errno ? errno : e; schritt_dazu("als Besitzer des Ordners darüber entfernen", v); }
        }
        if (ok) g_schritt[0] = 0;
        if (ok && uid == 0) return 0;   /* art 1 hat sich selbst gemerkt */
    }
    if (!ok) { errno = e ? e : EACCES; return -1; }
    privat_merken(d, 'L', uid, ort, 1);
    if (art != 3 && !(art == 4 && j.weg_n == 0)) privat_merken(d, 'W', uid, g_ziel ? g_ziel : v, 1);   /* gezählt beim Ziel des Befehls; art 4 nur, wenn wirklich etwas entfernt wurde (Codex-Prüfung 182) */
    return 0;
}
/* Ergebnis eines Lösch-Schritts: r == 0 -> 250; an fehlenden Rechten gescheitert -> einmal privat_weg; sonst 550 mit dem Grund (bzw. text) */
/* Codex-Prüfung 185 (P1): soll/eltern_fd = das Ziel und der Ordner darüber, wie der Befehl sie gesehen hat (NULL/-1 nur, wenn das Ziel
 * nicht einmal ansehbar war) - der zweite Versuch als Besitzer löscht nur genau dieses Ziel, nie einen inzwischen eingetauschten Ordner */
static antwort weg_antwort(dz *d, arena *a, const char *v, int art, int r, const char *text, const struct stat *soll, int eltern_fd)
{
    if (r == 0) return A(a, 250, "250 OK");
    int e = errno;
    const char *zuerst = g_schritt[0] ? a_kopie(a, g_schritt) : NULL;   /* NEU 1.0.10 (13.11.2026): woran der erste Versuch scheiterte */
    /* Codex-Prüfung 197 (1): keine Angabe gemerkt (z. B. DELE/RMD direkt gescheitert) - dann der Schritt selbst mit Pfad und Grund */
    if (!zuerst) zuerst = VERB(a, art == 0 ? "Löschen: Datei entfernen " : art == 1 ? "Löschen: leeren Ordner entfernen " : art == 2 ? "Löschen: Ordner samt Inhalt entfernen " : "Löschen ",
                               v, ": ", strerror(e ? e : EIO));
    if (g_besitzer_frei && rechte_fehler(e)) {
        g_schritt[0] = 0;
        if (privat_weg_x(d, a, v, art, soll, eltern_fd) == 0) return A(a, 250, "250 OK");
        e = errno;
        const char *dann = g_schritt[0] ? a_kopie(a, g_schritt) : "-";
        return A(a, 550, VERB(a, text ? text : VERB(a, "550 ", strerror(e ? e : EIO)), " [Schritt: zuerst ", zuerst ? zuerst : "-", "; dann ", dann, "]"));
    }
    if (zuerst) return A(a, 550, VERB(a, text ? text : VERB(a, "550 ", strerror(e ? e : EIO)), " [Schritt: ", zuerst, "]"));
    return A(a, 550, text ? text : VERB(a, "550 ", strerror(e ? e : EIO)));
}
/* Codex-Prüfung 198: abgelehnt ohne Löschversuch (falsche Art oder nicht ansehbar) - mit Schritt, Pfad und Grund */
static antwort ohne_versuch(arena *a, const char *text, const char *was, const char *v, bool hat, int hat_e, const char *art_text)
{
    return A(a, 550, VERB(a, text, " [Schritt: ", was, " ", v, ": ", hat ? art_text : strerror(hat_e ? hat_e : EIO), "]"));
}
static antwort d_befehl_loeschen(dz *d, arena *a, const char *befehl, const char *pfad)
{
    g_schritt[0] = 0;   /* NEU 1.0.10: Schritt-Angabe gilt je Befehl */
    const char *v = norm(a, pfad), *r = echt(d, a, v);
    const char *bl = NULL;
    int pfd = eltern_oeffnen(d, a, v, &bl);           /* NEU 1.1 (S4): Weg ohne Verknüpfungen */
    int art_weg = strcmp(befehl, "DELE") == 0 ? 0 : strcmp(befehl, "RMD") == 0 ? 1 : strcmp(befehl, "RMDA") == 0 ? 2 : -1;
    /* NEU 1.0.10: schon der Weg ist versperrt (privater Ordner) - Löschbefehle einmal in einem Durchgang als Besitzer (privat_weg) */
    if (pfd < 0) {
        int e = errno;
        const char *fd0 = env(d, "FAILDEL"), *x0 = getenv("RMDA");
        bool test_aus = (fd0 && txt_beginnt(v, fd0)) || (art_weg == 2 && d->o.test_einspielungen && (!x0 || strcmp(x0, "1") != 0));
        if (art_weg < 0 || test_aus) return A(a, 550, VERB(a, "550 ", strerror(e)));
        errno = e;
        SCHRITT_FEHLER("Weg zum Ziel öffnen", v);   /* Codex-Prüfung 197 (2): z. B. Verknüpfung auf dem Weg */
        return weg_antwort(d, a, v, art_weg, -1, NULL, NULL, -1);
    }
    antwort erg = A(a, 502, "502 no");
    struct stat st;
    bool hat = bschritt(B_STAT, pfd, bl, 0, &st) == 0;   /* 1.0.9: bschritt (ohne Wechsel außer bei DELE/RMD/RMDA) */
    int hat_e = hat ? 0 : errno;                       /* NEU 1.0.10: warum nicht ansehbar (fehlende Rechte -> privat_weg) */
    if (strcmp(befehl, "SITE CHMOD 777") == 0) {
        if (!hat || (S_ISLNK(st.st_mode) && !da(r))) erg = A(a, 550, "550 no");
        else if (S_ISLNK(st.st_mode)) erg = A(a, 550, "550 Verknuepfung - nicht veraendert");   /* NEU 1.1: nie das Ziel ändern */
        else if (S_ISDIR(st.st_mode)) erg = ordner_chmod(a, pfd, bl, &st);
        else if (st.st_nlink > 1) erg = A(a, 550, "550 mehrfach verknuepft - nicht veraendert");   /* Codex-Prüfung 202 (5): die Rechte gälten auch für den anderen Namen */
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
        if (fake(d, v)) erg = baum_weg_at(pfd, bl, NULL) != 0 ? A(a, 550, VERB(a, "550 ", strerror(errno))) : A(a, 250, "250 OK");
        else if (hat && (S_ISLNK(st.st_mode) || S_ISREG(st.st_mode))) erg = weg_antwort(d, a, v, 0, bschritt(B_WEG, pfd, bl, 0, NULL), NULL, &st, pfd);
        else if (!hat && rechte_fehler(hat_e)) { errno = hat_e; erg = weg_antwort(d, a, v, 0, -1, "550 no", NULL, -1); }   /* NEU 1.0.10: nicht einmal ansehbar */
        else erg = ohne_versuch(a, "550 no", "Löschen: Datei entfernen", v, hat, hat_e, "ist keine Datei");   /* Codex-Prüfung 198 */
    }
    else if (strcmp(befehl, "RMD") == 0) {
        if (hat && S_ISDIR(st.st_mode)) erg = weg_antwort(d, a, v, 1, bschritt(B_WEG, pfd, bl, AT_REMOVEDIR, NULL), "550 Directory not empty", &st, pfd);
        else if (!hat && rechte_fehler(hat_e)) { errno = hat_e; erg = weg_antwort(d, a, v, 1, -1, "550 Directory not empty", NULL, -1); }   /* NEU 1.0.10 */
        else erg = ohne_versuch(a, "550 Directory not empty", "Löschen: leeren Ordner entfernen", v, hat, hat_e, "ist kein Ordner");   /* Codex-Prüfung 198 */
    }
    else if (strcmp(befehl, "RMDA") == 0) {
        const char *x = getenv("RMDA");
        if (d->o.test_einspielungen && (!x || strcmp(x, "1") != 0)) erg = A(a, 502, "502 not implemented");
        else if (!hat) { errno = hat_e; erg = rechte_fehler(hat_e) ? weg_antwort(d, a, v, 2, -1, "550 no", NULL, -1) : ohne_versuch(a, "550 no", "Ziel ansehen", v, false, hat_e, NULL); }   /* NEU 1.0.10; Schritt: Codex-Prüfung 198 */
        else erg = weg_antwort(d, a, v, 2, baum_weg_at(pfd, bl, v), NULL, &st, pfd);
    }
    close(pfd);
    return erg;
}
#endif

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
/* NEU 1.0.9: jede Frage hält die Lesesperre - ein Besitzer-Wechsel (Schreibsperre) fällt so nie mitten in andere Datei-Arbeit */
static antwort l_cwd(zugriff *z, arena *a, const char *p) { zugriff_wechsel_sperren(); antwort r = d_cwd(z, a, p); zugriff_wechsel_frei(); return r; }
static antwort l_liste(zugriff *z, arena *a, const char *p, kdliste *aus) { zugriff_wechsel_sperren(); antwort r = d_liste(z, a, p, aus); zugriff_wechsel_frei(); return r; }
static antwort l_lies(zugriff *z, arena *a, const char *p, const unsigned char **d, size_t *n) { zugriff_wechsel_sperren(); antwort r = d_lies(z, a, p, d, n); zugriff_wechsel_frei(); return r; }
static antwort l_schreibe(zugriff *z, arena *a, const char *p, const void *d, size_t n) { zugriff_wechsel_sperren(); antwort r = d_schreibe(z, a, p, d, n); zugriff_wechsel_frei(); return r; }
static antwort l_kopf(zugriff *z, arena *a, const char *p, size_t n, const unsigned char **d, size_t *len) { zugriff_wechsel_sperren(); antwort r = d_kopf(z, a, p, n, d, len); zugriff_wechsel_frei(); return r; }
static antwort l_enthaelt(zugriff *z, arena *a, const char *p, const void *m, size_t ml, bool *ja) { zugriff_wechsel_sperren(); antwort r = d_enthaelt(z, a, p, m, ml, ja); zugriff_wechsel_frei(); return r; }
static antwort l_stuecke(zugriff *z, arena *a, const char *p, size_t s, bool (*w)(void *c, const unsigned char *d, size_t n), void *c) { zugriff_wechsel_sperren(); antwort r = d_stuecke(z, a, p, s, w, c); zugriff_wechsel_frei(); return r; }
static antwort l_lies_ab(zugriff *z, arena *a, const char *p, long long ab, size_t n, unsigned char *ziel, size_t *len) { zugriff_wechsel_sperren(); antwort r = d_lies_ab(z, a, p, ab, n, ziel, len); zugriff_wechsel_frei(); return r; }
static antwort l_befehl(zugriff *z, arena *a, const char *b, const char *p) { zugriff_wechsel_sperren(); antwort r = d_befehl(z, a, b, p); zugriff_wechsel_frei(); return r; }

zugriff *zugriff_datei(const datei_optionen *o)
{
    zugriff *z = calloc(1, sizeof *z);
    dz *d = calloc(1, sizeof *d);
    d->o = *o;
    d->o.wurzel = strdup(o->wurzel ? o->wurzel : "");
    d->o.test_hier = o->test_hier ? strdup(o->test_hier) : NULL;
    pthread_mutex_init(&d->pv_m, NULL); d->pv_a = arena_neu();   /* NEU 1.0.9: private Orte */
    z->art = "datei";
    /* NEU 1.0.9: jede Frage über l_... (Lesesperre gegen den Besitzer-Wechsel) */
    z->cwd = l_cwd; z->liste = l_liste; z->lies = l_lies; z->kopf = l_kopf; z->enthaelt = l_enthaelt; z->schreibe = l_schreibe; z->befehl = l_befehl;
    z->stuecke = l_stuecke;
    z->lies_ab = l_lies_ab;   /* NEU 1.0.8 */
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
