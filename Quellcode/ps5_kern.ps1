# Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
# =====================================================================
# ps5_kern.ps1  -  gemeinsamer Kern von PS5 Cleanup by da_Chris86 (Text- und Fensterversion)
#
# Wird von ps5_cleanup.ps1 (Text) und ps5_cleanup_gui.ps1 (Fenster) per
# Dot-Sourcing eingebunden. Enthält: FTP-Client, Schutzliste (Erlaubt),
# Löschen, Modell/Firmware, ShadowMount-Stopp, die Suche (Suche-Funde),
# die Auswahl-Helfer und die Ausführung (Fuehre-Loeschung).
# Ausgaben gehen über $script:Melde (Standard: Write-Host), damit das
# Fenster sie in sein Protokoll umleiten kann.
# =====================================================================
$ErrorActionPreference = 'Stop'
$VERSION       = '1.0.2 Beta'
$STANDARD_PORT = 2121
$BEHALTEN_APP  = 'PPSA01325'      # Astro's Playroom
$M2            = '/mnt/ext1'
$SYS_META      = '/system_data/priv/appmeta'   # hier merkt sich die PS5 jede Kachel
$script:ASTRO_NPWRS = @('NPWR20188_00')        # Trophäen-Kennung(en) von Astro (die Suche liest sie nach)
$UTF8          = New-Object System.Text.UTF8Encoding $false
# Ordner, in dem ps5info.elf und LIESMICH liegen: neben dem Skript - oder,
# als .exe gebaut (ps2exe), neben der .exe
$SkriptOrdner  = $PSScriptRoot
if ([string]::IsNullOrEmpty($SkriptOrdner)) { $SkriptOrdner = Split-Path -Parent ([System.Diagnostics.Process]::GetCurrentProcess().MainModule.FileName) }
try { [Console]::OutputEncoding = $UTF8 } catch { }

# Ausgabe-Kanal: Text-Version schreibt auf die Konsole, das Fenster ins Protokoll
$script:Melde = { param([string]$t) Write-Host $t }
# Warten: das Fenster hält dabei seine Oberfläche am Leben
$script:Warte = { param([int]$ms) Start-Sleep -Milliseconds $ms }
# Wird aufgerufen, solange auf eine Antwort der PS5 gewartet wird (das Fenster
# zeichnet sich dabei weiter - sonst meldet Windows "Keine Rückmeldung")
$script:Pumpe = { }
$script:FTP_TIMEOUT = 30000      # ms, beim Löschen höher (große Dateien)

# ---------------------------------------------------------------------
# Kleiner FTP-Client (eine Steuerverbindung, UTF-8 - Sonderzeichen heil)
# ---------------------------------------------------------------------
$script:Ftp = $null

# Eine Zeile von der Steuerverbindung lesen, ohne das Programm zu blockieren:
# solange nichts da ist, wird die Oberfläche am Leben gehalten.
$script:FtpPuffer = New-Object System.Collections.Generic.List[byte]
function Ftp-Zeile {
    $st = $script:Ftp.Stream
    $start = Get-Date
    while ($true) {
        $i = $script:FtpPuffer.IndexOf([byte]10)
        if ($i -ge 0) {
            $roh = $script:FtpPuffer.GetRange(0, $i).ToArray()
            $script:FtpPuffer.RemoveRange(0, $i + 1)
            $len = $roh.Length; if ($len -gt 0 -and $roh[$len - 1] -eq 13) { $len-- }
            return $UTF8.GetString($roh, 0, $len)
        }
        if ($st.DataAvailable) {
            $buf = New-Object byte[] 8192
            $n = $st.Read($buf, 0, $buf.Length)
            if ($n -le 0) { throw 'FTP-Verbindung unterbrochen' }
            $teil = New-Object byte[] $n; [Array]::Copy($buf, $teil, $n)
            $script:FtpPuffer.AddRange($teil)
            $start = Get-Date
            continue
        }
        if (-not $script:Ftp.Tcp.Connected) { throw 'FTP-Verbindung unterbrochen' }
        if (((Get-Date) - $start).TotalMilliseconds -gt $script:FTP_TIMEOUT) { throw 'Die PS5 antwortet nicht (Zeitüberschreitung)' }
        & $script:Pumpe ([int]((Get-Date) - $start).TotalMilliseconds)
        # bis zu 20 ms auf Daten warten - kommt etwas an, geht es sofort weiter.
        # Meldet der Socket "lesbar", aber es liegen keine Daten an, hat die PS5
        # die Verbindung geschlossen (sonst würde hier bis zum Zeitlimit gewartet).
        if ($script:Ftp.Tcp.Client.Poll(20000, [System.Net.Sockets.SelectMode]::SelectRead) -and
            $script:Ftp.Tcp.Client.Available -eq 0 -and -not $st.DataAvailable) { throw 'FTP-Verbindung unterbrochen' }
    }
}

function Ftp-Antwort {
    $zeile = Ftp-Zeile
    $text = $zeile
    if ($zeile.Length -ge 4 -and $zeile[3] -eq '-') {
        $code = $zeile.Substring(0, 3)
        while ($true) {
            $zeile = Ftp-Zeile
            $text += "`n" + $zeile
            if ($zeile.StartsWith("$code ")) { break }
        }
    }
    return [pscustomobject]@{ Code = [int]$text.Substring(0, 3); Text = $text }
}

function Ftp-Befehl([string]$cmd) {
    # Steuerzeichen (z.B. ein Zeilenumbruch in einem Dateinamen) würden einen
    # zweiten, ungewollten Befehl erzeugen - nie senden.
    if ($cmd -match '[\x00-\x1f\x7f]') { throw "Ungültiges Zeichen im FTP-Befehl: $($cmd -replace '[\x00-\x1f\x7f]', '?')" }
    $script:Ftp.Writer.Write($cmd + "`r`n")
    $script:Ftp.Writer.Flush()
    return Ftp-Antwort
}

function Ftp-Verbinden([string]$ip, [int]$port) {
    if ($null -ne $script:Ftp) { try { $script:Ftp.Tcp.Close() } catch { } }   # alte (evtl. hängende) Verbindung schließen
    $script:Ftp = $null                                   # klappt die neue nicht, bleibt keine alte übrig
    $tcp = New-Object System.Net.Sockets.TcpClient
    $ar = $tcp.BeginConnect($ip, $port, $null, $null)
    if (-not $ar.AsyncWaitHandle.WaitOne(5000)) { $tcp.Close(); return $false }
    try { $tcp.EndConnect($ar) } catch { $tcp.Close(); return $false }
    $tcp.ReceiveTimeout = 30000
    $st = $tcp.GetStream()
    $script:FtpPuffer.Clear()
    $script:Ftp = [pscustomobject]@{
        Ip = $ip; Port = $port; Tcp = $tcp; Stream = $st
        Writer = New-Object System.IO.StreamWriter($st, $UTF8)
    }
    $null = Ftp-Antwort                                   # 220 Begrüßung
    $a = Ftp-Befehl 'USER anonymous'
    if ($a.Code -eq 331) { $a = Ftp-Befehl 'PASS anonymous' }
    if ($a.Code -ge 400) { return $false }
    $null = Ftp-Befehl 'TYPE I'
    return $true
}

function Ftp-Daten([string]$cmd) {
    # Datenverbindung (EPSV), liefert die Bytes oder $null bei Fehler
    $a = Ftp-Befehl 'EPSV'
    if ($a.Code -ne 229 -or $a.Text -notmatch '\|\|\|(\d+)\|') { return $null }
    $dport = [int]$Matches[1]
    $d = New-Object System.Net.Sockets.TcpClient
    $d.Connect($script:Ftp.Ip, $dport)
    $d.ReceiveTimeout = 30000
    $a = Ftp-Befehl $cmd
    if ($a.Code -ge 400) { $d.Close(); return $null }
    $ms = New-Object System.IO.MemoryStream
    $d.GetStream().CopyTo($ms)
    $d.Close()
    $a = Ftp-Antwort                                      # 226 = vollständig übertragen
    if ($a.Code -ge 400) { return $null }                 # abgebrochen - Inhalt unvollständig
    return ,$ms.ToArray()
}

# Prüft, ob die Steuerverbindung sauber antwortet (Befehl und Antwort passen
# zusammen). Liefert $false, wenn die Verbindung aus dem Takt oder tot ist.
function Ftp-Gesund {
    try {
        $a = Ftp-Befehl 'NOOP'
        if ($a.Code -ne 200) { return $false }
        $a = Ftp-Befehl 'PWD'
        if ($a.Code -ne 257) { return $false }
        return ($null -ne (Liste '/user'))
    } catch { return $false }
}
function Ftp-Cwd([string]$pfad) {
    return ((Ftp-Befehl "CWD $pfad").Code -lt 400)
}

# Ordnerinhalt: Liste von Objekten {Name, IstOrdner}; $null wenn es den Ordner nicht gibt
# $streng: nur "gibt es nicht" (550) liefert $null, jeder andere Fehler bricht ab.
# Für Listen, aus denen geschlossen wird "Spiel ist nicht mehr da" - eine
# gescheiterte Liste darf nie wie eine leere aussehen.
function Liste([string]$pfad, [bool]$streng = $false) {
    $a = Ftp-Befehl "CWD $pfad"
    if ($a.Code -ge 400) {
        if ($streng -and $a.Code -ne 550) { throw "PS5 meldet beim Lesen von $pfad : $($a.Text)" }
        return $null
    }
    $bytes = Ftp-Daten 'LIST'
    $null = Ftp-Cwd '/'
    if ($null -eq $bytes) {
        if ($streng) { throw "Ordner $pfad konnte nicht vollständig gelesen werden - bitte nochmal verbinden." }
        return $null
    }
    $erg = @()
    foreach ($z in ($UTF8.GetString($bytes) -split "`r?`n")) {
        if ($z.Trim() -eq '') { continue }
        $teile = $z -split ' +', 9
        if ($teile.Count -lt 9) { continue }
        $name = $teile[8]
        if ($name -eq '.' -or $name -eq '..') { continue }
        if ($name -match '[\x00-\x1f\x7f]') { continue }      # Steuerzeichen im Namen: nie anfassen
        $groesse = 0; if ($teile[4] -match '^\d+$') { $groesse = [int64]$teile[4] }
        # 'l' = Verknüpfung (Symlink): nie als Ordner behandeln, nie hineinschauen.
        # Manche Server hängen das Ziel an ("name -> ziel") - das gehört nicht zum Namen.
        if ($z.StartsWith('l') -and $name.Contains(' -> ')) { $name = $name.Substring(0, $name.IndexOf(' -> ')) }
        $erg += [pscustomobject]@{ Name = $name; IstOrdner = $z.StartsWith('d'); IstLink = $z.StartsWith('l'); Groesse = $groesse }
    }
    return ,$erg
}

function Namen([string]$pfad, [bool]$streng = $false) {
    $l = Liste $pfad $streng
    if ($null -eq $l) { return @() }
    return ,@($l | ForEach-Object { $_.Name })
}

# Gibt es den Pfad? Nur ein klares "gibt es nicht" (550) zählt als nein -
# jeder andere Fehler bricht ab, statt fälschlich "schon weg" zu melden.
function Existiert([string]$pfad) {
    $eltern = $pfad.Substring(0, $pfad.LastIndexOf('/'))
    if ($eltern -eq '') { $eltern = '/' }
    $name = $pfad.Substring($pfad.LastIndexOf('/') + 1)
    $a = Ftp-Befehl "CWD $eltern"
    if ($a.Code -eq 550) { return $false }
    if ($a.Code -ge 400) { throw "PS5 meldet beim Prüfen von $pfad : $($a.Text)" }
    $bytes = Ftp-Daten 'LIST'
    $null = Ftp-Cwd '/'
    if ($null -eq $bytes) { throw "Ordner $eltern konnte nicht gelesen werden" }
    foreach ($z in ($UTF8.GetString($bytes) -split "`r?`n")) {
        $teile = $z -split ' +', 9
        if ($teile.Count -ge 9 -and $teile[8] -ceq $name) { return $true }
    }
    return $false
}

function Datei-Text([string]$pfad) {
    $b = Ftp-Daten "RETR $pfad"
    if ($null -eq $b) { return '' }
    return $UTF8.GetString($b)
}

# Kleine Datei auf die PS5 schreiben (STOR). Liefert $true bei Erfolg.
function Datei-Schreiben([string]$pfad, [byte[]]$bytes) {
    $a = Ftp-Befehl 'EPSV'
    if ($a.Code -ne 229 -or $a.Text -notmatch '\|\|\|(\d+)\|') { return $false }
    $dport = [int]$Matches[1]
    $d = New-Object System.Net.Sockets.TcpClient
    $d.Connect($script:Ftp.Ip, $dport)
    $a = Ftp-Befehl "STOR $pfad"
    if ($a.Code -ge 400) { $d.Close(); return $false }
    if ($null -eq $bytes) { $bytes = New-Object byte[] 0 }
    $st = $d.GetStream(); $st.Write($bytes, 0, $bytes.Length); $st.Flush(); $d.Close()
    $a = Ftp-Antwort                                        # 226
    return ($a.Code -lt 400)
}

# ---------------------------------------------------------------------
# ShadowMount anhalten
# ---------------------------------------------------------------------
# ShadowMount sucht alle 15 s nach Spielen und legt gelöschte Kacheln sofort
# wieder an. ShadowMount+ (ab 1.7) hat dafür eine eingebaute Stopp-Datei:
# /data/shadowmount/STOP - es löscht die Datei und beendet sich sauber.
$SM_ORDNER = '/user/data/shadowmount'
function ShadowMount-Laeuft {
    # Läuft es, holt es sich die STOP-Datei innerhalb von ~3 s selbst ab.
    if ($null -eq (Liste $SM_ORDNER)) { return $false }
    if (-not (Datei-Schreiben "$SM_ORDNER/STOP" $null)) { return $false }
    $ende = (Get-Date).AddSeconds(8)
    while ((Get-Date) -lt $ende) {
        & $script:Warte 500
        if (-not (Existiert "$SM_ORDNER/STOP")) { return $true }
    }
    $null = Ftp-Befehl "DELE $SM_ORDNER/STOP"               # niemand hat sie abgeholt
    return $false
}
$script:SM_ANGEHALTEN = $false      # nur $true, wenn ShadowMount wirklich lief und beendet wurde
function ShadowMount-Anhalten {
    $script:SM_ANGEHALTEN = $false
    & $script:Melde 'Halte ShadowMount an (sonst legt es die Kacheln sofort wieder an) ...'
    # nur eine Abmeldung NACH dem Anhalten zählt (alte Zeilen im Protokoll nicht)
    $vorher = ([regex]::Matches((Datei-Text "$SM_ORDNER/debug.log"), '\[SHUTDOWN\] cleanup complete')).Count
    $laeuft = ShadowMount-Laeuft
    if (-not $laeuft) { & $script:Melde '  ShadowMount läuft nicht (oder ist älter als 1.7 - dann vorher'; & $script:Melde '  von Hand beenden: PS5 neu starten und nur den FTP-Server laden).'; return $true }
    # Es hat die STOP-Datei abgeholt - jetzt bis zum Ende des Aufräumens warten.
    $ende = (Get-Date).AddSeconds(90)
    while ((Get-Date) -lt $ende) {
        & $script:Warte 2000
        $log = Datei-Text "$SM_ORDNER/debug.log"
        if (([regex]::Matches($log, '\[SHUTDOWN\] cleanup complete')).Count -gt $vorher) { & $script:Melde '  ShadowMount ist beendet.'; $script:SM_ANGEHALTEN = $true; return $true }
    }
    & $script:Melde '  ShadowMount hat sich nach 90 s nicht sauber beendet.'
    return $false
}

# ---------------------------------------------------------------------
# Löschen
# ---------------------------------------------------------------------
function Freigeben([string]$d) {
    $null = Ftp-Befehl "SITE CHMOD 777 $d"
    $l = Liste $d
    if ($null -eq $l) { return }
    foreach ($e in $l) { if ($e.IstOrdner) { Freigeben "$d/$($e.Name)" } }
}

function Lösche-Baum([string]$d) {
    $l = Liste $d
    if ($null -ne $l) {
        foreach ($e in $l) {
            $p = "$d/$($e.Name)"
            if ($e.IstOrdner) { Lösche-Baum $p }
            else {
                $null = Ftp-Befehl "SITE CHMOD 777 $p"
                $null = Ftp-Befehl "DELE $p"
            }
        }
    }
    $null = Ftp-Befehl "RMD $d"
}

$script:ASTRO_IDS = @($BEHALTEN_APP)     # Astro unter allen Kennungen (die Suche ergänzt weitere Länderfassungen)
# Gehört der Pfad zu Astro (App, Daten, Spielstand, Update, DLC, Trophäen-
# fortschritt)? Eigene Aufnahmen aus Astro in der Mediengalerie zählen nicht dazu.
function Astro-Pfad([string]$p) {
    if ($p -notlike '/user/av_contents/*') { foreach ($a in $script:ASTRO_IDS) { if ($p -match "/$([regex]::Escape($a))(/|$|[-_.])") { return $true } } }
    foreach ($n in $script:ASTRO_NPWRS) { if ($p -match "^/user/home/[^/]+/.*/$([regex]::Escape($n))(/|$)") { return $true } }
    return $false
}
function Erlaubt([string]$p) {
    if ($p -match '(^|/)\.\.?(/|$)' -or $p -match '//' -or $p -match '\s$' -or $p -match '[\x00-\x1f\x7f]' -or $p -notmatch '^/') { return $false }
    # Astro's Playroom: nie, egal wo und unter welcher Länderkennung
    # (eigene Aufnahmen aus Astro in der Mediengalerie sind davon ausgenommen)
    if (Astro-Pfad $p) { return $false }
    # M.2-Verwaltung: nur einzelne Spiele/Updates, nie die Ordner selbst
    if ($p -match "^$M2/user/(app|patch)/[^/]+$") { return ($p -notmatch "/(NPXS|$BEHALTEN_APP)") }
    if ($p -match "^$M2/user/addcont/[A-Z]{4}\d{5}(/[^/]+)?$") { return ($p -notmatch "/(NPXS|$BEHALTEN_APP)") }
    # Kachel-Infos der PS5: nur einzelne Spiel-/App-Kennungen, nie NPXS, Astro, external
    if ($p -match "^$SYS_META/[A-Z]{4}\d{5}$") { return ($p -notmatch '/NPXS' -and $p -ne "$SYS_META/$BEHALTEN_APP") }
    # Merkzettel für Spiele auf externen USB-Platten (nur einzelne Spiel-Kennungen)
    if ($p -match "^$SYS_META/external/[A-Z]{4}\d{5}$") { return ($p -notmatch '/NPXS') }
    # Fake-Lizenzen des Jailbreaks (act.dat und nobackup/ = echte Lizenzen, nie)
    if ($p -match '^/user/license/fake[^/]+\.(rif|idx)$') { return $true }
    # Share-Einstellungen einzelner Spiele (trophy/ und die .mst-Datei nie)
    if ($p -match '^/user/system/share/[A-Z]{4}\d{5}$') { return ($p -notmatch '/NPXS' -and $p -ne "/user/system/share/$BEHALTEN_APP") }
    # Store-Bildercache
    if ($p -match '^/user/catalog_downloader/(conceptmeta|appmeta)/[^/]+$') { return $true }
    # temporäre Dateien: nur Inhalte, nie die Ordner selbst
    if ($p -match '^/user/(bgft/trash|savedata/temp|temp/gls/screenshot|temp/video_recording|temp/common_temp/telemetry)/[^/]+$') { return $true }
    # Fehlerberichte: nur die einzelnen .json-Dateien
    if ($p -match '^/system_data/priv/error/history/[^/]+\.json$') { return $true }
    # Trophäen-Pakete der Spiele (Astro nie)
    if ($p -match '^/user/(trophy/conf|trophy2/nobackup/conf|np_uds/nobackup/conf)/(NPWR\d{5}_\d{2})$') { return ($script:ASTRO_NPWRS -notcontains $Matches[2]) }
    # niemals: Systemordner, Konto, Lizenzen, Astro, Sony-Systemanwendungen
    $gesperrt = @(
        "^/user/app/$BEHALTEN_APP(/|$)", "^/user/appmeta/$BEHALTEN_APP(/|$)",
        "^/user/patch/$BEHALTEN_APP(/|$)", "^/user/addcont/$BEHALTEN_APP(/|$)",
        '^/user/appmeta/external$', '^/user/appmeta/addcont$',
        "^/user/home/[^/]+/savedata[^/]*/$BEHALTEN_APP(/|$)",
        '^/user/home/[^/]+/savedata[^/]*/(NPXS|sce_)',
        "^$M2/user(/|$)", "^$M2/system_data(/|$)", "^$M2/lost\+found(/|$)",
        '/app/NPXS', '/appmeta/NPXS', '/patch/NPXS', '/addcont/NPXS', '^/user/download/NPXS',
        '^/user/license/(act\.dat|nobackup)', '^/user/system/share/(trophy|share_)',
        '^/user/catalog_downloader/[^/]+$'
    )
    foreach ($g in $gesperrt) { if ($p -match $g) { return $false } }
    $erlaubt = @(
        '^/user/app/[^/]+', '^/user/appmeta/[^/]+', '^/user/patch/[^/]+',
        '^/user/home/[^/]+/savedata/[^/]+', '^/user/home/[^/]+/savedata_prospero/[^/]+',
        '^/user/home/[^/]+/trophy/[^/]+', '^/user/home/[^/]+/trophy2/[^/]+',
        '^/user/home/[^/]+/webkit/shell/local$',
        '^/user/av_contents/[^/]+/[^/]+',
        '^/user/data/[^/]+', '^/user/download/[^/]+', '^/user/addcont/[^/]+',
        '^/user/_zu_loeschen_[^/]+', "^$M2/[^/]+"
    )
    foreach ($e in $erlaubt) { if ($p -match $e) { return $true } }
    return $false
}

# Steckt in einem Ordner auf dem Weg eine Verknüpfung (Symlink)? Dann könnte
# der Pfad ganz woanders hinzeigen (z.B. auf die M.2) - die Schutzliste prüft
# aber nur den Text. Solche Pfade werden nie gelöscht. (Der letzte Teil selbst
# darf eine Verknüpfung sein - DELE entfernt dann nur die Verknüpfung.)
$script:LINK_CACHE = @{}
function Ohne-Verknuepfung([string]$p) {
    $teile = $p.Split('/')
    $weg = ''
    for ($i = 1; $i -lt $teile.Count - 1; $i++) {
        $eltern = if ($weg -eq '') { '/' } else { $weg }
        $weg = "$weg/$($teile[$i])"
        if (-not $script:LINK_CACHE.ContainsKey($eltern)) { $script:LINK_CACHE[$eltern] = Liste $eltern $true }
        $l = $script:LINK_CACHE[$eltern]
        if ($null -eq $l) { return $true }                  # gibt es nicht -> nichts zu löschen
        $e = $l | Where-Object { $_.Name -ceq $teile[$i] } | Select-Object -First 1
        # nicht gefunden, obwohl der Ordner darüber da ist: nur weitermachen, wenn
        # es den ganzen Pfad wirklich nicht gibt (sonst lieber nicht löschen)
        if ($null -eq $e) { return (-not (Existiert $p)) }
        if ($e.IstLink -or -not $e.IstOrdner) { return $false }
    }
    return $true
}
$script:WAR_SCHON_WEG = 0            # Pfade, die beim Löschen gar nicht mehr da waren
function Loeschen([string]$p) {
    if (-not (Erlaubt $p)) {
        & $script:Melde "  SICHERHEITSSTOPP: $p ist gesperrt - nichts gelöscht."
        return $false
    }
    if (-not (Ohne-Verknuepfung $p)) {
        & $script:Melde "  SICHERHEITSSTOPP: $p liegt hinter einer Verknüpfung - nichts gelöscht."
        return $false
    }
    if (-not (Existiert $p)) { $script:WAR_SCHON_WEG++; return $true }
    # Ist der Eintrag selbst eine Verknüpfung: nur sie entfernen, nie ihr Ziel
    $eltern = $p.Substring(0, $p.LastIndexOf('/')); if ($eltern -eq '') { $eltern = '/' }
    $selbst = (Liste $eltern $true) | Where-Object { $_.Name -ceq $p.Substring($p.LastIndexOf('/') + 1) } | Select-Object -First 1
    if ($null -ne $selbst -and $selbst.IstLink) {
        $null = Ftp-Befehl "DELE $p"
        return (-not (Existiert $p))
    }
    $null = Ftp-Befehl "SITE CHMOD 777 $p"
    $null = Ftp-Befehl "DELE $p"                          # falls Datei
    if (-not (Existiert $p)) { return $true }
    Freigeben $p                                           # Ordner
    $null = Ftp-Befehl "RMDA $p"
    if (Existiert $p) { Lösche-Baum $p }
    return (-not (Existiert $p))
}

# ---------------------------------------------------------------------
# Modell / Firmware
# ---------------------------------------------------------------------
function Firmware-Lesen {
    $t = Datei-Text '/system_ex/etc/index.dat'
    if ($t -notmatch 'release:\s*0x([0-9a-fA-F]{8})') { return 'unbekannt' }
    $hex = $Matches[1]
    return ('{0}.{1}' -f $hex.Substring(0, 2).TrimStart('0'), $hex.Substring(2, 2))
}

function Port-Offen([string]$ip, [int]$port) {
    try {
        $t = New-Object System.Net.Sockets.TcpClient
        $ar = $t.BeginConnect($ip, $port, $null, $null)
        $ok = $ar.AsyncWaitHandle.WaitOne(2000) -and $t.Connected
        $t.Close()
        return $ok
    } catch { return $false }
}

function Http-Post([string]$url, [byte[]]$body) {
    try {
        $req = [System.Net.HttpWebRequest]::Create($url)
        $req.Method = 'POST'
        $req.Timeout = 15000
        $req.Headers.Add('X-PLDMGR-Request', '1')
        $req.ContentType = 'application/octet-stream'
        if ($null -eq $body) { $body = New-Object byte[] 0 }
        $req.ContentLength = $body.Length
        $s = $req.GetRequestStream(); $s.Write($body, 0, $body.Length); $s.Close()
        $resp = $req.GetResponse(); $resp.Close()
        return $true
    } catch { return $false }
}

$INFO_DATEI = '/data/ps5info.txt'
# Kopplungs-Status des abnehmbaren Laufwerks aus dem Payload. $null = noch nicht
# gelesen, -1 = kein abnehmbares Laufwerk / nicht lesbar, 0..6 siehe Laufwerk-Text.
$script:DDD_STATUS = $null
# Liest aus der Payload-Ausgabe Modell und Laufwerks-Status; Modell wird zurück-
# gegeben, der Status landet in $script:DDD_STATUS.
function Lies-Info([string]$t) {
    if ($t -match 'PS5INFO_DDD=(-?\d+)') { $script:DDD_STATUS = [int]$Matches[1] }
    if ($t -match 'PS5INFO_MODEL=(CFI-[^\r\n]*)') { return $Matches[1].Trim() }
    return ''
}
function Info-Datei-Lesen { return (Lies-Info (Datei-Text $INFO_DATEI)) }

# Weg 1: Payload-Lader (Port 9021) - Antwort kommt direkt zurück
function Modell-Ueber-Lader([string]$ip, [byte[]]$elf) {
    try {
        $tcp = New-Object System.Net.Sockets.TcpClient
        $ar = $tcp.BeginConnect($ip, 9021, $null, $null)
        if (-not $ar.AsyncWaitHandle.WaitOne(3000) -or -not $tcp.Connected) { $tcp.Close(); return '' }
        $tcp.EndConnect($ar)
        $st = $tcp.GetStream()
        $st.Write($elf, 0, $elf.Length)
        $st.ReadTimeout = 15000
        $ms = New-Object System.IO.MemoryStream
        $buf = New-Object byte[] 4096
        $ende = (Get-Date).AddSeconds(15)
        while ((Get-Date) -lt $ende) {
            try { $n = $st.Read($buf, 0, $buf.Length) } catch { break }
            if ($n -le 0) { break }
            $ms.Write($buf, 0, $n)
            if ($UTF8.GetString($ms.ToArray()) -match "PS5INFO_DDD=") { break }
        }
        $tcp.Close()
        return (Lies-Info ($UTF8.GetString($ms.ToArray())))
    } catch { }
    return ''
}

# Weg 2: Payload-Manager (Port 8084) - Antwort kommt als Datei, per FTP gelesen
function Modell-Ueber-Manager([string]$ip, [byte[]]$elf) {
    $basis = "http://${ip}:8084"
    $null = Http-Post "$basis/manage:delete?filename=ps5info.elf" $null
    if (-not (Http-Post "$basis/manage:upload?filename=ps5info.elf" $elf)) { return '' }
    $m = ''
    if (Http-Post "$basis/loadpayload:ps5info.elf" $null) {
        $ende = (Get-Date).AddSeconds(15)
        while ((Get-Date) -lt $ende -and $m -eq '') {
            & $script:Warte 700
            $m = Info-Datei-Lesen
        }
    }
    $null = Http-Post "$basis/manage:delete?filename=ps5info.elf" $null
    return $m
}

function Modell-Lesen([string]$ip) {
    $pfad = Join-Path $SkriptOrdner 'ps5info.elf'
    if (-not (Test-Path $pfad)) { return '' }
    $elf = [System.IO.File]::ReadAllBytes($pfad)
    $script:DDD_STATUS = $null
    $null = Ftp-Befehl "DELE $INFO_DATEI"                 # alte Antwort weg
    $m = ''
    if (Port-Offen $ip 9021) { $m = Modell-Ueber-Lader $ip $elf }
    if ($m -eq '' -and (Port-Offen $ip 8084)) { $m = Modell-Ueber-Manager $ip $elf }
    $null = Ftp-Befehl "DELE $INFO_DATEI"                 # aufräumen
    return $m
}

# Modellreihe aus der Nummer: CFI-10xx/11xx/12xx = erste PS5, CFI-20xx/21xx =
# Slim, CFI-70xx = Pro. Der Buchstabe nach den vier Ziffern sagt, ob es eine
# Digital Edition ist (B) oder eine mit Laufwerk (A).
function Modell-Typ([string]$m) {
    $m = ($m -replace '\s', '').ToUpper()
    if ($m -match '^CFI-1[012]') { return 'fat' }
    if ($m -match '^CFI-2[01]')  { return 'slim' }
    if ($m -match '^CFI-70')     { return 'pro' }
    return ''
}
$script:MODELL_DIGITAL = $false
function Modell-Digital([string]$m) { return (($m -replace '\s', '').ToUpper() -match '^CFI-\d{4}B') }
# Lesbarer Name der Reihe samt Laufwerks-Situation
function Modell-Reihe([string]$typ, [bool]$digital) {
    switch ($typ) {
        'fat'  { if ($digital) { return 'PS5 Digital Edition (erstes Modell)' }
                 return 'PS5 (erstes Modell)' }
        'slim' { if ($digital) { return 'PS5 Slim Digital Edition' }
                 return 'PS5 Slim' }
        'pro'  { return 'PS5 Pro' }
    }
    return 'unbekannt'
}

$script:MODELL_TYP = ''
# Klartext zum tatsächlich ausgelesenen Laufwerks-Status (nur lesend ermittelt).
# -1 = kein abnehmbares Laufwerk / nicht lesbar, 0..6 = Statuszahl des Treibers.
# Hinweis: Bei Slim/Pro heißt die vom Treiber vergebene Bezeichnung für ein
# nachträglich gekoppeltes Laufwerk "PAIRED_TO_ANOTHER" (Status 1) - das Laufwerk
# ist trotzdem mit DIESER Konsole gekoppelt und einsatzbereit.
function Laufwerk-Text {
    switch ($script:DDD_STATUS) {
        0 { return 'Laufwerk ist mit dieser Konsole gekoppelt und einsatzbereit.' }
        1 { return 'Laufwerk ist mit dieser Konsole gekoppelt (nachträglich gekoppelt) und einsatzbereit.' }
        2 { return 'Kein gekoppeltes Laufwerk (keins angeschlossen oder noch nicht gekoppelt).' }
        3 { return 'Laufwerks-Kopplung ist noch nicht abgeschlossen.' }
        4 { return 'Laufwerks-Entkopplung ist noch nicht abgeschlossen.' }
        5 { return 'Laufwerk ist ab Werk mit dieser Konsole gekoppelt und einsatzbereit.' }
        6 { return 'Hinweis: Laufwerk ist ab Werk mit einer ANDEREN Konsole gekoppelt - es würde an dieser Konsole nicht als Disc-Laufwerk arbeiten.' }
        default {
            if ($script:MODELL_TYP -eq 'fat') {
                if ($script:MODELL_DIGITAL) { return 'Kein Laufwerk (Digital Edition) - eine Kopplung gibt es nicht.' }
                return 'Fest eingebautes Laufwerk (erste PS5) - keine Kopplung nötig.'
            }
            return 'Laufwerks-Status nicht auslesbar (kein Laufwerk angeschlossen oder Abfrage nicht möglich).'
        }
    }
}
# Ist der ausgelesene Status unkritisch (gekoppelt/einsatzbereit oder gar kein
# Laufwerk vorhanden)? Nur Status 6 (ab Werk mit anderer Konsole) ist ein Hinweis.
function Kopplung-Ok { return ($script:DDD_STATUS -in @($null, -1, 0, 1, 2, 5)) }
function Kopplungs-Text {
    $l = Laufwerk-Text
    if (Kopplung-Ok) { return "$l Das Programm fasst die Kopplung ohnehin nie an." }
    return "$l Das Programm fasst die Kopplung ohnehin nie an - es wird durch das Aufräumen nichts daran verändert."
}
function Kopplungs-Hinweis { Write-Host " >> $(Laufwerk-Text) <<" }

# Liest param.json (erst appmeta, dann app/sce_sys - Homebrew hat nur das
# zweite) und nimmt den deutschen, sonst englischen, sonst Standard-Namen.
$script:APP_ROH = ''
# Titel aus einer param.json (PS5): deutscher, sonst englischer, sonst Standard-Name.
function Json-Titel([string]$t) {
    $t = $t.TrimStart([char]0xFEFF)
    if ($t -notmatch 'titleName') { return '' }
    try {
        $lp = ($t | ConvertFrom-Json).localizedParameters
        if ($null -ne $lp) {
            foreach ($s in @('de-DE', 'en-US', 'en-GB', [string]$lp.defaultLanguage)) {
                if ($s -ne '' -and $null -ne $lp.$s -and [string]$lp.$s.titleName -ne '') { return [string]$lp.$s.titleName }
            }
        }
    } catch { }
    if ($t -match '"titleName"\s*:\s*"([^"]*)"') { return $Matches[1] }
    return ''
}
# param.sfo (PS4-Format) lesen: liefert @{ Schlüssel = Wert } oder eine leere Tabelle.
function Sfo-Lesen([byte[]]$b) {
    $h = @{}
    if ($null -eq $b -or $b.Length -lt 20 -or $b[0] -ne 0 -or $b[1] -ne 0x50 -or $b[2] -ne 0x53 -or $b[3] -ne 0x46) { return $h }
    try {
        $keyTab = [BitConverter]::ToUInt32($b, 8); $datTab = [BitConverter]::ToUInt32($b, 12); $n = [BitConverter]::ToUInt32($b, 16)
        for ($i = 0; $i -lt $n -and $i -lt 200; $i++) {
            $e = 20 + 16 * $i
            $kOff = [BitConverter]::ToUInt16($b, $e); $fmt = [BitConverter]::ToUInt16($b, $e + 2)
            $len = [BitConverter]::ToUInt32($b, $e + 4); $dOff = [BitConverter]::ToUInt32($b, $e + 12)
            $ks = $keyTab + $kOff; $ke = [Array]::IndexOf($b, [byte]0, [int]$ks)
            if ($ke -lt 0) { continue }
            $key = [System.Text.Encoding]::ASCII.GetString($b, $ks, $ke - $ks)
            $ds = $datTab + $dOff
            if ($fmt -eq 0x0404 -and $len -ge 4) { $h[$key] = [BitConverter]::ToUInt32($b, $ds) }
            else { $h[$key] = $UTF8.GetString($b, $ds, $len).TrimEnd([char]0) }
        }
    } catch { }
    return $h
}
# Titel aus param.sfo: deutsch (TITLE_04), englisch (TITLE_01), sonst Standard.
function Sfo-Titel($h) {
    foreach ($k in @('TITLE_04', 'TITLE_01', 'TITLE')) { if ($h.ContainsKey($k) -and [string]$h[$k] -ne '') { return [string]$h[$k] } }
    return ''
}
# Je Suche gemerkt (Suche-Funde leert den Speicher), damit jede Kennung nur
# einmal gelesen wird - auch für die Astro-Erkennung vorab.
$script:TITEL_CACHE = @{}
function App-Titel([string]$id) {
    if ($script:TITEL_CACHE.ContainsKey($id)) { $c = $script:TITEL_CACHE[$id]; $script:APP_ROH = $c.Roh; return $c.T }
    $t = App-Titel-Lesen $id
    $script:TITEL_CACHE[$id] = @{ T = $t; Roh = $script:APP_ROH }
    return $t
}
function App-Titel-Lesen([string]$id) {
    $script:APP_ROH = ''
    foreach ($datei in @("/user/appmeta/$id/param.json", "/user/app/$id/sce_sys/param.json", "$SYS_META/$id/param.json")) {
        $t = (Datei-Text $datei).TrimStart([char]0xFEFF)
        if ($t -notmatch 'titleName') { continue }
        $script:APP_ROH = $t
        $n = Json-Titel $t
        if ($n -ne '') { return $n }
    }
    # PS4-Spiele haben statt param.json eine param.sfo
    foreach ($datei in @("/user/appmeta/$id/param.sfo", "/user/app/$id/sce_sys/param.sfo", "$SYS_META/$id/param.sfo")) {
        $n = Sfo-Titel (Sfo-Lesen (Ftp-Daten "RETR $datei"))
        if ($n -ne '') { $script:APP_ROH = $n; return $n }
    }
    return ''
}
# Hängt bei Ordnern, die nach einer Spiel-ID benannt sind, den Namen an.
$script:NAMEN_CACHE = @{}
function Mit-Name([string]$x) {
    if ($x -notmatch '^([A-Z]{4}\d{5})') { return $x }
    $id = $Matches[1]
    if (-not $script:NAMEN_CACHE.ContainsKey($id)) { $script:NAMEN_CACHE[$id] = App-Titel $id }
    $t = $script:NAMEN_CACHE[$id]
    if ($t -eq '') { return $x }
    return "$x ($t)"
}
# Trophäen-Kennung (NPWR...) lesbar machen: Spielname aus den Kachel-Infos
# (Spiel ist da) oder aus einer Aufnahme in der Mediengalerie (Spiel weg).
$script:NPWR_NAMEN = @{}
$script:NPWR_GALERIE = @{}
function Trophaen-Name([string]$npwr) {
    if ($script:NPWR_NAMEN.ContainsKey($npwr) -and $script:NPWR_NAMEN[$npwr] -ne '') { return "$($script:NPWR_NAMEN[$npwr]) ($npwr)" }
    if ($script:NPWR_GALERIE.ContainsKey($npwr)) { return "$($script:NPWR_GALERIE[$npwr]) ($npwr)" }
    return $npwr
}
# Zusatz hinter dem Namen, wenn das Spiel nicht mehr auf der Konsole ist
function Trophaen-Weg([string]$npwr) { if ($script:NPWR_NAMEN.ContainsKey($npwr)) { return '' } else { return ' - Spiel ist nicht mehr auf der Konsole' } }
# Zählt Screenshots/Videos in einem Galerie-Ordner eines Spiels (eine Ebene
# Unterordner, Begleitdateien .ext/.meta/.dat nicht mitgezählt).
function Zaehle-Medien([string]$p) {
    $n = 0
    foreach ($e in (Liste $p)) {
        if ($e.IstOrdner) { foreach ($d in (Liste "$p/$($e.Name)")) { if (-not $d.IstOrdner -and $d.Name -match '\.(jxr|jpe?g|png|mp4|webm|mov)$') { $n++ } } }
        elseif ($e.Name -match '\.(jxr|jpe?g|png|mp4|webm|mov)$') { $n++ }
    }
    return $n
}
# Name eines Spiels in der Mediengalerie. Erst die Kachel-Infos, sonst die
# Begleitdatei (.dat) einer Aufnahme - darin steht der Spielname im Klartext,
# auch wenn das Spiel längst nicht mehr auf der Konsole ist.
$script:GALERIE_NAMEN = @{}
function Galerie-Name([string]$id, [string[]]$ordner = @()) {
    if ($id -like 'NPXS*') { return "Systemmenü ($id)" }
    $m = Mit-Name $id
    if ($m -ne $id) { return ($m -replace '^[A-Z]{4}\d{5} \((.*)\)$', '$1') + " ($id)" }
    if (-not $script:GALERIE_NAMEN.ContainsKey($id)) {
        $script:GALERIE_NAMEN[$id] = ''
        :suche foreach ($o in $ordner) {
            foreach ($e in (Liste $o)) {
                if (-not $e.IstOrdner) { continue }
                foreach ($d in (Liste "$o/$($e.Name)")) {
                    if ($d.IstOrdner -or $d.Name -notlike '*.dat') { continue }
                    $b = Ftp-Daten "RETR $o/$($e.Name)/$($d.Name)"
                    if ($null -eq $b -or $b.Length -lt 8) { continue }
                    # nach dem 8-Byte-Kopf: erster Klartext bis zum nächsten Nullbyte = Spielname
                    $start = 8; while ($start -lt $b.Length -and $b[$start] -lt 32) { $start++ }
                    if ($start -ge $b.Length) { continue }
                    $ende = [Array]::IndexOf($b, [byte]0, $start); if ($ende -lt 0) { $ende = $b.Length }
                    $t = ($UTF8.GetString($b, $start, $ende - $start) -replace '_\d{14}.*$', '').Trim()   # Zeitstempel hinter dem Namen weg
                    if ($t.Length -lt 2 -or $t.Length -gt 100) { continue }
                    $script:GALERIE_NAMEN[$id] = $t
                    # Begleitdatei .ext nennt die Trophäen-Kennung -> Name auch für Bereich 3
                    $ext = Datei-Text ($o + '/' + $e.Name + '/' + ($d.Name -replace '\.dat$', '.ext'))
                    if ($ext -match 'NPWR\d{5}_\d{2}') { $script:NPWR_GALERIE[$Matches[0]] = $t }
                    break suche
                }
            }
        }
    }
    if ($script:GALERIE_NAMEN[$id] -ne '') { return "$($script:GALERIE_NAMEN[$id]) ($id, Spiel ist nicht mehr auf der Konsole)" }
    return "$id (Spiel ist nicht mehr auf der Konsole)"
}
# Prüft alle Sprachfassungen des Namens, nicht nur die angezeigte.
function Ist-Astro([string]$t) { return (($t + ' ' + $script:APP_ROH).ToUpper() -match 'ASTRO.*PLAYROOM') }
function Astro-Merken([string]$id) { if ($script:ASTRO_IDS -notcontains $id) { $script:ASTRO_IDS += $id } }


# ---------------------------------------------------------------------
# Suchen: füllt $script:Funde (Einträge mit Pfade/Kat/Text/Wahl/Quelle/
# Gesperrt), $script:m2Da, $script:KOMPRIMIERUNG, $script:AUFGEKLAPPT
# ---------------------------------------------------------------------
$script:KatName = @('',
    'Spiele & Apps (interne SSD)',
    'Spielstände',
    'Trophäen',
    'Bilder & Videos (Aufnahmen)',
    'Downloads, Zusatzinhalte & Reste',
    'Jailbreak-Tools & Homebrew-Daten',
    'Inhalt der M.2-SSD')
$script:KatInfo = @('',
    (' Installierte Spiele und Apps auf der internen SSD (PS5 und PS4). Spiel,',
     ' Update und jeder Zusatzinhalt (DLC) sind einzeln wählbar - wer das Spiel',
     ' wählt, nimmt Update und Zusatzinhalte automatisch mit.'),
    (' Spielstände auf der Konsole. Mit PS Plus sind sie evtl. in der Cloud',
     ' gesichert - ohne Sicherung sind sie endgültig weg.'),
    (' Dein Trophäen-Fortschritt je Spiel und die Trophäen-Listen der Spiele.',
     ' Mit dem PSN abgeglichener Fortschritt kommt beim nächsten Spielstart',
     ' zurück. ACHTUNG: Nie abgeglichene Trophäen (Konsole immer offline) sind',
     ' endgültig weg.'),
    (' Deine Screenshots und Videoclips aus der Mediengalerie, je Spiel mit',
     ' Anzahl. Dazu Vorschaubilder und Zwischenordner der Aufnahme-Funktion',
     ' (die legt die PS5 bei Bedarf neu an).'),
    (' Downloads, Zusatzinhalte und Updates ohne Spiel, Reste gelöschter Spiele.'),
    (' Einstellungen und Daten der Jailbreak-Tools (z.B. Payload Manager,',
     ' ShadowMount, kstuff, etaHEN), ihre Kacheln im Menü und Homebrew-Daten',
     ' (z.B. Emulator-Spielstände). DANACH MUSST DU DEINEN JAILBREAK NEU',
     ' EINRICHTEN. Wer per Browser-Seite jailbreakt: der Browserspeicher kann',
     ' auch einen offline gespeicherten Zugang enthalten.'),
    (' Alles auf der M.2-SSD (Spiele, Ordner, Dateien). Die Verwaltungsordner der',
     ' PS5 bleiben. Echtes Formatieren geht nur an der PS5 selbst: Einstellungen',
     ' > Speicher > M.2-SSD > Formatieren (ebenfalls ungefährlich für die',
     ' Kopplung).'))
$script:ALLE_KAT = @(1, 2, 3, 4, 5, 6, 7)
$script:Funde = New-Object System.Collections.ArrayList
$script:MERKE_OK = $false            # hat der letzte Merke-Aufruf einen Eintrag angelegt?
function Merke([string[]]$pfade, [int]$kat, [string]$text, [string]$quelle = '', [bool]$gesperrt = $false) {
    # Astro bleibt immer - seine Pfade werden gar nicht erst angeboten
    $pfade = @($pfade | Where-Object { -not (Astro-Pfad $_) })
    $script:MERKE_OK = ($pfade.Count -gt 0)
    if (-not $script:MERKE_OK) { return }
    if ($gesperrt) { $text += '  [GESPERRT: Game Compressor arbeitet gerade daran]' }
    # PfadeBasis/TextBasis: der Eintrag wie gefunden. Bei ShadowMount-Spielen
    # wird Pfade je nach Entscheidung (mit/ohne Spieldateien) jedes Mal neu
    # daraus gebaut - so wirkt eine abgebrochene Entscheidung nie nach.
    $null = $script:Funde.Add([pscustomobject]@{ Pfade = $pfade; Kat = $kat; Text = $text; Wahl = $false; Quelle = $quelle; Gesperrt = $gesperrt
                                                 PfadeBasis = $pfade; TextBasis = $text })
}
# Eintrag, der zu einem Spiel gehört (Teil = Spiel, Update oder DLC). Über die
# Gruppe (Spiel-ID) findet das Programm später zusammengehörige Teile.
function Merke-Teil([string[]]$pfade, [int]$kat, [string]$text, [string]$gruppe, [string]$teil) {
    Merke $pfade $kat $text
    if (-not $script:MERKE_OK) { return }
    $f = $script:Funde[$script:Funde.Count - 1]
    $f | Add-Member -NotePropertyName Gruppe -NotePropertyValue $gruppe
    $f | Add-Member -NotePropertyName Teil -NotePropertyValue $teil
}
function Ist-Teil($f, [string]$teil) { return ($null -ne $f.PSObject.Properties['Teil'] -and $f.Teil -eq $teil) }
# Version eines Updates: PS5 aus param.json (contentVersion), PS4 aus param.sfo (APP_VER)
function Patch-Version([string]$id, [string]$basis = '/user') {
    # entpackte Updates (PS5) haben sce_sys im Ordner ...
    $t = Datei-Text "$basis/patch/$id/sce_sys/param.json"
    if ($t -match '"contentVersion"\s*:\s*"([^"]+)"') { return " (Version $($Matches[1]))" }
    $h = Sfo-Lesen (Ftp-Daten "RETR $basis/patch/$id/sce_sys/param.sfo")
    if ($h.ContainsKey('APP_VER') -and [string]$h['APP_VER'] -ne '') { return " (Version $($h['APP_VER']))" }
    # ... verpackte (PS4: patch.pkg) nicht - dann steht die Version in den Kachel-Infos
    $t = Datei-Text "$SYS_META/$id/param.json"
    if ($t -match '"contentVersion"\s*:\s*"([^"]+)"') { return " (Version $($Matches[1]))" }
    foreach ($d in @("$SYS_META/$id/param.sfo", "/user/appmeta/$id/param.sfo")) {
        $h = Sfo-Lesen (Ftp-Daten "RETR $d")
        if ($h.ContainsKey('APP_VER') -and [string]$h['APP_VER'] -ne '') { return " (Version $($h['APP_VER']))" }
    }
    return ''
}
# Spiel, Update und Zusatzinhalte eines normal installierten Spiels als eigene
# Einträge merken. $basis = '/user' (interne SSD) oder "$M2/user" (M.2).
function Merke-Spielteile([string[]]$spielPfade, [int]$kat, [string]$name, [string]$id, [string]$basis,
                          [bool]$hatUpdate, [bool]$hatDlc, [string[]]$dlcInfos, $Benutzt, [bool]$zwilling = $false) {
    # Share-Einstellungen des Spiels gehen mit dem Spiel (nicht, wenn es dieselbe
    # Kennung intern und auf der M.2 gibt - dann braucht die andere sie noch)
    if ($script:SHARES -contains $id -and -not $zwilling) { $spielPfade = @($spielPfade) + "/user/system/share/$id" }
    # Gruppe = Ort + Kennung: dasselbe Spiel intern und auf der M.2 sind zwei Gruppen
    $gruppe = "$basis|$id"
    Merke-Teil $spielPfade $kat "$name ($id) - Spiel" $gruppe 'Spiel'
    if ($hatUpdate) {
        $Benutzt["$basis/patch/$id"] = 1
        Merke-Teil @("$basis/patch/$id") $kat "$name ($id) - Update$(Patch-Version $id $basis)" $gruppe 'Update'
    }
    if ($hatDlc) {
        $Benutzt["$basis/addcont/$id"] = 1
        $leer = @("$basis/addcont/$id")
        $meta = @()
        if ($dlcInfos -contains $id) {
            $Benutzt["/user/appmeta/addcont/$id"] = 1
            if (-not $zwilling) { $leer += "/user/appmeta/addcont/$id"; $meta = Namen "/user/appmeta/addcont/$id" }
        }
        $script:LEER_DANACH[$gruppe] = $leer
        foreach ($d in (Namen "$basis/addcont/$id")) {
            $dp = @("$basis/addcont/$id/$d")
            if ($meta -contains $d) { $dp += "/user/appmeta/addcont/$id/$d" }
            Merke-Teil $dp $kat "$name ($id) - Zusatzinhalt: $(DLC-Name $id $d $basis)" $gruppe 'DLC'
        }
    }
}
# Name eines Zusatzinhalts (DLC) - sonst der Ordnername
function DLC-Name([string]$id, [string]$dlc, [string]$basis = '/user') {
    foreach ($d in @("/user/appmeta/addcont/$id/$dlc/param.json", "$basis/addcont/$id/$dlc/sce_sys/param.json")) {
        $n = Json-Titel (Datei-Text $d); if ($n -ne '') { return $n }
    }
    foreach ($d in @("/user/appmeta/addcont/$id/$dlc/param.sfo", "$basis/addcont/$id/$dlc/sce_sys/param.sfo")) {
        $n = Sfo-Titel (Sfo-Lesen (Ftp-Daten "RETR $d")); if ($n -ne '') { return $n }
    }
    return $dlc
}
# Hakt man ein Spiel an, gehen sein Update und seine Zusatzinhalte automatisch
# mit - ohne das Spiel sind sie nutzlos und blieben sonst als Reste liegen.
function Abhaengige-Mitnehmen {
    $basis = @{}
    foreach ($f in $script:Funde) { if ($f.Wahl -and (Ist-Teil $f 'Spiel')) { $basis[$f.Gruppe] = 1 } }
    foreach ($f in $script:Funde) {
        if ($null -ne $f.PSObject.Properties['Teil'] -and $f.Teil -ne 'Spiel' -and $basis.ContainsKey($f.Gruppe) -and -not $f.Gesperrt) { $f.Wahl = $true }
    }
}
function Suche-Funde {
    $script:FUNDE_IP = ''              # erst gültig, wenn die Suche ganz durch ist
    $script:Funde = New-Object System.Collections.ArrayList
    $script:LEER_DANACH = @{}          # Spiel-ID -> Ordner, die nach dem Löschen leer sein können
    # Diese Listen entscheiden, was als "nicht mehr installiert" gilt - darum
    # streng: scheitert eine, bricht die Suche ab, statt Falsches anzubieten.
    $script:SHARES = Namen '/user/system/share' $true
    $homes    = Namen '/user/home' $true
    $appmeta  = Namen '/user/appmeta' $true
    $apps     = Namen '/user/app' $true
    $patches  = Namen '/user/patch' $true
    $addconts = Namen '/user/addcont' $true
    $dlcInfos = Namen '/user/appmeta/addcont' $true
    $m2Liste  = Liste $M2 $true
    $m2Da     = ($null -ne $m2Liste); $script:m2Da = $m2Da
    $m2Apps   = @(); $m2Patches = @()
    $m2Addconts = @()
    if ($m2Da) { $m2Apps = Namen "$M2/user/app" $true; $m2Patches = Namen "$M2/user/patch" $true; $m2Addconts = Namen "$M2/user/addcont" $true }
    $Benutzt  = @{}                    # Pfade, die schon zu einem Spiel gehören
    # Die PS5 legt für jede Kachel einen Ordner unter /system_data/priv/appmeta an.
    # Bleibt er stehen, zeigt das Menü nach dem Datenbank-Neuaufbau eine leere
    # Kachel mit Schloss. Darum geht er beim Löschen eines Spiels/einer App mit.
    $sysmeta  = Namen $SYS_META $true
    function Mit-Kachel([string[]]$pfade, [string]$id) {
        if ($sysmeta -contains $id -and $id -notlike 'NPXS*' -and $script:ASTRO_IDS -notcontains $id) { return ($pfade + "$SYS_META/$id") }
        return $pfade
    }
    # Namen aller Kacheln mit Trophäen zuerst lesen - dabei wird Astro unter
    # jeder Länderkennung erkannt, bevor irgendetwas anderes gelistet wird.
    $script:ASTRO_IDS = @($BEHALTEN_APP); $script:ASTRO_NPWRS = @('NPWR20188_00')
    $script:NAMEN_CACHE = @{}; $script:TITEL_CACHE = @{}
    foreach ($id in @(@($apps) + @($m2Apps) + @($appmeta) + @($sysmeta) | Select-Object -Unique)) {
        if ($id -notmatch '^[A-Z]{4}\d{5}$' -or $id -like 'NPXS*') { continue }
        if (Ist-Astro (App-Titel $id)) { Astro-Merken $id }
    }
    # Zuordnung Trophäen-Kennung (NPWR...) -> Spielname über die Kachel-Infos
    # der PS5 (npbind.dat). Astro wird darüber erkannt und nie angefasst.
    $script:NPWR_NAMEN = @{}
    foreach ($id in $sysmeta) {
        if ($id -notmatch '^[A-Z]{4}\d{5}$') { continue }
        foreach ($sub in @('trophy2', 'trophy')) {
            $nb = Datei-Text "$SYS_META/$id/$sub/npbind.dat"
            $ms = [regex]::Matches($nb, 'NPWR\d{5}_\d{2}')
            if ($ms.Count -eq 0) { continue }
            if (-not $script:NAMEN_CACHE.ContainsKey($id)) { $script:NAMEN_CACHE[$id] = App-Titel $id; if (Ist-Astro $script:NAMEN_CACHE[$id]) { Astro-Merken $id } }
            if ($script:ASTRO_IDS -contains $id) { foreach ($m in $ms) { if ($script:ASTRO_NPWRS -notcontains $m.Value) { $script:ASTRO_NPWRS += $m.Value } } }
            foreach ($m in $ms) { if (-not $script:NPWR_NAMEN.ContainsKey($m.Value)) { $script:NPWR_NAMEN[$m.Value] = $script:NAMEN_CACHE[$id] } }
            break
        }
    }
    $script:KOMPRIMIERUNG = @()        # halbfertige Game-Compressor-Dateien
    $script:AUFGEKLAPPT   = @()        # Ordner, die wegen ShadowMount-Spielen einzeln gelistet sind

    # ---- ShadowMount ----------------------------------------------------
    # /user/app/<ID> ist bei ShadowMount nur eine Verknüpfung (Kachel). Das Spiel
    # selbst (Image-Datei oder Ordner) liegt auf der M.2 oder der internen SSD -
    # steht in mount_img.lnk bzw. mount.lnk. Wird das Spiel gelöscht, geht die
    # Verknüpfung immer mit (keine tote Kachel).
    $SpielQuellen = New-Object System.Collections.ArrayList
    $QuelleInfo   = @{}
    function SM-Quelle([string]$id) {
        foreach ($lnk in @('mount_img.lnk', 'mount.lnk')) {
            foreach ($z in ((Datei-Text "/user/app/$id/$lnk") -split "`r?`n")) {
                $z = $z.Trim()
                if ($z -eq '' -or $z -like '/mnt/shadowmnt*') { continue }
                if ($z -like '/data/*') { $z = '/user' + $z }
                return $z
            }
        }
        return ''
    }
    function Quelle-Ort([string]$q) {
        if ($q -match '[\x00-\x1f\x7f]') { return '' }     # unbrauchbarer Eintrag: nur die Kachel anbieten
        $m = [regex]::Escape($M2)
        if ($q -match "^$m/(user|system_data|lost\+found)(/|$)") { return '' }
        if ($q -match "^$m/[^/]+") { return 'M.2' }
        if ($q -match '^/user/data/[^/]+') { return 'internen SSD' }
        return ''
    }
    # Game Compressor: arbeitet er gerade an einem Spiel, liegt neben der Quelle
    # eine Zwischendatei ".<Name>...gc-compress.tmp" - dann ist das Spiel gesperrt.
    $script:ORDNER_CACHE = @{}
    function Wird-Komprimiert([string]$q, [string]$id) {
        $eltern = $q.Substring(0, $q.LastIndexOf('/'))
        $name   = $q.Substring($q.LastIndexOf('/') + 1)
        $basis  = $name -replace '\.[^.]+$', ''
        if (-not $script:ORDNER_CACHE.ContainsKey($eltern)) { $script:ORDNER_CACHE[$eltern] = Namen $eltern $true }
        foreach ($n in $script:ORDNER_CACHE[$eltern]) {
            if (-not (Ist-Zwischendatei $n)) { continue }
            if ($n -like "*$id*" -or $n.TrimStart('.').StartsWith($basis)) { return $true }
        }
        return $false
    }
    function Ist-Zwischendatei([string]$name) { return ($name -match '\.gc-compress\.tmp') }

    # Listet einen Ordner auf. Unterordner mit ShadowMount-Spielen werden
    # aufgeklappt; ein Spiel steht dort mit Namen und nimmt seine Verknüpfung mit.
    # $eintraege aus Liste: in Verknüpfungen (Symlinks) wird nie hineingeschaut -
    # sie könnten z.B. auf die M.2 zeigen.
    function Merke-Ordner([string]$basis, $eintraege, [int]$kat, [string]$vorsatz) {
        foreach ($e in $eintraege) {
            $x = $e.Name; $link = [bool]$e.IstLink
            $p = "$basis/$x"
            if (Ist-Zwischendatei $x) { $script:KOMPRIMIERUNG += $p; continue }
            if ($SpielQuellen -contains $p) {
                $i = $QuelleInfo[$p]
                Merke (@($p) + $i.Links) $kat "$vorsatz$x  [Spiel: $($i.Name) - Kachel geht mit]" $p $i.Gesperrt
                continue
            }
            $drin = @($SpielQuellen | Where-Object { $_.StartsWith("$p/") })
            if ($drin.Count -gt 0 -and -not $link) {
                $script:AUFGEKLAPPT += $p
                $unter = Liste $p $true; if ($null -eq $unter) { $unter = @() }
                Merke-Ordner $p $unter $kat "$vorsatz$x/"
            } elseif ($link) {
                Merke @($p) $kat "$vorsatz$x  (Verknüpfung - nur sie selbst wird gelöscht)"
            } else {
                Merke @($p) $kat "$vorsatz$x"
            }
        }
    }

    # ---- 1 Spiele & Apps (und Tool-Kacheln -> 6) ------------------------
    foreach ($x in $apps) {
        if ($script:ASTRO_IDS -contains $x -or $x -like 'NPXS*') { continue }
        $t = App-Titel $x
        if (Ist-Astro $t) { Astro-Merken $x; continue }
        $roh = $script:APP_ROH
        if ($t -eq '') { $t = 'unbekannter Name' }
        $p = @("/user/app/$x")
        # dieselbe Kennung auch auf der M.2: Kachel-Infos & Co. gehören beiden - bleiben
        $zwilling = ($m2Apps -contains $x)
        if ($appmeta -contains $x) { $Benutzt["/user/appmeta/$x"] = 1; if (-not $zwilling) { $p += "/user/appmeta/$x" } }
        if (-not $zwilling) { $p = Mit-Kachel $p $x }
        $inhalt = Namen "/user/app/$x" $true        # streng: entscheidet ShadowMount-Spiel oder normales Spiel
        if ($roh -match '"deeplinkUri"\s*:\s*"https?://(127\.0\.0\.1|localhost)') {
            # Tool-Kachel (Payload Manager, ShadowMount+, PS5Upload-Minion, ...) - auch
            # wenn sie wie ein ShadowMount-Spiel eine mount.lnk hat
            Merke $p 6 "Kachel des Tools: $t ($x)"
        } elseif ($inhalt -contains 'mount.lnk' -or $inhalt -contains 'mount_img.lnk') {
            $q = SM-Quelle $x
            $ort = Quelle-Ort $q
            $da = $false
            if ($ort -ne '') { $da = Existiert $q }      # nicht prüfbar (anderer Fehler als "gibt es nicht"): Suche bricht ab
            if ($ort -ne '' -and $da) {
                $g = Wird-Komprimiert $q $x
                $null = $SpielQuellen.Add($q)
                $QuelleInfo[$q] = [pscustomobject]@{ Links = $p; Name = "$t ($x)"; Gesperrt = $g }
                Merke $p 1 "$t ($x) - ShadowMount-Spiel auf der $ort" $q $g
            } elseif ($ort -ne '') {
                Merke $p 1 "$t ($x) - ShadowMount: Spiel nicht mehr da, tote Kachel"
            } else {
                if ($q -eq '') { $wo = 'Speicherort unbekannt' } else { $wo = "Spiel liegt auf $q" }
                Merke $p 1 "$t ($x) - ShadowMount: $wo, nur die Kachel wird gelöscht"
            }
        } else {
            # normal installiertes Spiel (PS5 oder PS4): Spiel, Update und jeder
            # Zusatzinhalt als eigener Eintrag - man kann einzeln wählen.
            Merke-Spielteile $p 1 $t $x '/user' ($patches -contains $x) ($addconts -contains $x) $dlcInfos $Benutzt $zwilling
        }
    }

    # ---- 2 Spielstände -------------------------------------------------
    foreach ($u in $homes) {
        foreach ($sd in @('savedata', 'savedata_prospero')) {
            foreach ($x in (Namen "/user/home/$u/$sd")) {
                if ($script:ASTRO_IDS -contains $x -or $x -like 'sce_*') { continue }   # sce_* legt die PS5 selbst an
                Merke @("/user/home/$u/$sd/$x") 2 "Spielstand $(Mit-Name $x)"
            }
        }
    }
    # ---- 4 Bilder & Videos ----------------------------------------------
    # Aufbau der Mediengalerie: photo|video/<Galerie-App>/<Spiel-ID>/<xxx>/<Datei>
    # (.jxr/.jpg = Screenshot, .mp4/.webm = Video, dazu kleine Begleitdateien),
    # thumbnails/photo|video/... gleicher Aufbau, content_tmp/<Spiel-ID> =
    # Zwischenordner der Aufnahme-Funktion. Ein Eintrag je Spiel.
    $galerie = [ordered]@{}            # Spiel-ID -> @{Pfade; Bilder; Videos}
    foreach ($art in @('photo', 'video')) {
        foreach ($app in (Namen "/user/av_contents/$art")) {
            foreach ($id in (Namen "/user/av_contents/$art/$app")) {
                if (-not $galerie.Contains($id)) { $galerie[$id] = @{ Pfade = @(); Bilder = 0; Videos = 0 } }
                $g = $galerie[$id]
                $g.Pfade += "/user/av_contents/$art/$app/$id"
                $n = Zaehle-Medien "/user/av_contents/$art/$app/$id"
                if ($art -eq 'photo') { $g.Bilder += $n } else { $g.Videos += $n }
            }
        }
    }
    # Vorschaubilder: gehören zum jeweiligen Spiel, ohne Aufnahme nur Reste
    $vorschauReste = @()
    foreach ($art in (Namen '/user/av_contents/thumbnails')) {
        foreach ($app in (Namen "/user/av_contents/thumbnails/$art")) {
            foreach ($id in (Namen "/user/av_contents/thumbnails/$art/$app")) {
                $pf = "/user/av_contents/thumbnails/$art/$app/$id"
                if ($galerie.Contains($id)) { $galerie[$id].Pfade += $pf } else { $vorschauReste += $pf }
            }
        }
    }
    foreach ($id in $galerie.Keys) {
        $g = $galerie[$id]
        $teile = @()
        if ($g.Bilder -gt 0) { $teile += "$($g.Bilder) Screenshot$(if ($g.Bilder -ne 1) {'s'})" }
        if ($g.Videos -gt 0) { $teile += "$($g.Videos) Video$(if ($g.Videos -ne 1) {'s'})" }
        $was = if ($teile.Count -gt 0) { $teile -join ', ' } else { 'leerer Ordner' }
        Merke $g.Pfade 4 "Aufnahmen von $(Galerie-Name $id $g.Pfade): $was"
    }
    if ($vorschauReste.Count -gt 0) { Merke $vorschauReste 4 "Vorschaubilder ohne zugehörige Aufnahme ($($vorschauReste.Count)) - Reste, legt die PS5 neu an" }
    # Zwischenordner der Aufnahme-Funktion: leer = Rest, sonst unfertige Aufnahme
    $leere = @()
    foreach ($id in (Namen '/user/av_contents/content_tmp')) {
        $pf = "/user/av_contents/content_tmp/$id"
        $n = 0; foreach ($x in (Namen $pf)) { $n++ }      # (Namen nie in @() oder eine Pipeline stecken)
        if ($n -eq 0) { $leere += $pf }
        else { Merke @($pf) 4 "Zwischenspeicher der Aufnahme-Funktion von $(Galerie-Name $id) ($n Datei$(if ($n -ne 1) {'en'}), unfertige Aufnahme)" }
    }
    if ($leere.Count -gt 0) { Merke $leere 4 "Leere Zwischenordner der Aufnahme-Funktion ($($leere.Count)) - Reste, legt die PS5 neu an" }
    # Sonstige Galerie-Ordner (z.B. sdr, extra), nur wenn etwas drin ist
    foreach ($s in (Namen '/user/av_contents')) {
        if ($s -in @('photo', 'video', 'thumbnails', 'content_tmp')) { continue }
        foreach ($x in (Namen "/user/av_contents/$s")) { Merke @("/user/av_contents/$s/$x") 4 "Weitere Galerie-Daten: $s/$(Mit-Name $x)" }
    }
    # ---- 3 Trophäen ----------------------------------------------------
    # Trophäen-Stand (Fortschritt) je Spiel
    foreach ($u in $homes) {
        foreach ($x in (Namen "/user/home/$u/trophy/data")) {
            if ($script:ASTRO_NPWRS -contains $x) { continue }        # Astro bleibt
            $t = if ($x -eq 'sce_trop') { 'Trophäen-Gesamtzähler der Konsole (Level, Anzahl Bronze/Silber/Gold - baut die PS5 neu auf)' }
                 elseif ($x -match '^NPWR') { "Dein Trophäen-Fortschritt in $(Trophaen-Name $x) (PS4-Spiel)$(Trophaen-Weg $x)" }
                 else { "Trophäen-Daten trophy/data/$x" }
            Merke @("/user/home/$u/trophy/data/$x") 3 $t
        }
        foreach ($x in (Namen "/user/home/$u/trophy")) {
            if ($x -ne 'data') { Merke @("/user/home/$u/trophy/$x") 3 "Trophäen-Daten trophy/$x" }
        }
        foreach ($x in (Namen "/user/home/$u/trophy2/nobackup/data")) {
            if ($script:ASTRO_NPWRS -contains $x) { continue }        # Astro bleibt
            $t = if ($x -match '^NPWR') { "Dein Trophäen-Fortschritt in $(Trophaen-Name $x)$(Trophaen-Weg $x)" } else { "Trophäen-Daten trophy2/nobackup/data/$x" }
            Merke @("/user/home/$u/trophy2/nobackup/data/$x") 3 $t
        }
        foreach ($x in (Namen "/user/home/$u/trophy2")) {
            if ($x -eq 'nobackup') {
                foreach ($y in (Namen "/user/home/$u/trophy2/nobackup")) {
                    if ($y -ne 'data') { Merke @("/user/home/$u/trophy2/nobackup/$y") 3 "Trophäen-Daten trophy2/nobackup/$y" }
                }
            } else { Merke @("/user/home/$u/trophy2/$x") 3 "Trophäen-Daten trophy2/$x" }
        }
    }
    # Trophäen-Pakete der Spiele (Beschreibung, welche Trophäen es gibt) - nicht
    # der Fortschritt. Ein Eintrag je Spiel; Astro wird übersprungen.
    $pakete = @{}
    foreach ($o in @('/user/trophy2/nobackup/conf', '/user/trophy/conf', '/user/np_uds/nobackup/conf')) {
        foreach ($x in (Namen $o)) {
            if ($x -notmatch '^NPWR\d{5}_\d{2}$' -or $script:ASTRO_NPWRS -contains $x) { continue }
            if (-not $pakete.ContainsKey($x)) { $pakete[$x] = @() }
            $pakete[$x] += "$o/$x"
        }
    }
    foreach ($k in @($pakete.Keys | Sort-Object)) {
        if ($script:NPWR_NAMEN.ContainsKey($k)) {
            # Spiel ist noch da: Paket gehört dazu, geht bei KOMPLETT nur mit der M.2 weg
            Merke $pakete[$k] 3 "Trophäen-Liste von $(Trophaen-Name $k) - Spiel ist noch da, Liste nur weg, wenn das Spiel auch weg soll (nur Namen und Bilder der Trophäen, nicht dein Fortschritt)" '' $false
            if ($script:MERKE_OK) { $script:Funde[$script:Funde.Count - 1] | Add-Member -NotePropertyName Paket -NotePropertyValue $true }
        } else {
            Merke $pakete[$k] 3 "Trophäen-Liste von $(Trophaen-Name $k) - Spiel ist nicht mehr auf der Konsole, kann weg (nur Namen und Bilder der Trophäen, nicht dein Fortschritt)"
        }
    }
    # ---- 5 Downloads, Zusatzinhalte & Reste -----------------------------
    foreach ($x in (Namen '/user/download')) {
        if ($x -like 'NPXS*') { continue }
        Merke @("/user/download/$x") 5 "Download: $(Mit-Name $x)"
    }
    foreach ($x in $patches)  { if (-not $Benutzt.ContainsKey("/user/patch/$x"))   { Merke @("/user/patch/$x") 5 "Update ohne Spiel: $(Mit-Name $x)" } }
    foreach ($x in $addconts) { if (-not $Benutzt.ContainsKey("/user/addcont/$x")) { Merke @("/user/addcont/$x") 5 "Zusatzinhalt: $(Mit-Name $x)" } }
    foreach ($x in (Namen '/user/appmeta/external/addcont')) { Merke @("/user/appmeta/external/addcont/$x") 5 "Zusatzinhalt-Infos: $(Mit-Name $x)" }
    foreach ($x in (Namen '/user')) {
        if ($x -like '_zu_loeschen_*') { Merke @("/user/$x") 5 "Rest einer früheren Aufräumaktion: /user/$x" }
    }
    if ($m2Da) {
        foreach ($e in $m2Liste) {
            if ($e.Name -like '_zu_loeschen_*') { Merke @("$M2/$($e.Name)") 7 "Rest einer früheren Aufräumaktion: $M2/$($e.Name)" }
        }
    }
    # ---- 7 M.2 (vor den Resten, damit Spiel-Infos dem M.2-Spiel zugeordnet werden)
    if ($m2Da) {
        foreach ($x in $m2Apps) {
            if ($x -like 'NPXS*' -or $script:ASTRO_IDS -contains $x) { continue }
            $t = App-Titel $x
            if (Ist-Astro $t) { Astro-Merken $x; continue }
            if ($t -eq '') { $t = 'unbekannter Name' }
            $p = @("$M2/user/app/$x")
            # Ist dieselbe Kennung auch intern installiert, gehören Kachel-Infos,
            # Share-Einstellungen und DLC-Infos beiden - dann bleiben sie.
            $zwilling = ($apps -contains $x)
            if (-not $zwilling) {
                if ($appmeta -contains $x) { $p += "/user/appmeta/$x"; $Benutzt["/user/appmeta/$x"] = 1 }
                $p = Mit-Kachel $p $x
            }
            Merke-Spielteile $p 7 $t $x "$M2/user" ($m2Patches -contains $x) ($m2Addconts -contains $x) $dlcInfos $Benutzt $zwilling
        }
        foreach ($x in $m2Patches) {
            if (-not $Benutzt.ContainsKey("$M2/user/patch/$x")) { Merke @("$M2/user/patch/$x") 7 "Update ohne Spiel: $(Mit-Name $x)" }
        }
        foreach ($x in $m2Addconts) {
            if (-not $Benutzt.ContainsKey("$M2/user/addcont/$x")) { Merke @("$M2/user/addcont/$x") 7 "Zusatzinhalte ohne Spiel: $(Mit-Name $x)" }
        }
        foreach ($e in $m2Liste) {
            $x = $e.Name
            if ($x -eq 'lost+found' -or $x -eq 'system_data' -or $x -eq 'user' -or $x -like '_zu_loeschen_*') { continue }
            Merke-Ordner $M2 @($e) 7 ''
        }
    }
    foreach ($x in $appmeta) {
        if ($script:ASTRO_IDS -contains $x -or $x -eq 'external' -or $x -eq 'addcont' -or $x -like 'NPXS*') { continue }
        if ($apps -contains $x -or $Benutzt.ContainsKey("/user/appmeta/$x")) { continue }
        $t = App-Titel $x
        if (Ist-Astro $t) { Astro-Merken $x; continue }
        if ($t -eq '') { $t = 'entferntes Spiel' }
        Merke (Mit-Kachel @("/user/appmeta/$x") $x) 5 "Reste: $t ($x)"
    }
    # Share-Einstellungen aller je gestarteten Spiele (ein Eintrag für alle)
    # (Namen nie direkt in eine Pipeline geben - erst in eine Variable, sonst kommt
    #  die ganze Liste als EIN Element an)
    $shareIds = @()
    foreach ($x in (Namen '/user/system/share')) {
        if ($x -notmatch '^[A-Z]{4}\d{5}$' -or $x -like 'NPXS*' -or $script:ASTRO_IDS -contains $x) { continue }
        if ($apps -contains $x -or $m2Apps -contains $x) { continue }      # Spiel ist noch installiert
        $shareIds += $x
    }
    if ($shareIds.Count -gt 0) {
        Merke @($shareIds | ForEach-Object { "/user/system/share/$_" }) 5 "Share-Einstellungen alter Spiele ($($shareIds.Count) $(if ($shareIds.Count -eq 1) {'Spiel'} else {'Spiele'}), legt die PS5 bei Bedarf neu an)"
    }
    # Store-Bildercache (Logos/Icons der Bibliothek, holt die PS5 online neu)
    $cache = @()
    foreach ($u in @('conceptmeta', 'appmeta')) { foreach ($x in (Namen "/user/catalog_downloader/$u")) { $cache += "/user/catalog_downloader/$u/$x" } }
    if ($cache.Count -gt 0) { Merke $cache 5 "Store-Bildercache ($($cache.Count) Einträge, holt die PS5 online neu)" }
    # temporäre Dateien (nur Inhalte, die Ordner bleiben)
    $tmp = @()
    foreach ($o in @('/user/bgft/trash', '/user/savedata/temp', '/user/temp/gls/screenshot', '/user/temp/video_recording', '/user/temp/common_temp/telemetry')) {
        foreach ($x in (Namen $o)) { $tmp += "$o/$x" }
    }
    if ($tmp.Count -gt 0) { Merke $tmp 5 "Temporäre Dateien ($($tmp.Count))" }
    # Fehlerberichte der Konsole
    $fehler = @()
    foreach ($x in (Namen '/system_data/priv/error/history')) { if ($x -like '*.json') { $fehler += "/system_data/priv/error/history/$x" } }
    if ($fehler.Count -gt 0) { Merke $fehler 5 "Fehlerberichte der Konsole ($($fehler.Count))" }
    # Merkzettel für Spiele auf externen USB-Platten (PS5 legt sie beim Anstecken neu an)
    $ext = @()
    foreach ($x in (Namen "$SYS_META/external")) { if ($x -match '^[A-Z]{4}\d{5}$' -and $x -notlike 'NPXS*') { $ext += "$SYS_META/external/$x" } }
    if ($ext.Count -gt 0) { Merke $ext 5 "Merkzettel für Spiele auf externen USB-Platten ($($ext.Count) $(if ($ext.Count -eq 1) {'Spiel'} else {'Spiele'}), legt die PS5 beim Anstecken neu an)" }
    # verwaiste Kachel-Infos: Spiel/App gibt es nirgends mehr, nur die Kachel
    foreach ($x in $sysmeta) {
        if ($x -notmatch '^[A-Z]{4}\d{5}$' -or $x -like 'NPXS*' -or $script:ASTRO_IDS -contains $x) { continue }
        if ($apps -contains $x -or $appmeta -contains $x -or $m2Apps -contains $x) { continue }
        $t = App-Titel $x
        if (Ist-Astro $t) { Astro-Merken $x; continue }
        if ($t -eq '') { $t = 'unbekannter Name' }
        Merke @("$SYS_META/$x") 5 "Leere Kachel (Schloss): $t ($x)"
    }
    # DLC-Infos ohne Spiel (erst hier, damit auch M.2-Spiele ihre schon erhalten haben)
    foreach ($x in $dlcInfos) { if (-not $Benutzt.ContainsKey("/user/appmeta/addcont/$x")) { Merke @("/user/appmeta/addcont/$x") 5 "Zusatzinhalt-Infos: $(Mit-Name $x)" } }
    # ---- 6 Jailbreak-Tools & Homebrew-Daten -----------------------------
    $dl = Liste '/user/data' $true; if ($null -eq $dl) { $dl = @() }
    Merke-Ordner '/user/data' $dl 6 ''
    # Fake-Lizenzen des Jailbreaks (fake*.rif + fake*.idx); echte Lizenzen bleiben
    $fakeLiz = @()
    foreach ($x in (Namen '/user/license' $true)) { if ($x -match '^fake.+\.(rif|idx)$') { $fakeLiz += $x } }
    $gruppen = @()
    foreach ($x in $fakeLiz) { $g = $x -replace '\.(rif|idx)$', ''; if ($gruppen -notcontains $g) { $gruppen += $g } }
    foreach ($g in $gruppen) {
        $pf = @()
        foreach ($x in $fakeLiz) { if ($x -eq "$g.rif" -or $x -eq "$g.idx") { $pf += "/user/license/$x" } }
        # Gehört die Lizenz zu einem noch installierten Spiel, geht sie nur
        # zusammen mit diesem Spiel weg - sonst startet das Spiel nicht mehr.
        if ($g -match '([A-Z]{4}\d{5})') {
            $tid = $Matches[1]
            $spiele = @($script:Funde | Where-Object { (Ist-Teil $_ 'Spiel') -and $_.Gruppe -like "*|$tid" })
            if ($spiele.Count -eq 1) { $spiele[0].Pfade = @($spiele[0].Pfade) + $pf; $spiele[0].PfadeBasis = $spiele[0].Pfade; continue }
            # Spiel ist mehrfach oder als ShadowMount-/Tool-Kachel installiert: Lizenz bleibt
            if ($spiele.Count -gt 1 -or $apps -contains $tid -or $m2Apps -contains $tid) { continue }
        }
        $n = $g -replace '^fake', ''
        if ($g -match '([A-Z]{4}\d{5})') { $m = Mit-Name $Matches[1]; if ($m -ne $Matches[1]) { $n = "$n - $($m -replace '^[A-Z]{4}\d{5} ', '')" } }
        Merke $pf 6 "Fake-Lizenz: $n (Spiel ist nicht mehr installiert)"
    }
    foreach ($u in $homes) {
        if ((Namen "/user/home/$u/webkit/shell/local").Count -gt 0) {
            Merke @("/user/home/$u/webkit/shell/local") 6 'Browserspeicher (Jailbreak-Seiten)'
        }
    }
    # Einträge fest nach Bereich ordnen (innerhalb eines Bereichs bleibt die
    # Fundreihenfolge), damit Anzeige, Nummern und Liste immer zusammenpassen.
    $sortiert = New-Object System.Collections.ArrayList
    foreach ($k in 1..7) { foreach ($f in $script:Funde) { if ($f.Kat -eq $k) { $null = $sortiert.Add($f) } } }
    $script:Funde = $sortiert
    $script:FUNDE_IP = "$($script:Ftp.Ip):$($script:Ftp.Port)"
}

function Anzahl([int]$k) { return @($script:Funde | Where-Object { $_.Kat -eq $k }).Count }
function Anzahl-Komplett([int]$k) { return @($script:Funde | Where-Object { $_.Kat -eq $k -and -not $_.Gesperrt -and -not $_.PSObject.Properties['Paket'] }).Count }
function Gewaehlt { return ,@($script:Funde | Where-Object { $_.Wahl }) }
function Hat-Pakete { return (@($script:Funde | Where-Object { $_.PSObject.Properties['Paket'] }).Count -gt 0) }

# ---------------------------------------------------------------------
# Auswahl-Helfer
# ---------------------------------------------------------------------
# KOMPLETT: alles auf der internen SSD (Bereich 1-6), M.2 nur auf Wunsch.
# Trophäen-Pakete nur, wenn die M.2 mit geleert wird.
# ShadowMount-Spiele (Bereich 1) auf "wie gefunden" zurücksetzen: nur Kachel,
# Text ohne Zusatz. Vor jeder neuen Entscheidung aufrufen.
function SM-Zuruecksetzen {
    foreach ($f in $script:Funde) {
        if ($f.Kat -eq 1 -and $f.Quelle -ne '') { $f.Pfade = $f.PfadeBasis; $f.Text = $f.TextBasis }
    }
}
function Waehle-Komplett([bool]$m2Leeren) {
    SM-Zuruecksetzen
    foreach ($f in $script:Funde) { $f.Wahl = $false }
    foreach ($f in $script:Funde) { if ($f.Kat -le 6 -and -not $f.Gesperrt -and -not $f.PSObject.Properties['Paket']) { $f.Wahl = $true } }
    if ($m2Leeren) { foreach ($f in $script:Funde) { if (($f.Kat -eq 7 -or $f.PSObject.Properties['Paket']) -and -not $f.Gesperrt) { $f.Wahl = $true } } }
    # ShadowMount-Spiele: liegt das Spiel auf der internen SSD, geht es mit;
    # liegt es auf der M.2, nur wenn die M.2 geleert wird (dann über Bereich 7).
    foreach ($f in $script:Funde) {
        if ($f.Kat -ne 1 -or $f.Quelle -eq '' -or $f.Gesperrt) { continue }
        if ($f.Quelle -like '/user/data/*') { $f.Pfade = @($f.Quelle) + $f.PfadeBasis; $f.Text = "$($f.TextBasis) - Spiel + Kachel" }
        elseif ($m2Leeren)                  { $f.Text = "$($f.TextBasis) - Spiel + Kachel (über Bereich 7)" }
        else                                { $f.Text = "$($f.TextBasis) - nur Kachel, Spiel bleibt" }
    }
}
# ERWEITERT: gewählte ShadowMount-Spiele aus Bereich 1 - Spieldateien mit oder nicht
function SM-Gewaehlt { return ,@($script:Funde | Where-Object { $_.Wahl -and $_.Kat -eq 1 -and $_.Quelle -ne '' }) }
function Setze-SM-Spieldateien([bool]$mit) {
    SM-Zuruecksetzen
    foreach ($f in (SM-Gewaehlt)) {
        if ($mit) { $f.Pfade = @($f.Quelle) + $f.PfadeBasis; $f.Text = "$($f.TextBasis) - Spiel + Kachel" }   # Spieldatei zuerst
        else      { $f.Text = "$($f.TextBasis) - nur Kachel, Spiel bleibt" }
    }
}

# ---------------------------------------------------------------------
# Ausführen (Jailbreak-Tools zuletzt). $Weiter wird gefragt, wenn ShadowMount
# nicht sauber angehalten werden konnte; liefert $true = trotzdem löschen.
# Ergebnis: @{ Abgebrochen; Fehler; SmGestoppt }
# ---------------------------------------------------------------------
# Zeitlimit und "nicht alles gezählt"-Marke für die Größenberechnung. Das
# Durchzählen großer M.2-Spiele über FTP kann lange dauern oder hängen; nach
# Ablauf der Frist wird abgebrochen und die Summe als Untergrenze ausgewiesen.
$script:GROESSE_DEADLINE = [datetime]::MaxValue
$script:GROESSE_GEKAPPT = $false
function Groesse-Zeit-Aus { return ((Get-Date) -gt $script:GROESSE_DEADLINE) }
# Zwischenstand beim Zählen (das Fenster zeigt ihn in der Statuszeile an)
$script:GroesseStand = { param([int64]$bytes, [int]$dateien) }
$script:G_LAUF = @{ Bytes = [int64]0; Dateien = 0; Zeit = [datetime]::MinValue }
# Groesse eines Pfads (Datei oder Ordner samt Inhalt): Bytes, Dateien, Ordner
function Baum-Groesse([string]$p) {
    $erg = @{ Bytes = [int64]0; Dateien = 0; Ordner = 0 }
    $eltern = $p.Substring(0, $p.LastIndexOf('/')); if ($eltern -eq '') { $eltern = '/' }
    $name = $p.Substring($p.LastIndexOf('/') + 1)
    $l = try { Liste $eltern } catch { $script:GROESSE_GEKAPPT = $true; return $erg }
    if ($null -eq $l) { return $erg }
    $e = $l | Where-Object { $_.Name -ceq $name } | Select-Object -First 1
    if ($null -eq $e) { return $erg }
    if (-not $e.IstOrdner) { $erg.Bytes = $e.Groesse; $erg.Dateien = 1; return $erg }
    $erg.Ordner = 1
    $stapel = New-Object System.Collections.Stack; $stapel.Push($p)
    while ($stapel.Count -gt 0) {
        if ((Groesse-Zeit-Aus) -or ($erg.Dateien + $erg.Ordner -gt 200000)) { $script:GROESSE_GEKAPPT = $true; break }
        $d = $stapel.Pop()
        $k = try { Liste $d } catch { $script:GROESSE_GEKAPPT = $true; break }
        if ($null -eq $k) { continue }
        foreach ($x in $k) {
            if ($x.IstOrdner) { $erg.Ordner++; $stapel.Push("$d/$($x.Name)") }
            else { $erg.Dateien++; $erg.Bytes += $x.Groesse; $script:G_LAUF.Dateien++; $script:G_LAUF.Bytes += $x.Groesse }
        }
        if (((Get-Date) - $script:G_LAUF.Zeit).TotalMilliseconds -gt 400) {
            $script:G_LAUF.Zeit = Get-Date
            & $script:GroesseStand $script:G_LAUF.Bytes $script:G_LAUF.Dateien
        }
    }
    return $erg
}
# Groesse aller gewaehlten Eintraege ermitteln (vor dem Loeschen); Ergebnis an
# jedem Eintrag als Bytes/Dateien/Ordner, Summe als @{Bytes;Dateien;Ordner}
function Ermittle-Groessen {
    Abhaengige-Mitnehmen
    $sum = @{ Bytes = [int64]0; Dateien = 0; Ordner = 0; Gekappt = $false }
    $gesehen = @{}
    # Höchstens ein paar Sekunden zählen und dabei kurze FTP-Wartezeit, damit ein
    # langsamer Ordner nicht das ganze Fenster blockiert.
    $script:GROESSE_GEKAPPT = $false
    $script:GROESSE_DEADLINE = (Get-Date).AddSeconds(60)
    $script:G_LAUF = @{ Bytes = [int64]0; Dateien = 0; Zeit = [datetime]::MinValue }
    $altTimeout = $script:FTP_TIMEOUT
    $script:FTP_TIMEOUT = 4000
    # Die Verbindung kann stillgelegen haben (z.B. während ein Dialog offen war)
    # und von der PS5 abgehängt worden sein - vor dem Zählen auffrischen.
    try { if (-not (Ftp-Gesund)) { $null = Ftp-Verbinden $script:Ftp.Ip $script:Ftp.Port } } catch { }
    try {
        foreach ($f in (Gewaehlt)) {
            $b = [int64]0; $nd = 0; $no = 0
            foreach ($p in $f.Pfade) {
                if ($gesehen.ContainsKey($p)) { continue }
                $gesehen[$p] = 1
                $g = Baum-Groesse $p
                $b += $g.Bytes; $nd += $g.Dateien; $no += $g.Ordner
            }
            foreach ($n in @('Bytes','Dateien','Ordner')) { if (-not $f.PSObject.Properties[$n]) { $f | Add-Member -NotePropertyName $n -NotePropertyValue 0 } }
            $f.Bytes = $b; $f.Dateien = $nd; $f.Ordner = $no
            $sum.Bytes += $b; $sum.Dateien += $nd; $sum.Ordner += $no
        }
    } finally { $script:FTP_TIMEOUT = $altTimeout }
    # Nach dem kurzen FTP-Timeout kann die Verbindung aus dem Takt sein - sauber
    # zurücksetzen, damit das spätere Löschen wieder klappt.
    if ($script:GROESSE_GEKAPPT) {
        try { if (-not (Ftp-Gesund)) { $null = Ftp-Verbinden $script:Ftp.Ip $script:Ftp.Port } } catch { }
    }
    $sum.Gekappt = $script:GROESSE_GEKAPPT
    return $sum
}
function Groesse-Text([int64]$bytes) {
    if ($bytes -ge 1GB) { return ('{0:N1} GB' -f ($bytes / 1GB)) }
    if ($bytes -ge 1MB) { return ('{0:N1} MB' -f ($bytes / 1MB)) }
    if ($bytes -ge 1KB) { return ('{0:N0} KB' -f ($bytes / 1KB)) }
    return "$bytes Byte"
}
# "etwa 12,3 GB in 3.125 Dateien und 202 Ordnern" - oder, wenn die Zählung
# wegen des Zeitlimits nicht fertig wurde, "mindestens 12,3 GB (...)".
function Groesse-Satz($g) {
    $wie = if ($g.Gekappt) { 'mindestens' } else { 'etwa' }
    $t = ('{0} {1} in {2:N0} Dateien und {3:N0} Ordnern' -f $wie, (Groesse-Text $g.Bytes), $g.Dateien, $g.Ordner)
    if ($g.Gekappt) { $t += ' (große Ordner nicht ganz durchgezählt)' }
    return $t
}
function Freigeraeumt-Text($g) {
    $wie = if ($g.Gekappt) { 'mindestens ' } else { '' }
    return ('Freigeräumt: {0}{1} in {2:N0} Dateien und {3:N0} Ordnern' -f $wie, (Groesse-Text $g.Bytes), $g.Dateien, $g.Ordner)
}

# Nur einen wirklich leeren Ordner entfernen. RMD löscht nie Inhalt mit
# (anders als RMDA) - auch wenn die Liste davor aus irgendeinem Grund falsch war.
function Leeren-Ordner-Entfernen([string]$o) {
    try {
        if (-not (Erlaubt $o)) { return }
        $l = Liste $o
        if ($null -eq $l -or $l.Count -ne 0) { return }
        $a = Ftp-Befehl "RMD $o"
        if ($a.Code -lt 400) { & $script:Melde "  gelöscht   leerer Ordner $o" }
    } catch { }
}
$script:FUNDE_IP = ''               # von welcher PS5 (IP:Port) die aktuelle Liste stammt
function Fuehre-Loeschung([scriptblock]$Weiter) {
    Abhaengige-Mitnehmen
    $script:LINK_CACHE = @{}
    $gewaehlt = Gewaehlt
    $fehl = 0
    $frei = @{ Bytes = [int64]0; Dateien = 0; Ordner = 0; Gekappt = [bool]$script:GROESSE_GEKAPPT }
    # Die Liste muss von genau der PS5 stammen, mit der gerade verbunden ist.
    if ($null -eq $script:Ftp -or $script:FUNDE_IP -eq '' -or "$($script:Ftp.Ip):$($script:Ftp.Port)" -ne $script:FUNDE_IP) {
        & $script:Melde 'FEHLER: Die Liste stammt nicht von der gerade verbundenen PS5 - nichts gelöscht. Bitte neu verbinden und nachschauen.'
        return @{ Abgebrochen = $true; Fehler = 1; SmGestoppt = $false; Frei = $frei }
    }
    # Erst sicherstellen, dass die Verbindung sauber antwortet - sonst würde
    # jede Prüfung "schon weg" melden, ohne dass etwas passiert.
    if (-not (Ftp-Gesund)) {
        & $script:Melde 'Verbindung zur PS5 antwortet nicht sauber - verbinde neu ...'
        $ok = $false
        try { $ok = Ftp-Verbinden $script:Ftp.Ip $script:Ftp.Port } catch { $ok = $false }
        if (-not $ok -or -not (Ftp-Gesund)) {
            & $script:Melde 'FEHLER: Keine saubere Verbindung zur PS5 - nichts gelöscht. Bitte neu verbinden und nochmal nachschauen.'
            return @{ Abgebrochen = $true; Fehler = 1; SmGestoppt = $false; Frei = $frei }
        }
    }
    # Sobald ShadowMount-Kacheln, -Spiele, -Daten oder die M.2 dran sind: erst anhalten.
    $smBetroffen = @($gewaehlt | Where-Object {
        $_.Quelle -ne '' -or $_.Kat -eq 7 -or $_.Text -like '*Kachel*' -or
        @($_.Pfade | Where-Object { $_ -like "$SM_ORDNER*" -or $_ -eq '/user/data' }).Count -gt 0 }).Count
    $smGestoppt = $false
    if ($smBetroffen -gt 0) {
        $smGestoppt = ShadowMount-Anhalten
        if (-not $smGestoppt -and (Existiert "$SM_ORDNER/debug.log")) {
            & $script:Melde ''
            & $script:Melde ' ShadowMount läuft womöglich noch. Dann werden gelöschte Kacheln gleich'
            & $script:Melde ' wieder angelegt und die Spiele könnten gerade eingebunden sein.'
            if (-not (& $Weiter)) { return @{ Abgebrochen = $true; Fehler = 0; SmGestoppt = $false; Frei = $frei } }
        }
        & $script:Melde ''
    }
    & $script:Melde 'Lösche ... (große Spiele können etwas dauern)'
    $altTimeout = $script:FTP_TIMEOUT
    $script:FTP_TIMEOUT = 7200000                     # Löschen großer Dateien kann Minuten dauern - solange die Verbindung steht, warten
    $spielFehlt = @{}                                 # Gruppen, deren Spiel nicht gelöscht werden konnte
    $verloren = $false
    try {
        foreach ($k in @(1, 2, 3, 4, 5, 7, 6)) {
            foreach ($f in ($gewaehlt | Where-Object { $_.Kat -eq $k })) {
                if ($verloren) { $fehl++; continue }
                # Update/DLC eines Spiels, das selbst nicht gelöscht werden konnte: bleiben
                if ($null -ne $f.PSObject.Properties['Teil'] -and $f.Teil -ne 'Spiel' -and $spielFehlt.ContainsKey($f.Gruppe)) {
                    & $script:Melde "  übersprungen   $($f.Text) (das Spiel selbst konnte nicht gelöscht werden)"; $fehl++; continue
                }
                $ok = $true
                $script:WAR_SCHON_WEG = 0
                foreach ($p in $f.Pfade) {
                    try { if (-not (Loeschen $p)) { $ok = $false } }
                    catch {
                        $ok = $false
                        & $script:Melde "    ($p : $($_.Exception.Message))"
                        # Steht die Verbindung noch? Kurz nachfragen (höchstens 10 s).
                        $t = $script:FTP_TIMEOUT; $script:FTP_TIMEOUT = 10000
                        try { if ((Ftp-Befehl 'NOOP').Code -ne 200) { $verloren = $true } } catch { $verloren = $true }
                        $script:FTP_TIMEOUT = $t
                    }
                    # Der Rest des Eintrags (Kachel, Infos, Lizenz) nur, wenn das
                    # Vorherige wirklich weg ist - sonst bliebe ein Spiel ohne Kachel.
                    if (-not $ok) { break }
                }
                if ($ok -and $script:WAR_SCHON_WEG -ge $f.Pfade.Count) { & $script:Melde "  war schon weg   $($f.Text)"; continue }
                if ($ok) {
                    & $script:Melde "  gelöscht   $($f.Text)"
                    if ($f.PSObject.Properties['Bytes']) { $frei.Bytes += $f.Bytes; $frei.Dateien += $f.Dateien; $frei.Ordner += $f.Ordner }
                } else {
                    & $script:Melde "  FEHLER      $($f.Text) (konnte nicht ganz gelöscht werden)"; $fehl++
                    if (Ist-Teil $f 'Spiel') { $spielFehlt[$f.Gruppe] = 1 }
                }
            }
        }
        if ($verloren) {
            & $script:Melde 'FEHLER: Die Verbindung zur PS5 ist abgerissen - der Rest wurde nicht gelöscht. Bitte neu verbinden und nachschauen.'
        } else {
            # Ordner, die nur wegen ShadowMount-Spielen einzeln gelistet waren und
            # jetzt leer sind, auch entfernen (innerste zuerst).
            $aufgeklappt = @($script:AUFGEKLAPPT); [array]::Reverse($aufgeklappt)
            foreach ($o in $aufgeklappt) {
                $betroffen = @($gewaehlt | Where-Object { @($_.Pfade | Where-Object { $_.StartsWith("$o/") }).Count -gt 0 }).Count
                if ($betroffen -gt 0) { Leeren-Ordner-Entfernen $o }
            }
            # Ordner von Spielen, deren Teile gelöscht wurden und die jetzt leer sind
            foreach ($g in @($script:LEER_DANACH.Keys)) {
                $betroffen = $false
                foreach ($f in $gewaehlt) { if ($null -ne $f.PSObject.Properties['Gruppe'] -and $f.Gruppe -eq $g) { $betroffen = $true } }
                if (-not $betroffen) { continue }
                foreach ($o in $script:LEER_DANACH[$g]) { Leeren-Ordner-Entfernen $o }
            }
        }
    } finally { $script:FTP_TIMEOUT = $altTimeout }
    return @{ Abgebrochen = $false; Fehler = $fehl; SmGestoppt = $smGestoppt; Frei = $frei; SmText = (SM-Schlusstext $gewaehlt); Verloren = $verloren }
}
# Was der Nutzer zu ShadowMount wissen muss, je nachdem was gelöscht wurde.
function SM-Schlusstext($gewaehlt) {
    $smDatenWeg = $false; $kachelnWeg = $false
    foreach ($f in $gewaehlt) {
        foreach ($p in $f.Pfade) { if ($p -eq $SM_ORDNER -or $p -eq '/user/data') { $smDatenWeg = $true } }
        if ($f.Kat -eq 1 -and $f.Quelle -ne '') { $kachelnWeg = $true }
    }
    if ($smDatenWeg) { return 'ShadowMount wurde mitgelöscht - richte es neu ein, dann erscheinen die Spiele auf der M.2 wieder als Kacheln.' }
    if ($script:SM_ANGEHALTEN) { return 'ShadowMount wurde angehalten - zum Weiterspielen einfach neu laden.' }
    if ($kachelnWeg) { return 'Die Spiele auf der Platte erscheinen wieder als Kacheln, sobald ShadowMount läuft.' }
    return ''
}

# Hinweis-Text zum Schluss (Datenbank neu erstellen)
# Ohne Einrückung, jeder Schritt eine eigene Zeile - so bricht es auch im
# Fertig-Fenster sauber um.
$script:SCHLUSS_TEXT = @(
    'Letzter Schritt an der PS5, damit sie alles sauber neu einliest:',
    '1. PS5 ausschalten.',
    '2. Einschaltknopf gedrückt halten, bis es zum zweiten Mal piept.',
    "3. Controller per USB anstecken und 'Datenbank neu erstellen' wählen.",
    "Wichtig: NICHT 'PS5 zurücksetzen' oder 'Systemsoftware neu installieren' wählen!")
function Ftp-Trennen { try { $null = Ftp-Befehl 'QUIT' } catch { } }
