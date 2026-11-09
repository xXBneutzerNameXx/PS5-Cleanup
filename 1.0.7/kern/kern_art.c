/* PS5 Cleanup 1.0.7 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * kern_art.c - NEU 1.0.7 (Grundlage da_Chris86 31.10.2026 13:12, doku/PLAN_1.0.7.md):
 *  - Art jedes Eintrags (Spiel, Spielstand, Rest ...) -> Ampel und ein kurzer Satz in der Zeile ("endgültig weg").
 *  - Rot = endgültig weg: nie über "ganzen Bereich wählen", nur einzeln (die vollständige Bereinigung bleibt wie sie ist).
 *  - Update/Zusatzinhalt eines noch installierten Spiels geht nur mit dem Spiel (wie Sonys eigenes Löschen).
 *  - Satz vor dem Löschen: "Danach endgültig weg: ...".
 * Texte laut abgestimmter Textliste (Docs "PS5 Cleanup 1.0.7 – Texte je Eintrag", Freigabe 31.10.2026 13:16) und
 * den Recherchen (doku/recherche_*_2026-10-31.md). Nur Anzeige und Auswahl-Regeln - Pfade und Schutz bleiben unberührt.
 */
#include "kern.h"
#include <string.h>

#define FUER_ALLE(f) for (int _i = 0; _i < k->Funde.n; _i++) for (eintrag *f = k->Funde.e[_i]; f; f = NULL)

static const char *ohne_ort(const eintrag *f)
{
    const char *v = kern_ort_vorsatz(f->Ort), *t = f->TextBasis ? f->TextBasis : "";
    /* NEU 1.0.7: Teil unter seiner Kachel ("   – USB-Stick – pldmgr_v0.6.3.elf"); seit 02.11.2026 (ein Block pro Kachel) auch ohne Ort -
     * das Einrücken zeichnet die Oberfläche selbst */
    if (txt_beginnt(t, "   – ")) t += strlen("   – ");
    return (v[0] && txt_beginnt(t, v)) ? t + strlen(v) : t;
}
static bool beg(const char *t, const char *de, const char *en) { return txt_beginnt(t, T(de, en)); }
static bool hat(const char *t, const char *de, const char *en) { return txt_enthaelt(t, T(de, en)); }

/* Schlüssel eines Ordnernamens: klein, nur Buchstaben/Ziffern (wie tool_schluessel in kern_suche.c) */
static void schluessel(const char *s, char *o, size_t max)
{
    size_t j = 0;
    for (; *s && j + 1 < max; s++) { char c = *s; if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) o[j++] = c; }
    o[j] = 0;
}
/* Recherche 31.10.2026 (doku/recherche_tooldaten_2026-10-31.md): in diesen Daten-Ordnern liegt Unersetzliches */
static const char *wichtig_was(const char *ordner)
{
    char s[64]; schluessel(ordner, s, sizeof s);
    if (!strcmp(s, "garlic")) return T("Spielstand-Backups", "saved-data backups");
    if (!strcmp(s, "ps5upload")) return T("Sicherungen (z. B. Spielstände)", "backups (e.g. saved data)");
    if (!strcmp(s, "playden")) return T("Spielstände der PC-Spiele", "saves of the PC games");
    if (!strcmp(s, "etahen")) return T("eigene Cheats, Plugins und Spiel-Dumps", "own cheats, plugins and game dumps");
    if (!strcmp(s, "cheatrunner")) return T("eigene Cheats und Patches", "own cheats and patches");
    if (!strcmp(s, "neoenabler")) return T("Sicherungen deiner Patch-Dateien", "backups of your patch files");
    /* "user" (/data/user/patches): seit 1.0.9 bei APP_DATEN - Patch Manager (ps-patch-system), PS5-Test 09.11.2026 */
    const char *w = NULL;   /* NEU 1.0.9 (#146): Daten-Ordner bekannter Apps, in denen nachweislich eigene Dateien liegen */
    if (kern_bekannte_app(ordner, NULL, &w) && w) return w;
    return NULL;
}
/* NEU 1.0.7 (PS5-Test 03.11.2026): "Was ist wichtig" für die Zeile - feste Angabe aus der Recherche, sonst das, was im Ordner gefunden wurde */
static const char *wichtig_von(const eintrag *f, const char *dn)
{
    const char *w = dn ? wichtig_was(dn) : NULL;
    return w ? w : f->WichtigWas;
}
/* "deine eigenen Cheats" statt "deine eigene Cheats", nie "deine deine Spiel-Patches" (da_Chris86 03.11.2026) */
static const char *mit_deine(arena *a, const char *w)
{
    if (txt_beginnt(w, "eigene ")) return VERB(a, "deine eigenen ", w + strlen("eigene "));
    if (txt_beginnt(w, "deine ") || txt_beginnt(w, "your ") || txt_beginnt(w, "vermutlich ") || txt_beginnt(w, "probably ")) return w;
    return VERB(a, T("deine ", "your "), w);
}
static const char *daten_ordner(arena *a, const eintrag *f)   /* "garlic" aus /user/data/garlic bzw. /user/data/homebrew/garlic */
{
    if (f->PfadeBasis.n == 0) return NULL;
    htreffer m = h_mt(a, f->ToolOrdner ? f->ToolOrdner : f->PfadeBasis.e[0], "^/user/data/(homebrew/)?([^/]+)$");   /* Ordner: NEU 1.0.7 folder-source */
    return m.ok ? m.gr[2] : NULL;
}
static bool spiel_sicherung(const eintrag *f)   /* LZ4-Game-Compressor: verschobene Originale (Recherche Widersprüche, Punkt 2) */
{
    for (int j = 0; j < f->PfadeBasis.n; j++) if (h_m(f->PfadeBasis.e[j], "(?i)/game-backups(/|$)")) return true;
    return false;
}
static bool gepacktes_spiel(const eintrag *f)   /* LZ4-Game-Compressor: <TitelID>-app_AMPR = gepacktes Spiel */
{
    for (int j = 0; j < f->PfadeBasis.n; j++) if (h_m(f->PfadeBasis.e[j], "[A-Z]{4}\\d{5}-app_AMPR(/|$)")) return true;
    return false;
}

static int art_von_kat(kern *k, const eintrag *f, int kat);
/* NEU 1.0.7 (02.11.2026): ein Eintrag, der unter seine Kachel gezogen wurde, bleibt, was er ist (Art aus seinem alten Bereich) - nur was
 * dort "Inhalt unbekannt" war, ist jetzt bekannt: Daten der App, wie bei Tool-Daten */
static int art_von(kern *k, const eintrag *f)
{
    if (!f->KatHer) return art_von_kat(k, f, f->Kat);
    int a = art_von_kat(k, f, f->KatHer);
    /* NEU 1.0.7 (PS5-Test 03.11.2026): eine Punkt-Datei direkt auf einem Speicher, die laut Programm einer Kachel gehört (.orbit-drive von
     * Orbit Store), ist eine Markierung des Tools - kein "neu herunterladen" */
    if (a == ART_DATEI && f->ToolKachel && !kern_bereiche_106() && f->PfadeBasis.n == 1 && h_m(f->PfadeBasis.e[0], "^/mnt/(ext\\d+|usb\\d+)/\\.[^/]+$")
        && !h_m(f->PfadeBasis.e[0], "(?i)\\.(elf|self|bin|pkg)$")) return ART_TOOL_DATEN;
    return (a == ART_UNBEKANNT) ? art_von_kat(k, f, 6) : a;
}
static bool alte_sicherung(const eintrag *f)   /* ps5cleanup_db_… (kern_suche.c alte_sicherungen_zusammen) */
{
    if (f->PfadeBasis.n == 0) return false;
    for (int j = 0; j < f->PfadeBasis.n; j++) if (!h_m(f->PfadeBasis.e[j], "/ps5cleanup_db_[^/]*$")) return false;
    return true;
}
static bool protokoll_datei(const eintrag *f)   /* NEU 1.0.8: ps5cleanup_protokoll_2026-11-07_120028.txt (bis hier "neu herunterladen") */
{
    if (f->PfadeBasis.n == 0) return false;
    for (int j = 0; j < f->PfadeBasis.n; j++) if (!h_m(f->PfadeBasis.e[j], "/ps5cleanup_protokoll_[^/]*\\.txt$")) return false;
    return true;
}
/* NEU 1.0.8 (PS5-Test 07.11.2026 22:53): was in einem Ordner liegt, den die Konsole auf ein USB-Laufwerk kopiert hat (kern_suche.c
 * usb_kopie_markieren) - "Screenshots und Videos", "Spielstände und Datensicherung" ... */
static const char *usb_kopie_was(arena *a, const eintrag *f)
{
    const char *w[4]; int n = 0;
    if (f->UsbKopie & USB_KOPIE_AUFNAHMEN) { w[n++] = T("Screenshots", "screenshots"); w[n++] = T("Videos", "videos"); }
    if (f->UsbKopie & USB_KOPIE_SPIELSTAENDE) w[n++] = T("Spielstände", "saved data");
    if (f->UsbKopie & USB_KOPIE_SICHERUNG) w[n++] = T("Datensicherung", "backup");
    if (n == 0) return T("Kopien von der Konsole", "copies from the console");
    const char *s = w[0];
    for (int i = 1; i < n; i++) s = VERB(a, s, i == n - 1 ? T(" und ", " and ") : ", ", w[i]);
    return s;
}
static int art_von_kat(kern *k, const eintrag *f, int kat)
{
    if (f->SmDatei) return f->Jailbreak ? ART_JAILBREAK : ART_SM_DATEI;
    /* NEU 1.0.8 (Befund 14): "kat" statt f->Kat - eine Programmdatei, die unter die Kachel eines Jailbreak-Einstiegs gezogen wurde
     * (webkit-autoloader-installer.elf unter WebKit Autoloader), ist selbst kein Einstieg */
    if (f->Jailbreak || (kat == 7 && !kern_bereiche_106())) return ART_JAILBREAK;
    if (f->PkgArt && !kern_bereiche_106()) return ART_PAKET_DATEI;   /* NEU 1.0.8 (Befund 12): PKG-Datei, egal unter welchem Spiel */
    if (f->StoreTeil && !kern_bereiche_106()) return f->StoreTeil == STORE_FASSUNG ? ART_STORE_FASSUNG : ART_REST;   /* NEU 1.0.9 (#145): ProsperoStore */
    const char *t = ohne_ort(f);
    if (f->Teil) {
        if (!strcmp(f->Teil, "Spiel")) return f->Kat == 6 ? ART_APP : ART_SPIEL;
        if (!strcmp(f->Teil, "Update")) return ART_UPDATE;
        if (!strcmp(f->Teil, "DLC")) return ART_DLC;
    }
    if (spiel_sicherung(f)) return ART_SPIEL_SICHERUNG;
    if (gepacktes_spiel(f)) return ART_ABBILD;
    if (!kern_bereiche_106() && alte_sicherung(f)) return ART_ALTE_SICHERUNG;
    if (!kern_bereiche_106() && f->UsbKopie) return ART_USB_KOPIE;   /* NEU 1.0.8 (PS5-Test 07.11.2026 22:53) */
    if (!kern_bereiche_106() && protokoll_datei(f)) return ART_PROTOKOLL;   /* NEU 1.0.8 (07.11.2026) */
    switch (kat) {
    case 1:
        if (f->Quelle && f->Quelle[0]) return ART_SM_KACHEL;
        if (hat(t, " - ShadowMount-Spiel", " - ShadowMount game")) return ART_SM_KACHEL;   /* Spieldatei gehört mehreren Kacheln: nur die Kachel */
        if (hat(t, " - ShadowMount: ", " - ShadowMount: ")) return ART_SM_TOT;
        return ART_ABBILD;
    case 2: return ART_SPIELSTAND;
    case 3:
        if (f->Paket) return ART_TR_LISTE;
        if (beg(t, "Trophäen-Liste: ", "Trophy list: ")) return ART_TR_LISTE_REST;
        if (beg(t, "Trophäen-Fortschritt: ", "Trophy progress: ")) return ART_TR_FORTSCHRITT;
        if (beg(t, "Trophäen-Gesamtzähler", "Trophy totals")) return ART_TR_ZAEHLER;
        return ART_TR_DATEN;
    case 4:
        if (beg(t, "Aufnahmen von ", "Captures of ")) return ART_AUFNAHMEN;
        if (beg(t, "Unfertige Aufnahme", "Unfinished capture")) return ART_AUFNAHME_UNFERTIG;
        if (beg(t, "Vorschaubilder ohne Aufnahme", "Thumbnails without capture") || beg(t, "Leere Aufnahme-Zwischenordner", "Empty capture temp folders")) return ART_GALERIE_REST;
        return ART_GALERIE_SONST;
    case 6: {
        if (f->Kachel && beg(t, "Tool-Kachel: ", "Tool tile: ")) return ART_TOOL_KACHEL;
        arena *a = arena_neu();
        const char *o = daten_ordner(a, f);
        int r = ART_DATEI;
        if (o && !h_m(o, "(?i)\\.(elf|bin|pkg)$")) r = wichtig_was(o) ? ART_TOOL_DATEN_WICHTIG : ART_TOOL_DATEN;
        else if (f->PfadeBasis.n > 0 && h_m(f->PfadeBasis.e[0], "(?i)\\.pkg$")) r = ART_PAKET_DATEI;
        arena_frei(a);
        return r;
    }
    case 5: {
        if (beg(t, "Fake-Lizenz: ", "Fake license: ")) return ART_FAKE_LIZENZ;
        if (beg(t, "App-Daten ohne installierte App: ", "App data without installed app: ")) return ART_APP_DATEN;
        if (beg(t, "Update ohne Spiel: ", "Update without game: ") || beg(t, "Zusatzinhalt: ", "Add-on: ") || beg(t, "Zusatzinhalt-Infos: ", "Add-on info: ")
            || beg(t, "Reste: ", "Leftovers: ") || beg(t, "Leere Kachel (Schloss): ", "Empty tile (lock icon): ") || beg(t, "Share-Einstellungen alter Spiele", "Share settings of old games")
            || beg(t, "Store-Bildercache", "Store image cache") || beg(t, "Temporäre Dateien", "Temporary files") || beg(t, "Fehlerberichte", "Error reports")
            || beg(t, "Infos zu Spielen auf USB-Platten", "Info on games on USB drives") || beg(t, "Rest einer früheren Aufräumaktion", "Leftover from an earlier cleanup")
            || hat(t, "  (leerer Ordner)", "  (empty folder)") || hat(t, "  (Verknüpfung, nur sie wird gelöscht)", "  (symlink, only the link is deleted)"))
            return ART_REST;
        if (f->PfadeBasis.n > 0 && h_m(f->PfadeBasis.e[0], "(?i)\\.pkg$")) return ART_PAKET_DATEI;
        if (f->PfadeBasis.n > 0 && h_m(f->ToolOrdner ? f->ToolOrdner : f->PfadeBasis.e[0], "^/user/data/[^/]+$")) return ART_UNBEKANNT;   /* Codex-Prüfung 85: auch der Rest ohne PKG-Dateien */
        return ART_DATEI;
    }
    }
    (void)k;
    return ART_SONST;
}

int kern_ampel(const eintrag *f)
{
    if (f->PkgOffen && !kern_bereiche_106()) return AMPEL_ROT;   /* Codex-Prüfung 90: Ordner kann PKG-Dateien enthalten - wie eine PKG-Datei */
    switch (f->Art) {
    case ART_SM_KACHEL: case ART_SM_TOT: case ART_TR_LISTE_REST: case ART_GALERIE_REST: case ART_REST:
        return AMPEL_GRUEN;
    case ART_SM_DATEI: case ART_ABBILD: case ART_SPIELSTAND: case ART_TR_FORTSCHRITT: case ART_TR_DATEN: case ART_AUFNAHMEN:
    case ART_GALERIE_SONST: case ART_TOOL_DATEN_WICHTIG: case ART_SPIEL_SICHERUNG: case ART_JAILBREAK:
    case ART_USB_KOPIE:   /* NEU 1.0.8 (PS5-Test 07.11.2026 22:53): eigene Daten auf dem USB-Laufwerk */
    case ART_STORE_FASSUNG:   /* NEU 1.0.9 (#145): vorige Fassung einer App samt den Dateien darin */
        return AMPEL_ROT;
    case ART_PAKET_DATEI:   /* NEU 1.0.8 (Befund 12, Entscheidung da_Chris86 06.11.2026): eine PKG-Datei ist danach endgültig weg */
        return kern_bereiche_106() ? AMPEL_GELB : AMPEL_ROT;
    default:
        return AMPEL_GELB;
    }
}

/* NEU 1.0.7 (PS5-Test 03.11.2026: /data/PCSX2 von PS5SX2 hat die PS2-Speicherkarten Mcd001.ps2/Mcd002.ps2, /data/prosperoeden die
 * Ordner roms, keys und user - beides stand als "Einstellungen ... startet mit Grundeinstellungen" da): ein Daten-Ordner, in dem direkt
 * ein nicht leerer Ordner wie roms, saves, savedata, memcards, keys, games, backups liegt oder eine Speicherkarten-/Spielstand-Datei
 * (.ps2 .mcd .mcr .srm .sav), enthält vermutlich eigene Spiele oder Spielstände - rot, nur einzeln, mit Hinweis. Allgemein für jedes
 * Tool (Emulatoren legen so etwas an), keine Namensliste. Nur lesen, nur die oberste Ebene. */
static void wichtiger_inhalt(kern *k, eintrag *f)
{
    if (kern_bereiche_106() || f->Art != ART_TOOL_DATEN || f->PfadeBasis.n == 0) return;
    arena *a = k->a;
    const char *p = f->ToolOrdner ? f->ToolOrdner : f->PfadeBasis.e[0];
    if (!h_m(p, "^/user/data/(homebrew/)?[^/]+$")) return;
    /* Codex-Prüfung 56: eine einzelne Speicherkarten-/Spielstand-Datei (z. B. /user/data/Emulator.sav) ist selbst der Spielstand */
    if (!f->ToolOrdner && f->PfadeBasis.n == 1 && h_m(p, "\\.(ps2|mcd|mcr|srm|sav)$")) {
        f->Art = ART_TOOL_DATEN_WICHTIG;
        f->WichtigWas = T("vermutlich eigene Spiele, Spielstände oder Sicherungen", "probably own games, saves or backups");
        f->WichtigFund = h_blatt(a, p);
        return;
    }
    dliste *dl = NULL;
    VERSUCHE { dl = s_liste(k->S, p, false); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); dl = NULL; } ENDE_VERSUCHE;
    sliste fund = {0};
    for (int i = 0; dl && i < dl->n && fund.n < 4; i++) {
        const char *n = dl->e[i].name;
        if (dl->e[i].ist_link) continue;
        if (dl->e[i].ist_ordner && h_m(n, "^(roms?|saves?|save_?data|savedata|memcards?|memory ?cards?|keys|games|backups?)$")) {
            dliste *innen = NULL;
            VERSUCHE { innen = s_liste(k->S, VERB(a, p, "/", n), false); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); innen = NULL; } ENDE_VERSUCHE;
            if (innen && innen->n > 0) sl_add(a, &fund, n);
        } else if (!dl->e[i].ist_ordner && dl->e[i].groesse > 0 && h_m(n, "\\.(ps2|mcd|mcr|srm|sav)$")) sl_add(a, &fund, n);
    }
    if (fund.n == 0) return;
    f->Art = ART_TOOL_DATEN_WICHTIG;
    f->WichtigWas = T("vermutlich eigene Spiele, Spielstände oder Sicherungen", "probably own games, saves or backups");
    f->WichtigFund = sl_join(a, &fund, ", ");
}
/* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 2): /data/etaHEN stand mit 0 Byte rot da ("deine eigenen Cheats, Plugins und Spiel-Dumps
 * endgültig weg"), obwohl nichts darin liegt. Ein Ordner, der als wichtig gilt, in dem aber nachweislich keine Datei mit Inhalt liegt,
 * ist nur ein Einstellungs-Ordner (gelb, "startet mit Grundeinstellungen"). Nur lesen, höchstens 3 Ebenen tief und 200 Einträge; nicht
 * lesbar, eine Verknüpfung, eine unbekannte Größe oder die Grenze erreicht = bleibt rot. */
#define LEER_TIEFE 3
#define LEER_MAX 200
static bool ohne_inhalt(kern *k, const char *p, int tiefe, int *rest)
{
    if (tiefe > LEER_TIEFE) return false;
    dliste *volatile dl = NULL;
    VERSUCHE { dl = s_liste(k->S, p, false); } FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); dl = NULL; } ENDE_VERSUCHE;
    if (!dl) return false;
    for (int i = 0; i < dl->n; i++) {
        if (--*rest < 0 || dl->e[i].ist_link) return false;
        if (!dl->e[i].ist_ordner) { if (dl->e[i].groesse != 0) return false; continue; }
        if (!ohne_inhalt(k, VERB(k->a, p, "/", dl->e[i].name), tiefe + 1, rest)) return false;
    }
    return true;
}
static void wichtig_leer(kern *k, eintrag *f)
{
    if (kern_bereiche_106() || f->Art != ART_TOOL_DATEN_WICHTIG || f->ToolOrdner || f->PfadeBasis.n == 0) return;
    int rest = LEER_MAX;
    for (int j = 0; j < f->PfadeBasis.n; j++) if (!ohne_inhalt(k, f->PfadeBasis.e[j], 1, &rest)) return;
    f->Art = ART_TOOL_DATEN; f->WichtigLeer = true;
}
void kern_arten_setzen(kern *k)
{
    FUER_ALLE(f) {
        f->Art = art_von(k, f);
        wichtiger_inhalt(k, f);
        wichtig_leer(k, f);   /* NEU 1.0.8 (Befund 2) */
        f->Rot = !kern_bereiche_106() && kern_ampel(f) == AMPEL_ROT;
    }
}

/* kurzer Satz neben der Zeile (Textliste "So steht es in der Zeile", Spalte "Daneben") */
const char *kern_folge(const eintrag *f)
{
    if (f->PkgOffen && !kern_bereiche_106()) return T("kann PKG-Dateien enthalten – endgültig weg", "may contain PKG files – gone for good");   /* Codex-Prüfung 90 */
    switch (f->Art) {
    case ART_SPIEL:              return T("neu installieren nötig", "reinstall needed");
    case ART_APP:                return T("neu laden und neu anmelden", "download and sign in again");
    case ART_UPDATE:
    case ART_DLC:                return T("geht nur mit dem Spiel", "goes only with the game");
    case ART_SM_KACHEL:          return T("kommt von selbst wieder", "comes back by itself");
    case ART_SM_DATEI:           return T("endgültig weg", "gone for good");
    case ART_SM_TOT:             return T("es geht nichts verloren", "nothing is lost");
    case ART_ABBILD:             return T("dein Spiel – endgültig weg", "your game – gone for good");
    case ART_SPIELSTAND:         return T("endgültig weg, außer du hast eine Sicherung", "gone for good unless you have a backup");
    case ART_TR_FORTSCHRITT:     return T("endgültig weg", "gone for good");
    case ART_TR_ZAEHLER:         return T("Anzeige kann sich ändern", "display may change");
    case ART_TR_LISTE:           return T("kommt beim Spielstart wieder", "comes back when the game starts");
    case ART_TR_LISTE_REST:      return T("es geht nichts verloren", "nothing is lost");
    case ART_TR_DATEN:           return T("Inhalt unklar – lieber stehen lassen", "content unclear – better keep it");
    case ART_AUFNAHMEN:          return T("endgültig weg", "gone for good");
    case ART_AUFNAHME_UNFERTIG:  return T("ließ sich nicht abspielen", "could not be played anyway");
    case ART_GALERIE_REST:       return T("es geht nichts verloren", "nothing is lost");
    case ART_GALERIE_SONST:      return T("Inhalt unklar – lieber stehen lassen", "content unclear – better keep it");
    case ART_TOOL_KACHEL:        return T("Tool neu installieren", "reinstall the tool");
    case ART_TOOL_DATEN:         return T("startet mit Grundeinstellungen", "starts with default settings");
    case ART_TOOL_DATEN_WICHTIG: return T("eigene Sicherungen endgültig weg", "own backups gone for good");
    case ART_DATEI:              return T("neu herunterladen", "download again");
    case ART_PAKET_DATEI:        return kern_bereiche_106() ? T("neu besorgen", "get it again") : T("endgültig weg", "gone for good");
    case ART_REST:               return T("es geht nichts verloren", "nothing is lost");
    case ART_FAKE_LIZENZ:        return T("kommt mit dem Spiel wieder", "comes back with the game");
    case ART_APP_DATEN:
    case ART_UNBEKANNT:          return T("Inhalt unbekannt", "content unknown");
    case ART_SPIEL_SICHERUNG:    return T("deine Original-Spiele – endgültig weg", "your original games – gone for good");
    case ART_JAILBREAK:          return T("Jailbreak danach neu einrichten", "set up the jailbreak again afterwards");
    case ART_ALTE_SICHERUNG:     return T("wird nicht mehr gebraucht", "no longer needed");
    case ART_USB_KOPIE:          return T("deine Daten – endgültig weg", "your data – gone for good");   /* NEU 1.0.8 */
    case ART_PROTOKOLL:          return T("Protokoll – nur zum Nachlesen", "log – for reference only");   /* NEU 1.0.8 */
    case ART_STORE_FASSUNG:      return T("ältere Fassung samt deinen Dateien – endgültig weg", "older version with your files – gone for good");   /* NEU 1.0.9 (#145) */
    default:                     return T("neu einrichten oder neu laden", "set up or download again");
    }
}

/* NEU 1.0.8 (Arbeitsplan 4.3, K5): kurze Fassung für das Schildchen in der Zeile der neuen Auswahl - der ganze Satz steht rechts im
 * Kasten ("Was passiert danach?", kern_art_info). Bis 1.0.7 brach z. B. "endgültig weg, außer du hast eine Sicherung" in jeder Zeile um. */
const char *kern_folge_kurz(const eintrag *f)
{
    switch (f->Art) {
    case ART_SPIEL: case ART_TOOL_KACHEL:            return T("neu installieren", "reinstall");
    case ART_APP:                                    return T("neu laden", "download again");
    case ART_UPDATE: case ART_DLC:                   return T("nur mit dem Spiel", "only with the game");
    case ART_SM_KACHEL:                              return T("kommt wieder", "comes back");
    /* 1.0.9 (PS5-Test 09.11.2026 20:14, da_Chris86 "Sicher, dass das die Fehler sind"): Schild und Farbe passen zusammen - beide sind gelb
     * ("kommt" nicht sicher von selbst wieder: Trophäen-Liste bei selbst kopierten Spielen, Fake-Lizenz nur mit dem Spiel), also auch der
     * gelbe Text wie in der Farb-Erklärung ("Lässt sich zurückholen") */
    case ART_TR_LISTE:                               return T("neu laden", "download again");
    case ART_FAKE_LIZENZ:                            return T("neu installieren", "reinstall");
    case ART_SM_TOT: case ART_TR_LISTE_REST: case ART_GALERIE_REST: case ART_REST: return T("nichts geht verloren", "nothing is lost");
    case ART_SM_DATEI: case ART_ABBILD: case ART_SPIELSTAND: case ART_TR_FORTSCHRITT: case ART_AUFNAHMEN: case ART_TOOL_DATEN_WICHTIG:
    case ART_SPIEL_SICHERUNG:                        return T("endgültig weg", "gone for good");
    case ART_TR_ZAEHLER:                             return T("Anzeige ändert sich", "display changes");
    case ART_TR_DATEN: case ART_GALERIE_SONST: case ART_APP_DATEN: case ART_UNBEKANNT: return T("Inhalt unklar", "content unclear");
    case ART_AUFNAHME_UNFERTIG:                      return T("war unbrauchbar", "was unusable");
    case ART_TOOL_DATEN:                             return T("neu einrichten", "set up again");
    case ART_DATEI:                                  return T("neu herunterladen", "download again");
    case ART_PAKET_DATEI:                            return T("endgültig weg", "gone for good");   /* NEU 1.0.8 (Befund 12) */
    case ART_JAILBREAK:                              return T("Jailbreak neu einrichten", "set up jailbreak again");
    case ART_ALTE_SICHERUNG:                         return T("nicht mehr gebraucht", "no longer needed");
    case ART_USB_KOPIE:                              return T("endgültig weg", "gone for good");   /* NEU 1.0.8 */
    case ART_PROTOKOLL:                              return T("nur zum Nachlesen", "for reference only");   /* NEU 1.0.8 */
    case ART_STORE_FASSUNG:                          return T("endgültig weg", "gone for good");   /* NEU 1.0.9 (#145) */
    default:                                         return T("neu einrichten", "set up again");
    }
}
/* Name aus "Name (PPSA12345) - Art" bzw. "Vorsatz: Name ..." - ohne Kennung, ohne Marken "  [...]" */
static const char *name_von(arena *a, const char *t, const char *vorsatz)
{
    if (vorsatz && txt_beginnt(t, vorsatz)) t += strlen(vorsatz);
    const char *m = strstr(t, "  [");
    if (m) t = a_kopie_n(a, t, (size_t)(m - t));
    int p = txt_index(t, ") - ");
    if (p >= 0) t = a_kopie_n(a, t, (size_t)p + 1);
    htreffer mn = h_mt(a, t, "^[A-Z]{4}\\d{5} \\((.+)\\)$");   /* "PPSA01326 (ASTRO PLAYROOM)" -> "ASTRO PLAYROOM" */
    if (mn.ok && strcmp(mn.gr[1], T("Spiel/App unbekannt", "game/app unknown")) != 0) t = mn.gr[1];   /* NEU 1.0.7: unbekannt -> Kennung bleibt sichtbar */
    t = r_r(a, t, " \\([A-Z]{4}\\d{5}\\)", "");
    t = r_r(a, t, " \\(NPWR\\d{5}_\\d{2}\\)", "");
    t = r_r(a, t, " - (Spiel|game) (noch da|still installed|nicht mehr da|removed).*$", "");
    t = r_r(a, t, " \\((PS4-Spiel|PS4 game)\\).*$", "");
    return h_trim(a, t);
}
/* NEU 1.0.8 (Codex-Prüfung 73): Inhalt eines Zusatzes "  [<vorsatz>...]" im Text (Spielname aus dem Abbild "  [Spiel: …]" - 2.1 - oder
 * aus dem PKG "  [PKG: …]" - 2.2). Bis hierher schnitt die Zeile alles ab "  [" ab, die gelesenen Namen kamen nie auf den Bildschirm.
 * Ende: das letzte "]" vor dem nächsten "  [" oder dem Textende. NULL = kein solcher Zusatz. */
static const char *zusatz_von(arena *a, const char *t, const char *vorsatz)
{
    const char *v = VERB(a, "  [", vorsatz), *p = strstr(t, v);
    if (!p) return NULL;
    p += strlen(v);
    const char *naechst = strstr(p, "  ["), *ende = naechst ? naechst : p + strlen(p), *z = NULL;
    for (const char *q = p; q < ende; q++) if (*q == ']') z = q;
    return z && z > p ? a_kopie_n(a, p, (size_t)(z - p)) : NULL;
}
/* NEU 1.0.8 (PS5-Test 07.11.2026 16:31): Spielname einer Spieldatei oder eines Spiel-Ordners ohne Kachel - ohne die Kennung, die die Suche
 * anhängt ("[Spiel: Marvel's Spider-Man 2 (PPSA03016)]": die Kennung braucht nur die Oberfläche für das Bild) */
static const char *spiel_von(arena *a, const char *t)
{
    const char *sp = zusatz_von(a, t, T("Spiel: ", "game: "));
    return sp ? r_r(a, sp, " \\([A-Z]{4}\\d{5}\\)$", "") : NULL;
}
/* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 12): was eine PKG-Zeile ist ("Installationsdatei", "Update-Datei 01.04", "Zusatzinhalt-Datei:
 * Bonus", "Zusatzinhalte (120 PKG-Dateien)") und unter welchem Namen sie steht (installiertes Spiel bzw. "<Spiel> · nicht installiert") */
static const char *pkg_was(arena *a, const eintrag *f)
{
    if (f->PkgAnzahl > 1)
        return a_fmt(a, f->PkgArt == PKG_UPDATE ? T("Updates (%d PKG-Dateien)", "Updates (%d PKG files)") : f->PkgArt == PKG_DLC ? T("Zusatzinhalte (%d PKG-Dateien)", "Add-ons (%d PKG files)")
                                                : T("Installationsdateien (%d PKG-Dateien)", "Install files (%d PKG files)"), f->PkgAnzahl);
    const char *v = f->PkgVer ? f->PkgVer : "";
    if (f->PkgArt == PKG_UPDATE) return VERB(a, T("Update-Datei", "Update file"), v[0] ? " " : "", v);
    if (f->PkgArt == PKG_DLC) return VERB(a, T("Zusatzinhalt-Datei", "Add-on file"), v[0] ? ": " : "", v);
    return T("Installationsdatei", "Install file");
}
static const char *pkg_block(arena *a, const eintrag *f)
{
    if (f->ToolKachel) return kern_name(a, f->ToolKachel);
    const eintrag *k = f->PkgOhneSpiel ? f : (f->Begleit && f->Begleit->PkgOhneSpiel) ? f->Begleit : NULL;
    return k && k->BlockName ? k->BlockName : NULL;
}
/* NEU 1.0.9 (#145): was ein Teil einer App in den Ordnern von ProsperoStore ist (kern_suche.c merke_store) */
static const char *store_was(const eintrag *f)
{
    const char *p = f->PfadeBasis.n > 0 ? f->PfadeBasis.e[0] : "";
    if (f->StoreTeil == STORE_BELEG) return T("Beleg von ProsperoStore", "receipt of ProsperoStore");
    if (f->StoreTeil == STORE_FASSUNG) return r_m(p, "/prosperostore/backup/") ? T("Sicherung von ProsperoStore aus einem Update", "backup of ProsperoStore from an update")
                                                                             : T("ältere Fassung, von ProsperoStore aufgehoben", "older version kept by ProsperoStore");
    return r_m(p, "/prosperostore/trash/") ? T("Rest einer Deinstallation durch ProsperoStore", "leftover of an uninstall by ProsperoStore")
                                           : T("angefangener Download von ProsperoStore", "unfinished download of ProsperoStore");
}
static const char *zeile_ohne_warnung(arena *a, const eintrag *f)
{
    const char *o = kern_ort_vorsatz(f->Ort), *t = ohne_ort(f);
    if (f->StoreTeil && !kern_bereiche_106()) {   /* NEU 1.0.9 (#145): "prosperostore/previous/PPSA99008 – ältere Fassung, ...", ohne Kachel mit dem Namen der App */
        const char *m = strstr(t, "  ["), *app = f->ToolKachel ? NULL : zusatz_von(a, t, T("App: ", "app: "));
        return VERB(a, o, m ? a_kopie_n(a, t, (size_t)(m - t)) : t, " – ", store_was(f), app ? VERB(a, " – ", r_r(a, app, " \\([A-Z]{4}[0-9]{5}\\)$", "")) : "");
    }
    switch (f->Art) {
    case ART_PAKET_DATEI: {  /* NEU 1.0.8 (Befund 12): unter seinem Spiel "<Spiel> – Update-Datei 01.04", sonst wie bisher mit dem Dateinamen */
        if (f->PkgArt && !kern_bereiche_106()) {
            const char *b = pkg_block(a, f);
            /* NEU 1.0.8 (PS5-Test 07.11.2026): Nummer nicht lesbar (steht nur wegen ihres Ordners hier) - der Dateiname sagt mehr als "Installationsdatei" */
            if (b && b[0] && !f->PkgId && f->PfadeBasis.n == 1) return VERB(a, o, b, " – ", h_blatt(a, f->PfadeBasis.e[0]));
            if (b && b[0]) return VERB(a, o, b, " – ", pkg_was(a, f));
            if (f->PkgAnzahl > 1) return VERB(a, o, f->PkgTitel && f->PkgTitel[0] ? f->PkgTitel : f->PkgId, " – ", pkg_was(a, f));
        }
        const char *m = strstr(t, "  [");
        const char *pk = zusatz_von(a, t, "PKG: ");   /* NEU 1.0.8: Spielname aus dem PKG (2.2) */
        return VERB(a, o, m ? a_kopie_n(a, t, (size_t)(m - t)) : t, pk ? VERB(a, " – PKG: ", pk) : "");
    }
    case ART_SPIEL:          return VERB(a, o, name_von(a, t, NULL), T(" – Spiel", " – game"));
    case ART_APP:            return VERB(a, o, name_von(a, t, NULL), T(" – App", " – app"));
    case ART_UPDATE:         return VERB(a, o, name_von(a, t, NULL), " – Update");
    case ART_DLC: {
        int p = txt_index(t, T(" - Zusatzinhalt: ", " - add-on: "));
        return VERB(a, o, name_von(a, t, NULL), T(" – Zusatzinhalt", " – add-on"), p >= 0 ? VERB(a, ": ", t + p + strlen(T(" - Zusatzinhalt: ", " - add-on: "))) : "");
    }
    case ART_SM_KACHEL:      return VERB(a, o, name_von(a, t, NULL), T(" – ShadowMount-Kachel", " – ShadowMount tile"));
    case ART_SM_DATEI:       return VERB(a, o, name_von(a, t, NULL), T(" – Spieldatei", " – game file"));
    case ART_SM_TOT:         return VERB(a, T("Kachel ohne Spiel: ", "Tile without game: "), name_von(a, t, NULL));
    case ART_ABBILD: {       /* NEU 1.0.8: Spielname aus dem Abbild (2.1) vorne, der Dateiname dahinter */
        const char *sp = spiel_von(a, t);
        return sp ? VERB(a, o, T("Spieldatei ohne Kachel: ", "Game file without tile: "), sp, " (", name_von(a, t, NULL), ")")
                  : VERB(a, o, T("Spieldatei ohne Kachel: ", "Game file without tile: "), name_von(a, t, NULL));
    }
    case ART_SPIELSTAND:     return VERB(a, name_von(a, t, T("Spielstand ", "Saved data ")), T(" – Spielstände", " – saved data"));
    case ART_TR_FORTSCHRITT: return VERB(a, name_von(a, t, T("Trophäen-Fortschritt: ", "Trophy progress: ")), T(" – dein Trophäen-Fortschritt", " – your trophy progress"),
                                         f->ListeNr > 0 ? a_fmt(a, T(" (Liste %d)", " (list %d)"), f->ListeNr) : "",
                                         zusatz_von(a, t, T("Kopie: ", "copy: ")) ? T(" (Kopie)", " (copy)") : "");   /* NEU 1.0.8: "<Liste>_BK" */
    case ART_TR_ZAEHLER:     return T("Trophäen-Gesamtzähler", "Trophy totals");
    case ART_TR_LISTE:
    case ART_TR_LISTE_REST:  return VERB(a, name_von(a, t, T("Trophäen-Liste: ", "Trophy list: ")), T(" – Trophäen-Liste", " – trophy list"),
                                         f->ListeNr > 0 ? a_fmt(a, " %d", f->ListeNr) : "");   /* NEU 1.0.7 (02.11.2026): mehrere Listen eines Spiels */
    case ART_TOOL_KACHEL:    return VERB(a, name_von(a, t, T("Tool-Kachel: ", "Tool tile: ")), T(" – Kachel", " – tile"));
    case ART_SPIEL_SICHERUNG: { const char *m = strstr(t, "  ["); return VERB(a, o, T("Spiel-Sicherung: ", "Game backup: "), m ? a_kopie_n(a, t, (size_t)(m - t)) : t); }
    case ART_JAILBREAK: {
        const char *w = kern_jb_weg_eintrag(f);
        const char *n = txt_beginnt(t, T("Spielstand ", "Saved data ")) ? VERB(a, name_von(a, t, T("Spielstand ", "Saved data ")), T(" – Spielstand", " – saved data"))
                      : txt_beginnt(t, T("Browserspeicher", "Browser storage")) ? T("Browserspeicher", "Browser storage")
                      /* NEU 1.0.8 (Befund 14): die Kachel eines Jailbreak-Tools wie jede Tool-Kachel ("WebKit Autoloader v0.4.0 – Kachel") - sie ist
                       * jetzt Kopf eines Blocks mit ihren ELF-Dateien, bis hier stand "Tool-Kachel: " im Namen */
                      : (!kern_bereiche_106() && txt_beginnt(t, T("Tool-Kachel: ", "Tool tile: "))) ? VERB(a, name_von(a, t, T("Tool-Kachel: ", "Tool tile: ")), T(" – Kachel", " – tile"))
                      : txt_beginnt(t, T("App-Daten eines Jailbreak-Einstiegs: ", "App data of a jailbreak entry: ")) ? VERB(a, name_von(a, t, T("App-Daten eines Jailbreak-Einstiegs: ", "App data of a jailbreak entry: ")), T(" – App-Daten", " – app data"))
                      : f->SmDatei ? VERB(a, name_von(a, t, NULL), T(" – Spieldatei", " – game file"))
                      : (f->Quelle && f->Quelle[0]) ? VERB(a, name_von(a, t, NULL), T(" – ShadowMount-Kachel", " – ShadowMount tile"))
                      : name_von(a, r_r(a, t, " - (Spiel|game)(  \\[.*)?$", ""), NULL);
        return VERB(a, o, n, w ? VERB(a, T(" – Jailbreak-Start: ", " – jailbreak start: "), w) : T(" – Jailbreak-Start", " – jailbreak start"));
    }
    case ART_TOOL_DATEN: case ART_TOOL_DATEN_WICHTIG: {
        arena *b = arena_neu();
        const char *dn = daten_ordner(b, f), *w = f->WichtigLeer ? NULL : wichtig_von(f, dn);   /* NEU 1.0.8 (Befund 2): leer - nur "Einstellungen" */
        /* NEU 1.0.7 (03.11.2026): eine einzelne Datei des Tools (whitelist.txt, ps5upload_startup.log) heißt einfach wie sie heißt */
        bool datei = dn && h_m(dn, "\\.[A-Za-z0-9]{1,8}$");
        const char *r = datei ? (w ? VERB(a, a_kopie(a, dn), " – ", a_kopie(a, w)) : a_kopie(a, dn))
                      : dn ? VERB(a, a_kopie(a, dn), w ? VERB(a, T(" – Einstellungen und ", " – settings and "), a_kopie(a, w)) : T(" – Einstellungen", " – settings"))
                      : VERB(a, o, name_von(a, t, "   – "));
        /* 1.0.9 (PS5-Test 09.11.2026 19:39): /data/user des Patch Managers - der Ordnername "user" sagt nichts, Einstellungen liegen darin
         * keine (nur patches und patch_backup): nur, was es ist */
        if (dn && !datei && !strcmp(dn, "user") && w) r = T("Deine Spiel-Patches und ihre Sicherungen", "Your game patches and their backups");
        const char *pp = f->ToolOrdner ? f->ToolOrdner : f->PfadeBasis.n > 0 ? f->PfadeBasis.e[0] : "";
        const char *app = (dn && !f->ToolKachel && r_m(pp, "^/user/data/[^/]+$")) ? kern_bekannte_app(dn, NULL, NULL) : NULL;   /* NEU 1.0.9 (#146): ohne ihre Kachel - wem es gehört (nur /data/<Ordner>, Codex-Prüfung 137) */
        if (app) r = VERB(a, r, T(" – gehört zu ", " – belongs to "), app);
        arena_frei(b);
        return r;
    }
    case ART_USB_KOPIE: {    /* NEU 1.0.8 (PS5-Test 07.11.2026 22:53): was darin liegt ("USB-Laufwerk – PS5 – deine Screenshots und Videos") */
        const char *m = strstr(t, "  [");
        return VERB(a, o, m ? a_kopie_n(a, t, (size_t)(m - t)) : t, T(" – deine ", " – your "), usb_kopie_was(a, f));
    }
    default: {
        const char *m = strstr(t, "  [");
        const char *pk = f->Art == ART_PAKET_DATEI ? zusatz_von(a, t, "PKG: ") : NULL;   /* NEU 1.0.8: Spielname aus dem PKG (2.2) */
        return VERB(a, o, m ? a_kopie_n(a, t, (size_t)(m - t)) : t, pk ? VERB(a, " – PKG: ", pk) : "");
    }
    }
}

/* Kasten unter der Liste: "Was passiert danach?" mit den Recherche-Ergebnissen (Textliste, Abschnitt "Was die Recherche
 * zusätzlich ergeben hat"). Belege: doku/recherche_*_2026-10-31.md */
/* NEU 1.0.7 (PS5-Test 02.11.2026, Red Dead Redemption 2): kaputte Zusatzinhalt-Angaben deutlich in Zeile und Erklärung */
/* NEU 1.0.8 (Arbeitsplan 2.4): dasselbe Spiel mehr als einmal auf der Konsole (kern_suche.c doppelte_spiele) - kurz, wo die anderen Kopien liegen */
static const char *ort_dativ(int ort)
{
    switch (ort) { case ORT_M2: return T("auf der M.2-SSD", "on the M.2 SSD"); case ORT_USB: return T("auf dem erweiterten Speicher", "on the extended storage");
                   case ORT_STICK: return T("auf dem USB-Laufwerk", "on the USB drive"); }
    return T("auf der internen SSD", "on the internal SSD");
}
const char *kern_doppelt_text(arena *a, const eintrag *f)
{
    if (f->Doppelt.n == 0 || kern_bereiche_106()) return "";
    const char *t = "", *w[8]; int nw = 0;
    for (int i = 0; i < f->Doppelt.n && nw < 8; i++) {
        const eintrag *o = f->Doppelt.e[i];
        bool inst = ist_teil(o, "Spiel") && !(o->Quelle && o->Quelle[0]);
        const char *x = o->PkgArt && o->Ort == f->Ort ? T("auch auf einem anderen USB-Laufwerk", "also on another USB drive")   /* Codex-Prüfung 83: zwei USB-Laufwerke */
                      : o->PkgArt ? VERB(a, T("auch ", "also "), ort_dativ(o->Ort))   /* NEU 1.0.8 (Befund 12): dieselbe PKG-Datei auf einem anderen Speicher */
                      : VERB(a, inst ? T("auch installiert ", "also installed ") : T("auch als Spieldatei ", "also as game file "), ort_dativ(o->Ort));
        bool schon = false; for (int j = 0; j < nw; j++) if (!strcmp(w[j], x)) schon = true;
        if (!schon) { w[nw++] = x; t = VERB(a, t, t[0] ? ", " : "", x); }
    }
    return VERB(a, T(" – doppelt: ", " – duplicate: "), t);
}
const char *kern_zeile(arena *a, const eintrag *f)
{
    const char *z = VERB(a, zeile_ohne_warnung(a, f), kern_doppelt_text(a, f));
    return f->SidKaputt > 0 ? VERB(a, z, T(" – ACHTUNG: kaputte Zusatzinhalt-Angaben", " – WARNING: broken add-on entries")) : z;
}
/* NEU 1.0.8 (Arbeitsplan 2.6): nur der Name des Spiels, der App oder des Tools - für A–Z und die zugeklappte Zeile einer App in der neuen
 * Auswahl ("Orbit Store", nicht "Orbit Store – Kachel"). Ohne Ort vorne; was keinen eigenen Namen hat, heißt wie seine Zeile. */
const char *kern_name(arena *a, const eintrag *f)
{
    const char *t = ohne_ort(f);
    switch (f->Art) {
    case ART_ABBILD: { const char *sp = spiel_von(a, t); return sp ? sp : name_von(a, t, NULL); }
    case ART_PAKET_DATEI: {  /* der Name aus dem PKG ohne Nummer und Art ("Spiel Eins (CUSA30001), Update 01.05" -> "Spiel Eins") */
        if (f->PkgArt && !kern_bereiche_106()) {   /* NEU 1.0.8 (Befund 12): der Name des Blocks, in dem die Datei steht */
            const char *b = pkg_block(a, f);
            if (b && b[0]) return b;
            if (f->PkgAnzahl > 1) return f->PkgTitel && f->PkgTitel[0] ? f->PkgTitel : f->PkgId;
        }
        const char *pk = zusatz_von(a, t, "PKG: ");
        if (pk) { const char *k = r_r(a, pk, " \\([A-Z]{4}\\d{5}\\)(, .*)?$", ""); k = r_r(a, k, ", (Update|update|Zusatzinhalt|add-on).*$", ""); if (k[0]) return h_trim(a, k); }
        break;
    }
    case ART_SPIEL: case ART_APP: case ART_UPDATE: case ART_DLC: case ART_SM_KACHEL: case ART_SM_DATEI: case ART_SM_TOT:
        return name_von(a, t, NULL);
    case ART_SPIELSTAND:     return name_von(a, t, T("Spielstand ", "Saved data "));
    case ART_TR_FORTSCHRITT: return name_von(a, t, T("Trophäen-Fortschritt: ", "Trophy progress: "));
    case ART_TR_LISTE:
    case ART_TR_LISTE_REST:  return name_von(a, t, T("Trophäen-Liste: ", "Trophy list: "));
    case ART_TOOL_KACHEL:    return name_von(a, t, T("Tool-Kachel: ", "Tool tile: "));
    case ART_JAILBREAK:      /* NEU 1.0.8 (4.3): ohne " – Spielstand" und " – Jailbreak-Start: …" - das steht in der Zeile */
        if (txt_beginnt(t, T("Spielstand ", "Saved data "))) return name_von(a, t, T("Spielstand ", "Saved data "));
        if (txt_beginnt(t, T("Browserspeicher", "Browser storage"))) return T("Browserspeicher", "Browser storage");
        if (!kern_bereiche_106() && txt_beginnt(t, T("Tool-Kachel: ", "Tool tile: "))) return name_von(a, t, T("Tool-Kachel: ", "Tool tile: "));   /* NEU 1.0.8 (Befund 14) */
        if (txt_beginnt(t, T("App-Daten eines Jailbreak-Einstiegs: ", "App data of a jailbreak entry: "))) return name_von(a, t, T("App-Daten eines Jailbreak-Einstiegs: ", "App data of a jailbreak entry: "));
        if (f->SmDatei || (f->Quelle && f->Quelle[0])) return name_von(a, t, NULL);
        return name_von(a, r_r(a, t, " - (Spiel|game)(  \\[.*)?$", ""), NULL);
    default: break;
    }
    const char *z = zeile_ohne_warnung(a, f), *o = kern_ort_vorsatz(f->Ort);
    return (o[0] && txt_beginnt(z, o)) ? z + strlen(o) : z;
}
static const char *art_info_basis(arena *a, const eintrag *f);
const char *kern_art_info(arena *a, const eintrag *f)
{
    const char *i = art_info_basis(a, f);
    if (f->PkgOffen && !kern_bereiche_106())   /* Codex-Prüfung 90 */
        i = VERB(a, i ? i : "", i && i[0] ? "\n\n" : "",
                 T("In diesem Ordner konnte PS5 Cleanup nicht nach PKG-Dateien suchen (zu viele Dateien, zu tief verschachtelt oder nicht lesbar). Er kann PKG-Dateien enthalten - darum nur einzeln wählbar, wie eine PKG-Datei. Gelöscht ist alles darin endgültig weg.",
                   "PS5 Cleanup could not look for PKG files in this folder (too many files, nested too deep or not readable). It may contain PKG files - so it can only be selected individually, like a PKG file. Once deleted, everything in it is gone for good."));
    if (f->Doppelt.n > 0 && !kern_bereiche_106())   /* NEU 1.0.8 (Arbeitsplan 2.4) */
        i = VERB(a, i ? i : "", i && i[0] ? "\n\n" : "", f->PkgArt
                 ? T("Doppelt: Dieselbe Datei (gleicher Name, gleiche Größe) liegt auch auf einem anderen Speicher (wo, steht in der Zeile). Löschst du nur eine davon, bleibt die andere.",
                     "Duplicate: the same file (same name, same size) is also on another storage (where is shown in the row). If you delete only one of them, the other one stays.")
                 : T("Doppelt: Dieses Spiel liegt mehr als einmal auf der Konsole und belegt den Platz mehrfach (wo, steht in der Zeile). Löschst du nur eine Kopie, bleibt die andere.",
                     "Duplicate: this game is on the console more than once and takes up the space several times (where is shown in the row). If you delete only one copy, the other one stays."));
    if (f->SidKaputt <= 0) return i;
    return VERB(a, i ? i : "", T("\n\nACHTUNG: Dieses Spiel hat kaputte Angaben zu Zusatzinhalten. Daran kann die PS5 abstürzen und sich ausschalten, wenn sie im Hintergrund die Spiele durchgeht - auch wenn das Spiel gar nicht läuft. Abhilfe: das Spiel mit PS5 Cleanup löschen (die kaputten Angaben werden vorher repariert, sonst bricht Sonys Deinstallation ab) und mit einem unveränderten Update neu installieren.",
                                 "\n\nWARNING: This game has broken add-on entries. This can make the PS5 crash and switch off when it goes through the games in the background - even if the game is not running. Fix: delete the game with PS5 Cleanup (the broken entries are repaired first, otherwise Sony's uninstall aborts) and reinstall it with an unmodified update."));
}
static const char *art_info_basis(arena *a, const eintrag *f)
{
    if (f->StoreTeil && !kern_bereiche_106())   /* NEU 1.0.9 (#145) */
        return f->StoreTeil == STORE_FASSUNG
            ? T("Die vorige Fassung dieser App samt allem, was darin lag - auch Dateien, die du selbst hineingelegt hattest (z. B. Spiele oder BIOS-Dateien eines Emulators). ProsperoStore hebt sie bei einem Update auf; die App selbst braucht sie nicht. Gelöscht ist sie endgültig weg.",
                "The previous version of this app with everything that was in it - including files you put there yourself (e.g. games or BIOS files of an emulator). ProsperoStore keeps it on an update; the app itself does not need it. Once deleted, it is gone for good.")
            : f->StoreTeil == STORE_BELEG
            ? T("Der Beleg, mit dem sich ProsperoStore merkt, dass er diese App installiert hat. Er gehört zur App - ohne die App braucht ihn niemand. Es geht nichts verloren.",
                "The receipt ProsperoStore uses to remember that it installed this app. It belongs to the app - without the app nobody needs it. Nothing is lost.")
            : T("Ein Rest von ProsperoStore: ein angefangener Download oder eine App, die er nicht fertig gelöscht hat. Es geht nichts verloren.",
                "A leftover of ProsperoStore: an unfinished download or an app it did not finish deleting. Nothing is lost.");
    switch (f->Art) {
    case ART_SPIEL: return T("Spiel und Kachel samt Update und Zusatzinhalten - immer alles zusammen, wie beim Löschen im PS5-Menü. Spielstände, Trophäen und Aufnahmen bleiben. Neu installieren geht über den PS Store (nur mit PSN-Zugang) oder mit der Installationsdatei - selbst installierte PS5-Spiele starten danach nur bis Firmware 11.60.",
                             "Game and tile with update and add-ons - always together, like deleting in the PS5 menu. Saved data, trophies and captures stay. Reinstall via the PS Store (PSN access only) or with the install file - self-installed PS5 games only start up to firmware 11.60.");
    case ART_APP: return T("App und Kachel samt ihren Daten. Neu laden geht nur über den PlayStation Store, also nur mit PSN-Zugang. Danach musst du dich in der App neu anmelden.",
                           "App and tile with their data. Downloading again only works via the PlayStation Store, so only with PSN access. Afterwards you have to sign in to the app again.");
    case ART_UPDATE: case ART_DLC: return T("Geht nur zusammen mit dem Spiel weg - ob das Spiel ohne Update oder Zusatzinhalt noch startet, ist nicht sicher.",
                                            "Goes only together with the game - it is not certain the game still starts without its update or add-on.");
    case ART_SM_KACHEL: return T("Nur die Kachel. Die Spieldatei bleibt - ShadowMount legt die Kachel beim nächsten Start wieder an, solange die Spieldatei in einem seiner Suchordner liegt.",
                                 "Only the tile. The game file stays - ShadowMount creates the tile again on its next start as long as the game file is in one of its scan folders.");
    case ART_SM_DATEI: return T("Die Spieldatei - das ist das Spiel selbst. Die Kachel geht automatisch mit. Danach endgültig weg: zum Spielen bräuchtest du die Datei wieder.",
                                "The game file - this is the game itself. The tile goes with it automatically. Gone for good afterwards: to play you would need the file again.");
    case ART_SM_TOT: return T("Nur die Kachel, die Spieldatei wird nie angefasst. Ist die Spieldatei wieder erreichbar, legt ShadowMount die Kachel beim nächsten Start wieder an.",
                              "Only the tile, the game file is never touched. Once the game file is reachable again, ShadowMount creates the tile again on its next start.");
    case ART_ABBILD: return T("Das ist kein Rest, sondern dein Spiel: Liegt die Datei in einem Suchordner von ShadowMount, wird beim nächsten Start wieder ein spielbares Spiel daraus. Gelöscht ist sie endgültig weg.",
                              "This is not a leftover but your game: if the file is in a ShadowMount scan folder, it becomes a playable game again on the next start. Once deleted, it is gone for good.");
    case ART_SPIELSTAND: return T("Alle Spielstände dieses Spiels auf der Konsole. Endgültig weg, außer du hast sie gesichert (PS Plus Cloud oder USB).",
                                  "All saved data of this game on the console. Gone for good unless you backed it up (PS Plus cloud or USB).");
    case ART_TR_FORTSCHRITT: return T("Welche Trophäen du in diesem Spiel hast - die einzige Kopie auf der Konsole. Was schon mit dem PSN abgeglichen war, bleibt in deinem Konto; ob es zurück auf die Konsole kommt, ist nicht sicher.",
                                      "Which trophies you have in this game - the only copy on the console. Anything already synced with PSN stays in your account; whether it comes back to the console is not certain.");
    case ART_TR_ZAEHLER: return T("Nur die Zusammenfassung (Level, Bronze/Silber/Gold). Zahlen und Level auf der Konsole können sich danach ändern oder falsch angezeigt werden. Dein Fortschritt je Spiel bleibt.",
                                  "Only the summary (level, bronze/silver/gold). Numbers and level on the console may change or show wrongly afterwards. Your progress per game stays.");
    case ART_TR_LISTE: return T("Nur Namen und Bilder der Trophäen, nicht dein Fortschritt. Die PS5 legt sie beim Spielstart an; bei selbst kopierten (gedumpten) Spielen klappt das eventuell nicht.",
                                "Only names and images of the trophies, not your progress. The PS5 creates them when the game starts; with self-copied (dumped) games this may not work.");
    case ART_AUFNAHMEN: return T("Alle Screenshots und Videos dieses Spiels samt Vorschaubildern. Endgültig weg, außer du hast sie auf PC, Handy oder USB gesichert.",
                                 "All screenshots and videos of this game with thumbnails. Gone for good unless you saved them to PC, phone or USB.");
    case ART_AUFNAHME_UNFERTIG: return T("Das Speichern wurde abgebrochen. Normale Player können solche Videos nicht abspielen; retten ginge höchstens mit Spezialprogrammen.",
                                         "Saving was interrupted. Normal players cannot play such videos; rescuing would only be possible with special tools.");
    case ART_TOOL_DATEN:
        if (f->WichtigLeer)   /* NEU 1.0.8 (Befund 2) */
            return T("Einstellungen des Tools. Geprüft: Darin liegt keine Datei mit Inhalt - es geht also nichts Eigenes verloren. Das Tool legt den Ordner beim nächsten Start neu an.",
                     "Settings of the tool. Checked: there is no file with content in it - so nothing of your own is lost. The tool recreates the folder on its next start.");
        return T("Einstellungen, Listen und Protokolle des Tools. Das Tool legt den Ordner beim nächsten Start neu an, mit Grundeinstellungen.",
                 "Settings, lists and logs of the tool. The tool recreates the folder on its next start, with default settings.");
    case ART_TOOL_DATEN_WICHTIG: {
        arena *b = arena_neu();
        const char *dn = daten_ordner(b, f), *w = wichtig_von(f, dn);
        const char *fund = (dn && !wichtig_was(dn) && f->WichtigFund) ? VERB(a, T(" (gefunden: ", " (found: "), f->WichtigFund, ")") : "";
        const char *r = VERB(a, T("Achtung: Hier liegen ", "Warning: this holds "), w ? a_kopie(a, w) : T("eigene Sicherungen", "own backups"), fund,
                             T(" - die sind danach endgültig weg. Die Einstellungen des Tools legt es beim nächsten Start neu an.", " - they are gone for good afterwards. The tool recreates its settings on its next start."));
        arena_frei(b);
        return r;
    }
    case ART_FAKE_LIZENZ: return T("Die Lizenz eines selbst installierten Spiels, das nicht mehr da ist. Installierst du es neu, bringt das Paket sie wieder mit. Sie gilt auch für die Zusatzinhalte dieses Spiels. Echte Lizenzen fasst PS5 Cleanup nie an.",
                                   "The license of a self-installed game that is no longer there. If you reinstall it, the package brings it back. It also covers this game's add-ons. PS5 Cleanup never touches real licenses.");
    case ART_APP_DATEN: return T("Daten, die eine App nachgeladen hat - die App selbst ist weg. Was genau darin steckt, weiß nur die App.",
                                 "Data a app downloaded - the app itself is gone. Only the app knows exactly what is in it.");
    case ART_ALTE_SICHERUNG: return T("Kopien der Kachel-Datenbank, die ältere Versionen von PS5 Cleanup vor dem Löschen angelegt haben. Die jetzige Version legt keine mehr an und braucht sie nicht. Gelöscht sind sie endgültig weg.",
                                      "Copies of the tile database that older versions of PS5 Cleanup made before deleting. The current version no longer makes or needs them. Once deleted, they are gone for good.");
    case ART_SPIEL_SICHERUNG: return T("Die Originale deiner Spiele, die der LZ4-Game-Compressor vor dem Packen gesichert hat. Danach endgültig weg.",
                                       "The originals of your games that LZ4-Game-Compressor backed up before packing. Gone for good afterwards.");
    case ART_JAILBREAK: return T("Darüber startet dein Jailbreak. Danach startet er über diesen Weg nicht mehr - du musst ihn neu einrichten, mit PC und der passenden App-Version.",
                                 "Your jailbreak starts through this. Afterwards it no longer starts this way - you have to set it up again, with a PC and the matching app version.");
    case ART_USB_KOPIE:   /* NEU 1.0.8 (PS5-Test 07.11.2026 22:53, da_Chris86s USB-Laufwerk: PS5/CREATE stand als "neu herunterladen" da) */
        return VERB(a, T("Diesen Ordner legt die PlayStation an, wenn du etwas auf ein USB-Laufwerk kopierst. Darin: deine ", "The PlayStation creates this folder when you copy something to a USB drive. It holds your "),
                    usb_kopie_was(a, f), T(". Gelöscht ist das endgültig weg, außer es liegt noch auf der Konsole oder woanders.", ". Once deleted, it is gone for good unless it is still on the console or somewhere else."));
    case ART_PROTOKOLL:   /* NEU 1.0.8 (07.11.2026) */
        return T("Ein Protokoll von PS5 Cleanup aus einem früheren Lauf. Das Programm braucht es nicht mehr. Gelöscht ist es weg - brauchst du es noch, um einen Fehler zu melden, lass es stehen.",
                 "A log of PS5 Cleanup from an earlier run. The program does not need it anymore. Once deleted, it is gone - if you still need it to report a problem, keep it.");
    case ART_PAKET_DATEI: {   /* NEU 1.0.8 (PS5-Test 06.11.2026, Befund 12) */
        if (kern_bereiche_106() || !f->PkgArt) return "";
        const char *n = f->PkgAnzahl > 1 ? a_fmt(a, T("Das sind %d PKG-Dateien auf diesem Speicher. ", "These are %d PKG files on this storage. "), f->PkgAnzahl) : "";
        if (f->ToolKachel) {
            const char *sp = kern_name(a, f->ToolKachel);
            const char *was = f->PkgArt == PKG_UPDATE ? T("Update-Datei", "update file") : f->PkgArt == PKG_DLC ? T("Zusatzinhalt-Datei", "add-on file") : T("Installationsdatei", "install file");
            if (!f->PkgId)   /* NEU 1.0.8 (PS5-Test 07.11.2026): Nummer nicht lesbar - sie steht nur wegen ihres Ordners hier (kachel_bloecke) */
                return VERB(a, n, T("Eine Installationsdatei (PKG) im Ordner von ", "An install file (PKG) in the folder of "), sp,
                            T(". Was genau darin steckt, ließ sich nicht lesen. Was installiert ist, bleibt - gelöscht ist die Datei endgültig weg.",
                              ". What exactly it contains could not be read. What is installed stays - once deleted, the file is gone for good."));
            if (f->PkgSpielFehlt) {   /* NEU 1.0.8 (PS5-Test 07.11.2026): davon ist nur ein Rest da - die Datei ist das Einzige zum Neuinstallieren */
                if (f->PkgAnzahl > 1)
                    return VERB(a, n, T("Von ", "Only leftovers of "), sp,
                                T(" ist gerade nur ein Rest installiert (Kachel ohne Daten). Zum Neuinstallieren bräuchtest du diese Dateien - gelöscht sind sie endgültig weg.",
                                  " are installed right now (a tile without data). To reinstall you would need these files - once deleted, they are gone for good."));
                return VERB(a, T("Eine ", "An "), was, T(" von ", " of "), sp, ". ",
                            T("Installiert ist davon gerade nur ein Rest (Kachel ohne Daten). Zum Neuinstallieren bräuchtest du diese Datei - gelöscht ist sie endgültig weg.",
                              "Only leftovers of it are installed right now (a tile without data). To reinstall you would need this file - once deleted, it is gone for good."));
            }
            return VERB(a, n, f->PkgAnzahl > 1 ? "" : VERB(a, T("Eine ", "An "), was, T(" von ", " of "), sp, ". "),
                        T("Was installiert ist, bleibt - die Datei braucht nur, wer neu installieren will. Gelöscht ist sie endgültig weg.",
                          "What is installed stays - the file is only needed to reinstall. Once deleted, it is gone for good."));
        }
        const eintrag *kk = f->PkgOhneSpiel ? f : f->Begleit;
        if (kk && kk->PkgOhneSpiel)
            return VERB(a, n, T("Dieses Spiel ist gerade nicht installiert. Seine PKG-Dateien sind alles, was davon auf der Konsole liegt - gelöscht ist es ganz weg von der Konsole. Zum Spielen bräuchtest du die Dateien wieder.",
                                "This game is not installed right now. Its PKG files are all there is of it on the console - once deleted, it is gone from the console completely. To play you would need the files again."));
        return VERB(a, n, T("Eine Installationsdatei (PKG). Zu welchem Spiel sie gehört, ließ sich nicht lesen. Gelöscht ist sie endgültig weg - zum Installieren bräuchtest du sie wieder.",
                            "An install file (PKG). Which game it belongs to could not be read. Once deleted, it is gone for good - to install you would need it again."));
    }
    default: return "";
    }
}

/* Update/Zusatzinhalt eines noch installierten Spiels (da_Chris86 31.10.2026 12:49: "wenn ein Spiel gelöscht wird, dann alles samt Inhalt") */
bool kern_nur_mit_spiel(kern *k, const eintrag *f)
{
    if (kern_bereiche_106() || !f->Teil || !strcmp(f->Teil, "Spiel") || !f->Gruppe) return false;
    FUER_ALLE(s) if (s != f && ist_teil(s, "Spiel") && s->Gruppe && txt_gleich_ci(s->Gruppe, f->Gruppe)) return true;
    return false;
}

/* "Danach endgültig weg: 12 Spielstände, 3 Spieldateien, ..." - nur was gewählt UND rot ist */
const char *kern_endgueltig_satz(kern *k, arena *a)
{
    int spst = 0, datei = 0, trf = 0, trd = 0, aufn = 0, gal = 0, sich = 0, jb = 0, pkg = 0, kopie = 0, fass = 0;
    sliste wichtig = {0};
    FUER_ALLE(f) {
        if (!f->Wahl || f->Gesperrt) continue;
        switch (f->Art) {
        case ART_PAKET_DATEI: if (!kern_bereiche_106()) pkg += f->PkgAnzahl > 0 ? f->PkgAnzahl : 1; break;   /* NEU 1.0.8 (Befund 12) */
        case ART_SPIELSTAND: spst++; break;
        case ART_SM_DATEI: case ART_ABBILD: datei++; break;
        case ART_TR_FORTSCHRITT: trf++; break;
        case ART_TR_DATEN: trd++; break;
        case ART_AUFNAHMEN: aufn++; break;
        case ART_GALERIE_SONST: gal++; break;
        case ART_USB_KOPIE: kopie++; break;   /* NEU 1.0.8 (PS5-Test 07.11.2026 22:53) */
        case ART_STORE_FASSUNG: fass++; break;   /* NEU 1.0.9 (#145) */
        case ART_SPIEL_SICHERUNG: sich++; break;
        case ART_JAILBREAK: jb++; break;
        case ART_TOOL_DATEN_WICHTIG: {
            const char *dn = daten_ordner(a, f), *w = wichtig_von(f, dn);
            if (dn && w) sl_add(a, &wichtig, VERB(a, mit_deine(a, w), " (", dn, ")"));
            break;
        }
        default: break;
        }
        /* ShadowMount: Spiel + Kachel (vollständige Bereinigung, Speicher wird geleert) zählt als Spieldatei */
        if (f->Art == ART_SM_KACHEL && f->Quelle[0]) for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], f->Quelle)) {
            bool zeile = false; FUER_ALLE(d) if (d->SmDatei && d->SmKachel == f && d->Wahl) zeile = true;
            if (!zeile) datei++;
            break;
        }
    }
    sliste t = {0};
    #define TEIL(n, e1, eN, en1, enN) if (n > 0) sl_add(a, &t, VERB(a, a_fmt(a, "%d", n), " ", n == 1 ? T(e1, en1) : T(eN, enN)))
    TEIL(spst, "Spielstand (Spiel)", "Spielstände (Spiele)", "saved-data set", "saved-data sets");
    TEIL(datei, "Spieldatei", "Spieldateien", "game file", "game files");
    TEIL(trf, "Trophäen-Fortschritt", "Trophäen-Fortschritte", "trophy progress", "trophy progresses");
    TEIL(trd, "Trophäen-Datei", "Trophäen-Dateien", "trophy data item", "trophy data items");
    TEIL(aufn, "Spiel mit Aufnahmen", "Spiele mit Aufnahmen", "game with captures", "games with captures");
    TEIL(gal, "Galerie-Eintrag", "Galerie-Einträge", "gallery item", "gallery items");
    TEIL(pkg, "Installationsdatei (PKG)", "Installationsdateien (PKG)", "install file (PKG)", "install files (PKG)");
    TEIL(kopie, "Ordner mit deinen Daten auf dem USB-Laufwerk", "Ordner mit deinen Daten auf dem USB-Laufwerk", "folder with your data on the USB drive", "folders with your data on the USB drive");
    TEIL(fass, "ältere Fassung einer App (ProsperoStore)", "ältere Fassungen von Apps (ProsperoStore)", "older version of an app (ProsperoStore)", "older versions of apps (ProsperoStore)");   /* NEU 1.0.9 (#145) */
    #undef TEIL
    if (sich > 0) sl_add(a, &t, T("deine Spiel-Sicherungen (LZ4-Game-Compressor)", "your game backups (LZ4-Game-Compressor)"));
    for (int i = 0; i < wichtig.n; i++) sl_add(a, &t, wichtig.e[i]);
    const char *s = t.n ? VERB(a, T("Danach endgültig weg: ", "Gone for good afterwards: "), sl_join(a, &t, ", "), ".") : "";
    if (jb > 0) s = VERB(a, s, s[0] ? " " : "", T("Dein Jailbreak muss danach neu eingerichtet werden.", "Your jailbreak has to be set up again afterwards."));
    return s;
}
