#!/bin/bash
# Testbaum download_v1 (27.10.2026): alle Fälle für /user/download (Korrektur 1.0.6/1.1). Basis root9.
cd "$(dirname "$0")"
rm -rf download_v1; cp -r root9 download_v1; D=download_v1
mk() { mkdir -p "$D/user/download/$1"; printf 'UFS2-Testinhalt %s\n' "$1" > "$D/user/download/$1/download0.dat"; }
# 1 installiert intern (root9: PPSA55555 hat app + appmeta + sysmeta) -> nicht anbieten
# 2 nur auf der M.2
mk PPSA70001; mkdir -p $D/mnt/ext1/user/app/PPSA70001/sce_sys
# 3 nur auf der USB-Erweiterung
mk PPSA70002; mkdir -p $D/mnt/ext0/user/app/PPSA70002/sce_sys
# 4 nur Kachel-Infos /user/appmeta
mk PPSA70003; mkdir -p $D/user/appmeta/PPSA70003; echo '{"titleId":"PPSA70003"}' > $D/user/appmeta/PPSA70003/param.json
# 5 nur System-Kachel-Infos
mk PPSA70004; mkdir -p $D/system_data/priv/appmeta/PPSA70004; echo '{"titleId":"PPSA70004"}' > $D/system_data/priv/appmeta/PPSA70004/param.json
# 6 Homebrew in /system_ex/app
mk HBLD70005; mkdir -p $D/system_ex/app/HBLD70005/sce_sys
# 7 YouTube ohne installierte App (Y2JB-Daten) -> gesperrt sichtbar
mk PPSA01650
# 8 echte Waise -> "App-Daten ohne installierte App"
mk PPSA70006
# 10 nur /user/app (ohne Kachel-Infos), PS4-Titel
mk CUSA70008; mkdir -p $D/user/app/CUSA70008/sce_sys
# 11 nur externe System-Kachel-Infos
mk PPSA70007; mkdir -p $D/system_data/priv/appmeta/external/PPSA70007
# 12 installierte YouTube-App (Y2JB) mit Daten -> App gesperrt, Daten nicht angeboten
mk PPSA01651; mkdir -p $D/user/app/PPSA01651/sce_sys
printf '{"titleId":"PPSA01651","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"YouTube"}}}' > $D/user/app/PPSA01651/sce_sys/param.json
# 13 YouTube- und Netflix-Daten ohne App -> gesperrt sichtbar
mk PPSA01652; mk PPSA01615
# 14 Datenspeicher des installierten Spiels PPSA55555 muss erhalten bleiben
printf 'UFS2-Testinhalt PPSA55555\n' > $D/user/download/PPSA55555/download0.dat 2>/dev/null || { mkdir -p $D/user/download/PPSA55555; printf 'x\n' > $D/user/download/PPSA55555/download0.dat; }
# 15 Zusatzinhalt-Infos eines Jailbreak-Einstiegs -> gesperrt; Sammeleintrag externer Infos mit Einstieg + fremdem Titel
mkdir -p $D/user/appmeta/addcont/PPSA01650; echo x > $D/user/appmeta/addcont/PPSA01650/param.json
mkdir -p $D/system_data/priv/appmeta/external/PPSA01614 $D/system_data/priv/appmeta/external/PPSA70009
# 16-21 NEU 29.10.2026 (da_Chris86: nur die Jailbreak-Einstiege schützen)
# 16 Lua-Jailbreak: Spiel CUSA17122 (Aerial Life) installiert + Spielstand -> Jailbreak-Einstieg, eingerichtet
mkdir -p $D/user/app/CUSA17122/sce_sys $D/user/appmeta/CUSA17122 $D/user/home/u1/savedata/CUSA17122
printf '{"titleId":"CUSA17122","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"Aerial Life"}}}' > $D/user/app/CUSA17122/sce_sys/param.json
echo x > $D/user/app/CUSA17122/eboot.bin; echo '{}' > $D/user/appmeta/CUSA17122/param.json; echo s > $D/user/home/u1/savedata/CUSA17122/sdimg_SAVEDATA00
# 17 PS2-Spielstand-Jailbreak: nur der Spielstand von Okage (CUSA02199)
mkdir -p $D/user/home/u1/savedata/CUSA02199; echo s > $D/user/home/u1/savedata/CUSA02199/sdimg_SAVEDATA00
# 18 Netflix JP (PPSA01616) installiert, ohne App-Daten -> geschützt, aber nicht eingerichtet
mkdir -p $D/user/app/PPSA01616/sce_sys
printf '{"titleId":"PPSA01616","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"Netflix"}}}' > $D/user/app/PPSA01616/sce_sys/param.json
# 19 Browserspeicher (root9: /user/home/u1/webkit/shell/local/a) -> Jailbreak-Einstieg (WebKit)
# 20 Jailbreak-Tools bleiben wie bisher (kein Einstieg)
mkdir -p $D/user/data/ps5_autoloader; echo elfldr.elf > $D/user/data/ps5_autoloader/autoload.txt
# 21 normaler PS4-Spielstand -> kein Einstieg
mkdir -p $D/user/home/u1/savedata/CUSA70010; echo s > $D/user/home/u1/savedata/CUSA70010/sdimg_SAVEDATA00
# 9 Disc-Player -> nie
mkdir -p $D/user/download/NPXS40140; printf 'disc\n' > $D/user/download/NPXS40140/download0.dat
echo "download_v1: $(ls $D/user/download | tr '\n' ' ')"
