#!/bin/bash
cd "$(dirname "$0")"
nohup bash ablauf_ref.sh > /dev/null 2>&1 &
echo gestartet
