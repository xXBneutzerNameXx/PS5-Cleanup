// PS5 Cleanup 1.1 - Referenz-Testprogramm: Kopie von _test104\TestKern.cs,
// ergänzt NUR um die Ausgaben "detail", "auswahl" und "pruefen" (Prüflauf vor Schritt 10, nur lesen). Der Kern ist unverändert 1.0.5 (Quellcode\*.cs).
// Regressionstest des C#-Kerns gegen die Test-PS5 (miniftp3.py) -
// dieselben Szenarien wie regress.ps1 der PowerShell-Version.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace PS5Cleanup
{
    public static class TestKern
    {
        static int fehler = 0;
        static void Pruefe(bool ok, string was) { Console.WriteLine((ok ? "OK   " : "FAIL ") + was); if (!ok) fehler++; }

        public static int Main(string[] args)
        {
            Console.OutputEncoding = new UTF8Encoding(false);
            string sz = args[0]; int port = 2199; if (args.Length > 1) int.TryParse(args[1], out port);
            if (sz == "erlaubt")
            {
                var KE = new Kern(); KE.ASTRO_IDS.Add("PPSA01326"); KE.ASTRO_NPWRS.Add("NPWR99999_00");
                var js = new System.Web.Script.Serialization.JavaScriptSerializer();
                var aus = new List<string>();
                foreach (var z in File.ReadAllLines(args[1], Encoding.UTF8)) { if (z == "") continue; aus.Add(KE.Erlaubt(js.Deserialize<string>(z)) ? "1" : "0"); }
                File.WriteAllLines(args[2], aus);
                return 0;
            }
            if (sz == "laufwerk") return Laufwerk();
            if (sz == "feedback") return FeedbackTest(args.Length > 2 ? args[2] : ".");
            if (sz == "elfcheck") return ElfCheck(args[1], args[2]);
            if (sz == "serverinfo")
            {
                // NEU 1.0.5: echte PS5 nur lesend: anmelden, Server erkennen, /user auflisten, abmelden
                var f = Ftp.Verbinden(args[1], int.Parse(args[2]), 5000);
                if (f == null) { Console.WriteLine("KEINE VERBINDUNG: " + Ftp.VerbindenFehler); return 1; }
                Console.WriteLine("BEGRUESSUNG: " + Ftp.BegruessungKurz(f.Begruessung));
                Console.WriteLine("SERVER: " + f.ServerErkennen());
                var s = new Sitzung(f);
                var u = s.Liste("/user");
                Console.WriteLine("LISTE /user: " + (u == null ? "FEHLER " + f.LetzterFehler : u.Count + " Einträge"));
                Console.WriteLine("DATENVERBINDUNG: " + f.DatenArt());
                f.Trennen();
                return 0;
            }
            string hier = args.Length > 2 ? args[2] : ".";
            // Sprachvergleich: SPRACHE=en -> der Kern läuft auf Englisch (gelöscht werden muss genau dasselbe)
            if (Environment.GetEnvironmentVariable("SPRACHE") == "en") Sp.EN = true;
            var K = new Kern();
            K.Melde = t => Console.WriteLine("LOG: " + t);
            K.Warte = ms => System.Threading.Thread.Sleep(ms);
            // Schritt 9: REF_IP = IP der echten PS5 (nur "detail" = nur lesen); sonst wie bisher der Test-Server
            // Sicherung: gegen eine echte PS5 sind NUR "detail" und "pruefen" erlaubt (lesen nur) - alle anderen Szenarien löschen!
            if (Environment.GetEnvironmentVariable("REF_IP") != null && sz != "detail" && sz != "pruefen") { Console.WriteLine("FAIL REF_IP ist nur mit detail oder pruefen erlaubt"); return 1; }
            if (!K.Verbinden(Environment.GetEnvironmentVariable("REF_IP") ?? "127.0.0.1", port)) { Console.WriteLine("FAIL keine Verbindung"); return 1; }
            string suchFehler = "";
            try { K.SucheFunde(); } catch (Exception ex) { suchFehler = ex.Message; }
            Func<string, List<Eintrag>> finde = m => K.Funde.Where(f => H.Like(f.Text, m)).ToList();
            if (sz == "verweigert")
            {
                // NEU 1.0.5: ein vorhandener, aber gesperrter Hauptordner darf nicht als leer gelten
                Console.WriteLine("Suche: " + suchFehler);
                Pruefe(suchFehler != "" && K.FUNDE_IP == "" && (suchFehler.Contains("verweigert") || suchFehler.Contains("denies")), "Zugriff verweigert -> Suche bricht ab, keine Liste");
                var e0 = K.FuehreLoeschung(() => false);
                Pruefe(e0.Abgebrochen, "Löschen ohne gültige Liste abgebrochen");
                Console.WriteLine("FEHLERZAHL " + fehler); return 0;
            }
            if (sz == "keindaten")
            {
                // NEU 1.0.5: Server kennt weder EPSV noch PASV -> Suche bricht ab, Grund ist bekannt
                Console.WriteLine("Suche: " + suchFehler + " | Grund: " + K.S.Ftp.LetzterFehler);
                Pruefe(suchFehler != "" && K.FUNDE_IP == "", "ohne Datenverbindung: Suche bricht ab, keine Liste");
                Pruefe(K.S.Ftp.LetzterFehler.Contains("EPSV") && K.S.Ftp.LetzterFehler.Contains("PASV"), "Grund nennt EPSV und PASV");
                var e0 = K.FuehreLoeschung(() => false);
                Pruefe(e0.Abgebrochen, "Löschen ohne gültige Liste abgebrochen");
                Console.WriteLine("FEHLERZAHL " + fehler); return 0;
            }
            if (sz == "m2listfehler")
            {
                Console.WriteLine("Suche: " + suchFehler);
                Pruefe(suchFehler != "" && K.FUNDE_IP == "", "gescheiterte M.2-Liste bricht die Suche ab (keine Liste zum Löschen)");
                var e0 = K.FuehreLoeschung(() => false);
                Pruefe(e0.Abgebrochen, "Löschen ohne gültige Liste abgebrochen");
                Console.WriteLine("FEHLERZAHL " + fehler); return 0;
            }
            if (suchFehler != "") { Console.WriteLine("FAIL Suche: " + suchFehler); return 1; }
            switch (sz)
            {
                case "ftpname":
                case "ftpsrvname":
                case "zftpdname":
                    {
                        // NEU 1.0.5: FTP-Server erkennen - nur was der Server selbst eindeutig sagt
                        string v;
                        Pruefe(Ftp.ServerAusText("220-Welcome to ftpsrv.elf running on pid 42\n220-Version: v0.21.1 (built Oct  1 2026 12:00:00)\n220 Service is ready", "", out v) == "ftpsrv" && v == "0.21.1", "Text: ftpsrv mit Version 0.21.1 (" + v + ")");
                        Pruefe(Ftp.ServerAusText("220 Service ready for new user.", "211-Extensions supported:\n211- EPSV\n211- XZFTPD INSTANCE ab12\n211 End", out v) == "zftpd" && v == "", "Text: zftpd über FEAT, Version leer");
                        Pruefe(Ftp.ServerAusText("220 Service ready for new user.", " EPSV\n XZFTPD INSTANCE ab12", out v) == "zftpd", "Text: zftpd über FEAT (Zeilen ohne Code)");
                        Pruefe(Ftp.ServerAusText("220 Service ready for new user.", "", out v) == "", "Text: nichts Eindeutiges -> unbekannt");
                        Pruefe(Ftp.ServerAusText("220 ProFTPD 1.3.5 Server ready", "", out v) == "" && v == "", "Text: fremder Server wird nicht geraten");
                        Pruefe(Ftp.ServerAusText("220 ftpsrv ready", "", out v) == "ftpsrv" && v == "", "Text: ftpsrv ohne Version -> Version leer");
                        Pruefe(Ftp.ServerAusText("220 zftpd v1.6.0 ready", "", out v) == "zftpd" && v == "1.6.0", "Text: Name mit Version direkt dahinter (" + v + ")");
                        Pruefe(Ftp.ServerAusText("220 Welcome", "211- UTF8\n211- MLSD", out v) == "", "Text: FEAT ohne Kennung -> unbekannt");
                        Pruefe(Ftp.BegruessungKurz("220-Hallo\n220 Service ready") == "Hallo Service ready", "Begrüßung kurz ohne Codes");

                        string s = K.S.Ftp.ServerErkennen();
                        string art = K.S.Ftp.EpsvBenutzt ? "EPSV" : K.S.Ftp.PasvBenutzt ? "PASV" : "?";
                        Console.WriteLine("SERVER '" + s + "'  Datenverbindung " + art + "  Begrüßung '" + Ftp.BegruessungKurz(K.S.Ftp.Begruessung) + "'");
                        if (sz == "ftpname")
                        {
                            Pruefe(s == "", "Fake-Server ohne Kennung -> unbekannt (FEAT abgelehnt)");
                            Pruefe(art == "EPSV" && K.S.Ftp.DatenArt() == "EPSV", "Datenverbindung EPSV erkannt");
                        }
                        if (sz == "ftpsrvname")
                        {
                            Pruefe(s == "ftpsrv, Version 0.21.1", "ftpsrv-Begrüßung erkannt: " + s);
                            Pruefe(art == "PASV" && !K.S.Ftp.EpsvBenutzt && K.S.Ftp.DatenArt().StartsWith("PASV"), "Datenverbindung PASV erkannt");
                        }
                        if (sz == "zftpdname")
                        {
                            Pruefe(s.StartsWith("zftpd") && s.Contains(Feedback.Unbekannt) && !s.Contains("0123456789abcdef"), "zftpd über FEAT erkannt, Version unbekannt, keine Instanz-Kennung: " + s);
                            Pruefe(art == "EPSV", "Datenverbindung EPSV erkannt");
                        }
                        // Nach der Erkennung läuft die Verbindung normal weiter
                        var u = K.S.Liste("/user");
                        Pruefe(u != null && u.Count > 0, "Verbindung nach der Erkennung weiter nutzbar");
                        // Feedback: Angaben drin, IP aus einer Begrüßung entfernt
                        var d = new FeedbackDaten { FtpServer = s != "" ? s : Feedback.Unbekannt + " (Begrüßung: „Welcome 192.168.1.77“)", FtpDaten = art, Port = port };
                        string rb = Feedback.Bericht(d);
                        Pruefe(rb.Contains(s != "" ? s : Feedback.Unbekannt) && rb.Contains(art) && !rb.Contains("192.168.1.77"), "Feedback-Bericht: FTP-Server und Datenverbindung drin, keine IP");
                    }
                    break;
                case "nurpasv":
                    // NEU 1.0.5: Server ohne EPSV -> alles läuft über PASV
                    Pruefe(K.Funde.Count == 57, "Suche über PASV vollständig (" + K.Funde.Count + " Einträge)");
                    Pruefe(K.S.Ftp.NurPasv, "EPSV abgelehnt, auf PASV gewechselt");
                    {
                        K.WaehleKomplett(true, true);
                        var e = K.FuehreLoeschung(() => false);
                        Console.WriteLine("ERG Fehler=" + e.Fehler + " Abgebrochen=" + e.Abgebrochen);
                        Pruefe(e.Fehler == 0 && !e.Abgebrochen, "vollständige Bereinigung inkl. M.2 über PASV ohne Fehler");
                    }
                    break;
                case "verknuepfung":
                    {
                        // Schritt 2 (1.1): OhneVerknuepfung für jeden Pfad aus VPFADE (JSON-Zeilen)
                        var js2 = new System.Web.Script.Serialization.JavaScriptSerializer();
                        var ov = typeof(Kern).GetMethod("OhneVerknuepfung", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
                        foreach (var z in File.ReadAllLines(Environment.GetEnvironmentVariable("VPFADE"), Encoding.UTF8))
                        {
                            if (z == "") continue;
                            string p = js2.Deserialize<string>(z);
                            string r;
                            try { r = ((bool)ov.Invoke(K, new object[] { p })) ? "1" : "0"; }
                            catch (System.Reflection.TargetInvocationException ex) { r = "AUSNAHME " + ex.InnerException.Message; }
                            Console.WriteLine("V " + r + " " + p);
                        }
                    }
                    break;
                case "detail":
                    foreach (var f in K.Funde) Console.WriteLine("DETAIL " + Zeile(f));
                    Console.WriteLine("HINWEIS " + K.SpeicherHinweis().Trim().Replace("\n", " / "));
                    Console.WriteLine("ANZAHL " + K.Funde.Count);
                    break;
                case "auswahl":
                    {
                        Action<string> dump = titel => {
                            Console.WriteLine("== " + titel);
                            foreach (var f in K.Funde) if (f.Wahl) Console.WriteLine("WAHL " + f.Kat + " | " + f.Text + " :: " + string.Join(" | ", f.Pfade));
                        };
                        K.WaehleKomplett(false, false); dump("KOMPLETT");
                        K.WaehleKomplett(true, true); dump("KOMPLETT_M2");
                        K.WaehleKomplett(false, false); K.SetzeSmSpieldateien(false); dump("KOMPLETT_SM_NUR_KACHEL");
                        K.SetzeSmSpieldateien(true); dump("KOMPLETT_SM_MIT_SPIEL");
                        foreach (var sp in K.Funde.Where(f => f.IstTeil("Spiel")).ToList())
                        {
                            K.SmZuruecksetzen();
                            foreach (var f in K.Funde) f.Wahl = false;
                            sp.Wahl = true; K.AbhaengigeMitnehmen();
                            dump("SPIEL " + sp.Gruppe);
                        }
                    }
                    break;
                case "pruefen":
                    {
                        // NEU (Prüflauf vor Schritt 10): NUR LESEN - Schutzentscheidung je Pfad, Größen je Pfad/Eintrag,
                        // Auswahl der vollständigen Bereinigung ohne/mit M.2. Es werden nur Häkchen im Speicher gesetzt
                        // und Ordnerlisten gelesen; kein Löschen, kein ShadowMount-Stopp, keine Datei.
                        Console.WriteLine("PRUEFLAUF OHNE_SELBSTSCHUTZ_WIE_105");
                        var flags = System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance;
                        var ov = typeof(Kern).GetMethod("OhneVerknuepfung", flags);
                        var pg = typeof(Kern).GetField("PFAD_GROESSE", flags);
                        Func<Groesse, string> gz = g => g.Bytes + " " + g.Dateien + " " + g.Ordner + " " + (g.Gekappt ? "gekappt" : "ganz");
                        var pfade = new List<string>(); var ges = new HashSet<string>(StringComparer.Ordinal);
                        Action<List<string>, HashSet<string>, string> neu = (l, h, p) => { if (!string.IsNullOrEmpty(p) && h.Add(p)) l.Add(p); };
                        foreach (var f in K.Funde) { foreach (var p in f.Pfade) neu(pfade, ges, p); foreach (var p in f.PfadeBasis) neu(pfade, ges, p); neu(pfade, ges, f.Quelle); }
                        foreach (var p in K.AUFGEKLAPPT) neu(pfade, ges, p);
                        foreach (var l in K.LEER_DANACH.Values) foreach (var p in l) neu(pfade, ges, p);
                        Console.WriteLine("== PFADE " + pfade.Count);
                        foreach (var p in pfade)
                        {
                            string v;
                            try { v = ((bool)ov.Invoke(K, new object[] { p })) ? "1" : "0"; }
                            catch (System.Reflection.TargetInvocationException ex) { v = "AUSNAHME " + ex.InnerException.Message; }
                            Console.WriteLine("P " + (K.Erlaubt(p) ? "1" : "0") + " " + v + " " + p);
                        }
                        int nG = 0;
                        K.GroessenImHintergrund(() => false, e => nG++);
                        Console.WriteLine("== GROESSEN gezaehlt=" + nG + " von " + K.Funde.Count);
                        var tab = (Dictionary<string, Groesse>)pg.GetValue(K);
                        var gp = new List<string>(); var gs = new HashSet<string>(StringComparer.Ordinal);
                        foreach (var f in K.Funde) { foreach (var p in f.PfadeBasis) neu(gp, gs, p); if (f.Kat == 1 && f.Quelle != "") neu(gp, gs, f.Quelle); }
                        foreach (var p in gp) { Groesse g; Console.WriteLine(tab.TryGetValue(p, out g) ? "G " + gz(g) + " " + p : "G FEHLT " + p); }
                        foreach (var f in K.Funde) { var g = K.EintragGroesse(f, true); Console.WriteLine("E " + (g == null ? "FEHLT" : gz(g)) + " " + f.Kat + " | " + f.Text); }
                        Action<string, Groesse> wahl = (titel, g) => {
                            Console.WriteLine("== " + titel);
                            foreach (var f in K.Funde) if (f.Wahl) Console.WriteLine("WAHL " + f.Kat + " | " + f.Text + " :: " + string.Join(" | ", f.Pfade) + (g == null ? "" : f.HatGroesse ? " ## " + f.Bytes + " " + f.Dateien + " " + f.Ordner : " ## -"));
                            if (g != null) Console.WriteLine("SUMME " + gz(g) + " | " + Kern.GroesseSatz(g));
                        };
                        K.WaehleKomplett(false, false); wahl("KOMPLETT", K.ErmittleGroessen());
                        K.WaehleKomplett(true, true); wahl("KOMPLETT_M2", K.ErmittleGroessen());
                        K.WaehleKomplett(false, false); K.SetzeSmSpieldateien(false); wahl("KOMPLETT_SM_NUR_KACHEL", K.ErmittleGroessen());
                        K.SetzeSmSpieldateien(true); wahl("KOMPLETT_SM_MIT_SPIEL", K.ErmittleGroessen());
                        foreach (var sp in K.Funde.Where(f => f.IstTeil("Spiel")).ToList())
                        {
                            K.SmZuruecksetzen();
                            foreach (var f in K.Funde) f.Wahl = false;
                            sp.Wahl = true; K.AbhaengigeMitnehmen();
                            wahl("SPIEL " + sp.Gruppe, null);
                        }
                        foreach (var f in K.Funde) f.Wahl = false;
                        K.SmZuruecksetzen();
                        Console.WriteLine("PRUEFLAUF ENDE OHNE_SELBSTSCHUTZ_WIE_105");
                    }
                    break;
                case "liste":
                    foreach (var f in K.Funde) Console.WriteLine(string.Format("FUND [{0}] {1} :: {2}", f.Kat, f.Text, string.Join(" | ", f.Pfade)));
                    break;
                case "astro":
                    Pruefe(K.Erlaubt("/user/home/u1/savedata_prospero/PPSA01326"), "Astro-Spielstand ist löschbar wie jeder andere (1.0.6)");
                    // 1.0.6 (da_Chris86 28.10.2026): Astro's Playroom ist ein normales Spiel
                    Pruefe(K.ASTRO_IDS.Count == 0, "Astro wird nicht mehr gesondert erkannt (1.0.6)");
                    Pruefe(K.Funde.Any(f => System.Text.RegularExpressions.Regex.IsMatch(string.Join(" ", f.Pfade), "PPSA01327")), "Astro auf der M.2 wird wie jedes Spiel angeboten (1.0.6)");
                    Pruefe(K.Erlaubt("/user/home/u1/trophy2/nobackup/data/NPWR20188_00"), "Astro-Trophäenfortschritt ist löschbar wie jeder andere (1.0.6)");
                    break;
                case "lizenz":
                    Pruefe(finde(Sp.T("Fake-Lizenz", "Fake license") + "*PPSA11111*").Count == 0, "Lizenz eines installierten ShadowMount-Spiels nicht angeboten");
                    Pruefe(finde(Sp.T("Fake-Lizenz", "Fake license") + "*SLIB00001*").Count == 1, "Lizenz ohne Spiel angeboten");
                    var m2 = K.Funde.Where(f => f.IstTeil("Spiel") && H.Like(f.Gruppe, "*|CUSA09172")).ToList();
                    Pruefe(m2.Count == 1 && string.Join(" ", m2[0].Pfade).Contains("fakeEP1004-CUSA09172"), "Lizenz des M.2-Spiels hängt am Spiel");
                    Pruefe(finde("*_zu_loeschen_1*")[0].Kat == 5 && finde("*_zu_loeschen_1*")[0].Ort == Kern.ORT_M2, "M.2-Rest einer Aufräumaktion in Bereich 5 mit Ort M.2 (1.0.6)");
                    break;
                case "gruppen":
                    {
                        var g = K.Funde.Where(f => f.Gruppe != null && H.Like(f.Gruppe, "*PPSA77700")).ToList();
                        foreach (var f in g) Console.WriteLine("  " + f.Gruppe + " " + f.Teil + " " + f.Kat + " " + f.Text);
                        var intern = g.First(f => f.Gruppe == "/user|PPSA77700" && f.Teil == "Spiel");
                        intern.Wahl = true; K.AbhaengigeMitnehmen();
                        Pruefe(g.Count(f => f.Gruppe.StartsWith("/mnt/ext1") && f.Wahl) == 0, "internes Spiel gewählt -> M.2-Fassung bleibt abgewählt");
                        Pruefe(g.Count(f => f.Gruppe == "/user|PPSA77700" && f.Wahl) == 4, "internes Spiel nimmt eigenes Update + 2 DLCs mit");
                        string sp = string.Join(" ", intern.Pfade);
                        Pruefe(!System.Text.RegularExpressions.Regex.IsMatch(sp, "/user/appmeta/PPSA77700|/system_data"), "internes Spiel mit M.2-Zwilling lässt geteilte Kachel-Infos stehen (" + sp + ")");
                    }
                    break;
                case "sm":
                    {
                        // NEU 1.0.5: ShadowMount - nur bestätigt angehalten oder nachweislich aus -> ohne Frage löschen
                        string fall = Environment.GetEnvironmentVariable("SMFALL") ?? "ok";
                        if (fall == "aus_mount" || fall == "stumm" || fall == "trotzdem")
                        {
                            Directory.CreateDirectory(Lokal(hier, "/mnt/shadowmnt/PPSA99999"));
                            File.WriteAllText(Lokal(hier, "/mnt/shadowmnt/PPSA99999/eboot.bin"), "x");
                        }
                        bool gefragt = false;
                        foreach (var f in K.Funde) f.Wahl = false;
                        var tool = K.Funde.First(f => f.Kat == 6 && f.Text.Contains("TOOL00001"));
                        tool.Wahl = true;
                        string ziel = tool.Pfade[0];
                        var e = K.FuehreLoeschung(() => { gefragt = true; return fall == "trotzdem"; });
                        bool weg = !Directory.Exists(Lokal(hier, ziel)) && !File.Exists(Lokal(hier, ziel));
                        bool sollFragen = fall == "aus_mount" || fall == "stopfail" || fall == "trotzdem";
                        bool sollWeg = !sollFragen || fall == "trotzdem";
                        Console.WriteLine("ERG Fall=" + fall + " gefragt=" + gefragt + " weg=" + weg + " abgebrochen=" + e.Abgebrochen + " Grund=" + K.SM_GRUND);
                        Pruefe(gefragt == sollFragen, "Fall " + fall + ": Rückfrage " + (sollFragen ? "kommt" : "kommt nicht"));
                        Pruefe(weg == sollWeg, "Fall " + fall + ": " + (sollWeg ? "gelöscht" : "nichts gelöscht"));
                    }
                    break;
                case "kacheln":
                    {
                        // NEU: Kachel-Merker statt Textsuche - muss in beiden Sprachen gleich wirken
                        var deK = K.Funde.Where(f => H.Like(f.Text, "*Kachel*")).ToList();
                        if (!Sp.EN) Pruefe(deK.Count > 0 && deK.All(f => f.Kachel), "alle Einträge mit 'Kachel' im Text sind markiert (" + deK.Count + ")");
                        var dePfade = deK.Select(f => f.PfadeBasis[0]).ToList();
                        Sp.EN = true; K.SucheFunde();
                        var enPfade = K.Funde.Where(f => f.Kachel).Select(f => f.PfadeBasis[0]).ToList();
                        Pruefe(dePfade.All(p => enPfade.Contains(p)), "auf Englisch dieselben Einträge markiert (" + enPfade.Count + ")");
                        var deutsch = K.Funde.Where(f => H.M(f.Text, "Kachel|Spielstand |Trophäen|Zusatzinhalt|ohne Spiel|unbekannter")).Select(f => f.Text).ToList();
                        Pruefe(deutsch.Count == 0, "keine deutschen Wörter in den englischen Einträgen" + (deutsch.Count > 0 ? ": " + deutsch[0] : ""));
                        var log = new List<string>(); K.Melde = t => { log.Add(t); Console.WriteLine("LOG: " + t); };
                        foreach (var f in K.Funde) f.Wahl = false;
                        var tool = K.Funde.First(f => f.Kat == 6 && f.Text.Contains("TOOL00001"));
                        tool.Wahl = true;
                        var e = K.FuehreLoeschung(() => false);
                        Pruefe(log.Any(z => z.Contains("Stopping ShadowMount")), "nur eine Tool-Kachel gewählt (Englisch) -> ShadowMount wird vorher angehalten");
                        Console.WriteLine("ERG Fehler=" + e.Fehler);
                    }
                    break;
                case "k1":
                    {
                        K.WaehleKomplett(false, false);
                        var c = finde("*PPSA33333*ShadowMount*")[0];
                        Pruefe(string.Join(" ", c.Pfade).Contains("Game C"), "KOMPLETT nimmt SM-Spiel von der internen SSD mit");
                        foreach (var f in K.Funde) f.Wahl = false;
                        c.Wahl = true;
                        K.SetzeSmSpieldateien(false);
                        Pruefe(!string.Join(" ", c.Pfade).Contains("Game C"), "danach \"nur Kacheln\": Spieldatei NICHT mehr in der Liste");
                        Pruefe(!c.Text.Contains("Spiel + Kachel") && c.Text.Split('-').Length < 6, "Text sauber: " + c.Text);
                        K.SetzeSmSpieldateien(true); K.SetzeSmSpieldateien(true);
                        Pruefe(c.Pfade.Count(p => p.Contains("Game C")) == 1, "zweimal \"mit Spiel\": Spieldatei genau einmal");
                        K.SetzeSmSpieldateien(false);
                        var e = K.FuehreLoeschung(() => false);
                        Console.WriteLine("ERG Fehler=" + e.Fehler);
                    }
                    break;
                case "symlink":
                    {
                        Pruefe(!K.Funde.Any(f => f.Pfade.Any(p => p.StartsWith("/user/data/hb/"))), "nichts hinter der Verknüpfung angeboten");
                        K.WaehleKomplett(false, false);
                        var e = K.FuehreLoeschung(() => false);
                        Console.WriteLine("ERG Fehler=" + e.Fehler);
                    }
                    break;
                case "komplett":
                    {
                        K.WaehleKomplett(false, false);
                        // NEU 1.0.5: Fortschritt - jeder gewählte Eintrag genau einmal, durchgezählt bis n
                        var meldungen = new List<int>(); int nGesamt = -1;
                        K.Fortschritt = (i, n, f) => { meldungen.Add(i); nGesamt = n; };
                        var g = K.ErmittleGroessen();
                        Console.WriteLine("GROESSE " + Kern.GroesseSatz(g));
                        var e = K.FuehreLoeschung(() => false);
                        Console.WriteLine("ERG Fehler=" + e.Fehler + " Abgebrochen=" + e.Abgebrochen + " " + Kern.FreigeraeumtText(e.Frei));
                        Pruefe(nGesamt > 0 && meldungen.Count == nGesamt && meldungen.SequenceEqual(Enumerable.Range(1, nGesamt)), "Fortschritt meldet jeden Eintrag einmal: " + meldungen.Count + " von " + nGesamt);
                    }
                    break;
                case "komplettm2":
                    {
                        K.WaehleKomplett(true, true);
                        var e = K.FuehreLoeschung(() => false);
                        Console.WriteLine("ERG Fehler=" + e.Fehler + " Abgebrochen=" + e.Abgebrochen);
                    }
                    break;
                case "m2einzeln":
                    {
                        // NEU: M.2-Ordner aufgeklappt - eine einzelne Datei löschen, Rest + Ordner bleiben
                        var m2e = K.Funde.Where(f => f.Ort == Kern.ORT_M2 && f.Quelle == "").ToList();   // 1.0.6: statt Bereich 7 der Ort
                        foreach (var f in m2e) Console.WriteLine("  M2 " + f.Text + " :: " + string.Join(" | ", f.Pfade));
                        var einer = m2e.FirstOrDefault(f => f.Pfade.Count == 1 && f.Pfade[0].Split('/').Length == 5 && f.Quelle == "" && !f.Pfade[0].Contains("_zu_loeschen_"));
                        Pruefe(einer != null, "Einträge innerhalb der M.2-Ordner einzeln vorhanden");
                        Pruefe(!m2e.Any(f => f.Pfade[0].StartsWith("/mnt/ext1/") && f.Pfade[0].Split('/').Length == 4 && !f.NurLeer && !f.Pfade[0].Contains("_zu_loeschen_") && IstOrdnerMitInhalt(hier, f.Pfade[0])), "kein voller M.2-Ordner als Ganzes angeboten");
                        if (einer != null)
                        {
                            einer.Wahl = true;
                            var e = K.FuehreLoeschung(() => false);
                            string ordner = H.Eltern(einer.Pfade[0]);
                            Console.WriteLine("ERG Fehler=" + e.Fehler + " geloescht=" + einer.Pfade[0]);
                            Pruefe(e.Fehler == 0 && !File.Exists(Lokal(hier, einer.Pfade[0])) && !Directory.Exists(Lokal(hier, einer.Pfade[0])), "gewählter Eintrag gelöscht");
                            Pruefe(Directory.Exists(Lokal(hier, ordner)), "M.2-Ordner selbst bleibt stehen");
                        }
                    }
                    break;
                case "leerordner":
                    {
                        // Ein als "leer" angebotener M.2-Ordner wird nur per RMD entfernt -
                        // ist inzwischen etwas drin, bleibt alles stehen.
                        K.Funde.First(f => f.Text.StartsWith("M.2 – games/x.iso")).Wahl = true;   // 1.0.6: Ort vorne
                        K.FuehreLoeschung(() => false);
                        K.SucheFunde();
                        var leer = K.Funde.FirstOrDefault(f => f.NurLeer && f.Pfade[0] == "/mnt/ext1/games");
                        Pruefe(leer != null && leer.NurLeer, "leerer Ordner games wird als 'nur leer' angeboten");
                        File.WriteAllText(Lokal(hier, "/mnt/ext1/games/neu.iso"), "spiel");
                        leer.Wahl = true;
                        var e1 = K.FuehreLoeschung(() => false);
                        Pruefe(e1.Fehler == 1 && File.Exists(Lokal(hier, "/mnt/ext1/games/neu.iso")), "inzwischen gefüllt: nichts gelöscht, Datei bleibt (Fehler=" + e1.Fehler + ")");
                        File.Delete(Lokal(hier, "/mnt/ext1/games/neu.iso"));
                        K.SucheFunde();
                        leer = K.Funde.First(f => f.NurLeer && f.Pfade[0] == "/mnt/ext1/games"); leer.Wahl = true;
                        var e2 = K.FuehreLoeschung(() => false);
                        Pruefe(e2.Fehler == 0 && !Directory.Exists(Lokal(hier, "/mnt/ext1/games")), "wirklich leer: Ordner entfernt");
                    }
                    break;
                case "groessen":
                    {
                        // NEU: Hintergrund-Zählung über eine zweite Verbindung
                        int n = 0;
                        K.GroessenImHintergrund(() => false, e => n++);
                        Pruefe(n == K.Funde.Count, "Hintergrund-Zählung hat alle " + K.Funde.Count + " Einträge gezählt (" + n + ")");
                        Pruefe(K.Funde.All(f => K.EintragGroesse(f, true) != null), "jeder Eintrag hat eine Größe");
                        K.WaehleKomplett(true, true);
                        var g = K.ErmittleGroessen();
                        Console.WriteLine("GROESSE " + Kern.GroesseSatz(g));
                        Pruefe(g.Bytes > 0 && !g.Gekappt, "Summe vor dem Löschen aus den gemerkten Größen");
                        Pruefe(K.S.Gesund(), "Hauptverbindung danach in Ordnung");
                    }
                    break;
                case "abbruch":
                    {
                        // NEU: Abbrechen-Knopf - Suche bricht an sicherer Stelle ab, Verbindung bleibt im Takt
                        K.Abbruch = true;
                        bool abg = false;
                        try { K.SucheFunde(); } catch (AbbruchAusnahme) { abg = true; }
                        Pruefe(abg && K.FUNDE_IP == "", "Suche abgebrochen, keine Liste zum Löschen");
                        K.Abbruch = false;
                        Pruefe(K.S.Gesund(), "Verbindung nach dem Abbruch weiter sauber");
                        var e = K.FuehreLoeschung(() => false);
                        Pruefe(e.Abgebrochen, "Löschen nach abgebrochener Suche verweigert");
                    }
                    break;
                case "listefehler":
                    {
                        var d = finde("*Season Pass*")[0]; d.Wahl = true;
                        File.WriteAllText(Path.Combine(hier, "faillist.on"), "x");
                        var e = K.FuehreLoeschung(() => false);
                        Console.WriteLine("ERG Fehler=" + e.Fehler);
                        File.Delete(Path.Combine(hier, "faillist.on"));
                        Pruefe(e.Fehler == 1, "fehlgeschlagene Ordnerliste = Fehler, nicht \"schon weg\"");
                    }
                    break;
                case "spielfehler":
                    {
                        var s = K.Funde.First(f => f.Gruppe == "/user|PPSA77700" && f.Teil == "Spiel"); s.Wahl = true;
                        var e = K.FuehreLoeschung(() => false);
                        Console.WriteLine("ERG Fehler=" + e.Fehler);
                        Pruefe(e.Fehler == 4, "Spiel-Fehler: Spiel + Update + 2 DLC nicht gelöscht");
                    }
                    break;
                case "abriss":
                    {
                        foreach (var f in K.Funde) if (f.Kat == 5) f.Wahl = true;
                        var t = DateTime.Now;
                        var e = K.FuehreLoeschung(() => false);
                        double sek = (DateTime.Now - t).TotalSeconds;
                        Console.WriteLine("ERG Fehler=" + e.Fehler + " Verloren=" + e.Verloren + " Sekunden=" + (int)sek);
                        Pruefe(e.Verloren && sek < 15, "Abriss erkannt und sofort beendet");
                        Pruefe(K.S.Ftp.Timeout == 30000, "Zeitlimit danach wieder normal");
                    }
                    break;
                case "speicher":
                    {
                        // NEU 1.0.5: USB-/M.2-Hinweis - nur Text, die Liste muss gleich bleiben
                        Func<string> liste = () => string.Join("\n", K.Funde.Select(f => f.Kat + " " + string.Join("|", f.PfadeBasis)).OrderBy(x => x, StringComparer.Ordinal));
                        foreach (var en in new[] { false, true })
                        {
                            Sp.EN = en; string sp = en ? "EN" : "DE";
                            K.SucheFunde();
                            string basis = liste();
                            Pruefe(K.m2Da && !K.M2_OHNE_USER && !K.USB_SPEICHER && K.SpeicherHinweis() == "", sp + ": normale PS5 -> kein Hinweis");
                            Directory.CreateDirectory(Lokal(hier, "/mnt/ext0/user/app/CUSA00001"));
                            K.SucheFunde();
                            string h = K.SpeicherHinweis();
                            Console.WriteLine("  " + sp + " USB: " + h.Trim());
                            Pruefe(K.USB_SPEICHER && h.Contains(en ? "USB drive" : "USB-Platte"), sp + ": USB-Platte als erweiterter Speicher -> Hinweis");
                            // 1.0.6 (da_Chris86 28.10.2026: "USB wie M.2"): das USB-Spiel steht in Bereich 1, sonst bleibt die Liste gleich
                            Func<string, string> ohneUsb = x => string.Join("\n", x.Split('\n').Where(z => !z.Contains("CUSA00001")));
                            string mitUsb = liste();
                            Pruefe(ohneUsb(mitUsb) == ohneUsb(basis) && mitUsb.Split('\n').Any(z => z.StartsWith("1 /mnt/ext0/user/app/CUSA00001", StringComparison.Ordinal)), sp + ": mit USB-Speicher: USB-Spiel in Bereich 1, sonst dieselbe Liste (1.0.6)");
                            Directory.Move(Lokal(hier, "/mnt/ext0"), Lokal(hier, "/mnt/ext0_weg"));
                            K.SucheFunde();
                            Pruefe(!K.USB_SPEICHER && K.SpeicherHinweis() == "", sp + ": USB-Platte weg -> Hinweis weg");
                            Directory.Move(Lokal(hier, "/mnt/ext1/user"), Lokal(hier, "/mnt/user_weg"));
                            K.SucheFunde();
                            h = K.SpeicherHinweis();
                            Console.WriteLine("  " + sp + " M2 ohne user: " + h.Trim());
                            Pruefe(K.m2Da && K.M2_OHNE_USER && h.Contains("M.2"), sp + ": M.2 ohne user-Ordner -> Hinweis");
                            Directory.Move(Lokal(hier, "/mnt/ext1"), Lokal(hier, "/mnt/ext1_weg"));
                            K.SucheFunde();
                            h = K.SpeicherHinweis();
                            Console.WriteLine("  " + sp + " ohne M2: " + h.Trim());
                            Pruefe(!K.m2Da && h.Contains(en ? "No M.2" : "Keine M.2"), sp + ": keine M.2 -> bisheriger Hinweis");
                            Directory.Move(Lokal(hier, "/mnt/ext1_weg"), Lokal(hier, "/mnt/ext1"));
                            Directory.Move(Lokal(hier, "/mnt/user_weg"), Lokal(hier, "/mnt/ext1/user"));
                            Directory.Move(Lokal(hier, "/mnt/ext0_weg"), Lokal(hier, "/mnt/ext0"));
                            Directory.Delete(Lokal(hier, "/mnt/ext0"), true);
                            K.SucheFunde();
                            Pruefe(liste() == basis && K.SpeicherHinweis() == "", sp + ": alles zurück -> gleiche Liste, kein Hinweis");
                            if (en) Pruefe(!System.Text.RegularExpressions.Regex.IsMatch(K.SpeicherHinweis() + h, "[äöüÄÖÜß]|HINWEIS|Kachel"), "EN: keine deutschen Wörter im Hinweis");
                        }
                    }
                    break;
                case "andereps5":
                    {
                        foreach (var f in K.Funde) if (f.Kat == 5) f.Wahl = true;
                        K.Verbinden("127.0.0.1", port + 1);
                        var e = K.FuehreLoeschung(() => false);
                        Pruefe(e.Abgebrochen, "Liste einer anderen PS5 -> abgebrochen");
                    }
                    break;
            }
            K.Trennen();
            Console.WriteLine("FEHLERZAHL " + fehler);
            return 0;
        }
        // NEU 1.0.5: Laufwerks-Anzeige für alle Modelle, alle Statuswerte und ungültige Antworten
        static int Laufwerk()
        {
            var modelle = new[] { "CFI-1016A", "CFI-1216B", "CFI-2016", "CFI-2116B", "CFI-7021 B01Y", "CFI-7016", "" };
            var werte = new int?[] { null, -1, 0, 1, 2, 3, 4, 5, 6, 7, 99, -5 };
            foreach (var en in new[] { false, true })
            {
                Sp.EN = en; string sp = en ? "EN" : "DE";
                foreach (var m in modelle)
                    foreach (var w in werte)
                    {
                        var K = new Kern();
                        K.MODELL_TYP = Kern.ModellTyp(m); K.MODELL_DIGITAL = Kern.ModellDigital(m); K.DDD_STATUS = w;
                        string t = K.LaufwerkText();
                        string was = sp + " " + (m == "" ? "Modell unbekannt" : m) + " Status " + (w.HasValue ? w.Value.ToString() : "nicht abgefragt") + ": " + t;
                        string soll;
                        if (K.MODELL_TYP == "fat") soll = K.MODELL_DIGITAL ? (en ? "No pairing: Digital Edition" : "Keine Kopplung: Digital Edition") : (en ? "No pairing needed: built-in" : "Keine Kopplung nötig: fest eingebautes");
                        // 1.0.6 (da_Chris86 28.10.2026): kurze Texte ohne Statuswert
                        else if (w == null) soll = en ? "Not queried: could not be read." : "Nicht abgefragt: Konnte nicht gelesen werden.";
                        else if (w == 1) soll = en ? "Pairing confirmed" : "Kopplung bestätigt";
                        else if (w == 2) soll = en ? "No pairing: no disc drive" : "Keine Kopplung: Kein Laufwerk";
                        else if (w >= 0 && w <= 6) soll = en ? "Not reliably determinable" : "Nicht sicher bestimmbar";
                        else soll = en ? "Not queried: could not be read." : "Nicht abgefragt: Konnte nicht gelesen werden.";
                        Pruefe(t.StartsWith(soll, StringComparison.Ordinal), was);
                        var st = K.KopplungsStufe();
                        var stSoll = K.MODELL_TYP == "fat" || w == 2 ? Kern.KopplungsArt.Keine : w == 1 ? Kern.KopplungsArt.Bestaetigt : (w.HasValue && w >= 0 && w <= 6) ? Kern.KopplungsArt.Unklar : Kern.KopplungsArt.NichtAuslesbar;
                        Pruefe(st == stSoll, "  Farbe/Stufe passt zum Text: " + st);
                        Pruefe(!System.Text.RegularExpressions.Regex.IsMatch(t, "ANDEREN|anderen Konsole|ANOTHER|another console"), "  keine unbelegte Aussage 'andere Konsole'");
                        if (en) Pruefe(!System.Text.RegularExpressions.Regex.IsMatch(t, "[äöüÄÖÜß]|Kopplung|Laufwerk"), "  keine deutschen Wörter");
                        Pruefe(K.KopplungsText().StartsWith(t, StringComparison.Ordinal), "  KopplungsText baut darauf auf");
                    }
            }
            // Antwort des PS5-Programms einlesen (wie über Port 9021 / Datei)
            Sp.EN = false;
            var KE = new Kern();
            var li = typeof(Kern).GetMethod("LiesInfo", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
            string mo = (string)li.Invoke(KE, new object[] { "PS5INFO_MODEL=CFI-7021 B01Y\nPS5INFO_DDD=1\n" });
            Pruefe(mo == "CFI-7021 B01Y" && KE.DDD_STATUS == 1, "Antwort 'DDD=1' gelesen");
            KE.DDD_STATUS = null; li.Invoke(KE, new object[] { "PS5INFO_MODEL=\nPS5INFO_DDD=-1\n" });
            Pruefe(KE.DDD_STATUS == -1 && KE.LaufwerkText().StartsWith("Nicht abgefragt"), "Antwort 'DDD=-1' (ungültig) -> nicht lesbar");
            KE.DDD_STATUS = null; li.Invoke(KE, new object[] { "Murks ohne Wert" });
            Pruefe(KE.DDD_STATUS == null && KE.LaufwerkText().StartsWith("Nicht abgefragt"), "keine Antwort -> nicht abgefragt");
            Console.WriteLine("FEHLERZAHL " + fehler);
            return 0;
        }

        // NEU 1.0.5: freiwilliger Feedback-Bericht - nur Text, nichts wird gesendet
        static int FeedbackTest(string hier)
        {
            var namen = new[] { "da_Chris86", "WORKSTATION-PC" };
            foreach (var en in new[] { false, true })
            {
                Sp.EN = en; string sp = en ? "EN" : "DE"; string unb = en ? "Unknown" : "Unbekannt";
                // 1 nichts erkannt -> überall "Unbekannt"
                var leer = new FeedbackDaten();
                string b0 = Feedback.Bericht(leer);
                Console.WriteLine("---- " + sp + " leerer Bericht:\n" + b0);
                Pruefe(System.Text.RegularExpressions.Regex.Matches(b0, ":\\*\\* " + unb).Count >= 7, sp + ": nicht Erkanntes heißt '" + unb + "'");
                Pruefe(b0.Contains(en ? "FTP server:** " + unb : "FTP-Server:** " + unb), sp + ": FTP-Server " + unb + " (Variante B)");
                Pruefe(b0.Contains(en ? "Scan:** not done" : "Scan:** nicht durchgeführt"), sp + ": ohne Scan als 'nicht durchgeführt' markiert");
                Pruefe(b0.Contains(en ? "none (not queried" : "keiner (nicht abgefragt"), sp + ": ohne Laufwerksabfrage kein Rückgabewert");
                // 2 persönliche Daten werden entfernt, technische Angaben bleiben
                string roh = "FEHLER: 192.168.178.78:2121 antwortet nicht; Datei C:\\Users\\da_Chris86\\Desktop\\PS5-Cleanup.log; NAS \\\\NAS\\Filme\\x; /user/home/1a2b3c4d/savedata_prospero/PPSA01325; mail max@example.com; PC WORKSTATION-PC; Version 1.0.5 Beta, Firmware 12.00, CFI-7021 B01Y";
                string sauber = Feedback.Bereinigen(roh, namen);
                Console.WriteLine("  " + sp + " bereinigt: " + sauber);
                Pruefe(!sauber.Contains("192.168") && !sauber.Contains("da_Chris86") && !sauber.Contains("Users") && !sauber.Contains("NAS") && !sauber.Contains("1a2b3c4d") && !sauber.Contains("max@example.com") && !sauber.Contains("WORKSTATION"), sp + ": IP, Pfade, Benutzer, PC-Name, E-Mail, PS5-Benutzerkennung entfernt");
                Pruefe(sauber.Contains("1.0.5 Beta") && sauber.Contains("12.00") && sauber.Contains("CFI-7021 B01Y") && sauber.Contains("PPSA01325"), sp + ": Version, Firmware, Modell, Spielkennung bleiben");
                // 3 voller Bericht
                var d = new FeedbackDaten { Modell = "CFI-2016", Reihe = "PS5 Slim", ModellQuelle = "automatisch (ps5info)", Firmware = "11.00", DddRoh = 6, LaufwerkText = "Kopplung nicht sicher bestimmbar: Statuswert 6", Port = 2121, ScanGemacht = true, ScanImTestmodus = true, M2 = "erkannt", Usb = "nicht gefunden", ShadowMountSpiele = 2, Windows = "Windows 11 (Build 22631)", PersoenlicheNamen = namen };
                for (int k = 1; k <= 7; k++) d.AnzahlJeBereich[k] = k;
                for (int i = 0; i < 14; i++) d.Fehler.Add("FEHLER " + i + " bei C:\\Users\\da_Chris86\\x.log von 192.168.178.78");
                string b1 = Feedback.Bericht(d);
                Console.WriteLine("---- " + sp + " voller Bericht:\n" + b1);
                Pruefe(b1.Contains("CFI-2016 (PS5 Slim)") && b1.Contains("11.00") && b1.Contains(":** 6\n") && b1.Contains("2121") && b1.Contains("28 "), sp + ": Modell, Firmware, Rückgabewert, Port, Summe der Einträge");
                Pruefe(b1.Contains(": 7\n\n**M.2-SSD:**"), sp + ": Leerzeile nach der Bereichsliste (GitHub-Darstellung)");
                Pruefe(b1.Contains("FEHLER 9 ") && !b1.Contains("FEHLER 10 ") && b1.Contains(en ? "4 more" : "4 weitere"), sp + ": höchstens 10 Fehlerzeilen, Rest gezählt");
                Pruefe(!b1.Contains("192.168") && !b1.Contains("da_Chris86"), sp + ": auch im vollen Bericht keine IP und kein Name");
                if (en) Pruefe(!System.Text.RegularExpressions.Regex.IsMatch(b1.Replace("FEHLER", "").Replace("Kopplung nicht sicher bestimmbar: Statuswert", "").Replace("automatisch", "").Replace("erkannt", "").Replace("nicht gefunden", ""), "Unbekannt|Rückgabewert|Einträge|Spiele|nicht durchgeführt|Fehlermeldungen|gelöscht"), "EN: Bericht ohne deutsche Wörter");
                // 4 Link
                bool gek;
                string titel = Feedback.Titel(d);
                string l1 = Feedback.IssueLink(titel, b1, out gek);
                Pruefe(!gek && l1.StartsWith(Feedback.ISSUE_NEU + "?title=") && l1.Length <= Feedback.MAX_LINK && l1.Contains(Uri.EscapeDataString("PS5 Slim CFI-2016")), sp + ": Link vorausgefüllt (" + l1.Length + " Zeichen): " + titel);
                string l2 = Feedback.IssueLink(titel, new string('x', 20000), out gek);
                Pruefe(gek && l2.Length <= Feedback.MAX_LINK, sp + ": zu langer Bericht -> kurzer Link, Text in die Zwischenablage");
                Pruefe(Feedback.FehlerAus("Verbunden.\nFEHLER: x\nSICHERHEITSSTOPP: y\nERROR: z\nalles gut").Count == 3, sp + ": Fehlerzeilen aus dem Protokoll erkannt");
            }
            // 5 im Quelltext: keine Netzwerk-Funktion im Bericht, Browser nur im Klick auf "Auf GitHub öffnen", nichts beim Start
            string q = Path.Combine(hier, "cs");
            string fb = File.ReadAllText(Path.Combine(q, "Feedback.cs"));
            Pruefe(!System.Text.RegularExpressions.Regex.IsMatch(fb, "WebRequest|WebClient|HttpClient|TcpClient|Socket|Process\\.Start"), "Feedback.cs: kein Netzwerk, kein Browser-Start");
            string fe = File.ReadAllText(Path.Combine(q, "Fenster.cs"));
            int a = fe.IndexOf("public Window FeedbackFenster", StringComparison.Ordinal), e = fe.IndexOf("void FeedbackOeffnen", StringComparison.Ordinal);
            string teil = fe.Substring(a, e - a);
            int ps = teil.IndexOf("Process.Start", StringComparison.Ordinal);
            Pruefe(System.Text.RegularExpressions.Regex.Matches(teil, "Process\\.Start").Count == 1 && ps > teil.IndexOf("bg.Click +=", StringComparison.Ordinal), "Fenster.cs: Browser nur nach Klick auf 'Auf GitHub öffnen'");
            Pruefe(!System.Text.RegularExpressions.Regex.IsMatch(teil, "WebRequest|WebClient|HttpClient|TcpClient"), "Fenster.cs: Feedback-Fenster ohne Netzwerk-Aufruf");
            string pr = File.ReadAllText(Path.Combine(q, "Programm.cs"));
            Pruefe(!pr.Contains("Feedback"), "Programm.cs: beim Start wird kein Feedback gebaut oder gesendet");
            Console.WriteLine("FEHLERZAHL " + fehler);
            return 0;
        }

        // NEU 1.0.5: steckt in der exe genau die neu gebaute ps5info.elf?
        static int ElfCheck(string exe, string elf)
        {
            var asm = System.Reflection.Assembly.LoadFile(Path.GetFullPath(exe));
            byte[] drin;
            using (var st = asm.GetManifestResourceStream("PS5Cleanup.ps5info.elf"))
            using (var ms = new MemoryStream()) { st.CopyTo(ms); drin = ms.ToArray(); }
            byte[] datei = File.ReadAllBytes(elf);
            var sha = System.Security.Cryptography.SHA256.Create();
            string a = BitConverter.ToString(sha.ComputeHash(drin)).Replace("-", "").ToLowerInvariant();
            string b = BitConverter.ToString(sha.ComputeHash(datei)).Replace("-", "").ToLowerInvariant();
            Console.WriteLine("ELF in der exe:   " + a + " (" + drin.Length + " Bytes)");
            Console.WriteLine("ELF im Quellcode: " + b + " (" + datei.Length + " Bytes)");
            Pruefe(a == b, Path.GetFileName(exe) + " enthält die neu gebaute ps5info.elf");
            Console.WriteLine("FEHLERZAHL " + fehler);
            return 0;
        }

        static string Zeile(Eintrag f)
        {
            return f.Kat + " | " + f.Text + " | basis=" + f.TextBasis + " | gruppe=" + (f.Gruppe ?? "-") + " | teil=" + (f.Teil ?? "-")
                + " | gesperrt=" + f.Gesperrt + " | paket=" + f.Paket + " | nurleer=" + f.NurLeer + " | kachel=" + f.Kachel + " | smtipp=" + f.SmTipp
                + " | quelle=" + f.Quelle + " :: " + string.Join(" | ", f.Pfade) + " || " + string.Join(" | ", f.PfadeBasis);
        }
        static string Lokal(string hier, string p) { return Path.Combine(hier, "w") + p.Replace('/', Path.DirectorySeparatorChar); }
        static bool IstOrdnerMitInhalt(string hier, string p) { string l = Lokal(hier, p); return Directory.Exists(l) && Directory.EnumerateFileSystemEntries(l).Any(); }
    }
}
