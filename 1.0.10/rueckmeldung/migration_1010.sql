-- PS5 Cleanup 1.0.10: neue Spalten für die bestehende Datenbank (einmalig, VOR dem Hochladen des neuen Workers)
--   npx wrangler d1 execute ps5cleanup-feedback --remote --file=migration_1010.sql
ALTER TABLE meldungen ADD COLUMN fehler2 TEXT CHECK (fehler2 IS NULL OR (json_valid(fehler2) AND json_array_length(fehler2) BETWEEN 1 AND 16));
ALTER TABLE meldungen ADD COLUMN uz TEXT CHECK (uz IS NULL OR length(uz) <= 1600);
ALTER TABLE meldungen ADD COLUMN protokoll TEXT CHECK (protokoll IS NULL OR length(protokoll) <= 120000);
ALTER TABLE meldungen ADD COLUMN db TEXT CHECK (db IS NULL OR length(db) <= 64);
ALTER TABLE meldungen ADD COLUMN hb TEXT CHECK (hb IS NULL OR length(hb) <= 1600);
ALTER TABLE meldungen ADD COLUMN ek TEXT CHECK (ek IS NULL OR length(ek) <= 1600);
