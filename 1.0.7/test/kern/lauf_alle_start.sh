#!/bin/bash
# Startet lauf_alle.sh in WSL im Hintergrund (Aufruf aus Git-Bash)
nohup wsl.exe -e bash -lc 'bash "/pfad/zu/PS5-Cleanup/1.0.7/test/kern/lauf_alle.sh"' > /dev/null 2>&1 &
echo gestartet
