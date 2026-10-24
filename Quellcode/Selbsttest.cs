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

        static Eintrag E(int kat, string text, string quelle = "", bool gesperrt = false)
        {
            if (gesperrt) text += "  [GESPERRT: Game Compressor arbeitet gerade daran]";
            return new Eintrag { Pfade = new List<string> { "/x" + text.GetHashCode() }, PfadeBasis = new List<string> { "/x" + text.GetHashCode() }, Kat = kat, Text = text, TextBasis = text, Quelle = quelle, Gesperrt = gesperrt };
        }

        public static void Laufen(Oberflaeche o, string bild)
        {
            datei = bild != "" ? Path.Combine(Path.GetDirectoryName(Path.GetFullPath(bild)), "_selbsttest.txt") : Path.Combine(Programm.Ordner, "_selbsttest.txt");
            File.WriteAllText(datei, "Selbsttest " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + " HIER=" + Programm.Ordner + " Version=" + Kern.VERSION + "\r\n", new UTF8Encoding(false));
            var K = o.K;
            K.Funde = new List<Eintrag> {
                E(7, "homebrew/Red Dead Redemption - PPSA30528.exfat  [Spiel: Red Dead Redemption (PPSA30528) - Kachel geht mit]", "/mnt/ext1/homebrew/x.exfat"),
                E(1, "Red Dead Redemption (PPSA30528) - ShadowMount-Spiel auf der M.2", "/mnt/ext1/homebrew/x.exfat"),
                E(1, "Echtes Spiel (PPSA55555) + Update + Zusatzinhalte"),
                E(1, "Spiel X (PPSA11111) - ShadowMount-Spiel auf der M.2", "/mnt/ext1/homebrew/y.exfat", true),
                E(2, "Spielstand PPSA55555 (Echtes Spiel)"),
                E(3, "Trophäen-Daten trophy/data"),
                E(4, "Aufnahme content_tmp/PPSA55555 (Echtes Spiel)"),
                E(5, "Share-Einstellungen alter Spiele (66 Spiele, legt die PS5 bei Bedarf neu an)"),
                E(5, "Fehlerberichte der Konsole (100)"),
                E(6, "Kachel des Tools: Payload Manager (PLDM00001)"),
                E(6, "shadowmount"),
                E(7, "games/DuckTales Remastered [BLES01969].iso") };
            // ein normal installiertes PS4-Spiel mit Update und DLC
            var teile = new[] { new[] { "Spiel", "Bloodborne (CUSA00900) - Spiel" }, new[] { "Update", "Bloodborne (CUSA00900) - Update (Version 01.09)" }, new[] { "DLC", "Bloodborne (CUSA00900) - Zusatzinhalt: The Old Hunters" } };
            int pos = 2;
            foreach (var t in teile) { var e = E(1, t[1]); e.Gruppe = "/user|CUSA00900"; e.Teil = t[0]; K.Funde.Insert(pos++, e); }
            K.Funde = Kern.ALLE_KAT.SelectMany(k => K.Funde.Where(f => f.Kat == k)).ToList();
            K.m2Da = true;
            o.W.Loaded += delegate
            {
                bool hakenOk = false, teilOk = false, knopfOk = false, smOk = false, schliessOk = false, groesseOk = false, updateOk = false;
                try
                {
                    var tbInfo = (TextBlock)o.W.FindName("tbInfo"); var tbIp = (TextBox)o.W.FindName("tbIp"); var tbLeer = (TextBlock)o.W.FindName("tbLeer");
                    tbLeer.Visibility = Visibility.Collapsed;
                    tbInfo.Text = "Modell: CFI-7021 B01Y   |   Reihe: PS5 Pro   |   Firmware: 12.00   |   Laufwerk ist mit dieser Konsole gekoppelt (nachträglich gekoppelt) und einsatzbereit.";
                    tbIp.Text = "192.168.50.78";
                    o.ZeigeBereiche();
                    o.Log("Selbsttest: Fenster aufgebaut."); o.Log("Verbunden."); o.Log(K.Funde.Count + " Einträge gefunden. Nichts wurde verändert.");
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
                        if (f.Text.StartsWith("Bloodborne") && f.Text.EndsWith("- Spiel")) basisCb = c; else if (f.Text.StartsWith("Bloodborne")) teilCbs.Add(c);
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
                    K.WaehleKomplett(false); foreach (var f in K.Funde) f.Wahl = false;
                    smf.Wahl = true; K.SetzeSmSpieldateien(false);
                    smOk = !smf.Pfade.Contains("/user/data/x.exfat") && smf.Text == smf.TextBasis + " - nur Kachel, Spiel bleibt";
                    K.SmZuruecksetzen(); foreach (var f in K.Funde) f.Wahl = false; o.ZeigeHaken();
                    Schreib("SM-Probe: nach abgebrochenem KOMPLETT 'nur Kacheln' ohne Spieldatei -> " + (smOk ? "OK" : "FEHLER"));

                    // Schließ-Probe: Fenster wird mitten im Zählen geschlossen -> keine Frage,
                    // kein Fehler, kein Löschen. (Zählen und Löschen hier nur nachgestellt.)
                    var altZaehlen = o.Zaehlen; bool geloescht = false; string fehler = "";
                    o.Zaehlen = () => { o.FENSTER_ZU = true; return new Groesse(); };
                    o.LoescheJetztErsatz = () => { geloescht = true; };
                    foreach (var f in K.Funde) f.Wahl = f.Kat == 5;
                    try { o.AuswahlAblauf(); } catch (Exception ex) { fehler = ex.Message; }
                    bool fragen = !o.FrageWahl("x", "x", "a", "b") && o.FrageText("x", "x") == null && !o.FrageJaNein("x", "x");
                    o.Zaehlen = altZaehlen; o.LoescheJetztErsatz = null; o.FENSTER_ZU = false;
                    // M.2 in der Auswahl -> eigene Liste stimmt, Abschlussfrage warnt oben
                    foreach (var f in K.Funde) f.Wahl = f.Kat == 7 || f.Kat == 5;
                    var m2 = K.M2Gewaehlt();
                    bool m2Zahl = m2.Count == K.Funde.Count(f => f.Kat == 7) && m2.All(f => f.Kat == 7);
                    bool m2Warn = o.Zusammenfassung(false).StartsWith("!!! ACHTUNG") && o.Zusammenfassung(false).Contains("M.2");
                    foreach (var f in K.Funde) f.Wahl = false; o.ZeigeHaken();
                    schliessOk = !geloescht && fehler == "" && fragen && m2Zahl && m2Warn;
                    Schreib("Schließ-Probe: gelöscht=" + geloescht + ", Fehler='" + fehler + "', Fragen sicher=" + fragen + ", M.2-Warnung=" + m2Warn + ", M.2-Anzahl richtig=" + m2Zahl + " -> " + (schliessOk ? "OK" : "FEHLER"));

                    // Größen-Probe (NEU): bekannte Größe erscheint am Eintrag und in der Kopfzeile
                    var g1 = K.Funde.First(f => f.Kat == 7 && f.Quelle == "");
                    K.GroesseSetzen(g1.PfadeBasis[0], new Groesse { Bytes = 18738184192L, Dateien = 1 });
                    o.ZeigeBereiche();
                    var cb7 = o.Kaestchen.First(c => c.Tag == g1);
                    string inhalt = new TextRange(cb7);
                    var ex7 = (Expander)sp.Children[Array.IndexOf(Kern.ALLE_KAT, 7)];
                    bool kopfOhneSumme = !((CheckBox)ex7.Header).Content.ToString().Contains("GB");   // anderer M.2-Eintrag noch ungezählt
                    var g2 = K.Funde.First(f => f.Kat == 7 && f.Quelle != "");
                    K.GroesseSetzen(g2.PfadeBasis[0], new Groesse { Bytes = 10385096704L, Dateien = 1 });
                    o.ZeigeBereiche();
                    bool kopfSumme = ((CheckBox)((Expander)sp.Children[Array.IndexOf(Kern.ALLE_KAT, 7)]).Header).Content.ToString().Contains("27,1 GB");
                    groesseOk = inhalt.Contains("17,5 GB") && kopfOhneSumme && kopfSumme;
                    Schreib("Größen-Probe: Eintrag zeigt '" + inhalt.Substring(Math.Max(0, inhalt.Length - 12)).Trim() + "', Kopf erst ohne Summe=" + kopfOhneSumme + ", dann mit Summe=" + kopfSumme + " -> " + (groesseOk ? "OK" : "FEHLER"));

                    // Update-Probe (NEU): Versionsvergleich
                    updateOk = Oberflaeche.Neuer("v1.0.5-beta", "1.0.4 Beta") && !Oberflaeche.Neuer("v1.0.4-beta", "1.0.4 Beta") && !Oberflaeche.Neuer("v1.0.3-beta", "1.0.4 Beta") && Oberflaeche.Neuer("v1.1-beta", "1.0.4 Beta");
                    Schreib("Update-Probe: Versionsvergleich -> " + (updateOk ? "OK" : "FEHLER"));
                    Schreib("ps5info.elf eingebaut: " + (Programm.Ressource("ps5info.elf") != null) + ", LIESMICH eingebaut: " + (Programm.Ressource("LIESMICH.txt") != null));

                    if (bild != "")
                    {
                        o.W.UpdateLayout();
                        var rtb = new RenderTargetBitmap((int)o.W.ActualWidth, (int)o.W.ActualHeight, 96, 96, PixelFormats.Pbgra32);
                        rtb.Render(o.W);
                        var enc = new PngBitmapEncoder(); enc.Frames.Add(BitmapFrame.Create(rtb));
                        using (var fs = File.Create(bild)) enc.Save(fs);
                        Schreib("Bild gespeichert: " + bild);
                    }
                    Schreib(hakenOk && teilOk && knopfOk && smOk && schliessOk && groesseOk && updateOk ? "SELBSTTEST OK" : "SELBSTTEST FEHLER (siehe Probe oben)");
                }
                catch (Exception ex) { Schreib("FEHLER: " + ex.Message + " | " + ex.StackTrace); }
                o.W.Dispatcher.BeginInvoke(DispatcherPriority.Background, new Action(() => o.W.Close()));
            };
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
