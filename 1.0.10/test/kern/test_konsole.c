/* PS5 Cleanup - Test Laufwerks-Anzeige: 1:1 die Prüfungen aus TestKern.Laufwerk() (1.0.5),
 * gleiche Ausgabezeilen. (Der letzte Teil von 1.0.5 - die Antwort von ps5info.elf einlesen -
 * entfällt: auf der Konsole wird direkt abgefragt.)
 * NEU 1.0.8 (06.11.2026): alle Werte 0..6 in Worten (kern_konsole.c) - Vergleich gegen test/kern/laufwerk_108.out
 * (die Ausgabe von 1.0.5, nur die Zeilen der Werte 0, 1, 3 bis 6 bei Slim/Pro/unbekanntem Modell auf die neuen Texte gesetzt). */
#include "../../kern/kern.h"
#include <stdio.h>
#include <string.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static const char *stufe_name(kopplungsart s) { switch (s) { case KOPPLUNG_BESTAETIGT: return "Bestaetigt"; case KOPPLUNG_KEINE: return "Keine"; case KOPPLUNG_UNKLAR: return "Unklar"; default: return "NichtAuslesbar"; } }

int main(void)
{
    const char *modelle[] = { "CFI-1016A", "CFI-1216B", "CFI-2016", "CFI-2116B", "CFI-7021 B01Y", "CFI-7016", "" };
    int werte[] = { 0, -1, 0, 1, 2, 3, 4, 5, 6, 7, 99, -5 };   /* werte[0] = null */
    for (int en = 0; en < 2; en++) {
        SP_EN = en; const char *sp = en ? "EN" : "DE";
        for (int mi = 0; mi < 7; mi++)
            for (int wi = 0; wi < 12; wi++) {
                datei_optionen o = { .wurzel = "/gibtsnicht" };
                kern *K = kern_neu(zugriff_datei(&o));
                arena *a = K->a;
                const char *m = modelle[mi];
                K->MODELL_TYP = kern_modell_typ(a, m); K->MODELL_DIGITAL = kern_modell_digital(a, m);
                K->DDD_BEKANNT = wi != 0; K->DDD_STATUS = werte[wi];
                const char *t = kern_laufwerk_text(K);
                const char *was = VERB(a, sp, " ", m[0] ? m : "Modell unbekannt", " Status ", wi ? a_fmt(a, "%d", werte[wi]) : "nicht abgefragt", ": ", t);
                const char *soll;
                int w = werte[wi]; bool hat = wi != 0;
                if (!strcmp(K->MODELL_TYP, "fat")) soll = K->MODELL_DIGITAL ? (en ? "No pairing: Digital Edition" : "Keine Kopplung: Digital Edition") : (en ? "No pairing needed: built-in" : "Keine Kopplung nötig: fest eingebautes");
                /* 1.0.6 (da_Chris86 28.10.2026): kurze Texte ohne Statuswert - gleich wie TestKernRef.cs Laufwerk() */
                else if (!hat) soll = en ? "Not queried: could not be read." : "Nicht abgefragt: Konnte nicht gelesen werden.";
                else if (w == 0) soll = en ? "Paired: this console" : "Gekoppelt: Diese Konsole";
                else if (w == 1) soll = en ? "Paired online: " : "Online gekoppelt: ";
                else if (w == 2) soll = en ? "No pairing: no disc drive" : "Keine Kopplung: Kein Laufwerk";
                else if (w == 3) soll = en ? "Pairing not finished: " : "Koppeln nicht fertig: ";
                else if (w == 4) soll = en ? "Unpairing not finished: " : "Entkoppeln nicht fertig: ";
                else if (w == 5 || w == 6) soll = en ? "Factory paired: " : "Ab Werk gekoppelt: ";
                else soll = en ? "Not queried: could not be read." : "Nicht abgefragt: Konnte nicht gelesen werden.";
                pruefe(txt_beginnt(t, soll), was);
                kopplungsart st = kern_kopplungs_stufe(K);
                kopplungsart stSoll = (!strcmp(K->MODELL_TYP, "fat") || (hat && w == 2)) ? KOPPLUNG_KEINE
                                    : (hat && (w == 0 || w == 1 || w == 5 || w == 6)) ? KOPPLUNG_BESTAETIGT : (hat && (w == 3 || w == 4)) ? KOPPLUNG_UNKLAR : KOPPLUNG_NICHT_AUSLESBAR;
                pruefe(st == stSoll, VERB(a, "  Farbe/Stufe passt zum Text: ", stufe_name(st)));
                pruefe(!r_m(t, "ANDEREN|anderen Konsole|ANOTHER|another console"), "  keine unbelegte Aussage 'andere Konsole'");
                if (en) pruefe(!r_m(t, "[äöüÄÖÜß]|Kopplung|Laufwerk"), "  keine deutschen Wörter");
                pruefe(txt_beginnt(kern_kopplungs_text(K), t), "  KopplungsText baut darauf auf");
                kern_frei(K);
            }
    }
    printf("FEHLERZAHL %d\n", fehler);
    return 0;
}
