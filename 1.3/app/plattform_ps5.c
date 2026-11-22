/* PS5 Cleanup - Oberfläche am Fernseher
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * plattform_ps5.c - Konsole abfragen (nur lesen), Sprache, eigener Ort, Zugriff.
 * Modell nach PS5Upload (hw_info.c) / drakmor/ps5-hwinfo (GPL-3.0),
 * Laufwerks-Kopplung wie ps5info.c aus 1.0.5 (nach earthonion/ddd_status, GPL-3.0):
 * ein lesendes ioctl auf /dev/ddd. Die Kopplung wird nie verändert.
 * /system_ex: nur der eigene Startordner PSCL00002 wird bei der Selbstentfernung gelöscht (start/startordner.h,
 * Freigabe da_Chris86 29.10.2026 21:23); sonst wird /system_ex nicht verändert (gelesen nur für die Einbindungs-Prüfung:
 * statfs, Ordnerliste von /system_ex/app, Liste der Einbindungen).
 */
#include "plattform.h"
#include "../kern/kern.h"   /* NEU 1.0.7: kern_geist_regel (Geisterkacheln) */
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>   /* NEU 1.0.10: strncasecmp (Playden-Antwort) */
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <vm/vm_param.h>
#include <unistd.h>
#include <ps5/kernel.h>
#include <ps5/klog.h>
#include "../../Quellcode/ps5info_antwort.h"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "../start/startordner.h"   /* NEU 1.0.6: eigener Startordner PSCL00002 (Selbstentfernung) */
#include "../db/db_lesen.h"     /* NEU 1.0.7: Reste ohne Kachel - Kennung in app.db? (nur lesen) */
#ifdef KERN_LOESCHEN
#include "spielstand_db.h"          /* NEU 1.0.7: auch appinfo_zusatz_reparieren (kaputte Zusatzinhalt-Angaben) */
#endif

int sceKernelGetHwModelName(char *out);
typedef struct { uint64_t size; char text[0x1C]; uint32_t version; } SwVersion;
int sceKernelGetProsperoSystemSwVersion(SwVersion *v);
int sceSystemServiceHideSplashScreen(void);
int sceSystemServiceLoadExec(const char *pfad, const char **argv);
int sceSystemServiceParamGetInt(int id, int *wert);
#define SCE_SYSTEM_SERVICE_PARAM_ID_LANG 1
#define SPRACHE_DEUTSCH 4

#define DDD_STATUS_IOCTL 0xC0284406UL
typedef struct { uint8_t vorlauf[4]; int32_t ein_laenge; uint8_t *ein_puffer; int32_t aus_laenge; uint8_t *aus_puffer; uint32_t status; uint32_t nachlauf; } ddd_abfrage_t;

/* NEU 1.0.7 (da_Chris86 31.10.2026 19:28): Protokoll-Datei auf dem USB-Stick - der Hintergrundteil hängt bei der Selbstentfernung
 * seine Schlusszeilen dort an (Pfad geht mit dem Auftrag mit, siehe selbst_uebergeben) */
extern int (*kern_ps4_sony_eingetragen)(const char *id);   /* NEU 1.0.7: kern/kern_ps4.c */
static char prot_stick[200];
void (*plattform_meldung)(const char *t) = NULL;   /* NEU 1.0.7: Zeile ins Protokoll der Oberfläche (setzt app.c) */
void plattform_protokoll_stick_setzen(const char *pfad) { snprintf(prot_stick, sizeof prot_stick, "%s", pfad ? pfad : ""); }

/* NEU 1.0.7 (PS5-Testlauf 01.11.2026 20:13, da_Chris86 22:16 "die Kacheln sollen weg mit dem Tool"): Fehlt /system_ex/app/<id>,
 * meldet statfs auf der PS5 NICHT "gibt es nicht" (ENOENT) - alle 15 Apps/PS4-Spiele ohne Ordner dort blieben deshalb "unverändert",
 * alle 17 gelöschten Kacheln hatten einen Ordner dort. Dann zusätzlich nur LESEN: steht <id> nicht in der Ordnerliste von
 * /system_ex/app UND ist laut Liste aller Einbindungen (getmntinfo) an diesem Pfad nichts eingehängt -> 0 (nichts eingebunden).
 * Sonst bleibt es unklar (-1) und der genaue Grund (Antworten der PS5) kommt ins Protokoll. */
static int name_in_ordner(const char *ordner, const char *name, int *e)   /* 1 = steht drin, 0 = nicht, -1 = Liste nicht lesbar */
{
    *e = 0;
    DIR *d = opendir(ordner);
    if (!d) { *e = errno; return -1; }
    int r = 0;
    struct dirent *de;
    errno = 0;
    while (r == 0 && (de = readdir(d)) != NULL) if (!strcmp(de->d_name, name)) r = 1;
    if (r == 0 && errno) { *e = errno; r = -1; }
    closedir(d);
    return r;
}
static int pfad_eingehaengt(const char *p, int *e)   /* 1 = an genau diesem Pfad ist etwas eingehängt, 0 = nein, -1 = nicht lesbar */
{
    *e = 0;
    struct statfs *m = NULL;
    errno = 0;
    int n = getmntinfo(&m, MNT_NOWAIT);
    if (n <= 0 || !m) { *e = errno; return -1; }
    for (int i = 0; i < n; i++) if (!strcmp(m[i].f_mntonname, p)) return 1;
    return 0;
}
static int einbindung_ohne_statfs(const char *id, const char *p, int e1)
{
    if (e1 == ENOENT) return 0;
    int e2 = 0, e3 = 0;
    int liste = name_in_ordner("/system_ex/app", id, &e2);
    int ein = liste == 0 ? pfad_eingehaengt(p, &e3) : -1;
    if (liste == 0 && ein == 0) {
        klog_printf("PS5 Cleanup: Kachel %s: %s gibt es nicht (statfs: %s), nichts eingehängt\n", id, p, strerror(e1));
        return 0;
    }
    static char zuletzt[16];
    if (strcmp(zuletzt, id)) {   /* je Kennung nur einmal ins Protokoll */
        snprintf(zuletzt, sizeof zuletzt, "%s", id);
        char t[320];
        snprintf(t, sizeof t, "Kachel %s: Einbindung nicht feststellbar - statfs: %s, Ordnerliste: %s, Einbindungsliste: %s", id, strerror(e1),
                 liste == 1 ? "Ordner da" : liste == 0 ? "nicht drin" : strerror(e2),
                 ein == 1 ? "eingehängt" : ein == 0 ? "nichts" : liste == 0 ? strerror(e3) : "nicht gefragt");
        if (plattform_meldung) plattform_meldung(t); else klog_printf("PS5 Cleanup: %s\n", t);
    }
    return -1;
}
/* NEU 1.0.7 (da_Chris86 31.10.2026 21:50, Punkt 4): ist /system_ex/app/<id> ein eigener Einhängepunkt? Nur lesen (statfs).
 * 1 = ja (fstyp/quelle), 0 = nein (kein Ordner oder kein Einhängepunkt), -1 = nicht feststellbar. In allen Fassungen. */
static int einbindung_abfragen(arena *a, const char *id, const char **fstyp, const char **quelle)
{
    *fstyp = NULL; *quelle = NULL;
    if (!id || strlen(id) != 9) return -1;
    char p[64]; snprintf(p, sizeof p, "/system_ex/app/%s", id);
    struct statfs s;
    if (statfs(p, &s) != 0) return einbindung_ohne_statfs(id, p, errno);   /* NEU 1.0.7 (Testlauf 01.11.2026) */
    if (strcmp(s.f_mntonname, p) != 0) return 0;
    *fstyp = a_kopie(a, s.f_fstypename); *quelle = a_kopie(a, s.f_mntfromname);
    return 1;
}

static sigjmp_buf g_jb;
static void bei_fehler(int sig) { (void)sig; siglongjmp(g_jb, 1); }

/* NEU 1.0.6 (da_Chris86 28.10.2026): Kacheln über Sonys Deinstallations-Dienst entfernen (wie das PS5-Menü).
 * Vorbild (nur Vorgehen): ps5-payload-dev/ftpsrv install-ps5.c, ShadowMountPlus docs/api.md - Sony nimmt den Auftrag an
 * und löscht danach im Hintergrund (der Kern wartet, bis der App-Ordner weg ist).
 * Nie für den eigenen Startordner PSCL00002: darin läuft die Oberfläche gerade selbst (start/start_hbldr.c). */
/* NEU 1.3 (Rückmeldungen 66/68, 20.11.2026): Antworttexte in der Sprache der Oberfläche - bis 1.2 standen sie auch im englischen
 * Protokoll auf Deutsch. Entscheidungen hängen nie am Text (Merkmale ANTW_...); die Rückmelde-Kürzel erkennt der Kern in beiden
 * Sprachen (kern_loeschen.c GRUENDE). */
#define SA(de, en) (SP_EN ? (en) : (de))
static antwort sony_antwort(arena *a, int code, const char *t) { antwort r = { code, a_kopie(a, t), 0 }; return r; }
static antwort sony_antwort_art(arena *a, int code, const char *t, unsigned art) { antwort r = sony_antwort(a, code, t); r.art = art; return r; }   /* NEU 1.1 (zugriff.h ANTW_...) */
#ifdef KERN_LOESCHEN
int sceAppInstUtilInitialize(void);
int sceAppInstUtilAppUnInstall(const char *title_id);
int sceAppInstUtilAppExists(const char *title_id, int *exists);
/* NEU 1.0.10 (22:40): Zusatzinhalt austragen - Name = dir_name in addcont.db (ps4mount src/main.c ac_uninstall: erst der Name, dann die
 * content_id; Eintrag und Dateien weg, auf der PS5 bestätigt). Der PKG Manager ruft sie mit nur einem Wert auf - das ist falsch. */
int sceAppInstUtilAppUnInstallAddcont(const char *title_id, const char *addcont_name);
int sceAppInstUtilAppUnInstallPat(const char *title_id);   /* NEU 1.1: Update eines Spiels (wie ps5upload register.c uninstall_patch) */
/* NEU 1.0.6 (da_Chris86 29.10.2026 21:25): Sonys Deinstallation mit der geprüften Kennung, Abschluss abwarten, danach prüfen,
 * ob der Menüeintrag wirklich weg ist (sceAppInstUtilAppExists). Der Aufruf läuft in einem eigenen Faden mit Zeitgrenze:
 * Beim Test 29.10.2026 19:32 (Spectrum Library SLIB00001, ohne App-Ordner) kam Sonys Dienst nie zurück - dann darf die
 * Oberfläche nicht hängen bleiben. Antwort 200 nur, wenn der Eintrag nachweislich ausgetragen ist. */
/* Codex-Hinweis 29.10.2026 22:11: JEDER Sony-Aufruf (Initialisierung, Statusabfrage, Deinstallation) läuft in einem eigenen
 * Faden mit Zeitgrenze - auch Initialisierung und Statusabfrage können unbegrenzt hängen. Hing einmal ein Aufruf, wird Sonys
 * Dienst in dieser Sitzung nicht mehr benutzt (sonst stapeln sich hängende Fäden). */
typedef struct { int art; char id[16]; char zweit[64]; volatile int fertig; int e; int da; } sony_auftrag;   /* art: 0 Init, 1 eingetragen?, 2 Deinstallation, 3 Zusatzinhalt (zweit = Name), 4 Update (NEU 1.1) */
/* NEU 1.1 (da_Chris86 15.11.2026 07:21 "Sonys eigener Dienst muss klappen"; Vorbild ps5upload payload/src/register.c unregister_title und
 * include/authid.h, GPL-3.0): Sonys Deinstallation läuft mit der Kennung von Sonys eigener Oberfläche (ShellCore) - wie das PS5-Menü, das
 * sie aus ShellCore heraus aufruft. Nur für genau diesen Aufruf; danach wird die eigene Kennung zurückgestellt und nachgeprüft (bis zu 3 Mal).
 * Bis 1.0.10 lief der Aufruf mit der Kennung, die die Oberfläche gerade hatte. */
#define SONY_SHELLCORE_AUTHID 0x3800000000000010ULL
static uint64_t sony_kennung_vorher(void)
{
    pid_t pid = getpid();
    uint64_t vor = kernel_get_ucred_authid(pid);
    if (vor != 0 && vor != SONY_SHELLCORE_AUTHID) kernel_set_ucred_authid(pid, SONY_SHELLCORE_AUTHID);   /* 0 = nicht lesbar: nichts ändern (wie ps5upload) */
    return vor;
}
static bool sony_kennung_zurueck(uint64_t vor)   /* true = die eigene Kennung gilt wieder */
{
    pid_t pid = getpid();
    if (vor == 0) return true;
    for (int i = 0; i < 3 && kernel_get_ucred_authid(pid) != vor; i++) kernel_set_ucred_authid(pid, vor);
    if (kernel_get_ucred_authid(pid) == vor) return true;
    klog_printf("PS5 Cleanup: eigene Kennung nach Sonys Deinstallation nicht zurückgestellt (0x%016llx statt 0x%016llx)\n",
                (unsigned long long)kernel_get_ucred_authid(pid), (unsigned long long)vor);
    return false;
}
static void *sony_faden(void *x)
{
    sony_auftrag *a = x;
    if (a->art == 0) a->e = sceAppInstUtilInitialize();
    else if (a->art == 1) { int d = -1; a->e = sceAppInstUtilAppExists(a->id, &d); a->da = d; }
    else a->e = a->art == 3 ? sceAppInstUtilAppUnInstallAddcont(a->id, a->zweit) : a->art == 4 ? sceAppInstUtilAppUnInstallPat(a->id) : sceAppInstUtilAppUnInstall(a->id);   /* Kennung: sony_rufe2 */
    a->fertig = 1;
    return NULL;
}
static void sony_warte(int ms) { struct timespec z = { ms / 1000, (long)(ms % 1000) * 1000000L }; nanosleep(&z, NULL); }
static int sony_gestoert = 0;
/* 1 = fertig (*e, *da gesetzt), 0 = keine Antwort in ms (der Faden bleibt zurück, sein Auftrag wird nicht freigegeben) */
static int sony_rufe2(int art, const char *id, const char *zweit, int ms, int *e, int *da)
{
    if (sony_gestoert) return 0;
    sony_auftrag *au = calloc(1, sizeof *au);
    if (!au) { *e = -1; return 1; }
    au->art = art; snprintf(au->id, sizeof au->id, "%s", id ? id : ""); snprintf(au->zweit, sizeof au->zweit, "%s", zweit ? zweit : "");
    /* Codex-Prüfung 317 (P1/P2): die ShellCore-Kennung setzt und stellt der WARTENDE Faden zurück - auch wenn Sonys Aufruf hängt, gilt nach
     * der Zeitgrenze wieder die eigene Kennung. Klappt das Zurückstellen nicht: zugriff_rechte_kaputt (ab da verändert PS5 Cleanup nichts mehr,
     * der Kern hält mit Sicherheitsstopp an) und der Aufruf gilt als nicht beantwortet - nie "200 OK" mit fremder Kennung. */
    bool tausch = art == 2 || art == 3 || art == 4;
    uint64_t vor = tausch ? sony_kennung_vorher() : 0;
    pthread_t th;
    if (pthread_create(&th, NULL, sony_faden, au)) { free(au); if (tausch && !sony_kennung_zurueck(vor)) { zugriff_rechte_kaputt = true; sony_gestoert = 1; return 0; } *e = -1; return 1; }
    pthread_detach(th);
    for (int i = 0; i < ms / 100 && !au->fertig; i++) sony_warte(100);
    if (tausch && !sony_kennung_zurueck(vor)) { zugriff_rechte_kaputt = true; sony_gestoert = 1; if (au->fertig) free(au); return 0; }
    if (!au->fertig) { sony_gestoert = 1; klog_printf("PS5 Cleanup: Sony-Aufruf %d (%s) - keine Antwort nach %d ms\n", art, au->id, ms); return 0; }
    *e = au->e; if (da) *da = au->da; free(au);
    return 1;
}
static int sony_rufe(int art, const char *id, int ms, int *e, int *da) { return sony_rufe2(art, id, NULL, ms, e, da); }
/* 1 = eingetragen, 0 = nicht eingetragen, -1 = nicht prüfbar, -2 = keine Antwort */
/* PS5-Test 29.10.2026 22:29 (FAKE00000): sceAppInstUtilAppExists schreibt nur EIN Byte (vorher -255 = ...FF01, nachher -256 =
 * ...FF00 bei Vorbelegung -1) - ausgewertet wird darum nur das unterste Byte; alles außer 0/1 gilt als "nicht prüfbar" */
static int sony_eingetragen(const char *id) { int e = 0, d = -1; if (!sony_rufe(1, id, 5000, &e, &d)) return -2; if (e) return -1; int b = d & 0xFF; return b == 1 ? 1 : (b == 0 ? 0 : -1); }
/* NEU 1.0.7 (da_Chris86 31.10.2026 19:56, Punkt 3): jeder Schritt der Deinstallation auch ins Protokoll der Oberfläche */
static void sony_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* NEU 1.3 (Meldung #30): ja/nein/nicht prüfbar in der Sprache der Oberfläche */
static const char *jn(int v) { return v == 1 ? (SP_EN ? "yes" : "ja") : v == 0 ? (SP_EN ? "no" : "nein") : (SP_EN ? "not checkable" : "nicht prüfbar"); }
static void sony_log(const char *fmt, ...)
{
    char t[1600];   /* 1.3: 400 -> 1600 (Meldung #87: Inhalt der Rest-Ordner ganz ins Protokoll) */ va_list v; va_start(v, fmt); vsnprintf(t, sizeof t, fmt, v); va_end(v);
    if (plattform_meldung) plattform_meldung(t);   /* geht über das Protokoll auch ins Kernel-Protokoll */
    else klog_printf("PS5 Cleanup: %s\n", t);
}
/* NEU 1.0.7 (da_Chris86 01.11.2026 23:19): der Spielordner, den die Kachel selbst nennt - erste Zeile von /user/app/<ID>/mount_img.lnk
 * bzw. mount.lnk, die nicht auf /mnt/shadowmnt zeigt (wie kern_suche.c sm_quelle). Nur lesen. NULL = keiner. */
static const char *eigene_spielquelle(arena *a, const char *id)
{
    const char *lnk[2] = { "mount_img.lnk", "mount.lnk" };
    for (int i = 0; i < 2; i++) {
        char p[96]; snprintf(p, sizeof p, "/user/app/%s/%s", id, lnk[i]);
        FILE *f = fopen(p, "r");
        if (!f) continue;
        char z[1024]; const char *r = NULL;
        while (!r && fgets(z, sizeof z, f)) {
            size_t l = strlen(z); while (l && (z[l - 1] == '\n' || z[l - 1] == '\r' || z[l - 1] == ' ')) z[--l] = 0;
            const char *s = z; while (*s == ' ') s++;
            if (!*s || !strncmp(s, "/mnt/shadowmnt", 14)) continue;
            r = a_kopie(a, s);
        }
        fclose(f);
        if (r) return r;
    }
    /* NEU 1.3 (20.11.2026, Rückmeldung #70): Spiel im Kraken-Aufbau (Kraken Mount, TITAN CORE) - sein Zwischenspeicher
     * /data/<Programm>/caches/<Kennung> mit Stempel .done ("kind" pkg/folder, "source"). Genau einer, sonst keiner (nicht raten). */
    const char *treffer = NULL; int anz = 0;
    DIR *dd = opendir("/data");
    if (!dd) return NULL;
    struct dirent *de;
    while ((de = readdir(dd)) != NULL && anz < 2) {
        if (de->d_name[0] == '.' || strlen(de->d_name) > 64) continue;
        char st[200]; snprintf(st, sizeof st, "/data/%s/caches/%s/.done", de->d_name, id);
        struct stat s;
        if (lstat(st, &s) != 0 || !S_ISREG(s.st_mode) || s.st_size <= 0 || s.st_size > 65536) continue;
        FILE *f = fopen(st, "r");
        if (!f) continue;
        char b[4096]; size_t n = fread(b, 1, sizeof b - 1, f); b[n] = 0; fclose(f);
        if (!strstr(b, "\"source\"") || !strstr(b, "\"kind\"") || (!strstr(b, "\"pkg\"") && !strstr(b, "\"folder\""))) continue;
        anz++; treffer = a_fmt(a, "/data/%s/caches/%s", de->d_name, id);
    }
    closedir(dd);
    return anz == 1 ? treffer : NULL;
}
/* NEU 1.0.7 (Punkt 3, eingeschränkt am 31.10.2026 21:50 Punkt 4): für PSUP00001 mit der nullfs-Einbindung von ps5upload
 * (/data/ps5upload/tile/PSUP00001 -> /system_ex/app/PSUP00001, Vorbild ps5upload register.c unregister_title, GPL-3.0) und
 * NEU 01.11.2026 23:19 für die Einbindung des eigenen Spielordners der Kachel (kstuff, Ordner-Spiele auf der M.2).
 * Vor dem Lösen wird die Regel erneut geprüft (kern_geist_regel) - jede andere Einbindung bleibt unangetastet.
 * unmount löst nur die Ansicht /system_ex/app/<ID> - der Spielordner selbst wird nicht angefasst.
 * 1 = gelöst, 0 = keine Einbindung (mehr), -1 = nicht gelöst / nicht erlaubt. */
static int titel_laeuft(const char *id);   /* weiter unten (Codex-Pruefung 163): ist die Sandbox des Titels eingehaengt? */
static int p4m_einbindungen_loesen(const char *id);   /* NEU 1.3, weiter unten */
/* Codex-Pruefung 277: gesetzt genau dort, wo wirklich etwas veraendert wurde */
static bool sony_g_loesen, sony_g_rep, sony_g_dateien;
/* NEU 1.3 (PS5-Test 21.11.2026 13:48): Einträge im App-Ordner zählen (nur lesen) - lehnt Sony ab, nachdem es schon Dateien gelöscht hat,
 * darf nie "unverändert, es wurde nichts gelöscht" dastehen. -1 = nicht lesbar */
/* Codex-Prüfung 528 (3): alle Einträge darunter (nicht nur die erste Ebene) - löscht Sony z. B. nur sce_sys/param.sfo und lehnt dann ab, bleibt
 * die Zahl der ersten Ebene gleich. Verknüpfungen werden nicht verfolgt; höchstens 8 Ebenen und APP_ZAEHL_MAX Einträge (dann zählt die Grenze). */
#define APP_ZAEHL_MAX 200000
static void app_zaehlen(const char *p, int tiefe, int *n)
{
    DIR *d = opendir(p); if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && *n < APP_ZAEHL_MAX) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        (*n)++;
        if (tiefe >= 8) continue;
        char q[1024]; int l = snprintf(q, sizeof q, "%s/%s", p, e->d_name);
        if (l <= 0 || (size_t)l >= sizeof q) continue;
        struct stat st;
        if (lstat(q, &st) == 0 && S_ISDIR(st.st_mode)) app_zaehlen(q, tiefe + 1, n);
    }
    closedir(d);
}
static int app_ordner_eintraege(const char *id)
{
    char p[48]; snprintf(p, sizeof p, "/user/app/%s", id);
    DIR *d = opendir(p); if (!d) return -1;
    closedir(d);
    int n = 0; app_zaehlen(p, 1, &n); return n;
}
static int sony_einbindung_loesen(arena *a, const char *id)
{
    const char *fs = NULL, *q = NULL, *g = "";
    int m = einbindung_abfragen(a, id, &fs, &q);
    if (m == 0) return 0;
    int rg = kern_geist_regel(id, m, fs, q, eigene_spielquelle(a, id), &g);
    if (rg != GEIST_PS5UPLOAD && rg != GEIST_EIGENES_SPIEL && rg != GEIST_HINTERLASSEN) { sony_log(SP_EN ? "Tile %s: mount %s %s is neither from ps5upload nor its own game folder - left alone" : "Kachel %s: Einbindung %s %s weder von ps5upload noch der eigene Spielordner - nicht angefasst", id, fs ? fs : "?", q ? q : "?"); return -1; }
    char p[64]; snprintf(p, sizeof p, "/system_ex/app/%s", id);
    if (rg == GEIST_HINTERLASSEN) {   /* NEU 1.0.10 (14.11.2026 14:52): nie unter einem laufenden Spiel weg */
        int lf = titel_laeuft(id);
        if (lf != 0) { sony_log(SP_EN ? "Tile %s: %s - mount left alone" : "Kachel %s: %s - Einbindung nicht angefasst", id, lf > 0 ? (SP_EN ? "the game is running" : "das Spiel laeuft gerade") : (SP_EN ? "could not tell whether the game is running" : "ob das Spiel laeuft, liess sich nicht feststellen")); return -1; }
    }
    int r = unmount(p, 0);
    if (r == 0) sony_g_loesen = true;   /* Codex-Pruefung 277: jedes gelungene Aushaengen zaehlt, auch vor einem Folgeauftrag */
    int e = errno;
    const char *fs2 = NULL, *q2 = NULL;
    bool weg = einbindung_abfragen(a, id, &fs2, &q2) == 0;
    if (weg) sony_log(SP_EN ? "Tile %s: mount %s %s -> %s released before the uninstall" : "Kachel %s: Einbindung %s %s -> %s vor der Deinstallation gelöst", id, rg == GEIST_PS5UPLOAD ? (SP_EN ? "of ps5upload" : "von ps5upload") : rg == GEIST_HINTERLASSEN ? (SP_EN ? "of a closed helper program" : "eines beendeten Hilfsprogramms") : (SP_EN ? "of its own game folder" : "des eigenen Spielordners"), q, p);
    else sony_log(SP_EN ? "Tile %s: mount %s -> %s could not be released (%s)" : "Kachel %s: Einbindung %s -> %s ließ sich nicht lösen (%s)", id, q, p, r ? strerror(e) : (SP_EN ? "still mounted" : "noch eingehängt"));
    return weg ? 1 : -1;
}
/* NEU 1.0.7 (da_Chris86 01.11.2026 22:03 "löschen, wenn normale Sony- oder Kachellöschungen nicht funktionieren"; PS5-Test
 * start/test_rdr2b.c 22:08, Red Dead Redemption 2 mit umgebautem Patch): kaputte Zusatzinhalt-Angaben in appinfo.db VOR Sonys
 * Deinstallation reparieren - sonst löscht Sony die Spieldaten und bricht danach ab (0x80B21B02, im PS5-Menü stürzt ShellCore ab),
 * die Kachel bleibt. Ohne Sicherung (da_Chris86 02.11.2026 15:44: auf dem USB-Stick nur das Protokoll) - geändert wird nur nach
 * Prüfung, alles oder nichts (app/spielstand_db.c).
 * 0 = nichts kaputt bzw. repariert (*z Werte), -1 = kaputt und nicht repariert (nichts geändert), -2 = nicht prüfbar */
static int zusatz_reparieren(const char *id, int *z, char *grund, size_t gl)
{
    char db[160]; snprintf(db, sizeof db, "%s/appinfo.db", plattform_db_ordner());
    return appinfo_zusatz_reparieren(db, id, z, grund, gl);
}
/* Sonys Dienst einmal je Sitzung vorbereiten: 1 = bereit, 0 = keine Antwort (10 s), sonst Fehlercode in *e */
static int sony_bereit_wert = 0;
static int sony_bereit(int *e)
{
    *e = 0;
    if (sony_bereit_wert) return 1;
    if (!sony_rufe(0, NULL, 10000, e, NULL)) return 0;
    if (*e) return -1;
    sony_bereit_wert = 1;
    return 1;
}
/* NEU 1.0.7 (Punkt 1): fürs PS4-Inventar nur nachsehen, ob Sony die Kennung im Menü führt (sceAppInstUtilAppExists, nur lesen).
 * 1 / 0 / -1 = nicht prüfbar */
static int sony_ps4_eingetragen(const char *id)
{
    int e = 0;
    if (sony_gestoert || sony_bereit(&e) != 1) return -1;
    int r = sony_eingetragen(id);
    return r == 1 || r == 0 ? r : -1;
}
/* (bis 07.11.2026 hier ps5upload_ordner_da - entfällt, siehe "mehrfach" in sony_deinstallieren) */
/* NEU 1.0.10 (da_Chris86 10.11.2026 18:15, Punkt 3): nach einem gescheiterten Sony-Auftrag ins Protokoll, ob der Eintrag noch da ist - im
 * PS5-Menü (Sony) und in Sonys Liste (app.db). Nur lesen. */
static void sony_nachher(arena *a, const char *id)
{
    int m = sony_eingetragen(id);
    const char *g = "";
    int d = db_kennung_in_appdb(a, plattform_db_ordner(), id, &g);
    sony_log(SP_EN ? "Tile %s after the attempt: in the PS5 menu %s, in Sony's list (app.db) %s%s%s" : "Kachel %s nach dem Versuch: im PS5-Menü %s, in Sonys Liste (app.db) %s%s%s", id, jn(m),
             jn(d), d < 0 && g[0] ? " - " : "", d < 0 ? g : "");
}
/* da_Chris86 14.11.2026 15:51: nie "unveraendert" melden, wenn schon etwas veraendert wurde. Die Funktion merkt sich, ob sie die
 * Einbindung der Kachel geloest oder Sonys Liste repariert hat - scheitert der Auftrag danach, steht das im Grund (der Kern meldet dann TEILWEISE). */
static antwort sony_deinstallieren_innen(arena *a, const char *id, bool nur_pruefen);
static antwort sony_deinstallieren(arena *a, const char *id, bool nur_pruefen)
{
    sony_g_loesen = sony_g_rep = sony_g_dateien = false;
    antwort r = sony_deinstallieren_innen(a, id, nur_pruefen);
    if (sony_g_dateien && r.code != 200 && r.code != 226)   /* NEU 1.3: Sony hat vor der Ablehnung schon Dateien gelöscht */
        return sony_antwort_art(a, r.code == 451 ? 550 : r.code, a_fmt(a, SA("%s - bereits geändert: Sony hat dabei schon Dateien des Spiels gelöscht%s", "%s - already changed: Sony already deleted files of the game%s"), r.text ? r.text : "",
                                sony_g_loesen ? SA(", eine Einbindung wurde gelöst", ", a mount was released") : ""), r.art | ANTW_VERAENDERT);
    if ((sony_g_loesen || sony_g_rep) && r.code != 200 && r.code != 226)
        return sony_antwort_art(a, r.code == 451 ? 550 : r.code, a_fmt(a, SA("%s - bereits geändert: %s%s%s", "%s - already changed: %s%s%s"), r.text ? r.text : "",
                            sony_g_loesen ? SA("die Verknüpfung der Kachel im Systemordner wurde gelöst", "the link of the tile in the system folder was released") : "", sony_g_loesen && sony_g_rep ? SA(" und ", " and ") : "",
                            sony_g_rep ? SA("ein kaputter Eintrag in Sonys Liste wurde repariert", "a broken entry in Sony's list was repaired") : ""), r.art | ANTW_VERAENDERT);
    return r;
}
/* NEU 1.3 (da_Chris86 21.11.2026, Meldung #87 "Protokoll 87 muss lösbar sein für den User"): Reste eines Spiels, dessen Dateien an Sony
 * vorbei gelöscht wurden (z. B. mit einem Dateimanager oder per FTP) - Sony führt die Kennung nicht mehr im Menü, ihre Zeilen stehen aber noch
 * in Sonys Datenbank, die Ordner sind leer. Nur lesen. */
/* 1 = Ordner fehlt oder ist leer, 0 = es liegt etwas darin, -1 = nicht prüfbar */
static int rest_ordner_leer(const char *p)
{
    DIR *d = opendir(p);
    if (!d) return errno == ENOENT ? 1 : -1;
    struct dirent *e; int leer = 1;
    errno = 0;
    while ((e = readdir(d)) != NULL) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue; leer = 0; break; }
    if (leer && errno != 0) leer = -1;   /* Codex-Prüfung 459: ein Lesefehler ist nicht "leer" */
    closedir(d);
    return leer;
}
/* 1 = an keinem Ort, an dem ein Spiel liegen kann, ist etwas für die Kennung (Ordner fehlen oder sind leer); sonst *wo = der Ordner */
static int rest_kein_spiel(arena *a, const char *id, const char **wo)
{
    static const char *const ORTE[] = { "/user/app", "/mnt/ext0/user/app", "/mnt/ext1/user/app", "/system_ex/app", NULL };
    *wo = "";
    for (int i = 0; ORTE[i]; i++) {
        const char *p = a_fmt(a, "%s/%s", ORTE[i], id);
        int r = rest_ordner_leer(p);
        if (r != 1) { *wo = p; return r; }
    }
    return 1;
}
/* fürs Protokoll: was in den Ordnern der Kennung noch liegt (Name, Größe, Datum) - damit die Herkunft solcher Reste ablesbar ist */
static const char *rest_inhalt_ein(arena *a, const char *p, const char *vor, int tiefe, int *anz)
{
    DIR *d = opendir(p);
    if (!d) return "";
    const char *t = "";
    struct dirent *e;
    while ((e = readdir(d)) != NULL && *anz < 12) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        const char *q = a_fmt(a, "%s/%s", p, e->d_name);
        struct stat st;
        if (lstat(q, &st) != 0) continue;
        (*anz)++;
        char dz[24] = ""; struct tm tm;
        if (localtime_r(&st.st_mtime, &tm)) strftime(dz, sizeof dz, "%Y-%m-%d %H:%M", &tm);
        if (S_ISDIR(st.st_mode)) {
            t = a_fmt(a, "%s%s%s%s/ (%s)", t, t[0] ? ", " : "", vor, e->d_name, dz);
            if (tiefe < 3) { const char *u = rest_inhalt_ein(a, q, a_fmt(a, "%s%s/", vor, e->d_name), tiefe + 1, anz); if (u[0]) t = a_fmt(a, "%s, %s", t, u); }
        } else t = a_fmt(a, "%s%s%s%s %lld B (%s)", t, t[0] ? ", " : "", vor, e->d_name, (long long)st.st_size, dz);
    }
    closedir(d);
    return t;
}
static const char *rest_inhalt(arena *a, const char *id)
{
    static const char *const ORTE[] = { "/user/app", "/user/patch", "/user/appmeta", "/system_data/priv/appmeta", NULL };
    const char *t = "";
    for (int i = 0; ORTE[i]; i++) {
        const char *p = a_fmt(a, "%s/%s", ORTE[i], id);
        struct stat st;
        if (lstat(p, &st) != 0) { t = a_fmt(a, "%s%s%s %s", t, t[0] ? "; " : "", p, SA("fehlt", "missing")); continue; }
        char dz[24] = ""; struct tm tm;
        if (localtime_r(&st.st_mtime, &tm)) strftime(dz, sizeof dz, "%Y-%m-%d %H:%M", &tm);
        int anz = 0;
        const char *u = rest_inhalt_ein(a, p, "", 1, &anz);
        t = a_fmt(a, "%s%s%s (%s): %s", t, t[0] ? "; " : "", p, dz, u[0] ? u : SA("leer", "empty"));
    }
    return t;
}
static antwort sony_deinstallieren_innen(arena *a, const char *id, bool nur_pruefen)
{
    if (!strcmp(id, SO_ID) || !strcmp(id, "PSCL00002")) return sony_antwort(a, 550, SA("550 PS5 Cleanup läuft gerade über diese App", "550 PS5 Cleanup is running through this app right now"));   /* eigener Startordner */
    if (strlen(id) != 9) return sony_antwort(a, 550, SA("550 Kennung ungültig", "550 invalid ID"));
    /* NEU 1.0.7 (da_Chris86 31.10.2026 21:50, Punkt 4): Behandlung nach der Regel (kern_geister.c) - unklare Einbindung: 451, nichts tun */
    const char *efs = NULL, *eq = NULL, *eg = "";
    int em = einbindung_abfragen(a, id, &efs, &eq);
    int regel = kern_geist_regel(id, em, efs, eq, em == 1 ? eigene_spielquelle(a, id) : NULL, &eg);
    if (regel == GEIST_UNVERAENDERT) { if (!nur_pruefen) sony_log(SP_EN ? "Tile %s: %s - unchanged" : "Kachel %s: %s - unverändert", id, eg); return sony_antwort(a, 451, a_fmt(a, "451 %s", eg)); }
    if (nur_pruefen) return sony_antwort(a, 200, "200 OK");
    if (sony_gestoert) return sony_antwort(a, 550, SA("550 Sonys Dienst hat in dieser Sitzung schon einmal nicht geantwortet", "550 Sony's service already failed to respond once in this session"));
    {
        int e = 0, b = sony_bereit(&e);
        if (b == 0) return sony_antwort(a, 550, SA("550 Sonys Dienst antwortet nicht (Initialisierung, 10 s)", "550 Sony's service does not respond (initialization, 10 s)"));
        if (b < 0) return sony_antwort(a, 550, a_fmt(a, SA("550 Dienst nicht verfügbar 0x%08X", "550 service not available 0x%08X"), (unsigned)e));
    }
    int vorher = sony_eingetragen(id);
    klog_printf("PS5 Cleanup: Sony-Deinstallation %s - vorher eingetragen=%d\n", id, vorher);
    if (vorher == -2) return sony_antwort(a, 550, SA("550 Sonys Dienst antwortet nicht (Statusabfrage, 5 s)", "550 Sony's service does not respond (status query, 5 s)"));
    if (vorher == 0) {
        /* NEU 1.0.7 (da_Chris86 01.11.2026, Reste ohne Kachel - z. B. L.A. Noire am 01.11. morgens): Sony führt die Kennung nicht im
         * Menü. Steht sie auch nicht in app.db (gleiche Prüfung wie der PKG Manager), gibt es keine Kachel, die stehen bleiben
         * könnte -> 226, die Dateien löscht der Kern selbst. Steht sie in app.db oder ist das unklar: nichts löschen.
         * (Bis 1.0.7: "200" - der Kern wartete 60 s auf Sony und meldete fälschlich "Sony löscht noch".) */
        const char *g = "";
        int in_db = db_kennung_in_appdb(a, plattform_db_ordner(), id, &g);
        sony_log(SP_EN ? "Tile %s: not registered with Sony, in app.db: %s%s%s" : "Kachel %s: bei Sony nicht eingetragen, in app.db: %s%s%s", id, jn(in_db), g[0] ? " - " : "", g);
        if (in_db == 0) return sony_antwort(a, 226, SA("226 nicht im PS5-Menü eingetragen und nicht in app.db - Reste ohne Kachel", "226 not listed in the PS5 menu and not in app.db - leftovers without a tile"));
        if (in_db == 1) {
            /* NEU 1.3 (da_Chris86 21.11.2026, Meldung #87): bis 1.2 hier immer "nicht sicher, nichts gelöscht" - die leeren Ordner ließen sich nie
             * entfernen. Jetzt: ist an keinem Ort mehr ein Spiel für die Kennung (Ordner leer oder weg), trägt PS5 Cleanup genau diese Kennung
             * selbst aus Sonys Datenbank aus (app/spielstand_db.c kachel_db_rest_austragen: erst prüfen, dann alles oder nichts, ohne Sicherung -
             * da_Chris86 21.11.2026 06:57), prüft nach und lässt dann den Kern die leeren Ordner löschen (226). Sony selbst wird nicht beauftragt:
             * es führt die Kennung nicht mehr (ein Auftrag für eine Kennung, die Sony nicht kennt, kam schon einmal nie zurück - SLIB00001, 29.10.2026). */
            sony_log(SP_EN ? "Tile %s: what is left - %s" : "Kachel %s: was noch da ist - %s", id, rest_inhalt(a, id));
            const char *wo = "";
            int leer = rest_kein_spiel(a, id, &wo);
            if (leer != 1) return sony_antwort(a, 550, a_fmt(a, SA("550 Sony führt die Kennung nicht im Menü, sie steht aber in app.db - in %s liegt noch etwas%s, nichts gelöscht",
                                                                   "550 Sony does not list the ID in the menu, but it is in app.db - %s still contains something%s, nothing deleted"), wo, leer < 0 ? SA(" (nicht lesbar)", " (not readable)") : ""));
            char g2[1200] = ""; int z = 0;
            int pr = kachel_db_rest_austragen(plattform_db_ordner(), id, 1, &z, g2, sizeof g2);
            sony_log(SP_EN ? "Tile %s in Sony's database: %s" : "Kachel %s in Sonys Datenbank: %s", id, g2);
            if (pr != 0) return sony_antwort(a, 550, a_fmt(a, SA("550 Sony führt die Kennung nicht im Menü, sie steht aber in app.db - austragen geht nicht (%s), nichts gelöscht",
                                                                 "550 Sony does not list the ID in the menu, but it is in app.db - cannot be removed (%s), nothing deleted"), g2));
            int r = kachel_db_rest_austragen(plattform_db_ordner(), id, 0, &z, g2, sizeof g2);
            sony_log(SP_EN ? "Tile %s from Sony's database: %s" : "Kachel %s aus Sonys Datenbank: %s", id, g2);
            if (r != 0) return sony_antwort(a, 550, a_fmt(a, SA("550 Sony führt die Kennung nicht im Menü, sie steht aber in app.db - austragen fehlgeschlagen (%s), nichts gelöscht",
                                                                "550 Sony does not list the ID in the menu, but it is in app.db - removing failed (%s), nothing deleted"), g2));
            const char *g3 = "";
            int nach = db_kennung_in_appdb(a, plattform_db_ordner(), id, &g3);
            sony_log(SP_EN ? "Tile %s afterwards in app.db: %s%s%s" : "Kachel %s danach in app.db: %s%s%s", id, jn(nach), g3[0] ? " - " : "", g3);
            if (nach != 0) return sony_antwort_art(a, 550, a_fmt(a, SA("550 aus Sonys Datenbank ausgetragen, die Nachprüfung findet die Kennung aber noch (%s) - Ordner nicht gelöscht",
                                                                        "550 removed from Sony's database, but the check still finds the ID (%s) - folders not deleted"), nach == 1 ? SA("steht noch in app.db", "still in app.db") : g3), ANTW_VERAENDERT);
            return sony_antwort_art(a, 226, SA("226 Rest eines an Sony vorbei gelöschten Spiels - aus Sonys Datenbank ausgetragen, die Reste löscht PS5 Cleanup",
                                               "226 leftover of a game deleted outside Sony - removed from Sony's database, PS5 Cleanup deletes the leftovers"), ANTW_DB_AUSGETRAGEN);   /* art: Codex-Prüfung 459 */
        }
        return sony_antwort(a, 550, a_fmt(a, SA("550 nicht im PS5-Menü eingetragen, app.db ließ sich nicht prüfen (%s) - nichts gelöscht", "550 not listed in the PS5 menu, app.db could not be checked (%s) - nothing deleted"), g));
    }
    if (vorher < 0) return sony_antwort(a, 550, SA("550 ob die Kachel eingetragen ist, ließ sich vorher nicht prüfen - Sony nicht beauftragt", "550 whether the tile is listed could not be checked beforehand - Sony was not asked"));   /* 01.11.2026 (Prüfung) */
    /* NEU 1.2 (Bestandsaufnahme 14.11.2026 Punkt 10): das Programm der Kachel wird nicht mehr hier eigens gesucht und beendet - vor dem
     * Löschen schließt PS5 Cleanup ohnehin alle nachgeladenen Programme (plattform_fremde_schliessen). Bis 1.1 stand hier
     * kachel_programm_beenden: fand auf FW 12.00 nie etwas ("Anschlüsse nicht abfragbar") und beendete dann namenlose Programme auf Verdacht. */
    /* Codex-Pruefung 271 (1): vor jeder Aenderung pruefen, ob das Spiel laeuft - sonst waere die Zusatzinhalt-Reparatur schon geschrieben */
    if (regel == GEIST_HINTERLASSEN && titel_laeuft(id) != 0) return sony_antwort(a, 451, SA("451 das Spiel läuft gerade oder das ließ sich nicht feststellen - unverändert (Spiel schließen, dann noch einmal löschen)", "451 the game is running or that could not be determined - unchanged (close the game, then delete again)"));
    /* da_Chris86 14.11.2026 15:29 (Codex-Pruefung 273): erst alles pruefen und die Einbindung loesen, dann erst Sonys Liste anfassen -
     * wird die Loeschung verweigert, ist vorher nichts in appinfo.db geaendert. Die spaeteren Aufrufe unten finden dann keine Einbindung mehr. */
    bool geloest = false;   /* Codex-Pruefung 275: danach kein weiteres Loesen vor dem ersten Sony-Auftrag - nach der Reparatur gibt es keine Ablehnung mit unveraendert mehr */
    if (regel == GEIST_PS5UPLOAD || regel == GEIST_EIGENES_SPIEL || regel == GEIST_HINTERLASSEN) {
        if (sony_einbindung_loesen(a, id) < 0) return sony_antwort(a, 451, SA("451 die Einbindung der Kachel ließ sich nicht lösen (läuft das Spiel noch: schließen, dann noch einmal löschen)", "451 the mount of the tile could not be released (if the game is still running: close it, then delete again)"));
        geloest = true;
    }
    /* NEU 1.3 (PS5-Test da_Chris86 21.11.2026 13:48): PS4-Spiele von ps4mount - ps4mount bindet seine Ordner /system_data/ps4mount/... über
     * /user/app/<ID>, /user/patch/<ID> und /user/addcont/<ID>... ein. Der PS4Mount im Cooling Center hängt beim Beenden NICHT aus ("[main] worker
     * stopped"); Sony löschte dann durch die Einbindung hindurch ps4mounts Verweise und lehnte ab (0x80AF5002) - die Kachel blieb ohne Spiel.
     * ps4mount selbst macht es so (gasb07/ps4mount src/main.c app_remove): erst aushängen (nicht erzwungen), dann Sony beauftragen. Genau das,
     * nur für Einbindungen aus /system_data/ps4mount/ und nie unter einem laufenden Spiel. Geht es nicht: unverändert, Sony wird nicht beauftragt. */
    if (!strncmp(id, "CUSA", 4)) {
        int pr = p4m_einbindungen_loesen(id);
        if (pr == -2) {   /* die Antwort sagt selbst, was verändert ist (sony_g_loesen bleibt aus - sonst hängt die Hülle einen zweiten Satz an) */
            return sony_antwort_art(a, 553, SA("553 bereits geändert: ps4mount hatte das Spiel eingebunden - ein Teil der Einbindungen ist gelöst, ein Teil ließ sich nicht lösen (läuft das Spiel noch: schließen, dann noch einmal löschen)",
                                               "553 already changed: ps4mount had the game mounted - some mounts were released, some could not be released (if the game is still running: close it, then delete again)"), ANTW_VERAENDERT); }
        if (pr < 0) return sony_antwort(a, 451, SA("451 ps4mount hat das Spiel noch eingebunden und die Einbindung ließ sich nicht lösen (läuft das Spiel noch: schließen, dann noch einmal löschen) - unverändert",
                                                     "451 ps4mount still has the game mounted and the mount could not be released (if the game is still running: close it, then delete again) - unchanged"));
        if (pr > 0) { geloest = true; sony_g_loesen = true; }
    }
    {   /* NEU 1.0.7 (da_Chris86 01.11.2026 22:03, Test B 22:08): kaputte Zusatzinhalt-Angaben zuerst reparieren */
        char zg[320] = ""; int zz = 0;
        int zr = zusatz_reparieren(id, &zz, zg, sizeof zg);
        if (zr == -1) return sony_antwort(a, 550, a_fmt(a, SA("550 kaputte Zusatzinhalt-Angaben ließen sich nicht reparieren (%s) - nichts gelöscht%s", "550 broken add-on entries could not be repaired (%s) - nothing deleted%s"), zg, geloest ? SA(" (die Einbindung der Kachel ist schon gelöst)", " (the mount of the tile is already released)") : ""));
        if (zr == -2) sony_log(SP_EN ? "Tile %s: add-on entries could not be checked (%s) - Sony is asked as before" : "Kachel %s: Zusatzinhalt-Angaben nicht prüfbar (%s) - Sony wird wie bisher beauftragt", id, zg);
        if (zr == 0 && zz > 0) sony_g_rep = true;
        if (zr == 0 && zz > 0) sony_log(SP_EN ? "Tile %s: %d broken add-on entr(y/ies) in appinfo.db set to 0" : "Kachel %s: %d kaputte Zusatzinhalt-Angabe(n) in appinfo.db auf 0 gesetzt", id, zz);
    }
    bool ps5upload = regel == GEIST_PS5UPLOAD;
    /* NEU 1.0.7 (da_Chris86 01.11.2026 23:19): Ordner-Spiel - Einbindung des eigenen Spielordners lösen, dann EIN Sony-Auftrag wie sonst */
    int e = 0, jetzt = 1;
    /* NEU 1.0.7 (Punkt 3, nur ps5upload): Sony nimmt an, der Eintrag bleibt aber - auf neuerer Firmware legt die Registrierung über
     * AppInstallAll zwei Einträge an, eine Deinstallation entfernt einen (ps5upload webui_tile.c:180, gemessen auf FW 13.x). Darum
     * höchstens zwei weitere kontrollierte Versuche (je bis 20 s warten, nach dem letzten bis 60 s). Alle anderen Kacheln: EIN Auftrag,
     * bis 60 s warten - wie 1.0.6 und wie im echten Test auf FW 12.00 (31.10.2026). Nur Sonys Dienst, kein SQLite. */
    /* NEU 1.0.7 (01.11.2026, nach der unabhängigen Prüfung): mehrere Aufträge nur für nachweislich unterstützte Anwendungen (da_Chris86
     * 31.10.2026 21:50, Punkt 4), also nur PS5Upload - mit Einbindung (PS5-Test 01.11.2026, FW 12.00, PS5Upload 10: 2 Aufträge nötig)
     * und auch ohne Einbindung (PS5Upload läuft gerade nicht, z. B. nach einem Neustart), wenn sein eigener Kachel-Ordner da ist.
     * Alle anderen Kacheln: EIN Auftrag (PS5-Test 8/9 am 30.10.2026: 21 Kacheln, je ein Auftrag genügte). */
    /* NEU 1.0.8 (PS5-Test 07.11.2026 17:40): auch ohne den eigenen Kachel-Ordner von ps5upload - der war auf da_Chris86s PS5 schon gelöscht,
     * PS5 Cleanup beauftragte Sony darum nur einmal, die Kachel blieb eingetragen (FW 12.00 braucht zwei Aufträge, Test 01.11.2026) und Sony
     * entfernte die Dateien danach im Hintergrund - übrig blieb eine Kachel ohne App. Jede andere Einbindung bleibt wie bisher unangetastet
     * (em == 0: keine Einbindung). */
    bool mehrfach = ps5upload || (em == 0 && !strcmp(id, "PSUP00001"));
    /* NEU 1.0.10 (Forum 10.11.2026, TabShifter: "zwei Versionen mit unterschiedlichen Einträgen" in app.db, die Kachel ließ sich weder
     * starten noch löschen): doppelte Einträge gibt es nicht nur bei PS5Upload. Jetzt für JEDE Kachel: bleibt sie nach Sonys Zusage
     * eingetragen, bis zu zwei weitere Aufträge. PS5Upload wartet wie bisher je 20 s, alle anderen je bis 60 s (wie bisher beim
     * einzigen Auftrag) - eine Kachel, die schon beim ersten Mal geht, merkt davon nichts. */
    int versuche = 3, gemacht = 0;
    bool einbindung_abbruch = false;
    int dateien_vorher = app_ordner_eintraege(id);   /* NEU 1.3 */
    for (int versuch = 1; versuch <= versuche; versuch++) {
        if (versuch > 1) {
            sony_log(SP_EN ? "Tile %s: Sony accepted, but the entry is still there - attempt %d of 3" : "Kachel %s: Sony hat angenommen, der Eintrag ist aber noch da - Versuch %d von 3", id, versuch);
            if (sony_einbindung_loesen(a, id) < 0) { einbindung_abbruch = true; break; }
        }
        /* NEU 1.1 (Codex-Prüfung 295, P2): hatte Sony den 1. Auftrag schon angenommen, muss das im Grund stehen - sonst meldet der Kern
         * "die App bleibt vollständig erhalten", obwohl Sony sie noch im Hintergrund entfernen kann (kachel_bleibt erkennt "angenommen") */
        if (!sony_rufe(2, id, 30000, &e, NULL))
            return sony_antwort_art(a, 550, versuch > 1 ? a_fmt(a, SA("550 Sony hat den 1. Auftrag angenommen, der %d. antwortet nicht (30 s) - die Kachel ist noch eingetragen", "550 Sony accepted the 1st request, request %d does not respond (30 s) - the tile is still listed"), versuch)
                                                    : SA("550 Sony antwortet nicht (Deinstallation, 30 s) - der Auftrag ist vielleicht schon angenommen, Sony kann die App noch entfernen", "550 Sony does not respond (uninstall, 30 s) - the request may already be accepted, Sony may still remove the app"), ANTW_ANGENOMMEN);   /* Codex-Prüfung 296 (1) */
        gemacht = versuch;
        klog_printf("PS5 Cleanup: Sony-Deinstallation %s -> 0x%08X (Versuch %d)\n", id, (unsigned)e, versuch);
        if (e) { sony_nachher(a, id); int dn = app_ordner_eintraege(id); if (dateien_vorher > 0 && dn >= 0 && dn < dateien_vorher) sony_g_dateien = true; }   /* NEU 1.0.10; 1.3: Dateien schon weg? */
        if (e && versuch > 1) return sony_antwort_art(a, 550, a_fmt(a, SA("550 Sony hat den 1. Auftrag angenommen, den %d. abgelehnt (0x%08X) - die Kachel ist noch eingetragen", "550 Sony accepted the 1st request, refused request %d (0x%08X) - the tile is still listed"), versuch, (unsigned)e), ANTW_ANGENOMMEN);
        /* NEU 1.0.10 (Codex-Prüfung 171, 1): 553 = Sony hat den ERSTEN Auftrag ausdrücklich abgelehnt, nichts wurde angenommen - nur dann
         * darf der Kern eine gewählte Homebrew-Kachel gezielt aus Sonys Datenbank austragen (kein Wettlauf mit einem laufenden Auftrag) */
        if (e) return sony_antwort(a, 553, a_fmt(a, SA("553 Sony hat abgelehnt (0x%08X)", "553 Sony refused (0x%08X)"), (unsigned)e));
        int warten = (mehrfach && versuch < versuche) ? 20 : 300;   /* NEU 1.1 (15.11.2026, wie ps5upload: bis 5 Minuten; bis 1.0.10 60 s) */
        for (int i = 0; i < warten && (jetzt = sony_eingetragen(id)) == 1; i++) sony_warte(1000);
        if (jetzt != 1) break;
    }
    klog_printf("PS5 Cleanup: Sony-Deinstallation %s - danach eingetragen=%d\n", id, jetzt);
    if (jetzt == 1) sony_log(SP_EN ? "Tile %s: still registered after %d request(s) - stays in the menu" : "Kachel %s: nach %d Auftrag/Aufträgen noch eingetragen - bleibt im Menü", id, gemacht);
    if (jetzt == 1) sony_nachher(a, id);   /* NEU 1.0.10 */
    if (jetzt == 1 && einbindung_abbruch) return sony_antwort_art(a, 550, a_fmt(a, SA("550 Sony hat angenommen, die Kachel ist aber noch eingetragen - vor dem %d. Auftrag ließ sich die Einbindung nicht lösen, abgebrochen", "550 Sony accepted, but the tile is still listed - before request %d the mount could not be released, stopped"), gemacht + 1), ANTW_ANGENOMMEN);
    if (jetzt == 1) return sony_antwort_art(a, 550, gemacht > 1 ? a_fmt(a, SA("550 Sony hat angenommen, die Kachel ist nach %d Aufträgen aber noch eingetragen", "550 Sony accepted, but the tile is still listed after %d requests"), gemacht) : SA("550 Sony hat angenommen, die Kachel ist nach 5 Minuten aber noch eingetragen", "550 Sony accepted, but the tile is still listed after 5 minutes"), ANTW_ANGENOMMEN);
    if (jetzt == -2) return sony_antwort_art(a, 550, SA("550 Sony hat angenommen, die Statusabfrage antwortet aber nicht", "550 Sony accepted, but there is no answer to the status query"), ANTW_ANGENOMMEN);
    if (jetzt < 0) return sony_antwort_art(a, 550, SA("550 Sony hat angenommen, ob die Kachel ausgetragen ist, ließ sich nicht prüfen", "550 Sony accepted, but whether the tile was removed could not be checked"), ANTW_ANGENOMMEN);
    return sony_antwort(a, 200, "200 OK");
}
/* NEU 1.0.6: eigener Startordner bei der Selbstentfernung (start/startordner.h - nur die eigenen Dateien) */
static int startordner_weg(arena *a, bool nur_pruefen, const char **text)
{
    static char t[300];
    *text = NULL;
    SW_EN = SP_EN ? 1 : 0;   /* NEU 1.1 (Codex-Prüfung 326): Texte aus startordner.h in der Sprache der Oberfläche */
    if (nur_pruefen) { int z = so_zustand(); if (z < 0) { *text = a_kopie(a, SP_EN ? SO_DIR " does not clearly belong to PS5 Cleanup - stays." : SO_DIR " gehört nicht eindeutig zu PS5 Cleanup - bleibt."); return -1; } return z; }
    int r = so_entfernen(t, sizeof t);
    klog_printf("PS5 Cleanup: Startordner entfernen -> %d %s\n", r, t);
    if (t[0]) *text = a_kopie(a, t);
    return r;
}
/* NEU 1.0.7: /system_ex für leere Einhängeordner von ShadowMount öffnen (start/startordner.h so_oeffnen) */
static int system_ex_oeffnen(arena *a, const char **text)
{
    static char t[300];
    *text = NULL;
    SW_EN = SP_EN ? 1 : 0;   /* NEU 1.1 (Codex-Prüfung 326) */
    int vor = so_ist_ro(); so_nm_r = -99; so_nm_e = 0;
    int r = so_oeffnen(t, sizeof t);
    klog_printf("PS5 Cleanup: /system_ex öffnen -> %d %s\n", r, t);
    if (r == 1 && vor == 1 && plattform_meldung)   /* da_Chris86 14.11.2026 18:38: sichtbar machen, dass es offen bleibt */
        plattform_meldung(SP_EN ? "/system_ex is now writable and stays so until the next restart of the PS5 (like with ShadowMount)."
                                : "/system_ex ist jetzt beschreibbar und bleibt es bis zum nächsten Neustart der PS5 (wie bei ShadowMount).");
    if (r != 1 && plattform_meldung) {   /* da_Chris86 14.11.2026 16:36: Zustand, Rückgabewert und Fehler ins Protokoll */
        char m[420];
        if (so_nm_r == -99) snprintf(m, sizeof m, "/system_ex beschreibbar machen: Ergebnis %d, kein Umhänge-Versuch, schreibgeschützt vorher=%d jetzt=%d (1 ja, 0 nein, -1 unbekannt)", r, vor, so_ist_ro());
        else snprintf(m, sizeof m, "/system_ex beschreibbar machen: Ergebnis %d, nmount-Rückgabe %d, Fehler %d (%s), schreibgeschützt vorher=%d jetzt=%d (1 ja, 0 nein, -1 unbekannt)", r, so_nm_r, so_nm_e, so_nm_e ? strerror(so_nm_e) : "keiner", vor, so_ist_ro());
        plattform_meldung(m);
    }
    if (t[0]) *text = a_kopie(a, t);
    return r;
}
static volatile int selbst_uebergeben_ok = 0;   /* NEU 1.0.7: dann stellt der Hintergrundteil /system_ex zurück */
static int hintergrund_fragen(const char *pfad, const char *erwartet, char *t, size_t tl);   /* weiter unten (seit 1.0.8 in allen Fassungen) */
static int selbst_uebergeben(arena *a, bool kachel, bool startordner, const char **text)
{
    char t[300], pfad[320];
    snprintf(pfad, sizeof pfad, "/ps5cleanup-selbstweg?kachel=%d&start=%d&en=%d%s%s", kachel ? 1 : 0, startordner ? 1 : 0, SP_EN ? 1 : 0,
             prot_stick[0] ? "&prot=" : "", prot_stick);   /* NEU 1.0.7: Protokoll auf dem Stick (nur Buchstaben, Ziffern, / _ - .); NEU 1.1: Sprache */
    int r = hintergrund_fragen(pfad, "PS5CLEANUP-UEBERNOMMEN", t, sizeof t);
    klog_printf("PS5 Cleanup: Übergabe der Selbstentfernung an den Hintergrundteil (%s) -> %d %s\n", pfad, r, t);
    if (r == 1 && startordner) selbst_uebergeben_ok = 1;
    *text = a_kopie(a, t);
    return r;
}
bool plattform_neustart_uebergeben(void)
{
    char t[300];
    int r = hintergrund_fragen("/ps5cleanup-neustart", "PS5CLEANUP-NEUSTART-OK", t, sizeof t);
    klog_printf("PS5 Cleanup: Neustart an den Hintergrundteil -> %d %s\n", r, t);
    return r == 1;
}
/* Neustart über den Systemdienst (PS5-Suite 7.3: sceSystemServiceRequestReboot - nicht der rohe reboot()-Aufruf,
 * der die Konsole hängen lassen kann). da_Chris86 28.10.2026 23:19: nur Neustart und Beenden, kein Ruhemodus/Ausschalten. */
int sceSystemServiceRequestReboot(void);
bool plattform_neustart(void) { int e = sceSystemServiceRequestReboot(); klog_printf("PS5 Cleanup: Neustart -> 0x%08X\n", (unsigned)e); return e == 0; }
/* NEU 1.0.7: Hinweis nach ○ an die Abschlussmeldung des Hintergrundteils hängen */
bool plattform_hinweis_uebergeben(int art, bool englisch)
{
    char t[300], pfad[80];
    snprintf(pfad, sizeof pfad, "/ps5cleanup-hinweis?art=%d&en=%d", art == 2 ? 2 : 1, englisch ? 1 : 0);
    int r = hintergrund_fragen(pfad, "PS5CLEANUP-HINWEIS-OK", t, sizeof t);
    klog_printf("PS5 Cleanup: Hinweis an den Hintergrundteil (%s) -> %d %s\n", pfad, r, t);
    return r == 1;
}
#else
/* Simulation/Testfassungen: Sonys Dienst ist nie eingebunden - die Simulation prüft nur und führt "auf dem Papier" aus */
static antwort sony_deinstallieren(arena *a, const char *id, bool nur_pruefen)
{
    if (!strcmp(id, SO_ID)) return sony_antwort(a, 550, SA("550 PS5 Cleanup läuft gerade über diese App", "550 PS5 Cleanup is running through this app right now"));
    return nur_pruefen ? sony_antwort(a, 200, "200 OK") : sony_antwort(a, 550, SA("550 nicht eingebaut", "550 not built in"));
}
/* Simulation: nur nachsehen, ob der eigene Startordner da ist ("würde gelöscht") */
static int startordner_weg(arena *a, bool nur_pruefen, const char **text)
{
    (void)nur_pruefen; *text = NULL;
    int z = so_zustand();
    if (z < 0) { *text = a_kopie(a, SA(SO_DIR " gehört nicht eindeutig zu PS5 Cleanup - bleibt.", SO_DIR " does not clearly belong to PS5 Cleanup - stays.")); return -1; }
    return z;
}
bool plattform_neustart(void) { return false; }
bool plattform_neustart_uebergeben(void) { return false; }   /* Simulation: es wird nichts übergeben */
bool plattform_hinweis_uebergeben(int art, bool englisch) { (void)art; (void)englisch; return false; }   /* Simulation: nichts übergeben */
#endif

/* =====================================================================================================================
 * NEU 1.0.7 (da_Chris86 01.11.2026 17:31 "Neuaufbau der Datenbank ist inakzeptabel", 18:11 "alles in einem sauberen Rutsch",
 * 18:27 "wir machen alles"): Bilder/Videos und Spielstände ohne leere Einträge entfernen - Befehle siehe kern/zugriff.h.
 * In allen Fassungen werden nur die Prüfungen gemacht; gelöscht/ausgetragen wird nur in der echten Fassung (KERN_LOESCHEN).
 * ===================================================================================================================== */
#include <dlfcn.h>
#include <dirent.h>
#ifdef KERN_LOESCHEN
#include "spielstand_db.h"
#endif
static __attribute__((unused)) bool da_lstat(const char *p) { struct stat st; return lstat(p, &st) == 0; }

/* --- Bilder und Videos: Sonys Löschdienst (libSceContentDelete), wie "Löschen" in der Mediengalerie ---
 * PS5-Test 01.11.2026 17:53/18:01 (PS5 Pro FW 12.00, Test-ELF start/test_medien.c als App PSCL00002): sceContentDeleteByPath mit dem
 * Pfad, wie er in Sonys Mediengalerie-Datenbank steht ("/av_contents/...", ohne /user) -> 0; Sony löschte Aufnahme, .meta/.dat,
 * Vorschaubild, leere Ordner und den Eintrag in der Mediengalerie - kein leerer Eintrag, kein Neustart, kein Neuaufbau.
 * Geht nur in einer App (als reines Programm lehnte die Initialisierung ab) - die Oberfläche läuft als App PSCL00002.
 * Öffentlich belegt: Funktionsnamen/NIDs und Heap-Größe an Stelle 8 (AnyPS5 docs/dev/debt/libSceContentDelete.md). */
#ifdef KERN_LOESCHEN
typedef struct { uint64_t vorne; uint64_t heap_groesse; uint8_t reserviert[48]; } cd_init_t;
static cd_init_t cd_init;                  /* statisch: bleibt gültig, auch wenn ein Aufruf hängen bleibt */
static void *cd_nach_pfad = NULL;          /* sceContentDeleteByPath */
static int cd_zustand = 0;                 /* 0 = noch nicht vorbereitet, 1 = bereit, -1 = nicht verfügbar (cd_grund) */
static char cd_grund_de[200], cd_grund_en[200];   /* Codex-Prüfung 373 (5): beide Sprachen, ausgegeben in der aktuellen */
#define cd_grund (SP_EN ? cd_grund_en : cd_grund_de)
static int cd_gestoert = 0;                /* ein Aufruf kam nicht zurück - in dieser Sitzung nicht mehr benutzen */
typedef struct { int (*f)(const void *); const void *arg; volatile int fertig; volatile int e; } cd_auftrag;
static void *cd_faden(void *x) { cd_auftrag *au = x; au->e = au->f(au->arg); au->fertig = 1; return NULL; }
/* jeder Aufruf in einem eigenen Faden mit Zeitgrenze (wie sony_rufe). 1 = fertig (*e), 0 = keine Antwort (Faden bleibt zurück) */
static int cd_rufe(void *f, const void *arg, int ms, int *e)
{
    if (cd_gestoert) return 0;
    cd_auftrag *au = calloc(1, sizeof *au);
    if (!au) { *e = -1; return 1; }
    au->f = (int (*)(const void *))f; au->arg = arg;
    pthread_t th;
    if (pthread_create(&th, NULL, cd_faden, au)) { free(au); *e = -1; return 1; }
    pthread_detach(th);
    for (int i = 0; i < ms / 100 && !au->fertig; i++) sony_warte(100);
    if (!au->fertig) { cd_gestoert = 1; klog_printf("PS5 Cleanup: Sonys Löschdienst (Mediengalerie) - keine Antwort nach %d ms\n", ms); return 0; }
    *e = au->e; free(au);
    return 1;
}
static bool cd_bereit(void)
{
    if (cd_zustand) return cd_zustand > 0;
    cd_zustand = -1;
    static const char *const LIB = "libSceContentDelete.sprx";
    (void)dlopen(LIB, RTLD_LAZY);
    uint32_t h = 0;
    int e = 0;
    void *init = NULL;
    if (kernel_dynlib_handle(-1, LIB, &h)) { snprintf(cd_grund_de, sizeof cd_grund_de, "Sonys Löschdienst für Bilder/Videos ließ sich nicht laden"); snprintf(cd_grund_en, sizeof cd_grund_en, "Sony's delete service for images/videos could not be loaded"); goto aus; }
    init = (void *)kernel_dynlib_resolve(-1, h, "zoxb0wEChEM");          /* sceContentDeleteInitialize */
    cd_nach_pfad = (void *)kernel_dynlib_resolve(-1, h, "qC3QW+AltHs");  /* sceContentDeleteByPath */
    if (!init || !cd_nach_pfad) { snprintf(cd_grund_de, sizeof cd_grund_de, "Sonys Löschdienst für Bilder/Videos: Funktionen nicht gefunden"); snprintf(cd_grund_en, sizeof cd_grund_en, "Sony's delete service for images/videos: functions not found"); goto aus; }
    memset(&cd_init, 0, sizeof cd_init); cd_init.heap_groesse = 0x4000;
    if (!cd_rufe(init, &cd_init, 10000, &e)) { snprintf(cd_grund_de, sizeof cd_grund_de, "Sonys Löschdienst für Bilder/Videos antwortet nicht (Vorbereitung, 10 s)"); snprintf(cd_grund_en, sizeof cd_grund_en, "Sony's delete service for images/videos does not respond (preparation, 10 s)"); goto aus; }
    if (e) { snprintf(cd_grund_de, sizeof cd_grund_de, "Sonys Löschdienst für Bilder/Videos lehnt ab (Vorbereitung 0x%08X)", (unsigned)e); snprintf(cd_grund_en, sizeof cd_grund_en, "Sony's delete service for images/videos refuses (preparation 0x%08X)", (unsigned)e); goto aus; }
    cd_zustand = 1;
    sony_log(SP_EN ? "Media Gallery: Sony's delete service ready" : "Mediengalerie: Sonys Löschdienst bereit");
    return true;
aus:
    sony_log(SP_EN ? "Media Gallery: %s" : "Mediengalerie: %s", cd_grund);
    return false;
}
#endif
static antwort medium_loeschen(arena *a, const char *pfad, bool nur_pruefen)
{
    if (pfad) {
        if (!zugriff_medium_pfad(pfad)) return sony_antwort(a, 550, SA("550 keine Aufnahme der Mediengalerie", "550 not a Media Gallery capture"));
        struct stat st;
        if (lstat(pfad, &st) != 0) return sony_antwort(a, 550, a_fmt(a, "550 %s", strerror(errno)));
        if (!S_ISREG(st.st_mode)) return sony_antwort(a, 550, SA("550 keine normale Datei", "550 not a regular file"));
    }
#ifdef KERN_LOESCHEN
    if (!pfad) return cd_bereit() ? sony_antwort(a, 200, "200 OK") : sony_antwort(a, 502, a_fmt(a, "502 %s", cd_grund));
    if (nur_pruefen) return sony_antwort(a, 200, "200 OK");
    if (cd_gestoert) return sony_antwort(a, 550, SA("550 Sonys Löschdienst hat in dieser Sitzung schon einmal nicht geantwortet", "550 Sony's delete service already failed to respond once in this session"));
    if (!cd_bereit()) return sony_antwort(a, 550, a_fmt(a, "550 %s", cd_grund));
    /* worauf gewartet wird (nur was es vorher gab): Aufnahme, .meta, .dat, Vorschaubild */
    const char *bl = strrchr(pfad, '/') + 1, *pk = strrchr(bl, '.');
    int sl = (int)(pk && pk > bl ? (size_t)(pk - pfad) : strlen(pfad));
    const char *beg[4] = { pfad, a_fmt(a, "%.*s.meta", sl, pfad), a_fmt(a, "%.*s.dat", sl, pfad), a_fmt(a, "/user/av_contents/thumbnails/%s.jxr", pfad + 18) };
    bool warten[4];
    for (int i = 0; i < 4; i++) warten[i] = da_lstat(beg[i]);
    char *db = strdup(pfad + 5);                   /* "/av_contents/..." - so steht die Aufnahme in Sonys Mediengalerie-Datenbank */
    if (!db) return sony_antwort(a, 550, SA("550 kein Speicher", "550 out of memory"));
    int e = 0;
    if (!cd_rufe(cd_nach_pfad, db, 30000, &e)) return sony_antwort_art(a, 551, SA("551 Sonys Löschdienst antwortet nicht (30 s)", "551 Sony's delete service does not respond (30 s)"), ANTW_ANGENOMMEN);   /* db bleibt dem Faden; 551: evtl. schon angenommen */
    free(db);
    sony_log(SP_EN ? "Media Gallery: %s -> 0x%08X" : "Mediengalerie: %s -> 0x%08X", pfad + 5, (unsigned)e);
    if (e) return sony_antwort(a, 550, a_fmt(a, SA("550 Sony hat abgelehnt (0x%08X)", "550 Sony refused (0x%08X)"), (unsigned)e));
    for (int s = 0; s < 30; s++) {                 /* Sony löscht danach selbst (PS5-Test: unter 1 s) */
        bool rest = false;
        for (int i = 0; i < 4; i++) if (warten[i] && da_lstat(beg[i])) rest = true;
        if (!rest) return sony_antwort(a, 200, "200 OK");
        sony_warte(1000);
    }
    sony_log(SP_EN ? "Media Gallery: %s - Sony accepted, but something is still there after 30 s" : "Mediengalerie: %s - Sony hat angenommen, nach 30 s ist aber noch etwas da", pfad);
    return sony_antwort_art(a, 551, SA("551 Sony hat angenommen, nach 30 s ist die Aufnahme aber noch nicht ganz weg - Sony löscht evtl. noch", "551 Sony accepted, but after 30 s the capture is not completely gone - Sony may still be deleting"), ANTW_ANGENOMMEN);
#else
    return pfad && !nur_pruefen ? sony_antwort(a, 550, SA("550 nicht eingebaut", "550 not built in")) : sony_antwort(a, 200, "200 OK");   /* Simulation: nur prüfen */
#endif
}

/* --- Spielstände: Einträge aus Sonys Spielstand-Liste (savedata.db), Weg wie MacheteSVG (app/spielstand_db.c) ---
 * PS5-Test 01.11.2026 18:16 und 18:24 (start/test_spielstand.c, test_spielstand2.c): Eintrag gelöscht, danach die Dateien -> der
 * Spielstand verschwand aus der Liste, beim letzten Spielstand eines Spiels die ganze Gruppe; kein Neustart. Sonys eigene
 * Löschfunktion (sceSaveDataDelete) lehnte im Test ab. Freigabe da_Chris86 18:09 ("wenn wir einmalig die Datenbank anrühren müssen
 * für die Safe Games, dann ist es ja okay"). Nie, während das Spiel läuft oder der Spielstand eingebunden ist. */
/* 1 = in Benutzung (eingebunden oder ein Spiel mit dieser Kennung läuft), 0 = nein, -1 = nicht prüfbar */
static int spielstand_in_benutzung(const char *ordner, const char *titel, const char **wo)
{
    struct statfs *m = NULL;
    int n = getmntinfo(&m, MNT_NOWAIT);
    if (n <= 0 || !m) return -1;
    size_t ol = strlen(ordner), tl = strlen(titel);
    /* Prüfung 01.11.2026: ob ein Spiel läuft, lässt sich nur bei einer Spielkennung (AAAA12345) erkennen - sonst unklar */
    bool kennung = tl == 9;
    for (size_t i = 0; kennung && i < 9; i++) kennung = i < 4 ? (titel[i] >= 'A' && titel[i] <= 'Z') : (titel[i] >= '0' && titel[i] <= '9');
    if (!kennung) return -1;
    for (int i = 0; i < n; i++) {
        const char *von = m[i].f_mntfromname, *nach = m[i].f_mntonname;
        if (strncmp(von, ordner, ol) == 0 && (von[ol] == '/' || von[ol] == 0)) { *wo = nach; return 1; }
        /* Sandbox eines laufenden Spiels: /mnt/sandbox/<Kennung>_000/..., /mnt/sandbox/pfsmnt/<Kennung>-app0 - nur dort (ShadowMount
         * hängt Spiel-Abbilder dauerhaft unter /mnt/shadowmnt ein, das ist kein laufendes Spiel) */
        if (strncmp(nach, "/mnt/sandbox/", 13) == 0) for (const char *s = strstr(nach, titel); s; s = strstr(s + 1, titel))
            if ((s == nach || s[-1] == '/') && (s[tl] == '_' || s[tl] == '-' || s[tl] == '/' || s[tl] == 0)) { *wo = nach; return 1; }
    }
    return 0;
}
static antwort spielstand_austragen(arena *a, const char *ordner, bool nur_pruefen)
{
    if (!ordner) return sony_antwort(a, 200, "200 OK");
    char benutzer[9], titel[65]; bool prospero = false;
    if (!zugriff_spielstand_pfad(ordner, benutzer, &prospero, titel)) return sony_antwort(a, 550, SA("550 kein Spielstand-Ordner", "550 not a saved data folder"));
    const char *db = a_fmt(a, "/system_data/%s/%s/db/user/savedata.db", prospero ? "savedata_prospero" : "savedata", benutzer);
    struct stat st;
    if (stat(db, &st) != 0 || !S_ISREG(st.st_mode)) return sony_antwort(a, 550, a_fmt(a, SA("550 Sonys Spielstand-Liste %s nicht gefunden - ohne sie wird nichts gelöscht", "550 Sony's saved data list %s not found - without it nothing is deleted"), db));
    const char *wo = "";
    int b = spielstand_in_benutzung(ordner, titel, &wo);
    if (b > 0) return sony_antwort(a, 550, a_fmt(a, SA("550 der Spielstand ist gerade in Benutzung (%s) - erst das Spiel schließen", "550 the saved data is in use right now (%s) - close the game first"), wo));
    if (b < 0) return sony_antwort(a, 550, SA("550 ob das Spiel gerade läuft, ließ sich nicht prüfen - nichts gelöscht", "550 whether the game is running could not be checked - nothing deleted"));
    if (nur_pruefen) return sony_antwort(a, 200, "200 OK");
#ifdef KERN_LOESCHEN
    /* 02.11.2026 (da_Chris86 15:44): ohne Sicherung - auf dem USB-Stick nur das Protokoll. Geändert wird nach Prüfung, alles oder nichts. */
    char grund[300] = ""; int zeilen = 0;
    int r = spielstand_db_austragen(db, titel, &zeilen, grund, sizeof grund);
    if (r != 0) { sony_log(SP_EN ? "Sony's saved data list %s (%s): %s" : "Spielstand-Liste %s (%s): %s", titel, prospero ? "PS5" : "PS4", grund); return sony_antwort(a, 550, a_fmt(a, "550 %s", grund)); }
    sony_log(SP_EN ? "Sony's saved data list %s (%s): %d entr(y/ies) removed" : "Spielstand-Liste %s (%s): %d Eintrag/Einträge ausgetragen", titel, prospero ? "PS5" : "PS4", zeilen);
    if (zeilen == 0) {   /* Prüfung 01.11.2026: keine Einträge - liegen trotzdem Spielstände im Ordner, ist die Zuordnung unklar */
        DIR *dd = opendir(ordner);
        if (!dd) return sony_antwort(a, 550, SA("550 Spielstand-Ordner nicht lesbar - nichts gelöscht", "550 saved data folder not readable - nothing deleted"));
        int sd = 0; struct dirent *de;
        while ((de = readdir(dd)) != NULL) if (strncmp(de->d_name, "sdimg_", 6) == 0) sd++;
        closedir(dd);
        /* 1.3 (Rückmeldung #66, da_Chris86 20.11.2026 17:47 "alles einbauen"): bis 1.2 blieben solche Spielstände als "unklar" stehen. Sonys
         * Liste (savedata.db dieses Benutzers, Spalte title_id = Ordnername) führt sie nicht - Sony kennt sie nicht (z. B. von einem Programm
         * hineinkopiert, YouTube-Spielstände von Y2JB). Werden die Dateien gelöscht, bleibt in der Liste nichts zurück. */
        if (sd > 0)
            sony_log(SP_EN ? "Sony's saved data list %s: %d saved data file(s) without an entry - Sony does not know them, the files are deleted" : "Spielstand-Liste %s: %d Spielstand-Datei(en) ohne Eintrag - Sony kennt sie nicht, die Dateien werden gelöscht", titel, sd);
    }
    return sony_antwort(a, 200, a_fmt(a, SA("200 %d Einträge ausgetragen", "200 %d entries removed"), zeilen));
#else
    return sony_antwort(a, 550, SA("550 nicht eingebaut", "550 not built in"));   /* Simulation: nur prüfen */
#endif
}

/* NEU 1.0.7: Benachrichtigung oben rechts (Aufbau wie start/ps5cleanup.c melde(), nach ShadowMountPlus sm_platform.h) */
typedef struct { char unused[45]; char message[3075]; } pl_meldung_t;
int sceKernelSendNotificationRequest(int, pl_meldung_t *, size_t, int);
void plattform_melden(const char *text)
{
    pl_meldung_t m; memset(&m, 0, sizeof m);
    snprintf(m.message, sizeof m.message, "%s", text ? text : "");
    int e = sceKernelSendNotificationRequest(0, &m, sizeof m, 0);
    klog_printf("PS5 Cleanup: Benachrichtigung -> %d: %s\n", e, m.message);
}

/* NEU 1.0.6: Diagnoseprotokoll der Selbstentfernung = Kernel-Protokoll (liegt außerhalb der zu löschenden Daten) */
/* ------------------------------------------------------------- 1.0.6 Fix: Absturzmelder (pl_meldung_t s. o., 1.0.7) */
static const char *am_name = "PS5 Cleanup";
static volatile sig_atomic_t am_schon = 0;
static void am_melden(const char *grund)
{
    if (am_schon) _exit(1);
    am_schon = 1;
    static pl_meldung_t m;
    memset(&m, 0, sizeof m);
    snprintf(m.message, sizeof m.message, SP_EN
             ? "%s crashed: %s. Step: %s%s. Memory %.0f MB (peak %.0f MB). Please send a photo of this message to the developer (GitHub: PS5-Cleanup)."
             : "%s ist abgestürzt: %s. Schritt: %s%s. Speicher %.0f MB (Höchststand %.0f MB). Bitte ein Foto dieser Meldung an den Entwickler schicken (GitHub: PS5-Cleanup).",
             am_name, grund, basis_schritt ? basis_schritt : "?", basis_zaehlt ? (SP_EN ? ", sizes being counted" : ", Größen werden gezählt") : "",
             basis_speicher_belegt() / 1048576.0, basis_speicher_spitze() / 1048576.0);
    klog_printf("PS5 Cleanup: ABSTURZ - %s\n", m.message);
    sceKernelSendNotificationRequest(0, &m, sizeof m, 0);
    _exit(1);
}
static void am_signal(int sig)
{
    const char *g = sig == SIGSEGV ? (SP_EN ? "memory access error (SIGSEGV)" : "Speicherzugriffsfehler (SIGSEGV)")
                  : sig == SIGBUS ? (SP_EN ? "bus error (SIGBUS)" : "Busfehler (SIGBUS)")
                  : sig == SIGILL ? (SP_EN ? "illegal instruction (SIGILL)" : "ungültiger Befehl (SIGILL)")
                  : sig == SIGFPE ? (SP_EN ? "arithmetic error (SIGFPE)" : "Rechenfehler (SIGFPE)")
                  : basis_grund ? basis_grund : (SP_EN ? "aborted (SIGABRT)" : "abgebrochen (SIGABRT)");
    if (sig == SIGABRT && basis_grund && SP_EN) g = !strcmp(basis_grund, "kein Arbeitsspeicher mehr") ? "out of memory" : "unhandled error";
    am_melden(g);
}
static void am_kein_speicher(size_t n) { (void)n; am_melden(SP_EN ? "out of memory" : "kein Arbeitsspeicher mehr"); }
void plattform_absturzmelder(const char *name)
{
    am_name = name;
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = am_signal;
    sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL); sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL); sigaction(SIGABRT, &sa, NULL);
    basis_kein_speicher = am_kein_speicher;
    klog_printf("PS5 Cleanup: %s - Absturzmelder an\n", name);
}

static void diagnose_klog(const char *t) { klog_printf("PS5 Cleanup: %s\n", t ? t : ""); }

/* NEU 1.0.7 (Hänger 31.10.2026: Speicher der Konsole voll): Messung - alle 2 s ins Kernel-Protokoll, wie viel Speicher die
 * Konsole frei hat und wie viel PS5 Cleanup selbst belegt. Nur lesen (sysctl). Nur in den Testfassungen. */
extern int sceKernelAvailableFlexibleMemorySize(size_t *) __attribute__((weak));   /* Sony: verfügbarer Speicher */
#ifndef KERN_LOESCHEN   /* 1.0.8: Messung und Abfragen nur in den Testfassungen (die echte Fassung meldete sie als unbenutzt) */
static int sysctl_fehler;            /* errno der letzten fehlgeschlagenen Abfrage */
/* 31.10.2026: Puffer 8 Byte - je nach Kernel sind die Zähler 4 oder 8 Byte groß (4 Byte Puffer bei 8-Byte-Zähler = Fehler) */
static long long sysctl_wert(const char *n)
{
    unsigned long long v = 0; size_t l = sizeof v;
    if (sysctlbyname(n, &v, &l, NULL, 0) != 0) { sysctl_fehler = errno; return -1; }
    if (l == 4) return (long long)(unsigned int)v;
    return (long long)v;
}
static void *speicher_messung(void *x)
{
    (void)x;
    usleep(3000000);                 /* erst nach dem Anheben der Rechte (plattform_konsole) */
    /* einmal: welche Abfragen gehen, mit Fehlercode */
    const char *namen[] = { "vm.stats.vm.v_page_size", "vm.stats.vm.v_page_count", "vm.stats.vm.v_free_count", "vm.stats.vm.v_wire_count",
                            "vm.stats.vm.v_active_count", "vm.stats.vm.v_inactive_count", "hw.pagesize", "hw.physmem", "hw.usermem", NULL };
    for (int i = 0; namen[i]; i++) { sysctl_fehler = 0; long long w = sysctl_wert(namen[i]); klog_printf("PS5 Cleanup Speicherprobe: %s = %lld (Fehler %d)\n", namen[i], w, w < 0 ? sysctl_fehler : 0); }
    /* 31.10.2026 (Probe 2): die Konsole selbst auflisten lassen, welche Speicherwerte es gibt (alle sysctl-Namen durchgehen) */
    {
        int oid[CTL_MAXNAME]; size_t on = 1; oid[0] = 1; int gezeigt = 0, gesamt = 0;
        for (int runde = 0; runde < 20000 && gezeigt < 250; runde++) {
            int q[CTL_MAXNAME + 2]; q[0] = 0; q[1] = 2; memcpy(q + 2, oid, on * sizeof(int));
            int nx[CTL_MAXNAME]; size_t nl = sizeof nx;
            if (sysctl(q, (u_int)(on + 2), nx, &nl, NULL, 0) != 0) break;
            on = nl / sizeof(int); memcpy(oid, nx, nl); gesamt++;
            int qn[CTL_MAXNAME + 2]; qn[0] = 0; qn[1] = 1; memcpy(qn + 2, oid, on * sizeof(int));
            char name[256]; size_t ln = sizeof name - 1;
            if (sysctl(qn, (u_int)(on + 2), name, &ln, NULL, 0) != 0) continue;
            name[ln < sizeof name ? ln : sizeof name - 1] = 0;
            if (!(strstr(name, "mem") || strstr(name, "page") || strstr(name, "free") || strstr(name, "wire") || strstr(name, "swap") || !strncmp(name, "vm.", 3))) continue;
            unsigned char w[64]; size_t lw = sizeof w; long long v = -1;
            int r = sysctl(oid, (u_int)on, w, &lw, NULL, 0);
            if (r == 0 && lw == 4) v = (long long)*(unsigned int *)w; else if (r == 0 && lw == 8) v = *(long long *)w;
            klog_printf("PS5 Cleanup Speicherliste: %s = %lld (Länge %zu, Fehler %d)\n", name, v, r == 0 ? lw : 0, r ? errno : 0);
            gezeigt++;
        }
        klog_printf("PS5 Cleanup Speicherliste: %d Werte durchgesehen, %d gezeigt\n", gesamt, gezeigt);
    }
    {
        size_t fl = 0; int r = sceKernelAvailableFlexibleMemorySize ? sceKernelAvailableFlexibleMemorySize(&fl) : -999;
        klog_printf("PS5 Cleanup Speicherprobe: sceKernelAvailableFlexibleMemorySize -> 0x%08X, %zu Byte (%zu MB)\n", (unsigned)r, fl, fl / 1048576);
    }
    {
        struct vmtotal vt; size_t l = sizeof vt; int mib[2] = { CTL_VM, VM_TOTAL };
        int r = sysctl(mib, 2, &vt, &l, NULL, 0);
        klog_printf("PS5 Cleanup Speicherprobe: vm.vmtotal -> %d (Fehler %d), frei %lld Seiten, real %lld, aktiv real %lld\n", r, r ? errno : 0,
                    r ? -1LL : (long long)vt.t_free, r ? -1LL : (long long)vt.t_rm, r ? -1LL : (long long)vt.t_arm);
    }
    long long ps = sysctl_wert("vm.stats.vm.v_page_size"); if (ps <= 0) ps = sysctl_wert("hw.pagesize"); if (ps <= 0) ps = 16384;
    long long min_frei = -1;
    for (;;) {
        long long fr = sysctl_wert("vm.stats.vm.v_free_count"), wi = sysctl_wert("vm.stats.vm.v_wire_count"),
                  ge = sysctl_wert("vm.stats.vm.v_page_count"), ia = sysctl_wert("vm.stats.vm.v_inactive_count"),
                  ac = sysctl_wert("vm.stats.vm.v_active_count");
        long long mb = 1048576 / ps;
        if (fr >= 0 && (min_frei < 0 || fr < min_frei)) min_frei = fr;
        (void)fr; (void)wi; (void)ge; (void)ia; (void)ac; (void)mb;
        /* 31.10.2026: die vm.stats-Werte gibt es auf der PS5 nicht (Fehler 2) - gemessen wird Sonys eigene Angabe */
        size_t fl = 0; int fr2 = sceKernelAvailableFlexibleMemorySize ? sceKernelAvailableFlexibleMemorySize(&fl) : -999;
        static size_t fl_min = (size_t)-1; if (fr2 == 0 && fl < fl_min) fl_min = fl;
        klog_printf("PS5 Cleanup Speicher: Sony verfügbar %zu MB (tiefster %zu MB, Antwort 0x%08X) | PS5 Cleanup %zu MB (Spitze %zu MB)\n",
                    fl / 1048576, fl_min == (size_t)-1 ? 0 : fl_min / 1048576, (unsigned)fr2, basis_speicher_belegt() / 1048576, basis_speicher_spitze() / 1048576);
        usleep(2000000);
    }
    return NULL;
}
#endif

/* NEU 1.0.7 (Hänger 31.10.2026): Sicherheitsbremse für den Kern (zugriff_speicher_knapp). Höchstens alle 0,5 s neu gemessen.
 * Auslöser: PS5 Cleanup selbst über 150 MB (gemessen: bei 281 MB war die Konsole kurz vorm Einfrieren) oder Sonys freier Speicher
 * unter 100 MB (normal ca. 410 MB). */
static bool speicher_knapp(const char **grund)
{
    static struct timespec letzt; static bool knapp; static char text[200];
    struct timespec j; clock_gettime(CLOCK_MONOTONIC, &j);
    long long ms = (long long)(j.tv_sec - letzt.tv_sec) * 1000 + (j.tv_nsec - letzt.tv_nsec) / 1000000;
    if (letzt.tv_sec && ms < 500) { if (knapp) *grund = text; return knapp; }
    letzt = j; knapp = false;
    size_t eigen = basis_speicher_belegt(), fl = 0;
    bool en = plattform_englisch();
    if (eigen > (size_t)150 * 1048576) {
        snprintf(text, sizeof text, en ? "PS5 Cleanup uses %zu MB" : "PS5 Cleanup belegt %zu MB", eigen / 1048576); knapp = true;
    } else if (sceKernelAvailableFlexibleMemorySize && sceKernelAvailableFlexibleMemorySize(&fl) == 0 && fl < (size_t)100 * 1048576) {
        snprintf(text, sizeof text, en ? "only %zu MB free" : "nur noch %zu MB frei", fl / 1048576); knapp = true;
    }
    if (knapp) { *grund = text; klog_printf("PS5 Cleanup: SICHERHEITSSTOPP %s\n", text); }
    return knapp;
}
/* NEU 1.0.9 (da_Chris86 08.11.2026 08:02/08:20, private Ordner von Programmen wie Playden): einen Schritt als Besitzer ausführen. Root darf auf
 * der PS5 nicht in Ordner anderer Benutzer mit Rechten 0700 ("Root can't bypass permissions on the console", Playden). Darum: seteuid auf den
 * Besitzer, der eine Schritt, sofort zurück auf 0 - so wie Playden selbst (seteuid). Nur wenn PS5 Cleanup wirklich root ist (echte und
 * wirksame Kennung 0, siehe rechte_anheben) und nie auf root. Klappt das Zurückwechseln nicht: zugriff_rechte_kaputt - ab da verändert die
 * Zugriffsschicht nichts mehr und der Kern hört auf. Dass währenddessen keine andere Datei-Arbeit läuft, regelt die Zugriffsschicht
 * (zugriff_datei.c, Schreibsperre); dieser Riegel hält nur zwei Wechsel auseinander. In allen Fassungen: lesen als Besitzer verändert
 * nichts, löschen kann nur die echte Fassung. */
static pthread_mutex_t besitzer_m = PTHREAD_MUTEX_INITIALIZER;
static bool als_besitzer(int dfd, const char *name, unsigned uid, bool (*tu)(void *c), void *c)
{
    (void)dfd; (void)name;
    static int gemeldet = 0;
    if (uid == 0 || zugriff_rechte_kaputt) return false;
    pthread_mutex_lock(&besitzer_m);
    bool ok = false;
    if (getuid() != 0 || geteuid() != 0) {
        if (!gemeldet) { gemeldet = 1; klog_printf("PS5 Cleanup: Besitzer-Wechsel nicht möglich (uid %d, euid %d)\n", (int)getuid(), (int)geteuid()); }
    } else if (seteuid((uid_t)uid) != 0) {
        if (!gemeldet) { gemeldet = 1; klog_printf("PS5 Cleanup: Besitzer-Wechsel auf %u nicht möglich: %s\n", uid, strerror(errno)); }
    } else {
        ok = tu(c);
        int e = errno;
        if (seteuid(0) != 0 || geteuid() != 0) {
            zugriff_rechte_kaputt = true;
            klog_printf("PS5 Cleanup: SICHERHEITSSTOPP - nach dem Wechsel auf Benutzer %u nicht zurück (euid %d)\n", uid, (int)geteuid());
        }
        errno = e;
    }
    pthread_mutex_unlock(&besitzer_m);
    return ok;
}
/* NEU 1.0.10 (da_Chris86 12.11.2026 15:12-15:27): läuft Playden? Seine Webseite (ab Werk an) antwortet auf 127.0.0.1:7031-7039 mit /api/status
 * (Quellcode leolobato/playden-ps5 src/web/service.cpp, server.h first_port/last_port). Beenden mit Playdens eigenem Befehl: POST /api/quit
 * mit "X-Playden-Action: quit" (Playden beendet sich wie mit "Quit Playden", wartet auf ein Spiel, das gerade eingerichtet wird). Nur diese
 * zwei Anfragen, nur an 127.0.0.1. Rückgabe: 1 = Antwort von Playden (bzw. beendet), 0 = keine Antwort (bzw. nicht beendet), -1 = Fehler. */
static int pl_anfrage(int port, bool beenden, char *antw, size_t al)
{
    antw[0] = 0;
    int c = socket(AF_INET, SOCK_STREAM, 0);
    if (c < 0) return -1;
    struct timeval z = { 2, 0 };
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &z, sizeof z); setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &z, sizeof z);
    struct sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port); a.sin_addr.s_addr = htonl(0x7f000001);
    if (connect(c, (struct sockaddr *)&a, sizeof a) != 0) { int e = errno; close(c); return e == ECONNREFUSED ? 0 : -1; }
    char q[256];
    int ql = beenden ? snprintf(q, sizeof q, "POST /api/quit HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nX-Playden-Action: quit\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", port)
                     : snprintf(q, sizeof q, "GET /api/status HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nConnection: close\r\n\r\n", port);
    if (send(c, q, (size_t)ql, 0) != ql) { close(c); return -1; }
    size_t n = 0; ssize_t k = 0;
    while (n + 1 < al && (k = recv(c, antw + n, al - 1 - n, 0)) > 0) n += (size_t)k;
    bool fehler = k < 0;   /* Zeitüberschreitung oder abgebrochen (Codex-Prüfung 163 (2)) */
    antw[n] = 0; close(c);
    if (beenden) return strncmp(antw, "HTTP/1.1 202", 12) == 0 || strncmp(antw, "HTTP/1.0 202", 12) == 0 ? 1 : -1;
    /* nur Playden: Antwort 200 mit seinen Feldern ("kit_runtime", "redacted"). Codex-Prüfung 163 (2): eine unvollständige oder abgebrochene
     * Antwort ist unklar (-1) - nur eine vollständige Antwort eines anderen Programms zählt als "nicht Playden" (-2) */
    if ((!strncmp(antw, "HTTP/1.1 200", 12) || !strncmp(antw, "HTTP/1.0 200", 12)) && strstr(antw, "\"kit_runtime\"") && strstr(antw, "\"redacted\"")) return 1;
    {   /* Codex-Prüfung 164 (2): Antwort vollständig? Puffer voll oder weniger Inhalt als angekündigt = unklar */
        const char *kopf_ende = strstr(antw, "\r\n\r\n");
        if (n + 1 >= al) return -1;
        if (kopf_ende) {
            const char *cl = NULL;
            for (const char *q = antw; q && q < kopf_ende; q = strstr(q, "\r\n")) { q += 2; if (!strncasecmp(q, "Content-Length:", 15)) { cl = q + 15; break; } }
            if (cl && strtoul(cl, NULL, 10) > (unsigned long)(n - (size_t)(kopf_ende + 4 - antw))) return -1;
        }
    }
    if (fehler || n == 0 || !strstr(antw, "\r\n\r\n") || strncmp(antw, "HTTP/1.", 7) != 0 || strstr(antw, "\"kit_runtime\"") || strstr(antw, "layden")) return -1;
    return -2;
}
static int pl_port(int *unklar)   /* Port, auf dem Playden antwortet, 0 = keiner */
{
    char b[4096]; *unklar = 0;
    for (int p = 7031; p <= 7039; p++) { int r = pl_anfrage(p, false, b, sizeof b); if (r == 1) return p; if (r == -1) (*unklar)++; }
    return 0;
}
/* NEU 1.0.10 (Codex-Prüfung 163 (4)): läuft das Spiel mit dieser Kennung gerade? Wie spielstand_in_benutzung: seine Sandbox ist eingehängt
 * (/mnt/sandbox/<Kennung>_000/..., /mnt/sandbox/pfsmnt/<Kennung>-app0). 1 = ja, 0 = nein, -1 = nicht feststellbar. Nur lesen. */
#ifdef KERN_LOESCHEN
/* NEU 1.3: Einbindungen von ps4mount für dieses Spiel lösen. 0 = keine, 1 = gelöst, -1 = Spiel läuft / nicht prüfbar / ließ sich nicht lösen (nichts
 * verändert), -2 = ein Teil ist gelöst, ein Teil nicht (Codex-Prüfung 525 (3): das ist NICHT unverändert) */
static bool p4m_ziel(const struct statfs *m, const char *id)
{
    if (strncmp(m->f_mntfromname, "/system_data/ps4mount/", 22) != 0) return false;   /* nur ps4mounts eigene Ordner */
    char a1[48], a2[48], a3[48]; snprintf(a1, sizeof a1, "/user/app/%s", id); snprintf(a2, sizeof a2, "/user/patch/%s", id); snprintf(a3, sizeof a3, "/user/addcont/%s", id);
    const char *t = m->f_mntonname; size_t l3 = strlen(a3);
    return !strcmp(t, a1) || !strcmp(t, a2) || (!strncmp(t, a3, l3) && (t[l3] == 0 || t[l3] == '/'));
}
static int p4m_einbindungen_loesen(const char *id)
{
    if (!id || strlen(id) != 9) return -1;
    int gefunden = 0;
    for (int runde = 0; runde < 4; runde++) {   /* ps4mount kann mehrere Schichten übereinander legen (unmount_all_layers) */
        struct statfs *m = NULL; int n = getmntinfo(&m, MNT_NOWAIT);
        if (n <= 0 || !m) return gefunden ? -2 : -1;
        char ziele[8][96]; int z = 0;
        for (int i = n - 1; i >= 0 && z < 8; i--) if (p4m_ziel(&m[i], id)) { snprintf(ziele[z], sizeof ziele[z], "%s", m[i].f_mntonname); z++; }   /* zuletzt eingehängt zuerst */
        if (z == 0) break;
        if (runde == 0 && titel_laeuft(id) != 0) { sony_log(SP_EN ? "Tile %s: ps4mount mount left alone - the game is running or that could not be determined" : "Kachel %s: Einbindung von ps4mount nicht angefasst - das Spiel läuft oder das ließ sich nicht feststellen", id); return -1; }
        for (int j = 0; j < z; j++) {
            int r = unmount(ziele[j], 0), e = errno;
            sony_log(SP_EN ? "Tile %s: ps4mount mount %s %s" : "Kachel %s: Einbindung von ps4mount %s %s", id, ziele[j], r == 0 ? (SP_EN ? "released" : "gelöst") : strerror(e));
            if (r == 0) gefunden = 1;
        }
    }
    struct statfs *m = NULL; int n = getmntinfo(&m, MNT_NOWAIT);
    if (n <= 0 || !m) return gefunden ? -2 : -1;
    for (int i = 0; i < n; i++) if (p4m_ziel(&m[i], id)) return gefunden ? -2 : -1;   /* noch etwas eingehängt */
    return gefunden;
}
#endif
static int titel_laeuft(const char *id)
{
    size_t tl = id ? strlen(id) : 0;
    if (tl != 9) return -1;
    struct statfs *m = NULL;
    int n = getmntinfo(&m, MNT_NOWAIT);
    if (n <= 0 || !m) return -1;
    for (int i = 0; i < n; i++) {
        const char *nach = m[i].f_mntonname;
        if (strncmp(nach, "/mnt/sandbox/", 13) != 0) continue;
        for (const char *s = strstr(nach, id); s; s = strstr(s + 1, id))
            if ((s == nach || s[-1] == '/') && (s[tl] == '_' || s[tl] == '-' || s[tl] == '/' || s[tl] == 0)) return 1;
    }
    return 0;
}
/* NEU 1.0.10 (Codex-Prüfung 163 (3)): nimmt 127.0.0.1:port Verbindungen an? (ps4mount hält 9032, solange es läuft) 1 = ja, 0 = abgewiesen, -1 = unklar */
static int port_belegt(int port);
#ifdef KERN_LOESCHEN
/* NEU 1.0.10 (da_Chris86 12.11.2026 21:56, TabShifter TBSH00001): Homebrew-Kachel aus Sonys Datenbank austragen, nachdem Sony abgelehnt hat
 * (kern_loeschen.c kachel_db_weg, zugriff.h "SITE KACHEL-DB"). Nie die eigene Kachel. Läuft das Programm der Kachel gerade (ihre Adresse
 * http://127.0.0.1:<Port> bzw. localhost nimmt an), wird nichts geändert - es könnte die Kachel sofort wieder anlegen oder noch benutzen. */
/* NEU 1.0.10 (da_Chris86 12.11.2026 22:40, Zusatzinhalte): siehe kern/zugriff.h "SITE ZUSATZ". Sonys Liste addcont.db wird nur gelesen
 * (db_addcont_angaben, geprüfte Kopie im Speicher); ausgetragen wird nur über Sonys Dienst, danach wird nachgesehen. */
#define ZUSATZ_MAX 64
typedef struct { const char *id, *name; int n, mehr; char nm[ZUSATZ_MAX][40]; char cid[ZUSATZ_MAX][64]; } zusatz_liste;
static void zusatz_zeile(void *c, const char *t, const char *f, const char *w)
{
    zusatz_liste *z = c;
    if (strcmp(f, "ZUSATZ") != 0 || strcmp(t, z->id) != 0) return;
    const char *tab = strchr(w, '\t');
    size_t ln = tab ? (size_t)(tab - w) : strlen(w);
    if (z->name && (strlen(z->name) != ln || strncmp(w, z->name, ln) != 0)) return;
    if (z->n >= ZUSATZ_MAX || ln == 0 || ln >= sizeof z->nm[0]) { z->mehr++; return; }
    memcpy(z->nm[z->n], w, ln); z->nm[z->n][ln] = 0;
    snprintf(z->cid[z->n], sizeof z->cid[0], "%s", tab ? tab + 1 : "");
    z->n++;
}
/* -1 = nicht lesbar (*g), sonst Anzahl der Einträge für id (und name) */
static int zusatz_lesen(arena *a, zusatz_liste *z, const char **g)
{
    z->n = 0; z->mehr = 0;
    int r = db_addcont_angaben(a, plattform_db_ordner(), zusatz_zeile, z, g);
    if (r < 0) return -1;
    if (z->mehr) { *g = SA("mehr Einträge als erwartet", "more entries than expected"); return -1; }
    return z->n;
}
static antwort zusatz(arena *a, const char *arg, bool nur_pruefen)
{
    char id[10]; snprintf(id, sizeof id, "%.9s", arg);
    const char *name = strlen(arg) > 10 ? arg + 10 : NULL;
    if (!strcmp(id, SO_ID)) return sony_antwort(a, 550, SA("550 eigene Kennung - nie", "550 own ID - never"));
    zusatz_liste z; memset(&z, 0, sizeof z); z.id = id; z.name = name;
    const char *g = "";
    int n = zusatz_lesen(a, &z, &g);
    if (n < 0) return sony_antwort(a, 550, a_fmt(a, SA("550 Sonys Zusatzinhalt-Liste (addcont.db) ließ sich nicht prüfen (%s) - nichts geändert", "550 Sony's add-on list (addcont.db) could not be checked (%s) - nothing changed"), g));
    if (n == 0) return sony_antwort(a, 226, SA("226 Sony führt keinen Zusatzinhalt dazu", "226 Sony lists no add-on for it"));
    if (nur_pruefen) return sony_antwort(a, 200, a_fmt(a, SA("200 Sony führt %d Zusatzinhalt(e)", "200 Sony lists %d add-on(s)"), n));
    if (sony_gestoert) return sony_antwort(a, 550, SA("550 Sonys Dienst hat in dieser Sitzung schon einmal nicht geantwortet - nichts geändert", "550 Sony's service already failed to respond once in this session - nothing changed"));
    {
        int e = 0, b = sony_bereit(&e);
        if (b == 0) return sony_antwort(a, 550, SA("550 Sonys Dienst antwortet nicht (Initialisierung, 10 s) - nichts geändert", "550 Sony's service does not respond (initialization, 10 s) - nothing changed"));
        if (b < 0) return sony_antwort(a, 550, a_fmt(a, SA("550 Dienst nicht verfügbar 0x%08X - nichts geändert", "550 service not available 0x%08X - nothing changed"), (unsigned)e));
    }
    unsigned letzter = 0; int ok = 0;
    for (int i = 0; i < z.n; i++) {
        int e = 0;
        /* Codex-Prüfung 173 (2): keine Antwort - Sony kann schon dabei sein; das ist nie "unverändert", sondern offen (551, teilweise) */
        if (!sony_rufe2(3, id, z.nm[i], 30000, &e, NULL)) return sony_antwort_art(a, 551, a_fmt(a, SA("551 Sony antwortet nicht (Zusatzinhalt %s, 30 s) - ob Sony ihn noch austrägt, ist offen", "551 Sony does not respond (add-on %s, 30 s) - whether Sony still removes it is open"), z.nm[i]), ANTW_ANGENOMMEN);
        sony_log(SP_EN ? "Add-on %s/%s: Sony removal by name -> 0x%08X" : "Zusatzinhalt %s/%s: Sony austragen über den Namen -> 0x%08X", id, z.nm[i], (unsigned)e);
        if (e && z.cid[i][0]) {   /* wie ps4mount: dann über die content_id */
            int e2 = 0;
            if (!sony_rufe2(3, id, z.cid[i], 30000, &e2, NULL)) return sony_antwort_art(a, 551, a_fmt(a, SA("551 Sony antwortet nicht (Zusatzinhalt %s, 30 s) - ob Sony ihn noch austrägt, ist offen", "551 Sony does not respond (add-on %s, 30 s) - whether Sony still removes it is open"), z.nm[i]), ANTW_ANGENOMMEN);
            sony_log(SP_EN ? "Add-on %s/%s: Sony removal via %s -> 0x%08X" : "Zusatzinhalt %s/%s: Sony austragen über %s -> 0x%08X", id, z.nm[i], z.cid[i], (unsigned)e2);
            if (!e2) e = 0;
        }
        if (e) letzter = (unsigned)e; else ok++;
    }
    /* nachsehen: steht noch einer in Sonys Liste? (bis 30 s - Sony trägt im Hintergrund aus) */
    int rest = -1;
    for (int t = 0; t < 30; t++) {
        zusatz_liste z2; memset(&z2, 0, sizeof z2); z2.id = id; z2.name = name;
        const char *g2 = "";
        rest = zusatz_lesen(a, &z2, &g2);
        if (rest == 0) break;
        sony_warte(1000);
    }
    if (rest == 0) return sony_antwort(a, 200, a_fmt(a, SA("200 ausgetragen: %d", "200 removed: %d"), z.n));
    if (rest < 0) return sony_antwort(a, ok ? 551 : 550, ok ? SA("551 ob Sony die Zusatzinhalte ausgetragen hat, ließ sich nicht prüfen", "551 whether Sony removed the add-ons could not be checked") : SA("550 ob Sony die Zusatzinhalte ausgetragen hat, ließ sich nicht prüfen", "550 whether Sony removed the add-ons could not be checked"));
    /* Codex-Prüfung 172 (2): ein Teil ist schon weg -> 551 (teilweise), nicht "nichts geändert" */
    /* Codex-Prüfung 296 (2): Sony hat angenommen und nichts abgelehnt, die Einträge stehen nach 30 s aber noch da - Sony kann noch austragen,
     * das ist offen (551), nicht "unverändert" (wie oben bei keiner Antwort, Codex-Prüfung 173) */
    int code = (rest < z.n || (!letzter && ok)) ? 551 : 550;
    return sony_antwort_art(a, code, letzter ? a_fmt(a, SA("%d Sony hat %d von %d Zusatzinhalt(en) nicht ausgetragen (0x%08X)", "%d Sony did not remove %d of %d add-on(s) (0x%08X)"), code, rest, z.n, letzter)
                                         : a_fmt(a, SA("%d Sony hat angenommen, %d von %d Zusatzinhalt(en) stehen nach 30 s aber noch in Sonys Liste", "%d Sony accepted, but %d of %d add-on(s) are still in Sony's list after 30 s"), code, rest, z.n), letzter ? 0u : ANTW_ANGENOMMEN);
}
static antwort kachel_db(arena *a, const char *id, bool nur_pruefen)
{
    if (!strcmp(id, SO_ID) || !strcmp(id, "PSCL00002")) return sony_antwort(a, 550, SA("550 PS5 Cleanup läuft gerade über diese App", "550 PS5 Cleanup is running through this app right now"));
    char g[1200] = "", v[300] = ""; int z = 0;
    int r = kachel_db_austragen(plattform_db_ordner(), id, 1, &z, g, sizeof g, v, sizeof v);
    if (r != 0) return sony_antwort(a, 550, a_fmt(a, "550 %s", g));
    const char *p = !strncmp(v, "http://127.0.0.1:", 17) ? v + 17 : !strncmp(v, "http://localhost:", 17) ? v + 17 : NULL;
    if (p) {
        int port = atoi(p);
        int b = port > 0 && port < 65536 ? port_belegt(port) : -1;
        if (b != 0) return sony_antwort(a, 550, a_fmt(a, b == 1 ? SA("550 das Programm der Kachel läuft gerade (antwortet auf Port %d) - erst beenden, nichts geändert", "550 the tile's program is running right now (answers on port %d) - close it first, nothing changed")
                                                                 : SA("550 ob das Programm der Kachel gerade läuft, ließ sich nicht prüfen (Port %d) - nichts geändert", "550 whether the tile's program is running could not be checked (port %d) - nothing changed"), port));
    }
    if (nur_pruefen) return sony_antwort(a, 200, a_fmt(a, "200 %s", g));
    r = kachel_db_austragen(plattform_db_ordner(), id, 0, &z, g, sizeof g, NULL, 0);
    sony_log(SP_EN ? "Tile %s from Sony's database: %s" : "Kachel %s aus Sonys Datenbank: %s", id, g);
    return sony_antwort(a, r == 0 ? 200 : 550, a_fmt(a, "%d %s", r == 0 ? 200 : 550, g));
}
/* NEU 1.1 (Codex-Prüfung 328, zugriff.h "SITE EINGETRAGEN?"): führt Sony die Kennung noch - im Menü oder in app.db? Nur lesen. */
static antwort eingetragen_frage(arena *a, const char *id)
{
    if (!id || strlen(id) != 9) return sony_antwort(a, 550, SA("550 Kennung ungültig", "550 invalid ID"));
    if (sony_gestoert) return sony_antwort(a, 550, SA("550 Sonys Dienst hat in dieser Sitzung schon einmal nicht geantwortet", "550 Sony's service already failed to respond once in this session"));
    { int e = 0, b = sony_bereit(&e); if (b <= 0) return sony_antwort(a, 550, SA("550 Sonys Dienst ist nicht bereit", "550 Sony's service is not ready")); }
    int v = sony_eingetragen(id);
    if (v == 1) return sony_antwort(a, 200, SA("200 im PS5-Menü eingetragen", "200 listed in the PS5 menu"));
    if (v != 0) return sony_antwort(a, 550, SA("550 ob die Kennung eingetragen ist, ließ sich nicht prüfen", "550 whether the ID is listed could not be checked"));
    const char *g = "";
    int in_db = db_kennung_in_appdb(a, plattform_db_ordner(), id, &g);
    if (in_db == 0) return sony_antwort(a, 226, SA("226 weder im PS5-Menü noch in app.db", "226 neither in the PS5 menu nor in app.db"));
    if (in_db == 1) return sony_antwort(a, 200, SA("200 steht in app.db", "200 listed in app.db"));
    return sony_antwort(a, 550, a_fmt(a, SA("550 app.db ließ sich nicht prüfen (%s)", "550 app.db could not be checked (%s)"), g));
}
/* NEU 1.1 (zugriff.h "SITE UPDATE"; da_Chris86 15.11.2026, Vorbild ps5upload register.c uninstall_patch / storage_cleanup.c Kategorie A):
 * Update eines installierten Spiels über Sonys Dienst entfernen, danach warten, bis der Update-Ordner weg ist (bis 5 Minuten). Ist das Spiel
 * bei Sony gar nicht installiert, ist das Update nur noch ein Rest (226 - die Dateien löscht der Kern selbst, wie ps5upload Kategorie B). */
static antwort update_entfernen(arena *a, const char *pfad, bool nur_pruefen)
{
    const char *sl = pfad ? strrchr(pfad, '/') : NULL;
    if (!sl || strlen(sl + 1) != 9) return sony_antwort(a, 550, SA("550 keine Kennung", "550 no ID"));
    char id[10]; snprintf(id, sizeof id, "%s", sl + 1);
    if (!strcmp(id, SO_ID) || !strcmp(id, "PSCL00002")) return sony_antwort(a, 550, SA("550 eigene Kennung - nie", "550 own ID - never"));
    if (sony_gestoert) return sony_antwort(a, 550, SA("550 Sonys Dienst hat in dieser Sitzung schon einmal nicht geantwortet - nichts geändert", "550 Sony's service already failed to respond once in this session - nothing changed"));
    {
        int e = 0, b = sony_bereit(&e);
        if (b == 0) return sony_antwort(a, 550, SA("550 Sonys Dienst antwortet nicht (Initialisierung, 10 s) - nichts geändert", "550 Sony's service does not respond (initialization, 10 s) - nothing changed"));
        if (b < 0) return sony_antwort(a, 550, a_fmt(a, SA("550 Dienst nicht verfügbar 0x%08X - nichts geändert", "550 service not available 0x%08X - nothing changed"), (unsigned)e));
    }
    int da = sony_eingetragen(id);
    if (da == -2) return sony_antwort(a, 550, SA("550 Sonys Dienst antwortet nicht (Statusabfrage, 5 s) - nichts geändert", "550 Sony's service does not respond (status query, 5 s) - nothing changed"));
    if (da == 0) return sony_antwort(a, 226, SA("226 das Spiel ist bei Sony nicht installiert - das Update ist nur noch ein Rest", "226 the game is not installed according to Sony - the update is only a leftover"));
    if (da < 0) return sony_antwort(a, 550, SA("550 ob das Spiel installiert ist, ließ sich nicht prüfen - nichts geändert", "550 whether the game is installed could not be checked - nothing changed"));
    if (nur_pruefen) return sony_antwort(a, 200, SA("200 über Sony entfernbar", "200 removable via Sony"));
    int e = 0;
    if (!sony_rufe2(4, id, NULL, 30000, &e, NULL)) return sony_antwort_art(a, 551, SA("551 Sony antwortet nicht (Update, 30 s) - der Auftrag ist vielleicht schon angenommen, Sony kann das Update noch entfernen", "551 Sony does not respond (update, 30 s) - the request may already be accepted, Sony may still remove the update"), ANTW_ANGENOMMEN);
    sony_log(SP_EN ? "Update %s: removal via Sony -> 0x%08X" : "Update %s: über Sony entfernen -> 0x%08X", id, (unsigned)e);
    if (e) return sony_antwort(a, 550, a_fmt(a, SA("550 Sony hat das Update nicht entfernt (0x%08X)", "550 Sony did not remove the update (0x%08X)"), (unsigned)e));
    struct stat st;
    for (int t = 0; t < 300 && stat(pfad, &st) == 0; t++) sony_warte(1000);
    if (stat(pfad, &st) == 0) return sony_antwort_art(a, 551, SA("551 Sony hat angenommen, der Update-Ordner ist nach 5 Minuten aber noch da", "551 Sony accepted, but the update folder is still there after 5 minutes"), ANTW_ANGENOMMEN);
    /* Codex-Prüfung 323: nur ein nachgewiesenes Fehlen (ENOENT) zählt als entfernt - sonst ist offen, ob der Ordner noch da ist */
    if (errno != ENOENT) return sony_antwort_art(a, 551, a_fmt(a, SA("551 Sony hat angenommen, ob der Update-Ordner weg ist, ließ sich nicht prüfen (%s)", "551 Sony accepted, but whether the update folder is gone could not be checked (%s)"), strerror(errno)), ANTW_ANGENOMMEN);
    return sony_antwort(a, 200, SA("200 Update über Sony entfernt", "200 update removed via Sony"));
}
#endif
static int port_belegt(int port)
{
    int c = socket(AF_INET, SOCK_STREAM, 0);
    if (c < 0) return -1;
    struct timeval z = { 2, 0 };
    setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &z, sizeof z);
    struct sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port); a.sin_addr.s_addr = htonl(0x7f000001);
    int r = connect(c, (struct sockaddr *)&a, sizeof a) == 0 ? 1 : errno == ECONNREFUSED ? 0 : -1;
    close(c);
    return r;
}
static int playden_web(arena *a, bool beenden, const char **text)
{
    int unklar = 0, p = pl_port(&unklar);
    if (!beenden) {
        *text = p ? a_fmt(a, SA("Playden antwortet auf Port %d", "Playden answers on port %d"), p) : unklar ? a_fmt(a, SA("keine Antwort von Playden (%d Ports ohne klare Antwort)", "no answer from Playden (%d ports without a clear answer)"), unklar) : SA("keine Antwort von Playden", "no answer from Playden");
        klog_printf("PS5 Cleanup: Playden-Prüfung: %s\n", *text);
        return p ? 1 : unklar ? -1 : 0;
    }
    if (!p && unklar) { *text = SA("ob Playden läuft, ließ sich nicht feststellen", "whether Playden is running could not be determined"); return 0; }   /* Codex-Prüfung 164 (1) */
    if (!p) { *text = SA("Playden war schon zu", "Playden was already closed"); return 1; }
    char b[1024];
    int r = pl_anfrage(p, true, b, sizeof b);
    klog_printf("PS5 Cleanup: Playden beenden (Port %d): %s\n", p, r == 1 ? "angenommen" : "abgelehnt");
    if (r != 1) { *text = SA("Playden hat den Befehl zum Beenden nicht angenommen", "Playden did not accept the command to quit"); return 0; }
    for (int i = 0; i < 120; i++) {   /* Playden wartet evtl. auf ein Spiel, das gerade eingerichtet wird */
        sleep(1);
        int u2 = 0;
        if (!pl_port(&u2) && !u2) { sleep(2); if (!pl_port(&u2) && !u2) { *text = a_fmt(a, SA("Playden hat sich nach %d s beendet", "Playden quit after %d s"), i + 3); klog_printf("PS5 Cleanup: %s\n", *text); return 1; } }
    }
    *text = SA("Playden hat sich nach 120 s nicht beendet", "Playden did not quit after 120 s");
    return 0;
}
#ifdef WEG_DIAGNOSE
/* NEU 1.1.1 (da_Chris86 17.11.2026: "eine ausschließlich lesende Diagnosefassung für Firmware 13.20 ... unkompliziert und ungefährlich").
 * Rückmeldungen #9 (FW 13.20), #13 und #15 (FW 13.60): jedes Löschen scheiterte schon beim Weg zum Ziel. Diese Fassung kann nichts löschen
 * (Fassung ohne KERN_LOESCHEN, Schreibsperre in Kern und Oberfläche) und geht nach dem Ermitteln der Konsole genau den Weg, den das Löschen
 * öffnet - Ordner für Ordner, wie eltern_oeffnen -, und schreibt jeden Aufruf ins Protokoll: ROH = so, wie der PS5-Kern antwortet
 * (Rückgabewert rax und Carry-Flag; Carry 1 = Fehler, rax = Fehlernummer) - genau das, was die SDK-Stubs bis 1.1 als "Datei-Griff"
 * weitergaben. Benutzt nur: open/openat lesend (O_RDONLY), fstat, fstatat, fstatfs, close - nichts davon verändert etwas.
 * Bei einem Fehler zusätzlich Gegenproben (ohne O_NOFOLLOW, ohne O_CLOEXEC, ohne O_DIRECTORY, ganzer Pfad über openat, ganzer Pfad über
 * open aus Sonys Bibliothek), damit klar wird, welcher Teil des Aufrufs abgelehnt wird. Ein Griff wird nur geschlossen, wenn der Kern
 * ihn ohne Carry-Flag geliefert hat (also wirklich unserer ist). */
static void (*wd_zeile)(void *c, const char *t); static void *wd_c;
static void wd(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void wd(const char *fmt, ...) { char t[700]; va_list v; va_start(v, fmt); vsnprintf(t, sizeof t, fmt, v); va_end(v); wd_zeile(wd_c, t); }
static const char *wd_err(long r, int cf) { return cf ? strerror((int)r) : "ok"; }
/* ein openat roh, mit Ausgabe; liefert den Griff (>= 0) nur, wenn der Kern ihn ohne Fehler geliefert hat */
static int wd_openat(int fd, const char *ort, const char *name, int fl, const char *was)
{
    int cf = 0; long r = ps5c_roh(499, fd, (long)name, fl, 0, &cf);
    wd("   %s openat(%s, \"%s\", 0x%x) ROH rax=%ld carry=%d (%s)", was, ort, name, fl, r, cf, wd_err(r, cf));
    return cf ? -1 : (int)r;
}
static void wd_ansehen(int fd, const char *wer)
{
    struct stat st; struct statfs sf; int cf = 0; long r;
    memset(&st, 0, sizeof st);
    errno = 0; int r2 = fstat(fd, &st);   /* fstat kommt aus Sonys Bibliothek (wertet Fehler richtig aus) */
    wd("   fstat(%s) Sony = %d errno %d  Geraet %llu Knoten %llu Art %o Besitzer %u Rechte %o", wer, r2, r2 ? errno : 0,
       (unsigned long long)st.st_dev, (unsigned long long)st.st_ino, (unsigned)(st.st_mode & S_IFMT), (unsigned)st.st_uid, (unsigned)(st.st_mode & 07777));
    cf = 0; r = ps5c_roh(397, fd, (long)&sf, 0, 0, &cf);
    if (cf) wd("   fstatfs(%s) ROH rax=%ld carry=1 (%s)", wer, r, wd_err(r, cf));
    else wd("   fstatfs(%s) ROH rax=%ld carry=0  Dateisystem %s auf %s von %s", wer, r, sf.f_fstypename, sf.f_mntonname, sf.f_mntfromname);
}
static void wd_weg(const char *pfad)
{
    wd("== Weg zu %s", pfad);
    errno = 0;
    int fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    wd("   open(\"/\") Sony = %d errno %d (%s)", fd, fd < 0 ? errno : 0, fd < 0 ? strerror(errno) : "ok");
    if (fd < 0) return;
    wd_ansehen(fd, "/");
    char ort[512] = "/", kopie[512]; snprintf(kopie, sizeof kopie, "%s", pfad);
    char *sp = NULL;
    for (char *teil = strtok_r(kopie, "/", &sp); teil; teil = strtok_r(NULL, "/", &sp)) {
        struct stat ks; int cf = 0;
        long r = ps5c_roh(493, fd, (long)teil, (long)&ks, AT_SYMLINK_NOFOLLOW, &cf);
        wd("   fstatat(%s, \"%s\", NOFOLLOW) ROH rax=%ld carry=%d (%s)%s", ort, teil, r, cf, wd_err(r, cf),
           cf ? "" : (S_ISLNK(ks.st_mode) ? "  -> Verknüpfung" : S_ISDIR(ks.st_mode) ? "  -> Ordner" : "  -> keine Ordner"));
        int nfd = wd_openat(fd, ort, teil, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC, "wie beim Löschen:");
        char weiter[512]; snprintf(weiter, sizeof weiter, "%s%s%s", ort, strcmp(ort, "/") ? "/" : "", teil);
        if (nfd < 0) {
            wd("   -> abgelehnt. Gegenproben:");
            int g;
            if ((g = wd_openat(fd, ort, teil, O_RDONLY | O_DIRECTORY | O_CLOEXEC, "ohne NOFOLLOW:")) >= 0) close(g);
            if ((g = wd_openat(fd, ort, teil, O_RDONLY | O_DIRECTORY | O_NOFOLLOW, "ohne CLOEXEC:")) >= 0) close(g);
            if ((g = wd_openat(fd, ort, teil, O_RDONLY | O_NOFOLLOW | O_CLOEXEC, "ohne DIRECTORY:")) >= 0) close(g);
            if ((g = wd_openat(fd, ort, teil, O_RDONLY, "nur RDONLY:")) >= 0) close(g);
            if ((g = wd_openat(AT_FDCWD, "AT_FDCWD", weiter, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC, "ganzer Pfad:")) >= 0) close(g);
            errno = 0; g = open(weiter, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
            wd("   ganzer Pfad über Sonys open(\"%s\") = %d errno %d (%s)", weiter, g, g < 0 ? errno : 0, g < 0 ? strerror(errno) : "ok");
            if (g >= 0) { wd_ansehen(g, weiter); close(g); }
            close(fd);
            return;
        }
        wd_ansehen(nfd, weiter);
        struct stat ns;
        if (!cf && fstat(nfd, &ns) == 0) wd("   derselbe Ordner wie angesehen: %s", (ns.st_dev == ks.st_dev && ns.st_ino == ks.st_ino) ? "ja" : "NEIN");
        close(fd); fd = nfd; snprintf(ort, sizeof ort, "%s", weiter);
    }
    close(fd);
    wd("   -> ganzer Weg geöffnet");
}
static void wegdiagnose(void (*zeile)(void *c, const char *t), void *c)
{
    wd_zeile = zeile; wd_c = c;
    wd("WEG-DIAGNOSE 1.1.1 (nur lesen - diese Fassung kann nichts löschen)");
    wd("Prozess: pid %d uid %d euid %d gid %d", (int)getpid(), (int)getuid(), (int)geteuid(), (int)getgid());
    uint8_t caps[16];
    if (kernel_get_ucred_caps(getpid(), caps) == 0) wd("Rechte (caps): %02x %02x %02x %02x %02x %02x %02x %02x", caps[0], caps[1], caps[2], caps[3], caps[4], caps[5], caps[6], caps[7]);
    /* NEU 18.11.2026 (Vergleich mit ps5upload, läuft auf FW 13.60: setzt Wurzel- UND Gefängnis-Verzeichnis seines Prozesses auf die echte
     * Wurzel; unser Lader start/fremd/websrv/elfldr.c setzt das Gefängnis-Verzeichnis auf 0 und elfldr_exec stellt danach die Werte des
     * Trägerprozesses wieder her). Nur lesen (Kernel-Speicher lesen), ändert nichts. */
    {
        unsigned long rv = (unsigned long)kernel_get_root_vnode();
        unsigned long rd = (unsigned long)kernel_get_proc_rootdir(getpid()), jd = (unsigned long)kernel_get_proc_jaildir(getpid());
        wd("Prozess-Wurzel: echte Wurzel %#lx  Wurzel-Verzeichnis %#lx (%s)  Gefängnis-Verzeichnis %#lx (%s)", rv, rd,
           !rv ? "nicht lesbar" : rd == rv ? "= echte Wurzel" : "ANDERS", jd, !rv ? "nicht lesbar" : jd == rv ? "= echte Wurzel" : jd == 0 ? "leer" : "ANDERS");
        struct stat a, b; int ra = stat("/", &a), rb2 = stat("/user", &b);
        wd("stat(\"/\") = %d Gerät %llu Knoten %llu   stat(\"/user\") = %d Gerät %llu Knoten %llu", ra, (unsigned long long)a.st_dev, (unsigned long long)a.st_ino,
           rb2, (unsigned long long)b.st_dev, (unsigned long long)b.st_ino);
    }
    static const char *wege[] = { "/user/data", "/user/home", "/user/app", "/user/av_contents", "/system_data/priv", "/system_data/httpcache", "/data", "/mnt/usb0", NULL };
    for (int i = 0; wege[i]; i++) wd_weg(wege[i]);
    wd("WEG-DIAGNOSE ENDE - bitte die Rückmeldung senden (Knopf \"Rückmeldung\").");
}
#endif
void plattform_start(void)
{
#ifdef WEG_DIAGNOSE
    zugriff_wegdiagnose = wegdiagnose;   /* NEU 1.1.1: nur die Diagnose-Fassung (nur lesen) */
#endif
    zugriff_speicher_knapp = speicher_knapp;   /* NEU 1.0.7: Sicherheitsbremse (alle Fassungen) */
    zugriff_als_besitzer = als_besitzer;       /* NEU 1.0.9: private Ordner von Programmen (alle Fassungen, siehe oben) */
#ifndef KERN_LOESCHEN
    { pthread_t t; if (pthread_create(&t, NULL, speicher_messung, NULL) == 0) pthread_detach(t); }
#endif
    sceSystemServiceHideSplashScreen();
    zugriff_sony_deinstallieren = sony_deinstallieren; zugriff_startordner_entfernen = startordner_weg;
    zugriff_einbindung = einbindung_abfragen;   /* NEU 1.0.7: nur lesen (statfs) - in allen Fassungen */
    zugriff_medium_loeschen = medium_loeschen; zugriff_spielstand_austragen = spielstand_austragen;   /* NEU 1.0.7 (01.11.2026): in allen Fassungen - löschen nur die echte */
    zugriff_diagnose = diagnose_klog;
    zugriff_playden = playden_web;   /* NEU 1.0.10: nur fragen in allen Fassungen - beenden schickt nur die echte (zugriff_datei.c, KERN_LOESCHEN) */
    zugriff_titel_laeuft = titel_laeuft; zugriff_port_belegt = port_belegt;   /* NEU 1.0.10 (Codex-Prüfung 163): nur fragen */
#ifdef KERN_LOESCHEN
    zugriff_selbst_uebergeben = selbst_uebergeben;   /* nur die echte Fassung - die Simulation übergibt nie */
    kern_ps4_sony_eingetragen = sony_ps4_eingetragen;   /* NEU 1.0.7: PS4-Inventar - Menüeintrag nur abfragen */
    zugriff_system_ex_oeffnen = system_ex_oeffnen;   /* NEU 1.0.7: nur die echte Fassung */
    zugriff_kachel_db = kachel_db;   /* NEU 1.0.10: nur die echte Fassung (eigene SQLite mit Dateizugriff) */
    zugriff_zusatz = zusatz;           /* NEU 1.0.10 (22:40): Zusatzinhalte zuerst über Sony austragen, in addcont.db nachgesehen */
    zugriff_update = update_entfernen; /* NEU 1.1: Update eines installierten Spiels über Sony */
    zugriff_eingetragen = eingetragen_frage;   /* NEU 1.1 (Codex-Prüfung 328): nur lesen */
#endif
}
/* NEU 1.0.6 (da_Chris86 29.10.2026 23:08; seit 1.0.8 in allen Fassungen, auch zum Beenden): Auftrag an den Hintergrundteil von ps5cleanup.elf (127.0.0.1:9099, start/ps5cleanup.c).
 * 1 = Antwort enthält "erwartet", 0 = keine Verbindung, -1 = andere Antwort (Text in t) */
static int hintergrund_fragen(const char *pfad, const char *erwartet, char *t, size_t tl)
{
    t[0] = 0;
    int c = socket(AF_INET, SOCK_STREAM, 0);
    if (c < 0) { snprintf(t, tl, "kein Socket (%s)", strerror(errno)); return 0; }
    struct timeval z = { 3, 0 };
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &z, sizeof z); setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &z, sizeof z);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons(9099); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(c, (struct sockaddr *)&a, sizeof a)) { snprintf(t, tl, "keine Verbindung zu 127.0.0.1:9099 (%s)", strerror(errno)); close(c); return 0; }
    char anf[400]; snprintf(anf, sizeof anf, "GET %s HTTP/1.0\r\n\r\n", pfad);
    if (send(c, anf, strlen(anf), 0) < 0) { snprintf(t, tl, "Senden fehlgeschlagen (%s)", strerror(errno)); close(c); return 0; }
    char p[1024]; size_t n = 0;
    for (;;) { ssize_t r = recv(c, p + n, sizeof p - 1 - n, 0); if (r <= 0) break; n += (size_t)r; if (n >= sizeof p - 1) break; }
    p[n] = 0; close(c);
    if (strstr(p, erwartet)) { snprintf(t, tl, "%s", erwartet); return 1; }
    const char *k = strstr(p, "\r\n\r\n");
    snprintf(t, tl, "Antwort ohne Bestätigung: %.80s%s%.60s", n ? p : "(leer)", k && k[4] ? " / " : "", k && k[4] ? k + 4 : "");
    for (char *q = t; *q; q++) if (*q == '\r' || *q == '\n') *q = ' ';
    return -1;
}
/* NEU 1.0.8 (Arbeitsplan 3.2, PLAN 1.0.8 Punkt 8): sauber beenden. Bis 1.0.7 beendete sich die Oberfläche wie der SDL-Baukasten mit
 * sceSystemServiceLoadExec("exit") - die App wurde vollständig beendet, nur das Starten des Nachfolgers "exit" meldete die PS5 als Fehler
 * ("Exit spawn failed ... ELF path=exit"). Jetzt bittet sie den Hintergrundteil (start/ps5cleanup.c), sie von außen zu beenden
 * (sceSystemServiceKillApp, wie ps5-payload-dev/websrv hbldr.c beim Wechsel der App). Antwortet er nicht (z. B. ein älterer Hintergrundteil
 * oder keiner) oder läuft sie nach 5 s noch: wie bisher LoadExec("exit"). */
static bool hintergrund_beendet_uns(void)
{
    char pfad[64], t[300];
    snprintf(pfad, sizeof pfad, "/ps5cleanup-schliessen?pid=%d", (int)getpid());
    int r = hintergrund_fragen(pfad, "PS5CLEANUP-BEENDE-JETZT", t, sizeof t);
    klog_printf("PS5 Cleanup: Beenden über den Hintergrundteil -> %d %s\n", r, t);
    return r == 1;
}
void plattform_ende(void)
{
    /* NEU 1.0.7: /system_ex zurückstellen, wenn PS5 Cleanup es in dieser Sitzung beschreibbar gemacht hat - außer der
     * Hintergrundteil entfernt gleich noch den Startordner (er stellt danach selbst zurück) */
#ifdef KERN_LOESCHEN   /* nur die echte Fassung verändert etwas (Prüf-/Simulationsfassung: Sony stellt beim Neustart zurück) */
    if (!selbst_uebergeben_ok) { int r = so_zurueck(); klog_printf("PS5 Cleanup: /system_ex zurückstellen am Ende -> %d\n", r); }
#endif
    if (hintergrund_beendet_uns()) {
        for (int i = 0; i < 50; i++) usleep(100000);       /* gleich beendet uns der Hintergrundteil */
        klog_printf("PS5 Cleanup: nach 5 s noch nicht beendet - beende wie bisher\n");
    }
    sceSystemServiceLoadExec("exit", NULL);
}
const char *plattform_name(void) { return "PS5"; }

bool plattform_englisch(void)
{
    int v = SPRACHE_DEUTSCH;
    if (sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_LANG, &v) != 0) return false;
    return v != SPRACHE_DEUTSCH;
}

static char ort[1024];
const char *plattform_eigener_ort(void)
{
    if (!ort[0] && !getcwd(ort, sizeof ort)) ort[0] = 0;
    if (!strcmp(ort, "/")) ort[0] = 0;                 /* kein eigener Ordner bekannt */
    return ort;
}
int plattform_upd_suchorte(const char **o, int max)   /* NEU 1.3 - siehe plattform.h */
{
    static const char *const usb[] = { "/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3", "/mnt/usb4", "/mnt/usb5", "/mnt/usb6", "/mnt/usb7" };
    int n = 0;
    for (int i = 0; i < 8 && n < max; i++) o[n++] = usb[i];
    if (n < max) o[n++] = "/data";
    const char *e = plattform_eigener_ort();   /* der Laufzeitbereich der eigenen Kachel (/mnt/sandbox/...) ist kein Speicherort */
    if (e && e[0] == '/' && strncmp(e, "/mnt/sandbox", 12) && n < max) o[n++] = e;
    return n;
}

zugriff *plattform_zugriff(void)
{
    datei_optionen o = { .wurzel = "" };
#ifdef KERN_LOESCHEN
    return zugriff_datei(&o);                         /* Release-Bau (bauen.sh Fassung "release"): echtes Löschen */
#else
    return zugriff_sim(zugriff_datei(&o));            /* PS5-Bau ohne KERN_LOESCHEN -> kann gar nicht löschen */
#endif
}

/* wie ps5info.c: Rechte so weit anheben, dass /dev/ddd geöffnet werden darf (nur dieser Prozess) */
static void rechte_anheben(void)
{
    uint8_t caps[16];
    pid_t pid = getpid();
    kernel_set_ucred_authid(pid, 0x4800000000000010ULL);
    if (kernel_get_ucred_caps(pid, caps) == 0) { caps[7] |= 0x40; kernel_set_ucred_caps(pid, caps); }
    setuid(0);
}
static int laufwerk_status(void)
{
    int fd = open("/dev/ddd", O_RDWR);
    if (fd < 0) return -1;
    ddd_abfrage_t a; uint32_t aus = DDD_UNGUELTIG;
    memset(&a, 0, sizeof a);
    a.aus_laenge = (int32_t)sizeof aus; a.aus_puffer = (uint8_t *)&aus;
    int r = ioctl(fd, DDD_STATUS_IOCTL, &a);
    close(fd);
    return ddd_antwort_pruefen(r, a.status, aus);
}

void plattform_konsole(konsole_info *k)
{
    memset(k, 0, sizeof *k);
    struct sigaction sa, alt1, alt2; memset(&sa, 0, sizeof sa); sa.sa_handler = bei_fehler;
    sigaction(SIGSEGV, &sa, &alt1); sigaction(SIGBUS, &sa, &alt2);
    char m[256] = {0};
    if (sigsetjmp(g_jb, 1) == 0) { if (sceKernelGetHwModelName(m) == 0) snprintf(k->modell, sizeof k->modell, "%s", m); }
    SwVersion v; memset(&v, 0, sizeof v); v.size = sizeof v;
    if (sigsetjmp(g_jb, 1) == 0 && sceKernelGetProsperoSystemSwVersion(&v) == 0 && v.version)
        snprintf(k->firmware, sizeof k->firmware, "%x.%02x", (v.version >> 24) & 0xFF, (v.version >> 16) & 0xFF);
    int st = -1;
    if (sigsetjmp(g_jb, 1) == 0) { rechte_anheben(); st = laufwerk_status(); }
    k->ddd_bekannt = true; k->ddd = st;         /* wie 1.0.5: Antwort da, -1 = nicht auslesbar */
    sigaction(SIGSEGV, &alt1, NULL); sigaction(SIGBUS, &alt2, NULL);
}

/* NEU 1.0.7 (Infoleiste "Speicher frei"): nur lesen (statfs). M.2/USB zählen nur, wenn dort wirklich etwas eingehängt ist -
 * sonst würde statfs den darüberliegenden Speicher melden. */
/* NEU 1.0.7 (da_Chris86 02.11.2026 15:45 / 16:57): Protokoll ohne USB-Stick - eigener Ordner /data/PS5-Cleanup-Protokolle, jeder Lauf
 * eine eigene Datei mit Datum und Uhrzeit (wie auf dem Stick). NICHT im Ordner von PS5 Cleanup - der geht bei der Selbstentfernung weg.
 * Nie angeboten, nie gelöscht (kern_schutz.c, kern_suche.c); der Hintergrundteil hängt bei der Selbstentfernung seine Schlusszeilen
 * an (start/sw_protokoll.c). Nur die echte Fassung legt den Ordner an - einzig dieses mkdir in plattform_ps5.o (app/bauen.sh prüft; die Test-Fassung
 * mit Mitschnitt legt zusätzlich ihren Ordner PS5-Cleanup-Test an, plattform_test_ordner). */
long long plattform_frei_bytes(const char *pfad)
{
    struct statfs s;
    if (!pfad || statfs(pfad, &s) != 0 || s.f_blocks == 0) return -1;
    return (long long)s.f_bavail * (long long)s.f_bsize;
}
#ifdef MITSCHNITT
/* NEU 1.0.8 (Arbeitsplan 2.7): nur die Test-Fassung - Ordner PS5-Cleanup-Test für Mitschnitt und Bildschirmbilder: auf dem USB-Laufwerk,
 * ohne USB-Laufwerk unter /data. Neu angelegt oder schon da und ein echter Ordner (kein Verweis); nie etwas darin gelöscht. */
const char *plattform_test_ordner(void)
{
    static char p[64];
    const char *o = plattform_stick_ordner();
    const char *wo[2] = { o, "/data" };
    for (int i = 0; i < 2; i++) {
        if (!wo[i]) continue;
        snprintf(p, sizeof p, "%s/PS5-Cleanup-Test", wo[i]);
        struct stat st;
        zugriff_wechsel_sperren();   /* NEU 1.0.10: nie mitten in einem Besitzer-Wechsel anlegen */
        bool neu_ok = lstat(p, &st) == 0 || mkdir(p, 0755) == 0 || errno == EEXIST;
        int e = errno;
        zugriff_wechsel_frei();
        if (!neu_ok) { klog_printf("PS5 Cleanup: %s nicht anlegbar: %s\n", p, strerror(e)); continue; }
        if (lstat(p, &st) == 0 && S_ISDIR(st.st_mode)) return p;
    }
    return NULL;
}
#endif
/* da_Chris86 02.11.2026 17:23: USB-Stick -> interne Platte -> M.2 (nur wenn die interne Platte voll oder nicht beschreibbar ist) */
const char *plattform_protokoll_ordner(int nr)
{
    static const char *const O[2] = { "/data/PS5-Cleanup-Protokolle", "/mnt/ext1/PS5-Cleanup-Protokolle" };
    if (nr < 0 || nr > 1) return NULL;
    const char *o = O[nr];
#ifdef KERN_LOESCHEN
    long long frei = -1, ges = 0;
    if (nr == 0) frei = plattform_frei_bytes("/user");                    /* /data liegt auf /user */
    else if (!plattform_speicher(1, &frei, &ges)) { klog_printf("PS5 Cleanup: keine M.2 eingehängt - kein Protokoll-Ordner dort\n"); return NULL; }
    if (frei >= 0 && frei < PLATTFORM_PROT_MIN_FREI) { klog_printf("PS5 Cleanup: %s - zu wenig Platz (%lld Byte)\n", o, frei); return NULL; }
    struct stat st;
    if (lstat(o, &st) != 0) {
        if (mkdir(o, 0755) != 0 && errno != EEXIST) { klog_printf("PS5 Cleanup: Protokoll-Ordner %s nicht anlegbar: %s\n", o, strerror(errno)); return NULL; }
        if (lstat(o, &st) != 0) return NULL;
    }
    if (!S_ISDIR(st.st_mode)) { klog_printf("PS5 Cleanup: %s ist kein Ordner - nicht benutzt\n", o); return NULL; }   /* auch kein Verweis */
#endif
    return o;
}
const char *plattform_stick_ordner(void)
{
    static char o[16];
    for (int i = 0; i < 8; i++) {
        snprintf(o, sizeof o, "/mnt/usb%d", i);
        struct statfs s;
        if (statfs(o, &s) == 0 && !strcmp(s.f_mntonname, o) && s.f_blocks > 0 && !(s.f_flags & MNT_RDONLY)) return o;
    }
    return NULL;
}
const char *plattform_db_ordner(void) { return "/system_data/priv/mms"; }
/* NEU 1.0.7 (PS5-Test 02.11.2026): appinfo.db nur lesen und die Zeilen an den Kern geben (siehe plattform.h) */
typedef struct { kern *k; void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert); } appinfo_ctx;
static void appinfo_cb(void *c, const char *titel, const char *feld, const char *wert) { appinfo_ctx *x = c; x->zeile(x->k, titel, feld, wert); }
void plattform_appinfo_lesen(struct kern *k, void (*zeile)(struct kern *k, const char *titel, const char *feld, const char *wert))
{
    const char *q = plattform_db_ordner();
    if (!k || !zeile || !q) return;
    appinfo_ctx c = { k, zeile };
    const char *g = "";
    int r = db_appinfo_angaben(k->a, q, appinfo_cb, &c, &g);
    klog_printf("PS5 Cleanup: appinfo.db (nur lesen) - %d Zeilen mit Zuordnungen/Zusatzinhalt-Angaben%s%s\n", r, g[0] ? " - " : "", g);
    /* NEU 1.0.10 (da_Chris86 10.11.2026): app.db je Titel für die Datenbank-Übersicht im Protokoll - nur lesen */
    g = "";
    int t = db_appdb_titel(k->a, q, appinfo_cb, &c, &g);
    klog_printf("PS5 Cleanup: app.db (nur lesen) - %d Titel%s%s\n", t, g[0] ? " - " : "", g);
    /* NEU 1.0.8 (PS5-Test 07.11.2026): Sonys Spielstand-Liste je Benutzer (/system_data/savedata/<Benutzer>/db/user/savedata.db), nur lesen -
     * die Titel der Spielstände nennen meist das Spiel, auch wenn es nicht mehr installiert ist */
    DIR *d = opendir("/system_data/savedata");
    struct dirent *e; int nb = 0;
    while (d && (e = readdir(d)) != NULL && nb < 16) {
        if (e->d_name[0] == '.') continue;
        char p[256]; snprintf(p, sizeof p, "/system_data/savedata/%s/db/user/savedata.db", e->d_name);
        struct stat st;
        if (stat(p, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        nb++; g = "";
        int s = db_spielstand_angaben(k->a, p, appinfo_cb, &c, &g);
        klog_printf("PS5 Cleanup: %s (nur lesen) - %d Spielstand-Titel%s%s\n", p, s, g[0] ? " - " : "", g);
    }
    if (d) closedir(d);
}
void plattform_protokoll_zeile(const char *t) { klog_printf("PS5 Cleanup Protokoll: %s\n", t ? t : ""); }

/* Codex-Prüfung 91: alle USB-Laufwerke zusammen - die Funde auf /mnt/usb1 standen sonst am Balken von /mnt/usb0 */
static int usb_summe(long long *frei, long long *gesamt)
{
    int n = 0; long long f = 0, g = 0;
    for (int i = 0; i < 8; i++) {
        char p[16]; snprintf(p, sizeof p, "/mnt/usb%d", i);
        struct statfs s;
        if (statfs(p, &s) != 0 || strcmp(s.f_mntonname, p) != 0 || s.f_blocks == 0) continue;
        f += (long long)s.f_bavail * (long long)s.f_bsize; g += (long long)s.f_blocks * (long long)s.f_bsize; n++;
    }
    if (n > 0 && frei && gesamt) { *frei = f; *gesamt = g; }
    return n;
}
int plattform_usb_anzahl(void) { return usb_summe(NULL, NULL); }
bool plattform_speicher(int ort, long long *frei, long long *gesamt)
{
    static const char *const PFAD[4] = { "/user", "/mnt/ext1", "/mnt/ext0", "/mnt/usb0" };
    if (ort < 0 || ort > 3) return false;
    if (ort == 3) return usb_summe(frei, gesamt) > 0;
    struct statfs s;
    if (statfs(PFAD[ort], &s) != 0) return false;
    if (ort != 0 && strcmp(s.f_mntonname, PFAD[ort]) != 0) return false;
    if (s.f_blocks == 0) return false;
    *frei = (long long)s.f_bavail * (long long)s.f_bsize;
    *gesamt = (long long)s.f_blocks * (long long)s.f_bsize;
    return true;
}

/* =================================================================== NEU 1.0.9: Update-Hinweis - neueste Version bei GitHub abfragen
 * da_Chris86 08.11.2026 01:16: bei jedem Start. Nur lesen: eine HTTPS-Anfrage an api.github.com, gesendet wird nur die Anfrage selbst
 * (Programmname "PS5-Cleanup/<Version>" und zwei feste Kopfzeilen). Sonys HTTP-Bibliothek wird erst hier geladen (dlopen) und über die
 * Kennungen (NID) ihrer Funktionen gefunden - wie Sonys Löschdienst. Fehlt etwas, gibt es nur keinen Hinweis; der Start bleibt, wie er war.
 * Das Zertifikat von GitHub prüft Sonys Bibliothek selbst; nur wenn die PS5 die Ausstellerstelle nicht kennt, ein zweiter Versuch ohne (unten). Die NIDs sind aus den Funktionsnamen berechnet; das
 * Verfahren ist mit drei Kennungen gegengeprüft, die dieses Programm schon benutzt (sceContentDeleteInitialize zoxb0wEChEM,
 * sceContentDeleteByPath qC3QW+AltHs, sceAppInstUtilAppInstallTitleDir Wudg3Xe3heE). Aufbau der Aufrufe wie Sonys HTTP-Bibliothek auf
 * der PS4 (gleiche Funktionen). */
#include <dlfcn.h>
#include "cJSON.h"
#define UPD_URL "https://api.github.com/repos/xXBneutzerNameXx/PS5-Cleanup/releases/latest"
#define UPD_MAX (512 * 1024)   /* die Antwort zu 1.0.8 hat gut 10 KB */
enum { U_NETINIT, U_POOL, U_POOLWEG, U_SSLINIT, U_SSLENDE, U_HTTPINIT, U_HTTPENDE, U_VORLAGE, U_VORLAGEWEG, U_VERB, U_VERBWEG, U_ANFR,
       U_ANFRWEG, U_KOPF, U_ZAUF, U_ZVERB, U_ZSEND, U_ZEMPF, U_SENDEN, U_STATUS, U_LESEN, U_ANZ };
static const struct { const char *lib, *nid, *name; } UPD_NID[U_ANZ] = {
    { "libSceNet.sprx", "Nlev7Lg8k3A", "sceNetInit" }, { "libSceNet.sprx", "dgJBaeJnGpo", "sceNetPoolCreate" },
    { "libSceNet.sprx", "K7RlrTkI-mw", "sceNetPoolDestroy" },
    { "libSceSsl.sprx", "hdpVEUDFW3s", "sceSslInit" }, { "libSceSsl.sprx", "0K1yQ6Lv-Yc", "sceSslTerm" },
    { "libSceHttp.sprx", "A9cVMUtEp4Y", "sceHttpInit" }, { "libSceHttp.sprx", "Ik-KpLTlf7Q", "sceHttpTerm" },
    { "libSceHttp.sprx", "0gYjPTR-6cY", "sceHttpCreateTemplate" }, { "libSceHttp.sprx", "4I8vEpuEhZ8", "sceHttpDeleteTemplate" },
    { "libSceHttp.sprx", "qgxDBjorUxs", "sceHttpCreateConnectionWithURL" }, { "libSceHttp.sprx", "P6A3ytpsiYc", "sceHttpDeleteConnection" },
    { "libSceHttp.sprx", "Aeu5wVKkF9w", "sceHttpCreateRequestWithURL" }, { "libSceHttp.sprx", "qe7oZ+v4PWA", "sceHttpDeleteRequest" },
    { "libSceHttp.sprx", "EY28T2bkN7k", "sceHttpAddRequestHeader" },
    { "libSceHttp.sprx", "Tc-hAYDKtQc", "sceHttpSetResolveTimeOut" }, { "libSceHttp.sprx", "0S9tTH0uqTU", "sceHttpSetConnectTimeOut" },
    { "libSceHttp.sprx", "xegFfZKBVlw", "sceHttpSetSendTimeOut" }, { "libSceHttp.sprx", "yigr4V0-HTM", "sceHttpSetRecvTimeOut" },
    { "libSceHttp.sprx", "1e2BNwI-XzE", "sceHttpSendRequest" }, { "libSceHttp.sprx", "0a2TBNfE3BU", "sceHttpGetStatusCode" },
    { "libSceHttp.sprx", "P5pdoykPYTk", "sceHttpReadData" } };
/* NEU 1.0.9: Vorlage mit Zeitgrenzen (einmal am Anfang, für den zweiten Versuch eine frische - PS5-Test 08.11.2026 22:54: an der schon
 * benutzten Vorlage lehnte die PS5 das Abschalten ab) */
static int upd_vorlage(void **fn, int http, const char *ua)
{
    int v = ((int (*)(int, const char *, int, int))fn[U_VORLAGE])(http, ua, 2 /* HTTP 1.1 */, 1 /* Proxy wie die PS5 */);
    if (v < 0) return v;
    ((int (*)(int, uint32_t))fn[U_ZAUF])(v, 5u * 1000 * 1000);    /* Mikrosekunden */
    ((int (*)(int, uint32_t))fn[U_ZVERB])(v, 5u * 1000 * 1000);
    ((int (*)(int, uint32_t))fn[U_ZSEND])(v, 5u * 1000 * 1000);
    ((int (*)(int, uint32_t))fn[U_ZEMPF])(v, 8u * 1000 * 1000);
    return v;
}
static volatile unsigned upd_rueckruf_fehler;   /* was Sonys Prüfung am Zertifikat bemängelt hat (nur fürs Protokoll) */
static int upd_rueckruf(int ssl, unsigned int fehler, void *const zert[], int n, void *x)
{
    (void)ssl; (void)zert; (void)n; (void)x;
    upd_rueckruf_fehler = fehler;
    return 0;   /* trotzdem weiter - nur im zweiten Versuch gesetzt, siehe unten */
}
static void *upd_nid(const char *nid)
{
    uint32_t h = 0;
    return kernel_dynlib_handle(-1, "libSceHttp.sprx", &h) ? NULL : (void *)kernel_dynlib_resolve(-1, h, nid);
}
/* NEU 1.1: eine HTTPS-Anfrage (GET oder POST) mit Sonys HTTP-Bibliothek - der Ablauf von 1.0.9 (Update-Hinweis), jetzt auch für die
 * Rückmeldung (POST an die Rückmeldeseite), die Internet-Prüfung und das Herunterladen eines Updates. Kennt die PS5 die Ausstellerstelle
 * eines Zertifikats nicht (0x8095F00C), gibt es wie bisher einen zweiten Versuch ohne die Prüfung (ohne_pruefung = true). Die Antwort
 * steht in *antwort (malloc, mit 0 am Ende), höchstens amax Bytes. kopf: weitere Kopfzeilen als "Name", "Wert", ..., NULL. */
static bool https_anfrage_sony(const char *methode, const char *url, const char *const *kopf, const void *koerper, size_t klen, const char *ua,
                               char **antwort, size_t *alen, size_t amax, int *status, bool *ohne_pruefung, char *grund, size_t gn)
{
    *antwort = NULL; *alen = 0; *status = 0; *ohne_pruefung = false; grund[0] = 0;
    void *fn[U_ANZ];
    (void)dlopen("libSceNet.sprx", RTLD_LAZY); (void)dlopen("libSceSsl.sprx", RTLD_LAZY); (void)dlopen("libSceHttp.sprx", RTLD_LAZY);
    for (int i = 0; i < U_ANZ; i++) {
        uint32_t h = 0;
        fn[i] = kernel_dynlib_handle(-1, UPD_NID[i].lib, &h) ? NULL : (void *)kernel_dynlib_resolve(-1, h, UPD_NID[i].nid);
        if (!fn[i]) { snprintf(grund, gn, "Sonys HTTP-Bibliothek nicht verfügbar (%s)", UPD_NID[i].name); return false; }
    }
    bool post = methode && !strcmp(methode, "POST");
    int pool = -1, ssl = -1, http = -1, vorlage = -1, verb = -1, anfr = -1, r = 0, st = 0;
    bool ok = false;
    char *buf = NULL;
    ((int (*)(void))fn[U_NETINIT])();   /* meldet vielleicht "schon vorbereitet" - das ist kein Fehler */
    pool = ((int (*)(const char *, int, int))fn[U_POOL])("PS5CleanupNetz", 64 * 1024, 0);
    if (pool < 0) { snprintf(grund, gn, "Netz nicht bereit (0x%08X)", (unsigned)pool); goto ende; }
    ssl = ((int (*)(size_t))fn[U_SSLINIT])(256 * 1024);
    if (ssl < 0) { snprintf(grund, gn, "Verschlüsselung nicht bereit (0x%08X)", (unsigned)ssl); goto ende; }
    http = ((int (*)(int, int, size_t))fn[U_HTTPINIT])(pool, ssl, 256 * 1024);
    if (http < 0) { snprintf(grund, gn, "HTTP nicht bereit (0x%08X)", (unsigned)http); goto ende; }
    vorlage = upd_vorlage(fn, http, ua);
    if (vorlage < 0) { snprintf(grund, gn, "HTTP-Vorlage (0x%08X)", (unsigned)vorlage); goto ende; }
    const char *wie = ""; char wie_b[160] = "";
    for (int versuch = 0; ; versuch++) {
        verb = ((int (*)(int, const char *, int))fn[U_VERB])(vorlage, url, 0);
        if (verb < 0) { snprintf(grund, gn, "HTTP-Verbindung (0x%08X)", (unsigned)verb); goto ende; }
        anfr = ((int (*)(int, int, const char *, uint64_t))fn[U_ANFR])(verb, post ? 1 /* POST */ : 0 /* GET */, url, post ? (uint64_t)klen : 0);
        if (anfr < 0) { snprintf(grund, gn, "HTTP-Anfrage (0x%08X)", (unsigned)anfr); goto ende; }
        for (int i = 0; kopf && kopf[i] && kopf[i + 1]; i += 2) ((int (*)(int, const char *, const char *, uint32_t))fn[U_KOPF])(anfr, kopf[i], kopf[i + 1], 0);
        r = ((int (*)(int, const void *, size_t))fn[U_SENDEN])(anfr, post ? koerper : NULL, post ? klen : 0);
        /* NEU 1.0.9 (PS5-Test 08.11.2026: jede Prüfung endete mit 0x8095F00C - die PS5 kennt die Stelle nicht, die GitHubs Zertifikat
         * ausgestellt hat; Entscheidung 22:28 "entscheide du", Weg A): nur bei genau diesem Fehler ein zweiter Versuch ohne die
         * Zertifikatsprüfung (sceHttpsDisableOption, Kennung SERVER_VERIFY). Falscher Rechnername (0x8095F00B), abgelaufen (0x8095F00D)
         * und alles andere bleiben ein Fehler. Der zweite Versuch steht im Protokoll. */
        if (r == (int)0x8095F00C && versuch == 0) {
            /* PS5-Test 08.11.2026 22:54: "Abschalten abgelehnt" an der schon benutzten Vorlage. Jetzt: frische Vorlage, dann der Reihe nach
             * sceHttpsDisableOption, sceHttpsDisableOptionPrivate (wie auf der PS4), zuletzt ein Rückruf, der Sonys Urteil über das
             * Zertifikat überstimmt (sceHttpsSetSslCallback). Jeder Rückgabewert steht im Protokoll. */
            ((int (*)(int))fn[U_ANFRWEG])(anfr); anfr = -1; ((int (*)(int))fn[U_VERBWEG])(verb); verb = -1;
            ((int (*)(int))fn[U_VORLAGEWEG])(vorlage);
            vorlage = upd_vorlage(fn, http, ua);
            if (vorlage < 0) { snprintf(grund, gn, "die PS5 kennt das Zertifikat nicht (0x8095F00C), neue HTTP-Vorlage (0x%08X)", (unsigned)vorlage); goto ende; }
            void *aus = upd_nid("mSQCxzWTwVI"), *ausp = upd_nid("zJYi5br6ZiQ"), *rr = upd_nid("htyBOoWeS58");
            int a1 = aus ? ((int (*)(int, uint32_t))aus)(vorlage, 0x01 /* SERVER_VERIFY */) : -1;
            int a2 = a1 >= 0 ? 0 : ausp ? ((int (*)(int, uint32_t))ausp)(vorlage, 0x01) : -1;
            int a3 = a1 >= 0 || a2 >= 0 ? 0 : rr ? ((int (*)(int, int (*)(int, unsigned int, void *const[], int, void *), void *))rr)(vorlage, upd_rueckruf, NULL) : -1;
            /* Codex-Prüfung 145 (P3): die Rückgabewerte stehen immer im Protokoll, auch wenn danach etwas anderes scheitert */
            snprintf(wie_b, sizeof wie_b, "%s (Abschalten %s0x%08X, privat %s%s0x%08X, Rückruf %s%s0x%08X)",
                     a1 >= 0 ? "Prüfung abgeschaltet" : a2 >= 0 ? "Prüfung abgeschaltet (privat)" : a3 >= 0 ? "Zertifikat per Rückruf angenommen" : "nicht möglich",
                     aus ? "" : "fehlt ", (unsigned)a1, a1 >= 0 ? "nicht nötig " : "", ausp ? "" : "fehlt ", (unsigned)a2,
                     a1 >= 0 || a2 >= 0 ? "nicht nötig " : "", rr ? "" : "fehlt ", (unsigned)a3);
            wie = wie_b;
            if (a1 < 0 && a2 < 0 && a3 < 0) { snprintf(grund, gn, "die PS5 kennt das Zertifikat nicht (0x8095F00C), zweiter Versuch %s", wie); goto ende; }
            *ohne_pruefung = true; continue;
        }
        break;
    }
    if (r < 0) { snprintf(grund, gn, "keine Verbindung (0x%08X)%s%s", (unsigned)r, *ohne_pruefung ? ", auch im zweiten Versuch: " : "", wie); goto ende; }
    r = ((int (*)(int, int *))fn[U_STATUS])(anfr, &st);
    *status = st;
    if (r < 0) { snprintf(grund, gn, "keine Antwort (0x%08X)%s%s", (unsigned)r, *ohne_pruefung ? ", zweiter Versuch: " : "", wie); goto ende; }
    if (!(buf = malloc(amax + 1))) { snprintf(grund, gn, "kein Speicher"); goto ende; }
    size_t len = 0;
    for (;;) {
        int k = ((int (*)(int, void *, size_t))fn[U_LESEN])(anfr, buf + len, amax - len);
        if (k < 0) { snprintf(grund, gn, "Antwort unvollständig (0x%08X)%s%s", (unsigned)k, *ohne_pruefung ? ", zweiter Versuch: " : "", wie); goto ende; }
        if (k == 0) break;
        len += (size_t)k;
        if (len >= amax) { snprintf(grund, gn, "Antwort zu groß%s%s", *ohne_pruefung ? ", zweiter Versuch: " : "", wie); goto ende; }
    }
    buf[len] = 0;
    *antwort = buf; *alen = len; buf = NULL; ok = true;
    if (*ohne_pruefung) snprintf(grund, gn, "zweiter Versuch ohne Zertifikatsprüfung - die PS5 kennt das Zertifikat nicht; %s%s", wie,
                                 upd_rueckruf_fehler ? " - Sony bemängelte beim Rückruf etwas" : "");   /* bei Erfolg: Hinweis fürs Protokoll */
ende:
    if (anfr >= 0) ((int (*)(int))fn[U_ANFRWEG])(anfr);
    if (verb >= 0) ((int (*)(int))fn[U_VERBWEG])(verb);
    if (vorlage >= 0) ((int (*)(int))fn[U_VORLAGEWEG])(vorlage);
    if (http >= 0) ((int (*)(int))fn[U_HTTPENDE])(http);
    if (ssl >= 0) ((int (*)(int))fn[U_SSLENDE])(ssl);
    if (pool >= 0) ((int (*)(int))fn[U_POOLWEG])(pool);
    free(buf);
    return ok;
}
/* NEU 1.1 (Weg D, da_Chris86 15.11.2026): zuerst die eigene GEPRÜFTE Verbindung (netz_tls.c: mbedTLS 3.6 + Mozilla-Zertifikatsliste,
 * nur in release und testlauf). Kommt darüber keine Antwort, wie bisher Sonys Bibliothek (mit deren zweitem Versuch ohne Prüfung - ein
 * Update lädt plattform_holen darüber nie). grund nennt dann beide Wege fürs Protokoll. */
#ifdef MIT_NETZ_TLS
#include "netz_tls.h"
#endif
static bool https_anfrage(const char *methode, const char *url, const char *const *kopf, const void *koerper, size_t klen, const char *ua,
                          char **antwort, size_t *alen, size_t amax, int *status, bool *ohne_pruefung, char *grund, size_t gn)
{
#ifdef MIT_NETZ_TLS
    char cg[300] = "", sg[400] = "";
    *ohne_pruefung = false; grund[0] = 0;
    if (netz_tls_anfrage(methode, url, kopf, koerper, klen, ua, antwort, alen, amax, status, cg, sizeof cg)) {
        /* fürs Protokoll (PS5-Test): welcher Weg geklappt hat - nur Rechnername, keine Pfade */
        char z[200]; const char *h = strstr(url, "://"); h = h ? h + 3 : url; size_t hl = strcspn(h, "/?#");
        snprintf(z, sizeof z, SP_EN ? "Connection to %.*s: own verified connection, response %d" : "Verbindung zu %.*s: eigene geprüfte Verbindung, Antwort %d",
                 (int)(hl < 80 ? hl : 80), h, *status);   /* NEU 1.2 (Rückmeldung #10, englische Oberfläche): in der Sprache der Oberfläche */
        if (plattform_meldung) plattform_meldung(z); else klog_printf("PS5 Cleanup: %s\n", z);
        return true;
    }
    bool ok = https_anfrage_sony(methode, url, kopf, koerper, klen, ua, antwort, alen, amax, status, ohne_pruefung, sg, sizeof sg);
    snprintf(grund, gn, SP_EN ? "own connection: %s; via Sony: %s" : "eigene Verbindung: %s; über Sony: %s", cg, sg[0] ? sg : (SP_EN ? "got a response" : "Antwort bekommen"));
    return ok;
#else
    return https_anfrage_sony(methode, url, kopf, koerper, klen, ua, antwort, alen, amax, status, ohne_pruefung, grund, gn);
#endif
}
void plattform_name_merken(const char *url)
{
#ifdef MIT_NETZ_TLS
    netz_tls_name_merken(url);
#else
    (void)url;
#endif
}
static char upd_text[8192];   /* NEU 1.3: Release-Notes der letzten Antwort (nur der Faden der Update-Prüfung schreibt und liest) */
void plattform_neueste_text(char *aus, size_t n) { if (n) snprintf(aus, n, "%s", upd_text); }
bool plattform_neueste_version(const char *eigene, char *tag, size_t n, char *grund, size_t gn)
{
    tag[0] = 0; grund[0] = 0;
    upd_text[0] = 0;
    char ua[64]; snprintf(ua, sizeof ua, "PS5-Cleanup/%s", eigene && eigene[0] ? eigene : "?");
    static const char *const kopf[] = { "Accept", "application/vnd.github+json", "X-GitHub-Api-Version", "2022-11-28", NULL };
    char *buf = NULL, g[400] = ""; size_t len = 0; int st = 0; bool ohne = false;
    if (!https_anfrage("GET", UPD_URL, kopf, NULL, 0, ua, &buf, &len, UPD_MAX, &st, &ohne, g, sizeof g)) { snprintf(grund, gn, "GitHub: %s", g); return false; }
    bool ok = false;
    if (st != 200) snprintf(grund, gn, "GitHub antwortet mit %d%s%s", st, ohne ? " - " : "", ohne ? g : "");
    else {
        cJSON *j = cJSON_ParseWithLength(buf, len);
        const cJSON *t = j ? cJSON_GetObjectItemCaseSensitive(j, "tag_name") : NULL;
        if (cJSON_IsString(t) && t->valuestring && t->valuestring[0]) { snprintf(tag, n, "%s", t->valuestring); ok = true; if (ohne) snprintf(grund, gn, "%s", g); }
        if (ok) {   /* NEU 1.3: die Release-Notes merken - gekürzt nur an einer Zeichengrenze */
            const cJSON *b = cJSON_GetObjectItemCaseSensitive(j, "body");
            if (cJSON_IsString(b) && b->valuestring) {
                size_t l = strlen(b->valuestring);
                if (l >= sizeof upd_text) { l = sizeof upd_text - 1; while (l && ((unsigned char)b->valuestring[l] & 0xC0) == 0x80) l--; }
                memcpy(upd_text, b->valuestring, l); upd_text[l] = 0;
            }
        }
        else snprintf(grund, gn, "keine Versionsangabe in der Antwort%s%s", ohne ? " - " : "", ohne ? g : "");
        cJSON_Delete(j);
    }
    free(buf);
    return ok;
}
/* NEU 1.1: Rückmeldung direkt senden (POST, Formular-Daten) - true, wenn die Rückmeldeseite mit 200 antwortet */
bool plattform_senden(const char *url, const char *eigene, const char *koerper, size_t len, char *grund, size_t gn)
{
    grund[0] = 0;
    char ua[64]; snprintf(ua, sizeof ua, "PS5-Cleanup/%s", eigene && eigene[0] ? eigene : "?");
    static const char *const kopf[] = { "Content-Type", "application/x-www-form-urlencoded", "Accept", "text/plain", NULL };
    char *buf = NULL, g[400] = ""; size_t alen = 0; int st = 0; bool ohne = false;
    if (!https_anfrage("POST", url, kopf, koerper, len, ua, &buf, &alen, 16 * 1024, &st, &ohne, g, sizeof g)) { snprintf(grund, gn, "%s", g); return false; }
    bool ok = st == 200;
    if (!ok) snprintf(grund, gn, "Rückmeldeseite antwortet mit %d: %.120s", st, buf ? buf : "");
    else if (ohne) snprintf(grund, gn, "%s", g);
    free(buf);
    return ok;
}
/* NEU 1.1: Internet-Prüfung - irgendeine Antwort der Gegenstelle reicht */
bool plattform_erreichbar(const char *url, const char *eigene, char *grund, size_t gn)
{
    char ua[64]; snprintf(ua, sizeof ua, "PS5-Cleanup/%s", eigene && eigene[0] ? eigene : "?");
    static const char *const kopf[] = { "Accept", "text/plain", NULL };
    char *buf = NULL; size_t alen = 0; int st = 0; bool ohne = false;
    bool ok = https_anfrage("GET", url, kopf, NULL, 0, ua, &buf, &alen, 4096, &st, &ohne, grund, gn);
    free(buf);
    return ok && st > 0;
}
/* NEU 1.1 (Freigabe da_Chris86 15.11.2026: "Update holen" - Ja = laden, prüfen, dann die alte löschen; Nein = zusätzlich ablegen) */
/* NEU 1.1: liegt unter pfad genau diese Datei (gleiche Größe, gleicher Inhalt)? */
static bool gleiche_datei(const char *pfad, const char *daten, size_t len)
{
    struct stat st;
    if (lstat(pfad, &st) != 0 || !S_ISREG(st.st_mode) || (size_t)st.st_size != len) return false;
    int fd = open(pfad, O_RDONLY | O_NOFOLLOW); if (fd < 0) return false;
    char b[64 * 1024]; size_t o = 0; bool gleich = true;
    while (gleich && o < len) { ssize_t k = read(fd, b, sizeof b); if (k <= 0 || (size_t)k > len - o || memcmp(b, daten + o, (size_t)k)) gleich = false; else o += (size_t)k; }
    close(fd);
    return gleich && o == len;
}
bool plattform_update_ablegen(const char *ordner, const char *name, const char *daten, size_t len, const char *alt, bool ersetzen,
                             char *ziel, size_t zn, bool *schon_da, bool *alt_geloescht, char *grund, size_t gn)
{
    grund[0] = 0; *schon_da = false; *alt_geloescht = false; snprintf(ziel, zn, "%s/%s", ordner, name);
#ifdef KERN_LOESCHEN
    /* Codex-Prüfung 339: die Zwischendatei wird neu angelegt (O_EXCL) - eine vorhandene gleichnamige Datei oder ein Hardlink darauf wird nie
     * abgeschnitten; vorhandene Dateien werden nie überschrieben, außer der alten Version selbst bei "ersetzen" */
    /* Nachtprüfung 16.11.2026 (Codex-Prüfung 340): eigene Zwischendatei mit eindeutigem Namen (eine fremde gleichnamige Datei wird nie
     * gelöscht); der Zielname wird vor dem Umbenennen mit O_EXCL belegt (nichts Fremdes wird ersetzt); lässt sich die alte Version bei
     * "ersetzen" nicht löschen, ist das kein Erfolg */
    char tmp[1024], neu[1024];
    if (strchr(name, '/') || !strcmp(name, "..") || !strcmp(name, ".") || !name[0]) { snprintf(grund, gn, "ungültiger Dateiname"); return false; }
    if (snprintf(neu, sizeof neu, "%s/%s", ordner, name) >= (int)sizeof neu) { snprintf(grund, gn, "Pfad zu lang"); return false; }
    {   /* NEU 1.1 (PS5-Test 15.11.2026): dieselbe Datei liegt schon da - nicht noch eine Kopie */
        const char *punkt = strrchr(name, '.'); size_t bl = punkt ? (size_t)(punkt - name) : strlen(name);
        char p2[1024];
        for (int i = 1; i <= 9; i++) {
            if (i == 1) snprintf(p2, sizeof p2, "%s", neu);
            else if (snprintf(p2, sizeof p2, "%s/%.*s_%d%s", ordner, (int)bl, name, i, punkt ? punkt : "") >= (int)sizeof p2) break;
            if (!gleiche_datei(p2, daten, len)) continue;
            *schon_da = true; snprintf(ziel, zn, "%s", p2);
            if (ersetzen && alt && alt[0] && strcmp(alt, p2)) {
                if (unlink(alt) != 0) { snprintf(grund, gn, "die neue Version liegt schon als %s da, aber die alte ließ sich nicht löschen", p2); return false; }
                *alt_geloescht = true;
            }
            return true;
        }
    }
    int fd = -1;
    for (int i = 0; i < 20 && fd < 0; i++) {
        if (snprintf(tmp, sizeof tmp, "%s/.ps5cleanup_update_%d_%d.tmp", ordner, (int)getpid(), i) >= (int)sizeof tmp) { snprintf(grund, gn, "Pfad zu lang"); return false; }
        fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0777);
        if (fd < 0 && errno != EEXIST) break;
    }
    if (fd < 0) { snprintf(grund, gn, "Schreiben ging nicht (%s)", ordner); return false; }
    size_t w = 0; while (w < len) { ssize_t k = write(fd, daten + w, len - w); if (k <= 0) break; w += (size_t)k; }
    bool ok = w == len && fsync(fd) == 0; ok = close(fd) == 0 && ok;
    if (!ok) { unlink(tmp); snprintf(grund, gn, "Schreiben ging nicht (%s)", ordner); return false; }
    /* Ziel: bei "ersetzen" der Name der neuen Version, wenn er frei ist oder die alte Version selbst ist; sonst ein freier Name daneben.
     * Ein freier Name wird mit O_EXCL belegt - legt in diesem Moment jemand anders eine Datei an, wird der nächste Name genommen */
    bool alt_selbst = ersetzen && alt && alt[0] && !strcmp(alt, neu);
    if (!alt_selbst) {
        const char *punkt = strrchr(name, '.'); size_t bl = punkt ? (size_t)(punkt - name) : strlen(name);
        bool frei = false;
        for (int i = 1; i <= 9 && !frei; i++) {
            if (i > 1 && snprintf(neu, sizeof neu, "%s/%.*s_%d%s", ordner, (int)bl, name, i, punkt ? punkt : "") >= (int)sizeof neu) break;
            int r = open(neu, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0777);
            if (r >= 0) { close(r); frei = true; }
            else if (errno != EEXIST) break;
        }
        if (!frei) { unlink(tmp); snprintf(grund, gn, "kein freier Dateiname - die alte Version bleibt"); return false; }
    }
    if (rename(tmp, neu) != 0) { unlink(tmp); if (!alt_selbst) unlink(neu); snprintf(grund, gn, "Ablegen ging nicht - die alte Version bleibt"); return false; }   /* neu ist dann nur unser leerer Platzhalter */
    snprintf(ziel, zn, "%s", neu);
    if (ersetzen && alt && alt[0] && strcmp(alt, neu)) {
        if (unlink(alt) != 0) { snprintf(grund, gn, "die neue Version liegt als %s da, aber die alte ließ sich nicht löschen", neu); return false; }
        *alt_geloescht = true;
    }
    return true;
#else
    (void)daten; (void)len; (void)alt; (void)ersetzen;
    snprintf(grund, gn, "Testfassung: nur geladen und geprüft, nichts geschrieben");
    return true;
#endif
}
/* NEU 1.1: Datei herunterladen (GET) - in *daten (malloc), höchstens max Bytes; true nur bei Antwort 200 */
bool plattform_holen(const char *url, const char *eigene, const char *accept, char **daten, size_t *len, size_t max, char *grund, size_t gn)
{
    grund[0] = 0; *daten = NULL; *len = 0;
    char ua[64]; snprintf(ua, sizeof ua, "PS5-Cleanup/%s", eigene && eigene[0] ? eigene : "?");
    const char *kopf[] = { "Accept", accept && accept[0] ? accept : "*/*", NULL };
    char *buf = NULL, g[400] = ""; size_t alen = 0; int st = 0; bool ohne = false;
    if (!https_anfrage("GET", url, kopf, NULL, 0, ua, &buf, &alen, max, &st, &ohne, g, sizeof g)) { snprintf(grund, gn, "%s", g); return false; }
    if (st != 200) { snprintf(grund, gn, "Antwort %d", st); free(buf); return false; }
    /* Codex-Prüfung 339 (P1): ein Update ist ausführbarer Code - nur über eine geprüfte Verbindung. Konnte die PS5 das Zertifikat nicht
     * prüfen (0x8095F00C, zweiter Versuch ohne Prüfung), wird nichts übernommen. */
    if (ohne) { free(buf); snprintf(grund, gn, "die PS5 konnte das Zertifikat nicht prüfen - ohne geprüfte Verbindung wird kein Update geladen"); return false; }
    *daten = buf; *len = alen;
    return true;
}

/* NEU 1.0.10 (da_Chris86 14.11.2026 08:25): laufende Programme (nur lesen) - je Zeile Nummer, Eltern, Benutzer, Name und
 * Pfad der Programmdatei. Grundlage für das Schließen fremder Programme vor dem Löschen. */
#include <sys/user.h>
#include <stddef.h>
/* NEU 1.0.10: Anschlüsse (TCP, IPv4), die ein Programm offen hat - über die Liste seiner offenen Dateien. Rückgabe: Anzahl, -1 = die
 * PS5 gibt die Liste nicht her */
static int prog_ports(int pid, int *ports, int max)
{
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_FILEDESC, pid };
    size_t n = 0;
    if (sysctl(mib, 4, NULL, &n, NULL, 0) != 0) return -1;
    if (n == 0) return 0;
    n += n / 2 + 4096;
    unsigned char *b = malloc(n);
    if (!b) return -1;
    if (sysctl(mib, 4, b, &n, NULL, 0) != 0) { free(b); return -1; }
    int anz = 0;
    const size_t mind = offsetof(struct kinfo_file, kf_sa_local) + sizeof(struct sockaddr_in);
    for (size_t q = 0; q + 2 * sizeof(int) <= n; ) {
        struct kinfo_file *kf = (struct kinfo_file *)(b + q);
        if (kf->kf_structsize <= 0 || q + (size_t)kf->kf_structsize > n) break;
        q += (size_t)kf->kf_structsize;
        if ((size_t)kf->kf_structsize < mind || kf->kf_type != KF_TYPE_SOCKET || kf->kf_sock_domain != AF_INET || kf->kf_sock_type != SOCK_STREAM) continue;
        struct sockaddr_in sa; memcpy(&sa, &kf->kf_sa_local, sizeof sa);
        int p = (int)ntohs(sa.sin_port);
        if (p <= 0) continue;
        if (anz >= max) { free(b); return -1; }   /* Codex-Prüfung 243: mehr als erwartet - gilt als nicht abfragbar, nie als vollständig */
        ports[anz++] = p;
    }
    free(b);
    return anz;
}
static const char *prog_ports_text(int pid, char *t, size_t tl)
{
    int p[32], n = prog_ports(pid, p, 32); size_t g = 0;
    if (n < 0) { snprintf(t, tl, "Anschlüsse nicht abfragbar"); return t; }
    if (n == 0) { snprintf(t, tl, "-"); return t; }
    t[0] = 0;
    for (int i = 0; i < n; i++) { int w = snprintf(t + g, tl - g, "%s%d", i ? "," : "Anschlüsse ", p[i]); if (w < 0 || (size_t)w >= tl - g) break; g += (size_t)w; }
    return t;
}
int plattform_prozessliste(char *aus, size_t max)
{
    if (!aus || max < 2) return -1;
    aus[0] = 0;
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0 };
    size_t n = 0;
    if (sysctl(mib, 4, NULL, &n, NULL, 0) != 0 || n == 0) return -1;
    n += n / 4 + 4096;
    unsigned char *b = malloc(n);
    if (!b) return -1;
    if (sysctl(mib, 4, b, &n, NULL, 0) != 0) { free(b); return -1; }
    int anz = 0; size_t g = 0;
    for (size_t p = 0; p + sizeof(struct kinfo_proc) <= n; ) {
        struct kinfo_proc *ki = (struct kinfo_proc *)(b + p);
        if (ki->ki_structsize <= 0 || p + (size_t)ki->ki_structsize > n) break;
        p += (size_t)ki->ki_structsize;
        char pfad[512]; size_t pl = sizeof pfad - 1;
        int mp[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, (int)ki->ki_pid };
        if (sysctl(mp, 4, pfad, &pl, NULL, 0) != 0) pfad[0] = 0; else pfad[pl < sizeof pfad ? pl : sizeof pfad - 1] = 0;
        char nm[sizeof ki->ki_comm + 1]; memcpy(nm, ki->ki_comm, sizeof ki->ki_comm); nm[sizeof ki->ki_comm] = 0;
        int w = snprintf(aus + g, max - g, "%d %d %d %s %s\n", (int)ki->ki_pid, (int)ki->ki_ppid, (int)ki->ki_uid, nm, ki->ki_ppid == 0 ? "-" : prog_ports_text((int)ki->ki_pid, pfad, sizeof pfad));
        if (w < 0 || (size_t)w >= max - g) { aus[g] = 0; free(b); return -2 - anz; }   /* Codex-Prüfung 235: Platz reicht nicht - gekürzt, Rückgabe -2 - Anzahl */
        g += (size_t)w; anz++;
    }
    free(b);
    return anz;
}

/* NEU 1.0.10 (da_Chris86 14.11.2026 08:25): fremde Hintergrund-Programme beenden. Aufbau der Programme (PS5-Test 14.11.2026 08:48, FW 12.00):
 * Kern-Fäden haben die Eltern-Nummer 0; Sonys Dienste und Apps sind Kinder von SceSysCore.elf oder heißen Sce...; nachgeladene Programme
 * hängen an Nummer 1 (nanodns.elf, klogsrv.elf, ftpsrv.elf, pldmgr.elf) oder am Lader (elfldr.elf -> kstuff.elf, payload.elf). */
typedef struct { int pid, ppid; long start; char name[24]; } prog;
#define PROG_MAX 1024
static int prog_liste(prog *p, int max)
{
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0 };
    size_t n = 0;
    if (sysctl(mib, 4, NULL, &n, NULL, 0) != 0 || n == 0) return -1;
    n += n / 4 + 4096;
    unsigned char *b = malloc(n);
    if (!b) return -1;
    if (sysctl(mib, 4, b, &n, NULL, 0) != 0) { free(b); return -1; }
    int anz = 0;
    for (size_t q = 0; q + sizeof(struct kinfo_proc) <= n; ) {
        struct kinfo_proc *ki = (struct kinfo_proc *)(b + q);
        if (ki->ki_structsize <= 0 || q + (size_t)ki->ki_structsize > n) break;
        q += (size_t)ki->ki_structsize;
        if (anz >= max) { free(b); return -1; }               /* mehr als erwartet: lieber nichts anfassen */
        p[anz].pid = (int)ki->ki_pid; p[anz].ppid = (int)ki->ki_ppid; p[anz].start = (long)ki->ki_start.tv_sec;
        size_t l = sizeof ki->ki_comm < sizeof p[anz].name - 1 ? sizeof ki->ki_comm : sizeof p[anz].name - 1;
        memcpy(p[anz].name, ki->ki_comm, l); p[anz].name[l] = 0;
        anz++;
    }
    free(b);
    return anz;
}
/* bleibt immer laufen: der Jailbreak selbst, nanoDNS (hält Sonys Server fern), Payload Manager und FTP-Server (da_Chris86 14.11.2026 09:32:
 * die braucht man nach dem Löschen sofort wieder), ShadowMount (wird nur über seinen eigenen Ablauf angehalten)
 * und PS5 Cleanup */
/* NEU 1.1 (Schritt 4): EINE Liste (seit 1.2 gibt es nur noch den Weg "alle fremden Programme schließen"). Bis 1.0.10 hatte das
 * Beenden je Kachel eine eigene Liste ohne nanoDNS - gegen die Regel "nanoDNS wird nie beendet" (da_Chris86). */
/* NEU 1.3 (Rückmeldung #50): onionHEN wie etaHEN - seine Programme laufen als onion_daemon.elf, onion_util.elf, onion_elfldr.elf (Lader auf 9020),
 * der Entpacker als OnionHEN.elf (aydencharles/onionHEN docs/arch.md); kstuff.elf steht schon in der Liste */
static const char *const PROG_IMMER[] = { "elfldr", "kstuff", "etahen", "onion", "byepervisor", "nanodns", "shadowmount", "ps4mount", "playden", "ps5cleanup", NULL };   /* nie */
static const char *const PROG_NUR_GEWAEHLT[] = { "pldmgr", "ftpsrv", "zftpd", NULL };   /* nur wenn genau sie gelöscht werden */
static bool prog_in(const char *n, const char *const *v) { for (int i = 0; v[i]; i++) if (!strncasecmp(n, v[i], strlen(v[i]))) return true; return false; }
static bool prog_behalten(const char *n) { return prog_in(n, PROG_IMMER) || prog_in(n, PROG_NUR_GEWAEHLT); }
/* NEU 1.0.10 (da_Chris86 14.11.2026 12:18)): der Payload Manager und zftpd werden nur beendet, wenn genau sie zum Löschen gewählt sind (kern_laufende_gewaehlt) */
/* NEU 1.1 (da_Chris86 15.11.2026 07:11): für alle Programme, die sonst weiterlaufen (kern_laufende_gewaehlt) - nie ShadowMount, ps4mount, Playden
 * (eigener Ablauf zum Anhalten) und nie PS5 Cleanup; die stehen nie in der Liste */
static char prog_auch_liste[160];
void plattform_schliessen_auch(const char *liste) { snprintf(prog_auch_liste, sizeof prog_auch_liste, "%s", liste ? liste : ""); }
static bool prog_auch(const char *n)
{
    for (const char *z = prog_auch_liste; *z; ) {
        const char *e = strchr(z, ','); size_t l = e ? (size_t)(e - z) : strlen(z);
        const char *zz = z; if (l == 8 && !strncasecmp(z, "onionhen", 8)) { zz = "onion"; l = 5; }   /* NEU 1.3: Datenordner OnionHEN, Programme onion_... */
        if (l > 0 && !strncasecmp(n, zz, l) && strncasecmp(n, "ps5cleanup", 10) && strncasecmp(n, "shadowmount", 11) && strncasecmp(n, "ps4mount", 8) && strncasecmp(n, "playden", 7)) return true;
        if (!e) break;
        z = e + 1;
    }
    return false;
}
static const prog *prog_von(const prog *p, int n, int pid) { for (int i = 0; i < n; i++) if (p[i].pid == pid) return &p[i]; return NULL; }
/* nachgeladen (nicht von Sony): hängt an Nummer 1, ist der Lader selbst oder stammt von einem nachgeladenen Programm ab */
static bool prog_fremd(const prog *p, int n, const prog *x, int syscore, int tiefe)
{
    if (!x || tiefe > 16 || x->pid <= 1 || x->ppid == 0 || x->ppid == syscore || !strncmp(x->name, "Sce", 3)) return false;
    if (x->ppid == 1 || prog_behalten(x->name)) return true;
    return prog_fremd(p, n, prog_von(p, n, x->ppid), syscore, tiefe + 1);
}
/* NEU 1.1 (Schritt 4): was beide Wege zuerst brauchen - die Programmliste, SceSysCore, die eigene Nummer und (auf Wunsch) die Nummer des
 * eigenen Hintergrundteils. Der heißt wie jedes namenlose nachgeladene Programm "payload.elf" und bestätigt seine Nummer selbst; meldet
 * sich keiner, bleibt hg 0 - dann wird kein payload.elf angefasst. */
typedef struct { prog *p; int n, syscore, eig, hg; } prog_lage;
static int prog_hg_suchen(const prog *p, int n, int syscore)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(p[i].name, "payload.elf") || !prog_fremd(p, n, &p[i], syscore, 0)) continue;
        char pf[64], t[300]; snprintf(pf, sizeof pf, "/ps5cleanup-ichbin?pid=%d", p[i].pid);
        if (hintergrund_fragen(pf, "PS5CLEANUP-ICH-BIN-ES", t, sizeof t) == 1) return p[i].pid;
    }
    return 0;
}
static bool prog_lage_lesen(prog_lage *l, bool mit_hg)
{
    memset(l, 0, sizeof *l);
    l->p = malloc(sizeof(prog) * PROG_MAX);
    if (!l->p) return false;
    l->n = prog_liste(l->p, PROG_MAX); l->eig = (int)getpid();
    for (int i = 0; i < l->n; i++) if (!strcmp(l->p[i].name, "SceSysCore.elf")) l->syscore = l->p[i].pid;
    if (l->n <= 0 || !l->syscore) return false;
    if (mit_hg) l->hg = prog_hg_suchen(l->p, l->n, l->syscore);
    return true;
}
/* ein Programm ohne Namen, das erst kurz läuft, wird nie beendet (ShadowMount heißt beim Start kurz "payload.elf", Codex-Prüfung 237/238).
 * jetzt_s: der Stichtag - Codex-Prüfung 305: beim Beenden je Kachel EIN Stichtag für den ganzen Durchlauf (wie bis 1.0.10), sonst
 * würde ein Kandidat während des Durchlaufs "alt genug" */
static bool prog_kurz(const prog *x, long jetzt_s) { return x->start <= 0 || jetzt_s - x->start < 92; }
/* NEU 1.3 (21.11.2026, Rückmeldung): welche nachgeladenen Programme laufen (wie prog_fremd) - nur die Namen, klein, ohne .elf, ohne PS5 Cleanup
 * selbst und ohne namenlose payload.elf. Nur lesen, nichts wird angefasst. */
int plattform_hintergrund(char *aus, size_t max)
{
    if (!aus || max < 2) return -1;
    aus[0] = 0;
    prog_lage lg;
    if (!prog_lage_lesen(&lg, false)) { free(lg.p); return -1; }
    int anz = 0; size_t g = 0;
    for (int i = 0; i < lg.n && anz < 20; i++) {
        const prog *x = &lg.p[i];
        if (x->pid == lg.eig || !prog_fremd(lg.p, lg.n, x, lg.syscore, 0) || !strcmp(x->name, "payload.elf") || !strncasecmp(x->name, "ps5cleanup", 10)) continue;
        char nm[32]; size_t j = 0;
        for (const char *q = x->name; *q && j + 1 < sizeof nm; q++) { char c = *q; if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') nm[j++] = c; }
        nm[j] = 0;
        if (j > 4 && !strcmp(nm + j - 4, ".elf")) nm[j - 4] = 0;
        if (!nm[0] || strlen(nm) > 24) continue;
        bool schon = false;   /* doppelt (zwei Fäden desselben Programms) nur einmal */
        for (const char *z = aus; *z && !schon; ) { const char *e = strchr(z, ','); size_t l = e ? (size_t)(e - z) : strlen(z); if (l == strlen(nm) && !strncmp(z, nm, l)) schon = true; z = e ? e + 1 : z + l; }
        if (schon) continue;
        int w = snprintf(aus + g, max - g, "%s%s", g ? "," : "", nm);
        if (w < 0 || (size_t)w >= max - g) { aus[g] = 0; break; }
        g += (size_t)w; anz++;
    }
    free(lg.p);
    return anz;
}
int plattform_fremde_schliessen(char *aus, size_t max)
{
    if (!aus || max < 2) return -1;
    aus[0] = 0;
    prog_lage lg; prog *nach = malloc(sizeof(prog) * PROG_MAX);
    if (!prog_lage_lesen(&lg, true) || !nach) { klog_printf("PS5 Cleanup: Programme schließen - Liste %d, SceSysCore %d: nichts angefasst\n", lg.n, lg.syscore); free(lg.p); free(nach); return -1; }
    prog *p = lg.p; int n = lg.n, syscore = lg.syscore, eig = lg.eig, hg = lg.hg;
    int *weg = malloc(sizeof(int) * PROG_MAX), wn = 0, zu = 0; size_t g = 0; bool voll = false;
    if (!weg) { free(p); free(nach); return -1; }
#define ZEILE(z, e, nm) do { int w_ = snprintf(aus + g, max - g, "%d %d %s\n", (z), (e), (nm)); if (w_ > 0 && (size_t)w_ < max - g) g += (size_t)w_; else aus[g] = 0; } while (0)
    for (int i = 0; i < n; i++) {
        const prog *x = &p[i];
        if (x->pid == eig || x->pid == hg || !prog_fremd(p, n, x, syscore, 0) || (prog_behalten(x->name) && !prog_auch(x->name))) continue;
        if (max - g < 96 + 40 * (size_t)(wn + 1)) { voll = true; break; }   /* Codex-Prüfung 237: Platz für jede Ergebniszeile - sonst hier aufhören */
        /* Codex-Prüfung 237: ein namenloses Programm kann gerade erst starten (ShadowMount benennt sich erst im Startablauf um) - es
         * wird nie beendet (Codex-Prüfung 238: auch eine Wartezeit schützt nicht sicher). ps4mount und Playden haben
         * wie ShadowMount ihren eigenen Ablauf zum Anhalten (prog_behalten). */
        /* da_Chris86 14.11.2026 11:00: läuft seit dem Anhalten von ShadowMount und ps4mount (kern_loeschen.c) - ein Programm ohne Namen wird
         * jetzt beendet, außer ps4mount läuft noch (Anschluss 9032), der eigene Hintergrundteil hat sich nicht gemeldet oder es läuft erst kurz */
        if (!strcmp(x->name, "payload.elf")) {
            if (!hg || port_belegt(9032) != 0) { ZEILE(5, 0, x->name); continue; }
            if (prog_kurz(x, (long)time(NULL))) { ZEILE(3, 0, x->name); continue; }
            { int n2 = prog_liste(nach, PROG_MAX); const prog *y = prog_von(nach, n2, x->pid);   /* Codex-Pruefung 254: heisst es jetzt anders (ShadowMount benennt sich im Start um), nicht beenden */
              if (!y || strcmp(y->name, "payload.elf")) { ZEILE(3, 0, x->name); continue; } }
        }
        int r = kill(x->pid, SIGKILL), e = r ? errno : 0;
        klog_printf("PS5 Cleanup: Programm %s (%d) beenden -> %d (Fehler %d)\n", x->name, x->pid, r, e);
        if (r) { ZEILE(1, e, x->name); continue; }
        weg[wn++] = i;
    }
    /* bis zu 3 s warten, bis sie wirklich weg sind */
    int nn = 0;
    for (int runde = 0; runde < 15 && wn > 0; runde++) {
        usleep(200000);
        nn = prog_liste(nach, PROG_MAX);
        int da = 0;
        for (int j = 0; j < wn && nn > 0; j++) { const prog *y = prog_von(nach, nn, p[weg[j]].pid); if (y && !strcmp(y->name, p[weg[j]].name)) da++; }
        if (nn > 0 && !da) break;
    }
    for (int j = 0; j < wn; j++) {
        const prog *y = nn > 0 ? prog_von(nach, nn, p[weg[j]].pid) : NULL;
        bool da = nn <= 0 || (y && !strcmp(y->name, p[weg[j]].name));
        ZEILE(da ? 2 : 0, 0, p[weg[j]].name);
        if (!da) zu++;
    }
    if (voll) ZEILE(4, 0, "-");
#undef ZEILE
    free(p); free(nach); free(weg);
    return zu;
}

