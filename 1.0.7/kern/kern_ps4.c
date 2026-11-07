/* PS5 Cleanup 1.0.7 - Schutz installierter PS4-Spiele (siehe kern.h "NEU 1.0.7 ... kern_ps4.c")
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * Auftrag da_Chris86 31.10.2026 19:56: "Installierte PS4-Spiele dürfen durch PS5 Cleanup oder unnötig empfohlene
 * Datenbank-Neuaufbauten nicht mehr unbeabsichtigt verloren gehen."
 *  - Inventar (nur lesen): /user/app, /mnt/ext1/user/app (M.2), /mnt/ext0/user/app (USB-Speicher), Ordner CUSA<5 Ziffern>.
 *    Installiert = app.pkg mit mehr als 0 Byte (wie elf-arsenal src/fpkg_db.c detect_storage: ein leerer Ordner nach einem
 *    Neuaufbau zählt als "tot"). Ordner nicht lesbar = unklar = wie installiert (geschützt).
 *  - Schutz: Ein Pfad, der zur Installation eines installierten PS4-Spiels gehört, darf nur weg, wenn genau dieses Spiel in
 *    diesem Lauf gewählt ist. Gemeinsame Pfade (Lizenz, Kachel-Infos, PlayGo, App-Daten) nur, wenn ALLE installierten Kopien
 *    dieser Kennung gewählt sind. Spielstände, Trophäen, Aufnahmen gehören nicht dazu (eigene Bereiche, eigene Wahl).
 *  - Keine Schreibzugriffe, keine Datenbank-Zugriffe (Sonys Eintrag nur über die Statusabfrage der PS5-Fassung).
 */
#include "kern.h"
#include <string.h>

#define A (k->a)

int (*kern_ps4_sony_eingetragen)(const char *id) = NULL;

static const char *const BASEN[3] = { "/user", M2 "/user", USB_ERW "/user" };
static const int ORTE[3] = { ORT_INTERN, ORT_M2, ORT_USB };

bool kern_ps4_installiert(const ps4_spiel *s) { return s->unklar || s->app_da; }

const char *kern_ps4_ort_text(int ort)
{
    switch (ort) { case ORT_M2: return T("M.2-SSD", "M.2 SSD"); case ORT_USB: return kern_bereiche_106() ? T("USB-Speicher", "USB storage") : T("erweiterter Speicher", "extended storage"); default: return T("intern", "internal"); }
}

/* Größe einer Datei aus der Liste ihres Ordners (0 = nicht da). unklar != NULL (App-Ordner): Ordner nicht lesbar, oder die Datei
 * fehlt, der Ordner ist aber nicht leer (z. B. Installation läuft oder andere Form) -> *unklar = true (gilt als installiert). */
static long long datei_groesse(kern *k, const char *ordner, const char *name, bool *unklar)
{
    dliste *l = s_liste(k->S, ordner, false);
    if (!l) { if (unklar && s_existiert(k->S, ordner)) *unklar = true; return 0; }
    int andere = 0;
    for (int i = 0; i < l->n; i++) {
        if (!strcmp(l->e[i].name, name) && !l->e[i].ist_ordner && !l->e[i].ist_link) { if (l->e[i].groesse > 0) return l->e[i].groesse; }
        else andere++;
    }
    if (unklar && andere > 0) *unklar = true;
    return 0;
}
static const char *sfo_feld(kern *k, const char *pfad, const char *feld)
{
    const unsigned char *d; size_t n;
    if (!s_bytes(k->S, pfad, &d, &n)) return "";
    mtab h = kern_sfo_lesen(A, d, n);
    return feld ? kern_sfo_wert(A, &h, feld) : kern_sfo_titel(A, &h);
}

void kern_ps4_erfassen(kern *k)
{
    k->PS4 = (pliste){0};
    sliste lizenzen = s_namen(k->S, "/user/license", false);
    for (int b = 0; b < 3; b++) {
        const char *app = VERB(A, BASEN[b], "/app");
        /* streng: fehlt der Ordner (z. B. keine M.2), ist das kein Fehler; ist er da, aber nicht lesbar, bricht das Inventar ab
         * (der Aufrufer setzt dann PS4_UNKLAR - eine leere Liste dürfte NIE als "keine PS4-Spiele" gelten) */
        sliste ids = s_namen(k->S, app, true);
        for (int i = 0; i < ids.n; i++) {
            if (!h_m(ids.e[i], "^CUSA\\d{5}$")) continue;
            ps4_spiel *s = a_hole(A, sizeof *s);
            memset(s, 0, sizeof *s);
            s->id = ids.e[i]; s->basis = BASEN[b]; s->ort = ORTE[b]; s->eingetragen = -1;
            s->app_bytes = datei_groesse(k, VERB(A, app, "/", s->id), "app.pkg", &s->unklar);
            s->app_da = s->app_bytes > 0;
            s->patch_bytes = datei_groesse(k, VERB(A, BASEN[b], "/patch/", s->id), "patch.pkg", NULL);
            s->patch_da = s->patch_bytes > 0;
            s->dlc = s_namen(k->S, VERB(A, BASEN[b], "/addcont/", s->id), false).n;
            /* Name und Version wie die Suche (Kachel-Infos der PS5, sonst appmeta) */
            const char *sfo[3] = { VERB(A, SYS_META "/", s->id, "/param.sfo"), VERB(A, "/user/appmeta/", s->id, "/param.sfo"), VERB(A, app, "/", s->id, "/sce_sys/param.sfo") };
            const char *name = "";
            for (int j = 0; j < 3 && !name[0]; j++) name = sfo_feld(k, sfo[j], NULL);
            s->name = name[0] ? name : s->id;
            const char *v = s->patch_da ? sfo_feld(k, VERB(A, BASEN[b], "/patch/", s->id, "/sce_sys/param.sfo"), "APP_VER") : "";
            for (int j = 0; j < 3 && !v[0]; j++) v = sfo_feld(k, sfo[j], "APP_VER");
            s->version = v;
            for (int j = 0; j < lizenzen.n; j++) if (strstr(lizenzen.e[j], s->id)) {
                sl_add(A, &s->lizenzen, lizenzen.e[j]);
                if (!strncmp(lizenzen.e[j], "fake", 4)) s->fake = true;
            }
            if (kern_ps4_sony_eingetragen && kern_ps4_installiert(s)) s->eingetragen = kern_ps4_sony_eingetragen(s->id);
            pl_add(A, &k->PS4, s);
        }
    }
}

/* Zu welcher PS4-Installation gehört ein Pfad? Kennung oder NULL. *basis: Speicher der Kopie oder NULL = gemeinsam (mehrdeutig). */
const char *kern_ps4_zuordnung(arena *a, const char *p, const char **basis)
{
    *basis = NULL;
    htreffer t = h_mt(a, p, "^(/user|/mnt/ext[01]/user)/(app|patch|addcont)/(CUSA\\d{5})(/|$)");
    if (t.ok) { *basis = t.gr[1]; return t.gr[3]; }
    t = h_mt(a, p, "^(/mnt/ext[01]/user)/(appmeta|playgo/(app|patch|addcont))/(?:[A-Z]{2}\\d{4}-)?(CUSA\\d{5})(/|$|_)");
    if (t.ok) { *basis = t.gr[1]; return t.gr[4]; }
    static const char *const GEMEINSAM[] = {
        "^/user/appmeta/(?:addcont/)?(CUSA\\d{5})(/|$)",
        "^" SYS_META "/(CUSA\\d{5})(/|$)",
        "^/user/license/[^/]*(CUSA\\d{5})[^/]*$",
        "^/user/playgo/(?:app|patch|addcont)/[A-Z]{2}\\d{4}-(CUSA\\d{5})_",
        "^/system_data/playgo/(CUSA\\d{5})(/|$)",
        "^/user/download/(CUSA\\d{5})(/|$)",
        "^/user/system/share/(CUSA\\d{5})(/|$)",
        NULL };
    for (int i = 0; GEMEINSAM[i]; i++) { t = h_mt(a, p, GEMEINSAM[i]); if (t.ok) return t.gr[1]; }
    return NULL;
}

/* Gewählt ("darf weg") ist eine PS4-Installation, wenn ein gewählter Eintrag ihr Spiel ist (Teil "Spiel", Gruppe "Basis|CUSA")
 * oder genau ihren App-Ordner <Basis>/app/CUSAxxxxx löscht (z. B. Kachel eines ShadowMount- oder Jailbreak-Spiels). */
void kern_ps4_weg_setzen(kern *k)
{
    k->PS4_WEG = mt_neu(true);
    for (int i = 0; i < k->Funde.n; i++) {
        eintrag *f = k->Funde.e[i];
        if (!f->Wahl) continue;
        if (ist_teil(f, "Spiel") && f->Gruppe) {
            htreffer t = h_mt(A, f->Gruppe, "^(.+)\\|(CUSA\\d{5})$");
            if (t.ok) mt_setze(A, &k->PS4_WEG, VERB(A, t.gr[1], "|", t.gr[2]), (void *)1);
        }
        for (int j = 0; j <= f->Pfade.n; j++) {
            const char *p = j < f->Pfade.n ? f->Pfade.e[j] : f->KachelOrdner;
            if (!p) continue;
            htreffer t = h_mt(A, p, "^(/user|/mnt/ext[01]/user)/app/(CUSA\\d{5})$");
            if (t.ok) mt_setze(A, &k->PS4_WEG, VERB(A, t.gr[1], "|", t.gr[2]), (void *)1);
        }
    }
}

bool kern_ps4_geschuetzt(kern *k, const char *p, const char **grund)
{
    *grund = NULL;
    const char *basis;
    const char *id = kern_ps4_zuordnung(A, p, &basis);
    if (!id) {
        /* Ordner, in dem Teile installierter PS4-Spiele liegen (z. B. /mnt/ext1/user, /user/license): nur, wenn alle diese Spiele gewählt sind */
        size_t l = strlen(p);
        const char *vor = (l && p[l - 1] == '/') ? p : VERB(A, p, "/");
        for (int i = 0; i < k->PS4.n; i++) {
            const ps4_spiel *s = k->PS4.e[i];
            if (!kern_ps4_installiert(s) || mt_hat(&k->PS4_WEG, VERB(A, s->basis, "|", s->id))) continue;
            const char *teile[5] = { VERB(A, s->basis, "/app/", s->id), VERB(A, s->basis, "/patch/", s->id), VERB(A, s->basis, "/addcont/", s->id),
                                     VERB(A, "/user/appmeta/", s->id), VERB(A, SYS_META "/", s->id) };
            bool drin = false;
            for (int j = 0; j < 5 && !drin; j++) if (txt_beginnt(teile[j], vor)) drin = true;
            for (int j = 0; j < s->lizenzen.n && !drin; j++) if (txt_beginnt(VERB(A, "/user/license/", s->lizenzen.e[j]), vor)) drin = true;
            if (drin) {
                *grund = VERB(A, T("enthält das installierte PS4-Spiel ", "contains the installed PS4 game "), s->name, " (", s->id, ", ", kern_ps4_ort_text(s->ort),
                              T(") - das Spiel selbst ist nicht gewählt", ") - the game itself is not selected"));
                return true;
            }
        }
        if (k->PS4_UNKLAR) {
            static const char *const ORTE_PS4[] = { "/user/app/", "/user/patch/", "/user/addcont/", "/user/license/", "/user/appmeta/", M2 "/user/app/", USB_ERW "/user/app/", NULL };
            for (int j = 0; ORTE_PS4[j]; j++) if (txt_beginnt(ORTE_PS4[j], vor)) {
                *grund = T("enthält Ordner der PS4-Spiele - die PS4-Spiele ließen sich nicht erfassen, darum bleibt es", "contains folders of the PS4 games - the PS4 games could not be listed, so it stays");
                return true;
            }
        }
        return false;
    }
    bool installiert = false, alle_weg = true;
    const ps4_spiel *wer = NULL;
    for (int i = 0; i < k->PS4.n; i++) {
        const ps4_spiel *s = k->PS4.e[i];
        if (strcmp(s->id, id) || !kern_ps4_installiert(s)) continue;
        if (basis && strcmp(basis, s->basis)) continue;            /* andere Kopie - für diesen Pfad nicht maßgeblich */
        installiert = true; if (!wer) wer = s;
        if (!mt_hat(&k->PS4_WEG, VERB(A, s->basis, "|", s->id))) { alle_weg = false; wer = s; }
    }
    if (k->PS4_UNKLAR) {   /* Inventar fehlt: im Zweifel nicht löschen - außer das Spiel dieser Kennung ist gewählt */
        bool gewaehlt = false;
        for (int i = 0; i < k->PS4_WEG.n; i++) { htreffer t = h_mt(A, k->PS4_WEG.e[i].k, "\\|(CUSA\\d{5})$"); if (t.ok && !strcmp(t.gr[1], id) && (!basis || txt_beginnt(k->PS4_WEG.e[i].k, VERB(A, basis, "|")))) gewaehlt = true; }
        if (gewaehlt) return false;
        *grund = VERB(A, T("könnte zu einem installierten PS4-Spiel gehören (", "might belong to an installed PS4 game ("), id,
                      T(") - die PS4-Spiele ließen sich nicht erfassen, darum bleibt es", ") - the PS4 games could not be listed, so it stays"));
        return true;
    }
    if (!installiert || alle_weg) return false;
    *grund = VERB(A, T("gehört zum installierten PS4-Spiel ", "belongs to the installed PS4 game "), wer->name, " (", wer->id, ", ", kern_ps4_ort_text(wer->ort),
                  T(") - das Spiel selbst ist nicht gewählt", ") - the game itself is not selected"));
    return true;
}

const char *kern_ps4_zeile(arena *a, const ps4_spiel *s)
{
    const char *z = VERB(a, s->name, " (", s->id, ") – ", kern_ps4_ort_text(s->ort), " – ");
    if (s->unklar) z = VERB(a, z, T("Inhalt nicht eindeutig (gilt als installiert)", "contents unclear (treated as installed)"));
    else if (s->app_da) z = VERB(a, z, kern_groesse_text(a, s->app_bytes));
    else z = VERB(a, z, T("Installationsdaten FEHLEN (nur leerer Ordner)", "installation data MISSING (empty folder only)"));
    if (s->patch_da) z = VERB(a, z, " – Update ", s->version[0] ? s->version : "", " ", kern_groesse_text(a, s->patch_bytes));
    else if (s->version[0]) z = VERB(a, z, " – Version ", s->version);
    if (s->dlc > 0) z = VERB(a, z, " – ", a_fmt(a, "%d", s->dlc), T(" Zusatzinhalt(e)", " add-on(s)"));
    z = VERB(a, z, " – ", s->lizenzen.n == 0 ? T("keine Lizenzdatei", "no license file") : s->fake ? T("Fake-Lizenz", "fake license") : T("Lizenz", "license"));
    if (s->eingetragen == 1) z = VERB(a, z, T(" – im Menü", " – in the menu"));
    else if (s->eingetragen == 0) z = VERB(a, z, T(" – NICHT im Menü eingetragen", " – NOT registered in the menu"));
    return z;
}

/* NEU 1.0.7 (da_Chris86 31.10.2026 19:56, Punkt 4): Ende ohne Empfehlung zum Datenbank-Neuaufbau. Was im Menü stehen bleibt, wird
 * benannt und bleibt unverändert, dazu die Bitte ums Protokoll (seit 02.11.2026 kein Wort mehr zum Neuaufbau). "" = nichts zu sagen. */
const char *kern_rest_text(kern *k, arena *a)
{
    const char *t = "";
    if (k->GEISTER.n > 0)
        t = VERB(a, T("Diese Einträge bleiben im PS5-Menü sichtbar: ", "These entries stay visible in the PS5 menu: "), sl_join(a, &k->GEISTER, ", "),
                 T(". PS5 Cleanup lässt sie unverändert.", ". PS5 Cleanup leaves them unchanged."));
    else if (!k->SONY)
        t = T("Kacheln gelöschter Einträge können im PS5-Menü sichtbar bleiben. PS5 Cleanup lässt sie unverändert.",
              "Tiles of deleted entries can stay visible in the PS5 menu. PS5 Cleanup leaves them unchanged.");
    /* NEU 1.0.8 (PS5-Test 07.11.2026 15:55: Sony lehnte drei PS4-Spiele ab, hier stand trotzdem "Einbindung ... nicht eindeutig"): in
     * UNVERAENDERT landet alles, was unverändert blieb, aus jedem Grund (Sony lehnt ab, unklare Einbindung, Spielstand, Kachel bleibt) -
     * den Grund nennt die Zeile des Eintrags im Protokoll */
    if (k->UNVERAENDERT.n > 0)
        t = VERB(a, t, t[0] ? " " : "", T("Unverändert gelassen (den Grund nennt das Protokoll beim Eintrag): ", "Left unchanged (the log gives the reason at each entry): "),
                 sl_join(a, &k->UNVERAENDERT, ", "), ".");
    if (!t[0]) return "";
    /* da_Chris86 02.11.2026 15:54: "Den Risikoweg mit der Datenbank gehen wir nie wieder" - kein Wort mehr zum Neuaufbau (er löscht
     * installierte PS4-Spiele); statt dessen das Protokoll an uns schicken. */
    t = VERB(a, t, T(" Bitte schick uns das Protokoll (GitHub oder psxtools.de), damit wir das beheben können.", " Please send us the log (GitHub or psxtools.de) so we can fix this."));
    return t;
}
