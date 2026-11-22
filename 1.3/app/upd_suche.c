/* PS5 Cleanup - Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * upd_suche.c - NEU 1.3: die eigene Programmdatei für "Update holen - Ja, ersetzen" finden (siehe upd_suche.h). Nur lesen.
 */
#define _GNU_SOURCE
#include "upd_suche.h"
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef UPD_SUCHE_MAX_BYTE
#define UPD_SUCHE_MAX_BYTE (128ull * 1024 * 1024)   /* PS5 Cleanup ist rund 10 MB - größere Dateien werden gar nicht erst gelesen */
#endif
#define UPD_SUCHE_MAX_TREFFER 32

static const char *grundname(const char *p) { const char *s = strrchr(p, '/'); return s ? s + 1 : p; }
static bool endet_elf(const char *n) { size_t l = strlen(n); return l >= 4 && !strcasecmp(n + l - 4, ".elf"); }
/* "_v1", "-v2", " v3" ... im Namen = eine Versionsnummer (wie ps5cleanup_v1.2.elf, PS5-Cleanup-Test_v1.0.7.elf) */
static bool hat_version(const char *n)
{
    if (!n[0]) return false;
    for (size_t i = 1; n[i] && n[i + 1]; i++)
        if ((n[i] == 'v' || n[i] == 'V') && (n[i - 1] == '_' || n[i - 1] == '-' || n[i - 1] == ' ') && isdigit((unsigned char)n[i + 1])) return true;
    return false;
}

int upd_alte_waehlen(const char *const *pfade, int n, const char *version)
{
    char wunsch[96]; snprintf(wunsch, sizeof wunsch, "ps5cleanup_v%s.elf", version ? version : "");
    int genau = -1, ohne = -1, ohne_n = 0;
    for (int i = 0; i < n; i++) {
        const char *p = pfade[i];
        if (!p || p[0] != '/' || strstr(p, "/pldmgr/")) continue;
        const char *b = grundname(p);
        if (!endet_elf(b)) continue;
        if (!strcasecmp(b, wunsch)) {
            bool usb = !strncmp(p, "/mnt/usb", 8);
            if (genau < 0 || (usb && strncmp(pfade[genau], "/mnt/usb", 8))) genau = i;
        } else if (!hat_version(b)) { if (ohne < 0) ohne = i; ohne_n++; }
    }
    if (genau >= 0) return genau;
    return ohne_n == 1 ? ohne : -1;
}

/* das PS5-SDK hat kein memmem */
static bool enthaelt(const char *d, size_t n, const char *s, size_t l)
{
    if (!l || n < l) return false;
    for (size_t i = 0; i + l <= n; i++) if (d[i] == s[0] && !memcmp(d + i, s, l)) return true;
    return false;
}
/* 1 = ELF mit Kennung, 0 = nein, -1 = nicht lesbar */
static int ist_unsere(const char *p, const char *kennung)
{
    struct stat st;
    if (lstat(p, &st) != 0 || !S_ISREG(st.st_mode)) return 0;
    if (st.st_size < 4 || (unsigned long long)st.st_size > UPD_SUCHE_MAX_BYTE) return 0;
    size_t kl = strlen(kennung);
    if (!kl) return 0;
    int fd = open(p, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return -1;
    enum { B = 64 * 1024 };
    char *b = malloc(B + kl);
    if (!b) { close(fd); return -1; }
    size_t rest = 0; bool kopf = false; int r = 0;
    for (;;) {
        ssize_t k = read(fd, b + rest, B);
        if (k < 0) { r = -1; break; }
        if (k == 0) break;
        size_t m = rest + (size_t)k;
        if (!kopf) { if (m < 4) { rest = m; continue; } if (memcmp(b, "\x7f" "ELF", 4)) break; kopf = true; }
        if (enthaelt(b, m, kennung, kl)) { r = 1; break; }
        rest = m < kl - 1 ? m : kl - 1;   /* das Ende behalten - die Kennung kann über eine Stückgrenze gehen */
        memmove(b, b + m - rest, rest);
    }
    free(b); close(fd);
    return r;
}

int upd_eigene_elf_suchen(const char *const *ordner, int n, const char *kennung, const char *version, char *aus, size_t an)
{
    if (an) aus[0] = 0;
    char (*tr)[1024] = malloc(sizeof(char[UPD_SUCHE_MAX_TREFFER][1024]));
    if (!tr) return 0;
    int t = 0, gesamt = 0;
    for (int i = 0; i < n; i++) {
        const char *o = ordner[i];
        if (!o || o[0] != '/') continue;
        bool doppelt = false; for (int j = 0; j < i && !doppelt; j++) if (ordner[j] && !strcmp(ordner[j], o)) doppelt = true;
        if (doppelt) continue;
        DIR *d = opendir(o);
        if (!d) continue;
        size_t ol = strlen(o); bool schraeg = ol && o[ol - 1] == '/';
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            const char *nm = de->d_name;
            if (nm[0] == '.' || !endet_elf(nm) || !strcasestr(nm, "cleanup")) continue;
            char p[1024];
            if (snprintf(p, sizeof p, "%s%s%s", o, schraeg ? "" : "/", nm) >= (int)sizeof p) continue;
            if (ist_unsere(p, kennung) != 1) continue;
            gesamt++;
            if (t < UPD_SUCHE_MAX_TREFFER) snprintf(tr[t++], sizeof tr[0], "%s", p);
        }
        closedir(d);
    }
    const char *z[UPD_SUCHE_MAX_TREFFER]; for (int i = 0; i < t; i++) z[i] = tr[i];
    int w = upd_alte_waehlen(z, t, version);
    if (w >= 0 && an) snprintf(aus, an, "%s", tr[w]);
    free(tr);
    return gesamt;
}

/* ------------------------------------------------------------------ NEU 1.3: "Neu in x" aus den Release-Notes (siehe upd_suche.h) */
/* hängt b an (höchstens bis an - 1), nie mitten in einem Zeichen - gibt die neue Länge zurück */
static size_t neu_anhaengen(char *o, size_t ol, size_t on, const char *b, size_t bn)
{
    if (ol + bn >= on) return ol;   /* passt nicht mehr ganz - lieber weglassen als ein halbes Zeichen */
    memcpy(o + ol, b, bn); o[ol + bn] = 0;
    return ol + bn;
}
/* eine Zeile ohne Markdown-Zeichen, Links und Emojis, Leerraum zusammengefasst, vorne/hinten ohne Leerzeichen */
static void neu_sauber(const char *s, size_t n, char *o, size_t on)
{
    size_t ol = 0; o[0] = 0; bool leer = true;
    for (size_t i = 0; i < n && s[i]; ) {
        unsigned char c = (unsigned char)s[i];
        if (c == '*' || c == '`' || (c == '_' && i + 1 < n && s[i + 1] == '_')) { i += c == '_' ? 2 : 1; continue; }
        if (c == '[') {   /* [Text](Adresse) -> Text */
            const char *e = NULL;
            for (size_t j = i + 1; j + 1 < n && s[j] && s[j] != '\n'; j++) if (s[j] == ']' && s[j + 1] == '(') { e = s + j; break; }
            if (e) {
                const char *z = memchr(e + 2, ')', n - (size_t)(e + 2 - s));
                if (z) { neu_sauber(s + i + 1, (size_t)(e - (s + i + 1)), o + ol, on - ol); ol = strlen(o); leer = ol == 0 || o[ol - 1] == ' '; i = (size_t)(z - s) + 1; continue; }
            }
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { if (!leer) { ol = neu_anhaengen(o, ol, on, " ", 1); leer = true; } i++; continue; }
        size_t l = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
        if (l == 0 || i + l > n) { i++; continue; }   /* kein gültiger Anfang eines Zeichens */
        bool gut = true; for (size_t k = 1; k < l; k++) if (((unsigned char)s[i + k] & 0xC0) != 0x80) gut = false;
        if (!gut) { i++; continue; }
        if (l == 4) { i += 4; continue; }   /* Emojis, Flaggen */
        if (l == 3 && ((unsigned char)s[i] == 0xEF && (unsigned char)s[i + 1] == 0xB8 && (unsigned char)s[i + 2] == 0x8F)) { i += 3; continue; }   /* U+FE0F */
        if (l == 3 && ((unsigned char)s[i] == 0xE2 && (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0x8D)) { i += 3; continue; }   /* U+200D */
        size_t vor = ol; ol = neu_anhaengen(o, ol, on, s + i, l);
        if (ol == vor) break;   /* voll */
        leer = false; i += l;
    }
    while (ol && o[ol - 1] == ' ') o[--ol] = 0;
}
static const char *neu_ohne_einzug(const char *z, const char *e) { int k = 0; while (z < e && *z == ' ' && k < 3) { z++; k++; } return z; }
static bool neu_aufzaehlung(const char *z, const char *e) { return z + 1 < e && (z[0] == '-' || z[0] == '*' || z[0] == '+') && (z[1] == ' ' || z[1] == '\t'); }

int upd_neu_punkte(const char *text, int art, char (*aus)[UPD_NEU_LAENGE], int max)
{
    int n = 0;
    if (!text || !aus || max <= 0) return 0;
    if (art == UPD_NEU_EN) {
        const char *k = strstr(text, "In short:");
        if (!k) return 0;
        k += strlen("In short:");
        const char *e = strchr(k, '\n'); if (!e) e = k + strlen(k);
        char *s = malloc(4096); if (!s) return 0;
        neu_sauber(k, (size_t)(e - k), s, 4096);
        /* in Sätze teilen: ". ", "! ", "? " (auch nach einem Anführungszeichen) */
        const char *p = s;
        while (*p && n < max) {
            const char *q = p, *ende = NULL;
            for (; *q; q++) {
                if ((*q == '.' || *q == '!' || *q == '?') && (q[1] == ' ' || q[1] == 0)) { ende = q + 1; break; }
                if ((*q == '.' || *q == '!' || *q == '?') && (q[1] == '"' || q[1] == '\'') && (q[2] == ' ' || q[2] == 0)) { ende = q + 2; break; }
            }
            if (!ende) ende = p + strlen(p);
            neu_sauber(p, (size_t)(ende - p), aus[n], UPD_NEU_LAENGE);
            if (aus[n][0]) n++;
            p = ende; while (*p == ' ') p++;
        }
        free(s);
        return n;
    }
    bool drin = false;
    for (const char *z = text; *z && n < max; ) {
        const char *e = strchr(z, '\n'); if (!e) e = z + strlen(z);
        const char *a = neu_ohne_einzug(z, e);
        bool leer = true; for (const char *q = z; q < e; q++) if (*q != ' ' && *q != '\t' && *q != '\r') leer = false;
        if (neu_aufzaehlung(a, e)) {
            drin = true;
            const char *t = a + 2; while (t < e && (*t == ' ' || *t == '\t')) t++;
            const char *te = e;
            if (art == UPD_NEU_TITEL && te - t > 4 && t[0] == '*' && t[1] == '*') {   /* fette Überschrift am Anfang */
                for (const char *q = t + 2; q + 1 < te; q++) if (q[0] == '*' && q[1] == '*') { t += 2; te = q; break; }
            }
            neu_sauber(t, (size_t)(te - t), aus[n], UPD_NEU_LAENGE);
            if (art == UPD_NEU_TITEL && te != e) {   /* ":" am Ende der Überschrift weg */
                size_t l = strlen(aus[n]); while (l && (aus[n][l - 1] == ':' || aus[n][l - 1] == ' ')) aus[n][--l] = 0;
            }
            if (aus[n][0]) n++;
        } else if (drin && !leer) break;   /* die erste Aufzählung ist zu Ende (Überschrift oder Text) */
        z = *e ? e + 1 : e;
    }
    return n;
}
