#include "animals.h"
#include "world.h"
#include "mesh.h"
#include "noise.h"
#include <algorithm>
#include <map>
#include <climits>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

vec3 hexColour(const std::string &s) {
    unsigned v = 0;
    std::sscanf(s.c_str() + (s[0] == '#' ? 1 : 0), "%x", &v);
    return vec3(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f);
}

std::string trimmed(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

bool grassy(uint8_t t) { return t == T_GRASS || t == T_MEADOW || t == T_SCRUB; }

}  // namespace

// ------------------------------------------------------------------ data/animals.txt

bool Animals::load(const std::string &path) {
    std::ifstream in(path);
    if (!in) { std::cerr << "no " << path << "\n"; return false; }
    std::string line;
    AnimalKind *cur = nullptr;
    while (std::getline(in, line)) {
        std::string t = trimmed(line);
        if (t.empty() || t[0] == '#') continue;
        std::istringstream ss(t);
        std::string w;
        ss >> w;
        if (w == "animal") {
            kinds.push_back(AnimalKind());
            cur = &kinds.back();
            ss >> cur->id;
            cur->name = "a " + cur->id;
            cur->plural = cur->id + "s";
            continue;
        }
        if (w == "pasture" || w == "yard" || w == "inn" || w == "street" || w == "at" || w == "pen") {
            Rule r;
            r.where = w;
            if (w == "at" || w == "pen") {   // at "NAME" KIND LO HI, pen "NAME" KIND LO HI
                size_t a = t.find('"'), b = t.find('"', a + 1);
                if (a == std::string::npos || b == std::string::npos) continue;
                r.building = t.substr(a + 1, b - a - 1);
                ss.str(t.substr(b + 1));
                ss.clear();
            }
            ss >> r.kind >> r.lo >> r.hi;
            std::string kw, list;
            if (ss >> kw >> list && kw == "in") {
                std::string one;
                std::istringstream ls(list);
                while (std::getline(ls, one, ',')) {
                    for (auto &ch : one) if (ch == '_') ch = ' ';
                    r.in.push_back(one);
                }
            }
            rules.push_back(r);
            continue;
        }
        if (!cur) continue;
        if (w == "name") {
            std::string rest;
            std::getline(ss, rest);
            size_t bar = rest.find('|');
            cur->name = trimmed(rest.substr(0, bar));
            if (bar != std::string::npos) cur->plural = trimmed(rest.substr(bar + 1));
        } else if (w == "look") {
            std::getline(ss, cur->look);
            cur->look = trimmed(cur->look);
        } else if (w == "body") {
            std::string shape;
            ss >> shape >> cur->body.len >> cur->body.ht;
            cur->body.bird = shape == "bird";
        } else if (w == "colour" || w == "color") {
            std::string c;
            while (ss >> c) cur->colours.push_back(hexColour(c));
        } else if (w == "patches") {
            std::string c; ss >> c; cur->body.patch = hexColour(c);
        } else if (w == "head") {
            std::string c; ss >> c; cur->body.head = hexColour(c);
        } else if (w == "use") {   // use VERB [needs ITEM] [gives ITEM] [every MINUTES] | TEXT
            AnimalKind::Use u;
            std::string head = t.substr(4), text;
            size_t bar = head.find('|');
            if (bar != std::string::npos) { text = trimmed(head.substr(bar + 1)); head = head.substr(0, bar); }
            std::istringstream hs(head);
            hs >> u.verb;
            std::string kw;
            while (hs >> kw) {
                if (kw == "dead") u.dead = true;
                else if (kw == "needs") hs >> u.needs;
                else if (kw == "gives") hs >> u.gives;
                else if (kw == "every") hs >> u.every;
            }
            u.text = text;
            cur->uses.push_back(u);
        } else if (w == "done") {  // done VERB TEXT: when it was done too lately
            std::string verb, rest;
            ss >> verb;
            std::getline(ss, rest);
            for (auto &u : cur->uses) if (u.verb == verb) u.done = trimmed(rest);
        } else if (w == "speed") ss >> cur->speed;
        else if (w == "flock") cur->flock = true;
        else if (w == "stands") cur->lies = false;
        else if (w == "roams") cur->roams = true;
        else if (w == "horns") cur->body.horns = true;
        else if (w == "wool") cur->body.wool = true;
        else if (w == "mane") cur->body.mane = true;
        else if (w == "snout") cur->body.snout = true;
        else if (w == "ears") cur->body.ears = true;
        else if (w == "neck") cur->body.longNeck = true;
        else if (w == "comb") cur->body.comb = true;
        else if (w == "tailup") cur->body.tailUp = true;
        else if (w == "antlers") cur->body.antlers = true;
        else if (w == "wild") cur->wild = true;
        else if (w == "night") cur->night = true;
        else if (w == "hostile") ss >> cur->hostile;
        else if (w == "shy") ss >> cur->shy;
        else if (w == "run") ss >> cur->run;
        else if (w == "flees") ss >> cur->flees;
        else if (w == "fight") ss >> cur->hp >> cur->skill >> cur->dmg;
        else if (w == "every") ss >> cur->every;
        else if (w == "bout") ss >> cur->bout;
        else if (w == "biters") ss >> cur->biters;
    }
    for (auto &k : kinds) if (k.colours.empty()) k.colours.push_back(k.body.col);
    return true;
}

// ------------------------------------------------------------------ putting them out

void Animals::placeTown(World &w, int si) {
    if (si < 0 || si >= (int)w.settlements.size()) return;
    if ((int)placed.size() < (int)w.settlements.size()) placed.resize(w.settlements.size(), false);
    if (placed[si]) return;
    placed[si] = true;
    const Settlement &s = w.settlements[si];
    std::string nation = country.count(s.name) ? country.at(s.name) : "France";
    Rng rng(hash32((uint32_t)std::hash<std::string>()(s.name) + 77));
    auto kindOf = [&](const std::string &id) {
        for (size_t i = 0; i < kinds.size(); i++) if (kinds[i].id == id) return (int)i;
        return -1;
    };
    auto inSomeTown = [&](int X, int Y) {
        for (auto &o : w.settlements) if (o.inside(X, Y)) return true;
        return false;
    };
    std::string owner;   // whose the animals of the rule being placed are
    auto add = [&](int k, float x, float y, float hx, float hy, float radius, int herd, bool tethered, float facing) {
        Animal a;
        a.owner = owner;
        a.kind = k;
        a.x = a.fx = x; a.y = a.fy = y;
        a.hx = hx; a.hy = hy; a.radius = radius;
        a.herd = herd < 0 ? (int)list.size() : herd;
        a.tethered = tethered;
        a.seed = rng.next();
        a.col = kinds[k].colours[a.seed % kinds[k].colours.size()];
        a.facing = tethered ? facing : rng.uni() * 6.2832f;
        a.wait = rng.uni() * 6;
        list.push_back(a);
    };
    for (auto &r : rules) {
        int k = kindOf(r.kind);
        if (k < 0) continue;
        if (!r.in.empty() && std::find(r.in.begin(), r.in.end(), s.name) == r.in.end() &&
            std::find(r.in.begin(), r.in.end(), nation) == r.in.end()) continue;
        owner = r.where == "pasture" ? "a herdsman of " + s.name : r.where == "yard" ? s.name : "";
        if (r.where == "pasture") {
            if (s.hamlet) continue;
            // a herd on the grass outside the town: a spot 14 to 50 tiles beyond its edge
            for (int attempt = 0; attempt < 80; attempt++) {
                float ang = rng.uni() * 6.2832f, d = 14 + rng.uni() * 36;
                int cx = (int)(s.x0 + s.w * 0.5f + std::cos(ang) * (s.w * 0.5f + d));
                int cy = (int)(s.y0 + s.h * 0.5f + std::sin(ang) * (s.h * 0.5f + d));
                if (!w.inWorld(cx, cy) || inSomeTown(cx, cy)) continue;
                int good = 0;
                for (int dy = -4; dy <= 4; dy += 4)
                    for (int dx = -4; dx <= 4; dx += 4)
                        if (w.inWorld(cx + dx, cy + dy) && grassy(w.tile(cx + dx, cy + dy).terrain) && w.passable(cx + dx, cy + dy)) good++;
                if (good < 7) continue;
                int n = rng.range(r.lo, r.hi), first = -1;
                for (int i = 0, tries = 0; i < n && tries < 60; tries++) {
                    int X = cx + rng.range(-5, 5), Y = cy + rng.range(-5, 5);
                    if (!grassy(w.tile(X, Y).terrain) || !w.passable(X, Y)) continue;
                    add(k, X + 0.5f, Y + 0.5f, cx + 0.5f, cy + 0.5f, 7, first, false, 0);
                    if (first < 0) first = (int)list.size() - 1;
                    i++;
                }
                break;
            }
        } else if (r.where == "yard" || r.where == "street") {
            // yards and gardens of a farm or a village; or the streets and squares of a town
            std::vector<std::pair<int, int>> spots;
            for (int Y = s.y0; Y < s.y0 + s.h; Y++)
                for (int X = s.x0; X < s.x0 + s.w; X++) {
                    char c = s.ch(X, Y);
                    bool ok = r.where == "yard" ? (c == ';' || c == ',') : (c == '.' || c == ':');
                    if (ok && !w.tile(X, Y).bld && w.passable(X, Y)) spots.push_back({X, Y});
                }
            if (spots.empty()) continue;
            int n = rng.range(r.lo, r.hi), first = -1;
            float hx = 0, hy = 0;
            for (int i = 0; i < n; i++) {
                auto p = spots[rng.next() % spots.size()];
                if (r.where == "yard" && first >= 0) {   // a flock of hens keeps together: near the first
                    for (int tries = 0; tries < 20; tries++) {
                        auto q = spots[rng.next() % spots.size()];
                        if (std::abs(q.first - hx) + std::abs(q.second - hy) < 8) { p = q; break; }
                    }
                }
                if (first < 0) { hx = p.first + 0.5f; hy = p.second + 0.5f; }
                add(k, p.first + 0.5f, p.second + 0.5f, r.where == "yard" ? hx : p.first + 0.5f, r.where == "yard" ? hy : p.second + 0.5f,
                    r.where == "yard" ? 5 : 12, r.where == "yard" ? first : -1, false, 0);
                if (first < 0) first = (int)list.size() - 1;
            }
        } else if (r.where == "pen") {
            // standing inside an open building of that name, on its floor (the cattle market of Poissy):
            // they shift about where they stand, since animals do not walk into buildings
            for (size_t b = 0; b < w.buildings.size(); b++) {
                const Building &bd = w.buildings[b];
                if (!s.inside((bd.x0 + bd.x1) / 2, (bd.y0 + bd.y1) / 2) || bd.name.find(r.building) == std::string::npos) continue;
                int id = (int)b + 1, n = rng.range(r.lo, r.hi);
                owner = bd.name;
                for (int tries = 0, done = 0; tries < 200 && done < n; tries++) {
                    int X = rng.range(bd.x0 + 1, bd.x1 - 1), Y = rng.range(bd.y0 + 1, bd.y1 - 2);
                    if (w.tile(X, Y).bld != id || !w.passable(X, Y) || w.tile(X, Y).feature) continue;
                    add(k, X + 0.5f, Y + 0.5f, X + 0.5f, Y + 0.5f, 0, -1, false, 0);
                    done++;
                }
            }
        } else if (r.where == "inn" || r.where == "at") {
            // tied up by the street door of every inn (or round a building of that name)
            for (size_t b = 0; b < w.buildings.size(); b++) {
                const Building &bd = w.buildings[b];
                if (!s.inside((bd.x0 + bd.x1) / 2, (bd.y0 + bd.y1) / 2)) continue;
                if (r.where == "inn" ? bd.kind != "inn" : bd.name.find(r.building) == std::string::npos) continue;
                int id = (int)b + 1, n = rng.range(r.lo, r.hi), done = 0;
                owner = bd.name.empty() ? "the inn" : bd.name;
                const int d4[4][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
                for (int Y = bd.y0; Y <= bd.y1 && done < n; Y++)
                    for (int X = bd.x0; X <= bd.x1 && done < n; X++) {
                        if (w.tile(X, Y).bld != id || w.tile(X, Y).wall != W_DOOR) continue;
                        for (auto &d : d4) {
                            int ox = X + d[0], oy = Y + d[1];
                            if (w.tile(ox, oy).bld == id || !w.passable(ox, oy)) continue;
                            // along the wall on either side of the doorway, a tile out from it
                            for (int k2 = 2; k2 <= 6 && done < n; k2 += 2)
                                for (int sd : {1, -1}) {
                                    if (done >= n) break;
                                    int ax = ox - sd * k2 * d[1], ay = oy + sd * k2 * d[0];
                                    if (w.tile(ax, ay).bld || !w.passable(ax, ay) || w.linkAt(ax, ay, 0)) continue;
                                    float fac = std::atan2((float)(sd * d[1]), (float)(sd * d[0]));   // along the wall, away from the door
                                    add(k, ax + 0.5f, ay + 0.5f, ax + 0.5f, ay + 0.5f, r.where == "at" ? 3 : 0, -1, r.where == "inn", fac);
                                    done++;
                                }
                            break;
                        }
                    }
            }
        }
    }
}

void Animals::placeNear(World &w, float px, float py, float reach) {
    if ((int)placed.size() < (int)w.settlements.size()) placed.resize(w.settlements.size(), false);
    for (int i = 0; i < (int)w.settlements.size(); i++) {
        if (placed[i]) continue;
        const Settlement &s = w.settlements[i];
        float dx = std::max({(float)s.x0 - px, 0.0f, px - (float)(s.x0 + s.w)});
        float dy = std::max({(float)s.y0 - py, 0.0f, py - (float)(s.y0 + s.h)});
        if (dx * dx + dy * dy < reach * reach) placeTown(w, i);
    }
}

// ------------------------------------------------------------------ their day

void Animals::update(World &w, float dt, double minutes, int ptx, int pty) {
    double hour = std::fmod(minutes, 1440.0) / 60.0;
    bool night = hour >= 21.0 || hour < 5.0;
    for (auto &a : list) {
        if (a.gone) continue;
        if (std::abs(a.x - ptx) > 60 || std::abs(a.y - pty) > 60) continue;   // far off: nobody sees them
        if (a.wildOne) { wildStep(w, a, dt, minutes, ptx, pty); continue; }
        if (a.mine) {   // yours: it stands where you left it, or, led on a halter, walks after you
            a.lying = false;
            if (a.led && a.moving) {
                float dx = a.fx - a.x, dy = a.fy - a.y, d = std::sqrt(dx * dx + dy * dy), step = 7.0f * dt;
                if (d > 0.01f) a.facing = std::atan2(-dx, dy);
                if (d <= step) { a.x = a.fx; a.y = a.fy; a.moving = false; }
                else { a.x += dx / d * step; a.y += dy / d * step; a.phase += dt * 3.0f; }
            }
            continue;
        }
        const AnimalKind &k = kinds[a.kind];
        Rng rng(a.seed + (uint32_t)(minutes * 7));
        if (a.tethered) {                         // tied up: it shifts, and noses at the ground
            a.lying = false;
            a.headDown = 0.5f + 0.5f * std::sin((float)minutes * 0.9f + (a.seed & 255));
            continue;
        }
        if (night && !k.roams) {                  // asleep
            a.moving = false;
            a.lying = k.lies || k.body.bird;
            a.headDown = 0;
            continue;
        }
        a.lying = false;
        if (a.moving) {
            float dx = a.fx - a.x, dy = a.fy - a.y, d = std::sqrt(dx * dx + dy * dy);
            float step = k.speed * dt;
            if (d <= step) {
                a.x = a.fx; a.y = a.fy; a.moving = false;
                a.wait = k.roams ? 1.0f + rng.uni() * 5 : 4.0f + rng.uni() * 12;
            } else {
                a.x += dx / d * step; a.y += dy / d * step;
                a.phase += dt * k.speed * 1.4f;
            }
            a.headDown = std::max(0.0f, a.headDown - dt * 2);
            continue;
        }
        a.wait -= dt;
        if (!k.roams && !k.body.bird) a.headDown = std::min(1.0f, a.headDown + dt);   // grazing
        if (a.wait > 0 || a.radius <= 0) continue;
        a.wait = 1.0f + rng.uni() * 3;
        // somewhere else on its home ground (a flock: near the first of the herd), by a clear way
        float cx = a.hx, cy = a.hy, rad = a.radius;
        if (k.flock && a.herd >= 0 && &list[a.herd] != &a) { cx = list[a.herd].x; cy = list[a.herd].y; rad = 2.5f; }
        for (int tries = 0; tries < 6; tries++) {
            float tx = cx + (rng.uni() * 2 - 1) * rad, ty = cy + (rng.uni() * 2 - 1) * rad;
            float dx = tx - a.x, dy = ty - a.y, d = std::sqrt(dx * dx + dy * dy);
            if (d < 0.6f) continue;
            bool clear = true;
            for (float s = 0.5f; s <= d && clear; s += 0.5f) {
                int X = (int)std::floor(a.x + dx * s / d), Y = (int)std::floor(a.y + dy * s / d);
                if (!w.passable(X, Y) || w.tile(X, Y).bld || w.linkAt(X, Y, 0) || (X == ptx && Y == pty)) clear = false;
            }
            int X = (int)std::floor(tx), Y = (int)std::floor(ty);
            if (!clear || !w.passable(X, Y) || w.tile(X, Y).bld) continue;
            a.fx = tx; a.fy = ty; a.moving = true;
            a.facing = std::atan2(-dx, dy);
            break;
        }
    }
}

void Animals::draw(MeshBuf &m, World &w, int minx, int miny, int maxx, int maxy) {
    for (size_t i = 0; i < list.size(); i++) {
        const Animal &a = list[i];
        if ((int)i == ridden || a.gone) continue;
        if (a.x < minx - 4 || a.x > maxx + 4 || a.y < miny - 4 || a.y > maxy + 4) continue;
        AnimalLook look = kinds[a.kind].body;
        look.col = a.col;
        look.saddle = a.mine && kinds[a.kind].id == "horse";
        if (look.antlers && (a.seed & 1)) look.antlers = false;   // hinds have none
        if (look.wool && a.lastUse.count("shear")) { look.wool = false; look.col = vec3(0.82f, 0.76f, 0.68f); }   // shorn
        buildAnimal(m, look, vec3(a.x, a.y, w.groundZ(a.x, a.y)), a.facing, a.moving ? a.phase : 0, a.headDown, a.lying);
    }
}

int Animals::nearest(float x, float y, float maxd) const {
    int best = -1;
    float bd = maxd * maxd;
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i].gone) continue;
        float dx = list[i].x - x, dy = list[i].y - y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = (int)i; }
    }
    return best;
}

// ------------------------------------------------------------------ the wild ones

int Animals::kindOf(const std::string &id) const {
    for (size_t i = 0; i < kinds.size(); i++) if (kinds[i].id == id) return (int)i;
    return -1;
}

int Animals::spawn(int kind, float x, float y, uint32_t seed) {
    int i = -1;
    for (size_t j = 0; j < list.size(); j++) if (list[j].gone) { i = (int)j; break; }   // a slot left by one gone
    if (i < 0) { list.push_back(Animal()); i = (int)list.size() - 1; }
    Animal a;
    const AnimalKind &k = kinds[kind];
    a.kind = kind;
    a.x = a.fx = a.hx = x; a.y = a.fy = a.hy = y;
    a.radius = 6;
    a.herd = i;
    a.seed = seed;
    a.col = k.colours[seed % k.colours.size()];
    a.facing = (seed % 628) * 0.01f;
    a.wait = (seed % 50) * 0.1f;
    a.wildOne = true;
    a.hp = (float)std::max(1, k.hp);
    list[i] = a;
    return i;
}

void Animals::startle(float x, float y, float r) {
    for (auto &a : list) {
        if (a.gone || !a.wildOne || a.dead || kinds[a.kind].shy <= 0) continue;
        if (std::hypot(a.x - x, a.y - y) < r) a.fleeT = std::max(a.fleeT, 6.0f + (a.seed % 40) * 0.1f);
    }
}

void Animals::hurt(int i, float dmg, double minutes) {
    if (i < 0 || i >= (int)list.size()) return;
    Animal &a = list[i];
    const AnimalKind &k = kinds[a.kind];
    if (a.dead || a.gone) return;
    a.hp -= dmg;
    if (a.hp <= 0) {
        a.dead = true; a.deadAt = minutes; a.lying = true; a.after = false; a.fleeT = 0; a.moving = false;
        if (k.hostile > 0)   // one of a pack killed: the rest break and run
            for (auto &o : list)
                if (&o != &a && o.wildOne && !o.gone && !o.dead && o.kind == a.kind && o.herd == a.herd) { o.after = false; o.fleeT = 12.0f + (o.seed % 30) * 0.1f; }
        return;
    }
    if (k.shy > 0 || a.hp < k.flees * k.hp) {   // it runs, hurt; and a pack mostly runs with it
        a.fleeT = 10.0f + (a.seed % 30) * 0.1f; a.after = false;
        if (k.hostile > 0)
            for (auto &o : list)
                if (&o != &a && o.wildOne && !o.gone && !o.dead && o.kind == a.kind && o.herd == a.herd && ((o.seed >> 3) % 10) < 6) { o.after = false; o.fleeT = 10.0f + (o.seed % 30) * 0.1f; }
    }
    else if (k.hostile > 0) a.after = true;                                                                  // or turns on you
}

// one moment of a wild animal: lying where it fell; running away; going for you (and biting);
// or grazing and wandering about where it was met
void Animals::wildStep(World &w, Animal &a, float dt, double minutes, int ptx, int pty) {
    const AnimalKind &k = kinds[a.kind];
    if (a.dead) { a.lying = true; a.moving = false; return; }
    a.lying = false;
    if (a.strikeAnim > 0) a.strikeAnim = std::max(0.0f, a.strikeAnim - dt);
    float px = ptx + 0.5f, py = pty + 0.5f, dx = px - a.x, dy = py - a.y, d = std::sqrt(dx * dx + dy * dy);
    Rng rng(a.seed + (uint32_t)(minutes * 113));
    // a step in a direction, round whatever is in the way
    auto moveBy = [&](float vx, float vy, float speed) {
        float len = std::sqrt(vx * vx + vy * vy);
        if (len < 1e-4f) return false;
        float base = std::atan2(vy, vx);
        for (float off : {0.0f, 0.6f, -0.6f, 1.2f, -1.2f, 1.9f, -1.9f}) {
            float ang = base + off, nx = a.x + std::cos(ang) * speed * dt, ny = a.y + std::sin(ang) * speed * dt;
            int X = (int)std::floor(nx), Y = (int)std::floor(ny);
            if (!w.inWorld(X, Y) || !w.passable(X, Y) || w.tile(X, Y).bld || (X == ptx && Y == pty)) continue;
            bool crowded = false;   // not on top of another of the wild ones
            for (const Animal &o : list)
                if (&o != &a && o.wildOne && !o.gone && !o.dead && std::hypot(o.x - nx, o.y - ny) < 0.7f && std::hypot(o.x - a.x, o.y - a.y) >= std::hypot(o.x - nx, o.y - ny)) { crowded = true; break; }
            if (crowded) continue;
            a.x = nx; a.y = ny;
            a.facing = std::atan2(-std::cos(ang), std::sin(ang));
            a.phase += dt * speed * 1.4f;
            a.moving = true;
            return true;
        }
        a.moving = false;
        return false;
    };
    float fast = k.run > 0 ? k.run : k.speed * 2.5f;
    a.headDown = std::max(0.0f, a.headDown - dt * 2);
    if (a.fleeT > 0) { a.fleeT -= dt; moveBy(-dx, -dy, fast); return; }
    if (k.hostile > 0 && !a.after && !playerDown && d < k.hostile) a.after = true;
    if (a.after) {
        if (playerDown || d > 30) { a.after = false; a.afterT = 0; return; }
        a.afterT += dt;
        if (k.bout > 0 && a.afterT > k.bout) { a.after = false; a.afterT = 0; a.fleeT = 10; return; }   // a bear breaks off its charge
        if (k.biters > 0 && d > 1.55f) {   // a pack: no more than so many at you at once; the rest circle
            int at = 0;
            for (const Animal &o : list)
                if (&o != &a && o.wildOne && !o.gone && !o.dead && o.after && o.herd == a.herd && std::hypot(o.x - px, o.y - py) <= 1.55f) at++;
            if (at >= k.biters) {
                if (d < 2.6f) moveBy(-dx, -dy, k.speed * 2);
                else if (d > 3.4f) moveBy(dx, dy, fast);
                else { moveBy(-dy, dx, k.speed * 2); a.facing = std::atan2(-dx, dy); }
                return;
            }
        }
        if (d > 1.5f) {   // straight at you if it can; else by a way round, found on the tiles
            a.repath -= dt;
            if (a.path.empty() || a.repath <= 0) {
                a.repath = 1.5f;
                a.path.clear(); a.pathPos = 0;
                int sx = (int)std::floor(a.x), sy = (int)std::floor(a.y);
                std::map<std::pair<int, int>, std::pair<int, int>> prev;
                std::vector<std::pair<int, int>> q{{sx, sy}};
                prev[{sx, sy}] = {INT32_MIN, 0};
                bool found = false;
                for (size_t qi = 0; qi < q.size() && !found && prev.size() < 4000; qi++) {
                    auto [x, y] = q[qi];
                    for (int oy = -1; oy <= 1 && !found; oy++)
                        for (int ox = -1; ox <= 1 && !found; ox++) {
                            if (!ox && !oy) continue;
                            int nx = x + ox, ny = y + oy;
                            if (prev.count({nx, ny}) || std::abs(nx - sx) > 40 || std::abs(ny - sy) > 40) continue;
                            bool goal = std::abs(nx - ptx) <= 1 && std::abs(ny - pty) <= 1;
                            if (!goal && (!w.inWorld(nx, ny) || !w.passable(nx, ny) || w.tile(nx, ny).bld)) continue;
                            if (ox && oy && (!w.passable(x + ox, y) || !w.passable(x, y + oy))) continue;
                            prev[{nx, ny}] = {x, y};
                            if (goal) { found = true; for (std::pair<int, int> c{nx, ny}; c.first != INT32_MIN; c = prev[c]) a.path.push_back(c); }
                            else q.push_back({nx, ny});
                        }
                }
                std::reverse(a.path.begin(), a.path.end());
                if (!a.path.empty()) a.pathPos = 1;
            }
            if (a.pathPos < a.path.size()) {
                float wx = a.path[a.pathPos].first + 0.5f - a.x, wy = a.path[a.pathPos].second + 0.5f - a.y;
                if (std::hypot(wx, wy) < 0.35f) { a.pathPos++; return; }
                if ((a.path[a.pathPos].first == ptx && a.path[a.pathPos].second == pty) || !moveBy(wx, wy, fast)) a.repath = 0;
            } else moveBy(dx, dy, fast);
            return;
        }
        a.moving = false;
        a.facing = std::atan2(-dx, dy);
        a.strikeT -= dt;
        if (a.strikeT <= 0) {
            a.strikeT = k.every * (0.8f + rng.uni() * 0.5f);
            a.strikeAnim = 0.3f;
            a.headDown = 0.6f;
            float chance = std::max(0.15f, std::min(0.9f, 0.55f + 0.08f * (k.skill - playerSkill)));
            std::string who = k.name;
            if (!who.empty()) who = "The " + who.substr(who.find(' ') + 1);
            if (rng.uni() >= chance) notes.push_back(who + " snaps at you, and misses.");
            else {
                int dmg = std::max(0, 1 + (int)(rng.uni() * std::max(1, k.dmg)) + k.skill / 2 - playerArmour);
                if (dmg == 0) notes.push_back(who + " gets its teeth in your coat, and no further.");
                else { playerHurt += (float)dmg; notes.push_back(who + (k.dmg >= 7 ? " strikes you" : " bites you") + " (" + std::to_string(dmg) + ")."); }
            }
        }
        return;
    }
    if (k.shy > 0 && d < k.shy) { a.fleeT = 4.0f + rng.uni() * 4; return; }
    // about its own ground: graze, then a few steps somewhere else
    if (a.moving) {
        float mx = a.fx - a.x, my = a.fy - a.y, md = std::sqrt(mx * mx + my * my);
        if (md < 0.2f || !moveBy(mx, my, k.speed)) { a.moving = false; a.wait = 3.0f + rng.uni() * 8; }
        return;
    }
    a.wait -= dt;
    a.headDown = std::min(1.0f, a.headDown + dt * 3);
    if (a.wait > 0) return;
    a.fx = a.hx + (rng.uni() * 2 - 1) * a.radius;
    a.fy = a.hy + (rng.uni() * 2 - 1) * a.radius;
    a.moving = true;
}
