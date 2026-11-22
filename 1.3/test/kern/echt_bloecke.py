# NEU 1.3 (21.11.2026): Block-Aufstellung aus dem PC-Nachbau eines echten PS5-Mitschnitts ("test_mitschnitt zeige <datei> 1").
# Je Block eine Zeile "Bereich: Kopf || Teil | Teil ..." (Teile sortiert, Ort vorne wie "USB-Laufwerk – " weggelassen), sortiert.
# Aufruf: python3 -I echt_bloecke.py <zeige-ausgabe>   (Ausgabe auf stdout; lauf_echt.sh vergleicht sie mit <mitschnitt>.erwartet)
import re, sys
norm = lambda s: re.sub(r"^(USB-Laufwerk|M\.2) – ", "", s.strip())
aus, bereich, kopf, teile = [], None, None, []
def fertig():
    if kopf is not None: aus.append(bereich + ": " + kopf + " || " + " | ".join(sorted(teile)))
for z in open(sys.argv[1], encoding="utf-8", errors="replace"):
    if z.startswith("LOG:"): continue
    m = re.match(r"== (.+?) \(\d+ Bl", z)
    if m: fertig(); bereich, kopf, teile = m.group(1), None, []; continue
    if "  [" not in z or bereich is None: continue
    t = norm(z.split("  [")[0])
    if z.startswith("      "): teile.append(t)
    else: fertig(); kopf, teile = t, []
fertig()
print("\n".join(sorted(aus)))
