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
/* NEU 1.0.8 (PS5-Test 07.11.2026, L.A. Noire): name_belegt/name_belegt_spst sagen hier, ob "unbekannt" von einem Widerspruch kommt (dann
 * bleibt es dabei) oder nur daher, dass beim Anlegen des Eintrags noch kein Beleg da war. Nur solche Kennungen kommen nach NAMEN_OFFEN -
 * namen_nachtragen trägt nach der Suche den Namen ein, wenn eine PKG-Datei ihn belegt (die PKG-Dateien werden erst später gefunden). */
static bool nb_widerspruch;
static void offen_merken(kern *k, const char *id)
{
    if (kern_bereiche_106() || !id || !id[0]) return;
    /* Codex-Prüfung 108 (P2): Widerspruch bei EINEM Eintrag (z. B. Spielstand mit zwei INSTALL_DIR_SAVEDATA-Belegen) - dann wird für diese
     * Kennung nirgends nachgetragen (2), auch nicht bei einer Aufnahme derselben Kennung ohne Beleg */
    if (nb_widerspruch) { mt_setze(A, &k->NAMEN_OFFEN, a_kopie(A, id), (void *)2); return; }
    if (!mt_hat(&k->NAMEN_OFFEN, id)) mt_setze(A, &k->NAMEN_OFFEN, a_kopie(A, id), (void *)1);
}
static const char *mit_name_art(kern *k, const char *x, bool spst)
{
    htreffer m = h_mt(A, x, "^([A-Z]{4}\\d{5})");
    if (!m.ok) return x;
    int art = 3;
    const char *t = spst ? name_belegt_spst(k, m.gr[1], &art) : name_belegt(k, m.gr[1], &art);
    if (art <= 1) return VERB(A, x, " (", t, ")");
    if (kern_bereiche_106()) return x;   /* Vergleichstests mit 1.0.5/1.0.6 unverändert */
    if (art == 2) return VERB(A, x, " (", t, T(", nicht mehr auf der Konsole)", ", no longer on the console)"));
    offen_merken(k, m.gr[1]);
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
    if (art <= 2) return VERB(A, x, " (", t, ")");
    offen_merken(k, m.gr[1]);
    return VERB(A, x, " (", T("Spiel/App unbekannt", "game/app unknown"), ")");
}
/* NEU 1.0.8 (PS5-Test 07.11.2026 16:31, da_Chris86: "bei den PS4-Spielen sind die Bilder und die Beschriftung da, bei den PS5-Spielen
 * nicht"): Eine PS5-Trophäen-Liste (/user/trophy2/nobackup/conf/<NPWR>/TROPHY.UCP) trägt den Namen des Spiels selbst, offen lesbar.
 * Aufbau wie TROPHY.TRP: Kopf 0x60 Byte (Kennung B2 28 C6 0A, Gesamtlänge, Zahl der Einträge, Eintragsgröße 0x40), dann je Eintrag
 * Name (32 Byte), Anfang, Länge; darin tropmeta_<Sprache>.json mit "trophyNpCommId" und "metadata":{"titleMetadata":{"name":...}}
 * (auf da_Chris86s PS5 nachgesehen: NPWR23378_00 "Marvel's Spider-Man 2", NPWR21647_00 "Grand Theft Auto V"). Das ist Sonys eigene Angabe
 * zu genau dieser Liste - ein Beleg, kein Raten. Nur lesen: Kopf, Tabelle (höchstens 4096 Einträge = 256 KB) und eine JSON-Datei
 * (höchstens 256 KB); passt etwas nicht (andere Kennung, kaputt, zu groß, nicht lesbar), gibt es keinen Namen - wie bisher. */
#define UCP_EINTRAEGE_MAX 4096
#define UCP_JSON_MAX (256 * 1024)
static int net_laenge(const char *s);   /* steht weiter unten */
static uint32_t ucp_be32(const unsigned char *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3]; }
static unsigned long long ucp_be64(const unsigned char *p) { return (unsigned long long)ucp_be32(p) << 32 | ucp_be32(p + 4); }
static const char *ucp_lesen(kern *k, arena *t, const char *npwr)
{
    const char *p = VERB(t, "/user/trophy2/nobackup/conf/", npwr, "/TROPHY.UCP");
    unsigned char kopf[0x60]; size_t len = 0;
    if (!s_lies_ab(k->S, p, 0, sizeof kopf, kopf, &len) || len != sizeof kopf) return "";
    if (memcmp(kopf, "\xB2\x28\xC6\x0A", 4) != 0 || ucp_be32(kopf + 0x14) != 0x40) return "";
    unsigned long long gesamt = ucp_be64(kopf + 0x08);
    uint32_t anz = ucp_be32(kopf + 0x10);
    if (anz == 0 || anz > UCP_EINTRAEGE_MAX || gesamt < 0x60 + (unsigned long long)anz * 0x40) return "";
    size_t tl = (size_t)anz * 0x40;
    unsigned char *tab = a_hole(t, tl);
    if (!s_lies_ab(k->S, p, 0x60, tl, tab, &len) || len != tl) return "";
    const char *wunsch[3] = { T("tropmeta_de-DE.json", "tropmeta_en-US.json"), "tropmeta_en-US.json", "tropmeta_en-GB.json" };
    int beste = 99; unsigned long long ab = 0, lg = 0;
    for (uint32_t i = 0; i < anz; i++) {
        const unsigned char *e = tab + (size_t)i * 0x40;
        char name[33]; memcpy(name, e, 32); name[32] = 0;
        int rang = -1;
        for (int w = 0; w < 3; w++) if (!strcmp(name, wunsch[w])) { rang = w; break; }
        if (rang < 0 && !strncmp(name, "tropmeta_", 9) && h_m(name, "\\.json$")) rang = 3;
        if (rang < 0 || rang >= beste) continue;
        beste = rang; ab = ucp_be64(e + 32); lg = ucp_be64(e + 40);
    }
    if (beste == 99 || lg < 2 || lg > UCP_JSON_MAX || ab < 0x60 || ab > gesamt || lg > gesamt - ab || ab > (unsigned long long)INT64_MAX) return "";
    unsigned char *j = a_hole(t, (size_t)lg + 1);
    if (!s_lies_ab(k->S, p, (long long)ab, (size_t)lg, j, &len) || len != (size_t)lg || memchr(j, 0, (size_t)lg)) return "";
    j[lg] = 0;
    cJSON *o = cJSON_ParseWithOpts(h_ohne_bom((const char *)j), NULL, true);
    if (!o) return "";
    const char *r = "";
    cJSON *id = hole_ci(o, "trophyNpCommId"), *md = hole_ci(o, "metadata"), *tm = md ? hole_ci(md, "titleMetadata") : NULL, *nm = tm ? hole_ci(tm, "name") : NULL;
    if (cJSON_IsString(id) && id->valuestring && h_gleich(id->valuestring, npwr) && cJSON_IsString(nm) && nm->valuestring) {
        const char *n = h_trim(t, nm->valuestring);
        if (net_laenge(n) >= 1 && net_laenge(n) <= 100 && !h_m(n, "[\\x00-\\x1f]")) r = a_kopie(A, n);
    }
    cJSON_Delete(o);
    return r;
}
static const char *ucp_titel(kern *k, const char *npwr)
{
    if (kern_bereiche_106() || !h_m(npwr, "^NPWR\\d{5}_\\d{2}$")) return "";
    if (mt_hat(&k->NPWR_UCP, npwr)) return (const char *)mt_hole(&k->NPWR_UCP, npwr);
    arena *volatile t = arena_neu();
    const char *volatile r = "";
    VERSUCHE { r = ucp_lesen(k, t, npwr); }
    FANGE { arena_frei(t); t = NULL; if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); r = ""; } ENDE_VERSUCHE;
    if (t) arena_frei(t);
    mt_setze(A, &k->NPWR_UCP, a_kopie(A, npwr), (void *)r);
    return r;
}
static const char *trophaen_name(kern *k, const char *npwr)
{
    const char *n = mt_hole(&k->NPWR_NAMEN, npwr);
    if (mt_hat(&k->NPWR_NAMEN, npwr) && n && n[0]) return VERB(A, n, " (", npwr, ")");
    if (mt_hat(&k->NPWR_GALERIE, npwr)) return VERB(A, (const char *)mt_hole(&k->NPWR_GALERIE, npwr), " (", npwr, ")");
    if (kern_bereiche_106()) return npwr;
    const char *u = ucp_titel(k, npwr);   /* NEU 1.0.8: Name aus der PS5-Trophäen-Liste selbst */
    if (u[0]) return VERB(A, u, " (", npwr, ")");
    if (mt_hat(&k->PROT_NPWR, npwr)) return VERB(A, (const char *)mt_hole(&k->PROT_NPWR, npwr), " (", npwr, ")");   /* NEU 1.0.8: früheres Protokoll */
    if (!mt_hat(&k->NPWR_OFFEN, npwr)) mt_setze(A, &k->NPWR_OFFEN, a_kopie(A, npwr), (void *)1);   /* NEU 1.0.8: namen_nachtragen */
    return VERB(A, npwr, " (", T("Spiel/App unbekannt", "game/app unknown"), ")");   /* NEU 1.0.7: unbekannte Zuordnung ausdrücklich nennen */
}
static const char *trophaen_weg(kern *k, const char *npwr) { return mt_hat(&k->NPWR_NAMEN, npwr) ? "" : T(" - Spiel nicht mehr auf der Konsole", " - game no longer on the console"); }
/* NEU 1.0.8 (PS5-Test 07.11.2026 17:31, NPWR09412_00_BK): eine Kopie des Trophäen-Fortschritts - der Ordner heißt wie die Liste, mit einer
 * Endung (auf da_Chris86s PS5 entstanden um 15:46, in derselben Minute wie der neue Ordner NPWR09412_00, als Red Dead Redemption 2 wieder
 * installiert wurde - vermutlich legt die PS5 dabei den alten Fortschritt beiseite). Name, Zuordnung und Zustand kommen von der Liste
 * ohne Endung; die Zeile sagt "Kopie" (kern_art.c). */
static const char *npwr_basis(kern *k, const char *x)
{
    if (kern_bereiche_106()) return x;
    htreffer m = h_mt(A, x, "(?-i)^(NPWR\\d{5}_\\d{2})_[A-Za-z0-9]{1,16}$");
    return m.ok ? m.gr[1] : x;
}
static const char *npwr_kopie(kern *k, const char *x, const char *xb) { return strcmp(x, xb) ? VERB(A, T("  [Kopie: ", "  [copy: "), x, "]") : ""; }

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
    /* NEU 1.0.8 (F4, da_Chris86s Bilder vom 04.11.2026): Aufnahmen, die während PS5 Cleanup lief, gehören zu dessen Startordner PSCL00002
     * (bis 1.0.7: "Aufnahmen von 20261104175554 (PSCL00002, Spiel nicht mehr auf der Konsole)") */
    if (!kern_bereiche_106() && (!strcmp(id, "PSCL00002") || !strcmp(id, "PSCL00001"))) return VERB(A, "PS5 Cleanup (", id, ")");
    if (!kern_bereiche_106()) {   /* NEU 1.0.7 (PS5-Test 02.11.2026): Name und Zuordnung nur aus Metadaten (name_belegt) */
        int art = 3;
        const char *t = name_belegt(k, id, &art);
        if (art <= 1) return VERB(A, t, " (", id, ")");
        if (art == 2) return VERB(A, t, " (", id, T(", Spiel nicht mehr auf der Konsole)", ", game no longer on the console)"));
        offen_merken(k, id);   /* NEU 1.0.8: namen_nachtragen */
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
/* NEU 1.0.8 (Protokoll 174343 vom PS5-Test 07.11.2026: "Spielstand PPSA03420 (Gespeicherte Daten, nicht mehr auf der Konsole)" - der
 * Spielstand von GTA V): manche Spiele geben ihren Spielständen statt ihres Namens einen allgemeinen Titel ("Gespeicherte Daten", "Saved
 * Data", "Spielstand 2" ...). Das ist kein Spielname und belegt nichts - dann bleibt es "Spiel/App unbekannt" (mit dem Bild des Spielstands).
 * Nur der ganze Titel zählt (Groß-/Kleinschreibung egal), höchstens mit einer kleinen Nummer dahinter. */
static bool titel_allgemein(const char *t)
{
    return t && h_m(t, "^\\s*(gespeicherte daten|spielst(and|ände)|spieldaten|speicherdaten|systemdaten|benutzerdaten|saved? ?data|saves?|save ?game|saved game|"
                       "auto ?saves?|game ?data|system ?data|user ?data|données sauvegardées|sauvegardes?|datos guardados|partida guardada|"
                       "dati salvati|salvataggio|dados salvos|dados guardados|opgeslagen gegevens|zapisane dane|сохран[её]нные данные|"
                       "セーブデータ|存档|存檔|保存数据|저장 데이터|세이브 데이터)(\\s*[#.:-]?\\s*\\d{1,3})?\\s*$");
}
static void appinfo_zeile(kern *k, const char *titel, const char *feld, const char *wert)
{
    if (!titel || !feld) return;
    /* NEU 1.0.8 (PS5-Test 07.11.2026): Titel aus Sonys Spielstand-Liste (savedata.db) - für jede Kennung, auch nicht installierte. Wie
     * MAINTITLE in rest_name: tragen die Spielstände einer Kennung verschiedene Titel, ist der Name nicht belegt ("") */
    if (!strcmp(feld, "SPIELSTAND_TITEL")) {
        if (!kennung_ok(titel)) return;
        const char *w = h_trim(A, wert ? wert : "");
        if (net_laenge(w) < 2 || net_laenge(w) > 100) return;
        if (!kern_bereiche_106() && titel_allgemein(w)) return;   /* NEU 1.0.8: "Gespeicherte Daten" ist kein Spielname */
        if (!mt_hat(&k->SPST_TITEL, titel)) mt_setze(A, &k->SPST_TITEL, a_kopie(A, titel), (void *)a_kopie(A, w));
        else { const char *alt = mt_hole(&k->SPST_TITEL, titel); if (alt && alt[0] && strcmp(alt, w) != 0) mt_setze(A, &k->SPST_TITEL, a_kopie(A, titel), (void *)""); }
        return;
    }
    if (!mt_hat(&k->INSTALLIERT, titel)) return;
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
 *  2. NEU 1.0.8: ein früheres Protokoll von PS5 Cleanup (dort steht der Name so, wie PS5 Cleanup ihn damals aus den Angaben des
 *     installierten Spiels gelesen hat - protokoll_namen_lesen)
 *  3. Beschreibung der Spielstände (<savedata>_meta/user/<Kennung>/<Datei>.sfo, Feld MAINTITLE) - nur wenn ALLE denselben Namen tragen
 *     (ein Spiel kann seinen Spielständen eigene Titel geben - dann ist der Name nicht belegt). NEU 1.0.8: ein Spielstand, dessen Titel
 *     gleich seinem Untertitel ist, zählt nicht (L.A. Noire: "Benutzereinstellungen & Freischaltobjekte" - kein Spielname)
 *  4. NEU 1.0.8: derselbe Titel aus Sonys Spielstand-Liste (savedata.db, SPST_TITEL) - die .sfo liegt nur bei manchen Spielständen
 * "" = kein Beleg. */
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
    if (!n[0] && mt_hat(&k->PROT_NAMEN, id)) { const char *p = mt_hole(&k->PROT_NAMEN, id); if (p && p[0]) n = p; }
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
                if (!kern_bereiche_106() && !strcmp(mt, h_trim(A, kern_sfo_wert(A, &sf, "SUBTITLE")))) continue;   /* NEU 1.0.8: Titel = Untertitel */
                if (!kern_bereiche_106() && titel_allgemein(mt)) continue;   /* NEU 1.0.8: "Gespeicherte Daten" ist kein Spielname */
                if (!einer) einer = mt; else if (strcmp(einer, mt) != 0) gleich = false;
            }
        }
        if (einer && gleich && net_laenge(einer) >= 2 && net_laenge(einer) <= 100) n = einer;
    }
    if (!n[0] && mt_hat(&k->SPST_TITEL, id)) { const char *s = mt_hole(&k->SPST_TITEL, id); if (s && s[0]) n = s; }
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
    nb_widerspruch = false;
    if (t && t[0] && (!widerspruch || mt_hat(&k->INSTALLIERT, id))) { *art = 0; return t; }
    if (kern_bereiche_106() || widerspruch) { nb_widerspruch = true; *art = 3; return ""; }
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
    if (widerspruch) { nb_widerspruch = true; *art = 3; return ""; }
    *art = 1;
    return besitzer_name(k, o);
}
/* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86 13:32: "keine Daten speichern auf der PS5, außer die Logs"): Namen aus den Protokollen, die
 * PS5 Cleanup früher geschrieben hat (USB-Laufwerk, sonst /data/PS5-Cleanup-Protokolle bzw. dasselbe auf der M.2). Es zählen nur zwei
 * Arten von Zeilen - beide nennen den Namen so, wie PS5 Cleanup ihn damals aus den Angaben des installierten Spiels gelesen hat:
 *  - die Liste der PS4-Spiele beim Nachschauen   ("   L.A. Noire (CUSA09172) – intern – ...")
 *  - gelöschte oder unveränderte Spiele           ("  gelöscht   L.A. Noire (CUSA09172) - Spiel   (...)", auch "M.2 – X (ID) - ShadowMount-Spiel")
 * Platzhalter wie "unbekannter Name" zählen nicht. Neueste Datei zuerst (der Dateiname enthält Datum und Uhrzeit), der erste Fund gilt.
 * Nur lesen; höchstens PROT_DATEIEN_MAX Dateien zu je höchstens PROT_GROESSE_MAX, jede mit eigenem Speicher, der danach frei wird. */
#define PROT_DATEIEN_MAX 40
#define PROT_GROESSE_MAX (2LL * 1024 * 1024)
static const char *prot_ohne_ort(const char *b)
{
    static const char *const V[] = { "M.2 – ", "USB – ", "Erweiterter Speicher – ", "Extended storage – ", "USB-Stick – ", "USB stick – ", "USB-Laufwerk – ", "USB drive – " };
    for (size_t i = 0; i < sizeof V / sizeof V[0]; i++) if (!strncmp(b, V[i], strlen(V[i]))) return b + strlen(V[i]);
    return b;
}
/* Codex-Prüfung 109 (P2): Teile eines Blocks stehen eingerückt im Protokoll ("gelöscht      – M.2 – X (ID) - ShadowMount-Spiel", seit 1.0.8
 * auch die ShadowMount-Kachel im Block ihrer gleichnamigen Tool-Kachel) - die Einrückung gehört nicht zum Namen */
static const char *prot_ohne_block(const char *b)
{
    while (*b == ' ') b++;
    if (!strncmp(b, "– ", strlen("– "))) b += strlen("– ");
    return b;
}
static void prot_zeile(kern *k, arena *t, const char *z, bool *ps4)
{
    htreffer zt = h_mt(t, z, "^\\d\\d:\\d\\d:\\d\\d (.*)$");
    if (!zt.ok) { *ps4 = false; return; }
    const char *r = zt.gr[1], *body = NULL;
    bool liste = false;
    if (!strncmp(r, "   ", 3) && r[3] != ' ') { if (*ps4) { body = r + 3; liste = true; } }
    else {
        *ps4 = !strncmp(r, "PS4-Spiele (", 12) || !strncmp(r, "PS4 games (", 11);
        htreffer s = h_mt(t, r, "^  (?:gelöscht|deleted|unverändert|unchanged)   (.*)$");
        if (s.ok) body = prot_ohne_block(s.gr[1]);
    }
    if (!body) return;
    /* NEU 1.0.8 (PS5-Test 07.11.2026 17:43: der Fortschritt NPWR13863_00 blieb "unbekannt", weil seine Liste "L.A. Noire (NPWR13863_00)"
     * vier Minuten vorher gelöscht worden war): auch gelöschte oder unveränderte Trophäen-Listen und Fortschritte nennen ihren Namen */
    if (!liste) {
        htreffer mt = h_mt(t, body, "(?-i)^(?:Trophäen-Liste|Trophy list|Trophäen-Fortschritt|Trophy progress): (.+?) \\((NPWR\\d{5}_\\d{2})\\)(?: |$)");
        if (mt.ok) {
            const char *n = h_trim(t, mt.gr[1]);
            if (!mt_hat(&k->PROT_NPWR, mt.gr[2]) && net_laenge(n) >= 1 && net_laenge(n) <= 100 && !h_m(n, "^NPWR\\d{5}_\\d{2}") && !strstr(n, "Spiel/App unbekannt")
                && !strstr(n, "game/app unknown")) mt_setze(A, &k->PROT_NPWR, a_kopie(A, mt.gr[2]), (void *)a_kopie(A, n));
            return;
        }
    }
    htreffer m = h_mt(t, prot_ohne_ort(body), liste ? "^(.+?) \\(([A-Z]{4}\\d{5})\\) – "
                                                    : "^(.+?) \\(([A-Z]{4}\\d{5})\\) - (?:Spiel|game|ShadowMount-Spiel|ShadowMount game)(?: |$)");
    if (!m.ok || mt_hat(&k->PROT_NAMEN, m.gr[2])) return;
    const char *n = h_trim(t, m.gr[1]);
    if (net_laenge(n) < 2 || net_laenge(n) > 100 || strchr(n, '(') == n) return;
    if (h_m(n, "^[A-Z]{4}\\d{5}$")) return;   /* Codex-Prüfung 109 (P3): ohne lesbaren Namen steht dort die Kennung selbst ("CUSA09172 (CUSA09172)") */
    static const char *const PLATZ[] = { "unbekannter Name", "unknown name", "entferntes Spiel", "removed game" };
    for (size_t i = 0; i < sizeof PLATZ / sizeof PLATZ[0]; i++) if (!strcmp(n, PLATZ[i])) return;
    if (strstr(n, "Spiel/App unbekannt") || strstr(n, "game/app unknown")) return;
    mt_setze(A, &k->PROT_NAMEN, a_kopie(A, m.gr[2]), (void *)a_kopie(A, n));
}
/* Codex-Prüfung 108 (P2): gelesen werden höchstens so viele Bytes, wie die Ordnerliste nannte (bis PROT_GROESSE_MAX) - wächst die Datei
 * inzwischen, bleibt es dabei; zwischen den Dateien und alle 256 Zeilen wird das Abbrechen geprüft */
static void prot_datei(kern *k, arena *tf, const char *pfad, long long groesse)
{
    sitzung *ts = sitzung_neu(k->S->z, tf); ts->abbruch = k->S->abbruch; ts->abbruch_ctx = k->S->abbruch_ctx;
    if (groesse <= 0 || groesse > PROT_GROESSE_MAX) return;
    unsigned char *b = a_hole(tf, (size_t)groesse + 1);
    size_t len = 0;
    if (!s_lies_ab(ts, pfad, 0, (size_t)groesse, b, &len) || len > (size_t)groesse) return;
    bool ps4 = false;
    size_t q = 0; int z = 0;
    while (q < len) {
        if ((++z & 255) == 0 && k->abbruch) wirf(AUSN_ABBRUCH, "");
        size_t e = q;
        while (e < len && b[e] != '\n') e++;
        size_t l = e - q;
        if (l > 0 && b[q + l - 1] == '\r') l--;
        if (l > 0 && l < 1000 && !memchr(b + q, 0, l)) prot_zeile(k, tf, a_kopie_n(tf, (const char *)b + q, l), &ps4); else ps4 = false;
        q = e + 1;
    }
}
typedef struct { const char *name, *pfad; long long gr; } prot_dat;
static int prot_vgl_dat(const void *x, const void *y) { return -strcmp(((const prot_dat *)x)->name, ((const prot_dat *)y)->name); }
static void prot_dateien(kern *k, arena *t)
{
    sitzung *ts = sitzung_neu(k->S->z, t); ts->abbruch = k->S->abbruch; ts->abbruch_ctx = k->S->abbruch_ctx;
    prot_dat *da = NULL; int nd = 0, cap = 0;   /* sortiert nach dem Dateinamen (Datum und Uhrzeit), neueste zuerst */
    const char *orte[10]; int no = 0;
    for (int u = 0; u < 8; u++) orte[no++] = a_fmt(t, "/mnt/usb%d", u);
    orte[no++] = "/user/data/PS5-Cleanup-Protokolle"; orte[no++] = "/mnt/ext1/PS5-Cleanup-Protokolle";
    for (int o = 0; o < no; o++) {
        dliste *l = s_liste(ts, orte[o], false);
        for (int i = 0; l && i < l->n; i++) {
            const datei *d = &l->e[i];
            if (d->ist_ordner || d->ist_link || d->groesse <= 0 || d->groesse > PROT_GROESSE_MAX || !h_m(d->name, "(?i)^ps5cleanup_protokoll_.*\\.txt$")) continue;
            if (nd == cap) { cap = cap ? cap * 2 : 16; prot_dat *nn = a_hole(t, sizeof(prot_dat) * (size_t)cap); if (nd) memcpy(nn, da, sizeof(prot_dat) * (size_t)nd); da = nn; }
            da[nd++] = (prot_dat){ d->name, a_fmt(t, "%s/%s", orte[o], d->name), d->groesse };
        }
    }
    if (nd > 1) qsort(da, (size_t)nd, sizeof(prot_dat), prot_vgl_dat);
    for (int i = 0; i < nd && i < PROT_DATEIEN_MAX; i++) {
        if (k->abbruch) wirf(AUSN_ABBRUCH, "");
        arena *tf = arena_neu();
        VERSUCHE { prot_datei(k, tf, da[i].pfad, da[i].gr); }
        FANGE { arena_frei(tf); if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); continue; } ENDE_VERSUCHE;   /* nicht lesbar: nächste Datei */
        arena_frei(tf);
    }
}
static void protokoll_namen_lesen(kern *k)
{
    arena *t = arena_neu();
    VERSUCHE { prot_dateien(k, t); }
    FANGE { arena_frei(t); if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); return; } ENDE_VERSUCHE;   /* Namen sind ein Zusatz - kein Abbruch der Suche */
    arena_frei(t);
}
/* Zuordnungen und kaputte Zusatzinhalt-Angaben ermitteln - einmal je Suche, nach dem Lesen der Kachel-Namen */
static void verknuepfungen_ermitteln(kern *k, const sliste *const *quellen, int nq)
{
    k->VERKNUEPFT = mt_neu(true); k->VERK_SPST = mt_neu(true); k->VERK_BELEG = mt_neu(true); k->INSTALLIERT = mt_neu(true);
    k->SID_KAPUTT = mt_neu(true); k->SID_GEZAEHLT = mt_neu(true); k->SID_WARNUNG = (sliste){0}; k->REST_NAMEN = mt_neu(true);
    k->PROT_NAMEN = mt_neu(true); k->SPST_TITEL = mt_neu(true); k->PROT_NPWR = mt_neu(true);   /* NEU 1.0.8 */
    if (kern_bereiche_106()) return;
    protokoll_namen_lesen(k);   /* NEU 1.0.8 (PS5-Test 07.11.2026) */
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
#define JB_ABBILD "^(?=.*" JB_KENNUNG(JB_IDS) ").*\\.(?i:" ABBILD_ENDUNGEN ")$"
static bool jb_pfad(const char *p) { return h_m(p, JB_MUSTER) || h_m(p, JB_ABBILD); }
bool kern_jb_abbild(const char *p) { return h_m(p, JB_ABBILD); }   /* Codex-Prüfung 37: für den Nachkommenschutz beim Löschen */
static bool ist_abbild(const char *p) { return h_m(p, "(?i)\\.(" ABBILD_ENDUNGEN ")$"); }
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
    /* NEU 1.0.8 (F7, da_Chris86s Bilder vom 04.11.2026): die Karte passt zur Liste - der Browserspeicher steht unter "Jailbreak", also auch
     * hier (als "möglich", sicher erkennen lässt es sich nicht); die Disc hat keinen Eintrag, darum "nichts zu löschen" */
    if (browser && !kern_bereiche_106()) sl_add(a, &namen, T("WebKit im Browser (möglich)", "WebKit in the browser (possible)"));
    if (k->BD_DISC) sl_add(a, &namen, kern_bereiche_106() ? T("Blu-ray-Jailbreak (Disc im Laufwerk)", "Blu-ray jailbreak (disc in the drive)")
                                                          : T("Blu-ray-Disc im Laufwerk (nichts zu löschen)", "Blu-ray disc in the drive (nothing to delete)"));   /* NEU 1.0.6 */
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
            /* NEU 1.0.8 (Arbeitsplan 2.7): Mitschnitt und Bilder der Test-Fassung nie anbieten */
            if (kern_test_ordner(k, pfade->e[i])) continue;
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
            /* NEU 1.0.8 (F1): "wird entfernt" stimmt nur bei der vollständigen Bereinigung - nach einer Auswahl fragt PS5 Cleanup am Ende */
            o.sperrtext = nur_eigen ? (kern_bereiche_106() ? T("  [PS5 Cleanup - wird zum Schluss entfernt]", "  [PS5 Cleanup - removed at the end]")
                                                           : T("  [PS5 Cleanup selbst - kommt erst ganz am Ende dran]", "  [PS5 Cleanup itself - only at the very end]"))
                                    : T("  [GESPERRT: nicht sicher prüfbar]", "  [LOCKED: cannot be checked safely]");
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
    /* NEU 1.0.9 (08.11.2026, ShadowMountPlus 1.7 src/sm_config_file.c): Einstellungen im Format 2 ("[config] version = 2") haben Abschnitte -
     * eigene Suchorte stehen dort als "path = ..." unter [scan], der Wert auch in Anführungszeichen (darin \\ und \"). Abschnitt und
     * Schlüssel ohne Rücksicht auf Groß- und Kleinschreibung wie dort. Das alte "scanpath=" gilt weiter. */
    const char *abschnitt = "";
    for (int j = 0; j < z.n; j++) {
        htreffer ab = h_mt(A, z.e[j], "^\\s*\\[\\s*([^\\]]*?)\\s*\\]\\s*([#;].*)?$");
        if (ab.ok) { abschnitt = ab.gr[1]; continue; }
        const char *o = NULL;
        htreffer t = h_mt(A, z.e[j], "^\\s*(?i:scanpath)\\s*=\\s*(/[^#;]*?)\\s*([#;].*)?$");   /* Kommentar nach # oder ; wie ShadowMountPlus */
        if (t.ok) o = t.gr[1];
        else if (txt_gleich_ci(abschnitt, "scan")) {
            htreffer q = h_mt(A, z.e[j], "^\\s*(?i:path)\\s*=\\s*\"((?:[^\"\\\\]|\\\\[\\\\\"])*)\"\\s*([#;].*)?$");
            if (q.ok) o = h_r(A, q.gr[1], "\\\\([\\\\\"])", "$1");
            else { htreffer u = h_mt(A, z.e[j], "^\\s*(?i:path)\\s*=\\s*([^#;\"]*?)\\s*([#;].*)?$"); if (u.ok) o = u.gr[1]; }
        }
        if (!o || o[0] != '/') continue;
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
/* Codex-Prüfung 93: frei_d bekommt zu jedem freien Pfad seinen Eintrag aus der Ordnerliste (Ordner? Größe? - für die PKG-Dateien) */
static void sammle_ohne_schutz(kern *k, const char *p, sliste *frei, pliste *frei_d, sliste *eig)
{
    sl_add(A, &k->AUFGEKLAPPT, p);
    dliste *l = s_liste(k->S, p, true);
    for (int i = 0; l && i < l->n; i++) {
        const char *q = VERB(A, p, "/", l->e[i].name);
        if (!l->e[i].ist_link && l->e[i].ist_ordner && kern_selbstschutz_enthaelt(k, q)) sammle_ohne_schutz(k, q, frei, frei_d, eig);
        else if (!kern_selbstschutz_anbieten(k, q)) sl_add(A, eig, q);
        else { sl_add(A, frei, q); pl_add(A, frei_d, &l->e[i]); }
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
            if (!d->ist_ordner && (h_m(d->name, "(?i)\\.(" ABBILD_ENDUNGEN ")$")
                                   || (h_m(d->name, "(?i)\\.iso$") && h_m(d->name, "(?i)(^|[^a-z])[a-z]{4}[-_]?\\d{5}")))) ja = true;
            else if (d->ist_ordner && h_m(d->name, "(^|[^A-Z0-9])[A-Z]{4}\\d{5}([^0-9]|$)") && spiel_ordner(k, VERB(A, p, "/", d->name))) ja = true;
        }
    }
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); ja = false; } ENDE_VERSUCHE;
    return ja;
}
/* NEU 1.0.8 (Arbeitsplan 2.2): Spielname und Spielnummer aus einer PS4-PKG-Datei. Kopf "\x7FCNT" (Werte big-endian), in der
 * Eintragstabelle steht param.sfo unter der Kennung 0x1000 (nicht verschlüsselt). Anderes Format, Lesefehler, kein param.sfo: "" - dann
 * bleibt es beim Dateinamen wie bisher.
 * Geändert 05.11.2026 (Prüfung an da_Chris86s echten PKG-Dateien: 2 von 3 bekamen keinen Namen): Bei großen PKGs liegt param.sfo weit
 * hinten, hinter den Bildern, oft nach mehr als 1 MB. Statt vom Anfang an zu lesen, werden jetzt nur drei kleine Stücke an festen Stellen
 * gelesen: der Kopf (96 Byte), die Eintragstabelle (höchstens 8192 Einträge zu 32 Byte) und param.sfo selbst (höchstens 64 KB). */
#define PKG_KOPF 0x60
#define PKG_EINTRAEGE_MAX 8192
#define PKG_SFO_MAX 65536
static uint32_t pkg_be32(const unsigned char *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3]; }
/* Codex-Prüfung 71 (P2): aus param.sfo (Werte little-endian) nur EIN Textfeld holen - direkt aus dem Puffer, höchstens max-1 Byte,
 * ohne die ganze Datei in den Kern-Speicher zu lesen (eine manipulierte Datei konnte sonst viele MB belegen). false = nicht da. */
static uint32_t pkg_le32(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static bool pkg_sfo_feld(const unsigned char *b, size_t n, const char *schl, char *aus, size_t max)
{
    aus[0] = 0;
    if (n < 20 || memcmp(b, "\0PSF", 4) != 0) return false;
    size_t kt = pkg_le32(b + 8), dt = pkg_le32(b + 12), anz = pkg_le32(b + 16), sl = strlen(schl);
    if (anz > 1000 || 20 + anz * 16 > n || kt > n || dt > n) return false;
    for (size_t i = 0; i < anz; i++) {
        const unsigned char *e = b + 20 + i * 16;
        size_t ko = kt + ((size_t)e[0] | (size_t)e[1] << 8), art = (size_t)e[2] | (size_t)e[3] << 8, dl = pkg_le32(e + 4), dof = dt + pkg_le32(e + 12);
        if (ko > n || n - ko < sl + 1 || memcmp(b + ko, schl, sl + 1) != 0) continue;
        if ((art != 0x0204 && art != 0x0004) || dof > n || dl > n - dof) return false;   /* nur Text */
        size_t m = dl < max ? dl : max - 1;
        memcpy(aus, b + dof, m); aus[m] = 0;
        return true;
    }
    return false;
}
/* Anzeige-Text aus einer fremden Datei: Steuerzeichen werden Leerzeichen, ungültiges UTF-8 wird "?", ein am Ende
 * abgeschnittenes Zeichen fällt weg */
static const char *pkg_text(arena *a, const char *t)
{
    char *c = a_kopie(a, t ? t : "");
    unsigned char *q = (unsigned char *)c;
    while (*q) {
        int l = *q < 0x80 ? 1 : (*q & 0xE0) == 0xC0 ? 2 : (*q & 0xF0) == 0xE0 ? 3 : (*q & 0xF8) == 0xF0 ? 4 : 0, i = 1;
        while (i < l && (q[i] & 0xC0) == 0x80) i++;
        if (l == 0 || i < l) {                                   /* ungültig oder unvollständig */
            if (l > 1 && q[i] == 0) { *q = 0; break; }           /* am Ende abgeschnitten: das halbe Zeichen fällt weg */
            *q++ = '?'; continue;
        }
        if (l == 1 && *q < 0x20) *q = ' ';
        q += l;
    }
    return c;
}
static size_t pkg_zuletzt;   /* nur für die PC-Tests: so viele Bytes las der letzte Aufruf */
static bool pkg_lies(kern *k, const char *p, unsigned long long ab, size_t n, unsigned char *ziel, size_t *gelesen)
{
    size_t len = 0;
    if (!s_lies_ab(k->S, p, (long long)ab, n, ziel, &len) || len != n) return false;
    *gelesen += n;
    return true;
}
/* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 12): die gelesenen Angaben auch einzeln - Spielnummer, Name, Art (Spiel, Update, Zusatzinhalt,
 * App) und Version. Art aus CATEGORY: gd = Spiel (auch gdO, PS2-Klassiker), gp... = Update, ac = Zusatzinhalt, andere gd... = App. */
/* NEU 1.0.8 (PS5-Test 07.11.2026): dazu der Anfang der Trophäen-Datei (Eintrag 0x1400, trophy/trophy00.trp, nicht verschlüsselt). Sony
 * legt genau diese Datei beim Installieren als /user/trophy/conf/<NPWR>/TROPHY.TRP ab - der Kopf (96 Byte mit Größe und Prüfsumme) ist
 * gleich (geprüft an L.A. Noire und Red Dead Redemption 2). So bekommt eine Trophäen-Liste ihren Namen, auch wenn das Spiel weg ist. */
/* NEU 1.0.8 (PS5-Test 07.11.2026 17:32, DRIVECLUB BIKES NPWR09396_00): ein PKG kann mehrere Trophäen-Dateien tragen (0x1400 trophy00.trp,
 * 0x1401 trophy01.trp, ...) - im Update von DRIVECLUB steckt die Liste von DRIVECLUB BIKES als 0x1401. Gelesen werden die ersten 8. */
#define PKG_TRP_MAX 8
/* NEU 1.0.9 (da_Chris86 08.11.2026 01:19: Bilder auch für nicht installierte Spiele): dazu, wo in der PKG-Datei das Bild des Spiels liegt
 * (icon0.png, Eintrag 0x1200). Nur wenn es nicht verschlüsselt ist und wie jede PNG-Datei beginnt - gelesen werden davon hier nur 8 Byte,
 * das Bild selbst liest die Oberfläche (spiel_bild). */
#define PKG_BILD_MAX (4u * 1024 * 1024)   /* wie die Oberfläche */
typedef struct { const char *id, *titel, *ver; int art; int trp_n; unsigned char trp[PKG_TRP_MAX][96]; uint32_t bild_ab, bild_len; } pkginfo;
static const char *pkg_zusatz_info(kern *k, const char *p, pkginfo *pi);
static const char *pkg_zusatz(kern *k, const char *p) { return pkg_zusatz_info(k, p, NULL); }
static const char *pkg_zusatz_info(kern *k, const char *p, pkginfo *pi)
{
    if (pi) *pi = (pkginfo){ NULL, "", "", PKG_KEINE, 0, {{0}}, 0, 0 };
    unsigned char kopf[PKG_KOPF], trp[PKG_TRP_MAX][96];
    unsigned char *volatile tab = NULL, *volatile sb = NULL;
    volatile size_t sn = 0, g = 0;
    volatile int trp_n = 0;
    volatile uint32_t bild_ab = 0, bild_len = 0;
    pkg_zuletzt = 0;
    VERSUCHE {
        size_t gl = 0;
        if (pkg_lies(k, p, 0, sizeof kopf, kopf, &gl) && memcmp(kopf, "\x7F" "CNT", 4) == 0) {
            uint32_t anz = pkg_be32(kopf + 0x10), to = pkg_be32(kopf + 0x18);
            if (anz > 0 && anz <= PKG_EINTRAEGE_MAX && to >= PKG_KOPF && (tab = malloc((size_t)anz * 32)) != NULL && pkg_lies(k, p, to, (size_t)anz * 32, tab, &gl)) {
                int i_sfo = -1, i_trp[PKG_TRP_MAX], n_trp = 0, i_bild = -1;
                for (uint32_t i = 0; i < anz; i++) {
                    uint32_t eid = pkg_be32(tab + (size_t)i * 32);
                    if (eid == 0x1000 && i_sfo < 0) i_sfo = (int)i;
                    else if (eid == 0x1200 && i_bild < 0) i_bild = (int)i;   /* NEU 1.0.9: icon0.png */
                    else if (eid >= 0x1400 && eid <= 0x1463 && n_trp < PKG_TRP_MAX) i_trp[n_trp++] = (int)i;   /* trophy00.trp bis trophy99.trp */
                }
                if (i_sfo >= 0) {
                    const unsigned char *e = tab + (size_t)i_sfo * 32;
                    uint32_t o = pkg_be32(e + 16), s = pkg_be32(e + 20);
                    if (s >= 20 && s <= PKG_SFO_MAX && (sb = malloc(s)) != NULL && pkg_lies(k, p, o, s, sb, &gl)) sn = s;
                }
                for (int j = 0; j < n_trp && !kern_bereiche_106(); j++) {   /* NEU 1.0.8: nur wenn nicht verschlüsselt (Bit 31 der ersten Kennzeichen) */
                    const unsigned char *e = tab + (size_t)i_trp[j] * 32;
                    uint32_t o = pkg_be32(e + 16), s = pkg_be32(e + 20);
                    unsigned char *z = trp[trp_n];
                    if (!(pkg_be32(e + 8) & 0x80000000u) && s >= 96 && pkg_lies(k, p, o, 96, z, &gl)
                        && z[0] == 0xDC && z[1] == 0xA2 && z[2] == 0x4D && z[3] == 0x00) trp_n++;
                }
                if (i_bild >= 0 && !kern_bereiche_106()) {   /* NEU 1.0.9: nur nicht verschlüsselt und wirklich eine PNG-Datei */
                    const unsigned char *e = tab + (size_t)i_bild * 32;
                    uint32_t o = pkg_be32(e + 16), s = pkg_be32(e + 20);
                    unsigned char sig[8];
                    if (!(pkg_be32(e + 8) & 0x80000000u) && s >= 8 && s <= PKG_BILD_MAX && pkg_lies(k, p, o, sizeof sig, sig, &gl)
                        && !memcmp(sig, "\x89PNG\r\n\x1a\n", 8)) { bild_ab = o; bild_len = s; }
                }
            }
        }
        g = gl;
    } FANGE { free(tab); free(sb); if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); tab = sb = NULL; sn = 0; trp_n = 0; bild_ab = bild_len = 0; } ENDE_VERSUCHE;
    if (pi && trp_n > 0) { pi->trp_n = trp_n; memcpy(pi->trp, trp, sizeof trp[0] * (size_t)trp_n); }
    if (pi && bild_len) { pi->bild_ab = bild_ab; pi->bild_len = bild_len; }
    const char *z = "";
    if (sn > 0) {
        char f_ti[128], f_id[16], f_kat[8], f_ver[16];   /* Titel wie kern_sfo_titel: TITLE_04, TITLE_01, TITLE */
        const unsigned char *b = sb;
        if (!pkg_sfo_feld(b, sn, "TITLE_04", f_ti, sizeof f_ti) || !f_ti[0])
            if (!pkg_sfo_feld(b, sn, "TITLE_01", f_ti, sizeof f_ti) || !f_ti[0]) pkg_sfo_feld(b, sn, "TITLE", f_ti, sizeof f_ti);
        pkg_sfo_feld(b, sn, "TITLE_ID", f_id, sizeof f_id); pkg_sfo_feld(b, sn, "CATEGORY", f_kat, sizeof f_kat); pkg_sfo_feld(b, sn, "APP_VER", f_ver, sizeof f_ver);
        const char *ti = pkg_text(A, f_ti), *id = pkg_text(A, f_id), *kat = f_kat, *ver = pkg_text(A, f_ver);
        if (!id[0] && kopf[0x46] == '-') {   /* sonst die Spielnummer aus der Inhaltskennung im Kopf (UP0001-CUSA12345_00-...) */
            char t[10]; memcpy(t, kopf + 0x47, 9); t[9] = 0;
            if (h_m(t, "(?-i)^[A-Z]{4}\\d{5}$")) id = a_kopie(A, t);
        }
        if (ti[0] || id[0]) {
            const char *art = !strncmp(kat, "gp", 2) ? VERB(A, T(", Update", ", update"), ver[0] ? " " : "", ver) : !strncmp(kat, "ac", 2) ? T(", Zusatzinhalt", ", add-on") : "";
            z = ti[0] && id[0] ? VERB(A, "  [PKG: ", ti, " (", id, ")", art, "]") : VERB(A, "  [PKG: ", ti[0] ? ti : id, art, "]");
            if (pi) {
                pi->id = id[0] && h_m(id, "(?-i)^[A-Z]{4}\\d{5}$") ? id : NULL; pi->titel = h_trim(A, ti); pi->ver = ver;
                pi->art = !strncmp(kat, "gp", 2) ? PKG_UPDATE : !strncmp(kat, "ac", 2) ? PKG_DLC
                        : (!strcmp(kat, "gd") || !strcmp(kat, "gdO") || !kat[0]) ? PKG_SPIEL : !strncmp(kat, "gd", 2) ? PKG_APP : PKG_SPIEL;
            }
        }
    }
    pkg_zuletzt = g;
    free(tab); free(sb);
    return z;
}
/* NEU 1.0.8 (Codex-Prüfung 72): nur für die PC-Tests - wie pkg_zusatz, dazu wie viele Bytes gelesen wurden */
const char *kern_pkg_zusatz_test(kern *k, const char *p, size_t *gelesen) { const char *z = pkg_zusatz(k, p); *gelesen = pkg_zuletzt; return z; }
/* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 12): eine PKG-Datei als eigener Eintrag. Was der Kopf nicht hergibt, kommt aus dem Dateinamen
 * ("L.A. Noire - CUSA09172 - v1.04 - EU - Update.pkg": Spielnummer, Name davor, Update). Ohne Spielnummer bleibt sie unter "Sonstiges". */
static void merke_pkg(kern *k, const char *p, const char *anzeige, long long gr)
{
    pkginfo pi;
    const char *z = pkg_zusatz_info(k, p, &pi), *bl = h_blatt(A, p);
    /* NEU 1.0.8 (PS5-Test 07.11.2026, L.A. Noire): Name und Trophäen-Datei aus dem KOPF (nie aus dem Dateinamen) für Spielstände,
     * Trophäen-Listen und Reste des Spiels, die schon "unbekannt" angelegt sind (namen_nachtragen). Ein Zusatzinhalt trägt seinen
     * eigenen Namen, nicht den des Spiels - zählt darum nicht. Der erste Fund gilt. */
    if (!kern_bereiche_106() && pi.id && pi.titel[0] && pi.art != PKG_DLC && pi.art != PKG_KEINE && net_laenge(pi.titel) >= 2 && net_laenge(pi.titel) <= 100) {
        if (!mt_hat(&k->PKG_NAMEN, pi.id)) mt_setze(A, &k->PKG_NAMEN, a_kopie(A, pi.id), (void *)a_kopie(A, pi.titel));
        for (int j = 0; j < pi.trp_n; j++) {
            char hex[sizeof pi.trp[0] * 2 + 1];
            for (size_t i = 0; i < sizeof pi.trp[0]; i++) snprintf(hex + 2 * i, 3, "%02x", pi.trp[j][i]);
            if (!mt_hat(&k->PKG_TRP, hex)) {
                mt_setze(A, &k->PKG_TRP, a_kopie(A, hex), (void *)a_kopie(A, pi.titel));
                if (pi.trp_n > 1) mt_setze(A, &k->PKG_TRP_NR, a_kopie(A, hex), (void *)(intptr_t)(j + 1));   /* "Trophäen-Liste 2" wie bei installierten Spielen */
            }
        }
        /* NEU 1.0.9 (da_Chris86 08.11.2026 01:19): das Bild aus der PKG-Datei für die Kachel-Vorschau, wenn das Spiel nicht installiert ist.
         * Die Spiel- oder App-PKG geht vor einem Update (dessen Bild ist meist dasselbe); ein Zusatzinhalt trägt sein eigenes Bild und zählt
         * nicht (oben ausgeschlossen). Ein Zeilenumbruch im Pfad würde die Angabe "Pfad\nAnfang\nLänge" zerlegen - dann kein Bild. */
        bool vom_upd = mt_hat(&k->BILD_UPD, pi.id) && mt_hole(&k->BILD_UPD, pi.id);
        if (pi.bild_len && !strchr(p, '\n') && (!mt_hat(&k->BILD_ORT, pi.id) || (vom_upd && pi.art != PKG_UPDATE))) {
            mt_setze(A, &k->BILD_ORT, a_kopie(A, pi.id), (void *)a_fmt(A, "%s\n%u\n%u", p, (unsigned)pi.bild_ab, (unsigned)pi.bild_len));
            mt_setze(A, &k->BILD_UPD, a_kopie(A, pi.id), (void *)(intptr_t)(pi.art == PKG_UPDATE));
        }
    }
    sliste l = L1(A, p);
    MERKE(k, &l, 5, VERB(A, anzeige, z), 0);
    if (!k->MERKE_OK || k->bau.n == 0) return;
    eintrag *f = k->bau.e[k->bau.n - 1];
    htreffer m = h_mt(A, bl, "(?-i)^(.*?)[\\s_\\-\\[\\(]*(?:^|[^A-Za-z0-9])([A-Z]{4}\\d{5})(?:[^0-9]|$)");
    if (!pi.id && m.ok) pi.id = m.gr[2];
    if (!pi.titel[0] && m.ok && !h_m(m.gr[1], "(?-i)^[A-Z]{2}\\d{4}$")) pi.titel = h_trim(A, h_r(A, m.gr[1], "[\\s_\\-\\[\\(]+$", ""));   /* nicht "EP0001" aus EP0001-CUSA... */
    if (!pi.art) pi.art = h_m(bl, "(?i)(^|[^a-z])(update|patch)([^a-z]|$)") ? PKG_UPDATE : h_m(bl, "(?i)(^|[^a-z])(dlc|add-?ons?|zusatz\\w*)([^a-z]|$)") ? PKG_DLC : PKG_SPIEL;
    if (!pi.ver[0] && pi.art == PKG_UPDATE) { htreffer v = h_mt(A, bl, "(?i)(?:^|[^a-z0-9])v(\\d+(?:\\.\\d+)+)"); if (v.ok) pi.ver = v.gr[1]; }
    if (pi.art == PKG_DLC) {   /* im Kopf eines Zusatzinhalts steht SEIN Name - der Name des Spiels kommt dann aus dem Dateinamen */
        const char *dn = m.ok && !h_m(m.gr[1], "(?-i)^[A-Z]{2}\\d{4}$") ? h_trim(A, h_r(A, m.gr[1], "[\\s_\\-\\[\\(]+$", "")) : "";
        if (pi.titel[0] && strcmp(pi.titel, dn) != 0) pi.ver = pi.titel;
        pi.titel = dn;
    }
    f->PkgArt = pi.art; f->PkgId = pi.id; f->PkgTitel = pi.titel; f->PkgVer = pi.ver; f->PkgAnzahl = 1; f->PkgBytes = gr;
}
/* NEU 1.0.8 (Befund 12): PKG-Dateien in Ordnern (da_Chris86s USB-Laufwerk: "PS4" mit den PKG-Dateien von Heavy Rain, Red Dead Redemption
 * und L.A. Noire stand als EIN Eintrag unter "Sonstiges"). Sammelt die PKG-Dateien unter p (höchstens PKG_TIEFE Ebenen tief, höchstens
 * PKG_LISTE_MAX gelistete Einträge); rest bekommt alles andere, so grob wie möglich (ein Ordner ohne PKG-Datei als Ganzes).
 * Rückgabe: Anzahl PKG-Dateien, -1 = nicht lesbar oder Grenze erreicht (dann bleibt der Ordner, wie er war). Verknüpfungen nie folgen.
 * Codex-Prüfung 83: Ein Unterordner unter der Tiefengrenze ist ungeprüft - auch er heißt -1 (bis dahin kam er als Ganzes in den Rest,
 * "(ohne PKG-Dateien)" hätte dann doch PKG-Dateien mitgelöscht). Dafür 8 statt 3 Ebenen. Zusätzlich eine Grenze für die ganze Suche
 * (PKG_GESAMT_MAX gelistete Einträge): jede Liste bleibt im Speicher des Kerns, viele große Ordner hätten sonst viel Speicher gekostet.
 * 06.11.2026 abends: Die Listen liegen inzwischen nur noch kurz in eigenem Speicher (Codex-Prüfung 83) - die Grenze der ganzen Suche ist
 * darum 100000 statt 20000. Seit Codex-Prüfung 90 steht jeder Ordner rot da, in dem nicht gesucht werden konnte; bei 20000 wären auf
 * einer vollen Konsole sonst ab einem Punkt alle weiteren Ordner rot geworden (da_Chris86s Frage 21:19 "Sicher, dass alles fertig ist?"). */
#define PKG_TIEFE 8
#define PKG_LISTE_MAX 10000
#define PKG_GESAMT_MAX 100000
/* Codex-Prüfung 96: höchstens so viele PKG-Dateien aus Ordnern je Suche - jede wird gelesen (Kopf im Speicher des Kerns) und auf "doppelt"
 * verglichen; mit 100000 gelisteten Einträgen allein wären bis zu 100000 möglich (Speicherbremse). Ein Ordner, dessen PKG-Dateien nicht mehr
 * hineinpassen, bleibt EIN Eintrag, rot (PkgOffen) - Ordner ohne PKG-Datei betrifft das nicht. Für Tests kleiner einstellbar. */
#ifndef PKG_DATEIEN_MAX
#define PKG_DATEIEN_MAX 20000
#endif
static int pkg_sammeln(kern *k, sitzung *ts, arena *t, const char *p, int tiefe, sliste *pk, sliste *gr, sliste *rest, sliste *auf, int *budget)
{
    dliste *volatile dl = NULL;
    VERSUCHE { dl = s_liste(ts, p, false); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); dl = NULL; } ENDE_VERSUCHE;
    if (!dl || (*budget -= dl->n) < 0) return -1;
    int n = 0; sliste r = {0};
    for (int i = 0; i < dl->n; i++) {
        const datei *e = &dl->e[i];
        const char *cp = VERB(t, p, "/", e->name);
        if (e->ist_link || ist_zwischendatei(e->name)) { sl_add(t, &r, cp); continue; }
        if (!e->ist_ordner) {
            if (h_m(e->name, "(?i)\\.pkg$")) { sl_add(t, pk, cp); sl_add(t, gr, a_fmt(t, "%lld", e->groesse)); n++; } else sl_add(t, &r, cp);
            continue;
        }
        if (tiefe + 1 > PKG_TIEFE) return -1;          /* Codex-Prüfung 83: ungeprüft - nie als "ohne PKG-Dateien" in den Rest */
        sliste unter = {0};
        int m = pkg_sammeln(k, ts, t, cp, tiefe + 1, pk, gr, &unter, auf, budget);
        if (m < 0) return -1;
        if (m == 0) sl_add(t, &r, cp); else { n += m; sl_add_alle(t, &r, &unter); sl_add(t, auf, cp); }   /* Codex-Prüfung 94: aufgeteilt (innere zuerst) */
    }
    sl_add_alle(t, rest, &r);
    return n;
}
/* Ordner p (Anzeige "anzeige") mit PKG-Dateien: jede PKG-Datei ein eigener Eintrag, der übrige Inhalt EIN Eintrag "(ohne PKG-Dateien)".
 * 1 = aufgeteilt; 0 = keine PKG-Datei darin - dann bleibt der Ordner ein Eintrag wie bisher; -1 = nicht prüfbar (Grenze, zu tief, nicht
 * lesbar) - Codex-Prüfung 90: dann bleibt er auch EIN Eintrag, kann aber PKG-Dateien enthalten (der Aufrufer setzt PkgOffen).
 * Codex-Prüfung 83 (P2): gelistet wird mit eigener Sitzung in eigenem Speicher (wie das Zählen der Größen, kern_groessen_im_hintergrund);
 * in den Speicher des Kerns kommen nur die Pfade, die als Einträge bleiben. */
static int merke_pkg_ordner(kern *k, const char *p, const char *anzeige, int kat, const char *werkzeug)
{
    int frei = PKG_GESAMT_MAX - k->PKG_GELISTET;
    if (frei <= 0) return -1;                           /* Grenze der ganzen Suche erreicht: der Ordner bleibt ein Eintrag, ungeprüft */
    int budget = frei < PKG_LISTE_MAX ? frei : PKG_LISTE_MAX, start = budget;
    arena *t = arena_neu();
    sitzung *ts = sitzung_neu(k->S->z, t); ts->abbruch = k->S->abbruch; ts->abbruch_ctx = k->S->abbruch_ctx;
    sliste pk = {0}, pg = {0}, rest = {0}, auf = {0};
    volatile int r = -1;
    VERSUCHE { r = pkg_sammeln(k, ts, t, p, 0, &pk, &pg, &rest, &auf, &budget); }
    FANGE { arena_frei(t); WEITERWERFEN(); } ENDE_VERSUCHE;   /* ausn_text ist ein eigener Puffer - frei geben ist hier sicher */
    k->PKG_GELISTET += start - budget;
    if (r > 0 && k->PKG_GEFUNDEN + r > PKG_DATEIEN_MAX) r = -1;   /* Codex-Prüfung 96: passt nicht mehr - ungeprüft wie bei einer Grenze */
    if (r <= 0) { arena_frei(t); return r < 0 ? -1 : 0; }
    k->PKG_GEFUNDEN += r;
    /* nur die Ergebnisse in den Speicher des Kerns, dann den eigenen Speicher frei geben (auch bevor etwas abbrechen kann) */
    sliste rk = {0}, pkk = {0}; long long *grk = a_hole(A, sizeof(long long) * (size_t)pk.n);
    for (int j = 0; j < rest.n; j++) sl_add(A, &rk, a_kopie(A, rest.e[j]));
    for (int j = 0; j < pk.n; j++) { sl_add(A, &pkk, a_kopie(A, pk.e[j])); grk[j] = atoll(pg.e[j]); }
    /* Codex-Prüfung 94: der aufgeteilte Ordner und seine aufgeteilten Unterordner kommen zu AUFGEKLAPPT (äußere zuerst - der Nachlauf geht
     * von hinten): sind sie nach dem Löschen leer, werden sie mit entfernt (wie bei "alles außer PS5 Cleanup" und Ordnern mit Spielen) */
    sl_add(A, &k->AUFGEKLAPPT, p);
    for (int j = auf.n - 1; j >= 0; j--) sl_add(A, &k->AUFGEKLAPPT, a_kopie(A, auf.e[j]));
    arena_frei(t);
    if (rk.n > 0) {
        MERKE(k, &rk, kat, VERB(A, anzeige, T("  (ohne PKG-Dateien)", "  (without PKG files)")), 0);
        /* Codex-Prüfung 83 (P1): der Rest eines Tool-Ordners (/data/garlic mit einer PKG-Datei und backups/) bleibt, was der Ordner war -
         * Name und Art kommen weiter vom Tool-Ordner (wie bei folder-source), sonst wären z. B. Spielstand-Sicherungen nicht mehr rot */
        /* Codex-Prüfung 98 (P1): ein aufgeteilter Unterordner in "alles außer PS5 Cleanup" gehört ebenso zu seinem Tool-Ordner (werkzeug) */
        const char *to = werkzeug ? werkzeug : h_m(p, "^/user/data/(homebrew/)?[^/]+$") ? p : NULL;
        if (k->MERKE_OK && k->bau.n > 0 && to) { eintrag *f = k->bau.e[k->bau.n - 1]; if (!f->ToolOrdner) f->ToolOrdner = to; }
    }
    for (int j = 0; j < pkk.n; j++) merke_pkg(k, pkk.e[j], VERB(A, anzeige, pkk.e[j] + strlen(p)), grk[j]);
    return 1;
}
/* Gruppen gleicher Schlüssel ohne Hashtabelle: Indizes nach Schlüssel sortieren (bei 2000 PKG-Dateien sonst quadratisch) */
static const char **pkg_sort_sch;
static int pkg_sort_vgl(const void *x, const void *y)
{
    int a = *(const int *)x, b = *(const int *)y, c = strcmp(pkg_sort_sch[a], pkg_sort_sch[b]);
    return c ? c : (a < b ? -1 : a > b);
}
static int *pkg_sortiert(kern *k, const char **sch, int n, int *m)
{
    int *ix = a_hole(A, sizeof(int) * (size_t)(n > 0 ? n : 1)); *m = 0;
    for (int i = 0; i < n; i++) if (sch[i]) ix[(*m)++] = i;
    pkg_sort_sch = sch; qsort(ix, (size_t)*m, sizeof(int), pkg_sort_vgl); pkg_sort_sch = NULL;
    return ix;
}
/* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 12, da_Chris86: "was machen wir, wenn ein User das alles auf den USB-Stick, auf die Interne
 * oder auf den M.2-Speicher ballert?"): bis 5 PKG-Dateien gleicher Art eines Spiels auf einem Speicher stehen einzeln da, ab 6 als EINE
 * Zeile ("Zusatzinhalte (120 PKG-Dateien)"). Gelöscht wird dann jede Datei dieser Zeile. Gesperrtes bleibt einzeln. */
#define PKG_EINZELN_MAX 5
static const char *pkg_art_mehrzahl(int art)
{
    return art == PKG_UPDATE ? T("Updates", "Updates") : art == PKG_DLC ? T("Zusatzinhalte", "Add-ons") : T("Installationsdateien", "Install files");
}
/* Codex-Prüfung 83 (P2): der Speicher selbst, nicht nur seine Art - zwei USB-Laufwerke (/mnt/usb0, /mnt/usb1) sind zwei Speicher */
static const char *pkg_speicher(kern *k, const char *p)
{
    htreffer m = h_mt(A, p, "^(/mnt/[^/]+)/");
    return m.ok ? m.gr[1] : "intern";
}
static void pkg_zusammenfassen(kern *k)
{
    int n = k->bau.n, m;
    if (n == 0) return;
    const char **sch = a_hole(A, sizeof(char *) * (size_t)n);
    for (int i = 0; i < n; i++) {
        eintrag *f = k->bau.e[i]; sch[i] = NULL;
        if (f->PkgArt && f->PkgId && !f->Gesperrt && f->PfadeBasis.n == 1)
            sch[i] = a_fmt(A, "%s|%d|%s|%d", f->PkgId, f->PkgArt, pkg_speicher(k, f->PfadeBasis.e[0]), f->NurEinzeln ? 1 : 0);
    }
    int *ix = pkg_sortiert(k, sch, n, &m);
    eintrag **zu = a_hole(A, sizeof(eintrag *) * (size_t)n);   /* Zeile, in der ein Eintrag aufgeht (NULL = bleibt) */
    bool *neu_z = a_hole(A, sizeof(bool) * (size_t)n);
    for (int i = 0; i < n; i++) { zu[i] = NULL; neu_z[i] = false; }
    for (int a = 0; a < m; ) {
        int b = a; while (b < m && !strcmp(sch[ix[a]], sch[ix[b]])) b++;
        if (b - a > PKG_EINZELN_MAX) {
            eintrag *f = k->bau.e[ix[a]], *z = a_hole(A, sizeof *z);
            *z = *f; z->Pfade = (sliste){0}; z->PfadeBasis = (sliste){0}; z->PkgAnzahl = 0; z->PkgBytes = 0; z->Doppelt = (pliste){0};
            z->PkgGroessen = a_hole(A, sizeof(long long) * (size_t)(b - a));
            for (int j = a; j < b; j++) {
                eintrag *g = k->bau.e[ix[j]];
                z->PkgGroessen[z->PfadeBasis.n] = g->PkgBytes;   /* je eine Datei (PfadeBasis.n == 1, siehe oben) */
                sl_add_alle(A, &z->Pfade, &g->Pfade); sl_add_alle(A, &z->PfadeBasis, &g->PfadeBasis); z->PkgAnzahl++;
                if ((!z->PkgTitel || !z->PkgTitel[0]) && g->PkgTitel && g->PkgTitel[0]) z->PkgTitel = g->PkgTitel;
                zu[ix[j]] = z;
            }
            z->Text = z->TextBasis = VERB(A, kern_ort_vorsatz(z->Ort), z->PkgTitel && z->PkgTitel[0] ? z->PkgTitel : z->PkgId, " (", z->PkgId, ") – ",
                                          pkg_art_mehrzahl(z->PkgArt), a_fmt(A, T(" (%d PKG-Dateien)", " (%d PKG files)"), z->PkgAnzahl));
            neu_z[ix[a]] = true;
        }
        a = b;
    }
    pliste neu = {0};
    for (int i = 0; i < n; i++) {
        if (!zu[i]) pl_add(A, &neu, k->bau.e[i]);
        else if (neu_z[i]) pl_add(A, &neu, zu[i]);
    }
    k->bau = neu;
}
/* NEU 1.0.8 (Befund 12): dieselbe PKG-Datei (gleicher Name, gleiche Größe) auf zwei Speichern - beide Zeilen sagen "doppelt" und wo die
 * andere liegt (kern_doppelt_text). Nur Anzeige. */
static void pkg_doppelt(kern *k)
{
    /* Codex-Prüfung 83 (P3): je DATEI vergleichen, auch die Dateien einer zusammengefassten Zeile (PkgGroessen) - sonst bekam eine einzelne
     * Kopie auf der M.2 keinen Hinweis, wenn ihr Gegenstück in einer Zeile "Zusatzinhalte (8 PKG-Dateien)" steckte */
    /* Codex-Prüfung 85 (P2): eine zusammengefasste Zeile heißt nur "doppelt", wenn JEDE ihrer Dateien auch woanders liegt - sonst wären
     * beim Löschen der Zeile Dateien ohne Kopie weg, obwohl "doppelt" dasteht. Die einzelne Kopie woanders sagt es trotzdem. */
    int ne = k->bau.n, nz = 0;
    for (int i = 0; i < ne; i++) { eintrag *f = k->bau.e[i]; if (f->PkgArt && (f->PkgGroessen || f->PfadeBasis.n == 1)) nz += f->PfadeBasis.n; }
    if (nz == 0) return;
    const char **sch = a_hole(A, sizeof(char *) * (size_t)nz);
    eintrag **wer = a_hole(A, sizeof(eintrag *) * (size_t)nz);
    const char **sp = a_hole(A, sizeof(char *) * (size_t)nz);
    int *wi = a_hole(A, sizeof(int) * (size_t)nz), *nd = a_hole(A, sizeof(int) * (size_t)ne);
    bool *dup = a_hole(A, sizeof(bool) * (size_t)nz);
    int n = 0, m;
    for (int i = 0; i < ne; i++) {
        eintrag *f = k->bau.e[i];
        if (!f->PkgArt || !(f->PkgGroessen || f->PfadeBasis.n == 1)) continue;
        for (int j = 0; j < f->PfadeBasis.n; j++) {
            long long gr = f->PkgGroessen ? f->PkgGroessen[j] : f->PkgBytes;
            sch[n] = NULL; wer[n] = f; wi[n] = i; sp[n] = pkg_speicher(k, f->PfadeBasis.e[j]);
            if (gr > 0) {
                char *b = a_kopie(A, h_blatt(A, f->PfadeBasis.e[j]));
                for (char *c = b; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
                sch[n] = a_fmt(A, "%s|%lld", b, gr);
            }
            n++;
        }
    }
    int *ix = pkg_sortiert(k, sch, n, &m);
    for (int a = 0; a < m; ) {   /* 1. welche Dateien liegen auch auf einem anderen Speicher? */
        int b = a; while (b < m && !strcmp(sch[ix[a]], sch[ix[b]])) b++;
        for (int i = a; i < b; i++) for (int j = a; j < b; j++)
            if (wer[ix[i]] != wer[ix[j]] && strcmp(sp[ix[i]], sp[ix[j]]) != 0) dup[ix[i]] = true;
        a = b;
    }
    for (int x = 0; x < n; x++) if (dup[x]) nd[wi[x]]++;
    for (int a = 0; a < m; ) {   /* 2. Hinweis - eine zusammengefasste Zeile nur, wenn alle ihre Dateien doppelt sind */
        int b = a; while (b < m && !strcmp(sch[ix[a]], sch[ix[b]])) b++;
        for (int i = a; i < b; i++) for (int j = a; j < b; j++) {
            eintrag *f = wer[ix[i]], *g = wer[ix[j]];
            if (f == g || !strcmp(sp[ix[i]], sp[ix[j]])) continue;
            if (f->PkgGroessen && nd[wi[ix[i]]] < f->PfadeBasis.n) continue;
            bool schon = false; for (int x = 0; x < f->Doppelt.n && !schon; x++) if (f->Doppelt.e[x] == g) schon = true;
            if (!schon) pl_add(A, &f->Doppelt, g);
        }
        a = b;
    }
}
/* NEU 1.0.8 (Arbeitsplan 2.1): Spielname aus einem exFAT-Spiel-Abbild (z. B. PPSA03530.exfat), wenn der Dateiname nur die
 * Spielnummer hat. Gelesen werden nur kleine Stücke an festen Stellen: der Kopf (512 Byte), die nötigen Verzeichnisse (je höchstens
 * 256 KB) und param.json bzw. param.sfo (höchstens 64 KB), nie das ganze Abbild. sce_sys wird im Hauptverzeichnis gesucht, sonst in
 * einem einzigen Unterordner. Jeder Wert aus der Datei wird geprüft; passt etwas nicht (kein exFAT, kaputt, nicht lesbar), gibt es
 * keinen Namen und es bleibt beim Dateinamen wie bisher. UFS2-Abbilder (.ffpkg, .ufs) liest seit 1.0.9 ufs_innen (unten), PFS-Abbilder
 * (.ffpfs, .ffpfsc, .pfs) pfs_innen (unten). */
#define EXF_VERZ_MAX (256 * 1024)
#define EXF_DATEI_MAX (64 * 1024)
#define ABBILD_NUR_NUMMER "(?i)^[a-z]{4}\\d{5}([-_ .]*(app|v?\\d[\\d._-]*))?\\.(" ABBILD_ENDUNGEN ")$"
typedef struct { kern *k; const char *p; arena *t; unsigned long long fat, heap, cl, groesse; uint32_t anz; size_t gelesen;
                 unsigned long long basis; kern_pfsc *pfsc; } exf;   /* 1.0.9: Abbild im PFS-Abbild - ab Stelle basis oder gepackt (pfsc) */
typedef struct { bool ordner, am_stueck; uint32_t c; unsigned long long laenge; char name[64]; } exf_eintrag;
static unsigned long long exf_le64(const unsigned char *b) { return (unsigned long long)pkg_le32(b) | (unsigned long long)pkg_le32(b + 4) << 32; }
static bool exf_gleich(const char *a, const char *b)   /* ASCII, ohne Groß/Klein */
{
    for (; *a && *b; a++, b++) {
        char x = *a >= 'A' && *a <= 'Z' ? (char)(*a + 32) : *a, y = *b >= 'A' && *b <= 'Z' ? (char)(*b + 32) : *b;
        if (x != y) return false;
    }
    return *a == *b;
}
static bool exf_lies(exf *x, unsigned long long ab, size_t n, unsigned char *ziel)
{
    size_t len = 0;
    if (x->groesse && (ab > x->groesse || n > x->groesse - ab)) return false;
    if (x->pfsc) return kern_pfsc_lies(x->pfsc, ab, n, ziel);   /* 1.0.9: gepackt - gezählt wird, was wirklich gelesen wird (pfs_lies) */
    if (ab > (unsigned long long)INT64_MAX - n || x->basis > (unsigned long long)INT64_MAX - n - ab) return false;
    if (!s_lies_ab(x->k->S, x->p, (long long)(x->basis + ab), n, ziel, &len) || len != n) return false;
    x->gelesen += n;
    return true;
}
/* NEU 1.0.9: wo das Bild liegt, für die Oberfläche (BILD_ORT "Pfad\nAnfang\nLänge"); in einem gepackten Abbild (.ffpfsc) mit der vierten
 * Zeile "pfsc <Anfang> <Länge>" des Behälters - Anfang ist dann die Stelle im entpackten Abbild. "" = kein Bild. In Stücken: bild_ort_teile. */
static const char *bild_ort(exf *x, unsigned long long ab, unsigned long long len)
{
    kern *k = x->k;   /* für A */
    if (strchr(x->p, '\n')) return "";
    if (x->pfsc) return a_fmt(A, "%s\n%llu\n%llu\npfsc %llu %llu", x->p, ab, len, x->pfsc->ab, x->pfsc->gr);
    return a_fmt(A, "%s\n%llu\n%llu", x->p, x->basis + ab, len);
}
/* NEU 1.0.9 (#149): ein Bild in n Stücken (t[i] = Anfang und Länge im Abbild, n = 1 bis KERN_BILD_TEILE, zusammen len): bei einem Stück wie
 * bild_ort, sonst dazu die Zeile "teile n a1 l1 ..." (Anfang = das erste Stück). Gelesen wird es dann über kern_bild_lesen. */
static const char *bild_ort_teile(exf *x, unsigned long long (*t)[2], int n, unsigned long long len)
{
    kern *k = x->k;   /* für A */
    if (n < 1 || n > KERN_BILD_TEILE) return "";
    const char *o = bild_ort(x, t[0][0], len);
    if (n == 1 || !o[0]) return o;
    unsigned long long b = x->pfsc ? 0 : x->basis;
    const char *s = a_fmt(A, "%s\nteile %d", o, n);
    for (int i = 0; i < n; i++) s = a_fmt(A, "%s %llu %llu", s, b + t[i][0], t[i][1]);
    return s;
}
/* NEU 1.0.9 (Codex-Prüfung 130): BILD_ORT zerlegen - vorher in app.c spiel_bild, jetzt hier (und im Test geprüft). Zahlen nur aus Ziffern
 * (nicht leer, ohne Vorzeichen und Leerzeichen, ohne Überlauf), Pfad 1 bis 999 Zeichen; danach nur genau "pfsc <Anfang> <Länge>" und/oder
 * (NEU #149) "teile <n> <Anfang> <Länge> ..." (in dieser Reihenfolge, n = 2 bis KERN_BILD_TEILE, das erste Stück bei Anfang, keines leer,
 * zusammen Länge), das Bild dann 8 Byte bis 4 MB. false = nicht genau in dieser Form. */
static bool bild_zahl(const char *s, const char *e, unsigned long long *v)
{
    *v = 0;
    if (s >= e) return false;
    for (; s < e; s++) {
        if (*s < '0' || *s > '9' || *v > (~0ULL - (unsigned)(*s - '0')) / 10) return false;   /* bis 18446744073709551615 (Codex-Prüfung 131) */
        *v = *v * 10 + (unsigned)(*s - '0');
    }
    return true;
}
/* die Zahlen einer Zeile von s bis e, durch genau ein Leerzeichen getrennt (nicht davor, nicht dahinter); Anzahl, -1 = nicht so */
static int bild_zahlen(const char *s, const char *e, unsigned long long *v, int max)
{
    int n = 0;
    while (s < e) {
        const char *lz = memchr(s, ' ', (size_t)(e - s)), *z = lz ? lz : e;
        if (n >= max || !bild_zahl(s, z, &v[n])) return -1;
        n++;
        if (!lz) break;
        s = lz + 1;
        if (s == e) return -1;
    }
    return n;
}
bool kern_bild_ort_lesen(const char *o, kern_bild_ort *b)
{
    memset(b, 0, sizeof *b);
    const char *z1 = o ? strchr(o, '\n') : NULL, *z2 = z1 ? strchr(z1 + 1, '\n') : NULL, *z3 = z2 ? strchr(z2 + 1, '\n') : NULL;
    if (!z1 || !z2 || z1 == o || (size_t)(z1 - o) >= sizeof b->pfad) return false;
    if (!bild_zahl(z1 + 1, z2, &b->ab) || !bild_zahl(z2 + 1, z3 ? z3 : z2 + 1 + strlen(z2 + 1), &b->len)) return false;
    memcpy(b->pfad, o, (size_t)(z1 - o)); b->pfad[z1 - o] = 0;
    const char *s = z3 ? z3 + 1 : NULL;
    if (s && !strncmp(s, "pfsc ", 5)) {                               /* gepackt: der Behälter in der Datei */
        const char *e = strchr(s, '\n');
        unsigned long long v[2];
        if (bild_zahlen(s + 5, e ? e : s + strlen(s), v, 2) != 2) return false;
        b->cab = v[0]; b->cgr = v[1]; b->gepackt = true;
        s = e ? e + 1 : NULL;
    }
    if (s && !strncmp(s, "teile ", 6) && !strchr(s, '\n')) {         /* NEU #149: in Stücken */
        unsigned long long v[1 + 2 * KERN_BILD_TEILE], summe = 0;
        int z = bild_zahlen(s + 6, s + strlen(s), v, 1 + 2 * KERN_BILD_TEILE);
        if (z < 5 || v[0] < 2 || v[0] > KERN_BILD_TEILE || (unsigned long long)z != 1 + 2 * v[0] || v[1] != b->ab) return false;
        for (int i = 0; i < (int)v[0]; i++) {
            unsigned long long a = v[1 + 2 * i], l = v[2 + 2 * i];
            if (l == 0 || l > b->len - summe || a > ~0ULL - l) return false;
            summe += l; b->teile[i][0] = a; b->teile[i][1] = l;
        }
        if (summe != b->len) return false;
        b->teile_n = (int)v[0];
        s = NULL;
    }
    if (s) return false;                                               /* sonst nichts */
    if ((b->gepackt || b->teile_n) && (b->len < 8 || b->len > 4u * 1024 * 1024)) return false;
    return true;
}
static uint32_t exf_fat(exf *x, uint32_t c)   /* nächster Cluster der Kette; 0 = Ende oder ungültig */
{
    unsigned char b[4];
    if (!exf_lies(x, x->fat + (unsigned long long)c * 4, 4, b)) return 0;
    uint32_t n = pkg_le32(b);
    return n >= 2 && n - 2 < x->anz ? n : 0;
}
/* Daten ab Cluster c lesen (Kette über die FAT oder am Stück), höchstens max Byte, mit 0 am Ende; NULL = ein Stück nicht lesbar */
static unsigned char *exf_daten(exf *x, uint32_t c, unsigned long long laenge, bool am_stueck, size_t max, size_t *n)
{
    *n = 0;
    if (laenge > max) laenge = max;
    unsigned char *b = a_hole(x->t, (size_t)laenge + 1);
    size_t got = 0;
    while (got < laenge) {
        if (c < 2 || c - 2 >= x->anz) return NULL;
        size_t m = laenge - got < x->cl ? (size_t)(laenge - got) : (size_t)x->cl;
        if (!exf_lies(x, x->heap + (unsigned long long)(c - 2) * x->cl, m, b + got)) return NULL;
        got += m;
        if (got < laenge && !(c = am_stueck ? c + 1 : exf_fat(x, c))) break;
    }
    *n = got;
    return b;
}
/* nächsten Datei- oder Ordner-Eintrag ab Stelle i lesen; liefert die Stelle danach, 0 = keiner mehr */
static size_t exf_naechster(const unsigned char *v, size_t n, size_t i, exf_eintrag *e)
{
    while (i + 32 <= n) {
        if (v[i] == 0x00) return 0;                           /* Ende des Verzeichnisses */
        unsigned sek = v[i + 1];
        size_t weiter = i + 32 * ((size_t)sek + 1);
        if (v[i] != 0x85 || sek < 2 || sek > 18 || weiter > n || v[i + 32] != 0xC0) { i += 32; continue; }
        const unsigned char *st = v + i + 32;
        unsigned nl = st[3], z = 0; size_t o = 0; bool ascii = true;
        for (unsigned j = 2; j <= sek && z < nl; j++) {
            const unsigned char *f = v + i + 32 * j;
            if (f[0] != 0xC1) break;
            for (int q = 0; q < 15 && z < nl; q++, z++) {
                unsigned u = f[2 + 2 * q] | (unsigned)f[3 + 2 * q] << 8;
                if (u == 0 || u >= 0x80) ascii = false;
                else if (o < sizeof e->name - 1) e->name[o++] = (char)u;
            }
        }
        e->name[o] = 0;
        if (z != nl || !ascii || o != nl) e->name[0] = 0;     /* unvollständig, nicht ASCII oder zu lang: passt zu keinem gesuchten Namen */
        e->ordner = (v[i + 4] & 0x10) != 0;
        e->am_stueck = (st[1] & 0x02) != 0;
        e->c = pkg_le32(st + 20);
        e->laenge = exf_le64(st + 24);
        return weiter;
    }
    return 0;
}
/* NEU 1.0.8 (PS5-Test 07.11.2026 16:31): dazu die Kennung aus param.json/param.sfo (*tid) und, wenn icon0.png lesbar liegt (seit #149 auch
 * in Stücken), wo
 * (*bild = "Pfad\nAnfang\nLänge" für die Oberfläche - sie liest das Bild selbst, die Suche nicht) */
static const char *exf_innen(exf *x, const char **tid, const char **bild)
{
    kern *k = x->k;   /* für A */
    *tid = ""; *bild = "";
    unsigned char kopf[512];
    if (!exf_lies(x, 0, sizeof kopf, kopf) || memcmp(kopf + 3, "EXFAT   ", 8) != 0 || kopf[510] != 0x55 || kopf[511] != 0xAA) return "";
    unsigned bps = kopf[108], spc = kopf[109];
    if (bps < 9 || bps > 12 || spc > 25 - bps) return "";
    unsigned long long sek = 1ULL << bps;
    x->cl = 1ULL << (bps + spc);
    x->fat = pkg_le32(kopf + 80) * sek;
    x->heap = pkg_le32(kopf + 88) * sek;
    x->anz = pkg_le32(kopf + 92);
    uint32_t wurzel = pkg_le32(kopf + 96);
    if (x->fat < 24 * sek || x->heap <= x->fat || x->anz == 0 || x->anz > 0xFFFFFFF5u || wurzel < 2 || wurzel - 2 >= x->anz) return "";
    size_t n; exf_eintrag e = {0}, sys = {0}, einzig = {0}; int ordner = 0; bool sys_da = false;
    unsigned char *v = exf_daten(x, wurzel, EXF_VERZ_MAX, false, EXF_VERZ_MAX, &n);
    if (!v) return "";
    for (size_t i = 0; (i = exf_naechster(v, n, i, &e)) != 0; ) {
        if (!e.ordner) continue;
        ordner++; einzig = e;
        if (exf_gleich(e.name, "sce_sys")) { sys = e; sys_da = true; }
    }
    if (!sys_da && ordner == 1) {                             /* Spiel in einem einzigen Unterordner, z. B. PPSA03530-app/sce_sys */
        unsigned char *u = exf_daten(x, einzig.c, einzig.laenge, einzig.am_stueck, EXF_VERZ_MAX, &n);
        if (!u) return "";
        for (size_t i = 0; (i = exf_naechster(u, n, i, &e)) != 0; ) if (e.ordner && exf_gleich(e.name, "sce_sys")) { sys = e; sys_da = true; break; }
    }
    if (!sys_da) return "";
    unsigned char *s = exf_daten(x, sys.c, sys.laenge, sys.am_stueck, EXF_VERZ_MAX, &n);
    if (!s) return "";
    exf_eintrag pj = {0}, ps = {0}, ic = {0}; bool pj_da = false, ps_da = false, ic_da = false;
    for (size_t i = 0; (i = exf_naechster(s, n, i, &e)) != 0; ) {
        if (e.ordner) continue;
        if (exf_gleich(e.name, "param.json")) { pj = e; pj_da = true; }
        else if (exf_gleich(e.name, "param.sfo")) { ps = e; ps_da = true; }
        else if (exf_gleich(e.name, "icon0.png")) { ic = e; ic_da = true; }
    }
    const char *titel = "";
    if (pj_da && pj.laenge > 0 && pj.laenge <= EXF_DATEI_MAX) {
        unsigned char *j = exf_daten(x, pj.c, pj.laenge, pj.am_stueck, EXF_DATEI_MAX, &n);
        if (j && n == pj.laenge && !memchr(j, 0, n)) {
            titel = kern_json_titel(A, (const char *)j);
            htreffer m = h_mt(x->t, (const char *)j, "(?-i)\"titleId\"\\s*:\\s*\"([A-Z]{4}\\d{5})\"");
            if (titel[0] && m.ok) *tid = a_kopie(A, m.gr[1]);
        }
    }
    if (!titel[0] && ps_da && ps.laenge >= 20 && ps.laenge <= EXF_DATEI_MAX) {   /* PS4-Spiel im Abbild: Titel wie kern_sfo_titel */
        unsigned char *f = exf_daten(x, ps.c, ps.laenge, ps.am_stueck, EXF_DATEI_MAX, &n);
        char t[128], ti[16];
        if (f && n == ps.laenge && ((pkg_sfo_feld(f, n, "TITLE_04", t, sizeof t) && t[0]) || (pkg_sfo_feld(f, n, "TITLE_01", t, sizeof t) && t[0])
                                    || (pkg_sfo_feld(f, n, "TITLE", t, sizeof t) && t[0]))) {
            titel = a_kopie(A, t);
            if (pkg_sfo_feld(f, n, "TITLE_ID", ti, sizeof ti) && r_m(ti, "^[A-Z]{4}\\d{5}$")) *tid = a_kopie(A, ti);
        }
    }
    /* icon0.png am Stück (ohne FAT-Kette oder Cluster für Cluster hintereinander) oder - NEU 1.0.9 (#149) - in Stücken entlang der FAT-Kette
     * (höchstens 1024 Cluster, KERN_BILD_TEILE Stücke); lesen tut es die Oberfläche */
    if (titel[0] && ic_da && ic.laenge >= 8 && ic.laenge <= 4u * 1024 * 1024 && ic.c >= 2 && ic.c - 2 < x->anz) {
        unsigned long long cls = (ic.laenge + x->cl - 1) / x->cl;
        bool stueck = cls <= (unsigned long long)(x->anz - (ic.c - 2));
        if (stueck && !ic.am_stueck && cls > 1) {         /* die FAT-Einträge der Cluster liegen hintereinander: ein Stück lesen (höchstens 32 KB) */
            size_t fl = (size_t)(cls - 1) * 4;
            unsigned char *fb = a_hole(x->t, fl);
            if (!exf_lies(x, x->fat + (unsigned long long)ic.c * 4, fl, fb)) stueck = false;
            for (unsigned long long i = 0; stueck && i + 1 < cls; i++) stueck = pkg_le32(fb + 4 * i) == ic.c + i + 1;
        }
        unsigned long long ab = x->heap + (unsigned long long)(ic.c - 2) * x->cl;
        if (stueck && (!x->groesse || (ab <= x->groesse && ic.laenge <= x->groesse - ab)))
            *bild = bild_ort(x, ab, ic.laenge);
        else if (!stueck && !ic.am_stueck && cls <= 1024) {   /* NEU 1.0.9 (#149): der FAT-Kette folgen, Stücke merken */
            unsigned long long t[KERN_BILD_TEILE][2], rest = ic.laenge; int n = 0; uint32_t c = ic.c; bool ok = true;
            for (unsigned long long i = 0; ok && i < cls; i++) {
                unsigned long long a = x->heap + (unsigned long long)(c - 2) * x->cl, l = rest < x->cl ? rest : x->cl;
                for (int j = 0; j < n; j++) if (a < t[j][0] + t[j][1] && t[j][0] < a + l) ok = false;   /* Codex-Prüfungen 134, 135: kein Cluster zweimal (Kreis), nichts überschneidet sich */
                if (!ok || (x->groesse && (a > x->groesse || l > x->groesse - a))) ok = false;
                else if (n > 0 && t[n - 1][0] + t[n - 1][1] == a) t[n - 1][1] += l;
                else if (n < KERN_BILD_TEILE) { t[n][0] = a; t[n][1] = l; n++; }
                else ok = false;
                rest -= l;
                if (ok && i + 1 < cls && !(c = exf_fat(x, c))) ok = false;
            }
            unsigned char sig[8];
            if (ok && n >= 2 && exf_lies(x, t[0][0], sizeof sig, sig) && !memcmp(sig, "\x89PNG\r\n\x1a\n", 8)) *bild = bild_ort_teile(x, t, n, ic.laenge);
        }
    }
    return titel;
}
/* NEU 1.0.9 (08.11.2026, da_Chris86 01:19 "außer Exfat auch noch andere Dateiformate", Nachprüfung 07:45: ShadowMountPlus empfiehlt
 * .ffpkg): Name, Kennung und Bild auch aus UFS2-Abbildern - .ffpkg, .ufs (kstuff-lite) und UFS2Tool-.img. Aufbau nach FreeBSD
 * sys/ufs/ffs/fs.h, sys/ufs/ufs/dinode.h und dir.h: Superblock bei 64 KB (Kennung 0x19540119 an Stelle 1372, Werte little-endian),
 * Inodes zu 256 Byte, Verzeichnisse in Stücken zu 512 Byte (ein Eintrag überspannt nie ein Stück). ShadowMountPlus legt .ffpkg mit
 * newfs -O 2 -b 65536 -f 65536 an (mkufs2.sh), sce_sys liegt im Hauptverzeichnis. Gelesen werden nur der Superblock, wenige Inodes,
 * die nötigen Verzeichnisse (je höchstens 256 KB) und param.json bzw. param.sfo (höchstens 64 KB), nie das ganze Abbild. Jeder Wert
 * aus der Datei wird geprüft (Größen, Grenzen, Inode- und Blocknummern innerhalb des Dateisystems); passt etwas nicht, gibt es keinen
 * Namen und es bleibt beim Dateinamen. icon0.png nur, wenn es mit der PNG-Kennung beginnt - am Stück oder (#149) in Stücken; dann liest
 * die Oberfläche das Bild selbst (BILD_ORT), die Suche nicht. */
#define UFS2_SB_AB   65536ULL
#define UFS2_KENNUNG 0x19540119u
#define UFS2_WURZEL  2ULL
#define UFS_NICHT    (~0ULL)   /* Blocknummer nicht lesbar oder außerhalb */
typedef struct { unsigned long long fsize, bsize, frag, inopb, ipg, fpg, iblkno, ncg, frags; } ufs_sb;
typedef struct { unsigned art; unsigned long long laenge, db[12], ib0; } ufs_inode;   /* art: 4 Ordner, 8 Datei, 0 sonst */
typedef struct { unsigned long long ino; unsigned typ; char name[64]; } ufs_eintrag;
static bool ufs_zweierpotenz(unsigned long long v) { return v && !(v & (v - 1)); }
static bool ufs_sb_lesen(exf *x, ufs_sb *s)
{
    unsigned char b[1376];
    if (!exf_lies(x, UFS2_SB_AB, sizeof b, b) || pkg_le32(b + 1372) != UFS2_KENNUNG) return false;
    s->iblkno = pkg_le32(b + 16); s->ncg = pkg_le32(b + 44); s->bsize = pkg_le32(b + 48); s->fsize = pkg_le32(b + 52);
    s->frag = pkg_le32(b + 56); s->inopb = pkg_le32(b + 120); s->ipg = pkg_le32(b + 184); s->fpg = pkg_le32(b + 188);
    s->frags = exf_le64(b + 1080);   /* fs_size: Fragmente im Dateisystem */
    if (!ufs_zweierpotenz(s->fsize) || s->fsize < 512 || !ufs_zweierpotenz(s->bsize) || s->bsize < 4096 || s->bsize > 65536
        || s->fsize > s->bsize || s->frag != s->bsize / s->fsize || s->frag > 8 || s->inopb != s->bsize / 256) return false;
    if (s->ipg == 0 || s->ipg % s->inopb || s->ncg == 0 || s->fpg == 0 || s->fpg % s->frag || s->iblkno >= s->fpg) return false;
    return s->frags > 0 && s->frags <= (1ULL << 40) && s->frags <= s->ncg * s->fpg;
}
static bool ufs_inode_lesen(exf *x, const ufs_sb *s, unsigned long long ino, ufs_inode *i)
{
    if (ino < UFS2_WURZEL || ino / s->ipg >= s->ncg) return false;
    unsigned long long cg = ino / s->ipg, rest = ino % s->ipg;
    unsigned long long fsba = s->fpg * cg + s->iblkno + (rest / s->inopb) * s->frag;   /* ino_to_fsba (UFS2: cgstart = cgbase) */
    if (fsba >= s->frags || s->frags - fsba < s->frag) return false;
    unsigned char b[256];
    if (!exf_lies(x, fsba * s->fsize + (ino % s->inopb) * 256, sizeof b, b)) return false;
    unsigned mode = (unsigned)b[0] | (unsigned)b[1] << 8;
    i->art = (mode & 0170000) == 0040000 ? 4 : (mode & 0170000) == 0100000 ? 8 : 0;
    i->laenge = exf_le64(b + 16);
    for (int j = 0; j < 12; j++) i->db[j] = exf_le64(b + 112 + 8 * j);
    i->ib0 = exf_le64(b + 208);
    return i->art != 0;
}
/* Fragment-Nummer von Block lbn der Datei (0 = Loch, UFS_NICHT = nicht lesbar) - nur die direkten Blöcke und die erste Stufe (ib[0]) */
static unsigned long long ufs_block(exf *x, const ufs_sb *s, const ufs_inode *i, unsigned long long lbn, unsigned char **ind)
{
    unsigned long long a, n = s->bsize / 8;
    if (lbn < 12) a = i->db[lbn];
    else if (lbn - 12 >= n) return UFS_NICHT;
    else {
        if (i->ib0 == 0) return 0;
        if (!*ind) {
            if (i->ib0 >= s->frags || s->frags - i->ib0 < s->frag) return UFS_NICHT;
            unsigned char *b = a_hole(x->t, (size_t)s->bsize);
            if (!exf_lies(x, i->ib0 * s->fsize, (size_t)s->bsize, b)) return UFS_NICHT;
            *ind = b;
        }
        a = exf_le64(*ind + 8 * (lbn - 12));
    }
    return a != 0 && a >= s->frags ? UFS_NICHT : a;
}
/* höchstens max Byte der Datei, mit 0 am Ende; NULL = ein Stück nicht lesbar */
static unsigned char *ufs_daten(exf *x, const ufs_sb *s, const ufs_inode *i, size_t max, size_t *n)
{
    *n = 0;
    unsigned long long laenge = i->laenge < max ? i->laenge : max;
    unsigned char *b = a_hole(x->t, (size_t)laenge + 1), *ind = NULL;
    size_t got = 0;
    for (unsigned long long lbn = 0; got < laenge; lbn++) {
        size_t m = laenge - got < s->bsize ? (size_t)(laenge - got) : (size_t)s->bsize;
        unsigned long long a = ufs_block(x, s, i, lbn, &ind);
        if (a == UFS_NICHT) return NULL;
        if (a == 0) memset(b + got, 0, m);                     /* Loch: liest sich als Nullen */
        else if ((m + s->fsize - 1) / s->fsize > s->frags - a || !exf_lies(x, a * s->fsize, m, b + got)) return NULL;
        got += m;
    }
    b[got] = 0; *n = got;
    return b;
}
/* nächsten Eintrag ab Stelle i lesen; liefert die Stelle danach, 0 = keiner mehr. Kaputter Eintrag: Rest seines 512-Byte-Stücks überspringen */
static size_t ufs_naechster(const unsigned char *v, size_t n, size_t i, ufs_eintrag *e)
{
    while (i + 8 <= n) {
        size_t ende = (i / 512 + 1) * 512;
        if (ende > n) ende = n;
        unsigned long long ino = pkg_le32(v + i);
        size_t rl = (size_t)v[i + 4] | (size_t)v[i + 5] << 8, nl = v[i + 7];
        if (rl < 12 || rl % 4 || rl > ende - i || 8 + nl > rl) { i = ende; continue; }
        size_t weiter = i + rl;
        if (ino == 0 || nl == 0 || nl >= sizeof e->name || memchr(v + i + 8, 0, nl)) { i = weiter; continue; }
        memcpy(e->name, v + i + 8, nl); e->name[nl] = 0;
        e->ino = ino; e->typ = v[i + 6];
        return weiter;
    }
    return 0;
}
static bool ufs_ordner(exf *x, const ufs_sb *s, const ufs_eintrag *e, ufs_inode *d)   /* d_type 4 = Ordner, 0 = unbekannt: im Inode nachsehen */
{
    return (e->typ == 4 || e->typ == 0) && ufs_inode_lesen(x, s, e->ino, d) && d->art == 4;
}
static const char *ufs_innen(exf *x, const char **tid, const char **bild)
{
    kern *k = x->k;   /* für A */
    *tid = ""; *bild = "";
    ufs_sb s; ufs_inode w = {0}, d = {0}, sys = {0}, einzig = {0}; ufs_eintrag e = {0};
    if (!ufs_sb_lesen(x, &s) || !ufs_inode_lesen(x, &s, UFS2_WURZEL, &w) || w.art != 4) return "";
    size_t n;
    unsigned char *v = ufs_daten(x, &s, &w, EXF_VERZ_MAX, &n);
    if (!v) return "";
    int ordner = 0; bool sys_da = false;
    for (size_t i = 0; (i = ufs_naechster(v, n, i, &e)) != 0; ) {
        if (!strcmp(e.name, ".") || !strcmp(e.name, "..") || !strcmp(e.name, ".snap") || !strcmp(e.name, "lost+found")) continue;
        if (!ufs_ordner(x, &s, &e, &d)) continue;
        ordner++; einzig = d;
        if (exf_gleich(e.name, "sce_sys")) { sys = d; sys_da = true; }
    }
    if (!sys_da && ordner == 1) {                             /* Spiel in einem einzigen Unterordner, z. B. PPSA03530-app/sce_sys */
        unsigned char *u = ufs_daten(x, &s, &einzig, EXF_VERZ_MAX, &n);
        if (!u) return "";
        for (size_t i = 0; (i = ufs_naechster(u, n, i, &e)) != 0; )
            if (exf_gleich(e.name, "sce_sys") && ufs_ordner(x, &s, &e, &d)) { sys = d; sys_da = true; break; }
    }
    if (!sys_da) return "";
    unsigned char *sv = ufs_daten(x, &s, &sys, EXF_VERZ_MAX, &n);
    if (!sv) return "";
    ufs_inode pj = {0}, ps = {0}, ic = {0}; bool pj_da = false, ps_da = false, ic_da = false;
    for (size_t i = 0; (i = ufs_naechster(sv, n, i, &e)) != 0; ) {
        int was = exf_gleich(e.name, "param.json") ? 1 : exf_gleich(e.name, "param.sfo") ? 2 : exf_gleich(e.name, "icon0.png") ? 3 : 0;
        if (!was || (e.typ != 8 && e.typ != 0) || !ufs_inode_lesen(x, &s, e.ino, &d) || d.art != 8) continue;
        if (was == 1) { pj = d; pj_da = true; } else if (was == 2) { ps = d; ps_da = true; } else { ic = d; ic_da = true; }
    }
    const char *titel = "";                                   /* Name und Kennung wie exf_innen */
    if (pj_da && pj.laenge > 0 && pj.laenge <= EXF_DATEI_MAX) {
        unsigned char *j = ufs_daten(x, &s, &pj, EXF_DATEI_MAX, &n);
        if (j && n == pj.laenge && !memchr(j, 0, n)) {
            titel = kern_json_titel(A, (const char *)j);
            htreffer m = h_mt(x->t, (const char *)j, "(?-i)\"titleId\"\\s*:\\s*\"([A-Z]{4}\\d{5})\"");
            if (titel[0] && m.ok) *tid = a_kopie(A, m.gr[1]);
        }
    }
    if (!titel[0] && ps_da && ps.laenge >= 20 && ps.laenge <= EXF_DATEI_MAX) {   /* PS4-Spiel im Abbild: Titel wie kern_sfo_titel */
        unsigned char *f = ufs_daten(x, &s, &ps, EXF_DATEI_MAX, &n);
        char t[128], ti[16];
        if (f && n == ps.laenge && ((pkg_sfo_feld(f, n, "TITLE_04", t, sizeof t) && t[0]) || (pkg_sfo_feld(f, n, "TITLE_01", t, sizeof t) && t[0])
                                    || (pkg_sfo_feld(f, n, "TITLE", t, sizeof t) && t[0]))) {
            titel = a_kopie(A, t);
            if (pkg_sfo_feld(f, n, "TITLE_ID", ti, sizeof ti) && r_m(ti, "^[A-Z]{4}\\d{5}$")) *tid = a_kopie(A, ti);
        }
    }
    /* icon0.png, wenn es ein PNG ist und keine Löcher hat - am Stück oder (NEU 1.0.9, #149) in Stücken: Block für Block, direkt
     * hintereinander liegende Blöcke sind ein Stück (höchstens KERN_BILD_TEILE Stücke) */
    if (titel[0] && ic_da && ic.laenge >= 8 && ic.laenge <= 4u * 1024 * 1024 && !strchr(x->p, '\n')) {
        unsigned long long bl = (ic.laenge + s.bsize - 1) / s.bsize, t[KERN_BILD_TEILE][2], rest = ic.laenge;
        unsigned char *ind = NULL; bool ok = true; int n = 0;
        for (unsigned long long lbn = 0; ok && lbn < bl; lbn++) {
            unsigned long long a = ufs_block(x, &s, &ic, lbn, &ind), l = rest < s.bsize ? rest : s.bsize;
            if (a == 0 || a == UFS_NICHT || (l + s.fsize - 1) / s.fsize > s.frags - a) { ok = false; break; }
            unsigned long long ab = a * s.fsize;
            for (int j = 0; j < n; j++) if (ab < t[j][0] + t[j][1] && t[j][0] < ab + l) ok = false;   /* Codex-Prüfungen 134, 135: kein Block zweimal, keine Überschneidung */
            if (!ok || (x->groesse && (ab > x->groesse || l > x->groesse - ab))) ok = false;
            else if (n > 0 && t[n - 1][0] + t[n - 1][1] == ab) t[n - 1][1] += l;
            else if (n < KERN_BILD_TEILE) { t[n][0] = ab; t[n][1] = l; n++; }
            else ok = false;
            rest -= l;
        }
        unsigned char sig[8];
        if (ok && n >= 1 && exf_lies(x, t[0][0], sizeof sig, sig) && !memcmp(sig, "\x89PNG\r\n\x1a\n", 8))
            *bild = bild_ort_teile(x, t, n, ic.laenge);
    }
    return titel;
}
/* NEU 1.0.9 (08.11.2026, #144 - da_Chris86: "außer Exfat auch noch andere Dateiformate"; .ffpfs und .ffpfsc von ShadowMountPlus): Name,
 * Kennung und Bild auch aus PFS-Abbildern. Aufbau nach quer3q/ps5-dump-forge (crates/ps5-dump-forge-pfs, GPL-3.0, nur gelesen; Hinweis
 * da_Chris86 09:42) und an echten Dateien nachgeprüft (Orbit Store als .ffpfs und .ffpfsc, Pegasus DL+ als .ffpfsc): Kopf mit Kennung
 * 20130315 an Stelle 8, Art (0x1C), Blockgröße (0x20), Zahl der Inodes (0x30), der Blöcke (0x38) und der Inode-Blöcke (0x40); Inodes zu
 * 0xA8 Byte ab Block 1; Inode 0 ist die Über-Wurzel, darin "uroot" (das eigentliche Hauptverzeichnis). Verzeichniseinträge: Inode, Art
 * (2 Datei, 3 Ordner), Namenslänge, Eintragslänge (Vielfaches von 8), Name; 16 Null-Byte beenden die Liste. Gelesen wird nur das
 * übliche Abbild: ohne Signatur, 32-Bit-Inodes, unverschlüsselt, jede Datei am Stück (so legen es die Werkzeuge an) - sonst kein Name.
 * .ffpfsc: in uroot liegt genau eine Datei - gepackt (PFSC, kern_entpacken.c) oder nicht - und darin das eigentliche Abbild (exFAT, sonst
 * UFS2); gelesen werden nur die nötigen 64-KB-Blöcke, nie das ganze Abbild. Jeder Wert aus der Datei wird geprüft. */
#define PFS_KENNUNG 20130315ULL
#define PFS_INODE   0xA8u
typedef struct { unsigned long long bs, inodes, bloecke, iblocks; } pfs_kopf;
typedef struct { unsigned mode, flags; unsigned long long gr, gr_echt, ab; } pfs_inode;   /* gr: gespeichert, gr_echt: entpackt, ab: Daten */
typedef struct { unsigned long long ino; int typ; char name[64]; bool kaputt; } pfs_eintrag;   /* name "": zu lang oder mit 0 - zählt trotzdem
                                                   * mit; kaputt: die Liste endete an einem kaputten Eintrag (Codex-Prüfung 131) */
#define PFS_ORDNER(m) (((m) & 0xF000u) == 0x4000u)   /* Codex-Prüfung 130: die ganze Art vergleichen, nicht ein Bit (0xA000 Verknüpfung, */
#define PFS_DATEI(m)  (((m) & 0xF000u) == 0x8000u)   /* 0xC000 ...); in den echten Abbildern 0x416d und 0x816d */
static bool pfs_kopf_lesen(exf *x, pfs_kopf *h)
{
    unsigned char b[0x48];
    if (!exf_lies(x, 0, sizeof b, b)) return false;
    unsigned long long ver = exf_le64(b), kennung = exf_le64(b + 8);
    unsigned mode = (unsigned)b[0x1C] | (unsigned)b[0x1D] << 8;
    h->bs = pkg_le32(b + 0x20); h->inodes = exf_le64(b + 0x30); h->bloecke = exf_le64(b + 0x38); h->iblocks = exf_le64(b + 0x40);
    if (kennung != PFS_KENNUNG || ver < 1 || ver > 2 || (mode & 0x7)) return false;   /* 0x1 signiert, 0x2 64-Bit-Inodes, 0x4 verschlüsselt */
    if (h->bs < 4096 || h->bs > (1u << 20) || (h->bs & (h->bs - 1))) return false;
    if (h->bloecke < 3 || h->bloecke > (1ULL << 40) / h->bs || h->iblocks < 1 || h->iblocks >= h->bloecke - 1) return false;
    if (x->groesse && h->bloecke * h->bs > x->groesse) return false;
    if (h->inodes < 3 || h->inodes > h->iblocks * (h->bs / PFS_INODE)) return false;
    return true;
}
static bool pfs_inode_lesen(exf *x, const pfs_kopf *h, unsigned long long n, pfs_inode *i)
{
    unsigned long long pro = h->bs / PFS_INODE;
    unsigned char b[PFS_INODE];
    if (n >= h->inodes || !exf_lies(x, h->bs * (1 + n / pro) + (n % pro) * PFS_INODE, sizeof b, b)) return false;
    i->mode = (unsigned)b[0] | (unsigned)b[1] << 8;
    i->flags = pkg_le32(b + 4);
    i->gr = exf_le64(b + 8); i->gr_echt = exf_le64(b + 0x10);
    unsigned long long bl = pkg_le32(b + 0x60);
    long long db0 = (int32_t)pkg_le32(b + 0x64);
    /* nur am Stück: db[1..11] alle -1 - bei höchstens einem Block auch alle 0 (so die Über-Wurzel in den echten Abbildern) */
    int32_t r1 = (int32_t)pkg_le32(b + 0x68);
    if ((r1 != -1 && (r1 != 0 || bl > 1))) return false;
    for (int j = 2; j < 12; j++) if ((int32_t)pkg_le32(b + 0x64 + 4 * j) != r1) return false;
    /* ohne Indirekt-Blöcke: alle 0 oder - NEU 1.0.9 (PS5-Test 09.11.2026 17:06, Spider-Man 2 als .ffpfsc ohne Bild, per FTP nachgesehen) -
     * alle -1; beides heißt "keiner" (der Bau-Simulator hat 0, Spider-Man 2 -1) */
    uint32_t i0 = pkg_le32(b + 0x94);
    if (i0 != 0 && i0 != 0xFFFFFFFFu) return false;
    for (int j = 1; j < 5; j++) if (pkg_le32(b + 0x94 + 4 * j) != i0) return false;
    if (db0 < 0 || (unsigned long long)db0 > h->bloecke || bl > h->bloecke - (unsigned long long)db0 || i->gr > bl * h->bs) return false;
    if (i->gr > 0 && (unsigned long long)db0 <= h->iblocks) return false;                       /* Daten nie im Kopf oder bei den Inodes */
    i->ab = (unsigned long long)db0 * h->bs;
    return true;
}
/* Einträge eines Ordners der Reihe nach, ohne "." und ".."; 0 = Ende (auch bei einem kaputten Eintrag - dann gilt der Rest nicht und
 * e->kaputt ist gesetzt; Codex-Prüfung 131: so ist ein kaputter Eintrag vom richtigen Ende der Liste zu unterscheiden; 132: auch ein
 * angefangener Eintrag ganz am Ende - richtig endet eine Liste mit 16 Null-Byte oder genau am Ende, höchstens mit Nullen dahinter).
 * Codex-Prüfung 130: jeder andere Eintrag kommt zurück - auch einer mit langem Namen oder fremder Art (dann name ""), damit "genau eine
 * Datei" in pfs_innen richtig zählt; gesucht wird nur nach kurzen Namen. */
static size_t pfs_naechster(const unsigned char *v, size_t n, size_t i, pfs_eintrag *e)
{
    e->kaputt = false;
    while (i + 16 <= n) {
        uint32_t ino = pkg_le32(v + i);
        int32_t typ = (int32_t)pkg_le32(v + i + 4), nl = (int32_t)pkg_le32(v + i + 8), el = (int32_t)pkg_le32(v + i + 12);
        if (!ino && !typ && !nl && !el) return 0;
        if (el < 16 || (el & 7) || (size_t)el > n - i || nl < 1 || nl > el - 16) { e->kaputt = true; return 0; }
        size_t nx = i + (size_t)el;
        if (typ == 4 || typ == 5) { i = nx; continue; }                       /* "." und ".." */
        e->ino = ino; e->typ = typ; e->name[0] = 0;
        if (nl < (int32_t)sizeof e->name && !memchr(v + i + 16, 0, (size_t)nl)) { memcpy(e->name, v + i + 16, (size_t)nl); e->name[nl] = 0; }
        return nx;
    }
    for (; i < n; i++) if (v[i]) { e->kaputt = true; break; }
    return 0;
}
static unsigned char *pfs_daten(exf *x, const pfs_inode *i, size_t max, size_t *n)   /* ungepackte Daten mit 0 am Ende; NULL = nicht so lesbar */
{
    if ((i->flags & 1) || i->gr == 0 || i->gr > max) return NULL;
    unsigned char *d = a_hole(x->t, (size_t)i->gr + 1);
    if (!exf_lies(x, i->ab, (size_t)i->gr, d)) return NULL;
    d[i->gr] = 0; *n = (size_t)i->gr;
    return d;
}
static bool pfs_suchen(exf *x, const pfs_kopf *h, const pfs_inode *ordner, const char *name, int typ, pfs_inode *aus)
{
    size_t n; pfs_eintrag e, f = {0}; bool da = false;
    if (!PFS_ORDNER(ordner->mode)) return false;
    unsigned char *v = pfs_daten(x, ordner, EXF_VERZ_MAX, &n);
    if (!v) return false;
    for (size_t i = 0; (i = pfs_naechster(v, n, i, &e)) != 0; )
        if (!da && e.typ == typ && exf_gleich(e.name, name)) { f = e; da = true; }
    /* Codex-Prüfung 132: nur aus einer ganz lesbaren Liste (endet sie an einem kaputten Eintrag, wird ihr nicht getraut) */
    return da && !e.kaputt && pfs_inode_lesen(x, h, f.ino, aus) && (typ == 3 ? PFS_ORDNER(aus->mode) : PFS_DATEI(aus->mode));
}
static bool pfs_lies(void *c, unsigned long long ab, size_t n, unsigned char *ziel)   /* für kern_pfsc: die Datei selbst, Stelle ab Dateianfang */
{
    exf *x = c; size_t len = 0;
    if (ab > (unsigned long long)INT64_MAX - n || (x->groesse && (ab > x->groesse || n > x->groesse - ab))) return false;
    if (!s_lies_ab(x->k->S, x->p, (long long)ab, n, ziel, &len) || len != n) return false;
    x->gelesen += n;
    return true;
}
static const char *pfs_innen(exf *x, const char **tid, const char **bild)
{
    kern *k = x->k;   /* für A */
    *tid = ""; *bild = "";
    pfs_kopf h; pfs_inode sw, ur, d;
    if (x->pfsc || x->basis || !pfs_kopf_lesen(x, &h) || !pfs_inode_lesen(x, &h, 0, &sw) || !pfs_suchen(x, &h, &sw, "uroot", 3, &ur)) return "";
    /* uroot einmal lesen (Ordner sind meist einen ganzen Block groß): sce_sys - oder genau eine Datei (.ffpfsc); jeder Eintrag außer "." und
     * ".." zählt mit, auch einer mit langem Namen oder fremder Art (Codex-Prüfung 130) */
    size_t n; pfs_eintrag e, ein = {0}, se = {0}; int dateien = 0, andere = 0; bool sys_da = false;
    unsigned char *uv = pfs_daten(x, &ur, EXF_VERZ_MAX, &n);
    if (!uv) return "";
    for (size_t i = 0; (i = pfs_naechster(uv, n, i, &e)) != 0; ) {
        if (e.typ == 3 && !sys_da && exf_gleich(e.name, "sce_sys")) { se = e; sys_da = true; }
        if (e.typ == 2) { dateien++; ein = e; } else andere++;
    }
    if (e.kaputt) return "";                                   /* Codex-Prüfungen 131, 132: einer kaputten Liste wird nicht getraut */
    if (sys_da) {
        pfs_inode sys, pj, ps, ic; bool pj_da = false, ps_da = false, ic_da = false;
        if (!pfs_inode_lesen(x, &h, se.ino, &sys) || !PFS_ORDNER(sys.mode)) return "";
        unsigned char *sv = pfs_daten(x, &sys, EXF_VERZ_MAX, &n);
        for (size_t i = 0; sv && (i = pfs_naechster(sv, n, i, &e)) != 0; ) {   /* sce_sys auch nur einmal lesen */
            if (e.typ != 2) continue;
            if (!pj_da && exf_gleich(e.name, "param.json")) pj_da = pfs_inode_lesen(x, &h, e.ino, &pj) && PFS_DATEI(pj.mode);
            else if (!ps_da && exf_gleich(e.name, "param.sfo")) ps_da = pfs_inode_lesen(x, &h, e.ino, &ps) && PFS_DATEI(ps.mode);
            else if (!ic_da && exf_gleich(e.name, "icon0.png")) ic_da = pfs_inode_lesen(x, &h, e.ino, &ic) && PFS_DATEI(ic.mode);
        }
        if (e.kaputt) return "";                               /* sce_sys ebenso (Codex-Prüfung 132) */
        const char *titel = "";                                /* Name und Kennung wie exf_innen */
        unsigned char *j = pj_da ? pfs_daten(x, &pj, EXF_DATEI_MAX, &n) : NULL;
        if (j && !memchr(j, 0, n)) {
            titel = kern_json_titel(A, (const char *)j);
            htreffer m = h_mt(x->t, (const char *)j, "(?-i)\"titleId\"\\s*:\\s*\"([A-Z]{4}\\d{5})\"");
            if (titel[0] && m.ok) *tid = a_kopie(A, m.gr[1]);
        }
        unsigned char *f = !titel[0] && ps_da && ps.gr >= 20 ? pfs_daten(x, &ps, EXF_DATEI_MAX, &n) : NULL;
        char t[128], ti[16];
        if (f && ((pkg_sfo_feld(f, n, "TITLE_04", t, sizeof t) && t[0]) || (pkg_sfo_feld(f, n, "TITLE_01", t, sizeof t) && t[0])
                  || (pkg_sfo_feld(f, n, "TITLE", t, sizeof t) && t[0]))) {   /* PS4-Spiel im Abbild */
            titel = a_kopie(A, t);
            if (pkg_sfo_feld(f, n, "TITLE_ID", ti, sizeof ti) && r_m(ti, "^[A-Z]{4}\\d{5}$")) *tid = a_kopie(A, ti);
        }
        unsigned char sig[8];                                  /* Bild: ungepackt, am Stück, mit PNG-Kennung - dann liest es die Oberfläche */
        if (titel[0] && ic_da && !(ic.flags & 1) && ic.gr >= 8 && ic.gr <= 4u * 1024 * 1024 && exf_lies(x, ic.ab, sizeof sig, sig) && !memcmp(sig, "\x89PNG\r\n\x1a\n", 8))
            *bild = bild_ort(x, ic.ab, ic.gr);
        return titel;
    }
    /* .ffpfsc (und ein ungepacktes Abbild im Abbild): in uroot genau eine Datei, sonst nichts (die Liste ist ganz lesbar, siehe oben) */
    if (dateien != 1 || andere != 0 || !pfs_inode_lesen(x, &h, ein.ino, &d) || !PFS_DATEI(d.mode) || d.gr == 0) return "";
    exf y = *x;
    y.fat = y.heap = y.cl = 0; y.anz = 0; y.gelesen = 0;
    kern_pfsc *pc = a_hole(x->t, sizeof *pc);
    if (d.flags & 1) {
        if (!kern_pfsc_auf(pc, x->t, pfs_lies, x, d.ab, d.gr) || pc->laenge != d.gr_echt) return "";
        y.pfsc = pc; y.groesse = pc->laenge;
    } else { y.basis = d.ab; y.groesse = d.gr; }
    const char *r = exf_innen(&y, tid, bild);
    if (!r[0]) r = ufs_innen(&y, tid, bild);
    x->gelesen += y.gelesen;                                   /* gepackt: zählt pfs_lies selbst (y.gelesen bleibt 0) */
    return r;
}
static size_t exf_zuletzt;   /* nur für die PC-Tests: so viele Bytes las der letzte Aufruf */
static const char *abbild_titel_mit(kern *k, const char *p, long long groesse, const char **tid, const char **bild)
{
    arena *volatile t = arena_neu();
    exf x = { k, p, t, 0, 0, 0, groesse > 0 ? (unsigned long long)groesse : 0, 0, 0 };
    const char *volatile r = "";
    const char *ti = "", *bi = "";
    exf_zuletzt = 0;
    VERSUCHE {
        r = exf_innen(&x, &ti, &bi);
        if (!r[0] && !kern_bereiche_106()) r = ufs_innen(&x, &ti, &bi);   /* 1.0.9: sonst UFS2 */
        if (!r[0] && !kern_bereiche_106()) r = pfs_innen(&x, &ti, &bi);   /* 1.0.9 (#144): sonst PFS */
        exf_zuletzt = x.gelesen;
    }
    FANGE { arena_frei(t); t = NULL; if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); r = ""; ti = ""; bi = ""; } ENDE_VERSUCHE;
    if (t) arena_frei(t);
    *tid = r[0] ? ti : ""; *bild = r[0] ? bi : "";
    return r;
}
static const char *abbild_titel(kern *k, const char *p, long long groesse) { const char *ti, *bi; return abbild_titel_mit(k, p, groesse, &ti, &bi); }
/* NEU 1.0.8 (PS5-Test 07.11.2026 16:31): Name, Kennung und Bild eines entpackten Spiels (Ordner mit sce_sys/param.json bzw. param.sfo) -
 * wie bei installierten Spielen (app_titel_lesen), aber höchstens 256 KB (param.json) bzw. 64 KB (param.sfo) - der Ordner kann alles
 * Mögliche enthalten. Nur lesen, mit eigenem Speicher, der danach frei wird; ein Lesefehler heißt: kein Name. */
#define ORDNER_JSON_MAX (256 * 1024)
#define ORDNER_SFO_MAX (64 * 1024)
static const unsigned char *datei_bis(kern *k, arena *t, const char *p, size_t max, size_t *n)   /* NULL = fehlt, nicht lesbar, leer oder größer */
{
    unsigned char *b = a_hole(t, max + 2); size_t len = 0;
    if (!s_lies_ab(k->S, p, 0, max + 1, b, &len) || len == 0 || len > max) return NULL;
    b[len] = 0; *n = len;
    return b;
}
static const char *ordner_innen(kern *k, arena *t, const char *p, const char **tid)
{
    size_t n = 0; const char *titel = "", *ti = "";
    const unsigned char *j = datei_bis(k, t, VERB(t, p, "/sce_sys/param.json"), ORDNER_JSON_MAX, &n);
    if (j && !memchr(j, 0, n)) {
        const char *s = h_ohne_bom((const char *)j);
        if (h_m(s, "titleName")) {
            titel = kern_json_titel(t, s);
            htreffer m = h_mt(t, s, "(?-i)\"titleId\"\\s*:\\s*\"([A-Z]{4}\\d{5})\"");
            if (titel[0] && m.ok) ti = m.gr[1];
        }
    }
    if (!titel[0]) {                                           /* PS4-Spiel als Ordner: param.sfo */
        const unsigned char *f = datei_bis(k, t, VERB(t, p, "/sce_sys/param.sfo"), ORDNER_SFO_MAX, &n);
        if (f) {
            mtab h = kern_sfo_lesen(t, f, n);
            titel = kern_sfo_titel(t, &h);
            const char *s = kern_sfo_wert(t, &h, "TITLE_ID");
            if (titel[0] && r_m(s, "^[A-Z]{4}\\d{5}$")) ti = s;
        }
    }
    if (!titel[0]) return "";
    *tid = a_kopie(A, ti);
    return a_kopie(A, titel);
}
static const char *ordner_titel(kern *k, const char *p, const char **tid, const char **bild)
{
    *tid = ""; *bild = "";
    arena *volatile t = arena_neu();
    const char *volatile r = "", *volatile ti = "";
    VERSUCHE { const char *x = ""; r = ordner_innen(k, t, p, &x); ti = x; }
    FANGE { arena_frei(t); t = NULL; if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); r = ""; ti = ""; } ENDE_VERSUCHE;
    if (t) arena_frei(t);
    if (!r[0]) return "";
    *tid = ti;
    volatile bool da = false;
    VERSUCHE { da = ti[0] && s_existiert(k->S, VERB(A, p, "/sce_sys/icon0.png")); }
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); da = false; } ENDE_VERSUCHE;
    if (da) *bild = VERB(A, p, "/sce_sys/icon0.png\n0\n0");
    return r;
}
/* NEU 1.0.8 (PS5-Test 07.11.2026 16:31, da_Chris86: "bei den PS4-Spielen sind die Bilder und die Beschriftung da, bei den PS5-Spielen
 * nicht"): Anzeige-Zusatz "  [Spiel: <Name>]" für eine Spieldatei oder einen Spiel-Ordner ohne Kachel. Der Name kommt aus dem Spiel selbst
 * (Abbild: sce_sys im exFAT oder, seit 1.0.9, im UFS2; Ordner: sce_sys/param.json bzw. param.sfo), sonst aus den Belegen zur Spielnummer im Namen (Aufnahmen, frühere
 * Protokolle, Spielstände - rest_name, wie bei Resten). Bis hier wurde nur ein Abbild gelesen, dessen Dateiname nichts als die Spielnummer
 * hatte. Die Kennung, zu der der Name gehört, steht immer im Zusatz ("[Spiel: Marvel's Spider-Man 2 (PPSA03016)]") - daran findet die
 * Oberfläche das Bild, auch wenn im Dateinamen keine oder eine andere Nummer steht (Codex-Prüfung 111); Zeile und Name zeigen sie nicht
 * (kern_art.c spiel_von). Wo icon0.png liegt, merkt sich BILD_ORT (lesen tut es nur die Oberfläche). "" = kein Name. */
static const char *spiel_zusatz(kern *k, const char *p, const char *name, long long groesse, bool ordner)
{
    const char *t = "", *tid = "", *bild = "";
    if (kern_bereiche_106()) {                               /* Vergleichstests: wie bisher nur ein Abbild mit der Nummer als Namen */
        if (ordner || !h_m(name, ABBILD_NUR_NUMMER)) return "";
        t = abbild_titel(k, p, groesse);
    }
    else if (ordner) t = ordner_titel(k, p, &tid, &bild);
    else t = abbild_titel_mit(k, p, groesse, &tid, &bild);
    const char *kid = tid;                                   /* Kennung, zu der Name und Bild gehören */
    if (!t[0] && !kern_bereiche_106()) {
        htreffer m = h_mt(A, name, "(?-i)(?:^|[^A-Za-z0-9])([A-Z]{4}\\d{5})(?:[^0-9]|$)");
        if (m.ok) { t = rest_name(k, m.gr[1]); kid = m.gr[1]; }
    }
    if (strlen(t) > 100) { char b[101]; memcpy(b, t, 100); b[100] = 0; t = pkg_text(A, b); } else t = pkg_text(A, t);
    if (!kern_bereiche_106()) t = h_trim(A, r_r(A, t, "\\s{2,}", " "));   /* Codex-Prüfung 113: im Namen nie "  [" - so beginnt der nächste Zusatz */
    while (*t == ' ') t++;
    if (!t[0]) return "";
    if (tid[0] && bild[0] && !mt_hat(&k->BILD_ORT, tid)) mt_setze(A, &k->BILD_ORT, a_kopie(A, tid), (void *)a_kopie(A, bild));
    return VERB(A, T("  [Spiel: ", "  [game: "), t, kid[0] ? VERB(A, " (", kid, ")") : "", "]");
}
/* NEU 1.0.8: nur für die PC-Tests - Name aus dem Abbild (ohne Prüfung des Dateinamens) und wie viele Bytes gelesen wurden */
const char *kern_abbild_titel_test(kern *k, const char *p, size_t *gelesen) { const char *t = abbild_titel(k, p, 0); *gelesen = exf_zuletzt; return t; }
/* NEU 1.0.9 (Codex-Prüfung 125: die selbst gebauten Testabbilder prüfen den Leser nur gegen sich selbst): nur für die PC-Tests - eine
 * Datei im UFS2-Abbild über ihren Pfad lesen (höchstens 64 KB). Damit wird der Leser an einem fremden, echten UFS2 geprüft
 * (test/kern/daten/ufs2_dfvfs.raw.gz, mit FreeBSD newfs angelegt). Verknüpfungen und Ordner werden nicht als Datei gelesen. NULL = nicht da. */
const unsigned char *kern_ufs_datei_test(kern *k, const char *p, const char *innen, size_t *n)
{
    *n = 0;
    arena *volatile t = arena_neu();
    const unsigned char *volatile r = NULL;
    volatile size_t rn = 0;
    VERSUCHE {
        exf x = { k, p, t, 0, 0, 0, 0, 0, 0 };
        ufs_sb s; ufs_inode d = {0}; ufs_eintrag e = {0};
        bool ok = ufs_sb_lesen(&x, &s) && ufs_inode_lesen(&x, &s, UFS2_WURZEL, &d);
        sliste teile = r_split(t, innen, "/");
        for (int j = 0; ok && j < teile.n; j++) {
            if (!teile.e[j][0]) continue;
            size_t vn; unsigned char *v = d.art == 4 ? ufs_daten(&x, &s, &d, EXF_VERZ_MAX, &vn) : NULL;
            bool gef = false;
            for (size_t i = 0; v && (i = ufs_naechster(v, vn, i, &e)) != 0; ) if (!strcmp(e.name, teile.e[j])) { gef = ufs_inode_lesen(&x, &s, e.ino, &d); break; }
            ok = gef;
        }
        size_t m = 0; unsigned char *b = ok && d.art == 8 ? ufs_daten(&x, &s, &d, 64 * 1024, &m) : NULL;
        if (b && m == d.laenge) { unsigned char *c = a_hole(A, m + 1); memcpy(c, b, m + 1); r = c; rn = m; }
    }
    FANGE { arena_frei(t); t = NULL; if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); r = NULL; rn = 0; } ENDE_VERSUCHE;
    if (t) arena_frei(t);
    *n = rn;
    return r;
}
/* NEU 1.0.9 (#144): nur für die PC-Tests - eine Datei im PFS-Abbild über ihren Pfad unter uroot lesen (höchstens 64 KB, nur ungepackt). Damit
 * wird der Leser an echten Abbildern geprüft (am PC, nicht im Projekt: da_Chris86s Testdateien). NULL = nicht da. */
const unsigned char *kern_pfs_datei_test(kern *k, const char *p, const char *innen, size_t *n)
{
    *n = 0;
    arena *volatile t = arena_neu();
    const unsigned char *volatile r = NULL;
    volatile size_t rn = 0;
    VERSUCHE {
        exf x = { k, p, t, 0, 0, 0, 0, 0, 0, 0, NULL };
        pfs_kopf h; pfs_inode sw, d;
        bool ok = pfs_kopf_lesen(&x, &h) && pfs_inode_lesen(&x, &h, 0, &sw) && pfs_suchen(&x, &h, &sw, "uroot", 3, &d);
        sliste teile = r_split(t, innen, "/");
        for (int j = 0; ok && j < teile.n; j++) {
            if (!teile.e[j][0]) continue;
            pfs_inode z;
            ok = pfs_suchen(&x, &h, &d, teile.e[j], j + 1 < teile.n ? 3 : 2, &z);
            if (ok) d = z;
        }
        size_t m = 0; unsigned char *b = ok && PFS_DATEI(d.mode) ? pfs_daten(&x, &d, 64 * 1024, &m) : NULL;
        if (b) { unsigned char *c = a_hole(A, m + 1); memcpy(c, b, m + 1); r = c; rn = m; }
    }
    FANGE { arena_frei(t); t = NULL; if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); r = NULL; rn = 0; } ENDE_VERSUCHE;
    if (t) arena_frei(t);
    *n = rn;
    return r;
}
/* NEU 1.0.9 (#145, Recherche doku/recherche_prosperostore_2026-11-08.md; da_Chris86 08.11.2026 08:12: "schau mal wie viele apps es anbietet.
 * die alle sauber zu löschen dann wieder"): ProsperoStore (darkxex/ProsperoStore @44ae9f6, src/install/transaction.cpp, nur gelesen) legt je
 * installierter App einen Beleg /data/prosperostore/receipts/<Kennung>.json an ("titleId", "location") und je Laufwerk Arbeitsordner
 * <Laufwerk>/prosperostore/{previous,backup,trash,staging}/<Kennung> (staging auch <Kennung>.zip): previous ist die vorige Fassung samt allem,
 * was darin lag (bis zum nächsten Update), backup dasselbe während eines Updates, trash und staging sind Reste einer Arbeit. Deinstalliert
 * ProsperoStore eine App, bleibt previous liegen. Diese Teile gehören der App mit dieser Kennung, nicht ProsperoStore: jeder ist ein eigener
 * Eintrag (StoreTeil), steht bei der Kachel mit dieser Kennung (kachel_bloecke) und ohne sie für sich - mit dem Namen der App, wenn er sich
 * lesen lässt. Der Rest des Ordners bleibt EIN Eintrag bei ProsperoStore - nur ein Unterordner mit PKG-Dateien wird wie überall aufgeteilt
 * (PKG-Dateien bei ihrem Spiel, sein Rest eine eigene Zeile, auch bei ProsperoStore). Der App-Ordner selbst (homebrew/<Kennung>) steht schon über
 * seine Kennung bei der Kachel. Ein Beleg zählt nur, wenn er sich lesen lässt und genau seine Kennung nennt. Steht in journal.json eine
 * angefangene Arbeit, bleiben die Teile dieser Kennung im Rest (ProsperoStore macht sie beim nächsten Start selbst fertig); lässt sich
 * journal.json nicht lesen, wird nichts herausgelöst (wie bis 1.0.8). PKG-Dateien in den Teilen und im Rest stehen wie überall bei ihrem
 * Spiel (Befund 12). Nur lesen. */
#define STORE_JSON_MAX 16384
typedef struct { const char *pfad, *id, *an; int art; bool ordner; } store_teil;
/* "titleId" einer kleinen JSON-Datei von ProsperoStore (Beleg, journal.json): "" = keine gültige Kennung, NULL = nicht lesbar */
static const char *store_kennung(kern *k, const char *p)
{
    const unsigned char *volatile d = NULL; volatile size_t n = 0; volatile bool ok = false;
    VERSUCHE { const unsigned char *dd = NULL; size_t nn = 0; ok = s_kopf(k->S, p, STORE_JSON_MAX + 1, &dd, &nn); d = dd; n = nn; }
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); ok = false; } ENDE_VERSUCHE;
    if (!ok || !d) return NULL;
    if (n == 0 || n > STORE_JSON_MAX || memchr((const void *)d, 0, n)) return "";
    cJSON *o = cJSON_ParseWithOpts(a_kopie_n(A, (const char *)d, n), NULL, true);
    if (!o) return "";
    const cJSON *i = cJSON_IsObject(o) ? cJSON_GetObjectItemCaseSensitive(o, "titleId") : NULL;
    const char *r = (cJSON_IsString(i) && i->valuestring && r_m(i->valuestring, "^[A-Z]{4}[0-9]{5}$")) ? a_kopie(A, i->valuestring) : "";
    cJSON_Delete(o);
    return r;
}
/* Kennung der angefangenen Arbeit laut /data/prosperostore/journal.json: "" = keine, NULL = unklar (nicht lesbar oder ungültig) */
static const char *store_offen(kern *k)
{
    const char *j = "/user/data/prosperostore/journal.json";
    volatile int da = -1;
    VERSUCHE { da = s_existiert(k->S, j) ? 1 : 0; } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); da = -1; } ENDE_VERSUCHE;
    if (da <= 0) return da == 0 ? "" : NULL;
    const char *id = store_kennung(k, j);
    return id && id[0] ? id : NULL;
}
/* Zusatz "  [App: <Name> (<Kennung>)]" - der Name aus der aufgehobenen Fassung selbst (sce_sys), sonst wie bei Resten; "" = keiner */
static const char *store_zusatz(kern *k, const char *p, const char *id, bool ordner)
{
    const char *tid = "", *bild = "", *t = ordner ? ordner_titel(k, p, &tid, &bild) : "";
    if (!t[0] || strcmp(tid, id) != 0) { t = rest_name(k, id); bild = ""; }
    if (strlen(t) > 100) { char b[101]; memcpy(b, t, 100); b[100] = 0; t = pkg_text(A, b); } else t = pkg_text(A, t);
    t = h_trim(A, r_r(A, t, "\\s{2,}", " "));   /* im Namen nie "  [" (Codex-Prüfung 113) */
    if (!t[0]) return "";
    if (bild[0] && !mt_hat(&k->BILD_ORT, id)) mt_setze(A, &k->BILD_ORT, a_kopie(A, id), (void *)a_kopie(A, bild));
    return VERB(A, T("  [App: ", "  [app: "), t, " (", id, ")]");
}
/* p = .../prosperostore (intern, M.2, USB), anzeige = Name in der Zeile. false = nichts herausgelöst (dann wie bisher) */
static bool merke_store(kern *k, const char *p, int kat, const char *anzeige)
{
    if (kern_bereiche_106() || kern_selbstschutz_enthaelt(k, p)) return false;
    const char *pz = VERB(A, p, "/");
    for (int j = 0; j < k->SpielQuellen.n; j++) if (txt_beginnt(k->SpielQuellen.e[j], pz)) return false;   /* ein Spiel darin: wie bisher */
    dliste *volatile l = NULL;
    VERSUCHE { l = s_liste(k->S, p, false); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); l = NULL; } ENDE_VERSUCHE;
    if (!l) return false;
    const char *offen = store_offen(k);
    if (!offen) return false;                                           /* angefangene Arbeit unklar: nichts herauslösen */
    bool intern = !strcmp(p, "/user/data/prosperostore");               /* Belege gibt es nur hier */
    static const char *const UNTER[5] = { "previous", "backup", "trash", "staging", "receipts" };
    /* 1. was herausgelöst wird - noch nichts gemerkt */
    pliste tl = {0}, rest_d = {0}; sliste rest = {0}, auf = {0};
    for (int i = 0; i < l->n; i++) {
        const datei *e = &l->e[i];
        const char *q = VERB(A, p, "/", e->name);
        int u = -1;
        for (int j = 0; j < 5; j++) if (!strcmp(e->name, UNTER[j])) u = j;
        dliste *volatile ul = NULL;
        if (u >= 0 && !e->ist_link && e->ist_ordner && (u < 4 || intern)) {
            VERSUCHE { ul = s_liste(k->S, q, false); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); ul = NULL; } ENDE_VERSUCHE;
        }
        int vor = tl.n; sliste ur = {0}; pliste ur_d = {0};
        for (int j = 0; ul && j < ul->n; j++) {
            const datei *g = &ul->e[j];
            const char *gp = VERB(A, q, "/", g->name), *id = NULL;
            int art = 0;
            if (g->ist_link) {}
            else if (u < 4 && g->ist_ordner && r_m(g->name, "^[A-Z]{4}[0-9]{5}$")) { id = g->name; art = u < 2 ? STORE_FASSUNG : STORE_REST; }
            else if (u == 3 && !g->ist_ordner && r_m(g->name, "^[A-Z]{4}[0-9]{5}\\.zip$")) { id = a_kopie_n(A, g->name, 9); art = STORE_REST; }
            else if (u == 4 && !g->ist_ordner && r_m(g->name, "^[A-Z]{4}[0-9]{5}\\.json$")) {
                const char *b = a_kopie_n(A, g->name, 9), *jk = store_kennung(k, gp);
                if (jk && !strcmp(jk, b)) { id = b; art = STORE_BELEG; }
            }
            if (!id || !strcmp(id, offen)) { sl_add(A, &ur, gp); pl_add(A, &ur_d, (void *)g); continue; }
            store_teil *t = a_hole(A, sizeof *t);
            t->pfad = gp; t->id = id; t->art = art; t->ordner = g->ist_ordner; t->an = VERB(A, anzeige, "/", e->name, "/", g->name);
            pl_add(A, &tl, t);
        }
        if (tl.n == vor) { sl_add(A, &rest, q); pl_add(A, &rest_d, (void *)e); continue; }   /* nichts herausgelöst: als Ganzes im Rest */
        sl_add(A, &auf, q);
        for (int j = 0; j < ur.n; j++) { sl_add(A, &rest, ur.e[j]); pl_add(A, &rest_d, ur_d.e[j]); }
    }
    if (tl.n == 0) return false;
    /* 2. merken: aufgeteilte Ordner (äußere vor inneren, leer nach dem Löschen mit weg), die Teile, der Rest */
    sl_add(A, &k->AUFGEKLAPPT, p); sl_add_alle(A, &k->AUFGEKLAPPT, &auf);
    for (int i = 0; i < tl.n; i++) {
        store_teil *t = tl.e[i];
        const char *zs = store_zusatz(k, t->pfad, t->id, t->ordner && t->art == STORE_FASSUNG);
        int vb = k->bau.n, r = t->ordner ? merke_pkg_ordner(k, t->pfad, t->an, kat, NULL) : 0;   /* Befund 12: PKG-Dateien darin bei ihrem Spiel */
        if (r <= 0) {
            sliste e1 = L1(A, t->pfad);
            MERKE(k, &e1, kat, t->an, 0);
            if (r < 0 && k->MERKE_OK && k->bau.n > vb) ((eintrag *)k->bau.e[k->bau.n - 1])->PkgOffen = true;
        }
        for (int b = vb; b < k->bau.n; b++) {
            eintrag *f = k->bau.e[b];
            if (f->PkgArt) continue;
            f->StoreTeil = (unsigned char)t->art; f->Text = VERB(A, f->Text, zs); f->TextBasis = VERB(A, f->TextBasis, zs);
        }
    }
    sliste rest2 = {0}; bool pk_offen = false;
    const char *wz = intern ? p : NULL;                                 /* wie die Sammelzeile neben PS5 Cleanup (Codex-Prüfung 98) */
    for (int j = 0; j < rest.n; j++) {
        const char *q = rest.e[j]; const datei *d = rest_d.e[j];
        if (!d->ist_link && !ist_zwischendatei(d->name)) {
            const char *an = VERB(A, anzeige, q + strlen(p));
            if (!d->ist_ordner && h_m(d->name, "(?i)\\.pkg$")) { merke_pkg(k, q, an, d->groesse); continue; }
            if (d->ist_ordner) { int r = merke_pkg_ordner(k, q, an, kat, wz); if (r > 0) continue; if (r < 0) pk_offen = true; }
        }
        sl_add(A, &rest2, q);
    }
    if (rest2.n > 0) {
        MERKE(k, &rest2, kat, VERB(A, anzeige, T("  (ohne die Teile einzelner Apps)", "  (without the parts of single apps)")), 0);
        if (k->MERKE_OK && k->bau.n > 0) { eintrag *f = k->bau.e[k->bau.n - 1]; if (pk_offen) f->PkgOffen = true; if (wz && !f->ToolOrdner) f->ToolOrdner = wz; }
    }
    return true;
}
static void merke_ordner(kern *k, const char *basis, const datei *eintraege, int anz, int kat, const char *vorsatz)
{
    for (int i = 0; i < anz; i++) {
        const datei *e = &eintraege[i];
        const char *x = e->name; bool link = e->ist_link;
        const char *p = VERB(A, basis, "/", x);
        int pr = 0;   /* Codex-Prüfung 90: Ergebnis von merke_pkg_ordner (-1 = PKG-Dateien darin nicht prüfbar) */
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
        /* NEU 1.0.9 (#145): Ordner von ProsperoStore (intern, USB-Laufwerk) - die Teile einzelner Apps stehen bei ihrer App (merke_store) */
        if (!drin && !link && e->ist_ordner && h_gleich(x, "prosperostore") && h_m(basis, "^(/user/data|/mnt/(ext|usb)\\d+)$")
            && merke_store(k, p, kat, VERB(A, vorsatz, x))) continue;
        /* NEU 1.0.6 (PS5-Test 28.10.2026, da_Chris86: "doof, wenn so viele pldmgr-Einträge"): liegt NUR PS5 Cleanup in einem
         * Ordner (kein ShadowMount-Spiel), wird er nicht Datei für Datei aufgeklappt, sondern als EIN Eintrag "alles außer
         * PS5 Cleanup" angeboten; PS5 Cleanup selbst steht gesperrt daneben. Ausnahme: Ordner "homebrew" - dort bleibt jede
         * App ein eigener Eintrag. */
        if (!drin && !link && e->ist_ordner && kern_selbstschutz_enthaelt(k, p) && !h_gleich(x, "homebrew")) {
            sliste frei = {0}, eig = {0}; pliste frei_d = {0};
            sammle_ohne_schutz(k, p, &frei, &frei_d, &eig);
            bool echt_eigen = false;   /* 1.0.6 Fix: nur private, nicht lesbare Ordner geschützt (Playden) -> ohne Zusatz */
            /* NEU 1.0.7 (PS5-Test 02.11.2026, prosperoeden): "(alles außer PS5 Cleanup)" nur, wenn wirklich PS5 Cleanup darin liegt - einzelne
             * Dateien, die sich nicht lesen ließen und darum geschützt sind, machen den Ordner nicht zu "unserem". Sie bleiben still stehen
             * und werden nicht einzeln aufgelistet (wie private Ordner, da_Chris86 30.10.2026: "keinen Hinweis"). */
            for (int j = 0; j < eig.n; j++) if (kern_bereiche_106() ? !kern_selbstschutz_unlesbar(k, eig.e[j]) : kern_selbst_eigen(k, eig.e[j])) echt_eigen = true;
            /* Codex-Prüfung 93: auch neben PS5 Cleanup steht jede PKG-Datei bei ihrem Spiel, ein Ordner mit PKG-Dateien wird aufgeteilt
             * (wie überall); ließ sich ein Ordner nicht prüfen, kann die Sammelzeile PKG-Dateien enthalten (PkgOffen: rot, nur einzeln) */
            sliste frei2 = {0}; bool offen = false;
            const char *wz = h_m(p, "^/user/data/(homebrew/)?[^/]+$") ? p : NULL;   /* Codex-Prüfung 98 (P1): Tool-Ordner dieser Sammelzeile */
            for (int j = 0; j < frei.n; j++) {
                const char *q = frei.e[j]; const datei *d = frei_d.e[j];
                if (!kern_bereiche_106() && !d->ist_link && !ist_zwischendatei(d->name)) {
                    const char *an = VERB(A, vorsatz, x, q + strlen(p));
                    if (!d->ist_ordner && h_m(d->name, "(?i)\\.pkg$")) { merke_pkg(k, q, an, d->groesse); continue; }
                    if (d->ist_ordner) { int r = merke_pkg_ordner(k, q, an, kat, wz); if (r > 0) continue; if (r < 0) offen = true; }
                }
                sl_add(A, &frei2, q);
            }
            if (frei2.n > 0) {
                MERKE(k, &frei2, kat, echt_eigen ? VERB(A, vorsatz, x, T("  (alles außer PS5 Cleanup)", "  (everything except PS5 Cleanup)")) : VERB(A, vorsatz, x), 0);
                if (offen && k->MERKE_OK && k->bau.n > 0) ((eintrag *)k->bau.e[k->bau.n - 1])->PkgOffen = true;
                /* Codex-Prüfung 98 (P1): die Sammelzeile eines Tool-Ordners bleibt Daten dieses Ordners (wie der Rest "(ohne PKG-Dateien)",
                 * Codex-Prüfung 83) - Name und Art kommen vom Tool-Ordner, sonst wären z. B. Spielstand-Sicherungen "neu herunterladen" */
                if (wz && k->MERKE_OK && k->bau.n > 0) { eintrag *f = k->bau.e[k->bau.n - 1]; if (!f->ToolOrdner) f->ToolOrdner = wz; }
            }
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
            const char *nm = spiel_zusatz(k, p, x, 0, true);   /* NEU 1.0.8 (PS5-Test 07.11.2026 16:31): Name und Bild aus dem Spiel */
            sliste l = L1(A, p); MERKE(k, &l, 1, VERB(A, vorsatz, x, nm, T("  [Spiel-Ordner ohne Kachel]", "  [game folder, no tile]")), 0);
        }
        else if (!e->ist_ordner && h_m(x, "(?i)\\.(" ABBILD_ENDUNGEN ")$")) {   /* 28.10.2026: Spiel-Abbild ohne Kachel - ein Spiel (Bereich 1) */
            const char *zw = zweite_spieldatei(k, p, x);
            const char *nm = spiel_zusatz(k, p, x, e->groesse, false);   /* NEU 1.0.8 (Arbeitsplan 2.1, PS5-Test 07.11.2026 16:31): Spielname aus dem Abbild */
            sliste l = L1(A, p); MERKE(k, &l, 1, VERB(A, vorsatz, x, nm, zw ? zw : T("  [ohne Kachel]", "  [no tile]")), 0);
        }
        else if (!e->ist_ordner && h_m(x, "(?i)\\.iso$") && h_m(x, "(?i)(^|[^a-z])[a-z]{4}[-_]?\\d{5}")) {
            /* NEU 1.0.6 (PS5-Test 28.10.2026): ISO mit Spielnummer im Namen (z. B. PS3 BLES01969, PS2 SLES-12345) = Spiel-Abbild;
             * ohne Spielnummer (z. B. Linux-ISO) bleibt sie ein normaler Eintrag */
            sliste l = L1(A, p); MERKE(k, &l, 1, VERB(A, vorsatz, x, T("  [ISO]", "  [ISO]")), 0);
        }
        else if (!e->ist_ordner && h_m(x, "(?i)\\.elf$")) {   /* NEU 1.0.6: ELF-Dateien aller Speicher -> Jailbreak & Homebrew */
            sliste l = L1(A, p); MERKE(k, &l, 6, VERB(A, vorsatz, x), 0);
        }
        else if (!e->ist_ordner && !kern_bereiche_106() && h_m(x, "(?i)\\.pkg$")) {   /* NEU 1.0.8 (Arbeitsplan 2.2): Spielname aus dem PKG */
            merke_pkg(k, p, VERB(A, vorsatz, x), e->groesse);   /* NEU 1.0.8 (Befund 12): mit Spielnummer und Art, zuerst unter "Sonstiges" */
        }
        else if (e->ist_ordner && !kern_bereiche_106() && (pr = merke_pkg_ordner(k, p, VERB(A, vorsatz, x), kat, NULL)) > 0) {}   /* NEU 1.0.8 (Befund 12) */
        else {
            sliste l = L1(A, p); MERKE(k, &l, kat, VERB(A, vorsatz, x), 0);
            /* Codex-Prüfung 90: PKG-Dateien darin nicht prüfbar - der Ordner kann welche enthalten, also wie eine PKG-Datei nur einzeln */
            if (pr < 0 && k->MERKE_OK && k->bau.n > 0) ((eintrag *)k->bau.e[k->bau.n - 1])->PkgOffen = true;
        }
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
/* NEU 1.0.8 (PS5-Test 07.11.2026 22:53, da_Chris86s USB-Laufwerk: der Ordner "PS5" mit seinen Screenshots stand als "neu herunterladen"
 * unter "Sonstiges"): den Ordner "PS5" bzw. "PS4" legt die Konsole selbst an, wenn man etwas auf ein USB-Laufwerk kopiert - CREATE (PS5)
 * bzw. SHARE (PS4): Screenshots und Videos aus der Mediengalerie, SAVEDATA: Spielstände (psdevwiki.com/ps4/Save_Data: PS4/SAVEDATA/
 * <user Id>/<title Id>), EXPORT: Datensicherung der Konsole. Das sind eigene Daten (rot, "endgültig weg"). Erkannt nur an einem dieser
 * Unterordner - ein eigener Ordner "PS4" mit PKG-Dateien (wie bei da_Chris86) ist keiner. Gilt für das, was die Suche daraus unter
 * "Sonstiges" anbietet (den Ordner als Ganzes oder, aufgeklappt, seine Sony-Unterordner); PKG-Dateien, Spiele und ELF-Dateien behalten
 * ihre Art. Nur lesen, nur die oberste Ebene des Ordners. */
static int usb_kopie_teil(const char *rest)   /* rest = Weg hinter dem Ordner, z. B. "/CREATE/Screenshots" */
{
    if (h_m(rest, "(?i)^/(CREATE|SHARE)(/|$)")) return USB_KOPIE_AUFNAHMEN;
    if (h_m(rest, "(?i)^/SAVEDATA(/|$)")) return USB_KOPIE_SPIELSTAENDE;
    if (h_m(rest, "(?i)^/EXPORT(/|$)")) return USB_KOPIE_SICHERUNG;
    return 0;
}
static void usb_kopie_markieren(kern *k, const char *b, const datei *e, int vor)
{
    if (kern_bereiche_106() || k->bau.n <= vor || !e->ist_ordner || e->ist_link || !h_m(e->name, "(?i)^PS[45]$")) return;
    const char *p = VERB(A, b, "/", e->name);
    dliste *volatile l = NULL;
    VERSUCHE { l = s_liste(k->S, p, false); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); l = NULL; } ENDE_VERSUCHE;
    int alle = 0;
    for (int i = 0; l && i < l->n; i++)
        if (l->e[i].ist_ordner && !l->e[i].ist_link) alle |= usb_kopie_teil(VERB(A, "/", l->e[i].name));
    if (!alle) return;
    size_t lp = strlen(p);
    for (int j = vor; j < k->bau.n; j++) {
        eintrag *f = k->bau.e[j];
        if (f->Kat != 5 || f->PkgArt || f->Gesperrt) continue;
        int was = 0;
        for (int q = 0; q < f->PfadeBasis.n; q++) {
            const char *x = f->PfadeBasis.e[q];
            if (strncmp(x, p, lp) || (x[lp] && x[lp] != '/')) continue;
            was |= x[lp] ? usb_kopie_teil(x + lp) : alle;
        }
        if (was) f->UsbKopie = (unsigned char)was;
    }
}
static void merke_ext_ordner(kern *k, const char *basis, const datei *e)
{
    const char *p = VERB(A, basis, "/", e->name);
    int kat = ext_kat(e->name);
    if (!e->ist_ordner || e->ist_link || ist_zwischendatei(e->name) || sl_enthaelt(&k->SpielQuellen, p)) { merke_ordner(k, basis, e, 1, kat, ""); return; }
    /* NEU 1.0.9 (#145): Arbeitsordner von ProsperoStore auf M.2 und USB-Speicher - die Teile einzelner Apps stehen bei ihrer App, der Rest ist
     * EIN Eintrag (merke_store); ließ sich nichts herauslösen, wie bisher aufgeklappt */
    if (h_gleich(e->name, "prosperostore") && merke_store(k, p, kat, e->name)) return;
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
    /* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 14): WebKit Autoloader (itsPLK, WKAL00001) liest autoload.txt und die Payloads aus dem Ordner
     * ps5_autoloader (oben auf dem USB-Laufwerk, sonst /data/ps5_autoloader) - README github.com/itsPLK/ps5-webkit-autoloader */
    static const char *const T2[][2] = { { "PLDM00001", "pldmgr" }, { "PKGM00001", "pkgmgr" }, { "APDU00001", "ps5appdumper" },
                                         { "PSGC50001", "gamecompressor" }, { "LZGC50001", "amprstudio" }, { "WKAL00001", "ps5autoloader" }, { NULL, NULL } };
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
                                              "ps5suitefancfg", "user", NULL };   /* "user" = /data/user/patches von ps-patch-system (Patch Manager, APP_DATEN) */
/* NEU 1.0.9 (#146, da_Chris86 08.11.2026 08:12 zum ProsperoStore: "die alle sauber zu löschen dann wieder"): Daten-Ordner unter /data der
 * meistgenutzten Apps aus dem Katalog von ProsperoStore (darkxex/ps5-homebrew-catalog @d22b744, die 50 mit den meisten Likes und
 * Kommentaren) - je App im Quellcode nachgesehen, nur gelesen; Belege in doku/recherche_prosperostore_2026-11-08.md. Nur der eigene Ordner
 * der App, keine gemeinsam benutzten Ablagen (games, roms, xbox360, evoplayer, downloads). Gebraucht für: die feste Zuordnung zur Kachel mit
 * dieser Kennung (kachel_bloecke, vor dem Namensvergleich - "prospero-win" heißt nicht wie "PC5"); "bekannt" (ohne die App unter Apps statt
 * unter Sonstiges, in der Zeile "gehört zu <App>"); und wo darin nachweislich eigene Dateien liegen, was das ist (rot, "endgültig weg"). Wo
 * das nur manchmal so ist (games, roms, saves, memcards ...), erkennt es wie bisher wichtiger_inhalt. */
typedef struct { const char *id, *app, *ordner, *w_de, *w_en; } app_daten;
static const app_daten APP_DATEN[] = {
    { "PPSA99008", "ProsperoEden", "prosperoeden", NULL, NULL },
    { "PPSA99874", "ProsperoCraft", "ProsperoCraft", NULL, NULL },
    { "PPSA99000", "ProsperoStore", "prosperostore", NULL, NULL },
    { "PPSA11018", "PC5", "prospero-win", "deine Windows-Spiele, Schlüssel und Spielstände", "your Windows games, keys and saves" },
    { "PPSA99203", "PS5SX2", "PCSX2", NULL, NULL },
    { "PPSA99764", "Porpoise", "porpoise", NULL, NULL },
    { "PPSA99810", "PSBox Cloud Gaming", "psbox", NULL, NULL },
    { "PPSA99360", "PS5-ECAM", "ps5cemu", NULL, NULL },
    { "PPSA99515", "Emby5", "Emby5", NULL, NULL },
    { "PPSA99474", "Twispero", "ps5twitch", NULL, NULL },
    { "PPSA99002", "ProsperoLight", "prosperolight", NULL, NULL },
    { "PPSA90212", "Breath of the Wild", "savagewind", "Spieldateien aus deinem eigenen Dump", "game files from your own dump" },
    { "PPSA90210", "Prospero Themes", "prospero-themes", NULL, NULL },
    { "PPSA74126", "Stremio Plus", "Stremio", "deine heruntergeladenen Filme und Serien", "your downloaded movies and shows" },
    { "PPSA92878", "Breeze", "breeze", NULL, NULL },
    { "PPSA97358", "XPSemu", "xpsemu", NULL, NULL },
    { "PPSA97358", "XPSemu", "xemu", NULL, NULL },
    { "PPSA99030", "Forever Launcher", "forever", NULL, NULL },
    { "PPSA99320", "Ship of Harkinian", "soh", "Spielstände und die Spieldaten aus deinem eigenen ROM", "saves and the game data made from your own ROM" },
    { "PPSA99350", "Steam Remote Play", "steamlink-ps5", NULL, NULL },
    { "PPSA99025", "reUndr (NFS: Underground)", "nfsu1", "Spieldateien aus deinem eigenen Disc-Abbild", "game files from your own disc image" },
    { "PPSA99062", "ORION Media Center", "orion", NULL, NULL },
    { "PPSA99170", "MihawkOS", "ps5proton", "deine Windows-Umgebung mit Steam-Anmeldung und Spielen", "your Windows environment with Steam sign-in and games" },
    { "PPSA85300", "VLC", "vlc", NULL, NULL },
    { "PPSA98510", "SwanStationPS5", "SwanStationPS5", NULL, NULL },
    { "PPSA99003", "ProsperoTV", "prosperotv", NULL, NULL },
    { "PPSA99842", "Trophy Manager", "trophy-manager", "Trophäen-Sicherungen", "trophy backups" },
    { "PPSA99773", "PShare", "pshare", "deine gespeicherten Dateien", "your saved files" },
    { "PPSA99991", "YouTube Max'd", "youtube-maxd", NULL, NULL },
    { "PPSA99001", "ProsperoRadio", "prosperoradio", NULL, NULL },
    { "PPSA96024", "Chiral-FTP", "chiral-ftp", NULL, NULL },
    { "PPSA99318", "Art Swap", "artswap", NULL, NULL },
    /* NEU 1.0.9 (PS5-Test 09.11.2026 18:58): Patch Manager = ps-patch-system von illusionyy (github.com/illusionyy/ps-patch-system, nur gelesen):
     * Kachel ILNY26228 mit deeplinkUri http://127.0.0.1:23900 (laut Anleitung seine Seite), Patches in /data/user/patches (xml_orbis,
     * xml_prospero), Zwischenspeicher /data/user/patch_backup; auf da_Chris86s PS5 Kachel und Ordner in derselben Minute angelegt */
    { "ILNY26228", "Patch Manager", "user", "deine Spiel-Patches und ihre Sicherungen", "your game patches and their backups" },
    { NULL, NULL, NULL, NULL, NULL } };
const char *kern_bekannte_app(const char *ordner, const char **id, const char **wichtig)
{
    if (id) *id = NULL;
    if (wichtig) *wichtig = NULL;
    if (!ordner || kern_bereiche_106()) return NULL;
    for (int i = 0; APP_DATEN[i].id; i++) if (h_gleich(ordner, APP_DATEN[i].ordner)) {
        if (id) *id = APP_DATEN[i].id;
        if (wichtig && APP_DATEN[i].w_de) *wichtig = T(APP_DATEN[i].w_de, APP_DATEN[i].w_en);
        return APP_DATEN[i].app;
    }
    return NULL;
}
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
/* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 14): Kachel eines Jailbreak-Einstiegs als Kopf (nur für Programmdateien, siehe kopf_zaehlt) */
static bool ist_jb_kopf(const eintrag *g)
{
    if (kern_bereiche_106() || !g->Jailbreak || g->SmDatei || g->Kat != 7 || g->PfadeBasis.n == 0) return false;
    if (g->Teil && strcmp(g->Teil, "Spiel") != 0) return false;
    return kopf_kennung(g) != NULL;
}
/* NEU 1.0.8 (Befund 14): eine Versionsangabe am Ende eines Titels zählt beim Namensvergleich nicht ("WebKit Autoloader v0.4.0" ->
 * "WebKit Autoloader", "Web File Manager 1.13-rc2" -> "Web File Manager"). Nur eindeutige Versionen ("v" mit Ziffer oder eine Zahl mit
 * Punkt) - "Train Sim World 6" bleibt, wie er ist. */
static const char *titel_ohne_version(kern *k, const char *t)
{
    const char *r = h_trim(A, h_r(A, t, "(?i)[\\s_\\-]*[\\(\\[]?(v\\d[\\w.+\\-]*|\\d+\\.\\d+[\\w.+\\-]*)[\\)\\]]?\\s*$", ""));
    return r[0] ? r : t;
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
/* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 14): auch Kacheln eines Jailbreak-Einstiegs (WebKit-Autoloader WKAL00001) sind Köpfe - aber nur
 * für Programmdateien (ELF/BIN, z. B. webkit-autoloader-installer_v0.6.2.elf) und feste Zuordnungen (tool_alias: ps5_autoloader). Andere
 * Daten kommen nie per Namensvergleich in den Bereich Jailbreak. kjb[t]: Kopf t ist so eine Kachel; payload: der Eintrag ist eine
 * Programmdatei. */
static const bool *kopf_jb; static bool kopf_payload;
static bool kopf_zaehlt(const char **kid, int t, const char *dk)
{
    if (!kid[t]) return false;
    if (!kopf_jb || !kopf_jb[t] || kopf_payload) return true;
    const char *ak = tool_alias(kid[t]);
    return ak[0] && !strcmp(ak, dk);
}
static int kopf_fuer_namen(pliste *liste, int n, const char **kid, const char **ktk, const char **ktt, const char *dk, const char *stamm, int kat)
{
    int beste = 4, tr = -1, anz = 0;
    for (int t = 0; t < n; t++) {
        if (!kopf_zaehlt(kid, t, dk)) continue;
        int st = name_stufe(ktk[t], ktt[t], tool_alias(kid[t]), dk, stamm);
        if (!st) continue;
        if (st < beste) { beste = st; tr = t; anz = 1; }
        else if (st == beste) anz++;
    }
    if (anz <= 1) return tr;
    /* Codex-Prüfung 56: zwei verschiedene Apps mit gleichem Namen im selben Bereich ("FTP Server") - nicht raten */
    int erst = -1, passend = -1, np = 0, will = kat == 1 ? 1 : 6;
    for (int t = 0; t < n; t++) {
        if (!kopf_zaehlt(kid, t, dk) || name_stufe(ktk[t], ktt[t], tool_alias(kid[t]), dk, stamm) != beste) continue;
        if (erst < 0) erst = t;
        else if (strcmp(ktt[t], ktt[erst]) != 0) return -2;
        if (((eintrag *)liste->e[t])->Kat == will) { passend = t; np++; }
    }
    return np == 1 ? passend : -2;
}
/* Regel 2 (Name) der Zuordnung für den Besitzer-Teil oc eines Pfads: eine Programmdatei ohne Version und Endung, eine Datei ohne Endung, sonst
 * der Name. NEU 1.0.7 (PS5-Test 03.11.2026): "ps5-" vorne sagt nur, für welche Konsole es ist (ps5-web-file-manager_v1.11b.elf -> Kachel
 * "Web File Manager") - zählt nur, wenn mit dem vollen Namen gar nichts passt. Codex-Prüfung 103 (P2): als eigene Funktion, damit eine
 * PKG-Datei ohne lesbare Nummer (kachel_bloecke) genau dieselbe Regel bekommt wie ihr Ordner. Setzt kopf_payload. */
static int kopf_fuer_besitzer_namen(kern *k, pliste *liste, int n, const char **kid, const char **ktk, const char **ktt, const char *oc, int kat)
{
    bool datei = name_ist_datei(oc);
    htreffer pm = h_mt(A, oc, "^(.+?)(?:[-_ ]v?\\d[\\w.\\-]*)?\\.(?:elf|bin)$");
    bool payload = pm.ok;
    const char *stamm = payload ? pm.gr[1] : (datei ? h_r(A, oc, "\\.[A-Za-z][A-Za-z0-9]{0,7}$", "") : oc);
    const char *dk = tool_schluessel(k, stamm);
    kopf_payload = payload;
    int t1 = kopf_fuer_namen(liste, n, kid, ktk, ktt, dk, stamm, kat);
    if (t1 == -1) { htreffer mp = h_mt(A, stamm, "^ps5[-_ ](.+)$"); if (mp.ok) t1 = kopf_fuer_namen(liste, n, kid, ktk, ktt, tool_schluessel(k, mp.gr[1]), mp.gr[1], kat); }
    return t1;
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
        if (zu[d] >= 0 || kid[d] || f->Jailbreak || f->SmDatei || f->Teil || f->Kachel || f->Gesperrt || (f->PfadeBasis.n != 1 && !f->ToolOrdner)) continue;
        if (f->Kat != 5 && f->Kat != 6) continue;
        const char *p = f->ToolOrdner ? f->ToolOrdner : f->PfadeBasis.e[0], *x = NULL;   /* Codex-Prüfung 85: Rest ohne PKG-Dateien wie der Ordner */
        bool pk = false;
        htreffer md = h_mt(A, p, "^/user/data/(homebrew/)?([^/]+)$");
        htreffer mr = h_mt(A, p, "^/mnt/(?:ext\\d+|usb\\d+)/(\\.[^/]+)$");
        if (md.ok) {
            const char *y = md.gr[2];
            if (h_m(y, "^[A-Z]{4}\\d{5}") || h_gleich(y, "homebrew") || (!md.gr[1][0] && (tool_bekannt(k, y) || kern_bekannte_app(y, NULL, NULL)))) continue;   /* 1.0.9 (#146): auch bekannte App-Ordner */
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
static bool kopie_echt(kern *k, const eintrag *f, const char *id);
/* Codex-Prüfung 83/85: eine PKG-Datei hängt nur an einem Spiel, das wirklich installiert ist - nicht an einem leeren Rest /user/app/<Nummer>
 * und nicht an einer toten ShadowMount-Kachel (Spieldatei fehlt, Zeile "… - ShadowMount: …") */
static bool pkg_kopf_installiert(kern *k, const eintrag *g, const char *id)
{
    if (g->Quelle && g->Quelle[0]) return true;                                            /* ShadowMount mit Spieldatei */
    if (!g->Teil && g->TextBasis && strstr(g->TextBasis, " - ShadowMount: ")) return false;   /* tote ShadowMount-Kachel */
    if (!ist_teil(g, "Spiel")) {   /* Codex-Prüfung 90: eine Tool-/App-Kachel mit LEEREM App-Ordner ist kein installiertes Programm */
        for (int j = 0; j < g->PfadeBasis.n; j++) {
            const char *p = g->PfadeBasis.e[j], *b = strrchr(p, '/');
            if (!b || strcmp(b + 1, id) != 0 || !h_m(p, "(^|/)user/app/[^/]+$")) continue;
            dliste *l = s_liste(k->S, p, false);
            return !l || l->n > 0;                         /* nicht lesbar: wie bisher installiert */
        }
        return true;
    }
    return kopie_echt(k, g, id);
}
/* Die Teile des Kopfs i hinter ihm: nach Ort (intern, M.2, USB-Speicher, USB-Stick), darin in Fundreihenfolge - sonst stünde z. B. eine ELF
 * vom Stick (vorher "Apps") vor den Daten auf der internen SSD (vorher "Sonstiges"), weil die Liste vorher nach Bereichen sortiert war.
 * NEU 1.0.7 (03.11.2026): unter einer Trophäen-Liste zuerst die weiteren Listen, dann der Fortschritt (Durchgang 0 = Listen).
 * NEU 1.0.8 (Orbit Store, Codex-Prüfung 108): eine zusammengeführte ShadowMount-Kachel (sm_mit) hängt nur zur Anzeige am Block (Begleit),
 * direkt hinter ihr ihre Spieldatei-Zeile (zu = -2) und ihre eigenen Teile - die hängen weiter an IHR (ToolKachel), wie bisher. */
static void bloecke_teile(kern *k, pliste *neu, pliste *liste, int n, int *zu, const bool *tr_liste, const bool *sm_mit, int i)
{
    for (int dg = 0; dg < 2; dg++)
    for (int o = -1; o <= ORT_STICK; o++)
    for (int d = 0; d < n; d++) if (zu[d] == i) {
        eintrag *f = liste->e[d], *h = liste->e[i];
        bool ort_ok = (f->Ort >= ORT_INTERN && f->Ort <= ORT_STICK) ? f->Ort == o : o == -1;
        if (!ort_ok) continue;
        if ((dg == 0) != tr_liste[d]) continue;
        f->Text = VERB(A, "   – ", f->Text); f->TextBasis = VERB(A, "   – ", f->TextBasis);
        if (sm_mit[d]) f->Begleit = h; else f->ToolKachel = h;
        /* unter ihre Kachel - Trophäen bleiben, wo sie sind; NEU 1.0.8: eine PKG-Datei immer zu ihrem Spiel (auch zur zweiten Kopie ohne Kachel).
         * Codex-Prüfung 108 (P3): wurde der Eintrag schon vorher verschoben (bereiche_107), bleibt seine Herkunft */
        if (h->KachelId || (f->PkgArt && f->Kat != h->Kat)) { if (!f->KatHer) f->KatHer = f->Kat; f->Kat = h->Kat; }
        pl_add(A, neu, f);
        if (!sm_mit[d]) continue;
        for (int s = 0; s < n; s++) {
            eintrag *sd = liste->e[s];
            if (zu[s] != -2 || sd->SmKachel != f) continue;
            if (sd->Kat != f->Kat) { if (!sd->KatHer) sd->KatHer = sd->Kat; sd->Kat = f->Kat; }
            pl_add(A, neu, sd);
        }
        bloecke_teile(k, neu, liste, n, zu, tr_liste, sm_mit, d);   /* ihre eigenen Teile (eine Ebene tiefer gibt es nicht) */
    }
}
static void kachel_bloecke(kern *k, pliste *liste)
{
    int n = liste->n;
    int *zu = a_hole(A, sizeof(int) * (size_t)(n > 0 ? n : 1));
    for (int i = 0; i < n; i++) zu[i] = -1;
    signed char *echt = a_hole(A, (size_t)(n > 0 ? n : 1));   /* Codex-Prüfung 83: Kopf wirklich installiert? 0 = noch nicht geprüft, 1 ja, 2 nein */
    /* Köpfe: Kennung, Name, Alias */
    const char **kid = a_hole(A, sizeof(char *) * (size_t)(n > 0 ? n : 1)), **ktk = a_hole(A, sizeof(char *) * (size_t)(n > 0 ? n : 1));
    const char **ktt = a_hole(A, sizeof(char *) * (size_t)(n > 0 ? n : 1)), **kid_ohne_jb = a_hole(A, sizeof(char *) * (size_t)(n > 0 ? n : 1));
    bool *kjb = a_hole(A, sizeof(bool) * (size_t)(n > 0 ? n : 1));
    for (int i = 0; i < n; i++) {
        eintrag *g = liste->e[i]; kid[i] = kid_ohne_jb[i] = NULL; ktk[i] = ""; ktt[i] = ""; kjb[i] = false;
        if (!ist_block_kopf(g) && !(kjb[i] = ist_jb_kopf(g))) continue;
        kid[i] = kopf_kennung(g);
        if (!kjb[i]) kid_ohne_jb[i] = kid[i];
        ktt[i] = kern_bereiche_106() ? kopf_titel(k, g, kid[i]) : titel_ohne_version(k, kopf_titel(k, g, kid[i]));   /* NEU 1.0.8 (Befund 14) */
        ktk[i] = tool_schluessel(k, ktt[i]);
    }
    kopf_jb = kjb;
    for (int d = 0; d < n; d++) {
        eintrag *f = liste->e[d];
        if (kid[d] || f->Jailbreak || f->SmDatei || f->Teil || f->Kachel || f->PfadeBasis.n == 0) continue;
        if (f->Kat != 1 && f->Kat != 5 && f->Kat != 6) continue;
        /* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 12): eine PKG-Datei gehört zu dem Spiel bzw. der App mit ihrer Spielnummer (aus dem Kopf
         * der Datei, sonst aus dem Namen) - egal in welchem Ordner und auf welchem Speicher sie liegt. Ist es zweimal installiert (intern
         * und M.2), zur ersten Kopie. Nicht installiert: eigener Block (pkg_ohne_spiel). */
        if (f->PkgArt) {
            /* Codex-Prüfung 83 (P2): zuerst zu einem Spiel, das wirklich installiert ist (App-Ordner mit app.pkg bzw. eboot.bin, ShadowMount
             * mit Spieldatei). NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86 08:46: "bei Spiele sind jetzt wieder Doppelteinträge ... einen
             * Reiter mit der PKG und weiter unten die Spieledateien und die Kachel"): ist von dem Spiel nur noch ein Rest da (Kachel mit leerem
             * App-Ordner wie L.A. Noire, tote ShadowMount-Kachel, Tool-Kachel mit leerem App-Ordner), steht die PKG-Datei trotzdem in SEINEM
             * Block - mit eigenem Text (PkgSpielFehlt). Einen Block "· nicht installiert" gibt es nur noch, wenn von dem Spiel gar nichts da ist. */
            if (f->PkgId) {
                int rest = -1;
                for (int t = 0; t < n; t++) if (kid[t] && !strcmp(kid[t], f->PkgId)) {
                    if (!echt[t]) echt[t] = pkg_kopf_installiert(k, liste->e[t], kid[t]) ? 1 : 2;
                    if (echt[t] == 1) { zu[d] = t; break; }
                    if (rest < 0) rest = t;
                }
                if (zu[d] < 0 && rest >= 0) { zu[d] = rest; f->PkgSpielFehlt = true; }
            }
            continue;
        }
        /* NEU 1.0.9 (#145): ein Teil einer App aus den Ordnern von ProsperoStore (merke_store) gehört nur zu der Kachel mit seiner Kennung -
         * nie nach Name oder Programm (sonst stünde z. B. die aufgehobene Fassung einer gelöschten App unter ProsperoStore und ginge nur mit
         * ihm); ohne eindeutige Kachel bleibt er für sich */
        if (f->StoreTeil) {
            htreffer ms = h_mt(A, f->PfadeBasis.e[0], "(?-i)/prosperostore/(?:previous|backup|trash|staging|receipts)/([A-Z]{4}[0-9]{5})(?:\\.zip|\\.json)?(?:/|$)");
            int t1 = -1, a1 = 0;
            for (int t = 0; ms.ok && t < n; t++) if (kid_ohne_jb[t] && !strcmp(kid_ohne_jb[t], ms.gr[1])) { t1 = t; a1++; }
            if (a1 == 1) zu[d] = t1;
            continue;
        }
        /* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86 08:46: "unter Apps gibt es Orbit ... da sind auch zwei Reiter"): der Entpack-Ordner von
         * Orbit Store (".orbit-extract-<Zahl>", auf da_Chris86s M.2 51,3 GB) enthält den Ordner des Spiels, das Orbit gerade entpackt
         * ("LIZARD Marvel's Spider-Man 2 01.004.003 PPSA03016"). Tragen alle Einträge darin dieselbe Spielnummer, gehört der Entpack-Ordner
         * zu diesem Spiel - wie die halb geladene .rar.part daneben - statt als eigene Zeile unter Apps zu stehen. Nur bei genau einem
         * passenden Spiel; ist der Ordner nicht lesbar, leer oder gemischt, bleibt er, wo er war (nicht raten).
         * Codex-Prüfung 102 (P3): nur ein echter Ordner - bei einer Verknüpfung entschiede sonst der Inhalt ihres Ziels. */
        if (f->PfadeBasis.n == 1 && h_m(h_blatt(A, f->PfadeBasis.e[0]), "^\\.orbit-extract-[0-9A-Za-z]+$")) {
            const char *op = f->PfadeBasis.e[0], *ob = h_blatt(A, op);
            dliste *el = NULL, *ol = NULL;
            VERSUCHE { el = s_liste(k->S, h_eltern(A, op), false); ol = s_liste(k->S, op, false); }
            FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); el = ol = NULL; } ENDE_VERSUCHE;
            bool echt_ordner = false;
            for (int j = 0; el && j < el->n; j++) if (!strcmp(el->e[j].name, ob)) { echt_ordner = el->e[j].ist_ordner && !el->e[j].ist_link; break; }
            const char *oid = NULL; bool eins = echt_ordner && ol != NULL;
            for (int j = 0; ol && j < ol->n && eins; j++) {
                if (!strcmp(ol->e[j].name, ".") || !strcmp(ol->e[j].name, "..")) continue;
                htreffer mo = h_mt(A, ol->e[j].name, "(?-i)(?:^|[^A-Za-z0-9])([A-Z]{4}\\d{5})(?:[^0-9]|$)");
                if (!mo.ok || ol->e[j].ist_link) eins = false;
                else if (!oid) oid = mo.gr[1];
                else if (strcmp(oid, mo.gr[1]) != 0) eins = false;
            }
            if (eins && oid) {
                int t1 = -1, a1 = 0;
                for (int t = 0; t < n; t++) if (kid_ohne_jb[t] && !strcmp(kid_ohne_jb[t], oid)) { t1 = t; a1++; }
                if (a1 == 1) { zu[d] = t1; continue; }
            }
        }
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
         * App-Ordner mit einer Nummer im Namen (z. B. playden/cache-CUSA31001) gehen den normalen Weg über ihren Besitzer.
         * Codex-Prüfung 86: der Rest eines Tool-Ordners (ToolOrdner) steht für den Ordner selbst - nie nach dem Namen einer Datei oder eines
         * Unterordners darin (Schritte 0 und 0b), sondern über seinen Besitzer (/user/data/<Tool>) wie der ganze Ordner. */
        if (!f->ToolOrdner) {
            const char *bid = NULL; bool alle = true;
            for (int j = 0; j < f->PfadeBasis.n && alle; j++) {
                const char *bl = h_blatt(A, f->PfadeBasis.e[j]);
                htreffer mb = h_mt(A, bl, "(?-i)(?:^|[^A-Za-z0-9])([A-Z]{4}\\d{5})(?:[^0-9]|$)");
                if (!mb.ok || !(f->Kat == 1 || h_m(bl, "(?i)\\.(pkg|iso|" ABBILD_ENDUNGEN ")$"))) { alle = false; continue; }
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
        if (f->PfadeBasis.n == 1 && f->Kat != 1 && !f->ToolOrdner) {
            const char *bl = h_blatt(A, f->PfadeBasis.e[0]);
            if (strcmp(bl, oc) != 0) {
                htreffer pb = h_mt(A, bl, "^(.+?)(?:[-_ ]v?\\d[\\w.\\-]*)?\\.(?:elf|bin)$");
                const char *sb = pb.ok ? pb.gr[1] : (name_ist_datei(bl) ? h_r(A, bl, "\\.[A-Za-z][A-Za-z0-9]{0,7}$", "") : bl);
                const char *kb = tool_schluessel(k, sb);
                int t1 = -1, a1 = 0;
                kopf_payload = pb.ok;
                for (int t = 0; t < n; t++) if (kopf_zaehlt(kid, t, kb) && name_stufe(ktk[t], ktt[t], tool_alias(kid[t]), kb, sb) == 1) { t1 = t; a1++; }
                if (a1 == 1) { zu[d] = t1; continue; }
            }
        }
        /* 1. Kennung im Namen */
        htreffer mi = h_mt(A, oc, "(?-i)(?:^|[^A-Za-z0-9])([A-Z]{4}\\d{5})(?:[^0-9]|$)");
        if (mi.ok) {
            int t1 = -1, a1 = 0;
            const char **kk = h_m(oc, "(?i)\\.(elf|bin)$") ? kid : kid_ohne_jb;   /* NEU 1.0.8: Jailbreak-Kachel nur für Programmdateien */
            for (int t = 0; t < n; t++) if (kk[t] && !strcmp(kk[t], mi.gr[1])) { t1 = t; a1++; }
            if (a1 == 1) zu[d] = t1;
            continue;                              /* eine Spielnummer, die zu keiner (oder mehreren) Kachel(n) passt: nicht raten */
        }
        if (f->Kat == 1) continue;                 /* Spiele nur über die Kennung */
        /* 1b. NEU 1.0.9 (#146): Daten-Ordner einer bekannten App unter /data (APP_DATEN, aus ihrem Quellcode) - fest zur Kachel mit dieser
         * Kennung, vor dem Namensvergleich; ist die App nicht (eindeutig) da, gelten die übrigen Regeln wie bisher */
        {   /* Codex-Prüfung 137 (P2): nur /user/data/<Ordner> selbst (so ist er belegt) - nie ein gleichnamiger Ordner in homebrew */
            htreffer mb = h_mt(A, f->ToolOrdner ? f->ToolOrdner : f->PfadeBasis.e[0], "^/user/data/([^/]+)(?:/|$)");
            const char *aid = NULL;
            if (mb.ok && !h_gleich(mb.gr[1], "homebrew") && h_gleich(mb.gr[1], oc) && kern_bekannte_app(oc, &aid, NULL) && aid) {
                int ta = -1, aa = 0;
                for (int t = 0; t < n; t++) if (kid_ohne_jb[t] && !strcmp(kid_ohne_jb[t], aid)) { ta = t; aa++; }
                if (aa == 1) { zu[d] = ta; continue; }
            }
        }
        /* 2. Name (kopf_fuer_besitzer_namen, auch "ps5-" vorne) */
        int t1 = kopf_fuer_besitzer_namen(k, liste, n, kid, ktk, ktt, oc, f->Kat);
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
    kopf_jb = NULL; kopf_payload = false;
    beleg_aus_programmen(k, liste, n, zu, kid_ohne_jb);   /* NEU 1.0.8: Belege nur aus den Programmen von App- und Tool-Kacheln (wie bisher) */
    /* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86 08:46: "Spectrum Library ist unter Sonstiges auch ein Eintrag drin, der eigentlich unter Apps
     * gehört"): eine PKG-Datei, deren Spielnummer sich nicht lesen ließ (SpectrumLibrary_1.4.8.pkg ist im PS5-Format), steht bei dem Tool, in
     * dessen Daten-Ordner sie liegt (/user/data/SpectrumLibrary/...) - der Rest dieses Ordners ("ohne PKG-Dateien", ToolOrdner) steht schon
     * dort. Nur wenn genau ein Ordner am tiefsten passt (nicht raten).
     * Codex-Prüfung 102 (P2): liegt im Ordner außer PKG-Dateien nichts, gibt es keinen Rest mit ToolOrdner - dann gilt der Ordner, dem der Pfad
     * gehört (/user/data/<Tool>, /mnt/<Speicher>/<Ordner>, besitzer_teil), wie der ganze Ordner: Spielnummer im Namen, sonst der Name (nur
     * bei genau einem passenden Kopf; Jailbreak-Köpfe wie im Hauptdurchlauf nur über feste Zuordnungen). */
    kopf_jb = kjb; kopf_payload = false;
    for (int d = 0; d < n; d++) {
        eintrag *f = liste->e[d];
        if (zu[d] >= 0 || kid[d] || !f->PkgArt || f->PkgId || f->Gesperrt || f->Jailbreak || f->SmDatei || f->Teil || f->Kachel || f->PfadeBasis.n != 1) continue;
        if (f->Kat != 1 && f->Kat != 5 && f->Kat != 6) continue;
        const char *p = f->PfadeBasis.e[0];
        int best = -1; size_t bl = 0; bool zwei = false;
        for (int e = 0; e < n; e++) {
            eintrag *g = liste->e[e];
            int ziel = zu[e] >= 0 ? zu[e] : kid[e] ? e : -1;
            if (e == d || ziel < 0 || !g->ToolOrdner) continue;
            size_t lo = strlen(g->ToolOrdner);
            if (strncmp(p, g->ToolOrdner, lo) != 0 || p[lo] != '/') continue;
            if (lo > bl) { best = ziel; bl = lo; zwei = false; } else if (lo == bl && ziel != best) zwei = true;
        }
        if (best >= 0 && !zwei) { zu[d] = best; continue; }
        if (zwei) continue;
        const char *oc = besitzer_teil(k, p);
        if (!oc || !strstr(p, VERB(A, "/", oc, "/"))) continue;   /* nur IN einem Ordner, nie die Datei selbst */
        htreffer mi = h_mt(A, oc, "(?-i)(?:^|[^A-Za-z0-9])([A-Z]{4}\\d{5})(?:[^0-9]|$)");
        int t1 = -1, a1 = 0;
        if (mi.ok) {
            for (int t = 0; t < n; t++) if (kid_ohne_jb[t] && !strcmp(kid_ohne_jb[t], mi.gr[1])) { t1 = t; a1++; }
            if (a1 != 1) t1 = -1;
        } else if (f->Kat != 1) t1 = kopf_fuer_besitzer_namen(k, liste, n, kid, ktk, ktt, oc, f->Kat);   /* Codex-Prüfung 103: dieselbe Regel 2 */
        if (t1 >= 0) zu[d] = t1;
    }
    kopf_jb = NULL; kopf_payload = false;
    /* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86: Orbit Store "in einem Block unter Apps" - "es soll ja für alle zukünftigen Apps und Spiele
     * genauso laufen"): DASSELBE Programm mit zwei Kacheln - einer App- oder Tool-Kachel unter Apps (Orbit Store: ORBT00001 von
     * orbit_store.elf) und einer über ShadowMount eingetragenen App (PPSA99177 aus PPSA99177.ffpkg, stand unter Spiele) - steht als EIN Block
     * unter der Kachel unter Apps. Dasselbe Programm heißt: GENAU derselbe Name (ohne Versionsangabe, wie bei der Zuordnung von Daten), unter
     * Apps genau EINE Kachel und über ShadowMount genau EINE Kachel dieses Namens - sonst nicht raten. Die ShadowMount-Kachel steht im Block
     * mit ihrer Spieldatei-Zeile und allem, was unter ihr stand (bloecke_teile). Codex-Prüfung 108 (P1): nur Anzeige - die ShadowMount-
     * Kachel hängt über Begleit am Block, nicht über ToolKachel; ihre Teile hängen weiter an IHR. Beim Löschen ändert sich nichts. */
    bool *sm_mit = a_hole(A, sizeof(bool) * (size_t)(n > 0 ? n : 1));
    for (int p = 0; p < n; p++) sm_mit[p] = false;
    for (int p = 0; p < n; p++) {
        eintrag *gp = liste->e[p];
        if (kern_bereiche_106() || !kid[p] || kjb[p] || zu[p] >= 0 || !gp->Kachel || !ktt[p][0] || !strstr(gp->TextBasis ? gp->TextBasis : "", "ShadowMount")) continue;
        int o = -1, ao = 0, ap = 0;
        for (int t = 0; t < n; t++) {
            if (!kid[t] || kjb[t] || strcmp(ktt[t], ktt[p]) != 0) continue;
            eintrag *gt = liste->e[t];
            if (strstr(gt->TextBasis ? gt->TextBasis : "", "ShadowMount")) ap++;
            else if ((gt->Kat == 6 || gt->KatHer == 6) && gt->KachelId && gt->KachelId[0]) { o = t; ao++; }   /* 1.0.8: KatHer 6 = Tool-Kachel unter Spiele (bereiche_107) */
        }
        if (ao != 1 || ap != 1 || zu[o] >= 0) continue;
        zu[p] = o; sm_mit[p] = true;
        for (int d = 0; d < n; d++) {
            eintrag *f = liste->e[d];
            if (f->SmDatei && f->SmKachel == gp && zu[d] < 0) zu[d] = -2;   /* kommt direkt hinter ihre Kachel (bloecke_teile) */
        }
    }
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
            /* Codex-Prüfung 62: eine Liste kann an mehreren Orten liegen (trophy/conf, trophy2/nobackup/conf, np_uds) - alle Pfade dieselbe Nummer.
             * Codex-Prüfung 120 (P3): eine Kopie mit Endung (NPWR09412_00_BK, von der PS5 beim Neuinstallieren angelegt) gehört in den Block
             * ihrer Liste - Schlüssel ist die Nummer ohne Endung, wie bei Name und Listennummer (npwr_basis) */
            const char *npm = kern_bereiche_106() ? "(?-i)/(NPWR\\d{5}_\\d{2})$" : "(?-i)/(NPWR\\d{5}_\\d{2})(?:_[A-Za-z0-9]{1,16})?$";
            htreffer mn = h_mt(A, f->PfadeBasis.e[0], npm);
            bool eine = mn.ok;
            for (int j = 1; j < f->PfadeBasis.n && eine; j++) { htreffer mj = h_mt(A, f->PfadeBasis.e[j], npm); eine = mj.ok && !strcmp(mj.gr[1], mn.gr[1]); }
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
        if (zu[i] >= 0 || zu[i] == -2) continue;   /* -2: Spieldatei-Zeile einer zusammengeführten ShadowMount-Kachel (kommt mit ihr) */
        pl_add(A, &neu, liste->e[i]);
        bloecke_teile(k, &neu, liste, n, zu, tr_liste, sm_mit, i);
    }
    *liste = neu;
}
/* NEU 1.0.8 (Arbeitsplan 2.3, Vorschlag aus PLAN_1.0.7, da_Chris86 04.11.2026 14:30): Tools ohne Kachel. Auf da_Chris86s PS5 liegen
 * nanoDNS_0.4.elf und zftpd-ps5-v1.6.0.elf auf dem USB-Stick (bis 1.0.7 unter "Sonstiges"), ihre Daten unter /data/nanodns und /data/zftpd
 * ("Apps"). Jetzt steht die Payload-Datei (ELF/BIN) unter dem Daten-Ordner desselben Tools, beide unter "Apps" - ein Block "nanoDNS",
 * "ohne Kachel". Zugeordnet wird über den Namen wie bei den Kacheln (name_stufe): gleich (nanoDNS = nanodns), oder die Datei beginnt an
 * einer Grenze mit dem Ordnernamen (ab 5 Zeichen: zftpd-ps5 zu zftpd). Nur wenn in der besten Stufe genau EIN Daten-Ordner passt (nicht
 * raten). Ein Daten-Ordner unter "Sonstiges" kommt mit, wenn eine Payload-Datei genau zu ihm passt - dann ist er die Ablage dieses Tools.
 * Nur Anzeige (Begleit): anders als unter einer Kachel hängt beim Löschen nichts daran, jede Zeile behält ihren eigenen Haken. */
/* Codex-Prüfung 73: nur in einer Richtung - die Datei heißt wie der Ordner oder beginnt (an einer Grenze) mit seinem Namen
 * (zftpd-ps5-v1.6.0.elf zu zftpd), nie umgekehrt (Werkbank.elf nicht zu Werkbank-Archiv). Ein Ordner unter "Sonstiges" (kein bekanntes
 * Tool) nur bei GLEICHEM Namen - sonst käme fremder Inhalt als "Einstellungen" nach "Apps". */
static int ohne_kachel_ordner(kern *k, int n, const char **dk, const char **dn, const bool *bekannt, const char *stamm)
{
    const char *ek = tool_schluessel(k, stamm);
    int beste = 3, tr = -1, anz = 0;
    for (int t = 0; t < n; t++) {
        if (!dk[t]) continue;
        int st = !strcmp(dk[t], ek) ? 1 : (bekannt[t] && strlen(ek) > strlen(dk[t]) && strlen(dk[t]) >= 5 && beginnt_mit_schluessel(stamm, dk[t], false)) ? 2 : 0;
        if (!st) continue;
        if (st < beste) { beste = st; tr = t; anz = 1; }
        else if (st == beste) anz++;
    }
    return anz == 1 ? tr : anz > 1 ? -2 : -1;
}
static void tools_ohne_kachel(kern *k, pliste *liste)
{
    int n = liste->n;
    if (n == 0) return;
    const char **dk = a_hole(A, sizeof(char *) * (size_t)n), **dn = a_hole(A, sizeof(char *) * (size_t)n), **st = a_hole(A, sizeof(char *) * (size_t)n);
    bool *bekannt = a_hole(A, sizeof(bool) * (size_t)n);
    int *zu = a_hole(A, sizeof(int) * (size_t)n);
    for (int i = 0; i < n; i++) {   /* Daten-Ordner direkt unter /user/data, die zu keiner Kachel gehören */
        eintrag *f = liste->e[i];
        dk[i] = dn[i] = st[i] = NULL; zu[i] = -1;
        if ((f->Kat != 5 && f->Kat != 6) || f->ToolKachel || f->KachelId || f->Kachel || f->Jailbreak || f->Gesperrt || f->Teil || f->SmDatei || (f->PfadeBasis.n != 1 && !f->ToolOrdner)) continue;
        htreffer m = h_mt(A, f->ToolOrdner ? f->ToolOrdner : f->PfadeBasis.e[0], "^/user/data/([^/]+)$");   /* Codex-Prüfung 85: Rest wie der Ordner */
        if (!m.ok || name_ist_datei(m.gr[1])) continue;
        const char *s = tool_schluessel(k, m.gr[1]);
        if (strlen(s) < 4) continue;                   /* zu kurz, um es einem Programm sicher zuzuordnen */
        dn[i] = m.gr[1]; dk[i] = s; bekannt[i] = f->Kat == 6;
    }
    bool je = false;
    for (int d = 0; d < n; d++) {   /* Payload-Dateien (ELF/BIN), die zu keiner Kachel gehören */
        eintrag *f = liste->e[d];
        if (dk[d] || (f->Kat != 5 && f->Kat != 6) || f->ToolKachel || f->KachelId || f->Kachel || f->Jailbreak || f->Gesperrt || f->Teil || f->SmDatei || f->PfadeBasis.n != 1) continue;
        const char *bl = h_blatt(A, f->PfadeBasis.e[0]);
        htreffer pm = h_mt(A, bl, "^(.+?)(?:[-_ ]v?\\d[\\w.\\-]*)?\\.(?:elf|bin)$");
        if (!pm.ok) continue;
        const char *stamm = pm.gr[1];
        int t = ohne_kachel_ordner(k, n, dk, dn, bekannt, stamm);
        if (t == -1) { htreffer mp = h_mt(A, stamm, "^ps5[-_ ](.+)$"); if (mp.ok) { stamm = mp.gr[1]; t = ohne_kachel_ordner(k, n, dk, dn, bekannt, stamm); } }
        if (t >= 0) { zu[d] = t; st[d] = stamm; je = true; }
    }
    if (!je) return;
    pliste neu = {0};
    for (int i = 0; i < n; i++) {
        if (zu[i] >= 0) continue;
        eintrag *h = liste->e[i];
        pl_add(A, &neu, h);
        bool kopf = false;
        for (int o = -1; o <= ORT_STICK; o++)              /* wie kachel_bloecke: nach Ort, darin in Fundreihenfolge */
            for (int d = 0; d < n; d++) if (zu[d] == i) {
                eintrag *f = liste->e[d];
                bool ort_ok = (f->Ort >= ORT_INTERN && f->Ort <= ORT_STICK) ? f->Ort == o : o == -1;
                if (!ort_ok) continue;
                if (!kopf) {
                    kopf = true;
                    h->OhneKachel = true;
                    /* Name des Blocks: heißt die Datei genau wie der Ordner, ihre Schreibweise ("nanoDNS" statt "nanodns"), sonst der Ordner */
                    h->BlockName = !strcmp(tool_schluessel(k, st[d]), dk[i]) ? a_kopie(A, st[d]) : dn[i];
                    if (h->Kat != 6) { h->KatHer = h->Kat; h->Kat = 6; }
                }
                f->Text = VERB(A, "   – ", f->Text); f->TextBasis = VERB(A, "   – ", f->TextBasis);
                f->Begleit = h;
                if (f->Kat != 6) { f->KatHer = f->Kat; f->Kat = 6; }
                pl_add(A, &neu, f);
            }
    }
    *liste = neu;
}
/* Begleit-Blöcke (pkg_ohne_spiel, payload_gruppen): Kopf zuerst, darunter seine Teile nach Rang (rang(), 0 zuerst), darin nach Ort und
 * in Fundreihenfolge. zu[d] = Index des Kopfs von d (-1 = selbst Kopf oder für sich). */
static void begleit_ordnen(kern *k, pliste *liste, const int *zu, int (*rang)(const eintrag *), int kat_fest)
{
    int n = liste->n;
    pliste neu = {0};
    for (int i = 0; i < n; i++) {
        if (zu[i] >= 0) continue;
        eintrag *h = liste->e[i];
        pl_add(A, &neu, h);
        for (int r = 0; r < 3; r++) for (int o = -1; o <= ORT_STICK; o++) for (int d = 0; d < n; d++) if (zu[d] == i) {
            eintrag *f = liste->e[d];
            bool ort_ok = (f->Ort >= ORT_INTERN && f->Ort <= ORT_STICK) ? f->Ort == o : o == -1;
            if (!ort_ok || (rang ? rang(f) : 0) != r) continue;
            f->Text = VERB(A, "   – ", f->Text); f->TextBasis = VERB(A, "   – ", f->TextBasis);
            f->Begleit = h;
            int kat = kat_fest ? kat_fest : h->Kat;
            if (f->Kat != kat) { f->KatHer = f->Kat; f->Kat = kat; }
            pl_add(A, &neu, f);
        }
    }
    *liste = neu;
}
/* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 12, da_Chris86: "Spiel nicht installiert und halt auch zum Aufklappen unten drunter Update und
 * DLCs"): die PKG-Dateien eines Spiels, das gerade nicht installiert ist, stehen als EIN Block "<Spiel> · nicht installiert" unter
 * "Spiele" (eine App unter "Apps"): oben die Installationsdatei, darunter Updates, dann Zusatzinhalte, je nach Speicher. Nur Anzeige
 * (Begleit) - jede Zeile behält ihren eigenen Haken. Ohne Spielnummer bleibt die Datei unter "Sonstiges". */
static int pkg_rang(const eintrag *f) { return f->PkgArt == PKG_SPIEL || f->PkgArt == PKG_APP ? 0 : f->PkgArt == PKG_UPDATE ? 1 : 2; }
static void pkg_ohne_spiel(kern *k, pliste *liste)
{
    int n = liste->n, m;
    if (n == 0) return;
    const char **sch = a_hole(A, sizeof(char *) * (size_t)n);
    for (int i = 0; i < n; i++) {
        eintrag *f = liste->e[i]; sch[i] = NULL;
        if (f->PkgArt && f->PkgId && !f->ToolKachel && !f->Begleit && !f->Gesperrt) sch[i] = f->PkgId;
    }
    int *ix = pkg_sortiert(k, sch, n, &m), *zu = a_hole(A, sizeof(int) * (size_t)n);
    for (int i = 0; i < n; i++) zu[i] = -1;
    if (m == 0) return;
    for (int a = 0; a < m; ) {
        int b = a; while (b < m && !strcmp(sch[ix[a]], sch[ix[b]])) b++;
        int h = ix[a];   /* Kopf: die Installationsdatei (bzw. das erste Update), bei mehreren die zuerst gefundene */
        for (int j = a; j < b; j++) { eintrag *g = liste->e[ix[j]], *x = liste->e[h]; if (pkg_rang(g) < pkg_rang(x) || (pkg_rang(g) == pkg_rang(x) && ix[j] < h)) h = ix[j]; }
        const char *titel = NULL; bool app = false;
        for (int j = a; j < b; j++) { eintrag *g = liste->e[ix[j]]; if (g->PkgArt == PKG_APP) app = true; if (!titel && pkg_rang(g) == 0 && g->PkgTitel && g->PkgTitel[0]) titel = g->PkgTitel; }
        for (int j = a; j < b && !titel; j++) { eintrag *g = liste->e[ix[j]]; if (g->PkgTitel && g->PkgTitel[0]) titel = g->PkgTitel; }
        if (!titel) { int art = 3; const char *t = name_belegt(k, sch[ix[a]], &art); if (art <= 2 && t && t[0]) titel = t; }   /* bekannter Name zur Nummer */
        eintrag *kf = liste->e[h];
        kf->PkgOhneSpiel = true;
        kf->BlockName = VERB(A, titel ? titel : kf->PkgId, T(" · nicht installiert", " · not installed"));
        int kat = app ? 6 : 1;
        if (kf->Kat != kat) { kf->KatHer = kf->Kat; kf->Kat = kat; }
        for (int j = a; j < b; j++) if (ix[j] != h) zu[ix[j]] = h;
        a = b;
    }
    begleit_ordnen(k, liste, zu, pkg_rang, 0);
}
/* NEU 1.0.8 (PS5-Test 06.11.2026, da_Chris86 17:47: "das soll schon sauber geordnet sein, unabhängig, ob es installiert ist oder nicht"):
 * mehrere Programmdateien (ELF/BIN) desselben Tools, das weder eine Kachel noch einen Daten-Ordner hat (z. B. BFpilot_v0.4.4.elf und
 * BFpilot_v0.4.9-pre.elf auf dem USB-Laufwerk), stehen als EIN Block mit dem Namen des Tools unter "Apps". Derselbe heißt: gleicher Name
 * ohne Version, ohne "ps5-" vorne und "-ps5" hinten. Nur Anzeige (Begleit), jede Datei behält ihren eigenen Haken. */
static const char *payload_stamm(kern *k, const eintrag *f)
{
    if (f->PfadeBasis.n != 1) return NULL;
    htreffer pm = h_mt(A, h_blatt(A, f->PfadeBasis.e[0]), "^(.+?)(?:[-_ ]v?\\d[\\w.\\-]*)?\\.(?i:elf|bin)$");
    return pm.ok ? pm.gr[1] : NULL;
}
static void payload_gruppen(kern *k, pliste *liste)
{
    int n = liste->n, m;
    if (n == 0) return;
    const char **sch = a_hole(A, sizeof(char *) * (size_t)n), **stn = a_hole(A, sizeof(char *) * (size_t)n);
    for (int i = 0; i < n; i++) {
        eintrag *f = liste->e[i]; sch[i] = stn[i] = NULL;
        if ((f->Kat != 5 && f->Kat != 6) || f->ToolKachel || f->Begleit || f->OhneKachel || f->KachelId || f->Kachel || f->Jailbreak || f->Gesperrt
            || f->Teil || f->SmDatei || f->PkgArt) continue;
        const char *st = payload_stamm(k, f);
        if (!st) continue;
        st = h_r(A, h_r(A, st, "(?i)^ps5[-_ ]", ""), "(?i)[-_ ]ps5$", "");
        const char *s = tool_schluessel(k, st);
        if (strlen(s) >= 3) { sch[i] = s; stn[i] = st; }
    }
    int *ix = pkg_sortiert(k, sch, n, &m), *zu = a_hole(A, sizeof(int) * (size_t)n);
    for (int i = 0; i < n; i++) zu[i] = -1;
    bool je = false;
    for (int a = 0; a < m; ) {
        int b = a; while (b < m && !strcmp(sch[ix[a]], sch[ix[b]])) b++;
        if (b - a >= 2) {
            int h = ix[a];   /* der Kopf: die zuerst gefundene Datei (ix ist in Fundreihenfolge sortiert) */
            eintrag *kf = liste->e[h];
            kf->OhneKachel = true; kf->BlockName = stn[h];   /* Name ohne Version, ohne "ps5-" ("web-file-manager", "BFpilot") */
            if (kf->Kat != 6) { kf->KatHer = kf->Kat; kf->Kat = 6; }
            for (int j = a + 1; j < b; j++) zu[ix[j]] = h;
            je = true;
        }
        a = b;
    }
    if (je) begleit_ordnen(k, liste, zu, NULL, 6);
}
/* NEU 1.0.8 (Arbeitsplan 2.4, PLAN_1.0.8 Punkt 9, Anlass Nutzerfrage 04.11.2026): dasselbe Spiel mehr als einmal auf der Konsole -
 * installiert an zwei Orten (z. B. intern und auf der M.2, ein PS4-Spiel nach einer Neuinstallation), installiert und zusätzlich als
 * Spieldatei (Abbild), oder zwei Spieldateien. Bis 1.0.7 standen die Kopien als getrennte Zeilen da, ohne Hinweis. Jetzt kennt jede Kopie
 * die anderen (Doppelt), die Zeile sagt z. B. "doppelt: auch installiert auf der M.2-SSD" (kern_art.c kern_doppelt_text). Nur Anzeige:
 * was gelöscht wird, ändert sich nicht (gemeinsame Lizenz und Kachel-Infos bleiben geschützt, solange eine Kopie bleibt - kern_ps4.c).
 * Installationsdateien (PKG) zählen nicht als Kopie, sie stehen schon unter ihrem Spiel. Jailbreak-Einstiege bleiben außen vor. */
static const char *kopie_kennung(kern *k, const eintrag *f)
{
    if (f->Jailbreak || f->PfadeBasis.n == 0) return NULL;
    if (ist_teil(f, "Spiel") && !(f->Quelle && f->Quelle[0])) return kopf_kennung(f);              /* installiert */
    if (f->SmDatei) return f->SmKachel && f->SmKachel->KachelId && f->SmKachel->KachelId[0] ? f->SmKachel->KachelId : NULL;   /* Spieldatei einer ShadowMount-Kachel */
    if (f->Art == ART_ABBILD && f->PfadeBasis.n == 1) {                                            /* Spieldatei ohne Kachel: Nummer im Dateinamen */
        htreffer m = h_mt(A, h_blatt(A, f->PfadeBasis.e[0]), "(?-i)(?:^|[^A-Za-z0-9])([A-Z]{4}\\d{5})(?:[^0-9]|$)");
        return m.ok ? m.gr[1] : NULL;
    }
    return NULL;
}
/* Codex-Prüfung 73: eine installierte Kopie zählt nur, wenn ihr App-Ordner das Spiel wirklich enthält (app.pkg - PS4 und PS5 - oder
 * eboot.bin). Ein leerer Installationsrest neben einer echten Installation ist keine zweite Kopie - sonst stimmte der Satz "Löschst du
 * nur eine Kopie, bleibt die andere" nicht. Gelesen wird nur die Ordnerliste, und nur bei Spielen, die mehr als einmal vorkommen. */
static bool kopie_echt(kern *k, const eintrag *f, const char *id)
{
    if (!ist_teil(f, "Spiel") || (f->Quelle && f->Quelle[0])) return true;   /* Spieldatei: die Datei selbst ist die Kopie */
    for (int j = 0; j < f->PfadeBasis.n; j++) {
        const char *p = f->PfadeBasis.e[j], *b = strrchr(p, '/');
        if (!b || strcmp(b + 1, id) != 0 || !h_m(p, "(^|/)user/app/[^/]+$")) continue;
        dliste *l = s_liste(k->S, p, false);
        if (!l) return false;
        for (int i = 0; i < l->n; i++) if (!l->e[i].ist_ordner && !l->e[i].ist_link && (txt_gleich_ci(l->e[i].name, "app.pkg") || txt_gleich_ci(l->e[i].name, "eboot.bin"))) return true;
        return false;
    }
    return false;                                          /* kein App-Ordner dabei: nicht als Kopie zählen */
}
static void doppelte_spiele(kern *k)
{
    int n = k->Funde.n;
    if (n == 0) return;
    const char **id = a_hole(A, sizeof(char *) * (size_t)n);
    for (int i = 0; i < n; i++) id[i] = kopie_kennung(k, k->Funde.e[i]);
    for (int i = 0; i < n; i++) {                          /* nur Spiele, die mehr als einmal vorkommen, genauer ansehen */
        if (!id[i]) continue;
        bool mehr = false; for (int j = 0; j < n && !mehr; j++) if (j != i && id[j] && !strcmp(id[i], id[j])) mehr = true;
        if (mehr && !kopie_echt(k, k->Funde.e[i], id[i])) id[i] = NULL;
    }
    for (int i = 0; i < n; i++) {
        if (!id[i]) continue;
        eintrag *f = k->Funde.e[i];
        f->Doppelt = (pliste){0};
        for (int j = 0; j < n; j++) if (j != i && id[j] && !strcmp(id[i], id[j])) pl_add(A, &f->Doppelt, k->Funde.e[j]);
    }
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
        /* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86 zu Orbit Store: "Apps über ShadowMount unter Apps"): auch eine über ShadowMount
         * eingetragene Kachel (Quelle = Spieldatei), wenn ihre eigenen Angaben sagen, dass sie eine App ist - wie bei installierten Apps */
        if (f->Kat == 1 && (!(f->Quelle && f->Quelle[0]) || f->Kachel)) {
            for (int j = 0; j < f->PfadeBasis.n; j++) {
                htreffer m = h_mt(A, f->PfadeBasis.e[j], "/(app|patch|addcont)/([A-Z]{4}\\d{5})(/|$)");
                if (!m.ok) continue;
                app_titel(k, m.gr[2]);
                /* Codex-Prüfung 108 (P3): eine ShadowMount-Kachel bleibt, was sie ist (Art, Ampel, Hinweise wie unter Spiele) - Codex-Prüfung
                 * 109 (P3): auch ohne erreichbare Spieldatei (tote oder verschobene Kachel, Quelle leer); unter Spiele trägt nur sie .kachel */
                if (k->APP_ART == 2) { if (f->Kachel) f->KatHer = 1; f->Kat = 6; }
                break;
            }
        }
        /* NEU 1.0.8 (PS5-Test 07.11.2026, da_Chris86 17:35 zum Spielekompressor: "ist unter spiele drin nicht unter apps bitte prüfen",
         * 19:13 "entscheidest du"): eine Tool-Kachel, die sich bei Sony als Spiel meldet (param.json "applicationCategoryType": 0 - auf
         * da_Chris86s PS5 nur PSGC50001 "Spielekompressor", alle anderen Tools 65536), steht wie im PS5-Menü unter Spiele. Regel vom
         * 02.11.2026: "ob Spiel oder App, bleibt so, wie die PS5 es anzeigt" - bis hier galt sie für alle Kacheln außer Tool-Kacheln. Der
         * ganze Block geht mit (kachel_bloecke: Daten-Ordner und Programmdateien folgen ihrer Kachel). Sie bleibt eine Tool-Kachel (KatHer 6:
         * Art, Ampel, Hinweise und Löschen wie unter Apps). Ohne Angabe oder mit einem anderen Wert bleibt sie unter Apps. */
        else if (f->Kat == 6 && f->Kachel && !f->KatHer && txt_beginnt(f->TextBasis, T("Tool-Kachel: ", "Tool tile: "))) {
            for (int j = 0; j < f->PfadeBasis.n; j++) {
                htreffer m = h_mt(A, f->PfadeBasis.e[j], "^/user/(?:app|appmeta)/([A-Z]{4}\\d{5})$");
                if (!m.ok) continue;
                app_titel(k, m.gr[1]);
                if (k->APP_ART == 1) { f->KatHer = 6; f->Kat = 1; }
                break;
            }
        }
        else if (f->Kat == 6 && f->Ort == ORT_INTERN && !f->Kachel && f->PfadeBasis.n > 0) {
            /* Codex-Prüfung 83: der Rest eines Tool-Ordners ohne seine PKG-Dateien (merke_pkg_ordner) zählt wie der Ordner selbst */
            htreffer m = h_mt(A, f->ToolOrdner ? f->ToolOrdner : f->PfadeBasis.e[0], "^/user/data/(homebrew/)?([^/]+)$");
            if (!m.ok || h_m(m.gr[2], "(?i)\\.elf$")) continue;   /* ELF-Dateien bleiben unter Apps (Codex-Prüfung 39) */
            /* 02.11.2026: ob der Ordner zu einer Kachel gehört, entscheidet danach kachel_bloecke (gleiche Regeln für alle Kacheln) - er
             * kommt dann von hier aus unter seine Kachel; was zu keiner gehört, bleibt unter "Sonstiges" */
            if (tool_bekannt(k, m.gr[2]) || (!m.gr[1][0] && kern_bekannte_app(m.gr[2], NULL, NULL))) continue;   /* 1.0.9 (#146): bekannte App-Ordner unter /data (nicht homebrew) */
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
        d->Kat = f->Kat; d->KatHer = f->KatHer; d->Quelle = ""; d->Gesperrt = f->Gesperrt; d->SperrGrund = f->SperrGrund;   /* KatHer: NEU 1.0.8 (App über ShadowMount) */
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

/* NEU 1.0.8 (PS5-Test 07.11.2026, L.A. Noire: Spielstand und Trophäen-Liste "unbekannt", obwohl die PKG-Datei des Spiels auf dem
 * USB-Laufwerk lag): die PKG-Dateien werden erst nach Spielständen und Trophäen gefunden. Darum trägt namen_nachtragen nach der Suche den
 * Namen aus dem Kopf einer PKG-Datei ein - nur bei Kennungen, die beim Anlegen ohne jeden Beleg "unbekannt" blieben (NAMEN_OFFEN,
 * NPWR_OFFEN), nie bei einem Widerspruch. Die Texte bekommen genau die Form, die sie mit einem Beleg von Anfang an gehabt hätten. Eine
 * Trophäen-Liste bekommt den Namen, wenn ihre TROPHY.TRP mit der Trophäen-Datei einer PKG-Datei übereinstimmt (Kopf mit Prüfsumme). */
static const char *ersetze_alle(kern *k, const char *t, const char *alt, const char *neu)
{
    if (!t || !strstr(t, alt)) return t;
    const char *aus = ""; size_t al = strlen(alt);
    for (const char *q = t; ; ) {
        const char *p = strstr(q, alt);
        if (!p) { aus = VERB(A, aus, q); break; }
        aus = VERB(A, aus, a_kopie_n(A, q, (size_t)(p - q)), neu);
        q = p + al;
    }
    return aus;
}
static bool kennung_zeichen(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; }
/* "<id...> (unbekannt)" -> "<id...> (<Name>, nicht mehr auf der Konsole)" (mit_name_art) bzw. "(<Name>)" in Texten, die selbst schon
 * "nicht mehr installiert" sagen (mit_name_kurz) - nur, wo die Kennung als eigenes Wort beginnt */
static const char *nachtrag_kennung(kern *k, const char *t, const char *id, const char *name)
{
    const char *mk = T("Spiel/App unbekannt", "game/app unknown");
    if (!strstr(t, mk) || !strstr(t, id)) return t;
    /* Aufnahmen (galerie_name): "<id> (Spiel nicht mehr auf der Konsole, unbekannt)" */
    t = ersetze_alle(k, t, VERB(A, id, T(" (Spiel nicht mehr auf der Konsole, ", " (game no longer on the console, "), mk, ")"),
                     VERB(A, name, " (", id, T(", Spiel nicht mehr auf der Konsole)", ", game no longer on the console)")));
    /* Fake-Lizenz: "Fake-Lizenz: EP...-<id>_00-... - unbekannt (Spiel nicht mehr installiert)". Codex-Prüfung 108 (P2): nur, wenn die Lizenz
     * selbst diese Kennung trägt - die erste Spielnummer nach "Fake-Lizenz: ", genau wie beim Anlegen (tm in kern_suche_funde) */
    const char *fl = strstr(t, T("Fake-Lizenz: ", "Fake license: "));
    if (fl) {
        htreffer lm = h_mt(A, fl, "([A-Z]{4}\\d{5})");
        if (lm.ok && !strcmp(lm.gr[1], id))
            t = ersetze_alle(k, t, VERB(A, " - ", mk, T(" (Spiel nicht mehr installiert)", " (game no longer installed)")),
                             VERB(A, " - ", name, T(" (Spiel nicht mehr installiert)", " (game no longer installed)")));
    }
    /* ohne "nicht mehr auf der Konsole": Texte, die selbst schon "nicht mehr installiert" sagen, und - Prüfer 110 - installierte Kennungen
     * ohne lesbaren Namen (wie name_belegt, Codex-Prüfung 45) */
    bool kurz = strstr(t, T(" (Spiel nicht mehr installiert)", " (game no longer installed)")) != NULL || mt_hat(&k->INSTALLIERT, id);
    const char *ende = VERB(A, " (", mk, ")"), *neu = VERB(A, " (", name, kurz ? "" : T(", nicht mehr auf der Konsole", ", no longer on the console"), ")");
    size_t il = strlen(id), el = strlen(ende);
    const char *aus = "", *q = t; bool geaendert = false;
    for (const char *p = strstr(q, id); p; p = strstr(p + il, id)) {
        if (p > t && kennung_zeichen(p[-1])) continue;
        const char *e = p + il;
        while (kennung_zeichen(*e)) e++;
        if (strncmp(e, ende, el) != 0) continue;
        aus = VERB(A, aus, a_kopie_n(A, q, (size_t)(e - q)), neu);
        q = e + el; geaendert = true;
        p = q - il;   /* weiter hinter dem ersetzten Stück */
    }
    return geaendert ? VERB(A, aus, q) : t;
}
static void namen_nachtragen(kern *k)
{
    if (kern_bereiche_106() || k->PKG_NAMEN.n + k->PKG_TRP.n == 0) return;
    const char *mk = T("Spiel/App unbekannt", "game/app unknown");
    /* Trophäen-Listen (PS4): Kopf der TROPHY.TRP mit den Trophäen-Dateien der PKG-Dateien vergleichen - nur lesen, 96 Byte je Liste */
    k->NPWR_PKG = mt_neu(true);
    mtab nr = mt_neu(true);   /* NEU 1.0.8: Trophäen-Kennung -> Nummer der Liste im PKG (nur bei mehreren) */
    for (int i = 0; i < k->NPWR_OFFEN.n && k->PKG_TRP.n > 0; i++) {
        const char *np = k->NPWR_OFFEN.e[i].k;
        unsigned char b[96]; size_t len = 0;
        volatile bool ok = false;
        VERSUCHE { ok = s_lies_ab(k->S, VERB(A, "/user/trophy/conf/", np, "/TROPHY.TRP"), 0, sizeof b, b, &len) && len == sizeof b; }
        FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); ok = false; } ENDE_VERSUCHE;
        if (!ok) continue;
        char hex[sizeof b * 2 + 1];
        for (size_t j = 0; j < sizeof b; j++) snprintf(hex + 2 * j, 3, "%02x", b[j]);
        if (mt_hat(&k->PKG_TRP, hex)) mt_setze(A, &k->NPWR_PKG, np, mt_hole(&k->PKG_TRP, hex));
    }
    /* Codex-Prüfung 116 (P3): die Nummer für JEDE Liste ohne Nummer, deren Trophäen-Datei in einem PKG mit mehreren Listen steckt - auch wenn
     * ihr Name schon anders belegt ist (früheres Protokoll, Bilder). Nur lesen, 96 Byte je Liste. */
    for (int i = 0; i < k->bau.n && k->PKG_TRP_NR.n > 0; i++) {
        eintrag *f = k->bau.e[i];
        if (f->Kat != 3 || f->ListeNr > 0) continue;
        for (int j = 0; j < f->PfadeBasis.n; j++) {
            htreffer m = h_mt(A, f->PfadeBasis.e[j], "(?-i)^/user/trophy/conf/(NPWR\\d{5}_\\d{2})$");
            if (!m.ok || mt_hat(&nr, m.gr[1])) continue;
            unsigned char b[96]; size_t len = 0;
            volatile bool ok = false;
            VERSUCHE { ok = s_lies_ab(k->S, VERB(A, f->PfadeBasis.e[j], "/TROPHY.TRP"), 0, sizeof b, b, &len) && len == sizeof b; }
            FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); ok = false; } ENDE_VERSUCHE;
            if (!ok) continue;
            char hex[sizeof b * 2 + 1];
            for (size_t q = 0; q < sizeof b; q++) snprintf(hex + 2 * q, 3, "%02x", b[q]);
            if (mt_hat(&k->PKG_TRP_NR, hex)) mt_setze(A, &nr, a_kopie(A, m.gr[1]), mt_hole(&k->PKG_TRP_NR, hex));
        }
    }
    /* NEU 1.0.8 (PS5-Test 07.11.2026 17:32, DRIVECLUB und DRIVECLUB BIKES): mehrere Listen aus EINEM PKG tragen den Namen des Spiels -
     * ihre Nummer im PKG unterscheidet sie ("DRIVECLUB™ – Trophäen-Liste 2"), wie bei installierten Spielen (NPWR_NR). Für Liste,
     * Fortschritt und dessen Kopie (Ordnername ohne Endung) */
    for (int i = 0; i < k->bau.n && nr.n > 0; i++) {
        eintrag *f = k->bau.e[i];
        if (f->Kat != 3 || f->ListeNr > 0) continue;
        for (int j = 0; j < f->PfadeBasis.n; j++) {
            const char *b = npwr_basis(k, h_blatt(A, f->PfadeBasis.e[j]));
            if (mt_hat(&nr, b)) { f->ListeNr = (int)(intptr_t)mt_hole(&nr, b); break; }
        }
    }
    for (int i = 0; i < k->bau.n; i++) {
        eintrag *f = k->bau.e[i];
        if (!f->TextBasis || !strstr(f->TextBasis, mk)) continue;
        const char *tb = f->TextBasis, *tx = f->Text ? f->Text : "";
        for (int j = 0; j < k->NPWR_PKG.n; j++) {
            const char *np = k->NPWR_PKG.e[j].k, *nm = k->NPWR_PKG.e[j].v;
            const char *alt = VERB(A, np, " (", mk, ")"), *neu = VERB(A, nm, " (", np, ")");
            tb = ersetze_alle(k, tb, alt, neu); tx = ersetze_alle(k, tx, alt, neu);
        }
        for (int j = 0; j < k->NAMEN_OFFEN.n; j++) {
            const char *id = k->NAMEN_OFFEN.e[j].k;
            if ((intptr_t)k->NAMEN_OFFEN.e[j].v != 1 || !mt_hat(&k->PKG_NAMEN, id)) continue;   /* 2 = Widerspruch (offen_merken) */
            const char *nm = mt_hole(&k->PKG_NAMEN, id);
            tb = nachtrag_kennung(k, tb, id, nm); tx = nachtrag_kennung(k, tx, id, nm);
        }
        if (tb != f->TextBasis) { f->TextBasis = tb; f->Text = tx; }
    }
}

void kern_suche_funde(kern *k)
{
    k->S->abbruch = abbruch_kern; k->S->abbruch_ctx = k;
    k->FUNDE_GUELTIG = false;                 /* erst gültig, wenn die Suche ganz durch ist */
    k->bau = (pliste){0};                     /* die neue Liste wird erst am Ende sichtbar */
    k->PKG_GELISTET = 0; k->PKG_GEFUNDEN = 0;   /* NEU 1.0.8 (Codex-Prüfungen 83, 96): Grenzen der PKG-Suche in Ordnern gelten je Suche */
    /* NEU 1.0.8 (PS5-Test 07.11.2026): Namen, die erst nach dem Anlegen belegt werden (namen_nachtragen) - je Suche neu */
    k->PKG_NAMEN = mt_neu(true); k->PKG_TRP = mt_neu(true); k->NAMEN_OFFEN = mt_neu(true); k->NPWR_OFFEN = mt_neu(true); k->NPWR_PKG = mt_neu(true);
    k->PKG_TRP_NR = mt_neu(true);   /* NEU 1.0.8 */
    k->NPWR_UCP = mt_neu(true); k->BILD_ORT = mt_neu(true);   /* NEU 1.0.8 (PS5-Test 07.11.2026 16:31) */
    k->BILD_UPD = mt_neu(true);   /* NEU 1.0.9 */
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
            /* NEU 1.0.9 (da_Chris86 08.11.2026 10:10, Bild aus 1.0.8: "Leere Aufnahme-Zwischenordner (2)" mit content_tmp/PPSA01650 und
             * content_tmp/PSCL00002 unter dem Bild von Y2JB; an der PS5 nachgesehen): diesen Ordner legt die PS5 beim Start einer App selbst
             * an - beide waren an diesem Morgen beim Start von Y2JB (YouTube-App) bzw. PS5 Cleanup neu da. Leer und die App noch da
             * (installiert oder mit Kachel) = kein Rest, wie bei PlayGo und den ShadowMount-Ordnern; geht die App, nimmt das Löschen ihn
             * mit (kern_loeschen.c zwischenordner_weg). Der von PS5 Cleanup geht nur mit der Selbstentfernung (kern_loeschen.c 2b), der
             * von Systemprogrammen (NPXS) nie. */
            if (!kern_bereiche_106() && h_m(ids.e[i], "^PSCL0000[12]$")) continue;   /* Codex-Prüfung 127: der eigene auch nicht leer (kern_schutz.c sperrt ihn) */
            if (n == 0 && !kern_bereiche_106() && (h_like(ids.e[i], "NPXS*") || h_hat(&apps, ids.e[i]) || h_hat(&m2Apps, ids.e[i])
                                                   || h_hat(&usbApps, ids.e[i]) || h_hat(&appmeta, ids.e[i]) || h_hat(&sysmeta, ids.e[i]))) continue;
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
            else if (h_m(x, "^NPWR")) { const char *xb = npwr_basis(k, x);   /* NEU 1.0.8: Kopie "<Liste>_BK" */
                t = VERB(A, T("Trophäen-Fortschritt: ", "Trophy progress: "), trophaen_name(k, xb), T(" (PS4-Spiel)", " (PS4 game)"), trophaen_weg(k, xb), npwr_kopie(k, x, xb)); }
            else t = VERB(A, T("Trophäen-Daten ", "Trophy data "), "trophy/data/", x);
            sliste l = L1(A, VERB(A, "/user/home/", u, "/trophy/data/", x));
            MERKE(k, &l, 3, t, 0);
            const char *xn = npwr_basis(k, x);
            if (k->MERKE_OK && mt_hat(&k->NPWR_NR, xn)) ((eintrag *)k->bau.e[k->bau.n - 1])->ListeNr = (int)(intptr_t)mt_hole(&k->NPWR_NR, xn);
        }
        xs = s_namen(k->S, VERB(A, "/user/home/", u, "/trophy"), false);
        for (int j = 0; j < xs.n; j++)
            if (!h_gleich(xs.e[j], "data")) { sliste l = L1(A, VERB(A, "/user/home/", u, "/trophy/", xs.e[j])); MERKE(k, &l, 3, VERB(A, T("Trophäen-Daten ", "Trophy data "), "trophy/", xs.e[j]), 0); }
        xs = s_namen(k->S, VERB(A, "/user/home/", u, "/trophy2/nobackup/data"), false);
        for (int j = 0; j < xs.n; j++) {
            const char *x = xs.e[j];
            if (h_hat(&k->ASTRO_NPWRS, x)) continue;
            const char *xb = npwr_basis(k, x);   /* NEU 1.0.8: Kopie "<Liste>_BK" */
            const char *t = h_m(x, "^NPWR") ? VERB(A, T("Trophäen-Fortschritt: ", "Trophy progress: "), trophaen_name(k, xb), trophaen_weg(k, xb), npwr_kopie(k, x, xb))
                                             : VERB(A, T("Trophäen-Daten ", "Trophy data "), "trophy2/nobackup/data/", x);
            sliste l = L1(A, VERB(A, "/user/home/", u, "/trophy2/nobackup/data/", x));
            MERKE(k, &l, 3, t, 0);
            if (k->MERKE_OK && mt_hat(&k->NPWR_NR, xb)) ((eintrag *)k->bau.e[k->bau.n - 1])->ListeNr = (int)(intptr_t)mt_hole(&k->NPWR_NR, xb);
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
        VERSUCHE { for (int i = 0; i < nn; i++) { int vor = k->bau.n; merke_ordner(k, b, &ee[i], 1, ext_kat(ee[i].name), ""); usb_kopie_markieren(k, b, &ee[i], vor); } }
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
            else if (tm.ok) { int art = 3; const char *nm = name_belegt(k, tm.gr[1], &art); if (!nm[0]) offen_merken(k, tm.gr[1]); n = VERB(A, n, " - ", nm[0] ? nm : T("Spiel/App unbekannt", "game/app unknown")); }
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
    namen_nachtragen(k);   /* NEU 1.0.8 (PS5-Test 07.11.2026): Namen aus PKG-Dateien für "unbekannt" angelegte Einträge */
    if (!kern_bereiche_106()) bereiche_107(k);
    if (!kern_bereiche_106()) alte_sicherungen_zusammen(k);
    if (!kern_bereiche_106()) sm_spieldatei_zeilen(k);
    if (!kern_bereiche_106()) { pkg_zusammenfassen(k); pkg_doppelt(k); }   /* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 12) */
    /* fest nach Bereich ordnen, darin nach Ort (intern, M.2, USB, USB-Stick - NEU 1.0.6); sonst bleibt die Fundreihenfolge */
    pliste sortiert = {0};
    for (int ki = 0; ki < KAT_ANZ; ki++) for (int o = ORT_INTERN; o <= ORT_STICK; o++) for (int i = 0; i < k->bau.n; i++) {
        eintrag *f = k->bau.e[i];
        if (f->Kat == ALLE_KAT[ki] && f->Ort == o) pl_add(A, &sortiert, f);
    }
    if (kern_bereiche_106()) tool_daten_ordnen(k, &sortiert);
    else kachel_bloecke(k, &sortiert);   /* NEU 1.0.7 (02.11.2026): ein Block pro Kachel - ersetzt tool_daten_ordnen */
    if (!kern_bereiche_106()) ohne_eingeblendete_ordner(k, &sortiert);
    if (!kern_bereiche_106()) tools_ohne_kachel(k, &sortiert);   /* NEU 1.0.8 (Arbeitsplan 2.3) */
    if (!kern_bereiche_106()) pkg_ohne_spiel(k, &sortiert);      /* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 12) */
    if (!kern_bereiche_106()) payload_gruppen(k, &sortiert);     /* NEU 1.0.8 (PS5-Test 06.11.2026, da_Chris86 17:47) */
    k->Funde = sortiert;
    /* NEU 1.0.7 (PS5-Test 02.11.2026, Red Dead Redemption 2): kaputte Zusatzinhalt-Angaben deutlich an der Kachel des Spiels */
    if (!kern_bereiche_106() && k->SID_KAPUTT.n > 0)
        for (int i = 0; i < k->Funde.n; i++) {
            eintrag *f = k->Funde.e[i];
            const char *id = kopf_kennung(f);
            if (id && (f->KachelId || ist_teil(f, "Spiel")) && mt_hat(&k->SID_KAPUTT, id)) f->SidKaputt = (int)(intptr_t)mt_hole(&k->SID_KAPUTT, id);
        }
    kern_arten_setzen(k);   /* NEU 1.0.7: Art, Ampel, Rot */
    if (!kern_bereiche_106()) doppelte_spiele(k);   /* NEU 1.0.8 (Arbeitsplan 2.4): braucht die Art (Spieldatei ohne Kachel) */
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

