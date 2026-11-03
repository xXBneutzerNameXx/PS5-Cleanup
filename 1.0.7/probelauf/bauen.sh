#!/bin/bash
# PS5 Cleanup 1.1 – Probelauf bauen (in WSL mit ps5-payload-sdk)
# Aufruf aus Git-Bash:  wsl.exe -e bash -lc 'cd "/pfad/zu/PS5-Cleanup/1.0.7/probelauf" && bash bauen.sh'
set -e
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
SDL="../bau/sdl2"
CC=$PS5_PAYLOAD_SDK/bin/prospero-clang
mkdir -p ausgabe
$CC -Wall -O2 -I"$SDL/include" probelauf.c -L"$SDL/lib" -lSDL2 \
  -lSceVideoOut -lSceAudioOut -lScePad -lSceUserService -lSceSystemService \
  -lSceImeDialog -lSceKeyboard -lkernel_sys \
  -o ausgabe/eboot.elf
ls -la ausgabe/eboot.elf
sha256sum ausgabe/eboot.elf
