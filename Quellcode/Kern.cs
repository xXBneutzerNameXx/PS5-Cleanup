// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// Kern.cs - Schutzliste, Löschen, ShadowMount, Modell/Firmware.
// 1:1 nach ps5_kern.ps1 (Erlaubt, Ohne-Verknuepfung, Loeschen, ...).
// Die Suche steht in Suche.cs, Auswahl/Ausführen in Ausfuehren.cs.
// =====================================================================
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
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

    // NEU 1.0.7 (da_Chris86 31.10.2026): installiertes PS4-Spiel (Ordner CUSAxxxxx unter app auf interner SSD, M.2 oder USB-Speicher).
    // Wird bei der Suche nur lesend ermittelt (Ordnerlisten). Seine Daten sind beim Löschen geschützt, solange das Spiel nicht selbst
    // im selben Lauf gelöscht wird (Kern.Ps4Zuordnung).
    public class Ps4Spiel
    {
        public string Id = "";            // CUSAxxxxx
        public string Name = "";          // Name wie in der Liste, sonst die Kennung
        public int Ort;                   // Kern.ORT_INTERN, ORT_M2 oder ORT_USB
        public string Basis = "";         // "/user", "/mnt/ext1/user" oder "/mnt/ext0/user"
        public bool AppGelesen;           // der App-Ordner ließ sich lesen
        public bool AppPkg; public long AppPkgBytes;
        public bool Abbild;               // ShadowMount-Kachel (mount.lnk / mount_img.lnk) statt installierter Daten
        public bool Patch; public bool PatchPkg; public long PatchPkgBytes;
        public List<string> Addcont = new List<string>();    // vollständige Pfade der Zusatzinhalt-Ordner
        public List<string> Lizenzen = new List<string>();   // vollständige Pfade der Lizenzdateien (.rif/.idx)
        public int FakeLizenzen, EchteLizenzen;
        public string AppPfad { get { return Basis + "/app/" + Id; } }
        public string PatchPfad { get { return Basis + "/patch/" + Id; } }
        public string Schluessel { get { return Basis + "|" + Id; } }   // gleich wie Eintrag.Gruppe des Spiels
    }
    // NEU 1.0.7: Ergebnis der Zuordnung eines Pfads zu einem installierten PS4-Spiel (null = gehört zu keinem)
    public class Ps4Treffer { public Ps4Spiel Spiel; public bool Erlaubt; public bool Unklar; }
    // NEU 1.0.7: Ergebnis der SQLite-Kopfprüfung einer gesicherten Datenbank
    public class DbPruefung { public bool Ok; public string Grund = ""; public int SeitenGroesse; public long Seiten; public bool KopfGroesseGueltig; }
    // NEU 1.0.7 (da_Chris86 31.10.2026): Ergebnis der SQLite-Prüfung einer Rohkopie (Arbeitskopie -> integrity_check -> geprüfte Kopie)
    public class SqlitePruefung
    {
        public bool Ok; public string Grund = ""; public string Hinweis = "";
        public bool MitWal;                 // die Rohkopie hatte eine -wal-Datei (ihr Inhalt steckt in der geprüften Kopie)
        public string ArbeitOrdner = "";    // temporärer Arbeitsordner (wird am Ende entfernt)
        public long Bytes; public string Sha = "";   // geprüfte Kopie
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
            DbBereitschaftSetzen(DbStand.Ungeprueft, "");   // NEU 1.0.7: neue Verbindung - die Datenbank wird nach dem Nachschauen neu geprüft
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
            // NEU 1.0.7 (da_Chris86 31.10.2026): PS4-Spieleschutz - direkt vor dem Löschen neu geprüft. Geschützt zählt nicht als
            // Fehler (eigener Zähler PS4_GESCHUETZT, Zeile im Protokoll); der Rest des Eintrags läuft weiter.
            if (Ps4Schutz(p, false)) return true;
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
            if (Ps4Schutz(p, false)) return true;   // NEU 1.0.7 (da_Chris86 31.10.2026): PS4-Spieleschutz wie in Loeschen()
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
                if (Ps4Schutz(o, true)) return;   // NEU 1.0.7 (da_Chris86 31.10.2026): Ordner eines installierten PS4-Spiels bleiben (ohne Protokollzeile - nur Aufräumen)
                if (!Erlaubt(o)) return;
                var l = S.Liste(o);
                if (l == null || l.Count != 0) return;
                var a = S.Befehl("RMD " + o);
                if (a.Code < 400) Melde(T("  gelöscht   leerer Ordner ", "  deleted   empty folder ") + o);
            }
            catch { }
        }

        // -------------------------------------------------------------
        // NEU 1.0.7 (da_Chris86 31.10.2026): PS4-Spieleschutz
        // -------------------------------------------------------------
        // Anlass (PS5 Pro FW 12.00, 31.10.2026): Nach "Datenbank neu aufbauen" waren app.pkg/patch.pkg der PS4-FPKG-Spiele auf der
        // internen SSD weg. Darum: Daten eines installierten PS4-Spiels werden nur gelöscht, wenn genau dieses Spiel im selben Lauf
        // selbst gelöscht wird. Unklare Zuordnung = geschützt. Lizenzen anderer Spiele werden nie angefasst.
        public List<Ps4Spiel> PS4_SPIELE = new List<Ps4Spiel>();   // von der letzten Suche (nur gelesen)
        public int PS4_GESCHUETZT;                                   // geschützte Pfade im laufenden Löschlauf
        HashSet<string> PS4_LOESCHT;                                 // Installationen (Basis|Kennung), die in diesem Lauf mitgelöscht werden
        string LOESCH_TEXT = "";                                     // Eintrag, der gerade gelöscht wird (nur fürs Protokoll)
        static readonly string[] PS4_BASEN = { "/user", M2 + "/user", USB_ERW + "/user" };
        const string PS4_ID = "CUSA\\d{5}";

        // alle CUSA-Kennungen in einem Pfad (großzügig erkannt - lieber einmal zu viel geschützt)
        static List<string> Ps4Kennungen(string s)
        {
            var l = new List<string>();
            foreach (Match m in Regex.Matches(s ?? "", "(" + PS4_ID + ")(?![0-9])", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant))
            {
                string id = m.Groups[1].Value.ToUpperInvariant();
                if (!l.Contains(id)) l.Add(id);
            }
            return l;
        }

        // Pfade, die sicher zu einer Installation gehören (für die Prüfung "liegt darin ein installiertes Spiel?")
        static List<string> Ps4Pfade(Ps4Spiel g)
        {
            var l = new List<string> { g.AppPfad, g.PatchPfad, "/user/appmeta/" + g.Id, "/user/appmeta/addcont/" + g.Id, "/user/appmeta/external/addcont/" + g.Id,
                                       SYS_META + "/" + g.Id, SYS_META + "/external/" + g.Id };
            l.AddRange(g.Addcont); l.AddRange(g.Lizenzen);
            return l;
        }

        // Ist f der Eintrag des Spiels selbst? Teil "Spiel" seiner Gruppe - oder ein Eintrag, der den App-Ordner genau dieser
        // Installation löscht (z. B. Kachel eines ShadowMount-Spiels, Tool-Kachel, Jailbreak-Einstieg).
        public static bool Ps4SpielEintrag(Eintrag f, Ps4Spiel g)
        {
            if (f == null || g == null) return false;
            if (f.IstTeil("Spiel") && H.Gleich(f.Gruppe, g.Schluessel)) return true;
            return f.Pfade.Any(p => H.Gleich(p, g.AppPfad));
        }
        // Installationen, deren Spiel-Eintrag gewählt ist (Schlüssel Basis|Kennung)
        public static HashSet<string> Ps4Mitgeloescht(IEnumerable<Eintrag> gewaehlt, List<Ps4Spiel> spiele)
        {
            var s = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            if (gewaehlt == null || spiele == null) return s;
            foreach (var f in gewaehlt)
            {
                if (f == null || !f.Wahl) continue;
                foreach (var g in spiele) if (Ps4SpielEintrag(f, g)) s.Add(g.Schluessel);
            }
            return s;
        }

        // Gehört p zu einem installierten PS4-Spiel? null = nein (es gelten nur die bisherigen Regeln).
        // Sonst: Erlaubt = das Spiel (bei gemeinsamen Teilen: jede Installation dieser Kennung) wird in diesem Lauf mitgelöscht.
        // Reine Prüfung ohne Verbindung - der Selbsttest ruft sie direkt auf.
        public static Ps4Treffer Ps4Zuordnung(string p, List<Ps4Spiel> spiele, ICollection<string> loescht)
        {
            if (string.IsNullOrEmpty(p) || spiele == null || spiele.Count == 0) return null;
            Func<Ps4Spiel, bool> geht = g => loescht != null && loescht.Contains(g.Schluessel);
            // 1. Daten einer Installation: <Basis>/app/<ID>, <Basis>/patch/<ID>, <Basis>/addcont/<ID>... (intern, M.2, USB-Speicher)
            foreach (var b in PS4_BASEN)
            {
                var m = H.Mt(p, "^" + Regex.Escape(b) + "/(app|patch)/(" + PS4_ID + ")(/|$)");
                if (!m.Success) m = H.Mt(p, "^" + Regex.Escape(b) + "/(addcont)/(" + PS4_ID + ")(?![0-9])[^/]*(/|$)");
                if (!m.Success) continue;
                string id = m.Groups[2].Value;
                var alle = spiele.Where(g => H.Gleich(g.Id, id)).ToList();
                if (alle.Count == 0) return null;                      // Kennung nicht installiert: Rest eines gelöschten Spiels
                var hier = alle.FirstOrDefault(g => H.Gleich(g.Basis, b));
                if (hier != null) return new Ps4Treffer { Spiel = hier, Erlaubt = geht(hier) };
                // installiert, aber auf einem anderen Speicher - zu welcher Installation das gehört, ist unklar: geschützt
                return new Ps4Treffer { Spiel = alle[0], Unklar = true };
            }
            var ids = Ps4Kennungen(p).Where(i => spiele.Any(g => H.Gleich(g.Id, i))).ToList();
            // 2. Gemeinsame Teile aller Installationen einer Kennung: Kachel-Infos, Lizenzen (*<ID>*.rif/.idx), PlayGo
            bool gemeinsam = H.M(p, "^/user/appmeta/(addcont/|external/addcont/)?" + PS4_ID + "(?![0-9])[^/]*(/|$)")
                          || H.M(p, "^" + SYS_META + "/(external/)?" + PS4_ID + "(?![0-9])[^/]*(/|$)")
                          || H.M(p, "^/user/license/") || H.M(p, "^/user/playgo/");
            if (gemeinsam && ids.Count > 0)
            {
                foreach (var i in ids)
                    foreach (var g in spiele.Where(x => H.Gleich(x.Id, i)))
                        if (!geht(g)) return new Ps4Treffer { Spiel = g, Unklar = ids.Count > 1 };
                return new Ps4Treffer { Spiel = spiele.First(x => H.Gleich(x.Id, ids[0])), Erlaubt = true };
            }
            // 3. Ordner, in dem Daten installierter Spiele liegen (z. B. /mnt/ext1/user) - nur, wenn alle diese Spiele mitgehen
            string vor = p.TrimEnd('/') + "/";
            Ps4Spiel drin = null;
            foreach (var g in spiele)
            {
                if (!Ps4Pfade(g).Any(q => q.StartsWith(vor, StringComparison.OrdinalIgnoreCase))) continue;
                if (!geht(g)) return new Ps4Treffer { Spiel = g };
                if (drin == null) drin = g;
            }
            if (drin != null) return new Ps4Treffer { Spiel = drin, Erlaubt = true };
            // 4. Kennung eines installierten PS4-Spiels im Spiel-/Lizenz-Bereich, aber kein bekannter Aufbau: unklar, geschützt
            if (ids.Count > 0 && H.M(p, "^(/user/(app|patch|addcont|appmeta|license|playgo)(/|$)|" + SYS_META + "(/|$)|/mnt/ext[01]/user(/|$))"))
                return new Ps4Treffer { Spiel = spiele.First(x => H.Gleich(x.Id, ids[0])), Unklar = true };
            return null;
        }

        // true = geschützt, nicht löschen. still = ohne Zählung und Protokoll (Aufräumen leerer Ordner)
        bool Ps4Schutz(string p, bool still)
        {
            var z = Ps4Zuordnung(p, PS4_SPIELE, PS4_LOESCHT);
            if (z == null || z.Erlaubt) return false;
            if (still) return true;
            PS4_GESCHUETZT++;
            string text = (LOESCH_TEXT != "" ? LOESCH_TEXT + ": " : "") + p;
            string name = z.Spiel.Name != "" && !H.Gleich(z.Spiel.Name, z.Spiel.Id) ? z.Spiel.Name + " " : "";
            Melde(T("  geschützt   ", "  protected   ") + text + T(" (gehört zum installierten PS4-Spiel ", " (belongs to the installed PS4 game ") + name + z.Spiel.Id
                  + (z.Unklar ? T(" - Zuordnung unklar", " - assignment unclear") : "") + ")");
            return true;
        }

        // Protokollzeilen zu den erkannten PS4-Spielen (nach der Suche)
        public List<string> Ps4Protokoll()
        {
            var l = new List<string>();
            if (PS4_SPIELE.Count == 0) { l.Add(T("Installierte PS4-Spiele: keine erkannt.", "Installed PS4 games: none detected.")); return l; }
            l.Add(T("Installierte PS4-Spiele: ", "Installed PS4 games: ") + PS4_SPIELE.Count + " (" + T("intern ", "internal ") + PS4_SPIELE.Count(g => g.Ort == ORT_INTERN) +
                  ", M.2 " + PS4_SPIELE.Count(g => g.Ort == ORT_M2) + ", USB " + PS4_SPIELE.Count(g => g.Ort == ORT_USB) +
                  T(") - ihre Daten sind geschützt, solange das Spiel nicht selbst gelöscht wird:", ") - their data is protected unless the game itself is deleted:"));
            foreach (var g in PS4_SPIELE)
            {
                string z = "   " + (H.Gleich(g.Name, g.Id) ? g.Id : g.Name + " (" + g.Id + ")") + " – " + OrtName(g.Ort) + " – ";
                if (g.AppPkg) z += "app.pkg " + GroesseText(g.AppPkgBytes);
                else if (g.Abbild) z += T("ShadowMount-Kachel (kein app.pkg)", "ShadowMount tile (no app.pkg)");
                else z += g.AppGelesen ? T("kein app.pkg", "no app.pkg") : T("Ordner nicht lesbar", "folder not readable");
                if (g.Patch) z += " – Update" + (g.PatchPkg ? " (patch.pkg " + GroesseText(g.PatchPkgBytes) + ")" : T(" (ohne patch.pkg)", " (no patch.pkg)"));
                if (g.Addcont.Count > 0) z += " – " + g.Addcont.Count + T(" Zusatzinhalt-Ordner", " add-on folder(s)");
                var liz = new List<string>();
                if (g.FakeLizenzen > 0) liz.Add(g.FakeLizenzen + " Fake");
                if (g.EchteLizenzen > 0) liz.Add(g.EchteLizenzen + T(" echt", " real"));
                z += T(" – Lizenzdateien: ", " – license files: ") + (liz.Count > 0 ? string.Join(", ", liz) : T("keine gefunden", "none found"));
                l.Add(z);
            }
            return l;
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

        // Überschrift, nummerierte Punkte, zuletzt die Warnung (das Fertig-Fenster setzt sie so zusammen)
        // NEU 1.0.7 (da_Chris86 31.10.2026): "Datenbank neu aufbauen" ist kein empfohlener Schlussschritt mehr (bis 1.0.6 Fix: Schritte
        // 1-3 im abgesicherten Modus). Belegt am 31.10.2026 (PS5 Pro FW 12.00): die Neuerstellung löschte die installierten Daten der
        // PS4-FPKG-Spiele auf der internen SSD (app.pkg/patch.pkg weg); elf-arsenal und ps5-db-backup beschreiben dasselbe (M.2 bleibt,
        // USB-Speicher nur, wenn er abgesteckt ist). Die Windows-Fassung löscht nur Dateien und benutzt Sony nicht - darum löscht sie seit
        // 1.0.7 (da_Chris86 31.10.2026 23:32) keine Apps/Spiele mit Kachel mehr (Suche.cs KachelnSperren), sonst bliebe eine Geisterkachel.
        public static string[] SCHLUSS_TEXT { get { return SchlussText(true, null, 10); } }
        // kacheln: es wurde etwas gewählt, das eine Kachel im Menü haben kann. gefaehrdet: erkannte PS4-Spiele auf interner SSD und
        // USB-Speicher (null = nicht bekannt, dann ohne Liste). max: höchstens so viele Spiele nennen (0 = alle, fürs Protokoll).
        public static string[] SchlussText(bool kacheln, List<Ps4Spiel> gefaehrdet, int max)
        {
            var l = new List<string>();
            string liste = null;
            if (gefaehrdet != null)
            {
                if (gefaehrdet.Count == 0) liste = T("Erkannt: keine installierten PS4-Spiele auf der internen SSD oder dem USB-Speicher.", "Detected: no installed PS4 games on the internal SSD or the USB storage.");
                else
                {
                    int n = max > 0 ? Math.Min(max, gefaehrdet.Count) : gefaehrdet.Count;
                    var namen = gefaehrdet.Take(n).Select(g => (H.Gleich(g.Name, g.Id) || g.Name == "" ? g.Id + " (" : g.Name + " (" + g.Id + ", ") + OrtName(g.Ort) + ")");
                    liste = T("Davon betroffen wären: ", "This would affect: ") + string.Join(", ", namen)
                          + (gefaehrdet.Count > n ? T(" und ", " and ") + (gefaehrdet.Count - n) + T(" weitere (siehe Protokoll)", " more (see log)") : "") + ".";
                }
            }
            if (kacheln)
            {
                l.Add(T("Kacheln im PS5-Menü („Geisterkacheln“):", "Tiles in the PS5 menu (\"ghost tiles\"):"));
                // NEU 1.0.7 (da_Chris86 31.10.2026 23:32): Apps und Spiele mit Kachel löscht die Windows-Fassung nicht mehr (Suche.cs KachelnSperren)
                l.Add(T("1. Apps und Spiele mit Kachel löscht die Windows-Fassung nicht - sie kann Kacheln nicht austragen, es bliebe eine Geisterkachel. Das geht nur mit der PS5-App (über Sonys Deinstallation).",
                        "1. The Windows version does not delete apps and games that have a tile - it cannot remove tiles, a ghost tile would remain. Only the PS5 app can do that (via Sony's uninstall)."));
                l.Add(T("2. „Datenbank neu aufbauen“ (abgesicherter Modus) ist nicht nötig - es löscht installierte PS4-Spiele (FPKG) auf der internen SSD und auf angeschlossenem USB-Speicher (erweiterter Speicher).",
                        "2. 'Rebuild Database' (safe mode) is not needed - it deletes installed PS4 games (FPKG) on the internal SSD and on connected USB extended storage."));
                int nr = 3;
                if (liste != null) l.Add(nr++ + ". " + liste);
                l.Add(nr++ + ". " + T("PS4-Spiele auf der M.2-SSD überstehen das laut Szene (elf-arsenal) - von PS5 Cleanup nicht geprüft.", "PS4 games on the M.2 SSD survive it according to the scene (elf-arsenal) - not verified by PS5 Cleanup."));
                l.Add(nr++ + ". " + T("Die PS5 neu zu starten ist unbedenklich.", "Restarting the PS5 is fine."));
                l.Add(T("Darum empfiehlt PS5 Cleanup „Datenbank neu aufbauen“ nicht.", "That is why PS5 Cleanup does not recommend 'Rebuild Database'."));
            }
            else
            {
                l.Add(T("Hinweis:", "Note:"));
                l.Add(T("1. Die PS5 neu zu starten ist unbedenklich.", "1. Restarting the PS5 is fine."));
                l.Add(T("„Datenbank neu aufbauen“ ist nicht nötig - es würde installierte PS4-Spiele (FPKG) auf der internen SSD und auf angeschlossenem USB-Speicher löschen.",
                        "'Rebuild Database' is not necessary - it would delete installed PS4 games (FPKG) on the internal SSD and on connected USB extended storage."));
            }
            return l.ToArray();
        }

        // -------------------------------------------------------------
        // NEU 1.0.7 (da_Chris86 31.10.2026): Datenbank-Sicherung vor jedem echten Löschlauf
        // -------------------------------------------------------------
        // app.db und appinfo.db (dazu -wal/-journal/-shm, wenn in der Ordnerliste vorhanden und nicht leer) werden NUR GELESEN (RETR)
        // und neben dem Programm abgelegt: "Datenbank-Sicherung\<Datum>_FW<Firmware>_<Modell>\" mit info.txt. Jede Datei wird geladen,
        // bis zwei Ladungen nacheinander gleich sind (SHA-256, höchstens 3 Wiederholungen - die Konsole kann gerade schreiben); bei den
        // Datenbanken muss zusätzlich der SQLite-Kopf stimmen. Klappt das nicht, wird nichts gelöscht. Nie wird auf die PS5 geschrieben,
        // nie wird etwas zurückgespielt. Ausnahme -shm: nur ein Hilfsindex, den SQLite aus -wal neu aufbaut - ist er nicht stabil,
        // wird die letzte Ladung mit Hinweis gespeichert, ohne die Sicherung scheitern zu lassen.
        // NEU 1.0.7 (da_Chris86 31.10.2026, Entscheidung 2): echte SQLite-Prüfung statt nur des Kopfs - nur auf dem PC, nie auf der PS5:
        //  a) Rohkopie (Bytes wie auf der PS5) - wird danach nie von SQLite geöffnet. Ein "heißes" Rollback-Journal (nicht leer, beginnt
        //     mit d9 d5 05 f9 20 a1 63 d7) oder eine geänderte Ordnerliste heißt: die PS5 schreibt gerade -> kurz warten, alles nochmal
        //     laden (höchstens 3 Versuche), sonst Abbruch.
        //  b) Rohkopie in einen temporären Arbeitsordner kopieren (gleiche Namen, -wal daneben), dort mit der SQLite-Bibliothek von
        //     Windows (winsqlite3.dll) öffnen, PRAGMA integrity_check (genau eine Zeile "ok"), mit der SQLite-Backup-Schnittstelle
        //     "<name>_geprueft.db" in den Sicherungsordner schreiben, diese Datei öffnen und nochmal integrity_check, alles schließen,
        //     den Arbeitsordner (nur ihn) entfernen.
        //  c) Fehlt winsqlite3.dll oder klappt irgendein Schritt nicht, gilt die Sicherung als nicht geprüft -> es wird nichts gelöscht.
        public const string MMS = "/system_data/priv/mms";
        static readonly string[] MMS_DB = { "app.db", "appinfo.db" };
        static readonly string[] MMS_BEGLEIT = { "-wal", "-journal", "-shm" };
        public string FIRMWARE = "", MODELL = "";      // vom Verbinden (index.dat bzw. ps5info / Eingabe)
        public string SICHERUNG_WURZEL;                // null = Ordner "Datenbank-Sicherung" neben dem Programm
        public string SICHERUNG_ORDNER = "";           // Ordner der letzten Sicherung
        public int SICHERUNG_VERSUCHE;                 // NEU: Ladeversuche der letzten Sicherung (info.txt, Selbsttest)
        // feste Bezeichnungen (Protokoll, Meldungen, info.txt) - nie "garantiert wiederherstellbar"
        public static string ROH_TEXT { get { return T("Rohkopie (Bytes wie auf der PS5)", "raw copy (bytes as on the PS5)"); } }
        public static string GEPRUEFT_TEXT { get { return T("geprüfte Kopie (SQLite integrity_check: ok, erstellt mit der SQLite-Backup-Schnittstelle)", "checked copy (SQLite integrity_check: ok, created with the SQLite backup API)"); } }

        // -------------------------------------------------------------
        // NEU 1.0.7 (da_Chris86 31.10.2026, Entscheidung 1): Lässt sich die Datenbank überhaupt sichern?
        // -------------------------------------------------------------
        // Nach dem Verbinden einmal geprüft, nur lesend: Ordnerliste von /system_data/priv/mms, app.db und appinfo.db je einmal in den
        // Speicher laden (nichts auf die PS5, nichts auf die Platte), SQLite-Kopf prüfen; dazu, ob die SQLite-Bibliothek von Windows
        // nutzbar ist. Solange das nicht geklappt hat, sind alle Lösch-Wege gesperrt (Knöpfe, KomplettAblauf/AuswahlAblauf,
        // FuehreLoeschung) - Nachschauen und alle Anzeigen bleiben. Die Sicherung direkt vor dem Löschen bleibt die zweite Sperre.
        public enum DbStand { Ungeprueft, Bereit, Gesperrt }
        volatile DbStand dbStand = DbStand.Ungeprueft;
        volatile string dbGrund = "";
        public DbStand DbBereitschaft { get { return dbStand; } }
        public bool LoeschenGesperrt { get { return dbStand != DbStand.Bereit; } }
        public void DbBereitschaftSetzen(DbStand s, string grund) { dbGrund = grund ?? ""; dbStand = s; }
        // Grund der Sperre in Alltagssprache ("" = nicht gesperrt)
        public string LoeschSperreText()
        {
            if (dbStand == DbStand.Bereit) return "";
            if (dbStand == DbStand.Ungeprueft || dbGrund == "")
                return T("Löschen ist gesperrt: Ob sich die Datenbank der PS5 sichern lässt, wurde noch nicht geprüft. Bitte neu verbinden - dabei wird es geprüft.",
                         "Deleting is locked: it has not been checked yet whether the PS5 database can be backed up. Please connect again - this checks it.");
            return dbGrund;
        }
        static string SperrText(string ftpGrund, string bibGrund)
        {
            var t = new StringBuilder(T("Löschen ist gesperrt: ", "Deleting is locked: "));
            if (ftpGrund != null) t.Append(T("Der FTP-Server kann die Datenbank der PS5 (app.db und appinfo.db in ", "The FTP server cannot read the PS5 database (app.db and appinfo.db in ") + MMS + T(") nicht lesen.", ")."));
            if (bibGrund != null) t.Append((ftpGrund != null ? " " : "") + T("Auf diesem PC fehlt die SQLite-Bibliothek von Windows (winsqlite3.dll), mit der die Sicherung geprüft wird.", "The Windows SQLite library (winsqlite3.dll) used to check the backup is missing on this PC."));
            t.Append(T(" Vor jedem Löschen sichert und prüft PS5 Cleanup diese Datenbank - ohne geprüfte Sicherung wird nichts gelöscht. Nachschauen und alle Anzeigen funktionieren weiter.",
                       " PS5 Cleanup backs up and checks this database before every deletion - without a checked backup nothing is deleted. Scanning and all displays keep working."));
            t.Append(T(" Grund: ", " Reason: ") + (ftpGrund ?? "") + (ftpGrund != null && bibGrund != null ? " / " : "") + (bibGrund ?? ""));
            return t.ToString();
        }

        // nur für den Selbsttest: Ordnerliste/Laden aus einem Ordner auf dem PC statt von der PS5 (null = echte Verbindung)
        public Func<string, List<Datei>> DB_LISTE_ERSATZ;
        public Func<string, byte[]> DB_LADEN_ERSATZ;
        List<Datei> DbListe(string p) { return DB_LISTE_ERSATZ != null ? DB_LISTE_ERSATZ(p) : S.Liste(p, true); }
        byte[] DbBytes(string p)
        {
            if (DB_LADEN_ERSATZ != null) return DB_LADEN_ERSATZ(p);
            S.Ftp.LetzterFehler = "";       // kein alter Fehler im Grund
            return S.Bytes(p);
        }
        string FtpFehler() { return DB_LADEN_ERSATZ == null && S != null && S.Ftp.LetzterFehler != "" ? " (" + S.Ftp.LetzterFehler + ")" : ""; }

        // Die Prüfung selbst (nach dem Nachschauen). Liefert den neuen Stand und schreibt ihn ins Protokoll.
        public DbStand DatenbankBereitPruefen()
        {
            DbBereitschaftSetzen(DbStand.Ungeprueft, "");
            Melde(T("Prüfe, ob sich die Datenbank der PS5 sichern lässt (nur lesen - auf der PS5 wird nichts verändert) ...", "Checking whether the PS5 database can be backed up (read only - nothing is changed on the PS5) ..."));
            string ftp = null, groessen = "";
            List<Datei> l = null;
            try { l = DbListe(MMS); }
            catch (AbbruchAusnahme) { throw; }
            catch (Exception ex) { ftp = T("Ordner ", "Folder ") + MMS + T(" nicht lesbar: ", " not readable: ") + ex.Message; }
            if (ftp == null && l == null) ftp = T("Ordner ", "Folder ") + MMS + T(" nicht gefunden", " not found");
            if (ftp == null)
                foreach (var db in MMS_DB)
                {
                    string dbn = db;
                    var e = l.FirstOrDefault(x => x.Name == dbn && !x.IstOrdner);
                    if (e == null || e.Groesse <= 0) { ftp = MMS + "/" + db + T(" fehlt oder ist leer", " is missing or empty"); break; }
                    byte[] b = null;
                    try { b = DbBytes(MMS + "/" + db); }
                    catch (AbbruchAusnahme) { throw; }
                    catch (Exception ex) { ftp = db + T(" ließ sich nicht laden: ", " could not be downloaded: ") + ex.Message; break; }
                    if (b == null) { ftp = db + T(" ließ sich nicht laden", " could not be downloaded") + FtpFehler(); break; }
                    var k = SqliteKopfPruefen(b);
                    if (!k.Ok) { ftp = db + T(" kam nicht richtig an: ", " did not arrive correctly: ") + k.Grund; break; }
                    groessen += (groessen != "" ? ", " : "") + db + " " + GroesseText(b.LongLength);
                }
            string version;
            string bib = SqliteBibliothek(out version);
            if (ftp == null && bib == null)
            {
                DbBereitschaftSetzen(DbStand.Bereit, "");
                Melde(T("  Datenbank der PS5 lesbar (", "  PS5 database readable (") + groessen + T(") - Löschen ist möglich. Vor jedem Löschen wird sie gesichert und geprüft.", ") - deleting is possible. It is backed up and checked before every deletion."));
                Melde(T("  Prüfung mit der SQLite-Bibliothek von Windows (winsqlite3.dll), Version ", "  Checked with the Windows SQLite library (winsqlite3.dll), version ") + version);
                return dbStand;
            }
            DbBereitschaftSetzen(DbStand.Gesperrt, SperrText(ftp, bib));
            Melde(T("  ACHTUNG: ", "  WARNING: ") + dbGrund);
            return dbStand;
        }

        // Reine Prüfung des SQLite-Kopfs (ohne Verbindung - der Selbsttest ruft sie direkt auf):
        // "SQLite format 3\0", Seitengröße = Big-Endian uint16 bei 16 (1 = 65536), Seitenzahl = Big-Endian uint32 bei 28.
        // Gilt die Größe im Kopf (Bytes 24..27 = 92..95, Seitenzahl > 0 wie bei SQLite), muss Seitengröße × Seitenzahl = Dateilänge sein.
        public static DbPruefung SqliteKopfPruefen(byte[] b)
        {
            var r = new DbPruefung();
            if (b == null || b.Length < 100) { r.Grund = T("Datei fehlt oder ist kürzer als der SQLite-Kopf (100 Bytes)", "file missing or shorter than the SQLite header (100 bytes)"); return r; }
            byte[] magie = Encoding.ASCII.GetBytes("SQLite format 3\0");
            for (int i = 0; i < 16; i++)
                if (b[i] != magie[i]) { r.Grund = T("kein SQLite-Kopf („SQLite format 3“ fehlt)", "no SQLite header ('SQLite format 3' missing)"); return r; }
            int roh = (b[16] << 8) | b[17];
            int seite = roh == 1 ? 65536 : roh;
            if (seite < 512 || seite > 65536 || (seite & (seite - 1)) != 0) { r.Grund = T("ungültige Seitengröße ", "invalid page size ") + roh; return r; }
            r.SeitenGroesse = seite;
            r.Seiten = ((long)b[28] << 24) | ((long)b[29] << 16) | ((long)b[30] << 8) | b[31];
            r.KopfGroesseGueltig = r.Seiten > 0 && b[24] == b[92] && b[25] == b[93] && b[26] == b[94] && b[27] == b[95];
            if (r.KopfGroesseGueltig && (long)seite * r.Seiten != b.LongLength)
            {
                r.Grund = string.Format(CultureInfo.InvariantCulture, T("Größe passt nicht: {0} Seiten × {1} Bytes = {2} Bytes, die Datei hat {3} Bytes", "size mismatch: {0} pages × {1} bytes = {2} bytes, the file has {3} bytes"),
                                        r.Seiten, seite, (long)seite * r.Seiten, b.LongLength);
                return r;
            }
            r.Ok = true;
            return r;
        }

        static string Sha256(byte[] b)
        {
            using (var h = SHA256.Create()) return BitConverter.ToString(h.ComputeHash(b)).Replace("-", "").ToLowerInvariant();
        }
        // Firmware/Modell für den Ordnernamen: nur Buchstaben, Ziffern, Punkt, Minus; leer/unbekannt = "unbekannt"
        static string FuerDateiname(string s)
        {
            s = (s ?? "").Trim();
            if (s == "" || H.Gleich(s, "unbekannt") || H.Gleich(s, "unknown")) return "unbekannt";
            var b = new StringBuilder();
            foreach (char c in s) b.Append((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' ? c : '_');
            string r = b.ToString().Trim('.', '_');
            if (r.Length > 32) r = r.Substring(0, 32).TrimEnd('.', '_');
            return r == "" ? "unbekannt" : r;
        }

        class Ladung { public string Name; public long Liste; public bool Db; public bool Pflicht; public byte[] Daten; public string Sha = ""; public bool Ok; public int Versuche; public DbPruefung Pruefung; public string Grund = "";
                       public SqlitePruefung Sqlite; }   // NEU: Ergebnis der SQLite-Prüfung (nur bei app.db/appinfo.db)

        // eine Datei laden, bis zwei Ladungen nacheinander gleich sind (und bei Datenbanken der Kopf stimmt)
        void Laden(Ladung d)
        {
            string pfad = MMS + "/" + d.Name;
            byte[] vorher = DbBytes(pfad);
            if (vorher == null) { d.Grund = T("ließ sich nicht laden", "could not be downloaded") + FtpFehler(); return; }
            d.Daten = vorher; d.Sha = Sha256(vorher);
            string shaVorher = d.Sha;
            for (int w = 1; w <= 4; w++)      // Vergleich + bis zu 3 Wiederholungen
            {
                if (w > 1) Warte(1500);
                d.Versuche = w;
                byte[] neu = DbBytes(pfad);
                if (neu == null) { d.Grund = T("ließ sich beim zweiten Laden nicht laden", "could not be downloaded the second time") + FtpFehler(); return; }
                d.Daten = neu; d.Sha = Sha256(neu);
                if (d.Db) d.Pruefung = SqliteKopfPruefen(neu);
                bool gleich = d.Sha == shaVorher;
                if (gleich && (d.Pruefung == null || d.Pruefung.Ok)) { d.Ok = true; d.Grund = ""; return; }
                d.Grund = !gleich ? T("zwei Ladungen nacheinander waren verschieden - die Konsole schreibt evtl. gerade", "two consecutive downloads differed - the console may be writing")
                                  : T("beschädigt oder unvollständig: ", "damaged or incomplete: ") + d.Pruefung.Grund;
                shaVorher = d.Sha;
            }
            d.Grund += T(" (auch nach 3 Wiederholungen)", " (even after 3 retries)");
        }

        // "Heißes" Rollback-Journal: nicht leer und beginnt mit der Journal-Kennung d9 d5 05 f9 20 a1 63 d7 - die PS5 war mitten im
        // Schreiben, die Datenbankdatei kann halb geschrieben sein. (Nach dem Schreiben löscht, kürzt oder nullt SQLite den Anfang.)
        static readonly byte[] JOURNAL_KENNUNG = { 0xd9, 0xd5, 0x05, 0xf9, 0x20, 0xa1, 0x63, 0xd7 };
        public static bool HeissesJournal(byte[] b)
        {
            if (b == null || b.Length < JOURNAL_KENNUNG.Length) return false;
            for (int i = 0; i < JOURNAL_KENNUNG.Length; i++) if (b[i] != JOURNAL_KENNUNG[i]) return false;
            return true;
        }
        // app.db -> app_geprueft.db
        public static string GeprueftName(string db) { return Path.GetFileNameWithoutExtension(db) + "_geprueft.db"; }

        // Datenbanken und ihre Begleitdateien (ohne -shm, nur ein Hilfsindex) samt Größe: unterscheiden sich zwei Ordnerlisten? null = gleich
        static string ListeVergleich(List<Datei> a, List<Datei> b)
        {
            Func<List<Datei>, Dictionary<string, long>> wichtig = l =>
            {
                var t = new Dictionary<string, long>(StringComparer.Ordinal);
                foreach (var e in l)
                    if (!e.IstOrdner && MMS_DB.Any(db => e.Name == db || e.Name == db + "-wal" || e.Name == db + "-journal")) t[e.Name] = e.Groesse;
                return t;
            };
            var x = wichtig(a); var y = wichtig(b);
            foreach (var k in x.Keys)
            {
                if (!y.ContainsKey(k)) return k + T(" ist verschwunden", " disappeared");
                if (y[k] != x[k]) return k + ": " + x[k].ToString(CultureInfo.InvariantCulture) + " -> " + y[k].ToString(CultureInfo.InvariantCulture) + " Bytes";
            }
            foreach (var k in y.Keys) if (!x.ContainsKey(k)) return k + T(" ist neu dazugekommen", " appeared");
            return null;
        }

        // Ein Ladeversuch: Ordnerliste, jede Datei laden (je zweimal gleich), "heißes" Journal prüfen, Ordnerliste nochmal vergleichen.
        // null = alles geladen; sonst der Grund. schreibt = die PS5 schreibt gerade (ein neuer Versuch ist sinnvoll).
        string RohkopienLaden(out List<Ladung> laden, out bool schreibt)
        {
            laden = new List<Ladung>(); schreibt = false;
            List<Datei> l;
            try { l = DbListe(MMS); }
            catch (AbbruchAusnahme) { throw; }
            catch (Exception ex) { return T("Ordner ", "Folder ") + MMS + T(" nicht lesbar: ", " not readable: ") + ex.Message; }
            if (l == null) return T("Ordner ", "Folder ") + MMS + T(" nicht gefunden", " not found");
            foreach (var db in MMS_DB)
            {
                string dbn = db;
                var e = l.FirstOrDefault(x => x.Name == dbn && !x.IstOrdner);
                if (e == null || e.Groesse <= 0) return MMS + "/" + db + T(" fehlt oder ist leer", " is missing or empty");
                laden.Add(new Ladung { Name = db, Liste = e.Groesse, Db = true, Pflicht = true });
                foreach (var z in MMS_BEGLEIT)
                {
                    string bn = db + z;
                    var b = l.FirstOrDefault(x => x.Name == bn && !x.IstOrdner);
                    if (b != null && b.Groesse > 0) laden.Add(new Ladung { Name = bn, Liste = b.Groesse, Pflicht = z != "-shm" });
                }
            }
            foreach (var d in laden)
            {
                try { Laden(d); }
                catch (AbbruchAusnahme) { throw; }
                catch (Exception ex) { d.Ok = false; d.Grund = ex.Message; }
                if (d.Ok && d.Name.EndsWith("-journal", StringComparison.Ordinal) && HeissesJournal(d.Daten))
                {
                    d.Ok = false; schreibt = true;
                    d.Grund = T("das Journal ist „heiß“ (beginnt mit d9 d5 05 f9 20 a1 63 d7) - die PS5 war gerade mitten im Schreiben", "the journal is 'hot' (starts with d9 d5 05 f9 20 a1 63 d7) - the PS5 was in the middle of writing");
                }
                Melde("  " + d.Name + ": " + (d.Ok ? GroesseText(d.Daten.LongLength) + ", SHA-256 " + d.Sha.Substring(0, 16) + "..., " + ROH_TEXT
                                                   : (d.Pflicht ? T("FEHLER - ", "ERROR - ") : T("Hinweis - ", "Note - ")) + d.Grund + (d.Pflicht || d.Daten == null ? "" : T(" (Hilfsdatei - letzte Ladung gespeichert)", " (helper file - last download saved)"))));
                if (!d.Ok && d.Pflicht) return d.Name + ": " + d.Grund;
            }
            // Hat sich während des Ladens etwas geändert (Journal/WAL aufgetaucht, Größe anders)? Dann schreibt die PS5 gerade.
            List<Datei> l2;
            try { l2 = DbListe(MMS); }
            catch (AbbruchAusnahme) { throw; }
            catch (Exception ex) { return T("Ordner ", "Folder ") + MMS + T(" beim zweiten Lesen nicht lesbar: ", " not readable the second time: ") + ex.Message; }
            string anders = l2 == null ? T("Ordner verschwunden", "folder gone") : ListeVergleich(l, l2);
            if (anders != null)
            {
                schreibt = true;
                return T("Der Datenbank-Ordner hat sich während des Ladens geändert (", "The database folder changed while downloading (") + anders + T(") - die PS5 schreibt gerade", ") - the PS5 is writing right now");
            }
            return null;
        }

        // null = vollständig gesichert und geprüft (Ordner in SICHERUNG_ORDNER), sonst der Grund
        public string DatenbankSichern()
        {
            SICHERUNG_ORDNER = ""; SICHERUNG_VERSUCHE = 0;
            Melde(T("Sichere zuerst die Datenbank der PS5 (nur lesen - auf der PS5 wird nichts verändert) ...", "Backing up the PS5 database first (read only - nothing is changed on the PS5) ..."));
            // NEU (Entscheidung 2): ohne SQLite-Bibliothek keine geprüfte Kopie - dann gar nicht erst laden
            string version;
            string bib = SqliteBibliothek(out version);
            if (bib != null) return bib;
            Melde(T("  Prüfung mit der SQLite-Bibliothek von Windows (winsqlite3.dll), Version ", "  Checked with the Windows SQLite library (winsqlite3.dll), version ") + version);
            // 1. Rohkopien in den Speicher laden - schreibt die PS5 gerade: 2 s warten, alles nochmal (höchstens 3 Versuche)
            List<Ladung> laden = new List<Ladung>();
            string fehler = null; bool schreibt = false;
            for (int v = 1; v <= 3; v++)
            {
                SICHERUNG_VERSUCHE = v;
                if (v > 1)
                {
                    Melde("  " + T("Die PS5 schreibt gerade in die Datenbank - lade in 2 s alles nochmal (Versuch ", "The PS5 is writing to the database right now - downloading everything again in 2 s (attempt ") + v + T(" von 3) ...", " of 3) ..."));
                    Warte(2000);
                }
                fehler = RohkopienLaden(out laden, out schreibt);
                if (fehler == null || !schreibt) break;
            }
            if (fehler != null && schreibt) fehler += T(" - auch nach 3 Versuchen", " - even after 3 attempts");
            if (fehler != null && laden.All(x => x.Daten == null)) return fehler;   // nichts geladen (z.B. Ordner nicht lesbar) - kein Sicherungsordner
            // 2. Sicherungsordner anlegen, Rohkopien speichern (genau die geladenen Bytes)
            string wurzel = SICHERUNG_WURZEL ?? Path.Combine(Programm.Ordner, "Datenbank-Sicherung");
            string name = DateTime.Now.ToString("yyyy-MM-dd_HHmmss", CultureInfo.InvariantCulture) + "_FW" + FuerDateiname(FIRMWARE) + "_" + FuerDateiname(MODELL);
            string ordner = Path.Combine(wurzel, name);
            try
            {
                for (int n = 2; Directory.Exists(ordner); n++) ordner = Path.Combine(wurzel, name + "_" + n);
                Directory.CreateDirectory(ordner);
            }
            catch (Exception ex) { return T("Der Sicherungsordner ließ sich nicht anlegen (", "Could not create the backup folder (") + ordner + "): " + ex.Message + T(" - liegt das Programm in einem schreibgeschützten Ordner?", " - is the program in a read-only folder?"); }
            foreach (var d in laden)
            {
                if (d.Daten == null) continue;
                try { File.WriteAllBytes(Path.Combine(ordner, d.Name), d.Daten); }
                catch (Exception ex) { d.Ok = false; d.Pflicht = true; d.Grund = T("ließ sich nicht speichern: ", "could not be saved: ") + ex.Message; if (fehler == null) fehler = d.Name + ": " + d.Grund; }
            }
            // 3. geprüfte Kopien: Arbeitskopie im Temp-Ordner -> integrity_check -> SQLite-Backup-Schnittstelle -> integrity_check der Kopie
            if (fehler == null)
                foreach (var d in laden.Where(x => x.Db))
                {
                    d.Sqlite = GepruefteKopieErstellen(Path.Combine(ordner, d.Name), Path.Combine(ordner, GeprueftName(d.Name)));
                    if (d.Sqlite.Ok) Melde("  " + GeprueftName(d.Name) + ": " + GEPRUEFT_TEXT + ", " + GroesseText(d.Sqlite.Bytes));
                    else Melde("  " + d.Name + T(": FEHLER - SQLite-Prüfung: ", ": ERROR - SQLite check: ") + d.Sqlite.Grund);
                    if (d.Sqlite.Hinweis != "") Melde("  " + T("Hinweis: ", "Note: ") + d.Sqlite.Hinweis);
                    if (!d.Sqlite.Ok) { fehler = d.Name + T(": SQLite-Prüfung fehlgeschlagen - ", ": SQLite check failed - ") + d.Sqlite.Grund; break; }
                }
            // 4. Rohkopien unverändert? (SQLite öffnet sie nie - zur Sicherheit nachgerechnet)
            if (fehler == null)
                foreach (var d in laden)
                {
                    if (d.Daten == null) continue;
                    string jetzt;
                    try { jetzt = Sha256(File.ReadAllBytes(Path.Combine(ordner, d.Name))); }
                    catch (Exception ex) { jetzt = ex.Message; }
                    if (jetzt != d.Sha) { fehler = d.Name + T(": die gespeicherte Rohkopie weicht von der Ladung ab", ": the saved raw copy differs from the download"); break; }
                }
            // 5. info.txt - auch bei einem Fehler (dann bekommt der Ordner die Endung _FEHLGESCHLAGEN)
            var t = new StringBuilder();
            Action<string> zl = s => t.Append(s).Append("\r\n");
            zl("PS5 Cleanup " + VERSION + " - " + T("Datenbank-Sicherung", "database backup"));
            zl(T("Datum: ", "Date: ") + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss", CultureInfo.InvariantCulture));
            zl("Firmware: " + (FuerDateiname(FIRMWARE) == "unbekannt" ? T("unbekannt", "unknown") : FIRMWARE));
            zl(T("Modell: ", "Model: ") + (FuerDateiname(MODELL) == "unbekannt" ? T("unbekannt", "unknown") : MODELL));
            zl(T("Quelle auf der PS5: ", "Source on the PS5: ") + MMS + T(" (nur gelesen per FTP - auf der PS5 wurde nichts verändert)", " (read only via FTP - nothing was changed on the PS5)"));
            zl(T("Geprüft mit: SQLite-Bibliothek von Windows (winsqlite3.dll), Version ", "Checked with: Windows SQLite library (winsqlite3.dll), version ") + version);
            if (SICHERUNG_VERSUCHE > 1) zl(T("Ladeversuche: ", "Download attempts: ") + SICHERUNG_VERSUCHE + T(" (vorher schrieb die PS5 gerade in die Datenbank)", " (the PS5 was writing to the database before)"));
            zl("");
            zl(T("ROHKOPIEN", "RAW COPIES"));
            foreach (var d in laden)
            {
                zl(d.Name + " - " + ROH_TEXT);
                zl(T("  Größe: ", "  Size: ") + (d.Daten != null ? d.Daten.LongLength.ToString(CultureInfo.InvariantCulture) + " Bytes" : "-") + T(" (laut Ordnerliste ", " (folder listing ") + d.Liste.ToString(CultureInfo.InvariantCulture) + ")");
                zl("  SHA-256: " + (d.Sha != "" ? d.Sha : "-"));
                string pr;
                if (d.Ok) pr = T("OK - zweimal nacheinander gleich geladen", "OK - downloaded twice in a row, identical") + (d.Versuche > 1 ? " (" + T("Versuch ", "attempt ") + d.Versuche + ")" : "");
                else if (d.Daten == null && d.Grund == "") pr = T("nicht geladen (Sicherung vorher abgebrochen)", "not downloaded (backup stopped earlier)");
                else pr = (d.Pflicht ? T("FEHLER - ", "ERROR - ") : T("Hinweis - ", "Note - ")) + d.Grund;
                if (d.Pruefung != null)
                    pr += "; " + (d.Pruefung.Ok ? T("SQLite-Kopf gültig, Seitengröße ", "SQLite header valid, page size ") + d.Pruefung.SeitenGroesse + ", " + d.Pruefung.Seiten + T(" Seiten", " pages")
                                                 + (d.Pruefung.KopfGroesseGueltig ? T(" - passt zur Dateigröße", " - matches the file size") : T(" (Größe im Kopf nicht gültig, nicht verglichen)", " (in-header size not valid, not compared)"))
                                               : T("SQLite-Kopf: ", "SQLite header: ") + d.Pruefung.Grund);
                zl(T("  Prüfung: ", "  Check: ") + pr);
            }
            zl("");
            zl(T("GEPRÜFTE KOPIEN", "CHECKED COPIES"));
            foreach (var d in laden.Where(x => x.Db))
            {
                var p = d.Sqlite;
                if (p != null && p.Ok)
                {
                    zl(GeprueftName(d.Name) + " - " + GEPRUEFT_TEXT);
                    zl(T("  Erstellt aus der Rohkopie ", "  Created from the raw copy ") + d.Name + (p.MitWal ? " + " + d.Name + "-wal" : "") +
                       T(" über eine Arbeitskopie in einem temporären Ordner (die Rohkopie selbst wurde nicht geöffnet)", " via a working copy in a temporary folder (the raw copy itself was not opened)"));
                    zl(T("  integrity_check: Arbeitskopie ok, geprüfte Kopie ok", "  integrity_check: working copy ok, checked copy ok"));
                    zl(T("  Größe: ", "  Size: ") + p.Bytes.ToString(CultureInfo.InvariantCulture) + " Bytes");
                    zl("  SHA-256: " + p.Sha);
                }
                else if (p != null) zl(d.Name + T(" - keine geprüfte Kopie: ", " - no checked copy: ") + p.Grund);
                else zl(d.Name + T(" - keine geprüfte Kopie (nicht geprüft, Sicherung vorher abgebrochen)", " - no checked copy (not checked, backup stopped earlier)"));
            }
            zl("");
            zl(T("Ergebnis: ", "Result: ") + (fehler == null ? "OK" : T("FEHLGESCHLAGEN - ", "FAILED - ") + fehler + T(" (es wurde nichts gelöscht)", " (nothing was deleted)")));
            zl("");
            zl(T("Wichtig:", "Important:"));
            zl(T("- Das ist nur eine Sicherung. PS5 Cleanup hat keine Funktion zum Zurückspielen und spielt nichts zurück.", "- This is only a backup. PS5 Cleanup has no restore function and never restores anything."));
            zl(T("- Ein Zurückspielen auf die PS5 wurde nie getestet.", "- Restoring to the PS5 has never been tested."));
            zl(T("- „integrity_check: ok“ heißt nur: SQLite hat im Aufbau der Kopie keinen Fehler gefunden. Ob die PS5 mit einer zurückgespielten Kopie richtig arbeitet, ist damit nicht geprüft.",
                 "- 'integrity_check: ok' only means: SQLite found no error in the structure of the copy. Whether the PS5 works correctly with a restored copy has not been checked."));
            try { File.WriteAllText(Path.Combine(ordner, "info.txt"), t.ToString(), new UTF8Encoding(true)); }
            catch (Exception ex) { if (fehler == null) fehler = "info.txt: " + ex.Message; }
            if (fehler != null)
            {
                try { string neu = ordner + T("_FEHLGESCHLAGEN", "_FAILED"); if (!Directory.Exists(neu)) { Directory.Move(ordner, neu); ordner = neu; } } catch { }
                SICHERUNG_ORDNER = ordner;
                return fehler;
            }
            SICHERUNG_ORDNER = ordner;
            Melde(T("  Datenbank gesichert und geprüft: ", "  Database backed up and checked: ") + ordner);
            return null;
        }

        // -------------------------------------------------------------
        // NEU 1.0.7 (da_Chris86 31.10.2026, Entscheidung 2): SQLite-Prüfung auf dem PC (winsqlite3.dll)
        // -------------------------------------------------------------
        // null = winsqlite3.dll geladen und alle benutzten Funktionen vorhanden (version = ihre SQLite-Version), sonst der Grund
        public static string SqliteBibliothek(out string version)
        {
            version = "";
            try
            {
                Marshal.PrelinkAll(typeof(SqliteNativ));      // löst alle Funktionen sofort auf - fehlt eine, kommt der Fehler hier
                version = SqliteNativ.Version();
                return null;
            }
            catch (Exception ex) { return SqliteFehler(ex); }
        }
        static string SqliteFehler(Exception ex)
        {
            if (ex is DllNotFoundException || ex is EntryPointNotFoundException || ex is BadImageFormatException)
                return T("SQLite-Bibliothek von Windows (winsqlite3.dll) nicht nutzbar: ", "Windows SQLite library (winsqlite3.dll) not usable: ") + ex.GetType().Name + " - " + ex.Message;
            return ex.GetType().Name + ": " + ex.Message;
        }

        // Prüft eine Rohkopie (Datei roh, daneben ggf. roh-wal) und schreibt die geprüfte Kopie nach ziel. Die Rohkopie wird nur kopiert,
        // nie von SQLite geöffnet. Reine Funktion ohne PS5 - der Selbsttest ruft sie direkt auf. Bei einem Fehler bleibt keine Datei ziel
        // zurück (sie wird nur entfernt, wenn sie in diesem Aufruf neu angelegt wurde).
        public static SqlitePruefung GepruefteKopieErstellen(string roh, string ziel)
        {
            var r = new SqlitePruefung();
            r.ArbeitOrdner = Path.Combine(Path.GetTempPath(), "PS5-Cleanup", "db-pruefung-" + Guid.NewGuid().ToString("N"));
            if (!File.Exists(roh)) { r.Grund = T("Rohkopie fehlt: ", "raw copy missing: ") + roh; return r; }
            if (File.Exists(ziel)) { r.Grund = T("Die Datei gibt es schon: ", "the file already exists: ") + ziel; return r; }
            bool zielAngelegt = false;
            string fehler = PruefSchritte(roh, ziel, r, ref zielAngelegt);
            if (fehler == null)
            {
                try { var b = File.ReadAllBytes(ziel); r.Bytes = b.LongLength; r.Sha = Sha256(b); }
                catch (Exception ex) { fehler = T("geprüfte Kopie nicht lesbar: ", "checked copy not readable: ") + ex.Message; }
            }
            r.Ok = fehler == null; r.Grund = fehler ?? "";
            // eine nicht bestandene Kopie darf nicht "_geprueft" heißen: nur die in diesem Aufruf neu angelegte Datei entfernen
            if (!r.Ok && zielAngelegt)
                foreach (var e in new[] { "", "-journal", "-wal", "-shm" })
                    try { if (File.Exists(ziel + e)) File.Delete(ziel + e); } catch { }
            return r;
        }
        // Arbeitskopie -> integrity_check -> SQLite-Backup-Schnittstelle -> integrity_check der geprüften Kopie. null = alles ok, sonst der
        // Grund. Schließt in jedem Fall alle Verbindungen und entfernt den eigenen Arbeitsordner (nur ihn).
        static string PruefSchritte(string roh, string ziel, SqlitePruefung r, ref bool zielAngelegt)
        {
            string arbeit = r.ArbeitOrdner;
            IntPtr quelle = IntPtr.Zero, kopie = IntPtr.Zero;
            bool arbeitAngelegt = false;
            try
            {
                Directory.CreateDirectory(arbeit); arbeitAngelegt = true;
                // Arbeitskopie mit denselben Namen (app.db-wal neben app.db). -journal und -shm nicht: ein nicht "heißes" Journal ist
                // ohne Bedeutung, -shm baut SQLite aus -wal selbst neu auf.
                string arbeitDb = Path.Combine(arbeit, Path.GetFileName(roh));
                File.Copy(roh, arbeitDb);
                if (File.Exists(roh + "-wal") && new FileInfo(roh + "-wal").Length > 0) { File.Copy(roh + "-wal", arbeitDb + "-wal"); r.MitWal = true; }
                string f;
                quelle = SqliteNativ.Oeffnen(arbeitDb, SqliteNativ.OPEN_READWRITE, out f);
                if (quelle == IntPtr.Zero) return T("Arbeitskopie ließ sich nicht öffnen: ", "working copy could not be opened: ") + f;
                f = SqliteNativ.Integritaet(quelle);
                if (f != null) return T("Arbeitskopie: ", "working copy: ") + f;
                kopie = SqliteNativ.Oeffnen(ziel, SqliteNativ.OPEN_READWRITE | SqliteNativ.OPEN_CREATE, out f);
                zielAngelegt = File.Exists(ziel) || kopie != IntPtr.Zero;
                if (kopie == IntPtr.Zero) return T("geprüfte Kopie ließ sich nicht anlegen: ", "checked copy could not be created: ") + f;
                f = SqliteNativ.Sichern(quelle, kopie);
                if (f != null) return T("SQLite-Backup-Schnittstelle: ", "SQLite backup API: ") + f;
                SqliteNativ.Schliessen(kopie); kopie = IntPtr.Zero;
                kopie = SqliteNativ.Oeffnen(ziel, SqliteNativ.OPEN_READWRITE, out f);
                if (kopie == IntPtr.Zero) return T("geprüfte Kopie ließ sich nicht wieder öffnen: ", "checked copy could not be reopened: ") + f;
                f = SqliteNativ.Integritaet(kopie);
                if (f != null) return T("geprüfte Kopie: ", "checked copy: ") + f;
                return null;
            }
            catch (Exception ex) { return SqliteFehler(ex); }
            finally
            {
                try { if (kopie != IntPtr.Zero) SqliteNativ.Schliessen(kopie); } catch { }
                try { if (quelle != IntPtr.Zero) SqliteNativ.Schliessen(quelle); } catch { }
                // nur den eigenen Arbeitsordner entfernen (ein Virenscanner kann die Dateien kurz festhalten - dann bis zu 3 Versuche)
                if (arbeitAngelegt)
                {
                    string rest = "";
                    for (int i = 0; i < 3; i++)
                    {
                        try { if (Directory.Exists(arbeit)) Directory.Delete(arbeit, true); rest = ""; break; }
                        catch (Exception ex) { rest = ex.Message; Thread.Sleep(300); }
                    }
                    if (rest != "") r.Hinweis = T("Der temporäre Arbeitsordner ließ sich nicht entfernen (", "The temporary working folder could not be removed (") + arbeit + "): " + rest;
                }
            }
        }
    }

    // =====================================================================
    // NEU 1.0.7 (da_Chris86 31.10.2026, Entscheidung 2): SQLite-Bibliothek von Windows (winsqlite3.dll, ab Windows 10 in System32).
    // Nur für die Prüfung der Datenbank-Sicherung auf dem PC - nie für die PS5. Auf x86 __stdcall (CallingConvention.Winapi),
    // geladen nur aus dem Systemordner (keine fremde DLL neben dem Programm). Pfade und SQL gehen als UTF-8 mit Null am Ende hinein.
    // =====================================================================
    static class SqliteNativ
    {
        const string DLL = "winsqlite3.dll";
        public const int OK = 0, ROW = 100, DONE = 101;
        public const int OPEN_READWRITE = 0x00000002, OPEN_CREATE = 0x00000004;

        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern IntPtr sqlite3_libversion();
        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern int sqlite3_open_v2(byte[] filename, out IntPtr db, int flags, IntPtr zVfs);
        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern int sqlite3_close_v2(IntPtr db);
        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern IntPtr sqlite3_errmsg(IntPtr db);
        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern int sqlite3_prepare_v2(IntPtr db, byte[] zSql, int nByte, out IntPtr stmt, IntPtr pzTail);
        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern int sqlite3_step(IntPtr stmt);
        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern IntPtr sqlite3_column_text(IntPtr stmt, int iCol);
        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern int sqlite3_column_bytes(IntPtr stmt, int iCol);
        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern int sqlite3_finalize(IntPtr stmt);
        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern IntPtr sqlite3_backup_init(IntPtr pDest, byte[] zDestName, IntPtr pSource, byte[] zSourceName);
        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern int sqlite3_backup_step(IntPtr p, int nPage);
        [DllImport(DLL, CallingConvention = CallingConvention.Winapi, ExactSpelling = true), DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
        static extern int sqlite3_backup_finish(IntPtr p);

        // Text als UTF-8 mit Null am Ende (so erwartet ihn die DLL)
        static byte[] Z(string s) { return Encoding.UTF8.GetBytes(s + "\0"); }
        // UTF-8-Text von der DLL lesen; laenge < 0 = bis zur Null
        static string Text(IntPtr p, int laenge)
        {
            if (p == IntPtr.Zero) return "";
            if (laenge < 0) { laenge = 0; while (laenge < 1048576 && Marshal.ReadByte(p, laenge) != 0) laenge++; }
            var b = new byte[laenge];
            if (laenge > 0) Marshal.Copy(p, b, 0, laenge);
            return Encoding.UTF8.GetString(b);
        }
        public static string Version() { return Text(sqlite3_libversion(), -1); }
        public static string Fehlertext(IntPtr db) { try { return db == IntPtr.Zero ? "" : Text(sqlite3_errmsg(db), -1); } catch { return ""; } }

        // IntPtr.Zero = nicht geöffnet (fehler = Grund)
        public static IntPtr Oeffnen(string pfad, int flags, out string fehler)
        {
            IntPtr db;
            int rc = sqlite3_open_v2(Z(pfad), out db, flags, IntPtr.Zero);
            if (rc == OK && db != IntPtr.Zero) { fehler = null; return db; }
            fehler = "sqlite3_open_v2 " + rc + (db != IntPtr.Zero ? ": " + Fehlertext(db) : "");
            if (db != IntPtr.Zero) sqlite3_close_v2(db);
            return IntPtr.Zero;
        }
        public static void Schliessen(IntPtr db) { if (db != IntPtr.Zero) sqlite3_close_v2(db); }

        // eine Anweisung ganz ausführen (Zeilen werden übergangen); null = ok
        public static string Ausfuehren(IntPtr db, string sql)
        {
            IntPtr st;
            int rc = sqlite3_prepare_v2(db, Z(sql), -1, out st, IntPtr.Zero);
            if (rc != OK) return "prepare " + rc + ": " + Fehlertext(db);
            if (st == IntPtr.Zero) return null;
            try
            {
                while ((rc = sqlite3_step(st)) == ROW) { }
                return rc == DONE ? null : "step " + rc + ": " + Fehlertext(db);
            }
            finally { sqlite3_finalize(st); }
        }
        // erste Spalte der ersten Zeile als Text; null = Fehler oder keine Zeile
        public static string Wert(IntPtr db, string sql)
        {
            IntPtr st;
            if (sqlite3_prepare_v2(db, Z(sql), -1, out st, IntPtr.Zero) != OK || st == IntPtr.Zero) return null;
            try { return sqlite3_step(st) == ROW ? Spalte(st, 0) : null; }
            finally { sqlite3_finalize(st); }
        }
        static string Spalte(IntPtr st, int i)
        {
            IntPtr p = sqlite3_column_text(st, i);      // erst der Text, dann seine Länge (so verlangt es SQLite)
            int n = sqlite3_column_bytes(st, i);
            return Text(p, n);
        }

        // PRAGMA integrity_check: null = genau eine Zeile "ok", sonst der Grund (höchstens 5 Zeilen)
        public static string Integritaet(IntPtr db)
        {
            IntPtr st;
            int rc = sqlite3_prepare_v2(db, Z("PRAGMA integrity_check"), -1, out st, IntPtr.Zero);
            if (rc != OK || st == IntPtr.Zero) return "PRAGMA integrity_check: " + rc + " " + Fehlertext(db);
            var zeilen = new List<string>(); int n = 0;
            try
            {
                while (true)
                {
                    rc = sqlite3_step(st);
                    if (rc == ROW) { n++; if (zeilen.Count < 5) zeilen.Add(Spalte(st, 0)); continue; }
                    if (rc == DONE) break;
                    return "PRAGMA integrity_check: " + rc + " " + Fehlertext(db);
                }
            }
            finally { sqlite3_finalize(st); }
            if (n == 1 && zeilen[0] == "ok") return null;
            if (n == 0) return Sp.T("integrity_check lieferte kein Ergebnis", "integrity_check returned nothing");
            return "integrity_check: " + string.Join(" | ", zeilen) + (n > 5 ? " (+" + (n - 5) + ")" : "");
        }

        // SQLite-Backup-Schnittstelle: ganze Datenbank "main" von quelle nach ziel (backup_init, step(-1), finish); null = ok
        public static string Sichern(IntPtr quelle, IntPtr ziel)
        {
            IntPtr b = sqlite3_backup_init(ziel, Z("main"), quelle, Z("main"));
            if (b == IntPtr.Zero) return "sqlite3_backup_init: " + Fehlertext(ziel);
            int schritt = sqlite3_backup_step(b, -1);
            int ende = sqlite3_backup_finish(b);
            if (schritt != DONE) return "sqlite3_backup_step " + schritt + ": " + Fehlertext(ziel);
            if (ende != OK) return "sqlite3_backup_finish " + ende + ": " + Fehlertext(ziel);
            return null;
        }
    }
}
