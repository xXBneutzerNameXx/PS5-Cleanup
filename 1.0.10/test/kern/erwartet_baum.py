# Erwartete Baumliste "Typ Größe Pfad" (wie miniftp4.py den Baum zeigt) - unabhängig vom C-Kern
import os, sys, stat
root = sys.argv[1]
fake = set(x.strip('/') for x in os.environ.get('FAKELINKS', '').split(',') if x.strip())
out = []
def walk(rel):
    full = os.path.join(root, rel) if rel else root
    for n in sorted(os.listdir(full)):
        if any(ord(c) < 32 or ord(c) == 127 for c in n): continue
        r = (rel + '/' + n) if rel else n
        st = os.lstat(os.path.join(root, r))
        t = 'l' if (stat.S_ISLNK(st.st_mode) or r in fake) else 'd' if stat.S_ISDIR(st.st_mode) else 'f'
        size = 0 if stat.S_ISDIR(st.st_mode) else st.st_size
        out.append(f"{t} {size} {r}")
        if t == 'd': walk(r)
walk('')
print('\n'.join(sorted(out)))
