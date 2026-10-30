/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * kern_suche.c - die Suche, 1:1 aus Suche.cs (1.0.5):
 * Namen lesen (param.json/param.sfo, Galerie, Trophäen), Einträge merken,
 * ShadowMount-Quellen, M.2 aufklappen, SucheFunde.
 * Muster und Texte sind wörtlich übernommen; jede Funktion trägt den Namen
 * der C#-Methode.
 */
#include "kern.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define A (k->a)
static sliste L1(arena *a, const char *x) { sliste l = {0}; sl_add(a, &l, x); return l; }
static bool sl_enthaelt(const sliste *l, const char *s) { for (int i = 0; i < l->n; i++) if (strcmp(l->e[i], s) == 0) return true; return false; }  /* List.Contains (ordinal) */
static const char *zahl(arena *a, long long n) { return a_fmt(a, "%lld", n); }

const int ALLE_KAT[KAT_ANZ] = { 1, 2, 3, 4, 5, 6 };

/* NEU 1.0.6 (da_Chris86 28.10.2026): Speicherorte - steht vorne am Eintrag, die Bereiche sind danach sortiert */
int kern_pfad_ort(const char *p)
{
    if (!p) return ORT_INTERN;
    if (!strncmp(p, M2 "/", strlen(M2) + 1) || !strcmp(p, M2)) return ORT_M2;
    if (!strncmp(p, USB_ERW "/", strlen(USB_ERW) + 1) || !strcmp(p, USB_ERW)) return ORT_USB;
    if (!strncmp(p, "/mnt/usb", 8)) return ORT_STICK;
    return ORT_INTERN;
}
const char *kern_ort_vorsatz(int ort)
{
    switch (ort) { case ORT_M2: return "M.2 – "; case ORT_USB: return "USB – "; case ORT_STICK: return T("USB-Stick – ", "USB stick – "); }
    return "";
}
const char *kern_ort_name(int ort)
{
    switch (ort) { case ORT_M2: return T("M.2-SSD", "M.2 SSD"); case ORT_USB: return T("USB-Speicher", "USB storage"); case ORT_STICK: return T("USB-Stick", "USB stick"); }
    return T("interne SSD", "internal SSD");
}

const char *kern_kat_name(int k)
{
    switch (k) {
    case 1: return T("Spiele & Apps", "Games & apps");
    case 2: return T("Spielstände", "Saved data");
    case 3: return T("Trophäen", "Trophies");
    case 4: return T("Bilder & Videos", "Screenshots & videos");
    case 5: return T("Reste", "Leftovers");
    case 6: return T("Jailbreak & Homebrew", "Jailbreak & homebrew");
    }
    return "";
}
const char *kern_kat_info(int k)
{
    switch (k) {
    case 1: return T("Spiele und Apps auf interner SSD, M.2 und USB - der Ort steht vorne. Wer ein Spiel wählt, nimmt Update und DLCs mit.", "Games and apps on the internal SSD, M.2 and USB - the location comes first. Selecting a game takes its update and DLC with it.");
    case 2: return T("Spielstände auf der Konsole. Ohne Cloud-Sicherung (PS Plus) endgültig weg.", "Saved data on the console. Without a cloud backup (PS Plus) it is gone for good.");
    case 3: return T("Trophäen-Fortschritt und Trophäen-Listen. Nie mit dem PSN abgeglichene Trophäen sind endgültig weg.", "Trophy progress and trophy lists. Trophies never synced with PSN are gone for good.");
    case 4: return T("Screenshots und Videoclips, je Spiel mit Anzahl.", "Screenshots and video clips, per game with count.");
    case 5: return T("Downloads, Zusatzinhalte und Updates ohne Spiel, Reste gelöschter Spiele, Fehlerberichte und übrige Dateien auf M.2 und USB.", "Downloads, add-ons and updates without a game, leftovers of deleted games, error reports and other files on M.2 and USB.");
    case 6: return T("Kacheln und Daten der Jailbreak-Tools, Homebrew-Daten und ELF-Dateien aller Speicher. Danach Jailbreak neu einrichten.", "Tiles and data of jailbreak tools, homebrew data and ELF files on all storage. Set up your jailbreak again afterwards.");
    }
    return "";
}

/* ---------------------------------------------------------------
 * Namen
 * --------------------------------------------------------------- */
/* HoleCI: erster Schlüssel (in der Reihenfolge des Wörterbuchs) ohne Groß/klein.
 * JavaScriptSerializer legt doppelte Schlüssel an der Stelle des ersten Auftretens
 * mit dem Wert des letzten Auftretens ab - genau so wird hier gesucht. */
static cJSON *hole_ci(cJSON *o, const char *schluessel)
{
    if (!o || !cJSON_IsObject(o) || !schluessel) return NULL;
    for (cJSON *c = o->child; c; c = c->next) {
        if (!txt_gleich_ci(c->string, schluessel)) continue;
        cJSON *letzter = c;
        for (cJSON *d = c->next; d; d = d->next) if (strcmp(d->string, c->string) == 0) letzter = d;
        return letzter;
    }
    return NULL;
}
/* Str(o) = Convert.ToString(o, InvariantCulture) für die Werte von JavaScriptSerializer */
static const char *str_json(arena *a, cJSON *o)
{
    if (!o || cJSON_IsNull(o)) return "";
    if (cJSON_IsString(o)) return a_kopie(a, o->valuestring);
    if (cJSON_IsBool(o)) return cJSON_IsTrue(o) ? "True" : "False";
    if (cJSON_IsNumber(o)) {
        double v = o->valuedouble;
        if (v == (double)(long long)v && v > -9.2e18 && v < 9.2e18) return a_fmt(a, "%lld", (long long)v);
        char b[64]; snprintf(b, sizeof b, "%.15g", v); return a_kopie(a, b);
    }
    if (cJSON_IsObject(o)) return "System.Collections.Generic.Dictionary`2[System.String,System.Object]";
    if (cJSON_IsArray(o)) return "System.Object[]";
    return "";
}

/* JsonTitel: Titel aus einer param.json (PS5): in der gewählten Sprache, sonst die andere, sonst Standard-Name. */
const char *kern_json_titel(arena *a, const char *t0)
{
    const char *t = h_ohne_bom(t0 ? t0 : "");
    if (!h_m(t, "titleName")) return "";
    /* wie JavaScriptSerializer: der ganze Text muss JSON sein (nichts dahinter) */
    cJSON *o = cJSON_ParseWithOpts(t, NULL, true);
    if (o) {
        cJSON *lp = hole_ci(o, "localizedParameters");
        if (lp) {
            const char *reihe[4];
            if (SP_EN) { reihe[0] = "en-US"; reihe[1] = "en-GB"; reihe[2] = "de-DE"; }
            else { reihe[0] = "de-DE"; reihe[1] = "en-US"; reihe[2] = "en-GB"; }
            reihe[3] = str_json(a, hole_ci(lp, "defaultLanguage"));
            for (int i = 0; i < 4; i++) {
                const char *s = reihe[i];
                if (s[0] == 0) continue;
                cJSON *sp = hole_ci(lp, s);
                if (sp && str_json(a, hole_ci(sp, "titleName"))[0]) { const char *r = str_json(a, hole_ci(sp, "titleName")); cJSON_Delete(o); return r; }
            }
        }
        cJSON_Delete(o);
    }
    htreffer m = h_mt(a, t, "\"titleName\"\\s*:\\s*\"([^\"]*)\"");
    if (m.ok) return m.gr[1];
    return "";
}

/* param.sfo (PS4-Format) lesen */
typedef struct { bool zahl; uint32_t u; const char *s; } sfowert;
static uint32_t u32(const unsigned char *b) { return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24); }
static uint16_t u16(const unsigned char *b) { return (uint16_t)(b[0] | (b[1] << 8)); }
mtab kern_sfo_lesen(arena *a, const unsigned char *b, size_t n)
{
    mtab h = mt_neu(true);
    if (b == NULL || n < 20 || b[0] != 0 || b[1] != 0x50 || b[2] != 0x53 || b[3] != 0x46) return h;
    /* in C# fängt try/catch jeden Lesefehler ab - hier: bei jedem Zugriff außerhalb aufhören */
    uint32_t keyTab = u32(b + 8), datTab = u32(b + 12), anz = u32(b + 16);
    for (uint32_t i = 0; i < anz && i < 200; i++) {
        size_t e = 20 + 16 * (size_t)i;
        if (e + 16 > n) break;
        uint16_t kOff = u16(b + e), fmt = u16(b + e + 2);
        uint32_t len = u32(b + e + 4), dOff = u32(b + e + 12);
        size_t ks = (size_t)keyTab + kOff;
        if (ks > n) break;                                   /* Array.IndexOf: Start außerhalb -> Ausnahme */
        size_t ke = ks; while (ke < n && b[ke] != 0) ke++;
        if (ke >= n) continue;                               /* kein Nullbyte -> ke < 0 -> continue */
        char *key = a_kopie_n(a, (const char *)b + ks, ke - ks);
        for (char *c = key; *c; c++) if ((unsigned char)*c > 0x7F) *c = '?';   /* Encoding.ASCII */
        size_t ds = (size_t)datTab + dOff;
        sfowert *w = a_hole(a, sizeof *w);
        if (fmt == 0x0404 && len >= 4) {
            if (ds + 4 > n) break;
            w->zahl = true; w->u = u32(b + ds);
        } else {
            if (ds > n || (size_t)len > n - ds) break;
            /* GetString(b, ds, len).TrimEnd('\0'): Nullbytes am Ende weg, Nullbytes mittendrin
             * bleiben in .NET erhalten - hier als \x01 (siehe s_text) */
            size_t l = len; while (l > 0 && b[ds + l - 1] == 0) l--;
            char *k = a_kopie_n(a, (const char *)b + ds, l);
            for (size_t i = 0; i < l; i++) if (k[i] == 0) k[i] = 1;
            w->s = h_net_text(a, k);
        }
        mt_setze(a, &h, key, w);
    }
    return h;
}
static const char *str_sfo(arena *a, const sfowert *w) { if (!w) return ""; return w->zahl ? a_fmt(a, "%u", w->u) : w->s; }
const char *kern_sfo_titel(arena *a, const mtab *h)
{
    const char *k[3] = { "TITLE_04", "TITLE_01", "TITLE" };
    for (int i = 0; i < 3; i++) if (mt_hat(h, k[i]) && str_sfo(a, mt_hole(h, k[i]))[0]) return str_sfo(a, mt_hole(h, k[i]));
    return "";
}
static mtab sfo_von(kern *k, const char *pfad)
{
    const unsigned char *d; size_t n;
    if (!s_bytes(k->S, pfad, &d, &n)) return kern_sfo_lesen(A, NULL, 0);
    return kern_sfo_lesen(A, d, n);
}

typedef struct { const char *T; const char *Roh; } titelinfo;
static const char *app_titel_lesen(kern *k, const char *id)
{
    k->APP_ROH = "";
    const char *jd[3] = { VERB(A, "/user/appmeta/", id, "/param.json"), VERB(A, "/user/app/", id, "/sce_sys/param.json"), VERB(A, SYS_META "/", id, "/param.json") };
    for (int i = 0; i < 3; i++) {
        const char *t = h_ohne_bom(s_text(k->S, jd[i]));
        if (!h_m(t, "titleName")) continue;
        k->APP_ROH = t;
        const char *n = kern_json_titel(A, t);
        if (n[0]) return n;
    }
    /* PS4-Spiele haben statt param.json eine param.sfo */
    const char *sd[3] = { VERB(A, "/user/appmeta/", id, "/param.sfo"), VERB(A, "/user/app/", id, "/sce_sys/param.sfo"), VERB(A, SYS_META "/", id, "/param.sfo") };
    for (int i = 0; i < 3; i++) {
        mtab h = sfo_von(k, sd[i]);
        const char *n = kern_sfo_titel(A, &h);
        if (n[0]) { k->APP_ROH = n; return n; }
    }
    return "";
}
static const char *app_titel(kern *k, const char *id)            /* AppTitel */
{
    titelinfo *c = mt_hole(&k->TITEL_CACHE, id);
    if (c) { k->APP_ROH = c->Roh; return c->T; }
    const char *t = app_titel_lesen(k, id);
    c = a_hole(A, sizeof *c); c->T = t; c->Roh = k->APP_ROH;
    mt_setze(A, &k->TITEL_CACHE, id, c);
    return t;
}
/* MitName: hängt bei Ordnern, die nach einer Spiel-ID benannt sind, den Namen an */
static const char *mit_name(kern *k, const char *x)
{
    htreffer m = h_mt(A, x, "^([A-Z]{4}\\d{5})");
    if (!m.ok) return x;
    const char *id = m.gr[1];
    if (!mt_hat(&k->NAMEN_CACHE, id)) mt_setze(A, &k->NAMEN_CACHE, id, (void *)app_titel(k, id));
    const char *t = mt_hole(&k->NAMEN_CACHE, id);
    if (!t || !t[0]) return x;
    return VERB(A, x, " (", t, ")");
}
static const char *trophaen_name(kern *k, const char *npwr)
{
    const char *n = mt_hole(&k->NPWR_NAMEN, npwr);
    if (mt_hat(&k->NPWR_NAMEN, npwr) && n && n[0]) return VERB(A, n, " (", npwr, ")");
    if (mt_hat(&k->NPWR_GALERIE, npwr)) return VERB(A, (const char *)mt_hole(&k->NPWR_GALERIE, npwr), " (", npwr, ")");
    return npwr;
}
static const char *trophaen_weg(kern *k, const char *npwr) { return mt_hat(&k->NPWR_NAMEN, npwr) ? "" : T(" - Spiel nicht mehr auf der Konsole", " - game no longer on the console"); }

#define MEDIEN "\\.(jxr|jpe?g|png|mp4|webm|mov)$"
static int zaehle_medien(kern *k, const char *p)
{
    int n = 0;
    dliste *l = s_liste(k->S, p, false);
    if (!l) return 0;
    for (int i = 0; i < l->n; i++) {
        datei *e = &l->e[i];
        if (e->ist_ordner) {
            dliste *u = s_liste(k->S, VERB(A, p, "/", e->name), false);
            if (u) for (int j = 0; j < u->n; j++) if (!u->e[j].ist_ordner && h_m(u->e[j].name, MEDIEN)) n++;
        }
        else if (h_m(e->name, MEDIEN)) n++;
    }
    return n;
}
/* string.Length in .NET = UTF-16-Einheiten */
static int net_laenge(const char *s)
{
    int n = 0; const unsigned char *p = (const unsigned char *)s;
    while (*p) { uint32_t c = utf8_naechstes(&p); n += c >= 0x10000 ? 2 : 1; }
    return n;
}
/* GalerieName: erst die Kachel-Infos, sonst die Begleitdatei (.dat) einer Aufnahme */
static const char *galerie_name(kern *k, const char *id, const sliste *ordner)
{
    if (h_like(id, "NPXS*")) return VERB(A, T("Systemmenü (", "System menu ("), id, ")");
    const char *m = mit_name(k, id);
    if (strcmp(m, id) != 0) return VERB(A, h_r(A, m, "^[A-Z]{4}\\d{5} \\((.*)\\)$", "$1"), " (", id, ")");
    if (!mt_hat(&k->GALERIE_NAMEN, id)) {
        mt_setze(A, &k->GALERIE_NAMEN, id, (void *)"");
        bool fertig = false;
        for (int oi = 0; ordner && oi < ordner->n && !fertig; oi++) {
            const char *o = ordner->e[oi];
            dliste *l1 = s_liste(k->S, o, false); if (!l1) continue;
            for (int i = 0; i < l1->n && !fertig; i++) {
                datei *e = &l1->e[i];
                if (!e->ist_ordner) continue;
                dliste *l2 = s_liste(k->S, VERB(A, o, "/", e->name), false); if (!l2) continue;
                for (int j = 0; j < l2->n; j++) {
                    datei *d = &l2->e[j];
                    if (d->ist_ordner || !h_like(d->name, "*.dat")) continue;
                    const unsigned char *b; size_t bn;
                    if (!s_bytes(k->S, VERB(A, o, "/", e->name, "/", d->name), &b, &bn) || bn < 8) continue;
                    /* nach dem 8-Byte-Kopf: erster Klartext bis zum nächsten Nullbyte = Spielname */
                    size_t start = 8; while (start < bn && b[start] < 32) start++;
                    if (start >= bn) continue;
                    size_t ende = start; while (ende < bn && b[ende] != 0) ende++;
                    const char *roh = h_net_text(A, a_kopie_n(A, (const char *)b + start, ende - start));
                    const char *t = h_trim(A, h_r(A, roh, "_\\d{14}.*$", ""));
                    int tl = net_laenge(t);
                    if (tl < 2 || tl > 100) continue;
                    mt_setze(A, &k->GALERIE_NAMEN, id, (void *)t);
                    /* Begleitdatei .ext nennt die Trophäen-Kennung -> Name auch für Bereich 3 */
                    const char *ext = s_text(k->S, VERB(A, o, "/", e->name, "/", h_r(A, d->name, "\\.dat$", ".ext")));
                    htreffer nm = h_mt(A, ext, "NPWR\\d{5}_\\d{2}");
                    if (nm.ok) mt_setze(A, &k->NPWR_GALERIE, nm.wert, (void *)t);
                    fertig = true;
                    break;
                }
            }
        }
    }
    const char *g = mt_hole(&k->GALERIE_NAMEN, id);
    if (g && g[0]) return VERB(A, g, " (", id, T(", Spiel nicht mehr auf der Konsole)", ", game no longer on the console)"));
    return VERB(A, id, T(" (Spiel nicht mehr auf der Konsole)", " (game no longer on the console)"));
}
/* da_Chris86 28.10.2026: Astro's Playroom ist ein Spiel wie jedes andere - wird nicht mehr erkannt/geschützt */
static bool ist_astro(kern *k, const char *t) { (void)k; (void)t; return false; }
static void astro_merken(kern *k, const char *id) { if (!h_hat(&k->ASTRO_IDS, id)) sl_add(A, &k->ASTRO_IDS, id); }

/* ---------------------------------------------------------------
 * Einträge merken
 * --------------------------------------------------------------- */
const char *kern_gesperrt_text(void) { return T("  [GESPERRT: Game Compressor arbeitet gerade daran]", "  [LOCKED: Game Compressor is working on it]"); }
/* NEU 1.1 (Sicherheitsprüfung Punkt 7) */
const char *kern_mehrfach_text(void) { return T("  [GESPERRT: gehört zu mehreren Spielen - nicht eindeutig]", "  [LOCKED: belongs to several games - not unique]"); }
const char *kern_mehrfach_grund(void) { return T("Gesperrt - mehrere Kacheln zeigen auf diese Spieldatei. Welches Spiel gemeint ist, ist nicht eindeutig.", "Locked - several tiles point to this game file. It is not clear which game it belongs to."); }
/* NEU 1.0.6: bekannte Jailbreak-Einstiege. da_Chris86 29.10.2026: NUR die Einstiege, mit denen der Jailbreak gestartet wird
 * (keine Jailbreak-Tools wie etaHEN, Payload Manager, Autoloader-Ordner, ELF-Dateien - die bleiben wie bisher):
 *   YouTube (Y2JB: PPSA01650/1/2), Netflix ('N Hack: PPSA01614/5/6), Lua-Jailbreak (remote_lua_loader: 18 Artemis-Spiele),
 *   PS2-Spielstand-Jailbreak (mast1c0re/Luac0re: Okage CUSA02199, Star Wars Racer Revenge CUSA03474/CUSA03492) - jeweils mit
 *   Spielstand; dazu der Browserspeicher (offline gespeicherter WebKit-Jailbreak). BD-JB läuft von der Disc - nichts auf der Konsole. */
#define JB_Y2JB "PPSA0165[0-2]"
#define JB_NETFLIX "PPSA0161[4-6]"
#define JB_LUA "CUSA(16074|17068|27389|27390|16229|19556|17112|17126|13303|13586|25179|25180|29745|29746|14324|11977|11481|17122|19555|09825)"
#define JB_PS2 "CUSA(02199|02282|03474|03492|07842|07992)"
/* NEU 1.0.6 (Recherche 29.10.2026 19:4x, da_Chris86: "alle Jailbreak-Varianten"): Ren'Py-Jailbreak yarpe (A Year of Springs,
 * Arcade Spirits) - Spiel + Spielstand; WebKit-Autoloader-Kacheln (itsPLK WKAL00001, Goldengames GGAU00001) - Kachel + Browserspeicher */
#define JB_YARPE "CUSA(30428|30429|30430|30431|32096|32097)|PPSA(06409|06410)"
#define JB_WEBKIT "WKAL00001|WKAG00001|GGAU00001"   /* 1.0.6 Fix (Recherche 30.10.2026): + WKAG00001 = Yahballon JB_AUTOLOADER, Kachel "JAILBREAK ME" */
#define JB_IDS JB_Y2JB "|" JB_NETFLIX "|" JB_LUA "|" JB_PS2 "|" JB_YARPE "|" JB_WEBKIT
#define JB_MUSTER "^(/user/(app|appmeta|appmeta/addcont|appmeta/external/addcont|patch|addcont|download)|/system_data/priv/appmeta(/external)?|/mnt/ext[01]/user/(app|patch|addcont)|/user/home/[^/]+/savedata(_prospero)?)/(" JB_IDS ")(/|$)"
/* NEU 1.0.6 (Recherche 29.10.2026): der Offline-Jailbreak liegt im AppCache (appcache) UND in localStorage (local) - beide nötig */
#define JB_BROWSER "^/user/home/[^/]+/webkit/shell/(local|appcache)$"
/* Codex-Prüfung 36: Spiel-Abbild eines Einstiegs (z. B. "Raspberry Cube - CUSA16074.exfat"), auch als eigener Eintrag ohne Kachel */
#define JB_KENNUNG(x) "(^|[^A-Z0-9])(" x ")([^0-9]|$)"
#define JB_ABBILD "^(?=.*" JB_KENNUNG(JB_IDS) ").*\\.(?i:exfat|ffpfs|ffpfsc|ffpkg|pfs|img)$"
static bool jb_pfad(const char *p) { return h_m(p, JB_MUSTER) || h_m(p, JB_ABBILD); }
bool kern_jb_abbild(const char *p) { return h_m(p, JB_ABBILD); }   /* Codex-Prüfung 37: für den Nachkommenschutz beim Löschen */
static bool ist_abbild(const char *p) { return h_m(p, "(?i)\\.(exfat|ffpfs|ffpfsc|ffpkg|pfs|img)$"); }
bool kern_jb_einstieg(const char *id) { return h_m(id, "^(" JB_IDS ")$"); }
const char *kern_jb_grund(void) { return T("Möglicher Jailbreak-Einstieg.", "Possible jailbreak entry."); }
/* NEU 1.0.6: zu welchem Jailbreak ein Pfad gehört - NULL = zu keinem */
const char *kern_jb_weg(const char *p)
{
    if (!p) return NULL;
    if (h_m(p, JB_BROWSER)) return T("WebKit-Jailbreak (Browser)", "WebKit jailbreak (browser)");
    if (h_m(p, JB_KENNUNG(JB_Y2JB))) return "Y2JB (YouTube)";
    if (h_m(p, JB_KENNUNG(JB_NETFLIX))) return T("Netflix-Jailbreak", "Netflix jailbreak");
    if (h_m(p, JB_KENNUNG(JB_LUA))) return T("Lua-Jailbreak", "Lua jailbreak");
    if (h_m(p, JB_KENNUNG(JB_PS2))) return T("PS2-Spielstand-Jailbreak", "PS2 save jailbreak");
    if (h_m(p, JB_KENNUNG(JB_YARPE))) return T("Ren'Py-Jailbreak (yarpe)", "Ren'Py jailbreak (yarpe)");
    if (h_m(p, JB_KENNUNG(JB_WEBKIT))) return "WebKit-Autoloader";
    return NULL;
}
const char *kern_jb_weg_eintrag(const eintrag *f)
{
    for (int j = 0; j < f->PfadeBasis.n; j++) { const char *w = kern_jb_weg(f->PfadeBasis.e[j]); if (w) return w; }
    for (int j = 0; j < f->Pfade.n; j++) { const char *w = kern_jb_weg(f->Pfade.e[j]); if (w) return w; }
    { const char *w = kern_jb_weg(f->TextBasis ? f->TextBasis : f->Text); if (w) return w; }   /* Codex-Prüfung 37: Spiel-Abbild, Kennung nur im Namen */
    return T("Jailbreak", "jailbreak");
}
/* NEU 1.1 (Anforderungen 27.10.2026): Jailbreak-Einstiege sind einzeln löschbar, die vollständige Bereinigung fragt nach.
 * NEU 1.0.6 (da_Chris86 29.10.2026): mit dem Namen des Jailbreaks */
const char *kern_jb_hinweis(arena *a, const eintrag *f)
{
    return VERB(a, T("Gehört zum Jailbreak: ", "Belongs to the jailbreak: "), kern_jb_weg_eintrag(f),
                T(". Nur einzeln wählbar. Wird es gelöscht, funktioniert dieser Jailbreak erst wieder, wenn er neu eingerichtet ist. Die vollständige Bereinigung fragt, ob es bleibt.",
                  ". Can only be selected individually. If it is deleted, this jailbreak only works again once it has been set up again. The full cleanup asks whether it stays."));
}
/* NEU 1.0.6 (da_Chris86 29.10.2026 07:28): Warnung beim Anwählen */
const char *kern_jb_warnung(arena *a, const eintrag *f)
{
    return VERB(a, T("ACHTUNG: Gehört zum Jailbreak „", "WARNING: belongs to the jailbreak \""), kern_jb_weg_eintrag(f),
                T("“ - wird es gelöscht, geht dieser Jailbreak nicht mehr.", "\" - if it is deleted, this jailbreak stops working."));
}
/* NEU 1.0.6 (da_Chris86 28.10.2026 23:16, erweitert 29.10.2026): Anzeige in der Konsole-Karte. Als eingerichtet gilt ein
 * Einstieg nur mit seinen Jailbreak-Daten: YouTube/Netflix mit /user/download/<Kennung> (bei Y2JB die download0.dat),
 * Lua- und PS2-Spielstand-Jailbreak mit dem Spielstand des Spiels. Der Browserspeicher kann einen offline gespeicherten
 * WebKit-Jailbreak enthalten - das ist nicht sicher erkennbar, deshalb nur ein Hinweis. Ein Jailbreak über eine Disc
 * hinterlässt nichts. Rückgabe: Namen der eingerichteten Einstiege (NULL = keiner); *warnung: was man dafür nicht löschen darf. */
const char *kern_jailbreak_erkannt(kern *k, arena *a, const char **warnung)
{
    *warnung = NULL;
    sliste ids = {0}, namen = {0};
    bool browser = false;
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        if (!f->Jailbreak) continue;
        for (int j = 0; j < f->PfadeBasis.n; j++) {
            const char *p = f->PfadeBasis.e[j];
            if (h_m(p, JB_BROWSER)) { browser = true; continue; }
            htreffer t = h_mt(a, p, "/(" JB_IDS ")(/|$)");
            if (!t.ok) continue;
            bool daten = (h_m(t.gr[1], "^(" JB_Y2JB "|" JB_NETFLIX ")$") && txt_beginnt(p, "/user/download/"))
                      || (h_m(t.gr[1], "^(" JB_LUA "|" JB_PS2 "|" JB_YARPE ")$") && h_m(p, "^/user/home/[^/]+/savedata(_prospero)?/"))
                      || (h_m(t.gr[1], "^(" JB_WEBKIT ")$") && txt_beginnt(p, "/user/app/"));   /* NEU 1.0.6: Autoloader-Kachel */
            if (!daten) continue;
            if (!sl_enthaelt(&ids, t.gr[1])) sl_add(a, &ids, t.gr[1]);
            const char *nm = kern_jb_weg(p);
            if (nm && !sl_enthaelt(&namen, nm)) sl_add(a, &namen, nm);
        }
    }
    /* betroffene Einträge je Bereich: „Name (Kennung)“ (unter „Bereich“) */
    sliste gruppen = {0};
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        int kk = ALLE_KAT[ki];
        sliste eintraege = {0};
        for (int i = 0; i < k->Funde.n; i++) {
            eintrag *f = k->Funde.e[i];
            if (!f->Jailbreak || f->Kat != kk) continue;
            bool dabei = false;
            for (int j = 0; j < f->PfadeBasis.n && !dabei; j++) for (int x = 0; x < ids.n && !dabei; x++) if (h_m(f->PfadeBasis.e[j], VERB(a, "(^|[^A-Z0-9])", ids.e[x], "([^0-9]|$)"))) dabei = true;
            if (!dabei) continue;
            const char *t = f->TextBasis ? f->TextBasis : f->Text, *z = strstr(t, "  [");
            if (z) t = a_kopie_n(a, t, (size_t)(z - t));
            htreffer m = h_mt(a, t, "^([^()]*\\([A-Z]{4}\\d{5}\\))");      /* nur "Name (Kennung)" */
            const char *e = VERB(a, "„", m.ok ? m.gr[1] : t, "“");
            if (!sl_enthaelt(&eintraege, e)) sl_add(a, &eintraege, e);
        }
        if (eintraege.n > 0) sl_add(a, &gruppen, VERB(a, sl_join(a, &eintraege, ", "), T(" (unter „", " (under \""), kern_kat_name(kk), T("“)", "\")")));
    }
    const char *w = gruppen.n > 0 ? VERB(a, T("Nicht löschen, wenn du ihn behalten willst: ", "Do not delete if you want to keep it: "), sl_join(a, &gruppen, "; "), ".") : NULL;
    if (browser) {
        const char *bt = T("Ein offline gespeicherter WebKit-Jailbreak kann im „Browserspeicher“ liegen (unter „Jailbreak & Homebrew“).",
                           "An offline-saved WebKit jailbreak may be in the \"Browser storage\" (under \"Jailbreak & homebrew\").");
        w = w ? VERB(a, w, " ", bt) : bt;
    }
    if (k->BD_DISC) sl_add(a, &namen, T("Blu-ray-Jailbreak (Disc im Laufwerk)", "Blu-ray jailbreak (disc in the drive)"));   /* NEU 1.0.6 */
    *warnung = w;
    return namen.n > 0 ? sl_join(a, &namen, ", ") : NULL;
}
const char *kern_nur_einzeln_text(void) { return T("  [Jailbreak-Einstieg - nur einzeln]", "  [jailbreak entry - individually only]"); }
/* NEU 1.1 (Anforderungen 27.10.2026): der App-Datenspeicher /user/download/<ID> geht mit der App - nur wenn es ihn gibt,
 * die App keine Sony-App ist und keine SICHTBARE Kopie auf der USB-Erweiterung liegt. */
const char *kern_app_daten(kern *k, const char *id)
{
    if (id == NULL || id[0] == 0 || h_like(id, "NPXS*")) return NULL;
    const char *p = VERB(A, "/user/download/", id);
    if (!s_existiert(k->S, p)) return NULL;
    if (k->USB_SPEICHER) {
        volatile bool usb = true;
        VERSUCHE { sliste u = s_namen(k->S, USB_ERW "/user/app", true); usb = h_hat(&u, id); }
        FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); usb = false; } ENDE_VERSUCHE;   /* da_Chris86 27.10.2026 21:39: nur eine SICHTBARE Kopie zählt (Codex-Prüfung 22) */
        if (usb) return NULL;
    }
    return p;
}
static const char *selbst_grund(void) { return T("Das ist PS5 Cleanup selbst - es wird ganz zum Schluss entfernt (vollständige Bereinigung: immer, Auswahl: auf Nachfrage).", "This is PS5 Cleanup itself - it is removed at the very end (full cleanup: always, selection: on request)."); }

typedef struct { const char *quelle; bool gesperrt, kachel, smTipp; const char *sperrtext, *sperrgrund; } merke_opt;
static void merke_o(kern *k, const sliste *pfade, int kat, const char *text, merke_opt o)
{
    /* Astro bleibt immer - seine Pfade werden gar nicht erst angeboten */
    sliste p = {0};
    /* 1.0.6 Fix (da_Chris86 30.10.2026: "keinen Hinweis"): private, nicht lesbare Ordner (z. B. Playden) erscheinen gar nicht -
     * sie bleiben geschützt und unangetastet */
    if (k->SCHUTZ.unlesbar.n > 0) {
        sliste ohne = {0};
        for (int i = 0; i < pfade->n; i++) if (!kern_selbstschutz_unlesbar(k, pfade->e[i])) sl_add(A, &ohne, pfade->e[i]);
        if (ohne.n == 0) { k->MERKE_OK = false; return; }
        if (ohne.n != pfade->n) { sliste *np = a_hole(A, sizeof *np); *np = ohne; pfade = np; }
    }
    /* NEU 1.1, Selbstschutz: Ein Eintrag, der irgendetwas betrifft, das PS5 Cleanup oder der
     * Homebrew Launcher braucht (ein Pfad oder die Spieldatei), ist als Ganzes gesperrt - auch nicht
     * teilweise löschbar, damit nie ein Eintrag mehr verspricht, als gelöscht werden darf.
     * Anforderungen 27.10.2026: sichtbar gesperrt mit Grund statt versteckt. */
    if (k->SCHUTZ.aktiv) {
        bool betroffen = o.quelle && o.quelle[0] && !kern_selbstschutz_anbieten(k, o.quelle);
        for (int i = 0; i < pfade->n && !betroffen; i++) if (!kern_selbstschutz_anbieten(k, pfade->e[i])) betroffen = true;
        bool nur_eigen = betroffen;              /* NEU 28.10.2026: gehört alles zu PS5 Cleanup selbst -> "wird zum Schluss entfernt" */
        /* 1.0.6 Fix (PS5-Test 30.10.2026 15:50): eine geschützte Spieldatei, die NICHT zu PS5 Cleanup gehört, ist nie "PS5 Cleanup" */
        if (nur_eigen && o.quelle && o.quelle[0] && !kern_selbstschutz_anbieten(k, o.quelle) && !kern_selbst_eigen(k, o.quelle)) nur_eigen = false;
        for (int i = 0; i < pfade->n && nur_eigen; i++) if (!kern_selbstschutz_anbieten(k, pfade->e[i]) && !kern_selbst_eigen(k, pfade->e[i])) nur_eigen = false;
        /* NEU 1.0.6: eine Kopie auf einem USB-Stick wird nie automatisch entfernt - eigener Text */
        bool stick = false;
        for (int i = 0; i < pfade->n && nur_eigen && !stick; i++) if (kern_pfad_ort(pfade->e[i]) == ORT_STICK) stick = true;
        if (betroffen && !o.gesperrt && nur_eigen && stick) {
            o.gesperrt = true;
            o.sperrtext = T("  [PS5 Cleanup - Kopie auf dem USB-Stick, bleibt]", "  [PS5 Cleanup - copy on the USB stick, stays]");
            o.sperrgrund = T("Das ist eine Kopie von PS5 Cleanup auf einem USB-Stick. Sie wird nie automatisch entfernt - bei Bedarf am PC löschen.", "This is a copy of PS5 Cleanup on a USB stick. It is never removed automatically - delete it on a PC if needed.");
        }
        if (betroffen && !o.gesperrt) {
            o.gesperrt = true;
            o.sperrtext = nur_eigen ? T("  [PS5 Cleanup - wird zum Schluss entfernt]", "  [PS5 Cleanup - removed at the end]") : T("  [GESPERRT: nicht sicher prüfbar]", "  [LOCKED: cannot be checked safely]");
            o.sperrgrund = nur_eigen ? selbst_grund() : T("Gesperrt - ließ sich nicht sicher prüfen (Lesefehler oder Ort von PS5 Cleanup unklar).", "Locked - could not be checked safely (read error or location of PS5 Cleanup unclear).");
        }
    }
    for (int i = 0; i < pfade->n; i++) if (!kern_astro_pfad(k, pfade->e[i])) sl_add(A, &p, pfade->e[i]);
    k->MERKE_OK = p.n > 0;
    if (!k->MERKE_OK) return;
    /* NEU 1.0.6: Ort = Ort der Spieldatei (ShadowMount) bzw. des ersten Pfads außerhalb der internen SSD */
    int ort = ORT_INTERN;
    if (o.quelle && o.quelle[0]) ort = kern_pfad_ort(o.quelle);
    else for (int i = 0; i < p.n && ort == ORT_INTERN; i++) ort = kern_pfad_ort(p.e[i]);
    text = VERB(A, kern_ort_vorsatz(ort), text);
    if (o.gesperrt) text = VERB(A, text, o.sperrtext ? o.sperrtext : kern_gesperrt_text());
    eintrag *f = a_hole(A, sizeof *f);
    f->Pfade = p; f->PfadeBasis = sl_kopie(A, &p); f->Kat = kat; f->Text = text; f->TextBasis = text;
    f->Quelle = o.quelle ? o.quelle : ""; f->Gesperrt = o.gesperrt; f->Kachel = o.kachel; f->SmTipp = o.smTipp;
    f->SperrGrund = o.gesperrt ? o.sperrgrund : NULL;
    f->Ort = ort;
    /* NEU 1.0.6: USB-Stick nur einzeln, nie in der vollständigen Bereinigung. da_Chris86 30.10.2026 06:58: ausgenommen die interne
     * Kachel eines ShadowMount-Spiels, dessen Spieldatei auf dem Stick liegt - sie geht über "ganzen Bereich" und die vollständige
     * Bereinigung mit ("nur Kachel, Spiel bleibt"); die Spieldatei auf dem Stick nur, wenn ausdrücklich "Spiel + Kachel" gewählt wird. */
    if (k->MERKE_STICK || (ort == ORT_STICK && !(o.quelle && o.quelle[0]))) f->NurEinzeln = true;
    pl_add(A, &k->bau, f);
}
#define MERKE(k, pf, kat, text, ...) merke_o((k), (pf), (kat), (text), (merke_opt){ __VA_ARGS__ })
/* NEU 1.0.6 (da_Chris86 28.10.2026): der zuletzt gemerkte Eintrag hat eine Kachel im PS5-Menü -> beim Löschen über Sonys
 * Deinstallation (dann bleibt keine Geisterkachel). Nur für die einzige Kopie - bei einer zweiten Kopie (M.2/USB) nie. */
static void kachel_merken(kern *k, const char *id, const char *ordner)
{
    if (!k->MERKE_OK || k->bau.n == 0) return;
    eintrag *f = k->bau.e[k->bau.n - 1];
    f->KachelId = id; f->KachelOrdner = ordner;
}
/* NEU 1.1: zuletzt gemerkten Eintrag sperren, wenn er zu einem Spiel auf einer gerade nicht erkannten M.2 gehören könnte */
const char *kern_m2_unklar_grund(void) { return T("Gesperrt - die M.2-SSD wird gerade nicht erkannt. Dieser Eintrag kann zu einem Spiel auf der M.2 gehören. Erst die PS5 neu starten und neu nachschauen.", "Locked - the M.2 SSD is not detected right now. This entry may belong to a game on the M.2. Restart the PS5 first and scan again."); }
static void merke_teil(kern *k, const sliste *pfade, int kat, const char *text, const char *gruppe, const char *teil)
{
    MERKE(k, pfade, kat, text, 0);
    if (!k->MERKE_OK) return;
    eintrag *f = k->bau.e[k->bau.n - 1];
    f->Gruppe = gruppe; f->Teil = teil;
}

/* Version eines Updates: PS5 aus param.json (contentVersion), PS4 aus param.sfo (APP_VER) */
static const char *patch_version(kern *k, const char *id, const char *basis)
{
    htreffer m = h_mt(A, s_text(k->S, VERB(A, basis, "/patch/", id, "/sce_sys/param.json")), "\"contentVersion\"\\s*:\\s*\"([^\"]+)\"");
    if (m.ok) return VERB(A, " (Version ", m.gr[1], ")");
    mtab h = sfo_von(k, VERB(A, basis, "/patch/", id, "/sce_sys/param.sfo"));
    if (mt_hat(&h, "APP_VER") && str_sfo(A, mt_hole(&h, "APP_VER"))[0]) return VERB(A, " (Version ", str_sfo(A, mt_hole(&h, "APP_VER")), ")");
    m = h_mt(A, s_text(k->S, VERB(A, SYS_META "/", id, "/param.json")), "\"contentVersion\"\\s*:\\s*\"([^\"]+)\"");
    if (m.ok) return VERB(A, " (Version ", m.gr[1], ")");
    const char *d2[2] = { VERB(A, SYS_META "/", id, "/param.sfo"), VERB(A, "/user/appmeta/", id, "/param.sfo") };
    for (int i = 0; i < 2; i++) {
        h = sfo_von(k, d2[i]);
        if (mt_hat(&h, "APP_VER") && str_sfo(A, mt_hole(&h, "APP_VER"))[0]) return VERB(A, " (Version ", str_sfo(A, mt_hole(&h, "APP_VER")), ")");
    }
    return "";
}
static const char *dlc_name(kern *k, const char *id, const char *dlc, const char *basis)
{
    const char *j[2] = { VERB(A, "/user/appmeta/addcont/", id, "/", dlc, "/param.json"), VERB(A, basis, "/addcont/", id, "/", dlc, "/sce_sys/param.json") };
    for (int i = 0; i < 2; i++) { const char *n = kern_json_titel(A, s_text(k->S, j[i])); if (n[0]) return n; }
    const char *s[2] = { VERB(A, "/user/appmeta/addcont/", id, "/", dlc, "/param.sfo"), VERB(A, basis, "/addcont/", id, "/", dlc, "/sce_sys/param.sfo") };
    for (int i = 0; i < 2; i++) { mtab h = sfo_von(k, s[i]); const char *n = kern_sfo_titel(A, &h); if (n[0]) return n; }
    return dlc;
}
/* Spiel, Update und Zusatzinhalte eines normal installierten Spiels als eigene Einträge */
static void merke_spielteile(kern *k, const sliste *spielPfade0, int kat, const char *name, const char *id, const char *basis,
                             bool hatUpdate, bool hatDlc, const sliste *dlcInfos, mtab *benutzt, bool zwilling)
{
    sliste spielPfade = sl_kopie(A, spielPfade0);
    if (h_hat(&k->SHARES, id) && !zwilling) sl_add(A, &spielPfade, VERB(A, "/user/system/share/", id));
    /* NEU 1.1 (Anforderungen 27.10.2026): der App-Datenspeicher /user/download/<ID> gehört zur App und geht mit ihr -
     * er wird nicht als Download-Rest angeboten (Schritt 1), bleibt aber auch nicht verwaist zurück. */
    const char *ad = zwilling ? NULL : kern_app_daten(k, id);
    /* NEU 1.0.6: das Spiel liegt selbst auf dem USB-Speicher (keine andere sichtbare Kopie) - seine App-Daten gehen mit */
    if (!zwilling && !ad && !strncmp(basis, USB_ERW "/", strlen(USB_ERW) + 1) && !h_like(id, "NPXS*") && s_existiert(k->S, VERB(A, "/user/download/", id))) ad = VERB(A, "/user/download/", id);
    if (ad) sl_add(A, &spielPfade, ad);
    const char *gruppe = VERB(A, basis, "|", id);
    merke_teil(k, &spielPfade, kat, VERB(A, name, " (", id, ")", T(" - Spiel", " - game")), gruppe, "Spiel");
    if (!zwilling) kachel_merken(k, id, VERB(A, basis, "/app/", id));
    if (hatUpdate) {
        mt_setze(A, benutzt, VERB(A, basis, "/patch/", id), (void *)1);
        sliste u = L1(A, VERB(A, basis, "/patch/", id));
        merke_teil(k, &u, kat, VERB(A, name, " (", id, ") - Update", patch_version(k, id, basis)), gruppe, "Update");
    }
    if (hatDlc) {
        mt_setze(A, benutzt, VERB(A, basis, "/addcont/", id), (void *)1);
        sliste *leer = a_hole(A, sizeof *leer); *leer = L1(A, VERB(A, basis, "/addcont/", id));
        sliste meta = {0};
        if (h_hat(dlcInfos, id)) {
            mt_setze(A, benutzt, VERB(A, "/user/appmeta/addcont/", id), (void *)1);
            if (!zwilling) { sl_add(A, leer, VERB(A, "/user/appmeta/addcont/", id)); meta = s_namen(k->S, VERB(A, "/user/appmeta/addcont/", id), false); }
        }
        mt_setze(A, &k->LEER_DANACH, gruppe, leer);
        sliste dl = s_namen(k->S, VERB(A, basis, "/addcont/", id), false);
        for (int i = 0; i < dl.n; i++) {
            const char *d = dl.e[i];
            sliste dp = L1(A, VERB(A, basis, "/addcont/", id, "/", d));
            if (h_hat(&meta, d)) sl_add(A, &dp, VERB(A, "/user/appmeta/addcont/", id, "/", d));
            merke_teil(k, &dp, kat, VERB(A, name, " (", id, ")", T(" - Zusatzinhalt: ", " - add-on: "), dlc_name(k, id, d, basis)), gruppe, "DLC");
        }
    }
}

/* Hakt man ein Spiel an, gehen sein Update und seine Zusatzinhalte automatisch mit. */
void kern_abhaengige_mitnehmen(kern *k)
{
    mtab basis = mt_neu(true);
    for (int i = 0; i < k->Funde.n; i++) { eintrag *f = k->Funde.e[i]; if (f->Wahl && ist_teil(f, "Spiel")) mt_setze(A, &basis, f->Gruppe, (void *)1); }
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        if (f->Teil != NULL && strcmp(f->Teil, "Spiel") != 0 && f->Gruppe != NULL && mt_hat(&basis, f->Gruppe) && !f->Gesperrt) f->Wahl = true;
    }
}

/* ---------------------------------------------------------------
 * ShadowMount-Hilfen der Suche
 * --------------------------------------------------------------- */
typedef struct { sliste Links; const char *Name; bool Gesperrt; bool Mehrfach; bool Vermutet; bool Jb; const char *JbName; } qinfo;   /* Jb NEU 29.10.2026 (Codex-Prüfung 37): eine Kachel eines Jailbreak-Einstiegs zeigt hierher */   /* Vermutet NEU 27.10.2026: nur vermutete Spieldatei einer Kachel (Codex-Prüfung 11) */   /* Mehrfach NEU 1.1: mehrere Kacheln zeigen auf dieselbe Spieldatei */

static const char *sm_quelle(kern *k, const char *id)
{
    const char *lnk[2] = { "mount_img.lnk", "mount.lnk" };
    for (int i = 0; i < 2; i++) {
        sliste z = r_split(A, s_text(k->S, VERB(A, "/user/app/", id, "/", lnk[i])), "\r?\n");
        for (int j = 0; j < z.n; j++) {
            const char *s = h_trim(A, z.e[j]);
            if (s[0] == 0 || h_like(s, "/mnt/shadowmnt*")) continue;
            if (h_like(s, "/data/*")) s = VERB(A, "/user", s);
            /* NEU 1.1 (Codex-Prüfung 26.10.2026): Schreibweise vereinheitlichen ("//", "/./"), damit zwei Kacheln auf
             * dieselbe Datei als Mehrfach-Zuordnung erkannt werden: Ungewöhnliche Schreibweise ("//", "/./", "..") ->
             * Ort unbekannt, die Spieldatei wird gar nicht angeboten (nur die Kachel, bei unklarer M.2 gesperrt). */
            if (k->SCHUTZ.aktiv && h_m(s, "//|(^|/)\\.\\.?(/|$)")) return VERB(A, "?", s);   /* Ort unbekannt: keine Spieldatei anbieten */
            return s;
        }
    }
    return "";
}
static const char *quelle_ort(kern *k, const char *q)
{
    if (h_steuerzeichen(q)) return "";
    const char *m = h_escape(A, M2);
    if (h_m(q, VERB(A, "^", m, "/(user|system_data|lost\\+found)(/|$)"))) return "";
    if (h_m(q, VERB(A, "^", m, "/[^/]+"))) return "M.2";
    /* NEU 1.0.6 (da_Chris86 28.10.2026): USB wie M.2 - auch Spieldateien auf USB-Speicher und USB-Stick */
    const char *u = h_escape(A, USB_ERW);
    if (h_m(q, VERB(A, "^", u, "/(user|system_data|lost\\+found)(/|$)"))) return "";
    if (h_m(q, VERB(A, "^", u, "/[^/]+"))) return "USB";
    if (h_m(q, "^/mnt/usb\\d+/[^/]+")) return "USB-Stick";
    if (h_m(q, "^/user/data/[^/]+")) return "intern";
    return "";
}
/* NEU 1.1/1.0.6 (27.10.2026, Fehler auf da_Chris86s PS5): Die Kachel merkt sich den Ort der Spieldatei beim Eintragen
 * (mount_img.lnk). Wird die Datei danach verschoben (z. B. von /mnt/ext1/homebrew nach /data/homebrew), stimmt der
 * gemerkte Ort nicht mehr - das Spiel ist aber da, und ShadowMountPlus findet es beim nächsten Durchsuchen wieder.
 * Darum vor "tote Kachel" an allen Orten nachsehen, die ShadowMountPlus durchsucht (Standardliste aus
 * ShadowMountPlus include/sm_paths.h, dazu scanpath= aus /data/shadowmount/config.ini, Suchtiefe 1).
 * Ein Fund ist NUR EIN HINWEIS (Codex-Prüfung 11): gleicher Name oder gleiche Kennung beweist nicht dasselbe Spiel
 * (andere Version, fremde Datei). Darum wird nie zugeordnet - Kachel und vermutete Datei werden sichtbar gesperrt,
 * bis ShadowMount die Kachel neu eingetragen hat. Rückgabe: gleicher Dateiname; *idtreffer: nur gleiche Kennung. */
static const char *const SM_ORTE[] = {
    "/user/data/homebrew", "/user/data/homebrew/backports", "/user/data/etaHEN/games",
    USB_ERW "/homebrew", USB_ERW "/etaHEN/games", M2 "/homebrew", M2 "/etaHEN/games",
    "/mnt/usb0/homebrew", "/mnt/usb1/homebrew", "/mnt/usb2/homebrew", "/mnt/usb3/homebrew", "/mnt/usb4/homebrew", "/mnt/usb5/homebrew", "/mnt/usb6/homebrew", "/mnt/usb7/homebrew",
    "/mnt/usb0/etaHEN/games", "/mnt/usb1/etaHEN/games", "/mnt/usb2/etaHEN/games", "/mnt/usb3/etaHEN/games",
    "/mnt/usb4/etaHEN/games", "/mnt/usb5/etaHEN/games", "/mnt/usb6/etaHEN/games", "/mnt/usb7/etaHEN/games",
    "/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3", "/mnt/usb4", "/mnt/usb5", "/mnt/usb6", "/mnt/usb7", USB_ERW, M2, NULL };
static int sm_neue_orte(kern *k, const char *q, const char *id, sliste *treffer)
{
    *treffer = (sliste){0};
    int i = txt_letzter(q, '/');
    if (i < 0) return 0;
    const char *name = q + i + 1;
    if (name[0] == 0) return 0;
    sliste orte = {0};
    for (int j = 0; SM_ORTE[j]; j++) sl_add(A, &orte, SM_ORTE[j]);
    sliste z = r_split(A, s_text(k->S, SM_ORDNER "/config.ini"), "\r?\n");
    for (int j = 0; j < z.n; j++) {
        htreffer t = h_mt(A, z.e[j], "^\\s*scanpath\\s*=\\s*(/[^#;]*?)\\s*([#;].*)?$");   /* Kommentar nach # oder ; wie ShadowMountPlus */
        if (!t.ok) continue;
        const char *o = t.gr[1];
        if (h_like(o, "/data/*")) o = VERB(A, "/user", o);
        while (strlen(o) > 1 && o[strlen(o) - 1] == '/') o = a_kopie_n(A, o, strlen(o) - 1);
        if (!h_m(o, "//|(^|/)\\.\\.?(/|$)") && !h_hat(&orte, o)) sl_add(A, &orte, o);
    }
    /* ALLE Treffer (Codex-Prüfung 14): zuerst gleicher Name, dann gleiche Kennung - jeder wird gesperrt.
     * NEU 1.0.6 (Codex-Prüfung 30): ein nicht lesbarer USB-Stick bricht die Suche nicht ab - dort wird dann nichts gefunden. */
    for (int j = 0; j < orte.n; j++) {
        const char *kand = VERB(A, orte.e[j], "/", name);
        if (strcmp(kand, q) == 0 || sl_enthaelt(treffer, kand)) continue;
        volatile bool da = false;
        if (h_like(orte.e[j], "/mnt/usb*")) {
            VERSUCHE { da = s_existiert(k->S, kand); }
            FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); da = false; } ENDE_VERSUCHE;
        }
        else da = s_existiert(k->S, kand);
        if (da) sl_add(A, treffer, kand);
    }
    for (int j = 0; j < orte.n; j++) {
        sliste n = {0};
        if (h_like(orte.e[j], "/mnt/usb*")) {
            VERSUCHE { n = s_namen(k->S, orte.e[j], false); }
            FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); n = (sliste){0}; } ENDE_VERSUCHE;
        }
        else n = s_namen(k->S, orte.e[j], false);
        for (int m = 0; m < n.n; m++) if (h_like(n.e[m], VERB(A, "*", id, "*"))) {
            const char *kand = VERB(A, orte.e[j], "/", n.e[m]);
            if (strcmp(kand, q) != 0 && !sl_enthaelt(treffer, kand)) sl_add(A, treffer, kand);
        }
    }
    return treffer->n;
}
static bool ist_zwischendatei(const char *name) { return h_m(name, "\\.gc-compress\\.tmp"); }
static bool wird_komprimiert(kern *k, const char *q, const char *id)
{
    int i = txt_letzter(q, '/');
    if (i < 0) wirf(AUSN_FEHLER, "Die Länge darf nicht kleiner als 0 sein.");
    const char *eltern = a_kopie_n(A, q, (size_t)i);        /* q.Substring(0, LastIndexOf('/')) */
    const char *name = q + i + 1;
    const char *basis = h_r(A, name, "\\.[^.]+$", "");
    if (!mt_hat(&k->ORDNER_CACHE, eltern)) { sliste *n = a_hole(A, sizeof *n); *n = s_namen(k->S, eltern, true); mt_setze(A, &k->ORDNER_CACHE, eltern, n); }
    sliste *l = mt_hole(&k->ORDNER_CACHE, eltern);
    for (int j = 0; j < l->n; j++) {
        const char *n = l->e[j];
        if (!ist_zwischendatei(n)) continue;
        const char *ohne = n; while (*ohne == '.') ohne++;     /* TrimStart('.') */
        if (h_like(n, VERB(A, "*", id, "*")) || txt_beginnt(ohne, basis)) return true;
    }
    return false;
}

/* Listet einen Ordner auf. Unterordner mit ShadowMount-Spielen werden
 * aufgeklappt; in Verknüpfungen (Symlinks) wird nie hineingeschaut. */
/* NEU 1.0.6 (PS5-Test 28.10.2026, da_Chris86: "Thema Benutzerfreundlichkeit"): Spiel-Abbild, dessen Spielnummer zu einer Kachel
 * gehört, die eine ANDERE Datei nutzt -> "zweite Spieldatei" statt "ohne Kachel". NULL = keine solche Kachel. */
static const char *zweite_spieldatei(kern *k, const char *p, const char *name)
{
    htreffer t = h_mt(A, name, "([A-Z]{4}\\d{5})");
    if (!t.ok) return NULL;
    for (int j = 0; j < k->SpielQuellen.n; j++) {
        const char *q = k->SpielQuellen.e[j];
        if (!strcmp(q, p)) continue;
        int i = txt_letzter(q, '/');
        if (!strstr(q + i + 1, t.gr[1])) continue;
        qinfo *qi = mt_hole(&k->QuelleInfo, q);
        const char *wo;
        switch (kern_pfad_ort(q)) {
            case ORT_M2: wo = "M.2"; break;
            case ORT_USB: wo = "USB"; break;
            case ORT_STICK: wo = T("USB-Stick", "USB stick"); break;
            default: wo = T("intern", "internal"); break;
        }
        (void)qi;   /* 28.10.2026 (da_Chris86: Texte zu lang): kurz, ohne Namen - die Spielnummer steht im Dateinamen */
        return VERB(A, T("  [2. Kopie – Kachel nutzt ", "  [2nd copy – tile uses "), wo, "]");
    }
    return NULL;
}
/* NEU 1.0.6: alles unter p einsammeln, was nicht zu PS5 Cleanup gehört (frei), und die Teile von PS5 Cleanup (eig).
 * Ordner, die PS5 Cleanup enthalten, werden weiter durchsucht und nie als Ganzes in "frei" aufgenommen. */
static void sammle_ohne_schutz(kern *k, const char *p, sliste *frei, sliste *eig)
{
    sl_add(A, &k->AUFGEKLAPPT, p);
    dliste *l = s_liste(k->S, p, true);
    for (int i = 0; l && i < l->n; i++) {
        const char *q = VERB(A, p, "/", l->e[i].name);
        if (!l->e[i].ist_link && l->e[i].ist_ordner && kern_selbstschutz_enthaelt(k, q)) sammle_ohne_schutz(k, q, frei, eig);
        else if (!kern_selbstschutz_anbieten(k, q)) sl_add(A, eig, q);
        else sl_add(A, frei, q);
    }
}
/* NEU 1.0.6: entpacktes Spiel? (nur lesen; ein Lesefehler zählt als "nein" - dann bleibt der Ordner, wo er war) */
static bool spiel_ordner(kern *k, const char *p)
{
    volatile bool ja = false;
    VERSUCHE { ja = s_existiert(k->S, VERB(A, p, "/sce_sys/param.json")) || s_existiert(k->S, VERB(A, p, "/sce_sys/param.sfo")); }
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); ja = false; } ENDE_VERSUCHE;
    return ja;
}
/* NEU 1.0.6 (PS5-Test da_Chris86 30.10.2026 08:10: "es ist ein Spiel, daher hat es da unter 6 nichts zu suchen"): liegt in einem
 * Ordner direkt ein Spiel (Spiel-Abbild, ISO mit Spielnummer oder entpacktes Spiel) ohne Kachel, wird der Ordner aufgeklappt wie
 * bei ShadowMount-Spielen - das Spiel steht dann unter "Spiele", der Rest des Ordners einzeln an seinem Platz.
 * Beispiel: USB-Stick /mnt/usb0/homebrew/PPSA03530.exfat. Nur lesen; ein Lesefehler zählt als "nein" (Ordner bleibt ein Eintrag). */
static bool enthaelt_spiel(kern *k, const char *p)
{
    volatile bool ja = false;
    VERSUCHE {
        dliste *u = s_liste(k->S, p, false);
        for (int i = 0; u && i < u->n && !ja; i++) {
            const datei *d = &u->e[i];
            if (d->ist_link || ist_zwischendatei(d->name)) continue;
            if (!d->ist_ordner && (h_m(d->name, "(?i)\\.(exfat|ffpfs|ffpfsc|ffpkg|pfs|img)$")
                                   || (h_m(d->name, "(?i)\\.iso$") && h_m(d->name, "(?i)(^|[^a-z])[a-z]{4}[-_]?\\d{5}")))) ja = true;
            else if (d->ist_ordner && h_m(d->name, "(^|[^A-Z0-9])[A-Z]{4}\\d{5}([^0-9]|$)") && spiel_ordner(k, VERB(A, p, "/", d->name))) ja = true;
        }
    }
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); ja = false; } ENDE_VERSUCHE;
    return ja;
}
static void merke_ordner(kern *k, const char *basis, const datei *eintraege, int anz, int kat, const char *vorsatz)
{
    for (int i = 0; i < anz; i++) {
        const datei *e = &eintraege[i];
        const char *x = e->name; bool link = e->ist_link;
        const char *p = VERB(A, basis, "/", x);
        if (ist_zwischendatei(x)) { sl_add(A, &k->KOMPRIMIERUNG, p); continue; }
        if (sl_enthaelt(&k->SpielQuellen, p)) {
            qinfo *qi = mt_hole(&k->QuelleInfo, p);
            sliste pf = L1(A, p); sl_add_alle(A, &pf, &qi->Links);
            /* da_Chris86 28.10.2026 13:41: Ein Spiel-Abbild ist ein Spiel - auf der internen SSD genauso wie auf M.2/USB. Ist die
             * Zuordnung zu einer Kachel nicht eindeutig (vermutlich verschoben, mehrere Kacheln, andere Kennung im Namen), wird es
             * als eigenes Spiel angeboten (nur die Datei); die Kachel(n) sind eigene Einträge und nehmen die Datei nie mit. */
            if (qi->Vermutet || qi->Mehrfach) {
                sliste nur = L1(A, p);
                MERKE(k, &nur, 1, VERB(A, vorsatz, x, T("  [Spiel-Abbild: ", "  [game image: "), qi->Name,
                                         qi->Mehrfach ? T(" - mehrere Kacheln zeigen darauf, sie sind eigene Einträge]", " - several tiles point to it, they are separate entries]")
                                                      : T(" - die Kachel ist ein eigener Eintrag]", " - the tile is a separate entry]")), 0);
                /* Codex-Prüfung 37: gehört (auch) zu einer Kachel eines Jailbreak-Einstiegs - geschützt wie der Einstieg */
                eintrag *b = (qi->Jb && k->MERKE_OK && k->bau.n > 0) ? k->bau.e[k->bau.n - 1] : NULL;
                if (b && !b->Gesperrt) { b->Jailbreak = true; b->NurEinzeln = true;
                    b->Text = b->TextBasis = qi->JbName ? VERB(A, b->Text, T("  [Jailbreak-Einstieg ", "  [jailbreak entry "), qi->JbName, T(" - nur einzeln]", " - individually only]")) : VERB(A, b->Text, kern_nur_einzeln_text()); }
                continue;
            }
            /* NEU 1.0.6: Spiel-Abbild mit eindeutiger Kachel - steht EINMAL als Spiel in Bereich 1 (Eintrag der Kachel mit
             * "nur Kachel" oder "Spiel + Kachel"), nicht noch einmal hier (bis 1.0.5: zusätzlich in Bereich 6/7) */
            (void)pf;
            continue;
        }
        const char *pz = VERB(A, p, "/");
        bool drin = false;
        for (int j = 0; j < k->SpielQuellen.n; j++) if (txt_beginnt(k->SpielQuellen.e[j], pz)) drin = true;
        /* NEU 1.0.6 (PS5-Test 28.10.2026, da_Chris86: "doof, wenn so viele pldmgr-Einträge"): liegt NUR PS5 Cleanup in einem
         * Ordner (kein ShadowMount-Spiel), wird er nicht Datei für Datei aufgeklappt, sondern als EIN Eintrag "alles außer
         * PS5 Cleanup" angeboten; PS5 Cleanup selbst steht gesperrt daneben. Ausnahme: Ordner "homebrew" - dort bleibt jede
         * App ein eigener Eintrag. */
        if (!drin && !link && e->ist_ordner && kern_selbstschutz_enthaelt(k, p) && !h_gleich(x, "homebrew")) {
            sliste frei = {0}, eig = {0};
            sammle_ohne_schutz(k, p, &frei, &eig);
            bool echt_eigen = false;   /* 1.0.6 Fix: nur private, nicht lesbare Ordner geschützt (Playden) -> ohne Zusatz */
            for (int j = 0; j < eig.n; j++) if (!kern_selbstschutz_unlesbar(k, eig.e[j])) echt_eigen = true;
            if (frei.n > 0) MERKE(k, &frei, kat, echt_eigen ? VERB(A, vorsatz, x, T("  (alles außer PS5 Cleanup)", "  (everything except PS5 Cleanup)")) : VERB(A, vorsatz, x), 0);
            for (int j = 0; j < eig.n; j++) { sliste l = L1(A, eig.e[j]); MERKE(k, &l, kat, VERB(A, vorsatz, x, eig.e[j] + strlen(p)), 0); }
            continue;
        }
        /* NEU 1.1, Selbstschutz: liegt PS5 Cleanup in diesem Ordner, wird er aufgeklappt wie bei ShadowMount-Spielen */
        if (kern_selbstschutz_enthaelt(k, p)) drin = true;
        if (!drin && !link && e->ist_ordner && kat != 1 && enthaelt_spiel(k, p)) drin = true;   /* NEU 1.0.6 (30.10.2026): Spiel im Ordner */
        if (drin && !link) {
            sl_add(A, &k->AUFGEKLAPPT, p);
            dliste *unter = s_liste(k->S, p, true);
            merke_ordner(k, p, unter ? unter->e : NULL, unter ? unter->n : 0, kat, VERB(A, vorsatz, x, "/"));
        }
        else if (link) { sliste l = L1(A, p); MERKE(k, &l, kat, VERB(A, vorsatz, x, T("  (Verknüpfung, nur sie wird gelöscht)", "  (symlink, only the link is deleted)")), 0); }
        /* NEU 1.0.6 (PS5-Test da_Chris86 29.10.2026 19:44): entpacktes Spiel (Ordner mit Spielnummer im Namen und sce_sys/param.json
         * oder param.sfo darin, z. B. "F1® 25 - PPSA24662 - v1.26.0 - US") ist ein Spiel wie ein Spiel-Abbild - Bereich 1 */
        else if (e->ist_ordner && kat != 1 && h_m(x, "(^|[^A-Z0-9])[A-Z]{4}\\d{5}([^0-9]|$)")
                 && spiel_ordner(k, p)) {
            sliste l = L1(A, p); MERKE(k, &l, 1, VERB(A, vorsatz, x, T("  [Spiel-Ordner ohne Kachel]", "  [game folder, no tile]")), 0);
        }
        else if (!e->ist_ordner && h_m(x, "(?i)\\.(exfat|ffpfs|ffpfsc|ffpkg|pfs|img)$")) {   /* 28.10.2026: Spiel-Abbild ohne Kachel - ein Spiel (Bereich 1) */
            const char *zw = zweite_spieldatei(k, p, x);
            sliste l = L1(A, p); MERKE(k, &l, 1, VERB(A, vorsatz, x, zw ? zw : T("  [ohne Kachel]", "  [no tile]")), 0);
        }
        else if (!e->ist_ordner && h_m(x, "(?i)\\.iso$") && h_m(x, "(?i)(^|[^a-z])[a-z]{4}[-_]?\\d{5}")) {
            /* NEU 1.0.6 (PS5-Test 28.10.2026): ISO mit Spielnummer im Namen (z. B. PS3 BLES01969, PS2 SLES-12345) = Spiel-Abbild;
             * ohne Spielnummer (z. B. Linux-ISO) bleibt sie ein normaler Eintrag */
            sliste l = L1(A, p); MERKE(k, &l, 1, VERB(A, vorsatz, x, T("  [ISO]", "  [ISO]")), 0);
        }
        else if (!e->ist_ordner && h_m(x, "(?i)\\.elf$")) {   /* NEU 1.0.6: ELF-Dateien aller Speicher -> Jailbreak & Homebrew */
            sliste l = L1(A, p); MERKE(k, &l, 6, VERB(A, vorsatz, x), 0);
        }
        else { sliste l = L1(A, p); MERKE(k, &l, kat, VERB(A, vorsatz, x), 0); }
    }
}

/* NEU 1.0.4: Ordner direkt auf der M.2 werden aufgeklappt - jedes Spiel,
 * jeder Unterordner und jede Datei darin ist ein eigener Eintrag. Der
 * Ordner selbst bleibt immer stehen (nur ein leerer wird angeboten).
 * NEU 1.0.6: genauso auf dem USB-Speicher (da_Chris86 28.10.2026: "USB wie M.2"); die Einträge landen in
 * Bereich 5 (Reste), Spiel-Abbilder in Bereich 1, ELF-Dateien in Bereich 6 - jeweils mit Ort vorne. */
/* NEU 1.0.6 (Codex-Prüfung 27): Homebrew-Ordner (homebrew, etaHEN, payloads) auf M.2, USB-Speicher und USB-Stick
 * gehören zu "Jailbreak & Homebrew" (Bereich 6), alles andere zu den Resten (Bereich 5) */
static int ext_kat(const char *name) { return h_m(name, "(?i)^(homebrew|etahen|payloads?)$") ? 6 : 5; }
static void merke_ext_ordner(kern *k, const char *basis, const datei *e)
{
    const char *p = VERB(A, basis, "/", e->name);
    int kat = ext_kat(e->name);
    if (!e->ist_ordner || e->ist_link || ist_zwischendatei(e->name) || sl_enthaelt(&k->SpielQuellen, p)) { merke_ordner(k, basis, e, 1, kat, ""); return; }
    dliste *unter = s_liste(k->S, p, true);
    /* nicht lesbar: wie bisher als Ganzes anbieten (nie als "leer" ausgeben) */
    if (unter == NULL) { merke_ordner(k, basis, e, 1, kat, ""); return; }
    if (unter->n == 0) {
        sliste l = L1(A, p);
        MERKE(k, &l, kat, VERB(A, e->name, T("  (leerer Ordner)", "  (empty folder)")), 0);
        if (k->MERKE_OK) ((eintrag *)k->bau.e[k->bau.n - 1])->NurLeer = true;
        return;
    }
    merke_ordner(k, p, unter->e, unter->n, kat, VERB(A, e->name, "/"));
}

/* NEU 1.0.5: Hinweis, wenn Spiele auf einem Speicher liegen könnten, den das
 * Programm gerade nicht als "installiert" sieht. Nur Text - die Suche bleibt gleich. */
const char *kern_speicher_hinweis(kern *k)
{
    const char *t = "";
    /* da_Chris86 27.10.2026 21:39: fehlende/nicht erkannte M.2 sperrt nichts - nur der Hinweis wie 1.0.5 */
    if (!k->m2Da) t = VERB(A, t, T("\n\nHINWEIS: Keine M.2-SSD gefunden. Steckt eine M.2 mit Spielen drin, die gerade nicht erkannt wird: abbrechen und die PS5 neu starten - sonst werden Lizenzen und Kacheln dieser Spiele mit gelöscht.",
                                   "\n\nNOTE: No M.2 SSD found. If an M.2 with games is installed but not detected right now: cancel and restart the PS5 - otherwise the licenses and tiles of those games will be deleted too."));
    else if (k->M2_OHNE_USER) t = VERB(A, t, T("\n\nHINWEIS: Die M.2-SSD ist leer oder wird gerade nicht richtig erkannt. Liegen darauf Spiele: abbrechen und die PS5 neu starten - sonst werden Lizenzen und Kacheln dieser Spiele mit gelöscht.",
                                              "\n\nNOTE: The M.2 SSD is empty or not detected properly right now. If it holds games: cancel and restart the PS5 - otherwise the licenses and tiles of those games will be deleted too."));
    if (k->USB_SPEICHER)     /* NEU 1.1 (U17); seit 27.10.2026 auch ohne Selbstschutz, gleich wie Windows 1.0.6 - nur Hinweis (Codex-Prüfung 22) */
        t = VERB(A, t, T("\n\nHINWEIS: Eine USB-Platte als erweiterter Speicher ist angeschlossen. Spiele darauf gelten als installiert.",
                         "\n\nNOTE: A USB drive is connected as extended storage. Games on it count as installed."));
    return t;
}

/* mitKachel */
static sliste mit_kachel(kern *k, const sliste *pf, const char *id, const sliste *sysmeta)
{
    if (h_hat(sysmeta, id) && !h_like(id, "NPXS*") && !h_hat(&k->ASTRO_IDS, id)) { sliste n = sl_kopie(A, pf); sl_add(A, &n, VERB(A, SYS_META "/", id)); return n; }
    return *pf;
}

static int vergleich_ci_kultur(const void *x, const void *y)
{
    /* OrderBy(StringComparer.CurrentCultureIgnoreCase) über Schlüssel der Form
     * NPWR\d{5}_\d{2} (nur solche kommen hier an): gleich lange Ziffernfolgen an
     * gleicher Stelle -> dieselbe Reihenfolge wie ein Vergleich ohne Groß/klein. */
    return strcasecmp(*(const char *const *)x, *(const char *const *)y);
}

/* ---------------------------------------------------------------
 * Die Suche
 * --------------------------------------------------------------- */
static bool abbruch_kern(void *c) { return ((kern *)c)->abbruch; }

/* NEU 1.0.6 (PS5-Test 28.10.2026, da_Chris86: "Benutzerfreundlichkeit"): die Daten eines Jailbreak-Tools stehen direkt unter
 * seiner Tool-Kachel (eingerückt mit "   – "), jeder Eintrag bleibt einzeln wählbar (nur Tool, nur Daten oder beides).
 * Zugeordnet wird nur, wenn genau EINE Kachel passt - sonst bleibt der Eintrag, wo er war. */
static const char *tool_schluessel(kern *k, const char *s)
{
    size_t n = strlen(s); char *o = a_hole(A, n + 1); size_t j = 0;
    for (size_t i = 0; i < n; i++) { char c = s[i]; if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) o[j++] = c; }
    o[j] = 0; return o;
}
static const char *tool_alias(const char *id)
{
    static const char *const T2[][2] = { { "PLDM00001", "pldmgr" }, { "PKGM00001", "pkgmgr" }, { "APDU00001", "ps5appdumper" }, { NULL, NULL } };
    for (int i = 0; T2[i][0]; i++) if (!strcmp(T2[i][0], id)) return T2[i][1];
    return "";
}
static bool tool_passt(const char *tk, const char *ak, const char *dk)
{
    size_t lt = strlen(tk), ld = strlen(dk), p = 0;
    if (ld == 0) return false;
    if (ak[0] && !strcmp(ak, dk)) return true;
    if (ld >= 5 && strstr(tk, dk)) return true;
    if (lt >= 5 && strstr(dk, tk)) return true;
    while (p < lt && p < ld && tk[p] == dk[p]) p++;
    return p >= 8;
}
static void tool_daten_ordnen(kern *k, pliste *liste)
{
    const char *vor = T("Tool-Kachel: ", "Tool tile: ");
    int n = liste->n;
    int *zu = a_hole(A, sizeof(int) * (size_t)(n > 0 ? n : 1));   /* Index der Kachel, zu der ein Daten-Eintrag gehört (-1 = keine) */
    for (int i = 0; i < n; i++) zu[i] = -1;
    for (int d = 0; d < n; d++) {
        eintrag *f = liste->e[d];
        if (f->Kat != 6 || f->Ort != ORT_INTERN || f->Kachel || f->PfadeBasis.n == 0) continue;
        htreffer m = h_mt(A, f->PfadeBasis.e[0], "^/user/data/(homebrew/)?([^/]+)");
        if (!m.ok) continue;
        const char *dk = tool_schluessel(k, m.gr[2]);
        int treffer = -1, anz = 0;
        for (int t = 0; t < n; t++) {
            eintrag *g = liste->e[t];
            if (g->Kat != 6 || !g->Kachel || !txt_beginnt(g->TextBasis, vor) || g->PfadeBasis.n == 0) continue;
            htreffer mi = h_mt(A, g->PfadeBasis.e[0], "^/user/app/([A-Z]{4}\\d{5})$");
            if (!mi.ok) continue;
            const char *titel = g->TextBasis + strlen(vor);
            const char *ende = strstr(titel, VERB(A, " (", mi.gr[1], ")"));
            const char *tk = tool_schluessel(k, ende ? a_kopie_n(A, titel, (size_t)(ende - titel)) : titel);
            if (tool_passt(tk, tool_alias(mi.gr[1]), dk)) { treffer = t; anz++; }
        }
        if (anz == 1) zu[d] = treffer;
    }
    pliste neu = {0};
    for (int i = 0; i < n; i++) {
        if (zu[i] >= 0) continue;                       /* steht unter seiner Kachel */
        pl_add(A, &neu, liste->e[i]);
        for (int d = 0; d < n; d++) if (zu[d] == i) {
            eintrag *f = liste->e[d];
            f->Text = VERB(A, "   – ", f->Text); f->TextBasis = VERB(A, "   – ", f->TextBasis);
            pl_add(A, &neu, f);
        }
    }
    *liste = neu;
}

void kern_suche_funde(kern *k)
{
    k->S->abbruch = abbruch_kern; k->S->abbruch_ctx = k;
    k->FUNDE_GUELTIG = false;                 /* erst gültig, wenn die Suche ganz durch ist */
    k->bau = (pliste){0};                     /* die neue Liste wird erst am Ende sichtbar */
    k->LEER_DANACH = mt_neu(true);
    kern_groessen_zuruecksetzen(k);
    /* NEU 1.1 (S3): konnte der Selbstschutz nicht sicher ermittelt werden, wird gar nichts angeboten */
    if (k->SCHUTZ.aktiv && k->SCHUTZ.fehler) wirf(AUSN_FEHLER, k->SCHUTZ.fehler);
    /* Diese Listen entscheiden, was als "nicht mehr installiert" gilt - darum
     * streng: scheitert eine, bricht die Suche ab, statt Falsches anzubieten. */
    k->SHARES = s_namen(k->S, "/user/system/share", true);
    sliste homes = s_namen(k->S, "/user/home", true);
    sliste appmeta = s_namen(k->S, "/user/appmeta", true);
    sliste apps = s_namen(k->S, "/user/app", true);
    sliste patches = s_namen(k->S, "/user/patch", true);
    sliste addconts = s_namen(k->S, "/user/addcont", true);
    sliste dlcInfos = s_namen(k->S, "/user/appmeta/addcont", true);
    dliste *m2Liste = s_liste(k->S, M2, true);
    k->m2Da = m2Liste != NULL;
    k->M2_OHNE_USER = false;
    if (k->m2Da) { bool user = false; for (int i = 0; i < m2Liste->n; i++) if (strcmp(m2Liste->e[i].name, "user") == 0) user = true; k->M2_OHNE_USER = !user; }
    /* NEU 1.1 (Sicherheitsprüfung Punkt 7, Entscheidung 26.10.2026): Ist die M.2 nicht (richtig) erkannt, ist unklar, ob ihre
     * Spiele noch da sind. Dann werden Kacheln, Lizenzen und andere Reste, die zu solchen Spielen gehören könnten, gesperrt. */
    /* NEU 27.10.2026 (Durchlauf 1, nur lesend auf da_Chris86s PS5): Die eingebaute M.2 erscheint als Gerät /dev/ssd1 (ssd1.user ist auf
     * /mnt/ext1 eingehängt). Fehlt das Gerät, ist keine M.2 eingebaut - dann ist nichts unklar. Ist /dev nicht lesbar: unbekannt = unklar. */
    {
        volatile int g = -1;
        VERSUCHE { dliste *dev = s_liste(k->S, "/dev", false); if (dev) { g = 0; for (int i = 0; i < dev->n; i++) if (strcmp(dev->e[i].name, "ssd1") == 0) g = 1; } }
        FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); g = -1; } ENDE_VERSUCHE;
        k->M2_GERAET = g;
    }
    k->M2_UNKLAR = k->SCHUTZ.aktiv && (!k->m2Da || k->M2_OHNE_USER) && k->M2_GERAET != 0;
    /* NEU 1.1 (Codex-Prüfung 26.10.2026): M.2 nur teilweise sichtbar (user/ da, aber keine Spieleliste user/app)
     * gilt ebenfalls als unklar - sonst würden Reste ihrer Spiele als verwaist angeboten. */
    if (k->SCHUTZ.aktiv && k->m2Da && !k->M2_OHNE_USER && s_liste(k->S, M2 "/user/app", true) == NULL) k->M2_UNKLAR = true;
    k->USB_SPEICHER = false;
    {
        volatile bool usb = false;
        VERSUCHE { usb = s_liste(k->S, USB_ERW "/user", false) != NULL; }
        FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); } ENDE_VERSUCHE;
        k->USB_SPEICHER = usb;
    }
    /* NEU 1.1 (U17): Spiele auf der USB-Erweiterung gelten als installiert (1.0.5 kannte sie nicht und bot
     * ihre Kacheln, Lizenzen und Einstellungen als Reste an). Streng gelesen: Fehler = Abbruch. */
    sliste usbApps = {0}, usbPatches = {0}, usbAddconts = {0};
    if (k->USB_SPEICHER) {   /* 27.10.2026: auch ohne Selbstschutz, gleich wie Windows 1.0.6; NEU 1.0.6: USB wie M.2 (Updates, Zusatzinhalte) */
        usbApps = s_namen(k->S, USB_ERW "/user/app", true);
        usbPatches = s_namen(k->S, USB_ERW "/user/patch", false); usbAddconts = s_namen(k->S, USB_ERW "/user/addcont", false);
    }
    sliste m2Apps = {0}, m2Patches = {0}, m2Addconts = {0};
    if (k->m2Da) { m2Apps = s_namen(k->S, M2 "/user/app", true); m2Patches = s_namen(k->S, M2 "/user/patch", true); m2Addconts = s_namen(k->S, M2 "/user/addcont", true); }
    mtab benutzt = mt_neu(true);
    sliste sysmeta = s_namen(k->S, SYS_META, true);
    /* Namen aller Kacheln zuerst lesen - dabei wird Astro unter jeder
     * Länderkennung erkannt, bevor irgendetwas anderes gelistet wird. */
    k->ASTRO_IDS = (sliste){0}; k->ASTRO_NPWRS = (sliste){0};   /* 1.0.6: Astro ist ein normales Spiel */
    k->NAMEN_CACHE = mt_neu(true); k->TITEL_CACHE = mt_neu(true);
    k->GALERIE_NAMEN = mt_neu(true); k->NPWR_GALERIE = mt_neu(true);
    sliste alle = {0};
    const sliste *quellen[4] = { &apps, &m2Apps, &appmeta, &sysmeta };
    for (int q = 0; q < 4; q++) for (int i = 0; i < quellen[q]->n; i++) if (!sl_enthaelt(&alle, quellen[q]->e[i])) sl_add(A, &alle, quellen[q]->e[i]);
    for (int i = 0; i < alle.n; i++) {
        const char *id = alle.e[i];
        if (!h_m(id, "^[A-Z]{4}\\d{5}$") || h_like(id, "NPXS*")) continue;
        if (ist_astro(k, app_titel(k, id))) astro_merken(k, id);
    }
    /* Zuordnung Trophäen-Kennung (NPWR...) -> Spielname über die Kachel-Infos */
    k->NPWR_NAMEN = mt_neu(true);
    for (int i = 0; i < sysmeta.n; i++) {
        const char *id = sysmeta.e[i];
        if (!h_m(id, "^[A-Z]{4}\\d{5}$")) continue;
        const char *subs[2] = { "trophy2", "trophy" };
        for (int s = 0; s < 2; s++) {
            const char *nb = s_text(k->S, VERB(A, SYS_META "/", id, "/", subs[s], "/npbind.dat"));
            sliste ms = r_alle(A, nb, "NPWR\\d{5}_\\d{2}");
            if (ms.n == 0) continue;
            if (!mt_hat(&k->NAMEN_CACHE, id)) { mt_setze(A, &k->NAMEN_CACHE, id, (void *)app_titel(k, id)); if (ist_astro(k, mt_hole(&k->NAMEN_CACHE, id))) astro_merken(k, id); }
            if (h_hat(&k->ASTRO_IDS, id)) for (int j = 0; j < ms.n; j++) if (!h_hat(&k->ASTRO_NPWRS, ms.e[j])) sl_add(A, &k->ASTRO_NPWRS, ms.e[j]);
            for (int j = 0; j < ms.n; j++) if (!mt_hat(&k->NPWR_NAMEN, ms.e[j])) mt_setze(A, &k->NPWR_NAMEN, ms.e[j], mt_hole(&k->NAMEN_CACHE, id));
            break;
        }
    }
    k->KOMPRIMIERUNG = (sliste){0};
    k->AUFGEKLAPPT = (sliste){0};

    /* ---- ShadowMount ------------------------------------------------ */
    k->SpielQuellen = (sliste){0};
    k->JB_QUELLEN = (sliste){0};
    k->QuelleInfo = mt_neu(false);
    k->SperrQuellen = (sliste){0};
    k->ORDNER_CACHE = mt_neu(true);

    /* ---- 1 Spiele & Apps (und Tool-Kacheln -> 6) -------------------- */
    /* NEU 1.0.6 (Codex-Prüfung 31, Anlass push_resource): angeboten werden in den Sony-Spielordnern (app, patch, addcont,
     * appmeta/addcont, appmeta/external/addcont - intern, M.2, USB) nur Ordner mit Spielnummer; alles andere bleibt unangetastet */
    for (int i = 0; i < apps.n; i++) {
        const char *x = apps.e[i];
        if (h_hat(&k->ASTRO_IDS, x) || h_like(x, "NPXS*") || !h_m(x, "^[A-Z]{4}\\d{5}$")) continue;
        const char *t = app_titel(k, x);
        if (ist_astro(k, t)) { astro_merken(k, x); continue; }
        const char *roh = k->APP_ROH;
        if (t[0] == 0) t = T("unbekannter Name", "unknown name");
        sliste p = L1(A, VERB(A, "/user/app/", x));
        bool zwilling = h_hat(&m2Apps, x);
        /* NEU 1.1 (Codex-Prüfung 26.10.2026): auch eine Kopie auf der USB-Erweiterung oder eine gerade nicht erkannte
         * M.2 zählt als möglicher Zwilling - gemeinsame Kachel-Infos bleiben dann stehen. */
        if (h_hat(&usbApps, x)) zwilling = true;   /* nur eine SICHTBARE Kopie (da_Chris86 27.10.2026 21:39) */
        if (h_hat(&appmeta, x)) { mt_setze(A, &benutzt, VERB(A, "/user/appmeta/", x), (void *)1); if (!zwilling) sl_add(A, &p, VERB(A, "/user/appmeta/", x)); }
        if (!zwilling) p = mit_kachel(k, &p, x, &sysmeta);
        sliste inhalt = s_namen(k->S, VERB(A, "/user/app/", x), true);
        if (h_m(roh, "\"deeplinkUri\"\\s*:\\s*\"https?://(127\\.0\\.0\\.1|localhost)")) {
            const char *ad = zwilling ? NULL : kern_app_daten(k, x);   /* NEU 1.1: App-Daten gehen mit der Kachel */
            if (ad) sl_add(A, &p, ad);
            MERKE(k, &p, 6, VERB(A, T("Tool-Kachel: ", "Tool tile: "), t, " (", x, ")"), .kachel = true);
            if (!zwilling) kachel_merken(k, x, VERB(A, "/user/app/", x));
        }
        else if (h_hat(&inhalt, "mount.lnk") || h_hat(&inhalt, "mount_img.lnk")) {
            /* NEU 27.10.2026 (da_Chris86 19:50 und 21:39): "Nur Kachel" entfernt die Kachel samt Kachel-Infos - auch wenn M.2 oder USB fehlen
             * oder nicht erkannt werden. Gesperrt wird nur eine Spieldatei, deren Zuordnung nicht sicher ist (vermutet/mehrfach/fremd). */
            const char *q = sm_quelle(k, x);
            const char *ort = quelle_ort(k, q);
            bool da = false;
            if (ort[0]) da = s_existiert(k->S, q);
            /* NEU 27.10.2026 (Codex-Prüfung 14): trägt die Datei am gemerkten Ort eine ANDERE Kennung im Namen, ist es nicht dieses Spiel */
            bool fremd = false;                        /* dann: unklar - Datei sperren, Kachel nur Kachel */
            if (da) { int sl = txt_letzter(q, '/'); htreffer m = h_mt(A, q + (sl >= 0 ? sl + 1 : 0), "([A-Z]{4}\\d{5})"); if (m.ok && !txt_gleich_ci(m.gr[1], x)) { da = false; fremd = true; } }
            const char *kand = NULL; sliste kands = {0};
            if (ort[0] && !da) {                       /* NEU 27.10.2026: verschobene Spieldatei suchen - nur als Hinweis */
                sliste andere = {0};
                if (fremd) sl_add(A, &kands, q);
                sm_neue_orte(k, q, x, &andere);
                sl_add_alle(A, &kands, &andere);
                if (kands.n > 0) kand = kands.e[0];
            }
            if (ort[0] && da) {
                bool g = wird_komprimiert(k, q, x);
                qinfo *alt = mt_hole(&k->QuelleInfo, q);
                bool schon = sl_enthaelt(&k->SpielQuellen, q);   /* NEU 1.1: zeigt schon eine andere Kachel hierher (auch nur vermutet) -> mehrdeutig */
                if (!schon) sl_add(A, &k->SpielQuellen, q);
                qinfo *qi = a_hole(A, sizeof *qi); qi->Links = p; qi->Name = VERB(A, t, " (", x, ")"); qi->Gesperrt = g;
                qi->Jb = kern_jb_einstieg(x) || (alt && alt->Jb);
                qi->JbName = kern_jb_einstieg(x) ? qi->Name : (alt ? alt->JbName : NULL);
                if (kern_jb_einstieg(x) && !sl_enthaelt(&k->JB_QUELLEN, q)) sl_add(A, &k->JB_QUELLEN, q);
                if (schon) { if (alt) alt->Mehrfach = true; qi->Mehrfach = true; }
                mt_setze(A, &k->QuelleInfo, q, qi);
                MERKE(k, &p, 1, VERB(A, t, " (", x, ")", T(" - ShadowMount-Spiel", " - ShadowMount game"), strcmp(ort, "intern") == 0 ? T(" auf der internen SSD", " on the internal SSD") : ""),
                      .quelle = q, .gesperrt = g, .kachel = true);
            }
            /* da_Chris86 27.10.2026 19:38: Die Kachel (interne SSD) bleibt immer löschbar - "nur Kachel", nie die Spieldatei.
             * Gesperrt wird nur, was eine nicht erreichbare oder nicht sicher zugeordnete SPIELDATEI betrifft. */
            else if (ort[0] && kand) {                /* NEU 27.10.2026: vermutlich verschoben - Kachel: nur Kachel; vermutete Datei: gesperrt */
                MERKE(k, &p, 1, VERB(A, t, " (", x, ")", T(" - ShadowMount: Spiel vermutlich verschoben nach ", " - ShadowMount: game probably moved to "), kand,
                                     T(", nur die Kachel wird gelöscht", ", only the tile is deleted")), .kachel = true);
                for (int c = 0; c < kands.n; c++) {      /* alle Kandidaten sperren (Codex-Prüfung 14) */
                    qinfo *alt = mt_hole(&k->QuelleInfo, kands.e[c]);
                    if (kern_jb_einstieg(x) && !sl_enthaelt(&k->JB_QUELLEN, kands.e[c])) sl_add(A, &k->JB_QUELLEN, kands.e[c]);
                    if (alt) { alt->Mehrfach = true; if (kern_jb_einstieg(x)) { alt->Jb = true; alt->JbName = VERB(A, t, " (", x, ")"); } continue; }   /* schon echte Quelle einer anderen Kachel -> mehrdeutig, gesperrt */
                    sl_add(A, &k->SpielQuellen, kands.e[c]);
                    qinfo *qi = a_hole(A, sizeof *qi); qi->Name = VERB(A, t, " (", x, ")"); qi->Gesperrt = true; qi->Vermutet = true; qi->Jb = kern_jb_einstieg(x); qi->JbName = qi->Jb ? qi->Name : NULL;
                    mt_setze(A, &k->QuelleInfo, kands.e[c], qi);
                }
            }
            else if (ort[0]) {
                /* NEU 1.0.6: USB-Speicher/USB-Stick gerade nicht angesteckt -> nicht "tote Kachel", sondern Datenträger fehlt (wie bis 1.0.5 für /mnt/usb) */
                const char *wurzel = NULL;
                if (strcmp(ort, "M.2") != 0) { htreffer w = h_mt(A, q, "^(/mnt/[^/]+)/"); if (w.ok) wurzel = w.gr[1]; }
                if (wurzel && !s_existiert(k->S, wurzel))
                    MERKE(k, &p, 1, VERB(A, t, " (", x, ") - ShadowMount: ", T("Datenträger mit dem Spiel nicht gefunden (", "drive with the game not found ("), q, ")", T(", nur die Kachel wird gelöscht", ", only the tile is deleted")), .kachel = true);
                else if (strcmp(ort, "M.2") == 0 && k->M2_UNKLAR)   /* M.2 nicht erkannt: Spiel nicht erreichbar - die Kachel bleibt löschbar (nur Kachel) */
                    MERKE(k, &p, 1, VERB(A, t, " (", x, ")", T(" - ShadowMount: M.2 nicht erkannt, Spiel gerade nicht erreichbar", " - ShadowMount: M.2 not detected, game not reachable right now")), .kachel = true);
                else
                MERKE(k, &p, 1, VERB(A, t, " (", x, ")", T(" - ShadowMount: Spiel nicht mehr da, tote Kachel", " - ShadowMount: game gone, dead tile")), .kachel = true, .smTipp = true);
            }
            else {
                const char *wo = q[0] == 0 ? T("Speicherort unbekannt", "location unknown") : VERB(A, T("Spiel liegt auf ", "game is at "), q);
                /* NEU 27.10.2026 (Codex-Prüfung 11): Spiel auf einem Wechseldatenträger, der gerade fehlt -> Text sagt das; die Kachel bleibt löschbar (nur Kachel) */
                if (q[0] == '/' && h_like(q, "/mnt/*") && !s_existiert(k->S, q))
                    wo = VERB(A, T("Datenträger mit dem Spiel nicht gefunden (", "drive with the game not found ("), q, ")");
                MERKE(k, &p, 1, VERB(A, t, " (", x, ") - ShadowMount: ", wo, T(", nur die Kachel wird gelöscht", ", only the tile is deleted")), .kachel = true);
            }
            if (!zwilling) kachel_merken(k, x, VERB(A, "/user/app/", x));   /* NEU 1.0.6: ShadowMount-Kachel (jeder Fall oben merkt genau einen Eintrag) */
        }
        else {
            merke_spielteile(k, &p, 1, t, x, "/user", h_hat(&patches, x), h_hat(&addconts, x), &dlcInfos, &benutzt, zwilling);
        }
    }

    /* ---- 2 Spielstände -------------------------------------------- */
    for (int i = 0; i < homes.n; i++) {
        const char *sds[2] = { "savedata", "savedata_prospero" };
        for (int s = 0; s < 2; s++) {
            sliste xs = s_namen(k->S, VERB(A, "/user/home/", homes.e[i], "/", sds[s]), false);
            for (int j = 0; j < xs.n; j++) {
                const char *x = xs.e[j];
                if (h_hat(&k->ASTRO_IDS, x) || h_like(x, "sce_*") || h_like(x, "NPXS*")) continue;
                sliste l = L1(A, VERB(A, "/user/home/", homes.e[i], "/", sds[s], "/", x));
                MERKE(k, &l, 2, VERB(A, T("Spielstand ", "Saved data "), mit_name(k, x)), 0);
            }
        }
    }

    /* ---- 4 Bilder & Videos ----------------------------------------- */
    sliste galerieReihe = {0};
    mtab gPfade = mt_neu(true), gBilder = mt_neu(true), gVideos = mt_neu(true);
    const char *arten[2] = { "photo", "video" };
    for (int a1 = 0; a1 < 2; a1++) {
        sliste appsg = s_namen(k->S, VERB(A, "/user/av_contents/", arten[a1]), false);
        for (int i = 0; i < appsg.n; i++) {
            sliste ids = s_namen(k->S, VERB(A, "/user/av_contents/", arten[a1], "/", appsg.e[i]), false);
            for (int j = 0; j < ids.n; j++) {
                const char *id = ids.e[j];
                if (!mt_hat(&gPfade, id)) {
                    sl_add(A, &galerieReihe, id);
                    sliste *np = a_hole(A, sizeof *np); mt_setze(A, &gPfade, id, np);
                    mt_setze(A, &gBilder, id, (void *)(intptr_t)0); mt_setze(A, &gVideos, id, (void *)(intptr_t)0);
                }
                const char *pf = VERB(A, "/user/av_contents/", arten[a1], "/", appsg.e[i], "/", id);
                sl_add(A, (sliste *)mt_hole(&gPfade, id), pf);
                int n = zaehle_medien(k, pf);
                mtab *ziel = a1 == 0 ? &gBilder : &gVideos;
                mt_setze(A, ziel, id, (void *)(intptr_t)((intptr_t)mt_hole(ziel, id) + n));
            }
        }
    }
    sliste vorschauReste = {0};
    {
        sliste arts = s_namen(k->S, "/user/av_contents/thumbnails", false);
        for (int a1 = 0; a1 < arts.n; a1++) {
            sliste appsg = s_namen(k->S, VERB(A, "/user/av_contents/thumbnails/", arts.e[a1]), false);
            for (int i = 0; i < appsg.n; i++) {
                sliste ids = s_namen(k->S, VERB(A, "/user/av_contents/thumbnails/", arts.e[a1], "/", appsg.e[i]), false);
                for (int j = 0; j < ids.n; j++) {
                    const char *pf = VERB(A, "/user/av_contents/thumbnails/", arts.e[a1], "/", appsg.e[i], "/", ids.e[j]);
                    if (mt_hat(&gPfade, ids.e[j])) sl_add(A, (sliste *)mt_hole(&gPfade, ids.e[j]), pf); else sl_add(A, &vorschauReste, pf);
                }
            }
        }
    }
    for (int i = 0; i < galerieReihe.n; i++) {
        const char *id = galerieReihe.e[i];
        sliste teile = {0};
        long b = (long)(intptr_t)mt_hole(&gBilder, id), v = (long)(intptr_t)mt_hole(&gVideos, id);
        if (b > 0) sl_add(A, &teile, VERB(A, zahl(A, b), " Screenshot", b != 1 ? "s" : ""));
        if (v > 0) sl_add(A, &teile, VERB(A, zahl(A, v), " Video", v != 1 ? "s" : ""));
        const char *was = teile.n > 0 ? sl_join(A, &teile, ", ") : T("leerer Ordner", "empty folder");
        sliste *gp = mt_hole(&gPfade, id);
        MERKE(k, gp, 4, VERB(A, T("Aufnahmen von ", "Captures of "), galerie_name(k, id, gp), ": ", was), 0);
    }
    if (vorschauReste.n > 0) MERKE(k, &vorschauReste, 4, VERB(A, T("Vorschaubilder ohne Aufnahme (", "Thumbnails without capture ("), zahl(A, vorschauReste.n), T(") - Reste", ") - leftovers")), 0);
    sliste leere = {0};
    {
        sliste ids = s_namen(k->S, "/user/av_contents/content_tmp", false);
        for (int i = 0; i < ids.n; i++) {
            const char *pf = VERB(A, "/user/av_contents/content_tmp/", ids.e[i]);
            int n = s_namen(k->S, pf, false).n;
            /* NEU 1.1 (U12): Sammel-Eintrag - ein geschützter leerer Ordner (Homebrew Launcher) fällt nur
             * selbst heraus, die übrigen Reste bleiben wählbar (sonst verschwände der ganze Eintrag) */
            if (n == 0 && kern_laufende_aufnahme(pf)) continue;      /* NEU 1.1 (U16): laufende Aufnahme der PS5 - nie anbieten */
            if (n == 0 && k->SCHUTZ.aktiv) {          /* NEU 1.1 (S3): "leer" nur, wenn das Lesen wirklich klappt */
                volatile bool ok = false;
                VERSUCHE { dliste *pl = s_liste(k->S, pf, true); ok = pl == NULL || pl->n == 0; }
                FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); ok = false; } ENDE_VERSUCHE;
                if (!ok) continue;
            }
            if (n == 0) { if (!k->SCHUTZ.aktiv || kern_selbstschutz_anbieten(k, pf)) sl_add(A, &leere, pf); }
            else { sliste l = L1(A, pf); MERKE(k, &l, 4, VERB(A, T("Unfertige Aufnahme von ", "Unfinished capture of "), galerie_name(k, ids.e[i], NULL), " (", zahl(A, n), n != 1 ? T(" Dateien", " files") : T(" Datei", " file"), ")"), 0); }
        }
    }
    if (leere.n > 0) MERKE(k, &leere, 4, VERB(A, T("Leere Aufnahme-Zwischenordner (", "Empty capture temp folders ("), zahl(A, leere.n), T(") - Reste", ") - leftovers")), 0);
    {
        sliste ss = s_namen(k->S, "/user/av_contents", false);
        sliste fest = {0}; sl_add(A, &fest, "photo"); sl_add(A, &fest, "video"); sl_add(A, &fest, "thumbnails"); sl_add(A, &fest, "content_tmp");
        for (int i = 0; i < ss.n; i++) {
            const char *s = ss.e[i];
            if (h_hat(&fest, s)) continue;
            sliste xs = s_namen(k->S, VERB(A, "/user/av_contents/", s), false);
            for (int j = 0; j < xs.n; j++) { sliste l = L1(A, VERB(A, "/user/av_contents/", s, "/", xs.e[j])); MERKE(k, &l, 4, VERB(A, T("Weitere Galerie-Daten: ", "Other gallery data: "), s, "/", mit_name(k, xs.e[j])), 0); }
        }
    }

    /* ---- 3 Trophäen ------------------------------------------------ */
    for (int i = 0; i < homes.n; i++) {
        const char *u = homes.e[i];
        sliste xs = s_namen(k->S, VERB(A, "/user/home/", u, "/trophy/data"), false);
        for (int j = 0; j < xs.n; j++) {
            const char *x = xs.e[j];
            if (h_hat(&k->ASTRO_NPWRS, x)) continue;
            const char *t;
            if (h_gleich(x, "sce_trop")) t = T("Trophäen-Gesamtzähler (Level, Bronze/Silber/Gold - baut die PS5 neu auf)", "Trophy totals (level, bronze/silver/gold - rebuilt by the PS5)");
            else if (h_m(x, "^NPWR")) t = VERB(A, T("Trophäen-Fortschritt: ", "Trophy progress: "), trophaen_name(k, x), T(" (PS4-Spiel)", " (PS4 game)"), trophaen_weg(k, x));
            else t = VERB(A, T("Trophäen-Daten ", "Trophy data "), "trophy/data/", x);
            sliste l = L1(A, VERB(A, "/user/home/", u, "/trophy/data/", x));
            MERKE(k, &l, 3, t, 0);
        }
        xs = s_namen(k->S, VERB(A, "/user/home/", u, "/trophy"), false);
        for (int j = 0; j < xs.n; j++)
            if (!h_gleich(xs.e[j], "data")) { sliste l = L1(A, VERB(A, "/user/home/", u, "/trophy/", xs.e[j])); MERKE(k, &l, 3, VERB(A, T("Trophäen-Daten ", "Trophy data "), "trophy/", xs.e[j]), 0); }
        xs = s_namen(k->S, VERB(A, "/user/home/", u, "/trophy2/nobackup/data"), false);
        for (int j = 0; j < xs.n; j++) {
            const char *x = xs.e[j];
            if (h_hat(&k->ASTRO_NPWRS, x)) continue;
            const char *t = h_m(x, "^NPWR") ? VERB(A, T("Trophäen-Fortschritt: ", "Trophy progress: "), trophaen_name(k, x), trophaen_weg(k, x))
                                             : VERB(A, T("Trophäen-Daten ", "Trophy data "), "trophy2/nobackup/data/", x);
            sliste l = L1(A, VERB(A, "/user/home/", u, "/trophy2/nobackup/data/", x));
            MERKE(k, &l, 3, t, 0);
        }
        xs = s_namen(k->S, VERB(A, "/user/home/", u, "/trophy2"), false);
        for (int j = 0; j < xs.n; j++) {
            const char *x = xs.e[j];
            if (h_gleich(x, "nobackup")) {
                sliste ys = s_namen(k->S, VERB(A, "/user/home/", u, "/trophy2/nobackup"), false);
                for (int y = 0; y < ys.n; y++)
                    if (!h_gleich(ys.e[y], "data")) { sliste l = L1(A, VERB(A, "/user/home/", u, "/trophy2/nobackup/", ys.e[y])); MERKE(k, &l, 3, VERB(A, T("Trophäen-Daten ", "Trophy data "), "trophy2/nobackup/", ys.e[y]), 0); }
            }
            else { sliste l = L1(A, VERB(A, "/user/home/", u, "/trophy2/", x)); MERKE(k, &l, 3, VERB(A, T("Trophäen-Daten ", "Trophy data "), "trophy2/", x), 0); }
        }
    }
    mtab pakete = mt_neu(true);
    {
        const char *os[3] = { "/user/trophy2/nobackup/conf", "/user/trophy/conf", "/user/np_uds/nobackup/conf" };
        for (int o = 0; o < 3; o++) {
            sliste xs = s_namen(k->S, os[o], false);
            for (int j = 0; j < xs.n; j++) {
                const char *x = xs.e[j];
                if (!h_m(x, "^NPWR\\d{5}_\\d{2}$") || h_hat(&k->ASTRO_NPWRS, x)) continue;
                if (!mt_hat(&pakete, x)) { sliste *n = a_hole(A, sizeof *n); mt_setze(A, &pakete, x, n); }
                sl_add(A, (sliste *)mt_hole(&pakete, x), VERB(A, os[o], "/", x));
            }
        }
    }
    {
        sliste schl = {0}; for (int i = 0; i < pakete.n; i++) sl_add(A, &schl, pakete.e[i].k);
        if (schl.n > 1) qsort(schl.e, (size_t)schl.n, sizeof(char *), vergleich_ci_kultur);
        for (int i = 0; i < schl.n; i++) {
            const char *kk = schl.e[i];
            sliste *pk = mt_hole(&pakete, kk);
            if (mt_hat(&k->NPWR_NAMEN, kk)) {
                MERKE(k, pk, 3, VERB(A, T("Trophäen-Liste: ", "Trophy list: "), trophaen_name(k, kk), T(" - Spiel noch da, nur mit dem Spiel löschen (Namen/Bilder, nicht dein Fortschritt)", " - game still installed, delete only with the game (names/images, not your progress)")), .quelle = "", .gesperrt = false);
                if (k->MERKE_OK) ((eintrag *)k->bau.e[k->bau.n - 1])->Paket = true;
            }
            else { MERKE(k, pk, 3, VERB(A, T("Trophäen-Liste: ", "Trophy list: "), trophaen_name(k, kk), T(" - Spiel nicht mehr da, kann weg (Namen/Bilder, nicht dein Fortschritt)", " - game removed, can go (names/images, not your progress)")), 0); }
        }
    }

    /* ---- 5 Downloads, Zusatzinhalte & Reste ------------------------- */
    {
        /* NEU 1.0.6/1.1 (27.10.2026): /user/download/<ID>/download0.dat ist der dauerhafte App-Datenspeicher (/download0)
         * einer App (UFS2-Abbild, Größe aus downloadDataSize) - kein Download-Rest. Er wird NIE allein wegen seines
         * Speicherorts angeboten: nur wenn zu der Kennung keine Installation und keine Registrierung mehr zu finden ist
         * (interne SSD, M.2, USB-Erweiterung, /system_ex, Kachel-Infos, externe Kachel-Infos). Die Daten einer installierten
         * App gehen mit der App (merke_spielteile). Ist die M.2 nicht (richtig) erkannt: gesperrt.
         * Jailbreak-Einstiege (YouTube/Y2JB, Netflix): nur einzeln, die vollständige Bereinigung fragt (Anforderungen 27.10.2026).
         * Gleich wie Windows 1.0.6 (Quellcode/Suche.cs) - unabhängig vom Selbstschutz. */
        sliste dlUsb = {0};
        if (k->USB_SPEICHER) dlUsb = s_namen(k->S, USB_ERW "/user/app", true);
        sliste dlSysEx = s_namen(k->S, "/system_ex/app", true);
        sliste dlExtern = s_namen(k->S, SYS_META "/external", true);
        sliste xs = s_namen(k->S, "/user/download", false);
        for (int i = 0; i < xs.n; i++) {
            const char *x = xs.e[i];
            if (h_like(x, "NPXS*")) continue;
            if (!h_m(x, "^[A-Z]{4}\\d{5}$")) continue;   /* NEU 1.0.6: nur Ordner mit Spielnummer (wie Kachel-Reste) */
            if (h_hat(&apps, x) || h_hat(&m2Apps, x) || h_hat(&dlUsb, x) || h_hat(&dlSysEx, x) || h_hat(&appmeta, x) || h_hat(&sysmeta, x) || h_hat(&dlExtern, x)) continue;   /* App noch da - ihre Daten werden gebraucht */
            sliste l = L1(A, VERB(A, "/user/download/", x));
            const char *dt = kern_jb_einstieg(x) ? VERB(A, T("App-Daten eines Jailbreak-Einstiegs: ", "App data of a jailbreak entry: "), mit_name(k, x))
                                                 : VERB(A, T("App-Daten ohne installierte App: ", "App data without installed app: "), mit_name(k, x));
            /* da_Chris86 27.10.2026 21:39: keine pauschale Sperre bei fehlender/nicht lesbarer M.2 oder USB-Erweiterung */
            MERKE(k, &l, 5, dt, 0);
        }
    }
    for (int i = 0; i < patches.n; i++) { const char *x = patches.e[i]; if (!mt_hat(&benutzt, VERB(A, "/user/patch/", x)) && !h_like(x, "NPXS*") && h_m(x, "^[A-Z]{4}\\d{5}$")) { sliste l = L1(A, VERB(A, "/user/patch/", x)); MERKE(k, &l, 5, VERB(A, T("Update ohne Spiel: ", "Update without game: "), mit_name(k, x)), 0); } }
    for (int i = 0; i < addconts.n; i++) { const char *x = addconts.e[i]; if (!mt_hat(&benutzt, VERB(A, "/user/addcont/", x)) && !h_like(x, "NPXS*") && h_m(x, "^[A-Z]{4}\\d{5}$")) { sliste l = L1(A, VERB(A, "/user/addcont/", x)); MERKE(k, &l, 5, VERB(A, T("Zusatzinhalt: ", "Add-on: "), mit_name(k, x)), 0); } }
    {
        sliste xs = s_namen(k->S, "/user/appmeta/external/addcont", false);
        for (int i = 0; i < xs.n; i++) if (!h_like(xs.e[i], "NPXS*") && h_m(xs.e[i], "^[A-Z]{4}\\d{5}$")) { sliste l = L1(A, VERB(A, "/user/appmeta/external/addcont/", xs.e[i])); MERKE(k, &l, 5, VERB(A, T("Zusatzinhalt-Infos: ", "Add-on info: "), mit_name(k, xs.e[i])), 0); }
        xs = s_namen(k->S, "/user", false);
        for (int i = 0; i < xs.n; i++) if (h_like(xs.e[i], "_zu_loeschen_*")) { sliste l = L1(A, VERB(A, "/user/", xs.e[i])); MERKE(k, &l, 5, VERB(A, T("Rest einer früheren Aufräumaktion: ", "Leftover from an earlier cleanup: "), "/user/", xs.e[i]), 0); }
    }
    /* ---- M.2 und USB-Speicher (bis 1.0.5: eigener Bereich 7 "Inhalt der M.2-SSD") ------------------------------
     * NEU 1.0.6 (da_Chris86 28.10.2026): Spiele -> Bereich 1, Updates/Zusatzinhalte ohne Spiel und übrige Dateien -> Bereich 5,
     * ELF-Dateien -> Bereich 6, jeweils mit Ort vorne. Der USB-Speicher (erweiterter Speicher) wird genauso behandelt wie die M.2. */
    {
        dliste *usbListe = NULL;
        if (k->USB_SPEICHER) usbListe = s_liste(k->S, USB_ERW, true);
        const char *wurzel[2] = { M2, USB_ERW };
        bool da[2] = { k->m2Da, k->USB_SPEICHER && usbListe != NULL };
        dliste *liste[2] = { m2Liste, usbListe };
        sliste *eApps[2] = { &m2Apps, &usbApps }, *ePatches[2] = { &m2Patches, &usbPatches }, *eAddconts[2] = { &m2Addconts, &usbAddconts };
        for (int w = 0; w < 2; w++) {
            if (!da[w]) continue;
            const char *b = wurzel[w], *bu = VERB(A, b, "/user");
            for (int i = 0; i < liste[w]->n; i++)
                if (h_like(liste[w]->e[i].name, "_zu_loeschen_*")) { sliste l = L1(A, VERB(A, b, "/", liste[w]->e[i].name)); MERKE(k, &l, 5, VERB(A, T("Rest einer früheren Aufräumaktion: ", "Leftover from an earlier cleanup: "), b, "/", liste[w]->e[i].name), 0); }
            for (int i = 0; i < eApps[w]->n; i++) {
                const char *x = eApps[w]->e[i];
                if (h_like(x, "NPXS*") || h_hat(&k->ASTRO_IDS, x) || !h_m(x, "^[A-Z]{4}\\d{5}$")) continue;
                const char *t = app_titel(k, x);
                if (ist_astro(k, t)) { astro_merken(k, x); continue; }
                if (t[0] == 0) t = T("unbekannter Name", "unknown name");
                sliste p = L1(A, VERB(A, bu, "/app/", x));
                /* eine weitere SICHTBARE Kopie (intern, M.2, USB) braucht dieselbe Kachel - dann bleiben die Kachel-Infos */
                bool zwilling = h_hat(&apps, x) || h_hat(eApps[1 - w], x);
                if (!zwilling) {
                    if (h_hat(&appmeta, x)) { sl_add(A, &p, VERB(A, "/user/appmeta/", x)); mt_setze(A, &benutzt, VERB(A, "/user/appmeta/", x), (void *)1); }
                    p = mit_kachel(k, &p, x, &sysmeta);
                }
                merke_spielteile(k, &p, 1, t, x, bu, h_hat(ePatches[w], x), h_hat(eAddconts[w], x), &dlcInfos, &benutzt, zwilling);
            }
            for (int i = 0; i < ePatches[w]->n; i++) { const char *x = ePatches[w]->e[i]; if (!mt_hat(&benutzt, VERB(A, bu, "/patch/", x)) && !h_like(x, "NPXS*") && h_m(x, "^[A-Z]{4}\\d{5}$")) { sliste l = L1(A, VERB(A, bu, "/patch/", x)); MERKE(k, &l, 5, VERB(A, T("Update ohne Spiel: ", "Update without game: "), mit_name(k, x)), 0); } }
            for (int i = 0; i < eAddconts[w]->n; i++) { const char *x = eAddconts[w]->e[i]; if (!mt_hat(&benutzt, VERB(A, bu, "/addcont/", x)) && !h_like(x, "NPXS*") && h_m(x, "^[A-Z]{4}\\d{5}$")) { sliste l = L1(A, VERB(A, bu, "/addcont/", x)); MERKE(k, &l, 5, VERB(A, T("Zusatzinhalte ohne Spiel: ", "Add-ons without game: "), mit_name(k, x)), 0); } }
            for (int i = 0; i < liste[w]->n; i++) {
                const char *x = liste[w]->e[i].name;
                if (h_gleich(x, "lost+found") || h_gleich(x, "system_data") || h_gleich(x, "user") || h_like(x, "_zu_loeschen_*")) continue;
                merke_ext_ordner(k, b, &liste[w]->e[i]);
            }
        }
    }
    /* ---- normale USB-Sticks (NEU 1.0.6, da_Chris86 28.10.2026): anzeigen, nur einzeln löschbar ---------------------
     * Nur die oberste Ebene (Ordner als Ganzes); Ordner mit ShadowMount-Spielen oder PS5 Cleanup werden aufgeklappt.
     * Nie in der vollständigen Bereinigung und nie über "ganzen Bereich wählen". */
    k->STICK_DA = false;
    for (int u = 0; u < 8; u++) {
        const char *b = a_fmt(A, "/mnt/usb%d", u);
        dliste *l = NULL;
        VERSUCHE { l = s_liste(k->S, b, false); }
        FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); l = NULL; } ENDE_VERSUCHE;
        if (!l) continue;
        k->STICK_DA = true;
        /* Verwaltungsordner von Windows/macOS auf dem Stick nicht anbieten */
        int nn = 0; datei *ee = a_hole(A, sizeof(datei) * (size_t)(l->n > 0 ? l->n : 1));
        for (int i = 0; i < l->n; i++) if (!h_m(l->e[i].name, "(?i)^(System Volume Information|\\$RECYCLE\\.BIN|\\.Trashes|\\.Spotlight-V100|\\.fseventsd)$")) ee[nn++] = l->e[i];
        /* NEU 1.0.6 (Codex-Prüfung 27): ein Lesefehler auf dem Stick bricht nicht die ganze Suche ab - dieser Stick
         * wird dann gar nicht angezeigt (nichts davon angeboten), die übrige Suche läuft weiter. */
        k->MERKE_STICK = true;
        int vorher = k->bau.n;
        volatile bool kaputt = false;
        VERSUCHE { for (int i = 0; i < nn; i++) merke_ordner(k, b, &ee[i], 1, ext_kat(ee[i].name), ""); }
        FANGE { k->MERKE_STICK = false; if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); kaputt = true; } ENDE_VERSUCHE;
        k->MERKE_STICK = false;
        if (kaputt) k->bau.n = vorher;
    }
    for (int i = 0; i < appmeta.n; i++) {
        const char *x = appmeta.e[i];
        if (h_hat(&k->ASTRO_IDS, x) || h_gleich(x, "external") || h_gleich(x, "addcont") || h_like(x, "NPXS*")) continue;
        /* NEU 1.0.6 (Protokoll eines Nutzers 28.10.2026, FW 13.60): nur Ordner mit Spielnummer (z. B. CUSA06014) - Sony-Ordner
         * wie "push_resource" sind kein Spiel (1.0.5 bot sie an, der Sicherheitsstopp verhinderte das Löschen) */
        if (!h_m(x, "^[A-Z]{4}\\d{5}$")) continue;
        if (h_hat(&apps, x) || mt_hat(&benutzt, VERB(A, "/user/appmeta/", x)) || h_hat(&usbApps, x)) continue;
        const char *t = app_titel(k, x);
        if (ist_astro(k, t)) { astro_merken(k, x); continue; }
        if (t[0] == 0) t = T("entferntes Spiel", "removed game");
        sliste l = L1(A, VERB(A, "/user/appmeta/", x));
        sliste mk = mit_kachel(k, &l, x, &sysmeta);
        MERKE(k, &mk, 5, VERB(A, T("Reste: ", "Leftovers: "), t, " (", x, ")"), 0);
        /* NEU 1.0.6 (PS5-Test da_Chris86 29.10.2026 19:07, Spectrum Library SLIB00001): Kachel-Bilder ohne App-Ordner gehören
         * fast immer zu einer Kachel, die noch im Menü steht (z. B. Homebrew, das sein Programm woanders ablegt). Sony kann sie
         * ohne App-Ordner nicht austragen - darum wie "Leere Kachel": TEILWEISE melden und am Ende "Datenbank neu aufbauen" mit Namen */
        kachel_merken(k, x, NULL);
    }
    {
        sliste shareIds = {0};
        sliste xs = s_namen(k->S, "/user/system/share", false);
        for (int i = 0; i < xs.n; i++) {
            const char *x = xs.e[i];
            if (!h_m(x, "^[A-Z]{4}\\d{5}$") || h_like(x, "NPXS*") || h_hat(&k->ASTRO_IDS, x)) continue;
            if (h_hat(&apps, x) || h_hat(&m2Apps, x) || h_hat(&usbApps, x)) continue;
            sl_add(A, &shareIds, x);
        }
        if (shareIds.n > 0) {
            sliste l = {0}; for (int i = 0; i < shareIds.n; i++) sl_add(A, &l, VERB(A, "/user/system/share/", shareIds.e[i]));
            MERKE(k, &l, 5, VERB(A, T("Share-Einstellungen alter Spiele (", "Share settings of old games ("), zahl(A, shareIds.n), ")"), 0);
        }
    }
    {
        sliste cache = {0};
        const char *us[2] = { "conceptmeta", "appmeta" };
        for (int u = 0; u < 2; u++) { sliste xs = s_namen(k->S, VERB(A, "/user/catalog_downloader/", us[u]), false); for (int i = 0; i < xs.n; i++) sl_add(A, &cache, VERB(A, "/user/catalog_downloader/", us[u], "/", xs.e[i])); }
        if (cache.n > 0) MERKE(k, &cache, 5, VERB(A, T("Store-Bildercache (", "Store image cache ("), zahl(A, cache.n), T(", lädt die PS5 neu)", ", re-downloaded by the PS5)")), 0);
    }
    {
        sliste tmp = {0};
        const char *os[5] = { "/user/bgft/trash", "/user/savedata/temp", "/user/temp/gls/screenshot", "/user/temp/video_recording", "/user/temp/common_temp/telemetry" };
        for (int o = 0; o < 5; o++) { sliste xs = s_namen(k->S, os[o], false); for (int i = 0; i < xs.n; i++) sl_add(A, &tmp, VERB(A, os[o], "/", xs.e[i])); }
        if (tmp.n > 0) MERKE(k, &tmp, 5, VERB(A, T("Temporäre Dateien (", "Temporary files ("), zahl(A, tmp.n), ")"), 0);
    }
    {
        sliste fehler = {0};
        sliste xs = s_namen(k->S, "/system_data/priv/error/history", false);
        for (int i = 0; i < xs.n; i++) if (h_like(xs.e[i], "*.json")) sl_add(A, &fehler, VERB(A, "/system_data/priv/error/history/", xs.e[i]));
        if (fehler.n > 0) MERKE(k, &fehler, 5, VERB(A, T("Fehlerberichte (", "Error reports ("), zahl(A, fehler.n), ")"), 0);
    }
    {
        sliste ext = {0};
        sliste xs = s_namen(k->S, SYS_META "/external", false);
        for (int i = 0; i < xs.n; i++) if (h_m(xs.e[i], "^[A-Z]{4}\\d{5}$") && !h_like(xs.e[i], "NPXS*")) sl_add(A, &ext, VERB(A, SYS_META "/external/", xs.e[i]));
        if (ext.n > 0) MERKE(k, &ext, 5, VERB(A, T("Infos zu Spielen auf USB-Platten (", "Info on games on USB drives ("), zahl(A, ext.n), T(", legt die PS5 neu an)", ", recreated by the PS5)")), 0);
    }
    for (int i = 0; i < sysmeta.n; i++) {
        const char *x = sysmeta.e[i];
        if (!h_m(x, "^[A-Z]{4}\\d{5}$") || h_like(x, "NPXS*") || h_hat(&k->ASTRO_IDS, x)) continue;
        if (h_hat(&apps, x) || h_hat(&appmeta, x) || h_hat(&m2Apps, x) || h_hat(&usbApps, x)) continue;
        const char *t = app_titel(k, x);
        if (ist_astro(k, t)) { astro_merken(k, x); continue; }
        if (t[0] == 0) t = T("unbekannter Name", "unknown name");
        sliste l = L1(A, VERB(A, SYS_META "/", x));
        MERKE(k, &l, 5, VERB(A, T("Leere Kachel (Schloss): ", "Empty tile (lock icon): "), t, " (", x, ")"), .kachel = true);
        kachel_merken(k, x, NULL);   /* Codex-Prüfung 32: Kachel ohne App-Ordner - bleibt im Menü, wird als TEILWEISE gemeldet */
    }
    for (int i = 0; i < dlcInfos.n; i++) { const char *x = dlcInfos.e[i]; if (!mt_hat(&benutzt, VERB(A, "/user/appmeta/addcont/", x)) && !h_like(x, "NPXS*") && h_m(x, "^[A-Z]{4}\\d{5}$")) {   /* NPXS: Sony (Anforderungen 27.10.2026) */ sliste l = L1(A, VERB(A, "/user/appmeta/addcont/", x)); MERKE(k, &l, 5, VERB(A, T("Zusatzinhalt-Infos: ", "Add-on info: "), mit_name(k, x)), 0); } }

    /* ---- 6 Jailbreak-Tools & Homebrew-Daten ------------------------- */
    {
        dliste *dl = s_liste(k->S, "/user/data", true);
        merke_ordner(k, "/user/data", dl ? dl->e : NULL, dl ? dl->n : 0, 6, "");
    }
    {
        sliste fakeLiz = {0};
        /* NEU 1.0.6 (Codex-Prüfung 31): nur Dateien, keine Ordner */
        dliste *ll = s_liste(k->S, "/user/license", true);
        for (int i = 0; ll && i < ll->n; i++) if (!ll->e[i].ist_ordner && h_m(ll->e[i].name, "^fake.+\\.(rif|idx)$")) sl_add(A, &fakeLiz, ll->e[i].name);
        sliste gruppen = {0};
        for (int i = 0; i < fakeLiz.n; i++) { const char *g = h_r(A, fakeLiz.e[i], "\\.(rif|idx)$", ""); if (!h_hat(&gruppen, g)) sl_add(A, &gruppen, g); }
        for (int gi = 0; gi < gruppen.n; gi++) {
            const char *g = gruppen.e[gi];
            sliste pf = {0};
            for (int i = 0; i < fakeLiz.n; i++) if (h_gleich(fakeLiz.e[i], VERB(A, g, ".rif")) || h_gleich(fakeLiz.e[i], VERB(A, g, ".idx"))) sl_add(A, &pf, VERB(A, "/user/license/", fakeLiz.e[i]));
            htreffer tm = h_mt(A, g, "([A-Z]{4}\\d{5})");
            bool weiter = false;
            if (tm.ok) {
                const char *tid = tm.gr[1];
                pliste spiele = {0};
                for (int i = 0; i < k->bau.n; i++) { eintrag *f = k->bau.e[i]; if (ist_teil(f, "Spiel") && h_like(f->Gruppe, VERB(A, "*|", tid))) pl_add(A, &spiele, f); }
                /* NEU 1.1 (Codex-Prüfung 26.10.2026): Die Lizenz geht nur mit, wenn es keine weitere Kopie des Spiels gibt
                 * (intern, M.2, USB-Erweiterung) - und bei nicht erkannter M.2 nicht mit einem internen Spiel. */
                bool extern_kopie = false;
                if (spiele.n == 1) {   /* 27.10.2026: auch ohne Selbstschutz, gleich wie Windows 1.0.6 */
                    int kopien = (h_hat(&apps, tid) ? 1 : 0) + (h_hat(&m2Apps, tid) ? 1 : 0) + (h_hat(&usbApps, tid) ? 1 : 0);
                    extern_kopie = kopien > 1;   /* nur eine SICHTBARE Kopie (da_Chris86 27.10.2026 21:39) */
                }
                if (spiele.n == 1 && extern_kopie) weiter = true;   /* Lizenz bleibt - eine andere Kopie braucht sie evtl. */
                else if (spiele.n == 1) {
                    eintrag *f = spiele.e[0];
                    sliste neu = sl_kopie(A, &f->Pfade); sl_add_alle(A, &neu, &pf);
                    f->Pfade = neu; f->PfadeBasis = sl_kopie(A, &f->Pfade);
                    weiter = true;
                }
                else if (spiele.n > 1 || h_hat(&apps, tid) || h_hat(&m2Apps, tid) || h_hat(&usbApps, tid)) weiter = true;
            }
            if (weiter) continue;
            const char *n = h_r(A, g, "^fake", "");
            if (tm.ok) { const char *mn = mit_name(k, tm.gr[1]); if (strcmp(mn, tm.gr[1]) != 0) n = VERB(A, n, " - ", h_r(A, mn, "^[A-Z]{4}\\d{5} ", "")); }
            MERKE(k, &pf, 6, VERB(A, T("Fake-Lizenz: ", "Fake license: "), n, T(" (Spiel nicht mehr installiert)", " (game no longer installed)")), 0);
        }
    }
    for (int i = 0; i < homes.n; i++)
        if (s_namen(k->S, VERB(A, "/user/home/", homes.e[i], "/webkit/shell/local"), false).n > 0) {
            sliste l = L1(A, VERB(A, "/user/home/", homes.e[i], "/webkit/shell/local"));
            const char *bt = T("Browserspeicher (Jailbreak-Seiten, evtl. mit offline gespeichertem Jailbreak)", "Browser storage (jailbreak pages, may contain an offline-saved jailbreak page)");
            MERKE(k, &l, 6, bt, 0);
            /* da_Chris86 29.10.2026: kann einen offline gespeicherten WebKit-Jailbreak enthalten - Jailbreak-Einstieg (nur einzeln,
             * die vollständige Bereinigung fragt; bis 29.10.2026 wie 1.0.5 auch in der vollständigen Bereinigung) */
            eintrag *b = (k->MERKE_OK && k->bau.n > 0) ? k->bau.e[k->bau.n - 1] : NULL;
            if (b && !b->Gesperrt) {
                b->Jailbreak = true; b->NurEinzeln = true;
                b->Text = b->TextBasis = VERB(A, b->Text, kern_nur_einzeln_text());
            }
        }
    /* NEU 1.0.6 (da_Chris86 29.10.2026 19:12): Blu-ray-Jailbreak-Disc im Laufwerk - nur lesen. Eine Jailbreak-Disc hat neben
     * BDMV eigene .jar/.elf-Dateien im obersten Ordner (BD-JB-Autoloader: poops.jar, autoloader.jar, ps5-unified-autoloader.elf) */
    {
        volatile bool bd = false;
        VERSUCHE {
            sliste d = s_namen(k->S, "/mnt/disc", false); bool bdmv = false, eig = false;
            for (int i = 0; i < d.n; i++) { if (h_gleich(d.e[i], "BDMV")) bdmv = true; else if (h_m(d.e[i], "(?i)\\.(jar|elf)$")) eig = true; }
            bd = bdmv && eig;
        } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); bd = false; } ENDE_VERSUCHE;
        k->BD_DISC = bd;
    }

    /* NEU 1.0.6/1.1: Jailbreak-Einstiege (App, Update, Zusatzinhalt, Kachel-Infos, App-Daten) - Anforderungen 27.10.2026:
     * einzeln löschbar (nicht über "ganzen Bereich wählen"), die vollständige Bereinigung fragt, ob sie bleiben. */
    /* NEU 27.10.2026 (Codex-Prüfung 14): nicht sicher zugeordnete Spieldateien merken - der Löschcode fasst sie nie an, auch nicht über einen Elternordner */
    /* da_Chris86 28.10.2026 13:41: Spiel-Abbilder sind als eigene Spiele löschbar - SperrQuellen bleibt leer (der Löschcode prüft sie weiter) */
    /* NEU 27.10.2026 (Codex-Prüfung 11): zeigen mehrere Kacheln auf dieselbe Spieldatei, geht bei "Spiel + Kachel" nur die Kachel */
    for (int i = 0; i < k->bau.n; i++) {
        eintrag *f = k->bau.e[i];
        if (f->Kat != 1 || !f->Quelle || !f->Quelle[0]) continue;
        qinfo *qi = mt_hole(&k->QuelleInfo, f->Quelle);
        if (!qi || !qi->Mehrfach) continue;
        f->Quelle = "";
        /* NEU 1.0.6 (Codex-Prüfung 27): ohne Spieldatei zählt der Ort der übrigen Pfade (meist intern) - nicht der Ort des Abbilds */
        int alt = f->Ort, neu = ORT_INTERN;
        for (int j = 0; j < f->Pfade.n && neu == ORT_INTERN; j++) neu = kern_pfad_ort(f->Pfade.e[j]);
        if (neu != alt) {
            const char *v = kern_ort_vorsatz(alt);
            const char *rest = txt_beginnt(f->Text, v) ? f->Text + strlen(v) : f->Text;
            f->Text = VERB(A, kern_ort_vorsatz(neu), rest);
            f->Ort = neu;
            if (alt == ORT_STICK && neu != ORT_STICK) f->NurEinzeln = false;
        }
        f->Text = f->TextBasis = VERB(A, f->Text, T(" - die Spieldatei gehört zu mehreren Kacheln, nur die Kachel wird gelöscht", " - the game file belongs to several tiles, only the tile is deleted"));
    }
    pliste jbNeu = {0};
    for (int i = 0; i < k->bau.n; i++) {
        eintrag *f = k->bau.e[i];
        if (f->Gesperrt) continue;
        bool jb = false;
        bool abbild = f->Quelle && f->Quelle[0];
        for (int j = 0; j < f->Pfade.n && !jb; j++) if (jb_pfad(f->Pfade.e[j])) jb = true;
        for (int j = 0; j < f->Pfade.n; j++) if (ist_abbild(f->Pfade.e[j])) abbild = true;
        if (!jb) continue;
        /* Sammeleinträge (mehrere Titel): nur die Pfade des Einstiegs herausnehmen, der Rest bleibt wählbar */
        sliste rest = {0}, ids = {0}, jbp = {0}, jbids = {0};
        for (int j = 0; j < f->Pfade.n; j++) {
            if (!jb_pfad(f->Pfade.e[j])) sl_add(A, &rest, f->Pfade.e[j]);
            else { sl_add(A, &jbp, f->Pfade.e[j]); htreffer tj = h_mt(A, f->Pfade.e[j], "([A-Z]{4}\\d{5})"); if (tj.ok && !sl_enthaelt(&jbids, tj.gr[1])) sl_add(A, &jbids, tj.gr[1]); }
            htreffer t = h_mt(A, f->Pfade.e[j], "([A-Z]{4}\\d{5})");
            const char *id = t.ok ? t.gr[1] : "";
            if (!sl_enthaelt(&ids, id)) sl_add(A, &ids, id);
        }
        if (rest.n > 0 && ids.n > 1) {
            f->Pfade = rest; f->PfadeBasis = sl_kopie(A, &rest);
            /* Anforderungen 27.10.2026 (Codex-Prüfung 9): der herausgenommene Teil bleibt sichtbar - als eigener Jailbreak-Eintrag */
            eintrag *n = a_hole(A, sizeof *n);
            n->Pfade = jbp; n->PfadeBasis = sl_kopie(A, &jbp); n->Kat = 6; n->Quelle = ""; n->Kachel = f->Kachel;
            n->Jailbreak = true; n->NurEinzeln = true; n->Ort = f->Ort;
            n->Text = n->TextBasis = VERB(A, T("Jailbreak-Einstieg ", "Jailbreak entry "), sl_join(A, &jbids, ", "), " (", h_r(A, f->TextBasis, "\\s*\\(.*$", ""), ")", kern_nur_einzeln_text());
            pl_add(A, &jbNeu, n);
            continue;
        }
        f->Jailbreak = true; f->NurEinzeln = true;
        /* da_Chris86 28.10.2026 22:55: Jailbreak-Einstiege stehen unter "Jailbreak & Homebrew". da_Chris86 29.10.2026: ein ShadowMount-Spiel
         * (mit Spieldatei) bleibt unter "Spiele" - dort ist das Löschen von Spieldatei und Kachel geregelt; geschützt und gewarnt wird es trotzdem */
        if (!(f->Kat == 1 && abbild)) f->Kat = 6;   /* Codex-Prüfung 36: auch ein Spiel-Abbild ohne Kachel bleibt unter "Spiele" */
        f->Text = f->TextBasis = VERB(A, f->Text, kern_nur_einzeln_text());
    }
    for (int i = 0; i < jbNeu.n; i++) pl_add(A, &k->bau, jbNeu.e[i]);
    /* fest nach Bereich ordnen, darin nach Ort (intern, M.2, USB, USB-Stick - NEU 1.0.6); sonst bleibt die Fundreihenfolge */
    pliste sortiert = {0};
    for (int ki = 0; ki < KAT_ANZ; ki++) for (int o = ORT_INTERN; o <= ORT_STICK; o++) for (int i = 0; i < k->bau.n; i++) {
        eintrag *f = k->bau.e[i];
        if (f->Kat == ALLE_KAT[ki] && f->Ort == o) pl_add(A, &sortiert, f);
    }
    tool_daten_ordnen(k, &sortiert);
    k->Funde = sortiert;
    k->FUNDE_GUELTIG = true;
    k->funde_verbindung_nr = k->verbindung_nr;  /* FUNDE_IP = S.Ftp.Ip + ":" + S.Ftp.Port (Suche.cs:765) - fehlte bis 25.10.2026, siehe ABWEICHUNGEN A10 */
}

