# =====================================================================
# bauen.ps1 - baut PS5-Cleanup.exe aus ps5_cleanup_gui.ps1 + ps5_kern.ps1
#
# Werkzeug: ps2exe (Markus Scholtes, MIT-Lizenz, liegt daneben). Es packt das
# PowerShell-Skript in eine .exe, die Windows-PowerShell im Hintergrund nutzt.
# Aufruf (im Ordner "Quellcode\bauen"):  powershell -ExecutionPolicy Bypass -File bauen.ps1
# Ergebnis: ..\..\PS5-Cleanup.exe - eine einzige Datei; ps5info.elf und LIESMICH.txt
#           sind eingebaut und werden beim Start nach %TEMP%\PS5-Cleanup gelegt
# =====================================================================
$ErrorActionPreference = 'Stop'
$hier  = Split-Path -Parent $MyInvocation.MyCommand.Path
$quell  = Split-Path -Parent $hier          # Quellcode-Ordner
$wurzel = Split-Path -Parent $quell         # Ordner, in den die .exe kommt (neben LIESMICH.txt)
$gui   = Get-Content -Raw -Encoding UTF8 (Join-Path $quell 'ps5_cleanup_gui.ps1')
$kern  = Get-Content -Raw -Encoding UTF8 (Join-Path $quell 'ps5_kern.ps1')
$marke = ". (Join-Path `$HIER 'ps5_kern.ps1')"
if (-not $gui.Contains($marke)) { throw 'Einbinde-Zeile für den Kern nicht gefunden.' }
# Der Kern wird direkt eingesetzt - so steckt alles in einer Datei
$gesamt = $gui.Replace($marke, "# ---- ps5_kern.ps1 (eingebettet) ----`r`n" + $kern + "`r`n# ---- Ende Kern ----")
$tmp = Join-Path $hier '_gesamt.ps1'
[System.IO.File]::WriteAllText($tmp, $gesamt, (New-Object System.Text.UTF8Encoding $true))
$ziel = Join-Path $wurzel 'PS5-Cleanup.exe'
if (Test-Path $ziel) { Remove-Item $ziel -Force }
. (Join-Path $hier 'ps2exe.ps1')          # stellt Invoke-ps2exe bereit
$eingebaut = @{ '%TEMP%\PS5-Cleanup\ps5info.elf' = (Join-Path $quell 'ps5info.elf'); '%TEMP%\PS5-Cleanup\LIESMICH.txt' = (Join-Path $wurzel 'LIESMICH.txt') }
Invoke-ps2exe -inputFile $tmp -outputFile $ziel -noConsole -STA -x64 -DPIAware -embedFiles $eingebaut `
    -iconFile (Join-Path $hier 'cleanup.ico') -title 'PS5 Cleanup' -product 'PS5 Cleanup' `
    -description 'PS5 Cleanup - Zustand wie nach einer Neuinstallation (Kopplung und Konto bleiben)' `
    -company 'da_Chris86' -copyright 'Copyright (c) 2026 da_Chris86 - MIT-Lizenz' -version '1.0.3.0'
Remove-Item $tmp -Force
if (Test-Path $ziel) { Write-Output "FERTIG: $ziel ($([math]::Round((Get-Item $ziel).Length / 1KB)) KB)" } else { throw 'Bau fehlgeschlagen.' }
