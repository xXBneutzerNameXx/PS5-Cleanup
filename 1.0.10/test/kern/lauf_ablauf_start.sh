#!/bin/bash
# Startet lauf_ablauf.sh (Vergleich F) in WSL im Hintergrund (Aufruf aus Git-Bash)
nohup wsl.exe -e bash -lc 'bash "/pfad/zu/PS5-Cleanup/1.0.10/test/kern/lauf_ablauf.sh"' > /dev/null 2>&1 &
echo gestartet
