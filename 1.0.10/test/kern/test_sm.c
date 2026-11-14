/* PS5 Cleanup - Test ShadowMount: verschobene Spieldateien (27.10.2026, Fehler auf da_Chris86s PS5:
 * Red Dead Redemption PPSA30528 und Overdrive PPSA99058 lagen in /data/homebrew, die Kacheln zeigten noch auf
 * /mnt/ext1/homebrew - PS5 Cleanup meldete "Spiel nicht mehr da, tote Kachel").
 *   test_sm <hier> [schutz]   Testbaum <hier>/w (siehe lauf_sm.sh) */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static void melde_log(void *c, const char *t) { (void)c; (void)t; }
static kern *K; static arena *A0;
static eintrag *kachel(const char *id) { for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Kat == 1 && strstr(f->Text, id)) return f; } return NULL; }
static bool nein(void *c) { (void)c; return false; }
static bool da_w(const char *hier, const char *p) { struct stat st; return lstat(VERB(A0, hier, "/w", p), &st) == 0; }
static bool hat(const eintrag *f, const char *p) { for (int j = 0; j < f->Pfade.n; j++) if (!strcmp(f->Pfade.e[j], p)) return true; return false; }
static eintrag *mit_pfad(const char *p) { for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (hat(f, p)) return f; } return NULL; }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 2) { fprintf(stderr, "Aufruf: test_sm <hier> [schutz]\n"); return 2; }
    bool schutz = argc >= 3 && !strcmp(argv[2], "schutz");
    A0 = arena_neu();
    printf("== ShadowMount verschoben %s\n", schutz ? "mit Selbstschutz" : "ohne Selbstschutz");
    datei_optionen o = { .wurzel = VERB(A0, argv[1], "/w"), .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    K = kern_neu(zugriff_datei(&o)); K->melde = melde_log;
    volatile int fe = 0;
    VERSUCHE {
        if (schutz) { kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K); }
        kern_suche_funde(K);
    } FANGE { printf("FAIL Suche: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) { printf("FEHLERZAHL 1\n"); return 1; }
    if (argc >= 4 && !strcmp(argv[3], "einhaenge")) {   /* NEU 03.11.2026 (PS5-Test: ProsperoStore/Playden) - Einhängeordner gelöschter Spiele */
        eintrag *s1 = kachel("PPSA90001"), *s2 = kachel("PPSA90002");
        pruefe(s1 && !s1->Gesperrt && s1->Quelle && s1->Quelle[0], "Einhängeordner: internes ShadowMount-Spiel PPSA90001 mit eindeutiger Spieldatei");
        pruefe(da_w(argv[1], "/system_ex/app/PPSA90001") && da_w(argv[1], "/system_ex/app/FAKE77031"), "Einhängeordner: Testordner angelegt");
        kern_waehle_komplett(K, false, false);   /* vollständige Bereinigung, M.2 behalten */
        pruefe(s1 && s1->Wahl && hat(s1, s1->Quelle), "Einhängeordner: PPSA90001 als \"Spiel + Kachel\" gewählt");
        pruefe(s2 && (!s2->Wahl || !s2->Quelle[0] || !hat(s2, s2->Quelle)), "Einhängeordner: Spieldatei von PPSA90002 (M.2 behalten) nicht gewählt");
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        printf("ERG Fehler=%d Teilweise=%d\n", e.Fehler, e.Teilweise);
        pruefe(e.Fehler == 0, "Einhängeordner: ohne Fehler gelöscht");
        pruefe(!da_w(argv[1], "/user/data/homebrew/Spiel Eins - PPSA90001.exfat"), "Einhängeordner: Spieldatei von PPSA90001 weg");
        pruefe(!da_w(argv[1], "/system_ex/app/PPSA90001"), "Einhängeordner: leerer Ordner des ganz gelöschten Spiels PPSA90001 mit weg");
        pruefe(da_w(argv[1], "/system_ex/app/PPSA90002"), "Einhängeordner: Ordner von PPSA90002 bleibt (Spiel bleibt auf der M.2)");
        pruefe(da_w(argv[1], "/system_ex/app/PPSA90003/x"), "Einhängeordner: nicht leerer Ordner bleibt");
        pruefe(!da_w(argv[1], "/system_ex/app/FAKE78001"), "Einhängeordner: Spiel aus Playden (FAKE78001), nicht installiert - sein leerer Ordner geht wie jeder andere (1.0.10, da_Chris86 12.11.2026)");
        pruefe(!da_w(argv[1], "/system_ex/app/FAKE77031"), "Einhängeordner: Playden selbst nicht installiert - sein leerer Ordner geht (da_Chris86 03.11.2026 15:11)");
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    if (argc >= 4 && !strcmp(argv[3], "playden")) {   /* NEU 1.0.10 (da_Chris86 12.11.2026 07:49, Codex-Prüfung 161 (1)) */
        eintrag *a1 = NULL, *a2 = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (!f->SmDatei || f->PfadeBasis.n != 1) continue;
            if (!strcmp(f->PfadeBasis.e[0], "/user/data/homebrew/FAKE78011")) a1 = f;
            if (!strcmp(f->PfadeBasis.e[0], "/user/data/homebrew/FAKE78012")) a2 = f; }
        pruefe(a1 && a1->PlaydenStarter && kern_ampel(a1) == AMPEL_GELB && !a1->Rot, a_fmt(A0, "Starter eines Playden-Spiels (mit Playdens Bau-Angaben): gelb (%s)", a1 ? kern_zeile(A0, a1) : "-"));
        pruefe(a2 && !a2->PlaydenStarter && kern_ampel(a2) == AMPEL_ROT, a_fmt(A0, "fremdes Spiel mit nachgemachten Angaben, nicht in Playdens Liste (FAKE78012): rot wie jede Spieldatei (Codex-Prüfung 162) (%s)", a2 ? kern_zeile(A0, a2) : "-"));
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    if (argc >= 4 && !strcmp(argv[3], "gesperrt")) {   /* NEU 1.0.10 (wie ShadowMountPlus 1.7beta12): gesperrter Suchort bricht die Suche nicht ab */
        eintrag *g = kachel("PPSA77799"), *v = kachel("PPSA77798"), *m = kachel("PPSA77797");
        printf("INFO %s\nINFO %s\nINFO %s\n", g ? g->Text : "-", v ? v->Text : "-", m ? m->Text : "-");
        pruefe(true, "gesperrter Suchordner: die Suche läuft durch (bis 1.0.9 brach sie ab)");
        pruefe(g && !strstr(g->Text, "tote Kachel") && strstr(g->Text, "gesperrt: /user/data/etaHEN/games)") && !g->Gesperrt && (!g->Quelle || !g->Quelle[0]),
               "Spiel nicht am gemerkten Ort, ein Suchordner gesperrt: nicht \"tote Kachel\", gesperrter Ordner genannt, nur die Kachel löschbar");
        pruefe(v && strstr(v->Text, "vermutlich verschoben nach /user/data/homebrew/Weg2 - PPSA77798.exfat"), "anderer Suchordner gesperrt, Datei woanders gefunden: wie bisher \"vermutlich verschoben\"");
        pruefe(m && !strstr(m->Text, "tote Kachel") && strstr(m->Text, "/user/data/geheim)") && (!m->Quelle || !m->Quelle[0]), "schon der gemerkte Ort ist gesperrt: nicht \"tote Kachel\", Ordner genannt, nur die Kachel");
        pruefe(!mit_pfad("/user/data/etaHEN/games/Weg - PPSA77799.exfat") && !mit_pfad("/user/data/geheim/Drin - PPSA77797.exfat"), "Dateien in gesperrten Ordnern werden nie Löschziel");
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    if (argc >= 4 && !strcmp(argv[3], "m2weg")) {   /* keine M.2 eingebaut (kein Gerät /dev/ssd1): Kachel löschbar, samt Kachel-Infos */
        eintrag *f = kachel("PPSA90002");
        printf("INFO M2_GERAET=%d M2_UNKLAR=%d %s\n", K->M2_GERAET, K->M2_UNKLAR, f ? f->Text : "-");
        pruefe(K->M2_GERAET == 0 && !K->M2_UNKLAR, "kein M.2-Gerät: Zustand \"keine M.2 eingebaut\", nichts unklar");
        pruefe(f && !f->Gesperrt && (!f->Quelle || !f->Quelle[0]), "keine M.2: Kachel des M.2-Spiels löschbar, nur die Kachel");
        kern_waehle_komplett(K, false, false);
        bool m2pfad = false; if (f) for (int j = 0; j < f->Pfade.n; j++) if (!strncmp(f->Pfade.e[j], "/mnt/", 5)) m2pfad = true;
        pruefe(f && f->Wahl && !m2pfad, "keine M.2: vollständige Bereinigung wählt die Kachel, keinen Pfad auf einem Datenträger");
        pruefe(f && hat(f, "/user/app/PPSA90002") && hat(f, "/user/appmeta/PPSA90002") && hat(f, "/system_data/priv/appmeta/PPSA90002"),
               "keine M.2: \"nur Kachel\" nimmt die Kachel-Infos mit, auch /system_data/priv/appmeta (wie 1.0.5; Codex-Prüfung 19)");
        ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
        printf("ERG Fehler=%d\n", e.Fehler);
        pruefe(!da_w(argv[1], "/user/app/PPSA90002") && !da_w(argv[1], "/user/appmeta/PPSA90002") && !da_w(argv[1], "/system_data/priv/appmeta/PPSA90002"), "keine M.2: nach dem Löschen sind Kachel-Ordner und Kachel-Infos weg (Dateien - Datenbank-Neuaufbau selbst ist hier nicht prüfbar)");
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    if (argc >= 4 && !strcmp(argv[3], "m2unerkannt")) {   /* M.2-Gerät da, aber nicht eingehängt - da_Chris86 27.10.2026 21:39: Kacheln trotzdem löschbar */
        eintrag *f = kachel("PPSA90002");
        printf("INFO M2_GERAET=%d M2_UNKLAR=%d %s\n", K->M2_GERAET, K->M2_UNKLAR, f ? f->Text : "-");
        pruefe(f && !f->Gesperrt && (!f->Quelle || !f->Quelle[0]), "M.2 unerkannt: Kachel des M.2-Spiels löschbar, nur die Kachel (Spieldatei nicht erreichbar)");
        pruefe(f && hat(f, "/user/app/PPSA90002") && hat(f, "/user/appmeta/PPSA90002"), "M.2 unerkannt: \"nur Kachel\" nimmt die Kachel-Infos mit");
        eintrag *k1 = kachel("PPSA90001");
        const char *eins = "/user/data/homebrew/Spiel Eins - PPSA90001.exfat";
        printf("INFO %s\n", k1 ? k1->Text : "-");
        pruefe(k1 && !k1->Gesperrt && k1->Quelle && !strcmp(k1->Quelle, eins), "M.2 unerkannt: Spiel auf der internen SSD erreichbar und eindeutig - \"Spiel + Kachel\" möglich");
        bool gesp1 = false; for (int j = 0; j < K->SperrQuellen.n; j++) if (!strcmp(K->SperrQuellen.e[j], eins)) gesp1 = true;
        pruefe(!gesp1, "M.2 unerkannt: eindeutige interne Spieldatei nicht auf der Sperrliste");
        kern_waehle_komplett(K, true, true);
        bool m2pfad = false; if (f) for (int j = 0; j < f->Pfade.n; j++) if (!strncmp(f->Pfade.e[j], "/mnt/", 5)) m2pfad = true;
        pruefe(f && f->Wahl && !m2pfad, "M.2 unerkannt: vollständige Bereinigung wählt die Kachel, keinen Pfad auf einem Datenträger");
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    const char *rdr = "/user/data/homebrew/Red Dead Redemption - PPSA30528 - v1.2.0 - US.exfat";
    const char *ovd = "/user/data/homebrew/Overdrive.exfat";
    struct { const char *id, *datei; } faelle[2] = { { "PPSA30528", rdr }, { "PPSA99058", ovd } };
    /* Codex-Prüfung 11: ein Fund am anderen Ort ist nur ein Hinweis - nie zuordnen, Kachel und Datei sichtbar sperren */
    for (int i = 0; i < 2; i++) {
        eintrag *f = kachel(faelle[i].id);
        printf("INFO %s\n", f ? f->Text : "-");
        pruefe(f && !strstr(f->Text, "tote Kachel") && strstr(f->Text, "vermutlich verschoben") && strstr(f->Text, faelle[i].datei) && !f->Gesperrt,
               VERB(A0, faelle[i].id, ": nicht mehr \"tote Kachel\", vermuteter Ort genannt, Kachel bleibt löschbar (da_Chris86 19:38)"));
        pruefe(f && (!f->Quelle || !f->Quelle[0]) && !hat(f, faelle[i].datei), VERB(A0, faelle[i].id, ": Kachel ist der vermuteten Datei NICHT als Löschziel zugeordnet"));
        eintrag *d = mit_pfad(faelle[i].datei);
        /* da_Chris86 28.10.2026 13:41: ein Spiel-Abbild ist ein Spiel - eigener, löschbarer Eintrag in Bereich 1, nie an der Kachel */
        pruefe(d && d != f && !d->Gesperrt && d->Kat == 1 && strstr(d->Text, "Spiel-Abbild") && d->Pfade.n == 1, a_fmt(A0, "%s: die Datei steht als eigenes Spiel-Abbild da, ohne Kachel (%s)", faelle[i].id, d ? d->Text : "-"));
    }
    eintrag *weg = kachel("PPSA90003");
    pruefe(weg && strstr(weg->Text, "tote Kachel") && !weg->Gesperrt, a_fmt(A0, "echte tote Kachel bleibt tot und wählbar: %s", weg ? weg->Text : "-"));
    eintrag *usb = kachel("PPSA77771");
    pruefe(usb && !usb->Gesperrt && strstr(usb->Text, "/mnt/usb0/homebrew/"), a_fmt(A0, "auf USB verschoben: vermuteter Ort genannt, Kachel löschbar (%s)", usb ? usb->Text : "-"));
    eintrag *um = kachel("PPSA77772");
    pruefe(um && !um->Gesperrt && strstr(um->Text, "Neu PPSA77772.exfat") && (!um->Quelle || !um->Quelle[0]), a_fmt(A0, "umbenannt (nur Kennung gleich): nicht tot, nur Kachel löschbar (%s)", um ? um->Text : "-"));
    eintrag *um2 = mit_pfad("/user/data/homebrew/Neu PPSA77772.exfat");
    pruefe(um2 && !um2->Gesperrt && strstr(um2->Text, "Spiel-Abbild"), "umbenannte Datei selbst: eigenes Spiel-Abbild (1.0.6)");
    eintrag *sp = kachel("PPSA77773");
    pruefe(sp && strstr(sp->Text, "/user/data/meinespiele/Eigen.exfat") && mit_pfad("/user/data/meinespiele/Eigen.exfat") && !mit_pfad("/user/data/meinespiele/Eigen.exfat")->Gesperrt, a_fmt(A0, "eigener Suchort aus config.ini (mit Kommentar am Zeilenende) berücksichtigt, Datei eigenes Spiel-Abbild (%s)", sp ? sp->Text : "-"));
    /* NEU 1.0.9 (08.11.2026, ShadowMountPlus 1.7): Einstellungen im Format 2 */
    eintrag *sp2 = kachel("PPSA77782");
    pruefe(sp2 && strstr(sp2->Text, "/user/data/meine spiele2/Eigen2.exfat") && mit_pfad("/user/data/meine spiele2/Eigen2.exfat"), a_fmt(A0, "eigener Suchort aus config.ini Format 2 ([ Scan ] Path = \"...\" mit Kommentar) berücksichtigt (%s)", sp2 ? sp2->Text : "-"));
    eintrag *sp3 = kachel("PPSA77783");
    pruefe(sp3 && strstr(sp3->Text, "tote Kachel") && !strstr(sp3->Text, "nichtscan"), a_fmt(A0, "\"path\" unter [emulators] ist kein Suchort (%s)", sp3 ? sp3->Text : "-"));
    eintrag *ab = kachel("PPSA77774");
    pruefe(ab && !ab->Gesperrt && strstr(ab->Text, "Datenträger") && strstr(ab->Text, "nur die Kachel") && (!ab->Quelle || !ab->Quelle[0]), a_fmt(A0, "Spiel auf abgezogenem USB-Datenträger: Kachel löschbar, nur die Kachel (%s)", ab ? ab->Text : "-"));
    eintrag *m1 = kachel("PPSA77775"), *m2b = kachel("PPSA77776");
    eintrag *md = mit_pfad("/user/data/homebrew/Geteilt - PPSA77775.exfat");
    pruefe(m1 && m2b && (!m1->Quelle || !m1->Quelle[0]) && (!m2b->Quelle || !m2b->Quelle[0]) && strstr(m1->Text, "mehreren Kacheln"), "zwei Kacheln auf dieselbe Datei: bei beiden geht nur die Kachel");
    pruefe(md && !md->Gesperrt && strstr(md->Text, "mehrere Kacheln"), a_fmt(A0, "zwei Kacheln auf dieselbe Datei: die Datei ist ein eigenes Spiel-Abbild (%s)", md ? md->Text : "-"));
    eintrag *m2 = kachel("PPSA90002");
    pruefe(m2 && !strncmp(m2->Text, "M.2 – ", strlen("M.2 – ")) && m2->Ort == ORT_M2 && !m2->Gesperrt, a_fmt(A0, "normales M.2-Spiel: Ort M.2 vorne (%s)", m2 ? m2->Text : "-"));
    /* Codex-Prüfung 14 */
    eintrag *fr = kachel("PPSA77777");
    pruefe(fr && (!fr->Quelle || !fr->Quelle[0]), a_fmt(A0, "fremde Datei (andere Kennung im Namen) am gemerkten Ort ist nicht die Spieldatei (%s)", fr ? fr->Text : "-"));
    eintrag *dop = mit_pfad("/user/data/homebrew/Doppel.exfat"), *rue = mit_pfad("/user/data/homebrew/Rueck.exfat");
    eintrag *k78 = kachel("PPSA77778"), *k79 = kachel("PPSA77779"), *k70 = kachel("PPSA77770"), *k80 = kachel("PPSA77780");
    pruefe(dop && !dop->Gesperrt && k79 && (!k79->Quelle || !k79->Quelle[0]) && k78 && !k78->Gesperrt, "vermutet, danach echte Quelle: Datei eigenes Spiel-Abbild, beide Kacheln nur Kachel");
    pruefe(rue && !rue->Gesperrt && k70 && (!k70->Quelle || !k70->Quelle[0]) && k80 && !k80->Gesperrt, "echte Quelle, danach vermutet: Datei eigenes Spiel-Abbild, beide Kacheln nur Kachel");
    eintrag *z1 = mit_pfad("/user/data/homebrew/Zwei.exfat"), *z2 = mit_pfad("/user/data/homebrew/backports/Zwei.exfat");
    pruefe(z1 && !z1->Gesperrt && z2 && !z2->Gesperrt, "mehrere Kandidaten: jede Datei ein eigenes Spiel-Abbild");
    pruefe(K->SperrQuellen.n == 0, a_fmt(A0, "Sperrliste bleibt leer (da_Chris86 28.10.2026: Spiel-Abbilder sind löschbar): %d", K->SperrQuellen.n));
    /* vollständige Bereinigung ("M.2 behalten"): Kacheln nur als Kachel; Spiel-Abbilder auf der internen SSD als eigene Spiele */
    kern_waehle_komplett(K, false, false);
    eintrag *f1 = kachel("PPSA30528"), *d1 = mit_pfad(rdr), *d2 = mit_pfad(ovd);
    pruefe(f1 && f1->Wahl && f1->Pfade.n > 0 && !hat(f1, rdr) && d1 && d1->Wahl && d2 && d2->Wahl, "vollständige Bereinigung: Kachel nur als Kachel, die Spiel-Abbilder als eigene Spiele gewählt (1.0.6)");
    pruefe(md && md->Wahl && um && um->Wahl && ab && ab->Wahl && !hat(ab, "/mnt/usb3/homebrew/Abgezogen - PPSA77774.exfat"), "vollständige Bereinigung: Kacheln ohne erreichbare Datei gewählt (nur Kachel), geteilte Datei als eigenes Spiel");
    pruefe(weg && weg->Wahl, "vollständige Bereinigung: echte tote Kachel gewählt");
    /* Elternordner einer gesperrten Datei: Löschcode hält an (auch bei manuell gebautem Eintrag) */
    for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
    eintrag *ue = a_hole(A0, sizeof *ue); ue->Pfade = (sliste){0}; sl_add(A0, &ue->Pfade, "/user/data/homebrew/backports"); ue->PfadeBasis = ue->Pfade; ue->Kat = 6;
    ue->Text = ue->TextBasis = "Test: Elternordner"; ue->Quelle = ""; ue->Wahl = true;
    pl_add(K->a, &K->Funde, ue);
    ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
    printf("ERG Fehler=%d\n", e.Fehler);
    /* 1.0.6: es gibt keine gesperrten Spiel-Abbilder mehr - der Ordner wird ganz normal gelöscht */
    pruefe(e.Fehler == 0 && !da_w(argv[1], "/user/data/homebrew/backports/Zwei.exfat"), "Elternordner ohne Gesperrtes: wird gelöscht (1.0.6)");
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
