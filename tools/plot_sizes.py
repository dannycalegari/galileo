"""plot_sizes.py [TOWN ...]: the landmarks where named people live or work (data/people/*.txt,
"near" and "work landmark" lines) and their size in tiles, from data/cities/*.map, with the ones
below the size rule flagged (plot buildings bigger than life, roomy inside).
Rule of thumb: houses, halls and inns at least 10 x 8 tiles; chapels and churches at least 10 x 6;
palaces and colleges at least 12 x 12 with wings 4 deep (legend depth=). Towers, gates and
statues are left out."""
import glob, os, re, sys

def used_names():
    used = set()
    for f in glob.glob(os.path.join(os.path.dirname(__file__), '..', 'data', 'people', '*.txt')):
        for l in open(f):
            m = re.match(r'\s+(near|work landmark)\s+(.*)', l)
            if m: used.add(m.group(2).strip().lower())
    return used

MIN = {'mansion': (10, 8), 'hall': (10, 8), 'inn': (10, 8), 'halle': (10, 8), 'chapel': (10, 6),
       'church': (10, 6), 'palace': (12, 12), 'college': (12, 12), 'gallery': (10, 6), 'rotunda': (8, 8)}

def main():
    used = used_names()
    towns = sys.argv[1:]
    for m in sorted(glob.glob(os.path.join(os.path.dirname(__file__), '..', 'data', 'cities', '*.map'))):
        town = os.path.basename(m)[:-4]
        if towns and town not in towns: continue
        lines = open(m).read().split('\n')
        leg = {}
        for l in lines:
            mm = re.match(r'legend (\S)\s+(\S+)\s+"([^"]+)"(.*)', l)
            if mm: leg[mm.group(1)] = (mm.group(2), mm.group(3), mm.group(4))
        if 'map' not in lines: continue
        grid = lines[lines.index('map') + 1:]
        if 'end' in grid: grid = grid[:grid.index('end')]
        out = []
        for c, (kind, name, opts) in leg.items():
            pts = [(x, y) for y, r in enumerate(grid) for x, ch in enumerate(r) if ch == c]
            if not pts or kind in ('tower', 'gate', 'obelisk'): continue
            w = max(p[0] for p in pts) - min(p[0] for p in pts) + 1
            h = max(p[1] for p in pts) - min(p[1] for p in pts) + 1
            plot = any(u in name.lower() for u in used)
            a, b = MIN.get(kind, (0, 0))
            small = max(w, h) < a or min(w, h) < b
            dm = re.search(r'depth=(\d+)', opts)
            if kind in ('palace', 'college') and dm and int(dm.group(1)) < 4: small = True
            if plot or small:
                out.append(f"  {'SMALL' if small else '     '} {'plot' if plot else '    '} {kind:9s} {w:3d} x {h:<3d} {name}{'  ' + opts.strip() if opts.strip() else ''}")
        if out:
            print(town)
            print("\n".join(out))

if __name__ == '__main__':
    main()
