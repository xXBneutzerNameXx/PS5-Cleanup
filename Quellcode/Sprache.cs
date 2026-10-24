// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// Sprache.cs - Deutsch / Englisch (NEU 1.0.4). Jeder sichtbare Text steht
// an seiner Stelle in beiden Sprachen: T("deutsch", "english").
// Gewählt wird nach der Windows-Sprache; der Knopf oben rechts schaltet um
// (gespeichert in %APPDATA%\PS5-Cleanup\sprache.txt).
// Selbsttest und Testfälle laufen immer auf Deutsch, außer -Sprache en.
// =====================================================================
using System;
using System.Globalization;
using System.IO;
using System.Text;

namespace PS5Cleanup
{
    public static class Sp
    {
        public static bool EN;
        public static string T(string de, string en) { return EN ? en : de; }
        // Zahlenformat passend zur Sprache (17,5 GB / 17.5 GB)
        public static CultureInfo Kultur { get { return EN ? CultureInfo.GetCultureInfo("en-US") : CultureInfo.GetCultureInfo("de-DE"); } }

        static string Datei { get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "PS5-Cleanup", "sprache.txt"); } }

        // gespeicherte Wahl, sonst Windows-Sprache (Deutsch -> Deutsch, alles andere -> Englisch)
        public static void Laden()
        {
            try
            {
                if (File.Exists(Datei))
                {
                    string s = File.ReadAllText(Datei).Trim().ToLowerInvariant();
                    if (s == "de" || s == "en") { EN = s == "en"; return; }
                }
            }
            catch { }
            EN = !string.Equals(CultureInfo.CurrentUICulture.TwoLetterISOLanguageName, "de", StringComparison.OrdinalIgnoreCase);
        }

        public static bool Speichern(bool en)
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(Datei));
                File.WriteAllText(Datei, en ? "en" : "de", new UTF8Encoding(false));
                return true;
            }
            catch { return false; }
        }
    }
}
