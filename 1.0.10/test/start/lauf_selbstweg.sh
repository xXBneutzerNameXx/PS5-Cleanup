#!/bin/bash
# PC-Test start/selbstweg.c (29.10.2026) - Sony + Einhängen nachgebildet, gelöscht wird nur im Testordner
K="${K:-/pfad/zu/PS5-Cleanup/1.0.10}"   # läuft in WSL
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
T=$HOME/ps5c-test/sw; B=$HOME/ps5c-bau/test; mkdir -p $B $T "$K/test/kern/ergebnis"
exec > "$K/test/kern/ergebnis/selbstweg.log" 2>&1
RC=0; date "+%Y-%m-%d %H:%M:%S Start"
gcc -O2 -Wall -Wno-unused-function -I$KK/test/start/shim -include $KK/test/start/shim/pc_ps5.h -DSW_TEST_MOUNTS -DSO_WURZEL="\"$T/w\"" \
    -DSW_PROT_WURZEL="\"$T/w\"" -DSW_WIEDERHOLUNG_S=2 $KK/start/selbstweg.c $KK/start/sw_protokoll.c $KK/test/start/test_selbstweg.c -lpthread -o $B/test_selbstweg || { echo BAU FEHLGESCHLAGEN; exit 1; }
gcc -O2 -Wall -Wno-unused-function -DSIMULATION -I$KK/test/start/shim -include $KK/test/start/shim/pc_ps5.h -c $KK/start/selbstweg.c -o $B/selbstweg_sim.o || { echo BAU SIM FEHLGESCHLAGEN; exit 1; }
echo "Simulation - verwendete Aufrufe: $(nm -u $B/selbstweg_sim.o | tr -s ' ' | tr '\n' ' ')"
nm -u $B/selbstweg_sim.o | grep -w -E "unlink|rmdir|nmount|sceAppInstUtil.*" && { echo "FAIL Simulation enthält löschende Aufrufe"; RC=1; }
# NEU 1.0.7: Protokoll-Anhang - Simulation schreibt nichts
gcc -O2 -Wall -DSIMULATION -c $KK/start/sw_protokoll.c -o $B/sw_protokoll_sim.o || { echo BAU SIM FEHLGESCHLAGEN; exit 1; }
nm -u $B/sw_protokoll_sim.o | grep -w -E "open|write|unlink|mkdir" && { echo "FAIL Simulation: sw_protokoll.o schreibt"; RC=1; } || echo "OK   Simulation: sw_protokoll.o ohne schreibenden Aufruf"
for fall in ${FAELLE:-ok ohne_kachel ro_zurueck ealready zweiter_wechsel ablehnen fremd_start fremd_kachel sitzung_offen haengt start_frei start_gesperrt protokoll protokoll_intern zweimal nie_weg}; do
  rm -rf $T/w; mkdir -p $T/w/user/temp $T/w/system_ex/app/PSCL00002/sce_sys $T/w/system_ex/app/FAKE00000/sce_sys $T/w/system_ex/app/NPXS40106 $T/w/user/app/PSCL00001/sce_sys
  printf 'PS5CLEANUP-STARTORDNER\n' > $T/w/system_ex/app/PSCL00002/.ps5cleanup_start; echo x > $T/w/system_ex/app/PSCL00002/eboot.bin; echo '{}' > $T/w/system_ex/app/PSCL00002/sce_sys/param.json
  echo x > $T/w/system_ex/app/FAKE00000/eboot.bin; echo x > $T/w/system_ex/app/NPXS40106/eboot.bin
  printf 'PS5CLEANUP-KACHEL\n' > $T/w/user/app/PSCL00001/.ps5cleanup_kachel; echo 1 > $T/w/user/app/PSCL00001/.ps5cleanup_eingetragen; echo '{}' > $T/w/user/app/PSCL00001/sce_sys/param.json
  $B/test_selbstweg $fall || { echo "FAIL Testprogramm meldet Fehler (Rückgabewert)"; RC=1; }
done
date "+%Y-%m-%d %H:%M:%S FERTIG"; exit $RC
