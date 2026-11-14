// items.cc: the catalogue of things, where they lie, how they look, and saving them.
#include "items.h"
#include "noise.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <set>
#include <sstream>

namespace {

std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
std::string lower(std::string s) {
    for (auto &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
vec3 hexc(const std::string &s) {
    unsigned v = 0;
    std::sscanf(s.c_str() + (s[0] == '#' ? 1 : 0), "%x", &v);
    return vec3(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f);
}

const std::map<std::string, int> &featureWords() {
    static const std::map<std::string, int> m = {
        {"table", F_TABLE}, {"counter", F_COUNTER}, {"stall", F_STALL}, {"chest", F_CHEST}, {"bench", F_BENCH},
        {"altar", F_ALTAR}, {"barrel", F_BARREL}, {"anvil", F_ANVIL}, {"oven", F_OVEN}, {"hearth", F_HEARTH},
        {"bed", F_BED}, {"forge", F_FORGE}, {"statue", F_STATUE}, {"hay", F_HAY}};
    return m;
}

// a piece of furniture somewhere in the world
struct Spot { int x, y, z, feature, bld; };

}  // namespace

std::string featureName(int f) {
    switch (f) {
        case F_OAK: return "an oak";
        case F_PINE: return "a pine";
        case F_FRUIT: return "a fruit tree";
        case F_POPLAR: return "a poplar";
        case F_BUSH: return "a bush";
        case F_BOULDER: return "a boulder";
        case F_PILLAR: return "a pillar";
        case F_TABLE: return "a table";
        case F_HEARTH: return "a hearth";
        case F_BED: return "a bed";
        case F_BENCH: return "a bench";
        case F_ALTAR: return "an altar";
        case F_WELL: return "a well";
        case F_FOUNTAIN: return "a fountain";
        case F_STALL: return "a market stall";
        case F_CROSS: return "a stone cross";
        case F_CHEST: return "a chest";
        case F_HAY: return "a haystack";
        case F_BARREL: return "a barrel";
        case F_STATUE: return "a statue";
        case F_STAGE: return "a stage";
        case F_LADDER: return "a ladder";
        case F_HATCH: return "a trapdoor";
        case F_STAIRS: return "a stair";
        case F_SHAFT: return "a shaft with a windlass";
        case F_BELL: return "a bell";
        case F_ANVIL: return "an anvil";
        case F_FORGE: return "a forge";
        case F_COUNTER: return "a counter";
        case F_OVEN: return "an oven";
        case F_OBELISK: return "an obelisk";
        case F_COLUMN: return "a column";
        case F_MOORING: return "a mooring post";
        case F_TENT: return "a tent";
        case F_GABION: return "gabions";
        case F_CANNON: return "a cannon";
        default: return "";
    }
}

float Items::surface(int f) {
    switch (f) {
        case F_TABLE: return 0.56f;
        case F_STALL: return 0.56f;
        case F_COUNTER: return 0.80f;
        case F_CHEST: return 0.35f;
        case F_BENCH: return 0.36f;
        case F_ALTAR: return 0.66f;
        case F_BARREL: return 0.55f;
        case F_HAY: return 0.6f;
        case F_ANVIL: return 0.5f;
        default: return 0.0f;
    }
}

bool Items::load(const std::string &fn) {
    std::ifstream in(fn);
    if (!in) return false;
    std::string dir = fn.substr(0, fn.rfind('/') + 1);   // pictures are named from the data folder
    std::string line;
    ItemKind *cur = nullptr;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        std::istringstream ss(t);
        std::string w;
        ss >> w;
        if (!cur) {
            if (w == "item") {
                kinds.emplace_back();
                cur = &kinds.back();
                ss >> cur->id;
                byId[cur->id] = (int)kinds.size() - 1;
            } else if (w == "on" || w == "beside" || w == "art" || w.rfind("moored", 0) == 0 || w == "harbour") {
                rules.push_back(t);
            }
            continue;
        }
        std::string rest;
        std::getline(ss, rest);
        rest = trim(rest);
        if (w == "end") cur = nullptr;
        else if (w == "name") {
            size_t bar = rest.find('|');
            cur->name = trim(rest.substr(0, bar));
            cur->plural = bar == std::string::npos ? cur->name : trim(rest.substr(bar + 1));
        } else if (w == "shape") {
            std::istringstream rs(rest);
            std::string c1, c2;
            rs >> cur->shape >> c1 >> c2;
            if (!c1.empty()) cur->color = hexc(c1);
            if (!c2.empty()) cur->color2 = hexc(c2);
            cur->boat = cur->shape == "boat" || cur->shape == "gondola" || cur->shape == "ship";
        }
        else if (w == "heals") cur->heals = std::atoi(rest.c_str());
        else if (w == "weight") cur->weight = (float)std::atof(rest.c_str());
        else if (w == "price") cur->price = std::atoi(rest.c_str());
        else if (w == "money") cur->money = std::atoi(rest.c_str());
        else if (w == "fixed") cur->fixed = true;
        else if (w == "chained") cur->fixed = cur->chained = true;
        else if (w == "weapon") cur->weapon = std::atoi(rest.c_str());
        else if (w == "armour") cur->armour = std::atoi(rest.c_str());
        else if (w == "title") cur->title = rest;
        else if (w == "look") cur->look = rest;
        else if (w == "image") cur->image = dir + rest;
        else if (w == "credit") cur->credit = rest;
        else if (w == "after") {   // after KEY image FILE | look TEXT | credit TEXT
            std::istringstream as(rest);
            std::string key, what;
            as >> key >> what;
            std::string val;
            std::getline(as, val);
            size_t b = val.find_first_not_of(' ');
            val = b == std::string::npos ? "" : val.substr(b);
            cur->afterKey = key;
            if (what == "image") cur->afterImage = dir + val;
            else if (what == "look") cur->afterLook = val;
            else if (what == "credit") cur->afterCredit = val;
        }
        else if (w == "use") {   // use VERB TEXT {EFFECTS}; for read: use read TITLE | TEXT
            std::istringstream us(rest);
            us >> cur->useVerb;
            std::string text;
            std::getline(us, text);
            for (size_t b; (b = text.find('{')) != std::string::npos;) {
                size_t e = text.find('}', b);
                if (e == std::string::npos) break;
                cur->useEffects.push_back(trim(text.substr(b + 1, e - b - 1)));
                text.erase(b, e - b + 1);
            }
            text = trim(text);
            size_t bar = text.find('|');
            if (cur->useVerb == "read" && bar != std::string::npos) { cur->useTitle = trim(text.substr(0, bar)); text = trim(text.substr(bar + 1)); }
            if ((cur->useVerb == "make" || cur->useVerb == "play") && bar != std::string::npos) {
                std::istringstream hs(text.substr(0, bar));
                if (cur->useVerb == "make") { hs >> cur->makes; if (!(hs >> cur->makesN)) cur->makesN = 1; }
                else hs >> cur->play;
                text = trim(text.substr(bar + 1));
            }
            cur->useText = text;
        }
        else if (w == "book") { std::istringstream bs(rest); bs >> cur->book; }
        else if (w == "needs") { std::istringstream ns(rest); ns >> cur->needs; std::getline(ns, cur->needsText); cur->needsText = trim(cur->needsText); }
        else if (w == "night") {   // night TEXT {EFFECTS}
            std::string text = rest;
            for (size_t b; (b = text.find('{')) != std::string::npos;) {
                size_t e = text.find('}', b);
                if (e == std::string::npos) break;
                cur->nightEffects.push_back(trim(text.substr(b + 1, e - b - 1)));
                text.erase(b, e - b + 1);
            }
            cur->nightText = trim(text);
        }
        else if (w == "near") { std::istringstream ns(rest); ns >> cur->nearKind; std::getline(ns, cur->nearText); cur->nearText = trim(cur->nearText); }
        else if (w == "at") {   // at "PLACE" TEXT
            size_t a = rest.find('"'), b = a == std::string::npos ? a : rest.find('"', a + 1);
            if (b != std::string::npos) { cur->atPlace = rest.substr(a + 1, b - a - 1); cur->atText = trim(rest.substr(b + 1)); }
        }
        else if (w == "also") { std::istringstream ws(rest); std::string x; while (ws >> x) cur->also.push_back(lower(x)); }
        else if (w == "with") { std::istringstream ws(rest); std::string id; int n; while (ws >> id) { if (!(ws >> n)) n = 1; cur->with.push_back({id, n}); } }
        else if (w == "takes") cur->takes = std::atoi(rest.c_str());
        else std::fprintf(stderr, "items.txt: %s: cannot read \"%s\"\n", cur->id.c_str(), t.c_str());
    }
    return true;
}

std::string moneyString(long d) {
    long l = d / 240, s = (d % 240) / 12, dn = d % 12;
    std::string out;
    auto part = [&](long n, const char *one, const char *many) {
        if (!n) return;
        if (!out.empty()) out += ", ";
        out += std::to_string(n) + " " + (n == 1 ? one : many);
    };
    part(l, "livre", "livres");
    part(s, "sou", "sous");
    part(dn, "denier", "deniers");
    return out.empty() ? "nothing" : out;
}

int Items::match(const std::string &w0, int onlyPass) const {
    std::string w = lower(w0);
    // "physicians'", "Bellarmine's", "bread.": the word itself (a bare "physicians'" otherwise
    // missed the certificate and fell through to the first four letters, Aristotle's Physics)
    while (!w.empty() && !std::isalnum((unsigned char)w.back()) && w.back() != '\'') w.pop_back();
    if (w.size() > 2 && w.compare(w.size() - 2, 2, "'s") == 0) w.erase(w.size() - 2);
    while (!w.empty() && !std::isalnum((unsigned char)w.back())) w.pop_back();
    if (w.empty()) return -1;
    // a whole word first, by its id or a word of its name ("bread" is the loaf of bread); only
    // then the first four letters, as in Ultima ("brea" would otherwise find the breastplate)
    static const std::set<std::string> small = {"a", "an", "the", "some", "of", "pair", "pairs", "handful", "handfuls", "armful", "armfuls"};
    for (int pass = 0; pass < 2; pass++) {
        if (onlyPass >= 0 && pass != onlyPass) continue;
        auto like = [&](const std::string &a, const std::string &b) {
            if (a == b) return true;
            return pass == 1 && a.size() >= 4 && b.size() >= 4 && a.compare(0, 4, b, 0, 4) == 0;
        };
        for (size_t k = 0; k < kinds.size(); k++)
            if (!kinds[k].fixed && like(w, lower(kinds[k].id))) return (int)k;
        for (size_t k = 0; k < kinds.size(); k++) {
            if (kinds[k].fixed) continue;
            for (const std::string *n : {&kinds[k].name, &kinds[k].plural}) {
                std::string s = lower(*n);
                size_t of = s.find(" of ");
                if (of != std::string::npos) s = s.substr(of + 4);
                std::istringstream ss(s);
                std::string x;
                while (ss >> x) {
                    while (!x.empty() && !std::isalnum((unsigned char)x.back())) x.pop_back();
                    if (x.size() > 2 && x.compare(x.size() - 2, 2, "'s") == 0) x.erase(x.size() - 2);   // "Bellarmine's": "bellarmine"
                    if (!small.count(x) && like(w, x)) return (int)k;
                }
            }
            for (const std::string &x : kinds[k].also) if (like(w, x)) return (int)k;   // "also" words
        }
        if (pass == 0)   // last of the whole words, the one before " of " ("pouch": a pouch of cochineal; "disc", "crate", "box")
            for (size_t k = 0; k < kinds.size(); k++) {
                if (kinds[k].fixed) continue;
                for (const std::string *n : {&kinds[k].name, &kinds[k].plural}) {
                    std::string s = lower(*n);
                    size_t of = s.find(" of ");
                    if (of == std::string::npos) continue;
                    std::istringstream ss(s.substr(0, of));
                    std::string x, head;
                    while (ss >> x) if (!small.count(x)) head = x;
                    if (!head.empty() && head == w) return (int)k;
                }
            }
    }
    return -1;
}

std::string Items::nameOf(int kind, int n) const {
    const ItemKind &k = kinds[kind];
    if (n == 1) return k.name;
    static const char *words[] = {"", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve"};
    std::string num = n <= 12 ? words[n] : std::to_string(n);
    return num + " " + k.plural;
}

void Items::add(const Thing &t) {
    things.push_back(t);
    if (things.back().count0 < 0) things.back().count0 = things.back().count;
    index[World::key3(t.x, t.y, t.z)].push_back((int)things.size() - 1);
}

void Items::reindex() {
    index.clear();
    for (size_t i = 0; i < things.size(); i++)
        if (!things[i].gone) index[World::key3(things[i].x, things[i].y, things[i].z)].push_back((int)i);
}

std::vector<int> Items::at(int X, int Y, int Z) const {
    std::vector<int> out;
    auto it = index.find(World::key3(X, Y, Z));
    if (it == index.end()) return out;
    for (int i : it->second)
        if (!things[i].gone) out.push_back(i);
    return out;
}

void Items::take(int i) {
    things[i].gone = true;
    version++;
}

void Items::put(World &w, int kind, int n, int X, int Y, int Z) {
    Thing t;
    t.kind = kind; t.count = n; t.x = X; t.y = Y; t.z = Z;
    const Tile *tl = w.tileAt(X, Y, Z);
    int f = tl ? tl->feature : F_NONE;
    t.h = surface(f);
    // beside what is there already
    int k = (int)at(X, Y, Z).size();
    static const float off[6][2] = {{0.5f, 0.5f}, {0.28f, 0.6f}, {0.72f, 0.42f}, {0.4f, 0.3f}, {0.62f, 0.72f}, {0.3f, 0.78f}};
    t.ox = off[k % 6][0];
    t.oy = off[k % 6][1];
    t.facing = (float)(hash01(X, Y, (uint32_t)k + 77) * 6.2832);
    add(t);
    version++;
}

// ------------------------------------------------------------------ where things lie

void Items::place(World &w) {
    things.clear();
    index.clear();
    // every piece of furniture of the towns and hamlets, and of every floor
    std::vector<Spot> spots;
    for (auto &s : w.settlements)
        for (int Y = s.y0; Y < s.y0 + s.h; Y++)
            for (int X = s.x0; X < s.x0 + s.w; X++) {
                if (s.ch(X, Y) == ' ') continue;
                const Tile &t = s.tileAt(X, Y);
                if (t.feature != F_NONE) spots.push_back({X, Y, 0, t.feature, t.bld});
            }
    for (auto &L : w.levels)
        for (int Y = L.y0; Y < L.y0 + L.h; Y++)
            for (int X = L.x0; X < L.x0 + L.w; X++) {
                const Tile &t = L.at(X, Y);
                if (t.terrain != T_VOID && t.feature != F_NONE) spots.push_back({X, Y, L.z, t.feature, L.bld});
            }
    // a free floor tile: nothing on it, nothing in the way, no ladder
    auto settlementAt = [&](int X, int Y) -> Settlement * {
        for (auto &s : w.settlements)
            if (s.inside(X, Y) && s.ch(X, Y) != ' ') return &s;
        return nullptr;
    };
    auto freeFloor = [&](int X, int Y, int Z) {
        if (w.linkAt(X, Y, Z)) return false;
        const Tile *t = nullptr;
        if (Z == 0) {
            Settlement *s = settlementAt(X, Y);
            if (!s) return false;
            t = &s->tileAt(X, Y);
        } else {
            t = w.tileAt(X, Y, Z);
        }
        if (!t || t->terrain == T_VOID || t->terrain == T_WATER || t->terrain == T_DEEP) return false;
        return t->feature == F_NONE && (t->wall == W_NONE);
    };
    auto tileOf = [&](int X, int Y, int Z) -> const Tile * {
        if (Z != 0) return w.tileAt(X, Y, Z);
        Settlement *s = settlementAt(X, Y);
        return s ? &s->tileAt(X, Y) : nullptr;
    };
    auto buildingMatches = [&](int bld, const std::string &filter) {
        if (filter.empty()) return true;
        if (bld <= 0 || bld > (int)w.buildings.size()) return false;
        const Building &b = w.buildings[bld - 1];
        if (filter[0] == '"') return lower(b.name).find(lower(filter.substr(1, filter.size() - 2))) != std::string::npos;
        return b.kind == filter;
    };
    auto keyOf = [](int kind, int X, int Y, int Z, int slot) {
        return hash32((uint32_t)kind * 2654435761u ^ hash2i(X, Y, (uint32_t)(Z + 64) * 131u + (uint32_t)slot)) | 1u;
    };
    static const float slotOff[3][2] = {{0.3f, 0.52f}, {0.55f, 0.42f}, {0.75f, 0.56f}};
    std::map<int64_t, int> used;   // things already on a tile
    auto putAt = [&](int kind, int count, int X, int Y, int Z, float h, int bld) {
        int &k = used[World::key3(X, Y, Z)];
        if (k >= 3) return false;
        Thing t;
        t.kind = kind; t.count = count; t.x = X; t.y = Y; t.z = Z; t.h = h; t.owner = bld;
        t.ox = slotOff[k][0]; t.oy = slotOff[k][1];
        t.facing = (float)(hash01(X * 3 + k, Y, 91) * 6.2832);
        t.key = keyOf(kind, X, Y, Z, k);
        add(t);
        k++;
        return true;
    };

    int ruleNo = 0;
    for (const std::string &r : rules) {
        ruleNo++;
        size_t colon = r.find(':');
        if (colon == std::string::npos) { std::fprintf(stderr, "items.txt: cannot read \"%s\"\n", r.c_str()); continue; }
        std::string head = trim(r.substr(0, colon)), body = trim(r.substr(colon + 1));
        // the things of the rule: ITEM [CHANCE] ...
        std::vector<std::pair<int, float>> list;
        {
            std::istringstream bs(body);
            std::string tok;
            while (bs >> tok) {
                if (!list.empty() && (std::isdigit((unsigned char)tok[0]) || tok[0] == '.')) { list.back().second = (float)std::atof(tok.c_str()); continue; }
                int k = find(tok);
                if (k < 0) { std::fprintf(stderr, "items.txt: no item called %s\n", tok.c_str()); continue; }
                list.push_back({k, 1.0f});
            }
        }
        std::istringstream hs(head);
        std::string verb;
        hs >> verb;
        if (verb == "moored" || verb == "harbour") continue;   // after all the others, below
        if (verb == "on" || verb == "beside") {
            std::string fw, in, filter;
            hs >> fw >> in;
            std::getline(hs, filter);
            filter = trim(filter);
            auto fit = featureWords().find(fw);
            if (fit == featureWords().end()) { std::fprintf(stderr, "items.txt: no furniture called %s\n", fw.c_str()); continue; }
            for (const Spot &sp : spots) {
                if (sp.feature != fit->second || !buildingMatches(sp.bld, filter)) continue;
                int X = sp.x, Y = sp.y;
                float h = surface(sp.feature);
                if (verb == "beside") {   // a free floor tile next to it
                    static const int d8[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
                    bool found = false;
                    for (int q = 0; q < 8 && !found; q++) {
                        int nx = sp.x + d8[q][0], ny = sp.y + d8[q][1];
                        if (freeFloor(nx, ny, sp.z) && used[World::key3(nx, ny, sp.z)] < 2) { X = nx; Y = ny; found = true; }
                    }
                    if (!found) continue;
                    h = 0;
                }
                for (size_t j = 0; j < list.size(); j++) {
                    float c = list[j].second;
                    int n = c > 1 ? (int)c : 1;
                    if (c <= 1 && hash01(X * 7 + (int)j, Y * 13 + sp.z, (uint32_t)ruleNo * 101u + 5) >= c) continue;
                    putAt(list[j].first, n, X, Y, sp.z, h, sp.bld);
                }
            }
        } else if (verb == "art") {
            std::string where;
            std::getline(hs, where);
            where = trim(where);
            // art statue near "NAME": ITEM ...   each on the free statue nearest that building:
            // standing statues (giant, group, pieta) on a pedestal ('y'), the others ('none')
            // are the bronze rider of an equestrian statue ('s')
            if (where.rfind("statue near", 0) == 0) {
                size_t a = where.find('"'), b = where.rfind('"');
                std::string name = a != std::string::npos && b > a ? lower(where.substr(a + 1, b - a - 1)) : "";
                int bid = 0;
                for (size_t i = 0; i < w.buildings.size(); i++)
                    if (lower(w.buildings[i].name).find(name) != std::string::npos) { bid = (int)i + 1; break; }
                if (!bid) { std::fprintf(stderr, "items.txt: no building called %s\n", name.c_str()); continue; }
                const Building &B = w.buildings[bid - 1];
                float cx = (B.x0 + B.x1 + 1) * 0.5f, cy = (B.y0 + B.y1 + 1) * 0.5f;
                for (auto &it : list) {
                    bool pedestal = kinds[it.first].shape != "none";
                    const Spot *best = nullptr;
                    float bd = 40;
                    for (const Spot &sp : spots) {
                        const Tile *t = tileOf(sp.x, sp.y, 0);
                        if (sp.feature != F_STATUE || sp.z != 0 || !t || (t->var == 1) != pedestal || used[World::key3(sp.x, sp.y, 0)]) continue;
                        float d = std::hypot(sp.x + 0.5f - cx, sp.y + 0.5f - cy);
                        if (d < bd) { bd = d; best = &sp; }
                    }
                    if (!best) { std::fprintf(stderr, "items.txt: no free %s near %s\n", pedestal ? "pedestal" : "statue", name.c_str()); continue; }
                    Thing th;
                    th.kind = it.first; th.x = best->x; th.y = best->y; th.z = 0; th.h = pedestal ? 1.2f : 0;
                    th.ox = th.oy = 0.5f;
                    th.facing = std::atan2(cx - (best->x + 0.5f), -(cy - (best->y + 0.5f)));   // looking away from the building
                    th.key = keyOf(th.kind, th.x, th.y, 0, 0);
                    add(th);
                    used[World::key3(th.x, th.y, 0)] = 3;
                }
                continue;
            }
            // art statue in "TOWN"
            if (where.rfind("statue", 0) == 0) {
                std::string town = trim(where.substr(where.find("in") + 2));
                if (!town.empty() && town[0] == '"') town = town.substr(1, town.size() - 2);
                for (auto &s : w.settlements) {
                    if (lower(s.name) != lower(town)) continue;
                    for (const Spot &sp : spots)
                        if (sp.feature == F_STATUE && s.inside(sp.x, sp.y) && !list.empty()) {
                            putAt(list[0].first, 1, sp.x, sp.y, 0, 0, 0);
                            break;
                        }
                }
                continue;
            }
            // art "NAME" wall | floor | row | on FEATURE | over FEATURE
            size_t q1 = where.find('"'), q2 = where.find('"', q1 + 1);
            if (q1 == std::string::npos || q2 == std::string::npos) { std::fprintf(stderr, "items.txt: cannot read \"%s\"\n", r.c_str()); continue; }
            std::string name = lower(where.substr(q1 + 1, q2 - q1 - 1)), how = trim(where.substr(q2 + 1));
            int bid = 0;
            for (size_t b = 0; b < w.buildings.size(); b++)
                if (lower(w.buildings[b].name).find(name) != std::string::npos) { bid = (int)b + 1; break; }
            if (!bid) { std::fprintf(stderr, "items.txt: no building called %s\n", name.c_str()); continue; }
            const Building &B = w.buildings[bid - 1];
            if (how == "upstairs") {
                // along the outer walls of the building's upper floors (the chained books of Duke
                // Humfrey's Library), evenly round the room, keeping clear of the stairs
                struct UC { int x, y, z; float ang; };
                std::vector<UC> uc;
                float cx = (B.x0 + B.x1 + 1) * 0.5f, cy = (B.y0 + B.y1 + 1) * 0.5f;
                for (const Level &L : w.levels) {
                    if (L.bld != bid || L.z < 1) continue;
                    for (int Y = L.y0 + 1; Y < L.y0 + L.h - 1; Y++)
                        for (int X = L.x0 + 1; X < L.x0 + L.w - 1; X++) {
                            if (!freeFloor(X, Y, L.z) || used[World::key3(X, Y, L.z)]) continue;
                            bool nearStair = false;
                            for (int dy = -1; dy <= 1; dy++)
                                for (int dx = -1; dx <= 1; dx++)
                                    if (w.linkAt(X + dx, Y + dy, L.z)) nearStair = true;
                            if (nearStair) continue;
                            bool outer = X == L.x0 + 1 || Y == L.y0 + 1 || X == L.x0 + L.w - 2 || Y == L.y0 + L.h - 2;
                            if (!outer) continue;
                            uc.push_back({X, Y, L.z, std::atan2(Y + 0.5f - cy, X + 0.5f - cx)});
                        }
                }
                if (uc.size() < list.size()) { std::fprintf(stderr, "items.txt: no room upstairs in %s\n", name.c_str()); continue; }
                std::sort(uc.begin(), uc.end(), [](const UC &a, const UC &b) { return a.ang < b.ang; });
                for (size_t j = 0; j < list.size(); j++) {
                    const UC &c = uc[(j * uc.size()) / list.size()];
                    Thing t;
                    t.kind = list[j].first; t.x = c.x; t.y = c.y; t.z = c.z; t.owner = bid;
                    t.key = keyOf(t.kind, c.x, c.y, c.z, 0);
                    t.facing = std::atan2(-(cx - (c.x + 0.5f)), cy - (c.y + 0.5f));   // towards the middle
                    add(t);
                    used[World::key3(c.x, c.y, c.z)] = 3;
                }
                continue;
            }
            if (how == "behind altar") {   // high in the wall behind the altar: an east window
                std::vector<const Spot *> alts;
                for (const Spot &sp : spots)
                    if (sp.bld == bid && sp.feature == F_ALTAR && sp.z == 0) alts.push_back(&sp);
                if (alts.empty() || list.empty()) { std::fprintf(stderr, "items.txt: no altar in %s\n", name.c_str()); continue; }
                float ax = 0, ay = 0;
                for (auto *a : alts) { ax += a->x + 0.5f; ay += a->y + 0.5f; }
                ax /= alts.size(); ay /= alts.size();
                float bx = (B.x0 + B.x1 + 1) * 0.5f, by = (B.y0 + B.y1 + 1) * 0.5f;
                int sd = std::fabs(ax - bx) >= std::fabs(ay - by) ? (ax > bx ? 1 : 0) : (ay > by ? 3 : 2);
                static const int d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                int X = alts[0]->x, Y = alts[0]->y;
                for (int k2 = 0; k2 < 30; k2++) {   // on from the altar to the wall
                    const Tile *n = tileOf(X + d4[sd][0], Y + d4[sd][1], 0);
                    if (!n || n->wall != W_NONE || n->bld != bid) break;
                    X += d4[sd][0]; Y += d4[sd][1];
                }
                Thing th;
                th.kind = list[0].first; th.x = X; th.y = Y; th.z = 0; th.h = 0; th.owner = bid; th.side = sd;
                th.ox = sd < 2 ? 0.5f : ax - X;   // centred on the altar (between the halves of a double one)
                th.oy = sd < 2 ? ay - Y : 0.5f;
                th.key = keyOf(th.kind, X, Y, 0, 0);
                add(th);
                used[World::key3(X, Y, 0)] = 3;
                continue;
            }
            if (how == "left of altar") {   // beside the altar, on its left as you face it, against the side wall
                std::vector<const Spot *> alts;   // (Michelangelo's Christ at the Minerva, left of the high altar)
                for (const Spot &sp : spots)
                    if (sp.bld == bid && sp.feature == F_ALTAR && sp.z == 0) alts.push_back(&sp);
                if (alts.empty() || list.empty()) { std::fprintf(stderr, "items.txt: no altar in %s\n", name.c_str()); continue; }
                float ax = 0, ay = 0;
                for (auto *a : alts) { ax += a->x + 0.5f; ay += a->y + 0.5f; }
                ax /= alts.size(); ay /= alts.size();
                float bx = (B.x0 + B.x1 + 1) * 0.5f, by = (B.y0 + B.y1 + 1) * 0.5f;
                int dx = 0, dy = 0;   // from the nave towards the altar
                if (std::fabs(ax - bx) >= std::fabs(ay - by)) dx = ax > bx ? 1 : -1; else dy = ay > by ? 1 : -1;
                int lx = -dy, ly = dx;   // the left hand, facing the altar (y is north)
                const Spot *start = alts[0];
                for (auto *a : alts) if (a->x * lx + a->y * ly > start->x * lx + start->y * ly) start = a;
                int X = start->x, Y = start->y;
                for (int k2 = 0; k2 < 30; k2++) {   // on from the altar's end to the wall
                    const Tile *n = tileOf(X + lx, Y + ly, 0);
                    if (!n || n->wall != W_NONE || n->bld != bid || n->feature == F_PILLAR) break;
                    X += lx; Y += ly;
                }
                if (X == start->x && Y == start->y) { std::fprintf(stderr, "items.txt: no room left of the altar in %s\n", name.c_str()); continue; }
                Thing th;
                th.kind = list[0].first; th.x = X; th.y = Y; th.z = 0; th.owner = bid;
                th.facing = std::atan2((float)dx, (float)-dy);   // towards the nave
                th.key = keyOf(th.kind, X, Y, 0, 0);
                add(th);
                used[World::key3(X, Y, 0)] = 3;
                continue;
            }
            if (how.rfind("on ", 0) == 0) {
                auto fit = featureWords().find(trim(how.substr(3)));
                if (fit == featureWords().end()) continue;
                size_t j = 0;
                for (const Spot &sp : spots)
                    if (j < list.size() && sp.bld == bid && sp.feature == fit->second && sp.z == 0)
                        putAt(list[j++].first, 1, sp.x, sp.y, 0, surface(sp.feature), bid);
                continue;
            }
            if (how.rfind("over ", 0) == 0) {   // standing on the floor over it (a canopy over an altar)
                auto fit = featureWords().find(trim(how.substr(5)));
                if (fit == featureWords().end() || list.empty()) continue;
                for (const Spot &sp : spots) {
                    if (sp.bld != bid || sp.feature != fit->second || sp.z != 0) continue;
                    Thing th;
                    th.kind = list[0].first; th.x = sp.x; th.y = sp.y; th.z = 0; th.h = 0; th.owner = bid;
                    // between the two halves of a double altar
                    auto same = [&](int dx, int dy) { const Tile *q = tileOf(sp.x + dx, sp.y + dy, 0); return q && q->feature == sp.feature; };
                    th.ox = same(1, 0) ? 1.0f : (same(-1, 0) ? 0.0f : 0.5f);
                    th.oy = same(0, 1) ? 1.0f : (same(0, -1) ? 0.0f : 0.5f);
                    th.key = keyOf(th.kind, th.x, th.y, 0, 0);
                    add(th);
                    for (int dy = -2; dy <= 2; dy++)   // nothing else is put under or against it
                        for (int dx = -2; dx <= 2; dx++) used[World::key3(th.x + dx, th.y + dy, 0)] = 3;
                    break;
                }
                continue;
            }
            // "inner wall:", "inner floor:": only in the room at the building's middle (the Louvre's
            // inner chamber), the tiles reached from there without passing a wall or a door
            std::set<std::pair<int, int>> inner;
            if (how.rfind("inner ", 0) == 0) {
                how = trim(how.substr(6));
                int sx = (B.x0 + B.x1) / 2, sy = (B.y0 + B.y1) / 2;
                std::vector<std::pair<int, int>> stack{{sx, sy}};
                while (!stack.empty()) {
                    auto [X, Y] = stack.back();
                    stack.pop_back();
                    if (X < B.x0 || X > B.x1 || Y < B.y0 || Y > B.y1 || inner.count({X, Y})) continue;
                    const Tile *t = tileOf(X, Y, 0);
                    if (!t || t->bld != bid || t->wall != W_NONE) continue;
                    inner.insert({X, Y});
                    stack.push_back({X + 1, Y}); stack.push_back({X - 1, Y}); stack.push_back({X, Y + 1}); stack.push_back({X, Y - 1});
                }
                if (inner.empty()) { std::fprintf(stderr, "items.txt: no inner room in %s\n", name.c_str()); continue; }
                if (std::getenv("GALILEO_DEBUG_INNER")) {
                    const Tile *t = tileOf(sx, sy, 0);
                    std::fprintf(stderr, "inner room of %s: %zu tiles from %d,%d, terrain %d\n", name.c_str(), inner.size(), sx, sy, t ? t->terrain : -1);
                }
            }
            // candidate tiles inside the building, on the ground floor
            struct Cand { int x, y, side; float score; };
            std::vector<Cand> cands;
            float cx = (B.x0 + B.x1) * 0.5f, cy = (B.y0 + B.y1) * 0.5f;
            for (int Y = B.y0; Y <= B.y1; Y++)
                for (int X = B.x0; X <= B.x1; X++) {
                    const Tile *t = tileOf(X, Y, 0);
                    if (!t || t->bld != bid || !freeFloor(X, Y, 0) || used[World::key3(X, Y, 0)]) continue;
                    if (!inner.empty() && !inner.count({X, Y})) continue;
                    // indoors only, not in a courtyard; or ("court", "court wall") only in its courtyard,
                    // for the fountain of the Lions and the plaster of the Alhambra
                    bool indoor = t->terrain == T_FLOOR_WOOD || t->terrain == T_FLOOR_STONE || t->terrain == T_FLOOR_TILE;
                    bool court = how.rfind("court", 0) == 0;
                    if (indoor == court) continue;
                    if (how == "wall" || how == "court wall") {
                        static const int d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                        for (int s = 0; s < 4; s++) {
                            const Tile *n = tileOf(X + d4[s][0], Y + d4[s][1], 0);
                            // an inner wall, or (in a roofed fortress, the Waag) the thick outer one
                            if (n && (n->wall == W_WALL || n->wall == W_CITYWALL || n->wall == W_TOWER)) { cands.push_back({X, Y, s, 0}); break; }
                        }
                    } else {
                        // the floor: not against a wall, not in a doorway
                        bool open = true;
                        for (int dy = -1; dy <= 1 && open; dy++)
                            for (int dx = -1; dx <= 1 && open; dx++) {
                                const Tile *n = tileOf(X + dx, Y + dy, 0);
                                if (!n || n->wall != W_NONE || n->bld != bid) open = false;
                            }
                        if (open || how == "row") cands.push_back({X, Y, -1, 0});
                    }
                }
            // in a row (two tiles apart, the same way), as near the middle as can be
            if (how == "row") {
                std::set<std::pair<int, int>> ok;
                for (auto &c : cands) ok.insert({c.x, c.y});
                int n = (int)list.size(), bx = 0, by = 0, bdx = 0, bdy = 0;
                float bd = 1e9f;
                for (auto &c : cands)
                    for (int dir = 0; dir < 2; dir++) {
                        int dx = dir ? 0 : 2, dy = dir ? 2 : 0;
                        bool all = true;
                        for (int k = 1; k < n && all; k++) all = ok.count({c.x + dx * k, c.y + dy * k}) > 0;
                        float d = std::hypot(c.x + dx * (n - 1) * 0.5f - cx, c.y + dy * (n - 1) * 0.5f - cy);
                        if (all && d < bd) { bd = d; bx = c.x; by = c.y; bdx = dx; bdy = dy; }
                    }
                if (bd > 1e8f) { std::fprintf(stderr, "items.txt: no room for a row in %s\n", name.c_str()); continue; }
                for (int k = 0; k < n; k++) {
                    Thing t;
                    t.kind = list[k].first; t.x = bx + bdx * k; t.y = by + bdy * k; t.z = 0; t.owner = bid;
                    t.key = keyOf(t.kind, t.x, t.y, 0, 0);
                    // facing across the room
                    t.facing = bdx ? (cy > t.y + 0.5f ? 0.0f : 3.14159f) : (cx > t.x + 0.5f ? -1.5708f : 1.5708f);
                    add(t);
                    used[World::key3(t.x, t.y, 0)] = 3;
                }
                continue;
            }
            // spread them out: each one as far as possible from the others, starting near the middle
            std::vector<Cand> chosen;
            for (const Thing &o : things)   // keep away from works of art already put in this building
                if (o.owner == bid && o.z == 0 && kinds[o.kind].fixed) chosen.push_back({o.x, o.y, -1, 0});
            for (size_t j = 0; j < list.size() && !cands.empty(); j++) {
                int best = -1;
                float bs = -1e9f;
                for (size_t c = 0; c < cands.size(); c++) {
                    float d = 1e9f;
                    for (auto &o : chosen) d = std::min(d, std::hypot((float)(cands[c].x - o.x), (float)(cands[c].y - o.y)));
                    float s = chosen.empty() ? -std::hypot(cands[c].x - cx, cands[c].y - cy) : std::min(d, 6.0f) - 0.05f * std::hypot(cands[c].x - cx, cands[c].y - cy);
                    if (s > bs) { bs = s; best = (int)c; }
                }
                Cand c = cands[best];
                chosen.push_back(c);
                Thing t;
                t.kind = list[j].first; t.x = c.x; t.y = c.y; t.z = 0; t.owner = bid; t.side = c.side;
                t.key = keyOf(t.kind, c.x, c.y, 0, 0);
                // works of art on the floor face the middle of the building
                t.facing = std::atan2(-(cx - (c.x + 0.5f)), cy - (c.y + 0.5f));
                add(t);
                used[World::key3(c.x, c.y, 0)] = 3;
                cands.erase(cands.begin() + best);
            }
        }
    }
    // boats at their moorings: "moored: ITEM" everywhere, "moored in "TOWN": ITEM" in one town
    std::map<std::string, int> boatFor;   // town (lower case, "" for anywhere) -> kind
    for (const std::string &r : rules) {
        if (r.rfind("moored", 0) != 0) continue;
        size_t colon = r.find(':');
        if (colon == std::string::npos) continue;
        std::string head = trim(r.substr(6, colon - 6)), item = trim(r.substr(colon + 1)), town;
        size_t q1 = head.find('"'), q2 = head.rfind('"');
        if (q1 != std::string::npos && q2 > q1) town = lower(head.substr(q1 + 1, q2 - q1 - 1));
        int k = find(item);
        if (k < 0 || !kinds[k].boat) { std::fprintf(stderr, "items.txt: %s is not a boat\n", item.c_str()); continue; }
        boatFor[town] = k;
    }
    for (auto &s : w.settlements) {
        auto it = boatFor.find(lower(s.name));
        if (it == boatFor.end()) it = boatFor.find("");
        if (it == boatFor.end()) continue;
        for (int Y = s.y0; Y < s.y0 + s.h; Y++)
            for (int X = s.x0; X < s.x0 + s.w; X++) {
                if (s.ch(X, Y) == ' ') continue;
                const Tile &t = s.tileAt(X, Y);
                if (t.feature != F_MOORING || t.var != 1) continue;
                // lie along the bank: bow and stern towards water, not into the quay
                auto wet = [&](int x, int y) { const Tile &n = w.tile(x, y); return n.terrain == T_WATER || n.terrain == T_DEEP; };
                bool alongY = wet(X, Y - 1) || wet(X, Y + 1), alongX = wet(X - 1, Y) || wet(X + 1, Y);
                bool landEW = !wet(X - 1, Y) || !wet(X + 1, Y), landNS = !wet(X, Y - 1) || !wet(X, Y + 1);
                float facing = (alongY && (landEW || !alongX)) ? 0.0f : 1.5708f;
                if (!alongY && !alongX) facing = landNS ? 1.5708f : 0.0f;
                Thing th;
                th.kind = it->second; th.x = X; th.y = Y; th.z = 0; th.h = 0; th.ox = th.oy = 0.5f;
                th.facing = facing;
                th.key = keyOf(th.kind, X, Y, 0, 7);
                add(th);
            }
    }
    // ships in harbours: "harbour X,Y: ITEM", X,Y on the old map; the ship lies on the nearest
    // water beside land (to be boarded from the quay or the shore), along the shore
    for (const std::string &r : rules) {
        if (r.rfind("harbour", 0) != 0) continue;
        size_t colon = r.find(':');
        float hx, hy;
        if (colon == std::string::npos || std::sscanf(r.c_str() + 7, " %f,%f", &hx, &hy) != 2) continue;
        int k = find(trim(r.substr(colon + 1)));
        if (k < 0 || !kinds[k].boat) { std::fprintf(stderr, "items.txt: %s is not a boat\n", r.c_str()); continue; }
        int cx = (int)std::lround(hx * w.S), cy = (int)std::lround(hy * w.S), bx = 0, by = 0;
        float bf = 0;
        if (!shoreSpot(w, cx, cy, 40, bx, by, bf)) { std::fprintf(stderr, "items.txt: no water beside land for %s\n", r.c_str()); continue; }
        Thing th;
        th.kind = k; th.x = bx; th.y = by; th.z = 0; th.h = 0; th.ox = th.oy = 0.5f; th.facing = bf;
        th.key = keyOf(th.kind, bx, by, 0, 8);
        add(th);
    }
    version++;
}

int Items::boatAt(int X, int Y) const {
    for (int i : at(X, Y, 0))
        if (kinds[things[i].kind].boat) return i;
    return -1;
}

bool Items::shoreSpot(World &w, int cx, int cy, int radius, int &bx, int &by, float &facing) const {
    float bd = 1e9f;
    auto wet = [&](int x, int y) { const Tile &n = w.tile(x, y); return n.terrain == T_WATER || n.terrain == T_DEEP; };
    for (int Y = cy - radius; Y <= cy + radius; Y++)
        for (int X = cx - radius; X <= cx + radius; X++) {
            if (!w.inWorld(X, Y) || !wet(X, Y) || boatAt(X, Y) >= 0) continue;
            const Tile &t = w.tile(X, Y);
            if (t.feature == F_MOORING) continue;
            bool landEW = w.passable(X - 1, Y) || w.passable(X + 1, Y), landNS = w.passable(X, Y - 1) || w.passable(X, Y + 1);
            if (!landEW && !landNS) continue;
            // room along the shore for the hull
            bool alongNS = landEW && wet(X, Y - 1) && wet(X, Y + 1), alongEW = landNS && wet(X - 1, Y) && wet(X + 1, Y);
            if (!alongNS && !alongEW) continue;
            float d = std::hypot((float)(X - cx), (float)(Y - cy));
            if (d < bd) { bd = d; bx = X; by = Y; facing = alongNS ? 0.0f : 1.5708f; }
        }
    return bd < 1e8f;
}

void Items::moor(int kind, int X, int Y, float facing) {
    Thing t;
    t.kind = kind; t.x = X; t.y = Y; t.z = 0; t.h = 0; t.ox = t.oy = 0.5f; t.facing = facing;
    add(t);
    version++;
}

// ------------------------------------------------------------------ how things look

// A hull from stations along its length: at each, the half-beam at the gunwale and the
// height of the gunwale above the water. Bow at +y before turning by the facing.
// a ship: a round-bellied hull with a high stern, three masts with square sails, a bowsprit,
// a flag (colour2 the sails, the third colour of "shape" none: the flag is red and white)
static void drawShip(MeshBuf &m, const ItemKind &k, vec3 p, float facing) {
    vec3 hull = k.color, sail = k.color2, wood(0.36f, 0.26f, 0.17f), deck(0.62f, 0.5f, 0.34f);
    float cr = std::cos(facing), sr = std::sin(facing);
    auto W = [&](float x, float y, float z) { return vec3(p.x + x * cr - y * sr, p.y + x * sr + y * cr, p.z + z); };
    struct St { float y, half, top; };
    std::vector<St> st = {{-2.0f, 0.5f, 1.3f}, {-1.6f, 0.72f, 1.15f}, {-0.6f, 0.82f, 0.75f}, {0.4f, 0.8f, 0.72f},
                          {1.3f, 0.62f, 0.8f}, {1.9f, 0.3f, 0.95f}, {2.2f, 0.02f, 1.05f}};
    const float keel = -0.25f, deckZ = 0.62f;
    for (size_t i = 0; i + 1 < st.size(); i++) {
        const St &a = st[i], &b = st[i + 1];
        float ab = a.half * 0.35f, bb = b.half * 0.35f;
        for (int sd = -1; sd <= 1; sd += 2) {
            vec3 out = W((float)sd, (a.y + b.y) * 0.5f, 0) - W(0, (a.y + b.y) * 0.5f, 0);
            m.quad(W(sd * ab, a.y, keel), W(sd * bb, b.y, keel), W(sd * b.half, b.y, 0.3f), W(sd * a.half, a.y, 0.3f), hull * 0.85f, out);
            m.quad(W(sd * a.half, a.y, 0.3f), W(sd * b.half, b.y, 0.3f), W(sd * b.half, b.y, b.top), W(sd * a.half, a.y, a.top), hull, out);
            m.quad(W(sd * a.half * 0.92f, a.y, a.top), W(sd * b.half * 0.92f, b.y, b.top), W(sd * b.half * 0.92f, b.y, deckZ), W(sd * a.half * 0.92f, a.y, deckZ), hull * 0.7f, out * -1.0f);
        }
        m.quad(W(-a.half * 0.92f, a.y, deckZ), W(a.half * 0.92f, a.y, deckZ), W(b.half * 0.92f, b.y, deckZ), W(-b.half * 0.92f, b.y, deckZ), deck, vec3(0, 0, 1));
    }
    // the stern: a flat transom, the sterncastle and the poop
    const St &s0 = st[0];
    m.quad(W(-s0.half, s0.y, s0.top), W(s0.half, s0.y, s0.top), W(s0.half * 0.35f, s0.y, keel), W(-s0.half * 0.35f, s0.y, keel), hull * 0.9f, W(0, -1, 0) - W(0, 0, 0));
    auto box = [&](float x0, float y0, float x1, float y1, float z0, float z1, vec3 c) {
        vec3 a = W(x0, y0, z0), b = W(x1, y0, z0), cc = W(x1, y1, z0), d = W(x0, y1, z0);
        vec3 a2 = W(x0, y0, z1), b2 = W(x1, y0, z1), c2 = W(x1, y1, z1), d2 = W(x0, y1, z1);
        m.quad(a2, b2, c2, d2, c, vec3(0, 0, 1));
        m.quad(a, b, b2, a2, c * 0.8f, W(0, -1, 0) - W(0, 0, 0));
        m.quad(cc, d, d2, c2, c * 0.8f, W(0, 1, 0) - W(0, 0, 0));
        m.quad(b, cc, c2, b2, c * 0.9f, W(1, 0, 0) - W(0, 0, 0));
        m.quad(d, a, a2, d2, c * 0.9f, W(-1, 0, 0) - W(0, 0, 0));
    };
    box(-0.55f, -2.0f, 0.55f, -1.1f, deckZ, 1.35f, hull * 1.05f);
    box(-0.45f, 1.45f, 0.45f, 1.95f, deckZ, 1.0f, hull * 1.05f);   // the forecastle
    // masts and yards and sails: fore, main, mizzen
    struct Mast { float y, h, w; };
    for (const Mast &ms : {Mast{1.1f, 3.4f, 1.0f}, Mast{0.0f, 4.2f, 1.3f}, Mast{-1.35f, 2.8f, 0.7f}}) {
        box(-0.05f, ms.y - 0.05f, 0.05f, ms.y + 0.05f, deckZ, deckZ + ms.h, wood);
        for (int tier = 0; tier < 2; tier++) {
            float z1 = deckZ + ms.h * (tier ? 0.95f : 0.62f), z0 = deckZ + ms.h * (tier ? 0.66f : 0.25f), hw = ms.w * (tier ? 0.75f : 1.0f);
            box(-hw - 0.05f, ms.y - 0.03f, hw + 0.05f, ms.y + 0.03f, z1, z1 + 0.06f, wood);   // the yard
            // the sail, bellied a little forward
            vec3 n = W(0, 1, 0) - W(0, 0, 0);
            m.quad(W(-hw, ms.y + 0.05f, z0), W(hw, ms.y + 0.05f, z0), W(hw * 0.95f, ms.y + 0.15f, z1), W(-hw * 0.95f, ms.y + 0.15f, z1), sail, n);
            m.quad(W(hw, ms.y + 0.04f, z0), W(-hw, ms.y + 0.04f, z0), W(-hw * 0.95f, ms.y + 0.14f, z1), W(hw * 0.95f, ms.y + 0.14f, z1), sail * 0.9f, n * -1.0f);
        }
    }
    // the bowsprit, and a flag at the mainmast head
    vec3 b0 = W(0, 1.9f, 1.0f), b1 = W(0, 3.0f, 1.6f);
    m.quad(b0 + vec3(0.04f, 0, 0), b1 + vec3(0.04f, 0, 0), b1 + vec3(-0.04f, 0, 0), b0 + vec3(-0.04f, 0, 0), wood, vec3(0, 0, 1));
    float fz = deckZ + 4.2f;
    m.quad(W(0, 0, fz), W(0, -0.7f, fz), W(0, -0.7f, fz + 0.35f), W(0, 0, fz + 0.35f), vec3(0.75f, 0.12f, 0.1f), W(1, 0, 0) - W(0, 0, 0));
    m.quad(W(0, -0.7f, fz), W(0, 0, fz), W(0, 0, fz + 0.35f), W(0, -0.7f, fz + 0.35f), vec3(0.95f, 0.93f, 0.88f), W(-1, 0, 0) - W(0, 0, 0));
}

void drawBoat(MeshBuf &m, const ItemKind &k, vec3 p, float facing) {
    if (k.shape == "ship") { drawShip(m, k, p, facing); return; }
    bool gondola = k.shape == "gondola";
    vec3 c = k.color, c2 = k.color2;
    float cr = std::cos(facing), sr = std::sin(facing);
    const float S = gondola ? 1.15f : 1.3f;   // a little larger than life, as everyday things are
    auto W = [&](float x, float y, float z) { x *= S; y *= S; z *= S; return vec3(p.x + x * cr - y * sr, p.y + x * sr + y * cr, p.z + z); };
    struct St { float y, half, top; };
    std::vector<St> st;
    if (gondola)   // long and narrow, both ends rising, a little lopsided as gondolas are
        st = {{-1.45f, 0.02f, 0.62f}, {-1.2f, 0.14f, 0.36f}, {-0.7f, 0.25f, 0.24f}, {0.0f, 0.29f, 0.22f},
              {0.7f, 0.25f, 0.24f}, {1.2f, 0.13f, 0.36f}, {1.45f, 0.02f, 0.58f}};
    else
        st = {{-0.82f, 0.22f, 0.3f}, {-0.45f, 0.3f, 0.27f}, {0.05f, 0.32f, 0.26f}, {0.5f, 0.25f, 0.28f}, {0.86f, 0.02f, 0.36f}};
    const float keel = -0.06f, floorZ = 0.07f;
    for (size_t i = 0; i + 1 < st.size(); i++) {
        const St &a = st[i], &b = st[i + 1];
        float ab = a.half * 0.45f, bb = b.half * 0.45f;   // the bottom is narrower than the gunwale
        for (int sd = -1; sd <= 1; sd += 2) {
            vec3 out = W((float)sd, (a.y + b.y) * 0.5f, 0) - W(0, (a.y + b.y) * 0.5f, 0);
            m.quad(W(sd * ab, a.y, keel), W(sd * bb, b.y, keel), W(sd * b.half, b.y, b.top), W(sd * a.half, a.y, a.top), c, out);
            // the inside of the side, seen over the gunwale
            m.quad(W(sd * a.half * 0.9f, a.y, a.top), W(sd * b.half * 0.9f, b.y, b.top), W(sd * bb, b.y, floorZ), W(sd * ab, a.y, floorZ), c2 * 0.8f, out * -1.0f);
        }
        m.quad(W(-ab, a.y, floorZ), W(ab, a.y, floorZ), W(bb, b.y, floorZ), W(-bb, b.y, floorZ), c2, vec3(0, 0, 1));
    }
    if (!gondola) {
        // the transom across the stern, two thwarts, and the oars shipped along the sides
        const St &s0 = st[0];
        m.quad(W(-s0.half, s0.y, s0.top), W(s0.half, s0.y, s0.top), W(s0.half * 0.45f, s0.y, keel), W(-s0.half * 0.45f, s0.y, keel), c * 0.9f, W(0, -1, 0) - W(0, 0, 0));
        for (float y : {-0.35f, 0.25f}) {
            vec3 lo = W(-0.29f, y - 0.06f, 0.2f), hi = W(0.29f, y + 0.06f, 0.24f);
            float hw = 0.29f;
            m.quad(W(-hw, y - 0.06f, 0.24f), W(hw, y - 0.06f, 0.24f), W(hw, y + 0.06f, 0.24f), W(-hw, y + 0.06f, 0.24f), c2 * 0.95f, vec3(0, 0, 1));
            (void)lo; (void)hi;
        }
        for (int sd = -1; sd <= 1; sd += 2)
            m.quad(W(sd * 0.27f, -0.6f, 0.3f), W(sd * 0.3f, -0.6f, 0.3f), W(sd * 0.3f, 0.75f, 0.3f), W(sd * 0.27f, 0.75f, 0.3f), vec3(0.62f, 0.5f, 0.34f), vec3(0, 0, 1));
    } else {
        // the ferro at the bow: an iron blade with six teeth; the felze, a little black cabin
        vec3 iron(0.78f, 0.78f, 0.8f);
        float yb = 1.42f;
        m.quad(W(0, yb - 0.05f, 0.4f), W(0, yb + 0.12f, 0.45f), W(0, yb + 0.14f, 0.85f), W(0, yb - 0.02f, 0.8f), iron, W(1, 0, 0) - W(0, 0, 0));
        m.quad(W(0, yb - 0.02f, 0.8f), W(0, yb + 0.14f, 0.85f), W(0, yb + 0.12f, 0.45f), W(0, yb - 0.05f, 0.4f), iron, W(-1, 0, 0) - W(0, 0, 0));
        for (int t = 0; t < 6; t++) {
            float z = 0.46f + t * 0.06f;
            m.quad(W(-0.015f, yb + 0.12f, z), W(0.015f, yb + 0.12f, z), W(0.015f, yb + 0.22f, z + 0.02f), W(-0.015f, yb + 0.22f, z + 0.02f), iron, vec3(0, 0, 1));
        }
        vec3 felt = c * 0.9f + vec3(0.02f, 0.02f, 0.02f);
        float fy0 = -0.15f, fy1 = 0.45f, fw = 0.22f, fz0 = 0.2f, fz1 = 0.55f;
        m.quad(W(-fw, fy0, fz1), W(fw, fy0, fz1), W(fw, fy1, fz1), W(-fw, fy1, fz1), felt, vec3(0, 0, 1));
        m.quad(W(-fw, fy0, fz0), W(-fw, fy1, fz0), W(-fw, fy1, fz1), W(-fw, fy0, fz1), felt * 0.85f, W(-1, 0, 0) - W(0, 0, 0));
        m.quad(W(fw, fy1, fz0), W(fw, fy0, fz0), W(fw, fy0, fz1), W(fw, fy1, fz1), felt * 0.85f, W(1, 0, 0) - W(0, 0, 0));
        m.quad(W(fw, fy1, fz0), W(-fw, fy1, fz0), W(-fw, fy1, fz1), W(fw, fy1, fz1), felt * 0.92f, W(0, 1, 0) - W(0, 0, 0));
        m.quad(W(-fw, fy0, fz0), W(fw, fy0, fz0), W(fw, fy0, fz1), W(-fw, fy0, fz1), felt * 0.92f, W(0, -1, 0) - W(0, 0, 0));
        // a red cushion at the stern, and the forcola, the oarlock, on the right
        m.quad(W(-0.18f, -0.85f, 0.18f), W(0.18f, -0.85f, 0.18f), W(0.18f, -0.5f, 0.18f), W(-0.18f, -0.5f, 0.18f), c2, vec3(0, 0, 1));
        vec3 wood(0.5f, 0.36f, 0.22f);
        m.quad(W(0.24f, -0.95f, 0.25f), W(0.27f, -0.95f, 0.25f), W(0.27f, -0.95f, 0.62f), W(0.24f, -0.95f, 0.62f), wood, W(0, -1, 0) - W(0, 0, 0));
    }
}


// ------------------------------------------------------------------ finer things, close up
static mat4 rotX(float a) {
    mat4 R;
    float c = std::cos(a), s = std::sin(a);
    R.at(1, 1) = c; R.at(1, 2) = -s;
    R.at(2, 1) = s; R.at(2, 2) = c;
    return R;
}
// The same things as drawThing, at the same size and place, only rounder: turned jugs, cups with
// handles, candles in holders, round apples with stalks. False: no finer drawing of that shape.
static bool drawThingFine(MeshBuf &m, const ItemKind &k, const Thing &t, vec3 p, uint32_t seed) {
    const std::string &s = k.shape;
    vec3 c = k.color, c2 = k.color2;
    const float HALF = 1.5707963f;
    mat4 M = translate(p) * rotateZ(t.facing);
    int n = std::min(t.count, 3);
    auto at = [&](float x, float y) { return M * translate(vec3(x, y, 0)); };
    if (s == "loaf") {
        for (int i = 0; i < n; i++) {
            mat4 L = at(0.06f * i, -0.05f * i);
            meshEllipsoid(m, L, vec3(0, 0, 0.05f), vec3(0.13f, 0.09f, 0.07f), 10, 5, c * (1 - 0.05f * i), -0.25f, HALF);
            for (int q = -1; q <= 1; q++)   // the slashes in the crust
                meshEllipsoid(m, L, vec3(q * 0.05f, 0, 0.113f), vec3(0.008f, 0.06f, 0.008f), 4, 2, c * 0.72f);
        }
        return true;
    }
    if (s == "jug") {
        meshTube(m, M, 0.0f, 0.05f, 0.05f, 0.05f, 0.075f, 0.075f, 12, c);                  // the foot to the belly
        meshTube(m, M, 0.05f, 0.13f, 0.075f, 0.075f, 0.06f, 0.06f, 12, c, 0, 0, false, false);
        meshTube(m, M, 0.13f, 0.2f, 0.04f, 0.04f, 0.05f, 0.05f, 12, c * 1.04f, 0, 0, false, true);   // the neck and lip
        meshTube(m, M, 0.19f, 0.205f, 0.054f, 0.054f, 0.054f, 0.054f, 12, c * 0.9f, 0, 0, false, false);
        for (int q = 0; q < 5; q++) {   // the handle, a loop at the back
            float a = -0.3f + q * 0.65f;
            meshEllipsoid(m, M, vec3(-0.072f - 0.03f * std::sin(a * 1.2f), 0, 0.09f + 0.07f * q / 4.0f), vec3(0.012f, 0.012f, 0.016f), 5, 3, c * 0.95f);
        }
        return true;
    }
    if (s == "cup") {
        for (int i = 0; i < n; i++) {
            mat4 C = at(0.09f * i, 0.03f * i);
            meshTube(m, C, 0.0f, 0.07f, 0.028f, 0.028f, 0.038f, 0.038f, 10, c);
            meshTube(m, C, 0.055f, 0.07f, 0.039f, 0.039f, 0.039f, 0.039f, 10, c * 0.85f, 0, 0, false, false);
            meshEllipsoid(m, C, vec3(-0.046f, 0, 0.036f), vec3(0.012f, 0.006f, 0.02f), 6, 3, c * 0.95f);   // the handle
        }
        return true;
    }
    if (s == "plate") {
        for (int i = 0; i < n; i++) {
            float z = 0.012f * i;
            meshTube(m, M, z, z + 0.005f, 0.06f, 0.06f, 0.06f, 0.06f, 14, c * (1 - 0.04f * i));
            meshTube(m, M, z + 0.005f, z + 0.014f, 0.07f, 0.07f, 0.105f, 0.105f, 14, c * (1.02f - 0.04f * i), 0, 0, false, false);   // the rim
        }
        return true;
    }
    if (s == "candle") {
        for (int i = 0; i < n; i++) {
            mat4 C = at(0.07f * i, 0);
            vec3 brass(0.62f, 0.5f, 0.28f);
            meshTube(m, C, 0.0f, 0.012f, 0.05f, 0.05f, 0.046f, 0.046f, 12, brass);              // the dish
            meshTube(m, C, 0.012f, 0.035f, 0.014f, 0.014f, 0.022f, 0.022f, 8, brass * 0.9f);    // the socket
            meshTube(m, C, 0.035f, 0.17f, 0.019f, 0.019f, 0.018f, 0.018f, 8, c);                 // the candle
            meshEllipsoid(m, C, vec3(0.05f, 0, 0.012f), vec3(0.018f, 0.007f, 0.006f), 5, 2, brass);   // the thumb ring
            float f = m.flags;
            m.flags = 1;
            meshEllipsoid(m, C, vec3(0, 0, 0.19f), vec3(0.011f, 0.011f, 0.024f), 6, 4, vec3(1.0f, 0.78f, 0.32f));   // the flame
            m.flags = f;
        }
        return true;
    }
    if (s == "bottle") {
        for (int i = 0; i < n; i++) {
            mat4 B = at(0.08f * i, 0);
            meshTube(m, B, 0.0f, 0.1f, 0.042f, 0.042f, 0.044f, 0.044f, 12, c);
            meshEllipsoid(m, B, vec3(0, 0, 0.1f), vec3(0.044f, 0.044f, 0.045f), 12, 3, c, 0.0f, 1.2f);   // the shoulders
            meshTube(m, B, 0.14f, 0.235f, 0.016f, 0.016f, 0.014f, 0.014f, 8, c * 1.1f);
            meshTube(m, B, 0.225f, 0.25f, 0.017f, 0.017f, 0.017f, 0.017f, 8, vec3(0.62f, 0.5f, 0.36f));   // the cork
        }
        return true;
    }
    if (s == "apples" || s == "onions") {
        static const float o[5][2] = {{0, 0}, {0.07f, 0.03f}, {-0.05f, 0.06f}, {0.03f, -0.06f}, {0.02f, 0.03f}};
        for (int i = 0; i < 5; i++) {
            float z = 0.035f + (i == 4 ? 0.05f : 0);
            vec3 col = c * (0.9f + 0.05f * (i % 3));
            if (s == "apples") {
                meshEllipsoid(m, at(o[i][0], o[i][1]), vec3(0, 0, z), vec3(0.034f, 0.034f, 0.031f), 8, 5, col);
                meshTube(m, at(o[i][0], o[i][1]), z + 0.025f, z + 0.045f, 0.003f, 0.003f, 0.003f, 0.003f, 4, vec3(0.3f, 0.2f, 0.1f));
            } else {
                meshEllipsoid(m, at(o[i][0], o[i][1]), vec3(0, 0, z), vec3(0.036f, 0.036f, 0.034f), 8, 5, col);
                meshTube(m, at(o[i][0], o[i][1]), z + 0.025f, z + 0.06f, 0.012f, 0.012f, 0.002f, 0.002f, 5, col * 0.85f);
            }
        }
        return true;
    }
    if (s == "cabbage") {
        meshEllipsoid(m, M, vec3(0, 0, 0.08f), vec3(0.075f, 0.075f, 0.07f), 10, 6, c * 1.05f);
        for (int q = 0; q < 6; q++) {   // the outer leaves, curling back
            float a = q * 1.047f + 0.3f;
            meshEllipsoid(m, M * rotateZ(a) * translate(vec3(0.07f, 0, 0.07f)) * rotX(0), vec3(0, 0, 0), vec3(0.045f, 0.06f, 0.055f), 6, 4, c * (0.85f + 0.04f * (q % 3)), -0.6f, 1.2f, -1.5f, 1.5f);
        }
        return true;
    }
    if (s == "cheese") {
        meshTube(m, M, 0.0f, 0.07f, 0.1f, 0.1f, 0.098f, 0.098f, 16, c);
        meshTube(m, M, 0.06f, 0.072f, 0.099f, 0.099f, 0.094f, 0.094f, 16, c * 0.88f, 0, 0, false, true);   // the rind at the top edge
        return true;
    }
    if (s == "fish") {
        for (int i = 0; i < n; i++) {
            mat4 F = at(0, 0.06f * i);
            meshEllipsoid(m, F, vec3(0, 0, 0.03f), vec3(0.1f, 0.032f, 0.028f), 10, 5, c);
            meshTube(m, F * translate(vec3(-0.095f, 0, 0.03f)) * rotateZ(HALF) * rotX(HALF), 0.0f, 0.06f, 0.004f, 0.004f, 0.03f, 0.006f, 4, c * 0.9f);   // the tail
            meshEllipsoid(m, F, vec3(0.07f, 0.024f, 0.04f), vec3(0.007f, 0.004f, 0.007f), 4, 2, vec3(0.08f, 0.08f, 0.08f));   // an eye
        }
        return true;
    }
    if (s == "purse") {
        meshEllipsoid(m, M, vec3(0, 0, 0.05f), vec3(0.062f, 0.058f, 0.052f), 10, 6, c);
        meshTube(m, M, 0.09f, 0.115f, 0.02f, 0.02f, 0.03f, 0.03f, 8, c * 0.85f);   // gathered at the neck
        meshTube(m, M, 0.088f, 0.096f, 0.024f, 0.024f, 0.024f, 0.024f, 8, c2.x > 0 ? c2 : c * 0.5f, 0, 0, false, false);   // the drawstring
        return true;
    }
    if (s == "pot") {
        meshEllipsoid(m, M, vec3(0, 0, 0.09f), vec3(0.11f, 0.11f, 0.07f), 14, 5, c, -1.2f, 0.5f);
        meshTube(m, M, 0.12f, 0.16f, 0.095f, 0.095f, 0.1f, 0.1f, 14, c * 0.95f, 0, 0, false, false);
        for (int i = 0; i < 3; i++) {   // the feet
            float a = i * 2.094f;
            meshTube(m, M, 0.0f, 0.04f, 0.012f, 0.012f, 0.016f, 0.016f, 5, c * 0.8f, 0.07f * std::cos(a), 0.07f * std::sin(a));
        }
        for (int sd = -1; sd <= 1; sd += 2) meshEllipsoid(m, M, vec3(sd * 0.115f, 0, 0.14f), vec3(0.015f, 0.008f, 0.02f), 5, 3, c * 0.85f);   // ears
        return true;
    }
    if (s == "inkpot") {
        meshTube(m, M, 0.0f, 0.03f, 0.03f, 0.03f, 0.034f, 0.034f, 10, c);
        meshEllipsoid(m, M, vec3(0, 0, 0.03f), vec3(0.034f, 0.034f, 0.016f), 10, 3, c, 0.0f, HALF);
        meshTube(m, M, 0.04f, 0.05f, 0.012f, 0.012f, 0.014f, 0.014f, 8, c * 0.8f);
        return true;
    }
    if (s == "quill") {
        meshTube(m, M * translate(vec3(-0.1f, 0, 0.008f)) * rotateZ(-HALF) * rotX(HALF), 0.0f, 0.2f, 0.003f, 0.003f, 0.004f, 0.004f, 5, c * 0.9f);   // the shaft
        meshEllipsoid(m, M, vec3(0.03f, 0.0f, 0.009f), vec3(0.075f, 0.022f, 0.003f), 8, 2, c);   // the vane
        return true;
    }
    if (s == "firewood") {
        for (int i = 0; i < 4; i++) {
            float y = -0.09f + 0.06f * (i % 3), zz = (i == 3 ? 0.06f : 0.0f) + 0.03f;
            mat4 Lg = M * translate(vec3(-0.2f, y, zz)) * rotateZ(-HALF) * rotX(HALF);
            vec3 bark = c * (0.9f + 0.06f * (i % 2));
            meshTube(m, Lg, 0.0f, 0.4f, 0.028f, 0.028f, 0.03f, 0.03f, 7, bark, 0, 0, true, true);
        }
        return true;
    }
    (void)c2; (void)seed;
    return false;
}

static void drawThing(MeshBuf &m, const ItemKind &k, const Thing &t, vec3 p, uint32_t seed) {
    const std::string &s = k.shape;
    vec3 c = k.color, c2 = k.color2;
    float r = t.facing, cr = std::cos(r), sr = std::sin(r);
    auto rot = [&](float x, float y) { return vec3(p.x + x * cr - y * sr, p.y + x * sr + y * cr, 0); };
    auto rbox = [&](float x0, float y0, float x1, float y1, float z0, float z1, vec3 col) {   // a box turned by the facing
        vec3 a = rot(x0, y0), b = rot(x1, y0), cc = rot(x1, y1), d = rot(x0, y1);
        vec3 A(a.x, a.y, p.z + z0), B(b.x, b.y, p.z + z0), C(cc.x, cc.y, p.z + z0), D(d.x, d.y, p.z + z0);
        vec3 A2 = A + vec3(0, 0, z1 - z0), B2 = B + vec3(0, 0, z1 - z0), C2 = C + vec3(0, 0, z1 - z0), D2 = D + vec3(0, 0, z1 - z0);
        vec3 mid((A.x + C.x) * 0.5f, (A.y + C.y) * 0.5f, 0);
        auto out = [&](vec3 u, vec3 v) { vec3 o = (u + v) * 0.5f - mid; o.z = 0; return o; };
        m.quad(A2, B2, C2, D2, col, vec3(0, 0, 1));
        m.quad(A, B, B2, A2, col * 0.92f, out(A, B));
        m.quad(B, C, C2, B2, col * 0.85f, out(B, C));
        m.quad(C, D, D2, C2, col * 0.92f, out(C, D));
        m.quad(D, A, A2, D2, col * 0.85f, out(D, A));
    };
    int n = std::min(t.count, 3);
    if (s == "none") return;
    if (k.boat) { drawBoat(m, k, p, t.facing); return; }
    if (fineFigures() && drawThingFine(m, k, t, p, seed)) return;
    if (s == "loaf") {
        for (int i = 0; i < n; i++) m.blob(vec3(p.x + 0.06f * i, p.y - 0.05f * i, p.z + 0.06f), vec3(0.13f, 0.09f, 0.065f), 6, c * (1 - 0.05f * i), seed + i, 0.1f);
    } else if (s == "jug") {
        m.prism(p.x, p.y, p.z, p.z + 0.15f, 0.07f, 8, c);
        m.prism(p.x, p.y, p.z + 0.15f, p.z + 0.22f, 0.045f, 8, c * 1.05f);
    } else if (s == "cup") {
        for (int i = 0; i < n; i++) m.prism(p.x + 0.09f * i, p.y + 0.03f * i, p.z, p.z + 0.07f, 0.035f, 7, c);
    } else if (s == "plate") {
        for (int i = 0; i < n; i++) m.prism(p.x, p.y, p.z + 0.012f * i, p.z + 0.012f * (i + 1), 0.1f, 10, c * (1 - 0.04f * i));
    } else if (s == "candle") {
        for (int i = 0; i < n; i++) {
            float x = p.x + 0.07f * i, y = p.y;
            m.prism(x, y, p.z, p.z + 0.03f, 0.04f, 6, vec3(0.55f, 0.5f, 0.42f));
            m.prism(x, y, p.z + 0.03f, p.z + 0.17f, 0.022f, 6, c);
            float f = m.flags;
            m.flags = 1;
            m.box(vec3(x - 0.012f, y - 0.012f, p.z + 0.17f), vec3(x + 0.012f, y + 0.012f, p.z + 0.21f), vec3(1.0f, 0.75f, 0.3f));
            m.flags = f;
        }
    } else if (s == "book") {
        for (int i = 0; i < n; i++) {   // heights in rbox are above the surface it lies on
            float z = 0.045f * i;
            rbox(-0.09f, -0.065f, 0.09f, 0.065f, z, z + 0.045f, c);
            rbox(-0.08f, -0.06f, 0.095f, 0.06f, z + 0.008f, z + 0.037f, c2);
        }
    } else if (s == "sword") {    // a sword lying flat: blade, guard, grip
        rbox(-0.45f, -0.012f, 0.18f, 0.012f, 0, 0.012f, c);
        rbox(0.18f, -0.07f, 0.2f, 0.07f, 0, 0.02f, c);
        rbox(0.2f, -0.016f, 0.32f, 0.016f, 0, 0.025f, c2);
    } else if (s == "cuirass") { // a breastplate, lying on its back
        rbox(-0.2f, -0.16f, 0.2f, 0.16f, 0, 0.1f, c);
        rbox(-0.14f, -0.12f, 0.14f, 0.12f, 0.1f, 0.15f, c * 1.1f);
    } else if (s == "buffcoat") { // a thick leather coat, folded
        rbox(-0.22f, -0.16f, 0.22f, 0.16f, 0, 0.08f, c);
        rbox(-0.18f, -0.12f, 0.18f, 0.12f, 0.08f, 0.11f, c2);
    } else if (s == "knife") {
        rbox(-0.12f, -0.012f, 0.04f, 0.012f, 0, 0.012f, c);
        rbox(0.04f, -0.018f, 0.12f, 0.018f, 0, 0.025f, c2);
    } else if (s == "apples" || s == "onions") {
        static const float o[5][2] = {{0, 0}, {0.07f, 0.03f}, {-0.05f, 0.06f}, {0.03f, -0.06f}, {0.02f, 0.03f}};
        for (int i = 0; i < 5; i++) {
            vec3 q = rot(o[i][0], o[i][1]);
            float z = p.z + 0.035f + (i == 4 ? 0.05f : 0);
            if (s == "apples") m.box(vec3(q.x - 0.03f, q.y - 0.03f, z - 0.03f), vec3(q.x + 0.03f, q.y + 0.03f, z + 0.03f), c * (0.9f + 0.05f * (i % 3)));
            else m.blob(vec3(q.x, q.y, z), vec3(0.035f, 0.035f, 0.04f), 5, c * (0.9f + 0.05f * (i % 3)), seed + i, 0.15f);
        }
    } else if (s == "ball" || s == "shot") {   // a cannonball; musket balls, a few together
        float r = s == "ball" ? 0.075f : 0.022f;
        int k = s == "ball" ? n : 5;
        static const float o[5][2] = {{0, 0}, {0.05f, 0.02f}, {-0.04f, 0.04f}, {0.02f, -0.05f}, {-0.03f, -0.02f}};
        for (int i = 0; i < k; i++) {
            vec3 q = s == "ball" ? rot(0.17f * i, 0) : rot(o[i][0], o[i][1]);
            m.blob(vec3(q.x, q.y, p.z + r), vec3(r, r, r), 7, c * (0.92f + 0.04f * (i % 3)), seed + i, 0.0f);
        }
    } else if (s == "cabbage") {
        m.blob(vec3(p.x, p.y, p.z + 0.08f), vec3(0.09f, 0.09f, 0.08f), 6, c, seed, 0.25f);
    } else if (s == "cheese") {
        m.prism(p.x, p.y, p.z, p.z + 0.07f, 0.1f, 10, c);
    } else if (s == "fish") {
        for (int i = 0; i < n; i++) {
            vec3 q = rot(0, 0.06f * i);
            m.blob(vec3(q.x, q.y, p.z + 0.03f), vec3(0.11f, 0.035f, 0.03f), 6, c, seed + i, 0.05f);
        }
    } else if (s == "horseshoe") {
        for (int i = 0; i < n; i++) {
            float z = 0.015f * i;
            rbox(-0.06f, -0.06f, -0.035f, 0.05f, z, z + 0.015f, c);
            rbox(0.035f, -0.06f, 0.06f, 0.05f, z, z + 0.015f, c);
            rbox(-0.06f, 0.05f, 0.06f, 0.075f, z, z + 0.015f, c);
        }
    } else if (s == "nails") {
        for (int i = 0; i < 6; i++) {
            vec3 q = rot(0.05f * std::cos(i * 2.1f), 0.04f * std::sin(i * 1.7f));
            m.box(vec3(q.x - 0.01f, q.y - 0.03f, p.z), vec3(q.x + 0.01f, q.y + 0.03f, p.z + 0.012f), c);
        }
    } else if (s == "hammer") {
        rbox(-0.14f, -0.015f, 0.08f, 0.015f, 0, 0.03f, c2);
        rbox(0.08f, -0.05f, 0.13f, 0.05f, 0, 0.05f, c);
    } else if (s == "tongs") {
        rbox(-0.16f, -0.03f, 0.12f, -0.01f, 0, 0.015f, c);
        rbox(-0.16f, 0.01f, 0.12f, 0.03f, 0, 0.015f, c);
    } else if (s == "coil") {
        for (int i = 0; i < 8; i++) {
            float a = i * 0.785f;
            vec3 q = rot(0.045f * std::cos(a), 0.045f * std::sin(a));
            m.box(vec3(q.x - 0.015f, q.y - 0.015f, p.z), vec3(q.x + 0.015f, q.y + 0.015f, p.z + 0.02f), c);
        }
    } else if (s == "purse") {
        m.blob(vec3(p.x, p.y, p.z + 0.05f), vec3(0.06f, 0.06f, 0.05f), 6, c, seed, 0.15f);
        m.prism(p.x, p.y, p.z + 0.09f, p.z + 0.12f, 0.025f, 6, c * 0.8f);
    } else if (s == "bottle") {
        for (int i = 0; i < n; i++) {
            float x = p.x + 0.08f * i;
            m.prism(x, p.y, p.z, p.z + 0.16f, 0.04f, 8, c);
            m.prism(x, p.y, p.z + 0.16f, p.z + 0.25f, 0.015f, 6, c * 1.1f);
        }
    } else if (s == "firewood") {
        for (int i = 0; i < 4; i++) {
            float y = -0.09f + 0.06f * (i % 3), z = i == 3 ? 0.06f : 0.0f;
            rbox(-0.2f, y - 0.03f, 0.2f, y + 0.03f, z, z + 0.06f, c * (0.9f + 0.06f * (i % 2)));
        }
    } else if (s == "pot") {
        m.prism(p.x, p.y, p.z + 0.03f, p.z + 0.15f, 0.11f, 10, c);
        for (int i = 0; i < 3; i++) {
            vec3 q = rot(0.07f * std::cos(i * 2.094f), 0.07f * std::sin(i * 2.094f));
            m.box(vec3(q.x - 0.012f, q.y - 0.012f, p.z), vec3(q.x + 0.012f, q.y + 0.012f, p.z + 0.03f), c);
        }
    } else if (s == "inkpot") {
        m.prism(p.x, p.y, p.z, p.z + 0.04f, 0.03f, 8, c);
    } else if (s == "quill") {
        rbox(-0.1f, -0.006f, 0.1f, 0.006f, 0.005f, 0.012f, c);
        rbox(-0.02f, -0.02f, 0.1f, 0.02f, 0.006f, 0.01f, c * 0.95f);
    } else if (s == "window") {
        // a great window of painted glass in the wall at side t.side (art "NAME" behind altar: the
        // east window), kept below the ground-floor walls' height (GF_H), which is all of a wall
        // that is drawn while you are inside: a stone frame, five lights in two tiers under a stepped head,
        // panes of coloured glass in lead, and a pale cross in the middle light of the upper tier.
        // c is the stone, c2 the glass that is commonest.
        int sd = t.side < 0 ? 1 : t.side;
        bool alongY = sd < 2;
        float wall = sd == 0 ? (float)t.x : sd == 1 ? (float)(t.x + 1) : sd == 2 ? (float)t.y : (float)(t.y + 1);
        float in = (sd == 0 || sd == 2) ? 1.0f : -1.0f;   // into the room
        float mid = alongY ? p.y : p.x;                     // the middle of the window, along the wall
        auto pane = [&](float a0, float a1, float d0, float d1, float z0, float z1, vec3 col) {
            vec3 lo, hi;
            if (alongY) { lo = vec3(wall + in * d0, mid + a0, z0); hi = vec3(wall + in * d1, mid + a1, z1); }
            else { lo = vec3(mid + a0, wall + in * d0, z0); hi = vec3(mid + a1, wall + in * d1, z1); }
            m.box(vec3(std::min(lo.x, hi.x), std::min(lo.y, hi.y), z0), vec3(std::max(lo.x, hi.x), std::max(lo.y, hi.y), z1), col, true);
        };
        const float half = 0.78f, z0 = p.z + 0.7f, z1 = p.z + 2.12f, zt = z0 + (z1 - z0) * 0.46f;
        const int lights = 5;
        const float mw = 0.045f, lw = (2 * half - (lights + 1) * mw) / lights;
        auto topOf = [&](int i) { int o = std::abs(i - lights / 2); return z1 - (o == 0 ? 0.0f : o == 1 ? 0.07f : 0.18f); };
        pane(-half - 0.06f, half + 0.06f, 0.0f, 0.02f, z0 - 0.06f, z1 + 0.04f, vec3(0.08f, 0.08f, 0.1f));   // the lead behind it all
        const vec3 glass[] = {c2, c2, vec3(0.72f, 0.1f, 0.12f), vec3(0.72f, 0.1f, 0.12f), vec3(0.13f, 0.46f, 0.24f),
                              vec3(0.88f, 0.68f, 0.16f), vec3(0.9f, 0.88f, 0.78f), vec3(0.45f, 0.16f, 0.5f), vec3(0.88f, 0.64f, 0.5f)};
        const int nglass = sizeof(glass) / sizeof(glass[0]);
        for (int i = 0; i < lights; i++) {
            float a0 = -half + mw + i * (lw + mw), a1 = a0 + lw;
            for (int tier = 0; tier < 2; tier++) {
                float b0 = tier ? zt + 0.025f : z0 + 0.02f, b1 = tier ? topOf(i) - 0.02f : zt - 0.025f;
                int rows = std::max(2, (int)((b1 - b0) / 0.13f));
                float rh = (b1 - b0) / rows;
                for (int rr = 0; rr < rows; rr++)
                    for (int cc = 0; cc < 2; cc++) {
                        float pa0 = a0 + cc * lw * 0.5f + 0.008f, pa1 = a0 + (cc + 1) * lw * 0.5f - 0.008f;
                        float pz0 = b0 + rr * rh + 0.008f, pz1 = b0 + (rr + 1) * rh - 0.008f;
                        vec3 col = glass[(int)(hash01(i * 17 + cc * 5 + tier * 131, rr, seed + 41) * nglass) % nglass];
                        // the cross: down the middle light of the upper tier, and across it near the top
                        if (tier == 1 && i == lights / 2 && rr >= 1 && rr < rows - 1) col = vec3(0.93f, 0.9f, 0.8f);
                        if (tier == 1 && std::abs(i - lights / 2) <= 1 && rr == rows - 3) col = vec3(0.93f, 0.9f, 0.8f);
                        pane(pa0, pa1, 0.02f, 0.04f, pz0, pz1, col * (0.9f + 0.2f * hash01(i, rr * 3 + cc, seed + 7)));
                    }
            }
            // the mullion to its left, and the last one
            pane(a0 - mw, a0, 0.0f, 0.07f, z0, std::max(topOf(i), i > 0 ? topOf(i - 1) : topOf(i)), c);
            if (i == lights - 1) pane(a1, a1 + mw, 0.0f, 0.07f, z0, topOf(i), c);
            pane(a0 - mw, a1 + mw, 0.0f, 0.07f, topOf(i) - 0.01f, topOf(i) + 0.05f, c);   // the head over this light
        }
        pane(-half, half, 0.0f, 0.07f, zt - 0.025f, zt + 0.025f, c);       // the transom
        pane(-half - 0.06f, half + 0.06f, 0.0f, 0.09f, z0 - 0.06f, z0, c); // the sill
    } else if (s == "bell") {
        // a cow's bell on its strap: hung on a nail in the wall at side t.side, or lying on the floor
        if (t.side < 0) {
            m.prism(p.x, p.y, p.z, p.z + 0.1f, 0.06f, 6, c);
            m.prism(p.x, p.y, p.z + 0.1f, p.z + 0.14f, 0.04f, 6, c * 0.95f);
            rbox(-0.015f, -0.09f, 0.015f, 0.09f, 0.14f, 0.155f, c2);
        } else {
            int sd = t.side;
            vec2 o = sd == 0 ? vec2(t.x, t.y) : sd == 1 ? vec2(t.x + 1, t.y) : sd == 2 ? vec2(t.x, t.y) : vec2(t.x, t.y + 1);
            bool alongY = sd < 2;
            float in = (sd == 0 || sd == 2) ? 1.0f : -1.0f;   // into the room
            auto panel = [&](float a0, float a1, float d0, float d1, float zz0, float zz1, vec3 col) {
                vec3 lo, hi;
                if (alongY) { lo = vec3(o.x + in * d0, t.y + a0, zz0); hi = vec3(o.x + in * d1, t.y + a1, zz1); }
                else { lo = vec3(t.x + a0, o.y + in * d0, zz0); hi = vec3(t.x + a1, o.y + in * d1, zz1); }
                m.box(vec3(std::min(lo.x, hi.x), std::min(lo.y, hi.y), zz0), vec3(std::max(lo.x, hi.x), std::max(lo.y, hi.y), zz1), col, true);
            };
            panel(0.485f, 0.515f, 0.0f, 0.04f, p.z + 1.42f, p.z + 1.46f, vec3(0.25f, 0.25f, 0.27f));   // the nail
            panel(0.47f, 0.53f, 0.02f, 0.04f, p.z + 1.18f, p.z + 1.44f, c2);                         // the strap
            panel(0.44f, 0.56f, 0.035f, 0.135f, p.z + 1.08f, p.z + 1.2f, c * 0.95f);                 // the body
            panel(0.42f, 0.58f, 0.03f, 0.15f, p.z + 1.0f, p.z + 1.08f, c);                           // the mouth
        }
    } else if (s == "painting") {
        // hung on the wall at side t.side, in a gilt frame; the canvas in a few broad patches
        int sd = t.side < 0 ? 3 : t.side;
        vec2 o = sd == 0 ? vec2(t.x, t.y) : sd == 1 ? vec2(t.x + 1, t.y) : sd == 2 ? vec2(t.x, t.y) : vec2(t.x, t.y + 1);
        bool alongY = sd < 2;
        float in = (sd == 0 || sd == 2) ? 1.0f : -1.0f;   // into the room
        float z0 = p.z + 0.35f, z1 = p.z + 2.1f;
        auto panel = [&](float a0, float a1, float d0, float d1, float zz0, float zz1, vec3 col) {
            vec3 lo, hi;
            if (alongY) { lo = vec3(o.x + in * d0, t.y + a0, zz0); hi = vec3(o.x + in * d1, t.y + a1, zz1); }
            else { lo = vec3(t.x + a0, o.y + in * d0, zz0); hi = vec3(t.x + a1, o.y + in * d1, zz1); }
            m.box(vec3(std::min(lo.x, hi.x), std::min(lo.y, hi.y), zz0), vec3(std::max(lo.x, hi.x), std::max(lo.y, hi.y), zz1), col, true);
        };
        panel(-0.35f, 1.35f, 0.0f, 0.06f, z0, z1, c);
        panel(-0.27f, 1.27f, 0.06f, 0.075f, z0 + 0.08f, z1 - 0.08f, c2 * 0.8f);
        for (int i = 0; i < 5; i++) {   // patches of the picture
            float a = -0.22f + 0.29f * i, h0 = z0 + 0.15f + 0.3f * hash01(i, (int)seed, 3), h1 = h0 + 0.35f + 0.4f * hash01(i, (int)seed, 4);
            vec3 col = i % 2 ? c2 : vec3(0.85f, 0.7f, 0.55f) * (0.8f + 0.3f * hash01(i, (int)seed, 5));
            panel(a, a + 0.22f, 0.075f, 0.085f, std::min(h0, z1 - 0.2f), std::min(h1, z1 - 0.1f), col);
        }
    } else if (s == "tomb" || s == "canopy") {
        vec3 st = c;
        rbox(-0.45f, -0.3f, 0.45f, 0.3f, 0, 0.5f, st * 0.9f);
        rbox(-0.48f, -0.33f, 0.48f, 0.33f, 0.5f, 0.56f, st);
        FigureLook fl;
        fl.coat = c; fl.legs = c * 0.95f; fl.hat = c; fl.skin = c * 1.02f; fl.hair = c * 0.95f; fl.apron = c;
        fl.hatKind = 0; fl.robe = true; fl.statue = true;
        buildFigure(m, vec3(p.x, p.y, p.z + 0.23f), r + 1.5708f, 0, fl, true);   // the effigy on the lid
        if (s == "canopy") {
            for (int i = 0; i < 4; i++) {
                float x = (i & 1) ? 0.45f : -0.45f, y = (i & 2) ? 0.3f : -0.3f;
                vec3 q = rot(x, y);
                m.prism(q.x, q.y, p.z + 0.56f, p.z + 1.7f, 0.05f, 6, st);
            }
            rbox(-0.52f, -0.36f, 0.52f, 0.36f, 1.7f, 1.82f, st);
            // the living king and queen kneel on top
            fl.robe = true;
            for (int i = 0; i < 2; i++) {
                vec3 q = rot(i ? 0.2f : -0.2f, 0);
                buildFigure(m, vec3(q.x, q.y, p.z + 1.82f), r, 0, fl, false);
            }
        }
    } else if (s == "statue") {
        m.prism(p.x, p.y, p.z, p.z + 0.5f, 0.3f, 8, c * 0.88f);
        FigureLook fl;
        fl.coat = c; fl.legs = c * 0.96f; fl.hat = c; fl.skin = c * 1.02f; fl.hair = c * 0.96f; fl.apron = c;
        fl.hatKind = 0; fl.dress = true; fl.statue = true;
        buildFigure(m, vec3(p.x, p.y, p.z + 0.5f), r, 0, fl, false);
    } else if (s == "giant" || s == "group" || s == "pieta") {
        // larger than life: a standing figure (on a pedestal, 'y' in the maps), a group of
        // figures twisting upwards, or a mother with her dead son across her knees
        FigureLook fl;
        fl.coat = c; fl.legs = c * 0.97f; fl.hat = c; fl.skin = c; fl.hair = c * 0.95f; fl.apron = c;
        fl.hatKind = 0; fl.statue = true;
        size_t v0 = m.v.size();
        float S = s == "giant" ? 1.7f : (s == "group" ? 1.4f : 1.25f);
        if (s == "giant") {
            buildFigure(m, p, r, 0, fl, false);
        } else if (s == "group") {
            rbox(-0.32f, -0.26f, 0.32f, 0.26f, 0, 0.16f, c * 0.86f);
            buildFigure(m, vec3(p.x, p.y, p.z + 0.16f), r, 0.12f, fl, false);              // the one standing
            vec3 q = rot(0.05f, -0.2f);
            buildFigure(m, vec3(q.x, q.y, p.z - 0.26f), r + 2.6f, 0, fl, true);          // crouching at his feet
            fl.dress = true;
            q = rot(0.06f, 0.02f);
            buildFigure(m, vec3(q.x, q.y, p.z + 0.86f), r + 0.9f, 0.3f, fl, false);      // held up, twisting away
        } else {
            rbox(-0.36f, -0.24f, 0.36f, 0.24f, 0, 0.22f, c * 0.88f);
            fl.robe = true;
            vec3 q = rot(0, -0.08f);
            buildFigure(m, vec3(q.x, q.y, p.z + 0.22f), r, 0, fl, false);                  // the mother
            fl.robe = false;
            q = rot(-0.32f, 0.12f);
            buildFigure(m, vec3(q.x, q.y, p.z + 0.02f), r - 1.5708f, 0, fl, true);       // across her knees
        }
        for (size_t k = v0; k < m.v.size(); k++) {
            Vertex &v = m.v[k];
            v.px = p.x + (v.px - p.x) * S;
            v.py = p.y + (v.py - p.y) * S;
            v.pz = p.z + (v.pz - p.z) * S;
        }
    } else if (s == "baldacchino") {
        // Bernini's canopy over the high altar of St Peter's: four twisted bronze columns,
        // a fringed canopy, and volutes rising to the orb and cross; gilt where c2
        float hx = 1.15f, hy = 1.25f, H = 4.4f;
        for (int i = 0; i < 4; i++) {
            vec3 q = rot((i & 1) ? hx : -hx, (i & 2) ? hy : -hy);
            rbox(((i & 1) ? hx : -hx) - 0.24f, ((i & 2) ? hy : -hy) - 0.24f, ((i & 1) ? hx : -hx) + 0.24f, ((i & 2) ? hy : -hy) + 0.24f, 0, 0.55f, vec3(0.86f, 0.82f, 0.74f));
            const int N = 16;
            for (int k = 0; k < N; k++) {   // the spiral: short drums wandering round the axis
                float a = k * 1.1f + i, z0 = 0.55f + (H - 0.55f) * k / N, z1 = 0.55f + (H - 0.55f) * (k + 1) / N;
                m.prism(q.x + 0.06f * std::cos(a), q.y + 0.06f * std::sin(a), p.z + z0, p.z + z1, 0.13f, 8, (k % 4 == 3) ? c2 : c);
            }
            m.prism(q.x, q.y, p.z + H, p.z + H + 0.25f, 0.2f, 8, c2);   // capital
        }
        rbox(-hx - 0.25f, -hy - 0.25f, hx + 0.25f, hy + 0.25f, H + 0.25f, H + 0.6f, c);
        rbox(-hx - 0.3f, -hy - 0.3f, hx + 0.3f, hy + 0.3f, H + 0.6f, H + 0.7f, c2);
        for (int i = 0; i < 4; i++) {   // the volutes, leaning in to the top
            vec3 q0 = rot((i & 1) ? hx : -hx, (i & 2) ? hy : -hy), q1 = rot((i & 1) ? 0.3f : -0.3f, (i & 2) ? 0.3f : -0.3f);
            for (int k = 0; k < 5; k++) {
                float f = k / 5.0f, z = H + 0.7f + 1.2f * std::sin(f * 1.5708f);
                vec3 q = q0 + (q1 - q0) * f;
                m.prism(q.x, q.y, p.z + z, p.z + z + 0.28f, 0.09f, 6, c);
            }
        }
        m.blob(vec3(p.x, p.y, p.z + H + 2.15f), vec3(0.22f, 0.22f, 0.22f), 6, c2, seed, 0.0f);
        m.prism(p.x, p.y, p.z + H + 2.3f, p.z + H + 2.9f, 0.04f, 4, c2);
        rbox(-0.03f, -0.2f, 0.03f, 0.2f, H + 2.65f, H + 2.71f, c2);
    } else if (s == "pulpit") {   // the Pisani's pulpits: a carved drum on columns, with a stair
        for (int i = 0; i < 6; i++) {
            float a = 2 * PI_F * i / 6;
            vec3 q = rot(0.55f * std::cos(a), 0.55f * std::sin(a));
            m.prism(q.x, q.y, p.z, p.z + 1.1f, 0.06f, 6, c * 0.92f);
        }
        m.prism(p.x, p.y, p.z, p.z + 1.1f, 0.12f, 6, c * 0.85f);
        m.prism(p.x, p.y, p.z + 1.1f, p.z + 1.75f, 0.7f, 6, c);
        m.prism(p.x, p.y, p.z + 1.75f, p.z + 1.85f, 0.74f, 6, c2);
        for (int k = 0; k < 4; k++) rbox(0.7f + 0.22f * k, -0.2f, 0.92f + 0.22f * k, 0.2f, 0, 1.1f - 0.27f * k, c * 0.9f);
    } else if (s == "lamp") {     // a bronze lamp hanging from the vault on a long chain
        m.prism(p.x, p.y, p.z + 1.6f, p.z + 4.5f, 0.015f, 4, c * 0.7f);
        m.prism(p.x, p.y, p.z + 1.45f, p.z + 1.6f, 0.32f, 10, c);
        m.prism(p.x, p.y, p.z + 1.3f, p.z + 1.45f, 0.18f, 8, c * 0.9f);
        float f = m.flags;
        m.flags = 1;
        for (int i = 0; i < 6; i++) {
            float a = 2 * PI_F * i / 6;
            m.box(vec3(p.x + 0.3f * std::cos(a) - 0.02f, p.y + 0.3f * std::sin(a) - 0.02f, p.z + 1.6f),
                  vec3(p.x + 0.3f * std::cos(a) + 0.02f, p.y + 0.3f * std::sin(a) + 0.02f, p.z + 1.68f), vec3(1.0f, 0.78f, 0.35f));
        }
        m.flags = f;
    } else if (s == "lectern") {  // Galileo's chair: a wooden stand on steps
        rbox(-0.35f, -0.3f, 0.35f, 0.3f, 0, 1.1f, c);
        rbox(-0.4f, -0.35f, 0.4f, 0.35f, 1.1f, 1.35f, c2);
        for (int k = 0; k < 4; k++) rbox(0.35f + 0.18f * k, -0.25f, 0.53f + 0.18f * k, 0.25f, 0, 1.0f - 0.25f * k, c * 0.9f);
    } else if (s == "bookdesk" || s == "openbook") {
        // a reading desk with a book open on it; a bookdesk's book is chained to a rod at the back
        // (the Bodleian), an openbook's is not (a book shown in a palace or a mosque)
        vec3 iron(0.22f, 0.22f, 0.24f);
        rbox(-0.32f, -0.22f, 0.32f, 0.22f, 0, 0.72f, c * 0.85f);                  // the desk's body
        rbox(-0.38f, -0.3f, 0.38f, 0.3f, 0.72f, 0.78f, c);                       // its top
        rbox(-0.27f, -0.19f, 0.27f, 0.19f, 0.78f, 0.81f, c * 0.45f);             // the book's boards
        rbox(-0.25f, -0.17f, -0.012f, 0.17f, 0.81f, 0.845f, c2);                 // its two open pages
        rbox(0.012f, -0.17f, 0.25f, 0.17f, 0.81f, 0.845f, c2);
        if (s == "bookdesk") {
            rbox(-0.38f, -0.36f, 0.38f, -0.3f, 0.6f, 1.05f, c * 0.9f);           // a board at the back
            rbox(-0.36f, -0.35f, 0.36f, -0.32f, 0.98f, 1.01f, iron);             // the rod the chains run on
            rbox(-0.015f, -0.33f, 0.015f, -0.19f, 0.8f, 0.83f, iron);            // the chain
            rbox(-0.015f, -0.345f, 0.015f, -0.32f, 0.8f, 0.99f, iron);
        }
    } else if (s == "block") {    // a block of stone (the Stone of Shame)
        rbox(-0.3f, -0.3f, 0.3f, 0.3f, 0, 0.45f, c);
    } else if (s == "urn") {      // a great marble jar on a low base (the urns of Pergamon in Ayasofya)
        m.prism(p.x, p.y, p.z, p.z + 0.15f, 0.42f, 10, c * 0.9f);
        m.prism(p.x, p.y, p.z + 0.15f, p.z + 0.35f, 0.3f, 12, c * 0.96f);
        m.prism(p.x, p.y, p.z + 0.35f, p.z + 0.8f, 0.52f, 14, c);
        m.prism(p.x, p.y, p.z + 0.8f, p.z + 1.1f, 0.58f, 14, c * 1.03f);
        m.prism(p.x, p.y, p.z + 1.1f, p.z + 1.3f, 0.48f, 14, c * 0.97f);
        m.prism(p.x, p.y, p.z + 1.3f, p.z + 1.42f, 0.56f, 14, c);
        m.prism(p.x, p.y, p.z + 1.42f, p.z + 1.43f, 0.4f, 14, c * 0.45f);   // the mouth
    } else if (s == "shrine") {   // a Gothic canopy of bronze over a silver coffin (St Sebaldus)
        rbox(-0.68f, -0.36f, 0.68f, 0.36f, 0, 0.22f, c * 0.9f);
        rbox(-0.5f, -0.22f, 0.5f, 0.22f, 0.22f, 0.55f, vec3(0.8f, 0.8f, 0.82f));
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 2; j++) {
                vec3 q = rot(-0.6f + 0.6f * i, j ? 0.3f : -0.3f);
                m.prism(q.x, q.y, p.z + 0.22f, p.z + 1.75f, 0.045f, 6, c);
            }
        rbox(-0.68f, -0.36f, 0.68f, 0.36f, 1.75f, 1.86f, c);
        for (int i = 0; i < 3; i++) {
            vec3 q = rot(-0.45f + 0.45f * i, 0);
            m.cone(q.x, q.y, p.z + 1.86f, p.z + (i == 1 ? 2.7f : 2.4f), 0.2f, 6, c * 1.05f);
        }
    } else if (s == "fountain") { // a basin on a ring of lions, with a jet (the Court of the Lions)
        for (int i = 0; i < 12; i++) {
            float a = i * 6.2832f / 12;
            vec3 q(p.x + 0.78f * std::cos(a), p.y + 0.78f * std::sin(a), 0);   // each lion faces outwards
            m.box(vec3(q.x - 0.11f, q.y - 0.11f, p.z), vec3(q.x + 0.11f, q.y + 0.11f, p.z + 0.36f), c2);
            m.prism(q.x + 0.12f * std::cos(a), q.y + 0.12f * std::sin(a), p.z + 0.26f, p.z + 0.5f, 0.09f, 6, c2 * 1.05f);
        }
        m.prism(p.x, p.y, p.z, p.z + 0.5f, 0.32f, 10, c * 0.92f);
        m.prism(p.x, p.y, p.z + 0.5f, p.z + 0.68f, 0.66f, 16, c);
        m.prism(p.x, p.y, p.z + 0.68f, p.z + 0.69f, 0.56f, 16, vec3(0.45f, 0.6f, 0.7f));   // the water
        m.prism(p.x, p.y, p.z + 0.68f, p.z + 0.9f, 0.12f, 8, c * 0.95f);
        m.prism(p.x, p.y, p.z + 0.9f, p.z + 0.96f, 0.3f, 10, c);
    } else if (s == "reliquary") {
        rbox(-0.25f, -0.12f, 0.25f, 0.12f, 0, 0.3f, c);
        rbox(-0.2f, -0.1f, 0.2f, 0.1f, 0.3f, 0.36f, c * 1.08f);
        float f = m.flags;
        m.flags = 1;
        rbox(-0.04f, -0.04f, 0.04f, 0.04f, 0.36f, 0.48f, vec3(0.95f, 0.8f, 0.4f));
        m.flags = f;
    } else {
        rbox(-0.06f, -0.06f, 0.06f, 0.06f, 0, 0.1f, c);
    }
}

void Items::buildMesh(World &w, MeshBuf &out, int x0, int y0, int x1, int y1, int z) const {
    for (size_t i = 0; i < things.size(); i++) {
        const Thing &t = things[i];
        if (t.gone || t.x < x0 || t.x > x1 || t.y < y0 || t.y > y1) continue;
        if (t.z != z && !(z > 0 && t.z == 0)) continue;
        float fz = w.floorAt(t.x + 0.5f, t.y + 0.5f, t.z);
        vec3 base(t.x + t.ox, t.y + t.oy, fz + t.h);
        if (kinds[t.kind].fixed) { drawThing(out, kinds[t.kind], t, base, t.key ? t.key : (uint32_t)i); continue; }
        // everyday things a third larger than life, to be seen and clicked (as in Ultima)
        size_t v0 = out.v.size();
        drawThing(out, kinds[t.kind], t, base, t.key ? t.key : (uint32_t)i);
        const float S = 1.35f;
        for (size_t k = v0; k < out.v.size(); k++) {
            Vertex &v = out.v[k];
            v.px = base.x + (v.px - base.x) * S;
            v.py = base.y + (v.py - base.y) * S;
            v.pz = base.z + (v.pz - base.z) * S;
        }
    }
}

// ------------------------------------------------------------------ saving

// What the player changed: things taken from where the world put them ("gone KEY"),
// and things put down ("thing ID COUNT X Y Z").
void Items::save(std::ostream &out) const {
    for (const Thing &t : things) {
        if (t.key && t.gone) out << "gone " << t.key << "\n";
        else if (t.key && t.count != t.count0) out << "count " << t.key << " " << t.count << "\n";   // some taken from a stack
        if (!t.key && !t.gone) out << "thing " << kinds[t.kind].id << " " << t.count << " " << t.x << " " << t.y << " " << t.z << " " << t.h << " " << t.facing << "\n";
    }
}

void Items::loadSave(std::istream &in) {
    std::map<uint32_t, int> byKey;
    for (size_t i = 0; i < things.size(); i++)
        if (things[i].key) byKey[things[i].key] = (int)i;
    std::string line;
    struct Put { int kind, n, x, y, z; float h, f; bool hasF; };
    std::vector<Put> puts;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string w;
        ss >> w;
        if (w == "gone") {
            uint32_t k;
            ss >> k;
            auto it = byKey.find(k);
            if (it != byKey.end()) things[it->second].gone = true;
        } else if (w == "count") {   // a stack with fewer left in it than the world was made with
            uint32_t k;
            int n = 0;
            ss >> k >> n;
            auto it = byKey.find(k);
            if (it != byKey.end() && n > 0) things[it->second].count = n;
        } else if (w == "thing") {
            std::string id;
            Put p;
            p.h = 0; p.f = 0;
            ss >> id >> p.n >> p.x >> p.y >> p.z >> p.h;
            p.hasF = (bool)(ss >> p.f);
            p.kind = find(id);
            if (p.kind >= 0) puts.push_back(p);
        }
    }
    reindex();
    for (auto &p : puts) {
        Thing t;
        t.kind = p.kind; t.count = p.n; t.x = p.x; t.y = p.y; t.z = p.z; t.h = p.h;
        int k = (int)at(p.x, p.y, p.z).size();
        static const float off[6][2] = {{0.5f, 0.5f}, {0.28f, 0.6f}, {0.72f, 0.42f}, {0.4f, 0.3f}, {0.62f, 0.72f}, {0.3f, 0.78f}};
        t.ox = off[k % 6][0]; t.oy = off[k % 6][1];
        if (p.hasF) t.facing = p.f;
        if (p.kind >= 0 && kinds[p.kind].boat) t.ox = t.oy = 0.5f;
        add(t);
    }
    version++;
}

// ------------------------------------------------------------------ books
// data/books/ID.txt:
//   book ID
//   language LANGUAGE            (a skill: italian, latin ...; english needs none)
//   title AS PRINTED | IN ENGLISH
//   page [image FILE | CREDIT]   then the page as printed, a line "=", the page in English;
//                                a blank line starts a new paragraph, a line ending in // stands on
//                                its own (title pages, verse); {EFFECTS} lines happen
//                                when the page is read and understood
bool Items::loadBooks(const std::string &dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return false;
    std::vector<std::string> files;
    for (auto &e : fs::directory_iterator(dir, ec))
        if (e.path().extension() == ".txt") files.push_back(e.path().string());
    std::sort(files.begin(), files.end());
    std::string data = fs::path(dir).parent_path().string() + "/";
    for (auto &fn : files) {
        std::ifstream in(fn);
        Book b;
        BookPage *pg = nullptr;
        bool english = false;
        auto add = [&](std::string &to, const std::string &line) {
            if (line.empty()) {   // a paragraph: one blank line, however it was reached
                if (to.empty() || (to.size() >= 2 && to.compare(to.size() - 2, 2, "\n\n") == 0)) return;
                to += to.back() == '\n' ? "\n" : "\n\n";
                return;
            }
            if (!to.empty() && to.back() != '\n') to += " ";
            to += line;
        };
        std::string line;
        while (std::getline(in, line)) {
            std::string t = trim(line);
            if (!pg) {
                if (t.empty() || t[0] == '#') continue;
                std::istringstream ss(t);
                std::string w;
                ss >> w;
                std::string rest;
                std::getline(ss, rest);
                rest = trim(rest);
                if (w == "book") b.id = rest;
                else if (w == "language") b.language = rest;
                else if (w == "title") {
                    size_t bar = rest.find('|');
                    b.title = trim(rest.substr(0, bar));
                    b.titleEn = bar == std::string::npos ? b.title : trim(rest.substr(bar + 1));
                }
                else if (w != "page") std::fprintf(stderr, "%s: cannot read \"%s\"\n", fn.c_str(), t.c_str());
                if (w != "page") continue;
            }
            if (t.rfind("page", 0) == 0 && (t.size() == 4 || t[4] == ' ')) {
                b.pages.emplace_back();
                pg = &b.pages.back();
                english = false;
                std::string rest = trim(t.substr(4));
                if (rest.rfind("image", 0) == 0) {
                    rest = trim(rest.substr(5));
                    size_t bar = rest.find('|');
                    pg->image = data + trim(rest.substr(0, bar));
                    if (bar != std::string::npos) pg->credit = trim(rest.substr(bar + 1));
                }
                continue;
            }
            if (t[0] == '#' ) continue;
            if (t == "=") { english = true; continue; }
            if (!t.empty() && t[0] == '{') {
                for (size_t a = 0; (a = t.find('{', a)) != std::string::npos;) {
                    size_t e = t.find('}', a);
                    if (e == std::string::npos) break;
                    pg->effects.push_back(trim(t.substr(a + 1, e - a - 1)));
                    a = e;
                }
                continue;
            }
            bool br = t.size() >= 2 && t.compare(t.size() - 2, 2, "//") == 0;   // a line of its own (a title page, verse)
            if (br) t = trim(t.substr(0, t.size() - 2));
            std::string &to = english ? pg->english : pg->orig;
            add(to, t);
            if (br) to += "\n";
        }
        for (auto &p : b.pages) {
            while (p.orig.size() && p.orig.back() == '\n') p.orig.pop_back();
            while (p.english.size() && p.english.back() == '\n') p.english.pop_back();
            if (p.english.empty()) p.english = p.orig;   // a page in English, or a picture
        }
        if (b.id.empty()) { std::fprintf(stderr, "%s: no \"book\" line\n", fn.c_str()); continue; }
        books[b.id] = b;
    }
    for (auto &k : kinds)
        if (!k.book.empty() && !books.count(k.book)) std::fprintf(stderr, "items.txt: %s: no book %s in %s\n", k.id.c_str(), k.book.c_str(), dir.c_str());
    return true;
}
