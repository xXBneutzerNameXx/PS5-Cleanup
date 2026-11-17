#!/usr/bin/env python3
# PS5 Cleanup 1.0.8 (Arbeitsplan 4.1) - Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
# Erzeugt app/symbole.h aus den Symbolen des eigenen Entwurfs 2 (Artefakt "PS5 Cleanup Oberfläche", von Claude gezeichnet):
# jeder SVG-Pfad wird in Linienzüge zerlegt (Bögen und Kurven in kurze Stücke), Kreise bleiben Kreise. Aufruf: python3 symbole_erzeugen.py
import math, re, os

# Name: (Breite, Höhe des Rasters, Strichstärke im Raster, [Elemente])  - Elemente: ('p', d) Pfad, ('c', cx, cy, r) Kreis,
# ('d', cx, cy, r) gefüllter Punkt, ('r', x, y, w, h, rx) Rechteck
SYM = {
 'HAKEN':      (24, 24, 3.0, [('p', 'M5 12.5l4.5 4.5L19 7.5')]),
 'SPIELE':     (24, 24, 1.8, [('p', 'M7 6.5h10a4.5 4.5 0 0 1 4.4 3.6l1 5.2a2.7 2.7 0 0 1-4.6 2.4l-2.2-2.2H8.4l-2.2 2.2a2.7 2.7 0 0 1-4.6-2.4l1-5.2A4.5 4.5 0 0 1 7 6.5z'), ('p', 'M7.5 9.5v4M5.5 11.5h4')]),
 'APPS':       (24, 24, 1.8, [('r', 4, 4, 6.5, 6.5, 1.5), ('r', 13.5, 4, 6.5, 6.5, 1.5), ('r', 4, 13.5, 6.5, 6.5, 1.5), ('r', 13.5, 13.5, 6.5, 6.5, 1.5)]),
 'SPIELSTAND': (24, 24, 1.8, [('p', 'M5 4h11.5L20 7.5V19a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1z'), ('p', 'M8 4v5h7V4'), ('p', 'M7.5 20v-6h9v6')]),
 'TROPHAE':    (24, 24, 1.8, [('p', 'M8 4h8v5.5a4 4 0 0 1-8 0z'), ('p', 'M8 6H5.5a2.5 2.5 0 0 0 2.6 3.9M16 6h2.5a2.5 2.5 0 0 1-2.6 3.9'), ('p', 'M12 13.5V17M8.5 20.5h7M10 17h4l.5 3.5h-5z')]),
 'BILDER':     (24, 24, 1.8, [('r', 3.5, 5, 17, 14, 2), ('c', 9, 10, 1.6), ('p', 'M4.5 17.5l4.5-4 3.5 3 3-2.5 4 3.5')]),
 'SONSTIGES':  (24, 24, 1.8, [('d', 6, 12, 1.4), ('d', 12, 12, 1.4), ('d', 18, 12, 1.4)]),
 'JAILBREAK':  (24, 24, 1.8, [('p', 'M12 3l7.5 3v5.5c0 4.6-3.2 8.3-7.5 9.5-4.3-1.2-7.5-4.9-7.5-9.5V6z')]),
 'SCHILD_HAKEN': (24, 24, 2.0, [('p', 'M12 3l7.5 3v5.5c0 4.6-3.2 8.3-7.5 9.5-4.3-1.2-7.5-4.9-7.5-9.5V6z'), ('p', 'M8.8 12.2l2.2 2.2 4.2-4.4')]),
 'SORTIEREN':  (24, 24, 2.0, [('p', 'M4 6h10M4 12h7M4 18h4M18 5v14M15 16l3 3 3-3')]),
 'RUNTER':     (24, 24, 2.4, [('p', 'M6 9l6 6 6-6')]),
 'RECHTS':     (24, 24, 2.4, [('p', 'M9 6l6 6-6 6')]),
 'NEU':        (24, 24, 2.2, [('p', 'M19.5 12a7.5 7.5 0 0 1-12.9 5.2M4.5 12A7.5 7.5 0 0 1 17.4 6.8'), ('p', 'M17.5 3.5v3.5H14M6.5 20.5V17H10')]),
 'WARNUNG':    (24, 24, 2.2, [('p', 'M12 4l9 16H3z'), ('p', 'M12 10v4.5'), ('d', 12, 17.3, 0.9)]),
 'INFO':       (24, 24, 2.0, [('c', 12, 12, 9), ('p', 'M12 11v5'), ('d', 12, 7.8, 0.9)]),
 'SCHLOSS':    (24, 24, 2.0, [('r', 5, 10.5, 14, 10, 2), ('p', 'M8 10.5V8a4 4 0 0 1 8 0v2.5')]),
 'DOKUMENT':   (24, 24, 1.9, [('p', 'M7 3.5h7l4 4V20a.5.5 0 0 1-.5.5h-10.5A.5.5 0 0 1 6.5 20V4a.5.5 0 0 1 .5-.5z'), ('p', 'M14 3.5V8h4M9.5 12.5h5M9.5 16h5')]),
 'QR':         (24, 24, 1.6, [('r', 3, 3, 7, 7, 1), ('r', 14, 3, 7, 7, 1), ('r', 3, 14, 7, 7, 1), ('p', 'M14 14h3v3h-3zM20.5 14v.01M14 20.5h.01M17.5 20.5h3M20.5 17.5v3')]),
 'KONSOLE':    (24, 24, 1.8, [('r', 6, 6, 12, 12, 2), ('r', 9.5, 9.5, 5, 5, 1), ('p', 'M9 2.5v3M15 2.5v3M9 18.5v3M15 18.5v3M2.5 9h3M2.5 15h3M18.5 9h3M18.5 15h3')]),
 'SCHEIBE':    (24, 24, 1.8, [('c', 12, 12, 9), ('c', 12, 12, 2.5), ('p', 'M12 6.5a5.5 5.5 0 0 0-5.5 5.5')]),
 'SSD':        (24, 24, 1.8, [('r', 3, 7, 18, 10, 2), ('p', 'M7 12h6'), ('d', 17, 12, 0.9)]),
 'M2':         (24, 24, 1.8, [('r', 2.5, 8.5, 19, 7, 1.5), ('p', 'M6 12h2.5M10.5 12h2.5'), ('d', 18, 12, 0.9)]),
 'USB':        (24, 24, 1.8, [('r', 8.5, 2.5, 7, 5, 1), ('r', 6.5, 7.5, 11, 14, 2.5), ('p', 'M10.8 4.6v1M13.2 4.6v1')]),
 'LAEDT':      (24, 24, 2.6, [('p', 'M12 3a9 9 0 0 1 9 9')]),
 'KN_KREUZ':   (14, 14, 2.0, [('p', 'M2.5 2.5l9 9M11.5 2.5l-9 9')]),
 'KN_KREIS':   (14, 14, 2.0, [('c', 7, 7, 5)]),
 'KN_QUADRAT': (14, 14, 2.0, [('r', 2.5, 2.5, 9, 9, 1)]),
 'KN_DREIECK': (16, 14, 2.0, [('p', 'M8 2l6 10H2z')]),
 'KN_OPTIONEN': (16, 12, 2.0, [('p', 'M1.5 1.5h13M1.5 6h13M1.5 10.5h13')]),
 'KN_HOCH_RUNTER': (18, 16, 2.0, [('p', 'M5 13V3M2 6l3-3 3 3M13 3v10M10 10l3 3 3-3')]),
 'KN_LINKS_RECHTS': (20, 14, 2.0, [('p', 'M3 7h14M6 4L3 7l3 3M14 4l3 3-3 3')]),
}

def bogen(x1, y1, rx, ry, phi, fa, fs, x2, y2):   # SVG-Bogen (Endpunkt-Form) -> Punkte
    if rx == 0 or ry == 0: return [(x2, y2)]
    c, s = math.cos(math.radians(phi)), math.sin(math.radians(phi))
    dx, dy = (x1 - x2) / 2, (y1 - y2) / 2
    x1p, y1p = c * dx + s * dy, -s * dx + c * dy
    rx, ry = abs(rx), abs(ry)
    l = x1p ** 2 / rx ** 2 + y1p ** 2 / ry ** 2
    if l > 1: rx, ry = rx * math.sqrt(l), ry * math.sqrt(l)
    z = max(0.0, (rx ** 2 * ry ** 2 - rx ** 2 * y1p ** 2 - ry ** 2 * x1p ** 2) / (rx ** 2 * y1p ** 2 + ry ** 2 * x1p ** 2))
    k = math.sqrt(z) * (-1 if fa == fs else 1)
    cxp, cyp = k * rx * y1p / ry, -k * ry * x1p / rx
    cx, cy = c * cxp - s * cyp + (x1 + x2) / 2, s * cxp + c * cyp + (y1 + y2) / 2
    def w(ux, uy, vx, vy):
        a = math.atan2(ux * vy - uy * vx, ux * vx + uy * vy); return a
    t1 = w(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dt = w((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not fs and dt > 0: dt -= 2 * math.pi
    if fs and dt < 0: dt += 2 * math.pi
    n = max(2, int(abs(dt) / (math.pi / 16)) + 1)
    pts = []
    for i in range(1, n + 1):
        t = t1 + dt * i / n
        pts.append((cx + rx * math.cos(t) * c - ry * math.sin(t) * s, cy + rx * math.cos(t) * s + ry * math.sin(t) * c))
    return pts

def kurve(p0, p1, p2, p3, n=10):
    out = []
    for i in range(1, n + 1):
        t = i / n; u = 1 - t
        out.append((u**3*p0[0] + 3*u*u*t*p1[0] + 3*u*t*t*p2[0] + t**3*p3[0], u**3*p0[1] + 3*u*u*t*p1[1] + 3*u*t*t*p2[1] + t**3*p3[1]))
    return out

def pfad(d):
    tok = re.findall(r'[MmLlHhVvCcAaZz]|-?(?:\d+\.?\d*|\.\d+)', d)
    i, cmd, cur, start, zuege, zug = 0, None, (0.0, 0.0), (0.0, 0.0), [], []
    def zahl():
        nonlocal i
        v = float(tok[i]); i += 1; return v
    while i < len(tok):
        if re.match(r'[A-Za-z]', tok[i]): cmd = tok[i]; i += 1
        rel = cmd.islower(); C = cmd.upper()
        if C == 'Z':
            if zug: zug.append(start); zuege.append(zug); zug = []
            cur = start; continue
        if C == 'M':
            x, y = zahl(), zahl()
            if rel: x, y = cur[0] + x, cur[1] + y
            if zug: zuege.append(zug)
            zug = [(x, y)]; cur = start = (x, y); cmd = 'l' if rel else 'L'; continue
        if C == 'L':
            x, y = zahl(), zahl()
            if rel: x, y = cur[0] + x, cur[1] + y
            zug.append((x, y)); cur = (x, y); continue
        if C == 'H':
            x = zahl(); x = cur[0] + x if rel else x
            zug.append((x, cur[1])); cur = (x, cur[1]); continue
        if C == 'V':
            y = zahl(); y = cur[1] + y if rel else y
            zug.append((cur[0], y)); cur = (cur[0], y); continue
        if C == 'C':
            a = [zahl() for _ in range(6)]
            if rel: a = [a[j] + (cur[0] if j % 2 == 0 else cur[1]) for j in range(6)]
            zug += kurve(cur, (a[0], a[1]), (a[2], a[3]), (a[4], a[5])); cur = (a[4], a[5]); continue
        if C == 'A':
            rx, ry, phi, fa, fs, x, y = zahl(), zahl(), zahl(), zahl(), zahl(), zahl(), zahl()
            if rel: x, y = cur[0] + x, cur[1] + y
            zug += bogen(cur[0], cur[1], rx, ry, phi, int(fa), int(fs), x, y); cur = (x, y); continue
        raise ValueError(cmd)
    if zug: zuege.append(zug)
    return zuege

def rechteck(x, y, w, h, r):
    pts = []
    for (cx, cy, a0) in ((x + w - r, y + r, -90), (x + w - r, y + h - r, 0), (x + r, y + h - r, 90), (x + r, y + r, 180)):
        for k in range(0, 7):
            a = math.radians(a0 + 15 * k); pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    pts.append(pts[0]); return [pts]

zeilen = ['/* Erzeugt von app/werkzeug/symbole_erzeugen.py - nicht von Hand ändern. Symbole aus dem eigenen Entwurf 2 (Arbeitsplan 4.1).',
          ' * Aufbau je Symbol: Breite, Höhe des Rasters, Strichstärke, dann Elemente: 1 n x0 y0 .. (Linienzug), 2 cx cy r (Kreis), 3 cx cy r (Punkt), 0 = Ende */',
          '#ifndef SYMBOLE_H', '#define SYMBOLE_H']
for name, (bw, bh, st, el) in SYM.items():
    w = [bw, bh, st]
    for e in el:
        if e[0] == 'p':
            for z in pfad(e[1]):
                if len(z) < 2: z = [z[0], (z[0][0] + 0.01, z[0][1])]
                w += [1, len(z)] + [round(v, 2) for p in z for v in p]
        elif e[0] == 'r':
            for z in rechteck(*e[1:]): w += [1, len(z)] + [round(v, 2) for p in z for v in p]
        elif e[0] == 'c': w += [2, e[1], e[2], e[3]]
        elif e[0] == 'd': w += [3, e[1], e[2], e[3]]
    w.append(0)
    def fmt(v):
        if not isinstance(v, float): return str(v)
        s = '%g' % v
        return (s if ('.' in s or 'e' in s) else s + '.0') + 'f'
    zeilen.append('static const float SYM_%s[] = { %s };' % (name, ', '.join(fmt(v) for v in w)))
zeilen.append('#endif')
ziel = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'symbole.h')
open(ziel, 'w', encoding='utf-8').write('\n'.join(zeilen) + '\n')
print('geschrieben:', ziel, len(SYM), 'Symbole')
