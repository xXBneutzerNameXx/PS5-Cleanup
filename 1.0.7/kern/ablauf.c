/* PS5 Cleanup 1.1 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * ablauf.c - Bedien-Abläufe 1:1 aus Fenster.cs (1.0.5): Starte, Suchen (aus Verbinde),
 * KomplettAblauf, AuswahlAblauf, Zusammenfassung, GroesseErmitteln, LoescheJetzt,
 * Kästchen-Logik (ZeigeBereiche/GruppenAbgleich), KopfText, AktualisiereStand,
 * HintergrundStarten/-Anhalten. Texte wortgleich.
 *
 * NEU 1.1 (nur Anzeige):
 *  - Simulation (KN->simulation): überall deutlich "SIMULATION", "würde", nie "gelöscht".
 *    Nach der Simulation zeigt die Liste wieder den echten Stand (frische Simulationsschicht).
 *  - Selbstschutz-Hinweis in den Fragen; PS5 Cleanup entfernt sich am Ende selbst (vollständig: immer, Auswahl: auf Nachfrage).
 *  - Anzeige-Texte im eigenen Speicher (a->ta), weil die Oberfläche jedes Bild neu zeichnet.
 */
#include "ablauf.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
static bool sl_enthaelt(const sliste *l, const char *s) { for (int i = 0; i < l->n; i++) if (strcmp(l->e[i], s) == 0) return true; return false; }  /* NEU 1.0.6: wie in kern_suche.c */

#define KN (a->K)
#define AR (a->K->a)
#define TA (a->ta)
#define FUER_ALLE(f) for (int _i = 0; _i < KN->Funde.n; _i++) for (eintrag *f = KN->Funde.e[_i]; f; f = NULL)

static void logz(ablauf *a, const char *t) { if (a->ui.log) a->ui.log(a->ui.ctx, t); }
static void stand(ablauf *a, const char *t, double p) { if (a->ui.stand) a->ui.stand(a->ui.ctx, t, p); }
static void liste_neu(ablauf *a) { if (a->ui.liste_neu) a->ui.liste_neu(a->ui.ctx); }
static bool frage_wahl(ablauf *a, const char *text, const char *titel, const char *sicher, const char *gefaehrlich)
{ return a->ui.frage_wahl ? a->ui.frage_wahl(a->ui.ctx, text, titel, sicher, gefaehrlich) : false; }
static const char *frage_text(ablauf *a, const char *text, const char *titel) { return a->ui.frage_text ? a->ui.frage_text(a->ui.ctx, text, titel) : NULL; }
static const char *zahl(arena *ar, long long n) { return a_fmt(ar, "%lld", n); }   /* string + int in C# (ohne Trenner) */
/* NEU 1.1 (U11): Einzahl - "1 Eintrag" statt "1 Einträge" wie in 1.0.5 */
static const char *eintraege(long long n) { return n == 1 ? T(" Eintrag", " entry") : T(" Einträge", " entries"); }
/* NEU 1.0.7 (PS5-Test 03.11.2026, da_Chris86 beim Fenster "Vollständige Bereinigung": "Hier kann das nicht stimmen"): Seit "ein Block pro
 * Kachel" stehen Daten, Updates und Zusatzinhalte als eigene Zeilen unter ihrer Kachel - gezählt wurden Zeilen ("Apps: 22" bei rund 13 Apps).
 * Jetzt zählt jeder Block (Kachel samt allem darunter) einmal; die Zahl der Zeilen steht dahinter, wenn sie abweicht. */
static const char *block_schluessel(arena *ar, const eintrag *f)
{
    if (f->SmDatei && f->SmKachel) f = f->SmKachel;   /* Codex-Prüfung 48: Spieldatei gehört zum Block ihrer ShadowMount-Kachel */
    for (int i = 0; f->ToolKachel && i < 8; i++) f = f->ToolKachel;
    /* Codex-Prüfung 109 (P3): eine ShadowMount-Kachel im Block ihrer gleichnamigen App- oder Tool-Kachel (Begleit, kern_suche.c
     * kachel_bloecke) zählt mit ihr als EIN Block - wie in der Auswahl (kern_kopf_von). Prüfer 110: auch unter einer installierten App (die
     * trägt kein .Kachel); Begleit an einem Eintrag mit .Kachel setzt nur kachel_bloecke */
    if (!kern_bereiche_106() && f->Kachel && f->Begleit && f->Begleit->Kat == f->Kat) f = f->Begleit;
    if (f->Teil && f->Gruppe) return VERB(ar, "G|", f->Gruppe);
    return a_fmt(ar, "%p", (const void *)f);
}
const char *ablauf_block_schluessel(arena *ar, const eintrag *f) { return block_schluessel(ar, f); }   /* auch für test_zuordnung */
/* NEU 1.0.8 (F3, da_Chris86s Bilder vom 04.11.2026): die Klammer passt zum Bereich - "mit Daten und Updates" nur bei Spielen (bei Trophäen
 * klang das falsch), "mit App-Daten" bei Apps, sonst nur die Zahl der Einträge. kat 0 = Spiele/Apps gemischt (Warnung beim Leeren). */
static const char *bereich_zahl(arena *ar, int kat, int bloecke, int zeilen)
{
    if (kern_bereiche_106() || bloecke == zeilen) return zahl(ar, zeilen);
    const char *mit = kat == 1 || kat == 0 ? T("mit Daten und Updates ", "with data and updates ") : kat == 6 ? T("mit App-Daten ", "with app data ") : "";
    return SP_EN ? VERB(ar, zahl(ar, bloecke), " (", zahl(ar, zeilen), eintraege(zeilen), mit[0] ? " " : "", mit[0] ? a_kopie_n(ar, mit, strlen(mit) - 1) : "", ")")
                 : VERB(ar, zahl(ar, bloecke), " (", mit, zahl(ar, zeilen), eintraege(zeilen), ")");
}
/* NEU 1.0.7 (PS5-Test 03.11.2026): vollständige Bereinigung - was je Speicher dabei ist, nach Bereichen; gezählt werden Spiele/Apps
 * (Blöcke), dahinter die Zeilen. Regel = kern_komplett_dabei (dieselbe wie die Auswahl). ShadowMount: die Kachel zählt intern, ihre
 * Spieldatei dort, wo sie liegt (intern steht sie schon mit der Kachel da). */
static bool komplett_zaehlt(ablauf *a, const eintrag *f, int ort)
{
    if (!kern_komplett_dabei(KN, f, true, true)) return false;
    if (f->SmDatei) return ort != ORT_INTERN && f->Ort == ort;
    return kern_komplett_ort(f) == ort;
}
static const char *komplett_liste(ablauf *a, int ort, int *zeilen)
{
    const char *z = ""; int zs = 0;
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        int k = ALLE_KAT[ki], n = 0; mtab bl = mt_neu(false);
        FUER_ALLE(f) if (f->Kat == k && komplett_zaehlt(a, f, ort)) { n++; mt_setze(AR, &bl, block_schluessel(AR, f), (void *)1); }
        if (n > 0) { z = VERB(AR, z, "   - ", kern_kat_name(k), ": ", bereich_zahl(AR, k, bl.n, n), "\n"); zs += n; }
    }
    if (zeilen) *zeilen = zs;
    return z;
}
static const char *anzahl_bloecke(arena *ar, int bloecke, int zeilen)
{
    return bloecke == zeilen ? VERB(ar, zahl(ar, zeilen), eintraege(zeilen)) : bereich_zahl(ar, 0, bloecke, zeilen);
}
/* "auf der M.2-SSD" / "auf dem erweiterten Speicher (USB-Festplatte für Spiele)" - da_Chris86 03.11.2026: nie nur "USB" */
static const char *ort_auf(int o)
{
    switch (o) {
    case ORT_M2: return T("auf der M.2-SSD", "on the M.2 SSD");
    case ORT_USB: return T("auf dem erweiterten Speicher (USB-Festplatte für Spiele)", "on the extended storage (USB drive for games)");
    case ORT_STICK: return T("auf dem USB-Laufwerk", "on the USB drive");
    }
    return T("auf der internen SSD", "on the internal SSD");
}
/* NEU 1.1: Hinweis vor der Statuszeile und im Protokoll (Testversion "nur lesen" setzt einen eigenen) */
static const char *sim_kurz(ablauf *a) { return a->sim_hinweis ? a->sim_hinweis : T("SIMULATION - es wird nichts gelöscht.", "SIMULATION - nothing will be deleted."); }

void ablauf_init(ablauf *a, kern *k, const ablauf_ui *ui) { memset(a, 0, sizeof *a); a->K = k; a->ui = *ui; a->ta = arena_neu(); }
void ablauf_texte_frei(ablauf *a) { arena_frei(a->ta); a->ta = arena_neu(); }
bool ablauf_verbunden(ablauf *a) { return KN->S != NULL && KN->FUNDE_GUELTIG && KN->funde_verbindung_nr == KN->verbindung_nr; }

/* ------------------------------------------------------------- Kästchen (ZeigeBereiche / GruppenAbgleich) */
static bool gruppe_gewaehlt(ablauf *a, const char *gruppe)
{
    FUER_ALLE(f) if (f->Wahl && ist_teil(f, "Spiel") && f->Gruppe && txt_gleich_ci(f->Gruppe, gruppe)) return true;   /* H.Tab = ohne Groß/klein */
    return false;
}
void ablauf_eintrag_setzen(ablauf *a, eintrag *ff, bool an)
{
    const char *grund;
    if (ablauf_eintrag_gesperrt(a, ff, &grund)) return;       /* cb.IsEnabled = false */
    ff->Wahl = an;
    /* Spiel abgewählt -> seine Updates/DLCs auch wieder abwählen */
    if (ist_teil(ff, "Spiel") && !ff->Wahl)
        FUER_ALLE(x) if (x->Gruppe != NULL && ff->Gruppe != NULL && strcmp(x->Gruppe, ff->Gruppe) == 0 && !ist_teil(x, "Spiel")) x->Wahl = false;
    kern_abhaengige_mitnehmen(KN);                               /* GruppenAbgleich() */
}
/* NEU 1.0.7 (Grundlage da_Chris86 31.10.2026 13:12): Rotes (endgültig weg) nur einzeln; Update/Zusatzinhalt eines installierten
 * Spiels nur mit dem Spiel (kommt über kern_abhaengige_mitnehmen). Beim Abwählen gehen beide mit. */
static bool nur_einzeln_107(ablauf *a, const eintrag *f) { return f->Rot || kern_nur_mit_spiel(KN, f); }
void ablauf_bereich_setzen(ablauf *a, int kat, bool an)
{
    /* Trophäen-Listen noch installierter Spiele (gelb) nur einzeln - beim Abwählen gehen sie mit */
    /* NEU 1.1: "nur einzeln" Wählbares (M.2, Browserspeicher) ebenso - beim Abwählen geht es mit */
    FUER_ALLE(f) if (f->Kat == kat && !f->Gesperrt && !((f->Paket || f->NurEinzeln || nur_einzeln_107(a, f)) && an)) f->Wahl = an;
    kern_abhaengige_mitnehmen(KN);
}
int ablauf_bereich_zustand(ablauf *a, int kat)
{
    int n = 0, w = 0;
    /* NEU 1.1: nicht gewähltes "nur einzeln" zählt nicht mit - sonst ließe sich ein Bereich nie ganz abwählen */
    FUER_ALLE(f) if (f->Kat == kat && !f->Gesperrt && (!(f->NurEinzeln || nur_einzeln_107(a, f)) || f->Wahl)) { n++; if (f->Wahl) w++; }
    return w == 0 ? 0 : (w == n ? 2 : 1);
}
/* NEU 1.1: true, wenn im Bereich nichts gesammelt wählbar ist, aber etwas einzeln (z. B. die M.2 mit Selbstschutz) */
bool ablauf_bereich_nur_einzeln(ablauf *a, int kat)
{
    int n = 0, e = 0;
    FUER_ALLE(f) if (f->Kat == kat && !f->Gesperrt) { n++; if (f->NurEinzeln || f->Paket || nur_einzeln_107(a, f)) e++; }
    return n > 0 && e == n;
}
bool ablauf_eintrag_gesperrt(ablauf *a, const eintrag *f, const char **grund)
{
    *grund = NULL;
    if (f->Gesperrt) { *grund = f->SperrGrund ? f->SperrGrund : T("Gesperrt - der Game Compressor arbeitet gerade daran.", "Locked - the Game Compressor is working on it."); return true; }
    if (f->Teil != NULL && strcmp(f->Teil, "Spiel") != 0 && f->Gruppe && gruppe_gewaehlt(a, f->Gruppe)) { *grund = T("Geht mit dem Spiel weg.", "Goes with the game."); return true; }
    /* NEU 1.0.7: Update/Zusatzinhalt nur zusammen mit dem Spiel */
    if (kern_nur_mit_spiel(KN, f)) { *grund = T("Geht nur zusammen mit dem Spiel weg - wähle dafür das Spiel.", "Goes only together with the game - select the game for that."); return true; }
    /* NEU 1.0.7: ShadowMount-Kachel, deren Zeile "Spieldatei" gewählt ist */
    if (f->Quelle && f->Quelle[0]) FUER_ALLE(d) if (d->SmDatei && d->SmKachel == f && d->Wahl) { *grund = T("Geht mit der Spieldatei weg.", "Goes with the game file."); return true; }
    if (f->SmTipp) *grund = T("ShadowMount: Wird das Spiel gelöscht, verschwindet auch seine Kachel im Menü.", "ShadowMount: if the game is deleted, its tile disappears from the menu too.");
    else if (f->Jailbreak) *grund = kern_jb_hinweis(TA, f);   /* NEU 1.1 (Anforderungen 27.10.2026); 1.0.6 mit Namen (da_Chris86 29.10.2026) */
    else if (f->NurEinzeln && f->Ort == ORT_STICK) *grund = kern_bereiche_106() ? T("USB-Stick: nur einzeln wählbar - nie über \"ganzen Bereich wählen\" und nie in der vollständigen Bereinigung.", "USB stick: individually only - never via \"select the whole area\" and never in the full cleanup.")
                                                                  : T("USB-Laufwerk: nur einzeln wählbar - nie über \"ganzen Bereich wählen\" und nie in der vollständigen Bereinigung.", "USB drive: individually only - never via \"select the whole area\" and never in the full cleanup.");   /* NEU 1.0.6 */
    else if (f->NurEinzeln) *grund = T("Nur einzeln wählbar - nicht über \"ganzen Bereich wählen\".", "Individually only - not via \"select the whole area\".");   /* NEU 1.1 */
    else if (f->Rot) *grund = T("Nur einzeln wählbar - danach endgültig weg.", "Individually only - gone for good afterwards.");   /* NEU 1.0.7 */
    return false;
}

/* ------------------------------------------------------------- Größen in der Anzeige */
static const char *groesse_kurz(arena *ar, groesse g) { return VERB(ar, g.Gekappt ? T("mind. ", "min. ") : "", kern_groesse_text(ar, g.Bytes)); }
static void plus(groesse *s, const groesse *g) { s->Bytes += g->Bytes; s->Dateien += g->Dateien; s->Ordner += g->Ordner; if (g->Gekappt) s->Gekappt = true; if (g->Lesefehler) s->Lesefehler = true; }

const char *ablauf_eintrag_groesse(ablauf *a, const eintrag *f)
{
    groesse *g = kern_eintrag_groesse_in(KN, TA, f, true);
    return g ? VERB(TA, "   ", groesse_kurz(TA, *g)) : "";
}
const char *ablauf_bereich_kopf(ablauf *a, int k)
{
    int n = 0; FUER_ALLE(f) if (f->Kat == k) n++;
    const char *t = VERB(TA, "[", zahl(TA, k), "] ", kern_kat_name(k), "   (", zahl(TA, n), eintraege(n));
    if (n > 0) {
        groesse sum = {0}; bool alleDa = true;
        FUER_ALLE(f) if (f->Kat == k && alleDa) { groesse *g = kern_eintrag_groesse_in(KN, TA, f, true); if (!g) alleDa = false; else plus(&sum, g); }
        if (alleDa) t = VERB(TA, t, " · ", groesse_kurz(TA, sum));
    }
    return VERB(TA, t, ")");
}
const char *ablauf_stand_text(ablauf *a)
{
    int n = 0; FUER_ALLE(f) if (f->Wahl) n++;
    const char *t = n == 0 ? T("Nichts ausgewählt.", "Nothing selected.") : VERB(TA, zahl(TA, n), eintraege(n), T(" ausgewählt.", " selected."));
    if (n > 0) {
        groesse sum = {0}; bool alleDa = true;
        FUER_ALLE(f) if (f->Wahl && alleDa) { groesse *g = kern_eintrag_groesse_in(KN, TA, f, true); if (!g) alleDa = false; else plus(&sum, g); }
        if (alleDa) t = VERB(TA, zahl(TA, n), eintraege(n), T(" ausgewählt (", " selected ("), groesse_kurz(TA, sum), ").");
    }
    /* statt "TESTMODUS - es wird nichts gelöscht." in 1.0.5 */
    if (KN->simulation) t = VERB(TA, sim_kurz(a), "  ", t);
    int hn = a->hg_n, hg = a->hg_gesamt;
    if (a->hintergrund && hg > 0 && hn < hg) t = VERB(TA, t, "   ", T("Größen werden ermittelt (", "Counting sizes ("), zahl(TA, hn), T(" von ", " of "), zahl(TA, hg), ") ...");
    return t;
}

/* ------------------------------------------------------------- Zusammenfassung (Arbeits-Faden) */
const char *ablauf_zusammenfassung(ablauf *a, bool komplett)
{
    pliste g = kern_gewaehlt(KN);
    const char *t = VERB(AR, T("Wird ENDGÜLTIG gelöscht: ", "Will be deleted PERMANENTLY: "), zahl(AR, g.n), eintraege(g.n), "\n");
    /* NEU 1.0.7 (Grundlage da_Chris86 31.10.2026 13:12, Punkt 5): vorneweg konkret, was endgültig weg ist */
    if (!kern_bereiche_106()) { const char *e = kern_endgueltig_satz(KN, AR); if (e[0]) t = VERB(AR, e, "\n\n", t); }
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        int k = ALLE_KAT[ki], c = 0, pak = 0;
        mtab bl = mt_neu(false);
        for (int i = 0; i < g.n; i++) {
            eintrag *f = g.e[i];
            if (f->Kat != k) continue;
            if (f->Paket && !kern_bereiche_106()) { pak++; continue; }   /* NEU 1.0.8 (F3): getrennt gezählt wie im ersten Fenster */
            c++; mt_setze(AR, &bl, block_schluessel(AR, f), (void *)1);
        }
        if (c > 0) t = VERB(AR, t, "   - ", kern_kat_name(k), ": ", bereich_zahl(AR, k, bl.n, c), "\n");
        if (pak > 0) t = VERB(AR, t, T("   - Trophäen-Listen installierter Spiele: ", "   - Trophy lists of installed games: "), zahl(AR, pak), "\n");
    }
    int ext[4] = { 0, 0, 0, 0 }, smDatei = 0;
    /* NEU 1.0.8 (F2, da_Chris86s Bilder vom 04.11.2026): sind auf einem Speicher nur einzelne Dateien gewählt (z. B. drei Protokolle auf dem
     * USB-Laufwerk), nur diese Dateien nennen - "was darin liegt (Spiele, Ordner, Dateien)" klang nach dem ganzen Laufwerk */
    int nurDatei[4] = { 0, 0, 0, 0 }, dateiZahl[4] = { 0, 0, 0, 0 }, dn[4] = { 0, 0, 0, 0 };
    const char *dnamen[4] = { "", "", "", "" };
    for (int i = 0; i < g.n; i++) {
        eintrag *f = g.e[i];
        int o = f->Ort;
        if (f->Quelle[0]) { o = ORT_INTERN; for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], f->Quelle)) o = kern_pfad_ort(f->Quelle); }   /* Codex-Prüfung 28 */
        if (o >= 0 && o <= 3) ext[o]++;
        if (o >= 1 && o <= 3 && f->HatGroesse && f->Ordner == 0 && f->Dateien >= 1 && !f->Quelle[0] && f->Pfade.n > 0) {
            nurDatei[o]++; dateiZahl[o] += f->Dateien;
            for (int j = 0; j < f->Pfade.n && dn[o] < 5; j++) { const char *p = f->Pfade.e[j], *b = strrchr(p, '/'); dnamen[o] = VERB(AR, dnamen[o], dn[o] ? ", " : "", b ? b + 1 : p); dn[o]++; }
        }
        if (f->Quelle[0]) for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], f->Quelle)) { smDatei++; break; }
    }
    if (smDatei > 0) t = VERB(AR, t, T("   davon ShadowMount-Spiele MIT Spieldatei: ", "   of these, ShadowMount games WITH game file: "), zahl(AR, smDatei), "\n");
    /* NEU 1.0.7 (da_Chris86 31.10.2026 19:56): installierte PS4-Spiele - welche gehen mit (ausdrücklich gewählt), wie viele bleiben */
    if (!kern_bereiche_106() && KN->PS4.n > 0) {
        kern_ps4_weg_setzen(KN);
        const char *weg = ""; int bleiben = 0;
        for (int i = 0; i < KN->PS4.n; i++) {
            const ps4_spiel *s = KN->PS4.e[i];
            if (!kern_ps4_installiert(s)) continue;
            if (mt_hat(&KN->PS4_WEG, VERB(AR, s->basis, "|", s->id))) weg = VERB(AR, weg, "      ", kern_ps4_zeile(AR, s), "\n");
            else bleiben++;
        }
        if (weg[0]) t = VERB(AR, t, T("   PS4-Spiele, die GELÖSCHT werden (installiertes Spiel weg, PKG neu installieren nötig):\n", "   PS4 games that will be DELETED (installed game gone, PKG must be installed again):\n"), weg);
        if (bleiben > 0) t = VERB(AR, t, T("   PS4-Spiele, die geschützt bleiben: ", "   PS4 games that stay protected: "), zahl(AR, bleiben), "\n");
    }
    /* NEU 1.0.6: je Speicher außerhalb der internen SSD */
    for (int o = ORT_M2; o <= ORT_STICK; o++) {
        if (komplett && ext[o] == 0 && (o == ORT_M2 ? KN->m2Da : o == ORT_USB ? KN->USB_SPEICHER : false))
            t = kern_bereiche_106() ? VERB(AR, t, "   (", kern_ort_name(o), T(" bleibt unverändert)\n", " stays unchanged)\n"))
                                    : VERB(AR, t, T("   Bleibt unverändert: ", "   Stays unchanged: "), kern_ort_name(o), "\n");   /* 03.11.2026: keine Klammer in Klammer */
        if (!komplett && ext[o] > 0) {
            if (kern_bereiche_106())
                t = VERB(AR, T("!!! ACHTUNG: ", "!!! WARNING: "), zahl(AR, ext[o]), T(" EINTRÄGE AUF ", " ENTRIES ON "), kern_ort_name(o), T(" SIND GEWÄHLT - was darin liegt (Spiele, Dateien), ist danach weg !!!\n\n", " ARE SELECTED - whatever is in them (games, files) will be gone !!!\n\n"), t);
            else if (nurDatei[o] == ext[o])   /* NEU 1.0.8 (F2): nur einzelne Dateien */
                t = VERB(AR, o == ORT_STICK ? T("Auf dem USB-Laufwerk", "On the USB drive") : o == ORT_M2 ? T("Auf der M.2-SSD", "On the M.2 SSD") : T("Auf dem erweiterten Speicher", "On the extended storage"),
                         T(" werden nur einzelne Dateien gelöscht (", ", only single files are deleted ("), zahl(AR, dateiZahl[o]), "): ", dnamen[o], dateiZahl[o] > dn[o] ? T(" und weitere", " and more") : "", ".\n\n", t);
            else
                t = VERB(AR, T("!!! ACHTUNG: ", "!!! WARNING: "), zahl(AR, ext[o]), eintraege(ext[o]), " ", ort_auf(o), nurDatei[o] > 0 ? T(" sind gewählt, darunter Ordner", " are selected, including folders") : T(" sind gewählt", " are selected"),
                         T(" - was in diesen Ordnern liegt (Spiele, Dateien), ist danach weg !!!\n\n", " - whatever is in these folders (games, files) will be gone !!!\n\n"), t);
        }
    }
    int gesp = 0, gesp11 = 0;
    bool selbst = false;
    FUER_ALLE(f) if (f->Gesperrt) {
        /* Codex-Prüfung 49: Spieldatei-Zeile nicht doppelt; PS5 Cleanup selbst geht bei der vollständigen Bereinigung zum Schluss mit */
        if (!kern_bereiche_106() && f->SmDatei) continue;
        /* Codex-Prüfungen 49-53: PS5 Cleanup selbst nicht als Zahl - was davon bleibt, hängt an Kopien auf behaltenen Speichern
         * (kern_loeschen.c selbst_weg_erlaubt); hier nur ein Satz */
        if (!kern_bereiche_106() && komplett && kern_selbst_gesperrt(f)) { selbst = true; continue; }
        if (f->SperrGrund) gesp11++; else gesp++;
    }
    if (gesp > 0) t = VERB(AR, t, T("   Bleibt (Game Compressor arbeitet daran): ", "   Kept (Game Compressor is working on it): "), zahl(AR, gesp), "\n");
    if (gesp11 > 0) t = VERB(AR, t, T("   Bleibt (gesperrt, der Grund steht am Eintrag): ", "   Kept (locked, the reason is shown at the entry): "), zahl(AR, gesp11), "\n");   /* NEU 1.1 */
    if (selbst) t = VERB(AR, t, T("   PS5 Cleanup selbst wird zum Schluss entfernt - Kopien auf einem USB-Laufwerk, auf behaltener M.2 oder behaltenem erweitertem Speicher bleiben.\n",
                                  "   PS5 Cleanup itself is removed at the end - copies on a USB drive, on a kept M.2 or kept extended storage stay.\n"));   /* Codex-Prüfung 53 */
    if (!komplett) { const char *h = kern_speicher_hinweis(KN); if (h[0]) { while (*h == '\n') h++; t = VERB(AR, t, h, "\n"); } }
    t = VERB(AR, t, T("\nDas sind ", "\nThat is "), kern_groesse_satz(AR, a->GROSS), ".\n");
    t = VERB(AR, t, "\n", kern_kopplungs_text(KN), T("\nBenutzung auf eigene Gefahr - Gelöschtes ist endgültig weg.", "\nUse at your own risk - deleted data is gone for good."));
    /* NEU 1.1 */
    const char *sh = kern_selbstschutz_hinweis(KN, komplett);
    if (sh[0]) t = VERB(AR, t, "\n\n", sh);
    if (KN->simulation) t = VERB(AR, T("SIMULATION - es wird NICHTS gelöscht. So sähe es beim echten Löschen aus:\n\n", "SIMULATION - NOTHING will be deleted. This is how real deleting would look:\n\n"), t);
    return t;
}

/* ------------------------------------------------------------- Größen im Hintergrund */
typedef struct { ablauf *a; pthread_t t; volatile bool stopp; bool laeuft; } hlauf;
static bool h_stopp(void *c) { return ((hlauf *)c)->stopp; }
static void h_fertig(void *c, eintrag *e)
{
    (void)e; hlauf *h = c;
    if (h->stopp) return;
    h->a->hg_n++;
    if (h->a->groesse_neu) h->a->groesse_neu(h->a->groesse_ctx);
}
static void *h_lauf(void *c)
{
    hlauf *h = c;
    basis_zaehlt = 1;
    zugriff_bremse_hier = 1;      /* NEU 1.0.7: kern_groessen_im_hintergrund fängt jedes Anhalten auf */
    kern_groessen_im_hintergrund(h->a->K, h_stopp, h, h_fertig, h);
    zugriff_bremse_hier = 0;
    basis_zaehlt = 0;
    if (!h->stopp) { h->a->hg_n = h->a->hg_gesamt; if (h->a->groesse_neu) h->a->groesse_neu(h->a->groesse_ctx); }
    return NULL;
}
void ablauf_hintergrund_starten(ablauf *a)
{
    ablauf_hintergrund_anhalten(a);
    if (!ablauf_verbunden(a) || KN->Funde.n == 0) return;
    hlauf *h = calloc(1, sizeof *h); h->a = a;
    a->hg_n = 0; a->hg_gesamt = KN->Funde.n;
    a->hintergrund = h;
    if (basis_faden(&h->t, h_lauf, h) == 0) h->laeuft = true;   /* 1.0.6 Fix: mit ausreichend Stapel */ else { a->hintergrund = NULL; free(h); }
}
/* NEU 1.0.8 (Arbeitsplan 3.4): Löschen nach dem laufenden Eintrag anhalten. Wirkt nur zwischen zwei Einträgen (kern_loeschen.c) - der
 * laufende Eintrag wird immer fertig. Danach keine Selbstentfernung und keine alten Protokolle; was übrig ist, sagt das Protokoll. */
/* Codex-Prüfung 89: Annehmen des Wunsches (Oberfläche) und Schließen des Fensters (Löschfaden) sind gegeneinander gesperrt - sonst
 * konnte die Oberfläche "Wird angehalten" sagen, während der Lauf schon an der letzten Prüfung vorbei war (und PS5 Cleanup sich entfernte) */
static pthread_mutex_t anhalt_m = PTHREAD_MUTEX_INITIALIZER;
bool ablauf_loeschen_anhalten(ablauf *a)
{
    pthread_mutex_lock(&anhalt_m);
    bool ok = a->anhaltbar;
    if (ok) KN->anhalten = true;
    pthread_mutex_unlock(&anhalt_m);
    return ok;
}
/* Hält die Zählung an und wartet, bis ihr Faden wirklich weg ist - erst danach geht es zum Löschen weiter. */
void ablauf_hintergrund_anhalten(ablauf *a)
{
    hlauf *h = a->hintergrund; a->hintergrund = NULL;
    if (!h) return;
    h->stopp = true;
    if (h->laeuft && !pthread_equal(h->t, pthread_self())) pthread_join(h->t, NULL);
    free(h);
}

/* ------------------------------------------------------------- GroesseErmitteln */
static void groesse_stand(void *c, long long b, int n)
{
    ablauf *a = c;
    stand(a, VERB(AR, T("Zähle nur die Größe - es wird noch NICHTS gelöscht ... bisher ", "Only counting the size - NOTHING is deleted yet ... so far "), kern_groesse_text(AR, b),
                  " in ", kern_zahl_n0(AR, n), T(" Dateien", " files")), ABLAUF_BALKEN_GLEICH);
}
static void groesse_ermitteln(ablauf *a)
{
    bool fehlt = false;
    FUER_ALLE(f) if (f->Wahl && !fehlt) for (int j = 0; j < f->Pfade.n; j++) if (!kern_pfad_gezaehlt(KN, f->Pfade.e[j])) { fehlt = true; break; }
    if (fehlt) {
        logz(a, T("Zähle, wie viel gewählt ist (noch wird NICHTS gelöscht - danach kommt die Abschlussfrage) ...", "Counting the selection (NOTHING is deleted yet - the final question comes next) ..."));
        stand(a, T("Zähle nur die Größe - es wird noch NICHTS gelöscht ...", "Only counting the size - NOTHING is deleted yet ..."), ABLAUF_UNBESTIMMT);
    }
    KN->GroesseStand = groesse_stand; KN->groesse_ctx = a;
    a->abbrechbar = fehlt;
    VERSUCHE { a->GROSS = kern_ermittle_groessen(KN); }
    FANGE { a->abbrechbar = false; KN->GroesseStand = NULL; WEITERWERFEN(); } ENDE_VERSUCHE;
    a->abbrechbar = false;
    KN->GroesseStand = NULL;
    if (fehlt) stand(a, NULL, 0);
    if (KN->abbruch) wirf(AUSN_ABBRUCH, "");
    logz(a, VERB(AR, T("Ausgewählt: ", "Selected: "), kern_groesse_satz(AR, a->GROSS), "."));
}

/* ------------------------------------------------------------- LoescheJetzt */
/* string.Length / Substring zählen UTF-16-Einheiten */
static int net_laenge(const char *s) { int n = 0; const unsigned char *p = (const unsigned char *)s; while (*p) { uint32_t c = utf8_naechstes(&p); n += c >= 0x10000 ? 2 : 1; } return n; }
static const char *net_anfang(arena *ar, const char *s, int n)
{
    const unsigned char *p = (const unsigned char *)s; int z = 0;
    while (*p) { const unsigned char *v = p; uint32_t c = utf8_naechstes(&p); int w = c >= 0x10000 ? 2 : 1; if (z + w > n) { p = v; break; } z += w; }
    return a_kopie_n(ar, s, (size_t)(p - (const unsigned char *)s));
}
typedef struct { ablauf *a; bool groessenDa; long long gesamt, erledigt; const eintrag *vorher; } fstand;
static void fortschritt(void *c, int i, int n, const eintrag *f)
{
    fstand *s = c; ablauf *a = s->a;
    if (s->vorher && s->vorher->HatGroesse) s->erledigt += s->vorher->Bytes;
    s->vorher = f;
    const char *name = net_laenge(f->Text) > 70 ? VERB(AR, net_anfang(AR, f->Text, 67), "...") : f->Text;
    const char *t = VERB(AR, KN->simulation ? T("SIMULATION · prüfe Eintrag ", "SIMULATION · checking entry ") : T("Lösche Eintrag ", "Deleting entry "), zahl(AR, i), T(" von ", " of "), zahl(AR, n),
                         (s->groessenDa && s->gesamt > 0) ? VERB(AR, "  ·  ", kern_groesse_text(AR, s->erledigt), T(" von ", " of "), kern_groesse_text(AR, s->gesamt)) : "", "  ·  ", name);
    double wert = n > 0 ? ((s->groessenDa && s->gesamt > 0) ? s->erledigt * 100.0 / (double)s->gesamt : (i - 1) * 100.0 / n) : 0;
    /* NEU 1.0.8 (4.5) - Codex-Prüfung 76: die Oberfläche liest die Angaben in einem anderen Faden; lf_seq ist ungerade, solange sie
     * geschrieben werden (die Oberfläche nimmt dann den Stand beim nächsten Bild). Frei geräumt: nur, was wirklich weg ist (FREI_BISHER). */
    unsigned q = a->lf_seq;
    __atomic_store_n(&a->lf_seq, q + 1u, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    __atomic_store_n(&a->lf_frei, (long long)KN->FREI_BISHER, __ATOMIC_RELAXED);
    __atomic_store_n(&a->lf_gesamt, (s->groessenDa && s->gesamt > 0) ? s->gesamt : -1LL, __ATOMIC_RELAXED);
    __atomic_store_n(&a->lf_f, f, __ATOMIC_RELAXED);
    __atomic_store_n(&a->lf_n, n, __ATOMIC_RELAXED);
    __atomic_store_n(&a->lf_nr, i, __ATOMIC_RELAXED);
    __atomic_store_n(&a->lf_fehler, (int)KN->FEHLER_ZAHL, __ATOMIC_RELAXED);   /* Codex-Prüfung 77: Fehler bis vor diesen Eintrag */
    __atomic_store_n(&a->lf_unv, KN->UNVERAENDERT.n, __ATOMIC_RELAXED);
    __atomic_store_n(&a->lf_seq, q + 2u, __ATOMIC_RELEASE);
    stand(a, t, wert);
}
static bool sm_frage(void *c)
{
    ablauf *a = c;
    return frage_wahl(a, VERB(AR, T("ShadowMount konnte nicht sicher angehalten werden:\n", "ShadowMount could not be stopped safely:\n"), KN->SM_GRUND,
        T("\n\nLäuft es noch, können eingebundene Spieldaten beim Löschen BESCHÄDIGT werden, und gelöschte Kacheln kommen sofort wieder.\n\nEmpfehlung: abbrechen, die PS5 neu starten, nur den FTP-Server laden (ohne ShadowMount) und dann nochmal löschen.",
          "\n\nIf it is still running, mounted game data can be DAMAGED while deleting, and deleted tiles come back right away.\n\nRecommended: cancel, restart the PS5, load only the FTP server (without ShadowMount) and delete again.")),
        "ShadowMount", T("Abbrechen (empfohlen)", "Cancel (recommended)"), T("Trotzdem löschen (auf eigene Gefahr)", "Delete anyway (at your own risk)"));
}

static void bremse_melden(ablauf *a);   /* NEU 1.0.7, unten */
static void loesche_jetzt(ablauf *a)
{
    /* NEU 1.0.7: direkt vor dem ersten Löschen (vor_loeschen - die PS5-App beginnt dort das Protokoll, bis 02.11.2026 stand hier die
     * Datenbank-Sicherung). Meldet es false, wird nichts gelöscht */
    if (a->vor_loeschen && !a->vor_loeschen(a->vor_loeschen_ctx)) {
        logz(a, T("Es wurde NICHTS gelöscht.", "NOTHING was deleted."));
        kern_sm_zuruecksetzen(KN); liste_neu(a); ablauf_hintergrund_starten(a);
        return;
    }
    stand(a, KN->simulation ? T("SIMULATION - es wird nichts gelöscht ...", "SIMULATION - nothing is deleted ...") : T("Lösche ... (große Spiele können etwas dauern)", "Deleting ... (large games can take a while)"), 0);
    /* NEU 1.0.5: "Eintrag i von n" (und GB, wenn die Größen bekannt sind) - nur Anzeige */
    fstand fs = { a, true, 0, 0, NULL };
    int rm_n = 0;   /* NEU 1.0.7: für die Rückmeldung per QR-Code (nur die Zahl) */
    int lf_n0 = 0;  /* NEU 1.0.9 (PS5-Test da_Chris86 09.11.2026 16:09: "1 von 49", danach "11 von 48"): wie der Kern zählen - eine
                     * ShadowMount-Spieldatei (SmDatei) geht mit ihrer Kachel und ist kein eigener Schritt (kern_gewaehlt) */
    FUER_ALLE(f) if (f->Wahl) { rm_n++; if (!f->SmDatei) lf_n0++; if (!f->HatGroesse) fs.groessenDa = false; else fs.gesamt += f->Bytes; }
    KN->Fortschritt = fortschritt; KN->fortschritt_ctx = &fs;
    a->lf_nr = 0; a->lf_n = lf_n0; a->lf_f = NULL; a->lf_frei = 0; a->lf_gesamt = fs.groessenDa && fs.gesamt > 0 ? fs.gesamt : -1;   /* NEU 1.0.8 (4.5) */
    memset(&a->rm_frei, 0, sizeof a->rm_frei);
    logz(a, "");
    static _Thread_local ergebnis erg;          /* (über longjmp hinweg gültig) */
    static _Thread_local bool rm_ab;            /* NEU 1.0.7 (Codex-Prüfung 69): Abbruch über Ausnahme (z. B. Speicherbremse) */
    rm_ab = false;
    memset(&erg, 0, sizeof erg);
    /* NEU 1.0.7 (Gegenprüfung 03.11.2026): ungünstig vorbelegen - endet der Lauf über eine Ausnahme, zeigt der QR-Code "abgebrochen" statt
     * eines alten Stands; der normale Schluss überschreibt es */
    a->rm_da = true; a->rm_abgebrochen = true; a->rm_gewaehlt = rm_n; a->rm_fehler = 1; a->rm_teilweise = a->rm_geschuetzt = a->rm_unveraendert = 0;
    a->loescht = true;
    KN->RM_FEHLER_N = 0; KN->RM_SCHRITT[0] = 0; KN->RM_CODE[0] = 0; KN->RM_ERFASSEN = true;   /* NEU 1.0.7: QR-Rückmeldung */
    KN->FEHLER_ZAHL = 0; KN->FREI_BISHER = 0;   /* NEU 1.0.8 (Codex-Prüfung 76): Anzeige "Löschen läuft" */
    pthread_mutex_lock(&anhalt_m); KN->anhalten = false; a->anhaltbar = true; pthread_mutex_unlock(&anhalt_m);   /* NEU 1.0.8 (3.4): ein alter Wunsch gilt nicht für diesen Lauf */
    VERSUCHE { erg = kern_fuehre_loeschung(KN, sm_frage, a); }
    FANGE {
        if (ausn_art != AUSN_ABBRUCH) { pthread_mutex_lock(&anhalt_m); a->anhaltbar = false; pthread_mutex_unlock(&anhalt_m); a->loescht = false; KN->RM_ERFASSEN = false; KN->Fortschritt = NULL; WEITERWERFEN(); }
        /* kann heute nicht vorkommen (Abbrechen ist beim Löschen gesperrt) - aber dann ehrlich melden */
        memset(&erg, 0, sizeof erg); erg.Fehler = 1; rm_ab = true;
        logz(a, T("Löschen wurde unterbrochen - ein Teil kann schon gelöscht sein. Bitte das Protokoll prüfen.", "Deleting was interrupted - part may already be deleted. Please check the log."));
        if (zugriff_speicher_gebremst) bremse_melden(a);   /* NEU 1.0.7: Grund nennen */
    } ENDE_VERSUCHE;
    pthread_mutex_lock(&anhalt_m); a->anhaltbar = false; bool gewuenscht = KN->anhalten; pthread_mutex_unlock(&anhalt_m);   /* Codex-Prüfung 89 */
    a->loescht = false;
    /* Codex-Prüfung 75: kam der Wunsch erst während des letzten Eintrags (oder danach), gilt er trotzdem - auch dann keine Selbstentfernung
     * und keine alten Protokolle */
    if (gewuenscht && !erg.Angehalten && !erg.Abgebrochen && !rm_ab) {
        erg.Angehalten = true;
        logz(a, T("Angehalten auf deinen Wunsch - da lief schon der letzte Eintrag, übrig ist nichts.", "Stopped at your request - the last entry was already running, nothing is left."));
    }
    KN->RM_ERFASSEN = false;
    KN->Fortschritt = NULL;
    stand(a, "", ABLAUF_BALKEN_GLEICH);
    if (erg.Abgebrochen) {
        a->rm_gewaehlt = rm_n; a->rm_fehler = erg.Fehler; a->rm_teilweise = erg.Teilweise; a->rm_geschuetzt = erg.Geschuetzt; a->rm_unveraendert = erg.Unveraendert; a->rm_abgebrochen = true; a->rm_da = true;   /* NEU 1.0.7 */
        if (KN->SM_STOP_GESENDET) logz(a, T("Abgebrochen - nichts gelöscht. ShadowMount hat das Stopp-Signal erhalten - zum Weiterspielen neu laden.", "Cancelled - nothing deleted. ShadowMount received the stop signal - load it again to keep playing."));
        else logz(a, T("Abgebrochen - nichts verändert.", "Cancelled - nothing changed."));
        kern_sm_zuruecksetzen(KN); return;
    }
    stand(a, "", 100);
    /* NEU 28.10.2026 (da_Chris86): PS5 Cleanup ganz zum Schluss selbst entfernen - vollständige Bereinigung ohne Frage,
     * Auswahl auf Nachfrage. Nur wenn alles andere fehlerfrei durchlief (sonst wird PS5 Cleanup evtl. noch gebraucht). */
    const char *selbst_text = "";
    bool sm2 = a->selbst_modus == 1 && a->leeren_m2, susb = a->selbst_modus == 1 && a->leeren_usb;   /* Codex-Prüfung 28 */
    /* NEU 03.11.2026 (da_Chris86 17:51): vollständige Bereinigung - alte Protokolle auf der Konsole mit (nie auf dem USB-Laufwerk, nie dieses) */
    if (a->selbst_modus == 1 && erg.Fehler == 0 && !erg.Angehalten && !kern_bereiche_106()) {
        KN->RM_SCHRITT[0] = 0; KN->RM_CODE[0] = 0; KN->RM_ERFASSEN = true;   /* NEU 1.0.7: QR-Rückmeldung */
        int pf = kern_alte_protokolle_weg(KN);
        KN->RM_ERFASSEN = false;
        if (pf > 0) kern_rm_fehler_dazu(KN, "datei", "rest");
        erg.Fehler += pf;
    }
    if (a->selbst_modus && kern_selbst_da(KN, sm2, susb)) {
        if (erg.Angehalten) {   /* NEU 1.0.8 (3.4) */
            selbst_text = erg.Rest > 0 ? T("PS5 Cleanup bleibt, weil du angehalten hast - so kannst du später weitermachen.", "PS5 Cleanup stays because you stopped - so you can continue later.")
                                       : T("PS5 Cleanup bleibt, weil du angehalten hast.", "PS5 Cleanup stays because you stopped.");
            logz(a, selbst_text);
        } else if (erg.Fehler > 0) {
            selbst_text = T("PS5 Cleanup bleibt, weil Fehler aufgetreten sind - so kannst du es nochmal versuchen.", "PS5 Cleanup stays because errors occurred - so you can try again.");
            logz(a, selbst_text);
        } else {
            bool weg = a->selbst_modus == 1
                || frage_wahl(a, T("Löschung abgeschlossen.\n\nPS5 Cleanup jetzt auch entfernen (Programm, eigene Daten und Kachel)?\n\nBEHALTEN: PS5 Cleanup bleibt für die nächste Bereinigung.\n\nENTFERNEN: PS5 Cleanup löscht sich jetzt selbst.",
                                     "Deletion finished.\n\nRemove PS5 Cleanup too now (program, its data and tile)?\n\nKEEP: PS5 Cleanup stays for the next cleanup.\n\nREMOVE: PS5 Cleanup deletes itself now."),
                              "PS5 Cleanup", T("Behalten", "Keep"), T("Entfernen", "Remove"));
            if (weg) {
                logz(a, T("Entferne PS5 Cleanup selbst ...", "Removing PS5 Cleanup itself ..."));
                if (a->vor_selbstweg) a->vor_selbstweg(a->vor_selbstweg_ctx);   /* NEU 1.0.7: Protokoll vorher auf den Stick */
                a->loescht = true;
                KN->RM_SCHRITT[0] = 0; KN->RM_CODE[0] = 0; KN->RM_ERFASSEN = true;   /* NEU 1.0.7 (Codex-Prüfung 69): auch hier Fehlercodes */
                int f = kern_selbst_entfernen(KN, sm2, susb);
                KN->RM_ERFASSEN = false;
                if (f > 0) kern_rm_fehler_dazu(KN, "selbst", "app");
                a->loescht = false;
                erg.Fehler += f;
                selbst_text = f == 0 ? (KN->simulation ? T("PS5 Cleanup würde sich jetzt selbst entfernen.", "PS5 Cleanup would now remove itself.")
                                                       : T("PS5 Cleanup hat sich selbst entfernt.", "PS5 Cleanup has removed itself."))
                                     : T("PS5 Cleanup konnte sich nicht ganz selbst entfernen - siehe Protokoll.", "PS5 Cleanup could not remove itself completely - see log.");
                /* Codex-Prüfung 32: Sony hat die eigene Kachel nicht ausgetragen - dann nicht "ganz entfernt" melden */
                /* NEU 1.0.6 (da_Chris86 29.10.2026 23:08): Kachel + Startordner entfernt der Hintergrundteil nach dem Beenden -
                 * "vollständig entfernt" meldet erst er, nachdem er es geprüft hat (Benachrichtigung + Kernel-Protokoll) */
                if (f == 0 && KN->SELBST_UEBERGEBEN)
                    selbst_text = T("PS5 Cleanup hat seine Dateien entfernt. Die Kachel wird entfernt, gleich nachdem du ✕ oder ○ gedrückt hast - danach kommt eine Benachrichtigung.",
                                    "PS5 Cleanup has removed its files. The tile is removed right after you press ✕ or ○ - a notification follows.");   /* da_Chris86 30.10.2026 09:07 */
                if (f == 0 && sl_enthaelt(&KN->GEISTER, "PS5 Cleanup"))
                    selbst_text = !kern_bereiche_106() ? (KN->simulation ? T("PS5 Cleanup würde seine Dateien entfernen - die eigene Kachel bliebe im Menü sichtbar.", "PS5 Cleanup would remove its files - its tile would stay visible in the menu.")
                                                                         : T("PS5 Cleanup hat seine Dateien entfernt - die eigene Kachel bleibt im Menü sichtbar.", "PS5 Cleanup has removed its files - its tile stays visible in the menu."))
                                : KN->simulation ? T("PS5 Cleanup würde seine Dateien entfernen - die eigene Kachel bliebe bis zum Datenbank-Neuaufbau sichtbar.", "PS5 Cleanup would remove its files - its tile would stay visible until the database is rebuilt.")
                                                 : T("PS5 Cleanup hat seine Dateien entfernt - die eigene Kachel bleibt bis zum Datenbank-Neuaufbau sichtbar.", "PS5 Cleanup has removed its files - its tile stays visible until the database is rebuilt.");
                logz(a, selbst_text);
            }
        }
    }
    logz(a, "");
    const char *kopf, *kopf_meldung, *frei;
    const char *smtext = erg.SmText ? erg.SmText : "";
    if (KN->simulation) {
        kopf = erg.Fehler == 0 ? T("SIMULATION fertig - es wurde NICHTS gelöscht. So wäre es beim echten Löschen gelaufen.", "SIMULATION done - NOTHING was deleted. This is how real deleting would have gone.")
                               : VERB(AR, T("SIMULATION fertig - es wurde NICHTS gelöscht. Beim echten Löschen gäbe es ", "SIMULATION done - NOTHING was deleted. Real deleting would have "), zahl(AR, erg.Fehler), T(" Fehler - siehe Protokoll.", " error(s) - see log."));
        kopf_meldung = kopf;
        const char *ft = kern_freigeraeumt_text(AR, erg.Frei);       /* "Freigeräumt: ..." */
        const char *nach = strchr(ft, ':');
        frei = VERB(AR, T("Würde freigeräumt", "Would be freed"), nach ? nach : "");
    } else {
        kopf = erg.Fehler == 0 ? T("Fertig - alles Ausgewählte ist gelöscht.", "Done - everything selected has been deleted.")
                               : VERB(AR, T("Fertig, aber mit ", "Done, but with "), zahl(AR, erg.Fehler), T(" Fehler(n) - siehe oben.", " error(s) - see above."));
        kopf_meldung = erg.Fehler == 0 ? kopf : VERB(AR, T("Fertig, aber mit ", "Done, but with "), zahl(AR, erg.Fehler), T(" Fehler(n) - siehe Protokoll.", " error(s) - see log."));
        frei = kern_freigeraeumt_text(AR, erg.Frei);
    }
    /* NEU 1.0.6 (da_Chris86 28.10.2026): Ein Eintrag, dessen Kachel Sony nicht entfernt hat, gilt nicht als ganz entfernt */
    if (erg.Teilweise > 0 && erg.Fehler == 0) {
        const char *tw = VERB(AR, zahl(AR, erg.Teilweise), erg.Teilweise == 1 ? T(" Eintrag wurde nur teilweise entfernt", " entry was only partly removed")
                                                                            : T(" Einträge wurden nur teilweise entfernt", " entries were only partly removed"),
                              kern_bereiche_106() ? T(" - die Kachel bleibt im Menü (siehe Protokoll).", " - the tile stays in the menu (see log).")
                                                  : T(" - was übrig ist, steht im Protokoll.", " - the log shows what is left."));   /* PS5-Test 03.11.2026: nicht immer eine Kachel */
        if (KN->simulation) kopf = kopf_meldung = VERB(AR, T("SIMULATION fertig - es wurde NICHTS gelöscht. ", "SIMULATION done - NOTHING was deleted. "), T("Beim echten Löschen: ", "Real deleting: "), tw);
        else kopf = kopf_meldung = VERB(AR, T("Fertig - aber ", "Done - but "), tw);
    }
    /* NEU 1.0.8 (PS5-Test 07.11.2026 17:40, da_Chris86: "ps5 upload wurde nicht sauber entfernt"): blieb ein Eintrag unverändert (Sony lehnt
     * ab, die Kachel bleibt, Einbindung unklar), stand oben trotzdem "alles Ausgewählte ist gelöscht" mit grünem Haken - jetzt ehrlich, wie
     * viele sich nicht ganz entfernen ließen (kein Fehler, aber orange wie "teilweise"). Codex-Prüfung 118 (P2): nicht "blieb unverändert" -
     * hat Sony einen Auftrag angenommen, kann Sony Dateien noch im Hintergrund entfernen. Der Vergleichsmodus 1.0.6 der Tests behält den
     * alten Text. */
    const bool unv = erg.Unveraendert > 0 && erg.Fehler == 0 && erg.Teilweise == 0 && !kern_bereiche_106();
    if (unv) {
        const char *z = zahl(AR, erg.Unveraendert);
        bool eins = erg.Unveraendert == 1;
        if (KN->simulation)
            kopf = kopf_meldung = VERB(AR, T("SIMULATION fertig - es wurde NICHTS gelöscht. Beim echten Löschen ", "SIMULATION done - NOTHING was deleted. Real deleting would not fully remove "),
                                       eins ? VERB(AR, T("ließe sich ", ""), z, T(" Eintrag nicht ganz entfernen", " entry")) : VERB(AR, T("ließen sich ", ""), z, T(" Einträge nicht ganz entfernen", " entries")),
                                       T(" (siehe unten).", " (see below)."));
        else
            kopf = kopf_meldung = VERB(AR, T("Fertig - aber ", "Done - but "), z,
                                       eins ? T(" Eintrag ließ sich nicht ganz entfernen", " entry could not be fully removed") : T(" Einträge ließen sich nicht ganz entfernen", " entries could not be fully removed"),
                                       T(" (siehe unten).", " (see below)."));
    }
    if (erg.Angehalten && erg.Rest == 0) {   /* Codex-Prüfung 75: angehalten erst beim letzten Eintrag - gelöscht ist alles, nur der Schluss fehlt */
        const char *fz = erg.Fehler ? VERB(AR, T(", dazu ", ", plus "), zahl(AR, erg.Fehler), T(" Fehler", " error(s)")) : "";
        if (KN->simulation) kopf = kopf_meldung = T("SIMULATION angehalten - es wurde NICHTS gelöscht. Es war schon der letzte Eintrag.", "SIMULATION stopped - NOTHING was deleted. It was already the last entry.");
        else {
            kopf = VERB(AR, T("Angehalten - es war schon der letzte Eintrag", "Stopped - it was already the last entry"), fz, T(" (siehe oben).", " (see above)."));
            kopf_meldung = VERB(AR, T("Angehalten - es war schon der letzte Eintrag", "Stopped - it was already the last entry"), fz, T(" (siehe Protokoll).", " (see log)."));
        }
    } else if (erg.Angehalten) {   /* NEU 1.0.8 (3.4): ehrlich sagen, dass nicht alles gelöscht ist */
        const char *rest = VERB(AR, zahl(AR, erg.Rest), erg.Rest == 1 ? T(" Eintrag wurde", " entry was") : T(" Einträge wurden", " entries were"));
        if (KN->simulation) kopf = kopf_meldung = VERB(AR, T("SIMULATION angehalten - es wurde NICHTS gelöscht. ", "SIMULATION stopped - NOTHING was deleted. "), rest, T(" nicht mehr geprüft.", " not checked any more."));
        else {
            kopf = VERB(AR, T("Angehalten - ", "Stopped - "), rest, T(" nicht gelöscht", " not deleted"),
                        erg.Fehler ? VERB(AR, T(", dazu ", ", plus "), zahl(AR, erg.Fehler), T(" Fehler", " error(s)")) : "", T(" (siehe oben).", " (see above)."));
            kopf_meldung = VERB(AR, T("Angehalten - ", "Stopped - "), rest, T(" nicht gelöscht", " not deleted"),
                                erg.Fehler ? VERB(AR, T(", dazu ", ", plus "), zahl(AR, erg.Fehler), T(" Fehler", " error(s)")) : "", T(" (siehe Protokoll).", " (see log)."));
        }
    }
    logz(a, kopf);
    logz(a, frei);
    logz(a, kern_kopplungs_text(KN));
    if (smtext[0]) logz(a, smtext);
    if (selbst_text[0]) kopf_meldung = VERB(AR, kopf_meldung, "\n", selbst_text);
    /* NEU 1.0.6: "Datenbank neu aufbauen" nur noch, wenn Kacheln im Menü bleiben. Ohne Sonys Dienst (PC/Windows) wie bisher immer. */
    KN->DB_HINWEIS = kern_bereiche_106() && (!KN->SONY || KN->GEISTER.n > 0);   /* NEU 1.0.7: nie mehr (nur der 1.0.6-Vergleichsmodus der Tests) */
    if (!kern_bereiche_106()) {   /* NEU 1.0.7 (da_Chris86 31.10.2026 19:56): was bleibt, benennen - Neuaufbau nicht empfehlen, PS4-Folgen nennen */
        const char *rt = kern_rest_text(KN, AR);
        if (rt[0]) { sliste z = r_split(AR, rt, "\n"); for (int i = 0; i < z.n; i++) logz(a, z.e[i]); }
        if (KN->PS4_GESCHUETZT > 0) logz(a, VERB(AR, zahl(AR, KN->PS4_GESCHUETZT), T(" Einträge hat der PS4-Schutz zurückgehalten - sie gehören zu installierten PS4-Spielen, die nicht gewählt waren.",
                                                                              " entries were held back by the PS4 protection - they belong to installed PS4 games that were not selected.")));
    }
    if (KN->DB_HINWEIS) {
        if (KN->SONY) logz(a, VERB(AR, T("Diese Einträge bleiben im PS5-Menü sichtbar, bis die Datenbank neu aufgebaut ist: ", "These entries stay visible in the PS5 menu until the database has been rebuilt: "), sl_join(AR, &KN->GEISTER, ", ")));
        const char **sz; int nz = kern_schluss_text(&sz);
        if (KN->simulation) logz(a, T("(SIMULATION - nach einem echten Löschen käme jetzt:)", "(SIMULATION - after real deleting, this would come next:)"));
        for (int i = 0; i < nz; i++) logz(a, sz[i]);
    }
    a->rm_gewaehlt = rm_n; a->rm_fehler = erg.Fehler; a->rm_teilweise = erg.Teilweise; a->rm_geschuetzt = erg.Geschuetzt; a->rm_unveraendert = erg.Unveraendert; a->rm_abgebrochen = rm_ab || erg.Angehalten; a->rm_da = true;   /* NEU 1.0.7: QR-Rückmeldung */
    a->rm_frei = erg.Frei;   /* NEU 1.0.8 (4.5): "X frei geräumt" */
    if (a->ui.fertig) a->ui.fertig(a->ui.ctx, kopf_meldung, erg.Fehler == 0 && erg.Teilweise == 0 && !erg.Angehalten && !unv, frei, smtext);   /* 1.0.8: !unv */
    /* neu nachschauen, damit die Liste stimmt. Klappt das nicht, keine alte Liste stehen lassen. */
    if (KN->simulation && a->neue_verbindung) {
        zugriff *z = a->neue_verbindung(a->verbindung_ctx);
        if (z) { kern_verbinden(KN, z); logz(a, T("SIMULATION: Die Liste zeigt wieder den echten Stand - es wurde nichts gelöscht.", "SIMULATION: the list shows the real state again - nothing was deleted.")); }
    }
    if (a->vor_neusuche) a->vor_neusuche(a->vor_neusuche_ctx);   /* NEU 1.0.8 (Codex-Prüfung 74): Mitschnitt der Test-Fassung */
    VERSUCHE { KN->abbruch = false; kern_suche_funde(KN); }
    FANGE {
        logz(a, VERB(AR, T("Konnte nicht neu nachschauen: ", "Could not scan again: "), ausn_text, T(" - bitte neu verbinden.", " - please connect again.")));
        KN->Funde = (pliste){0}; KN->FUNDE_GUELTIG = false;
    } ENDE_VERSUCHE;
    liste_neu(a);
    stand(a, NULL, 0);
    ablauf_hintergrund_starten(a);
}

/* ------------------------------------------------------------- KomplettAblauf */
static void alles_abwaehlen(ablauf *a) { FUER_ALLE(f) f->Wahl = false; kern_sm_zuruecksetzen(KN); }

void ablauf_komplett(ablauf *a)
{
    ablauf_hintergrund_anhalten(a);
    const char *t = T("VOLLSTÄNDIGE BEREINIGUNG: Auf der internen SSD wird ALLES gelöscht:\n", "FULL CLEANUP: EVERYTHING on the internal SSD will be deleted:\n");
    int extAnz[3] = { 0, 0, 0 };
    const char *extListe[3] = { "", "", "" };
    if (!kern_bereiche_106()) {
        /* NEU 1.0.7 (PS5-Test 03.11.2026, da_Chris86: "es kann doch nicht sein, dass bei der vollständigen Bereinigung die Hälfte fehlt"):
         * alles zeigen - interne SSD, M.2, erweiterter Speicher und was auf jeden Fall bleibt; gezählt nach Spielen/Apps */
        t = VERB(AR, t, komplett_liste(a, ORT_INTERN, NULL));
        { int ap = kern_alte_protokolle(KN);   /* NEU 03.11.2026 (da_Chris86 17:51) */
          if (ap > 0) t = VERB(AR, t, T("   - alte Protokolle von PS5 Cleanup auf der Konsole: ", "   - old PS5 Cleanup logs on the console: "), zahl(AR, ap),
                               T(" (die auf dem USB-Laufwerk bleiben)\n", " (those on the USB drive stay)\n")); }
        bool bedingt = false; int pak = kern_komplett_pakete(KN, &bedingt);
        if (pak > 0) t = VERB(AR, t, T("   - Trophäen-Listen installierter Spiele: ", "   - Trophy lists of installed games: "), zahl(AR, pak),
                              bedingt ? T(" - gehen nur mit, wenn auf keinem Speicher ein Spiel bleibt\n", " - only go if no game stays on any storage\n") : "\n");
        for (int o = ORT_M2; o <= ORT_USB; o++) {
            extListe[o] = komplett_liste(a, o, &extAnz[o]);
            if (extAnz[o] > 0) t = VERB(AR, t, "\n", o == ORT_M2 ? T("Auf der M.2-SSD", "On the M.2 SSD") : T("Auf dem erweiterten Speicher (USB-Festplatte für Spiele)", "On the extended storage (USB drive for games)"),
                                         T(" - gleich kommt die Frage: behalten oder auch leeren.\n", " - you will be asked next: keep or also wipe.\n"), extListe[o]);
        }
        if (extAnz[ORT_M2] > 0 || extAnz[ORT_USB] > 0)
            t = VERB(AR, t, T("\n(Hat ein Spiel Daten auf mehreren Speichern, steht es bei jedem - z. B. ShadowMount-Spiel: Kachel intern, Spieldatei auf der M.2.)\n",
                              "\n(A game with data on several storages is listed at each of them - e.g. ShadowMount game: tile internal, game file on the M.2.)\n"));
        t = VERB(AR, t, T("\nDas heißt:\n - Spielstände ohne Cloud-Sicherung (PS Plus) sind endgültig weg.\n - Nie mit dem PSN abgeglichene Trophäen sind weg.\n - Jailbreak-Tools samt Einstellungen sind weg - danach den Jailbreak NEU EINRICHTEN.\n",
                          "\nThis means:\n - Saved data without a cloud backup (PS Plus) is gone for good.\n - Trophies never synced with PSN are gone.\n - Jailbreak tools and their settings are gone - set up your jailbreak AGAIN afterwards.\n"));
        t = VERB(AR, t, T("\nBleibt auf jeden Fall:\n", "\nStays in any case:\n"));
        int jb = kern_anzahl_jailbreak(KN, true, true);
        if (jb > 0) t = VERB(AR, t, T(" - Jailbreak-Einstiege: ", " - Jailbreak entries: "), zahl(AR, jb),
                             T(" - die vollständige Bereinigung fasst den Bereich „Jailbreak“ nie an (einzeln löschen geht dort)\n", " - the full cleanup never touches the \"Jailbreak\" section (you can delete them individually there)\n"));
        if (KN->STICK_DA) {
            int st = 0; FUER_ALLE(f) if (f->Ort == ORT_STICK && !f->SmDatei) st++;
            t = VERB(AR, t, T(" - USB-Laufwerk mit Dateien (PKG, ELF, Protokoll): ", " - USB drive with files (PKG, ELF, log): "), zahl(AR, st), eintraege(st),
                     T(" - wird nie angefasst (einzeln löschen geht)\n", " - never touched (you can delete them individually)\n"));
        }
        int gesp = 0; FUER_ALLE(f) if (f->Gesperrt && !f->SmDatei && !kern_selbst_gesperrt(f)) gesp++;   /* Codex-Prüfung 48: PS5 Cleanup selbst - hängt an der Antwort zu M.2/erweitertem Speicher, steht nicht unter "auf jeden Fall" */
        if (gesp > 0) t = VERB(AR, t, T(" - Gesperrt: ", " - Locked: "), zahl(AR, gesp), eintraege(gesp), T(" (der Grund steht am Eintrag)\n", " (the reason is shown at the entry)\n"));
        t = VERB(AR, t, T(" - Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, echte Lizenzen, Sony-Systemanwendungen.", " - system software, disc drive pairing, user account, real licenses, Sony system apps."));
    } else {   /* Text bis 1.0.6 (nur PC-Vergleichstests) */
        /* 1.0.7: in Anzeige-Reihenfolge; "Jailbreak" (7) gehört nie zur vollständigen Bereinigung und wird hier nicht aufgezählt */
        for (int ki = 0; ki < KAT_ANZ; ki++) {
            int k = ALLE_KAT[ki]; if (k == 7) continue;
            int n = 0; mtab bl = mt_neu(false);
            FUER_ALLE(f) if (f->Kat == k && f->Ort == ORT_INTERN && !f->Gesperrt && !f->Paket && !f->NurEinzeln && !f->SmDatei) { n++; mt_setze(AR, &bl, block_schluessel(AR, f), (void *)1); }
            t = VERB(AR, t, "   - ", kern_kat_name(k), ": ", bereich_zahl(AR, k, bl.n, n), "\n");
        }
        t = VERB(AR, t, T("(M.2 und USB-Speicher: gleich wird jeweils gefragt. USB-Sticks bleiben unangetastet.)\n", "(M.2 and USB storage: you will be asked for each. USB sticks stay untouched.)\n"));
        t = VERB(AR, t, T("\nDas heißt:\n - Spielstände ohne Cloud-Sicherung (PS Plus) sind endgültig weg.\n - Nie mit dem PSN abgeglichene Trophäen sind weg.\n - Jailbreak-Tools samt Einstellungen sind weg - danach den Jailbreak NEU EINRICHTEN.\n\nBleibt: Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, echte Lizenzen, Sony-Systemanwendungen.",
                          "\nThis means:\n - Saved data without a cloud backup (PS Plus) is gone for good.\n - Trophies never synced with PSN are gone.\n - Jailbreak tools and their settings are gone - set up your jailbreak AGAIN afterwards.\n\nKept: system software, disc drive pairing, user account, real licenses, Sony system apps."));
        /* bis 1.0.6 (nur PC-Vergleichstests): Jailbreak-Einstiege - danach wird gefragt */
        if (kern_anzahl_jailbreak(KN, true, true) > 0)
            t = VERB(AR, t, T("\nMögliche Jailbreak-Einstiege (YouTube, Netflix, Lua-Spiele, PS2-Spielstände, Browser): ", "\nPossible jailbreak entries (YouTube, Netflix, Lua games, PS2 saves, browser): "), zahl(AR, kern_anzahl_jailbreak(KN, true, true)),
                     T(" - gleich wird gefragt, ob sie bleiben.", " - you will be asked next whether they stay."));
    }
    t = VERB(AR, t, kern_speicher_hinweis(KN));
    /* NEU 1.1 */
    const char *sh = kern_selbstschutz_hinweis(KN, true);
    if (sh[0]) t = VERB(AR, t, "\n\n", sh);
    if (KN->simulation) t = VERB(AR, T("SIMULATION - es wird NICHTS gelöscht.\n\n", "SIMULATION - NOTHING will be deleted.\n\n"), t);
    if (!frage_wahl(a, t, T("Vollständige Bereinigung", "Full cleanup"), T("Abbrechen", "Cancel"), T("Weiter", "Continue"))) { ablauf_hintergrund_starten(a); return; }
    /* da_Chris86 28.10.2026: M.2 und erweiterter Speicher gehören zur vollständigen Bereinigung - je auf Nachfrage (wie 1.0.5 bei der M.2).
     * Zählt die Spieldateien von ShadowMount-Spielen mit, die auf diesem Speicher liegen. USB-Sticks nie (nur einzeln).
     * 1.0.7: gezählt wie im ersten Fenster (komplett_liste); bis 1.0.6 wie unten. */
    /* Codex-Prüfung 37: Jailbreak-Einstiege auf M.2/USB zählen mit (sonst fehlt die Frage, wenn dort nur Einstiege liegen) */
    if (kern_bereiche_106())
        FUER_ALLE(f) if (!f->Gesperrt && !f->SmDatei && (!f->NurEinzeln || f->Jailbreak)) { int o = f->Quelle[0] ? kern_pfad_ort(f->Quelle) : f->Ort; if (o == ORT_M2 || o == ORT_USB) extAnz[o]++; }
    bool leeren[3] = { false, false, false };
    for (int o = ORT_M2; o <= ORT_USB; o++) {
        if (extAnz[o] == 0) continue;
        if (!kern_bereiche_106()) {
            /* PS5-Test 03.11.2026: dieselbe Aufzählung wie im ersten Fenster, klarer Name (erweiterter Speicher = USB-Festplatte für Spiele) */
            const char *kurz = o == ORT_M2 ? T("M.2-SSD", "M.2 SSD") : T("Erweiterten Speicher", "Extended storage");
            leeren[o] = frage_wahl(a, VERB(AR, o == ORT_M2 ? T("Auf der M.2-SSD liegen:\n", "On the M.2 SSD there are:\n") : T("Auf dem erweiterten Speicher (USB-Festplatte für Spiele) liegen:\n", "On the extended storage (USB drive for games) there are:\n"),
                extListe[o],
                T("\nBEHALTEN: Die Spiele bleiben. Nur die Kacheln der ShadowMount-Spiele, deren Spieldateien darauf liegen, gehen weg (die Kacheln liegen intern) - sie kommen wieder, sobald ShadowMount neu eingerichtet ist.\n\nAUCH LEEREN: Alle Spiele und Dateien darauf werden endgültig gelöscht.",
                  "\nKEEP: the games stay. Only the tiles of ShadowMount games whose game files are on it go (the tiles are internal) - they come back once ShadowMount is set up again.\n\nALSO WIPE: all games and files on it are deleted permanently.")),
                o == ORT_M2 ? T("M.2-SSD", "M.2 SSD") : T("Erweiterter Speicher (USB-Festplatte)", "Extended storage (USB drive)"),
                VERB(AR, kurz, T(" behalten (empfohlen)", ": keep (recommended)")), VERB(AR, kurz, T(" auch leeren", ": also wipe")));
            continue;
        }
        const char *n = kern_ort_name(o);
        leeren[o] = frage_wahl(a, VERB(AR, T("Auf ", "On "), o == ORT_M2 ? T("der M.2-SSD", "the M.2 SSD") : T("dem USB-Speicher", "the USB storage"), T(" liegen ", " there are "), zahl(AR, extAnz[o]), eintraege(extAnz[o]),
            T(" (Spiele, Ordner, Dateien).\n\nBEHALTEN: Die Spiele bleiben. Nur Kacheln von ShadowMount-Spielen darauf gehen weg - sie kommen wieder, sobald ShadowMount neu eingerichtet ist.\n\nAUCH LEEREN: Alle Spiele und Dateien darauf werden endgültig gelöscht.",
              " (games, folders, files).\n\nKEEP: the games stay. Only tiles of ShadowMount games on it go - they come back once ShadowMount is set up again.\n\nALSO WIPE: all games and files on it are deleted permanently.")),   /* Text bis 1.0.6 (nur PC-Vergleichstests) */
            n, VERB(AR, n, T(" behalten (empfohlen)", ": keep (recommended)")), VERB(AR, n, T(" auch leeren", ": also wipe")));
    }
    bool m2Leeren = leeren[ORT_M2], usbLeeren = leeren[ORT_USB];
    /* NEU 1.1 (Anforderungen 27.10.2026): Jailbreak-Einstiege nur auf ausdrücklichen Wunsch */
    int jbAnz = kern_bereiche_106() ? kern_anzahl_jailbreak(KN, m2Leeren, usbLeeren) : 0;   /* 1.0.7: keine Frage - Jailbreak bleibt */
    bool jbLoeschen = false;
    if (jbAnz > 0)
        jbLoeschen = frage_wahl(a, VERB(AR, T("Gefunden: ", "Found: "), zahl(AR, jbAnz), T(" mögliche Jailbreak-Einstiege (YouTube, Netflix, Lua-Spiele, PS2-Spielstände, Browser) samt ihren Daten.", " possible jailbreak entries (YouTube, Netflix, Lua games, PS2 saves, browser) with their data."),
            T("\n\nBEHALTEN: Sie bleiben - dieser Jailbreak-Weg funktioniert weiter.\n\nAUCH LÖSCHEN: Sie werden mit gelöscht. Danach funktioniert dieser Jailbreak-Weg erst wieder, wenn er neu eingerichtet ist.",
              "\n\nKEEP: they stay - this jailbreak method keeps working.\n\nALSO DELETE: they are deleted too. This jailbreak method only works again once it has been set up again.")),
            T("Jailbreak-Einstiege", "Jailbreak entries"), T("Behalten (empfohlen)", "Keep (recommended)"), T("Auch löschen", "Also delete"));
    kern_waehle_komplett(KN, m2Leeren, usbLeeren);
    if (jbLoeschen) kern_jailbreak_dazu(KN, m2Leeren, usbLeeren);
    liste_neu(a);                                               /* ZeigeHaken(); AktualisiereStand(); */
    { bool paketBleibt = false; FUER_ALLE(f) if (f->Paket && !f->Wahl) paketBleibt = true;
      if (paketBleibt) logz(a, kern_bereiche_106() ? T("(Trophäen-Listen bleiben, weil Spiele auf M.2 oder USB bleiben.)", "(Trophy lists stay because games on M.2 or USB stay.)")
                                                   : T("(Trophäen-Listen bleiben, weil Spiele auf der M.2, dem erweiterten Speicher oder dem USB-Laufwerk bleiben.)", "(Trophy lists stay because games on the M.2, the extended storage or the USB drive stay.)")); }
    VERSUCHE { groesse_ermitteln(a); }
    FANGE { if (ausn_art == AUSN_ABBRUCH) { alles_abwaehlen(a); liste_neu(a); } WEITERWERFEN(); } ENDE_VERSUCHE;
    /* da_Chris86 28.10.2026: deutliche Warnung mit der echten Anzahl, wenn M.2 oder USB mit geleert werden
     * (gezählt nach der Auswahl - mit Jailbreak-Einträgen und ShadowMount-Spieldateien, Codex-Prüfung 28) */
    int gewAnz[3] = { 0, 0, 0 };
    mtab gewBl[3] = { mt_neu(false), mt_neu(false), mt_neu(false) };
    FUER_ALLE(f) if (f->Wahl && !(f->SmDatei && !kern_bereiche_106())) {   /* Codex-Prüfung 48: Spieldatei zählt über ihre Kachel (Pfade), nicht doppelt */
        int o = f->Ort;
        if (f->Quelle[0]) { o = ORT_INTERN; for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], f->Quelle)) o = kern_pfad_ort(f->Quelle); }
        if (o == ORT_M2 || o == ORT_USB) { gewAnz[o]++; mt_setze(AR, &gewBl[o], block_schluessel(AR, f), (void *)1); }
    }
    const char *warn = "";
    for (int o = ORT_M2; o <= ORT_USB; o++) {
        if (extAnz[o] == 0) continue;
        if (!kern_bereiche_106()) {   /* PS5-Test 03.11.2026: klarer Name, gezählt nach Spielen/Apps */
            if (leeren[o]) warn = VERB(AR, warn, T("!!! ACHTUNG: ", "!!! WARNING: "), o == ORT_M2 ? T("DIE M.2-SSD", "THE M.2 SSD") : T("DER ERWEITERTE SPEICHER (USB-FESTPLATTE FÜR SPIELE)", "THE EXTENDED STORAGE (USB DRIVE FOR GAMES)"),
                                       T(" WIRD MIT GELEERT - ", " WILL BE WIPED TOO - "), anzahl_bloecke(AR, gewBl[o].n, gewAnz[o]), T(", alle Spiele und Dateien darauf sind danach weg !!!\n\n", ", all games and files on it will be gone !!!\n\n"));
            else warn = VERB(AR, warn, o == ORT_M2 ? T("Die M.2-SSD", "The M.2 SSD") : T("Der erweiterte Speicher (USB-Festplatte für Spiele)", "The extended storage (USB drive for games)"), T(" bleibt unangetastet.\n\n", " stays untouched.\n\n"));
            continue;
        }
        if (leeren[o]) warn = VERB(AR, warn, T("!!! ACHTUNG: ", "!!! WARNING: "), o == ORT_M2 ? T("DIE M.2-SSD", "THE M.2 SSD") : T("DER USB-SPEICHER", "THE USB STORAGE"),
                                   T(" WIRD MIT GELEERT - ", " WILL BE WIPED TOO - "), zahl(AR, gewAnz[o]), eintraege(gewAnz[o]), T(", alle Spiele und Dateien darauf sind danach weg !!!\n\n", ", all games and files on it will be gone !!!\n\n"));
        else warn = VERB(AR, warn, o == ORT_M2 ? T("Die M.2-SSD", "The M.2 SSD") : T("Der USB-Speicher", "The USB storage"), T(" bleibt unangetastet.\n\n", " stays untouched.\n\n"));
    }
    if (jbAnz > 0 && !jbLoeschen) warn = VERB(AR, warn, T("Die Jailbreak-Einstiege bleiben.\n\n", "The jailbreak entries stay.\n\n"));
    a->leeren_m2 = m2Leeren; a->leeren_usb = usbLeeren;   /* Codex-Prüfung 50: die Zusammenfassung muss wissen, was mit geleert wird */
    const char *antw = frage_text(a, VERB(AR, warn, ablauf_zusammenfassung(a, true), T("\n\nZum Bestätigen LÖSCHEN eintippen (alles andere = abbrechen):", "\n\nType DELETE to confirm (anything else = cancel):")), T("Endgültig löschen?", "Delete permanently?"));
    bool bestaetigt = antw != NULL && (SP_EN ? !strcmp(antw, "DELETE") : (!strcmp(antw, "LOESCHEN") || !strcmp(antw, "LÖSCHEN")));
    if (!bestaetigt) {
        logz(a, T("Abgebrochen - nichts verändert.", "Cancelled - nothing changed."));
        a->leeren_m2 = a->leeren_usb = false;
        alles_abwaehlen(a); liste_neu(a); ablauf_hintergrund_starten(a);
        return;
    }
    a->selbst_modus = 1;                     /* vollständige Bereinigung: PS5 Cleanup am Ende ohne Frage mit entfernen */
    a->leeren_m2 = m2Leeren; a->leeren_usb = usbLeeren;
    loesche_jetzt(a);
    a->selbst_modus = 0; a->leeren_m2 = a->leeren_usb = false;
}

/* ------------------------------------------------------------- AuswahlAblauf */
void ablauf_auswahl(ablauf *a)
{
    ablauf_hintergrund_anhalten(a);
    pliste sm = kern_sm_gewaehlt(KN);
    /* NEU 1.0.7 (da_Chris86 31.10.2026 12:51): keine Abfrage mehr - Kachel und Spieldatei sind eigene Zeilen */
    if (!kern_bereiche_106()) kern_sm_nach_auswahl(KN);
    else if (sm.n > 0) {
        sliste z = {0}; for (int i = 0; i < sm.n; i++) sl_add(AR, &z, VERB(AR, "   ", ((eintrag *)sm.e[i])->Text));
        const char *m2z = "";   /* bis 27.10.2026: Zusatz "Spiele auf der M.2 bleiben" (U20) - ersetzt durch U24, wieder wie 1.0.5 */
        bool mit = frage_wahl(a, VERB(AR, T("Du hast ", "You selected "), zahl(AR, sm.n), T(" ShadowMount-Spiel(e) gewählt:\n", " ShadowMount game(s):\n"), sl_join(AR, &z, "\n"),
            T("\n\nNUR KACHELN LÖSCHEN: Die Spiele bleiben auf der Platte und erscheinen wieder, sobald ShadowMount neu geladen ist.\n\nSPIELE + KACHELN LÖSCHEN: Die Spieldateien werden endgültig gelöscht.",
              "\n\nDELETE TILES ONLY: the games stay on the SSD/M.2 and come back once ShadowMount is loaded again.\n\nDELETE GAMES + TILES: the game files are deleted permanently."), m2z),
            T("ShadowMount-Spiele", "ShadowMount games"), T("Nur Kacheln löschen (empfohlen)", "Delete tiles only (recommended)"), T("Spiele + Kacheln löschen", "Delete games + tiles"));
        kern_setze_sm_spieldateien(KN, mit);
    }
    else if (kern_bereiche_106()) kern_sm_zuruecksetzen(KN);
    pliste m2 = kern_ext_gewaehlt(KN);   /* NEU 1.0.6: M.2, USB-Speicher und USB-Stick */
    if (m2.n > 0) {
        sliste z = {0}; for (int i = 0; i < m2.n && i < 15; i++) sl_add(AR, &z, VERB(AR, "   ", ((eintrag *)m2.e[i])->Text));
        const char *l = sl_join(AR, &z, "\n");
        if (m2.n > 15) l = VERB(AR, l, T("\n   ... und ", "\n   ... and "), zahl(AR, m2.n - 15), T(" weitere", " more"));
        /* NEU 1.0.8 (F2, wie in der Zusammenfassung): nur einzelne Dateien gewählt (z. B. Protokolle auf dem USB-Laufwerk) - dann keine
         * Warnung "Alles, was darin liegt", das klang nach dem ganzen Laufwerk. Hier ist meist nur im Hintergrund gezählt (die Zählung
         * für die Abschlussfrage kommt erst danach) - ein Ordner zählt dort als Ordner (baum_groesse). Ohne Zählung bleibt die Warnung. */
        int nd = 0, dz = 0;
        for (int i = 0; i < m2.n; i++) {
            eintrag *f = m2.e[i];
            if (f->Quelle[0] || f->Pfade.n == 0) continue;
            int ordner = -1, dateien = 0;
            if (f->HatGroesse) { ordner = f->Ordner; dateien = f->Dateien; }
            else { groesse *g = kern_eintrag_groesse_in(KN, AR, f, false); if (g) { ordner = g->Ordner; dateien = g->Dateien; } }
            if (ordner == 0 && dateien >= 1) { nd++; dz += dateien; }
        }
        bool nurDateien = !kern_bereiche_106() && nd == m2.n;
        const char *schluss = !nurDateien ? T("\n\nAlles, was darin liegt (Spiele, Ordner, Dateien), wird endgültig gelöscht.\n\nGelöscht wird erst nach der Abschlussfrage.", "\n\nEverything in them (games, folders, files) will be deleted permanently.\n\nNothing is deleted before the final question.")
                            : dz == 1 ? T("\n\nDiese Datei wird endgültig gelöscht.\n\nGelöscht wird erst nach der Abschlussfrage.", "\n\nThis file will be deleted permanently.\n\nNothing is deleted before the final question.")
                                      : T("\n\nDiese Dateien werden endgültig gelöscht.\n\nGelöscht wird erst nach der Abschlussfrage.", "\n\nThese files will be deleted permanently.\n\nNothing is deleted before the final question.");
        if (!frage_wahl(a, VERB(AR, nurDateien ? T("Du hast ", "You selected ") : T("ACHTUNG - du hast ", "WARNING - you selected "), zahl(AR, m2.n),
                kern_bereiche_106() ? (m2.n == 1 ? T(" Eintrag auf M.2/USB gewählt:\n", " entry on M.2/USB:\n") : T(" Einträge auf M.2/USB gewählt:\n", " entries on M.2/USB:\n"))
                                    : (m2.n == 1 ? T(" Eintrag außerhalb der internen SSD gewählt (M.2, erweiterter Speicher, USB-Laufwerk):\n", " entry outside the internal SSD (M.2, extended storage, USB drive):\n")
                                                 : T(" Einträge außerhalb der internen SSD gewählt (M.2, erweiterter Speicher, USB-Laufwerk):\n", " entries outside the internal SSD (M.2, extended storage, USB drive):\n")), l,
                schluss),
                kern_bereiche_106() ? "M.2 / USB" : T("Außerhalb der internen SSD", "Outside the internal SSD"), T("Abbrechen", "Cancel"), T("Weiter (diese Einträge löschen)", "Continue (delete these entries)")))
        { logz(a, T("Abgebrochen - nichts verändert.", "Cancelled - nothing changed.")); kern_sm_zuruecksetzen(KN); ablauf_hintergrund_starten(a); return; }
    }
    /* NEU 1.0.6 (da_Chris86 29.10.2026 07:28): gewählte Jailbreak-Einstiege - deutliche Warnung mit Namen */
    {
        sliste z = {0}; int jn = 0;
        FUER_ALLE(f) if (f->Wahl && f->Jailbreak && !f->Gesperrt) { jn++; if (z.n < 15) sl_add(AR, &z, VERB(AR, "   ", f->Text, " → ", kern_jb_weg_eintrag(f))); }
        if (jn > 0) {
            const char *l = sl_join(AR, &z, "\n");
            if (jn > 15) l = VERB(AR, l, T("\n   ... und ", "\n   ... and "), zahl(AR, jn - 15), T(" weitere", " more"));
            if (!frage_wahl(a, VERB(AR, T("ACHTUNG - du hast ", "WARNING - you selected "), zahl(AR, jn), jn == 1 ? T(" Eintrag gewählt, der zu deinem Jailbreak gehört:\n", " entry that belongs to your jailbreak:\n") : T(" Einträge gewählt, die zu deinem Jailbreak gehören:\n", " entries that belong to your jailbreak:\n"), l,
                    T("\n\nWerden sie gelöscht, funktioniert dieser Jailbreak erst wieder, wenn er neu eingerichtet ist.\n\nGelöscht wird erst nach der Abschlussfrage.", "\n\nIf they are deleted, this jailbreak only works again once it has been set up again.\n\nNothing is deleted before the final question.")),
                    "Jailbreak", T("Abbrechen", "Cancel"), T("Weiter (trotzdem löschen)", "Continue (delete anyway)")))
            { logz(a, T("Abgebrochen - nichts verändert.", "Cancelled - nothing changed.")); kern_sm_zuruecksetzen(KN); ablauf_hintergrund_starten(a); return; }
        }
    }
    groesse_ermitteln(a);
    if (!frage_wahl(a, ablauf_zusammenfassung(a, false), T("Endgültig löschen?", "Delete permanently?"), T("Abbrechen", "Cancel"), T("Jetzt endgültig löschen", "Delete permanently now")))
    { logz(a, T("Abgebrochen - nichts verändert.", "Cancelled - nothing changed.")); kern_sm_zuruecksetzen(KN); ablauf_hintergrund_starten(a); return; }
    a->selbst_modus = 2;                     /* Auswahl: am Ende fragen, ob PS5 Cleanup auch weg soll */
    loesche_jetzt(a);
    a->selbst_modus = 0;
}

/* ------------------------------------------------------------- Suchen (Teil aus Verbinde) */
void ablauf_suchen(ablauf *a, const char *modell, const char *firmware)
{
    ablauf_hintergrund_anhalten(a);
    const char *reihe = kern_modell_reihe(KN->MODELL_TYP, KN->MODELL_DIGITAL);
    if (!modell || !modell[0]) modell = T("unbekannt", "unknown");
    logz(a, VERB(AR, T("Modell: ", "Model: "), modell, T("  Reihe: ", "  Series: "), reihe, "  Firmware: ", firmware ? firmware : ""));
    logz(a, VERB(AR, T("Laufwerk: ", "Disc drive: "), kern_laufwerk_text(KN)));
    logz(a, T("Schaue nach, was drauf ist ...", "Scanning what is on it ..."));
    stand(a, T("Schaue nach, was auf der PS5 liegt ...", "Scanning what is on the PS5 ..."), ABLAUF_UNBESTIMMT);
    a->abbrechbar = true;
    VERSUCHE { kern_suche_funde(KN); }                         /* Fehler gehen wie in 1.0.5 an Starte() */
    FANGE { a->abbrechbar = false; WEITERWERFEN(); } ENDE_VERSUCHE;
    a->abbrechbar = false;
    stand(a, NULL, 0);
    if (KN->KOMPRIMIERUNG.n > 0) {
        logz(a, T("ACHTUNG: Der Game Compressor arbeitet gerade (oder wurde abgebrochen):", "WARNING: The Game Compressor is working right now (or was interrupted):"));
        for (int i = 0; i < KN->KOMPRIMIERUNG.n; i++) logz(a, VERB(AR, "   ", KN->KOMPRIMIERUNG.e[i]));
        logz(a, T("Das betroffene Spiel ist GESPERRT. Lass den Game Compressor erst fertig werden.", "The affected game is LOCKED. Let the Game Compressor finish first."));
    }
    liste_neu(a);
    if (KN->Funde.n == 0) logz(a, T("Nichts zu tun - die PS5 ist bereits sauber.", "Nothing to do - the PS5 is already clean."));
    else logz(a, VERB(AR, zahl(AR, KN->Funde.n), eintraege(KN->Funde.n), T(" gefunden. Nichts wurde verändert.", " found. Nothing was changed.")));
    { const char *h = h_trim(AR, kern_speicher_hinweis(KN)); if (h[0]) logz(a, h); }
    /* NEU 1.0.7 (da_Chris86 31.10.2026 19:56): installierte PS4-Spiele ins Protokoll (Name, Kennung, Ort, Größe, Update, Lizenz) */
    if (!kern_bereiche_106() && KN->PS4.n > 0) {
        logz(a, VERB(AR, T("PS4-Spiele (geschützt, solange du das Spiel selbst nicht anhakst): ", "PS4 games (protected unless you tick the game itself): "), zahl(AR, KN->PS4.n)));
        for (int i = 0; i < KN->PS4.n; i++) logz(a, VERB(AR, "   ", kern_ps4_zeile(AR, KN->PS4.e[i])));
    }
    /* NEU 1.0.7 (PS5-Test 02.11.2026, Red Dead Redemption 2): kaputte Zusatzinhalt-Angaben deutlich melden */
    for (int i = 0; i < KN->SID_WARNUNG.n; i++) logz(a, KN->SID_WARNUNG.e[i]);
    if (KN->simulation) logz(a, VERB(AR, "(", sim_kurz(a), ")"));
    ablauf_hintergrund_starten(a);
}

/* ------------------------------------------------------------- Starte */
/* NEU 1.0.7: Text der Sicherheitsbremse (Speicher knapp) */
static void bremse_melden(ablauf *a)
{
    logz(a, VERB(AR, T("SICHERHEITSSTOPP: Der Arbeitsspeicher der PS5 wird knapp (", "SAFETY STOP: The PS5 is running low on memory ("), (const char *)zugriff_speicher_grund,
                 T("). PS5 Cleanup hat angehalten, damit die Konsole nicht einfriert. Bitte PS5 Cleanup beenden (○ halten), die PS5 neu starten und es dann nochmal versuchen.",
                   "). PS5 Cleanup has stopped so the console does not freeze. Please quit PS5 Cleanup (hold ○), restart the PS5 and then try again.")));
}
void ablauf_starte(ablauf *a, void (*fn)(void *), void *arg)
{
    KN->abbruch = false;
    zugriff_speicher_gebremst = 0;
    zugriff_bremse_hier = 1;      /* NEU 1.0.7: hier wird jedes Anhalten aufgefangen (FANGE unten) */
    VERSUCHE { fn(arg); }
    FANGE {
        if (ausn_art == AUSN_ABBRUCH && zugriff_speicher_gebremst) {
            bremse_melden(a);
            stand(a, T("Sicherheitsstopp - Speicher knapp.", "Safety stop - low memory."), 0);
            kern_sm_zuruecksetzen(KN);
        } else if (ausn_art == AUSN_ABBRUCH) {
            logz(a, T("Abgebrochen - nichts gelöscht.", "Cancelled - nothing deleted."));
            stand(a, T("Abgebrochen.", "Cancelled."), 0);
            kern_sm_zuruecksetzen(KN);
            if (ablauf_verbunden(a)) ablauf_hintergrund_starten(a);
        } else {
            const char *msg = a_kopie(AR, ausn_text);
            logz(a, VERB(AR, T("FEHLER bei ", "ERROR in "), T("dem Ablauf", "the process"), " : ", msg));
            if (a->ui.fehler) a->ui.fehler(a->ui.ctx, VERB(AR, T("Es ist ein Fehler aufgetreten:\n", "An error occurred:\n"), msg,
                T("\n\nDas Protokoll liegt hier:\n", "\n\nThe log is here:\n"), a->protokoll_datei ? a->protokoll_datei : "",
                T("\n\nBitte diese Datei bei einer Fehlermeldung mitschicken.", "\n\nPlease attach this file when reporting the problem.")));
            stand(a, NULL, 0);
        }
    } ENDE_VERSUCHE;
    zugriff_bremse_hier = 0;
    liste_neu(a);
}
