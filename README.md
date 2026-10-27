# PS5 Cleanup

<img width="600" height="600" alt="image" src="https://github.com/user-attachments/assets/5bc025d3-b6b1-47f6-9f27-4ea7dd483b52" />


**Deine PS5 aufräumen wie frisch eingerichtet – ohne Neuinstallation, ohne Zurücksetzen.**
*Clean up your PS5 as if freshly set up – no reinstall, no reset. [English below](#english)*

> **Beta** – getestet auf einer PS5 Pro (CFI-7021) mit Firmware 12.00 und M.2.
> Andere Modelle und Firmwares sollten laufen, sind aber noch nicht getestet.
> Rückmeldungen am einfachsten über den Knopf **„Feedback / Kompatibilität melden“** im Programm – er bereitet ein [Issue](../../issues) vor, das du vor dem Absenden siehst und änderst. Es wird nichts automatisch gesendet.
>
> Erfahrungen, Fragen und Ideen gern auch in den [Discussions](../../discussions) – Modell, Firmware und Ergebnis unter [Compatibility](../../discussions/categories/compatibility).

Wer seine PS5 mit Jailbreak eine Weile nutzt, kennt das: tote Kacheln, Reste gelöschter Spiele, alte Downloads, Fehlerberichte, Daten von Tools, die man längst nicht mehr nutzt. Die Systemsoftware neu aufspielen will man deswegen aber nicht – erst recht nicht bei Slim und Pro, wo dabei die Laufwerks-Kopplung verloren gehen kann.

**PS5 Cleanup** ist ein kleines Windows-Programm, das per FTP auf die PS5 schaut, alles übersichtlich mit Namen auflistet und nur das löscht, was du auswählst.

**Zwei Wege:**
- **Gezielt** – einzelne Einträge anhaken, z. B. wenn ein Spiel oder Tool Probleme macht: nur dessen Daten, Spielstand oder Trophäen löschen, der Rest bleibt.
- **Vollständige Bereinigung** – alles auf der internen SSD auf einmal, wie frisch eingerichtet (die M.2 wird extra gefragt, Bestätigung mit `LÖSCHEN`).

## Was es kann

- Findet die PS5 im Heimnetz selbst – auch den passenden FTP-Port (2121 oder 2120) – und erkennt Modell, Firmware und ob das Laufwerk gekoppelt ist
- Erkennt den FTP-Server samt Version (soweit der Server sie nennt) und stellt sich selbst auf dessen Art der Datenverbindung ein
- Übersicht auf einen Blick: Konsole, Laufwerks-Kopplung mit Statuslegende und Speicher; farbige Hinweise (grün = geschützt, blau = Info, gelb = Hinweis, rot = endgültig)
- Startet im **Testmodus**: erst nur anschauen, es wird nichts gelöscht
- Zeigt alles mit Klarnamen: Spiele (PS5 und PS4) mit **Update und DLCs einzeln wählbar**, Spielstände, Trophäen, Screenshots und Videos je Spiel, Downloads, Reste, Jailbreak-Tools
- **M.2: jedes Spiel und jede Datei einzeln** – die Ordner selbst bleiben stehen, für die M.2 gibt es eine eigene Rückfrage
- **Größe jedes Eintrags direkt in der Liste** (wird im Hintergrund gezählt) und ein **Abbrechen-Knopf** beim Suchen und Zählen
- **Deutsch und Englisch** – richtet sich nach der Windows-Sprache, umschaltbar oben rechts
- ShadowMount-Spiele: wahlweise nur die Kachel oder Spiel samt Kachel – ShadowMount+ wird vorher sauber angehalten
- Räumt die Kachel-Infos mit weg – keine leeren Kacheln mit Schloss nach dem Datenbank-Neuaufbau
- Zeigt vorher, wie viel Platz frei wird, und hinterher, wie viel freigeräumt wurde
- Schreibt ein Protokoll, das man bei Problemen mitschicken kann (mit „Alles kopieren“ und eigenem Fenster)
- **Feedback / Kompatibilität melden**: freiwilliger Bericht zum Prüfen und Ändern – ohne IP-Adressen, Benutzernamen oder Pfade, gesendet wird erst, wenn du es auf GitHub selbst absendest
- Weist beim Start darauf hin, wenn es auf GitHub eine neuere Version gibt

## Was das Programm nicht anfasst

Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, Aktivierung, echte Lizenzen und die Sony-Systemanwendungen. Es wird nichts installiert, nichts zurückgesetzt und keine Systemdatenbank verändert.

## Voraussetzungen

- PS5 mit Jailbreak und laufendem FTP-Server – getestet: [ftpsrv von drakmor](https://github.com/drakmor) 1.16 (Port 2121), [ftpsrv](https://github.com/ps5-payload-dev/ftpsrv) 0.21.1 (Port 2121), [zftpd](https://github.com/seregonwar/zftpd) 1.6.0 (Port 2120). „PS5 im Netz suchen“ findet beide Ports, ein anderer Port geht per `IP:Port`.
- Windows 10/11 – eine einzelne `.exe`, nichts zu installieren
- Optional für Modell- und Laufwerkserkennung: Payload-Lader (Port 9021) oder Payload Manager (Port 8084)

## So geht's

1. Jailbreak laden, `PS5-Cleanup.exe` aus den [Releases](../../releases) starten
2. **PS5 im Netz suchen**, dann **Verbinden und nachschauen**
3. Im Testmodus alles anschauen, dann den Haken *Testmodus* entfernen und löschen
4. Danach an der PS5: ausschalten, Einschaltknopf halten bis zum zweiten Piepton, Controller per USB anstecken, **Datenbank neu erstellen**.
   **Nie** „PS5 zurücksetzen“ oder „Systemsoftware neu installieren“ wählen!

Ausführliche Anleitung: [LIESMICH.txt](LIESMICH.txt) · English: [README_EN.txt](README_EN.txt)

## Hinweis zum Virenscanner

Seit 1.0.4 ist PS5 Cleanup ein echtes Windows-Programm (C#, .NET Framework 4.8 – in Windows 10/11 schon enthalten), kein verpacktes Skript mehr. Die `.exe` ist nicht digital signiert; einzelne Virenscanner melden neue, unsignierte Programme trotzdem manchmal fälschlich. Der komplette Quellcode liegt in diesem Repository – du kannst die `.exe` mit `Quellcode/bauen.cmd` selbst bauen (braucht nur Windows, nichts zu installieren).

## Aufbau

| Datei | Inhalt |
|---|---|
| `Quellcode/Kern.cs`, `Suche.cs`, `Ausfuehren.cs` | Schutzliste, Suche, Löschen |
| `Quellcode/Ftp.cs`, `Sitzung.cs` | FTP-Verbindung zur PS5 |
| `Quellcode/Fenster.cs`, `Fenster.xaml`, `Programm.cs` | Oberfläche und Start |
| `Quellcode/Sprache.cs` | Deutsch / Englisch |
| `Quellcode/Feedback.cs` | freiwilliger Feedback-Bericht (nichts wird automatisch gesendet) |
| `Quellcode/Selbsttest.cs`, `UiTest.cs` | Selbsttest (`PS5-Cleanup.exe -SelbstTest`) und Oberflächentest für die Entwicklung |
| `Quellcode/bauen.cmd` | baut die `.exe` mit dem in Windows enthaltenen Compiler |
| `Quellcode/ps5info.c`, `ps5info_antwort.h` | kleines PS5-Programm: liest Modell und Laufwerks-Status (nur lesend) |
| `Quellcode/powershell-1.0.3/` | die bisherige PowerShell-Fassung (1.0.3) samt Textversion |

## Lizenz

MIT – Copyright (c) 2026 da_Chris86, siehe [LICENSE.txt](LICENSE.txt).
`ps5info` steht unter der GPL-3.0 (und die alte PowerShell-Fassung nutzte PS2EXE, Ms-LPL) – Details ebenfalls in [LICENSE.txt](LICENSE.txt).

## Danke an

[drakmor](https://github.com/drakmor) (ftpsrv, ps5-hwinfo), [earthonion](https://github.com/earthonion) (ddd_status), John Törnblom (ps5-payload-sdk, ftpsrv), [seregonwar](https://github.com/seregonwar) (zftpd), die Macher von ShadowMountPlus und PS5Upload sowie Markus Scholtes (PS2EXE).

Entstanden mit Unterstützung der KI Claude (Anthropic). Idee, Vorgaben und die Tests an der echten Konsole kommen von mir.

**Benutzung auf eigene Gefahr – Gelöschtes ist endgültig weg.**
Inoffizielles Community-Projekt, keine Verbindung zu Sony. PlayStation und PS5 sind Marken von Sony Interactive Entertainment Inc.

---

## English

**PS5 Cleanup** brings a jailbroken PS5 back to a freshly-set-up state – without reinstalling or resetting the system software. It connects over FTP, lists everything with readable names and deletes only what you select – single games, saved data, trophies (e.g. when one game or tool causes problems) or everything at once. The program is available in **English and German** (follows the Windows language, switch at the top right).

> **Beta** – tested on a PS5 Pro (CFI-7021), firmware 12.00, with M.2. Other models and firmwares should work but are untested. The easiest way to report results is the **"Send Feedback / Compatibility Report"** button in the program: it prepares an [issue](../../issues) that you can check and edit before submitting. Nothing is sent automatically. Experiences, questions and ideas are also welcome in [Discussions](../../discussions) – model, firmware and result under [Compatibility](../../discussions/categories/compatibility).

**Features:** finds the PS5 on your network including the right FTP port (2121 or 2120) and detects model, firmware and disc drive pairing status · detects the FTP server and its version (if the server reports it) and adapts to its kind of data connection · dashboard for console, disc drive pairing (with status legend) and storage · starts in test mode (view only) · readable names for games (PS5 and PS4) with update and each DLC selectable separately, saves, trophies, captures per game, downloads, leftovers, jailbreak tools · every game and file on the M.2 listed individually (folders are kept) · size of every entry shown in the list, abort button · update hint · *Full cleanup* (M.2 asked separately, confirmed by typing `DELETE`) or per-item selection · ShadowMount games: tile only or game + tile, ShadowMount+ is stopped cleanly first · removes tile metadata so no locked ghost tiles remain after rebuilding the database · shows space to be freed and space freed · writes a log for bug reports ("Copy all", separate window) · voluntary feedback report to check and edit, without IP addresses, user names or paths – only submitted if you submit it on GitHub yourself.

**Not touched by the program:** system software, disc drive pairing, user account, activation and real licenses, Sony system apps. Nothing is installed or reset, and no system database is modified.

**Requirements:** jailbroken PS5 with an FTP server (tested: ftpsrv by drakmor 1.16 and ftpsrv 0.21.1 on port 2121, zftpd 1.6.0 on port 2120 – "Find PS5 on network" finds both ports) · Windows 10/11, single `.exe`, no installation · optional for model/drive detection: payload loader (port 9021) or Payload Manager (port 8084).

**Usage:** start `PS5-Cleanup.exe` → *Find PS5 on network* → *Connect and scan* → review in test mode, untick *Test mode*, then *Delete selection only* or *Full cleanup* → on the PS5: Safe Mode → *Rebuild Database*. **Never** choose *Reset PS5* or *Reinstall System Software*. Full guide: [README_EN.txt](README_EN.txt).

**Antivirus:** since 1.0.4 the program is a native Windows application (C#, .NET Framework 4.8), no longer a packed script. The `.exe` is not code-signed, so a few scanners may still flag it as a false positive. The full source is in this repository – build it yourself with `Quellcode/bauen.cmd` (Windows only, nothing to install).

**License:** MIT (c) 2026 da_Chris86 – `ps5info` is GPL-3.0, see [LICENSE.txt](LICENSE.txt). Made with the help of the AI Claude (Anthropic); idea, requirements and real-console testing by da_Chris86.

**Use at your own risk – deleted data is gone for good.** Unofficial community project, not affiliated with Sony. PlayStation and PS5 are trademarks of Sony Interactive Entertainment Inc.
