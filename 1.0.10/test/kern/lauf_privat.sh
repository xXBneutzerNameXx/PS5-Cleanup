#!/bin/bash
# Test private Ordner von Programmen (Playden & Co., Entwurf 1.0.9, aufgenommen 1.0.10: nur bekannte Programme, nie über Verknüpfungen) - läuft in WSL.
export PLAYDEN_WEB=zu   # NEU 1.0.10 (12.11.2026): Playden gilt im Test als geschlossen (zugriff_datei.c d_playden_web)
# "Privat" am PC = Rechte 000 (wirkt nur ohne root). Echt (seteuid) prüfbar nur an der PS5.
K="/pfad/zu/PS5-Cleanup/1.0.10"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/privat; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/privat.log" 2>&1
set -o pipefail
RC=0
date "+%Y-%m-%d %H:%M:%S Start"
if [ "$(id -u)" = "0" ]; then echo "FAIL läuft als root - Rechte 000 wirken dann nicht, der Test sagt nichts"; echo "FEHLERZAHL 1"; exit 1; fi
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
   $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_privat.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_privat || { echo BAU FEHLGESCHLAGEN; exit 1; }
baum() {   # $1: zugriff = dazu private Ordner in Sony-Bereichen (dort darf nie gewechselt werden)
  [ -d $T/w ] && chmod -R u+rwx $T/w 2>/dev/null; rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  P=$T/w/user/data/playden   # wie Playden (leolobato/playden-ps5 self_heal.cpp): runtime-state, user/game-settings-private, art, games/<Kennung>
  mkdir -p $P/runtime-state/requests $P/runtime-state/trash $P/user/game-settings-private $P/art $P/games/PPSA12345
  printf '{"version":1}\n' > $P/config.json
  head -c 300 /dev/zero | tr '\0' 'x' > $P/runtime-state/state.json      # über 64 Byte: der Selbstschutz liest den Anfang
  printf '{"r":1}\n' > $P/runtime-state/requests/r1.json
  { printf '"SteamLogin"\n{ "user" "x" }\n'; head -c 200 /dev/zero | tr '\0' 'y'; } > $P/user/steam.vdf
  printf '{"x":2}\n' > $P/user/game-settings-private/settings.json
  { printf '\x89PNG\r\n\x1a\n'; head -c 200 /dev/zero; } > $P/art/a.png
  head -c 5000 /dev/zero | tr '\0' 'g' > $P/games/PPSA12345/game.bin
  ln -s runtime-state $P/zeiger
  chmod 1777 $P/user
  M=$T/w/mnt/ext1/homebrew/privatapp; mkdir -p $M/cache; head -c 100 /dev/zero | tr '\0' 'm' > $M/cache/x.bin; printf '{}\n' > $M/app.json
  # NEU 1.0.10: privatapp ist unbekannt (bleibt geschützt) - snes9x ist bekannt (APP_DATEN), dort darf gewechselt werden
  S=$T/w/mnt/ext1/homebrew/snes9x; mkdir -p $S/cache; head -c 100 /dev/zero | tr '\0' 's' > $S/cache/s.bin; printf '{}\n' > $S/app.json
  # NEU 1.0.10 (Codex-Prüfung 126, P1): Verknüpfungen IM privaten Ordner, die aus dem Testbaum hinaus zeigen - nie verfolgen
  rm -rf $T/aussen; mkdir -p $T/aussen; echo geheim > $T/aussen/g.txt
  ln -s ../../../../../aussen $P/runtime-state/aussen; ln -s ../../../../../aussen/g.txt $P/runtime-state/g.lnk
  if [ "$1" = zugriff ]; then
    mkdir -p $T/w/user/home/10000000/privat $T/w/mnt/ext1/user/privat
    echo geheim > $T/w/user/home/10000000/privat/x; echo geheim > $T/w/mnt/ext1/user/privat/x
    chmod 000 $T/w/user/home/10000000/privat $T/w/mnt/ext1/user/privat
    # NEU 1.0.10 (PS5-Test 12.11.2026 19:49): Playdens Listen - die Installationsliste im privaten Ordner (prüfen und austragen als Besitzer)
    printf '{"schema_version":1,"reservations":[{"install_id":"steam-5050","title_id":"FAKE78050"}],"installs":[{"install_id":"steam-5050","title_id":"FAKE78050","app_id":5050,"install_path":"/data/playden-games/5050","state":"ready","journal":null}]}' > $P/runtime-state/installs.json
    chmod 600 $P/runtime-state/installs.json
    mkdir -p $T/w/user/data/playden-games/5050; printf '5050\tinternal\t100\n7\tinternal\t5\n' > $T/w/user/data/playden-games/.playden-games
    # NEU 1.0.10 (22:40, PS5-Test 12:46: RMDA steam-223850.kept scheiterte): aufgehobene Spielstände wie Playden sie anlegt - privat, verschachtelt
    K2=$P/games/steam-223850.kept; mkdir -p "$K2/drive_c/users/steamuser/Saved Games/Bethesda"
    printf 'save1' > "$K2/drive_c/users/steamuser/Saved Games/Bethesda/s1.sav"; printf '{"k":1}\n' > $K2/kept.json
    chmod 000 $K2
    # NEU 1.0.10 (PS5-Test 13.11.2026 08:19): Ordner, die sich öffnen lassen, deren Geöffnetes aber unbrauchbar ist (OFFEN_UNBRAUCHBAR)
    K3=$P/games/steam-77777.kept; mkdir -p $K3/drive_c/a; printf 'x' > $K3/drive_c/a/x.sav; printf '{}' > $K3/kept.json
    K4=$P/games/steam-88888.kept; mkdir -p $K4/innen/tief; printf 'y' > $K4/innen/tief/y.sav; printf 'z' > $K4/z.txt
    mkdir -p $P/games/steam-55555.kept $P/games/tausch-b; printf 'a' > $P/games/steam-55555.kept/a.txt; printf 'b' > $P/games/tausch-b/b.txt   # Codex-Prüfung 184
    K6=$P/games/steam-66666.kept; mkdir -p $K6/innen/ro $P/games/tausch-c; printf 'r' > $K6/innen/ro/r.txt; chmod 555 $K6/innen/ro; printf 'c' > $P/games/tausch-c/c.txt   # Codex-Prüfung 185
    mkdir -p $P/games/steam-33333.kept/innen3/tief $P/games/steam-33334.kept/a $P/games/steam-33335.kept $P/games/tausch-e; printf 'x' > $P/games/steam-33333.kept/innen3/tief/x.sav; printf 'y' > $P/games/steam-33334.kept/a/y.sav; printf 'k' > $P/games/steam-33335.kept/k.txt; printf 'e' > $P/games/tausch-e/e.txt   # PS5 12:25
    mkdir -p $P/games/steam-11111.kept/u1; printf 'd' > $P/games/steam-11111.kept/u1/sid; printf 'x' > $P/games/steam-11111.kept/u1/x.txt   # PS5 12:50
    mkdir -p $P/games/steam-12121.kept/innen5/t; printf 'g' > $P/games/steam-12121.kept/innen5/t/g.txt   # PS5 12:58
    mkdir -p $P/games/steam-13131.kept/innen6/t; printf 'g' > $P/games/steam-13131.kept/innen6/t/g.txt   # Codex-Prüfung 193 (1)
    mkdir -p $P/games/steam-14141.kept/innen7/t; printf 'g' > $P/games/steam-14141.kept/innen7/t/g.txt   # Codex-Prüfung 194
    mkdir -p $P/games/steam-21211.kept/innen8/t $P/games/steam-21212.kept/a; printf 'g' > $P/games/steam-21211.kept/innen8/t/g.txt; printf 'h' > $P/games/steam-21212.kept/a/h.txt   # PS5 23:35, Codex-Prüfung 217
    # Codex-Prüfung 172 (3): der Ordner eines bekannten Programms ist SELBST privat
    G=$T/w/user/data/gamecompressor; mkdir -p $G/sub; printf 'a' > $G/a.txt; printf 'b' > $G/sub/b.txt; chmod 000 $G
  fi
  chmod 000 $P/runtime-state $P/user/game-settings-private $P/art $P/games/PPSA12345 $P/user/steam.vdf $M/cache $S/cache
}
for art in zugriff ohne kern m2 sim kaputt kaputt_ende kaputt_nachlauf kaputt_selbst; do
  echo "-- $art"
  baum $art
  (cd $T; RMDA=1 FAKESM=1 $B/test_privat $T $art) | grep -v "^LOG:" || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"; RC=1; }
done
chmod -R u+rwx $T/w 2>/dev/null
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
