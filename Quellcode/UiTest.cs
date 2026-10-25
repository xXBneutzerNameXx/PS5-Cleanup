// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// UiTest.cs - Oberflächentest gegen die Test-PS5 (nur für die Entwicklung):
// PS5-Cleanup.exe -UiTest <port> <ordner der Test-PS5>
// Verbindet, wartet auf die Hintergrund-Zählung, löscht eine M.2-Datei über
// die echten Abläufe (Antworten vorgegeben), prüft Abbrechen und Schließen.
// =====================================================================
#if UITEST
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;

namespace PS5Cleanup
{
    public static class UiTest
    {
        static string datei; static int fehler;
        static void Schreib(string t) { File.AppendAllText(datei, t + "\r\n", new UTF8Encoding(false)); }
        static void Pruefe(bool ok, string was) { Schreib((ok ? "OK   " : "FAIL ") + was); if (!ok) fehler++; }
        static bool WarteBis(Func<bool> f, int sek) { var ende = DateTime.Now.AddSeconds(sek); while (DateTime.Now < ende) { if (f()) return true; Thread.Sleep(100); } return f(); }

        public static void Laufen(Oberflaeche o, int port, string ps5)
        {
            datei = Path.Combine(Programm.Ordner, "_uitest.txt");
            File.WriteAllText(datei, "Oberflächentest " + DateTime.Now.ToString("HH:mm:ss") + " Version " + Kern.VERSION + "\r\n", new UTF8Encoding(false));
            o.W.Loaded += delegate { var t = new Thread(() => { try { Treiber(o, port, ps5); } catch (Exception ex) { Schreib("FEHLER: " + ex); } }); t.IsBackground = true; t.Start(); };
        }

        static T Hol<T>(Oberflaeche o, string name) where T : class { T r = null; o.UI(() => r = o.W.FindName(name) as T); return r; }
        static CheckBox Kaestchen(Oberflaeche o, string textAnfang)
        {
            CheckBox r = null;
            o.UI(() => r = o.Kaestchen.FirstOrDefault(c => ((Eintrag)c.Tag).Text.StartsWith(textAnfang)));
            return r;
        }
        static string LogText(Oberflaeche o) { string t = ""; o.UI(() => t = Hol<TextBox>(o, "tbLog").Text); return t; }
        static string Lokal(string ps5, string p) { return ps5 + p.Replace('/', '\\'); }

        static void Verbinden(Oberflaeche o)
        {
            o.TestAntworten = new Queue<object>(new object[] { "" });      // Modell-Frage: leer lassen
            o.UI(() => o.Starte(o.Verbinde, true));
            WarteBis(() => o.BESCHAEFTIGT, 2);
            WarteBis(() => !o.BESCHAEFTIGT, 90);
        }

        static void Suchen(Oberflaeche o)
        {
            o.UI(() => Hol<Button>(o, "btSuchen").RaiseEvent(new RoutedEventArgs(Button.ClickEvent)));
            WarteBis(() => o.BESCHAEFTIGT, 2);
            WarteBis(() => !o.BESCHAEFTIGT, 60);
            Thread.Sleep(300);      // Statuszeile wird über den Dispatcher gesetzt
        }

        static void Treiber(Oberflaeche o, int port, string ps5)
        {
            Thread.Sleep(500);
            o.UI(() => { Hol<TextBox>(o, "tbIp").Text = "127.0.0.1:" + port; Hol<CheckBox>(o, "cbModell").IsChecked = false; });

            // 1 Verbinden, Liste, Größen im Hintergrund
            Verbinden(o);
            Pruefe(o.VERBUNDEN && o.K.Funde.Count == 57, "verbunden, 57 Einträge (" + o.K.Funde.Count + ")");
            Pruefe(WarteBis(() => !o.HintergrundLaeuft, 60), "Hintergrund-Zählung fertig");
            Thread.Sleep(500);
            Pruefe(o.K.Funde.All(f => o.K.EintragGroesse(f, true) != null), "alle Einträge haben eine Größe");
            string kopf7 = ""; o.UI(() => { var sp = Hol<StackPanel>(o, "spBereiche"); kopf7 = Convert.ToString(((CheckBox)((Expander)sp.Children[6]).Header).Content); });
            Pruefe(kopf7.Contains("·"), "Kopfzeile Bereich 7 zeigt die Summe: " + kopf7);
            var cbIso = Kaestchen(o, "games/x.iso");
            Pruefe(cbIso != null, "M.2-Datei games/x.iso als eigener Eintrag");

            // 2 M.2-Frage mit "Abbrechen" beantworten -> nichts gelöscht
            o.UI(() => { Hol<CheckBox>(o, "cbTest").IsChecked = false; cbIso.IsChecked = true; cbIso.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent)); });
            o.TestAntworten = new Queue<object>(new object[] { false });
            o.UI(() => o.Starte(o.AuswahlAblauf, false));
            WarteBis(() => o.BESCHAEFTIGT, 2); WarteBis(() => !o.BESCHAEFTIGT, 60);
            Pruefe(File.Exists(Lokal(ps5, "/mnt/ext1/games/x.iso")), "M.2-Frage abgebrochen -> Datei noch da");

            // 3 M.2-Frage "Weiter", Abschlussfrage "Jetzt löschen" -> genau diese Datei weg
            o.TestAntworten = new Queue<object>(new object[] { true, true });
            o.UI(() => o.Starte(o.AuswahlAblauf, false));
            WarteBis(() => o.BESCHAEFTIGT, 2); WarteBis(() => !o.BESCHAEFTIGT, 120);
            string log = LogText(o);
            Pruefe(!File.Exists(Lokal(ps5, "/mnt/ext1/games/x.iso")) && Directory.Exists(Lokal(ps5, "/mnt/ext1/games")), "Datei gelöscht, Ordner games bleibt");
            Pruefe(log.Contains("gelöscht   games/x.iso") && log.Contains("[Test] Meldung: Fertig"), "Protokoll: gelöscht + Fertig-Meldung");
            Pruefe(o.VERBUNDEN && !o.K.Funde.Any(f => f.Text.StartsWith("games/x.iso")) && o.K.Funde.Any(f => f.Text == "games  (leerer Ordner)"), "neu nachgeschaut: x.iso weg, leerer Ordner games wird angeboten");
            Pruefe(File.Exists(Lokal(ps5, "/mnt/ext1/homebrew/anderes.txt")), "andere M.2-Dateien unberührt");
            // NEU 1.0.5: Feedback-Bericht nach dem Löschen - nicht mehr "im Testmodus", "gelöscht: ja"
            FeedbackDaten fd = null; o.UI(() => fd = o.FeedbackSammeln());
            Pruefe(fd.GeloeschtInSitzung && fd.ScanGemacht && !fd.ScanImTestmodus, "Feedback nach dem Löschen: gelöscht=ja, Scan nicht im Testmodus");

            // 3b NEU 1.0.5: Während des Löschens muss das Fenster bedienbar bleiben (Schließen-Klick
            // zeigt die Bitte zu warten). Ein großer Ordner mit vielen Unterordnern erzeugt sehr viele
            // FTP-Befehle - gemessen wird, wie lange Mausklicks (Eingabe) warten müssen.
            string riesig = Lokal(ps5, "/mnt/ext1/homebrew/Riesenordner");
            for (int i = 0; i < 300; i++) for (int j = 0; j < 5; j++) { string d = Path.Combine(riesig, "o" + i, "u" + j); Directory.CreateDirectory(d); File.WriteAllText(Path.Combine(d, "x.bin"), "x"); }
            Verbinden(o);
            var cbRiesig = Kaestchen(o, "homebrew/Riesenordner");
            Pruefe(cbRiesig != null, "großer M.2-Ordner (1500 Unterordner) als Eintrag");
            if (cbRiesig != null)
            {
                o.UI(() => { Hol<CheckBox>(o, "cbTest").IsChecked = false; foreach (var f in o.K.Funde) f.Wahl = false; o.ZeigeHaken(); cbRiesig.IsChecked = true; cbRiesig.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent)); });
                o.TestAntworten = new Queue<object>(new object[] { true, true });
                o.UI(() => o.Starte(o.AuswahlAblauf, false));
                WarteBis(() => o.LAEUFT, 30);
                double maxMs = 0; int proben = 0; bool zuVersucht = false; bool rotGesehen = false; bool fortschritt = false;
                while (o.BESCHAEFTIGT)
                {
                    var t0 = DateTime.Now;
                    o.W.Dispatcher.Invoke(new Action(() => { }), System.Windows.Threading.DispatcherPriority.Input);
                    double ms = (DateTime.Now - t0).TotalMilliseconds; if (ms > maxMs) maxMs = ms; proben++;
                    if (o.LAEUFT) o.UI(() => { if (Hol<Border>(o, "bdLaeuft").Visibility == Visibility.Visible) rotGesehen = true; if (Hol<TextBlock>(o, "tbStand").Text.Contains("Lösche Eintrag 1 von 1")) fortschritt = true; });
                    if (!zuVersucht && proben > 5) { zuVersucht = true; o.W.Dispatcher.Invoke(new Action(() => { if (o.LAEUFT) o.W.Close(); else zuVersucht = false; }), System.Windows.Threading.DispatcherPriority.Input); }
                    Thread.Sleep(30);
                }
                Schreib("  Eingabe-Wartezeit beim Löschen: höchstens " + (int)maxMs + " ms (" + proben + " Proben)");
                Pruefe(proben > 3 && maxMs < 500, "Fenster bleibt beim Löschen bedienbar (Klicks warten höchstens " + (int)maxMs + " ms)");
                bool offen = false; o.UI(() => offen = o.W.IsVisible);
                Pruefe(rotGesehen, "roter Hinweis 'Löschen läuft' war während des Löschens sichtbar");
                Pruefe(fortschritt, "Fortschritt 'Lösche Eintrag 1 von 1' angezeigt");
                Pruefe(LogText(o).Contains("Schließen während des Löschens"), "Schließen-Versuch steht im Protokoll");
                Pruefe(zuVersucht && offen && LogText(o).Contains("[Test] Meldung: PS5 Cleanup"), "Schließen beim Löschen: Fenster bleibt offen, Bitte-warten-Meldung kommt");
                Pruefe(!Directory.Exists(riesig), "großer Ordner gelöscht");
            }

            // 3c NEU 1.0.5: eine einzelne, lange PS5-Antwort (wie beim Löschen einer riesigen Spieldatei)
            string lang = Lokal(ps5, "/mnt/ext1/homebrew/langsam-spiel");
            Directory.CreateDirectory(Path.Combine(lang, "sce_sys")); File.WriteAllText(Path.Combine(lang, "sce_sys", "x.bin"), "x");
            Verbinden(o);
            var cbLang = Kaestchen(o, "homebrew/langsam-spiel");
            if (cbLang != null)
            {
                o.UI(() => { Hol<CheckBox>(o, "cbTest").IsChecked = false; foreach (var f in o.K.Funde) f.Wahl = false; o.ZeigeHaken(); cbLang.IsChecked = true; cbLang.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent)); });
                o.TestAntworten = new Queue<object>(new object[] { true, true });
                o.UI(() => o.Starte(o.AuswahlAblauf, false));
                WarteBis(() => o.LAEUFT, 30);
                double maxL = 0; bool warteText = false;
                while (o.BESCHAEFTIGT)
                {
                    var t0 = DateTime.Now;
                    o.W.Dispatcher.Invoke(new Action(() => { }), System.Windows.Threading.DispatcherPriority.Input);
                    double ms = (DateTime.Now - t0).TotalMilliseconds; if (ms > maxL) maxL = ms;
                    o.UI(() => { if (Hol<TextBlock>(o, "tbStand").Text.Contains("[PS5")) warteText = true; });
                    Thread.Sleep(30);
                }
                Schreib("  Eingabe-Wartezeit bei langer PS5-Antwort: höchstens " + (int)maxL + " ms");
                Pruefe(maxL < 500 && warteText, "lange PS5-Antwort: Fenster bedienbar, Anzeige 'PS5 arbeitet seit ...' erscheint");
                Pruefe(!Directory.Exists(lang), "langsames Spiel gelöscht");
            }
            else Pruefe(false, "langsam-spiel als Eintrag gefunden");

            // 4 Abbrechen-Knopf beim Nachschauen
            o.TestAntworten = new Queue<object>(new object[] { "" });
            o.UI(() => o.Starte(o.Verbinde, true));
            WarteBis(() => o.K.S != null && !o.VERBUNDEN && LogText(o).Contains("Schaue nach"), 10);
            o.UI(() => { var b = Hol<Button>(o, "btAbbrechen"); b.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent)); });
            WarteBis(() => !o.BESCHAEFTIGT, 60);
            log = LogText(o);
            bool abgebrochen = log.Contains("Abgebrochen - nichts gelöscht.");
            Pruefe(abgebrochen ? !o.VERBUNDEN && o.K.FUNDE_IP == "" : o.VERBUNDEN, abgebrochen ? "Abbrechen beim Nachschauen: keine Liste, Löschen gesperrt" : "(Suche war schon fertig, bevor Abbrechen kam)");
            bool knoepfe = false; o.UI(() => knoepfe = !Hol<Button>(o, "btKomplett").IsEnabled == abgebrochen && Hol<Button>(o, "btVerbinden").IsEnabled);
            Pruefe(knoepfe, "Knöpfe danach richtig (Verbinden frei, Löschen nur mit Liste)");

            // 4b NEU 1.0.5: Netzsuche auf zwei Ports. Der zweite Test-Server meldet sich wie ftpsrv 0.21.1
            // und kennt kein EPSV. Zwei Treffer -> Auswahl (Antwort: der zweite).
            int port2 = port - 1;
            o.FTP_PORTE = new[] { port, port2 };
            o.TestSuchHosts = new List<string> { "127.0.0.1" };
            o.TestAntworten = new Queue<object>(new object[] { 1 });
            o.UI(() => Hol<Button>(o, "btSuchen").RaiseEvent(new RoutedEventArgs(Button.ClickEvent)));
            WarteBis(() => o.BESCHAEFTIGT, 2);
            WarteBis(() => !o.BESCHAEFTIGT, 60);
            string ipText = ""; o.UI(() => ipText = Hol<TextBox>(o, "tbIp").Text);
            log = LogText(o);
            Pruefe(log.Contains("[Test] Auswahl:") && log.Contains("Port " + port) && log.Contains("Port " + port2), "Suche: beide Ports gefunden, Auswahl angeboten");
            Pruefe(log.Contains("ftpsrv, Version 0.21.1") && log.Contains("FTP-Server: Unbekannt"), "Suche: Servernamen erkannt bzw. Unbekannt");
            Pruefe(ipText == "127.0.0.1:" + port2, "Auswahl übernommen: " + ipText);
            Verbinden(o);
            log = LogText(o);
            Pruefe(o.VERBUNDEN && log.Contains("FTP-Server: ftpsrv, Version 0.21.1") && log.Contains("Datenverbindung: PASV"), "verbunden mit dem 2. Server, Protokoll nennt ftpsrv 0.21.1 und PASV");
            string bericht = ""; o.UI(() => bericht = Feedback.Bericht(o.FeedbackSammeln()));
            Pruefe(bericht.Contains("ftpsrv, Version 0.21.1") && bericht.Contains("PASV") && !bericht.Contains("127.0.0.1"), "Feedback-Bericht: FTP-Server und Datenverbindung drin, keine IP");

            // 4c nur die IP eingegeben, der erste Port ist zu -> automatisch der nächste übliche Port
            int frei1 = port - 2, frei2 = port - 3;
            o.FTP_PORTE = new[] { frei1, port };
            o.UI(() => Hol<TextBox>(o, "tbIp").Text = "127.0.0.1");
            Verbinden(o);
            log = LogText(o); o.UI(() => ipText = Hol<TextBox>(o, "tbIp").Text);
            Pruefe(o.VERBUNDEN && log.Contains("versuche Port " + port) && ipText == "127.0.0.1:" + port, "nur IP: Port " + frei1 + " zu -> automatisch über Port " + port + " verbunden (" + ipText + ")");
            Pruefe(log.Contains("FTP-Server: Unbekannt (Begrüßung: „mini“)") && log.Contains("Datenverbindung: EPSV"), "Protokoll: unbekannter Server mit Begrüßung, EPSV");

            // 4d gar nichts erreichbar -> einfache Meldung, Technik nur im Protokoll
            o.FTP_PORTE = new[] { frei1, frei2 };
            o.UI(() => Hol<TextBox>(o, "tbIp").Text = "127.0.0.1");
            Verbinden(o);
            log = LogText(o); string info = ""; o.UI(() => info = Hol<TextBlock>(o, "tbInfo").Text);
            Pruefe(!o.VERBUNDEN && info.StartsWith("PS5 nicht erreichbar.") && !info.Contains("EPSV") && !info.Contains("Port " + frei1), "nicht erreichbar: einfache Meldung ohne Technik");
            Pruefe(log.Contains("Grund: Port " + frei1 + ":") && log.Contains("/ Port " + frei2 + ":"), "Protokoll nennt den Grund für beide Ports");
            // 4e Netzsuche: zwei / einer / keiner - auf Deutsch und Englisch
            foreach (bool en in new[] { false, true })
            {
                Sp.EN = en; string sp = en ? "EN" : "DE";
                o.TestSuchHosts = new List<string> { "127.0.0.1" };
                // zwei Server -> Auswahl, Statuszeile nennt die Anzahl
                int vorher = Regex.Matches(LogText(o), "\\[Test\\] Auswahl").Count;
                o.FTP_PORTE = new[] { port, port2 };
                o.TestAntworten = new Queue<object>(new object[] { 0 });
                Suchen(o);
                log = LogText(o); o.UI(() => ipText = Hol<TextBox>(o, "tbIp").Text);
                Pruefe(Regex.Matches(log, "\\[Test\\] Auswahl").Count == vorher + 1 && log.Contains("Statuszeile: " + (en ? "2 FTP servers found – please select one." : "2 FTP-Server gefunden – bitte einen auswählen.")), sp + ": zwei Server -> Auswahl, Statuszeile '2 ... gefunden – bitte einen auswählen'");
                Pruefe(ipText == "127.0.0.1", sp + ": Auswahl Port " + port + " (Standard) -> nur IP eingetragen (" + ipText + ")");
                // ein Server -> ohne Auswahlfenster übernommen
                o.FTP_PORTE = new[] { frei1, port2 };
                o.TestAntworten = new Queue<object>();
                Suchen(o);
                log = LogText(o); string stand = ""; o.UI(() => { ipText = Hol<TextBox>(o, "tbIp").Text; stand = Hol<TextBlock>(o, "tbStand").Text; });
                Pruefe(Regex.Matches(log, "\\[Test\\] Auswahl").Count == vorher + 1 && ipText == "127.0.0.1:" + port2 && stand.StartsWith(en ? "Found:" : "Gefunden:"), sp + ": ein Server -> ohne Auswahl übernommen (" + ipText + ", '" + stand + "')");
                // kein Server -> verständliche Meldung unter dem IP-Feld und in der Statuszeile
                o.FTP_PORTE = new[] { frei1, frei2 };
                Suchen(o);
                string info2 = ""; o.UI(() => { info2 = Hol<TextBlock>(o, "tbInfo").Text; stand = Hol<TextBlock>(o, "tbStand").Text; });
                string erwartet = en ? "No PS5 with a running FTP server found. Please check:" : "Keine PS5 mit laufendem FTP-Server gefunden. Bitte prüfen:";
                Pruefe(info2.StartsWith(erwartet) && stand.StartsWith(erwartet) && !info2.Contains("EPSV") && !info2.Contains("Port"), sp + ": kein Server -> verständliche Meldung (" + info2 + ")");
            }
            Sp.EN = false;
            o.FTP_PORTE = new[] { Kern.STANDARD_PORT, Kern.ZWEIT_PORT }; o.TestSuchHosts = null;
            o.UI(() => Hol<TextBox>(o, "tbIp").Text = "127.0.0.1:" + port);

            // 5 Neu verbinden, Fenster während der Hintergrund-Zählung schließen
            Verbinden(o);
            Pruefe(o.VERBUNDEN, "wieder verbunden");
            Schreib("FEHLERZAHL " + fehler);
            Schreib("SCHLIESSE");
            o.UI(() => o.W.Close());
        }
    }
}
#endif
