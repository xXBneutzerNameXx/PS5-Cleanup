/* PS5 Cleanup 1.1 - gemeinsamer C-Kern
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

/* NEU 1.0.7: Anzeige-Reihenfolge - Apps direkt nach Spielen, Jailbreak zuletzt */
static const int ALLE_KAT_107[KAT_ANZ] = { 1, 6, 2, 3, 4, 5, 7 };
static const int ALLE_KAT_106[KAT_ANZ] = { 1, 2, 3, 4, 5, 6, 7 };
/* Nur für die PC-Tests (Linux): PS5C_BEREICHE_106=1 hält die Einteilung von 1.0.6 (6 Bereiche) - damit vergleichen die
 * Bedienablauf- und Fundlisten-Tests weiter Eintrag für Eintrag mit 1.0.5 (gleiche Funde, gleiches Löschen, nur andere
 * Überschriften). Auf der PS5 (kein __linux__) gibt es den Schalter nicht. */
bool kern_bereiche_106(void)
{
#ifdef __linux__
    static int v = -1;
    if (v < 0) { const char *e = getenv("PS5C_BEREICHE_106"); v = (e && e[0] == '1') ? 1 : 0; }
    return v == 1;
#else
    return false;
#endif
}
const int *kern_alle_kat(void) { return kern_bereiche_106() ? ALLE_KAT_106 : ALLE_KAT_107; }

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
    /* da_Chris86 03.11.2026 06:28: erweiterter Speicher (USB-Festplatte für Spiele) und USB-Stick (Dateien) klar trennen */
    switch (ort) { case ORT_M2: return "M.2 – "; case ORT_USB: return kern_bereiche_106() ? "USB – " : T("Erweiterter Speicher – ", "Extended storage – ");
                   case ORT_STICK: return kern_bereiche_106() ? T("USB-Stick – ", "USB stick – ") : T("USB-Laufwerk – ", "USB drive – "); }   /* da_Chris86 03.11.2026 16:15: auch Festplatten */
    return "";
}
const char *kern_ort_name(int ort)
{
    switch (ort) { case ORT_M2: return T("M.2-SSD", "M.2 SSD");
                   case ORT_USB: return kern_bereiche_106() ? T("USB-Speicher", "USB storage") : T("erweiterter Speicher (USB-Festplatte)", "extended storage (USB drive)");
                   case ORT_STICK: return kern_bereiche_106() ? T("USB-Stick", "USB stick") : T("USB-Laufwerk", "USB drive"); }
    return T("interne SSD", "internal SSD");
}

const char *kern_kat_name(int k)
{
    if (kern_bereiche_106()) switch (k) {   /* Namen bis 1.0.6 (nur PC-Vergleichstests) */
    case 1: return T("Spiele & Apps", "Games & apps");
    case 5: return T("Reste", "Leftovers");
    case 6: return T("Jailbreak & Homebrew", "Jailbreak & homebrew");
    }
    switch (k) {
    case 1: return T("Spiele", "Games");
    case 2: return T("Spielstände", "Saved data");
    case 3: return T("Trophäen", "Trophies");
    case 4: return T("Bilder & Videos", "Screenshots & videos");
    case 5: return T("Sonstiges", "Other");
    case 6: return T("Apps", "Apps");
    case 7: return T("Jailbreak", "Jailbreak");
    }
    return "";
}
const char *kern_kat_info(int k)
{
    if (kern_bereiche_106()) switch (k) {   /* Texte bis 1.0.6 (nur PC-Vergleichstests) */
    case 1: return T("Spiele und Apps auf interner SSD, M.2 und USB - der Ort steht vorne. Wer ein Spiel wählt, nimmt Update und DLCs mit.", "Games and apps on the internal SSD, M.2 and USB - the location comes first. Selecting a game takes its update and DLC with it.");
    case 5: return T("Downloads, Zusatzinhalte und Updates ohne Spiel, Reste gelöschter Spiele, Fehlerberichte und übrige Dateien auf M.2 und USB.", "Downloads, add-ons and updates without a game, leftovers of deleted games, error reports and other files on M.2 and USB.");
    case 6: return T("Kacheln und Daten der Jailbreak-Tools, Homebrew-Daten und ELF-Dateien aller Speicher. Danach Jailbreak neu einrichten.", "Tiles and data of jailbreak tools, homebrew data and ELF files on all storage. Set up your jailbreak again afterwards.");
    }
    switch (k) {
    case 1: return T("Spiele samt Update und Zusatzinhalten - intern, auf der M.2 und auf dem erweiterten Speicher (USB-Festplatte). Der Ort steht vorne.", "Games with their update and add-ons - internal, on the M.2 and on the extended storage (USB drive). The location comes first.");
    case 2: return T("Spielstände auf der Konsole. Ohne Cloud-Sicherung (PS Plus) endgültig weg.", "Saved data on the console. Without a cloud backup (PS Plus) it is gone for good.");
    case 3: return T("Trophäen-Fortschritt und Trophäen-Listen. Nie mit dem PSN abgeglichene Trophäen sind endgültig weg.", "Trophy progress and trophy lists. Trophies never synced with PSN are gone for good.");
    case 4: return T("Screenshots und Videoclips, je Spiel mit Anzahl.", "Screenshots and video clips, per game with count.");
    case 5: return T("Was keinem Spiel und keiner App mehr gehört oder wo nicht sicher ist, wem es gehört: Updates und Zusatzinhalte ohne Spiel, Reste gelöschter Spiele, Fehlerberichte, temporäre Dateien, unbekannte Ordner.", "Whatever no longer belongs to a game or app, or where it is not certain who it belongs to: updates and add-ons without a game, leftovers of deleted games, error reports, temporary files, unknown folders.");
    case 6: return T("Normale Apps und Homebrew-Tools - Kachel und Daten zusammen, dazu ELF-Dateien aller Speicher.", "Regular apps and homebrew tools - tile and data together, plus ELF files on all storage.");
    case 7: return T("Nur, was dein Jailbreak zum Starten braucht. Nur einzeln wählbar, nie bei der vollständigen Bereinigung.", "Only what your jailbreak needs to start. Single selection only, never part of the full cleanup.");
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
const char *kern_sfo_wert(arena *a, const mtab *h, const char *schluessel) { return mt_hat(h, schluessel) ? str_sfo(a, mt_hole(h, schluessel)) : ""; }   /* NEU 1.0.7 (kern_ps4.c) */
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

typedef struct { const char *T; const char *Roh; int Art; } titelinfo;
/* NEU 1.0.7: Spiel oder App? PS5 param.json "applicationCategoryType": 0 = Spiel, 65536/65792 = Medien-App;
 * PS4 param.sfo CATEGORY: "gd" = Spiel, "gdc"/"gde"/"gdk" = App (PS5/PS4 Dev Wiki, 31.10.2026). Alles andere: unbekannt -> bleibt Spiel. */
static int art_json(kern *k, const char *t)
{
    htreffer m = h_mt(A, t, "\"applicationCategoryType\"\\s*:\\s*(\\d+)");
    if (!m.ok) return 0;
    long v = strtol(m.gr[1], NULL, 10);
    return v == 0 ? 1 : (v == 65536 || v == 65792) ? 2 : 0;
}
static int art_sfo(const mtab *h)
{
    const sfowert *w = mt_hole(h, "CATEGORY");
    if (!w || w->zahl || !w->s) return 0;
    if (!strcmp(w->s, "gd")) return 1;
    return (!strcmp(w->s, "gdc") || !strcmp(w->s, "gde") || !strcmp(w->s, "gdk")) ? 2 : 0;
}
static const char *app_titel_lesen(kern *k, const char *id)
{
    k->APP_ROH = ""; k->APP_ART = 0;
    const char *jd[3] = { VERB(A, "/user/appmeta/", id, "/param.json"), VERB(A, "/user/app/", id, "/sce_sys/param.json"), VERB(A, SYS_META "/", id, "/param.json") };
    for (int i = 0; i < 3; i++) {
        const char *t = h_ohne_bom(s_text(k->S, jd[i]));
        if (!h_m(t, "titleName")) continue;
        k->APP_ROH = t; k->APP_ART = art_json(k, t);
        const char *n = kern_json_titel(A, t);
        if (n[0]) return n;
    }
    /* PS4-Spiele haben statt param.json eine param.sfo */
    const char *sd[3] = { VERB(A, "/user/appmeta/", id, "/param.sfo"), VERB(A, "/user/app/", id, "/sce_sys/param.sfo"), VERB(A, SYS_META "/", id, "/param.sfo") };
    for (int i = 0; i < 3; i++) {
        mtab h = sfo_von(k, sd[i]);
        const char *n = kern_sfo_titel(A, &h);
        if (n[0]) { k->APP_ROH = n; k->APP_ART = art_sfo(&h); return n; }
    }
    return "";
}
static const char *app_titel(kern *k, const char *id)            /* AppTitel */
{
    titelinfo *c = mt_hole(&k->TITEL_CACHE, id);
    if (c) { k->APP_ROH = c->Roh; k->APP_ART = c->Art; return c->T; }
    const char *t = app_titel_lesen(k, id);
    c = a_hole(A, sizeof *c); c->T = t; c->Roh = k->APP_ROH; c->Art = k->APP_ART;
    mt_setze(A, &k->TITEL_CACHE, id, c);
    return t;
}
/* MitName: hängt bei Ordnern, die nach einer Spiel-ID benannt sind, den Namen an.
 * NEU 1.0.7 (PS5-Test 02.11.2026, da_Chris86: "Wenn das Spiel nicht mehr da ist, dann ist alles ... nur noch mit der ID gekennzeichnet"):
 * auch Reste deinstallierter Spiele und Apps bekommen ihren Namen - aber nur aus Metadaten (name_belegt). Gibt es keinen Beleg, steht
 * ausdrücklich "Spiel/App unbekannt" da. art: 0 eigene Angaben, 1 gehört laut Metadaten zu einem installierten Spiel, 2 nicht mehr
 * installiert - Name aus verbliebenen Angaben, 3 unbekannt. */
static const char *name_belegt(kern *k, const char *id, int *art);
static const char *name_belegt_spst(kern *k, const char *id, int *art);
static const char *mit_name_art(kern *k, const char *x, bool spst)
{
    htreffer m = h_mt(A, x, "^([A-Z]{4}\\d{5})");
    if (!m.ok) return x;
    int art = 3;
    const char *t = spst ? name_belegt_spst(k, m.gr[1], &art) : name_belegt(k, m.gr[1], &art);
    if (art <= 1) return VERB(A, x, " (", t, ")");
    if (kern_bereiche_106()) return x;   /* Vergleichstests mit 1.0.5/1.0.6 unverändert */
    if (art == 2) return VERB(A, x, " (", t, T(", nicht mehr auf der Konsole)", ", no longer on the console)"));
    return VERB(A, x, " (", T("Spiel/App unbekannt", "game/app unknown"), ")");
}
static const char *mit_name(kern *k, const char *x) { return mit_name_art(k, x, false); }
static const char *mit_name_spst(kern *k, const char *x) { return mit_name_art(k, x, true); }   /* Spielstände: auch INSTALL_DIR_SAVEDATA */
/* wie mit_name, aber ohne den Zusatz "nicht mehr auf der Konsole" (für Texte, die das schon selbst sagen) */
static const char *mit_name_kurz(kern *k, const char *x)
{
    htreffer m = h_mt(A, x, "^([A-Z]{4}\\d{5})");
    if (!m.ok || kern_bereiche_106()) return mit_name(k, x);
    int art = 3;
    const char *t = name_belegt(k, m.gr[1], &art);
    return art <= 2 ? VERB(A, x, " (", t, ")") : VERB(A, x, " (", T("Spiel/App unbekannt", "game/app unknown"), ")");
}
static const char *trophaen_name(kern *k, const char *npwr)
{
    const char *n = mt_hole(&k->NPWR_NAMEN, npwr);
    if (mt_hat(&k->NPWR_NAMEN, npwr) && n && n[0]) return VERB(A, n, " (", npwr, ")");
    if (mt_hat(&k->NPWR_GALERIE, npwr)) return VERB(A, (const char *)mt_hole(&k->NPWR_GALERIE, npwr), " (", npwr, ")");
    if (kern_bereiche_106()) return npwr;
    return VERB(A, npwr, " (", T("Spiel/App unbekannt", "game/app unknown"), ")");   /* NEU 1.0.7: unbekannte Zuordnung ausdrücklich nennen */
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
/* Spielname aus der Begleitdatei (.dat) einer Aufnahme - NEU 1.0.7 aus galerie_name herausgelöst (auch für Spielstände, Lizenzen ...).
 * Sony schreibt beim Aufnehmen den Spielnamen hinein - ein Beleg aus Metadaten. "" = keiner. */
static const char *galerie_dat_name(kern *k, const char *id, const sliste *ordner)
{
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
    return g ? g : "";
}
/* GalerieName: erst die Kachel-Infos, sonst die Begleitdatei (.dat) einer Aufnahme */
static const char *galerie_name(kern *k, const char *id, const sliste *ordner)
{
    if (h_like(id, "NPXS*")) return VERB(A, T("Systemmenü (", "System menu ("), id, ")");
    if (!kern_bereiche_106()) {   /* NEU 1.0.7 (PS5-Test 02.11.2026): Name und Zuordnung nur aus Metadaten (name_belegt) */
        int art = 3;
        const char *t = name_belegt(k, id, &art);
        if (art <= 1) return VERB(A, t, " (", id, ")");
        if (art == 2) return VERB(A, t, " (", id, T(", Spiel nicht mehr auf der Konsole)", ", game no longer on the console)"));
        return VERB(A, id, T(" (Spiel nicht mehr auf der Konsole, Spiel/App unbekannt)", " (game no longer on the console, game/app unknown)"));
    }
    const char *m = mit_name(k, id);
    if (strcmp(m, id) != 0) return VERB(A, h_r(A, m, "^[A-Z]{4}\\d{5} \\((.*)\\)$", "$1"), " (", id, ")");
    const char *g = galerie_dat_name(k, id, ordner);
    if (g[0]) return VERB(A, g, " (", id, T(", Spiel nicht mehr auf der Konsole)", ", game no longer on the console)"));
    return VERB(A, id, T(" (Spiel nicht mehr auf der Konsole)", " (game no longer on the console)"));
}

/* ---- NEU 1.0.7 (PS5-Test 02.11.2026, GTA V und Red Dead Redemption 2): Zuordnung und Namen nur aus Metadaten ----------------------
 * Ein installiertes Spiel nennt in seinen eigenen Angaben andere Kennungen, die zu ihm gehören:
 *  - SERVICE_ID_ADDCONT_ADD_1..7 (PS4: param.sfo; appinfo.db) bzw. addcont.serviceIdForSharing (PS5: param.json): dasselbe Spiel unter
 *    einer anderen Kennung (playstationdev.wiki/ps5devwiki Param.json: "Shares the game id of the same title on PS4 & PS5").
 *    -> gilt für alle Daten dieser Kennung: sie tragen den Namen des Spiels; Reste (Update, Zusatzinhalt, Lizenz, PlayGo, App-Daten) werden
 *    NICHT angeboten, solange das Spiel installiert ist - es kann sie brauchen (Codex-Prüfung 42 (1)).
 *  - INSTALL_DIR_SAVEDATA (PS4: param.sfo; PS4 und PS5: Sonys appinfo.db): unter dieser Kennung legt das Spiel seine Spielstände ab.
 *    -> belegt nur den Ordner der Spielstände, gilt darum nur für Spielstände (Codex-Prüfung 42 (3)).
 *    Beleg: da_Chris86s appinfo.db vom 02.11.2026 - Grand Theft Auto V (PPSA04264) INSTALL_DIR_SAVEDATA = PPSA03420 und
 *    SERVICE_ID_ADDCONT_ADD_1 = EP1004-PPSA03420_00; der Spielstand PPSA03420 stand bis 1.0.7 ohne Namen da und wurde als Rest gelöscht.
 * Nur diese Belege zählen - nie ähnliche Kennungen oder Namen. Nennen zwei installierte Spiele dieselbe Kennung: widersprüchlich, keine
 * Zuordnung. Ist die Kennung selbst installiert (auch ohne lesbaren Namen), gehört sie sich selbst (Codex-Prüfung 42 (4)).
 * Eine Zusatzinhalt-Kennung ist gültig als "EP1004-PPSA03420_00" oder leer/0 (unbenutzt). Alles andere ist kaputt - so bei einem umgebauten
 * Update von Red Dead Redemption 2 (7 x Zeilenumbruch + 2 Leerzeichen): beim Durchgehen der Spiele stürzt die PS5 daran ab (ShellCore
 * "invalid string position", da_Chris86s Aufzeichnungen 01./02.11.2026; github.com/skimasklover/Relapse-RDR2-Fix). */
static bool kennung_ok(const char *v) { return v && strlen(v) == 9 && r_m(v, "^[A-Z]{4}\\d{5}$"); }   /* Länge genau: "$" passt in PCRE2
                                                                                                    * auch vor einem Zeilenumbruch (Codex-Prüfung 42 (6)) */
static bool sid_gueltig(const char *v) { return v && strlen(v) == 19 && r_m(v, "^[A-Z]{2}\\d{4}-[A-Z]{4}\\d{5}_\\d{2}$"); }
/* leer/unbenutzt: "", "0" oder nur Leerzeichen. PS5-Test 03.11.2026 06:20: die GTA Definitive Editions haben in param.json 7 x 19 Leerzeichen
 * (Platzhalter in Kennungs-Länge) - Sony übernimmt sie gar nicht in appinfo.db/app.db (dort keine Zeile). Zeilenumbruch, Tab o. Ä. bleibt kaputt. */
static bool sid_leer(const char *v)
{
    if (!v || !strcmp(v, "0")) return true;   /* Codex-Prüfung 47 (2): "0" nur genau so, "  0" ist kaputt */
    while (*v == ' ') v++;
    return !*v;
}
static void verknuepfen(kern *k, mtab *ziel, const char *lid, const char *besitzer, const char *beleg)
{
    if (!kennung_ok(lid) || h_like(lid, "NPXS*") || h_gleich(lid, besitzer)) return;
    if (mt_hat(&k->INSTALLIERT, lid)) return;   /* selbst installiert - gehört sich selbst */
    if (mt_hat(ziel, lid)) {
        const char *alt = mt_hole(ziel, lid);
        if (alt && alt[0] && !h_gleich(alt, besitzer)) {
            mt_setze(A, ziel, lid, (void *)"");
            mt_setze(A, &k->VERK_BELEG, lid, (void *)VERB(A, T("widersprüchlich: ", "contradictory: "), alt, " / ", besitzer));
        }
        return;
    }
    lid = a_kopie(A, lid);
    mt_setze(A, ziel, lid, (void *)a_kopie(A, besitzer));
    if (!mt_hat(&k->VERK_BELEG, lid)) mt_setze(A, &k->VERK_BELEG, lid, (void *)VERB(A, beleg, T(" von ", " of "), besitzer));
}
/* eine Zusatzinhalt-Angabe: gültig -> Zuordnung, leer -> nichts, sonst kaputt. Ein kaputtes Feld zählt je Spiel einmal, egal aus welcher
 * Quelle (param.sfo, param.json, appinfo.db) - eine leere oder gültige Angabe der einen Quelle verdeckt nie eine kaputte der anderen
 * (Codex-Prüfung 42 (2)). */
static void sid_angabe(kern *k, const char *besitzer, const char *feld, const char *wert)
{
    if (sid_leer(wert)) return;
    if (sid_gueltig(wert)) { verknuepfen(k, &k->VERKNUEPFT, a_kopie_n(A, wert + 7, 9), besitzer, feld); return; }
    const char *gz = VERB(A, besitzer, "|", feld);
    if (mt_hat(&k->SID_GEZAEHLT, gz)) return;
    mt_setze(A, &k->SID_GEZAEHLT, gz, (void *)1);
    besitzer = a_kopie(A, besitzer);
    mt_setze(A, &k->SID_KAPUTT, besitzer, (void *)((intptr_t)mt_hole(&k->SID_KAPUTT, besitzer) + 1));
}
static void appinfo_zeile(kern *k, const char *titel, const char *feld, const char *wert)
{
    if (!titel || !feld || !mt_hat(&k->INSTALLIERT, titel)) return;
    titel = a_kopie(A, titel);
    if (!strcmp(feld, "INSTALL_DIR_SAVEDATA")) { if (kennung_ok(wert)) verknuepfen(k, &k->VERK_SPST, a_kopie(A, wert), titel, "INSTALL_DIR_SAVEDATA (appinfo.db)"); }
    else if (r_m(feld, "^SERVICE_ID_ADDCONT_ADD_\\d+$")) sid_angabe(k, titel, a_kopie(A, feld), wert ? a_kopie(A, wert) : "");
}
/* nennt mindestens ein installiertes Spiel id als dasselbe Spiel (auch wenn es mehrere sind - Besitzer dann unbekannt)? Dann werden Reste
 * dieser Kennung nicht angeboten (Codex-Prüfung 43 (1)) */
static bool verk_belegt(kern *k, const char *id) { return id && !kern_bereiche_106() && mt_hat(&k->VERKNUEPFT, id); }
/* Kennung des installierten Spiels, das id als dasselbe Spiel nennt (Zusatzinhalt-Kennung) - sonst NULL (auch bei Widerspruch) */
static const char *verk_inst(kern *k, const char *id)
{
    if (!id || kern_bereiche_106() || !mt_hat(&k->VERKNUEPFT, id)) return NULL;
    const char *o = mt_hole(&k->VERKNUEPFT, id);
    return o && o[0] ? o : NULL;
}
/* Name eines nicht (mehr) installierten Spiels aus verbliebenen Metadaten:
 *  1. Begleitdatei einer Aufnahme (Sony schreibt den Spielnamen hinein)
 *  2. Beschreibung der Spielstände (<savedata>_meta/user/<Kennung>/<Datei>.sfo, Feld MAINTITLE) - nur wenn ALLE denselben Namen tragen
 *     (ein Spiel kann seinen Spielständen eigene Titel geben - dann ist der Name nicht belegt). "" = kein Beleg. */
static const char *rest_name(kern *k, const char *id)
{
    if (mt_hat(&k->REST_NAMEN, id)) return mt_hole(&k->REST_NAMEN, id);
    id = a_kopie(A, id);
    mt_setze(A, &k->REST_NAMEN, id, (void *)"");
    const char *n = "";
    {
        sliste ordner = {0};
        const char *arten[2] = { "photo", "video" };
        for (int a1 = 0; a1 < 2; a1++) {
            sliste ag = s_namen(k->S, VERB(A, "/user/av_contents/", arten[a1]), false);
            for (int i = 0; i < ag.n; i++) { const char *p = VERB(A, "/user/av_contents/", arten[a1], "/", ag.e[i], "/", id); if (s_existiert(k->S, p)) sl_add(A, &ordner, p); }
        }
        if (ordner.n > 0) n = galerie_dat_name(k, id, &ordner);
    }
    if (!n[0]) {
        sliste homes = s_namen(k->S, "/user/home", false);
        const char *sds[2] = { "savedata_meta", "savedata_prospero_meta" }, *unter[2] = { "/user/", "/" };
        const char *einer = NULL; bool gleich = true;
        for (int h = 0; h < homes.n; h++) for (int s = 0; s < 2; s++) for (int u = 0; u < 2; u++) {
            const char *o = VERB(A, "/user/home/", homes.e[h], "/", sds[s], unter[u], id);
            dliste *l = s_liste(k->S, o, false);
            if (!l) continue;
            for (int j = 0; j < l->n; j++) {
                datei *d = &l->e[j];
                if (d->ist_ordner || !h_like(d->name, "*.sfo")) continue;
                mtab sf = sfo_von(k, VERB(A, o, "/", d->name));
                const char *mt = h_trim(A, kern_sfo_wert(A, &sf, "MAINTITLE"));
                if (!mt[0]) continue;
                if (!einer) einer = mt; else if (strcmp(einer, mt) != 0) gleich = false;
            }
        }
        if (einer && gleich && net_laenge(einer) >= 2 && net_laenge(einer) <= 100) n = einer;
    }
    mt_setze(A, &k->REST_NAMEN, id, (void *)n);
    return n;
}
static const char *besitzer_name(kern *k, const char *o) { const char *t = app_titel(k, o); return t && t[0] ? t : o; }
static const char *name_belegt(kern *k, const char *id, int *art)
{
    if (!mt_hat(&k->NAMEN_CACHE, id)) mt_setze(A, &k->NAMEN_CACHE, a_kopie(A, id), (void *)app_titel(k, id));
    const char *t = mt_hole(&k->NAMEN_CACHE, id);
    /* Codex-Prüfung 44: nennen mehrere installierte Spiele diese Kennung, ist die Zuordnung unbekannt - dann auch kein Name aus Resten
     * oder verbliebenen eigenen Angaben (außer die Kennung ist selbst installiert) */
    bool widerspruch = !kern_bereiche_106() && mt_hat(&k->VERKNUEPFT, id) && !verk_inst(k, id);
    if (t && t[0] && (!widerspruch || mt_hat(&k->INSTALLIERT, id))) { *art = 0; return t; }
    if (kern_bereiche_106() || widerspruch) { *art = 3; return ""; }
    const char *o = verk_inst(k, id);
    if (o) { *art = 1; return besitzer_name(k, o); }
    const char *r = rest_name(k, id);
    if (r[0]) { *art = mt_hat(&k->INSTALLIERT, id) ? 0 : 2; return r; }   /* Codex-Prüfung 45: installiert ohne lesbaren Namen ist nicht "nicht mehr da" */
    *art = 3;
    return "";
}
/* für Spielstände zusätzlich INSTALL_DIR_SAVEDATA; nennen die beiden Belege verschiedene Spiele: widersprüchlich -> unbekannt */
static const char *name_belegt_spst(kern *k, const char *id, int *art)
{
    const char *t = name_belegt(k, id, art);
    if (kern_bereiche_106() || !mt_hat(&k->VERK_SPST, id)) return t;
    const char *o = mt_hole(&k->VERK_SPST, id), *g = verk_inst(k, id);
    bool widerspruch = !o || !o[0] || (g && !h_gleich(g, o)) || (!g && mt_hat(&k->VERKNUEPFT, id));
    if (*art == 0 && (mt_hat(&k->INSTALLIERT, id) || !widerspruch)) return t;   /* eigene Angaben - außer bei Widerspruch und nicht installiert (Codex 43 (4)) */
    if (widerspruch) { *art = 3; return ""; }
    *art = 1;
    return besitzer_name(k, o);
}
/* Zuordnungen und kaputte Zusatzinhalt-Angaben ermitteln - einmal je Suche, nach dem Lesen der Kachel-Namen */
static void verknuepfungen_ermitteln(kern *k, const sliste *const *quellen, int nq)
{
    k->VERKNUEPFT = mt_neu(true); k->VERK_SPST = mt_neu(true); k->VERK_BELEG = mt_neu(true); k->INSTALLIERT = mt_neu(true);
    k->SID_KAPUTT = mt_neu(true); k->SID_GEZAEHLT = mt_neu(true); k->SID_WARNUNG = (sliste){0}; k->REST_NAMEN = mt_neu(true);
    if (kern_bereiche_106()) return;
    /* installiert ist installiert - auch ohne lesbaren Namen (Codex-Prüfung 42 (4)) */
    for (int q = 0; q < nq; q++) for (int i = 0; i < quellen[q]->n; i++) {
        const char *id = quellen[q]->e[i];
        if (kennung_ok(id) && !h_like(id, "NPXS*")) mt_setze(A, &k->INSTALLIERT, id, (void *)1);
    }
    for (int i = 0; i < k->INSTALLIERT.n; i++) {
        const char *id = k->INSTALLIERT.e[i].k;
        /* alle drei Orte der Angaben (wie app_titel_lesen) - nicht nur der erste mit Namen (Codex-Prüfung 43 (2)/(3)) */
        const char *jd[3] = { VERB(A, "/user/appmeta/", id, "/param.json"), VERB(A, "/user/app/", id, "/sce_sys/param.json"), VERB(A, SYS_META "/", id, "/param.json") };
        for (int q = 0; q < 3; q++) {
        const char *roh = s_text(k->S, jd[q]);
        /* PS5: param.json addcont.serviceIdForSharing - mit cJSON gelesen, damit Escapes wie E richtig ankommen (Codex-Prüfung 42 (5));
         * Reihenfolge wie SERVICE_ID_ADDCONT_ADD_1..n in appinfo.db */
        if (roh && roh[0]) {
            cJSON *o = cJSON_ParseWithOpts(h_ohne_bom(roh), NULL, true);
            if (o && cJSON_IsObject(o)) {
                cJSON *ad = hole_ci(o, "addcont"), *arr = ad ? hole_ci(ad, "serviceIdForSharing") : NULL;
                if (!arr) arr = hole_ci(o, "serviceIdForSharing");
                if (arr && cJSON_IsArray(arr)) {
                    int j = 0;
                    for (cJSON *c = arr->child; c; c = c->next, j++)
                        sid_angabe(k, id, a_fmt(A, "SERVICE_ID_ADDCONT_ADD_%d", j + 1),
                                   cJSON_IsString(c) && c->valuestring ? a_kopie(A, c->valuestring) : cJSON_IsNull(c) ? "" : "?");
                }
                else if (arr && cJSON_IsString(arr) && arr->valuestring) sid_angabe(k, id, "SERVICE_ID_ADDCONT_ADD_1", a_kopie(A, arr->valuestring));
            }
            if (o) cJSON_Delete(o);
        }
        }
        /* PS4: param.sfo (dieselben Orte wie app_titel_lesen) */
        const char *sd[3] = { VERB(A, "/user/appmeta/", id, "/param.sfo"), VERB(A, "/user/app/", id, "/sce_sys/param.sfo"), VERB(A, SYS_META "/", id, "/param.sfo") };
        for (int s = 0; s < 3; s++) {
            mtab h = sfo_von(k, sd[s]);
            if (h.n == 0) continue;
            for (int j = 1; j <= 7; j++) { const char *f = a_fmt(A, "SERVICE_ID_ADDCONT_ADD_%d", j); if (mt_hat(&h, f)) sid_angabe(k, id, f, kern_sfo_wert(A, &h, f)); }
            const char *sv = kern_sfo_wert(A, &h, "INSTALL_DIR_SAVEDATA");
            if (kennung_ok(sv)) verknuepfen(k, &k->VERK_SPST, sv, id, "INSTALL_DIR_SAVEDATA (param.sfo)");
        }
    }
    if (k->appinfo_lesen) k->appinfo_lesen(k, appinfo_zeile);
    for (int i = 0; i < k->SID_KAPUTT.n; i++) {
        const char *id = k->SID_KAPUTT.e[i].k, *t = app_titel(k, id);
        int n = (int)(intptr_t)k->SID_KAPUTT.e[i].v;
        sl_add(A, &k->SID_WARNUNG, VERB(A, T("ACHTUNG: ", "WARNING: "), t[0] ? VERB(A, t, " (", id, ")") : id, " ",
            a_fmt(A, T("hat %d kaputte Angabe(n) zu Zusatzinhalten. Daran kann die PS5 abstürzen und sich ausschalten, wenn sie im Hintergrund die Spiele durchgeht - auch wenn das Spiel gar nicht läuft. Abhilfe: das Spiel mit PS5 Cleanup löschen (die kaputten Angaben werden vorher repariert) und mit einem unveränderten Update neu installieren.",
                       "has %d broken add-on entr(y/ies). This can make the PS5 crash and switch off when it goes through the games in the background - even if the game is not running. Fix: delete the game with PS5 Cleanup (the broken entries are repaired first) and reinstall it with an unmodified update."), n)));
    }
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
/* NEU 1.0.7 (da_Chris86 03.11.2026): eingerichtete Jailbreak-Einstiege als Kürzel für die freiwillige QR-Rückmeldung -
 * nur feste Kürzel, keine Namen und keine Kennungen. "" = keiner erkannt. */
const char *kern_jailbreak_kuerzel(kern *k, arena *a)
{
    static const char *const KZ[] = { "y2jb", "netflix", "lua", "ps2", "renpy", "webkit" };
    static const char *const MU[] = { "/(" JB_Y2JB ")(/|$)", "/(" JB_NETFLIX ")(/|$)", "/(" JB_LUA ")(/|$)", "/(" JB_PS2 ")(/|$)", "/(" JB_YARPE ")(/|$)", "/(" JB_WEBKIT ")(/|$)" };
    bool da[6] = {0}, browser = false;
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        if (!f->Jailbreak) continue;
        for (int j = 0; j < f->PfadeBasis.n; j++) {
            const char *p = f->PfadeBasis.e[j];
            if (h_m(p, JB_BROWSER)) { browser = true; continue; }
            for (int x = 0; x < 6; x++) if (!da[x] && h_m(p, MU[x])) da[x] = true;
        }
    }
    const char *t = "";
    for (int x = 0; x < 6; x++) if (da[x]) t = VERB(a, t, t[0] ? "," : "", KZ[x]);
    if (k->BD_DISC) t = VERB(a, t, t[0] ? "," : "", "bd");
    if (browser) t = VERB(a, t, t[0] ? "," : "", "browser");
    return t;
}
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
        const char *bk = kern_kat_name(kern_bereiche_106() ? 6 : 7);   /* 1.0.7: Bereich "Jailbreak" (bis 1.0.6 "Jailbreak & Homebrew") */
        const char *bt = VERB(a, T("Ein offline gespeicherter WebKit-Jailbreak kann im „Browserspeicher“ liegen (unter „", "An offline-saved WebKit jailbreak may be in the \"Browser storage\" (under \""),
                              bk, T("“).", "\")."));
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

/* NEU 1.0.7 (Codex-Prüfung 48): Eintrag von PS5 Cleanup selbst, der zum Schluss entfernt wird - zählt nicht als "bleibt" */
bool kern_selbst_gesperrt(const eintrag *f) { return f && f->Gesperrt && f->SperrGrund && !strcmp(f->SperrGrund, selbst_grund()); }
typedef struct { const char *quelle; bool gesperrt, kachel, smTipp; const char *sperrtext, *sperrgrund; } merke_opt;
/* NEU 1.0.7 (Recherche 31.10.2026): Einhängeordner der Tools und die Merkdatei von ShadowMount gar nicht erst anbieten
 * (Löschen lehnt kern_nie_anfassen ohnehin ab - so stehen sie auch nicht in der Liste) */
static bool einhaengeordner(const char *p)
{
    if (kern_bereiche_106()) return false;   /* Vergleichstests mit 1.0.5/1.0.6 unverändert */
    return h_m(p, "^/user/data/(imgmnt|save_mnt|mount_sd|mount_owner)(/|$)") || h_m(p, "^/user/data/\\.kstuff_noautomount$") || h_m(p, "^/mnt/ps5upload(/|$)")
        || h_m(p, "^/user/data/(homebrew/)?[^/]+/folder-source(/|$)")   /* LZ4 Game Compressor: eingeblendeter Spielordner (Recherche 31.10.2026) */
        || h_m(p, "^/user/data/PS5-Cleanup-Protokolle(/|$)")            /* NEU 02.11.2026: Protokolle von PS5 Cleanup ohne USB-Stick (Ordner) */
        || h_m(p, "^/mnt/ext1/PS5-Cleanup-Protokolle(/|$)");            /* NEU 02.11.2026 17:23: dasselbe auf der M.2 */
}
static void merke_o(kern *k, const sliste *pfade, int kat, const char *text, merke_opt o)
{
    {
        sliste ohne = {0};
        for (int i = 0; i < pfade->n; i++) {
            if (einhaengeordner(pfade->e[i])) continue;
            /* NEU 1.0.7 (02.11.2026): die Protokoll-Datei, in die gerade geschrieben wird, nie anbieten */
            if (!kern_bereiche_106() && k->EIGENES_PROTOKOLL && k->EIGENES_PROTOKOLL[0] && !strcmp(pfade->e[i], k->EIGENES_PROTOKOLL)) continue;
            sl_add(A, &ohne, pfade->e[i]);
        }
        if (ohne.n == 0) { k->MERKE_OK = false; return; }
        if (ohne.n != pfade->n) { sliste *np = a_hole(A, sizeof *np); *np = ohne; pfade = np; }
    }
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
            o.sperrtext = kern_bereiche_106() ? T("  [PS5 Cleanup - Kopie auf dem USB-Stick, bleibt]", "  [PS5 Cleanup - copy on the USB stick, stays]")
                                              : T("  [PS5 Cleanup - Kopie auf dem USB-Laufwerk, bleibt]", "  [PS5 Cleanup - copy on the USB drive, stays]");
            o.sperrgrund = kern_bereiche_106() ? T("Das ist eine Kopie von PS5 Cleanup auf einem USB-Stick. Sie wird nie automatisch entfernt - bei Bedarf am PC löschen.", "This is a copy of PS5 Cleanup on a USB stick. It is never removed automatically - delete it on a PC if needed.")
                                               : T("Das ist eine Kopie von PS5 Cleanup auf einem USB-Laufwerk. Sie wird nie automatisch entfernt - bei Bedarf am PC löschen.", "This is a copy of PS5 Cleanup on a USB drive. It is never removed automatically - delete it on a PC if needed.");
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
        /* NEU 1.0.7: Zeile "Spieldatei" eines ShadowMount-Spiels gewählt -> die Kachel geht mit */
        if (f->SmDatei && f->Wahl && f->SmKachel && !f->SmKachel->Gesperrt) f->SmKachel->Wahl = true;
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
            case ORT_USB: wo = kern_bereiche_106() ? "USB" : T("erweiterten Speicher", "extended storage"); break;   /* Codex-Prüfung 48 */
            case ORT_STICK: wo = kern_bereiche_106() ? T("USB-Stick", "USB stick") : T("USB-Laufwerk", "USB drive"); break;
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
            /* NEU 1.0.7 (PS5-Test 02.11.2026, prosperoeden): "(alles außer PS5 Cleanup)" nur, wenn wirklich PS5 Cleanup darin liegt - einzelne
             * Dateien, die sich nicht lesen ließen und darum geschützt sind, machen den Ordner nicht zu "unserem". Sie bleiben still stehen
             * und werden nicht einzeln aufgelistet (wie private Ordner, da_Chris86 30.10.2026: "keinen Hinweis"). */
            for (int j = 0; j < eig.n; j++) if (kern_bereiche_106() ? !kern_selbstschutz_unlesbar(k, eig.e[j]) : kern_selbst_eigen(k, eig.e[j])) echt_eigen = true;
            if (frei.n > 0) MERKE(k, &frei, kat, echt_eigen ? VERB(A, vorsatz, x, T("  (alles außer PS5 Cleanup)", "  (everything except PS5 Cleanup)")) : VERB(A, vorsatz, x), 0);
            for (int j = 0; j < eig.n; j++) {
                if (!kern_bereiche_106() && !k->SCHUTZ.unsicher && !kern_selbst_eigen(k, eig.e[j])) continue;
                sliste l = L1(A, eig.e[j]); MERKE(k, &l, kat, VERB(A, vorsatz, x, eig.e[j] + strlen(p)), 0);
            }
            continue;
        }
        /* NEU 1.1, Selbstschutz: liegt PS5 Cleanup in diesem Ordner, wird er aufgeklappt wie bei ShadowMount-Spielen */
        if (kern_selbstschutz_enthaelt(k, p)) drin = true;
        if (!drin && !link && e->ist_ordner && kat != 1 && enthaelt_spiel(k, p)) drin = true;   /* NEU 1.0.6 (30.10.2026): Spiel im Ordner */
        /* NEU 1.0.7 (02.11.2026, ein Block pro Kachel): /user/data/homebrew wird immer aufgeklappt - jeder Programmordner darin
         * (homebrew/<Kennung>) steht unter seiner Kachel; bis hier war der ganze Ordner EIN Eintrag "homebrew" */
        if (!drin && !link && e->ist_ordner && !kern_bereiche_106() && !strcmp(basis, "/user/data") && h_gleich(x, "homebrew")) {
            dliste *hb = s_liste(k->S, p, false);   /* Codex-Prüfung 40 (7): ein LEERER homebrew-Ordner bleibt ein Eintrag (wie bisher) */
            if (hb && hb->n > 0) drin = true;
        }
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
    size_t n = strlen(s); char *o = a_hole(A, 4 * n + 1); size_t j = 0;
    bool plus = !kern_bereiche_106();   /* NEU 1.0.7 (PS5-Test 03.11.2026): "Shadow Mount+" = ShadowMountPlus_1.7-beta7.elf */
    for (size_t i = 0; i < n; i++) { char c = s[i]; if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) o[j++] = c;
                                     else if (c == '+' && plus) { memcpy(o + j, "plus", 4); j += 4; } }
    o[j] = 0; return o;
}
static const char *tool_alias(const char *id)
{
    /* NEU 1.0.7 (da_Chris86 01.11.2026 23:22): Game Compressor = PSGC50001 (/data/GameCompressor), LZ4-Game-Compressor = LZGC50001
     * (/data/AMPRStudio) - Belege doku/recherche_tools_B_2026-10-31.md */
    static const char *const T2[][2] = { { "PLDM00001", "pldmgr" }, { "PKGM00001", "pkgmgr" }, { "APDU00001", "ps5appdumper" },
                                         { "PSGC50001", "gamecompressor" }, { "LZGC50001", "amprstudio" }, { NULL, NULL } };
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
/* NEU 1.0.7 (Recherche 31.10.2026, Quellcode LZ4 Game Compressor ampr-ps5/standalone/library.h mountFolderView): Der LZ4 Game
 * Compressor blendet den gewählten Spielordner schreibgeschützt (nullfs) unter <Tool-Ordner>/folder-source ein. Das ist das ORIGINAL
 * des Spiels, keine Kopie (da_Chris86s PS5: /data/AMPRStudio/folder-source = F1 25 von der M.2, 86 GB). Damit es weder mitgezählt noch
 * mitgelöscht wird, steht so ein Tool-Ordner als sein Inhalt OHNE folder-source in den Pfaden (die Löschschicht lehnt eingehängte
 * Ordner zusätzlich ab). f->ToolOrdner merkt sich den Tool-Ordner für Name und Art der Zeile. Ist sonst nichts drin: kein Eintrag. */
static void ohne_eingeblendete_ordner(kern *k, pliste *liste)
{
    pliste neu = {0};
    for (int i = 0; i < liste->n; i++) {
        eintrag *f = liste->e[i];
        bool betroffen = false;
        sliste pf = {0};
        for (int j = 0; j < f->PfadeBasis.n; j++) {
            const char *p = f->PfadeBasis.e[j];
            dliste *dl = h_m(p, "^/user/data/(homebrew/)?[^/]+$") ? s_liste(k->S, p, false) : NULL;
            bool hat = false;
            for (int x = 0; dl && x < dl->n; x++) if (dl->e[x].ist_ordner && !strcmp(dl->e[x].name, "folder-source")) hat = true;
            /* 31.10.2026 (da_Chris86: "Du entscheidest"): LEERES folder-source = nichts eingeblendet (z. B. nach Neustart) - dann ist es
             * nur ein Rest-Ordner und geht mit dem Tool-Ordner mit. Nicht lesbar = wie eingeblendet behandeln. Wird inzwischen doch
             * eingeblendet, lehnt die Löschschicht ab (zugriff_datei.c baum_pruefen/baum_loeschen: einbindung). */
            if (hat) { dliste *fs = s_liste(k->S, VERB(A, p, "/folder-source"), false); if (fs && fs->n == 0) hat = false; }
            if (!hat) { sl_add(A, &pf, p); continue; }
            betroffen = true;
            if (!f->ToolOrdner) f->ToolOrdner = p;
            for (int x = 0; x < dl->n; x++) if (strcmp(dl->e[x].name, "folder-source")) sl_add(A, &pf, VERB(A, p, "/", dl->e[x].name));
        }
        if (betroffen) {
            if (pf.n == 0) continue;                         /* nur der eingeblendete Spielordner: nichts anzubieten */
            f->PfadeBasis = pf; f->Pfade = sl_kopie(A, &pf);
        }
        pl_add(A, &neu, f);
    }
    *liste = neu;
}
static void tool_daten_ordnen(kern *k, pliste *liste)
{
    const char *vor = T("Tool-Kachel: ", "Tool tile: ");
    int n = liste->n;
    int *zu = a_hole(A, sizeof(int) * (size_t)(n > 0 ? n : 1));   /* Index der Kachel, zu der ein Daten-Eintrag gehört (-1 = keine) */
    for (int i = 0; i < n; i++) zu[i] = -1;
    for (int d = 0; d < n; d++) {
        eintrag *f = liste->e[d];
        if (f->Kat != 6 || f->Kachel || f->PfadeBasis.n == 0) continue;
        /* NEU 1.0.7 (da_Chris86 01.11.2026 abends: "wie bei Spielen ... die Kachel war an Position 3, die Elf-Datei an Position 12"):
         * auch die Payload-Datei eines Tools (ELF/BIN, intern oder auf dem USB-Stick) steht unter seiner Kachel. Schluessel = Dateiname
         * ohne Versionsangabe ("pldmgr_v0.6.3.elf" -> pldmgr, "PKG-Manager_v1.5.1.elf" -> pkgmanager). Bis 1.0.6 unveraendert. */
        const char *dk = NULL;
        htreffer me = (kern_bereiche_106() || f->PfadeBasis.n != 1 || (f->Ort != ORT_INTERN && f->Ort != ORT_STICK)) ? (htreffer){0}
                    : h_mt(A, f->PfadeBasis.e[0], "^(?:/user/data/|/mnt/usb\\d+/)(?:[^/]+/)*?([^/]+?)(?:[-_ ]v?\\d[\\w.\\-]*)?\\.(?i:elf|bin)$");
        if (me.ok) dk = tool_schluessel(k, me.gr[1]);
        else if (f->Ort == ORT_INTERN) { htreffer m = h_mt(A, f->PfadeBasis.e[0], "^/user/data/(homebrew/)?([^/]+)"); if (m.ok) dk = tool_schluessel(k, m.gr[2]); }
        if (!dk) continue;
        int treffer = -1, anz = 0, at = -1, aanz = 0;
        for (int t = 0; t < n; t++) {
            eintrag *g = liste->e[t];
            if (g->Kat != 6 || !g->Kachel || !txt_beginnt(g->TextBasis, vor) || g->PfadeBasis.n == 0) continue;
            htreffer mi = h_mt(A, g->PfadeBasis.e[0], "^/user/app/([A-Z]{4}\\d{5})$");
            if (!mi.ok) continue;
            const char *titel = g->TextBasis + strlen(vor);
            const char *ende = strstr(titel, VERB(A, " (", mi.gr[1], ")"));
            const char *tk = tool_schluessel(k, ende ? a_kopie_n(A, titel, (size_t)(ende - titel)) : titel);
            const char *ak = tool_alias(mi.gr[1]);
            if (ak[0] && !strcmp(ak, dk)) { at = t; aanz++; }   /* NEU 1.0.7 (23:22): feste Zuordnung */
            if (tool_passt(tk, ak, dk)) { treffer = t; anz++; }
        }
        if (aanz == 1) zu[d] = at;          /* feste Zuordnung zählt vor dem Namensvergleich */
        else if (anz == 1) zu[d] = treffer;
    }
    pliste neu = {0};
    for (int i = 0; i < n; i++) {
        if (zu[i] >= 0) continue;                       /* steht unter seiner Kachel */
        pl_add(A, &neu, liste->e[i]);
        for (int d = 0; d < n; d++) if (zu[d] == i) {
            eintrag *f = liste->e[d];
            f->Text = VERB(A, "   – ", f->Text); f->TextBasis = VERB(A, "   – ", f->TextBasis);
            f->ToolKachel = liste->e[i];   /* NEU 1.0.7: Daten gehen nur mit ihrer Kachel (kern_loeschen.c) */
            pl_add(A, &neu, f);
        }
    }
    *liste = neu;
}

/* NEU 1.0.7 (da_Chris86 31.10.2026): bekannte Homebrew-Tools - ihre Daten-Ordner unter /user/data stehen auch ohne Kachel unter "Apps".
 * Schlüssel wie tool_schluessel (klein, nur Buchstaben/Ziffern). Belegt durch die Ordner auf da_Chris86s PS5 (Exe-Protokoll 30.10.2026)
 * und die dazugehörigen Tools; die Liste wird mit der Repository-Liste von nexgen999 erweitert. Unbekannte Ordner -> "Sonstiges". */
static const char *const BEKANNTE_TOOLS[] = { "pldmgr", "pkgmgr", "bfpilot", "orbitstore", "shadowmount", "ps5upload", "gamecompressor",
                                              "homebrew", "nanodns", "etahen", "spectrumlibrary", "playden", "zftpd", "blackbox", "blackboxv2",
                                              "ps5autoloader",   /* Autoloader-Einstellungen = Homebrew (da_Chris86 30.10.2026) */
                                              /* NEU 1.0.7 (Recherche 31.10.2026, doku/recherche_tools_*.md, recherche_tooldaten_*.md): Daten-Ordner aus dem Quellcode der Tools */
                                              "cheatrunner", "garlic", "liveshell", "neoenabler", "neosettings", "onionhen", "ps4mount", "dumpforge", "ps5dumpforge",
                                              "wfm", "ps5save", "orbitstore", "amprstudio", "ps5appdumper", "elfarsenal", "goldhen", "psxs5", "exfat2ampr",
                                              "ps5suitefancfg", "user", NULL };   /* "user" = /data/user/patches von ps-patch-system */
static bool tool_bekannt(kern *k, const char *ordner)
{
    const char *s = tool_schluessel(k, ordner);
    for (int i = 0; BEKANNTE_TOOLS[i]; i++) if (!strcmp(s, BEKANNTE_TOOLS[i])) return true;
    return false;
}
/* NEU 1.0.7 (PS5-Test 02.11.2026, da_Chris86: "alles, was zu einer Kachel gehört, soll auch aufgelistet werden" - "das sucht sich ja
 * der User dumm und dämlich"; Grundsatz 19:57: für jede Konsole, keine Namenslisten einzelner Apps): EIN Block pro Kachel. Unter jeder
 * Kachel (Spiel, App, Tool - eingeordnet, wie die PS5 es anzeigt) steht eingerückt alles, was dazugehört, egal wo es liegt: Daten-Ordner
 * unter /user/data (auch homebrew/<Kennung>), Ordner und Dateien auf M.2, USB-Speicher und USB-Stick, ELF/BIN-Payloads, PKG- und
 * Abbild-Dateien mit der Spielnummer im Namen. Zugeordnet wird
 *   1. über die Kennung im Namen (CUSA08519, FAKE77031 ...) - nur wenn genau EINE Kachel diese Kennung hat;
 *   2. sonst über den Namen der App (klein, nur Buchstaben/Ziffern): gleich > einer enthält den anderen (ab 5 Zeichen; bei Dateien muss
 *      der Name mit dem App-Namen anfangen). Es zählt die beste Stufe, in der überhaupt etwas passt - und nur, wenn darin genau EINE
 *      Kachel passt. Ein bloß gleicher Anfang (bis 02.11.2026 bei Tool-Kacheln ab 8 Zeichen) zählt nicht mehr: der Zufallstest fand
 *      "sprocketteil87" unter der Kachel "Sprocket Zenith" - das wäre geraten.
 * Passt nichts eindeutig, bleibt der Eintrag, wo er war (Unbekanntes unter "Sonstiges"). Jeder Eintrag behält seinen eigenen Haken; wie
 * bei den Tool-Daten gehen Teile einer mit angehakten Kachel nur, wenn die Kachel wirklich entfernt wurde (kern_loeschen.c, ToolKachel).
 * Nie verschoben: Jailbreak-Einstiege, Spieldateien von ShadowMount-Kacheln (eigene Regel), Update/Zusatzinhalt (gehören schon zum Spiel),
 * Spielstände, Trophäen, Aufnahmen. Zusätzlich: mehrere Trophäen-Listen EINES Spiels stehen zusammen (erste Liste oben). */
static const char *kopf_kennung(const eintrag *g)
{
    if (g->KachelId && g->KachelId[0]) return g->KachelId;
    if (g->Gruppe && ist_teil(g, "Spiel")) { const char *b = strrchr(g->Gruppe, '|'); if (b && r_m(b + 1, "^[A-Z]{4}\\d{5}$")) return b + 1; }
    return NULL;
}
static bool ist_block_kopf(const eintrag *g)
{
    if (g->Jailbreak || g->SmDatei || (g->Kat != 1 && g->Kat != 6) || g->PfadeBasis.n == 0) return false;
    if (g->Teil && strcmp(g->Teil, "Spiel") != 0) return false;
    return kopf_kennung(g) != NULL;
}
static const char *kopf_titel(kern *k, const eintrag *g, const char *id)
{
    const char *t = app_titel(k, id);
    if (t && t[0]) return t;
    const char *x = g->TextBasis ? g->TextBasis : "";
    if (txt_beginnt(x, T("Tool-Kachel: ", "Tool tile: "))) x += strlen(T("Tool-Kachel: ", "Tool tile: "));
    const char *e = strstr(x, VERB(A, " (", id, ")"));
    return e ? a_kopie_n(A, x, (size_t)(e - x)) : "";
}
/* Der Teil eines Pfads, der sagt, wem er gehört: /user/data/<X>, /user/data/homebrew/<X>, /mnt/<Speicher>/<X> - bei reinen Sammelordnern
 * (homebrew, payloads, games ...) auf M.2/USB der Ordner darin. NULL = gehört keiner App (z. B. Sony-Ordner). */
static const char *besitzer_teil(kern *k, const char *p)
{
    htreffer m = h_mt(A, p, "^/user/data/homebrew/([^/]+)");
    if (m.ok) return m.gr[1];
    m = h_mt(A, p, "^/user/data/([^/]+)");
    if (m.ok) return h_gleich(m.gr[1], "homebrew") ? NULL : m.gr[1];
    m = h_mt(A, p, "^/mnt/(?:ext\\d+|usb\\d+)/([^/]+)(?:/([^/]+))?");
    if (m.ok) {
        if (h_m(m.gr[1], "^(homebrew|payloads?|games|data|user|system_data|lost\\+found)$")) return m.gr[2][0] ? m.gr[2] : NULL;
        return m.gr[1];
    }
    return NULL;
}
static bool name_ist_datei(const char *x) { return r_m(x, "\\.[A-Za-z][A-Za-z0-9]{0,7}$"); }
/* Beginnt der Text (Ordner-/Dateiname oder Titel) mit dem Schlüssel s (klein, nur Buchstaben/Ziffern - andere Zeichen im Text zählen beim
 * Vergleich nicht), und endet der Schlüssel dort an einer Grenze? Grenze: Ende, - _ . + oder eine Ziffer; mit leer_ok auch ein Leerzeichen.
 * Codex-Prüfung 40 (2): "Minecraft Dungeons" (ein anderes Produkt) gehört nicht zur Kachel "Minecraft" - "playden-games", "blackbox-v2",
 * "orbit_store" schon (App-Ordner tragen Trenner, Produktnamen Leerzeichen). */
static bool beginnt_mit_schluessel(const char *text, const char *s, bool titel)
{
    const char *p = text;
    if (!s[0]) return false;
    while (*s) {
        unsigned char c = (unsigned char)*p;
        if (!c) return false;
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        bool an = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        /* Codex-Prüfung 56: "+" ist im Schlüssel "plus" (tool_schluessel) - hier genauso ("SpiegelMount+-cache" unter "Spiegel Mount+") */
        if (c == '+' && !kern_bereiche_106()) { if (strncmp(s, "plus", 4) != 0) return false; s += 4; p++; continue; }
        if (!an) { p++; continue; }   /* Leerzeichen, Satz- und Sonderzeichen (auch ™ ®) zählen beim Vergleich nicht - wie tool_schluessel */
        if (c != (unsigned char)*s) return false;
        p++; s++;
    }
    unsigned char n = (unsigned char)*p;
    if (n == 0) return true;
    /* Codex-Prüfung 41 (5): im Titel ist JEDES Zeichen außer Buchstabe/Ziffer eine Grenze ("ProsperoEden™ Switch UI") */
    if (titel) return !((n >= 'a' && n <= 'z') || (n >= 'A' && n <= 'Z') || (n >= '0' && n <= '9'));
    return n == '-' || n == '_' || n == '.' || n == '+' || (n >= '0' && n <= '9');
}
/* 1 = gleich, 2 = einer beginnt mit dem anderen (an einer Grenze); 0 = passt nicht. Nur der ANFANG zählt - ein Name irgendwo mittendrin
 * nicht mehr (Codex-Prüfung 40). tk/dk: Schlüssel von App und Eintrag, titel/stamm: die Originale dazu. */
static int name_stufe(const char *tk, const char *titel, const char *ak, const char *dk, const char *stamm)
{
    size_t lt = strlen(tk), ld = strlen(dk);
    if (ld == 0) return 0;
    if ((ak[0] && !strcmp(ak, dk)) || (lt && !strcmp(tk, dk))) return 1;
    if (lt == 0) return 0;
    if (ld > lt && lt >= 5 && beginnt_mit_schluessel(stamm, tk, false)) return 2;   /* "playden-games" unter "Playden" */
    if (lt > ld && ld >= 5 && beginnt_mit_schluessel(titel, dk, true)) return 2;     /* "prosperoeden" unter "ProsperoEden Switch UI" */
    return 0;
}
/* NEU 1.0.7 (PS5-Test 03.11.2026): bester Kopf für einen Namen (Stufen wie name_stufe). -1 = keiner passt, -2 = mehrere.
 * Passen in der besten Stufe mehrere Kacheln mit GENAU demselben Namen (da_Chris86s PS5: zwei Kacheln "Orbit Store" - ORBT00001 vom
 * Programm orbit_store.elf und dessen TV-App PPSA99177), ist es dasselbe Programm: der Eintrag steht unter der Kachel im passenden
 * Bereich (Daten und Dateien unter der App/Tool-Kachel, Spiele unter dem Spiel) - nur, wenn es dort genau EINE davon gibt. Gelöscht wird er trotzdem nur,
 * wenn er selbst angehakt ist und seine Kachel wirklich entfernt wurde (kern_loeschen.c). */
static int kopf_fuer_namen(pliste *liste, int n, const char **kid, const char **ktk, const char **ktt, const char *dk, const char *stamm, int kat)
{
    int beste = 4, tr = -1, anz = 0;
    for (int t = 0; t < n; t++) {
        if (!kid[t]) continue;
        int st = name_stufe(ktk[t], ktt[t], tool_alias(kid[t]), dk, stamm);
        if (!st) continue;
        if (st < beste) { beste = st; tr = t; anz = 1; }
        else if (st == beste) anz++;
    }
    if (anz <= 1) return tr;
    /* Codex-Prüfung 56: zwei verschiedene Apps mit gleichem Namen im selben Bereich ("FTP Server") - nicht raten */
    int erst = -1, passend = -1, np = 0, will = kat == 1 ? 1 : 6;
    for (int t = 0; t < n; t++) {
        if (!kid[t] || name_stufe(ktk[t], ktt[t], tool_alias(kid[t]), dk, stamm) != beste) continue;
        if (erst < 0) erst = t;
        else if (strcmp(ktt[t], ktt[erst]) != 0) return -2;
        if (((eintrag *)liste->e[t])->Kat == will) { passend = t; np++; }
    }
    return np == 1 ? passend : -2;
}
static bool gleich_ci_n(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y || !x) return false;
    }
    return true;
}
/* NEU 1.0.7 (PS5-Test 03.11.2026, da_Chris86 16:15 und 16:22: "jedes Spiel und jede Kachel-App soll alles an Daten bekommen, was sie
 * selbst anlegt" - "nicht nur nach meinen Programmen ... auch zukünftig für die Community"): BELEG AUS DER APP SELBST. Was nach Kennung
 * und Namen keiner Kachel gehört, wird im Programm der Kacheln gesucht - nur lesen, stückweise. Programm = ELF-Dateien (eboot.bin, *.elf,
 * erkannt an den ersten 4 Bytes) direkt im App-Ordner der Kachel, in ihrem Homebrew-Ordner unter /data (ShadowMount-Quelle) und in den
 * Daten-Ordnern und Payload-Dateien, die schon unter ihr stehen. Steht dort der Pfad (/data/<Name>, /data/homebrew/<Name>) bzw. bei einer
 * Punkt-Datei direkt auf einem Speicher ihr Name (.orbit-drive), gehört der Eintrag zu dieser Kachel. da_Chris86s PS5: /data/PCSX2,
 * /data/PS5SXHelper.lock und /data/whitelist.txt stehen im Programm von PS5SX2, .orbit-drive in orbit_store.elf (Orbit Store).
 * Nennen mehrere Kacheln den Pfad, zählt die mit den meisten Programmdateien, die ihn nennen (PS5SX2: eboot.bin, PS5SXHelper.elf und
 * PS5SX2Installer.elf - die Disc-Kachel "Reload PS2 Disc", die PS5SX2 anlegt, nur ihre Kopie der eboot.bin); bei Gleichstand bleibt
 * der Eintrag, wo er war - nicht raten. Nie so zugeordnet: Kennungs-Ordner, Daten-Ordner bekannter Tools ohne Kachel (etaHEN, garlic ...),
 * ELF-, BIN- und PKG-Dateien (ein Programm nennt oft fremde Tools, die es nur benutzt), Gesperrtes (PS5 Cleanup selbst). Grenzen:
 * Programmdateien bis 128 MB, zusammen höchstens 1 GB - was danach käme, wird nicht gelesen und kann eine Zuordnung verhindern, nie
 * herbeiführen. Verknüpfungen werden nie verfolgt, auch nicht im Weg davor. Höchstens 4096 ungeklärte Einträge je Suche, EIN Durchgang.
 * (Codex-Prüfung 56/57: linke Grenze, nicht Lesbares zählt als offen, gleichnamige Apps, Speicher, Abbrechen.) */
#define BELEG_DATEI_MAX (128LL << 20)   /* Codex-Prüfung 58: Abbrechen wirkt zwischen zwei Dateien - eine Datei ist in Bruchteilen einer Sekunde gelesen */
#define BELEG_UNBEKANNT 1000000          /* Codex-Prüfung 58: Ordner nicht lesbar - unbekannt viele Programme, keine sichere Zuordnung */
#define BELEG_GESAMT_MAX (1LL << 30)
#define BELEG_KANDIDATEN 4096  /* je Suche höchstens so viele ungeklärte Einträge (Speicher) - der Rest bleibt, wo er war */
typedef struct { const char *pfad; int kopf; long long gr; bool zu_gross; } bdatei;
typedef struct { bdatei *e; int n, cap; mtab gesehen; int *offen; } bliste;
/* Art eines Pfads: 0 = gibt es nicht oder ein Teil des Wegs ist eine Verknüpfung (wird nie verfolgt - Codex-Prüfung 56/57: auch nicht im
 * Weg davor), 1 Datei, 2 Ordner, -1 nicht lesbar (zählt als offen). Geprüft wird jeder Teil unterhalb der Speicherwurzel
 * (/user/app, /user/data, /mnt/extN, /mnt/usbN ...) aus der Liste seines Ordners. */
static int beleg_art(kern *k, const char *p, long long *gr)
{
    htreffer w = h_mt(A, p, "^(/[^/]+/[^/]+)(/.+)$");
    if (!w.ok) return 0;
    const char *ob = w.gr[1], *rest = w.gr[2];
    int art = 0;
    while (*rest == '/') {
        rest++;
        const char *e = strchr(rest, '/');
        const char *teil = e ? a_kopie_n(A, rest, (size_t)(e - rest)) : rest;
        dliste *dl = NULL;
        VERSUCHE { dl = s_liste(k->S, ob, false); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); dl = NULL; } ENDE_VERSUCHE;
        if (!dl) return -1;
        art = 0;
        for (int i = 0; i < dl->n; i++) if (!strcmp(dl->e[i].name, teil)) {
            if (dl->e[i].ist_link) return 0;
            if (gr) *gr = dl->e[i].groesse;
            art = dl->e[i].ist_ordner ? 2 : 1;
            break;
        }
        if (!art) return 0;
        if (!e) break;
        if (art != 2) return 0;
        ob = VERB(A, ob, "/", teil); rest = e;
    }
    return art;
}
static void beleg_datei(kern *k, bliste *l, const char *p, long long gr, int kopf)
{
    if (!h_m(h_blatt(A, p), "\\.(elf|self|bin)$")) return;
    const char *sch = a_fmt(A, "%d|%s", kopf, p);
    if (mt_hat(&l->gesehen, sch)) return;
    mt_setze(A, &l->gesehen, sch, (void *)1);
    const unsigned char *d; size_t dn;
    if (!s_kopf(k->S, p, 4, &d, &dn)) { l->offen[kopf]++; return; }   /* Codex-Prüfung 57: nicht lesbar = offen */
    if (dn < 4) return;
    bool elf = d[0] == 0x7F && d[1] == 'E' && d[2] == 'L' && d[3] == 'F', self = d[0] == 0x4F && d[1] == 0x15 && d[2] == 0x3D && d[3] == 0x1D;
    if (!elf && !self) return;
    if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 32; bdatei *neu = a_hole(A, sizeof(bdatei) * (size_t)l->cap); if (l->n) memcpy(neu, l->e, sizeof(bdatei) * (size_t)l->n); l->e = neu; }
    l->e[l->n++] = (bdatei){ p, kopf, gr, gr > BELEG_DATEI_MAX };   /* zu große Programme zählen als "nicht gelesen" */
}
static void beleg_pfad(kern *k, bliste *l, const char *p, int kopf)   /* Datei: sie selbst; Ordner: seine oberste Ebene */
{
    long long gr = 0;
    int art = beleg_art(k, p, &gr);
    if (art < 0) { l->offen[kopf] = BELEG_UNBEKANNT; return; }
    if (art == 1) { beleg_datei(k, l, p, gr, kopf); return; }
    if (art != 2) return;
    dliste *dl = NULL;
    VERSUCHE { dl = s_liste(k->S, p, false); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); dl = NULL; } ENDE_VERSUCHE;
    if (!dl) { l->offen[kopf] = BELEG_UNBEKANNT; return; }
    for (int i = 0; i < dl->n; i++) if (!dl->e[i].ist_ordner && !dl->e[i].ist_link) beleg_datei(k, l, VERB(A, p, "/", dl->e[i].name), dl->e[i].groesse, kopf);
}
/* Suche in den Programmdateien (Codex-Prüfung 56/57/59): EIN Durchgang je Datei, Aufwand linear in der Dateigröße - egal wie viele
 * Einträge gesucht werden. Gefunden wird
 *  - "/data/<Weg>" bzw. "/user/data/<Weg>" mit Grenze links (Anfang der Datei, \0 oder Anführungszeichen davor - "/mnt/usb0/data/X" und
 *    "/mnt/usb0/user/data/X" zählen also nicht) und rechts (\0, "/" oder Anführungszeichen): jeder Teil-Weg bis zu einem "/" wird in einer
 *    Streutabelle nachgeschlagen ("PCSX2/bios/x" -> "PCSX2", "PCSX2/bios", ...);
 *  - Punkt-Namen (".orbit-drive") mit \0, "/" oder Anführungszeichen davor und \0, "/" oder Anführungszeichen dahinter.
 * Gelesen wird in Stücken zu 1 MB; zwischen zwei Stücken wird das Abbrechen geprüft. */
#define BELEG_STUECK (1u << 20)
#define BELEG_WEG_MAX 1000
#define BELEG_REST (BELEG_WEG_MAX + 100)    /* so viel bleibt vom Ende eines Stücks für das nächste stehen */
typedef struct { const char *k; size_t kl; int c; } bh_eintrag;
typedef struct { bh_eintrag *e; size_t cap; } bhash;
static uint32_t bh_wert(uint32_t h, unsigned char c) { return (h ^ c) * 16777619u; }
#define BH_SCHRITTE 64   /* Codex-Prüfung 60: jede Suche in der Streutabelle höchstens so viele Schritte - Aufwand bleibt begrenzt */
static bool bh_rein(bhash *t, const char *k, int c, int *naechster)   /* false: passt nicht (wird dann nicht geprüft) */
{
    uint32_t h = 2166136261u; size_t kl = strlen(k);
    for (size_t i = 0; i < kl; i++) h = bh_wert(h, (unsigned char)k[i]);
    size_t i = h & (t->cap - 1);
    for (int s = 0; s < BH_SCHRITTE; s++, i = (i + 1) & (t->cap - 1)) {
        if (!t->e[i].k) { t->e[i] = (bh_eintrag){ k, kl, c }; return true; }
        if (t->e[i].kl == kl && !memcmp(t->e[i].k, k, kl)) { naechster[c] = t->e[i].c; t->e[i].c = c; return true; }   /* gleicher Name: Kette */
    }
    return false;
}
static int bh_hole(const bhash *t, uint32_t h, const char *k, size_t kl)
{
    if (!t->cap) return -1;
    size_t i = h & (t->cap - 1);
    for (int s = 0; s < BH_SCHRITTE && t->e[i].k; s++, i = (i + 1) & (t->cap - 1))
        if (t->e[i].kl == kl && !memcmp(t->e[i].k, k, kl)) return t->e[i].c;
    return -1;
}
typedef struct {
    kern *k; bhash wege, punkte; int *naechster; bool *traf;
    unsigned char *b; size_t rn, schon; bool abbruch;
} bscan;
static void beleg_treffer(bscan *st, int c) { for (; c >= 0; c = st->naechster[c]) st->traf[c] = true; }
static bool links_frei(const unsigned char *b, size_t q) { return q == 0 || b[q - 1] == 0 || b[q - 1] == '"'; }
static void beleg_bereich(bscan *st, const unsigned char *b, size_t len, size_t von, size_t bis)
{
    for (size_t p = von; p < bis; ) {
        const unsigned char *t = basis_suche(b + p, len - p, "/data/", 6);
        if (!t) break;
        size_t q = (size_t)(t - b);
        if (q >= bis) break;
        p = q + 1;
        bool frei = (q >= 5 && !memcmp(b + q - 5, "/user", 5)) ? links_frei(b, q - 5) : links_frei(b, q);
        if (!frei) continue;
        size_t i = q + 6; uint32_t h = 2166136261u;
        const char *w = (const char *)(b + i);
        while (i < len && i - (q + 6) < BELEG_WEG_MAX && b[i] != 0 && b[i] != '"') {
            if (b[i] == '/' && i > q + 6) { int c = bh_hole(&st->wege, h, w, i - (q + 6)); if (c >= 0) beleg_treffer(st, c); }
            h = bh_wert(h, b[i]); i++;
        }
        if (i - (q + 6) >= BELEG_WEG_MAX) { p = i; continue; }
        if (i > q + 6) { int c = bh_hole(&st->wege, h, w, i - (q + 6)); if (c >= 0) beleg_treffer(st, c); }
        p = i;                                              /* weiter hinter dem Weg - jedes Byte höchstens zweimal angesehen */
    }
    if (!st->punkte.cap) return;
    for (size_t p = von; p < bis; ) {
        const unsigned char *t = memchr(b + p, '.', bis - p);
        if (!t) break;
        size_t q = (size_t)(t - b);
        p = q + 1;
        if (!(q == 0 || b[q - 1] == 0 || b[q - 1] == '/' || b[q - 1] == '"')) continue;
        size_t i = q; uint32_t h = 2166136261u;
        while (i < len && i - q < 256 && b[i] != 0 && b[i] != '/' && b[i] != '"') { h = bh_wert(h, b[i]); i++; }
        if (i - q < 2 || i - q >= 256) { p = i > q ? i : q + 1; continue; }
        int c = bh_hole(&st->punkte, h, (const char *)(b + q), i - q);
        if (c >= 0) beleg_treffer(st, c);
        p = i;
    }
}
static bool beleg_weiter(void *c, const unsigned char *d, size_t n)
{
    bscan *st = c;
    if (st->k->abbruch) { st->abbruch = true; return false; }   /* Codex-Prüfung 59: zwischen zwei Stücken */
    if (d && n > BELEG_STUECK) {                               /* Codex-Prüfung 60: nie mehr als ein Stück auf einmal in den Puffer */
        for (size_t o = 0; o < n; o += BELEG_STUECK) if (!beleg_weiter(c, d + o, n - o < BELEG_STUECK ? n - o : BELEG_STUECK)) return false;
        return true;
    }
    bool ende = d == NULL;
    if (!ende) memcpy(st->b + st->rn, d, n);
    size_t len = st->rn + (ende ? 0 : n);
    size_t bis = ende ? len : (len > BELEG_REST ? len - BELEG_REST : 0);
    if (bis > st->schon) beleg_bereich(st, st->b, len, st->schon, bis);
    size_t neu = bis > st->schon ? bis : st->schon;
    size_t ab = neu > 8 ? neu - 8 : 0;                     /* 8 Bytes davor bleiben für die Grenze links ("/user", \0) */
    memmove(st->b, st->b + ab, len - ab);
    st->rn = len - ab; st->schon = neu - ab;
    return true;
}
static void beleg_aus_programmen(kern *k, pliste *liste, int n, int *zu, const char **kid)
{
    /* 1. Kandidaten (höchstens BELEG_KANDIDATEN - EIN Durchgang, damit die Lesegrenze für die ganze Suche gilt; Codex-Prüfung 57) */
    int *kand = a_hole(A, sizeof(int) * BELEG_KANDIDATEN), nk = 0;
    const char **was = a_hole(A, sizeof(char *) * BELEG_KANDIDATEN);
    bool *punkt = a_hole(A, sizeof(bool) * BELEG_KANDIDATEN);
    for (int d = 0; d < n && nk < BELEG_KANDIDATEN; d++) {
        eintrag *f = liste->e[d];
        if (zu[d] >= 0 || kid[d] || f->Jailbreak || f->SmDatei || f->Teil || f->Kachel || f->Gesperrt || f->PfadeBasis.n != 1) continue;
        if (f->Kat != 5 && f->Kat != 6) continue;
        const char *p = f->PfadeBasis.e[0], *x = NULL;
        bool pk = false;
        htreffer md = h_mt(A, p, "^/user/data/(homebrew/)?([^/]+)$");
        htreffer mr = h_mt(A, p, "^/mnt/(?:ext\\d+|usb\\d+)/(\\.[^/]+)$");
        if (md.ok) {
            const char *y = md.gr[2];
            if (h_m(y, "^[A-Z]{4}\\d{5}") || h_gleich(y, "homebrew") || (!md.gr[1][0] && tool_bekannt(k, y))) continue;
            x = VERB(A, md.gr[1], y);                   /* hinter "/data/" */
        } else if (mr.ok) { x = mr.gr[1]; pk = true; }
        if (!x || strlen(x) > 1000 || h_m(h_blatt(A, p), "\\.(elf|self|bin|pkg)$")) continue;
        kand[nk] = d; was[nk] = x; punkt[nk] = pk; nk++;
    }
    if (nk == 0) return;
    /* 2. Programmdateien je Kachel - Verknüpfungen nie (beleg_art); was sich nicht lesen lässt, zählt als offen */
    int *offen = a_hole(A, sizeof(int) * (size_t)n);
    memset(offen, 0, sizeof(int) * (size_t)n);
    bliste l = { NULL, 0, 0, mt_neu(true), offen };
    for (int t = 0; t < n; t++) {
        eintrag *g = liste->e[t];
        if (!kid[t] || g->Gesperrt) continue;
        for (int j = 0; j < g->PfadeBasis.n; j++) if (h_m(g->PfadeBasis.e[j], "^(/mnt/ext\\d+)?/user/app/[A-Z]{4}\\d{5}$")) beleg_pfad(k, &l, g->PfadeBasis.e[j], t);
        if (g->Quelle && h_m(g->Quelle, "^/user/data/")) beleg_pfad(k, &l, g->Quelle, t);
        for (int d = 0; d < n; d++) {
            eintrag *f = liste->e[d];
            if (zu[d] != t || f->Gesperrt) continue;
            for (int j = 0; j < f->PfadeBasis.n; j++) beleg_pfad(k, &l, f->PfadeBasis.e[j], t);
        }
    }
    if (l.n == 0) return;
    /* nur Kacheln mit Programmen bekommen eine Spalte (Codex-Prüfung 57: Speicher) */
    int *spalte = a_hole(A, sizeof(int) * (size_t)n), ns = 0;
    for (int t = 0; t < n; t++) spalte[t] = -1;
    for (int i = 0; i < l.n; i++) if (spalte[l.e[i].kopf] < 0) spalte[l.e[i].kopf] = ns++;
    int *zahl = a_hole(A, sizeof(int) * (size_t)nk * (size_t)ns);
    memset(zahl, 0, sizeof(int) * (size_t)nk * (size_t)ns);
    /* 3. lesen: jede Datei einmal, ein Durchgang; nicht gelesene Programme zählen als offen */
    bscan st = { k, { NULL, 0 }, { NULL, 0 }, NULL, NULL, NULL, 0, 0, false };
    size_t cap = 16; while (cap < (size_t)nk * 2) cap <<= 1;
    st.wege.e = a_hole(A, sizeof(bh_eintrag) * cap); memset(st.wege.e, 0, sizeof(bh_eintrag) * cap); st.wege.cap = cap;
    st.naechster = a_hole(A, sizeof(int) * (size_t)nk);
    bool punkte = false;
    for (int c = 0; c < nk; c++) { st.naechster[c] = -1; if (punkt[c]) punkte = true; }
    if (punkte) { st.punkte.e = a_hole(A, sizeof(bh_eintrag) * cap); memset(st.punkte.e, 0, sizeof(bh_eintrag) * cap); st.punkte.cap = cap; }
    bool *geprueft = a_hole(A, sizeof(bool) * (size_t)nk);
    for (int c = 0; c < nk; c++) geprueft[c] = bh_rein(punkt[c] ? &st.punkte : &st.wege, was[c], c, st.naechster);
    st.traf = a_hole(A, sizeof(bool) * (size_t)nk);
    st.b = malloc(BELEG_REST + 8 + BELEG_STUECK);
    if (!st.b) return;                                            /* kein Speicher: nichts zuordnen */
    long long summe = 0;
    volatile int fehler_art = 0;
    VERSUCHE {
        for (int i = 0; i < l.n; i++) {
            if (l.e[i].zu_gross || summe + l.e[i].gr > BELEG_GESAMT_MAX) { offen[l.e[i].kopf]++; continue; }
            summe += l.e[i].gr;
            st.rn = 0; st.schon = 0; st.abbruch = false;
            memset(st.traf, 0, sizeof(bool) * (size_t)nk);
            int r = s_stuecke(k->S, l.e[i].pfad, BELEG_STUECK, beleg_weiter, &st);
            if (st.abbruch) wirf(AUSN_ABBRUCH, "");
            if (r < 0) { offen[l.e[i].kopf]++; continue; }
            for (int c = 0; c < nk; c++) if (st.traf[c]) zahl[c * ns + spalte[l.e[i].kopf]]++;
        }
    } FANGE { fehler_art = ausn_art == AUSN_ABBRUCH ? 1 : 2; } ENDE_VERSUCHE;
    free(st.b);
    if (fehler_art == 1) wirf(AUSN_ABBRUCH, "");
    if (fehler_art == 2) return;                                   /* etwas ging schief: nichts zuordnen */
    /* 4. die Kachel mit den meisten Belegen - nur wenn sie sicher vorne liegt: keine andere Kachel käme mit ihren nicht gelesenen
     *    Programmen auch nur auf Gleichstand */
    for (int c = 0; c < nk; c++) {
        int beste = -1, max = 0;
        for (int t = 0; t < n; t++) if (spalte[t] >= 0 && zahl[c * ns + spalte[t]] > max) { max = zahl[c * ns + spalte[t]]; beste = t; }
        if (beste < 0 || !geprueft[c]) continue;
        bool sicher = true;
        for (int t = 0; t < n && sicher; t++) if (t != beste && (spalte[t] >= 0 ? zahl[c * ns + spalte[t]] : 0) + offen[t] >= max) sicher = false;
        if (sicher) zu[kand[c]] = beste;
    }
}
static void kachel_bloecke(kern *k, pliste *liste)
{
    int n = liste->n;
    int *zu = a_hole(A, sizeof(int) * (size_t)(n > 0 ? n : 1));
    for (int i = 0; i < n; i++) zu[i] = -1;
    /* Köpfe: Kennung, Name, Alias */
    const char **kid = a_hole(A, sizeof(char *) * (size_t)(n > 0 ? n : 1)), **ktk = a_hole(A, sizeof(char *) * (size_t)(n > 0 ? n : 1));
    const char **ktt = a_hole(A, sizeof(char *) * (size_t)(n > 0 ? n : 1));
    for (int i = 0; i < n; i++) {
        eintrag *g = liste->e[i]; kid[i] = NULL; ktk[i] = ""; ktt[i] = "";
        if (!ist_block_kopf(g)) continue;
        kid[i] = kopf_kennung(g);
        ktt[i] = kopf_titel(k, g, kid[i]);
        ktk[i] = tool_schluessel(k, ktt[i]);
    }
    for (int d = 0; d < n; d++) {
        eintrag *f = liste->e[d];
        if (kid[d] || f->Jailbreak || f->SmDatei || f->Teil || f->Kachel || f->PfadeBasis.n == 0) continue;
        if (f->Kat != 1 && f->Kat != 5 && f->Kat != 6) continue;
        /* alle Pfade müssen demselben gehören */
        const char *oc = NULL; bool gleich = true;
        for (int j = 0; j < f->PfadeBasis.n && gleich; j++) {
            const char *b = besitzer_teil(k, f->PfadeBasis.e[j]);
            if (!b) gleich = false; else if (!oc) oc = b; else if (strcmp(oc, b) != 0) gleich = false;
        }
        if (!gleich || !oc) continue;
        /* 0. Codex-Prüfung 40 (1) / 41 (2): eine SPIELDATEI (PKG, Spiel-Abbild - oder ein Eintrag unter "Spiele") mit Spielnummer im
         * eigenen Namen gehört zu dem Spiel mit dieser Nummer, egal in welchem Ordner sie liegt (.../OrbitStore/Spiel-CUSA31001.pkg).
         * Gibt es dieses Spiel nicht (genau einmal), bleibt sie für sich - nie unter der App, nach der ihr Ordner heißt. Gewöhnliche
         * App-Ordner mit einer Nummer im Namen (z. B. playden/cache-CUSA31001) gehen den normalen Weg über ihren Besitzer. */
        {
            const char *bid = NULL; bool alle = true;
            for (int j = 0; j < f->PfadeBasis.n && alle; j++) {
                const char *bl = h_blatt(A, f->PfadeBasis.e[j]);
                htreffer mb = h_mt(A, bl, "(?-i)(?:^|[^A-Za-z0-9])([A-Z]{4}\\d{5})(?:[^0-9]|$)");
                if (!mb.ok || !(f->Kat == 1 || h_m(bl, "(?i)\\.(pkg|exfat|ffpkg|ffpfs|ffpfsc|pfs|img|iso)$"))) { alle = false; continue; }
                if (!bid) bid = mb.gr[1]; else if (strcmp(bid, mb.gr[1]) != 0) alle = false;
            }
            if (alle && bid) {
                int t1 = -1, a1 = 0;
                for (int t = 0; t < n; t++) if (kid[t] && !strcmp(kid[t], bid)) { t1 = t; a1++; }
                if (a1 == 1) zu[d] = t1;
                continue;                          /* Spiel nicht (eindeutig) da: nicht raten */
            }
        }
        /* 0b. Codex-Prüfung 41 (1): liegt der Eintrag IN seinem Besitzer-Ordner (z. B. /mnt/ext1/OrbitStore/pldmgr_v0.6.3.elf) und heißt er
         * selbst GENAU wie eine Kachel (gleicher Name oder feste Zuordnung), gehört er zu dieser Kachel - nicht zu der, nach der der Ordner heißt */
        if (f->PfadeBasis.n == 1 && f->Kat != 1) {
            const char *bl = h_blatt(A, f->PfadeBasis.e[0]);
            if (strcmp(bl, oc) != 0) {
                htreffer pb = h_mt(A, bl, "^(.+?)(?:[-_ ]v?\\d[\\w.\\-]*)?\\.(?:elf|bin)$");
                const char *sb = pb.ok ? pb.gr[1] : (name_ist_datei(bl) ? h_r(A, bl, "\\.[A-Za-z][A-Za-z0-9]{0,7}$", "") : bl);
                const char *kb = tool_schluessel(k, sb);
                int t1 = -1, a1 = 0;
                for (int t = 0; t < n; t++) if (kid[t] && name_stufe(ktk[t], ktt[t], tool_alias(kid[t]), kb, sb) == 1) { t1 = t; a1++; }
                if (a1 == 1) { zu[d] = t1; continue; }
            }
        }
        /* 1. Kennung im Namen */
        htreffer mi = h_mt(A, oc, "(?-i)(?:^|[^A-Za-z0-9])([A-Z]{4}\\d{5})(?:[^0-9]|$)");
        if (mi.ok) {
            int t1 = -1, a1 = 0;
            for (int t = 0; t < n; t++) if (kid[t] && !strcmp(kid[t], mi.gr[1])) { t1 = t; a1++; }
            if (a1 == 1) zu[d] = t1;
            continue;                              /* eine Spielnummer, die zu keiner (oder mehreren) Kachel(n) passt: nicht raten */
        }
        if (f->Kat == 1) continue;                 /* Spiele nur über die Kennung */
        /* 2. Name */
        bool datei = name_ist_datei(oc);
        htreffer pm = h_mt(A, oc, "^(.+?)(?:[-_ ]v?\\d[\\w.\\-]*)?\\.(?:elf|bin)$");
        bool payload = pm.ok;
        const char *stamm = payload ? pm.gr[1] : (datei ? h_r(A, oc, "\\.[A-Za-z][A-Za-z0-9]{0,7}$", "") : oc);
        const char *dk = tool_schluessel(k, stamm);
        int t1 = kopf_fuer_namen(liste, n, kid, ktk, ktt, dk, stamm, f->Kat);
        /* NEU 1.0.7 (PS5-Test 03.11.2026): "ps5-" vorne sagt nur, für welche Konsole es ist (ps5-web-file-manager_v1.11b.elf -> Kachel
         * "Web File Manager") - zählt nur, wenn mit dem vollen Namen gar nichts passt */
        if (t1 == -1) { htreffer mp = h_mt(A, stamm, "^ps5[-_ ](.+)$"); if (mp.ok) t1 = kopf_fuer_namen(liste, n, kid, ktk, ktt, tool_schluessel(k, mp.gr[1]), mp.gr[1], f->Kat); }
        if (t1 >= 0) zu[d] = t1;
    }
    /* NEU 1.0.7 (PS5-Test 03.11.2026: ps5upload_startup.log stand unter Sonstiges, der Ordner ps5upload unter "PS5Upload Minion"): eine
     * Datei direkt in /user/data, deren Name mit dem Namen eines Daten-Ordners daneben beginnt (dahinter _ - oder .), gehört zu derselben
     * Kachel wie dieser Ordner. Bei zwei gleich langen passenden Ordnern verschiedener Kacheln: nicht raten. */
    for (int d = 0; d < n; d++) {
        eintrag *f = liste->e[d];
        if (zu[d] >= 0 || kid[d] || f->Jailbreak || f->SmDatei || f->Teil || f->Kachel || f->Gesperrt || f->PfadeBasis.n != 1) continue;
        if (f->Kat != 5 && f->Kat != 6) continue;
        htreffer mf = h_mt(A, f->PfadeBasis.e[0], "^/user/data/([^/]+)$");
        if (!mf.ok || !name_ist_datei(mf.gr[1])) continue;
        int best = -1; size_t bl = 0; bool zwei = false;
        for (int e = 0; e < n; e++) {
            if (e == d || zu[e] < 0) continue;
            eintrag *g = liste->e[e];
            if (g->PfadeBasis.n != 1) continue;
            htreffer mg = h_mt(A, g->PfadeBasis.e[0], "^/user/data/([^/]+)$");
            if (!mg.ok || name_ist_datei(mg.gr[1])) continue;
            size_t lg = strlen(mg.gr[1]);
            if (lg < 3 || strlen(mf.gr[1]) <= lg || !gleich_ci_n(mf.gr[1], mg.gr[1], lg)) continue;
            char c = mf.gr[1][lg];
            if (c != '_' && c != '-' && c != '.') continue;
            if (lg > bl) { best = zu[e]; bl = lg; zwei = false; } else if (lg == bl && zu[e] != best) zwei = true;
        }
        if (best >= 0 && !zwei) zu[d] = best;
    }
    beleg_aus_programmen(k, liste, n, zu, kid);
    /* NEU 1.0.7 (da_Chris86 03.11.2026 17:12: "das mit den Trophys genauso machen, dass die Liste und der Fortschritt untereinander ist"):
     * je Spiel EIN Block unter "Trophäen" - oben die (erste) Trophäen-Liste, darunter weitere Listen und der Fortschritt aller Konten.
     * Zusammen gehört, was dieselbe Trophäen-Nummer (NPWR...) hat oder laut npbind.dat zum selben Spiel gehört. Beides sind getrennte
     * Dateien (Liste = Namen/Bilder, kommt beim Spielstart wieder; Fortschritt = deine Trophäen, einzige Kopie) - jede Zeile behält ihren
     * eigenen Haken und ihre eigene Ampel. Gibt es keine Liste mehr, steht Fortschritt nur unter dem Fortschritt DESSELBEN Kontos
     * (Codex-Prüfung 40 (5)). Bis 02.11.2026 standen nur mehrere Listen eines Spiels zusammen. */
    {
        const char **tk = a_hole(A, sizeof(char *) * (size_t)(n > 0 ? n : 1)), **tu = a_hole(A, sizeof(char *) * (size_t)(n > 0 ? n : 1));
        bool *tl = a_hole(A, sizeof(bool) * (size_t)(n > 0 ? n : 1));
        for (int d = 0; d < n; d++) {
            eintrag *f = liste->e[d];
            tk[d] = NULL; tu[d] = ""; tl[d] = false;
            if (f->Kat != 3 || f->PfadeBasis.n == 0 || zu[d] >= 0) continue;
            bool la = txt_beginnt(f->TextBasis, T("Trophäen-Liste: ", "Trophy list: "));
            if (!la && !txt_beginnt(f->TextBasis, T("Trophäen-Fortschritt: ", "Trophy progress: "))) continue;
            /* Codex-Prüfung 62: eine Liste kann an mehreren Orten liegen (trophy/conf, trophy2/nobackup/conf, np_uds) - alle Pfade dieselbe Nummer */
            htreffer mn = h_mt(A, f->PfadeBasis.e[0], "(?-i)/(NPWR\\d{5}_\\d{2})$");
            bool eine = mn.ok;
            for (int j = 1; j < f->PfadeBasis.n && eine; j++) { htreffer mj = h_mt(A, f->PfadeBasis.e[j], "(?-i)/(NPWR\\d{5}_\\d{2})$"); eine = mj.ok && !strcmp(mj.gr[1], mn.gr[1]); }
            if (!eine || (!la && f->PfadeBasis.n != 1)) continue;
            tk[d] = mt_hat(&k->NPWR_KACHEL, mn.gr[1]) ? VERB(A, "S|", (const char *)mt_hole(&k->NPWR_KACHEL, mn.gr[1])) : VERB(A, "N|", mn.gr[1]);
            tl[d] = la;
            if (!la) { htreffer mu = h_mt(A, f->PfadeBasis.e[0], "^/user/home/([^/]+)/"); tu[d] = mu.ok ? mu.gr[1] : ""; }
        }
        /* Kopf je Spiel: die Liste mit der kleinsten Nummer (bei Gleichstand die zuerst gefundene) */
        for (int d = 0; d < n; d++) {
            if (!tk[d]) continue;
            int kopf = -1;
            for (int t = 0; t < n; t++) {
                if (!tk[t] || !tl[t] || strcmp(tk[t], tk[d]) != 0) continue;
                eintrag *g = liste->e[t];
                if (kopf < 0 || g->ListeNr < ((eintrag *)liste->e[kopf])->ListeNr) kopf = t;
            }
            if (kopf < 0) {   /* keine Liste: Fortschritt unter dem ersten Fortschritt desselben Kontos */
                for (int t = 0; t < n; t++) {
                    if (!tk[t] || tl[t] || strcmp(tk[t], tk[d]) != 0 || strcmp(tu[t], tu[d]) != 0) continue;
                    eintrag *g = liste->e[t];
                    if (kopf < 0 || g->ListeNr < ((eintrag *)liste->e[kopf])->ListeNr) kopf = t;
                }
            }
            if (kopf >= 0 && kopf != d) zu[d] = kopf;
        }
    }
    /* NEU 1.0.7 (03.11.2026): vorher merken, welche Einträge Trophäen-Listen sind (der Text bekommt unten das Einrücken davor) */
    bool *tr_liste = a_hole(A, sizeof(bool) * (size_t)(n > 0 ? n : 1));
    for (int d = 0; d < n; d++) { eintrag *f = liste->e[d]; tr_liste[d] = f->Kat == 3 && txt_beginnt(f->TextBasis, T("Trophäen-Liste: ", "Trophy list: ")); }
    pliste neu = {0};
    for (int i = 0; i < n; i++) {
        if (zu[i] >= 0) continue;
        pl_add(A, &neu, liste->e[i]);
        /* Teile nach Ort (intern, M.2, USB-Speicher, USB-Stick), darin in Fundreihenfolge - sonst stünde z. B. eine ELF vom Stick
         * (vorher "Apps") vor den Daten auf der internen SSD (vorher "Sonstiges"), weil die Liste vorher nach Bereichen sortiert war */
        /* NEU 1.0.7 (03.11.2026): unter einer Trophäen-Liste zuerst die weiteren Listen, dann der Fortschritt (Durchgang 0 = Listen) */
        for (int dg = 0; dg < 2; dg++)
        for (int o = -1; o <= ORT_STICK; o++)
        for (int d = 0; d < n; d++) if (zu[d] == i) {
            eintrag *f = liste->e[d], *h = liste->e[i];
            bool ort_ok = (f->Ort >= ORT_INTERN && f->Ort <= ORT_STICK) ? f->Ort == o : o == -1;
            if (!ort_ok) continue;
            if ((dg == 0) != tr_liste[d]) continue;
            f->Text = VERB(A, "   – ", f->Text); f->TextBasis = VERB(A, "   – ", f->TextBasis);
            f->ToolKachel = h;
            if (h->KachelId) { f->KatHer = f->Kat; f->Kat = h->Kat; }   /* unter ihre Kachel - Trophäen bleiben, wo sie sind */
            pl_add(A, &neu, f);
        }
    }
    *liste = neu;
}
/* NEU 1.0.7 (PS5-Test 02.11.2026): Datenbank-Kopien, die ältere Versionen von PS5 Cleanup angelegt haben (ps5cleanup_db_<Zeit>_…),
 * stehen als EIN Eintrag je Speicher unter "Apps" - "PS5 Cleanup – alte Sicherungen" (bis hier je Datei unter "Sonstiges" mit dem falschen
 * Satz "neu herunterladen"). Eigene Dateinamen von PS5 Cleanup, keine Namensliste fremder Apps. */
static bool ist_alte_sicherung(const eintrag *f)
{
    if (f->PfadeBasis.n == 0) return false;
    for (int j = 0; j < f->PfadeBasis.n; j++) if (!h_m(f->PfadeBasis.e[j], "/ps5cleanup_db_[^/]*$")) return false;
    return true;
}
static void alte_sicherungen_zusammen(kern *k)
{
    pliste neu = {0};
    eintrag *je_ort[ORT_STICK + 1] = { NULL };
    for (int i = 0; i < k->bau.n; i++) {
        eintrag *f = k->bau.e[i];
        if (f->Gesperrt || f->Jailbreak || !ist_alte_sicherung(f) || f->Ort < 0 || f->Ort > ORT_STICK) { pl_add(A, &neu, f); continue; }
        eintrag *z = je_ort[f->Ort];
        if (!z) {
            z = a_hole(A, sizeof *z); *z = *f;
            z->Pfade = sl_kopie(A, &f->Pfade); z->PfadeBasis = sl_kopie(A, &f->PfadeBasis); z->Kat = 6;
            je_ort[f->Ort] = z; pl_add(A, &neu, z);
        } else { sl_add_alle(A, &z->Pfade, &f->Pfade); sl_add_alle(A, &z->PfadeBasis, &f->PfadeBasis); z->NurEinzeln = z->NurEinzeln || f->NurEinzeln; }
    }
    for (int o = 0; o <= ORT_STICK; o++) {
        eintrag *z = je_ort[o]; if (!z) continue;
        int nd = z->PfadeBasis.n;
        z->Text = z->TextBasis = VERB(A, kern_ort_vorsatz(o), T("PS5 Cleanup – alte Sicherungen (", "PS5 Cleanup – old backups ("), a_fmt(A, "%d", nd),
                                      nd == 1 ? T(" Datei)", " file)") : T(" Dateien)", " files)"));
    }
    k->bau = neu;
}
/* NEU 1.0.7: Bereiche nach der Suche festlegen - Apps aus "Spiele" heraus (Sony sagt, ob Spiel oder App), Daten-Ordner ohne
 * bekanntes Tool nach "Sonstiges". Jailbreak-Einstiege sind schon unter 7. Nur der Bereich ändert sich - Pfade und Schutz bleiben. */
static void bereiche_107(kern *k)
{
    for (int i = 0; i < k->bau.n; i++) {
        eintrag *f = k->bau.e[i];
        /* jeder Jailbreak-Einstieg unter "Jailbreak" - auch die, die schon beim Merken markiert wurden (z. B. gemeinsame Spieldatei, Codex-Prüfung 37) */
        if (f->Jailbreak) { f->Kat = 7; continue; }
        if (f->Gesperrt) continue;
        if (f->Kat == 1 && !(f->Quelle && f->Quelle[0])) {
            for (int j = 0; j < f->PfadeBasis.n; j++) {
                htreffer m = h_mt(A, f->PfadeBasis.e[j], "/(app|patch|addcont)/([A-Z]{4}\\d{5})(/|$)");
                if (!m.ok) continue;
                app_titel(k, m.gr[2]);
                if (k->APP_ART == 2) f->Kat = 6;
                break;
            }
        }
        else if (f->Kat == 6 && f->Ort == ORT_INTERN && !f->Kachel && f->PfadeBasis.n > 0) {
            htreffer m = h_mt(A, f->PfadeBasis.e[0], "^/user/data/(homebrew/)?([^/]+)$");
            if (!m.ok || h_m(m.gr[2], "(?i)\\.elf$")) continue;   /* ELF-Dateien bleiben unter Apps (Codex-Prüfung 39) */
            /* 02.11.2026: ob der Ordner zu einer Kachel gehört, entscheidet danach kachel_bloecke (gleiche Regeln für alle Kacheln) - er
             * kommt dann von hier aus unter seine Kachel; was zu keiner gehört, bleibt unter "Sonstiges" */
            if (tool_bekannt(k, m.gr[2])) continue;
            f->Kat = 5;
        }
    }
}

/* NEU 1.0.7 (da_Chris86 31.10.2026 12:49): ShadowMount-Kachel und Spieldatei als zwei Zeilen. Die Zeile "Spieldatei" steht direkt
 * hinter ihrer Kachel und wird nie selbst gelöscht - ist sie gewählt, nimmt die Kachel beim Löschen die Datei mit ("Spiel + Kachel",
 * kern_sm_nach_auswahl). Nur für eindeutig zugeordnete Spieldateien (f->Quelle gesetzt - mehrdeutige sind schon "nur Kachel"). */
static void sm_spieldatei_zeilen(kern *k)
{
    pliste neu = {0};
    for (int i = 0; i < k->bau.n; i++) {
        eintrag *f = k->bau.e[i];
        pl_add(A, &neu, f);
        if (!f->Quelle || !f->Quelle[0] || f->SmDatei) continue;
        eintrag *d = a_hole(A, sizeof *d);
        d->Pfade = L1(A, f->Quelle); d->PfadeBasis = sl_kopie(A, &d->Pfade);
        d->Kat = f->Kat; d->Quelle = ""; d->Gesperrt = f->Gesperrt; d->SperrGrund = f->SperrGrund;
        d->Ort = kern_pfad_ort(f->Quelle); d->Jailbreak = f->Jailbreak;
        d->NurEinzeln = f->NurEinzeln || d->Ort == ORT_STICK;   /* Spieldatei auf dem USB-Stick: nur einzeln (wie bisher "Spiel + Kachel" nur ausdrücklich) */
        d->SmDatei = true; d->SmKachel = f;
        const char *v = kern_ort_vorsatz(f->Ort), *t = txt_beginnt(f->TextBasis, v) ? f->TextBasis + strlen(v) : f->TextBasis;
        int p = txt_index(t, ") - ");
        const char *name = p >= 0 ? a_kopie_n(A, t, (size_t)p + 1) : t;
        int sl = txt_letzter(f->Quelle, '/');
        d->Text = d->TextBasis = VERB(A, kern_ort_vorsatz(d->Ort), name, T(" - Spieldatei ", " - game file "), f->Quelle + (sl >= 0 ? sl + 1 : 0),
                                     f->Jailbreak ? kern_nur_einzeln_text() : "");
        pl_add(A, &neu, d);
    }
    k->bau = neu;
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
    {   /* NEU 1.0.7 (PS5-Test 02.11.2026): Zuordnungen und kaputte Zusatzinhalt-Angaben aus Metadaten der installierten Spiele und Apps */
        const sliste *inst[3] = { &apps, &m2Apps, &usbApps };
        verknuepfungen_ermitteln(k, inst, 3);
    }
    /* Zuordnung Trophäen-Kennung (NPWR...) -> Spielname über die Kachel-Infos */
    k->NPWR_NAMEN = mt_neu(true);
    k->NPWR_KACHEL = mt_neu(true);
    k->NPWR_NR = mt_neu(true);
    for (int i = 0; i < sysmeta.n; i++) {
        const char *id = sysmeta.e[i];
        if (!h_m(id, "^[A-Z]{4}\\d{5}$")) continue;
        /* NEU 1.0.7 (PS5-Test 01.11.2026: die Trophäen von Red Dead Redemption 2 und L.A. Noire galten als "Spiel nicht mehr auf der
         * Konsole"): bei PS4-Spielen liegt npbind.dat direkt im Kachel-Info-Ordner (/system_data/priv/appmeta/CUSA08519/npbind.dat) */
        const char *subs[3] = { "trophy2/", "trophy/", "" };
        for (int s = 0; s < (kern_bereiche_106() ? 2 : 3); s++) {
            const char *nb = s_text(k->S, VERB(A, SYS_META "/", id, "/", subs[s], "npbind.dat"));
            sliste ms = r_alle(A, nb, "NPWR\\d{5}_\\d{2}");
            if (ms.n == 0) continue;
            if (!mt_hat(&k->NAMEN_CACHE, id)) { mt_setze(A, &k->NAMEN_CACHE, id, (void *)app_titel(k, id)); if (ist_astro(k, mt_hole(&k->NAMEN_CACHE, id))) astro_merken(k, id); }
            if (h_hat(&k->ASTRO_IDS, id)) for (int j = 0; j < ms.n; j++) if (!h_hat(&k->ASTRO_NPWRS, ms.e[j])) sl_add(A, &k->ASTRO_NPWRS, ms.e[j]);
            for (int j = 0; j < ms.n; j++) if (!mt_hat(&k->NPWR_NAMEN, ms.e[j])) mt_setze(A, &k->NPWR_NAMEN, ms.e[j], mt_hole(&k->NAMEN_CACHE, id));
            for (int j = 0; j < ms.n; j++) if (!mt_hat(&k->NPWR_KACHEL, ms.e[j])) mt_setze(A, &k->NPWR_KACHEL, ms.e[j], (void *)a_kopie(A, id));
            /* NEU 1.0.7 (PS5-Test 02.11.2026: DRIVECLUB brachte zwei Listen mit - die zweite ist "DRIVECLUB BIKES"): hat ein Spiel mehrere
             * Trophäen-Listen, bekommen sie Nummern in der Reihenfolge, in der das Spiel sie nennt (npbind.dat) */
            if (!kern_bereiche_106()) {
                sliste u = {0}; for (int j = 0; j < ms.n; j++) if (!h_hat(&u, ms.e[j])) sl_add(A, &u, ms.e[j]);
                if (u.n > 1) for (int j = 0; j < u.n; j++) if (!mt_hat(&k->NPWR_NR, u.e[j])) mt_setze(A, &k->NPWR_NR, u.e[j], (void *)(intptr_t)(j + 1));
            }
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
                /* NEU 1.0.7 (Recherche 31.10.2026, recherche_sony_ordner_B: MacheteSVG, PS5-Suite, Save-Mounter): Zu jedem Spielstand
                 * gehören Bild und Beschreibung in <savedata>_meta(/user)/<Kennung>. Sie gehen mit dem Spielstand, nie allein - bis 1.0.6
                 * blieben sie als Rest liegen. Nur was es wirklich gibt. */
                if (!kern_bereiche_106() && h_m(x, "^[A-Z]{4}\\d{5}$")) {
                    const char *m[2] = { VERB(A, "/user/home/", homes.e[i], "/", sds[s], "_meta/user/", x), VERB(A, "/user/home/", homes.e[i], "/", sds[s], "_meta/", x) };
                    for (int q = 0; q < 2; q++) if (s_existiert(k->S, m[q])) sl_add(A, &l, m[q]);
                }
                MERKE(k, &l, 2, VERB(A, T("Spielstand ", "Saved data "), mit_name_spst(k, x)), 0);
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
    /* NEU 1.0.7 (PS5-Test 02.11.2026): /user/av_contents/sdr/<photo|video>/<App>/<Spiel> ist eine zweite Fassung der Aufnahmen - auf
     * da_Chris86s PS5 dieselben Ordner, zur selben Minute angelegt wie die Red-Dead-Aufnahmen (vermutlich die PS5-eigene SDR-Fassung von
     * HDR-Screenshots). Sie gehört zu "Aufnahmen von <Spiel>" und geht mit ihnen; gezählt werden die Originale (keine Doppelzählung).
     * Bis hier stand der ganze Ordner als "Weitere Galerie-Daten: sdr/photo - Inhalt unklar, lieber stehen lassen". */
    mtab sdrDrin = mt_neu(true);
    if (!kern_bereiche_106()) {
        sliste arts = s_namen(k->S, "/user/av_contents/sdr", false);
        for (int a1 = 0; a1 < arts.n; a1++) {
            if (!h_m(arts.e[a1], "^(photo|video)$")) continue;
            bool foto = h_gleich(arts.e[a1], "photo");
            sliste appsg = s_namen(k->S, VERB(A, "/user/av_contents/sdr/", arts.e[a1]), false);
            for (int i = 0; i < appsg.n; i++) {
                sliste ids = s_namen(k->S, VERB(A, "/user/av_contents/sdr/", arts.e[a1], "/", appsg.e[i]), false);
                for (int j = 0; j < ids.n; j++) {
                    const char *id = ids.e[j];
                    if (!r_m(id, "^[A-Z]{4}\\d{5}$")) continue;
                    const char *pf = VERB(A, "/user/av_contents/sdr/", arts.e[a1], "/", appsg.e[i], "/", id);
                    /* Codex-Prüfung 40 (3/4): nur zusammen mit echten Original-Aufnahmen - die löscht Sony, die zweite Fassung geht danach mit.
                     * Gibt es zu dieser Kennung keine Original-Aufnahme, bleibt sie wie bisher "Weitere Galerie-Daten" (Inhalt unklar) - ohne
                     * Original wäre nicht belegt, dass das Löschen der Datei allein keine Leiche in der Mediengalerie hinterlässt. */
                    (void)foto;
                    if (!mt_hat(&gPfade, id) || (intptr_t)mt_hole(&gBilder, id) + (intptr_t)mt_hole(&gVideos, id) <= 0) continue;
                    /* Codex-Prüfung 41 (3): JEDE Aufnahme der zweiten Fassung (Unterordner, z. B. 16a) braucht ihr Original im selben
                     * Unterordner unter photo|video/<App>/<Spiel> - sonst bleibt die ganze zweite Fassung "Weitere Galerie-Daten" */
                    {
                        sliste su = s_namen(k->S, pf, false), ou = s_namen(k->S, VERB(A, "/user/av_contents/", arts.e[a1], "/", appsg.e[i], "/", id), false);
                        bool alle = su.n > 0;
                        for (int q = 0; q < su.n && alle; q++) if (!h_hat(&ou, su.e[q])) alle = false;
                        if (!alle) continue;
                    }
                    sl_add(A, (sliste *)mt_hole(&gPfade, id), pf);
                    mt_setze(A, &sdrDrin, pf, (void *)1);
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
            if (!kern_bereiche_106() && h_gleich(s, "sdr")) {   /* NEU 1.0.7: was nicht zu "Aufnahmen von …" gehört, einzeln wie bisher */
                sliste xs = s_namen(k->S, "/user/av_contents/sdr", false);
                for (int j = 0; j < xs.n; j++) {
                    if (!h_m(xs.e[j], "^(photo|video)$")) { sliste l = L1(A, VERB(A, "/user/av_contents/sdr/", xs.e[j])); MERKE(k, &l, 4, VERB(A, T("Weitere Galerie-Daten: ", "Other gallery data: "), "sdr/", xs.e[j]), 0); continue; }
                    sliste ag = s_namen(k->S, VERB(A, "/user/av_contents/sdr/", xs.e[j]), false);
                    for (int g = 0; g < ag.n; g++) {
                        sliste ids = s_namen(k->S, VERB(A, "/user/av_contents/sdr/", xs.e[j], "/", ag.e[g]), false);
                        for (int q = 0; q < ids.n; q++) {
                            const char *pf = VERB(A, "/user/av_contents/sdr/", xs.e[j], "/", ag.e[g], "/", ids.e[q]);
                            if (mt_hat(&sdrDrin, pf)) continue;
                            sliste l = L1(A, pf); MERKE(k, &l, 4, VERB(A, T("Weitere Galerie-Daten: ", "Other gallery data: "), "sdr/", xs.e[j], "/", ag.e[g], "/", mit_name(k, ids.e[q])), 0);
                        }
                    }
                }
                continue;
            }
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
            if (k->MERKE_OK && mt_hat(&k->NPWR_NR, x)) ((eintrag *)k->bau.e[k->bau.n - 1])->ListeNr = (int)(intptr_t)mt_hole(&k->NPWR_NR, x);
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
            if (k->MERKE_OK && mt_hat(&k->NPWR_NR, x)) ((eintrag *)k->bau.e[k->bau.n - 1])->ListeNr = (int)(intptr_t)mt_hole(&k->NPWR_NR, x);
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
            if (k->MERKE_OK && mt_hat(&k->NPWR_NR, kk)) ((eintrag *)k->bau.e[k->bau.n - 1])->ListeNr = (int)(intptr_t)mt_hole(&k->NPWR_NR, kk);   /* NEU 1.0.7 (02.11.2026) */
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
            /* NEU 1.0.7 (PS5-Test 02.11.2026, GTA V): ein installiertes Spiel nennt diese Kennung als dasselbe Spiel (Zusatzinhalt-
             * Kennung) - seine Daten können gebraucht werden, wie bei der App selbst: nicht als Rest anbieten (Codex-Prüfung 42 (1)) */
            if (verk_belegt(k, x)) continue;
            const char *dt = kern_jb_einstieg(x) ? VERB(A, T("App-Daten eines Jailbreak-Einstiegs: ", "App data of a jailbreak entry: "), mit_name(k, x))
                                                 : VERB(A, T("App-Daten ohne installierte App: ", "App data without installed app: "), mit_name(k, x));
            /* da_Chris86 27.10.2026 21:39: keine pauschale Sperre bei fehlender/nicht lesbarer M.2 oder USB-Erweiterung */
            MERKE(k, &l, 5, dt, 0);
        }
    }
    for (int i = 0; i < patches.n; i++) { const char *x = patches.e[i]; if (!mt_hat(&benutzt, VERB(A, "/user/patch/", x)) && !h_like(x, "NPXS*") && h_m(x, "^[A-Z]{4}\\d{5}$")) { sliste l = L1(A, VERB(A, "/user/patch/", x)); if (!verk_belegt(k, x)) MERKE(k, &l, 5, VERB(A, T("Update ohne Spiel: ", "Update without game: "), mit_name(k, x)), 0); } }
    for (int i = 0; i < addconts.n; i++) { const char *x = addconts.e[i]; if (!mt_hat(&benutzt, VERB(A, "/user/addcont/", x)) && !h_like(x, "NPXS*") && h_m(x, "^[A-Z]{4}\\d{5}$")) { sliste l = L1(A, VERB(A, "/user/addcont/", x)); if (!verk_belegt(k, x)) MERKE(k, &l, 5, VERB(A, T("Zusatzinhalt: ", "Add-on: "), mit_name(k, x)), 0); } }
    {
        sliste xs = s_namen(k->S, "/user/appmeta/external/addcont", false);
        for (int i = 0; i < xs.n; i++) if (!h_like(xs.e[i], "NPXS*") && h_m(xs.e[i], "^[A-Z]{4}\\d{5}$")) { sliste l = L1(A, VERB(A, "/user/appmeta/external/addcont/", xs.e[i])); if (!verk_belegt(k, xs.e[i])) MERKE(k, &l, 5, VERB(A, T("Zusatzinhalt-Infos: ", "Add-on info: "), mit_name(k, xs.e[i])), 0); }
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
            for (int i = 0; i < ePatches[w]->n; i++) { const char *x = ePatches[w]->e[i]; if (!mt_hat(&benutzt, VERB(A, bu, "/patch/", x)) && !h_like(x, "NPXS*") && h_m(x, "^[A-Z]{4}\\d{5}$")) { sliste l = L1(A, VERB(A, bu, "/patch/", x)); if (!verk_belegt(k, x)) MERKE(k, &l, 5, VERB(A, T("Update ohne Spiel: ", "Update without game: "), mit_name(k, x)), 0); } }
            for (int i = 0; i < eAddconts[w]->n; i++) { const char *x = eAddconts[w]->e[i]; if (!mt_hat(&benutzt, VERB(A, bu, "/addcont/", x)) && !h_like(x, "NPXS*") && h_m(x, "^[A-Z]{4}\\d{5}$")) { sliste l = L1(A, VERB(A, bu, "/addcont/", x)); if (!verk_belegt(k, x)) MERKE(k, &l, 5, VERB(A, T("Zusatzinhalte ohne Spiel: ", "Add-ons without game: "), mit_name(k, x)), 0); } }
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
        const char *roh = k->APP_ROH;
        if (t[0] == 0) t = T("entferntes Spiel", "removed game");
        sliste l = L1(A, VERB(A, "/user/appmeta/", x));
        sliste mk = mit_kachel(k, &l, x, &sysmeta);
        /* NEU 1.0.7 (PS5-Test 03.11.2026, da_Chris86: "bei Spectrum Library wird gar nichts zum Löschen angeboten"): eine Kachel ohne
         * App-Ordner, deren Beschreibung auf ein Programm auf der Konsole verlinkt (deeplinkUri http://127.0.0.1 bzw. localhost - wie
         * die Tool-Kacheln oben), ist kein Rest, sondern die Kachel eines Tools: Sony führt sie in app.db (da_Chris86s PS5: SLIB00001
         * "Spectrum Library", /user/appmeta/SLIB00001), das Programm (ELF) legt sie bei jedem Start wieder an. Sie steht darum als
         * "Spectrum Library – Kachel" unter Apps, Einstellungen und ELF darunter. Gelöscht wird wie bisher (über Sony austragen). */
        if (!kern_bereiche_106() && h_m(roh, "\"deeplinkUri\"\\s*:\\s*\"https?://(127\\.0\\.0\\.1|localhost)"))
            MERKE(k, &mk, 6, VERB(A, T("Tool-Kachel: ", "Tool tile: "), t, " (", x, ")"), .kachel = true);
        else
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
    for (int i = 0; i < dlcInfos.n; i++) { const char *x = dlcInfos.e[i]; if (!mt_hat(&benutzt, VERB(A, "/user/appmeta/addcont/", x)) && !h_like(x, "NPXS*") && h_m(x, "^[A-Z]{4}\\d{5}$")) {   /* NPXS: Sony (Anforderungen 27.10.2026) */ sliste l = L1(A, VERB(A, "/user/appmeta/addcont/", x)); if (!verk_belegt(k, x)) MERKE(k, &l, 5, VERB(A, T("Zusatzinhalt-Infos: ", "Add-on info: "), mit_name(k, x)), 0); } }

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
            /* NEU 1.0.7 (Codex-Prüfung 42 (1)): Lizenz einer Kennung, die ein installiertes Spiel als dasselbe Spiel nennt - kann gebraucht werden */
            if (!weiter && tm.ok && verk_belegt(k, tm.gr[1])) weiter = true;
            if (weiter) continue;
            const char *n = h_r(A, g, "^fake", "");
            if (tm.ok && kern_bereiche_106()) { const char *mn = mit_name(k, tm.gr[1]); if (strcmp(mn, tm.gr[1]) != 0) n = VERB(A, n, " - ", h_r(A, mn, "^[A-Z]{4}\\d{5} ", "")); }
            else if (tm.ok) { int art = 3; const char *nm = name_belegt(k, tm.gr[1], &art); n = VERB(A, n, " - ", nm[0] ? nm : T("Spiel/App unbekannt", "game/app unknown")); }
            MERKE(k, &pf, kern_bereiche_106() ? 6 : 5, VERB(A, T("Fake-Lizenz: ", "Fake license: "), n,   /* 1.0.7: Rest eines gelöschten Spiels -> Sonstiges */ T(" (Spiel nicht mehr installiert)", " (game no longer installed)")), 0);
        }
    }
    /* NEU 1.0.7 (Recherche 31.10.2026: ps4mount README/build.h, ps5upload cleanup_rules.h Kategorie F; selbst gelesen: d0.pdb nennt
     * Spiel, Version und Quelle der Installation): /user/playgo/{app,patch,addcont}/<Content-ID> sind die Installationsdaten (PlayGo)
     * eines Spiels. Angeboten werden sie NUR, wenn das Spiel nirgends mehr installiert ist (intern, M.2, USB) - wie ps5upload; ist
     * eine Liste nicht lesbar, bricht die Suche ohnehin ab. /system_data/playgo bleibt unangetastet (fasst kein Tool an). */
    if (!kern_bereiche_106()) {
        const char *arten[3] = { "app", "patch", "addcont" };
        sliste ids = {0};
        mtab grp = mt_neu(true);
        for (int a1 = 0; a1 < 3; a1++) {
            sliste cs = s_namen(k->S, VERB(A, "/user/playgo/", arten[a1]), false);
            for (int j = 0; j < cs.n; j++) {
                htreffer m = h_mt(A, cs.e[j], "^[A-Z]{2}\\d{4}-([A-Z]{4}\\d{5})_\\d{2}-[A-Z0-9]{16}$");
                if (!m.ok || h_like(m.gr[1], "NPXS*")) continue;
                const char *tid = m.gr[1];
                if (h_hat(&apps, tid) || h_hat(&m2Apps, tid) || h_hat(&usbApps, tid)) continue;     /* Spiel noch da */
                if (verk_belegt(k, tid)) continue;   /* NEU 1.0.7: dasselbe Spiel unter anderer Kennung ist installiert (Codex-Prüfung 42 (1)) */
                if (!mt_hat(&grp, tid)) { sl_add(A, &ids, tid); sliste *np = a_hole(A, sizeof *np); mt_setze(A, &grp, tid, np); }
                sl_add(A, (sliste *)mt_hole(&grp, tid), VERB(A, "/user/playgo/", arten[a1], "/", cs.e[j]));
            }
        }
        for (int i = 0; i < ids.n; i++) {
            sliste *pf = mt_hole(&grp, ids.e[i]);
            MERKE(k, pf, 5, VERB(A, T("Reste: Installationsdaten (PlayGo) von ", "Leftovers: install data (PlayGo) of "), mit_name_kurz(k, ids.e[i]),
                                 T(" (Spiel nicht mehr installiert)", " (game no longer installed)")), 0);
        }
    }
    /* NEU 1.0.7 (da_Chris86 31.10.2026 17:00, Ausnahme zu "/system_ex nie anfassen"; belegt: ShadowMountPlus legt /system_ex/app/<Kennung>
     * als Einhängeordner an und löscht ihn nie, auf da_Chris86s PS5 41 Stück): leere Einhängeordner ohne Spiel. Nur ganz leere Ordner
     * (eingehängte sind nie leer), nur ohne /user/app/<Kennung> (intern, M.2, USB), nie Sony/PS5 Cleanup/Launcher/Playden-Spiele
     * (kern_schutz.c). Ein Eintrag für alle, gelöscht nur mit rmdir. */
    if (!kern_bereiche_106()) {
        dliste *sx = s_liste(k->S, "/system_ex/app", false);
        sliste leer = {0};
        for (int i = 0; sx && i < sx->n; i++) {
            const char *x = sx->e[i].name;
            const char *p = VERB(A, "/system_ex/app/", x);
            if (!sx->e[i].ist_ordner || sx->e[i].ist_link || !kern_erlaubt(k, p)) continue;
            if (h_hat(&apps, x) || h_hat(&m2Apps, x) || h_hat(&usbApps, x)) continue;          /* Spiel/App noch da */
            dliste *in = s_liste(k->S, p, false);
            if (!in || in->n > 0) continue;                                                   /* nicht leer oder nicht lesbar */
            sl_add(A, &leer, p);
        }
        if (leer.n > 0) {
            MERKE(k, &leer, 5, VERB(A, T("Reste: leere Einhängeordner von ShadowMount (", "Leftovers: empty ShadowMount mount folders ("), zahl(A, leer.n), ")"), 0);
            if (k->MERKE_OK) ((eintrag *)k->bau.e[k->bau.n - 1])->NurLeer = true;
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
                b->Jailbreak = true; b->NurEinzeln = true; b->Kat = kern_bereiche_106() ? 6 : 7;   /* 1.0.7: Bereich Jailbreak */
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
        if (!f->Quelle || !f->Quelle[0]) continue;
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
            n->Pfade = jbp; n->PfadeBasis = sl_kopie(A, &jbp); n->Kat = kern_bereiche_106() ? 6 : 7; n->Quelle = ""; n->Kachel = f->Kachel;
            n->Jailbreak = true; n->NurEinzeln = true; n->Ort = f->Ort;
            n->Text = n->TextBasis = VERB(A, T("Jailbreak-Einstieg ", "Jailbreak entry "), sl_join(A, &jbids, ", "), " (", h_r(A, f->TextBasis, "\\s*\\(.*$", ""), ")", kern_nur_einzeln_text());
            pl_add(A, &jbNeu, n);
            continue;
        }
        f->Jailbreak = true; f->NurEinzeln = true;
        /* da_Chris86 28.10.2026 22:55: Jailbreak-Einstiege stehen unter "Jailbreak & Homebrew". da_Chris86 29.10.2026: ein ShadowMount-Spiel
         * (mit Spieldatei) bleibt unter "Spiele" - dort ist das Löschen von Spieldatei und Kachel geregelt; geschützt und gewarnt wird es trotzdem */
        /* NEU 1.0.7 (da_Chris86 31.10.2026 "nutzerfreundlich - wo der Nutzer ihn sucht"): alle Einstiege unter "Jailbreak", auch ein
         * Jailbreak-Spiel als Spiel-Abbild (bis 1.0.6 unter "Spiele"). Spieldatei/Kachel-Regeln hängen an Quelle, nicht am Bereich. */
        if (kern_bereiche_106()) { if (!(f->Kat == 1 && abbild)) f->Kat = 6; } else f->Kat = 7;
        f->Text = f->TextBasis = VERB(A, f->Text, kern_nur_einzeln_text());
    }
    for (int i = 0; i < jbNeu.n; i++) pl_add(A, &k->bau, jbNeu.e[i]);
    if (!kern_bereiche_106()) bereiche_107(k);
    if (!kern_bereiche_106()) alte_sicherungen_zusammen(k);
    if (!kern_bereiche_106()) sm_spieldatei_zeilen(k);
    /* fest nach Bereich ordnen, darin nach Ort (intern, M.2, USB, USB-Stick - NEU 1.0.6); sonst bleibt die Fundreihenfolge */
    pliste sortiert = {0};
    for (int ki = 0; ki < KAT_ANZ; ki++) for (int o = ORT_INTERN; o <= ORT_STICK; o++) for (int i = 0; i < k->bau.n; i++) {
        eintrag *f = k->bau.e[i];
        if (f->Kat == ALLE_KAT[ki] && f->Ort == o) pl_add(A, &sortiert, f);
    }
    if (kern_bereiche_106()) tool_daten_ordnen(k, &sortiert);
    else kachel_bloecke(k, &sortiert);   /* NEU 1.0.7 (02.11.2026): ein Block pro Kachel - ersetzt tool_daten_ordnen */
    if (!kern_bereiche_106()) ohne_eingeblendete_ordner(k, &sortiert);
    k->Funde = sortiert;
    /* NEU 1.0.7 (PS5-Test 02.11.2026, Red Dead Redemption 2): kaputte Zusatzinhalt-Angaben deutlich an der Kachel des Spiels */
    if (!kern_bereiche_106() && k->SID_KAPUTT.n > 0)
        for (int i = 0; i < k->Funde.n; i++) {
            eintrag *f = k->Funde.e[i];
            const char *id = kopf_kennung(f);
            if (id && (f->KachelId || ist_teil(f, "Spiel")) && mt_hat(&k->SID_KAPUTT, id)) f->SidKaputt = (int)(intptr_t)mt_hole(&k->SID_KAPUTT, id);
        }
    kern_arten_setzen(k);   /* NEU 1.0.7: Art, Ampel, Rot */
    if (!kern_bereiche_106()) {   /* NEU 1.0.7 (da_Chris86 31.10.2026 19:56): installierte PS4-Spiele - nur lesen */
        k->PS4_UNKLAR = false;
        VERSUCHE { kern_ps4_erfassen(k); }
        FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); k->PS4 = (pliste){0}; k->PS4_UNKLAR = true;
                kern_melde(k, VERB(A, T("PS4-Spiele ließen sich nicht vollständig erfassen (", "PS4 games could not be listed completely ("), ausn_text,
                              T(") - alles, was zu einem PS4-Spiel gehören könnte, bleibt geschützt.", ") - everything that might belong to a PS4 game stays protected."))); } ENDE_VERSUCHE;
    }
    k->FUNDE_GUELTIG = true;
    k->funde_verbindung_nr = k->verbindung_nr;  /* FUNDE_IP = S.Ftp.Ip + ":" + S.Ftp.Port (Suche.cs:765) - fehlte bis 25.10.2026, siehe ABWEICHUNGEN A10 */
}

