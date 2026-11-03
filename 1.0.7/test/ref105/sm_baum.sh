#!/bin/bash
# Testbaum sm_v1 = schutz_v1 + verschobene/mehrdeutige ShadowMount-Spiele (wie test/kern/lauf_sm.sh), fuer den Windows-Vergleich
cd "$(dirname "$0")"; T=sm_v1; rm -rf $T; cp -r schutz_v1 $T
rm -f "$T/mnt/ext1/homebrew/.Zwei - PPSA90002.exfat.gc-compress.tmp"
kachel() { mkdir -p "$T/user/app/$1/sce_sys"
  printf '{"titleId":"%s","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"%s"}},"contentVersion":"01.00"}' "$1" "$2" > "$T/user/app/$1/sce_sys/param.json"
  printf '/mnt/shadowmnt/%s\n%s' "$1" "$3" > "$T/user/app/$1/mount_img.lnk"; }
kachel PPSA30528 "Red Dead Redemption" "/mnt/ext1/homebrew/Red Dead Redemption - PPSA30528 - v1.2.0 - US.exfat"
echo x > "$T/user/data/homebrew/Red Dead Redemption - PPSA30528 - v1.2.0 - US.exfat"
kachel PPSA99058 "Overdrive" "/mnt/ext1/homebrew/Overdrive.exfat"; echo x > "$T/user/data/homebrew/Overdrive.exfat"
kachel PPSA77772 "Umbenannt" "/mnt/ext1/homebrew/Alt - PPSA77772.exfat"; echo x > "$T/user/data/homebrew/Neu PPSA77772.exfat"
kachel PPSA77774 "Abgezogen" "/mnt/usb3/homebrew/Abgezogen - PPSA77774.exfat"
kachel PPSA77775 "Geteilt A" "/data/homebrew/Geteilt - PPSA77775.exfat"
kachel PPSA77776 "Geteilt B" "/data/homebrew/Geteilt - PPSA77775.exfat"
echo x > "$T/user/data/homebrew/Geteilt - PPSA77775.exfat"
echo "sm_v1 erzeugt"
