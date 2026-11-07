/* PS5 Cleanup 1.1 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * kern_schutz.c - Schutzliste, 1:1 aus Kern.cs (1.0.5):
 *   AstroPfad, GESPERRT, ERLAUBT, Erlaubt, OhneVerknuepfung.
 * Die Muster sind WÖRTLICH aus Kern.cs übernommen (nur die C#-Verkettung
 * "..." + KONSTANTE + "..." ist hier als C-Textverkettung geschrieben).
 *
 * WICHTIG: Hier wird nichts neu interpretiert. Der Selbstschutz der
 * ELF-Version (Schritt 6) kommt als eigene, ZUSÄTZLICHE Sperre davor -
 * er kann nur mehr sperren, nie etwas erlauben.
 */
#include "kern.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static void melde_nichts(void *c, const char *t) { (void)c; (void)t; }
static void warte_echt(void *c, int ms) { (void)c; usleep((useconds_t)ms * 1000); }   /* wie Thread.Sleep */

kern *kern_neu(zugriff *z)
{
    arena *a = arena_neu();
    kern *k = a_hole(a, sizeof *k);
    k->a = a;
    k->S = sitzung_neu(z, a);
    k->melde = melde_nichts; k->warte = warte_echt;
    k->ga = arena_neu();
    pthread_mutex_t *m = a_hole(a, sizeof *m); pthread_mutex_init(m, NULL); k->groessen_sperre = m;
    k->SM_GRUND = "";
    /* 1.0.6 (da_Chris86 28.10.2026): Astro's Playroom ist ein normales Spiel - ASTRO_IDS/ASTRO_NPWRS bleiben leer */
    k->LINK_CACHE = mt_neu(false);                     /* StringComparer.Ordinal */
    /* Suche: Wörterbücher wie in Suche.cs (H.Tab = ohne Groß/klein) */
    k->LEER_DANACH = mt_neu(true); k->TITEL_CACHE = mt_neu(true); k->NAMEN_CACHE = mt_neu(true);
    k->NPWR_NAMEN = mt_neu(true); k->NPWR_GALERIE = mt_neu(true); k->GALERIE_NAMEN = mt_neu(true);
    k->VERKNUEPFT = mt_neu(true); k->VERK_SPST = mt_neu(true); k->VERK_BELEG = mt_neu(true); k->INSTALLIERT = mt_neu(true);   /* NEU 1.0.7 */
    k->SID_KAPUTT = mt_neu(true); k->SID_GEZAEHLT = mt_neu(true); k->REST_NAMEN = mt_neu(true);
    k->QuelleInfo = mt_neu(true); k->ORDNER_CACHE = mt_neu(true);
    k->APP_ROH = "";
    k->PFAD_GROESSE = mt_neu(false);                   /* StringComparer.Ordinal */
    return k;
}
void kern_frei(kern *k) { if (k) { arena *ga = k->ga; arena_frei(k->a); arena_frei(ga); } }
void kern_melde(kern *k, const char *t) { k->melde(k->melde_ctx, t); }

/* -------------------------------------------------------------
 * Schutzliste
 * -------------------------------------------------------------
 * Gehört der Pfad zu Astro (App, Daten, Spielstand, Update, DLC, Trophäen-
 * fortschritt)? Eigene Aufnahmen aus Astro in der Mediengalerie zählen nicht dazu. */
bool kern_astro_pfad(kern *k, const char *p)
{
    arena *a = arena_neu();
    bool ja = false;
    if (!h_like(p, "/user/av_contents/*"))
        for (int i = 0; i < k->ASTRO_IDS.n && !ja; i++)
            if (h_m(p, VERB(a, "/", h_escape(a, k->ASTRO_IDS.e[i]), "(/|$|[-_.])"))) ja = true;
    for (int i = 0; i < k->ASTRO_NPWRS.n && !ja; i++)
        if (h_m(p, VERB(a, "^/user/home/[^/]+/.*/", h_escape(a, k->ASTRO_NPWRS.e[i]), "(/|$)"))) ja = true;
    arena_frei(a);
    return ja;
}

static const char *const GESPERRT[] = {
    /* 1.0.6: die Astro-Sperren aus 1.0.5 sind weg (Astro ist ein normales Spiel) */
    "^/user/appmeta/external$", "^/user/appmeta/addcont$",
    "^/user/home/[^/]+/savedata[^/]*/(NPXS|sce_)",
    "^" M2 "/user(/|$)", "^" M2 "/system_data(/|$)", "^" M2 "/lost\\+found(/|$)",
    "^" USB_ERW "/user(/|$)", "^" USB_ERW "/system_data(/|$)", "^" USB_ERW "/lost\\+found(/|$)",   /* NEU 1.0.6: USB wie M.2 */
    "/app/NPXS", "/appmeta/NPXS", "/patch/NPXS", "/addcont/NPXS", "^/user/download/NPXS",
    "^/user/license/(act\\.dat|nobackup)", "^/user/system/share/(trophy|share_)",
    "^/user/catalog_downloader/[^/]+$" };
static const char *const ERLAUBT[] = {
    /* NEU 1.0.6 (Nutzer-Protokoll 28.10.2026, Sony-Ordner "push_resource"): unter appmeta und download nur Spielnummern */
    "^/user/(app|patch|addcont)/[A-Za-z]{4}\\d{5}(/|$)", "^/user/appmeta/(addcont/|external/addcont/)?[A-Za-z]{4}\\d{5}(/|$)",
    "^/user/home/[^/]+/savedata/[^/]+", "^/user/home/[^/]+/savedata_prospero/[^/]+",
    "^/user/home/[^/]+/trophy/[^/]+", "^/user/home/[^/]+/trophy2/[^/]+",
    "^/user/home/[^/]+/webkit/shell/local$",
    "^/user/av_contents/[^/]+/[^/]+",
    "^/user/data/[^/]+", "^/user/download/[A-Za-z]{4}\\d{5}(/|$)",
    "^/user/_zu_loeschen_[^/]+", "^" M2 "/[^/]+",
    "^" USB_ERW "/[^/]+", "^/mnt/usb\\d+/[^/]+" };   /* NEU 1.0.6: USB-Speicher wie M.2, USB-Stick (nur einzeln angeboten) */
#define ANZ(x) ((int)(sizeof(x) / sizeof((x)[0])))

/* NEU 1.1 (U16): Zwischendatei der Teilen-/Aufnahmefunktion der PS5, z. B.
 * /user/av_contents/content_tmp/sharevideo_f6754cdb_hdr.webm - kommt und geht, kann gerade
 * beschrieben werden. Nie anbieten, nie löschen. Andere WebM-Dateien sind nicht betroffen. */
bool kern_laufende_aufnahme(const char *p)
{
    return p && h_m(p, "^/user/av_contents/content_tmp/sharevideo_[0-9A-Fa-f]+_hdr\\.webm$");
}

/* NEU 1.1 (S5): Was PS5 Cleanup NIE anfasst - hier ausdrücklich aufgeschrieben, zusätzlich zur Liste aus
 * 1.0.5 (die all das schon nicht erlaubt). Kann nur sperren, nie erlauben. Die Ausnahmen (?!...) sind genau
 * die Einzelfälle, die 1.0.5 erlaubt - am Ergebnis der Prüfliste ändert sich dadurch nichts.
 *  Systemsoftware/Systembereiche, Sony-Apps, Systemeinstellungen, Laufwerks-Kopplung, Konten,
 *  Aktivierung und echte Lizenzen, eingebundene Spiele (ShadowMount) und Wechselmedien. */
static const char *const NIE[] = {
    "^/(system|system_ex|preinst|preinst2|update|dev|proc|app0|hostapp|data|usr|lib|etc|bin|sbin|var|tmp)(/|$)",
    "^/mnt/(sandbox|pfs|shadowmnt|rnps|disc|host|nfs|ext\\d+/system_data|ext\\d+/lost\\+found)(/|$)",
    "^/system_data(/|$)(?!priv/appmeta/(external/)?[A-Z]{4}\\d{5}$|priv/error/history/[^/]+\\.json$)",
    "^/user/home/[^/]+/?$",
    "^/user/home/[^/]+/(?!savedata/[^/]|savedata_prospero/[^/]|trophy/[^/]|trophy2/[^/]|webkit/shell/local$)",
    "^/user/license(/|$)(?!fake[^/]+\\.(rif|idx)$)",
    "^/user/system(/|$)(?!share/[A-Z]{4}\\d{5}$)",
    "^/user/(priv|common|sshared|music_player_service|devlog|lost\\+found|playgo|savedata|catalog_downloader|np_uds|trophy|trophy2|bgft|temp|app|appmeta|patch|addcont|download|av_contents|data|home)/?$",
    "^/user/(app|appmeta|patch|addcont|download)/NPXS",
    /* NEU 1.1 (Anforderungen 27.10.2026, Codex-Prüfung 7): Sony-Apps auch in Unterordnern der Kachel-Infos */
    "^/user/appmeta/(.+/)?NPXS",
};
/* NEU 1.0.7 (Recherche 31.10.2026, doku/recherche_shadowmount_ffpkg + recherche_tools_B): Einhängeordner der Tools
 * (ps5upload, kstuff-lite, garlic) und die Merkdatei, mit der ShadowMount kstuff vom Einhängen abhält. Zusätzlich lehnt
 * die Löschschicht jeden eingehängten Ordner ab (zugriff_datei.c, einbindung()). Nicht in den Vergleichstests mit 1.0.5/1.0.6
 * (PS5C_BEREICHE_106) - deren Testbäume enthalten .kstuff_noautomount, das 1.0.5 noch angeboten hat. */
static const char *const NIE_107[] = {
    "^/mnt/ps5upload(/|$)",
    "^/user/data/(imgmnt|save_mnt|mount_sd|mount_owner)(/|$)",
    "^/user/data/\\.kstuff_noautomount$",
    "^/user/data/(homebrew/)?[^/]+/folder-source(/|$)",   /* LZ4 Game Compressor: eingeblendetes Spiel-Original (31.10.2026) */
    "^/user/data/PS5-Cleanup-Protokolle(/|$)",            /* NEU 02.11.2026: Protokolle von PS5 Cleanup ohne USB-Stick (Ordner, je Lauf eine Datei) */
    "^/mnt/ext1/PS5-Cleanup-Protokolle(/|$)",             /* NEU 02.11.2026 17:23: dasselbe auf der M.2 (interne Platte voll) */
};
/* NEU 1.0.7 (Recherche 31.10.2026): Bild/Beschreibung eines Spielstands - nur zusammen mit dem Spielstand (kern_suche.c), nie NPXS/sce_ */
static bool spielstand_meta(const char *p)
{
    return !kern_bereiche_106() && h_m(p, "^/user/home/[^/]+/savedata(_prospero)?_meta/(user/)?[A-Z]{4}\\d{5}$") && !h_m(p, "/NPXS");
}
/* NEU 1.0.7 (da_Chris86 31.10.2026 17:00, Ausnahme zu "/system_ex nie anfassen"; Recherche: ShadowMountPlus sm_filesystem.c legt
 * /system_ex/app/<Kennung> als Einhängeordner an und löscht ihn nie): leere Einhängeordner. Nie NPXS (Sony), nie PSCL (PS5 Cleanup),
 * nie FAKE00000 (Homebrew Launcher), nie FAKE78xxx (von Playden verwaltete Spiele - nur in Playden entfernen). Playden selbst
 * (FAKE77031) seit 03.11.2026 (da_Chris86 15:11, PS5-Test: Playden ganz gelöscht, sein leerer Ordner blieb) wie jedes andere Spiel:
 * nur leer und nur, wenn Playden nicht mehr installiert ist. Angeboten nur leer, ohne installiertes Spiel; gelöscht nur mit rmdir
 * (NurLeer), eingehängte lehnt die Löschschicht ab. */
static bool sm_leerer_ordner(const char *p)
{
    return !kern_bereiche_106() && h_m(p, "^/system_ex/app/[A-Z]{4}\\d{5}$") && !h_m(p, "/(NPXS|PSCL)\\d{5}$") && !h_m(p, "/FAKE(00000|78\\d{3})$");
}
bool kern_nie_anfassen(const char *p)
{
    if (spielstand_meta(p)) return false;
    if (sm_leerer_ordner(p)) return false;
    for (size_t i = 0; i < sizeof NIE / sizeof NIE[0]; i++) if (h_m(p, NIE[i])) return true;
    if (!kern_bereiche_106()) for (size_t i = 0; i < sizeof NIE_107 / sizeof NIE_107[0]; i++) if (h_m(p, NIE_107[i])) return true;
    return false;
}

static bool erlaubt_innen(kern *k, arena *a, const char *p)
{
    if (p == NULL) return false;
    if (h_m(p, "(^|/)\\.\\.?(/|$)") || h_m(p, "//") || h_m(p, "\\s$") || h_steuerzeichen(p) || !h_m(p, "^/")) return false;
    if (h_m(p, "./$")) return false;                             /* NEU 1.1 (Codex-Prüfung 7): "/x/" umginge Sperren mit $ */
    if (kern_laufende_aufnahme(p)) return false;                 /* NEU 1.1 (U16) */
    /* NEU 03.11.2026 (da_Chris86 17:51: "intern ja, USB nein"): alte Protokolle auf der Konsole - nur in kern_alte_protokolle_weg */
    if (!kern_bereiche_106() && k->PROTOKOLLE_FREI && (kern_altes_protokoll(k, p) || h_m(p, "^/user/data/PS5-Cleanup-Protokolle$"))) return true;
    if (kern_nie_anfassen(p)) return false;                      /* NEU 1.1 (S5) */
    /* Astro's Playroom: seit 1.0.6 ein normales Spiel (Liste leer) */
    if (kern_astro_pfad(k, p)) return false;
    if (spielstand_meta(p)) return true;                         /* NEU 1.0.7: geht nur mit dem Spielstand (eigene Zeile gibt es nicht) */
    if (sm_leerer_ordner(p)) return true;                        /* NEU 1.0.7: leerer Einhängeordner (nur rmdir) */
    /* NEU 1.0.7: leere Arbeitsordner, die die PS5 für unsere eigene App anlegt - nur während der Selbstentfernung */
    if (!kern_bereiche_106() && k->SCHUTZ.frei && h_m(p, "^/user/(temp/share|av_contents/content_tmp)/PSCL00002$")) return true;
    /* NEU 1.0.7: Installationsdaten (PlayGo) eines Spiels - einzelne Content-ID-Ordner, nie der Ordner selbst, nie NPXS */
    if (!kern_bereiche_106() && h_m(p, "^/user/playgo/(app|patch|addcont)/[A-Z]{2}\\d{4}-[A-Z]{4}\\d{5}_\\d{2}-[A-Z0-9]{16}$")) return !h_m(p, "-NPXS");
    /* M.2- und USB-Verwaltung (USB NEU 1.0.6): nur einzelne Spiele/Updates, nie die Ordner selbst */
    if (h_m(p, "^(" M2 "|" USB_ERW ")/user/(app|patch)/[^/]+$")) return !h_m(p, "/NPXS");
    if (h_m(p, "^(" M2 "|" USB_ERW ")/user/addcont/[A-Z]{4}\\d{5}(/[^/]+)?$")) return !h_m(p, "/NPXS");
    /* Kachel-Infos der PS5: nur einzelne Spiel-/App-Kennungen, nie NPXS, external */
    if (h_m(p, "^" SYS_META "/[A-Z]{4}\\d{5}$")) return !h_m(p, "/NPXS");
    /* Merkzettel für Spiele auf externen USB-Platten */
    if (h_m(p, "^" SYS_META "/external/[A-Z]{4}\\d{5}$")) return !h_m(p, "/NPXS");
    /* Fake-Lizenzen des Jailbreaks (act.dat und nobackup/ = echte Lizenzen, nie) */
    if (h_m(p, "^/user/license/fake[^/]+\\.(rif|idx)$")) return true;
    /* Share-Einstellungen einzelner Spiele (trophy/ und die .mst-Datei nie) */
    if (h_m(p, "^/user/system/share/[A-Z]{4}\\d{5}$")) return !h_m(p, "/NPXS");
    /* Store-Bildercache */
    if (h_m(p, "^/user/catalog_downloader/(conceptmeta|appmeta)/[^/]+$")) return true;
    /* temporäre Dateien: nur Inhalte, nie die Ordner selbst */
    if (h_m(p, "^/user/(bgft/trash|savedata/temp|temp/gls/screenshot|temp/video_recording|temp/common_temp/telemetry)/[^/]+$")) return true;
    /* Fehlerberichte: nur die einzelnen .json-Dateien */
    if (h_m(p, "^/system_data/priv/error/history/[^/]+\\.json$")) return true;
    /* Trophäen-Pakete der Spiele (Astro nie) */
    htreffer tm = h_mt(a, p, "^/user/(trophy/conf|trophy2/nobackup/conf|np_uds/nobackup/conf)/(NPWR\\d{5}_\\d{2})$");
    if (tm.ok) return !h_hat(&k->ASTRO_NPWRS, tm.gr[2]);
    /* niemals: Systemordner, Konto, Lizenzen, Astro, Sony-Systemanwendungen */
    for (int i = 0; i < ANZ(GESPERRT); i++) if (h_m(p, GESPERRT[i])) return false;
    for (int i = 0; i < ANZ(ERLAUBT); i++) if (h_m(p, ERLAUBT[i])) return true;
    return false;
}

/* NEU 1.0.8 (Codex-Prüfung 74): Ordner der Test-Fassung (Mitschnitt, Bilder) - weder er noch etwas darin noch ein Ordner darüber wird
 * angeboten oder gelöscht, auch nicht von der Selbstentfernung (alle Löschwege fragen kern_erlaubt). Die Oberfläche füllt die Liste in
 * jeder Fassung (Codex-Prüfung 75). Groß/klein egal (Codex-Prüfung 75): auf dem USB-Laufwerk (exFAT) ist "ps5-cleanup-test" derselbe Ordner. */
static bool gleich_ci_n(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned char x = (unsigned char)a[i], y = (unsigned char)b[i];
        if (x >= 'A' && x <= 'Z') x = (unsigned char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (unsigned char)(y + 32);
        if (x != y) return false;
        if (x == 0) return true;
    }
    return true;
}
bool kern_test_ordner(const kern *k, const char *p)
{
    if (!p || k->TEST_ORDNER.n == 0) return false;
    size_t l = strlen(p);
    if (l == 1 && p[0] == '/') return true;
    for (int i = 0; i < k->TEST_ORDNER.n; i++) {
        const char *t = k->TEST_ORDNER.e[i]; size_t tl = strlen(t);
        if (l >= tl && gleich_ci_n(p, t, tl) && (p[tl] == 0 || p[tl] == '/')) return true;   /* der Ordner selbst oder etwas darin */
        if (l < tl && gleich_ci_n(t, p, l) && t[l] == '/') return true;                       /* ein Ordner darüber */
    }
    return false;
}
bool kern_erlaubt(kern *k, const char *p)
{
    if (kern_test_ordner(k, p)) return false;
    arena *a = arena_neu();
    bool r = erlaubt_innen(k, a, p);
    arena_frei(a);
    return r;
}

/* Steckt in einem Ordner auf dem Weg eine Verknüpfung (Symlink)? Dann
 * könnte der Pfad ganz woanders hinzeigen (z.B. auf die M.2) - solche Pfade
 * werden nie gelöscht. (Der letzte Teil selbst darf eine Verknüpfung sein -
 * DELE entfernt dann nur die Verknüpfung.) */
static int teile_pfad(arena *a, const char *p, const char ***aus)   /* p.Split('/') */
{
    int n = 1; for (const char *c = p; *c; c++) if (*c == '/') n++;
    const char **t = a_hole(a, sizeof(char *) * (size_t)n);
    int i = 0; const char *anf = p;
    for (const char *c = p;; c++) {
        if (*c == '/' || *c == 0) { t[i++] = a_kopie_n(a, anf, (size_t)(c - anf)); anf = c + 1; if (!*c) break; }
    }
    *aus = t;
    return n;
}

bool kern_ohne_verknuepfung(kern *k, const char *p)
{
    arena *a = k->a;
    const char **teile; int n = teile_pfad(a, p, &teile);
    const char *weg = "";
    for (int i = 1; i < n - 1; i++) {
        const char *eltern = weg[0] == 0 ? "/" : weg;
        weg = VERB(a, weg, "/", teile[i]);
        if (!mt_hat(&k->LINK_CACHE, eltern)) mt_setze(a, &k->LINK_CACHE, eltern, s_liste(k->S, eltern, true));
        dliste *l = mt_hole(&k->LINK_CACHE, eltern);
        if (l == NULL) return true;                    /* gibt es nicht -> nichts zu löschen */
        datei *e = NULL;
        for (int j = 0; j < l->n; j++) if (strcmp(l->e[j].name, teile[i]) == 0) { e = &l->e[j]; break; }
        /* nicht gefunden, obwohl der Ordner darüber da ist: nur weitermachen,
         * wenn es den ganzen Pfad wirklich nicht gibt */
        if (e == NULL) return !s_existiert(k->S, p);
        if (e->ist_link || !e->ist_ordner) return false;
    }
    return true;
}
