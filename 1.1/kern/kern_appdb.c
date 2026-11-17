/* PS5 Cleanup - Datenbank-Übersicht: wie Sonys Titel-Liste (app.db) zu dem passt, was auf der Konsole liegt
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * NEU 1.0.10 (da_Chris86 10.11.2026 17:14/17:30, doku/ANALYSE_1.0.10.md Punkt 1 und PLAN_1.0.10_DATENBANK_ERKENNUNG.md Abschnitt 7):
 * Meldungen wie #3 (FW 9.00, 22 Fehler "ohne Code") zeigen nicht, was in Sonys Liste steht. Darum nach jeder Suche je Titel aus app.db
 * eine Einordnung ins Protokoll und als Zähler in die Rückmeldung. NUR LESEN - hier wird nichts verändert und nichts entschieden, was
 * gelöscht wird. Sony wird nur gefragt, ob es den Titel führt (sceAppInstUtilAppExists, wie beim PS4-Inventar).
 *
 * Einordnung (Reihenfolge = Vorrang):
 *   s  System (NPXS..., uninstallable 0) - nur gezählt
 *   p  Platzhalter: contentLocation 0 = nicht installiert (Download, gekauft, Spielebibliothek) - Sony verwaltet ihn. Ein fehlender
 *      Ordner ist hier normal (Beleg: PPSA01650 auf da_Chris86s PS5, Download mit download0.dat, 10.11.2026)
 *   i  installiert: PS5 Cleanup hat die Installation gefunden (bis 12.11.2026 auch: "Sony führt den Titel" - siehe k)
 *   x  auf erweitertem Speicher (hddLocation / externalHddAppStatus nicht 0), aber kein solches Laufwerk angeschlossen
 *   w  widersprüchlich: dieselbe Spielgruppe (localConceptId) gehört noch anderen Titeln
 *   v  ohne Installation: Sony führt ihn nicht, keine Installation gefunden, Speicher erreichbar - nur noch ein Eintrag in Sonys Liste
 *      (Kandidat für die spätere Austragung - die gibt es hier NICHT)
 *   k  Kachel ohne Dateien (NEU 1.0.10, da_Chris86 12.11.2026 20:06, TabShifter TBSH00001): Sony führt den Titel noch (Kachel im Menü),
 *      app.db sagt "installiert, interne SSD", auf keinem Speicher liegt etwas dazu. Bis dahin zählte "Sony führt den Titel" allein als
 *      installiert - so stand TBSH00001 (Ordner weg, Kachel im Menü) im Protokoll vom 12.11.2026 19:42 unter "installiert 20" und wurde nicht
 *      angeboten. Ob sie angeboten wird, entscheidet die Suche mit weiteren Belegen (kern_suche.c kachel_ohne_dateien).
 *   u  unklar: Sony ließ sich nicht fragen, Angaben fehlen oder unbekannter Speicherort-Wert
 */
#include "kern.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define A (k->a)

/* Wert eines Kürzels aus "u=..;l=..;...;n=Name" ("n" ist immer das letzte und darf ';' enthalten). "" = fehlt */
static const char *wert_von(arena *a, const char *w, char kz)
{
    if (!w) return "";
    if (kz == 'n') { const char *p = strstr(w, ";n="); return p ? p + 3 : !strncmp(w, "n=", 2) ? w + 2 : ""; }
    for (const char *p = w; *p; ) {
        if (p[0] == kz && p[1] == '=') { const char *e = strchr(p + 2, ';'); return e ? a_kopie_n(a, p + 2, (size_t)(e - p - 2)) : a_kopie(a, p + 2); }
        if (p[0] == 'n' && p[1] == '=') break;
        const char *e = strchr(p, ';'); if (!e) break; p = e + 1;
    }
    return "";
}
static bool null_wert(const char *v) { return !strcmp(v, "0"); }
static bool bekannt(const char *v) { return v[0] && strcmp(v, "?") != 0; }

char kern_appdb_einordnen(arena *a, const char *id, const char *w, int sony, bool installiert, bool ext_da, const char **text)
{
    const char *u = wert_von(a, w, 'u'), *l = wert_von(a, w, 'l'), *h = wert_von(a, w, 'h'), *x = wert_von(a, w, 'x'), *g = wert_von(a, w, 'g');
    *text = "";
    if ((id && !strncmp(id, "NPXS", 4)) || null_wert(u)) { *text = T("System", "system"); return 's'; }
    if (null_wert(l)) { *text = T("nicht installiert – Sonys Platzhalter (Download, Kauf oder Spielebibliothek), verwaltet Sony selbst",
                                  "not installed – Sony's placeholder (download, purchase or game library), managed by Sony"); return 'p'; }
    if (installiert) { *text = T("installiert", "installed"); return 'i'; }
    bool ext = (bekannt(h) && !null_wert(h)) || (bekannt(x) && !null_wert(x));
    if (ext && !ext_da) { *text = T("auf erweitertem Speicher eingetragen – kein solches Laufwerk angeschlossen, bleibt unangetastet",
                                    "registered on extended storage – no such drive connected, left untouched"); return 'x'; }
    if (bekannt(g) && !null_wert(g)) { *text = a_fmt(a, T("widersprüchlich – die Spielgruppe gehört noch %s anderen Titel(n)", "contradictory – the game group also belongs to %s other title(s)"), g); return 'w'; }
    if (!bekannt(u) || !bekannt(l)) { *text = T("unklar – Angaben in app.db unvollständig", "unclear – entries in app.db incomplete"); return 'u'; }
    /* ohne Angabe zum Speicherort (ältere Firmware ohne diese Spalten) lässt sich ein externer Titel nicht ausschließen */
    if (!bekannt(h) || !bekannt(x)) { *text = T("unklar – app.db nennt den Speicherort nicht (vielleicht auf erweitertem Speicher)", "unclear – app.db does not name the storage (maybe on extended storage)"); return 'u'; }
    if (strcmp(l, "2") != 0 && !ext) { *text = a_fmt(a, T("unklar – unbekannter Speicherort-Wert %s", "unclear – unknown storage value %s"), l); return 'u'; }
    if (sony == 0) { *text = T("ohne Installation – Sony führt den Titel nicht mehr, nur noch ein Eintrag in Sonys Liste",
                               "without installation – Sony no longer lists the title, only an entry in Sony's list is left"); return 'v'; }
    if (sony == 1 && ext) { *text = T("installiert", "installed"); return 'i'; }   /* Codex-Prüfung 169 (6): erweiterter Speicher - wie bis 12.11.2026 */
    if (sony == 1) { *text = T("Kachel ohne Dateien – Sony führt den Titel (Kachel im Menü), auf keinem Speicher liegt etwas dazu",
                               "tile without files – Sony lists the title (tile in the menu), nothing for it is on any storage"); return 'k'; }
    *text = T("unklar – Sony ließ sich nicht fragen, keine Installation gefunden", "unclear – Sony could not be asked, no installation found");
    return 'u';
}

void kern_appdb_uebersicht(kern *k)
{
    k->APPDB_ZEILEN = (sliste){0}; k->APPDB_RM[0] = 0;
    if (k->APPDB.n == 0) return;
    volatile bool ext_da = false;
    VERSUCHE { ext_da = s_existiert(k->S, M2 "/user") || s_existiert(k->S, USB_ERW "/user"); }
    FANGE { if (ausn_art == AUSN_ABBRUCH) WEITERWERFEN(); ext_da = false; } ENDE_VERSUCHE;
    static const char REIHE[] = "ipxwvkus";
    int anz[8] = { 0 };
    k->APPDB_KACHEL = mt_neu(true);
    sliste zeilen = {0};
    for (int i = 0; i < k->APPDB.n; i++) {
        const char *id = k->APPDB.e[i].k, *w = k->APPDB.e[i].v;
        bool inst = mt_hat(&k->INSTALLIERT, id);
        for (int j = 0; !inst && j < k->PS4.n; j++) { const ps4_spiel *s = k->PS4.e[j]; if (!strcmp(s->id, id) && kern_ps4_installiert(s)) inst = true; }
        int sony = -1;
        /* nur fragen, wenn es etwas entscheidet: System, Platzhalter und gefundene Installationen brauchen Sony nicht */
        const char *tx;
        char vor = kern_appdb_einordnen(A, id, w, -1, inst, ext_da, &tx);
        if (vor == 'u' && kern_ps4_sony_eingetragen) sony = kern_ps4_sony_eingetragen(id);
        char e = sony == -1 ? vor : kern_appdb_einordnen(A, id, w, sony, inst, ext_da, &tx);
        const char *r = strchr(REIHE, e); if (r) anz[r - REIHE]++;
        if (e == 'k') mt_setze(A, &k->APPDB_KACHEL, id, (void *)1);
        if (e == 's' || e == 'i') continue;
        const char *n = wert_von(A, w, 'n');
        sl_add(A, &zeilen, VERB(A, "   ", id, n[0] ? VERB(A, "  ", n) : "", " – ", tx));
    }
    int ges = 0; for (int i = 0; i < 8; i++) ges += anz[i];
    sl_add(A, &k->APPDB_ZEILEN, a_fmt(A, T("Sonys Titel-Liste (app.db, nur gelesen): %d Titel – installiert %d, Platzhalter %d, Laufwerk fehlt %d, widersprüchlich %d, ohne Installation %d, Kachel ohne Dateien %d, unklar %d, System %d",
                                          "Sony's title list (app.db, read only): %d titles – installed %d, placeholder %d, drive missing %d, contradictory %d, without installation %d, tile without files %d, unclear %d, system %d"),
                                      ges, anz[0], anz[1], anz[2], anz[3], anz[4], anz[5], anz[6], anz[7]));
    for (int i = 0; i < zeilen.n; i++) sl_add(A, &k->APPDB_ZEILEN, zeilen.e[i]);
    /* Rückmeldung wie bisher "i.p.x.w.v.u" (der Empfänger kennt nur diese Kürzel): "Kachel ohne Dateien" zählt dort bei v (nur noch Sonys Eintrag) */
    static const char RM[] = "ipxwvu";
    int rm[6] = { anz[0], anz[1], anz[2], anz[3], anz[4] + anz[5], anz[6] };
    size_t l = 0;
    for (int i = 0; i < 6; i++) if (rm[i] || i == 0) l += (size_t)snprintf(k->APPDB_RM + l, sizeof k->APPDB_RM - l, "%s%c%d", l ? "." : "", RM[i], rm[i] > 9999 ? 9999 : rm[i]);
}
