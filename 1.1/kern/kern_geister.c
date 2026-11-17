/* PS5 Cleanup - Geisterkacheln: welche Kachel bekommt welche Behandlung - siehe kern.h "GEIST_*"
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * da_Chris86 31.10.2026 21:50 (Punkt 4): Einbindungen lösen und mehrere Deinstallationsversuche nur für nachweislich
 * unterstützte Anwendungen; bei unklarer Zuordnung bleibt die Anwendung unverändert; ShadowMount+ auf FW 12.00 beachten.
 * Belegt ist nur ps5upload: register.c (unregister_title) löst die eigene nullfs-Einbindung /data/ps5upload/tile/PSUP00001 ->
 * /system_ex/app/PSUP00001, deinstalliert über Sony und versucht es bis zu zweimal erneut (webui_tile.c:180: auf FW 13.x
 * entfernt eine Deinstallation nur einen von zwei Einträgen). ShadowMount+-Kacheln: im echten Test (FW 12.00, 31.10.2026)
 * verschwanden sie nach dem Anhalten von ShadowMount+ mit EINEM Sony-Auftrag - eine noch bestehende Einbindung bedeutet,
 * dass ShadowMount+ oder kstuff wieder eingebunden hat: dann unverändert lassen.
 * NEU 1.0.7 (da_Chris86 01.11.2026 23:19 "ja soll schon weg", PS5-Test 23:13): kstuff hängt Spiele, die als ORDNER auf der M.2 liegen,
 * sofort wieder ein, sobald ShadowMount+ angehalten ist (klog: "Title Mounted Successfully: <Ordner> -> /system_ex/app/<ID>"). Zeigt die
 * nullfs-Einbindung genau auf den Spielordner, den die Kachel selbst nennt, gehört sie nachweislich zu dieser Kachel (GEIST_EIGENES_SPIEL).
 * Reine Entscheidung - liest und verändert nichts.
 */
#include "kern.h"
#include <string.h>

static bool ps5upload_quelle(const char *q)
{
    static const char *const ERLAUBT[] = { "/data/ps5upload/tile/PSUP00001", "/user/data/ps5upload/tile/PSUP00001", NULL };
    if (!q) return false;
    size_t l = strlen(q);
    while (l > 1 && q[l - 1] == '/') l--;
    for (int i = 0; ERLAUBT[i]; i++) if (strlen(ERLAUBT[i]) == l && !strncmp(q, ERLAUBT[i], l)) return true;
    return false;
}

/* NEU 1.0.7: ein Spielordner an einem Ort, den ShadowMount+/kstuff durchsuchen - nie Systemordner, keine ungewöhnliche Schreibweise */
static bool spielordner_ok(const char *q)
{
    if (!q || !q[0] || strstr(q, "//") || strstr(q, "/./") || strstr(q, "/../") || strchr(q, '\n')) return false;
    size_t l = strlen(q);
    if (l >= 3 && !strcmp(q + l - 3, "/..")) return false;
    static const char *const ORTE[] = { "/mnt/ext0/", "/mnt/ext1/", "/data/", "/user/data/", NULL };
    for (int i = 0; ORTE[i]; i++) if (!strncmp(q, ORTE[i], strlen(ORTE[i])) && q[strlen(ORTE[i])] && q[strlen(ORTE[i])] != '/') return true;
    if (!strncmp(q, "/mnt/usb", 8) && q[8] >= '0' && q[8] <= '9') { const char *s = strchr(q + 8, '/'); return s && s[1] && s[1] != '/'; }
    return false;
}
static const char *ohne_user(const char *p) { return !strncmp(p, "/user/data/", 11) ? p + 5 : p; }   /* "/user/data/x" = "/data/x" */
static bool gleiche_quelle(const char *a, const char *b)
{
    if (!a || !b) return false;
    a = ohne_user(a); b = ohne_user(b);
    size_t la = strlen(a), lb = strlen(b);
    while (la > 1 && a[la - 1] == '/') la--;
    while (lb > 1 && b[lb - 1] == '/') lb--;
    return la > 0 && la == lb && !strncmp(a, b, la);
}

/* NEU 1.0.10 (da_Chris86 14.11.2026 14:28): nur wahr, solange in diesem Löschlauf alle nachgeladenen Hilfsprogramme sauber beendet sind */
static bool hinterlassen_ok = false;
void kern_geist_hinterlassen(bool an) { hinterlassen_ok = an; }

int kern_geist_regel(const char *id, int eingehaengt, const char *fstyp, const char *quelle, const char *eigene_quelle, const char **grund)
{
    *grund = "";
    if (!id || !id[0]) { *grund = T("Kennung fehlt", "ID missing"); return GEIST_UNVERAENDERT; }
    if (eingehaengt == 0) return GEIST_NORMAL;
    if (eingehaengt < 0) { *grund = T("ob dort etwas eingebunden ist, ließ sich nicht feststellen", "whether something is mounted there could not be determined"); return GEIST_UNVERAENDERT; }
    if (!strcmp(id, "PSUP00001") && fstyp && !strcmp(fstyp, "nullfs") && ps5upload_quelle(quelle)) {
        *grund = T("Einbindung von ps5upload - wird vor Sonys Deinstallation gelöst", "mount of ps5upload - released before Sony's uninstall");
        return GEIST_PS5UPLOAD;
    }
    if (fstyp && !strcmp(fstyp, "nullfs") && spielordner_ok(eigene_quelle) && spielordner_ok(quelle) && gleiche_quelle(quelle, eigene_quelle)) {
        *grund = T("Einbindung des eigenen Spielordners (kstuff/ShadowMount) - wird vor Sonys Deinstallation gelöst, der Spielordner bleibt",
                   "mount of the game's own folder (kstuff/ShadowMount) - released before Sony's uninstall, the game folder stays");
        return GEIST_EIGENES_SPIEL;
    }
    if (hinterlassen_ok && fstyp && !strcmp(fstyp, "nullfs") && spielordner_ok(quelle)) {
        *grund = T("Einbindung eines beendeten Hilfsprogramms - wird vor Sonys Deinstallation gelöst, Spieldatei und Spielordner bleiben",
                   "mount left by a closed helper program - released before Sony's uninstall, game file and game folder stay");
        return GEIST_HINTERLASSEN;
    }
    *grund = T("im Systemordner ist noch etwas eingebunden (ShadowMount+, kstuff oder unbekannt) - Zuordnung unklar, darum unverändert",
               "something is still mounted in the system folder (ShadowMount+, kstuff or unknown) - assignment unclear, so left unchanged");
    return GEIST_UNVERAENDERT;
}
