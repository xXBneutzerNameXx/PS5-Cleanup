// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// Fenster.cs - die Oberfläche (WPF). Das Fenster zeigt nur an und fragt -
// gelöscht wird ausschließlich über den geprüften Kern (SucheFunde,
// WaehleKomplett, FuehreLoeschung) mit derselben Schutzliste.
// Lange Arbeiten laufen in einem eigenen Faden; die Oberfläche bleibt frei.
// =====================================================================
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Markup;
using System.Windows.Media;
using System.Xml;

namespace PS5Cleanup
{
    public partial class Oberflaeche
    {
        public Window W;
        public Kern K = new Kern();
        public bool HintergrundLaeuft { get { var t = hintergrund; return t != null && t.T.IsAlive; } }
        TextBox tbIp, tbLog; Button btSuchen, btVerbinden, btProtokoll, btHilfe, btAuswahl, btKomplett, btAbbrechen;
        CheckBox cbModell, cbTest; TextBlock tbInfo, tbFuss, tbLeer, tbStand, tbUpdate; StackPanel spBereiche; ProgressBar pbFortschritt;

        public volatile bool BESCHAEFTIGT;   // eine Arbeit läuft
        public volatile bool LAEUFT;         // es wird gerade gelöscht - Fenster bleibt offen
        public volatile bool FENSTER_ZU;     // Fenster wurde geschlossen: keine Fragen mehr
        public volatile bool VERBUNDEN;
        string STAND_BASIS = "";
        public List<CheckBox> Kaestchen = new List<CheckBox>();
        List<CheckBox> KopfKaestchen = new List<CheckBox>();
        // Zählen vor dem Löschen (der Selbsttest ersetzt es durch eine Attrappe)
        public Func<Groesse> Zaehlen;
        public Groesse GROSS = new Groesse();

        // ---------------------------------------------------------------
        // Aufbau
        // ---------------------------------------------------------------
        public Oberflaeche()
        {
            using (var s = typeof(Oberflaeche).Assembly.GetManifestResourceStream("PS5Cleanup.Fenster.xaml"))
                W = (Window)XamlReader.Load(s);
            tbIp = (TextBox)W.FindName("tbIp"); tbLog = (TextBox)W.FindName("tbLog");
            btSuchen = (Button)W.FindName("btSuchen"); btVerbinden = (Button)W.FindName("btVerbinden");
            btProtokoll = (Button)W.FindName("btProtokoll"); btHilfe = (Button)W.FindName("btHilfe");
            btAuswahl = (Button)W.FindName("btAuswahl"); btKomplett = (Button)W.FindName("btKomplett");
            btAbbrechen = (Button)W.FindName("btAbbrechen");
            cbModell = (CheckBox)W.FindName("cbModell"); cbTest = (CheckBox)W.FindName("cbTest");
            tbInfo = (TextBlock)W.FindName("tbInfo"); tbFuss = (TextBlock)W.FindName("tbFuss");
            tbLeer = (TextBlock)W.FindName("tbLeer"); tbStand = (TextBlock)W.FindName("tbStand");
            tbUpdate = (TextBlock)W.FindName("tbUpdate");
            spBereiche = (StackPanel)W.FindName("spBereiche"); pbFortschritt = (ProgressBar)W.FindName("pbFortschritt");
            try { W.Icon = System.Windows.Media.Imaging.BitmapFrame.Create(typeof(Oberflaeche).Assembly.GetManifestResourceStream("PS5Cleanup.cleanup.ico")); } catch { }
            tbFuss.Text = "PS5 Cleanup " + Kern.VERSION + "  ·  da_Chris86  ·  Inoffizielles Community-Projekt, keine Verbindung zu Sony";
            Zaehlen = () => K.ErmittleGroessen();

            K.Melde = t => Log(t);
            K.GroesseStand = (b, n) => Stand(string.Format("Zähle nur die Größe - es wird noch NICHTS gelöscht ... bisher {0} in {1:N0} Dateien", Kern.GroesseText(b), n));

            btSuchen.Click += delegate { Starte(SuchenAblauf, true); };
            btVerbinden.Click += delegate { Starte(Verbinde, true); };
            btKomplett.Click += delegate { Starte(KomplettAblauf, false); };
            btAuswahl.Click += delegate { Starte(AuswahlAblauf, false); };
            btAbbrechen.Click += delegate { K.Abbruch = true; btAbbrechen.IsEnabled = false; Stand("Breche ab ..."); };
            btProtokoll.Click += delegate { ProtokollSpeichern(); };
            btHilfe.Click += delegate { HilfeOeffnen(); };
            cbTest.Click += delegate { if (VERBUNDEN) { Knoepfe(true); AktualisiereStand(); } };
            W.Closing += (s, e) =>
            {
                if (LAEUFT)
                {
                    e.Cancel = true;
                    Meldung("Es wird gerade auf der PS5 gelöscht. Bitte warten, bis 'Fertig' erscheint - ein Abbruch mittendrin hinterlässt halb gelöschte Spiele.", "PS5 Cleanup", MessageBoxImage.Warning);
                    return;
                }
                // Fenster geht zu (z.B. mitten im Zählen): alles anhalten, danach keine Fragen mehr
                FENSTER_ZU = true;
                K.Abbruch = true;
                var h = hintergrund; if (h != null) { h.Stopp = true; var f = h.F; if (f != null) f.Schliessen(); }
            };
            // kein QUIT mehr senden - ein Arbeitsfaden könnte die Verbindung gerade benutzen
            W.Closed += delegate { try { if (K.S != null) K.S.Ftp.Schliessen(); } catch { } Protokoll.Zeile("Ende."); };
        }

        // ---------------------------------------------------------------
        // Hilfen (aus jedem Faden aufrufbar)
        // ---------------------------------------------------------------
        public void UI(Action a)
        {
            if (W.Dispatcher.CheckAccess()) a(); else W.Dispatcher.Invoke(a);
        }
        T UI<T>(Func<T> f)
        {
            if (W.Dispatcher.CheckAccess()) return f();
            T r = default(T);
            W.Dispatcher.Invoke(new Action(() => r = f()));
            return r;
        }
        public void Log(string t)
        {
            Protokoll.Zeile(t);
            Action a = () => { tbLog.AppendText(t + "\r\n"); tbLog.ScrollToEnd(); };
            if (W.Dispatcher.CheckAccess()) a(); else W.Dispatcher.BeginInvoke(a);
        }
        public void Stand(string t)
        {
            Action a = () => { STAND_BASIS = t; tbStand.Text = t; };
            if (W.Dispatcher.CheckAccess()) a(); else W.Dispatcher.BeginInvoke(a);
        }
        public bool Testmodus() { return UI(() => cbTest.IsChecked == true); }
        bool FensterZu() { return FENSTER_ZU || !UI(() => W.IsVisible); }

        public void FehlerMelden(Exception ex, string wo)
        {
            Log("FEHLER bei " + wo + " : " + ex.Message);
            Protokoll.Zeile("   Stapel: " + (ex.StackTrace ?? "").Replace("\r\n", " | ").Replace("\n", " | "));
            Meldung("Es ist ein Fehler aufgetreten:\n" + ex.Message + "\n\nDas Protokoll liegt hier:\n" + Protokoll.Datei + "\n\nBitte diese Datei bei einer Fehlermeldung mitschicken.", "PS5 Cleanup", MessageBoxImage.Error);
        }

        // ---------------------------------------------------------------
        // Rückfragen. Ist das Fenster zu, gilt jede Frage als "Abbrechen".
        // ---------------------------------------------------------------
        // Oberflächentest: vorgegebene Antworten statt Dialoge (null = echte Dialoge)
        public Queue<object> TestAntworten;
        object TestAntwort(string titel, object sonst)
        {
            Log("[Test] Frage: " + titel);
            if (TestAntworten.Count == 0) return sonst;
            return TestAntworten.Dequeue();
        }
        public bool FrageJaNein(string text, string titel)
        {
            if (FensterZu()) return false;
            if (TestAntworten != null) return (bool)TestAntwort(titel, false);
            return UI(() => MessageBox.Show(W, text, titel, MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No) == MessageBoxResult.Yes);
        }
        public void Meldung(string text, string titel, MessageBoxImage art = MessageBoxImage.Information)
        {
            if (FensterZu()) return;
            if (TestAntworten != null) { Log("[Test] Meldung: " + titel); return; }
            UI(() => { MessageBox.Show(W, text, titel, MessageBoxButton.OK, art); });
        }
        const string DLG_KOPF = "<Window xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" " +
            "SizeToContent=\"Height\" WindowStartupLocation=\"CenterOwner\" Background=\"#1E1F24\" FontFamily=\"Segoe UI\" FontSize=\"13\" ResizeMode=\"NoResize\" ";
        static Window Dialog(string xaml) { return (Window)XamlReader.Load(XmlReader.Create(new StringReader(xaml))); }

        public string FrageText(string text, string titel)
        {
            if (FensterZu()) return null;
            if (TestAntworten != null) return (string)TestAntwort(titel, null);
            return UI(() =>
            {
                var d = Dialog(DLG_KOPF + "Width=\"520\"><StackPanel Margin=\"16\">" +
                    "<TextBlock x:Name=\"tx\" Foreground=\"#E8E8EA\" TextWrapping=\"Wrap\" Margin=\"0,0,0,10\"/>" +
                    "<TextBox x:Name=\"te\" Background=\"#16171B\" Foreground=\"#E8E8EA\" BorderBrush=\"#3A3D47\" Padding=\"6,4\" CaretBrush=\"White\" FontSize=\"14\"/>" +
                    "<StackPanel Orientation=\"Horizontal\" HorizontalAlignment=\"Right\" Margin=\"0,12,0,0\">" +
                    "<Button x:Name=\"bAbbr\" Content=\"Abbrechen\" Padding=\"12,6\" Margin=\"4\" Foreground=\"White\" Background=\"#2A2C33\" BorderBrush=\"#3A3D47\"/>" +
                    "<Button x:Name=\"bOk\" Content=\"OK\" Padding=\"12,6\" Margin=\"4\" Foreground=\"White\" Background=\"#2F7BFF\" BorderBrush=\"#2F7BFF\" IsDefault=\"True\"/>" +
                    "</StackPanel></StackPanel></Window>");
                d.Title = titel; if (W.IsVisible) d.Owner = W;
                ((TextBlock)d.FindName("tx")).Text = text;
                var te = (TextBox)d.FindName("te");
                ((Button)d.FindName("bOk")).Click += delegate { d.DialogResult = true; d.Close(); };
                ((Button)d.FindName("bAbbr")).Click += delegate { d.DialogResult = false; d.Close(); };
                te.Focus();
                if (d.ShowDialog() == true) return te.Text.Trim();
                return (string)null;
            });
        }
        // Entscheidung mit zwei klar beschrifteten Knöpfen. Der sichere Knopf ist
        // vorbelegt (Enter/Esc = sicher), der gefährliche rot und muss angeklickt werden.
        public bool FrageWahl(string text, string titel, string sicher, string gefaehrlich)
        {
            if (FensterZu()) return false;
            if (TestAntworten != null) return (bool)TestAntwort(titel, false);
            return UI(() =>
            {
                var d = Dialog(DLG_KOPF + "Width=\"560\"><StackPanel Margin=\"16\">" +
                    "<TextBlock x:Name=\"tx\" Foreground=\"#E8E8EA\" TextWrapping=\"Wrap\" Margin=\"0,0,0,10\"/>" +
                    "<StackPanel Orientation=\"Horizontal\" HorizontalAlignment=\"Right\" Margin=\"0,12,0,0\">" +
                    "<Button x:Name=\"bGef\" Padding=\"12,6\" Margin=\"4\" Foreground=\"White\" Background=\"#8B2A2A\" BorderBrush=\"#AA3333\"/>" +
                    "<Button x:Name=\"bSicher\" Padding=\"12,6\" Margin=\"4\" Foreground=\"White\" Background=\"#2F7BFF\" BorderBrush=\"#2F7BFF\" IsDefault=\"True\" IsCancel=\"True\"/>" +
                    "</StackPanel></StackPanel></Window>");
                d.Title = titel; if (W.IsVisible) d.Owner = W;
                ((TextBlock)d.FindName("tx")).Text = text;
                var bs = (Button)d.FindName("bSicher"); var bg = (Button)d.FindName("bGef");
                bs.Content = sicher; bg.Content = gefaehrlich;
                bool wahl = false;
                bg.Click += delegate { wahl = true; d.Close(); };
                bs.Click += delegate { wahl = false; d.Close(); };
                bs.Focus();
                d.ShowDialog();
                return wahl;
            });
        }

        // ---------------------------------------------------------------
        // Sperren, Knöpfe, Arbeiten starten
        // ---------------------------------------------------------------
        void Sperre(bool an, bool abbrechbar)
        {
            UI(() =>
            {
                BESCHAEFTIGT = an;
                spBereiche.IsEnabled = !an; cbTest.IsEnabled = !an; cbModell.IsEnabled = !an; tbIp.IsEnabled = !an;
                btAbbrechen.Visibility = an && abbrechbar ? Visibility.Visible : Visibility.Collapsed;
                btAbbrechen.IsEnabled = true;
                Knoepfe(!an);
            });
        }
        public void Knoepfe(bool an)
        {
            UI(() =>
            {
                if (BESCHAEFTIGT) an = false;
                foreach (var b in new[] { btSuchen, btVerbinden, btAuswahl, btKomplett }) b.IsEnabled = an;
                // Löschen nur mit einer gültigen Liste der gerade verbundenen PS5
                bool test = cbTest.IsChecked == true;
                if (an && (K.Funde.Count == 0 || test || !VERBUNDEN)) { btAuswahl.IsEnabled = false; btKomplett.IsEnabled = false; }
                if (an && !test && VERBUNDEN && K.Funde.Count > 0) { btKomplett.IsEnabled = true; btAuswahl.IsEnabled = K.Gewaehlt().Count > 0; }
            });
        }

        // Arbeit in eigenem Faden; Fehler landen im Protokoll und als Meldung.
        public void Starte(Action ablauf, bool abbrechbar)
        {
            if (BESCHAEFTIGT) return;
            K.Abbruch = false;
            Sperre(true, abbrechbar);
            var t = new Thread(() =>
            {
                try { ablauf(); }
                catch (AbbruchAusnahme) { if (!FENSTER_ZU) { Log("Abgebrochen - nichts gelöscht."); Stand("Abgebrochen."); K.SmZuruecksetzen(); if (VERBUNDEN) HintergrundStarten(); } }
                catch (Exception ex) { if (!FENSTER_ZU) { FehlerMelden(ex, "dem Ablauf"); UI(() => { pbFortschritt.IsIndeterminate = false; }); } }
                finally
                {
                    if (!FENSTER_ZU)
                    {
                        UI(() => { pbFortschritt.IsIndeterminate = false; });
                        Sperre(false, false);
                        if (VERBUNDEN) AktualisiereStand();
                    }
                }
            });
            t.IsBackground = true;
            t.SetApartmentState(ApartmentState.STA);
            t.Start();
        }

        // ---------------------------------------------------------------
        // PS5 im Heimnetz suchen (FTP-Port 2121 auf allen eigenen /24-Netzen)
        // ---------------------------------------------------------------
        void SuchenAblauf()
        {
            var netze = new List<string>();
            foreach (var ni in NetworkInterface.GetAllNetworkInterfaces())
            {
                if (ni.OperationalStatus != OperationalStatus.Up) continue;
                foreach (var ua in ni.GetIPProperties().UnicastAddresses)
                {
                    if (ua.Address.AddressFamily != AddressFamily.InterNetwork) continue;
                    string t = ua.Address.ToString();
                    if (t.StartsWith("127.") || t.StartsWith("169.254.")) continue;
                    string n = t.Substring(0, t.LastIndexOf('.'));
                    if (!netze.Contains(n)) netze.Add(n);
                }
            }
            netze.Sort(StringComparer.Ordinal);
            var treffer = new List<string>();
            foreach (var n in netze)
            {
                if (K.Abbruch) throw new AbbruchAusnahme();
                Stand("Suche im Netz " + n + ".0 ...");
                var clients = new List<KeyValuePair<string, TcpClient>>();
                for (int h = 1; h <= 254; h++)
                {
                    var c = new TcpClient();
                    try { c.BeginConnect(n + "." + h, Kern.STANDARD_PORT, null, null); } catch { }
                    clients.Add(new KeyValuePair<string, TcpClient>(n + "." + h, c));
                }
                Thread.Sleep(1500);
                foreach (var e in clients)
                {
                    try { if (e.Value.Connected) treffer.Add(e.Key); } catch { }
                    try { e.Value.Close(); } catch { }
                }
            }
            UI(() =>
            {
                if (treffer.Count == 0) { Stand("Keine PS5 mit FTP-Server gefunden (ist der Jailbreak geladen?)."); return; }
                tbIp.Text = treffer[0];
                Stand(treffer.Count == 1 ? "Gefunden: " + treffer[0] : "Gefunden: " + string.Join(", ", treffer) + " - die erste ist eingetragen.");
            });
        }

        // ---------------------------------------------------------------
        // Verbinden und nachschauen
        // ---------------------------------------------------------------
        public void Verbinde()
        {
            string eingabe = UI(() => Regex.Replace(tbIp.Text, "\\s", ""));
            string ip = eingabe; int port = Kern.STANDARD_PORT;
            var m = Regex.Match(eingabe, "^(.+):(\\d+)$");
            if (m.Success) { ip = m.Groups[1].Value; int.TryParse(m.Groups[2].Value, out port); }
            if (!Regex.IsMatch(ip, "^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$") || port < 1 || port > 65535) { Meldung("Bitte eine gültige IP-Adresse eingeben (Beispiel: 192.168.1.50).", "PS5 Cleanup", MessageBoxImage.Warning); return; }
            HintergrundAnhalten();
            // Alte Liste sofort verwerfen - sie gehört evtl. zu einer anderen PS5
            VERBUNDEN = false; K.FUNDE_IP = ""; K.DDD_STATUS = null;
            K.Funde = new List<Eintrag>();
            UI(() =>
            {
                tbInfo.Text = "Verbinde ...";
                spBereiche.Children.Clear(); Kaestchen.Clear(); KopfKaestchen.Clear();
                tbLeer.Visibility = Visibility.Visible; spBereiche.Children.Add(tbLeer);
                tbLog.Clear();
            });
            Log("Verbinde mit PS5 (" + ip + ", Port " + port + ") ...");
            bool ok = false;
            try { ok = K.Verbinden(ip, port) && K.S.Liste("/user") != null; } catch (AbbruchAusnahme) { throw; } catch { ok = false; }
            if (!ok)
            {
                Log("FEHLER: PS5 nicht erreichbar.");
                UI(() => tbInfo.Text = "Nicht erreichbar. Ist die PS5 an, der Jailbreak geladen und läuft der FTP-Server (Port " + port + ")? " + FTP_TIPP);
                return;
            }
            K.S.Ftp.Wartet = ms => Warteanzeige(ms);
            Log("Verbunden.");
            string firmware = K.FirmwareLesen();
            string modell = "";
            if (UI(() => cbModell.IsChecked == true)) { Log("Frage das Modell ab ..."); modell = K.ModellLesen(ip, Programm.Ressource("ps5info.elf")); }
            if (modell == "")
            {
                modell = FrageText("Modell konnte nicht automatisch erkannt werden.\nDie Modellnummer steht auf dem Aufkleber der Konsole oder unter Einstellungen > System > Systemsoftware > Konsoleninformationen.\n\nModellnummer (z.B. CFI-7016), leer = überspringen:", "Modell") ?? "";
                modell = Regex.Replace(modell, "\\s", "");
            }
            K.MODELL_TYP = Kern.ModellTyp(modell);
            K.MODELL_DIGITAL = Kern.ModellDigital(modell);
            string reihe = Kern.ModellReihe(K.MODELL_TYP, K.MODELL_DIGITAL);
            if (modell == "") modell = "unbekannt";
            string info = "Modell: " + modell + "   |   Reihe: " + reihe + "   |   Firmware: " + firmware + "   |   " + K.LaufwerkText();
            UI(() => tbInfo.Text = info);
            Log("Modell: " + modell + "  Reihe: " + reihe + "  Firmware: " + firmware);
            Log("Laufwerk: " + K.LaufwerkText());
            Log("Schaue nach, was drauf ist ...");
            Stand("Schaue nach, was auf der PS5 liegt ...");
            UI(() => pbFortschritt.IsIndeterminate = true);
            try { K.SucheFunde(); }
            catch (AbbruchAusnahme) { throw; }
            catch (Exception ex) { throw new Exception(ex.Message + "\n\n" + FTP_TIPP, ex); }
            UI(() => pbFortschritt.IsIndeterminate = false);
            if (K.KOMPRIMIERUNG.Count > 0)
            {
                Log("ACHTUNG: Der Game Compressor arbeitet gerade (oder wurde abgebrochen):");
                foreach (var z in K.KOMPRIMIERUNG) Log("   " + z);
                Log("Das betroffene Spiel ist GESPERRT. Lass den Game Compressor erst fertig werden.");
            }
            VERBUNDEN = K.FUNDE_IP != "";
            UI(() => { tbLeer.Visibility = Visibility.Collapsed; ZeigeBereiche(); });
            if (K.Funde.Count == 0) Log("Nichts zu tun - die PS5 ist bereits sauber.");
            else Log(K.Funde.Count + " Einträge gefunden. Nichts wurde verändert.");
            if (Testmodus()) Log("(Testmodus - zum Löschen den Haken \"Testmodus\" entfernen.)");
            HintergrundStarten();
        }

        void Warteanzeige(int ms)
        {
            if (ms > 3000) { int s = ms / 1000; Action a = () => tbStand.Text = STAND_BASIS + "   [PS5 arbeitet seit " + s + " s an einer großen Datei ...]"; W.Dispatcher.BeginInvoke(a); }
            else if (ms == 0) { Action a = () => { if (tbStand.Text != STAND_BASIS) tbStand.Text = STAND_BASIS; }; W.Dispatcher.BeginInvoke(a); }
        }

        // ---------------------------------------------------------------
        // Größen im Hintergrund (NEU 1.0.4)
        // ---------------------------------------------------------------
        // Jeder Zähl-Lauf hat sein eigenes Stopp-Zeichen und seine eigene Verbindung.
        class HLauf { public volatile bool Stopp; public Thread T; public volatile Ftp F; }
        HLauf hintergrund;
        void HintergrundStarten()
        {
            HintergrundAnhalten();
            if (!VERBUNDEN || K.Funde.Count == 0 || FENSTER_ZU) return;
            var lauf = new HLauf();
            var liste = K.Funde;
            lauf.T = new Thread(() =>
            {
                int n = 0, gesamt = liste.Count;
                K.GroessenImHintergrund(() => lauf.Stopp || FENSTER_ZU, e =>
                {
                    n++;
                    int nn = n;
                    W.Dispatcher.BeginInvoke(new Action(() => { if (lauf.Stopp || K.Funde != liste) return; ZeigeGroesse(e); if (!BESCHAEFTIGT) AktualisiereStand(nn < gesamt ? "Größen werden ermittelt (" + nn + " von " + gesamt + ") ..." : null); }));
                }, f => lauf.F = f);
                if (!lauf.Stopp && !FENSTER_ZU) W.Dispatcher.BeginInvoke(new Action(() => { if (!lauf.Stopp && !BESCHAEFTIGT) AktualisiereStand(); }));
            });
            lauf.T.IsBackground = true;
            hintergrund = lauf;
            lauf.T.Start();
        }
        // Hält die Zählung an und wartet, bis ihr Faden wirklich weg ist (die Verbindung
        // wird dazu geschlossen) - erst danach geht es zum Löschen weiter.
        void HintergrundAnhalten()
        {
            var lauf = hintergrund;
            hintergrund = null;
            if (lauf == null) return;
            lauf.Stopp = true;
            var f = lauf.F; if (f != null) f.Schliessen();
            if (lauf.T != Thread.CurrentThread) lauf.T.Join();
        }

        static string GroesseKurz(Groesse g) { return (g.Gekappt ? "mind. " : "") + Kern.GroesseText(g.Bytes); }

        // ---------------------------------------------------------------
        // Bereiche anzeigen. Jedes Kästchen trägt in .Tag den Eintrag selbst.
        // ---------------------------------------------------------------
        public void ZeigeBereiche()
        {
            spBereiche.Children.Clear(); Kaestchen.Clear(); KopfKaestchen.Clear();
            foreach (var k in Kern.ALLE_KAT)
            {
                var eintraege = K.Funde.Where(f => f.Kat == k).ToList();
                var ex = new Expander();
                ex.IsExpanded = eintraege.Count > 0 && eintraege.Count <= 15;
                ex.Margin = new Thickness(0, 4, 0, 4);
                var alle = new CheckBox();
                alle.FontWeight = FontWeights.SemiBold; alle.FontSize = 14;
                alle.IsEnabled = eintraege.Count > 0;
                alle.Tag = k;
                ex.Header = alle;
                var inhalt = new StackPanel { Margin = new Thickness(24, 2, 0, 6) };
                inhalt.Children.Add(new TextBlock { Text = Kern.KatInfo[k], Foreground = (Brush)W.FindResource("Grau"), Margin = new Thickness(0, 0, 0, 6) });
                if (k == 1 && eintraege.Any(f => f.Quelle != ""))
                    inhalt.Children.Add(new TextBlock { Foreground = (Brush)W.FindResource("Gelb"), Margin = new Thickness(0, 0, 0, 6),
                        Text = "ShadowMount-Spiel: Im Menü ist nur eine Kachel, das Spiel selbst liegt auf der M.2 oder der internen SSD. Beim Löschen wirst du gefragt, ob die Spieldateien mit weg sollen." });
                var kinder = new List<CheckBox>();
                foreach (var f in eintraege)
                {
                    var cb = new CheckBox();
                    cb.Tag = f;
                    cb.Content = KaestchenInhalt(f);
                    if (f.Gesperrt) { cb.IsEnabled = false; cb.ToolTip = "Der Game Compressor arbeitet gerade daran - gesperrt."; }
                    if (f.Text.Contains("Kachel geht mit") || f.Text.Contains("tote Kachel")) cb.ToolTip = "ShadowMount: Wird das Spiel gelöscht, verschwindet auch seine Kachel im Menü.";
                    cb.Click += (s, e) =>
                    {
                        var c = (CheckBox)s; var ff = (Eintrag)c.Tag;
                        ff.Wahl = c.IsChecked == true;
                        // Spiel abgewählt -> seine Updates/DLCs auch wieder abwählen
                        if (ff.IstTeil("Spiel") && !ff.Wahl)
                            foreach (var x in K.Funde) if (x.Gruppe != null && x.Gruppe == ff.Gruppe && !x.IstTeil("Spiel")) x.Wahl = false;
                        GruppenAbgleich(); AktualisiereStand();
                    };
                    inhalt.Children.Add(cb);
                    Kaestchen.Add(cb); kinder.Add(cb);
                }
                alle.Click += (s, e) =>
                {
                    var a = (CheckBox)s;
                    foreach (var c in kinder) { var ff = (Eintrag)c.Tag; if (!ff.Gesperrt) { c.IsChecked = a.IsChecked; ff.Wahl = a.IsChecked == true; } }
                    GruppenAbgleich(); AktualisiereStand();
                };
                KopfKaestchen.Add(alle);
                KopfText(alle);
                ex.Content = inhalt;
                spBereiche.Children.Add(ex);
            }
            GruppenAbgleich();
            AktualisiereStand();
        }
        object KaestchenInhalt(Eintrag f)
        {
            var tb = new TextBlock { TextWrapping = TextWrapping.Wrap };
            var r = new Run(f.Text);
            if (f.Paket) r.Foreground = (Brush)W.FindResource("Gelb");
            tb.Inlines.Add(r);
            var g = K.EintragGroesse(f, true);
            if (g != null) tb.Inlines.Add(new Run("   " + GroesseKurz(g)) { Foreground = (Brush)W.FindResource("Grau") });
            return tb;
        }
        void ZeigeGroesse(Eintrag e)
        {
            foreach (var cb in Kaestchen) if (cb.Tag == e) cb.Content = KaestchenInhalt(e);
            foreach (var a in KopfKaestchen) if ((int)a.Tag == e.Kat) KopfText(a);
        }
        void KopfText(CheckBox alle)
        {
            int k = (int)alle.Tag;
            var eintraege = K.Funde.Where(f => f.Kat == k).ToList();
            if (k == 7 && !K.m2Da) { alle.Content = "[7] " + Kern.KatName[7] + "   (keine M.2 gefunden)"; return; }
            string t = "[" + k + "] " + Kern.KatName[k] + "   (" + eintraege.Count + " Einträge";
            if (eintraege.Count > 0)
            {
                var sum = new Groesse(); bool alleDa = true;
                foreach (var f in eintraege) { var g = K.EintragGroesse(f, true); if (g == null) { alleDa = false; break; } sum.Plus(g); }
                if (alleDa) t += " · " + GroesseKurz(sum);
            }
            alle.Content = t + ")";
        }
        public void AktualisiereStand(string zusatz = null)
        {
            UI(() =>
            {
                var gew = K.Gewaehlt();
                int n = gew.Count;
                string t = n == 0 ? "Nichts ausgewählt." : n + " Einträge ausgewählt.";
                if (n > 0)
                {
                    var sum = new Groesse(); bool alleDa = true;
                    var gesehen = new HashSet<string>(StringComparer.Ordinal);
                    foreach (var f in gew)
                    {
                        var g = K.EintragGroesse(f, true);
                        if (g == null) { alleDa = false; break; }
                        sum.Plus(g);
                    }
                    if (alleDa) t = n + " Einträge ausgewählt (" + GroesseKurz(sum) + ").";
                }
                if (cbTest.IsChecked == true) t = "TESTMODUS - es wird nichts gelöscht.  " + t;
                if (zusatz != null) t += "   " + zusatz;
                STAND_BASIS = t; tbStand.Text = t;
                btAuswahl.IsEnabled = n > 0 && cbTest.IsChecked != true && !BESCHAEFTIGT && VERBUNDEN;
            });
        }
        public void ZeigeHaken() { UI(() => { foreach (var cb in Kaestchen) cb.IsChecked = ((Eintrag)cb.Tag).Wahl; GruppenAbgleich(); }); }
        // Ist ein Spiel angehakt, sind sein Update und seine DLCs ebenfalls
        // angehakt und gesperrt (sie gehen mit). Sonst sind sie frei wählbar.
        public void GruppenAbgleich()
        {
            K.AbhaengigeMitnehmen();
            var basis = H.Tab<int>();
            foreach (var f in K.Funde) if (f.Wahl && f.IstTeil("Spiel")) basis[f.Gruppe] = 1;
            foreach (var cb in Kaestchen)
            {
                var f = (Eintrag)cb.Tag;
                if (f.Teil == null || f.Teil == "Spiel" || f.Gesperrt) continue;
                cb.IsChecked = f.Wahl;
                if (basis.ContainsKey(f.Gruppe)) { cb.IsEnabled = false; cb.ToolTip = "Geht mit dem Spiel weg."; }
                else { cb.IsEnabled = true; cb.ToolTip = null; }
            }
        }

        // ---------------------------------------------------------------
        // Löschen
        // ---------------------------------------------------------------
        public string Zusammenfassung(bool komplett)
        {
            var g = K.Gewaehlt();
            string t = "Wird ENDGÜLTIG gelöscht: " + g.Count + " Einträge\n";
            foreach (var k in Kern.ALLE_KAT)
            {
                int c = g.Count(f => f.Kat == k);
                if (c > 0) t += "   - " + Kern.KatName[k] + ": " + c + "\n";
            }
            int m2 = g.Count(f => f.Kat == 7);
            if (komplett && m2 == 0) t += "   (M.2-SSD bleibt unverändert)\n";
            if (!komplett && m2 > 0) t = "!!! ACHTUNG: " + m2 + " EINTRÄGE DER M.2-SSD SIND GEWÄHLT - was darin liegt (Spiele, Dateien), ist danach weg !!!\n\n" + t;
            int gesp = K.Funde.Count(f => f.Gesperrt);
            if (gesp > 0) t += "   Bleibt (Game Compressor arbeitet daran): " + gesp + "\n";
            t += "\nDas sind " + Kern.GroesseSatz(GROSS) + ".\n";
            t += "\n" + K.KopplungsText() + "\nBenutzung auf eigene Gefahr - Gelöschtes ist endgültig weg.";
            return t;
        }

        // Zählen vor der Abschlussfrage; false = abgebrochen / Fenster zu
        public bool GroesseErmitteln()
        {
            bool fehlt = K.Gewaehlt().Any(f => f.Pfade.Any(p => !K.PfadGezaehlt(p)));
            if (fehlt)
            {
                Log("Zähle, wie viel gewählt ist (noch wird NICHTS gelöscht - danach kommt die Abschlussfrage) ...");
                Stand("Zähle nur die Größe - es wird noch NICHTS gelöscht ...");
                UI(() => { btAbbrechen.Visibility = Visibility.Visible; btAbbrechen.IsEnabled = true; pbFortschritt.IsIndeterminate = true; });
            }
            try { GROSS = Zaehlen(); }
            finally { UI(() => { btAbbrechen.Visibility = Visibility.Collapsed; pbFortschritt.IsIndeterminate = false; }); }
            if (FensterZu()) return false;
            if (K.Abbruch) throw new AbbruchAusnahme();
            Log("Ausgewählt: " + Kern.GroesseSatz(GROSS) + ".");
            return true;
        }

        public void KomplettAblauf()
        {
            HintergrundAnhalten();
            string t = "KOMPLETT: Auf der internen SSD wird ALLES gelöscht:\n";
            foreach (var k in new[] { 1, 2, 3, 4, 5, 6 }) t += "   - " + Kern.KatName[k] + ": " + K.AnzahlKomplett(k) + "\n";
            t += "\nDas heißt:\n - Spielstände ohne Cloud-Sicherung (PS Plus) sind endgültig weg.\n - Nie mit dem PSN abgeglichene Trophäen sind weg.\n - Die Jailbreak-Tools samt Einstellungen sind weg - danach musst du deinen Jailbreak NEU EINRICHTEN.\n - Wer per Browser-Seite jailbreakt: ein offline gespeicherter Zugang im Browserspeicher kann auch weg sein.\n\nBleibt: Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, Lizenzen und die Sony-Systemanwendungen.";
            if (!K.m2Da) t += "\n\nHINWEIS: Es wurde keine M.2-SSD gefunden. Steckt eine M.2 mit Spielen in der PS5, die gerade nicht erkannt wird: jetzt abbrechen und die PS5 neu starten. Sonst gelten diese Spiele als nicht installiert und ihre Lizenzen und Kacheln werden mit gelöscht.";
            t += "\n\nWeiter?";
            if (!FrageJaNein(t, "KOMPLETT")) { HintergrundStarten(); return; }
            bool m2Leeren = false;
            if (K.m2Da && K.Anzahl(7) > 0)
                m2Leeren = FrageWahl("Auf der M.2-SSD liegen " + K.Anzahl(7) + " Einträge (Spiele, Ordner, Dateien).\n\nM.2 BEHALTEN: Die Spiele auf der M.2 bleiben unangetastet. Nur die Kacheln im Menü gehen weg - sie kommen wieder, sobald ShadowMount neu eingerichtet ist.\n\nM.2 AUCH LEEREN: Alle Spiele und Dateien auf der M.2 werden endgültig gelöscht.", "M.2-SSD", "M.2 behalten (empfohlen)", "M.2 auch leeren");
            K.WaehleKomplett(m2Leeren);
            ZeigeHaken(); AktualisiereStand();
            if (!m2Leeren && K.HatPakete()) Log("(Trophäen-Pakete der Spiele bleiben, weil die Spiele auf der M.2 bleiben.)");
            bool weiter;
            try { weiter = GroesseErmitteln(); }
            catch (AbbruchAusnahme) { foreach (var f in K.Funde) f.Wahl = false; K.SmZuruecksetzen(); ZeigeHaken(); throw; }
            if (!weiter) { Protokoll.Zeile("Fenster geschlossen - abgebrochen, nichts gelöscht."); return; }
            string warn = m2Leeren ? "!!! ACHTUNG: DIE M.2-SSD WIRD MIT GELEERT - " + K.Anzahl(7) + " Einträge, alle Spiele darauf sind danach weg !!!\n\n" : "Die M.2-SSD bleibt unangetastet.\n\n";
            string a = FrageText(warn + Zusammenfassung(true) + "\n\nZum Bestätigen LÖSCHEN eintippen (alles andere = abbrechen):", "Endgültig löschen?");
            if (a != "LOESCHEN" && a != "LÖSCHEN")
            {
                Log("Abgebrochen - nichts verändert.");
                foreach (var f in K.Funde) f.Wahl = false;
                K.SmZuruecksetzen(); ZeigeHaken(); AktualisiereStand(); HintergrundStarten();
                return;
            }
            LoescheJetzt();
        }

        public void AuswahlAblauf()
        {
            HintergrundAnhalten();
            var sm = K.SmGewaehlt();
            if (sm.Count > 0)
            {
                string l = string.Join("\n", sm.Select(f => "   " + f.Text));
                bool mit = FrageWahl("Du hast " + sm.Count + " ShadowMount-Spiel(e) gewählt:\n" + l + "\n\nNUR KACHELN LÖSCHEN: Die Spiele bleiben auf der Platte und erscheinen wieder, sobald ShadowMount neu geladen ist.\n\nSPIELE + KACHELN LÖSCHEN: Die Spieldateien werden endgültig gelöscht.", "ShadowMount-Spiele", "Nur Kacheln löschen (empfohlen)", "Spiele + Kacheln löschen");
                K.SetzeSmSpieldateien(mit);
            }
            else K.SmZuruecksetzen();
            var m2 = K.M2Gewaehlt();
            if (m2.Count > 0)
            {
                string l = string.Join("\n", m2.Take(15).Select(f => "   " + f.Text));
                if (m2.Count > 15) l += "\n   ... und " + (m2.Count - 15) + " weitere";
                if (!FrageWahl("ACHTUNG - du hast " + m2.Count + " Einträge der M.2-SSD gewählt:\n" + l + "\n\nAlles, was darin liegt (Spiele, Ordner, Dateien), wird endgültig gelöscht.\n\nGelöscht wird erst nach der Abschlussfrage.", "M.2-SSD", "Abbrechen", "Weiter (M.2-Einträge löschen)"))
                { Log("Abgebrochen - nichts verändert."); K.SmZuruecksetzen(); HintergrundStarten(); return; }
            }
            if (!GroesseErmitteln()) { Protokoll.Zeile("Fenster geschlossen - abgebrochen, nichts gelöscht."); return; }
            if (!FrageWahl(Zusammenfassung(false), "Endgültig löschen?", "Abbrechen", "Jetzt endgültig löschen"))
            { Log("Abgebrochen - nichts verändert."); K.SmZuruecksetzen(); HintergrundStarten(); return; }
            LoescheJetzt();
        }

        public Action LoescheJetztErsatz;     // nur für den Selbsttest
        void LoescheJetzt()
        {
            if (LoescheJetztErsatz != null) { LoescheJetztErsatz(); return; }
            UI(() => { pbFortschritt.IsIndeterminate = true; btAbbrechen.Visibility = Visibility.Collapsed; });
            Stand("Lösche ... (große Spiele können dauern, das Fenster bleibt dabei am Leben)");
            Log("");
            LAEUFT = true;
            Ergebnis erg;
            try
            {
                try
                {
                erg = K.FuehreLoeschung(() => FrageWahl("ShadowMount läuft womöglich noch. Dann werden gelöschte Kacheln gleich wieder angelegt und die Spiele könnten gerade eingebunden sein.\n\nEmpfehlung: abbrechen, die PS5 neu starten, nur den FTP-Server laden und dann nochmal löschen.", "ShadowMount", "Abbrechen (empfohlen)", "Trotzdem löschen"));
                }
                catch (AbbruchAusnahme)
                {
                    // kann heute nicht vorkommen (Abbrechen ist beim Löschen gesperrt) - aber dann ehrlich melden
                    erg = new Ergebnis { Fehler = 1 };
                    Log("Löschen wurde unterbrochen - ein Teil kann schon gelöscht sein. Bitte das Protokoll prüfen.");
                }
            }
            finally { LAEUFT = false; }
            UI(() => pbFortschritt.IsIndeterminate = false);
            Stand("");
            if (erg.Abgebrochen) { Log("Abgebrochen - nichts verändert."); K.SmZuruecksetzen(); return; }
            UI(() => pbFortschritt.Value = 100);
            Log("");
            Log(erg.Fehler == 0 ? "Fertig - alles Ausgewählte ist gelöscht." : "Fertig, aber mit " + erg.Fehler + " Fehler(n) - siehe oben.");
            Log(Kern.FreigeraeumtText(erg.Frei));
            Log(K.KopplungsText());
            if (erg.SmText != "") Log(erg.SmText);
            foreach (var z in Kern.SCHLUSS_TEXT) Log(z);
            string smt = erg.SmText != "" ? erg.SmText + "\n\n" : "";
            Meldung((erg.Fehler == 0 ? "Fertig - alles Ausgewählte ist gelöscht." : "Fertig, aber mit " + erg.Fehler + " Fehler(n) - siehe Protokoll.") + "\n\n" +
                    Kern.FreigeraeumtText(erg.Frei) + "\n\n" + smt + string.Join("\n", Kern.SCHLUSS_TEXT) + "\n\nProtokoll: " + Protokoll.Datei, "Fertig");
            // neu nachschauen, damit die Liste stimmt. Klappt das nicht, keine alte
            // Liste stehen lassen - lieber neu verbinden lassen.
            try { K.Abbruch = false; K.SucheFunde(); }
            catch (Exception ex)
            {
                Log("Konnte nicht neu nachschauen: " + ex.Message + " - bitte neu verbinden.");
                K.Funde = new List<Eintrag>(); K.FUNDE_IP = "";
            }
            VERBUNDEN = K.FUNDE_IP != "";
            UI(() => { ZeigeBereiche(); pbFortschritt.Value = 0; });
            HintergrundStarten();
        }

        // ---------------------------------------------------------------
        // Protokoll speichern, Hilfe
        // ---------------------------------------------------------------
        void ProtokollSpeichern()
        {
            var d = new Microsoft.Win32.SaveFileDialog();
            d.Title = "Protokoll speichern"; d.Filter = "Textdatei (*.log;*.txt)|*.log;*.txt|Alle Dateien|*.*";
            d.FileName = "PS5-Cleanup_" + DateTime.Now.ToString("yyyy-MM-dd_HH-mm") + ".log";
            if (d.ShowDialog(W) != true) return;
            try
            {
                string inhalt = (Protokoll.Datei != "" && File.Exists(Protokoll.Datei)) ? File.ReadAllText(Protokoll.Datei) : tbLog.Text;
                File.WriteAllText(d.FileName, inhalt, new UTF8Encoding(true));
                Log("Protokoll gespeichert: " + d.FileName);
            }
            catch (Exception ex) { Meldung("Konnte nicht speichern: " + ex.Message, "PS5 Cleanup", MessageBoxImage.Error); }
        }
        void HilfeOeffnen()
        {
            string p = Programm.LiesmichPfad();
            if (p != "") { try { Process.Start("notepad.exe", "\"" + p + "\""); } catch { } }
            else Meldung("LIESMICH.txt nicht gefunden.", "PS5 Cleanup");
        }

        // ---------------------------------------------------------------
        // Hinweis auf neue Versionen (NEU 1.0.4): fragt einmal beim Start bei
        // GitHub nach der neuesten Version - nur anzeigen, nichts herunterladen.
        // ---------------------------------------------------------------
        // Rückmeldung aus dem Forum: mit anderen FTP-Servern klappt das Lesen nicht zuverlässig
        public const string FTP_TIPP = "Empfohlen: der FTP-Server ftpsrv von drakmor (getestet mit Version 1.16).";
        public const string RELEASES = "https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases/latest";
        public void UpdatePruefen()
        {
            var t = new Thread(() =>
            {
                try
                {
                    ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;
                    var req = (HttpWebRequest)WebRequest.Create("https://api.github.com/repos/xXBneutzerNameXx/PS5-Cleanup/releases/latest");
                    req.UserAgent = "PS5-Cleanup/" + Kern.VERSION;
                    req.Accept = "application/vnd.github+json";
                    req.Timeout = 8000;
                    string json;
                    using (var r = req.GetResponse()) using (var sr = new StreamReader(r.GetResponseStream(), Encoding.UTF8)) json = sr.ReadToEnd();
                    var m = Regex.Match(json, "\"tag_name\"\\s*:\\s*\"([^\"]+)\"");
                    if (!m.Success) return;
                    string neu = m.Groups[1].Value;
                    if (!Neuer(neu, Kern.VERSION)) return;
                    string anzeige = Regex.Replace(neu, "^[vV]", "").Replace("-beta", " Beta");
                    W.Dispatcher.BeginInvoke(new Action(() =>
                    {
                        tbUpdate.Inlines.Clear();
                        tbUpdate.Inlines.Add(new Run("Es gibt eine neuere Version: " + anzeige + "  "));
                        var link = new Hyperlink(new Run("auf GitHub ansehen")) { NavigateUri = new Uri(RELEASES), Foreground = (Brush)W.FindResource("Blau") };
                        link.RequestNavigate += (s, e) => { try { Process.Start(new ProcessStartInfo(RELEASES) { UseShellExecute = true }); } catch { } };
                        tbUpdate.Inlines.Add(link);
                        tbUpdate.Visibility = Visibility.Visible;
                    }));
                    Protokoll.Zeile("Hinweis: neuere Version auf GitHub: " + neu);
                }
                catch { }
            });
            t.IsBackground = true;
            t.Start();
        }
        public static bool Neuer(string tag, string eigene)
        {
            Func<string, int[]> zahlen = s =>
            {
                var m = Regex.Match(s ?? "", "(\\d+)\\.(\\d+)(?:\\.(\\d+))?");
                if (!m.Success) return null;
                return new[] { int.Parse(m.Groups[1].Value), int.Parse(m.Groups[2].Value), m.Groups[3].Success ? int.Parse(m.Groups[3].Value) : 0 };
            };
            var a = zahlen(tag); var b = zahlen(eigene);
            if (a == null || b == null) return false;
            for (int i = 0; i < 3; i++) { if (a[i] > b[i]) return true; if (a[i] < b[i]) return false; }
            // gleiche Nummer: die endgültige Fassung ist neuer als die Beta
            return Regex.IsMatch(eigene ?? "", "beta", RegexOptions.IgnoreCase) && !Regex.IsMatch(tag ?? "", "beta", RegexOptions.IgnoreCase);
        }
    }
}
