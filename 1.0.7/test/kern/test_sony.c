/* PS5 Cleanup 1.0.6 - Test Sonys Deinstallations-Dienst (da_Chris86 28.10.2026):
 * Kacheln gehen über Sony (keine Geisterkachel); bei "Spiel + Kachel" seit 1.0.7 (da_Chris86 01.11.2026) erst die Kachel, dann die Spieldatei;
 * bei "nur Kachel" bleibt die Spieldatei; lehnt Sony ab -> 1.0.6: Dateien weg, Eintrag "TEILWEISE", Kachel in GEISTER;
 * NEU 1.0.7 (da_Chris86 31.10.2026 23:32, keine Geisterkacheln): lehnt Sony ab oder geht Sony nicht -> die App bleibt VOLLSTÄNDIG
 * erhalten ("unverändert", keine Datei gelöscht); ohne Sony-Dienst (PC/Windows-Kern) alles wie bisher.
 * NEU 1.0.7 (PS5-Test 01.11.2026 20:14): Daten eines Tools stehen unter seiner Kachel und gehen nur, wenn die Kachel wirklich weg ist;
 * Fall "einbindung": ob an der Kachel etwas eingebunden ist, laesst sich nicht feststellen -> Kachel und Daten bleiben.
 *   test_sony <hier> <fall>   fall: ok | sim | ablehnen | nie | ohne | langsam | einbindung   (Umgebung UNINSTALL setzt lauf_sony.sh) */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool sl_enthaelt(const sliste *l, const char *s) { for (int i = 0; i < l->n; i++) if (strcmp(l->e[i], s) == 0) return true; return false; }
static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static arena *A0; static kern *K; static sliste LOG;
static void melde_log(void *c, const char *t) { (void)c; sl_add(A0, &LOG, t); printf("LOG: %s\n", t); }
static bool nein(void *c) { (void)c; return false; }
static const char *H;
static bool da_w(const char *p) { struct stat st; return lstat(VERB(A0, H, "/w", p), &st) == 0; }
static eintrag *mit_id(const char *id) { for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (strstr(f->Text, id) && !f->Gesperrt && (ist_teil(f, "Spiel") || f->KachelId)) return f; } return NULL; }
static int log_zeile(const char *a, const char *b) { for (int i = 0; i < LOG.n; i++) if (strstr(LOG.e[i], a) && (!b || strstr(LOG.e[i], b))) return i; return -1; }
static int buch_zeile(const sliste *bu, const char *a) { for (int i = 0; i < bu->n; i++) if (strstr(bu->e[i], a)) return i; return -1; }
/* NEU 1.0.7 (PS5-Test 01.11.2026): bei TOOL61111 laesst sich die Einbindung nicht feststellen (wie statfs-Fehler ohne Ersatz), sonst nichts eingebunden */
static int einbindung_unklar(arena *a, const char *id, const char **fs, const char **q) { (void)a; (void)fs; (void)q; return strcmp(id, "TOOL61111") ? 0 : -1; }
/* NEU 1.0.7 (PS5-Test 01.11.2026 23:13): kstuff hat den Spielordner der Kachel PPSA63333 eingebunden (eigenes) oder einen fremden Ordner (fremd) */
static const char *EINB_Q = NULL;
static int einbindung_ordner(arena *a, const char *id, const char **fs, const char **q) { (void)a; if (strcmp(id, "PPSA63333")) return 0; *fs = "nullfs"; *q = EINB_Q; return 1; }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 3) { fprintf(stderr, "Aufruf: test_sony <hier> <fall>\n"); return 2; }
    H = argv[1]; const char *fall = argv[2];
    A0 = arena_neu();
    printf("== Sony-Deinstallation: %s\n", fall);
    datei_optionen o = { .wurzel = VERB(A0, H, "/w"), .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    zugriff *z = zugriff_datei(&o);
    bool sim = !strcmp(fall, "sim");
    if (sim) z = zugriff_sim(z);
    K = kern_neu(z); K->melde = melde_log; K->simulation = sim;
    volatile int fe = 0;
    VERSUCHE { kern_selbstschutz_eigener_pfad(K, "/user/data/homebrew/PS5Cleanup"); kern_selbstschutz_ermitteln(K); kern_suche_funde(K); }
    FANGE { printf("FAIL Suche: %s\n", ausn_text); fe = 1; } ENDE_VERSUCHE;
    if (fe) { printf("FEHLERZAHL 1\n"); return 1; }
    eintrag *spiel = mit_id("PPSA61111"), *tool = mit_id("TOOL61111"), *smmit = mit_id("PPSA62222"), *smohne = mit_id("PPSA63333"), *fake = mit_id("FAKE00000");
    printf("INFO spiel=%s\nINFO tool=%s\nINFO smmit=%s\nINFO smohne=%s\nINFO fake=%s\n", spiel ? spiel->Text : "-", tool ? tool->Text : "-", smmit ? smmit->Text : "-", smohne ? smohne->Text : "-", fake ? fake->Text : "-");
    pruefe(spiel && spiel->KachelId && !strcmp(spiel->KachelOrdner, "/user/app/PPSA61111"), "normales Spiel hat eine Kachel für Sony (/user/app/PPSA61111)");
    pruefe(tool && tool->KachelId && !strcmp(tool->KachelId, "TOOL61111"), "Tool-Kachel hat eine Kachel für Sony");
    pruefe(smmit && smmit->KachelId && smmit->Quelle[0], "ShadowMount-Spiel (Spiel + Kachel) hat eine Kachel für Sony und eine Spieldatei");
    pruefe(smohne && smohne->KachelId, "ShadowMount-Spiel (nur Kachel) hat eine Kachel für Sony");
    pruefe(fake && fake->KachelId, "Homebrew Launcher FAKE00000 hat eine Kachel für Sony (1.0.6: PS5 Cleanup läuft nicht mehr darin)");
    if (!spiel || !tool || !smmit || !smohne || !fake) { printf("FEHLERZAHL %d\n", fehler); return 1; }
    /* NEU 1.0.7 (PS5-Test 01.11.2026 20:14): Daten des Tools stehen unter seiner Kachel */
    eintrag *td = NULL; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) if (!strcmp(f->PfadeBasis.e[j], "/user/data/Testtool")) td = f; }
    printf("INFO tooldaten=%s\n", td ? td->Text : "-");
    pruefe(td && td->Kat == 6 && td->ToolKachel == tool, "Daten des Tools (/user/data/Testtool) stehen unter seiner Kachel");
    /* NEU 1.0.7 (da_Chris86 01.11.2026 abends): auch die ELF des Tools auf dem USB-Stick steht unter der Kachel - nur einzeln waehlbar */
    eintrag *te = NULL, *fremd = NULL;
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) {
        if (!strcmp(f->PfadeBasis.e[j], "/mnt/usb0/Testtool_v1.2.elf")) te = f; if (!strcmp(f->PfadeBasis.e[j], "/mnt/usb0/homebrew/tool.elf")) fremd = f; } }
    printf("INFO tool-elf=%s | Zeile: %s\n", te ? te->Text : "-", te ? kern_zeile(A0, te) : "-");
    pruefe(te && te->Kat == 6 && te->ToolKachel == tool && te->Ort == ORT_STICK && te->NurEinzeln, "ELF des Tools auf dem USB-Stick (Testtool_v1.2.elf) steht unter seiner Kachel, nur einzeln");
    pruefe(te && !strcmp(kern_zeile(A0, te), "USB-Laufwerk – Testtool_v1.2.elf"), "Zeile der ELF: Ort einmal, ohne Strich davor");
    { int it = -1, id = -1, ie = -1; for (int i = 0; i < K->Funde.n; i++) { if (K->Funde.e[i] == tool) it = i; if (K->Funde.e[i] == td) id = i; if (K->Funde.e[i] == te) ie = i; }
      pruefe(it >= 0 && id == it + 1 && ie == it + 2, "Reihenfolge: Kachel, darunter Daten und ELF"); }
    pruefe(fremd && fremd->ToolKachel == NULL, "andere ELF (tool.elf, passt zu keiner Kachel) bleibt fuer sich");
    {   /* NEU 1.0.7 (da_Chris86 01.11.2026 23:22): Game Compressor und LZ4-Game-Compressor nicht verwechseln */
        eintrag *lz = mit_id("LZGC50001"), *gc = mit_id("PSGC50001"), *gd = NULL, *ge = NULL, *ad = NULL, *le = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; for (int j = 0; j < f->PfadeBasis.n; j++) { const char *p = f->PfadeBasis.e[j];
            if (!strcmp(p, "/user/data/GameCompressor")) gd = f; if (!strcmp(p, "/mnt/usb0/game-compressor_v1.0.4.elf")) ge = f;
            if (!strcmp(p, "/user/data/AMPRStudio")) ad = f; if (!strcmp(p, "/mnt/usb0/LZ4-Game-Compressor.elf")) le = f; } }
        printf("INFO lz4=%s gc=%s | GameCompressor->%s game-compressor.elf->%s AMPRStudio->%s LZ4.elf->%s\n", lz ? "ja" : "-", gc ? "ja" : "-",
               gd && gd->ToolKachel ? gd->ToolKachel->KachelId : "-", ge && ge->ToolKachel ? ge->ToolKachel->KachelId : "-",
               ad && ad->ToolKachel ? ad->ToolKachel->KachelId : "-", le && le->ToolKachel ? le->ToolKachel->KachelId : "-");
        pruefe(lz && gc && gd && ge && gd->ToolKachel == gc && ge->ToolKachel == gc, "Game Compressor: Daten und ELF unter Spielekompressor (PSGC50001), nicht unter LZ4");
        pruefe(lz && ad && le && ad->ToolKachel == lz && le->ToolKachel == lz, "LZ4-Game-Compressor: AMPRStudio und LZ4-Game-Compressor.elf unter LZ4 Game Compressor (LZGC50001)");
    }
    /* NEU 1.0.6: Jailbreak-Anzeige (da_Chris86 28.10.2026 23:16) und Jailbreak-Einstiege unter "Jailbreak & Homebrew" (22:55) */
    {
        const char *w = NULL, *jb = kern_jailbreak_erkannt(K, A0, &w);
        printf("INFO Jailbreak=%s | %s\n", jb ? jb : "-", w ? w : "-");
        pruefe(jb && strstr(jb, "Y2JB (YouTube)") && w && strstr(w, "YouTube (PPSA01650)") && strstr(w, "(unter „Jailbreak“)"), "Jailbreak-Anzeige: Y2JB (YouTube) erkannt, Warnung nennt den YouTube-Eintrag (unter „Jailbreak“, 1.0.7)");
        eintrag *yt = NULL; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (strstr(f->Text, "PPSA01650") && f->Jailbreak) yt = f; }
        pruefe(yt && yt->Kat == 7 && yt->NurEinzeln, "YouTube steht unter Jailbreak (Bereich 7, 1.0.7), nur einzeln");
        /* NEU 29.10.2026: Lua-Jailbreak-Spiel über ShadowMount bleibt unter "Spiele" (da_Chris86), geschützt + gewarnt */
        eintrag *rc = NULL; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (strstr(f->Text, "CUSA16074") && f->Quelle[0]) rc = f; }
        printf("INFO Lua-SM=%s\n", rc ? rc->Text : "-");
        pruefe(rc && rc->Jailbreak && rc->NurEinzeln && rc->Kat == 7 && rc->Quelle[0], "Lua-Spiel über ShadowMount: Jailbreak-Einstieg, nur einzeln, unter Jailbreak (1.0.7), Spieldatei-Regeln bleiben");
        pruefe(jb && strstr(jb, "Lua-Jailbreak") && w && strstr(w, "(CUSA16074)") && strstr(w, "unter „Jailbreak“"), "Konsole-Karte: Lua-Jailbreak erkannt, Warnung nennt das Spiel unter Jailbreak (1.0.7)");
        kern_waehle_komplett(K, false, false);
        pruefe(rc && !rc->Wahl, "vollständige Bereinigung hakt das Lua-Spiel (ShadowMount) nicht an");
        pruefe(yt && !yt->Wahl, "vollständige Bereinigung hakt YouTube/Y2JB nicht an");
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        kern_sm_zuruecksetzen(K);   /* die vollständige Bereinigung hat bei ShadowMount-Spielen die Spieldatei dazugenommen */
    }
    /* Codex-Prüfung 36: Spiel-Abbild eines Lua-Spiels ohne Kachel */
    {
        eintrag *ab = NULL; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (strstr(f->Text, "CUSA17068")) ab = f; }
        printf("INFO Lua-Abbild=%s\n", ab ? ab->Text : "-");
        pruefe(ab && ab->Jailbreak && ab->NurEinzeln && ab->Kat == 7, "Spiel-Abbild eines Lua-Spiels ohne Kachel: Jailbreak-Einstieg, nur einzeln, unter Jailbreak (1.0.7)");
        kern_waehle_komplett(K, false, false);
        pruefe(ab && !ab->Wahl, "vollständige Bereinigung hakt das Lua-Spiel-Abbild nicht an");
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        kern_sm_zuruecksetzen(K);
    }
    /* NEU 30.10.2026 (da_Chris86 06:58): ShadowMount-Spiel mit Spieldatei auf dem USB-Stick - die interne Kachel geht über "ganzen
     * Bereich" und die vollständige Bereinigung mit ("nur Kachel"), die Spieldatei auf dem Stick nie automatisch */
    {
        eintrag *vc = NULL; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (strstr(f->Text, "PPSA63530") && f->Quelle[0]) vc = f; }
        printf("INFO Stick-SM=%s\n", vc ? vc->Text : "-");
        pruefe(vc && vc->Ort == ORT_STICK && !vc->NurEinzeln && vc->Kat == 1, "ShadowMount-Spiel mit Spieldatei auf dem USB-Stick: Kachel normal wählbar (nicht nur einzeln)");
        kern_waehle_komplett(K, false, false);
        bool stick_drin = false; if (vc) for (int j = 0; j < vc->Pfade.n; j++) if (strstr(vc->Pfade.e[j], "/mnt/usb0")) stick_drin = true;
        pruefe(vc && vc->Wahl && !stick_drin && strstr(vc->Text, "nur Kachel"), "vollständige Bereinigung: Kachel ja, Spieldatei auf dem Stick nicht (\"nur Kachel, Spiel bleibt\")");
        for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
        kern_sm_zuruecksetzen(K);
        int nst = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Ort == ORT_STICK && !f->Quelle[0] && !f->Gesperrt) nst++; }
        int nst_einzeln = 0; for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (f->Ort == ORT_STICK && !f->Quelle[0] && !f->Gesperrt && f->NurEinzeln) nst_einzeln++; }
        pruefe(nst == nst_einzeln, "übrige Einträge auf dem USB-Stick bleiben nur einzeln wählbar");
        printf("INFO Stick-Einträge ohne Kachel: %d (nur einzeln: %d)\n", nst, nst_einzeln);
        /* NEU 30.10.2026 (da_Chris86 08:10): Spiel-Abbild ohne Kachel im Ordner homebrew auf dem Stick - Spiel unter "Spiele",
         * der Ordner wird aufgeklappt (nicht als Ganzes unter Jailbreak & Homebrew), die ELF daneben einzeln unter Bereich 6 */
        eintrag *ab = NULL, *ganz = NULL, *elf = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i];
            for (int j = 0; j < f->Pfade.n; j++) {
                if (!strcmp(f->Pfade.e[j], "/mnt/usb0/homebrew/PPSA03530.exfat")) ab = f;
                if (!strcmp(f->Pfade.e[j], "/mnt/usb0/homebrew")) ganz = f;
                if (!strcmp(f->Pfade.e[j], "/mnt/usb0/homebrew/tool.elf")) elf = f; } }
        printf("INFO Stick-Abbild=%s\n", ab ? ab->Text : "-");
        pruefe(ab && ab->Kat == 1 && ab->Ort == ORT_STICK && ab->NurEinzeln && strstr(ab->Text, "ohne Kachel"), "Spiel-Abbild ohne Kachel im Ordner homebrew auf dem Stick: unter Spiele, nur einzeln");
        pruefe(ganz == NULL, "Ordner homebrew auf dem Stick wird aufgeklappt, nicht als Ganzes angeboten");
        pruefe(elf && elf->Kat == 6, "ELF daneben einzeln unter Apps (Bereich 6, 1.0.7)");
        /* Backups: Abbild eines Einstiegs direkt im Ordner -> aufgeklappt, das Abbild selbst geschützt */
        eintrag *jk = NULL, *bg = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i];
            for (int j = 0; j < f->Pfade.n; j++) {
                if (!strcmp(f->Pfade.e[j], "/user/data/homebrew/Backups/Jinki Resurrection - CUSA25179.exfat")) jk = f;
                if (!strcmp(f->Pfade.e[j], "/user/data/homebrew/Backups")) bg = f; } }
        pruefe(bg == NULL && jk && jk->Kat == 7 && jk->Jailbreak && jk->NurEinzeln, "Ordner mit Abbild eines Einstiegs direkt darin: aufgeklappt, Abbild geschützt (unter Jailbreak, nur einzeln, 1.0.7)");
    }
    /* Codex-Prüfung 37: gemeinsame Spieldatei einer Einstiegs- und einer normalen Kachel; Abbild im Unterordner */
    {
        eintrag *gm = NULL, *bk = NULL;
        for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (strstr(f->Text, "gemeinsam.exfat")) gm = f; if (strstr(f->Text, "Archiv") && !f->Gesperrt) bk = f; }   /* 30.10.2026: Archiv statt Backups (siehe lauf_sony.sh) */
        printf("INFO gemeinsam=%s\nINFO Archiv=%s\n", gm ? gm->Text : "-", bk ? bk->Text : "-");
        pruefe(gm && gm->Jailbreak && gm->NurEinzeln && gm->Kat == 7, "gemeinsame Spieldatei (auch Kachel eines Lua-Spiels): Jailbreak-Einstieg, nur einzeln, unter Jailbreak (1.0.7)");
        if (gm) pruefe(!strcmp(kern_jb_weg_eintrag(gm), "Lua-Jailbreak"), "gemeinsame Spieldatei gehört zum Lua-Jailbreak (Kennung aus dem Namen)");
        if (!strcmp(fall, "jbordner")) {
            pruefe(bk != NULL, "Ordner Archiv (Abbild eine Ebene tiefer) wird als Ganzes angeboten");
            if (bk) {
                for (int i = 0; i < K->Funde.n; i++) ((eintrag *)K->Funde.e[i])->Wahl = false;
                bk->Wahl = true;
                ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
                pruefe(e.Fehler >= 1 && log_zeile("SICHERHEITSSTOPP", "CUSA25179") >= 0, "Ordner mit Abbild eines Einstiegs: Sicherheitsstopp, nichts gelöscht");
                pruefe(da_w("/user/data/homebrew/Archiv/alt/Jinki Resurrection - CUSA25179.exfat") && da_w("/user/data/homebrew/Archiv/notiz.txt"), "Abbild und Ordner noch da");
            }
            printf("FEHLERZAHL %d\n", fehler);
            return fehler ? 1 : 0;
        }
    }
    if (!strcmp(fall, "selbstjb2")) {
        int f2 = kern_selbst_entfernen(K, false, false);
        printf("INFO Selbstentfernung2=%d\n", f2);
        pruefe(f2 >= 2 && log_zeile("BLEIBT", "/user/data/homebrew/PS5Cleanup") >= 0 && da_w("/user/data/homebrew/PS5Cleanup/mika.exfat"), "Selbstentfernung: Spieldatei ohne Kennung eines Lua-Spiels im eigenen Ordner bleibt");
        pruefe(log_zeile("BLEIBT", "/user/app/PSCL00001") >= 0 && log_zeile("Kachel PSCL00001", "(über Sony)") < 0 && da_w("/user/app/PSCL00001/Raspberry - CUSA16074.exfat"), "Selbstentfernung: eigener App-Ordner mit Abbild eines Einstiegs - nie über Sony, bleibt");
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    if (!strcmp(fall, "selbstjb")) {
        int f2 = kern_selbst_entfernen(K, false, false);
        printf("INFO Selbstentfernung=%d\n", f2);
        pruefe(f2 >= 1 && log_zeile("BLEIBT", "Lua-Jailbreak") >= 0, "Selbstentfernung: Ordner mit Lua-Spiel-Abbild bleibt, mit Grund (kein \"ganz entfernt\")");
        pruefe(da_w("/user/data/homebrew/PS5Cleanup/Fuyu Kiss - CUSA29745.exfat"), "Selbstentfernung: Lua-Spiel-Abbild im Ordner von PS5 Cleanup ist noch da");
        printf("FEHLERZAHL %d\n", fehler);
        return fehler ? 1 : 0;
    }
    const char *mitbild = smmit->Quelle, *ohnebild = smohne->Quelle;
    /* Auswahl: Spiel (+ Update), Tool, SM mit Spieldatei (Spieldatei vorne, wie kern_setze_sm_spieldateien), SM nur Kachel, Launcher */
    spiel->Wahl = tool->Wahl = smmit->Wahl = smohne->Wahl = true;
    eintrag *schloss = NULL, *geschuetzt = NULL;
    for (int i = 0; i < K->Funde.n; i++) { eintrag *f = K->Funde.e[i]; if (strstr(f->Text, "PPSA64444")) schloss = f; if (strstr(f->Text, "PPSA65555") && ist_teil(f, "Spiel")) geschuetzt = f; }
    printf("INFO schloss=%s\nINFO geschuetzt=%s\n", schloss ? schloss->Text : "-", geschuetzt ? geschuetzt->Text : "-");
    if (!strcmp(fall, "nie") || !strcmp(fall, "ok") || !strcmp(fall, "sim")) { fake->Wahl = true; if (schloss) schloss->Wahl = true; if (geschuetzt) geschuetzt->Wahl = true; }
    if (!strcmp(fall, "ablehnen") && schloss) schloss->Wahl = true;   /* NEU 1.0.6: leere Kachel - Sony lehnt ab -> "Entfernung unvollständig" */
    if (!strcmp(fall, "langsam")) { tool->Wahl = smmit->Wahl = smohne->Wahl = false; }
    if (td) td->Wahl = tool->Wahl;   /* NEU 1.0.7: Kachel und Daten angehakt */
    if (!strcmp(fall, "einbindung")) { zugriff_einbindung = einbindung_unklar; if (te) te->Wahl = true; }   /* dazu die ELF auf dem Stick */
    if (!strcmp(fall, "eigenes") || !strcmp(fall, "fremd")) {
        EINB_Q = !strcmp(fall, "eigenes") ? "/data/homebrew/SM Ohne - PPSA63333.exfat/" : "/mnt/ext1/homebrew/Anderes Spiel - PPSA99999";
        zugriff_einbindung = einbindung_ordner;
    }
    { sliste p = {0}; sl_add(A0, &p, mitbild); sl_add_alle(A0, &p, &smmit->PfadeBasis); smmit->Pfade = p; }
    kern_abhaengige_mitnehmen(K);
    K->RM_ERFASSEN = true; K->RM_FEHLER_N = 0;   /* NEU 1.0.7: Fehler-Kürzel für die QR-Rückmeldung wie im Ablauf erfassen */
    ergebnis e = kern_fuehre_loeschung(K, nein, NULL);
    K->RM_ERFASSEN = false;
    zugriff_einbindung = NULL;
    printf("ERG Fehler=%d Teilweise=%d Unveraendert=%d SONY=%d GEISTER=%s\n", e.Fehler, e.Teilweise, e.Unveraendert, K->SONY, sl_join(A0, &K->GEISTER, " | "));
    if (!strcmp(fall, "ohne")) {
        pruefe(!K->SONY, "ohne Sony-Dienst: K->SONY aus");
        pruefe(e.Fehler == 0 && e.Teilweise == 0 && K->GEISTER.n == 0, "ohne Sony-Dienst: keine Fehler, nichts teilweise");
        pruefe(log_zeile("über Sony", NULL) < 0 && log_zeile("TEILWEISE", NULL) < 0, "ohne Sony-Dienst: Protokoll wie bisher (kein Sony, kein TEILWEISE)");
        pruefe(!da_w("/user/app/PPSA61111") && !da_w("/user/app/TOOL61111") && !da_w(mitbild) && da_w(ohnebild), "ohne Sony-Dienst: Dateien wie bisher gelöscht, Spieldatei bei \"nur Kachel\" bleibt");
        pruefe(!da_w("/user/data/Testtool"), "ohne Sony-Dienst: Tool-Kachel weg, ihre Daten auch");
    } else if (!strcmp(fall, "ok") || !strcmp(fall, "sim")) {
        pruefe(K->SONY, "Sony-Dienst erkannt");
        /* 1.0.7: statt Sicherheitsstopp (Fehler) bleibt die App ganz - sie hat eine Kachel, die nicht über Sony gehen kann */
        pruefe(e.Fehler == 0 && e.Unveraendert == 1 && log_zeile("unverändert", "PPSA65555") >= 0 && log_zeile("PPSA65555", "(Kachel über Sony entfernt)") < 0
               && da_w("/user/app/PPSA65555") && da_w("/user/appmeta/PPSA65555"),
               "App-Ordner mit Geschütztem: Sony wird NICHT beauftragt, die App bleibt vollständig erhalten (Codex 32, 1.0.7: keine Geisterkachel)");
        /* NEU 1.0.6 (da_Chris86 29.10.2026 21:25): keine neuen Geisterkacheln - Launcher und leere Kachel (ohne App-Ordner) über Sony */
        pruefe(e.Teilweise == 0 && K->GEISTER.n == 0, "keine Kachel bleibt im Menü (keine Geisterkacheln)");
        pruefe(log_zeile("FAKE00000", "(Kachel über Sony entfernt)") >= 0, "Homebrew Launcher FAKE00000: über Sony entfernt");
        pruefe(log_zeile("PPSA64444", "(Kachel über Sony entfernt)") >= 0, "leere Kachel (Schloss) ohne App-Ordner: über Sony mit der Kennung entfernt");
        /* NEU 03.11.2026 (da_Chris86 17:50, PS5-Test: 9 leere Einhängeordner blieben): Kachel über Sony weg -> leerer Einhängeordner auch bei "nur Kachel" */
        if (!strcmp(fall, "ok")) {
            pruefe(!da_w("/system_ex/app/PPSA63333") && da_w(ohnebild), "nur Kachel über Sony: leerer Einhängeordner weg, Spieldatei bleibt");
            pruefe(!da_w("/system_ex/app/PPSA62222"), "Spiel + Kachel: leerer Einhängeordner weg");
        } else pruefe(da_w("/system_ex/app/PPSA63333"), "Simulation: Einhängeordner bleibt");
        pruefe(log_zeile("PPSA61111", "(Kachel über Sony entfernt)") >= 0, "Spiel: über Sony entfernt");
        pruefe(log_zeile("TOOL61111", "(Kachel über Sony entfernt)") >= 0, "Tool-Kachel: über Sony entfernt");
        pruefe(log_zeile("Testtool", sim ? "würde gelöscht" : "gelöscht") >= 0 && log_zeile("die Kachel bleibt", NULL) < 0 && (sim ? da_w("/user/data/Testtool") : !da_w("/user/data/Testtool")),
               "Daten des Tools: mit der Kachel gelöscht (1.0.7)");
        pruefe(log_zeile("PPSA62222", "(Kachel über Sony entfernt)") >= 0 && log_zeile("PPSA63333", "(Kachel über Sony entfernt)") >= 0, "beide ShadowMount-Kacheln über Sony entfernt");
        pruefe(log_zeile("Update", sim ? "würde gelöscht" : "gelöscht") >= 0, "Update des Spiels ist mit weg");

        if (sim) {
            const sliste *bu = zugriff_sim_buch(z);
            int b1 = buch_zeile(bu, mitbild), u1 = buch_zeile(bu, "SITE UNINSTALL /user/app/PPSA62222");
            pruefe(u1 >= 0 && b1 > u1, "Spiel + Kachel: erst die Kachel über Sony, dann die Spieldatei (Reihenfolge im Buch, da_Chris86 01.11.2026)");
            pruefe(buch_zeile(bu, ohnebild) < 0 && buch_zeile(bu, "SITE UNINSTALL /user/app/PPSA63333") >= 0, "nur Kachel: Spieldatei unangetastet, Kachel über Sony");
            pruefe(buch_zeile(bu, "SITE UNINSTALL /user/app/FAKE00000") >= 0 && buch_zeile(bu, "SITE UNINSTALL /user/app/PPSA64444") >= 0, "Launcher und leere Kachel werden Sony übergeben");
            pruefe(buch_zeile(bu, "SITE UNINSTALL /user/app/PPSA65555") < 0, "Ordner mit Geschütztem wird Sony nie übergeben (Codex 32)");
            pruefe(da_w("/user/app/PPSA61111") && da_w(mitbild), "Simulation: auf der Platte ist nichts verändert");
        } else {
            pruefe(!da_w("/user/app/PPSA61111") && !da_w("/user/appmeta/PPSA61111") && !da_w("/user/patch/PPSA61111"), "Spiel samt Kachel-Infos und Update weg");
            pruefe(!da_w(mitbild) && da_w(ohnebild), "Spieldatei nur bei \"Spiel + Kachel\" gelöscht, bei \"nur Kachel\" bleibt sie");
            pruefe(!da_w("/user/app/FAKE00000") && !da_w("/system_data/priv/appmeta/PPSA64444"), "Launcher und leere Kachel: Dateien weg (nach Sony)");
            /* eigene Kachel über Sony */
            int f2 = kern_selbst_entfernen(K, false, false);
            pruefe(f2 == 0 && log_zeile("Kachel PSCL00001", "(über Sony)") >= 0 && !da_w("/user/app/PSCL00001"), "Selbstentfernung: eigene Kachel PSCL00001 über Sony");
        }
    } else if (!strcmp(fall, "ablehnen")) {
        /* NEU 1.0.7 (da_Chris86 31.10.2026 23:32): Sony lehnt ab -> keine Geisterkachel: nichts wird gelöscht - seit 01.11.2026 auch bei
         * "Spiel + Kachel" (ShadowMount): erst die Kachel über Sony, die Spieldatei zuletzt - lehnt Sony ab, bleibt auch die Spieldatei. */
        pruefe(K->SONY && e.Fehler == 0, "Sony-Dienst da, keine Fehler");
        {   /* NEU 1.0.7 (Codex-Prüfung 69): Sonys Ablehnung landet als Kürzel in der QR-Rückmeldung - ohne Namen, Kennung oder Pfad */
            bool sauber = K->RM_FEHLER_N >= 1;
            for (int i = 0; i < K->RM_FEHLER_N; i++) {
                printf("     QR-Fehler %d: %s\n", i + 1, K->RM_FEHLER[i]);
                if (strncmp(K->RM_FEHLER[i], "sony.", 5) || !strstr(K->RM_FEHLER[i], ".0x80990001") || strchr(K->RM_FEHLER[i], '/') || strstr(K->RM_FEHLER[i], "PPSA")) sauber = false;
            }
            pruefe(sauber && K->RM_FEHLER_N <= 5, "QR-Rückmeldung: Sonys Ablehnung als \"sony.<art>.<ort>.0x80990001\" (höchstens 5, keine Kennung, kein Pfad)");
        }
        pruefe(e.Unveraendert == 7 && e.Teilweise == 0 && K->GEISTER.n == 0,
               "Sony lehnt ab: Spiel, sein Update, Tool, Daten des Tools, \"Spiel + Kachel\", \"nur Kachel\" und leere Kachel unverändert - nichts teilweise, keine Geisterkachel");
        pruefe(log_zeile("die Kachel bleibt - darum bleiben auch ihre Daten", NULL) >= 0 && da_w("/user/data/Testtool/einstellungen.json"),
               "Daten des Tools: die Kachel bleibt -> die Daten bleiben auch (1.0.7, PS5-Test 01.11.2026)");
        pruefe(log_zeile("unverändert", "PPSA64444") >= 0 && da_w("/system_data/priv/appmeta/PPSA64444"), "leere Kachel ohne App-Ordner: Sony lehnt ab -> nichts gelöscht");
        pruefe(log_zeile("unverändert", "PPSA61111") >= 0 && log_zeile("unverändert", "Update") >= 0 && log_zeile("gelöscht: /user/app/PPSA61111", NULL) < 0,
               "Spiel: unverändert mit Grund, sein Update bleibt auch");
        pruefe(da_w("/user/app/PPSA61111") && da_w("/user/appmeta/PPSA61111") && da_w("/user/patch/PPSA61111") && da_w("/user/app/TOOL61111")
               && da_w("/user/app/PPSA63333") && da_w(ohnebild), "Spiel, Update, Tool und \"nur Kachel\": alle Dateien noch da");
        pruefe(da_w(mitbild) && da_w("/user/app/PPSA62222") && log_zeile("unverändert", "PPSA62222") >= 0 && log_zeile("TEILWEISE", "PPSA62222") < 0,
               "\"Spiel + Kachel\": Sony lehnt ab -> Spieldatei und Kachel samt App-Ordner bleiben (erst Kachel, dann Spieldatei - da_Chris86 01.11.2026)");
    } else if (!strcmp(fall, "einbindung")) {
        /* NEU 1.0.7 (PS5-Test 01.11.2026 20:14): Einbindung nicht feststellbar -> Kachel bleibt mit Grund, ihre Daten bleiben, der Rest geht normal */
        pruefe(e.Fehler == 0 && e.Teilweise == 0 && e.Unveraendert == 3 && K->GEISTER.n == 0, "Einbindung unklar: Tool, seine Daten und seine ELF unverändert, sonst nichts, keine Geisterkachel");
        pruefe(da_w("/mnt/usb0/Testtool_v1.2.elf"), "ELF auf dem USB-Stick bleibt mit der Kachel");
        pruefe(log_zeile("unverändert", "TOOL61111") >= 0 && log_zeile("TOOL61111", "nicht feststellen") >= 0, "Tool-Kachel: unverändert mit Grund (Einbindung nicht feststellbar)");
        pruefe(log_zeile("die Kachel bleibt - darum bleiben auch ihre Daten", NULL) >= 0 && da_w("/user/app/TOOL61111") && da_w("/user/data/Testtool/einstellungen.json"),
               "Daten des Tools bleiben mit der Kachel - keine halbe Löschung");
        pruefe(log_zeile("PPSA61111", "(Kachel über Sony entfernt)") >= 0 && !da_w("/user/app/PPSA61111"), "andere Kacheln gehen normal über Sony");
    } else if (!strcmp(fall, "eigenes")) {
        pruefe(e.Fehler == 0 && e.Teilweise == 0 && e.Unveraendert == 0 && K->GEISTER.n == 0, "Einbindung des eigenen Spielordners: nichts bleibt unverändert, keine Geisterkachel");
        pruefe(log_zeile("PPSA63333", "eigenen Spielordners") >= 0 && log_zeile("PPSA63333", "(Kachel über Sony entfernt)") >= 0, "Kachel PPSA63333: Einbindung wird gelöst, dann über Sony entfernt");
        pruefe(da_w(ohnebild), "nur Kachel: der Spielordner bzw. die Spieldatei bleibt");
    } else if (!strcmp(fall, "fremd")) {
        pruefe(e.Fehler == 0 && e.Teilweise == 0 && e.Unveraendert == 1 && log_zeile("unverändert", "PPSA63333") >= 0 && da_w("/user/app/PPSA63333"),
               "Einbindung eines fremden Ordners: Kachel bleibt unverändert, nichts gelöscht");
    } else if (!strcmp(fall, "langsam")) {
        pruefe(e.Teilweise == 2 && log_zeile("übrig:", "60 s") >= 0 && log_zeile("übrig:", "Sony löscht das Spiel noch") >= 0 && da_w("/user/patch/PPSA61111"),
               "Sony nimmt an, wird aber nicht fertig: Spiel und sein Update TEILWEISE mit Grund, Update nicht selbst gelöscht (Codex 33)");
        pruefe(da_w("/user/app/PPSA61111"), "danach NICHT selbst weitergelöscht (kein Wettlauf mit Sony, Codex 32)");
    }
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
