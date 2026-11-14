#!/usr/bin/env python3
"""Draws the first versions of data/cities/paris.map and data/cities/saint_denis.map.

Each town is designed on a coarse grid and then doubled (each character becomes 2 x 2
tiles), so streets, houses and landmarks are twice the size of a person's tile.

The .map files are the master copies once written: edit them by hand afterwards.
Run again only to start over: it overwrites the maps of the towns named on the command line.
"""
import math, os, random, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "data", "cities")

# ---------------------------------------------------------------- Paris

def paris():
    W, H = 110, 96
    g = [[' '] * W for _ in range(H)]
    rnd = random.Random(1632)

    def put(x, y, c):
        if 0 <= x < W and 0 <= y < H:
            g[y][x] = c

    def get(x, y):
        return g[y][x] if 0 <= x < W and 0 <= y < H else '?'

    # the Seine comes in from the south-east (matching the old map), flows west through
    # the town around the islands, and leaves to the west; the Marne joins it from the east
    SEINE = [(101, 99, 4.5), (100, 84, 4.5), (98, 68, 4.5), (95, 58, 5.5), (91, 51, 7.5), (86, 47.5, 8.5),
             (60, 47, 8.5), (30, 47, 8.5), (24, 47, 6.0), (18, 46.5, 4.5), (-3, 46.5, 4.5)]
    MARNE = [(113, 42, 3.2), (106, 47, 3.2), (100, 56, 3.2), (97, 62, 3.5)]

    def near_river(x, y, line):
        for (ax, ay, aw), (bx, by, bw) in zip(line, line[1:]):
            dx, dy = bx - ax, by - ay
            L2 = dx * dx + dy * dy
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / L2))
            px, py = ax + t * dx, ay + t * dy
            if math.hypot(x - px, y - py) <= aw + (bw - aw) * t:
                return True
        return False

    cite = lambda x, y: 28 <= x <= 72 and 42 <= y <= 52 and not (x <= 30 and abs(y - 47) > (x - 27))
    stlouis = lambda x, y: 76 <= x <= 88 and 44 <= y <= 50 and not (x in (76, 88) and y in (44, 50))

    for y in range(H):
        for x in range(W):
            if (near_river(x, y, SEINE) or near_river(x, y, MARNE)) and not cite(x, y) and not stlouis(x, y):
                g[y][x] = '~'

    # walls: right bank (north) and left bank (south) ellipses
    RB = (54, 47, 48, 42)
    LB = (52, 50, 38, 40)

    def rho(e, x, y):
        cx, cy, a, b = e
        return math.hypot((x - cx) / a, (y - cy) / b)

    def interior(x, y):
        if get(x, y) == '~':
            return False
        if cite(x, y) or stlouis(x, y):
            return True
        if y < 47:
            return rho(RB, x, y) < 1
        return rho(LB, x, y) < 1

    inside = [[interior(x, y) for x in range(W)] for y in range(H)]
    for y in range(H):
        for x in range(W):
            if not inside[y][x] or cite(x, y) or stlouis(x, y):
                continue
            e = RB if y < 47 else LB
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if get(nx, ny) == '~':
                    continue
                if not (0 <= nx < W and 0 <= ny < H) or rho(e, nx, ny) >= 1:
                    g[y][x] = '#'
                    break

    def isin(x, y):
        return 0 <= x < W and 0 <= y < H and inside[y][x] and g[y][x] != '#'

    # town fabric: houses everywhere inside
    for y in range(H):
        for x in range(W):
            if isin(x, y):
                g[y][x] = 'h'

    # street along the inside of the walls, and the quays
    for y in range(H):
        for x in range(W):
            if g[y][x] != 'h':
                continue
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    c = get(x + dx, y + dy)
                    if c == '#' or (c == '~' and (dx == 0 or dy == 0)):
                        g[y][x] = '.'

    def street(points, width=2, ch='.', only=None):
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            n = max(abs(x1 - x0), abs(y1 - y0)) * 2 + 1
            for i in range(n + 1):
                t = i / n
                x = round(x0 + (x1 - x0) * t)
                y = round(y0 + (y1 - y0) * t)
                for k in range(width):
                    for j in range(width):
                        xx, yy = x + k, y + j
                        c = get(xx, yy)
                        if c in ('?', '~', '#'):
                            continue
                        if only and c not in only:
                            continue
                        put(xx, yy, ch)

    # main streets, right bank
    street([(45, 7), (45, 20), (46, 31), (45, 38)])              # rue Saint-Denis
    street([(53, 6), (53, 22), (53, 38)])                         # rue Saint-Martin
    street([(10, 31), (18, 27), (22, 24), (46, 24)])              # rue Saint-Honore
    street([(92, 22), (85, 26), (72, 33), (58, 37)])              # rue Saint-Antoine
    street([(64, 9), (63, 20), (60, 33)])                         # rue du Temple
    street([(42, 19), (46, 19)], 2)                               # to the Halles
    # main streets, left bank
    street([(53, 56), (53, 70), (53, 89)])                        # rue Saint-Jacques
    street([(45, 56), (43, 66), (38, 86)])                        # rue de la Harpe
    street([(45, 58), (30, 60), (16, 62)])                        # rue Saint-Andre-des-Arts, to Porte de Buci
    street([(55, 59), (70, 60), (87, 62)])                        # rue Saint-Victor
    # Ile de la Cite
    street([(45, 42), (45, 52)])                                  # rue de la Barillerie
    street([(53, 42), (53, 52)])                                  # rue de la Juiverie
    street([(31, 47), (72, 47)], 1)                               # a lane along the island

    # lanes: split every block of houses in two until it is about two houses deep
    def components(tiles_ok):
        seen = set()
        comps = []
        for y in range(H):
            for x in range(W):
                if (x, y) in seen or not tiles_ok(x, y):
                    continue
                comp, stack = [], [(x, y)]
                seen.add((x, y))
                while stack:
                    cx, cy = stack.pop()
                    comp.append((cx, cy))
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        q = (cx + dx, cy + dy)
                        if q not in seen and tiles_ok(*q):
                            seen.add(q)
                            stack.append(q)
                comps.append(comp)
        return comps

    def split(comp, depth=0):
        xs = [p[0] for p in comp]
        ys = [p[1] for p in comp]
        w, h = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
        if (w <= 11 and h <= 11) or depth > 12:
            return
        cs = set(comp)
        if w >= h:
            cut = min(xs) + w // 2 + rnd.randint(-2, 2)
            jog_at = rnd.randint(min(ys), max(ys))
            jog = 0
            line = {(cut + (jog if y >= jog_at else 0), y) for y in range(min(ys), max(ys) + 1)}
        else:
            cut = min(ys) + h // 2 + rnd.randint(-2, 2)
            jog_at = rnd.randint(min(xs), max(xs))
            jog = 0
            line = {(x, cut + (jog if x >= jog_at else 0)) for x in range(min(xs), max(xs) + 1)}
        for (x, y) in line:
            if (x, y) in cs:
                put(x, y, '.')
        rest = cs - line
        for sub in components(lambda x, y: (x, y) in rest):
            split(sub, depth + 1)

    def lanes():
        for comp in components(lambda x, y: get(x, y) == 'h'):
            split(comp)

    def ring(ch):
        # a street around each landmark, where it would otherwise touch houses
        for y in range(H):
            for x in range(W):
                if get(x, y) != ch:
                    continue
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        if get(x + dx, y + dy) == 'h':
                            put(x + dx, y + dy, '.')

    def rect(x0, y0, x1, y1, ch, only=None):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                c = get(x, y)
                if c == '?':
                    continue
                if only and c not in only:
                    continue
                put(x, y, ch)

    # squares
    rect(57, 36, 71, 38, ':', only=('h', '.'))                   # Place de Greve
    rect(56, 43, 58, 51, ':', only=('h', '.'))                   # parvis Notre-Dame
    rect(31, 11, 44, 18, ':', only=('h', '.'))                   # market around the Halles
    rect(58, 59, 66, 62, ':', only=('h', '.'))                   # Place Maubert
    rect(32, 46, 35, 48, ':', only=('h', '.'))                   # Place Dauphine

    # landmarks (letters are explained in the legend)
    rect(22, 26, 34, 37, 'L')        # Louvre, the square court
    rect(4, 39, 21, 41, 'K')         # Grande Galerie along the river
    rect(36, 26, 43, 31, 'A')        # Saint-Germain-l'Auxerrois
    rect(38, 33, 44, 37, 'C')        # Grand Chatelet
    rect(60, 30, 68, 35, 'V')        # Hotel de Ville
    rect(34, 12, 41, 16, 'Q')        # les Halles
    rect(21, 18, 31, 23, 'E')        # Saint-Eustache
    rect(72, 15, 82, 25, 'R')        # Place Royale
    rect(86, 25, 94, 33, 'B')        # Bastille
    rect(64, 10, 71, 17, 'X')        # the Temple
    rect(36, 42, 44, 50, 'P')        # Palais de la Cite
    rect(47, 43, 52, 46, 'S')        # Sainte-Chapelle
    rect(59, 43, 71, 49, 'N')        # Notre-Dame
    rect(56, 64, 64, 71, 'U')        # Sorbonne
    rect(64, 76, 73, 81, 'G')        # Sainte-Genevieve
    rect(4, 60, 13, 65, 'Y')         # Saint-Germain-des-Pres (outside the wall)
    rect(6, 77, 20, 87, 'Z')         # Luxembourg (outside the wall)
    rect(27, 3, 33, 6, 'I')          # an inn outside Porte Montmartre? (outside the wall)

    for ch in 'LKACVQERBXPSNUGYZI':
        ring(ch)
    lanes()

    # gardens of the Luxembourg and the Tuileries, the tip of the Cite
    rect(6, 88, 20, 94, ',', only=(' ',))
    for x in range(7, 20, 2):
        for y in (89, 92):
            put(x, y, 't')
    rect(0, 33, 5, 38, ',', only=(' ',))
    for x in range(0, 6, 2):
        for y in (34, 37):
            put(x, y, 't')
    for (x, y) in ((28, 47), (29, 46), (29, 48), (30, 47)):
        if get(x, y) in ('h', '.'):
            put(x, y, 't' if (x + y) % 2 else ',')

    # bridges, two tiles wide, from bank to bank
    def bridge(x, y0, y1):
        for y in range(y0, y1 + 1):
            for k in (0, 1):
                if get(x + k, y) == '~':
                    put(x + k, y, '=')
    bridge(29, 38, 56)      # Pont Neuf, across the tip of the Cite
    bridge(45, 37, 42)      # Pont au Change
    bridge(53, 37, 42)      # Pont Notre-Dame
    bridge(45, 52, 57)      # Pont Saint-Michel
    bridge(53, 52, 57)      # Petit Pont
    for x in range(72, 77):  # Pont Rouge, to the Ile Saint-Louis
        for y in (46, 47):
            if get(x, y) == '~':
                put(x, y, '=')
    bridge(81, 38, 44)      # Pont Marie
    bridge(81, 50, 57)      # Pont de la Tournelle
    # the Pont Neuf crosses the island as a street
    for y in range(38, 57):
        for k in (0, 1):
            if get(29 + k, y) in ('h', 't', ',', ':'):
                put(29 + k, y, '.')

    # Ile Saint-Louis: half built, half meadow in 1632
    for y in range(44, 51):
        for x in range(76, 89):
            if get(x, y) == 'h' and x > 83:
                put(x, y, ',' if (x * 7 + y) % 5 else 't')

    # gates: (x, y, digit, passage 'v' for north-south, 'h' for east-west)
    gates = [(9, 32, '1', 'h'), (45, 6, '2', 'v'), (53, 5, '3', 'v'), (92, 22, '4', 'h'),
             (88, 62, '5', 'h'), (53, 89, '6', 'v'), (38, 87, '7', 'v'), (15, 62, '8', 'h')]
    for (x, y, d, o) in gates:
        # find the wall tile nearest to the intended spot along the passage
        best = None
        for k in range(-3, 4):
            xx, yy = (x, y + k) if o == 'v' else (x + k, y)
            if get(xx, yy) == '#':
                best = (xx, yy)
                break
        if not best:
            print("no wall for gate", d, file=sys.stderr)
            continue
        x, y = best
        if o == 'v':
            for k in (0, 1):
                put(x + k, y, d)
                for dy in (-1, 1):
                    if get(x + k, y + dy) in ('#', 'h', ' '):
                        put(x + k, y + dy, '.' if inside[y + dy][x + k] else '_')
            for k in (-1, 2):
                if get(x + k, y) not in ('O',):
                    put(x + k, y, '#')
        else:
            for k in (0, 1):
                put(x, y + k, d)
                for dx in (-1, 1):
                    if get(x + dx, y + k) in ('#', 'h', ' '):
                        put(x + dx, y + k, '.' if inside[y + k][x + dx] else '_')
            for k in (-1, 2):
                put(x, y + k, '#')
        # road outward to the edge of the map
        cx, cy = (x, y)
        if o == 'v':
            step = -1 if y < 47 else 1
            yy = y + step
            while 0 <= yy < H:
                for k in (0, 1):
                    if get(x + k, yy) in (' ', ',', '_'):
                        put(x + k, yy, '_')
                    elif get(x + k, yy) == '~':
                        put(x + k, yy, '=')
                yy += step
        else:
            step = -1 if x < 55 else 1
            xx = x + step
            while 0 <= xx < W:
                for k in (0, 1):
                    if get(xx, y + k) in (' ', ',', '_'):
                        put(xx, y + k, '_')
                    elif get(xx, y + k) == '~':
                        put(xx, y + k, '=')
                xx += step

    # wall towers every few tiles
    count = 0
    for y in range(H):
        for x in range(W):
            if g[y][x] != '#':
                continue
            near = any(get(x + dx, y + dy) not in ('#', 'h', '.', ' ', '~', ',', 't', '_', ':')
                       for dx in range(-3, 4) for dy in range(-3, 4))
            if near:
                continue
            count += 1
            if count % 9 == 0:
                put(x, y, 'O')
    # the Tour de Nesle, where the left-bank wall meets the river
    for (x, y) in ((14, 55), (15, 55), (14, 56), (15, 56)):
        if get(x, y) != '~':
            put(x, y, 'O')

    # faubourg Saint-Germain around the abbey: a few houses along the road
    rect(4, 66, 13, 70, 'h', only=(' ',))
    rect(4, 57, 13, 59, 'h', only=(' ',))
    rect(14, 66, 20, 70, 'h', only=(' ',))
    street([(4, 66), (20, 66)], 1, '.', only=('h',))
    street([(9, 57), (9, 70)], 1, '.', only=('h',))

    # Henri IV on horseback, on the Pont Neuf at the tip of the Cite (set up in 1614)
    put(31, 47, 's')
    # wells, fountains, market stalls, crosses
    for (x, y, c) in ((64, 37, 'f'), (60, 61, 'o'), (34, 47, 'f'), (57, 47, '+'), (24, 59, 'o'),
                      (70, 21, 'o'), (47, 15, 'o'), (85, 70, 'o')):
        if get(x, y) in (':', '.', 'h'):
            put(x, y, c)
    for y in range(11, 19):
        for x in range(31, 45):
            if get(x, y) == ':' and rnd.random() < 0.18 and (x + y) % 2 == 0:
                put(x, y, 'm')

    # gardens behind some blocks (monastery and college gardens)
    for (x0, y0, x1, y1) in ((76, 64, 84, 72), (24, 66, 30, 72), (70, 37, 74, 38)):
        rect(x0, y0, x1, y1, ',', only=('h',))
        for y in range(y0, y1 + 1, 2):
            for x in range(x0, x1 + 1, 3):
                if get(x, y) == ',':
                    put(x, y, 't')

    return double(g)


def double(g):
    """Each coarse character becomes 2 x 2 tiles; a single feature (a tree, a well, a
    statue...) stays a single tile, the other three becoming the ground around it."""
    ground = {'t': ',', 'o': ':', 'f': ':', 'm': ':', '+': ':', 'b': ':', 's': ':', 'y': ':', 'x': ';', 'k': '.',
              'i': ':', 'j': ':', 'g': '~', 'p': '~', 'u': ';', 'n': ' '}   # a gun, a tent: one to a cell
    out = []
    for r in g:
        top, bot = [], []
        for c in r:
            if c in ground:
                top += [c, ground[c]]
                bot += [ground[c], ground[c]]
            else:
                top += [c, c]
                bot += [c, c]
        out.append(''.join(top).rstrip())
        out.append(''.join(bot).rstrip())
    return out


PARIS_HEADER = """\
# Paris in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py; this file is now the master copy.
#
# fixed characters:
#   ' ' countryside      '~' water        '=' bridge      '.' street      '_' road
#   ':' paved square     ',' garden       ';' yard        't' tree        'h' houses
#   '#' town wall        'O' wall tower   '1'-'9' gates   'o' well        'f' fountain
#   'm' market stall     '+' cross        '%' hedge       'b' bench       'x' hay
#   'k' barrel       's' statue
# the other capital letters are landmarks:

style paris
ground 1.2
towerroof cone

legend L palace   "the Louvre"                   depth=6 storeys=3 clock
legend K gallery  "the Grande Galerie"
legend A church   "Saint-Germain-l'Auxerrois"
legend C fortress "the Grand Châtelet"           height=6.5 roofs
legend V hall     "the Hôtel de Ville"
legend Q halle    "les Halles"
legend E church   "Saint-Eustache"
legend R place    "the Place Royale"             style=brick
legend B fortress "the Bastille"                 height=8.5
legend X fortress "the Temple"                   height=7 roofs
legend P palace   "the Palais de la Cité"        depth=6 storeys=3
legend S chapel   "the Sainte-Chapelle"
legend N cathedral "Notre-Dame de Paris"
legend U college  "the Sorbonne"                 depth=6 storeys=3
legend G church   "Sainte-Geneviève"
legend Y abbey    "Saint-Germain-des-Prés"
legend Z palace   "the Palais du Luxembourg"     depth=6 storeys=3
legend I inn      "the Pomme de Pin"

gate 1 "the Porte Saint-Honoré"
gate 2 "the Porte Saint-Denis"
gate 3 "the Porte Saint-Martin"
gate 4 "the Porte Saint-Antoine"
gate 5 "the Porte Saint-Victor"
gate 6 "the Porte Saint-Jacques"
gate 7 "the Porte Saint-Michel"
gate 8 "the Porte de Buci"

map
"""

# ---------------------------------------------------------------- Saint-Denis

def saint_denis():
    W, H = 50, 32
    g = [[' '] * W for _ in range(H)]

    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                g[y][x] = c

    X0, X1, Y0, Y1 = 9, 42, 3, 28          # the wall
    rect(X0, Y0, X1, Y1, '#')
    rect(X0 + 1, Y0 + 1, X1 - 1, Y1 - 1, 'h')
    # streets
    rect(X0 + 1, Y0 + 1, X1 - 1, Y0 + 1, '.')      # inside the walls
    rect(X0 + 1, Y1 - 1, X1 - 1, Y1 - 1, '.')
    rect(X0 + 1, Y0 + 1, X0 + 1, Y1 - 1, '.')
    rect(X1 - 1, Y0 + 1, X1 - 1, Y1 - 1, '.')
    rect(25, Y0 + 1, 26, Y1 - 1, '.')               # the main street, north to south
    rect(X0 + 1, 15, X1 - 1, 15, '.')
    rect(16, Y0 + 1, 16, Y1 - 1, '.')
    rect(X0 + 1, 21, 24, 21, '.')
    # landmarks
    rect(27, 7, 27, 14, ':')                        # the square before the basilica
    rect(28, 7, 40, 13, 'D')                        # the basilica
    rect(30, 17, 40, 25, 'C')                       # the abbey
    rect(28, 14, 40, 14, '.')
    rect(28, 16, 29, 26, ',')
    rect(17, 16, 24, 20, ':')                       # market
    g[18][20] = 'o'
    for (x, y) in ((18, 17), (22, 17), (18, 19), (23, 19)):
        g[y][x] = 'm'
    rect(17, 23, 21, 26, 'I')                       # the inn
    # gates, roads and towers
    for k in (25, 26):
        g[Y0][k] = '2'
        g[Y1][k] = '1'
        for y in range(0, Y0):
            g[y][k] = '_'
        for y in range(Y1 + 1, H):
            g[y][k] = '_'
    for (x, y) in ((X0, Y0), (X1, Y0), (X0, Y1), (X1, Y1), (17, Y0), (34, Y0), (17, Y1), (34, Y1),
                   (X0, 11), (X0, 20), (X1, 11), (X1, 20)):
        g[y][x] = 'O'
    # orchards and fields outside
    for y in range(Y0 + 2, Y1 - 1, 3):
        for x in range(2, 7, 2):
            g[y][x] = 't'
    return double(g)


SD_HEADER = """\
# Saint-Denis in 1632: the royal abbey north of Paris, its market, and a small walled town.
# Same characters as paris.map.

style village
ground 1.0
towerroof cone

legend D cathedral "the Basilica of Saint-Denis"   spire=north
legend C palace    "the Abbey of Saint-Denis"      depth=6 storeys=2
legend I inn       "the Écu de France"

gate 1 "the Paris gate"
gate 2 "the Pontoise gate"

map
"""


# ---------------------------------------------------------------- London

def london():
    W, H = 110, 90
    g = [[' '] * W for _ in range(H)]
    rnd = random.Random(1603)

    def put(x, y, c):
        if 0 <= x < W and 0 <= y < H:
            g[y][x] = c

    def get(x, y):
        return g[y][x] if 0 <= x < W and 0 <= y < H else '?'

    def near(x, y, line):
        for (ax, ay, aw), (bx, by, bw) in zip(line, line[1:]):
            dx, dy = bx - ax, by - ay
            L2 = dx * dx + dy * dy
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / L2))
            if math.hypot(x - (ax + t * dx), y - (ay + t * dy)) <= aw + (bw - aw) * t:
                return True
        return False

    # the Thames, from the west past Westminster and the City, opening into the estuary
    # beyond the Tower (as on the old map); the Fleet coming down from the north
    THAMES = [(-3, 42, 4.0), (12, 42.5, 4.0), (26, 44.5, 4.2), (42, 47.5, 4.4), (60, 50, 4.6), (74, 51, 4.8),
              (86, 52, 5.5), (94, 53, 8.0), (102, 56, 13.0), (114, 58, 20.0)]
    FLEET = [(38, 3, 1.2), (38.5, 12, 1.3), (39, 30, 1.5), (40, 44, 1.8)]
    for y in range(H):
        for x in range(W):
            if near(x, y, THAMES) or near(x, y, FLEET):
                g[y][x] = '~'

    def rect(x0, y0, x1, y1, ch, only=None):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                c = get(x, y)
                if c == '?' or (only and c not in only):
                    continue
                put(x, y, ch)

    def street(points, width=2, ch='.', only=None, bridge=True):
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            n = max(abs(x1 - x0), abs(y1 - y0)) * 2 + 1
            for i in range(n + 1):
                t = i / n
                x, y = round(x0 + (x1 - x0) * t), round(y0 + (y1 - y0) * t)
                for k in range(width):
                    for j in range(width):
                        c = get(x + k, y + j)
                        if c == '?' or c == '#':
                            continue
                        if c == '~':
                            if bridge:
                                put(x + k, y + j, '=')
                            continue
                        if only and c not in only:
                            continue
                        put(x + k, y + j, ch)

    # the City inside London Wall
    E = (63, 46, 22, 32)
    rho = lambda x, y: math.hypot((x - E[0]) / E[2], (y - E[1]) / E[3])
    inside = [[get(x, y) != '~' and y <= 48 and rho(x, y) < 1 for x in range(W)] for y in range(H)]
    for y in range(H):
        for x in range(W):
            if not inside[y][x]:
                continue
            g[y][x] = 'h'
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if get(nx, ny) == '~':
                    continue
                if not (0 <= nx < W and 0 <= ny < H) or not inside[ny][nx]:
                    if rho(nx, ny) >= 1 or ny > 48:
                        g[y][x] = '#'
                        break
    for y in range(H):   # the river front is open: no wall along the Thames
        for x in range(W):
            if g[y][x] == '#' and get(x, y + 1) == '~' and y > 40:
                g[y][x] = 'h'
    # streets just inside the wall, and along the river
    for y in range(H):
        for x in range(W):
            if g[y][x] != 'h':
                continue
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    c = get(x + dx, y + dy)
                    if c == '#' or (c == '~' and (dx == 0 or dy == 0)):
                        g[y][x] = '.'

    # suburbs: houses along the roads out of town, Southwark, Westminster
    rect(26, 15, 37, 19, 'h', only=(' ',))      # Holborn, north side
    rect(26, 22, 36, 24, 'h', only=(' ',))
    rect(72, 3, 75, 17, 'h', only=(' ',))       # Bishopsgate without (Shoreditch)
    rect(79, 3, 82, 17, 'h', only=(' ',))
    rect(86, 21, 101, 23, 'h', only=(' ',))     # Aldgate without (Whitechapel)
    rect(86, 28, 101, 29, 'h', only=(' ',))
    rect(14, 20, 37, 27, 'h', only=(' ',))      # the Strand, north side
    rect(28, 52, 62, 58, 'h', only=(' ',))      # Bankside
    rect(63, 57, 80, 82, 'h', only=(' ',))      # Southwark along Borough High Street
    rect(14, 37, 27, 42, 'h', only=(' ',))      # Westminster by the river

    # streets
    street([(46, 26), (56, 26), (68, 27), (76, 27), (81, 25)])          # Cheapside, Cornhill, Leadenhall
    street([(43, 34), (46, 32)])                                         # Ludgate Hill
    street([(76, 20), (75, 30), (72, 40), (71, 46)])                     # Bishopsgate, Gracechurch, Fish Street Hill
    street([(52, 18), (53, 26)])                                         # Aldersgate Street
    street([(58, 15), (59, 26)])                                         # Wood Street
    street([(66, 14), (65, 26)])                                         # Coleman Street
    street([(60, 35), (72, 36), (80, 38)])                               # Candlewick, Tower Street
    street([(42, 33), (30, 32), (20, 28), (13, 26)])                     # Fleet Street and the Strand
    street([(12, 26), (12, 42)])                                         # King Street, Westminster
    street([(71, 57), (71, 80)])                                         # Borough High Street
    street([(28, 59), (64, 59)], 1)                                      # Bankside lane
    street([(41, 59), (41, 60)], 2)
    street([(33, 59), (33, 60)], 2)

    # squares, landmarks
    rect(45, 27, 62, 35, ':', only=('h', '.'))   # St Paul's churchyard
    rect(42, 11, 50, 16, ':')                    # Smithfield
    rect(77, 26, 83, 31, ':', only=('h', '.', ' '))   # Tower Hill
    rect(47, 28, 60, 33, 'P')                    # Old St Paul's
    rect(60, 19, 66, 23, 'H')                    # the Guildhall
    rect(72, 29, 77, 35, 'X')                    # the Royal Exchange
    rect(62, 29, 66, 32, 'I')                    # the Mermaid Tavern
    for (x0, y0, x1, y1) in ((46, 8, 49, 10), (55, 8, 58, 10), (67, 22, 70, 25), (24, 29, 27, 31),
                             (32, 34, 35, 36), (78, 34, 81, 36), (66, 40, 69, 43)):
        rect(x0, y0, x1, y1, 'C')                # parish churches (named in the legend, in this order)
    # the Tower in its moat
    rect(84, 31, 98, 46, '~')
    rect(86, 33, 96, 44, 'T')
    for x in range(84, 99):
        if get(x, 47) in (' ', 'h', '.'):
            put(x, 47, '~')
    for x in (84, 85):              # the causeway from Tower Hill
        for y in (37, 38):
            put(x, y, '=')
    # Southwark
    rect(60, 57, 68, 61, 'S')                    # St Saviour's
    rect(50, 54, 57, 58, 'W')                    # Winchester Palace
    rect(38, 61, 44, 67, 'G')                    # the Globe
    rect(30, 61, 35, 66, 'B')                    # the Bear Garden
    rect(73, 64, 76, 67, 'J')                    # the Tabard
    # Westminster
    rect(1, 29, 11, 34, 'A')                     # the Abbey
    rect(2, 36, 9, 39, 'V')                      # Westminster Hall
    rect(14, 28, 18, 31, 'D')                    # the Banqueting House
    rect(15, 33, 25, 40, 'Y')                    # Whitehall
    rect(27, 33, 31, 37, 'M')                    # Somerset House
    rect(32, 36, 36, 39, 'M')                    # Arundel House
    put(10, 24, '+')                             # Charing Cross
    put(61, 28, '+')                             # Paul's Cross

    def ring(chs):
        for y in range(H):
            for x in range(W):
                if get(x, y) not in chs:
                    continue
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        if get(x + dx, y + dy) == 'h':
                            put(x + dx, y + dy, '.')
    ring('PHXICTSWGBJAVDYM')

    # London Bridge: houses on both sides, the Great Stone Gate at the Southwark end
    for y in range(45, 58):
        for x in range(69, 75):
            if get(x, y) == '~':
                put(x, y, '=')
    for y in range(47, 54):
        for x in (69, 70, 73, 74):
            if get(x, y) == '=':
                put(x, y, 'Q')
    for x in (71, 72):
        if get(x, 55) == '=':
            put(x, 55, '9')

    # gates in London Wall, and the roads leaving them
    gates = [(43, 34, '1', 'h', [(42, 34), (40, 33)]),
             (46, 26, '2', 'h', [(44, 26), (38, 24), (25, 18), (-2, 12)]),
             (52, 18, '3', 'v', [(52, 16), (52, -2)]),
             (58, 15, '4', 'v', [(58, 13), (58, -2)]),
             (66, 14, '5', 'v', [(66, 12), (66, -2)]),
             (76, 20, '6', 'v', [(76, 18), (77, 8), (77, -2)]),
             (81, 25, '7', 'h', [(83, 25), (100, 25), (112, 25)])]
    for (x, y, d, o, road) in gates:
        best = None
        for k in range(0, 4):
            for kk in (k, -k):
                xx, yy = (x, y + kk) if o == 'v' else (x + kk, y)
                if best is None and get(xx, yy) == '#':
                    best = (xx, yy)
        if not best:
            print("no wall for gate", d, file=sys.stderr)
            continue
        x, y = best
        if o == 'v':
            for k in (0, 1):
                put(x + k, y, d)
                for dy in (-1, 1):
                    if get(x + k, y + dy) in ('#', 'h'):
                        put(x + k, y + dy, '.')
            for k in (-1, 2):
                put(x + k, y, '#')
        else:
            for k in (0, 1):
                put(x, y + k, d)
                for dx in (-1, 1):
                    if get(x + dx, y + k) in ('#', 'h'):
                        put(x + dx, y + k, '.')
            for k in (-1, 2):
                put(x, y + k, '#')
        if len(road) > 1 and d != '1':
            street(road, 2, '_', only=(' ', ',', '_', 'h'))
    street([(12, 26), (-2, 24)], 2, '_', only=(' ', ',', '_', 'h'))      # west from Charing Cross
    street([(71, 80), (70, 92)], 2, '_', only=(' ', ',', '_', 'h', '.'))  # south, towards Canterbury
    # the Strand and Fleet Street become streets again where the roads crossed them
    street([(42, 33), (30, 32), (20, 28), (13, 26)], 2, '.', only=('_', 'h'))

    # towers on London Wall
    count = 0
    for y in range(H):
        for x in range(W):
            if g[y][x] != '#':
                continue
            if any(get(x + dx, y + dy).isdigit() for dx in range(-3, 4) for dy in range(-3, 4)):
                continue
            count += 1
            if count % 8 == 0:
                put(x, y, 'O')

    # lanes through the blocks
    def components(ok):
        seen, comps = set(), []
        for y in range(H):
            for x in range(W):
                if (x, y) in seen or not ok(x, y):
                    continue
                comp, stack = [], [(x, y)]
                seen.add((x, y))
                while stack:
                    cx, cy = stack.pop()
                    comp.append((cx, cy))
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        q = (cx + dx, cy + dy)
                        if q not in seen and ok(*q):
                            seen.add(q)
                            stack.append(q)
                comps.append(comp)
        return comps

    def split(comp, depth=0):
        xs = [p[0] for p in comp]
        ys = [p[1] for p in comp]
        w, h = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
        if (w <= 10 and h <= 10) or depth > 12:
            return
        cs = set(comp)
        if w >= h:
            cut = min(xs) + w // 2 + rnd.randint(-2, 2)
            line = {(cut, y) for y in range(min(ys), max(ys) + 1)}
        else:
            cut = min(ys) + h // 2 + rnd.randint(-2, 2)
            line = {(x, cut) for x in range(min(xs), max(xs) + 1)}
        for (x, y) in line:
            if (x, y) in cs:
                put(x, y, '.')
        rest = cs - line
        for sub in components(lambda x, y: (x, y) in rest):
            split(sub, depth + 1)

    for comp in components(lambda x, y: get(x, y) == 'h'):
        split(comp)

    # wells, market stalls, gardens and trees
    for (x, y) in ((46, 13), (64, 25), (70, 62), (20, 24)):
        if get(x, y) in (':', '.'):
            put(x, y, 'o')
    for y in range(11, 17):
        for x in range(42, 51):
            if get(x, y) == ':' and rnd.random() < 0.2 and (x + y) % 2 == 0:
                put(x, y, 'm')
    for y in range(H):
        for x in range(W):
            if get(x, y) == ' ' and rnd.random() < 0.02:
                put(x, y, 't')
    return double(g)


LONDON_HEADER = """\
# London in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py; this file is now the master copy.
# Characters as in paris.map; the capital letters are landmarks:

style london
ground 1.2
towerroof flat

legend P cathedral "Old St Paul's"                westtower=no crossing=tower stone=C9C2B0
legend T fortress  "the Tower of London"          height=6.5 keep=white
legend Q bridgehouses "London Bridge"
legend H hall      "the Guildhall"                nocampanile
legend X palace    "the Royal Exchange"           depth=4 storeys=2 clock
legend I inn       "the Mermaid Tavern"
legend J inn       "the Tabard"
legend C church    "a parish church"              tower=battlement stone=A49C8C names=St_Bartholomew-the-Great|St_Giles_Cripplegate|St_Mary-le-Bow|St_Dunstan-in-the-West|St_Bride's|St_Olave_Hart_Street|St_Magnus_the_Martyr
legend S church    "St Saviour's, Southwark"      tower=battlement stone=A8A090
legend W mansion   "Winchester Palace"            style=londonbrick
legend G theatre   "the Globe"                    flag
legend B theatre   "the Bear Garden"              flag
legend A cathedral "Westminster Abbey"            towerheight=low stone=CFC6B2
legend V hall      "Westminster Hall"             nocampanile
legend D mansion   "the Banqueting House"         style=palladian
legend Y palace    "the Palace of Whitehall"      depth=6 storeys=2 brick
legend M mansion   "a great house"                style=londonbrick names=Somerset_House|Arundel_House

gate 1 "Ludgate"
gate 2 "Newgate"
gate 3 "Aldersgate"
gate 4 "Cripplegate"
gate 5 "Moorgate"
gate 6 "Bishopsgate"
gate 7 "Aldgate"
gate 9 "the Great Stone Gate"

map
"""


# ---------------------------------------------------------------- helpers for the later towns

class Draft:
    """A coarse grid (doubled at the end) with the tools the town plans use."""

    def __init__(self, W, H, seed):
        self.W, self.H = W, H
        self.g = [[' '] * W for _ in range(H)]
        self.rnd = random.Random(seed)

    def put(self, x, y, c):
        if 0 <= x < self.W and 0 <= y < self.H:
            self.g[y][x] = c

    def get(self, x, y):
        return self.g[y][x] if 0 <= x < self.W and 0 <= y < self.H else '?'

    @staticmethod
    def near(x, y, line):
        """within a polyline whose points carry a half-width: [(x, y, w), ...]"""
        for (ax, ay, aw), (bx, by, bw) in zip(line, line[1:]):
            dx, dy = bx - ax, by - ay
            L2 = dx * dx + dy * dy or 1e-9
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / L2))
            if math.hypot(x - (ax + t * dx), y - (ay + t * dy)) <= aw + (bw - aw) * t:
                return True
        return False

    def water(self, line, only=None):
        for y in range(self.H):
            for x in range(self.W):
                if self.near(x, y, line) and (only is None or self.g[y][x] in only):
                    self.g[y][x] = '~'

    @staticmethod
    def inpoly(x, y, poly):
        inside, n = False, len(poly)
        for i in range(n):
            (ax, ay), (bx, by) = poly[i], poly[(i + 1) % n]
            if (ay > y) != (by > y) and x < ax + (y - ay) * (bx - ax) / (by - ay):
                inside = not inside
        return inside

    def rect(self, x0, y0, x1, y1, ch, only=None):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                c = self.get(x, y)
                if c == '?' or (only and c not in only):
                    continue
                self.put(x, y, ch)

    def street(self, points, width=2, ch='.', only=None, bridge=True):
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            n = max(abs(x1 - x0), abs(y1 - y0)) * 2 + 1
            for i in range(n + 1):
                t = i / n
                x, y = round(x0 + (x1 - x0) * t), round(y0 + (y1 - y0) * t)
                for k in range(width):
                    for j in range(width):
                        c = self.get(x + k, y + j)
                        if c == '?' or c == '#' or c == 'O' or c == '=':
                            continue
                        if c == '~':
                            if bridge:
                                self.put(x + k, y + j, '=')
                            continue
                        if only and c not in only:
                            continue
                        self.put(x + k, y + j, ch)

    def walled(self, inside, wall=True, fill='h'):
        """houses wherever inside(x, y) holds (but not on water), a town wall round the edge"""
        ins = [[inside(x, y) for x in range(self.W)] for y in range(self.H)]
        for y in range(self.H):
            for x in range(self.W):
                if not ins[y][x] or self.g[y][x] == '~':
                    continue
                self.g[y][x] = fill
                if not wall:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if self.get(nx, ny) == '~':
                        continue
                    if not (0 <= nx < self.W and 0 <= ny < self.H) or not ins[ny][nx]:
                        self.g[y][x] = '#'
                        break

    def inner_streets(self, river=True):
        """a street just inside the wall, and along the water (quays)"""
        for y in range(self.H):
            for x in range(self.W):
                if self.g[y][x] != 'h':
                    continue
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        c = self.get(x + dx, y + dy)
                        if c == '#' or (river and c == '~' and (dx == 0 or dy == 0)):
                            self.g[y][x] = '.'

    def ring(self, chs):
        """a street round each landmark"""
        for y in range(self.H):
            for x in range(self.W):
                if self.get(x, y) not in chs:
                    continue
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        if self.get(x + dx, y + dy) == 'h':
                            self.put(x + dx, y + dy, '.')

    def gates(self, gates):
        """gates: (x, y, digit, 'h' (a road east-west through a wall running north-south) or 'v', road points)"""
        for (x, y, d, o, road) in gates:
            best = None
            for k in range(0, 5):
                for kk in (k, -k):
                    xx, yy = (x, y + kk) if o == 'v' else (x + kk, y)
                    if best is None and self.get(xx, yy) == '#':
                        best = (xx, yy)
            if not best:
                print("no wall for gate", d, file=sys.stderr)
                continue
            x, y = best
            if o == 'v':
                for k in (0, 1):
                    self.put(x + k, y, d)
                    for dy in (-1, 1):
                        if self.get(x + k, y + dy) in ('#', 'h'):
                            self.put(x + k, y + dy, '.')
                for k in (-1, 2):
                    if self.get(x + k, y) not in ('~',):
                        self.put(x + k, y, '#')
            else:
                for k in (0, 1):
                    self.put(x, y + k, d)
                    for dx in (-1, 1):
                        if self.get(x + dx, y + k) in ('#', 'h'):
                            self.put(x + dx, y + k, '.')
                for k in (-1, 2):
                    if self.get(x, y + k) not in ('~',):
                        self.put(x, y + k, '#')
            if len(road) > 1:
                self.street(road, 2, '_', only=(' ', ',', '_', 'h', 't'))

    def towers(self, every=8, keep=3):
        count = 0
        for y in range(self.H):
            for x in range(self.W):
                if self.g[y][x] != '#':
                    continue
                if any(self.get(x + dx, y + dy).isdigit() for dx in range(-keep, keep + 1) for dy in range(-keep, keep + 1)):
                    continue
                count += 1
                if count % every == 0:
                    self.put(x, y, 'O')

    def components(self, ok):
        seen, comps = set(), []
        for y in range(self.H):
            for x in range(self.W):
                if (x, y) in seen or not ok(x, y):
                    continue
                comp, stack = [], [(x, y)]
                seen.add((x, y))
                while stack:
                    cx, cy = stack.pop()
                    comp.append((cx, cy))
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        q = (cx + dx, cy + dy)
                        if q not in seen and ok(*q):
                            seen.add(q)
                            stack.append(q)
                comps.append(comp)
        return comps

    def lanes(self, size=10):
        """cut blocks of houses larger than size x size with lanes"""
        def split(comp, depth=0):
            xs = [p[0] for p in comp]
            ys = [p[1] for p in comp]
            w, h = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
            if (w <= size and h <= size) or depth > 12:
                return
            cs = set(comp)
            if w >= h:
                cut = min(xs) + w // 2 + self.rnd.randint(-2, 2)
                line = {(cut, y) for y in range(min(ys), max(ys) + 1)}
            else:
                cut = min(ys) + h // 2 + self.rnd.randint(-2, 2)
                line = {(x, cut) for x in range(min(xs), max(xs) + 1)}
            for (x, y) in line:
                if (x, y) in cs:
                    self.put(x, y, '.')
            rest = cs - line
            for sub in self.components(lambda x, y: (x, y) in rest):
                split(sub, depth + 1)
        for comp in self.components(lambda x, y: self.get(x, y) == 'h'):
            split(comp)

    def trees(self, p=0.02, ch=' '):
        for y in range(self.H):
            for x in range(self.W):
                if self.get(x, y) == ch and self.rnd.random() < p:
                    self.put(x, y, 't')

    def stalls(self, x0, y0, x1, y1, p=0.2):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if self.get(x, y) == ':' and self.rnd.random() < p and (x + y) % 2 == 0:
                    self.put(x, y, 'm')

    def connect(self, avoid=None, gap=2):
        """Bridge small pockets of streets cut off by a narrow canal: a straight bridge over at
        most `gap` cells of water (houses in the way become a lane), but never across `avoid`
        (a canal given as a polyline, the Grand Canal)."""
        walk = set('.:,;=otmjsfb')
        for _ in range(40):
            comps = self.components(lambda x, y: self.get(x, y) in walk)
            if len(comps) < 2:
                return
            comps.sort(key=len, reverse=True)
            main = set(comps[0])
            done = False
            for comp in comps[1:]:
                if len(comp) > 60:   # an island, or the mainland: by boat
                    continue
                best = None
                for (x, y) in comp:
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        path, wet = [], 0
                        for k in range(1, gap + 3):
                            q = (x + dx * k, y + dy * k)
                            if q in main:
                                if path and wet <= gap and (best is None or (wet, len(path)) < best[0]):
                                    best = ((wet, len(path)), path)
                                break
                            c = self.get(*q)
                            if c == '~' and not (avoid and self.near(q[0], q[1], [(a, b, w + 0.4) for (a, b, w) in avoid])):
                                wet += 1
                            elif c != 'h':
                                break
                            path.append(q)
                if best:
                    for q in best[1]:
                        self.put(q[0], q[1], '=' if self.get(*q) == '~' else '.')
                    done = True
                    break
            if not done:
                return

    def done(self):
        return double(self.g)


# ---------------------------------------------------------------- Florence

def florence():
    d = Draft(90, 78, 1610)
    # the Arno, from the east to the west
    ARNO = [(94, 40, 2.0), (78, 41.5, 2.1), (60, 44, 2.2), (44, 45.5, 2.3), (26, 46, 2.3), (10, 46.5, 2.2), (-3, 47, 2.1)]
    d.water(ARNO)
    # the walls of 1333, round both banks; the Belvedere on its hill at the southern tip
    OUTLINE = [(6, 45), (3, 34), (5, 17), (15, 4), (40, 2), (64, 5), (79, 15), (86, 29), (88, 39),
               (82, 49), (74, 53), (70, 62), (65, 74), (55, 75), (50, 69), (40, 72), (28, 70), (18, 64), (8, 57), (4, 49)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, OUTLINE))
    d.inner_streets()
    # streets
    d.street([(44, 23), (44, 30)])            # Via dei Calzaiuoli
    d.street([(44, 36), (43, 42)])            # Por Santa Maria, to the Ponte Vecchio
    d.street([(30, 22), (31, 42)])            # Via de' Tornabuoni
    d.street([(40, 3), (41, 14)])             # Via San Gallo
    d.street([(46, 3), (47, 14)])             # Via Larga
    d.street([(58, 23), (76, 26), (86, 29)])  # Borgo degli Albizi, Borgo la Croce
    d.street([(4, 34), (18, 25)])             # Via della Scala, from the Porta al Prato
    d.street([(6, 39), (26, 41)])             # Borgo Ognissanti
    d.street([(52, 38), (60, 36)])            # Borgo de' Greci, to Santa Croce
    d.street([(43, 49), (40, 53)])            # Via Guicciardini, to the Pitti
    d.street([(38, 61), (40, 71)])            # Via Romana
    d.street([(31, 49), (33, 54)])            # Via Maggio
    d.street([(8, 56), (24, 50)])             # Borgo San Frediano
    d.street([(47, 49), (72, 51)])            # Via de' Bardi, San Niccolò
    d.street([(20, 8), (36, 9)])
    # squares
    d.rect(32, 14, 58, 23, ':', only=('h', '.'))   # around the Duomo
    d.rect(37, 27, 47, 36, ':', only=('h', '.'))   # Piazza della Signoria
    d.rect(55, 27, 61, 37, ':', only=('h', '.'))   # Piazza Santa Croce
    d.rect(18, 19, 29, 25, ':', only=('h', '.'))   # Piazza Santa Maria Novella
    d.rect(48, 4, 58, 13, ':', only=('h', '.'))    # Piazza della Santissima Annunziata
    d.rect(29, 50, 47, 53, ':', only=('h', '.'))   # Piazza Pitti
    d.rect(19, 57, 28, 60, ':', only=('h', '.'))   # Piazza Santo Spirito
    d.rect(33, 26, 38, 28, ':', only=('h', '.'))   # the Mercato Vecchio
    d.stalls(33, 26, 38, 28, 0.35)
    # landmarks
    d.rect(42, 15, 57, 21, 'D')               # Santa Maria del Fiore
    d.rect(39, 22, 41, 24, 'C')               # Giotto's campanile
    d.rect(33, 15, 37, 19, 'B')               # the Baptistery
    d.rect(46, 30, 52, 35, 'V')               # the Palazzo Vecchio
    d.rect(47, 28, 48, 29, 'T')               # its tower
    d.rect(40, 35, 44, 36, 'L')               # the Loggia dei Lanzi
    d.rect(41, 37, 43, 42, 'U')               # the Uffizi: the west wing
    d.rect(48, 37, 50, 42, 'U')               # the east wing (not touching the west one)
    d.rect(62, 28, 73, 34, 'S')               # Santa Croce
    d.rect(19, 13, 28, 18, 'N')               # Santa Maria Novella
    d.rect(31, 7, 39, 11, 'Z')                # San Lorenzo
    d.rect(41, 7, 45, 11, 'M')                # the Palazzo Medici
    d.rect(51, 1, 57, 3, 'A') if False else d.rect(52, 5, 58, 8, 'A')   # Santissima Annunziata
    d.rect(7, 5, 15, 13, 'F')                 # the Fortezza da Basso
    d.rect(33, 22, 37, 25, 'P')               # Landini's press, by the Duomo
    d.rect(24, 33, 28, 36, 'I')               # an inn
    d.rect(30, 54, 46, 59, 'X')               # the Palazzo Pitti
    d.rect(20, 52, 27, 56, 'Y')               # Santo Spirito
    d.rect(9, 51, 14, 54, 'K')                # the Carmine
    d.rect(56, 66, 63, 72, 'E')               # the Forte di Belvedere
    # the Boboli gardens behind the Pitti, up the hill
    d.rect(28, 60, 52, 69, ',', only=('h', '.', ':', ' '))
    for y in range(60, 70):
        for x in range(28, 53):
            if d.get(x, y) == ',' and d.rnd.random() < 0.22:
                d.put(x, y, 't')
            elif d.get(x, y) == ',' and (x == 40 or y == 64):
                d.put(x, y, '%' if (x + y) % 5 else ',')
    d.ring('DCBVTLUSNZMAFPIXYKE')
    # the bridges: the Ponte Vecchio with its goldsmiths' shops, Santa Trinita, la Carraia, alle Grazie
    for y in range(40, 52):
        for x in range(41, 47):
            if d.get(x, y) == '~':
                d.put(x, y, '=')
    for y in range(40, 52):
        for x in (41, 42, 45, 46):             # shops two cells (four tiles) deep, as on London Bridge
            if d.get(x, y) == '=':
                d.put(x, y, 'Q')
    d.street([(31, 42), (31, 50)])
    d.street([(21, 42), (21, 50)])
    d.street([(58, 40), (58, 49)])
    # statues, the fountain of Neptune, wells
    d.put(39, 31, 's')                        # Cosimo I on horseback (Giambologna)
    d.put(45, 31, 'y')                        # Michelangelo's David, before the Palazzo Vecchio
    d.put(40, 34, 'y')                        # Cellini's Perseus, at the front of the Loggia
    d.put(43, 34, 'y')                        # Giambologna's Sabine women, beside it
    d.put(45, 33, 'f') if d.get(45, 33) == ':' else None
    d.put(44, 34, 'f') if d.get(44, 34) == ':' else None
    for (x, y) in ((50, 11), (58, 32), (24, 22), (23, 58), (34, 12)):
        if d.get(x, y) in (':', '.'):
            d.put(x, y, 'o')
    # gates, and the roads leaving them
    d.gates([(3, 34, '1', 'h', [(1, 34), (-2, 34)]),
             (40, 2, '2', 'v', [(40, 0), (40, -2)]),
             (64, 5, '3', 'v', [(64, 3), (66, -2)]),
             (87, 29, '4', 'h', [(89, 29), (92, 30)]),
             (81, 50, '5', 'h', [(84, 52), (92, 54)]),
             (40, 72, '6', 'v', [(40, 74), (39, 80)]),
             (7, 56, '7', 'h', [(5, 57), (-2, 58)])])
    d.towers(7)
    d.lanes(9)
    d.trees(0.03)
    return d.done()


FLORENCE_HEADER = """\
# Florence in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py; this file is now the master copy.
# Characters as in paris.map; the capital letters are landmarks:

style florence
ground 1.2
towerroof flat

legend D cathedral "Santa Maria del Fiore"        crossing=dome domeat=0.72 domescale=1.15 domeheight=1.4 ribs=8 sides=8 dome=B5553C roof=tile front=flat stone=ECE6D8
legend C tower     "Giotto's campanile"           height=17 top=flat stone=ECE6D8
legend B rotunda   "the Baptistery of San Giovanni" sides=8 roof=pyramid height=6 stone=E6E0D2 dome=E0DBD0
legend V hall      "the Palazzo Vecchio"          roof=tile battlements stone=B59B72
legend T tower     "the tower of the Palazzo Vecchio" height=18 top=battlement stone=B59B72
legend L halle     "the Loggia dei Lanzi"         nostalls
legend U gallery   "the Uffizi"                   roof=tile stone=D9CFBC
legend S church    "Santa Croce"                  roof=tile front=flat stone=D8CBB4
legend N church    "Santa Maria Novella"          roof=tile front=flat stone=ECE6D8
legend Z church    "San Lorenzo"                  roof=tile front=flat crossing=dome domescale=0.8 dome=B5553C stone=C9B48A
legend M palace    "the Palazzo Medici"           depth=4 storeys=3 roof=tile stone=B59B72
legend A church    "Santissima Annunziata"        roof=tile front=flat stone=DDD4C2
legend F fortress  "the Fortezza da Basso"        height=6 stone=B39C7A
legend P mansion   "Landini's printing house"     style=florence  sign=book
legend I inn       "the Osteria della Corona"
legend X palace    "the Palazzo Pitti"            depth=5 storeys=3 open=south roof=tile stone=B8A27C
legend Y church    "Santo Spirito"                roof=tile front=flat stone=E8E0CC
legend K church    "Santa Maria del Carmine"      roof=tile front=flat stone=C9B48A
legend E fortress  "the Forte di Belvedere"       height=6 stone=C2AE88
legend Q bridgehouses "the Ponte Vecchio"           shops

gate 1 "the Porta al Prato"
gate 2 "the Porta San Gallo"
gate 3 "the Porta a Pinti"
gate 4 "the Porta alla Croce"
gate 5 "the Porta San Niccolò"
gate 6 "the Porta Romana"
gate 7 "the Porta San Frediano"

map
"""


# ---------------------------------------------------------------- Rome

def rome():
    d = Draft(110, 100, 1623)
    TIBER = [(26, -3, 2.2), (28, 8, 2.2), (33, 16, 2.3), (31, 24, 2.3), (32, 32, 2.3), (37, 40, 2.4), (42, 48, 2.4),
             (43, 58, 2.4), (40, 70, 2.4), (41, 82, 2.5), (43, 103, 2.5)]
    d.water(TIBER)
    # the Aurelian walls, the walls of Trastevere and the Janiculum, and the Leonine walls round the Vatican
    OUTLINE = [(1, 5), (16, 2), (30, 1), (46, 1), (58, 2), (74, 4), (86, 8), (98, 14), (106, 30), (106, 46), (104, 62),
               (96, 84), (84, 97), (66, 98), (52, 97), (46, 90), (42, 84), (35, 77), (24, 66), (16, 56), (18, 44),
               (24, 38), (14, 37), (2, 31)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, OUTLINE), fill=',')
    # where people live: the Borgo, the Campo Marzio in the bend of the river, Monti, Trastevere
    BORGO = [(12, 3), (30, 4), (30, 37), (13, 36)]
    MARZIO = [(31, 2), (48, 3), (56, 8), (62, 16), (72, 24), (82, 30), (84, 44), (70, 48), (60, 52), (50, 50),
              (44, 48), (36, 42), (33, 30), (31, 18)]
    TRASTEVERE = [(19, 40), (36, 40), (42, 48), (42, 62), (34, 68), (24, 64), (19, 54)]
    for y in range(d.H):
        for x in range(d.W):
            if d.get(x, y) == ',' and any(d.inpoly(x + 0.5, y + 0.5, P) for P in (BORGO, MARZIO, TRASTEVERE)):
                d.put(x, y, 'h')
    d.inner_streets()
    # the island in the river, with its two bridges
    d.rect(41, 46, 44, 50, 'h', only=('~',))
    d.rect(41, 46, 44, 46, '.', only=('h',))
    # streets
    d.street([(41, 6), (50, 40)])          # the Corso
    d.street([(38, 7), (35, 20)])          # Via di Ripetta
    d.street([(43, 7), (52, 13)])          # Via del Babuino
    d.street([(36, 20), (46, 22), (60, 26)])
    d.street([(34, 26), (34, 44)])         # by Navona and the Campo de' Fiori
    d.street([(50, 40), (62, 40), (76, 34), (86, 32)])   # to Santa Maria Maggiore
    d.street([(56, 46), (72, 52), (88, 60), (102, 64)])  # past the Colosseum to the Lateran
    d.street([(86, 34), (104, 44)])        # to the Porta Maggiore
    d.street([(78, 62), (82, 96)])         # the Via Appia, to the Porta San Sebastiano
    d.street([(46, 52), (52, 96)])         # to the Porta San Paolo
    d.street([(60, 26), (80, 8)])          # Via Pia
    d.street([(18, 20), (30, 20)])         # the Borgo, to Castel Sant'Angelo
    d.street([(30, 20), (37, 19)])         # Ponte Sant'Angelo
    d.street([(34, 44), (44, 44)])         # Ponte Sisto
    d.street([(38, 44), (30, 58), (20, 54)])
    d.street([(44, 48), (40, 48)], 1)      # Ponte Cestio
    d.street([(44, 48), (47, 48)], 1)      # Ponte Fabricio
    # squares
    d.rect(17, 20, 23, 30, ':', only=('h', '.', ','))   # before St Peter's
    d.rect(37, 3, 44, 7, ':', only=('h', '.', ','))     # Piazza del Popolo
    d.rect(49, 12, 54, 16, ':', only=('h', '.'))        # Piazza di Spagna
    d.rect(43, 21, 48, 23, ':', only=('h', '.'))        # Piazza Colonna
    d.rect(41, 22, 46, 24, ':', only=('h', '.'))        # before the Pantheon
    d.rect(35, 24, 37, 36, ':', only=('h', '.'))        # Piazza Navona, long as the old circus
    d.rect(32, 38, 36, 41, ':', only=('h', '.'))        # Campo de' Fiori
    d.rect(52, 43, 58, 45, ':', only=('h', '.', ','))   # the Capitol
    d.rect(29, 51, 36, 52, ':', only=('h', '.'))        # Santa Maria in Trastevere
    d.stalls(32, 38, 36, 41, 0.7)            # the Campo de' Fiori
    d.stalls(35, 24, 37, 36, 0.45)           # the market in Piazza Navona
    # the Ripa Grande, the river port below Trastevere: boats tied up along the west bank
    for y in (58, 60, 62, 64):
        for x in range(34, 46):
            if d.get(x, y) == '~':
                d.put(x, y, 'g')
                break
    # the Forum, the Campo Vaccino: grass and broken columns; the Circus Maximus
    d.rect(56, 47, 70, 56, ';', only=(',', 'h', '.'))
    for (x, y) in ((58, 49), (60, 52), (63, 50), (66, 53), (68, 49), (61, 55)):
        d.put(x, y, 'j')
    # landmarks
    d.rect(3, 20, 16, 30, 'P')             # St Peter's
    d.rect(6, 6, 20, 16, 'V')              # the Vatican Palace
    d.rect(8, 17, 14, 19, 'S')             # the Sistine Chapel
    d.rect(23, 15, 29, 21, 'A')            # Castel Sant'Angelo
    d.rect(44, 3, 48, 6, 'M')              # Santa Maria del Popolo
    d.rect(50, 4, 56, 8, 'D')              # the Villa Medici
    d.rect(55, 10, 60, 13, 'T')            # Trinità dei Monti
    d.rect(62, 18, 70, 24, 'B')            # the Palazzo Barberini, building
    d.rect(60, 26, 72, 32, 'Q')            # the Quirinal Palace
    d.rect(41, 25, 45, 31, 'R')            # the Pantheon
    d.rect(47, 28, 51, 31, 'N')            # Santa Maria sopra Minerva
    d.rect(52, 27, 57, 33, 'C')            # the Collegio Romano
    d.rect(38, 32, 43, 37, 'W')            # the Sapienza, south of the Pantheon
    d.rect(29, 42, 34, 46, 'F')            # the Palazzo Farnese
    d.rect(48, 34, 53, 38, 'G')            # the Gesù
    d.rect(51, 39, 56, 42, 'Z')            # the Palazzo Venezia
    d.rect(52, 46, 58, 48, 'K')            # the Palazzo Senatorio, on the Capitol
    d.rect(72, 52, 82, 60, 'X')            # the Colosseum
    d.rect(80, 30, 88, 35, 'E')            # Santa Maria Maggiore
    d.rect(88, 62, 100, 68, 'L')           # San Giovanni in Laterano
    d.rect(90, 55, 98, 60, 'Y')            # the Lateran Palace
    d.rect(30, 53, 35, 56, 'U')            # Santa Maria in Trastevere
    d.rect(20, 50, 24, 53, 'H')            # San Pietro in Montorio
    d.rect(44, 70, 49, 73, 'J')            # Santa Sabina
    d.rect(47, 91, 50, 94, 'q')            # the Pyramid of Cestius (a lowercase letter: the capitals are all taken)
    d.rect(35, 18, 39, 21, 'I')            # the Osteria dell'Orso
    d.ring('PVSAMDTBQRNCWFGZKXELYUHJIq')
    # obelisks, columns, fountains, statues
    d.put(20, 25, 'i')                     # the Vatican obelisk, raised by Fontana in 1586
    d.put(40, 5, 'i')                      # Piazza del Popolo
    d.put(89, 32, 'i') if d.get(89, 32) in ('.', ':', ',', 'h') else None
    d.put(87, 63, 'i') if d.get(87, 63) in ('.', ':', ',', 'h', ';') else None
    d.put(45, 22, 'j')                     # the column of Marcus Aurelius
    d.put(58, 39, 'j') if d.get(58, 39) in ('.', ':', 'h', ',') else None   # Trajan's column
    d.put(55, 44, 's')                     # Marcus Aurelius on horseback, on the Capitol
    d.put(36, 27, 'f'); d.put(36, 34, 'f')  # the fountains of Piazza Navona
    d.put(35, 32, 'y')                     # Pasquino, at the south-west corner of the Piazza Navona
    d.put(51, 14, 'f')                     # the Barcaccia (Pietro Bernini, 1629)
    d.put(42, 23, 'f') if d.get(42, 23) == ':' else None
    d.put(32, 52, 'f') if d.get(32, 52) == ':' else None
    # gates
    d.gates([(40, 1, '1', 'v', [(40, -1), (41, -3)]),       # the Porta del Popolo, the Via Flaminia
             (80, 7, '2', 'v', [(81, 5), (84, -2)]),        # the Porta Pia
             (105, 45, '3', 'h', [(107, 45), (112, 46)]),   # the Porta Maggiore
             (103, 64, '4', 'h', [(105, 65), (112, 68)]),   # the Porta San Giovanni
             (82, 96, '5', 'v', [(83, 98), (86, 103)]),     # the Porta San Sebastiano, the Via Appia
             (52, 97, '6', 'v', [(52, 99), (50, 103)]),     # the Porta San Paolo
             (17, 55, '7', 'h', [(15, 55), (-2, 56)]),      # the Porta San Pancrazio
             (14, 2, '8', 'v', [(14, 0), (12, -3)])])       # the Porta Angelica
    d.towers(7)
    d.lanes(9)
    # the disabitato: vineyards and orchards inside the walls
    for y in range(d.H):
        for x in range(d.W):
            if d.get(x, y) == ',' and d.rnd.random() < 0.16:
                d.put(x, y, 't')
    d.trees(0.02)
    return d.done()


ROME_HEADER = """\
# Rome in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py; this file is now the master copy.
# Characters as in paris.map; the capital letters are landmarks:

style rome
ground 1.2
towerroof flat

legend P cathedral "the Basilica of St Peter"    faces=east front=flat frontheight=2.6 norose crossing=dome altar=crossing domeat=0.68 domescale=1.2 domeheight=1.05 sides=16 ribs=16 dome=8E959A roof=lead stone=ECE6D6
legend V palace    "the Vatican Palace"          depth=5 storeys=3 roof=tile stone=E2D6BC
legend S chapel    "the Sistine Chapel"          plain height=9 stone=C9A27E
legend A rotunda   "Castel Sant'Angelo"          fortress roof=battlement sides=20 height=6.5 stone=B8A888
legend M church    "Santa Maria del Popolo"      roof=tile front=flat stone=E4DCC8
legend D mansion   "the Villa Medici"            style=rome
legend T church    "Trinità dei Monti"           towers roof=tile stone=EAE2CE
legend B palace    "the Palazzo Barberini"       depth=4 storeys=3 open=east roof=tile stone=E0CFAE
legend Q palace    "the Quirinal Palace"         depth=5 storeys=3 roof=tile stone=E0C49A
legend R rotunda   "the Pantheon"                portico=north sides=24 height=6 domeheight=0.5 dome=8E959A stone=C8BCA4
legend N church    "Santa Maria sopra Minerva"   roof=tile front=flat stone=E8E0CC
legend C college   "the Collegio Romano"         depth=4 storeys=3 roof=tile stone=E6D2AA
legend W college   "the Sapienza"                depth=4 storeys=2 roof=tile stone=E3CBA0
legend F palace    "the Palazzo Farnese"         depth=5 storeys=3 roof=tile stone=E3C397
legend G church    "the Gesù"                    roof=tile front=flat crossing=dome domescale=0.8 dome=8E959A stone=E6DCC4
legend Z fortress  "the Palazzo Venezia"         height=5.5 stone=C9B892
legend K hall      "the Palazzo Senatorio"       roof=tile stone=E6DAC0
legend X arena     "the Colosseum"               height=9 ruined=0.4 stone=C9B48A
legend E church    "Santa Maria Maggiore"        roof=tile front=flat stone=E8DCC4
legend L cathedral "San Giovanni in Laterano"    faces=east front=flat roof=tile stone=E6DCC6
legend Y palace    "the Lateran Palace"          depth=4 storeys=3 roof=tile stone=E2D4B6
legend U church    "Santa Maria in Trastevere"   roof=tile front=flat stone=D8C9AC
legend H church    "San Pietro in Montorio"      roof=tile front=flat stone=E8E0CC
legend J church    "Santa Sabina"                roof=tile front=flat stone=D8CCB6
legend I inn       "the Osteria dell'Orso"
legend q tower     "the Pyramid of Cestius"      height=0.3 top=spire roof=EEEAE2 stone=EEEAE2

gate 1 "the Porta del Popolo"
gate 2 "the Porta Pia"
gate 3 "the Porta Maggiore"
gate 4 "the Porta San Giovanni"
gate 5 "the Porta San Sebastiano"
gate 6 "the Porta San Paolo"
gate 7 "the Porta San Pancrazio"
gate 8 "the Porta Angelica"

map
"""


# ---------------------------------------------------------------- Amsterdam

def amsterdam():
    """Amsterdam in 1632: the old town along the Damrak and the Amstel, and the new canal ring
    on the west, dug from 1613, from the IJ round to the Leidsegracht (the rest came after 1658).
    The ring is one curve about a point on the IJ, scaled outwards: Singel 1, Herengracht 1.22,
    Keizersgracht 1.44, Prinsengracht 1.66, the new walls 1.95; in the east the old moat at 0.95
    and the walls at 1.3."""
    d = Draft(104, 78, 1632)
    CX, CY = 58.0, 14.0
    def R(phi):   # the Singel's distance from the centre, at angle phi (degrees; 180 west, 90 south)
        return 28 + 4 * (180 - phi) / 90 if phi >= 90 else 32.0
    def P(phi, k):
        a = math.radians(phi)
        return (CX + k * R(phi) * math.cos(a), CY + k * R(phi) * math.sin(a))
    def polar(x, y):
        dx, dy = x + 0.5 - CX, y + 0.5 - CY
        phi = math.degrees(math.atan2(dy, dx)) % 360
        return phi, math.hypot(dx, dy) / R(phi if phi <= 180 else 0)
    def wall_k(phi):
        if phi >= 130: return 1.95
        if phi >= 108: return 1.3 + 0.65 * (phi - 108) / 22
        return 1.3
    # the IJ along the north
    d.rect(0, 0, d.W - 1, 13, '~')
    inside = lambda x, y: y >= 14 and polar(x, y)[0] <= 180 and polar(x, y)[1] <= wall_k(polar(x, y)[0])
    d.walled(inside)
    # the canals of the ring: arcs from the IJ (west) to the Leidsegracht; the Singel runs on to the Munt
    def arc(k, phi0, phi1, w=0.7):
        pts = [P(phi, k) + (w,) for phi in range(int(phi1), int(phi0) - 1, -3)]
        d.water(pts)
    arc(1.0, 87, 180)      # the Singel
    arc(1.22, 128, 180)    # the Herengracht
    arc(1.44, 128, 180)    # the Keizersgracht
    arc(1.66, 128, 180)    # the Prinsengracht
    arc(0.95, 0, 86)       # the old moat in the east: the Geldersekade and the Kloveniersburgwal
    d.water([P(128, 1.0) + (0.7,), P(128, 1.93) + (0.7,)])                  # the Leidsegracht
    for phi in (150, 162):  # the Rozengracht and the Egelantiersgracht, in the Jordaan
        d.water([P(phi, 1.66) + (0.6,), P(phi, 1.93) + (0.6,)])
    d.water([(4, 16, 0.7), (31, 16, 0.7)])                                   # the Brouwersgracht
    # the Amstel, dammed at the Dam; the Damrak to the IJ
    d.water([(60, 78, 2.0), (59, 60, 1.9), (58, 46, 1.6), (57, 38, 1.2)])
    d.water([(57, 29, 1.5), (57, 13, 1.7)])
    d.water([(62, 14, 0.6), (62, 40, 0.6)])                                  # the Oudezijds Voorburgwal
    d.water([(68, 17, 0.6), (68, 42, 0.6)])                                  # the Oudezijds Achterburgwal
    # streets, with bridges where they cross the water
    for pts in ([(54, 14), (54, 29)],                 # the Nieuwendijk
                [(60, 14), (60, 30)],                 # the Warmoesstraat
                [(55, 36), (55, 45)],                 # the Kalverstraat
                [(57, 33), (64, 33), (76, 32)],       # to the Nieuwmarkt
                [(76, 32), (82, 39), (90, 33)],       # the Breestraat, to the Sint Antoniespoort
                [(60, 46), (70, 42), (78, 36)],
                [(65, 14), (88, 16)]):                # along the harbour
        d.street(pts, 1)
    for phi in (170, 155, 140, 132, 120, 105):       # radial streets through the ring
        d.street([tuple(int(round(c)) for c in P(phi, 0.98)), tuple(int(round(c)) for c in P(phi, wall_k(phi) - 0.04))], 1)
    d.street([(50, 34), (40, 36)], 1)                 # the Raadhuisstraat way
    d.inner_streets()
    # squares
    d.rect(51, 29, 60, 35, ':', only=('h', '.'))      # the Dam
    d.rect(73, 28, 80, 34, ':', only=('h', '.'))      # the Nieuwmarkt
    d.rect(57, 45, 62, 47, ':', only=('h', '.'))      # by the Munt
    d.stalls(51, 29, 60, 35, 0.4)                     # the fish market on the Dam
    d.stalls(73, 28, 80, 34, 0.4)                     # the Nieuwmarkt
    # landmarks
    d.rect(45, 28, 50, 33, 'N')            # the Nieuwe Kerk
    d.rect(47, 34, 51, 37, 'T')            # the old Town Hall
    d.rect(61, 35, 66, 40, 'B')            # the Exchange, over the Rokin
    d.rect(63, 20, 67, 25, 'K')            # the Oude Kerk
    d.rect(87, 15, 88, 16, 'S')            # the Schreierstoren
    d.rect(74, 29, 79, 33, 'A')            # the Waag, the old Sint Antoniespoort
    d.rect(60, 43, 61, 44, 'M')            # the Munttoren
    d.rect(70, 37, 74, 41, 'Z')            # the Zuiderkerk
    d.rect(75, 39, 76, 40, 'u')            # its tower
    d.rect(64, 29, 67, 32, 'E')            # the East India House
    d.rect(63, 15, 67, 18, 'P')            # Blaeu's printing house, by the Damrak (not on it: the canal stays open)
    d.rect(80, 36, 84, 39, 'U')            # Uylenburgh's house, in the Breestraat
    d.rect(62, 47, 66, 50, 'R')            # Menasseh ben Israel's press, on Vlooienburg
    d.rect(16, 34, 21, 38, 'W')            # the Westerkerk, new last year
    d.rect(6, 21, 10, 24, 'J')             # the Noorderkerk
    d.rect(51, 24, 55, 27, 'I')            # an inn by the Dam
    d.ring('NTBKSAMZuEPURWJI')
    # gates
    gx, gy = P(176, 1.95)
    d.gates([(int(gx), int(gy), '1', 'h', [(int(gx) - 2, int(gy)), (-3, int(gy) + 2)])])
    gx, gy = P(134, 1.95)
    d.gates([(int(gx), int(gy), '2', 'v', [(int(gx), int(gy) + 2), (int(gx) - 6, d.H + 2)])])
    gx, gy = P(100, 1.33)
    d.gates([(int(gx), int(gy), '3', 'v', [(int(gx), int(gy) + 2), (int(gx) + 1, d.H + 2)])])
    gx, gy = P(30, 1.3)
    d.gates([(int(gx), int(gy), '4', 'h', [(int(gx) + 2, int(gy)), (d.W + 2, int(gy) + 4)])])
    d.towers(9)
    d.lanes(7)
    d.connect(gap=2)
    # wells, trees on the quays, boats in the harbour and on the Damrak
    for (x, y) in ((55, 32), (76, 33)):
        if d.get(x, y) == ':':
            d.put(x, y, 'o')
    for y in range(d.H):
        for x in range(d.W):
            if d.get(x, y) == '.' and any(d.get(x + a, y + b) == '~' for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))) and d.rnd.random() < 0.06:
                d.put(x, y, 't')   # the elms along the canals
    for x in range(36, 92, 5):
        if d.get(x, 13) == '~' and d.get(x, 14) in ('.', 'h', ':'):
            d.put(x, 13, 'g')
    d.put(56, 20, 'g') if d.get(56, 20) == '~' else None
    return d.done()


AMSTERDAM_HEADER = """\
# Amsterdam in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py; this file is now the master copy.
# Characters as in paris.map; the capital letters are landmarks.

style amsterdam
ground 1.0
towerroof cone

legend N church    "the Nieuwe Kerk"              brick roof=slate
legend T hall      "the Town Hall"                nocampanile brick roof=slate
legend B palace    "the Exchange"                 depth=4 storeys=2 roof=slate brick clock
legend K church    "the Oude Kerk"                brick roof=slate
legend S tower     "the Schreierstoren"           round height=5 top=pyramid brick roof=4A4440
legend A fortress  "the Waag"                     height=5 roofs roofed brick
legend M tower     "the Munttoren"                height=11 top=bulb brick roof=4A4440
legend Z church    "the Zuiderkerk"               brick roof=slate
legend u tower     "the tower of the Zuiderkerk"  height=15 top=spire brick roof=4A4440
legend E palace    "the East India House"         depth=2 storeys=3 roof=slate brick
legend P mansion   "the printing house of Willem Blaeu, at the Sign of the Gilded Sundial"  sign=book
legend U mansion   "the house of Hendrick Uylenburgh"  sign=palette
legend R mansion   "the house of Menasseh ben Israel"  sign=book
legend W church    "the Westerkerk"               brick roof=slate
legend J church    "the Noorderkerk"              brick roof=slate
legend I inn       "the Arms of Amsterdam"

gate 1 "the Haarlemmerpoort"
gate 2 "the Leidsepoort"
gate 3 "the Regulierspoort"
gate 4 "the Sint Antoniespoort"

map
"""


# ---------------------------------------------------------------- Venice

VENICE_FERRIES = []   # filled by venice(): "ferry COL ROW COL ROW KIND NAME | NAME" lines for the header


def venice():
    d = Draft(120, 90, 1632)
    def fill(poly, ch, only=None):
        for y in range(d.H):
            for x in range(d.W):
                if d.inpoly(x + 0.5, y + 0.5, poly) and (only is None or d.get(x, y) in only):
                    d.put(x, y, ch)
    # the lagoon; beyond it (' ') the country and the sea of the world map
    fill([(14, 2), (40, 1), (64, 0), (90, 1), (104, 6), (108, 14), (110, 40), (112, 90), (80, 90), (50, 88), (28, 86),
          (18, 80), (15, 70), (16, 58), (13, 52), (13, 36), (16, 26), (13, 14)], '~')
    # the mainland at Fusina
    fill([(0, 37), (13, 37), (13, 51), (0, 51)], ',')
    # the city, Murano, the Giudecca, San Giorgio, the Lido
    VENEZIA = [(30, 40), (33, 33), (40, 28), (50, 24), (62, 22), (74, 23), (86, 26), (96, 30), (103, 35), (104, 41),
               (100, 46), (93, 50), (85, 52), (79, 52), (76, 53), (73, 56), (66, 58), (56, 58), (46, 58), (38, 55), (32, 48)]
    fill(VENEZIA, 'h')
    fill([(63, 4), (80, 3), (85, 8), (79, 13), (66, 12), (62, 8)], 'h')            # Murano
    fill([(33, 62), (70, 61), (74, 63), (70, 67), (36, 67), (31, 65)], ',')         # the Giudecca: gardens
    fill([(33, 62), (70, 61), (72, 62), (34, 64)], 'h')                              # houses along its quay
    fill([(79, 55), (88, 54), (89, 59), (80, 60)], ',')                              # San Giorgio
    fill([(103, 18), (108, 18), (112, 90), (106, 90)], ',')                          # the Lido
    # the Grand Canal, and the rii
    GC = [(30, 41, 1.5), (38, 36, 1.5), (46, 32, 1.6), (54, 31, 1.6), (60, 33, 1.6), (63, 37, 1.6), (62, 41, 1.6),
          (57, 44, 1.6), (51, 46, 1.6), (50, 50, 1.7), (55, 53, 1.8), (63, 53, 2.0), (70, 52, 2.0), (78, 53, 2.4)]
    d.water(GC)
    for rio in ([(38, 36), (36, 30), (36, 25)],              # the Rio di Cannaregio
                [(44, 30), (56, 27), (68, 25), (72, 22)],    # the Misericordia
                [(85, 50), (85, 44), (84, 39), (86, 33)],    # the Rio di Palazzo, behind the Doge's Palace
                [(51, 47), (48, 52), (47, 58)],              # San Trovaso
                [(46, 46), (42, 50), (40, 56)],              # Ca' Foscari
                [(80, 31), (79, 22)],                        # the Mendicanti
                [(91, 46), (90, 37), (88, 27)],              # San Lorenzo
                [(40, 36), (44, 40), (48, 43)],              # the Rio Marin
                [(62, 8), (86, 8)]):                         # the Rio dei Vetrai, on Murano
        d.water([(x, y, 0.6) for (x, y) in rio])
    # the Rialto Bridge: the only bridge over the Grand Canal, stone since 1591, with two rows of shops
    d.rect(59, 37, 65, 42, '=')                # a rectangle, its ends resting on the banks
    d.rect(59, 37, 65, 38, 'Q')
    d.rect(59, 41, 65, 42, 'Q')
    # the calli (a street crosses a rio by a bridge)
    for pts in ([(81, 40), (74, 38), (68, 39)],                        # the Mercerie, San Marco to the Rialto
                [(57, 39), (54, 40), (52, 43), (49, 45), (46, 48), (44, 52), (42, 56)],   # to the Frari and the Zattere
                [(67, 36), (66, 31), (60, 28), (50, 27), (43, 28), (38, 31), (35, 34)],   # across Cannaregio
                [(60, 28), (60, 22)],                                  # to the Fondamenta Nuove
                [(81, 40), (86, 38), (92, 37)],                        # to the Arsenale
                [(80, 37), (82, 30), (76, 27), (70, 26)],              # to Santi Giovanni e Paolo
                [(52, 42), (46, 40), (40, 40), (34, 42)],              # Santa Croce
                [(46, 48), (47, 49), (48, 54), (56, 56), (63, 56)],    # Dorsoduro, to the Salute
                [(72, 4), (72, 12)],                                   # Murano
                [(64, 6), (64, 10)]):                                  # over the Rio dei Vetrai, west of the glassworks
        d.street(pts, 1)
    d.street([(84, 51), (93, 49), (101, 44)], 2)                      # the Riva degli Schiavoni
    d.street([(53, 39), (59, 39)], 2)                                 # to the Rialto Bridge, clear at both ends
    d.street([(66, 39), (70, 39)], 2)
    d.inner_streets()                                                  # fondamente along the water
    # squares
    d.rect(68, 40, 80, 44, ':', only=('h', '.'))                      # the Piazza San Marco
    d.rect(80, 45, 83, 51, ':', only=('h', '.'))                      # the Piazzetta, to the water
    d.rect(52, 35, 55, 37, ':', only=('h', '.'))                      # Campo San Polo
    d.rect(57, 46, 60, 48, ':', only=('h', '.'))                      # Campo Santo Stefano
    d.rect(44, 49, 47, 51, ':', only=('h', '.'))                      # Campo Santa Margherita
    d.rect(77, 31, 79, 34, ':', only=('h', '.'))                      # Campo Santi Giovanni e Paolo
    d.put(78, 31, 's')                                                # Colleoni on horseback (Verrocchio)
    d.rect(37, 28, 40, 30, ':', only=('h', '.'))                      # the Ghetto Nuovo
    d.rect(70, 9, 73, 11, ':', only=('h', '.'))                       # Murano
    d.stalls(52, 35, 55, 37, 0.9)                                     # the market spills into Campo San Polo
    d.stalls(68, 40, 80, 44, 0.12)                                    # and stalls in the Piazza
    # landmarks
    d.rect(81, 39, 86, 44, 'B')            # the Basilica of San Marco, facing west onto the Piazza
    d.rect(79, 45, 79, 45, 'C')            # its campanile, at the corner of the Piazza and the Piazzetta
    d.rect(84, 45, 90, 50, 'D')            # the Doge's Palace
    d.rect(74, 46, 78, 50, 'L')            # the Library
    d.rect(68, 38, 80, 38, 'P')            # the Procuratie Vecchie
    d.rect(68, 45, 75, 45, 'P')            # the Procuratie Nuove
    d.rect(52, 38, 56, 41, 'M')            # the Rialto market
    d.rect(64, 34, 67, 36, 'T')            # the Fondaco dei Tedeschi
    d.rect(46, 38, 50, 42, 'F')            # the Frari
    d.rect(45, 43, 48, 45, 'S')            # the Scuola Grande di San Rocco
    d.rect(80, 30, 85, 34, 'Z')            # Santi Giovanni e Paolo
    d.rect(93, 32, 99, 39, 'A')            # the Arsenale
    d.rect(82, 56, 86, 58, 'G')            # San Giorgio Maggiore
    d.rect(52, 63, 56, 65, 'Y')            # the Redentore
    d.rect(64, 55, 66, 57, 'K')            # the Salute, begun last year
    d.rect(58, 29, 61, 30, 'X')            # the Ca' d'Oro
    d.rect(44, 33, 48, 36, 'V')            # the Fondaco dei Turchi
    d.rect(41, 29, 44, 32, 'H')            # the synagogue of the Ghetto
    d.rect(46, 25, 50, 27, 'E')            # Santa Maria dei Servi
    d.rect(75, 9, 78, 11, 'U')             # Santi Maria e Donato, on Murano
    d.rect(66, 4, 70, 7, 'W')              # the glassworks
    d.rect(105, 23, 107, 25, 'J')          # San Nicolò al Lido
    d.rect(69, 47, 73, 50, 'I')            # the Locanda della Luna
    d.ring('BCDLPMTFSZAGYKXVHEUWJI')
    # Fusina, on the mainland: a landing, an inn, a customs post on the road to Padua
    d.rect(5, 40, 11, 48, 'h', only=(',',))
    d.rect(12, 39, 12, 49, '.', only=(',', 'h'))
    d.rect(5, 44, 11, 44, '.', only=('h',))
    d.rect(6, 45, 10, 48, 'N')
    d.ring('N')
    for y in range(41, 48):
        d.put(1, y, '#')
    d.gates([(1, 44, '1', 'h', [(0, 44), (-3, 44)])])
    d.street([(2, 44), (5, 44)], 1)
    # columns, wells, trees, gardens
    d.put(80, 51, 'j'); d.put(82, 51, 'j')  # the columns of San Marco and San Todaro
    for (x, y) in ((53, 36), (58, 47), (45, 50), (78, 33), (38, 29), (71, 10), (75, 42)):
        if d.get(x, y) == ':':
            d.put(x, y, 'o')
    for y in range(d.H):
        for x in range(d.W):
            if d.get(x, y) == ',' and d.rnd.random() < 0.07:
                d.put(x, y, 't')
    d.lanes(6)
    d.connect(avoid=GC)
    # gondolas at the posts: the Molo, along the Grand Canal, at Fusina and Murano
    def moor(x, y, ch='g'):
        for r in range(0, 5):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    if max(abs(dx), abs(dy)) != r:
                        continue
                    xx, yy = x + dx, y + dy
                    if d.get(xx, yy) != '~':
                        continue
                    if any(d.get(xx + a, yy + b) in ('.', ':', 'h', ',') for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                        d.put(xx, yy, ch)
                        return
    for (x, y) in ((73, 52), (75, 52), (79, 53), (88, 53), (61, 43), (58, 45), (55, 31), (47, 33), (62, 52),
                   (13, 41), (13, 47), (68, 13), (40, 38), (56, 56)):
        moor(x, y)
    # ferries: the traghetti over the Grand Canal, and boats to the islands and the mainland
    global VENICE_FERRIES
    VENICE_FERRIES = []
    for (ax, ay, bx, by, kind, a, b) in (
            (13, 44, 42, 58, 'boat', 'the landing at Fusina', 'the Zattere'),
            (48, 47, 53, 47, 'gondola', 'San Tomà', "Sant'Angelo"),
            (61, 32, 59, 36, 'gondola', 'Santa Sofia', 'the Rialto market'),
            (64, 50, 64, 56, 'gondola', 'Santa Maria del Giglio', 'the Salute'),
            (82, 52, 84, 55, 'gondola', 'the Molo of San Marco', 'San Giorgio Maggiore'),
            (50, 58, 50, 62, 'gondola', 'the Zattere', 'the Giudecca'),
            (70, 23, 72, 13, 'boat', 'the Fondamenta Nuove', 'Murano'),
            (97, 45, 104, 40, 'boat', 'the Riva degli Schiavoni', 'the Lido')):
        VENICE_FERRIES.append("ferry %d %d %d %d %s %s | %s" % (2 * ax, 2 * ay, 2 * bx, 2 * by, kind, a, b))
    return d.done()


def venice_header():
    return VENICE_HEADER.replace("{ferries}\n", "".join(f + "\n" for f in VENICE_FERRIES))


VENICE_HEADER = """\
# Venice in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py; this file is now the master copy.
# Characters as in paris.map ('g' a mooring post with a gondola); the capital letters are landmarks.
# A ferry line joins two landings (near those columns and rows): walk into the boat to cross.

style venice
ground 1.0
towerroof flat

legend B basilica  "the Basilica of San Marco"    dome=8E959A stone=E8DCCB
legend C tower     "the Campanile of San Marco"   height=21 top=belfry spire brick roof=5E8A70
legend D palace    "the Doge's Palace"            depth=4 storeys=3 roof=lead stone=ECCFC6 floors=1
legend L gallery   "the Library of San Marco"     stone=EEE8DC roof=lead
legend P gallery   "the Procuratie"               stone=E8E0D0 roof=tile names=the_Procuratie_Vecchie|the_Procuratie_Nuove
legend Q bridgehouses "the Rialto Bridge"         shops
legend M halle     "the Rialto market"
legend T palace    "the Fondaco dei Tedeschi"     depth=3 storeys=3 roof=tile stone=E6D6C0
legend F church    "Santa Maria Gloriosa dei Frari"  brick roof=tile
legend S hall      "the Scuola Grande di San Rocco"  nocampanile stone=E8E0D0 roof=lead
legend Z church    "Santi Giovanni e Paolo"       brick roof=tile
legend A fortress  "the Arsenale"                 height=6 brick
legend G church    "San Giorgio Maggiore"         front=flat norose crossing=dome dome=8E959A stone=F0ECE4 roof=lead
legend Y church    "the Redentore"                front=flat norose crossing=dome dome=8E959A stone=F0ECE4 roof=lead
legend K rotunda   "the building site of Santa Maria della Salute"  sides=8 height=2 roof=battlement stone=F0ECE4
legend X palace    "the Ca' d'Oro"                depth=2 storeys=3 roof=tile stone=EEE4D8 open=south
legend V hall      "the Fondaco dei Turchi"       nocampanile roof=tile stone=E8DCCA
legend H hall      "the synagogue of the Ghetto"  nocampanile stone=D8C8B0 roof=tile floors=1
legend E church    "Santa Maria dei Servi"        brick roof=tile
legend U church    "Santi Maria e Donato"         brick roof=tile
legend W hall      "the glassworks of the Barovier"  nocampanile brick roof=tile  sign=goblet
legend J church    "San Nicolò al Lido"           roof=tile
legend I inn       "the Locanda della Luna"
legend N inn       "the inn at Fusina"

gate 1 "the customs post at Fusina"

{ferries}
map
"""



# ---------------------------------------------------------------- Haarlem and Leiden

def haarlem():
    """Haarlem in 1632, squeezed between the IJ (north, just beyond the map) and the tip of the
    Haarlemmermeer (south-east corner): the walled town on the Spaarne, the Grote Markt with
    the Grote Kerk, the Town Hall and the Vleeshal. 40 x 33 cells, north at the top; the map's
    edges meet the old-map cells of world.txt (the Meer's tip, the dunes to the west)."""
    d = Draft(40, 33, 1633)
    # the tip of the Haarlemmermeer, and the Spaarne from it north towards the IJ
    meer = [(22, 33.6), (25, 29), (28, 27.3), (31, 28.3), (35, 30), (39, 31.3), (40.5, 33.6)]
    for y in range(d.H):
        for x in range(d.W):
            if Draft.inpoly(x + 0.5, y + 0.5, meer):
                d.put(x, y, '~')
    d.water([(28, 28, 1.1), (27, 21, 1.0), (26, 13, 1.0), (24, 6, 1.0), (23, -1, 1.1)])
    inside = lambda x, y: ((x - 17.5) / 15.0) ** 2 + ((y - 14.5) / 13.0) ** 2 <= 1.0
    d.walled(inside)
    d.street([(6, 15), (16, 15), (30, 14)], 1)      # the Zijlstraat and on east to the Amsterdamse Poort
    d.street([(17, 4), (16, 14), (14, 26)], 1)      # north to south: the Jansstraat, the Grote Houtstraat
    d.street([(9, 8), (16, 9)], 1)
    d.street([(8, 21), (22, 21)], 1)
    d.inner_streets()
    d.rect(12, 11, 18, 16, ':', only=('h', '.'))    # the Grote Markt
    d.stalls(12, 11, 18, 16, 0.35)
    d.rect(19, 10, 24, 15, 'K')            # the Grote Kerk, Saint Bavo
    d.rect(6, 11, 10, 15, 'T')             # the Town Hall
    d.rect(11, 17, 14, 20, 'V')            # the Vleeshal
    d.rect(17, 22, 21, 25, 'M')            # the Oudemannenhuis, the old men's almshouse ('O' is a wall tower)
    d.rect(11, 6, 15, 9, 'I')              # the Golden Lion
    d.rect(5, 17, 9, 20, 'H')              # Frans Hals's house (inside the wall)
    d.rect(17, 17, 21, 20, 'L')            # the Leyster brewery
    d.rect(18, 4, 22, 6, 'C')              # the Janskerk
    d.ring('KTVMIHLC')
    d.gates([(17, 3, '1', 'v', [(17, 1), (17, -2)]),
             (5, 15, '2', 'h', [(3, 15), (-3, 15)]),
             (14, 26, '3', 'v', [(14, 28), (12, 35)]),
             (30, 14, '4', 'h', [(32, 14), (42, 15)])])
    d.towers(7)
    d.lanes(6)
    d.connect(gap=2)
    d.put(15, 13, 'o') if d.get(15, 13) == ':' else None
    for y in range(d.H):
        for x in range(d.W):
            if d.get(x, y) == '.' and any(d.get(x + a, y + b) == '~' for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))) and d.rnd.random() < 0.1:
                d.put(x, y, 't')
    for y in range(8, 24, 4):
        if d.get(27, y) == '~': d.put(27, y, 'g')
    return d.done()


HAARLEM_HEADER = """\
# Haarlem in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (haarlem); the script is the master copy for now.
# Characters as in paris.map; the capital letters are landmarks.

style dutchtown
ground 1.0
towerroof cone

legend K church    "the Grote Kerk"               brick roof=slate crossing=tower stone=C8B8A0
legend T hall      "the Town Hall of Haarlem"     nocampanile brick roof=slate floors=1
legend V hall      "the Vleeshal"                 nocampanile brick roof=slate
legend M hall      "the Oudemannenhuis"           nocampanile brick roof=tile
legend I inn       "the Golden Lion"
legend H mansion   "the house of Frans Hals"  sign=palette
legend L mansion   "the Leyster brewery"  sign=ale
legend C church    "the Janskerk"                 brick roof=slate

gate 1 "the Janspoort"
gate 2 "the Zijlpoort"
gate 3 "the Grote Houtpoort"
gate 4 "the Amsterdamse Poort"

map
"""


def leiden():
    """Leiden in 1632: the Oude and the Nieuwe Rijn meeting by the Burcht and flowing west as one;
    the Rapenburg with the Academy and the Hortus behind it; the Pieterskerk, the Hooglandse
    Kerk, the Town Hall on the Breestraat, the anatomy theatre. 60 x 38 cells (enlarged by 1.5
    about the middle, the landmarks drawn bigger than life); the Rijn enters
    and leaves at the east and west edges, where the old-map cells of world.txt carry it on."""
    d = Draft(60, 38, 1634)
    d.water([(61.5, 15.25, 1.1), (45, 16, 1.1), (36, 18.25, 1.2)])     # the Oude Rijn
    d.water([(61.5, 22.75, 1.1), (45, 22, 1.1), (36, 19.75, 1.2)])     # the Nieuwe Rijn
    d.water([(36, 19, 1.4), (18, 19, 1.4), (-1.5, 19, 1.4)])           # the two as one, west to the sea
    d.water([(18, 19.75, 0.8), (18, 34.75, 0.8)])                      # the Rapenburg
    inside = lambda x, y: ((x - 30) / 29.4) ** 2 + ((y - 19) / 18.0) ** 2 <= 1.0
    d.walled(inside)
    d.street([(21, 24), (36, 24), (54, 24)], 1)     # the Breestraat
    d.street([(24, 3), (27, 14)], 1)
    d.street([(41, 4), (41, 14)], 1)
    d.street([(2, 14), (27, 14)], 1)
    d.inner_streets()
    d.rect(37, 25, 42, 27, ':', only=('h', '.'))   # the Botermarkt, off the Breestraat
    d.stalls(37, 25, 42, 27, 0.4)
    d.rect(20, 25, 25, 30, 'A')            # the Academy, on the Rapenburg
    d.rect(20, 31, 27, 33, 'G')            # the Ambulacrum of the Hortus
    d.rect(20, 34, 30, 34, ',', only=('h', '.'))   # the Hortus beds
    d.rect(7, 21, 15, 26, 'P')             # the Pieterskerk
    d.rect(33, 11, 37, 15, 'B')            # the Burcht, on its mound
    d.rect(43, 7, 50, 11, 'K')             # the Hooglandse Kerk
    d.rect(28, 25, 35, 28, 'T')            # the Town Hall, on the Breestraat
    d.rect(12, 28, 16, 32, 'Q')            # the anatomy theatre
    d.rect(19, 21, 23, 23, 'E')            # the Elzevirs' printing house
    d.rect(29, 7, 33, 10, 'I')             # the Golden Fleece
    d.rect(6, 10, 10, 13, 'R')             # the van Rijn house, by the Weddesteeg
    d.ring('APBKTQEIR')
    d.gates([(24, 1, '1', 'v', [(24, -1), (22, -4)]),
             (41, 3, '2', 'v', [(41, 0), (44, -4)]),
             (1, 14, '3', 'h', [(-1, 14), (-4, 14)]),
             (59, 26, '4', 'h', [(61, 26), (65, 27)])])
    d.towers(7)
    d.lanes(7)
    d.connect(gap=2)
    for (x, y) in ((22, 34), (25, 34), (28, 34), (30, 34)):
        if d.get(x, y) == ',': d.put(x, y, 't')
    for y in range(d.H):
        for x in range(d.W):
            if d.get(x, y) == '.' and any(d.get(x + a, y + b) == '~' for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))) and d.rnd.random() < 0.08:
                d.put(x, y, 't')
    return d.done()


LEIDEN_HEADER = """\
# Leiden in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (leiden); the script is the master copy for now.
# Characters as in paris.map; the capital letters are landmarks.

style dutchtown
ground 1.0
towerroof cone

legend A college   "the Academy of Leiden"        depth=4 storeys=2 roof=slate brick floors=1
legend G gallery   "the Ambulacrum of the Hortus" roof=tile brick
legend P church    "the Pieterskerk"              brick roof=slate
legend B rotunda   "the Burcht"                   fortress roof=battlement sides=16 height=4 brick
legend K church    "the Hooglandse Kerk"          brick roof=slate
legend T hall      "the Town Hall of Leiden"      nocampanile brick roof=slate floors=1
legend Q hall      "the anatomy theatre"          nocampanile brick roof=slate
legend E mansion   "the printing house of the Elzevirs"  sign=book
legend I inn       "the Golden Fleece"
legend R mansion   "the van Rijn house"

gate 1 "the Zijlpoort"
gate 2 "the Rijnsburgerpoort"
gate 3 "the Morschpoort"
gate 4 "the Hogewoerdspoort"

map
"""


# ---------------------------------------------------------------- Padua

def padua():
    """Padua in 1632, under Venice: the Venetian walls of the last century round a city of
    arcaded streets; the Piovego along the north-east (the boats for Venice leave from the
    Portello), the old moat (the Naviglio) round the centre; the Palazzo della Ragione between
    its two market squares, the Bo (the University) and its anatomy theatre, the Duomo, the
    Santo with its domes, the Orto botanico, the Prato della Valle and Santa Giustina, the
    Scrovegni chapel, the Castello (a prison). 55 x 46 cells; the Piovego enters at the top and
    leaves at the east edge, where the old map's river crosses them."""
    d = Draft(55, 46, 1632)
    OUTLINE = [(2, 11), (10, 3), (25, 1), (41, 2), (47, 7), (51, 14), (53, 23), (52, 33), (46, 42),
               (33, 45), (17, 44), (7, 38), (2, 27)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, OUTLINE))
    # streets first; the landmarks are drawn over them
    d.street([(3, 20), (19, 20)])                     # Via Savonarola, from the west
    d.street([(28, 21), (40, 22), (54, 23)])          # Via Altinate, east to the Portello
    d.street([(20, 1), (21, 10), (23, 15)])           # north, from the Porta Codalunga
    d.street([(24, 25), (25, 33), (24, 45)])          # south, to the Porta Santa Croce
    d.street([(31, 27), (35, 32)])                    # Via del Santo
    d.street([(13, 27), (11, 33)])                    # to the Castello
    d.street([(37, 22), (38, 31)])                    # Via Vignali
    d.street([(9, 36), (22, 37)])
    d.street([(40, 14), (44, 25)])
    # the water: the Piovego, and the Naviglio round the old town, joined to it at both ends
    PIOVEGO = [(45, -1, 1.0), (46, 6, 1.0), (49, 12, 1.0), (52, 17, 1.0), (56, 21, 1.0)]
    NAVIGLIO = [(48, 11, 0.55), (40, 12, 0.55), (33, 12, 0.55), (26, 10.5, 0.55), (18, 10.5, 0.55),
                (11, 13, 0.55), (9, 20, 0.55), (10, 28, 0.55), (14, 33, 0.55), (22, 35, 0.55),
                (30, 35, 0.55), (36, 32, 0.55), (45, 29, 0.55), (52, 24, 0.55)]
    for line in (PIOVEGO, NAVIGLIO):
        for y in range(d.H):
            for x in range(d.W):
                if d.near(x, y, line) and d.get(x, y) in ('h', '.', ' ', '#'):
                    d.put(x, y, '~')
    for (x, y) in [(x, y) for y in range(d.H) for x in range(d.W)]:
        if d.get(x, y) == '.' and any(d.get(x + a, y + b) == '~' for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            pass
    # streets crossing water become bridges
    for pts in ([(3, 20), (19, 20)], [(28, 21), (40, 22), (54, 23)], [(20, 1), (21, 10), (23, 15)],
                [(24, 25), (25, 33), (24, 45)], [(13, 27), (11, 33)], [(37, 22), (38, 31)], [(40, 14), (44, 25)]):
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            n = max(abs(x1 - x0), abs(y1 - y0)) * 2 + 1
            for i in range(n + 1):
                t = i / n
                x, y = round(x0 + (x1 - x0) * t), round(y0 + (y1 - y0) * t)
                for k in (0, 1):
                    for j in (0, 1):
                        if d.get(x + k, y + j) == '~':
                            d.put(x + k, y + j, '=')
    d.inner_streets()
    # squares
    d.rect(19, 16, 28, 18, ':', only=('h', '.'))      # the Piazza dei Frutti
    d.rect(19, 22, 28, 24, ':', only=('h', '.'))      # the Piazza delle Erbe
    d.rect(13, 16, 18, 19, ':', only=('h', '.'))      # the Piazza dei Signori
    d.rect(33, 33, 36, 38, ':', only=('h', '.'))      # the Piazza del Santo
    d.rect(12, 27, 14, 29, ':', only=('h', '.'))      # before the Duomo
    d.stalls(19, 16, 28, 18, 0.3)
    d.stalls(19, 22, 28, 24, 0.3)
    # the Prato della Valle: a meadow for fairs and the cattle market, half marsh
    d.rect(22, 36, 30, 41, ',', only=('h', '.'))
    # the Orto botanico
    d.rect(38, 41, 44, 43, ',', only=('h', '.'))
    # landmarks
    # (the enlargement rule: plot buildings are drawn bigger than life, roomy inside)
    d.rect(19, 18, 28, 21, 'R')                       # the Palazzo della Ragione (its Salone upstairs)
    d.rect(14, 14, 15, 15, 'T')                       # the Torre dell'Orologio
    d.rect(16, 13, 19, 15, 'V')                       # the Palazzo del Capitanio
    d.rect(13, 22, 17, 26, 'D')                       # the Duomo
    d.rect(12, 19, 14, 21, 'P')                       # the Baptistery
    d.rect(29, 22, 35, 28, 'B')                       # the Bo
    d.rect(36, 23, 39, 26, 'Q')                       # its anatomy theatre
    d.rect(20, 25, 24, 27, 'I')                       # an inn
    d.rect(40, 25, 44, 28, 'G')                       # Galileo's old house, in the Borgo dei Vignali
    d.rect(37, 32, 44, 37, 'S')                       # the Santo
    d.rect(31, 40, 36, 43, 'J')                       # Santa Giustina
    d.rect(38, 38, 42, 40, 'H')                       # the house of the prefect of the Orto
    d.rect(5, 29, 9, 32, 'K')                         # the Castello
    d.rect(34, 6, 39, 8, 'E')                         # the Eremitani
    d.rect(40, 6, 44, 8, 'C')                         # the Scrovegni chapel
    d.ring('RVDBSJEK')
    for y in range(40, 44):
        for x in range(38, 45):
            if d.get(x, y) == ',' and (x * 3 + y) % 5 == 0:
                d.put(x, y, 't')
    for (x, y) in ((23, 37), (27, 37), (29, 40), (23, 40), (26, 39)):
        if d.get(x, y) == ',':
            d.put(x, y, 't')
    d.put(34, 36, 's')                                # Gattamelata, Donatello's horseman
    for (x, y) in ((24, 17), (24, 23), (35, 34), (16, 18)):
        if d.get(x, y) in (':', '.'):
            d.put(x, y, 'o')
    d.gates([(20, 2, '1', 'v', [(20, 0), (20, -3)]),
             (53, 25, '2', 'h', [(55, 25), (58, 26)]),
             (24, 44, '3', 'v', [(24, 46), (23, 49)]),
             (2, 20, '4', 'h', [(0, 20), (-3, 20)])])
    d.towers(7)
    d.lanes(7)
    d.connect(gap=2)
    # boats at the Portello, on the Piovego by the east gate
    for (x, y) in ((50, 15), (51, 18), (52, 20), (49, 13)):
        if d.get(x, y) == '~':
            d.put(x, y, 'g')
    d.trees(0.02)
    return d.done()


PADUA_HEADER = """\
# Padua in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (padua); the script is the master copy for now.
# Characters as in paris.map ('g' a mooring post with a boat); the capital letters are landmarks.

style padua
ground 1.1
towerroof flat

legend R hall      "the Palazzo della Ragione"    nocampanile roof=lead stone=E6D3B0 floors=1
legend T tower     "the Torre dell'Orologio"      height=10 top=battlement stone=E2D4B6
legend V palace    "the Palazzo del Capitanio"    depth=3 storeys=3 roof=tile clock stone=E8DCC4
legend D cathedral "the Duomo of Padua"           front=flat roof=tile crossing=dome domescale=0.8 dome=9A9C9A stone=E6DAC2
legend P rotunda   "the Baptistery of Padua"      sides=8 roof=dome height=4.5 stone=D8B890 dome=B5603E
legend B college   "the Palazzo del Bo"           depth=4 storeys=2 roof=tile stone=EDE3CE floors=1
legend Q hall      "the anatomy theatre of the Bo" nocampanile roof=tile stone=E6D8BE
legend I inn       "the Osteria della Stella"
legend G mansion   "Galileo's old house"
legend S basilica  "the Basilica of the Santo"    dome=8E959A stone=D9B98E
legend J church    "Santa Giustina"               front=flat crossing=dome dome=8E959A brick roof=tile
legend H mansion   "the house of the prefect of the Orto botanico"
legend K fortress  "the Castello"                 height=5.5 brick
legend E church    "the Eremitani"                brick roof=tile
legend C chapel    "the Scrovegni chapel"         plain height=5 brick

gate 1 "the Porta Codalunga"
gate 2 "the Porta Portello"
gate 3 "the Porta Santa Croce"
gate 4 "the Porta Savonarola"

map
"""


# ---------------------------------------------------------------- Pisa

def pisa():
    """Pisa in 1632, under Florence: the medieval walls round both banks of the Arno; the
    Piazza dei Miracoli in the north-west corner (the Duomo, the Baptistery, the leaning
    campanile, the Camposanto), the Piazza dei Cavalieri, the Sapienza, the Orto botanico, the
    two citadels and the Medici arsenal, Santa Maria della Spina on the south bank, the house
    where Galileo was born. 48 x 36 cells; the Arno enters at the east edge and leaves at the
    west edge, where the old map's cells (world.txt) carry it from Florence and to the sea."""
    d = Draft(48, 36, 1564)
    ARNO = [(49, 20, 1.2), (42, 19.6, 1.25), (35, 18.3, 1.3), (28, 18.7, 1.3), (21, 19.5, 1.3),
            (14, 18.8, 1.3), (7, 17.9, 1.3), (-1, 17.5, 1.3)]
    d.water(ARNO)
    OUTLINE = [(3, 4), (16, 2), (33, 3), (44, 6), (46, 14), (45, 24), (40, 31), (26, 33), (12, 32),
               (4, 27), (2, 15)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, OUTLINE))
    d.inner_streets()
    # streets
    d.street([(24, 3), (25, 9)])                      # Borgo Stretto, from the Porta a Lucca
    d.street([(26, 12), (28, 16), (28, 23), (28, 32)])   # to the Ponte di Mezzo, and over it
    d.street([(16, 9), (18, 16)])                     # Via Santa Maria, from the Miracoli to the river
    d.street([(28, 12), (46, 12)])                    # east, to the Porta alle Piagge
    d.street([(4, 25), (43, 27)])                     # the south bank, from the Porta a Mare to the Porta Fiorentina
    d.street([(9, 15), (9, 23)])                      # the Ponte a Mare
    d.street([(39, 15), (39, 24)])                    # the Ponte della Fortezza
    d.street([(3, 9), (8, 9)])                        # in from the Porta Nuova
    # squares and lawns
    d.rect(4, 4, 19, 12, ',', only=('h', '.'))        # the Piazza dei Miracoli, a lawn
    d.rect(20, 9, 25, 12, ':', only=('h', '.'))       # the Piazza dei Cavalieri
    d.rect(26, 21, 31, 23, ':', only=('h', '.'))      # the Piazza di Banchi, at the south end of the bridge
    d.rect(9, 14, 14, 16, ',', only=('h', '.'))       # the Orto botanico (moved south to make room for its keeper's house)
    d.rect(29, 14, 33, 16, ':', only=('h', '.'))      # the Piazza delle Vettovaglie, the market
    d.stalls(26, 21, 31, 23, 0.3)
    d.stalls(29, 14, 33, 16, 0.6)
    # landmarks
    d.rect(10, 7, 16, 10, 'D')                        # the Duomo (a row south, leaving the Camposanto its lawn)
    d.rect(5, 5, 8, 8, 'B')                           # the Baptistery, west of it
    d.rect(18, 10, 19, 11, 'T')                       # the campanile, east of it, leaning
    # (the enlargement rule: plot buildings are drawn bigger than life, roomy inside)
    d.rect(7, 3, 16, 5, 'C')                          # the Camposanto, north of it
    d.rect(17, 4, 23, 8, 'K')                         # the Palazzo della Carovana
    d.rect(24, 8, 28, 11, 'S')                        # Santo Stefano dei Cavalieri
    d.rect(19, 13, 26, 16, 'U')                       # the Sapienza
    d.rect(11, 10, 15, 13, 'H')                       # the house of the keeper of the Orto
    d.rect(3, 11, 8, 13, 'F')                         # the Cittadella Vecchia
    d.rect(3, 14, 8, 16, 'A')                         # the Medici arsenal, on the Arno
    d.rect(32, 14, 36, 16, 'I')                       # an inn
    d.rect(29, 21, 31, 22, 'L')                       # the Logge di Banchi
    d.rect(39, 21, 44, 26, 'N')                       # the Cittadella Nuova
    d.rect(12, 21, 16, 23, 'P')                       # Santa Maria della Spina, on the Lungarno (clear of San Paolo: its door is at the west end)
    d.rect(6, 22, 10, 24, 'R')                        # San Paolo a Ripa d'Arno
    d.rect(18, 23, 22, 25, 'G')                       # the house where Galileo was born
    d.ring('DBCKSUFNRLP')
    for x in range(6, 17):                            # a gravel path along the Camposanto, for its doors
        if d.get(x, 6) == ',':
            d.put(x, 6, '.')
    d.put(22, 11, 'y')                                # Cosimo I, by Francavilla
    for (x, y) in ((33, 9), (20, 28), (36, 26), (8, 11)):
        if d.get(x, y) in (':', '.', ','):
            d.put(x, y, 'o')
    for y in range(13, 16):
        for x in range(10, 15):
            if d.get(x, y) == ',' and (x + 2 * y) % 4 == 0:
                d.put(x, y, 't')
    d.gates([(24, 3, '1', 'v', [(24, 1), (25, -3)]),
             (2, 9, '2', 'h', [(0, 9), (-3, 9)]),
             (4, 25, '3', 'h', [(2, 26), (-3, 28)]),
             (43, 27, '4', 'h', [(45, 28), (51, 30)]),
             (46, 12, '5', 'h', [(48, 12), (51, 12)])])
    d.towers(7)
    d.lanes(7)
    d.connect(gap=2)
    for (x, y) in ((12, 18), (20, 20), (33, 17)):
        if d.get(x, y) == '~':
            d.put(x, y, 'g')
    d.trees(0.02)
    return d.done()


PISA_HEADER = """\
# Pisa in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (pisa); the script is the master copy for now.
# Characters as in paris.map ('g' a mooring post with a boat); the capital letters are landmarks.

style pisa
ground 1.1
towerroof flat

legend D cathedral "the Duomo of Pisa"            front=flat norose roof=lead crossing=dome domescale=0.45 dome=C9C2B4 stone=F0ECE2
legend B rotunda   "the Baptistery of Pisa"       sides=16 roof=dome height=6 stone=F0ECE2 dome=C9C2B4
legend T tower     "the Leaning Tower"            round lean=5.5 height=12 top=belfry stone=F0ECE2
legend C gallery   "the Camposanto"               stone=EEE8DA roof=lead
legend K palace    "the Palazzo della Carovana"   depth=3 storeys=3 roof=tile stone=D6D2C8 floors=1
legend S church    "Santo Stefano dei Cavalieri"  front=flat roof=tile stone=ECE6D8
legend U college   "the Palazzo della Sapienza"   depth=3 storeys=2 roof=tile stone=E6D6B6 floors=1
legend H mansion   "the house of the keeper of the Orto botanico"
legend F fortress  "the Cittadella Vecchia"       height=6 roofed brick
legend A hall      "the Medici arsenal"           nocampanile brick roof=tile
legend I inn       "the Osteria del Cappello"
legend L halle     "the Logge di Banchi"          nostalls
legend N fortress  "the Cittadella Nuova"         height=5 roofed brick
legend P chapel    "Santa Maria della Spina"      plain height=4 stone=F2EEE6
legend R church    "San Paolo a Ripa d'Arno"      front=flat roof=tile stone=ECE6D8
legend G mansion   "the house where Galileo was born"

gate 1 "the Porta a Lucca"
gate 2 "the Porta Nuova"
gate 3 "the Porta a Mare"
gate 4 "the Porta Fiorentina"
gate 5 "the Porta alle Piagge"

map
"""


# ---------------------------------------------------------------- Constantinople

CONSTANTINOPLE_FERRIES = []

def constantinople():
    """Constantinople in 1632, the city of Sultan Murad IV: the old city on its peninsula between
    the Golden Horn and the Sea of Marmara, closed on the land side by the walls of Theodosius;
    Galata, the Genoese town across the Horn, and the embassies on the hill of Pera above it;
    Uskudar across the Bosphorus, in Asia. 90 x 70 cells; the Bosphorus enters at the top, where
    the old map (world.txt) brings it from the Black Sea, and the Marmara is the south of the map."""
    d = Draft(90, 70, 1453)
    # the water: the Bosphorus, the Golden Horn, the Sea of Marmara
    d.water([(50, -1, 6), (51, 8, 5), (52, 18, 4.5), (51, 28, 4.5), (50, 38, 5), (53, 48, 6.5), (58, 56, 9)])
    HORN = [(48, 28, 2.6), (38, 26.6, 2.4), (28, 25.2, 2.2), (18, 23.2, 1.8), (9, 21.5, 1.4), (4, 20.6, 1.0)]
    d.water(HORN)
    marmara = [(0, 56), (10, 55), (20, 54.6), (30, 54), (40, 53), (47, 50), (55, 52), (65, 55.5), (74, 57.5),
               (79, 60), (80, 71), (-1, 71)]
    for y in range(d.H):
        for x in range(d.W):
            if Draft.inpoly(x + 0.5, y + 0.5, marmara): d.put(x, y, '~')
    peninsula = [(1, 21.5), (8, 22.3), (18, 24.8), (28, 26.8), (38, 28.4), (46, 30.2), (48.5, 36), (47.5, 43),
                 (46, 48), (40, 52), (30, 53.2), (20, 53.8), (10, 54.2), (1, 55)]
    galata = [(24, 23.8), (30, 24.4), (38, 25.6), (46, 26.2), (47.5, 21), (46.5, 15), (42, 10), (35, 8), (29, 10), (24.5, 15)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, peninsula))
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, galata))
    # unwalled: Pera above Galata, Kasimpasa and the arsenal on the Horn, Uskudar in Asia
    pera = lambda x, y: 20 <= x <= 46 and 1 <= y <= 7
    kasim = lambda x, y: 5 <= x <= 23 and 11 <= y <= 18
    uskudar = lambda x, y: 55 <= x <= 87 and 22 <= y <= 50 and d.get(x, y) != '~'   # (not to the map's edge: a door there opens on the world's trees)
    for f in (pera, kasim, uskudar):
        d.walled(f, wall=False)
    d.inner_streets()
    # streets
    for pts in ([(36, 44), (25, 42), (15, 40), (3, 37)],        # the Divan Yolu, to the Topkapi gate
                [(22, 38), (12, 31), (3, 27)],                   # to the Edirne gate
                [(25, 49), (12, 51), (5, 52)],                   # to Yedikule
                [(27, 30), (24, 38)], [(40, 30), (39, 36)], [(10, 45), (22, 46)],
                [(30, 24), (34, 11)], [(26, 20), (45, 20)],      # Galata
                [(34, 8), (32, 5)], [(22, 5), (45, 5)],          # Pera's high street
                [(6, 14), (23, 14)],                             # Kasimpasa
                [(59, 40), (75, 38), (89, 36)], [(62, 28), (64, 49)], [(60, 34), (80, 30)]):   # Uskudar
        d.street(pts)
    # squares and gardens
    d.rect(21, 43, 26, 51, ':', only=('h', '.'))     # the Hippodrome, At Meydani
    d.rect(36, 31, 46, 39, ',', only=('h', '.'))     # the gardens of the Seraglio point
    d.rect(23, 29, 31, 31, ':', only=('h', '.'))     # Eminonu, at the landing
    d.stalls(23, 29, 31, 31, 0.4)
    d.rect(70, 41, 86, 50, ',', only=('h', '.'))     # the cemetery of Karacaahmet, under cypresses
    for y in range(41, 51):
        for x in range(70, 87):
            if d.get(x, y) == ',' and (x * 5 + y * 3) % 7 < 4: d.put(x, y, 't')
    for y in range(1, 8):                             # the vineyards of Pera
        for x in range(20, 47):
            if d.get(x, y) == 'h' and (x + y) % 5 == 0: d.put(x, y, ',')
    # landmarks
    d.rect(40, 32, 46, 38, 'T')     # the Topkapi Palace, on the Seraglio point
    d.rect(33, 39, 39, 44, 'A')     # Ayasofya
    d.rect(27, 46, 32, 50, 'S')     # the mosque of Sultan Ahmed
    d.rect(14, 38, 19, 42, 'B')     # the Grand Bazaar
    d.rect(12, 31, 17, 35, 'M')     # the Suleymaniye
    d.rect(26, 27, 29, 29, 'Y')     # the Yeni Cami, a building site
    d.rect(5, 32, 9, 36, 'F')       # the mosque of Fatih
    d.rect(2, 49, 7, 53, 'K')       # Yedikule
    d.rect(4, 23, 8, 26, 'P')       # the Patriarchate, in the Fener
    d.rect(20, 34, 22, 36, 'I')     # the Kurkcu Han
    d.rect(17, 28, 21, 31, 'Q')     # a coffeehouse of Tahtakale
    d.rect(34, 9, 35, 10, 'G')      # the Galata Tower
    d.rect(28, 18, 30, 20, 'N')     # San Pietro e Paolo
    d.rect(10, 15, 17, 17, 'R')     # the Tersane
    d.rect(27, 1, 31, 4, 'E')       # the Palais de France
    d.rect(33, 1, 37, 4, 'V')       # the Venetian bailo
    d.rect(39, 1, 43, 4, 'L')       # the English ambassador
    d.rect(21, 1, 25, 4, 'D')       # the Dutch ambassador
    d.rect(60, 32, 63, 35, 'U')     # the mosque of Mihrimah Sultan at Uskudar
    d.ring('TASBMFKPRNEVLDUQI')
    d.put(23, 46, 'i')              # the obelisk of Theodosius
    d.put(24, 49, 'i')              # the serpent column
    for (x, y) in ((30, 42), (18, 45), (10, 40), (32, 15), (66, 38), (28, 6)):
        if d.get(x, y) in (':', '.'): d.put(x, y, 'f')   # fountains
    d.gates([(1, 27, '1', 'h', [(-1, 27), (-3, 27)]),     # the Edirne gate
             (1, 37, '2', 'h', [(-1, 37), (-3, 37)]),     # the Topkapi gate (the Cannon gate)
             (1, 46, '3', 'h', [(-1, 46), (-3, 46)]),     # the Silivri gate
             (31, 9, '4', 'v', [(31, 7)]),                # Galata: the Kuledibi gate, up to Pera
             (25, 15, '5', 'h', [(23, 15)])])             # Galata: the Azapkapi gate, to Kasimpasa
    d.towers(6)
    d.lanes(6)
    d.connect(avoid=HORN, gap=2)   # no bridge over the Golden Horn: the first was built in 1845
    for (x, y) in ((25, 28), (31, 28), (36, 29), (27, 23), (33, 24), (40, 24), (47, 33), (47, 44), (57, 30), (57, 42),
                   (16, 22), (20, 23)):
        for (xx, yy) in ((x, y), (x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if d.get(xx, yy) == '~' and any(d.get(xx + a, yy + b) in ('.', ':', 'h') for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                d.put(xx, yy, 'g'); break
    global CONSTANTINOPLE_FERRIES
    CONSTANTINOPLE_FERRIES = []
    for (ax, ay, bx, by, kind, a, b) in (
            (27, 29, 28, 24, 'boat', 'Eminonu', 'Galata'),
            (45, 40, 58, 40, 'boat', 'the Seraglio point', 'Uskudar'),
            (40, 24, 58, 26, 'boat', 'Karakoy', 'Uskudar'),
            (14, 21, 13, 19, 'boat', 'the Fener', 'Kasimpasa')):
        CONSTANTINOPLE_FERRIES.append("ferry %d %d %d %d %s %s | %s" % (2 * ax, 2 * ay, 2 * bx, 2 * by, kind, a, b))
    d.trees(0.015)
    return d.done()


CONSTANTINOPLE_HEADER = """\
# Constantinople in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (constantinople); the script is the master copy for now.
# Characters as in paris.map ('g' a mooring post with a boat); the capital letters are landmarks.

style ottoman
ground 1.2
towerroof cone

legend T palace    "the Topkapi Palace"            depth=4 storeys=2 roof=lead stone=EEE6D6
legend A mosque    "Ayasofya"                      minarets=4 height=6 stone=D9B08A
legend S mosque    "the mosque of Sultan Ahmed"    minarets=6 height=5.5 stone=E6E0D4
legend B halle     "the Grand Bazaar"
legend M mosque    "the Suleymaniye"               minarets=4 height=5.5 stone=E8E2D4
legend Y mosque    "the Yeni Cami"                 building minarets=0 stone=E8E2D6
legend F mosque    "the mosque of Fatih"           minarets=2 height=5 stone=E4DCCC
legend K fortress  "Yedikule, the Castle of the Seven Towers"  height=6 stone=CDBFA6
legend P church    "the Patriarchate"              roof=tile front=flat stone=E2D6BE
legend I inn       "the Kurkcu Han"
legend Q hall      "a coffeehouse of Tahtakale"    nocampanile roof=tile
legend G tower     "the Galata Tower"              round height=11 top=spire roof=4A4E58 stone=C8BBA2
legend N church    "San Pietro e Paolo"            roof=tile front=flat stone=E4DACA
legend R hall      "the Tersane"                   nocampanile roof=lead stone=D8CDB8
legend E mansion   "the Palais de France"          style=venice
legend V mansion   "the palace of the Venetian bailo"  style=venice
legend L mansion   "the house of the English ambassador"  style=venice
legend D mansion   "the house of the Dutch ambassador"    style=venice
legend U mosque    "the mosque of Mihrimah Sultan"  minarets=2 height=4.5 stone=E2DACA

gate 1 "the Edirne gate"
gate 2 "the Topkapi gate"
gate 3 "the Silivri gate"
gate 4 "the Kuledibi gate"
gate 5 "the Azapkapi gate"

{ferries}
map
"""

def constantinople_header():
    return CONSTANTINOPLE_HEADER.replace("{ferries}\n", "".join(f + "\n" for f in CONSTANTINOPLE_FERRIES))


# ---------------------------------------------------------------- Ragusa

def ragusa():
    """Ragusa (Dubrovnik) in 1632, the merchant republic of the Adriatic, free under the Sultan's
    protection for a yearly tribute: the walled town on its rock, the Stradun from the Pile gate
    to the Ploce gate, the Rector's palace, the Sponza (the customs house and mint), Orlando's
    column, the fortress of Lovrijenac on its own rock, the Lazaretto where ships wait out their
    quarantine. 52 x 42 cells (enlarged by 1.5 about the middle, the landmarks
    drawn bigger than life); the sea to the west and south, Mount Srd to the north-east."""
    d = Draft(52, 42, 1358)
    sea = [(-1.75, -1.5), (2, -1.5), (2.75, 9), (2, 15), (2.75, 23.25), (8.75, 24.75), (13.25, 25.5), (16.25, 32.25),
           (29.75, 33), (35.75, 28.5), (37.25, 24), (43.25, 23.25), (53.75, 22.5), (53.75, 43.5), (-1.75, 43.5)]
    for y in range(d.H):
        for x in range(d.W):
            if Draft.inpoly(x + 0.5, y + 0.5, sea): d.put(x, y, '~')
    town = [(13.25, 12), (32.75, 10.5), (37.25, 15), (38, 23.25), (29.75, 31.5), (17.75, 31.5), (14, 24)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, town))
    d.walled(lambda x, y: 39 <= x <= 51 and 13 <= y <= 21 and d.get(x, y) != '~', wall=False)   # Ploce
    d.walled(lambda x, y: 3 <= x <= 12 and 3 <= y <= 13 and d.get(x, y) != '~', wall=False)     # Pile, outside
    d.inner_streets()
    d.street([(15, 20), (36, 20)])          # the Stradun
    d.street([(25, 12), (25, 19)])
    d.street([(39, 20), (51, 20)])
    d.rect(27, 18, 30, 21, ':', only=('h', '.'))   # the Luza square, before the Sponza
    d.stalls(27, 18, 30, 21, 0.3)
    d.rect(28, 22, 33, 27, 'R')   # the Rector's palace
    d.rect(31, 15, 35, 18, 'S')   # the Sponza
    d.rect(19, 14, 24, 18, 'F')   # the Franciscans, with their pharmacy
    d.rect(29, 11, 33, 13, 'D')   # the Dominicans
    d.rect(22, 23, 26, 27, 'C')   # the cathedral
    d.rect(15, 11, 18, 14, 'M')   # the Minceta tower, the town wall's great tower (a street between it and the wall, for its door)
    d.rect(4, 18, 8, 23, 'L')     # Lovrijenac, on its rock over the sea
    d.rect(43, 15, 48, 19, 'Z')   # the Lazaretto
    d.rect(16, 23, 20, 26, 'I')   # an inn, off the Stradun
    d.ring('RSFDCLZIM')
    d.put(28, 19, 'y')            # Orlando's column
    d.put(16, 19, 'f')            # Onofrio's fountain
    d.gates([(14, 20, '1', 'h', [(10, 18), (6, 13), (4, 6), (4, -1)]),
             (37, 20, '2', 'h', [(39, 20), (52, 20)])])
    d.towers(6)
    d.lanes(6)
    d.connect(gap=2)
    for y in range(0, 13):         # Mount Srd: scrub and pines above the town
        for x in range(13, 52):
            if d.get(x, y) == ' ' and (x * 3 + y * 7) % 9 == 0: d.put(x, y, 't')
    for (x, y) in ((44, 22), (47, 22), (50, 22)):   # boats at the quay of Ploce, by the old port
        placed = False
        for r in range(0, 3):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    xx, yy = x + dx, y + dy
                    if placed or max(abs(dx), abs(dy)) != r or d.get(xx, yy) != '~':
                        continue
                    if any(d.get(xx + a, yy + b) not in ('~', ' ', 'g') for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                        d.put(xx, yy, 'g')
                        placed = True
    return d.done()


RAGUSA_HEADER = """\
# Ragusa (Dubrovnik) in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (ragusa); the script is the master copy for now.
# Characters as in paris.map ('g' a mooring post with a boat); the capital letters are landmarks.

style ragusa
ground 1.2
towerroof flat

legend R palace    "the Rector's palace"          depth=4 storeys=2 roof=tile stone=E8E0CC floors=1
legend S hall      "the Sponza"                   nocampanile roof=tile stone=E2D8C0 floors=1
legend F church    "the Franciscan monastery"     roof=tile front=flat stone=E6DCC6
legend D church    "the Dominican monastery"      roof=tile front=flat stone=E4D8C2
legend C church    "the cathedral of Ragusa"      roof=tile front=flat crossing=dome domescale=0.6 dome=9A9C9A stone=EAE2CE
legend M rotunda   "the Minceta tower"            fortress roof=battlement sides=14 height=7 stone=D8CCB4
legend L rotunda   "Lovrijenac"                   fortress roof=battlement sides=12 height=6 stone=D8CCB4
legend Z inn       "the Lazaretto"
legend I inn       "the inn of Saint Blaise"

gate 1 "the Pile gate"
gate 2 "the Ploce gate"

map
"""

# ---------------------------------------------------------------- Seville

def seville():
    """Seville in 1632, the port of the Indies: the walled city on the north bank of the
    Guadalquivir (here running west, as the old map has it), with the Cathedral and the Giralda,
    the Alcazar, the Casa de Contratacion that rules the trade of America, the Lonja, the Torre del
    Oro on the bank, the Arenal where the fleets unload; across the bridge of boats, Triana of the
    potters, and the castle of San Jorge where the Inquisition keeps its prisoners. 68 x 52 cells
    (enlarged by 1.5 about the middle, the landmarks drawn bigger than life;
    the roads Seville.2 and .3 in world.txt start beyond the new edge)."""
    d = Draft(68, 52, 1492)
    RIVER = [(69.5, 22.25, 2.5), (52.75, 23.75, 2.6), (33.25, 24.5, 2.7), (15.25, 23.75, 2.7), (-1.5, 23.75, 2.6)]
    d.water(RIVER)
    city = [(4.75, 2), (30.25, 0.5), (57.25, 2), (65.5, 8.75), (65.5, 21.75), (4, 21.75), (4, 13.25)]   # (down to the water: no wall on the river)
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, city))
    d.walled(lambda x, y: 6 <= x <= 63 and 28 <= y <= 49 and d.get(x, y) != '~', wall=False)   # Triana
    d.inner_streets()
    for pts in ([(23, 2), (23, 15)],                  # from the Puerta de la Macarena to the Plaza
                [(3, 16), (28, 16)],                  # from the Puerta de Triana to the Lonja
                [(26, 4), (56, 4)],                   # behind the Cathedral
                [(42, 4), (45, 13)], [(45, 13), (65, 9)],   # past the Giralda to the Puerta de Carmona
                [(27, 19), (27, 28)],                 # the bridge of boats over the river to Triana
                [(27, 28), (27, 49)], [(8, 36), (60, 36)], [(12, 42), (54, 45)]):
        d.street(pts)
    d.rect(20, 9, 25, 13, ':', only=('h', '.'))     # the Plaza de San Francisco
    d.rect(5, 18, 26, 20, ':', only=('h', '.'))     # the Arenal, along the river
    d.stalls(20, 9, 25, 13, 0.3)
    d.rect(27, 6, 41, 12, 'C')    # the Cathedral
    d.rect(42, 9, 43, 10, 'G')    # the Giralda
    d.rect(29, 14, 34, 19, 'L')   # the Lonja
    d.rect(36, 14, 44, 19, 'A')   # the Alcazar
    d.rect(47, 13, 53, 17, 'H')   # the Casa de Contratacion
    d.rect(57, 19, 60, 22, 'T')   # the Torre del Oro, on the bank
    d.rect(51, 4, 56, 8, 'P')     # the Casa de Pilatos
    d.rect(15, 9, 19, 14, 'M')    # the town hall
    d.rect(9, 4, 14, 8, 'J')      # the Jesuits' college of San Hermenegildo
    d.rect(10, 10, 14, 13, 'I')   # the Posada del Lucero
    d.rect(19, 30, 25, 35, 'K')   # the castle of San Jorge, the Inquisition's
    d.rect(39, 38, 43, 41, 'R')   # a potter's workshop of Triana
    d.ring('CGLAHPMJKRIT')
    d.put(22, 11, 'f')
    d.gates([(3, 16, '1', 'h', [(0, 16), (-3, 16)]),     # the Puerta de Triana, west
             (23, 2, '2', 'v', [(23, -1), (22, -3)]),     # the Puerta de la Macarena, north
             (65, 9, '3', 'h', [(68, 9), (71, 8)])])      # the Puerta de Carmona, east
    d.towers(7)
    d.lanes(7)
    d.connect(avoid=RIVER, gap=2)
    for (x, y) in ((9, 21), (15, 21), (21, 21), (12, 27), (45, 27), (50, 21)):   # boats at the banks
        placed = False
        for r in range(0, 4):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    xx, yy = x + dx, y + dy
                    if placed or max(abs(dx), abs(dy)) != r or d.get(xx, yy) != '~':
                        continue
                    if any(d.get(xx + a, yy + b) in ('.', ':', 'h') for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                        d.put(xx, yy, 'g')
                        placed = True
    d.trees(0.015)
    return d.done()


SEVILLE_HEADER = """\
# Seville in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (seville); the script is the master copy for now.
# Characters as in paris.map ('g' a mooring post with a boat); the capital letters are landmarks.

style spain
ground 1.0
towerroof flat

legend C cathedral "the Cathedral of Seville"     roof=lead stone=D8C6A2
legend G tower     "the Giralda"                  height=19 top=belfry brick stone=D9A274
legend L college   "the Lonja"                    depth=4 storeys=2 roof=tile stone=E2D2B2 floors=1
legend A palace    "the Alcazar of Seville"       depth=4 storeys=2 roof=tile stone=E6D8BC floors=1
legend H hall      "the Casa de Contratacion"     nocampanile roof=tile stone=E2D4B8 floors=1
legend T rotunda   "the Torre del Oro"            fortress roof=battlement sides=12 height=5 stone=D8B882
legend P mansion   "the Casa de Pilatos"          style=spain
legend M hall      "the town hall of Seville"     nocampanile roof=tile stone=E4D8C0
legend J college   "the college of San Hermenegildo"  depth=3 storeys=2 roof=tile stone=E6DCC6
legend I inn       "the Posada del Lucero"
legend K fortress  "the castle of San Jorge"      height=5 roofed stone=C9B894
legend R mansion   "a potter's workshop of Triana"

gate 1 "the Puerta de Triana"
gate 2 "the Puerta de la Macarena"
gate 3 "the Puerta de Carmona"

map
"""


# ---------------------------------------------------------------- Granada

def granada():
    """Granada in 1632: the Alhambra, the red castle of the Nasrid kings, on its hill to the
    south-east, walled, with the Nasrid palaces, the unfinished palace of Charles V and the
    Alcazaba; the Generalife's gardens above it; the Albaicin, the old Moorish quarter, on the
    hill to the north-west; below, the Christian city of the Cathedral, the Capilla Real where the
    Catholic Kings lie, the Alcaiceria (the silk market) and the Corral del Carbon. 40 x 32 cells."""
    d = Draft(40, 32, 1492)
    d.walled(lambda x, y: True, wall=False)
    # the Alhambra drawn bigger than life, so that its palaces have rooms and courts to walk in
    # (the rule for plot buildings: roomy inside)
    alhambra = [(20.5, 13.5), (27, 11.5), (35, 12), (37, 16.5), (34.5, 23), (24, 23.5), (20, 18.5)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, alhambra))
    d.inner_streets()
    for pts in ([(2, 8), (12, 14), (20, 16)], [(3, 28), (12, 25), (20, 22)], [(12, 14), (12, 30)],
                [(5, 20), (19, 20)], [(14, 2), (16, 12)], [(20, 16), (21, 17)], [(37, 15), (39, 13)]):
        d.street(pts)
    d.rect(8, 24, 11, 27, ':', only=('h', '.'))     # the Plaza Bib-Rambla
    d.rect(15, 12, 19, 14, ':', only=('h', '.'))    # the Plaza Nueva
    d.stalls(8, 24, 11, 27, 0.3)
    d.rect(35, 8, 39, 13, ',', only=('h', '.'))     # the gardens of the Generalife
    for y in range(8, 14):
        for x in range(35, 40):
            if d.get(x, y) == ',' and (x + y) % 3 == 0: d.put(x, y, 't')
    for y in range(0, 7):                            # the Sacromonte, its caves and prickly pears
        for x in range(20, 40):
            if d.get(x, y) == 'h' and (x * 3 + y) % 4 == 0: d.put(x, y, ',')
    d.rect(23, 13, 28, 18, 'N')   # the Nasrid palaces: the Comares palace round the Court of the Myrtles
    d.rect(29, 13, 34, 18, 'L')   # the Nasrid palaces: the palace of the Lions round its court
    d.rect(30, 19, 33, 21, 'V')   # the palace of Charles V, unfinished, south of them (clear of the gate)
    d.rect(21, 17, 22, 19, 'K')   # the Alcazaba, at the western tip
    d.rect(36, 10, 37, 11, 'E')   # the Generalife
    d.rect(8, 16, 14, 20, 'C')    # the Cathedral
    d.rect(7, 21, 11, 23, 'R')    # the Capilla Real
    d.rect(12, 21, 16, 24, 'S')   # the Alcaiceria, the silk market
    d.rect(17, 24, 21, 27, 'I')   # the Corral del Carbon (clear of the road up to the Alhambra)
    d.rect(16, 10, 19, 11, 'H')   # the Royal Chancery, on the Plaza Nueva
    d.rect(4, 11, 7, 13, 'U')     # the University
    d.rect(10, 4, 14, 6, 'Q')     # San Salvador, the mosque of the Albaicin that is a church now
    d.ring('NLVKCRSIHUQ')
    d.put(17, 13, 'f')
    d.gates([(27, 23, '1', 'v', [(27, 25), (22, 26)]),
             (36, 15, '2', 'h', [(37, 15)])])
    d.towers(5)
    d.lanes(6)
    d.connect(gap=2)
    d.trees(0.02)
    return d.done()


GRANADA_HEADER = """\
# Granada in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (granada); the script is the master copy for now.
# Characters as in paris.map; the capital letters are landmarks.

style granada
ground 1.4
towerroof flat

legend N palace    "the Court of the Myrtles, in the Nasrid palaces"  depth=3 storeys=1 roof=tile stone=D8A27A
legend L palace    "the Court of the Lions, in the Nasrid palaces"    depth=3 storeys=1 roof=tile stone=DCAA82
legend V palace    "the palace of Charles V"      depth=2 storeys=2 roof=tile stone=D9C49E
legend K tower     "the Torre de la Vela"         height=9 top=battlement stone=B8603E
legend E mansion   "the Generalife"               style=granada
legend C cathedral "the Cathedral of Granada"     front=flat roof=lead stone=E2D6BE
legend R chapel    "the Capilla Real"             stone=E0D2B4
legend S halle     "the Alcaiceria"
legend I inn       "the Corral del Carbon"
legend H hall      "the Royal Chancery"           nocampanile roof=tile stone=E2D6BE
legend U college   "the University of Granada"    depth=3 storeys=2 roof=tile stone=E6DCC6
legend Q church    "San Salvador"                 roof=tile front=flat stone=EEE8DC

gate 1 "the Gate of Justice"
gate 2 "the gate of the Generalife"

map
"""

# ---------------------------------------------------------------- Oxford and Cambridge

def oxford():
    """Oxford in 1632: the walled town on its gravel spit between the Thames (here the Isis, along
    the south of the map, joining the old map's river at both edges) and the Cherwell; Christ
    Church, the Bodleian Library and the Schools, St Mary's, Merton, New College, Magdalen outside
    the East Gate by the Physic Garden, the Castle (the county gaol). 52 x 42 cells (enlarged by
    1.5, about the middle across and about the Isis down, so that the river
    meets the old map where it did; the landmarks drawn bigger than life)."""
    d = Draft(52, 42, 1167)
    THAMES = [(-1.75, 30.95, 1.8), (14.75, 32, 1.8), (29.75, 32.9, 1.8), (53.75, 34.4, 1.9)]
    d.water(THAMES)
    d.water([(46.25, 0.5, 0.9), (47, 9.5, 0.9), (44.75, 24.5, 0.9), (41.75, 32, 1.0)])   # the Cherwell
    walls = [(7.25, 3.5), (32.75, 2), (35, 12.5), (34.25, 23), (8.75, 25.25), (7.25, 14)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, walls))
    d.walled(lambda x, y: (x < 7 or x > 35 or y < 2 or y > 25) and d.get(x, y) == ' ' and 1 <= y <= 28 and 2 <= x <= 43,   # (not on the map's edge)
             wall=False)   # the suburbs: St Giles to the north, St Aldate's and St Thomas's
    d.inner_streets()
    for pts in ([(19, -1), (19, 2)], [(1, 12), (51, 12)], [(12, 24), (13, 29), (21, 30), (21, 37)], [(9, 6), (33, 6)]):
        d.street(pts)
    d.rect(13, 11, 16, 14, ':', only=('h', '.'))     # the Cornmarket, by Carfax
    d.stalls(13, 11, 16, 14, 0.5)
    d.rect(18, 11, 19, 12, 'X')    # Carfax, the crossroads tower
    d.rect(16, 15, 25, 23, 'C')    # Christ Church
    d.rect(17, 4, 23, 9, 'B')      # the Bodleian Library and the Schools (off the wall's line: the North Gate is in it)
    d.rect(24, 9, 28, 11, 'M')     # St Mary's, the university church, on the High
    d.rect(27, 16, 32, 21, 'R')    # Merton College
    d.rect(26, 3, 31, 8, 'N')      # New College
    d.rect(37, 6, 42, 11, 'G')     # Magdalen College, outside the East Gate
    d.rect(37, 17, 42, 22, ',', only=('h', '.', ' '))   # the Physic Garden, new walled
    for y in range(17, 23):
        for x in range(37, 43):
            if d.get(x, y) == ',' and (x + y) % 3 == 0: d.put(x, y, 't')
    d.rect(2, 14, 6, 19, 'K')      # the Castle
    d.rect(11, 7, 15, 10, 'I')     # the Mitre
    d.ring('CBMRNGKI')
    d.gates([(19, 2, '1', 'v', [(19, 0), (19, -1)]),      # the North Gate (Bocardo)
             (35, 12, '2', 'h', [(37, 12)]),              # the East Gate, to Magdalen and London
             (12, 23, '3', 'v', [(12, 26)]),              # the South Gate, to Folly Bridge
             (7, 12, '4', 'h', [(4, 12), (-1, 12)])])     # the West Gate, by the Castle
    d.towers(6)
    d.lanes(7)
    d.connect(avoid=THAMES, gap=2)
    for (x, y) in ((18, 29), (25, 30), (12, 29)):
        done = False
        for r in range(0, 3):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    xx, yy = x + dx, y + dy
                    if done or max(abs(dx), abs(dy)) != r or d.get(xx, yy) != '~': continue
                    if any(d.get(xx + a, yy + b) in ('.', 'h') for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                        d.put(xx, yy, 'g'); done = True
    d.trees(0.03)
    return d.done()


OXFORD_HEADER = """\
# Oxford in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (oxford); the script is the master copy for now.
# Characters as in paris.map ('g' a mooring post with a boat); the capital letters are landmarks.

style london
ground 1.0
towerroof flat

legend X tower     "Carfax"                       height=7 top=battlement stone=C9B48A
legend C college   "Christ Church"                depth=4 storeys=2 roof=slate stone=D8C9A0 floors=1
legend B college   "the Bodleian Library"         depth=4 storeys=3 roof=lead stone=D9C8A0 library=Duke_Humfrey's_Library
legend M church    "St Mary's, the university church"  roof=lead stone=D2C29A
legend R college   "Merton College"               depth=4 storeys=2 roof=slate stone=D0BE96
legend N college   "New College"                  depth=4 storeys=2 roof=slate stone=D6C69E
legend G college   "Magdalen College"             depth=4 storeys=2 roof=slate stone=D8C9A2
legend K rotunda   "Oxford Castle"                fortress roof=battlement sides=12 height=6 stone=B8A888
legend I inn       "the Mitre"

gate 1 "the North Gate"
gate 2 "the East Gate"
gate 3 "the South Gate"
gate 4 "the West Gate"

map
"""


def cambridge():
    """Cambridge in 1632: the colleges along the Cam (here entering at the south and leaving at
    the north, towards the Wash), King's College Chapel, Trinity's Great Court, St John's,
    Queens', Christ's, Emmanuel, Great St Mary's, the Round Church, Market Hill, Hobson's
    stables, the Castle on its mound over the Great Bridge. 54 x 42 cells, unwalled (enlarged by
    1.5, about the Cam across, so that it leaves the top edge where it did,
    and about the middle down; the landmarks drawn bigger than life)."""
    d = Draft(54, 42, 1209)
    CAM = [(14.25, 38.25, 1.3), (15.75, 30, 1.3), (17.25, 18, 1.3), (18.75, 7.5, 1.3), (21.75, -1.5, 1.3)]
    d.water(CAM)
    d.walled(lambda x, y: x >= 19 or (x <= 12 and y <= 9), wall=False)
    d.inner_streets()
    for pts in ([(19, 6), (54, 9)], [(32, 0), (34, 40)], [(19, 19), (54, 21)], [(25, 30), (54, 33)], [(10, 4), (19, 6)]):
        d.street(pts)
    d.rect(32, 18, 38, 23, ':', only=('h', '.'))    # Market Hill
    d.stalls(32, 18, 38, 23, 0.3)
    d.rect(20, 9, 28, 15, 'T')    # Trinity College
    d.rect(20, 1, 26, 6, 'J')     # St John's College
    d.rect(22, 18, 29, 21, 'K')   # King's College Chapel
    d.rect(20, 24, 25, 29, 'Q')   # Queens' College
    d.rect(38, 12, 43, 17, 'C')   # Christ's College
    d.rect(40, 25, 45, 30, 'E')   # Emmanuel College
    d.rect(29, 15, 33, 17, 'M')   # Great St Mary's, on Market Hill
    d.rect(28, 1, 31, 4, 'R')     # the Round Church
    d.rect(7, 1, 11, 6, 'S')      # the Castle
    d.rect(34, 24, 38, 27, 'H')   # Hobson's stables
    d.rect(27, 24, 31, 27, 'I')   # the Rose
    d.ring('TJKQCEMRSHI')
    d.put(24, 12, 'f')            # Trinity's fountain
    d.lanes(7)
    d.connect(avoid=CAM, gap=2)
    for y in range(0, 42):        # the Backs: meadows and trees west of the river
        for x in range(0, 18):
            if d.get(x, y) == 'h' and y > 9: d.put(x, y, ',' if (x + y) % 3 else 't')
    for (x, y) in ((17, 22), (16, 33), (17, 13)):
        done = False
        for r in range(0, 3):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    xx, yy = x + dx, y + dy
                    if done or max(abs(dx), abs(dy)) != r or d.get(xx, yy) != '~': continue
                    if any(d.get(xx + a, yy + b) in ('.', 'h', ',') for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                        d.put(xx, yy, 'g'); done = True
    d.trees(0.02)
    return d.done()


CAMBRIDGE_HEADER = """\
# Cambridge in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (cambridge); the script is the master copy for now.
# Characters as in paris.map ('g' a mooring post with a boat); the capital letters are landmarks.

style london
ground 1.0
towerroof flat

legend T college   "Trinity College"              depth=4 storeys=2 roof=slate stone=D8C9A0
legend J college   "St John's College"            depth=4 storeys=2 roof=slate brick
legend K chapel    "King's College Chapel"        plain height=10 stone=E2D8BC
legend Q college   "Queens' College"              depth=4 storeys=2 roof=slate brick
legend C college   "Christ's College"             depth=4 storeys=2 roof=slate stone=D6C69E
legend E college   "Emmanuel College"             depth=4 storeys=2 roof=slate brick
legend M church    "Great St Mary's"              roof=lead stone=D2C29A
legend R rotunda   "the Round Church"             sides=10 roof=pyramid height=4 stone=D0C098
legend S rotunda   "Cambridge Castle"             fortress roof=battlement sides=12 height=5 stone=B8A888
legend H mansion   "Hobson's stables"  sign=horseshoe
legend I inn       "the Rose"

map
"""

# ---------------------------------------------------------------- Central Europe

def zurich():
    """Zurich in 1632, Protestant and neutral: the walled town on both banks of the Limmat where
    it leaves the lake (the lake enters from the east edge, the Limmat leaves at the top, as on the
    old map); the Grossmunster, the Fraumunster, St Peter with its great clock, the Rathaus on the
    river, the Lindenhof, the Wellenberg prison tower in the water. 52 x 42 cells (enlarged by 1.5
    about the middle, the landmarks drawn bigger than life)."""
    d = Draft(52, 42, 1218)
    lake = [(53.75, 19.5), (53.75, 34.5), (43.25, 36), (34.25, 33), (29, 29.25), (32.75, 24.75), (41.75, 21)]
    for y in range(d.H):
        for x in range(d.W):
            if Draft.inpoly(x + 0.5, y + 0.5, lake): d.put(x, y, '~')
    LIMMAT = [(29.75, 29.25, 1.5), (26.75, 16.5, 1.4), (24.5, 6, 1.4), (23, -1.5, 1.4)]
    d.water(LIMMAT)
    town = [(1.25, 1.5), (47.75, 1.5), (50.75, 13.5), (44.75, 20.25), (34.25, 23.25), (29.75, 30), (22.25, 37.5),
            (2.75, 37.5), (0.5, 19.5)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, town))
    d.inner_streets()
    for pts in ([(1, 16), (33, 16)],                 # the Rennweg, and the upper bridge by the Rathaus
                [(28, 13), (49, 9)], [(12, 1), (12, 37)], [(37, 3), (37, 21)],
                [(19, 9), (30, 9)]):                 # the lower bridge
        d.street(pts)
    d.rect(31, 6, 36, 11, 'G')    # the Grossmunster
    d.rect(18, 21, 23, 25, 'F')   # the Fraumunster
    d.rect(15, 18, 23, 20, ':', only=('h', '.'))    # the Munsterhof
    d.rect(24, 11, 24, 15, ':', only=('h', '.'))    # the Weinplatz, by the river
    d.rect(4, 10, 8, 14, 'P')     # St Peter
    d.rect(28, 18, 32, 21, 'R')   # the Rathaus, on the river
    d.rect(15, 5, 20, 8, ',', only=('h', '.'))      # the Lindenhof
    for (x, y) in ((15, 5), (18, 5), (16, 7), (20, 6), (19, 8)):
        if d.get(x, y) == ',': d.put(x, y, 't')
    d.rect(31, 27, 32, 28, 'W')   # the Wellenberg, in the river
    d.rect(40, 4, 45, 9, 'C')     # the Carolinum
    d.rect(19, 11, 23, 14, 'I')   # the Storchen
    d.ring('GFPRWCI')
    d.stalls(15, 18, 23, 20, 0.3)
    for x in range(27, 31):
        if d.get(x, 27) == '~': d.put(x, 27, '=')    # the plank walk out to the Wellenberg
    moor = 0
    for (x, y) in ((33, 22), (31, 24), (36, 22), (24, 19), (24, 22), (25, 25)):
        if moor >= 2: break
        done = False
        for r in range(0, 3):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    xx, yy = x + dx, y + dy
                    if done or max(abs(dx), abs(dy)) != r or d.get(xx, yy) != '~': continue
                    if any(d.get(xx + a, yy + b) in ('.', ':') for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                        d.put(xx, yy, 'g'); moor += 1; done = True
    d.gates([(1, 16, '1', 'h', [(-1, 16)]),
             (40, 1, '2', 'v', [(40, -1)])])
    d.towers(6)
    d.lanes(7)
    d.connect(gap=2)
    d.trees(0.03)
    return d.done()


ZURICH_HEADER = """\
# Zurich in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (zurich); the script is the master copy for now.
# Characters as in paris.map ('g' a mooring post with a boat); the capital letters are landmarks.

style german
ground 1.1
towerroof cone

legend G church    "the Grossmünster"             towers roof=tile stone=D8CDB4
legend F church    "the Fraumünster"              roof=tile stone=D6CAAE
legend P church    "St Peter"                     roof=tile stone=D8CDB4
legend R hall      "the Rathaus of Zurich"        nocampanile roof=tile stone=E2D6BC floors=1
legend W tower     "the Wellenberg"               height=8 top=pyramid stone=B8A888
legend C college   "the Carolinum"                depth=4 storeys=2 roof=tile stone=E2D8C0 floors=1
legend I inn       "the Storchen"

gate 1 "the Rennweg gate"
gate 2 "the Niederdorf gate"

map
"""


def nuremberg():
    """Nuremberg in 1632, a free city of the Empire, its walls the strongest in Germany; this
    summer the King of Sweden's army is camped round it and Wallenstein's at Zirndorf. The
    Kaiserburg on its rock at the north, the Pegnitz through the middle, St Sebaldus and the
    Hauptmarkt with the Schoner Brunnen to the north of it, St Lorenz to the south, the Holy
    Ghost hospital on the river, Durer's house under the castle. Round it all, the King's lines:
    a ring of earth ramparts ('e') with guns ('u') behind them, broken where the roads and the river
    pass, and the Swedish tents ('n') between the lines and the walls. 45 x 36 cells (the town of
    35 x 28 in the middle, moved by 5, 4)."""
    d = Draft(45, 36, 1050)
    X, Y = 5, 4   # the old town's place in the bigger map
    PEGNITZ = [(46, 19, 0.9), (30 + X, 15 + Y, 0.9), (20 + X, 15.5 + Y, 1.0), (10 + X, 16.5 + Y, 1.0), (-1, 21.5, 1.1)]
    d.water(PEGNITZ)
    cx, cy = 17 + X, 14 + Y
    inside = lambda x, y: ((x - cx) / 15.5) ** 2 + ((y - cy) / 12.5) ** 2 <= 1.0
    d.walled(inside)
    d.inner_streets(river=False)   # the houses stand in the Pegnitz, as they do
    for pts in ([(17, 2), (17, 26)], [(2, 9), (32, 9)], [(2, 21), (32, 21)], [(7, 13), (7, 19)], [(27, 13), (27, 19)]):
        d.street([(a + X, b + Y) for a, b in pts])   # the first crosses the river by the Fleischbrucke; the last two are bridges
    d.rect(18 + X, 10 + Y, 22 + X, 12 + Y, ':', only=('h', '.'))   # the Hauptmarkt
    d.stalls(18 + X, 10 + Y, 22 + X, 12 + Y, 0.35)
    d.rect(11 + X, 2 + Y, 18 + X, 4 + Y, 'K')     # the Kaiserburg
    d.rect(12 + X, 6 + Y, 15 + X, 8 + Y, 'S')     # St Sebaldus
    d.rect(19 + X, 18 + Y, 23 + X, 20 + Y, 'L')   # St Lorenz
    d.rect(12 + X, 10 + Y, 16 + X, 13 + Y, 'R')   # the Rathaus
    d.rect(22 + X, 12 + Y, 26 + X, 14 + Y, 'H')   # the hospital of the Holy Ghost, on the Pegnitz (not across it: the river must run on)
    d.rect(7 + X, 5 + Y, 11 + X, 8 + Y, 'D')      # Durer's house
    d.rect(23 + X, 5 + Y, 27 + X, 8 + Y, 'M')     # an instrument maker's workshop
    d.rect(6 + X, 10 + Y, 10 + X, 13 + Y, 'I')    # the Golden Goose
    d.ring('KSLRHDMI')
    d.put(20 + X, 11 + Y, 'f')            # the Schoner Brunnen
    d.gates([(32 + X, 9 + Y, '1', 'h', [(34 + X, 9 + Y), (46, 9 + Y)]),       # the Laufer gate, east
             (17 + X, 26 + Y, '2', 'v', [(17 + X, 28 + Y), (17 + X, 37)]),    # the Frauentor, south
             (2 + X, 21 + Y, '3', 'h', [(0 + X, 21 + Y), (-2, 21 + Y)]),      # the Spittlertor, west, to Zirndorf
             (8 + X, 4 + Y, '4', 'v', [(7 + X, 2 + Y), (5 + X, -1)])])        # the Tiergartner gate, north-west
    d.towers(5)
    d.lanes(8)
    d.connect(avoid=PEGNITZ, gap=2)
    d.trees(0.02)
    # the King of Sweden's lines, dug this June: a ring of earth, open for the roads and the river
    e = lambda x, y: ((x - cx) / 20.5) ** 2 + ((y - cy) / 16.5) ** 2
    ring = []
    for y in range(d.H):
        for x in range(d.W):
            if e(x, y) > 1 or not any(e(x + a, y + b) > 1 for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                continue
            ring.append((x, y))
            if d.get(x, y) in (' ', ',', 't'):
                d.put(x, y, 'e')
    # guns behind the ramparts, every so often, on the inner side
    import math
    for k, (x, y) in enumerate(sorted(ring, key=lambda q: math.atan2(q[1] - cy, q[0] - cx))):
        if k % 7 != 3 or d.get(x, y) != 'e':
            continue
        ix, iy = x + (1 if x < cx else -1 if x > cx else 0), y + (1 if y < cy else -1 if y > cy else 0)
        if d.get(ix, iy) == ' ':
            d.put(ix, iy, 'u')
    # the camp: rows of tents between the walls and the lines
    rnd = random.Random(1632)
    for y in range(d.H):
        for x in range(d.W):
            w_e = ((x - cx) / 15.5) ** 2 + ((y - cy) / 12.5) ** 2
            if d.get(x, y) == ' ' and w_e > 1.25 and e(x, y) < 0.82 and (x + 2 * y) % 3 == 0 and rnd.random() < 0.6:
                d.put(x, y, 'n')
    return d.done()


def zirndorf():
    """Zirndorf, a village of the Margrave of Ansbach on the Bibert, west of Nuremberg, and the
    Alte Veste, the ruin of an old castle on the wooded ridge to the north of it. In the summer of
    1632 Wallenstein's army camps on the ridge: rows of tents ('n'), a line of gabions ('v')
    facing Nuremberg to the east, with guns ('u') in it. The town file's "camp" keeps them out of
    the world until the army comes. 30 x 24 cells."""
    d = Draft(30, 24, 1632)
    # (the Bibert, a stream south of the village, is left out: it would join no river of the old map)
    d.street([(0, 16), (29, 16)], width=1, ch='_')                   # the road east to Nuremberg
    d.street([(9, 11), (9, 20)], width=1)                            # the village street
    for (x0, y0, x1, y1) in ((4, 13, 8, 15), (10, 13, 13, 15), (4, 17, 8, 19), (10, 17, 14, 19), (15, 17, 18, 19)):
        d.rect(x0, y0, x1, y1, 'h')
    d.rect(10, 10, 14, 12, 'C')       # St Rochus
    d.rect(12, 2, 15, 4, 'A')         # the Alte Veste, on the ridge
    d.rect(17, 12, 21, 15, 'W')       # a farm by the camp (the Duke's quarters)
    d.ring('CW')
    # the ridge: woods north of the village
    rnd = random.Random(7)
    for y in range(0, 11):
        for x in range(30):
            if d.get(x, y) == ' ' and rnd.random() < 0.25:
                d.put(x, y, 't')
    # the camp: tents in rows on the ridge, gabions along its eastern face, with guns
    for y in range(3, 12, 2):
        for x in range(17, 26, 2):
            if d.get(x, y) in (' ', 't'):
                d.put(x, y, 'n')
    for y in range(1, 15):
        if d.get(27, y) in (' ', 't'):
            d.put(27, y, 'u' if y % 4 == 2 else 'v')
    return d.done()


ZIRNDORF_HEADER = """\
# Zirndorf in 1632, and the Alte Veste on the ridge above it.  One character is one tile; north
# is at the top. Drawn first by tools/draft_towns.py (zirndorf); the script is the master copy.
# Characters as in paris.map; 'n' a tent, 'v' gabions, 'u' a gun: the camp of Wallenstein's army,
# in the world only from the event wallenstein-comes until siege-ends (events.txt).

style german
ground 1.0
camp wallenstein-comes until siege-ends

legend A fortress  "the Alte Veste"               height=3 stone=A89878
legend C church    "St Rochus"                    roof=tile stone=C8B898
legend W mansion   "a farm by the Alte Veste"

map
"""


def stockholm():
    """Stockholm in 1632: the town on Stadsholmen, between Lake Malaren (west) and the Saltsjon,
    the Baltic's inlet (east); Tre Kronor, the castle, at its north end, with its great tower;
    Storkyrkan beside it and the Stortorget; the German church; Skeppsbron, the quay along the
    east shore; Riddarholmen to the west with its church; Helgeandsholmen on the north bridge;
    Norrmalm across the water to the north, Sodermalm to the south across the bridge at the
    sluice. 60 x 48 cells (enlarged by 1.5 about the middle, the landmarks
    drawn bigger than life)."""
    d = Draft(60, 48, 1632)
    for y in range(d.H):
        for x in range(d.W):
            d.put(x, y, '~')
    rnd = random.Random(16)
    def land(inside, fill='h'):
        for y in range(d.H):
            for x in range(d.W):
                if inside(x, y):
                    d.put(x, y, fill)
    # Norrmalm, the north shore; Sodermalm, the south, higher and greener
    land(lambda x, y: y <= 9 + (2 if 18 < x < 45 else 0) - (2 if x < 9 or x > 52 else 0))
    land(lambda x, y: y >= 39 - (2 if 21 < x < 42 else 0) + (2 if x < 6 or x > 54 else 0))
    for y in range(40, 48):   # Sodermalm: gardens and fields beyond the first houses
        for x in range(60):
            if d.get(x, y) == 'h' and (y >= 45 or abs(x - 31.5) > 19.5):
                d.put(x, y, ',' if rnd.random() < 0.7 else 't')
    for y in range(0, 2):     # Norrmalm: thinner towards the edge of the map
        for x in range(60):
            if d.get(x, y) == 'h' and y <= 0 and rnd.random() < 0.6:
                d.put(x, y, ',' if rnd.random() < 0.8 else 't')
    # Stadsholmen, Riddarholmen, Helgeandsholmen
    land(lambda x, y: ((x - 32.25) / 12.3) ** 2 + ((y - 25.5) / 9.45) ** 2 <= 1)
    land(lambda x, y: ((x - 13.2) / 3.9) ** 2 + ((y - 27) / 5.1) ** 2 <= 1)
    land(lambda x, y: 35 <= x <= 43 and 13 <= y <= 14, fill=',')
    # streets: the two long streets of Stadsholmen, cross lanes, Skeppsbron along the east shore
    d.street([(24, 18), (22, 31)], width=1)          # Vasterlanggatan
    d.street([(39, 18), (40, 31)], width=1)          # Osterlanggatan
    for y in (22, 27, 31):
        d.street([(22, y), (40, y)], width=1)
    for y in range(16, 36):                           # Skeppsbron: the quay on the east shore
        for x in range(59, 30, -1):
            if d.get(x, y) != '~':
                d.put(x, y, '.')
                if d.get(x + 1, y) == '~' and y % 3 == 0:
                    d.put(x + 1, y, 'g')
                break
    d.street([(37, 4), (37, 12)], width=2)           # Norrmalm, from the north bridge
    d.street([(6, 6), (57, 6)], width=1)
    d.street([(31, 39), (31, 45)], width=2)          # Sodermalm, from the sluice
    d.street([(9, 40), (54, 40)], width=1)
    # bridges: north over Helgeandsholmen to Norrmalm; south at the sluice; west to Riddarholmen
    for y in range(10, 19):
        for x in (37, 38):
            d.put(x, y, '=' if d.get(x, y) == '~' else '.')
    for y in range(34, 40):
        for x in (31, 32):
            d.put(x, y, '=' if d.get(x, y) == '~' else '.')
    for x in range(15, 22):
        if d.get(x, 27) == '~':
            d.put(x, 27, '=')
    # the landmarks
    d.rect(22, 18, 23, 19, 'T')       # the tower of Tre Kronor
    d.rect(24, 15, 35, 20, 'K')       # Tre Kronor, the castle
    d.rect(33, 22, 37, 24, 'S')       # Storkyrkan, the great church
    d.rect(28, 24, 32, 26, ':')       # the Stortorget, the market
    d.stalls(29, 24, 32, 26, 0.5)
    d.put(30, 24, 'o')
    d.rect(35, 26, 39, 28, 'G')       # the German church
    d.rect(10, 26, 14, 28, 'R')       # the Riddarholm church
    d.rect(25, 28, 29, 31, 'I')       # an inn
    d.ring('TKSGRI')
    for y in range(d.H):          # no houses on the edges of the map: their doors would face the lake or the sea
        for x in range(d.W):
            if d.get(x, y) == 'h' and (x in (0, 1, d.W - 2, d.W - 1) or y in (0, d.H - 1)):
                d.put(x, y, ',')
    for y in range(19, 34):       # Riddarholmen: its church among gardens (houses there had no door to a street)
        for x in range(7, 19):
            if d.get(x, y) == 'h' and ((x - 13.2) / 3.9) ** 2 + ((y - 27) / 5.1) ** 2 <= 1:
                d.put(x, y, ',')
    d.lanes(7)
    # a road north out of Norrmalm, towards Uppsala
    for y in range(0, 5):
        if d.get(37, y) not in ('~',):
            d.put(37, y, '_'); d.put(38, y, '_')
    return d.done()


STOCKHOLM_HEADER = """\
# Stockholm in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (stockholm); the script is the master copy for now.
# Characters as in paris.map; the capital letters are landmarks.

style stockholm
ground 1.0
towerroof cone

legend T tower     "the tower of Tre Kronor"      height=16 top=belfry spire stone=D2C2A2 roof=5E7E6E
legend K palace    "Tre Kronor, the King's castle"  depth=4 storeys=3 roof=slate stone=D8C8A8 floors=1
legend S church    "Storkyrkan, the great church"  roof=tile stone=C8885A
legend G church    "the German church"            roof=5E7E6E stone=C8805A
legend R church    "the Riddarholm church"        roof=slate brick
legend I inn       "the Golden Key"

map
"""


NUREMBERG_HEADER = """\
# Nuremberg in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (nuremberg); the script is the master copy for now.
# Characters as in paris.map; the capital letters are landmarks; and the King of Sweden's lines:
# 'e' a rampart of earth, 'u' a gun, 'n' a tent.

style german
ground 1.1
towerroof cone

legend K fortress  "the Kaiserburg"               height=6 stone=B8805A
legend S church    "St Sebaldus"                  towers roof=slate stone=B87A5A
legend L church    "St Lorenz"                    towers roof=slate stone=B87A5A
legend R hall      "the Rathaus of Nuremberg"     nocampanile roof=slate stone=D8C4A4 floors=2
legend H hall      "the hospital of the Holy Ghost"  nocampanile roof=tile stone=C9A07A floors=1
legend D mansion   "Albrecht Dürer's house"
legend M mansion   "an instrument maker's workshop"  sign=dividers
legend I inn       "the Golden Goose"

gate 1 "the Laufer gate"
gate 2 "the Frauentor"
gate 3 "the Spittlertor"
gate 4 "the Tiergärtner gate"

map
"""


def prague():
    """Prague in 1632, retaken by Wallenstein from the Saxons last month: the Vltava through it
    (entering at the top and leaving at the bottom, as on the old map), the Castle and the choir
    of St Vitus on the hill to the west, the Mala Strana with Wallenstein's new palace below it,
    the stone bridge, the Old Town with its square, the Tyn church, the Old Town hall and its
    astronomical clock, the Clementinum of the Jesuits, the Carolinum, the Jewish town with the
    Old-New synagogue and the old cemetery, the New Town and its horse market. 60 x 47 cells
    (enlarged by 4/3 about the middle, so that the river and the roads stay
    where they were, and the landmarks drawn bigger than life)."""
    d = Draft(60, 47, 1348)
    VLTAVA = [(26.7, -1.3, 2.2), (28, 10.7, 2.2), (31.3, 21.3, 2.3), (30, 33.3, 2.2), (28, 48, 2.2)]
    d.water(VLTAVA)
    oldtown = [(35, 4), (52, 4), (57, 13), (56, 30), (36, 31), (34, 21)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, oldtown))
    d.walled(lambda x, y: x >= 35 and y >= 32 and y <= 45 and x <= 57, wall=False)   # the New Town
    d.walled(lambda x, y: 3 <= x <= 26 and 4 <= y <= 40 and d.get(x, y) != '~', wall=False)   # the Castle and the Mala Strana
    d.inner_streets()
    for pts in ([(23, 21), (35, 21)],                       # the first: the stone bridge
                [(35, 21), (45, 21), (57, 17)],             # through the Old Town to the Powder Gate
                [(42, 3), (42, 14)], [(44, 22), (44, 46)],  # from the Poric gate to the square, and south
                [(3, 12), (16, 12)], [(16, 12), (16, 18), (23, 21)], [(4, 17), (16, 17)],   # the Castle steps
                [(13, 24), (13, 40)], [(37, 33), (56, 40)]):
        d.street(pts)
    d.rect(41, 14, 48, 20, ':', only=('h', '.'))    # the Old Town square
    d.rect(49, 33, 53, 44, ':', only=('h', '.'))    # the horse market
    d.stalls(41, 14, 48, 20, 0.3)
    d.stalls(49, 33, 53, 44, 0.2)
    d.rect(4, 5, 15, 11, 'K')     # Prague Castle
    d.rect(6, 13, 13, 16, 'V')    # the choir of St Vitus
    d.rect(17, 24, 23, 30, 'W')   # the Wallenstein palace
    d.rect(17, 32, 23, 36, ',', only=('h', '.'))    # its garden
    for (x, y) in ((17, 32), (23, 32), (17, 36), (23, 36), (20, 34)):
        d.put(x, y, 't')
    d.rect(49, 13, 54, 18, 'T')   # the Tyn church, on the east side of the square
    d.rect(36, 15, 40, 19, 'H')   # the Old Town hall, at its south-west corner
    d.rect(41, 15, 41, 15, 'Q')   # the tower with the clock, on the square
    d.rect(36, 23, 42, 28, 'C')   # the Clementinum, by the bridge
    d.rect(47, 23, 53, 28, 'U')   # the Carolinum
    d.rect(36, 5, 40, 8, 'J')     # the Old-New synagogue
    d.rect(36, 10, 40, 13, ',', only=('h', '.'))    # the old Jewish cemetery
    for y in range(10, 14):
        for x in range(36, 41):
            if d.get(x, y) == ',' and (x + y) % 2 == 0: d.put(x, y, 't')
    d.rect(44, 8, 48, 11, 'I')    # the Golden Unicorn
    d.ring('KVWTHCUJI')
    d.gates([(57, 17, '1', 'h', [(59, 17), (61, 17)]),
             (42, 4, '2', 'v', [(42, 1), (42, -1)])])
    d.towers(6)
    d.lanes(7)
    d.connect(avoid=VLTAVA, gap=2)
    for (x, y) in ((27, 9), (30, 13), (29, 29), (31, 36), (28, 24)):    # boats along the banks
        placed = False
        for r in range(0, 4):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    xx, yy = x + dx, y + dy
                    if placed or max(abs(dx), abs(dy)) != r or d.get(xx, yy) != '~':
                        continue
                    if any(d.get(xx + a, yy + b) in ('.', 'h', ':') for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                        d.put(xx, yy, 'g')
                        placed = True
    d.trees(0.02)
    return d.done()


PRAGUE_HEADER = """\
# Prague in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (prague); the script is the master copy for now.
# Characters as in paris.map ('g' a mooring post with a boat); the capital letters are landmarks.

style german
ground 1.2
towerroof cone

legend K palace    "Prague Castle"                depth=4 storeys=3 roof=slate stone=E2D8C0
legend V cathedral "St Vitus's Cathedral"         roof=lead stone=C9BC9E
legend W palace    "the Wallenstein palace"       depth=4 storeys=2 roof=tile stone=EEE6D2 floors=1
legend T church    "the Týn church"               towers roof=slate stone=C8B898
legend H hall      "the Old Town hall"            nocampanile roof=tile stone=E2D6BC floors=1
legend Q tower     "the tower of the Old Town hall"  height=10 top=spire stone=D8CAAA
legend C college   "the Clementinum"              depth=4 storeys=2 roof=tile stone=EAE2CE floors=1
legend U college   "the Carolinum"                depth=4 storeys=2 roof=tile stone=E2D6BE floors=1
legend J hall      "the Old-New synagogue"        nocampanile roof=tile brick floors=1
legend I inn       "the Golden Unicorn"

gate 1 "the Powder Gate"
gate 2 "the Poric gate"

map
"""

# ---------------------------------------------------------------- the East

def vienna():
    """Vienna in 1632, the Emperor's residence: the walled town with an arm of the Danube along
    its west and north sides (as on the old map, which starts the river just south of the town);
    St Stephen's with its great south tower, the Hofburg, the Schottenstift, the University of the
    Jesuits, the Graben and the Hoher Markt with the Schranne, the town's court. 52 x 42 cells
    (enlarged by 1.5 about the middle, the landmarks drawn bigger than life; the
    Danube still leaves the top edge where the old map's water meets it, and world.txt's roads
    start beyond the new edges)."""
    d = Draft(52, 42, 1137)
    DANUBE = [(4.25, 36.75, 2.0), (4.25, 27, 2.1), (7.25, 13.5, 2.1), (13.25, 4.5, 2.1), (20, -1.5, 2.1)]
    d.water(DANUBE)
    inside = lambda x, y: ((x - 31.25) / 18.75) ** 2 + ((y - 22.5) / 17.25) ** 2 <= 1.0
    d.walled(inside)
    def riverx(y):   # where the Danube is, row by row
        for (x0, y0, _), (x1, y1, _) in zip(DANUBE, DANUBE[1:]):
            if y1 <= y <= y0: return x0 + (x1 - x0) * (y - y0) / (y1 - y0)
        return 4.25
    d.walled(lambda x, y: 6 <= y <= 39 and riverx(y) + 3.3 < x <= 12 and not inside(x, y) and d.get(x, y) == ' ', wall=False)
    d.inner_streets(river=False)
    for pts in ([(31, 6), (31, 39)], [(13, 22), (49, 22)]):
        d.street(pts)
    d.rect(25, 17, 36, 21, 'S')   # St Stephen's
    d.rect(31, 22, 32, 23, 'Q')   # its south tower
    d.rect(15, 25, 23, 30, 'H')   # the Hofburg
    d.rect(19, 10, 23, 13, 'K')   # the Schottenstift (inside the wall)
    d.rect(39, 14, 44, 19, 'U')   # the University
    d.rect(21, 14, 29, 16, ':', only=('h', '.'))   # the Graben
    d.rect(33, 9, 38, 12, ':', only=('h', '.'))    # the Hoher Markt
    d.stalls(21, 14, 29, 16, 0.3)
    d.stalls(33, 9, 38, 12, 0.3)
    d.rect(39, 9, 43, 12, 'X')    # the Schranne
    d.rect(35, 25, 39, 28, 'I')   # the Red Hedgehog
    d.ring('SQHKUXI')
    d.gates([(31, 39, '1', 'v', [(31, 42)]),     # the Karntnertor, south
             (49, 22, '2', 'h', [(52, 22)]),     # the Stubentor, east
             (13, 22, '3', 'h', [(9, 22)]),      # the Schottentor, west
             (31, 6, '4', 'v', [(31, 3), (31, -1)])])   # the Rotenturmtor, north, to the Danube
    d.towers(5)
    d.lanes(7)
    d.connect(avoid=DANUBE, gap=2)
    moor = 0
    for (x, y) in ((9, 18), (7, 21), (10, 10), (12, 9), (7, 27)):
        if moor >= 3: break
        done = False
        for r in range(0, 3):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    xx, yy = x + dx, y + dy
                    if done or max(abs(dx), abs(dy)) != r or d.get(xx, yy) != '~': continue
                    if any(d.get(xx + a, yy + b) in ('.', ':') for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                        d.put(xx, yy, 'g'); moor += 1; done = True
    d.trees(0.02)
    return d.done()


VIENNA_HEADER = """\
# Vienna in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (vienna); the script is the master copy for now.
# Characters as in paris.map ('g' a mooring post with a boat); the capital letters are landmarks.

style german
ground 1.1
towerroof cone

legend S cathedral "St Stephen's Cathedral"       westtower roof=tile stone=D4C8AE
legend Q tower     "the Steffl"                   height=16 top=spire stone=CFC2A4
legend H palace    "the Hofburg"                  depth=4 storeys=3 roof=slate stone=E6DEC8 floors=1
legend K church    "the Schottenstift"            roof=tile stone=E0D6BE
legend U college   "the University of Vienna"     depth=4 storeys=2 roof=tile stone=E8E0CC floors=1
legend X hall      "the Schranne"                 nocampanile roof=tile stone=E2D6BC
legend I inn       "the Red Hedgehog"

gate 1 "the Kärntnertor"
gate 2 "the Stubentor"
gate 3 "the Schottentor"
gate 4 "the Rotenturmtor"

map
"""


def cracow():
    """Cracow in 1632, the old royal city (the court has gone to Warsaw, but kings are still
    crowned and buried here): the Vistula down the west side (as on the old map), the Wawel on its
    rock at the bend, the walled town with the great market square, the Cloth Hall, St Mary's and
    the town hall tower, the Collegium Maius of the University, and Kazimierz, the town of the
    Jews, to the south-east. 52 x 42 cells (enlarged by 1.5 about the middle,
    the landmarks drawn bigger than life; world.txt's road Cracow.2 starts beyond the new edge)."""
    d = Draft(52, 42, 1257)
    VISTULA = [(4.25, -1.5, 1.8), (3.5, 9, 1.8), (4.25, 19.5, 1.8), (10.25, 30, 1.8), (16.25, 43.5, 1.8)]
    d.water(VISTULA)
    town = [(11.75, 1.5), (41.75, 1.5), (46.25, 7.5), (46.25, 21), (37.25, 24), (20.75, 24), (13.25, 18), (11, 7.5)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, town))
    d.walled(lambda x, y: (y >= 26 and x >= 27) or (21 <= y <= 33 and 9 <= x <= 25 and d.get(x, y) == ' ' and not d.near(x, y, [(a, b, w + 0.6) for (a, b, w) in VISTULA])), wall=False)
    d.inner_streets(river=False)
    for pts in ([(27, 1), (27, 24)], [(12, 13), (46, 13)], [(27, 24), (18, 25)], [(27, 24), (37, 33)], [(28, 28), (49, 28)]):
        d.street(pts)
    d.rect(21, 9, 35, 17, ':', only=('h', '.'))    # the Rynek
    d.rect(24, 11, 32, 14, 'S')   # the Cloth Hall
    d.rect(36, 9, 40, 13, 'M')    # St Mary's
    d.rect(22, 16, 23, 17, 'T')   # the Town Hall tower
    d.rect(14, 8, 19, 13, 'U')    # the Collegium Maius
    d.rect(10, 23, 17, 29, 'W')   # the Wawel castle
    d.rect(19, 29, 23, 33, 'C')   # the Wawel cathedral
    d.rect(39, 34, 43, 37, 'J')   # the Old Synagogue
    d.rect(31, 4, 35, 7, 'I')     # the Pod Baranami
    d.stalls(21, 9, 35, 17, 0.2)
    d.ring('SMTUWCJI')
    d.gates([(27, 1, '1', 'v', [(27, -1)]),          # the Florian gate, north
             (46, 13, '2', 'h', [(49, 13), (52, 13)])])  # the Mikolajska gate, east
    d.towers(5)
    d.lanes(8)
    d.connect(avoid=VISTULA, gap=2)
    d.trees(0.02)
    return d.done()


CRACOW_HEADER = """\
# Cracow in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (cracow); the script is the master copy for now.
# Characters as in paris.map; the capital letters are landmarks.

style german
ground 1.1
towerroof cone

legend S halle     "the Cloth Hall"
legend M church    "St Mary's Basilica"           towers brick roof=slate
legend T tower     "the Town Hall tower"          height=11 top=spire brick
legend U college   "the Collegium Maius"          depth=4 storeys=2 roof=tile brick floors=1
legend W palace    "the Wawel castle"             depth=4 storeys=3 roof=tile stone=E8DEC6 floors=1
legend C cathedral "the Wawel cathedral"          westtower roof=lead stone=D8CCB0
legend J hall      "the Old Synagogue"            nocampanile roof=tile stone=D8CCB4
legend I inn       "the Pod Baranami"

gate 1 "the Florian gate"
gate 2 "the Mikolajska gate"

map
"""


def danzig():
    """Danzig in 1632, the richest city of Poland, and its granary: the sea down the west edge
    (as on the old map), the Vistula's last arm along the north, the Motlawa through the town;
    the Main Town with the Long Market, the Artus Court and the town hall, St Mary's, the Crane
    over the water, the granary island across the river, the Upland gate and the prison tower.
    35 x 28 cells."""
    d = Draft(35, 28, 1361)
    for y in range(d.H):
        for x in range(0, 2):
            if y <= 22: d.put(x, y, '~')
    VISTULA = [(-1, 4, 1.6), (12, 4.5, 1.6), (24, 4, 1.6), (36, 3.5, 1.6)]
    MOTLAWA = [(24, 4, 1.2), (24, 14, 1.2), (23.5, 25.5, 1.2)]
    d.water(VISTULA)
    d.water(MOTLAWA)
    town = [(3, 7), (22, 7), (22, 25), (8, 25), (3, 20)]
    d.walled(lambda x, y: d.inpoly(x + 0.5, y + 0.5, town))
    d.walled(lambda x, y: x >= 26 and 7 <= y <= 26 and d.get(x, y) == ' ', wall=False)   # the granary island
    d.inner_streets(river=False)
    for pts in ([(3, 16), (28, 16)], [(12, 7), (12, 25)], [(4, 11), (22, 11)], [(4, 21), (22, 21)], [(30, 7), (30, 26)]):
        d.street(pts)              # the first: the Long Lane and the Long Market, and the bridge over to the granaries
    d.rect(13, 15, 21, 17, ':', only=('h', '.'))   # the Long Market
    d.stalls(13, 15, 21, 17, 0.2)
    d.rect(5, 8, 10, 12, 'M')     # St Mary's
    d.rect(7, 13, 11, 14, 'R')    # the town hall
    d.rect(11, 12, 11, 12, 'Q')   # its tower
    d.rect(14, 12, 18, 14, 'A')   # the Artus Court
    d.rect(22, 7, 22, 8, 'K')     # the Crane
    d.rect(4, 18, 5, 19, 'P')     # the Stockturm, the prison tower
    d.rect(14, 18, 18, 21, 'I')   # the Salmon
    d.ring('MRQAKPI')
    d.rect(22, 7, 22, 24, '.', only=(' ',))   # the Long Bridge, the quay along the Motlawa
    d.gates([(3, 16, '1', 'h', [(2, 16)]),            # the Upland gate, west, to the shore
             (21, 16, '2', 'h', [(22, 16)]),          # the Green Gate, onto the bridge
             (17, 24, '3', 'v', [(17, 27)])])         # the Leege gate, south
    d.towers(5)
    d.lanes(8)
    d.connect(avoid=VISTULA + MOTLAWA, gap=2)
    for y in range(d.H):
        if d.get(23, y) == '=' and d.get(24, y) == '~': d.put(23, y, '~')   # no piers to nowhere
    moor = 0
    for y in range(8, 24):
        if moor < 4 and y % 3 == 0 and d.get(23, y) == '~' and d.get(22, y) in ('.', ':'):
            d.put(23, y, 'g'); moor += 1
    d.trees(0.015)
    return d.done()


DANZIG_HEADER = """\
# Danzig in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (danzig); the script is the master copy for now.
# Characters as in paris.map ('g' a mooring post with a boat); the capital letters are landmarks.

style dutchtown
ground 1.0
towerroof cone

legend M church    "St Mary's of Danzig"          brick westtower roof=tile
legend R hall      "the Main Town hall"           nocampanile brick roof=slate
legend Q tower     "the tower of the Danzig town hall"  height=12 top=spire brick
legend A hall      "the Artus Court"              nocampanile roof=slate stone=E2DAC6 floors=1
legend K tower     "the Crane"                    height=7 top=pyramid brick
legend P tower     "the Stockturm"                height=8 top=spire brick
legend I inn       "the Salmon"

gate 1 "the Upland gate"
gate 2 "the Green Gate"
gate 3 "the Leege gate"

map
"""


def sarajevo():
    """Sarajevo in 1632, the greatest town of Bosnia, and an open one, with no walls: the
    Miljacka through it from east to west (joined to the old map's water at both edges), the
    bazaar, the Bascarsija, on its north bank with the mosque and the medrese of Gazi Husrev-beg,
    the Bezistan and the Morica Han; the Latin quarter of the Ragusan merchants, and their bridge;
    the Emperor's mosque across the river; the courtyard of the Jews. 52 x 42 cells (enlarged by
    1.5, about the middle across and about the river down, so that the Miljacka
    stays where the old map's water meets it; world.txt's three roads start beyond the new edges)."""
    d = Draft(52, 42, 1462)
    MILJACKA = [(53.75, 24.6, 1.1), (35.75, 25.4, 1.1), (17.75, 26.1, 1.1), (-1.75, 26.1, 1.1)]
    d.water(MILJACKA)
    d.walled(lambda x, y: 1 <= x <= 50 and 1 <= y <= 37, wall=False)
    d.inner_streets(river=True)
    for pts in ([(1, 22), (51, 22)], [(1, 29), (51, 29)], [(15, 2), (15, 37)], [(24, 16), (24, 30)], [(1, 8), (51, 8)], [(42, 1), (42, 22)]):
        d.street(pts)              # the third and fourth cross the river: the Latin bridge, the Emperor's bridge
    d.rect(25, 11, 31, 16, 'G')   # the Gazi Husrev-beg mosque
    d.rect(25, 1, 31, 6, 'M')     # the Kursumlija medrese
    d.rect(17, 11, 22, 14, 'B')   # the Bezistan
    d.rect(34, 11, 40, 16, ':', only=('h', '.'))   # the Bascarsija
    d.stalls(34, 11, 40, 16, 0.3)
    d.put(37, 13, 'f')
    d.rect(34, 17, 39, 20, 'H')   # the Morica Han
    d.rect(21, 31, 27, 35, 'E')   # the Emperor's mosque
    d.rect(6, 14, 10, 16, 'C')    # the old church
    d.rect(44, 14, 48, 18, 'J')   # the courtyard of the Jews
    d.rect(6, 30, 10, 33, 'K')    # the kadi's court
    d.ring('GMBHECJK')
    d.lanes(8)
    d.connect(avoid=MILJACKA, gap=2)
    for y in range(d.H):
        for x in range(d.W):
            if (y <= 5 or y >= 34) and d.get(x, y) == 'h' and (x * 7 + y * 3) % 5 == 0:
                d.put(x, y, ',')   # the gardens of the mahalas on the hillsides
    d.trees(0.03)
    return d.done()


SARAJEVO_HEADER = """\
# Sarajevo in 1632.  One character is one tile; north is at the top.
# Drawn first by tools/draft_towns.py (sarajevo); the script is the master copy for now.
# Characters as in paris.map; the capital letters are landmarks.

style ottoman
ground 1.2
towerroof cone

legend G mosque    "the Gazi Husrev-beg mosque"   minarets=1 height=4.5 stone=E6DECC
legend M college   "the Kuršumlija medrese"       depth=3 storeys=1 roof=lead stone=D8CCB0
legend B hall      "the Bezistan"                 nocampanile roof=lead stone=D4C8AE
legend H inn       "the Morića Han"
legend E mosque    "the Emperor's mosque"         minarets=1 height=4 stone=E8E2D4
legend C church    "the old church of the Serbs"  roof=tile stone=D8CCB4
legend J hall      "the courtyard of the Jews"    nocampanile roof=tile stone=E2D8C2
legend K hall      "the kadı's court"             nocampanile roof=tile stone=E2D8C2

map
"""

def main():
    """python3 tools/draft_towns.py paris saint_denis london   (any of them)"""
    towns = {"paris": (PARIS_HEADER, paris), "saint_denis": (SD_HEADER, saint_denis), "london": (LONDON_HEADER, london),
             "florence": (FLORENCE_HEADER, florence), "rome": (ROME_HEADER, rome),
             "venice": (venice_header, venice), "amsterdam": (AMSTERDAM_HEADER, amsterdam),
             "haarlem": (HAARLEM_HEADER, haarlem), "leiden": (LEIDEN_HEADER, leiden),
             "padua": (PADUA_HEADER, padua), "pisa": (PISA_HEADER, pisa),
             "constantinople": (constantinople_header, constantinople), "ragusa": (RAGUSA_HEADER, ragusa),
             "seville": (SEVILLE_HEADER, seville), "granada": (GRANADA_HEADER, granada),
             "oxford": (OXFORD_HEADER, oxford), "cambridge": (CAMBRIDGE_HEADER, cambridge),
             "zurich": (ZURICH_HEADER, zurich), "nuremberg": (NUREMBERG_HEADER, nuremberg), "zirndorf": (ZIRNDORF_HEADER, zirndorf), "stockholm": (STOCKHOLM_HEADER, stockholm), "prague": (PRAGUE_HEADER, prague),
             "vienna": (VIENNA_HEADER, vienna), "cracow": (CRACOW_HEADER, cracow), "danzig": (DANZIG_HEADER, danzig),
             "sarajevo": (SARAJEVO_HEADER, sarajevo)}
    wanted = sys.argv[1:]
    if not wanted:
        print(main.__doc__)
        return
    os.makedirs(OUT, exist_ok=True)
    for name in wanted:
        header, fn = towns[name]
        body = fn()
        if callable(header):   # a header that depends on the plan (Venice's ferries)
            header = header()
        with open(os.path.join(OUT, name + ".map"), "w") as f:
            f.write(header)
            f.write("\n".join(body) + "\nend\n")
        print("wrote", name + ".map")


if __name__ == "__main__":
    main()
