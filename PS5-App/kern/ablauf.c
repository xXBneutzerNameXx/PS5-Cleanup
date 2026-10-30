/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
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
void ablauf_bereich_setzen(ablauf *a, int kat, bool an)
{
    /* Trophäen-Listen noch installierter Spiele (gelb) nur einzeln - beim Abwählen gehen sie mit */
    /* NEU 1.1: "nur einzeln" Wählbares (M.2, Browserspeicher) ebenso - beim Abwählen geht es mit */
    FUER_ALLE(f) if (f->Kat == kat && !f->Gesperrt && !((f->Paket || f->NurEinzeln) && an)) f->Wahl = an;
    kern_abhaengige_mitnehmen(KN);
}
int ablauf_bereich_zustand(ablauf *a, int kat)
{
    int n = 0, w = 0;
    /* NEU 1.1: nicht gewähltes "nur einzeln" zählt nicht mit - sonst ließe sich ein Bereich nie ganz abwählen */
    FUER_ALLE(f) if (f->Kat == kat && !f->Gesperrt && (!f->NurEinzeln || f->Wahl)) { n++; if (f->Wahl) w++; }
    return w == 0 ? 0 : (w == n ? 2 : 1);
}
/* NEU 1.1: true, wenn im Bereich nichts gesammelt wählbar ist, aber etwas einzeln (z. B. die M.2 mit Selbstschutz) */
bool ablauf_bereich_nur_einzeln(ablauf *a, int kat)
{
    int n = 0, e = 0;
    FUER_ALLE(f) if (f->Kat == kat && !f->Gesperrt) { n++; if (f->NurEinzeln || f->Paket) e++; }
    return n > 0 && e == n;
}
bool ablauf_eintrag_gesperrt(ablauf *a, const eintrag *f, const char **grund)
{
    *grund = NULL;
    if (f->Gesperrt) { *grund = f->SperrGrund ? f->SperrGrund : T("Gesperrt - der Game Compressor arbeitet gerade daran.", "Locked - the Game Compressor is working on it."); return true; }
    if (f->Teil != NULL && strcmp(f->Teil, "Spiel") != 0 && f->Gruppe && gruppe_gewaehlt(a, f->Gruppe)) { *grund = T("Geht mit dem Spiel weg.", "Goes with the game."); return true; }
    if (f->SmTipp) *grund = T("ShadowMount: Wird das Spiel gelöscht, verschwindet auch seine Kachel im Menü.", "ShadowMount: if the game is deleted, its tile disappears from the menu too.");
    else if (f->Jailbreak) *grund = kern_jb_hinweis(TA, f);   /* NEU 1.1 (Anforderungen 27.10.2026); 1.0.6 mit Namen (da_Chris86 29.10.2026) */
    else if (f->NurEinzeln && f->Ort == ORT_STICK) *grund = T("USB-Stick: nur einzeln wählbar - nie über \"ganzen Bereich wählen\" und nie in der vollständigen Bereinigung.", "USB stick: individually only - never via \"select the whole area\" and never in the full cleanup.");   /* NEU 1.0.6 */
    else if (f->NurEinzeln) *grund = T("Nur einzeln wählbar - nicht über \"ganzen Bereich wählen\".", "Individually only - not via \"select the whole area\".");   /* NEU 1.1 */
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
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        int k = ALLE_KAT[ki], c = 0;
        for (int i = 0; i < g.n; i++) if (((eintrag *)g.e[i])->Kat == k) c++;
        if (c > 0) t = VERB(AR, t, "   - ", kern_kat_name(k), ": ", zahl(AR, c), "\n");
    }
    int ext[4] = { 0, 0, 0, 0 }, smDatei = 0;
    for (int i = 0; i < g.n; i++) {
        eintrag *f = g.e[i];
        int o = f->Ort;
        if (f->Quelle[0]) { o = ORT_INTERN; for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], f->Quelle)) o = kern_pfad_ort(f->Quelle); }   /* Codex-Prüfung 28 */
        if (o >= 0 && o <= 3) ext[o]++;
        if (f->Kat == 1 && f->Quelle[0]) for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], f->Quelle)) { smDatei++; break; }
    }
    if (smDatei > 0) t = VERB(AR, t, T("   davon ShadowMount-Spiele MIT Spieldatei: ", "   of these, ShadowMount games WITH game file: "), zahl(AR, smDatei), "\n");
    /* NEU 1.0.6: je Speicher außerhalb der internen SSD */
    for (int o = ORT_M2; o <= ORT_STICK; o++) {
        if (komplett && ext[o] == 0 && (o == ORT_M2 ? KN->m2Da : o == ORT_USB ? KN->USB_SPEICHER : false))
            t = VERB(AR, t, "   (", kern_ort_name(o), T(" bleibt unverändert)\n", " stays unchanged)\n"));
        if (!komplett && ext[o] > 0)
            t = VERB(AR, T("!!! ACHTUNG: ", "!!! WARNING: "), zahl(AR, ext[o]), T(" EINTRÄGE AUF ", " ENTRIES ON "), kern_ort_name(o), T(" SIND GEWÄHLT - was darin liegt (Spiele, Dateien), ist danach weg !!!\n\n", " ARE SELECTED - whatever is in them (games, files) will be gone !!!\n\n"), t);
    }
    int gesp = 0, gesp11 = 0; FUER_ALLE(f) if (f->Gesperrt) { if (f->SperrGrund) gesp11++; else gesp++; }
    if (gesp > 0) t = VERB(AR, t, T("   Bleibt (Game Compressor arbeitet daran): ", "   Kept (Game Compressor is working on it): "), zahl(AR, gesp), "\n");
    if (gesp11 > 0) t = VERB(AR, t, T("   Bleibt (gesperrt, der Grund steht am Eintrag): ", "   Kept (locked, the reason is shown at the entry): "), zahl(AR, gesp11), "\n");   /* NEU 1.1 */
    if (!komplett) { const char *h = kern_speicher_hinweis(KN); if (h[0]) { while (*h == '\n') h++; t = VERB(AR, t, h, "\n"); } }
    t = VERB(AR, t, T("\nDas sind ", "\nThat is "), kern_groesse_satz(AR, a->GROSS), ".\n");
    t = VERB(AR, t, "\n", kern_kopplungs_text(KN), T("\nBenutzung auf eigene Gefahr - Gelöschtes ist endgültig weg.", "\nUse at your own risk - deleted data is gone for good."));
    /* NEU 1.1 */
    const char *sh = kern_selbstschutz_hinweis(KN);
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
    kern_groessen_im_hintergrund(h->a->K, h_stopp, h, h_fertig, h);
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

static void loesche_jetzt(ablauf *a)
{
    stand(a, KN->simulation ? T("SIMULATION - es wird nichts gelöscht ...", "SIMULATION - nothing is deleted ...") : T("Lösche ... (große Spiele können etwas dauern)", "Deleting ... (large games can take a while)"), 0);
    /* NEU 1.0.5: "Eintrag i von n" (und GB, wenn die Größen bekannt sind) - nur Anzeige */
    fstand fs = { a, true, 0, 0, NULL };
    FUER_ALLE(f) if (f->Wahl) { if (!f->HatGroesse) fs.groessenDa = false; else fs.gesamt += f->Bytes; }
    KN->Fortschritt = fortschritt; KN->fortschritt_ctx = &fs;
    logz(a, "");
    static _Thread_local ergebnis erg;          /* (über longjmp hinweg gültig) */
    memset(&erg, 0, sizeof erg);
    a->loescht = true;
    VERSUCHE { erg = kern_fuehre_loeschung(KN, sm_frage, a); }
    FANGE {
        if (ausn_art != AUSN_ABBRUCH) { a->loescht = false; KN->Fortschritt = NULL; WEITERWERFEN(); }
        /* kann heute nicht vorkommen (Abbrechen ist beim Löschen gesperrt) - aber dann ehrlich melden */
        memset(&erg, 0, sizeof erg); erg.Fehler = 1;
        logz(a, T("Löschen wurde unterbrochen - ein Teil kann schon gelöscht sein. Bitte das Protokoll prüfen.", "Deleting was interrupted - part may already be deleted. Please check the log."));
    } ENDE_VERSUCHE;
    a->loescht = false;
    KN->Fortschritt = NULL;
    stand(a, "", ABLAUF_BALKEN_GLEICH);
    if (erg.Abgebrochen) {
        if (KN->SM_STOP_GESENDET) logz(a, T("Abgebrochen - nichts gelöscht. ShadowMount hat das Stopp-Signal erhalten - zum Weiterspielen neu laden.", "Cancelled - nothing deleted. ShadowMount received the stop signal - load it again to keep playing."));
        else logz(a, T("Abgebrochen - nichts verändert.", "Cancelled - nothing changed."));
        kern_sm_zuruecksetzen(KN); return;
    }
    stand(a, "", 100);
    /* NEU 28.10.2026 (da_Chris86): PS5 Cleanup ganz zum Schluss selbst entfernen - vollständige Bereinigung ohne Frage,
     * Auswahl auf Nachfrage. Nur wenn alles andere fehlerfrei durchlief (sonst wird PS5 Cleanup evtl. noch gebraucht). */
    const char *selbst_text = "";
    bool sm2 = a->selbst_modus == 1 && a->leeren_m2, susb = a->selbst_modus == 1 && a->leeren_usb;   /* Codex-Prüfung 28 */
    if (a->selbst_modus && kern_selbst_da(KN, sm2, susb)) {
        if (erg.Fehler > 0) {
            selbst_text = T("PS5 Cleanup bleibt, weil Fehler aufgetreten sind - so kannst du es nochmal versuchen.", "PS5 Cleanup stays because errors occurred - so you can try again.");
            logz(a, selbst_text);
        } else {
            bool weg = a->selbst_modus == 1
                || frage_wahl(a, T("Löschung abgeschlossen.\n\nPS5 Cleanup jetzt auch entfernen (Programm, eigene Daten und Kachel)?\n\nBEHALTEN: PS5 Cleanup bleibt für die nächste Bereinigung.\n\nENTFERNEN: PS5 Cleanup löscht sich jetzt selbst.",
                                     "Deletion finished.\n\nRemove PS5 Cleanup too now (program, its data and tile)?\n\nKEEP: PS5 Cleanup stays for the next cleanup.\n\nREMOVE: PS5 Cleanup deletes itself now."),
                              "PS5 Cleanup", T("Behalten", "Keep"), T("Entfernen", "Remove"));
            if (weg) {
                logz(a, T("Entferne PS5 Cleanup selbst ...", "Removing PS5 Cleanup itself ..."));
                a->loescht = true;
                int f = kern_selbst_entfernen(KN, sm2, susb);
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
                    selbst_text = KN->simulation ? T("PS5 Cleanup würde seine Dateien entfernen - die eigene Kachel bliebe bis zum Datenbank-Neuaufbau sichtbar.", "PS5 Cleanup would remove its files - its tile would stay visible until the database is rebuilt.")
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
                              T(" - die Kachel bleibt im Menü (siehe Protokoll).", " - the tile stays in the menu (see log)."));
        if (KN->simulation) kopf = kopf_meldung = VERB(AR, T("SIMULATION fertig - es wurde NICHTS gelöscht. ", "SIMULATION done - NOTHING was deleted. "), T("Beim echten Löschen: ", "Real deleting: "), tw);
        else kopf = kopf_meldung = VERB(AR, T("Fertig - aber ", "Done - but "), tw);
    }
    logz(a, kopf);
    logz(a, frei);
    logz(a, kern_kopplungs_text(KN));
    if (smtext[0]) logz(a, smtext);
    if (selbst_text[0]) kopf_meldung = VERB(AR, kopf_meldung, "\n", selbst_text);
    /* NEU 1.0.6: "Datenbank neu aufbauen" nur noch, wenn Kacheln im Menü bleiben. Ohne Sonys Dienst (PC/Windows) wie bisher immer. */
    KN->DB_HINWEIS = !KN->SONY || KN->GEISTER.n > 0;
    if (KN->DB_HINWEIS) {
        if (KN->SONY) logz(a, VERB(AR, T("Diese Einträge bleiben im PS5-Menü sichtbar, bis die Datenbank neu aufgebaut ist: ", "These entries stay visible in the PS5 menu until the database has been rebuilt: "), sl_join(AR, &KN->GEISTER, ", ")));
        const char **sz; int nz = kern_schluss_text(&sz);
        if (KN->simulation) logz(a, T("(SIMULATION - nach einem echten Löschen käme jetzt:)", "(SIMULATION - after real deleting, this would come next:)"));
        for (int i = 0; i < nz; i++) logz(a, sz[i]);
    }
    if (a->ui.fertig) a->ui.fertig(a->ui.ctx, kopf_meldung, erg.Fehler == 0 && erg.Teilweise == 0, frei, smtext);
    /* neu nachschauen, damit die Liste stimmt. Klappt das nicht, keine alte Liste stehen lassen. */
    if (KN->simulation && a->neue_verbindung) {
        zugriff *z = a->neue_verbindung(a->verbindung_ctx);
        if (z) { kern_verbinden(KN, z); logz(a, T("SIMULATION: Die Liste zeigt wieder den echten Stand - es wurde nichts gelöscht.", "SIMULATION: the list shows the real state again - nothing was deleted.")); }
    }
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
    for (int k = 1; k <= KAT_ANZ; k++) { int n = 0; FUER_ALLE(f) if (f->Kat == k && f->Ort == ORT_INTERN && !f->Gesperrt && !f->Paket && !f->NurEinzeln) n++; t = VERB(AR, t, "   - ", kern_kat_name(k), ": ", zahl(AR, n), "\n"); }
    t = VERB(AR, t, T("(M.2 und USB-Speicher: gleich wird jeweils gefragt. USB-Sticks bleiben unangetastet.)\n", "(M.2 and USB storage: you will be asked for each. USB sticks stay untouched.)\n"));
    t = VERB(AR, t, T("\nDas heißt:\n - Spielstände ohne Cloud-Sicherung (PS Plus) sind endgültig weg.\n - Nie mit dem PSN abgeglichene Trophäen sind weg.\n - Jailbreak-Tools samt Einstellungen sind weg - danach den Jailbreak NEU EINRICHTEN.\n\nBleibt: Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, echte Lizenzen, Sony-Systemanwendungen.",
                      "\nThis means:\n - Saved data without a cloud backup (PS Plus) is gone for good.\n - Trophies never synced with PSN are gone.\n - Jailbreak tools and their settings are gone - set up your jailbreak AGAIN afterwards.\n\nKept: system software, disc drive pairing, user account, real licenses, Sony system apps."));
    /* NEU 1.1 (Anforderungen 27.10.2026): Jailbreak-Einstiege - danach wird gefragt */
    if (kern_anzahl_jailbreak(KN, true, true) > 0)
        t = VERB(AR, t, T("\nMögliche Jailbreak-Einstiege (YouTube, Netflix, Lua-Spiele, PS2-Spielstände, Browser): ", "\nPossible jailbreak entries (YouTube, Netflix, Lua games, PS2 saves, browser): "), zahl(AR, kern_anzahl_jailbreak(KN, true, true)),
                 T(" - gleich wird gefragt, ob sie bleiben.", " - you will be asked next whether they stay."));
    t = VERB(AR, t, kern_speicher_hinweis(KN));
    /* NEU 1.1 */
    const char *sh = kern_selbstschutz_hinweis(KN);
    if (sh[0]) t = VERB(AR, t, "\n\n", sh);
    if (KN->simulation) t = VERB(AR, T("SIMULATION - es wird NICHTS gelöscht.\n\n", "SIMULATION - NOTHING will be deleted.\n\n"), t);
    if (!frage_wahl(a, t, T("Vollständige Bereinigung", "Full cleanup"), T("Abbrechen", "Cancel"), T("Weiter", "Continue"))) { ablauf_hintergrund_starten(a); return; }
    /* da_Chris86 28.10.2026: M.2 und USB-Speicher gehören zur vollständigen Bereinigung - je auf Nachfrage (wie 1.0.5 bei der M.2).
     * Zählt die Spieldateien von ShadowMount-Spielen mit, die auf diesem Speicher liegen. USB-Sticks nie (nur einzeln). */
    int extAnz[3] = { 0, 0, 0 };
    /* Codex-Prüfung 37: Jailbreak-Einstiege auf M.2/USB zählen mit (sonst fehlt die Frage, wenn dort nur Einstiege liegen) */
    FUER_ALLE(f) if (!f->Gesperrt && (!f->NurEinzeln || f->Jailbreak)) { int o = f->Quelle[0] ? kern_pfad_ort(f->Quelle) : f->Ort; if (o == ORT_M2 || o == ORT_USB) extAnz[o]++; }
    bool leeren[3] = { false, false, false };
    for (int o = ORT_M2; o <= ORT_USB; o++) {
        if (extAnz[o] == 0) continue;
        const char *n = kern_ort_name(o);
        leeren[o] = frage_wahl(a, VERB(AR, T("Auf ", "On "), o == ORT_M2 ? T("der M.2-SSD", "the M.2 SSD") : T("dem USB-Speicher", "the USB storage"), T(" liegen ", " there are "), zahl(AR, extAnz[o]), eintraege(extAnz[o]),
            T(" (Spiele, Ordner, Dateien).\n\nBEHALTEN: Die Spiele bleiben. Nur Kacheln von ShadowMount-Spielen darauf gehen weg - sie kommen wieder, sobald ShadowMount neu eingerichtet ist.\n\nAUCH LEEREN: Alle Spiele und Dateien darauf werden endgültig gelöscht.",
              " (games, folders, files).\n\nKEEP: the games stay. Only tiles of ShadowMount games on it go - they come back once ShadowMount is set up again.\n\nALSO WIPE: all games and files on it are deleted permanently.")),
            n, VERB(AR, n, T(" behalten (empfohlen)", ": keep (recommended)")), VERB(AR, n, T(" auch leeren", ": also wipe")));
    }
    bool m2Leeren = leeren[ORT_M2], usbLeeren = leeren[ORT_USB];
    /* NEU 1.1 (Anforderungen 27.10.2026): Jailbreak-Einstiege nur auf ausdrücklichen Wunsch */
    int jbAnz = kern_anzahl_jailbreak(KN, m2Leeren, usbLeeren);
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
      if (paketBleibt) logz(a, T("(Trophäen-Listen bleiben, weil Spiele auf M.2 oder USB bleiben.)", "(Trophy lists stay because games on M.2 or USB stay.)")); }
    VERSUCHE { groesse_ermitteln(a); }
    FANGE { if (ausn_art == AUSN_ABBRUCH) { alles_abwaehlen(a); liste_neu(a); } WEITERWERFEN(); } ENDE_VERSUCHE;
    /* da_Chris86 28.10.2026: deutliche Warnung mit der echten Anzahl, wenn M.2 oder USB mit geleert werden
     * (gezählt nach der Auswahl - mit Jailbreak-Einträgen und ShadowMount-Spieldateien, Codex-Prüfung 28) */
    int gewAnz[3] = { 0, 0, 0 };
    FUER_ALLE(f) if (f->Wahl) {
        int o = f->Ort;
        if (f->Quelle[0]) { o = ORT_INTERN; for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], f->Quelle)) o = kern_pfad_ort(f->Quelle); }
        if (o == ORT_M2 || o == ORT_USB) gewAnz[o]++;
    }
    const char *warn = "";
    for (int o = ORT_M2; o <= ORT_USB; o++) {
        if (extAnz[o] == 0) continue;
        if (leeren[o]) warn = VERB(AR, warn, T("!!! ACHTUNG: ", "!!! WARNING: "), o == ORT_M2 ? T("DIE M.2-SSD", "THE M.2 SSD") : T("DER USB-SPEICHER", "THE USB STORAGE"),
                                   T(" WIRD MIT GELEERT - ", " WILL BE WIPED TOO - "), zahl(AR, gewAnz[o]), eintraege(gewAnz[o]), T(", alle Spiele und Dateien darauf sind danach weg !!!\n\n", ", all games and files on it will be gone !!!\n\n"));
        else warn = VERB(AR, warn, o == ORT_M2 ? T("Die M.2-SSD", "The M.2 SSD") : T("Der USB-Speicher", "The USB storage"), T(" bleibt unangetastet.\n\n", " stays untouched.\n\n"));
    }
    if (jbAnz > 0 && !jbLoeschen) warn = VERB(AR, warn, T("Die Jailbreak-Einstiege bleiben.\n\n", "The jailbreak entries stay.\n\n"));
    const char *antw = frage_text(a, VERB(AR, warn, ablauf_zusammenfassung(a, true), T("\n\nZum Bestätigen LÖSCHEN eintippen (alles andere = abbrechen):", "\n\nType DELETE to confirm (anything else = cancel):")), T("Endgültig löschen?", "Delete permanently?"));
    bool bestaetigt = antw != NULL && (SP_EN ? !strcmp(antw, "DELETE") : (!strcmp(antw, "LOESCHEN") || !strcmp(antw, "LÖSCHEN")));
    if (!bestaetigt) {
        logz(a, T("Abgebrochen - nichts verändert.", "Cancelled - nothing changed."));
        alles_abwaehlen(a); liste_neu(a); ablauf_hintergrund_starten(a);
        return;
    }
    a->auto_neustart = a->neustart_frage ? a->neustart_frage(a->neustart_ctx) : false;   /* NEU 1.0.6 */
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
    if (sm.n > 0) {
        sliste z = {0}; for (int i = 0; i < sm.n; i++) sl_add(AR, &z, VERB(AR, "   ", ((eintrag *)sm.e[i])->Text));
        const char *m2z = "";   /* bis 27.10.2026: Zusatz "Spiele auf der M.2 bleiben" (U20) - ersetzt durch U24, wieder wie 1.0.5 */
        bool mit = frage_wahl(a, VERB(AR, T("Du hast ", "You selected "), zahl(AR, sm.n), T(" ShadowMount-Spiel(e) gewählt:\n", " ShadowMount game(s):\n"), sl_join(AR, &z, "\n"),
            T("\n\nNUR KACHELN LÖSCHEN: Die Spiele bleiben auf der Platte und erscheinen wieder, sobald ShadowMount neu geladen ist.\n\nSPIELE + KACHELN LÖSCHEN: Die Spieldateien werden endgültig gelöscht.",
              "\n\nDELETE TILES ONLY: the games stay on the SSD/M.2 and come back once ShadowMount is loaded again.\n\nDELETE GAMES + TILES: the game files are deleted permanently."), m2z),
            T("ShadowMount-Spiele", "ShadowMount games"), T("Nur Kacheln löschen (empfohlen)", "Delete tiles only (recommended)"), T("Spiele + Kacheln löschen", "Delete games + tiles"));
        kern_setze_sm_spieldateien(KN, mit);
    }
    else kern_sm_zuruecksetzen(KN);
    pliste m2 = kern_ext_gewaehlt(KN);   /* NEU 1.0.6: M.2, USB-Speicher und USB-Stick */
    if (m2.n > 0) {
        sliste z = {0}; for (int i = 0; i < m2.n && i < 15; i++) sl_add(AR, &z, VERB(AR, "   ", ((eintrag *)m2.e[i])->Text));
        const char *l = sl_join(AR, &z, "\n");
        if (m2.n > 15) l = VERB(AR, l, T("\n   ... und ", "\n   ... and "), zahl(AR, m2.n - 15), T(" weitere", " more"));
        if (!frage_wahl(a, VERB(AR, T("ACHTUNG - du hast ", "WARNING - you selected "), zahl(AR, m2.n), m2.n == 1 ? T(" Eintrag auf M.2/USB gewählt:\n", " entry on M.2/USB:\n") : T(" Einträge auf M.2/USB gewählt:\n", " entries on M.2/USB:\n"), l,
                T("\n\nAlles, was darin liegt (Spiele, Ordner, Dateien), wird endgültig gelöscht.\n\nGelöscht wird erst nach der Abschlussfrage.", "\n\nEverything in them (games, folders, files) will be deleted permanently.\n\nNothing is deleted before the final question.")),
                "M.2 / USB", T("Abbrechen", "Cancel"), T("Weiter (diese Einträge löschen)", "Continue (delete these entries)")))
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
    a->auto_neustart = a->neustart_frage ? a->neustart_frage(a->neustart_ctx) : false;   /* NEU 1.0.6 */
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
    if (KN->simulation) logz(a, VERB(AR, "(", sim_kurz(a), ")"));
    ablauf_hintergrund_starten(a);
}

/* ------------------------------------------------------------- Starte */
void ablauf_starte(ablauf *a, void (*fn)(void *), void *arg)
{
    KN->abbruch = false;
    VERSUCHE { fn(arg); }
    FANGE {
        if (ausn_art == AUSN_ABBRUCH) {
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
    liste_neu(a);
}
