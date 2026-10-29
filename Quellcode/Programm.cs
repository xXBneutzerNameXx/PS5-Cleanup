// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// Programm.cs - Start, eingebaute Dateien (ps5info.elf, LIESMICH.txt),
// Protokolldatei.
//   PS5-Cleanup.exe                       -> Fenster
//   PS5-Cleanup.exe -SelbstTest [-Bild x] -> Fenster mit Probe-Einträgen
//                                            aufbauen, prüfen, schließen
// =====================================================================
using System;
using System.IO;
using System.Reflection;
using System.Text;
using System.Windows;

[assembly: AssemblyTitle("PS5 Cleanup")]
[assembly: AssemblyDescription("PS5 aufräumen wie frisch eingerichtet - ohne Neuinstallation")]
[assembly: AssemblyCompany("da_Chris86")]
[assembly: AssemblyProduct("PS5 Cleanup")]
[assembly: AssemblyCopyright("Copyright (c) 2026 da_Chris86 - MIT-Lizenz")]
[assembly: AssemblyVersion("1.0.6.0")]
[assembly: AssemblyFileVersion("1.0.6.0")]
[assembly: AssemblyInformationalVersion("1.0.6")]

namespace PS5Cleanup
{
    public static class Protokoll
    {
        public static string Datei = "";
        static readonly object sperre = new object();
        public static void Anlegen()
        {
            foreach (var o in new[] { Programm.Ordner, Path.Combine(Path.GetTempPath(), "PS5-Cleanup") })
            {
                try
                {
                    if (!Directory.Exists(o)) Directory.CreateDirectory(o);
                    string p = Path.Combine(o, "PS5-Cleanup.log");
                    if (!File.Exists(p)) File.WriteAllText(p, "", new UTF8Encoding(true));
                    else File.AppendAllText(p, "\r\n");
                    Datei = p; break;
                }
                catch { }
            }
            string os = Programm.WindowsVersion();
            if (os == "") os = Environment.OSVersion.VersionString;
            Zeile(new string('=', 60));
            Zeile("PS5 Cleanup " + Kern.VERSION + " by da_Chris86 - " + Sp.T("Protokoll", "log"));
            Zeile("Start: " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "  |  " + os + "  |  .NET " + Environment.Version + "  |  " + Sp.T("Programm: ", "Program: ") + Programm.Ordner + "  |  " + Sp.T("Sprache: Deutsch", "Language: English"));
        }
        public static void Zeile(string t)
        {
            if (Datei == "") return;
            lock (sperre)
            {
                try { File.AppendAllText(Datei, "[" + DateTime.Now.ToString("HH:mm:ss") + "] " + t + "\r\n", new UTF8Encoding(false)); } catch { }
            }
        }
    }

    public static class Programm
    {
        public static string Ordner = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');

        public static byte[] Ressource(string name)
        {
            using (var s = typeof(Programm).Assembly.GetManifestResourceStream("PS5Cleanup." + name))
            {
                if (s == null) return null;
                var ms = new MemoryStream(); s.CopyTo(ms); return ms.ToArray();
            }
        }

        // Anleitung (LIESMICH.txt / README_EN.txt) neben dem Programm - sonst die eingebaute in %TEMP% ablegen
        public static string LiesmichPfad()
        {
            string name = Sp.EN ? "README_EN.txt" : "LIESMICH.txt";
            foreach (var o in new[] { Ordner, Path.GetDirectoryName(Ordner) ?? Ordner })
            {
                string p = Path.Combine(o, name);
                if (File.Exists(p)) return p;
            }
            try
            {
                var b = Ressource(name);
                if (b == null) return "";
                string d = Path.Combine(Path.GetTempPath(), "PS5-Cleanup");
                Directory.CreateDirectory(d);
                string p = Path.Combine(d, name);
                File.WriteAllBytes(p, b);
                return p;
            }
            catch { return ""; }
        }

        // NEU 1.0.5: echte Windows-Version, nur aus der Registrierung gelesen. Windows 11 trägt dort
        // weiter "Windows 10 ..." als Namen - darum entscheidet die Build-Nummer (ab 22000 = Windows 11).
        public static string WindowsVersion()
        {
            try
            {
                using (var k = Microsoft.Win32.Registry.LocalMachine.OpenSubKey("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"))
                {
                    if (k == null) return "";
                    int build; int.TryParse(Convert.ToString(k.GetValue("CurrentBuildNumber")), out build);
                    string name = Convert.ToString(k.GetValue("ProductName")).Trim();
                    string anzeige = Convert.ToString(k.GetValue("DisplayVersion")).Trim();
                    if (build >= 22000 && name.StartsWith("Windows 10", StringComparison.Ordinal)) name = "Windows 11" + name.Substring(10);
                    if (name == "") name = build >= 22000 ? "Windows 11" : build >= 10240 ? "Windows 10" : "Windows";
                    return name + (anzeige != "" ? " " + anzeige : "") + (build > 0 ? " (Build " + build + ")" : "");
                }
            }
            catch { }
            return "";
        }

        [STAThread]
        public static int Main(string[] args)
        {
            bool selbstTest = false; string bild = ""; int uiPort = 0; string uiOrdner = ""; string sprache = ""; string fenster = "";
            for (int i = 0; i < args.Length; i++)
            {
                if (string.Equals(args[i], "-SelbstTest", StringComparison.OrdinalIgnoreCase)) selbstTest = true;
                else if (string.Equals(args[i], "-Bild", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length) bild = args[++i];
                else if (string.Equals(args[i], "-Sprache", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length) sprache = args[++i].ToLowerInvariant();
                else if (string.Equals(args[i], "-Fenster", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length) fenster = args[++i];   // nur Selbsttest: Fenstergröße BxH
                else if (string.Equals(args[i], "-UiTest", StringComparison.OrdinalIgnoreCase) && i + 2 < args.Length) { uiPort = int.Parse(args[++i]); uiOrdner = args[++i]; }
            }
            // Sprache: Selbsttest/Oberflächentest auf Deutsch (außer -Sprache en), sonst gespeicherte Wahl bzw. Windows-Sprache
            if (sprache == "en" || sprache == "de") Sp.EN = sprache == "en";
            else if (selbstTest || uiPort > 0) Sp.EN = false;
            else Sp.Laden();
            var app = new Application();
            app.ShutdownMode = ShutdownMode.OnMainWindowClose;
            var o = new Oberflaeche();
            // unerwarteter Fehler in der Oberfläche: ins Protokoll, melden, weiterlaufen
            app.DispatcherUnhandledException += (s, e) =>
            {
                e.Handled = true;
                try { Protokoll.Zeile(Sp.T("FEHLER (Oberfläche): ", "ERROR (window): ") + e.Exception); o.Log(Sp.T("FEHLER: ", "ERROR: ") + e.Exception.Message); } catch { }
            };
            app.MainWindow = o.W;
            if (selbstTest)
            {
                var fm = System.Text.RegularExpressions.Regex.Match(fenster, "^(\\d+)x(\\d+)$");
                o.FensterMerken = false;      // Tests: weder gemerkte Größe laden noch speichern
                if (fm.Success) { o.W.Width = int.Parse(fm.Groups[1].Value); o.W.Height = int.Parse(fm.Groups[2].Value); }
                Selbsttest.Laufen(o, bild); app.Run(o.W); return 0;
            }
#if UITEST
            if (uiPort > 0) { o.FensterMerken = false; UiTest.Laufen(o, uiPort, uiOrdner); app.Run(o.W); return 0; }
#endif
#if UPDATETEST
            // Testfassung: echte Versionsabfrage bei GitHub, nach 8 s Bild vom Fenster speichern und schließen
            if (bild != "")
            {
                o.UpdatePruefen();
                var uhr = new System.Windows.Threading.DispatcherTimer { Interval = TimeSpan.FromSeconds(8) };
                uhr.Tick += delegate
                {
                    uhr.Stop();
                    try
                    {
                        o.W.UpdateLayout();
                        var rtb = new System.Windows.Media.Imaging.RenderTargetBitmap((int)o.W.ActualWidth, (int)o.W.ActualHeight, 96, 96, System.Windows.Media.PixelFormats.Pbgra32);
                        rtb.Render(o.W);
                        var enc = new System.Windows.Media.Imaging.PngBitmapEncoder(); enc.Frames.Add(System.Windows.Media.Imaging.BitmapFrame.Create(rtb));
                        using (var fs = File.Create(bild)) enc.Save(fs);
                    }
                    catch { }
                    o.W.Close();
                };
                uhr.Start();
                app.Run(o.W); return 0;
            }
#endif
            Protokoll.Anlegen();
            o.UpdatePruefen();
            app.Run(o.W);
            return 0;
        }
    }
}
