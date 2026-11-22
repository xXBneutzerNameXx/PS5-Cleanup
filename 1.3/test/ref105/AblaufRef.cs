// PS5 Cleanup 1.1 - Referenz der BEDIENABLÄUFE von 1.0.5 (Vergleich F)
// Lizenz: MIT - Copyright (c) 2026 da_Chris86
//
// Startet die UNVERÄNDERTE Oberfläche von 1.0.5 (Quellcode\*.cs) gegen den Test-FTP-Server
// und spielt ein Szenario ab: Häkchen setzen, "Vollständige Bereinigung" / "Nur Auswahl löschen",
// Fragen beantworten. Die echten Dialogfenster werden abgefangen: Titel, Text und Knöpfe
// werden ins Protokoll geschrieben, dann wird wie im Szenario geklickt.
// Ergebnis: das Protokoll von 1.0.5 (jede Log-Zeile + jede Frage in ihrer Reihenfolge).
//
//   Ref105Ablauf.exe <port> <szenario-datei> <ausgabe-datei>
//
// Szenario-Zeilen:  SPRACHE de|en  ·  MODELL <nr>  ·  ZEIGE  ·  WAEHLE BEREICH <k>  ·  ABWAEHLEN BEREICH <k>
//                   WAEHLE TEXT <anfang>  ·  ABWAEHLEN TEXT <anfang>  ·  ABLAUF KOMPLETT|AUSWAHL
//                   ANTWORT J|N|ABBRECHEN|<text>   (in der Reihenfolge der Fragen)
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
using System.Windows.Documents;
using System.Windows.Threading;

namespace PS5Cleanup
{
    public static class AblaufRef
    {
        static Oberflaeche o;
        static readonly Queue<string> antworten = new Queue<string>();
        static readonly HashSet<Window> gesehen = new HashSet<Window>();
        static string modell = "";
        static volatile bool fertig;
        static int fehler;

        static void Z(string t) { Protokoll.Zeile(t); }
        static void Mehrzeilig(string t) { foreach (var z in (t ?? "").Replace("\r", "").Split('\n')) Z("  | " + z); }
        static bool WarteBis(Func<bool> f, int sek) { var ende = DateTime.Now.AddSeconds(sek); while (DateTime.Now < ende) { if (f()) return true; Thread.Sleep(100); } return f(); }
        static T Hol<T>(string name) where T : class { T r = null; o.UI(() => r = o.W.FindName(name) as T); return r; }
        static void Klick(ButtonBase b) { b.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent)); }
        static string Text(TextBlock tb) { if (tb == null) return ""; return tb.Inlines.Count > 0 ? string.Concat(tb.Inlines.OfType<Run>().Select(r => r.Text)) : tb.Text; }

        [STAThread]
        public static int Main(string[] args)
        {
            int port = int.Parse(args[0]);
            var zeilen = File.ReadAllLines(args[1], Encoding.UTF8).Select(z => z.TrimEnd()).Where(z => z != "" && !z.StartsWith("#")).ToList();
            foreach (var z in zeilen) if (z.StartsWith("SPRACHE ")) Sp.EN = z.Substring(8) == "en";
            foreach (var z in zeilen) if (z.StartsWith("MODELL ")) modell = z.Substring(7);
            foreach (var z in zeilen) if (z.StartsWith("ANTWORT ")) { string a = z.Substring(8); if (a == "BESTAETIGUNG") a = Sp.EN ? "DELETE" : "LÖSCHEN"; antworten.Enqueue(a); }
            File.WriteAllText(args[2], "", new UTF8Encoding(false));
            Protokoll.Datei = args[2];
            var app = new Application { ShutdownMode = ShutdownMode.OnMainWindowClose };
            o = new Oberflaeche();
            o.FensterMerken = false;
            app.DispatcherUnhandledException += (s, e) => { e.Handled = true; Z("FEHLER (Oberfläche): " + e.Exception.Message); };
            app.MainWindow = o.W;
            // außerhalb des Bildschirms, ohne Fokus zu stehlen
            o.W.ShowActivated = false; o.W.WindowStartupLocation = WindowStartupLocation.Manual; o.W.Left = -20000; o.W.Top = -20000;
            var uhr = new DispatcherTimer(DispatcherPriority.Background) { Interval = TimeSpan.FromMilliseconds(60) };
            uhr.Tick += delegate { Faenger(); };
            uhr.Start();
            o.W.Loaded += delegate
            {
                var t = new Thread(() =>
                {
                    try { Treiber(port, zeilen); }
                    catch (Exception ex) { Z("TREIBER-FEHLER: " + ex); fehler++; }
                    Z("ENDE fehler=" + fehler);
                    fertig = true;
                    o.UI(() => o.W.Close());
                });
                t.IsBackground = true; t.Start();
            };
            app.Run(o.W);
            return fehler;
        }

        // ---------- Dialoge abfangen (läuft im Oberflächen-Faden, auch während ShowDialog)
        static void Faenger()
        {
            foreach (Window w in Application.Current.Windows)
            {
                if (w == o.W || gesehen.Contains(w) || !w.IsLoaded) continue;
                gesehen.Add(w);
                w.Left = -20000; w.Top = -20000;
                var tx = w.FindName("tx") as TextBlock;
                var bGef = w.FindName("bGef") as Button; var bSicher = w.FindName("bSicher") as Button;
                var te = w.FindName("te") as TextBox; var bOk = w.FindName("bOk") as Button; var bAbbr = w.FindName("bAbbr") as Button;
                var tKopf = w.FindName("tKopf") as TextBlock;
                if (te != null && (w.Title == "Modell" || w.Title == "Model"))
                {   // Modellfrage beim Verbinden (gibt es in der ELF-Version nicht - dort wird direkt abgefragt)
                    te.Text = modell; Klick(bOk); continue;
                }
                if (tKopf != null)
                {
                    Z("FERTIG " + tKopf.Text);
                    Z("  frei: " + Text(w.FindName("tFrei") as TextBlock));
                    var ts = w.FindName("tSm") as TextBlock;
                    if (ts != null && ts.Visibility == Visibility.Visible && ts.Text != "") { Z("  sm:"); Mehrzeilig(ts.Text); }
                    Z("  schritt: " + Text(w.FindName("tSchritt") as TextBlock));
                    var g = w.FindName("gSchritte") as Grid;
                    var zellen = g.Children.OfType<TextBlock>().ToList();
                    for (int i = 0; i + 1 < zellen.Count; i += 2) Z("  " + zellen[i].Text + " " + zellen[i + 1].Text);
                    Z("  wichtig: " + Text(w.FindName("tWichtig") as TextBlock));
                    Klick(bOk); continue;
                }
                Z("DIALOG " + w.Title);
                if (tx != null) Mehrzeilig(tx.Text);
                string a = antworten.Count > 0 ? antworten.Dequeue() : "N";
                if (bGef != null)
                {
                    Z("  KNOPF sicher: " + bSicher.Content);
                    Z("  KNOPF gefährlich: " + bGef.Content);
                    Z("  ANTWORT " + (a == "J" ? "J" : "N"));
                    Klick(a == "J" ? bGef : bSicher);
                }
                else if (te != null)
                {
                    Z("  EINGABE");
                    Z("  ANTWORT " + a);
                    if (a == "ABBRECHEN") Klick(bAbbr); else { te.Text = a; Klick(bOk); }
                }
                else { Z("  UNBEKANNTER DIALOG"); fehler++; w.Close(); }
            }
        }

        // ---------- Anzeige wie in der Oberfläche: Bereichsköpfe, Einträge, Statuszeile
        static void Zeige()
        {
            WarteBis(() => !o.HintergrundLaeuft, 120);
            Thread.Sleep(400);
            o.UI(() =>
            {
                var sp = (StackPanel)o.W.FindName("spBereiche");
                foreach (var ex in sp.Children.OfType<Expander>())
                {
                    var kopf = (CheckBox)ex.Header;
                    Z("KOPF " + kopf.Content + (kopf.IsEnabled ? "" : "  [nicht wählbar]"));
                    foreach (var cb in ((StackPanel)ex.Content).Children.OfType<CheckBox>())
                    {
                        var f = (Eintrag)cb.Tag;
                        var tt = cb.ToolTip as ToolTip; string tip = tt != null ? ((TextBlock)tt.Content).Text : "";
                        string zusatz = tip.StartsWith(f.Text + "\n\n") ? tip.Substring(f.Text.Length + 2) : "";
                        Z("  " + (cb.IsChecked == true ? "[x] " : "[ ] ") + Text((TextBlock)cb.Content) + (cb.IsEnabled ? "" : "  [gesperrt]") + (zusatz != "" ? "  {" + zusatz + "}" : "") + (f.Paket ? "  (gelb)" : ""));
                    }
                }
                Z("STAND " + ((TextBlock)o.W.FindName("tbStand")).Text);
            });
        }

        static void Setze(string art, string was, bool an)
        {
            o.UI(() =>
            {
                if (art == "BEREICH")
                {
                    var sp = (StackPanel)o.W.FindName("spBereiche");
                    int k = int.Parse(was);
                    // 1.0.6: leere Bereiche werden nicht angezeigt - den Bereich über seine Nummer suchen
                    var ex = sp.Children.OfType<Expander>().FirstOrDefault(x => (int)((CheckBox)x.Header).Tag == k);
                    if (ex == null) { Z("BEREICH NICHT ANGEZEIGT " + k); return; }
                    var kopf = (CheckBox)ex.Header;
                    kopf.IsChecked = an; Klick(kopf);
                }
                else
                {
                    var alt = was.Split('|');
                    var cb = o.Kaestchen.FirstOrDefault(c => alt.Any(x => ((Eintrag)c.Tag).Text.StartsWith(x, StringComparison.Ordinal)));
                    if (cb == null) { Z("FEHLT " + was); fehler++; return; }
                    if (!cb.IsEnabled) { Z("NICHT WÄHLBAR " + was); return; }   // so wie ein Klick auf ein gesperrtes Kästchen
                    cb.IsChecked = an; Klick(cb);
                }
            });
            Thread.Sleep(200);
        }

        static void Treiber(int port, List<string> zeilen)
        {
            Thread.Sleep(500);
            o.UI(() => { Hol<TextBox>("tbIp").Text = "127.0.0.1:" + port; Hol<CheckBox>("cbModell").IsChecked = false; Hol<CheckBox>("cbTest").IsChecked = false; });
            o.UI(() => o.Starte(o.Verbinde, true));
            WarteBis(() => o.BESCHAEFTIGT, 3); WarteBis(() => !o.BESCHAEFTIGT, 120);
            if (!o.VERBUNDEN) { Z("NICHT VERBUNDEN"); fehler++; return; }
            Z("=== ABLAUF");
            foreach (var z in zeilen)
            {
                var t = z.Split(new[] { ' ' }, 3);
                switch (t[0])
                {
                    case "ZEIGE": Z("> ZEIGE"); Zeige(); break;
                    case "WAEHLE": Z("> " + z); Setze(t[1], t[2], true); break;
                    case "ABWAEHLEN": Z("> " + z); Setze(t[1], t[2], false); break;
                    case "ABLAUF":
                        Z("> " + z);
                        WarteBis(() => !o.HintergrundLaeuft, 120);
                        if (t[1] == "KOMPLETT") o.UI(() => o.Starte(o.KomplettAblauf, false)); else o.UI(() => o.Starte(o.AuswahlAblauf, false));
                        WarteBis(() => o.BESCHAEFTIGT, 3); WarteBis(() => !o.BESCHAEFTIGT, 600);
                        Z("> ABLAUF ENDE verbunden=" + o.VERBUNDEN);
                        break;
                }
            }
            if (antworten.Count > 0) { Z("UNBENUTZTE ANTWORTEN " + antworten.Count); fehler++; }
        }
    }
}
