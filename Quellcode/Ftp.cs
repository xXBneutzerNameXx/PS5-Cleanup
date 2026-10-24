// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// Ftp.cs - kleiner FTP-Client für PS5 Cleanup (eine Steuerverbindung,
// UTF-8, EPSV). 1:1 nach ps5_kern.ps1 (Ftp-Zeile ... Datei-Schreiben).
// =====================================================================
using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Text.RegularExpressions;

namespace PS5Cleanup
{
    public class FtpAntwort
    {
        public int Code;
        public string Text;
    }

    // Ein Eintrag einer Ordnerliste
    public class Datei
    {
        public string Name;
        public bool IstOrdner;
        public bool IstLink;
        public long Groesse;
    }

    // Wird geworfen, wenn der Nutzer "Abbrechen" gedrückt hat (nur an sicheren
    // Stellen geprüft - nie mitten in einem FTP-Befehl).
    public class AbbruchAusnahme : Exception
    {
        public AbbruchAusnahme() : base(Sp.T("Abgebrochen.", "Cancelled.")) { }
    }

    public class Ftp
    {
        public static readonly UTF8Encoding UTF8 = new UTF8Encoding(false);
        public string Ip;
        public int Port;
        public int Timeout = 30000;          // ms, beim Löschen höher (große Dateien)
        TcpClient tcp;
        NetworkStream st;
        readonly List<byte> puffer = new List<byte>();
        // wird aufgerufen, solange auf die PS5 gewartet wird (ms seit Beginn)
        public Action<int> Wartet;

        public bool Verbunden { get { return tcp != null; } }

        public static Ftp Verbinden(string ip, int port)
        {
            var f = new Ftp();
            f.Ip = ip; f.Port = port;
            var t = new TcpClient();
            try
            {
                var ar = t.BeginConnect(ip, port, null, null);
                if (!ar.AsyncWaitHandle.WaitOne(5000)) { t.Close(); return null; }
                t.EndConnect(ar);
            }
            catch { try { t.Close(); } catch { } return null; }
            t.ReceiveTimeout = 30000;
            f.tcp = t; f.st = t.GetStream();
            try
            {
                f.Antwort();                                   // 220 Begrüßung
                var a = f.Befehl("USER anonymous");
                if (a.Code == 331) a = f.Befehl("PASS anonymous");
                if (a.Code >= 400) { f.Schliessen(); return null; }
                f.Befehl("TYPE I");
            }
            catch { f.Schliessen(); return null; }
            return f;
        }

        // Schließt auch eine gerade laufende Datenverbindung - ein wartender Faden
        // kommt dadurch sofort frei.
        public void Schliessen()
        {
            try { if (tcp != null) tcp.Close(); } catch { }
            try { var d = daten; if (d != null) d.Close(); } catch { }
        }
        volatile TcpClient daten;
        TcpClient DatenVerbinden(int dport)
        {
            var d = new TcpClient();
            daten = d;
            var ar = d.BeginConnect(Ip, dport, null, null);
            if (!ar.AsyncWaitHandle.WaitOne(15000)) { d.Close(); throw new IOException(Sp.T("Datenverbindung zur PS5 kommt nicht zustande", "Data connection to the PS5 could not be established")); }
            try { d.EndConnect(ar); } catch { d.Close(); throw; }
            return d;
        }

        public void Trennen()
        {
            try { Befehl("QUIT"); } catch { }
            Schliessen();
        }

        // Eine Zeile lesen; solange nichts da ist, warten (mit Zeitlimit) und
        // erkennen, wenn die PS5 die Verbindung geschlossen hat.
        string Zeile()
        {
            DateTime start = DateTime.Now;
            byte[] buf = new byte[8192];
            while (true)
            {
                int i = puffer.IndexOf((byte)10);
                if (i >= 0)
                {
                    byte[] roh = puffer.GetRange(0, i).ToArray();
                    puffer.RemoveRange(0, i + 1);
                    int len = roh.Length; if (len > 0 && roh[len - 1] == 13) len--;
                    return UTF8.GetString(roh, 0, len);
                }
                if (st.DataAvailable)
                {
                    int n = st.Read(buf, 0, buf.Length);
                    if (n <= 0) throw new IOException(Sp.T("FTP-Verbindung unterbrochen", "FTP connection interrupted"));
                    for (int k = 0; k < n; k++) puffer.Add(buf[k]);
                    start = DateTime.Now;
                    continue;
                }
                if (!tcp.Connected) throw new IOException(Sp.T("FTP-Verbindung unterbrochen", "FTP connection interrupted"));
                double ms = (DateTime.Now - start).TotalMilliseconds;
                if (ms > Timeout) throw new IOException(Sp.T("Die PS5 antwortet nicht (Zeitüberschreitung)", "The PS5 is not responding (timeout)"));
                if (Wartet != null) Wartet((int)ms);
                if (tcp.Client.Poll(20000, SelectMode.SelectRead) && tcp.Client.Available == 0 && !st.DataAvailable)
                    throw new IOException(Sp.T("FTP-Verbindung unterbrochen", "FTP connection interrupted"));
            }
        }

        public FtpAntwort Antwort()
        {
            string zeile = Zeile();
            string text = zeile;
            if (zeile.Length >= 4 && zeile[3] == '-')
            {
                string code = zeile.Substring(0, 3);
                while (true)
                {
                    zeile = Zeile();
                    text += "\n" + zeile;
                    if (zeile.StartsWith(code + " ", StringComparison.Ordinal)) break;
                }
            }
            if (Wartet != null) Wartet(0);
            int c;
            if (text.Length < 3 || !int.TryParse(text.Substring(0, 3), out c)) throw new IOException(Sp.T("Unverständliche Antwort der PS5: ", "Unexpected answer from the PS5: ") + text);
            return new FtpAntwort { Code = c, Text = text };
        }

        public FtpAntwort Befehl(string cmd)
        {
            // Steuerzeichen (z.B. ein Zeilenumbruch in einem Dateinamen) würden einen
            // zweiten, ungewollten Befehl erzeugen - nie senden.
            if (Regex.IsMatch(cmd, "[\\x00-\\x1f\\x7f]"))
                throw new IOException(Sp.T("Ungültiges Zeichen im FTP-Befehl: ", "Invalid character in FTP command: ") + Regex.Replace(cmd, "[\\x00-\\x1f\\x7f]", "?"));
            byte[] b = UTF8.GetBytes(cmd + "\r\n");
            st.Write(b, 0, b.Length);
            st.Flush();
            return Antwort();
        }

        int EpsvPort()
        {
            var a = Befehl("EPSV");
            if (a.Code != 229) return -1;
            var m = Regex.Match(a.Text, "\\|\\|\\|(\\d+)\\|");
            if (!m.Success) return -1;
            return int.Parse(m.Groups[1].Value);
        }

        // Datenverbindung (EPSV): liefert die Bytes oder null bei Fehler
        public byte[] Daten(string cmd)
        {
            int dport = EpsvPort();
            if (dport < 0) return null;
            var d = DatenVerbinden(dport);
            d.ReceiveTimeout = 30000;
            var a = Befehl(cmd);
            if (a.Code >= 400) { d.Close(); return null; }
            var ms = new MemoryStream();
            d.GetStream().CopyTo(ms);
            d.Close();
            a = Antwort();                                     // 226 = vollständig übertragen
            if (a.Code >= 400) return null;                    // abgebrochen - Inhalt unvollständig
            return ms.ToArray();
        }

        // Kleine Datei auf die PS5 schreiben (STOR)
        public bool Schreiben(string pfad, byte[] bytes)
        {
            int dport = EpsvPort();
            if (dport < 0) return false;
            var d = DatenVerbinden(dport);
            var a = Befehl("STOR " + pfad);
            if (a.Code >= 400) { d.Close(); return false; }
            if (bytes == null) bytes = new byte[0];
            var s = d.GetStream(); s.Write(bytes, 0, bytes.Length); s.Flush(); d.Close();
            a = Antwort();                                     // 226
            return a.Code < 400;
        }
    }
}
