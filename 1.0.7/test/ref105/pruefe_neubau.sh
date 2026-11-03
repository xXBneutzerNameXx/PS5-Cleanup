#!/bin/bash
# Nach dem Neubau von Ref105.exe (REF_IP-Zusatz): "detail" auf root9 muss byte-gleich mit der eingefrorenen Referenz sein,
# und REF_IP mit einem löschenden Szenario muss abgewiesen werden.
export MSYS_NO_PATHCONV=1
cd "$(dirname "$0")"
PY="/f/projekt/ComfyUI/python_embeded/python.exe"
exec > ergebnis/_pruefe_neubau.log 2>&1
rm -rf w; cp -r root9 w
env RMDA=1 FAKESM=1 "$PY" -I miniftp4.py w 2199 ftp.log > /dev/null 2>&1 &
pid=$!; sleep 1.5
SPRACHE=de timeout 300 ./Ref105.exe detail 2199 "$(cygpath -w "$(pwd)")" > ergebnis/_neubau_detail_root9.out 2>&1
REF_IP=127.0.0.1 timeout 60 ./Ref105.exe komplett 2199 "$(cygpath -w "$(pwd)")" > ergebnis/_neubau_sperre.out 2>&1
kill $pid; wait $pid 2>/dev/null
cmp ergebnis/de_detail_root9.out ergebnis/_neubau_detail_root9.out && echo "detail root9: byte-gleich mit der Referenz"
cat ergebnis/_neubau_sperre.out
(cd w && find . -printf '%y %s %P\n' | LC_ALL=C sort | md5sum)
(cd root9 && find . -printf '%y %s %P\n' | LC_ALL=C sort | md5sum)
rm -rf w
echo FERTIG
