#!/bin/bash
cd "$(dirname "$0")"
nohup bash alles.sh > /dev/null 2>&1 &
echo gestartet
