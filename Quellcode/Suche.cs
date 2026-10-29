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
            T("Spiele & Apps", "Games & apps"),
            T("Spielstände", "Saved data"),
            T("Trophäen", "Trophies"),
            T("Bilder & Videos", "Screenshots & videos"),
            T("Reste", "Leftovers"),
            T("Jailbreak & Homebrew", "Jailbreak & homebrew") }; } }
        public static string[] KatInfo { get { return new[] {
            "",
            T("Spiele und Apps auf interner SSD, M.2 und USB - der Ort steht vorne. Wer ein Spiel wählt, nimmt Update und DLCs mit.", "Games and apps on the internal SSD, M.2 and USB - the location comes first. Selecting a game takes its update and DLC with it."),
            T("Spielstände auf der Konsole. Ohne Cloud-Sicherung (PS Plus) endgültig weg.", "Saved data on the console. Without a cloud backup (PS Plus) it is gone for good."),
            T("Trophäen-Fortschritt und Trophäen-Listen. Nie mit dem PSN abgeglichene Trophäen sind endgültig weg.", "Trophy progress and trophy lists. Trophies never synced with PSN are gone for good."),
            T("Screenshots und Videoclips, je Spiel mit Anzahl.", "Screenshots and video clips, per game with count."),
            T("Downloads, Zusatzinhalte und Updates ohne Spiel, Reste gelöschter Spiele, Fehlerberichte und übrige Dateien auf M.2 und USB.", "Downloads, add-ons and updates without a game, leftovers of deleted games, error reports and other files on M.2 and USB."),
            T("Kacheln und Daten der Jailbreak-Tools, Homebrew-Daten und ELF-Dateien aller Speicher. Danach Jailbreak neu einrichten.", "Tiles and data of jailbreak tools, homebrew data and ELF files on all storage. Set up your jailbreak again afterwards.") }; } }
        public static readonly int[] ALLE_KAT = { 1, 2, 3, 4, 5, 6 };   // NEU 1.0.6: 6 Bereiche (der M.2-Bereich ist aufgeteilt)

        // NEU 1.0.6 (da_Chris86 28.10.2026): Speicherorte - steht vorne am Eintrag, die Bereiche sind danach sortiert
        public static int PfadOrt(string p)
        {
            if (p == null) return ORT_INTERN;
            if (p == M2 || p.StartsWith(M2 + "/", StringComparison.Ordinal)) return ORT_M2;
            if (p == USB_ERW || p.StartsWith(USB_ERW + "/", StringComparison.Ordinal)) return ORT_USB;
            if (p.StartsWith("/mnt/usb", StringComparison.Ordinal)) return ORT_STICK;
            return ORT_INTERN;
        }
        public static string OrtVorsatz(int ort)
        {
            switch (ort) { case ORT_M2: return "M.2 – "; case ORT_USB: return "USB – "; case ORT_STICK: return T("USB-Stick – ", "USB stick – "); }
            return "";
        }
        public static string OrtName(int ort)
        {
            switch (ort) { case ORT_M2: return T("M.2-SSD", "M.2 SSD"); case ORT_USB: return T("USB-Speicher", "USB storage"); case ORT_STICK: return T("USB-Stick", "USB stick"); }
            return T("interne SSD", "internal SSD");
        }
        public bool STICK_DA;        // NEU 1.0.6: mindestens ein normaler USB-Stick (/mnt/usbN) lesbar
        bool MERKE_STICK;            // NEU 1.0.6: gerade wird ein USB-Stick gelistet -> Einträge nur einzeln

        public List<Eintrag> Funde = new List<Eintrag>();
        public bool m2Da;
        // NEU 1.0.5: nur für einen Hinweis - an Suche und Löschen ändert sich nichts.
        public bool M2_OHNE_USER;      // M.2-Ordner da, aber ohne user-Ordner (leer oder nicht richtig eingebunden)
        public bool USB_SPEICHER;      // USB-Platte als erweiterter Speicher (/mnt/ext0) mit Inhalt
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
        bool IstAstro(string t) { return false; }   // da_Chris86 28.10.2026: Astro's Playroom ist ein Spiel wie jedes andere
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
            // NEU 1.0.6: Ort = Ort der Spieldatei (ShadowMount) bzw. des ersten Pfads außerhalb der internen SSD
            int ort = ORT_INTERN;
            if (!string.IsNullOrEmpty(quelle)) ort = PfadOrt(quelle);
            else foreach (var x in p) { ort = PfadOrt(x); if (ort != ORT_INTERN) break; }
            text = OrtVorsatz(ort) + text;
            if (gesperrt) text += GESPERRT_TEXT;
            bau.Add(new Eintrag { Pfade = p, PfadeBasis = new List<string>(p), Kat = kat, Text = text, TextBasis = text, Quelle = quelle ?? "", Gesperrt = gesperrt, Kachel = kachel, SmTipp = smTipp,
                                  Ort = ort, NurEinzeln = MERKE_STICK || (ort == ORT_STICK && string.IsNullOrEmpty(quelle)) });   // NEU 1.0.6: USB-Stick nur einzeln; da_Chris86 30.10.2026: außer der internen Kachel eines ShadowMount-Spiels mit Spieldatei auf dem Stick (wie PS5-App)
        }
        void MerkeTeil(IEnumerable<string> pfade, int kat, string text, string gruppe, string teil)
        {
            Merke(pfade, kat, text);
            if (!MERKE_OK) return;
            var f = bau[bau.Count - 1];
            f.Gruppe = gruppe; f.Teil = teil;
        }
        // NEU 1.0.6: bekannte Jailbreak-Einstiege. da_Chris86 29.10.2026: NUR die Einstiege, mit denen der Jailbreak gestartet wird
        // (keine Jailbreak-Tools wie etaHEN, Payload Manager, Autoloader-Ordner, ELF-Dateien - die bleiben wie bisher):
        //   YouTube (Y2JB: PPSA01650/1/2), Netflix ('N Hack: PPSA01614/5/6), Lua-Jailbreak (remote_lua_loader: 18 Artemis-Spiele),
        //   PS2-Spielstand-Jailbreak (mast1c0re/Luac0re: Okage CUSA02199, Star Wars Racer Revenge CUSA03474/CUSA03492) - jeweils mit
        //   Spielstand; dazu der Browserspeicher (offline gespeicherter WebKit-Jailbreak). BD-JB läuft von der Disc - nichts auf der Konsole.
        const string JB_Y2JB = "PPSA0165[0-2]";
        const string JB_NETFLIX = "PPSA0161[4-6]";
        const string JB_LUA = "CUSA(16074|17068|27389|27390|16229|19556|17112|17126|13303|13586|25179|25180|29745|29746|14324|11977|11481|17122|19555|09825)";
        const string JB_PS2 = "CUSA(02199|02282|03474|03492|07842|07992)";
        // NEU 1.0.6 (Recherche 29.10.2026, da_Chris86: "alle Jailbreak-Varianten"): Ren'Py-Jailbreak yarpe - Spiel + Spielstand;
        // WebKit-Autoloader-Kacheln (itsPLK WKAL00001, Goldengames GGAU00001) - Kachel + Browserspeicher
        const string JB_YARPE = "CUSA(30428|30429|30430|30431|32096|32097)|PPSA(06409|06410)";
        const string JB_WEBKIT = "WKAL00001|GGAU00001";
        const string JB_IDS = JB_Y2JB + "|" + JB_NETFLIX + "|" + JB_LUA + "|" + JB_PS2 + "|" + JB_YARPE + "|" + JB_WEBKIT;
        const string JB_MUSTER = "^(/user/(app|appmeta|appmeta/addcont|appmeta/external/addcont|patch|addcont|download)|/system_data/priv/appmeta(/external)?|/mnt/ext[01]/user/(app|patch|addcont)|/user/home/[^/]+/savedata(_prospero)?)/(" + JB_IDS + ")(/|$)";
        const string JB_BROWSER = "^/user/home/[^/]+/webkit/shell/(local|appcache)$";   // 1.0.6: AppCache UND localStorage
        // Codex-Prüfung 36: Spiel-Abbild eines Einstiegs (z. B. "Raspberry Cube - CUSA16074.exfat"), auch als eigener Eintrag ohne Kachel
        static string JbKennung(string x) { return "(^|[^A-Z0-9])(" + x + ")([^0-9]|$)"; }
        static readonly string JB_ABBILD = "^(?=.*" + JbKennung(JB_IDS) + ").*\\.(?i:exfat|ffpfs|ffpfsc|ffpkg|pfs|img)$";
        static bool JbPfad(string p) { return H.M(p, JB_MUSTER) || H.M(p, JB_ABBILD); }
        public static bool JbAbbild(string p) { return H.M(p, JB_ABBILD); }   // Codex-Prüfung 37: für den Nachkommenschutz beim Löschen
        static bool IstAbbild(string p) { return H.M(p, "(?i)\\.(exfat|ffpfs|ffpfsc|ffpkg|pfs|img)$"); }
        static bool JbEinstieg(string id) { return H.M(id, "^(" + JB_IDS + ")$"); }
        // NEU 1.0.6: zu welchem Jailbreak ein Pfad gehört - null = zu keinem (gleich wie kern_jb_weg)
        public static string JbWeg(string p)
        {
            if (p == null) return null;
            if (H.M(p, JB_BROWSER)) return T("WebKit-Jailbreak (Browser)", "WebKit jailbreak (browser)");
            if (H.M(p, JbKennung(JB_Y2JB))) return "Y2JB (YouTube)";
            if (H.M(p, JbKennung(JB_NETFLIX))) return T("Netflix-Jailbreak", "Netflix jailbreak");
            if (H.M(p, JbKennung(JB_LUA))) return T("Lua-Jailbreak", "Lua jailbreak");
            if (H.M(p, JbKennung(JB_PS2))) return T("PS2-Spielstand-Jailbreak", "PS2 save jailbreak");
            if (H.M(p, JbKennung(JB_YARPE))) return T("Ren'Py-Jailbreak (yarpe)", "Ren'Py jailbreak (yarpe)");
            if (H.M(p, JbKennung(JB_WEBKIT))) return "WebKit-Autoloader";
            return null;
        }
        public static string JbWeg(Eintrag f)
        {
            foreach (var p in f.PfadeBasis) { var w = JbWeg(p); if (w != null) return w; }
            foreach (var p in f.Pfade) { var w = JbWeg(p); if (w != null) return w; }
            { var w = JbWeg(f.TextBasis ?? f.Text); if (w != null) return w; }   // Codex-Prüfung 37: Spiel-Abbild, Kennung nur im Namen
            return T("Jailbreak", "jailbreak");
        }
        // NEU 1.0.6 (da_Chris86 29.10.2026): mit dem Namen des Jailbreaks
        public static string JbHinweis(Eintrag f) { return T("Gehört zum Jailbreak: ", "Belongs to the jailbreak: ") + JbWeg(f) +
            T(". Nur einzeln wählbar. Wird es gelöscht, funktioniert dieser Jailbreak erst wieder, wenn er neu eingerichtet ist. Die vollständige Bereinigung fragt, ob es bleibt.",
              ". Can only be selected individually. If it is deleted, this jailbreak only works again once it has been set up again. The full cleanup asks whether it stays."); }
        // NEU 1.0.6 (da_Chris86 29.10.2026 07:28): Warnung beim Anwählen
        public static string JbWarnung(Eintrag f) { return T("ACHTUNG: Gehört zum Jailbreak „", "WARNING: belongs to the jailbreak \"") + JbWeg(f) +
            T("“ - wird es gelöscht, geht dieser Jailbreak nicht mehr.", "\" - if it is deleted, this jailbreak stops working."); }
        // NEU 1.0.6 (da_Chris86 28.10.2026 23:16, erweitert 29.10.2026): eingerichteter Jailbreak-Einstieg laut letzter Suche (gleich wie
        // kern_jailbreak_erkannt). YouTube/Netflix mit /user/download/<Kennung>, Lua- und PS2-Spielstand-Jailbreak mit dem Spielstand.
        // Der Browserspeicher kann einen offline gespeicherten WebKit-Jailbreak enthalten - nur ein Hinweis. null = keiner.
        public bool BD_DISC;   // NEU 1.0.6: Blu-ray-Jailbreak-Disc im Laufwerk (nur gelesen)
        public string JailbreakErkannt(out string warnung)
        {
            warnung = null;
            var ids = new List<string>(); var namen = new List<string>(); bool browser = false;
            foreach (var f in Funde)
            {
                if (!f.Jailbreak) continue;
                foreach (var p in f.PfadeBasis)
                {
                    if (H.M(p, JB_BROWSER)) { browser = true; continue; }
                    var t = H.Mt(p, "/(" + JB_IDS + ")(/|$)");
                    if (!t.Success) continue;
                    string id = t.Groups[1].Value;
                    bool daten = (H.M(id, "^(" + JB_Y2JB + "|" + JB_NETFLIX + ")$") && p.StartsWith("/user/download/", StringComparison.Ordinal))
                              || (H.M(id, "^(" + JB_LUA + "|" + JB_PS2 + "|" + JB_YARPE + ")$") && H.M(p, "^/user/home/[^/]+/savedata(_prospero)?/"))
                              || (H.M(id, "^(" + JB_WEBKIT + ")$") && p.StartsWith("/user/app/", StringComparison.Ordinal));   // NEU 1.0.6: Autoloader-Kachel
                    if (!daten) continue;
                    if (!ids.Contains(id)) ids.Add(id);
                    string nm = JbWeg(p);
                    if (nm != null && !namen.Contains(nm)) namen.Add(nm);
                }
            }
            // betroffene Einträge je Bereich: „Name (Kennung)“ (unter „Bereich“)
            var gruppen = new List<string>();
            foreach (var kk in ALLE_KAT)
            {
                var eintraege = new List<string>();
                foreach (var f in Funde)
                {
                    if (!f.Jailbreak || f.Kat != kk) continue;
                    if (!f.PfadeBasis.Any(p => ids.Any(x => H.M(p, "(^|[^A-Z0-9])" + x + "([^0-9]|$)")))) continue;
                    string t = f.TextBasis ?? f.Text; int z = t.IndexOf("  [", StringComparison.Ordinal);
                    if (z >= 0) t = t.Substring(0, z);
                    var m = H.Mt(t, "^([^()]*\\([A-Z]{4}\\d{5}\\))");      // nur "Name (Kennung)"
                    string e = "„" + (m.Success ? m.Groups[1].Value : t) + "“";
                    if (!eintraege.Contains(e)) eintraege.Add(e);
                }
                if (eintraege.Count > 0) gruppen.Add(string.Join(", ", eintraege) + T(" (unter „", " (under \"") + KatName[kk] + T("“)", "\")"));
            }
            string w = gruppen.Count > 0 ? T("Nicht löschen, wenn du ihn behalten willst: ", "Do not delete if you want to keep it: ") + string.Join("; ", gruppen) + "." : null;
            if (browser)
            {
                string bt = T("Ein offline gespeicherter WebKit-Jailbreak kann im „Browserspeicher“ liegen (unter „Jailbreak & Homebrew“).",
                              "An offline-saved WebKit jailbreak may be in the \"Browser storage\" (under \"Jailbreak & homebrew\").");
                w = w != null ? w + " " + bt : bt;
            }
            if (BD_DISC) namen.Add(T("Blu-ray-Jailbreak (Disc im Laufwerk)", "Blu-ray jailbreak (disc in the drive)"));   // NEU 1.0.6
            warnung = w;
            return namen.Count > 0 ? string.Join(", ", namen) : null;
        }
        List<string> DL_NAMEN;   // NEU 1.0.6: /user/download einmal je Suche lesen
        List<string> DL_USB;     // NEU 1.0.6: Apps auf der USB-Erweiterung (null = noch nicht gelesen)
        bool DL_USB_UNKLAR;
        // NEU 1.0.6: der App-Datenspeicher /user/download/<ID> geht mit der App - nur wenn es ihn gibt, keine Sony-App und
        // keine SICHTBARE Kopie auf der USB-Erweiterung liegt. Sonst null. (DL_USB_UNKLAR merkt nur, dass das Lesen schon scheiterte.)
        public string AppDaten(string id)
        {
            if (string.IsNullOrEmpty(id) || H.Like(id, "NPXS*")) return null;
            if (DL_NAMEN == null) DL_NAMEN = S.Namen("/user/download");
            if (!H.Hat(DL_NAMEN, id)) return null;
            if (USB_SPEICHER)
            {
                if (DL_USB == null && !DL_USB_UNKLAR)
                {
                    try { DL_USB = S.Namen(USB_ERW + "/user/app", true); }
                    catch (AbbruchAusnahme) { throw; }
                    catch { DL_USB_UNKLAR = true; }
                }
                if (DL_USB != null && H.Hat(DL_USB, id)) return null;   // da_Chris86 27.10.2026 21:39: nur eine SICHTBARE USB-Kopie zählt (nicht lesbar sperrt nichts)
            }
            return "/user/download/" + id;
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
            // NEU 1.0.6: der App-Datenspeicher /user/download/<ID> gehört zur App und geht mit ihr (kein Download-Rest)
            string ad = zwilling ? null : AppDaten(id);
            // NEU 1.0.6: das Spiel liegt selbst auf dem USB-Speicher (keine andere sichtbare Kopie) - seine App-Daten gehen mit
            if (!zwilling && ad == null && basis.StartsWith(USB_ERW + "/", StringComparison.Ordinal) && !H.Like(id, "NPXS*"))
            {
                if (DL_NAMEN == null) DL_NAMEN = S.Namen("/user/download");
                if (H.Hat(DL_NAMEN, id)) ad = "/user/download/" + id;
            }
            if (ad != null) spielPfade.Add(ad);
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
        class QInfo { public List<string> Links = new List<string>(); public string Name; public bool Gesperrt; public bool Mehrfach; public bool Vermutet; public bool Jb; public string JbName; }   // Jb NEU 29.10.2026 (Codex-Prüfung 37)   // Mehrfach/Vermutet NEU 1.0.6
        List<string> SpielQuellen;
        Dictionary<string, QInfo> QuelleInfo;
        public List<string> SperrQuellen = new List<string>();   // NEU 1.0.6: nicht sicher zugeordnete Spieldateien - nie löschen, auch nicht über einen Elternordner
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
            // NEU 1.0.6 (da_Chris86 28.10.2026): USB wie M.2 - auch Spieldateien auf USB-Speicher und USB-Stick
            string u = Regex.Escape(USB_ERW);
            if (H.M(q, "^" + u + "/(user|system_data|lost\\+found)(/|$)")) return "";
            if (H.M(q, "^" + u + "/[^/]+")) return "USB";
            if (H.M(q, "^/mnt/usb\\d+/[^/]+")) return "USB-Stick";
            if (H.M(q, "^/user/data/[^/]+")) return "intern";
            return "";
        }
        // NEU 1.0.6 (27.10.2026): Die Kachel merkt sich den Ort der Spieldatei beim Eintragen (mount_img.lnk). Wurde die Datei
        // danach verschoben, ist das Spiel trotzdem da - ShadowMountPlus findet es beim nächsten Durchsuchen wieder. Darum vor
        // "tote Kachel" an allen Orten nachsehen, die ShadowMountPlus durchsucht (Standardliste aus ShadowMountPlus
        // include/sm_paths.h, dazu scanpath= aus /data/shadowmount/config.ini, Suchtiefe 1). Ein Fund ist NUR EIN HINWEIS
        // (gleicher Name/Kennung beweist nicht dasselbe Spiel): nie zuordnen, Kachel und vermutete Datei sichtbar sperren.
        static readonly string[] SM_ORTE = {
            "/user/data/homebrew", "/user/data/homebrew/backports", "/user/data/etaHEN/games",
            "/mnt/ext0/homebrew", "/mnt/ext0/etaHEN/games", "/mnt/ext1/homebrew", "/mnt/ext1/etaHEN/games",
            "/mnt/usb0/homebrew", "/mnt/usb1/homebrew", "/mnt/usb2/homebrew", "/mnt/usb3/homebrew", "/mnt/usb4/homebrew", "/mnt/usb5/homebrew", "/mnt/usb6/homebrew", "/mnt/usb7/homebrew",
            "/mnt/usb0/etaHEN/games", "/mnt/usb1/etaHEN/games", "/mnt/usb2/etaHEN/games", "/mnt/usb3/etaHEN/games",
            "/mnt/usb4/etaHEN/games", "/mnt/usb5/etaHEN/games", "/mnt/usb6/etaHEN/games", "/mnt/usb7/etaHEN/games",
            "/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3", "/mnt/usb4", "/mnt/usb5", "/mnt/usb6", "/mnt/usb7", "/mnt/ext0", "/mnt/ext1" };
        // ALLE Treffer (Codex-Prüfung 14): zuerst gleicher Name, dann gleiche Kennung - jeder wird gesperrt
        List<string> SmNeueOrte(string q, string id)
        {
            var treffer = new List<string>();
            int i = q.LastIndexOf('/');
            if (i < 0 || i == q.Length - 1) return treffer;
            string name = q.Substring(i + 1);
            var orte = new List<string>(SM_ORTE);
            foreach (string z in Regex.Split(S.Text("/user/data/shadowmount/config.ini"), "\r?\n"))
            {
                var t = H.Mt(z, "^\\s*scanpath\\s*=\\s*(/[^#;]*?)\\s*([#;].*)?$");   // Kommentar nach # oder ; wie ShadowMountPlus
                if (!t.Success) continue;
                string o = t.Groups[1].Value;
                if (H.Like(o, "/data/*")) o = "/user" + o;
                while (o.Length > 1 && o.EndsWith("/")) o = o.Substring(0, o.Length - 1);
                if (!H.M(o, "//|(^|/)\\.\\.?(/|$)") && !H.Hat(orte, o)) orte.Add(o);
            }
            // NEU 1.0.6 (Codex-Prüfung 30): ein nicht lesbarer USB-Stick bricht die Suche nicht ab - dort wird dann nichts gefunden
            foreach (var o in orte)
            {
                string kand = o + "/" + name;
                if (kand == q || treffer.Contains(kand)) continue;
                bool stick = o.StartsWith("/mnt/usb", StringComparison.Ordinal), da;
                try { da = S.Existiert(kand); }
                catch (AbbruchAusnahme) { throw; }
                catch (Exception) { if (!stick) throw; da = false; }
                if (da) treffer.Add(kand);
            }
            foreach (var o in orte)
            {
                bool stick = o.StartsWith("/mnt/usb", StringComparison.Ordinal);
                List<string> namen;
                try { namen = S.Namen(o); }
                catch (AbbruchAusnahme) { throw; }
                catch (Exception) { if (!stick) throw; namen = new List<string>(); }
                foreach (var n in namen)
                    if (H.Like(n, "*" + id + "*")) { string kand = o + "/" + n; if (kand != q && !treffer.Contains(kand)) treffer.Add(kand); }
            }
            return treffer;
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
        // NEU 1.0.6 (PS5-Test 28.10.2026, da_Chris86: "Thema Benutzerfreundlichkeit"): Spiel-Abbild, dessen Spielnummer zu einer Kachel
        // gehört, die eine ANDERE Datei nutzt -> "zweite Spieldatei" statt "ohne Kachel". null = keine solche Kachel.
        string ZweiteSpieldatei(string p, string name)
        {
            var t = H.Mt(name, "([A-Z]{4}\\d{5})");
            if (!t.Success) return null;
            string id = t.Groups[1].Value;
            foreach (var q in SpielQuellen)
            {
                if (q == p) continue;
                if (!q.Substring(q.LastIndexOf('/') + 1).Contains(id)) continue;
                string wo;
                switch (PfadOrt(q))
                {
                    case ORT_M2: wo = "M.2"; break;
                    case ORT_USB: wo = "USB"; break;
                    case ORT_STICK: wo = T("USB-Stick", "USB stick"); break;
                    default: wo = T("intern", "internal"); break;
                }
                // 28.10.2026 (da_Chris86: Texte zu lang): kurz, ohne Namen - die Spielnummer steht im Dateinamen
                return T("  [2. Kopie – Kachel nutzt ", "  [2nd copy – tile uses ") + wo + "]";
            }
            return null;
        }
        // NEU 1.0.6 (PS5-Test 28.10.2026, da_Chris86: "Benutzerfreundlichkeit"): die Daten eines Jailbreak-Tools stehen direkt unter
        // seiner Tool-Kachel (eingerückt mit "   – "), jeder Eintrag bleibt einzeln wählbar (nur Tool, nur Daten oder beides).
        // Zugeordnet wird nur, wenn genau EINE Kachel passt - sonst bleibt der Eintrag, wo er war. Gleich wie kern_suche.c.
        static string ToolSchluessel(string s)
        {
            var b = new StringBuilder();
            foreach (char c0 in s) { char c = c0; if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) b.Append(c); }
            return b.ToString();
        }
        static string ToolAlias(string id)
        {
            switch (id) { case "PLDM00001": return "pldmgr"; case "PKGM00001": return "pkgmgr"; case "APDU00001": return "ps5appdumper"; }
            return "";
        }
        static bool ToolPasst(string tk, string ak, string dk)
        {
            if (dk.Length == 0) return false;
            if (ak != "" && ak == dk) return true;
            if (dk.Length >= 5 && tk.Contains(dk)) return true;
            if (tk.Length >= 5 && dk.Contains(tk)) return true;
            int p = 0; while (p < tk.Length && p < dk.Length && tk[p] == dk[p]) p++;
            return p >= 8;
        }
        static List<Eintrag> ToolDatenOrdnen(List<Eintrag> liste)
        {
            string vor = T("Tool-Kachel: ", "Tool tile: ");
            int n = liste.Count;
            var zu = new int[n];
            for (int i = 0; i < n; i++) zu[i] = -1;
            for (int d = 0; d < n; d++)
            {
                var f = liste[d];
                if (f.Kat != 6 || f.Ort != ORT_INTERN || f.Kachel || f.PfadeBasis.Count == 0) continue;
                var m = H.Mt(f.PfadeBasis[0], "^/user/data/(homebrew/)?([^/]+)");
                if (!m.Success) continue;
                string dk = ToolSchluessel(m.Groups[2].Value);
                int treffer = -1, anz = 0;
                for (int t = 0; t < n; t++)
                {
                    var g = liste[t];
                    if (g.Kat != 6 || !g.Kachel || !g.TextBasis.StartsWith(vor, StringComparison.Ordinal) || g.PfadeBasis.Count == 0) continue;
                    var mi = H.Mt(g.PfadeBasis[0], "^/user/app/([A-Z]{4}\\d{5})$");
                    if (!mi.Success) continue;
                    string titel = g.TextBasis.Substring(vor.Length);
                    int ende = titel.IndexOf(" (" + mi.Groups[1].Value + ")", StringComparison.Ordinal);
                    string tk = ToolSchluessel(ende >= 0 ? titel.Substring(0, ende) : titel);
                    if (ToolPasst(tk, ToolAlias(mi.Groups[1].Value), dk)) { treffer = t; anz++; }
                }
                if (anz == 1) zu[d] = treffer;
            }
            var neu = new List<Eintrag>();
            for (int i = 0; i < n; i++)
            {
                if (zu[i] >= 0) continue;
                neu.Add(liste[i]);
                for (int d = 0; d < n; d++) if (zu[d] == i) { var f = liste[d]; f.Text = "   – " + f.Text; f.TextBasis = "   – " + f.TextBasis; neu.Add(f); }
            }
            return neu;
        }
        // NEU 1.0.6: entpacktes Spiel? (nur lesen; ein Lesefehler zählt als "nein" - dann bleibt der Ordner, wo er war)
        bool SpielOrdner(string p)
        {
            try { return S.Existiert(p + "/sce_sys/param.json") || S.Existiert(p + "/sce_sys/param.sfo"); }
            catch (AbbruchAusnahme) { throw; } catch { return false; }
        }
        // NEU 1.0.6 (PS5-Test da_Chris86 30.10.2026 08:10: "es ist ein Spiel, daher hat es da unter 6 nichts zu suchen"): liegt in einem
        // Ordner direkt ein Spiel (Spiel-Abbild, ISO mit Spielnummer oder entpacktes Spiel) ohne Kachel, wird der Ordner aufgeklappt wie
        // bei ShadowMount-Spielen - das Spiel steht dann unter "Spiele", der Rest des Ordners einzeln an seinem Platz.
        // Beispiel: USB-Stick /mnt/usb0/homebrew/PPSA03530.exfat. Nur lesen; ein Lesefehler zählt als "nein" (Ordner bleibt ein Eintrag).
        bool EnthaeltSpiel(string p)
        {
            try
            {
                var u = S.Liste(p, false);
                if (u == null) return false;
                foreach (var d in u)
                {
                    if (d.IstLink || IstZwischendatei(d.Name)) continue;
                    if (!d.IstOrdner && (H.M(d.Name, "(?i)\\.(exfat|ffpfs|ffpfsc|ffpkg|pfs|img)$")
                                         || (H.M(d.Name, "(?i)\\.iso$") && H.M(d.Name, "(?i)(^|[^a-z])[a-z]{4}[-_]?\\d{5}")))) return true;
                    if (d.IstOrdner && H.M(d.Name, "(^|[^A-Z0-9])[A-Z]{4}\\d{5}([^0-9]|$)") && SpielOrdner(p + "/" + d.Name)) return true;
                }
                return false;
            }
            catch (AbbruchAusnahme) { throw; } catch { return false; }
        }
        void MerkeOrdner(string basis, IEnumerable<Datei> eintraege, int kat, string vorsatz)
        {
            foreach (var e in eintraege)
            {
                string x = e.Name; bool link = e.IstLink;
                string p = basis + "/" + x;
                if (IstZwischendatei(x)) { KOMPRIMIERUNG.Add(p); continue; }
                if (SpielQuellen.Contains(p))   // seit 27.10.2026 Groß/klein genau
                {
                    var i = QuelleInfo[p];
                    // da_Chris86 28.10.2026 13:41: Ein Spiel-Abbild ist ein Spiel - intern genauso wie auf M.2/USB. Ist die Zuordnung zu einer
                    // Kachel nicht eindeutig (vermutet, mehrere Kacheln), wird es als eigenes Spiel angeboten (nur die Datei); die Kacheln sind eigene Einträge.
                    if (i.Vermutet || i.Mehrfach)
                    {
                        Merke(L(p), 1, vorsatz + x + T("  [Spiel-Abbild: ", "  [game image: ") + i.Name
                            + (i.Mehrfach ? T(" - mehrere Kacheln zeigen darauf, sie sind eigene Einträge]", " - several tiles point to it, they are separate entries]")
                                          : T(" - die Kachel ist ein eigener Eintrag]", " - the tile is a separate entry]")));
                        // Codex-Prüfung 37: gehört (auch) zu einer Kachel eines Jailbreak-Einstiegs - geschützt wie der Einstieg
                        var b = i.Jb && MERKE_OK && bau.Count > 0 ? bau[bau.Count - 1] : null;
                        if (b != null && !b.Gesperrt) { b.Jailbreak = true; b.NurEinzeln = true;
                            b.Text = b.TextBasis = b.Text + (i.JbName != null ? T("  [Jailbreak-Einstieg ", "  [jailbreak entry ") + i.JbName + T(" - nur einzeln]", " - individually only]") : T("  [Jailbreak-Einstieg - nur einzeln]", "  [jailbreak entry - individually only]")); }
                        continue;
                    }
                    // NEU 1.0.6: Spiel-Abbild mit eindeutiger Kachel - steht EINMAL als Spiel in Bereich 1 (Eintrag der Kachel mit
                    // "nur Kachel" oder "Spiel + Kachel"), nicht noch einmal hier (bis 1.0.5: zusätzlich in Bereich 6/7)
                    continue;
                }
                bool drin = SpielQuellen.Any(q => q.StartsWith(p + "/", StringComparison.Ordinal));
                if (!drin && !link && e.IstOrdner && kat != 1 && EnthaeltSpiel(p)) drin = true;   // NEU 1.0.6 (30.10.2026): Spiel im Ordner
                if (drin && !link)
                {
                    AUFGEKLAPPT.Add(p);
                    var unter = S.Liste(p, true) ?? new List<Datei>();
                    MerkeOrdner(p, unter, kat, vorsatz + x + "/");
                }
                else if (link) Merke(L(p), kat, vorsatz + x + T("  (Verknüpfung, nur sie wird gelöscht)", "  (symlink, only the link is deleted)"));
                // NEU 1.0.6 (PS5-Test da_Chris86 29.10.2026 19:44): entpacktes Spiel (Ordner mit Spielnummer im Namen und sce_sys/param.json
                // oder param.sfo darin, z. B. "F1® 25 - PPSA24662 - v1.26.0 - US") ist ein Spiel wie ein Spiel-Abbild - Bereich 1
                else if (e.IstOrdner && kat != 1 && H.M(x, "(^|[^A-Z0-9])[A-Z]{4}\\d{5}([^0-9]|$)") && SpielOrdner(p)) Merke(L(p), 1, vorsatz + x + T("  [Spiel-Ordner ohne Kachel]", "  [game folder, no tile]"));
                else if (!e.IstOrdner && H.M(x, "(?i)\\.(exfat|ffpfs|ffpfsc|ffpkg|pfs|img)$")) Merke(L(p), 1, vorsatz + x + (ZweiteSpieldatei(p, x) ?? T("  [ohne Kachel]", "  [no tile]")));   // 28.10.2026: ein Spiel (Bereich 1)
                // NEU 1.0.6 (PS5-Test 28.10.2026): ISO mit Spielnummer im Namen (z. B. PS3 BLES01969, PS2 SLES-12345) = Spiel-Abbild;
                // ohne Spielnummer (z. B. Linux-ISO) bleibt sie ein normaler Eintrag
                else if (!e.IstOrdner && H.M(x, "(?i)\\.iso$") && H.M(x, "(?i)(^|[^a-z])[a-z]{4}[-_]?\\d{5}")) Merke(L(p), 1, vorsatz + x + T("  [ISO]", "  [ISO]"));
                else if (!e.IstOrdner && H.M(x, "(?i)\\.elf$")) Merke(L(p), 6, vorsatz + x);   // NEU 1.0.6: ELF-Dateien aller Speicher -> Jailbreak & Homebrew
                else Merke(L(p), kat, vorsatz + x);
            }
        }

        // NEU 1.0.4: Ordner direkt auf der M.2 werden aufgeklappt - jedes Spiel,
        // jeder Unterordner und jede Datei darin ist ein eigener Eintrag. Der
        // Ordner selbst bleibt immer stehen (nur ein leerer wird angeboten).
        // NEU 1.0.6: genauso auf dem USB-Speicher (da_Chris86 28.10.2026: "USB wie M.2"); die Einträge landen in
        // Bereich 5 (Reste), Spiel-Abbilder in Bereich 1, ELF-Dateien in Bereich 6 - jeweils mit Ort vorne.
        // NEU 1.0.6 (Codex-Prüfung 27): Homebrew-Ordner (homebrew, etaHEN, payloads) auf M.2, USB-Speicher und USB-Stick
        // gehören zu "Jailbreak & Homebrew" (Bereich 6), alles andere zu den Resten (Bereich 5)
        static int ExtKat(string name) { return H.M(name, "(?i)^(homebrew|etahen|payloads?)$") ? 6 : 5; }
        void MerkeExtOrdner(string basis, Datei e)
        {
            string p = basis + "/" + e.Name;
            int kat = ExtKat(e.Name);
            if (!e.IstOrdner || e.IstLink || IstZwischendatei(e.Name) || SpielQuellen.Contains(p)) { MerkeOrdner(basis, new[] { e }, kat, ""); return; }
            var unter = S.Liste(p, true);
            // nicht lesbar: wie bisher als Ganzes anbieten (nie als "leer" ausgeben)
            if (unter == null) { MerkeOrdner(basis, new[] { e }, kat, ""); return; }
            if (unter.Count == 0)
            {
                Merke(L(p), kat, e.Name + T("  (leerer Ordner)", "  (empty folder)"));
                if (MERKE_OK) bau[bau.Count - 1].NurLeer = true;
                return;
            }
            MerkeOrdner(p, unter, kat, e.Name + "/");
        }

        // NEU 1.0.5: Hinweis, wenn Spiele auf einem Speicher liegen könnten, den das
        // Programm gerade nicht als "installiert" sieht. Nur Text - die Suche bleibt gleich.
        public const string USB_ERW = "/mnt/ext0";
        public string SpeicherHinweis()
        {
            string t = "";
            if (!m2Da) t += T("\n\nHINWEIS: Keine M.2-SSD gefunden. Steckt eine M.2 mit Spielen drin, die gerade nicht erkannt wird: abbrechen und die PS5 neu starten - sonst werden Lizenzen und Kacheln dieser Spiele mit gelöscht.",
                              "\n\nNOTE: No M.2 SSD found. If an M.2 with games is installed but not detected right now: cancel and restart the PS5 - otherwise the licenses and tiles of those games will be deleted too.");
            else if (M2_OHNE_USER) t += T("\n\nHINWEIS: Die M.2-SSD ist leer oder wird gerade nicht richtig erkannt. Liegen darauf Spiele: abbrechen und die PS5 neu starten - sonst werden Lizenzen und Kacheln dieser Spiele mit gelöscht.",
                                         "\n\nNOTE: The M.2 SSD is empty or not detected properly right now. If it holds games: cancel and restart the PS5 - otherwise the licenses and tiles of those games will be deleted too.");
            if (USB_SPEICHER) t += T("\n\nHINWEIS: Eine USB-Platte als erweiterter Speicher ist angeschlossen. Spiele darauf gelten als installiert.",
                                     "\n\nNOTE: A USB drive is connected as extended storage. Games on it count as installed.");
            return t;
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
            M2_OHNE_USER = m2Da && !m2Liste.Any(e => e.Name == "user");
            USB_SPEICHER = false;
            try { USB_SPEICHER = S.Liste(USB_ERW + "/user") != null; }
            catch (AbbruchAusnahme) { throw; }
            catch { }
            var m2Apps = new List<string>(); var m2Patches = new List<string>(); var m2Addconts = new List<string>();
            if (m2Da) { m2Apps = S.Namen(M2 + "/user/app", true); m2Patches = S.Namen(M2 + "/user/patch", true); m2Addconts = S.Namen(M2 + "/user/addcont", true); }
            // NEU 1.0.6 (Codex-Prüfung 16): Kopien auf der USB-Erweiterung zählen als Zwilling - gemeinsame Kachel-Infos und Fake-Lizenz bleiben
            var usbApps = new List<string>();   // da_Chris86 27.10.2026 21:39: nicht lesbare USB-Erweiterung sperrt nichts - es zählt nur eine SICHTBARE Kopie
            if (USB_SPEICHER) { try { usbApps = S.Namen(USB_ERW + "/user/app", true); } catch (AbbruchAusnahme) { throw; } catch { } }
            // NEU 1.0.6: USB wie M.2 (Updates, Zusatzinhalte)
            var usbPatches = USB_SPEICHER ? S.Namen(USB_ERW + "/user/patch") : new List<string>();
            var usbAddconts = USB_SPEICHER ? S.Namen(USB_ERW + "/user/addcont") : new List<string>();
            var benutzt = H.Tab<int>();
            DL_NAMEN = null; DL_USB = null; DL_USB_UNKLAR = false;
            var sysmeta = S.Namen(SYS_META, true);
            Func<List<string>, string, List<string>> mitKachel = (pf, id) =>
            {
                if (H.Hat(sysmeta, id) && !H.Like(id, "NPXS*") && !H.Hat(ASTRO_IDS, id)) { var n = new List<string>(pf); n.Add(SYS_META + "/" + id); return n; }
                return pf;
            };
            // Namen aller Kacheln zuerst lesen - dabei wird Astro unter jeder
            // Länderkennung erkannt, bevor irgendetwas anderes gelistet wird.
            ASTRO_IDS = new List<string>(); ASTRO_NPWRS = new List<string>();   // 1.0.6: Astro ist ein normales Spiel
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
            QuelleInfo = new Dictionary<string, QInfo>(StringComparer.Ordinal);
            SperrQuellen = new List<string>();
            ORDNER_CACHE = H.Tab<List<string>>();

            // ---- 1 Spiele & Apps (und Tool-Kacheln -> 6) --------------------
            // NEU 1.0.6 (Codex-Prüfung 31, Anlass push_resource): angeboten werden in den Sony-Spielordnern (app, patch, addcont,
            // appmeta/addcont, appmeta/external/addcont - intern, M.2, USB) nur Ordner mit Spielnummer; alles andere bleibt unangetastet
            foreach (var x in apps)
            {
                if (H.Hat(ASTRO_IDS, x) || H.Like(x, "NPXS*") || !H.M(x, "^[A-Z]{4}\\d{5}$")) continue;
                string t = AppTitel(x);
                if (IstAstro(t)) { AstroMerken(x); continue; }
                string roh = APP_ROH;
                if (t == "") t = T("unbekannter Name", "unknown name");
                var p = L("/user/app/" + x);
                bool zwilling = H.Hat(m2Apps, x) || H.Hat(usbApps, x);
                if (H.Hat(appmeta, x)) { benutzt["/user/appmeta/" + x] = 1; if (!zwilling) p.Add("/user/appmeta/" + x); }
                if (!zwilling) p = mitKachel(p, x);
                var inhalt = S.Namen("/user/app/" + x, true);
                if (H.M(roh, "\"deeplinkUri\"\\s*:\\s*\"https?://(127\\.0\\.0\\.1|localhost)"))
                {
                    string ad = zwilling ? null : AppDaten(x);   // NEU 1.0.6: App-Daten gehen mit der Kachel
                    if (ad != null) p.Add(ad);
                    Merke(p, 6, T("Tool-Kachel: ", "Tool tile: ") + t + " (" + x + ")", kachel: true);
                }
                else if (H.Hat(inhalt, "mount.lnk") || H.Hat(inhalt, "mount_img.lnk"))
                {
                    string q = SmQuelle(x);
                    string ort = QuelleOrt(q);
                    bool da = false;
                    if (ort != "") da = S.Existiert(q);
                    // NEU 1.0.6: trägt die Datei am gemerkten Ort eine ANDERE Kennung im Namen, ist es nicht dieses Spiel
                    bool fremd = false;   // dann: unklar - Datei sperren, Kachel nur Kachel
                    if (da) { var m = H.Mt(q.Substring(q.LastIndexOf('/') + 1), "([A-Z]{4}\\d{5})"); if (m.Success && !string.Equals(m.Groups[1].Value, x, StringComparison.OrdinalIgnoreCase)) { da = false; fremd = true; } }
                    string kand = null; var kands = new List<string>();
                    if (ort != "" && !da)               // NEU 1.0.6: verschobene Spieldatei suchen - nur als Hinweis
                    {
                        if (fremd) kands.Add(q);
                        kands.AddRange(SmNeueOrte(q, x));
                        if (kands.Count > 0) kand = kands[0];
                    }
                    if (ort != "" && da)
                    {
                        bool g = WirdKomprimiert(q, x);
                        QInfo alt = QuelleInfo.ContainsKey(q) ? QuelleInfo[q] : null;
                        bool schon = SpielQuellen.Contains(q);   // NEU 1.0.6: zeigt schon eine andere Kachel hierher (auch nur vermutet) -> mehrdeutig
                        if (!schon) SpielQuellen.Add(q);
                        var qi = new QInfo { Links = p, Name = t + " (" + x + ")", Gesperrt = g };
                        qi.Jb = JbEinstieg(x) || (alt != null && alt.Jb);
                        qi.JbName = JbEinstieg(x) ? qi.Name : (alt != null ? alt.JbName : null);
                        if (schon) { if (alt != null) alt.Mehrfach = true; qi.Mehrfach = true; }
                        QuelleInfo[q] = qi;
                        Merke(p, 1, t + " (" + x + ")" + T(" - ShadowMount-Spiel", " - ShadowMount game") + (ort == "intern" ? T(" auf der internen SSD", " on the internal SSD") : ""), q, g, kachel: true);
                    }
                    // da_Chris86 27.10.2026 19:38: die Kachel bleibt immer löschbar (nur Kachel); gesperrt wird nur eine nicht erreichbare/unklare Spieldatei
                    else if (ort != "" && kand != null)   // NEU 1.0.6: vermutlich verschoben - Kachel: nur Kachel; vermutete Datei: gesperrt
                    {
                        Merke(p, 1, t + " (" + x + ")" + T(" - ShadowMount: Spiel vermutlich verschoben nach ", " - ShadowMount: game probably moved to ") + kand + T(", nur die Kachel wird gelöscht", ", only the tile is deleted"), kachel: true);
                        foreach (var kd in kands)   // alle Kandidaten sperren
                        {
                            if (QuelleInfo.ContainsKey(kd)) { QuelleInfo[kd].Mehrfach = true; if (JbEinstieg(x)) { QuelleInfo[kd].Jb = true; QuelleInfo[kd].JbName = t + " (" + x + ")"; } continue; }
                            SpielQuellen.Add(kd); QuelleInfo[kd] = new QInfo { Name = t + " (" + x + ")", Gesperrt = true, Vermutet = true, Jb = JbEinstieg(x), JbName = JbEinstieg(x) ? t + " (" + x + ")" : null };
                        }
                    }
                    else if (ort != "")
                    {
                        // NEU 1.0.6: USB-Speicher/USB-Stick gerade nicht angesteckt -> nicht "tote Kachel", sondern Datenträger fehlt
                        var w = ort != "M.2" ? H.Mt(q, "^(/mnt/[^/]+)/") : null;
                        if (w != null && w.Success && !S.Existiert(w.Groups[1].Value))
                            Merke(p, 1, t + " (" + x + ") - ShadowMount: " + T("Datenträger mit dem Spiel nicht gefunden (", "drive with the game not found (") + q + ")" + T(", nur die Kachel wird gelöscht", ", only the tile is deleted"), kachel: true);
                        else Merke(p, 1, t + " (" + x + ")" + T(" - ShadowMount: Spiel nicht mehr da, tote Kachel", " - ShadowMount: game gone, dead tile"), kachel: true, smTipp: true);
                    }
                    else
                    {
                        string wo = q == "" ? T("Speicherort unbekannt", "location unknown") : T("Spiel liegt auf ", "game is at ") + q;
                        if (q.StartsWith("/mnt/") && !S.Existiert(q))   // NEU 1.0.6: Wechseldatenträger fehlt -> Text sagt das; die Kachel bleibt löschbar (nur Kachel)
                            wo = T("Datenträger mit dem Spiel nicht gefunden (", "drive with the game not found (") + q + ")";
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
                if (n == 0 && H.M(pf, "^/user/av_contents/content_tmp/sharevideo_[0-9A-Fa-f]+_hdr\\.webm$")) continue;   // laufende Aufnahme der PS5 - nie anbieten (wie 1.1 U16, Codex-Prüfung 24)
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
            // NEU 1.0.6 (27.10.2026): /user/download/<ID>/download0.dat ist der dauerhafte App-Datenspeicher (/download0)
            // einer App (UFS2-Abbild, Größe aus downloadDataSize) - kein Download-Rest. Er wird NIE allein wegen seines
            // Speicherorts angeboten: nur wenn zu der Kennung keine Installation und keine Registrierung mehr zu finden ist
            // (interne SSD, M.2, USB-Erweiterung, /system_ex, Kachel-Infos, externe Kachel-Infos). Die Daten einer installierten
            // App gehen mit der App. Jailbreak-Einstiege: nur einzeln, vollständig auf Nachfrage.
            var dlUsb = USB_SPEICHER ? S.Namen(USB_ERW + "/user/app", true) : new List<string>();
            var dlSysEx = S.Namen("/system_ex/app", true);
            var dlExtern = S.Namen(SYS_META + "/external", true);
            foreach (var x in S.Namen("/user/download"))
            {
                if (H.Like(x, "NPXS*")) continue;
                if (!H.M(x, "^[A-Z]{4}\\d{5}$")) continue;   // NEU 1.0.6: nur Ordner mit Spielnummer (wie Kachel-Reste)
                if (H.Hat(apps, x) || H.Hat(m2Apps, x) || H.Hat(dlUsb, x) || H.Hat(dlSysEx, x) || H.Hat(appmeta, x) || H.Hat(sysmeta, x) || H.Hat(dlExtern, x)) continue;   // App noch da - ihre Daten werden gebraucht
                // da_Chris86 27.10.2026 21:39: keine pauschale Sperre bei fehlender/nicht lesbarer M.2 oder USB-Erweiterung
                string dt = JbEinstieg(x) ? T("App-Daten eines Jailbreak-Einstiegs: ", "App data of a jailbreak entry: ") + MitName(x)
                                          : T("App-Daten ohne installierte App: ", "App data without installed app: ") + MitName(x);
                Merke(L("/user/download/" + x), 5, dt);
            }
            foreach (var x in patches) if (!benutzt.ContainsKey("/user/patch/" + x) && !H.Like(x, "NPXS*") && H.M(x, "^[A-Z]{4}\\d{5}$")) Merke(L("/user/patch/" + x), 5, T("Update ohne Spiel: ", "Update without game: ") + MitName(x));
            foreach (var x in addconts) if (!benutzt.ContainsKey("/user/addcont/" + x) && !H.Like(x, "NPXS*") && H.M(x, "^[A-Z]{4}\\d{5}$")) Merke(L("/user/addcont/" + x), 5, T("Zusatzinhalt: ", "Add-on: ") + MitName(x));
            foreach (var x in S.Namen("/user/appmeta/external/addcont")) if (!H.Like(x, "NPXS*") && H.M(x, "^[A-Z]{4}\\d{5}$")) Merke(L("/user/appmeta/external/addcont/" + x), 5, T("Zusatzinhalt-Infos: ", "Add-on info: ") + MitName(x));
            foreach (var x in S.Namen("/user"))
                if (H.Like(x, "_zu_loeschen_*")) Merke(L("/user/" + x), 5, T("Rest einer früheren Aufräumaktion: ", "Leftover from an earlier cleanup: ") + "/user/" + x);
            // ---- M.2 und USB-Speicher (bis 1.0.5: eigener Bereich 7 "Inhalt der M.2-SSD") ------------------------------
            // NEU 1.0.6 (da_Chris86 28.10.2026): Spiele -> Bereich 1, Updates/Zusatzinhalte ohne Spiel und übrige Dateien -> Bereich 5,
            // ELF-Dateien -> Bereich 6, jeweils mit Ort vorne. Der USB-Speicher (erweiterter Speicher) wird genauso behandelt wie die M.2.
            {
                List<Datei> usbListe = null;
                if (USB_SPEICHER) usbListe = S.Liste(USB_ERW, true);
                var wurzel = new[] { M2, USB_ERW };
                var da = new[] { m2Da, USB_SPEICHER && usbListe != null };
                var liste = new[] { m2Liste, usbListe };
                var eApps = new[] { m2Apps, usbApps }; var ePatches = new[] { m2Patches, usbPatches }; var eAddconts = new[] { m2Addconts, usbAddconts };
                for (int w = 0; w < 2; w++)
                {
                    if (!da[w]) continue;
                    string b = wurzel[w], bu = b + "/user";
                    foreach (var e in liste[w])
                        if (H.Like(e.Name, "_zu_loeschen_*")) Merke(L(b + "/" + e.Name), 5, T("Rest einer früheren Aufräumaktion: ", "Leftover from an earlier cleanup: ") + b + "/" + e.Name);
                    foreach (var x in eApps[w])
                    {
                        if (H.Like(x, "NPXS*") || H.Hat(ASTRO_IDS, x) || !H.M(x, "^[A-Z]{4}\\d{5}$")) continue;
                        string t = AppTitel(x);
                        if (IstAstro(t)) { AstroMerken(x); continue; }
                        if (t == "") t = T("unbekannter Name", "unknown name");
                        var p = L(bu + "/app/" + x);
                        // eine weitere SICHTBARE Kopie (intern, M.2, USB) braucht dieselbe Kachel - dann bleiben die Kachel-Infos
                        bool zwilling = H.Hat(apps, x) || H.Hat(eApps[1 - w], x);
                        if (!zwilling)
                        {
                            if (H.Hat(appmeta, x)) { p.Add("/user/appmeta/" + x); benutzt["/user/appmeta/" + x] = 1; }
                            p = mitKachel(p, x);
                        }
                        MerkeSpielteile(p, 1, t, x, bu, H.Hat(ePatches[w], x), H.Hat(eAddconts[w], x), dlcInfos, benutzt, zwilling);
                    }
                    foreach (var x in ePatches[w])
                        if (!benutzt.ContainsKey(bu + "/patch/" + x) && !H.Like(x, "NPXS*") && H.M(x, "^[A-Z]{4}\\d{5}$")) Merke(L(bu + "/patch/" + x), 5, T("Update ohne Spiel: ", "Update without game: ") + MitName(x));
                    foreach (var x in eAddconts[w])
                        if (!benutzt.ContainsKey(bu + "/addcont/" + x) && !H.Like(x, "NPXS*") && H.M(x, "^[A-Z]{4}\\d{5}$")) Merke(L(bu + "/addcont/" + x), 5, T("Zusatzinhalte ohne Spiel: ", "Add-ons without game: ") + MitName(x));
                    foreach (var e in liste[w])
                    {
                        string x = e.Name;
                        if (H.Gleich(x, "lost+found") || H.Gleich(x, "system_data") || H.Gleich(x, "user") || H.Like(x, "_zu_loeschen_*")) continue;
                        MerkeExtOrdner(b, e);
                    }
                }
            }
            // ---- normale USB-Sticks (NEU 1.0.6, da_Chris86 28.10.2026): anzeigen, nur einzeln löschbar ---------------------
            // Nur die oberste Ebene (Ordner als Ganzes); Ordner mit ShadowMount-Spielen werden aufgeklappt.
            // Nie in der vollständigen Bereinigung und nie über "ganzen Bereich wählen".
            STICK_DA = false;
            for (int u = 0; u < 8; u++)
            {
                string b = "/mnt/usb" + u;
                List<Datei> l = null;
                try { l = S.Liste(b); } catch (AbbruchAusnahme) { throw; } catch { l = null; }
                if (l == null) continue;
                STICK_DA = true;
                // Verwaltungsordner von Windows/macOS auf dem Stick nicht anbieten
                l = l.Where(e => !H.M(e.Name, "(?i)^(System Volume Information|\\$RECYCLE\\.BIN|\\.Trashes|\\.Spotlight-V100|\\.fseventsd)$")).ToList();
                // NEU 1.0.6 (Codex-Prüfung 27): ein Lesefehler auf dem Stick bricht nicht die ganze Suche ab - dieser Stick
                // wird dann gar nicht angezeigt, die übrige Suche läuft weiter
                MERKE_STICK = true;
                int vorher = bau.Count;
                try { foreach (var e in l) MerkeOrdner(b, new[] { e }, ExtKat(e.Name), ""); }
                catch (AbbruchAusnahme) { throw; }
                catch { bau.RemoveRange(vorher, bau.Count - vorher); }
                finally { MERKE_STICK = false; }
            }
            foreach (var x in appmeta)
            {
                if (H.Hat(ASTRO_IDS, x) || H.Gleich(x, "external") || H.Gleich(x, "addcont") || H.Like(x, "NPXS*")) continue;
                // NEU 1.0.6 (Protokoll eines Nutzers 28.10.2026, FW 13.60): nur Ordner mit Spielnummer (z. B. CUSA06014) - Sony-Ordner
                // wie "push_resource" sind kein Spiel (1.0.5 bot sie an, der Sicherheitsstopp verhinderte das Löschen)
                if (!H.M(x, "^[A-Z]{4}\\d{5}$")) continue;
                if (H.Hat(apps, x) || benutzt.ContainsKey("/user/appmeta/" + x) || H.Hat(usbApps, x)) continue;   // 27.10.2026: USB-Kopie wie 1.1
                string t = AppTitel(x);
                if (IstAstro(t)) { AstroMerken(x); continue; }
                if (t == "") t = T("entferntes Spiel", "removed game");
                Merke(mitKachel(L("/user/appmeta/" + x), x), 5, T("Reste: ", "Leftovers: ") + t + " (" + x + ")");
            }
            var shareIds = new List<string>();
            foreach (var x in S.Namen("/user/system/share"))
            {
                if (!H.M(x, "^[A-Z]{4}\\d{5}$") || H.Like(x, "NPXS*") || H.Hat(ASTRO_IDS, x)) continue;
                if (H.Hat(apps, x) || H.Hat(m2Apps, x) || H.Hat(usbApps, x)) continue;   // 27.10.2026: USB-Kopie wie 1.1
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
                if (H.Hat(apps, x) || H.Hat(appmeta, x) || H.Hat(m2Apps, x) || H.Hat(usbApps, x)) continue;   // 27.10.2026: USB-Kopie wie 1.1
                string t = AppTitel(x);
                if (IstAstro(t)) { AstroMerken(x); continue; }
                if (t == "") t = T("unbekannter Name", "unknown name");
                Merke(L(SYS_META + "/" + x), 5, T("Leere Kachel (Schloss): ", "Empty tile (lock icon): ") + t + " (" + x + ")", kachel: true);
            }
            foreach (var x in dlcInfos) if (!benutzt.ContainsKey("/user/appmeta/addcont/" + x) && !H.Like(x, "NPXS*") && H.M(x, "^[A-Z]{4}\\d{5}$")) Merke(L("/user/appmeta/addcont/" + x), 5, T("Zusatzinhalt-Infos: ", "Add-on info: ") + MitName(x));

            // ---- 6 Jailbreak-Tools & Homebrew-Daten -------------------------
            var dl = S.Liste("/user/data", true) ?? new List<Datei>();
            MerkeOrdner("/user/data", dl, 6, "");
            var fakeLiz = new List<string>();
            // NEU 1.0.6 (Codex-Prüfung 31): nur Dateien, keine Ordner
            foreach (var e in S.Liste("/user/license", true) ?? new List<Datei>())
                if (!e.IstOrdner && H.M(e.Name, "^fake.+\\.(rif|idx)$")) fakeLiz.Add(e.Name);
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
                    int kopien = (H.Hat(apps, tid) ? 1 : 0) + (H.Hat(m2Apps, tid) ? 1 : 0) + (H.Hat(usbApps, tid) ? 1 : 0);
                    if (spiele.Count == 1 && kopien > 1) continue;   // NEU 1.0.6: eine andere SICHTBARE Kopie braucht die Lizenz
                    if (spiele.Count == 1) { spiele[0].Pfade = spiele[0].Pfade.Concat(pf).ToList(); spiele[0].PfadeBasis = new List<string>(spiele[0].Pfade); continue; }
                    if (spiele.Count > 1 || H.Hat(apps, tid) || H.Hat(m2Apps, tid) || H.Hat(usbApps, tid)) continue;
                }
                string n = H.R(g, "^fake", "");
                if (tm.Success) { string mn = MitName(tm.Groups[1].Value); if (mn != tm.Groups[1].Value) n = n + " - " + H.R(mn, "^[A-Z]{4}\\d{5} ", ""); }
                Merke(pf, 6, T("Fake-Lizenz: ", "Fake license: ") + n + T(" (Spiel nicht mehr installiert)", " (game no longer installed)"));
            }
            foreach (var u in homes)
                if (S.Namen("/user/home/" + u + "/webkit/shell/local").Count > 0)
                {
                    Merke(L("/user/home/" + u + "/webkit/shell/local"), 6, T("Browserspeicher (Jailbreak-Seiten, evtl. mit offline gespeichertem Jailbreak)", "Browser storage (jailbreak pages, may contain an offline-saved jailbreak page)"));
                    // da_Chris86 29.10.2026: kann einen offline gespeicherten WebKit-Jailbreak enthalten - Jailbreak-Einstieg (nur einzeln,
                    // die vollständige Bereinigung fragt; bis 29.10.2026 wie 1.0.5 auch in der vollständigen Bereinigung)
                    var b = MERKE_OK && bau.Count > 0 ? bau[bau.Count - 1] : null;
                    if (b != null && !b.Gesperrt) { b.Jailbreak = true; b.NurEinzeln = true; b.Text = b.TextBasis = b.Text + T("  [Jailbreak-Einstieg - nur einzeln]", "  [jailbreak entry - individually only]"); }
                }
            // NEU 1.0.6 (da_Chris86 29.10.2026 19:12): Blu-ray-Jailbreak-Disc im Laufwerk - nur lesen. Eine Jailbreak-Disc hat neben
            // BDMV eigene .jar/.elf-Dateien im obersten Ordner (BD-JB-Autoloader: poops.jar, autoloader.jar, ps5-unified-autoloader.elf)
            BD_DISC = false;
            try { var d = S.Namen("/mnt/disc"); BD_DISC = d.Contains("BDMV") && d.Any(x => H.M(x, "(?i)\\.(jar|elf)$")); } catch (AbbruchAusnahme) { throw; } catch { BD_DISC = false; }

            // NEU 1.0.6: Jailbreak-Einstiege (App, Update, Zusatzinhalt, Kachel-Infos, App-Daten): einzeln löschbar, die vollständige
            // Bereinigung fragt, ob sie bleiben. Gehören alle Pfade eines Eintrags zum Einstieg: Eintrag markieren. Sammeleinträge (mehrere Titel): nur die
            // Pfade des Einstiegs herausnehmen, der Rest bleibt wählbar.
            // NEU 1.0.6: nicht sicher zugeordnete Spieldateien merken - der Löschcode fasst sie nie an, auch nicht über einen Elternordner
            // da_Chris86 28.10.2026 13:41: Spiel-Abbilder sind als eigene Spiele löschbar - SperrQuellen bleibt leer
            // NEU 1.0.6: zeigen mehrere Kacheln auf dieselbe Spieldatei, geht bei "Spiel + Kachel" nur die Kachel
            foreach (var f in bau)
            {
                if (f.Kat != 1 || string.IsNullOrEmpty(f.Quelle) || !QuelleInfo.ContainsKey(f.Quelle) || !QuelleInfo[f.Quelle].Mehrfach) continue;
                f.Quelle = "";
                // NEU 1.0.6 (Codex-Prüfung 27): ohne Spieldatei zählt der Ort der übrigen Pfade (meist intern) - nicht der Ort des Abbilds
                int alt = f.Ort, neu = ORT_INTERN;
                foreach (var p in f.Pfade) { neu = PfadOrt(p); if (neu != ORT_INTERN) break; }
                if (neu != alt)
                {
                    string v = OrtVorsatz(alt);
                    f.Text = OrtVorsatz(neu) + (f.Text.StartsWith(v, StringComparison.Ordinal) ? f.Text.Substring(v.Length) : f.Text);
                    f.Ort = neu;
                    if (alt == ORT_STICK && neu != ORT_STICK) f.NurEinzeln = false;
                }
                f.Text = f.TextBasis = f.Text + T(" - die Spieldatei gehört zu mehreren Kacheln, nur die Kachel wird gelöscht", " - the game file belongs to several tiles, only the tile is deleted");
            }
            var jbNeu = new List<Eintrag>();
            foreach (var f in bau)
            {
                if (f.Gesperrt || !f.Pfade.Any(p => JbPfad(p))) continue;
                bool abbild = !string.IsNullOrEmpty(f.Quelle) || f.Pfade.Any(p => IstAbbild(p));
                var rest = f.Pfade.Where(p => !JbPfad(p)).ToList();
                bool sammel = rest.Count > 0 && f.Pfade.Select(p => H.Mt(p, "([A-Z]{4}\\d{5})").Groups[1].Value).Distinct().Count() > 1;
                if (sammel)
                {
                    // der herausgenommene Teil bleibt sichtbar - als eigener Jailbreak-Eintrag
                    var jbp = f.Pfade.Where(p => JbPfad(p)).ToList();
                    string ids = string.Join(", ", jbp.Select(p => H.Mt(p, "([A-Z]{4}\\d{5})").Groups[1].Value).Distinct());
                    string tx = T("Jailbreak-Einstieg ", "Jailbreak entry ") + ids + " (" + H.R(f.TextBasis, "\\s*\\(.*$", "") + ")" + T("  [Jailbreak-Einstieg - nur einzeln]", "  [jailbreak entry - individually only]");
                    jbNeu.Add(new Eintrag { Pfade = jbp, PfadeBasis = new List<string>(jbp), Kat = 6, Text = tx, TextBasis = tx, Kachel = f.Kachel, Jailbreak = true, NurEinzeln = true, Ort = f.Ort });
                    f.Pfade = rest; f.PfadeBasis = new List<string>(rest); continue;
                }
                f.Jailbreak = true; f.NurEinzeln = true; f.Text = f.TextBasis = f.Text + T("  [Jailbreak-Einstieg - nur einzeln]", "  [jailbreak entry - individually only]");
                // da_Chris86 28.10.2026 22:55: Jailbreak-Einstiege stehen unter "Jailbreak & Homebrew". da_Chris86 29.10.2026: ein ShadowMount-Spiel
                // (mit Spieldatei) bleibt unter "Spiele" - dort ist das Löschen von Spieldatei und Kachel geregelt; geschützt und gewarnt wird es trotzdem
                if (!(f.Kat == 1 && abbild)) f.Kat = 6;   // Codex-Prüfung 36: auch ein Spiel-Abbild ohne Kachel bleibt unter "Spiele"
            }
            bau.AddRange(jbNeu);
            // fest nach Bereich ordnen (innerhalb eines Bereichs bleibt die Fundreihenfolge)
            var sortiert = new List<Eintrag>();
            // fest nach Bereich ordnen, darin nach Ort (intern, M.2, USB, USB-Stick - NEU 1.0.6); sonst bleibt die Fundreihenfolge
            foreach (var k in ALLE_KAT) for (int o = ORT_INTERN; o <= ORT_STICK; o++) foreach (var f in bau) if (f.Kat == k && f.Ort == o) sortiert.Add(f);
            sortiert = ToolDatenOrdnen(sortiert);
            Funde = sortiert;
            FUNDE_IP = S.Ftp.Ip + ":" + S.Ftp.Port;
        }
    }
}
