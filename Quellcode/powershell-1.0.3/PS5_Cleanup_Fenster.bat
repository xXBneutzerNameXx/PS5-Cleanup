@echo off
title PS5 Cleanup by da_Chris86
rem Fensterversion aus dem Skript (STA ist fuer Fenster noetig). Die .exe macht dasselbe.
powershell.exe -NoProfile -ExecutionPolicy Bypass -STA -WindowStyle Hidden -File "%~dp0ps5_cleanup_gui.ps1" %*
