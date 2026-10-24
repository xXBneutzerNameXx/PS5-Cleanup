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
[assembly: AssemblyVersion("1.0.4.0")]
[assembly: AssemblyFileVersion("1.0.4.0")]
[assembly: AssemblyInformationalVersion("1.0.4 Beta")]

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
            string os = Environment.OSVersion.VersionString;
            try
            {
                using (var k = Microsoft.Win32.Registry.LocalMachine.OpenSubKey("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"))
                    if (k != null && k.GetValue("ProductName") != null) os = k.GetValue("ProductName").ToString();
            }
            catch { }
            Zeile(new string('=', 60));
            Zeile("PS5 Cleanup " + Kern.VERSION + " by da_Chris86 - Protokoll");
            Zeile("Start: " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "  |  " + os + "  |  .NET " + Environment.Version + "  |  Programm: " + Programm.Ordner);
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

        // LIESMICH neben dem Programm - sonst die eingebaute in %TEMP% ablegen
        public static string LiesmichPfad()
        {
            foreach (var o in new[] { Ordner, Path.GetDirectoryName(Ordner) ?? Ordner })
            {
                string p = Path.Combine(o, "LIESMICH.txt");
                if (File.Exists(p)) return p;
            }
            try
            {
                var b = Ressource("LIESMICH.txt");
                if (b == null) return "";
                string d = Path.Combine(Path.GetTempPath(), "PS5-Cleanup");
                Directory.CreateDirectory(d);
                string p = Path.Combine(d, "LIESMICH.txt");
                File.WriteAllBytes(p, b);
                return p;
            }
            catch { return ""; }
        }

        [STAThread]
        public static int Main(string[] args)
        {
            bool selbstTest = false; string bild = ""; int uiPort = 0; string uiOrdner = "";
            for (int i = 0; i < args.Length; i++)
            {
                if (string.Equals(args[i], "-SelbstTest", StringComparison.OrdinalIgnoreCase)) selbstTest = true;
                else if (string.Equals(args[i], "-Bild", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length) bild = args[++i];
                else if (string.Equals(args[i], "-UiTest", StringComparison.OrdinalIgnoreCase) && i + 2 < args.Length) { uiPort = int.Parse(args[++i]); uiOrdner = args[++i]; }
            }
            var app = new Application();
            app.ShutdownMode = ShutdownMode.OnMainWindowClose;
            var o = new Oberflaeche();
            // unerwarteter Fehler in der Oberfläche: ins Protokoll, melden, weiterlaufen
            app.DispatcherUnhandledException += (s, e) =>
            {
                e.Handled = true;
                try { Protokoll.Zeile("FEHLER (Oberfläche): " + e.Exception); o.Log("FEHLER: " + e.Exception.Message); } catch { }
            };
            app.MainWindow = o.W;
            if (selbstTest) { Selbsttest.Laufen(o, bild); app.Run(o.W); return 0; }
#if UITEST
            if (uiPort > 0) { UiTest.Laufen(o, uiPort, uiOrdner); app.Run(o.W); return 0; }
#endif
            Protokoll.Anlegen();
            o.UpdatePruefen();
            app.Run(o.W);
            return 0;
        }
    }
}
