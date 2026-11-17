/* PS5 Cleanup - Bedienung im Browser
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * fern_tv.h - NEU 1.1 (da_Chris86 15.11.2026 20:17: "Ich möchte, dass die Browsersteuerung genauso funktioniert und genauso ausschaut
 * wie auf der App"; 20:26: "1.0.10 war auch ein Abbild von der App"): was der Fernseher gerade zeigt, als Daten für die Browserseite.
 * Die Seite (fern_seite.html) baut daraus dasselbe Bild in HTML (kein Fernsehbild, kein Steuerkreuz - da_Chris86 08.11.2026) und schickt
 * dieselben Tasten bzw. Klicks zurück (fd_befehl: "taste", "klick", ...). Die Texte kommen aus denselben Funktionen wie am Fernseher.
 * Wird von app.c eingebunden (vor fd_zustand_json); alles static.
 */

/* welche Seite der Fernseher zeigt (wie zeichnen()) */
static const char *fd_schirm(bool *dialog)
{
    *dialog = false;
    if (RM.offen && G.art != D_FERTIG && !G.beschaeftigt) return "rm";
    if (G.art == D_FERTIG) return RM.offen ? "rm" : "fertig";
    if (KB.auto_an && G.art != D_KEINER && !G.beantwortet && (G.art == D_TEXT || G.art == D_WAHL)) return "arbeit";
    if (komplett_frage()) return "komplett";
    if (bestaetigen_aktiv()) return "best";
    if (G.art == D_WAHL || G.art == D_FEHLER) return "frage";
    *dialog = G.art != D_KEINER;
    if (G.beschaeftigt) return loeschseite ? "loeschen" : (auftrag_jetzt == AUF_SUCHEN && !nach_loeschen) ? "suche" : "arbeit";
    if (GESUCHT && AUSWAHL_OFFEN && ablauf_verbunden(&A)) return "liste";
    return GESUCHT && ablauf_verbunden(&A) ? "nachher" : "start";
}
static void fj_paar(cJSON *a, const char *k, const char *v) { cJSON *z = cJSON_CreateObject(); fj_s(z, "k", k); fj_s(z, "v", v); cJSON_AddItemToArray(a, z); }

/* drei Zeilen oben rechts auf der Startseite (start_oben11): art 0 Glas, 1 grün, 2 gold, 3 noch offen; sym; bild 0-5 (Konsole) oder -1 */
static void fd_chips(cJSON *t)
{
    cJSON *c = cJSON_AddArrayToObject(t, "chips");
    bool fertig = GESUCHT && ablauf_verbunden(&A);
    #define CHIP(art_, sym_, bild_, ti_, un_) do { cJSON *z_ = cJSON_CreateObject(); fj_n(z_, "a", art_); fj_s(z_, "sym", sym_); fj_n(z_, "b", bild_); fj_s(z_, "t", ti_); fj_s(z_, "u", un_); cJSON_AddItemToArray(c, z_); } while (0)
    {
        const char *reihe = K->MODELL_TYP[0] ? kern_modell_reihe(K->MODELL_TYP, K->MODELL_DIGITAL) : T("Unbekanntes Modell", "Unknown model");
        bool lw = strcmp(K->MODELL_TYP, "fat") ? kern_kopplungs_stufe(K) == KOPPLUNG_BESTAETIGT : false;
        const char *titel = lw ? VERB(fa, reihe, T(" mit Laufwerk", " with disc drive")) : reihe;
        const char *nr = KI.modell[0] ? a_kopie_n(fa, KI.modell, strcspn(KI.modell, " ")) : T("Modell nicht erkannt", "Model not detected");
        const bild11 *kb = konsole_bild11(); int bi = kb->px ? (int)(kb - b11_kon) : -1;
        CHIP(0, "KONSOLE", bi, titel, KI.firmware[0] ? VERB(fa, nr, " · Firmware ", KI.firmware) : nr);
    }
    {
        kopplungsart ks = kern_kopplungs_stufe(K);
        if (ks == KOPPLUNG_BESTAETIGT) CHIP(1, "SCHILD_HAKEN", -1, T("Laufwerk gekoppelt", "Disc drive paired"), T("bleibt immer geschützt", "always stays protected"));
        else { const char *tt = kern_laufwerk_text(K), *dp = strstr(tt, ": "); CHIP(ks == KOPPLUNG_KEINE ? 0 : 3, "SCHEIBE", -1, dp ? a_kopie_n(fa, tt, (size_t)(dp - tt)) : tt, T("wird nie verändert", "never changed")); }
    }
    {
        const char *warn = NULL, *jb = fertig && !G.beschaeftigt ? kern_jailbreak_erkannt(K, fa, &warn) : NULL;
        if (!fertig || G.beschaeftigt) CHIP(3, G.beschaeftigt ? "LAEDT" : "JAILBREAK", -1, "Jailbreak", G.beschaeftigt ? T("wird erkannt …", "being detected …") : T("noch nicht geprüft", "not checked yet"));
        else if (!jb) CHIP(0, "JAILBREAK", -1, "Jailbreak", T("kein Einstieg gefunden", "no entry found"));
        else {
            sliste nm = r_split(fa, jb, ", "); const char *erst = nm.n ? nm.e[0] : jb, *kurz, *ueber = jb_ueber11(erst, &kurz);
            const char *unter = VERB(fa, kurz, " · ", nm.n > 1 ? a_fmt(fa, T("bleibt geschützt · %d weitere erkannt", "stays protected · %d more detected"), nm.n - 1) : T("bleibt geschützt", "stays protected"));
            CHIP(2, "SCHILD_HAKEN", -1, VERB(fa, "Jailbreak ", ueber), unter);
        }
    }
    #undef CHIP
}
/* Nachschauen läuft (start_suche11) */
static void fd_suche(cJSON *t)
{
    cJSON *s = cJSON_AddObjectToObject(t, "suche");
    int schritt = K->SUCHE_SCHRITT, ort = K->SUCHE_ORT;
    if (schritt < 0) schritt = 0; if (schritt > KERN_SUCHE_SCHRITTE) schritt = KERN_SUCHE_SCHRITTE;
    int pz = suche_pz11_jetzt;   /* NEU 1.1 (16.11.2026): wie am Fernseher (dort gemessen) */
    fj_n(s, "pz", pz); fj_n(s, "teil", K->SUCHE_TEIL); fj_b(s, "pruef", schritt == 0); fj_b(s, "ab", K->abbruch);
    Uint32 ms = SDL_GetTicks() - arbeit_seit; const char *rz = T("wird gezählt …", "estimating …");
    if (suche_rest11_ok && pz >= 15 && ms > 1500) { long rest = (long)((double)ms * (100 - pz) / pz / 1000.0) + 1; rz = rest >= 60 ? a_fmt(fa, T("noch etwa %ld Min.", "about %ld min left"), (rest + 30) / 60) : a_fmt(fa, T("noch etwa %ld Sek.", "about %ld s left"), rest); }
    fj_s(s, "rest", rz);
    speicher11 sp = speicher11_holen();
    cJSON *lw = cJSON_AddArrayToObject(s, "lw");
    for (int o = 0; o < 4; o++) if (sp.da[o]) {
        int zu; float an;
        if (o == ORT_INTERN)   /* Codex-Prüfung 347 (P3): wie am Fernseher - nie rückwärts */ { zu = schritt >= KERN_SUCHE_SCHRITTE - 1 ? 2 : 1; an = pz / 100.f; }
        else if (o == ORT_STICK) { zu = schritt > 8 ? 2 : 1; an = zu == 2 ? 1.f : pz / 100.f; }
        else { zu = schritt > 7 ? 2 : 1; an = zu == 2 ? 1.f : pz / 100.f; }
        if (ort == o && zu == 0) zu = 1;
        if (an > 1.f) an = 1.f;
        cJSON *z = cJSON_CreateObject(); fj_s(z, "n", ort_name11(o, sp.usb_n)); fj_s(z, "g", kern_groesse_text(fa, sp.ges[o])); fj_n(z, "zu", zu); fj_n(z, "an", an);
        cJSON_AddItemToArray(lw, z);
    }
    fj_n(s, "gef", schritt > 0 && K->bau.n > 0 ? K->bau.n : 0); fj_s(s, "eintr", EINTRAEGE(K->bau.n > 0 ? K->bau.n : 0));
}
/* "Wusstest du?" - dieselbe Auswahl wie wusstest11 */
static void fd_wusstest(cJSON *t, int art)
{
    const char *w = NULL, *s = NULL; wusstest_wahl(art, &w, &s);
    cJSON *x = cJSON_AddObjectToObject(t, "wusstest"); fj_s(x, "w", w); fj_s(x, "s", s);
}
/* nach dem Nachschauen (start_nachher11) */
static void fd_nachher(cJSON *t)
{
    cJSON *x = cJSON_AddObjectToObject(t, "nachher");
    long long ges[8] = { 0 }; int anz[8] = { 0 }; bool alle_da = true, da_k[8] = { 0 }; long long summe = 0;
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        int k = ALLE_KAT[ki]; kblock *b = NULL; int nb = kern_bloecke(K, fa, k, SORT_AZ, &b);
        anz[k] = kern_anzahl(K, k); da_k[k] = true;
        for (int i = 0; i < nb; i++) { if (!b[i].groesse_da) alle_da = da_k[k] = false; else ges[k] += b[i].bytes; }
        summe += ges[k];
    }
    bool zl = zaehlen_laeuft();
    speicher11 sp = speicher11_holen(); const char *orte = NULL;
    for (int o = 0; o < 4; o++) if (sp.da[o]) orte = orte ? VERB(fa, orte, o == 3 ? T(" und ", " and ") : ", ", ort_name11(o, sp.usb_n)) : ort_name11(o, sp.usb_n);
    fj_s(x, "zusatz", VERB(fa, orte ? orte : "", T(" · nichts wurde verändert", " · nothing was changed")));
    fj_s(x, "gross", VERB(fa, !alle_da && !zl ? T("mind. ", "min. ") : "", kern_groesse_text(fa, summe)));
    fj_b(x, "zaehlt", !alle_da && zl);
    cJSON *ka = cJSON_AddArrayToObject(x, "k");
    for (int ki = 0; ki < KAT_ANZ; ki++) {
        int k = ALLE_KAT[ki];
        cJSON *z = cJSON_CreateObject(); fj_n(z, "k", k); fj_n(z, "bi", kat_index11(k)); fj_s(z, "n", kern_kat_name(k));
        fj_s(z, "g", !anz[k] ? "–" : (da_k[k] || !zl) ? kern_groesse_text(fa, ges[k]) : T("zählt …", "counting …"));
        fj_s(z, "z", !anz[k] ? T("keine", "none") : a_fmt(fa, anz[k] == 1 ? T("%d Eintrag", "%d entry") : T("%d Einträge", "%d entries"), anz[k]));
        cJSON_AddItemToArray(ka, z);
    }
}
/* Tafel "Optionen" (optionen11) */
static void fd_optionen(cJSON *t)
{
    cJSON *x = cJSON_AddObjectToObject(t, "optionen");
    int ids[10], n = opt_ids11(ids);
    fj_n(x, "f", opt_fokus < n ? opt_fokus : n - 1);
    cJSON *z = cJSON_AddArrayToObject(x, "z");
    for (int i = 0; i < n; i++) {
        const char *t1 = "", *t2 = ""; char a[64];
        switch (ids[i]) {
        case OP_SPRACHE: t1 = T("Sprache", "Language"); t2 = SP_EN ? "English" : "Deutsch"; break;
        case OP_TEST: t1 = T("Testmodus", "Test mode");
#ifdef NUR_LESEN
            t2 = T("diese Testversion schaut immer nur nach", "this test version only ever looks");
#else
            t2 = testmodus ? T("an – es wird nur angeschaut", "on – only looking") : T("aus – Löschen ist möglich", "off – deleting is possible");
#endif
            break;
        case OP_NOCHMAL: t1 = T("Nochmal nachschauen", "Scan again"); t2 = T("sucht alles noch einmal", "searches everything again"); break;
        case OP_SORT: t1 = T("Reihenfolge", "Order"); t2 = sortierung == SORT_AZ ? "A–Z" : T("größte zuerst", "largest first"); break;
        case OP_ORDNER: t1 = T("Ordner anzeigen", "Show folders"); t2 = ordner_zeigen ? T("an – im Kasten rechts", "on – in the box on the right") : T("aus", "off"); break;
        case OP_BROWSER: t1 = T("Bedienung im Browser", "Browser control"); if (fern_an()) { fern_adresse(a, sizeof a); t2 = VERB(fa, T("an · ", "on · "), txt_beginnt(a, "http://") ? a + 7 : a); } else t2 = T("nicht verfügbar", "not available"); break;
        case OP_PROT: t1 = T("Protokoll speichern", "Save log"); t2 = T("auf das USB-Laufwerk · □ ansehen", "to the USB drive · □ view"); break;
        default: t1 = T("Rückmeldung geben", "Give feedback"); t2 = T("Direkt senden · ohne Internet per QR-Code", "Send directly · without internet via QR code"); break;
        }
        cJSON *r = cJSON_CreateObject(); fj_n(r, "id", ids[i]); fj_s(r, "t1", t1); fj_s(r, "t2", t2); cJSON_AddItemToArray(z, r);
    }
    char un[32]; upd_holen(un, sizeof un);
    fj_s(x, "info", VERB(fa, "Version ", VERSION_NUMMER, T(" · von da_Chris86", " · by da_Chris86")));
    if (un[0]) { fj_s(x, "un", un); fj_s(x, "ort", VERB(fa, T("Kommt dorthin, wo PS5 Cleanup jetzt liegt: ", "Goes where PS5 Cleanup is now: "), upd_ort_name())); fj_s(x, "ortn", upd_ort_name()); }   /* ortn: Nachtprüfung 16.11.2026 - kurze Zeile wie am Fernseher */
}
/* Update-Fenster (upd_dialog11) */
static void fd_update(cJSON *t)
{
    cJSON *x = cJSON_AddObjectToObject(t, "upd");
    char un[32]; upd_holen(un, sizeof un);
    int st = __atomic_load_n(&UPD.stand, __ATOMIC_ACQUIRE);
    fj_n(x, "st", st); fj_n(x, "f", UPD.fokus); fj_s(x, "un", un); fj_s(x, "alt", VERSION_NUMMER);
    fj_s(x, "ort", VERB(fa, upd_ort_name(), T(" – dort, wo PS5 Cleanup jetzt liegt", " – where PS5 Cleanup is now")));
    if (st == 2) { fj_s(x, "ziel", UPD.ziel); fj_s(x, "datei", UPD.datei); fj_b(x, "da", UPD.schon_da); }
    if (st == 3) fj_s(x, "grund", UPD.grund);
}
/* Rückmeldung (schirm_rueckmeldung) */
static void fd_rm(cJSON *t)
{
    cJSON *x = cJSON_AddObjectToObject(t, "rm");
    rueckmeldung r = rueckmeldung_daten();
    bool komplett = K->KOMPLETT_LAUF, neustart = RM.aktion == 2 || (RM.aktion == 1 && neustart_pflicht);
    int netz = __atomic_load_n(&RM.netz, __ATOMIC_ACQUIRE), sd = __atomic_load_n(&RM.senden, __ATOMIC_ACQUIRE);
    bool offline = RM.offline || netz == 2, geloescht = r.s == 2;
    fj_n(x, "aktion", RM.aktion); fj_n(x, "f", RM.fokus); fj_n(x, "w", RM.wertung); fj_s(x, "text", RM.text);
    fj_n(x, "netz", netz); fj_n(x, "sd", sd); fj_b(x, "offline", offline); fj_b(x, "neustart", neustart);
    if (RM.aktion) fj_s(x, "chip", VERB(fa, komplett ? T("nach „Alles bereinigen“", "after \"Clean up everything\"") : T("nach „Selbst auswählen“", "after \"Choose yourself\""), " · ", neustart ? T("Neu starten", "Restart") : T("Beenden", "Quit")));
    fj_s(x, "nt", netz == 0 ? T("Internet wird geprüft …", "Checking internet …") : offline && netz == 1 ? T("Senden ging nicht", "Sending failed") : netz == 1 ? T("Internet verbunden", "Internet connected") : T("Kein Internet", "No internet"));
    const char *aktt = RM.aktion == 0 ? T("zurück", "back") : neustart ? T("neu starten", "restart") : T("beenden", "quit");
    fj_s(x, "aktt", aktt);
    bool kp = geloescht && komplett;
    if (!offline) {
        fj_s(x, "titel", !geloescht ? T("Wie läuft PS5 Cleanup bei dir?", "How is PS5 Cleanup doing for you?") : komplett ? T("Ist die PS5 jetzt sauber?", "Is the PS5 clean now?") : T("Hat alles geklappt?", "Did everything work?"));
        const char *sz = !geloescht ? T("Bis jetzt wurde nichts gelöscht.", "Nothing has been deleted so far.") : komplett ? VERB(fa, T("Du hast die interne SSD", "You cleaned the internal SSD"), A.leeren_m2 ? T(" und die M.2-SSD", " and the M.2 SSD") : "", T(" komplett bereinigt.", " completely."))
                                  : rm_satz_selbst(&r);
        fj_s(x, "satz", VERB(fa, sz, T(" Deine Rückmeldung zeigt, wie PS5 Cleanup auf anderen Konsolen läuft – auch wenn alles gut ging.", " Your feedback shows how PS5 Cleanup runs on other consoles – even if everything went well.")));
        fj_s(x, "ja", kp ? T("Ja, alles sauber", "Yes, all clean") : T("Ja, alles gut", "Yes, all good"));
        fj_s(x, "ja2", kp ? T("Auf den gewählten Laufwerken ist alles weg", "Everything on the chosen drives is gone") : T("Alles Ausgewählte ist weg", "Everything selected is gone"));
        fj_s(x, "nein", kp ? T("Nein, etwas ist noch da", "No, something is still there") : T("Nein, etwas hat nicht geklappt", "No, something did not work"));
        fj_s(x, "hw", RM.aktion == 0 ? T("Danach geht es zurück zum Ergebnis.", "Afterwards you return to the result.")
                      : neustart ? T("Du hast „Neu starten“ gewählt – die PS5 startet danach neu, mit oder ohne Rückmeldung.", "You chose \"Restart\" – the PS5 restarts afterwards, with or without feedback.")
                                 : T("Du hast „Beenden“ gewählt – PS5 Cleanup beendet sich danach, mit oder ohne Rückmeldung.", "You chose \"Quit\" – PS5 Cleanup quits afterwards, with or without feedback."));
    } else {
        fj_s(x, "titel", T("Rückmeldung per Handy", "Feedback via phone"));
        fj_s(x, "satz", RM.offline && netz == 1 ? T("Direkt senden hat gerade nicht geklappt. Deine Rückmeldung geht trotzdem – über dein Handy.", "Sending directly did not work just now. Your feedback still works – via your phone.")
                                                : T("Die PS5 ist gerade nicht mit dem Internet verbunden. Deine Rückmeldung geht trotzdem – über dein Handy.", "The PS5 is not connected to the internet right now. Your feedback still works – via your phone."));
        fj_s(x, "link", rueckmeldung_link(fa, &r));
        {   /* derselbe QR-Code wie am Fernseher (rueckmeldung_qr): Zeilen aus 0/1 */
            int qn = rueckmeldung_qr(&r);
            if (qn > 0) { char *q = a_hole(fa, (size_t)qn * (size_t)(qn + 1) + 1); size_t p = 0;
                for (int j = 0; j < qn; j++) { for (int i = 0; i < qn; i++) q[p++] = qrcodegen_getModule(qr_bild, i, j) ? '1' : '0'; q[p++] = j + 1 < qn ? '/' : 0; }
                fj_s(x, "qr", q); }
        }
        fj_s(x, "knopf", RM.aktion == 0 ? T("Zurück", "Back") : neustart ? T("Neu starten", "Restart") : T("Beenden", "Quit"));
        fj_s(x, "qr_hw", neustart ? T("Das Protokoll ist zu groß für den Code – es bleibt auf dem USB-Laufwerk. Freiwillig – du kannst auch einfach neu starten.", "The log is too big for the code – it stays on the USB drive. Optional – you can simply restart.") : T("Das Protokoll ist zu groß für den Code – es bleibt auf dem USB-Laufwerk. Freiwillig – du kannst auch einfach weitermachen.", "The log is too big for the code – it stays on the USB drive. Optional – you can simply carry on."));
    }
    cJSON *kv = cJSON_AddArrayToObject(x, "kv");
    const char *reihe = K->MODELL_TYP[0] ? kern_modell_reihe(K->MODELL_TYP, K->MODELL_DIGITAL) : "PS5";
    fj_paar(kv, T("Konsole", "Console"), VERB(fa, reihe, r.modell[0] ? VERB(fa, " · ", r.modell) : ""));
    fj_paar(kv, "Firmware", r.fw[0] ? r.fw : T("unbekannt", "unknown"));
    fj_paar(kv, "PS5 Cleanup", VERB(fa, "Version ", VERSION_NUMMER));
    fj_paar(kv, T("Art", "Type"), komplett ? T("Alles bereinigen", "Clean up everything") : T("Selbst ausgewählt", "Selected yourself"));
    if (komplett) fj_paar(kv, T("Laufwerke", "Drives"), VERB(fa, T("Interne SSD", "Internal SSD"), A.leeren_m2 ? ", M.2-SSD" : "", A.leeren_usb ? T(", erweiterter Speicher", ", extended storage") : ""));
    fj_paar(kv, T("Ergebnis", "Result"), a_fmt(fa, T("%d gelöscht · %d Fehler", "%d deleted · %d errors"), rm_geloescht(&r), r.f));
    fj_paar(kv, T("Freigeräumt", "Freed"), groesse_von(fa, A.rm_frei.Bytes, A.rm_frei.Gekappt));
    fj_paar(kv, T("Dauer", "Duration"), dauer_text(r.t));
    if (!offline) fj_paar(kv, T("Protokoll", "Log"), VERB(fa, T("mit Spiel-/App-Namen · ", "with game/app names · "), kern_groesse_text(fa, (long long)RM.prot_bytes)));
    if (!offline) fj_paar(kv, T("Deine Antwort", "Your answer"), RM.wertung ? (kp ? T("Nein, etwas ist noch da", "No, something is still there") : T("Nein, etwas hat nicht geklappt", "No, something did not work")) : (kp ? T("Ja, alles sauber", "Yes, all clean") : T("Ja, alles gut", "Yes, all good")));
    if (!offline && RM.text[0]) fj_paar(kv, T("Dein Text", "Your note"), text_geschwaerzt(fa, RM.text));   /* Codex-Prüfung 359: wie gesendet */
    if (sd) {
        fj_s(x, "s1", sd == 2 ? T("Rückmeldung gesendet – danke!", "Feedback sent – thank you!") : sd == 3 ? T("Senden hat nicht geklappt", "Sending did not work") : T("Wird gesendet …", "Sending …"));
        fj_s(x, "s2", sd == 2 ? (RM.aktion == 2 || neustart ? T("Die PS5 startet in 3 Sekunden neu …", "The PS5 restarts in 3 seconds …") : RM.aktion == 1 ? T("PS5 Cleanup beendet sich gleich …", "PS5 Cleanup quits in a moment …") : T("Gleich geht es zurück.", "Returning in a moment."))
                       : sd == 3 ? T("Kein Problem – gleich erscheint der QR-Code fürs Handy.", "No problem – the QR code for your phone appears in a moment.") : T("Bitte kurz warten.", "Please wait a moment."));
    }
}
/* Speicher danach (speicher_danach11): je Laufwerk Name, gesamt, frei danach, was frei wird */
static void fd_speicher_danach(cJSON *x, const long long *weg)
{
    speicher11 sp = speicher11_holen();
    cJSON *a = cJSON_AddArrayToObject(x, "sd");
    for (int o = 0; o < 4; o++) if (sp.da[o] && (o == 0 || weg[o] > 0)) {
        long long ges = sp.ges[o], jetzt = sp.frei[o] < ges ? sp.frei[o] : ges, frei = sp.frei[o] + weg[o]; if (frei > ges) frei = ges;
        cJSON *z = cJSON_CreateObject(); fj_s(z, "n", ort_name11(o, sp.usb_n)); fj_s(z, "frei", speicher_frei_text(jetzt, frei));   /* PS5-Test 17:58: jetzt → danach */
        if (weg[o] > 0) fj_s(z, "plus", VERB(fa, " · +", kern_groesse_text(fa, weg[o])));
        fj_n(z, "belegt", ges > 0 ? (double)(ges - frei) / (double)ges : 0); fj_n(z, "weg", ges > 0 ? (double)weg[o] / (double)ges : 0);
        cJSON_AddItemToArray(a, z);
    }
}
/* Alles bereinigen (schirm_komplett11) */
static void fd_komplett(cJSON *t)
{
    komplett_vorbereiten();
    cJSON *x = cJSON_AddObjectToObject(t, "kb");
    speicher11 sp = speicher11_holen();
    int orte[3] = { ORT_INTERN, ORT_M2, ORT_USB }, n = 0;
    long long frei_ges = 0; int anz_ges = 0; const char *wo = T("Interne SSD", "Internal SSD");
    long long weg[4] = { 0, 0, 0, 0 };
    cJSON *ol = cJSON_AddArrayToObject(x, "orte");
    for (int i = 0; i < 3; i++) {
        int o = orte[i]; if (o != ORT_INTERN && !KB.da[o]) continue;
        int an_n = 0, sp_n = 0; long long b = komplett_bytes(o, &an_n, &sp_n);
        bool an = o == ORT_INTERN || KB.leeren[o];
        if (an) { int wn = 0; long long wb = komplett_bytes_wahl(o, &wn, NULL); frei_ges += wb; anz_ges += wn; weg[o] = wb; if (o != ORT_INTERN) wo = VERB(fa, wo, T(" und ", " and "), ort_name11(o, sp.usb_n)); }   /* 1.1: wie am Fernseher */
        cJSON *z = cJSON_CreateObject(); fj_s(z, "n", ort_name11(o, sp.usb_n)); fj_b(z, "fest", o == ORT_INTERN); fj_b(z, "an", an);
        fj_s(z, "u", komplett_unter(o, an_n, sp_n, b));   /* 1.1: wie am Fernseher (mit den Spielen) */
        cJSON_AddItemToArray(ol, z); n++;
    }
    fj_n(x, "f", KB.fokus); fj_s(x, "warn", komplett_warnung());   /* 1.1 */
    fj_s(x, "frei", VERB(fa, kv_voll ? T("etwa ", "about ") : T("mindestens ", "at least "), kern_groesse_text(fa, frei_ges)));
    fj_s(x, "wo", VERB(fa, wo, " · ", a_fmt(fa, "%d", anz_ges), EINTRAEGE(anz_ges)));
    const char *jn = kopf_jb_name();
    fj_s(x, "bleibt", VERB(fa, T("Laufwerkskopplung, Lizenzen und Konten, Systemsoftware", "Disc drive pairing, licenses and accounts, system software"),
                       jn ? VERB(fa, T(", Jailbreak ", ", jailbreak "), jn) : "", T(", der Bereich Jailbreak und alles auf dem USB-Laufwerk.", ", the jailbreak area and everything on the USB drive.")));
    pthread_mutex_lock(&G.m); char ein[64]; snprintf(ein, sizeof ein, "%s", G.eingabe); pthread_mutex_unlock(&G.m);
    fj_s(x, "eingabe", ein); fj_b(x, "passt", eingabe_passt()); fj_s(x, "wort", T("LÖSCHEN", "DELETE"));
    fd_speicher_danach(x, weg);
}
/* Bestätigen (schirm_bestaetigen): nach Bereichen wie am Fernseher, was bleibt, Speicher danach */
static void fd_best(cJSON *t)
{
    cJSON *x = cJSON_AddObjectToObject(t, "best");
    static const int KO[8] = { 1, 6, 2, 3, 4, 5, 7, 0 };
    int nmax = 0; for (int i = 0; i < K->Funde.n; i++) if (((eintrag *)K->Funde.e[i])->Wahl) nmax++;
    b_zeile *gz[8]; int gn[8] = { 0 }; long long weg[4] = { 0, 0, 0, 0 }; bool jb_gew = false, jb_da = false;
    for (int g = 0; g < 8; g++) gz[g] = a_hole(fa, sizeof(b_zeile) * (size_t)(nmax + 1));
    for (int i = 0; i < K->Funde.n; i++) {
        eintrag *f = K->Funde.e[i]; if (f->Jailbreak) jb_da = true;
        if (!f->Wahl || f->Gesperrt) continue;
        if (f->Jailbreak) jb_gew = true;
        int g = 7; for (int j = 0; j < 8; j++) if (KO[j] == f->Kat) g = j;
        groesse *gr = kern_eintrag_groesse_in(K, fa, f, true);
        gz[g][gn[g]++] = (b_zeile){ f, gr ? gr->Bytes : -1, gr != NULL, gr ? gr->Gekappt : false };
        if (gr) { int o = anzeige_ort(f); if (o >= 0 && o < 4) weg[o] += gr->Bytes; }
    }
    fj_s(x, "titel", tippen_aktiv() ? T("Alles wird gelöscht", "Everything will be deleted") : T("Das wird gelöscht", "This will be deleted"));
    if (A.GROSS.Bytes > 0) fj_s(x, "gt", groesse_von(fa, A.GROSS.Bytes, A.GROSS.Gekappt));
    cJSON *ga = cJSON_AddArrayToObject(x, "g");
    for (int g = 0; g < 8; g++) {
        if (!gn[g]) continue;
        qsort(gz[g], (size_t)gn[g], sizeof(b_zeile), b_vergleich);
        cJSON *y = cJSON_CreateObject(); fj_n(y, "k", KO[g]); fj_s(y, "n", gross11(kern_kat_name(KO[g]))); fj_n(y, "anz", gn[g]);
        cJSON *e = cJSON_AddArrayToObject(y, "e");
        for (int j = 0; j < gn[g] && j < 60; j++) {
            eintrag *f = gz[g][j].f; const char *nm, *ws; zeile_teilen(f, f->Kat, &nm, &ws);
            cJSON *z = cJSON_CreateObject(); fj_s(z, "n", nm); fj_s(z, "u", ws[0] ? VERB(fa, ws, " · ", schild_text(f, f->Kat)) : schild_text(f, f->Kat));
            if (kern_ampel(f) == AMPEL_ROT) fj_b(z, "rot", true);
            if (gz[g][j].da) fj_s(z, "g", groesse_von(fa, gz[g][j].b, gz[g][j].gek));
            const char *bid = bild_kennung(f); if (bid && strlen(bid) < 16) fj_s(z, "i", bid);
            fj_n(z, "k", f->Kat); cJSON_AddItemToArray(e, z);
        }
        cJSON_AddItemToArray(ga, y);
    }
    cJSON *bl = cJSON_AddArrayToObject(x, "bleibt");
    cJSON_AddItemToArray(bl, cJSON_CreateString(T("Laufwerkskopplung", "Disc drive pairing"))); cJSON_AddItemToArray(bl, cJSON_CreateString(T("Lizenzen und Konten", "Licenses and accounts")));
    cJSON_AddItemToArray(bl, cJSON_CreateString(T("Systemsoftware", "System software")));
    if (jb_da && !jb_gew) { const char *jn = kopf_jb_name(); cJSON_AddItemToArray(bl, cJSON_CreateString(jn && jn[0] ? VERB(fa, "Jailbreak ", jn) : "Jailbreak")); }
    cJSON_AddItemToArray(bl, cJSON_CreateString(T("Alles nicht Angehakte", "Everything not ticked")));
    fd_speicher_danach(x, weg);
    if (tippen_aktiv()) { fj_b(x, "tippen", true); pthread_mutex_lock(&G.m); fj_s(x, "eingabe", G.eingabe); pthread_mutex_unlock(&G.m); fj_b(x, "passt", eingabe_passt()); }
}
/* Fertig (schirm_fertig): die Hinweise unter dem Ergebnis */
static void fd_fertig(cJSON *t)
{
    cJSON *x = cJSON_AddObjectToObject(t, "fertig");
    pthread_mutex_lock(&G.m); const char *kopf = a_kopie(fa, G.f_kopf ? G.f_kopf : ""), *sm = a_kopie(fa, G.f_sm ? G.f_sm : ""); pthread_mutex_unlock(&G.m);
    bool fehler = fertig_fehler();
    const char *wo = protokoll_wo(), *rt = kern_rest_text(K, fa);
    cJSON *h = cJSON_AddArrayToObject(x, "hw");
    #define HW(t_, c_, s_) do { cJSON *z_ = cJSON_CreateObject(); fj_s(z_, "t", t_); fj_s(z_, "c", c_); fj_s(z_, "s", s_); cJSON_AddItemToArray(h, z_); } while (0)
    HW(wo[0] ? VERB(fa, T("Das Protokoll liegt ", "The log is "), wo, ".") : T("Das Protokoll siehst du mit der Options-Taste.", "View the log with the Options button."), "d", "INFO");
    { const char *nl = strchr(kopf, '\n'); if (nl && nl[1]) HW(nl + 1, "d", "INFO"); }
    if (sm[0]) HW(sm, "y", "INFO");
    if (rt[0]) HW(rt, fehler ? "y" : "d", fehler ? "WARNUNG" : "INFO");
    #undef HW
    fj_b(x, "pflicht", neustart_pflicht);
    fj_s(x, "wieder", VERB(fa, T("PS5 neu starten und PS5 Cleanup noch einmal laufen lassen.", "Restart the PS5 and run PS5 Cleanup again."), wo[0] ? VERB(fa, T(" Das Protokoll liegt ", " The log is "), wo, ".") : ""));
}
/* Auswahl: Reiter wie am Fernseher (sicht[]) und welcher gewählt ist */
static void fd_reiter(cJSON *t)
{
    sicht_neu();
    cJSON *a = cJSON_AddArrayToObject(t, "sicht");
    for (int i = 0; i < sicht_n; i++) cJSON_AddItemToArray(a, cJSON_CreateNumber(sicht[i]));
    fj_n(t, "ber", fokus_bereich);
    int k = sicht[fokus_bereich]; al_ordnen(k);
    fj_n(t, "mb", al_b); fj_n(t, "mt", al_mehr(al_b) ? al_t : -1);
    kasten_d d; memset(&d, 0, sizeof d); al_kasten_daten(520, k, &d);
    cJSON *x = cJSON_AddObjectToObject(t, "kasten");
    fj_s(x, "t", d.titel ? d.titel : ""); fj_s(x, "u", d.unter ? d.unter : ""); fj_n(x, "k", d.k);
    if (d.bild && d.bild != &RESTE_ZEILE) { const char *bid = bild_kennung(d.bild); if (bid && strlen(bid) < 16) fj_s(x, "i", bid); fj_b(x, "mitbild", true); }
    cJSON *ab = cJSON_AddArrayToObject(x, "abs");
    for (int i = 0; i < d.n; i++) {
        const k_absatz *q = &d.ab[i]; if (!q->text || !q->text[0]) continue;
        cJSON *z = cJSON_CreateObject();
        if (q->frage) fj_s(z, "f", gross11(q->frage));
        fj_s(z, "t", q->text);
        fj_s(z, "c", q->farbe == N_ROT ? "r" : q->farbe == N_GRUEN ? "g" : q->farbe == N_GELB || q->farbe == N_ORANGE ? "y" : q->farbe == N_GRAU || q->farbe == N_GRAU2 ? "d" : "t");
        if (q->sym) fj_s(z, "sym", q->sym == SYM_JAILBREAK ? "JAILBREAK" : q->sym == SYM_WARNUNG ? "WARNUNG" : q->sym == SYM_SCHLOSS ? "SCHLOSS" : "INFO");
        cJSON_AddItemToArray(ab, z);
    }
    fj_n(x, "st_art", d.st_art); if (d.st) fj_s(x, "st", d.st);
    fj_s(t, "sort", sortierung == SORT_AZ ? "az" : "gr");
    fj_b(t, "ordner", ordner_zeigen);
}
static void fd_tv(cJSON *o)
{
    cJSON *t = cJSON_AddObjectToObject(o, "tv");
    bool dialog; const char *s = fd_schirm(&dialog);
    fj_s(t, "s", s); fj_b(t, "dialog", dialog); fj_s(o, "vnr", VERSION_NUMMER); fj_b(t, "opt", optionen_offen && !G.beschaeftigt && G.art == D_KEINER);
    fj_b(t, "prot", protokoll_offen); fj_n(t, "df", dlg_fokus); fj_b(t, "upd_offen", UPD.offen && optionen_offen);
    if (!strcmp(s, "start") || !strcmp(s, "suche") || !strcmp(s, "nachher")) fd_chips(t);
    if (!strcmp(s, "suche")) { fd_suche(t); fd_wusstest(t, 0); }
    if (!strcmp(s, "loeschen")) fd_wusstest(t, 1);
    if (!strcmp(s, "nachher")) fd_nachher(t);
    if (!strcmp(s, "liste")) fd_reiter(t);
    if (optionen_offen) fd_optionen(t);
    if (UPD.offen) fd_update(t);
    if (RM.offen) fd_rm(t);
    if (!strcmp(s, "komplett")) fd_komplett(t);
    if (!strcmp(s, "best")) fd_best(t);
    if (!strcmp(s, "fertig")) fd_fertig(t);
    if (protokoll_offen) {   /* wo das Protokoll liegt und der Kasten "Rückmeldung (freiwillig)" - wie schirm_protokoll */
        pthread_mutex_lock(&G.m);
        const char *wo = prot_stick_f ? VERB(fa, T("Gespeichert auf dem USB-Laufwerk: ", "Saved on the USB drive: "), prot_stick_pfad)
                       : prot_f ? VERB(fa, T("Gespeichert auf der PS5: ", "Saved on the PS5: "), prot_intern_pfad, T("  ·  □ = zusätzlich auf das USB-Laufwerk", "  ·  □ = also save to the USB drive"))
#ifdef KERN_LOESCHEN
                       : T("Nur hier in der Ansicht  ·  □ = auf das USB-Laufwerk speichern", "Only in this view  ·  □ = save to the USB drive");
#else
                       : T("Nur hier in der Ansicht - die Testfassung speichert nichts", "Only in this view - the test version saves nothing");
#endif
        pthread_mutex_unlock(&G.m);
        fj_s(t, "prot_wo", prot_meldung[0] ? prot_meldung : wo); fj_b(t, "prot_gelb", prot_meldung[0] != 0);
#ifdef KERN_LOESCHEN
        fj_b(t, "prot_usb", true);
#endif
        rueckmeldung r = rueckmeldung_daten(); int qn = rueckmeldung_qr(&r);
        if (qn > 0) {
            cJSON *k = cJSON_AddObjectToObject(t, "prot_rm");
            char *q = a_hole(fa, (size_t)qn * (size_t)(qn + 1) + 1); size_t p = 0;
            for (int j = 0; j < qn; j++) { for (int i = 0; i < qn; i++) q[p++] = qrcodegen_getModule(qr_bild, i, j) ? '1' : '0'; q[p++] = j + 1 < qn ? '/' : 0; }
            fj_s(k, "qr", q);
            fj_s(k, "info", VERB(fa, T("Mit dem Handy scannen und sagen, ob es geklappt hat.\nIm Code: ", "Scan with your phone and tell us whether it worked.\nIn the code: "),
                                 rueckmeldung_werte(fa, &r), T(" - dazu Jailbreak-Einstieg, Speicher, ShadowMount, Zahlen je Bereich", " - plus jailbreak entry, storage, ShadowMount, numbers per area"),
                                 r.fe_n ? T(" und Fehlercodes", " and error codes") : "", T(". Das Handy zeigt alles vor dem Senden.", ". Your phone shows everything before sending.")));
            fj_s(k, "schutz", T("Keine Namen von Spielen oder Apps, kein Konto, keine Dateien. Gesendet wird erst mit \"Senden\" am Handy.",
                                "No game or app names, no account, no files. Nothing is sent until you tap \"Send\" on your phone."));
            const char *adr = RUECKMELDUNG_ADRESSE; if (!strncmp(adr, "https://", 8)) adr += 8;
            fj_s(k, "adr", VERB(fa, T("Ohne Scannen: ", "Without scanning: "), adr));
            fj_s(k, "link", rueckmeldung_link(fa, &r));
        }
    }
    if (!strcmp(s, "loeschen")) { pthread_mutex_lock(&G.m); fj_b(t, "unb", G.unbestimmt); fj_n(t, "p", G.prozent); pthread_mutex_unlock(&G.m); }
}
