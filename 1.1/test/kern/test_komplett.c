/* PS5 Cleanup - Test: Fenster der vollständigen Bereinigung (PS5-Test 03.11.2026, da_Chris86: "es kann doch nicht sein, dass
 * bei der vollständigen Bereinigung die Hälfte fehlt" / "USB wird gefragt, aber dann steht da, der USB wird nie angefasst").
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 *   test_komplett <wurzel>        (ZEIGE=1: alle Fenster ausgeben)
 *
 * Durchsucht EINEN Baum, spielt die vollständige Bereinigung zweimal durch (1: Weiter, alles "auch leeren", 2: Weiter, alles
 * "behalten"; jeweils am Ende LÖSCHEN NICHT eintippen = nichts gelöscht) und prüft gegen die echte Auswahl (kern_waehle_komplett):
 *  - jeder Eintrag, den die vollständige Bereinigung nimmt, steht im ersten Fenster - mit der richtigen Zahl je Speicher
 *  - kern_komplett_dabei (Anzeige) == kern_waehle_komplett (Auswahl) für alle vier Antworten (M.2/erweiterter Speicher behalten/leeren)
 *  - je Speicher mit Einträgen genau eine Frage, mit derselben Aufzählung; ohne Einträge keine
 *  - nie "USB-Speicher" oder ein nacktes "USB"; "USB-Stick" nur, wenn einer steckt; Schluss-Warnung passend zur Antwort
 *  - danach ist nichts gewählt
 * Ausgabe: "FAIL ..." je Abweichung, am Ende "ERGEBNIS OK|FAIL <n>".
 */
#define _GNU_SOURCE
#include "../../kern/ablauf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *titel, *text; bool wahl; } dialog;
static dialog D[64]; static int nd = 0;
static int fehler = 0;
static bool leeren_antwort = true;
static arena *TA;

static void fail(const char *m) { printf("FAIL %s\n", m); fehler++; }
static bool u_wahl(void *c, const char *text, const char *titel, const char *sicher, const char *gef)
{
    (void)c; (void)sicher; (void)gef;
    if (nd < 64) D[nd++] = (dialog){ a_kopie(TA, titel), a_kopie(TA, text), true };
    if (!strcmp(titel, "Endgültig löschen?")) return false;   /* Auswahl: Abschlussfrage nie bestätigen - nichts wird gelöscht */
    if (nd == 1) return true;                     /* erstes Fenster: Weiter */
    return leeren_antwort;                        /* M.2 / erweiterter Speicher */
}
static const char *u_text(void *c, const char *text, const char *titel)
{
    (void)c;
    if (nd < 64) D[nd++] = (dialog){ a_kopie(TA, titel), a_kopie(TA, text), false };
    return NULL;                                  /* nicht bestätigen - nichts wird gelöscht */
}
static void u_log(void *c, const char *t) { (void)c; (void)t; }
static void u_stand(void *c, const char *t, double p) { (void)c; (void)t; (void)p; }
static void u_fertig(void *c, const char *kopf, bool ok, const char *frei, const char *sm) { (void)c; (void)kopf; (void)ok; (void)frei; (void)sm; fail("Fertig-Meldung, obwohl nicht bestätigt"); }
static void u_fehler(void *c, const char *t) { (void)c; printf("FEHLERMELDUNG %s\n", t); }
static bool nie(void *c) { (void)c; return false; }
static void nichts(void *c, eintrag *e) { (void)c; (void)e; }

/* Ort, an dem die vollständige Bereinigung einen gewählten Eintrag zählt (unabhängig von ablauf.c aufgeschrieben):
 * ShadowMount-Spieldatei -> wo sie liegt (intern: steht schon mit ihrer Kachel da = -1), sonst Kachel intern, sonst Ort */
static int ort_von(const eintrag *f)
{
    if (f->SmDatei) return f->Ort == ORT_INTERN ? -1 : f->Ort;
    return f->Quelle[0] ? ORT_INTERN : f->Ort;
}
/* Block = Spiel/App samt allem darunter (Codex-Prüfung 48: auch hier gezählt, nicht nur die Zeilen) */
static const char *bkey(const eintrag *f)
{
    if (f->SmDatei && f->SmKachel) f = f->SmKachel;
    for (int i = 0; f->ToolKachel && i < 8; i++) f = f->ToolKachel;
    if (f->Teil && f->Gruppe) return a_fmt(TA, "G|%s", f->Gruppe);
    return a_fmt(TA, "%p", (const void *)f);
}
static int bloecke_ist;   /* Summe der Blöcke im zuletzt gelesenen Abschnitt */
/* NEU 1.0.8 (F3): die Klammer hängt vom Bereich ab ("(mit Daten und Updates 14 Einträge)", "(14 Einträge)") - die Zahl vor "Einträge)" */
static int klammer_zeilen(const char *z)
{
    const char *k = strrchr(z, '(');
    if (!k) return -1;
    const char *e = strstr(k, " Einträge)"); if (!e) e = strstr(k, " Eintrag)");
    if (!e) return -1;
    const char *d = e; while (d > k && d[-1] >= '0' && d[-1] <= '9') d--;
    return d < e ? atoi(d) : -1;
}
/* Abschnitt im ersten Fenster: Zeilen "   - Name: N" bzw. "... B (mit Daten und Updates M Einträge)" -> Summe der Zeilen */
static int abschnitt(const char *text, const char *kopf, bool *da, bool intern, int *pakete)
{
    *da = false; if (pakete) *pakete = -1; bloecke_ist = 0;
    const char *p = intern ? strchr(text, '\n') : strstr(text, kopf);
    if (!p) return 0;
    *da = true;
    if (!intern) p = strchr(p, '\n');
    int summe = 0;
    while (p && p[1] == ' ' && p[2] == ' ' && p[3] == ' ' && p[4] == '-') {
        const char *e = strchr(p + 1, '\n'); if (!e) e = p + strlen(p);
        char z[600]; size_t l = (size_t)(e - p - 1); if (l >= sizeof z) l = sizeof z - 1; memcpy(z, p + 1, l); z[l] = 0;
        const char *dp = strstr(z, ": ");
        if (strstr(z, "Trophäen-Listen installierter Spiele: ") == z + 5) { if (pakete) *pakete = atoi(dp + 2); }
        else if (dp) {
            int kz = klammer_zeilen(z);
            summe += kz >= 0 ? kz : atoi(dp + 2);
            bloecke_ist += atoi(dp + 2);
        }
        p = *e ? e : NULL;
    }
    return summe;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "Aufruf: test_komplett <wurzel>\n"); return 2; }
    TA = arena_neu();
    datei_optionen o = { .wurzel = argv[1], .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    kern *K = kern_neu(zugriff_datei(&o));
    K->DDD_BEKANNT = false;
    ablauf_ui ui = { u_wahl, u_text, u_log, u_stand, u_fertig, u_fehler, NULL, NULL };
    ablauf A; ablauf_init(&A, K, &ui);
    K->melde = (void (*)(void *, const char *))u_log; K->melde_ctx = NULL;
    A.protokoll_datei = "(Protokoll)";
    VERSUCHE { kern_suche_funde(K); } FANGE { printf("X %s\n", ausn_text); return 1; } ENDE_VERSUCHE;
    kern_groessen_im_hintergrund(K, nie, NULL, nichts, NULL);

    /* 1. Anzeige-Regel == Auswahl, für alle vier Antworten */
    for (int m = 0; m < 4; m++) {
        bool m2 = m & 1, usb = m & 2;
        kern_waehle_komplett(K, m2, usb);
        for (int i = 0; i < K->Funde.n; i++) {
            eintrag *f = K->Funde.e[i];
            if (f->Paket) continue;
            if (f->Wahl != kern_komplett_dabei(K, f, m2, usb)) fail(a_fmt(TA, "Anzeige-Regel != Auswahl (M.2 %d, erw. %d): %s", m2, usb, f->Text));
            if (f->Wahl && (ort_von(f) == ORT_STICK || (ort_von(f) == ORT_M2 && !m2) || (ort_von(f) == ORT_USB && !usb)))
                fail(a_fmt(TA, "gewählt, obwohl der Speicher bleibt (M.2 %d, erw. %d): %s", m2, usb, f->Text));
            /* Codex-Prüfung 48: ShadowMount-Kachel nimmt ihre Spieldatei genau dann mit, wenn deren Speicher geleert wird */
            if (f->Wahl && f->Quelle[0] && !f->SmDatei) {
                bool hat = false; for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], f->Quelle)) hat = true;
                int qo = kern_pfad_ort(f->Quelle);
                bool soll = qo == ORT_INTERN || (qo == ORT_M2 && m2) || (qo == ORT_USB && usb);
                if (hat != soll) fail(a_fmt(TA, "ShadowMount-Spieldatei %s (M.2 %d, erw. %d): %s", hat ? "mitgenommen, obwohl ihr Speicher bleibt" : "fehlt, obwohl ihr Speicher geleert wird", m2, usb, f->Text));
            }
        }
    }
    /* Soll-Zahlen aus der Auswahl "alles leeren" */
    kern_waehle_komplett(K, true, true);
    int soll[4] = { 0, 0, 0, 0 }, pak = 0, pakAlle = 0, sollBl[4] = { 0, 0, 0, 0 }, sollBlAlle[4] = { 0, 0, 0, 0 };
    mtab blk[4][16], blAlle[4];
    for (int w = 0; w < 4; w++) { blAlle[w] = mt_neu(false); for (int q = 0; q < 16; q++) blk[w][q] = mt_neu(false); }
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i];
        if (f->Paket && !f->Gesperrt && !f->NurEinzeln) pakAlle++;
        if (!f->Wahl) continue;
        if (f->Paket) { pak++; continue; }
        int w = ort_von(f); if (w >= 0) { soll[w]++; mt_setze(TA, &blk[w][f->Kat & 15], bkey(f), (void *)1); mt_setze(TA, &blAlle[w], bkey(f), (void *)1); }
    }
    for (int w = 0; w < 4; w++) { for (int q = 0; q < 16; q++) sollBl[w] += blk[w][q].n; sollBlAlle[w] = blAlle[w].n; }
    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
    kern_sm_zuruecksetzen(K);

    for (int lauf = 0; lauf < 2; lauf++) {
        leeren_antwort = lauf == 0; nd = 0;
        ablauf_starte(&A, (void (*)(void *))ablauf_komplett, &A);
        ablauf_hintergrund_anhalten(&A);
        const char *wie = lauf == 0 ? "leeren" : "behalten";
        if (getenv("ZEIGE")) for (int i = 0; i < nd; i++) printf("=== FENSTER %d (%s) [%s]\n%s\n", i + 1, wie, D[i].titel, D[i].text);
        if (nd < 2) { fail(a_fmt(TA, "%s: weniger als 2 Fenster (%d)", wie, nd)); continue; }
        const char *t1 = D[0].text;
        bool da;
        int ist = abschnitt(t1, NULL, &da, true, NULL);
        int pk = -1; abschnitt(t1, NULL, &da, true, &pk);
        if (ist != soll[ORT_INTERN]) fail(a_fmt(TA, "%s: interne SSD im ersten Fenster %d Zeilen, die Auswahl nimmt %d", wie, ist, soll[ORT_INTERN]));
        abschnitt(t1, NULL, &da, true, NULL);
        if (bloecke_ist != sollBl[ORT_INTERN]) fail(a_fmt(TA, "%s: interne SSD im ersten Fenster %d Spiele/Apps, die Auswahl nimmt %d", wie, bloecke_ist, sollBl[ORT_INTERN]));
        /* Trophäen-Listen installierter Spiele: alle stehen da; gehen sie bei "alles leeren" nicht alle mit (Spiel auf dem USB-Stick),
         * muss die Bedingung dabeistehen */
        if ((pakAlle > 0) != (pk >= 0) || (pk >= 0 && pk != pakAlle)) fail(a_fmt(TA, "%s: Trophäen-Listen im ersten Fenster %d, vorhanden %d", wie, pk, pakAlle));
        if (pak != pakAlle && !strstr(t1, "gehen nur mit, wenn auf keinem Speicher ein Spiel bleibt")) fail(a_fmt(TA, "%s: Trophäen-Listen ohne Bedingung, obwohl nicht alle mitgehen", wie));
        const char *kopf[3] = { NULL, "Auf der M.2-SSD - gleich kommt die Frage:", "Auf dem erweiterten Speicher (USB-Festplatte für Spiele) - gleich kommt die Frage:" };
        const char *ftitel[3] = { NULL, "M.2-SSD", "Erweiterter Speicher (USB-Festplatte)" };
        int fragen = 0;
        for (int w = ORT_M2; w <= ORT_USB; w++) {
            int n = abschnitt(t1, kopf[w], &da, false, NULL);
            if (soll[w] > 0 && !da) fail(a_fmt(TA, "%s: %d Einträge %s, im ersten Fenster nicht erwähnt", wie, soll[w], ftitel[w]));
            if (soll[w] == 0 && da) fail(a_fmt(TA, "%s: %s im ersten Fenster, aber nichts dabei", wie, ftitel[w]));
            if (da && n != soll[w]) fail(a_fmt(TA, "%s: %s im ersten Fenster %d Zeilen, die Auswahl nimmt %d", wie, ftitel[w], n, soll[w]));
            if (da && bloecke_ist != sollBl[w]) fail(a_fmt(TA, "%s: %s im ersten Fenster %d Spiele/Apps, die Auswahl nimmt %d", wie, ftitel[w], bloecke_ist, sollBl[w]));
            int gefunden = 0;
            for (int i = 1; i < nd; i++) if (D[i].wahl && !strcmp(D[i].titel, ftitel[w])) {
                gefunden++;
                const char *ab = strstr(t1, kopf[w]); ab = ab ? strchr(ab, '\n') + 1 : "";
                const char *ende = strstr(ab, "\n\n"); size_t l = ende ? (size_t)(ende - ab) + 1 : strlen(ab);
                if (strncmp(strchr(D[i].text, '\n') + 1, ab, l) != 0) fail(a_fmt(TA, "%s: Frage %s zählt anders auf als das erste Fenster", wie, ftitel[w]));
            }
            if (gefunden != (soll[w] > 0 ? 1 : 0)) fail(a_fmt(TA, "%s: %d Fragen zu %s (Soll %d)", wie, gefunden, ftitel[w], soll[w] > 0));
            fragen += gefunden;
            /* Schluss-Fenster */
            const char *s = D[nd - 1].text;
            if (soll[w] > 0) {
                const char *gross = w == ORT_M2 ? "DIE M.2-SSD WIRD MIT GELEERT" : "DER ERWEITERTE SPEICHER (USB-FESTPLATTE FÜR SPIELE) WIRD MIT GELEERT";
                const char *klein = w == ORT_M2 ? "Die M.2-SSD bleibt unangetastet." : "Der erweiterte Speicher (USB-Festplatte für Spiele) bleibt unangetastet.";
                if (!strstr(s, lauf == 0 ? gross : klein)) fail(a_fmt(TA, "%s: Schluss-Fenster sagt nicht \"%s\"", wie, lauf == 0 ? gross : klein));
                /* Codex-Prüfung 48: auch die Zahl in der Warnung (Spiele/Apps und Zeilen) */
                const char *g = lauf == 0 ? strstr(s, gross) : NULL;
                if (g) {
                    g = strstr(g, " - ") + 3; int b = atoi(g), z = b;
                    const char *ko = strchr(g, ',');
                    if (ko) { int kz = klammer_zeilen(a_kopie_n(TA, g, (size_t)(ko - g))); if (kz >= 0) z = kz; }
                    if (z != soll[w] || b != sollBlAlle[w]) fail(a_fmt(TA, "%s: Warnung %s nennt %d Spiele/Apps, %d Zeilen - die Auswahl nimmt %d / %d", wie, ftitel[w], b, z, sollBlAlle[w], soll[w]));
                }
            }
        }
        if (D[nd - 1].wahl) fail(a_fmt(TA, "%s: letztes Fenster ist keine LÖSCHEN-Eingabe", wie));
        /* NEU 1.0.8 (F3, da_Chris86s Bilder vom 04.11.2026): die Trophäen-Listen installierter Spiele stehen im Schluss-Fenster getrennt wie
         * im ersten Fenster (bis 1.0.7: dort 8 + 6, am Ende 14 unter "Trophäen"), und "mit Daten und Updates" nur bei Spielen */
        if (lauf == 0 && pak > 0 && !strstr(D[nd - 1].text, a_fmt(TA, "   - Trophäen-Listen installierter Spiele: %d\n", pak)))
            fail(a_fmt(TA, "%s: Schluss-Fenster nennt die %d Trophäen-Listen installierter Spiele nicht getrennt", wie, pak));
        for (int i = 0; i < nd; i++) {
            const char *tz = strstr(D[i].text, "   - Trophäen: ");
            const char *te = tz ? strchr(tz, '\n') : NULL;
            if (tz && te && strstr(a_kopie_n(TA, tz, (size_t)(te - tz)), "Daten und Updates")) fail(a_fmt(TA, "%s: Fenster %d sagt bei Trophäen \"mit Daten und Updates\"", wie, i + 1));
        }
        if (nd != 2 + fragen) fail(a_fmt(TA, "%s: %d Fenster, erwartet %d", wie, nd, 2 + fragen));
        for (int i = 0; i < nd; i++) {
            if (strstr(D[i].text, "USB-Speicher") || strstr(D[i].titel, "USB-Speicher")) fail(a_fmt(TA, "%s: Fenster %d sagt \"USB-Speicher\"", wie, i + 1));
            if (strstr(D[i].text, "USB – ")) fail(a_fmt(TA, "%s: Fenster %d sagt \"USB – \"", wie, i + 1));
        }
        if (!K->STICK_DA && strstr(t1, "USB-Laufwerk")) fail(a_fmt(TA, "%s: erstes Fenster nennt einen USB-Stick, aber keiner steckt", wie));
        if (K->STICK_DA && !strstr(t1, "USB-Laufwerk mit Dateien")) fail(a_fmt(TA, "%s: USB-Stick steckt, erstes Fenster sagt nicht, dass er bleibt", wie));
        for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Wahl) { fail(a_fmt(TA, "%s: nach Abbruch noch gewählt: %s", wie, ((eintrag *)K->Funde.e[i])->Text)); break; }
    }
    /* 3. Auswahl: alles Wählbare einzeln angehakt (auch USB-Stick), am Ende nicht bestätigen - Fenster ohne "USB-Speicher" */
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i]; const char *g;
        if (!ablauf_eintrag_gesperrt(&A, f, &g)) ablauf_eintrag_setzen(&A, f, true);
    }
    leeren_antwort = true; nd = 0;
    ablauf_starte(&A, (void (*)(void *))ablauf_auswahl, &A);
    ablauf_hintergrund_anhalten(&A);
    if (getenv("ZEIGE")) for (int i = 0; i < nd; i++) printf("=== FENSTER %d (Auswahl) [%s]\n%s\n", i + 1, D[i].titel, D[i].text);
    for (int i = 0; i < nd; i++) if (strstr(D[i].text, "USB-Speicher") || strstr(D[i].text, "USB – ")) fail(a_fmt(TA, "Auswahl: Fenster %d sagt \"USB-Speicher\" oder \"USB – \"", i + 1));
    printf("INFO intern %d, M.2 %d, erweiterter Speicher %d, Trophäen-Listen %d, Stick %s\n", soll[ORT_INTERN], soll[ORT_M2], soll[ORT_USB], pak, K->STICK_DA ? "ja" : "nein");
    printf("ERGEBNIS %s %d\n", fehler ? "FAIL" : "OK", fehler);
    return fehler ? 1 : 0;
}
