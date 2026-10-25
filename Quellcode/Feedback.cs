// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// Feedback.cs - NEU 1.0.5: freiwilliger Kompatibilitätsbericht.
// Hier wird NUR Text gebaut - kein Netzwerk, keine Übertragung. Der Bericht
// wird dem Nutzer im Fenster gezeigt; erst sein Klick auf "Auf GitHub öffnen"
// öffnet den Browser mit einem vorausgefüllten Issue, abgeschickt wird es
// erst dort vom Nutzer selbst. (Die Versionsprüfung beim Start ist davon
// getrennt und steht in Fenster.cs.)
// =====================================================================
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace PS5Cleanup
{
    // Was die Oberfläche über die Sitzung weiß (nichts davon wird gesendet)
    public class FeedbackDaten
    {
        public string Modell = "";            // z.B. "CFI-7021 B01Y", "" = unbekannt
        public string Reihe = "";             // z.B. "PS5 Pro"
        public string ModellQuelle = "";      // automatisch / von Hand / aus / leer
        public string Firmware = "";
        public int? DddRoh;                   // Rückgabewert der Laufwerksabfrage, null = keiner
        public string LaufwerkText = "";
        public string FtpServer = "";         // NEU 1.0.5 (Variante 2): wie sich der Server selbst nennt, "" = "Unbekannt"
        public string FtpDaten = "";          // EPSV / PASV, "" = unbekannt
        public int Port;                      // 0 = nicht verbunden
        public bool ScanGemacht;
        public bool ScanImTestmodus;
        public int[] AnzahlJeBereich = new int[8];
        public string M2 = "", Usb = "";
        public int ShadowMountSpiele = -1;    // -1 = unbekannt
        public bool GeloeschtInSitzung;
        public List<string> Fehler = new List<string>();
        public string Windows = "";
        public string[] PersoenlicheNamen = new string[0];   // Windows-Benutzer, PC-Name - werden entfernt
    }

    public static class Feedback
    {
        static string T(string de, string en) { return Sp.T(de, en); }
        public const string ISSUE_NEU = "https://github.com/xXBneutzerNameXx/PS5-Cleanup/issues/new";
        public const int MAX_LINK = 7000;       // GitHub verträgt nur begrenzt lange Links
        public static string Unbekannt { get { return T("Unbekannt", "Unknown"); } }
        static string U(string s) { return string.IsNullOrWhiteSpace(s) ? Unbekannt : s.Trim(); }

        // Entfernt, was nicht in einen öffentlichen Bericht gehört.
        public static string Bereinigen(string t, IEnumerable<string> namen)
        {
            if (string.IsNullOrEmpty(t)) return t ?? "";
            // Pfade: Ordner dürfen Leerzeichen enthalten, der letzte Teil nicht - so bleibt der Text danach stehen
            t = Regex.Replace(t, @"\\\\[^\\\s""'<>|;,]+\\(?:[^\\\r\n""'<>|;,]*\\)*[^\\\s""'<>|;,]*", T("[Pfad entfernt]", "[path removed]"));   // \\server\freigabe\...
            t = Regex.Replace(t, @"(?i)\b[a-z]:\\(?:[^\\\r\n""'<>|;,]*\\)*[^\\\s""'<>|;,]*", T("[Pfad entfernt]", "[path removed]"));          // C:\Users\...
            t = Regex.Replace(t, "\\b\\d{1,3}(\\.\\d{1,3}){3}(:\\d{1,5})?\\b", T("[IP entfernt]", "[IP removed]"));                  // 192.168.x.y[:port]
            t = Regex.Replace(t, "(/user/home/)[^/\\s]+", "$1" + T("[Benutzer]", "[user]"));                                           // PS5-Benutzerkennung
            t = Regex.Replace(t, "[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}", T("[E-Mail entfernt]", "[e-mail removed]"));
            if (namen != null)
                foreach (var n in namen.Where(x => x != null && x.Trim().Length >= 3).OrderByDescending(x => x.Length))
                    t = Regex.Replace(t, Regex.Escape(n.Trim()), T("[entfernt]", "[removed]"), RegexOptions.IgnoreCase);
            return t;
        }

        public static string Titel(FeedbackDaten d)
        {
            string m = d.Reihe != "" && d.Reihe != T("unbekannt", "unknown") ? d.Reihe : "";
            if (d.Modell != "") m = (m + " " + d.Modell).Trim();
            if (m == "") m = Unbekannt;
            return T("Kompatibilität: ", "Compatibility: ") + m + ", Firmware " + U(d.Firmware);
        }

        public static string Bericht(FeedbackDaten d)
        {
            var b = new StringBuilder();
            Action<string, string> z = (k, v) => b.Append("**" + k + ":** " + v + "\n");
            b.Append("### " + T("PS5 Cleanup – Kompatibilitätsbericht", "PS5 Cleanup – compatibility report") + "\n\n");
            b.Append(T("_Vom Programm vorbereitet. Bitte vor dem Absenden prüfen und bei Bedarf ändern._", "_Prepared by the program. Please check and edit before submitting._") + "\n\n");
            z("PS5 Cleanup", Kern.VERSION + " (" + T("Deutsch", "English") + ")");
            z("Windows", U(d.Windows));
            z(T("PS5-Modell", "PS5 model"), d.Modell == "" ? Unbekannt : d.Modell + (d.Reihe != "" ? " (" + d.Reihe + ")" : ""));
            z(T("Modellerkennung", "Model detection"), U(d.ModellQuelle));
            z("Firmware", U(d.Firmware));
            z(T("Laufwerksstatus", "Disc drive status"), U(d.LaufwerkText));
            z(T("Rückgabewert der Laufwerksabfrage", "Raw value of the disc drive query"), d.DddRoh.HasValue ? d.DddRoh.Value.ToString() : T("keiner (nicht abgefragt oder keine Antwort)", "none (not queried or no answer)"));
            z(T("FTP-Server", "FTP server"), U(d.FtpServer));
            z(T("FTP-Port", "FTP port"), d.Port > 0 ? d.Port.ToString() : Unbekannt);
            z(T("FTP-Datenverbindung", "FTP data connection"), U(d.FtpDaten));
            if (d.ScanGemacht)
            {
                z(T("Scan", "Scan"), (d.ScanImTestmodus ? T("ja, im Testmodus", "yes, in test mode") : T("ja", "yes")) + " – " + d.AnzahlJeBereich.Sum() + T(" Einträge gefunden", " entries found"));
                foreach (var k in Kern.ALLE_KAT) b.Append("- " + Kern.KatName[k] + ": " + d.AnzahlJeBereich[k] + "\n");
                b.Append("\n");   // Leerzeile: sonst rechnet GitHub die nächsten Zeilen noch zur Liste
            }
            else z(T("Scan", "Scan"), T("nicht durchgeführt", "not done"));
            z("M.2-SSD", U(d.M2));
            z(T("USB-Speicher", "USB storage"), U(d.Usb));
            z(T("ShadowMount-Spiele", "ShadowMount games"), d.ShadowMountSpiele >= 0 ? d.ShadowMountSpiele.ToString() : Unbekannt);
            z(T("In dieser Sitzung gelöscht", "Deleted in this session"), d.GeloeschtInSitzung ? T("ja", "yes") : T("nein", "no"));
            if (d.Fehler.Count == 0) z(T("Fehlermeldungen", "Error messages"), T("keine", "none"));
            else
            {
                b.Append("**" + T("Fehlermeldungen", "Error messages") + ":**\n```\n");
                foreach (var f in d.Fehler.Take(10)) b.Append(f.Length > 300 ? f.Substring(0, 300) + " ..." : f).Append("\n");
                if (d.Fehler.Count > 10) b.Append("... (" + (d.Fehler.Count - 10) + T(" weitere)", " more)") + "\n");
                b.Append("```\n");
            }
            b.Append("\n**" + T("Was hat funktioniert, was nicht?", "What worked, what did not?") + "**\n\n" + T("(hier eintragen)", "(write here)") + "\n");
            return Bereinigen(b.ToString(), d.PersoenlicheNamen);
        }

        // Link für ein vorausgefülltes GitHub-Issue. Ist der Bericht zu lang, kommt
        // nur ein kurzer Text in den Link - der volle Bericht geht in die Zwischenablage.
        public static string IssueLink(string titel, string text, out bool gekuerzt)
        {
            string l = ISSUE_NEU + "?title=" + Uri.EscapeDataString(titel) + "&body=" + Uri.EscapeDataString(text);
            gekuerzt = l.Length > MAX_LINK;
            if (!gekuerzt) return l;
            return ISSUE_NEU + "?title=" + Uri.EscapeDataString(titel) + "&body=" + Uri.EscapeDataString(T("Der vollständige Bericht liegt in der Zwischenablage – bitte hier einfügen (Strg+V) und vor dem Absenden prüfen.", "The full report is on the clipboard – please paste it here (Ctrl+V) and check it before submitting."));
        }

        // Fehlerzeilen aus dem Protokoll dieser Sitzung
        public static List<string> FehlerAus(string protokoll)
        {
            var l = new List<string>();
            foreach (var zl in (protokoll ?? "").Split(new[] { "\r\n", "\n" }, StringSplitOptions.RemoveEmptyEntries))
                if (Regex.IsMatch(zl, "FEHLER|ERROR|SICHERHEITSSTOPP|SAFETY STOP|verweigert|denies", RegexOptions.IgnoreCase)) l.Add(zl.Trim());
            return l;
        }
    }
}
