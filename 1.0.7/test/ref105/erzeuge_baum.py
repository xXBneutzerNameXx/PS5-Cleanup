# PS5 Cleanup 1.1 - erzeugt zusätzliche, große Test-PS5-Bäume (gen1, gen2) mit vielen Sonderfällen,
# damit Suche/Auswahl/Löschen der C-Version über die vorhandenen Testbäume hinaus gegen 1.0.5
# verglichen werden können. Gleicher Startwert -> immer derselbe Baum.
import json, os, random, struct, sys

def sfo(werte):
    """param.sfo (PS4-Format) schreiben: werte = {schluessel: str|int}"""
    keys = sorted(werte)
    keytab = b''; daten = b''; eintraege = []
    for k in keys:
        v = werte[k]
        koff = len(keytab); keytab += k.encode('ascii') + b'\0'
        if isinstance(v, int):
            d = struct.pack('<I', v); fmt = 0x0404; ln = 4; mx = 4
        else:
            d = v.encode('utf-8') + b'\0'; fmt = 0x0204; ln = len(d); mx = (len(d) + 3) // 4 * 4; d = d.ljust(mx, b'\0')
        eintraege.append((koff, fmt, ln, mx, len(daten))); daten += d
    while len(keytab) % 4: keytab += b'\0'
    kt = 20 + 16 * len(keys); dt = kt + len(keytab)
    kopf = b'\0PSF' + struct.pack('<IIII', 0x101, kt, dt, len(keys))
    tab = b''.join(struct.pack('<HHIII', a, b, c, d, e) for a, b, c, d, e in eintraege)
    return kopf + tab + keytab + daten

def main(ziel, seed):
    R = random.Random(seed)
    def datei(p, inhalt=b''):
        full = os.path.join(ziel, p.lstrip('/'))
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, 'wb') as f: f.write(inhalt if isinstance(inhalt, bytes) else inhalt.encode('utf-8'))
    def ordner(p): os.makedirs(os.path.join(ziel, p.lstrip('/')), exist_ok=True)
    titel = ['Weltraum Rennen', 'Ünïcödé Ärger', 'Café Crème', '東京ドリフト', 'Space  Doppel  Leer', 'Ω Omega', 'Bärenhöhle 2',
             'Ninja\'s "Zitat"', 'Größter Spaß™', 'Ελληνικά', 'Русская игра', 'a', 'Emoji 🎮 Spiel', 'Tab\tim Namen', 'Schräg/Strich']
    def pj(name, art):
        lp = {}
        if art == 'normal': lp = {'defaultLanguage': 'en-US', 'de-DE': {'titleName': name + ' DE'}, 'en-US': {'titleName': name}}
        elif art == 'nurfr': lp = {'defaultLanguage': 'fr-FR', 'fr-FR': {'titleName': name + ' FR'}}
        elif art == 'grossklein': lp = {'DefaultLanguage': 'en-US', 'EN-us': {'TitleName': name + ' ci'}}
        elif art == 'zahl': lp = {'defaultLanguage': 'en-US', 'en-US': {'titleName': 12345}}
        elif art == 'leer': lp = {'defaultLanguage': 'en-US', 'en-US': {'titleName': ''}}
        t = json.dumps({'titleId': 'X', 'localizedParameters': lp, 'contentVersion': '01.0%d' % R.randint(0, 9)}, ensure_ascii=R.random() < 0.5)
        if art == 'bom': t = '﻿' + json.dumps({'localizedParameters': {'defaultLanguage': 'de-DE', 'de-DE': {'titleName': name + ' BOM'}}}, ensure_ascii=False)
        if art == 'kaputt': t = '{"localizedParameters": {"en-US": {"titleName": "' + name + ' kaputt",}}, }'
        if art == 'doppelt': t = '{"localizedParameters": {"defaultLanguage":"en-US","en-US": {"titleName": "erst"}, "en-US": {"titleName": "' + name + ' zweit"}}}'
        if art == 'muell': t = json.dumps({'localizedParameters': {'en-US': {'titleName': name + ' muell'}}}) + ' nachspann'
        if art == 'tool': t = json.dumps({'localizedParameters': {'en-US': {'titleName': name + ' Tool'}}, 'deeplinkUri': 'http://127.0.0.1:8080/x'})
        if art == 'astro': t = json.dumps({'localizedParameters': {'en-US': {'titleName': "ASTRO's PLAYROOM"}}})
        return t
    arten = ['normal', 'normal', 'normal', 'nurfr', 'grossklein', 'zahl', 'leer', 'bom', 'kaputt', 'doppelt', 'muell', 'tool', 'normal']
    homes = ['1a2b3c4d', 'u2']
    for h in homes:
        ordner('/user/home/%s/savedata' % h); ordner('/user/home/%s/savedata_prospero' % h)
    ordner('/user/appmeta/addcont'); ordner('/user/system/share'); ordner('/user/license')
    ordner('/user/patch'); ordner('/user/addcont'); ordner('/system_data/priv/appmeta/external')
    ordner('/user/data'); ordner('/mnt/ext1/user/app'); ordner('/mnt/ext1/user/patch'); ordner('/mnt/ext1/user/addcont')
    ids = []
    for i in range(36):
        ps4 = i % 5 == 0
        tid = ('CUSA%05d' if ps4 else 'PPSA%05d') % (10000 + i * 37)
        ids.append(tid)
        name = R.choice(titel) + ' %d' % i
        art = R.choice(arten) if i != 7 else 'astro'
        ort = R.random()
        if i % 9 == 4:      # nur auf der M.2
            basis = '/mnt/ext1/user'
        else:
            basis = '/user'
        if ps4:
            datei(basis + '/app/%s/sce_sys/param.sfo' % tid, sfo({'TITLE': name + ' PS4', 'APP_VER': '01.%02d' % i, 'ATTRIBUTE': i}))
        else:
            datei(basis + '/app/%s/sce_sys/param.json' % tid, pj(name, art))
        datei(basis + '/app/%s/eboot.bin' % tid, bytes(R.getrandbits(8) for _ in range(R.randint(0, 3000))))
        if i % 3 == 0: datei('/user/appmeta/%s/param.json' % tid, pj(name + ' meta', 'normal'))
        if i % 4 == 0: datei('/system_data/priv/appmeta/%s/param.json' % tid, pj(name + ' sys', 'normal'))
        if i % 4 == 1: datei('/system_data/priv/appmeta/%s/trophy2/npbind.dat' % tid, b'\x00\x01NPWR%05d_00\x00' % (30000 + i))
        if i % 6 == 2:      # Update
            if ps4: datei(basis + '/patch/%s/sce_sys/param.sfo' % tid, sfo({'APP_VER': '02.%02d' % i}))
            else: datei(basis + '/patch/%s/sce_sys/param.json' % tid, json.dumps({'contentVersion': '02.0%d0' % (i % 10)}))
        if i % 7 == 3:      # DLCs
            for d in range(R.randint(1, 3)):
                dn = 'DLC%04d' % d
                datei(basis + '/addcont/%s/%s/sce_sys/param.json' % (tid, dn), pj('Erweiterung %d' % d, R.choice(['normal', 'leer', 'kaputt'])))
                if d % 2 == 0: datei('/user/appmeta/addcont/%s/%s/param.json' % (tid, dn), pj('Erw meta %d' % d, 'normal'))
        if i % 5 == 1: datei('/user/system/share/%s/x.dat' % tid, b'x')
        if i % 8 == 5: datei('/user/license/fake%s-%s.rif' % ('EP0000', tid), b'r'); datei('/user/license/fake%s-%s.idx' % ('EP0000', tid), b'i')
        h = R.choice(homes)
        if i % 2 == 0: datei('/user/home/%s/savedata_prospero/%s/sdimage' % (h, tid), b'save')
        if i % 3 == 1: datei('/user/home/%s/savedata/%s/sdimage' % (h, tid), b'save')
    # Zwilling: gleiches Spiel intern und auf der M.2
    tw = ids[2]
    datei('/mnt/ext1/user/app/%s/sce_sys/param.json' % tw, pj('Zwilling', 'normal'))
    datei('/mnt/ext1/user/patch/%s/sce_sys/param.json' % tw, json.dumps({'contentVersion': '03.000'}))
    # ShadowMount-Spiele
    sm = [('PPSA90001', '/data/homebrew/Spiel Eins - PPSA90001.exfat', True), ('PPSA90002', '/mnt/ext1/homebrew/Zwei - PPSA90002.exfat', True),
          ('PPSA90003', '/data/homebrew/weg.exfat', False), ('PPSA90004', '/somewhere/else.exfat', False), ('PPSA90005', '', False),
          ('PPSA90006', '/data/homebrew/sub/tief/Sechs - PPSA90006.ffpkg', True), ('PPSA90007', '/mnt/ext1/user/app/x.exfat', False)]
    for tid, q, da in sm:
        datei('/user/app/%s/sce_sys/param.json' % tid, pj('SM ' + tid, 'normal'))
        datei('/user/app/%s/%s' % (tid, 'mount_img.lnk' if tid[-1] in '1357' else 'mount.lnk'), ('/mnt/shadowmnt/%s\n%s\n' % (tid, q)) if q else '\n\n')
        datei('/user/appmeta/%s/param.json' % tid, pj('SM ' + tid, 'normal'))
        if da:
            p = q.replace('/data/', '/user/data/', 1) if q.startswith('/data/') else q
            datei(p, bytes(R.getrandbits(8) for _ in range(1000)))
    datei('/user/data/homebrew/.Spiel Eins - PPSA90001.exfat.gc-compress.tmp', b'tmp')
    datei('/user/data/homebrew/anderes.bin', b'x' * 500)
    datei('/user/data/homebrew/sub/tief/nachbar.txt', b'n')
    datei('/user/data/ps5upload/tile/PSUP00001/x', b'x'); datei('/user/data/etaHEN/config.ini', b'c')
    datei('/user/data/shadowmount/debug.log', b'[x] start\n')
    # Trophäen
    for h in homes:
        datei('/user/home/%s/trophy/data/sce_trop/trpsummary.dat' % h, b't')
        for n in (30001, 30005, 39999):
            datei('/user/home/%s/trophy2/nobackup/data/NPWR%05d_00/trophy.img' % (h, n), b't')
        datei('/user/home/%s/trophy/data/NPWR%05d_00/x' % (h, 31000), b't')
        datei('/user/home/%s/trophy2/nobackup/other.dat' % h, b'o')
        datei('/user/home/%s/trophy2/conf.dat' % h, b'o')
    for n in (30001, 30005, 30009, 39999, 20188, 31000):
        datei('/user/trophy2/nobackup/conf/NPWR%05d_00/TROPCONF.SFM' % n, b'c')
        if n % 2: datei('/user/np_uds/nobackup/conf/NPWR%05d_00/x' % n, b'c')
    datei('/user/trophy/conf/npwr31000_00/x', b'c')   # klein geschrieben
    datei('/user/home/1a2b3c4d/webkit/shell/local/x.db', b'w')
    ordner('/user/home/u2/webkit/shell/local')
    # Galerie
    for g, (app, anz) in enumerate([('NPXS40087', 3), ('NPXS40087', 0), ('NPXS40000', 2)]):
        for gid in (ids[0], ids[4], 'PPSA99001', 'CUSA77777', 'NPXS40140'):
            for k in range(anz):
                datei('/user/av_contents/photo/%s/%s/%03x/bild%d.jpg' % (app, gid, k, k), b'j')
                if gid == 'PPSA99001':
                    datei('/user/av_contents/photo/%s/%s/%03x/bild%d.dat' % (app, gid, k, k), b'\x01\x02\x03\x04\x05\x06\x07\x08\x03Galerie Spiel Neun_20261001123456\x00rest')
                    datei('/user/av_contents/photo/%s/%s/%03x/bild%d.ext' % (app, gid, k, k), b'xx NPWR39999_00 yy')
            if anz == 0: ordner('/user/av_contents/video/%s/%s' % (app, gid))
    datei('/user/av_contents/video/NPXS40087/%s/1aa/clip.mp4' % ids[0], b'v')
    datei('/user/av_contents/video/NPXS40087/%s/1aa/clip.webm' % ids[0], b'v')
    datei('/user/av_contents/thumbnails/photo/NPXS40087/%s/03b/t.jpg' % ids[0], b't')
    datei('/user/av_contents/thumbnails/video/NPXS40087/PPSA55500/1cd/t.jpg', b't')
    datei('/user/av_contents/content_tmp/%s/a.tmp' % ids[1], b't'); ordner('/user/av_contents/content_tmp/PPSA12000')
    datei('/user/av_contents/extra/%s/x' % ids[3], b'e'); datei('/user/av_contents/sdr/abc', b'e')
    # Reste
    datei('/user/download/%s/x.pkg' % ids[5], b'd'); datei('/user/download/NPXS40140/x', b'd'); datei('/user/download/PPSA44400/y', b'd')
    datei('/user/patch/PPSA44401/sce_sys/param.json', '{}'); datei('/user/addcont/PPSA44402/DLC/x', b'a')
    datei('/user/appmeta/external/addcont/PPSA44403/x', b'a')
    datei('/user/_zu_loeschen_7/x', b'z'); datei('/mnt/ext1/_zu_loeschen_2/x', b'z')
    datei('/user/appmeta/PPSA44404/param.json', pj('Rest Spiel', 'normal')); datei('/system_data/priv/appmeta/PPSA44404/x', b'k')
    datei('/user/appmeta/PPSA44405/x', b'k')
    datei('/system_data/priv/appmeta/PPSA44406/param.json', pj('Schloss', 'normal')); ordner('/system_data/priv/appmeta/NPXS40001')
    datei('/system_data/priv/appmeta/external/CUSA44407/x', b'u')
    datei('/user/catalog_downloader/conceptmeta/123/x', b'c'); datei('/user/catalog_downloader/appmeta/456/x', b'c')
    datei('/user/bgft/trash/x', b't'); datei('/user/temp/gls/screenshot/s.png', b't'); ordner('/user/savedata/temp'); datei('/user/temp/common_temp/telemetry/t', b't')
    datei('/system_data/priv/error/history/e1.json', b'{}'); datei('/system_data/priv/error/history/e2.txt', b'x')
    datei('/user/appmeta/addcont/PPSA44408/D/x', b'a')
    datei('/user/license/act.dat', b'a'); datei('/user/license/fakeEP0000-PPSA44409.rif', b'r'); datei('/user/license/fakeUP9999-XYZ.idx', b'r')
    # M.2-Ordner
    datei('/mnt/ext1/games/a.iso', bytes(R.getrandbits(8) for _ in range(100))); datei('/mnt/ext1/games/sub/b.iso', b'b'); ordner('/mnt/ext1/leer')
    datei('/mnt/ext1/homebrew/Zwei - PPSA90002.exfat', bytes(R.getrandbits(8) for _ in range(100)))
    datei('/mnt/ext1/homebrew/.Zwei - PPSA90002.exfat.gc-compress.tmp', b't')
    datei('/mnt/ext1/datei.bin', b'x'); ordner('/mnt/ext1/lost+found'); ordner('/mnt/ext1/system_data')
    print('fertig', ziel)

KENNUNG = b'PS5CLEANUP-ELF-KENNUNG:7c1e5b2a-selbstschutz'
def selbstschutz(ziel, variante):
    """Schritt 6: Homebrew Launcher und PS5 Cleanup an verschiedenen Orten dazulegen"""
    def datei(p, inhalt=b''):
        full = os.path.join(ziel, p.lstrip('/'))
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, 'wb') as f: f.write(inhalt if isinstance(inhalt, bytes) else inhalt.encode('utf-8'))
    hl = json.dumps({'titleId': 'FAKE00000', 'localizedParameters': {'defaultLanguage': 'en-US', 'en-US': {'titleName': 'Homebrew Launcher'}}})
    def launcher(basis, tid):
        datei(basis + '/app/%s/sce_sys/param.json' % tid, hl.replace('FAKE00000', tid))
        datei(basis + '/app/%s/eboot.bin' % tid, b'launcher')
        datei('/user/appmeta/%s/param.json' % tid, hl.replace('FAKE00000', tid))
        datei('/system_data/priv/appmeta/%s/param.json' % tid, hl.replace('FAKE00000', tid))
        datei('/user/license/fakeEP0000-%s.rif' % tid, b'r')
    elf = b'\x7fELF' + b'\0' * 64 + KENNUNG + b'\0' * 64
    fremd = b'\x7fELF' + b'\0' * 64 + b'irgendein anderes Programm' + b'\0' * 64
    if variante == 'v1':        # Launcher auf der M.2 (wie bei da_Chris86), PS5 Cleanup intern, Zweitkopie umbenannt auf der M.2
        launcher('/mnt/ext1/user', 'FAKE00000')
        datei('/user/data/homebrew/PS5Cleanup/eboot.elf', elf); datei('/user/data/homebrew/PS5Cleanup/sce_sys/icon0.png', b'png')
        datei('/mnt/ext1/homebrew/MeinCleaner/eboot.elf', elf)
    elif variante == 'v2':      # Launcher intern
        launcher('/user', 'FAKE00000')
        datei('/user/data/homebrew/PS5Cleanup/eboot.elf', elf); datei('/user/data/homebrew/PS5Cleanup/sce_sys/icon0.png', b'png')
    elif variante == 'v3':      # Launcher auf USB-Erweiterung + zweiter Launcher mit anderer Kennung intern, PS5 Cleanup nur auf der M.2
        launcher('/mnt/ext0/user', 'FAKE00000')
        launcher('/user', 'HBLD00001')
        datei('/mnt/ext1/homebrew/PS5Cleanup/eboot.elf', elf)
    elif variante == 'v4':      # PS5 Cleanup nicht auffindbar (keine Kennung) -> unsicher
        launcher('/user', 'FAKE00000')
        datei('/user/data/homebrew/PS5Cleanup/eboot.elf', fremd)
    datei('/user/data/homebrew/AndereApp/eboot.elf', fremd); datei('/user/data/homebrew/AndereApp/sce_sys/icon0.png', b'png')
    datei('/user/data/PS5Cleanup/einstellungen.txt', b'sprache=de')
    print('selbstschutz', variante)

if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]))
    if len(sys.argv) > 3: selbstschutz(sys.argv[1], sys.argv[3])
