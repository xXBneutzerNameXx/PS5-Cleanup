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
