#!/bin/bash
cd "$(dirname "$0")"
GEN_ZIEL=ergebnis_gen nohup bash gen.sh > /dev/null 2>&1 &
echo gestartet
