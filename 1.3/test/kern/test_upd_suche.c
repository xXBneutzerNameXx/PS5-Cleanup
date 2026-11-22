/* PS5 Cleanup 1.3 - Test für app/upd_suche.c (Update "Ja, ersetzen" findet die alte Programmdatei auch ohne Nachschauen).
 * Legt in argv[1] Testordner an und prüft Suche und Auswahl. Ausgabe: OK/FAIL-Zeilen, Rückgabewert 0 nur ohne FAIL.
 * Gebaut mit -DUPD_SUCHE_MAX_BYTE=300000 (Grenze für zu große Dateien prüfbar). */
#define _GNU_SOURCE
#include "../../app/upd_suche.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *KENN = "PS5CLEANUP-ELF-KENNUNG:7c1e5b2a-selbstschutz";
static int fehler = 0;
static void pruefe(int ok, const char *was) { printf("%s %s\n", ok ? "OK  " : "FAIL", was); if (!ok) fehler++; }

/* Datei: elf = mit ELF-Kopf, kenn_bei = Position der Kennung (-1 = keine), groesse = Gesamtgröße */
static void datei(const char *p, int elf, long kenn_bei, long groesse)
{
    FILE *f = fopen(p, "wb"); if (!f) { perror(p); exit(2); }
    char *b = calloc(1, (size_t)groesse); if (!b) exit(2);
    if (elf) memcpy(b, "\x7f" "ELF", 4);
    if (kenn_bei >= 0) memcpy(b + kenn_bei, KENN, strlen(KENN));
    fwrite(b, 1, (size_t)groesse, f); fclose(f); free(b);
}
static int gleich_ende(const char *a, const char *ende) { size_t la = strlen(a), le = strlen(ende); return la >= le && !strcmp(a + la - le, ende); }

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    char A[512], B[512], C[512], D[512], P[1024];
    snprintf(A, sizeof A, "%s/usb", argv[1]); snprintf(B, sizeof B, "%s/data", argv[1]);
    snprintf(C, sizeof C, "%s/neuer", argv[1]); snprintf(D, sizeof D, "%s/gross", argv[1]);
    mkdir(argv[1], 0755); mkdir(A, 0755); mkdir(B, 0755); mkdir(C, 0755); mkdir(D, 0755);
    /* A: die laufende 1.3 (Kennung genau über der 64-KB-Grenze), eine ältere 1.2, eine ELF ohne Kennung, eine Verknüpfung, ein Text */
    snprintf(P, sizeof P, "%s/ps5cleanup_v1.3.elf", A); datei(P, 1, 65536 - 10, 200000);
    snprintf(P, sizeof P, "%s/ps5cleanup_v1.2.elf", A); datei(P, 1, 500, 100000);
    snprintf(P, sizeof P, "%s/fremd_cleanup.elf", A); datei(P, 1, -1, 50000);
    snprintf(P, sizeof P, "%s/text_cleanup.elf", A); datei(P, 0, 100, 5000);          /* Kennung, aber kein ELF-Kopf */
    snprintf(P, sizeof P, "%s/ps5cleanup_verkn.elf", A); if (symlink("ps5cleanup_v1.3.elf", P)) perror("symlink");
    snprintf(P, sizeof P, "%s/notiz.txt", A); datei(P, 0, 0, 200);
    snprintf(P, sizeof P, "%s/ganz_anders.elf", A); datei(P, 1, 10, 3000);           /* Kennung, aber "cleanup" fehlt im Namen */
    /* B: eine umbenannte Kopie ohne Versionsnummer, Großbuchstaben */
    snprintf(P, sizeof P, "%s/PS5Cleanup.ELF", B); datei(P, 1, 20, 4000);
    /* C: nur eine NEUERE Version */
    snprintf(P, sizeof P, "%s/ps5cleanup_v1.5.elf", C); datei(P, 1, 20, 4000);
    /* D: zu groß (Grenze im Test 300000) */
    snprintf(P, sizeof P, "%s/ps5cleanup_v1.3.elf", D); datei(P, 1, 100, 400000);

    char aus[1024]; int n;
    { const char *o[] = { A }; n = upd_eigene_elf_suchen(o, 1, KENN, "1.3", aus, sizeof aus);
      pruefe(n == 2, "nur echte PS5-Cleanup-Dateien gezählt (2: v1.3 und v1.2 - ohne Kennung, ohne ELF-Kopf, Verknüpfung, ohne \"cleanup\" nicht)");
      pruefe(gleich_ende(aus, "/usb/ps5cleanup_v1.3.elf"), "Kennung über der Lesegrenze erkannt, laufende Version 1.3 gewählt"); }
    { const char *o[] = { A }; upd_eigene_elf_suchen(o, 1, KENN, "1.4", aus, sizeof aus);
      pruefe(aus[0] == 0, "Version 1.4 läuft, nur v1.3/v1.2 da: keine gewählt (nie eine andere Version löschen)"); }
    { const char *o[] = { B }; upd_eigene_elf_suchen(o, 1, KENN, "1.3", aus, sizeof aus);
      pruefe(gleich_ende(aus, "/data/PS5Cleanup.ELF"), "einzige Datei ohne Versionsnummer (Großschreibung) gewählt"); }
    { const char *o[] = { C }; n = upd_eigene_elf_suchen(o, 1, KENN, "1.4", aus, sizeof aus);
      pruefe(n == 1 && aus[0] == 0, "nur eine NEUERE Version da: gefunden, aber nicht gewählt"); }
    { const char *o[] = { D }; n = upd_eigene_elf_suchen(o, 1, KENN, "1.3", aus, sizeof aus);
      pruefe(n == 0 && aus[0] == 0, "zu große Datei wird nicht gelesen"); }
    { const char *o[] = { A, A, "/gibt/es/nicht", NULL, "relativ" }; n = upd_eigene_elf_suchen(o, 5, KENN, "1.3", aus, sizeof aus);
      pruefe(n == 2 && gleich_ende(aus, "/usb/ps5cleanup_v1.3.elf"), "doppelter Ordner einmal, fehlende/leere/relative Ordner ohne Absturz"); }
    { const char *o[] = { B, A }; upd_eigene_elf_suchen(o, 2, KENN, "1.3", aus, sizeof aus);
      pruefe(gleich_ende(aus, "/usb/ps5cleanup_v1.3.elf"), "genaue Version vor einer Datei ohne Versionsnummer"); }
    { const char *o[] = { B, A }; upd_eigene_elf_suchen(o, 2, KENN, "1.4", aus, sizeof aus);
      pruefe(gleich_ende(aus, "/data/PS5Cleanup.ELF"), "keine genaue Version: die einzige ohne Versionsnummer"); }
    /* Auswahl direkt (Liste aus dem Nachschauen) */
    { const char *l[] = { "/data/ps5cleanup_v1.3.elf", "/mnt/usb0/ps5cleanup_v1.3.elf" };
      pruefe(upd_alte_waehlen(l, 2, "1.3") == 1, "USB-Laufwerk vor interner Kopie"); }
    { const char *l[] = { "/user/data/pldmgr/payloads/repo/ps5cleanup@x/ps5cleanup_v1.3.elf" };
      pruefe(upd_alte_waehlen(l, 1, "1.3") == -1, "Kopie des Payload Managers wird nie gewählt"); }
    { const char *l[] = { "/mnt/usb0/ps5cleanup_v1.1.elf", "/mnt/usb0/ps5cleanup_v1.2.elf" };
      pruefe(upd_alte_waehlen(l, 2, "1.3") == -1, "mehrere fremde Versionen: keine (vorher nahm 1.2 die erste)"); }
    { const char *l[] = { "/mnt/usb0/ps5cleanup.elf", "/data/ps5cleanup.elf" };
      pruefe(upd_alte_waehlen(l, 2, "1.3") == -1, "zwei Dateien ohne Versionsnummer: keine (nicht eindeutig)"); }
    { const char *l[] = { "/data/homebrew/PS5Cleanup", "/mnt/usb0/PS5-Cleanup-Test_v1.0.7.elf" };
      pruefe(upd_alte_waehlen(l, 2, "1.3") == -1, "Ordner und alte Test-Datei mit Versionsnummer: keine"); }
    /* NEU 1.3: "Neu in x" aus den Release-Notes (upd_neu_punkte) - Aufbau wie die Release-Notes von 1.2 */
    {
        static const char RN[] =
            "## Changelog 1.2\n\n"
            "- **Behoben \xE2\x80\x93 Firmware 13.x:** Auf Firmware 13.x scheiterte das L\xC3\xB6schen bisher mit \xE2\x80\x9E" "Bad file descriptor\". Jetzt nicht mehr.\n"
            "- **Sicherheit:** Vor dem L\xC3\xB6schen wird gepr\xC3\xBC" "ft.\n"
            "* Ohne \xC3\x9C" "berschrift mit `Code`, [Link](https://example.org) und Emoji \xF0\x9F\x99\x8F\xEF\xB8\x8F fertig\n\n"
            "**Browser-Bedienung:** meist `http://<PS5-IP>:9100`.\n\n"
            "### Welche Datei?\n- **`ps5cleanup_v1.2.elf`** \xE2\x80\x93 die eine Datei\n\n---\n\n"
            "\xF0\x9F\x87\xAC\xF0\x9F\x87\xA7 **In short:** Deleting on firmware 13.x used to fail with \"Bad file descriptor\". Fixed in 1.2 \xE2\x80\x93 like on 12.x. Thanks! \xF0\x9F\x99\x8F\n";
        char a[6][UPD_NEU_LAENGE]; int n;
        n = upd_neu_punkte(RN, UPD_NEU_TITEL, a, 6);
        pruefe(n == 3, "Neu: drei Punkte aus der ERSTEN Aufzählung (nicht \"Welche Datei?\")");
        pruefe(n >= 1 && !strcmp(a[0], "Behoben \xE2\x80\x93 Firmware 13.x"), "Neu: fette Überschrift ohne Doppelpunkt");
        pruefe(n >= 2 && !strcmp(a[1], "Sicherheit"), "Neu: zweite Überschrift");
        pruefe(n >= 3 && !strcmp(a[2], "Ohne \xC3\x9C" "berschrift mit Code, Link und Emoji fertig"), "Neu: Punkt ohne Überschrift ganz - ohne `, Link-Adresse und Emoji");
        n = upd_neu_punkte(RN, UPD_NEU_TITEL, a, 2);
        pruefe(n == 2, "Neu: höchstens max Punkte");
        n = upd_neu_punkte(RN, UPD_NEU_GANZ, a, 6);
        pruefe(n == 3 && strstr(a[0], "Behoben \xE2\x80\x93 Firmware 13.x: Auf Firmware") == a[0] && strstr(a[0], "Jetzt nicht mehr."), "Neu (Protokoll): ganzer Punkt samt Überschrift");
        n = upd_neu_punkte(RN, UPD_NEU_EN, a, 6);
        pruefe(n == 3, "Neu (englisch): drei Sätze aus \"In short\"");
        pruefe(n >= 1 && !strcmp(a[0], "Deleting on firmware 13.x used to fail with \"Bad file descriptor\"."), "Neu (englisch): erster Satz ganz, Satzende nach Anführungszeichen");
        pruefe(n >= 3 && !strcmp(a[2], "Thanks!"), "Neu (englisch): Emoji am Ende weg");
        pruefe(upd_neu_punkte("Nur Text.\n\nKeine Aufz\xC3\xA4hlung.", UPD_NEU_TITEL, a, 6) == 0, "Neu: ohne Aufzählung keine Punkte");
        pruefe(upd_neu_punkte("- Punkt", UPD_NEU_EN, a, 6) == 0, "Neu (englisch): ohne \"In short\" keine Punkte");
        pruefe(upd_neu_punkte(NULL, UPD_NEU_TITEL, a, 6) == 0 && upd_neu_punkte("", UPD_NEU_TITEL, a, 6) == 0, "Neu: kein Text ohne Absturz");
        static char lang[3000]; strcpy(lang, "- "); for (int i = 0; i < 900; i++) strcat(lang, "\xC3\xA4");   /* 900 x "ä" (je 2 Bytes) */
        n = upd_neu_punkte(lang, UPD_NEU_TITEL, a, 6);
        size_t l = n ? strlen(a[0]) : 0;
        pruefe(n == 1 && l > 300 && l < UPD_NEU_LAENGE && l % 2 == 0, "Neu: zu langer Punkt gekürzt, nie mitten in einem Zeichen");
        pruefe(upd_neu_punkte("- **Fett ohne Ende\n- zweiter", UPD_NEU_TITEL, a, 6) == 2 && !strcmp(a[0], "Fett ohne Ende"), "Neu: offenes ** ohne Absturz");
    }
    printf("ZUSAMMEN: %d Fehler\n", fehler);
    return fehler ? 1 : 0;
}
