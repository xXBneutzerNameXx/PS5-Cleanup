/* PS5 Cleanup - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * kern_auswahl.c - Auswahl, 1:1 aus Ausfuehren.cs (1.0.5):
 * Anzahl, AnzahlKomplett, Gewaehlt, HatPakete, SmZuruecksetzen, WaehleKomplett,
 * SM_MIT/SM_OHNE, SmGewaehlt, SetzeSmSpieldateien, M2Gewaehlt, GroessenZuruecksetzen.
 */
#include "kern.h"

#define A (k->a)
#define FUER_ALLE(f) for (int _i = 0; _i < k->Funde.n; _i++) for (eintrag *f = k->Funde.e[_i]; f; f = NULL)

int kern_anzahl(kern *k, int kat) { int n = 0; FUER_ALLE(f) if (f->Kat == kat) n++; return n; }
int kern_anzahl_komplett(kern *k, int kat) { int n = 0; FUER_ALLE(f) if (f->Kat == kat && !f->Gesperrt && !f->Paket && !f->NurEinzeln && !f->SmDatei) n++; return n; }
/* NEU 1.0.7: die Zeile "Spieldatei" eines ShadowMount-Spiels wird nie selbst gelöscht - die Kachel nimmt die Datei mit (kern_sm_nach_auswahl) */
pliste kern_gewaehlt(kern *k) { pliste l = {0}; FUER_ALLE(f) if (f->Wahl && !f->SmDatei) pl_add(A, &l, f); return l; }
bool kern_hat_pakete(kern *k) { FUER_ALLE(f) if (f->Paket) return true; return false; }

/* ShadowMount-Spiele (Bereich 1) auf "wie gefunden" zurücksetzen */
void kern_sm_zuruecksetzen(kern *k)
{
    FUER_ALLE(f) if (f->Quelle[0]) { f->Pfade = sl_kopie(A, &f->PfadeBasis); f->Text = f->TextBasis; }
}

const char *kern_sm_mit(void) { return T(" - Spiel + Kachel", " - game + tile"); }
const char *kern_sm_ohne(void) { return T(" - nur Kachel, Spiel bleibt", " - tile only, game stays"); }
/* (bis 27.10.2026 benutzt, seit 1.0.6 ohne Bereich 7) */
const char *kern_sm_ohne_m2(void) { return T(" - nur Kachel, Spiel auf der M.2 bleibt", " - tile only, game on the M.2 stays"); }

static sliste quelle_davor(kern *k, eintrag *f)
{
    sliste l = {0}; sl_add(A, &l, f->Quelle); sl_add_alle(A, &l, &f->PfadeBasis);
    /* NEU 1.1 (Anforderungen 27.10.2026): bei "Spiel + Kachel" geht auch der App-Datenspeicher mit ("nur Kachel": er bleibt) */
    htreffer t = f->PfadeBasis.n > 0 ? h_mt(A, f->PfadeBasis.e[0], "^/user/app/([^/]+)$") : (htreffer){0};
    const char *ad = t.ok ? kern_app_daten(k, t.gr[1]) : NULL;
    if (ad && !h_hat(&l, ad)) sl_add(A, &l, ad);
    return l;
}

/* NEU 1.0.6: Liegen die Daten eines Eintrags auf einem Speicher, der mit geleert wird? (ShadowMount: die Kachel liegt intern) */
static bool ort_dabei(int ort, bool m2, bool usb) { return ort == ORT_INTERN || (ort == ORT_M2 && m2) || (ort == ORT_USB && usb); }
static int daten_ort(const eintrag *f) { return f->Quelle[0] ? ORT_INTERN : f->Ort; }
int kern_anzahl_ort(kern *k, int ort) { int n = 0; FUER_ALLE(f) if (f->Ort == ort && !f->Gesperrt && !f->NurEinzeln && !f->SmDatei) n++; return n; }

/* NEU 1.0.7 (PS5-Test 03.11.2026, da_Chris86: "bei der vollständigen Bereinigung fehlt die Hälfte"): eine Regel für Auswahl und Anzeige -
 * das erste Fenster der vollständigen Bereinigung zählt mit kern_komplett_dabei genau das, was kern_waehle_komplett nimmt.
 * Trophäen-Listen noch installierter Spiele (Paket) hängen daran, ob irgendwo ein Spiel bleibt - die zählt kern_komplett_pakete. */
static bool komplett_basis(const eintrag *f, bool m2, bool usb) { return !f->Gesperrt && !f->Paket && !f->NurEinzeln && !f->Erkannt && !kern_db_sicherung(f) && ort_dabei(daten_ort(f), m2, usb); }
bool kern_komplett_dabei(kern *k, const eintrag *f, bool m2, bool usb)
{
    if (!komplett_basis(f, m2, usb)) return false;
    if (kern_bereiche_106() || !kern_nur_mit_spiel(k, f)) return true;
    FUER_ALLE(s) if (ist_teil(s, "Spiel") && s->Gruppe && txt_gleich_ci(s->Gruppe, f->Gruppe) && komplett_basis(s, m2, usb)) return true;
    return false;
}
int kern_komplett_ort(const eintrag *f) { return daten_ort(f); }
int kern_komplett_pakete(kern *k, bool *bedingt)
{
    int n = 0; bool aussen = false;
    FUER_ALLE(f) if (f->Paket && !f->Gesperrt && !f->NurEinzeln) n++;
    FUER_ALLE(f) if (ist_teil(f, "Spiel") && f->Ort != ORT_INTERN) aussen = true;
    if (bedingt) *bedingt = aussen;
    return n;
}

/* KOMPLETT: alles auf der internen SSD (Bereich 1-6); M.2 und USB-Speicher nur auf Wunsch (NEU 1.0.6: je eine Frage);
 * USB-Sticks nie (nur einzeln). */
void kern_waehle_komplett(kern *k, bool m2, bool usb)
{
    /* Anforderungen 27.10.2026: die vollständige Bereinigung umfasst alles Zulässige - M.2 wie in 1.0.5 auf Nachfrage.
     * "nur einzeln" (NurEinzeln) sind die Jailbreak-Einstiege (kommen über kern_jailbreak_dazu) und USB-Sticks. */
    kern_sm_zuruecksetzen(k);
    k->PL_BEHALTEN = (m2 ? 0 : 1 << ORT_M2) | (usb ? 0 : 1 << ORT_USB) | 1 << ORT_STICK;   /* NEU 1.0.10 (Codex-Prüfung 163 (1)) */
    FUER_ALLE(f) f->Wahl = false;
    FUER_ALLE(f) if (komplett_basis(f, m2, usb)) f->Wahl = true;
    /* NEU 1.0.7: Update/Zusatzinhalt nur mit ihrem Spiel - bleibt das Spiel (gesperrt, Game Compressor), bleiben sie auch */
    if (!kern_bereiche_106()) FUER_ALLE(f) if (f->Wahl && kern_nur_mit_spiel(k, f)) {
        bool spielDa = false; FUER_ALLE(s) if (ist_teil(s, "Spiel") && s->Gruppe && txt_gleich_ci(s->Gruppe, f->Gruppe) && s->Wahl) spielDa = true;
        if (!spielDa) f->Wahl = false;
    }
    /* Trophäen-Listen noch installierter Spiele: gehen mit, wenn kein Spiel auf einem behaltenen Speicher bleibt */
    bool spielBleibt = false;
    FUER_ALLE(f) if (ist_teil(f, "Spiel") && !ort_dabei(f->Ort, m2, usb)) spielBleibt = true;
    if (!spielBleibt) FUER_ALLE(f) if (f->Paket && !f->Gesperrt && !f->NurEinzeln) f->Wahl = true;
    /* ShadowMount-Spiele: Kachel immer; die Spieldatei geht mit, wenn ihr Speicher mit geleert wird (USB-Stick nie). */
    FUER_ALLE(f) {
        if (f->Quelle[0] == 0 || f->Gesperrt) continue;   /* 1.0.7: Spieldatei statt Bereich 1 (Jailbreak-Spiele stehen unter 7) */
        if (ort_dabei(kern_pfad_ort(f->Quelle), m2, usb)) { f->Pfade = quelle_davor(k, f); f->Text = VERB(A, f->TextBasis, kern_sm_mit()); }
        else f->Text = VERB(A, f->TextBasis, kern_sm_ohne());
    }
}

/* NEU 1.1 (Anforderungen 27.10.2026): Jailbreak-Einstiege nur auf ausdrücklichen Wunsch */
/* Codex-Prüfung 36: Ort der Daten wie bei der vollständigen Bereinigung (ShadowMount: die Kachel liegt intern) */
int kern_anzahl_jailbreak(kern *k, bool m2, bool usb) { int n = 0; FUER_ALLE(f) if (f->Jailbreak && !f->Gesperrt && ort_dabei(daten_ort(f), m2, usb)) n++; return n; }
void kern_jailbreak_dazu(kern *k, bool m2, bool usb) { FUER_ALLE(f) if (f->Jailbreak && !f->Gesperrt && ort_dabei(daten_ort(f), m2, usb)) f->Wahl = true; }

pliste kern_sm_gewaehlt(kern *k) { pliste l = {0}; FUER_ALLE(f) if (f->Wahl && f->Quelle[0] && !f->SmDatei) pl_add(A, &l, f); return l; }

/* NEU 1.0.7 (da_Chris86 31.10.2026 12:49/12:51): Kachel und Spieldatei sind zwei Zeilen - die Abfrage beim Löschen entfällt.
 * Je gewählter ShadowMount-Kachel: "Spiel + Kachel", wenn ihre Zeile "Spieldatei" gewählt ist, sonst "nur Kachel". */
void kern_sm_nach_auswahl(kern *k)
{
    kern_sm_zuruecksetzen(k);
    pliste g = kern_sm_gewaehlt(k);
    for (int i = 0; i < g.n; i++) {
        eintrag *f = g.e[i];
        bool mit = false;
        FUER_ALLE(d) if (d->SmDatei && d->SmKachel == f && d->Wahl && !d->Gesperrt) mit = true;
        if (mit) { f->Pfade = quelle_davor(k, f); f->Text = VERB(A, f->TextBasis, kern_sm_mit()); }
        else f->Text = VERB(A, f->TextBasis, kern_sm_ohne());
    }
}

void kern_setze_sm_spieldateien(kern *k, bool mit)
{
    kern_sm_zuruecksetzen(k);
    pliste g = kern_sm_gewaehlt(k);
    for (int i = 0; i < g.n; i++) {
        eintrag *f = g.e[i];
        if (mit) { f->Pfade = quelle_davor(k, f); f->Text = VERB(A, f->TextBasis, kern_sm_mit()); }
        else f->Text = VERB(A, f->TextBasis, kern_sm_ohne());
    }
}

/* NEU 1.0.6: gewählte Einträge, deren Daten auf M.2, USB-Speicher oder USB-Stick liegen (statt M2Gewaehlt) */
pliste kern_ext_gewaehlt(kern *k) { pliste l = {0}; FUER_ALLE(f) if (f->Wahl && daten_ort(f) != ORT_INTERN) pl_add(A, &l, f); return l; }

