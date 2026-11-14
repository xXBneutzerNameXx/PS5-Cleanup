-- PS5 Cleanup – Rückmeldungen (Cloudflare D1)
-- Einspielen im Dashboard: D1 → Datenbank „ps5cleanup-feedback“ → Console → diesen Text einfügen → Execute
-- oder per Befehl:  npx wrangler d1 execute ps5cleanup-feedback --remote --file=schema.sql
--
-- Bewusst KEINE Spalten für IP-Adresse, Browser (User-Agent), Land oder Cookies.
-- Die Kurznamen der Spalten sind dieselben wie im QR-Link (v, b, bd, s, m, fw, ...).

CREATE TABLE IF NOT EXISTS meldungen (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,                        -- laufende Meldungs-Nummer (wird nie neu vergeben)
  zeit        TEXT    NOT NULL,                                         -- Zeitpunkt des Absendens, UTC, z. B. 2026-11-03T18:47:12Z
  wertung     TEXT    NOT NULL CHECK (wertung IN ('gut', 'problem')),   -- 👍 = gut, 👎 = problem
  text        TEXT    CHECK (text IS NULL OR length(text) <= 500),      -- freiwilliger Text, schon geschwärzt, höchstens 500 Zeichen
  v           TEXT    CHECK (v  IS NULL OR length(v) <= 8),             -- Programmversion, z. B. 1.0.7
  b           TEXT    CHECK (b  IS NULL OR b IN ('r', 't')),            -- Fassung: r = normal (löscht), t = Test (löscht nie)
  bd          TEXT    CHECK (bd IS NULL OR length(bd) = 10),            -- Bau-Stempel JJMMTTHHMM, z. B. 2611031946
  s           INTEGER CHECK (s  IS NULL OR s IN (0, 1, 2)),             -- 0 vor dem Nachschauen, 1 danach, 2 am Ende
  m           TEXT    CHECK (m  IS NULL OR length(m) BETWEEN 1 AND 16), -- Modellnummer, z. B. CFI-7021
  fw          TEXT    CHECK (fw IS NULL OR length(fw) <= 5),            -- Firmware, z. B. 12.00
  l           TEXT    NOT NULL CHECK (l IN ('de', 'en')),               -- Sprache der Seite
  dd          INTEGER CHECK (dd IS NULL OR dd BETWEEN -1 AND 99),       -- Laufwerks-Kopplung, Statuswert (-1 = nicht abgefragt)
  jb          TEXT    CHECK (jb IS NULL OR length(jb) <= 60),           -- Jailbreak-Einstiege, z. B. y2jb,browser ('' = keine angegeben)
  sp          TEXT    CHECK (sp IS NULL OR length(sp) = 3),             -- Speicher M.2 / erweitert / USB, z. B. 110
  sm          INTEGER CHECK (sm IS NULL OR sm BETWEEN 0 AND 3),         -- ShadowMount: 0 keine Spiele, 1 Spiele vorhanden, 2 angehalten, 3 unklar
  a           TEXT    CHECK (a  IS NULL OR length(a) <= 48),            -- Einträge je Bereich, z. B. 12.30.5.8.40.20.3
  n           INTEGER CHECK (n  IS NULL OR n  BETWEEN 0 AND 100000),    -- gefundene Einträge
  g           INTEGER CHECK (g  IS NULL OR g  BETWEEN 0 AND 100000),    -- zum Löschen ausgewählt
  f           INTEGER CHECK (f  IS NULL OR f  BETWEEN 0 AND 100000),    -- Fehler
  p           INTEGER CHECK (p  IS NULL OR p  BETWEEN 0 AND 100000),    -- nur teilweise entfernt
  ps          INTEGER CHECK (ps IS NULL OR ps BETWEEN 0 AND 100000),    -- vom PS4-Schutz zurückgehalten
  u           INTEGER CHECK (u  IS NULL OR u  BETWEEN 0 AND 100000),    -- nicht lesbare Ordner
  sb          INTEGER CHECK (sb IS NULL OR sb IN (0, 1)),               -- Speicherbremse
  mb          INTEGER CHECK (mb IS NULL OR mb BETWEEN 0 AND 100000),    -- höchster Speicherverbrauch in MB
  t           INTEGER CHECK (t  IS NULL OR t  BETWEEN 0 AND 100000),    -- Dauer des Nachschauens in Sekunden
  ab          INTEGER CHECK (ab IS NULL OR ab IN (0, 1)),               -- abgebrochen
  ok          INTEGER CHECK (ok IS NULL OR ok IN (0, 1)),               -- laut App alles geklappt (1) oder nicht (0)
  fehler      TEXT    CHECK (fehler IS NULL OR (json_valid(fehler) AND json_array_length(fehler) BETWEEN 1 AND 5)),
                                                                        -- bis zu 5 Fehler als JSON: [{"schritt":"sony","art":"ps4","ort":"i","code":"0x80990001"}]
  fehler2     TEXT    CHECK (fehler2 IS NULL OR (json_valid(fehler2) AND json_array_length(fehler2) BETWEEN 1 AND 16)),
                                                                        -- NEU 1.0.10: alle Fehler (bis 16 verschiedene) mit Anzahl "anz"
  uz          TEXT    CHECK (uz IS NULL OR length(uz) <= 1600),         -- NEU 1.0.10: nicht geprüfte Ordner "bereich.grund*anzahl~..."
  db          TEXT    CHECK (db IS NULL OR length(db) <= 64),           -- NEU 1.0.10: Sonys Titel-Liste "i12.p1.v3" (nur gelesen)
  hb          TEXT    CHECK (hb IS NULL OR length(hb) <= 1600),         -- NEU 1.0.10: Programme der Fehler "playden*3~hb*1"
  ek          TEXT    CHECK (ek IS NULL OR length(ek) <= 1600),         -- NEU 1.0.10: erfolgreiche Wege "sony.ps5*2~datei.rest*9"
  protokoll   TEXT    CHECK (protokoll IS NULL OR length(protokoll) <= 120000),  -- NEU 1.0.10: freiwillig mitgeschicktes, bereinigtes Protokoll
  formular_id TEXT    NOT NULL UNIQUE CHECK (length(formular_id) = 22)  -- Zufallswert je Formular, verhindert doppeltes Speichern
);

-- Zähler für die Home-Assistant-Benachrichtigungen (höchstens 20 pro UTC-Tag).
-- Enthält nur Datum und Anzahl.
CREATE TABLE IF NOT EXISTS benachrichtigungen (
  tag    TEXT    PRIMARY KEY,   -- UTC-Tag, z. B. 2026-11-03
  anzahl INTEGER NOT NULL
);
