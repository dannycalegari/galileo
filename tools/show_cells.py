"""show_cells.py TOWN [X0 Y0 X1 Y1]: print a town's draft (tools/draft_towns.py) in cells, before
they are doubled into tiles, with coordinates; for planning changes to the rects."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import draft_towns as dt
town = sys.argv[1]
captured = {}
orig = dt.double
dt.double = lambda g: captured.setdefault('g', [list(r) for r in g]) or orig(g)
getattr(dt, town)()
g = captured['g']
x0, y0, x1, y1 = (int(a) for a in sys.argv[2:6]) if len(sys.argv) >= 6 else (0, 0, len(g[0]) - 1, len(g) - 1)
print('    ' + ''.join(str((x // 10) % 10) for x in range(x0, x1 + 1)))
print('    ' + ''.join(str(x % 10) for x in range(x0, x1 + 1)))
for y in range(y0, y1 + 1):
    print(f'{y:3d} ' + ''.join(g[y][x] if x < len(g[y]) else ' ' for x in range(x0, x1 + 1)))
