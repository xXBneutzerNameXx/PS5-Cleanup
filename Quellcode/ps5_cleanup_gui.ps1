# Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
# =====================================================================
# PS5 Cleanup by da_Chris86 - Fensterversion (WPF, Windows-PowerShell, nichts zu installieren)
#
# Gleiche Löschlogik wie die Textversion: beide binden ps5_kern.ps1 ein.
# Das Fenster zeigt nur an und fragt - gelöscht wird ausschließlich über
# die geprüften Kern-Funktionen (Suche-Funde, Waehle-Komplett,
# Fuehre-Loeschung) mit derselben Schutzliste.
#
# Aufruf: PS5_Cleanup_Fenster.bat   (startet powershell.exe -STA -File ...)
#         ps5_cleanup_gui.ps1 -SelbstTest  -> Fenster aufbauen, sofort schließen
# =====================================================================
param([switch]$SelbstTest, [string]$Bild = '')

$HIER = $PSScriptRoot
if ([string]::IsNullOrEmpty($HIER)) { $HIER = Split-Path -Parent ([System.Diagnostics.Process]::GetCurrentProcess().MainModule.FileName) }
#KERN-EINBINDEN# (beim Bau der .exe wird diese Zeile durch den Inhalt von ps5_kern.ps1 ersetzt)
. (Join-Path $HIER 'ps5_kern.ps1')
# Die .exe bringt ps5info.elf und LIESMICH.txt eingebaut mit und legt sie beim
# Start unter %TEMP%\PS5-Cleanup ab. Liegen sie nicht neben dem Programm,
# werden sie von dort genommen.
$script:RES = Join-Path $env:TEMP 'PS5-Cleanup'
if (-not (Test-Path (Join-Path $SkriptOrdner 'ps5info.elf')) -and (Test-Path (Join-Path $script:RES 'ps5info.elf'))) { $SkriptOrdner = $script:RES }
function Liesmich-Pfad {
    foreach ($o in @($HIER, (Split-Path -Parent $HIER), $script:RES)) { $p = Join-Path $o 'LIESMICH.txt'; if (Test-Path $p) { return $p } }
    return ''
}

Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase, System.Xaml

# ---------------------------------------------------------------------
# Fenster (dunkles Design)
# ---------------------------------------------------------------------
[xml]$XAML = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="PS5 Cleanup"
        Width="1040" Height="760" MinWidth="860" MinHeight="600"
        WindowStartupLocation="CenterScreen" Background="#1E1F24" FontFamily="Segoe UI" FontSize="13">
  <Window.Resources>
    <SolidColorBrush x:Key="Fl" Color="#2A2C33"/>
    <SolidColorBrush x:Key="Rand" Color="#3A3D47"/>
    <SolidColorBrush x:Key="Text" Color="#E8E8EA"/>
    <SolidColorBrush x:Key="Grau" Color="#9A9DA8"/>
    <SolidColorBrush x:Key="Blau" Color="#2F7BFF"/>
    <SolidColorBrush x:Key="Rot" Color="#C62828"/>
    <SolidColorBrush x:Key="Gelb" Color="#E0A32B"/>
    <SolidColorBrush x:Key="Gruen" Color="#34A853"/>
    <Style TargetType="TextBlock"><Setter Property="Foreground" Value="{StaticResource Text}"/><Setter Property="TextWrapping" Value="Wrap"/></Style>
    <Style TargetType="CheckBox"><Setter Property="Foreground" Value="{StaticResource Text}"/><Setter Property="Margin" Value="0,2,0,2"/></Style>
    <Style TargetType="Button">
      <Setter Property="Foreground" Value="White"/><Setter Property="Background" Value="{StaticResource Fl}"/>
      <Setter Property="BorderBrush" Value="{StaticResource Rand}"/><Setter Property="Padding" Value="12,6"/>
      <Setter Property="Margin" Value="4"/><Setter Property="Cursor" Value="Hand"/>
      <Setter Property="Template">
        <Setter.Value>
          <ControlTemplate TargetType="Button">
            <Border Background="{TemplateBinding Background}" BorderBrush="{TemplateBinding BorderBrush}" BorderThickness="1" CornerRadius="6" Padding="{TemplateBinding Padding}">
              <ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/>
            </Border>
            <ControlTemplate.Triggers>
              <Trigger Property="IsMouseOver" Value="True"><Setter Property="Opacity" Value="0.85"/></Trigger>
              <Trigger Property="IsEnabled" Value="False"><Setter Property="Opacity" Value="0.4"/></Trigger>
            </ControlTemplate.Triggers>
          </ControlTemplate>
        </Setter.Value>
      </Setter>
    </Style>
    <Style TargetType="TextBox">
      <Setter Property="Background" Value="#16171B"/><Setter Property="Foreground" Value="{StaticResource Text}"/>
      <Setter Property="BorderBrush" Value="{StaticResource Rand}"/><Setter Property="Padding" Value="6,4"/>
      <Setter Property="CaretBrush" Value="White"/>
    </Style>
    <Style TargetType="Expander"><Setter Property="Foreground" Value="{StaticResource Text}"/></Style>
    <Style TargetType="ProgressBar"><Setter Property="Background" Value="#16171B"/><Setter Property="Foreground" Value="{StaticResource Blau}"/><Setter Property="BorderBrush" Value="{StaticResource Rand}"/></Style>
  </Window.Resources>
  <Grid Margin="14">
    <Grid.RowDefinitions>
      <RowDefinition Height="Auto"/>
      <RowDefinition Height="Auto"/>
      <RowDefinition Height="*"/>
      <RowDefinition Height="Auto"/>
      <RowDefinition Height="170"/>
    </Grid.RowDefinitions>

    <!-- Kopf -->
    <Border Grid.Row="0" Background="{StaticResource Fl}" CornerRadius="8" Padding="14" Margin="0,0,0,10">
      <StackPanel>
        <TextBlock FontSize="20" FontWeight="SemiBold" Text="PS5 Cleanup"/>
        <TextBlock Foreground="{StaticResource Grau}" Margin="0,2,0,0"
                   Text="Keine Neuinstallation, kein Zurücksetzen: Es werden nur die Inhalte gelöscht, die du auswählst. Systemsoftware, Laufwerks-Kopplung, Benutzerkonto und Lizenzen bleiben immer. Benutzung auf eigene Gefahr - Gelöschtes ist endgültig weg."/>
      </StackPanel>
    </Border>

    <!-- Verbindung -->
    <Border Grid.Row="1" Background="{StaticResource Fl}" CornerRadius="8" Padding="14" Margin="0,0,0,10">
      <Grid>
        <Grid.ColumnDefinitions>
          <ColumnDefinition Width="Auto"/><ColumnDefinition Width="200"/><ColumnDefinition Width="Auto"/>
          <ColumnDefinition Width="Auto"/><ColumnDefinition Width="Auto"/><ColumnDefinition Width="*"/>
        </Grid.ColumnDefinitions>
        <Grid.RowDefinitions><RowDefinition/><RowDefinition/></Grid.RowDefinitions>
        <TextBlock Grid.Row="0" Grid.Column="0" VerticalAlignment="Center" Margin="0,0,8,0" Text="IP der PS5:"/>
        <TextBox   Grid.Row="0" Grid.Column="1" x:Name="tbIp" VerticalAlignment="Center" ToolTip="PS5: Einstellungen > Netzwerk > Verbindungsstatus anzeigen. Anderer FTP-Port? Dann IP:Port"/>
        <Button    Grid.Row="0" Grid.Column="2" x:Name="btSuchen" Content="PS5 im Netz suchen"/>
        <Button    Grid.Row="0" Grid.Column="3" x:Name="btVerbinden" Content="Verbinden und nachschauen" Background="{StaticResource Blau}" BorderBrush="{StaticResource Blau}"/>
        <CheckBox  Grid.Row="0" Grid.Column="4" x:Name="cbModell" IsChecked="True" VerticalAlignment="Center" Margin="10,0,0,0" Content="Modell automatisch erkennen"
                   ToolTip="Startet kurz das kleine Programm ps5info.elf auf der PS5 (Port 9021 oder 8084) und entfernt es danach wieder."/>
        <CheckBox  Grid.Row="1" Grid.Column="0" Grid.ColumnSpan="3" x:Name="cbTest" IsChecked="True" Margin="0,10,0,0" FontWeight="SemiBold" Foreground="{StaticResource Gruen}"
                   Content="Testmodus: nur anschauen, nichts löschen" ToolTip="Solange der Haken gesetzt ist, sind die Lösch-Knöpfe gesperrt. Zum Löschen den Haken entfernen."/>
        <TextBlock Grid.Row="1" Grid.Column="3" Grid.ColumnSpan="3" x:Name="tbInfo" Margin="10,10,0,0" Foreground="{StaticResource Grau}" Text="Nicht verbunden."/>
      </Grid>
    </Border>

    <!-- Bereiche -->
    <Border Grid.Row="2" Background="{StaticResource Fl}" CornerRadius="8" Padding="6">
      <ScrollViewer VerticalScrollBarVisibility="Auto">
        <StackPanel x:Name="spBereiche" Margin="8">
          <TextBlock x:Name="tbLeer" Foreground="{StaticResource Grau}" Text="Erst verbinden - dann erscheint hier, was auf der PS5 gelöscht werden kann."/>
        </StackPanel>
      </ScrollViewer>
    </Border>

    <!-- Knoepfe -->
    <Grid Grid.Row="3" Margin="0,10,0,10">
      <Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="Auto"/><ColumnDefinition Width="Auto"/><ColumnDefinition Width="Auto"/><ColumnDefinition Width="Auto"/></Grid.ColumnDefinitions>
      <TextBlock Grid.Column="0" x:Name="tbStand" VerticalAlignment="Center" Foreground="{StaticResource Grau}"/>
      <Button Grid.Column="1" x:Name="btProtokoll" Content="Protokoll speichern" ToolTip="Speichert das Protokoll als Textdatei - bei Problemen bitte mitschicken."/>
      <Button Grid.Column="2" x:Name="btHilfe" Content="Hilfe (LIESMICH)"/>
      <Button Grid.Column="3" x:Name="btAuswahl" Content="Nur Auswahl löschen" IsEnabled="False"/>
      <Button Grid.Column="4" x:Name="btKomplett" Content="KOMPLETT - wie neu installiert" Background="{StaticResource Rot}" BorderBrush="{StaticResource Rot}" FontWeight="SemiBold" IsEnabled="False"/>
    </Grid>

    <!-- Protokoll -->
    <Grid Grid.Row="4">
      <Grid.RowDefinitions><RowDefinition Height="Auto"/><RowDefinition Height="*"/></Grid.RowDefinitions>
      <ProgressBar Grid.Row="0" x:Name="pbFortschritt" Height="8" Margin="0,0,0,6" Minimum="0" Maximum="100"/>
      <TextBox Grid.Row="1" x:Name="tbLog" IsReadOnly="True" FontFamily="Consolas" FontSize="12" VerticalScrollBarVisibility="Auto" TextWrapping="NoWrap"/>
      <TextBlock Grid.Row="1" HorizontalAlignment="Right" VerticalAlignment="Bottom" Margin="0,0,22,6" FontSize="11" Foreground="#5A5E6B" IsHitTestVisible="False" x:Name="tbFuss" Text="PS5 Cleanup  ·  da_Chris86  ·  Inoffizielles Community-Projekt, keine Verbindung zu Sony"/>
    </Grid>
  </Grid>
</Window>
'@

$leser = New-Object System.Xml.XmlNodeReader $XAML
$W = [Windows.Markup.XamlReader]::Load($leser)
foreach ($n in @('tbIp','btSuchen','btVerbinden','cbModell','cbTest','tbInfo','btProtokoll','tbFuss','spBereiche','tbLeer','tbStand','btHilfe','btAuswahl','btKomplett','pbFortschritt','tbLog')) {
    Set-Variable -Name $n -Value $W.FindName($n) -Scope Script
}

# ---------------------------------------------------------------------
# Hilfen für die Oberfläche
# ---------------------------------------------------------------------
function Pumpe { # Oberfläche zwischendurch zeichnen lassen
    $null = $W.Dispatcher.Invoke([Windows.Threading.DispatcherPriority]::Background, [Action]{})
}
# Protokolldatei: neben dem Programm, sonst unter %TEMP%\PS5-Cleanup. Jede Zeile
# mit Uhrzeit, Fehler mit Stapel - damit man bei Problemen nachsehen kann.
$script:LOGDATEI = ''
function Log-Datei-Anlegen {
    # eine Datei, jeder Lauf wird angehängt (PS5-Cleanup.log neben dem Programm)
    foreach ($o in @($HIER, $script:RES)) {
        try {
            if (-not (Test-Path $o)) { New-Item -ItemType Directory -Path $o -Force | Out-Null }
            $p = Join-Path $o 'PS5-Cleanup.log'
            if (-not (Test-Path $p)) { [System.IO.File]::WriteAllText($p, '', (New-Object System.Text.UTF8Encoding $true)) }
            else { [System.IO.File]::AppendAllText($p, "`r`n") }
            $script:LOGDATEI = $p; break
        } catch { }
    }
    $os = try { (Get-CimInstance Win32_OperatingSystem).Caption } catch { 'Windows' }
    Log-Zeile ('=' * 60)
    Log-Zeile "PS5 Cleanup $VERSION by da_Chris86 - Protokoll"
    Log-Zeile "Start: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')  |  $os  |  PowerShell $($PSVersionTable.PSVersion)  |  Programm: $HIER"
}
function Log-Zeile([string]$t) {
    if ($script:LOGDATEI -eq '') { return }
    try { Add-Content -Path $script:LOGDATEI -Value ("[" + (Get-Date -Format 'HH:mm:ss') + "] " + $t) -Encoding UTF8 } catch { }
}
function Log([string]$t) {
    $script:tbLog.AppendText($t + "`r`n"); $script:tbLog.ScrollToEnd(); Pumpe
    Log-Zeile $t
}
# Fehler aus einem Knopf-Ablauf: ins Protokoll (mit Stapel) und ein Hinweis
function Fehler-Melden($fehler, [string]$wo) {
    Log "FEHLER bei $wo : $($fehler.Exception.Message)"
    Log-Zeile ("   Ort: " + ($fehler.InvocationInfo.PositionMessage -replace "`r?`n", ' | '))
    Log-Zeile ("   Stapel: " + ($fehler.ScriptStackTrace -replace "`r?`n", ' | '))
    Meldung "Es ist ein Fehler aufgetreten:`n$($fehler.Exception.Message)`n`nDas Protokoll liegt hier:`n$($script:LOGDATEI)`n`nBitte diese Datei bei einer Fehlermeldung mitschicken." 'PS5 Cleanup' 'Error'
}
function Protokoll-Speichern {
    Add-Type -AssemblyName System.Windows.Forms
    $d = New-Object System.Windows.Forms.SaveFileDialog
    $d.Title = 'Protokoll speichern'; $d.Filter = 'Textdatei (*.log;*.txt)|*.log;*.txt|Alle Dateien|*.*'
    $d.FileName = 'PS5-Cleanup_' + (Get-Date -Format 'yyyy-MM-dd_HH-mm') + '.log'
    if ($d.ShowDialog() -ne 'OK') { return }
    try {
        $inhalt = if ($script:LOGDATEI -ne '' -and (Test-Path $script:LOGDATEI)) { Get-Content -Raw $script:LOGDATEI } else { $script:tbLog.Text }
        [System.IO.File]::WriteAllText($d.FileName, $inhalt, (New-Object System.Text.UTF8Encoding $true))
        Log "Protokoll gespeichert: $($d.FileName)"
    } catch { Meldung "Konnte nicht speichern: $($_.Exception.Message)" 'PS5 Cleanup' 'Error' }
}
$script:Melde = { param([string]$t) Log $t }
$script:Warte = { param([int]$ms)
    $ende = (Get-Date).AddMilliseconds($ms)
    while ((Get-Date) -lt $ende) { Pumpe; Start-Sleep -Milliseconds 50 }
}
# Beim Warten auf die PS5 weiterzeichnen; dauert es länger, steht es in der Statuszeile
$script:STAND_BASIS = ''
$script:Pumpe = { param([int]$ms)
    if ($ms -gt 3000) { $script:tbStand.Text = "$script:STAND_BASIS   [PS5 arbeitet seit $([int]($ms / 1000)) s an einer großen Datei ...]" }
    elseif ($ms -eq 0 -and $script:tbStand.Text -ne $script:STAND_BASIS) { $script:tbStand.Text = $script:STAND_BASIS }
    Pumpe
}
function Stand([string]$t) { $script:STAND_BASIS = $t; $script:tbStand.Text = $t; Pumpe }
$script:GroesseStand = { param([int64]$bytes, [int]$dateien)
    Stand ('Ermittle die Größe ... bisher {0} in {1:N0} Dateien' -f (Groesse-Text $bytes), $dateien) }
function Frage-JaNein([string]$text, [string]$titel = 'PS5 Cleanup') {
    $r = [System.Windows.MessageBox]::Show($W, $text, $titel, 'YesNo', 'Question')
    return ($r -eq 'Yes')
}
function Meldung([string]$text, [string]$titel = 'PS5 Cleanup', [string]$art = 'Information') {
    $null = [System.Windows.MessageBox]::Show($W, $text, $titel, 'OK', $art)
}
# Eingabefenster (für die LÖSCHEN-Bestätigung und die Modellnummer)
function Frage-Text([string]$text, [string]$titel) {
    [xml]$x = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Width="520" SizeToContent="Height" WindowStartupLocation="CenterOwner" Background="#1E1F24" FontFamily="Segoe UI" FontSize="13" ResizeMode="NoResize">
  <StackPanel Margin="16">
    <TextBlock x:Name="tx" Foreground="#E8E8EA" TextWrapping="Wrap" Margin="0,0,0,10"/>
    <TextBox x:Name="te" Background="#16171B" Foreground="#E8E8EA" BorderBrush="#3A3D47" Padding="6,4" CaretBrush="White" FontSize="14"/>
    <StackPanel Orientation="Horizontal" HorizontalAlignment="Right" Margin="0,12,0,0">
      <Button x:Name="bAbbr" Content="Abbrechen" Padding="12,6" Margin="4" Foreground="White" Background="#2A2C33" BorderBrush="#3A3D47"/>
      <Button x:Name="bOk" Content="OK" Padding="12,6" Margin="4" Foreground="White" Background="#2F7BFF" BorderBrush="#2F7BFF" IsDefault="True"/>
    </StackPanel>
  </StackPanel>
</Window>
'@
    $d = [Windows.Markup.XamlReader]::Load((New-Object System.Xml.XmlNodeReader $x))
    $d.Title = $titel; $d.Owner = $W
    $d.FindName('tx').Text = $text
    $te = $d.FindName('te')
    $d.FindName('bOk').Add_Click({ $d.DialogResult = $true; $d.Close() })
    $d.FindName('bAbbr').Add_Click({ $d.DialogResult = $false; $d.Close() })
    $te.Focus() | Out-Null
    if ($d.ShowDialog() -eq $true) { return $te.Text.Trim() }
    return $null
}
# Entscheidung mit zwei klar beschrifteten Knöpfen. Der sichere Knopf ist
# vorbelegt (Enter/Esc = sicher), der gefährliche rot und muss angeklickt werden.
function Frage-Wahl([string]$text, [string]$titel, [string]$sicher, [string]$gefaehrlich) {
    [xml]$x = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Width="560" SizeToContent="Height" WindowStartupLocation="CenterOwner" Background="#1E1F24" FontFamily="Segoe UI" FontSize="13" ResizeMode="NoResize">
  <StackPanel Margin="16">
    <TextBlock x:Name="tx" Foreground="#E8E8EA" TextWrapping="Wrap" Margin="0,0,0,10"/>
    <StackPanel Orientation="Horizontal" HorizontalAlignment="Right" Margin="0,12,0,0">
      <Button x:Name="bGef" Padding="12,6" Margin="4" Foreground="White" Background="#8B2A2A" BorderBrush="#A33"/>
      <Button x:Name="bSicher" Padding="12,6" Margin="4" Foreground="White" Background="#2F7BFF" BorderBrush="#2F7BFF" IsDefault="True" IsCancel="True"/>
    </StackPanel>
  </StackPanel>
</Window>
'@
    $d = [Windows.Markup.XamlReader]::Load((New-Object System.Xml.XmlNodeReader $x))
    $d.Title = $titel; $d.Owner = $W
    $d.FindName('tx').Text = $text
    $bs = $d.FindName('bSicher'); $bg = $d.FindName('bGef')
    $bs.Content = $sicher; $bg.Content = $gefaehrlich
    $script:WAHL = $false
    $bg.Add_Click({ $script:WAHL = $true; $d.Close() })
    $bs.Add_Click({ $script:WAHL = $false; $d.Close() })
    $bs.Focus() | Out-Null
    $null = $d.ShowDialog()
    return $script:WAHL
}
function Testmodus { return [bool]$script:cbTest.IsChecked }
# Solange das Programm mit der PS5 spricht, ist alles gesperrt - sonst kann ein
# zweiter Klick in den laufenden Vorgang grätschen und die Verbindung aus dem
# Takt bringen (dann stimmt keine Antwort mehr).
$script:BESCHAEFTIGT = $false
function Sperre([bool]$an) {
    $script:BESCHAEFTIGT = $an
    $script:spBereiche.IsEnabled = -not $an
    $script:cbTest.IsEnabled = -not $an
    $script:cbModell.IsEnabled = -not $an
    $script:tbIp.IsEnabled = -not $an
    Knoepfe (-not $an)
}
function Knoepfe([bool]$an) {
    if ($script:BESCHAEFTIGT) { $an = $false }
    foreach ($b in @($script:btSuchen, $script:btVerbinden, $script:btAuswahl, $script:btKomplett)) { $b.IsEnabled = $an }
    if ($an -and ($script:Funde.Count -eq 0 -or (Testmodus))) { $script:btAuswahl.IsEnabled = $false; $script:btKomplett.IsEnabled = $false }
    if ($an -and -not (Testmodus) -and $script:Funde.Count -gt 0) { $script:btKomplett.IsEnabled = $true; $script:btAuswahl.IsEnabled = ((Gewaehlt).Count -gt 0) }
    Pumpe
}

# ---------------------------------------------------------------------
# PS5 im Heimnetz suchen (FTP-Port 2121 auf allen eigenen /24-Netzen)
# ---------------------------------------------------------------------
function Suche-PS5 {
    $netze = @()
    foreach ($ni in [System.Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces()) {
        if ($ni.OperationalStatus -ne 'Up') { continue }
        foreach ($ua in $ni.GetIPProperties().UnicastAddresses) {
            $a = $ua.Address
            if ($a.AddressFamily -ne 'InterNetwork') { continue }
            $t = $a.ToString()
            if ($t -like '127.*' -or $t -like '169.254.*') { continue }
            $netze += ($t -replace '\.\d+$', '')
        }
    }
    $netze = @($netze | Sort-Object -Unique)
    if ($netze.Count -eq 0) { return @() }
    $treffer = @()
    foreach ($n in $netze) {
        Stand "Suche im Netz $n.0 ..."
        $clients = @()
        foreach ($h in 1..254) {
            $c = New-Object System.Net.Sockets.TcpClient
            try { $null = $c.BeginConnect("$n.$h", $STANDARD_PORT, $null, $null) } catch { }
            $clients += @{ Ip = "$n.$h"; C = $c }
        }
        & $script:Warte 1500
        foreach ($e in $clients) {
            if ($e.C.Connected) { $treffer += $e.Ip }
            try { $e.C.Close() } catch { }
        }
    }
    return $treffer
}

# ---------------------------------------------------------------------
# Bereiche anzeigen
# ---------------------------------------------------------------------
# Jedes Kästchen trägt in .Tag den Eintrag selbst (nicht eine Nummer) - so kann
# ein Häkchen nie den falschen Eintrag markieren, egal in welcher Reihenfolge gesucht wurde.
$script:Kaestchen = @{}      # laufende Nummer -> CheckBox (nur zum Durchgehen)
function Zeige-Bereiche {
    $script:spBereiche.Children.Clear(); $script:Kaestchen = @{}
    $i = 0
    foreach ($k in $script:ALLE_KAT) {
        $eintraege = @($script:Funde | Where-Object { $_.Kat -eq $k })
        $ex = New-Object System.Windows.Controls.Expander
        $ex.IsExpanded = ($eintraege.Count -gt 0 -and $eintraege.Count -le 15)
        $ex.Margin = '0,4,0,4'
        # Kopf: Kaestchen "alle" + Name + Anzahl
        $kopf = New-Object System.Windows.Controls.StackPanel; $kopf.Orientation = 'Horizontal'
        $alle = New-Object System.Windows.Controls.CheckBox
        $alle.Content = "[$k] $($script:KatName[$k])   ($($eintraege.Count) Einträge)"
        $alle.FontWeight = 'SemiBold'; $alle.FontSize = 14
        $alle.IsEnabled = ($eintraege.Count -gt 0)
        if ($k -eq 7 -and -not $script:m2Da) { $alle.Content = "[7] $($script:KatName[7])   (keine M.2 gefunden)" }
        $kopf.Children.Add($alle) | Out-Null
        $ex.Header = $kopf
        $inhalt = New-Object System.Windows.Controls.StackPanel; $inhalt.Margin = '24,2,0,6'
        $info = New-Object System.Windows.Controls.TextBlock
        $info.Text = ((@($script:KatInfo[$k]) | ForEach-Object { $_.Trim() }) -join ' ')
        $info.Foreground = $W.FindResource('Grau'); $info.Margin = '0,0,0,6'
        $inhalt.Children.Add($info) | Out-Null
        if ($k -eq 1 -and @($eintraege | Where-Object { $_.Quelle -ne '' }).Count -gt 0) {
            $h = New-Object System.Windows.Controls.TextBlock; $h.Foreground = $W.FindResource('Gelb'); $h.Margin = '0,0,0,6'
            $h.Text = 'ShadowMount-Spiel: Im Menü ist nur eine Kachel, das Spiel selbst liegt auf der M.2 oder der internen SSD. Beim Löschen wirst du gefragt, ob die Spieldateien mit weg sollen.'
            $inhalt.Children.Add($h) | Out-Null
        }
        $kinder = @()
        foreach ($f in $eintraege) {
            $cb = New-Object System.Windows.Controls.CheckBox
            $cb.Content = $f.Text -replace '_', '__'     # sonst frisst WPF den Unterstrich (Tastenkürzel)
            $cb.Tag = $f
            if ($f.Gesperrt) { $cb.IsEnabled = $false; $cb.ToolTip = 'Der Game Compressor arbeitet gerade daran - gesperrt.' }
            if ($f.PSObject.Properties['Paket']) { $cb.Foreground = $W.FindResource('Gelb') }
            if ($f.Text -like '*Kachel geht mit*' -or $f.Text -like '*tote Kachel*') { $cb.ToolTip = 'ShadowMount: Wird das Spiel gelöscht, verschwindet auch seine Kachel im Menü.' }
            $cb.Add_Click({ param($s, $e)
                $s.Tag.Wahl = [bool]$s.IsChecked
                # Spiel abgewählt -> seine Updates/DLCs auch wieder abwählen
                if ((Ist-Teil $s.Tag 'Spiel') -and -not $s.Tag.Wahl) {
                    foreach ($f in $script:Funde) { if ($null -ne $f.PSObject.Properties['Gruppe'] -and $f.Gruppe -eq $s.Tag.Gruppe -and -not (Ist-Teil $f 'Spiel')) { $f.Wahl = $false } }
                }
                Gruppen-Abgleich; Aktualisiere-Stand })
            $inhalt.Children.Add($cb) | Out-Null
            $script:Kaestchen[$i] = $cb; $kinder += $cb
            $i++
        }
        $alle.Tag = $kinder
        $alle.Add_Click({ param($s, $e)
            foreach ($c in $s.Tag) { if (-not $c.Tag.Gesperrt) { $c.IsChecked = $s.IsChecked; $c.Tag.Wahl = [bool]$s.IsChecked } }
            Gruppen-Abgleich; Aktualisiere-Stand })
        $ex.Content = $inhalt
        $script:spBereiche.Children.Add($ex) | Out-Null
    }
    Gruppen-Abgleich
    Aktualisiere-Stand
}
function Aktualisiere-Stand {
    $n = (Gewaehlt).Count
    $t = if ($n -eq 0) { 'Nichts ausgewählt.' } else { "$n Einträge ausgewählt." }
    if (Testmodus) { $t = "TESTMODUS - es wird nichts gelöscht.  $t" }
    Stand $t
    $script:btAuswahl.IsEnabled = ($n -gt 0 -and -not (Testmodus) -and -not $script:BESCHAEFTIGT)
}
function Zeige-Haken { foreach ($cb in $script:Kaestchen.Values) { $cb.IsChecked = [bool]$cb.Tag.Wahl }; Gruppen-Abgleich }
# Ist ein Spiel angehakt, sind sein Update und seine DLCs ebenfalls angehakt und
# gesperrt (sie gehen mit). Sonst sind sie frei wählbar.
function Gruppen-Abgleich {
    Abhaengige-Mitnehmen
    $basis = @{}
    foreach ($f in $script:Funde) { if ($f.Wahl -and (Ist-Teil $f 'Spiel')) { $basis[$f.Gruppe] = 1 } }
    foreach ($cb in $script:Kaestchen.Values) {
        $f = $cb.Tag
        if ($null -eq $f.PSObject.Properties['Teil'] -or $f.Teil -eq 'Spiel' -or $f.Gesperrt) { continue }
        $cb.IsChecked = [bool]$f.Wahl
        if ($basis.ContainsKey($f.Gruppe)) { $cb.IsEnabled = $false; $cb.ToolTip = 'Geht mit dem Spiel weg.' }
        else { $cb.IsEnabled = $true; $cb.ToolTip = $null }
    }
}

# ---------------------------------------------------------------------
# Verbinden und nachschauen
# ---------------------------------------------------------------------
$script:VERBUNDEN = $false
function Verbinde {
    $eingabe = ($script:tbIp.Text -replace '\s', '')
    $ip = $eingabe; $port = $STANDARD_PORT
    if ($eingabe -match '^(.+):(\d+)$') { $ip = $Matches[1]; $port = [int]$Matches[2] }
    if ($ip -notmatch '^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$') { Meldung 'Bitte eine gültige IP-Adresse eingeben (Beispiel: 192.168.1.50).' 'PS5 Cleanup' 'Warning'; return }
    Knoepfe $false
    $script:tbLog.Clear()
    Log "Verbinde mit PS5 ($ip, Port $port) ..."
    $ok = $false
    try { $ok = Ftp-Verbinden $ip $port } catch { $ok = $false }
    if (-not $ok -or $null -eq (Liste '/user')) {
        Log 'FEHLER: PS5 nicht erreichbar.'
        $script:tbInfo.Text = "Nicht erreichbar. Ist die PS5 an, der Jailbreak geladen und läuft der FTP-Server (Port $port)?"
        return
    }
    Log 'Verbunden.'
    $firmware = Firmware-Lesen
    $modell = ''
    if ($script:cbModell.IsChecked) { Log 'Frage das Modell ab ...'; $modell = Modell-Lesen $ip }
    if ($modell -eq '') {
        $modell = Frage-Text "Modell konnte nicht automatisch erkannt werden.`nDie Modellnummer steht auf dem Aufkleber der Konsole oder unter Einstellungen > System > Systemsoftware > Konsoleninformationen.`n`nModellnummer (z.B. CFI-7016), leer = überspringen:" 'Modell'
        if ($null -eq $modell) { $modell = '' }
        $modell = $modell -replace '\s', ''
    }
    $script:MODELL_TYP = Modell-Typ $modell
    $script:MODELL_DIGITAL = Modell-Digital $modell
$reihe = Modell-Reihe $script:MODELL_TYP $script:MODELL_DIGITAL
    if ($modell -eq '') { $modell = 'unbekannt' }
    $script:tbInfo.Text = "Modell: $modell   |   Reihe: $reihe   |   Firmware: $firmware   |   $(Laufwerk-Text)"
    Log "Modell: $modell  Reihe: $reihe  Firmware: $firmware"
    Log "Laufwerk: $(Laufwerk-Text)"
    Log 'Schaue nach, was drauf ist ...'
    Suche-Funde
    if ($script:KOMPRIMIERUNG.Count -gt 0) {
        Log 'ACHTUNG: Der Game Compressor arbeitet gerade (oder wurde abgebrochen):'
        foreach ($z in $script:KOMPRIMIERUNG) { Log "   $z" }
        Log 'Das betroffene Spiel ist GESPERRT. Lass den Game Compressor erst fertig werden.'
    }
    $script:tbLeer.Visibility = 'Collapsed'
    Zeige-Bereiche
    $script:VERBUNDEN = $true
    if ($script:Funde.Count -eq 0) { Log 'Nichts zu tun - die PS5 ist bereits sauber.' }
    else { Log "$($script:Funde.Count) Einträge gefunden. Nichts wurde verändert." }
    if (Testmodus) { Log '(Testmodus - zum Löschen den Haken "Testmodus" entfernen.)' }
    Knoepfe $true
}

# ---------------------------------------------------------------------
# Löschen
# ---------------------------------------------------------------------
function Zusammenfassung([bool]$komplett) {
    $g = Gewaehlt
    $t = "Wird ENDGÜLTIG gelöscht: $($g.Count) Einträge`n"
    foreach ($k in $script:ALLE_KAT) {
        $c = @($g | Where-Object { $_.Kat -eq $k }).Count
        if ($c -gt 0) { $t += ('   - {0}: {1}' -f $script:KatName[$k], $c) + "`n" }
    }
    if ($komplett -and @($g | Where-Object { $_.Kat -eq 7 }).Count -eq 0) { $t += "   (M.2-SSD bleibt unverändert)`n" }
    $gesp = @($script:Funde | Where-Object { $_.Gesperrt }).Count
    if ($gesp -gt 0) { $t += "   Bleibt (Game Compressor arbeitet daran): $gesp`n" }
    $t += "`nDas sind $(Groesse-Satz $script:GROSS).`n"
    $t += "`n$(Kopplungs-Text)`nBenutzung auf eigene Gefahr - Gelöschtes ist endgültig weg."
    return $t
}
$script:GROSS = @{ Bytes = [int64]0; Dateien = 0; Ordner = 0 }
function Groesse-Ermitteln {
    Stand 'Ermittle die Größe ...'; Knoepfe $false
    $script:pbFortschritt.IsIndeterminate = $true
    $script:GROSS = Ermittle-Groessen
    $script:pbFortschritt.IsIndeterminate = $false
    Stand "Ausgewählt: $(Groesse-Satz $script:GROSS)."
    Knoepfe $true
    Log "Ausgewählt: $(Groesse-Satz $script:GROSS)."
}
$script:LAEUFT = $false            # solange gelöscht wird, lässt sich das Fenster nicht schließen
function Lösche-Jetzt([bool]$komplett) {
    Knoepfe $false
    $script:pbFortschritt.IsIndeterminate = $true
    Stand 'Lösche ... (große Spiele können dauern, das Fenster bleibt dabei am Leben)'
    Log ''
    $script:LAEUFT = $true
    try {
        $erg = Fuehre-Loeschung { return (Frage-JaNein "ShadowMount läuft womöglich noch. Dann werden gelöschte Kacheln gleich wieder angelegt und die Spiele könnten gerade eingebunden sein.`n`nTrotzdem weitermachen?" 'ShadowMount') }
    } finally { $script:LAEUFT = $false }
    $script:pbFortschritt.IsIndeterminate = $false
    Stand ''
    if ($erg.Abgebrochen) { Log 'Abgebrochen - nichts verändert.'; Knoepfe $true; return }
    $script:pbFortschritt.Value = 100
    Log ''
    if ($erg.Fehler -eq 0) { Log 'Fertig - alles Ausgewählte ist gelöscht.' } else { Log "Fertig, aber mit $($erg.Fehler) Fehler(n) - siehe oben." }
    Log (Freigeraeumt-Text $erg.Frei)
    Log (Kopplungs-Text)
    if ($erg.SmText) { Log $erg.SmText }
    foreach ($z in $script:SCHLUSS_TEXT) { Log $z }
    $schluss = ($script:SCHLUSS_TEXT -join "`n")
    $sm = if ($erg.SmText) { "$($erg.SmText)`n`n" } else { '' }
    Meldung "$(if ($erg.Fehler -eq 0) { 'Fertig - alles Ausgewählte ist gelöscht.' } else { "Fertig, aber mit $($erg.Fehler) Fehler(n) - siehe Protokoll." })`n`n$(Freigeraeumt-Text $erg.Frei)`n`n$sm$schluss`n`nProtokoll: $($script:LOGDATEI)" 'Fertig'
    # neu nachschauen, damit die Liste stimmt
    Suche-Funde; Zeige-Bereiche
    Knoepfe $true
}
function Komplett-Klick {
    $t = "KOMPLETT: Auf der internen SSD wird ALLES gelöscht:`n"
    foreach ($k in 1..6) { $t += ('   - {0}: {1}' -f $script:KatName[$k], (Anzahl-Komplett $k)) + "`n" }
    $t += "`nDas heißt:`n - Spielstände ohne Cloud-Sicherung (PS Plus) sind endgültig weg.`n - Nie mit dem PSN abgeglichene Trophäen sind weg.`n - Die Jailbreak-Tools samt Einstellungen sind weg - danach musst du deinen Jailbreak NEU EINRICHTEN.`n - Wer per Browser-Seite jailbreakt: ein offline gespeicherter Zugang im Browserspeicher kann auch weg sein.`n`nBleibt: Systemsoftware, Laufwerks-Kopplung, Benutzerkonto, Lizenzen und die Sony-Systemanwendungen.`n`nWeiter?"
    if (-not (Frage-JaNein $t 'KOMPLETT')) { return }
    $m2Leeren = $false
    if ($script:m2Da -and (Anzahl 7) -gt 0) {
        $m2Leeren = Frage-Wahl "Auf der M.2-SSD liegen $(Anzahl 7) Einträge (Spiele, Ordner, Dateien).`n`nM.2 BEHALTEN: Die Spiele auf der M.2 bleiben unangetastet. Nur die Kacheln im Menü gehen weg - sie kommen wieder, sobald ShadowMount neu eingerichtet ist.`n`nM.2 AUCH LEEREN: Alle Spiele und Dateien auf der M.2 werden endgültig gelöscht." 'M.2-SSD' 'M.2 behalten (empfohlen)' 'M.2 auch leeren'
    }
    Waehle-Komplett $m2Leeren
    Zeige-Haken; Aktualisiere-Stand
    if (-not $m2Leeren -and (Hat-Pakete)) { Log '(Trophäen-Pakete der Spiele bleiben, weil die Spiele auf der M.2 bleiben.)' }
    Groesse-Ermitteln
    $warn = if ($m2Leeren) { "!!! ACHTUNG: DIE M.2-SSD WIRD MIT GELEERT - $(Anzahl 7) Einträge, alle Spiele darauf sind danach weg !!!`n`n" } else { "Die M.2-SSD bleibt unangetastet.`n`n" }
    $a = Frage-Text ($warn + (Zusammenfassung $true) + "`n`nZum Bestätigen LÖSCHEN eintippen (alles andere = abbrechen):") 'Endgültig löschen?'
    if ($a -cne 'LOESCHEN' -and $a -cne 'LÖSCHEN') { Log 'Abgebrochen - nichts verändert.'; foreach ($f in $script:Funde) { $f.Wahl = $false }; Zeige-Haken; Aktualisiere-Stand; return }
    Lösche-Jetzt $true
}
function Auswahl-Klick {
    $sm = SM-Gewaehlt
    if ($sm.Count -gt 0) {
        $l = ($sm | ForEach-Object { '   ' + $_.Text }) -join "`n"
        $mit = Frage-Wahl "Du hast $($sm.Count) ShadowMount-Spiel(e) gewählt:`n$l`n`nNUR KACHELN LÖSCHEN: Die Spiele bleiben auf der Platte und erscheinen wieder, sobald ShadowMount neu geladen ist.`n`nSPIELE + KACHELN LÖSCHEN: Die Spieldateien werden endgültig gelöscht." 'ShadowMount-Spiele' 'Nur Kacheln löschen (empfohlen)' 'Spiele + Kacheln löschen'
        Setze-SM-Spieldateien $mit
    }
    Groesse-Ermitteln
    if (-not (Frage-JaNein ((Zusammenfassung $false) + "`n`nJetzt endgültig löschen?") 'Endgültig löschen?')) {
        Log 'Abgebrochen - nichts verändert.'; return
    }
    Lösche-Jetzt $false
}

# ---------------------------------------------------------------------
# Knoepfe verdrahten
# ---------------------------------------------------------------------
$btSuchen.Add_Click({
    if ($script:BESCHAEFTIGT) { return }
    Sperre $true
    $t = @()
    try { foreach ($x in @(Suche-PS5)) { if ("$x" -match '^\d+\.\d+\.\d+\.\d+$') { $t += "$x" } } }
    catch { Fehler-Melden $_ 'PS5 suchen'; $t = @() }
    finally { Sperre $false }
    if ($t.Count -eq 0) { Stand 'Keine PS5 mit FTP-Server gefunden (ist der Jailbreak geladen?).'; return }
    $script:tbIp.Text = $t[0]
    if ($t.Count -eq 1) { Stand "Gefunden: $($t[0])" } else { Stand "Gefunden: $($t -join ', ') - die erste ist eingetragen." }
})
$btVerbinden.Add_Click({
    if ($script:BESCHAEFTIGT) { return }
    Sperre $true
    try { Verbinde } catch { Fehler-Melden $_ 'Verbinden' }
    finally { Sperre $false; if ($script:VERBUNDEN) { Aktualisiere-Stand } }
})
$W.Add_Closing({ param($s, $e)
    if ($script:LAEUFT) {
        $e.Cancel = $true
        Meldung "Es wird gerade auf der PS5 gelöscht. Bitte warten, bis 'Fertig' erscheint - ein Abbruch mittendrin hinterlässt halb gelöschte Spiele." 'PS5 Cleanup' 'Warning'
    }
})
$btProtokoll.Add_Click({ Protokoll-Speichern })
$cbTest.Add_Click({ if ($script:VERBUNDEN) { Knoepfe $true; Aktualisiere-Stand } })
$btKomplett.Add_Click({
    if ($script:BESCHAEFTIGT) { return }
    Sperre $true
    try { Komplett-Klick } catch { Fehler-Melden $_ 'KOMPLETT'; $script:pbFortschritt.IsIndeterminate = $false }
    finally { Sperre $false; Aktualisiere-Stand }
})
$btAuswahl.Add_Click({
    if ($script:BESCHAEFTIGT) { return }
    Sperre $true
    try { Auswahl-Klick } catch { Fehler-Melden $_ 'Auswahl löschen'; $script:pbFortschritt.IsIndeterminate = $false }
    finally { Sperre $false; Aktualisiere-Stand }
})
$btHilfe.Add_Click({
    $p = Liesmich-Pfad
    if ($p -ne '') { Start-Process notepad.exe $p } else { Meldung 'LIESMICH.txt nicht gefunden.' }
})
$W.Add_Closed({ if ($script:VERBUNDEN) { Ftp-Trennen }; Log-Zeile 'Ende.' })
Log-Datei-Anlegen
$tbFuss.Text = "PS5 Cleanup $VERSION  ·  da_Chris86  ·  Inoffizielles Community-Projekt, keine Verbindung zu Sony"

if ($SelbstTest) {
    # Fenster aufbauen, mit Probe-Einträgen füllen, optional als Bild sichern, schließen.
    # Ergebnis landet in _selbsttest.txt neben dem Bild (als .exe gibt es keine Konsole).
    $protokoll = if ($Bild -ne '') { Join-Path (Split-Path -Parent $Bild) '_selbsttest.txt' } else { Join-Path $HIER '_selbsttest.txt' }
    $schreib = { param($t) Add-Content -Path $protokoll -Value $t -Encoding UTF8 }
    Set-Content -Path $protokoll -Value "Selbsttest $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') HIER=$HIER" -Encoding UTF8
    $script:Funde = New-Object System.Collections.ArrayList
    $probe = @(
        @(1, "Red Dead Redemption (PPSA30528) - ShadowMount-Spiel auf der M.2", '/mnt/ext1/homebrew/x.exfat', $false),
        @(1, "Echtes Spiel (PPSA55555) + Update + Zusatzinhalte", '', $false),
        @(1, "Spiel X (PPSA11111) - ShadowMount-Spiel auf der M.2", '/mnt/ext1/homebrew/y.exfat', $true),
        @(2, "Spielstand PPSA55555 (Echtes Spiel)", '', $false),
        @(3, "Trophäen-Daten trophy/data", '', $false),
        @(4, "Aufnahme content_tmp/PPSA55555 (Echtes Spiel)", '', $false),
        @(5, "Share-Einstellungen alter Spiele (66 Spiele, legt die PS5 bei Bedarf neu an)", '', $false),
        @(5, "Fehlerberichte der Konsole (100)", '', $false),
        @(6, "Kachel des Tools: Payload Manager (PLDM00001)", '', $false),
        @(6, "shadowmount", '', $false),
        @(7, "homebrew/Red Dead Redemption - PPSA30528.exfat  [Spiel: Red Dead Redemption (PPSA30528) - Kachel geht mit]", '/mnt/ext1/homebrew/x.exfat', $false))
    foreach ($e in $probe) {
        $t = $e[1]; if ($e[3]) { $t += '  [GESPERRT: Game Compressor arbeitet gerade daran]' }
        $null = $script:Funde.Add([pscustomobject]@{ Pfade = @('/x'); Kat = $e[0]; Text = $t; Wahl = $false; Quelle = $e[2]; Gesperrt = $e[3] })
    }
    # ein normal installiertes PS4-Spiel mit Update und DLC
    foreach ($t in @(@('DLC', 'Bloodborne (CUSA00900) - Zusatzinhalt: The Old Hunters'), @('Update', 'Bloodborne (CUSA00900) - Update (Version 01.09)'), @('Spiel', 'Bloodborne (CUSA00900) - Spiel'))) {   # rückwärts, weil jeweils an Stelle 1 eingefügt
        $o = [pscustomobject]@{ Pfade = @('/x'); Kat = 1; Text = $t[1]; Wahl = $false; Quelle = ''; Gesperrt = $false; Gruppe = 'CUSA00900'; Teil = $t[0] }
        $null = $script:Funde.Insert(1, $o)
    }
    # absichtlich durcheinander: M.2-Eintrag nach vorn (so wie es eine Suche liefern könnte)
    $m2 = $script:Funde[$script:Funde.Count - 1]; $script:Funde.RemoveAt($script:Funde.Count - 1); $script:Funde.Insert(0, $m2)
    $script:m2Da = $true
    $W.Add_Loaded({
        try {
            $script:tbLeer.Visibility = 'Collapsed'
            $script:tbInfo.Text = 'Modell: CFI-7021 B01Y   |   Reihe: PS5 Pro   |   Firmware: 12.00   |   Laufwerk ist mit DIESER Konsole gekoppelt.'
            $script:tbIp.Text = '192.168.50.78'
            Zeige-Bereiche
            Log 'Selbsttest: Fenster aufgebaut.'; Log 'Verbunden.'; Log '14 Einträge gefunden. Nichts wurde verändert.'
            & $schreib "Fenster aufgebaut: $($script:spBereiche.Children.Count) Bereiche, $($script:Kaestchen.Count) Einträge"
            # Häkchen-Probe: alle Kästchen von Bereich 6 wirklich anklicken, dann
            # darf genau Bereich 6 gewählt sein - nichts anderes.
            # wie ein Mausklick: Haken setzen und das Klick-Ereignis auslösen
            # Die Kästchen werden über den SICHTBAREN Bereich im Fenster gefunden
            # (6. aufklappbarer Bereich), nicht über die interne Zuordnung.
            $soll = 0; $sichtbar6 = @()
            $ex6 = $script:spBereiche.Children[[array]::IndexOf($script:ALLE_KAT, 6)]
            foreach ($c in $ex6.Content.Children) {
                if ($c -is [System.Windows.Controls.CheckBox] -and $c.IsEnabled) {
                    $soll++; $sichtbar6 += $c
                    $c.IsChecked = $true
                    $c.RaiseEvent((New-Object System.Windows.RoutedEventArgs([System.Windows.Controls.Primitives.ButtonBase]::ClickEvent)))
                }
            }
            $sechs = 0; $falsch = 0; $anzeige = 0
            foreach ($f in $script:Funde) { if ($f.Wahl) { if ($f.Kat -eq 6) { $sechs++ } else { $falsch++ } } }
            for ($b = 0; $b -lt $script:spBereiche.Children.Count; $b++) {
                if ($script:ALLE_KAT[$b] -eq 6) { continue }
                foreach ($c in $script:spBereiche.Children[$b].Content.Children) { if ($c -is [System.Windows.Controls.CheckBox] -and $c.IsChecked) { $anzeige++ } }
            }
            $script:HAKEN_OK = ($soll -gt 0 -and $sechs -eq $soll -and $falsch -eq 0 -and $anzeige -eq 0)
            & $schreib "Häkchen-Probe: Bereich 6 angeklickt=$soll, davon gewählt=$sechs, andere gewählt=$falsch, andere angezeigt=$anzeige -> $(if ($script:HAKEN_OK) {'OK'} else {'FEHLER'})"
            foreach ($f in $script:Funde) { $f.Wahl = $false }; Zeige-Haken
            # Teil-Probe: Spiel anklicken -> Update und DLC gehen mit (angehakt, gesperrt);
            # Spiel wieder abwählen -> beide wieder frei und abgewählt.
            $klick = { param($c, [bool]$an) $c.IsChecked = $an; $c.RaiseEvent((New-Object System.Windows.RoutedEventArgs([System.Windows.Controls.Primitives.ButtonBase]::ClickEvent))) }
            $ex1 = $script:spBereiche.Children[[array]::IndexOf($script:ALLE_KAT, 1)]
            $basisCb = $null; $teilCbs = @()
            foreach ($c in $ex1.Content.Children) {
                if ($c -isnot [System.Windows.Controls.CheckBox]) { continue }
                if ("$($c.Content)" -like 'Bloodborne*- Spiel') { $basisCb = $c } elseif ("$($c.Content)" -like 'Bloodborne*') { $teilCbs += $c }
            }
            & $klick $basisCb $true
            $mit = 0; foreach ($c in $teilCbs) { if ($c.Tag.Wahl -and $c.IsChecked -and -not $c.IsEnabled) { $mit++ } }
            & $klick $basisCb $false
            $frei = 0; foreach ($c in $teilCbs) { if (-not $c.Tag.Wahl -and -not $c.IsChecked -and $c.IsEnabled) { $frei++ } }
            # nur das Update wählen -> Spiel und DLC bleiben unberührt
            & $klick $teilCbs[0] $true
            $einzeln = ($teilCbs[0].Tag.Wahl -and -not $basisCb.Tag.Wahl -and -not $teilCbs[1].Tag.Wahl)
            $script:TEIL_OK = ($teilCbs.Count -eq 2 -and $mit -eq 2 -and $frei -eq 2 -and $einzeln)
            & $schreib "Teil-Probe: mitgenommen=$mit/2, wieder frei=$frei/2, Update einzeln=$einzeln -> $(if ($script:TEIL_OK) {'OK'} else {'FEHLER'})"
            foreach ($f in $script:Funde) { $f.Wahl = $false }; Zeige-Haken
            & $schreib "ps5info.elf: $(Join-Path $SkriptOrdner 'ps5info.elf') vorhanden=$(Test-Path (Join-Path $SkriptOrdner 'ps5info.elf'))"
            & $schreib "LIESMICH: $(Liesmich-Pfad)"
            if ($Bild -ne '') {
                Pumpe
                $rtb = New-Object System.Windows.Media.Imaging.RenderTargetBitmap([int]$W.ActualWidth, [int]$W.ActualHeight, 96, 96, [System.Windows.Media.PixelFormats]::Pbgra32)
                $rtb.Render($W)
                $enc = New-Object System.Windows.Media.Imaging.PngBitmapEncoder
                $enc.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($rtb))
                $fs = [System.IO.File]::Create($Bild); $enc.Save($fs); $fs.Close()
                & $schreib "Bild gespeichert: $Bild"
            }
            & $schreib $(if ($script:HAKEN_OK -and $script:TEIL_OK) { 'SELBSTTEST OK' } else { 'SELBSTTEST FEHLER (siehe Probe oben)' })
        } catch { & $schreib "FEHLER: $($_.Exception.Message)" }
        $W.Dispatcher.BeginInvoke([Windows.Threading.DispatcherPriority]::Background, [Action]{ $W.Close() }) | Out-Null })
    try { $null = $W.ShowDialog() } catch { & $schreib "FEHLER beim Anzeigen: $($_.Exception.Message)" }
    exit 0
}
$null = $W.ShowDialog()
