# PS5 Cleanup - Testbaum "PKG- und ELF-Dateien bei ihrem Spiel bzw. Programm" (PS5-Test 06.11.2026, Befunde 12 und 14,
# da_Chris86 17:47: "Jede ELF-Datei ... das muss ja überall sein"). Nur nachgebildete Dateien - nie eine PS5.
#   python3 zuordnung_baum.py <w>          Fälle für test_zuordnung (Lauf 0)
#   python3 zuordnung_baum.py <w> viele    2000 PKG-Dateien auf USB-Laufwerk, M.2 und intern (Lauf 2)
#   python3 zuordnung_baum.py <w> store_kaputt   NEU 1.0.9 (#145): ProsperoStore mit unlesbarem journal.json (Lauf 4)
#   python3 zuordnung_baum.py <w> store_ohne_journal   NEU 1.0.9 (Codex-Prüfung 137): ProsperoStore ohne journal.json (Lauf 5)
#   python3 zuordnung_baum.py <w> store_json_kaputt   NEU 1.0.9 (Codex-Prüfung 138): ProsperoStore mit journal.json ohne gültiges JSON (Lauf 6)
import os, struct, sys

W = sys.argv[1]
art = sys.argv[2] if len(sys.argv) > 2 else "faelle"


def sfo(eintr):
    keys = b""; offs = []
    for k, _ in eintr: offs.append(len(keys)); keys += k.encode() + b"\0"
    while len(keys) % 4: keys += b"\0"
    daten = b""; doffs = []; laengen = []
    for _, v in eintr:
        d = v.encode() + b"\0"; doffs.append(len(daten)); laengen.append(len(d)); d += b"\0" * ((-len(d)) % 4); daten += d
    kt = 20 + 16 * len(eintr); dt = kt + len(keys)
    kopf = b"\0PSF" + struct.pack("<IIII", 0x0101, kt, dt, len(eintr))
    tab = b"".join(struct.pack("<HHIII", offs[i], 0x0204, laengen[i], laengen[i] + ((-laengen[i]) % 4), doffs[i]) for i in range(len(eintr)))
    return kopf + tab + keys + daten


def pkg(pfad, titel, tid, kat, ver="01.00", rest=64):
    """PS4-PKG-Kopf wie lauf_ps4.sh pkgdatei (normal), param.sfo bei 0x3000; rest = Bytes danach (Größe für "doppelt")"""
    s = sfo([("APP_VER", ver), ("CATEGORY", kat), ("TITLE", titel), ("TITLE_ID", tid)])
    so = 0x3000; tab = 0x2A80
    liste = [(0x0001, 0x2B00, 64), (0x0400, 0x2B40, 0x100), (0x1000, so, len(s)), (0x1200, 0x3800, 0x400)]
    h = bytearray(max(0x4000, so + len(s) + 16))
    h[0:4] = b"\x7FCNT"; struct.pack_into(">I", h, 0x10, len(liste)); struct.pack_into(">I", h, 0x18, tab)
    cid = ("UP0001-" + tid + "_00-PS5CLEANUPTEST01").encode(); h[0x40:0x40 + len(cid)] = cid
    for i, (eid, off, gr) in enumerate(liste): struct.pack_into(">IIIIII", h, tab + 32 * i, eid, 0, 0, 0, off, gr)
    h[so:so + len(s)] = s
    datei(pfad, bytes(h) + b"x" * rest)


def datei(pfad, inhalt=b"x"):
    p = W + pfad
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "wb") as f: f.write(inhalt if isinstance(inhalt, bytes) else inhalt.encode())


def ps4_spiel(tid, titel, kat="gd"):
    """installiert intern: App-Ordner mit app.pkg, Kachel-Infos mit param.sfo"""
    datei(f"/user/app/{tid}/app.pkg", b"p" * 100); datei(f"/user/app/{tid}/eboot.bin")
    for o in (f"/user/appmeta/{tid}/param.sfo", f"/system_data/priv/appmeta/{tid}/param.sfo"):
        datei(o, sfo([("CATEGORY", kat), ("TITLE", titel), ("TITLE_ID", tid)]))


def kachel(tid, titel, deeplink=None, kategorie=65536):
    dl = f',"deeplinkUri":"{deeplink}"' if deeplink else ""
    j = f'{{"titleId":"{tid}","applicationCategoryType":{kategorie}{dl},"localizedParameters":{{"defaultLanguage":"en-US","en-US":{{"titleName":"{titel}"}}}}}}'
    datei(f"/user/appmeta/{tid}/param.json", j); datei(f"/system_data/priv/appmeta/{tid}/param.json", j); datei(f"/user/app/{tid}/eboot.bin")


ELF = b"\x7fELF\x02\x01\x01" + b"\0" * 57



# NEU 1.0.9 (#146): Daten-Ordner bekannter Apps (aus ihrem Quellcode) - fest bei ihrer Kachel, ohne Kachel unter Apps mit "gehört zu"
def app_daten_baum():
    kachel("PPSA11018", "PC5")
    datei("/user/data/prospero-win/prefixes/spiel/drive_c/Games/spiel.exe", b"MZ" + b"\0" * 100)   # heißt nicht wie die Kachel
    datei("/user/data/savagewind/game/romfs/Actor/a.bin", b"r" * 200)                            # App nicht installiert, eigene Spieldateien
    datei("/user/data/vlc/vlcrc", b"v")                                                         # App nicht installiert, nur Einstellungen
    datei("/user/data/vlcx/notiz.txt", b"n")                                                    # Gegenprobe: unbekannt - Sonstiges
    datei("/user/data/homebrew/prospero-win/notiz.txt", b"n")   # Codex-Prüfung 137: gleichnamiger Ordner in homebrew - nicht belegt, nicht fest zu PC5
    kachel("ILNY26228", "Patch Manager", "http://127.0.0.1:23900")   # PS5-Test 09.11.2026: ps-patch-system, Ordner heißt "user"
    datei("/user/data/user/patches/xml_prospero/PPSA01234.xml", b"<Patch/>"); datei("/user/data/user/patch_backup/cache.bin", b"c" * 50)
    # NEU 1.0.10: ein Ordner, zwei Kennungen - nur die Spiegel-Kachel PPSA99016 ist da (nicht PPSA99008); neue App ohne Kachel
    kachel("PPSA99016", "ProsperoEden Mirror")
    datei("/user/data/prosperoeden/user/nand/system.bin", b"s" * 30)
    datei("/user/data/mupen64plus/roms/spiel.z64", b"z" * 300)

# NEU 1.0.9 (#145): ProsperoStore (darkxex/ProsperoStore @44ae9f6) wie auf der PS5 - Belege intern, Arbeitsordner je Laufwerk.
# journal_ok=False: journal.json lässt sich nicht lesen (Lauf 4) bzw. ist kein JSON (kaputt=True, Lauf 6) - dann wird nichts herausgelöst
def store_baum(journal_ok, journal=True, kaputt=False):
    def pj(tid, titel):
        return f'{{"titleId":"{tid}","contentVersion":"01.000.000","localizedParameters":{{"defaultLanguage":"en-US","en-US":{{"titleName":"{titel}"}}}}}}'
    def beleg(tid, nennt=None):
        null64 = "0" * 64
        return f'{{"schema":1,"titleId":"{nennt or tid}","location":"/data/homebrew","contentVersion":"01.000.000","releaseTag":"v1","sha256":"{null64}","installedAt":"2026-11-08T08:00:00Z"}}\n'
    kachel("PPSA99000", "ProsperoStore"); kachel("PPSA99708", "Eichenhain"); kachel("PPSA99716", "Kieselbach")
    S, M = "/user/data/prosperostore", "/mnt/ext1/prosperostore"
    os.makedirs(W + "/mnt/ext1/user/app", exist_ok=True)
    datei("/user/data/homebrew/PPSA99708/sce_sys/param.json", pj("PPSA99708", "Eichenhain")); datei("/user/data/homebrew/PPSA99708/eboot.bin", ELF + b"k")
    # gehört der App: Beleg, aufgehobene Fassung (mit selbst hineingelegter Datei), nicht fertig gelöschte App, angefangener Download
    datei(f"{S}/receipts/PPSA99708.json", beleg("PPSA99708")); datei(f"{S}/receipts/PPSA99716.json", beleg("PPSA99716"))
    datei(f"{S}/previous/PPSA99708/sce_sys/param.json", pj("PPSA99708", "Eichenhain")); datei(f"{S}/previous/PPSA99708/eden/roms/spiel.bin", b"r" * 300)
    datei(f"{S}/trash/PPSA99708/eboot.bin", ELF + b"t")
    datei(f"{S}/previous/PPSA99716/sce_sys/param.json", pj("PPSA99716", "Kieselbach")); datei(f"{S}/previous/PPSA99716/notiz.txt", b"n")
    if journal_ok:   # Befund 12: eine PKG-Datei in der aufgehobenen Fassung steht bei ihrem Spiel
        pkg(f"{S}/previous/PPSA99716/pakete/Wolkenreise - CUSA40001 - v1.07 - EU - Update.pkg", "Wolkenreise", "CUSA40001", "gp", "01.07", rest=30)
    datei(f"{S}/staging/PPSA99716.zip", b"z" * 70)
    # bleibt im Rest: falscher und kaputter Beleg, keine Kennung, Kleinschreibung, Verknüpfung, angefangene Arbeit laut journal.json
    datei(f"{S}/receipts/PPSA99710.json", beleg("PPSA99710", "PPSA99711")); datei(f"{S}/receipts/PPSA99712.json", "kaputt")
    datei(f"{S}/previous/notiz.txt", b"n"); datei(f"{S}/previous/ppsa99715/datei.txt", b"x")
    os.symlink("../trash", W + f"{S}/previous/PPSA99714")
    if journal:
        jt = '{"schema":1,"operation":"install","state":"staging","titleId":"PPSA99713","location":"/data/homebrew"}\n'
        if journal_ok: datei(f"{S}/journal.json", jt)
        elif kaputt: datei(f"{S}/journal.json", "kaputt{")   # Codex-Prüfung 138: kein JSON (Lauf 6)
        else:   # Codex-Prüfung 138: gültiger Inhalt, aber nicht lesbar - auch unter root (PS5 Cleanup öffnet nie eine Verknüpfung)
            datei(f"{S}/journal_daten.json", jt); os.symlink("journal_daten.json", W + f"{S}/journal.json")
    datei(f"{S}/staging/PPSA99713.zip", b"z" * 50); datei(f"{S}/staging/PPSA99713/eboot.bin", ELF + b"u")
    datei(f"{S}/settings.txt", b"s"); datei(f"{S}/logs/store.log", b"l"); datei(f"{S}/cache/bild.png", b"p")
    if journal_ok:   # Codex-Prüfung 137: PKG-Datei im Rest - steht bei ihrem Spiel, der Rest des Unterordners bleibt bei ProsperoStore
        pkg(f"{S}/cache/pakete/Notizbuch - CUSA40003 - v1.01 - Update.pkg", "Notizbuch", "CUSA40003", "gp", "01.01", rest=31)
        datei(f"{S}/cache/pakete/info.txt", b"i")
    # M.2: App gelöscht (keine Kachel) - ihre aufgehobene Fassung und ein angefangener Download; dazu ein Rest von ProsperoStore
    datei(f"{M}/previous/PPSA99709/sce_sys/param.json", pj("PPSA99709", "Moosgarten")); datei(f"{M}/previous/PPSA99709/saves/stand1.sav", b"v" * 40)
    datei(f"{M}/staging/PPSA99709.zip", b"z" * 60); datei(f"{M}/staging/teil.tmp", b"t")
    os.makedirs(W + f"{M}/backup", exist_ok=True)

if art == "faelle":
    # --- PKG (Befund 12) ---
    ps4_spiel("CUSA40001", "Wolkenreise")
    ps4_spiel("CUSA40003", "Notizbuch", "gde")                       # eine App (PS4), installiert
    U = "/mnt/usb0"
    pkg(f"{U}/PS4/Wolkenreise - CUSA40001 - v1.00 - EU.pkg", "Wolkenreise", "CUSA40001", "gd", rest=11)
    pkg(f"{U}/PS4/Wolkenreise - CUSA40001 - v1.02 - EU - Update.pkg", "Wolkenreise", "CUSA40001", "gp", "01.02", rest=12)
    pkg(f"{U}/PS4/Nebelpfad - CUSA40002 - v1.00 - EU.pkg", "Nebelpfad", "CUSA40002", "gd", rest=500)   # dieselbe Datei auch auf der M.2
    pkg(f"{U}/PS4/Nebelpfad - CUSA40002 - v1.05 - EU - Update.pkg", "Nebelpfad", "CUSA40002", "gp", "01.05", rest=13)
    pkg(f"{U}/PS4/Notizbuch - CUSA40003 - v1.00.pkg", "Notizbuch", "CUSA40003", "gde", rest=14)
    pkg(f"{U}/PS4/Zeichenbrett - CUSA40004.pkg", "Zeichenbrett", "CUSA40004", "gde", rest=15)
    datei(f"{U}/PS4/unbekannt.pkg", b"kein PKG")
    datei(f"{U}/PS4/Cover/bild.jpg", b"jpg"); datei(f"{U}/PS4/liesmich.txt", b"txt")
    # 1.0.10 (PS5-Test 12.11.2026, Bild von 1.0.9): ps4mounts Bibliothek <Laufwerk>/ps4/<Spielnummer>/app.pkg mit Notiz origin.txt
    pkg(f"{U}/ps4/CUSA40021/app.pkg", "Regennacht", "CUSA40021", "gd", rest=31)
    datei(f"{U}/ps4/CUSA40021/origin.txt", "app.pkg\tRegennacht - CUSA40021 - v1.00 - EU.pkg\n")
    datei(f"{U}/ps4/CUSA40022/origin.txt", "app.pkg\tAltlast - CUSA40022 - v1.00 - EU.pkg\n")   # PKG schon weg: ein Rest
    # Codex-Prüfung 166: ein Ordner "origin.txt" ist keine Notiz; eine Bibliothek nur mit einer Notiz ohne PKG
    pkg(f"{U}/ps4/CUSA40024/app.pkg", "Funkstille", "CUSA40024", "gd", rest=32)
    datei(f"{U}/ps4/CUSA40024/origin.txt/backup.zip", b"zip")
    datei("/mnt/usb1/ps4/CUSA40023/origin.txt", "app.pkg\tEinsam - CUSA40023 - v1.00 - EU.pkg\n")
    # Codex-Prüfung 167: auf der M.2 kommt der Spielnummer-Ordner einzeln an
    pkg("/mnt/ext1/ps4/CUSA40025/app.pkg", "Steinbruch", "CUSA40025", "gd", rest=33)
    datei("/mnt/ext1/ps4/CUSA40025/origin.txt", "app.pkg\tSteinbruch - CUSA40025 - v1.00 - EU.pkg\n")
    datei("/mnt/ext1/ps4/CUSA40026/origin.txt", "app.pkg\tFernweh - CUSA40026 - v1.00 - EU.pkg\n")
    for i in range(8):                                                   # ab 6 gleicher Art auf einem Speicher: eine Zeile
        pkg(f"{U}/DLC/EP0001-CUSA40002_00-NEBELDLC0000000{i}.pkg", f"Nebelpfad Paket {i}", "CUSA40002", "ac", rest=20 + i)
    M = "/mnt/ext1"
    os.makedirs(W + M + "/user/app", exist_ok=True)
    pkg(f"{M}/PKG/Nebelpfad - CUSA40002 - v1.00 - EU.pkg", "Nebelpfad", "CUSA40002", "gd", rest=500)
    pkg(f"{M}/PKG/EP0001-CUSA40002_00-NEBELDLC00000010.pkg", "Nebelpfad Paket 10", "CUSA40002", "ac", rest=40)
    pkg(f"{M}/PKG/EP0001-CUSA40002_00-NEBELDLC00000011.pkg", "Nebelpfad Paket 11", "CUSA40002", "ac", rest=41)
    pkg("/user/data/pkg/Wolkenreise - CUSA40001 - v1.03 - EU - Update.pkg", "Wolkenreise", "CUSA40001", "gp", "01.03", rest=16)
    pkg(f"{U}/Sammlung/A/B/C/Tiefsee - CUSA40005 - v1.00.pkg", "Tiefsee", "CUSA40005", "gd", rest=17)
    pkg(f"{U}/Sammlung/A/B/C/D/Zu tief - CUSA40006.pkg", "Zu tief", "CUSA40006", "gd", rest=18)
    datei(f"{U}/Sammlung/A/notiz.txt", b"n")
    # --- Codex-Prüfung 83 ---
    # P1: tiefer als 8 Ebenen ungeprüft -> der ganze Ordner bleibt EIN Eintrag (auch die flache PKG-Datei darin)
    pkg(f"{U}/Tief/1/2/3/4/5/6/7/8/9/Grund - CUSA40007.pkg", "Grund", "CUSA40007", "gd", rest=22)
    pkg(f"{U}/Tief/1/Flach - CUSA40008.pkg", "Flach", "CUSA40008", "gd", rest=23)
    # NEU 1.0.10: Tiefe 9 wird geprüft (Grenze 64) - erst darunter bleibt ein Ordner ungeprüft
    pkg(f"{U}/Ganztief/" + "x/" * 66 + "Unten - CUSA40019.pkg", "Unten", "CUSA40019", "gd", rest=24)
    # P1: Tool-Ordner mit PKG-Datei und Spielstand-Sicherung - der Rest bleibt ein (roter) Tool-Ordner
    datei("/user/data/ps5upload/backups/spiel2/sdimg", b"Spielstand")
    pkg("/user/data/ps5upload/Wolkenreise - CUSA40001 - v1.04 - EU - Update.pkg", "Wolkenreise", "CUSA40001", "gp", "01.04", rest=19)
    # P2: zweites USB-Laufwerk - eigene Zeilen, "doppelt" zwischen beiden Laufwerken
    for i in range(3):
        pkg(f"/mnt/usb1/DLC/EP0001-CUSA40002_00-NEBELDLC0000002{i}.pkg", f"Nebelpfad Paket 2{i}", "CUSA40002", "ac", rest=60 + i)
    pkg("/mnt/usb1/PS4/Wolkenreise - CUSA40001 - v1.02 - EU - Update.pkg", "Wolkenreise", "CUSA40001", "gp", "01.02", rest=12)
    # P2: leerer Rest eines Spiels (/user/app ohne app.pkg und eboot.bin) ist nicht installiert
    os.makedirs(W + "/user/app/CUSA40009", exist_ok=True)
    for o in ("/user/appmeta/CUSA40009/param.sfo", "/system_data/priv/appmeta/CUSA40009/param.sfo"):
        datei(o, sfo([("CATEGORY", "gd"), ("TITLE", "Leerlauf"), ("TITLE_ID", "CUSA40009")]))
    pkg(f"{U}/PS4/Leerlauf - CUSA40009 - v1.00.pkg", "Leerlauf", "CUSA40009", "gd", rest=21)
    # P3: dieselbe Datei wie in der Zeile "Zusatzinhalte (8 PKG-Dateien)" auch auf der M.2
    pkg("/mnt/ext1/PKG/EP0001-CUSA40002_00-NEBELDLC00000000.pkg", "Nebelpfad Paket 0", "CUSA40002", "ac", rest=20)
    # --- Codex-Prüfung 85 ---
    # P1: unbekanntes Tool mit Kachel, Sicherungen und einer PKG-Datei im Daten-Ordner - der Rest bleibt rot bei der Kachel
    kachel("TEMU71030", "TestEmulator")
    datei("/user/data/TestEmulator/backups/spiel3/sdimg", b"Spielstand")
    pkg("/user/data/TestEmulator/Wolkenreise - CUSA40001 - v1.05 - EU - Update.pkg", "Wolkenreise", "CUSA40001", "gp", "01.05", rest=24)
    # P2: tote ShadowMount-Kachel (Spieldatei fehlt) ist kein installiertes Spiel
    tot = "PPSA40010"
    j = f'{{"titleId":"{tot}","localizedParameters":{{"defaultLanguage":"en-US","en-US":{{"titleName":"Totenreich"}}}},"contentVersion":"01.00"}}'
    datei(f"/user/app/{tot}/sce_sys/param.json", j); datei(f"/user/appmeta/{tot}/param.json", j)
    datei(f"/user/app/{tot}/mount_img.lnk", f"/mnt/shadowmnt/{tot}\n/mnt/ext1/homebrew/Totenreich - {tot}.exfat")
    pkg(f"{U}/PS5/Totenreich - {tot} - v1.00.pkg", "Totenreich", tot, "gd", rest=25)
    # --- Codex-Prüfung 86 ---
    # P2: eine Kachel heißt wie der Unterordner im Rest eines Tool-Ordners ("backups") - der Rest bleibt bei seinem Besitzer
    kachel("BACK71031", "Backups")
    # --- Codex-Prüfung 90 ---
    # P2: Tool-Kachel mit LEEREM App-Ordner (nur noch die Kachel-Infos) ist kein installiertes Programm - ihre PKG-Datei steht unter
    # "Leerwerkzeug · nicht installiert"
    tl = "LEER71033"
    j = f'{{"titleId":"{tl}","applicationCategoryType":65536,"deeplinkUri":"http://127.0.0.1:9095/","localizedParameters":{{"defaultLanguage":"en-US","en-US":{{"titleName":"Leerwerkzeug"}}}}}}'
    datei(f"/user/appmeta/{tl}/param.json", j); datei(f"/system_data/priv/appmeta/{tl}/param.json", j)
    os.makedirs(W + f"/user/app/{tl}", exist_ok=True)
    pkg(f"{U}/Leerwerkzeug-{tl}.pkg", "Leerwerkzeug", tl, "gd", rest=26)
    # --- Codex-Prüfung 90 (P1) ---
    # Ordner mit mehr Einträgen, als die PKG-Suche je Ordner liest (10000): bleibt EIN Eintrag, kann PKG-Dateien enthalten - rot, nur einzeln
    for i in range(10001): datei(f"{U}/Viele/f{i:05d}.txt", b"v")
    pkg(f"{U}/Viele/Paket - CUSA40011.pkg", "Paket", "CUSA40011", "gd", rest=29)
    # --- Codex-Prüfung 93 ---
    # P1: PKG-Datei in einem Ordner, in dem auch eine Kopie von PS5 Cleanup liegt ("alles außer PS5 Cleanup") - auch dort bei ihrem Spiel
    datei("/user/data/pldmgr/local/ps5c/ps5c.elf", b"\x7fELF" + b"\0" * 2000 + b"PS5CLEANUP-ELF-KENNUNG:7c1e5b2a-selbstschutz" + b"\0" * 2000)
    datei("/user/data/pldmgr/local/notiz.txt", b"n")
    pkg("/user/data/pldmgr/Pakete/Wolkenreise - CUSA40001 - v1.06 - EU - Update.pkg", "Wolkenreise", "CUSA40001", "gp", "01.06", rest=28)
    # Gegenprobe: dieselbe Art Kachel MIT Inhalt im App-Ordner - ihre PKG-Datei steht bei ihr
    kachel("VOLL71034", "Vollwerkzeug", "http://127.0.0.1:9096/")
    pkg(f"{U}/Vollwerkzeug-VOLL71034.pkg", "Vollwerkzeug", "VOLL71034", "gd", rest=27)
    # --- Codex-Prüfung 98 ---
    # P1: Daten-Ordner eines bekannten Tools (ps5save) mit einer Kopie von PS5 Cleanup, einer PKG-Datei und Spielstand-Sicherungen ("alles
    # außer PS5 Cleanup"): die Sammelzeile bleibt Daten des Tool-Ordners - rot, nicht "neu herunterladen"; ebenso der Rest eines dort
    # aufgeteilten Unterordners. Gegenprobe: unbekannter Ordner (sicherer) - wie der ganze Ordner "Inhalt unbekannt", nicht "neu herunterladen"
    for o in ("ps5save", "sicherer"):
        datei(f"/user/data/{o}/ps5c.elf", b"\x7fELF" + b"\0" * 2000 + b"PS5CLEANUP-ELF-KENNUNG:7c1e5b2a-selbstschutz" + b"\0" * 2000)
        datei(f"/user/data/{o}/backups/spiel1/sdimg", b"Spielstand")
    pkg("/user/data/ps5save/Sternfall - CUSA40041 - v1.00.pkg", "Sternfall", "CUSA40041", "gd", rest=35)
    pkg("/user/data/ps5save/ablage/Sternfall - CUSA40041 - v1.01 - Update.pkg", "Sternfall", "CUSA40041", "gp", "01.01", rest=36)
    datei("/user/data/ps5save/ablage/notiz.txt", b"n")
    # --- PS5-Test 07.11.2026 (da_Chris86 08:46) ---
    # Entpack-Ordner von Orbit Store mit EINEM Spielordner (Spielnummer im Namen, darin halb entpackte .pak-Dateien wie auf da_Chris86s M.2):
    # gehört zu diesem Spiel. Gegenprobe: zwei verschiedene Spielnummern darin - bleibt für sich
    kachel("PPSA40042", "Sternwind", kategorie=0)
    AMPR = b"AMPR" + b"\0" * 60
    datei("/mnt/ext1/homebrew/.orbit-extract-a1b2c3d4/LIZARD  Sternwind     01.000.000 PPSA40042/ampr_assets-bulk-lane00-vol00-002.pak", AMPR)
    datei("/mnt/ext1/homebrew/.orbit-extract-a1b2c3d4/LIZARD  Sternwind     01.000.000 PPSA40042/ampr_assets-default-lane00-vol00-003.pak", AMPR)
    datei("/mnt/ext1/homebrew/.orbit-extract-e5f6a7b8/Sternwind PPSA40042/ampr_assets-bulk-lane00-vol00-001.pak", AMPR)
    datei("/mnt/ext1/homebrew/.orbit-extract-e5f6a7b8/Anderes PPSA40043/ampr_assets-bulk-lane00-vol00-001.pak", AMPR)
    # PKG-Datei im PS5-Format (Kopf "\x7FFIH", Spielnummer nicht lesbar) im Daten-Ordner eines Tools mit Kachel: gehört zu diesem Tool
    datei("/user/data/TestEmulator/TestEmulator_2.0.pkg", b"\x7fFIH" + b"\0" * 300)
    # Codex-Prüfung 102: dasselbe, wenn im Ordner NUR diese PKG-Datei liegt (kein Rest); Gegenprobe: Ordner ohne passende Kachel bleibt unter
    # Sonstiges; eine Verknüpfung .orbit-extract-... auf einen Entpack-Ordner gehört zu keinem Spiel
    kachel("NURP71035", "Nurpaket")
    datei("/user/data/Nurpaket/Nurpaket_1.0.pkg", b"\x7fFIH" + b"\1" * 300)
    datei("/user/data/Fremdablage/fremd.pkg", b"\x7fFIH" + b"\2" * 300)
    os.symlink(".orbit-extract-a1b2c3d4", W + "/mnt/ext1/homebrew/.orbit-extract-c9c9c9c9")
    # Codex-Prüfung 103: derselbe Namensvergleich wie für den Ordner, auch "ps5-" vorne (Ordner ps5-webdateien zur Kachel "Webdateien")
    kachel("WEBD71036", "Webdateien")
    datei("/user/data/ps5-webdateien/setup.pkg", b"\x7fFIH" + b"\3" * 300)
    # --- PS5-Test 07.11.2026 (da_Chris86: Orbit Store "in einem Block unter Apps") ---
    # dasselbe Programm zweimal: Tool-Kachel (ELF, Bedienung im Browser) und App über ShadowMount (.ffpkg auf der M.2), beide "Sternhafen"
    def sm_kachel(tid, titel, kategorie, groesse=100, spieldatei=True):
        j = f'{{"titleId":"{tid}","applicationCategoryType":{kategorie},"localizedParameters":{{"defaultLanguage":"en-US","en-US":{{"titleName":"{titel}"}}}}}}'
        datei(f"/user/app/{tid}/sce_sys/param.json", j); datei(f"/user/appmeta/{tid}/param.json", j)
        datei(f"/user/app/{tid}/mount_img.lnk", f"/mnt/shadowmnt/{tid}\n/mnt/ext1/homebrew/{tid}.ffpkg")
        if spieldatei: datei(f"/mnt/ext1/homebrew/{tid}.ffpkg", b"F" * groesse)
    kachel("ORBT71040", "Sternhafen", "http://127.0.0.1:34177/")
    sm_kachel("PPSA71041", "Sternhafen", 0, 50000)   # Spieldatei größer als die Kachel - steht trotzdem direkt hinter ihr (Codex-Prüfung 109)
    # Gegenprobe: unter Apps zwei Kacheln "Zwilling" - die ShadowMount-Kachel gleichen Namens bleibt unter Spiele (nicht raten)
    kachel("ZWIL71042", "Zwilling", "http://127.0.0.1:9097/"); kachel("ZWIL71043", "Zwilling", "http://127.0.0.1:9098/")
    sm_kachel("PPSA71044", "Zwilling", 0)
    # App über ShadowMount ohne Tool-Kachel, laut eigenen Angaben eine App (applicationCategoryType 65536): unter Apps
    sm_kachel("PPSA71045", "Ladenfenster", 65536)
    # Codex-Prüfung 109 (P3): App über ShadowMount, deren Spieldatei fehlt (tote Kachel) - unter Apps, Art wie unter Spiele
    sm_kachel("PPSA71046", "Ladenleer", 65536, spieldatei=False)
    # Prüfer 110 (P3): dieselbe App normal installiert und zusätzlich über ShadowMount - ein Block, auch beim Zählen
    kachel("APPN71047", "Doppelapp")
    sm_kachel("PPSA71048", "Doppelapp", 65536)
    # --- ELF (Befund 14 und da_Chris86 17:47) ---
    kachel("WKAL00001", "WebKit Autoloader v0.4.0", "http://127.0.0.1:7577/")   # Jailbreak-Einstieg (itsPLK)
    datei(f"{U}/webkit-autoloader-installer_v0.6.2.elf", ELF + b"a")
    datei(f"{U}/webkit-autoloader-installer_v0.7.0-dev-3a34c3b.elf", ELF + b"b")
    datei("/user/data/ps5_autoloader/autoload.txt", b"kstuff.elf\n")
    kachel("WERK71020", "Werkzeugkiste v2.1", "http://127.0.0.1:8893")
    datei(f"{U}/werkzeugkiste_v2.0.elf", ELF + b"c"); datei(f"{U}/Werkzeugkiste-2.1.elf", ELF + b"d")
    datei(f"{U}/Zangenwerk_v0.4.4.elf", ELF + b"e"); datei(f"{U}/Zangenwerk_v0.4.9-pre.elf", ELF + b"f")
    datei(f"{U}/ps5-hammer_v1.elf", ELF + b"g"); datei(f"{U}/hammer-ps5-v2.elf", ELF + b"h")
    datei(f"{U}/einzeln_v1.elf", ELF + b"i")
    kachel("SPUR71021", "Spurensuche 3")                                   # Titel mit Zahl, aber ohne Version: bleibt
    datei(f"{U}/spurensuche_v1.elf", ELF + b"j")
    # --- Befund 2: bekannter wichtiger Ordner ohne Inhalt / mit Inhalt ---
    os.makedirs(W + "/user/data/etaHEN/cheats", exist_ok=True); os.makedirs(W + "/user/data/etaHEN/plugins", exist_ok=True)
    datei("/user/data/etaHEN/config.ini", b"")
    datei("/user/data/garlic/backups/spiel1/sdimg", b"Spielstand")
    # --- NEU 1.0.9 (#145): ProsperoStore - Beleg, aufgehobene Fassung und Reste stehen bei ihrer App, der Rest bei ProsperoStore ---
    store_baum(True)
    app_daten_baum()   # NEU 1.0.9 (#146)
    # --- NEU 1.0.10 (da_Chris86 12.11.2026, Durchsicht seiner ganzen PS5): Daten je App in /system_data ---
    # Web-Zwischenspeicher (Sony) und ps4mount-Daten je Kennung: bei der Kachel (Vollwerkzeug VOLL71034), sonst Rest; NPXS nie
    for d in ("/system_data/httpcache/nobackup/VOLL71034", "/system_data/httpcache/nobackup/PPSA79999", "/system_data/httpcache/nobackup/NPXS40002"):
        os.makedirs(W + d, exist_ok=True)
    datei("/system_data/ps4mount/stage/VOLL71034/mount.lnk", b"x")
    datei("/system_data/ps4mount/app/CUSA79998/app.json", b"{}")
    # NEU 1.0.10 (da_Chris86 14.11.2026 19:36): PKG eines nicht installierten Spiels und ps4mount-Daten mit derselben Kennung
    pkg("/mnt/ext1/PKG/Sternfall - CUSA40071 - v1.00 - EU.pkg", "Sternfall", "CUSA40071", "gd", rest=70)
    datei("/system_data/ps4mount/app/CUSA40071/app.json", b"{}")
    datei("/mnt/usb0/ps4/CUSA79998/app.pkg", b"Spiel")   # Codex-Prüfung 159 (5): das Ziel liegt im Testbaum und ist auflösbar
    os.symlink(W + "/mnt/usb0/ps4/CUSA79998/app.pkg", W + "/system_data/ps4mount/app/CUSA79998/app.pkg")
    # Sicherungen eines Tools neben Sonys App-Datenbank (TabShifter): zur Kachel "TabShifter"; ein Tool ohne Kachel: Rest;
    # Sonys eigene Dateien und ein namenloses app.db.bak nie
    kachel("TBSH71050", "TabShifter", "http://127.0.0.1:9099/")
    for n in ("app.db.tabshifter.bak", "appinfo.db.tabshifter.bak", "app.db.fremdtool.bak", "app.db.bak", "app.db", "app.bckdb", "addcont_backup.db"):
        datei("/system_data/priv/mms/" + n, b"S")
    # Selbst hochgeladene Programme beim Payload Manager (payloads/local, "web_upload") gehören dem Programm, nicht dem Verwalter:
    # TabShifter zur Kachel TabShifter, ein Programm ohne Kachel für sich; Downloads aus einer Quelle (payloads/repo) bleiben beim Verwalter
    datei("/user/data/pldmgr/payloads/local/tabshifter/TabShifter.elf", ELF + b"T" * 300)
    datei("/user/data/pldmgr/payloads/local/tabshifter/meta.json", b'{"kind":"local","origin":"web_upload"}')
    datei("/user/data/pldmgr/payloads/local/einzelwerk/einzelwerk.elf", ELF + b"E" * 300)
    datei("/user/data/pldmgr/payloads/repo/klogsrv@e8cce5d1/klogsrv.elf", ELF + b"K" * 300)
    # NEU 1.0.10 (da_Chris86 13.11.2026): ein Download aus einer Quelle steht beim selben Programm, wenn PS5 Cleanup es schon anzeigt -
    # neben seiner Programmdatei auf dem USB-Laufwerk, unter seiner Kachel, bei seinem Daten-Ordner ("ps5-" vorne zählt nicht);
    # sonst (klogsrv) beim Verwalter; mit einer Kopie von PS5 Cleanup darin bleibt er, wie er ist; ein verknüpfter Ordner nie
    datei(f"{U}/Schattenwerk_1.7-beta11.elf", ELF + b"S" * 300)
    datei("/user/data/pldmgr/payloads/repo/schattenwerk@e8cce5d1/Schattenwerk_1.7beta11.elf", ELF + b"s" * 300)
    datei("/user/data/pldmgr/payloads/repo/schattenwerk@e8cce5d1/meta.json", b'{"kind":"repo"}')
    kachel("KWRK71052", "Kachelwerk", "http://127.0.0.1:9101/")
    datei("/user/data/pldmgr/payloads/repo/kachelwerk@e8cce5d1/kachelwerk.elf", ELF + b"k" * 300)
    datei("/user/data/datenwerk/einstellungen.ini", b"x=1")
    datei("/user/data/pldmgr/payloads/repo/ps5-datenwerk@e8cce5d1/datenwerk.elf", ELF + b"d" * 300)
    datei("/user/data/schutzwerk/einstellungen.ini", b"x=1")
    datei("/user/data/pldmgr/payloads/repo/schutzwerk@e8cce5d1/ps5c.elf", b"\x7fELF" + b"\0" * 2000 + b"PS5CLEANUP-ELF-KENNUNG:7c1e5b2a-selbstschutz" + b"\0" * 2000)
    datei(f"{U}/linkwerk_v2.elf", ELF + b"L" * 300)
    datei("/extern/linkwerk-ziel/linkwerk.elf", ELF + b"l" * 300)
    os.symlink(W + "/extern/linkwerk-ziel", W + "/user/data/pldmgr/payloads/repo/linkwerk@e8cce5d1")
    # Codex-Prüfung 174 (P2): eine PKG-Datei im Download-Ordner bzw. im Upload-Ordner bleibt eine eigene Zeile mit Warnung - der Ordner bleibt, wie er ist
    datei(f"{U}/pkgwerk_v1.elf", ELF + b"P" * 300)
    datei("/user/data/pldmgr/payloads/repo/pkgwerk@e8cce5d1/pkgwerk.elf", ELF + b"p" * 300)
    pkg("/user/data/pldmgr/payloads/repo/pkgwerk@e8cce5d1/Pkgwerk - CUSA40061 - v1.00.pkg", "Pkgwerk", "CUSA40061", "gd", rest=21)
    datei("/user/data/pldmgr/payloads/local/upkg/upkg.elf", ELF + b"u" * 300)
    pkg("/user/data/pldmgr/payloads/local/upkg/Upkg - CUSA40062 - v1.00.pkg", "Upkg", "CUSA40062", "gd", rest=22)
    # derselbe Aufbau bei einem anderen Verwalter ohne Kopie von PS5 Cleanup (der Ordner war bis hier EIN Eintrag)
    datei("/user/data/zweitlader/payloads/local/tabshifter/TabShifter.elf", ELF + b"Z" * 300)
    datei("/user/data/zweitlader/einstellungen.txt", b"e")
    # Codex-Prüfung 159: (1) Verwalter, in dem NUR das Hochgeladene liegt; (2) payloads als Verknüpfung - nie dahinter aufteilen;
    # (3) im Upload-Ordner auch eine Kopie von PS5 Cleanup (der Ordner ist schon aufgeteilt); (4) Sicherungen in gemischter Schreibweise
    datei("/user/data/nurlader/payloads/local/tabshifter/TabShifter.elf", ELF + b"N" * 300)
    datei("/user/data/linklader/einstellungen.txt", b"e")
    datei("/extern/linklader-payloads/local/tabshifter/TabShifter.elf", ELF + b"L" * 300)
    os.symlink(W + "/extern/linklader-payloads", W + "/user/data/linklader/payloads")
    datei("/user/data/pldmgr/payloads/local/mitkopie/ps5c.elf", b"\x7fELF" + b"\0" * 2000 + b"PS5CLEANUP-ELF-KENNUNG:7c1e5b2a-selbstschutz" + b"\0" * 2000)
    datei("/user/data/pldmgr/payloads/local/mitkopie/werk.elf", ELF + b"W" * 300)
    datei("/user/data/pldmgr/payloads/local/mitkopie/meta.json", b'{"kind":"local"}')
    kachel("MISC71051", "Mischwerk", "http://127.0.0.1:9100/")
    # da_Chris86 12.11.2026 06:48 (Liste auf der PS5): Zwischenspeicher von YouTube (Jailbreak-Einstieg Y2JB) bei YouTube;
    # Sperrdatei eines Payloads ohne Kachel (garlic-savemgr.instance.lock) bei seiner ELF-Datei
    kachel("PPSA01650", "YouTube")
    os.makedirs(W + "/system_data/httpcache/nobackup/PPSA01650", exist_ok=True)
    datei("/mnt/usb0/garlic-savemgr_v1.15-beta1.elf", ELF + b"G" * 300)
    datei("/user/data/garlic-savemgr.instance.lock", b"")
    # da_Chris86 12.11.2026 06:44-06:59: Spiele aus Playden stehen im Block von Playden; Spieldateien und Spielstände laut Playdens Liste
    # (runtime-state/installs.json) bei ihrer Kachel; Unsinn in der Liste wird nicht beachtet
    kachel("FAKE77031", "Playden"); kachel("FAKE78001", "3DMark"); kachel("FAKE78002", "Fallout: New Vegas")
    datei("/user/data/homebrew/FAKE78001/lapy.elf", ELF + b"A" * 100); datei("/user/data/homebrew/FAKE78002/lapy.elf", ELF + b"B" * 100)
    datei("/user/data/playden/logs/runtime.log", b"log"); datei("/user/data/playden/kits/playden-kit/wine.bin", b"wine")
    datei("/user/data/playden-games/223850/3DMark.exe", b"MZ" + b"x" * 500); datei("/user/data/playden-games/999/rest.exe", b"MZ")
    datei("/user/data/playden/games/steam-223850/save.dat", b"s" * 50); datei("/user/data/playden/games/steam-22380/save.dat", b"t" * 50)
    datei("/mnt/ext1/playden/games/22380/FalloutNV.exe", b"MZ" + b"y" * 500)
    datei("/user/data/playden/runtime-state/installs.json", (
        # NEU 12.11.2026 15:12: Einträge wie Playden sie schreibt (install_id, state, journal, Reservierungen) - für das Austragen
        '{"schema_version":1,"next_runtime_number":3000000,"reservations":[{"install_id":"steam-223850","title_id":"FAKE78001"},'
        '{"install_id":"steam-22380","title_id":"FAKE78002"},{"install_id":"steam-1010","title_id":"FAKE78010"},{"install_id":"steam-1012","title_id":"FAKE78012"},{"install_id":"steam-1013","title_id":"FAKE78013"}],"installs":['
        '{"install_id":"steam-223850","title_id":"FAKE78001","app_id":223850,"install_path":"/data/playden-games/223850","data_root":"/data/playden/games/steam-223850","state":"ready","last_failure":null,"journal":null,"size":12345678901234567890},'
        '{"install_id":"steam-22380","title_id":"FAKE78002","app_id":22380,"install_path":"/mnt/ext1/playden/games/22380","data_root":"/data/playden/games/steam-22380","state":"ready","last_failure":null,"journal":null},'
        '{"title_id":"FAKE78003","app_id":1,"install_path":"/data/homebrew","data_root":"/data/pldmgr"},'
        '{"title_id":"FAKE78004","app_id":2,"install_path":"/data/playden-games/../pldmgr","data_root":"/data/playden/games/.."},'
        '{"title_id":"PPSA01650","app_id":3,"install_path":"/data/playden-games/999","data_root":"/data/playden/logs"},'
        # Codex-Prüfung 160: (1) zwei Spiele mit demselben Ordner, (3) Spielstände mit einer Kopie von PS5 Cleanup darin, (4) Spiel ohne Kachel
        '{"title_id":"FAKE78006","app_id":6,"install_path":"/data/playden-games/777","data_root":"/data/playden/games/steam-6"},'
        '{"title_id":"FAKE78007","app_id":7,"install_path":"/data/playden-games/777","data_root":"/data/playden/games/steam-7"},'
        '{"title_id":"FAKE78008","app_id":8,"install_path":"/data/playden-games/888","data_root":"/data/playden/games/steam-888"},'
        '{"title_id":"FAKE78009","app_id":9,"install_path":"/data/playden-games/555","data_root":"/data/playden/games/steam-555"},'
        # NEU 1.0.10 (da_Chris86 12.11.2026 20:06, PS5-Test 19:49): Kachel und Spieldateien schon weg, nur noch in Playdens Listen (FAKE78010);
        # dasselbe, aber Playden arbeitet gerade daran (FAKE78011) - wird nicht angeboten
        '{"install_id":"steam-1010","title_id":"FAKE78010","app_id":1010,"install_path":"/data/playden-games/1010","data_root":"/data/playden/games/steam-1010","state":"ready","last_failure":null,"journal":null},'
        '{"install_id":"steam-1011","title_id":"FAKE78011","app_id":1011,"install_path":"/data/playden-games/1011","data_root":"/data/playden/games/steam-1011","state":"installing","last_failure":null,"journal":{"op":"install"}},'
        # Codex-Prüfung 169: (2) Sony führt die Kachel noch (FAKE78012) - Playdens Eintrag bleibt; (1) Spielstände eines solchen Spiels, dessen
        # Vorprüfung scheitert (FAKE78013, zwei Zeilen in der Spieleliste) - Eintrag und Spielstände bleiben
        '{"install_id":"steam-1012","title_id":"FAKE78012","app_id":1012,"install_path":"/data/playden-games/1012","data_root":"/data/playden/games/steam-1012","state":"ready","last_failure":null,"journal":null},'
        '{"install_id":"steam-1013","title_id":"FAKE78013","app_id":1013,"install_path":"/data/playden-games/1013","data_root":"/data/playden/games/steam-1013","state":"ready","last_failure":null,"journal":null}]}').encode())
    kachel("FAKE78006", "Zwilling Eins"); kachel("FAKE78007", "Zwilling Zwei"); kachel("FAKE78008", "Mitkopie")
    # Playdens Spieleliste (.playden-games) und aufgehobene Spielstände eines deinstallierten Spiels (NEU 12.11.2026 15:25)
    datei("/user/data/playden-games/.playden-games", b"223850\tinternal\t10224767053\n22380\text1\t9999\n1010\tinternal\t5000\n1011\tinternal\t0\n1012\tinternal\t5000\n1013\tinternal\t5000\n1013\tinternal\t5000\n")
    datei("/user/data/playden/games/steam-1013/drive_c/save.sav", b"3" * 20)
    # NEU 1.0.10 (20:06, TabShifter TBSH00001): Kachel ohne jede Datei (nur Sonys Eintrag, im Test über appinfo_lesen) - VERW71060 wird
    # angeboten; VERW71061 hat einen Einhängeordner eines Tools (bleibt), VERW71062 ist normal installiert
    os.makedirs(W + "/system_ex/app/VERW71061", exist_ok=True)
    kachel("VERW71062", "Normal installiert")
    kachel("KOPI00002", "Kopie mit Ordner", "http://127.0.0.1:9091/")   # aktive, direkt eingetragene Homebrew-Kachel (Sony lehnt im Test ab)
    # NEU 1.0.10 (da_Chris86 23:43): das Programm der Kachel ohne Dateien "Kopie" (KOPI00001), selbst hochgeladen beim Payload Manager - steht unter ihr
    datei("/user/data/pldmgr/payloads/local/kopie/Kopie.elf", ELF + b"K" * 300)
    datei("/user/data/pldmgr/payloads/local/kopie/meta.json", b'{"kind":"local","origin":"web_upload"}')
    datei("/user/data/playden/games/steam-400.kept/playden-kept.json", b'{"app_id":400,"install_id":"steam-400","name":"Portal","kept_at":1,"bytes":5}')
    datei("/user/data/playden/games/steam-400.kept/drive_c/save.sav", b"p" * 20)
    datei("/user/data/playden/games/steam-223850.saves/1.sav", b"b" * 20)
    datei("/user/data/playden-games/777/geteilt.exe", b"MZ"); datei("/user/data/playden/games/steam-6/s", b"6"); datei("/user/data/playden/games/steam-7/s", b"7")
    datei("/user/data/playden-games/888/spiel.exe", b"MZ"); datei("/user/data/playden/games/steam-888/save.dat", b"8" * 50)
    datei("/user/data/playden/games/steam-888/ps5c.elf", b"\x7fELF" + b"\0" * 2000 + b"PS5CLEANUP-ELF-KENNUNG:7c1e5b2a-selbstschutz" + b"\0" * 2000)
    datei("/user/data/playden-games/555/ohne.exe", b"MZ"); datei("/user/data/playden/games/steam-555/s", b"5")
    datei("/system_data/priv/mms/app.db.MischWerk.bak", b"S"); datei("/system_data/priv/mms/appinfo.db.mischwerk.bak", b"S")
elif art == "store_kaputt":
    store_baum(False)   # NEU 1.0.9 (#145): journal.json nicht lesbar - nichts herauslösen (Lauf 4)
elif art == "store_json_kaputt":
    store_baum(False, kaputt=True)   # Codex-Prüfung 138: journal.json ist kein JSON - nichts herauslösen (Lauf 6)
elif art == "store_ohne_journal":
    store_baum(True, journal=False)   # Codex-Prüfung 137: keine angefangene Arbeit - auch PPSA99713 wird herausgelöst (Lauf 5)
else:
    # 2000 PKG-Dateien: 40 Spiele (10 davon installiert), je Spiel Installationsdatei, Updates, Zusatzinhalte - verteilt auf
    # USB-Laufwerk (Ordner PS4, PS5, DLC), M.2 (Ordner PKG) und intern (/data/pkg)
    for s in range(10): ps4_spiel(f"CUSA5{s:04d}", f"Spiel {s}")
    n = 0; orte = [("/mnt/usb0/PS4", 600), ("/mnt/usb0/DLC", 600), ("/mnt/ext1/PKG", 500), ("/user/data/pkg", 300)]
    os.makedirs(W + "/mnt/ext1/user/app", exist_ok=True)
    for ort, anz in orte:
        for i in range(anz):
            s = (n * 7) % 40; tid = f"CUSA5{s:04d}"
            k = "ac" if "DLC" in ort or i % 5 >= 2 else ("gp" if i % 5 == 1 else "gd")
            name = f"Spiel {s} - {tid} - {'DLC' if k == 'ac' else 'Update' if k == 'gp' else 'Basis'} {n}.pkg"
            pkg(f"{ort}/{name}", f"Spiel {s}", tid, k, f"01.{n % 100:02d}", rest=n % 97)
            n += 1
    print(n, "PKG-Dateien")
