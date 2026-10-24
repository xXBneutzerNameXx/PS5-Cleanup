@echo off
rem Lizenz: MIT - Copyright (c) 2026 da_Chris86 (siehe LICENSE)
rem Baut PS5-Cleanup.exe mit dem C#-Compiler, der in Windows schon enthalten ist
rem (.NET Framework 4.x). Nichts zu installieren. Ergebnis: ..\PS5-Cleanup.exe
setlocal
cd /d "%~dp0"
set FW=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319
if not exist "%FW%\csc.exe" set FW=%WINDIR%\Microsoft.NET\Framework\v4.0.30319
if not exist "%FW%\csc.exe" (echo .NET Framework 4 nicht gefunden. & exit /b 1)
"%FW%\csc.exe" -nologo -target:winexe -optimize+ -out:..\PS5-Cleanup.exe -win32icon:cleanup.ico ^
  -resource:Fenster.xaml,PS5Cleanup.Fenster.xaml -resource:ps5info.elf,PS5Cleanup.ps5info.elf ^
  -resource:..\LIESMICH.txt,PS5Cleanup.LIESMICH.txt -resource:cleanup.ico,PS5Cleanup.cleanup.ico ^
  -r:"%FW%\WPF\PresentationFramework.dll" -r:"%FW%\WPF\PresentationCore.dll" -r:"%FW%\WPF\WindowsBase.dll" ^
  -r:"%FW%\System.Xaml.dll" -r:"%FW%\System.Web.Extensions.dll" -r:"%FW%\System.Xml.dll" -r:"%FW%\System.Core.dll" ^
  Ftp.cs Sitzung.cs Kern.cs Suche.cs Ausfuehren.cs Fenster.cs Programm.cs Selbsttest.cs UiTest.cs
if errorlevel 1 (echo FEHLER beim Bauen. & exit /b 1)
echo FERTIG: ..\PS5-Cleanup.exe
