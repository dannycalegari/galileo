// city.cc: reading a town's text map and building its houses and landmarks.
//
// Characters with a fixed meaning (the legend of the town file adds the landmarks):
//   ' ' countryside (generated)     '~' water          '=' bridge
//   '.' street                       '_' road           ':' paved square
//   ',' garden                       ';' yard (earth)   't' tree in a garden
//   'h' houses (cut automatically)   '#' town wall      'O' wall tower
//   '1'-'9' gates                    'o' well           'f' fountain
//   'm' market stall                 '+' cross          '%' hedge
//   'b' bench                        'x' hay            'k' barrel
//   's' statue (on horseback)        'i' obelisk        'j' column
//   'y' a pedestal for a standing statue (the statue itself is a work of art, data/items.txt)
//   'g' mooring post with a boat     'p' mooring post (both in water)
//   uppercase letters other than O: landmarks named in the legend
//
// Sizes: a person is about 1.1 tall and one tile wide (see GF_H and friends in world.h).
#include "world.h"
#include "noise.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <set>
#include <deque>
#include <functional>
#include <cstdio>
#include <cctype>

extern std::unordered_map<std::string, vec2> g_gates;

namespace {

struct Legend {
    std::string kind, name;
    std::map<std::string, std::string> opt;
    float num(const std::string &k, float def) const {
        auto it = opt.find(k);
        return it == opt.end() ? def : (float)std::atof(it->second.c_str());
    }
};

struct Cluster {
    std::vector<std::pair<int, int>> tiles;  // world coords
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
};

const vec3 STONE(0.80f, 0.76f, 0.66f);
const vec3 PALE_STONE(0.88f, 0.85f, 0.76f);
const vec3 LEAD(0.42f, 0.46f, 0.52f);
const vec3 SLATE(0.36f, 0.40f, 0.47f);
const vec3 GLASS(0.16f, 0.20f, 0.30f);
const vec3 STAINED(0.20f, 0.22f, 0.55f);
const vec3 FRAME(0.95f, 0.93f, 0.88f);
const vec3 TERRACOTTA(0.70f, 0.37f, 0.25f);
const vec3 BRICK(0.66f, 0.33f, 0.26f);
const vec3 GOLD(0.85f, 0.70f, 0.30f);

vec3 hexColour(const std::string &h) {
    unsigned v = 0;
    std::sscanf(h.c_str() + (h[0] == '#' ? 1 : 0), "%x", &v);
    return vec3(((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f);
}
// the options every landmark may take: stone=HEX (its walls), brick, roof=tile|lead|slate|HEX
vec3 stoneOf(const Legend &L, vec3 def) {
    if (L.opt.count("stone")) return hexColour(L.opt.at("stone"));
    if (L.opt.count("brick")) return BRICK;
    return def;
}
vec3 roofOf(const Legend &L, vec3 def) {
    if (!L.opt.count("roof")) return def;
    const std::string &r = L.opt.at("roof");
    if (r == "tile") return TERRACOTTA;
    if (r == "lead") return LEAD;
    if (r == "slate") return SLATE;
    if (!r.empty() && (r[0] == '#' || std::isxdigit((unsigned char)r[0]))) return hexColour(r);
    return def;
}

bool blocksRoom(int f) {
    return f == F_HEARTH || f == F_TABLE || f == F_BED || f == F_CHEST || f == F_PILLAR || f == F_ALTAR ||
           f == F_BARREL || f == F_LADDER || f == F_HATCH || f == F_STAIRS || f == F_BELL || f == F_SHAFT;
}

struct TownBuilder {
    World &w;
    Settlement &s;
    Rng rng;
    float g;
    float towerRoof = 1;  // 1: conical roofs on wall towers, 0: flat with battlements
    struct Ferry { int c1, r1, c2, r2; bool gondola; std::string a, b; };
    std::vector<Ferry> ferryList;   // "ferry" lines of the town map
    std::vector<std::pair<int, std::string>> legendSigns;   // legend "sign=EMBLEM": hung once the town is built

    TownBuilder(World &w_, Settlement &s_) : w(w_), s(s_), rng(hash32(s_.x0 * 7 + s_.y0)), g(s_.ground) {}

    Level *cur = nullptr;                          // when set, T() and in() refer to this floor
    std::set<std::pair<int, int>> reserved;        // tiles kept free (in front of ladders)
    std::vector<std::pair<int, int>> entries;      // where one arrives on a floor
    bool in(int X, int Y) const { return cur ? cur->inside(X, Y) : s.inside(X, Y); }
    char ch(int X, int Y) const { return s.ch(X, Y); }
    Tile &T(int X, int Y) { return cur ? cur->at(X, Y) : s.tileAt(X, Y); }
    const Style &style() const { return w.styles[s.style]; }
    template <class V> const typename V::value_type &pick(const V &v) { return v[rng.next() % v.size()]; }

    // can a person stand here (as far as the town map says)?
    bool open(int X, int Y) {
        if (!in(X, Y)) return true;
        char c = ch(X, Y);
        if (c == ' ') return true;
        Tile &t = T(X, Y);
        if (t.terrain == T_WATER || t.terrain == T_DEEP) return false;
        if (t.wall != W_NONE && t.wall != W_DOOR && t.wall != W_GATE) return false;
        if (c == 'h') return false;  // future houses
        return true;
    }
    bool streetLike(int X, int Y) {
        if (!in(X, Y)) return true;
        int tr = T(X, Y).terrain;
        return open(X, Y) && (tr == T_STREET || tr == T_SQUARE || tr == T_ROAD || tr == T_BRIDGE);
    }

    int newBuilding(const std::string &kind, const std::string &name, int x0, int y0, int x1, int y1) {
        Building b;
        b.kind = kind; b.name = name;
        b.x0 = x0; b.y0 = y0; b.x1 = x1; b.y1 = y1;
        b.ground = g;
        b.style = s.style;
        b.wallColor = pick(style().walls);
        b.roofColor = pick(style().roofs);
        b.trimColor = style().trim;
        w.buildings.push_back(b);
        return (int)w.buildings.size();
    }
    Building &B(int id) { return w.buildings[id - 1]; }

    // ---------------------------------------------------------- part helpers
    static Part box(vec3 a, vec3 b, vec3 c, bool upper = true) {
        Part p; p.kind = P_BOX; p.a = a; p.b = b; p.color = c; p.upper = upper; return p;
    }
    static Part roof(PartKind k, float x0, float y0, float x1, float y1, float ze, float zr, int axis,
                     vec3 c, vec3 c2) {
        Part p; p.kind = k; p.a = vec3(x0, y0, ze); p.b = vec3(x1, y1, zr); p.axis = axis;
        p.color = c; p.color2 = c2; return p;
    }
    static Part prism(float cx, float cy, float z0, float z1, float r, int sides, vec3 c, bool upper = true) {
        Part p; p.kind = P_PRISM; p.a = vec3(cx, cy, z0); p.b = vec3(r, 0, z1); p.sides = sides;
        p.color = c; p.upper = upper; return p;
    }
    static Part cone(float cx, float cy, float z0, float z1, float r, int sides, vec3 c) {
        Part p; p.kind = P_CONE; p.a = vec3(cx, cy, z0); p.b = vec3(r, 0, z1); p.sides = sides;
        p.color = c; return p;
    }
    static Part windows(float x0, float y0, float x1, float y1, float z0, float z1, float storey,
                        vec3 glass, vec3 frame, float spacing = 1.6f, float tall = 0.55f) {
        Part p; p.kind = P_WINDOWS; p.a = vec3(x0, y0, z0); p.b = vec3(x1, y1, z1);
        p.color = glass; p.color2 = frame; p.p1 = storey; p.p2 = tall; p.sides = (int)(spacing * 100);
        return p;
    }
    static Part crenel(float x0, float y0, float x1, float y1, float z, float h, vec3 c) {
        Part p; p.kind = P_CRENEL; p.a = vec3(x0, y0, z); p.b = vec3(x1, y1, z + h); p.color = c; return p;
    }

    // ---------------------------------------------------------- clusters
    std::vector<Cluster> clusters(std::function<bool(char)> pred) {
        std::vector<Cluster> out;
        std::vector<uint8_t> seen(s.w * s.h, 0);
        for (int row = 0; row < s.h; row++)
            for (int col = 0; col < s.w; col++) {
                char c = s.chars[row * s.w + col];
                if (!pred(c) || seen[row * s.w + col]) continue;
                Cluster cl;
                std::vector<std::pair<int, int>> stack{{col, row}};
                seen[row * s.w + col] = 1;
                while (!stack.empty()) {
                    auto [cc, rr] = stack.back(); stack.pop_back();
                    int X = s.x0 + cc, Y = s.y0 + s.h - 1 - rr;
                    cl.tiles.push_back({X, Y});
                    cl.x0 = std::min(cl.x0, X); cl.x1 = std::max(cl.x1, X);
                    cl.y0 = std::min(cl.y0, Y); cl.y1 = std::max(cl.y1, Y);
                    const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (auto &dd : d) {
                        int c2 = cc + dd[0], r2 = rr + dd[1];
                        if (c2 < 0 || r2 < 0 || c2 >= s.w || r2 >= s.h) continue;
                        if (seen[r2 * s.w + c2] || s.chars[r2 * s.w + c2] != c) continue;
                        seen[r2 * s.w + c2] = 1;
                        stack.push_back({c2, r2});
                    }
                }
                out.push_back(cl);
            }
        return out;
    }

    // ---------------------------------------------------------- base tiles
    void baseTiles() {
        for (int row = 0; row < s.h; row++)
            for (int col = 0; col < s.w; col++) {
                char c = s.chars[row * s.w + col];
                Tile t;
                t.var = (uint8_t)(hash2i(col, row, 3) & 0xff);
                t.terrain = T_YARD;
                switch (c) {
                    case '~': t.terrain = T_WATER; break;
                    case '=': t.terrain = T_BRIDGE; break;
                    case '.': t.terrain = T_STREET; break;
                    case '_': t.terrain = T_ROAD; break;
                    case ':': t.terrain = T_SQUARE; break;
                    case ',': t.terrain = T_GARDEN; break;
                    case ';': t.terrain = T_YARD; break;
                    case 't': {
                        t.terrain = T_GARDEN;
                        int r = t.var % 10;
                        t.feature = r < 5 ? F_OAK : (r < 8 ? F_FRUIT : F_POPLAR);
                        break;
                    }
                    case '#': t.wall = W_CITYWALL; break;
                    case 'O': t.wall = W_TOWER; break;
                    case 'o': t.terrain = T_SQUARE; t.feature = F_WELL; break;
                    case 'f': t.terrain = T_SQUARE; t.feature = F_FOUNTAIN; break;
                    case 'm': t.terrain = T_SQUARE; t.feature = F_STALL; break;
                    case '+': t.terrain = T_SQUARE; t.feature = F_CROSS; break;
                    case 'b': t.terrain = T_SQUARE; t.feature = F_BENCH; break;
                    case 's': t.terrain = T_SQUARE; t.feature = F_STATUE; t.var = 0; break;   // on horseback
                    case 'y': t.terrain = T_SQUARE; t.feature = F_STATUE; t.var = 1; break;   // a pedestal
                    case 'i': t.terrain = T_SQUARE; t.feature = F_OBELISK; break;
                    case 'j': t.terrain = T_SQUARE; t.feature = F_COLUMN; break;
                    case 'g': t.terrain = T_WATER; t.feature = F_MOORING; t.var = 1; break;   // a boat waits here
                    case 'p': t.terrain = T_WATER; t.feature = F_MOORING; t.var = 0; break;   // a post only
                    case 'x': t.terrain = T_YARD; t.feature = F_HAY; break;
                    case 'k': t.terrain = T_STREET; t.feature = F_BARREL; break;
                    case '%': t.terrain = T_GARDEN; t.wall = W_HEDGE; break;
                    case 'e': t.wall = W_EARTHWORK; break;                         // a rampart of earth
                    case 'n': t.feature = F_TENT; t.var = (uint8_t)(hash2i(col / 2, row / 2, 5) & 1); break;   // a tent
                    case 'v': t.feature = F_GABION; t.var = (uint8_t)(hash2i(col, row, 6) & 1); break;   // gabions
                    case 'u': {                                                    // a gun, pointing away from the middle
                        int dc = col - s.w / 2, dr = row - s.h / 2;
                        t.feature = F_CANNON;
                        t.var = std::abs(dc) * s.h >= std::abs(dr) * s.w ? (dc < 0 ? 0 : 1) : (dr > 0 ? 2 : 3);
                        break;
                    }
                    default:
                        if (c >= '1' && c <= '9') { t.terrain = T_STREET; t.wall = W_GATE; }
                        break;
                }
                if (!s.campEvent.empty() && (c == 'n' || c == 'v' || c == 'u')) {   // pitched later (People::pitchCamps)
                    s.campSpots.push_back({s.x0 + col, s.y0 + s.h - 1 - row, t.feature, t.var});
                    t.feature = F_NONE;
                }
                s.tiles[row * s.w + col] = t;
            }
    }

    void waterExits() {
        // runs of water along the four edges of the map
        auto scan = [&](int n, std::function<std::pair<int, int>(int)> at) {
            int runStart = -1;
            for (int k = 0; k <= n; k++) {
                bool wet = false;
                if (k < n) {
                    auto [X, Y] = at(k);
                    wet = (ch(X, Y) == '~');
                }
                if (wet && runStart < 0) runStart = k;
                if (!wet && runStart >= 0) {
                    s.waterExits.push_back(at((runStart + k - 1) / 2));
                    runStart = -1;
                }
            }
        };
        scan(s.w, [&](int k) { return std::make_pair(s.x0 + k, s.y0); });
        scan(s.w, [&](int k) { return std::make_pair(s.x0 + k, s.y0 + s.h - 1); });
        scan(s.h, [&](int k) { return std::make_pair(s.x0, s.y0 + k); });
        scan(s.h, [&](int k) { return std::make_pair(s.x0 + s.w - 1, s.y0 + k); });
    }

    // ---------------------------------------------------------- wall towers and gates
    void wallTowers() {
        for (auto &cl : clusters([](char c) { return c == 'O'; })) {
            int id = newBuilding("tower", "", cl.x0, cl.y0, cl.x1, cl.y1);
            Building &b = B(id);
            float cx = (cl.x0 + cl.x1 + 1) * 0.5f, cy = (cl.y0 + cl.y1 + 1) * 0.5f;
            float r = std::max(cl.x1 - cl.x0 + 1, cl.y1 - cl.y0 + 1) * 0.5f + 0.35f;
            vec3 c = STONE;
            float top = g + TOWNWALL_H + 1.4f;
            b.parts.push_back(prism(cx, cy, g - 0.6f, g + GF_H, r, 12, c, false));
            b.parts.push_back(prism(cx, cy, g + GF_H, top, r, 12, c));
            if (towerRoof > 0) {
                b.parts.push_back(prism(cx, cy, top, top + 0.2f, r + 0.15f, 12, c * 0.9f));
                b.parts.push_back(cone(cx, cy, top + 0.2f, top + 0.2f + 2.0f * r, r + 0.15f, 12, SLATE));
            } else {
                b.parts.push_back(crenel(cx - r, cy - r, cx + r, cy + r, top, 0.5f, c));
            }
            for (auto &p : cl.tiles) T(p.first, p.second).bld = (uint16_t)id;
        }
    }

    // follow the road ('_') leaving a gate to where it leaves the map
    vec2 roadHead(const Cluster &cl) {
        std::vector<std::pair<int, int>> frontier;
        std::map<std::pair<int, int>, int> dist;
        for (auto &p : cl.tiles) {
            const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto &dd : d) {
                int X = p.first + dd[0], Y = p.second + dd[1];
                if (in(X, Y) && ch(X, Y) == '_' && !dist.count({X, Y})) {
                    dist[{X, Y}] = 0;
                    frontier.push_back({X, Y});
                }
            }
        }
        std::pair<int, int> best{-1, -1};
        int bestD = -1;
        size_t qi = 0;
        while (qi < frontier.size()) {
            auto p = frontier[qi++];
            int dp = dist[p];
            bool edge = p.first == s.x0 || p.second == s.y0 || p.first == s.x0 + s.w - 1 ||
                        p.second == s.y0 + s.h - 1;
            if (edge && dp > bestD) { bestD = dp; best = p; }
            const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto &dd : d) {
                int X = p.first + dd[0], Y = p.second + dd[1];
                if (in(X, Y) && ch(X, Y) == '_' && !dist.count({X, Y})) {
                    dist[{X, Y}] = dp + 1;
                    frontier.push_back({X, Y});
                }
            }
        }
        if (bestD < 0) return vec2((cl.x0 + cl.x1 + 1) * 0.5f, (cl.y0 + cl.y1 + 1) * 0.5f);
        vec2 p(best.first + 0.5f, best.second + 0.5f);
        if (best.first == s.x0) p.x -= 1.5f;
        if (best.first == s.x0 + s.w - 1) p.x += 1.5f;
        if (best.second == s.y0) p.y -= 1.5f;
        if (best.second == s.y0 + s.h - 1) p.y += 1.5f;
        return p;
    }

    void gates(const std::map<char, std::string> &names) {
        for (auto &cl : clusters([](char c) { return c >= '1' && c <= '9'; })) {
            char c = ch(cl.tiles[0].first, cl.tiles[0].second);
            std::string nm = names.count(c) ? names.at(c) : "";
            auto isWall = [&](int X, int Y) { char d = ch(X, Y); return d == '#' || d == 'O'; };
            bool ew = isWall(cl.x0 - 1, cl.y0) || isWall(cl.x1 + 1, cl.y0);
            int bx0 = cl.x0, bx1 = cl.x1, by0 = cl.y0, by1 = cl.y1;
            if (ew) { bx0 -= 2; bx1 += 2; } else { by0 -= 2; by1 += 2; }
            int id = newBuilding("gate", nm, bx0, by0, bx1, by1);
            Building &b = B(id);
            for (int X = bx0; X <= bx1; X++)
                for (int Y = by0; Y <= by1; Y++)
                    if (in(X, Y)) {
                        T(X, Y).bld = (uint16_t)id;
                        if (T(X, Y).wall == W_CITYWALL) T(X, Y).wall = W_TOWER;
                    }
            float x0 = bx0, x1 = bx1 + 1, y0 = by0, y1 = by1 + 1;
            float ins = 0.3f;
            if (ew) { y0 -= ins; y1 += ins; } else { x0 -= ins; x1 += ins; }
            float za = g + DOOR_H + 1.3f, zt = g + TOWNWALL_H + 1.2f;
            b.parts.push_back(box(vec3(x0, y0, za), vec3(x1, y1, zt), STONE));
            b.parts.push_back(roof(P_HIP, x0 - 0.15f, y0 - 0.15f, x1 + 0.15f, y1 + 0.15f, zt, zt + 2.4f, ew ? 0 : 1, SLATE, STONE));
            auto tower = [&](float cx, float cy) {
                b.parts.push_back(prism(cx, cy, g - 0.5f, g + GF_H, 1.35f, 10, STONE, false));
                b.parts.push_back(prism(cx, cy, g + GF_H, zt + 1.0f, 1.35f, 10, STONE));
                b.parts.push_back(cone(cx, cy, zt + 1.0f, zt + 3.8f, 1.5f, 10, SLATE));
            };
            if (ew) { tower(bx0 + 1.0f, (by0 + by1 + 1) * 0.5f); tower(bx1, (by0 + by1 + 1) * 0.5f); }
            else { tower((bx0 + bx1 + 1) * 0.5f, by0 + 1.0f); tower((bx0 + bx1 + 1) * 0.5f, by1); }
            g_gates[s.name + "." + std::string(1, c)] = roadHead(cl);
        }
    }

    // ---------------------------------------------------------- generic pieces
    void setFloor(int id, int x0, int y0, int x1, int y1, uint8_t floor) {
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++) {
                Tile &t = T(X, Y);
                t.terrain = floor; t.wall = W_NONE; t.feature = F_NONE; t.bld = (uint16_t)id;
            }
    }
    void ringWalls(int id, int x0, int y0, int x1, int y1, int windowEvery) {
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++) {
                bool per = X == x0 || X == x1 || Y == y0 || Y == y1;
                if (!per) continue;
                bool corner = (X == x0 || X == x1) && (Y == y0 || Y == y1);
                Tile &t = T(X, Y);
                t.bld = (uint16_t)id;
                t.feature = F_NONE;
                int k = (X - x0) + (Y - y0);
                t.wall = (!corner && windowEvery > 0 && k % windowEvery == 1) ? W_WINDOW : W_WALL;
            }
    }
    // put a door in the wall of a rectangle, on a side facing a street if possible.
    // side: -1 any, 0 west, 1 east, 2 south, 3 north
    // a second way in and out of an inn, on the side away from its street door, so that a crowd
    // at supper cannot shut you in. No random numbers, so
    // the rest of the town is unchanged.
    void backDoor(int x0, int y0, int x1, int y1) {
        int fx = 0, fy = 0, n = 0;   // where the street door (or doors) are
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++)
                if ((X == x0 || X == x1 || Y == y0 || Y == y1) && T(X, Y).wall == W_DOOR) { fx += X; fy += Y; n++; }
        float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
        float ddx = n ? fx / (float)n - cx : 0, ddy = n ? fy / (float)n - cy : 0;
        // the sides in order of preference: the one opposite the door, then the two beside it
        int opp = std::fabs(ddx) >= std::fabs(ddy) ? (ddx < 0 ? 1 : 0) : (ddy < 0 ? 3 : 2);   // 0 west 1 east 2 south 3 north
        int order[4] = {opp, opp < 2 ? 2 : 0, opp < 2 ? 3 : 1, opp ^ 1};
        for (int k = 0; k < 3; k++) {
            int sd = order[k];
            int len = sd < 2 ? y1 - y0 - 1 : x1 - x0 - 1;
            for (int j = 0; j < len; j++) {   // from the middle of that wall outwards
                int off = (j % 2 ? 1 : -1) * ((j + 1) / 2);
                int X = sd == 0 ? x0 : sd == 1 ? x1 : (x0 + x1) / 2 + off;
                int Y = sd == 2 ? y0 : sd == 3 ? y1 : (y0 + y1) / 2 + off;
                if (X <= x0 && sd >= 2) continue;
                if (X >= x1 && sd >= 2) continue;
                if (Y <= y0 && sd < 2) continue;
                if (Y >= y1 && sd < 2) continue;
                int ox = X + (sd == 0 ? -1 : sd == 1 ? 1 : 0), oy = Y + (sd == 2 ? -1 : sd == 3 ? 1 : 0);
                int ix = X - (ox - X), iy = Y - (oy - Y);
                if (T(X, Y).wall != W_WALL && T(X, Y).wall != W_WINDOW) continue;
                if (!open(ox, oy) || !in(ox, oy)) continue;
                const Tile &it = T(ix, iy);
                if (it.wall != W_NONE || it.feature != F_NONE) continue;
                T(X, Y).wall = W_DOOR;
                return;
            }
        }
    }

    bool door(int x0, int y0, int x1, int y1, int side = -1, bool centred = false) {
        struct Cand { int X, Y, score; };
        std::vector<Cand> cands;
        auto consider = [&](int X, int Y, int ox, int oy, int sd) {
            if (side >= 0 && sd != side) return;
            if (!open(ox, oy)) return;
            // not right beside a corner
            if (sd < 2 && (Y == y0 + 1 || Y == y1 - 1) && y1 - y0 > 4) return;
            if (sd >= 2 && (X == x0 + 1 || X == x1 - 1) && x1 - x0 > 4) return;
            int sc = streetLike(ox, oy) ? 10 : 3;
            if (centred) {
                int mid = (sd < 2) ? (y0 + y1) / 2 : (x0 + x1) / 2;
                int pos = (sd < 2) ? Y : X;
                sc += 5 - std::min(5, std::abs(pos - mid));
            }
            sc += rng.range(0, 2);
            cands.push_back({X, Y, sc});
        };
        for (int Y = y0 + 1; Y < y1; Y++) { consider(x0, Y, x0 - 1, Y, 0); consider(x1, Y, x1 + 1, Y, 1); }
        for (int X = x0 + 1; X < x1; X++) { consider(X, y0, X, y0 - 1, 2); consider(X, y1, X, y1 + 1, 3); }
        if (cands.empty()) return false;
        auto best = std::max_element(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) { return a.score < b.score; });
        T(best->X, best->Y).wall = W_DOOR;
        return true;
    }

    // every floor tile of the rectangle reachable from its outside doors?
    bool roomConnected(int x0, int y0, int x1, int y1) {
        std::vector<std::pair<int, int>> st;
        std::map<std::pair<int, int>, bool> seen;
        int need = 0;
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++) {
                Tile &t = T(X, Y);
                bool per = X == x0 || X == x1 || Y == y0 || Y == y1;
                if (t.wall == W_NONE && !blocksRoom(t.feature)) need++;
                if (per && t.wall == W_DOOR) { st.push_back({X, Y}); seen[{X, Y}] = true; }
            }
        int got = 0;
        for (auto &e : entries)
            if (e.first >= x0 && e.first <= x1 && e.second >= y0 && e.second <= y1 && !seen.count(e)) {
                st.push_back(e);
                seen[e] = true;
                Tile &t = T(e.first, e.second);
                if (t.wall == W_NONE && !blocksRoom(t.feature)) got++;
            }
        while (!st.empty()) {
            auto p = st.back(); st.pop_back();
            const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto &dd : d) {
                int X = p.first + dd[0], Y = p.second + dd[1];
                if (X < x0 || X > x1 || Y < y0 || Y > y1 || seen.count({X, Y})) continue;
                Tile &t = T(X, Y);
                if (t.wall != W_NONE && t.wall != W_DOOR) continue;
                if (blocksRoom(t.feature)) continue;
                seen[{X, Y}] = true;
                if (t.wall == W_NONE) got++;
                st.push_back({X, Y});
            }
        }
        return got >= need;
    }

    bool nearDoor(int X, int Y) {
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
                if (in(X + dx, Y + dy) && T(X + dx, Y + dy).wall == W_DOOR) return true;
        return false;
    }

    // place a piece of furniture, unless it would block the way
    int wantBeds[2] = {0, 0};   // at least so many beds on the ground floor and upstairs (the player's home)
    void ensureBeds(int x0, int y0, int x1, int y1, int n) {   // more beds along the inner walls, if there are too few
        int have = 0;
        for (int X = x0 + 1; X < x1; X++) for (int Y = y0 + 1; Y < y1; Y++) if (T(X, Y).feature == F_BED) have++;
        for (int X = x0 + 1; X < x1 && have < n; X++)
            for (int Y = y0 + 1; Y < y1 && have < n; Y++) {
                bool byWall = X == x0 + 1 || X == x1 - 1 || Y == y0 + 1 || Y == y1 - 1;
                if (byWall && put(x0, y0, x1, y1, X, Y, F_BED, 0)) have++;
            }
    }
    bool put(int bx0, int by0, int bx1, int by1, int X, int Y, uint8_t f, uint8_t var) {
        Tile &t = T(X, Y);
        if (t.wall != W_NONE || t.feature != F_NONE || nearDoor(X, Y) || reserved.count({X, Y})) return false;
        t.feature = f;
        t.var = var;
        if (blocksRoom(f) && !roomConnected(bx0, by0, bx1, by1)) { t.feature = F_NONE; return false; }
        return true;
    }

    // furnish one room (interior tiles rx0..rx1, ry0..ry1) of the house bx0..by1
    void furnishRoom(int bx0, int by0, int bx1, int by1, int rx0, int ry0, int rx1, int ry1, int kind) {
        int rw = rx1 - rx0 + 1, rh = ry1 - ry0 + 1;
        if (rw < 1 || rh < 1) return;
        auto alongWall = [&](int &X, int &Y) {
            int side = rng.range(0, 3);
            if (side == 0) { X = rx0; Y = rng.range(ry0, ry1); }
            else if (side == 1) { X = rx1; Y = rng.range(ry0, ry1); }
            else if (side == 2) { X = rng.range(rx0, rx1); Y = ry0; }
            else { X = rng.range(rx0, rx1); Y = ry1; }
            return side;
        };
        if (kind == 0 || kind == 2) {
            // hearth against the north or west wall
            for (int k = 0; k < 6; k++) {
                bool north = rng.chance(0.5f);
                int X = north ? rng.range(rx0, rx1) : rx0, Y = north ? ry1 : rng.range(ry0, ry1);
                if (put(bx0, by0, bx1, by1, X, Y, F_HEARTH, north ? 3 : 0)) break;
            }
            if (rw >= 3 && rh >= 3) {
                int cx = (rx0 + rx1) / 2, cy = (ry0 + ry1) / 2;
                if (put(bx0, by0, bx1, by1, cx, cy, F_TABLE, 0)) {
                    put(bx0, by0, bx1, by1, cx, cy - 1, F_BENCH, 0);
                    put(bx0, by0, bx1, by1, cx, cy + 1, F_BENCH, 0);
                }
            }
            for (int k = 0, n = rw * rh / 10; k < n; k++) {
                int X, Y;
                alongWall(X, Y);
                put(bx0, by0, bx1, by1, X, Y, rng.chance(0.5f) ? F_BARREL : F_CHEST, 0);
            }
        }
        if (kind == 1 || kind == 2) {
            int beds = kind == 2 ? 1 : std::max(1, std::min(3, rw * rh / 8));
            for (int k = 0, placed = 0; k < 12 && placed < beds; k++) {
                int X, Y;
                alongWall(X, Y);
                if (put(bx0, by0, bx1, by1, X, Y, F_BED, (uint8_t)rng.range(0, 3))) placed++;
            }
            for (int k = 0; k < 4; k++) {
                int X, Y;
                alongWall(X, Y);
                if (put(bx0, by0, bx1, by1, X, Y, F_CHEST, 0)) break;
            }
        }
    }

    // split a house into a front room and a back room, and furnish both
    void rooms(int x0, int y0, int x1, int y1, int kFront = 0, int kBack = 1, int kSingle = 2) {
        int ix0 = x0 + 1, iy0 = y0 + 1, ix1 = x1 - 1, iy1 = y1 - 1;
        int iw = ix1 - ix0 + 1, ih = iy1 - iy0 + 1;
        if (iw < 1 || ih < 1) return;
        if (std::max(iw, ih) >= 6 && std::min(iw, ih) >= 3) {
            bool cutX = iw >= ih;   // the partition is a line of constant X
            for (int attempt = 0; attempt < 5; attempt++) {
                int p = cutX ? ix0 + (int)(iw * 0.45f) + rng.range(-1, 1) : iy0 + (int)(ih * 0.45f) + rng.range(-1, 1);
                if (cutX && (p <= ix0 + 1 || p >= ix1 - 1)) continue;
                if (!cutX && (p <= iy0 + 1 || p >= iy1 - 1)) continue;
                bool bad = cutX ? (T(p, y0).wall == W_DOOR || T(p, y1).wall == W_DOOR || T(p - 1, y0).wall == W_DOOR ||
                                   T(p + 1, y0).wall == W_DOOR || T(p - 1, y1).wall == W_DOOR || T(p + 1, y1).wall == W_DOOR)
                                : (T(x0, p).wall == W_DOOR || T(x1, p).wall == W_DOOR || T(x0, p - 1).wall == W_DOOR ||
                                   T(x0, p + 1).wall == W_DOOR || T(x1, p - 1).wall == W_DOOR || T(x1, p + 1).wall == W_DOOR);
                int n = cutX ? ih : iw;
                for (int k = 0; k < n && !bad; k++) {
                    int X = cutX ? p : ix0 + k, Y = cutX ? iy0 + k : p;
                    if (T(X, Y).feature != F_NONE || reserved.count({X, Y})) bad = true;
                }
                if (bad) continue;
                int dpos = rng.range(0, n - 1);
                // not a door with a ladder, a trapdoor or a bed against it on either side (the Pod
                // Baranami's inner door opened on to its cellar hatch, and sealed the back room):
                // the nearest clear place along the partition, else another partition
                auto clear = [&](int k) {
                    int X = cutX ? p : ix0 + k, Y = cutX ? iy0 + k : p;
                    int ax = cutX ? 1 : 0, ay = cutX ? 0 : 1;
                    return !blocksRoom(T(X - ax, Y - ay).feature) && !blocksRoom(T(X + ax, Y + ay).feature);
                };
                if (!clear(dpos)) {
                    int found = -1;
                    for (int r = 1; r < n && found < 0; r++)
                        for (int k : {dpos - r, dpos + r})
                            if (found < 0 && k >= 0 && k < n && clear(k)) found = k;
                    if (found < 0) continue;
                    dpos = found;
                }
                for (int k = 0; k < n; k++) {
                    int X = cutX ? p : ix0 + k, Y = cutX ? iy0 + k : p;
                    T(X, Y).wall = (k == dpos) ? W_DOOR : W_WALL;
                }
                // which side has the street door?
                bool frontLow = false;
                for (int X = x0; X <= x1; X++)
                    for (int Y = y0; Y <= y1; Y++)
                        if ((X == x0 || X == x1 || Y == y0 || Y == y1) && T(X, Y).wall == W_DOOR)
                            frontLow = cutX ? X < p : Y < p;
                if (cutX) {
                    furnishRoom(x0, y0, x1, y1, ix0, iy0, p - 1, iy1, frontLow ? kFront : kBack);
                    furnishRoom(x0, y0, x1, y1, p + 1, iy0, ix1, iy1, frontLow ? kBack : kFront);
                } else {
                    furnishRoom(x0, y0, x1, y1, ix0, iy0, ix1, p - 1, frontLow ? kFront : kBack);
                    furnishRoom(x0, y0, x1, y1, ix0, p + 1, ix1, iy1, frontLow ? kBack : kFront);
                }
                return;
            }
        }
        furnishRoom(x0, y0, x1, y1, ix0, iy0, ix1, iy1, kSingle);
    }

    // a place for a ladder against the inside of a wall, away from the doors, with a free
    // tile in front of it.  side: the wall it leans on (0 west, 1 east, 2 south, 3 north)
    bool ladderSpot(int x0, int y0, int x1, int y1, int &lx, int &ly, int &fx, int &fy, int &side,
                    int avoidX = -1000, int avoidY = -1000) {
        std::vector<std::pair<int, int>> doors;
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++)
                if (T(X, Y).wall == W_DOOR) doors.push_back({X, Y});
        const int d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        float best = -1;
        for (int X = x0 + 1; X < x1; X++)
            for (int Y = y0 + 1; Y < y1; Y++) {
                Tile &t = T(X, Y);
                if (t.wall != W_NONE || t.feature != F_NONE || reserved.count({X, Y}) || nearDoor(X, Y)) continue;
                if (std::abs(X - avoidX) + std::abs(Y - avoidY) < 3) continue;
                for (int sd = 0; sd < 4; sd++) {
                    int wx = X + d4[sd][0], wy = Y + d4[sd][1];
                    bool perim = wx == x0 || wx == x1 || wy == y0 || wy == y1;
                    Tile &wt = T(wx, wy);
                    if (!perim || wt.wall == W_NONE || wt.wall == W_DOOR) continue;
                    int qx = X - d4[sd][0], qy = Y - d4[sd][1];
                    if (qx <= x0 || qx >= x1 || qy <= y0 || qy >= y1) continue;
                    Tile &q = T(qx, qy);
                    if (q.wall != W_NONE || q.feature != F_NONE || reserved.count({qx, qy})) continue;
                    float score = 0;
                    for (auto &d : doors) score += std::abs(d.first - X) + std::abs(d.second - Y);
                    score = (doors.empty() ? 10 : score / doors.size()) + rng.uni();
                    if (score > best) { best = score; lx = X; ly = Y; fx = qx; fy = qy; side = sd; }
                }
            }
        return best >= 0;
    }

    // a floor plan over (or under) the rectangle: walls round the edge, floor inside
    Level makeFloor(const std::string &name, int id, int z, int x0, int y0, int x1, int y1, uint8_t floor, int windowEvery) {
        Level L;
        L.name = name; L.bld = id; L.z = z;
        L.init(x0, y0, x1, y1);
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++) {
                Tile &t = L.at(X, Y);
                bool per = X == x0 || X == x1 || Y == y0 || Y == y1;
                bool corner = (X == x0 || X == x1) && (Y == y0 || Y == y1);
                t.terrain = floor;
                t.bld = (uint16_t)id;
                t.var = (uint8_t)(hash2i(X, Y, 4) & 0xff);
                t.wall = per ? ((!corner && windowEvery > 0 && (X + Y) % windowEvery == 0) ? W_WINDOW : W_WALL) : W_NONE;
            }
        return L;
    }

    // stairs at (sx,sy) on the ground, in front of which one stands at (fx,fy), up to stairs at
    // (tx,ty) on floor L, in front of which one arrives at (tfx,tfy)
    // where a stair's foot goes in a walled box (x0..x1, y0..y1 the walls): on a free tile that is
    // not a doorway's inner tile, as far from the doors as it can be, with a free tile beside it
    // to step off on to (Carfax and the White Tower once had their stairs just inside the door,
    // so that you could not leave without climbing)
    void stairSpot(int x0, int y0, int x1, int y1, int &sx, int &sy, int &fx, int &fy) {
        sx = x0 + 1; sy = y0 + 1; fx = x0 + 2; fy = y0 + 1;
        std::vector<std::pair<int, int>> doors;
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++)
                if (T(X, Y).wall == W_DOOR) doors.push_back({X, Y});
        auto freeAt = [&](int X, int Y) { return X > x0 && X < x1 && Y > y0 && Y < y1 && T(X, Y).wall == W_NONE && T(X, Y).feature == F_NONE; };
        int best = -1;
        for (int X = x0 + 1; X < x1; X++)
            for (int Y = y0 + 1; Y < y1; Y++) {
                if (!freeAt(X, Y)) continue;
                int d = 1000;
                for (auto &dr : doors) d = std::min(d, std::abs(dr.first - X) + std::abs(dr.second - Y));
                if (doors.empty()) d = 0;
                else if (d <= 1) continue;   // the doorway's inner tile
                const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto &q : d4) {
                    if (!freeAt(X + q[0], Y + q[1])) continue;
                    if (d > best) { best = d; sx = X; sy = Y; fx = X + q[0]; fy = Y + q[1]; }
                    break;
                }
            }
    }

    void linkUp(Level &L, int sx, int sy, int fx, int fy, int tx, int ty, int tfx, int tfy, uint8_t below, uint8_t above,
                const std::string &up, const std::string &down) {
        Tile &g0 = s.tileAt(sx, sy);
        g0.feature = below;
        if (s.tileAt(fx, fy).wall == W_NONE) s.tileAt(fx, fy).feature = F_NONE;
        L.at(tx, ty).feature = above;
        L.at(tx, ty).wall = W_NONE;
        if (L.at(tx, ty).terrain == T_VOID) L.at(tx, ty).terrain = T_FLOOR_STONE;
        L.at(tfx, tfy).feature = F_NONE;
        L.at(tfx, tfy).wall = W_NONE;
        if (L.at(tfx, tfy).terrain == T_VOID) L.at(tfx, tfy).terrain = T_FLOOR_STONE;
        w.addLink(sx, sy, 0, tfx, tfy, L.z, up);
        w.addLink(tx, ty, L.z, fx, fy, 0, down);
    }

    // a tower top: parapet round the edge, stone floor inside
    Level towerTop(const std::string &name, int id, int x0, int y0, int x1, int y1, float floor, vec3 stone) {
        Level L = makeFloor(name, id, 1, x0, y0, x1, y1, T_FLOOR_STONE, 0);
        L.floor = floor;
        L.wallH = 0.6f;
        L.drawFloor = false;
        L.drawWalls = false;
        L.hideBuilding = false;
        L.wallColor = stone;
        // the parapet is drawn by the building, at (or just outside) the edge of the tower: the
        // whole top is floor, and the edge of the level stops you (no ring of
        // walls inside the parapet, which would make an invisible wall)
        for (Tile &t : L.tiles) t.wall = W_NONE;
        L.parapet = true;
        return L;
    }

    // stairs from (sx,sy) on the ground (front tile fx,fy) to the same place on floor L
    void stairsTo(Level &L, int sx, int sy, int fx, int fy, uint8_t feature, const std::string &up, const std::string &down) {
        L.at(sx, sy).feature = feature;
        L.at(fx, fy).feature = F_NONE;
        if (L.at(fx, fy).wall != W_NONE) L.at(fx, fy).wall = W_NONE;
        w.addLink(sx, sy, 0, fx, fy, L.z, up);
        w.addLink(sx, sy, L.z, fx, fy, 0, down);
    }

    void roofFor(Building &b, const Style &st, float x0, float y0, float x1, float y1, float ze, int forceAxis = -1) {
        float wx = x1 - x0, wy = y1 - y0;
        int axis = forceAxis >= 0 ? forceAxis : (wx >= wy ? 0 : 1);
        float half = 0.5f * (axis == 0 ? wy : wx);
        float o = 0.3f;  // eaves overhang
        const std::string &r = st.roofTypes.empty() ? st.roof : st.roofTypes[rng.next() % st.roofTypes.size()];
        if (r == "mansard") {
            Part p = roof(P_MANSARD, x0 - 0.08f, y0 - 0.08f, x1 + 0.08f, y1 + 0.08f, ze, ze + st.roofPitch * half * 1.2f,
                          axis, b.roofColor, b.trimColor);
            p.p1 = st.dormers ? 1 : 0;
            b.parts.push_back(p);
        } else if (r == "hip") {
            b.parts.push_back(roof(P_HIP, x0 - o, y0 - o, x1 + o, y1 + o, ze, ze + st.roofPitch * half, axis, b.roofColor, b.wallColor));
        } else if (r == "flat") {
            b.parts.push_back(box(vec3(x0 - 0.15f, y0 - 0.15f, ze), vec3(x1 + 0.15f, y1 + 0.15f, ze + 0.3f), b.trimColor));
            b.parts.push_back(box(vec3(x0 + 0.1f, y0 + 0.1f, ze + 0.3f), vec3(x1 - 0.1f, y1 - 0.1f, ze + 0.4f), b.roofColor));
            if (st.balustrade) {
                Part p; p.kind = P_BALUSTRADE; p.a = vec3(x0 - 0.1f, y0 - 0.1f, ze + 0.3f); p.b = vec3(x1 + 0.1f, y1 + 0.1f, ze + 1.0f);
                p.color = b.trimColor;
                b.parts.push_back(p);
            }
            return;
        } else {
            Part p = roof(P_GABLE, x0 - o, y0 - o, x1 + o, y1 + o, ze, ze + st.roofPitch * half, axis, b.roofColor, b.wallColor);
            if (axis == 0) { p.a.x = x0 - 0.08f; p.b.x = x1 + 0.08f; } else { p.a.y = y0 - 0.08f; p.b.y = y1 + 0.08f; }
            b.parts.push_back(p);
            if (st.stepGable) {
                // stepped gable ends, rising a little above the roof, with a finial on top
                float H = st.roofPitch * half;
                const int n = 4;
                for (int end = 0; end < 2; end++)
                    for (int k = 0; k < n; k++) {
                        float hw = half * (1.0f - (float)k / n) + 0.05f, z0 = ze + H * k / n, z1 = ze + H * (k + 1) / n + 0.3f;
                        vec3 lo, hi;
                        if (axis == 0) {
                            float xe = end == 0 ? x0 - 0.1f : x1 - 0.22f;
                            float cy2 = (y0 + y1) * 0.5f;
                            lo = vec3(xe, cy2 - hw, z0); hi = vec3(xe + 0.32f, cy2 + hw, z1);
                        } else {
                            float ye = end == 0 ? y0 - 0.1f : y1 - 0.22f;
                            float cx2 = (x0 + x1) * 0.5f;
                            lo = vec3(cx2 - hw, ye, z0); hi = vec3(cx2 + hw, ye + 0.32f, z1);
                        }
                        b.parts.push_back(box(lo, hi, b.wallColor * (k == n - 1 ? 0.95f : 1.0f)));
                        if (k == n - 1) b.parts.push_back(box(vec3((lo.x + hi.x) * 0.5f - 0.08f, (lo.y + hi.y) * 0.5f - 0.08f, z1),
                                                          vec3((lo.x + hi.x) * 0.5f + 0.08f, (lo.y + hi.y) * 0.5f + 0.08f, z1 + 0.35f), b.trimColor));
                    }
            }
        }
        // chimneys
        int nch = (wx * wy >= 40) ? 2 : 1;
        for (int k = 0; k < nch; k++) {
            if (!rng.chance(0.85f)) continue;
            float t = nch == 2 ? (k == 0 ? 0.22f : 0.78f) : (rng.chance(0.5f) ? 0.22f : 0.78f);
            float cx = axis == 0 ? x0 + wx * t : (x0 + x1) * 0.5f + 0.5f;
            float cy = axis == 1 ? y0 + wy * t : (y0 + y1) * 0.5f + 0.5f;
            float top = ze + st.roofPitch * half + st.chimneyTall;
            vec3 cc = st.chimney.x + st.chimney.y + st.chimney.z > 0 ? st.chimney : b.wallColor * 0.85f;
            b.parts.push_back(box(vec3(cx - 0.3f, cy - 0.3f, ze), vec3(cx + 0.3f, cy + 0.3f, top), cc));
            if (st.funnel) {   // the Venetian funnel: wide at the top, to catch the sparks
                b.parts.push_back(box(vec3(cx - 0.38f, cy - 0.38f, top - 0.2f), vec3(cx + 0.38f, cy + 0.38f, top + 0.15f), cc * 0.95f));
                b.parts.push_back(box(vec3(cx - 0.5f, cy - 0.5f, top + 0.15f), vec3(cx + 0.5f, cy + 0.5f, top + 0.5f), cc * 0.9f));
            }
            if (st.chimneyTall > 1.0f) {   // a cluster of stacks with caps
                b.parts.push_back(box(vec3(cx - 0.34f, cy - 0.34f, top - 0.15f), vec3(cx + 0.34f, cy + 0.34f, top), cc * 0.8f));
                b.parts.push_back(box(vec3(cx + 0.3f, cy - 0.25f, ze), vec3(cx + 0.75f, cy + 0.25f, top - 0.3f), cc * 0.95f));
            }
        }
    }

    // ordinary house on rectangle (tile coords, inclusive)
    void house(int x0, int y0, int x1, int y1, int styleIdx = -1, const std::string &name = "", int minSt = 0, int maxSt = 0,
               bool cellarForce = false) {
        const Style &st = w.styles[styleIdx >= 0 ? styleIdx : s.style];
        int id = newBuilding("house", name, x0, y0, x1, y1);
        Building &b = B(id);
        b.style = styleIdx >= 0 ? styleIdx : s.style;
        b.wallColor = pick(st.walls);
        b.roofColor = pick(st.roofs);
        b.trimColor = st.trim;
        setFloor(id, x0, y0, x1, y1, T_FLOOR_WOOD);
        ringWalls(id, x0, y0, x1, y1, 0);
        // windows where the wall faces open ground
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++) {
                bool per = X == x0 || X == x1 || Y == y0 || Y == y1;
                bool corner = (X == x0 || X == x1) && (Y == y0 || Y == y1);
                if (!per || corner) continue;
                int ox = X == x0 ? X - 1 : (X == x1 ? X + 1 : X);
                int oy = Y == y0 ? Y - 1 : (Y == y1 ? Y + 1 : Y);
                if (open(ox, oy) && ((X + Y) % 2 == 0)) T(X, Y).wall = W_WINDOW;
            }
        door(x0, y0, x1, y1);
        // which side is the street door on? (0 west, 1 east, 2 south, 3 north)
        int doorSide = -1;
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++)
                if (T(X, Y).wall == W_DOOR && (X == x0 || X == x1 || Y == y0 || Y == y1))
                    doorSide = X == x0 ? 0 : X == x1 ? 1 : Y == y0 ? 2 : 3;
        int lo = minSt ? minSt : st.minStoreys, hi = maxSt ? maxSt : st.maxStoreys;
        int n = rng.range(lo, hi);
        bool timber = rng.chance(st.timberChance);
        if (n > 1 && timber) b.wallColor = vec3(0.93f, 0.9f, 0.82f) * (0.95f + 0.05f * rng.uni());
        // a ladder to the upper floor, and a trapdoor to the cellar
        reserved.clear(); entries.clear();
        bool big = x1 - x0 >= 4 && y1 - y0 >= 4;
        int ux = 0, uy = 0, ufx = 0, ufy = 0, us = 0, cx = 0, cy = 0, cfx = 0, cfy = 0, cs = 0;
        bool hasUp = n >= 2 && big && ladderSpot(x0, y0, x1, y1, ux, uy, ufx, ufy, us);
        if (hasUp) { T(ux, uy).feature = F_LADDER; T(ux, uy).var = (uint8_t)us; reserved.insert({ufx, ufy}); }
        bool hasCellar = (cellarForce || rng.chance(0.3f)) && big && ladderSpot(x0, y0, x1, y1, cx, cy, cfx, cfy, cs, ux, uy);
        if (hasCellar) { T(cx, cy).feature = F_HATCH; T(cx, cy).var = (uint8_t)cs; reserved.insert({cfx, cfy}); }
        rooms(x0, y0, x1, y1);
        if (wantBeds[0]) ensureBeds(x0, y0, x1, y1, wantBeds[0]);
        if (hasUp) {
            Level L = makeFloor("the upper floor", id, 1, x0, y0, x1, y1, T_FLOOR_WOOD, 2);
            L.floor = g + GF_H;
            L.wallH = st.storey;
            L.wallColor = b.wallColor;
            cur = &L;
            reserved = {{ufx, ufy}};
            entries = {{ufx, ufy}};
            stairsTo(L, ux, uy, ufx, ufy, F_HATCH, "You climb the ladder to the upper floor.", "You climb down the ladder.");
            T(ux, uy).var = (uint8_t)us;
            rooms(x0, y0, x1, y1, 1, 1, 1);
            if (wantBeds[1]) ensureBeds(x0, y0, x1, y1, wantBeds[1]);
            cur = nullptr;
            w.addLevel(std::move(L));
        }
        if (hasCellar) {
            Level C = makeFloor("the cellar", id, -1, x0, y0, x1, y1, T_FLOOR_STONE, 0);
            C.floor = g - 2.6f;
            C.wallH = 2.4f;
            C.underground = true;
            C.hideBuilding = false;
            C.wallColor = vec3(0.56f, 0.53f, 0.47f);
            cur = &C;
            reserved = {{cfx, cfy}};
            entries = {{cfx, cfy}};
            C.at(cx, cy).feature = F_LADDER;
            C.at(cx, cy).var = (uint8_t)(cs | (2 << 2));
            w.addLink(cx, cy, 0, cfx, cfy, -1, "You climb down through the trapdoor into the cellar.");
            w.addLink(cx, cy, -1, cfx, cfy, 0, "You climb the ladder up out of the cellar.");
            for (int X = x0 + 1; X < x1; X++)
                for (int Y = y0 + 1; Y < y1; Y++)
                    if ((X == x0 + 1 || X == x1 - 1 || Y == y0 + 1 || Y == y1 - 1) && rng.chance(cellarForce ? 0.7f : 0.45f))
                        put(x0, y0, x1, y1, X, Y, rng.chance(0.8f) ? F_BARREL : F_CHEST, 0);
            cur = nullptr;
            w.addLevel(std::move(C));
        }
        reserved.clear(); entries.clear();
        float zb = g + GF_H, ztop = zb + (n - 1) * st.storey;
        float fx0 = x0, fy0 = y0, fx1 = x1 + 1, fy1 = y1 + 1;
        // an arcade (Padua): the upper storeys reach out over the street on the door side,
        // carried on columns at its edge. Only drawn: you walk under it on the street's own
        // tiles, and through the thin columns. A hash, not the town's random numbers, decides,
        // so that the houses of other towns stay as they were.
        if (st.portico > 0 && n > 1 && doorSide >= 0 && hash01(x0, y0, 23) < st.portico) {
            bool ok = true;
            int len = 0;
            for (int X = x0; X <= x1 && ok; X++)
                for (int Y = y0; Y <= y1 && ok; Y++) {
                    bool edge = doorSide == 0 ? X == x0 : doorSide == 1 ? X == x1 : doorSide == 2 ? Y == y0 : Y == y1;
                    if (!edge) continue;
                    int ox = X + (doorSide == 0 ? -1 : doorSide == 1 ? 1 : 0), oy = Y + (doorSide == 2 ? -1 : doorSide == 3 ? 1 : 0);
                    int tr = in(ox, oy) ? T(ox, oy).terrain : -1;
                    if (!in(ox, oy) || !open(ox, oy) || (tr != T_STREET && tr != T_SQUARE)) ok = false;
                    len++;
                }
            if (ok && len >= 2) {
                const float PJ = 1.1f, C = 0.32f;
                float a0, a1, e;   // the run of the arcade, and its outer edge
                if (doorSide <= 1) { a0 = fy0; a1 = fy1; e = doorSide == 0 ? fx0 - PJ : fx1 + PJ; }
                else { a0 = fx0; a1 = fx1; e = doorSide == 2 ? fy0 - PJ : fy1 + PJ; }
                int nc = std::max(2, (int)std::lround((a1 - a0) / 1.7f) + 1);
                vec3 col = b.trimColor;
                for (int k = 0; k < nc; k++) {
                    float t = a0 + 0.06f + (a1 - a0 - 0.12f - C) * k / (nc - 1);
                    float ex = doorSide == 0 ? e : doorSide == 1 ? e - C : 0, ey = doorSide == 2 ? e : doorSide == 3 ? e - C : 0;
                    if (doorSide <= 1) b.parts.push_back(box(vec3(ex, t, g), vec3(ex + C, t + C, zb), col));
                    else b.parts.push_back(box(vec3(t, ey, g), vec3(t + C, ey + C, zb), col));
                }
                // the arches' band under the storeys
                if (doorSide <= 1) {
                    float ex = doorSide == 0 ? e : e - 0.2f;
                    b.parts.push_back(box(vec3(ex, a0, zb - 0.5f), vec3(ex + 0.2f, a1, zb), col));
                } else {
                    float ey = doorSide == 2 ? e : e - 0.2f;
                    b.parts.push_back(box(vec3(a0, ey, zb - 0.5f), vec3(a1, ey + 0.2f, zb), col));
                }
                if (doorSide == 0) fx0 -= PJ; else if (doorSide == 1) fx1 += PJ; else if (doorSide == 2) fy0 -= PJ; else fy1 += PJ;
            }
        }
        if (n > 1) {
            int bands = (timber && st.jetty) ? n - 1 : 1;   // each jettied storey is a box of its own
            float jstep = timber ? 0.22f : 0.0f;
            for (int k = 1; k <= bands; k++) {
                float j = jstep * k;
                float z0 = bands == 1 ? zb : zb + (k - 1) * st.storey, z1 = bands == 1 ? ztop : zb + k * st.storey;
                b.parts.push_back(box(vec3(fx0 - j, fy0 - j, z0), vec3(fx1 + j, fy1 + j, z1), b.wallColor));
                if (timber) {
                    Part tp = windows(fx0 - j, fy0 - j, fx1 + j, fy1 + j, z0, z1, st.storey, GLASS, b.trimColor);
                    tp.kind = P_TIMBER;
                    tp.color2 = vec3(0.30f, 0.20f, 0.12f);
                    b.parts.push_back(tp);
                }
                b.parts.push_back(windows(fx0 - j, fy0 - j, fx1 + j, fy1 + j, z0, z1, st.storey, GLASS, b.trimColor));
            }
            float j = jstep * bands;
            fx0 -= j; fy0 -= j; fx1 += j; fy1 += j;
        }
        int axis = -1;
        if (st.gableFront && doorSide >= 0) axis = (doorSide >= 2) ? 1 : 0;   // ridge runs away from the street
        roofFor(b, st, fx0, fy0, fx1, fy1, ztop, axis);
    }

    // mirror a building east to west (its parts and its tiles): for churches that face east,
    // as St Peter's does. Only for buildings with no upper floors.
    void mirrorX(int id, int x0, int x1, int y0, int y1) {
        float M = (float)x0 + (float)x1 + 1.0f;
        for (Part &p : B(id).parts) {
            switch (p.kind) {
                case P_PRISM: case P_CONE: case P_DOME: case P_RING: case P_RINGWALL: case P_ELLRING:
                    p.a.x = M - p.a.x; break;
                case P_ROSE:
                    p.a.x = M - p.a.x;
                    if (p.axis == 0) p.axis = 1; else if (p.axis == 1) p.axis = 0;
                    break;
                case P_BUTTRESS:
                    p.a.x = M - p.a.x; p.b.x = M - p.b.x; break;
                default: {
                    float a = M - p.b.x, b = M - p.a.x;
                    p.a.x = a; p.b.x = b;
                    break;
                }
            }
        }
        for (int Y = y0; Y <= y1; Y++)
            for (int X = x0; X < x0 + (x1 - x0 + 1) / 2; X++) std::swap(T(X, Y), T(x0 + x1 - X, Y));
    }

    // ---------------------------------------------------------- landmarks
    void cathedral(const Cluster &cl, const Legend &L, bool church = false) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding(church ? "church" : "cathedral", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        vec3 stone = PALE_STONE * (0.96f + 0.04f * rng.uni());
        if (L.opt.count("stone")) { unsigned v = 0; std::sscanf(L.opt.at("stone").c_str(), "%x", &v); stone = vec3(((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f); }
        if (L.opt.count("brick")) stone = BRICK;
        b.wallColor = stone;
        vec3 roofc = roofOf(L, LEAD);
        bool tiled = roofc.x > 0.6f;                       // a low roof of Italian tiles
        bool flatFront = L.opt.count("front") && L.opt.at("front") == "flat";
        setFloor(id, x0, y0, x1, y1, T_FLOOR_STONE);
        ringWalls(id, x0, y0, x1, y1, 3);
        int Wd = y1 - y0 + 1, Lx = x1 - x0 + 1;
        int ym = (y0 + y1) / 2;
        bool twoTowers = (!church || L.opt.count("towers")) && !L.opt.count("westtower") && !flatFront;
        bool noWestTower = L.opt.count("westtower") && L.opt.at("westtower") == "no";
        bool battlement = L.opt.count("tower") && L.opt.at("tower") == "battlement";
        bool lowTowers = L.opt.count("towerheight") && L.opt.at("towerheight") == "low";
        int tw = twoTowers ? std::max(2, std::min(7, (int)std::lround(Wd * 0.36f))) : 0;
        int at = Wd >= 10 ? (int)std::lround(Wd * 0.22f) : 0;   // aisle width, tiles
        if (twoTowers)
            for (int X = x0; X < x0 + tw; X++)
                for (int k = 0; k < tw; k++) { T(X, y0 + k).wall = W_WALL; T(X, y1 - k).wall = W_WALL; }
        // portals in the west front
        T(x0, ym).wall = W_DOOR;
        if (Wd % 2 == 0) T(x0, ym + 1).wall = W_DOOR;
        if (!twoTowers && Wd >= 7) { T(x0, ym - 2).wall = W_DOOR; T(x0, ym + 2 + (Wd % 2 == 0)).wall = W_DOOR; }
        if (!open(x0 - 1, ym)) door(x0, y0, x1, y1, 2, true);
        // transept doors
        int xt = x0 + tw + (Lx - tw) / 2;
        door(xt - 2, y0, xt + 2, y1, 3, true);
        door(xt - 2, y0, xt + 2, y1, 2, true);
        // pillars between nave and aisles, benches in the nave, the altar
        if (at > 0)
            for (int X = x0 + tw + 2; X <= x1 - 4; X += 3) {
                T(X, y0 + at).feature = F_PILLAR;
                T(X, y1 - at).feature = F_PILLAR;
            }
        T(x1 - 3, ym).feature = F_ALTAR;
        if (Wd % 2 == 0) T(x1 - 3, ym + 1).feature = F_ALTAR;
        if (at > 0)
            for (int X = x0 + tw + 3; X <= x1 - 7; X += 2)
                for (int Y = y0 + at + 2; Y <= y1 - at - 2; Y++)
                    if (std::abs(Y - ym) > 1 && Y != ym + 1 && T(X, Y).feature == F_NONE) T(X, Y).feature = F_BENCH;

        float zb = g + GF_H;
        float X0 = x0, X1 = x1 + 1, Y0 = y0, Y1 = y1 + 1, YM = (Y0 + Y1) * 0.5f;
        float naveH = church ? 4.6f : 7.0f;
        float aisle = at > 0 ? at + 0.0f : 0.0f;
        float nx0 = X0 + (twoTowers ? tw : (noWestTower ? 1.4f : 4.4f)), nx1 = X1 - 2.4f;
        if (aisle > 0) {
            b.parts.push_back(box(vec3(nx0, Y0, zb), vec3(nx1, Y0 + aisle, zb + 2.1f), stone));
            b.parts.push_back(box(vec3(nx0, Y1 - aisle, zb), vec3(nx1, Y1, zb + 2.1f), stone));
            b.parts.push_back(roof(P_GABLE, nx0, Y0 - 0.15f, nx1, Y0 + aisle + 0.15f, zb + 2.1f, zb + 2.9f, 0, roofc, stone));
            b.parts.push_back(roof(P_GABLE, nx0, Y1 - aisle - 0.15f, nx1, Y1 + 0.15f, zb + 2.1f, zb + 2.9f, 0, roofc, stone));
            b.parts.push_back(windows(nx0, Y0, nx1, Y0 + aisle, zb, zb + 2.1f, 2.1f, STAINED, stone, 2.0f, 0.65f));
            b.parts.push_back(windows(nx0, Y1 - aisle, nx1, Y1, zb, zb + 2.1f, 2.1f, STAINED, stone, 2.0f, 0.65f));
        }
        float ny0 = Y0 + aisle, ny1 = Y1 - aisle;
        b.parts.push_back(box(vec3(nx0, ny0, zb), vec3(nx1, ny1, zb + naveH), stone));
        b.parts.push_back(windows(nx0, ny0, nx1, ny1, zb + (aisle > 0 ? 2.9f : 0.5f), zb + naveH, naveH - (aisle > 0 ? 2.9f : 0.5f), STAINED, stone, 2.0f, 0.75f));
        float ridge = zb + naveH + (tiled ? 0.55f : 1.25f) * (ny1 - ny0) * 0.5f;
        b.parts.push_back(roof(P_GABLE, nx0, ny0 - 0.2f, nx1, ny1 + 0.2f, zb + naveH, ridge, 0, roofc, stone));
        // transept
        float xm = (nx0 + nx1) * 0.5f + (church ? 2.0f : 1.0f);
        if (L.opt.count("crossing") && L.opt.at("crossing") == "dome") {
            // a great dome over the crossing, on a drum, with a lantern: dome=HEX its colour,
            // domeheight (1 round, more pointed), ribs=N white ribs, sides=N (8: octagonal)
            float R = std::min((ny1 - ny0) * 0.5f + 1.6f, 6.5f) * L.num("domescale", 1.0f);
            if (L.opt.count("domeat")) xm = X0 + L.num("domeat", 0.6f) * (X1 - X0);
            int sides = (int)L.num("sides", 16);
            float dz = zb + naveH - 0.6f, dh = L.num("drum", 2.6f);
            vec3 dc = L.opt.count("dome") ? hexColour(L.opt.at("dome")) : roofc;
            b.parts.push_back(prism(xm, YM, dz, dz + dh, R, sides, stone));
            b.parts.push_back(prism(xm, YM, dz + dh - 0.25f, dz + dh, R + 0.2f, sides, stone * 0.94f));
            Part dm; dm.kind = P_DOME; dm.a = vec3(xm, YM, dz + dh); dm.b = vec3(R, 0, 0); dm.color = dc; dm.sides = sides;
            dm.p1 = L.num("domeheight", 1.0f); dm.p2 = L.num("ribs", 0); dm.color2 = vec3(0.94f, 0.93f, 0.9f);
            b.parts.push_back(dm);
            float top = dz + dh + R * dm.p1;
            b.parts.push_back(prism(xm, YM, top - 0.2f, top + 1.5f, std::max(0.6f, R * 0.17f), 8, vec3(0.94f, 0.93f, 0.9f)));
            b.parts.push_back(cone(xm, YM, top + 1.5f, top + 3.0f, std::max(0.7f, R * 0.19f), 8, dc * 0.9f));
            b.parts.push_back(prism(xm, YM, top + 3.0f, top + 3.4f, 0.18f, 6, GOLD));
            if (L.opt.count("altar") && L.opt.at("altar") == "crossing") {
                // the high altar under the dome (St Peter's), with room around it for a canopy
                T(x1 - 3, ym).feature = F_NONE;
                if (Wd % 2 == 0) T(x1 - 3, ym + 1).feature = F_NONE;
                int xa = (int)std::floor(xm);
                for (int X = xa - 2; X <= xa + 2; X++)
                    for (int Y = ym - 2; Y <= ym + 3; Y++)
                        if (T(X, Y).feature == F_BENCH) T(X, Y).feature = F_NONE;
                T(xa, ym).feature = F_ALTAR;
                if (Wd % 2 == 0) T(xa, ym + 1).feature = F_ALTAR;
            }
        }
        if (L.opt.count("crossing") && L.opt.at("crossing") == "tower") {
            // a great square tower over the crossing, its spire long gone
            float tz = ridge + 5.5f, tw2 = std::min(3.4f, (ny1 - ny0) * 0.5f + 0.6f);
            b.parts.push_back(box(vec3(xm - tw2, YM - tw2, zb + naveH), vec3(xm + tw2, YM + tw2, tz), stone));
            b.parts.push_back(windows(xm - tw2, YM - tw2, xm + tw2, YM + tw2, ridge - 0.5f, tz, 2.6f, GLASS, stone, 1.4f, 0.75f));
            b.parts.push_back(crenel(xm - tw2, YM - tw2, xm + tw2, YM + tw2, tz, 0.5f, stone));
            {
                int a0 = (int)std::ceil(xm - tw2), a1 = (int)std::floor(xm + tw2) - 1;
                int c0 = (int)std::ceil(YM - tw2), c1 = (int)std::floor(YM + tw2) - 1;
                if (a1 - a0 >= 3 && c1 - c0 >= 3) {
                    Level top = towerTop("the great tower", id, a0, c0, a1, c1, tz, stone);
                    linkUp(top, a0 + 1, c0 + 1, a0 + 2, c0 + 1, a0 + 1, c0 + 1, a0 + 2, c0 + 1, F_STAIRS, F_STAIRS,
                           "You climb the long stair to the top of the great tower.", "You go down the stair into the church.");
                    w.addLevel(std::move(top));
                }
            }
            for (int c = 0; c < 4; c++) {
                float px = xm + ((c & 1) ? tw2 - 0.35f : -tw2 + 0.35f), py = YM + ((c & 2) ? tw2 - 0.35f : -tw2 + 0.35f);
                b.parts.push_back(roof(P_PYRAMID, px - 0.35f, py - 0.35f, px + 0.35f, py + 0.35f, tz, tz + 2.0f, 0, stone, stone));
            }
        }
        if (!church || Lx >= 22) {
            float tH = 2.4f;
            b.parts.push_back(box(vec3(xm - tH, Y0, zb), vec3(xm + tH, Y1, zb + naveH), stone));
            b.parts.push_back(roof(P_GABLE, xm - tH - 0.15f, Y0 - 0.15f, xm + tH + 0.15f, Y1 + 0.15f, zb + naveH,
                                   zb + naveH + (tiled ? 0.55f : 1.25f) * tH, 1, roofc, stone));
            Part rose; rose.kind = P_ROSE; rose.a = vec3(xm, Y0 - 0.03f, zb + naveH * 0.6f); rose.b = vec3(1.5f, 0, 0);
            rose.axis = 2; rose.color = STAINED; rose.color2 = stone;
            b.parts.push_back(rose);
            rose.a = vec3(xm, Y1 + 0.03f, zb + naveH * 0.6f); rose.axis = 3;
            b.parts.push_back(rose);
        }
        // apse
        float ar = (ny1 - ny0) * 0.5f + 0.2f;
        b.parts.push_back(prism(X1 - 2.4f - 0.02f, YM, zb, zb + naveH, ar, 12, stone));
        b.parts.push_back(cone(X1 - 2.4f - 0.02f, YM, zb + naveH, zb + naveH + (tiled ? 1.0f : 2.1f), ar + 0.2f, 12, roofc));
        if (flatFront) {
            // a flat screen front, wider than the nave, with a pediment (the Italian manner)
            float fy0 = aisle > 0 ? Y0 - 0.3f : ny0 - 0.6f, fy1 = aisle > 0 ? Y1 + 0.3f : ny1 + 0.6f;
            float fz = zb + naveH + L.num("frontheight", 1.2f);
            b.parts.push_back(box(vec3(X0, fy0, g), vec3(X0 + 1.3f, fy1, fz), stone));
            b.parts.push_back(windows(X0, fy0, X0 + 1.3f, fy1, zb, fz, 2.4f, GLASS, stone * 0.92f, 2.2f, 0.7f));
            b.parts.push_back(roof(P_GABLE, X0 - 0.1f, fy0 + (fy1 - fy0) * 0.2f, X0 + 1.4f, fy1 - (fy1 - fy0) * 0.2f, fz, fz + (fy1 - fy0) * 0.16f, 0, roofc, stone));
            if (!L.opt.count("norose")) {
                Part rose; rose.kind = P_ROSE; rose.a = vec3(X0 - 0.03f, YM, zb + naveH * 0.6f);
                rose.b = vec3(std::min(1.8f, (ny1 - ny0) * 0.3f), 0, 0); rose.axis = 0; rose.color = STAINED; rose.color2 = stone;
                b.parts.push_back(rose);
            }
        } else if (noWestTower) {
            // a gabled west front between two slender turrets
            float fz = zb + naveH + 1.0f;
            b.parts.push_back(box(vec3(X0, ny0 - 0.3f, zb), vec3(X0 + 1.4f, ny1 + 0.3f, fz), stone));
            for (int side = 0; side < 2; side++) {
                float ty = side == 0 ? ny0 - 0.3f : ny1 + 0.3f;
                b.parts.push_back(prism(X0 + 0.7f, ty, zb, fz + 2.2f, 0.75f, 8, stone));
                b.parts.push_back(cone(X0 + 0.7f, ty, fz + 2.2f, fz + 4.5f, 0.8f, 8, LEAD));
            }
            Part rose; rose.kind = P_ROSE; rose.a = vec3(X0 - 0.03f, YM, zb + naveH * 0.6f);
            rose.b = vec3(std::min(2.4f, (ny1 - ny0) * 0.4f), 0, 0); rose.axis = 0; rose.color = STAINED; rose.color2 = stone;
            b.parts.push_back(rose);
        } else if (twoTowers) {
            float tz = zb + naveH + (lowTowers ? 0.6f : (church ? 3.8f : 5.4f));
            for (int side = 0; side < 2; side++) {
                float ty0 = side == 0 ? Y0 : Y1 - tw, ty1 = side == 0 ? Y0 + tw : Y1;
                b.parts.push_back(box(vec3(X0, ty0, zb), vec3(X0 + tw, ty1, tz), stone));
                b.parts.push_back(windows(X0, ty0, X0 + tw, ty1, zb + naveH * 0.6f, tz, 2.4f, GLASS, stone, 1.6f, 0.75f));
                bool spire = L.opt.count("spire") && ((L.opt.at("spire") == "north") == (side == 1));
                if (spire) b.parts.push_back(roof(P_PYRAMID, X0 + 0.3f, ty0 + 0.3f, X0 + tw - 0.3f, ty1 - 0.3f, tz, tz + 8.0f, 0, LEAD, stone));
                else b.parts.push_back(crenel(X0, ty0, X0 + tw, ty1, tz, 0.45f, stone));
                if (!spire && tw >= 4) {
                    // hollow the tower, with a door from the aisle and a winding stair to the top
                    int tyA = side == 0 ? y0 : y1 - tw + 1;
                    for (int X = x0; X < x0 + tw; X++)
                        for (int Y = tyA; Y < tyA + tw; Y++) {
                            bool per = X == x0 || X == x0 + tw - 1 || Y == tyA || Y == tyA + tw - 1;
                            T(X, Y).wall = per ? W_WALL : W_NONE;
                            T(X, Y).feature = F_NONE;
                        }
                    int dY = tyA + tw / 2;
                    T(x0 + tw - 1, dY).wall = W_DOOR;
                    if (T(x0 + tw, dY).wall == W_NONE) T(x0 + tw, dY).feature = F_NONE;
                    int sy = side == 0 ? tyA + 1 : tyA + tw - 2;
                    Level top = towerTop(side == 0 ? "the south tower" : "the north tower", id, x0, tyA, x0 + tw - 1, tyA + tw - 1, tz, stone);
                    linkUp(top, x0 + 1, sy, x0 + 2, sy, x0 + 1, sy, x0 + 2, sy, F_STAIRS, F_STAIRS,
                           "You climb the winding stair to the top of the tower.", "You go down the winding stair.");
                    int bx = x0 + tw / 2, by = tyA + tw / 2;
                    if (tw >= 5 && !(bx == x0 + 2 && by == sy)) top.at(bx, by).feature = F_BELL;
                    w.addLevel(std::move(top));
                }
            }
            float fz = zb + naveH + 2.1f;
            b.parts.push_back(box(vec3(X0, Y0 + tw, zb), vec3(X0 + 2.0f, Y1 - tw, fz), stone));
            b.parts.push_back(crenel(X0, Y0 + tw, X0 + 2.0f, Y1 - tw, fz, 0.4f, stone));
            Part rose; rose.kind = P_ROSE; rose.a = vec3(X0 - 0.03f, YM, zb + naveH * 0.55f);
            rose.b = vec3(std::min(2.2f, (Wd - 2 * tw) * 0.5f + 0.6f), 0, 0); rose.axis = 0;
            rose.color = STAINED; rose.color2 = stone;
            b.parts.push_back(rose);
            // flying buttresses
            if (aisle > 0)
                for (float X = nx0 + 1.5f; X < nx1 - 0.8f; X += 3.0f)
                    for (int side = 0; side < 2; side++) {
                        Part bt; bt.kind = P_BUTTRESS;
                        float yo = side == 0 ? Y0 - 0.4f : Y1 + 0.4f, yi = side == 0 ? ny0 : ny1;
                        bt.a = vec3(X, yo, zb + 3.4f); bt.b = vec3(X, yi, zb + naveH - 0.6f);
                        bt.p1 = 0.45f; bt.p2 = g; bt.color = stone;
                        b.parts.push_back(bt);
                    }
            if (!church) {
                float sz = ridge - 0.3f;
                b.parts.push_back(roof(P_PYRAMID, xm - 0.7f, YM - 0.7f, xm + 0.7f, YM + 0.7f, sz, sz + 7.5f, 0, LEAD, stone));
            }
        } else if (!flatFront) {
            float tz = zb + naveH + 3.9f;
            float tx0 = X0, tx1 = X0 + 4.4f, ty0 = YM - 2.2f, ty1 = YM + 2.2f;
            b.parts.push_back(box(vec3(tx0, ty0, zb), vec3(tx1, ty1, tz), stone));
            b.parts.push_back(windows(tx0, ty0, tx1, ty1, zb + naveH * 0.7f, tz, 2.1f, GLASS, stone, 1.5f, 0.7f));
            if (battlement) {
                // a square tower with battlements and a pinnacle at each corner
                b.parts.push_back(crenel(tx0, ty0, tx1, ty1, tz, 0.5f, stone));
                int a0 = (int)std::ceil(ty0), a1 = (int)std::floor(ty1) - 1;
                if (a1 - a0 >= 3 && Wd >= 7) {
                    Level top = towerTop("the tower", id, x0, a0, x0 + 3, a0 + 3, tz, stone);
                    linkUp(top, x0 + 1, a0, x0 + 2, a0, x0 + 1, a0 + 1, x0 + 2, a0 + 1, F_STAIRS, F_STAIRS,
                           "You climb the stair to the top of the tower.", "You go down the stair.");
                    top.at(x0 + 1, a0 + 2).feature = F_BELL;
                    w.addLevel(std::move(top));
                }
                for (int c = 0; c < 4; c++) {
                    float px = (c & 1) ? tx1 - 0.3f : tx0 + 0.3f, py = (c & 2) ? ty1 - 0.3f : ty0 + 0.3f;
                    b.parts.push_back(roof(P_PYRAMID, px - 0.3f, py - 0.3f, px + 0.3f, py + 0.3f, tz, tz + 1.6f, 0, stone, stone));
                }
            } else {
                b.parts.push_back(roof(P_PYRAMID, tx0 - 0.1f, ty0 - 0.1f, tx1 + 0.1f, ty1 + 0.1f, tz, tz + 6.0f, 0, SLATE, stone));
            }
            b.parts.push_back(box(vec3(X0, Y0, zb), vec3(nx0, Y1, zb + naveH * 0.8f), stone));
        }
        if (L.opt.count("faces") && L.opt.at("faces") == "east") mirrorX(id, x0, x1, y0, y1);
    }

    void chapel(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding("chapel", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        vec3 stone = PALE_STONE;
        setFloor(id, x0, y0, x1, y1, T_FLOOR_TILE);
        ringWalls(id, x0, y0, x1, y1, 2);
        door(x0, y0, x1, y1, 0, true) || door(x0, y0, x1, y1);
        T(x1 - 2, (y0 + y1) / 2).feature = F_ALTAR;
        float zb = g + GF_H, X0 = x0, X1 = x1 + 1, Y0 = y0, Y1 = y1 + 1, YM = (Y0 + Y1) * 0.5f;
        float h = 7.2f;
        if (L.opt.count("plain")) {   // plain=1: a tall box, small high windows, a low roof behind a parapet
            stone = stoneOf(L, BRICK);
            b.wallColor = stone;
            h = L.num("height", 8.0f);
            if (L.num("ruined", 0.0f) > 0) {
                // ruined=0..1 (the cisterns of Hippo): roofless, a stone floor and no altar, the
                // walls broken to uneven heights (ruinTop: the ground-floor walls of the tiles, and
                // above them a part where a stretch stands higher), fallen blocks outside the lowest
                b.ruined = std::min(1.0f, L.num("ruined", 0.0f));
                b.ruinH = h;
                T(x1 - 2, (y0 + y1) / 2).feature = F_NONE;
                for (int Y = y0; Y <= y1; Y++)
                    for (int X = x0; X <= x1; X++) {
                        Tile &t = T(X, Y);
                        if (t.wall == W_NONE) { t.terrain = T_FLOOR_STONE; continue; }
                        if (t.wall == W_WINDOW) t.wall = W_WALL;
                        if (t.wall != W_WALL) continue;
                        float H = ruinTop(b, X, Y);
                        if (H > GF_H) b.parts.push_back(box(vec3(X, Y, zb), vec3(X + 1, Y + 1, g + H), stone));
                        if (H < h * 0.45f && hash01(X, Y, 93) < 0.6f) {
                            int ox = X == x0 ? -1 : X == x1 ? 1 : 0, oy = Y == y0 ? -1 : Y == y1 ? 1 : 0;
                            float sz = 0.22f + 0.2f * hash01(X, Y, 94);
                            float cx = X + 0.5f + ox * 0.95f + 0.3f * (hash01(X, Y, 95) - 0.5f);
                            float cy = Y + 0.5f + oy * 0.95f + 0.3f * (hash01(X, Y, 96) - 0.5f);
                            b.parts.push_back(box(vec3(cx - sz, cy - sz * 0.8f, g), vec3(cx + sz, cy + sz * 0.8f, g + sz * 1.1f), stone * 0.9f));
                        }
                    }
                return;
            }
            b.parts.push_back(box(vec3(X0, Y0, zb), vec3(X1, Y1, zb + h), stone));
            b.parts.push_back(windows(X0, Y0, X1, Y1, zb + h * 0.55f, zb + h - 0.6f, h, GLASS, stone, 1.6f, 0.7f));
            b.parts.push_back(crenel(X0 - 0.1f, Y0 - 0.1f, X1 + 0.1f, Y1 + 0.1f, zb + h, 0.45f, stone));
            int axis = (X1 - X0) >= (Y1 - Y0) ? 0 : 1;
            b.parts.push_back(roof(P_HIP, X0 + 0.3f, Y0 + 0.3f, X1 - 0.3f, Y1 - 0.3f, zb + h, zb + h + 0.25f * std::min(X1 - X0, Y1 - Y0), axis, roofOf(L, TERRACOTTA), stone));
            return;
        }
        b.parts.push_back(box(vec3(X0, Y0, zb), vec3(X1 - 2.0f, Y1, zb + h), stone));
        b.parts.push_back(windows(X0, Y0, X1 - 2.0f, Y1, zb + 0.5f, zb + h, h - 0.5f, STAINED, stone, 1.8f, 0.85f));
        float ridge = zb + h + 1.9f * (Y1 - Y0) * 0.5f;
        b.parts.push_back(roof(P_GABLE, X0 - 0.08f, Y0 - 0.2f, X1 - 2.0f, Y1 + 0.2f, zb + h, ridge, 0, LEAD, stone));
        b.parts.push_back(prism(X1 - 2.0f, YM, zb, zb + h, (Y1 - Y0) * 0.5f, 10, stone));
        b.parts.push_back(cone(X1 - 2.0f, YM, zb + h, ridge, (Y1 - Y0) * 0.5f + 0.15f, 10, LEAD));
        float xm = (X0 + X1 - 2.0f) * 0.5f;
        b.parts.push_back(roof(P_PYRAMID, xm - 0.5f, YM - 0.5f, xm + 0.5f, YM + 0.5f, ridge - 0.6f, ridge + 6.3f, 0, vec3(0.75f, 0.62f, 0.3f), stone));
        for (float X = X0 + 1.8f; X < X1 - 2.0f; X += 1.8f)
            for (int side = 0; side < 2; side++)
                b.parts.push_back(box(vec3(X - 0.25f, side == 0 ? Y0 - 0.5f : Y1, g), vec3(X + 0.25f, side == 0 ? Y0 : Y1 + 0.5f, zb + h + 0.6f), stone, true));
    }

    // a mosque (Constantinople): a square prayer hall under a great dome on a drum, four small
    // domes at its corners, half-domes east and west, and minarets=N (2, 4 or 6) at the corners,
    // slender, with a balcony and a pointed lead cap. building=1: a building site (the Yeni Cami),
    // walls half raised and no dome. The mihrab is on the south-east wall (an altar to the game).
    void mosque(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding("church", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        vec3 stone = stoneOf(L, vec3(0.88f, 0.85f, 0.78f));
        vec3 dc = L.opt.count("dome") ? hexColour(L.opt.at("dome")) : LEAD;
        b.wallColor = stone;
        setFloor(id, x0, y0, x1, y1, T_FLOOR_TILE);
        ringWalls(id, x0, y0, x1, y1, 2);
        door(x0, y0, x1, y1);   // on the side that faces open ground
        T((x0 + x1) / 2, y0 + 1).feature = F_ALTAR;                // the mihrab, on the qibla wall
        float zb = g, X0 = x0, X1 = x1 + 1, Y0 = y0, Y1 = y1 + 1, cx = (X0 + X1) * 0.5f, cy = (Y0 + Y1) * 0.5f;
        float half = 0.5f * std::min(X1 - X0, Y1 - Y0), h = L.num("height", 5.0f);
        bool site = L.opt.count("building");
        if (site) h *= 0.55f;
        b.parts.push_back(box(vec3(X0, Y0, zb), vec3(X1, Y1, zb + h), stone));
        b.parts.push_back(windows(X0, Y0, X1, Y1, zb + 1.2f, zb + h - 0.3f, 1.6f, GLASS, stone, 1.4f, 0.6f));
        if (!site) {
            b.parts.push_back(box(vec3(X0 + 0.2f, Y0 + 0.2f, zb + h), vec3(X1 - 0.2f, Y1 - 0.2f, zb + h + 0.25f), dc));   // the lead roof
            float R = half * 0.62f, drum = 0.9f;
            b.parts.push_back(prism(cx, cy, zb + h, zb + h + drum, R * 1.02f, 20, stone));
            b.parts.push_back(windows(cx - R * 0.7f, cy - R * 0.7f, cx + R * 0.7f, cy + R * 0.7f, zb + h + 0.2f, zb + h + drum - 0.1f, drum, GLASS, stone, 0.8f, 0.3f));
            Part dm; dm.kind = P_DOME; dm.a = vec3(cx, cy, zb + h + drum); dm.b = vec3(R, 0, 0); dm.color = dc; dm.sides = 20;
            b.parts.push_back(dm);
            b.parts.push_back(cone(cx, cy, zb + h + drum + R * 0.95f, zb + h + drum + R * 0.95f + 0.9f, 0.08f, 6, GOLD));   // the finial
            for (int side = -1; side <= 1; side += 2) {   // half-domes east and west, lower
                Part hd; hd.kind = P_DOME; hd.a = vec3(cx + side * R * 0.95f, cy, zb + h); hd.b = vec3(R * 0.62f, 0, 0); hd.color = dc; hd.sides = 16;
                b.parts.push_back(hd);
            }
            for (int c = 0; c < 4; c++) {   // small domes at the corners
                float px = (c & 1) ? X1 - half * 0.32f : X0 + half * 0.32f, py = (c & 2) ? Y1 - half * 0.32f : Y0 + half * 0.32f;
                Part sd; sd.kind = P_DOME; sd.a = vec3(px, py, zb + h + 0.2f); sd.b = vec3(half * 0.2f, 0, 0); sd.color = dc; sd.sides = 12;
                b.parts.push_back(sd);
            }
        } else {
            for (float X = X0 + 1.2f; X < X1 - 0.5f; X += 2.4f)   // scaffolding poles
                b.parts.push_back(box(vec3(X, Y0 - 0.15f, zb), vec3(X + 0.1f, Y0 - 0.05f, zb + h + 1.6f), vec3(0.55f, 0.42f, 0.28f)));
        }
        int nm = (int)L.num("minarets", 2);
        float mh = site ? 0 : h * 2.6f + half * 0.9f;
        for (int c = 0; c < nm && mh > 0; c++) {
            // 2: the two northern corners; 4: all four; 6: four, and two more out at the forecourt's corners
            float px, py;
            if (c < 4) { int cc = nm == 2 ? (c == 0 ? 2 : 3) : c; px = (cc & 1) ? X1 + 0.25f : X0 - 0.25f; py = (cc & 2) ? Y1 + 0.25f : Y0 - 0.25f; }
            else { px = c == 4 ? X0 - 0.25f : X1 + 0.25f; py = Y1 + half * 1.2f; }
            float r = 0.32f;
            b.parts.push_back(prism(px, py, zb, zb + mh, r, 12, stone));
            b.parts.push_back(prism(px, py, zb + mh * 0.72f, zb + mh * 0.72f + 0.18f, r + 0.22f, 12, stone * 0.95f));   // the balcony
            if (nm >= 4) b.parts.push_back(prism(px, py, zb + mh * 0.5f, zb + mh * 0.5f + 0.16f, r + 0.2f, 12, stone * 0.95f));
            b.parts.push_back(cone(px, py, zb + mh, zb + mh + 2.2f, r + 0.05f, 12, dc));
        }
    }

    // floors=N on a block building (a hall): N upper floors (at most 2) over the whole rectangle,
    // each cut into rooms and furnished as a house's are, with stairs from the floor below
    // (important buildings may be enlarged by adding storeys)
    void upperFloors(int id, int x0, int y0, int x1, int y1, int nf, float zb, vec3 stone) {
        nf = std::min(nf, 2);
        if (nf <= 0 || x1 - x0 < 4 || y1 - y0 < 4) return;
        std::vector<Level> ups;
        for (int k = 1; k <= nf; k++) {
            Level U = makeFloor(k == 1 ? "the upper floor" : "the second floor", id, k, x0, y0, x1, y1, T_FLOOR_WOOD, 2);
            U.floor = zb + (k - 1) * 2.0f;
            U.wallH = 2.0f;
            U.wallColor = stone;
            ups.push_back(std::move(U));
        }
        int ux, uy, ufx, ufy, us;
        cur = nullptr;
        reserved.clear(); entries.clear();
        if (!ladderSpot(x0, y0, x1, y1, ux, uy, ufx, ufy, us)) return;
        T(ux, uy).feature = F_STAIRS; T(ux, uy).var = (uint8_t)us;
        stairsTo(ups[0], ux, uy, ufx, ufy, F_STAIRS, "You climb the stairs to the upper floor.", "You go down the stairs.");
        int px = ux, py = uy;
        for (int k = 0; k < nf; k++) {
            Level &U = ups[k];
            cur = &U;
            reserved = {{k == 0 ? ufx : px, k == 0 ? ufy : py}};
            entries = reserved.empty() ? std::vector<std::pair<int, int>>{} : std::vector<std::pair<int, int>>{*reserved.begin()};
            int vx = -1, vy = -1, vfx = 0, vfy = 0, vs = 0;
            if (k + 1 < nf && ladderSpot(x0, y0, x1, y1, vx, vy, vfx, vfy, vs, px, py)) {   // stairs on to the next floor
                U.at(vx, vy).feature = F_STAIRS; U.at(vx, vy).var = (uint8_t)vs;
                reserved.insert({vfx, vfy});
            }
            rooms(x0, y0, x1, y1, 0, 1, 1);
            if (vx >= 0) {
                Level &N = ups[k + 1];
                N.at(vx, vy).feature = F_STAIRS; N.at(vx, vy).var = (uint8_t)vs;
                N.at(vfx, vfy).feature = F_NONE; N.at(vfx, vfy).wall = W_NONE;
                w.addLink(vx, vy, U.z, vfx, vfy, N.z, "You climb the stairs to the second floor.");
                w.addLink(vx, vy, N.z, vfx, vfy, U.z, "You go down the stairs.");
                px = vfx; py = vfy;
            } else if (k + 1 < nf) { ups.resize(k + 1); break; }   // no room for more stairs: one floor only
        }
        cur = nullptr;
        reserved.clear(); entries.clear();
        for (auto &U : ups) w.addLevel(std::move(U));
    }

    // a palace: wings around a courtyard; one side may be open    // a palace: wings around a courtyard; one side may be open
    void palace(const Cluster &cl, const Legend &L, const std::string &kind = "palace") {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int d = (int)L.num("depth", 6);
        std::string openSide = L.opt.count("open") ? L.opt.at("open") : "";
        int storeys = (int)L.num("storeys", 3);
        int id = newBuilding(L.opt.count("as") ? L.opt.at("as") : kind, L.name, x0, y0, x1, y1);   // as=church: a convent
        Building &b = B(id);
        vec3 stone = stoneOf(L, PALE_STONE);
        vec3 roofc = roofOf(L, SLATE);
        b.wallColor = stone;
        bool hasN = openSide != "north", hasS = openSide != "south", hasE = openSide != "east", hasW = openSide != "west";
        setFloor(id, x0, y0, x1, y1, T_SQUARE);
        struct R { int x0, y0, x1, y1; };
        std::vector<R> wings;
        if (hasS) wings.push_back({x0, y0, x1, y0 + d - 1});
        if (hasN) wings.push_back({x0, y1 - d + 1, x1, y1});
        if (hasW) wings.push_back({x0, hasS ? y0 + d - 1 : y0, x0 + d - 1, hasN ? y1 - d + 1 : y1});
        if (hasE) wings.push_back({x1 - d + 1, hasS ? y0 + d - 1 : y0, x1, hasN ? y1 - d + 1 : y1});
        auto inside = [](const R &r, int X, int Y) { return X > r.x0 && X < r.x1 && Y > r.y0 && Y < r.y1; };
        for (auto &r : wings)
            for (int X = r.x0; X <= r.x1; X++)
                for (int Y = r.y0; Y <= r.y1; Y++) T(X, Y).terrain = T_FLOOR_TILE;
        for (auto &r : wings)
            for (int X = r.x0; X <= r.x1; X++)
                for (int Y = r.y0; Y <= r.y1; Y++) {
                    bool per = X == r.x0 || X == r.x1 || Y == r.y0 || Y == r.y1;
                    if (!per) continue;
                    bool shared = false;
                    for (auto &q : wings)
                        if (&q != &r && inside(q, X, Y)) shared = true;
                    if (shared) continue;
                    bool corner = (X == r.x0 || X == r.x1) && (Y == r.y0 || Y == r.y1);
                    T(X, Y).wall = (!corner && (X + Y) % 2 == 0) ? W_WINDOW : W_WALL;
                }
        // open the joints between wings meeting at a corner
        for (auto &r : wings)
            for (auto &q : wings) {
                if (&r == &q) continue;
                for (int X = std::max(r.x0, q.x0); X <= std::min(r.x1, q.x1); X++)
                    for (int Y = std::max(r.y0, q.y0); Y <= std::min(r.y1, q.y1); Y++) {
                        if (inside(r, X, Y) || inside(q, X, Y)) continue;
                        bool nearR = false, nearQ = false;
                        const int dd[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                        for (auto &e : dd) {
                            if (inside(r, X + e[0], Y + e[1])) nearR = true;
                            if (inside(q, X + e[0], Y + e[1])) nearQ = true;
                        }
                        if (nearR && nearQ) T(X, Y).wall = W_NONE;
                    }
            }
        int xm = (x0 + x1) / 2, ym = (y0 + y1) / 2;
        // rooms: cross walls along each wing, each with a door
        for (auto &r : wings) {
            bool alongX = (r.x1 - r.x0) > (r.y1 - r.y0);
            int lo = alongX ? r.x0 : r.y0, hi = alongX ? r.x1 : r.y1;
            int mid = alongX ? xm : ym;
            for (int p = lo + d + 3; p <= hi - d - 3; p += 8) {
                if (std::abs(p - mid) <= 2) continue;
                int a0 = alongX ? r.y0 + 1 : r.x0 + 1, a1 = alongX ? r.y1 - 1 : r.x1 - 1;
                int dpos = (a0 + a1) / 2;
                for (int a = a0; a <= a1; a++) {
                    int X = alongX ? p : a, Y = alongX ? a : p;
                    T(X, Y).wall = (a == dpos) ? W_DOOR : W_WALL;
                }
            }
        }
        // entrance passages through the wings, centred
        auto passage = [&](int side) {
            if (side == 2 && hasS) for (int Y = y0; Y <= y0 + d - 1; Y++) for (int k = 0; k < 2; k++) T(xm + k, Y).wall = (Y == y0 || Y == y0 + d - 1) ? W_DOOR : W_NONE;
            if (side == 3 && hasN) for (int Y = y1 - d + 1; Y <= y1; Y++) for (int k = 0; k < 2; k++) T(xm + k, Y).wall = (Y == y1 || Y == y1 - d + 1) ? W_DOOR : W_NONE;
            if (side == 0 && hasW) for (int X = x0; X <= x0 + d - 1; X++) for (int k = 0; k < 2; k++) T(X, ym + k).wall = (X == x0 || X == x0 + d - 1) ? W_DOOR : W_NONE;
            if (side == 1 && hasE) for (int X = x1 - d + 1; X <= x1; X++) for (int k = 0; k < 2; k++) T(X, ym + k).wall = (X == x1 || X == x1 - d + 1) ? W_DOOR : W_NONE;
        };
        int best = -1, bestScore = -1;
        int sx[4] = {x0 - 1, x1 + 1, xm, xm}, sy[4] = {ym, ym, y0 - 1, y1 + 1};
        bool has[4] = {hasW, hasE, hasS, hasN};
        for (int k = 0; k < 4; k++) {
            if (!has[k]) continue;
            int sc = streetLike(sx[k], sy[k]) ? 3 : (open(sx[k], sy[k]) ? 1 : 0);
            if (sc > bestScore) { bestScore = sc; best = k; }
        }
        if (best >= 0) passage(best);
        for (int k = 0; k < 4; k++)
            if (k != best && has[k] && open(sx[k], sy[k]) && rng.chance(0.6f)) passage(k);
        if (x1 - x0 + 1 - 2 * d >= 4 && y1 - y0 + 1 - 2 * d >= 4) T(xm, ym).feature = F_FOUNTAIN;

        float zb = g + GF_H, st = 2.0f, zt = zb + (storeys - 1) * st;
        for (auto &r : wings) {
            float X0 = r.x0, X1 = r.x1 + 1, Y0 = r.y0, Y1 = r.y1 + 1;
            b.parts.push_back(box(vec3(X0, Y0, zb), vec3(X1, Y1, zt), stone));
            b.parts.push_back(windows(X0, Y0, X1, Y1, zb, zt, st, GLASS, FRAME, 1.7f, 0.62f));
            int axis = (X1 - X0) >= (Y1 - Y0) ? 0 : 1;
            b.parts.push_back(roof(P_HIP, X0 - 0.2f, Y0 - 0.2f, X1 + 0.2f, Y1 + 0.2f, zt, zt + (roofc.x > 0.6f ? 0.5f : 1.15f) * d * 0.5f + 0.5f, axis, roofc, stone));
        }
        auto pavilion = [&](float X0, float Y0, float X1, float Y1, float extra) {
            b.parts.push_back(box(vec3(X0 - 0.3f, Y0 - 0.3f, zb), vec3(X1 + 0.3f, Y1 + 0.3f, zt + extra), stone));
            b.parts.push_back(windows(X0 - 0.3f, Y0 - 0.3f, X1 + 0.3f, Y1 + 0.3f, zb, zt + extra, st, GLASS, FRAME, 1.7f, 0.62f));
            b.parts.push_back(roof(P_PYRAMID, X0 - 0.45f, Y0 - 0.45f, X1 + 0.45f, Y1 + 0.45f, zt + extra, zt + extra + 1.6f * d * 0.5f + 1.5f, 0, roofc, stone));
        };
        float X0 = x0, X1 = x1 + 1, Y0 = y0, Y1 = y1 + 1;
        if (hasS && hasW) pavilion(X0, Y0, X0 + d, Y0 + d, 0.9f);
        if (hasS && hasE) pavilion(X1 - d, Y0, X1, Y0 + d, 0.9f);
        if (hasN && hasW) pavilion(X0, Y1 - d, X0 + d, Y1, 0.9f);
        if (hasN && hasE) pavilion(X1 - d, Y1 - d, X1, Y1, 0.9f);
        if (L.opt.count("clock")) {
            float cx = (X0 + X1) * 0.5f;
            pavilion(cx - d * 0.5f - 1.0f, Y1 - d, cx + d * 0.5f + 1.0f, Y1, 2.4f);
        }
        // reading tables down the middle of a college's wings (with a bench each side), and a
        // few tables in a palace. No random numbers here, so the rest of the town is unchanged.
        int every = kind == "college" ? 4 : 9;
        for (auto &r : wings) {
            bool alongX = (r.x1 - r.x0) >= (r.y1 - r.y0);
            int mid = alongX ? (r.y0 + r.y1) / 2 : (r.x0 + r.x1) / 2;
            int a0 = (alongX ? r.x0 : r.y0) + 3, a1 = (alongX ? r.x1 : r.y1) - 3;
            for (int a = a0; a <= a1; a += every) {
                int X = alongX ? a : mid, Y = alongX ? mid : a;
                if (!put(x0, y0, x1, y1, X, Y, F_TABLE, 0)) continue;
                if (kind != "college") continue;
                put(x0, y0, x1, y1, alongX ? X : X - 1, alongX ? Y - 1 : Y, F_BENCH, 0);
                put(x0, y0, x1, y1, alongX ? X : X + 1, alongX ? Y + 1 : Y, F_BENCH, 0);
            }
        }
        // floors=N (not with library=): upper floors over the wings, as the ground floor's walls
        // (windows where the ground floor has its passages), open over the courtyard, with beds,
        // tables and chests, and stairs in a corner of each floor
        int nfl = L.opt.count("library") ? 0 : std::min(2, (int)L.num("floors", 0));
        if (nfl > 0 && d >= 3) {
            auto inWing = [&](int X, int Y) {
                for (auto &r : wings) if (X >= r.x0 && X <= r.x1 && Y >= r.y0 && Y <= r.y1) return true;
                return false;
            };
            const int corner[2][4] = {{x0 + 1, y0 + 1, x0 + 2, y0 + 1}, {x1 - 1, y1 - 1, x1 - 2, y1 - 1}};
            std::vector<Level> ups;
            for (int k = 1; k <= nfl; k++) {
                Level U = makeFloor(k == 1 ? "the upper floor" : "the second floor", id, k, x0, y0, x1, y1, T_FLOOR_WOOD, 0);
                U.floor = zb + (k - 1) * st;
                U.wallH = st;
                U.wallColor = stone;
                for (int X = x0; X <= x1; X++)
                    for (int Y = y0; Y <= y1; Y++) {
                        Tile &u = U.at(X, Y);
                        if (!inWing(X, Y)) { u.terrain = T_VOID; u.wall = W_NONE; continue; }
                        const Tile &gt = T(X, Y);
                        bool outer = X == x0 || X == x1 || Y == y0 || Y == y1;
                        bool courtSide = !inWing(X + 1, Y) || !inWing(X - 1, Y) || !inWing(X, Y + 1) || !inWing(X, Y - 1);
                        u.wall = gt.wall;
                        if (u.wall == W_DOOR && (outer || courtSide)) u.wall = W_WINDOW;   // no passages upstairs
                        if ((outer || courtSide) && u.wall == W_WALL && (X + Y) % 2 == 0) u.wall = W_WINDOW;
                        if ((outer || courtSide) && u.wall == W_NONE) u.wall = W_WALL;
                    }
                ups.push_back(std::move(U));
            }
            // stairs: the ground floor's corner up to the first floor, and its other corner on to the second
            bool ok = T(corner[0][0], corner[0][1]).wall == W_NONE && T(corner[0][2], corner[0][3]).wall == W_NONE;
            if (ok) {
                T(corner[0][0], corner[0][1]).feature = F_NONE;
                if (T(corner[0][2], corner[0][3]).feature != F_NONE) T(corner[0][2], corner[0][3]).feature = F_NONE;
                stairsTo(ups[0], corner[0][0], corner[0][1], corner[0][2], corner[0][3], F_STAIRS, "You climb the stairs to the upper floor.", "You go down the stairs.");
                T(corner[0][0], corner[0][1]).feature = F_STAIRS;
                if (nfl > 1 && ups[0].at(corner[1][0], corner[1][1]).wall == W_NONE && ups[0].at(corner[1][2], corner[1][3]).wall == W_NONE) {
                    for (int k = 0; k < 2; k++) {
                        ups[k].at(corner[1][0], corner[1][1]).feature = F_STAIRS;
                        ups[k].at(corner[1][2], corner[1][3]).feature = F_NONE;
                    }
                    w.addLink(corner[1][0], corner[1][1], 1, corner[1][2], corner[1][3], 2, "You climb the stairs to the second floor.");
                    w.addLink(corner[1][0], corner[1][1], 2, corner[1][2], corner[1][3], 1, "You go down the stairs.");
                } else ups.resize(1);
                // furniture: beds and chests against the walls, a table here and there
                for (size_t k = 0; k < ups.size(); k++) {
                    cur = &ups[k];
                    for (int X = x0 + 1; X < x1; X++)
                        for (int Y = y0 + 1; Y < y1; Y++) {
                            Tile &u = ups[k].at(X, Y);
                            if (u.terrain == T_VOID || u.wall != W_NONE || u.feature != F_NONE) continue;
                            bool nearStair = false, byWall = false, byDoor = false;
                            for (int dy = -1; dy <= 1; dy++)
                                for (int dx = -1; dx <= 1; dx++) {
                                    const Tile &n = ups[k].at(X + dx, Y + dy);
                                    if (n.feature == F_STAIRS) nearStair = true;
                                    if (n.wall == W_DOOR) byDoor = true;
                                    if ((dx == 0) != (dy == 0) && (n.wall == W_WALL || n.wall == W_WINDOW)) byWall = true;
                                }
                            if (nearStair || byDoor) continue;
                            uint32_t hsh = hash2i(X, Y, 40 + (int)k);
                            uint8_t f = byWall ? (hsh % 9 == 0 ? F_BED : hsh % 13 == 1 ? F_CHEST : F_NONE) : (hsh % 11 == 0 ? F_TABLE : F_NONE);
                            if (f != F_NONE) put(x0, y0, x1, y1, X, Y, f, (uint8_t)(hsh % 4));
                        }
                    cur = nullptr;
                }
                for (auto &U : ups) w.addLevel(std::move(U));
            }
        }
        // library=NAME (the Bodleian): a reading room upstairs, round all four wings, its inner
        // walls along the courtyard, with stairs in a corner of the ground floor. Its desks and
        // chained books are items (items.txt "art "NAME" upstairs: ..."); a town file may guard it
        // ("guarded NAME").
        if (L.opt.count("library") && hasN && hasS && hasE && hasW && x1 - x0 >= 2 * d + 1 && y1 - y0 >= 2 * d + 1) {
            int sx = x0 + 1, sy = y0 + 1, fx = x0 + 2, fy = y0 + 1;
            if (T(sx, sy).wall == W_NONE && T(fx, fy).wall == W_NONE) {
                std::string lname = L.opt.at("library");   // library=Duke_Humfrey's_Library: '_' for a space
                for (char &c : lname) if (c == '_') c = ' ';
                Level U = makeFloor(lname, id, 1, x0, y0, x1, y1, T_FLOOR_WOOD, 2);
                U.floor = zb;
                U.wallH = st;
                U.wallColor = stone;
                for (int X = x0 + d; X <= x1 - d; X++)
                    for (int Y = y0 + d; Y <= y1 - d; Y++) {
                        bool rim = X == x0 + d || X == x1 - d || Y == y0 + d || Y == y1 - d;
                        U.at(X, Y).wall = rim && (X + Y) % 2 == 0 ? W_WINDOW : W_WALL;   // windows onto the courtyard
                    }
                T(sx, sy).feature = F_NONE;
                if (T(fx, fy).feature != F_NONE) T(fx, fy).feature = F_NONE;
                stairsTo(U, sx, sy, fx, fy, F_STAIRS, "You climb the stairs to " + U.name + ".", "You go down the stairs.");
                T(sx, sy).feature = F_STAIRS;
                w.addLevel(std::move(U));
            }
        }
    }

    // a long single wing, such as the Louvre's gallery along the river
    void gallery(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding("gallery", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        vec3 stone = stoneOf(L, PALE_STONE);
        vec3 roofc = roofOf(L, SLATE);
        setFloor(id, x0, y0, x1, y1, T_FLOOR_TILE);
        float zb = g + GF_H, zt = zb + 2.0f;
        float X0 = x0, X1 = x1 + 1, Y0 = y0, Y1 = y1 + 1;
        int axis = (X1 - X0) >= (Y1 - Y0) ? 0 : 1;
        if (std::min(x1 - x0, y1 - y0) <= 1) {
            // too thin for rooms (the Procuratie, two tiles deep): an arcade along the side that
            // faces the open ground, under the upper storeys, and a back wall with passages
            // through it.
            int openA = 0, openB = 0;   // A: the south or west side, B: the north or east side
            for (int k = axis == 0 ? x0 : y0; k <= (axis == 0 ? x1 : y1); k++) {
                if (axis == 0) { openA += streetLike(k, y0 - 1); openB += streetLike(k, y1 + 1); }
                else { openA += streetLike(x0 - 1, k); openB += streetLike(x1 + 1, k); }
            }
            bool frontA = openA >= openB;
            bool thin = (axis == 0 ? y1 - y0 : x1 - x0) == 0;
            for (int X = x0; X <= x1; X++)
                for (int Y = y0; Y <= y1; Y++) {
                    int across = axis == 0 ? Y - y0 : X - x0;          // 0 on side A
                    int along = axis == 0 ? X - x0 : Y - y0;
                    bool front = thin || (frontA ? across == 0 : across == 1);
                    Tile &t = T(X, Y);
                    t.feature = F_NONE;
                    if (front) { t.wall = W_NONE; continue; }
                    int bx = axis == 0 ? X : (frontA ? X + 1 : X - 1), by = axis == 0 ? (frontA ? Y + 1 : Y - 1) : Y;
                    t.wall = (along % 7 == 3 && streetLike(bx, by)) ? W_DOOR : W_WALL;
                }
            const float C = 0.34f;
            float e = axis == 0 ? (frontA ? Y0 : Y1 - C) : (frontA ? X0 : X1 - C);
            float a0 = axis == 0 ? X0 : Y0, a1 = axis == 0 ? X1 : Y1;
            int nc = std::max(2, (int)std::lround((a1 - a0) / 1.7f) + 1);
            for (int k = 0; k < nc; k++) {
                float t = a0 + 0.05f + (a1 - a0 - 0.1f - C) * k / (nc - 1);
                if (axis == 0) b.parts.push_back(box(vec3(t, e, g), vec3(t + C, e + C, zb), stone));
                else b.parts.push_back(box(vec3(e, t, g), vec3(e + C, t + C, zb), stone));
            }
            if (axis == 0) b.parts.push_back(box(vec3(X0, e, zb - 0.5f), vec3(X1, e + C, zb), stone * 0.96f));
            else b.parts.push_back(box(vec3(e, Y0, zb - 0.5f), vec3(e + C, Y1, zb), stone * 0.96f));
        } else {
            ringWalls(id, x0, y0, x1, y1, 2);
            door(x0, y0, x1, y1, -1, true);
            door(x0, y0, x0 + 8, y1);
            door(x1 - 8, y0, x1, y1);
            // a door near an end may land on the inner edge of that end's piece: a doorway standing
            // in the middle of the floor. Only the outer
            // wall has doors. (The same random numbers are drawn, so the rest of the town is unchanged.)
            for (int X = x0 + 1; X < x1; X++)
                for (int Y = y0 + 1; Y < y1; Y++)
                    if (T(X, Y).wall == W_DOOR) T(X, Y).wall = W_NONE;
        }
        b.parts.push_back(box(vec3(X0, Y0, zb), vec3(X1, Y1, zt), stone));
        b.parts.push_back(windows(X0, Y0, X1, Y1, zb, zt, 2.0f, GLASS, FRAME, 1.7f, 0.65f));
        float half = 0.5f * std::min(X1 - X0, Y1 - Y0);
        b.parts.push_back(roof(P_HIP, X0 - 0.15f, Y0 - 0.2f, X1 + 0.15f, Y1 + 0.2f, zt, zt + (roofc.x > 0.6f ? 0.55f : 1.1f) * half + 0.4f, axis, roofc, stone));
    }

    void fortress(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding("fortress", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        bool conical = L.opt.count("roofs");
        float height = L.num("height", 7.5f);
        vec3 stone = STONE * 0.95f;
        setFloor(id, x0, y0, x1, y1, T_YARD);
        const int th = 2;   // thickness of the walls
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++)
                if (X < x0 + th || X > x1 - th || Y < y0 + th || Y > y1 - th) T(X, Y).wall = W_CITYWALL;
        std::vector<vec2> tc;
        float X0 = x0, X1 = x1 + 1, Y0 = y0, Y1 = y1 + 1;
        float tr = 2.6f, ti = 1.8f;
        tc.push_back(vec2(X0 + ti, Y0 + ti)); tc.push_back(vec2(X1 - ti, Y0 + ti));
        tc.push_back(vec2(X0 + ti, Y1 - ti)); tc.push_back(vec2(X1 - ti, Y1 - ti));
        if (x1 - x0 >= 18) { tc.push_back(vec2((X0 + X1) * 0.5f, Y0 + ti)); tc.push_back(vec2((X0 + X1) * 0.5f, Y1 - ti)); }
        if (y1 - y0 >= 18) { tc.push_back(vec2(X0 + ti, (Y0 + Y1) * 0.5f)); tc.push_back(vec2(X1 - ti, (Y0 + Y1) * 0.5f)); }
        for (auto &c : tc) {
            for (int X = (int)std::floor(c.x - tr); X <= (int)std::floor(c.x + tr); X++)
                for (int Y = (int)std::floor(c.y - tr); Y <= (int)std::floor(c.y + tr); Y++)
                    if (X >= x0 && X <= x1 && Y >= y0 && Y <= y1 && length(vec2(X + 0.5f, Y + 0.5f) - c) < tr + 0.2f)
                        T(X, Y).wall = W_TOWER;
            b.parts.push_back(prism(c.x, c.y, g - 0.6f, g + GF_H, tr, 14, stone, false));
            b.parts.push_back(prism(c.x, c.y, g + GF_H, g + height + 0.8f, tr, 14, stone));
            if (conical) b.parts.push_back(cone(c.x, c.y, g + height + 0.8f, g + height + 0.8f + 1.7f * tr, tr + 0.2f, 14, SLATE));
            else b.parts.push_back(crenel(c.x - tr, c.y - tr, c.x + tr, c.y + tr, g + height + 0.8f, 0.5f, stone));
        }
        b.parts.push_back(box(vec3(X0, Y0, g + TOWNWALL_H), vec3(X1, Y0 + th, g + height), stone));
        b.parts.push_back(box(vec3(X0, Y1 - th, g + TOWNWALL_H), vec3(X1, Y1, g + height), stone));
        b.parts.push_back(box(vec3(X0, Y0 + th, g + TOWNWALL_H), vec3(X0 + th, Y1 - th, g + height), stone));
        b.parts.push_back(box(vec3(X1 - th, Y0 + th, g + TOWNWALL_H), vec3(X1, Y1 - th, g + height), stone));
        b.parts.push_back(crenel(X0, Y0, X1, Y1, g + height, 0.5f, stone));
        // gate, two tiles wide: on the side and at the place where open ground outside meets the
        // courtyard inside with no tower in the way
        int xm = (x0 + x1) / 2, ym = (y0 + y1) / 2;
        int best = -1, shift = 0, bs = -1000;
        for (int k = 0; k < 4; k++)
            for (int o = -12; o <= 12; o++) {
                bool clear = true;
                for (int q = 0; q < 2 && clear; q++)
                    for (int t = 0; t <= th + 1 && clear; t++) {
                        int X = k == 0 ? x0 + t : k == 1 ? x1 - t : xm + q + o;
                        int Y = k == 2 ? y0 + t : k == 3 ? y1 - t : ym + q + o;
                        if (X < x0 || X > x1 || Y < y0 || Y > y1 || T(X, Y).wall == W_TOWER) clear = false;
                    }
                if (!clear) continue;
                int sc = 0;
                for (int q = 0; q < 2; q++) {
                    int X = k == 0 ? x0 - 1 : k == 1 ? x1 + 1 : xm + q + o;
                    int Y = k == 2 ? y0 - 1 : k == 3 ? y1 + 1 : ym + q + o;
                    // a landmark not built yet (the Medici arsenal against Pisa's Cittadella Vecchia) is no way out
                    bool future = in(X, Y) && std::isupper((unsigned char)ch(X, Y)) && T(X, Y).bld == 0;
                    sc += future ? -20 : streetLike(X, Y) ? 6 : (open(X, Y) ? 2 : -20);
                }
                sc -= std::abs(o);
                if (sc > bs) { bs = sc; best = k; shift = o; }
            }
        if (best < 0) { best = 2; shift = 0; }
        for (int k = 0; k < 2; k++)
            for (int t = 0; t < th; t++) {
                int X = best == 0 ? x0 + t : best == 1 ? x1 - t : xm + k + shift;
                int Y = best == 2 ? y0 + t : best == 3 ? y1 - t : ym + k + shift;
                T(X, Y).wall = W_GATE;
                T(X, Y).terrain = T_FLOOR_STONE;
            }
        if (L.opt.count("roofed")) {
            // a building, not a courtyard (the Waag): a wooden floor inside, under one roof
            for (int X = x0 + th; X <= x1 - th; X++)
                for (int Y = y0 + th; Y <= y1 - th; Y++)
                    if (T(X, Y).wall == W_NONE) T(X, Y).terrain = T_FLOOR_WOOD;
            float a0 = X0 + th, a1 = X1 - th, c0 = Y0 + th, c1 = Y1 - th;
            b.parts.push_back(box(vec3(a0, c0, g + GF_H), vec3(a1, c1, g + height), stone));
            b.parts.push_back(roof(P_HIP, a0 - 0.2f, c0 - 0.2f, a1 + 0.2f, c1 + 0.2f, g + height, g + height + 0.55f * std::min(a1 - a0, c1 - c0),
                                   (a1 - a0) >= (c1 - c0) ? 0 : 1, roofOf(L, SLATE), stone));
        }
        if (L.opt.count("keep") && x1 - x0 >= 20 && y1 - y0 >= 18) {
            vec3 white(0.9f, 0.88f, 0.82f);
            int hx0 = xm - 5, hx1 = xm + 5, hy0 = ym - 4, hy1 = ym + 4;
            for (int X = hx0; X <= hx1; X++)
                for (int Y = hy0; Y <= hy1; Y++) {
                    bool per = X == hx0 || X == hx1 || Y == hy0 || Y == hy1;
                    T(X, Y).terrain = T_FLOOR_STONE;
                    T(X, Y).wall = per ? W_WALL : W_NONE;
                }
            door(hx0, hy0, hx1, hy1);
            for (int X = hx0 + 2; X <= hx1 - 2; X += 3) T(X, ym).feature = F_PILLAR;
            T(hx0 + 2, hy1 - 1).feature = F_CHEST;
            T(hx1 - 2, hy1 - 1).feature = F_CHEST;
            float kz = g + GF_H + 6.5f;
            float K0x = hx0, K1x = hx1 + 1, K0y = hy0, K1y = hy1 + 1;
            {
                Level top = towerTop(L.opt.at("keep") == "white" ? "the roof of the White Tower" : "the roof of the keep", id, hx0, hy0, hx1, hy1, kz, white);
                for (int c = 0; c < 4; c++)   // the turrets
                    for (int dx = 0; dx < 2; dx++)
                        for (int dy = 0; dy < 2; dy++)
                            top.at((c & 1) ? hx1 - dx : hx0 + dx, (c & 2) ? hy1 - dy : hy0 + dy).wall = W_WALL;
                int sx, sy, fx, fy;
                stairSpot(hx0, hy0, hx1, hy1, sx, sy, fx, fy);
                linkUp(top, sx, sy, fx, fy, hx0 + 3, hy0 + 2, hx0 + 4, hy0 + 2, F_STAIRS, F_STAIRS,
                       "You climb the stair to the roof.", "You go down the stair.");
                w.addLevel(std::move(top));
            }
            b.parts.push_back(box(vec3(K0x, K0y, g + GF_H), vec3(K1x, K1y, kz), white));
            b.parts.push_back(windows(K0x, K0y, K1x, K1y, g + GF_H, kz, 2.2f, GLASS, white, 1.8f, 0.45f));
            b.parts.push_back(crenel(K0x, K0y, K1x, K1y, kz, 0.5f, white));
            for (int c = 0; c < 4; c++) {
                float px = (c & 1) ? K1x - 0.6f : K0x + 0.6f, py = (c & 2) ? K1y - 0.6f : K0y + 0.6f;
                bool round = c == 3;   // one of the four turrets is round
                if (round) b.parts.push_back(prism(px, py, g + GF_H, kz + 2.4f, 1.3f, 12, white));
                else b.parts.push_back(box(vec3(px - 1.2f, py - 1.2f, g + GF_H), vec3(px + 1.2f, py + 1.2f, kz + 2.4f), white));
                Part dm; dm.kind = P_DOME; dm.a = vec3(px, py, kz + 2.4f); dm.b = vec3(1.15f, 0, 0); dm.color = LEAD; dm.sides = 10;
                b.parts.push_back(dm);
                b.parts.push_back(roof(P_PYRAMID, px - 0.12f, py - 0.12f, px + 0.12f, py + 0.12f, kz + 3.4f, kz + 4.6f, 0, vec3(0.75f, 0.62f, 0.3f), white));
            }
        } else if (x1 - x0 >= 16 && y1 - y0 >= 14) {
            // a keep inside, if there is room
            int hx0 = xm - 4, hx1 = xm + 4, hy0 = ym - 2, hy1 = ym + 3;
            if (best == 2) { hy0 = ym; hy1 = ym + 5; } else if (best == 3) { hy0 = ym - 5; hy1 = ym; }
            for (int X = hx0; X <= hx1; X++)
                for (int Y = hy0; Y <= hy1; Y++) {
                    bool per = X == hx0 || X == hx1 || Y == hy0 || Y == hy1;
                    T(X, Y).terrain = T_FLOOR_STONE;
                    T(X, Y).wall = per ? W_WALL : W_NONE;
                }
            door(hx0, hy0, hx1, hy1);
            T(xm, (hy0 + hy1) / 2).feature = F_TABLE;
            T(xm, (hy0 + hy1) / 2 + 1).feature = F_BENCH;
            T(hx1 - 1, hy1 - 1).feature = F_HEARTH;
            T(hx1 - 1, hy1 - 1).var = 3;
            b.parts.push_back(box(vec3(hx0, hy0, g + GF_H), vec3(hx1 + 1, hy1 + 1, g + GF_H + 2.0f), stone));
            b.parts.push_back(windows(hx0, hy0, hx1 + 1, hy1 + 1, g + GF_H, g + GF_H + 2.0f, 2.0f, GLASS, stone, 1.8f, 0.5f));
            b.parts.push_back(roof(P_HIP, hx0 - 0.2f, hy0 - 0.2f, hx1 + 1.2f, hy1 + 1.2f, g + GF_H + 2.0f, g + GF_H + 4.0f, 0, SLATE, stone));
        }
        b.wallColor = stone;
        // the battlements, with a stair up from the courtyard
        {
            Level top = towerTop("the battlements", id, x0, y0, x1, y1, g + height, stone);
            for (int X = x0; X <= x1; X++)
                for (int Y = y0; Y <= y1; Y++) {
                    Tile &t = T(X, Y);
                    bool onWall = t.wall == W_CITYWALL || t.wall == W_TOWER || t.wall == W_GATE;
                    top.at(X, Y).terrain = onWall ? T_FLOOR_STONE : T_VOID;
                    top.at(X, Y).wall = W_NONE;
                }
            const int d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            // which courtyard tiles can be reached from the gate?
            auto reachable = [&]() {
                std::set<std::pair<int, int>> seen;
                std::vector<std::pair<int, int>> st;
                for (int X = x0; X <= x1; X++)
                    for (int Y = y0; Y <= y1; Y++)
                        if (T(X, Y).wall == W_GATE) { st.push_back({X, Y}); seen.insert({X, Y}); }
                while (!st.empty()) {
                    auto p = st.back(); st.pop_back();
                    for (auto &d : d4) {
                        int X = p.first + d[0], Y = p.second + d[1];
                        if (X < x0 || X > x1 || Y < y0 || Y > y1 || seen.count({X, Y})) continue;
                        Tile &t = T(X, Y);
                        if ((t.wall != W_NONE && t.wall != W_DOOR && t.wall != W_GATE) || blocksRoom(t.feature)) continue;
                        seen.insert({X, Y});
                        st.push_back({X, Y});
                    }
                }
                return seen;
            };
            auto before = reachable();
            bool done = false;
            for (int Y = y0 + th; Y <= y1 - th && !done; Y++)
                for (int X = x0 + th; X <= x1 - th && !done; X++) {
                    Tile &t = T(X, Y);
                    if (t.wall != W_NONE || t.feature != F_NONE || !before.count({X, Y})) continue;
                    for (auto &d : d4) {
                        int wx = X + d[0], wy = Y + d[1], ox = wx + d[0], oy = wy + d[1], fx = X - d[0], fy = Y - d[1];
                        if (T(wx, wy).wall != W_CITYWALL || T(ox, oy).wall != W_CITYWALL) continue;
                        if (T(fx, fy).wall != W_NONE || T(fx, fy).feature != F_NONE || !before.count({fx, fy})) continue;
                        t.feature = F_STAIRS;
                        auto after = reachable();
                        t.feature = F_NONE;
                        if (after.size() + 1 < before.size()) continue;   // it would cut the courtyard in two
                        linkUp(top, X, Y, fx, fy, wx, wy, ox, oy, F_STAIRS, F_STAIRS,
                               "You climb the stair to the battlements.", "You go down the stair.");
                        done = true;
                        break;
                    }
                }
            if (done) w.addLevel(std::move(top));
        }
    }

    void hall(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding("hall", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        vec3 stone = stoneOf(L, PALE_STONE);
        vec3 roofc = roofOf(L, SLATE);
        b.wallColor = stone;
        setFloor(id, x0, y0, x1, y1, T_FLOOR_TILE);
        ringWalls(id, x0, y0, x1, y1, 2);
        door(x0, y0, x1, y1, -1, true);
        door(x0, y0, x1, y1);
        int xm = (x0 + x1) / 2, ym = (y0 + y1) / 2;
        bool tribunal = L.opt.count("tribunal") > 0;   // judges behind the table, and the floor before it bare (the Holy Office)
        for (int k = -2; k <= 2; k++) {   // a long table with benches, inside the walls (a small hall has a short one)
            int X = xm + k;
            if (X <= x0 + 1 || X >= x1 - 1) continue;
            if (T(X, ym).wall == W_NONE) T(X, ym).feature = F_TABLE;
            if (!tribunal && ym - 1 > y0 && T(X, ym - 1).wall == W_NONE) T(X, ym - 1).feature = F_BENCH;
            if (ym + 1 < y1 && T(X, ym + 1).wall == W_NONE) T(X, ym + 1).feature = F_BENCH;
        }
        float zb = g + GF_H, zt = zb + 2 * 2.0f;
        upperFloors(id, x0, y0, x1, y1, (int)L.num("floors", 0), zb, stone);
        float X0 = x0, X1 = x1 + 1, Y0 = y0, Y1 = y1 + 1;
        int axis = (X1 - X0) >= (Y1 - Y0) ? 0 : 1;
        b.parts.push_back(box(vec3(X0, Y0, zb), vec3(X1, Y1, zt), stone));
        b.parts.push_back(windows(X0, Y0, X1, Y1, zb, zt, 2.0f, GLASS, FRAME, 1.7f, 0.62f));
        float half = 0.5f * std::min(X1 - X0, Y1 - Y0);
        float ridge = zt + (roofc.x > 0.6f ? 0.5f : 1.3f) * half;   // tiles: a low Italian roof
        if (L.opt.count("battlements")) {   // a parapet on corbels round the top (the Palazzo Vecchio)
            b.parts.push_back(box(vec3(X0 - 0.35f, Y0 - 0.35f, zt - 0.6f), vec3(X1 + 0.35f, Y1 + 0.35f, zt + 0.5f), stone * 0.95f));
            b.parts.push_back(crenel(X0 - 0.35f, Y0 - 0.35f, X1 + 0.35f, Y1 + 0.35f, zt + 0.5f, 0.55f, stone));
            b.parts.push_back(roof(P_HIP, X0 + 0.2f, Y0 + 0.2f, X1 - 0.2f, Y1 - 0.2f, zt + 0.4f, zt + 0.4f + 0.45f * half, axis, roofc, stone));
            return;
        }
        b.parts.push_back(roof(P_HIP, X0 - 0.25f, Y0 - 0.25f, X1 + 0.25f, Y1 + 0.25f, zt, ridge, axis, roofc, stone));
        float cx = (X0 + X1) * 0.5f, cy = (Y0 + Y1) * 0.5f;
        if (L.opt.count("nocampanile")) {
            // a great hall: a lantern on the ridge instead
            b.parts.push_back(box(vec3(cx - 0.8f, cy - 0.8f, ridge - 0.6f), vec3(cx + 0.8f, cy + 0.8f, ridge + 1.0f), stone));
            b.parts.push_back(roof(P_PYRAMID, cx - 1.0f, cy - 1.0f, cx + 1.0f, cy + 1.0f, ridge + 1.0f, ridge + 2.6f, 0, LEAD, stone));
            return;
        }
        b.parts.push_back(box(vec3(cx - 1.1f, cy - 1.1f, ridge - 1.2f), vec3(cx + 1.1f, cy + 1.1f, ridge + 2.0f), stone));
        b.parts.push_back(windows(cx - 1.1f, cy - 1.1f, cx + 1.1f, cy + 1.1f, ridge - 0.3f, ridge + 2.0f, 2.3f, GLASS, stone, 1.1f, 0.6f));
        Part dm; dm.kind = P_DOME; dm.a = vec3(cx, cy, ridge + 2.0f); dm.b = vec3(1.25f, 0, 0); dm.color = LEAD; dm.sides = 12;
        b.parts.push_back(dm);
        b.parts.push_back(roof(P_PYRAMID, cx - 0.12f, cy - 0.12f, cx + 0.12f, cy + 0.12f, ridge + 3.2f, ridge + 4.8f, 0, vec3(0.7f, 0.6f, 0.3f), stone));
        for (int side = 0; side < 2; side++) {
            float pw = 4.4f;
            float px0 = axis == 0 ? (side == 0 ? X0 : X1 - pw) : X0, px1 = axis == 0 ? (side == 0 ? X0 + pw : X1) : X1;
            float py0 = axis == 1 ? (side == 0 ? Y0 : Y1 - pw) : Y0, py1 = axis == 1 ? (side == 0 ? Y0 + pw : Y1) : Y1;
            b.parts.push_back(roof(P_PYRAMID, px0 - 0.35f, py0 - 0.35f, px1 + 0.35f, py1 + 0.35f, zt, ridge + (roofc.x > 0.6f ? 0.3f : 1.6f), 0, roofc, stone));
        }
    }

    // covered market: a big roof on pillars
    void halle(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding("market", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        bool timber = L.opt.count("timber") > 0;   // a byre or a shed: timber posts and an earth floor with straw
        setFloor(id, x0, y0, x1, y1, timber ? T_YARD : T_SQUARE);
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++) {
                bool per = X == x0 || X == x1 || Y == y0 || Y == y1;
                if (per && ((X - x0) % 4 == 0 || X == x1) && ((Y - y0) % 4 == 0 || Y == y1)) { T(X, Y).feature = F_PILLAR; T(X, Y).var = timber ? 1 : 0; }
                else if (timber && !per && Y == y1 - 1 && (X - x0) % 3 == 1) T(X, Y).feature = F_HAY;   // the hay along the back
                else if (!per && !L.opt.count("nostalls") && (X - x0) % 3 == 2 && (Y - y0) % 2 == 1 && rng.chance(0.6f)) T(X, Y).feature = F_STALL;
            }
        float X0 = x0, X1 = x1 + 1, Y0 = y0, Y1 = y1 + 1;
        int axis = (X1 - X0) >= (Y1 - Y0) ? 0 : 1;
        float half = 0.5f * std::min(X1 - X0, Y1 - Y0);
        b.parts.push_back(box(vec3(X0 - 0.3f, Y0 - 0.3f, g + 3.4f), vec3(X1 + 0.3f, Y1 + 0.3f, g + 3.7f), vec3(0.45f, 0.32f, 0.2f)));
        b.parts.push_back(roof(P_HIP, X0 - 0.5f, Y0 - 0.5f, X1 + 0.5f, Y1 + 0.5f, g + 3.7f, g + 3.7f + 0.8f * half, axis, vec3(0.62f, 0.32f, 0.22f), STONE));
    }

    // a square of uniform pavilions around a garden (the Place Royale)
    void place(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        if (!L.name.empty()) { Building a; a.name = L.name; a.kind = "place"; a.x0 = x0; a.y0 = y0; a.x1 = x1; a.y1 = y1; a.ground = 0; w.areas.push_back(a); }
        int d = (int)L.num("depth", 6);
        int sty = L.opt.count("style") ? w.styleIndex(L.opt.at("style")) : s.style;
        for (int X = x0 + d; X <= x1 - d; X++)
            for (int Y = y0 + d; Y <= y1 - d; Y++) {
                Tile &t = T(X, Y);
                t.terrain = T_GARDEN; t.wall = W_NONE; t.feature = F_NONE;
                bool edge = X == x0 + d + 1 || X == x1 - d - 1 || Y == y0 + d + 1 || Y == y1 - d - 1;
                if (edge && (X + Y) % 3 == 0) t.feature = F_OAK;
            }
        int xm = (x0 + x1) / 2, ym = (y0 + y1) / 2;
        T(xm, ym).feature = F_FOUNTAIN;
        T(xm, ym).terrain = T_SQUARE;
        // passages under the King's and the Queen's pavilions, two tiles wide
        for (int k = 0; k < 2; k++) {
            for (int Y = y0; Y < y0 + d; Y++) { T(xm + k, Y).terrain = T_STREET; T(xm + k, Y).wall = W_NONE; }
            for (int Y = y1 - d + 1; Y <= y1; Y++) { T(xm + k, Y).terrain = T_STREET; T(xm + k, Y).wall = W_NONE; }
        }
        auto row = [&](int ax0, int ay0, int ax1, int ay1, bool alongX) {
            int a = alongX ? ax0 : ay0, end = alongX ? ax1 : ay1;
            while (a <= end) {
                int len = std::min(end - a + 1, 7);
                if (end - (a + len) + 1 < 5 && end - a + 1 <= 12) len = end - a + 1;
                int hx0 = alongX ? a : ax0, hx1 = alongX ? a + len - 1 : ax1;
                int hy0 = alongX ? ay0 : a, hy1 = alongX ? ay1 : a + len - 1;
                if (alongX && hx0 <= xm + 1 && hx1 >= xm) {
                    if (xm - 1 >= hx0 + 4) house(hx0, hy0, xm - 1, hy1, sty, "", 3, 3);
                    if (hx1 >= xm + 6) house(xm + 2, hy0, hx1, hy1, sty, "", 3, 3);
                    a += len;
                    continue;
                }
                house(hx0, hy0, hx1, hy1, sty, "", 3, 3);
                a += len;
            }
        };
        row(x0, y0, x1, y0 + d - 1, true);
        row(x0, y1 - d + 1, x1, y1, true);
        row(x0, y0 + d, x0 + d - 1, y1 - d, false);
        row(x1 - d + 1, y0 + d, x1, y1 - d, false);
        for (int side = 0; side < 2; side++) {
            int py0 = side == 0 ? y0 : y1 - d + 1, py1 = side == 0 ? y0 + d - 1 : y1;
            int id = newBuilding("pavilion", side == 0 ? "the Pavillon du Roi" : "the Pavillon de la Reine", xm, py0, xm + 1, py1);
            Building &b = B(id);
            for (int Y = py0; Y <= py1; Y++) for (int k = 0; k < 2; k++) T(xm + k, Y).bld = (uint16_t)id;
            vec3 brick(0.66f, 0.30f, 0.24f);
            float zb = g + DOOR_H + 1.2f;
            b.parts.push_back(box(vec3(xm - 1.0f, py0 - 0.2f, zb), vec3(xm + 3.0f, py1 + 1.2f, zb + 4.4f), brick));
            b.parts.push_back(windows(xm - 1.0f, py0 - 0.2f, xm + 3.0f, py1 + 1.2f, zb, zb + 4.4f, 2.0f, GLASS, FRAME, 1.6f, 0.6f));
            b.parts.push_back(roof(P_PYRAMID, xm - 1.3f, py0 - 0.5f, xm + 3.3f, py1 + 1.5f, zb + 4.4f, zb + 7.6f, 0, SLATE, brick));
            b.parts.push_back(box(vec3(xm - 1.0f, py0 - 0.2f, g + GF_H), vec3(xm + 3.0f, py1 + 1.2f, zb), FRAME));
        }
    }

    // a round open-air playhouse: galleries in a ring, a yard, a stage under a roof
    void theatre(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding("theatre", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        float cx = (x0 + x1 + 1) * 0.5f, cy = (y0 + y1 + 1) * 0.5f;
        float R = std::min(x1 - x0 + 1, y1 - y0 + 1) * 0.5f - 0.2f, Ri = R - 3.2f;
        vec3 plaster(0.9f, 0.86f, 0.76f), timber(0.32f, 0.22f, 0.13f), thatch(0.66f, 0.56f, 0.36f);
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++) {
                float d = length(vec2(X + 0.5f - cx, Y + 0.5f - cy));
                Tile &t = T(X, Y);
                t.bld = (uint16_t)id;
                t.feature = F_NONE;
                t.wall = W_NONE;
                if (d > R) { t.terrain = T_YARD; t.bld = 0; continue; }
                if (d > R - 1.0f) { t.wall = W_TOWER; t.terrain = T_FLOOR_WOOD; continue; }      // outer wall
                if (d > Ri) { t.terrain = T_FLOOR_WOOD; if (((X + Y) & 3) == 0 && d < R - 2.0f) t.feature = F_BENCH; continue; }
                t.terrain = T_YARD;   // the yard, open to the sky
                if (d > Ri - 1.0f && (int)std::lround(std::atan2(Y + 0.5f - cy, X + 0.5f - cx) * 8 / PI_F) % 2 == 0) t.feature = F_PILLAR;
            }
        // the entrance faces the best street; the stage is opposite
        int best = 0, bs = -1;
        int sx[4] = {x0 - 1, x1 + 1, (x0 + x1) / 2, (x0 + x1) / 2}, sy[4] = {(y0 + y1) / 2, (y0 + y1) / 2, y0 - 1, y1 + 1};
        for (int k = 0; k < 4; k++) {
            int sc = streetLike(sx[k], sy[k]) ? 3 : (open(sx[k], sy[k]) ? 1 : 0);
            if (sc > bs) { bs = sc; best = k; }
        }
        vec2 dir = best == 0 ? vec2(-1, 0) : best == 1 ? vec2(1, 0) : best == 2 ? vec2(0, -1) : vec2(0, 1);
        for (float r = 0; r <= R + 0.5f; r += 0.5f)
            for (int w2 = -1; w2 <= 0; w2++) {
                vec2 p = vec2(cx, cy) + dir * r + vec2(-dir.y, dir.x) * (w2 + 0.5f);
                int X = (int)std::floor(p.x), Y = (int)std::floor(p.y);
                if (X < x0 || X > x1 || Y < y0 || Y > y1) continue;
                Tile &t = T(X, Y);
                if (t.wall == W_TOWER) t.wall = W_DOOR;
                if (t.feature == F_PILLAR || t.feature == F_BENCH) t.feature = F_NONE;
            }
        // the stage
        vec2 sc = vec2(cx, cy) - dir * (Ri - 2.0f);
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++) {
                vec2 q(X + 0.5f - sc.x, Y + 0.5f - sc.y);
                if (std::fabs(dot(q, dir)) < 1.6f && std::fabs(dot(q, vec2(-dir.y, dir.x))) < 2.6f && T(X, Y).terrain == T_YARD &&
                    length(vec2(X + 0.5f - cx, Y + 0.5f - cy)) < Ri) {
                    T(X, Y).feature = F_STAGE;
                    T(X, Y).var = (uint8_t)(X + Y);
                }
            }
        // the tiring-house behind the stage, where the players dress: the galleries there walled off from the yard, with
        // the stage's two doors into it, and walled at its two ends
        {
            vec2 side(-dir.y, dir.x);
            for (int X = x0; X <= x1; X++)
                for (int Y = y0; Y <= y1; Y++) {
                    Tile &t = T(X, Y);
                    vec2 q(X + 0.5f - cx, Y + 0.5f - cy);
                    float d = length(q), along = -dot(q, dir), across = std::fabs(dot(q, side));   // along: towards the stage
                    if (t.bld != (uint16_t)id || t.wall == W_TOWER || t.wall == W_DOOR || along <= 0) continue;
                    if (d <= Ri || d > R - 1.0f || across > 3.2f) continue;   // the galleries' ring behind the stage only
                    t.feature = F_NONE;   // no benches in it
                    if (across > 2.2f) t.wall = W_WALL;
                    else if (d < Ri + 1.0f) t.wall = std::fabs(across - 1.5f) < 0.6f ? W_DOOR : W_WALL;
                }
        }
        float zw = g + GF_H + 2.6f;
        for (auto pr : {prism(cx, cy, g - 0.4f, g + GF_H, R, 20, plaster, false), prism(cx, cy, g + GF_H, zw, R, 20, plaster),
                        prism(cx, cy, g + GF_H - 0.12f, g + GF_H + 0.08f, R + 0.06f, 20, timber)}) {
            pr.p1 = 1;   // the yard is open to the sky
            b.parts.push_back(pr);
        }
        Part iw; iw.kind = P_RINGWALL; iw.a = vec3(cx, cy, g + GF_H); iw.b = vec3(Ri + 0.05f, 0, zw); iw.sides = 20;
        iw.color = plaster * 0.9f; iw.color2 = timber;
        b.parts.push_back(iw);
        Part rg; rg.kind = P_RING; rg.a = vec3(cx, cy, zw); rg.b = vec3(R + 0.4f, Ri - 0.3f, zw + 1.6f); rg.sides = 20; rg.color = thatch;
        b.parts.push_back(rg);
        // the "heavens" over the stage, and the hut above
        vec2 st0 = sc - dir * 1.4f - vec2(-dir.y, dir.x) * 2.6f, st1 = sc + dir * 1.2f + vec2(-dir.y, dir.x) * 2.6f;
        vec3 lo(std::min(st0.x, st1.x), std::min(st0.y, st1.y), zw - 0.6f), hi(std::max(st0.x, st1.x), std::max(st0.y, st1.y), zw - 0.3f);
        b.parts.push_back(box(lo, hi, timber));
        b.parts.push_back(roof(P_GABLE, lo.x - 0.2f, lo.y - 0.2f, hi.x + 0.2f, hi.y + 0.2f, zw - 0.3f, zw + 1.2f, dir.x != 0 ? 1 : 0, thatch, plaster));
        for (int c = 0; c < 2; c++) {
            vec2 pp = sc + dir * 1.0f + vec2(-dir.y, dir.x) * (c == 0 ? -2.2f : 2.2f);
            b.parts.push_back(prism(pp.x, pp.y, g, zw - 0.6f, 0.15f, 8, vec3(0.55f, 0.3f, 0.2f), true));
        }
        if (L.opt.count("flag")) {
            vec2 fp = vec2(cx, cy) - dir * (R - 0.6f);
            b.parts.push_back(prism(fp.x, fp.y, zw + 1.0f, zw + 4.0f, 0.06f, 6, timber));
            b.parts.push_back(box(vec3(fp.x, fp.y - 0.02f, zw + 3.0f), vec3(fp.x + 1.2f, fp.y + 0.02f, zw + 3.8f), vec3(0.85f, 0.85f, 0.8f)));
        }
    }

    // a row of houses built on a bridge
    void bridgeHouses(const Cluster &cl, const Legend &L) {
        bool alongY = (cl.y1 - cl.y0) >= (cl.x1 - cl.x0);
        int a = alongY ? cl.y0 : cl.x0, end = alongY ? cl.y1 : cl.x1;
        while (a <= end) {
            int len = std::min(end - a + 1, rng.range(5, 7));
            if (end - (a + len) + 1 < 4) len = end - a + 1;
            if (len >= 4) {
                // shops: each house carries the bridge's name (the goldsmiths of the Ponte Vecchio)
                const std::string nm = L.opt.count("shops") ? L.name : std::string();
                if (alongY) house(cl.x0, a, cl.x1, a + len - 1, -1, nm);
                else house(a, cl.y0, a + len - 1, cl.y1, -1, nm);
            }
            a += len;
        }
    }

    // ---------------------------------------------------------- Italian and Dutch pieces
    // a free-standing tower (a campanile, a belfry): height=, top=pyramid|spire|battlement|cupola|
    // belfry|bulb, stone=HEX or brick, roof=tile|lead; with a stair to the top inside
    void tower(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding("tower", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        vec3 stone = stoneOf(L, PALE_STONE);
        vec3 roofc = roofOf(L, LEAD);
        b.wallColor = stone;
        setFloor(id, x0, y0, x1, y1, T_FLOOR_STONE);
        ringWalls(id, x0, y0, x1, y1, 0);
        door(x0, y0, x1, y1, -1, true);
        float H = L.num("height", 16.0f);
        float X0 = x0, X1 = x1 + 1, Y0 = y0, Y1 = y1 + 1, cx = (X0 + X1) * 0.5f, cy = (Y0 + Y1) * 0.5f;
        float top = g + H, half = 0.5f * std::min(X1 - X0, Y1 - Y0);
        bool round = L.opt.count("round");
        float lean = L.num("lean", 0.0f);
        if (round && lean > 0) { leaningTower(id, L, x0, y0, x1, y1, top, half, lean, stone); return; }
        if (round) b.parts.push_back(prism(cx, cy, g, top, half, 14, stone));
        else b.parts.push_back(box(vec3(X0, Y0, g), vec3(X1, Y1, top), stone));
        if (!round) {
            b.parts.push_back(windows(X0, Y0, X1, Y1, g + GF_H, top - 2.6f, 3.2f, GLASS, stone * 0.92f, 1.6f, 0.5f));
            // string courses between the stages
            for (float z = g + 4.0f; z < top - 1; z += 4.0f)
                b.parts.push_back(box(vec3(X0 - 0.08f, Y0 - 0.08f, z), vec3(X1 + 0.08f, Y1 + 0.08f, z + 0.25f), stone * 0.9f));
        }
        std::string kind = L.opt.count("top") ? L.opt.at("top") : "pyramid";
        float spireTop = top;
        if (kind == "battlement") {
            b.parts.push_back(box(vec3(X0 - 0.3f, Y0 - 0.3f, top - 0.8f), vec3(X1 + 0.3f, Y1 + 0.3f, top), stone * 0.95f));
            b.parts.push_back(crenel(X0 - 0.3f, Y0 - 0.3f, X1 + 0.3f, Y1 + 0.3f, top, 0.6f, stone));
        } else if (kind == "belfry") {
            // an open stage of arches, then a pyramid (St Mark's) or a cornice (Giotto's)
            float bh = 2.4f;
            for (int c = 0; c < 4; c++) {
                float px = (c & 1) ? X1 - 0.45f : X0, py = (c & 2) ? Y1 - 0.45f : Y0;
                b.parts.push_back(box(vec3(px, py, top), vec3(px + 0.45f, py + 0.45f, top + bh), stone));
            }
            b.parts.push_back(box(vec3(X0 - 0.15f, Y0 - 0.15f, top + bh), vec3(X1 + 0.15f, Y1 + 0.15f, top + bh + 0.5f), stone * 0.95f));
            b.parts.push_back(box(vec3(cx - 0.4f, cy - 0.4f, top), vec3(cx + 0.4f, cy + 0.4f, top + 1.2f), GOLD * 0.8f));   // the bell
            if (L.opt.count("spire")) {
                b.parts.push_back(box(vec3(X0 + 0.3f, Y0 + 0.3f, top + bh + 0.5f), vec3(X1 - 0.3f, Y1 - 0.3f, top + bh + 2.2f), stone));
                b.parts.push_back(roof(P_PYRAMID, X0 + 0.3f, Y0 + 0.3f, X1 - 0.3f, Y1 - 0.3f, top + bh + 2.2f, top + bh + 2.2f + 2.4f * half, 0, roofc, stone));
                b.parts.push_back(prism(cx, cy, top + bh + 2.2f + 2.4f * half, top + bh + 2.6f + 2.4f * half, 0.2f, 6, GOLD));   // the angel
            }
        } else if (kind == "cupola") {
            b.parts.push_back(prism(cx, cy, top, top + 1.4f, half * 0.75f, 8, stone));
            Part dm; dm.kind = P_DOME; dm.a = vec3(cx, cy, top + 1.4f); dm.b = vec3(half * 0.78f, 0, 0); dm.color = roofc; dm.sides = 8;
            b.parts.push_back(dm);
        } else if (kind == "bulb") {
            // a Dutch open lantern of wood, stage on stage, with an onion top (the Westerkerk, the Munttoren)
            vec3 wood(0.88f, 0.86f, 0.80f);
            float s1 = half * 0.85f, s2 = half * 0.6f;
            b.parts.push_back(prism(cx, cy, top, top + 2.0f, s1, 8, wood));
            b.parts.push_back(prism(cx, cy, top + 2.0f, top + 3.6f, s2, 8, wood));
            Part dm; dm.kind = P_DOME; dm.a = vec3(cx, cy, top + 3.6f); dm.b = vec3(s2 * 1.1f, 0, 0); dm.color = roofc; dm.sides = 8; dm.p1 = 1.3f;
            b.parts.push_back(dm);
            b.parts.push_back(cone(cx, cy, top + 3.6f + s2 * 1.4f, top + 6.0f + s2, 0.25f, 6, roofc));
            spireTop = top + 6.0f;
        } else if (kind == "spire") {
            b.parts.push_back(roof(P_PYRAMID, X0 - 0.1f, Y0 - 0.1f, X1 + 0.1f, Y1 + 0.1f, top, top + 4.0f * half, 0, roofc, stone));
        } else if (kind == "none" || kind == "flat") {
            b.parts.push_back(box(vec3(X0 - 0.2f, Y0 - 0.2f, top), vec3(X1 + 0.2f, Y1 + 0.2f, top + 0.4f), stone * 0.92f));
        } else {
            if (round) b.parts.push_back(cone(cx, cy, top, top + 1.6f * half, half + 0.2f, 14, roofc));
            else b.parts.push_back(roof(P_PYRAMID, X0 - 0.2f, Y0 - 0.2f, X1 + 0.2f, Y1 + 0.2f, top, top + (roofc.x > 0.6f ? 0.7f : 1.6f) * half, 0, roofc, stone));
        }
        (void)spireTop;
        // a stair to the top, when there is room for one (and the top is a platform)
        if (x1 - x0 >= 3 && y1 - y0 >= 3 && (kind == "battlement" || kind == "belfry" || kind == "flat" || kind == "none")) {
            float tz = kind == "belfry" ? top : top + (kind == "battlement" ? 0.0f : 0.4f);
            Level t = towerTop("the top of " + (L.name.empty() ? std::string("the tower") : L.name), id, x0, y0, x1, y1, tz, stone);
            t.wallH = kind == "belfry" ? 0.0f : 0.6f;
            int sx, sy, fx, fy;
            stairSpot(x0, y0, x1, y1, sx, sy, fx, fy);
            linkUp(t, sx, sy, fx, fy, sx, sy, fx, fy, F_STAIRS, F_STAIRS,
                   "You climb the long stair to the top of the tower.", "You go down the stair.");
            if (kind == "belfry") t.at((x0 + x1) / 2, (y0 + y1) / 2 + 1).feature = F_BELL;
            w.addLevel(std::move(t));
        }
    }

    // the campanile of Pisa: a round tower of a solid base, six open galleries and a belfry,
    // each storey set a little further south than the one below, so that it leans by `lean`
    // degrees. Walking is as for any tower (the tiles of its foot); the gallery round the
    // belfry, at the top, is a floor of its own, shifted south by the lean.
    void leaningTower(int id, const Legend &L, int x0, int y0, int x1, int y1, float top, float half, float lean, vec3 stone) {
        Building &b = B(id);
        float X0 = x0, X1 = x1 + 1, Y0 = y0, Y1 = y1 + 1, cx = (X0 + X1) * 0.5f, cy = (Y0 + Y1) * 0.5f;
        float tl = std::tan(lean * PI_F / 180.0f);
        auto off = [&](float z) { return -tl * (z - g); };   // towards the south (smaller y)
        const int SIDES = 20;
        float bel = 1.9f, shaft = top - bel;                // the belfry stands on the shaft
        int n = 7;                                           // a base and six galleries
        float h0 = (shaft - g) / n;
        for (int k = 0; k < n; k++) {
            float z0 = g + h0 * k, z1 = z0 + h0, zm = (z0 + z1) * 0.5f, oy = off(zm);
            if (k == 0) {
                b.parts.push_back(prism(cx, cy + oy, z0, z1, half, SIDES, stone));
            } else {
                // a gallery: a shadowed core, and a ring of slender columns carrying the cornice
                b.parts.push_back(prism(cx, cy + oy, z0, z1, half * 0.8f, SIDES, stone * 0.8f));
                for (int c = 0; c < 16; c++) {
                    float a = 2 * PI_F * (c + 0.5f) / 16, r = half - 0.09f;
                    float px = cx + r * std::cos(a), py = cy + oy + r * std::sin(a);
                    b.parts.push_back(box(vec3(px - 0.07f, py - 0.07f, z0), vec3(px + 0.07f, py + 0.07f, z1 - 0.16f), stone));
                }
            }
            b.parts.push_back(prism(cx, cy + off(z1), z1 - 0.16f, z1 + 0.04f, half + 0.08f, SIDES, stone * 0.95f));   // the cornice
        }
        // the belfry, narrower, and a low parapet round the top gallery
        float bo = off(shaft + bel * 0.5f);
        b.parts.push_back(prism(cx, cy + bo, shaft, top, half * 0.64f, SIDES, stone));
        b.parts.push_back(prism(cx, cy + off(top), top, top + 0.2f, half * 0.7f, SIDES, stone * 0.92f));
        for (int c = 0; c < 10; c++) {   // the bell openings
            float a = 2 * PI_F * (c + 0.5f) / 10, r = half * 0.64f + 0.01f;
            float px = cx + r * std::cos(a), py = cy + bo + r * std::sin(a);
            b.parts.push_back(box(vec3(px - 0.16f, py - 0.16f, shaft + 0.5f), vec3(px + 0.16f, py + 0.16f, top - 0.4f), vec3(0.16f, 0.15f, 0.14f)));
        }
        for (int c = 0; c < 24; c++) {
            float a = 2 * PI_F * (c + 0.5f) / 24, r = half - 0.05f;
            float px = cx + r * std::cos(a), py = cy + off(shaft) + r * std::sin(a);
            b.parts.push_back(box(vec3(px - 0.06f, py - 0.06f, shaft), vec3(px + 0.06f, py + 0.06f, shaft + 0.55f), stone));
        }
        // the gallery at the top: the ring of tiles round the belfry, shifted south by the lean
        if (x1 - x0 >= 3 && y1 - y0 >= 3) {
            int d = (int)std::lround(tl * (shaft - g));
            Level t = towerTop("the top of " + (L.name.empty() ? std::string("the tower") : L.name), id, x0, y0 - d, x1, y1 - d, shaft, stone);
            for (int X = x0; X <= x1; X++)
                for (int Y = y0 - d; Y <= y1 - d; Y++) {
                    bool corner = (X == x0 || X == x1) && (Y == y0 - d || Y == y1 - d);
                    if (corner) t.at(X, Y).terrain = T_VOID;   // outside the round of the gallery
                }
            t.at(x0 + 1, y1 - d - 1).feature = F_BELL;          // in the belfry, open to the gallery
            linkUp(t, x0 + 1, y0 + 1, x0 + 2, y0 + 1, x0 + 1, y0 - d, x0 + 2, y0 - d, F_STAIRS, F_STAIRS,
                   "You climb the stair that winds up inside the wall, leaning as you go, two hundred and ninety-four steps to the top.",
                   "You go down the long leaning stair.");
            w.addLevel(std::move(t));
        }
    }

    // a round (or many-sided) building: the Pantheon, the Baptistery, Castel Sant'Angelo.
    // sides=N, height=, roof=dome|pyramid|battlement, portico=west (a porch of columns),
    // stone=HEX, dome=HEX
    void rotunda(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        bool portico = L.opt.count("portico");
        bool north = portico && L.opt.at("portico") == "north";
        if (north) { rotundaNorth(cl, L); return; }
        int d = std::min(x1 - x0 + 1, y1 - y0 + 1);
        int rx1 = x1, rx0 = portico ? x1 - d + 1 : x0;   // the round part fills the east end
        int id = newBuilding(L.opt.count("fortress") ? "fortress" : "church", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        vec3 stone = stoneOf(L, PALE_STONE);
        b.wallColor = stone;
        float cx = (rx0 + rx1 + 1) * 0.5f, cy = (y0 + y1 + 1) * 0.5f, R = d * 0.5f;
        int sides = (int)L.num("sides", 20);
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++) {
                Tile &t = T(X, Y);
                float dd = std::hypot(X + 0.5f - cx, Y + 0.5f - cy);
                t.feature = F_NONE;
                if (X >= rx0 && dd <= R - 0.9f) { t.terrain = T_FLOOR_STONE; t.wall = W_NONE; t.bld = (uint16_t)id; }
                else if (X >= rx0 && dd <= R + 0.1f) { t.terrain = T_FLOOR_STONE; t.wall = W_WALL; t.bld = (uint16_t)id; }
                else if (portico && X < rx0 + (int)(R * 0.4f) && std::abs(Y + 0.5f - cy) <= R * 0.8f) { t.terrain = T_FLOOR_STONE; t.wall = W_NONE; t.bld = (uint16_t)id; }
                else { t.terrain = T_SQUARE; t.wall = W_NONE; t.bld = 0; }
            }
        // the door on the west, through the ring
        int ym = (int)std::floor(cy);
        for (int X = rx0; X < rx0 + 3; X++)
            for (int Y = ym - 1; Y <= ym; Y++) if (T(X, Y).wall == W_WALL) T(X, Y).wall = W_DOOR;
        if (!L.opt.count("fortress")) T((int)cx + (int)(R * 0.5f), ym).feature = F_ALTAR;
        float H = L.num("height", 6.0f);
        b.parts.push_back(prism(cx, cy, g, g + H, R, sides, stone));
        b.parts.push_back(prism(cx, cy, g + H - 0.3f, g + H + 0.2f, R + 0.2f, sides, stone * 0.93f));
        std::string r = L.opt.count("roof") ? L.opt.at("roof") : "dome";
        vec3 dc = L.opt.count("dome") ? hexColour(L.opt.at("dome")) : LEAD;
        if (r == "pyramid") {
            b.parts.push_back(cone(cx, cy, g + H + 0.2f, g + H + 0.2f + R * 0.45f, R + 0.2f, sides, dc));
            b.parts.push_back(prism(cx, cy, g + H + 0.2f + R * 0.4f, g + H + 1.6f + R * 0.4f, 0.6f, 8, stone));
        } else if (r == "battlement") {
            b.parts.push_back(prism(cx, cy, g + H + 0.2f, g + H + 0.9f, R - 0.1f, sides, stone));
            float rr = R * 0.45f;   // the keep on the drum, and the angel on top
            b.parts.push_back(box(vec3(cx - rr, cy - rr, g + H), vec3(cx + rr, cy + rr, g + H + 4.0f), stone * 0.97f));
            b.parts.push_back(windows(cx - rr, cy - rr, cx + rr, cy + rr, g + H + 0.6f, g + H + 4.0f, 1.7f, GLASS, stone, 1.4f, 0.55f));
            b.parts.push_back(crenel(cx - rr, cy - rr, cx + rr, cy + rr, g + H + 4.0f, 0.5f, stone));
            b.parts.push_back(prism(cx, cy, g + H + 4.0f, g + H + 5.4f, 0.28f, 6, vec3(0.45f, 0.52f, 0.48f)));
        } else {
            Part dm; dm.kind = P_DOME; dm.a = vec3(cx, cy, g + H + 0.2f); dm.b = vec3(R * 0.98f, 0, 0); dm.color = dc; dm.sides = sides;
            dm.p1 = L.num("domeheight", 0.55f);
            b.parts.push_back(dm);
        }
        if (portico) {
            // a porch of columns with a pediment, joined to the drum
            float px0 = x0, px1 = rx0 + R * 0.4f, py0 = cy - R * 0.8f, py1 = cy + R * 0.8f, pz = g + H * 0.7f;
            for (int k = 0; k < 8; k++) {
                float py = py0 + 0.4f + k * (py1 - py0 - 0.8f) / 7.0f;
                b.parts.push_back(prism(px0 + 0.5f, py, g, pz, 0.32f, 8, stone));
            }
            b.parts.push_back(box(vec3(px0, py0, pz), vec3(px1, py1, pz + 0.8f), stone));
            b.parts.push_back(roof(P_GABLE, px0 - 0.1f, py0, px1, py1, pz + 0.8f, pz + 0.8f + (py1 - py0) * 0.18f, 0, LEAD, stone));
        }
    }

    // the rotunda with its porch to the north (the Pantheon): built facing west, then turned
    // a quarter round by swapping the axes of its parts and tiles
    void rotundaNorth(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding("church", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        vec3 stone = stoneOf(L, PALE_STONE);
        b.wallColor = stone;
        int d = std::min(x1 - x0 + 1, y1 - y0 + 1);
        int ry0 = y0, ry1 = y0 + d - 1;   // the drum fills the south; the porch is north of it
        float cx = (x0 + x1 + 1) * 0.5f, cy = (ry0 + ry1 + 1) * 0.5f, R = d * 0.5f;
        int sides = (int)L.num("sides", 20);
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++) {
                Tile &t = T(X, Y);
                float dd = std::hypot(X + 0.5f - cx, Y + 0.5f - cy);
                t.feature = F_NONE;
                if (Y <= ry1 && dd <= R - 0.9f) { t.terrain = T_FLOOR_STONE; t.wall = W_NONE; t.bld = (uint16_t)id; }
                else if (Y <= ry1 && dd <= R + 0.1f) { t.terrain = T_FLOOR_STONE; t.wall = W_WALL; t.bld = (uint16_t)id; }
                else if (Y > ry1 - (int)(R * 0.4f) && std::abs(X + 0.5f - cx) <= R * 0.8f) { t.terrain = T_FLOOR_STONE; t.wall = W_NONE; t.bld = (uint16_t)id; }
                else { t.terrain = T_SQUARE; t.wall = W_NONE; t.bld = 0; }
            }
        int xm = (int)std::floor(cx);
        for (int Y = ry1 - 2; Y <= ry1; Y++)
            for (int X = xm - 1; X <= xm; X++) if (T(X, Y).wall == W_WALL) T(X, Y).wall = W_DOOR;
        T(xm, (int)cy - (int)(R * 0.5f)).feature = F_ALTAR;
        float H = L.num("height", 6.0f);
        b.parts.push_back(prism(cx, cy, g, g + H, R, sides, stone));
        b.parts.push_back(prism(cx, cy, g + H - 0.3f, g + H + 0.2f, R + 0.2f, sides, stone * 0.93f));
        vec3 dc = L.opt.count("dome") ? hexColour(L.opt.at("dome")) : LEAD;
        Part dm; dm.kind = P_DOME; dm.a = vec3(cx, cy, g + H + 0.2f); dm.b = vec3(R * 0.98f, 0, 0); dm.color = dc; dm.sides = sides;
        dm.p1 = L.num("domeheight", 0.55f);
        b.parts.push_back(dm);
        b.parts.push_back(prism(cx, cy, g + H + 0.2f + R * dm.p1 - 0.2f, g + H + 0.3f + R * dm.p1, R * 0.18f, 12, stone));   // the oculus ring
        // the porch: sixteen granite columns under a pediment
        float px0 = cx - R * 0.8f, px1 = cx + R * 0.8f, py0 = ry1 + 1 - R * 0.4f, py1 = y1 + 1, pz = g + H * 0.7f;
        for (int k = 0; k < 8; k++) {
            float px = px0 + 0.4f + k * (px1 - px0 - 0.8f) / 7.0f;
            b.parts.push_back(prism(px, py1 - 0.5f, g, pz, 0.32f, 8, vec3(0.62f, 0.6f, 0.58f)));
        }
        b.parts.push_back(box(vec3(px0, py0, pz), vec3(px1, py1, pz + 0.8f), stone));
        b.parts.push_back(roof(P_GABLE, px0, py0, px1, py1 + 0.1f, pz + 0.8f, pz + 0.8f + (px1 - px0) * 0.18f, 1, LEAD, stone));
    }

    // an amphitheatre (the Colosseum): an elliptical ring of arcades round an arena of sand.
    // height=, ruined=0..1, stone=HEX
    void arena(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding("arena", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        vec3 stone = stoneOf(L, vec3(0.80f, 0.72f, 0.56f));
        b.wallColor = stone;
        float cx = (x0 + x1 + 1) * 0.5f, cy = (y0 + y1 + 1) * 0.5f, rx = (x1 - x0 + 1) * 0.5f, ry = (y1 - y0 + 1) * 0.5f;
        float t = std::max(3.0f, std::min(rx, ry) * 0.3f);
        for (int X = x0; X <= x1; X++)
            for (int Y = y0; Y <= y1; Y++) {
                Tile &tl = T(X, Y);
                float u = (X + 0.5f - cx), v = (Y + 0.5f - cy);
                float eo = (u * u) / (rx * rx) + (v * v) / (ry * ry), ei = (u * u) / ((rx - t) * (rx - t)) + (v * v) / ((ry - t) * (ry - t));
                tl.feature = F_NONE;
                if (eo > 1) { tl.terrain = T_SQUARE; tl.wall = W_NONE; tl.bld = 0; continue; }
                tl.bld = (uint16_t)id;
                if (ei <= 1) { tl.terrain = T_YARD; tl.wall = W_NONE; continue; }
                bool gate = std::fabs(v) < 1.2f || std::fabs(u) < 1.2f;   // the four great gates
                tl.terrain = T_FLOOR_STONE;
                tl.wall = gate ? W_NONE : W_WALL;
            }
        Part p; p.kind = P_ELLRING; p.a = vec3(cx, cy, g); p.b = vec3(rx, ry, L.num("height", 8.0f));
        p.p1 = t; p.p2 = L.num("ruined", 0.0f); p.sides = 64; p.color = stone; p.color2 = stone * 0.45f;
        b.parts.push_back(p);
    }

    // a church of domes on a Greek cross (St Mark's): five domes, a front of arches
    void basilica(const Cluster &cl, const Legend &L) {
        int x0 = cl.x0, y0 = cl.y0, x1 = cl.x1, y1 = cl.y1;
        int id = newBuilding("cathedral", L.name, x0, y0, x1, y1);
        Building &b = B(id);
        vec3 stone = stoneOf(L, vec3(0.90f, 0.85f, 0.76f));
        vec3 dc = L.opt.count("dome") ? hexColour(L.opt.at("dome")) : LEAD;
        b.wallColor = stone;
        setFloor(id, x0, y0, x1, y1, T_FLOOR_TILE);
        ringWalls(id, x0, y0, x1, y1, 4);
        int ym = (y0 + y1) / 2;
        for (int k = -1; k <= 1; k++) T(x0, ym + k).wall = W_DOOR;
        door(x0, y0, x1, y1, 2, true);
        door(x0, y0, x1, y1, 3, true);
        T(x1 - 2, ym).feature = F_ALTAR;
        float X0 = x0, X1 = x1 + 1, Y0 = y0, Y1 = y1 + 1, cx = (X0 + X1) * 0.5f, cy = (Y0 + Y1) * 0.5f;
        float H = L.num("height", 5.5f), zb = g;
        for (int X = x0 + 3; X <= x1 - 3; X += 4)
            for (int Y = y0 + 3; Y <= y1 - 3; Y += 4)
                if (T(X, Y).feature == F_NONE && std::abs(Y - ym) > 1) T(X, Y).feature = F_PILLAR;
        b.parts.push_back(box(vec3(X0 + 1.4f, Y0, zb), vec3(X1, Y1, zb + H), stone));
        // the front: a row of five great arches with gilded mosaics, and pinnacles
        b.parts.push_back(box(vec3(X0, Y0 - 0.2f, zb), vec3(X0 + 1.6f, Y1 + 0.2f, zb + H + 0.6f), stone * 1.02f));
        for (int k = 0; k < 5; k++) {
            float yy0 = Y0 + 0.3f + k * (Y1 - Y0 - 0.6f) / 5.0f, yy1 = yy0 + (Y1 - Y0 - 0.6f) / 5.0f - 0.4f;
            b.parts.push_back(box(vec3(X0 - 0.03f, yy0 + 0.2f, zb + 0.3f), vec3(X0, yy1, zb + H * 0.55f), vec3(0.25f, 0.2f, 0.18f)));
            b.parts.push_back(box(vec3(X0 - 0.03f, yy0 + 0.2f, zb + H * 0.62f), vec3(X0, yy1, zb + H + 0.3f), GOLD));
            b.parts.push_back(cone(X0 + 0.8f, yy1 + 0.2f, zb + H + 0.6f, zb + H + 2.2f, 0.25f, 6, stone));
        }
        // five domes: over the crossing and the four arms
        float R = std::min(X1 - X0, Y1 - Y0) * 0.17f;
        const float off[5][2] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (int k = 0; k < 5; k++) {
            float dx = cx + off[k][0] * (X1 - X0) * 0.3f, dy = cy + off[k][1] * (Y1 - Y0) * 0.3f;
            float rr = k == 0 ? R * 1.15f : R;
            b.parts.push_back(prism(dx, dy, zb + H, zb + H + 1.0f, rr * 0.95f, 12, stone));
            Part dm; dm.kind = P_DOME; dm.a = vec3(dx, dy, zb + H + 1.0f); dm.b = vec3(rr, 0, 0); dm.color = dc; dm.sides = 12; dm.p1 = 1.25f;
            b.parts.push_back(dm);
            float tz = zb + H + 1.0f + rr * 1.25f;
            b.parts.push_back(prism(dx, dy, tz - 0.1f, tz + 0.8f, 0.35f, 6, dc));
            b.parts.push_back(prism(dx, dy, tz + 0.8f, tz + 1.4f, 0.1f, 4, GOLD));
        }
    }

    void landmark(const Cluster &cl, const Legend &L) {
        const std::string &k = L.kind;
        if (k == "tower") { tower(cl, L); return; }
        if (k == "rotunda") { rotunda(cl, L); return; }
        if (k == "arena") { arena(cl, L); return; }
        if (k == "basilica") { basilica(cl, L); return; }
        if (k == "mosque") { mosque(cl, L); return; }
        if (k == "theatre") { theatre(cl, L); return; }
        if (k == "bridgehouses") { bridgeHouses(cl, L); return; }
        if (k == "cathedral") cathedral(cl, L, false);
        else if (k == "church" || k == "abbey") cathedral(cl, L, true);
        else if (k == "chapel") chapel(cl, L);
        else if (k == "palace" || k == "college") palace(cl, L, k);
        else if (k == "gallery") gallery(cl, L);
        else if (k == "fortress") fortress(cl, L);
        else if (k == "hall") hall(cl, L);
        else if (k == "halle") halle(cl, L);
        else if (k == "place") place(cl, L);
        else if (k == "mansion") {
            house(cl.x0, cl.y0, cl.x1, cl.y1, L.opt.count("style") ? w.styleIndex(L.opt.at("style")) : -1, L.name, 3, 4);
        } else if (k == "inn") {
            int before = (int)w.buildings.size();
            house(cl.x0, cl.y0, cl.x1, cl.y1, -1, L.name, 2, 3, true);
            w.buildings[before].kind = "inn";
            // the common room: tables with benches in rows, and a counter. No random numbers
            // here, so the rest of the town is unchanged.
            for (int Y = cl.y0 + 2; Y < cl.y1 - 1; Y += 4)
                for (int X = cl.x0 + 2; X < cl.x1 - 1; X += 3) {
                    if (T(X, Y).feature != F_NONE || T(X, Y - 1).feature != F_NONE || T(X, Y + 1).feature != F_NONE) continue;
                    if (!put(cl.x0, cl.y0, cl.x1, cl.y1, X, Y, F_TABLE, 0)) continue;
                    put(cl.x0, cl.y0, cl.x1, cl.y1, X, Y - 1, F_BENCH, 0);
                    put(cl.x0, cl.y0, cl.x1, cl.y1, X, Y + 1, F_BENCH, 0);
                }
            bool counter = false;
            for (int X = cl.x0 + 1; X < cl.x1 && !counter; X++)
                counter = put(cl.x0, cl.y0, cl.x1, cl.y1, X, cl.y1 - 1, F_COUNTER, 0);
            backDoor(cl.x0, cl.y0, cl.x1, cl.y1);
        } else {
            int before = (int)w.buildings.size();
            bool home = L.opt.count("home") > 0;   // two storeys: your parents' beds below, yours under the eaves
            if (home) { wantBeds[0] = 2; wantBeds[1] = 1; }
            house(cl.x0, cl.y0, cl.x1, cl.y1, -1, L.name, home ? 2 : 0, home ? 2 : 0);
            wantBeds[0] = wantBeds[1] = 0;
            if (L.opt.count("home") && before < (int)w.buildings.size()) w.buildings[before].home = true;   // the player's home
        }
    }

    // ---------------------------------------------------------- house blocks
    void houses() {
        const Style &hs = style();
        const int MINW = hs.lotWMin, MAXW = hs.lotWMax, DMIN = hs.lotDMin, DMAX = hs.lotDMax;
        const int MERGE = std::max(13, DMAX + 4), MIN_HOUSE = 4;
        std::vector<int> owner(s.w * s.h, 0);   // 0 free, -1 not a house tile, >0 lot number
        for (int i = 0; i < s.w * s.h; i++) owner[i] = (s.chars[i] == 'h') ? 0 : -1;
        struct Lot { int c0, r0, c1, r1; };
        std::vector<Lot> lots;
        auto isFree = [&](int c, int r) {
            return c >= 0 && r >= 0 && c < s.w && r < s.h && owner[r * s.w + c] == 0;
        };
        for (int r = 0; r < s.h; r++)
            for (int c = 0; c < s.w; c++) {
                if (!isFree(c, r)) continue;
                int wt = rng.range(MINW, MAXW), dt = rng.range(DMIN, DMAX);
                int wc = 0;
                while (wc < wt && isFree(c + wc, r)) wc++;
                int rest = 0;
                while (isFree(c + wc + rest, r) && rest < MIN_HOUSE + 1) rest++;
                if (rest > 0 && rest < MIN_HOUSE + 1 && wc + rest <= MERGE) wc += rest;
                int dc = 1;
                auto rowFree = [&](int rr) {
                    for (int k = 0; k < wc; k++) if (!isFree(c + k, rr)) return false;
                    return true;
                };
                while (dc < dt && rowFree(r + dc)) dc++;
                int drest = 0;
                while (rowFree(r + dc + drest) && drest < MIN_HOUSE + 1) drest++;
                if (drest > 0 && drest < MIN_HOUSE + 1 && dc + drest <= MERGE) dc += drest;
                int lot = (int)lots.size() + 1;
                for (int k = 0; k < wc; k++)
                    for (int j = 0; j < dc; j++) owner[(r + j) * s.w + c + k] = lot;
                lots.push_back({c, r, c + wc - 1, r + dc - 1});
            }
        // merge slivers into a neighbouring lot when the two make a rectangle
        {
            std::vector<bool> dead(lots.size(), false);
            for (int pass = 0; pass < 3; pass++)
                for (size_t i = 0; i < lots.size(); i++) {
                    if (dead[i]) continue;
                    Lot &S = lots[i];
                    int sw = S.c1 - S.c0 + 1, sd = S.r1 - S.r0 + 1;
                    if (sw >= MIN_HOUSE && sd >= MIN_HOUSE) continue;
                    for (size_t j = 0; j < lots.size(); j++) {
                        if (j == i || dead[j]) continue;
                        Lot &N = lots[j];
                        bool rowsSame = N.r0 == S.r0 && N.r1 == S.r1;
                        bool colsSame = N.c0 == S.c0 && N.c1 == S.c1;
                        bool side = rowsSame && (N.c1 + 1 == S.c0 || S.c1 + 1 == N.c0);
                        bool above = colsSame && (N.r1 + 1 == S.r0 || S.r1 + 1 == N.r0);
                        if (!side && !above) continue;
                        Lot U{std::min(S.c0, N.c0), std::min(S.r0, N.r0), std::max(S.c1, N.c1), std::max(S.r1, N.r1)};
                        if (U.c1 - U.c0 + 1 > MERGE + 2 || U.r1 - U.r0 + 1 > MERGE + 2) continue;
                        N = U;
                        dead[i] = true;
                        break;
                    }
                }
            std::vector<Lot> kept;
            for (size_t i = 0; i < lots.size(); i++) if (!dead[i]) kept.push_back(lots[i]);
            lots.swap(kept);
        }
        auto worldX = [&](int c) { return s.x0 + c; };
        auto worldY = [&](int r) { return s.y0 + s.h - 1 - r; };
        std::vector<int> kind(lots.size(), 0);   // 1 house, 2 courtyard
        for (size_t i = 0; i < lots.size(); i++) {
            auto &L = lots[i];
            int wc = L.c1 - L.c0 + 1, dc = L.r1 - L.r0 + 1;
            if (wc < MIN_HOUSE || dc < MIN_HOUSE) { kind[i] = 2; continue; }
            bool touches = false;
            for (int c = L.c0 - 1; c <= L.c1 + 1; c++)
                for (int r = L.r0 - 1; r <= L.r1 + 1; r++) {
                    bool edge = c == L.c0 - 1 || c == L.c1 + 1 || r == L.r0 - 1 || r == L.r1 + 1;
                    bool corner = (c == L.c0 - 1 || c == L.c1 + 1) && (r == L.r0 - 1 || r == L.r1 + 1);
                    if (!edge || corner) continue;
                    if (c < 0 || r < 0 || c >= s.w || r >= s.h) { touches = true; continue; }
                    if (s.chars[r * s.w + c] != 'h' && open(worldX(c), worldY(r))) touches = true;
                }
            kind[i] = touches ? 1 : 2;
        }
        if (getenv("GALILEO_DEBUG_LOTS")) {
            int nh = 0, nc = 0;
            for (size_t i = 0; i < lots.size(); i++) (kind[i] == 1 ? nh : nc)++;
            fprintf(stderr, "%s: %zu lots, %d houses, %d courtyards\n", s.name.c_str(), lots.size(), nh, nc);
        }
        // courtyards first, so that houses can open doors onto them
        for (size_t i = 0; i < lots.size(); i++) {
            if (kind[i] != 2) continue;
            auto &L = lots[i];
            for (int c = L.c0; c <= L.c1; c++)
                for (int r = L.r0; r <= L.r1; r++) {
                    Tile &t = s.tiles[r * s.w + c];
                    t.terrain = (hash2i(c / 3, r / 3, 9) % 3 == 0) ? T_GARDEN : T_YARD;
                    t.wall = W_NONE;
                    uint32_t hh = hash2i(c, r, 10) % 40;
                    t.feature = hh == 0 ? F_FRUIT : (hh == 1 ? F_BARREL : (hh == 2 ? F_HAY : (hh == 3 ? F_WELL : F_NONE)));
                    s.chars[r * s.w + c] = ';';
                }
        }
        for (size_t i = 0; i < lots.size(); i++) {
            if (kind[i] != 1) continue;
            auto &L = lots[i];
            house(worldX(L.c0), worldY(L.r1), worldX(L.c1), worldY(L.r0));
        }
    }

    void run(const std::map<char, Legend> &legend, const std::map<char, std::string> &gateNames) {
        baseTiles();
        waterExits();
        wallTowers();
        gates(gateNames);
        std::map<char, int> seenLetter;
        // landmarks: capital letters (but O, the wall towers), and any other letter the legend names
        for (auto &cl : clusters([&](char c) { return (c >= 'A' && c <= 'Z' && c != 'O') || (c >= 'a' && c <= 'z' && legend.count(c)); })) {
            char c = ch(cl.tiles[0].first, cl.tiles[0].second);
            auto it = legend.find(c);
            Legend L;
            if (it != legend.end()) L = it->second;
            else { L.kind = "house"; }
            // several clusters of one letter: names=First_name|Second_name|...
            int nth = seenLetter[c]++;
            if (L.opt.count("names")) {
                std::vector<std::string> names;
                std::string all = L.opt.at("names"), cur;
                for (char x : all) { if (x == '|') { names.push_back(cur); cur.clear(); } else cur += (x == '_' ? ' ' : x); }
                names.push_back(cur);
                if (nth < (int)names.size()) L.name = names[nth];
            }
            int firstNew = (int)w.buildings.size();
            landmark(cl, L);
            if (L.opt.count("sign"))
                for (int b = firstNew; b < (int)w.buildings.size(); b++) legendSigns.push_back({b + 1, L.opt.at("sign")});
            for (auto &p : cl.tiles) {
                int col = p.first - s.x0, row = s.y0 + s.h - 1 - p.second;
                s.chars[row * s.w + col] = '*';
            }
        }
        houses();
        unblockRooms();
        ferries();
        wallWalk();
        for (auto &ls : legendSigns) w.addSign(ls.first, ls.second);
    }

    // Ferries (the traghetti of Venice, the watermen's stairs of London): at a landing on
    // each side a ferry boat waits; walk into it and it takes you to the other landing.
    // They are links, like ladders, so the people of the town use them too.
    void ferries() {
        for (auto &f : ferryList) {
            int X1 = s.x0 + f.c1, Y1 = s.y0 + s.h - 1 - f.r1, X2 = s.x0 + f.c2, Y2 = s.y0 + s.h - 1 - f.r2;
            int wx1, wy1, wx2, wy2;
            if (!landing(X1, Y1, wx1, wy1, f.gondola) || !landing(X2, Y2, wx2, wy2, f.gondola)) {
                std::cerr << s.name << ": no landing found for the ferry between " << f.a << " and " << f.b << "\n";
                continue;
            }
            w.addLink(wx1, wy1, 0, X2, Y2, 0, "The ferryman rows you across to " + f.b + ".");
            w.addLink(wx2, wy2, 0, X1, Y1, 0, "The ferryman rows you across to " + f.a + ".");
        }
    }
    // a landing near X,Y: open ground with water beside it, where nothing is moored yet.
    // X,Y become the landing, WX,WY the water where the ferry boat waits.
    bool landing(int &X, int &Y, int &WX, int &WY, bool gondola) {
        const int d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (int r = 0; r < 14; r++)
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    int x = X + dx, y = Y + dy;
                    if (!in(x, y) || ch(x, y) == ' ') continue;
                    Tile &t = T(x, y);
                    if (t.terrain == T_WATER || t.terrain == T_DEEP || t.terrain == T_BRIDGE || t.wall != W_NONE || t.feature != F_NONE || t.bld) continue;
                    for (int k = 0; k < 4; k++) {
                        int nx = x + d4[k][0], ny = y + d4[k][1];
                        if (!in(nx, ny)) continue;
                        Tile &n = T(nx, ny);
                        if (n.terrain != T_WATER || n.feature != F_NONE || w.linkAt(nx, ny, 0)) continue;
                        X = x; Y = y; WX = nx; WY = ny;
                        n.feature = F_MOORING;
                        n.var = (uint8_t)(2 + (k < 2 ? 0 : 1) + (gondola ? 0 : 2));   // the boat lies along the bank
                        return true;
                    }
                }
        return false;
    }

    // Furniture must not cut part of a building off from its doors. After furnishing, find
    // the open tiles of each building that cannot be reached from a door to the outside, and
    // take away, one at a time, the piece of furniture that stands in the way (a bed last).
    void unblockRooms() {
        auto rank = [](int f) {   // which pieces may go, the most willingly first; 0: none
            switch (f) {
                case F_TABLE: return 5; case F_CHEST: return 5; case F_BARREL: return 5;
                case F_COUNTER: return 4; case F_HEARTH: return 3; case F_BED: return 1;
                default: return 0;
            }
        };
        int fixed = 0;
        auto pass = [&](int X, int Y) {   // as World::passable, but from this town's own tiles (it is not in the world yet)
            if (!in(X, Y) || ch(X, Y) == ' ') return w.passable(X, Y);
            const Tile &t = T(X, Y);
            if (t.terrain == T_WATER || t.terrain == T_DEEP || t.terrain == T_ROCK || t.terrain == T_SNOW) return false;
            if (t.wall != W_NONE && t.wall != W_DOOR && t.wall != W_GATE) return false;
            return !featureBlocksWalk(t.feature);
        };
        auto diag = [&](int X, int Y) {
            if (!in(X, Y) || ch(X, Y) == ' ') return w.blocksDiagonal(X, Y);
            const Tile &t = T(X, Y);
            return t.wall != W_NONE && t.wall != W_DOOR && t.wall != W_GATE;
        };
        for (size_t i = 0; i < w.buildings.size(); i++) {
            const Building &b = w.buildings[i];
            if (b.x0 < s.x0 || b.y0 < s.y0 || b.x0 >= s.x0 + s.w || b.y0 >= s.y0 + s.h) continue;
            int id = (int)i + 1, bw = b.x1 - b.x0 + 1, bh = b.y1 - b.y0 + 1;
            auto mine = [&](int X, int Y) { return X >= b.x0 && X <= b.x1 && Y >= b.y0 && Y <= b.y1 && T(X, Y).bld == id; };
            for (int X = b.x0; X <= b.x1; X++)   // nothing stands in a doorway
                for (int Y = b.y0; Y <= b.y1; Y++)
                    if (mine(X, Y) && (T(X, Y).wall == W_DOOR || T(X, Y).wall == W_GATE) && featureBlocksWalk(T(X, Y).feature)) {
                        T(X, Y).feature = F_NONE;
                        fixed++;
                    }
            for (int round = 0; round < 16; round++) {
                std::vector<char> seen(bw * bh, 0);
                std::deque<std::pair<int, int>> q;
                auto at = [&](int X, int Y) -> char & { return seen[(Y - b.y0) * bw + (X - b.x0)]; };
                const int d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                for (int X = b.x0; X <= b.x1; X++)
                    for (int Y = b.y0; Y <= b.y1; Y++) {
                        if (!mine(X, Y) || (T(X, Y).wall != W_DOOR && T(X, Y).wall != W_GATE)) continue;
                        bool outer = false;
                        for (auto &d : d4) {
                            int nx = X + d[0], ny = Y + d[1];
                            if (!mine(nx, ny) && pass(nx, ny)) outer = true;
                        }
                        if (outer) { at(X, Y) = 1; q.push_back({X, Y}); }
                    }
                if (q.empty()) { if (getenv("GALILEO_DEBUG_UNBLOCK") && round == 0 && b.kind == "house") std::fprintf(stderr, "  building %d: no door to the outside\n", id); break; }
                while (!q.empty()) {
                    auto [x, y] = q.front(); q.pop_front();
                    for (int dx = -1; dx <= 1; dx++)
                        for (int dy = -1; dy <= 1; dy++) {
                            if (!dx && !dy) continue;
                            int nx = x + dx, ny = y + dy;
                            if (!mine(nx, ny) || at(nx, ny) || !pass(nx, ny)) continue;
                            if (dx && dy && (diag(x + dx, y) || diag(x, y + dy))) continue;
                            if (dx && dy && !pass(x + dx, y) && !pass(x, y + dy)) continue;
                            at(nx, ny) = 1;
                            q.push_back({nx, ny});
                        }
                }
                std::vector<std::pair<int, int>> lost;
                for (int X = b.x0; X <= b.x1; X++)
                    for (int Y = b.y0; Y <= b.y1; Y++)
                        if (mine(X, Y) && pass(X, Y) && !at(X, Y)) lost.push_back({X, Y});
                if (lost.empty()) break;
                if (getenv("GALILEO_DEBUG_UNBLOCK")) std::fprintf(stderr, "  building %d: %zu tiles shut off (round %d)\n", id, lost.size(), round);
                // the piece to take: beside a reached tile; best if it also touches a lost one,
                // otherwise the nearest to one; among equals, the most willingly taken
                int bx = -1, by = -1;
                float best = 1e9f;
                for (int X = b.x0; X <= b.x1; X++)
                    for (int Y = b.y0; Y <= b.y1; Y++) {
                        if (!mine(X, Y) || T(X, Y).wall != W_NONE || !rank(T(X, Y).feature)) continue;
                        bool byReached = false;
                        for (auto &d : d4) if (mine(X + d[0], Y + d[1]) && at(X + d[0], Y + d[1])) byReached = true;
                        if (!byReached) continue;
                        float dist = 1e9f;
                        for (auto &l : lost) dist = std::min(dist, (float)(std::abs(l.first - X) + std::abs(l.second - Y)));
                        float score = dist * 10 - rank(T(X, Y).feature);
                        if (score < best) { best = score; bx = X; by = Y; }
                    }
                if (bx < 0) break;
                T(bx, by).feature = F_NONE;
                fixed++;
            }
        }
        if (getenv("GALILEO_DEBUG_UNBLOCK") && fixed) std::fprintf(stderr, "%s: took away %d pieces of furniture that shut off rooms\n", s.name.c_str(), fixed);
    }

    // the walk along the top of the town walls, with ladders up from the streets
    void wallWalk() {
        auto kindOf = [&](int bld) { return bld ? w.buildings[bld - 1].kind : std::string(); };
        auto onWall = [&](const Tile &t) {
            std::string k = kindOf(t.bld);
            if (t.wall == W_CITYWALL) return t.bld == 0 || k == "tower" || k == "gate";
            if (t.wall == W_TOWER || t.wall == W_GATE) return k == "tower" || k == "gate";
            return false;
        };
        Level L;
        L.name = "the town walls";
        L.z = 1;
        L.init(s.x0, s.y0, s.x0 + s.w - 1, s.y0 + s.h - 1);
        L.floor = g + TOWNWALL_H;
        L.wallH = 0;
        L.drawFloor = false;
        L.drawWalls = false;
        L.hideBuilding = false;
        int n = 0;
        for (int X = s.x0; X < s.x0 + s.w; X++)
            for (int Y = s.y0; Y < s.y0 + s.h; Y++)
                if (onWall(T(X, Y))) { L.at(X, Y).terrain = T_FLOOR_STONE; n++; }
        if (n == 0) return;
        vec2 c(s.x0 + s.w * 0.5f, s.y0 + s.h * 0.5f);
        std::vector<std::pair<int, int>> ladders;
        const int d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        auto streetish = [&](int X, int Y) {
            if (!in(X, Y) || ch(X, Y) == ' ') return false;
            Tile &t = T(X, Y);
            return t.wall == W_NONE && t.feature == F_NONE && t.bld == 0 &&
                   (t.terrain == T_STREET || t.terrain == T_SQUARE || t.terrain == T_YARD || t.terrain == T_GARDEN);
        };
        for (int Y = s.y0; Y < s.y0 + s.h; Y++)
            for (int X = s.x0; X < s.x0 + s.w; X++) {
                if (T(X, Y).wall != W_CITYWALL || T(X, Y).bld != 0) continue;
                bool near = false;
                for (auto &l : ladders) if (std::abs(l.first - X) + std::abs(l.second - Y) < 40) near = true;
                if (near) continue;
                for (int sd = 0; sd < 4; sd++) {
                    // the ladder stands at S, leaning on the wall at (X,Y); one climbs it from F
                    int sx = X - d4[sd][0], sy = Y - d4[sd][1], fx = sx - d4[sd][0], fy = sy - d4[sd][1];
                    if (dot(vec2((float)(sx - X), (float)(sy - Y)), c - vec2((float)X, (float)Y)) <= 0) continue;   // town side
                    if (!streetish(sx, sy) || !streetish(fx, fy)) continue;
                    T(sx, sy).feature = F_LADDER;
                    T(sx, sy).var = (uint8_t)(sd | (1 << 2));
                    L.at(sx, sy).terrain = T_FLOOR_WOOD;
                    L.at(sx, sy).feature = F_LADDER;
                    L.at(sx, sy).var = (uint8_t)(sd | (1 << 2));
                    w.addLink(sx, sy, 0, X, Y, 1, "You climb the ladder onto the town walls.");
                    w.addLink(sx, sy, 1, fx, fy, 0, "You climb down the ladder.");
                    ladders.push_back({X, Y});
                    break;
                }
            }
        // every separate stretch of wall gets a ladder of its own
        {
            std::set<std::pair<int, int>> seen;
            for (int Y = s.y0; Y < s.y0 + s.h; Y++)
                for (int X = s.x0; X < s.x0 + s.w; X++) {
                    if (L.at(X, Y).terrain != T_FLOOR_STONE || seen.count({X, Y})) continue;
                    std::vector<std::pair<int, int>> comp, st{{X, Y}};
                    seen.insert({X, Y});
                    bool hasLadder = false;
                    while (!st.empty()) {
                        auto p = st.back(); st.pop_back();
                        comp.push_back(p);
                        for (int dx = -1; dx <= 1; dx++)
                            for (int dy = -1; dy <= 1; dy++) {
                                int a = p.first + dx, b2 = p.second + dy;
                                if (!L.inside(a, b2)) continue;
                                if (L.at(a, b2).feature == F_LADDER && w.linkAt(a, b2, 1)) {
                                    const Link *lk = w.linkAt(a, b2, 0);
                                    if (lk && std::abs(lk->x - p.first) + std::abs(lk->y - p.second) <= 1) hasLadder = true;
                                }
                                if (L.at(a, b2).terrain != T_FLOOR_STONE || seen.count({a, b2})) continue;
                                seen.insert({a, b2});
                                st.push_back({a, b2});
                            }
                    }
                    for (auto &l : ladders)
                        for (auto &p : comp)
                            if (p == l) hasLadder = true;
                    if (hasLadder || comp.size() < 6) continue;
                    for (size_t k = comp.size() / 2, tries = 0; tries < comp.size(); tries++, k = (k + 7) % comp.size()) {
                        int Xw = comp[k].first, Yw = comp[k].second;
                        if (T(Xw, Yw).wall != W_CITYWALL) continue;
                        bool placed = false;
                        for (int sd = 0; sd < 4 && !placed; sd++) {
                            int sx = Xw - d4[sd][0], sy = Yw - d4[sd][1], fx = sx - d4[sd][0], fy = sy - d4[sd][1];
                            if (!streetish(sx, sy) || !streetish(fx, fy)) continue;
                            T(sx, sy).feature = F_LADDER;
                            T(sx, sy).var = (uint8_t)(sd | (1 << 2));
                            L.at(sx, sy).terrain = T_FLOOR_WOOD;
                            L.at(sx, sy).feature = F_LADDER;
                            L.at(sx, sy).var = (uint8_t)(sd | (1 << 2));
                            w.addLink(sx, sy, 0, Xw, Yw, 1, "You climb the ladder onto the town walls.");
                            w.addLink(sx, sy, 1, fx, fy, 0, "You climb down the ladder.");
                            ladders.push_back({Xw, Yw});
                            placed = true;
                        }
                        if (placed) break;
                    }
                }
        }
        if (!ladders.empty()) w.addLevel(std::move(L));
    }
};

}  // namespace

bool loadSettlement(World &w, Settlement &s, const std::string &fn, int cx, int cy) {
    std::ifstream in(fn);
    if (!in) { std::cerr << "cannot open " << fn << "\n"; return false; }
    std::string line, styleName;
    std::map<char, Legend> legend;
    std::map<char, std::string> gateNames;
    std::vector<TownBuilder::Ferry> ferries;
    std::vector<std::string> rows;
    bool inMap = false;
    float towerRoof = 1;
    s.margin = 14 * w.kS;
    while (std::getline(in, line)) {
        if (inMap) {
            if (line == "end") { inMap = false; continue; }
            rows.push_back(line);
            continue;
        }
        std::istringstream ss(line);
        std::string w0;
        if (!(ss >> w0) || w0[0] == '#') continue;
        if (w0 == "map") { inMap = true; continue; }
        if (w0 == "style") ss >> styleName;
        else if (w0 == "ground") ss >> s.ground;
        else if (w0 == "margin") ss >> s.margin;
        else if (w0 == "towerroof") { std::string t; ss >> t; towerRoof = (t == "cone") ? 1 : 0; }
        else if (w0 == "camp") { std::string u; ss >> s.campEvent >> u >> s.campUntil; }   // camp EVENT [until EVENT]: its tents and guns stand only between
        else if (w0 == "legend") {
            std::string c; Legend L;
            ss >> c >> L.kind >> std::quoted(L.name);
            std::string o;
            while (ss >> o) {
                auto eq = o.find('=');
                if (eq == std::string::npos) L.opt[o] = "yes";
                else L.opt[o.substr(0, eq)] = o.substr(eq + 1);
            }
            legend[c[0]] = L;
        } else if (w0 == "gate") {
            std::string c, nm;
            ss >> c >> std::quoted(nm);
            gateNames[c[0]] = nm;
        } else if (w0 == "ferry") {
            // ferry COL1 ROW1 COL2 ROW2 gondola|boat NAME ONE | NAME TWO   (near which tiles of the map)
            TownBuilder::Ferry f;
            std::string kind, rest;
            ss >> f.c1 >> f.r1 >> f.c2 >> f.r2 >> kind;
            std::getline(ss, rest);
            size_t bar = rest.find('|');
            auto trimmed = [](std::string x) { size_t a = x.find_first_not_of(' '), b = x.find_last_not_of(' '); return a == std::string::npos ? std::string() : x.substr(a, b - a + 1); };
            f.a = trimmed(rest.substr(0, bar));
            f.b = bar == std::string::npos ? f.a : trimmed(rest.substr(bar + 1));
            f.gondola = kind == "gondola";
            ferries.push_back(f);
        }
    }
    if (rows.empty()) { std::cerr << fn << ": no map\n"; return false; }
    if (!s.styleOverride.empty()) styleName = s.styleOverride;
    for (auto &kv : s.legendNames)
        if (legend.count(kv.first)) legend[kv.first].name = kv.second;
    size_t wmax = 0;
    for (auto &r : rows) wmax = std::max(wmax, r.size());
    s.w = (int)wmax;
    s.h = (int)rows.size();
    s.x0 = cx - s.w / 2;
    s.y0 = cy - s.h / 2;
    s.chars.assign(s.w * s.h, ' ');
    for (int r = 0; r < s.h; r++)
        for (size_t c = 0; c < rows[r].size(); c++) s.chars[r * s.w + c] = rows[r][c];
    s.tiles.assign(s.w * s.h, Tile());
    s.style = w.styleIndex(styleName);
    TownBuilder tb(w, s);
    tb.towerRoof = towerRoof;
    tb.ferryList = ferries;
    tb.run(legend, gateNames);
    return true;
}
