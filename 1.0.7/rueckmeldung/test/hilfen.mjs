// Test-Hilfen: D1-Ersatz auf Basis von node:sqlite (D1 ist SQLite), eine Netz-Attrappe für
// Turnstile (verhält sich wie die offiziellen Cloudflare-Test-Schlüssel) und für den
// Home-Assistant-Webhook, sowie ein ctx mit waitUntil wie im Worker.
import { DatabaseSync } from 'node:sqlite';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const HIER = path.dirname(fileURLToPath(import.meta.url));
export const SCHEMA = readFileSync(path.join(HIER, '..', 'schema.sql'), 'utf8');

// Offizielle Turnstile-Test-Schlüssel (developers.cloudflare.com/turnstile/troubleshooting/testing/)
export const TEST_SITEKEY_OK = '1x00000000000000000000AA';
export const TEST_SECRET_OK = '1x0000000000000000000000000000000AA';
export const TEST_SECRET_FALSCH = '2x0000000000000000000000000000000AA';
export const TEST_SECRET_DOPPELT = '3x0000000000000000000000000000000AA';
export const DUMMY_TOKEN = 'XXXX.DUMMY.TOKEN.XXXX';
export const SITEVERIFY = 'https://challenges.cloudflare.com/turnstile/v0/siteverify';
export const WEBHOOK = 'https://ha.beispiel.invalid/api/webhook/ps5-test-123';

const flach = (zeile) => (zeile ? { ...zeile } : zeile);

class D1Anweisung {
  constructor(db, sql, werte = []) {
    this.db = db;
    this.sql = sql;
    this.werte = werte;
  }
  bind(...werte) {
    for (const w of werte) {
      // Wie D1: undefined, Booleans und Objekte sind nicht erlaubt.
      if (w === undefined) throw new TypeError("D1_TYPE_ERROR: Type 'undefined' not supported");
      if (typeof w === 'boolean' || (typeof w === 'object' && w !== null)) {
        throw new TypeError(`D1_TYPE_ERROR: Type '${typeof w}' not supported`);
      }
    }
    const platzhalter = (this.sql.match(/\?/g) || []).length;
    if (platzhalter !== werte.length) {
      throw new Error(`Wrong number of parameter bindings: ${platzhalter} erwartet, ${werte.length} bekommen`);
    }
    return new D1Anweisung(this.db, this.sql, werte);
  }
  _alle() {
    return this.db.prepare(this.sql).all(...this.werte).map(flach);
  }
  async all() {
    return { success: true, results: this._alle(), meta: {} };
  }
  async first(spalte) {
    const zeilen = this._alle();
    if (!zeilen.length) return null;
    return spalte ? (zeilen[0][spalte] ?? null) : zeilen[0];
  }
  async run() {
    const info = this.db.prepare(this.sql).run(...this.werte);
    return { success: true, results: [], meta: { changes: Number(info.changes), last_row_id: Number(info.lastInsertRowid) } };
  }
}

export class D1Ersatz {
  constructor() {
    this.db = new DatabaseSync(':memory:');
    this.db.exec(SCHEMA);
    this.kaputt = false; // true = jede Abfrage schlägt fehl (zum Testen)
  }
  prepare(sql) {
    if (this.kaputt) throw new Error('D1 nicht erreichbar');
    return new D1Anweisung(this.db, sql);
  }
  async batch(anweisungen) {
    this.db.exec('BEGIN');
    try {
      const ergebnisse = anweisungen.map((a) => ({ success: true, results: a._alle(), meta: {} }));
      this.db.exec('COMMIT');
      return ergebnisse;
    } catch (fehler) {
      this.db.exec('ROLLBACK');
      throw fehler;
    }
  }
  // Nur für Tests: alles synchron auslesen.
  alleZeilen() {
    return this.db.prepare('SELECT * FROM meldungen ORDER BY id').all().map(flach);
  }
  zaehler() {
    return this.db.prepare('SELECT * FROM benachrichtigungen ORDER BY tag').all().map(flach);
  }
  // Wirklich ALLES aus allen Tabellen (auch interne wie sqlite_sequence).
  komplettAbzug() {
    const tabellen = this.db.prepare("SELECT name FROM sqlite_master WHERE type = 'table'").all().map((t) => t.name);
    const abzug = {};
    for (const t of tabellen) abzug[t] = this.db.prepare(`SELECT * FROM "${t}"`).all().map(flach);
    return abzug;
  }
  spalten() {
    return this.db.prepare('PRAGMA table_info(meldungen)').all().map((s) => s.name);
  }
}

// Netz-Attrappe. Turnstile wie in der Cloudflare-Doku für die Test-Secret-Keys:
// 1x… = immer gültig, 2x… = immer ungültig, 3x… = „timeout-or-duplicate“, alles andere = ungültiger Schlüssel.
// Webhook: zeichnet jeden Aufruf auf; webhook = 'ok' | 'fehler500' | 'netzweg'.
export function netzAttrappe({ turnstileWeg = false, webhook = 'ok' } = {}) {
  const turnstile = [];
  const webhookAufrufe = [];
  const fremd = [];
  const fetchErsatz = async (eingabe, init = {}) => {
    const url = typeof eingabe === 'string' ? eingabe : eingabe.url;
    if (url === SITEVERIFY) {
      if (turnstileWeg) throw new TypeError('fetch failed');
      const koerper = init.body instanceof URLSearchParams ? init.body.toString() : String(init.body ?? '');
      const felder = Object.fromEntries(new URLSearchParams(koerper));
      turnstile.push({ url, methode: init.method, koerper, felder });
      let antwort;
      if (!felder.secret) antwort = { success: false, 'error-codes': ['missing-input-secret'] };
      else if (!felder.response) antwort = { success: false, 'error-codes': ['missing-input-response'] };
      else if (felder.secret === TEST_SECRET_OK)
        antwort = felder.response === DUMMY_TOKEN
          ? { success: true, 'error-codes': [], challenge_ts: new Date().toISOString(), hostname: 'example.com', action: '', cdata: '' }
          : { success: false, 'error-codes': ['invalid-input-response'] }; // Test-Secret nimmt nur das Dummy-Token
      else if (felder.secret === TEST_SECRET_FALSCH) antwort = { success: false, 'error-codes': ['invalid-input-response'] };
      else if (felder.secret === TEST_SECRET_DOPPELT) antwort = { success: false, 'error-codes': ['timeout-or-duplicate'] };
      else antwort = { success: false, 'error-codes': ['invalid-input-secret'] };
      return new Response(JSON.stringify(antwort), { status: 200, headers: { 'content-type': 'application/json' } });
    }
    if (url.startsWith('https://ha.beispiel.invalid/')) {
      webhookAufrufe.push({
        url,
        methode: init.method,
        kopf: Object.fromEntries(new Headers(init.headers || {})),
        json: JSON.parse(String(init.body)),
      });
      if (webhook === 'netzweg') throw new TypeError('fetch failed');
      if (webhook === 'fehler500') return new Response('kaputt', { status: 500 });
      return new Response(null, { status: 200 });
    }
    fremd.push(url);
    throw new Error('Unerwarteter Netzaufruf: ' + url);
  };
  return { fetchErsatz, turnstile, webhookAufrufe, fremd };
}

// ctx wie im Worker: waitUntil sammelt die Hintergrund-Aufgaben, fertig() wartet darauf.
export function neuerCtx() {
  const aufgaben = [];
  return {
    waitUntil(p) {
      aufgaben.push(Promise.resolve(p));
    },
    async fertig() {
      await Promise.allSettled(aufgaben);
    },
    aufgaben,
  };
}

export const PASSWORT = 'richtiges-Passwort-42';

export function neueUmgebung(extra = {}) {
  return {
    DB: new D1Ersatz(),
    TURNSTILE_SITEKEY: TEST_SITEKEY_OK,
    TURNSTILE_SECRET: TEST_SECRET_OK,
    UEBERSICHT_PASSWORT: PASSWORT,
    ...extra,
  };
}

export const BASIS = 'https://ps5cleanup-feedback.beispiel.workers.dev';

export function basicAuth(benutzer, passwort) {
  const bytes = new TextEncoder().encode(`${benutzer}:${passwort}`);
  return 'Basic ' + Buffer.from(bytes).toString('base64');
}
