// PS5 Cleanup – Rückmeldungs-Seite (Cloudflare Worker, eine einzige Datei, keine Abhängigkeiten)
//
// Wege:
//   GET  /r             Rückmeldungs-Seite: zeigt die Daten aus dem QR-Code-Link in Klartext
//   POST /senden        Rückmeldung prüfen (Turnstile), Text schwärzen, in D1 speichern
//   GET  /uebersicht    Übersicht für den Entwickler (HTTP Basic Auth)
//   GET  /export.json   alle Meldungen als JSON (gleiche Basic Auth)
//   GET  /datenschutz   Datenschutz-Hinweis (DE/EN)
//   alles andere        404
//
// Grundsatz: Gespeichert wird nur, was auf der Seite steht, plus 👍/👎 und Text
// (dazu Uhrzeit und eine zufällige Formular-Nummer gegen doppeltes Absenden).
// Keine IP, kein User-Agent, kein Land, keine Cookies – weder gespeichert noch geloggt.

const MAX_TEXT = 500; // Zeichen im Textfeld (wie maxlength im Browser gezählt)
const MAX_ZAHL = 100000;
const MAX_FEHLER = 5;
const MAX_TOKEN = 2048;
const MAX_KOERPER = 16384; // Bytes für POST /senden
const MAX_ROH = 400; // längster erlaubter Rohwert eines Link-Parameters
const TAGESLIMIT = 20; // höchstens so viele Home-Assistant-Benachrichtigungen pro UTC-Tag
const WEBHOOK_TEXT = 200; // Freitext in der Benachrichtigung, gekürzt auf so viele Zeichen
const LETZTE = 200; // so viele Meldungen zeigt /uebersicht
const SITEVERIFY_URL = 'https://challenges.cloudflare.com/turnstile/v0/siteverify';
const TURNSTILE_JS = 'https://challenges.cloudflare.com/turnstile/v0/api.js';
const TURNSTILE_HOST = 'https://challenges.cloudflare.com';
const GITHUB_ISSUES = 'https://github.com/xXBneutzerNameXx/PS5-Cleanup/issues';
const CF_DATENSCHUTZ = { de: 'https://www.cloudflare.com/de-de/privacypolicy/', en: 'https://www.cloudflare.com/privacypolicy/' };
const CF_TURNSTILE_DATENSCHUTZ = 'https://www.cloudflare.com/turnstile-privacy-policy/';
const ENTFERNT = '[entfernt]';

// ---------------------------------------------------------------------------
// Einstieg
// ---------------------------------------------------------------------------

export default {
  async fetch(request, env, ctx) {
    let antwort;
    try {
      antwort = await verteile(request, env || {}, ctx);
    } catch (_) {
      // Bewusst nur ein fester Text: keine Anfrage-Daten (IP, Header, Text) ins Log.
      console.error('Interner Fehler im Rueckmeldungs-Worker');
      antwort = htmlAntwort(fehlerSeite('de', 'Interner Fehler', 'Bitte später nochmal versuchen.'), 500);
    }
    if (request.method === 'HEAD') {
      return new Response(null, { status: antwort.status, headers: antwort.headers });
    }
    return antwort;
  },
};

async function verteile(request, env, ctx) {
  const url = new URL(request.url);
  const pfad = url.pathname;
  const methode = request.method;
  const lesen = methode === 'GET' || methode === 'HEAD';

  if (pfad === '/r' && lesen) return seiteRueckmeldung(url, env);
  if (pfad === '/senden' && methode === 'POST') return senden(request, env, ctx, url);
  if (pfad === '/uebersicht' && lesen) return uebersicht(request, env);
  if (pfad === '/export.json' && lesen) return exportJson(request, env);
  if (pfad === '/datenschutz' && lesen) return htmlAntwort(datenschutzSeite(spracheAus(url.searchParams.get('l'))), 200);
  const l = spracheAus(url.searchParams.get('l'));
  return htmlAntwort(fehlerSeite(l, '404', T[l].nichtGefunden), 404);
}

// ---------------------------------------------------------------------------
// Feste Listen (alles, was nicht hier steht, wird verworfen)
// ---------------------------------------------------------------------------

const JB = {
  y2jb: ['YouTube (Y2JB)', 'YouTube (Y2JB)'],
  netflix: ["Netflix ('N Hack)", "Netflix ('N Hack)"],
  lua: ['Lua-Spiel', 'Lua game'],
  ps2: ['PS2-Spielstand', 'PS2 save'],
  renpy: ["Ren'Py", "Ren'Py"],
  webkit: ['WebKit', 'WebKit'],
  bd: ['Blu-ray-Disc', 'Blu-ray disc'],
  browser: ['Browserspeicher', 'browser storage'],
};
const SCHRITT = {
  sony: ['Sony-Deinstallation', 'Sony uninstall'],
  datei: ['Datei/Ordner löschen', 'deleting file/folder'],
  ss: ['Spielstand-Liste', 'save data list'],
  med: ['Mediengalerie', 'media gallery'],
  sm: ['ShadowMount-Einbindung', 'ShadowMount mount'],
  appinfo: ['Zusatzinhalt-Reparatur', 'add-on repair'],
  lesen: ['Nachschauen', 'scanning'],
  selbst: ['Selbstentfernung', 'self-removal'],
  sonst: ['Sonstiges', 'other'],
};
const ART = {
  ps4: ['PS4-Spiel', 'PS4 game'],
  ps5: ['PS5-Spiel', 'PS5 game'],
  app: ['App', 'app'],
  tool: ['Tool', 'tool'],
  ss: ['Spielstand', 'save data'],
  tr: ['Trophäen', 'trophies'],
  med: ['Bilder & Videos', 'pictures & videos'],
  rest: ['Reste', 'leftovers'],
  jb: ['Jailbreak', 'jailbreak'],
  sonst: ['Sonstiges', 'other'],
};
const ORT = {
  i: ['intern', 'internal'],
  m2: ['M.2', 'M.2'],
  ext: ['erweiterter Speicher', 'extended storage'],
  usb: ['USB-Laufwerk', 'USB drive'],
};
const BEREICHE = [
  ['spiele', 'Spiele', 'games'],
  ['apps', 'Apps', 'apps'],
  ['spielstaende', 'Spielstände', 'saves'],
  ['trophaeen', 'Trophäen', 'trophies'],
  ['medien', 'Bilder & Videos', 'pictures & videos'],
  ['sonstiges', 'Sonstiges', 'other'],
  ['jailbreak', 'Jailbreak', 'jailbreak'],
];
const SPEICHER = [
  ['m2', 'M.2', 'M.2'],
  ['ext', 'erweiterter Speicher', 'extended storage'],
  ['usb', 'USB-Laufwerk', 'USB drive'],
];

const idx = (l) => (l === 'en' ? 1 : 0);
const hat = (liste, schluessel) => Object.hasOwn(liste, schluessel);

// ---------------------------------------------------------------------------
// Daten aus Link bzw. Formular prüfen (immer serverseitig, für GET und POST)
// ---------------------------------------------------------------------------

const RE_VERSION = /^[0-9]{1,2}\.[0-9]{1,2}\.[0-9]{1,2}$/;
const RE_BAU = /^[0-9]{10}$/;
const RE_MODELL = /^[A-Z0-9-]{0,16}$/; // leer = unbekannt
const RE_FIRMWARE = /^[0-9]{1,2}\.[0-9]{2}$/;
const RE_ZAHL = /^[0-9]{1,6}$/;
const RE_KOPPLUNG = /^(?:-1|[0-9]{1,2})$/;
const RE_SPEICHER = /^[01]{3}$/;
const RE_BEREICHE = /^[0-9]{1,6}(?:\.[0-9]{1,6}){6}$/;
const RE_CODE = /^(?:0x[0-9a-f]{8}|E[A-Z0-9]{2,15}|-?[0-9]{1,6}|-)$/;
const RE_FID = /^[A-Za-z0-9_-]{22}$/;

// Alle Link-Parameter in der Reihenfolge, in der die App sie schreibt.
const PARAMETER = ['v', 'b', 'bd', 's', 'm', 'fw', 'l', 'dd', 'jb', 'sp', 'sm', 'a', 'n', 'g', 'f', 'p', 'ps', 'u', 'sb', 'mb', 't', 'ab', 'ok', 'e'];
const ZAHLEN = ['n', 'g', 'f', 'p', 'ps', 'u', 'mb', 't'];
const SCHALTER = ['sb', 'ab', 'ok']; // nur 0 oder 1

function spracheAus(wert) {
  return wert === 'en' ? 'en' : 'de';
}

// holen(name) liefert den rohen Wert (String) oder null.
// Ergebnis: nur geprüfte Werte, alles Ungültige ist null (bzw. bei e: weggelassen).
function pruefeDaten(holen) {
  const roh = (name) => {
    const x = holen(name);
    return typeof x === 'string' && x.length <= MAX_ROH ? x : null;
  };
  const passt = (name, muster) => {
    const x = roh(name);
    return x !== null && muster.test(x) ? x : null;
  };
  const zahl = (name) => {
    const x = passt(name, RE_ZAHL);
    if (x === null) return null;
    const z = Number(x);
    return z <= MAX_ZAHL ? z : null;
  };
  const auswahl = (name, erlaubt) => {
    const x = roh(name);
    return x !== null && erlaubt.includes(x) ? Number(x) : null;
  };

  const d = {
    v: passt('v', RE_VERSION),
    b: ((x) => (x === 'r' || x === 't' ? x : null))(roh('b')),
    bd: pruefeBau(roh('bd')),
    s: auswahl('s', ['0', '1', '2']),
    m: ((x) => (x === null || x === '' ? null : x))(passt('m', RE_MODELL)),
    fw: passt('fw', RE_FIRMWARE),
    l: spracheAus(roh('l')),
    dd: ((x) => (x === null ? null : Number(x)))(passt('dd', RE_KOPPLUNG)),
    jb: pruefeJailbreak(roh('jb')),
    sp: passt('sp', RE_SPEICHER),
    sm: auswahl('sm', ['0', '1', '2', '3']),
    a: pruefeBereiche(roh('a')),
    e: pruefeFehler(roh('e')),
  };
  for (const name of ZAHLEN) d[name] = zahl(name);
  for (const name of SCHALTER) d[name] = auswahl(name, ['0', '1']);
  return d;
}

// Bau-Stempel JJMMTTHHMM, zusätzlich auf gültiges Datum/Uhrzeit geprüft.
function pruefeBau(x) {
  if (x === null || !RE_BAU.test(x)) return null;
  const [mo, ta, st, mi] = [x.slice(2, 4), x.slice(4, 6), x.slice(6, 8), x.slice(8, 10)].map(Number);
  if (mo < 1 || mo > 12 || ta < 1 || ta > 31 || st > 23 || mi > 59) return null;
  return x;
}

// Kommagetrennt aus fester Liste; Unbekanntes verwerfen, doppelte weg, Reihenfolge wie im Link.
// null = Parameter fehlt, [] = Parameter da, aber nichts Gültiges darin.
function pruefeJailbreak(x) {
  if (x === null) return null;
  const liste = [];
  for (const teil of x.split(',').slice(0, 20)) {
    if (hat(JB, teil) && !liste.includes(teil)) liste.push(teil);
  }
  return liste;
}

function pruefeBereiche(x) {
  if (x === null || !RE_BEREICHE.test(x)) return null;
  const zahlen = x.split('.').map(Number);
  return zahlen.every((z) => z <= MAX_ZAHL) ? zahlen : null;
}

// Bis zu 5 Fehler „schritt.art.ort.code“, getrennt durch ~. Ungültige einzeln verwerfen.
function pruefeFehler(x) {
  const liste = [];
  if (x === null) return liste;
  for (const teil of x.split('~').slice(0, 20)) {
    const stuecke = teil.split('.');
    if (stuecke.length !== 4) continue;
    const [schritt, art, ort, code] = stuecke;
    if (!hat(SCHRITT, schritt) || !hat(ART, art) || !hat(ORT, ort) || !RE_CODE.test(code)) continue;
    liste.push({ schritt, art, ort, code });
    if (liste.length === MAX_FEHLER) break;
  }
  return liste;
}

// Gibt es überhaupt Gerätedaten? (Sprache zählt nicht.)
function hatGeraetedaten(d) {
  return PARAMETER.some((name) => name !== 'l' && (name === 'e' ? d.e.length > 0 : d[name] !== null));
}

// Geprüfte Werte wieder als Text in Link-Schreibweise (für versteckte Felder und Sprachwechsel).
function alsLinkWerte(d) {
  const w = {};
  for (const name of PARAMETER) {
    const x = d[name];
    if (name === 'e') {
      if (x.length) w.e = x.map((f) => `${f.schritt}.${f.art}.${f.ort}.${f.code}`).join('~');
    } else if (name === 'jb') {
      if (x !== null) w.jb = x.join(',');
    } else if (name === 'a') {
      if (x !== null) w.a = x.join('.');
    } else if (x !== null) {
      w[name] = String(x);
    }
  }
  return w;
}

// Eine gespeicherte Zeile wieder in dieselbe Form bringen (läuft erneut durch die Prüfung).
function zeileZuDaten(z) {
  let fehler = null;
  if (typeof z.fehler === 'string') {
    try {
      const liste = JSON.parse(z.fehler);
      if (Array.isArray(liste)) fehler = liste.map((f) => [f.schritt, f.art, f.ort, f.code].join('.')).join('~');
    } catch (_) {
      fehler = null;
    }
  }
  return pruefeDaten((name) => {
    if (name === 'e') return fehler;
    const x = z[name];
    return x === null || x === undefined ? null : String(x);
  });
}

// Text aus dem Textfeld: Zeilenumbrüche vereinheitlichen (Browser senden \r\n,
// zählen im Feld aber \n), Steuer- und Richtungszeichen entfernen, Leerraum am Rand weg.
function bereinigeText(wert) {
  if (typeof wert !== 'string') return '';
  return wert
    .replace(/\r\n?/g, '\n')
    .replace(/\t/g, ' ')
    .replace(/[\u0000-\u0008\u000B-\u001F\u007F‪-‮⁦-⁩]/g, '')
    .trim();
}

// ---------------------------------------------------------------------------
// Freitext schwärzen: IP-Adressen (v4/v6), E-Mail-Adressen, MAC-Adressen,
// Folgen von 9+ Ziffern und Telefonnummern mit Leerzeichen/Strichen → „[entfernt]“
// ---------------------------------------------------------------------------

const OKTETT = '(?:25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])';
const RE_EMAIL = /[\p{L}\p{N}._%+-]+@[\p{L}\p{N}-]+(?:\.[\p{L}\p{N}-]+)+/gu;
const RE_MAC = /(?<![0-9A-Fa-f:-])[0-9A-Fa-f]{2}([:-])(?:[0-9A-Fa-f]{2}\1){4}[0-9A-Fa-f]{2}(?![0-9A-Fa-f:-])/g;
const RE_MAC_PUNKT = /(?<![0-9A-Fa-f.])[0-9A-Fa-f]{4}\.[0-9A-Fa-f]{4}\.[0-9A-Fa-f]{4}(?![0-9A-Fa-f.])/g;
const RE_IPV6_KANDIDAT = new RegExp(
  `(?<![0-9A-Za-z:.])(?:[0-9A-Fa-f]{0,4}:){2,7}(?:(?:${OKTETT}\\.){3}${OKTETT}|[0-9A-Fa-f]{0,4})(?:%[0-9A-Za-z]{1,15})?(?![0-9A-Za-z:])`,
  'g'
);
const RE_IPV4 = new RegExp(`(?<![0-9.])(?:${OKTETT}\\.){3}${OKTETT}(?![0-9]|\\.[0-9])`, 'g');
const RE_ZIFFERN = /[0-9]{9,}/g;
const RE_TELEFON = /(?<![\p{L}\p{N}+])(?:\+|\(?0)[0-9 ()/-]{7,}[0-9]/gu;

function istIPv6(kandidat) {
  const ohneZone = kandidat.replace(/%.*$/, '');
  if (!/[0-9A-Fa-f]/.test(ohneZone) || (ohneZone.match(/:/g) || []).length < 2) return false;
  try {
    new URL(`http://[${ohneZone}]/`);
    return true;
  } catch (_) {
    return false;
  }
}

function schwaerze(text) {
  return text
    .replace(RE_EMAIL, ENTFERNT)
    .replace(RE_MAC, ENTFERNT)
    .replace(RE_MAC_PUNKT, ENTFERNT)
    .replace(RE_IPV6_KANDIDAT, (k) => (istIPv6(k) ? ENTFERNT : k))
    .replace(RE_IPV4, ENTFERNT)
    .replace(RE_ZIFFERN, ENTFERNT)
    .replace(RE_TELEFON, (k) => ((k.match(/[0-9]/g) || []).length >= 9 ? ENTFERNT : k));
}

// Auf höchstens max UTF-16-Einheiten kürzen, ohne ein Emoji zu zerschneiden.
function kuerzeEinheiten(s, max) {
  if (s.length <= max) return s;
  let x = s.slice(0, max);
  if (/[\uD800-\uDBFF]$/.test(x)) x = x.slice(0, -1);
  return x;
}

// Auf höchstens max Zeichen kürzen (mit „…“ am Ende, wenn gekürzt).
function kuerzeZeichen(s, max) {
  const zeichen = Array.from(s);
  return zeichen.length <= max ? s : zeichen.slice(0, max - 1).join('') + '…';
}

// ---------------------------------------------------------------------------
// GET /r
// ---------------------------------------------------------------------------

function seiteRueckmeldung(url, env) {
  const daten = pruefeDaten((name) => url.searchParams.get(name));
  const mitLink = PARAMETER.some((name) => name !== 'l' && url.searchParams.has(name));
  const bereit = istEingerichtet(env);
  const nonce = erzeugeZufall();
  const html = formularSeite(daten, {
    nonce,
    fid: erzeugeZufall(),
    sitekey: bereit ? env.TURNSTILE_SITEKEY : '',
    bereit,
    mitLink,
    wertung: null,
    text: '',
    meldung: bereit ? null : T[daten.l].nichtEingerichtet,
  });
  return htmlAntwort(html, bereit ? 200 : 503, { nonce, turnstile: bereit });
}

function istEingerichtet(env) {
  return Boolean(
    env &&
      typeof env.TURNSTILE_SITEKEY === 'string' && env.TURNSTILE_SITEKEY.length > 0 &&
      typeof env.TURNSTILE_SECRET === 'string' && env.TURNSTILE_SECRET.length > 0 &&
      env.DB
  );
}

// ---------------------------------------------------------------------------
// POST /senden
// ---------------------------------------------------------------------------

const SPALTEN = ['zeit', 'wertung', 'text', 'v', 'b', 'bd', 's', 'm', 'fw', 'l', 'dd', 'jb', 'sp', 'sm', 'a',
  'n', 'g', 'f', 'p', 'ps', 'u', 'sb', 'mb', 't', 'ab', 'ok', 'fehler', 'formular_id'];
const SPALTEN_LESEN = ['id', ...SPALTEN.filter((s) => s !== 'formular_id')].join(', ');

async function senden(request, env, ctx, url) {
  const art = (request.headers.get('content-type') || '').toLowerCase();
  if (!art.startsWith('application/x-www-form-urlencoded')) {
    return htmlAntwort(fehlerSeite('de', T.de.ungueltigTitel, T.de.ungueltigText), 400);
  }
  const koerper = await leseKoerper(request, MAX_KOERPER);
  if (koerper === null) {
    return htmlAntwort(fehlerSeite('de', T.de.ungueltigTitel, T.de.zuGross), 413);
  }
  const form = new URLSearchParams(koerper);
  const daten = pruefeDaten((name) => form.get(name));
  const t = T[daten.l];
  const wertungRoh = form.get('wertung');
  const wertung = wertungRoh === 'gut' || wertungRoh === 'problem' ? wertungRoh : null;
  const text = bereinigeText(form.get('text'));
  const token = form.get('cf-turnstile-response') || '';
  const fidRoh = form.get('fid') || '';
  const fid = RE_FID.test(fidRoh) ? fidRoh : null;

  // Bei behebbaren Fehlern die Seite mit Hinweis erneut zeigen (Wertung und Text bleiben erhalten).
  const nochmal = (status, meldung) => {
    const nonce = erzeugeZufall();
    const bereit = istEingerichtet(env);
    const html = formularSeite(daten, {
      nonce,
      fid: fid || erzeugeZufall(),
      sitekey: bereit ? env.TURNSTILE_SITEKEY : '',
      bereit,
      mitLink: hatGeraetedaten(daten),
      wertung,
      text,
      meldung,
    });
    return htmlAntwort(html, status, { nonce, turnstile: bereit });
  };

  if (!istEingerichtet(env)) return nochmal(503, t.nichtEingerichtet);
  if (!fid) return nochmal(400, t.formularAlt);

  // Doppelt abgeschickt (Doppel-Tippen, Neu laden, Zurück + nochmal)? Dann einfach wieder „Danke“.
  const schonDa = await sucheFormular(env, fid);
  if (schonDa !== null) return htmlAntwort(dankeSeite(daten.l, schonDa), 200);

  if (!wertung) return nochmal(400, t.fehltWertung);
  if (text.length > MAX_TEXT) return nochmal(400, t.zuLang);
  if (!token || token.length > MAX_TOKEN) return nochmal(400, t.fehltSpamschutz);

  const pruefung = await pruefeTurnstile(token, env.TURNSTILE_SECRET);
  if (!pruefung.ok) {
    if (pruefung.grund === 'nicht_erreichbar') return nochmal(503, t.spamschutzWeg);
    if (pruefung.grund === 'einrichtung') return nochmal(503, t.nichtEingerichtet);
    if (pruefung.grund === 'abgelaufen') return nochmal(403, t.spamschutzAbgelaufen);
    return nochmal(403, t.spamschutzFalsch);
  }

  const sauber = kuerzeEinheiten(schwaerze(text), MAX_TEXT);
  const zeit = new Date().toISOString().replace(/\.\d{3}Z$/, 'Z');
  const werte = {
    ...daten,
    zeit,
    wertung,
    text: sauber.length > 0 ? sauber : null,
    jb: daten.jb === null ? null : daten.jb.join(','),
    a: daten.a === null ? null : daten.a.join('.'),
    fehler: daten.e.length ? JSON.stringify(daten.e) : null,
    formular_id: fid,
  };
  const zeile = await env.DB.prepare(
    `INSERT INTO meldungen (${SPALTEN.join(', ')}) VALUES (${SPALTEN.map(() => '?').join(', ')}) ` +
      'ON CONFLICT (formular_id) DO NOTHING RETURNING id'
  )
    .bind(...SPALTEN.map((s) => werte[s]))
    .first();

  if (!zeile) {
    // Gleichzeitig doppelt abgeschickt: die andere Anfrage hat schon gespeichert.
    return htmlAntwort(dankeSeite(daten.l, await sucheFormular(env, fid)), 200);
  }

  // Benachrichtigung erst NACH dem Speichern und im Hintergrund – sie kann das Speichern nie verhindern.
  const auftrag = benachrichtige(env, url.origin, zeile.id, zeit, daten, wertung, werte.text);
  if (ctx && typeof ctx.waitUntil === 'function') ctx.waitUntil(auftrag);
  else await auftrag;

  return htmlAntwort(dankeSeite(daten.l, zeile.id), 200);
}

async function sucheFormular(env, fid) {
  const z = await env.DB.prepare('SELECT id FROM meldungen WHERE formular_id = ?').bind(fid).first();
  return z ? z.id : null;
}

async function leseKoerper(request, max) {
  const laengeKopf = request.headers.get('content-length');
  if (laengeKopf !== null && Number(laengeKopf) > max) return null;
  if (!request.body) return '';
  const leser = request.body.getReader();
  const teile = [];
  let laenge = 0;
  for (;;) {
    const { done, value } = await leser.read();
    if (done) break;
    laenge += value.byteLength;
    if (laenge > max) {
      try {
        await leser.cancel();
      } catch (_) {
        /* egal */
      }
      return null;
    }
    teile.push(value);
  }
  const alles = new Uint8Array(laenge);
  let pos = 0;
  for (const teil of teile) {
    alles.set(teil, pos);
    pos += teil.byteLength;
  }
  return new TextDecoder().decode(alles);
}

// Turnstile serverseitig prüfen. Die IP des Besuchers wird bewusst NICHT
// mitgeschickt (der Parameter remoteip ist freiwillig).
async function pruefeTurnstile(token, secret) {
  let antwort;
  try {
    antwort = await fetch(SITEVERIFY_URL, {
      method: 'POST',
      body: new URLSearchParams({ secret, response: token }),
      signal: AbortSignal.timeout(10000),
    });
  } catch (_) {
    return { ok: false, grund: 'nicht_erreichbar' };
  }
  let ergebnis;
  try {
    ergebnis = await antwort.json();
  } catch (_) {
    return { ok: false, grund: 'nicht_erreichbar' };
  }
  if (ergebnis && ergebnis.success === true) return { ok: true };
  const codes = ergebnis && Array.isArray(ergebnis['error-codes']) ? ergebnis['error-codes'] : [];
  if (codes.includes('timeout-or-duplicate')) return { ok: false, grund: 'abgelaufen' };
  if (codes.includes('missing-input-secret') || codes.includes('invalid-input-secret')) {
    return { ok: false, grund: 'einrichtung' };
  }
  if (codes.includes('internal-error') || !antwort.ok) return { ok: false, grund: 'nicht_erreichbar' };
  return { ok: false, grund: 'ungueltig' };
}

// ---------------------------------------------------------------------------
// Benachrichtigung an Home Assistant (Webhook)
// ---------------------------------------------------------------------------

function webhookNachricht(origin, id, daten, wertung, text) {
  const t = T.de;
  const fassung = daten.b === 't' ? 'Test' : daten.b === 'r' ? 'normal' : 'Fassung unbekannt';
  const teile = [
    daten.m !== null ? daten.m : 'Modell unbekannt',
    `FW ${daten.fw !== null ? daten.fw : 'unbekannt'}`,
    `${daten.v !== null ? daten.v : 'Version unbekannt'}/${fassung}`,
    daten.e.length ? `Fehler: ${fehlerKlartext(daten.e[0], 'de', ', ')}` : 'kein Fehler gemeldet',
  ];
  if (text) teile.push(`„${kuerzeZeichen(text.replace(/\s+/g, ' '), WEBHOOK_TEXT)}“`);
  return {
    titel: `PS5 Cleanup: ${wertung === 'problem' ? t.problem : t.gut}`,
    text: teile.join(' · '),
    link: `${origin}/uebersicht#m${id}`,
  };
}

async function benachrichtige(env, origin, id, zeit, daten, wertung, text) {
  try {
    const ziel = env.HA_WEBHOOK_URL;
    if (typeof ziel !== 'string' || !/^https:\/\/[^\s]+$/i.test(ziel)) return; // nicht gesetzt → nichts senden
    const tag = zeit.slice(0, 10); // UTC-Tag, z. B. 2026-11-03
    // Zähler in D1 hochzählen – aber nur, solange er unter dem Tageslimit liegt (in einem Schritt, ohne Wettlauf).
    const zaehler = await env.DB.prepare(
      'INSERT INTO benachrichtigungen (tag, anzahl) VALUES (?, 1) ' +
        'ON CONFLICT (tag) DO UPDATE SET anzahl = anzahl + 1 WHERE anzahl < ? RETURNING anzahl'
    )
      .bind(tag, TAGESLIMIT)
      .first();
    if (!zaehler) return; // Tageslimit erreicht → nur gespeichert, keine Benachrichtigung
    if (zaehler.anzahl === 1) {
      // Erster des Tages: alte Zählerzeilen (älter als 7 Tage) aufräumen.
      await env.DB.prepare("DELETE FROM benachrichtigungen WHERE tag < date(?, '-7 days')").bind(tag).run();
    }
    await fetch(ziel, {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify(webhookNachricht(origin, id, daten, wertung, text)),
      signal: AbortSignal.timeout(10000),
    });
  } catch (_) {
    // Fehler schlucken: die Meldung ist längst gespeichert. Nichts loggen.
  }
}

// ---------------------------------------------------------------------------
// GET /uebersicht und GET /export.json (Basic Auth)
// ---------------------------------------------------------------------------

function nichtAngemeldet() {
  const antwort = htmlAntwort(fehlerSeite('de', 'Anmeldung nötig', 'Diese Seite ist mit einem Passwort geschützt.'), 401);
  antwort.headers.set('WWW-Authenticate', 'Basic realm="PS5 Cleanup Uebersicht", charset="UTF-8"');
  return antwort;
}

async function uebersicht(request, env) {
  if (!(await istAngemeldet(request, env))) return nichtAngemeldet();
  if (!env.DB) return htmlAntwort(fehlerSeite('de', 'Nicht eingerichtet', 'Die Datenbank (D1-Bindung „DB“) fehlt.'), 503);

  const zaehlen =
    "COUNT(*) AS anzahl, SUM(CASE WHEN wertung = 'gut' THEN 1 ELSE 0 END) AS gut, " +
    "SUM(CASE WHEN wertung = 'problem' THEN 1 ELSE 0 END) AS problem";
  const [gesamt, jeModell, jeFirmware, jeVersion, fehler, letzte] = await env.DB.batch([
    env.DB.prepare(`SELECT ${zaehlen} FROM meldungen`),
    env.DB.prepare(`SELECT m AS wert, ${zaehlen} FROM meldungen GROUP BY m ORDER BY anzahl DESC, wert LIMIT 100`),
    env.DB.prepare(`SELECT fw AS wert, ${zaehlen} FROM meldungen GROUP BY fw ORDER BY anzahl DESC, wert LIMIT 100`),
    env.DB.prepare(`SELECT v, b, ${zaehlen} FROM meldungen GROUP BY v, b ORDER BY anzahl DESC, v DESC, b LIMIT 100`),
    env.DB.prepare(
      "SELECT json_extract(j.value, '$.schritt') AS schritt, json_extract(j.value, '$.art') AS art, " +
        "json_extract(j.value, '$.code') AS code, COUNT(*) AS anzahl, COUNT(DISTINCT x.id) AS meldungen " +
        'FROM meldungen AS x, json_each(x.fehler) AS j GROUP BY schritt, art, code ' +
        'ORDER BY anzahl DESC, schritt, art, code LIMIT 30'
    ),
    env.DB.prepare(`SELECT ${SPALTEN_LESEN} FROM meldungen ORDER BY id DESC LIMIT ${LETZTE}`),
  ]);

  const g = (gesamt.results && gesamt.results[0]) || {};
  const html = uebersichtSeite({
    anzahl: g.anzahl || 0,
    gut: g.gut || 0,
    problem: g.problem || 0,
    jeModell: jeModell.results || [],
    jeFirmware: jeFirmware.results || [],
    jeVersion: jeVersion.results || [],
    fehler: fehler.results || [],
    letzte: letzte.results || [],
  });
  return htmlAntwort(html, 200);
}

const EXPORT_STUECK = 500;
const ja_nein_json = (stelle) => `CASE substr(sp, ${stelle}, 1) WHEN '1' THEN 'true' ELSE 'false' END`;
const BEREICH_ARRAY = "'[' || replace(a, '.', ',') || ']'";
const EXPORT_SQL =
  'SELECT COUNT(*) AS anzahl, MAX(id) AS bis, json_group_array(json(zeile)) AS teil FROM (' +
  'SELECT id, json_object(' +
  "'id', id, 'zeit', zeit, 'wertung', wertung, 'text', text, 'v', v, 'b', b, 'bd', bd, 's', s, 'm', m, 'fw', fw, 'l', l, 'dd', dd, " +
  "'jb', json(CASE WHEN jb IS NULL THEN 'null' WHEN jb = '' THEN '[]' ELSE '[\"' || replace(jb, ',', '\",\"') || '\"]' END), " +
  `'sp', json(CASE WHEN sp IS NULL THEN 'null' ELSE '{"m2":' || ${ja_nein_json(1)} || ',"ext":' || ${ja_nein_json(2)} || ',"usb":' || ${ja_nein_json(3)} || '}' END), ` +
  "'sm', sm, " +
  `'a', json(CASE WHEN a IS NULL THEN 'null' ELSE json_object(${BEREICHE.map(([k], i) => `'${k}', json_extract(${BEREICH_ARRAY}, '$[${i}]')`).join(', ')}) END), ` +
  "'n', n, 'g', g, 'f', f, 'p', p, 'ps', ps, 'u', u, 'sb', sb, 'mb', mb, 't', t, 'ab', ab, 'ok', ok, " +
  "'e', json(COALESCE(fehler, '[]'))" +
  `) AS zeile FROM meldungen WHERE id > ? ORDER BY id LIMIT ${EXPORT_STUECK})`;
const EXPORT_HINWEIS =
  'Zeiten in UTC. Kurznamen wie im QR-Link: v Version, b Fassung (r normal, t Test), bd Bau-Stempel JJMMTTHHMM, ' +
  's Zeitpunkt (0 vor, 1 nach dem Nachschauen, 2 am Ende), m Modell, fw Firmware, l Sprache, dd Laufwerks-Kopplung (-1 nicht abgefragt), ' +
  'jb Jailbreak-Einstiege, sp Speicher, sm ShadowMount (0 keine Spiele, 1 Spiele vorhanden, 2 vor dem Löschen angehalten, 3 unklar), ' +
  'a Einträge je Bereich, n gefunden, g ausgewählt, f Fehler, p teilweise entfernt, ps vom PS4-Schutz zurückgehalten, ' +
  'u nicht lesbare Ordner, sb Speicherbremse, mb höchster Speicherverbrauch MB, t Dauer s, ab abgebrochen, ok Ergebnis laut App, ' +
  'e Fehlerliste (schritt, art, ort, code).';

async function exportJson(request, env) {
  if (!(await istAngemeldet(request, env))) return nichtAngemeldet();
  if (!env.DB) return htmlAntwort(fehlerSeite('de', 'Nicht eingerichtet', 'Die Datenbank (D1-Bindung „DB“) fehlt.'), 503);
  // Das JSON baut die Datenbank selbst (json_object / json_group_array), in Stücken zu je 500 Meldungen.
  // So bleibt die Rechenzeit im Worker auch bei vielen Meldungen klein (kostenloser Plan: 10 ms CPU je Aufruf).
  const teile = [];
  let anzahl = 0;
  let ab = 0;
  for (let runde = 0; runde < 900; runde++) {
    const stueck = await env.DB.prepare(EXPORT_SQL).bind(ab).first();
    if (!stueck || !stueck.anzahl) break;
    teile.push(stueck.teil.slice(1, -1));
    anzahl += stueck.anzahl;
    ab = stueck.bis;
    if (stueck.anzahl < EXPORT_STUECK) break;
  }
  const jetzt = new Date().toISOString().replace(/\.\d{3}Z$/, 'Z');
  const kopf = { erzeugt: jetzt, anzahl, hinweis: EXPORT_HINWEIS };
  const json = `${JSON.stringify(kopf).slice(0, -1)},"meldungen":[\n${teile.join(',\n')}\n]}\n`;
  return new Response(json, {
    status: 200,
    headers: {
      ...sicherheitsHeader({}),
      'Content-Type': 'application/json; charset=utf-8',
      'Content-Disposition': `attachment; filename="ps5cleanup-rueckmeldungen-${jetzt.slice(0, 10)}.json"`,
    },
  });
}

async function istAngemeldet(request, env) {
  const soll = env.UEBERSICHT_PASSWORT;
  if (typeof soll !== 'string' || soll.length === 0) return false; // ohne Passwort immer gesperrt
  let ist = '';
  const kopf = request.headers.get('authorization') || '';
  const treffer = /^Basic[ ]+([A-Za-z0-9+/=]+)[ ]*$/i.exec(kopf);
  if (treffer) {
    try {
      const binaer = atob(treffer[1]);
      const bytes = Uint8Array.from(binaer, (z) => z.charCodeAt(0));
      const klar = new TextDecoder().decode(bytes);
      const doppelpunkt = klar.indexOf(':');
      if (doppelpunkt >= 0) ist = klar.slice(doppelpunkt + 1); // Benutzername ist egal
    } catch (_) {
      ist = '';
    }
  }
  const gleich = await zeitkonstantGleich(ist, soll);
  return treffer !== null && gleich;
}

// Vergleich in konstanter Zeit: beide Seiten werden gehasht (gleiche Länge),
// dann Byte für Byte ohne vorzeitigen Abbruch verglichen.
async function zeitkonstantGleich(a, b) {
  const kodierer = new TextEncoder();
  const [ha, hb] = await Promise.all([
    crypto.subtle.digest('SHA-256', kodierer.encode(a)),
    crypto.subtle.digest('SHA-256', kodierer.encode(b)),
  ]);
  const x = new Uint8Array(ha);
  const y = new Uint8Array(hb);
  let unterschied = 0;
  for (let i = 0; i < x.length; i++) unterschied |= x[i] ^ y[i];
  return unterschied === 0;
}

// ---------------------------------------------------------------------------
// Antworten und Sicherheits-Header
// ---------------------------------------------------------------------------

function erzeugeZufall() {
  const bytes = new Uint8Array(16);
  crypto.getRandomValues(bytes);
  return btoa(String.fromCharCode(...bytes)).replace(/[+/=]/g, (z) => ({ '+': '-', '/': '_', '=': '' })[z]);
}

function inhaltsRichtlinie(opt) {
  const teile = ["default-src 'none'"];
  if (opt.turnstile && opt.nonce) {
    teile.push(`script-src 'nonce-${opt.nonce}' ${TURNSTILE_HOST}`);
    teile.push(`frame-src ${TURNSTILE_HOST}`);
    teile.push("connect-src 'self'");
  }
  // Stile nur inline (keine externen Schriften/Stylesheets). Alle Ausgaben werden escaped.
  teile.push("style-src 'unsafe-inline'");
  teile.push("form-action 'self'");
  teile.push("base-uri 'none'");
  teile.push("frame-ancestors 'none'");
  return teile.join('; ');
}

function sicherheitsHeader(opt) {
  return {
    'Content-Security-Policy': inhaltsRichtlinie(opt),
    'X-Content-Type-Options': 'nosniff',
    'Referrer-Policy': 'no-referrer',
    'X-Frame-Options': 'DENY',
    'Permissions-Policy': 'camera=(), microphone=(), geolocation=(), payment=(), usb=()',
    'Strict-Transport-Security': 'max-age=31536000',
    'Cache-Control': 'no-store',
    'X-Robots-Tag': 'noindex, nofollow',
  };
}

function htmlAntwort(html, status, opt = {}) {
  return new Response(html, {
    status,
    headers: { 'Content-Type': 'text/html; charset=utf-8', ...sicherheitsHeader(opt) },
  });
}

const ESC_ZEICHEN = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' };
function esc(wert) {
  const s = String(wert === null || wert === undefined ? '' : wert);
  return /[&<>"']/.test(s) ? s.replace(/[&<>"']/g, (z) => ESC_ZEICHEN[z]) : s;
}

// ---------------------------------------------------------------------------
// Texte
// ---------------------------------------------------------------------------

const T = {
  de: {
    lang: 'de',
    titel: 'PS5 Cleanup – Rückmeldung',
    imLink: 'Diese Angaben stehen im Link:',
    ohneDaten: 'Diese Rückmeldung wird ohne Gerätedaten gesendet – im Link stehen keine Angaben.',
    unbekannt: 'unbekannt',
    ja: 'ja',
    nein: 'nein',
    gruppe: { programm: 'Programm', konsole: 'Konsole', umgebung: 'Umgebung', ergebnis: 'Ergebnis', fehler: 'Fehler' },
    feld: {
      v: 'Version',
      b: 'Fassung',
      bd: 'Bau',
      s: 'Zeitpunkt',
      l: 'Seitensprache',
      m: 'Modell',
      fw: 'Firmware',
      dd: 'Laufwerks-Kopplung',
      jb: 'Jailbreak-Einstiege',
      sp: 'Speicher',
      sm: 'ShadowMount',
      a: 'Einträge je Bereich',
      n: 'Gefundene Einträge',
      g: 'Zum Löschen ausgewählt',
      f: 'Fehler',
      p: 'Nur teilweise entfernt',
      ps: 'Vom PS4-Schutz zurückgehalten',
      u: 'Nicht lesbare Ordner',
      sb: 'Speicherbremse',
      mb: 'Höchster Speicherverbrauch',
      t: 'Dauer des Nachschauens',
      ab: 'Abgebrochen',
      ok: 'Ergebnis laut App',
    },
    b: { r: 'normale Fassung (löscht)', t: 'Test-Fassung (löscht nie)' },
    s: { 0: 'vor dem Nachschauen', 1: 'nach dem Nachschauen', 2: 'am Ende, nach dem Löschen' },
    s2Test: 'am Ende, nach der Simulation',
    sm: { 0: 'keine ShadowMount-Spiele', 1: 'ShadowMount-Spiele vorhanden', 2: 'vor dem Löschen angehalten', 3: 'Zustand unklar' },
    dd: (x) => (x === -1 ? 'nicht abgefragt' : `Statuswert ${x}`),
    bau: (x) => `${x.slice(4, 6)}.${x.slice(2, 4)}.${x.slice(0, 2)} ${x.slice(6, 8)}:${x.slice(8, 10)}`,
    jbLeer: 'keine angegeben',
    ok: { 1: 'alles geklappt', 0: 'nicht alles geklappt' },
    keinCode: 'ohne Code',
    keineFehler: 'keine Fehlerdetails im Link',
    sekunden: (x) => `${x} s`,
    hinweis:
      'Gesendet wird nur, was hier steht, plus dein 👍/👎 und dein Text. Keine Namen von Spielen oder Apps, kein Konto, keine Dateien. Wie bei jedem Webseitenaufruf sieht der Server kurz die Internet-Adresse deines Handys – wir speichern sie nicht.',
    hinweisKlein: 'Außerdem gespeichert: die Uhrzeit des Absendens und eine zufällige Formular-Nummer, die doppeltes Absenden verhindert.',
    frage: 'Wie lief es?',
    gut: '👍 Hat geklappt',
    problem: '👎 Problem',
    textLabel: 'Was ist passiert? (freiwillig, höchstens 500 Zeichen)',
    textHinweis: 'Bitte schreib keine persönlichen Daten in das Textfeld.',
    textPlatzhalter: 'z. B. was nicht geklappt hat',
    senden: 'Senden',
    wirdGesendet: 'Wird gesendet …',
    schonGesendet: 'Schon gesendet. Für eine weitere Rückmeldung bitte die Seite neu laden.',
    nichtsGespeichert: 'Erst wenn du auf „Senden“ tippst, wird etwas gespeichert.',
    datenschutz: 'Datenschutz',
    andereSprache: 'English',
    keinJs: 'Für den Spamschutz braucht diese Seite JavaScript. Bitte JavaScript einschalten.',
    jsFehltWertung: 'Bitte zuerst 👍 oder 👎 antippen.',
    jsFehltToken: 'Der Spamschutz prüft noch – bitte einen Moment warten und nochmal tippen.',
    tsFehler: 'Der Spamschutz konnte nicht geladen werden. Bitte die Seite neu laden.',
    nichtEingerichtet: 'Rückmeldungen sind gerade nicht möglich, weil die Seite noch nicht fertig eingerichtet ist.',
    formularAlt: 'Das Formular war unvollständig oder veraltet. Bitte nochmal auf „Senden“ tippen.',
    fehltWertung: 'Bitte wähle 👍 oder 👎 aus.',
    zuLang: 'Der Text ist zu lang. Bitte höchstens 500 Zeichen.',
    fehltSpamschutz: 'Die Spamschutz-Prüfung fehlt. Bitte warte kurz, bis sie fertig ist, und tippe dann nochmal auf „Senden“.',
    spamschutzFalsch: 'Die Spamschutz-Prüfung hat nicht geklappt. Bitte nochmal versuchen.',
    spamschutzAbgelaufen: 'Die Spamschutz-Prüfung ist abgelaufen oder wurde schon benutzt. Bitte nochmal auf „Senden“ tippen.',
    spamschutzWeg: 'Der Spamschutz ist gerade nicht erreichbar. Bitte in ein paar Minuten nochmal versuchen.',
    ungueltigTitel: 'Ungültige Anfrage',
    ungueltigText: 'Diese Anfrage konnte nicht verarbeitet werden.',
    zuGross: 'Die Anfrage ist zu groß.',
    nichtGefunden: 'Diese Seite gibt es nicht.',
    dankeTitel: 'Danke! Deine Rückmeldung ist angekommen.',
    dankeNummer: (id) => `Deine Meldungs-Nummer: ${id}`,
    dankeLoeschen: 'Falls du deine Meldung später löschen lassen willst, nenne diese Nummer in einem GitHub-Issue.',
    dankeSchliessen: 'Du kannst diese Seite jetzt schließen.',
  },
  en: {
    lang: 'en',
    titel: 'PS5 Cleanup – Feedback',
    imLink: 'This is the information in the link:',
    ohneDaten: 'This feedback is sent without device data – the link contains no information.',
    unbekannt: 'unknown',
    ja: 'yes',
    nein: 'no',
    gruppe: { programm: 'Program', konsole: 'Console', umgebung: 'Environment', ergebnis: 'Result', fehler: 'Errors' },
    feld: {
      v: 'Version',
      b: 'Build',
      bd: 'Built',
      s: 'Point in time',
      l: 'Page language',
      m: 'Model',
      fw: 'Firmware',
      dd: 'Drive pairing',
      jb: 'Jailbreak entry points',
      sp: 'Storage',
      sm: 'ShadowMount',
      a: 'Entries per area',
      n: 'Entries found',
      g: 'Selected for deletion',
      f: 'Errors',
      p: 'Only partly removed',
      ps: 'Held back by PS4 protection',
      u: 'Unreadable folders',
      sb: 'Memory brake',
      mb: 'Highest memory use',
      t: 'Scan duration',
      ab: 'Cancelled',
      ok: 'Result according to app',
    },
    b: { r: 'normal build (deletes)', t: 'test build (never deletes)' },
    s: { 0: 'before scanning', 1: 'after scanning', 2: 'at the end, after deleting' },
    s2Test: 'at the end, after the simulation',
    sm: { 0: 'no ShadowMount games', 1: 'ShadowMount games present', 2: 'paused before deleting', 3: 'state unclear' },
    dd: (x) => (x === -1 ? 'not queried' : `status value ${x}`),
    bau: (x) => `20${x.slice(0, 2)}-${x.slice(2, 4)}-${x.slice(4, 6)} ${x.slice(6, 8)}:${x.slice(8, 10)}`,
    jbLeer: 'none given',
    ok: { 1: 'everything worked', 0: 'not everything worked' },
    keinCode: 'no code',
    keineFehler: 'no error details in the link',
    sekunden: (x) => `${x} s`,
    hinweis:
      'Only what is shown here is sent, plus your 👍/👎 and your text. No names of games or apps, no account, no files. As with any website visit, the server briefly sees your phone’s internet address – we do not store it.',
    hinweisKlein: 'Also stored: the time of sending and a random form number that prevents sending twice.',
    frage: 'How did it go?',
    gut: '👍 It worked',
    problem: '👎 Problem',
    textLabel: 'What happened? (optional, at most 500 characters)',
    textHinweis: 'Please do not write any personal data into the text field.',
    textPlatzhalter: 'e.g. what did not work',
    senden: 'Send',
    wirdGesendet: 'Sending …',
    schonGesendet: 'Already sent. To send another report, please reload the page.',
    nichtsGespeichert: 'Nothing is stored until you tap “Send”.',
    datenschutz: 'Privacy',
    andereSprache: 'Deutsch',
    keinJs: 'This page needs JavaScript for its spam protection. Please enable JavaScript.',
    jsFehltWertung: 'Please tap 👍 or 👎 first.',
    jsFehltToken: 'The spam check is still running – please wait a moment and tap again.',
    tsFehler: 'The spam protection could not be loaded. Please reload the page.',
    nichtEingerichtet: 'Feedback is not possible right now because this page has not been fully set up yet.',
    formularAlt: 'The form was incomplete or outdated. Please tap “Send” again.',
    fehltWertung: 'Please choose 👍 or 👎.',
    zuLang: 'The text is too long. Please use at most 500 characters.',
    fehltSpamschutz: 'The spam check is missing. Please wait until it has finished and tap “Send” again.',
    spamschutzFalsch: 'The spam check failed. Please try again.',
    spamschutzAbgelaufen: 'The spam check expired or was already used. Please tap “Send” again.',
    spamschutzWeg: 'The spam protection is not reachable right now. Please try again in a few minutes.',
    ungueltigTitel: 'Invalid request',
    ungueltigText: 'This request could not be processed.',
    zuGross: 'The request is too large.',
    nichtGefunden: 'This page does not exist.',
    dankeTitel: 'Thank you! Your feedback has arrived.',
    dankeNummer: (id) => `Your report number: ${id}`,
    dankeLoeschen: 'If you want your report deleted later, mention this number in a GitHub issue.',
    dankeSchliessen: 'You can close this page now.',
  },
};

// ---------------------------------------------------------------------------
// Klartext (gemeinsam für /r, /uebersicht und Benachrichtigung)
// ---------------------------------------------------------------------------

function fehlerKlartext(f, l, trenner = ' · ') {
  const i = idx(l);
  return [SCHRITT[f.schritt][i], ART[f.art][i], ORT[f.ort][i], f.code === '-' ? T[l].keinCode : f.code].join(trenner);
}

function zeitpunktText(d, t) {
  if (d.s === null) return t.unbekannt;
  if (d.s === 2 && d.b === 't') return t.s2Test;
  return t.s[d.s];
}

// Liefert die Gruppen Programm · Konsole · Umgebung · Ergebnis · Fehler mit [Bezeichnung, Wert]-Zeilen.
// Kernangaben (Version, Fassung, Zeitpunkt, Modell, Firmware) erscheinen immer (ggf. „unbekannt“),
// alle anderen nur, wenn sie im Link gültig vorhanden sind.
function gruppen(d, l) {
  const t = T[l];
  const i = idx(l);
  const F = t.feld;
  const janein = (x) => (x === 1 ? t.ja : t.nein);
  const wennDa = (zeilen, name, text) => {
    if (d[name] !== null) zeilen.push([F[name], text(d[name])]);
  };

  const programm = [
    [F.v, d.v !== null ? d.v : t.unbekannt],
    [F.b, d.b !== null ? t.b[d.b] : t.unbekannt],
  ];
  wennDa(programm, 'bd', t.bau);
  programm.push([F.s, zeitpunktText(d, t)]);
  programm.push([F.l, d.l === 'en' ? 'English' : 'Deutsch']);

  const konsole = [
    [F.m, d.m !== null ? d.m : t.unbekannt],
    [F.fw, d.fw !== null ? d.fw : t.unbekannt],
  ];
  wennDa(konsole, 'dd', t.dd);

  const umgebung = [];
  wennDa(umgebung, 'jb', (x) => (x.length ? x.map((k) => JB[k][i]).join(', ') : t.jbLeer));
  wennDa(umgebung, 'sp', (x) => SPEICHER.map(([, de, en], k) => `${l === 'en' ? en : de} ${x[k] === '1' ? t.ja : t.nein}`).join(', '));
  wennDa(umgebung, 'sm', (x) => t.sm[x]);

  const ergebnis = [];
  wennDa(ergebnis, 'a', (x) => BEREICHE.map(([, de, en], k) => `${l === 'en' ? en : de} ${x[k]}`).join(' · '));
  for (const name of ['n', 'g', 'f', 'p', 'ps', 'u']) wennDa(ergebnis, name, String);
  wennDa(ergebnis, 'sb', janein);
  wennDa(ergebnis, 'mb', (x) => `${x} MB`);
  wennDa(ergebnis, 't', t.sekunden);
  wennDa(ergebnis, 'ab', janein);
  wennDa(ergebnis, 'ok', (x) => t.ok[x]);

  const fehler = d.e.length ? d.e.map((f, k) => [`${k + 1}.`, fehlerKlartext(f, l)]) : [['', t.keineFehler]];

  return [
    ['programm', programm],
    ['konsole', konsole],
    ['umgebung', umgebung],
    ['ergebnis', ergebnis],
    ['fehler', fehler],
  ]
    .filter(([, zeilen]) => zeilen.length > 0)
    .map(([schluessel, zeilen]) => ({ schluessel, titel: t.gruppe[schluessel], zeilen }));
}

// ---------------------------------------------------------------------------
// HTML
// ---------------------------------------------------------------------------

const STIL = `
:root{color-scheme:dark;--bg:#0f1216;--karte:#181d24;--rand:#2a313b;--text:#e9edf2;--leise:#a3adba;
--gut:#1f7a3a;--gut-hell:#2fa356;--problem:#9b2c2c;--problem-hell:#d24545;--blau:#2f6fdf;--warn:#5a4310;--warn-text:#ffe2a3}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--text);font:17px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif}
main{max-width:560px;margin:0 auto;padding:20px 16px 40px}
h1{font-size:1.45rem;line-height:1.25;margin:4px 0 16px}
h2{font-size:1.05rem;margin:0 0 6px}
p{margin:0 0 12px}
a{color:#8fb6ff}
.karte{background:var(--karte);border:1px solid var(--rand);border-radius:14px;padding:12px 16px;margin:0 0 12px}
.klein{font-size:.92rem;color:var(--leise)}
.hinweis{background:#13243f;border:1px solid #2c4a7a;border-radius:14px;padding:14px 16px;margin:16px 0 18px}
.hinweis p:last-child{margin:0}
.meldung{background:var(--warn);color:var(--warn-text);border-radius:14px;padding:14px 16px;margin:0 0 14px;font-weight:600}
table.werte{border-collapse:collapse;width:100%;font-size:.95rem}
table.werte th,table.werte td{text-align:left;padding:5px 4px;border-top:1px solid var(--rand);vertical-align:top;overflow-wrap:anywhere}
table.werte tr:first-child th,table.werte tr:first-child td{border-top:0}
table.werte th{font-weight:400;color:var(--leise);width:45%}
table.liste-fehler th{width:2.2em}
table.werte td{font-weight:600}
fieldset{border:0;padding:0;margin:0 0 14px}
legend{font-size:1.1rem;font-weight:700;padding:0;margin:0 0 10px}
.wahl{display:flex;gap:12px}
.wahl label{flex:1;position:relative}
.wahl input{position:absolute;opacity:0;width:1px;height:1px}
.wahl span{display:flex;align-items:center;justify-content:center;min-height:64px;padding:10px;border-radius:14px;
border:2px solid var(--rand);background:var(--karte);font-size:1.1rem;font-weight:700;text-align:center;cursor:pointer;user-select:none}
.wahl .gut input:checked+span{background:var(--gut);border-color:var(--gut-hell)}
.wahl .problem input:checked+span{background:var(--problem);border-color:var(--problem-hell)}
.wahl input:focus-visible+span{outline:3px solid #8fb6ff;outline-offset:2px}
label.text{display:block;font-weight:600;margin:0 0 4px}
textarea{width:100%;min-height:120px;padding:12px;border-radius:12px;border:1px solid var(--rand);background:#0b0e12;color:var(--text);font:inherit;resize:vertical}
.zaehler{text-align:right;font-size:.9rem;color:var(--leise);margin-top:4px}
.ts{margin:16px 0;min-height:65px}
button,.knopf{display:flex;align-items:center;justify-content:center;width:100%;min-height:60px;padding:10px 16px;border:0;border-radius:14px;background:var(--blau);color:#fff;font:inherit;font-size:1.2rem;font-weight:700;line-height:1.25;cursor:pointer;text-align:center;text-decoration:none}
button:disabled{opacity:.6;cursor:default}
#jsHinweis{color:var(--warn-text);font-weight:600;min-height:1.5em;margin:10px 0 0}
footer{margin-top:26px;font-size:.92rem;color:var(--leise);display:flex;gap:18px;flex-wrap:wrap}
`;

function grundgeruest(l, titel, inhalt, extraKopf = '') {
  return `<!doctype html>
<html lang="${esc(l)}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="dark">
<meta name="robots" content="noindex, nofollow">
<title>${esc(titel)}</title>
<style>${STIL}</style>
${extraKopf}
</head>
<body>
<main>
${inhalt}
</main>
</body>
</html>`;
}

function gruppenHtml(d, l) {
  return gruppen(d, l)
    .map(
      (g) => `<section class="karte">
<h2>${esc(g.titel)}</h2>
<table class="werte${g.schluessel === 'fehler' ? ' liste-fehler' : ''}">${g.zeilen.map(([name, wert]) => `<tr><th scope="row">${esc(name)}</th><td>${esc(wert)}</td></tr>`).join('')}</table>
</section>`
    )
    .join('\n');
}

function formularSeite(daten, opt) {
  const t = T[daten.l];
  const andere = daten.l === 'de' ? 'en' : 'de';
  const linkWerte = alsLinkWerte(daten);
  const versteckt = Object.entries(linkWerte)
    .map(([name, wert]) => `<input type="hidden" name="${esc(name)}" value="${esc(wert)}">`)
    .join('\n');
  const sprachLink = new URLSearchParams({ ...linkWerte, l: andere }).toString();

  const formular = opt.bereit
    ? `<form id="formular" method="post" action="/senden" accept-charset="utf-8" novalidate>
${versteckt}
<input type="hidden" name="fid" value="${esc(opt.fid)}">
<fieldset>
<legend>${esc(t.frage)}</legend>
<div class="wahl">
<label class="gut"><input type="radio" name="wertung" value="gut"${opt.wertung === 'gut' ? ' checked' : ''}><span>${esc(t.gut)}</span></label>
<label class="problem"><input type="radio" name="wertung" value="problem"${opt.wertung === 'problem' ? ' checked' : ''}><span>${esc(t.problem)}</span></label>
</div>
</fieldset>
<label class="text" for="text">${esc(t.textLabel)}</label>
<p class="klein">${esc(t.textHinweis)}</p>
<textarea id="text" name="text" maxlength="${MAX_TEXT}" placeholder="${esc(t.textPlatzhalter)}">${esc(opt.text)}</textarea>
<div class="zaehler" id="zaehler" aria-live="polite">${opt.text.length} / ${MAX_TEXT}</div>
<div class="ts"><div class="cf-turnstile" data-sitekey="${esc(opt.sitekey)}" data-theme="dark" data-size="flexible" data-language="${esc(daten.l)}" data-action="rueckmeldung" data-callback="tsFertig" data-expired-callback="tsWeg" data-timeout-callback="tsWeg" data-error-callback="tsFehler"></div></div>
<noscript><p class="meldung">${esc(t.keinJs)}</p></noscript>
<button type="submit" id="senden">${esc(t.senden)}</button>
<p id="jsHinweis" role="status"></p>
<p class="klein">${esc(t.nichtsGespeichert)}</p>
</form>`
    : '';

  const texte = { wird: t.wirdGesendet, schon: t.schonGesendet, wertung: t.jsFehltWertung, token: t.jsFehltToken, tsFehler: t.tsFehler };
  const skript = opt.bereit
    ? `<script nonce="${esc(opt.nonce)}">
(function () {
  var TEXTE = ${JSON.stringify(texte).replace(/</g, '\\u003c')};
  var token = '';
  var gesendet = false;
  window.tsFertig = function (wert) { token = wert || ''; if (!gesendet) hinweis(''); };
  window.tsWeg = function () { token = ''; };
  window.tsFehler = function () { token = ''; hinweis(TEXTE.tsFehler); };
  function hinweis(x) { var el = document.getElementById('jsHinweis'); if (el) el.textContent = x; }
  document.addEventListener('DOMContentLoaded', function () {
    var form = document.getElementById('formular');
    var feld = document.getElementById('text');
    var zaehler = document.getElementById('zaehler');
    var knopf = document.getElementById('senden');
    if (feld && zaehler) {
      feld.addEventListener('input', function () { zaehler.textContent = feld.value.length + ' / ${MAX_TEXT}'; });
    }
    if (!form) return;
    form.addEventListener('change', function (e) {
      if (e.target && e.target.name === 'wertung' && document.getElementById('jsHinweis').textContent === TEXTE.wertung) hinweis('');
    });
    form.addEventListener('submit', function (e) {
      if (gesendet) { e.preventDefault(); return; }
      if (!form.querySelector('input[name="wertung"]:checked')) { e.preventDefault(); hinweis(TEXTE.wertung); return; }
      var feldToken = form.querySelector('input[name="cf-turnstile-response"]');
      if (!token && !(feldToken && feldToken.value)) { e.preventDefault(); hinweis(TEXTE.token); return; }
      gesendet = true;
      knopf.disabled = true;
      knopf.textContent = TEXTE.wird;
      hinweis('');
    });
    // Zurück-Taste nach dem Senden: Seite kommt aus dem Zwischenspeicher – nicht nochmal senden lassen.
    window.addEventListener('pageshow', function (e) {
      if (e.persisted && gesendet) { knopf.disabled = true; hinweis(TEXTE.schon); }
    });
    // Spamschutz-Skript gar nicht geladen (z. B. blockiert)? Dann nach einer Weile Bescheid sagen.
    setTimeout(function () { if (!window.turnstile && !token && !gesendet) hinweis(TEXTE.tsFehler); }, 12000);
  });
})();
</script>
<script nonce="${esc(opt.nonce)}" src="${TURNSTILE_JS}" async defer></script>`
    : '';

  const daten_ = opt.mitLink
    ? `<p class="klein">${esc(t.imLink)}</p>\n${gruppenHtml(daten, daten.l)}`
    : `<section class="karte"><p style="margin:0">${esc(t.ohneDaten)}</p></section>`;

  const inhalt = `
<h1>${esc(t.titel)}</h1>
${opt.meldung ? `<p class="meldung" role="alert">${esc(opt.meldung)}</p>` : ''}
${daten_}
<section class="hinweis">
<p>${esc(t.hinweis)}</p>
<p class="klein">${esc(t.hinweisKlein)} <a href="/datenschutz?l=${esc(daten.l)}">${esc(t.datenschutz)}</a></p>
</section>
${formular}
<footer>
<a href="/datenschutz?l=${esc(daten.l)}">${esc(t.datenschutz)}</a>
<a href="/r?${esc(sprachLink)}" lang="${andere}">${esc(t.andereSprache)}</a>
</footer>
${skript}`;
  return grundgeruest(daten.l, t.titel, inhalt);
}

function dankeSeite(l, id) {
  const t = T[l];
  const inhalt = `
<h1>${esc(t.titel)}</h1>
<section class="karte">
<p style="font-size:1.25rem;font-weight:700">${esc(t.dankeTitel)}</p>
${id !== null && id !== undefined ? `<p>${esc(t.dankeNummer(id))}</p><p class="klein">${esc(t.dankeLoeschen)}</p>` : ''}
<p class="klein" style="margin:0">${esc(t.dankeSchliessen)}</p>
</section>
<footer><a href="/datenschutz?l=${esc(l)}">${esc(t.datenschutz)}</a></footer>`;
  return grundgeruest(l, t.titel, inhalt);
}

function fehlerSeite(l, titel, text) {
  const inhalt = `
<h1>${esc(titel)}</h1>
<section class="karte"><p style="margin:0">${esc(text)}</p></section>`;
  return grundgeruest(l, titel, inhalt);
}

function datenschutzSeite(l) {
  const andere = l === 'de' ? 'en' : 'de';
  const link = (href, text) => `<a href="${esc(href)}" rel="noreferrer noopener">${esc(text)}</a>`;
  const inhalt =
    l === 'de'
      ? `
<h1>Datenschutz – PS5 Cleanup Rückmeldung</h1>
<section class="karte">
<p>Diese Seite sammelt Rückmeldungen zum Programm PS5 Cleanup. Die PS5 selbst sendet nichts: Sie zeigt nur einen QR-Code mit einem Link. Erst wenn du auf deinem Handy auf „Senden“ tippst, wird etwas gespeichert.</p>
<h2>Was gespeichert wird</h2>
<ul>
<li>die Angaben aus dem Link, so wie sie auf der Seite angezeigt werden: Programmversion, Fassung, Bau-Stempel, Zeitpunkt in der App, Modellnummer und Firmware der PS5, Laufwerks-Kopplung, eingerichtete Jailbreak-Einstiege, vorhandene Speicherorte, ShadowMount-Zustand, Anzahl der Einträge je Bereich, die Zahlen aus dem Ergebnis (gefunden, ausgewählt, Fehler usw.) und bis zu fünf Fehlercodes</li>
<li>deine Wertung 👍 oder 👎 und dein Text, falls du einen schreibst. Im Text werden IP-Adressen, E-Mail-Adressen, MAC-Adressen, Telefonnummern und lange Ziffernfolgen vor dem Speichern automatisch durch „[entfernt]“ ersetzt.</li>
<li>die Uhrzeit des Absendens (UTC), eine laufende Meldungs-Nummer und eine zufällige Formular-Nummer, die doppeltes Absenden verhindert</li>
<li>die Sprache der Seite</li>
</ul>
<h2>Was nicht gespeichert wird</h2>
<p>Keine IP-Adresse, kein Browser- oder Handy-Typ, kein Land, keine Cookies, keine Namen von Spielen oder Apps, keine Dateien, kein Konto.</p>
<h2>Wofür</h2>
<p>Nur um die Kompatibilität von PS5 Cleanup zu verbessern und Fehler zu finden. Keine Werbung, keine Weitergabe, kein Verkauf. Die Meldungen sieht nur der Entwickler von PS5 Cleanup über eine passwortgeschützte Übersicht. Bei einer neuen Meldung bekommt er eine kurze Benachrichtigung auf sein eigenes System (Modell, Firmware, Version, erster Fehler und der Anfang deines Textes).</p>
<h2>Wer die Seite technisch betreibt</h2>
<p>Die Seite und die Datenbank laufen bei Cloudflare (Cloudflare Workers und D1). Wie bei jedem Webseitenaufruf verarbeitet Cloudflare dabei technisch nötige Daten wie die IP-Adresse deines Handys. Diese Seite speichert die IP-Adresse nicht. Details: ${link(CF_DATENSCHUTZ.de, 'Datenschutzerklärung von Cloudflare')}.</p>
<p>Gegen Spam wird Cloudflare Turnstile verwendet. Turnstile prüft beim Laden der Seite, ob ein Mensch sie benutzt, und wertet dafür technische Merkmale aus (zum Beispiel IP-Adresse und Browser-Angaben). Diese Seite bekommt davon nur das Ergebnis „geprüft“ oder „nicht geprüft“. Details: ${link(CF_TURNSTILE_DATENSCHUTZ, 'Turnstile-Datenschutzhinweis von Cloudflare (englisch)')}.</p>
<h2>Löschen</h2>
<p>Die Meldungen werden aufbewahrt, solange sie für die Weiterentwicklung von PS5 Cleanup nützlich sind. Auf Wunsch wird deine Meldung gelöscht: Schreib dazu ein Issue auf ${link(GITHUB_ISSUES, 'GitHub')} und nenne die Meldungs-Nummer, die nach dem Senden angezeigt wurde (oder ungefähr Uhrzeit und Modell). Schreib dort bitte keine persönlichen Daten.</p>
</section>
<footer><a href="/datenschutz?l=${andere}" lang="${andere}">English</a></footer>`
      : `
<h1>Privacy – PS5 Cleanup feedback</h1>
<section class="karte">
<p>This page collects feedback about the program PS5 Cleanup. The PS5 itself sends nothing: it only shows a QR code with a link. Nothing is stored until you tap “Send” on your phone.</p>
<h2>What is stored</h2>
<ul>
<li>the information from the link, exactly as shown on the page: program version, build, build stamp, point in time in the app, model number and firmware of the PS5, drive pairing, jailbreak entry points set up, storage locations present, ShadowMount state, number of entries per area, the result numbers (found, selected, errors etc.) and up to five error codes</li>
<li>your rating 👍 or 👎 and your text, if you write one. IP addresses, e-mail addresses, MAC addresses, phone numbers and long digit sequences in the text are automatically replaced by “[entfernt]” before storing.</li>
<li>the time of sending (UTC), a running report number and a random form number that prevents sending twice</li>
<li>the language of the page</li>
</ul>
<h2>What is not stored</h2>
<p>No IP address, no browser or phone type, no country, no cookies, no names of games or apps, no files, no account.</p>
<h2>What for</h2>
<p>Only to improve the compatibility of PS5 Cleanup and to find bugs. No advertising, no sharing, no selling. Only the developer of PS5 Cleanup sees the reports, through a password-protected overview. For each new report he gets a short notification on his own system (model, firmware, version, first error and the beginning of your text).</p>
<h2>Who runs the page technically</h2>
<p>The page and the database run on Cloudflare (Cloudflare Workers and D1). As with any website visit, Cloudflare processes technically necessary data such as your phone’s IP address. This page does not store the IP address. Details: ${link(CF_DATENSCHUTZ.en, 'Cloudflare privacy policy')}.</p>
<p>Cloudflare Turnstile is used against spam. When the page loads, Turnstile checks whether a human is using it and evaluates technical signals for that (for example IP address and browser details). This page only receives the result “checked” or “not checked”. Details: ${link(CF_TURNSTILE_DATENSCHUTZ, 'Cloudflare Turnstile privacy addendum')}.</p>
<h2>Deletion</h2>
<p>Reports are kept as long as they are useful for developing PS5 Cleanup. On request your report will be deleted: open an issue on ${link(GITHUB_ISSUES, 'GitHub')} and mention the report number shown after sending (or roughly the time and model). Please do not post personal data there.</p>
</section>
<footer><a href="/datenschutz?l=${andere}" lang="${andere}">Deutsch</a></footer>`;
  return grundgeruest(l, l === 'de' ? 'Datenschutz – PS5 Cleanup' : 'Privacy – PS5 Cleanup', inhalt);
}

// Zeit für die Übersicht in deutscher Zeit. Selbst gerechnet (EU-Regel: Sommerzeit vom letzten
// Sonntag im März bis zum letzten Sonntag im Oktober, jeweils 01:00 UTC) – das ist deutlich
// schneller als Intl und hängt nicht von Zeitzonen-Daten der Laufzeitumgebung ab.
function zeitAnzeige(iso) {
  const datum = new Date(iso);
  const ms = datum.getTime();
  if (Number.isNaN(ms)) return String(iso);
  const jahr = datum.getUTCFullYear();
  const letzterSonntag = (monat) => {
    const d = new Date(Date.UTC(jahr, monat + 1, 0, 1)); // letzter Tag des Monats, 01:00 UTC
    return d.getTime() - d.getUTCDay() * 86400000;
  };
  const sommer = ms >= letzterSonntag(2) && ms < letzterSonntag(9);
  const x = new Date(ms + (sommer ? 2 : 1) * 3600000);
  const zz = (z) => String(z).padStart(2, '0');
  return `${zz(x.getUTCDate())}.${zz(x.getUTCMonth() + 1)}.${x.getUTCFullYear()} ${zz(x.getUTCHours())}:${zz(x.getUTCMinutes())} Uhr (deutsche Zeit)`;
}

function uebersichtSeite(s) {
  const t = T.de;
  const zahl = (x) => esc(x === null || x === undefined ? '–' : x);
  const tabelle = (titel, kopf, zeilen) =>
    `<h2>${esc(titel)}</h2>
<div class="scroll"><table class="liste"><thead><tr>${kopf.map((k) => `<th>${esc(k)}</th>`).join('')}</tr></thead><tbody>
${zeilen.length ? zeilen.map((z) => `<tr>${z.map((w) => `<td>${w}</td>`).join('')}</tr>`).join('\n') : `<tr><td colspan="${kopf.length}">noch keine</td></tr>`}
</tbody></table></div>`;
  const gruppe = (titel, zeilen, name) =>
    tabelle(titel, ['Wert', 'Meldungen', '👍', '👎'], zeilen.map((z) => [esc(name(z)), zahl(z.anzahl), zahl(z.gut || 0), zahl(z.problem || 0)]));

  const versionName = (z) =>
    `${RE_VERSION.test(z.v || '') ? z.v : 'Version unbekannt'} · ${z.b === 't' ? 'Test-Fassung' : z.b === 'r' ? 'normale Fassung' : 'Fassung unbekannt'}`;
  const fehlerZeilen = s.fehler
    .filter((z) => hat(SCHRITT, z.schritt) && hat(ART, z.art) && RE_CODE.test(z.code || ''))
    .map((z) => [
      `<code>${esc(`${z.schritt}.${z.art}.${z.code}`)}</code><br><span class="klein">${esc(
        `${SCHRITT[z.schritt][0]} · ${ART[z.art][0]} · ${z.code === '-' ? t.keinCode : z.code}`
      )}</span>`,
      zahl(z.anzahl),
      zahl(z.meldungen),
    ]);

  const karten = s.letzte
    .map((z) => {
      const d = zeileZuDaten(z);
      const zeilen = gruppen(d, 'de')
        .map((g) => `<p><b>${esc(g.titel)}:</b> ${esc(g.zeilen.map(([n, w]) => (!n ? w : n.endsWith('.') ? `${n} ${w}` : `${n}: ${w}`)).join(' · '))}</p>`)
        .join('\n');
      const id = Number(z.id);
      return `<article class="meldung-karte ${z.wertung === 'problem' ? 'schlecht' : 'gut'}" id="m${esc(id)}">
<p class="kopf"><a href="#m${esc(id)}">#${esc(id)}</a> · ${esc(zeitAnzeige(z.zeit))} · ${esc(z.wertung === 'problem' ? t.problem : t.gut)}</p>
${zeilen}
${z.text ? `<p class="text">${esc(z.text)}</p>` : '<p class="klein">(kein Text)</p>'}
</article>`;
    })
    .join('\n');

  const inhalt = `
<h1>PS5 Cleanup – Übersicht der Rückmeldungen</h1>
<section class="karte">
<p style="font-size:1.15rem;font-weight:700;margin:0">${zahl(s.anzahl)} ${s.anzahl === 1 ? 'Meldung' : 'Meldungen'} · 👍 ${zahl(s.gut)} · 👎 ${zahl(s.problem)}</p>
</section>
<a class="knopf" href="/export.json" download>Herunterladen (JSON)</a>
${gruppe('Je Modell', s.jeModell, (z) => (z.wert === null ? 'unbekannt' : z.wert))}
${gruppe('Je Firmware', s.jeFirmware, (z) => (z.wert === null ? 'unbekannt' : z.wert))}
${gruppe('Je Version / Fassung', s.jeVersion, versionName)}
${tabelle('Häufigste Fehler (Schritt.Art.Code)', ['Fehler', 'Anzahl', 'in Meldungen'], fehlerZeilen)}
<h2>${s.letzte.length === 1 ? 'Letzte Meldung' : `Letzte ${s.letzte.length} Meldungen`} (neueste zuerst)</h2>
${karten || '<p>noch keine Meldungen</p>'}`;

  const stilExtra = `<style>
main{max-width:900px}
h2{margin:22px 0 8px}
.knopf{margin:4px 0 8px}
.scroll{overflow-x:auto;-webkit-overflow-scrolling:touch;border:1px solid var(--rand);border-radius:12px}
table.liste{border-collapse:collapse;width:100%;font-size:.92rem}
table.liste th,table.liste td{padding:6px 8px;border-bottom:1px solid var(--rand);text-align:left;vertical-align:top}
table.liste th{background:var(--karte);color:var(--leise);font-weight:600;white-space:nowrap}
.meldung-karte{background:var(--karte);border:1px solid var(--rand);border-left:6px solid var(--gut-hell);border-radius:12px;padding:10px 14px;margin:0 0 12px;font-size:.93rem;overflow-wrap:anywhere}
.meldung-karte.schlecht{border-left-color:var(--problem-hell)}
.meldung-karte:target{outline:3px solid #8fb6ff}
.meldung-karte p{margin:0 0 6px}
.meldung-karte .kopf{font-weight:700;font-size:1rem}
.meldung-karte .text{white-space:pre-wrap;unicode-bidi:plaintext;background:#0b0e12;border-radius:8px;padding:8px 10px;margin:8px 0 0}
code{font-size:.95em}
</style>`;
  return grundgeruest('de', 'PS5 Cleanup – Übersicht', inhalt, stilExtra);
}
