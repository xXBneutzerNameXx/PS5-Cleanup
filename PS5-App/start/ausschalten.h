/* PS5 Cleanup 1.0.6 - PS5 ganz ausschalten (für den Datenbank-Neuaufbau)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 * NEU 1.0.6 (da_Chris86 30.10.2026 07:29): Muss die Datenbank neu aufgebaut werden, bietet das Ende "Ausschalten" statt "Neu starten".
 * sceSystemStateMgrTurnOff schaltet wirklich aus. sceSystemServiceRequestPowerOff folgt der Einstellung des Einschaltknopfs und
 * landet oft im Ruhemodus (Beleg: PS5Upload payload/src/runtime.c, Nutzerbericht) - darum KEIN Ersatz damit: Ruhemodus ist
 * ausgeschlossen (da_Chris86 28.10.2026 23:19), für den Neuaufbau muss die PS5 ganz aus sein.
 * Aufruf über dlsym wie PS5Upload: fehlt der Aufruf in einer Firmware, lädt die ELF trotzdem - es kommt dann nur der Hinweis. */
#ifndef PS5CLEANUP_AUSSCHALTEN_H
#define PS5CLEANUP_AUSSCHALTEN_H
#include <dlfcn.h>
/* 0 = Auftrag angenommen, -1 = Aufruf in dieser Firmware nicht vorhanden, sonst Sonys Fehlercode */
static inline int ps5_ausschalten(void)
{
    void *f = dlsym(RTLD_DEFAULT, "sceSystemStateMgrTurnOff");
    if (!f) return -1;
    return ((int (*)(int))f)(0);       /* 0 = normales Ausschalten (wie PS5Upload) */
}
#endif
