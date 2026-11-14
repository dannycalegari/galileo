// under.cc: networks of tunnels under the map (the old quarries under Paris).
#include "world.h"
#include "noise.h"
#include <iostream>
#include <algorithm>

// A maze of galleries two tiles wide between pillars of rock, with a few larger chambers
// whose roof is held up by stone pillars. Entrances: a shaft on the surface near (sx,sy),
// and ladders down from cellars that lie over the network.
void World::buildUnderground(const std::string &name, int z, int X0, int Y0, int X1, int Y1, int sx, int sy) {
    Rng rng(hash32(X0 * 31 + Y0 * 17 + z));
    Level L;
    L.name = name;
    L.z = z;
    L.init(X0, Y0, X1, Y1);
    L.floor = -5.0f;
    L.wallH = 2.6f;
    L.underground = true;
    L.hideBuilding = false;
    L.wallColor = vec3(0.66f, 0.62f, 0.53f);
    for (auto &t : L.tiles) { t.terrain = T_YARD; t.wall = W_WALL; t.feature = F_NONE; }
    const int P = 4, cw = 2;
    int nx = (L.w - 2) / P, ny = (L.h - 2) / P;
    auto carve = [&](int X, int Y) {
        if (!L.inside(X, Y)) return;
        Tile &t = L.at(X, Y);
        t.wall = W_NONE;
        t.terrain = (hash2i(X, Y, 61) % 4 == 0) ? T_FLOOR_STONE : T_YARD;
        t.var = (uint8_t)(hash2i(X, Y, 62) & 0xff);
    };
    auto cellTile = [&](int i, int j, int &X, int &Y) { X = X0 + 1 + i * P; Y = Y0 + 1 + j * P; };
    auto carveCell = [&](int i, int j) {
        int X, Y;
        cellTile(i, j, X, Y);
        for (int a = 0; a < cw; a++)
            for (int b = 0; b < cw; b++) carve(X + a, Y + b);
    };
    auto carveBetween = [&](int i, int j, int i2, int j2) {
        int X, Y, X2, Y2;
        cellTile(i, j, X, Y);
        cellTile(i2, j2, X2, Y2);
        for (int x = std::min(X, X2); x <= std::max(X, X2) + cw - 1; x++)
            for (int y = std::min(Y, Y2); y <= std::max(Y, Y2) + cw - 1; y++) carve(x, y);
    };
    // depth-first maze
    std::vector<uint8_t> seen(nx * ny, 0);
    std::vector<std::pair<int, int>> stack{{nx / 2, ny / 2}};
    seen[(ny / 2) * nx + nx / 2] = 1;
    carveCell(nx / 2, ny / 2);
    const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!stack.empty()) {
        auto [i, j] = stack.back();
        int order[4] = {0, 1, 2, 3};
        for (int k = 3; k > 0; k--) std::swap(order[k], order[rng.range(0, k)]);
        bool moved = false;
        for (int k : order) {
            int i2 = i + d4[k][0], j2 = j + d4[k][1];
            if (i2 < 0 || j2 < 0 || i2 >= nx || j2 >= ny || seen[j2 * nx + i2]) continue;
            seen[j2 * nx + i2] = 1;
            carveCell(i2, j2);
            carveBetween(i, j, i2, j2);
            stack.push_back({i2, j2});
            moved = true;
            break;
        }
        if (!moved) stack.pop_back();
    }
    // extra passages, so that there are loops to get lost in
    for (int k = 0; k < nx * ny / 8; k++) {
        int i = rng.range(0, nx - 2), j = rng.range(0, ny - 2);
        if (rng.chance(0.5f)) carveBetween(i, j, i + 1, j); else carveBetween(i, j, i, j + 1);
    }
    // chambers, their roofs held up by pillars of stacked stone
    for (int k = 0; k < std::max(2, nx * ny / 40); k++) {
        int ci = rng.range(0, nx - 3), cj = rng.range(0, ny - 3), si = rng.range(2, 3), sj = rng.range(2, 3);
        int Xa, Ya, Xb, Yb;
        cellTile(ci, cj, Xa, Ya);
        cellTile(std::min(nx - 1, ci + si), std::min(ny - 1, cj + sj), Xb, Yb);
        for (int x = Xa; x <= Xb + cw - 1; x++)
            for (int y = Ya; y <= Yb + cw - 1; y++) carve(x, y);
        for (int x = Xa + 2; x <= Xb + cw - 3; x += 3)
            for (int y = Ya + 2; y <= Yb + cw - 3; y += 3)
                if (rng.chance(0.8f)) L.at(x, y).feature = F_PILLAR;
        if (rng.chance(0.5f)) L.at(Xa + 1, Yb).feature = F_WELL;   // the quarrymen's wells
    }
    // rubble in some dead ends
    for (int Y = Y0 + 1; Y < Y1; Y++)
        for (int X = X0 + 1; X < X1; X++) {
            Tile &t = L.at(X, Y);
            if (t.wall != W_NONE || t.feature != F_NONE) continue;
            int open = 0;
            for (auto &d : d4)
                if (L.inside(X + d[0], Y + d[1]) && L.at(X + d[0], Y + d[1]).wall == W_NONE) open++;
            if (open == 1 && rng.chance(0.4f)) t.feature = F_BOULDER;
        }
    int idx = addLevel(std::move(L));
    Level &U = levels[idx];

    // the nearest open place below (X,Y), with an open neighbour: (bx,by) for a ladder, (ax,ay) to arrive
    auto openBelow = [&](int X, int Y, int &bx, int &by, int &ax, int &ay) {
        for (int r = 0; r < 60; r++)
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    int x = X + dx, y = Y + dy;
                    if (!U.inside(x, y) || U.at(x, y).wall != W_NONE || U.at(x, y).feature != F_NONE) continue;
                    // the ladder leans on a rock face
                    for (int sd = 0; sd < 4; sd++) {
                        static const int dd[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                        int wx = x + dd[sd][0], wy = y + dd[sd][1], qx = x - dd[sd][0], qy = y - dd[sd][1];
                        if (!U.inside(wx, wy) || U.at(wx, wy).wall == W_NONE) continue;
                        if (!U.inside(qx, qy) || U.at(qx, qy).wall != W_NONE || U.at(qx, qy).feature != F_NONE) continue;
                        bx = x; by = y; ax = qx; ay = qy;
                        U.at(x, y).feature = F_LADDER;
                        U.at(x, y).var = (uint8_t)(sd | (3 << 2));
                        return true;
                    }
                }
        return false;
    };

    // the shaft on the surface
    {
        int tx = sx, ty = sy;
        bool found = false;
        for (int r = 0; r < 40 && !found; r++)
            for (int dy = -r; dy <= r && !found; dy++)
                for (int dx = -r; dx <= r && !found; dx++) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    int x = sx + dx, y = sy + dy;
                    if (!passable(x, y) || tile(x, y).terrain == T_ROAD || !passable(x + 1, y) || !passable(x - 1, y)) continue;
                    tx = x; ty = y;
                    found = true;
                }
        int bx, by, ax, ay;
        if (found && openBelow(tx, ty, bx, by, ax, ay)) {
            tile(tx, ty).feature = F_SHAFT;
            addLink(tx, ty, 0, ax, ay, z, "You climb down the long ladder into the shaft. The quarries are dark and cold.");
            addLink(bx, by, z, tx + 1, ty, 0, "You climb the long ladder up the shaft, into daylight.");
        } else {
            std::cerr << "no place for the shaft into " << name << "\n";
        }
    }

    // ladders down from cellars that lie over the network
    int made = 0;
    for (size_t i = 0; i < levels.size() && made < 3; i++) {
        Level &C = levels[i];
        if (C.z != z + 1 || !C.underground || (int)i == idx) continue;
        if (C.x0 < X0 + 2 || C.y0 < Y0 + 2 || C.x0 + C.w > X1 - 2 || C.y0 + C.h > Y1 - 2) continue;
        if (hash32((uint32_t)i * 7 + 3) % 3 != 0) continue;
        // a free spot in the cellar, with a free tile in front
        bool ok = false;
        int hx = 0, hy = 0, fx = 0, fy = 0;
        for (int Y = C.y0 + 1; Y < C.y0 + C.h - 1 && !ok; Y++)
            for (int X = C.x0 + 1; X < C.x0 + C.w - 1 && !ok; X++) {
                if (C.at(X, Y).wall != W_NONE || C.at(X, Y).feature != F_NONE) continue;
                if (C.at(X + 1, Y).wall != W_NONE || C.at(X + 1, Y).feature != F_NONE) continue;
                if (linkAt(X, Y, C.z) || linkAt(X + 1, Y, C.z)) continue;
                bool nearLadder = false;
                for (int dx = -1; dx <= 2; dx++)
                    for (int dy = -1; dy <= 1; dy++)
                        if (C.inside(X + dx, Y + dy) && C.at(X + dx, Y + dy).feature == F_LADDER) nearLadder = true;
                if (nearLadder) continue;
                hx = X; hy = Y; fx = X + 1; fy = Y;
                ok = true;
            }
        int bx, by, ax, ay;
        if (!ok || !openBelow(hx, hy, bx, by, ax, ay)) continue;
        C.at(hx, hy).feature = F_HATCH;
        C.at(hx, hy).var = 0;
        addLink(hx, hy, C.z, ax, ay, z, "Under a loose board, a ladder goes down into the dark. You climb down into the quarries.");
        addLink(bx, by, z, fx, fy, C.z, "You climb the ladder back up into the cellar.");
        made++;
    }
}
