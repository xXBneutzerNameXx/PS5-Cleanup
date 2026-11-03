// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// Selbsttest.cs - Fenster mit Probe-Einträgen aufbauen, echte Klicks
// nachstellen, Ergebnis in _selbsttest.txt schreiben, Fenster schließen.
// (Ohne PS5 - prüft die Oberfläche und die Auswahl-Logik.)
// =====================================================================
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace PS5Cleanup
{
    public static class Selbsttest
    {
        static string datei;
        static void Schreib(string t) { File.AppendAllText(datei, t + "\r\n", new UTF8Encoding(false)); }

        static void Klick(CheckBox c, bool an)
        {
            c.IsChecked = an;
            c.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
        }

        static Eintrag E(int kat, string text, string quelle = "", bool gesperrt = false, int ort = Kern.ORT_INTERN)
        {
            if (gesperrt) text += Kern.GESPERRT_TEXT;
            return new Eintrag { Pfade = new List<string> { "/x" + text.GetHashCode() }, PfadeBasis = new List<string> { "/x" + text.GetHashCode() }, Kat = kat, Text = text, TextBasis = text, Quelle = quelle, Gesperrt = gesperrt, Ort = ort };
        }

        public static void Laufen(Oberflaeche o, string bild)
        {
            datei = bild != "" ? Path.Combine(Path.GetDirectoryName(Path.GetFullPath(bild)), "_selbsttest.txt") : Path.Combine(Programm.Ordner, "_selbsttest.txt");
            File.WriteAllText(datei, "Selbsttest " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + " HIER=" + Programm.Ordner + " Version=" + Kern.VERSION + " Sprache=" + (Sp.EN ? "en" : "de") + "\r\n", new UTF8Encoding(false));
            var K = o.K;
            Func<string, string, string> T = Sp.T;
            K.Funde = new List<Eintrag> {
                E(1, "M.2 – Red Dead Redemption (PPSA30528)" + T(" - ShadowMount-Spiel", " - ShadowMount game"), "/mnt/ext1/homebrew/x.exfat", false, Kern.ORT_M2),
                E(1, T("Echtes Spiel (PPSA55555) + Update + Zusatzinhalte", "Real game (PPSA55555) + update + add-ons")),
                E(1, "M.2 – Game X (PPSA11111)" + T(" - ShadowMount-Spiel", " - ShadowMount game"), "/mnt/ext1/homebrew/y.exfat", true, Kern.ORT_M2),
                E(2, T("Spielstand ", "Saved data ") + "PPSA55555 (Real Game)"),
                E(3, T("Trophäen-Daten ", "Trophy data ") + "trophy/data"),
                E(4, T("Unfertige Aufnahme von ", "Unfinished capture of ") + "Real Game (PPSA55555) (3" + T(" Dateien)", " files)")),
                E(5, T("Share-Einstellungen alter Spiele (66)", "Share settings of old games (66)")),
                E(5, T("Fehlerberichte (100)", "Error reports (100)")),
                E(6, T("Tool-Kachel: ", "Tool tile: ") + "Payload Manager (PLDM00001)"),
                E(6, "shadowmount"),
                E(5, "M.2 – games/DuckTales Remastered [BLES01969].iso", "", false, Kern.ORT_M2),
                E(5, "M.2 – games/Spiel-Sicherung.zip", "", false, Kern.ORT_M2) };
            // ein normal installiertes PS4-Spiel mit Update und DLC
            var teile = new[] { new[] { "Spiel", "Bloodborne (CUSA00900)" + T(" - Spiel", " - game") }, new[] { "Update", "Bloodborne (CUSA00900) - Update (Version 01.09)" }, new[] { "DLC", "Bloodborne (CUSA00900)" + T(" - Zusatzinhalt: ", " - add-on: ") + "The Old Hunters" } };
            int pos = 2;
            foreach (var t in teile) { var e = E(1, t[1]); e.Gruppe = "/user|CUSA00900"; e.Teil = t[0]; K.Funde.Insert(pos++, e); }
            K.Funde = Kern.ALLE_KAT.SelectMany(k => K.Funde.Where(f => f.Kat == k)).ToList();
            K.m2Da = true;
            o.W.Loaded += delegate
            {
                bool hakenOk = false, teilOk = false, knopfOk = false, smOk = false, schliessOk = false, groesseOk = false, updateOk = false, fertigOk = false;
                bool ps4Ok = false, dbOk = false;   // NEU 1.0.7
                bool sqliteOk = false, sicherungOk = false, sperreOk = false;   // NEU 1.0.7 (da_Chris86 31.10.2026, Entscheidungen 1-3)
                try
                {
                    // ohne PS5: die Datenbank gilt als lesbar (die Sperre bei nicht lesbarer Datenbank prüft die Sperre-Probe unten)
                    K.DbBereitschaftSetzen(Kern.DbStand.Bereit, "");
                    var tbInfo = (TextBlock)o.W.FindName("tbInfo"); var tbIp = (TextBox)o.W.FindName("tbIp"); var tbLeer = (TextBlock)o.W.FindName("tbLeer");
                    tbLeer.Visibility = Visibility.Collapsed;
                    K.DDD_STATUS = 1; K.MODELL_TYP = "pro"; K.MODELL_DIGITAL = true;
                    tbInfo.Text = T("Verbunden mit ", "Connected to ") + "192.168.1.50" + T(" (Port ", " (port ") + "2121).";
                    tbIp.Text = "192.168.1.50";
                    o.F_MODELL = "CFI-7021 B01Y"; o.F_REIHE = "PS5 Pro"; o.F_FIRMWARE = "12.00"; o.F_QUELLE = T("automatisch (ps5info)", "automatic (ps5info)"); o.F_PORT = 2121; o.F_SCAN = true; o.F_SCAN_TEST = true;
                    o.ZeigeKonsole("CFI-7021 B01Y", "PS5 Pro", "12.00", o.F_QUELLE, 2121);
                    o.VERBUNDEN = true; o.ZeigeBereiche(); o.ZeigeSpeicher();
                    o.Log(T("Selbsttest: Fenster aufgebaut.", "Self-test: window built.")); o.Log(T("Verbunden.", "Connected.")); o.Log(K.Funde.Count + T(" Einträge gefunden. Nichts wurde verändert.", " entries found. Nothing was changed."));
                    var sp = (StackPanel)o.W.FindName("spBereiche");
                    Schreib("Fenster aufgebaut: " + sp.Children.Count + " Bereiche, " + o.Kaestchen.Count + " Einträge");

                    // Häkchen-Probe: alle Kästchen des SICHTBAREN 6. Bereichs anklicken
                    var ex6 = (Expander)sp.Children[Array.IndexOf(Kern.ALLE_KAT, 6)];
                    int soll = 0;
                    foreach (var c in ((StackPanel)ex6.Content).Children.OfType<CheckBox>()) if (c.IsEnabled) { soll++; Klick(c, true); }
                    int sechs = K.Funde.Count(f => f.Wahl && f.Kat == 6), falsch = K.Funde.Count(f => f.Wahl && f.Kat != 6);
                    int anzeige = 0;
                    for (int b = 0; b < sp.Children.Count; b++)
                    {
                        if (Kern.ALLE_KAT[b] == 6) continue;
                        foreach (var c in ((StackPanel)((Expander)sp.Children[b]).Content).Children.OfType<CheckBox>()) if (c.IsChecked == true) anzeige++;
                    }
                    hakenOk = soll > 0 && sechs == soll && falsch == 0 && anzeige == 0;
                    Schreib("Häkchen-Probe: Bereich 6 angeklickt=" + soll + ", davon gewählt=" + sechs + ", andere gewählt=" + falsch + ", andere angezeigt=" + anzeige + " -> " + (hakenOk ? "OK" : "FEHLER"));
                    foreach (var f in K.Funde) f.Wahl = false; o.ZeigeHaken();

                    // Teil-Probe: Spiel anklicken -> Update/DLC mit (angehakt, gesperrt); wieder ab -> frei
                    var ex1 = (StackPanel)((Expander)sp.Children[Array.IndexOf(Kern.ALLE_KAT, 1)]).Content;
                    CheckBox basisCb = null; var teilCbs = new List<CheckBox>();
                    foreach (var c in ex1.Children.OfType<CheckBox>())
                    {
                        var f = (Eintrag)c.Tag;
                        if (f.Text.StartsWith("Bloodborne") && f.IstTeil("Spiel")) basisCb = c; else if (f.Text.StartsWith("Bloodborne")) teilCbs.Add(c);
                    }
                    Klick(basisCb, true);
                    int mit = teilCbs.Count(c => ((Eintrag)c.Tag).Wahl && c.IsChecked == true && !c.IsEnabled);
                    Klick(basisCb, false);
                    int frei = teilCbs.Count(c => !((Eintrag)c.Tag).Wahl && c.IsChecked != true && c.IsEnabled);
                    Klick(teilCbs[0], true);
                    bool einzeln = ((Eintrag)teilCbs[0].Tag).Wahl && !((Eintrag)basisCb.Tag).Wahl && !((Eintrag)teilCbs[1].Tag).Wahl;
                    teilOk = teilCbs.Count == 2 && mit == 2 && frei == 2 && einzeln;
                    Schreib("Teil-Probe: mitgenommen=" + mit + "/2, wieder frei=" + frei + "/2, Update einzeln=" + einzeln + " -> " + (teilOk ? "OK" : "FEHLER"));
                    foreach (var f in K.Funde) f.Wahl = false; o.ZeigeHaken();

                    // Knopf-Probe: Löschen geht nur mit gültiger Liste der verbundenen PS5
                    var cbTest = (CheckBox)o.W.FindName("cbTest"); var btK = (Button)o.W.FindName("btKomplett"); var btA = (Button)o.W.FindName("btAuswahl");
                    cbTest.IsChecked = false;
                    o.VERBUNDEN = true; o.Knoepfe(true); bool mitListe = btK.IsEnabled;
                    o.VERBUNDEN = false; o.Knoepfe(true); bool ohneListe = !btK.IsEnabled && !btA.IsEnabled;
                    cbTest.IsChecked = true; o.Knoepfe(true); bool test = !btK.IsEnabled;
                    knopfOk = mitListe && ohneListe && test;
                    Schreib("Knopf-Probe: verbunden=" + mitListe + ", ohne gültige Liste gesperrt=" + ohneListe + ", Testmodus gesperrt=" + test + " -> " + (knopfOk ? "OK" : "FEHLER"));

                    // SM-Probe: KOMPLETT abgebrochen, dann "nur Kacheln" -> keine Spieldatei mehr
                    var smf = K.Funde.First(f => f.Kat == 1 && f.Quelle != "" && !f.Gesperrt);
                    smf.Pfade = new List<string> { "/user/app/PPSA30528" }; smf.PfadeBasis = new List<string>(smf.Pfade); smf.Quelle = "/user/data/x.exfat";
                    K.DL_NAMEN = new List<string>();   // 1.0.7 Fix: ohne PS5 kein /user/download (sonst bricht der Selbsttest seit 1.0.6 hier ab)
                    K.WaehleKomplett(false, false); foreach (var f in K.Funde) f.Wahl = false;
                    smf.Wahl = true; K.SetzeSmSpieldateien(false);
                    smOk = !smf.Pfade.Contains("/user/data/x.exfat") && smf.Text == smf.TextBasis + Kern.SM_OHNE;
                    K.SmZuruecksetzen(); foreach (var f in K.Funde) f.Wahl = false; o.ZeigeHaken();
                    Schreib("SM-Probe: nach abgebrochenem KOMPLETT 'nur Kacheln' ohne Spieldatei -> " + (smOk ? "OK" : "FEHLER"));

                    // Schließ-Probe: Fenster wird mitten im Zählen geschlossen -> keine Frage,
                    // kein Fehler, kein Löschen. (Zählen und Löschen hier nur nachgestellt.)
                    var altZaehlen = o.Zaehlen; bool geloescht = false; string fehler = "";
                    o.Zaehlen = () => { o.FENSTER_ZU = true; return new Groesse(); };
                    o.LoescheJetztErsatz = () => { geloescht = true; };
                    foreach (var f in K.Funde) f.Wahl = f.Kat == 5 && f.Ort == Kern.ORT_INTERN;   // 1.0.7 Fix: ohne M.2-Einträge (seit 1.0.6 in Bereich 5) - sonst wartet die M.2-Frage auf einen Klick
                    try { o.AuswahlAblauf(); } catch (Exception ex) { fehler = ex.Message; }
                    bool fragen = !o.FrageWahl("x", "x", "a", "b") && o.FrageText("x", "x") == null && !o.FrageJaNein("x", "x");
                    o.Zaehlen = altZaehlen; o.LoescheJetztErsatz = null; o.FENSTER_ZU = false;
                    // M.2 in der Auswahl -> eigene Liste stimmt, Abschlussfrage warnt oben
                    foreach (var f in K.Funde) f.Wahl = f.Ort == Kern.ORT_M2 || f.Kat == 5;
                    var m2 = K.ExtGewaehlt();   // 1.0.6: statt Bereich 7 der Ort
                    bool m2Zahl = m2.Count == K.Funde.Count(f => f.Ort == Kern.ORT_M2 && f.Quelle == "") && m2.All(f => f.Ort == Kern.ORT_M2);
                    bool m2Warn = o.Zusammenfassung(false).StartsWith("!!! ") && o.Zusammenfassung(false).Contains("M.2");
                    foreach (var f in K.Funde) f.Wahl = false; o.ZeigeHaken();
                    schliessOk = !geloescht && fehler == "" && fragen && m2Zahl && m2Warn;
                    Schreib("Schließ-Probe: gelöscht=" + geloescht + ", Fehler='" + fehler + "', Fragen sicher=" + fragen + ", M.2-Warnung=" + m2Warn + ", M.2-Anzahl richtig=" + m2Zahl + " -> " + (schliessOk ? "OK" : "FEHLER"));

                    // Größen-Probe (NEU): bekannte Größe erscheint am Eintrag und in der Kopfzeile
                    // 1.0.6: Bereich 5 (Reste) mit den M.2-Dateien statt Bereich 7
                    var g1 = K.Funde.First(f => f.Ort == Kern.ORT_M2 && f.Quelle == "");
                    K.GroesseSetzen(g1.PfadeBasis[0], new Groesse { Bytes = 18738184192L, Dateien = 1 });
                    o.ZeigeBereiche();
                    var cb7 = o.Kaestchen.First(c => c.Tag == g1);
                    string inhalt = new TextRange(cb7);
                    var ex7 = (Expander)sp.Children[Array.IndexOf(Kern.ALLE_KAT, g1.Kat)];
                    bool kopfOhneSumme = !((CheckBox)ex7.Header).Content.ToString().Contains("GB");   // andere Einträge des Bereichs noch ungezählt
                    var rest = K.Funde.Where(f => f.Kat == g1.Kat && f != g1).ToList();
                    foreach (var g2 in rest) K.GroesseSetzen(g2.PfadeBasis[0], new Groesse { Bytes = 10385096704L, Dateien = 1 });
                    o.ZeigeBereiche();
                    bool kopfSumme = ((CheckBox)((Expander)sp.Children[Array.IndexOf(Kern.ALLE_KAT, g1.Kat)]).Header).Content.ToString().Contains(Kern.GroesseText(18738184192L + rest.Count * 10385096704L));
                    groesseOk = inhalt.Contains(Kern.GroesseText(18738184192L)) && kopfOhneSumme && kopfSumme;
                    Schreib("Größen-Probe: Eintrag zeigt '" + inhalt.Substring(Math.Max(0, inhalt.Length - 12)).Trim() + "', Kopf erst ohne Summe=" + kopfOhneSumme + ", dann mit Summe=" + kopfSumme + " -> " + (groesseOk ? "OK" : "FEHLER"));

                    // Update-Probe (NEU): Versionsvergleich
                    updateOk = Oberflaeche.Neuer("v1.0.5-beta", "1.0.4 Beta") && !Oberflaeche.Neuer("v1.0.4-beta", "1.0.4 Beta") && !Oberflaeche.Neuer("v1.0.3-beta", "1.0.4 Beta") && Oberflaeche.Neuer("v1.1-beta", "1.0.4 Beta");
                    Schreib("Update-Probe: Versionsvergleich -> " + (updateOk ? "OK" : "FEHLER"));
                    // NEU 1.0.7 (da_Chris86 31.10.2026): PS4-Spieleschutz und Prüfung der Datenbank-Sicherung (ohne PS5)
                    ps4Ok = Ps4Probe();
                    dbOk = DbProbe();
                    // NEU 1.0.7 (da_Chris86 31.10.2026, Entscheidung 2+3): echte SQLite-Prüfung und die ganze Sicherung ohne PS5 - in einem
                    // eigenen Temp-Ordner, der danach wieder entfernt wird (nur er)
                    {
                        string tmp = Path.Combine(Path.GetTempPath(), "PS5-Cleanup-Selbsttest-" + Guid.NewGuid().ToString("N"));
                        try
                        {
                            Directory.CreateDirectory(tmp);
                            sqliteOk = SqliteProbe(Path.Combine(tmp, "sqlite"));
                            sicherungOk = sqliteOk && SicherungProbe(K, Path.Combine(tmp, "sicherung"));
                            if (!sqliteOk) Schreib("Sicherung-Probe: übersprungen (SQLite-Probe fehlgeschlagen) -> FEHLER");
                        }
                        catch (Exception ex) { Schreib("SQLite-/Sicherung-Probe FEHLER: " + ex.Message + " | " + ex.StackTrace); }
                        finally
                        {
                            K.DbBereitschaftSetzen(Kern.DbStand.Bereit, "");
                            try { if (Directory.Exists(tmp)) Directory.Delete(tmp, true); }
                            catch (Exception ex) { Schreib("  (Temp-Ordner der Probe ließ sich nicht entfernen: " + tmp + " - " + ex.Message + ")"); }
                        }
                    }
                    // NEU 1.0.7 (da_Chris86 31.10.2026, Entscheidung 1): Datenbank nicht lesbar -> Löschen gesperrt, Nachschauen frei
                    sperreOk = SperreProbe(o, sp);
                    // NEU 1.0.7 (da_Chris86 31.10.2026 23:32): Apps/Spiele mit App-Ordner (Kachel) sind in Windows gesperrt - sonst Geisterkachel
                    {
                        Func<string, Eintrag> e1 = p => new Eintrag { Pfade = new List<string> { p }, PfadeBasis = new List<string> { p }, Text = p, TextBasis = p };
                        var kl = new List<Eintrag> { e1("/user/app/PPSA00001"), e1("/mnt/ext1/user/app/CUSA00002"), e1("/mnt/ext0/user/app/PPSA00003"),
                                                     e1("/user/patch/PPSA00001"), e1("/user/appmeta/PPSA00004"), e1("/user/app/PPSA00001/sce_sys"), e1("/mnt/ext1/homebrew/Spiel - PPSA00005.exfat") };
                        var sm = e1("/user/app/PPSA00006"); sm.Quelle = "/data/homebrew/x.exfat"; sm.Pfade = new List<string> { "/data/homebrew/x.exfat", "/user/app/PPSA00006" }; kl.Add(sm);
                        kl[0].Wahl = true;
                        Kern.KachelnSperren(kl);
                        bool gesperrtRichtig = kl[0].Gesperrt && kl[1].Gesperrt && kl[2].Gesperrt && sm.Gesperrt && !kl[0].Wahl && kl[0].SperrGrund != null && kl[0].SperrGrund.Contains(T("Geisterkachel", "ghost tile"));
                        bool freiRichtig = !kl[3].Gesperrt && !kl[4].Gesperrt && !kl[5].Gesperrt && !kl[6].Gesperrt;
                        sperreOk = sperreOk && gesperrtRichtig && freiRichtig;
                        Schreib("Kachel-Sperre (Windows): App-Ordner gesperrt=" + gesperrtRichtig + ", Updates/Kachel-Reste/Abbilder frei=" + freiRichtig + " -> " + (gesperrtRichtig && freiRichtig ? "OK" : "FEHLER"));
                    }
                    Schreib("ps5info.elf eingebaut: " + (Programm.Ressource("ps5info.elf") != null) + ", LIESMICH eingebaut: " + (Programm.Ressource("LIESMICH.txt") != null) + ", README_EN eingebaut: " + (Programm.Ressource("README_EN.txt") != null));

                    // Fertig-Probe (NEU): Schlussfenster mit sauber eingerückten Punkten
                    // NEU 1.0.7 (da_Chris86 31.10.2026): Schlusstext ohne "Datenbank neu aufbauen" als Schritt, mit den gefährdeten PS4-Spielen
                    var gefaehrdet = new List<Ps4Spiel> { new Ps4Spiel { Id = "CUSA00900", Name = "Bloodborne", Basis = "/user", Ort = Kern.ORT_INTERN, AppPkg = true, AppGelesen = true },
                                                          new Ps4Spiel { Id = "CUSA07408", Name = "CUSA07408", Basis = "/mnt/ext0/user", Ort = Kern.ORT_USB, AppPkg = true, AppGelesen = true } };
                    var schluss = Kern.SchlussText(true, gefaehrdet, 10);
                    var fw = Oberflaeche.FertigFenster(T("Fertig - alles Ausgewählte ist gelöscht.", "Done - everything selected has been deleted."), true,
                        Kern.FreigeraeumtText(new Groesse { Bytes = 12240656794L, Dateien = 528, Ordner = 177 }),
                        T("ShadowMount wurde mitgelöscht - richte es neu ein, dann erscheinen die Spiele auf der M.2 wieder als Kacheln.", "ShadowMount was deleted too - set it up again and the games on the M.2 will show up as tiles again."), "F:\\PS5\\PS5-Cleanup.log", schluss);
                    fw.ShowActivated = false; fw.WindowStartupLocation = WindowStartupLocation.Manual; fw.Left = -20000; fw.Top = -20000;
                    fw.Show(); fw.UpdateLayout();
                    var gs = (System.Windows.Controls.Grid)fw.FindName("gSchritte");
                    var twi = (System.Windows.Controls.TextBlock)fw.FindName("tWichtig");
                    int nummern = schluss.Count(z => System.Text.RegularExpressions.Regex.IsMatch(z, "^\\d+\\."));
                    string alles = string.Join("\n", schluss) + "\n" + string.Join("\n", Kern.SchlussText(false, null, 10));
                    bool ohneAufbau = !alles.Contains("Cache leeren") && !alles.Contains("Clear Cache") && !alles.Contains("piept") && !alles.Contains("beeps");
                    bool spieleDrin = alles.Contains("Bloodborne (CUSA00900, ") && alles.Contains("CUSA07408 (") && !alles.Contains("CUSA07408 (CUSA07408");
                    fertigOk = nummern == 5 && gs.RowDefinitions.Count == nummern && gs.Children.Count == 2 * nummern && twi.Text == schluss[schluss.Length - 1] && ohneAufbau && spieleDrin;
                    Schreib("Fertig-Probe: Punkte=" + gs.RowDefinitions.Count + "/" + nummern + ", Warnzeile=" + (twi.Text != "") + ", ohne Datenbank-Schritte=" + ohneAufbau + ", PS4-Spiele genannt=" + spieleDrin + " -> " + (fertigOk ? "OK" : "FEHLER"));
                    if (bild != "")
                    {
                        var rf = new RenderTargetBitmap((int)fw.ActualWidth, (int)fw.ActualHeight, 96, 96, PixelFormats.Pbgra32);
                        rf.Render(fw);
                        var ef = new PngBitmapEncoder(); ef.Frames.Add(BitmapFrame.Create(rf));
                        string fb = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(bild)), "_fertig.png");
                        using (var fs = File.Create(fb)) ef.Save(fs);
                        Schreib("Bild gespeichert: " + fb);
                    }
                    fw.Close();

                    // Feedback-Probe (NEU 1.0.5): Bericht mit echten Angaben, ohne IP, FTP-Server "Unbekannt"
                    bool feedbackOk = false;
                    {
                        o.VERBUNDEN = true; o.F_SCAN = true;
                        var fd = o.FeedbackSammeln();
                        string fb = Feedback.Bericht(fd);
                        string unb = T("Unbekannt", "Unknown");
                        feedbackOk = fb.Contains("CFI-7021 B01Y") && fb.Contains("12.00") && fb.Contains(":** 1\n") && fb.Contains((Sp.EN ? "FTP server:** " : "FTP-Server:** ") + unb)
                                     && !fb.Contains("192.168") && (Environment.UserName.Length < 3 || fb.IndexOf(Environment.UserName, StringComparison.OrdinalIgnoreCase) < 0);
                        var ffw = o.FeedbackFenster(fd);
                        ffw.ShowActivated = false; ffw.WindowStartupLocation = WindowStartupLocation.Manual; ffw.Left = -20000; ffw.Top = -20000;
                        ffw.Show(); ffw.UpdateLayout();
                        if (bild != "")
                        {
                            var rf = new RenderTargetBitmap((int)ffw.ActualWidth, (int)ffw.ActualHeight, 96, 96, PixelFormats.Pbgra32);
                            rf.Render(ffw);
                            var ef = new PngBitmapEncoder(); ef.Frames.Add(BitmapFrame.Create(rf));
                            string fbb = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(bild)), "_feedback" + (Sp.EN ? "_en" : "_de") + ".png");
                            using (var fs = File.Create(fbb)) ef.Save(fs);
                            Schreib("Bild gespeichert: " + fbb);
                        }
                        ffw.Close();
                        o.VERBUNDEN = false;
                        Schreib("Feedback-Probe: Modell/Firmware/Rückgabewert drin, FTP-Server unbekannt, keine IP, kein Benutzername -> " + (feedbackOk ? "OK" : "FEHLER"));
                        Schreib("Windows erkannt: " + Programm.WindowsVersion());
                    }
                    // Protokoll-Probe (NEU 1.0.5): "Alles kopieren" nimmt den ganzen Inhalt, eigenes Fenster läuft mit
                    bool protokollOk = false;
                    try
                    {
                        for (int i = 1; i <= 300; i++) o.Log("Probe-Zeile " + i);
                        var tbLogP = (TextBox)o.W.FindName("tbLog");
                        string voll = tbLogP.Text;
                        string kopie = "";
                        try { ((Button)o.W.FindName("btLogKopie")).RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent)); kopie = Clipboard.GetText(); } catch (Exception ex) { Schreib("  (Zwischenablage: " + ex.Message + ")"); }
                        bool kopieOk = kopie == voll && kopie.Contains("Probe-Zeile 1\r\n") && kopie.Contains("Probe-Zeile 300");
                        ((Button)o.W.FindName("btLogFenster")).RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                        var lf = Application.Current.Windows.OfType<Window>().FirstOrDefault(x => x != o.W && x.FindName("tLog") != null);
                        bool fensterOk = false;
                        if (lf != null)
                        {
                            o.Log("Live-Zeile");
                            var tl = (TextBox)lf.FindName("tLog");
                            fensterOk = tl.Text.Contains("Probe-Zeile 1\r\n") && tl.Text.Contains("Live-Zeile");
                            if (bild != "")
                            {
                                lf.UpdateLayout();
                                var rl = new RenderTargetBitmap((int)lf.ActualWidth, (int)lf.ActualHeight, 96, 96, PixelFormats.Pbgra32); rl.Render(lf);
                                var el = new PngBitmapEncoder(); el.Frames.Add(BitmapFrame.Create(rl));
                                using (var fs = File.Create(Path.Combine(Path.GetDirectoryName(Path.GetFullPath(bild)), "_protokollfenster" + (Sp.EN ? "_en" : "_de") + ".png"))) el.Save(fs);
                            }
                            lf.Close();
                        }
                        protokollOk = kopieOk && fensterOk;
                        Schreib("Protokoll-Probe: Alles kopieren = ganzer Inhalt " + kopieOk + ", eigenes Fenster läuft mit " + fensterOk + " -> " + (protokollOk ? "OK" : "FEHLER"));
                        tbLogP.Clear();
                        o.Log(T("Selbsttest: Fenster aufgebaut.", "Self-test: window built.")); o.Log(T("Verbunden.", "Connected.")); o.Log(K.Funde.Count + T(" Einträge gefunden. Nichts wurde verändert.", " entries found. Nothing was changed."));
                    }
                    catch (Exception ex) { Schreib("Protokoll-Probe FEHLER: " + ex.Message); }
                    Schreib("Fenster " + (int)o.W.ActualWidth + "x" + (int)o.W.ActualHeight + ", Vergrößerung " + o.Zoom.ToString("0.00"));
                    // NEU 1.0.5: jeder Eintrag zeigt seinen vollständigen Namen als Infofeld; Vergrößerung höchstens 1,3
                    bool tippOk;
                    {
                        int mitTipp = o.Kaestchen.Count(c => { var tt = c.ToolTip as System.Windows.Controls.ToolTip; var tb = tt != null ? tt.Content as TextBlock : null; return tb != null && tb.Text.StartsWith(((Eintrag)c.Tag).Text); });
                        bool zoomOk = o.Zoom <= Oberflaeche.MAX_ZOOM + 0.001;
                        Schreib("Infofeld-Probe: " + mitTipp + "/" + o.Kaestchen.Count + " Einträge mit vollem Namen, Vergrößerung " + o.Zoom.ToString("0.00") + " <= " + Oberflaeche.MAX_ZOOM.ToString("0.0") + " -> " + (mitTipp == o.Kaestchen.Count && o.Kaestchen.Count > 0 && zoomOk ? "OK" : "FEHLER"));
                        tippOk = mitTipp == o.Kaestchen.Count && o.Kaestchen.Count > 0 && zoomOk;
                    }
                    // Bild vom Hauptfenster im verbundenen Zustand (Testmodus)
                    o.VERBUNDEN = true; foreach (var f in K.Funde) f.Wahl = false; o.ZeigeBereiche(); o.ZeigeSpeicher(); o.Knoepfe(true);
                    if (bild != "")
                    {
                        o.W.UpdateLayout();
                        var rtb = new RenderTargetBitmap((int)o.W.ActualWidth, (int)o.W.ActualHeight, 96, 96, PixelFormats.Pbgra32);
                        rtb.Render(o.W);
                        var enc = new PngBitmapEncoder(); enc.Frames.Add(BitmapFrame.Create(rtb));
                        using (var fs = File.Create(bild)) enc.Save(fs);
                        Schreib("Bild gespeichert: " + bild);
                    }
                    Schreib(hakenOk && teilOk && knopfOk && smOk && schliessOk && groesseOk && updateOk && fertigOk && feedbackOk && protokollOk && tippOk && ps4Ok && dbOk
                            && sqliteOk && sicherungOk && sperreOk ? "SELBSTTEST OK" : "SELBSTTEST FEHLER (siehe Probe oben)");
                }
                catch (Exception ex) { Schreib("FEHLER: " + ex.Message + " | " + ex.StackTrace); }
                o.W.Dispatcher.BeginInvoke(DispatcherPriority.Background, new Action(() => o.W.Close()));
            };
        }

        // NEU 1.0.7 (da_Chris86 31.10.2026): PS4-Spieleschutz ohne PS5 - Zuordnung Pfad -> installiertes Spiel und Entscheidung.
        // Gewählt: Bloodborne (intern) und Spiel C nur intern (zweite Kopie auf USB bleibt); von Spiel B (M.2) nur das Update.
        static bool Ps4Probe()
        {
            var spiele = new List<Ps4Spiel> {
                new Ps4Spiel { Id = "CUSA00900", Name = "Bloodborne", Basis = "/user", Ort = Kern.ORT_INTERN, AppPkg = true, AppGelesen = true,
                               Addcont = new List<string> { "/user/addcont/CUSA00900" }, Lizenzen = new List<string> { "/user/license/fakeEP9000-CUSA00900_00.rif" } },
                new Ps4Spiel { Id = "CUSA01234", Name = "Spiel B", Basis = "/mnt/ext1/user", Ort = Kern.ORT_M2, AppPkg = true, AppGelesen = true, Patch = true,
                               Lizenzen = new List<string> { "/user/license/fakeEP9000-CUSA01234_00.rif", "/user/license/EP9000-CUSA01234_00-ABCDEF0123456789.rif" } },
                new Ps4Spiel { Id = "CUSA05555", Name = "Spiel C", Basis = "/user", Ort = Kern.ORT_INTERN, AppPkg = true, AppGelesen = true },
                new Ps4Spiel { Id = "CUSA05555", Name = "Spiel C", Basis = "/mnt/ext0/user", Ort = Kern.ORT_USB, AppPkg = true, AppGelesen = true } };
            var eintraege = new List<Eintrag> {
                new Eintrag { Wahl = true, Teil = "Spiel", Gruppe = "/user|CUSA00900", Pfade = new List<string> { "/user/app/CUSA00900", "/user/appmeta/CUSA00900" } },
                new Eintrag { Wahl = true, Teil = "Update", Gruppe = "/mnt/ext1/user|CUSA01234", Pfade = new List<string> { "/mnt/ext1/user/patch/CUSA01234" } },
                new Eintrag { Wahl = false, Teil = "Spiel", Gruppe = "/mnt/ext1/user|CUSA01234", Pfade = new List<string> { "/mnt/ext1/user/app/CUSA01234" } },
                new Eintrag { Wahl = true, Teil = "Spiel", Gruppe = "/user|CUSA05555", Pfade = new List<string> { "/user/app/CUSA05555" } } };
            var los = Kern.Ps4Mitgeloescht(eintraege, spiele);
            bool auswahl = los.Count == 2 && los.Contains("/user|CUSA00900") && los.Contains("/user|CUSA05555");
            Func<string, Ps4Treffer> z = p => Kern.Ps4Zuordnung(p, spiele, los);
            Func<string, bool> erlaubt = p => { var t = z(p); return t != null && t.Erlaubt; };
            Func<string, bool> geschuetzt = p => { var t = z(p); return t != null && !t.Erlaubt; };
            Func<string, bool> unklar = p => { var t = z(p); return t != null && !t.Erlaubt && t.Unklar; };
            Func<string, bool> frei = p => z(p) == null;
            // gewähltes Spiel -> seine Daten dürfen weg
            bool gewaehlt = erlaubt("/user/app/CUSA00900") && erlaubt("/user/app/CUSA00900/app.pkg") && erlaubt("/user/patch/CUSA00900") && erlaubt("/user/addcont/CUSA00900/DLC1")
                         && erlaubt("/user/appmeta/CUSA00900") && erlaubt("/system_data/priv/appmeta/CUSA00900") && erlaubt("/user/license/fakeEP9000-CUSA00900_00.rif")
                         && erlaubt("/user/playgo/0/EP9000-CUSA00900_00-PLAYGO") && erlaubt("/user/app/CUSA05555");
            // nicht gewähltes Spiel -> geschützt (auch sein Update allein und die zweite Kopie auf USB)
            bool nichtGewaehlt = geschuetzt("/mnt/ext1/user/app/CUSA01234") && geschuetzt("/mnt/ext1/user/patch/CUSA01234") && geschuetzt("/user/appmeta/CUSA01234")
                              && geschuetzt("/system_data/priv/appmeta/CUSA01234") && geschuetzt("/mnt/ext0/user/app/CUSA05555") && geschuetzt("/user/appmeta/CUSA05555")
                              && geschuetzt("/mnt/ext1/user");
            // unklare Zuordnung -> geschützt (Update intern, Spiel auf M.2; unbekannter Aufbau; Lizenz mit zwei Kennungen)
            bool unklarOk = unklar("/user/patch/CUSA01234") && unklar("/user/addcont/CUSA01234") && unklar("/user/appmeta/external/CUSA00900")
                         && unklar("/user/license/fakeEP9000-CUSA00900_00-CUSA01234.idx");
            // Lizenzen anderer Spiele bleiben unangetastet (Fake und echt)
            bool lizenzen = geschuetzt("/user/license/fakeEP9000-CUSA01234_00.rif") && geschuetzt("/user/license/fakeEP9000-CUSA01234_00.idx")
                         && geschuetzt("/user/license/EP9000-CUSA01234_00-ABCDEF0123456789.rif");
            // keine Spieldaten bzw. nicht installiert -> bisherige Regeln (Spielstände, Aufnahmen, PS5-Spiele, Reste)
            bool sonst = frei("/user/home/10000000/savedata/CUSA00900") && frei("/user/av_contents/photo/x/CUSA01234") && frei("/user/app/CUSA99999")
                      && frei("/user/license/fakeEP9000-CUSA99999_00.rif") && frei("/user/app/PPSA01234") && frei("/mnt/ext1/homebrew/Spiel-CUSA01234.exfat");
            bool ok = auswahl && gewaehlt && nichtGewaehlt && unklarOk && lizenzen && sonst;
            Schreib("PS4-Schutz-Probe: Auswahl=" + auswahl + ", gewähltes Spiel erlaubt=" + gewaehlt + ", nicht gewähltes geschützt=" + nichtGewaehlt + ", unklar geschützt=" + unklarOk
                    + ", fremde Lizenzen geschützt=" + lizenzen + ", Übriges unverändert=" + sonst + " -> " + (ok ? "OK" : "FEHLER"));
            return ok;
        }

        // NEU 1.0.7 (da_Chris86 31.10.2026): Prüfung der Datenbank-Sicherung mit künstlichen SQLite-Köpfen
        static byte[] DbKopf(int seiteRoh, uint seiten, int laenge, bool kopfGueltig)
        {
            var b = new byte[laenge];
            var magie = Encoding.ASCII.GetBytes("SQLite format 3\0");
            Array.Copy(magie, b, Math.Min(16, laenge));
            if (laenge < 100) return b;
            b[16] = (byte)(seiteRoh >> 8); b[17] = (byte)(seiteRoh & 0xFF);
            b[27] = 7;                                   // Änderungszähler
            b[95] = (byte)(kopfGueltig ? 7 : 6);         // "gültig für Version"
            b[28] = (byte)(seiten >> 24); b[29] = (byte)(seiten >> 16); b[30] = (byte)(seiten >> 8); b[31] = (byte)seiten;
            return b;
        }
        static bool DbProbe()
        {
            var gut = Kern.SqliteKopfPruefen(DbKopf(4096, 3, 12288, true));
            bool gueltig = gut.Ok && gut.SeitenGroesse == 4096 && gut.Seiten == 3 && gut.KopfGroesseGueltig;
            var falsch = DbKopf(4096, 3, 12288, true); falsch[0] = (byte)'X';
            bool magie = !Kern.SqliteKopfPruefen(falsch).Ok;
            bool groesse = !Kern.SqliteKopfPruefen(DbKopf(4096, 4, 12288, true)).Ok;
            bool nichtVerglichen = Kern.SqliteKopfPruefen(DbKopf(4096, 4, 12288, false)).Ok;   // Größe im Kopf ungültig -> nicht verglichen
            var gross = Kern.SqliteKopfPruefen(DbKopf(1, 2, 131072, true));
            bool seite1 = gross.Ok && gross.SeitenGroesse == 65536;
            bool kurz = !Kern.SqliteKopfPruefen(new byte[50]).Ok && !Kern.SqliteKopfPruefen(null).Ok && !Kern.SqliteKopfPruefen(DbKopf(1000, 3, 3000, true)).Ok;
            bool ok = gueltig && magie && groesse && nichtVerglichen && seite1 && kurz;
            Schreib("Datenbank-Sicherung-Probe: gültiger Kopf=" + gueltig + ", falsche Kennung erkannt=" + magie + ", Größenfehler erkannt=" + groesse + ", ungültige Kopfgröße nicht verglichen=" + nichtVerglichen
                    + ", Seitengröße 1 = 65536=" + seite1 + ", zu kurz/ungültige Seitengröße erkannt=" + kurz + " -> " + (ok ? "OK" : "FEHLER"));
            return ok;
        }

        // NEU 1.0.7 (da_Chris86 31.10.2026, Entscheidung 1): "Datenbank nicht lesbar" gesetzt -> beide Lösch-Knöpfe gesperrt (Grund im
        // Infofeld, in der Statuszeile und im gelben Hinweis), Suchen/Verbinden/Anhaken bleiben frei; die Abläufe fragen nichts, zählen
        // nichts, löschen nichts; FuehreLoeschung bricht vor allem anderen ab. Danach wieder freigeben -> Knöpfe wieder frei.
        static bool SperreProbe(Oberflaeche o, StackPanel sp)
        {
            var K = o.K;
            Func<string, string, string> T = Sp.T;
            var cbT = (CheckBox)o.W.FindName("cbTest"); var btKo = (Button)o.W.FindName("btKomplett"); var btAu = (Button)o.W.FindName("btAuswahl");
            var btSu = (Button)o.W.FindName("btSuchen"); var btVe = (Button)o.W.FindName("btVerbinden");
            var tbH = (TextBlock)o.W.FindName("tbHinweis"); var bdH = (Border)o.W.FindName("bdHinweis"); var tbSt = (TextBlock)o.W.FindName("tbStand");
            var tbLogS = (TextBox)o.W.FindName("tbLog");
            bool altVerb = o.VERBUNDEN; bool? altTest = cbT.IsChecked; var altZaehlen = o.Zaehlen;
            bool knoepfeZu = false, suchenFrei = false, hinweisDa = false, ablaeufeZu = false, wiederFrei = false;
            string grund = T("Löschen ist gesperrt: Selbsttest - der FTP-Server kann die Datenbank nicht lesen.", "Deleting is locked: self-test - the FTP server cannot read the database.");
            string kopf = T("LÖSCHEN GESPERRT", "DELETING LOCKED");
            try
            {
                K.Abbruch = false;
                K.DbBereitschaftSetzen(Kern.DbStand.Gesperrt, grund);
                cbT.IsChecked = false; o.VERBUNDEN = true;
                foreach (var f in K.Funde) f.Wahl = false;
                K.Funde.First(f => f.Kat == 5 && f.Ort == Kern.ORT_INTERN).Wahl = true;
                o.ZeigeHaken(); o.ZeigeSpeicher(); o.Knoepfe(true); o.AktualisiereStand();
                knoepfeZu = !btKo.IsEnabled && !btAu.IsEnabled && Convert.ToString(btKo.ToolTip) == grund && Convert.ToString(btAu.ToolTip) == grund && ToolTipService.GetShowOnDisabled(btKo);
                suchenFrei = btSu.IsEnabled && btVe.IsEnabled && sp.IsEnabled && o.Kaestchen.Any(c => c.IsEnabled);
                hinweisDa = bdH.Visibility == Visibility.Visible && tbH.Text.StartsWith(grund) && tbSt.Text.StartsWith(kopf);
                // die Abläufe selbst (falls ein Knopf doch erreichbar wäre) und der Kern: keine Frage, kein Zählen, kein Löschen
                int logVorher = tbLogS.Text.Length; bool geloescht = false, gezaehlt = false;
                o.TestAntworten = new Queue<object>(); o.LoescheJetztErsatz = () => { geloescht = true; };
                o.Zaehlen = () => { gezaehlt = true; return new Groesse(); };
                o.AuswahlAblauf(); o.KomplettAblauf();
                var erg = K.FuehreLoeschung(() => true);    // ohne Verbindung käme sonst "Liste stammt nicht von der PS5" - die Sperre muss vorher greifen
                string neu = tbLogS.Text.Substring(logVorher);
                ablaeufeZu = !geloescht && !gezaehlt && !neu.Contains("[Test] Frage") && !neu.Contains("[Test] Auswahl") && neu.Contains(grund)
                             && erg.Abgebrochen && erg.DbGesperrt && erg.SicherungGrund == grund && K.Funde.Count(f => f.Wahl) == 1;
                // wieder freigeben
                K.DbBereitschaftSetzen(Kern.DbStand.Bereit, "");
                o.ZeigeSpeicher(); o.Knoepfe(true); o.AktualisiereStand();
                wiederFrei = btKo.IsEnabled && btAu.IsEnabled && !tbH.Text.Contains(grund) && !tbSt.Text.StartsWith(kopf) && !ToolTipService.GetShowOnDisabled(btKo);
            }
            finally
            {
                o.TestAntworten = null; o.LoescheJetztErsatz = null; o.Zaehlen = altZaehlen;
                K.DbBereitschaftSetzen(Kern.DbStand.Bereit, "");
                foreach (var f in K.Funde) f.Wahl = false;
                cbT.IsChecked = altTest; o.VERBUNDEN = altVerb;
                o.ZeigeHaken(); o.Knoepfe(true); o.AktualisiereStand();
            }
            bool ok = knoepfeZu && suchenFrei && hinweisDa && ablaeufeZu && wiederFrei;
            Schreib("Sperre-Probe: Datenbank nicht lesbar -> Lösch-Knöpfe gesperrt mit Grund=" + knoepfeZu + ", Suchen/Verbinden/Anhaken frei=" + suchenFrei + ", Grund sichtbar=" + hinweisDa
                    + ", Abläufe und Kern löschen nichts=" + ablaeufeZu + ", danach wieder frei=" + wiederFrei + " -> " + (ok ? "OK" : "FEHLER"));
            return ok;
        }

        // NEU 1.0.7 (da_Chris86 31.10.2026, Entscheidung 2+3): echte SQLite-Prüfung ohne PS5 (winsqlite3.dll):
        // gesunde Datenbank -> geprüfte Kopie ok; beschädigte Kopie -> Fehler; WAL-Fall -> Inhalt aus -wal steht in der geprüften Kopie
        static bool SqliteProbe(string ordner)
        {
            string version;
            string bib = Kern.SqliteBibliothek(out version);
            if (bib != null) { Schreib("SQLite-Probe: " + bib + " -> FEHLER"); return false; }
            Directory.CreateDirectory(ordner);
            // 1. gesunde Datenbank wie auf der PS5 (Rollback-Journal, Seitengröße 4096)
            string roh = Path.Combine(ordner, "roh"); Directory.CreateDirectory(roh);
            string db = Path.Combine(roh, "app.db");
            string f = TestDbAnlegen(db, false, 2000, null);
            if (f != null) { Schreib("SQLite-Probe: Test-Datenbank ließ sich nicht anlegen: " + f + " -> FEHLER"); return false; }
            string shaVorher = DateiSha(db);
            string ziel = Path.Combine(ordner, "app_geprueft.db");
            var gut = Kern.GepruefteKopieErstellen(db, ziel);
            bool gutOk = gut.Ok && !gut.MitWal && Zeilen(ziel) == "2000" && DateiSha(db) == shaVorher && !File.Exists(db + "-journal")
                         && !Directory.Exists(gut.ArbeitOrdner) && gut.Hinweis == "";
            // 2. beschädigte Kopie (64 Bytes mitten in einer Blatt-Seite) -> Fehler, keine Datei "_geprueft" bleibt
            string kaputtOrdner = Path.Combine(ordner, "kaputt"); Directory.CreateDirectory(kaputtOrdner);
            string kaputt = Path.Combine(kaputtOrdner, "app.db");
            File.Copy(db, kaputt);
            bool beschaedigt = Beschaedigen(kaputt);
            string kaputtZiel = Path.Combine(ordner, "kaputt_geprueft.db");
            var schlecht = Kern.GepruefteKopieErstellen(kaputt, kaputtZiel);
            bool schlechtOk = beschaedigt && !schlecht.Ok && schlecht.Grund != "" && !File.Exists(kaputtZiel) && !Directory.Exists(schlecht.ArbeitOrdner);
            // 3. WAL: die Zeilen stehen nur in app.db-wal (bei offener Verbindung kopiert) -> müssen in der geprüften Kopie stehen
            string walOrdner = Path.Combine(ordner, "wal"); Directory.CreateDirectory(walOrdner);
            string walRoh = Path.Combine(ordner, "walroh");
            string fw = TestDbAnlegen(Path.Combine(walOrdner, "app.db"), true, 500, walRoh);
            string walDatei = Path.Combine(walRoh, "app.db-wal");
            bool walDa = fw == null && File.Exists(walDatei) && new FileInfo(walDatei).Length > 0;
            string walZiel = Path.Combine(ordner, "wal_geprueft.db");
            SqlitePruefung wal = walDa ? Kern.GepruefteKopieErstellen(Path.Combine(walRoh, "app.db"), walZiel) : null;
            bool walOk = wal != null && wal.Ok && wal.MitWal && Zeilen(walZiel) == "500";
            bool ok = gutOk && schlechtOk && walOk;
            Schreib("SQLite-Probe (winsqlite3 " + version + "): gesunde Datenbank -> geprüfte Kopie mit allen Zeilen, Rohkopie unverändert, Arbeitsordner entfernt=" + gutOk
                    + ", beschädigte Kopie erkannt=" + schlechtOk + ", WAL-Inhalt übernommen=" + walOk + " -> " + (ok ? "OK" : "FEHLER"));
            Schreib("  beschädigt: " + (beschaedigt ? schlecht.Grund : "keine passende Seite gefunden"));
            if (!gut.Ok) Schreib("  gesund: " + gut.Grund);
            if (gut.Hinweis != "") Schreib("  gesund: " + gut.Hinweis);
            if (fw != null) Schreib("  WAL-Datenbank: " + fw);
            if (wal != null && !wal.Ok) Schreib("  WAL: " + wal.Grund);
            return ok;
        }

        // NEU 1.0.7 (da_Chris86 31.10.2026, Entscheidung 2+3): die ganze Sicherung (DatenbankSichern) und die Bereitschaftsprüfung ohne
        // PS5 - ein Ordner auf dem PC spielt /system_data/priv/mms (Ordnerliste und Laden über die Ersatz-Zugänge des Kerns)
        static bool SicherungProbe(Kern K, string ordner)
        {
            Func<string, string, string> T = Sp.T;
            string ps5 = Path.Combine(ordner, "ps5"); Directory.CreateDirectory(ps5);
            string f = TestDbAnlegen(Path.Combine(ps5, "app.db"), false, 1500, null) ?? TestDbAnlegen(Path.Combine(ps5, "appinfo.db"), false, 800, null);
            if (f != null) { Schreib("Sicherung-Probe: Test-Datenbank ließ sich nicht anlegen: " + f + " -> FEHLER"); return false; }
            bool listeKaputt = false, ladenKaputt = false, journalWeg = false; int journalGelesen = 0;
            var altWarte = K.Warte;
            K.Warte = ms => { };
            K.DB_LISTE_ERSATZ = p =>
            {
                if (listeKaputt) throw new IOException("550 Permission denied (Selbsttest)");
                if (p != Kern.MMS) return null;
                return new DirectoryInfo(ps5).GetFiles().Select(x => new Datei { Name = x.Name, Groesse = x.Length }).ToList();
            };
            K.DB_LADEN_ERSATZ = p =>
            {
                if (ladenKaputt || !p.StartsWith(Kern.MMS + "/", StringComparison.Ordinal)) return null;
                string q = Path.Combine(ps5, p.Substring(Kern.MMS.Length + 1));
                if (!File.Exists(q)) return null;
                byte[] b = File.ReadAllBytes(q);
                if (journalWeg && q.EndsWith("-journal", StringComparison.Ordinal) && ++journalGelesen == 2) File.Delete(q);   // die "PS5" ist fertig mit Schreiben
                return b;
            };
            K.SICHERUNG_WURZEL = Path.Combine(ordner, "Datenbank-Sicherung");
            K.FIRMWARE = "12.00"; K.MODELL = "CFI-7016";
            try
            {
                // a) normal: Rohkopien byte-gleich, geprüfte Kopien mit allen Zeilen, info.txt mit den festen Bezeichnungen
                string g1 = K.DatenbankSichern(); string o1 = K.SICHERUNG_ORDNER;
                string info = g1 == null && File.Exists(Path.Combine(o1, "info.txt")) ? File.ReadAllText(Path.Combine(o1, "info.txt")) : "";
                bool normal = g1 == null && K.SICHERUNG_VERSUCHE == 1
                    && DateienGleich(Path.Combine(o1, "app.db"), Path.Combine(ps5, "app.db")) && DateienGleich(Path.Combine(o1, "appinfo.db"), Path.Combine(ps5, "appinfo.db"))
                    && Zeilen(Path.Combine(o1, "app_geprueft.db")) == "1500" && Zeilen(Path.Combine(o1, "appinfo_geprueft.db")) == "800"
                    && info.Contains(Kern.ROH_TEXT) && info.Contains(Kern.GEPRUEFT_TEXT)
                    && info.Contains(T("keine Funktion zum Zurückspielen", "no restore function")) && info.Contains(T("wurde nie getestet", "has never been tested"))
                    && !System.Text.RegularExpressions.Regex.IsMatch(info, "garantiert|guarantee", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
                // b) "heißes" Journal bleibt: 3 Versuche, dann nichts gesichert (die PS5 schreibt)
                byte[] heiss = new byte[512];
                Array.Copy(new byte[] { 0xd9, 0xd5, 0x05, 0xf9, 0x20, 0xa1, 0x63, 0xd7 }, heiss, 8);
                File.WriteAllBytes(Path.Combine(ps5, "app.db-journal"), heiss);
                string g2 = K.DatenbankSichern();
                bool heissBleibt = g2 != null && g2.Contains("journal") && K.SICHERUNG_VERSUCHE == 3 && K.SICHERUNG_ORDNER.EndsWith(T("_FEHLGESCHLAGEN", "_FAILED"))
                                   && !File.Exists(Path.Combine(K.SICHERUNG_ORDNER, "app_geprueft.db")) && Kern.HeissesJournal(heiss) && !Kern.HeissesJournal(new byte[512]);
                // c) "heißes" Journal ist nach dem ersten Versuch weg (die PS5 ist fertig): der zweite Versuch klappt
                journalWeg = true;
                string g3 = K.DatenbankSichern();
                bool heissWeg = g3 == null && K.SICHERUNG_VERSUCHE == 2 && !File.Exists(Path.Combine(ps5, "app.db-journal")) && File.Exists(Path.Combine(K.SICHERUNG_ORDNER, "app_geprueft.db"));
                journalWeg = false;
                // d) beschädigte appinfo.db auf der "PS5": integrity_check schlägt an, nichts gilt als gesichert
                string heil = Path.Combine(ordner, "appinfo_heil.db");
                File.Copy(Path.Combine(ps5, "appinfo.db"), heil);
                bool beschaedigt = Beschaedigen(Path.Combine(ps5, "appinfo.db"));
                string g4 = K.DatenbankSichern();
                bool kaputtErkannt = beschaedigt && g4 != null && g4.Contains("SQLite") && K.SICHERUNG_ORDNER.EndsWith(T("_FEHLGESCHLAGEN", "_FAILED"))
                                     && !File.Exists(Path.Combine(K.SICHERUNG_ORDNER, "appinfo_geprueft.db"));
                File.Copy(heil, Path.Combine(ps5, "appinfo.db"), true);
                // e) Bereitschaft (Entscheidung 1): lesbar -> bereit; Ordner nicht lesbar oder Datei nicht ladbar -> gesperrt mit Grund
                bool bereit = K.DatenbankBereitPruefen() == Kern.DbStand.Bereit && !K.LoeschenGesperrt && K.LoeschSperreText() == "";
                listeKaputt = true;
                bool gesperrt1 = K.DatenbankBereitPruefen() == Kern.DbStand.Gesperrt && K.LoeschenGesperrt && K.LoeschSperreText().Contains("550 Permission denied (Selbsttest)");
                listeKaputt = false; ladenKaputt = true;
                bool gesperrt2 = K.DatenbankBereitPruefen() == Kern.DbStand.Gesperrt && K.LoeschenGesperrt && K.LoeschSperreText().Contains(T("ließ sich nicht laden", "could not be downloaded"));
                ladenKaputt = false;
                bool ok = normal && heissBleibt && heissWeg && kaputtErkannt && bereit && gesperrt1 && gesperrt2;
                Schreib("Sicherung-Probe (ohne PS5, ein Ordner spielt die PS5): normal=" + normal + ", heißes Journal bleibt -> 3 Versuche, nichts gesichert=" + heissBleibt
                        + ", heißes Journal verschwindet -> 2. Versuch klappt=" + heissWeg + ", beschädigte Datenbank erkannt=" + kaputtErkannt
                        + ", Bereitschaft lesbar=" + bereit + ", Ordner nicht lesbar -> gesperrt=" + gesperrt1 + ", Datei nicht ladbar -> gesperrt=" + gesperrt2 + " -> " + (ok ? "OK" : "FEHLER"));
                if (g1 != null) Schreib("  normal: " + g1);
                if (g2 != null) Schreib("  heißes Journal: " + g2);
                if (g3 != null) Schreib("  Journal weg: " + g3);
                if (g4 != null) Schreib("  beschädigt: " + g4);
                return ok;
            }
            finally
            {
                K.Warte = altWarte; K.DB_LISTE_ERSATZ = null; K.DB_LADEN_ERSATZ = null;
                K.SICHERUNG_WURZEL = null; K.SICHERUNG_ORDNER = ""; K.FIRMWARE = ""; K.MODELL = "";
                K.DbBereitschaftSetzen(Kern.DbStand.Bereit, "");
            }
        }

        // kleine Test-Datenbank: Tabelle t + Index, zeilen Zeilen, Seitengröße 4096. wal = WAL-Modus ohne automatisches Zurückschreiben
        // (die Zeilen stehen dann nur in -wal); kopieNach != null: db und -wal bei noch offener Verbindung dorthin kopieren
        static string TestDbAnlegen(string pfad, bool wal, int zeilen, string kopieNach)
        {
            string f;
            IntPtr db = SqliteNativ.Oeffnen(pfad, SqliteNativ.OPEN_READWRITE | SqliteNativ.OPEN_CREATE, out f);
            if (db == IntPtr.Zero) return f;
            try
            {
                var befehle = new List<string> { "PRAGMA page_size=4096" };
                if (!wal) befehle.Add("PRAGMA journal_mode=DELETE");
                befehle.Add("CREATE TABLE t(id INTEGER PRIMARY KEY, b TEXT)");
                befehle.Add("CREATE INDEX t_b ON t(b)");
                if (wal) { befehle.Add("PRAGMA journal_mode=WAL"); befehle.Add("PRAGMA wal_autocheckpoint=0"); }
                befehle.Add("WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM c WHERE x<" + zeilen + ") INSERT INTO t(b) SELECT 'zeile-' || x || '-' || hex(randomblob(40)) FROM c");
                foreach (var sql in befehle)
                {
                    f = SqliteNativ.Ausfuehren(db, sql);
                    if (f != null) return sql + ": " + f;
                }
                if (kopieNach != null)
                {
                    Directory.CreateDirectory(kopieNach);
                    foreach (var e in new[] { "", "-wal" })
                        if (File.Exists(pfad + e)) File.WriteAllBytes(Path.Combine(kopieNach, Path.GetFileName(pfad) + e), LiesGeteilt(pfad + e));
                }
                return null;
            }
            finally { SqliteNativ.Schliessen(db); }
        }
        // Datei lesen, auch wenn SQLite sie gerade offen hat
        static byte[] LiesGeteilt(string p)
        {
            using (var fs = new FileStream(p, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
            {
                var ms = new MemoryStream(); fs.CopyTo(ms); return ms.ToArray();
            }
        }
        // Zeilen der Test-Tabelle in einer (geprüften) Kopie; null = nicht lesbar
        static string Zeilen(string pfad)
        {
            string f;
            if (!File.Exists(pfad)) return null;
            IntPtr db = SqliteNativ.Oeffnen(pfad, SqliteNativ.OPEN_READWRITE, out f);
            if (db == IntPtr.Zero) return null;
            try { return SqliteNativ.Wert(db, "SELECT count(*) FROM t"); }
            finally { SqliteNativ.Schliessen(db); }
        }
        static string DateiSha(string p) { using (var h = System.Security.Cryptography.SHA256.Create()) return BitConverter.ToString(h.ComputeHash(File.ReadAllBytes(p))); }
        static bool DateienGleich(string a, string b) { return File.Exists(a) && File.Exists(b) && DateiSha(a) == DateiSha(b); }
        // 64 Bytes mitten im Zellinhalt einer Blatt-Seite (Tabelle 0x0D / Index 0x0A, mindestens 5 Zellen) überschreiben - möglichst
        // in der Mitte der Datei; Seite 1 (Kopf + Schema) nie. false = keine passende Seite gefunden
        static bool Beschaedigen(string pfad)
        {
            var b = File.ReadAllBytes(pfad);
            if (b.Length < 100) return false;
            int ps = (b[16] << 8) | b[17]; if (ps == 1) ps = 65536;
            int n = b.Length / ps;
            var reihe = new List<int>();
            for (int s = n / 2; s <= n; s++) reihe.Add(s);
            for (int s = n / 2 - 1; s >= 2; s--) reihe.Add(s);
            foreach (int seite in reihe)
            {
                if (seite < 2) continue;
                int o = (seite - 1) * ps;
                if (b[o] != 0x0D && b[o] != 0x0A) continue;
                int zellen = (b[o + 3] << 8) | b[o + 4], inhalt = (b[o + 5] << 8) | b[o + 6];
                if (zellen < 5 || inhalt == 0 || inhalt > ps - 256) continue;
                int mitte = o + inhalt + (ps - inhalt) / 2 - 32;
                for (int i = 0; i < 64; i++) b[mitte + i] = 0xA5;
                File.WriteAllBytes(pfad, b);
                return true;
            }
            return false;
        }

        // sichtbarer Text eines Kästchens
        class TextRange
        {
            string t;
            public TextRange(CheckBox c)
            {
                var tb = c.Content as TextBlock;
                t = tb == null ? Convert.ToString(c.Content) : string.Concat(tb.Inlines.OfType<System.Windows.Documents.Run>().Select(r => r.Text));
            }
            public static implicit operator string(TextRange r) { return r.t; }
        }
    }
}
