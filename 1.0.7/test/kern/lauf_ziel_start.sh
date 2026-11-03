#!/bin/bash
nohup wsl.exe -e bash -lc 'ZIEL="$1" bash "/pfad/zu/PS5-Cleanup/1.0.7/test/kern/lauf_ziel.sh"' _ "$1" > /dev/null 2>&1 &
echo gestartet
