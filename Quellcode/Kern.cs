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
        public bool NurEinzeln;    // NEU 1.0.6: nur einzeln wählbar (nie über den ganzen Bereich); vollständig nur auf ausdrücklichen Wunsch
        public bool Jailbreak;     // NEU 1.0.6: möglicher Jailbreak-Einstieg - einzeln löschbar, die vollständige Bereinigung fragt
        public string SperrGrund;  // NEU 1.0.6: Grund einer Sperre (null = Game Compressor arbeitet daran)
        // Größe beim Löschen (für "Freigeräumt")
        public long Bytes; public int Dateien; public int Ordner; public bool HatGroesse;
        public int Ort;            // NEU 1.0.6: wo die Daten liegen (Kern.ORT_*) - steht vorne im Text, entscheidet bei der vollständigen Bereinigung
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
        public const string VERSION = "1.0.6 Fix";   // da_Chris86 29.10.2026: ohne "Beta"; 30.10.2026: korrigierte 1.0.6
        static string T(string de, string en) { return Sp.T(de, en); }
        public const int STANDARD_PORT = 2121;
        public const int ZWEIT_PORT = 2120;      // NEU 1.0.5: z.B. zftpd lauscht auf der Konsole auf 2120
        public const string BEHALTEN_APP = "PPSA01325";   // Astro's Playroom - seit 1.0.6 ein normales Spiel (da_Chris86 28.10.2026)
        // NEU 1.0.6 (da_Chris86 28.10.2026): Speicherorte. USB = USB-Platte als erweiterter Speicher, USB-Stick = normaler Datenträger
        public const int ORT_INTERN = 0, ORT_M2 = 1, ORT_USB = 2, ORT_STICK = 3;
        public const string M2 = "/mnt/ext1";
        public const string SYS_META = "/system_data/priv/appmeta";
        public const string SM_ORDNER = "/user/data/shadowmount";
        public const string INFO_DATEI = "/data/ps5info.txt";

        public Sitzung S;                                   // die Hauptverbindung
        public Action<string> Melde = delegate { };         // Protokoll
        public Action<int> Warte = delegate (int ms) { Thread.Sleep(ms); };
        volatile bool abbruch;
        public bool Abbruch { get { return abbruch; } set { abbruch = value; } }

        public List<string> ASTRO_IDS = new List<string>();     // 1.0.6: Astro ist ein normales Spiel - bleibt leer
        public List<string> ASTRO_NPWRS = new List<string>();

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
            // 1.0.6: die Astro-Sperren aus 1.0.5 sind weg (Astro ist ein normales Spiel)
            "^/user/appmeta/external$", "^/user/appmeta/addcont$",
            "^/user/home/[^/]+/savedata[^/]*/(NPXS|sce_)",
            "^" + M2 + "/user(/|$)", "^" + M2 + "/system_data(/|$)", "^" + M2 + "/lost\\+found(/|$)",
            "^" + USB_ERW + "/user(/|$)", "^" + USB_ERW + "/system_data(/|$)", "^" + USB_ERW + "/lost\\+found(/|$)",   // NEU 1.0.6: USB wie M.2
            "/app/NPXS", "/appmeta/NPXS", "/patch/NPXS", "/addcont/NPXS", "^/user/download/NPXS",
            "^/user/license/(act\\.dat|nobackup)", "^/user/system/share/(trophy|share_)",
            "^/user/catalog_downloader/[^/]+$" };
        static readonly string[] ERLAUBT = {
            // NEU 1.0.6 (Nutzer-Protokoll 28.10.2026, Sony-Ordner "push_resource"): unter appmeta und download nur Spielnummern
            "^/user/(app|patch|addcont)/[A-Za-z]{4}\\d{5}(/|$)", "^/user/appmeta/(addcont/|external/addcont/)?[A-Za-z]{4}\\d{5}(/|$)",
            "^/user/home/[^/]+/savedata/[^/]+", "^/user/home/[^/]+/savedata_prospero/[^/]+",
            "^/user/home/[^/]+/trophy/[^/]+", "^/user/home/[^/]+/trophy2/[^/]+",
            "^/user/home/[^/]+/webkit/shell/local$",
            "^/user/av_contents/[^/]+/[^/]+",
            "^/user/data/[^/]+", "^/user/download/[A-Za-z]{4}\\d{5}(/|$)",
            "^/user/_zu_loeschen_[^/]+", "^" + M2 + "/[^/]+",
            "^" + USB_ERW + "/[^/]+", "^/mnt/usb\\d+/[^/]+" };   // NEU 1.0.6: USB-Speicher wie M.2, USB-Stick (nur einzeln angeboten)

        public bool Erlaubt(string p)
        {
            if (p == null) return false;
            if (H.M(p, "(^|/)\\.\\.?(/|$)") || H.M(p, "//") || H.M(p, "\\s$") || H.Steuerzeichen(p) || !H.M(p, "^/")) return false;
            if (H.M(p, "./$") || H.M(p, "^/user/appmeta/(.+/)?NPXS")) return false;   // NEU 1.0.6: "/x/" umginge Sperren mit $; Sony-Apps auch in Unterordnern
            // Astro's Playroom: seit 1.0.6 ein normales Spiel (Liste leer)
            if (AstroPfad(p)) return false;
            // M.2- und USB-Verwaltung (USB NEU 1.0.6): nur einzelne Spiele/Updates, nie die Ordner selbst
            if (H.M(p, "^(" + M2 + "|" + USB_ERW + ")/user/(app|patch)/[^/]+$")) return !H.M(p, "/NPXS");
            if (H.M(p, "^(" + M2 + "|" + USB_ERW + ")/user/addcont/[A-Z]{4}\\d{5}(/[^/]+)?$")) return !H.M(p, "/NPXS");
            // Kachel-Infos der PS5: nur einzelne Spiel-/App-Kennungen, nie NPXS, external
            if (H.M(p, "^" + SYS_META + "/[A-Z]{4}\\d{5}$")) return !H.M(p, "/NPXS");
            // Merkzettel für Spiele auf externen USB-Platten
            if (H.M(p, "^" + SYS_META + "/external/[A-Z]{4}\\d{5}$")) return !H.M(p, "/NPXS");
            // Fake-Lizenzen des Jailbreaks (act.dat und nobackup/ = echte Lizenzen, nie)
            if (H.M(p, "^/user/license/fake[^/]+\\.(rif|idx)$")) return true;
            // Share-Einstellungen einzelner Spiele (trophy/ und die .mst-Datei nie)
            if (H.M(p, "^/user/system/share/[A-Z]{4}\\d{5}$")) return !H.M(p, "/NPXS");
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
                    else if (e.IstLink) S.Befehl("DELE " + p);          // nur die Verknüpfung, kein CHMOD (würde dem Ziel gelten)
                    else { S.Befehl("SITE CHMOD 777 " + p); S.Befehl("DELE " + p); }
                }
            }
            S.Befehl("RMD " + d);
        }

        // NEU 1.0.6 (27.10.2026, Codex-Prüfung 23, wie 1.1 kern_loeschen.c): Liegt in einem Ordner irgendetwas Geschütztes,
        // wird der Ordner gar nicht angefasst - auch kein CHMOD. Nicht lesbar = geschützt. Verknüpfungen werden nicht verfolgt.
        bool NachkommeGeschuetzt(string c)
        {
            return (!H.M(c, "^/user/av_contents/") && H.M(c, "/NPXS\\d{5}(/|$)")) || H.M(c, "^/user/home/[^/]+/savedata[^/]*/sce_") || H.M(c, "^/user/license/(act\\.dat|nobackup)")
                || H.M(c, "^/user/av_contents/content_tmp/sharevideo_[0-9A-Fa-f]+_hdr\\.webm$") || AstroPfad(c);
        }
        bool BaumHatGeschuetztes(string d, ref string fund, int tiefe)
        {
            if (tiefe > 64) { fund = d; return true; }
            List<Datei> l;
            try { l = S.Liste(d, true); }
            catch (AbbruchAusnahme) { throw; }
            catch { l = null; }
            if (l == null) { fund = d; return true; }
            foreach (var e in l)
            {
                string c = d + "/" + e.Name;
                // Codex-Prüfung 37: ein Spiel-Abbild eines Jailbreak-Einstiegs IN einem gewählten Ordner - nur einzeln löschbar
                if (NachkommeGeschuetzt(c) || JbAbbild(c)) { fund = c; return true; }
                if (e.IstOrdner && !e.IstLink && BaumHatGeschuetztes(c, ref fund, tiefe + 1)) return true;
            }
            return false;
        }

        int WAR_SCHON_WEG;
        bool Loeschen(string p)
        {
            // NEU 1.0.6: nicht sicher zugeordnete Spieldatei - weder sie noch ein Ordner darüber
            foreach (var sq in SperrQuellen)
                if (sq == p || sq.StartsWith(p + "/", StringComparison.Ordinal) || p.StartsWith(sq + "/", StringComparison.Ordinal)) { Melde(T("  SICHERHEITSSTOPP: ", "  SAFETY STOP: ") + p + T(" enthält eine nicht sicher zugeordnete Spieldatei (", " contains a game file that is not assigned for certain (") + sq + T(") - nichts gelöscht.", ") - nothing deleted.")); return false; }
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
            if (selbst == null) { Melde(T("  SICHERHEITSSTOPP: ", "  SAFETY STOP: ") + p + T(" steht nicht in der Ordnerliste - nichts gelöscht.", " is not in the folder listing - nothing deleted.")); return false; }
            // 27.10.2026 (Codex-Prüfung 24): auch für einzeln gewählte Dateien
            {
                string fund = NachkommeGeschuetzt(p) ? p : null;
                if (fund != null || (selbst.IstOrdner && BaumHatGeschuetztes(p, ref fund, 0)))
                { Melde(T("  SICHERHEITSSTOPP: ", "  SAFETY STOP: ") + p + T(" enthält Geschütztes (", " contains something protected (") + fund + T(") - nichts gelöscht.", ") - nothing deleted.")); return false; }
            }
            S.Befehl("SITE CHMOD 777 " + p);
            S.Befehl("DELE " + p);                           // falls Datei
            if (!S.Existiert(p)) return true;
            Freigeben(p);                                     // Ordner
            var ra = S.Befehl("RMDA " + p);
            if (S.Existiert(p))
            {
                // 27.10.2026 (Codex-Prüfung 24, wie 1.1): Ausweichweg Datei für Datei nur, wenn der Server RMDA gar nicht kennt (502)
                // 30.10.2026 (PS5-Test da_Chris86 09:22, Beleg Claude Arbeit\2026-10-30\zftpd\probe.txt): zftpd antwortet auf RMDA mit
                // "500 Unknown command" statt 502 - auch 500 heißt "Befehl unbekannt, nicht ausgeführt" -> Ausweichweg erlaubt
                if (ra.Code != 502 && ra.Code != 500) { Melde(T("  SICHERHEITSSTOPP: ", "  SAFETY STOP: ") + p + T(" ließ sich nicht sicher als Ganzes löschen - der Rest bleibt stehen.", " could not be deleted safely as a whole - the rest is kept.")); return false; }
                LoescheBaum(p);
            }
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
        // NEU 1.0.5: drei Zustände. Gelöscht wird nur, wenn ShadowMount bestätigt
        // angehalten ist oder nachweislich nicht läuft - sonst wird gefragt (Abbrechen vorbelegt).
        public enum SmZustand { Angehalten, LaeuftNicht, Unbekannt }
        public const string SM_MOUNTS = "/mnt/shadowmnt";   // dort bindet ShadowMount die Spiele ein
        public bool SM_ANGEHALTEN;
        public bool SM_STOP_GESENDET;                     // STOP wurde abgeholt (für die Meldung bei Abbruch)
        public string SM_GRUND = "";
        static int Abmeldungen(string log) { return Regex.Matches(log, "\\[SHUTDOWN\\] cleanup complete").Count; }

        // Sind gerade Spiele eingebunden? true = keine, false = ja, null = nicht feststellbar
        bool? KeineSpieleEingebunden()
        {
            List<Datei> l;
            try { l = S.Liste(SM_MOUNTS, true); }
            catch (AbbruchAusnahme) { throw; }
            catch { return null; }
            if (l == null) return true;
            foreach (var e in l)
            {
                if (!e.IstOrdner) continue;
                List<Datei> u;
                try { u = S.Liste(SM_MOUNTS + "/" + e.Name, true); }
                catch (AbbruchAusnahme) { throw; }
                catch { return null; }
                if (u != null && u.Count > 0) return false;
            }
            return true;
        }

        SmZustand Unbekannt(string grund)
        {
            SM_GRUND = grund;
            Melde("  " + grund);
            return SmZustand.Unbekannt;
        }

        public SmZustand ShadowMountAnhalten()
        {
            SM_ANGEHALTEN = false; SM_STOP_GESENDET = false; SM_GRUND = "";
            Melde(T("Halte ShadowMount an (sonst legt es die Kacheln sofort wieder an) ...", "Stopping ShadowMount (otherwise it recreates the tiles right away) ..."));
            try
            {
                var ordner = S.Liste(SM_ORDNER, true);
                if (ordner == null)
                {
                    var frei = KeineSpieleEingebunden();
                    if (frei == true) { Melde(T("  ShadowMount ist nicht eingerichtet und es sind keine Spiele eingebunden.", "  ShadowMount is not set up and no games are mounted.")); return SmZustand.LaeuftNicht; }
                    return Unbekannt(frei == false ? T("Es sind Spiele eingebunden, aber ShadowMount ist nicht erreichbar.", "Games are mounted, but ShadowMount cannot be reached.")
                                                   : T("Ob Spiele eingebunden sind, ließ sich nicht prüfen.", "Could not check whether games are mounted."));
                }
                // nur eine Abmeldung NACH dem Anhalten zählt (alte Zeilen im Protokoll nicht)
                int vorher = Abmeldungen(S.Text(SM_ORDNER + "/debug.log"));
                if (!S.Schreiben(SM_ORDNER + "/STOP", null))
                    return Unbekannt(T("Die Stopp-Datei für ShadowMount ließ sich nicht anlegen.", "Could not create the stop file for ShadowMount."));
                // Läuft es, holt es sich die STOP-Datei innerhalb von ~3 s selbst ab.
                bool abgeholt = false;
                DateTime ende = DateTime.Now.AddSeconds(8);
                while (DateTime.Now < ende)
                {
                    Warte(500);
                    if (!S.Existiert(SM_ORDNER + "/STOP")) { abgeholt = true; break; }
                }
                if (!abgeholt)
                {
                    S.Befehl("DELE " + SM_ORDNER + "/STOP");      // niemand hat sie abgeholt
                    var frei = KeineSpieleEingebunden();
                    if (frei == true) { Melde(T("  ShadowMount läuft nicht (keine Reaktion, keine Spiele eingebunden).", "  ShadowMount is not running (no response, no games mounted).")); return SmZustand.LaeuftNicht; }
                    return Unbekannt(frei == false ? T("ShadowMount reagiert nicht (evtl. älter als 1.7), aber Spiele sind eingebunden.", "ShadowMount does not respond (maybe older than 1.7), but games are mounted.")
                                                   : T("ShadowMount reagiert nicht und ob Spiele eingebunden sind, ließ sich nicht prüfen.", "ShadowMount does not respond and it could not be checked whether games are mounted."));
                }
                SM_STOP_GESENDET = true;
                // Bestätigung: Abmeldung im Protokoll oder alle Spiele ausgehängt (falls das Protokoll abgeschaltet ist)
                ende = DateTime.Now.AddSeconds(90);
                while (DateTime.Now < ende)
                {
                    Warte(2000);
                    bool abgemeldet = Abmeldungen(S.Text(SM_ORDNER + "/debug.log")) > vorher;
                    if (!abgemeldet && KeineSpieleEingebunden() == true) { Warte(3000); abgemeldet = KeineSpieleEingebunden() == true; }
                    if (abgemeldet) { Melde(T("  ShadowMount ist beendet.", "  ShadowMount has stopped.")); SM_ANGEHALTEN = true; return SmZustand.Angehalten; }
                }
                return Unbekannt(T("ShadowMount hat sich nach 90 s nicht bestätigt beendet.", "ShadowMount did not confirm it stopped within 90 s."));
            }
            catch (AbbruchAusnahme) { throw; }
            catch (Exception ex) { return Unbekannt(T("ShadowMount-Zustand nicht feststellbar: ", "ShadowMount state unknown: ") + ex.Message); }
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
            if (Regex.IsMatch(m, "^CFI-7[01]")) return "pro";   // 1.0.6: auch die zweite Pro-Baureihe CFI-71xx (Sep. 2025, PS5 Dev Wiki)
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

        // NEU 1.0.6 (da_Chris86 28.10.2026): Modellzeile "CFI-7021 · Europa · seit Nov. 2024" statt "CFI-7021 B01Y".
        // Region aus den letzten beiden Ziffern, Datum aus der Baureihe - Quelle: PS5 Dev Wiki (SKU Models).
        // Unbekannte Nummern: nur die Nummer. Gleich wie kern_modell_zeile (C-Kern).
        public static string ModellZeile(string m)
        {
            var t = Regex.Match((m ?? "").ToUpperInvariant(), "^\\s*(CFI-(\\d)(\\d)(\\d\\d)[A-Z]*)");
            if (!t.Success) return m ?? "";
            string nr = t.Groups[1].Value, region = null, seit = null;
            int reihe = int.Parse(t.Groups[2].Value) * 10 + int.Parse(t.Groups[3].Value), reg = int.Parse(t.Groups[4].Value);
            if (reihe == 70 || reihe == 71)   // PS5 Pro: eigene Regionsnummern; 71 = zweite Baureihe
            {
                switch (reg)
                {
                    case 0: region = "Japan"; break; case 2: region = T("Australien", "Australia"); break; case 14: region = T("Süd-/Mittelamerika", "South/Central America"); break;
                    case 19: region = T("USA/Kanada", "US/Canada"); break; case 20: region = T("Mexiko", "Mexico"); break; case 21: region = T("Europa", "Europe"); break;
                    case 22: region = T("Südostasien/Korea", "Southeast Asia/Korea"); break;
                }
                seit = reihe == 70 ? T("Nov. 2024", "Nov 2024") : T("Sep. 2025", "Sep 2025");
            }
            else
            {
                switch (reg)
                {
                    case 0: region = "Japan"; break; case 2: region = T("Australien", "Australia"); break; case 8: region = T("Osteuropa/Indien", "Eastern Europe/India"); break;
                    case 9: region = "China"; break; case 14: region = T("Süd-/Mittelamerika", "South/Central America"); break; case 15: region = T("USA/Kanada", "US/Canada"); break;
                    case 16: region = T("Europa", "Europe"); break; case 18: region = T("Südostasien/Korea", "Southeast Asia/Korea"); break;
                }
                if (reihe == 10) seit = T("Nov. 2020", "Nov 2020");
                else if (reihe == 11) seit = "2021";
                else if (reihe == 12) seit = "2022";
                else if (reihe == 20) seit = T("Nov. 2023", "Nov 2023");
            }
            string z = nr;
            if (region != null) z += " · " + region;
            if (seit != null) z += T(" · seit ", " · since ") + seit;
            // Sonderedition 30 Jahre PlayStation (Slim und Pro): Endung B30 statt B01 (PS5 Dev Wiki, da_Chris86 28.10.2026)
            if (Regex.IsMatch((m ?? "").ToUpperInvariant(), "^\\s*CFI-\\d{4}[A-Z]*\\s*B30")) z += " · 30th Anniversary Edition";
            return z;
        }

        // Klartext zum ausgelesenen Laufwerks-Status (nur lesend ermittelt).
        // NEU 1.0.6 (da_Chris86 28.10.2026): ohne Statuswerte - Titel vor dem Doppelpunkt, danach ein kurzer Satz.
        // NEU 1.0.5: Drei klar getrennte Aussagen - Kopplung bestätigt / keine
        // Kopplung / nicht sicher bestimmbar bzw. nicht auslesbar. Zuerst das Modell:
        // Bei der ersten PS5 sagt der Statuswert nichts aus (dort wird auch 0 gemeldet).
        // Belegt sind nur: 1 = gekoppeltes Laufwerk (PS5 Pro, eigene Messung), 2 = kein
        // gekoppeltes Laufwerk (Slim/Pro Digital) - siehe earthonion/ddd_status.
        // 6 wurde dort bei Slim-Konsolen mit ab Werk gekoppeltem Laufwerk beobachtet,
        // die Bezeichnung des Werts passt aber nicht dazu - darum keine feste Aussage.
        // Keine Einschränkung auf Modell oder Firmware: jeder gültige Wert wird angezeigt.
        public string LaufwerkText()
        {
            if (MODELL_TYP == "fat")
            {
                if (MODELL_DIGITAL) return T("Keine Kopplung: Digital Edition ohne Laufwerk (erste PS5).", "No pairing: Digital Edition without disc drive (original PS5).");
                return T("Keine Kopplung nötig: fest eingebautes Laufwerk (erste PS5).", "No pairing needed: built-in disc drive (original PS5).");
            }
            if (DDD_STATUS == null) return T("Nicht abgefragt: Konnte nicht gelesen werden.", "Not queried: could not be read.");
            int s = DDD_STATUS.Value;
            switch (s)
            {
                case 1: return T("Kopplung bestätigt: Laufwerk ist mit dieser Konsole gekoppelt.", "Pairing confirmed: the disc drive is paired with this console.");
                case 2: return T("Keine Kopplung: Kein Laufwerk gekoppelt.", "No pairing: no disc drive is paired.");
                case 0: case 3: case 4: case 5: case 6:
                    return T("Nicht sicher bestimmbar: Der Zustand ließ sich nicht eindeutig lesen.", "Not reliably determinable: the state could not be read unambiguously.");
            }
            return T("Nicht abgefragt: Konnte nicht gelesen werden.", "Not queried: could not be read.");
        }
        // NEU 1.0.5: nur für die Farbe der Anzeige - dieselbe Einteilung wie LaufwerkText()
        public enum KopplungsArt { Bestaetigt, Keine, Unklar, NichtAuslesbar }
        public KopplungsArt KopplungsStufe()
        {
            if (MODELL_TYP == "fat") return KopplungsArt.Keine;
            if (DDD_STATUS == null) return KopplungsArt.NichtAuslesbar;
            switch (DDD_STATUS.Value)
            {
                case 1: return KopplungsArt.Bestaetigt;
                case 2: return KopplungsArt.Keine;
                case 0: case 3: case 4: case 5: case 6: return KopplungsArt.Unklar;
            }
            return KopplungsArt.NichtAuslesbar;
        }
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
            // 1.0.6 Fix (30.10.2026): genau die Menünamen von Sony, ohne Nummer (die ändert sich mit der Firmware)
            T("3. Controller per USB anstecken: „Cache leeren und Datenbank neu erstellen“ → „Datenbank neu aufbauen“.", "3. Connect the controller via USB: 'Clear Cache and Rebuild Database' → 'Rebuild Database'."),
            T("Wichtig: NICHT 'PS5 zurücksetzen' oder 'Systemsoftware neu installieren' wählen!", "Important: do NOT choose 'Reset PS5' or 'Reinstall System Software'!") }; } }
    }
}
