/* PS5 Cleanup 1.0.6 - gemeinsamer C-Kern
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later (siehe LICENSE)
 *
 * kern_konsole.c - Modell und Laufwerks-Kopplung, 1:1 aus Kern.cs (1.0.5):
 * ModellTyp, ModellDigital, ModellReihe, LaufwerkText, KopplungsStufe,
 * KopplungsText, SCHLUSS_TEXT. Nur Anzeige - die Kopplung wird nie angefasst.
 */
#include "kern.h"
#include <stdlib.h>

const char *kern_modell_typ(arena *a, const char *m)
{
    const char *x = a_gross(a, r_r(a, m ? m : "", "\\s", ""));
    if (r_m(x, "^CFI-1[012]")) return "fat";
    if (r_m(x, "^CFI-2[01]")) return "slim";
    if (r_m(x, "^CFI-7[01]")) return "pro";   /* 1.0.6: auch die zweite Pro-Baureihe CFI-71xx (Sep. 2025, PS5 Dev Wiki) */
    return "";
}
bool kern_modell_digital(arena *a, const char *m) { return r_m(a_gross(a, r_r(a, m ? m : "", "\\s", "")), "^CFI-\\d{4}B"); }
const char *kern_modell_reihe(const char *typ, bool digital)
{
    if (!strcmp(typ, "fat")) return digital ? T("PS5 Digital Edition (erstes Modell)", "PS5 Digital Edition (original model)") : T("PS5 (erstes Modell)", "PS5 (original model)");
    if (!strcmp(typ, "slim")) return digital ? "PS5 Slim Digital Edition" : "PS5 Slim";
    if (!strcmp(typ, "pro")) return "PS5 Pro";
    return T("unbekannt", "unknown");
}

/* NEU 1.0.6 (da_Chris86 28.10.2026): Modellzeile "CFI-7021 · Europa · seit Nov. 2024" statt "CFI-7021 B01Y".
 * Region aus den letzten beiden Ziffern, Datum aus der Baureihe - Quelle: PS5 Dev Wiki (SKU Models).
 * Unbekannte Nummern: nur die Nummer. Das Anhängsel nach dem Leerzeichen (z. B. "B01Y") sagt dem Nutzer nichts. */
const char *kern_modell_zeile(arena *a, const char *m)
{
    const char *x = a_gross(a, m ? m : "");
    htreffer t = h_mt(a, x, "^\\s*(CFI-(\\d)(\\d)(\\d\\d)[A-Z]*)");
    if (!t.ok) return m && m[0] ? m : "";
    const char *nr = t.gr[1];
    int reihe = (t.gr[2][0] - '0') * 10 + (t.gr[3][0] - '0'), reg = atoi(t.gr[4]);
    const char *region = NULL, *seit = NULL;
    if (reihe == 70 || reihe == 71) {   /* PS5 Pro: eigene Regionsnummern; 71 = zweite Baureihe */
        switch (reg) { case 0: region = "Japan"; break; case 2: region = T("Australien", "Australia"); break; case 14: region = T("Süd-/Mittelamerika", "South/Central America"); break;
                       case 19: region = T("USA/Kanada", "US/Canada"); break; case 20: region = T("Mexiko", "Mexico"); break; case 21: region = T("Europa", "Europe"); break;
                       case 22: region = T("Südostasien/Korea", "Southeast Asia/Korea"); break; }
        seit = reihe == 70 ? T("Nov. 2024", "Nov 2024") : T("Sep. 2025", "Sep 2025");
    } else {
        switch (reg) { case 0: region = "Japan"; break; case 2: region = T("Australien", "Australia"); break; case 8: region = T("Osteuropa/Indien", "Eastern Europe/India"); break;
                       case 9: region = "China"; break; case 14: region = T("Süd-/Mittelamerika", "South/Central America"); break; case 15: region = T("USA/Kanada", "US/Canada"); break;
                       case 16: region = T("Europa", "Europe"); break; case 18: region = T("Südostasien/Korea", "Southeast Asia/Korea"); break; }
        if (reihe == 10) seit = T("Nov. 2020", "Nov 2020");
        else if (reihe == 11) seit = "2021";
        else if (reihe == 12) seit = "2022";
        else if (reihe == 20) seit = T("Nov. 2023", "Nov 2023");
    }
    const char *z = nr;
    if (region) z = VERB(a, z, " · ", region);
    if (seit) z = VERB(a, z, T(" · seit ", " · since "), seit);
    /* Sonderedition 30 Jahre PlayStation (Slim und Pro): Endung B30 statt B01 (PS5 Dev Wiki, da_Chris86 28.10.2026) */
    if (h_m(x, "^\\s*CFI-\\d{4}[A-Z]*\\s*B30")) z = VERB(a, z, " · 30th Anniversary Edition");
    return z;
}

/* Klartext zum ausgelesenen Laufwerks-Status (nur lesend ermittelt)
 * NEU 1.0.6 (da_Chris86 28.10.2026): ohne Statuswerte - Titel vor dem Doppelpunkt, danach ein kurzer Satz */
const char *kern_laufwerk_text(kern *k)
{
    const char *typ = k->MODELL_TYP ? k->MODELL_TYP : "";
    if (!strcmp(typ, "fat")) {
        if (k->MODELL_DIGITAL) return T("Keine Kopplung: Digital Edition ohne Laufwerk (erste PS5).", "No pairing: Digital Edition without disc drive (original PS5).");
        return T("Keine Kopplung nötig: fest eingebautes Laufwerk (erste PS5).", "No pairing needed: built-in disc drive (original PS5).");
    }
    if (!k->DDD_BEKANNT) return T("Nicht abgefragt: Konnte nicht gelesen werden.", "Not queried: could not be read.");
    int s = k->DDD_STATUS;
    switch (s) {
    case 1: return T("Kopplung bestätigt: Laufwerk ist mit dieser Konsole gekoppelt.", "Pairing confirmed: the disc drive is paired with this console.");
    case 2: return T("Keine Kopplung: Kein Laufwerk gekoppelt.", "No pairing: no disc drive is paired.");
    case 0: case 3: case 4: case 5: case 6:
        return T("Nicht sicher bestimmbar: Der Zustand ließ sich nicht eindeutig lesen.", "Not reliably determinable: the state could not be read unambiguously.");
    }
    return T("Nicht abgefragt: Konnte nicht gelesen werden.", "Not queried: could not be read.");
}
kopplungsart kern_kopplungs_stufe(kern *k)
{
    if (k->MODELL_TYP && !strcmp(k->MODELL_TYP, "fat")) return KOPPLUNG_KEINE;
    if (!k->DDD_BEKANNT) return KOPPLUNG_NICHT_AUSLESBAR;
    switch (k->DDD_STATUS) {
    case 1: return KOPPLUNG_BESTAETIGT;
    case 2: return KOPPLUNG_KEINE;
    case 0: case 3: case 4: case 5: case 6: return KOPPLUNG_UNKLAR;
    }
    return KOPPLUNG_NICHT_AUSLESBAR;
}
const char *kern_kopplungs_text(kern *k) { return VERB(k->a, kern_laufwerk_text(k), T(" Das Programm fasst die Kopplung nie an.", " The program never touches the pairing.")); }

int kern_schluss_text(const char ***zeilen)
{
    static const char *z[5];
    z[0] = T("Letzter Schritt an der PS5, damit sie alles sauber neu einliest:", "Last step on the PS5 so it re-reads everything cleanly:");
    z[1] = T("1. PS5 ausschalten.", "1. Turn off the PS5.");
    z[2] = T("2. Einschaltknopf gedrückt halten, bis es zum zweiten Mal piept.", "2. Hold the power button until it beeps a second time.");
    z[3] = T("3. Controller per USB anstecken und 'Datenbank neu erstellen' wählen.", "3. Connect the controller via USB and choose 'Rebuild Database'.");
    z[4] = T("Wichtig: NICHT 'PS5 zurücksetzen' oder 'Systemsoftware neu installieren' wählen!", "Important: do NOT choose 'Reset PS5' or 'Reinstall System Software'!");
    *zeilen = z;
    return 5;
}
