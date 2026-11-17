#!/bin/bash
# Startet bauen.sh in WSL im Hintergrund (Aufruf aus Git-Bash); Ergebnis: ausgabe/_bauen.log
nohup wsl.exe -e bash -lc 'bash "/pfad/zu/PS5-Cleanup/1.1/app/bauen.sh"' > /dev/null 2>&1 &
echo gestartet
