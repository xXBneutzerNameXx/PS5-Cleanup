// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// Kern.cs - Schutzliste, Löschen, ShadowMount, Modell/Firmware.
// 1:1 nach ps5_kern.ps1 (Erlaubt, Ohne-Verknuepfung, Loeschen, ...).
// Die Suche steht in Suche.cs, Auswahl/Ausführen in Ausfuehren.cs.
// =====================================================================
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;

namespace PS5Cleanup
{
    public class Eintrag
    {
        public List<string> Pfade = new List<string>();
        public List<string> PfadeBasis = new List<string>();
        public int Kat;
        public string Text = "";
        public string TextBasis = "";
        public bool Wahl;
        public string Quelle = "";
        public bool Gesperrt;
        public string Gruppe;      // null = gehört zu keinem Spiel
        public string Teil;        // Spiel / Update / DLC
        public bool Paket;         // Trophäen-Liste eines noch installierten Spiels
        public bool NurLeer;       // leerer M.2-Ordner: wird nur entfernt, wenn er wirklich leer ist (RMD)
        public bool Kachel;        // betrifft eine Kachel im Menü -> ShadowMount vor dem Löschen anhalten
        public bool SmTipp;        // ShadowMount-Spiel, dessen Kachel mitgeht (Hinweis am Kästchen)
        // Größe beim Löschen (für "Freigeräumt")
        public long Bytes; public int Dateien; public int Ordner; public bool HatGroesse;
        public bool IstTeil(string t) { return Teil != null && Teil == t; }
    }

    public class Groesse
    {
        public long Bytes; public int Dateien; public int Ordner; public bool Gekappt;
        public bool Lesefehler;    // Verbindung evtl. aus dem Takt - nicht merken, Verbindung erneuern
        public void Plus(Groesse g) { Bytes += g.Bytes; Dateien += g.Dateien; Ordner += g.Ordner; if (g.Gekappt) Gekappt = true; if (g.Lesefehler) Lesefehler = true; }
    }

    public partial class Kern
    {
        public const string VERSION = "1.0.4 Beta";
        static string T(string de, string en) { return Sp.T(de, en); }
        public const int STANDARD_PORT = 2121;
        public const string BEHALTEN_APP = "PPSA01325";   // Astro's Playroom
        public const string M2 = "/mnt/ext1";
        public const string SYS_META = "/system_data/priv/appmeta";
        public const string SM_ORDNER = "/user/data/shadowmount";
        public const string INFO_DATEI = "/data/ps5info.txt";

        public Sitzung S;                                   // die Hauptverbindung
        public Action<string> Melde = delegate { };         // Protokoll
        public Action<int> Warte = delegate (int ms) { Thread.Sleep(ms); };
        volatile bool abbruch;
        public bool Abbruch { get { return abbruch; } set { abbruch = value; } }

        public List<string> ASTRO_IDS = new List<string> { BEHALTEN_APP };
        public List<string> ASTRO_NPWRS = new List<string> { "NPWR20188_00" };

        public bool Verbinden(string ip, int port)
        {
            if (S != null) S.Ftp.Schliessen();
            S = null;
            var f = Ftp.Verbinden(ip, port);
            if (f == null) return false;
            S = NeueSitzung(f);
            return true;
        }

        public Sitzung NeueSitzung(Ftp f)
        {
            var s = new Sitzung(f);
            s.Abbruch = delegate { return abbruch; };
            return s;
        }

        public void Trennen() { if (S != null) S.Ftp.Trennen(); }

        // -------------------------------------------------------------
        // Schutzliste
        // -------------------------------------------------------------
        // Gehört der Pfad zu Astro (App, Daten, Spielstand, Update, DLC, Trophäen-
        // fortschritt)? Eigene Aufnahmen aus Astro in der Mediengalerie zählen nicht dazu.
        public bool AstroPfad(string p)
        {
            if (!H.Like(p, "/user/av_contents/*"))
                foreach (var a in ASTRO_IDS) if (H.M(p, "/" + Regex.Escape(a) + "(/|$|[-_.])")) return true;
            foreach (var n in ASTRO_NPWRS) if (H.M(p, "^/user/home/[^/]+/.*/" + Regex.Escape(n) + "(/|$)")) return true;
            return false;
        }

        static readonly string[] GESPERRT = {
            "^/user/app/" + BEHALTEN_APP + "(/|$)", "^/user/appmeta/" + BEHALTEN_APP + "(/|$)",
            "^/user/patch/" + BEHALTEN_APP + "(/|$)", "^/user/addcont/" + BEHALTEN_APP + "(/|$)",
            "^/user/appmeta/external$", "^/user/appmeta/addcont$",
            "^/user/home/[^/]+/savedata[^/]*/" + BEHALTEN_APP + "(/|$)",
            "^/user/home/[^/]+/savedata[^/]*/(NPXS|sce_)",
            "^" + M2 + "/user(/|$)", "^" + M2 + "/system_data(/|$)", "^" + M2 + "/lost\\+found(/|$)",
            "/app/NPXS", "/appmeta/NPXS", "/patch/NPXS", "/addcont/NPXS", "^/user/download/NPXS",
            "^/user/license/(act\\.dat|nobackup)", "^/user/system/share/(trophy|share_)",
            "^/user/catalog_downloader/[^/]+$" };
        static readonly string[] ERLAUBT = {
            "^/user/app/[^/]+", "^/user/appmeta/[^/]+", "^/user/patch/[^/]+",
            "^/user/home/[^/]+/savedata/[^/]+", "^/user/home/[^/]+/savedata_prospero/[^/]+",
            "^/user/home/[^/]+/trophy/[^/]+", "^/user/home/[^/]+/trophy2/[^/]+",
            "^/user/home/[^/]+/webkit/shell/local$",
            "^/user/av_contents/[^/]+/[^/]+",
            "^/user/data/[^/]+", "^/user/download/[^/]+", "^/user/addcont/[^/]+",
            "^/user/_zu_loeschen_[^/]+", "^" + M2 + "/[^/]+" };

        public bool Erlaubt(string p)
        {
            if (p == null) return false;
            if (H.M(p, "(^|/)\\.\\.?(/|$)") || H.M(p, "//") || H.M(p, "\\s$") || H.Steuerzeichen(p) || !H.M(p, "^/")) return false;
            // Astro's Playroom: nie, egal wo und unter welcher Länderkennung
            if (AstroPfad(p)) return false;
            // M.2-Verwaltung: nur einzelne Spiele/Updates, nie die Ordner selbst
            if (H.M(p, "^" + M2 + "/user/(app|patch)/[^/]+$")) return !H.M(p, "/(NPXS|" + BEHALTEN_APP + ")");
            if (H.M(p, "^" + M2 + "/user/addcont/[A-Z]{4}\\d{5}(/[^/]+)?$")) return !H.M(p, "/(NPXS|" + BEHALTEN_APP + ")");
            // Kachel-Infos der PS5: nur einzelne Spiel-/App-Kennungen, nie NPXS, Astro, external
            if (H.M(p, "^" + SYS_META + "/[A-Z]{4}\\d{5}$")) return !H.M(p, "/NPXS") && !H.Gleich(p, SYS_META + "/" + BEHALTEN_APP);
            // Merkzettel für Spiele auf externen USB-Platten
            if (H.M(p, "^" + SYS_META + "/external/[A-Z]{4}\\d{5}$")) return !H.M(p, "/NPXS");
            // Fake-Lizenzen des Jailbreaks (act.dat und nobackup/ = echte Lizenzen, nie)
            if (H.M(p, "^/user/license/fake[^/]+\\.(rif|idx)$")) return true;
            // Share-Einstellungen einzelner Spiele (trophy/ und die .mst-Datei nie)
            if (H.M(p, "^/user/system/share/[A-Z]{4}\\d{5}$")) return !H.M(p, "/NPXS") && !H.Gleich(p, "/user/system/share/" + BEHALTEN_APP);
            // Store-Bildercache
            if (H.M(p, "^/user/catalog_downloader/(conceptmeta|appmeta)/[^/]+$")) return true;
            // temporäre Dateien: nur Inhalte, nie die Ordner selbst
            if (H.M(p, "^/user/(bgft/trash|savedata/temp|temp/gls/screenshot|temp/video_recording|temp/common_temp/telemetry)/[^/]+$")) return true;
            // Fehlerberichte: nur die einzelnen .json-Dateien
            if (H.M(p, "^/system_data/priv/error/history/[^/]+\\.json$")) return true;
            // Trophäen-Pakete der Spiele (Astro nie)
            var tm = H.Mt(p, "^/user/(trophy/conf|trophy2/nobackup/conf|np_uds/nobackup/conf)/(NPWR\\d{5}_\\d{2})$");
            if (tm.Success) return !H.Hat(ASTRO_NPWRS, tm.Groups[2].Value);
            // niemals: Systemordner, Konto, Lizenzen, Astro, Sony-Systemanwendungen
            foreach (var g in GESPERRT) if (H.M(p, g)) return false;
            foreach (var e in ERLAUBT) if (H.M(p, e)) return true;
            return false;
        }

        // Steckt in einem Ordner auf dem Weg eine Verknüpfung (Symlink)? Dann
        // könnte der Pfad ganz woanders hinzeigen (z.B. auf die M.2) - solche Pfade
        // werden nie gelöscht. (Der letzte Teil selbst darf eine Verknüpfung sein -
        // DELE entfernt dann nur die Verknüpfung.)
        Dictionary<string, List<Datei>> LINK_CACHE = new Dictionary<string, List<Datei>>(StringComparer.Ordinal);
        bool OhneVerknuepfung(string p)
        {
            string[] teile = p.Split('/');
            string weg = "";
            for (int i = 1; i < teile.Length - 1; i++)
            {
                string eltern = weg == "" ? "/" : weg;
                weg = weg + "/" + teile[i];
                if (!LINK_CACHE.ContainsKey(eltern)) LINK_CACHE[eltern] = S.Liste(eltern, true);
                var l = LINK_CACHE[eltern];
                if (l == null) return true;                 // gibt es nicht -> nichts zu löschen
                var e = l.FirstOrDefault(x => x.Name == teile[i]);
                // nicht gefunden, obwohl der Ordner darüber da ist: nur weitermachen,
                // wenn es den ganzen Pfad wirklich nicht gibt
                if (e == null) return !S.Existiert(p);
                if (e.IstLink || !e.IstOrdner) return false;
            }
            return true;
        }

        void Freigeben(string d)
        {
            S.Befehl("SITE CHMOD 777 " + d);
            var l = S.Liste(d);
            if (l == null) return;
            foreach (var e in l) if (e.IstOrdner) Freigeben(d + "/" + e.Name);
        }

        void LoescheBaum(string d)
        {
            var l = S.Liste(d);
            if (l != null)
            {
                foreach (var e in l)
                {
                    string p = d + "/" + e.Name;
                    if (e.IstOrdner) LoescheBaum(p);
                    else { S.Befehl("SITE CHMOD 777 " + p); S.Befehl("DELE " + p); }
                }
            }
            S.Befehl("RMD " + d);
        }

        int WAR_SCHON_WEG;
        bool Loeschen(string p)
        {
            if (!Erlaubt(p)) { Melde(T("  SICHERHEITSSTOPP: ", "  SAFETY STOP: ") + p + T(" ist gesperrt - nichts gelöscht.", " is protected - nothing deleted.")); return false; }
            if (!OhneVerknuepfung(p)) { Melde(T("  SICHERHEITSSTOPP: ", "  SAFETY STOP: ") + p + T(" liegt hinter einer Verknüpfung - nichts gelöscht.", " is behind a symlink - nothing deleted.")); return false; }
            if (!S.Existiert(p)) { WAR_SCHON_WEG++; return true; }
            // Ist der Eintrag selbst eine Verknüpfung: nur sie entfernen, nie ihr Ziel
            var el = S.Liste(H.Eltern(p), true);
            Datei selbst = el == null ? null : el.FirstOrDefault(x => x.Name == H.Blatt(p));
            if (selbst != null && selbst.IstLink)
            {
                S.Befehl("DELE " + p);
                return !S.Existiert(p);
            }
            S.Befehl("SITE CHMOD 777 " + p);
            S.Befehl("DELE " + p);                           // falls Datei
            if (!S.Existiert(p)) return true;
            Freigeben(p);                                     // Ordner
            S.Befehl("RMDA " + p);
            if (S.Existiert(p)) LoescheBaum(p);
            return !S.Existiert(p);
        }

        // Leeren Ordner entfernen, der als eigener Eintrag gewählt wurde: nur RMD -
        // RMD löscht nie Inhalt mit. Ist inzwischen etwas drin, schlägt es fehl.
        bool LeerenOrdnerLoeschen(string p)
        {
            if (!Erlaubt(p)) { Melde(T("  SICHERHEITSSTOPP: ", "  SAFETY STOP: ") + p + T(" ist gesperrt - nichts gelöscht.", " is protected - nothing deleted.")); return false; }
            if (!OhneVerknuepfung(p)) { Melde(T("  SICHERHEITSSTOPP: ", "  SAFETY STOP: ") + p + T(" liegt hinter einer Verknüpfung - nichts gelöscht.", " is behind a symlink - nothing deleted.")); return false; }
            if (!S.Existiert(p)) { WAR_SCHON_WEG++; return true; }
            var l = S.Liste(p, true);
            if (l == null || l.Count != 0) { Melde("  " + p + T(" ist nicht mehr leer - bleibt stehen.", " is no longer empty - kept.")); return false; }
            S.Befehl("RMD " + p);
            return !S.Existiert(p);
        }

        // Nur einen wirklich leeren Ordner entfernen (RMD löscht nie Inhalt mit)
        void LeerenOrdnerEntfernen(string o)
        {
            try
            {
                if (!Erlaubt(o)) return;
                var l = S.Liste(o);
                if (l == null || l.Count != 0) return;
                var a = S.Befehl("RMD " + o);
                if (a.Code < 400) Melde(T("  gelöscht   leerer Ordner ", "  deleted   empty folder ") + o);
            }
            catch { }
        }

        // -------------------------------------------------------------
        // ShadowMount anhalten
        // -------------------------------------------------------------
        bool ShadowMountLaeuft()
        {
            // Läuft es, holt es sich die STOP-Datei innerhalb von ~3 s selbst ab.
            if (S.Liste(SM_ORDNER) == null) return false;
            if (!S.Schreiben(SM_ORDNER + "/STOP", null)) return false;
            DateTime ende = DateTime.Now.AddSeconds(8);
            while (DateTime.Now < ende)
            {
                Warte(500);
                if (!S.Existiert(SM_ORDNER + "/STOP")) return true;
            }
            S.Befehl("DELE " + SM_ORDNER + "/STOP");          // niemand hat sie abgeholt
            return false;
        }
        public bool SM_ANGEHALTEN;
        static int Abmeldungen(string log) { return Regex.Matches(log, "\\[SHUTDOWN\\] cleanup complete").Count; }
        bool ShadowMountAnhalten()
        {
            SM_ANGEHALTEN = false;
            Melde(T("Halte ShadowMount an (sonst legt es die Kacheln sofort wieder an) ...", "Stopping ShadowMount (otherwise it recreates the tiles right away) ..."));
            // nur eine Abmeldung NACH dem Anhalten zählt (alte Zeilen im Protokoll nicht)
            int vorher = Abmeldungen(S.Text(SM_ORDNER + "/debug.log"));
            if (!ShadowMountLaeuft())
            {
                Melde(T("  ShadowMount läuft nicht (oder ist älter als 1.7 - dann vorher", "  ShadowMount is not running (or older than 1.7 - then stop it"));
                Melde(T("  von Hand beenden: PS5 neu starten und nur den FTP-Server laden).", "  by hand first: restart the PS5 and load only the FTP server)."));
                return true;
            }
            DateTime ende = DateTime.Now.AddSeconds(90);
            while (DateTime.Now < ende)
            {
                Warte(2000);
                if (Abmeldungen(S.Text(SM_ORDNER + "/debug.log")) > vorher) { Melde(T("  ShadowMount ist beendet.", "  ShadowMount has stopped.")); SM_ANGEHALTEN = true; return true; }
            }
            Melde(T("  ShadowMount hat sich nach 90 s nicht sauber beendet.", "  ShadowMount did not stop cleanly within 90 s."));
            return false;
        }

        // -------------------------------------------------------------
        // Modell / Firmware
        // -------------------------------------------------------------
        public string FirmwareLesen()
        {
            var m = H.Mt(S.Text("/system_ex/etc/index.dat"), "release:\\s*0x([0-9a-fA-F]{8})");
            if (!m.Success) return T("unbekannt", "unknown");
            string hex = m.Groups[1].Value;
            return hex.Substring(0, 2).TrimStart('0') + "." + hex.Substring(2, 2);
        }

        static bool PortOffen(string ip, int port)
        {
            try
            {
                var t = new TcpClient();
                var ar = t.BeginConnect(ip, port, null, null);
                bool ok = ar.AsyncWaitHandle.WaitOne(2000) && t.Connected;
                t.Close();
                return ok;
            }
            catch { return false; }
        }

        static bool HttpPost(string url, byte[] body)
        {
            try
            {
                var req = (HttpWebRequest)WebRequest.Create(url);
                req.Method = "POST";
                req.Timeout = 15000;
                req.Proxy = null;
                req.Headers.Add("X-PLDMGR-Request", "1");
                req.ContentType = "application/octet-stream";
                if (body == null) body = new byte[0];
                req.ContentLength = body.Length;
                using (var s = req.GetRequestStream()) s.Write(body, 0, body.Length);
                req.GetResponse().Close();
                return true;
            }
            catch { return false; }
        }

        // Kopplungs-Status des abnehmbaren Laufwerks aus dem Payload. null = noch nicht
        // gelesen, -1 = kein abnehmbares Laufwerk / nicht lesbar, 0..6 siehe LaufwerkText.
        public int? DDD_STATUS;
        public string MODELL_TYP = "";
        public bool MODELL_DIGITAL;

        string LiesInfo(string t)
        {
            var d = H.Mt(t, "PS5INFO_DDD=(-?\\d+)");
            if (d.Success) DDD_STATUS = int.Parse(d.Groups[1].Value);
            var m = H.Mt(t, "PS5INFO_MODEL=(CFI-[^\\r\\n]*)");
            if (m.Success) return m.Groups[1].Value.Trim();
            return "";
        }

        // Weg 1: Payload-Lader (Port 9021) - Antwort kommt direkt zurück
        string ModellUeberLader(string ip, byte[] elf)
        {
            try
            {
                var tcp = new TcpClient();
                var ar = tcp.BeginConnect(ip, 9021, null, null);
                if (!ar.AsyncWaitHandle.WaitOne(3000) || !tcp.Connected) { tcp.Close(); return ""; }
                tcp.EndConnect(ar);
                var st = tcp.GetStream();
                st.Write(elf, 0, elf.Length);
                st.ReadTimeout = 15000;
                var ms = new MemoryStream();
                var buf = new byte[4096];
                DateTime ende = DateTime.Now.AddSeconds(15);
                while (DateTime.Now < ende)
                {
                    int n;
                    try { n = st.Read(buf, 0, buf.Length); } catch { break; }
                    if (n <= 0) break;
                    ms.Write(buf, 0, n);
                    if (Ftp.UTF8.GetString(ms.ToArray()).Contains("PS5INFO_DDD=")) break;
                }
                tcp.Close();
                return LiesInfo(Ftp.UTF8.GetString(ms.ToArray()));
            }
            catch { }
            return "";
        }

        // Weg 2: Payload-Manager (Port 8084) - Antwort kommt als Datei, per FTP gelesen
        string ModellUeberManager(string ip, byte[] elf)
        {
            string basis = "http://" + ip + ":8084";
            HttpPost(basis + "/manage:delete?filename=ps5info.elf", null);
            if (!HttpPost(basis + "/manage:upload?filename=ps5info.elf", elf)) return "";
            string m = "";
            if (HttpPost(basis + "/loadpayload:ps5info.elf", null))
            {
                DateTime ende = DateTime.Now.AddSeconds(15);
                while (DateTime.Now < ende && m == "")
                {
                    Warte(700);
                    m = LiesInfo(S.Text(INFO_DATEI));
                }
            }
            HttpPost(basis + "/manage:delete?filename=ps5info.elf", null);
            return m;
        }

        public string ModellLesen(string ip, byte[] elf)
        {
            if (elf == null || elf.Length == 0) return "";
            DDD_STATUS = null;
            S.Befehl("DELE " + INFO_DATEI);                  // alte Antwort weg
            string m = "";
            if (PortOffen(ip, 9021)) m = ModellUeberLader(ip, elf);
            if (m == "" && PortOffen(ip, 8084)) m = ModellUeberManager(ip, elf);
            S.Befehl("DELE " + INFO_DATEI);                  // aufräumen
            return m;
        }

        // CFI-10xx/11xx/12xx = erste PS5, CFI-20xx/21xx = Slim, CFI-70xx = Pro.
        public static string ModellTyp(string m)
        {
            m = Regex.Replace(m ?? "", "\\s", "").ToUpperInvariant();
            if (Regex.IsMatch(m, "^CFI-1[012]")) return "fat";
            if (Regex.IsMatch(m, "^CFI-2[01]")) return "slim";
            if (Regex.IsMatch(m, "^CFI-70")) return "pro";
            return "";
        }
        public static bool ModellDigital(string m) { return Regex.IsMatch(Regex.Replace(m ?? "", "\\s", "").ToUpperInvariant(), "^CFI-\\d{4}B"); }
        public static string ModellReihe(string typ, bool digital)
        {
            switch (typ)
            {
                case "fat": return digital ? T("PS5 Digital Edition (erstes Modell)", "PS5 Digital Edition (original model)") : T("PS5 (erstes Modell)", "PS5 (original model)");
                case "slim": return digital ? "PS5 Slim Digital Edition" : "PS5 Slim";
                case "pro": return "PS5 Pro";
            }
            return T("unbekannt", "unknown");
        }

        // Klartext zum ausgelesenen Laufwerks-Status (nur lesend ermittelt).
        public string LaufwerkText()
        {
            switch (DDD_STATUS ?? -99)
            {
                case 0: return T("Laufwerk ist mit dieser Konsole gekoppelt und einsatzbereit.", "Disc drive is paired with this console and ready.");
                case 1: return T("Laufwerk ist mit dieser Konsole gekoppelt (nachträglich) und einsatzbereit.", "Disc drive is paired with this console (paired later) and ready.");
                case 2: return T("Kein gekoppeltes Laufwerk (keins angeschlossen oder nicht gekoppelt).", "No paired disc drive (none attached or not paired).");
                case 3: return T("Laufwerks-Kopplung ist noch nicht abgeschlossen.", "Disc drive pairing is not finished yet.");
                case 4: return T("Laufwerks-Entkopplung ist noch nicht abgeschlossen.", "Disc drive unpairing is not finished yet.");
                case 5: return T("Laufwerk ist ab Werk mit dieser Konsole gekoppelt und einsatzbereit.", "Disc drive is factory-paired with this console and ready.");
                case 6: return T("Hinweis: Laufwerk ist ab Werk mit einer ANDEREN Konsole gekoppelt - an dieser arbeitet es nicht als Disc-Laufwerk.", "Note: the disc drive is factory-paired with ANOTHER console - it will not work as a disc drive on this one.");
            }
            if (MODELL_TYP == "fat")
            {
                if (MODELL_DIGITAL) return T("Kein Laufwerk (Digital Edition) - eine Kopplung gibt es nicht.", "No disc drive (Digital Edition) - there is no pairing.");
                return T("Fest eingebautes Laufwerk (erste PS5) - keine Kopplung nötig.", "Built-in disc drive (original PS5) - no pairing needed.");
            }
            return T("Laufwerks-Status nicht auslesbar (kein Laufwerk oder Abfrage nicht möglich).", "Disc drive status not readable (no drive attached or query not possible).");
        }
        public bool KopplungOk() { return DDD_STATUS == null || new[] { -1, 0, 1, 2, 5 }.Contains(DDD_STATUS.Value); }
        public string KopplungsText()
        {
            string l = LaufwerkText();
            return l + T(" Das Programm fasst die Kopplung nie an.", " The program never touches the pairing.");
        }

        // Überschrift, nummerierte Schritte, zuletzt die Warnung (das Fertig-Fenster setzt sie so zusammen)
        public static string[] SCHLUSS_TEXT { get { return new[] {
            T("Letzter Schritt an der PS5, damit sie alles sauber neu einliest:", "Last step on the PS5 so it re-reads everything cleanly:"),
            T("1. PS5 ausschalten.", "1. Turn off the PS5."),
            T("2. Einschaltknopf gedrückt halten, bis es zum zweiten Mal piept.", "2. Hold the power button until it beeps a second time."),
            T("3. Controller per USB anstecken und 'Datenbank neu erstellen' wählen.", "3. Connect the controller via USB and choose 'Rebuild Database'."),
            T("Wichtig: NICHT 'PS5 zurücksetzen' oder 'Systemsoftware neu installieren' wählen!", "Important: do NOT choose 'Reset PS5' or 'Reinstall System Software'!") }; } }
    }
}
