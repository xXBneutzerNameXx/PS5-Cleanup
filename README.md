<div align="center">

<img src="bilder/1.0.7/banner.png" width="100%" alt="PS5 Cleanup – Clean up your PS5, no reinstall, no reset">

**Deine PS5 aufräumen wie frisch eingerichtet – ohne Neuinstallation, ohne Zurücksetzen.**

[![Version](https://img.shields.io/github/v/release/xXBneutzerNameXx/PS5-Cleanup?style=for-the-badge&label=Version&color=2f6fdf)](https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases/latest) [![Downloads](https://img.shields.io/github/downloads/xXBneutzerNameXx/PS5-Cleanup/total?style=for-the-badge&label=Downloads&color=1f7a3a)](https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases) [![Getestet](https://img.shields.io/badge/getestet-PS5%20Pro%20%C2%B7%20FW%2012.00-444444?style=for-the-badge)](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions/categories/compatibility) [![Lizenz](https://img.shields.io/badge/Lizenz-GPL--3.0-6e40c9?style=for-the-badge)](LICENSE.txt)

### [⬇️ PS5 Cleanup 1.0.8 herunterladen](https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases/latest)

[So geht's](#so-gehts) · [Was es kann](#was-es-kann) · [Rückmeldung](#rückmeldungen) · [Community](#community) · [**🇬🇧 English**](#english)

</div>

## Auf einen Blick

| 🧹 Sauber | 🛡️ Sicher | 🎮 Direkt am Fernseher |
|---|---|---|
| Kacheln, Apps und Spiele trägt Sony selbst aus – keine leeren Kacheln, kein Datenbank-Neuaufbau | Jailbreak-Einstiege und PS4-Spiele sind geschützt. Gelöscht wird erst nach deiner Bestätigung | Eigene Kachel, Bedienung mit dem Controller – kein PC, kein FTP, kein USB-Laufwerk nötig |
| **📦 Ein Block pro App** | **🧪 Testmodus** | **📱 Rückmeldung per QR-Code** |
| Kachel oben, darunter alles, was dazugehört – zugeordnet nur mit Beleg, geraten wird nie | Die App startet im Testmodus: Sie zeigt alles wie beim echten Löschen und löscht nichts, bis du ihn ausschaltest | Mit dem Handy scannen, ohne Konto, ohne persönliche Daten |

<details>
<summary><b>📸 So sieht's aus</b> (Bilder von der PS5, zum Aufklappen)</summary>
<br>

<table>
<tr>
<td width="50%"><img src="bilder/1.0.8/ablauf-1-start.jpg" alt="Startseite von PS5 Cleanup"></td>
<td width="50%"><img src="bilder/1.0.8/ablauf-2-auswahl.jpg" alt="Auswahl der Einträge"></td>
</tr>
<tr>
<td><b>1. Start:</b> Konsole, Firmware, Jailbreak, Laufwerks-Kopplung und Speicher auf einen Blick. Nachschauen ändert nichts.</td>
<td><b>2. Auswahl:</b> Links die Bereiche, in der Mitte jeder Eintrag mit Größe und einer Farbe dafür, was danach passiert, rechts die Erklärung. PKG-Dateien stehen bei ihrem Spiel.</td>
</tr>
<tr>
<td width="50%"><img src="bilder/1.0.8/ablauf-3-bestaetigen.jpg" alt="Bestätigungsseite: Das wird gelöscht"></td>
<td width="50%"><img src="bilder/1.0.8/ablauf-4-fertig.jpg" alt="Fertig-Seite mit Rückmeldung"></td>
</tr>
<tr>
<td><b>3. Bestätigen:</b> Vorher steht genau da, was gelöscht wird und was auf jeden Fall bleibt. Gelöscht wird erst, wenn du ✕ gedrückt hältst.</td>
<td><b>4. Fertig:</b> Freigeräumter Platz, Protokoll auf dem USB-Laufwerk und QR-Code für die Rückmeldung.</td>
</tr>
</table>

</details>

<a name="community"></a>
### 🗳️ PS5 Cleanup Community – Deine Meinung zählt!

**Welche PS5 nutzt du?** Hilf uns, PS5 Cleanup für verschiedene Konsolen und Firmware-Versionen zu verbessern!

[**🗳️ Jetzt abstimmen**](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions/4) · [**💬 Diskussionen & Vorschläge**](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions) · [**🎮 Kompatibilität melden**](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions/categories/compatibility)

## ⚠️ Welche Datei ist was? Bitte zuerst lesen!

| Datei | Wofür | Löscht? |
|---|---|---|
| **`ps5cleanup_v1.0.8.elf`** | **PS5-App** – läuft direkt auf der PS5, Bedienung mit dem Controller. Startet im **Testmodus** (oben steht „TESTMODUS“) | **Erst, wenn du den Testmodus ausschaltest** und das Löschen ausdrücklich bestätigst |
| **`PS5-Cleanup-1.0.8.zip`** | die ELF-Datei + Anleitungen + Prüfsummen | – |

> [!TIP]
> **Neu in 1.0.8: nur noch eine Datei.** Die App startet im Testmodus und zeigt dir genau, was sie löschen würde – ohne jedes Risiko. Erst wenn die Liste stimmt, schaltest du den Testmodus im Options-Menü aus. Die frühere `PS5-Cleanup-Test_v1.0.7.elf` gibt es nicht mehr.

> [!WARNING]
> **Kein „Datenbank neu aufbauen“ mehr:** Sonys „Datenbank neu aufbauen“ (abgesicherter Modus) löscht installierte **PS4-Spiele aus Fake-PKGs** auf der internen SSD und auf dem erweiterten Speicher (USB-Festplatte). Die PKG-Dateien bleiben, die Spiele müssen aber neu installiert werden (beobachtet am 31.10.2026 auf einer PS5 Pro mit Firmware 12.00; andere Tools wie elf-arsenal und ps5-db-backup beschreiben dasselbe). Frühere Fassungen dieser Anleitung haben den Neuaufbau empfohlen – **bitte nicht mehr machen.** Ab 1.0.7 ist er auch nicht mehr nötig. Was sich nicht sauber entfernen lässt, nennt PS5 Cleanup am Ende und lässt es unverändert. Bleibt trotzdem eine leere Kachel stehen, schick uns bitte das Protokoll (siehe [Rückmeldungen](#rückmeldungen)). **Nie** „PS5 zurücksetzen“ oder „Systemsoftware neu installieren“ wählen.

> [!NOTE]
> **Keine Windows-Fassung mehr:** Ab 1.0.7 gibt es keine `PS5-Cleanup.exe` mehr – [warum, steht unten](#warum-keine-windows-fassung-mehr).

<details>
<summary><b>✨ Neu in 1.0.8 – neue Oberfläche, Testmodus in der App, Löschen mit Halten bestätigen</b> (zum Aufklappen)</summary>

<br>

- **Neue Oberfläche am Fernseher:** Startseite mit Konsole, Laufwerks-Kopplung, Jailbreak und Speicher je Laufwerk. In der Auswahl links die Bereiche, in der Mitte die Blöcke, rechts ein Kasten mit Bild und Erklärung. Größte Einträge zuerst (oder A–Z), L1/R1 blättern seitenweise.
- **Nur noch eine Datei – mit Testmodus:** Die App startet im Testmodus, oben steht „TESTMODUS“. Sie zeigt alles und löscht nichts, bis du ihn im Options-Menü ausschaltest.
- **Bestätigen auf einer eigenen Seite:** was endgültig weg ist, was du neu laden oder einrichten musst, was nichts kostet – und was immer bleibt. Nur Auswahl löschen: ✕ 1,5 Sekunden gedrückt halten. Vollständige Bereinigung: wie bisher `LÖSCHEN` eintippen.
- **„Nachschauen läuft“ und „Löschen läuft“:** Du siehst, was gerade passiert. Das Löschen lässt sich nach dem laufenden Eintrag anhalten (○ gedrückt halten).
- **Fertig-Seite:** freigeräumter Platz, was übrig blieb, wo das Protokoll liegt, QR-Code für die Rückmeldung.
- **Besser erkannt:** Spieldateien (`.exfat`) und entpackte Spiele ohne Kachel zeigen Namen und Bild des Spiels; PKG-Dateien auf dem USB-Stick mit Spielnamen; Tools ohne Kachel stehen mit ihren Daten zusammen; doppelte Spiele sind deutlich markiert. Was die Konsole selbst auf ein USB-Laufwerk kopiert hat (Screenshots, Videos, Spielstände), ist rot („deine Daten – endgültig weg“).
- **Namen auch bei Resten gelöschter Spiele:** Spielstände, Trophäen, Bilder und Videos von Spielen, die nicht mehr installiert sind, zeigen ihren Namen, wenn eine Quelle ihn belegt (bei PS5-Trophäen die Liste selbst, frühere Protokolle von PS5 Cleanup, Sonys Spielstand-Liste, die PKG-Datei des Spiels). Sonst wenigstens ihr Bild. Geraten wird nie.
- **Apps über ShadowMount** stehen unter Apps, wenn ihre eigenen Angaben sie als App ausweisen. Hat dasselbe Programm zusätzlich eine Kachel unter Apps (gleicher Name), stehen beide in einem Block.
- **Weniger Speicher beim Nachschauen:** Bei sehr vielen Dateien hielt 1.0.7 schon beim Nachschauen an (Rückmeldung von einer PS5 Slim). Behoben.
- **Laufwerks-Kopplung in klaren Worten:** online gekoppelt, ab Werk gekoppelt, keine Kopplung, Koppeln oder Entkoppeln nicht fertig – statt „nicht sicher bestimmbar“.
- **Neue Version:** Wer den QR-Code scannt, sieht, wenn es eine neuere Version gibt.

Getestet auf einer PS5 Pro (CFI-7021) mit Firmware 12.00 und M.2. Andere Modelle und Firmwares sind nicht gesperrt, aber noch nicht getestet – dort bitte im Testmodus zuerst schauen, ob die Liste stimmt. [Alle Neuerungen](https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases)

</details>

## Worum geht's?

Wer seine PS5 mit Jailbreak eine Weile nutzt, kennt das: tote Kacheln, Reste gelöschter Spiele, alte Downloads, Fehlerberichte, Daten von Tools, die man längst nicht mehr nutzt. Die Systemsoftware neu aufspielen will man deswegen aber nicht – erst recht nicht bei Slim und Pro, wo dabei die Laufwerks-Kopplung verloren gehen kann.

**PS5 Cleanup** läuft direkt auf der PS5, listet alles übersichtlich mit Namen auf und löscht nur das, was du auswählst.

**Zwei Wege:**
- **Gezielt** – einzelne Einträge anhaken, z. B. wenn ein Spiel oder Tool Probleme macht: nur dessen Daten, Spielstand oder Trophäen löschen, der Rest bleibt.
- **Vollständige Bereinigung** – alles auf der internen SSD auf einmal, wie frisch eingerichtet (die M.2 wird extra gefragt, Bestätigung mit `LÖSCHEN`).

Beim Start ist der **Testmodus** an: Du kannst alles anschauen, gelöscht wird nichts.

## Was es kann

- **Eigene Kachel** auf dem Startbildschirm, Bedienung mit dem Controller am Fernseher – kein PC, kein Homebrew Launcher nötig
- **Kacheln verschwinden sauber** über Sonys eigenen Deinstallations-Dienst, **Bilder und Videos** über Sonys Löschdienst, **Spielstände** samt Eintrag in Sonys Spielstand-Liste
- **Jailbreak-Schutz:** YouTube (Y2JB), Netflix ('N Hack), Lua-Spiele, PS2-Spielstand-Jailbreak und offline gespeicherter WebKit-Jailbreak werden erkannt, nie automatisch angehakt und vor dem Löschen deutlich gewarnt
- **Übersicht auf einen Blick:** Modell, Firmware, eingerichtete Jailbreak-Einstiege, Laufwerks-Kopplung und freier Speicher
- **Ein Block pro App und Spiel:** Kachel oben, darunter alles, was dazugehört – zugeordnet nur mit Beleg (Kennung, Name oder das Programm der App selbst)
- **Klarnamen und Kachelbilder** in 7 Bereichen: Spiele (PS5 und PS4) mit **Update und DLCs einzeln wählbar**, Apps, Spielstände, Trophäen, Bilder & Videos, Sonstiges, Jailbreak
- **M.2: jedes Spiel und jede Datei einzeln** – die Ordner selbst bleiben stehen, für M.2 und erweiterten Speicher gibt es eine eigene Rückfrage
- **Größe jedes Eintrags direkt in der Liste** (wird im Hintergrund gezählt)
- **ShadowMount-Spiele:** wahlweise nur die Kachel oder Spiel samt Kachel – ShadowMount+ wird vorher sauber angehalten
- **Nachschauen und Anhaken löscht nichts** – gelöscht wird erst nach ausdrücklicher Bestätigung (Nur Auswahl: ✕ 1,5 Sekunden halten, vollständige Bereinigung: `LÖSCHEN` eintippen)
- **Testmodus** beim Start: alles anschauen, nichts löschen – im Options-Menü ausschalten
- **Protokoll** jederzeit mit Options; beim Löschen zusätzlich als Datei auf dem USB-Laufwerk, sonst auf der internen Platte oder der M.2
- **Am Ende:** ✕ Neu starten (empfohlen) oder ○ Beenden
- **Deutsch und Englisch** – richtet sich nach der Systemsprache der PS5, Quadrat schaltet vor dem ersten Nachschauen um

## Was das Programm nicht anfasst

Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, Aktivierung, echte Lizenzen und die Sony-Systemanwendungen. Es wird nichts installiert und nichts zurückgesetzt. Sonys Datenbanken ändert die PS5-App nur gezielt: die Einträge gelöschter Spielstände und kaputte Zusatzinhalt-Angaben eines Spiels, das du löschst. Jede dieser Änderungen wird vorher geprüft und ganz oder gar nicht gemacht.

## Voraussetzungen

- PS5 mit Jailbreak und einem Payload-Lader oder dem Payload Manager
- Kein PC, kein FTP und kein USB-Laufwerk nötig

## So geht's

1. Jailbreak laden, `ps5cleanup_v1.0.8.elf` aus den [Releases](https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases/latest) über den Payload-Lader oder den Payload Manager starten. Steckt ein USB-Laufwerk, kommt das Protokoll darauf.
2. **Nachschauen** und anschauen, was gefunden wurde. Die App ist im **Testmodus** und löscht nichts. Stimmt die Liste, den Testmodus im Options-Menü ausschalten.
3. Einzelne Einträge anhaken und Quadrat = **Nur Auswahl löschen** (zum Bestätigen ✕ 1,5 Sekunden halten) – oder Dreieck = **Vollständige Bereinigung** (`LÖSCHEN` eintippen).
4. Am Ende **✕ Neu starten** (empfohlen) oder **○ Beenden**. Nach dem Neustart den Jailbreak wie gewohnt starten.
   **Kein** „Datenbank neu aufbauen“ (löscht installierte PS4-Spiele, siehe Warnung oben). **Nie** „PS5 zurücksetzen“ oder „Systemsoftware neu installieren“ wählen!

Ausführliche Anleitung: [LIESMICH.txt](LIESMICH.txt) · English: [README_EN.txt](README_EN.txt)

## Rückmeldungen

**Am einfachsten per QR-Code:** Am Ende und unter Options, „Protokoll & Rückmeldung“ zeigt PS5 Cleanup einen QR-Code. Mit dem Handy scannen, auf „Hat geklappt“ oder „Problem“ tippen – ohne Konto, ohne PC. Die PS5 sendet dabei nichts.

Im Link stehen nur Version, Fassung, Bau-Stempel, Modellnummer, Firmware, Laufwerks-Kopplung, eingerichtete Jailbreak-Einstiege, angeschlossene Speicher, ShadowMount, Zahlen je Bereich und bei Fehlern kurze Fehlercodes (Schritt, Art des Eintrags, Ort, Code) – **keine Namen von Spielen oder Apps, keine Spielkennungen, kein Konto, keine Dateien, keine IP der PS5**. Dieselben Angaben stehen in Klartext daneben und noch einmal auf dem Handy; gespeichert wird erst mit „Senden“. Wie bei jedem Webseitenaufruf sieht der Server kurz die Internet-Adresse des Handys – sie wird nicht gespeichert. Die Seite läuft bei Cloudflare unter `feedback.ps5cleanup.workers.dev` ([Datenschutz-Hinweis](https://feedback.ps5cleanup.workers.dev/datenschutz)).

Ausführlicher: Über [Issues](https://github.com/xXBneutzerNameXx/PS5-Cleanup/issues), die [Discussions](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions) (Modell, Firmware und Ergebnis gern unter [Compatibility](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions/categories/compatibility)) oder auf psxtools.de – am besten mit dem **Protokoll**. Das geht auch ohne etwas zu löschen: In der Protokoll-Ansicht (Options, „Protokoll & Rückmeldung“) speichert Quadrat es auf das USB-Laufwerk – auch im Testmodus.

## Warum keine Windows-Fassung mehr?

<details>
<summary>Zum Aufklappen</summary>

<br>

- Die Windows-Fassung arbeitet über FTP. Über FTP lassen sich Kacheln und die Einträge in Mediengalerie und Spielstand-Liste nicht sauber austragen – genau das macht 1.0.7 aus. Sie hätte also nur halb aufräumen können: Leere Kacheln und leere Einträge wären geblieben.
- Wer auf der PS5 einen FTP-Server starten kann, kann dort genauso die PS5-App starten. Sie kann alles, was die Windows-Fassung konnte, und mehr.
- Geplant für die nächste Version: PS5 Cleanup auch vom PC oder Handy aus über den Browser bedienen, wie es inzwischen viele PS5-Tools anbieten (z. B. ShadowMount oder der Payload Manager).
- Der Quellcode der bisherigen Windows-Fassung (1.0.6) bleibt im Ordner `Quellcode/`, wird aber nicht weiterentwickelt.

</details>

## Aufbau

<details>
<summary>Ordner im Quellcode (zum Aufklappen)</summary>

<br>

| Ordner | Inhalt |
|---|---|
| `1.0.7/kern/` | Kern der PS5-App: Schutzliste, Suche, Löschen, Abläufe |
| `1.0.7/app/` | Oberfläche am Fernseher (SDL2, DualSense) und PS5-Anbindung (Sony-Deinstallation, Löschdienst, Spielstand-Liste) |
| `1.0.7/start/` | `ps5cleanup.elf`: Start, eigene Kachel (PSCL00001), Hintergrundteil für Selbstentfernung und Neustart |
| `1.0.7/db/` | Prüfung von Sonys Datenbanken (SQLite) |
| `1.0.7/rueckmeldung/` | Handy-Seite für die Rückmeldung per QR-Code (Cloudflare Worker) |
| `1.0.7/test/` | Tests am PC |
| `1.0.7/fremd/` | Anleitung und Skript für die fremden Bibliotheken (PCRE2, cJSON) |
| `Quellcode/` | die bisherige Windows-Fassung 1.0.6 (C#) und ältere Fassungen – nicht mehr weiterentwickelt |

</details>

## Lizenz

Die PS5-App (`1.0.7/`, `ps5cleanup_v1.0.8.elf`) steht unter der GPL-3.0 (oder später) – Copyright (c) 2026 da_Chris86. Der Code der bisherigen Windows-Fassung steht unter MIT. Details und Bestandteile anderer Urheber in [LICENSE.txt](LICENSE.txt).

## Danke an

[drakmor](https://github.com/drakmor) (ftpsrv, ps5-hwinfo), [earthonion](https://github.com/earthonion) (ddd_status), John Törnblom (ps5-payload-sdk, ftpsrv), [seregonwar](https://github.com/seregonwar) (zftpd), die Macher von ShadowMountPlus und PS5Upload, [Project Nayuki](https://www.nayuki.io/page/qr-code-generator-library) (QR-Code-Bibliothek) sowie Markus Scholtes (PS2EXE).

Entwickelt von da_Chris86 mit Claude (Anthropic). Unabhängige Code-Prüfung: OpenAI Codex. Idee, Vorgaben und die Tests an der echten Konsole kommen von mir.

**Benutzung auf eigene Gefahr – Gelöschtes ist endgültig weg.**
Inoffizielles Community-Projekt, keine Verbindung zu Sony. PlayStation und PS5 sind Marken von Sony Interactive Entertainment Inc.

---

## English

<p align="center"><b><a href="https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases/latest">⬇️ Download PS5 Cleanup 1.0.8</a></b></p>

### At a glance

| 🧹 Clean | 🛡️ Safe | 🎮 Right on your TV |
|---|---|---|
| Tiles, apps and games are removed by Sony itself – no empty tiles, no database rebuild | Jailbreak entry points and PS4 games are protected. Nothing is deleted until you confirm | Own home-screen tile, controlled with the controller – no PC, no FTP, no USB drive needed |
| **📦 One block per app** | **🧪 Test mode** | **📱 Feedback via QR code** |
| The tile at the top, everything that belongs to it below – assigned only with evidence, nothing is ever guessed | The app starts in test mode: it shows everything like real deleting and deletes nothing until you switch it off | Scan with your phone, no account, no personal data |

<details>
<summary><b>📸 What it looks like</b> (screenshots from the PS5, click to expand)</summary>
<br>

<table>
<tr>
<td width="50%"><img src="bilder/1.0.8/ablauf-1-start-en.jpg" alt="PS5 Cleanup start screen"></td>
<td width="50%"><img src="bilder/1.0.8/ablauf-2-auswahl-en.jpg" alt="Selecting entries"></td>
</tr>
<tr>
<td><b>1. Start:</b> Console, firmware, jailbreak, drive pairing and storage at a glance. Scanning changes nothing.</td>
<td><b>2. Selection:</b> Areas on the left, every entry in the middle with its size and a color for what happens afterwards, the explanation on the right.</td>
</tr>
<tr>
<td width="50%"><img src="bilder/1.0.8/ablauf-3-bestaetigen-en.jpg" alt="Confirm page: This will be deleted"></td>
<td width="50%"><img src="bilder/1.0.8/ablauf-4-fertig-en.jpg" alt="Done screen with feedback"></td>
</tr>
<tr>
<td><b>3. Confirm:</b> Before anything happens, it lists exactly what will be deleted and what always stays. Nothing is deleted until you hold ✕.</td>
<td><b>4. Done:</b> Space freed, log on the USB drive and a QR code for feedback.</td>
</tr>
</table>

<sub>The app runs in German and English – switch the language on the start screen.</sub>

</details>

### 🗳️ PS5 Cleanup Community – Your Feedback Matters!

**Which PS5 do you own?** Help us improve PS5 Cleanup across different console models and firmware versions!

[**🗳️ Vote in our poll**](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions/4) · [**💬 Join discussions**](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions) · [**🎮 Report compatibility**](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions/categories/compatibility)

### ⚠️ Which file is which? Please read first!

| File | What for | Deletes? |
|---|---|---|
| **`ps5cleanup_v1.0.8.elf`** | **PS5 app** – runs directly on the PS5, controlled with the DualSense. Starts in **test mode** ("TEST MODE" at the top) | **Only after you switch test mode off** and explicitly confirm |
| **`PS5-Cleanup-1.0.8.zip`** | the ELF file + guides + checksums | – |

> [!TIP]
> **New in 1.0.8: just one file.** The app starts in test mode and shows you exactly what it would delete – without any risk. Only when the list is right, switch test mode off in the Options menu. The former `PS5-Cleanup-Test_v1.0.7.elf` is gone.

> [!WARNING]
> **No more "Rebuild Database":** Sony's "Rebuild Database" (Safe Mode) deletes installed **PS4 games from fake PKGs** on the internal SSD and on the extended storage (USB hard drive for games). The PKG files stay, but the games have to be installed again (observed on 31 Oct 2026 on a PS5 Pro with firmware 12.00; other tools such as elf-arsenal and ps5-db-backup describe the same). Earlier versions of this guide recommended the rebuild – **please do not do it any more.** From 1.0.7 on it is not needed either. Whatever cannot be removed cleanly is named at the end and left unchanged. If an empty tile still stays, please send us the log (see *Feedback* below). **Never** choose "Reset PS5" or "Reinstall System Software".

> [!NOTE]
> **No Windows version any more:** from 1.0.7 on there is no `PS5-Cleanup.exe` – see *Why no Windows version any more?* below.

<details>
<summary><b>✨ New in 1.0.8 – new interface, test mode in the app, confirm deleting by holding</b> (click to expand)</summary>

<br>

- **New interface on the TV:** start screen with console, disc drive pairing, jailbreak and storage per drive. In the selection the areas on the left, the blocks in the middle, a box with image and explanation on the right. Largest entries first (or A–Z), L1/R1 page through.
- **Just one file – with test mode:** the app starts in test mode, "TEST MODE" at the top. It shows everything and deletes nothing until you switch it off in the Options menu.
- **Confirming on its own screen:** what is gone for good, what you have to download or set up again, what costs nothing – and what always stays. Delete selection only: hold ✕ for 1.5 seconds. Full cleanup: type `DELETE` as before.
- **"Scanning" and "Deleting" screens:** you see what is happening. Deleting can be stopped after the current entry (hold ○).
- **Done screen:** space freed, what was left over, where the log is, QR code for feedback.
- **Better detection:** game files (`.exfat`) and unpacked games without a tile show the game's name and picture; PKG files on the USB stick with game names; tools without a tile shown together with their data; duplicate games clearly marked. What the console itself copied to a USB drive (screenshots, videos, saved data) is red ("your data – gone for good").
- **Names for leftovers of deleted games, too:** saved data, trophies, screenshots and videos of games that are no longer installed show their name when a source confirms it (for PS5 trophies the list itself, earlier PS5 Cleanup logs, Sony's saved data list, the game's PKG file). Otherwise at least their picture. Nothing is guessed.
- **Apps via ShadowMount** are listed under Apps when their own data says they are an app. If the same program also has a tile under Apps (same name), both are shown in one block.
- **Less memory while scanning:** with very many files 1.0.7 already stopped while scanning (feedback from a PS5 Slim). Fixed.
- **Disc drive pairing in plain words:** paired online, factory paired, no pairing, pairing or unpairing not finished – instead of "not reliably determinable".
- **New version:** scanning the QR code shows you when there is a newer version.

Tested on a PS5 Pro (CFI-7021), firmware 12.00, with M.2. Other models and firmwares are not blocked but untested – there, please check in test mode first that the list is right.

</details>

### What is it?

**PS5 Cleanup** brings a jailbroken PS5 back to a freshly-set-up state – without reinstalling or resetting the system software. It runs **directly on the PS5** (`ps5cleanup_v1.0.8.elf`, own home-screen tile, no PC and no Homebrew Launcher needed, DualSense on your TV), lists everything with readable names and deletes only what you select – single games, saved data, trophies (e.g. when one game or tool causes problems) or everything at once. Available in **English and German** (follows the PS5 system language; Square switches it before the first scan).

### Features

- Tiles removed via Sony's uninstall service, screenshots and videos via Sony's delete service, saved data together with its entry in the saved data list
- Jailbreak detection and protection (YouTube/Y2JB, Netflix 'N Hack, Lua games, PS2 save jailbreak, offline WebKit jailbreak)
- Dashboard for model, firmware, jailbreak entry points, disc drive pairing and free storage
- One block per app and game (tile at the top, everything belonging to it below, assigned only with evidence – ID, name or the app's own program)
- Readable names and tile images in 7 areas, games (PS5 and PS4) with update and each DLC selectable separately
- Every game and file on the M.2 listed individually (folders are kept)
- Size of every entry shown in the list
- ShadowMount games: tile only or game + tile, ShadowMount+ is stopped cleanly first
- Scanning and ticking deletes nothing – nothing is deleted until you confirm (delete selection only: hold ✕ for 1.5 seconds, full cleanup: type `DELETE`)
- Test mode at start: look at everything, delete nothing – switch it off in the Options menu
- Log any time with Options, saved as a file when deleting
- At the end: restart (recommended) or quit

**Not touched by the program:** system software, disc drive pairing, user account, activation and real licenses, Sony system apps. Nothing is installed or reset. The PS5 app changes Sony's databases only selectively: the entries of deleted saved data and broken add-on information of a game you delete. Each of these changes is checked first and made completely or not at all.

**Requirements:** jailbroken PS5 with a payload loader or the Payload Manager. No PC, no FTP and no USB drive needed.

### How to use

1. Load your jailbreak, start `ps5cleanup_v1.0.8.elf` from the [releases](https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases/latest) with your payload loader or Payload Manager.
2. *Scan* and look at what was found. The app is in **test mode** and deletes nothing. If the list is right, switch test mode off in the Options menu.
3. Tick entries and Square = *Delete selection only* (hold ✕ for 1.5 seconds to confirm) – or Triangle = *Full cleanup* (type `DELETE`).
4. At the end ✕ *Restart* (recommended) or ○ *Quit*, then start your jailbreak as usual.
   **No** *Rebuild Database* (it deletes installed PS4 games, see the warning above). **Never** choose *Reset PS5* or *Reinstall System Software*.

Full guide: [README_EN.txt](README_EN.txt)

### Feedback

**Easiest via QR code** – at the end and under Options, "Log & feedback", scan it with your phone and tap "It worked" or "Problem", no account needed. The PS5 sends nothing.

The link only contains version, edition, build stamp, model number, firmware, drive pairing, jailbreak entry points, connected storage, ShadowMount, numbers per area and, for errors, short error codes (step, kind of entry, location, code) – **no game or app names, no title IDs, no account, no files, no PS5 IP**. The same details are shown in plain text next to it and on your phone; nothing is stored until you tap "Send". As with any website visit, the server briefly sees your phone's internet address – it is not stored. The page runs on Cloudflare at `feedback.ps5cleanup.workers.dev` ([privacy notice](https://feedback.ps5cleanup.workers.dev/datenschutz?l=en)).

More detail: via [issues](https://github.com/xXBneutzerNameXx/PS5-Cleanup/issues), [Discussions](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions) (model, firmware and result under [Compatibility](https://github.com/xXBneutzerNameXx/PS5-Cleanup/discussions/categories/compatibility)) or on psxtools.de – ideally with the **log**. This also works without deleting anything: in the log view (Options, "Log & feedback"), Square saves it to the USB drive – in test mode too.

### Why no Windows version any more?

<details>
<summary>Click to expand</summary>

<br>

The Windows version works over FTP. Over FTP, tiles and the entries in the Media Gallery and the saved data list cannot be removed cleanly – exactly what 1.0.7 is about – so it could only have done half the job. Anyone who can start an FTP server on the PS5 can just as well start the PS5 app, which can do everything the Windows version could, and more. Planned for the next version: control PS5 Cleanup from your PC or phone in the browser, as many PS5 tools offer by now (e.g. ShadowMount or the Payload Manager). The source code of the previous Windows version (1.0.6) stays in `Quellcode/` but is no longer developed.

</details>

### License and thanks

The PS5 app (`1.0.7/`, `ps5cleanup_v1.0.8.elf`) is GPL-3.0-or-later (c) 2026 da_Chris86; the code of the former Windows version is MIT – details and third-party components in [LICENSE.txt](LICENSE.txt).

Thanks to [drakmor](https://github.com/drakmor) (ftpsrv, ps5-hwinfo), [earthonion](https://github.com/earthonion) (ddd_status), John Törnblom (ps5-payload-sdk, ftpsrv), [seregonwar](https://github.com/seregonwar) (zftpd), the makers of ShadowMountPlus and PS5Upload, [Project Nayuki](https://www.nayuki.io/page/qr-code-generator-library) (QR code library) and Markus Scholtes (PS2EXE).

Developed by da_Chris86 with Claude (Anthropic). Independent code review: OpenAI Codex. Idea, requirements and real-console testing by da_Chris86.

**Use at your own risk – deleted data is gone for good.** Unofficial community project, not affiliated with Sony. PlayStation and PS5 are trademarks of Sony Interactive Entertainment Inc.
