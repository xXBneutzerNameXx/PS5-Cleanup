#!/bin/bash
# NEU 1.1.1 (Sicherheitsfix 17.11.2026): Dateisystem-Aufrufe des PS5-SDK am PC nachgestellt (test_sdk.c, bau/ps5_systemaufrufe.h) - läuft in WSL
#  - wie die SDK-Stubs bis 1.1 (PS5C_SDK_ALT=1: Fehler als positive Zahl, errno unverändert) und wie die Korrektur
#  - gescheiterter Weg zum Ziel (wie FW 13.20/13.60), fremder offener Griff, stderr, gescheitertes Entfernen, normales Löschen
K="/pfad/zu/PS5-Cleanup/1.3"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/sdk
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/sdk.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
RC=0
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN \
    -DPS5C_SDK_NACHBILDUNG -include $KK/bau/ps5_systemaufrufe.h -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
    $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_sdk.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_sdk || { echo BAU FEHLGESCHLAGEN; exit 1; }
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN \
    -DPS5C_SDK_NACHBILDUNG -include $KK/bau/ps5_systemaufrufe.h -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson \
    $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_s4.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_s4_nb || { echo BAU FEHLGESCHLAGEN test_s4_nb; exit 1; }
baum() {   # $1 = Arbeitsordner
  rm -rf $1; mkdir -p $1/user/data/a/tief $1/user/data/b $1/draussen
  echo ziel > $1/user/data/a/x.txt; echo fremd > $1/user/data/b/x.txt; echo c > $1/user/data/c.txt
  echo t > $1/user/data/a/tief/t.txt; echo w > $1/draussen/wichtig.txt
  ln -s ../../../draussen $1/user/data/a/tief/nach_draussen; ln -s ../../draussen $1/user/data/umweg
}
fall() {   # name alt fehler [nur_pfad]
  local f=$1 W=$T/$1; baum $W
  (PS5C_SDK_ALT=$2 PS5C_FEHLER=$3 PS5C_NUR_PFAD=$4 $B/test_sdk $W $f) || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert, Fall $f)"; return 1; }
}
{ fall fix_ebadf  0 "openat:user:13" > $T/_fix_ebadf.txt 2>&1; echo $? > $T/_rc_fix_ebadf; } &
{ fall alt_ebadf  1 "openat:user:78" > $T/_alt_ebadf.txt 2>&1; echo $? > $T/_rc_alt_ebadf; } &
{ fall fix_fremd  0 "openat:a:13" > $T/_fix_fremd.txt 2>&1; echo $? > $T/_rc_fix_fremd; } &
{ fall alt_fremd  1 "openat:a:13" > $T/_alt_fremd.txt 2>&1; echo $? > $T/_rc_alt_fremd; } &
{ fall fix_stderr 0 "openat:a:2" > $T/_fix_stderr.txt 2>&1; echo $? > $T/_rc_fix_stderr; } &
{ fall alt_stderr 1 "openat:a:2" > $T/_alt_stderr.txt 2>&1; echo $? > $T/_rc_alt_stderr; } &
{ fall fix_weg    0 "unlinkat:c.txt:1" > $T/_fix_weg.txt 2>&1; echo $? > $T/_rc_fix_weg; } &
{ fall fix_normal 0 "" > $T/_fix_normal.txt 2>&1; echo $? > $T/_rc_fix_normal; } &
{ fall p13_normal 0 "" 1 > $T/_p13_normal.txt 2>&1; echo $? > $T/_rc_p13_normal; } &
{ fall p13_link   0 "" 1 > $T/_p13_link.txt 2>&1;   echo $? > $T/_rc_p13_link; } &
# NEU 18.11.2026: die Sicherheitsprüfung S4 (Verknüpfungen, Einbindungen, harte Links) unverändert noch einmal - auf dem zweiten Weg (wie FW 13.x)
s4_13() {
  S=$T/s4_13; rm -rf $S; mkdir -p $S/w/user/data/a $S/w/aussen $S/w/aussen2/unter $S/w/user/data/normal_ordner/tief
  echo wichtig > $S/w/aussen/wichtig.txt; chmod 644 $S/w/aussen/wichtig.txt; echo w2 > $S/w/aussen2/wichtig2.txt; echo x > $S/w/aussen2/unter/x.txt
  ln -s ../../../aussen $S/w/user/data/a/ziel; ln -s ../../aussen2 $S/w/user/data/getauscht
  ln -s ../../aussen/wichtig.txt $S/w/user/data/link_auf_datei; ln -s ../../aussen2 $S/w/user/data/link_auf_ordner
  echo n > $S/w/user/data/normal.txt; echo n > $S/w/user/data/normal_ordner/tief/n.txt
  mkdir -p $S/w/user/data/shadowmount; echo WICHTIG > $S/w/aussen/act.dat; ln $S/w/aussen/act.dat $S/w/user/data/shadowmount/STOP
  echo "== S4 auf dem zweiten Weg (PS5C_NUR_PFAD=1, wie FW 13.x)"
  PS5C_NUR_PFAD=1 $B/test_s4_nb $S/w || { echo "FAIL S4 auf dem zweiten Weg"; return 1; }
  mkdir -p $S/w/user/data/m/innen; echo v > $S/w/user/data/m/vorher.txt
  if unshare -rm true 2>/dev/null; then
    PS5C_NUR_PFAD=1 unshare -rm sh -c "mount -t tmpfs none '$S/w/user/data/m/innen' && echo i > '$S/w/user/data/m/innen/im_mount.txt' && '$B/test_s4_nb' '$S/w' einbindung" || { echo "FAIL S4 Einbindung auf dem zweiten Weg"; return 1; }
  else echo "HINWEIS: unshare nicht verfügbar - Einbindungs-Test übersprungen"; fi
}
{ s4_13 > $T/_s4_13.txt 2>&1; echo $? > $T/_rc_s4_13; } &
# NEU 18.11.2026 (Rückmeldung #22): wie s4_13, aber die verändernden Aufrufe lehnt die Konsole mit EPERM statt EINVAL ab (PS5C_NUR_PFAD=2)
s4_13b() {
  S=$T/s4_13b; rm -rf $S; mkdir -p $S/w/user/data/a $S/w/aussen $S/w/aussen2/unter $S/w/user/data/normal_ordner/tief
  echo wichtig > $S/w/aussen/wichtig.txt; chmod 644 $S/w/aussen/wichtig.txt; echo w2 > $S/w/aussen2/wichtig2.txt; echo x > $S/w/aussen2/unter/x.txt
  ln -s ../../../aussen $S/w/user/data/a/ziel; ln -s ../../aussen2 $S/w/user/data/getauscht
  ln -s ../../aussen/wichtig.txt $S/w/user/data/link_auf_datei; ln -s ../../aussen2 $S/w/user/data/link_auf_ordner
  echo n > $S/w/user/data/normal.txt; echo n > $S/w/user/data/normal_ordner/tief/n.txt
  mkdir -p $S/w/user/data/shadowmount; echo WICHTIG > $S/w/aussen/act.dat; ln $S/w/aussen/act.dat $S/w/user/data/shadowmount/STOP
  echo "== S4 auf dem zweiten Weg, Löschen mit anderer Ablehnung (PS5C_NUR_PFAD=2, wie FW 13.60 laut #22)"
  PS5C_NUR_PFAD=2 $B/test_s4_nb $S/w || { echo "FAIL S4 auf dem zweiten Weg (andere Ablehnung)"; return 1; }
}
{ s4_13b > $T/_s4_13b.txt 2>&1; echo $? > $T/_rc_s4_13b; } &
wait
for f in fix_ebadf alt_ebadf fix_fremd alt_fremd fix_stderr alt_stderr fix_weg fix_normal p13_normal p13_link s4_13 s4_13b; do cat $T/_$f.txt; [ "$(cat $T/_rc_$f 2>/dev/null)" = 0 ] || RC=1; done
date "+%Y-%m-%d %H:%M:%S FERTIG"
exit $RC
