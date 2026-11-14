#!/bin/bash
# PS5 Cleanup 1.0.8 (04.11.2026, Arbeitsplan 1.2) - SDL2 für die PS5 bauen, jetzt mit -ffile-prefix-map: im fertigen Programm
# steht dann kein Bau-Pfad mehr (bis 1.0.7 wurde er in Releases/1.0.7 per release_107.py überschrieben). Gleiche Quelle
# (1.0.7/referenz/SDL), Einstellungen wie build-scripts/ps5-payload-sdk.sh, aber ohne OpenGL (wie die bisherige Bibliothek). Läuft in WSL, nur bauen, nichts an die PS5.
# Die neue Bibliothek ersetzt bau/sdl2/lib nur, wenn die erzeugten Kopfdateien (SDL_config.h, SDL_revision.h) gleich sind wie
# die vorhandenen und kein Bau-Pfad mehr drinsteht. Die alte Bibliothek kommt vorher nach ~/ps5c-bau/sdl2-alt (in WSL).
# Aufruf aus Git-Bash: wsl.exe -e bash -lc 'bash "/pfad/zu/PS5-Cleanup/1.0.10/app/sdl2_bauen.sh"'
set -e
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
K="/pfad/zu/PS5-Cleanup/1.0.10"
S=$HOME/ps5c-bau/sdl2-alt
Q=$HOME/ps5c-bau/sdl2-quelle; B=$HOME/ps5c-bau/sdl2-bau
date "+%Y-%m-%d %H:%M:%S Start"
rm -rf "$Q" "$B"; mkdir -p "$Q"
cp -r "$K/referenz/SDL/." "$Q/"
echo "Quelle kopiert: $(du -sh "$Q" | cut -f1)"
source "$PS5_PAYLOAD_SDK/toolchain/prospero.sh"
${CMAKE} -DCMAKE_BUILD_TYPE=Release -DSDL_OPENGL=OFF -DSDL_LOADSO=YES "-DCMAKE_C_FLAGS=-ffile-prefix-map=$Q=SDL" -B "$B" -S "$Q" > "$B.cmake.txt" 2>&1 || { tail -30 "$B.cmake.txt"; echo "CMAKE FEHLGESCHLAGEN"; exit 1; }
${MAKE} -C "$B" -j"$(nproc)" > "$B.make.txt" 2>&1 || { tail -30 "$B.make.txt"; echo "MAKE FEHLGESCHLAGEN"; exit 1; }
date "+%Y-%m-%d %H:%M:%S gebaut"
ls -la "$B"/*.a
NEU="$B/libSDL2.a"; [ -f "$NEU" ] || { echo "libSDL2.a fehlt"; exit 1; }
OK=1
for h in SDL_config.h SDL_revision.h; do
  n=$(find "$B" -name "$h" | head -1)
  if [ -z "$n" ]; then echo "UNTERSCHIED $h: im Bau nicht gefunden"; OK=0
  elif cmp -s "$n" "$K/bau/sdl2/include/SDL2/$h"; then echo "GLEICH $h ($n)"
  else echo "UNTERSCHIED $h ($n):"; diff "$K/bau/sdl2/include/SDL2/$h" "$n" | head -20; OK=0; fi
done
A1=$(grep -a -c "/mnt/f/" "$NEU" || true); A2=$(grep -a -c "projekt" "$NEU" || true); A3=$(grep -a -c "/home/workstation" "$NEU" || true); A4=$(grep -a -c "ps5c-bau" "$NEU" || true)
echo "Bau-Pfade in der neuen Bibliothek: /mnt/f/ $A1, projekt $A2, /home/workstation $A3, ps5c-bau $A4 (alle 0 = sauber)"
echo "so steht die Datei jetzt drin: $(grep -a -o "SDL/src/render/software/SDL_render_sw.c" "$NEU" | head -1)"
[ "$A1$A2$A3$A4" = "0000" ] || OK=0
if [ $OK = 1 ]; then
  mkdir -p "$S"; cp "$K/bau/sdl2/lib/libSDL2.a" "$K/bau/sdl2/lib/libSDL2main.a" "$S/"
  cp "$NEU" "$K/bau/sdl2/lib/libSDL2.a"
  [ -f "$B/libSDL2main.a" ] && cp "$B/libSDL2main.a" "$K/bau/sdl2/lib/libSDL2main.a"
  echo "ERSETZT: bau/sdl2/lib (alte Fassung in $S)"
  sha256sum "$S/libSDL2.a" "$K/bau/sdl2/lib/libSDL2.a"
else
  echo "NICHT ERSETZT: bau/sdl2/lib bleibt wie bisher (neue Bibliothek liegt in $B)"
fi
date "+%Y-%m-%d %H:%M:%S FERTIG"
