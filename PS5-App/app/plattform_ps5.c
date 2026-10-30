/* PS5 Cleanup 1.0.6 - Oberfläche am Fernseher
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * plattform_ps5.c - Konsole abfragen (nur lesen), Sprache, eigener Ort, Zugriff.
 * Modell nach PS5Upload (hw_info.c) / drakmor/ps5-hwinfo (GPL-3.0),
 * Laufwerks-Kopplung wie ps5info.c aus 1.0.5 (nach earthonion/ddd_status, GPL-3.0):
 * ein lesendes ioctl auf /dev/ddd. Die Kopplung wird nie verändert.
 * /system_ex: nur der eigene Startordner PSCL00002 wird bei der Selbstentfernung gelöscht (start/startordner.h,
 * Freigabe da_Chris86 29.10.2026 21:23); sonst wird /system_ex weder gelesen noch verändert.
 */
#include "plattform.h"
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
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
#include "../start/startordner.h"   /* NEU 1.0.6: eigener Startordner PSCL00002 (Selbstentfernung) */

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
static antwort sony_deinstallieren(arena *a, const char *id, bool nur_pruefen)
{
    if (!strcmp(id, SO_ID) || !strcmp(id, "PSCL00002")) return sony_antwort(a, 550, "550 PS5 Cleanup läuft gerade über diese App");   /* eigener Startordner */
    if (strlen(id) != 9) return sony_antwort(a, 550, "550 Kennung ungültig");
    if (nur_pruefen) return sony_antwort(a, 200, "200 OK");
    if (sony_gestoert) return sony_antwort(a, 550, "550 Sonys Dienst hat in dieser Sitzung schon einmal nicht geantwortet");
    static int bereit = 0;
    if (!bereit) {
        int e = 0;
        if (!sony_rufe(0, NULL, 10000, &e, NULL)) return sony_antwort(a, 550, "550 Sonys Dienst antwortet nicht (Initialisierung, 10 s)");
        if (e) return sony_antwort(a, 550, a_fmt(a, "550 Dienst nicht verfügbar 0x%08X", (unsigned)e));
        bereit = 1;
    }
    int vorher = sony_eingetragen(id);
    klog_printf("PS5 Cleanup: Sony-Deinstallation %s - vorher eingetragen=%d\n", id, vorher);
    if (vorher == -2) return sony_antwort(a, 550, "550 Sonys Dienst antwortet nicht (Statusabfrage, 5 s)");
    if (vorher == 0) return sony_antwort(a, 200, "200 nicht (mehr) eingetragen");
    int e = 0;
    if (!sony_rufe(2, id, 30000, &e, NULL)) return sony_antwort(a, 550, "550 Sony antwortet nicht (Deinstallation, 30 s)");
    klog_printf("PS5 Cleanup: Sony-Deinstallation %s -> 0x%08X\n", id, (unsigned)e);
    if (e) return sony_antwort(a, 550, a_fmt(a, "550 0x%08X", (unsigned)e));
    int jetzt = 1;
    for (int i = 0; i < 60 && (jetzt = sony_eingetragen(id)) == 1; i++) sony_warte(1000);   /* Abschluss: höchstens etwa 60 s */
    klog_printf("PS5 Cleanup: Sony-Deinstallation %s - danach eingetragen=%d\n", id, jetzt);
    if (jetzt == 1) return sony_antwort(a, 550, "550 Sony hat angenommen, die Kachel ist nach 60 s aber noch eingetragen");
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
/* NEU 1.0.6 (da_Chris86 29.10.2026 23:08): Auftrag an den Hintergrundteil von ps5cleanup.elf (127.0.0.1:9099, start/ps5cleanup.c).
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
    char anf[200]; snprintf(anf, sizeof anf, "GET %s HTTP/1.0\r\n\r\n", pfad);
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
static int selbst_uebergeben(arena *a, bool kachel, bool startordner, const char **text)
{
    char t[300], pfad[96];
    snprintf(pfad, sizeof pfad, "/ps5cleanup-selbstweg?kachel=%d&start=%d", kachel ? 1 : 0, startordner ? 1 : 0);
    int r = hintergrund_fragen(pfad, "PS5CLEANUP-UEBERNOMMEN", t, sizeof t);
    klog_printf("PS5 Cleanup: Übergabe der Selbstentfernung an den Hintergrundteil (%s) -> %d %s\n", pfad, r, t);
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
/* NEU 1.0.6 (da_Chris86 30.10.2026 07:29): Ausschalten, wenn die Datenbank neu aufgebaut werden muss (start/ausschalten.h) */
#include "../start/ausschalten.h"
bool plattform_ausschalten(void) { int e = ps5_ausschalten(); klog_printf("PS5 Cleanup: Ausschalten -> %d (0x%08X)\n", e, (unsigned)e); return e == 0; }
bool plattform_ausschalten_uebergeben(void)
{
    char t[300];
    int r = hintergrund_fragen("/ps5cleanup-ausschalten", "PS5CLEANUP-AUSSCHALTEN-OK", t, sizeof t);
    klog_printf("PS5 Cleanup: Ausschalten an den Hintergrundteil -> %d %s\n", r, t);
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
bool plattform_ausschalten(void) { return false; }
bool plattform_ausschalten_uebergeben(void) { return false; }
#endif

/* NEU 1.0.6: Diagnoseprotokoll der Selbstentfernung = Kernel-Protokoll (liegt außerhalb der zu löschenden Daten) */
/* ------------------------------------------------------------- 1.0.6 Fix: Absturzmelder */
typedef struct { char unused[45]; char message[3075]; } pl_meldung_t;
int sceKernelSendNotificationRequest(int, pl_meldung_t *, size_t, int);
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
void plattform_start(void)
{
    sceSystemServiceHideSplashScreen();
    zugriff_sony_deinstallieren = sony_deinstallieren; zugriff_startordner_entfernen = startordner_weg;
    zugriff_diagnose = diagnose_klog;
#ifdef KERN_LOESCHEN
    zugriff_selbst_uebergeben = selbst_uebergeben;   /* nur die echte Fassung - die Simulation übergibt nie */
#endif
}
void plattform_ende(void) { sceSystemServiceLoadExec("exit", NULL); }
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
