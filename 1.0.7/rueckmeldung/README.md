# PS5 Cleanup – Rückmeldungs-Seite (Cloudflare Worker)

Technische Kurzbeschreibung. Die Schritt-für-Schritt-Anleitung in Alltagssprache steht in `EINRICHTUNG.txt`.

## Ablauf

Die PS5-App zeigt einen QR-Code mit `https://<HOST>/r?<parameter>`. Das Handy öffnet die Seite, sieht alle Angaben
in Klartext, tippt 👍/👎, optional Text (max. 500 Zeichen), „Senden“. Erst dann wird gespeichert. Die PS5 sendet nie etwas.

| Weg | Zweck |
|---|---|
| `GET /r` | Rückmeldungs-Seite (DE/EN nach `l`), handyfreundlich, dunkel |
| `POST /senden` | Turnstile prüfen, alles erneut prüfen, Text schwärzen, in D1 speichern, Dankeseite |
| `GET /uebersicht` | Statistik + letzte 200 Meldungen (Basic Auth, Passwort aus `UEBERSICHT_PASSWORT`) |
| `GET /export.json` | alle Meldungen als JSON (gleiche Basic Auth) |
| `GET /datenschutz` | Datenschutz-Hinweis DE/EN |
| alles andere | 404 |

Alles steckt in einer Datei (`src/index.js`, keine Abhängigkeiten), damit sie auch einfach im Cloudflare-Dashboard
in den Editor kopiert werden kann.

## Geheimnisse / Einstellungen

| Name | Pflicht | Inhalt |
|---|---|---|
| `DB` | ja | D1-Bindung (Datenbank `ps5cleanup-feedback`, Tabellen aus `schema.sql`) |
| `TURNSTILE_SITEKEY` | ja | Site Key des Turnstile-Widgets (öffentlich, steht unter `[vars]` in `wrangler.toml`) |
| `TURNSTILE_SECRET` | ja | Secret Key des Turnstile-Widgets |
| `UEBERSICHT_PASSWORT` | für `/uebersicht` | ohne → Übersicht und Export immer gesperrt |
| `HA_WEBHOOK_URL` | nein | Home-Assistant-Webhook (muss mit `https://` beginnen); ohne → keine Benachrichtigung |
| `NEUESTE_VERSION` | nein | neueste veröffentlichte Version, z. B. `1.0.8` (steht unter `[vars]` in `wrangler.toml`). Ist die Version im Link älter, zeigt `/r` oben einen Hinweis mit Link zu GitHub (Releases). Ohne oder ungültig → kein Hinweis. Bei jeder Veröffentlichung hochsetzen und neu hochladen. |

Fehlen Turnstile-Schlüssel oder D1, zeigt `/r` einen Hinweis „noch nicht fertig eingerichtet“ (503) statt des Formulars.

## Turnstile-Test-Schlüssel (nur lokal!)

Offizielle Werte von Cloudflare, Quelle: <https://developers.cloudflare.com/turnstile/troubleshooting/testing/>

| | Wert | Verhalten |
|---|---|---|
| Site Key | `1x00000000000000000000AA` | sichtbar, besteht immer |
| Site Key | `2x00000000000000000000AB` | sichtbar, scheitert immer |
| Site Key | `3x00000000000000000000FF` | erzwingt eine Aufgabe |
| Secret Key | `1x0000000000000000000000000000000AA` | Prüfung klappt immer |
| Secret Key | `2x0000000000000000000000000000000AA` | Prüfung scheitert immer |
| Secret Key | `3x0000000000000000000000000000000AA` | Fehler `timeout-or-duplicate` |

Test-Site-Keys erzeugen das Dummy-Token `XXXX.DUMMY.TOKEN.XXXX`. Test-Secret-Keys nehmen nur dieses Dummy-Token an,
echte Secret-Keys lehnen es ab. Test-Schlüssel funktionieren auf jeder Adresse, auch `localhost`.
Vorlage für `wrangler dev`: `.dev.vars.example` → in `.dev.vars` umbenennen.

## Gespeicherte Daten (`schema.sql`)

Tabelle `meldungen`: eine Spalte je Link-Parameter (gleiche Kurznamen wie im Link, nur geprüfte Werte, sonst `NULL`),
dazu `id`, `zeit` (UTC, ISO), `wertung` (`gut`/`problem`), `text` (geschwärzt, ≤ 500), `fehler` (JSON-Liste mit bis zu
5 Einträgen `{schritt, art, ort, code}`) und `formular_id` (Zufallswert je Seitenaufruf, `UNIQUE`, verhindert doppeltes
Speichern; wird weder angezeigt noch exportiert). `jb`, `sp`, `a` stehen in derselben Kurzschreibweise wie im Link
(`y2jb,browser`, `110`, `12.30.5.8.40.20.3`). Keine Spalten für IP, User-Agent, Land, Cookies.

Tabelle `benachrichtigungen`: `tag` (UTC-Datum) und `anzahl` – Zähler für das Tageslimit (20) der Benachrichtigungen.
Zählerzeilen älter als 7 Tage werden beim ersten Aufruf eines Tages gelöscht.

## Prüfregeln (kurz)

Alle Parameter optional. Ungültig → auf der Seite „unbekannt“ (Version, Fassung, Zeitpunkt, Modell, Firmware) bzw.
weggelassen (alle übrigen). Jede Ausgabe wird HTML-escaped; Werte werden serverseitig bei `POST /senden` erneut geprüft.
Zusätzlich zur Vorgabe: `bd` muss ein gültiges Datum/Uhrzeit sein (Monat 01–12, Tag 01–31, Stunde 00–23, Minute 00–59);
`jb` behält die Reihenfolge des Links, doppelte und unbekannte Einträge fallen weg, `jb=` (leer) heißt „keine angegeben“;
`e`: höchstens 20 Teile werden angesehen, ungültige verworfen, die ersten 5 gültigen behalten.

Schwärzen (vor dem Speichern, auch in der Benachrichtigung): E-Mail-Adressen, MAC-Adressen (`aa:bb:..`, `aa-bb-..`,
`aabb.ccdd.eeff`), IPv6 (geprüft mit dem URL-Parser), IPv4, Folgen von 9+ Ziffern, zusätzlich Telefonnummern mit
Leerzeichen/Strichen/Schrägstrich, die mit `+` oder `0` beginnen und mindestens 9 Ziffern haben → `[entfernt]`.
Fehlercodes wie `0x80990001`, Versionen, Firmware, Uhrzeiten und Daten bleiben stehen. Wird der Text durch das
Schwärzen länger als 500 Zeichen, wird er auf 500 gekürzt. Texte über 500 Zeichen werden abgelehnt (nicht gekürzt).

## Benachrichtigung (Home Assistant)

Nach dem Speichern, im Hintergrund (`ctx.waitUntil`), Fehler werden geschluckt – das Speichern hängt nie davon ab.
`POST HA_WEBHOOK_URL` mit JSON:

```json
{"titel": "PS5 Cleanup: 👎 Problem",
 "text": "CFI-7021 · FW 12.00 · 1.0.7/Test · Fehler: Sony-Deinstallation, PS4-Spiel, intern, 0x80990001 · „Text, höchstens 200 Zeichen…“",
 "link": "https://<HOST>/uebersicht#m42"}
```

Höchstens 20 pro UTC-Tag (atomar über D1 gezählt), darüber wird nur gespeichert. Beispiel für die Automation auf der
Home-Assistant-Seite (Webhooks sind dort standardmäßig nur aus dem Heimnetz erlaubt, daher `local_only: false`):

```yaml
alias: PS5 Cleanup Rückmeldung
mode: queued
triggers:
  - trigger: webhook
    webhook_id: "<lange-zufaellige-id>"
    allowed_methods: [POST]
    local_only: false
actions:
  - action: notify.mobile_app_<handy>
    data:
      title: "{{ trigger.json.titel }}"
      message: "{{ trigger.json.text }}"
      data:
        url: "{{ trigger.json.link }}"
        clickAction: "{{ trigger.json.link }}"
```

`HA_WEBHOOK_URL` ist dann `https://<home-assistant-adresse>/api/webhook/<lange-zufaellige-id>`.

## Tests

```
npm test
```

Node ≥ 22.13, keine Pakete nötig. Der Worker wird direkt mit Web-Standard-`Request`/`Response` aufgerufen; D1 wird
durch `node:sqlite` ersetzt (D1 ist SQLite, gleiches `schema.sql`), `fetch` für Turnstile und Webhook durch eine
Attrappe, die sich wie die offiziellen Test-Schlüssel verhält. Geprüft werden u. a.: echter App-Link, fehlende und
bösartige Parameter, Turnstile ohne/ungültig/abgelaufen/nicht erreichbar, Speichern ohne IP/User-Agent/Land/Cookie,
Schwärzen, Textlänge, doppeltes Absenden, Webhook (Inhalt, Tageslimit 20, Fehler/Hänger blockieren nicht),
Basic Auth für `/uebersicht` und `/export.json`, Datenschutz-Seite, 404, Sicherheits-Header.

Grenze: Die Tests laufen in Node, nicht in der echten Workers-Laufzeit (workerd/Miniflare) und nicht gegen echtes D1.

## Kostenloser Plan – Rechenzeit

Workers Free erlaubt 10 ms CPU je Aufruf. Gemessen (Node, gleiche V8-Engine, Datenbank-Zeit herausgerechnet):
`/r` < 1 ms, `/uebersicht` mit 200 Meldungen ≈ 4–6 ms, `/export.json` ≈ 4 ms bei 1 000 Meldungen und ≈ 20 ms bei
5 000 Meldungen. Der Export baut das JSON deshalb schon in der Datenbank (in Stücken zu 500). Bei mehreren tausend
Meldungen kann der Export trotzdem an die Grenze stoßen (Cloudflare-Fehler 1102) – dann `npx wrangler d1 export
ps5cleanup-feedback --remote --output=meldungen.sql` oder den bezahlten Plan nehmen.
