/* PS5 Cleanup 1.0.6 - Test vollständige Selbstentfernung inkl. Startordner (29.10.2026, PS5-Test Schritt 4):
 * Ablauf wie auf der PS5: Testeintrag wählen -> "Nur Auswahl löschen" -> "PS5 Cleanup jetzt auch entfernen?" -> Entfernen.
 * Sony-Dienst und Startordner-Entfernung sind hier Testfassungen (wie plattform_ps5.c, nur im Testbaum).
 *   test_selbst <hier> <fall>   fall: sofort | appmeta_bleibt | appmeta_spaet | sysmeta_fehlt   (ohne Hintergrundteil, wie bisher)
 *                                     uebergabe | kein_hintergrund | fehler_vorher               (NEU 23:08: Übergabe an den Hintergrundteil) */
#define _GNU_SOURCE
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>

static bool sl_enthaelt(const sliste *l, const char *s) { for (int i = 0; i < l->n; i++) if (strcmp(l->e[i], s) == 0) return true; return false; }
static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static arena *A0; static kern *K; static sliste LOG;
static void melde_log(void *c, const char *t) { (void)c; sl_add(A0, &LOG, t); printf("LOG: %s\n", t); }
static bool weiter_ja(void *c) { (void)c; return true; }
static const char *H, *FALL;
static const char *w(const char *p) { return VERB(A0, H, "/w", p); }
static bool da_w(const char *p) { struct stat st; return lstat(w(p), &st) == 0; }
static void weg(const char *p) { char c[9000]; snprintf(c, sizeof c, "rm -rf '%s'", w(p)); if (system(c)) {} }
static int log_zeile(const char *a, const char *b) { for (int i = 0; i < LOG.n; i++) if (strstr(LOG.e[i], a) && (!b || strstr(LOG.e[i], b))) return i; return -1; }

/* Sony (Testfassung): App-Ordner sofort weg; Kachel-Infos je nach Fall */
static void *spaet_faden(void *x) { (void)x; usleep(400 * 1000); weg("/user/appmeta/PSCL00001"); weg("/system_data/priv/appmeta/PSCL00001"); return NULL; }
static int SONY_AUFTRAEGE = 0;
static bool sofort_fall(void) { return !strcmp(FALL, "sofort") || !strcmp(FALL, "kein_hg_ro") || !strcmp(FALL, "sandbox") || !strcmp(FALL, "uebergabe") || !strcmp(FALL, "kein_hintergrund") || !strcmp(FALL, "fehler_vorher"); }
static antwort sony_test(arena *a, const char *id, bool nur_pruefen)
{
    (void)a;
    if (!strcmp(id, "PSCL00002")) return (antwort){550, "550 PS5 Cleanup läuft gerade über diese App"};
    if (nur_pruefen) return (antwort){200, "200 OK"};
    if (!strcmp(FALL, "kein_hg_ablehnen")) { SONY_AUFTRAEGE++; return (antwort){550, "550 Sony 0x80990001"}; }
    printf("INFO Sony deinstalliert %s\n", id); SONY_AUFTRAEGE++;
    weg(VERB(A0, "/user/app/", id));
    if (sofort_fall()) { weg(VERB(A0, "/user/appmeta/", id)); weg(VERB(A0, "/system_data/priv/appmeta/", id)); }
    else if (!strcmp(FALL, "appmeta_spaet")) { pthread_t t; pthread_create(&t, NULL, spaet_faden, NULL); pthread_detach(t); }
    /* appmeta_bleibt / sysmeta_fehlt: Kachel-Infos bleiben liegen (Cleanup löscht sie selbst) */
    return (antwort){200, "200 OK"};
}
/* Startordner (Testfassung von so_entfernen): nur die eigenen Dateien im Testbaum */
static int STARTORDNER_AUFRUFE = 0;
static int startordner_test(arena *a, bool nur_pruefen, const char **text)
{
    (void)a; *text = NULL; STARTORDNER_AUFRUFE++;
    if (!strcmp(FALL, "kein_hg_ro")) { *text = "/system_ex ist in dieser Sitzung nicht mehr beschreibbar - Startordner bleibt (nichts verändert)."; return -2; }   /* PS5: nur einmal pro Einschalten */
    if (!da_w("/system_ex/app/PSCL00002")) return 0;
    if (!da_w("/system_ex/app/PSCL00002/.ps5cleanup_start")) { *text = "fremd"; return -1; }
    if (nur_pruefen) return 1;
    weg("/system_ex/app/PSCL00002");
    return 1;
}

static sliste DIAG;
static void diag_test(const char *t) { sl_add(A0, &DIAG, t); printf("DIAG: %s\n", t); }
static int diag_zeile(const char *a, const char *b) { for (int i = 0; i < DIAG.n; i++) if (strstr(DIAG.e[i], a) && (!b || strstr(DIAG.e[i], b))) return i; return -1; }
static int UEB_AUFRUFE = 0; static bool UEB_KACHEL, UEB_START;
static int ueb_ja(arena *a, bool kachel, bool startordner, const char **text) { (void)a; UEB_AUFRUFE++; UEB_KACHEL = kachel; UEB_START = startordner; *text = "PS5CLEANUP-UEBERNOMMEN"; return 1; }
static int ueb_unbestaetigt(arena *a, bool kachel, bool startordner, const char **text) { (void)a; (void)kachel; (void)startordner; UEB_AUFRUFE++; *text = "Antwort ohne Bestätigung: (leer)"; return -1; }
static int ueb_nein(arena *a, bool kachel, bool startordner, const char **text) { (void)a; (void)kachel; (void)startordner; UEB_AUFRUFE++; *text = "keine Verbindung zu 127.0.0.1:9099 (Connection refused)"; return 0; }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: test_selbst <hier> <fall>\n"); return 2; }
    H = argv[1]; FALL = argv[2];
    A0 = arena_neu();
    printf("== Selbstentfernung mit Startordner: %s\n", FALL);
    zugriff_sony_deinstallieren = sony_test;
    zugriff_startordner_entfernen = startordner_test;
    zugriff_diagnose = diag_test;
    bool ueb = !strcmp(FALL, "uebergabe") || !strcmp(FALL, "fehler_vorher") || !strcmp(FALL, "sandbox") || !strcmp(FALL, "geist"), ohne_hg = !strcmp(FALL, "kein_hintergrund") || !strcmp(FALL, "kein_hg_ro") || !strcmp(FALL, "kein_hg_ablehnen");
    if (ueb) zugriff_selbst_uebergeben = ueb_ja; else if (ohne_hg) zugriff_selbst_uebergeben = ueb_nein; else if (!strcmp(FALL, "unbestaetigt")) zugriff_selbst_uebergeben = ueb_unbestaetigt;
    datei_optionen o = { .wurzel = w(""), .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    K = kern_neu(zugriff_datei(&o)); K->melde = melde_log;
    volatile int fe = 0;
    /* NEU 23:50 (PS5-Test 23:45): über die Kachel ist der Arbeitsordner der Laufzeitbereich der App PSCL00002 */
    const char *ort = !strcmp(FALL, "sandbox") ? "/mnt/sandbox/PSCL00002_000" : "/user/data/homebrew/PS5Cleanup";
    VERSUCHE { kern_selbstschutz_eigener_pfad(K, ort); kern_selbstschutz_ermitteln(K); kern_suche_funde(K); }
    FANGE { printf("FAIL Suche: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) { printf("FEHLERZAHL 1\n"); return 1; }
    for (int i = 0; i < K->SCHUTZ.eigene.n; i++) printf("INFO eigen: %s\n", K->SCHUTZ.eigene.e[i]);
    eintrag *t = NULL;
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; f->Wahl = false; if (strstr(f->Text, "ps5cleanup_austragen.txt")) t = f; }
    pruefe(t != NULL && !t->Gesperrt, "Testeintrag ps5cleanup_austragen.txt wird angeboten");
    if (!t) { printf("FEHLERZAHL %d\n", fehler); return 1; }
    printf("INFO Testeintrag: Bereich %d | %s\n", t->Kat, t->Text);
    t->Wahl = true;
    ergebnis e = kern_fuehre_loeschung(K, weiter_ja, NULL);
    pruefe(e.Fehler == 0 && !da_w("/user/data/ps5cleanup_austragen.txt"), "Nur Auswahl löschen: Testeintrag gelöscht, ohne Fehler (sonst käme die Frage nach der Selbstentfernung nicht)");
    pruefe(kern_selbst_da(K, false, false), "danach: PS5 Cleanup ist noch da (Frage \"PS5 Cleanup jetzt auch entfernen?\")");
    int n0 = LOG.n;
    int f = kern_selbst_entfernen(K, false, false);
    printf("INFO Selbstentfernung Fehler=%d, Startordner-Aufrufe=%d\n", f, STARTORDNER_AUFRUFE);
    for (int i = n0; i < LOG.n; i++) printf("SELBST: %s\n", LOG.e[i]);
    usleep(800 * 1000);
    pruefe(log_zeile("Startordner", NULL) < 0 && log_zeile("PSCL00002", NULL) < 0, "kein Hinweis zum Startordner auf dem Bildschirm (da_Chris86 30.10.2026 00:26)");
    pruefe(diag_zeile("Selbstentfernung: Beginn", NULL) >= 0 && diag_zeile("Selbstentfernung: Ende", NULL) >= 0, "Diagnoseprotokoll: Beginn und Ende der Selbstentfernung");
    pruefe(diag_zeile("/mnt/usb0/ps5cleanup.elf", "übersprungen: liegt auf USB-Laufwerk - wird nicht mitgeleert") >= 0, "Diagnoseprotokoll: ELF auf dem Stick mit Grund übersprungen");
    pruefe(da_w("/mnt/usb0/ps5cleanup.elf"), "ELF auf dem USB-Stick bleibt (Nur Auswahl)");
    if (!strcmp(FALL, "uebergabe")) {
        pruefe(f == 0 && K->SELBST_UEBERGEBEN, "Übergabe: ohne Fehler, an den Hintergrundteil übergeben");
        pruefe(UEB_AUFRUFE == 1 && UEB_KACHEL && UEB_START, "Übergabe: genau ein Auftrag (Kachel + Startordner)");
        pruefe(SONY_AUFTRAEGE == 0 && da_w("/user/app/PSCL00001/.ps5cleanup_kachel"), "Übergabe: die Oberfläche beauftragt Sony NICHT selbst, Kachel bleibt bis zum Hintergrundteil");
        pruefe(STARTORDNER_AUFRUFE == 0 && da_w("/system_ex/app/PSCL00002/.ps5cleanup_start"), "Übergabe: die Oberfläche fasst den Startordner nicht an");
        pruefe(!da_w("/user/data/PS5Cleanup"), "Übergabe: übrige eigene Daten löscht die Oberfläche wie bisher");
        pruefe(log_zeile("Die Kachel wird entfernt, sobald PS5 Cleanup beendet ist", NULL) >= 0 && log_zeile("hat sich selbst entfernt", NULL) < 0, "Übergabe: Meldung - Kachel folgt nach dem Beenden, kein \"ganz entfernt\"");
        pruefe(diag_zeile("Kachel PSCL00001 - Prüfung bestanden", NULL) >= 0 && diag_zeile("Übergabe an den Hintergrundteil", "-> 1") >= 0, "Diagnoseprotokoll: Prüfung und Übergabe mit Ergebnis");
        pruefe(diag_zeile("/user/app/PSCL00001", "übernimmt der Hintergrundteil") >= 0, "Diagnoseprotokoll: Kachel-Ordner mit Grund übersprungen");
    } else if (!strcmp(FALL, "sandbox")) {
        pruefe(!K->SCHUTZ.unsicher, "Laufzeitbereich als Arbeitsordner: eigener Ort gilt als bekannt (keine Sperre aller Homebrew-Daten)");
        pruefe(f == 0 && K->SELBST_UEBERGEBEN && UEB_AUFRUFE == 1, "Laufzeitbereich: kein Fehler, Übergabe an den Hintergrundteil (PS5-Fehler 22:53/23:45 behoben)");
        pruefe(log_zeile("SICHERHEITSSTOPP", "/mnt/sandbox") < 0 && da_w("/mnt/sandbox/PSCL00002_000/system/x"), "Laufzeitbereich wird nicht angefasst und nicht als Fehler gemeldet");
    } else if (!strcmp(FALL, "unbestaetigt")) {
        pruefe(f >= 1 && !K->SELBST_UEBERGEBEN && UEB_AUFRUFE == 1 && SONY_AUFTRAEGE == 0, "nicht bestätigt: Fehler, die Oberfläche macht NICHT selbst weiter (kein Sony-Auftrag)");
        pruefe(da_w("/user/app/PSCL00001/.ps5cleanup_kachel") && da_w("/system_ex/app/PSCL00002/.ps5cleanup_start") && STARTORDNER_AUFRUFE == 0, "nicht bestätigt: Kachel und Startordner bleiben");
        pruefe(log_zeile("FEHLER", "nicht bestätigt") >= 0, "nicht bestätigt: Fehlerzeile auf dem Bildschirm");
    } else if (!strcmp(FALL, "geist")) {
        /* NEU 1.0.7 (da_Chris86 31.10.2026 23:32, keine Geisterkacheln): Kachel bleibt samt Dateien - PS5 Cleanup bleibt startbar */
        pruefe(UEB_AUFRUFE == 0 && SONY_AUFTRAEGE == 0 && f >= 1 && log_zeile("BLEIBT", "Kachel - es gibt noch eine Kopie") >= 0,
               "Kachel nicht über Sony möglich (Kopie auf M.2): keine Übergabe, Kachel als bleibend gemeldet (kein \"ganz entfernt\")");
        pruefe(da_w("/user/app/PSCL00001/.ps5cleanup_kachel") && da_w("/user/appmeta/PSCL00001") && da_w("/system_data/priv/appmeta/PSCL00001"),
               "Kachel nicht über Sony möglich: ihre Dateien bleiben (keine Geisterkachel, 1.0.7)");
        /* 1.0.7: die bleibende Kachel zählt als BLEIBT - dann "Übergabe nicht ausgelöst - vorher 1 Fehler/BLEIBT" */
        pruefe(diag_zeile("Übergabe nicht ausgelöst", NULL) >= 0 && da_w("/system_ex/app/PSCL00002/.ps5cleanup_start"), "Diagnoseprotokoll: Grund; Startordner bleibt");
    } else if (!strcmp(FALL, "kein_hg_ablehnen")) {
        pruefe(UEB_AUFRUFE == 1 && SONY_AUFTRAEGE == 1 && f >= 1 && log_zeile("BLEIBT", "Sony hat sie nicht entfernt") >= 0, "kein Hintergrundteil + Sony lehnt ab: Kachel bleibt (gemeldet)");
        /* NEU 1.0.7 (da_Chris86 31.10.2026 23:32): bis 1.0.6 wurden die Kachel-Dateien trotzdem gelöscht (Geisterkachel) */
        pruefe(da_w("/user/app/PSCL00001/.ps5cleanup_kachel") && da_w("/user/appmeta/PSCL00001") && da_w("/system_data/priv/appmeta/PSCL00001"),
               "kein Hintergrundteil + Sony lehnt ab: Kachel-Dateien bleiben (keine Geisterkachel, 1.0.7)");
    } else if (!strcmp(FALL, "fehler_vorher")) {
        pruefe(f >= 1 && !K->SELBST_UEBERGEBEN && UEB_AUFRUFE == 0, "Fehler vorher: keine Übergabe");
        pruefe(SONY_AUFTRAEGE == 0 && da_w("/user/app/PSCL00001") && da_w("/system_ex/app/PSCL00002/.ps5cleanup_start"), "Fehler vorher: Kachel und Startordner bleiben (PS5 Cleanup bleibt startbar)");
        pruefe(log_zeile("BLEIBT", "Kachel - es sind Fehler") >= 0, "Fehler vorher: Meldung BLEIBT Kachel");
        pruefe(diag_zeile("Übergabe nicht ausgelöst", "1 Fehler") >= 0 && diag_zeile("FEHLER", "/user/data/PS5Cleanup") >= 0, "Diagnoseprotokoll: Fehlerzeile und Grund für die fehlende Übergabe");
    } else {
        pruefe(f == 0, "Selbstentfernung ohne Fehler");
        pruefe(!da_w("/user/app/PSCL00001") && !da_w("/user/appmeta/PSCL00001") && !da_w("/system_data/priv/appmeta/PSCL00001"), "Kachel PSCL00001 mit Kachel-Infos weg");
        if (!strcmp(FALL, "kein_hg_ro")) {
            pruefe(STARTORDNER_AUFRUFE == 1 && da_w("/system_ex/app/PSCL00002/.ps5cleanup_start"), "nicht mehr beschreibbar: Startordner bleibt");
            pruefe(diag_zeile("Startordner: bleibt - /system_ex ist in dieser Sitzung nicht mehr beschreibbar", NULL) >= 0, "Diagnoseprotokoll: Grund, warum der Startordner bleibt");
        } else {
            pruefe(STARTORDNER_AUFRUFE == 1 && !da_w("/system_ex/app/PSCL00002"), "Startordner /system_ex/app/PSCL00002 entfernt");
            pruefe(diag_zeile("Startordner durch die Oberfläche", "-> 1") >= 0 && diag_zeile("Startordner: gelöscht", NULL) >= 0, "Diagnoseprotokoll: Startordner mit Ergebnis");
        }
        if (ohne_hg) pruefe(UEB_AUFRUFE == 1 && diag_zeile("Übergabe an den Hintergrundteil", "-> 0 keine Verbindung") >= 0 && SONY_AUFTRAEGE == 1 && log_zeile("Hintergrundteil nicht erreichbar", NULL) >= 0,
                            "kein Hintergrundteil: Grund im Protokoll, die Oberfläche entfernt Kachel (Sony) und Startordner selbst");
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
