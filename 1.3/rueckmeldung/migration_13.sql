-- NEU 1.3 (21.11.2026): zwei neue Spalten - gr (bei 👎 ein Tipp, was nicht geklappt hat) und hg (Namen der nachgeladenen Programme, die liefen)
-- Einspielen VOR dem Hochladen des neuen Workers:  npx wrangler d1 execute ps5cleanup-feedback --remote --file=migration_13.sql
ALTER TABLE meldungen ADD COLUMN gr TEXT CHECK (gr IS NULL OR gr IN ('fehlt', 'zuordnung', 'loeschen', 'haengt'));
ALTER TABLE meldungen ADD COLUMN hg TEXT CHECK (hg IS NULL OR length(hg) <= 600);
