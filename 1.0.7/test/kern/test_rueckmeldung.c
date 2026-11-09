/* PS5 Cleanup 1.0.7 - Test der freiwilligen QR-Rückmeldung (03.11.2026): aus Protokollzeilen nur Schritt + Fehlercode,
 * nie Text, Namen oder Pfade; Jailbreak-Einstiege nur als Kürzel.
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }

static void zeile(kern *k, const char *t, const char *schritt, const char *code, const char *was)
{
    k->RM_SCHRITT[0] = 0; k->RM_CODE[0] = 0;
    kern_rm_zeile(k, t);
    bool ok = !strcmp(k->RM_SCHRITT, schritt) && !strcmp(k->RM_CODE, code);
    if (!ok) printf("     bekommen: '%s' / '%s'\n", k->RM_SCHRITT, k->RM_CODE);
    pruefe(ok, was);
}
static eintrag *jb_eintrag(arena *a, const char *pfad)
{
    eintrag *f = calloc(1, sizeof *f); f->Jailbreak = true; sl_add(a, &f->PfadeBasis, pfad); return f;
}

int main(void)
{
    kern *k = calloc(1, sizeof *k);
    printf("== Fehlercode und Schritt aus Protokollzeilen\n");
    printf("   (gelesen werden nur Antworttexte der Zugriffsschicht bzw. Sonys - sie beginnen mit dem Code, z. B. \"550 ...\")\n");
    zeile(k, "550 Sony hat abgelehnt (0x80990001)", "sony", "0x80990001", "Sony lehnt ab -> sony / 0x80990001");
    zeile(k, "550 Sony hat den 1. Auftrag angenommen, den 2. abgelehnt (0x8099000F) - die Kachel ist noch eingetragen", "sony", "0x8099000f", "Großbuchstaben im Code -> klein");
    zeile(k, "550 Permission denied", "datei", "EACCES", "Systemmeldung -> errno-Name");
    zeile(k, "550 Eintrag ließ sich nicht aus der Spielstand-Liste austragen (0x80b3000f)", "ss", "0x80b3000f", "Spielstand-Liste -> ss");
    zeile(k, "550 Mediengalerie: Sony hat abgelehnt (0x80020003)", "med", "0x80020003", "Mediengalerie vor Sony erkannt -> med");
    zeile(k, "550 Einbindung des Spielordners ließ sich nicht lösen: Device busy", "sm", "EBUSY", "ShadowMount-Einbindung, BSD-Text -> sm / EBUSY");
    zeile(k, "451 Device or resource busy", "datei", "EBUSY", "glibc-Text \"resource busy\", Antwort 451 -> EBUSY");
    zeile(k, "550 kaputte Zusatzinhalt-Angaben ließen sich nicht reparieren: Read-only file system", "appinfo", "EROFS", "Zusatzinhalt-Reparatur -> appinfo / EROFS");
    zeile(k, "  gelöscht   Grand Theft Auto V (PPSA04264)", "", "", "Protokollzeile -> nichts gemerkt");
    zeile(k, "Einbindung ließ sich nicht lösen: Device busy", "", "", "Text ohne Antwort-Code am Anfang -> nichts gemerkt");
    zeile(k, "550 Kachel über Sony entfernt (0x00000000)", "", "", "0x00000000 (beginnt nicht mit 0x8) -> nichts gemerkt");
    zeile(k, "550 Wert 0x812345678 und 0x8123", "", "", "nicht genau 8 Hexziffern -> nichts gemerkt");
    zeile(k, NULL, "", "", "NULL -> nichts");
    /* Codex-Prüfung 69 (1) und Gegenprüfung: Namen und Pfade aus Protokollzeilen dürfen nie als Code gelten */
    zeile(k, "  FEHLER  /user/data/0x81234567.pkg: 550 Permission denied", "", "", "Protokollzeile mit 0x8-Zahl im Pfad -> nichts gemerkt");
    zeile(k, "  FEHLER  Fahrenheit 451 (CUSA00001): 0x81234567 busy", "", "", "Name mit \"451 \" und 0x8-Zahl -> nichts gemerkt");
    zeile(k, "  unverändert   Spiel 0x81234567 (PPSA01234)   (Sony 0x80990001 - die App bleibt vollständig erhalten, es wurde nichts gelöscht)", "", "",
          "Protokollzeile von kachel_bleibt -> nichts (der Code kommt aus Sonys Antwort selbst)");
    {   /* je Eintrag zählt der erste Code - spätere Antworten überschreiben ihn nicht */
        k->RM_SCHRITT[0] = 0; k->RM_CODE[0] = 0;
        kern_rm_zeile(k, "550 Sony hat abgelehnt (0x80990001)");
        kern_rm_zeile(k, "550 Permission denied");
        kern_rm_zeile(k, "550 0x81111111");
        pruefe(!strcmp(k->RM_SCHRITT, "sony") && !strcmp(k->RM_CODE, "0x80990001"), "erster Code bleibt (spätere Antworten überschreiben nicht)");
    }
    {   /* der gemerkte Code enthält nie mehr als das Kürzel */
        k->RM_SCHRITT[0] = 0; k->RM_CODE[0] = 0;
        kern_rm_zeile(k, "550 Selbstentfernung: Startordner: Operation not permitted");
        pruefe(!strcmp(k->RM_SCHRITT, "selbst") && !strcmp(k->RM_CODE, "EPERM") && !strchr(k->RM_CODE, '/'), "Selbstentfernung -> selbst / EPERM, ohne Pfad");
    }
    {   /* Fehler außerhalb eines Eintrags */
        k->RM_FEHLER_N = 0; k->RM_SCHRITT[0] = 0; k->RM_CODE[0] = 0;
        kern_rm_zeile(k, "550 Sony hat abgelehnt (0x80990002)");
        kern_rm_fehler_dazu(k, "selbst", "app");
        pruefe(k->RM_FEHLER_N == 1 && !strcmp(k->RM_FEHLER[0], "selbst.app.i.0x80990002"), "Selbstentfernung gescheitert -> \"selbst.app.i.0x80990002\"");
        k->RM_SCHRITT[0] = 0; k->RM_CODE[0] = 0; kern_rm_fehler_dazu(k, "datei", "rest");
        pruefe(k->RM_FEHLER_N == 2 && !strcmp(k->RM_FEHLER[1], "datei.rest.i.-"), "ohne Code -> \"-\"");
        for (int i = 0; i < 10; i++) kern_rm_fehler_dazu(k, "datei", "rest");
        pruefe(k->RM_FEHLER_N == 5, "höchstens 5 Fehler");
    }
    printf("== Jailbreak-Einstiege als Kürzel\n");
    arena *a = arena_neu();
    pruefe(!strcmp(kern_jailbreak_kuerzel(k, a), ""), "keine Funde -> leer");
    pl_add(a, &k->Funde, jb_eintrag(a, "/user/download/PPSA01650"));
    pl_add(a, &k->Funde, jb_eintrag(a, "/user/home/1a2b3c4d/webkit/shell/local"));
    pl_add(a, &k->Funde, jb_eintrag(a, "/user/home/1a2b3c4d/savedata/CUSA02199/SAVEDATA00"));
    eintrag *kein = calloc(1, sizeof *kein); sl_add(a, &kein->PfadeBasis, "/user/download/PPSA01614"); pl_add(a, &k->Funde, kein);   /* kein Jailbreak-Eintrag */
    const char *jb = kern_jailbreak_kuerzel(k, a);
    printf("     %s\n", jb);
    pruefe(!strcmp(jb, "y2jb,ps2,browser"), "YouTube + PS2-Spielstand + Browserspeicher -> \"y2jb,ps2,browser\" (Netflix ohne Jailbreak-Markierung nicht)");
    k->BD_DISC = true;
    pruefe(!strcmp(kern_jailbreak_kuerzel(k, a), "y2jb,ps2,bd,browser"), "Blu-ray-Disc im Laufwerk -> bd");
    pruefe(strstr(kern_jailbreak_kuerzel(k, a), "1a2b3c4d") == NULL && strstr(kern_jailbreak_kuerzel(k, a), "PPSA") == NULL, "keine Benutzer-Kennung und keine Spielkennung im Ergebnis");
    printf("== NEU 1.0.9: Versionsnummern für den Update-Hinweis\n");
    {
        struct { const char *neu, *alt; bool soll; } V[] = {
            { "1.0.10", "1.0.9", true }, { "v1.0.10", "1.0.9", true }, { "V1.1", "1.0.9", true }, { "1.0.9.1", "1.0.9", true }, { "2", "1.9.9", true },
            { "v1.0.9", "1.0.9", false }, { "1.0.9.0", "1.0.9", false }, { "1.0.8", "1.0.9", false }, { "1.0.9", "1.0.10", false },
            { "1.0.9-beta", "1.0.8", false }, { "1..2", "1.0", false }, { "", "1.0.9", false }, { "v", "1.0.9", false }, { "1.0.", "1.0", false },
            { "123456.0", "1.0", false }, { "1.2.3.4.5", "1.0", false }, { " 1.1", "1.0", false }, { "1.1 ", "1.0", false }, { "vv1.1", "1.0", false },
            { "1.1", "kaputt", false }, { NULL, "1.0", false } };
        for (size_t i = 0; i < sizeof V / sizeof *V; i++) {
            char t[120]; snprintf(t, sizeof t, "\"%s\" neuer als \"%s\": %s", V[i].neu ? V[i].neu : "(NULL)", V[i].alt, V[i].soll ? "ja" : "nein");
            pruefe(kern_version_neuer(V[i].neu, V[i].alt) == V[i].soll, t);
        }
        pruefe(kern_version_gueltig("v1.0.8") && kern_version_gueltig("12.34.5.67890") && !kern_version_gueltig("1.0.8 Fix") && !kern_version_gueltig("x1.0"),
               "gültig: v1.0.8 und 12.34.5.67890, ungültig: \"1.0.8 Fix\" und \"x1.0\"");
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
