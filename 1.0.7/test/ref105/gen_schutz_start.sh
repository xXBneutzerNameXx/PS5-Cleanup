#!/bin/bash
cd "$(dirname "$0")"
nohup bash gen_schutz.sh > /dev/null 2>&1 &
echo gestartet
