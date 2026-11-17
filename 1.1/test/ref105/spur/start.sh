#!/bin/bash
cd "$(dirname "$0")"
nohup bash spur.sh > /dev/null 2>&1 &
echo gestartet
