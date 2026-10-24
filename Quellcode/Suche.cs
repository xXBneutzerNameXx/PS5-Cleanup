// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// Suche.cs - Namen lesen (param.json/param.sfo, Galerie, Trophäen) und
// die Suche selbst (Suche-Funde). 1:1 nach ps5_kern.ps1, neu in 1.0.4:
// die Ordner auf der M.2 werden aufgeklappt - jedes Spiel und jede Datei
// darin ist ein eigener Eintrag.
// =====================================================================
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace PS5Cleanup
{
    public partial class Kern
    {
        public static string[] KatName { get { return new[] {
            "",
            T("Spiele & Apps (interne SSD)", "Games & apps (internal SSD)"),
            T("Spielstände", "Saved data"),
            T("Trophäen", "Trophies"),
            T("Bilder & Videos (Aufnahmen)", "Screenshots & videos"),
            T("Downloads, Zusatzinhalte & Reste", "Downloads, add-ons & leftovers"),
            T("Jailbreak-Tools & Homebrew-Daten", "Jailbreak tools & homebrew data"),
            T("Inhalt der M.2-SSD", "M.2 SSD contents") }; } }
        public static string[] KatInfo { get { return new[] {
            "",
            T("Installierte Spiele und Apps. Wer ein Spiel wählt, nimmt Update und DLCs mit.", "Installed games and apps. Selecting a game takes its update and DLC with it."),
            T("Spielstände auf der Konsole. Ohne Cloud-Sicherung (PS Plus) endgültig weg.", "Saved data on the console. Without a cloud backup (PS Plus) it is gone for good."),
            T("Trophäen-Fortschritt und Trophäen-Listen. Nie mit dem PSN abgeglichene Trophäen sind endgültig weg.", "Trophy progress and trophy lists. Trophies never synced with PSN are gone for good."),
            T("Screenshots und Videoclips, je Spiel mit Anzahl.", "Screenshots and video clips, per game with count."),
            T("Downloads, Zusatzinhalte und Updates ohne Spiel, Reste gelöschter Spiele.", "Downloads, add-ons and updates without a game, leftovers of deleted games."),
            T("Daten und Kacheln der Jailbreak-Tools, Homebrew-Daten. Danach Jailbreak neu einrichten.", "Data and tiles of jailbreak tools, homebrew data. Set up your jailbreak again afterwards."),
            T("Jedes Spiel und jede Datei auf der M.2 einzeln. Komplett leeren: an der PS5 unter Speicher > M.2-SSD > Formatieren.", "Every game and file on the M.2 individually. To wipe it completely: on the PS5 under Storage > M.2 SSD > Format.") }; } }
        public static readonly int[] ALLE_KAT = { 1, 2, 3, 4, 5, 6, 7 };

        public List<Eintrag> Funde = new List<Eintrag>();
        public bool m2Da;
        public string FUNDE_IP = "";
        public List<string> KOMPRIMIERUNG = new List<string>();
        public List<string> AUFGEKLAPPT = new List<string>();
        public Dictionary<string, List<string>> LEER_DANACH = H.Tab<List<string>>();
        List<string> SHARES = new List<string>();
        bool MERKE_OK;
        List<Eintrag> bau = new List<Eintrag>();

        // ---------------------------------------------------------------
        // Namen
        // ---------------------------------------------------------------
        static object HoleCI(object o, string schluessel)
        {
            var d = o as Dictionary<string, object>;
            if (d == null || schluessel == null) return null;
            foreach (var kv in d) if (string.Equals(kv.Key, schluessel, StringComparison.OrdinalIgnoreCase)) return kv.Value;
            return null;
        }
        static string Str(object o) { return o == null ? "" : Convert.ToString(o, System.Globalization.CultureInfo.InvariantCulture); }

        // Titel aus einer param.json (PS5): in der gewählten Sprache, sonst die andere, sonst Standard-Name.
        public static string JsonTitel(string t)
        {
            t = (t ?? "").TrimStart('﻿');
            if (!H.M(t, "titleName")) return "";
            try
            {
                var o = new JavaScriptSerializer { MaxJsonLength = int.MaxValue }.DeserializeObject(t);
                var lp = HoleCI(o, "localizedParameters");
                if (lp != null)
                {
                    foreach (var s in (Sp.EN ? new[] { "en-US", "en-GB", "de-DE" } : new[] { "de-DE", "en-US", "en-GB" }).Concat(new[] { Str(HoleCI(lp, "defaultLanguage")) }))
                    {
                        if (s == "") continue;
                        var sp = HoleCI(lp, s);
                        if (sp != null && Str(HoleCI(sp, "titleName")) != "") return Str(HoleCI(sp, "titleName"));
                    }
                }
            }
            catch { }
            var m = H.Mt(t, "\"titleName\"\\s*:\\s*\"([^\"]*)\"");
            if (m.Success) return m.Groups[1].Value;
            return "";
        }

        // param.sfo (PS4-Format) lesen
        public static Dictionary<string, object> SfoLesen(byte[] b)
        {
            var h = H.Tab<object>();
            if (b == null || b.Length < 20 || b[0] != 0 || b[1] != 0x50 || b[2] != 0x53 || b[3] != 0x46) return h;
            try
            {
                uint keyTab = BitConverter.ToUInt32(b, 8), datTab = BitConverter.ToUInt32(b, 12), n = BitConverter.ToUInt32(b, 16);
                for (int i = 0; i < n && i < 200; i++)
                {
                    int e = 20 + 16 * i;
                    ushort kOff = BitConverter.ToUInt16(b, e), fmt = BitConverter.ToUInt16(b, e + 2);
                    uint len = BitConverter.ToUInt32(b, e + 4), dOff = BitConverter.ToUInt32(b, e + 12);
                    int ks = (int)(keyTab + kOff);
                    int ke = Array.IndexOf(b, (byte)0, ks);
                    if (ke < 0) continue;
                    string key = Encoding.ASCII.GetString(b, ks, ke - ks);
                    int ds = (int)(datTab + dOff);
                    if (fmt == 0x0404 && len >= 4) h[key] = BitConverter.ToUInt32(b, ds);
                    else h[key] = Ftp.UTF8.GetString(b, ds, (int)len).TrimEnd('\0');
                }
            }
            catch { }
            return h;
        }
        public static string SfoTitel(Dictionary<string, object> h)
        {
            foreach (var k in new[] { "TITLE_04", "TITLE_01", "TITLE" }) if (h.ContainsKey(k) && Str(h[k]) != "") return Str(h[k]);
            return "";
        }

        class TitelInfo { public string T; public string Roh; }
        Dictionary<string, TitelInfo> TITEL_CACHE = H.Tab<TitelInfo>();
        string APP_ROH = "";
        string AppTitel(string id)
        {
            TitelInfo c;
            if (TITEL_CACHE.TryGetValue(id, out c)) { APP_ROH = c.Roh; return c.T; }
            string t = AppTitelLesen(id);
            TITEL_CACHE[id] = new TitelInfo { T = t, Roh = APP_ROH };
            return t;
        }
        string AppTitelLesen(string id)
        {
            APP_ROH = "";
            foreach (var datei in new[] { "/user/appmeta/" + id + "/param.json", "/user/app/" + id + "/sce_sys/param.json", SYS_META + "/" + id + "/param.json" })
            {
                string t = S.Text(datei).TrimStart('﻿');
                if (!H.M(t, "titleName")) continue;
                APP_ROH = t;
                string n = JsonTitel(t);
                if (n != "") return n;
            }
            // PS4-Spiele haben statt param.json eine param.sfo
            foreach (var datei in new[] { "/user/appmeta/" + id + "/param.sfo", "/user/app/" + id + "/sce_sys/param.sfo", SYS_META + "/" + id + "/param.sfo" })
            {
                string n = SfoTitel(SfoLesen(S.Bytes(datei)));
                if (n != "") { APP_ROH = n; return n; }
            }
            return "";
        }
        // Hängt bei Ordnern, die nach einer Spiel-ID benannt sind, den Namen an.
        Dictionary<string, string> NAMEN_CACHE = H.Tab<string>();
        string MitName(string x)
        {
            var m = H.Mt(x, "^([A-Z]{4}\\d{5})");
            if (!m.Success) return x;
            string id = m.Groups[1].Value;
            if (!NAMEN_CACHE.ContainsKey(id)) NAMEN_CACHE[id] = AppTitel(id);
            string t = NAMEN_CACHE[id];
            if (t == "") return x;
            return x + " (" + t + ")";
        }
        Dictionary<string, string> NPWR_NAMEN = H.Tab<string>();
        Dictionary<string, string> NPWR_GALERIE = H.Tab<string>();
        string TrophaenName(string npwr)
        {
            if (NPWR_NAMEN.ContainsKey(npwr) && NPWR_NAMEN[npwr] != "") return NPWR_NAMEN[npwr] + " (" + npwr + ")";
            if (NPWR_GALERIE.ContainsKey(npwr)) return NPWR_GALERIE[npwr] + " (" + npwr + ")";
            return npwr;
        }
        string TrophaenWeg(string npwr) { return NPWR_NAMEN.ContainsKey(npwr) ? "" : T(" - Spiel nicht mehr auf der Konsole", " - game no longer on the console"); }

        const string MEDIEN = "\\.(jxr|jpe?g|png|mp4|webm|mov)$";
        int ZaehleMedien(string p)
        {
            int n = 0;
            var l = S.Liste(p);
            if (l == null) return 0;
            foreach (var e in l)
            {
                if (e.IstOrdner)
                {
                    var u = S.Liste(p + "/" + e.Name);
                    if (u != null) foreach (var d in u) if (!d.IstOrdner && H.M(d.Name, MEDIEN)) n++;
                }
                else if (H.M(e.Name, MEDIEN)) n++;
            }
            return n;
        }
        // Name eines Spiels in der Mediengalerie: erst die Kachel-Infos, sonst die
        // Begleitdatei (.dat) einer Aufnahme - darin steht der Spielname im Klartext.
        Dictionary<string, string> GALERIE_NAMEN = H.Tab<string>();
        string GalerieName(string id, List<string> ordner = null)
        {
            if (H.Like(id, "NPXS*")) return T("Systemmenü (", "System menu (") + id + ")";
            string m = MitName(id);
            if (m != id) return H.R(m, "^[A-Z]{4}\\d{5} \\((.*)\\)$", "$1") + " (" + id + ")";
            if (!GALERIE_NAMEN.ContainsKey(id))
            {
                GALERIE_NAMEN[id] = "";
                bool fertig = false;
                foreach (var o in (ordner ?? new List<string>()))
                {
                    if (fertig) break;
                    var l1 = S.Liste(o); if (l1 == null) continue;
                    foreach (var e in l1)
                    {
                        if (fertig) break;
                        if (!e.IstOrdner) continue;
                        var l2 = S.Liste(o + "/" + e.Name); if (l2 == null) continue;
                        foreach (var d in l2)
                        {
                            if (d.IstOrdner || !H.Like(d.Name, "*.dat")) continue;
                            byte[] b = S.Bytes(o + "/" + e.Name + "/" + d.Name);
                            if (b == null || b.Length < 8) continue;
                            // nach dem 8-Byte-Kopf: erster Klartext bis zum nächsten Nullbyte = Spielname
                            int start = 8; while (start < b.Length && b[start] < 32) start++;
                            if (start >= b.Length) continue;
                            int ende = Array.IndexOf(b, (byte)0, start); if (ende < 0) ende = b.Length;
                            string t = H.R(Ftp.UTF8.GetString(b, start, ende - start), "_\\d{14}.*$", "").Trim();
                            if (t.Length < 2 || t.Length > 100) continue;
                            GALERIE_NAMEN[id] = t;
                            // Begleitdatei .ext nennt die Trophäen-Kennung -> Name auch für Bereich 3
                            string ext = S.Text(o + "/" + e.Name + "/" + H.R(d.Name, "\\.dat$", ".ext"));
                            var nm = H.Mt(ext, "NPWR\\d{5}_\\d{2}");
                            if (nm.Success) NPWR_GALERIE[nm.Value] = t;
                            fertig = true;
                            break;
                        }
                    }
                }
            }
            if (GALERIE_NAMEN[id] != "") return GALERIE_NAMEN[id] + " (" + id + T(", Spiel nicht mehr auf der Konsole)", ", game no longer on the console)");
            return id + T(" (Spiel nicht mehr auf der Konsole)", " (game no longer on the console)");
        }
        bool IstAstro(string t) { return H.M((t + " " + APP_ROH).ToUpperInvariant(), "ASTRO.*PLAYROOM"); }
        void AstroMerken(string id) { if (!H.Hat(ASTRO_IDS, id)) ASTRO_IDS.Add(id); }

        // ---------------------------------------------------------------
        // Einträge merken
        // ---------------------------------------------------------------
        void Merke(IEnumerable<string> pfade, int kat, string text, string quelle = "", bool gesperrt = false, bool kachel = false, bool smTipp = false)
        {
            // Astro bleibt immer - seine Pfade werden gar nicht erst angeboten
            var p = pfade.Where(x => !AstroPfad(x)).ToList();
            MERKE_OK = p.Count > 0;
            if (!MERKE_OK) return;
            if (gesperrt) text += GESPERRT_TEXT;
            bau.Add(new Eintrag { Pfade = p, PfadeBasis = new List<string>(p), Kat = kat, Text = text, TextBasis = text, Quelle = quelle ?? "", Gesperrt = gesperrt, Kachel = kachel, SmTipp = smTipp });
        }
        void MerkeTeil(IEnumerable<string> pfade, int kat, string text, string gruppe, string teil)
        {
            Merke(pfade, kat, text);
            if (!MERKE_OK) return;
            var f = bau[bau.Count - 1];
            f.Gruppe = gruppe; f.Teil = teil;
        }
        public static string GESPERRT_TEXT { get { return T("  [GESPERRT: Game Compressor arbeitet gerade daran]", "  [LOCKED: Game Compressor is working on it]"); } }
        static List<string> L(params string[] p) { return new List<string>(p); }

        // Version eines Updates: PS5 aus param.json (contentVersion), PS4 aus param.sfo (APP_VER)
        string PatchVersion(string id, string basis)
        {
            var m = H.Mt(S.Text(basis + "/patch/" + id + "/sce_sys/param.json"), "\"contentVersion\"\\s*:\\s*\"([^\"]+)\"");
            if (m.Success) return " (Version " + m.Groups[1].Value + ")";
            var h = SfoLesen(S.Bytes(basis + "/patch/" + id + "/sce_sys/param.sfo"));
            if (h.ContainsKey("APP_VER") && Str(h["APP_VER"]) != "") return " (Version " + Str(h["APP_VER"]) + ")";
            m = H.Mt(S.Text(SYS_META + "/" + id + "/param.json"), "\"contentVersion\"\\s*:\\s*\"([^\"]+)\"");
            if (m.Success) return " (Version " + m.Groups[1].Value + ")";
            foreach (var d in new[] { SYS_META + "/" + id + "/param.sfo", "/user/appmeta/" + id + "/param.sfo" })
            {
                h = SfoLesen(S.Bytes(d));
                if (h.ContainsKey("APP_VER") && Str(h["APP_VER"]) != "") return " (Version " + Str(h["APP_VER"]) + ")";
            }
            return "";
        }
        string DlcName(string id, string dlc, string basis)
        {
            foreach (var d in new[] { "/user/appmeta/addcont/" + id + "/" + dlc + "/param.json", basis + "/addcont/" + id + "/" + dlc + "/sce_sys/param.json" })
            { string n = JsonTitel(S.Text(d)); if (n != "") return n; }
            foreach (var d in new[] { "/user/appmeta/addcont/" + id + "/" + dlc + "/param.sfo", basis + "/addcont/" + id + "/" + dlc + "/sce_sys/param.sfo" })
            { string n = SfoTitel(SfoLesen(S.Bytes(d))); if (n != "") return n; }
            return dlc;
        }
        // Spiel, Update und Zusatzinhalte eines normal installierten Spiels als eigene Einträge
        void MerkeSpielteile(List<string> spielPfade, int kat, string name, string id, string basis,
                             bool hatUpdate, bool hatDlc, List<string> dlcInfos, Dictionary<string, int> benutzt, bool zwilling)
        {
            spielPfade = new List<string>(spielPfade);
            if (H.Hat(SHARES, id) && !zwilling) spielPfade.Add("/user/system/share/" + id);
            string gruppe = basis + "|" + id;
            MerkeTeil(spielPfade, kat, name + " (" + id + ")" + T(" - Spiel", " - game"), gruppe, "Spiel");
            if (hatUpdate)
            {
                benutzt[basis + "/patch/" + id] = 1;
                MerkeTeil(L(basis + "/patch/" + id), kat, name + " (" + id + ") - Update" + PatchVersion(id, basis), gruppe, "Update");
            }
            if (hatDlc)
            {
                benutzt[basis + "/addcont/" + id] = 1;
                var leer = L(basis + "/addcont/" + id);
                var meta = new List<string>();
                if (H.Hat(dlcInfos, id))
                {
                    benutzt["/user/appmeta/addcont/" + id] = 1;
                    if (!zwilling) { leer.Add("/user/appmeta/addcont/" + id); meta = S.Namen("/user/appmeta/addcont/" + id); }
                }
                LEER_DANACH[gruppe] = leer;
                foreach (var d in S.Namen(basis + "/addcont/" + id))
                {
                    var dp = L(basis + "/addcont/" + id + "/" + d);
                    if (H.Hat(meta, d)) dp.Add("/user/appmeta/addcont/" + id + "/" + d);
                    MerkeTeil(dp, kat, name + " (" + id + ")" + T(" - Zusatzinhalt: ", " - add-on: ") + DlcName(id, d, basis), gruppe, "DLC");
                }
            }
        }

        // Hakt man ein Spiel an, gehen sein Update und seine Zusatzinhalte automatisch mit.
        public void AbhaengigeMitnehmen()
        {
            var basis = H.Tab<int>();
            foreach (var f in Funde) if (f.Wahl && f.IstTeil("Spiel")) basis[f.Gruppe] = 1;
            foreach (var f in Funde)
                if (f.Teil != null && f.Teil != "Spiel" && f.Gruppe != null && basis.ContainsKey(f.Gruppe) && !f.Gesperrt) f.Wahl = true;
        }

        // ---------------------------------------------------------------
        // ShadowMount-Hilfen der Suche
        // ---------------------------------------------------------------
        class QInfo { public List<string> Links; public string Name; public bool Gesperrt; }
        List<string> SpielQuellen;
        Dictionary<string, QInfo> QuelleInfo;
        Dictionary<string, List<string>> ORDNER_CACHE;

        string SmQuelle(string id)
        {
            foreach (var lnk in new[] { "mount_img.lnk", "mount.lnk" })
            {
                foreach (string z0 in Regex.Split(S.Text("/user/app/" + id + "/" + lnk), "\r?\n"))
                {
                    string z = z0.Trim();
                    if (z == "" || H.Like(z, "/mnt/shadowmnt*")) continue;
                    if (H.Like(z, "/data/*")) z = "/user" + z;
                    return z;
                }
            }
            return "";
        }
        static string QuelleOrt(string q)
        {
            if (H.Steuerzeichen(q)) return "";
            string m = Regex.Escape(M2);
            if (H.M(q, "^" + m + "/(user|system_data|lost\\+found)(/|$)")) return "";
            if (H.M(q, "^" + m + "/[^/]+")) return "M.2";
            if (H.M(q, "^/user/data/[^/]+")) return "intern";
            return "";
        }
        static bool IstZwischendatei(string name) { return H.M(name, "\\.gc-compress\\.tmp"); }
        bool WirdKomprimiert(string q, string id)
        {
            string eltern = q.Substring(0, q.LastIndexOf('/'));
            string name = q.Substring(q.LastIndexOf('/') + 1);
            string basis = H.R(name, "\\.[^.]+$", "");
            if (!ORDNER_CACHE.ContainsKey(eltern)) ORDNER_CACHE[eltern] = S.Namen(eltern, true);
            foreach (var n in ORDNER_CACHE[eltern])
            {
                if (!IstZwischendatei(n)) continue;
                if (H.Like(n, "*" + id + "*") || n.TrimStart('.').StartsWith(basis, StringComparison.Ordinal)) return true;
            }
            return false;
        }

        // Listet einen Ordner auf. Unterordner mit ShadowMount-Spielen werden
        // aufgeklappt; in Verknüpfungen (Symlinks) wird nie hineingeschaut.
        void MerkeOrdner(string basis, IEnumerable<Datei> eintraege, int kat, string vorsatz)
        {
            foreach (var e in eintraege)
            {
                string x = e.Name; bool link = e.IstLink;
                string p = basis + "/" + x;
                if (IstZwischendatei(x)) { KOMPRIMIERUNG.Add(p); continue; }
                if (H.Hat(SpielQuellen, p))
                {
                    var i = QuelleInfo[p];
                    var pf = L(p); pf.AddRange(i.Links);
                    Merke(pf, kat, vorsatz + x + T("  [Spiel: ", "  [game: ") + i.Name + T(", Kachel geht mit]", ", tile goes with it]"), p, i.Gesperrt, kachel: true, smTipp: true);
                    continue;
                }
                bool drin = SpielQuellen.Any(q => q.StartsWith(p + "/", StringComparison.Ordinal));
                if (drin && !link)
                {
                    AUFGEKLAPPT.Add(p);
                    var unter = S.Liste(p, true) ?? new List<Datei>();
                    MerkeOrdner(p, unter, kat, vorsatz + x + "/");
                }
                else if (link) Merke(L(p), kat, vorsatz + x + T("  (Verknüpfung, nur sie wird gelöscht)", "  (symlink, only the link is deleted)"));
                else Merke(L(p), kat, vorsatz + x);
            }
        }

        // NEU 1.0.4: Ordner direkt auf der M.2 werden aufgeklappt - jedes Spiel,
        // jeder Unterordner und jede Datei darin ist ein eigener Eintrag. Der
        // Ordner selbst bleibt immer stehen (nur ein leerer wird angeboten).
        void MerkeM2Ordner(Datei e)
        {
            string p = M2 + "/" + e.Name;
            if (!e.IstOrdner || e.IstLink || IstZwischendatei(e.Name) || H.Hat(SpielQuellen, p)) { MerkeOrdner(M2, new[] { e }, 7, ""); return; }
            var unter = S.Liste(p, true);
            // nicht lesbar: wie bisher als Ganzes anbieten (nie als "leer" ausgeben)
            if (unter == null) { MerkeOrdner(M2, new[] { e }, 7, ""); return; }
            if (unter.Count == 0)
            {
                Merke(L(p), 7, e.Name + T("  (leerer Ordner)", "  (empty folder)"));
                if (MERKE_OK) bau[bau.Count - 1].NurLeer = true;
                return;
            }
            MerkeOrdner(p, unter, 7, e.Name + "/");
        }

        // ---------------------------------------------------------------
        // Die Suche
        // ---------------------------------------------------------------
        public void SucheFunde()
        {
            FUNDE_IP = "";                    // erst gültig, wenn die Suche ganz durch ist
            bau = new List<Eintrag>();        // die neue Liste wird erst am Ende sichtbar
            LEER_DANACH = H.Tab<List<string>>();
            GroessenZuruecksetzen();
            // Diese Listen entscheiden, was als "nicht mehr installiert" gilt - darum
            // streng: scheitert eine, bricht die Suche ab, statt Falsches anzubieten.
            SHARES = S.Namen("/user/system/share", true);
            var homes = S.Namen("/user/home", true);
            var appmeta = S.Namen("/user/appmeta", true);
            var apps = S.Namen("/user/app", true);
            var patches = S.Namen("/user/patch", true);
            var addconts = S.Namen("/user/addcont", true);
            var dlcInfos = S.Namen("/user/appmeta/addcont", true);
            var m2Liste = S.Liste(M2, true);
            m2Da = m2Liste != null;
            var m2Apps = new List<string>(); var m2Patches = new List<string>(); var m2Addconts = new List<string>();
            if (m2Da) { m2Apps = S.Namen(M2 + "/user/app", true); m2Patches = S.Namen(M2 + "/user/patch", true); m2Addconts = S.Namen(M2 + "/user/addcont", true); }
            var benutzt = H.Tab<int>();
            var sysmeta = S.Namen(SYS_META, true);
            Func<List<string>, string, List<string>> mitKachel = (pf, id) =>
            {
                if (H.Hat(sysmeta, id) && !H.Like(id, "NPXS*") && !H.Hat(ASTRO_IDS, id)) { var n = new List<string>(pf); n.Add(SYS_META + "/" + id); return n; }
                return pf;
            };
            // Namen aller Kacheln zuerst lesen - dabei wird Astro unter jeder
            // Länderkennung erkannt, bevor irgendetwas anderes gelistet wird.
            ASTRO_IDS = L(BEHALTEN_APP); ASTRO_NPWRS = L("NPWR20188_00");
            NAMEN_CACHE = H.Tab<string>(); TITEL_CACHE = H.Tab<TitelInfo>();
            GALERIE_NAMEN = H.Tab<string>(); NPWR_GALERIE = H.Tab<string>();
            var alle = new List<string>();
            foreach (var x in apps.Concat(m2Apps).Concat(appmeta).Concat(sysmeta)) if (!alle.Contains(x)) alle.Add(x);
            foreach (var id in alle)
            {
                if (!H.M(id, "^[A-Z]{4}\\d{5}$") || H.Like(id, "NPXS*")) continue;
                if (IstAstro(AppTitel(id))) AstroMerken(id);
            }
            // Zuordnung Trophäen-Kennung (NPWR...) -> Spielname über die Kachel-Infos
            NPWR_NAMEN = H.Tab<string>();
            foreach (var id in sysmeta)
            {
                if (!H.M(id, "^[A-Z]{4}\\d{5}$")) continue;
                foreach (var sub in new[] { "trophy2", "trophy" })
                {
                    string nb = S.Text(SYS_META + "/" + id + "/" + sub + "/npbind.dat");
                    var ms = Regex.Matches(nb, "NPWR\\d{5}_\\d{2}");
                    if (ms.Count == 0) continue;
                    if (!NAMEN_CACHE.ContainsKey(id)) { NAMEN_CACHE[id] = AppTitel(id); if (IstAstro(NAMEN_CACHE[id])) AstroMerken(id); }
                    if (H.Hat(ASTRO_IDS, id)) foreach (Match m in ms) if (!H.Hat(ASTRO_NPWRS, m.Value)) ASTRO_NPWRS.Add(m.Value);
                    foreach (Match m in ms) if (!NPWR_NAMEN.ContainsKey(m.Value)) NPWR_NAMEN[m.Value] = NAMEN_CACHE[id];
                    break;
                }
            }
            KOMPRIMIERUNG = new List<string>();
            AUFGEKLAPPT = new List<string>();

            // ---- ShadowMount ------------------------------------------------
            SpielQuellen = new List<string>();
            QuelleInfo = H.Tab<QInfo>();
            ORDNER_CACHE = H.Tab<List<string>>();

            // ---- 1 Spiele & Apps (und Tool-Kacheln -> 6) --------------------
            foreach (var x in apps)
            {
                if (H.Hat(ASTRO_IDS, x) || H.Like(x, "NPXS*")) continue;
                string t = AppTitel(x);
                if (IstAstro(t)) { AstroMerken(x); continue; }
                string roh = APP_ROH;
                if (t == "") t = T("unbekannter Name", "unknown name");
                var p = L("/user/app/" + x);
                bool zwilling = H.Hat(m2Apps, x);
                if (H.Hat(appmeta, x)) { benutzt["/user/appmeta/" + x] = 1; if (!zwilling) p.Add("/user/appmeta/" + x); }
                if (!zwilling) p = mitKachel(p, x);
                var inhalt = S.Namen("/user/app/" + x, true);
                if (H.M(roh, "\"deeplinkUri\"\\s*:\\s*\"https?://(127\\.0\\.0\\.1|localhost)"))
                {
                    Merke(p, 6, T("Tool-Kachel: ", "Tool tile: ") + t + " (" + x + ")", kachel: true);
                }
                else if (H.Hat(inhalt, "mount.lnk") || H.Hat(inhalt, "mount_img.lnk"))
                {
                    string q = SmQuelle(x);
                    string ort = QuelleOrt(q);
                    bool da = false;
                    if (ort != "") da = S.Existiert(q);
                    if (ort != "" && da)
                    {
                        bool g = WirdKomprimiert(q, x);
                        SpielQuellen.Add(q);
                        QuelleInfo[q] = new QInfo { Links = p, Name = t + " (" + x + ")", Gesperrt = g };
                        Merke(p, 1, t + " (" + x + ")" + T(" - ShadowMount-Spiel auf der ", " - ShadowMount game on the ") + (ort == "M.2" ? "M.2" : T("internen SSD", "internal SSD")), q, g, kachel: true);
                    }
                    else if (ort != "") Merke(p, 1, t + " (" + x + ")" + T(" - ShadowMount: Spiel nicht mehr da, tote Kachel", " - ShadowMount: game gone, dead tile"), kachel: true, smTipp: true);
                    else
                    {
                        string wo = q == "" ? T("Speicherort unbekannt", "location unknown") : T("Spiel liegt auf ", "game is at ") + q;
                        Merke(p, 1, t + " (" + x + ") - ShadowMount: " + wo + T(", nur die Kachel wird gelöscht", ", only the tile is deleted"), kachel: true);
                    }
                }
                else
                {
                    MerkeSpielteile(p, 1, t, x, "/user", H.Hat(patches, x), H.Hat(addconts, x), dlcInfos, benutzt, zwilling);
                }
            }

            // ---- 2 Spielstände --------------------------------------------
            foreach (var u in homes)
                foreach (var sd in new[] { "savedata", "savedata_prospero" })
                    foreach (var x in S.Namen("/user/home/" + u + "/" + sd))
                    {
                        if (H.Hat(ASTRO_IDS, x) || H.Like(x, "sce_*") || H.Like(x, "NPXS*")) continue;
                        Merke(L("/user/home/" + u + "/" + sd + "/" + x), 2, T("Spielstand ", "Saved data ") + MitName(x));
                    }

            // ---- 4 Bilder & Videos -----------------------------------------
            var galerieReihe = new List<string>();
            var gPfade = H.Tab<List<string>>(); var gBilder = H.Tab<int>(); var gVideos = H.Tab<int>();
            foreach (var art in new[] { "photo", "video" })
                foreach (var app in S.Namen("/user/av_contents/" + art))
                    foreach (var id in S.Namen("/user/av_contents/" + art + "/" + app))
                    {
                        if (!gPfade.ContainsKey(id)) { galerieReihe.Add(id); gPfade[id] = new List<string>(); gBilder[id] = 0; gVideos[id] = 0; }
                        gPfade[id].Add("/user/av_contents/" + art + "/" + app + "/" + id);
                        int n = ZaehleMedien("/user/av_contents/" + art + "/" + app + "/" + id);
                        if (art == "photo") gBilder[id] += n; else gVideos[id] += n;
                    }
            var vorschauReste = new List<string>();
            foreach (var art in S.Namen("/user/av_contents/thumbnails"))
                foreach (var app in S.Namen("/user/av_contents/thumbnails/" + art))
                    foreach (var id in S.Namen("/user/av_contents/thumbnails/" + art + "/" + app))
                    {
                        string pf = "/user/av_contents/thumbnails/" + art + "/" + app + "/" + id;
                        if (gPfade.ContainsKey(id)) gPfade[id].Add(pf); else vorschauReste.Add(pf);
                    }
            foreach (var id in galerieReihe)
            {
                var teile = new List<string>();
                if (gBilder[id] > 0) teile.Add(gBilder[id] + " Screenshot" + (gBilder[id] != 1 ? "s" : ""));
                if (gVideos[id] > 0) teile.Add(gVideos[id] + " Video" + (gVideos[id] != 1 ? "s" : ""));
                string was = teile.Count > 0 ? string.Join(", ", teile) : T("leerer Ordner", "empty folder");
                Merke(gPfade[id], 4, T("Aufnahmen von ", "Captures of ") + GalerieName(id, gPfade[id]) + ": " + was);
            }
            if (vorschauReste.Count > 0) Merke(vorschauReste, 4, T("Vorschaubilder ohne Aufnahme (", "Thumbnails without capture (") + vorschauReste.Count + T(") - Reste", ") - leftovers"));
            var leere = new List<string>();
            foreach (var id in S.Namen("/user/av_contents/content_tmp"))
            {
                string pf = "/user/av_contents/content_tmp/" + id;
                int n = S.Namen(pf).Count;
                if (n == 0) leere.Add(pf);
                else Merke(L(pf), 4, T("Unfertige Aufnahme von ", "Unfinished capture of ") + GalerieName(id) + " (" + n + (n != 1 ? T(" Dateien", " files") : T(" Datei", " file")) + ")");
            }
            if (leere.Count > 0) Merke(leere, 4, T("Leere Aufnahme-Zwischenordner (", "Empty capture temp folders (") + leere.Count + T(") - Reste", ") - leftovers"));
            foreach (var s in S.Namen("/user/av_contents"))
            {
                if (H.Hat(new[] { "photo", "video", "thumbnails", "content_tmp" }, s)) continue;
                foreach (var x in S.Namen("/user/av_contents/" + s)) Merke(L("/user/av_contents/" + s + "/" + x), 4, T("Weitere Galerie-Daten: ", "Other gallery data: ") + s + "/" + MitName(x));
            }

            // ---- 3 Trophäen ------------------------------------------------
            foreach (var u in homes)
            {
                foreach (var x in S.Namen("/user/home/" + u + "/trophy/data"))
                {
                    if (H.Hat(ASTRO_NPWRS, x)) continue;
                    string t;
                    if (H.Gleich(x, "sce_trop")) t = T("Trophäen-Gesamtzähler (Level, Bronze/Silber/Gold - baut die PS5 neu auf)", "Trophy totals (level, bronze/silver/gold - rebuilt by the PS5)");
                    else if (H.M(x, "^NPWR")) t = T("Trophäen-Fortschritt: ", "Trophy progress: ") + TrophaenName(x) + T(" (PS4-Spiel)", " (PS4 game)") + TrophaenWeg(x);
                    else t = T("Trophäen-Daten ", "Trophy data ") + "trophy/data/" + x;
                    Merke(L("/user/home/" + u + "/trophy/data/" + x), 3, t);
                }
                foreach (var x in S.Namen("/user/home/" + u + "/trophy"))
                    if (!H.Gleich(x, "data")) Merke(L("/user/home/" + u + "/trophy/" + x), 3, T("Trophäen-Daten ", "Trophy data ") + "trophy/" + x);
                foreach (var x in S.Namen("/user/home/" + u + "/trophy2/nobackup/data"))
                {
                    if (H.Hat(ASTRO_NPWRS, x)) continue;
                    string t = H.M(x, "^NPWR") ? T("Trophäen-Fortschritt: ", "Trophy progress: ") + TrophaenName(x) + TrophaenWeg(x) : T("Trophäen-Daten ", "Trophy data ") + "trophy2/nobackup/data/" + x;
                    Merke(L("/user/home/" + u + "/trophy2/nobackup/data/" + x), 3, t);
                }
                foreach (var x in S.Namen("/user/home/" + u + "/trophy2"))
                {
                    if (H.Gleich(x, "nobackup"))
                    {
                        foreach (var y in S.Namen("/user/home/" + u + "/trophy2/nobackup"))
                            if (!H.Gleich(y, "data")) Merke(L("/user/home/" + u + "/trophy2/nobackup/" + y), 3, T("Trophäen-Daten ", "Trophy data ") + "trophy2/nobackup/" + y);
                    }
                    else Merke(L("/user/home/" + u + "/trophy2/" + x), 3, T("Trophäen-Daten ", "Trophy data ") + "trophy2/" + x);
                }
            }
            var pakete = H.Tab<List<string>>();
            foreach (var o in new[] { "/user/trophy2/nobackup/conf", "/user/trophy/conf", "/user/np_uds/nobackup/conf" })
                foreach (var x in S.Namen(o))
                {
                    if (!H.M(x, "^NPWR\\d{5}_\\d{2}$") || H.Hat(ASTRO_NPWRS, x)) continue;
                    if (!pakete.ContainsKey(x)) pakete[x] = new List<string>();
                    pakete[x].Add(o + "/" + x);
                }
            foreach (var k in pakete.Keys.OrderBy(k => k, StringComparer.CurrentCultureIgnoreCase).ToList())
            {
                if (NPWR_NAMEN.ContainsKey(k))
                {
                    Merke(pakete[k], 3, T("Trophäen-Liste: ", "Trophy list: ") + TrophaenName(k) + T(" - Spiel noch da, nur mit dem Spiel löschen (Namen/Bilder, nicht dein Fortschritt)", " - game still installed, delete only with the game (names/images, not your progress)"), "", false);
                    if (MERKE_OK) bau[bau.Count - 1].Paket = true;
                }
                else Merke(pakete[k], 3, T("Trophäen-Liste: ", "Trophy list: ") + TrophaenName(k) + T(" - Spiel nicht mehr da, kann weg (Namen/Bilder, nicht dein Fortschritt)", " - game removed, can go (names/images, not your progress)"));
            }

            // ---- 5 Downloads, Zusatzinhalte & Reste -------------------------
            foreach (var x in S.Namen("/user/download"))
            {
                if (H.Like(x, "NPXS*")) continue;
                Merke(L("/user/download/" + x), 5, "Download: " + MitName(x));
            }
            foreach (var x in patches) if (!benutzt.ContainsKey("/user/patch/" + x) && !H.Like(x, "NPXS*")) Merke(L("/user/patch/" + x), 5, T("Update ohne Spiel: ", "Update without game: ") + MitName(x));
            foreach (var x in addconts) if (!benutzt.ContainsKey("/user/addcont/" + x) && !H.Like(x, "NPXS*")) Merke(L("/user/addcont/" + x), 5, T("Zusatzinhalt: ", "Add-on: ") + MitName(x));
            foreach (var x in S.Namen("/user/appmeta/external/addcont")) Merke(L("/user/appmeta/external/addcont/" + x), 5, T("Zusatzinhalt-Infos: ", "Add-on info: ") + MitName(x));
            foreach (var x in S.Namen("/user"))
                if (H.Like(x, "_zu_loeschen_*")) Merke(L("/user/" + x), 5, T("Rest einer früheren Aufräumaktion: ", "Leftover from an earlier cleanup: ") + "/user/" + x);
            if (m2Da)
                foreach (var e in m2Liste)
                    if (H.Like(e.Name, "_zu_loeschen_*")) Merke(L(M2 + "/" + e.Name), 7, T("Rest einer früheren Aufräumaktion: ", "Leftover from an earlier cleanup: ") + M2 + "/" + e.Name);

            // ---- 7 M.2 -------------------------------------------------------
            if (m2Da)
            {
                foreach (var x in m2Apps)
                {
                    if (H.Like(x, "NPXS*") || H.Hat(ASTRO_IDS, x)) continue;
                    string t = AppTitel(x);
                    if (IstAstro(t)) { AstroMerken(x); continue; }
                    if (t == "") t = T("unbekannter Name", "unknown name");
                    var p = L(M2 + "/user/app/" + x);
                    bool zwilling = H.Hat(apps, x);
                    if (!zwilling)
                    {
                        if (H.Hat(appmeta, x)) { p.Add("/user/appmeta/" + x); benutzt["/user/appmeta/" + x] = 1; }
                        p = mitKachel(p, x);
                    }
                    MerkeSpielteile(p, 7, t, x, M2 + "/user", H.Hat(m2Patches, x), H.Hat(m2Addconts, x), dlcInfos, benutzt, zwilling);
                }
                foreach (var x in m2Patches)
                    if (!benutzt.ContainsKey(M2 + "/user/patch/" + x) && !H.Like(x, "NPXS*")) Merke(L(M2 + "/user/patch/" + x), 7, T("Update ohne Spiel: ", "Update without game: ") + MitName(x));
                foreach (var x in m2Addconts)
                    if (!benutzt.ContainsKey(M2 + "/user/addcont/" + x) && !H.Like(x, "NPXS*")) Merke(L(M2 + "/user/addcont/" + x), 7, T("Zusatzinhalte ohne Spiel: ", "Add-ons without game: ") + MitName(x));
                foreach (var e in m2Liste)
                {
                    string x = e.Name;
                    if (H.Gleich(x, "lost+found") || H.Gleich(x, "system_data") || H.Gleich(x, "user") || H.Like(x, "_zu_loeschen_*")) continue;
                    MerkeM2Ordner(e);
                }
            }
            foreach (var x in appmeta)
            {
                if (H.Hat(ASTRO_IDS, x) || H.Gleich(x, "external") || H.Gleich(x, "addcont") || H.Like(x, "NPXS*")) continue;
                if (H.Hat(apps, x) || benutzt.ContainsKey("/user/appmeta/" + x)) continue;
                string t = AppTitel(x);
                if (IstAstro(t)) { AstroMerken(x); continue; }
                if (t == "") t = T("entferntes Spiel", "removed game");
                Merke(mitKachel(L("/user/appmeta/" + x), x), 5, T("Reste: ", "Leftovers: ") + t + " (" + x + ")");
            }
            var shareIds = new List<string>();
            foreach (var x in S.Namen("/user/system/share"))
            {
                if (!H.M(x, "^[A-Z]{4}\\d{5}$") || H.Like(x, "NPXS*") || H.Hat(ASTRO_IDS, x)) continue;
                if (H.Hat(apps, x) || H.Hat(m2Apps, x)) continue;
                shareIds.Add(x);
            }
            if (shareIds.Count > 0)
                Merke(shareIds.Select(x => "/user/system/share/" + x), 5, T("Share-Einstellungen alter Spiele (", "Share settings of old games (") + shareIds.Count + ")");
            var cache = new List<string>();
            foreach (var u in new[] { "conceptmeta", "appmeta" }) foreach (var x in S.Namen("/user/catalog_downloader/" + u)) cache.Add("/user/catalog_downloader/" + u + "/" + x);
            if (cache.Count > 0) Merke(cache, 5, T("Store-Bildercache (", "Store image cache (") + cache.Count + T(", lädt die PS5 neu)", ", re-downloaded by the PS5)"));
            var tmp = new List<string>();
            foreach (var o in new[] { "/user/bgft/trash", "/user/savedata/temp", "/user/temp/gls/screenshot", "/user/temp/video_recording", "/user/temp/common_temp/telemetry" })
                foreach (var x in S.Namen(o)) tmp.Add(o + "/" + x);
            if (tmp.Count > 0) Merke(tmp, 5, T("Temporäre Dateien (", "Temporary files (") + tmp.Count + ")");
            var fehler = new List<string>();
            foreach (var x in S.Namen("/system_data/priv/error/history")) if (H.Like(x, "*.json")) fehler.Add("/system_data/priv/error/history/" + x);
            if (fehler.Count > 0) Merke(fehler, 5, T("Fehlerberichte (", "Error reports (") + fehler.Count + ")");
            var ext = new List<string>();
            foreach (var x in S.Namen(SYS_META + "/external")) if (H.M(x, "^[A-Z]{4}\\d{5}$") && !H.Like(x, "NPXS*")) ext.Add(SYS_META + "/external/" + x);
            if (ext.Count > 0) Merke(ext, 5, T("Infos zu Spielen auf USB-Platten (", "Info on games on USB drives (") + ext.Count + T(", legt die PS5 neu an)", ", recreated by the PS5)"));
            foreach (var x in sysmeta)
            {
                if (!H.M(x, "^[A-Z]{4}\\d{5}$") || H.Like(x, "NPXS*") || H.Hat(ASTRO_IDS, x)) continue;
                if (H.Hat(apps, x) || H.Hat(appmeta, x) || H.Hat(m2Apps, x)) continue;
                string t = AppTitel(x);
                if (IstAstro(t)) { AstroMerken(x); continue; }
                if (t == "") t = T("unbekannter Name", "unknown name");
                Merke(L(SYS_META + "/" + x), 5, T("Leere Kachel (Schloss): ", "Empty tile (lock icon): ") + t + " (" + x + ")", kachel: true);
            }
            foreach (var x in dlcInfos) if (!benutzt.ContainsKey("/user/appmeta/addcont/" + x)) Merke(L("/user/appmeta/addcont/" + x), 5, T("Zusatzinhalt-Infos: ", "Add-on info: ") + MitName(x));

            // ---- 6 Jailbreak-Tools & Homebrew-Daten -------------------------
            var dl = S.Liste("/user/data", true) ?? new List<Datei>();
            MerkeOrdner("/user/data", dl, 6, "");
            var fakeLiz = new List<string>();
            foreach (var x in S.Namen("/user/license", true)) if (H.M(x, "^fake.+\\.(rif|idx)$")) fakeLiz.Add(x);
            var gruppen = new List<string>();
            foreach (var x in fakeLiz) { string g = H.R(x, "\\.(rif|idx)$", ""); if (!H.Hat(gruppen, g)) gruppen.Add(g); }
            foreach (var g in gruppen)
            {
                var pf = new List<string>();
                foreach (var x in fakeLiz) if (H.Gleich(x, g + ".rif") || H.Gleich(x, g + ".idx")) pf.Add("/user/license/" + x);
                var tm = H.Mt(g, "([A-Z]{4}\\d{5})");
                if (tm.Success)
                {
                    string tid = tm.Groups[1].Value;
                    var spiele = bau.Where(f => f.IstTeil("Spiel") && H.Like(f.Gruppe, "*|" + tid)).ToList();
                    if (spiele.Count == 1) { spiele[0].Pfade = spiele[0].Pfade.Concat(pf).ToList(); spiele[0].PfadeBasis = new List<string>(spiele[0].Pfade); continue; }
                    if (spiele.Count > 1 || H.Hat(apps, tid) || H.Hat(m2Apps, tid)) continue;
                }
                string n = H.R(g, "^fake", "");
                if (tm.Success) { string mn = MitName(tm.Groups[1].Value); if (mn != tm.Groups[1].Value) n = n + " - " + H.R(mn, "^[A-Z]{4}\\d{5} ", ""); }
                Merke(pf, 6, T("Fake-Lizenz: ", "Fake license: ") + n + T(" (Spiel nicht mehr installiert)", " (game no longer installed)"));
            }
            foreach (var u in homes)
                if (S.Namen("/user/home/" + u + "/webkit/shell/local").Count > 0)
                    Merke(L("/user/home/" + u + "/webkit/shell/local"), 6, T("Browserspeicher (Jailbreak-Seiten, evtl. mit offline gespeichertem Jailbreak)", "Browser storage (jailbreak pages, may contain an offline-saved jailbreak page)"));

            // fest nach Bereich ordnen (innerhalb eines Bereichs bleibt die Fundreihenfolge)
            var sortiert = new List<Eintrag>();
            foreach (var k in ALLE_KAT) foreach (var f in bau) if (f.Kat == k) sortiert.Add(f);
            Funde = sortiert;
            FUNDE_IP = S.Ftp.Ip + ":" + S.Ftp.Port;
        }
    }
}
