/* PS5 Cleanup 1.0.6 - abschließende Selbstentfernung durch den Hintergrundteil - siehe selbstweg.h
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * Sonys Dienst: dieselben Aufrufe und Zeitgrenzen wie in app/plattform_ps5.c (Initialisierung 10 s, Statusabfrage 5 s,
 * Deinstallation 30 s, Abschluss höchstens 60 s), jeder Aufruf in einem eigenen Faden (Codex-Hinweis 29.10.2026 22:11).
 * sceAppInstUtilAppExists schreibt nur EIN Byte (PS5-Test 29.10.2026 22:29) - ausgewertet wird das unterste Byte.
 * Die Simulations-Fassung (-DSIMULATION) enthält keinen einzigen löschenden Aufruf.
 */
#include "selbstweg.h"
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ps5/klog.h>
#ifndef SIMULATION
#include <fcntl.h>
#include "startordner.h"   /* nur die echte Fassung: Simulation ohne jeden löschenden Aufruf */
#endif
#ifndef SO_WURZEL
#define SO_WURZEL ""
#endif

#define SW_APP      SO_WURZEL "/user/app/" SW_KACHEL_ID
#define SW_MARKE    SW_APP "/.ps5cleanup_kachel"
#define SW_MARKE_TEXT "PS5CLEANUP-KACHEL\n"
#define SW_APPMETA  SO_WURZEL "/user/appmeta/" SW_KACHEL_ID
#define SW_SYSMETA  SO_WURZEL "/system_data/priv/appmeta/" SW_KACHEL_ID

void selbstweg_log(const char *fmt, ...)
{
    char t[900];
    va_list v; va_start(v, fmt); vsnprintf(t, sizeof t, fmt, v); va_end(v);
    klog_printf("PS5 Cleanup Selbstentfernung: %s\n", t);
}
static void sw_warte(int ms) { struct timespec z = { ms / 1000, (long)(ms % 1000) * 1000000L }; nanosleep(&z, NULL); }
/* 1 = da oder nicht feststellbar (vorsichtig), 0 = nachweislich nicht da (ENOENT) - Review 30.10.2026 */
static int sw_da(const char *p) { struct stat st; return lstat(p, &st) == 0 || errno != ENOENT; }

#ifndef SIMULATION
int sceAppInstUtilInitialize(void);
int sceAppInstUtilAppUnInstall(const char *title_id);
int sceAppInstUtilAppExists(const char *title_id, int *exists);

typedef struct { int art; volatile int fertig; int e; int da; } sw_auftrag;   /* art: 0 Init, 1 eingetragen?, 2 Deinstallation */
static void *sw_faden(void *x)
{
    sw_auftrag *a = x;
    if (a->art == 0) a->e = sceAppInstUtilInitialize();
    else if (a->art == 1) { int d = -1; a->e = sceAppInstUtilAppExists(SW_KACHEL_ID, &d); a->da = d; }
    else a->e = sceAppInstUtilAppUnInstall(SW_KACHEL_ID);
    a->fertig = 1;
    return NULL;
}
static int sw_gestoert = 0;
/* 1 = Antwort (*e, *da), 0 = keine Antwort in ms (der Faden bleibt zurück, Sonys Dienst wird danach nicht mehr benutzt) */
static int sw_rufe(int art, int ms, int *e, int *da)
{
    if (sw_gestoert) return 0;
    sw_auftrag *au = calloc(1, sizeof *au);
    if (!au) { *e = -1; return 1; }
    au->art = art;
    pthread_t th;
    if (pthread_create(&th, NULL, sw_faden, au)) { free(au); *e = -1; return 1; }
    pthread_detach(th);
    for (int i = 0; i < ms / 100 && !au->fertig; i++) sw_warte(100);
    if (!au->fertig) { sw_gestoert = 1; selbstweg_log("Sony-Aufruf %d - keine Antwort nach %d ms", art, ms); return 0; }
    *e = au->e; if (da) *da = au->da; free(au);
    return 1;
}
/* 1 = eingetragen, 0 = nicht eingetragen, -1 = nicht prüfbar, -2 = keine Antwort */
static int sw_eingetragen(void) { int e = 0, d = -1; if (!sw_rufe(1, 5000, &e, &d)) return -2; if (e) return -1; int b = d & 0xFF; return b == 1 ? 1 : (b == 0 ? 0 : -1); }

/* 1 = eindeutig die eigene Kachel (Ordner + Markierung mit genau diesem Inhalt) */
static int sw_eigene_kachel(void)
{
    struct stat st;
    if (lstat(SW_APP, &st) || !S_ISDIR(st.st_mode)) return 0;
    if (lstat(SW_MARKE, &st) || !S_ISREG(st.st_mode) || st.st_size != (off_t)(sizeof SW_MARKE_TEXT - 1)) return 0;
    char p[32] = {0};
    int fd = open(SW_MARKE, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return 0;
    ssize_t n = read(fd, p, sizeof p - 1); close(fd);
    return n == (ssize_t)(sizeof SW_MARKE_TEXT - 1) && !memcmp(p, SW_MARKE_TEXT, (size_t)n);
}

int selbstweg_kachel(char *g, size_t gl)
{
    g[0] = 0;
    int ordner = sw_da(SW_APP), eigen = ordner && sw_eigene_kachel();
    selbstweg_log("Kachel %s - vorher: Ordner %s, Markierung %s", SW_KACHEL_ID, ordner ? "da" : "nicht da", eigen ? "eigene" : (ordner ? "FEHLT/fremd" : "-"));
    if (ordner && !eigen) { snprintf(g, gl, "Kachel %s gehört nicht eindeutig zu PS5 Cleanup - nicht angefasst", SW_KACHEL_ID); return -1; }
    int e = 0;
    if (!sw_rufe(0, 10000, &e, NULL)) { snprintf(g, gl, "Sonys Dienst antwortet nicht (Initialisierung, 10 s)"); return -1; }
    selbstweg_log("Sony Initialisierung -> 0x%08X%s", (unsigned)e, e ? " (evtl. schon initialisiert - es geht weiter, die Statusabfrage entscheidet)" : "");
    /* Review 30.10.2026: im selben Prozess hat kachel_einrichten den Dienst evtl. schon initialisiert - ein zweiter Aufruf
     * kann einen Fehlercode liefern. Entscheidend ist, ob die Statusabfrage danach antwortet. */
    int vorher = sw_eingetragen();
    selbstweg_log("Kachel %s - vorher eingetragen=%d (1 ja, 0 nein, -1 nicht prüfbar, -2 keine Antwort)", SW_KACHEL_ID, vorher);
    if (vorher == -2) { snprintf(g, gl, "Sonys Statusabfrage antwortet nicht (5 s)"); return -1; }
    if (vorher < 0) { snprintf(g, gl, "ob die Kachel eingetragen ist, ließ sich nicht prüfen"); return -1; }
    if (vorher == 0 && !ordner) { selbstweg_log("Kachel %s - war nicht da", SW_KACHEL_ID); return 0; }
    if (vorher == 0) {   /* Ordner da, aber nicht eingetragen: keine Kachel im Menü - Sony kann hier nichts austragen */
        snprintf(g, gl, "Kachel-Ordner %s ist da, aber nicht eingetragen - Sony kann ihn nicht austragen, er bleibt", SW_APP);
        return -1;
    }
    if (!ordner) { snprintf(g, gl, "Kachel %s ist eingetragen, aber ihr Ordner fehlt - nicht eindeutig eigene, nicht angefasst", SW_KACHEL_ID); return -1; }
    if (!sw_rufe(2, 30000, &e, NULL)) { snprintf(g, gl, "Sony antwortet nicht (Deinstallation, 30 s)"); return -1; }
    selbstweg_log("Sony-Deinstallation %s -> 0x%08X", SW_KACHEL_ID, (unsigned)e);
    if (e) { snprintf(g, gl, "Entfernung unvollständig - Sony hat abgelehnt (0x%08X)", (unsigned)e); return -1; }
    int jetzt = 1, s = 0;
    for (; s < 60 && (jetzt = sw_eingetragen()) == 1; s++) sw_warte(1000);
    int o = 0;                               /* App-Ordner und Kachel-Infos: gemeinsam höchstens 60 s (Review 30.10.2026) */
    for (; o < 60 && (sw_da(SW_APP) || sw_da(SW_APPMETA) || sw_da(SW_SYSMETA)); o++) sw_warte(1000);
    int ordner_nach = sw_da(SW_APP), am = sw_da(SW_APPMETA), sm = sw_da(SW_SYSMETA);
    selbstweg_log("Kachel %s - nachher: eingetragen=%d (nach %d s), Ordner %s (nach %d s), appmeta %s, Systemdaten-appmeta %s",
                  SW_KACHEL_ID, jetzt, s, ordner_nach ? "NOCH DA" : "weg", o, am ? "NOCH DA" : "weg", sm ? "NOCH DA" : "weg");
    if (jetzt == 1) { snprintf(g, gl, "Sony hat angenommen, die Kachel ist nach 60 s aber noch eingetragen"); return -1; }
    if (jetzt < 0) { snprintf(g, gl, "Sony hat angenommen, ob die Kachel ausgetragen ist, ließ sich nicht prüfen"); return -1; }
    /* da_Chris86 29.10.2026: bloßes Dateilöschen ist keine Deinstallation - Reste werden gemeldet, nicht selbst gelöscht.
     * Review 30.10.2026: Ist die Kachel ausgetragen und ihr App-Ordner weg, ist sie aus dem Menü - übrige Kachel-Infos nur protokollieren. */
    if (ordner_nach) { snprintf(g, gl, "Kachel ausgetragen, aber Sony hat %s nach 60 s noch nicht gelöscht", SW_APP); return -1; }
    if (am || sm) selbstweg_log("Kachel aus dem Menü; Rest bleibt: %s%s%s", am ? SW_APPMETA : "", am && sm ? ", " : "", sm ? SW_SYSMETA : "");
    return 1;
}

/* Eigene Laufzeit-Einhängungen der beendeten App (/mnt/sandbox/PSCL00002_<Ziffern>...) zählen - nur lesen (getfsstat).
 * -1 = unbekannt (Liste nicht lesbar/unvollständig, /system_ex fehlt in der Liste). Nach Codex app_mount_ende.h (30.10.2026),
 * ohne das Beenden fremder oder eigener Apps. */
#ifndef SW_TEST_MOUNTS
#include <sys/mount.h>
static int sw_eigene_mounts(void)
{
    int n = getfsstat(NULL, 0, MNT_NOWAIT);
    if (n < 0 || n > 8160) return -1;
    int cap = n + 32;
    struct statfs *m = calloc((size_t)cap, sizeof *m);
    if (!m) return -1;
    int got = getfsstat(m, (long)((size_t)cap * sizeof *m), MNT_NOWAIT);
    if (got < 0 || got >= cap) { free(m); return -1; }
    static const char pre[] = "/mnt/sandbox/" SO_ID "_";
    int gefunden = 0, global = 0;
    for (int i = 0; i < got; i++) {
        const char *p = m[i].f_mntonname;
        if (!strcmp(p, "/system_ex")) global = 1;
        if (!strncmp(p, pre, sizeof pre - 1)) {
            const char *q = p + sizeof pre - 1; int z = 0;
            while (*q >= '0' && *q <= '9') { z++; q++; }
            if (z && (!*q || *q == '/')) gefunden++;
        }
    }
    free(m);
    return global ? gefunden : -1;
}
#else
int sw_eigene_mounts(void);   /* PC-Test */
#endif
/* PS5-Test Codex 29.10. 23:50: Schreibschutz zurückstellen scheiterte ("Operation already in progress"), solange Sony die
 * beendete App noch abbaute (EndAppMount kam erst danach). Darum vorher höchstens 60 s warten, bis ihre Einhängungen weg sind. */
static int sw_app_abbau_abwarten(void)
{
    int m = -1;
    for (int s = 0; s <= 60; s++) {
        m = sw_eigene_mounts();
        if (m == 0) { selbstweg_log("Laufzeit-Einhängungen der beendeten Oberfläche abgebaut (nach %d s)", s); return 1; }
        if (s % 10 == 0) selbstweg_log("warte auf den Abbau der beendeten Oberfläche: %d eigene Einhängungen (-1 = nicht feststellbar), %d s", m, s);
        sw_warte(1000);
    }
    selbstweg_log("Abbau der beendeten Oberfläche nach 60 s nicht bestätigt (%d) - es wird trotzdem versucht, Rückstellung mit Wiederholung", m);
    return 0;
}

int selbstweg_startordner(char *g, size_t gl)
{
    g[0] = 0;
    int ro_vorher = so_ist_ro(), z = so_zustand();
    selbstweg_log("Startordner %s - vorher: Zustand %d (1 eigener, 0 nicht da, -1 fremd/unklar), /system_ex schreibgeschützt=%d", SO_DIR, z, ro_vorher);
    if (z == 0) return 0;
    if (z < 0) { snprintf(g, gl, "%s gehört nicht eindeutig zu PS5 Cleanup - nichts gelöscht", SO_DIR); return -1; }
    if (ro_vorher < 0) { snprintf(g, gl, "Einhängezustand von /system_ex unbekannt - Startordner nicht gelöscht"); return -1; }
    sw_app_abbau_abwarten();
    char t[300];
    int r = so_entfernen(t, sizeof t);      /* höchstens EIN Wechsel auf beschreibbar (PS5: einmal pro Einschalten) */
    selbstweg_log("Startordner entfernen -> %d %s (schreibgeschützt jetzt=%d)", r, t, so_ist_ro());
    if (r == -2) { snprintf(g, gl, "%s", t); return -2; }   /* da_Chris86 30.10.2026 00:26: dann ohne Hinweis lassen */
    int z_nach = so_zustand(), ro_nach = so_ist_ro();
    selbstweg_log("Startordner %s - nachher: Zustand %d, Ordner %s, /system_ex schreibgeschützt=%d (vorher %d)", SO_DIR, z_nach, sw_da(SO_DIR) ? "NOCH DA" : "weg", ro_nach, ro_vorher);
    if (r != 1 || z_nach != 0 || sw_da(SO_DIR)) { snprintf(g, gl, "%s", t[0] ? t : "Startordner nicht ganz gelöscht"); return -1; }
    if (ro_nach != ro_vorher) { snprintf(g, gl, "Startordner gelöscht, aber /system_ex ist nicht wieder wie vorher eingehängt (schreibgeschützt=%d, vorher %d)", ro_nach, ro_vorher); return -1; }
    return 1;
}

int selbstweg_ausfuehren(int kachel, int startordner, char *erg, size_t el)
{
    selbstweg_log("Beginn - Kachel %s, Startordner %s, Hintergrundteil Prozess %d", kachel ? "ja" : "nein", startordner ? "ja" : "nein", (int)getpid());
    char kg[400] = "", sg[400] = "";
    int k = kachel ? selbstweg_kachel(kg, sizeof kg) : 0;
    if (kachel) selbstweg_log("Kachel -> %d %s", k, kg);
    int s = 0;
    if (startordner) {
        /* ohne ausgetragene Kachel bleibt der Startordner - sonst bliebe eine Kachel, die nichts mehr starten kann */
        if (kachel && k < 0) { s = -3; snprintf(sg, sizeof sg, "bleibt, weil die Kachel nicht entfernt ist"); }   /* -3: eigener Code (Review 30.10.2026) */
        else s = selbstweg_startordner(sg, sizeof sg);
        selbstweg_log("Startordner -> %d %s", s, sg);
    }
    /* da_Chris86 30.10.2026 00:26: zum Startordner gibt es keinen Hinweis - er steht nur im Kernel-Protokoll (oben) */
    int voll = !kachel || k >= 0;
    if (voll) snprintf(erg, el, "PS5 Cleanup wurde entfernt.");
    else snprintf(erg, el, "PS5 Cleanup: Die Kachel ließ sich nicht entfernen (%s).", kg);
    selbstweg_log("Zusammenfassung: Kachel %d, Startordner %d (%s), Schreibschutz jetzt=%d", k, s, sg, so_ist_ro());
    selbstweg_log("ERGEBNIS: %s", voll ? "Kachel entfernt (geprüft)" : erg);
    return voll;
}
#else
/* Simulation: nie etwas löschen - der Hintergrundteil nimmt keinen Auftrag an (ps5cleanup.c) */
int selbstweg_kachel(char *g, size_t gl) { snprintf(g, gl, "Simulation"); return -1; }
int selbstweg_startordner(char *g, size_t gl) { snprintf(g, gl, "Simulation"); return -1; }
int selbstweg_ausfuehren(int kachel, int startordner, char *erg, size_t el) { (void)kachel; (void)startordner; snprintf(erg, el, "SIMULATION: nichts entfernt."); selbstweg_log("ERGEBNIS: Simulation - nichts entfernt"); return 0; }
#endif
