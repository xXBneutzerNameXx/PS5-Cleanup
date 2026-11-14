/* PS5 Cleanup - Test der freiwilligen QR-Rückmeldung (03.11.2026): aus Protokollzeilen nur Schritt + Fehlercode,
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
    /* NEU 1.0.10: eine Fehlerantwort ohne Code bekommt wenigstens die Antwortnummer bzw. ein Grund-Kürzel (bis 1.0.9: nichts) */
    zeile(k, "550 Kachel über Sony entfernt (0x00000000)", "sony", "g-550", "0x00000000 (beginnt nicht mit 0x8) -> kein Code, nur g-550");
    zeile(k, "550 Wert 0x812345678 und 0x8123", "datei", "g-550", "nicht genau 8 Hexziffern -> kein Code, nur g-550");
    printf("== NEU 1.0.10: Grund-Kürzel statt \"ohne Code\" (Meldung 10.11.2026 10:27)\n");
    zeile(k, "550 Sony führt die Kennung nicht im Menü, sie steht aber in app.db - nicht sicher, nichts gelöscht", "sony", "g-appdb", "nicht im Menü, aber in app.db -> sony / g-appdb");
    zeile(k, "550 nicht im PS5-Menü eingetragen, app.db ließ sich nicht prüfen (kaputt) - nichts gelöscht", "sony", "g-appdbx", "app.db nicht prüfbar -> sony / g-appdbx");
    zeile(k, "550 Sony hat angenommen, die Kachel ist nach 3 Aufträgen aber noch eingetragen", "sony", "g-bleibt", "Kachel bleibt eingetragen -> sony / g-bleibt");
    zeile(k, "550 Sonys Dienst antwortet nicht (Initialisierung, 10 s)", "sony", "g-still", "Dienst antwortet nicht -> sony / g-still");
    zeile(k, "451 Einbindung von ps5upload ließ sich nicht lösen - unverändert", "sm", "g-mount", "Einbindung -> sm / g-mount");
    zeile(k, "550 Sony hat abgelehnt (0x80AF5025)", "sony", "0x80af5025", "echter Code geht vor dem Grund-Kürzel");
    zeile(k, "550 /user/app/CUSA01234 steht aber in app.db", "sony", "g-appdb", "Pfad in der Antwort -> nur das Kürzel, kein Pfad");
    zeile(k, NULL, "", "", "NULL -> nichts");
    /* Codex-Prüfung 177 (P3): Namen im Zusatz "[Schritt: ...]" gelten nie als Code */
    zeile(k, "550 Permission denied [Schritt: Prüfen: öffnen 0x81234567: Permission denied]", "datei", "EACCES", "Name im Schritt-Zusatz -> der Grund davor (EACCES), nicht 0x81234567");
    zeile(k, "550 Permission denied [Schritt: Prüfen: öffnen add-on: Permission denied]", "datei", "EACCES", "Name \"add-on\" im Schritt-Zusatz -> Schritt datei, nicht appinfo (Codex-Prüfung 178)");
    zeile(k, "550 Resource temporarily unavailable [Schritt: zuerst Prüfen: Ordner lesen in Tiefe 0: Permission denied; dann als Besitzer: Ziel anders als auf dem Weg gesehen: Gerät 1/2, Knoten 3/4 (3 Teile)]", "datei", "EAGAIN",
          "Schritt-Zusatz mit Zahlen -> EAGAIN aus der Antwort davor");
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
        pruefe(k->RM_FEHLER_N == 2 && k->RM_ANZ[1] == 11 && k->RM_ANZ[0] == 1, "NEU 1.0.10: gleiche Fehler zusammengefasst (11-mal datei.rest.i.-)");
        for (int i = 0; i < 40; i++) { snprintf(k->RM_CODE, sizeof k->RM_CODE, "0x8000%04d", i); kern_rm_fehler_dazu(k, "sony", "ps4"); }
        pruefe(k->RM_FEHLER_N == RM_MAX, "höchstens RM_MAX verschiedene Kürzel");
    }
    printf("== NEU 1.0.10: nicht lesbare Ordner nach Bereich und Grund\n");
    {
        arena *b = arena_neu();
        const char *P[] = { "/user/app/CUSA00001/x", "/user/app/CUSA00002/y", "/user/app/PPSA01234/z", "/user/data/playden/runtime-state",
                            "/user/data/tief/a/b/c", "/mnt/ext1/home/meinname/geheim" };
        for (int i = 0; i < 6; i++) sl_add(b, &k->SCHUTZ.diag, P[i]);
        sl_add(b, &k->SCHUTZ.zu_tief, P[4]);
        sl_add(b, &k->SCHUTZ.lese_fehler, P[0]); sl_add(b, &k->SCHUTZ.lese_code, "EACCES");
        sl_add(b, &k->SCHUTZ.lese_fehler, P[1]); sl_add(b, &k->SCHUTZ.lese_code, "EACCES");
        sl_add(b, &k->SCHUTZ.lese_fehler, P[3]); sl_add(b, &k->SCHUTZ.lese_code, "EACCES");
        sl_add(b, &k->SCHUTZ.lese_fehler, P[5]); sl_add(b, &k->SCHUTZ.lese_code, "?");
        char uz[400]; int n = kern_rm_unlesbar(k, uz, sizeof uz);
        printf("     %s\n", uz);
        pruefe(n == 5 && !strncmp(uz, "app4.EACCES*2~", 14), "häufigste zuerst: app4.EACCES*2");
        pruefe(strstr(uz, "app5.lesen*1") && strstr(uz, "data.EACCES*1") && strstr(uz, "data.tief*1") && strstr(uz, "m2.lesen*1"), "PS5-Spiel, Daten, zu tief, M.2 ohne Grund");
        pruefe(!strstr(uz, "CUSA") && !strstr(uz, "playden") && !strstr(uz, "meinname") && !strchr(uz, '/'), "keine Kennung, kein Name, kein Pfad");
        pruefe(!strcmp(kern_lese_grund(k, P[0]), "EACCES") && kern_lese_grund(k, P[5]) == NULL && kern_lese_grund(k, P[2]) == NULL, "Grund fürs Protokoll");
        char kurz[20]; kern_rm_unlesbar(k, kurz, sizeof kurz);
        pruefe(!strcmp(kurz, "app4.EACCES*2"), "zu wenig Platz: nur ganze Gruppen");
        k->SCHUTZ.diag = (sliste){0};
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
    printf("== NEU 1.0.10 (da_Chris86 10.11.2026 18:15): Programme der Fehler und erfolgreiche Wege\n");
    {
        rm_zaehler z = {0};
        kern_rm_zaehle(&z, "hb"); kern_rm_zaehle(&z, "playden"); kern_rm_zaehle(&z, "playden"); kern_rm_zaehle(&z, ""); kern_rm_zaehle(&z, NULL);
        char t[200]; kern_rm_zaehler_text(&z, t, sizeof t);
        printf("     %s\n", t);
        pruefe(!strcmp(t, "playden*2~hb*1"), "zusammengefasst, häufigste zuerst, leere Kürzel ignoriert");
        rm_zaehler v = {0};
        for (int i = 0; i < RM_MAX + 5; i++) { char kz[16]; snprintf(kz, sizeof kz, "k%d", i); kern_rm_zaehle(&v, kz); }
        pruefe(v.z == RM_MAX, "höchstens RM_MAX verschiedene");
        char eng[12]; kern_rm_zaehler_text(&z, eng, sizeof eng);
        pruefe(!strcmp(eng, "playden*2"), "zu wenig Platz: nur ganze Kürzel");
        arena *b = arena_neu();
        struct { const char *id, *pfad; int art; unsigned char store; const char *soll; } F[] = {
            { "FAKE77031", "/user/app/FAKE77031", ART_TOOL_KACHEL, 0, "playden" },
            { "FAKE78001", "/system_ex/app/FAKE78001", ART_SPIEL, 0, "playden" },
            { NULL, "/user/data/playden/runtime-state", ART_TOOL_DATEN, 0, "playden" },
            { NULL, "/data/homebrew/prosperostore/previous/PPSA99008", ART_APP_DATEN, 1, "pstore" },
            { "TBSH00001", "/user/app/TBSH00001", ART_TOOL_KACHEL, 0, "tabshifter" },
            { NULL, "/user/data/prosperoeden", ART_APP_DATEN, 0, "bekannt" },
            { "PPSA12345", "/user/app/PPSA12345", ART_APP, 0, "hb" },
            { "PPSA04264", "/user/app/PPSA04264", ART_SPIEL, 0, "spiel" },
            { NULL, "/user/home/u1/savedata/CUSA00001", ART_SPIELSTAND, 0, "sonst" },
        };
        for (size_t i = 0; i < sizeof F / sizeof *F; i++) {
            eintrag *f = calloc(1, sizeof *f); f->KachelId = F[i].id; f->Art = F[i].art; f->StoreTeil = F[i].store; sl_add(b, &f->Pfade, F[i].pfad);
            const char *ist = kern_rm_programm(k, f);
            char was[160]; snprintf(was, sizeof was, "%s -> %s (ist %s)", F[i].pfad, F[i].soll, ist);
            pruefe(!strcmp(ist, F[i].soll), was);
        }
        eintrag *kachel = calloc(1, sizeof *kachel); kachel->KachelId = "PLDM00001"; kachel->Art = ART_TOOL_KACHEL;
        eintrag *daten = calloc(1, sizeof *daten); daten->Art = ART_TOOL_DATEN; daten->ToolKachel = kachel; sl_add(b, &daten->Pfade, "/user/data/irgendwas");
        pruefe(!strcmp(kern_rm_programm(k, daten), "pldmgr"), "Daten unter einer Tool-Kachel gehören zu deren Programm");
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
