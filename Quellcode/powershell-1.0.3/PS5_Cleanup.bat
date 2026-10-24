@echo off
chcp 65001 >nul
title PS5 Cleanup by da_Chris86
rem Textversion: startet ps5_cleanup.ps1 aus demselben Ordner mit Windows-PowerShell.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0ps5_cleanup.ps1" %*
echo.
pause
