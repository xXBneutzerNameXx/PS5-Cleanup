// Automatische Tests für den Rückmeldungs-Worker.
// Start:  npm test   (oder: node --test test/)
import { describe, it, beforeEach, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import worker from '../src/index.js';
import {
  neueUmgebung,
  netzAttrappe,
  neuerCtx,
  basicAuth,
  BASIS,
  PASSWORT,
  DUMMY_TOKEN,
  TEST_SECRET_FALSCH,
  TEST_SECRET_DOPPELT,
  SITEVERIFY,
  WEBHOOK,
} from './hilfen.mjs';

const ECHTES_FETCH = globalThis.fetch;
let netz;

beforeEach(() => {
  netz = netzAttrappe();
  globalThis.fetch = netz.fetchErsatz;
});
afterEach(() => {
  globalThis.fetch = ECHTES_FETCH;
  assert.deepEqual(netz.fremd, [], 'kein Aufruf an fremde Adressen');
});

// Genau so sieht ein echter Link aus der PS5-App aus (Vorgabe aus der App, 03.11.2026).
const ECHTER_LINK =
  '/r?v=1.0.7&b=t&bd=2611031946&s=2&m=CFI-7021&fw=12.00&l=de&dd=1&jb=y2jb,browser&sp=110&sm=1' +
  '&a=12.30.5.8.40.20.3&n=118&g=4&f=1&p=0&ps=0&u=0&sb=0&mb=38&t=12&ab=0&ok=0' +
  '&e=sony.ps4.i.0x80990001~datei.rest.m2.EACCES';

const ERWARTETE_ZEILE = {
  v: '1.0.7', b: 't', bd: '2611031946', s: 2, m: 'CFI-7021', fw: '12.00', l: 'de', dd: 1, jb: 'y2jb,browser',
  sp: '110', sm: 1, a: '12.30.5.8.40.20.3', n: 118, g: 4, f: 1, p: 0, ps: 0, u: 0, sb: 0, mb: 38, t: 12, ab: 0, ok: 0,
  fehler: '[{"schritt":"sony","art":"ps4","ort":"i","code":"0x80990001"},{"schritt":"datei","art":"rest","ort":"m2","code":"EACCES"}]',
};

const escHtml = (s) =>
  String(s).replace(/[&<>"']/g, (z) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[z]);
const zeile = (name, wert) => `<tr><th scope="row">${escHtml(name)}</th><td>${escHtml(wert)}</td></tr>`;
const unesc = (s) => s.replace(/&quot;/g, '"').replace(/&#39;/g, "'").replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&amp;/g, '&');

async function hole(env, pfad, init = {}, ctx = neuerCtx()) {
  const antwort = await worker.fetch(new Request(BASIS + pfad, init), env, ctx);
  const text = await antwort.text();
  return { antwort, text, status: antwort.status, ctx };
}

// Liest die versteckten Formularfelder aus der Seite (so wie der Browser sie mitschicken würde).
function versteckteFelder(html) {
  const felder = {};
  for (const m of html.matchAll(/<input type="hidden" name="([^"]+)" value="([^"]*)">/g)) felder[unesc(m[1])] = unesc(m[2]);
  return felder;
}

function formular(felder) {
  const q = new URLSearchParams();
  for (const [k, v] of Object.entries(felder)) if (v !== undefined) q.set(k, v);
  return q;
}

let fidZaehler = 0;
const neueFid = () => ('testformular' + String(++fidZaehler).padStart(10, '0')).slice(0, 22);

async function sende(env, felder, { kopf = {}, ctx = neuerCtx() } = {}) {
  const alle = { fid: neueFid(), 'cf-turnstile-response': DUMMY_TOKEN, ...felder };
  const ergebnis = await hole(
    env,
    '/senden',
    { method: 'POST', headers: { 'content-type': 'application/x-www-form-urlencoded', ...kopf }, body: formular(alle).toString() },
    ctx
  );
  await ctx.fertig();
  return ergebnis;
}

// Kompletter Weg wie im Browser: Seite holen, versteckte Felder übernehmen, Wertung/Text dazu, absenden.
async function seiteUndSenden(env, link, { wertung = 'problem', text = '', kopf = {}, ctx } = {}) {
  const seite = await hole(env, link);
  return sende(env, { ...versteckteFelder(seite.text), wertung, text }, { kopf, ctx });
}

function pruefeSicherheitsHeader(antwort) {
  const csp = antwort.headers.get('content-security-policy') || '';
  assert.match(csp, /default-src 'none'/);
  assert.match(csp, /frame-ancestors 'none'/);
  assert.equal(antwort.headers.get('x-content-type-options'), 'nosniff');
  assert.equal(antwort.headers.get('referrer-policy'), 'no-referrer');
  assert.equal(antwort.headers.get('x-frame-options'), 'DENY');
  assert.equal(antwort.headers.get('set-cookie'), null, 'es darf kein Cookie gesetzt werden');
}

const auth = (pw = PASSWORT, benutzer = 'admin') => ({ headers: { Authorization: basicAuth(benutzer, pw) } });

// ---------------------------------------------------------------------------

describe('GET /r – echter Link aus der App', () => {
  it('zeigt alle Angaben in Klartext, gruppiert, mit Hinweisen und Turnstile', async () => {
    const env = neueUmgebung();
    const { antwort, text, status } = await hole(env, ECHTER_LINK);
    assert.equal(status, 200);
    assert.match(antwort.headers.get('content-type'), /^text\/html; charset=utf-8/);
    pruefeSicherheitsHeader(antwort);
    assert.ok(text.includes('<html lang="de">'));
    assert.ok(text.includes('<h1>PS5 Cleanup – Rückmeldung</h1>'));

    // Gruppen in der vorgegebenen Reihenfolge
    const titel = [...text.matchAll(/<h2>([^<]+)<\/h2>/g)].map((m) => m[1]);
    assert.deepEqual(titel, ['Programm', 'Konsole', 'Umgebung', 'Ergebnis', 'Fehler']);

    for (const [name, wert] of [
      ['Version', '1.0.7'],
      ['Fassung', 'Test-Fassung (löscht nie)'],
      ['Bau', '03.11.26 19:46'],
      ['Zeitpunkt', 'am Ende, nach der Simulation'],
      ['Seitensprache', 'Deutsch'],
      ['Modell', 'CFI-7021'],
      ['Firmware', '12.00'],
      ['Laufwerks-Kopplung', 'online gekoppelt (Wert 1)'],
      ['Jailbreak-Einstiege', 'YouTube (Y2JB), Browserspeicher'],
      ['Speicher', 'M.2 ja, erweiterter Speicher ja, USB-Laufwerk nein'],
      ['ShadowMount', 'ShadowMount-Spiele vorhanden'],
      ['Einträge je Bereich', 'Spiele 12 · Apps 30 · Spielstände 5 · Trophäen 8 · Bilder & Videos 40 · Sonstiges 20 · Jailbreak 3'],
      ['Gefundene Einträge', '118'],
      ['Zum Löschen ausgewählt', '4'],
      ['Fehler', '1'],
      ['Nur teilweise entfernt', '0'],
      ['Vom PS4-Schutz zurückgehalten', '0'],
      ['Nicht geprüfte Ordner', '0'],
      ['Speicherbremse', 'nein'],
      ['Höchster Speicherverbrauch', '38 MB'],
      ['Dauer des Nachschauens', '12 s'],
      ['Abgebrochen', 'nein'],
      ['Ergebnis laut App', 'nicht alles geklappt'],
      ['1.', 'Sony-Deinstallation · PS4-Spiel · intern · 0x80990001'],
      ['2.', 'Datei/Ordner löschen · Reste · M.2 · EACCES'],
    ]) {
      assert.ok(text.includes(zeile(name, wert)), `fehlt: ${name} = ${wert}`);
    }

    // Pflicht-Hinweise
    assert.ok(
      text.includes(
        'Gesendet wird nur, was hier steht, plus dein 👍/👎 und dein Text. Keine Namen von Spielen oder Apps, kein Konto, keine Dateien. Wie bei jedem Webseitenaufruf sieht der Server kurz die Internet-Adresse deines Handys – wir speichern sie nicht.'
      )
    );
    assert.ok(text.includes('Bitte schreib keine persönlichen Daten in das Textfeld.'));
    assert.ok(text.includes('href="/datenschutz?l=de"'));
    assert.ok(text.includes('👍 Hat geklappt'));
    assert.ok(text.includes('👎 Problem'));
    assert.ok(text.includes('maxlength="500"'));
    assert.ok(text.includes('data-sitekey="1x00000000000000000000AA"'));
    assert.ok(text.includes('action="/senden"'));
    assert.ok(!text.includes('ohne Gerätedaten'));

    // Versteckte Felder = genau die geprüften Werte aus dem Link (in derselben Schreibweise)
    const felder = versteckteFelder(text);
    const erwartet = Object.fromEntries(new URLSearchParams(ECHTER_LINK.slice(3)));
    assert.match(felder.fid, /^[A-Za-z0-9_-]{22}$/);
    delete felder.fid;
    assert.deepEqual(felder, erwartet);

    // Die Seite selbst ruft nichts auf.
    assert.equal(netz.turnstile.length + netz.webhookAufrufe.length, 0);
  });

  it('Englisch mit l=en', async () => {
    const { text } = await hole(neueUmgebung(), ECHTER_LINK.replace('l=de', 'l=en'));
    assert.ok(text.includes('<html lang="en">'));
    assert.ok(text.includes('<h1>PS5 Cleanup – Feedback</h1>'));
    const titel = [...text.matchAll(/<h2>([^<]+)<\/h2>/g)].map((m) => m[1]);
    assert.deepEqual(titel, ['Program', 'Console', 'Environment', 'Result', 'Errors']);
    assert.ok(text.includes(zeile('Build', 'test build (never deletes)')));
    assert.ok(text.includes(zeile('Built', '2026-11-03 19:46')));
    assert.ok(text.includes(zeile('ShadowMount', 'ShadowMount games present')));
    assert.ok(text.includes(zeile('1.', 'Sony uninstall · PS4 game · internal · 0x80990001')));
    assert.ok(text.includes('Only what is shown here is sent, plus your 👍/👎 and your text.'));
    assert.ok(text.includes('Please do not write any personal data into the text field.'));
    assert.ok(text.includes('href="/datenschutz?l=en"'));
    assert.ok(text.includes('data-language="en"'));
  });

  it('Sprachwechsel-Link behält alle Werte', async () => {
    const { text } = await hole(neueUmgebung(), ECHTER_LINK);
    const href = unesc(/<a href="(\/r\?[^"]+)" lang="en">/.exec(text)[1]);
    const neu = Object.fromEntries(new URL(BASIS + href).searchParams);
    const alt = Object.fromEntries(new URLSearchParams(ECHTER_LINK.slice(3)));
    assert.deepEqual(neu, { ...alt, l: 'en' });
  });

  // NEU 1.0.8 (06.11.2026): Kopplung in Worten wie in der App, Zahl in Klammern; erste PS5 nach der Modellnummer
  it('Laufwerks-Kopplung in Worten (alle Werte, erste PS5, Englisch)', async () => {
    const env = neueUmgebung();
    for (const [dd, m, wort] of [
      ['0', 'CFI-2016', 'gekoppelt (Wert 0)'],
      ['1', 'CFI-7021', 'online gekoppelt (Wert 1)'],
      ['2', 'CFI-2116B', 'nicht gekoppelt (Wert 2)'],
      ['3', 'CFI-7021', 'Koppeln nicht fertig (Wert 3)'],
      ['4', 'CFI-2116', 'Entkoppeln nicht fertig (Wert 4)'],
      ['5', 'CFI-2016', 'ab Werk gekoppelt (Wert 5)'],
      ['6', 'CFI-2016', 'ab Werk gekoppelt (Wert 6)'],
      ['7', 'CFI-2016', 'Wert 7'],
      ['0', 'CFI-1216A', 'erste PS5, Laufwerk fest eingebaut (Wert 0)'],
      ['0', 'CFI-1016B', 'erste PS5, Digital Edition (Wert 0)'],
      ['-1', 'CFI-7021', 'nicht abgefragt'],
    ]) {
      const { text } = await hole(env, `/r?v=1.0.8&m=${m}&dd=${dd}`);
      assert.ok(text.includes(zeile('Laufwerks-Kopplung', wort)), `dd=${dd} m=${m}`);
    }
    let { text } = await hole(env, '/r?v=1.0.8&m=CFI-2016&dd=6&l=en');
    assert.ok(text.includes(zeile('Drive pairing', 'factory paired (value 6)')));
    ({ text } = await hole(env, '/r?v=1.0.8&m=CFI-7021&dd=1&l=en'));
    assert.ok(text.includes(zeile('Drive pairing', 'paired online (value 1)')));
    ({ text } = await hole(env, '/r?v=1.0.8&m=CFI-7021&dd=4&l=en'));
    assert.ok(text.includes(zeile('Drive pairing', 'unpairing not finished (value 4)')));
    ({ text } = await hole(env, '/r?v=1.0.8&m=CFI-1016A&dd=0&l=en'));
    assert.ok(text.includes(zeile('Drive pairing', 'original PS5, built-in drive (value 0)')));
    ({ text } = await hole(env, '/r?v=1.0.8&dd=1'));
    assert.ok(text.includes(zeile('Laufwerks-Kopplung', 'online gekoppelt (Wert 1)')), 'ohne Modell: nach dem Wert');
  });

  it('weitere Werte: normale Fassung, ShadowMount-Zustände, Kopplung nicht abgefragt, Jailbreak leer', async () => {
    const env = neueUmgebung();
    let { text } = await hole(env, '/r?v=1.0.7&b=r&s=2&sm=2&dd=-1&jb=');
    assert.ok(text.includes(zeile('Zeitpunkt', 'am Ende, nach dem Löschen')));
    assert.ok(text.includes(zeile('Fassung', 'normale Fassung (löscht)')));
    assert.ok(text.includes(zeile('ShadowMount', 'vor dem Löschen angehalten')));
    assert.ok(text.includes(zeile('Laufwerks-Kopplung', 'nicht abgefragt')));
    assert.ok(text.includes(zeile('Jailbreak-Einstiege', 'keine angegeben')));
    assert.ok(text.includes(zeile('', 'keine Fehlerdetails im Link')));
    ({ text } = await hole(env, '/r?s=0&sm=0'));
    assert.ok(text.includes(zeile('Zeitpunkt', 'vor dem Nachschauen')));
    assert.ok(text.includes(zeile('ShadowMount', 'keine ShadowMount-Spiele')));
    assert.ok(!text.includes('<h2>Ergebnis</h2>'), 'ohne Ergebnis-Werte keine Ergebnis-Gruppe');
    ({ text } = await hole(env, '/r?s=1&sm=3&ok=1&sb=1&ab=1'));
    assert.ok(text.includes(zeile('Zeitpunkt', 'nach dem Nachschauen')));
    assert.ok(text.includes(zeile('ShadowMount', 'Zustand unklar')));
    assert.ok(text.includes(zeile('Ergebnis laut App', 'alles geklappt')));
    assert.ok(text.includes(zeile('Speicherbremse', 'ja')));
    assert.ok(text.includes(zeile('Abgebrochen', 'ja')));
  });

  it('ohne Parameter: funktioniert, Hinweis „ohne Gerätedaten“', async () => {
    for (const pfad of ['/r', '/r?', '/r?l=de']) {
      const { text, status } = await hole(neueUmgebung(), pfad);
      assert.equal(status, 200);
      assert.ok(text.includes('ohne Gerätedaten'), pfad);
      assert.ok(!text.includes('<h2>Programm</h2>'));
      assert.ok(text.includes('<form id="formular"'));
      assert.deepEqual(Object.keys(versteckteFelder(text)), ['l', 'fid']);
    }
    const en = await hole(neueUmgebung(), '/r?l=en');
    assert.ok(en.text.includes('without device data'));
  });

  it('CSP: nur eigenes Skript (Nonce) und Turnstile; Nonce jedes Mal neu; nichts Fremdes geladen', async () => {
    const env = neueUmgebung();
    const a = await hole(env, ECHTER_LINK);
    const b = await hole(env, ECHTER_LINK);
    const csp = a.antwort.headers.get('content-security-policy');
    const nonce = /'nonce-([A-Za-z0-9_-]+)'/.exec(csp)[1];
    assert.match(csp, /script-src 'nonce-[A-Za-z0-9_-]+' https:\/\/challenges\.cloudflare\.com/);
    assert.match(csp, /frame-src https:\/\/challenges\.cloudflare\.com/);
    const skripte = [...a.text.matchAll(/<script\b[^>]*>/g)].map((m) => m[0]);
    assert.equal(skripte.length, 2, 'genau zwei Skripte: eigenes + Turnstile');
    for (const s of skripte) assert.ok(s.includes(`nonce="${nonce}"`));
    assert.ok(skripte[1].includes('src="https://challenges.cloudflare.com/turnstile/v0/api.js"'));
    assert.notEqual(nonce, /'nonce-([A-Za-z0-9_-]+)'/.exec(b.antwort.headers.get('content-security-policy'))[1]);
    assert.notEqual(versteckteFelder(a.text).fid, versteckteFelder(b.text).fid);
    const extern = [...a.text.matchAll(/\b(?:src|href)="(https?:[^"]*)"/g)].map((m) => m[1]);
    assert.deepEqual(extern, ['https://challenges.cloudflare.com/turnstile/v0/api.js']);
    assert.ok(!/<link\b/i.test(a.text));
    assert.ok(!/@import|url\(/i.test(a.text));
  });

  it('HEAD /r liefert Kopfzeilen ohne Inhalt', async () => {
    const antwort = await worker.fetch(new Request(BASIS + ECHTER_LINK, { method: 'HEAD' }), neueUmgebung(), neuerCtx());
    assert.equal(antwort.status, 200);
    assert.equal(await antwort.text(), '');
  });
});

describe('GET /r – bösartige und ungültige Parameter', () => {
  it('m=<script> und Müll in allen Feldern: nichts ungefiltert, „unbekannt“ bzw. weggelassen', async () => {
    const boese = {
      m: '<script>alert(1)</script>',
      v: '"><img src=x onerror=alert(1)>',
      fw: '<svg/onload=alert(1)>',
      l: '"><script>alert(1)</script>',
      b: "t' onmouseover='alert(1)",
      bd: '<b>2611031946</b>',
      s: 'javascript:alert(1)',
      dd: '1<script>',
      jb: '<script>alert(1)</script>,y2jb"onclick="alert(1)',
      sp: '1<1',
      sm: '9',
      a: '1.2.3.4.5.6.<7>',
      n: '<b>1</b>',
      g: '-5',
      f: '100001',
      p: '1e3',
      ok: '1<script>',
      e: '<script>alert(1)</script>~sony.ps4.i.<img src=x onerror=alert(1)>~sony.ps4.i.0x80990001"><b>',
      foo: '<script>alert(2)</script>',
    };
    const { text, status, antwort } = await hole(neueUmgebung(), '/r?' + new URLSearchParams(boese));
    assert.equal(status, 200);
    pruefeSicherheitsHeader(antwort);
    for (const verboten of ['alert(', 'onerror', 'onload', 'onmouseover', 'onclick', '<img', '<svg', '<b>']) {
      assert.ok(!text.includes(verboten), `"${verboten}" darf nicht auftauchen`);
    }
    assert.equal([...text.matchAll(/<script\b/g)].length, 2, 'nur die zwei eigenen Skript-Tags');
    assert.ok(text.includes('<html lang="de">'));
    for (const [name, wert] of [['Version', 'unbekannt'], ['Fassung', 'unbekannt'], ['Zeitpunkt', 'unbekannt'], ['Modell', 'unbekannt'], ['Firmware', 'unbekannt']]) {
      assert.ok(text.includes(zeile(name, wert)), name);
    }
    assert.ok(text.includes(zeile('Jailbreak-Einstiege', 'keine angegeben')));
    for (const weg of ['Bau', 'Laufwerks-Kopplung', 'Speicher', 'ShadowMount', 'Einträge je Bereich', 'Gefundene Einträge']) {
      assert.ok(!text.includes(`<th scope="row">${weg}</th>`), `${weg} sollte weggelassen sein`);
    }
    assert.ok(text.includes(zeile('', 'keine Fehlerdetails im Link')));
    assert.deepEqual(Object.keys(versteckteFelder(text)).sort(), ['fid', 'jb', 'l']);
  });

  it('e mit Müll: ungültige Einzelfehler werden verworfen, höchstens 5 bleiben', async () => {
    const e = [
      'sony.ps4.i.0x80990001', // gültig
      'sony.ps4.i.0x8099000G', // kein Hex
      'sony.ps4.i.0X80990001', // großes X
      'x.y.z.w', // unbekannte Liste
      'sony.ps4.i', // zu kurz
      'sony.ps4.i.0x80990001.mehr', // zu lang
      'constructor.ps4.i.-', // kein Eintrag der festen Liste (auch nicht über Prototyp)
      'datei.rest.m2.EACCES', // gültig
      'lesen.sonst.usb.-', // gültig, ohne Code
      'selbst.jb.ext.-12', // gültig
      'med.med.i.E1', // E + nur 1 Zeichen → ungültig
      'appinfo.app.ext.123456', // gültig
      'sm.ss.m2.1234567', // 7 Ziffern → ungültig
      'ss.tr.i.ENOENT', // gültig (6.)
      'sonst.tool.i.0', // gültig (7.) → schon über 5
    ].join('~');
    const { text } = await hole(neueUmgebung(), '/r?e=' + encodeURIComponent(e));
    assert.equal(versteckteFelder(text).e, 'sony.ps4.i.0x80990001~datei.rest.m2.EACCES~lesen.sonst.usb.-~selbst.jb.ext.-12~appinfo.app.ext.123456');
    assert.ok(text.includes(zeile('3.', 'Nachschauen · Sonstiges · USB-Laufwerk · ohne Code')));
    assert.ok(text.includes(zeile('5.', 'Zusatzinhalt-Reparatur · App · erweiterter Speicher · 123456')));
    assert.ok(!text.includes('<th scope="row">6.</th>'));
    assert.ok(!text.includes('ENOENT'));
  });

  it('Grenzfälle: Kleinbuchstaben, zu lang, falsches Datum, außerhalb des Bereichs → unbekannt/weg', async () => {
    const felder = {
      v: '1.0.700', b: 'R', bd: '2613031946', s: '3', m: 'cfi-7021', fw: '12.0', l: 'EN', dd: '100', jb: 'Y2JB',
      sp: '1101', sm: '-1', a: '1.2.3.4.5.6', n: '0100001', g: '100000', f: ' 1', p: '', ok: 'true', mb: '100001', t: '12.5',
    };
    const { text } = await hole(neueUmgebung(), '/r?' + new URLSearchParams(felder));
    assert.ok(text.includes('<html lang="de">'));
    assert.deepEqual(versteckteFelder(text), { l: 'de', jb: '', g: '100000', fid: versteckteFelder(text).fid });
    assert.ok(!text.includes('<th scope="row">Bau</th>'));
    // bd: 13. Monat, 32. Tag, 24 Uhr, 60. Minute sind ungültig
    for (const bd of ['2600031946', '2611321946', '2611032446', '2611031960']) {
      const r = await hole(neueUmgebung(), '/r?bd=' + bd);
      assert.ok(!r.text.includes('<th scope="row">Bau</th>'), bd);
    }
    const leer = await hole(neueUmgebung(), '/r?m=');
    assert.ok(leer.text.includes(zeile('Modell', 'unbekannt')));
  });

  it('Text mit HTML-Sonderzeichen wird beim erneuten Anzeigen maskiert', async () => {
    const env = neueUmgebung({ TURNSTILE_SECRET: TEST_SECRET_FALSCH });
    const { text } = await sende(env, { wertung: 'gut', text: `<a href="x" onclick='y'>&</a>` });
    assert.ok(text.includes('&lt;a href=&quot;x&quot; onclick=&#39;y&#39;&gt;&amp;&lt;/a&gt;</textarea>'));
    assert.ok(!text.includes("onclick='y'"));
  });
});

describe('POST /senden – Spamschutz (Turnstile)', () => {
  it('ohne Turnstile-Token: abgelehnt, nichts gespeichert, Cloudflare nicht gefragt', async () => {
    const env = neueUmgebung();
    const { status, text } = await sende(env, { wertung: 'gut', 'cf-turnstile-response': undefined });
    assert.equal(status, 400);
    assert.ok(text.includes('Die Spamschutz-Prüfung fehlt.'));
    assert.equal(env.DB.alleZeilen().length, 0);
    assert.equal(netz.turnstile.length, 0);
  });

  it('zu langes Token (> 2048 Zeichen): abgelehnt', async () => {
    const env = neueUmgebung();
    const { status } = await sende(env, { wertung: 'gut', 'cf-turnstile-response': 'x'.repeat(2049) });
    assert.equal(status, 400);
    assert.equal(env.DB.alleZeilen().length, 0);
  });

  it('ungültiges Token (Test-Secret „schlägt immer fehl“): abgelehnt, Eingaben bleiben erhalten', async () => {
    const env = neueUmgebung({ TURNSTILE_SECRET: TEST_SECRET_FALSCH });
    const { status, text } = await sende(env, { text: 'Mein Text <b>bleibt</b>', wertung: 'problem' });
    assert.equal(status, 403);
    assert.ok(text.includes('Die Spamschutz-Prüfung hat nicht geklappt.'));
    assert.equal(env.DB.alleZeilen().length, 0);
    assert.equal(netz.turnstile.length, 1);
    assert.ok(text.includes('Mein Text &lt;b&gt;bleibt&lt;/b&gt;</textarea>'));
    assert.ok(text.includes('value="problem" checked'));
  });

  it('echtes (Nicht-Dummy-)Token mit Test-Secret: abgelehnt', async () => {
    const env = neueUmgebung();
    const { status } = await sende(env, { wertung: 'gut', 'cf-turnstile-response': '0.echtes-token-vom-handy' });
    assert.equal(status, 403);
    assert.equal(env.DB.alleZeilen().length, 0);
  });

  it('schon benutztes/abgelaufenes Token („timeout-or-duplicate“): abgelehnt', async () => {
    const env = neueUmgebung({ TURNSTILE_SECRET: TEST_SECRET_DOPPELT });
    const { status, text } = await sende(env, { wertung: 'gut' });
    assert.equal(status, 403);
    assert.ok(text.includes('abgelaufen oder wurde schon benutzt'));
    assert.equal(env.DB.alleZeilen().length, 0);
  });

  it('falscher Secret-Key (Einrichtungsfehler): 503', async () => {
    const env = neueUmgebung({ TURNSTILE_SECRET: 'falscher-schluessel' });
    const { status, text } = await sende(env, { wertung: 'gut' });
    assert.equal(status, 503);
    assert.ok(text.includes('noch nicht fertig eingerichtet'));
    assert.equal(env.DB.alleZeilen().length, 0);
  });

  it('Cloudflare nicht erreichbar: 503, nichts gespeichert', async () => {
    netz = netzAttrappe({ turnstileWeg: true });
    globalThis.fetch = netz.fetchErsatz;
    const env = neueUmgebung();
    const { status, text } = await sende(env, { wertung: 'gut' });
    assert.equal(status, 503);
    assert.ok(text.includes('gerade nicht erreichbar'));
    assert.equal(env.DB.alleZeilen().length, 0);
  });
});

describe('POST /senden – Speichern mit Test-Schlüssel', () => {
  it('echter Link: speichert genau die geprüften Werte, KEINE IP, keinen User-Agent, kein Land, keine Cookies', async () => {
    const env = neueUmgebung();
    const konsole = [];
    const alt = {};
    for (const art of ['log', 'info', 'warn', 'error', 'debug']) {
      alt[art] = console[art];
      console[art] = (...x) => konsole.push([art, ...x]);
    }
    let ergebnis;
    try {
      ergebnis = await seiteUndSenden(env, ECHTER_LINK, {
        wertung: 'problem',
        text: 'Spiel blieb hängen.',
        kopf: {
          'CF-Connecting-IP': '203.0.113.77',
          'CF-Connecting-IPv6': '2001:db8::77',
          'X-Forwarded-For': '203.0.113.77, 198.51.100.9',
          'X-Real-IP': '203.0.113.77',
          'True-Client-IP': '203.0.113.77',
          'CF-IPCountry': 'DE',
          'User-Agent': 'Testhandy/9.9 (GeheimerBrowser)',
          Cookie: 'sitzung=geheim123',
        },
      });
    } finally {
      for (const art of Object.keys(alt)) console[art] = alt[art];
    }
    const { status, text, antwort } = ergebnis;
    assert.equal(status, 200);
    pruefeSicherheitsHeader(antwort);
    assert.ok(text.includes('Danke! Deine Rückmeldung ist angekommen.'));
    assert.ok(text.includes('Deine Meldungs-Nummer: 1'));

    const zeilen = env.DB.alleZeilen();
    assert.equal(zeilen.length, 1);
    const { zeit, formular_id, ...rest } = zeilen[0];
    assert.match(zeit, /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/);
    assert.match(formular_id, /^[A-Za-z0-9_-]{22}$/);
    assert.deepEqual(rest, { id: 1, wertung: 'problem', text: 'Spiel blieb hängen.', ...ERWARTETE_ZEILE });

    // Tabelle hat genau die vorgesehenen Spalten – keine für IP, Browser, Land.
    assert.deepEqual(env.DB.spalten(), [
      'id', 'zeit', 'wertung', 'text', 'v', 'b', 'bd', 's', 'm', 'fw', 'l', 'dd', 'jb', 'sp', 'sm', 'a',
      'n', 'g', 'f', 'p', 'ps', 'u', 'sb', 'mb', 't', 'ab', 'ok', 'fehler', 'formular_id',
    ]);

    // Nirgends in der ganzen Datenbank taucht IP, Browser, Land oder Cookie auf.
    const alles = JSON.stringify(env.DB.komplettAbzug());
    for (const verboten of ['203.0.113.77', '198.51.100.9', '2001:db8::77', 'Testhandy', 'GeheimerBrowser', 'geheim123', '"DE"']) {
      assert.ok(!alles.includes(verboten), `"${verboten}" darf nicht in der DB stehen`);
    }

    // An Cloudflare geht nur secret + response, keine IP (remoteip).
    assert.equal(netz.turnstile.length, 1);
    assert.equal(netz.turnstile[0].url, SITEVERIFY);
    assert.equal(netz.turnstile[0].methode, 'POST');
    assert.deepEqual(Object.keys(netz.turnstile[0].felder).sort(), ['response', 'secret']);
    assert.ok(!netz.turnstile[0].koerper.includes('203.0.113.77'));

    // Nichts geloggt.
    assert.deepEqual(konsole, []);
  });

  it('prüft die Daten serverseitig erneut (manipuliertes Formular)', async () => {
    const env = neueUmgebung();
    const { status } = await sende(env, {
      v: '1.0.7.1', b: 'x', bd: '99', s: '9', m: '<script>alert(1)</script>', fw: '12.0', l: 'fr', dd: '-2',
      jb: 'evil,lua', sp: '2', sm: '4', a: '1.2', n: '100001', g: '-1', f: 'abc', p: '5', ok: '2',
      e: 'sony.ps4.i.0x80990001~boese.ps4.i.-', wertung: 'gut',
    });
    assert.equal(status, 200);
    const [z] = env.DB.alleZeilen();
    assert.deepEqual(
      { v: z.v, b: z.b, bd: z.bd, s: z.s, m: z.m, fw: z.fw, l: z.l, dd: z.dd, jb: z.jb, sp: z.sp, sm: z.sm, a: z.a, n: z.n, g: z.g, f: z.f, p: z.p, ok: z.ok },
      { v: null, b: null, bd: null, s: null, m: null, fw: null, l: 'de', dd: null, jb: 'lua', sp: null, sm: null, a: null, n: null, g: null, f: null, p: 5, ok: null }
    );
    assert.equal(z.fehler, '[{"schritt":"sony","art":"ps4","ort":"i","code":"0x80990001"}]');
  });

  it('ohne Parameter (nur Adresse eingetippt): wird ohne Gerätedaten gespeichert', async () => {
    const env = neueUmgebung();
    const { status } = await seiteUndSenden(env, '/r', { wertung: 'gut', text: 'Alles gut' });
    assert.equal(status, 200);
    const [z] = env.DB.alleZeilen();
    assert.equal(z.wertung, 'gut');
    assert.equal(z.l, 'de');
    for (const name of ['v', 'b', 'bd', 's', 'm', 'fw', 'dd', 'jb', 'sp', 'sm', 'a', 'n', 'g', 'f', 'p', 'ps', 'u', 'sb', 'mb', 't', 'ab', 'ok', 'fehler']) {
      assert.equal(z[name], null, name);
    }
  });

  it('ohne oder mit falscher Wertung: abgelehnt, Turnstile-Token wird nicht verbraucht', async () => {
    const env = neueUmgebung();
    const ohne = await sende(env, { wertung: undefined });
    assert.equal(ohne.status, 400);
    assert.ok(ohne.text.includes('Bitte wähle 👍 oder 👎 aus.'));
    const falsch = await sende(env, { wertung: 'super' });
    assert.equal(falsch.status, 400);
    assert.equal(env.DB.alleZeilen().length, 0);
    assert.equal(netz.turnstile.length, 0);
  });

  it('englische Dankeseite bei l=en', async () => {
    const env = neueUmgebung();
    const { text } = await sende(env, { l: 'en', wertung: 'gut' });
    assert.ok(text.includes('Thank you! Your feedback has arrived.'));
    assert.ok(text.includes('<html lang="en">'));
  });

  it('falscher Content-Type (JSON) und zu großer Inhalt werden abgelehnt', async () => {
    const env = neueUmgebung();
    const json = await hole(env, '/senden', { method: 'POST', headers: { 'content-type': 'application/json' }, body: '{"wertung":"gut"}' });
    assert.equal(json.status, 400);
    const gross = await sende(env, { wertung: 'gut', text: 'x'.repeat(20000) });
    assert.equal(gross.status, 413);
    assert.equal(env.DB.alleZeilen().length, 0);
  });
});

describe('POST /senden – doppeltes Absenden', () => {
  it('dasselbe Formular zweimal (Neu laden / Zurück + Senden): nur 1 Meldung, beide Male „Danke“ mit gleicher Nummer', async () => {
    const env = neueUmgebung();
    const seite = await hole(env, ECHTER_LINK);
    const felder = { ...versteckteFelder(seite.text), wertung: 'gut', text: 'eins' };
    const a = await sende(env, felder);
    const b = await sende(env, { ...felder, text: 'zwei' });
    assert.equal(a.status, 200);
    assert.equal(b.status, 200);
    assert.ok(a.text.includes('Deine Meldungs-Nummer: 1'));
    assert.ok(b.text.includes('Deine Meldungs-Nummer: 1'));
    assert.equal(env.DB.alleZeilen().length, 1);
    assert.equal(env.DB.alleZeilen()[0].text, 'eins');
    assert.equal(netz.turnstile.length, 1, 'beim zweiten Mal wird Turnstile gar nicht mehr gefragt');
  });

  it('gleichzeitig doppelt (Doppel-Tippen): nur 1 Meldung, 1 Benachrichtigung', async () => {
    const env = neueUmgebung({ HA_WEBHOOK_URL: WEBHOOK });
    const felder = { fid: 'gleichzeitigAbgesendet', wertung: 'problem' };
    const [a, b] = await Promise.all([sende(env, felder), sende(env, felder)]);
    assert.equal(a.status, 200);
    assert.equal(b.status, 200);
    assert.equal(env.DB.alleZeilen().length, 1);
    assert.equal(netz.webhookAufrufe.length, 1);
  });

  it('zweites Formular (neu geladene Seite) ergibt eine zweite Meldung', async () => {
    const env = neueUmgebung();
    await seiteUndSenden(env, ECHTER_LINK, { wertung: 'gut' });
    await seiteUndSenden(env, ECHTER_LINK, { wertung: 'gut' });
    assert.equal(env.DB.alleZeilen().length, 2);
  });

  it('fehlende oder ungültige Formular-Nummer: nichts gespeichert, Seite mit neuer Nummer erneut gezeigt', async () => {
    const env = neueUmgebung();
    for (const fid of [undefined, '', 'kurz', 'x'.repeat(23), 'ungültig/ungültig/ung']) {
      const r = await sende(env, { fid, wertung: 'gut', text: 'bleibt' });
      assert.equal(r.status, 400, String(fid));
      assert.ok(r.text.includes('Bitte nochmal auf „Senden“ tippen.'));
      assert.match(versteckteFelder(r.text).fid, /^[A-Za-z0-9_-]{22}$/);
      assert.ok(r.text.includes('>bleibt</textarea>'));
    }
    assert.equal(env.DB.alleZeilen().length, 0);
    assert.equal(netz.turnstile.length, 0);
  });

  it('Seite verhindert Doppel-Tippen auch im Browser (Knopf wird gesperrt)', async () => {
    const { text } = await hole(neueUmgebung(), ECHTER_LINK);
    assert.ok(text.includes('if (gesendet) { e.preventDefault(); return; }'));
    assert.ok(text.includes('knopf.disabled = true;'));
  });
});

describe('POST /senden – Textlänge', () => {
  it('501 Zeichen: abgelehnt, nichts gespeichert, Turnstile nicht gefragt', async () => {
    const env = neueUmgebung();
    const { status, text } = await sende(env, { wertung: 'gut', text: 'a'.repeat(501) });
    assert.equal(status, 400);
    assert.ok(text.includes('Der Text ist zu lang.'));
    assert.equal(env.DB.alleZeilen().length, 0);
    assert.equal(netz.turnstile.length, 0);
  });

  it('genau 500 Zeichen: gespeichert', async () => {
    const env = neueUmgebung();
    const { status } = await sende(env, { wertung: 'gut', text: 'a'.repeat(500) });
    assert.equal(status, 200);
    assert.equal(env.DB.alleZeilen()[0].text.length, 500);
  });

  it('Zeilenumbruch zählt wie im Textfeld als 1 Zeichen (Browser senden \\r\\n)', async () => {
    const env = neueUmgebung();
    const { status } = await sende(env, { wertung: 'gut', text: 'a'.repeat(498) + '\r\n' + 'b' });
    assert.equal(status, 200);
    const t = env.DB.alleZeilen()[0].text;
    assert.equal(t.length, 500);
    assert.ok(!t.includes('\r'));
  });

  it('Emojis werden wie im Textfeld gezählt (250 × 😀 = 500)', async () => {
    const env = neueUmgebung();
    assert.equal((await sende(env, { wertung: 'gut', text: '😀'.repeat(250) })).status, 200);
    assert.equal((await sende(env, { wertung: 'gut', text: '😀'.repeat(251) })).status, 400);
    assert.equal(env.DB.alleZeilen().length, 1);
  });

  it('Steuer- und Richtungszeichen werden entfernt, leerer Text wird als NULL gespeichert', async () => {
    const env = neueUmgebung();
    await sende(env, { wertung: 'gut', text: '  a\u0000b\u0007c\u001b‮d⁦\n  ' });
    await sende(env, { wertung: 'gut', text: '   ' });
    const [z1, z2] = env.DB.alleZeilen();
    assert.equal(z1.text, 'abcd');
    assert.equal(z2.text, null);
  });
});

describe('POST /senden – Freitext schwärzen', () => {
  const FAELLE = [
    // [Eingabe, gespeichert]
    ['Meine IP ist 192.168.178.20.', 'Meine IP ist [entfernt].'],
    ['Router 10.0.0.1:8080 und 8.8.8.8', 'Router [entfernt]:8080 und [entfernt]'],
    ['IPv6 2001:db8::1 und fe80::1ff:fe23:4567:890a%eth0 ok', 'IPv6 [entfernt] und [entfernt] ok'],
    ['voll 2001:0db8:85a3:0000:0000:8a2e:0370:7334 und ::ffff:192.0.2.1', 'voll [entfernt] und [entfernt]'],
    ['Mail an max.mustermann+ps5@example.de bitte', 'Mail an [entfernt] bitte'],
    ['jürgen@müller-beispiel.de', '[entfernt]'],
    ['MAC AA:BB:CC:DD:EE:FF oder aa-bb-cc-dd-ee-0f oder aabb.ccdd.eeff', 'MAC [entfernt] oder [entfernt] oder [entfernt]'],
    ['Seriennummer 123456789 und 12345678901234', 'Seriennummer [entfernt] und [entfernt]'],
    ['Ruf an: +49 176 12345678 oder 0176 1234 5678 oder 089/1234567', 'Ruf an: [entfernt] oder [entfernt] oder [entfernt]'],
    // Das hier muss stehen bleiben (nützlich für die Fehlersuche):
    [
      'Version 1.0.7 auf FW 12.00, CFI-7021, Code 0x80990001, EACCES, 12345678, um 12:30:45 am 03.11.2026, 118 Einträge, -2147',
      'Version 1.0.7 auf FW 12.00, CFI-7021, Code 0x80990001, EACCES, 12345678, um 12:30:45 am 03.11.2026, 118 Einträge, -2147',
    ],
    ['0 Fehler, 3 Spiele, 0x8 ok', '0 Fehler, 3 Spiele, 0x8 ok'],
  ];

  for (const [ein, aus] of FAELLE) {
    it(`„${ein.slice(0, 50)}“`, async () => {
      const env = neueUmgebung();
      const { status } = await sende(env, { wertung: 'problem', text: ein });
      assert.equal(status, 200);
      assert.equal(env.DB.alleZeilen()[0].text, aus);
    });
  }

  it('nach dem Schwärzen höchstens 500 Zeichen, keine IP übrig', async () => {
    const env = neueUmgebung();
    const text = '1.1.1.1 '.repeat(62) + '1.1.1'; // 501 Zeichen, unten auf 500 gekürzt; geschwärzt wären es über 600
    assert.equal((await sende(env, { wertung: 'gut', text: text.slice(0, 500) })).status, 200);
    const t = env.DB.alleZeilen()[0].text;
    assert.ok(t.length <= 500);
    assert.ok(!/1\.1\.1\.1/.test(t));
    assert.ok(t.startsWith('[entfernt] [entfernt]'));
  });

  it('auch die Benachrichtigung bekommt nur den geschwärzten Text', async () => {
    const env = neueUmgebung({ HA_WEBHOOK_URL: WEBHOOK });
    await sende(env, { wertung: 'problem', text: 'schreib mir: a@b.de, IP 192.168.0.5' });
    assert.equal(netz.webhookAufrufe.length, 1);
    const s = JSON.stringify(netz.webhookAufrufe[0].json);
    assert.ok(!s.includes('a@b.de'));
    assert.ok(!s.includes('192.168.0.5'));
    assert.ok(s.includes('[entfernt]'));
  });
});

describe('Benachrichtigung an Home Assistant', () => {
  it('nach dem Speichern: POST mit titel/text/link an HA_WEBHOOK_URL (über ctx.waitUntil)', async () => {
    const env = neueUmgebung({ HA_WEBHOOK_URL: WEBHOOK });
    const ctx = neuerCtx();
    const { status } = await seiteUndSenden(env, ECHTER_LINK, { wertung: 'problem', text: 'Hängt beim Löschen.\nDanach Neustart.', ctx });
    assert.equal(status, 200);
    assert.equal(ctx.aufgaben.length, 1, 'Benachrichtigung läuft über ctx.waitUntil');
    assert.equal(netz.webhookAufrufe.length, 1);
    const aufruf = netz.webhookAufrufe[0];
    assert.equal(aufruf.url, WEBHOOK);
    assert.equal(aufruf.methode, 'POST');
    assert.equal(aufruf.kopf['content-type'], 'application/json');
    assert.deepEqual(aufruf.json, {
      titel: 'PS5 Cleanup: 👎 Problem',
      text: 'CFI-7021 · FW 12.00 · 1.0.7/Test · Fehler: Sony-Deinstallation, PS4-Spiel, intern, 0x80990001 · „Hängt beim Löschen. Danach Neustart.“',
      link: BASIS + '/uebersicht#m1',
    });
  });

  it('👍 ohne Fehler und ohne Text; langer Text wird auf 200 Zeichen gekürzt', async () => {
    const env = neueUmgebung({ HA_WEBHOOK_URL: WEBHOOK });
    await sende(env, { wertung: 'gut' });
    await sende(env, { wertung: 'gut', m: 'CFI-2016', fw: '11.60', v: '1.0.6', b: 'r', text: 'x'.repeat(300) });
    const [a, b] = netz.webhookAufrufe.map((w) => w.json);
    assert.equal(a.titel, 'PS5 Cleanup: 👍 Hat geklappt');
    assert.equal(a.text, 'Modell unbekannt · FW unbekannt · Version unbekannt/Fassung unbekannt · kein Fehler gemeldet');
    assert.equal(a.link, BASIS + '/uebersicht#m1');
    assert.equal(b.text, `CFI-2016 · FW 11.60 · 1.0.6/normal · kein Fehler gemeldet · „${'x'.repeat(199)}…“`);
    assert.equal(b.link, BASIS + '/uebersicht#m2');
  });

  it('ohne HA_WEBHOOK_URL (oder nicht https): keine Benachrichtigung, Zähler bleibt leer', async () => {
    for (const ziel of [undefined, '', 'http://192.168.0.10:8123/api/webhook/x', 'kein-link']) {
      netz = netzAttrappe();
      globalThis.fetch = netz.fetchErsatz;
      const env = neueUmgebung({ HA_WEBHOOK_URL: ziel });
      const { status } = await sende(env, { wertung: 'problem' });
      assert.equal(status, 200);
      assert.equal(env.DB.alleZeilen().length, 1);
      assert.equal(netz.webhookAufrufe.length, 0, String(ziel));
      assert.deepEqual(env.DB.zaehler(), []);
    }
  });

  it('Tageslimit: höchstens 20 Benachrichtigungen pro UTC-Tag, alle Meldungen werden trotzdem gespeichert', async () => {
    const env = neueUmgebung({ HA_WEBHOOK_URL: WEBHOOK });
    for (let i = 0; i < 25; i++) assert.equal((await sende(env, { wertung: i % 2 ? 'gut' : 'problem' })).status, 200);
    assert.equal(env.DB.alleZeilen().length, 25);
    assert.equal(netz.webhookAufrufe.length, 20);
    const heute = new Date().toISOString().slice(0, 10);
    assert.deepEqual(env.DB.zaehler(), [{ tag: heute, anzahl: 20 }]);
    assert.equal(netz.webhookAufrufe[19].json.link, BASIS + '/uebersicht#m20');
  });

  it('Limit gilt je Tag: ein voller Vortag bremst heute nicht; alte Zählerzeilen werden aufgeräumt', async () => {
    const env = neueUmgebung({ HA_WEBHOOK_URL: WEBHOOK });
    const tag = (versatz) => new Date(Date.now() + versatz * 86400000).toISOString().slice(0, 10);
    env.DB.db.prepare('INSERT INTO benachrichtigungen (tag, anzahl) VALUES (?, 20), (?, 20)').run(tag(-1), tag(-30));
    await sende(env, { wertung: 'problem' });
    assert.equal(netz.webhookAufrufe.length, 1);
    assert.deepEqual(env.DB.zaehler(), [{ tag: tag(-1), anzahl: 20 }, { tag: tag(0), anzahl: 1 }]);
  });

  it('Webhook-Fehler (500, kein Netz) verhindern das Speichern nicht', async () => {
    for (const art of ['fehler500', 'netzweg']) {
      netz = netzAttrappe({ webhook: art });
      globalThis.fetch = netz.fetchErsatz;
      const env = neueUmgebung({ HA_WEBHOOK_URL: WEBHOOK });
      const { status, text } = await sende(env, { wertung: 'problem', text: 'trotzdem da' });
      assert.equal(status, 200, art);
      assert.ok(text.includes('Danke!'));
      assert.equal(netz.webhookAufrufe.length, 1);
      assert.equal(env.DB.alleZeilen()[0].text, 'trotzdem da');
    }
  });

  it('hängender Webhook hält die Antwort nicht auf (läuft im Hintergrund)', async () => {
    const env = neueUmgebung({ HA_WEBHOOK_URL: WEBHOOK });
    globalThis.fetch = async (eingabe, init) => {
      const url = typeof eingabe === 'string' ? eingabe : eingabe.url;
      if (url === WEBHOOK) return new Promise(() => {}); // antwortet nie
      return netz.fetchErsatz(eingabe, init);
    };
    const ctx = neuerCtx();
    const antwort = await worker.fetch(
      new Request(BASIS + '/senden', {
        method: 'POST',
        headers: { 'content-type': 'application/x-www-form-urlencoded' },
        body: formular({ fid: neueFid(), wertung: 'problem', 'cf-turnstile-response': DUMMY_TOKEN }).toString(),
      }),
      env,
      ctx
    );
    assert.equal(antwort.status, 200);
    assert.equal(env.DB.alleZeilen().length, 1);
    assert.equal(ctx.aufgaben.length, 1);
  });

  it('kaputte Zähler-Tabelle: keine Benachrichtigung, Meldung trotzdem gespeichert', async () => {
    const env = neueUmgebung({ HA_WEBHOOK_URL: WEBHOOK });
    env.DB.db.exec('DROP TABLE benachrichtigungen');
    const { status } = await sende(env, { wertung: 'problem' });
    assert.equal(status, 200);
    assert.equal(env.DB.alleZeilen().length, 1);
    assert.equal(netz.webhookAufrufe.length, 0);
  });
});

describe('GET /uebersicht – Passwortschutz und Inhalt', () => {
  it('ohne Anmeldung: 401 mit Basic-Auth-Abfrage', async () => {
    const { status, antwort, text } = await hole(neueUmgebung(), '/uebersicht');
    assert.equal(status, 401);
    assert.match(antwort.headers.get('www-authenticate'), /^Basic realm=/);
    assert.ok(!text.includes('Meldungen ·'));
    pruefeSicherheitsHeader(antwort);
  });

  it('falsches Passwort: 401', async () => {
    const env = neueUmgebung();
    for (const pw of ['falsch', PASSWORT + 'x', PASSWORT.slice(0, -1), '', PASSWORT.toUpperCase()]) {
      assert.equal((await hole(env, '/uebersicht', auth(pw))).status, 401, `Passwort "${pw}"`);
    }
    assert.equal((await hole(env, '/uebersicht', { headers: { Authorization: 'Basic %%%' } })).status, 401);
    assert.equal((await hole(env, '/uebersicht', { headers: { Authorization: 'Bearer ' + PASSWORT } })).status, 401);
    assert.equal((await hole(env, '/uebersicht', { headers: { Authorization: 'Basic ' + btoa(PASSWORT) } })).status, 401);
  });

  it('richtiges Passwort (Benutzername egal): 200', async () => {
    const env = neueUmgebung();
    for (const benutzer of ['admin', 'egal', '']) {
      const { status, text } = await hole(env, '/uebersicht', auth(PASSWORT, benutzer));
      assert.equal(status, 200);
      assert.ok(text.includes('0 Meldungen · 👍 0 · 👎 0'));
    }
  });

  it('Passwort mit Umlauten funktioniert', async () => {
    const env = neueUmgebung({ UEBERSICHT_PASSWORT: 'Grüße-aus-Bayern' });
    assert.equal((await hole(env, '/uebersicht', auth('Grüße-aus-Bayern'))).status, 200);
  });

  it('ohne gesetztes Passwort immer gesperrt', async () => {
    for (const env of [neueUmgebung({ UEBERSICHT_PASSWORT: undefined }), neueUmgebung({ UEBERSICHT_PASSWORT: '' })]) {
      for (const pw of ['', 'undefined', 'irgendwas']) {
        assert.equal((await hole(env, '/uebersicht', auth(pw))).status, 401);
        assert.equal((await hole(env, '/export.json', auth(pw))).status, 401);
      }
    }
  });

  it('zeigt Statistik, Meldungen mit Anker und Klartext, Texte escaped', async () => {
    const env = neueUmgebung();
    await seiteUndSenden(env, ECHTER_LINK, { wertung: 'problem', text: '<img src=x onerror=alert(1)> & "Hallo"' });
    await seiteUndSenden(env, ECHTER_LINK, { wertung: 'gut' });
    await seiteUndSenden(env, ECHTER_LINK.replace('b=t', 'b=r').replace('m=CFI-7021', 'm=CFI-2016').replace('fw=12.00', 'fw=11.60'), { wertung: 'gut' });
    await seiteUndSenden(env, '/r', { wertung: 'problem', text: 'ohne Daten' });
    const { status, text } = await hole(env, '/uebersicht', auth());
    assert.equal(status, 200);
    assert.ok(text.includes('4 Meldungen · 👍 2 · 👎 2'));
    // je Modell / je Firmware / je Version+Fassung
    assert.ok(text.includes('<tr><td>CFI-7021</td><td>2</td><td>1</td><td>1</td></tr>'));
    assert.ok(text.includes('<tr><td>CFI-2016</td><td>1</td><td>1</td><td>0</td></tr>'));
    assert.ok(text.includes('<tr><td>unbekannt</td><td>1</td><td>0</td><td>1</td></tr>'));
    assert.ok(text.includes('<tr><td>12.00</td><td>2</td><td>1</td><td>1</td></tr>'));
    assert.ok(text.includes('<tr><td>11.60</td><td>1</td><td>1</td><td>0</td></tr>'));
    assert.ok(text.includes('<tr><td>1.0.7 · Test-Fassung</td><td>2</td><td>1</td><td>1</td></tr>'));
    assert.ok(text.includes('<tr><td>1.0.7 · normale Fassung</td><td>1</td><td>1</td><td>0</td></tr>'));
    // häufigste Fehler schritt.art.code (3 Meldungen mit je 2 Fehlern)
    assert.ok(text.includes('<code>sony.ps4.0x80990001</code><br><span class="klein">Sony-Deinstallation · PS4-Spiel · 0x80990001</span></td><td>3</td><td>3</td>'));
    assert.ok(text.includes('<code>datei.rest.EACCES</code>'));
    // Meldungen: Anker, Klartext, neueste zuerst, Text escaped
    for (const id of [1, 2, 3, 4]) assert.ok(text.includes(`id="m${id}"`), `Anker m${id}`);
    assert.ok(text.indexOf('id="m4"') < text.indexOf('id="m1"'));
    assert.ok(text.includes('<b>Konsole:</b> Modell: CFI-7021 · Firmware: 12.00 · Laufwerks-Kopplung: online gekoppelt (Wert 1)'));
    assert.ok(text.includes('<b>Umgebung:</b> Jailbreak-Einstiege: YouTube (Y2JB), Browserspeicher · Speicher: M.2 ja, erweiterter Speicher ja, USB-Laufwerk nein · ShadowMount: ShadowMount-Spiele vorhanden'));
    assert.ok(text.includes('<b>Fehler:</b> 1. Sony-Deinstallation · PS4-Spiel · intern · 0x80990001 · 2. Datei/Ordner löschen · Reste · M.2 · EACCES'));
    assert.ok(text.includes('Bau: 03.11.26 19:46'));
    assert.ok(text.includes('&lt;img src=x onerror=alert(1)&gt; &amp; &quot;Hallo&quot;'));
    assert.ok(!text.includes('<img'));
    assert.ok(text.includes('href="/export.json" download'));
    assert.ok(!text.includes('formular'), 'Formular-Nummer wird nicht angezeigt');
    assert.ok(!/<script\b/.test(text));
  });

  it('Uhrzeit in deutscher Zeit stimmt (auch an den Sommerzeit-Grenzen), verglichen mit Intl', async () => {
    const env = neueUmgebung();
    const zeiten = [];
    for (let i = 0; i < 192; i++) zeiten.push(new Date(Date.UTC(2026, 0, 1) + i * 7777777 * 1000 / 3).toISOString().replace(/\.\d{3}Z$/, 'Z'));
    zeiten.push(
      '2026-03-29T00:59:00Z', '2026-03-29T01:00:00Z', '2026-10-25T00:59:00Z', '2026-10-25T01:00:00Z',
      '2026-12-31T23:30:00Z', '2027-03-28T01:00:00Z', '2028-02-29T12:00:00Z', '2026-11-03T18:47:12Z'
    );
    const einfuegen = env.DB.db.prepare("INSERT INTO meldungen (zeit, wertung, l, formular_id) VALUES (?, 'gut', 'de', ?)");
    zeiten.forEach((z, i) => einfuegen.run(z, String(i).padStart(22, 'z')));
    const { text } = await hole(env, '/uebersicht', auth());
    const angezeigt = new Map([...text.matchAll(/id="m(\d+)">\n<p class="kopf"><a href="#m\d+">#\d+<\/a> · ([^·]+) · /g)].map((m) => [Number(m[1]), m[2].trim()]));
    const intl = new Intl.DateTimeFormat('de-DE', { timeZone: 'Europe/Berlin', day: '2-digit', month: '2-digit', year: 'numeric', hour: '2-digit', minute: '2-digit' });
    let geprueft = 0;
    zeiten.forEach((z, i) => {
      const id = i + 1;
      if (!angezeigt.has(id)) return; // nur die letzten 200 werden angezeigt
      assert.equal(angezeigt.get(id), `${intl.format(new Date(z)).replace(',', '')} Uhr (deutsche Zeit)`, z);
      geprueft++;
    });
    assert.equal(geprueft, 200);
    assert.equal(angezeigt.get(200), '03.11.2026 19:47 Uhr (deutsche Zeit)');
    assert.equal(angezeigt.get(194), '29.03.2026 03:00 Uhr (deutsche Zeit)');
    assert.equal(angezeigt.get(196), '25.10.2026 02:00 Uhr (deutsche Zeit)');
  });

  it('zeigt höchstens die letzten 200 Meldungen, neueste zuerst', async () => {
    const env = neueUmgebung();
    const einfuegen = env.DB.db.prepare(
      "INSERT INTO meldungen (zeit, wertung, l, formular_id) VALUES ('2026-11-03T12:00:00Z', 'gut', 'de', ?)"
    );
    for (let i = 0; i < 205; i++) einfuegen.run(String(i).padStart(22, 'f'));
    const { text } = await hole(env, '/uebersicht', auth());
    assert.ok(text.includes('205 Meldungen'));
    assert.ok(text.includes('Letzte 200 Meldungen'));
    const nummern = [...text.matchAll(/<article class="meldung-karte [a-z]+" id="m(\d+)">/g)].map((m) => Number(m[1]));
    assert.equal(nummern.length, 200);
    assert.equal(nummern[0], 205);
    assert.equal(nummern[199], 6);
  });
});

describe('GET /export.json', () => {
  it('401 ohne und mit falschem Passwort', async () => {
    const env = neueUmgebung();
    const ohne = await hole(env, '/export.json');
    assert.equal(ohne.status, 401);
    assert.match(ohne.antwort.headers.get('www-authenticate'), /^Basic realm=/);
    assert.ok(!ohne.text.includes('meldungen'));
    assert.equal((await hole(env, '/export.json', auth('falsch'))).status, 401);
  });

  it('200 mit richtigem Passwort: alle Meldungen als JSON zum Auswerten', async () => {
    const env = neueUmgebung();
    await seiteUndSenden(env, ECHTER_LINK, { wertung: 'problem', text: 'Text' });
    await seiteUndSenden(env, '/r', { wertung: 'gut' });
    const { status, text, antwort } = await hole(env, '/export.json', auth());
    assert.equal(status, 200);
    assert.match(antwort.headers.get('content-type'), /^application\/json; charset=utf-8/);
    assert.match(antwort.headers.get('content-disposition'), /^attachment; filename="ps5cleanup-rueckmeldungen-\d{4}-\d{2}-\d{2}\.json"$/);
    pruefeSicherheitsHeader(antwort);
    const daten = JSON.parse(text);
    assert.equal(daten.anzahl, 2);
    assert.equal(daten.meldungen.length, 2);
    const { zeit, ...erste } = daten.meldungen[0];
    assert.match(zeit, /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/);
    assert.deepEqual(erste, {
      id: 1, wertung: 'problem', text: 'Text', v: '1.0.7', b: 't', bd: '2611031946', s: 2, m: 'CFI-7021', fw: '12.00', l: 'de', dd: 1,
      jb: ['y2jb', 'browser'],
      sp: { m2: true, ext: true, usb: false },
      sm: 1,
      a: { spiele: 12, apps: 30, spielstaende: 5, trophaeen: 8, medien: 40, sonstiges: 20, jailbreak: 3 },
      n: 118, g: 4, f: 1, p: 0, ps: 0, u: 0, sb: 0, mb: 38, t: 12, ab: 0, ok: 0,
      e: [
        { schritt: 'sony', art: 'ps4', ort: 'i', code: '0x80990001' },
        { schritt: 'datei', art: 'rest', ort: 'm2', code: 'EACCES' },
      ],
    });
    assert.equal(daten.meldungen[1].m, null);
    assert.deepEqual(daten.meldungen[1].e, []);
    assert.ok(!text.includes('formular'), 'keine Formular-Nummer im Export');
  });

  it('viele Meldungen (in Stücken zu 500) und Sonderzeichen im Text kommen vollständig und gültig an', async () => {
    const env = neueUmgebung();
    const sonder = 'Zeile 1\nZeile 2 "Zitat" \\ Backslash </script> 😀 ü';
    await sende(env, { wertung: 'problem', text: sonder, jb: '', sp: '001', a: '0.0.0.0.0.0.100000' });
    const einfuegen = env.DB.db.prepare("INSERT INTO meldungen (zeit, wertung, l, formular_id) VALUES ('2026-11-03T12:00:00Z', 'gut', 'en', ?)");
    for (let i = 0; i < 1202; i++) einfuegen.run(String(i).padStart(22, 'e'));
    const { text, status } = await hole(env, '/export.json', auth());
    assert.equal(status, 200);
    const daten = JSON.parse(text);
    assert.equal(daten.anzahl, 1203);
    assert.deepEqual(daten.meldungen.map((m) => m.id), Array.from({ length: 1203 }, (_, i) => i + 1));
    assert.equal(daten.meldungen[0].text, sonder);
    assert.deepEqual(daten.meldungen[0].jb, []);
    assert.deepEqual(daten.meldungen[0].sp, { m2: false, ext: false, usb: true });
    assert.equal(daten.meldungen[0].a.jailbreak, 100000);
    assert.equal(daten.meldungen[1202].l, 'en');
    // leere Datenbank
    const leer = JSON.parse((await hole(neueUmgebung(), '/export.json', auth())).text);
    assert.equal(leer.anzahl, 0);
    assert.deepEqual(leer.meldungen, []);
  });
});

describe('GET /datenschutz', () => {
  it('Deutsch: was gespeichert wird, wofür, keine IP, Cloudflare, Löschen über GitHub', async () => {
    const { status, text, antwort } = await hole(neueUmgebung(), '/datenschutz');
    assert.equal(status, 200);
    pruefeSicherheitsHeader(antwort);
    assert.ok(text.includes('<html lang="de">'));
    assert.ok(text.includes('Was gespeichert wird'));
    assert.ok(text.includes('Keine IP-Adresse'));
    assert.ok(text.includes('Kompatibilität von PS5 Cleanup'));
    assert.ok(text.includes('href="https://www.cloudflare.com/de-de/privacypolicy/"'));
    assert.ok(text.includes('href="https://github.com/xXBneutzerNameXx/PS5-Cleanup/issues"'));
    assert.ok(!/<script\b/.test(text));
    assert.ok(!/@[a-z0-9-]+\.[a-z]{2,}/i.test(text.replace(/<[^>]+>/g, '')), 'keine E-Mail-Adresse');
  });

  it('Englisch mit l=en', async () => {
    const { text } = await hole(neueUmgebung(), '/datenschutz?l=en');
    assert.ok(text.includes('<html lang="en">'));
    assert.ok(text.includes('What is stored'));
    assert.ok(text.includes('No IP address'));
    assert.ok(text.includes('href="https://www.cloudflare.com/privacypolicy/"'));
  });
});

describe('Alles andere → 404, Einrichtung unvollständig → 503', () => {
  it('unbekannte Wege und falsche Methoden geben 404', async () => {
    const env = neueUmgebung();
    const faelle = [
      ['GET', '/'], ['GET', '/r/'], ['GET', '/R'], ['GET', '/admin'], ['GET', '/favicon.ico'], ['GET', '/senden'],
      ['GET', '/export'], ['GET', '/uebersicht/'], ['POST', '/r'], ['PUT', '/uebersicht'], ['DELETE', '/datenschutz'],
      ['OPTIONS', '/senden'], ['POST', '/export.json'],
    ];
    for (const [methode, pfad] of faelle) {
      const { status, antwort } = await hole(env, pfad, { method: methode });
      assert.equal(status, 404, `${methode} ${pfad}`);
      pruefeSicherheitsHeader(antwort);
    }
    assert.equal(env.DB.alleZeilen().length, 0);
  });

  it('ohne Turnstile-Schlüssel: Seite zeigt Hinweis statt Formular, Senden wird abgelehnt', async () => {
    const env = neueUmgebung({ TURNSTILE_SITEKEY: undefined, TURNSTILE_SECRET: undefined });
    const seite = await hole(env, ECHTER_LINK);
    assert.equal(seite.status, 503);
    assert.ok(seite.text.includes('noch nicht fertig eingerichtet'));
    assert.ok(!seite.text.includes('<form'));
    assert.ok(!/<script\b/.test(seite.text));
    assert.ok(!/script-src/.test(seite.antwort.headers.get('content-security-policy')));
    const senden = await sende(env, { wertung: 'gut' });
    assert.equal(senden.status, 503);
    assert.equal(env.DB.alleZeilen().length, 0);
    assert.equal(netz.turnstile.length, 0);
  });

  it('Datenbank-Ausfall: 500 mit festem Text, nichts über die Anfrage geloggt', async () => {
    const env = neueUmgebung();
    env.DB.kaputt = true;
    const log = [];
    const alt = console.error;
    console.error = (...x) => log.push(x.join(' '));
    try {
      const { status, text } = await sende(env, { wertung: 'gut', text: 'geheimer Text' }, { kopf: { 'CF-Connecting-IP': '203.0.113.5' } });
      assert.equal(status, 500);
      assert.ok(text.includes('Interner Fehler'));
    } finally {
      console.error = alt;
    }
    assert.deepEqual(log, ['Interner Fehler im Rueckmeldungs-Worker']);
  });
});

describe('GET /r – Hinweis auf neuere Version', () => {
  const RELEASES = 'https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases/latest';
  const mitVersion = (v) => ECHTER_LINK.replace('v=1.0.7', `v=${v}`);
  const hatHinweis = (text) => text.includes('<section class="neu">');

  it('ältere Version im Link: Hinweis mit Link zu GitHub, Formular bleibt', async () => {
    const { status, text } = await hole(neueUmgebung({ NEUESTE_VERSION: '1.0.8' }), ECHTER_LINK);
    assert.equal(status, 200);
    assert.ok(hatHinweis(text));
    assert.ok(text.includes('<strong>Es gibt eine neuere Version: 1.0.8</strong>'));
    assert.ok(text.includes('Du nutzt 1.0.7. Deine Rückmeldung kannst du trotzdem schicken.'));
    assert.ok(text.includes(`<a class="knopf zweit" href="${RELEASES}" target="_blank" rel="noopener noreferrer">Zur neuen Version (GitHub)</a>`));
    assert.ok(text.indexOf('<section class="neu">') < text.indexOf('<section class="karte">'));
    assert.ok(text.includes('<form id="formular"'));
  });

  it('gleiche oder neuere Version im Link: kein Hinweis', async () => {
    const faelle = [[ECHTER_LINK, '1.0.7'], [mitVersion('1.0.8'), '1.0.7'], [mitVersion('1.1.0'), '1.0.9'], [mitVersion('2.0.0'), '1.9.9']];
    for (const [link, neueste] of faelle) {
      const { status, text } = await hole(neueUmgebung({ NEUESTE_VERSION: neueste }), link);
      assert.equal(status, 200);
      assert.ok(!hatHinweis(text), `${link} gegen ${neueste}`);
    }
  });

  it('Teile werden als Zahlen verglichen (1.0.9 ist älter als 1.0.10)', async () => {
    const alt = await hole(neueUmgebung({ NEUESTE_VERSION: '1.0.10' }), mitVersion('1.0.9'));
    assert.ok(alt.text.includes('Es gibt eine neuere Version: 1.0.10'));
    const neu = await hole(neueUmgebung({ NEUESTE_VERSION: '1.0.9' }), mitVersion('1.0.10'));
    assert.ok(!hatHinweis(neu.text));
  });

  it('Einstellung fehlt oder ist ungültig: kein Hinweis, Seite geht trotzdem', async () => {
    for (const neueste of [undefined, '', ' ', '1.0', '1.0.8.1', 'v1.0.8', 'abc', '1.0.8<b>', 108, null]) {
      const env = neueUmgebung();
      if (neueste !== undefined) env.NEUESTE_VERSION = neueste;
      const { status, text } = await hole(env, mitVersion('1.0.6'));
      assert.equal(status, 200, String(neueste));
      assert.ok(!hatHinweis(text), String(neueste));
    }
  });

  it('Leerzeichen um die Einstellung stören nicht', async () => {
    const { text } = await hole(neueUmgebung({ NEUESTE_VERSION: ' 1.0.8\n' }), ECHTER_LINK);
    assert.ok(text.includes('Es gibt eine neuere Version: 1.0.8'));
  });

  it('keine oder ungültige Version im Link: kein Hinweis', async () => {
    const env = neueUmgebung({ NEUESTE_VERSION: '1.0.8' });
    const links = ['/r', '/r?l=en', ECHTER_LINK.replace('v=1.0.7&', ''), mitVersion('1.0'), mitVersion('abc'), mitVersion('1.0.6%3Cb%3E')];
    for (const link of links) {
      const { status, text } = await hole(env, link);
      assert.equal(status, 200, link);
      assert.ok(!hatHinweis(text), link);
    }
  });

  it('Englisch mit l=en', async () => {
    const { text } = await hole(neueUmgebung({ NEUESTE_VERSION: '1.0.8' }), ECHTER_LINK.replace('l=de', 'l=en'));
    assert.ok(text.includes('<strong>A newer version is available: 1.0.8</strong>'));
    assert.ok(text.includes('You are using 1.0.7. You can still send your feedback.'));
    assert.ok(text.includes('>Get the new version (GitHub)</a>'));
  });

  it('bleibt da, wenn das Formular nach einem Fehler neu gezeigt wird', async () => {
    const env = neueUmgebung({ NEUESTE_VERSION: '1.0.8' });
    const seite = await hole(env, ECHTER_LINK);
    const { status, text } = await sende(env, { ...versteckteFelder(seite.text) });
    assert.equal(status, 400);
    assert.ok(text.includes('Es gibt eine neuere Version: 1.0.8'));
    assert.equal(env.DB.alleZeilen().length, 0);
  });

  it('Seite ohne fertige Einrichtung (503) zeigt den Hinweis trotzdem', async () => {
    const env = neueUmgebung({ NEUESTE_VERSION: '1.0.8' });
    delete env.TURNSTILE_SECRET;
    const { status, text } = await hole(env, ECHTER_LINK);
    assert.equal(status, 503);
    assert.ok(hatHinweis(text));
  });
});
