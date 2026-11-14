# PS5 Cleanup 1.0.10 - Testbaum "Rezept lesen" (da_Chris86 11.11.2026, universelle Erkennung): die Wege, auf denen Programme an die Konsole
# kommen, je mit einem UNBEKANNTEN Programm (in keiner Liste). Nur nachgebildete Dateien - nie eine PS5.  python3 -I rezept_baum.py <w>
import os, sys, random
W = sys.argv[1]
def datei(p, inhalt=b"x"):
    q = W + p; os.makedirs(os.path.dirname(q), exist_ok=True)
    open(q, "wb").write(inhalt if isinstance(inhalt, bytes) else inhalt.encode())
def kachel(tid, titel, deeplink=None):
    dl = f',"deeplinkUri":"{deeplink}"' if deeplink else ""
    j = f'{{"titleId":"{tid}","applicationCategoryType":65536{dl},"localizedParameters":{{"defaultLanguage":"en-US","en-US":{{"titleName":"{titel}"}}}}}}'
    datei(f"/user/appmeta/{tid}/param.json", j); datei(f"/system_data/priv/appmeta/{tid}/param.json", j)
def prog(kopf, *pfade):
    b = kopf + b"\0" * 300
    for p in pfade: b += p.encode() + b"\0" + b"\x90" * 29 + b"\0"
    return b + b"\0" * 300
ELF = b"\x7fELF\x02\x01\x01" + b"\0" * 57
FSELF = b"\x4f\x15\x3d\x1d" + b"\0" * 60
zufall = bytes(random.Random(7).getrandbits(8) for _ in range(8192))
# 1 PKG-App (Programm verschlüsselt): Ordner bleibt ein Rest - Grenze
kachel("PPSA77001", "Paketapp"); datei("/user/app/PPSA77001/app.pkg", zufall); datei("/user/data/pakdaten/x.json", "{}")
# 2 Ordner-App (mount.lnk), Programm lose: Ordner gehört zur Kachel
kachel("PPSA77002", "Ordnerapp"); datei("/user/app/PPSA77002/mount.lnk", "/data/homebrew/PPSA77002")
datei("/user/data/homebrew/PPSA77002/eboot.bin", prog(FSELF, "/data/ordner-daten", "/data/ordner-daten/config.json", "/data/homebrew"))
datei("/user/data/homebrew/PPSA77002/sce_sys/param.json", "{}"); datei("/user/data/ordner-daten/config.json", "{}")
# 3 ELF-Payload im Payload Manager
datei("/user/data/pldmgr/payloads/repo/meinwz@ab12/meinwerkzeug_v1.0.elf", prog(ELF, "/data/meinwz", "/data/meinwz/log.txt", "/data/meinwz/ini", "/data/fremdordner"))
datei("/user/data/meinwz/log.txt", "l"); datei("/user/data/fremdordner/a", "a")
# 4 ELF auf dem USB-Stick in einem Unterordner
datei("/mnt/usb0/payloads/stickwerkzeug_v1.2.elf", prog(ELF, "/data/stickwz", "/data/stickwz/log.txt", "/data/etaHEN/payloads"))
datei("/user/data/stickwz/log.txt", "l")
# 5 etaHEN-Plugin (Kopf etaHEN_PLUGIN + Klartext-ELF)
datei("/user/data/etaHEN/plugins/plugwerk.plugin", prog(b"etaHEN_PLUGIN" + b"PLUG00001" + b"\0" * 7 + ELF, "/data/plugdat", "/data/plugdat/a.cfg"))
datei("/user/data/plugdat/a.cfg", "c")
# 6 etaHEN-Plugin-App: homebrew.elf im Systemordner
kachel("ETAP00001", "Pluginapp"); datei("/system_ex/app/ETAP00001/eboot.bin", zufall)
datei("/system_ex/app/ETAP00001/homebrew.elf", prog(ELF, "/data/plugindaten", "/data/plugindaten/a.cfg")); datei("/user/data/plugindaten/a.cfg", "c")
# 7 Web-Kachel, Programm nicht auf der Konsole: Rest - Grenze
kachel("WEBZ00009", "Webwerkzeug", "http://127.0.0.1:9999/"); datei("/user/app/WEBZ00009/sce_sys/param.json", "{}"); datei("/user/data/webwz/x.json", "{}")
# Gegenproben: Gleichstand (keiner), Sammelordner (nie), verschlüsselte "ELF" (nichts), Reste
datei("/user/data/payloads/gleich.elf", prog(ELF, "/data/gleich-a", "/data/gleich-a/x", "/data/gleich-b", "/data/gleich-b/y"))
datei("/user/data/gleich-a/x", "x"); datei("/user/data/gleich-b/y", "y")
datei("/user/data/payloads/sammler.elf", prog(ELF, "/data/homebrew", "/data/homebrew/a", "/data/homebrew/b", "/data/games/x"))
datei("/user/data/payloads/geheim.elf", zufall); datei("/user/data/geheimdaten/x", "x")
datei("/user/data/altrest/x.dat", "r")
# Codex-Prüfung 155: zwei Programme wählen denselben Ordner (keins), ein Ordner nur einmal genannt (zu wenig)
datei("/user/data/payloads/streit_a.elf", prog(ELF, "/data/streit", "/data/streit/a.cfg"))
datei("/user/data/payloads/streit_b.elf", prog(ELF, "/data/streit", "/data/streit/b.cfg")); datei("/user/data/streit/a.cfg", "a")
datei("/user/data/payloads/einmal.elf", prog(ELF, "/data/einmal")); datei("/user/data/einmal/x", "x")
