#!/bin/bash
# Baut PCRE2 (BSD-Lizenz) für PC (Tests) und PS5, legt cJSON (MIT) ab. Läuft in WSL.
set -e
F="<PS5-App>/fremd"
BAU="<PS5-App>/bau"
T=$HOME/ps5c-bau/fremd
rm -rf "$T"; mkdir -p "$T"
tar xzf "$F/pcre2-10.44.tar.gz" -C "$T"
tar xzf "$F/cJSON-1.7.18.tar.gz" -C "$T"
OPT="-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DPCRE2_BUILD_PCRE2_8=ON -DPCRE2_BUILD_PCRE2_16=OFF -DPCRE2_BUILD_PCRE2_32=OFF -DPCRE2_SUPPORT_UNICODE=ON -DPCRE2_SUPPORT_JIT=OFF -DPCRE2_BUILD_TESTS=OFF -DPCRE2_BUILD_PCRE2GREP=OFF -DPCRE2_SUPPORT_LIBZ=OFF -DPCRE2_SUPPORT_LIBBZ2=OFF -DPCRE2_SUPPORT_LIBEDIT=OFF -DPCRE2_SUPPORT_LIBREADLINE=OFF"
echo "== PC"
cmake $OPT -DCMAKE_INSTALL_PREFIX="$BAU/pcre2-pc" -B "$T/pc" -S "$T/pcre2-10.44" > "$T/pc.cmake.log"
make -C "$T/pc" -j8 > "$T/pc.make.log"
make -C "$T/pc" install > /dev/null
echo "== PS5"
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
source $PS5_PAYLOAD_SDK/toolchain/prospero.sh
$CMAKE $OPT -DCMAKE_INSTALL_PREFIX="$BAU/pcre2-ps5" -B "$T/ps5" -S "$T/pcre2-10.44" > "$T/ps5.cmake.log"
$MAKE -C "$T/ps5" -j8 > "$T/ps5.make.log"
# install würde den SDK-Sysroot voranstellen -> von Hand kopieren
mkdir -p "$BAU/pcre2-ps5/lib" "$BAU/pcre2-ps5/include"
cp "$T/ps5/libpcre2-8.a" "$BAU/pcre2-ps5/lib/"
cp "$T/ps5/interface/pcre2.h" "$BAU/pcre2-ps5/include/" 2>/dev/null || cp "$T/ps5/pcre2.h" "$BAU/pcre2-ps5/include/"
ls -la "$BAU/pcre2-pc/lib" "$BAU/pcre2-ps5/lib" 2>&1
mkdir -p "$BAU/cjson"; cp "$T/cJSON-1.7.18/cJSON.c" "$T/cJSON-1.7.18/cJSON.h" "$T/cJSON-1.7.18/LICENSE" "$BAU/cjson/"
cp "$T/pcre2-10.44/LICENCE" "$BAU/LICENCE_PCRE2.txt"
ls -la "$BAU/cjson"
