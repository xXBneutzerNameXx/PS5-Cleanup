/* PS5 Cleanup - Protokoll-Datei auf dem USB-Stick für den Hintergrundteil - siehe sw_protokoll.h
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 */
#include "sw_protokoll.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifndef SIMULATION
#include <fcntl.h>
#include <unistd.h>
#endif
#ifndef SW_PROT_WURZEL
#define SW_PROT_WURZEL ""   /* PC-Test: Testordner davor */
#endif

static char sw_prot[128];

int sw_protokoll_setzen(const char *pfad, size_t n)
{
    static const char vorn[] = "/mnt/usb", mitte[] = "/ps5cleanup_protokoll_";
    /* da_Chris86 02.11.2026 16:57/17:23: ohne (vollen) USB-Stick - interne Platte, dann M.2; je Lauf eine Datei */
    static const char *const ordner[2] = { "/data/PS5-Cleanup-Protokolle", "/mnt/ext1/PS5-Cleanup-Protokolle" };
    sw_prot[0] = 0;
    size_t lv = sizeof vorn - 1, lm = sizeof mitte - 1, k = 0;   /* k: wo "/ps5cleanup_protokoll_" beginnt */
    if (!pfad || n >= sizeof sw_prot) return 0;
    for (int i = 0; i < 2 && !k; i++) { size_t l = strlen(ordner[i]); if (n > l && !memcmp(pfad, ordner[i], l)) k = l; }
    if (!k && n > lv + 1 && !memcmp(pfad, vorn, lv) && pfad[lv] >= '0' && pfad[lv] <= '7') k = lv + 1;   /* /mnt/usb0 .. /mnt/usb7 */
    if (!k) return 0;
    if (n < k + lm + 5 || memcmp(pfad + k, mitte, lm)) return 0;
    if (memcmp(pfad + n - 4, ".txt", 4)) return 0;
    for (size_t i = k + lm; i < n - 4; i++) if (!((pfad[i] >= '0' && pfad[i] <= '9') || pfad[i] == '_' || pfad[i] == '-')) return 0;
    memcpy(sw_prot, pfad, n); sw_prot[n] = 0;
    return 1;
}

void sw_protokoll_zeile(const char *t)
{
#ifdef SIMULATION
    (void)t;
#else
    if (!sw_prot[0]) return;
    char p[sizeof SW_PROT_WURZEL + sizeof sw_prot];
    snprintf(p, sizeof p, "%s%s", SW_PROT_WURZEL, sw_prot);
    int fd = open(p, O_WRONLY | O_APPEND | O_NOFOLLOW);   /* ohne O_CREAT: die Datei hat die Oberfläche angelegt */
    if (fd < 0) return;
    char z[960]; time_t j = time(NULL); struct tm tm; localtime_r(&j, &tm);
    int l = snprintf(z, sizeof z, "%02d:%02d:%02d Selbstentfernung: %s\r\n", tm.tm_hour, tm.tm_min, tm.tm_sec, t ? t : "");
    if (l > 0) (void)!write(fd, z, (size_t)(l < (int)sizeof z ? l : (int)sizeof z - 1));
    close(fd);
#endif
}
