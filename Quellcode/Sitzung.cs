// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// Sitzung.cs - Ordnerlisten, Existiert, Dateien lesen/schreiben über
// eine FTP-Verbindung (1:1 nach ps5_kern.ps1: Liste, Namen, Existiert,
// Datei-Text, Datei-Schreiben, Ftp-Gesund) + Hilfsfunktionen.
// =====================================================================
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;

namespace PS5Cleanup
{
    // Vergleiche wie in PowerShell (-match, -like, -contains: ohne Groß/klein)
    public static class H
    {
        const RegexOptions OPT = RegexOptions.IgnoreCase | RegexOptions.CultureInvariant;
        public static bool M(string s, string muster) { return Regex.IsMatch(s ?? "", muster, OPT); }
        public static Match Mt(string s, string muster) { return Regex.Match(s ?? "", muster, OPT); }
        public static string R(string s, string muster, string ersatz) { return Regex.Replace(s ?? "", muster, ersatz, OPT); }
        public static bool Like(string s, string wild)
        {
            string re = "^" + Regex.Escape(wild).Replace("\\*", ".*").Replace("\\?", ".") + "$";
            return Regex.IsMatch(s ?? "", re, OPT | RegexOptions.Singleline);
        }
        public static bool Hat(IEnumerable<string> liste, string s)
        {
            if (liste == null) return false;
            foreach (var x in liste) if (string.Equals(x, s, StringComparison.OrdinalIgnoreCase)) return true;
            return false;
        }
        public static bool Gleich(string a, string b) { return string.Equals(a, b, StringComparison.OrdinalIgnoreCase); }
        public static Dictionary<string, T> Tab<T>() { return new Dictionary<string, T>(StringComparer.OrdinalIgnoreCase); }
        public static readonly Regex Leer = new Regex(" +");
        public static bool Steuerzeichen(string s) { return Regex.IsMatch(s ?? "", "[\\x00-\\x1f\\x7f]"); }
        public static string Eltern(string p)
        {
            string e = p.Substring(0, p.LastIndexOf('/'));
            return e == "" ? "/" : e;
        }
        public static string Blatt(string p) { return p.Substring(p.LastIndexOf('/') + 1); }
    }

    public class Sitzung
    {
        public Ftp Ftp;
        // Abbruch durch den Nutzer: wird vor jeder Ordnerliste geprüft
        public Func<bool> Abbruch = delegate { return false; };

        public Sitzung(Ftp f) { Ftp = f; }

        public FtpAntwort Befehl(string c) { return Ftp.Befehl(c); }

        bool Cwd(string pfad) { return Ftp.Befehl("CWD " + pfad).Code < 400; }

        static List<Datei> Zerlege(byte[] bytes, bool nurName)
        {
            var erg = new List<Datei>();
            foreach (string z in Regex.Split(Ftp.UTF8.GetString(bytes), "\r?\n"))
            {
                if (z.Trim() == "") continue;
                string[] teile = H.Leer.Split(z, 9);
                if (teile.Length < 9) continue;
                string name = teile[8];
                if (name == "." || name == "..") continue;
                if (H.Steuerzeichen(name)) continue;          // Steuerzeichen im Namen: nie anfassen
                long groesse = 0;
                if (Regex.IsMatch(teile[4], "^\\d+$")) long.TryParse(teile[4], out groesse);
                // 'l' = Verknüpfung (Symlink): nie als Ordner behandeln, nie hineinschauen.
                // Manche Server hängen das Ziel an ("name -> ziel") - das gehört nicht zum Namen.
                if (z.StartsWith("l", StringComparison.Ordinal) && name.Contains(" -> ")) name = name.Substring(0, name.IndexOf(" -> ", StringComparison.Ordinal));
                erg.Add(new Datei { Name = name, IstOrdner = z.StartsWith("d", StringComparison.Ordinal), IstLink = z.StartsWith("l", StringComparison.Ordinal), Groesse = groesse });
            }
            return erg;
        }

        // Ordnerinhalt; null wenn es den Ordner nicht gibt.
        // streng: nur "gibt es nicht" (550) liefert null, jeder andere Fehler bricht ab.
        public List<Datei> Liste(string pfad, bool streng = false)
        {
            if (Abbruch()) throw new AbbruchAusnahme();
            var a = Ftp.Befehl("CWD " + pfad);
            if (a.Code >= 400)
            {
                if (streng && a.Code != 550) throw new Exception("PS5 meldet beim Lesen von " + pfad + " : " + a.Text);
                return null;
            }
            byte[] bytes = Ftp.Daten("LIST");
            Cwd("/");
            if (bytes == null)
            {
                if (streng) throw new System.IO.IOException("Ordner " + pfad + " konnte nicht vollständig gelesen werden - bitte nochmal verbinden.");
                return null;
            }
            return Zerlege(bytes, false);
        }

        public List<string> Namen(string pfad, bool streng = false)
        {
            var l = Liste(pfad, streng);
            if (l == null) return new List<string>();
            return l.Select(e => e.Name).ToList();
        }

        // Gibt es den Pfad? Nur ein klares "gibt es nicht" (550) zählt als nein -
        // jeder andere Fehler bricht ab, statt fälschlich "schon weg" zu melden.
        public bool Existiert(string pfad)
        {
            string eltern = H.Eltern(pfad);
            string name = H.Blatt(pfad);
            var a = Ftp.Befehl("CWD " + eltern);
            if (a.Code == 550) return false;
            if (a.Code >= 400) throw new Exception("PS5 meldet beim Prüfen von " + pfad + " : " + a.Text);
            byte[] bytes = Ftp.Daten("LIST");
            Cwd("/");
            if (bytes == null) throw new Exception("Ordner " + eltern + " konnte nicht gelesen werden");
            foreach (string z in Regex.Split(Ftp.UTF8.GetString(bytes), "\r?\n"))
            {
                string[] teile = H.Leer.Split(z, 9);
                if (teile.Length >= 9 && teile[8] == name) return true;
            }
            return false;
        }

        public string Text(string pfad)
        {
            byte[] b = Ftp.Daten("RETR " + pfad);
            if (b == null) return "";
            return Ftp.UTF8.GetString(b);
        }

        public byte[] Bytes(string pfad) { return Ftp.Daten("RETR " + pfad); }

        public bool Schreiben(string pfad, byte[] b) { return Ftp.Schreiben(pfad, b); }

        // Antwortet die Steuerverbindung sauber (Befehl und Antwort passen zusammen)?
        public bool Gesund()
        {
            try
            {
                if (Ftp.Befehl("NOOP").Code != 200) return false;
                if (Ftp.Befehl("PWD").Code != 257) return false;
                return Liste("/user") != null;
            }
            catch (AbbruchAusnahme) { throw; }
            catch { return false; }
        }
    }
}
