/* PS5 Cleanup 1.1 - Oberfläche am Fernseher
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
static antwort sony_antwort(arena *a, int code, const char *t) { antwort r = { code, a_kopie(a, t) }; return r; }
#ifdef KERN_LOESCHEN
int sceAppInstUtilInitialize(void);
int sceAppInstUtilAppUnInstall(const char *title_id);
int sceAppInstUtilAppExists(const char *title_id, int *exists);
/* NEU 1.0.6 (da_Chris86 29.10.2026 21:25): Sonys Deinstallation mit der geprüften Kennung, Abschluss abwarten, danach prüfen,
 * ob der Menüeintrag wirklich weg ist (sceAppInstUtilAppExists). Der Aufruf läuft in einem eigenen Faden mit Zeitgrenze:
 * Beim Test 29.10.2026 19:32 (Spectrum Library SLIB00001, ohne App-Ordner) kam Sonys Dienst nie zurück - dann darf die
 * Oberfläche nicht hängen bleiben. Antwort 200 nur, wenn der Eintrag nachweislich ausgetragen ist. */
/* Codex-Hinweis 29.10.2026 22:11: JEDER Sony-Aufruf (Initialisierung, Statusabfrage, Deinstallation) läuft in einem eigenen
 * Faden mit Zeitgrenze - auch Initialisierung und Statusabfrage können unbegrenzt hängen. Hing einmal ein Aufruf, wird Sonys
 * Dienst in dieser Sitzung nicht mehr benutzt (sonst stapeln sich hängende Fäden). */
typedef struct { int art; char id[16]; volatile int fertig; int e; int da; } sony_auftrag;   /* art: 0 Init, 1 eingetragen?, 2 Deinstallation */
static void *sony_faden(void *x)
{
    sony_auftrag *a = x;
    if (a->art == 0) a->e = sceAppInstUtilInitialize();
    else if (a->art == 1) { int d = -1; a->e = sceAppInstUtilAppExists(a->id, &d); a->da = d; }
    else a->e = sceAppInstUtilAppUnInstall(a->id);
    a->fertig = 1;
    return NULL;
}
static void sony_warte(int ms) { struct timespec z = { ms / 1000, (long)(ms % 1000) * 1000000L }; nanosleep(&z, NULL); }
static int sony_gestoert = 0;
/* 1 = fertig (*e, *da gesetzt), 0 = keine Antwort in ms (der Faden bleibt zurück, sein Auftrag wird nicht freigegeben) */
static int sony_rufe(int art, const char *id, int ms, int *e, int *da)
{
    if (sony_gestoert) return 0;
    sony_auftrag *au = calloc(1, sizeof *au);
    if (!au) { *e = -1; return 1; }
    au->art = art; snprintf(au->id, sizeof au->id, "%s", id ? id : "");
    pthread_t th;
    if (pthread_create(&th, NULL, sony_faden, au)) { free(au); *e = -1; return 1; }
    pthread_detach(th);
    for (int i = 0; i < ms / 100 && !au->fertig; i++) sony_warte(100);
    if (!au->fertig) { sony_gestoert = 1; klog_printf("PS5 Cleanup: Sony-Aufruf %d (%s) - keine Antwort nach %d ms\n", art, au->id, ms); return 0; }
    *e = au->e; if (da) *da = au->da; free(au);
    return 1;
}
/* 1 = eingetragen, 0 = nicht eingetragen, -1 = nicht prüfbar, -2 = keine Antwort */
/* PS5-Test 29.10.2026 22:29 (FAKE00000): sceAppInstUtilAppExists schreibt nur EIN Byte (vorher -255 = ...FF01, nachher -256 =
 * ...FF00 bei Vorbelegung -1) - ausgewertet wird darum nur das unterste Byte; alles außer 0/1 gilt als "nicht prüfbar" */
static int sony_eingetragen(const char *id) { int e = 0, d = -1; if (!sony_rufe(1, id, 5000, &e, &d)) return -2; if (e) return -1; int b = d & 0xFF; return b == 1 ? 1 : (b == 0 ? 0 : -1); }
/* NEU 1.0.7 (da_Chris86 31.10.2026 19:56, Punkt 3): jeder Schritt der Deinstallation auch ins Protokoll der Oberfläche */
static void sony_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void sony_log(const char *fmt, ...)
{
    char t[400]; va_list v; va_start(v, fmt); vsnprintf(t, sizeof t, fmt, v); va_end(v);
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
    return NULL;
}
/* NEU 1.0.7 (Punkt 3, eingeschränkt am 31.10.2026 21:50 Punkt 4): für PSUP00001 mit der nullfs-Einbindung von ps5upload
 * (/data/ps5upload/tile/PSUP00001 -> /system_ex/app/PSUP00001, Vorbild ps5upload register.c unregister_title, GPL-3.0) und
 * NEU 01.11.2026 23:19 für die Einbindung des eigenen Spielordners der Kachel (kstuff, Ordner-Spiele auf der M.2).
 * Vor dem Lösen wird die Regel erneut geprüft (kern_geist_regel) - jede andere Einbindung bleibt unangetastet.
 * unmount löst nur die Ansicht /system_ex/app/<ID> - der Spielordner selbst wird nicht angefasst.
 * 1 = gelöst, 0 = keine Einbindung (mehr), -1 = nicht gelöst / nicht erlaubt. */
static int sony_einbindung_loesen(arena *a, const char *id)
{
    const char *fs = NULL, *q = NULL, *g = "";
    int m = einbindung_abfragen(a, id, &fs, &q);
    if (m == 0) return 0;
    int rg = kern_geist_regel(id, m, fs, q, eigene_spielquelle(a, id), &g);
    if (rg != GEIST_PS5UPLOAD && rg != GEIST_EIGENES_SPIEL) { sony_log("Kachel %s: Einbindung %s %s weder von ps5upload noch der eigene Spielordner - nicht angefasst", id, fs ? fs : "?", q ? q : "?"); return -1; }
    char p[64]; snprintf(p, sizeof p, "/system_ex/app/%s", id);
    int r = unmount(p, 0);
    int e = errno;
    const char *fs2 = NULL, *q2 = NULL;
    bool weg = einbindung_abfragen(a, id, &fs2, &q2) == 0;
    if (weg) sony_log("Kachel %s: Einbindung %s %s -> %s vor der Deinstallation gelöst", id, rg == GEIST_PS5UPLOAD ? "von ps5upload" : "des eigenen Spielordners", q, p);
    else sony_log("Kachel %s: Einbindung %s -> %s ließ sich nicht lösen (%s)", id, q, p, r ? strerror(e) : "noch eingehängt");
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
static antwort sony_deinstallieren(arena *a, const char *id, bool nur_pruefen)
{
    if (!strcmp(id, SO_ID) || !strcmp(id, "PSCL00002")) return sony_antwort(a, 550, "550 PS5 Cleanup läuft gerade über diese App");   /* eigener Startordner */
    if (strlen(id) != 9) return sony_antwort(a, 550, "550 Kennung ungültig");
    /* NEU 1.0.7 (da_Chris86 31.10.2026 21:50, Punkt 4): Behandlung nach der Regel (kern_geister.c) - unklare Einbindung: 451, nichts tun */
    const char *efs = NULL, *eq = NULL, *eg = "";
    int em = einbindung_abfragen(a, id, &efs, &eq);
    int regel = kern_geist_regel(id, em, efs, eq, em == 1 ? eigene_spielquelle(a, id) : NULL, &eg);
    if (regel == GEIST_UNVERAENDERT) { if (!nur_pruefen) sony_log("Kachel %s: %s - unverändert", id, eg); return sony_antwort(a, 451, a_fmt(a, "451 %s", eg)); }
    if (nur_pruefen) return sony_antwort(a, 200, "200 OK");
    if (sony_gestoert) return sony_antwort(a, 550, "550 Sonys Dienst hat in dieser Sitzung schon einmal nicht geantwortet");
    {
        int e = 0, b = sony_bereit(&e);
        if (b == 0) return sony_antwort(a, 550, "550 Sonys Dienst antwortet nicht (Initialisierung, 10 s)");
        if (b < 0) return sony_antwort(a, 550, a_fmt(a, "550 Dienst nicht verfügbar 0x%08X", (unsigned)e));
    }
    int vorher = sony_eingetragen(id);
    klog_printf("PS5 Cleanup: Sony-Deinstallation %s - vorher eingetragen=%d\n", id, vorher);
    if (vorher == -2) return sony_antwort(a, 550, "550 Sonys Dienst antwortet nicht (Statusabfrage, 5 s)");
    if (vorher == 0) {
        /* NEU 1.0.7 (da_Chris86 01.11.2026, Reste ohne Kachel - z. B. L.A. Noire am 01.11. morgens): Sony führt die Kennung nicht im
         * Menü. Steht sie auch nicht in app.db (gleiche Prüfung wie der PKG Manager), gibt es keine Kachel, die stehen bleiben
         * könnte -> 226, die Dateien löscht der Kern selbst. Steht sie in app.db oder ist das unklar: nichts löschen.
         * (Bis 1.0.7: "200" - der Kern wartete 60 s auf Sony und meldete fälschlich "Sony löscht noch".) */
        const char *g = "";
        int in_db = db_kennung_in_appdb(a, plattform_db_ordner(), id, &g);
        sony_log("Kachel %s: bei Sony nicht eingetragen, in app.db: %s%s%s", id, in_db == 1 ? "ja" : in_db == 0 ? "nein" : "nicht prüfbar", g[0] ? " - " : "", g);
        if (in_db == 0) return sony_antwort(a, 226, "226 nicht im PS5-Menü eingetragen und nicht in app.db - Reste ohne Kachel");
        if (in_db == 1) return sony_antwort(a, 550, "550 Sony führt die Kennung nicht im Menü, sie steht aber in app.db - nicht sicher, nichts gelöscht");
        return sony_antwort(a, 550, a_fmt(a, "550 nicht im PS5-Menü eingetragen, app.db ließ sich nicht prüfen (%s) - nichts gelöscht", g));
    }
    if (vorher < 0) return sony_antwort(a, 550, "550 ob die Kachel eingetragen ist, ließ sich vorher nicht prüfen - Sony nicht beauftragt");   /* 01.11.2026 (Prüfung) */
    {   /* NEU 1.0.7 (da_Chris86 01.11.2026 22:03, Test B 22:08): kaputte Zusatzinhalt-Angaben zuerst reparieren */
        char zg[320] = ""; int zz = 0;
        int zr = zusatz_reparieren(id, &zz, zg, sizeof zg);
        if (zr == -1) return sony_antwort(a, 550, a_fmt(a, "550 kaputte Zusatzinhalt-Angaben ließen sich nicht reparieren (%s) - nichts gelöscht", zg));
        if (zr == -2) sony_log("Kachel %s: Zusatzinhalt-Angaben nicht prüfbar (%s) - Sony wird wie bisher beauftragt", id, zg);
        if (zr == 0 && zz > 0) sony_log("Kachel %s: %d kaputte Zusatzinhalt-Angabe(n) in appinfo.db auf 0 gesetzt", id, zz);
    }
    bool ps5upload = regel == GEIST_PS5UPLOAD;
    if (ps5upload && sony_einbindung_loesen(a, id) < 0) return sony_antwort(a, 451, "451 Einbindung von ps5upload ließ sich nicht lösen - unverändert");
    /* NEU 1.0.7 (da_Chris86 01.11.2026 23:19): Ordner-Spiel - Einbindung des eigenen Spielordners lösen, dann EIN Sony-Auftrag wie sonst */
    if (regel == GEIST_EIGENES_SPIEL && sony_einbindung_loesen(a, id) < 0) return sony_antwort(a, 451, "451 Einbindung des Spielordners ließ sich nicht lösen - unverändert");
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
    int versuche = mehrfach ? 3 : 1, gemacht = 0;
    bool einbindung_abbruch = false;
    for (int versuch = 1; versuch <= versuche; versuch++) {
        if (versuch > 1) {
            sony_log("Kachel %s: Sony hat angenommen, der Eintrag ist aber noch da - Versuch %d von 3", id, versuch);
            if (sony_einbindung_loesen(a, id) < 0) { einbindung_abbruch = true; break; }
        }
        if (!sony_rufe(2, id, 30000, &e, NULL)) return sony_antwort(a, 550, "550 Sony antwortet nicht (Deinstallation, 30 s)");
        gemacht = versuch;
        klog_printf("PS5 Cleanup: Sony-Deinstallation %s -> 0x%08X (Versuch %d)\n", id, (unsigned)e, versuch);
        if (e && versuch > 1) return sony_antwort(a, 550, a_fmt(a, "550 Sony hat den 1. Auftrag angenommen, den %d. abgelehnt (0x%08X) - die Kachel ist noch eingetragen", versuch, (unsigned)e));
        if (e) return sony_antwort(a, 550, a_fmt(a, "550 Sony hat abgelehnt (0x%08X)", (unsigned)e));
        int warten = (mehrfach && versuch < versuche) ? 20 : 60;
        for (int i = 0; i < warten && (jetzt = sony_eingetragen(id)) == 1; i++) sony_warte(1000);
        if (jetzt != 1) break;
    }
    klog_printf("PS5 Cleanup: Sony-Deinstallation %s - danach eingetragen=%d\n", id, jetzt);
    if (jetzt == 1) sony_log("Kachel %s: nach %d Auftrag/Aufträgen noch eingetragen - bleibt im Menü", id, gemacht);
    if (jetzt == 1 && einbindung_abbruch) return sony_antwort(a, 550, a_fmt(a, "550 Sony hat angenommen, die Kachel ist aber noch eingetragen - vor dem %d. Auftrag ließ sich die Einbindung nicht lösen, abgebrochen", gemacht + 1));
    if (jetzt == 1) return sony_antwort(a, 550, gemacht > 1 ? a_fmt(a, "550 Sony hat angenommen, die Kachel ist nach %d Aufträgen aber noch eingetragen", gemacht) : "550 Sony hat angenommen, die Kachel ist nach 60 s aber noch eingetragen");
    if (jetzt == -2) return sony_antwort(a, 550, "550 Sony hat angenommen, die Statusabfrage antwortet aber nicht");
    if (jetzt < 0) return sony_antwort(a, 550, "550 Sony hat angenommen, ob die Kachel ausgetragen ist, ließ sich nicht prüfen");
    return sony_antwort(a, 200, "200 OK");
}
/* NEU 1.0.6: eigener Startordner bei der Selbstentfernung (start/startordner.h - nur die eigenen Dateien) */
static int startordner_weg(arena *a, bool nur_pruefen, const char **text)
{
    static char t[300];
    *text = NULL;
    if (nur_pruefen) { int z = so_zustand(); if (z < 0) { *text = a_kopie(a, SO_DIR " gehört nicht eindeutig zu PS5 Cleanup - bleibt."); return -1; } return z; }
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
    int r = so_oeffnen(t, sizeof t);
    klog_printf("PS5 Cleanup: /system_ex öffnen -> %d %s\n", r, t);
    if (t[0]) *text = a_kopie(a, t);
    return r;
}
static volatile int selbst_uebergeben_ok = 0;   /* NEU 1.0.7: dann stellt der Hintergrundteil /system_ex zurück */
static int hintergrund_fragen(const char *pfad, const char *erwartet, char *t, size_t tl);   /* weiter unten (seit 1.0.8 in allen Fassungen) */
static int selbst_uebergeben(arena *a, bool kachel, bool startordner, const char **text)
{
    char t[300], pfad[320];
    snprintf(pfad, sizeof pfad, "/ps5cleanup-selbstweg?kachel=%d&start=%d%s%s", kachel ? 1 : 0, startordner ? 1 : 0,
             prot_stick[0] ? "&prot=" : "", prot_stick);   /* NEU 1.0.7: Protokoll auf dem Stick (nur Buchstaben, Ziffern, / _ - .) */
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
    if (!strcmp(id, SO_ID)) return sony_antwort(a, 550, "550 PS5 Cleanup läuft gerade über diese App");
    return nur_pruefen ? sony_antwort(a, 200, "200 OK") : sony_antwort(a, 550, "550 nicht eingebaut");
}
/* Simulation: nur nachsehen, ob der eigene Startordner da ist ("würde gelöscht") */
static int startordner_weg(arena *a, bool nur_pruefen, const char **text)
{
    (void)nur_pruefen; *text = NULL;
    int z = so_zustand();
    if (z < 0) { *text = a_kopie(a, SO_DIR " gehört nicht eindeutig zu PS5 Cleanup - bleibt."); return -1; }
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
static char cd_grund[200];
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
    if (kernel_dynlib_handle(-1, LIB, &h)) { snprintf(cd_grund, sizeof cd_grund, "Sonys Löschdienst für Bilder/Videos ließ sich nicht laden"); goto aus; }
    init = (void *)kernel_dynlib_resolve(-1, h, "zoxb0wEChEM");          /* sceContentDeleteInitialize */
    cd_nach_pfad = (void *)kernel_dynlib_resolve(-1, h, "qC3QW+AltHs");  /* sceContentDeleteByPath */
    if (!init || !cd_nach_pfad) { snprintf(cd_grund, sizeof cd_grund, "Sonys Löschdienst für Bilder/Videos: Funktionen nicht gefunden"); goto aus; }
    memset(&cd_init, 0, sizeof cd_init); cd_init.heap_groesse = 0x4000;
    if (!cd_rufe(init, &cd_init, 10000, &e)) { snprintf(cd_grund, sizeof cd_grund, "Sonys Löschdienst für Bilder/Videos antwortet nicht (Vorbereitung, 10 s)"); goto aus; }
    if (e) { snprintf(cd_grund, sizeof cd_grund, "Sonys Löschdienst für Bilder/Videos lehnt ab (Vorbereitung 0x%08X)", (unsigned)e); goto aus; }
    cd_zustand = 1;
    sony_log("Mediengalerie: Sonys Löschdienst bereit");
    return true;
aus:
    sony_log("Mediengalerie: %s", cd_grund);
    return false;
}
#endif
static antwort medium_loeschen(arena *a, const char *pfad, bool nur_pruefen)
{
    if (pfad) {
        if (!zugriff_medium_pfad(pfad)) return sony_antwort(a, 550, "550 keine Aufnahme der Mediengalerie");
        struct stat st;
        if (lstat(pfad, &st) != 0) return sony_antwort(a, 550, a_fmt(a, "550 %s", strerror(errno)));
        if (!S_ISREG(st.st_mode)) return sony_antwort(a, 550, "550 keine normale Datei");
    }
#ifdef KERN_LOESCHEN
    if (!pfad) return cd_bereit() ? sony_antwort(a, 200, "200 OK") : sony_antwort(a, 502, a_fmt(a, "502 %s", cd_grund));
    if (nur_pruefen) return sony_antwort(a, 200, "200 OK");
    if (cd_gestoert) return sony_antwort(a, 550, "550 Sonys Löschdienst hat in dieser Sitzung schon einmal nicht geantwortet");
    if (!cd_bereit()) return sony_antwort(a, 550, a_fmt(a, "550 %s", cd_grund));
    /* worauf gewartet wird (nur was es vorher gab): Aufnahme, .meta, .dat, Vorschaubild */
    const char *bl = strrchr(pfad, '/') + 1, *pk = strrchr(bl, '.');
    int sl = (int)(pk && pk > bl ? (size_t)(pk - pfad) : strlen(pfad));
    const char *beg[4] = { pfad, a_fmt(a, "%.*s.meta", sl, pfad), a_fmt(a, "%.*s.dat", sl, pfad), a_fmt(a, "/user/av_contents/thumbnails/%s.jxr", pfad + 18) };
    bool warten[4];
    for (int i = 0; i < 4; i++) warten[i] = da_lstat(beg[i]);
    char *db = strdup(pfad + 5);                   /* "/av_contents/..." - so steht die Aufnahme in Sonys Mediengalerie-Datenbank */
    if (!db) return sony_antwort(a, 550, "550 kein Speicher");
    int e = 0;
    if (!cd_rufe(cd_nach_pfad, db, 30000, &e)) return sony_antwort(a, 551, "551 Sonys Löschdienst antwortet nicht (30 s)");   /* db bleibt dem Faden; 551: evtl. schon angenommen */
    free(db);
    sony_log("Mediengalerie: %s -> 0x%08X", pfad + 5, (unsigned)e);
    if (e) return sony_antwort(a, 550, a_fmt(a, "550 Sony hat abgelehnt (0x%08X)", (unsigned)e));
    for (int s = 0; s < 30; s++) {                 /* Sony löscht danach selbst (PS5-Test: unter 1 s) */
        bool rest = false;
        for (int i = 0; i < 4; i++) if (warten[i] && da_lstat(beg[i])) rest = true;
        if (!rest) return sony_antwort(a, 200, "200 OK");
        sony_warte(1000);
    }
    sony_log("Mediengalerie: %s - Sony hat angenommen, nach 30 s ist aber noch etwas da", pfad);
    return sony_antwort(a, 551, "551 Sony hat angenommen, nach 30 s ist die Aufnahme aber noch nicht ganz weg - Sony löscht evtl. noch");
#else
    return pfad && !nur_pruefen ? sony_antwort(a, 550, "550 nicht eingebaut") : sony_antwort(a, 200, "200 OK");   /* Simulation: nur prüfen */
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
    if (!zugriff_spielstand_pfad(ordner, benutzer, &prospero, titel)) return sony_antwort(a, 550, "550 kein Spielstand-Ordner");
    const char *db = a_fmt(a, "/system_data/%s/%s/db/user/savedata.db", prospero ? "savedata_prospero" : "savedata", benutzer);
    struct stat st;
    if (stat(db, &st) != 0 || !S_ISREG(st.st_mode)) return sony_antwort(a, 550, a_fmt(a, "550 Sonys Spielstand-Liste %s nicht gefunden - ohne sie wird nichts gelöscht", db));
    const char *wo = "";
    int b = spielstand_in_benutzung(ordner, titel, &wo);
    if (b > 0) return sony_antwort(a, 550, a_fmt(a, "550 der Spielstand ist gerade in Benutzung (%s) - erst das Spiel schließen", wo));
    if (b < 0) return sony_antwort(a, 550, "550 ob das Spiel gerade läuft, ließ sich nicht prüfen - nichts gelöscht");
    if (nur_pruefen) return sony_antwort(a, 200, "200 OK");
#ifdef KERN_LOESCHEN
    /* 02.11.2026 (da_Chris86 15:44): ohne Sicherung - auf dem USB-Stick nur das Protokoll. Geändert wird nach Prüfung, alles oder nichts. */
    char grund[300] = ""; int zeilen = 0;
    int r = spielstand_db_austragen(db, titel, &zeilen, grund, sizeof grund);
    if (r != 0) { sony_log("Spielstand-Liste %s (%s): %s", titel, prospero ? "PS5" : "PS4", grund); return sony_antwort(a, 550, a_fmt(a, "550 %s", grund)); }
    sony_log("Spielstand-Liste %s (%s): %d Eintrag/Einträge ausgetragen", titel, prospero ? "PS5" : "PS4", zeilen);
    if (zeilen == 0) {   /* Prüfung 01.11.2026: keine Einträge - liegen trotzdem Spielstände im Ordner, ist die Zuordnung unklar */
        DIR *dd = opendir(ordner);
        if (!dd) return sony_antwort(a, 550, "550 Spielstand-Ordner nicht lesbar - nichts gelöscht");
        int sd = 0; struct dirent *de;
        while ((de = readdir(dd)) != NULL) if (strncmp(de->d_name, "sdimg_", 6) == 0) sd++;
        closedir(dd);
        if (sd > 0) {
            sony_log("Spielstand-Liste %s: %d Spielstand-Datei(en), aber kein Eintrag mit dieser Kennung - unklar, nichts gelöscht", titel, sd);
            return sony_antwort(a, 550, "550 im Ordner liegen Spielstände, die Spielstand-Liste führt aber keinen Eintrag mit dieser Kennung - Zuordnung unklar, nichts gelöscht");
        }
    }
    return sony_antwort(a, 200, a_fmt(a, "200 %d Einträge ausgetragen", zeilen));
#else
    return sony_antwort(a, 550, "550 nicht eingebaut");   /* Simulation: nur prüfen */
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
void plattform_start(void)
{
    zugriff_speicher_knapp = speicher_knapp;   /* NEU 1.0.7: Sicherheitsbremse (alle Fassungen) */
#ifndef KERN_LOESCHEN
    { pthread_t t; if (pthread_create(&t, NULL, speicher_messung, NULL) == 0) pthread_detach(t); }
#endif
    sceSystemServiceHideSplashScreen();
    zugriff_sony_deinstallieren = sony_deinstallieren; zugriff_startordner_entfernen = startordner_weg;
    zugriff_einbindung = einbindung_abfragen;   /* NEU 1.0.7: nur lesen (statfs) - in allen Fassungen */
    zugriff_medium_loeschen = medium_loeschen; zugriff_spielstand_austragen = spielstand_austragen;   /* NEU 1.0.7 (01.11.2026): in allen Fassungen - löschen nur die echte */
    zugriff_diagnose = diagnose_klog;
#ifdef KERN_LOESCHEN
    zugriff_selbst_uebergeben = selbst_uebergeben;   /* nur die echte Fassung - die Simulation übergibt nie */
    kern_ps4_sony_eingetragen = sony_ps4_eingetragen;   /* NEU 1.0.7: PS4-Inventar - Menüeintrag nur abfragen */
    zugriff_system_ex_oeffnen = system_ex_oeffnen;   /* NEU 1.0.7: nur die echte Fassung */
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
        if (lstat(p, &st) != 0 && mkdir(p, 0755) != 0 && errno != EEXIST) { klog_printf("PS5 Cleanup: %s nicht anlegbar: %s\n", p, strerror(errno)); continue; }
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
