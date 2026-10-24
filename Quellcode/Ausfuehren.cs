// Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
// =====================================================================
// Ausfuehren.cs - Auswahl-Helfer, Größen, Löschen ausführen.
// 1:1 nach ps5_kern.ps1, neu in 1.0.4: Größen werden je Pfad gemerkt und
// gleich nach dem Nachschauen über eine zweite Verbindung im Hintergrund
// gezählt - die Liste zeigt sie an, das Zählen vor dem Löschen entfällt.
// =====================================================================
using System;
using System.Collections.Generic;
using System.Linq;

namespace PS5Cleanup
{
    public class Ergebnis
    {
        public bool Abgebrochen; public int Fehler; public bool SmGestoppt; public Groesse Frei = new Groesse();
        public string SmText = ""; public bool Verloren;
    }

    public partial class Kern
    {
        public int Anzahl(int k) { return Funde.Count(f => f.Kat == k); }
        public int AnzahlKomplett(int k) { return Funde.Count(f => f.Kat == k && !f.Gesperrt && !f.Paket); }
        public List<Eintrag> Gewaehlt() { return Funde.Where(f => f.Wahl).ToList(); }
        public bool HatPakete() { return Funde.Any(f => f.Paket); }

        // ShadowMount-Spiele (Bereich 1) auf "wie gefunden" zurücksetzen
        public void SmZuruecksetzen()
        {
            foreach (var f in Funde)
                if (f.Kat == 1 && f.Quelle != "") { f.Pfade = new List<string>(f.PfadeBasis); f.Text = f.TextBasis; }
        }
        // KOMPLETT: alles auf der internen SSD (Bereich 1-6), M.2 nur auf Wunsch.
        public void WaehleKomplett(bool m2Leeren)
        {
            SmZuruecksetzen();
            foreach (var f in Funde) f.Wahl = false;
            foreach (var f in Funde) if (f.Kat <= 6 && !f.Gesperrt && !f.Paket) f.Wahl = true;
            if (m2Leeren) foreach (var f in Funde) if ((f.Kat == 7 || f.Paket) && !f.Gesperrt) f.Wahl = true;
            // ShadowMount-Spiele: liegt das Spiel auf der internen SSD, geht es mit;
            // liegt es auf der M.2, nur wenn die M.2 geleert wird (dann über Bereich 7).
            foreach (var f in Funde)
            {
                if (f.Kat != 1 || f.Quelle == "" || f.Gesperrt) continue;
                if (H.Like(f.Quelle, "/user/data/*")) { f.Pfade = new[] { f.Quelle }.Concat(f.PfadeBasis).ToList(); f.Text = f.TextBasis + SM_MIT; }
                else if (m2Leeren) f.Text = f.TextBasis + T(" - Spiel + Kachel (über Bereich 7)", " - game + tile (via section 7)");
                else f.Text = f.TextBasis + SM_OHNE;
            }
        }
        public static string SM_MIT { get { return T(" - Spiel + Kachel", " - game + tile"); } }
        public static string SM_OHNE { get { return T(" - nur Kachel, Spiel bleibt", " - tile only, game stays"); } }
        public List<Eintrag> SmGewaehlt() { return Funde.Where(f => f.Wahl && f.Kat == 1 && f.Quelle != "").ToList(); }
        public void SetzeSmSpieldateien(bool mit)
        {
            SmZuruecksetzen();
            foreach (var f in SmGewaehlt())
            {
                if (mit) { f.Pfade = new[] { f.Quelle }.Concat(f.PfadeBasis).ToList(); f.Text = f.TextBasis + SM_MIT; }
                else f.Text = f.TextBasis + SM_OHNE;
            }
        }
        public List<Eintrag> M2Gewaehlt() { return Funde.Where(f => f.Wahl && f.Kat == 7).ToList(); }

        // -------------------------------------------------------------
        // Größen
        // -------------------------------------------------------------
        readonly object groessenSperre = new object();
        Dictionary<string, Groesse> PFAD_GROESSE = new Dictionary<string, Groesse>(StringComparer.Ordinal);
        public Action<long, int> GroesseStand = delegate { };   // Zwischenstand beim Zählen vor dem Löschen
        void GroessenZuruecksetzen() { lock (groessenSperre) PFAD_GROESSE = new Dictionary<string, Groesse>(StringComparer.Ordinal); }

        public bool PfadGezaehlt(string p) { lock (groessenSperre) return PFAD_GROESSE.ContainsKey(p); }
        public void GroesseSetzen(string p, Groesse g) { lock (groessenSperre) PFAD_GROESSE[p] = g; }   // für den Selbsttest

        // Größe eines Pfads (Datei oder Ordner samt Inhalt). Gekappt = nicht ganz
        // durchgezählt (Zeitlimit, sehr viele Dateien oder Lesefehler).
        static Groesse BaumGroesse(Sitzung s, string p, DateTime deadline, Action<long, int> lauf)
        {
            var erg = new Groesse();
            List<Datei> l;
            // streng: nur "gibt es nicht" (550) ist null, jeder Lesefehler zählt als Fehler
            try { l = s.Liste(H.Eltern(p), true); }
            catch (AbbruchAusnahme) { throw; }
            catch (System.IO.IOException) { erg.Gekappt = true; erg.Lesefehler = true; return erg; }   // Verbindung evtl. aus dem Takt
            catch { erg.Gekappt = true; return erg; }                                                // saubere Fehlerantwort der PS5
            if (l == null) return erg;
            var e = l.FirstOrDefault(x => x.Name == H.Blatt(p));
            if (e == null) return erg;
            if (!e.IstOrdner) { erg.Bytes = e.Groesse; erg.Dateien = 1; if (lauf != null) lauf(e.Groesse, 1); return erg; }
            erg.Ordner = 1;
            var stapel = new Stack<string>(); stapel.Push(p);
            while (stapel.Count > 0)
            {
                if (DateTime.Now > deadline || erg.Dateien + erg.Ordner > 200000) { erg.Gekappt = true; break; }
                string d = stapel.Pop();
                List<Datei> k;
                try { k = s.Liste(d, true); }
                catch (AbbruchAusnahme) { throw; }
                catch (System.IO.IOException) { erg.Gekappt = true; erg.Lesefehler = true; break; }
                catch { erg.Gekappt = true; continue; }        // dieser Ordner nicht lesbar: Rest weiterzählen
                if (k == null) { erg.Gekappt = true; continue; }   // im Ordner darüber gelistet, aber nicht lesbar
                long b = 0; int n = 0;
                foreach (var x in k)
                {
                    if (x.IstOrdner) { erg.Ordner++; stapel.Push(d + "/" + x.Name); }
                    else if (!x.IstLink) { erg.Dateien++; erg.Bytes += x.Groesse; b += x.Groesse; n++; }
                    else { erg.Dateien++; n++; }
                }
                if (lauf != null) lauf(b, n);
            }
            return erg;
        }

        // Größe eines Eintrags aus den gemerkten Pfad-Größen (null = noch nicht alles gezählt)
        public Groesse EintragGroesse(Eintrag f, bool wieGefunden)
        {
            var g = new Groesse();
            lock (groessenSperre)
            {
                foreach (var p in (wieGefunden ? f.PfadeBasis : f.Pfade).Distinct())
                {
                    Groesse x;
                    if (!PFAD_GROESSE.TryGetValue(p, out x)) return null;
                    g.Plus(x);
                }
            }
            return g;
        }

        // Im Hintergrund über eine eigene Verbindung alle Einträge durchzählen.
        // Läuft, bis alles gezählt ist oder stopp() true liefert.
        public void GroessenImHintergrund(Func<bool> stopp, Action<Eintrag> eintragFertig, Action<Ftp> verbunden = null)
        {
            Ftp f = null;
            try
            {
                f = Ftp.Verbinden(S.Ftp.Ip, S.Ftp.Port);
                if (f == null) return;
                if (verbunden != null) verbunden(f);
                f.Timeout = 15000;
                var s = new Sitzung(f);
                s.Abbruch = stopp;
                var liste = Funde.ToList();
                foreach (var e in liste)
                {
                    if (stopp()) return;
                    var pfade = new List<string>(e.PfadeBasis);
                    if (e.Kat == 1 && e.Quelle != "") pfade.Add(e.Quelle);
                    foreach (var p in pfade.Distinct())
                    {
                        if (PfadGezaehlt(p)) continue;
                        var g = BaumGroesse(s, p, DateTime.Now.AddSeconds(120), null);
                        // Lesefehler: Verbindung evtl. aus dem Takt - nichts merken, aufhören
                        if (g.Lesefehler) return;
                        lock (groessenSperre) { if (!PFAD_GROESSE.ContainsKey(p)) PFAD_GROESSE[p] = g; }
                    }
                    eintragFertig(e);
                }
            }
            catch { }
            finally { if (f != null) { if (stopp()) f.Schliessen(); else f.Trennen(); } }
        }

        public bool GROESSE_GEKAPPT;
        // Größe aller gewählten Einträge (vor dem Löschen). Was die Hintergrund-
        // Zählung schon kennt, wird nicht nochmal gezählt.
        public Groesse ErmittleGroessen()
        {
            AbhaengigeMitnehmen();
            var sum = new Groesse();
            var gesehen = new HashSet<string>(StringComparer.Ordinal);
            DateTime deadline = DateTime.Now.AddSeconds(60);
            long laufB = 0; int laufN = 0; DateTime zuletzt = DateTime.MinValue;
            Action<long, int> lauf = (b, n) =>
            {
                laufB += b; laufN += n;
                if ((DateTime.Now - zuletzt).TotalMilliseconds > 400) { zuletzt = DateTime.Now; GroesseStand(laufB, laufN); }
            };
            int altTimeout = S.Ftp.Timeout;
            bool gekappt = false, verbindungUnsicher = false;
            // Die Verbindung kann stillgelegen haben - vor dem Zählen auffrischen.
            try { if (!S.Gesund()) VerbindungErneuern(); } catch (AbbruchAusnahme) { throw; } catch { }
            S.Ftp.Timeout = 4000;
            try
            {
                foreach (var f in Gewaehlt())
                {
                    var g = new Groesse();
                    foreach (var p in f.Pfade)
                    {
                        if (!gesehen.Add(p)) continue;
                        Groesse x;
                        bool da;
                        lock (groessenSperre) da = PFAD_GROESSE.TryGetValue(p, out x);
                        if (da && x.Gekappt) da = false;          // nicht ganz gezählt: nochmal versuchen
                        if (!da)
                        {
                            if (verbindungUnsicher) { try { if (!S.Gesund()) VerbindungErneuern(); } catch (AbbruchAusnahme) { throw; } catch { } S.Ftp.Timeout = 4000; verbindungUnsicher = false; }
                            x = BaumGroesse(S, p, deadline, lauf);
                            if (x.Lesefehler) verbindungUnsicher = true;
                            else if (!x.Gekappt) lock (groessenSperre) PFAD_GROESSE[p] = x;
                        }
                        else lauf(x.Bytes, x.Dateien);
                        g.Plus(x);
                    }
                    f.Bytes = g.Bytes; f.Dateien = g.Dateien; f.Ordner = g.Ordner; f.HatGroesse = true;
                    if (g.Gekappt) gekappt = true;
                    sum.Bytes += g.Bytes; sum.Dateien += g.Dateien; sum.Ordner += g.Ordner;
                }
            }
            finally { S.Ftp.Timeout = altTimeout; }
            if (gekappt || verbindungUnsicher) { try { if (!S.Gesund()) VerbindungErneuern(); } catch (AbbruchAusnahme) { throw; } catch { } }
            sum.Gekappt = gekappt;
            GROESSE_GEKAPPT = gekappt;
            return sum;
        }

        void VerbindungErneuern()
        {
            string ip = S.Ftp.Ip; int port = S.Ftp.Port;
            S.Ftp.Schliessen();
            var f = Ftp.Verbinden(ip, port);
            if (f != null) { f.Wartet = S.Ftp.Wartet; S.Ftp = f; }
        }

        public static string GroesseText(long bytes)
        {
            const double KB = 1024, MB = KB * 1024, GB = MB * 1024;
            var k = Sp.Kultur;
            if (bytes >= GB) return string.Format(k, "{0:N1} GB", bytes / GB);
            if (bytes >= MB) return string.Format(k, "{0:N1} MB", bytes / MB);
            if (bytes >= KB) return string.Format(k, "{0:N0} KB", bytes / KB);
            return bytes + (Sp.EN ? " bytes" : " Byte");
        }
        public static string GroesseSatz(Groesse g)
        {
            string t = string.Format(Sp.Kultur, T("{0} {1} in {2:N0} Dateien und {3:N0} Ordnern", "{0} {1} in {2:N0} files and {3:N0} folders"), g.Gekappt ? T("mindestens", "at least") : T("etwa", "about"), GroesseText(g.Bytes), g.Dateien, g.Ordner);
            if (g.Gekappt) t += T(" (große Ordner nicht ganz durchgezählt)", " (large folders not fully counted)");
            return t;
        }
        public static string FreigeraeumtText(Groesse g)
        {
            return string.Format(Sp.Kultur, T("Freigeräumt: {0}{1} in {2:N0} Dateien und {3:N0} Ordnern", "Freed: {0}{1} in {2:N0} files and {3:N0} folders"), g.Gekappt ? T("mindestens ", "at least ") : "", GroesseText(g.Bytes), g.Dateien, g.Ordner);
        }

        // -------------------------------------------------------------
        // Ausführen (Jailbreak-Tools zuletzt). weiter() wird gefragt, wenn
        // ShadowMount nicht sauber angehalten werden konnte; true = trotzdem löschen.
        // -------------------------------------------------------------
        public Ergebnis FuehreLoeschung(Func<bool> weiter)
        {
            var erg = new Ergebnis();
            // Merker vom letzten Löschlauf zurücksetzen (Schlussmeldung / Meldung bei Abbruch)
            SM_ANGEHALTEN = false; SM_STOP_GESENDET = false; SM_GRUND = "";
            // Fenster inzwischen zu oder Abbruch angefordert: gar nicht erst anfangen
            if (Abbruch) { erg.Abgebrochen = true; return erg; }
            AbhaengigeMitnehmen();
            LINK_CACHE = new Dictionary<string, List<Datei>>(StringComparer.Ordinal);
            var gewaehlt = Gewaehlt();
            erg.Frei.Gekappt = GROESSE_GEKAPPT;
            // Die Liste muss von genau der PS5 stammen, mit der gerade verbunden ist.
            if (S == null || FUNDE_IP == "" || (S.Ftp.Ip + ":" + S.Ftp.Port) != FUNDE_IP)
            {
                Melde(T("FEHLER: Die Liste stammt nicht von der gerade verbundenen PS5 - nichts gelöscht. Bitte neu verbinden.", "ERROR: The list is not from the PS5 currently connected - nothing deleted. Please connect again."));
                erg.Abgebrochen = true; erg.Fehler = 1; return erg;
            }
            // Erst sicherstellen, dass die Verbindung sauber antwortet.
            if (!S.Gesund())
            {
                Melde(T("Verbindung zur PS5 antwortet nicht sauber - verbinde neu ...", "Connection to the PS5 is not responding cleanly - reconnecting ..."));
                bool ok = false;
                try { VerbindungErneuern(); ok = S.Gesund(); } catch { ok = false; }
                if (!ok)
                {
                    Melde(T("FEHLER: Keine saubere Verbindung zur PS5 - nichts gelöscht. Bitte neu verbinden.", "ERROR: No clean connection to the PS5 - nothing deleted. Please connect again."));
                    erg.Abgebrochen = true; erg.Fehler = 1; return erg;
                }
            }
            // Sobald ShadowMount-Kacheln, -Spiele, -Daten oder die M.2 dran sind: erst anhalten.
            int smBetroffen = gewaehlt.Count(f => f.Quelle != "" || f.Kat == 7 || f.Kachel ||
                f.Pfade.Any(p => H.Like(p, SM_ORDNER + "*") || H.Gleich(p, "/user/data")));
            if (smBetroffen > 0)
            {
                // Nur bei bestätigt angehaltenem oder nachweislich nicht laufendem ShadowMount
                // wird ohne Rückfrage gelöscht. Unbekannt -> fragen (Abbrechen ist vorbelegt).
                var z = ShadowMountAnhalten();
                erg.SmGestoppt = z != SmZustand.Unbekannt;
                if (z == SmZustand.Unbekannt)
                {
                    Melde("");
                    Melde(T(" ShadowMount konnte nicht sicher angehalten werden. Läuft es noch, können", " ShadowMount could not be stopped safely. If it is still running,"));
                    Melde(T(" eingebundene Spieldaten beim Löschen beschädigt werden.", " mounted game data can be damaged while deleting."));
                    if (!weiter()) { erg.Abgebrochen = true; return erg; }
                    Melde(T(" Der Nutzer hat 'Trotzdem löschen' gewählt.", " The user chose 'Delete anyway'."));
                }
                Melde("");
            }
            Melde(T("Lösche ... (große Spiele können etwas dauern)", "Deleting ... (large games can take a while)"));
            int altTimeout = S.Ftp.Timeout;
            S.Ftp.Timeout = 7200000;                 // Löschen großer Dateien kann Minuten dauern
            var spielFehlt = H.Tab<int>();
            bool verloren = false;
            try
            {
                foreach (var k in new[] { 1, 2, 3, 4, 5, 7, 6 })
                {
                    foreach (var f in gewaehlt.Where(x => x.Kat == k))
                    {
                        if (verloren) { erg.Fehler++; continue; }
                        // Update/DLC eines Spiels, das selbst nicht gelöscht werden konnte: bleiben
                        if (f.Teil != null && f.Teil != "Spiel" && f.Gruppe != null && spielFehlt.ContainsKey(f.Gruppe))
                        { Melde(T("  übersprungen   ", "  skipped   ") + f.Text + T(" (das Spiel selbst konnte nicht gelöscht werden)", " (the game itself could not be deleted)")); erg.Fehler++; continue; }
                        bool ok = true;
                        WAR_SCHON_WEG = 0;
                        foreach (var p in f.Pfade)
                        {
                            try { if (!(f.NurLeer ? LeerenOrdnerLoeschen(p) : Loeschen(p))) ok = false; }
                            catch (AbbruchAusnahme) { throw; }
                            catch (Exception ex)
                            {
                                ok = false;
                                Melde("    (" + p + " : " + ex.Message + ")");
                                // Steht die Verbindung noch? Kurz nachfragen (höchstens 10 s).
                                int t = S.Ftp.Timeout; S.Ftp.Timeout = 10000;
                                try { if (S.Befehl("NOOP").Code != 200) verloren = true; } catch { verloren = true; }
                                S.Ftp.Timeout = t;
                            }
                            // Der Rest des Eintrags nur, wenn das Vorherige wirklich weg ist
                            if (!ok) break;
                        }
                        if (ok && WAR_SCHON_WEG >= f.Pfade.Count) { Melde(T("  war schon weg   ", "  already gone   ") + f.Text); continue; }
                        if (ok)
                        {
                            Melde(T("  gelöscht   ", "  deleted   ") + f.Text);
                            if (f.HatGroesse) { erg.Frei.Bytes += f.Bytes; erg.Frei.Dateien += f.Dateien; erg.Frei.Ordner += f.Ordner; }
                        }
                        else
                        {
                            Melde(T("  FEHLER      ", "  ERROR      ") + f.Text + T(" (konnte nicht ganz gelöscht werden)", " (could not be deleted completely)")); erg.Fehler++;
                            if (f.IstTeil("Spiel")) spielFehlt[f.Gruppe] = 1;
                        }
                    }
                }
                if (verloren)
                    Melde(T("FEHLER: Die Verbindung zur PS5 ist abgerissen - der Rest wurde nicht gelöscht. Bitte neu verbinden.", "ERROR: The connection to the PS5 dropped - the rest was not deleted. Please connect again."));
                else
                {
                    // Ordner, die nur wegen ShadowMount-Spielen einzeln gelistet waren und
                    // jetzt leer sind, auch entfernen (innerste zuerst).
                    var aufgeklappt = new List<string>(AUFGEKLAPPT); aufgeklappt.Reverse();
                    foreach (var o in aufgeklappt)
                        if (gewaehlt.Any(f => f.Pfade.Any(p => p.StartsWith(o + "/", StringComparison.Ordinal)))) LeerenOrdnerEntfernen(o);
                    // Ordner von Spielen, deren Teile gelöscht wurden und die jetzt leer sind
                    foreach (var g in LEER_DANACH.Keys.ToList())
                    {
                        if (!gewaehlt.Any(f => f.Gruppe != null && H.Gleich(f.Gruppe, g))) continue;
                        foreach (var o in LEER_DANACH[g]) LeerenOrdnerEntfernen(o);
                    }
                }
            }
            finally { S.Ftp.Timeout = altTimeout; }
            erg.SmText = SmSchlusstext(gewaehlt);
            erg.Verloren = verloren;
            return erg;
        }

        // Was der Nutzer zu ShadowMount wissen muss, je nachdem was gelöscht wurde.
        string SmSchlusstext(List<Eintrag> gewaehlt)
        {
            bool smDatenWeg = false, kachelnWeg = false;
            foreach (var f in gewaehlt)
            {
                foreach (var p in f.Pfade) if (H.Gleich(p, SM_ORDNER) || H.Gleich(p, "/user/data")) smDatenWeg = true;
                if (f.Kat == 1 && f.Quelle != "") kachelnWeg = true;
            }
            if (smDatenWeg) return T("ShadowMount wurde mitgelöscht - richte es neu ein, dann erscheinen die Spiele auf der M.2 wieder als Kacheln.", "ShadowMount was deleted too - set it up again and the games on the M.2 will show up as tiles again.");
            if (SM_ANGEHALTEN) return T("ShadowMount wurde angehalten - zum Weiterspielen einfach neu laden.", "ShadowMount was stopped - just load it again to keep playing.");
            if (kachelnWeg) return T("Die Spiele auf der Platte erscheinen wieder als Kacheln, sobald ShadowMount läuft.", "The games on the SSD/M.2 will show up as tiles again once ShadowMount is running.");
            return "";
        }
    }
}
