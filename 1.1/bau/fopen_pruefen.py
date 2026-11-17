# PS5 Cleanup 1.0.7 - prüft vorübersetzten C-Text (stdin): jedes fopen(...) darf nur lesend öffnen ("r" oder "rb").
# Aufruf: <cc> -E ... datei.c | python3 -I bau/fopen_pruefen.py datei.c   -> Rückgabe 1 bei einem Verstoß (da_Chris86 31.10.2026 21:50, Punkt 5)
import re, sys
text = sys.stdin.read()
name = sys.argv[1] if len(sys.argv) > 1 else "?"
fehler = 0; anzahl = 0
for m in re.finditer(r'\bfopen\s*\(', text):
    i = m.end(); tiefe = 1; arg_start = i; letzte = i; in_str = False
    while i < len(text) and tiefe > 0:
        c = text[i]
        if in_str:
            if c == '\\': i += 1
            elif c == '"': in_str = False
        elif c == '"': in_str = True
        elif c == '(': tiefe += 1
        elif c == ')': tiefe -= 1
        elif c == ',' and tiefe == 1: letzte = i + 1
        i += 1
    modus = text[letzte:i - 1].strip()
    if 'char' in text[arg_start:i]: continue   # Deklaration im Systemkopf, kein Aufruf
    anzahl += 1
    if modus not in ('"r"', '"rb"'):
        print("VERSTOSS %s: fopen(... %s)" % (name, modus)); fehler += 1
print("fopen in %s: %d Aufrufe, %d nicht lesend" % (name, anzahl, fehler))
sys.exit(1 if fehler else 0)
