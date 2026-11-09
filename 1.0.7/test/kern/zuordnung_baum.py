# PS5 Cleanup 1.0.8 - Testbaum "PKG- und ELF-Dateien bei ihrem Spiel bzw. Programm" (PS5-Test 06.11.2026, Befunde 12 und 14,
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
