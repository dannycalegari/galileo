#!/usr/bin/env python3
"""Plans the roads between the cities, and the post houses along them.

The roads are routed over the old 1000 x 800 map (data/legacy/europe_geo.map, with the
world file's "oldmap" edits) by a shortest-path search that keeps to the land, prefers the
lowlands, climbs the mountains only by the passes, and crosses rivers where they are one cell
wide (a town's road bridges at most 24 tiles of water). Each road runs between real post
stages of 1632, given below by longitude and latitude and put on the old map by a fit to the
towns of the game (the old map is drawn freely, so the fit is affine plus a correction that
follows the towns nearby).

Writes:
  data/world.txt                the block between the two "roads between the cities" markers
  data/people/posthouses.txt    the people of the post houses (an innkeeper, an ostler who
                                knows the road), generated: edit this script, not the file
The script is the master copy of both. Run from anywhere:  python3 tools/plan_roads.py
  --show        also write a picture of the network (scratch file named on the command line)
"""
import heapq, math, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
DATA = os.path.join(ROOT, "data")
S = 10   # tiles per cell of the old map

# ---------------------------------------------------------------- the old map

def load_legacy():
    def read(fn):
        with open(os.path.join(DATA, "legacy", fn)) as f:
            toks = f.read().split()
        w, h = int(toks[0]), int(toks[1])
        return w, h, [int(t) for t in toks[2:2 + w * h]]
    w, h, geo = read("europe_geo.map")
    _, _, ff = read("europe_ff.map")
    # the world file's oldmap blocks (as Legacy::edit)
    lines = open(os.path.join(DATA, "world.txt")).read().split("\n")
    k = 0
    while k < len(lines):
        t = lines[k].split()
        if len(t) >= 3 and t[0] == "oldmap":
            u0, v0 = int(t[1]), int(t[2])
            rows = []
            k += 1
            while k < len(lines) and not lines[k].startswith("end"):
                ln = lines[k]
                a = ln.find("|"); b = ln.find("|", a + 1) if a >= 0 else -1
                if a >= 0 and b >= 0: rows.append(ln[a + 1:b])
                k += 1
            for r, row in enumerate(rows):
                for c, ch in enumerate(row):
                    i, j = u0 + c, v0 - r
                    if not (0 <= i < w and 0 <= j < h): continue
                    g = {'~': 0, '.': 1, 't': 1, '-': 2, 'T': 2, '=': 3}.get(ch)
                    if g is None: continue
                    geo[i * h + j] = g
                    if ch in 'tT': ff[i * h + j] = 0
                    elif ff[i * h + j] in (0, 1): ff[i * h + j] = -1
        k += 1
    return w, h, geo, ff


class Map:
    def __init__(self):
        self.w, self.h, self.geo, self.ff = load_legacy()
        w, h = self.w, self.h
        # water density over 5 x 5 (as Legacy::derive): rivers are thin, seas and lakes dense
        self.wd = [0.0] * (w * h)
        for i in range(w):
            for j in range(h):
                n = 0
                for di in range(-2, 3):
                    for dj in range(-2, 3):
                        if self.water(i + di, j + dj): n += 1
                self.wd[i * h + j] = n / 25.0

    def g(self, i, j):
        if 0 <= i < self.w and 0 <= j < self.h: return self.geo[i * self.h + j]
        return -1

    def water(self, i, j): return self.g(i, j) <= 0

    def forest(self, i, j):
        if 0 <= i < self.w and 0 <= j < self.h: return self.ff[i * self.h + j] in (0, 1)
        return False

    def dens(self, i, j):
        if 0 <= i < self.w and 0 <= j < self.h: return self.wd[i * self.h + j]
        return 1.0

# ---------------------------------------------------------------- the towns

def game_stats():
    """settlement rectangles and road heads, from the game's own --stats"""
    exe = os.path.join(ROOT, "galileo")
    out = subprocess.run([exe, "--stats"], cwd=ROOT, capture_output=True, text=True).stdout
    rects, heads = {}, {}
    for ln in out.split("\n"):
        m = re.match(r"\s+settlement (.*)\|(-?\d+) (-?\d+) (\d+) (\d+)\|(\w+)", ln)
        if m:
            rects[m.group(1)] = (int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)), m.group(6))
        m = re.match(r"\s+road head (\S+) at \(([-\d.]+), ([-\d.]+)\)", ln)
        if m: heads[m.group(1)] = (float(m.group(2)) / S, float(m.group(3)) / S)
    return rects, heads


def town_files():
    out = {}
    for ln in open(os.path.join(DATA, "world.txt")):
        t = ln.split()
        if len(t) >= 5 and t[0] == "town": out[t[1].replace("_", " ")] = t[2]
    return out


def border_roads(name, file, rect):
    """the road cells ('_') of a town map that touch its edge: old-map points just outside"""
    rows, inmap = [], False
    for ln in open(os.path.join(DATA, file)):
        ln = ln.rstrip("\n")
        if ln.strip() == "map" and not inmap: inmap = True; continue
        if inmap:
            if ln.startswith("end"): break
            rows.append(ln)
    x0, y0, W, H, _ = rect
    pts = []
    for r, row in enumerate(rows):
        row = row.ljust(W)
        for c, ch in enumerate(row[:W]):
            if ch != '_': continue
            side = None
            if r == 0: side = 'N'
            elif r == H - 1: side = 'S'
            elif c == 0: side = 'W'
            elif c == W - 1: side = 'E'
            if side is None: continue
            X, Y = x0 + c + 0.5, y0 + H - 1 - r + 0.5
            pts.append((side, X / S, Y / S))
    # one point per run of road cells on each side
    out = []
    for side in 'NSEW':
        ps = sorted([p for p in pts if p[0] == side], key=lambda p: (p[1], p[2]))
        run = []
        for p in ps:
            if run and abs(p[1] - run[-1][1]) + abs(p[2] - run[-1][2]) > 0.15:
                out.append(run); run = []
            run.append(p)
        if run: out.append(run)
    res = []
    for run in out:
        side = run[0][0]
        x = sum(p[1] for p in run) / len(run); y = sum(p[2] for p in run) / len(run)
        dx, dy = {'N': (0, 1), 'S': (0, -1), 'E': (1, 0), 'W': (-1, 0)}[side]
        res.append((side, round(x + dx * 0.15, 2), round(y + dy * 0.15, 2), len(run)))
    return res

# ---------------------------------------------------------------- real places onto the old map

FIT = [  # town: longitude, latitude, old-map x, y (world.txt)
    ("Paris", 2.35, 48.86, 296, 383), ("London", -0.13, 51.51, 279.5, 441), ("Oxford", -1.26, 51.75, 252, 453.5),
    ("Cambridge", 0.12, 52.21, 282, 461), ("Amsterdam", 4.90, 52.37, 342, 441), ("Haarlem", 4.64, 52.38, 325, 442.75),
    ("Leiden", 4.49, 52.16, 324.5, 430), ("Florence", 11.26, 43.77, 395, 266), ("Pisa", 10.40, 43.72, 378, 265.5),
    ("Rome", 12.48, 41.90, 414, 238), ("Venice", 12.33, 45.44, 412, 297), ("Padua", 11.88, 45.41, 390, 301),
    ("Constantinople", 28.98, 41.01, 645, 217), ("Ragusa", 18.09, 42.65, 492.3, 238), ("Seville", -5.98, 37.39, 131, 211),
    ("Granada", -3.60, 37.18, 147.5, 196), ("Zurich", 8.54, 47.37, 356, 346), ("Nuremberg", 11.08, 49.45, 406, 389.5),
    ("Prague", 14.42, 50.08, 453.5, 408), ("Vienna", 16.37, 48.21, 503.5, 374.5), ("Cracow", 19.94, 50.06, 563, 404.5),
    ("Danzig", 18.65, 54.35, 532.5, 474.5), ("Sarajevo", 18.41, 43.86, 519.5, 279.5), ("Stockholm", 18.07, 59.33, 496.5, 559.5),
]


def solve3(A, b):
    # Gaussian elimination, 3 x 3
    M = [A[r][:] + [b[r]] for r in range(3)]
    for c in range(3):
        p = max(range(c, 3), key=lambda r: abs(M[r][c]))
        M[c], M[p] = M[p], M[c]
        for r in range(3):
            if r != c:
                f = M[r][c] / M[c][c]
                for k in range(c, 4): M[r][k] -= f * M[c][k]
    return [M[r][3] / M[r][r] for r in range(3)]


class Projection:
    def __init__(self):
        AtA = [[0.0] * 3 for _ in range(3)]
        bx, by = [0.0] * 3, [0.0] * 3
        for _, lo, la, x, y in FIT:
            v = (lo, la, 1.0)
            for r in range(3):
                for c in range(3): AtA[r][c] += v[r] * v[c]
                bx[r] += v[r] * x; by[r] += v[r] * y
        self.cx, self.cy = solve3(AtA, bx), solve3(AtA, by)
        self.res = []
        for nm, lo, la, x, y in FIT:
            ax, ay = self.affine(lo, la)
            self.res.append((lo, la, x - ax, y - ay))

    def affine(self, lo, la):
        return (self.cx[0] * lo + self.cx[1] * la + self.cx[2], self.cy[0] * lo + self.cy[1] * la + self.cy[2])

    def __call__(self, lo, la):
        ax, ay = self.affine(lo, la)
        sw = sx = sy = 0.0
        for rlo, rla, rx, ry in self.res:
            d2 = ((lo - rlo) * math.cos(math.radians(la))) ** 2 + (la - rla) ** 2
            wgt = 1.0 / (d2 + 0.3) ** 1.5
            sw += wgt; sx += wgt * rx; sy += wgt * ry
        return ax + sx / sw, ay + sy / sw


# ---------------------------------------------------------------- routing

LAND_COST = {1: 1.0, 2: 1.15, 3: 1.7, 4: 4.0, 5: 9.0, 6: 25.0}


class Router:
    def __init__(self, M, rects):
        self.M = M
        # cells under a settlement (and a cell round it) are kept clear, except the ends' own
        self.block = {}
        for nm, (x0, y0, W, H, kind) in rects.items():
            for i in range(int(x0 / S) - 1, int((x0 + W) / S) + 2):
                for j in range(int(y0 / S) - 1, int((y0 + H) / S) + 2):
                    self.block[(i, j)] = nm

    def cell_cost(self, i, j):
        """None: never (the sea, a lake); else the cost of entering the cell"""
        M = self.M
        g = M.g(i, j)
        if g <= 0:
            if M.dens(i, j) > 0.56: return None
            return 30.0   # a river: crossed only where it must be
        c = LAND_COST.get(g, 25.0)
        if M.forest(i, j): c += 0.25
        near = 0
        for di in (-1, 0, 1):
            for dj in (-1, 0, 1):
                if (di or dj) and M.water(i + di, j + dj): near += 1
        if near: c += 1.5 + 0.3 * near   # keep off the banks and the shore
        return c

    def route(self, a, b, allow=()):
        """cells from old-map point a to b (a list of (i, j))"""
        M = self.M
        si, sj = int(math.floor(a[0])), int(math.floor(a[1]))
        gi, gj = int(math.floor(b[0])), int(math.floor(b[1]))
        def ok_block(i, j):
            nm = self.block.get((i, j))
            return nm is None or nm in allow
        start, goal = (si, sj, 0), (gi, gj)
        dist = {start: 0.0}
        prev = {}
        pq = [(math.hypot(gi - si, gj - sj), 0.0, start)]
        cache = {}
        def cost(i, j):
            k = (i, j)
            if k not in cache: cache[k] = self.cell_cost(i, j)
            return cache[k]
        found = None
        while pq:
            f, d, st = heapq.heappop(pq)
            if d > dist.get(st, 1e18): continue
            i, j, wr = st
            if (i, j) == goal: found = st; break
            for di in (-1, 0, 1):
                for dj in (-1, 0, 1):
                    if not (di or dj): continue
                    ni, nj = i + di, j + dj
                    if not (0 <= ni < M.w and 0 <= nj < M.h): continue
                    c = cost(ni, nj)
                    if c is None: continue
                    if (ni, nj) != goal and (ni, nj) != (si, sj) and not ok_block(ni, nj): c += 40.0
                    wet = M.water(ni, nj)
                    if di and dj:
                        # never cut a corner across water: rivers are crossed square
                        if wet or M.water(i + di, j) or M.water(i, j + dj) or M.water(i, j): continue
                        c *= 1.4142
                    nwr = wr + 1 if wet else 0
                    if nwr > 2: continue   # two cells of river at most (a bridge of 20 tiles)
                    if nwr == 2: c += 60.0
                    ns = (ni, nj, nwr)
                    nd = d + c
                    if nd < dist.get(ns, 1e18):
                        dist[ns] = nd
                        prev[ns] = st
                        heapq.heappush(pq, (nd + math.hypot(gi - ni, gj - nj), nd, ns))
        if not found: return None
        path = [found]
        while path[-1] in prev: path.append(prev[path[-1]])
        return [(p[0], p[1]) for p in reversed(path)]


def segment_cells(a, b):
    n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 0.1))
    out = []
    for k in range(n + 1):
        t = k / n
        c = (int(math.floor(a[0] + (b[0] - a[0]) * t)), int(math.floor(a[1] + (b[1] - a[1]) * t)))
        if not out or out[-1] != c: out.append(c)
    return out


def simplify(M, R, pts):
    """Ramer-Douglas-Peucker on the path's points, keeping the land on both sides of every
    river crossing, and checking that no straight piece crosses more water than the path did"""
    keep = set([0, len(pts) - 1])
    for k in range(len(pts)):
        if M.water(int(math.floor(pts[k][0])), int(math.floor(pts[k][1]))):
            keep.update([max(0, k - 1), min(len(pts) - 1, k + 1)])
    def wet_count(cells): return sum(1 for c in cells if M.water(*c))
    def fine(a, b):
        cells = segment_cells(pts[a], pts[b])
        for c in cells:
            if R.cell_cost(*c) is None: return False
        orig = set()
        for k in range(a, b + 1): orig.add((int(math.floor(pts[k][0])), int(math.floor(pts[k][1]))))
        return wet_count(cells) <= sum(1 for c in orig if M.water(*c))
    def rdp(a, b, tol=0.7):
        if b - a < 2: return
        ax, ay = pts[a]; bx, by = pts[b]
        L = math.hypot(bx - ax, by - ay) or 1e-9
        worst, wk = -1, -1
        for k in range(a + 1, b):
            px, py = pts[k]
            d = abs((bx - ax) * (ay - py) - (ax - px) * (by - ay)) / L
            if k in keep: d = 1e9
            if d > worst: worst, wk = d, k
        if worst > tol or not fine(a, b) or L > 12:
            keep.add(wk); rdp(a, wk, tol); rdp(wk, b, tol)
    rdp(0, len(pts) - 1)
    return [pts[k] for k in sorted(keep)]


def path_length(pts):
    return sum(math.hypot(pts[k + 1][0] - pts[k][0], pts[k + 1][1] - pts[k][1]) for k in range(len(pts) - 1))

# ---------------------------------------------------------------- the network of 1632

# Post stages: name, longitude, latitude, country (as the people's files: "France" unless
# given), the name pool of its people. Each is a post house on the road (in Spain a venta, in
# the Ottoman lands a han, a caravanserai), unless its name starts with "~" (a point the road
# goes by, a pass, with no house).
PLACES = {
    # France
    "Senlis": (2.59, 49.21, "France", "french"),
    "Cambrai": (3.24, 50.18, "Netherlands", "french"),
    "Brussels": (4.35, 50.85, "Netherlands", "dutch"),
    "Antwerp": (4.40, 51.22, "Netherlands", "dutch"),
    "Dordrecht": (4.69, 51.81, "Netherlands", "dutch"),
    "Troyes": (4.08, 48.30, "France", "french"),
    "Langres": (5.33, 47.86, "France", "french"),
    "Basel": (7.59, 47.56, "Switzerland", "german"),
    "Orleans": (1.91, 47.90, "France", "french"),
    "Tours": (0.69, 47.39, "France", "french"),
    "Poitiers": (0.34, 46.58, "France", "french"),
    "Angouleme": (0.16, 45.65, "France", "french"),
    "Bordeaux": (-0.58, 44.84, "France", "french"),
    "Bayonne": (-1.47, 43.49, "France", "french"),
    "Calais": (1.86, 50.95, "France", "french"),
    # Spain
    "Vitoria": (-2.67, 42.85, "Spain", "spanish"),
    "Burgos": (-3.70, 42.34, "Spain", "spanish"),
    "~Somosierra": (-3.58, 41.13, "Spain", "spanish"),
    "Madrid": (-3.70, 40.42, "Spain", "spanish"),
    "Manzanares": (-3.37, 39.00, "Spain", "spanish"),
    "Cardenas": (-3.52, 38.38, "Spain", "spanish"),
    "Cordoba": (-4.78, 37.88, "Spain", "spanish"),
    "Ecija": (-5.08, 37.54, "Spain", "spanish"),
    # the Empire, Switzerland
    "Schaffhausen": (8.63, 47.70, "Switzerland", "german"),
    "Ulm": (9.99, 48.40, "Empire", "german"),
    "Augsburg": (10.90, 48.37, "Empire", "german"),
    "Donauworth": (10.78, 48.72, "Empire", "german"),
    "Mittenwald": (11.26, 47.44, "Empire", "german"),
    "Innsbruck": (11.40, 47.27, "Empire", "german"),
    "~Brenner": (11.51, 47.00, "Empire", "german"),
    "Bozen": (11.35, 46.50, "Empire", "german"),
    "Trent": (11.12, 46.07, "Italy", "italian"),
    "Verona": (10.99, 45.44, "Italy", "venetian"),
    "Vicenza": (11.55, 45.55, "Italy", "venetian"),
    "Bologna": (11.34, 44.49, "Italy", "italian"),
    "Ferrara": (11.62, 44.84, "Italy", "italian"),
    "Amberg": (11.86, 49.44, "Empire", "german"),
    "Pilsen": (13.38, 49.75, "Bohemia", "czech"),
    "Iglau": (15.59, 49.40, "Bohemia", "czech"),
    "Znaim": (16.05, 48.86, "Bohemia", "czech"),
    "Brno": (16.61, 49.20, "Bohemia", "czech"),
    "Olomouc": (17.25, 49.59, "Bohemia", "czech"),
    "Teschen": (18.63, 49.75, "Bohemia", "czech"),
    "Aachen": (6.08, 50.78, "Empire", "german"),
    "Cologne": (6.96, 50.94, "Empire", "german"),
    "Koblenz": (7.59, 50.36, "Empire", "german"),
    "Frankfurt": (8.68, 50.11, "Empire", "german"),
    "Wurzburg": (9.93, 49.79, "Empire", "german"),
    # Poland
    "Radom": (21.15, 51.40, "Poland", "polish"),
    "Warsaw": (21.01, 52.23, "Poland", "polish"),
    "Plock": (19.70, 52.55, "Poland", "polish"),
    "Torun": (18.60, 53.01, "Poland", "polish"),
    "Marienburg": (19.03, 54.04, "Poland", "polish"),
    # Hungary and the Ottoman lands
    "Pressburg": (17.11, 48.14, "Empire", "german"),
    "Raab": (17.63, 47.69, "Empire", "german"),
    "Buda": (19.04, 47.50, "Ottoman", "ottoman"),
    "Szeged": (20.14, 46.25, "Ottoman", "ottoman"),
    "Belgrade": (20.46, 44.82, "Ottoman", "ottoman"),
    "Jagodina": (21.26, 43.98, "Ottoman", "ottoman"),
    "Nis": (21.90, 43.32, "Ottoman", "ottoman"),
    "Sofia": (23.32, 42.70, "Ottoman", "ottoman"),
    "Plovdiv": (24.75, 42.14, "Ottoman", "ottoman"),
    "Edirne": (26.56, 41.68, "Ottoman", "ottoman"),
    "Corlu": (27.80, 41.16, "Ottoman", "ottoman"),
    "Visegrad": (19.29, 43.78, "Ottoman", "bosnian"),
    "Novi Pazar": (20.51, 43.14, "Ottoman", "bosnian"),
    # England
    "Bedford": (-0.47, 52.14, "England", "english"),
    "Canterbury": (1.08, 51.28, "England", "english"),
    "Dover": (1.31, 51.13, "England", "english"),
}

# The names people use for the places (with their accents), and what they call the house
SHOWN = {"Orleans": "Orléans", "Angouleme": "Angoulême", "Cardenas": "Cárdenas", "Cordoba": "Córdoba",
         "Ecija": "Écija", "Donauworth": "Donauwörth", "Wurzburg": "Würzburg", "Plock": "Płock",
         "Torun": "Toruń", "Nis": "Niš", "Corlu": "Çorlu", "Visegrad": "Višegrad", "Trent": "Trento"}


def house_name(place):
    country = PLACES[place][2]
    shown = SHOWN.get(place, place)
    if country == "Spain": return "the venta at " + shown
    if country == "Ottoman": return "the han at " + shown
    return "the post house at " + shown


# The roads: a list of points, each "@X,Y" (an old-map point: the end of a road that leaves a
# town already), "TOWN.N" (gate N of a town), or a place of PLACES. Every place on a road gets
# its post house once, however many roads meet there.
ROADS = [
    ("Paris to Brussels and Leiden", ["@301,412=Paris", "Cambrai", "Brussels", "Antwerp", "Dordrecht", "Leiden.4"]),
    ("Paris to Basel and Zurich", ["@330,366=Paris", "+Langres", "Basel", "@344,348=Zurich"]),
    ("Paris to Madrid and Seville", ["Paris.6", "Orleans", "Tours", "Poitiers", "Angouleme", "Bordeaux", "Bayonne",
                                     "Vitoria", "Burgos", "Madrid", "Manzanares", "Cardenas",
                                     "Cordoba", "@130,222=Seville"]),
    ("Zurich to Ulm, Augsburg and Nuremberg", ["@361,354=Zurich", "Ulm", "Augsburg", "Donauworth", "@406,381=Nuremberg"]),
    ("Augsburg over the Brenner to Verona and Padua", ["Augsburg", "Mittenwald", "Innsbruck", "~Brenner", "Bozen",
                                                       "Trent", "@389.5,312=Padua"]),
    ("Nuremberg to Pilsen and Prague", ["@416,391=Nuremberg", "+Amberg", "Pilsen", "@465,409.5=Prague"]),
    ("Prague to Iglau and Vienna", ["@465,409.5=Prague", "Iglau", "+Znaim", "Vienna.3"]),
    ("Vienna to Brno and Cracow", ["@512,377=Vienna", "+Brno", "Olomouc", "Teschen", "Cracow.1"]),
    ("Cracow to Warsaw and Danzig", ["@564,414=Cracow", "Radom", "Warsaw", "Plock", "Torun", "+Marienburg", "@533.5,466=Danzig"]),
    ("Vienna to Buda, Belgrade and Constantinople", ["@513,373.5=Vienna", "Raab", "Buda", "Szeged", "Belgrade",
                                                     "Jagodina", "Nis", "Sofia", "Plovdiv", "Edirne", "+Corlu", "@618,219=Constantinople"]),
    ("Sarajevo to Novi Pazar and Nis", ["@527,281=Sarajevo", "+Visegrad", "+Novi Pazar", "Nis"]),
    ("Brussels to Aachen, Cologne, Frankfurt and Nuremberg", ["Brussels", "Aachen", "+Cologne", "Koblenz", "Frankfurt",
                                                              "Wurzburg", "@400,396=Nuremberg"]),
    ("Oxford to Bedford and Cambridge", ["@253,462=Oxford", "Bedford", "@276.2,461=Cambridge"]),
    ("Florence to Bologna, Ferrara and Padua", ["Florence.2", "Bologna", "Ferrara", "Padua.3"]),
    # short roads out of the towns, routed again because the old ones stopped at a river
    ("Padua along the Brenta to Fusina", ["Padua.2", "Venice.1"]),
    ("Paris towards Chartres", ["Paris.7", "@284,350"]),
]

# world.txt road lines that this plan replaces (they stopped short at water)
REPLACED = ["road 3 Paris.6 297,370 296,358 294,340", "road 3 Florence.2 395,279 396,290 Padua.3",
            "road 3 Venice.1 397.5,297.4 Padua.2", "road 3 Paris.7 290,370 287,360 284,348",
            "road 3 Nuremberg.3 400.5,388.1 396,386"]

# ---------------------------------------------------------------- building the plan

HOUSE_W, HOUSE_H = 32, 22   # the post house template, in tiles (data/cities/hamlets/posthouse.map)
TEMPLATE = {"Spain": "venta", "Ottoman": "han"}
STYLE = {"France": "village", "England": "village", "Netherlands": "dutchtown", "Empire": "german",
         "Switzerland": "german", "Bohemia": "german", "Poland": "german", "Italy": "padua"}
INN_NAMES = {
    "France": ["the Lion d'Or", "the Écu de France", "the Cheval Blanc", "the Croix Blanche", "the Grand Cerf",
               "the Trois Rois", "the Soleil d'Or", "the Cheval Rouge", "the Image Notre-Dame"],
    "England": ["the Swan", "the Red Lion", "the Ship", "the George", "the Bell"],
    "Netherlands": ["the Golden Fleece", "the Swan", "the Moor's Head", "the Golden Hart", "the Arms of Brabant"],
    "Empire": ["the Golden Eagle", "the Black Bear", "the Golden Lamb", "the Red Ox", "the Swan", "the Golden Stag",
               "the Three Moors", "the White Horse", "the Golden Crown"],
    "Switzerland": ["the Stork", "the Wild Man", "the Golden Ox", "the Three Kings"],
    "Bohemia": ["the Golden Wheel", "the Black Horse", "the White Swan", "the Golden Pike"],
    "Poland": ["the White Eagle", "the Golden Duck", "the Black Horse", "the Lamb"],
    "Italy": ["the Sun", "the Three Kings", "the Moor", "the Pilgrim", "the Tower", "the Angel"],
}


def stable_hash(t):
    h = 2166136261
    for ch in t.encode("utf-8"):
        h = ((h ^ ch) * 16777619) & 0xffffffff
    return h


def load_names():
    pools = {}
    for ln in open(os.path.join(DATA, "people", "names.txt")):
        m = re.match(r"(\w+) (male|female|surname): (.*)", ln)
        if m: pools.setdefault(m.group(1), {})[m.group(2)] = m.group(3).split()
    return pools


def compass(dx, dy):
    a = math.degrees(math.atan2(dy, dx)) % 360
    names = ["east", "north-east", "north", "north-west", "west", "south-west", "south", "south-east"]
    return names[int((a + 22.5) // 45) % 8]


def resolve(pt, heads, P, M, R):
    """an old-map point for a road's point spec; second: the text for world.txt (None: a place)"""
    if pt.startswith("@"):
        x, y = pt[1:].split("=")[0].split(",")
        return (float(x), float(y)), pt[1:].split("=")[0]
    if re.match(r".+\.\d$", pt) and pt in heads:
        return heads[pt], pt
    lo, la = PLACES[pt][0], PLACES[pt][1]
    x, y = P(lo, la)
    # onto good ground near there: dry lowland, off the banks
    best, bd = None, 1e9
    i0, j0 = int(math.floor(x)), int(math.floor(y))
    for di in range(-5, 6):
        for dj in range(-5, 6):
            i, j = i0 + di, j0 + dj
            c = R.cell_cost(i, j)
            if c is None or c > 2.0 or (i, j) in R.block: continue
            d = math.hypot(di, dj)
            if d < bd: best, bd = (i + 0.5, j + 0.5), d
    if best is None: best = (x, y)
    return best, None


def house_spot(M, R, pt, before, after, roadpts):
    """where a post house stands beside the road at pt: the centre of its rectangle"""
    dx, dy = after[0] - before[0], after[1] - before[1]
    L = math.hypot(dx, dy) or 1.0
    nx, ny = -dy / L, dx / L
    hw, hh = HOUSE_W / 2 / S, HOUSE_H / 2 / S
    near = [q for q in roadpts if math.hypot(q[0] - pt[0], q[1] - pt[1]) < 8]
    for off in [x * 0.25 for x in range(5, 20)]:
        for sgn in (1, -1):
            cx, cy = pt[0] + sgn * nx * off, pt[1] + sgn * ny * off
            x0, x1, y0, y1 = cx - hw - 0.4, cx + hw + 0.4, cy - hh - 0.4, cy + hh + 0.4
            good = True
            for i in range(int(math.floor(x0)), int(math.floor(x1)) + 1):
                for j in range(int(math.floor(y0)), int(math.floor(y1)) + 1):
                    if M.water(i, j) or M.g(i, j) > 5 or (i, j) in R.block: good = False
            if not good: continue
            # the road (sampled) must not run through the rectangle
            for k in range(len(near) - 1):
                a, b = near[k], near[k + 1]
                n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 0.05))
                for t in range(n + 1):
                    qx = a[0] + (b[0] - a[0]) * t / n; qy = a[1] + (b[1] - a[1]) * t / n
                    if x0 - 0.3 < qx < x1 + 0.3 and y0 - 0.3 < qy < y1 + 0.3: good = False; break
                if not good: break
            if not good: continue
            # and stand near it: the yard's edge within a cell of the road
            gap = 1e9
            for k in range(len(near) - 1):
                a, b = near[k], near[k + 1]
                n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 0.1))
                for t in range(n + 1):
                    qx = a[0] + (b[0] - a[0]) * t / n; qy = a[1] + (b[1] - a[1]) * t / n
                    dx = max(cx - hw - qx, 0, qx - cx - hw); dy = max(cy - hh - qy, 0, qy - cy - hh)
                    gap = min(gap, math.hypot(dx, dy))
            if gap <= 1.0: return (round(cx, 2), round(cy, 2))
    return None


def plan():
    M = Map()
    rects, heads = game_stats()
    rects = {k: v for k, v in rects.items() if not k.startswith(("the post house at", "the venta at", "the han at"))}
    P = Projection()
    R = Router(M, rects)
    houses = {}      # place -> (x, y) the road point
    onroad = {}      # (road, place) -> the road point nearest a name-only place
    lines = []
    report = []
    for title, spec0 in ROADS:
        pts = []
        spec = [sp for sp in spec0 if not sp.startswith("+")]
        for sp in spec:
            q, text = resolve(sp, heads, P, M, R)
            pts.append((sp, q, text))
        # route leg by leg
        full = []
        allow = set()
        for sp, q, text in pts:
            if text and "." in text and not text.startswith(("@",)) and re.match(r".+\.\d$", text):
                allow.add(text.split(".")[0])
        for k in range(len(pts) - 1):
            a, b = pts[k][1], pts[k + 1][1]
            cells = R.route(a, b, allow)
            if cells is None:
                report.append(f"NO ROUTE: {title}: {pts[k][0]} -> {pts[k+1][0]}")
                continue
            leg = [a] + [(i + 0.5, j + 0.5) for i, j in cells[1:-1]] + [b]
            leg = simplify(M, R, leg)
            straight = math.hypot(b[0] - a[0], b[1] - a[1])
            if path_length(leg) > 1.6 * straight + 3:
                report.append(f"   DETOUR {pts[k][0]} -> {pts[k+1][0]}: {path_length(leg) * S:.0f} tiles for {straight * S:.0f}")
            if full: leg = leg[1:]
            full.extend(leg)
            # the place where the leg starts and ends is a vertex of the road
        # world.txt: the road line, the gate names kept as names
        L = path_length(full)
        report.append(f"{title}: {L * S:.0f} tiles of road, {len(full)} points")
        for k in range(1, len(full) - 1):
            ax, ay = full[k][0] - full[k - 1][0], full[k][1] - full[k - 1][1]
            bx, by = full[k + 1][0] - full[k][0], full[k + 1][1] - full[k][1]
            la, lb = math.hypot(ax, ay), math.hypot(bx, by)
            if la and lb and (ax * bx + ay * by) / (la * lb) < -0.7:
                report.append(f"   SPUR at {full[k][0]:.1f},{full[k][1]:.1f}")
        # name-only places: on the road where it passes nearest them; the order of the road's
        # places for the ostlers is then by where they fall along it
        dense = []
        for k in range(len(full) - 1):
            n = max(1, int(math.hypot(full[k + 1][0] - full[k][0], full[k + 1][1] - full[k][1]) / 0.25))
            for t in range(n): dense.append((full[k][0] + (full[k + 1][0] - full[k][0]) * t / n, full[k][1] + (full[k + 1][1] - full[k][1]) * t / n, k))
        for sp in spec0:
            if not sp.startswith("+"): continue
            nm = sp[1:]
            tx, ty = P(PLACES[nm][0], PLACES[nm][1])
            bx, by, bk = min(dense, key=lambda d: math.hypot(d[0] - tx, d[1] - ty))
            if math.hypot(bx - tx, by - ty) > 8:
                report.append(f"   {nm} is {math.hypot(bx - tx, by - ty):.0f} cells from the road: no post house")
                continue
            # make the point a vertex of the road, so the house finds it
            full.insert(bk + 1, (round(bx, 2), round(by, 2)))
            dense = [(d[0], d[1], d[2] + (1 if d[2] > bk else 0)) for d in dense]
            onroad[(title, nm)] = (bx, by)
        for k, (sp, q, text) in enumerate(pts):
            nm = sp if text is None else None
            if nm and not nm.startswith("~"): houses.setdefault(nm, q)
        for (tt, nm), q in onroad.items():
            if tt == title: houses.setdefault(nm, q)
        def end_name(sp, text):
            if text is None: return sp
            if sp.startswith("@") and "=" in sp: return sp.split("=")[1]
            if re.match(r".+\.\d$", text): return text.split(".")[0]
            return None
        lines.append({"title": title, "full": full, "first": pts[0][2], "last": pts[-1][2],
                      "a": end_name(*pts[0][0:1], pts[0][2]) if False else end_name(pts[0][0], pts[0][2]),
                      "b": end_name(pts[-1][0], pts[-1][2])})
    return M, R, P, rects, heads, houses, lines, report

# ---------------------------------------------------------------- a picture

def world_roads(heads, skip_block=True):
    out, inblock = [], False
    for ln in open(os.path.join(DATA, "world.txt")):
        if ln.startswith(BEGIN): inblock = True
        if ln.startswith(END): inblock = False; continue
        if inblock and skip_block: continue
        t = ln.split()
        if len(t) >= 3 and t[0] == "road":
            pts = []
            for tok in t[2:]:
                if "," in tok:
                    x, y = tok.split(","); pts.append((float(x), float(y)))
                elif tok in heads: pts.append(heads[tok])
            out.append(pts)
    return out


def picture(fn, M, rects, heads, lines, houses_at, x0=110, x1=660, y0=180, y1=480, px=3):
    W, H = (x1 - x0) * px, (y1 - y0) * px
    img = bytearray(W * H * 3)
    def put(X, Y, c):
        if 0 <= X < W and 0 <= Y < H:
            k = (Y * W + X) * 3; img[k:k + 3] = bytes(c)
    shade = {1: (150, 190, 120), 2: (170, 185, 120), 3: (180, 170, 120), 4: (160, 140, 110), 5: (190, 180, 170), 6: (240, 240, 240)}
    for i in range(x0, x1):
        for j in range(y0, y1):
            g = M.g(i, j)
            if g <= 0: c = (40, 70, 140) if M.dens(i, j) > 0.4 else (90, 150, 220)
            else:
                c = shade.get(g, (255, 255, 255))
                if M.forest(i, j): c = (c[0] * 3 // 4, c[1] * 7 // 8, c[2] * 3 // 4)
            for a in range(px):
                for b in range(px): put((i - x0) * px + a, (y1 - 1 - j) * px + b, c)
    def line(pts, c, wdt=1):
        for k in range(len(pts) - 1):
            (ax, ay), (bx, by) = pts[k], pts[k + 1]
            n = max(1, int(math.hypot(bx - ax, by - ay) * px * 2))
            for t in range(n + 1):
                x = ax + (bx - ax) * t / n; y = ay + (by - ay) * t / n
                X, Y = int((x - x0) * px), int((y1 - y) * px)
                for a in range(-wdt // 2, wdt - wdt // 2):
                    for b in range(-wdt // 2, wdt - wdt // 2): put(X + a, Y + b, c)
    for pts in world_roads(heads): line(pts, (120, 70, 30), 2)
    for ln in lines: line(ln['full'], (200, 30, 30), 2)
    for nm, (x, y, W2, H2, kind) in rects.items():
        if kind != "town": continue
        line([(x / S, y / S), ((x + W2) / S, y / S), ((x + W2) / S, (y + H2) / S), (x / S, (y + H2) / S), (x / S, y / S)], (0, 0, 0), 2)
    for (x, y) in houses_at:
        for a in range(-3, 4):
            for b in range(-3, 4): put(int((x - x0) * px) + a, int((y1 - y) * px) + b, (255, 220, 0))
    with open(fn + ".ppm", "wb") as f:
        f.write(b"P6 %d %d 255\n" % (W, H)); f.write(img)
    subprocess.run(["sips", "-s", "format", "png", fn + ".ppm", "--out", fn], capture_output=True)
    os.remove(fn + ".ppm")


BEGIN = "# ---------------------------------------------------------------- roads between the cities"
END = "# ---------------------------------------------------------------- end of the roads between the cities"


# Post houses by hand, where the coast leaves no room for the search: the place, the centre
# of the house (old map), the town at the other end of its road (for the ostler)
BY_HAND = [("Dover", (294.0, 434.6), "London"), ("Calais", (300.6, 421.4), "Saint-Denis")]

# The packet boat between Dover and Calais: its master waits at each post house (the fare in
# deniers, the crossing in game minutes), and your own horse goes in the hold for as much again
PACKET = {
    "Dover": ("Calais", "the master of the Dover packet",
              "The packet sails for Calais on every tide, when the wind serves: five sous a passage, and as much again for a horse. Two hours and a half, if God is kind. Aboard, then."),
    "Calais": ("Dover", "the master of the Calais packet",
               "The packet sails for Dover on every tide: five sous a passage, and as much again for a horse. The English will want to see your papers; I never do. Aboard, then."),
}
PACKET_FARE, PACKET_MINUTES = 60, 150

# The inns that are named for what they were
INN_FIXED = {"Bedford": "the Swan", "Dover": "the Ship", "Calais": "the Silver Lion"}

# Names for post houses put in where a road went too far without one (the nearest is taken)
FILLERS = {
    "Beroun": (14.07, 49.96, "Bohemia", "czech"), "Oswiecim": (19.23, 50.03, "Poland", "polish"),
    "Brixen": (11.66, 46.71, "Empire", "german"), "Kecskemet": (19.69, 46.91, "Ottoman", "ottoman"),
    "Wesel": (6.62, 51.66, "Empire", "german"), "Rovigo": (11.79, 45.07, "Italy", "venetian"),
    "Aleksinac": (21.70, 43.54, "Ottoman", "ottoman"), "Paracin": (21.41, 43.86, "Ottoman", "ottoman"), "Ostrau": (18.29, 49.84, "Bohemia", "czech"),
    "Kolin": (15.20, 50.03, "Bohemia", "czech"), "Wieliczka": (20.06, 49.98, "Poland", "polish"),
    "Bochnia": (20.43, 49.97, "Poland", "polish"),
    "Ocana": (-3.50, 39.96, "Spain", "spanish"), "Aranda": (-3.69, 41.67, "Spain", "spanish"),
    "Dax": (-1.05, 43.71, "France", "french"), "Chaumont": (5.14, 48.11, "France", "french"),
    "Mulhouse": (7.34, 47.75, "France", "german"), "Memmingen": (10.18, 47.99, "Empire", "german"),
    "Stockach": (9.01, 47.85, "Empire", "german"), "Gorazde": (18.98, 43.67, "Ottoman", "bosnian"),
    "Prijepolje": (19.65, 43.39, "Ottoman", "bosnian"),
    "Petrovaradin": (19.86, 45.25, "Ottoman", "ottoman"), "Duren": (6.48, 50.80, "Empire", "german"),
    "Hanau": (8.92, 50.13, "Empire", "german"), "Bamberg": (10.89, 49.89, "Empire", "german"),
    "Uherske Hradiste": (17.46, 49.07, "Bohemia", "czech"), "Bielsko": (19.04, 49.82, "Poland", "polish"),
    "Sandomierz": (21.75, 50.68, "Poland", "polish"), "Grudziadz": (18.75, 53.48, "Poland", "polish"),
    "Bozen": (11.35, 46.50, "Empire", "german"), "Brno": (16.61, 49.20, "Bohemia", "czech"),
    "Novi Pazar": (20.51, 43.14, "Ottoman", "bosnian"), "Visegrad": (19.29, 43.78, "Ottoman", "bosnian"),
    "Marienburg": (19.03, 54.04, "Poland", "polish"), "Corlu": (27.80, 41.16, "Ottoman", "ottoman"),
    "Langres": (5.33, 47.86, "France", "french"), "Monselice": (11.75, 45.24, "Italy", "venetian"),
    "Mohacs": (18.68, 45.99, "Ottoman", "ottoman"), "Paks": (18.86, 46.62, "Ottoman", "ottoman"),
    "Szekesfehervar": (18.41, 47.19, "Ottoman", "ottoman"), "Maastricht": (5.69, 50.85, "Netherlands", "dutch"),
    "Liege": (5.57, 50.63, "Empire", "french"), "Jaen": (-3.79, 37.77, "Spain", "spanish"),
}
SHOWN.update({"Oswiecim": "Oświęcim", "Kecskemet": "Kecskemét", "Ocana": "Ocaña", "Duren": "Düren",
              "Uherske Hradiste": "Uherské Hradiště", "Grudziadz": "Grudziądz", "Gorazde": "Goražde",
              "Kolin": "Kolín", "Tabor": "Tábor", "Paracin": "Paraćin", "Mohacs": "Mohács", "Szekesfehervar": "Székesfehérvár",
              "Liege": "Liège", "Jaen": "Jaén"})
GAP = 38   # cells of road (380 tiles) without a post house: one more is put in


def arc_points(full):
    acc, out = 0.0, [0.0]
    for k in range(len(full) - 1):
        acc += math.hypot(full[k + 1][0] - full[k][0], full[k + 1][1] - full[k][1]); out.append(acc)
    return out


def point_at(full, arcs, t):
    """the point at arc length t, and the index of the segment it is on"""
    for k in range(len(full) - 1):
        if arcs[k + 1] >= t:
            f = (t - arcs[k]) / max(1e-9, arcs[k + 1] - arcs[k])
            return (full[k][0] + (full[k + 1][0] - full[k][0]) * f, full[k][1] + (full[k + 1][1] - full[k][1]) * f), k
    return full[-1], len(full) - 2


def arc_of(full, arcs, q):
    best, bt = 1e9, 0.0
    for k in range(len(full) - 1):
        ax, ay = full[k]; bx, by = full[k + 1]
        L2 = (bx - ax) ** 2 + (by - ay) ** 2 or 1e-9
        f = max(0.0, min(1.0, ((q[0] - ax) * (bx - ax) + (q[1] - ay) * (by - ay)) / L2))
        d = math.hypot(ax + (bx - ax) * f - q[0], ay + (by - ay) * f - q[1])
        if d < best: best, bt = d, arcs[k] + f * math.sqrt(L2)
    return bt, best


def spot_near(M, R, line, t, roadpts):
    """a post house beside road `line` near arc length t: (centre, road point, arc), or None"""
    full = line["full"]
    arcs = arc_points(full)
    for dt in (0, 0.5, -0.5, 1, -1, 1.5, -1.5, 2, -2, 3, -3, 4, -4, 5, -5):
        tt = t + dt
        if tt <= 1 or tt >= arcs[-1] - 1: continue
        pt, k = point_at(full, arcs, tt)
        before, _ = point_at(full, arcs, max(0, tt - 0.6))
        after, _ = point_at(full, arcs, min(arcs[-1], tt + 0.6))
        spot = house_spot(M, R, pt, before, after, roadpts)
        if spot: return spot, pt, tt
    return None


def insert_vertex(line, pt):
    full = line["full"]
    arcs = arc_points(full)
    t, d = arc_of(full, arcs, pt)
    for q in full:
        if abs(q[0] - pt[0]) < 1e-6 and abs(q[1] - pt[1]) < 1e-6: return
    _, k = point_at(full, arcs, t)
    full.insert(k + 1, (round(pt[0], 3), round(pt[1], 3)))


def place_houses(M, R, P, houses, lines, report):
    """the post houses: place -> (centre of the hamlet, the road point it stands by)"""
    spots = {}
    roadpts = [r for ln in lines for r in ln["full"]]
    for nm, q in houses.items():
        for ln in lines:
            arcs = arc_points(ln["full"])
            t, d = arc_of(ln["full"], arcs, q)
            if d > 1e-3: continue
            got = spot_near(M, R, ln, t, roadpts)
            if got: spots[nm] = (got[0], got[1]); insert_vertex(ln, got[1])
            else: report.append(f"   no room for the post house at {nm}: left out")
            break
    for nm, c, town in BY_HAND: spots[nm] = (c, c)
    # long stretches without a house: one more, named after the nearest stage of FILLERS
    for _ in range(3):
        added = False
        for ln in lines:
            full = ln["full"]
            arcs = arc_points(full)
            marks = sorted(t for nm, (c, pt) in spots.items() for t, d in [arc_of(full, arcs, pt)] if d < 1e-3)
            stops = [0.0] + marks + [arcs[-1]]
            for k in range(len(stops) - 1):
                if stops[k + 1] - stops[k] <= GAP: continue
                got = spot_near(M, R, ln, (stops[k] + stops[k + 1]) / 2, roadpts)
                if not got: report.append(f"   {ln['title']}: a long stretch with no room for a house"); continue
                pt = got[1]
                cand = [(math.hypot(P(v[0], v[1])[0] - pt[0], P(v[0], v[1])[1] - pt[1]), nm) for nm, v in FILLERS.items()
                        if nm not in spots and nm not in PLACES]
                cand.sort()
                if not cand or cand[0][0] > 20:
                    report.append(f"   {ln['title']}: no name for a house at {pt[0]:.1f},{pt[1]:.1f}"); continue
                nm = cand[0][1]
                PLACES[nm] = FILLERS[nm]
                spots[nm] = (got[0], pt); insert_vertex(ln, pt)
                report.append(f"   {ln['title']}: filled with the post house at {nm} ({cand[0][0]:.0f} cells from its place)")
                added = True
                break
        if not added: break
    return spots


def road_text(ln):
    toks = [f"{q[0]:.2f},{q[1]:.2f}" for q in ln["full"]]
    if ln["first"]: toks[0] = ln["first"]
    if ln["last"]: toks[-1] = ln["last"]
    return "road 3 " + " ".join(toks)


def neighbours(spots, lines):
    """for each place or town, the next ones along the roads (the ostlers' directions)"""
    nb = {}
    def link(a, b):
        if not a or not b or a == b: return
        nb.setdefault(a, []); nb.setdefault(b, [])
        if b not in nb[a]: nb[a].append(b)
        if a not in nb[b]: nb[b].append(a)
    for ln in lines:
        full = ln["full"]
        arcs = arc_points(full)
        on = sorted((arc_of(full, arcs, pt)[0], nm) for nm, (c, pt) in spots.items() if arc_of(full, arcs, pt)[1] < 1e-3)
        seq = [ln["a"]] + [nm for _, nm in on] + [ln["b"]]
        for k in range(len(seq) - 1): link(seq[k], seq[k + 1])
    for nm, c, town in BY_HAND: link(nm, town)
    return nb


def write_all(M, R, P, rects, heads, houses, lines, report):
    spots = place_houses(M, R, P, houses, lines, report)
    # ---- world.txt
    wt = os.path.join(DATA, "world.txt")
    old = open(wt).read().split("\n")
    keep, inblock = [], False
    for ln in old:
        if ln.startswith(BEGIN): inblock = True; continue
        if ln.startswith(END): inblock = False; continue
        if inblock: continue
        if ln.strip() in REPLACED: continue
        keep.append(ln)
    while keep and keep[-1] == "": keep.pop()
    blk = [BEGIN,
           "# Written by tools/plan_roads.py, the master copy: edit the script and run it again, not these lines.",
           "# The post roads of 1632 between the cities, with a post house at each stage (in Spain a venta,",
           "# in the Ottoman lands a han): an innkeeper who hires and sells horses, beds and the post, and an",
           "# ostler who knows where the road goes (data/people/posthouses.txt).",
           "#   hamlet NAME posthouse X,Y style=STYLE I=NAME_OF_THE_INN   (the template's style and inn renamed)"]
    for ln in lines:
        blk.append("# " + ln["title"])
        blk.append(road_text(ln))
    blk.append("# the post houses")
    order = sorted(spots, key=lambda nm: (-spots[nm][0][1], spots[nm][0][0]))
    for nm in order:
        (cx, cy), _ = spots[nm]
        country = PLACES[nm][2]
        hn = house_name(nm).replace(" ", "_")
        if country == "Spain": inn, style = "the_venta", "spain"
        elif country == "Ottoman": inn, style = "the_han", "ottoman"
        else:
            names = INN_NAMES[country]
            inn, style = INN_FIXED.get(nm, names[stable_hash(nm) % len(names)]).replace(" ", "_"), STYLE[country]
        blk.append(f"hamlet {hn} posthouse {cx:.2f},{cy:.2f} style={style} I={inn}")
    blk.append(END)
    with open(wt, "w") as f: f.write("\n".join(keep) + "\n\n" + "\n".join(blk) + "\n")
    # ---- the people of the post houses
    pools = load_names()
    pos = {nm: spots[nm][1] for nm in spots}
    for nm, (x, y, W, H, kind) in rects.items():
        if kind == "town": pos[nm] = ((x + W / 2) / S, (y + H / 2) / S)
    nb = neighbours(spots, lines)
    def shown(n): return house_name(n) if n in PLACES else n
    out = ["# The people of the post houses on the roads between the cities: at each, the innkeeper",
           "# (the role: beds, horses to hire and to buy, the post) and an ostler who knows the road.",
           "# Written by tools/plan_roads.py: edit the script, not this file.", ""]
    for nm in order:
        lo, la, country, pool = PLACES[nm]
        pl = pools.get(pool) or pools["german"]
        h = stable_hash("ostler " + nm)
        first = pl["male"][h % len(pl["male"])]
        sur = pl["surname"][(h // 7) % len(pl["surname"])].replace("_", " ")
        here = pos[nm]
        parts = []
        for o in nb.get(nm, []):
            if o not in pos: continue
            d = compass(pos[o][0] - here[0], pos[o][1] - here[1])
            parts.append(f"{d} to {shown(o)}")
        if not parts: way = "The road goes on, both ways."
        elif len(parts) == 1: way = f"This is the end of the road: it goes back {parts[0]}."
        elif len(parts) == 2: way = f"The road runs {parts[0]}, and {parts[1]}."
        else: way = "Roads run from here " + ", ".join(parts[:-1]) + ", and " + parts[-1] + "."
        sid = re.sub(r"[^a-z]", "", nm.lower())
        out += [f"town {house_name(nm)}", f"names {pool}"]
        if country != "France": out.append(f"country {country}")
        out += ["crowd 1 innkeeper", f"person ostler-{sid}", f"  name {first} {sur}", "  role ostler", "  talk",
                f"    road,roads,way,where,next,directions,route: {way}", "  end", "end", ""]
        if nm in PACKET:
            other, title, text = PACKET[nm]
            h2 = stable_hash("packet " + nm)
            first2 = pl["male"][h2 % len(pl["male"])]
            sur2 = pl["surname"][(h2 // 7) % len(pl["surname"])].replace("_", " ")
            out[-1:-1] = []
            out += [f"person packet-{sid}", f"  name {first2} {sur2}", f"  title {title}", "  role sailor", "  work inn",
                    "  says The tide! The tide!", "  talk",
                    f"    job: I am master of the packet boat: the post, and passengers, across to {SHOWN.get(other, other)} and back.",
                    f"    passage,packet,boat,sail,crossing,cross,ship,{other.lower()},{'england' if other == 'Dover' else 'france'}: {text} "
                    f"{{passage {PACKET_FARE} {PACKET_MINUTES} {house_name(other)}}}",
                    "  end", "end", ""]
    with open(os.path.join(DATA, "people", "posthouses.txt"), "w") as f: f.write("\n".join(out))
    tl = os.path.join(DATA, "people", "towns.txt")
    t = open(tl).read()
    if "posthouses.txt" not in t:
        with open(tl, "w") as f: f.write(t.rstrip("\n") + "\nposthouses.txt\n")
    return spots


if __name__ == "__main__" and "--write" in sys.argv:
    M, R, P, rects, heads, houses, lines, report = plan()
    spots = write_all(M, R, P, rects, heads, houses, lines, report)
    for r in report: print(r)
    print(f"{len(lines)} roads, {sum(path_length(ln['full']) for ln in lines) * S:.0f} tiles; {len(spots)} post houses")
    if "--show" in sys.argv:
        fn = sys.argv[sys.argv.index("--show") + 1]
        picture(fn, M, rects, heads, lines, [v[0] for v in spots.values()])
        print("picture:", fn)
    sys.exit(0)

if __name__ == "__main__" and "--survey" in sys.argv:
    rects, heads = game_stats()
    files = town_files()
    for nm, f in files.items():
        if nm not in rects: continue
        x0, y0, W, H, _ = rects[nm]
        print(f"{nm}: cells x {x0/S:.1f}-{(x0+W)/S:.1f}, y {y0/S:.1f}-{(y0+H)/S:.1f}")
        for g, p in sorted(heads.items()):
            if g.split(".")[0] == nm: print(f"   gate {g} at {p[0]:.1f},{p[1]:.1f}")
        for b in border_roads(nm, f, rects[nm]): print(f"   road end {b[0]} at {b[1]},{b[2]} ({b[3]} tiles wide)")
    P = Projection()
    print("fit residuals (old-map cells) after the correction:")
    for nm, lo, la, x, y in FIT:
        px, py = P(lo, la)
        print(f"   {nm:15s} {x:6.1f},{y:6.1f}   affine off by {x - P.affine(lo, la)[0]:6.1f},{y - P.affine(lo, la)[1]:6.1f}")
    sys.exit(0)
