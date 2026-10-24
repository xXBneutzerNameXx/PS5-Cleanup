# Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
# =====================================================================
# PS5 Cleanup by da_Chris86  -  Zustand wie nach einer Neuinstallation  (PowerShell)
#
# KEINE Neuinstallation und KEIN Zurücksetzen! Das Skript löscht nur
# ausgewählte Inhalte, so dass die PS5 danach fast wie neu eingerichtet ist.
# Die LAUFWERKS-KOPPLUNG wird dabei NICHT berührt:
#   PS5 Slim (CFI-20xx/21xx) und PS5 Pro (CFI-70xx): Laufwerk gekoppelt
#   erste PS5 (CFI-10xx/11xx/12xx): Laufwerk fest eingebaut, keine Kopplung
# Benutzung auf eigene Gefahr.
#
# BLEIBT IMMER:  Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, Lizenzen,
#                Sony-Systemanwendungen
# ZWEI WEGE:     1 KOMPLETT  - alles auf der internen SSD (M.2 wird extra gefragt)
#                2 ERWEITERT - jeden Bereich einzeln wählen:
#                  1 Spiele & Apps   2 Spielstände   3 Trophäen
#                  4 Bilder & Videos 5 Downloads, Zusatzinhalte & Reste
#                  6 Jailbreak-Tools & Homebrew-Daten  7 Inhalt der M.2-SSD
#
# MODELL/FIRMWARE: Firmware per FTP aus /system_ex/etc/index.dat. Das Modell
#                fragt ps5info.elf (liegt daneben) ab. Es wird über den
#                Payload-Lader (Port 9021) gestartet, ersatzweise über den
#                Payload-Manager (Port 8084) - nur wenn der Nutzer zustimmt.
#                Sonst / klappt es nicht, wird nach der Nummer gefragt.
#
# VORAUSSETZUNG: PS5 mit geladenem Jailbreak und laufendem FTP-Server
#                (z.B. ftpsrv, Standard-Port 2121), PC im selben Netzwerk.
#                Keine Zusatzprogramme nötig (nur Windows-PowerShell).
#
# Aufruf:  PS5_Cleanup.bat            -> normal (mit Fragen)
#          PS5_Cleanup.bat -Test      -> zeigt nur, was gelöscht werden könnte
# =====================================================================

param([switch]$Test)

# gemeinsamer Kern (FTP, Schutz, Suche, Löschen) liegt daneben
. (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) 'ps5_kern.ps1')

# Mitschrift für die Fehlersuche: alles, was hier steht, landet auch in einer .log-Datei
$LOGDATEI = Join-Path $SkriptOrdner ('PS5-Cleanup_Text_' + (Get-Date -Format 'yyyy-MM-dd_HH-mm-ss') + '.log')
try { Start-Transcript -Path $LOGDATEI -Force | Out-Null } catch { $LOGDATEI = '' }

# ---------------------------------------------------------------------
# Ein-/Ausgabe
# ---------------------------------------------------------------------
function Frage([string]$text) {
    Write-Host -NoNewline $text
    $zeile = [Console]::In.ReadLine()
    if ($null -eq $zeile) { return '' }
    return $zeile.Trim()
}

# ---------------------------------------------------------------------
# Start
# ---------------------------------------------------------------------
$linie = '=' * 70
Write-Host $linie
Write-Host ' PS5 Cleanup by da_Chris86 - Zustand wie nach einer Neuinstallation'
Write-Host $linie
Write-Host ''
Write-Host ' WICHTIG: Das ist KEINE Neuinstallation und KEIN Zurücksetzen.'
Write-Host ' Es werden nur die Inhalte gelöscht, die du gleich auswählst. Danach'
Write-Host ' ist die PS5 in einem Zustand wie frisch eingerichtet.'
Write-Host ''
Write-Host ' LAUFWERKS-KOPPLUNG: Bei PS5 Slim (CFI-20xx/21xx) und PS5 Pro (CFI-70xx)'
Write-Host ' ist das abnehmbare Disc-Laufwerk mit der Konsole gekoppelt. Nichts, was'
Write-Host ' dieses Skript tut, hat mit dieser Kopplung zu tun - sie wird NICHT'
Write-Host ' gefährdet. Systemsoftware wird nicht angefasst, nichts neu installiert,'
Write-Host ' kein Update gemacht. Die erste PS5 (CFI-10xx/11xx/12xx) hat ein fest'
Write-Host ' eingebautes Laufwerk ohne Kopplung, Digital Editions haben gar keins -'
Write-Host ' dort spielt das keine Rolle. Geeignet für alle PS5-Modelle mit Jailbreak.'
Write-Host ''
Write-Host ' Bleibt immer: Systemsoftware, Kopplung, Benutzerkonto und Lizenzen.'
Write-Host ''
Write-Host ' BENUTZUNG AUF EIGENE GEFAHR. Gelöschtes ist endgültig weg.'
Write-Host ' Inoffizielles Community-Projekt von da_Chris86, keine Verbindung zu Sony.'
Write-Host ''
Write-Host $linie
Write-Host ''

# --- IP ---
while ($true) {
    Write-Host 'IP-Adresse der PS5 eingeben (PS5: Einstellungen > Netzwerk >'
    Write-Host "Verbindungsstatus anzeigen). Anderer FTP-Port als ${STANDARD_PORT}? Dann: IP:Port"
    $eingabe = (Frage 'IP: ') -replace '\s', ''
    if ($eingabe -eq '') { Write-Host 'Abgebrochen.'; exit 1 }
    $ip = $eingabe; $port = $STANDARD_PORT
    if ($eingabe -match '^(.+):(\d+)$') { $ip = $Matches[1]; $port = [int]$Matches[2] }
    if ($ip -match '^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$') { break }
    Write-Host '  Das ist keine gültige IP-Adresse (Beispiel: 192.168.1.50). Nochmal bitte.'
    Write-Host ''
}

Write-Host ''
Write-Host "Verbinde mit PS5 ($ip, Port $port) ..."
$ok = $false
try { $ok = Ftp-Verbinden $ip $port } catch { $ok = $false }
if (-not $ok -or $null -eq (Liste '/user')) {
    Write-Host ''
    Write-Host 'FEHLER: PS5 nicht erreichbar.'
    Write-Host "Ist die PS5 an, der Jailbreak geladen und läuft der FTP-Server (Port $port)?"
    exit 1
}
Write-Host 'Verbunden.'
Write-Host ''

# --- Modell und Firmware ---
$firmware = Firmware-Lesen
$modell = ''
Write-Host 'Modell automatisch erkennen? Dafür wird kurz ein kleines Programm'
Write-Host '(ps5info.elf) auf der PS5 gestartet und danach wieder entfernt.'
$mj = Frage 'Automatisch erkennen? (Enter/j = ja, n = nein): '
if ($mj -ne 'n' -and $mj -ne 'N') {
    Write-Host 'Frage das Modell ab ...'
    $modell = Modell-Lesen $ip
}
if ($modell -eq '') {
    Write-Host '  Modell konnte nicht automatisch erkannt werden.'
    Write-Host '  Die Modellnummer steht auf dem Aufkleber der Konsole oder unter'
    Write-Host '  Einstellungen > System > Systemsoftware > Konsoleninformationen.'
    $modell = (Frage '  Modellnummer (z.B. CFI-7016), Enter = überspringen: ') -replace '\s', ''
}
$script:MODELL_TYP = Modell-Typ $modell
$script:MODELL_DIGITAL = Modell-Digital $modell
$reihe = Modell-Reihe $script:MODELL_TYP $script:MODELL_DIGITAL
if ($modell -eq '') { $modell = 'unbekannt' }
Write-Host ''
Write-Host $linie
Write-Host " Modell:    $modell"
Write-Host " Reihe:     $reihe"
Write-Host " Firmware:  $firmware"
Write-Host " Laufwerk:  $(Laufwerk-Text)"
Write-Host $linie
Write-Host ''
Write-Host 'Schaue nach, was drauf ist ...'
Write-Host ''

try { Suche-Funde } catch {
    Write-Host ''
    Write-Host "FEHLER beim Nachschauen: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'Es wurde nichts verändert. Bitte die PS5 und den FTP-Server prüfen und nochmal starten.'
    Ftp-Trennen
    if ($LOGDATEI -ne '') { try { Stop-Transcript | Out-Null } catch { } }
    exit 1
}
$Funde = $script:Funde; $KatName = $script:KatName; $KatInfo = $script:KatInfo; $m2Da = $script:m2Da
function Zeige-Kat([int]$k) {
    $n = 1
    foreach ($f in ($Funde | Where-Object { $_.Kat -eq $k })) {
        Write-Host ('    {0,3}) {1}' -f $n, $f.Text); $n++
    }
}
function Kat-Kopf([int]$k) {
    Write-Host ''
    Write-Host ('-' * 70)
    Write-Host " [$k] $($KatName[$k])"
    Write-Host ('-' * 70)
    foreach ($z in @($KatInfo[$k])) { Write-Host $z }
    if ($k -eq 1 -and @($Funde | Where-Object { $_.Kat -eq 1 -and $_.Quelle -ne '' }).Count -gt 0) {
        Write-Host ''
        Write-Host ' "ShadowMount-Spiel": Im Menü ist nur eine Kachel (Verknüpfung), das'
        Write-Host ' Spiel selbst liegt auf der M.2 oder der internen SSD. Nach der Auswahl'
        Write-Host ' kommt EINE Frage, ob die Spieldateien auch gelöscht werden sollen.'
    }
    if ($k -eq 1 -and @($Funde | Where-Object { $_.Kat -eq 1 -and $_.Text -like '*tote Kachel*' }).Count -gt 0) {
        Write-Host ' "tote Kachel": Das Spiel ist schon weg, nur die Kachel ist noch da.'
    }
    if (@($Funde | Where-Object { $_.Kat -eq $k -and $_.Text -like '*Kachel geht mit*' }).Count -gt 0) {
        Write-Host ' Einträge mit [Spiel: ...] sind ShadowMount-Spiele - die Kachel geht mit.'
    }
    Write-Host ''
}
$ALLE_KAT = $script:ALLE_KAT
if ($script:KOMPRIMIERUNG.Count -gt 0) {
    Write-Host $linie
    Write-Host ' ACHTUNG: Der Game Compressor arbeitet gerade (oder wurde abgebrochen):'
    foreach ($z in $script:KOMPRIMIERUNG) { Write-Host "   $z" }
    Write-Host ' Diese Zwischendateien werden nicht angeboten, das betroffene Spiel ist'
    Write-Host ' GESPERRT. Lass den Game Compressor erst fertig werden.'
    Write-Host $linie
    Write-Host ''
}

if ($Funde.Count -eq 0) {
    Write-Host 'Nichts zu tun - die PS5 ist bereits sauber.'
    Kopplungs-Hinweis
    exit 0
}

if ($Test) {
    foreach ($k in $ALLE_KAT) {
        if ((Anzahl $k) -eq 0) { continue }
        Kat-Kopf $k
        Zeige-Kat $k
    }
    Write-Host ''
    Write-Host '(Testmodus - es wurde nichts verändert)'
    exit 0
}

# ---------------------------------------------------------------------
# Art der Säuberung
# ---------------------------------------------------------------------
Write-Host 'Gefunden:'
foreach ($k in $ALLE_KAT) {
    if ($k -eq 7 -and -not $m2Da) { Write-Host ('  [7] {0,-45} (keine M.2 gefunden)' -f $KatName[7]); continue }
    Write-Host ('  [{0}] {1,-45} {2,4} Einträge' -f $k, $KatName[$k], (Anzahl $k))
}
Write-Host ''
Write-Host 'Wie soll gesäubert werden?'
Write-Host '  1 = KOMPLETT   - alles auf der internen SSD löschen, wie neu installiert'
Write-Host '                   (die M.2 wird extra gefragt)'
Write-Host '  2 = ERWEITERT  - jeden Bereich einzeln durchgehen und selbst wählen'
Write-Host '  0 = abbrechen'
Kopplungs-Hinweis
$modus = Frage 'Auswahl: '
$KOMPLETT = $false

if ($modus -eq '1') {
    $KOMPLETT = $true
    Write-Host ''
    Write-Host $linie
    Write-Host ' KOMPLETT: Auf der internen SSD wird ALLES gelöscht:'
    foreach ($k in 1..6) { Write-Host ('   - {0,-45} {1,4}' -f $KatName[$k], (Anzahl-Komplett $k)) }
    Write-Host ''
    Write-Host ' Das heißt:'
    Write-Host '   - Spielstände ohne Cloud-Sicherung (PS Plus) sind endgültig weg.'
    Write-Host '   - Nie mit dem PSN abgeglichene Trophäen sind weg.'
    Write-Host '   - Die Jailbreak-Tools samt Einstellungen sind weg - danach musst du'
    Write-Host '     deinen Jailbreak NEU EINRICHTEN.'
    Write-Host '   - Wer per Browser-Seite jailbreakt: ein offline gespeicherter Zugang'
    Write-Host '     im Browserspeicher kann auch weg sein.'
    Write-Host ''
    Write-Host ' Bleibt: Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, Lizenzen'
    Write-Host '         und die Sony-Systemanwendungen.'
    if (-not $m2Da) {
        Write-Host ''
        Write-Host ' HINWEIS: Es wurde keine M.2-SSD gefunden. Steckt eine M.2 mit Spielen in' -ForegroundColor Yellow
        Write-Host ' der PS5, die gerade nicht erkannt wird: gleich NICHT LÖSCHEN eintippen,' -ForegroundColor Yellow
        Write-Host ' sondern die PS5 neu starten. Sonst gelten diese Spiele als nicht' -ForegroundColor Yellow
        Write-Host ' installiert und ihre Lizenzen und Kacheln werden mit gelöscht.' -ForegroundColor Yellow
    }
    Write-Host $linie
    $m2j = 'n'
    if ($m2Da -and (Anzahl 7) -gt 0) {
        Write-Host ''
        Write-Host "Auf der M.2-SSD liegen $(Anzahl 7) Einträge:"
        Zeige-Kat 7
        Write-Host ''
        Write-Host ' Enter oder n = M.2 BEHALTEN (empfohlen): Spiele bleiben, nur die Kacheln gehen'
        Write-Host '                weg und kommen wieder, sobald ShadowMount neu eingerichtet ist'
        Write-Host ' M2LEEREN      = M.2 auch leeren: alle Spiele und Dateien darauf endgültig weg'
        $m2j = Frage ' Deine Wahl: '
    }
    $m2Leeren = ($m2j -ceq 'M2LEEREN')
    Waehle-Komplett $m2Leeren
    if (-not $m2Leeren -and (Hat-Pakete)) {
        Write-Host ' (Trophäen-Pakete der Spiele bleiben, weil die Spiele auf der M.2 bleiben.)'
    }
} elseif ($modus -eq '2') {
    foreach ($k in $ALLE_KAT) {
        if ((Anzahl $k) -eq 0) { continue }
        Kat-Kopf $k
        Zeige-Kat $k
        Write-Host ''
        Write-Host '  Enter = behalten | k = alle löschen | Nummern (z.B.: 1 3 4) = nur diese'
        $a = Frage '  Auswahl: '
        $liste = @($Funde | Where-Object { $_.Kat -eq $k })
        if ($a -eq 'k' -or $a -eq 'K') {
            foreach ($f in $liste) { if ($f.Gesperrt) { Write-Host "    (gesperrt, bleibt: $($f.Text))" } else { $f.Wahl = $true } }
        } elseif ($a -ne '') {
            foreach ($z in ($a -split '[\s,]+')) {
                if ($z -match '^\d{1,4}$' -and [int]$z -ge 1 -and [int]$z -le $liste.Count) {
                    $f = $liste[[int]$z - 1]
                    if ($f.Gesperrt) { Write-Host "    ($z ist gesperrt, bleibt)" } else { $f.Wahl = $true }
                }
                elseif ($z -ne '') { Write-Host "    ($z ignoriert)" }
            }
        }
    }
    # ShadowMount-Spiele aus Bereich 1: einmal fragen, ob die Spieldateien mitgehen.
    $smEintraege = SM-Gewaehlt
    if ($smEintraege.Count -gt 0) {
        Write-Host ''
        Write-Host ('-' * 70)
        Write-Host " Du hast $($smEintraege.Count) ShadowMount-Spiel(e) gewählt:"
        foreach ($f in $smEintraege) { Write-Host "     $($f.Text)" }
        Write-Host ''
        Write-Host '   Enter oder n = nur die Kacheln löschen (empfohlen, Spiele bleiben)'
        Write-Host '   SPIELE       = Spiele UND Kacheln löschen (Spieldateien endgültig weg)'
        $sj = Frage '  Deine Wahl: '
        Setze-SM-Spieldateien ($sj -ceq 'SPIELE')
    }
} else {
    Write-Host 'Abgebrochen - nichts verändert.'
    exit 0
}

# ---------------------------------------------------------------------
# Zusammenfassung und Bestätigung
# ---------------------------------------------------------------------
# Spiel gewählt -> Update und Zusatzinhalte gehen mit (ohne Spiel nutzlos)
$vorher = (Gewaehlt).Count
Abhaengige-Mitnehmen
$dazu = (Gewaehlt).Count - $vorher
if ($dazu -gt 0) { Write-Host ''; Write-Host " ($dazu Update(s)/Zusatzinhalt(e) gehen automatisch mit, weil ihr Spiel gelöscht wird.)" }
$gewaehlt = Gewaehlt
Write-Host ''
Write-Host $linie
if ($gewaehlt.Count -eq 0) { Write-Host ' Nichts ausgewählt - nichts verändert.'; exit 0 }
Write-Host " Wird ENDGÜLTIG gelöscht: $($gewaehlt.Count) Einträge"
foreach ($k in $ALLE_KAT) {
    $c = @($gewaehlt | Where-Object { $_.Kat -eq $k }).Count
    if ($c -gt 0) { Write-Host ('   - {0,-45} {1,4}' -f $KatName[$k], $c) }
}
if ($KOMPLETT -and -not (@($gewaehlt | Where-Object { $_.Kat -eq 7 }).Count -gt 0)) {
    Write-Host '   (M.2-SSD bleibt unverändert)'
}
$gesp = @($Funde | Where-Object { $_.Gesperrt })
if ($gesp.Count -gt 0) {
    Write-Host ''
    Write-Host " Bleibt (Game Compressor arbeitet gerade daran): $($gesp.Count) Einträge"
}
if (-not $KOMPLETT) {
    Write-Host ''
    Write-Host ' Im Einzelnen:'
    foreach ($k in $ALLE_KAT) {
        foreach ($f in ($gewaehlt | Where-Object { $_.Kat -eq $k })) { Write-Host "   [$k] $($f.Text)" }
    }
}
Write-Host ''
Write-Host ' Ermittle die Größe ...'
$gross = Ermittle-Groessen
Write-Host " Das sind $(Groesse-Satz $gross)."
Write-Host ''
Kopplungs-Hinweis
Write-Host ' Benutzung auf eigene Gefahr - Gelöschtes ist endgültig weg.'
Write-Host $linie
if ($KOMPLETT) {
    if ($m2Leeren) { Write-Host " !!! ACHTUNG: DIE M.2-SSD WIRD MIT GELEERT - $(Anzahl 7) Einträge, alle Spiele darauf sind danach weg !!!" -ForegroundColor Red }
    else { Write-Host ' Die M.2-SSD bleibt unangetastet.' }
    $antwort = Frage 'Zum Bestätigen LÖSCHEN eintippen (alles andere = abbrechen): '
    if ($antwort -cne 'LOESCHEN' -and $antwort -cne 'LÖSCHEN') { Write-Host 'Abgebrochen - nichts verändert.'; exit 0 }
} else {
    $antwort = Frage 'Jetzt endgültig löschen? (j/n): '
    if ($antwort -ne 'j' -and $antwort -ne 'J') { Write-Host 'Abgebrochen - nichts verändert.'; exit 0 }
}

# ---------------------------------------------------------------------
# Ausführen
# ---------------------------------------------------------------------
Write-Host ''
$erg = Fuehre-Loeschung { $w = Frage ' Trotzdem weitermachen? (j/n): '; return ($w -eq 'j' -or $w -eq 'J') }
if ($erg.Abgebrochen) { Write-Host 'Abgebrochen - nichts verändert.'; exit 0 }
$fehl = $erg.Fehler; $smGestoppt = $erg.SmGestoppt

Write-Host ''
Write-Host $linie
if ($fehl -eq 0) { Write-Host ' Fertig - alles Ausgewählte ist gelöscht.' }
else             { Write-Host " Fertig, aber mit $fehl Fehler(n) - siehe oben." }
Write-Host " $(Freigeraeumt-Text $erg.Frei)"
Kopplungs-Hinweis
Write-Host $linie
Write-Host ''
if ($erg.SmText) {
    Write-Host $erg.SmText
    Write-Host ''
}
foreach ($z in $script:SCHLUSS_TEXT) { Write-Host $z }
Ftp-Trennen
if ($LOGDATEI -ne '') { Write-Host ''; Write-Host "Protokoll dieses Laufs: $LOGDATEI"; try { Stop-Transcript | Out-Null } catch { } }
