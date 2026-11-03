# PS5 Cleanup 1.0.7 - Zufallstest mit erfundenen Konsolen (da_Chris86 02.11.2026 19:58/20:04)
# Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
# Jede Konsole bekommt eine Firmware (3.xx bis 13.60) und zufälligen Inhalt: PS5- und PS4-Spiele, Apps, Homebrew, Tools mit Kachel,
# Daten-Ordner in verschiedenen Schreibweisen, Ordner auf M.2 und USB-Stick, ELF-Payloads, PKG-Dateien, Trophäen (1-3 Listen je
# Spiel), Aufnahmen samt zweiter Fassung (sdr), alte Sicherungen von PS5 Cleanup und Fallen (unbekannte Ordner, zu kurze Namen,
# PKG nicht installierter Spiele). Weil der Erzeuger alles selbst erfunden hat, weiß er, wo jedes Teil hingehört - test_zufall
# (gleicher Kern wie auf der PS5, nur lesend) muss genau das liefern.
# Aufruf: python3 zufall.py <test_zufall> <arbeitsordner> <anzahl> [startwert]
import os, random, shutil, struct, subprocess, sys

WOERTER = ["nova", "pixel", "garden", "stellar", "forge", "lumen", "drift", "harbor", "quartz", "ember", "violet", "canyon",
           "zephyr", "mosaic", "tundra", "orchid", "falcon", "glacier", "summit", "velvet", "cobalt", "meadow", "rocket",
           "saffron", "thunder", "willow", "cascade", "nimbus", "pebble", "raven", "sierra", "topaz", "umbra", "vortex",
           "wander", "yonder", "zenith", "atlas", "bramble", "cinder", "dynamo", "eclipse", "fable", "gizmo", "halcyon",
           "indigo", "jasper", "kestrel", "lagoon", "marble", "nectar", "onyx", "prairie", "quiver", "rustic", "sprocket"]
# Wörter, die im Kern eine feste Bedeutung haben (bekannte Tools, Sammelordner) - nie für erfundene Apps
VERBOTEN = {"pldmgr", "pkgmgr", "bfpilot", "orbitstore", "shadowmount", "ps5upload", "gamecompressor", "homebrew", "nanodns", "etahen",
            "spectrumlibrary", "playden", "zftpd", "blackbox", "ps5autoloader", "cheatrunner", "garlic", "liveshell", "neoenabler",
            "neosettings", "onionhen", "ps4mount", "dumpforge", "wfm", "ps5save", "amprstudio", "ps5appdumper", "elfarsenal", "goldhen",
            "psxs5", "exfat2ampr", "ps5suitefancfg", "user", "games", "payload", "payloads", "data", "tmp", "temp"}
FIRMWARES = ["3.20", "4.03", "4.51", "5.50", "6.02", "7.61", "8.40", "9.60", "10.01", "11.00", "11.60", "12.00", "13.00", "13.60"]

def sfo(werte):
    keys = sorted(werte); keytab = b''; daten = b''; ein = []
    for k in keys:
        d = werte[k].encode('utf-8') + b'\0'; koff = len(keytab); keytab += k.encode() + b'\0'
        mx = (len(d) + 3) // 4 * 4; ein.append((koff, 0x0204, len(d), mx, len(daten))); daten += d.ljust(mx, b'\0')
    while len(keytab) % 4: keytab += b'\0'
    kt = 20 + 16 * len(keys); dt = kt + len(keytab)
    return b'\0PSF' + struct.pack('<IIII', 0x101, kt, dt, len(keys)) + b''.join(struct.pack('<HHIII', *e) for e in ein) + keytab + daten

def schluessel(s): return ''.join(c for c in s.lower() if c.isalnum())

def erzeuge(W, R):
    """Baut einen Baum unter W. Rückgabe: Erwartungen."""
    def datei(p, inhalt=b'x'):
        full = os.path.join(W, p.lstrip('/')); os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, 'wb') as f: f.write(inhalt if isinstance(inhalt, bytes) else inhalt.encode())
    def ordner(p): os.makedirs(os.path.join(W, p.lstrip('/')), exist_ok=True)
    for p in ["/user/app", "/user/appmeta", "/user/data", "/user/patch", "/user/addcont", "/user/av_contents/photo", "/user/av_contents/video",
              "/user/av_contents/thumbnails", "/user/trophy/conf", "/user/trophy2/nobackup/conf", "/user/license", "/user/download", "/user/temp",
              "/user/system", "/system_data/priv/appmeta", "/system_ex/app"]:
        ordner(p)
    fw = R.choice(FIRMWARES); fwz = float(fw)
    uid = "%08x" % R.getrandbits(32); ordner("/user/home/%s/savedata_prospero" % uid)
    # Codex-Prüfung 40 (6): manchmal ein zweites Konto - dessen Trophäen-Fortschritt darf nie unter dem des ersten stehen
    uid2 = "%08x" % R.getrandbits(32) if R.random() < 0.4 else None
    if uid2: ordner("/user/home/%s/savedata_prospero" % uid2)
    m2 = fwz >= 4.0 and R.random() < 0.7
    stick = R.random() < 0.8
    if m2: ordner("/mnt/ext1/user/app")
    if stick: ordner("/mnt/usb0")
    erw = {"fw": fw, "kinder": [], "frei": [], "trophaeen": [], "aufnahmen": [], "sicherungen": [], "koepfe": {},
           "namen": [], "sid": {}, "geschuetzt": []}   # NEU 1.0.7 (02.11.2026 23:05): Namen/Zuordnung nur aus Metadaten, kaputte Zusatzinhalt-Angaben
    # Namen: eindeutig, keiner im anderen enthalten, kein gemeinsamer Anfang ab 8 Zeichen
    keys = []
    def neuer_name():
        for _ in range(200):
            w = R.sample(WOERTER, R.choice([1, 2, 2, 3]))
            titel = " ".join(x.capitalize() for x in w); k = schluessel(titel)
            if len(k) < 6 or k in VERBOTEN or any(v in k or k in v for v in VERBOTEN if len(v) >= 5): continue
            ok = True
            for o in keys:
                p = 0
                while p < min(len(o), len(k)) and o[p] == k[p]: p += 1
                if o in k or k in o or p >= 8: ok = False; break
            if ok: keys.append(k); return titel, k
        return None, None
    ids = set()
    def neue_id(praefix):
        while True:
            i = "%s%05d" % (praefix, R.randint(80000, 99999))
            if i not in ids: ids.add(i); return i
    npwr_benutzt = set()
    def neue_npwr():
        while True:
            n = "NPWR%05d_00" % R.randint(40000, 99999)
            if n not in npwr_benutzt: npwr_benutzt.add(n); return n
    apps = []
    # NEU 1.0.7 (da_Chris86 02.11.2026 23:05): andere Kennungen, die ein Spiel in SEINEN Angaben nennt (serviceIdForSharing, INSTALL_DIR_SAVEDATA,
    # SERVICE_ID_ADDCONT_ADD_n) - Spielstände/App-Daten darunter gehören zu ihm; eine Kennung, die zwei Spiele nennen, ist widersprüchlich
    verkn = []                      # (Kennung, Besitzer-Kennung, Titel)
    gemeinsam = neue_id(R.choice(["PPSA", "CUSA"])) if R.random() < 0.2 else None
    gemeinsam_bei = []
    def spst(kennung):
        sd = "savedata" if kennung.startswith("CUSA") else "savedata_prospero"
        p = "/user/home/%s/%s/%s" % (uid, sd, kennung); datei(p + "/sdimg"); return p, sd
    for _ in range(R.randint(2, 12)):
        art = R.choice(["ps5", "ps4", "medien", "homebrew", "homebrew", "tool"])
        titel, k = neuer_name()
        if not titel: continue
        if art == "ps4": i = neue_id("CUSA")
        elif art == "ps5" or art == "medien": i = neue_id("PPSA")
        else: i = neue_id(R.choice(["HBRW", "QZLA", "TOOL", "XAPP", "MNOP"]))
        if art == "ps4":
            werte = {"CATEGORY": "gd", "TITLE": titel}
            if R.random() < 0.3: l = neue_id("CUSA"); werte["INSTALL_DIR_SAVEDATA"] = l; verkn.append((l, i, titel, "spst"))
            if R.random() < 0.3: l = neue_id(R.choice(["CUSA", "PPSA"])); werte["SERVICE_ID_ADDCONT_ADD_1"] = "EP1004-%s_00" % l; verkn.append((l, i, titel, "dienst"))
            elif R.random() < 0.3: werte["SERVICE_ID_ADDCONT_ADD_1"] = R.choice(["", " " * 19, "  "])   # unbenutzt (auch Leerzeichen-Platzhalter)
            if R.random() < 0.15:
                nk = R.randint(1, 6)
                for x in range(nk): werte["SERVICE_ID_ADDCONT_ADD_%d" % (x + 2)] = R.choice(["\r\n  ", "\t", "EP1004-ABC", "CUSA12345", "EP1004-CUSA12345_00\n"])
                erw["sid"][i] = nk
            datei("/user/appmeta/%s/param.sfo" % i, sfo(werte))
        else:
            cat = 65536 if art == "medien" else 0
            dl = ',"deeplinkUri":"http://127.0.0.1:%d"' % R.randint(12000, 13000) if art == "tool" else ""
            teilen = []
            if art in ("ps5", "medien") and R.random() < 0.35:
                for _ in range(R.randint(1, 2)):
                    l = neue_id(R.choice(["CUSA", "PPSA"])); verkn.append((l, i, titel, "dienst"))
                    teilen.append(('"\\u0045P1004-%s_00"' if R.random() < 0.2 else '"EP1004-%s_00"') % l)   # Codex 42 (5): JSON-Escape
            if art in ("ps5", "medien") and gemeinsam and len(gemeinsam_bei) < 2 and R.random() < 0.6: teilen.append('"EP1004-%s_00"' % gemeinsam); gemeinsam_bei.append((i, titel))
            if art in ("ps5", "medien") and R.random() < 0.1: teilen.append('"\\r\\n  "'); erw["sid"][i] = 1
            if teilen and R.random() < 0.5: teilen.append('"EP1004-%s_00"' % i)   # die eigene Kennung zählt nicht
            if art in ("ps5", "medien") and R.random() < 0.2: teilen += ['"%s"' % (" " * 19)] * R.randint(1, 7)   # Platzhalter wie GTA DE: unbenutzt
            ad = ',"addcont":{"serviceIdForSharing":[%s]}' % ",".join(teilen) if teilen else ""
            datei("/user/appmeta/%s/param.json" % i, '{"titleId":"%s","applicationCategoryType":%d%s%s,"localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"%s"}}}' % (i, cat, dl, ad, titel))
        datei("/user/app/%s/eboot.bin" % i); ordner("/user/app/%s/sce_sys" % i); ordner("/system_data/priv/appmeta/%s" % i)
        bereich = 6 if art in ("medien", "tool") else 1
        erw["koepfe"][i] = bereich
        apps.append((art, i, titel, k, bereich))
    for art, i, titel, k, bereich in apps:
        def kind(p, wie): erw["kinder"].append((p, i, wie))
        if art in ("homebrew", "tool", "medien") or R.random() < 0.2:
            for v in R.sample(["exakt", "titel", "v2", "games", "import", "camel"], R.randint(0, 3)):
                woerter = titel.split(" ")
                n = {"exakt": k, "titel": "-".join(woerter), "v2": k + "-v2", "games": k + "-games", "import": k + "_import",
                     "camel": "".join(w.capitalize() for w in woerter)}[v]
                datei("/user/data/%s/einstellungen.json" % n, "{}"); kind("/user/data/%s" % n, "daten " + v)
        if art == "homebrew" and R.random() < 0.5:
            datei("/user/data/homebrew/%s/eboot.bin" % i); kind("/user/data/homebrew/%s" % i, "homebrew/<Kennung>")
        if m2 and R.random() < 0.3:
            datei("/mnt/ext1/%s/%s/a.dat" % (k, R.choice(["games", "cache", "save"]))); kind("/mnt/ext1/%s" % k, "M.2-Ordner")
        if stick and R.random() < 0.3:
            datei("/mnt/usb0/%s/b.dat" % k); kind("/mnt/usb0/%s" % k, "Stick-Ordner")
        if stick and art == "tool" and R.random() < 0.6:
            n = "%s_v%d.%d.elf" % (k, R.randint(0, 3), R.randint(0, 9)); datei("/mnt/usb0/" + n, b'\x7fELF' + b'\0' * 70); kind("/mnt/usb0/" + n, "ELF auf dem Stick")
        if stick and art in ("ps4", "ps5") and R.random() < 0.4:
            n = "%s - %s - v1.00 - EU.pkg" % (titel, i); datei("/mnt/usb0/" + n); kind("/mnt/usb0/" + n, "PKG")
            if R.random() < 0.5:
                n = "%s - %s - v1.%02d - EU - Update.pkg" % (titel, i, R.randint(1, 30)); datei("/mnt/usb0/" + n); kind("/mnt/usb0/" + n, "Update-PKG")
        # Trophäen
        if art in ("ps4", "ps5") and R.random() < 0.6:
            anz = R.choice([1, 1, 2, 3]) if art == "ps4" else R.choice([1, 1, 1, 2])
            listen = [neue_npwr() for _ in range(anz)]
            nb = b''.join(b'\x01' + n.encode() + b'\x00' for n in listen)
            if art == "ps4": datei("/system_data/priv/appmeta/%s/npbind.dat" % i, b'xx' + nb)
            else: datei("/system_data/priv/appmeta/%s/trophy2/npbind.dat" % i, b'xx' + nb)
            konf = "/user/trophy/conf" if art == "ps4" else "/user/trophy2/nobackup/conf"
            fort = "/user/home/%s/trophy/data" % uid if art == "ps4" else "/user/home/%s/trophy2/nobackup/data" % uid
            for n in listen: datei("%s/%s/%s" % (konf, n, "TROPHY.TRP" if art == "ps4" else "TROPHY.UCP"))
            mitf = [n for n in listen if R.random() < 0.6]
            for n in mitf: datei("%s/%s/TRPTITLE.DAT" % (fort, n))
            fpf = ["%s/%s" % (fort, n) for n in mitf]
            if uid2 and len(listen) > 1 and R.random() < 0.7:   # zweites Konto: nur eine spätere Liste gespielt
                fort2 = fort.replace("/user/home/%s/" % uid, "/user/home/%s/" % uid2)
                n2 = R.choice(listen[1:]); datei("%s/%s/TRPTITLE.DAT" % (fort2, n2)); fpf.append("%s/%s" % (fort2, n2))
            erw["trophaeen"].append((i, ["%s/%s" % (konf, n) for n in listen], fpf, [n in mitf for n in listen]))
        # Aufnahmen
        if art in ("ps4", "ps5") and R.random() < 0.4:
            nb = R.randint(1, 4); nv = R.randint(0, 2); sdr = R.random() < 0.5
            for x in range(nb): datei("/user/av_contents/photo/NPXS40087/%s/%03x/bild%d.jpg" % (i, x, x))
            for x in range(nv): datei("/user/av_contents/video/NPXS40087/%s/%03x/clip%d.mp4" % (i, x, x))
            if sdr:
                for x in range(nb): datei("/user/av_contents/sdr/photo/NPXS40087/%s/%03x/bild%d.jpg" % (i, x, x))
            erw["aufnahmen"].append((i, nb, nv, sdr))
    # NEU 1.0.7: Spielstände und App-Daten verknüpfter Kennungen tragen den Namen ihres Spiels; ähnliche Kennungen nie
    for l, besitzer, titel, typ in verkn:
        p, sd = spst(l); erw["namen"].append((p, "(%s)" % titel, "unbekannt"))
        if typ == "dienst":   # dasselbe Spiel unter anderer Kennung: Reste werden gebraucht - nie angeboten (Codex 42 (1))
            if R.random() < 0.4: datei("/user/download/%s/download0.dat" % l); erw["geschuetzt"].append("/user/download/%s" % l)
            if R.random() < 0.3: datei("/user/patch/%s/patch.pkg" % l); erw["geschuetzt"].append("/user/patch/%s" % l)
        nah = l[:4] + "%05d" % ((int(l[4:]) + 1) % 100000)
        if nah not in ids and R.random() < 0.3: ids.add(nah); q, _ = spst(nah); erw["namen"].append((q, "(Spiel/App unbekannt)", titel))
    if gemeinsam and gemeinsam_bei:
        p, _ = spst(gemeinsam)
        if len(gemeinsam_bei) == 1: erw["namen"].append((p, "(%s)" % gemeinsam_bei[0][1], "unbekannt"))
        else: erw["namen"].append((p, "(Spiel/App unbekannt)", gemeinsam_bei[0][1]))
    # NEU 1.0.7: Spielstände deinstallierter Spiele - Name nur, wenn ihre Beschreibung (MAINTITLE) ihn eindeutig belegt
    for _ in range(R.randint(0, 3)):
        l = neue_id(R.choice(["CUSA", "PPSA"])); p, sd = spst(l); fall = R.choice(["name", "name", "verschieden", "nichts"])
        meta = "/user/home/%s/%s_meta/user/%s" % (uid, sd, l)
        if fall == "name":
            t, _ = neuer_name()
            if not t: continue
            for x in range(R.randint(1, 3)): datei("%s/%sSAVE%d.sfo" % (meta, R.choice(["", "sce_bu_"]), x), sfo({"MAINTITLE": t, "TITLE_ID": l}))
            erw["namen"].append((p, "(%s, nicht mehr auf der Konsole)" % t, "unbekannt"))
        elif fall == "verschieden":
            datei("%s/A.sfo" % meta, sfo({"MAINTITLE": "Kapitel 1"})); datei("%s/B.sfo" % meta, sfo({"MAINTITLE": "Kapitel 2"}))
            erw["namen"].append((p, "(Spiel/App unbekannt)", "Kapitel"))
        else: erw["namen"].append((p, "(Spiel/App unbekannt)", "nicht mehr"))
    # Geschütztes: Sony-App, Jailbreak-Einstieg (YouTube/Y2JB)
    erw["geschuetzt"] = erw.get("geschuetzt", []) + ["/user/app/NPXS40999", "/user/appmeta/NPXS40999"]
    datei("/user/app/NPXS40999/eboot.bin"); datei("/user/appmeta/NPXS40999/param.json", '{"titleId":"NPXS40999"}')
    erw["jailbreak"] = R.random() < 0.3
    if erw["jailbreak"]:
        datei("/user/app/PPSA01650/eboot.bin"); ordner("/user/app/PPSA01650/sce_sys"); ordner("/system_data/priv/appmeta/PPSA01650")
        datei("/user/appmeta/PPSA01650/param.json", '{"titleId":"PPSA01650","applicationCategoryType":65536,"localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"YouTube"}}}')
    # Fallen
    for _ in range(R.randint(0, 4)):
        for _ in range(50):
            w = R.choice(WOERTER) + R.choice(["box", "zeug", "kram", "werk", "teil"]) + str(R.randint(10, 99))
            if not any(schluessel(w) in k or k in schluessel(w) for k in keys): break
        datei("/user/data/%s/x.txt" % w); erw["frei"].append(("/user/data/%s" % w, 5, "unbekannter Ordner"))
    # Codex-Prüfung 40 (2): ein anderes Produkt, dessen Name mit dem einer App beginnt ("Minecraft Dungeons" bei "Minecraft") -
    # mit Leerzeichen getrennt; gehört zu keiner Kachel
    if apps and R.random() < 0.5:
        t0 = R.choice(apps)[2]; n = "%s %s" % (t0, R.choice(["Dungeons", "Legends", "Remastered", "Online"]))
        datei("/user/data/%s/x.txt" % n); erw["frei"].append(("/user/data/%s" % n, 5, "anderes Produkt mit ähnlichem Namen"))
    if apps and R.random() < 0.5:
        k = R.choice(apps)[3][:4]
        if not any(k == x for x in keys): datei("/user/data/%s/x.txt" % k); erw["frei"].append(("/user/data/%s" % k, 5, "zu kurzer Name"))
    if stick and R.random() < 0.5:
        n = "Fremdes Spiel - CUSA%05d - v1.00 - EU.pkg" % R.randint(10000, 19999); datei("/mnt/usb0/" + n); erw["frei"].append(("/mnt/usb0/" + n, 5, "PKG ohne installiertes Spiel"))
    if stick:
        for x in range(R.choice([0, 0, 1, 2, 4])):
            n = "/mnt/usb0/ps5cleanup_db_2026-10-%02d_1200%02d_FW%s_%s.db" % (R.randint(1, 30), x, fw, R.choice(["app", "appinfo", "appinfo_geprueft"]))
            datei(n); erw["sicherungen"].append(n)
    return erw

def lese(text):
    K = {}; E = {}; P = {}
    for z in text.splitlines():
        t = z.split("\t")
        if t[0] == "K": K[t[1]] = int(t[2])
        elif t[0] == "E": E[int(t[1])] = {"kat": int(t[2]), "kather": int(t[3]), "art": int(t[4]), "nr": int(t[5]), "gesp": t[6] == "1", "kopf": int(t[7]), "kid": t[8], "zeile": t[9] if len(t) > 9 else ""}
        elif t[0] == "P": P.setdefault(int(t[1]), []).append(t[2])
    return K, E, P

def pruefe(erw, K, E, P):
    f = []
    wo = {}
    for e, ps in P.items():
        for p in ps:
            if p in wo: f.append("Pfad in zwei Einträgen: %s" % p)
            wo[p] = e
    def eintraege_fuer(p):   # Eintrag(e) mit p selbst oder Teilen darunter
        return sorted({e for q, e in wo.items() if q == p or q.startswith(p + "/")})
    for e, d in E.items():
        if not 1 <= d["kat"] <= 7: f.append("Bereich ungültig: %s" % d["zeile"])
    for p, kid, wie in erw["kinder"]:
        es = eintraege_fuer(p)
        if not es: f.append("%s fehlt: %s" % (wie, p)); continue
        for e in es:
            d = E[e]; k = d["kopf"]
            if k < 0 or E[k]["kid"] != kid: f.append("%s nicht unter %s: %s (Kopf %s)" % (wie, kid, p, E[k]["kid"] if k >= 0 else "-")); continue
            if d["kat"] != E[k]["kat"]: f.append("%s nicht im Bereich der Kachel: %s" % (wie, p))
            if d["kat"] != erw["koepfe"][kid]: f.append("%s im falschen Bereich (%d statt %d): %s" % (wie, d["kat"], erw["koepfe"][kid], p))
            if d["art"] == K["ART_UNBEKANNT"]: f.append("%s noch \"Inhalt unbekannt\": %s" % (wie, p))
    for kid, kat in erw["koepfe"].items():
        es = [e for e, d in E.items() if d["kid"] == kid and d["kopf"] < 0]
        if not es: f.append("Kachel fehlt: %s" % kid)
        elif E[es[0]]["kat"] != kat: f.append("Kachel %s im falschen Bereich (%d statt %d)" % (kid, E[es[0]]["kat"], kat))
    for p, kat, wie in erw["frei"]:
        es = eintraege_fuer(p)
        if not es: f.append("%s fehlt: %s" % (wie, p)); continue
        for e in es:
            if E[e]["kopf"] >= 0: f.append("%s wurde einer Kachel zugeordnet: %s -> %s" % (wie, p, E[E[e]["kopf"]]["kid"]))
            if E[e]["kat"] != kat: f.append("%s im Bereich %d statt %d: %s" % (wie, E[e]["kat"], kat, p))
    for kid, listen, fort, _ in erw["trophaeen"]:
        es = [wo.get(p) for p in listen]
        if None in es: f.append("Trophäen-Liste fehlt (%s): %s" % (kid, listen)); continue
        if len(listen) == 1:
            if E[es[0]]["nr"] != 0 or E[es[0]]["kopf"] >= 0: f.append("einzige Liste mit Nummer oder unter einem Kopf: %s" % listen[0])
        else:
            for n, e in enumerate(es, 1):
                if E[e]["nr"] != n: f.append("Liste %d hat Nummer %d: %s" % (n, E[e]["nr"], listen[n - 1]))
                if n == 1 and E[e]["kopf"] >= 0: f.append("erste Liste steht unter einem Kopf: %s" % listen[0])
                if n > 1 and E[e]["kopf"] != es[0]: f.append("Liste %d nicht unter Liste 1: %s" % (n, listen[n - 1]))
                if not E[e]["zeile"].split(" | ")[0].rstrip().endswith("Liste %d" % n) and "Liste %d" % n not in E[e]["zeile"]: f.append("Zeile nennt die Nummer nicht: %s" % E[e]["zeile"])
            for e in es:
                if E[e]["kat"] != 3: f.append("Trophäen-Liste nicht unter Trophäen: %s" % E[e]["zeile"])
        for p in fort:
            if p not in wo: f.append("Trophäen-Fortschritt fehlt: %s" % p)
    for kid, nb, nv, sdr in erw["aufnahmen"]:
        p = "/user/av_contents/photo/NPXS40087/%s" % kid
        e = wo.get(p)
        if e is None: f.append("Aufnahmen fehlen: %s" % kid); continue
        if sdr and wo.get("/user/av_contents/sdr/photo/NPXS40087/%s" % kid) != e: f.append("zweite Fassung (sdr) nicht bei den Aufnahmen: %s" % kid)
        z = E[e]["zeile"]
        if ("%d Screenshot" % nb) not in z or ("%d Screenshot" % (2 * nb) in z and sdr): f.append("Aufnahmen falsch gezählt (%d erwartet): %s" % (nb, z))
        if E[e]["art"] != K["ART_AUFNAHMEN"]: f.append("Aufnahmen mit falscher Art: %s" % z)
    if erw["sicherungen"]:
        es = {wo.get(p) for p in erw["sicherungen"]}
        if None in es or len(es) != 1: f.append("alte Sicherungen nicht in EINEM Eintrag: %s" % erw["sicherungen"])
        else:
            d = E[es.pop()]
            if d["kat"] != 6 or d["art"] != K["ART_ALTE_SICHERUNG"] or "PS5 Cleanup – alte Sicherungen (%d " % len(erw["sicherungen"]) not in d["zeile"]: f.append("alte Sicherungen falsch: %s" % d["zeile"])
    for g in erw["geschuetzt"]:
        if eintraege_fuer(g) or any(g.startswith(q + "/") for q in wo): f.append("Geschütztes angeboten: %s" % g)
    for e, d in E.items():
        if d["kopf"] >= 0 and E[d["kopf"]]["kat"] == 7: f.append("Teil unter einem Jailbreak-Eintrag: %s" % d["zeile"])
        if d["kopf"] >= 0:
            konto = {q.split("/")[3] for q in P.get(e, []) if q.startswith("/user/home/")}
            konto_k = {q.split("/")[3] for q in P.get(d["kopf"], []) if q.startswith("/user/home/")}
            if konto and konto_k and konto != konto_k: f.append("Teil eines Kontos unter dem eines anderen: %s" % d["zeile"])
    # NEU 1.0.7: Namen nur aus Metadaten, sonst ausdrücklich "unbekannt"
    for p, soll, nicht in erw["namen"]:
        e = wo.get(p)
        if e is None: f.append("Spielstand fehlt: %s" % p); continue
        tb = E[e]["zeile"].split(" | ", 1)[1] if " | " in E[e]["zeile"] else E[e]["zeile"]
        if soll not in tb: f.append("Name fehlt (%s erwartet): %s" % (soll, tb))
        if nicht and nicht in tb: f.append("geraten oder falsch (%s): %s" % (nicht, tb))
    # NEU 1.0.7: kaputte Zusatzinhalt-Angaben nur und genau beim betroffenen Spiel
    for kid in erw["koepfe"]:
        es = [e for e, d in E.items() if d["kid"] == kid and d["kopf"] < 0]
        if not es: continue
        hat = "ACHTUNG: kaputte Zusatzinhalt-Angaben" in E[es[0]]["zeile"]
        if hat != (kid in erw["sid"]): f.append("Warnung %s bei %s: %s" % ("fehlt" if not hat else "falsch", kid, E[es[0]]["zeile"]))
    for e, d in E.items():
        if "ACHTUNG: kaputte Zusatzinhalt-Angaben" in d["zeile"] and d["kid"] not in erw["sid"]: f.append("Warnung bei einem anderen Eintrag: %s" % d["zeile"])
    if erw["jailbreak"]:
        es = eintraege_fuer("/user/app/PPSA01650")
        if not es or any(E[e]["kat"] != 7 for e in es): f.append("YouTube (Jailbreak-Einstieg) nicht unter Jailbreak")
    for e, d in E.items():
        if d["kat"] == 5:
            for p in P.get(e, []):
                for q, kid, wie in erw["kinder"]:
                    if p == q or p.startswith(q + "/"): f.append("unter Sonstiges, obwohl es zu %s gehört: %s" % (kid, p))
    return f

def main():
    prog, arbeit, anzahl = sys.argv[1], sys.argv[2], int(sys.argv[3]); start = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    fehler = 0; konsolen = 0; teile = 0; fw_zahl = {}
    for seed in range(start, start + anzahl):
        R = random.Random(seed)
        W = os.path.join(arbeit, "w"); shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
        erw = erzeuge(W, R)
        fw_zahl[erw["fw"]] = fw_zahl.get(erw["fw"], 0) + 1
        r = subprocess.run([prog, W], capture_output=True, text=True, cwd=arbeit)
        konsolen += 1; teile += len(erw["kinder"]) + len(erw["frei"])
        if r.returncode != 0:
            fehler += 1; print("FAIL Konsole %d (FW %s): Suche abgebrochen: %s" % (seed, erw["fw"], (r.stdout + r.stderr).strip()[:300])); continue
        K, E, P = lese(r.stdout)
        fl = pruefe(erw, K, E, P)
        if fl:
            fehler += 1
            print("FAIL Konsole %d (FW %s): %d Abweichung(en)" % (seed, erw["fw"], len(fl)))
            for x in fl[:8]: print("       - " + x)
    # Codex-Prüfung 40 (6): Zusammenfassung nur "OK", wenn wirklich alles stimmt - und Rückgabewert für die Läufe
    print("%s %d erfundene Konsolen durchsucht, %d davon mit Abweichungen, %d zugeordnete oder freie Teile geprüft" % ("OK  " if fehler == 0 else "FAIL", konsolen, fehler, teile))
    print("INFO Firmwares: " + ", ".join("%s x%d" % (k, fw_zahl[k]) for k in sorted(fw_zahl, key=float)))
    print("FEHLERZAHL %d" % fehler)
    return 1 if fehler else 0

sys.exit(main())
