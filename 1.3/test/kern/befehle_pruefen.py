# NEU 1.1 (Schritt 5.1): die Befehle für z->befehl stehen als Konstanten in kern/zugriff.h (BEF_...). Diese Prüfung meldet jeden Befehl,
# der im Code von kern/ oder app/ noch ausgeschrieben steht ("SITE UNINSTALL", "DELE" ...).
# Codex-Prüfung 310: aneinandergehängte Zeichenketten ("SITE " "UNINSTALL") werden zusammengesetzt und C-Escapes ("\104ELE") aufgelöst;
# (Codex-Prüfung 311: vorher wie in C Backslash+Zeilenende entfernt, Kommentare vor dem Namen zählen nicht)
# auch #define-Zeilen zählen (nur die BEF_-Konstanten selbst in kern/zugriff.h nicht); Namen von Umgebungsvariablen (getenv("RMDA"),
# env(d, "RMDA")) sind keine Befehle; ein //-Kommentar mit Backslash am Zeilenende geht auf der nächsten Zeile weiter (wie in C).
# Grenze: ein Befehl, der erst zur Laufzeit aus Teilen zusammengesetzt wird, ist so nicht zu erkennen.
# Ausgabe: je Fund "datei:zeile:befehl" mit Leerzeichen getrennt; nichts = in Ordnung.
# Aufruf: python3 -I befehle_pruefen.py <Versionsordner>   |   python3 -I befehle_pruefen.py --selbsttest  (Rückgabe 0 = Prüfung arbeitet richtig)
import sys, re, glob, os
CMD = re.compile(r'^(SITE [A-Z0-9?-]+( [A-Z0-9?-]+)*|DELE|RMD|RMDA|NOOP)$')
ENV = re.compile(r'(\bgetenv|\benv)\s*\(\s*(\w+\s*,\s*)?$')

def kommentar_ende(s, i):
    if s.startswith('/*', i):
        j = s.find('*/', i + 2); return len(s) if j < 0 else j + 2
    j = s.find('\n', i); return len(s) if j < 0 else j

def vorstufe(s):
    """wie der C-Übersetzer zuerst: Backslash + Zeilenende entfernen (Zeilen zusammenfügen). Liefert den Text und je Zeichen
    die Zeile im Original (Codex-Prüfung 311: "D\\<Zeilenende>ELE" ist "DELE")"""
    t = []; zeile = []; z = 1; i = 0; n = len(s)
    while i < n:
        if s[i] == '\\' and s.startswith('\n', i + 1): i += 2; z += 1; continue
        if s[i] == '\\' and s.startswith('\r\n', i + 1): i += 3; z += 1; continue
        t.append(s[i]); zeile.append(z)
        if s[i] == '\n': z += 1
        i += 1
    return ''.join(t), zeile

def ohne_kommentare(s):
    """gleich lang, Kommentare durch Leerzeichen ersetzt (Zeichenketten bleiben) - für den Blick auf das, was davor steht"""
    o = list(s); i, n = 0, len(s)
    while i < n:
        if s.startswith('/*', i) or s.startswith('//', i):
            j = kommentar_ende(s, i)
            for k in range(i, j):
                if o[k] != '\n': o[k] = ' '
            i = j
        elif s[i] in '"\'':
            q = s[i]; j = i + 1
            while j < n and s[j] != q: j += 2 if s[j] == '\\' else 1
            i = j + 1
        else: i += 1
    return ''.join(o)

def c_text(roh):
    out = []; i = 0
    while i < len(roh):
        c = roh[i]
        if c != '\\': out.append(c); i += 1; continue
        n = roh[i + 1] if i + 1 < len(roh) else ''
        if n in '01234567':
            m = re.match(r'[0-7]{1,3}', roh[i + 1:]).group(0); out.append(chr(int(m, 8))); i += 1 + len(m)
        elif n == 'x':
            m = re.match(r'[0-9a-fA-F]+', roh[i + 2:]); m = m.group(0) if m else ''
            out.append(chr(int(m, 16)) if m else 'x'); i += 2 + len(m)
        else:
            out.append({'n': '\n', 't': '\t', 'r': '\r', '0': '\0'}.get(n, n)); i += 2
    return ''.join(out)

def pruefe_text(s, name, zh=False):
    s, zeile = vorstufe(s); maske = ohne_kommentare(s)
    funde = []; i, n = 0, len(s)
    while i < n:
        if s.startswith('/*', i) or s.startswith('//', i): i = kommentar_ende(s, i); continue
        if s[i] == "'":
            j = i + 1
            while j < n and s[j] != "'": j += 2 if s[j] == '\\' else 1
            i = j + 1; continue
        if s[i] != '"': i += 1; continue
        start = i; teile = []
        while True:                               # Folge aneinandergehängter Zeichenketten
            j = i + 1
            while j < n and s[j] != '"': j += 2 if s[j] == '\\' else 1
            teile.append(c_text(s[i + 1:j])); i = j + 1
            k = i
            while k < n:
                if s[k] in ' \t\r\n': k += 1
                elif s.startswith('/*', k) or s.startswith('//', k): k = kommentar_ende(s, k)
                else: break
            if k < n and s[k] == '"': i = k; continue
            break
        text = ''.join(teile)
        if not CMD.match(text): continue
        zs = s.rfind('\n', 0, start) + 1; vor = s[zs:start]
        if zh and re.match(r'\s*#\s*define\s+BEF_\w+\s*$', vor): continue   # die Konstanten selbst
        if ENV.search(maske[max(0, start - 200):start]): continue             # Name einer Umgebungsvariablen (Kommentare davor zählen nicht)
        funde.append('%s:%d:%s' % (name, zeile[start], text.replace(' ', '_')))
    return funde

def pruefe_ordner(root):
    funde = []
    for p in sorted(glob.glob(root + '/kern/*.c') + glob.glob(root + '/kern/*.h') + glob.glob(root + '/app/*.c') + glob.glob(root + '/app/*.h')):
        rel = os.path.relpath(p, root).replace('\\', '/')
        funde += pruefe_text(open(p, encoding='utf-8', errors='replace').read(), rel, rel == 'kern/zugriff.h')
    return funde

def selbsttest():
    soll_fund = ['s_befehl(s, "DELE", p);', 's_befehl(s, "SITE " "UNINSTALL", p);', 's_befehl(s, "\\104ELE", p);',
                 '#define ENTFERNE(s,p) s_befehl(s, "DELE", p)', 's_befehl(s, "SITE "\n   /* x */ "UNINSTALL?", p);',
                 'x = "SITE CHMOD 777";', 'if (!strcmp(b, "NOOP")) r = 1;',
                 's_befehl(s, "D\\\nELE", p);', 's_befehl(s, "SITE UNIN\\\r\nSTALL", p);']   # Codex-Prüfung 311: Zeilenfortsetzung
    kein_fund = ['/* "DELE" */ x = 1;', '// "SITE UNINSTALL"\nx = 1;', '// weiter \\\n "DELE"\nx = 1;', 'x = getenv("RMDA");',
                 'x = env(d, "RMDA");', 'x = getenv(/* Test-Schalter */ "RMDA");', 'x = env(d, /* Test-Schalter */ "RMDA");',
                 'x = getenv( // Schalter\n "RMDA");', 'x = "DELETE";', 'x = "SITE UNINSTALL ";', "c = '\"'; x = 1;", 'x = BEF_UNINSTALL " ";']
    fehler = 0
    for t in soll_fund:
        if not pruefe_text(t, 'probe'): print('SELBSTTEST FAIL nicht erkannt:', repr(t)); fehler += 1
    for t in kein_fund:
        f = pruefe_text(t, 'probe')
        if f: print('SELBSTTEST FAIL Fehlalarm:', repr(t), f); fehler += 1
    if pruefe_text('#define BEF_DELE "DELE"\n', 'kern/zugriff.h', True): print('SELBSTTEST FAIL Konstante gemeldet'); fehler += 1
    if not pruefe_text('#define BEF_DELE "DELE"\n', 'kern/x.c', False): print('SELBSTTEST FAIL #define außerhalb zugriff.h nicht erkannt'); fehler += 1
    return fehler

if __name__ == '__main__':
    if sys.argv[1] == '--selbsttest':
        f = selbsttest(); print('SELBSTTEST %s' % ('OK' if f == 0 else 'FAIL (%d)' % f)); sys.exit(1 if f else 0)
    print(' '.join(pruefe_ordner(sys.argv[1])))
