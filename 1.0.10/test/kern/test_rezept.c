/* PS5 Cleanup 1.0.10 - Test "Rezept lesen" (da_Chris86 11.11.2026: universelle Erkennung - Homebrew-Daten liegen nur in festen Zonen; welcher
 * Ordner zu welchem Programm gehört, steht im Programm selbst). Unbekannte Programme auf allen Wegen an die Konsole (rezept_baum.py).
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later. Nur nachgebildete Dateisysteme - nie eine PS5. */
#include "../../kern/kern.h"
#include "../../kern/ablauf.h"
#include <stdio.h>
#include <string.h>
static int fehler = 0;
static kern *K;
static arena *A0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void still(void *c, const char *t) { (void)c; if (strstr(t, "ohne Kachel") || strstr(t, " -> /data/")) printf("LOG %s\n", t); }
static eintrag *ordner(const char *o)
{
    const char *p = VERB(A0, "/user/data/", o);
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (!strcmp(f->PfadeBasis.e[j], p)) return f; }
    return NULL;
}
static const char *z(const eintrag *f) { return f ? kern_zeile(A0, f) : "nicht gefunden"; }
static void gehoert(const char *o, const char *prog)
{
    eintrag *f = ordner(o);
    pruefe(f && f->Kat == 6 && strstr(z(f), VERB(A0, "gehört zu ", prog)) && !f->Gesperrt && kern_ampel(f) == AMPEL_GELB,
           a_fmt(A0, "%s gehört zu %s, unter Apps, wählbar wie bisher (Bereich %d: %s)", o, prog, f ? f->Kat : -1, z(f)));
}
static void rest_wie(const char *o, const char *warum, const char *folge)
{
    eintrag *f = ordner(o);
    pruefe(f && f->Kat == 5 && !f->ToolKachel && strstr(kern_folge(f), folge),
           a_fmt(A0, "%s bleibt ein Rest unter Sonstiges - %s (Bereich %d: %s / %s)", o, warum, f ? f->Kat : -1, z(f), f ? kern_folge(f) : "-"));
}
static void rest(const char *o, const char *warum) { rest_wie(o, warum, "keinem Programm"); }
/* Codex-Prüfung 156: nennt ein Programm den Ordner (ohne klaren Gewinner), heißt es nicht "von keinem Programm genannt" */
static void rest_genannt(const char *o, const char *warum) { rest_wie(o, warum, "Inhalt unbekannt"); }
int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    A0 = arena_neu();
    datei_optionen o = { .wurzel = VERB(A0, argv[1], "/w"), .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    K = kern_neu(zugriff_datei(&o)); K->melde = still;
    kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K);
    kern_suche_funde(K);
    pruefe(kern_rezept_aktiv(), "Programme wurden gelesen");
    eintrag *od = ordner("ordner-daten");
    pruefe(od && od->ToolKachel && od->ToolKachel->KachelId && !strcmp(od->ToolKachel->KachelId, "PPSA77002"),
           a_fmt(A0, "Ordner-App: Daten-Ordner gehört fest zu ihrer Kachel (geht nur mit ihr) (%s)", z(od)));
    gehoert("meinwz", "meinwerkzeug");
    gehoert("stickwz", "stickwerkzeug");
    gehoert("plugdat", "plugwerk");
    {   /* NEU 1.0.10 (da_Chris86 14.11.2026 01:21): das Programm liegt im Kennungs-Ordner einer Kachel - der Daten-Ordner steht zur Anzeige in
         * ihrem Block (Erkannt, Begleit), mit eigenem Haken; KEINE feste Zuordnung (kein ToolKachel) */
        eintrag *pd = ordner("plugindaten");
        pruefe(pd && pd->Erkannt && !pd->ToolKachel && pd->Begleit && pd->Begleit->KachelId && !strcmp(pd->Begleit->KachelId, "ETAP00001")
               && pd->Kat == pd->Begleit->Kat && strstr(z(pd), "erkannt, nicht bewiesen"),
               a_fmt(A0, "plugindaten steht im Block von Pluginapp - nur Anzeige, eigener Haken, nicht fest zugeordnet (Bereich %d: %s)", pd ? pd->Kat : -1, z(pd)));
        int nach = -1, kopf = -1;   /* in der Liste direkt hinter dem Block der Kachel */
        for (int i = 0; i < K->Funde.n; i++) { if (K->Funde.e[i] == pd) nach = i; if (pd && K->Funde.e[i] == pd->Begleit) kopf = i; }
        pruefe(pd && kopf >= 0 && nach > kopf, a_fmt(A0, "plugindaten steht in der Liste hinter seiner Kachel (Kachel %d, Ordner %d)", kopf, nach));
        eintrag *od2 = ordner("ordner-daten"), *mw = ordner("meinwz");
        pruefe(od2 && !od2->Erkannt && mw && !mw->Erkannt, "fest zugeordnete Ordner und Ordner von Programmen ohne Kachel sind nie Erkannt");
        /* NEU 1.0.10 (Prüfung 14.11.2026, Punkt 1): ein nur erkannter Ordner wird nie automatisch gewählt - weder bei der vollständigen
         * Bereinigung (alle vier Antworten zu M.2/USB) noch bei "ganzen Bereich wählen"; einzeln anhaken geht weiter */
        {
            bool nie = true;
            for (int w = 0; w < 4 && pd; w++) { kern_waehle_komplett(K, (w & 1) != 0, (w & 2) != 0); if (pd->Wahl) nie = false; }
            pruefe(pd && nie, "vollständige Bereinigung (M.2/USB ja/nein): plugindaten (erkannt, nicht bewiesen) wird nicht gewählt");
            for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
            ablauf ab; memset(&ab, 0, sizeof ab); ab.K = K;
            if (pd) ablauf_bereich_setzen(&ab, pd->Kat, true);
            pruefe(pd && !pd->Wahl, "ganzen Bereich wählen: plugindaten wird nicht gewählt");
            for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
            if (pd) { pd->Wahl = true; kern_abhaengige_mitnehmen(K); }
            pruefe(pd && pd->Wahl, "einzeln anhaken: plugindaten bleibt gewählt");
            for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        }
    }
    rest_genannt("fremdordner", "ein Programm nennt ihn nur nebenbei");
    rest_genannt("gleich-a", "zwei Ordner gleich oft genannt - nicht raten");
    rest_genannt("gleich-b", "zwei Ordner gleich oft genannt - nicht raten");
    rest("geheimdaten", "verschlüsselte Datei ist kein lesbares Programm");
    rest("altrest", "kein Programm nennt ihn");
    rest_genannt("streit", "zwei Programme wählen denselben Ordner - nicht raten (Codex-Prüfung 155)");
    rest_genannt("einmal", "nur einmal genannt - zu wenig für eine Zuordnung (Codex-Prüfung 155)");
    rest("pakdaten", "PKG-App: Programm verschlüsselt (Grenze)");
    rest("webwz", "Programm liegt nicht auf der Konsole (Grenze)");
    eintrag *hb = ordner("homebrew");
    pruefe(!hb || !strstr(z(hb), "gehört zu sammler"), "Sammelordner homebrew wird nie einem Programm zugeordnet");
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
