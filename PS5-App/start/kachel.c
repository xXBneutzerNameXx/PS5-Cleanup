/* PS5 Cleanup 1.0.6 - eigene Startbildschirm-Kachel - siehe kachel.h
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * Vorbild (nur Vorgehen, kein Code kopiert): websrv (John Törnblom, GPL-3.0) src/ps5/sys.c install_app()
 * (Einzel-Eintrag per NID, sonst AppInstallAll), PS5Upload (phantomptr, GPL-3.0) Web-Kachel mit deeplinkUri.
 *
 * da_Chris86 28.10.2026: Kachel über Sonys Installationsdienst ist freigegeben, sie wird beim ersten Start
 * automatisch angelegt. Auf FW 12.00 gibt es nur noch AppInstallAll - das trägt auch andere App-Ordner ohne
 * Kachel ein; da_Chris86: "trotzdem anlegen". Web-Kacheln bekommen keine appmeta-Ordner (auf seiner PS5
 * belegt: PLDM00001, FAKE10101, FMGR88888, PKGM00001 haben Kacheln, aber keine appmeta) - deshalb merkt sich
 * PS5 Cleanup den erfolgreichen Eintrag mit einer eigenen Markierung.
 */
#include "kachel.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ps5/kernel.h>
#include <ps5/klog.h>

int sceAppInstUtilInitialize(void);
int sceAppInstUtilAppInstallAll(void *reserved);

#define APP_DIR    "/user/app/" KACHEL_ID
#define SYS_DIR    APP_DIR "/sce_sys"
#define MARKIERUNG APP_DIR "/.ps5cleanup_kachel"
#define EINGETRAGEN APP_DIR "/.ps5cleanup_eingetragen"

/* da_Chris86 29.10.2026 19:31: ohne "contentId" - damit sucht die PS5 keine Lizenz (Schloss auf der Kachel); wie die Kacheln
 * von Payload Manager, WebKit-Autoloader, PS5Upload und ShadowMountPlus (alle ohne contentId, ohne Schloss) */
static const char PARAM_JSON[] =
"{\n"
"  \"applicationCategoryType\": 66048,\n"
"  \"titleId\": \"" KACHEL_ID "\",\n"
"  \"deeplinkUri\": \"http://127.0.0.1:9099/\",\n"
"  \"localizedParameters\": {\n"
"    \"defaultLanguage\": \"en-US\",\n"
"    \"en-US\": { \"titleName\": \"PS5 Cleanup\" },\n"
"    \"de-DE\": { \"titleName\": \"PS5 Cleanup\" }\n"
"  }\n"
"}\n";

static int da(const char *p) { struct stat s; return lstat(p, &s) == 0; }
/* Codex-Prüfung 26: nur echte Ordner/Dateien (keine Verknüpfungen), Dateien mit genau der erwarteten Größe */
static int ordner(const char *p) { struct stat s; return lstat(p, &s) == 0 && S_ISDIR(s.st_mode); }
static int datei_gross(const char *p, size_t n) { struct stat s; return lstat(p, &s) == 0 && S_ISREG(s.st_mode) && (size_t)s.st_size == n; }

unsigned kachel_firmware(void)
{
    uint32_t raw = kernel_get_fw_version();             /* z. B. 0x12000xxx (BCD) */
    unsigned hi = (raw >> 24) & 0xFF, lo = (raw >> 16) & 0xFF;
    if (!raw) return 0;
    return ((hi >> 4) * 10 + (hi & 0xF)) * 100 + (lo >> 4) * 10 + (lo & 0xF);
}

static int schreibe(const char *pfad, const void *daten, size_t len)
{
    int fd = open(pfad, O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW, 0644);   /* nie einer Verknüpfung folgen (Codex-Prüfung 26) */
    if (fd < 0) return -1;
    size_t ges = 0;
    while (ges < len) {
        ssize_t w = write(fd, (const char *)daten + ges, len - ges);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) { close(fd); return -1; }
        ges += (size_t)w;
    }
    return close(fd) ? -1 : 0;
}

/* Einzel-Eintrag (gibt es auf FW 12 nicht mehr) - wie websrv im laufenden Prozess nachgeschlagen */
static int (*finde_titledir(void))(const char *, const char *, void *)
{
    uint32_t h = 0;
    if (kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &h)) return NULL;
    return (int (*)(const char *, const char *, void *))kernel_dynlib_resolve(-1, h, "Wudg3Xe3heE");
}

int kachel_einrichten(const unsigned char *icon, size_t icon_len, int *sony, char *text, size_t text_len)
{
    *sony = 0;
    if (da(APP_DIR) && !da(MARKIERUNG)) {
        snprintf(text, text_len, "Kennung %s gehört einer anderen App - keine eigene Kachel angelegt.", KACHEL_ID);
        return KACHEL_FREMD;
    }
    if (da(APP_DIR) && !ordner(APP_DIR)) {
        snprintf(text, text_len, "%s ist kein Ordner - keine Kachel angelegt.", APP_DIR);
        return KACHEL_FREMD;
    }
    if (da(EINGETRAGEN) && datei_gross(SYS_DIR "/param.json", sizeof PARAM_JSON - 1) && datei_gross(SYS_DIR "/icon0.png", icon_len)) {
        snprintf(text, text_len, "Kachel ist schon da.");
        return KACHEL_SCHON_DA;
    }
    /* Markierung gleich nach dem Ordner - so bleibt nie ein unmarkierter eigener Ordner zurück (Codex-Prüfung 26) */
    if ((!da(APP_DIR) && mkdir(APP_DIR, 0755)) || schreibe(MARKIERUNG, "PS5CLEANUP-KACHEL\n", 18)
        || (!da(SYS_DIR) && mkdir(SYS_DIR, 0755)) || !ordner(SYS_DIR)
        || schreibe(SYS_DIR "/param.json", PARAM_JSON, sizeof PARAM_JSON - 1)
        || schreibe(SYS_DIR "/icon0.png", icon, icon_len)) {
        snprintf(text, text_len, "Kachel-Dateien ließen sich nicht anlegen (%s).", strerror(errno));
        return KACHEL_SCHREIBFEHLER;
    }
    int err = sceAppInstUtilInitialize();
    if (err) { *sony = err; snprintf(text, text_len, "Installationsdienst nicht verfügbar (0x%08X) - keine Kachel.", (unsigned)err); return KACHEL_KEIN_DIENST; }
    int (*title_dir)(const char *, const char *, void *) = finde_titledir();
    err = title_dir ? title_dir(KACHEL_ID, "/user/app/", NULL) : sceAppInstUtilAppInstallAll(NULL);
    klog_printf("PS5 Cleanup Kachel: FW %u, %s -> 0x%08X\n", kachel_firmware(), title_dir ? "AppInstallTitleDir" : "AppInstallAll", (unsigned)err);
    if (err) { *sony = err; snprintf(text, text_len, "Sonys Installationsdienst meldet 0x%08X - keine Kachel.", (unsigned)err); return KACHEL_SONY_FEHLER; }
    schreibe(EINGETRAGEN, "1\n", 2);
    snprintf(text, text_len, "Kachel \"PS5 Cleanup\" angelegt.");
    return KACHEL_OK;
}
