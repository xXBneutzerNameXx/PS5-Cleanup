# PS5 Cleanup

**Deine PS5 aufräumen wie frisch eingerichtet – ohne Neuinstallation, ohne Zurücksetzen.**
*Clean up your PS5 as if freshly set up – no reinstall, no reset. [English below](#english)*

> **Beta** – getestet auf einer PS5 Pro (CFI-7021) mit Firmware 12.00 und M.2.
> Andere Modelle und Firmwares sollten laufen, sind aber noch nicht getestet.
> Rückmeldungen gern als [Issue](../../issues) – mit Modell, Firmware und der Datei `PS5-Cleanup.log`.

Wer seine PS5 mit Jailbreak eine Weile nutzt, kennt das: tote Kacheln, Reste gelöschter Spiele, alte Downloads, Fehlerberichte, Daten von Tools, die man längst nicht mehr nutzt. Die Systemsoftware neu aufspielen will man deswegen aber nicht – erst recht nicht bei Slim und Pro, wo dabei die Laufwerks-Kopplung verloren gehen kann.

**PS5 Cleanup** ist ein kleines Windows-Programm, das per FTP auf die PS5 schaut, alles übersichtlich mit Namen auflistet und nur das löscht, was du auswählst.

## Was es kann

- Findet die PS5 im Heimnetz selbst und erkennt Modell, Firmware und ob das Laufwerk gekoppelt ist
- Startet im **Testmodus**: erst nur anschauen, es wird nichts gelöscht
- Zeigt alles mit Klarnamen: Spiele (PS5 und PS4) mit **Update und DLCs einzeln wählbar**, Spielstände, Trophäen, Screenshots und Videos je Spiel, Downloads, Reste, Jailbreak-Tools, Inhalt der M.2
- Zwei Wege: **KOMPLETT** (wie neu eingerichtet, die M.2 wird extra gefragt) oder gezielt per Häkchen
- ShadowMount-Spiele: wahlweise nur die Kachel oder Spiel samt Kachel – ShadowMount+ wird vorher sauber angehalten
- Räumt die Kachel-Infos mit weg – keine leeren Kacheln mit Schloss nach dem Datenbank-Neuaufbau
- Zeigt vorher, wie viel Platz frei wird, und hinterher, wie viel freigeräumt wurde
- Schreibt ein Protokoll, das man bei Problemen mitschicken kann

## Was immer bleibt

Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, Aktivierung, echte Lizenzen und die Sony-Systemanwendungen. Es wird nichts installiert, nichts zurückgesetzt und keine Systemdatenbank verändert.

## Voraussetzungen

- PS5 mit Jailbreak und laufendem FTP-Server (Standard-Port 2121, anderer Port per `IP:Port`)
- Windows 10/11 – eine einzelne `.exe`, nichts zu installieren
- Optional für Modell- und Laufwerkserkennung: Payload-Lader (Port 9021) oder Payload Manager (Port 8084)

## So geht's

1. Jailbreak laden, `PS5-Cleanup.exe` aus den [Releases](../../releases) starten
2. **PS5 im Netz suchen**, dann **Verbinden und nachschauen**
3. Im Testmodus alles anschauen, dann den Haken *Testmodus* entfernen und löschen
4. Danach an der PS5: ausschalten, Einschaltknopf halten bis zum zweiten Piepton, Controller per USB anstecken, **Datenbank neu erstellen**.
   **Nie** „PS5 zurücksetzen“ oder „Systemsoftware neu installieren“ wählen!

Ausführliche Anleitung: [LIESMICH.txt](LIESMICH.txt)

## Hinweis zum Virenscanner

Die `.exe` ist ein mit [PS2EXE](https://github.com/MScholtes/PS2EXE) verpacktes PowerShell-Skript. Einzelne Virenscanner melden so etwas fälschlich als verdächtig. Der komplette Quellcode liegt in diesem Repository – du kannst die `.exe` mit `Quellcode/bauen/bauen.ps1` selbst bauen oder die Textversion `Quellcode/PS5_Cleanup.bat` nutzen.

## Aufbau

| Datei | Inhalt |
|---|---|
| `Quellcode/ps5_kern.ps1` | gemeinsamer Kern: FTP, Suche, Schutzliste, Löschen |
| `Quellcode/ps5_cleanup_gui.ps1` | Fenster (Quelltext der `.exe`) |
| `Quellcode/ps5_cleanup.ps1` | Textversion (`PS5_Cleanup.bat`) |
| `Quellcode/ps5info.c` | kleines PS5-Programm: liest Modell und Laufwerks-Status (nur lesend) |
| `Quellcode/bauen/` | baut die `.exe` (`bauen.ps1`, PS2EXE, Symbol) |

## Lizenz

MIT – Copyright (c) 2026 da_Chris86, siehe [LICENSE.txt](LICENSE.txt).
`ps5info` steht unter der GPL-3.0, PS2EXE unter der Ms-LPL – Details ebenfalls in [LICENSE.txt](LICENSE.txt).

## Danke an

[drakmor](https://github.com/drakmor) (ftpsrv, ps5-hwinfo), [earthonion](https://github.com/earthonion) (ddd_status), John Törnblom (ps5-payload-sdk), die Macher von ShadowMountPlus und PS5Upload sowie Markus Scholtes (PS2EXE).

Entstanden mit Unterstützung der KI Claude (Anthropic). Idee, Vorgaben und die Tests an der echten Konsole kommen von mir.

**Benutzung auf eigene Gefahr – Gelöschtes ist endgültig weg.**
Inoffizielles Community-Projekt, keine Verbindung zu Sony. PlayStation und PS5 sind Marken von Sony Interactive Entertainment Inc.

---

## English

**PS5 Cleanup** brings a jailbroken PS5 back to a freshly-set-up state – without reinstalling or resetting the system software. It connects over FTP, lists everything with readable names and deletes only what you select. The program itself is in German.

> **Beta** – tested on a PS5 Pro (CFI-7021), firmware 12.00, with M.2. Other models and firmwares should work but are untested. Please report results as an [issue](../../issues) with model, firmware and `PS5-Cleanup.log`.

**Features:** finds the PS5 on your network and detects model, firmware and disc drive pairing status · starts in test mode (view only) · readable names for games (PS5 and PS4) with update and each DLC selectable separately, saves, trophies, captures per game, downloads, leftovers, jailbreak tools, M.2 content · *KOMPLETT* (full clean, M.2 asked separately) or per-item selection · ShadowMount games: tile only or game + tile, ShadowMount+ is stopped cleanly first · removes tile metadata so no locked ghost tiles remain after rebuilding the database · shows space to be freed and space freed · writes a log for bug reports.

**Always kept:** system software, disc drive pairing, user account, activation and real licenses, Sony system apps. Nothing is installed or reset, and no system database is modified.

**Requirements:** jailbroken PS5 with an FTP server (default port 2121) · Windows 10/11, single `.exe`, no installation · optional for model/drive detection: payload loader (port 9021) or Payload Manager (port 8084).

**Usage:** start `PS5-Cleanup.exe` → *PS5 im Netz suchen* (find PS5) → *Verbinden und nachschauen* (connect and scan) → review in test mode, untick *Testmodus*, delete → on the PS5: Safe Mode → *Rebuild Database*. **Never** choose *Reset PS5* or *Reinstall System Software*.

**Antivirus:** the `.exe` is a PowerShell script packed with PS2EXE; a few scanners flag such files as false positives. The full source is in this repository – build it yourself with `Quellcode/bauen/bauen.ps1`.

**License:** MIT (c) 2026 da_Chris86 – `ps5info` is GPL-3.0, PS2EXE is Ms-LPL, see [LICENSE.txt](LICENSE.txt). Made with the help of the AI Claude (Anthropic); idea, requirements and real-console testing by da_Chris86.

**Use at your own risk – deleted data is gone for good.** Unofficial community project, not affiliated with Sony. PlayStation and PS5 are trademarks of Sony Interactive Entertainment Inc.
