#!/bin/bash
# Sicherheitsprüfung 25.10.2026: Tests S2, S3, S4, S5, U17 (läuft in WSL)
K="/pfad/zu/PS5-Cleanup/1.0.7"
ln -sfn "$K" $HOME/ps5c-k; KK=$HOME/ps5c-k
B=$HOME/ps5c-bau/test; T=$HOME/ps5c-test/sich; RF=$KK/test/ref105
mkdir -p "$K/test/kern/ergebnis" $B $T
exec > "$K/test/kern/ergebnis/sicherheit.log" 2>&1
date "+%Y-%m-%d %H:%M:%S Start"
# Codex-Prüfung 75: endet ein Testprogramm nicht sauber (Absturz, Abbruch, Fehler), steht das als FAIL im Log - auch wenn es vorher nichts ausgab
rc() { local r=$1; [ "$r" = "0" ] || echo "FAIL Testprogramm $2 endete mit Rückgabewert $r (Fehler, Absturz oder Abbruch)"; }
KERN="$(ls $KK/kern/*.c | tr "\n" " ")"
F="-O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers -Wno-clobbered -DKERN_LOESCHEN -I$KK/bau/pcre2-pc/include -I$KK/bau/cjson"
gcc $F $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_sicherheit.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_sicherheit || { echo BAU FEHLGESCHLAGEN; exit 1; }
gcc $F $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_s4.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_s4 || { echo BAU FEHLGESCHLAGEN; exit 1; }
gcc $F $KERN $KK/bau/cjson/cJSON.c $KK/test/kern/test_anhalten.c -L$KK/bau/pcre2-pc/lib -lpcre2-8 -lpthread -o $B/test_anhalten || { echo BAU FEHLGESCHLAGEN; exit 1; }
elf_mit_kennung() { { printf '\177ELF'; head -c 2000 /dev/zero; printf 'PS5CLEANUP-ELF-KENNUNG:7c1e5b2a-selbstschutz'; head -c 2000 /dev/zero; } > "$1"; }
# S2
rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
mkdir -p $T/w/user/data/pldmgr/local/ps5c $T/w/mnt/ext1/meinordner $T/w/user/data/payloads
elf_mit_kennung $T/w/user/data/pldmgr/local/ps5c/ps5c.elf; elf_mit_kennung $T/w/mnt/ext1/meinordner/ohne_endung
{ printf '\177ELF'; head -c 3000 /dev/zero; } > $T/w/user/data/payloads/fremd.elf
printf 'Notiz: PS5CLEANUP-ELF-KENNUNG:7c1e5b2a-selbstschutz steht in jeder Kopie\n%.0s' 1 2 3 4 5 > $T/w/user/data/notiz.txt
(cd $T; RMDA=1 FAKESM=1 $B/test_sicherheit s2 $T schutz); rc $? s2
# NEU 1.0.6: PS5 Cleanup entfernt sich am Ende selbst (intern immer, M.2/USB nur wenn mit geleert, USB-Stick nie)
rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
mkdir -p $T/w/user/data/pldmgr/local/ps5c $T/w/mnt/ext1/meinordner $T/w/user/app/PSCL00001/sce_sys
elf_mit_kennung $T/w/user/data/pldmgr/local/ps5c/ps5c.elf; elf_mit_kennung $T/w/mnt/ext1/meinordner/ohne_endung
echo PS5CLEANUP-KACHEL > $T/w/user/app/PSCL00001/.ps5cleanup_kachel; echo '{}' > $T/w/user/app/PSCL00001/sce_sys/param.json
(cd $T; RMDA=1 FAKESM=1 $B/test_sicherheit selbst $T schutz); rc $? selbst
# S3 a) M.2-App-Liste nicht lesbar
for art in schutz ohne; do
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w; touch $T/faillist.on
  (cd $T; RMDA=1 FAKESM=1 FAILLIST=/mnt/ext1/user/app $B/test_sicherheit s3 $T $art); rc $? "s3 a $art"; rm -f $T/faillist.on
done
# S3 b) Aufnahme-Zwischenordner nicht lesbar
for art in schutz ohne; do
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w; mkdir -p $T/w/user/av_contents/content_tmp/GESPERRT00/x; chmod 000 $T/w/user/av_contents/content_tmp/GESPERRT00
  (cd $T; RMDA=1 FAKESM=1 $B/test_sicherheit s3 $T $art); rc $? "s3 b $art"; chmod 755 $T/w/user/av_contents/content_tmp/GESPERRT00
done
# S5
(cd $T; $B/test_sicherheit s5 $T); rc $? s5
# U17
for art in schutz ohne; do
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
  mkdir -p $T/w/mnt/ext0/user/app/PPSA88888/sce_sys $T/w/user/appmeta/PPSA88888 $T/w/system_data/priv/appmeta/PPSA88888 $T/w/user/system/share/PPSA88888
  printf '{"titleId":"PPSA88888","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"USB Spiel"}}}' > $T/w/mnt/ext0/user/app/PPSA88888/sce_sys/param.json
  cp $T/w/mnt/ext0/user/app/PPSA88888/sce_sys/param.json $T/w/user/appmeta/PPSA88888/param.json
  cp $T/w/mnt/ext0/user/app/PPSA88888/sce_sys/param.json $T/w/system_data/priv/appmeta/PPSA88888/param.json
  echo x > $T/w/user/license/fakeEP0000-PPSA88888.rif
  (cd $T; RMDA=1 FAKESM=1 $B/test_sicherheit u17 $T $art); rc $? "u17 $art"
done
# NEU 27.10.2026: Geschütztes unter einem erlaubten Ordner
rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
mkdir -p $T/w/mnt/ext1/test/sicherung/NPXS40140 $T/w/mnt/ext1/test/frei; chmod 750 $T/w/mnt/ext1/test/sicherung
echo '{}' > $T/w/mnt/ext1/test/sicherung/NPXS40140/param.json; chmod 644 $T/w/mnt/ext1/test/sicherung/NPXS40140/param.json
echo e > $T/w/mnt/ext1/test/sicherung/eigenes.txt; echo f > $T/w/mnt/ext1/test/frei/f.txt
(cd $T; RMDA=1 FAKESM=1 $B/test_sicherheit baum $T schutz) | grep -v "^LOG:"; rc ${PIPESTATUS[0]} baum
# NEU 1.0.6 (Codex-Prüfung 27): Homebrew-Ordner auf der M.2, USB-Stick mit Lesefehler, PS5-Cleanup-Kopie auf dem Stick
rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
mkdir -p $T/w/mnt/ext1/payloads $T/w/mnt/usb0 $T/w/mnt/usb1/kaputt
echo l > $T/w/mnt/ext1/payloads/liste.txt; echo n > $T/w/mnt/usb0/notiz.txt; elf_mit_kennung $T/w/mnt/usb0/ps5c.elf
mkdir -p $T/w/mnt/usb0/user; elf_mit_kennung $T/w/mnt/usb0/user/k.elf
mkdir -p $T/w/user/appmeta/push_resource $T/w/system_data/priv/appmeta/push_resource $T/w/user/download/push_resource
echo p > $T/w/user/appmeta/push_resource/r.dat; echo p > $T/w/system_data/priv/appmeta/push_resource/r.dat; echo p > $T/w/user/download/push_resource/r.dat
mkdir -p $T/w/user/app/backup $T/w/user/patch/backup $T/w/user/license/fakeBackup.rif; echo b > $T/w/user/app/backup/b.dat
mkdir -p $T/w/mnt/ext1/isos; echo i > "$T/w/mnt/ext1/isos/DuckTales [BLES01969].iso"; echo l > $T/w/mnt/ext1/isos/linux.iso
echo k > $T/w/mnt/usb1/kaputt/k.txt; echo d > $T/w/mnt/usb1/datei.txt; chmod 000 $T/w/mnt/usb1/kaputt
mkdir -p $T/w/mnt/usb1/tief/a/b/c/d/e/f/g/h; echo t > $T/w/mnt/usb1/tief/a/b/c/d/e/f/g/h/t.txt   # NEU 1.0.8 (Arbeitsplan 2.5): zu tief verschachtelt
(cd $T; RMDA=1 FAKESM=1 $B/test_sicherheit c27 $T schutz) | grep -v "^LOG:"; rc ${PIPESTATUS[0]} c27
chmod 755 $T/w/mnt/usb1/kaputt
# NEU 1.0.8 (Codex-Prüfung 74): Ordner der Test-Fassung (intern und USB-Laufwerk), darin eine Kopie von PS5 Cleanup
rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
mkdir -p $T/w/user/data/PS5-Cleanup-Test $T/w/mnt/usb0/PS5-Cleanup-Test $T/w/user/data/pldmgr/local/ps5c $T/w/user/data/PS5-Cleanup-Testx
echo m > $T/w/user/data/PS5-Cleanup-Test/mitschnitt.txt; echo b > $T/w/mnt/usb0/PS5-Cleanup-Test/bild_0001.png; echo x > $T/w/user/data/PS5-Cleanup-Testx/x.txt
mkdir -p $T/w/mnt/usb1/ps5-cleanup-test; echo b > $T/w/mnt/usb1/ps5-cleanup-test/bild_0002.png   # Codex-Prüfung 75: andere Schreibweise
elf_mit_kennung $T/w/user/data/PS5-Cleanup-Test/ps5c.elf; elf_mit_kennung $T/w/user/data/pldmgr/local/ps5c/ps5c.elf
(cd $T; RMDA=1 FAKESM=1 $B/test_sicherheit testordner $T schutz) | grep -v "^LOG:"; rc ${PIPESTATUS[0]} testordner
# NEU 1.0.8 (Arbeitsplan 3.5): Tool-Ordner, aus dem sich eine Datei nicht löschen lässt (FAILDEL, Löschen Datei für Datei ohne RMDA), und einer,
# den das "Tool" gleich wieder anlegt
rm -rf $T/w; cp -r $RF/schutz_v1 $T/w
mkdir -p $T/w/user/data/zftpd/fest $T/w/user/data/nanodns; head -c 1000 /dev/zero > $T/w/user/data/zftpd/a.bin; head -c 500 /dev/zero > $T/w/user/data/zftpd/fest/x.bin
echo n > $T/w/user/data/nanodns/n.cfg
(cd $T; FAKESM=1 FAILDEL=/user/data/zftpd/fest $B/test_sicherheit teilweise $T schutz) | grep -v "^LOG:"; rc ${PIPESTATUS[0]} teilweise
# NEU 1.0.8 (Arbeitsplan 3.4): vollständige Bereinigung, während Eintrag 2 "anhalten" - dazu die Gegenprobe ohne Anhalten und
# (Codex-Prüfung 75) "anhalten" erst während des letzten Eintrags
for nach in 2 -1 0; do
  rm -rf $T/w; cp -r $RF/schutz_v1 $T/w; mkdir -p $T/w/user/data/pldmgr/local/ps5c; elf_mit_kennung $T/w/user/data/pldmgr/local/ps5c/ps5c.elf
  # Codex-Prüfung 79: leere Zusatzinhalt-Ordner bei PS5-Spielen ohne Zusatzinhalte. Nach dem Löschen eines Spiels räumt der Nachlauf seinen
  # leeren Ordner mit weg - nach "anhalten" aber nur bei Spielen, die an der Reihe waren; bei den übrigen bleibt auch er.
  LZ=""; for a in $T/w/user/app/PPSA1*; do id=${a##*/}; [ -e $T/w/user/addcont/$id ] || { mkdir -p $T/w/user/addcont/$id; LZ="$LZ $id"; }; done
  echo "== anhalten nach Eintrag $nach (0 = Gegenprobe)"
  (cd $T; RMDA=1 FAKESM=1 $B/test_anhalten $T $nach) | grep -v "^LOG:"; rc ${PIPESTATUS[0]} "anhalten $nach"
  bleibt=0; weg=0; falsch=""
  for id in $LZ; do
    if [ -d $T/w/user/app/$id ]; then bleibt=$((bleibt+1)); [ -d $T/w/user/addcont/$id ] || falsch="$falsch $id(Spiel da, Ordner weg)"
    else weg=$((weg+1)); [ -d $T/w/user/addcont/$id ] && falsch="$falsch $id(Spiel weg, Ordner da)"; fi
  done
  if [ -n "$falsch" ]; then echo "FAIL leere Zusatzinhalt-Ordner (anhalten $nach):$falsch"
  elif [ "$nach" = 2 ] && [ $bleibt -lt 1 ]; then echo "FAIL leere Zusatzinhalt-Ordner (anhalten 2): kein übriges Spiel zum Prüfen"
  elif [ "$nach" = 0 ] && [ $weg -lt 1 ]; then echo "FAIL leere Zusatzinhalt-Ordner (Gegenprobe): kein gelöschtes Spiel zum Prüfen"
  else echo "OK   leere Zusatzinhalt-Ordner (anhalten $nach): bei $weg gelöschten Spielen mit weg, bei $bleibt übrigen Spielen noch da"; fi
done
# S4
S=$T/s4; rm -rf $S; mkdir -p $S/w/user/data/a $S/w/aussen $S/w/aussen2/unter $S/w/user/data/normal_ordner/tief
echo wichtig > $S/w/aussen/wichtig.txt; chmod 644 $S/w/aussen/wichtig.txt; echo w2 > $S/w/aussen2/wichtig2.txt; echo x > $S/w/aussen2/unter/x.txt
ln -s ../../../aussen $S/w/user/data/a/ziel; ln -s ../../aussen2 $S/w/user/data/getauscht
ln -s ../../aussen/wichtig.txt $S/w/user/data/link_auf_datei; ln -s ../../aussen2 $S/w/user/data/link_auf_ordner
echo n > $S/w/user/data/normal.txt; echo n > $S/w/user/data/normal_ordner/tief/n.txt
mkdir -p $S/w/user/data/shadowmount; echo WICHTIG > $S/w/aussen/act.dat; ln $S/w/aussen/act.dat $S/w/user/data/shadowmount/STOP
$B/test_s4 $S/w; rc $? s4
# S4 Einbindung (tmpfs in eigenem Namensraum, ohne Rechte am PC)
mkdir -p $S/w/user/data/m/innen; echo v > $S/w/user/data/m/vorher.txt
if unshare -rm true 2>/dev/null; then
  unshare -rm sh -c "mount -t tmpfs none '$S/w/user/data/m/innen' && echo i > '$S/w/user/data/m/innen/im_mount.txt' && '$B/test_s4' '$S/w' einbindung"
else echo "HINWEIS: unshare nicht verfügbar - Einbindungs-Test übersprungen"; fi
date "+%Y-%m-%d %H:%M:%S FERTIG"
