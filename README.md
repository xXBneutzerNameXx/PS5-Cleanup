<div align="center">

<img src="bilder/1.0.10/banner.jpg" width="100%" alt="PS5 Cleanup – Clean up your PS5, no reinstall, no reset">

**🇩🇪 Deutsch** · [🇬🇧 English](README_EN.md)

**Deine PS5 aufräumen wie frisch eingerichtet – ohne Neuinstallation, ohne Zurücksetzen.**

[![Version](https://img.shields.io/github/v/release/xXBneutzerNameXx/PS5-Cleanup?style=for-the-badge&label=Version&color=2f6fdf)](https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases/latest) [![Downloads](https://img.shields.io/github/downloads/xXBneutzerNameXx/PS5-Cleanup/total?style=for-the-badge&label=Downloads&color=1f7a3a)](https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases) [![Getestet](https://img.shields.io/badge/getestet-PS5%20Pro%20%C2%B7%20FW%2012.00-444444?style=for-the-badge)](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions/categories/compatibility) [![Lizenz](https://img.shields.io/badge/Lizenz-GPL--3.0-6e40c9?style=for-the-badge)](LICENSE.txt)

### [⬇️ PS5 Cleanup 1.0.10 herunterladen](https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases/latest)

<sub>Du brauchst nur eine Datei: <code>ps5cleanup_v1.0.10.elf</code>. Die ZIP enthält dieselbe Datei plus Anleitungen und Prüfsummen.</sub>

[Auf einen Blick](#auf-einen-blick) · [So geht's](#so-gehts) · [Rückmeldung](#rückmeldungen) · [Community](#community)

<img src="bilder/1.0.10/ablauf-2-auswahl.jpg" width="100%" alt="Auswahl der Einträge in PS5 Cleanup">

<sub><b>Die Auswahl am Fernseher:</b> links die Bereiche, in der Mitte jeder Eintrag mit Größe und einer Farbe dafür, was danach passiert, rechts die Erklärung. Hier zu sehen: PS3-Spiele und Playden.</sub>

</div>

<details>
<summary><b>📸 Mehr Bilder</b> (Start, Bestätigen, Fertig – zum Aufklappen)</summary>
<br>

<table>
<tr>
<td width="33%"><img src="bilder/1.0.10/ablauf-1-start.jpg" alt="Startseite von PS5 Cleanup"></td>
<td width="33%"><img src="bilder/1.0.10/ablauf-3-bestaetigen.jpg" alt="Bestätigungsseite: Das wird gelöscht"></td>
<td width="33%"><img src="bilder/1.0.10/ablauf-4-fertig.jpg" alt="Fertig-Seite mit Rückmeldung"></td>
</tr>
<tr>
<td><b>Start:</b> Konsole, Firmware, Jailbreak, Laufwerks-Kopplung und Speicher auf einen Blick. Nachschauen ändert nichts.</td>
<td><b>Bestätigen:</b> Vorher steht genau da, was gelöscht wird und was auf jeden Fall bleibt. Gelöscht wird erst, wenn du ✕ gedrückt hältst.</td>
<td><b>Fertig:</b> Freigeräumter Platz, Protokoll auf dem USB-Laufwerk und QR-Code für die Rückmeldung.</td>
</tr>
</table>

</details>

## Worum geht's?

Wer seine PS5 mit Jailbreak eine Weile nutzt, kennt das: tote Kacheln, Reste gelöschter Spiele, alte Downloads, Fehlerberichte, Daten von Tools, die man längst nicht mehr nutzt. Die Systemsoftware neu aufspielen will man deswegen aber nicht – erst recht nicht bei Slim und Pro, wo dabei die Laufwerks-Kopplung verloren gehen kann.

**PS5 Cleanup** läuft direkt auf der PS5, listet alles übersichtlich mit Namen auf und löscht nur das, was du auswählst.

**Zwei Wege:**
- **Gezielt** – einzelne Einträge anhaken, z. B. wenn ein Spiel oder Tool Probleme macht: nur dessen Daten, Spielstand oder Trophäen löschen, der Rest bleibt.
- **Vollständige Bereinigung** – alles auf der internen SSD auf einmal, wie frisch eingerichtet (die M.2 wird extra gefragt, Bestätigung mit `LÖSCHEN`).

Beim Start ist der **Testmodus** an: Du kannst alles anschauen, gelöscht wird nichts.

## Auf einen Blick

| 🧹 Sauber | 🛡️ Sicher | 🎮 Am Fernseher oder im Browser |
|---|---|---|
| Kacheln, Apps und Spiele trägt Sony selbst aus – keine leeren Kacheln, kein Datenbank-Neuaufbau | Jailbreak-Einstiege und PS4-Spiele sind geschützt. Gelöscht wird erst nach deiner Bestätigung | Eigene Kachel, Bedienung mit dem Controller – oder im Browser am Handy oder PC. Kein FTP, kein USB-Laufwerk nötig |
| **📦 Ein Block pro App** | **🧪 Testmodus** | **📱 Rückmeldung per QR-Code** |
| Kachel oben, darunter alles, was dazugehört – zugeordnet nur mit Beleg, geraten wird nie | Die App startet im Testmodus: Sie zeigt alles wie beim echten Löschen und löscht nichts, bis du ihn ausschaltest | Mit dem Handy scannen, ohne Konto, ohne persönliche Daten |

<details>
<summary><b>📋 Alle Funktionen im Einzelnen</b> (zum Aufklappen)</summary>

<br>

**🎮 Bedienung**

- **Eigene Kachel** auf dem Startbildschirm, Bedienung mit dem Controller am Fernseher – kein PC, kein Homebrew Launcher nötig
- **Bedienung im Browser** am Handy oder PC, im Heimnetz, ohne Anmeldung – dieselbe App, dieselben Bestätigungen
- **Übersicht auf einen Blick:** Modell, Firmware, eingerichtete Jailbreak-Einstiege, Laufwerks-Kopplung und freier Speicher
- **Größe jedes Eintrags direkt in der Liste** (wird im Hintergrund gezählt)
- **Protokoll** jederzeit mit Options; beim Löschen zusätzlich als Datei auf dem USB-Laufwerk, sonst auf der internen Platte oder der M.2
- **Deutsch und Englisch** – richtet sich nach der Systemsprache der PS5, Quadrat schaltet vor dem ersten Nachschauen um
- **Im Payload Manager als eigene Quelle:** Settings → Manage Sources → Add Source, dann `https://raw.githubusercontent.com/xXBneutzerNameXx/PS5-Cleanup/main/payloads.json` – neue Versionen kommen dort von selbst an (Idee und Vorlage: Strongt1me)

**🧹 Sauber löschen**

- **Ein Durchgang reicht** – Programme im Hintergrund werden vorher beendet (FTP-Server, nanoDNS, Payload Manager und der Jailbreak laufen weiter), Verknüpfungen von ShadowMount und Co. sauber gelöst
- **Kacheln verschwinden sauber** über Sonys eigenen Deinstallations-Dienst, **Bilder und Videos** über Sonys Löschdienst, **Spielstände** samt Eintrag in Sonys Spielstand-Liste
- **M.2: jedes Spiel und jede Datei einzeln** – die Ordner selbst bleiben stehen, für M.2 und erweiterten Speicher gibt es eine eigene Rückfrage
- **ShadowMount-Spiele:** wahlweise nur die Kachel oder Spiel samt Kachel – ShadowMount+ wird vorher sauber angehalten
- **Playden (Steam-Spiele):** werden samt Eintrag in Playdens Spieleliste entfernt; Playdens eigene Daten sind rot markiert
- **Am Ende:** ✕ Neu starten (empfohlen) oder ○ Beenden

**🔎 Erkennen und zuordnen**

- **Ein Block pro App und Spiel:** Kachel oben, darunter alles, was dazugehört – zugeordnet nur mit Beleg (Kennung, Name oder das Programm der App selbst)
- **Klarnamen und Kachelbilder** in 7 Bereichen: Spiele (PS5, PS4 und PS3-ISO) mit **Update und DLCs einzeln wählbar**, Apps, Spielstände, Trophäen, Bilder & Videos, Sonstiges, Jailbreak
- **Unbekannte Homebrew-Apps:** ihre Ordner werden aus den Programmdateien erkannt – Vermutetes nie automatisch angehakt

**🛡️ Sicherheit**

- **Jailbreak-Schutz:** YouTube (Y2JB), Netflix ('N Hack), Lua-Spiele, PS2-Spielstand-Jailbreak und offline gespeicherter WebKit-Jailbreak werden erkannt, nie automatisch angehakt und vor dem Löschen deutlich gewarnt
- **Nachschauen und Anhaken löscht nichts** – gelöscht wird erst nach ausdrücklicher Bestätigung (Nur Auswahl: ✕ 1,5 Sekunden halten, vollständige Bereinigung: `LÖSCHEN` eintippen)
- **Testmodus** beim Start: alles anschauen, nichts löschen – im Options-Menü ausschalten

</details>

## Voraussetzungen

- PS5 mit Jailbreak und einem Payload-Lader oder dem Payload Manager
- Kein PC, kein FTP und kein USB-Laufwerk nötig
- Für die Bedienung im Browser: Handy oder PC im selben Heimnetz wie die PS5

## So geht's

1. Jailbreak laden, `ps5cleanup_v1.0.10.elf` aus den [Releases](https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases/latest) über den Payload-Lader oder den Payload Manager starten. Steckt ein USB-Laufwerk, kommt das Protokoll darauf. Wer lieber am Handy oder PC bedient, öffnet dort im Browser die Adresse unten auf der Startseite.
2. **Nachschauen** und anschauen, was gefunden wurde. Die App ist im **Testmodus** und löscht nichts. Stimmt die Liste, den Testmodus im Options-Menü ausschalten.
3. Einzelne Einträge anhaken und Quadrat = **Nur Auswahl löschen** (zum Bestätigen ✕ 1,5 Sekunden halten) – oder Dreieck = **Vollständige Bereinigung** (`LÖSCHEN` eintippen).
4. Am Ende **✕ Neu starten** (empfohlen) oder **○ Beenden**. Nach dem Neustart den Jailbreak wie gewohnt starten.

> [!WARNING]
> **Kein „Datenbank neu aufbauen“:** Sonys „Datenbank neu aufbauen“ (abgesicherter Modus) löscht installierte **PS4-Spiele aus Fake-PKGs** – sie müssen danach neu installiert werden. Mit PS5 Cleanup ist der Neuaufbau nicht nötig. **Nie** „PS5 zurücksetzen“ oder „Systemsoftware neu installieren“ wählen.

Ausführliche Anleitung: [LIESMICH.txt](LIESMICH.txt)

<details>
<summary><b>✨ Neu in 1.0.10 – ein Durchgang reicht, Playden, PS3-Spiele</b> (zum Aufklappen)</summary>

<br>

- **Ein Durchgang reicht:** Was du auswählst, ist nach einem Lauf weg – ohne zweiten Durchgang und ohne Neustart dazwischen. Das gilt auch für Kacheln, die ein Programm noch mit einem Spielordner verknüpft hat (etwa ShadowMount, Kraken Mount, PS5Upload), und für die leeren Ordner, die Homebrew-Kacheln im Systembereich hinterlassen.
- **Programme im Hintergrund** werden vor dem Löschen beendet, damit sie Gelöschtes nicht sofort wieder anlegen. FTP-Server, nanoDNS, Payload Manager und der Jailbreak laufen weiter. Die vollständige Bereinigung endet mit einem Neustart.
- **Playden:** Steam-Spiele werden erkannt und samt Eintrag in Playdens Spieleliste entfernt. Playdens eigene Daten sind rot markiert, denn ohne sie startet Playden nicht mehr.
- **Mehr Spiele mit Name und Bild:** PS3-Spiele als ISO und PS5-Spieldateien (`.pkg`). Unter Spiele steht PS5 vor PS4 vor PS3.
- **Unbekannte Homebrew-Apps:** PS5 Cleanup liest ihre Programmdateien und findet so ihre Ordner. Was nur vermutet ist, steht im Block der App mit eigenem Haken und wird nie von selbst angehakt.
- **TabShifter und Co.:** Kacheln, die ein Tool direkt in Sonys Liste eingetragen hat, werden gezielt ausgetragen. Sicherungen von Sonys App-Liste gehen nur mit, wenn du sie selbst anhakst.
- **Klares Ergebnis:** Bleibt etwas übrig, nennt PS5 Cleanup es mit Grund – im Protokoll auch den Schritt, an dem es scheiterte.
- **Erweiterte Fehlerdiagnose (freiwillig):** In der Browser-Bedienung lässt sich bei der Rückmeldung das bereinigte Protokoll mitschicken (siehe [Rückmeldungen](#rückmeldungen)).

Getestet auf einer PS5 Pro (CFI-7021) mit Firmware 12.00, M.2 und USB-Laufwerk – gezielt und vollständig, am Fernseher und im Browser. Andere Modelle und Firmwares sind nicht gesperrt, aber noch nicht getestet – dort bitte im Testmodus zuerst schauen, ob die Liste stimmt. [Alle Neuerungen](https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases)

</details>

## Was das Programm nicht anfasst

Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, Aktivierung, echte Lizenzen und die Sony-Systemanwendungen. Es wird nichts installiert und nichts zurückgesetzt. Sonys Datenbanken ändert die PS5-App nur gezielt: die Einträge gelöschter Spielstände, kaputte Zusatzinhalt-Angaben eines Spiels, das du löschst, und Kacheln, die ein Tool direkt in Sonys App-Liste eingetragen hat (z. B. TabShifter) und die du zum Löschen auswählst. Jede dieser Änderungen wird vorher geprüft und ganz oder gar nicht gemacht.

## Datenschutz

PS5 Cleanup legt auf der PS5 nichts ab außer seiner eigenen Kachel und seinen **Protokollen** (USB-Laufwerk, sonst auf der PS5) – keine Einstellungen, keine Listen, kein Merken zwischen zwei Starts.

<details>
<summary><b>Was dabei ins Netz geht</b> (zum Aufklappen)</summary>

<br>

- **Update-Hinweis:** Beim Start fragt PS5 Cleanup einmal bei GitHub (`api.github.com`) nach der neuesten Versionsnummer. Gesendet wird nur diese Anfrage – wie beim Öffnen einer Webseite sieht GitHub dabei die Internet-Adresse. Geladen oder ausgeführt wird nichts, angenommen wird nur eine Versionsnummer. Ohne Internet passiert nichts.
- **Bedienung im Browser:** Die Seite kommt von der PS5 selbst und antwortet nur Geräten aus dem Heimnetz. Ins Internet geht dabei nichts, im Browser bleibt nichts außer den Kachelbildern im Zwischenspeicher (höchstens 10 Minuten). Sobald PS5 Cleanup beendet ist, ist auch die Seite weg.
- **Rückmeldung per QR-Code:** siehe [Rückmeldungen](#rückmeldungen) – die PS5 selbst sendet dabei nichts.

</details>

## Rückmeldungen

**Am einfachsten per QR-Code:** Am Ende und unter Options, „Protokoll & Rückmeldung“ zeigt PS5 Cleanup einen QR-Code. Mit dem Handy scannen, auf „Hat geklappt“ oder „Problem“ tippen – ohne Konto, ohne PC. Die PS5 sendet dabei nichts.

<details>
<summary><b>Was in der Rückmeldung steht</b> (zum Aufklappen)</summary>

<br>

Im Link stehen nur Version, Fassung, Bau-Stempel, Modellnummer, Firmware, Laufwerks-Kopplung, eingerichtete Jailbreak-Einstiege, angeschlossene Speicher, ShadowMount, Zahlen je Bereich und bei Fehlern kurze Fehlercodes (Schritt, Art des Eintrags, Ort, Code) – **keine Namen von Spielen oder Apps, keine Spielkennungen, kein Konto, keine Dateien, keine IP der PS5**. Dieselben Angaben stehen in Klartext daneben und noch einmal auf dem Handy; gespeichert wird erst mit „Senden“. Wie bei jedem Webseitenaufruf sieht der Server kurz die Internet-Adresse des Handys – sie wird nicht gespeichert. Die Seite läuft bei Cloudflare unter `feedback.ps5cleanup.workers.dev` ([Datenschutz-Hinweis](https://feedback.ps5cleanup.workers.dev/datenschutz)).

**Erweiterte Fehlerdiagnose (freiwillig, neu in 1.0.10):** Gibst du die Rückmeldung aus der Browser-Bedienung, kannst du auf der nächsten Seite zusätzlich das Protokoll mitschicken. Der Haken ist zuerst aus. Das Protokoll wird vorher bereinigt (IP-, E-Mail- und MAC-Adressen, Benutzer-Nummern entfernt), enthält aber Namen von Spielen und Apps und Ordnerpfade deiner Konsole. Du siehst es vor dem Senden vollständig.

</details>

Ausführlicher: Über [Issues](https://github.com/xXBneutzerNameXx/PS5-Cleanup/issues), die [Discussions](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions) (Modell, Firmware und Ergebnis gern unter [Compatibility](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions/categories/compatibility)) oder auf psxtools.de – am besten mit dem **Protokoll**. Das geht auch ohne etwas zu löschen: In der Protokoll-Ansicht (Options, „Protokoll & Rückmeldung“) speichert Quadrat es auf das USB-Laufwerk – auch im Testmodus.

<a name="community"></a>
### 🗳️ PS5 Cleanup Community – Deine Meinung zählt!

**Welche PS5 nutzt du?** Hilf uns, PS5 Cleanup für verschiedene Konsolen und Firmware-Versionen zu verbessern!

[**🗳️ Jetzt abstimmen**](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions/4) · [**💬 Diskussionen & Vorschläge**](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions) · [**🎮 Kompatibilität melden**](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions/categories/compatibility)

## Aufbau

<details>
<summary>Ordner im Quellcode (zum Aufklappen)</summary>

<br>

| Ordner | Inhalt |
|---|---|
| `1.0.10/kern/` | Kern der PS5-App: Schutzliste, Suche, Löschen, Abläufe |
| `1.0.10/app/` | Oberfläche am Fernseher (SDL2, DualSense), Bedienung im Browser (`fern.c`, `fern_seite.html`) und PS5-Anbindung (Sony-Deinstallation, Löschdienst, Spielstand-Liste) |
| `1.0.10/start/` | `ps5cleanup.elf`: Start, eigene Kachel (PSCL00001), Hintergrundteil für Selbstentfernung und Neustart |
| `1.0.10/db/` | Prüfung von Sonys Datenbanken (SQLite) |
| `1.0.10/rueckmeldung/` | Handy-Seite für die Rückmeldung per QR-Code (Cloudflare Worker) |
| `1.0.10/test/` | Tests am PC |
| `1.0.10/fremd/` | Anleitung und Skript für die fremden Bibliotheken (PCRE2, cJSON) |
| `Quellcode/` | die bisherige Windows-Fassung 1.0.6 (C#) und ältere Fassungen – nicht mehr weiterentwickelt |

</details>

## Lizenz

Die PS5-App (`1.0.10/`, `ps5cleanup_v1.0.10.elf`) steht unter der GPL-3.0 (oder später) – Copyright (c) 2026 da_Chris86. Der Code der bisherigen Windows-Fassung steht unter MIT. Details und Bestandteile anderer Urheber in [LICENSE.txt](LICENSE.txt).

## Danke an

[drakmor](https://github.com/drakmor) (ftpsrv, ps5-hwinfo), [earthonion](https://github.com/earthonion) (ddd_status), John Törnblom (ps5-payload-sdk, ftpsrv), [seregonwar](https://github.com/seregonwar) (zftpd), die Macher von ShadowMountPlus und PS5Upload, [Project Nayuki](https://www.nayuki.io/page/qr-code-generator-library) (QR-Code-Bibliothek) sowie Markus Scholtes (PS2EXE).

Entwickelt von da_Chris86 mit Claude (Anthropic). Unabhängige Code-Prüfung: OpenAI Codex. Idee, Vorgaben und die Tests an der echten Konsole kommen von mir.

**Benutzung auf eigene Gefahr – Gelöschtes ist endgültig weg.**
Inoffizielles Community-Projekt, keine Verbindung zu Sony. PlayStation und PS5 sind Marken von Sony Interactive Entertainment Inc.
