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
        Button btSprache;
        // NEU 1.0.5: neue Oberfläche (Konsole, Kopplung, Speicher, Hinweise, Feedback)
        Button btLogKopie, btLogFenster; GridSplitter gsProtokoll; RowDefinition rdProtokoll; double protokollHoehe = 220;
        Window logFenster; TextBox logFensterTb;
        Button btFeedback; Expander exProtokoll; StackPanel wpSeite; ScrollViewer svSeite; Grid gHaupt; ColumnDefinition cdSeite;
        Border bdKopplung, bdKopplungSymbol, bdTest, bdHinweis, bdBereinigung;
        TextBlock tbReihe, tbModellNr, tbFirmware, tbErkannt, tbVerbindung, tbKopplungSymbol, tbKopplungTitel, tbKopplungText,
                  tbIntern, tbM2, tbUsb, tbSm, tbHinweis, tbFundInfo, tbVersion;
        // für den freiwilligen Feedback-Bericht (wird nur angezeigt, nie automatisch gesendet)
        public string F_MODELL = "", F_REIHE = "", F_QUELLE = "", F_FIRMWARE = "", F_FTPSERVER = "", F_BEGRUESSUNG = "", F_DATEN = ""; public int F_PORT; public bool F_SCAN, F_SCAN_TEST, F_GELOESCHT;
        static string T(string de, string en) { return Sp.T(de, en); }

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
            btSprache = (Button)W.FindName("btSprache");
            btFeedback = (Button)W.FindName("btFeedback"); exProtokoll = (Expander)W.FindName("exProtokoll");
            btLogKopie = (Button)W.FindName("btLogKopie"); btLogFenster = (Button)W.FindName("btLogFenster");
            gsProtokoll = (GridSplitter)W.FindName("gsProtokoll"); rdProtokoll = (RowDefinition)W.FindName("rdProtokoll");
            wpSeite = (StackPanel)W.FindName("wpSeite"); svSeite = (ScrollViewer)W.FindName("svSeite"); gHaupt = (Grid)W.FindName("gHaupt"); cdSeite = (ColumnDefinition)W.FindName("cdSeite");
            bdBereinigung = (Border)W.FindName("bdBereinigung");
            bdKopplung = (Border)W.FindName("bdKopplung"); bdKopplungSymbol = (Border)W.FindName("bdKopplungSymbol"); bdTest = (Border)W.FindName("bdTest"); bdHinweis = (Border)W.FindName("bdHinweis");
            Func<string, TextBlock> tb = n => (TextBlock)W.FindName(n);
            tbReihe = tb("tbReihe"); tbModellNr = tb("tbModellNr"); tbFirmware = tb("tbFirmware"); tbErkannt = tb("tbErkannt"); tbVerbindung = tb("tbVerbindung");
            tbKopplungSymbol = tb("tbKopplungSymbol"); tbKopplungTitel = tb("tbKopplungTitel"); tbKopplungText = tb("tbKopplungText");
            tbIntern = tb("tbIntern"); tbM2 = tb("tbM2"); tbUsb = tb("tbUsb"); tbSm = tb("tbSm"); tbHinweis = tb("tbHinweis"); tbFundInfo = tb("tbFundInfo"); tbVersion = tb("tbVersion");
            tbVersion.Text = Kern.VERSION;
            TexteSetzen();
            try { W.Icon = System.Windows.Media.Imaging.BitmapFrame.Create(typeof(Oberflaeche).Assembly.GetManifestResourceStream("PS5Cleanup.cleanup.ico")); } catch { }
            try { ((Image)W.FindName("imLogo")).Source = System.Windows.Media.Imaging.BitmapFrame.Create(typeof(Oberflaeche).Assembly.GetManifestResourceStream("PS5Cleanup.logo.png")); } catch { }
            tbFuss.Text = "PS5 Cleanup " + Kern.VERSION + "  ·  da_Chris86  ·  " + T("Inoffizielles Community-Projekt, keine Verbindung zu Sony", "Unofficial community project, not affiliated with Sony");
            Zaehlen = () => K.ErmittleGroessen();

            K.Melde = t => Log(t);
            K.GroesseStand = (b, n) => Stand(string.Format(Sp.Kultur, T("Zähle nur die Größe - es wird noch NICHTS gelöscht ... bisher {0} in {1:N0} Dateien", "Only counting the size - NOTHING is deleted yet ... so far {0} in {1:N0} files"), Kern.GroesseText(b), n));

            btSuchen.Click += delegate { Starte(SuchenAblauf, true); };
            btVerbinden.Click += delegate { Starte(Verbinde, true); };
            btKomplett.Click += delegate { Starte(KomplettAblauf, false); };
            btAuswahl.Click += delegate { Starte(AuswahlAblauf, false); };
            btAbbrechen.Click += delegate { K.Abbruch = true; btAbbrechen.IsEnabled = false; Stand(T("Breche ab ...", "Cancelling ...")); };
            btSprache.Click += delegate { SpracheWechseln(); };
            btProtokoll.Click += delegate { ProtokollSpeichern(); };
            btHilfe.Click += delegate { HilfeOeffnen(); };
            btFeedback.Click += delegate { FeedbackOeffnen(); };
            btLogKopie.Click += delegate { LogKopieren(); };
            btLogFenster.Click += delegate { LogFensterOeffnen(); };
            exProtokoll.Expanded += delegate { ProtokollZeile(); };
            exProtokoll.Collapsed += delegate { ProtokollZeile(); };
            ProtokollZeile();
            cbTest.Click += delegate { TestHinweis(); if (VERBUNDEN) { Knoepfe(true); AktualisiereStand(); } };
            KonsoleLeeren();
            // kleinere Bildschirme: Fenster nie größer als der freie Bildschirm, Layout passt sich an
            W.SourceInitialized += delegate { FensterGroesse(); };
            W.SizeChanged += delegate { LayoutAnpassen(); };
            W.Closing += (s, e) =>
            {
                if (LAEUFT)
                {
                    e.Cancel = true;
                    Log(T("Schließen während des Löschens - das Fenster bleibt offen, bis 'Fertig' erscheint.", "Close clicked while deleting - the window stays open until 'Done' appears."));
                    Meldung(T("Es wird gerade auf der PS5 gelöscht. Bitte warten, bis 'Fertig' erscheint.\n\nWas schon gelöscht ist, bleibt gelöscht - auch ein Abbruch bringt nichts zurück. Ein Abbruch mittendrin kann aber ein Spiel nur halb gelöscht zurücklassen.", "Deleting on the PS5 right now. Please wait until 'Done' appears.\n\nWhat is already deleted stays deleted - cancelling does not bring anything back. But stopping halfway can leave a game only half deleted."), "PS5 Cleanup", MessageBoxImage.Warning);
                    return;
                }
                FensterSpeichern();
                if (logFenster != null) { try { logFenster.Close(); } catch { } }
                // Fenster geht zu (z.B. mitten im Zählen): alles anhalten, danach keine Fragen mehr
                FENSTER_ZU = true;
                K.Abbruch = true;
                var h = hintergrund; if (h != null) { h.Stopp = true; var f = h.F; if (f != null) f.Schliessen(); }
            };
            // kein QUIT mehr senden - ein Arbeitsfaden könnte die Verbindung gerade benutzen
            W.Closed += delegate { try { if (K.S != null) K.S.Ftp.Schliessen(); } catch { } Protokoll.Zeile(T("Ende.", "End.")); };
        }

        // Texte des Hauptfensters in der gewählten Sprache (in Fenster.xaml steht Deutsch)
        void TexteSetzen()
        {
            btSprache.Content = Sp.EN ? "Deutsch" : "English";
            btSprache.ToolTip = T("Switch to English (das Programm startet dazu neu)", "Auf Deutsch umschalten (the program restarts)");
            if (!Sp.EN) return;
            ((TextBlock)W.FindName("tbKopf")).Text = "Deletes only what you select - single games, saved data, trophies or everything at once. No reset, no reinstall: system software, disc drive pairing, account and real licenses stay. Use at your own risk - deleted data is gone for good.";
            ((TextBlock)W.FindName("tbIpText")).Text = "PS5 IP:";
            tbIp.ToolTip = "PS5: Settings > Network > View Connection Status. Different FTP port? Then IP:port";
            btSuchen.Content = "Find PS5 on network";
            btVerbinden.Content = "Connect and scan";
            cbModell.Content = "Detect model automatically";
            cbModell.ToolTip = "Briefly runs the small program ps5info.elf on the PS5 (port 9021 or 8084). It briefly creates its own file /data/ps5info.txt - both are removed afterwards.";
            cbTest.Content = "Test mode: only look, delete nothing";
            cbTest.ToolTip = "While ticked, the delete buttons are locked. Untick to delete.";
            tbInfo.Text = "Not connected.";
            tbLeer.Text = "Connect first - then this shows what can be deleted on the PS5.";
            btAbbrechen.Content = "Cancel";
            btAbbrechen.ToolTip = "Stops searching or counting. Nothing is deleted.";
            btProtokoll.Content = "Save log";
            btProtokoll.ToolTip = "Saves the log as a text file - please attach it when reporting problems.";
            btHilfe.Content = "Help";
            btAuswahl.Content = "Delete selection only";
            btKomplett.Content = "Full cleanup";
            btKomplett.ToolTip = "Everything on the internal SSD (the M.2 is asked separately). A final question follows where you type DELETE.";
            btFeedback.Content = "Send Feedback / Compatibility Report";
            btFeedback.ToolTip = "Shows a ready-made report for you to check. Nothing is sent automatically.";
            btProtokoll.ToolTip = "Saves the log as a text file - please attach it when reporting problems. Check it for personal data before sharing.";
            Action<string, string> setze = (n, t) => ((TextBlock)W.FindName(n)).Text = t;
            setze("tbKonsoleKopf", "CONSOLE"); setze("tbLFirmware", "Firmware"); setze("tbLErkannt", "Model detected"); setze("tbLVerbindung", "Connection");
            setze("tbKopplungKopf", "DISC DRIVE PAIRING"); setze("tbKopplungNie", "Read only. The program never touches the pairing.");
            setze("tbLegendeKopf", "STATUS LEGEND"); setze("tbLeg1", "Pairing confirmed"); setze("tbLeg2", "No pairing"); setze("tbLeg3", "Not reliably determinable"); setze("tbLeg4", "Not readable / not queried");
            setze("tbSpeicherKopf", "STORAGE"); setze("tbLIntern", "Internal SSD"); setze("tbLM2", "M.2 SSD"); setze("tbLUsb", "USB storage"); setze("tbLSm", "ShadowMount");
            setze("tbBereinigungKopf", "Cleanup"); setze("tbProtokollKopf", "Log");
            btLogKopie.Content = "Copy all"; btLogKopie.ToolTip = "Copies the whole log of this session to the clipboard - not just the visible part.";
            btLogFenster.Content = "Open in window"; btLogFenster.ToolTip = "Opens the log in a separate, large window (updates live).";
            gsProtokoll.ToolTip = "Drag to make the log bigger or smaller";
            setze("tbLaeuft", "Deleting - please wait until \"Done\" appears. What is already deleted stays deleted - an error or cancelling does not bring anything back. So that no game is left only half deleted, the window cannot be closed until then.");
            setze("tbTestHinweis", "Test mode active: nothing will be deleted - the delete buttons are locked. Untick \"Test mode\" to delete.");
        }

        // Sprache umschalten: speichern und das Programm neu starten (nicht während einer Arbeit)
        void SpracheWechseln()
        {
            if (BESCHAEFTIGT || LAEUFT) { Meldung(T("Bitte warten, bis die laufende Arbeit fertig ist.", "Please wait until the current task has finished."), "PS5 Cleanup"); return; }
            if (!Sp.Speichern(!Sp.EN)) { Meldung(T("Die Sprache konnte nicht gespeichert werden.", "Could not save the language setting."), "PS5 Cleanup", MessageBoxImage.Warning); return; }
            try { Process.Start(System.Reflection.Assembly.GetExecutingAssembly().Location); }
            catch
            {
                Sp.Speichern(Sp.EN);
                Meldung(T("Das Programm konnte nicht neu gestartet werden. Bitte von Hand neu starten.", "The program could not be restarted. Please restart it manually."), "PS5 Cleanup", MessageBoxImage.Warning);
                return;
            }
            W.Close();
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
            Action a = () => { tbLog.AppendText(t + "\r\n"); tbLog.ScrollToEnd(); if (logFensterTb != null) { logFensterTb.AppendText(t + "\r\n"); logFensterTb.ScrollToEnd(); } };
            if (W.Dispatcher.CheckAccess()) a(); else W.Dispatcher.BeginInvoke(a);
        }
        public volatile string StandSoll = "";      // zuletzt gesetzte Statuszeile (auch vor der Anzeige bekannt)
        public void Stand(string t)
        {
            StandSoll = t;
            Action a = () => { STAND_BASIS = t; tbStand.Text = t; };
            if (W.Dispatcher.CheckAccess()) a(); else W.Dispatcher.BeginInvoke(a);
        }
        public bool Testmodus() { return UI(() => cbTest.IsChecked == true); }
        bool FensterZu() { return FENSTER_ZU || !UI(() => W.IsVisible); }

        public void FehlerMelden(Exception ex, string wo)
        {
            Log(T("FEHLER bei ", "ERROR in ") + wo + " : " + ex.Message);
            Protokoll.Zeile("   Stapel: " + (ex.StackTrace ?? "").Replace("\r\n", " | ").Replace("\n", " | "));
            Meldung(T("Es ist ein Fehler aufgetreten:\n", "An error occurred:\n") + ex.Message + T("\n\nDas Protokoll liegt hier:\n", "\n\nThe log is here:\n") + Protokoll.Datei + T("\n\nBitte diese Datei bei einer Fehlermeldung mitschicken.", "\n\nPlease attach this file when reporting the problem."), "PS5 Cleanup", MessageBoxImage.Error);
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
            "SizeToContent=\"Height\" WindowStartupLocation=\"CenterOwner\" Background=\"#1F2127\" FontFamily=\"Segoe UI\" FontSize=\"13\" ResizeMode=\"NoResize\" ";
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
                    "<Button x:Name=\"bAbbr\" Content=\"" + T("Abbrechen", "Cancel") + "\" Padding=\"12,6\" Margin=\"4\" Foreground=\"White\" Background=\"#2A2C33\" BorderBrush=\"#3A3D47\"/>" +
                    "<Button x:Name=\"bOk\" Content=\"OK\" Padding=\"12,6\" Margin=\"4\" Foreground=\"White\" Background=\"#2F6FDB\" BorderBrush=\"#3B8BFF\" IsDefault=\"True\"/>" +
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
        // NEU 1.0.5: Auswahl aus einer kurzen Liste (z.B. mehrere gefundene FTP-Server). -1 = abgebrochen
        public int FrageAuswahl(string text, string titel, List<string> optionen)
        {
            if (FensterZu()) return -1;
            if (TestAntworten != null) { Log("[Test] Auswahl: " + string.Join(" | ", optionen) + " || Statuszeile: " + StandSoll); return (int)TestAntwort(titel, -1); }
            return UI(() =>
            {
                var d = Dialog(DLG_KOPF + "Width=\"600\"><StackPanel Margin=\"16\">" +
                    "<TextBlock x:Name=\"tx\" Foreground=\"#E8E8EA\" TextWrapping=\"Wrap\" Margin=\"0,0,0,10\"/>" +
                    "<StackPanel x:Name=\"sp\"/>" +
                    "<StackPanel Orientation=\"Horizontal\" HorizontalAlignment=\"Right\" Margin=\"0,12,0,0\">" +
                    "<Button x:Name=\"bAbbr\" Content=\"" + T("Abbrechen", "Cancel") + "\" Padding=\"12,6\" Margin=\"4\" Foreground=\"White\" Background=\"#2A2C33\" BorderBrush=\"#3A3D47\" IsCancel=\"True\"/>" +
                    "<Button x:Name=\"bOk\" Content=\"" + T("Übernehmen", "Use this one") + "\" Padding=\"12,6\" Margin=\"4\" Foreground=\"White\" Background=\"#2F6FDB\" BorderBrush=\"#3B8BFF\" IsDefault=\"True\"/>" +
                    "</StackPanel></StackPanel></Window>");
                d.Title = titel; if (W.IsVisible) d.Owner = W;
                ((TextBlock)d.FindName("tx")).Text = text;
                var sp = (StackPanel)d.FindName("sp");
                var knoepfe = new List<RadioButton>();
                for (int i = 0; i < optionen.Count; i++)
                {
                    var r = new RadioButton { Content = optionen[i], IsChecked = i == 0, GroupName = "wahl", Margin = new Thickness(0, 4, 0, 4), Foreground = new SolidColorBrush(Color.FromRgb(0xE8, 0xE8, 0xEA)) };
                    r.MouseDoubleClick += delegate { r.IsChecked = true; d.DialogResult = true; };
                    knoepfe.Add(r); sp.Children.Add(r);
                }
                ((Button)d.FindName("bOk")).Click += delegate { d.DialogResult = true; };
                ((Button)d.FindName("bAbbr")).Click += delegate { d.DialogResult = false; };
                if (knoepfe.Count > 0) knoepfe[0].Focus();
                if (d.ShowDialog() == true) return knoepfe.FindIndex(x => x.IsChecked == true);
                return -1;
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
                    "<Button x:Name=\"bSicher\" Padding=\"12,6\" Margin=\"4\" Foreground=\"White\" Background=\"#2F6FDB\" BorderBrush=\"#3B8BFF\" IsDefault=\"True\" IsCancel=\"True\"/>" +
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

        // Schlussmeldung nach dem Löschen - eigenes Fenster statt der Windows-Meldung,
        // damit die Schritte sauber eingerückt sind (umbrechende Zeilen bleiben
        // unter ihrem Text statt unter der Nummer).
        public void FertigMeldung(string kopf, bool ok, string frei, string smText, string protokoll)
        {
            if (FensterZu()) return;
            if (TestAntworten != null) { Log("[Test] Meldung: Fertig"); return; }
            UI(() =>
            {
                var d = FertigFenster(kopf, ok, frei, smText, protokoll);
                if (W.IsVisible) d.Owner = W;
                d.ShowDialog();
            });
        }
        // baut das Fenster (auch für den Selbsttest, der ein Bild davon speichert)
        public static Window FertigFenster(string kopf, bool ok, string frei, string smText, string protokoll)
        {
                var d = Dialog(DLG_KOPF + "Width=\"580\"><StackPanel Margin=\"20,18,20,16\">" +
                    "<TextBlock x:Name=\"tKopf\" FontSize=\"16\" FontWeight=\"SemiBold\" TextWrapping=\"Wrap\"/>" +
                    "<TextBlock x:Name=\"tFrei\" Foreground=\"#E8E8EA\" TextWrapping=\"Wrap\" Margin=\"0,6,0,0\"/>" +
                    "<TextBlock x:Name=\"tSm\" Foreground=\"#E8E8EA\" TextWrapping=\"Wrap\" Margin=\"0,12,0,0\"/>" +
                    "<Border Background=\"#26282F\" BorderBrush=\"#3A3D47\" BorderThickness=\"1\" CornerRadius=\"4\" Padding=\"12,10\" Margin=\"0,16,0,0\">" +
                    "<StackPanel><TextBlock x:Name=\"tSchritt\" Foreground=\"#E8E8EA\" FontWeight=\"SemiBold\" TextWrapping=\"Wrap\" Margin=\"0,0,0,6\"/>" +
                    "<Grid x:Name=\"gSchritte\"><Grid.ColumnDefinitions><ColumnDefinition Width=\"Auto\"/><ColumnDefinition Width=\"*\"/></Grid.ColumnDefinitions></Grid>" +
                    "<TextBlock x:Name=\"tWichtig\" Foreground=\"#FF9F66\" FontWeight=\"SemiBold\" TextWrapping=\"Wrap\" Margin=\"0,8,0,0\"/></StackPanel></Border>" +
                    "<TextBlock x:Name=\"tProt\" Foreground=\"#9A9CA5\" FontSize=\"11.5\" TextWrapping=\"Wrap\" Margin=\"0,14,0,0\"/>" +
                    "<Button x:Name=\"bOk\" Content=\"OK\" HorizontalAlignment=\"Right\" MinWidth=\"90\" Padding=\"12,6\" Margin=\"0,14,0,0\" Foreground=\"White\" Background=\"#2F6FDB\" BorderBrush=\"#3B8BFF\" IsDefault=\"True\" IsCancel=\"True\"/>" +
                    "</StackPanel></Window>");
                d.Title = T("Fertig", "Done");
                var tk = (TextBlock)d.FindName("tKopf");
                tk.Text = kopf;
                tk.Foreground = new SolidColorBrush(ok ? Color.FromRgb(0x5C, 0xD0, 0x8A) : Color.FromRgb(0xFF, 0x9F, 0x66));
                ((TextBlock)d.FindName("tFrei")).Text = frei;
                var ts = (TextBlock)d.FindName("tSm");
                if (string.IsNullOrEmpty(smText)) ts.Visibility = Visibility.Collapsed; else ts.Text = smText;
                // Kern.SCHLUSS_TEXT: Überschrift, nummerierte Schritte, zuletzt die Warnung
                var g = (Grid)d.FindName("gSchritte");
                var tw = (TextBlock)d.FindName("tWichtig");
                var tsch = (TextBlock)d.FindName("tSchritt");
                var schluss = Kern.SCHLUSS_TEXT;
                for (int i = 0; i < schluss.Length; i++)
                {
                    string z = schluss[i];
                    var m = Regex.Match(z, "^(\\d+\\.)\\s*(.*)$");
                    if (m.Success)
                    {
                        g.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
                        int r = g.RowDefinitions.Count - 1;
                        var nr = new TextBlock { Text = m.Groups[1].Value, Foreground = new SolidColorBrush(Color.FromRgb(0xE8, 0xE8, 0xEA)), Margin = new Thickness(0, 2, 8, 2) };
                        var tx = new TextBlock { Text = m.Groups[2].Value, Foreground = new SolidColorBrush(Color.FromRgb(0xE8, 0xE8, 0xEA)), TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 2, 0, 2) };
                        Grid.SetRow(nr, r); Grid.SetColumn(nr, 0); Grid.SetRow(tx, r); Grid.SetColumn(tx, 1);
                        g.Children.Add(nr); g.Children.Add(tx);
                    }
                    else if (i == schluss.Length - 1) tw.Text = z;
                    else tsch.Text = z;
                }
                ((TextBlock)d.FindName("tProt")).Text = T("Protokoll: ", "Log: ") + protokoll;
                var b = (Button)d.FindName("bOk");
                b.Click += delegate { d.Close(); };
                d.Loaded += delegate { b.Focus(); };
                return d;
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
                catch (AbbruchAusnahme) { if (!FENSTER_ZU) { Log(T("Abgebrochen - nichts gelöscht.", "Cancelled - nothing deleted.")); Stand(T("Abgebrochen.", "Cancelled.")); K.SmZuruecksetzen(); if (VERBUNDEN) HintergrundStarten(); } }
                catch (Exception ex) { if (!FENSTER_ZU) { FehlerMelden(ex, T("dem Ablauf", "the process")); UI(() => { pbFortschritt.IsIndeterminate = false; }); } }
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
        // PS5 im Heimnetz suchen. NEU 1.0.5: die üblichen FTP-Ports 2121 und 2120 auf allen
        // eigenen /24-Netzen; gezählt wird nur, was sich wirklich als FTP-Server meldet.
        // Mehrere Treffer -> der Nutzer wählt aus einer kurzen Liste.
        // ---------------------------------------------------------------
        public int[] FTP_PORTE = { Kern.STANDARD_PORT, Kern.ZWEIT_PORT };
        public List<string> TestSuchHosts;      // nur Oberflächentest: diese Adressen statt der Heimnetze
        public class FtpTreffer { public string Ip; public int Port; public string Server; }
        void SuchenAblauf()
        {
            var gruppen = new List<KeyValuePair<string, List<string>>>();
            if (TestSuchHosts != null) gruppen.Add(new KeyValuePair<string, List<string>>("Test", TestSuchHosts));
            else
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
                foreach (var n in netze)
                {
                    var h = new List<string>();
                    for (int i = 1; i <= 254; i++) h.Add(n + "." + i);
                    gruppen.Add(new KeyValuePair<string, List<string>>(n + ".0", h));
                }
            }
            // 1. Wo ist einer der Ports offen?
            var offen = new List<KeyValuePair<string, int>>();
            foreach (var gr in gruppen)
            {
                if (K.Abbruch) throw new AbbruchAusnahme();
                Stand(T("Suche im Netz ", "Searching network ") + gr.Key + " ...");
                var clients = new List<Tuple<string, int, TcpClient>>();
                foreach (var h in gr.Value)
                    foreach (int p in FTP_PORTE)
                    {
                        var c = new TcpClient();
                        try { c.BeginConnect(h, p, null, null); } catch { }
                        clients.Add(Tuple.Create(h, p, c));
                    }
                Thread.Sleep(1500);
                foreach (var e in clients)
                {
                    try { if (e.Item3.Connected) offen.Add(new KeyValuePair<string, int>(e.Item1, e.Item2)); } catch { }
                    try { e.Item3.Close(); } catch { }
                }
            }
            // 2. Meldet sich dort wirklich ein FTP-Server? (anmelden, Namen erfragen, wieder abmelden - nichts wird verändert)
            var treffer = new List<FtpTreffer>();
            foreach (var o in offen)
            {
                if (K.Abbruch) throw new AbbruchAusnahme();
                Stand(T("Prüfe ", "Checking ") + o.Key + T(", Port ", ", port ") + o.Value + " ...");
                Ftp f = null;
                try { f = Ftp.Verbinden(o.Key, o.Value, 5000); } catch { f = null; }
                if (f == null)
                {
                    Log(T("Suche: ", "Search: ") + o.Key + T(", Port ", ", port ") + o.Value + T(" ist offen, meldet sich aber nicht als nutzbarer FTP-Server (", " is open but does not answer as a usable FTP server (") + Ftp.VerbindenFehler + ").");
                    continue;
                }
                string s = "";
                try { s = f.ServerErkennen(); } catch { }
                try { f.Trennen(); } catch { }
                treffer.Add(new FtpTreffer { Ip = o.Key, Port = o.Value, Server = s });
                Log(T("Suche: gefunden ", "Search: found ") + o.Key + T(", Port ", ", port ") + o.Value + T(" - FTP-Server: ", " - FTP server: ") + (s != "" ? s : Feedback.Unbekannt));
            }
            if (treffer.Count == 0)
            {
                // gut sichtbar: in der Statuszeile und direkt unter dem IP-Feld
                string keiner = T("Keine PS5 mit laufendem FTP-Server gefunden. Bitte prüfen: PS5 eingeschaltet und im selben Netzwerk wie dieser PC, Jailbreak geladen, FTP-Server auf der PS5 gestartet. Danach nochmal suchen.",
                    "No PS5 with a running FTP server found. Please check: PS5 switched on and on the same network as this PC, jailbreak loaded, FTP server started on the PS5. Then search again.");
                Stand(keiner);
                UI(() => tbInfo.Text = keiner);
                Log(T("Suche: kein FTP-Server gefunden (geprüfte Ports: ", "Search: no FTP server found (ports checked: ") + string.Join(", ", FTP_PORTE) + ").");
                return;
            }
            FtpTreffer w = treffer[0];
            if (treffer.Count > 1)
            {
                Stand(treffer.Count + T(" FTP-Server gefunden – bitte einen auswählen.", " FTP servers found – please select one."));
                var texte = treffer.Select(x => x.Ip + "   ·   Port " + x.Port + "   ·   " + (x.Server != "" ? x.Server : T("FTP-Server: Unbekannt", "FTP server: Unknown"))).ToList();
                int i = FrageAuswahl(T("Es wurden mehrere FTP-Server gefunden. Womit soll sich PS5 Cleanup verbinden?", "Several FTP servers were found. Which one should PS5 Cleanup connect to?"), T("FTP-Server auswählen", "Choose FTP server"), texte);
                if (i < 0) { Stand(T("Suche beendet - nichts ausgewählt.", "Search finished - nothing chosen.")); return; }
                w = treffer[i];
            }
            string eintrag = w.Port == FTP_PORTE[0] ? w.Ip : w.Ip + ":" + w.Port;
            UI(() => tbIp.Text = eintrag);
            Log(T("Eingetragen: ", "Entered: ") + eintrag);
            Stand(T("Gefunden: ", "Found: ") + w.Ip + T(" (Port ", " (port ") + w.Port + (w.Server != "" ? ", " + w.Server : "") + T("). Jetzt auf „Verbinden und nachschauen“ klicken.", "). Now click \"Connect and scan\"."));
        }

        // ---------------------------------------------------------------
        // Verbinden und nachschauen
        // ---------------------------------------------------------------
        public void Verbinde()
        {
            string eingabe = UI(() => Regex.Replace(tbIp.Text, "\\s", ""));
            string ip = eingabe; int port = FTP_PORTE[0];
            var m = Regex.Match(eingabe, "^(.+):(\\d+)$");
            bool portAngegeben = m.Success;
            if (m.Success) { ip = m.Groups[1].Value; int.TryParse(m.Groups[2].Value, out port); }
            if (!Regex.IsMatch(ip, "^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$") || port < 1 || port > 65535) { Meldung(T("Bitte eine gültige IP-Adresse eingeben (Beispiel: 192.168.1.50).", "Please enter a valid IP address (example: 192.168.1.50)."), "PS5 Cleanup", MessageBoxImage.Warning); return; }
            HintergrundAnhalten();
            // Alte Liste sofort verwerfen - sie gehört evtl. zu einer anderen PS5
            VERBUNDEN = false; K.FUNDE_IP = ""; K.DDD_STATUS = null;
            F_MODELL = ""; F_REIHE = ""; F_QUELLE = ""; F_FIRMWARE = ""; F_PORT = 0; F_SCAN = false; F_FTPSERVER = ""; F_BEGRUESSUNG = ""; F_DATEN = "";
            K.Funde = new List<Eintrag>();
            UI(() =>
            {
                tbInfo.Text = T("Verbinde ...", "Connecting ...");
                KonsoleLeeren();
                spBereiche.Children.Clear(); Kaestchen.Clear(); KopfKaestchen.Clear();
                tbLeer.Visibility = Visibility.Visible; spBereiche.Children.Add(tbLeer);
                tbLog.Clear();
            });
            Log(T("Verbinde mit PS5 (", "Connecting to PS5 (") + ip + ", Port " + port + ") ...");
            // NEU 1.0.5: zwei getrennte Fälle mit verständlicher Meldung und dem Grund im Protokoll
            bool ok = false; string grund = "";
            try { ok = K.Verbinden(ip, port); } catch (AbbruchAusnahme) { throw; } catch (Exception ex) { ok = false; grund = ex.Message; }
            if (!ok && grund == "") grund = Ftp.VerbindenFehler;
            // NEU 1.0.5: nur die IP eingegeben und über den üblichen Port klappt es nicht ->
            // die anderen üblichen Ports probieren (z.B. zftpd auf 2120). Der Nutzer muss keine Ports kennen.
            if (!ok && !portAngegeben)
            {
                grund = "Port " + port + ": " + grund;
                foreach (int p2 in FTP_PORTE.Skip(1))
                {
                    if (K.Abbruch) throw new AbbruchAusnahme();
                    Log(T("Über Port ", "No connection via port ") + port + T(" keine Verbindung - versuche Port ", " - trying port ") + p2 + " ...");
                    string g2 = "";
                    try { ok = K.Verbinden(ip, p2); } catch (AbbruchAusnahme) { throw; } catch (Exception ex) { ok = false; g2 = ex.Message; }
                    if (ok)
                    {
                        port = p2;
                        string neuIp = ip + ":" + p2;
                        UI(() => tbIp.Text = neuIp);
                        break;
                    }
                    grund += " / Port " + p2 + ": " + (g2 != "" ? g2 : Ftp.VerbindenFehler);
                }
            }
            if (!ok)
            {
                // Für den Nutzer einfach; der technische Grund steht im Protokoll
                Log(T("FEHLER: PS5 nicht erreichbar.", "ERROR: PS5 not reachable.") + (grund != "" ? T("  Grund: ", "  Reason: ") + grund : ""));
                UI(() => tbInfo.Text = T("PS5 nicht erreichbar. Bitte prüfen: PS5 eingeschaltet, Jailbreak geladen und FTP-Server gestartet. Tipp: „PS5 im Netz suchen“ findet die PS5 und den passenden Port von selbst. Den genauen Grund zeigt das Protokoll.",
                    "PS5 not reachable. Please check: PS5 switched on, jailbreak loaded and FTP server started. Tip: \"Find PS5 on network\" finds the PS5 and the right port by itself. The log shows the exact reason."));
                return;
            }
            List<Datei> user = null;
            try { user = K.S.Liste("/user"); } catch (AbbruchAusnahme) { throw; } catch (Exception ex) { K.S.Ftp.LetzterFehler = ex.Message; }
            if (user == null)
            {
                string g = K.S.Ftp.LetzterFehler;
                Log(T("FEHLER: Die PS5 antwortet, aber ihre Ordner lassen sich nicht lesen.", "ERROR: The PS5 answers, but its folders cannot be read.") + (g != "" ? T("  Grund: ", "  Reason: ") + g : ""));
                UI(() => tbInfo.Text = T("Die PS5 antwortet, aber ihre Ordner lassen sich nicht lesen. Bitte den FTP-Server auf der PS5 neu starten und es nochmal versuchen. ", "The PS5 answers, but its folders cannot be read. Please restart the FTP server on the PS5 and try again. ") + FTP_TIPP + T(" Den genauen Grund zeigt das Protokoll.", " The log shows the exact reason."));
                try { K.Trennen(); } catch { }
                return;
            }
            K.S.Ftp.Wartet = ms => Warteanzeige(ms);
            Log(T("Verbunden.", "Connected."));
            // NEU 1.0.5: welcher FTP-Server und welche Art der Datenverbindung (nur Auskunft, fürs Protokoll und den Feedback-Bericht)
            try { F_FTPSERVER = K.S.Ftp.ServerErkennen(); } catch { F_FTPSERVER = ""; }
            F_BEGRUESSUNG = Ftp.BegruessungKurz(K.S.Ftp.Begruessung);
            F_DATEN = K.S.Ftp.EpsvBenutzt ? "EPSV" : K.S.Ftp.PasvBenutzt ? "PASV" : "";
            Log(T("FTP-Server: ", "FTP server: ") + (F_FTPSERVER != "" ? F_FTPSERVER : Feedback.Unbekannt + (F_BEGRUESSUNG != "" ? T(" (Begrüßung: „", " (greeting: \"") + F_BEGRUESSUNG + T("“)", "\")") : "")) + T(", Port ", ", port ") + port);
            Log(T("Datenverbindung: ", "Data connection: ") + K.S.Ftp.DatenArt());
            string firmware = K.FirmwareLesen();
            string modell = "";
            bool abfrage = UI(() => cbModell.IsChecked == true);
            if (abfrage) { Log(T("Frage das Modell ab ...", "Asking for the model ...")); modell = K.ModellLesen(ip, Programm.Ressource("ps5info.elf")); }
            bool automatisch = modell != "";
            if (modell == "")
            {
                modell = FrageText(T("Modell konnte nicht automatisch erkannt werden.\nDie Modellnummer steht auf dem Aufkleber der Konsole oder unter Einstellungen > System > Systemsoftware > Konsoleninformationen.\n\nModellnummer (z.B. CFI-7016), leer = überspringen:", "The model could not be detected automatically.\nThe model number is on the console's label or under Settings > System > System Software > Console Information.\n\nModel number (e.g. CFI-7016), empty = skip:"), T("Modell", "Model")) ?? "";
                modell = Regex.Replace(modell, "\\s", "");
            }
            K.MODELL_TYP = Kern.ModellTyp(modell);
            K.MODELL_DIGITAL = Kern.ModellDigital(modell);
            string reihe = Kern.ModellReihe(K.MODELL_TYP, K.MODELL_DIGITAL);
            F_MODELL = modell; F_REIHE = K.MODELL_TYP != "" ? reihe : ""; F_FIRMWARE = firmware; F_PORT = port;
            F_QUELLE = automatisch ? T("automatisch (ps5info)", "automatic (ps5info)") : modell != "" ? T("von Hand eingegeben", "entered by hand") : abfrage ? T("nicht erkannt", "not detected") : T("Abfrage ausgeschaltet", "query switched off");
            { string m0 = modell, r0 = F_REIHE, f0 = firmware, q0 = F_QUELLE; int p0 = port; UI(() => ZeigeKonsole(m0, r0, f0, q0, p0)); }
            if (modell == "") modell = T("unbekannt", "unknown");
            string info = T("Verbunden mit ", "Connected to ") + ip + T(" (Port ", " (port ") + port + ").";
            UI(() => tbInfo.Text = info);
            Log(T("Modell: ", "Model: ") + modell + T("  Reihe: ", "  Series: ") + reihe + "  Firmware: " + firmware);
            Log(T("Laufwerk: ", "Disc drive: ") + K.LaufwerkText());
            Log(T("Schaue nach, was drauf ist ...", "Scanning what is on it ..."));
            Stand(T("Schaue nach, was auf der PS5 liegt ...", "Scanning what is on the PS5 ..."));
            UI(() => pbFortschritt.IsIndeterminate = true);
            try { K.SucheFunde(); }
            catch (AbbruchAusnahme) { throw; }
            catch (Exception ex) { throw new Exception(ex.Message + "\n\n" + FTP_TIPP, ex); }
            UI(() => pbFortschritt.IsIndeterminate = false);
            if (K.KOMPRIMIERUNG.Count > 0)
            {
                Log(T("ACHTUNG: Der Game Compressor arbeitet gerade (oder wurde abgebrochen):", "WARNING: The Game Compressor is working right now (or was interrupted):"));
                foreach (var z in K.KOMPRIMIERUNG) Log("   " + z);
                Log(T("Das betroffene Spiel ist GESPERRT. Lass den Game Compressor erst fertig werden.", "The affected game is LOCKED. Let the Game Compressor finish first."));
            }
            VERBUNDEN = K.FUNDE_IP != "";
            F_SCAN = VERBUNDEN; F_SCAN_TEST = Testmodus();
            UI(() => { tbLeer.Visibility = Visibility.Collapsed; ZeigeBereiche(); ZeigeSpeicher(); });
            if (K.Funde.Count == 0) Log(T("Nichts zu tun - die PS5 ist bereits sauber.", "Nothing to do - the PS5 is already clean."));
            else Log(K.Funde.Count + T(" Einträge gefunden. Nichts wurde verändert.", " entries found. Nothing was changed."));
            { string h = K.SpeicherHinweis(); if (h != "") Log(h.Trim()); }
            if (Testmodus()) Log(T("(Testmodus - zum Löschen den Haken \"Testmodus\" entfernen.)", "(Test mode - untick \"Test mode\" to delete.)"));
            HintergrundStarten();
        }

        // NEU 1.0.5: nur wenn sich die Anzeige ändert (sonst bis zu 50 Aufträge pro Sekunde an die
        // Oberfläche) und mit niedriger Priorität - Mausklicks und Tasten gehen immer vor.
        volatile int warteSek = 0;
        void Warteanzeige(int ms)
        {
            int s = ms > 3000 ? ms / 1000 : 0;
            if (s == warteSek) return;
            warteSek = s;
            Action a = s > 0 ? (Action)(() => tbStand.Text = STAND_BASIS + T("   [PS5 arbeitet seit ", "   [PS5 has been working for ") + s + T(" s an einer großen Datei ...]", " s on a large file ...]"))
                             : () => { if (tbStand.Text != STAND_BASIS) tbStand.Text = STAND_BASIS; };
            W.Dispatcher.BeginInvoke(a, System.Windows.Threading.DispatcherPriority.Background);
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
                    W.Dispatcher.BeginInvoke(new Action(() => { if (lauf.Stopp || K.Funde != liste) return; ZeigeGroesse(e); if (!BESCHAEFTIGT) AktualisiereStand(nn < gesamt ? T("Größen werden ermittelt (", "Counting sizes (") + nn + T(" von ", " of ") + gesamt + ") ..." : null); }));
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

        static string GroesseKurz(Groesse g) { return (g.Gekappt ? T("mind. ", "min. ") : "") + Kern.GroesseText(g.Bytes); }

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
                inhalt.Children.Add(new TextBlock { Text = Kern.KatInfo[k], Foreground = (Brush)W.FindResource("Grau"), TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 0, 0, 6) });
                if (k == 1 && eintraege.Any(f => f.Quelle != ""))
                    inhalt.Children.Add(new TextBlock { Foreground = (Brush)W.FindResource("Gelb"), Margin = new Thickness(0, 0, 0, 6),
                        Text = T("ShadowMount-Spiele: Bei \"Nur Auswahl löschen\" wirst du gefragt, ob nur die Kachel oder auch die Spieldatei weg soll.", "ShadowMount games: with \"Delete selection only\" you will be asked whether only the tile or the game file too should go.") });
                var kinder = new List<CheckBox>();
                foreach (var f in eintraege)
                {
                    var cb = new CheckBox();
                    cb.Tag = f;
                    cb.Content = KaestchenInhalt(f);
                    // NEU 1.0.5: der vollständige Name als Infofeld beim Darüberfahren (lange Titel ohne seitliches Scrollen lesbar)
                    string zusatz = null;
                    if (f.Gesperrt) { cb.IsEnabled = false; zusatz = T("Gesperrt - der Game Compressor arbeitet gerade daran.", "Locked - the Game Compressor is working on it."); }
                    if (f.SmTipp && !f.Gesperrt) zusatz = T("ShadowMount: Wird das Spiel gelöscht, verschwindet auch seine Kachel im Menü.", "ShadowMount: if the game is deleted, its tile disappears from the menu too.");
                    cb.ToolTip = InfoFeld(f, zusatz);
                    ToolTipService.SetShowOnDisabled(cb, true);
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
                    // Trophäen-Listen noch installierter Spiele (gelb) nur einzeln - beim Abwählen gehen sie mit
                    foreach (var c in kinder) { var ff = (Eintrag)c.Tag; if (!ff.Gesperrt && !(ff.Paket && a.IsChecked == true)) { c.IsChecked = a.IsChecked; ff.Wahl = a.IsChecked == true; } }
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
            if (k == 7 && !K.m2Da) { alle.Content = "[7] " + Kern.KatName[7] + T("   (keine M.2 gefunden)", "   (no M.2 found)"); return; }
            string t = "[" + k + "] " + Kern.KatName[k] + "   (" + eintraege.Count + T(" Einträge", " entries");
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
                string t = n == 0 ? T("Nichts ausgewählt.", "Nothing selected.") : n + T(" Einträge ausgewählt.", " entries selected.");
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
                    if (alleDa) t = n + T(" Einträge ausgewählt (", " entries selected (") + GroesseKurz(sum) + ").";
                }
                if (cbTest.IsChecked == true) t = T("TESTMODUS - es wird nichts gelöscht.  ", "TEST MODE - nothing will be deleted.  ") + t;
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
                if (basis.ContainsKey(f.Gruppe)) { cb.IsEnabled = false; cb.ToolTip = InfoFeld(f, T("Geht mit dem Spiel weg.", "Goes with the game.")); }
                else { cb.IsEnabled = true; cb.ToolTip = InfoFeld(f, null); }
            }
        }
        // NEU 1.0.5: Infofeld eines Eintrags = vollständiger Name (+ ggf. ein Hinweis)
        static ToolTip InfoFeld(Eintrag f, string zusatz)
        {
            string text = f.Text + (string.IsNullOrEmpty(zusatz) ? "" : "\n\n" + zusatz);
            return new ToolTip { Content = new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap, MaxWidth = 700 } };
        }

        // ---------------------------------------------------------------
        // Löschen
        // ---------------------------------------------------------------
        public string Zusammenfassung(bool komplett)
        {
            var g = K.Gewaehlt();
            string t = T("Wird ENDGÜLTIG gelöscht: ", "Will be deleted PERMANENTLY: ") + g.Count + T(" Einträge\n", " entries\n");
            foreach (var k in Kern.ALLE_KAT)
            {
                int c = g.Count(f => f.Kat == k);
                if (c > 0) t += "   - " + Kern.KatName[k] + ": " + c + "\n";
            }
            int m2 = g.Count(f => f.Kat == 7);
            int smDatei = g.Count(f => f.Kat == 1 && f.Quelle != "" && f.Pfade.Contains(f.Quelle));
            if (smDatei > 0) t += T("   davon ShadowMount-Spiele MIT Spieldatei: ", "   of these, ShadowMount games WITH game file: ") + smDatei + "\n";
            if (komplett && m2 == 0) t += T("   (M.2-SSD bleibt unverändert)\n", "   (M.2 SSD stays unchanged)\n");
            if (!komplett && m2 > 0) t = T("!!! ACHTUNG: ", "!!! WARNING: ") + m2 + T(" EINTRÄGE DER M.2-SSD SIND GEWÄHLT - was darin liegt (Spiele, Dateien), ist danach weg !!!\n\n", " M.2 SSD ENTRIES ARE SELECTED - whatever is in them (games, files) will be gone !!!\n\n") + t;
            int gesp = K.Funde.Count(f => f.Gesperrt);
            if (gesp > 0) t += T("   Bleibt (Game Compressor arbeitet daran): ", "   Kept (Game Compressor is working on it): ") + gesp + "\n";
            if (!komplett) { string h = K.SpeicherHinweis(); if (h != "") t += h.TrimStart('\n') + "\n"; }
            t += T("\nDas sind ", "\nThat is ") + Kern.GroesseSatz(GROSS) + ".\n";
            t += "\n" + K.KopplungsText() + T("\nBenutzung auf eigene Gefahr - Gelöschtes ist endgültig weg.", "\nUse at your own risk - deleted data is gone for good.");
            return t;
        }

        // Zählen vor der Abschlussfrage; false = abgebrochen / Fenster zu
        public bool GroesseErmitteln()
        {
            bool fehlt = K.Gewaehlt().Any(f => f.Pfade.Any(p => !K.PfadGezaehlt(p)));
            if (fehlt)
            {
                Log(T("Zähle, wie viel gewählt ist (noch wird NICHTS gelöscht - danach kommt die Abschlussfrage) ...", "Counting the selection (NOTHING is deleted yet - the final question comes next) ..."));
                Stand(T("Zähle nur die Größe - es wird noch NICHTS gelöscht ...", "Only counting the size - NOTHING is deleted yet ..."));
                UI(() => { btAbbrechen.Visibility = Visibility.Visible; btAbbrechen.IsEnabled = true; pbFortschritt.IsIndeterminate = true; });
            }
            try { GROSS = Zaehlen(); }
            finally { UI(() => { btAbbrechen.Visibility = Visibility.Collapsed; pbFortschritt.IsIndeterminate = false; }); }
            if (FensterZu()) return false;
            if (K.Abbruch) throw new AbbruchAusnahme();
            Log(T("Ausgewählt: ", "Selected: ") + Kern.GroesseSatz(GROSS) + ".");
            return true;
        }

        public void KomplettAblauf()
        {
            HintergrundAnhalten();
            string t = T("VOLLSTÄNDIGE BEREINIGUNG: Auf der internen SSD wird ALLES gelöscht:\n", "FULL CLEANUP: EVERYTHING on the internal SSD will be deleted:\n");
            foreach (var k in new[] { 1, 2, 3, 4, 5, 6 }) t += "   - " + Kern.KatName[k] + ": " + K.AnzahlKomplett(k) + "\n";
            t += T("\nDas heißt:\n - Spielstände ohne Cloud-Sicherung (PS Plus) sind endgültig weg.\n - Nie mit dem PSN abgeglichene Trophäen sind weg.\n - Jailbreak-Tools samt Einstellungen sind weg - danach den Jailbreak NEU EINRICHTEN.\n - Ein im Browser offline gespeicherter Jailbreak-Zugang kann auch weg sein.\n\nBleibt: Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, echte Lizenzen, Sony-Systemanwendungen.",
                   "\nThis means:\n - Saved data without a cloud backup (PS Plus) is gone for good.\n - Trophies never synced with PSN are gone.\n - Jailbreak tools and their settings are gone - set up your jailbreak AGAIN afterwards.\n - A jailbreak page saved offline in the browser may be gone too.\n\nKept: system software, disc drive pairing, user account, real licenses, Sony system apps.");
            t += K.SpeicherHinweis();
            if (!FrageWahl(t, T("Vollständige Bereinigung", "Full cleanup"), T("Abbrechen", "Cancel"), T("Weiter", "Continue"))) { HintergrundStarten(); return; }
            bool m2Leeren = false;
            if (K.m2Da && K.Anzahl(7) > 0)
                m2Leeren = FrageWahl(T("Auf der M.2-SSD liegen ", "The M.2 SSD holds ") + K.Anzahl(7) + T(" Einträge (Spiele, Ordner, Dateien).\n\nM.2 BEHALTEN: Die Spiele bleiben. Nur ihre Kacheln gehen weg - sie kommen wieder, sobald ShadowMount neu eingerichtet ist.\n\nM.2 AUCH LEEREN: Alle Spiele und Dateien auf der M.2 werden endgültig gelöscht.",
                    " entries (games, folders, files).\n\nKEEP M.2: the games stay. Only their tiles go - they come back once ShadowMount is set up again.\n\nALSO WIPE M.2: all games and files on the M.2 are deleted permanently."),
                    T("M.2-SSD", "M.2 SSD"), T("M.2 behalten (empfohlen)", "Keep M.2 (recommended)"), T("M.2 auch leeren", "Also wipe M.2"));
            K.WaehleKomplett(m2Leeren);
            ZeigeHaken(); AktualisiereStand();
            if (!m2Leeren && K.HatPakete()) Log(T("(Trophäen-Listen bleiben, weil die Spiele auf der M.2 bleiben.)", "(Trophy lists stay because the games on the M.2 stay.)"));
            bool weiter;
            try { weiter = GroesseErmitteln(); }
            catch (AbbruchAusnahme) { foreach (var f in K.Funde) f.Wahl = false; K.SmZuruecksetzen(); ZeigeHaken(); throw; }
            if (!weiter) { Protokoll.Zeile(T("Fenster geschlossen - abgebrochen, nichts gelöscht.", "Window closed - cancelled, nothing deleted.")); return; }
            string warn = m2Leeren ? T("!!! ACHTUNG: DIE M.2-SSD WIRD MIT GELEERT - ", "!!! WARNING: THE M.2 SSD WILL BE WIPED TOO - ") + K.Anzahl(7) + T(" Einträge, alle Spiele darauf sind danach weg !!!\n\n", " entries, all games on it will be gone !!!\n\n") : T("Die M.2-SSD bleibt unangetastet.\n\n", "The M.2 SSD stays untouched.\n\n");
            string a = FrageText(warn + Zusammenfassung(true) + T("\n\nZum Bestätigen LÖSCHEN eintippen (alles andere = abbrechen):", "\n\nType DELETE to confirm (anything else = cancel):"), T("Endgültig löschen?", "Delete permanently?"));
            bool bestaetigt = Sp.EN ? a == "DELETE" : (a == "LOESCHEN" || a == "LÖSCHEN");
            if (!bestaetigt)
            {
                Log(T("Abgebrochen - nichts verändert.", "Cancelled - nothing changed."));
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
                bool mit = FrageWahl(T("Du hast ", "You selected ") + sm.Count + T(" ShadowMount-Spiel(e) gewählt:\n", " ShadowMount game(s):\n") + l +
                    T("\n\nNUR KACHELN LÖSCHEN: Die Spiele bleiben auf der Platte und erscheinen wieder, sobald ShadowMount neu geladen ist.\n\nSPIELE + KACHELN LÖSCHEN: Die Spieldateien werden endgültig gelöscht.",
                      "\n\nDELETE TILES ONLY: the games stay on the SSD/M.2 and come back once ShadowMount is loaded again.\n\nDELETE GAMES + TILES: the game files are deleted permanently."),
                    T("ShadowMount-Spiele", "ShadowMount games"), T("Nur Kacheln löschen (empfohlen)", "Delete tiles only (recommended)"), T("Spiele + Kacheln löschen", "Delete games + tiles"));
                K.SetzeSmSpieldateien(mit);
            }
            else K.SmZuruecksetzen();
            var m2 = K.M2Gewaehlt();
            if (m2.Count > 0)
            {
                string l = string.Join("\n", m2.Take(15).Select(f => "   " + f.Text));
                if (m2.Count > 15) l += T("\n   ... und ", "\n   ... and ") + (m2.Count - 15) + T(" weitere", " more");
                if (!FrageWahl(T("ACHTUNG - du hast ", "WARNING - you selected ") + m2.Count + T(" Einträge der M.2-SSD gewählt:\n", " M.2 SSD entries:\n") + l +
                    T("\n\nAlles, was darin liegt (Spiele, Ordner, Dateien), wird endgültig gelöscht.\n\nGelöscht wird erst nach der Abschlussfrage.", "\n\nEverything in them (games, folders, files) will be deleted permanently.\n\nNothing is deleted before the final question."),
                    T("M.2-SSD", "M.2 SSD"), T("Abbrechen", "Cancel"), T("Weiter (M.2-Einträge löschen)", "Continue (delete M.2 entries)")))
                { Log(T("Abgebrochen - nichts verändert.", "Cancelled - nothing changed.")); K.SmZuruecksetzen(); HintergrundStarten(); return; }
            }
            if (!GroesseErmitteln()) { Protokoll.Zeile(T("Fenster geschlossen - abgebrochen, nichts gelöscht.", "Window closed - cancelled, nothing deleted.")); return; }
            if (!FrageWahl(Zusammenfassung(false), T("Endgültig löschen?", "Delete permanently?"), T("Abbrechen", "Cancel"), T("Jetzt endgültig löschen", "Delete permanently now")))
            { Log(T("Abgebrochen - nichts verändert.", "Cancelled - nothing changed.")); K.SmZuruecksetzen(); HintergrundStarten(); return; }
            LoescheJetzt();
        }

        public Action LoescheJetztErsatz;     // nur für den Selbsttest
        void LoescheJetzt()
        {
            if (LoescheJetztErsatz != null) { LoescheJetztErsatz(); return; }
            UI(() => { pbFortschritt.IsIndeterminate = false; pbFortschritt.Value = 0; btAbbrechen.Visibility = Visibility.Collapsed; exProtokoll.IsExpanded = true; });
            Stand(T("Lösche ... (große Spiele können etwas dauern)", "Deleting ... (large games can take a while)"));
            // NEU 1.0.5: "Eintrag i von n" (und GB, wenn die Größen bekannt sind) - nur Anzeige
            {
                var gew = K.Gewaehlt();
                bool groessenDa = gew.All(f => f.HatGroesse);
                long gesamt = gew.Where(f => f.HatGroesse).Sum(f => f.Bytes), erledigt = 0; Eintrag vorher = null;
                K.Fortschritt = (i, n, f) =>
                {
                    if (vorher != null && vorher.HatGroesse) erledigt += vorher.Bytes;
                    vorher = f;
                    string name = f.Text.Length > 70 ? f.Text.Substring(0, 67) + "..." : f.Text;
                    string t = T("Lösche Eintrag ", "Deleting entry ") + i + T(" von ", " of ") + n + (groessenDa && gesamt > 0 ? "  ·  " + Kern.GroesseText(erledigt) + T(" von ", " of ") + Kern.GroesseText(gesamt) : "") + "  ·  " + name;
                    double wert = n > 0 ? (groessenDa && gesamt > 0 ? erledigt * 100.0 / gesamt : (i - 1) * 100.0 / n) : 0;
                    Stand(t);
                    W.Dispatcher.BeginInvoke(new Action(() => pbFortschritt.Value = wert), System.Windows.Threading.DispatcherPriority.Background);
                };
            }
            Log("");
            LAEUFT = true;
            UI(() => { var b = (Border)W.FindName("bdLaeuft"); b.Visibility = Visibility.Visible; });
            Ergebnis erg;
            try
            {
                try
                {
                erg = K.FuehreLoeschung(() => FrageWahl(T("ShadowMount konnte nicht sicher angehalten werden:\n", "ShadowMount could not be stopped safely:\n") + K.SM_GRUND +
                    T("\n\nLäuft es noch, können eingebundene Spieldaten beim Löschen BESCHÄDIGT werden, und gelöschte Kacheln kommen sofort wieder.\n\nEmpfehlung: abbrechen, die PS5 neu starten, nur den FTP-Server laden (ohne ShadowMount) und dann nochmal löschen.",
                      "\n\nIf it is still running, mounted game data can be DAMAGED while deleting, and deleted tiles come back right away.\n\nRecommended: cancel, restart the PS5, load only the FTP server (without ShadowMount) and delete again."),
                    "ShadowMount", T("Abbrechen (empfohlen)", "Cancel (recommended)"), T("Trotzdem löschen (auf eigene Gefahr)", "Delete anyway (at your own risk)")));
                }
                catch (AbbruchAusnahme)
                {
                    // kann heute nicht vorkommen (Abbrechen ist beim Löschen gesperrt) - aber dann ehrlich melden
                    erg = new Ergebnis { Fehler = 1 };
                    Log(T("Löschen wurde unterbrochen - ein Teil kann schon gelöscht sein. Bitte das Protokoll prüfen.", "Deleting was interrupted - part may already be deleted. Please check the log."));
                }
            }
            finally { LAEUFT = false; K.Fortschritt = delegate { }; UI(() => { var b = (Border)W.FindName("bdLaeuft"); b.Visibility = Visibility.Collapsed; }); }
            UI(() => pbFortschritt.IsIndeterminate = false);
            Stand("");
            if (erg.Abgebrochen)
            {
                if (K.SM_STOP_GESENDET) Log(T("Abgebrochen - nichts gelöscht. ShadowMount hat das Stopp-Signal erhalten - zum Weiterspielen neu laden.", "Cancelled - nothing deleted. ShadowMount received the stop signal - load it again to keep playing."));
                else Log(T("Abgebrochen - nichts verändert.", "Cancelled - nothing changed."));
                K.SmZuruecksetzen(); return;
            }
            UI(() => pbFortschritt.Value = 100);
            Log("");
            Log(erg.Fehler == 0 ? T("Fertig - alles Ausgewählte ist gelöscht.", "Done - everything selected has been deleted.") : T("Fertig, aber mit ", "Done, but with ") + erg.Fehler + T(" Fehler(n) - siehe oben.", " error(s) - see above."));
            Log(Kern.FreigeraeumtText(erg.Frei));
            Log(K.KopplungsText());
            if (erg.SmText != "") Log(erg.SmText);
            foreach (var z in Kern.SCHLUSS_TEXT) Log(z);
            FertigMeldung(erg.Fehler == 0 ? T("Fertig - alles Ausgewählte ist gelöscht.", "Done - everything selected has been deleted.") : T("Fertig, aber mit ", "Done, but with ") + erg.Fehler + T(" Fehler(n) - siehe Protokoll.", " error(s) - see log."),
                          erg.Fehler == 0, Kern.FreigeraeumtText(erg.Frei), erg.SmText, Protokoll.Datei);
            // neu nachschauen, damit die Liste stimmt. Klappt das nicht, keine alte
            // Liste stehen lassen - lieber neu verbinden lassen.
            try { K.Abbruch = false; K.SucheFunde(); }
            catch (Exception ex)
            {
                Log(T("Konnte nicht neu nachschauen: ", "Could not scan again: ") + ex.Message + T(" - bitte neu verbinden.", " - please connect again."));
                K.Funde = new List<Eintrag>(); K.FUNDE_IP = "";
            }
            VERBUNDEN = K.FUNDE_IP != "";
            F_GELOESCHT = true; F_SCAN = VERBUNDEN; F_SCAN_TEST = Testmodus();
            UI(() => { ZeigeBereiche(); ZeigeSpeicher(); pbFortschritt.Value = 0; });
            HintergrundStarten();
        }

        // ---------------------------------------------------------------
        // NEU 1.0.5: Anzeige Konsole / Kopplung / Speicher / Hinweise
        // (nur Anzeige - an Suche, Löschen und Abfragen ändert sich nichts)
        // ---------------------------------------------------------------
        static SolidColorBrush Farbe(string hex) { return (SolidColorBrush)new BrushConverter().ConvertFromString(hex); }
        public void KonsoleLeeren()
        {
            tbReihe.Text = "–"; tbModellNr.Text = T("Noch nicht verbunden", "Not connected yet");
            foreach (var t in new[] { tbFirmware, tbErkannt, tbVerbindung, tbIntern, tbM2, tbUsb, tbSm }) { t.Text = "–"; t.Foreground = (Brush)W.FindResource("Text"); }
            tbFundInfo.Text = "";
            bdHinweis.Visibility = Visibility.Collapsed;
            ZeigeKopplung();
            TestHinweis();
        }
        public void ZeigeKonsole(string modell, string reihe, string firmware, string quelle, int port)
        {
            tbReihe.Text = reihe != "" ? reihe : T("Unbekanntes Modell", "Unknown model");
            tbModellNr.Text = modell != "" ? modell : T("Modellnummer unbekannt", "Model number unknown");
            tbFirmware.Text = firmware != "" ? firmware : T("unbekannt", "unknown");
            tbErkannt.Text = quelle;
            tbVerbindung.Text = "FTP, Port " + port;
            ZeigeKopplung();
        }
        public void ZeigeKopplung()
        {
            string t = K.LaufwerkText();
            int i = t.IndexOf(": ", StringComparison.Ordinal);
            tbKopplungTitel.Text = i > 0 ? t.Substring(0, i) : t;
            tbKopplungText.Text = i > 0 ? t.Substring(i + 2) : "";
            if (K.DDD_STATUS == null && K.MODELL_TYP == "" && !VERBUNDEN && F_PORT == 0) { tbKopplungTitel.Text = T("Nicht abgefragt", "Not queried"); tbKopplungText.Text = T("Erst verbinden.", "Connect first."); }
            string vg, hg, rand, sym; bool gestrichelt = false;
            switch (K.KopplungsStufe())
            {
                case Kern.KopplungsArt.Bestaetigt: vg = "#8FE0B5"; hg = "#173525"; rand = "#2E7A54"; sym = "\uE73E"; break;   // Haken
                case Kern.KopplungsArt.Keine: vg = "#C9CCD4"; hg = "#2A2D35"; rand = "#4A4F5C"; sym = "\uE738"; break;        // Strich
                case Kern.KopplungsArt.Unklar: vg = "#F2C46B"; hg = "#3A2E14"; rand = "#8A6A26"; sym = "\uE9CE"; break;       // Fragezeichen
                default: vg = "#C9CCD4"; hg = "#22242A"; rand = "#4A4F5C"; sym = "\uE711"; gestrichelt = true; break;        // nicht auslesbar
            }
            tbKopplungTitel.Foreground = Farbe(vg); tbKopplungSymbol.Foreground = Farbe(vg); tbKopplungSymbol.Text = sym;
            bdKopplungSymbol.Background = Farbe(hg); bdKopplungSymbol.BorderBrush = Farbe(rand);
            bdKopplung.BorderBrush = K.KopplungsStufe() == Kern.KopplungsArt.Bestaetigt ? Farbe("#2B5C45") : K.KopplungsStufe() == Kern.KopplungsArt.Unklar ? Farbe("#5C4A22") : (Brush)W.FindResource("Rand");
            bdKopplungSymbol.Opacity = gestrichelt ? 0.8 : 1;
        }
        public void ZeigeSpeicher()
        {
            if (!VERBUNDEN) return;
            int intern = K.Funde.Count(f => f.Kat >= 1 && f.Kat <= 6);
            tbIntern.Text = T("durchsucht · ", "scanned · ") + intern + T(" Einträge", " entries");
            if (!K.m2Da) { tbM2.Text = T("nicht gefunden", "not found"); tbM2.Foreground = (Brush)W.FindResource("Text"); }
            else if (K.M2_OHNE_USER) { tbM2.Text = T("leer oder nicht richtig erkannt", "empty or not detected properly"); tbM2.Foreground = (Brush)W.FindResource("Gelb"); }
            else { tbM2.Text = T("erkannt · ", "detected · ") + K.Anzahl(7) + T(" Einträge", " entries"); tbM2.Foreground = Farbe("#8FE0B5"); }
            tbUsb.Text = K.USB_SPEICHER ? T("angeschlossen (erweiterter Speicher)", "connected (extended storage)") : T("nicht gefunden", "not found");
            tbUsb.Foreground = K.USB_SPEICHER ? (Brush)W.FindResource("Gelb") : (Brush)W.FindResource("Text");
            int sm = K.Funde.Count(f => f.Kat == 1 && f.Quelle != "");
            tbSm.Text = sm > 0 ? sm + T(" Spiel(e) gefunden", " game(s) found") : T("keine Spiele gefunden", "no games found");
            tbFundInfo.Text = K.Funde.Count + T(" Einträge gefunden · nichts verändert", " entries found · nothing changed");
            string h = K.SpeicherHinweis().Trim();
            tbHinweis.Text = h;
            bdHinweis.Visibility = h != "" ? Visibility.Visible : Visibility.Collapsed;
        }
        // grüner Testmodus-Hinweis; in sehr niedrigen Fenstern zeigen ihn der grüne Haken und die Statuszeile
        public void TestHinweis() { bdTest.Visibility = cbTest.IsChecked == true && kompakt != true ? Visibility.Visible : Visibility.Collapsed; }

        // Fenster nie größer als der freie Bildschirm (z.B. 1366 x 768 mit 125 % Skalierung)
        // NEU 1.0.5: Startgröße. Gemerkte Größe/Position (wenn sie noch auf einen Bildschirm passt),
        // sonst passend zum freien Bildschirm. Werte sind geräteunabhängig - die Windows-Skalierung
        // rechnet WPF selbst ein.
        public bool FensterMerken = true;
        static string FensterDatei { get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "PS5-Cleanup", "fenster.txt"); } }
        void FensterGroesse()
        {
            var wa = SystemParameters.WorkArea;
            bool gemerkt = false, maximiert = false;
            if (FensterMerken)
            {
                try
                {
                    var t = File.Exists(FensterDatei) ? File.ReadAllText(FensterDatei).Trim().Split(';') : new string[0];
                    double l, o, br, ho;
                    if (t.Length >= 5 && double.TryParse(t[0], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out l)
                        && double.TryParse(t[1], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out o)
                        && double.TryParse(t[2], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out br)
                        && double.TryParse(t[3], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out ho))
                    {
                        // nur übernehmen, wenn das Fenster noch sichtbar auf den Bildschirmen liegt
                        var vs = new Rect(SystemParameters.VirtualScreenLeft, SystemParameters.VirtualScreenTop, SystemParameters.VirtualScreenWidth, SystemParameters.VirtualScreenHeight);
                        var r = new Rect(l, o, Math.Max(br, W.MinWidth), Math.Max(ho, W.MinHeight));
                        var schnitt = Rect.Intersect(vs, r);
                        if (!schnitt.IsEmpty && schnitt.Width >= 200 && schnitt.Height >= 150)
                        {
                            W.WindowStartupLocation = WindowStartupLocation.Manual;
                            W.Left = r.Left; W.Top = r.Top; W.Width = r.Width; W.Height = r.Height;
                            maximiert = t[4] == "1"; gemerkt = true;
                        }
                    }
                }
                catch { }
            }
            if (!gemerkt && FensterMerken)
            {
                // erster Start: etwa 80 % des freien Bildschirms, nicht riesig, nie kleiner als nötig
                W.Width = Math.Max(W.MinWidth, Math.Min(1500, Math.Floor(wa.Width * 0.82)));
                W.Height = Math.Max(W.MinHeight, Math.Min(1000, Math.Floor(wa.Height * 0.9)));
            }
            if (FensterMerken && W.Width > wa.Width) W.Width = Math.Max(W.MinWidth, wa.Width);
            if (FensterMerken && W.Height > wa.Height) W.Height = Math.Max(W.MinHeight, wa.Height);
            if (!gemerkt && W.WindowStartupLocation == WindowStartupLocation.CenterScreen) { W.Left = wa.Left + (wa.Width - W.Width) / 2; W.Top = wa.Top + (wa.Height - W.Height) / 2; }
            if (maximiert) W.WindowState = WindowState.Maximized;
            LayoutAnpassen();
        }
        void FensterSpeichern()
        {
            if (!FensterMerken) return;
            try
            {
                var r = W.WindowState == WindowState.Normal ? new Rect(W.Left, W.Top, W.ActualWidth, W.ActualHeight) : W.RestoreBounds;
                if (r.IsEmpty || r.Width < 100) return;
                Directory.CreateDirectory(Path.GetDirectoryName(FensterDatei));
                var ic = System.Globalization.CultureInfo.InvariantCulture;
                File.WriteAllText(FensterDatei, string.Format(ic, "{0:0};{1:0};{2:0};{3:0};{4}", r.Left, r.Top, r.Width, r.Height, W.WindowState == WindowState.Maximized ? "1" : "0"), new UTF8Encoding(false));
            }
            catch { }
        }

        // NEU 1.0.5: Anpassung an die Fenstergröße.
        // - Große Fenster (z.B. maximiert auf großen Monitoren): alles wird gleichmäßig größer
        //   (Schrift, Knöpfe, Karten) statt nur leere Flächen - höchstens 1,3-fach
        //   (Rückmeldung: 1,6 war im Vollbild zu groß, so passen mehr Spiele in die Liste).
        // - Kleinere Fenster: die linke Spalte wird schmaler, niedrige Fenster klappen das
        //   Protokoll ein, sehr niedrige zeigen einen kompakten Kopf. Keine Funktion fällt weg.
        public const double MAX_ZOOM = 1.3;
        bool? niedrig; bool? kompakt; double zoom = 1;
        public double Zoom { get { return zoom; } }
        public void LayoutAnpassen()
        {
            double b = W.ActualWidth > 0 ? W.ActualWidth : W.Width, h = W.ActualHeight > 0 ? W.ActualHeight : W.Height;
            double z = Math.Min(b / 1300.0, h / 900.0);
            z = Math.Max(1.0, Math.Min(MAX_ZOOM, Math.Floor(z * 20) / 20));      // in 5-%-Schritten
            if (Math.Abs(z - zoom) > 0.001)
            {
                zoom = z;
                ((Grid)W.FindName("gAussen")).LayoutTransform = z > 1.001 ? new ScaleTransform(z, z) : null;
                // vergrößerte Schrift scharf darstellen
                TextOptions.SetTextFormattingMode(W, z > 1.001 ? TextFormattingMode.Ideal : TextFormattingMode.Display);
            }
            double bl = b / zoom, hl = h / zoom;                              // Platz in "normaler" Größe
            double seite = bl >= 1150 ? 320 : Math.Max(240, Math.Floor(bl * 0.28));
            if (Math.Abs(cdSeite.Width.Value - seite) > 0.5) cdSeite.Width = new GridLength(seite);
            bool n = hl < 820;
            if (niedrig != n) { niedrig = n; exProtokoll.IsExpanded = !n; }
            bool k = hl < 700;
            if (kompakt != k)
            {
                kompakt = k;
                ((TextBlock)W.FindName("tbKopf")).Visibility = k ? Visibility.Collapsed : Visibility.Visible;
                tbFuss.Visibility = k ? Visibility.Collapsed : Visibility.Visible;
                TestHinweis();
            }
        }

        // ---------------------------------------------------------------
        // NEU 1.0.5: freiwilliges Feedback. Hier wird nichts gesendet - der
        // Bericht wird angezeigt; nur der Knopf "Auf GitHub öffnen" startet den
        // Browser mit einem vorausgefüllten Issue, abgeschickt wird erst dort.
        // ---------------------------------------------------------------
        public FeedbackDaten FeedbackSammeln()
        {
            var d = new FeedbackDaten();
            d.Modell = F_MODELL; d.Reihe = F_REIHE; d.ModellQuelle = F_PORT > 0 ? F_QUELLE : ""; d.Firmware = F_FIRMWARE;
            d.DddRoh = K.DDD_STATUS;
            d.LaufwerkText = F_PORT > 0 ? K.LaufwerkText() : "";
            // NEU 1.0.5 (Variante 2): was der Server selbst über sich sagt; sonst "Unbekannt" (+ Begrüßung zum Nachvollziehen)
            d.FtpServer = F_FTPSERVER != "" ? F_FTPSERVER : F_BEGRUESSUNG != "" ? Feedback.Unbekannt + T(" (Begrüßung: „", " (greeting: \"") + F_BEGRUESSUNG + T("“)", "\")") : "";
            d.FtpDaten = F_DATEN;
            d.Port = F_PORT;
            d.ScanGemacht = F_SCAN && VERBUNDEN; d.ScanImTestmodus = F_SCAN_TEST;
            foreach (var k in Kern.ALLE_KAT) d.AnzahlJeBereich[k] = K.Funde.Count(f => f.Kat == k);
            if (d.ScanGemacht)
            {
                d.M2 = !K.m2Da ? T("nicht gefunden", "not found") : K.M2_OHNE_USER ? T("leer oder nicht richtig erkannt", "empty or not detected properly") : T("erkannt", "detected");
                d.Usb = K.USB_SPEICHER ? T("angeschlossen (erweiterter Speicher)", "connected (extended storage)") : T("nicht gefunden", "not found");
                d.ShadowMountSpiele = K.Funde.Count(f => f.Kat == 1 && f.Quelle != "");
            }
            d.GeloeschtInSitzung = F_GELOESCHT;
            d.Fehler = Feedback.FehlerAus(tbLog.Text);
            d.Windows = Programm.WindowsVersion();
            d.PersoenlicheNamen = new[] { Environment.UserName, Environment.MachineName, Environment.UserDomainName };
            return d;
        }
        public Window FeedbackFenster(FeedbackDaten d)
        {
            var w = Dialog(DLG_KOPF.Replace("SizeToContent=\"Height\" ", "").Replace("ResizeMode=\"NoResize\" ", "") + "Width=\"720\" Height=\"640\" MinWidth=\"520\" MinHeight=\"420\"><Grid Margin=\"18\">" +
                "<Grid.RowDefinitions><RowDefinition Height=\"Auto\"/><RowDefinition Height=\"Auto\"/><RowDefinition Height=\"*\"/><RowDefinition Height=\"Auto\"/></Grid.RowDefinitions>" +
                "<TextBlock x:Name=\"tKopf\" FontSize=\"16\" FontWeight=\"SemiBold\" Foreground=\"#E9EAEE\" TextWrapping=\"Wrap\"/>" +
                "<Border Grid.Row=\"1\" Background=\"#16233A\" BorderBrush=\"#2C4A7A\" BorderThickness=\"1\" CornerRadius=\"10\" Padding=\"12,8\" Margin=\"0,10,0,10\">" +
                "<TextBlock x:Name=\"tInfo\" Foreground=\"#C6DAFB\" TextWrapping=\"Wrap\"/></Border>" +
                "<TextBox x:Name=\"tBericht\" Grid.Row=\"2\" AcceptsReturn=\"True\" TextWrapping=\"Wrap\" VerticalScrollBarVisibility=\"Auto\" FontFamily=\"Consolas\" FontSize=\"12\" Background=\"#15161A\" Foreground=\"#E9EAEE\" BorderBrush=\"#3A3E49\" Padding=\"8,6\" CaretBrush=\"White\"/>" +
                "<WrapPanel Grid.Row=\"3\" HorizontalAlignment=\"Right\" Margin=\"0,12,0,0\">" +
                "<Button x:Name=\"bZu\" Padding=\"14,7\" Margin=\"4\" Foreground=\"#E9EAEE\" Background=\"#2A2D35\" BorderBrush=\"#3A3E49\" IsCancel=\"True\"/>" +
                "<Button x:Name=\"bKopie\" Padding=\"14,7\" Margin=\"4\" Foreground=\"#E9EAEE\" Background=\"#2A2D35\" BorderBrush=\"#3A3E49\"/>" +
                "<Button x:Name=\"bGithub\" Padding=\"14,7\" Margin=\"4\" Foreground=\"White\" Background=\"#2F6FDB\" BorderBrush=\"#3B8BFF\" FontWeight=\"SemiBold\"/>" +
                "</WrapPanel></Grid></Window>");
            w.Title = T("Feedback / Kompatibilität melden", "Send Feedback / Compatibility Report");
            ((TextBlock)w.FindName("tKopf")).Text = T("Dein Bericht – bitte prüfen", "Your report – please check it");
            ((TextBlock)w.FindName("tInfo")).Text = T("Freiwillig: Es wird nichts automatisch gesendet. Erst mit „Auf GitHub öffnen“ öffnet sich dein Browser mit einem vorausgefüllten Issue – dort kannst du alles ändern und entscheidest selbst, ob du es absendest. GitHub-Issues sind öffentlich sichtbar, zum Absenden brauchst du normalerweise ein GitHub-Konto. IP-Adressen, Windows-Benutzername und Pfade sind bereits entfernt. Du kannst den Text hier auch selbst ändern.",
                "Voluntary: nothing is sent automatically. Only \"Open on GitHub\" opens your browser with a pre-filled issue – there you can change everything and decide yourself whether to submit it. GitHub issues are publicly visible, and you normally need a GitHub account to submit. IP addresses, Windows user name and paths have already been removed. You can also edit the text here.");
            var tx = (TextBox)w.FindName("tBericht");
            tx.Text = Feedback.Bericht(d).Replace("\n", "\r\n");
            string titel = Feedback.Titel(d);
            var bz = (Button)w.FindName("bZu"); var bk = (Button)w.FindName("bKopie"); var bg = (Button)w.FindName("bGithub");
            bz.Content = T("Schließen", "Close"); bk.Content = T("Nur kopieren", "Copy only"); bg.Content = T("Auf GitHub öffnen", "Open on GitHub");
            bz.Click += delegate { w.Close(); };
            bk.Click += delegate
            {
                try { Clipboard.SetText(tx.Text); Log(T("Feedback-Bericht in die Zwischenablage kopiert (nichts gesendet).", "Feedback report copied to the clipboard (nothing sent).")); }
                catch (Exception ex) { Meldung(T("Kopieren ging nicht: ", "Could not copy: ") + ex.Message, "PS5 Cleanup", MessageBoxImage.Warning); }
            };
            bg.Click += delegate
            {
                bool gekuerzt;
                string link = Feedback.IssueLink(titel, tx.Text.Replace("\r\n", "\n"), out gekuerzt);
                if (gekuerzt) { try { Clipboard.SetText(tx.Text); } catch { } }
                try
                {
                    Process.Start(new ProcessStartInfo(link) { UseShellExecute = true });
                    Log(T("Browser mit dem Feedback-Bericht geöffnet - abgeschickt wird erst, wenn du es auf GitHub selbst absendest.", "Browser opened with the feedback report - it is only submitted when you submit it on GitHub yourself."));
                    if (gekuerzt) Meldung(T("Der Bericht ist zu lang für den Link. Er liegt in der Zwischenablage - bitte auf GitHub einfügen (Strg+V).", "The report is too long for the link. It is on the clipboard - please paste it on GitHub (Ctrl+V)."), "PS5 Cleanup");
                }
                catch (Exception ex) { Meldung(T("Browser konnte nicht geöffnet werden: ", "Could not open the browser: ") + ex.Message + T("\n\nDu kannst den Bericht mit „Nur kopieren“ übernehmen.", "\n\nYou can take the report with \"Copy only\"."), "PS5 Cleanup", MessageBoxImage.Warning); }
            };
            return w;
        }
        void FeedbackOeffnen()
        {
            var w = FeedbackFenster(FeedbackSammeln());
            if (W.IsVisible) w.Owner = W;
            w.ShowDialog();
        }

        // ---------------------------------------------------------------
        // NEU 1.0.5: Protokoll - Größe per Trennlinie, alles kopieren, eigenes Fenster
        // ---------------------------------------------------------------
        void ProtokollZeile()
        {
            if (exProtokoll.IsExpanded)
            {
                rdProtokoll.Height = new GridLength(protokollHoehe);
                rdProtokoll.MinHeight = 110;
                gsProtokoll.Visibility = Visibility.Visible;
            }
            else
            {
                if (rdProtokoll.Height.IsAbsolute && rdProtokoll.Height.Value >= 110) protokollHoehe = rdProtokoll.Height.Value;
                rdProtokoll.MinHeight = 0;
                rdProtokoll.Height = GridLength.Auto;
                gsProtokoll.Visibility = Visibility.Collapsed;
            }
        }
        // kopiert immer den ganzen Inhalt (nicht nur den sichtbaren Ausschnitt)
        public string LogText() { return tbLog.Text; }
        void LogKopieren()
        {
            string t = tbLog.Text;
            if (t.Trim() == "") { Meldung(T("Das Protokoll ist noch leer.", "The log is still empty."), "PS5 Cleanup"); return; }
            try
            {
                Clipboard.SetText(t);
                int zeilen = t.Split(new[] { "\r\n" }, StringSplitOptions.None).Length - 1;
                Stand(T("Protokoll kopiert: ", "Log copied: ") + zeilen + T(" Zeilen in der Zwischenablage. Vor dem Teilen bitte auf persönliche Daten prüfen.", " lines on the clipboard. Please check for personal data before sharing."));
            }
            catch (Exception ex) { Meldung(T("Kopieren ging nicht: ", "Could not copy: ") + ex.Message, "PS5 Cleanup", MessageBoxImage.Warning); }
        }
        void LogFensterOeffnen()
        {
            if (logFenster != null) { logFenster.Activate(); return; }
            var w = Dialog(DLG_KOPF.Replace("SizeToContent=\"Height\" ", "").Replace("ResizeMode=\"NoResize\" ", "").Replace("WindowStartupLocation=\"CenterOwner\"", "WindowStartupLocation=\"CenterScreen\"") + "Width=\"980\" Height=\"640\" MinWidth=\"480\" MinHeight=\"300\"><Grid Margin=\"14\">" +
                "<Grid.RowDefinitions><RowDefinition Height=\"*\"/><RowDefinition Height=\"Auto\"/></Grid.RowDefinitions>" +
                "<TextBox x:Name=\"tLog\" IsReadOnly=\"True\" FontFamily=\"Consolas\" FontSize=\"12.5\" VerticalScrollBarVisibility=\"Auto\" HorizontalScrollBarVisibility=\"Auto\" TextWrapping=\"NoWrap\" Background=\"#15161A\" Foreground=\"#E9EAEE\" BorderBrush=\"#3A3E49\" Padding=\"8,6\"/>" +
                "<WrapPanel Grid.Row=\"1\" HorizontalAlignment=\"Right\" Margin=\"0,10,0,0\">" +
                "<Button x:Name=\"bKopie\" Padding=\"14,7\" Margin=\"4\" Foreground=\"#E9EAEE\" Background=\"#2A2D35\" BorderBrush=\"#3A3E49\"/>" +
                "<Button x:Name=\"bZu\" Padding=\"14,7\" Margin=\"4\" Foreground=\"White\" Background=\"#2F6FDB\" BorderBrush=\"#3B8BFF\" IsCancel=\"True\"/>" +
                "</WrapPanel></Grid></Window>");
            w.Title = T("PS5 Cleanup - Protokoll", "PS5 Cleanup - log");
            try { w.Icon = W.Icon; } catch { }
            var tx = (TextBox)w.FindName("tLog");
            tx.Text = tbLog.Text; tx.ScrollToEnd();
            var bk = (Button)w.FindName("bKopie"); var bz = (Button)w.FindName("bZu");
            bk.Content = T("Alles kopieren", "Copy all"); bz.Content = T("Schließen", "Close");
            bk.Click += delegate { LogKopieren(); };
            bz.Click += delegate { w.Close(); };
            w.Closed += delegate { logFenster = null; logFensterTb = null; };
            logFenster = w; logFensterTb = tx;
            if (W.IsVisible) w.Owner = W;
            w.Show();
        }

        // ---------------------------------------------------------------
        // Protokoll speichern, Hilfe
        // ---------------------------------------------------------------
        void ProtokollSpeichern()
        {
            var d = new Microsoft.Win32.SaveFileDialog();
            d.Title = T("Protokoll speichern", "Save log"); d.Filter = T("Textdatei (*.log;*.txt)|*.log;*.txt|Alle Dateien|*.*", "Text file (*.log;*.txt)|*.log;*.txt|All files|*.*");
            d.FileName = "PS5-Cleanup_" + DateTime.Now.ToString("yyyy-MM-dd_HH-mm") + ".log";
            if (d.ShowDialog(W) != true) return;
            try
            {
                string inhalt = (Protokoll.Datei != "" && File.Exists(Protokoll.Datei)) ? File.ReadAllText(Protokoll.Datei) : tbLog.Text;
                File.WriteAllText(d.FileName, inhalt, new UTF8Encoding(true));
                Log(T("Protokoll gespeichert: ", "Log saved: ") + d.FileName);
                Log(T("Hinweis: Vor dem Teilen bitte auf persönliche Daten prüfen (z. B. IP-Adresse, Benutzername in Pfaden).", "Note: before sharing, please check it for personal data (e.g. IP address, user name in paths)."));
            }
            catch (Exception ex) { Meldung(T("Konnte nicht speichern: ", "Could not save: ") + ex.Message, "PS5 Cleanup", MessageBoxImage.Error); }
        }
        void HilfeOeffnen()
        {
            string p = Programm.LiesmichPfad();
            if (p != "") { try { Process.Start("notepad.exe", "\"" + p + "\""); } catch { } }
            else Meldung(T("LIESMICH.txt nicht gefunden.", "README_EN.txt not found."), "PS5 Cleanup");
        }

        // ---------------------------------------------------------------
        // Hinweis auf neue Versionen (NEU 1.0.4): fragt einmal beim Start bei
        // GitHub nach der neuesten Version - nur anzeigen, nichts herunterladen.
        // ---------------------------------------------------------------
        // Rückmeldung aus dem Forum: mit anderen FTP-Servern klappt das Lesen nicht zuverlässig
        public static string FTP_TIPP { get { return T("Getestet sind die FTP-Server ftpsrv und zftpd (Einzelheiten in der Hilfe).", "Tested FTP servers: ftpsrv and zftpd (details in Help)."); } }
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
                    string eigene = Kern.VERSION;
#if UPDATETEST
                    eigene = "1.0.2 Beta";      // nur zum Testen: ältere Version vortäuschen
#endif
                    if (!Neuer(neu, eigene)) return;
                    string anzeige = Regex.Replace(neu, "^[vV]", "").Replace("-beta", " Beta");
                    W.Dispatcher.BeginInvoke(new Action(() =>
                    {
                        tbUpdate.Inlines.Clear();
                        tbUpdate.Inlines.Add(new Run(T("Es gibt eine neuere Version: ", "A newer version is available: ") + anzeige + "  "));
                        var link = new Hyperlink(new Run(T("auf GitHub ansehen", "view on GitHub"))) { NavigateUri = new Uri(RELEASES), Foreground = (Brush)W.FindResource("Blau") };
                        link.RequestNavigate += (s, e) => { try { Process.Start(new ProcessStartInfo(RELEASES) { UseShellExecute = true }); } catch { } };
                        tbUpdate.Inlines.Add(link);
                        tbUpdate.Visibility = Visibility.Visible;
                    }));
                    Protokoll.Zeile(T("Hinweis: neuere Version auf GitHub: ", "Note: newer version on GitHub: ") + neu);
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
