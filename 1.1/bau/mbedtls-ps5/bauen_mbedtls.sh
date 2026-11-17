#!/bin/bash
# NEU 1.1 (Weg D): baut mbedTLS 3.6 (LTS) für die PS5 als bau/mbedtls-ps5/lib/libmbedtls_ps5.a (in WSL, mit dem PS5-SDK).
# Aufruf: bash bauen_mbedtls.sh <Arbeitsordner>   - lädt dorthin den Quelltext und die Zertifikatsliste, prüft die SHA-256-Werte.
# Einstellungen: Standard von mbedTLS + die Zeilen am Ende von include/mbedtls/mbedtls_config.h (siehe unten, "PS5 Cleanup").
# Mit PC=1 zusätzlich eine Linux-Fassung (für den Test auf dem PC) nach <Arbeitsordner>/pc/libmbedtls_pc.a.
set -e
V=3.6.7
HIER="$(cd "$(dirname "$0")" && pwd)"
W="$1"; [ -n "$W" ] || { echo "Arbeitsordner fehlt"; exit 1; }
mkdir -p "$W"; cd "$W"
[ -f mbedtls-$V.tar.bz2 ] || curl -sfL -o mbedtls-$V.tar.bz2 https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$V/mbedtls-$V.tar.bz2
SOLL=$(curl -sf https://api.github.com/repos/Mbed-TLS/mbedtls/releases/tags/mbedtls-$V | grep '"body"' | grep -o -E '[0-9a-f]{64}' | head -1)   # Abschnitt "Checksum" der Veröffentlichung
IST=$(sha256sum mbedtls-$V.tar.bz2 | cut -c1-64)
echo "mbedTLS $V  SHA-256 $IST  (laut Veröffentlichung: ${SOLL:-nicht gefunden})"
[ -n "$SOLL" ] && [ "$SOLL" = "$IST" ] || { echo "BAU FEHLGESCHLAGEN: Prüfsumme mbedTLS"; exit 1; }
curl -sfL -o cacert.pem https://curl.se/ca/cacert.pem
SOLL2=$(curl -sfL https://curl.se/ca/cacert.pem.sha256 | cut -c1-64); IST2=$(sha256sum cacert.pem | cut -c1-64)
echo "Zertifikatsliste (Mozilla über curl.se) SHA-256 $IST2 (laut curl.se: $SOLL2)"; head -5 cacert.pem | grep -i "as of" || true
[ "$SOLL2" = "$IST2" ] || { echo "BAU FEHLGESCHLAGEN: Prüfsumme Zertifikatsliste"; exit 1; }
rm -rf q; mkdir q; tar xjf mbedtls-$V.tar.bz2 -C q --strip-components=1
cat >> q/include/mbedtls/mbedtls_config.h <<'CFG'

/* ===== PS5 Cleanup 1.1 (Weg D): Anpassungen für die PS5 ===== */
#define MBEDTLS_NO_PLATFORM_ENTROPY          /* Zufall kommt vom System (arc4random_buf), siehe netz_tls.c */
#define MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG
#undef MBEDTLS_NET_C                         /* eigene Socket-Funktionen in netz_tls.c */
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_FS_IO                         /* keine Dateien: Zertifikate sind im Programm */
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_PSA_ITS_FILE_C
#define MBEDTLS_THREADING_C                  /* Update-Hinweis, Rückmeldung und Update holen laufen in eigenen Fäden */
#define MBEDTLS_THREADING_PTHREAD
CFG
bau() {   # $1 Compiler, $2 Ziel-Archiv, $3 Ar
  rm -rf o; mkdir o
  ls q/library/*.c | xargs -P "$(nproc)" -I{} sh -c "$1 -c -O2 -ffunction-sections -fdata-sections -Iq/include -Iq/library '{}' -o o/\$(basename '{}' .c).o"
  rm -f "$2"; $3 rcs "$2" o/*.o
  ls -la "$2"
}
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
mkdir -p "$HIER/lib" "$HIER/include"
bau "$PS5_PAYLOAD_SDK/bin/prospero-clang" "$HIER/lib/libmbedtls_ps5.a" "$PS5_PAYLOAD_SDK/bin/prospero-ar"
rm -rf "$HIER/include/mbedtls" "$HIER/include/psa"; cp -r q/include/mbedtls q/include/psa "$HIER/include/"
cp cacert.pem "$HIER/ca-bundle.crt"; cp q/LICENSE "$HIER/LICENSE_mbedTLS.txt"
{ echo "mbedTLS $V  https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$V/mbedtls-$V.tar.bz2  SHA-256 $IST"
  echo "ca-bundle.crt = https://curl.se/ca/cacert.pem (Mozilla)  SHA-256 $IST2  geladen $(date +%Y-%m-%d)"
  echo "gebaut mit bauen_mbedtls.sh, PS5-SDK $(cat $PS5_PAYLOAD_SDK/VERSION 2>/dev/null)"; } > "$HIER/HERKUNFT.txt"
if [ "$PC" = 1 ]; then mkdir -p pc; bau gcc "$W/pc/libmbedtls_pc.a" ar; fi
echo FERTIG
