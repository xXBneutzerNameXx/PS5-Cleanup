# Schritt 6: Endzustand mit Selbstschutz gegen Endzustand ohne Selbstschutz (= Verhalten 1.0.5) prüfen.
#   python3 pruefe_schutz.py <variante> <baum_ohne_schutz> <baum_mit_schutz> <schutzliste>
# Regeln:
#  1. Was ohne Schutz stehen bleibt, bleibt auch mit Schutz stehen (der Schutz löscht nie mehr).
#  2. Was NUR mit Schutz stehen bleibt, muss geschützt sein (selbst, darin oder Elternordner davon),
#     bei "unsicher" auch alles unter user/data und jeder Ordner "homebrew".
#  3. Die erwarteten Teile von Launcher und PS5 Cleanup sind da; fremde Homebrew-App ist weg (außer unsicher).
import sys, re
var, f_off, f_on, f_schutz = sys.argv[1:5]
def baum(f):
    s = set()
    for z in open(f, encoding='utf-8', errors='surrogateescape'):
        t = z.rstrip('\n').split(' ', 2)
        if len(t) == 3 and t[2]: s.add(t[2])
    return s
off, on = baum(f_off), baum(f_on)
schutz = [z[7:].strip().lstrip('/') for z in open(f_schutz, encoding='utf-8') if z.startswith('SCHUTZ /')]
unsicher = any('unsicher=True' in z for z in open(f_schutz, encoding='utf-8'))
fehler = 0
def f(t):
    global fehler; fehler += 1; print('FAIL', t)
mehr_geloescht = off - on
if mehr_geloescht: f('mit Schutz mehr gelöscht als ohne: %s' % sorted(mehr_geloescht)[:5])
else: print('OK   der Schutz löscht nie mehr als 1.0.5')
# unsicher: ShadowMount-Spiele, deren Spieldatei in den gesperrten Homebrew-Daten liegt, werden samt
# Kachel nicht angeboten (sonst würde nur die halbe Sache gelöscht) - ihre Kachel bleibt also stehen.
sm_ids = set(re.match(r'user/app/([^/]+)/', p).group(1) for p in on if re.match(r'user/app/[^/]+/mount(_img)?\.lnk$', p))
def geschuetzt(p):
    if unsicher:
        m = re.match(r'(user/app|user/appmeta|system_data/priv/appmeta)/([^/]+)', p)
        if m and m.group(2) in sm_ids: return True
    for q in schutz:
        if p.lower() == q.lower() or p.lower().startswith(q.lower() + '/') or q.lower().startswith(p.lower() + '/'): return True
    # 1.0.6 (da_Chris86 28.10.2026): der Homebrew Launcher ist nicht mehr geschützt
    if unsicher and (p == 'user/data' or p.startswith('user/data/') or re.search(r'(?i)(^|/)homebrew(/|$)', p)): return True
    return False
nur_mit = sorted(on - off)
falsch = [p for p in nur_mit if not geschuetzt(p)]
if falsch: f('mit Schutz stehen geblieben, aber nicht geschützt: %s' % falsch[:8])
else: print('OK   nur geschützte Teile bleiben zusätzlich stehen (%d)' % len(nur_mit))
# 1.0.6 (da_Chris86 28.10.2026): nur noch PS5 Cleanup selbst ist geschützt (der Homebrew Launcher nicht mehr)
erwartet = {
 'v1': ['user/data/homebrew/PS5Cleanup/eboot.elf', 'user/data/homebrew/PS5Cleanup/sce_sys/icon0.png', 'mnt/ext1/homebrew/MeinCleaner/eboot.elf', 'user/data/PS5Cleanup/einstellungen.txt'],
 'v2': ['user/data/homebrew/PS5Cleanup/eboot.elf', 'user/data/homebrew/PS5Cleanup/sce_sys/icon0.png', 'user/data/PS5Cleanup/einstellungen.txt'],
 'v3': ['mnt/ext1/homebrew/PS5Cleanup/eboot.elf', 'user/data/PS5Cleanup/einstellungen.txt'],
 'v4': ['user/data/homebrew/PS5Cleanup/eboot.elf', 'user/data/homebrew/AndereApp/eboot.elf', 'user/data/PS5Cleanup/einstellungen.txt'],
}[var]
fehlt = [p for p in erwartet if p not in on]
if fehlt: f('geschützte Teile fehlen nach der Bereinigung: %s' % fehlt)
else: print('OK   alle %d geschützten Teile sind nach der vollständigen Bereinigung noch da' % len(erwartet))
if var != 'v4':
    if any(p.startswith('user/data/homebrew/AndereApp') for p in on): f('fremde Homebrew-App wurde nicht gelöscht')
    else: print('OK   fremde Homebrew-App (AndereApp) wurde wie bisher gelöscht')
else:
    print('OK   unsicher: Homebrew-Daten insgesamt gesperrt' if unsicher else 'FAIL unsicher nicht erkannt')
    if not unsicher: fehler += 1
print('FEHLERZAHL %d' % fehler)
