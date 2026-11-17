-- PS5 Cleanup 1.1: neue Spalten und Zähler für die bestehende Datenbank (einmalig, VOR dem Hochladen des neuen Workers)
-- Dashboard: D1 → „ps5cleanup-feedback“ → Console → einfügen → Execute
-- oder:  npx wrangler d1 execute ps5cleanup-feedback --remote --file=migration_11.sql
ALTER TABLE meldungen ADD COLUMN art TEXT CHECK (art IS NULL OR art IN ('auswahl', 'komplett'));
ALTER TABLE meldungen ADD COLUMN quelle TEXT CHECK (quelle IS NULL OR quelle IN ('ps5', 'browser'));
CREATE TABLE IF NOT EXISTS direkt_tage (
  tag    TEXT    PRIMARY KEY,
  anzahl INTEGER NOT NULL
);
