#!/bin/bash
# 27.10.2026: nur den Abriss-Fall neu laufen lassen (Ausloeser verlegt, siehe alles.sh)
export MSYS2_ENV_CONV_EXCL="FAILLIST;FAILDEL;FAKELINKS;DIEON"; export MSYS_NO_PATHCONV=1
cd "$(dirname "$0")"; H="$(pwd)"; E="$H/ergebnis"; PY="/f/projekt/ComfyUI/python_embeded/python.exe"
exec > abriss106.log 2>&1
eval "$(sed -n '/^lauf() {/,/^}/p' alles.sh)"
lauf abriss abriss root9 "DIEON=CHMOD 777 /user/appmeta/PPSA88888"
date "+%Y-%m-%d %H:%M:%S FERTIG"
