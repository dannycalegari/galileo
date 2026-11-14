// world.cc: generating Europe from the old 1000 x 800 map, and queries on the world.
#include "world.h"
#include "noise.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <deque>
#include <queue>
#include <unordered_map>

// ------------------------------------------------------------------ the old map

// per legacy cell, precomputed: fraction of water and of trees in a 5x5 neighbourhood
static std::vector<float> g_waterDens, g_treeDens;

bool Legacy::load(const std::string &dir) {
    auto readMap = [&](const std::string &fn, std::vector<int8_t> &out) {
        std::ifstream in(fn);
        if (!in) { std::cerr << "cannot open " << fn << "\n"; return false; }
        int ww, hh;
        in >> ww >> hh;
        w = ww; h = hh;
        out.assign((size_t)w * h, 0);
        for (size_t k = 0; k < out.size(); k++) { int v; in >> v; out[k] = (int8_t)v; }
        return true;
    };
    if (!readMap(dir + "/europe_geo.map", geo)) return false;
    if (!readMap(dir + "/europe_ff.map", ff)) return false;
    derive();
    return true;
}

void Legacy::edit(int u0, int v0, const std::vector<std::string> &rows) {
    for (size_t k = 0; k < rows.size(); k++)
        for (size_t c = 0; c < rows[k].size(); c++) {
            int i = u0 + (int)c, j = v0 - (int)k;
            if (i < 0 || i >= w || j < 0 || j >= h) continue;
            char ch = rows[k][c];
            int8_t &g = geo[i * h + j], &f = ff[i * h + j];
            bool trees = ch == 't' || ch == 'T';
            switch (ch) {
                case '~': g = 0; break;
                case '.': case 't': g = 1; break;
                case '-': case 'T': g = 2; break;
                case '=': g = 3; break;
                default: continue;
            }
            if (trees) f = 0;
            else if (f == 0 || f == 1) f = -1;
        }
}

void Legacy::derive() {
    // Make diagonal-only water connections 4-connected, so rivers survive upsampling.
    for (int i = 0; i + 1 < w; i++)
        for (int j = 0; j + 1 < h; j++) {
            bool a = geo[i * h + j] == 0, b = geo[(i + 1) * h + j] == 0;
            bool c = geo[i * h + j + 1] == 0, d = geo[(i + 1) * h + j + 1] == 0;
            if (a && d && !b && !c) {
                if (hash2i(i, j, 7) & 1) geo[(i + 1) * h + j] = 0; else geo[i * h + j + 1] = 0;
            } else if (b && c && !a && !d) {
                if (hash2i(i, j, 7) & 1) geo[i * h + j] = 0; else geo[(i + 1) * h + j + 1] = 0;
            }
        }

    g_waterDens.assign((size_t)w * h, 0);
    g_treeDens.assign((size_t)w * h, 0);
    for (int i = 0; i < w; i++)
        for (int j = 0; j < h; j++) {
            int nw = 0, nt = 0, n = 0;
            for (int di = -2; di <= 2; di++)
                for (int dj = -2; dj <= 2; dj++) {
                    n++;
                    if (geoAt(i + di, j + dj) == 0) nw++;
                    int f = ffAt(i + di, j + dj);
                    if (f == 0 || f == 1) nt++;
                }
            g_waterDens[i * h + j] = (float)nw / n;
            g_treeDens[i * h + j] = (float)nt / n;
        }
}

// "oldmap U0 V0" blocks of world.txt redraw cells of the old map (the Dutch coast round
// Amsterdam); they are read before anything else, since towns look at the land as they load
static bool loadOldMapEdits(Legacy &L, const std::string &fn) {
    std::ifstream in(fn);
    if (!in) return false;
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string w;
        int u0, v0;
        if (!(ss >> w) || w != "oldmap" || !(ss >> u0 >> v0)) continue;
        std::vector<std::string> rows;
        while (std::getline(in, line) && line.rfind("end", 0) != 0) {
            size_t bar = line.find('|');   // each row is written between bars: |....|
            size_t bar2 = bar == std::string::npos ? bar : line.find('|', bar + 1);
            if (bar == std::string::npos || bar2 == std::string::npos) continue;
            rows.push_back(line.substr(bar + 1, bar2 - bar - 1));
        }
        L.edit(u0, v0, rows);
        n++;
    }
    if (n) L.derive();
    return true;
}

static float elevOfClass(int c) {
    static const float E[7] = {-0.6f, 0.25f, 0.6f, 1.4f, 3.4f, 5.2f, 7.2f};
    return E[std::max(0, std::min(6, c))];
}

// bilinear interpolation of a per-cell quantity at legacy coordinates (u,v) (cell centres
// at integers)
template <class F> static float bilerp(const Legacy &L, float u, float v, F f) {
    int i = (int)std::floor(u), j = (int)std::floor(v);
    float fu = u - i, fv = v - j;
    float a = f(i, j), b = f(i + 1, j), c = f(i, j + 1), d = f(i + 1, j + 1);
    return (a * (1 - fu) + b * fu) * (1 - fv) + (c * (1 - fu) + d * fu) * fv;
}

static inline void legacyCoords(const World &w, float X, float Y, float &u, float &v) {
    // domain warp gives organic coastlines instead of blocky upsampled cells
    float f = 23.0f * w.kS;
    float wx = 0.45f * fbm(X / f, Y / f, 3, 11);
    float wy = 0.45f * fbm(X / f + 40, Y / f - 13, 3, 12);
    u = X / w.S - 0.5f + wx;
    v = Y / w.S - 0.5f + wy;
}

float World::landFrac(float X, float Y) const {
    float u, v;
    legacyCoords(*this, X, Y, u, v);
    float lf = bilerp(L, u, v, [&](int i, int j) { return L.geoAt(i, j) > 0 ? 1.0f : 0.0f; });
    return lf + 0.10f * fbm(X / (6.0f * kS), Y / (6.0f * kS), 2, 21);
}

float World::cliffAt(float X, float Y, const Cliff **which) const {
    float best = 0;
    for (const auto &c : cliffs) {
        float e = 15.0f * kS;   // the cliffs fade out at the ends of their stretch
        float fx = std::min(X - c.x0, c.x1 - X), fy = std::min(Y - c.y0, c.y1 - Y);
        if (fx <= 0 || fy <= 0) continue;
        float f = smoothstep(0, e, fx) * smoothstep(0, e, fy);
        if (f > best) { best = f; if (which) *which = &c; }
    }
    return best;
}

float World::seaNear(float X, float Y) const {
    float u, v;
    legacyCoords(*this, X, Y, u, v);
    return bilerp(L, u, v, [&](int i, int j) {
        i = std::max(0, std::min(L.w - 1, i)); j = std::max(0, std::min(L.h - 1, j));
        return g_waterDens[i * L.h + j];
    });
}

float World::baseElev(float X, float Y) const {
    float u, v;
    legacyCoords(*this, X, Y, u, v);
    float e = bilerp(L, u, v, [&](int i, int j) { return elevOfClass(L.geoAt(i, j)); });
    float d = fbm(X / (11.0f * kS), Y / (11.0f * kS), 4, 31);
    float h = e + d * (0.10f + 0.28f * std::max(0.0f, e));
    if (e > 2.0f) {  // ridges in the mountains
        float r = 1.0f - std::fabs(vnoise(X / (7.0f * kS), Y / (7.0f * kS), 41));
        h += (e - 2.0f) * 0.45f * r * r;
    }
    return h;
}

// the water of a tile inside settlement s (its map decides), or of the open country
static bool waterInSettlement(const Settlement &s, int X, int Y) {
    if (s.ch(X, Y) == ' ') return false;
    auto t = const_cast<Settlement &>(s).tileAt(X, Y).terrain;
    return t == T_WATER || t == T_DEEP || t == T_BRIDGE;
}

bool World::waterAt(int X, int Y) const {
    if (!inWorld(X, Y)) return true;
    for (const auto &s : settlements)
        if (s.inside(X, Y)) return waterInSettlement(s, X, Y);
    if (carved.count(key(X, Y))) return true;
    return landFrac(X + 0.5f, Y + 0.5f) < 0.5f;
}

bool World::waterAtIn(int X, int Y, const SettlementList &near) const {
    if (!inWorld(X, Y)) return true;
    for (const Settlement *s : near)
        if (s->inside(X, Y)) return waterInSettlement(*s, X, Y);
    if (carved.count(key(X, Y))) return true;
    return landFrac(X + 0.5f, Y + 0.5f) < 0.5f;
}

// distance from a point to a settlement rectangle (0 inside)
static float distToRect(const Settlement &s, float x, float y) {
    float dx = std::max({(float)s.x0 - x, 0.0f, x - (float)(s.x0 + s.w)});
    float dy = std::max({(float)s.y0 - y, 0.0f, y - (float)(s.y0 + s.h)});
    return std::sqrt(dx * dx + dy * dy);
}

// The settlements, in order, that can matter to anything computed for the tiles X0..X1 x Y0..Y1 (the
// corners X1, Y1 included): those whose rectangle is within 64 tiles of the block (or within their
// own blending margin), more than the 45 tiles that baseTile looks at.
World::SettlementList World::nearSettlements(int X0, int Y0, int X1, int Y1) const {
    SettlementList out;
    for (const auto &s : settlements) {
        float dx = std::max({(float)s.x0 - (float)X1, 0.0f, (float)X0 - (float)(s.x0 + s.w)});
        float dy = std::max({(float)s.y0 - (float)Y1, 0.0f, (float)Y0 - (float)(s.y0 + s.h)});
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < std::max(64.0f * kS, s.margin) + 2) out.push_back(&s);
    }
    return out;
}

Tile World::baseTile(int X, int Y) const {
    SettlementList all;
    for (const auto &s : settlements) all.push_back(&s);
    return baseTileIn(X, Y, all);
}

Tile World::baseTileIn(int X, int Y, const SettlementList &near) const {
    Tile t;
    float cx = X + 0.5f, cy = Y + 0.5f;
    float u, v;
    legacyCoords(*this, cx, cy, u, v);
    int ci = (int)std::floor(u + 0.5f), cj = (int)std::floor(v + 0.5f);
    float wd = bilerp(L, u, v, [&](int i, int j) {
        i = std::max(0, std::min(L.w - 1, i)); j = std::max(0, std::min(L.h - 1, j));
        return g_waterDens[i * L.h + j];
    });
    t.var = (uint8_t)(hash2i(X, Y, 5) & 0xff);
    if (waterAtIn(X, Y, near)) {
        t.terrain = (wd > 0.86f) ? T_DEEP : T_WATER;
        return t;
    }
    float e = baseElev(cx, cy);
    float n1 = fbm(cx / (13.0f * kS), cy / (13.0f * kS), 3, 51);
    int cls = L.geoAt(ci, cj);

    // near water?
    bool coast = false;
    int CR = (int)std::lround(2 * kS);
    if (wd > 0.55f)   // beaches on sea coasts, not along rivers
        for (int dy = -CR; dy <= CR && !coast; dy++)
            for (int dx = -CR; dx <= CR && !coast; dx++)
                if (waterAtIn(X + dx, Y + dy, near)) coast = true;

    if (e > 6.3f) t.terrain = T_SNOW;
    else if (e > 3.3f) t.terrain = T_ROCK;
    else if (coast && e < 0.9f && cliffAt(cx, cy) < 0.3f) t.terrain = T_SAND;
    else if (cls == 1 && n1 > 0.25f && e < 0.5f) t.terrain = T_MARSH;
    else if (e > 1.15f + 0.4f * n1) t.terrain = T_SCRUB;
    else t.terrain = (n1 > 0.05f) ? T_MEADOW : T_GRASS;

    // trees: forests where the old map had trees, plus scattered single trees
    float td = bilerp(L, u, v, [&](int i, int j) {
        i = std::max(0, std::min(L.w - 1, i)); j = std::max(0, std::min(L.h - 1, j));
        return g_treeDens[i * L.h + j];
    });
    float forest = 0.55f * fbm(cx / (27.0f * kS), cy / (27.0f * kS), 4, 61) + 2.2f * td - 0.12f;
    float r = hash01(X, Y, 71);
    float dens = 1.0f / (kS * kS);   // trees are bigger at larger scales, so fewer per tile
    bool tree = false;
    if (t.terrain == T_GRASS || t.terrain == T_MEADOW || t.terrain == T_SCRUB) {
        if (forest > 0.22f) tree = r < 0.75f * dens;
        else if (forest > 0.12f) tree = r < 0.15f * dens;
        else tree = r < 0.02f * dens;
    } else if (t.terrain == T_MARSH) {
        tree = r < 0.05f * dens;
    }

    // farmland around towns
    float dTown = 1e9f;
    for (const Settlement *s : near) dTown = std::min(dTown, distToRect(*s, cx, cy));
    if (dTown > 2 * kS && dTown < (34 + 10 * n1) * kS && (t.terrain == T_GRASS || t.terrain == T_MEADOW) &&
        e < 1.3f) {
        float fw = 7.0f * kS, fh = 5.0f * kS;
        float gx = cx + 3 * kS * vnoise(cy / (9 * kS), 0, 81), gy = cy + 3 * kS * vnoise(cx / (9 * kS), 1, 82);
        int fx = (int)std::floor(gx / fw);
        int fy = (int)std::floor(gy / fh);
        uint32_t hh = hash2i(fx, fy, 83);
        if ((hh & 7) < 6) {
            t.terrain = T_FIELD;
            t.var = (uint8_t)((hh >> 4) % 5);   // crop
            tree = false;
            if ((gx - fx * fw) < 1.0f && r < 0.12f) { t.feature = F_POPLAR; }
        } else {
            tree = r < 0.03f * dens;
        }
    }

    if (tree) {
        if (e > 1.6f || v > 520) t.feature = F_PINE;   // mountains, and the far north (old-map rows)
        else if (wd > 0.25f && r < 0.2f) t.feature = F_POPLAR;
        else if (dTown < 45 * kS && r < 0.06f * dens) t.feature = F_FRUIT;
        else t.feature = F_OAK;
    } else if (t.feature == F_NONE) {
        if (t.terrain == T_SCRUB && r > 0.93f) t.feature = F_BUSH;
        else if (t.terrain == T_ROCK && r > 0.9f) t.feature = F_BOULDER;
        else if (t.terrain == T_MEADOW && r > 0.985f) t.feature = F_BUSH;
    }
    return t;
}

// ------------------------------------------------------------------ chunks

void World::clearChunks() {
    chunks.clear();
    std::fill(chunkFlat.begin(), chunkFlat.end(), nullptr);
    lastChunk = nullptr;
}

Chunk *World::chunkIfReady(int cx, int cy) {
    if (lastChunk && lastChunk->cx == cx && lastChunk->cy == cy) return lastChunk;
    unsigned ix = (unsigned)(cx - flatX0), iy = (unsigned)(cy - flatY0);
    if (ix < (unsigned)flatW && iy < (unsigned)flatH) {
        Chunk *c = chunkFlat[(size_t)iy * flatW + ix];
        if (c) lastChunk = c;
        return c;
    }
    auto it = chunks.find(key(cx, cy));
    return it == chunks.end() ? nullptr : it->second.get();
}

Chunk *World::chunkSlow(int cx, int cy) {
    if (flatW == 0 && W > 0 && H > 0) {   // the index is made when the size of the world is known
        flatX0 = -2; flatY0 = -2;
        flatW = (W + CH - 1) / CH + 4; flatH = (H + CH - 1) / CH + 4;
        chunkFlat.assign((size_t)flatW * flatH, nullptr);
        for (auto &kv : chunks) {   // chunks made before it
            Chunk *c = kv.second.get();
            unsigned ix = (unsigned)(c->cx - flatX0), iy = (unsigned)(c->cy - flatY0);
            if (ix < (unsigned)flatW && iy < (unsigned)flatH) chunkFlat[(size_t)iy * flatW + ix] = c;
        }
    }
    unsigned ix = (unsigned)(cx - flatX0), iy = (unsigned)(cy - flatY0);
    const bool inFlat = ix < (unsigned)flatW && iy < (unsigned)flatH;
    if (inFlat) {
        Chunk *c = chunkFlat[(size_t)iy * flatW + ix];
        if (c) { lastChunk = c; return c; }
    } else {
        auto it = chunks.find(key(cx, cy));
        if (it != chunks.end()) return it->second.get();
    }
    auto c = std::make_unique<Chunk>();
    c->cx = cx; c->cy = cy;
    const int X0 = cx * CH, Y0 = cy * CH;
    const int M = (int)std::lround(4 * kS), N = CH + 2 * M;
    const SettlementList near = nearSettlements(X0, Y0, X0 + CH, Y0 + CH);
    std::vector<uint8_t> wm(N * N);
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) wm[j * N + i] = waterAtIn(X0 - M + i, Y0 - M + j, near);
    auto wet = [&](int X, int Y) { return wm[(Y - Y0 + M) * N + (X - X0 + M)] != 0; };

    for (int vy = 0; vy <= CH; vy++)
        for (int vx = 0; vx <= CH; vx++) {
            int X = X0 + vx, Y = Y0 + vy;
            float d = 99;
            for (int dy = -M; dy < M; dy++)
                for (int dx = -M; dx < M; dx++)
                    if (wet(X + dx, Y + dy)) {
                        float dd = std::max(std::fabs(dx + 0.5f), std::fabs(dy + 0.5f)) - 0.5f;
                        d = std::min(d, dd);
                    }
            float h = kH * baseElev((float)X, (float)Y);
            h = std::max(h, 0.12f);
            float base = h;
            if (d < M) h = mixf(0.12f, h, smoothstep(0, (float)M, d));
            // cliff coasts: the land stays high right to the sea, and ends in a sheer face
            const Cliff *cl = nullptr;
            float cf = cliffs.empty() ? 0.0f : cliffAt((float)X, (float)Y, &cl);
            if (cf > 0) {
                // chalk downs: highest at the sea, sloping gently inland over a cell or two of the old map
                float sn = seaNear((float)X, (float)Y);
                float hc = cl->height * cf * smoothstep(0.08f, 0.42f, sn) *
                           (0.85f + 0.3f * (0.5f + 0.5f * fbm(X / (9.0f * kS), Y / (9.0f * kS), 3, 95)));
                h = std::max(h, std::max(base, hc));
            }
            for (const Settlement *sp : near) {
                const Settlement &s = *sp;
                float dr = distToRect(s, (float)X, (float)Y);
                if (dr < s.margin) h = mixf(h, s.ground, smoothstep(s.margin, 0, dr));
            }
            c->h(vx, vy) = h;
        }

    for (int ly = 0; ly < CH; ly++)
        for (int lx = 0; lx < CH; lx++) {
            int X = X0 + lx, Y = Y0 + ly;
            Tile t;
            if (!inWorld(X, Y)) {
                t.terrain = T_DEEP;
            } else {
                const Settlement *in = nullptr;
                for (const Settlement *s : near)
                    if (s->inside(X, Y)) { in = s; break; }
                if (in && in->ch(X, Y) != ' ') {
                    t = const_cast<Settlement *>(in)->tileAt(X, Y);
                } else {
                    t = baseTileIn(X, Y, near);
                    // a blank (' ') tile of a town map is the open country, but no tree, bush or
                    // boulder may grow there within two tiles of the town's buildings, streets or
                    // walls (an oak shut the alley between Westminster Abbey and the Hall)
                    if (in && (t.feature == F_OAK || t.feature == F_PINE || t.feature == F_POPLAR ||
                               t.feature == F_FRUIT || t.feature == F_BUSH || t.feature == F_BOULDER)) {
                        bool nearTown = false;
                        for (int dy = -2; dy <= 2 && !nearTown; dy++)
                            for (int dx = -2; dx <= 2 && !nearTown; dx++) {
                                char ch = in->ch(X + dx, Y + dy);
                                nearTown = ch != ' ' && ch != '~' && ch != 't' && ch != 'T' && ch != ',';
                            }
                        if (nearTown) t.feature = F_NONE;
                    }
                    if (roads.count(key(X, Y))) {
                        t.feature = F_NONE;
                        t.terrain = (t.terrain == T_WATER || t.terrain == T_DEEP) ? T_BRIDGE : T_ROAD;
                    }
                }
            }
            c->t(lx, ly) = t;
        }
    Chunk *p = c.get();
    chunks[key(cx, cy)] = std::move(c);
    if (inFlat) chunkFlat[(size_t)iy * flatW + ix] = p;
    lastChunk = p;
    return p;
}

static inline int floordiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }

Tile &World::tile(int X, int Y) {
    Chunk *c = chunk(floordiv(X, CH), floordiv(Y, CH));
    return c->t(X - c->cx * CH, Y - c->cy * CH);
}

float World::cornerH(int X, int Y) {
    int cx = floordiv(X, CH), cy = floordiv(Y, CH);
    Chunk *c = chunk(cx, cy);
    return c->h(X - cx * CH, Y - cy * CH);
}

float World::groundZ(float x, float y) {
    int X = (int)std::floor(x), Y = (int)std::floor(y);
    Tile &t = tile(X, Y);
    float a = cornerH(X, Y), b = cornerH(X + 1, Y), c = cornerH(X, Y + 1), d = cornerH(X + 1, Y + 1);
    if (t.terrain == T_WATER || t.terrain == T_DEEP) return 0.0f;
    if (t.terrain == T_BRIDGE) return std::max({a, b, c, d}) + 0.12f;
    float fx = x - X, fy = y - Y;
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}

static bool featureBlocks(int f) {
    switch (f) {
        case F_OAK: case F_PINE: case F_FRUIT: case F_POPLAR: case F_BOULDER: case F_PILLAR:
        case F_TABLE: case F_HEARTH: case F_BED: case F_ALTAR: case F_WELL: case F_FOUNTAIN:
        case F_STALL: case F_STATUE: case F_CHEST: case F_BARREL: case F_CROSS:
        case F_LADDER: case F_HATCH: case F_STAIRS: case F_SHAFT: case F_BELL:
        case F_ANVIL: case F_FORGE: case F_COUNTER: case F_OVEN:
        case F_OBELISK: case F_COLUMN:
        case F_TENT: case F_GABION: case F_CANNON:
            return true;
        default:
            return false;
    }
}

bool featureBlocksWalk(int f) { return featureBlocks(f); }

bool World::passable(int X, int Y) {
    if (!inWorld(X, Y)) return false;
    Tile &t = tile(X, Y);
    if (t.terrain == T_WATER || t.terrain == T_DEEP || t.terrain == T_ROCK || t.terrain == T_SNOW)
        return false;
    if (t.wall != W_NONE && t.wall != W_DOOR && t.wall != W_GATE) return false;
    if (featureBlocks(t.feature)) return false;
    return true;
}

bool World::blocksDiagonal(int X, int Y) {
    Tile &t = tile(X, Y);
    return t.wall != W_NONE && t.wall != W_DOOR && t.wall != W_GATE;
}

// ------------------------------------------------------------------ floors

int World::addLevel(Level &&L) {
    int id = (int)levels.size();
    levels.push_back(std::move(L));
    Level &l = levels.back();
    for (int cy = (int)std::floor(l.y0 / (float)CH); cy <= (int)std::floor((l.y0 + l.h - 1) / (float)CH); cy++)
        for (int cx = (int)std::floor(l.x0 / (float)CH); cx <= (int)std::floor((l.x0 + l.w - 1) / (float)CH); cx++)
            levelIndex[key3(cx, cy, l.z)].push_back(id);
    return id;
}

Level *World::levelAt(int X, int Y, int z) {
    if (z == 0) return nullptr;
    auto it = levelIndex.find(key3((int)std::floor(X / (float)CH), (int)std::floor(Y / (float)CH), z));
    if (it == levelIndex.end()) return nullptr;
    for (int id : it->second) {
        Level &l = levels[id];
        if (l.inside(X, Y) && l.at(X, Y).terrain != T_VOID) return &l;
    }
    return nullptr;
}

Tile *World::tileAt(int X, int Y, int z) {
    if (z == 0) return inWorld(X, Y) ? &tile(X, Y) : nullptr;
    Level *l = levelAt(X, Y, z);
    return l ? &l->at(X, Y) : nullptr;
}

bool World::passable(int X, int Y, int z) {
    if (z == 0) return passable(X, Y);
    Tile *t = tileAt(X, Y, z);
    if (!t) return false;
    if (t->wall != W_NONE && t->wall != W_DOOR && t->wall != W_GATE) return false;
    return !featureBlocks(t->feature);
}

bool World::blocksDiagonal(int X, int Y, int z) {
    if (z == 0) return blocksDiagonal(X, Y);
    Tile *t = tileAt(X, Y, z);   // open air does not stop a diagonal step along a wall walk
    return t && t->wall != W_NONE && t->wall != W_DOOR && t->wall != W_GATE;
}

float World::floorAt(float x, float y, int z) {
    if (z == 0) return groundZ(x, y);
    Level *l = levelAt((int)std::floor(x), (int)std::floor(y), z);
    return l ? l->floor : 0.0f;
}

static inline size_t linkSlot(int64_t k, size_t mask) { return (size_t)(((uint64_t)k * 0x9E3779B97F4A7C15ull) >> 32) & mask; }

void World::addLink(int X, int Y, int z, int X2, int Y2, int z2, const std::string &verb) {
    const int64_t k = key3(X, Y, z);
    if (!links.count(k)) linkSources.push_back({X, Y, z});
    if (linkBits.empty() && W > 0 && H > 0) {   // (W and H are known before any place is loaded)
        linkBits.assign(((size_t)W * H + 63) / 64, 0);
        for (auto &s : linkSources) if (inWorld(s[0], s[1])) { size_t i = (size_t)s[1] * W + s[0]; linkBits[i >> 6] |= 1ull << (i & 63); }
    }
    if (!linkBits.empty() && inWorld(X, Y)) { size_t i = (size_t)Y * W + X; linkBits[i >> 6] |= 1ull << (i & 63); }
    links[k] = Link{X2, Y2, z2, verb};
    linksIntoFor = (size_t)-1;
    // (the node of `links` for k keeps its address when it is overwritten, or when the map rehashes)
    if ((links.size() + 1) * 4 > linkTab.size()) {
        size_t n = std::max<size_t>(1024, linkTab.size() * 2);
        linkTab.assign(n, LinkSlot{INT64_MIN, nullptr});
        for (auto &kv : links) {
            size_t s = linkSlot(kv.first, n - 1);
            while (linkTab[s].key != INT64_MIN) s = (s + 1) & (n - 1);
            linkTab[s] = LinkSlot{kv.first, &kv.second};
        }
        return;
    }
    size_t mask = linkTab.size() - 1, s = linkSlot(k, mask);
    while (linkTab[s].key != INT64_MIN && linkTab[s].key != k) s = (s + 1) & mask;
    linkTab[s] = LinkSlot{k, &links[k]};
}

const std::vector<std::array<int, 3>> &World::linksInto(int X, int Y, int z) {
    static const std::vector<std::array<int, 3>> none;
    if (linksIntoFor != linkSources.size()) {
        linksIntoMap.clear();
        for (auto &s : linkSources) {
            const Link &l = links[key3(s[0], s[1], s[2])];
            linksIntoMap[key3(l.x, l.y, l.z)].push_back(s);
        }
        linksIntoFor = linkSources.size();
    }
    auto it = linksIntoMap.find(key3(X, Y, z));
    return it == linksIntoMap.end() ? none : it->second;
}

const Link *World::linkAt(int X, int Y, int z) const {
    if (linkTab.empty()) return nullptr;
    if (!linkBits.empty() && inWorld(X, Y)) {
        size_t i = (size_t)Y * W + X;
        if (!((linkBits[i >> 6] >> (i & 63)) & 1)) return nullptr;
    }
    const int64_t k = key3(X, Y, z);
    const size_t mask = linkTab.size() - 1;
    for (size_t s = linkSlot(k, mask);; s = (s + 1) & mask) {
        if (linkTab[s].key == k) return linkTab[s].link;
        if (linkTab[s].key == INT64_MIN) return nullptr;
    }
}

std::string World::floorName(int X, int Y, int z) {
    if (z == 0) return placeName(X, Y);
    Level *l = levelAt(X, Y, z);
    if (!l) return "nowhere";
    // "the top of the Leaning Tower, Pisa", not "the top of the Leaning Tower, the Leaning Tower,
    // Pisa": the building (and the town) only when the floor's own name does not say it already
    auto says = [&](const std::string &part) {
        std::string a = l->name, b = part;
        for (char &c : a) c = (char)std::tolower((unsigned char)c);
        for (char &c : b) c = (char)std::tolower((unsigned char)c);
        if (b.rfind("the ", 0) == 0) b = b.substr(4);
        return !b.empty() && a.find(b) != std::string::npos;
    };
    std::string where;
    if (l->bld) {
        const Building &b = buildings[l->bld - 1];
        if (!b.name.empty() && !says(b.name)) where = b.name;
    }
    for (auto &s : settlements)
        if (s.inside(X, Y)) { if (!says(s.name)) where = where.empty() ? s.name : where + ", " + s.name; break; }
    return where.empty() ? l->name : l->name + ", " + where;
}

Building *World::buildingAt(int X, int Y) {
    int b = tile(X, Y).bld;
    return (b > 0 && b <= (int)buildings.size()) ? &buildings[b - 1] : nullptr;
}

std::string World::placeName(int X, int Y, int *bldOut) {
    if (bldOut) *bldOut = 0;
    Building *b = buildingAt(X, Y);
    for (auto &s : settlements)
        if (distToRect(s, X + 0.5f, Y + 0.5f) < 2) {
            if (b && !b->name.empty()) {
                if (bldOut) *bldOut = tile(X, Y).bld;
                return b->name + ", " + s.name;
            }
            return s.name;
        }
    // on a cliff coast?
    {
        const Cliff *cl = nullptr;
        if (!cliffs.empty() && cliffAt(X + 0.5f, Y + 0.5f, &cl) > 0.4f && seaNear(X + 0.5f, Y + 0.5f) > 0.25f && !cl->name.empty())
            return cl->name;
    }
    // nearest settlement and rough direction
    const Settlement *best = nullptr;
    float bd = 1e9f;
    for (auto &s : settlements) {
        float d = distToRect(s, X + 0.5f, Y + 0.5f);
        if (d < bd) { bd = d; best = &s; }
    }
    if (best && bd < 120) {
        float dx = X - (best->x0 + best->w * 0.5f), dy = Y - (best->y0 + best->h * 0.5f);
        float a = std::atan2(dy, dx) * 180 / PI_F;
        const char *dir[8] = {"east", "north-east", "north", "north-west", "west",
                              "south-west", "south", "south-east"};
        int k = ((int)std::floor((a + 360 + 22.5f) / 45.0f)) % 8;
        return std::string("countryside ") + dir[k] + " of " + best->name;
    }
    return "open country";
}

// ------------------------------------------------------------------ roads and channels

bool World::addRoad(const std::vector<vec2> &pts, int width, const std::string &line) {
    // sample the road's centre line; it bridges rivers, but stops where it meets the sea
    std::vector<std::pair<vec2, vec2>> samples;   // point, normal
    for (size_t k = 0; k + 1 < pts.size(); k++) {
        vec2 a = pts[k], b = pts[k + 1];
        float len = length(b - a);
        int n = std::max(1, (int)(len * 2));
        vec2 dir = (b - a) * (1.0f / std::max(len, 1e-3f));
        vec2 nrm(-dir.y, dir.x);
        for (int i = 0; i <= n; i++) {
            float t = (float)i / n;
            // gentle meander, vanishing at the ends
            float wob = 2.5f * kS * std::sin(t * PI_F) * vnoise(t * len / (15.0f * kS), (float)k, 91);
            samples.push_back({a + (b - a) * t + nrm * wob, nrm});
        }
    }
    const float maxBridge = 12.0f * kS;   // tiles of water a road may cross
    size_t end = samples.size();
    for (size_t i = 0; i < samples.size(); i++) {
        vec2 p = samples[i].first;
        if (!waterAt((int)std::floor(p.x), (int)std::floor(p.y))) continue;
        size_t j = i;
        while (j < samples.size() && waterAt((int)std::floor(samples[j].first.x), (int)std::floor(samples[j].first.y))) j++;
        if ((j - i) * 0.5f > maxBridge || j == samples.size()) { end = i; break; }
        i = j;
    }
    // a road that runs into the sea and stays in it ends at the shore, as meant; one with land
    // beyond the water it could not bridge is cut short (reported by --stats)
    bool landBeyond = false;
    for (size_t k = end; k < samples.size() && !landBeyond; k++)
        landBeyond = !waterAt((int)std::floor(samples[k].first.x), (int)std::floor(samples[k].first.y));
    if (end < samples.size() && landBeyond) roadStops.push_back({samples[end].first, line});
    for (size_t i = 0; i < end; i++) {
        vec2 p = samples[i].first, nrm = samples[i].second;
        for (int w = 0; w < width; w++) {
            vec2 q = p + nrm * (w - (width - 1) * 0.5f);
            roads.insert(key((int)std::floor(q.x), (int)std::floor(q.y)));
        }
    }
    return end == samples.size() || !landBeyond;
}

std::vector<float> World::roadDistances(int from) {
    const int n = (int)settlements.size();
    if ((int)roadTouch.size() != n) {
        roadTouch.assign(n, {});
        for (int64_t k : roads) {
            int X = (int)(k >> 32), Y = (int)(int32_t)(k & 0xffffffff);
            for (int i = 0; i < n; i++) {
                const Settlement &s = settlements[i];
                int m = s.hamlet ? 12 : 3;   // a post house stands back from its road, across its yard
                if (X >= s.x0 - m && X < s.x0 + s.w + m && Y >= s.y0 - m && Y < s.y0 + s.h + m) roadTouch[i].push_back(k);
            }
        }
    }
    std::vector<float> out(n, -1.0f);
    if (from < 0 || from >= n) return out;
    std::unordered_map<int64_t, float> dist;
    std::unordered_map<int64_t, std::vector<int>> hubOf;   // road tile -> the settlements it touches
    for (int i = 0; i < n; i++) for (int64_t k : roadTouch[i]) hubOf[k].push_back(i);
    typedef std::pair<float, int64_t> QE;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> q;
    auto enter = [&](int t, float d) {   // into settlement t: out again by any of its roads
        if (out[t] >= 0) return;
        out[t] = d;
        float cross = 0.35f * (settlements[t].w + settlements[t].h);   // through the streets
        for (int64_t k : roadTouch[t]) {
            float dd = (t == from) ? 0.0f : d + cross;
            auto it = dist.find(k);
            if (it == dist.end() || dd < it->second) { dist[k] = dd; q.push({dd, k}); }
        }
    };
    enter(from, 0);
    while (!q.empty()) {
        auto [d, k] = q.top(); q.pop();
        auto it = dist.find(k);
        if (it != dist.end() && d > it->second) continue;
        auto h = hubOf.find(k);
        if (h != hubOf.end()) for (int t : h->second) if (out[t] < 0) enter(t, d);
        int X = (int)(k >> 32), Y = (int)(int32_t)(k & 0xffffffff);
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                if (!dx && !dy) continue;
                int64_t nk = key(X + dx, Y + dy);
                if (!roads.count(nk)) continue;
                float nd = d + ((dx && dy) ? 1.4142f : 1.0f);
                auto jt = dist.find(nk);
                if (jt == dist.end() || nd < jt->second) { dist[nk] = nd; q.push({nd, nk}); }
            }
    }
    return out;
}

bool World::roadRoute(int from, int to, std::vector<RoadPt> &out) {
    out.clear();
    const int n = (int)settlements.size();
    roadDistances(from);   // builds roadTouch
    if (from < 0 || to < 0 || from >= n || to >= n || from == to) return false;
    std::unordered_map<int64_t, float> dist;
    std::unordered_map<int64_t, int64_t> prev;   // road tile -> the tile it was reached from (-1: a start)
    std::unordered_map<int64_t, std::vector<int>> hubOf;
    for (int i = 0; i < n; i++) for (int64_t k : roadTouch[i]) hubOf[k].push_back(i);
    typedef std::pair<float, int64_t> QE;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> q;
    std::vector<char> entered(n, 0);
    auto enter = [&](int t, float d, int64_t via) {
        if (entered[t]) return;
        entered[t] = 1;
        float cross = 0.35f * (settlements[t].w + settlements[t].h);
        for (int64_t k : roadTouch[t]) {
            float dd = (t == from) ? 0.0f : d + cross;
            auto it = dist.find(k);
            if (it == dist.end() || dd < it->second) { dist[k] = dd; prev[k] = t == from ? -1 : via; q.push({dd, k}); }
        }
    };
    enter(from, 0, -1);
    while (!q.empty()) {
        auto [d, k] = q.top(); q.pop();
        auto it = dist.find(k);
        if (it != dist.end() && d > it->second) continue;
        auto h = hubOf.find(k);
        if (h != hubOf.end()) for (int t : h->second) if (!entered[t]) enter(t, d, k);
        int X = (int)(k >> 32), Y = (int)(int32_t)(k & 0xffffffff);
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                if (!dx && !dy) continue;
                int64_t nk = key(X + dx, Y + dy);
                if (!roads.count(nk)) continue;
                float nd = d + ((dx && dy) ? 1.4142f : 1.0f);
                auto jt = dist.find(nk);
                if (jt == dist.end() || nd < jt->second) { dist[nk] = nd; prev[nk] = k; q.push({nd, nk}); }
            }
    }
    int64_t best = -1;
    float bd = 1e30f;
    for (int64_t k : roadTouch[to]) { auto it = dist.find(k); if (it != dist.end() && it->second < bd) { bd = it->second; best = k; } }
    if (best < 0) return false;
    for (int64_t k = best; k != -1; ) {
        out.push_back({(int)(k >> 32), (int)(int32_t)(k & 0xffffffff), dist[k]});
        auto pv = prev.find(k);
        if (pv == prev.end()) break;
        k = pv->second;
    }
    std::reverse(out.begin(), out.end());
    return true;
}

void World::carvePath(const std::vector<vec2> &pts, int width) {
    for (size_t k = 0; k + 1 < pts.size(); k++) {
        vec2 a = pts[k], b = pts[k + 1];
        float len = length(b - a);
        int n = std::max(1, (int)(len * 2));
        for (int i = 0; i <= n; i++) {
            vec2 p = a + (b - a) * ((float)i / n);
            float r = width * 0.5f;
            for (int dy = -(int)r - 1; dy <= (int)r + 1; dy++)
                for (int dx = -(int)r - 1; dx <= (int)r + 1; dx++)
                    if (dx * dx + dy * dy <= r * r + 0.5f)
                        carved.insert(key((int)std::floor(p.x) + dx, (int)std::floor(p.y) + dy));
        }
    }
}

void World::carveChannel(vec2 a, vec2 b, int width) {
    float len = length(b - a);
    int n = std::max(1, (int)(len * 2));
    vec2 dir = (b - a) * (1.0f / std::max(len, 1e-3f));
    vec2 nrm(-dir.y, dir.x);
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n;
        float wob = 3.0f * kS * std::sin(t * PI_F) * vnoise(t * len / (18.0f * kS), 3, 92);
        vec2 p = a + (b - a) * t + nrm * wob;
        for (int w = -width / 2 - 1; w <= width / 2 + 1; w++) {
            vec2 q = p + nrm * (float)w;
            carved.insert(key((int)std::floor(q.x), (int)std::floor(q.y)));
        }
    }
}

// ------------------------------------------------------------------ data files

static vec3 hexColor(const std::string &s) {
    unsigned v = 0;
    std::sscanf(s.c_str() + (s[0] == '#' ? 1 : 0), "%x", &v);
    return vec3(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f);
}

bool World::loadStyles(const std::string &fn) {
    std::ifstream in(fn);
    if (!in) { std::cerr << "cannot open " << fn << "\n"; return false; }
    std::string line;
    Style *cur = nullptr;
    while (std::getline(in, line)) {
        auto hashpos = line.find('#');
        // '#' starts a comment unless it begins a colour
        std::istringstream ss(line);
        std::string w;
        if (!(ss >> w)) continue;
        if (w[0] == '#' && w.size() == 1) continue;
        if (w.size() > 1 && w[0] == '#' && !isxdigit((unsigned char)w[1])) continue;
        (void)hashpos;
        if (w == "style") {
            styles.push_back(Style());
            cur = &styles.back();
            ss >> cur->name;
        } else if (!cur) {
            continue;
        } else if (w == "walls") {
            std::string c;
            while (ss >> c) cur->walls.push_back(hexColor(c));
        } else if (w == "roofs") {
            std::string c;
            while (ss >> c) cur->roofs.push_back(hexColor(c));
        } else if (w == "trim") {
            std::string c; ss >> c; cur->trim = hexColor(c);
        } else if (w == "roof") {
            std::string r;
            cur->roofTypes.clear();
            while (ss >> r) cur->roofTypes.push_back(r);
            if (!cur->roofTypes.empty()) cur->roof = cur->roofTypes[0];
        } else if (w == "storeys") {
            ss >> cur->minStoreys >> cur->maxStoreys;
        } else if (w == "storey") {
            ss >> cur->storey;
        } else if (w == "pitch") {
            ss >> cur->roofPitch;
        } else if (w == "timber") {
            std::string y; ss >> y;
            cur->timberChance = (y == "yes") ? 1.0f : (y == "no" ? 0.0f : (float)std::atof(y.c_str()));
            cur->timber = cur->timberChance > 0;
        } else if (w == "gablefront") {
            std::string y; ss >> y; cur->gableFront = (y == "yes");
        } else if (w == "jetty") {
            std::string y; ss >> y; cur->jetty = (y == "yes");
        } else if (w == "balustrade") {
            std::string y; ss >> y; cur->balustrade = (y == "yes");
        } else if (w == "stepgable") {
            std::string y; ss >> y; cur->stepGable = (y == "yes");
        } else if (w == "portico") {
            std::string y; ss >> y;
            cur->portico = (y == "yes") ? 1.0f : (y == "no" ? 0.0f : (float)std::atof(y.c_str()));
        } else if (w == "lots") {
            ss >> cur->lotWMin >> cur->lotWMax >> cur->lotDMin >> cur->lotDMax;
        } else if (w == "chimney") {
            std::string c; ss >> c; cur->chimney = hexColor(c);
            float t; if (ss >> t) cur->chimneyTall = t;
            std::string f; if (ss >> f) cur->funnel = (f == "funnel");
        } else if (w == "dormers") {
            std::string y; ss >> y; cur->dormers = (y == "yes");
        }
    }
    for (auto &s : styles) {
        if (s.walls.empty()) s.walls.push_back(vec3(0.85f, 0.8f, 0.7f));
        if (s.roofs.empty()) s.roofs.push_back(vec3(0.6f, 0.3f, 0.2f));
    }
    return !styles.empty();
}

int World::styleIndex(const std::string &name) const {
    for (size_t i = 0; i < styles.size(); i++)
        if (styles[i].name == name) return (int)i;
    return 0;
}

// gate positions, filled in by the town loader: "Paris.1" -> world tile
std::unordered_map<std::string, vec2> g_gates;

bool World::loadWorldFile(const std::string &fn) {
    std::ifstream in(fn);
    if (!in) { std::cerr << "cannot open " << fn << "\n"; return false; }
    std::string line;
    std::vector<std::vector<std::string>> roadLines;
    std::string startTown; float sdx = 0, sdy = 0;
    while (std::getline(in, line)) {
        auto c = line.find("//");
        if (c != std::string::npos) line = line.substr(0, c);
        std::istringstream ss(line);
        std::string w;
        if (!(ss >> w) || w[0] == '#') continue;
        if (w == "oldmap") {   // read before (loadOldMapEdits): skip to its "end"
            while (std::getline(in, line) && line.rfind("end", 0) != 0) {}
            continue;
        }
        if (w == "town") {
            std::string name, file; float lx, ly;
            ss >> name >> file >> lx >> ly;
            for (auto &ch : name) if (ch == '_') ch = ' ';
            settlements.push_back(Settlement());
            Settlement &s = settlements.back();
            s.name = name;
            s.file = file;
            if (!loadSettlement(*this, s, dataDir + "/" + file, (int)std::lround(lx * S), (int)std::lround(ly * S)))
                return false;
        } else if (w == "hamlet") {
            // hamlet NAME TEMPLATE X,Y : a farm, a hut or a quarry from data/cities/hamlets/TEMPLATE.map
            std::string name, tmpl, at;
            ss >> name >> tmpl >> at;
            for (auto &ch : name) if (ch == '_') ch = ' ';
            float lx, ly;
            std::sscanf(at.c_str(), "%f,%f", &lx, &ly);
            settlements.push_back(Settlement());
            Settlement &s = settlements.back();
            s.name = name;
            s.file = "cities/hamlets/" + tmpl + ".map";
            s.hamlet = true;
            std::string o;
            while (ss >> o) {   // style=NAME, and C=NAME for the landmark C of the template
                auto eq = o.find('=');
                if (eq == std::string::npos) continue;
                std::string k = o.substr(0, eq), v = o.substr(eq + 1);
                for (auto &ch : v) if (ch == '_') ch = ' ';
                if (k == "style") s.styleOverride = v;
                else if (k.size() == 1) s.legendNames[k[0]] = v;
            }
            if (!loadSettlement(*this, s, dataDir + "/" + s.file, (int)std::lround(lx * S), (int)std::lround(ly * S)))
                return false;
        } else if (w == "road") {
            std::vector<std::string> toks;
            std::string t;
            while (ss >> t) toks.push_back(t);
            roadLines.push_back(toks);
        } else if (w == "river") {
            // river WIDTH X,Y X,Y ...   a river through the countryside (old-map points), carved
            // like the channels that join town rivers to the old map's (it meanders a little)
            int width = 0;
            ss >> width;
            std::string t;
            std::vector<vec2> pts;
            while (ss >> t) {
                float lx, ly;
                if (std::sscanf(t.c_str(), "%f,%f", &lx, &ly) == 2) pts.push_back(vec2(lx * S, ly * S));
            }
            for (size_t k = 0; k + 1 < pts.size(); k++) carveChannel(pts[k], pts[k + 1], std::max(2, width));
        } else if (w == "start") {
            ss >> startTown >> sdx >> sdy;
        } else if (w == "clock") {
            ss >> clockRate;
        } else if (w == "journey") {
            ss >> journeyMinutes;
        } else if (w == "inland") {
            std::string t; while (ss >> t) inlandTowns.insert(t);
        } else if (w == "underground") {
            // underground Z X0,Y0 X1,Y1 SX,SY NAME   (old-map coordinates; SX,SY: the shaft)
            std::string a, b, c, nm;
            UndergroundSpec u;
            float ax, ay, bx, by, cx, cy;
            ss >> u.z >> a >> b >> c;
            std::getline(ss, nm);
            std::sscanf(a.c_str(), "%f,%f", &ax, &ay);
            std::sscanf(b.c_str(), "%f,%f", &bx, &by);
            std::sscanf(c.c_str(), "%f,%f", &cx, &cy);
            u.x0 = (int)(std::min(ax, bx) * S); u.x1 = (int)(std::max(ax, bx) * S);
            u.y0 = (int)(std::min(ay, by) * S); u.y1 = (int)(std::max(ay, by) * S);
            u.sx = (int)(cx * S); u.sy = (int)(cy * S);
            size_t k = nm.find_first_not_of(' ');
            u.name = k == std::string::npos ? "the tunnels" : nm.substr(k);
            undergroundSpecs.push_back(u);
        } else if (w == "cliffs") {
            std::string a, b, col, nm;
            Cliff c;
            ss >> a >> b >> c.height >> col;
            std::getline(ss, nm);
            float ax, ay, bx, by;
            std::sscanf(a.c_str(), "%f,%f", &ax, &ay);
            std::sscanf(b.c_str(), "%f,%f", &bx, &by);
            c.x0 = std::min(ax, bx) * S; c.x1 = std::max(ax, bx) * S;
            c.y0 = std::min(ay, by) * S; c.y1 = std::max(ay, by) * S;
            c.color = hexColor(col);
            size_t k = nm.find_first_not_of(' ');
            c.name = k == std::string::npos ? "" : nm.substr(k);
            cliffs.push_back(c);
        }
    }
    // Every town and hamlet is built now. Any block of tiles made while they were being built
    // (a town's sign looks at the ground outside its doors) was made before the places listed
    // later existed, and shows open country where they stand: forget it, to be made again.
    clearChunks();
    for (auto &toks : roadLines) {
        if (toks.empty()) continue;
        std::string whole;
        for (auto &t : toks) whole += (whole.empty() ? "" : " ") + t;
        int width = std::atoi(toks[0].c_str());
        std::vector<vec2> pts;
        for (size_t i = 1; i < toks.size(); i++) {
            const std::string &t = toks[i];
            auto comma = t.find(',');
            if (comma != std::string::npos) {
                float lx = std::atof(t.substr(0, comma).c_str()), ly = std::atof(t.substr(comma + 1).c_str());
                pts.push_back(vec2(lx * S, ly * S));
            } else {
                auto it = g_gates.find(t);
                if (it == g_gates.end()) { std::cerr << "unknown gate " << t << "\n"; continue; }
                pts.push_back(it->second);
            }
        }
        addRoad(pts, width, whole);
    }
    for (auto &s : settlements)
        if (s.name == startTown) start = vec2(s.x0 + s.w * 0.5f + sdx, s.y0 + s.h * 0.5f + sdy);
    return true;
}

bool World::init(const std::string &dir, int scale) {
    dataDir = dir;
    S = scale;
    kS = S / 5.0f;
    if (!L.load(dir + "/legacy")) return false;
    W = L.w * S;
    H = L.h * S;
    if (!loadStyles(dir + "/styles.txt")) return false;
    if (!loadOldMapEdits(L, dir + "/world.txt")) return false;
    if (!loadWorldFile(dir + "/world.txt")) return false;

    joinRivers();
    for (auto &u : undergroundSpecs) buildUnderground(u.name, u.z, u.x0, u.y0, u.x1, u.y1, u.sx, u.sy);
    return true;
}

// Where a town map meets the land, rivers must continue. "Exits" are where the town's
// own rivers reach the edge of its map; "crossings" are where rivers of the old map
// touch the map from outside. Each crossing is joined to the nearest exit by a channel
// running just outside the map's edge; an exit with no crossing is joined to the
// nearest river outside.
void World::joinRivers() {
    for (auto &s : settlements) {
        // the ring of tiles just outside the map, walked anticlockwise from the south-west corner
        const int off = 1;
        int rx0 = s.x0 - off, ry0 = s.y0 - off, rx1 = s.x0 + s.w - 1 + off, ry1 = s.y0 + s.h - 1 + off;
        std::vector<std::pair<int, int>> ring;
        for (int x = rx0; x < rx1; x++) ring.push_back({x, ry0});
        for (int y = ry0; y < ry1; y++) ring.push_back({rx1, y});
        for (int x = rx1; x > rx0; x--) ring.push_back({x, ry1});
        for (int y = ry1; y > ry0; y--) ring.push_back({rx0, y});
        int P = (int)ring.size();
        auto ringIndex = [&](int X, int Y) {
            int best = 0, bd = 1 << 30;
            for (int i = 0; i < P; i++) {
                int d = std::abs(ring[i].first - X) + std::abs(ring[i].second - Y);
                if (d < bd) { bd = d; best = i; }
            }
            return best;
        };
        auto inOtherTown = [&](int X, int Y) {
            for (auto &t : settlements) if (&t != &s && t.inside(X, Y)) return true;
            return false;
        };
        // crossings: runs of water of the old map along the ring
        std::vector<int> crossings;
        {
            std::vector<uint8_t> wet(P);
            for (int i = 0; i < P; i++)
                wet[i] = !inOtherTown(ring[i].first, ring[i].second) &&
                         landFrac(ring[i].first + 0.5f, ring[i].second + 0.5f) < 0.5f;
            int start = 0;
            while (start < P && wet[start]) start++;   // begin on dry land
            if (start < P)
                for (int k = 0; k < P;) {
                    int i = (start + k) % P;
                    if (!wet[i]) { k++; continue; }
                    int len = 0;
                    while (len < P && wet[(i + len) % P]) len++;
                    crossings.push_back((i + len / 2) % P);
                    k += len;
                }
        }
        std::vector<int> exits;
        for (auto &e : s.waterExits) exits.push_back(ringIndex(e.first, e.second));
        auto ringDist = [&](int a, int b) { int d = std::abs(a - b); return std::min(d, P - d); };
        // a channel from ring position a to ring position b: out from the map, along its
        // side (cutting the corners), and back in, meandering
        const int Wr = rx1 - rx0, Hr = ry1 - ry0;
        const int cornerAt[4] = {0, Wr, Wr + Hr, 2 * Wr + Hr};
        const vec2 cornerOut[4] = {vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, 1)};
        auto sideNormal = [&](int i) {
            if (i < Wr) return vec2(0, -1);
            if (i < Wr + Hr) return vec2(1, 0);
            if (i < 2 * Wr + Hr) return vec2(0, 1);
            return vec2(-1, 0);
        };
        auto around = [&](int a, int b) {
            const int width = (int)std::lround(4 * kS);
            vec2 pa(ring[a].first + 0.5f, ring[a].second + 0.5f), pb(ring[b].first + 0.5f, ring[b].second + 0.5f);
            if (ringDist(a, b) < 6 * kS) { carvePath({pa, pb}, width); return; }
            int d = b - a;
            if (d > P / 2) d -= P;
            if (d < -P / 2) d += P;
            float o = 4.0f * kS;
            std::vector<vec2> pts{pa, pa + sideNormal(a) * o};
            int step = d > 0 ? 1 : -1;
            for (int k = step; k != d; k += step) {
                int i = ((a + k) % P + P) % P;
                for (int c = 0; c < 4; c++)
                    if (i == cornerAt[c]) {
                        vec2 q(ring[i].first + 0.5f, ring[i].second + 0.5f);
                        pts.push_back(q + cornerOut[c] * (o * 1.3f));
                    }
            }
            pts.push_back(pb + sideNormal(b) * o);
            pts.push_back(pb);
            carvePath({pts[0], pts[1]}, width);
            for (size_t k = 1; k + 2 < pts.size(); k++) carveChannel(pts[k], pts[k + 1], width);
            carvePath({pts[pts.size() - 2], pts.back()}, width);
        };
        std::vector<bool> exitUsed(exits.size(), false);
        for (int c : crossings) {
            if (exits.empty()) break;
            int bi = 0;
            for (size_t i = 1; i < exits.size(); i++)
                if (ringDist(c, exits[i]) < ringDist(c, exits[bi])) bi = (int)i;
            exitUsed[bi] = true;
            if (ringDist(c, exits[bi]) <= 2) continue;   // already touching
            around(c, exits[bi]);
        }
        if (exits.empty() && crossings.size() >= 2)   // a river passing by a town without one
            for (size_t i = 0; i + 1 < crossings.size(); i += 2) around(crossings[i], crossings[i + 1]);
        // exits with no crossing: straight out to the nearest river
        for (size_t ei = 0; ei < exits.size(); ei++) {
            if (exitUsed[ei]) continue;
            auto &e = s.waterExits[ei];
            vec2 a((float)e.first + 0.5f, (float)e.second + 0.5f);
            // outward direction: away from the town's centre
            vec2 c(s.x0 + s.w * 0.5f, s.y0 + s.h * 0.5f);
            vec2 out = a - c;
            out = out * (1.0f / std::max(1e-3f, length(out)));
            float best = 1e9f; vec2 bp;
            for (int r = 3; r < (int)(70 * kS); r++)
                for (int k = 0; k < 64; k++) {
                    float ang = k * 2 * PI_F / 64;
                    vec2 d(std::cos(ang), std::sin(ang));
                    if (dot(d, out) < 0.2f) continue;
                    vec2 p = a + d * (float)r;
                    int X = (int)std::floor(p.x), Y = (int)std::floor(p.y);
                    bool inTown = false;
                    for (auto &t : settlements) if (distToRect(t, p.x, p.y) < 1) inTown = true;
                    if (inTown) continue;
                    if (landFrac(X + 0.5f, Y + 0.5f) < 0.5f) {
                        float score = r * (1.5f - dot(d, out));
                        if (score < best) { best = score; bp = p; }
                    }
                }
            if (best < 1e8f) carveChannel(a, bp, (int)std::lround(4 * kS));
        }
    }
}

bool World::reachesSea(int X0, int Y0, int maxTiles, int &seaX, int &seaY, int &visited) {
    std::unordered_set<int64_t> seen;
    std::deque<std::pair<int, int>> q;
    // start from the nearest water
    for (int r = 0; r < 6 && q.empty(); r++)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
                if (q.empty() && waterAt(X0 + dx, Y0 + dy)) q.push_back({X0 + dx, Y0 + dy});
    if (q.empty()) { visited = 0; return false; }
    seen.insert(key(q.front().first, q.front().second));
    while (!q.empty() && (int)seen.size() < maxTiles) {
        auto [x, y] = q.front(); q.pop_front();
        float u = x / (float)S, v = y / (float)S;
        int i = std::max(0, std::min(L.w - 1, (int)u)), j = std::max(0, std::min(L.h - 1, (int)v));
        if (g_waterDens[i * L.h + j] > 0.95f) { seaX = x; seaY = y; visited = (int)seen.size(); return true; }
        const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (auto &e : d) {
            int nx = x + e[0], ny = y + e[1];
            if (!inWorld(nx, ny) || seen.count(key(nx, ny)) || !waterAt(nx, ny)) continue;
            seen.insert(key(nx, ny));
            q.push_back({nx, ny});
        }
    }
    visited = (int)seen.size();
    return false;
}
