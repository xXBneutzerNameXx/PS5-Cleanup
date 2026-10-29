/* PS5 Cleanup 1.0.6 - ps5cleanup.elf: EINE ELF für den Payload-Lader
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * 1. Legt beim ersten Start die eigene Startbildschirm-Kachel "PS5 Cleanup" an (kachel.c).
 * 2. Startet die Vollbild-Oberfläche mit DualSense (start_hbldr.c) aus dem EIGENEN Startordner /system_ex/app/PSCL00002
 *    (startordner.h, legt ihn beim ersten Start an - Freigabe da_Chris86 29.10.2026 21:23). Kein Homebrew Launcher nötig.
 * 3. Bleibt im Hintergrund und wartet auf 127.0.0.1:9099. Ein Druck auf die Kachel öffnet diese Adresse im
 *    Browser der PS5 -> die Oberfläche wird erneut im Vollbild gestartet (der Browser wird dabei geschlossen).
 *    Läuft schon ein Hintergrundteil (Port belegt), startet dieser Aufruf nur die Oberfläche und endet.
 * 4. NEU 1.0.6: Läuft die Oberfläche schon (Prozess "ps5cleanup_ui"), wird sie NICHT neu gestartet - sonst
 *    würde ein laufendes Löschen abgebrochen. Es kommt nur eine Meldung.
 * Nach einem Neustart der PS5 muss ps5cleanup.elf wieder geladen werden (wie ShadowMountPlus).
 * Der Hintergrundteil löscht sonst nichts und liest keine Daten der PS5 - gelöscht wird in der Oberfläche nach Bestätigung.
 * 5. NEU 1.0.6 (da_Chris86 29.10.2026 23:08): Selbstentfernung - die Oberfläche übergibt nach der Bestätigung den Auftrag
 *    "/ps5cleanup-selbstweg". Der Hintergrundteil bestätigt, wartet nachweislich auf das Ende der Oberfläche und entfernt
 *    dann die eigene Kachel (Sony) und den eigenen Startordner (selbstweg.c), meldet das geprüfte Ergebnis und beendet sich.
 *    Wählt man am Ende "Neu starten", übernimmt er auch den Neustart - erst nach der Entfernung ("/ps5cleanup-neustart").
 *    NEU (da_Chris86 30.10.2026 07:29): ebenso "Ausschalten" bei nötigem Datenbank-Neuaufbau ("/ps5cleanup-ausschalten").
 */
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <ps5/kernel.h>
#include <ps5/klog.h>
#include "start_hbldr.h"
#include "kachel.h"
#include "selbstweg.h"
#include <sys/select.h>
#include "ausschalten.h"
int sceSystemServiceRequestReboot(void);
#define SO_DIR_TEXT "/system_ex/app/PSCL00002"

#define EINBINDEN(name, datei)                                              \
  __asm__(".section .rodata\n.global " #name "\n.balign 16\n" #name ":\n"   \
          ".incbin \"" datei "\"\n.global " #name "_ende\n" #name "_ende:\n" \
          ".byte 0\n.previous\n");                                          \
  extern const unsigned char name[], name##_ende[];
EINBINDEN(oberflaeche_elf, UI_ELF)
EINBINDEN(kachel_icon, ICON_PNG)

#ifndef START_CWD
#define START_CWD "/data/homebrew/PS5Cleanup"
#endif

int sceUserServiceInitialize(void *);
int sceUserServiceTerminate(void);
/* Benachrichtigung oben rechts (Aufbau wie ShadowMountPlus include/sm_platform.h, GPL-3.0) */
typedef struct { char unused[45]; char message[3075]; } meldung_t;
int sceKernelSendNotificationRequest(int, meldung_t *, size_t, int);
static void melde(const char *t)
{
    meldung_t m; memset(&m, 0, sizeof m);
    snprintf(m.message, sizeof m.message, "%s", t);
    sceKernelSendNotificationRequest(0, &m, sizeof m, 0);
    klog_printf("PS5 Cleanup: %s\n", t);
}

#define UI_NAME "ps5cleanup_ui"     /* Prozessname der Oberfläche (max. 15 Zeichen) */

static int oberflaeche_starten(void)
{
    pid_t laeuft = start_hbldr_find_pid(UI_NAME);
    if (laeuft > 0) {
        melde("PS5 Cleanup läuft schon - über die PS-Taste zurück zu PS5 Cleanup wechseln.");
        return 0;
    }
    if (laeuft == -2) {   /* Codex-Prüfung 26: unklar, ob die Oberfläche läuft - lieber nicht neu starten (könnte ein Löschen abbrechen) */
        melde("PS5 Cleanup: Es ließ sich nicht prüfen, ob PS5 Cleanup schon läuft - bitte über die PS-Taste nachsehen.");
        return -1;
    }
    size_t n = (size_t)(oberflaeche_elf_ende - oberflaeche_elf);
    uint8_t *elf = malloc(n);
    if (!elf) { melde("PS5 Cleanup: zu wenig Speicher."); return -1; }
    memcpy(elf, oberflaeche_elf, n);
    char *argv[] = { UI_NAME, NULL };
    char *envp[] = { NULL };
    pid_t pid = start_hbldr_launch(START_CWD, UI_NAME, elf, -1, argv, envp);
    free(elf);
    if (pid < 0) melde("PS5 Cleanup: Vollbild-Start fehlgeschlagen.");
    else klog_printf("PS5 Cleanup: Oberfläche gestartet (Prozess %d)\n", (int)pid);
    return pid < 0 ? -1 : 0;
}

/* NEU 1.0.6 (Kacheltest 28.10.2026 14:35): Das Fenster der Kachel gehört zur Systemoberfläche (kein eigenes App-
 * Fenster, im Protokoll kein launchApp) und blieb über PS5 Cleanup stehen. Die Seite schließt sich deshalb selbst;
 * klappt das nicht, sagt sie, wie man sie schließt. */
static const char ANTWORT[] =
    "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n"
    "<!doctype html><html><head><meta charset=\"utf-8\"><title>PS5 Cleanup</title></head>"
    "<body style=\"background:#0b1020;color:#fff;font:34px sans-serif;text-align:center;padding-top:18%\">"
    "PS5 Cleanup startet ...<br><br><span style=\"font-size:26px;color:#9fb4ff\">"
    "Bleibt dieses Fenster stehen: Kreis-Taste dr&uuml;cken.<br>If this window stays open: press Circle.</span>"
    "<script>function zu(){try{window.close();}catch(e){}try{window.open('','_self').close();}catch(e){}}"
    "setTimeout(zu,1500);setTimeout(zu,4000);</script></body></html>";

/* NEU 1.0.6: einen laufenden Hintergrundteil bitten, sich zu beenden (nur 127.0.0.1) */
static void bitte_beenden(void)
{
    int c = socket(AF_INET, SOCK_STREAM, 0);
    if (c < 0) return;
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons(KACHEL_PORT); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (!connect(c, (struct sockaddr *)&a, sizeof a)) {
        static const char B[] = "GET /ps5cleanup-ende HTTP/1.0\r\n\r\n";
        (void)send(c, B, sizeof B - 1, 0);
        char p[64]; (void)recv(c, p, sizeof p, 0);
    }
    close(c);
}

/* NEU 1.0.6 (Review 30.10.2026): Entfernt sich ein laufender Hintergrundteil gerade selbst, darf ein erneuter Start nichts anfassen
 * (sonst legte er Kachel/Startordner neu an, während sie entfernt werden). 1 = ja, er entfernt sich gerade */
static int entfernt_sich_gerade(void)
{
    int c = socket(AF_INET, SOCK_STREAM, 0);
    if (c < 0) return 0;
    struct timeval z = { 3, 0 };
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &z, sizeof z); setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &z, sizeof z);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons(KACHEL_PORT); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int ja = 0;
    if (!connect(c, (struct sockaddr *)&a, sizeof a)) {
        static const char B[] = "GET /ps5cleanup-status HTTP/1.0\r\n\r\n";
        (void)send(c, B, sizeof B - 1, 0);
        char p[512]; size_t n = 0;
        for (;;) { ssize_t r = recv(c, p + n, sizeof p - 1 - n, 0); if (r <= 0) break; n += (size_t)r; if (n >= sizeof p - 1) break; }
        p[n] = 0;
        ja = strstr(p, "PS5CLEANUP-ENTFERNT-SICH") != NULL;
    }
    close(c);
    return ja;
}

/* NEU 1.0.6: die abschließende Entfernung läuft in einem eigenen Faden - der Hauptteil beantwortet derweil Anfragen
 * (Kachel, erneuter Start), damit niemand Kachel oder Startordner während der Entfernung neu anlegt (Review 30.10.2026) */
typedef struct { int kachel, start; volatile int fertig; char erg[1200]; } entfernung_t;
static void *entfernung_faden(void *x)
{
    entfernung_t *e = x;
    selbstweg_ausfuehren(e->kachel, e->start, e->erg, sizeof e->erg);
    e->fertig = 1;
    return NULL;
}

/* Kachel im eigenen Faden einrichten (Zeitgrenze im Hauptteil) */
typedef struct { volatile int fertig; int k; int sony; char text[512]; } kachel_auftrag;
static void *kachel_faden(void *x)
{
    kachel_auftrag *a = x;
    a->k = kachel_einrichten(kachel_icon, (size_t)(kachel_icon_ende - kachel_icon), &a->sony, a->text, sizeof a->text);
    a->fertig = 1;
    return NULL;
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    if (sceUserServiceInitialize(0)) { melde("PS5 Cleanup: UserService nicht verfügbar."); return 1; }
    kernel_set_ucred_authid(-1, 0x4801000000000013L);     /* wie websrv sys_init (Weg A) */
    if (entfernt_sich_gerade()) {
        melde("PS5 Cleanup entfernt sich gerade - bitte warten, bis die Benachrichtigung kommt, und erst danach neu laden.");
        sceUserServiceTerminate();
        return 1;
    }
    {   /* NEU 1.0.6: eigener Startordner statt fremder Platzhalter-App (startordner.h) */
        char text[400]; int umgehaengt = 0;
        int r = start_startordner(text, sizeof text, &umgehaengt);
        klog_printf("PS5 Cleanup: Startordner %s -> %d (umgehängt %d) %s\n", SO_DIR_TEXT, r, umgehaengt, text);
        if (r) { char t[480]; snprintf(t, sizeof t, "PS5 Cleanup: %s", text[0] ? text : "Startordner nicht bereit - Start nicht möglich."); melde(t); return 1; }
        if (text[0]) { char t[480]; snprintf(t, sizeof t, "PS5 Cleanup: %s", text); melde(t); }
    }

    /* Kachel immer prüfen/einrichten - auch wenn schon ein Hintergrundteil läuft (Codex-Prüfung 26) */
    {   /* Codex-Hinweis 29.10.2026 22:11: Sonys Dienst kann bei Initialisierung/Eintrag hängen - höchstens 20 s, dann ohne Kachel weiter */
        static kachel_auftrag ka;
        memset(&ka, 0, sizeof ka);
        pthread_t th;
        if (pthread_create(&th, NULL, kachel_faden, &ka) == 0) {
            pthread_detach(th);
            for (int i = 0; i < 200 && !ka.fertig; i++) usleep(100000);
        } else kachel_faden(&ka);
        if (!ka.fertig) melde("PS5 Cleanup: Sonys Installationsdienst antwortet nicht (20 s) - die Kachel wird diesmal nicht geprüft.");
        else if (ka.k != KACHEL_SCHON_DA) { char t[600]; snprintf(t, sizeof t, "PS5 Cleanup: %s", ka.text); melde(t); }
        else klog_printf("PS5 Cleanup: %s\n", ka.text);
    }

    /* NEU 1.0.6: Läuft schon ein Hintergrundteil (z. B. ältere Fassung nach einem Update), wird er gebeten, sich zu
     * beenden ("/ps5cleanup-ende"), damit die Kachel ab jetzt von dieser Fassung bedient wird. Ältere Fassungen ohne
     * diese Bitte bleiben - dann startet dieser Aufruf nur die Oberfläche (wie bisher). */
    int s = -1, lauscht = 0;
    for (int versuch = 0; versuch < 6 && !lauscht; versuch++) {
        if (versuch) { bitte_beenden(); usleep(700000); }
        s = socket(AF_INET, SOCK_STREAM, 0);
        int ein = 1;
        struct sockaddr_in a; memset(&a, 0, sizeof a);
        a.sin_family = AF_INET; a.sin_port = htons(KACHEL_PORT); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (s >= 0) setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &ein, sizeof ein);
        lauscht = s >= 0 && !bind(s, (struct sockaddr *)&a, sizeof a) && !listen(s, 4);
        if (!lauscht && s >= 0) { close(s); s = -1; }
    }
    if (!lauscht) {                                       /* alter Hintergrundteil bleibt (oder Port belegt) */
        int r = oberflaeche_starten();
        sceUserServiceTerminate();
        return r ? 1 : 0;
    }

    oberflaeche_starten();
    klog_printf("PS5 Cleanup: Hintergrundteil läuft (Prozess %d), wartet auf 127.0.0.1:%d\n", (int)getpid(), KACHEL_PORT);

    time_t zuletzt = time(NULL);
    /* NEU 1.0.6: Auftrag zur Selbstentfernung (da_Chris86 29.10.2026 23:08) */
    int auftrag = 0, a_kachel = 0, a_start = 0, neustart = 0;   /* neustart: 1 = neu starten, 2 = ausschalten */
    pid_t gesehen = 0; time_t auftrag_seit = 0, zuletzt_gemeldet = 0, weg_seit = 0, unklar_seit = 0;
    static entfernung_t ent; int ent_laeuft = 0;
    for (;;) {
        time_t jetzt = time(NULL);
        if (ent_laeuft && ent.fertig) {                    /* Entfernung abgeschlossen: Ergebnis melden, ggf. Neustart, beenden */
            melde(ent.erg);                                /* Benachrichtigung + Kernel-Protokoll */
            close(s);
            if (neustart == 1) {
                sleep(3);
                int e = sceSystemServiceRequestReboot();
                selbstweg_log("Neustart der PS5 angefordert -> 0x%08X", (unsigned)e);
                if (e) melde("PS5 Cleanup: Der Neustart hat nicht geklappt - bitte über das PS5-Menü neu starten.");
            } else if (neustart == 2) {                    /* NEU: ganz ausschalten (Datenbank-Neuaufbau), kein Ruhemodus */
                sleep(3);
                int e = ps5_ausschalten();
                selbstweg_log("Ausschalten der PS5 angefordert -> %d (0x%08X)", e, (unsigned)e);
                if (e) melde("PS5 Cleanup: Das Ausschalten hat nicht geklappt - bitte die PS5 über das PS5-Menü ausschalten (nicht Ruhemodus).");
            }
            selbstweg_log("Hintergrundteil beendet sich");
            sceUserServiceTerminate();
            return 0;
        }
        if (auftrag && !ent_laeuft) {   /* nachweislich auf das Ende der Oberfläche warten (alle 0,5 s nachsehen) */
            pid_t p = start_hbldr_find_pid(UI_NAME);
            if (p > 0) {
                weg_seit = 0; unklar_seit = 0;
                if (p != gesehen) { gesehen = p; selbstweg_log("warte auf das Ende der Oberfläche (Prozess %d)", (int)p); zuletzt_gemeldet = jetzt; }
                else if (jetzt - zuletzt_gemeldet >= 120) { selbstweg_log("Oberfläche läuft noch (Prozess %d, seit %ld s)", (int)p, (long)(jetzt - auftrag_seit)); zuletzt_gemeldet = jetzt; }
            } else if (p == -1 && gesehen == 0) {          /* nachweislich warten: die Oberfläche muss vorher gesehen worden sein */
                unklar_seit = 0;
                if (jetzt - auftrag_seit > 10) {
                    selbstweg_log("ABGEBROCHEN: die Oberfläche (Prozess \"%s\") wurde nach dem Auftrag nie gefunden - ihr Ende ist nicht nachweisbar. Nichts entfernt.", UI_NAME);
                    melde("PS5 Cleanup: Selbstentfernung abgebrochen (Ende der Oberfläche nicht nachweisbar) - die Kachel bleibt.");
                    auftrag = 0; neustart = 0;
                }
            } else if (p == -1) {
                unklar_seit = 0;
                if (!weg_seit) weg_seit = jetzt;
                if (jetzt - weg_seit >= 3) {                   /* 3 s lang nicht mehr da: beendet (Review: nach Zeit, nicht nach Durchläufen) */
                    selbstweg_log("Oberfläche beendet (zuletzt gesehen: Prozess %d) - %ld s nach der Übernahme, beginne", (int)gesehen, (long)(jetzt - auftrag_seit));
                    memset(&ent, 0, sizeof ent); ent.kachel = a_kachel; ent.start = a_start;
                    pthread_t th;
                    if (pthread_create(&th, NULL, entfernung_faden, &ent) == 0) { pthread_detach(th); ent_laeuft = 1; }
                    else { selbstweg_log("Faden für die Entfernung ließ sich nicht starten - Entfernung im Hauptteil"); entfernung_faden(&ent); ent_laeuft = 1; }
                }
            } else {                                       /* -2: nicht feststellbar */
                weg_seit = 0;
                if (!unklar_seit) unklar_seit = jetzt;
                if (jetzt - unklar_seit >= 60) {
                    selbstweg_log("ABGEBROCHEN: Prozessliste 60 s lang nicht lesbar - ob die Oberfläche beendet ist, ist unklar. Nichts entfernt.");
                    melde("PS5 Cleanup: Selbstentfernung abgebrochen (unklar, ob PS5 Cleanup beendet ist) - die Kachel bleibt.");
                    auftrag = 0; neustart = 0; unklar_seit = 0;
                }
            }
        }
        fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds);
        struct timeval tw = { 0, 500000 };
        int bereit = select(s + 1, &fds, NULL, NULL, &tw);
        if (bereit < 0 && errno != EINTR) usleep(100000);  /* Review: nie Vollast bei einem Fehler */
        if (bereit <= 0) continue;                             /* Zeit um (oder unterbrochen) */
        int c = accept(s, NULL, NULL);
        if (c < 0) { if (errno == EINTR) continue; usleep(500000); continue; }
        struct timeval zeit = { 2, 0 };                    /* Codex-Prüfung 26: eine stumme Verbindung blockiert nie die Kachel */
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &zeit, sizeof zeit);
        setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &zeit, sizeof zeit);
        char puf[1024];
        ssize_t n = recv(c, puf, sizeof puf - 1, 0);
        puf[n > 0 ? n : 0] = 0;
        if (!strncmp(puf, "GET /ps5cleanup-status", 22)) {      /* NEU 1.0.6 (Review): erneuter Start fragt nach */
            static const char JA[] = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nPS5CLEANUP-ENTFERNT-SICH";
            static const char NORMAL[] = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nPS5CLEANUP-LAEUFT";
            (void)send(c, auftrag ? JA : NORMAL, (auftrag ? sizeof JA : sizeof NORMAL) - 1, 0); close(c);
            continue;
        }
        if (!strncmp(puf, "GET /ps5cleanup-selbstweg?", 26)) {   /* NEU 1.0.6: Auftrag der Oberfläche */
#ifdef SIMULATION
            static const char NEIN[] = "HTTP/1.1 409 Conflict\r\nConnection: close\r\n\r\nPS5CLEANUP-SIMULATION";
            (void)send(c, NEIN, sizeof NEIN - 1, 0); close(c);
            selbstweg_log("Auftrag abgelehnt - Simulation (hier wird nichts entfernt)");
            continue;
#else
            if (ent_laeuft) {                              /* Entfernung läuft schon - kein zweiter Auftrag */
                static const char NEIN[] = "HTTP/1.1 409 Conflict\r\nConnection: close\r\n\r\nPS5CLEANUP-ENTFERNT-SICH";
                (void)send(c, NEIN, sizeof NEIN - 1, 0); close(c);
                continue;
            }
            int k = strstr(puf, "kachel=1") != NULL, st = strstr(puf, "start=1") != NULL;
            if (!auftrag) { auftrag = 1; a_kachel = k; a_start = st; neustart = 0; weg_seit = 0; unklar_seit = 0; gesehen = 0; auftrag_seit = time(NULL); }
            static const char JA[] = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nPS5CLEANUP-UEBERNOMMEN";
            (void)send(c, JA, sizeof JA - 1, 0); close(c);
            selbstweg_log("Auftrag übernommen (Kachel %s, Startordner %s) - entfernt wird erst nach dem Ende der Oberfläche", a_kachel ? "ja" : "nein", a_start ? "ja" : "nein");
            continue;
#endif
        }
        if (!strncmp(puf, "GET /ps5cleanup-neustart", 24)) {     /* NEU 1.0.6: "Neu starten" erst nach der Entfernung */
            static const char JA[] = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nPS5CLEANUP-NEUSTART-OK";
            static const char NEIN[] = "HTTP/1.1 409 Conflict\r\nConnection: close\r\n\r\nPS5CLEANUP-KEIN-AUFTRAG";
            if (auftrag) neustart = 1;
            (void)send(c, auftrag ? JA : NEIN, (auftrag ? sizeof JA : sizeof NEIN) - 1, 0); close(c);
            selbstweg_log("Neustart nach der Entfernung: %s", auftrag ? "übernommen" : "abgelehnt (kein Auftrag)");
            continue;
        }
        if (!strncmp(puf, "GET /ps5cleanup-ausschalten", 27)) {  /* NEU (da_Chris86 30.10.2026 07:29): "Ausschalten" erst nach der Entfernung */
            static const char JA[] = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nPS5CLEANUP-AUSSCHALTEN-OK";
            static const char NEIN[] = "HTTP/1.1 409 Conflict\r\nConnection: close\r\n\r\nPS5CLEANUP-KEIN-AUFTRAG";
            if (auftrag) neustart = 2;
            (void)send(c, auftrag ? JA : NEIN, (auftrag ? sizeof JA : sizeof NEIN) - 1, 0); close(c);
            selbstweg_log("Ausschalten nach der Entfernung: %s", auftrag ? "übernommen" : "abgelehnt (kein Auftrag)");
            continue;
        }
        if (!strncmp(puf, "GET /ps5cleanup-ende", 20)) {  /* neuere Fassung übernimmt */
            if (auftrag) {                                 /* nie mitten in einer Selbstentfernung */
                static const char NEIN[] = "HTTP/1.1 409 Conflict\r\nConnection: close\r\n\r\nPS5CLEANUP-ENTFERNT-SICH";
                (void)send(c, NEIN, sizeof NEIN - 1, 0); close(c);
                continue;
            }
            static const char OK[] = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n";
            (void)send(c, OK, sizeof OK - 1, 0);
            close(c); close(s);
            klog_printf("PS5 Cleanup: Hintergrundteil beendet (neuere Fassung übernimmt)\n");
            sceUserServiceTerminate();
            return 0;
        }
        if (strncmp(puf, "GET / ", 6)) {                  /* z. B. favicon.ico: nur antworten, nichts starten */
            static const char NF[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            (void)send(c, NF, sizeof NF - 1, 0);
            close(c);
            continue;
        }
        if (auftrag) {                                     /* Kachel während der Selbstentfernung: nichts mehr starten */
            static const char WEG[] =
                "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n"
                "<!doctype html><html><head><meta charset=\"utf-8\"><title>PS5 Cleanup</title></head>"
                "<body style=\"background:#0b1020;color:#fff;font:34px sans-serif;text-align:center;padding-top:18%\">"
                "PS5 Cleanup wird entfernt.<br><br><span style=\"font-size:26px;color:#9fb4ff\">Kreis-Taste dr&uuml;cken.<br>"
                "PS5 Cleanup is being removed. Press Circle.</span></body></html>";
            (void)send(c, WEG, sizeof WEG - 1, 0);
            close(c);
            continue;
        }
        (void)send(c, ANTWORT, sizeof ANTWORT - 1, 0);
        close(c);
        if (jetzt - zuletzt < 10) continue;               /* Doppelaufrufe (z. B. favicon) nicht zweimal starten */
        zuletzt = jetzt;
        usleep(500000);
        oberflaeche_starten();
    }
}
