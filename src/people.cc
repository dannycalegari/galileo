// people.cc: loading people, finding them homes and work, their days, and conversation.
#include "people.h"
#include "items.h"
#include "noise.h"
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <queue>
#include <set>
#include <sstream>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <cstring>

// ------------------------------------------------------------------ small helpers

static std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static std::string lower(std::string s) {
    for (auto &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static vec3 hexc(const std::string &s) {
    unsigned v = 0;
    std::sscanf(s.c_str() + (s[0] == '#' ? 1 : 0), "%x", &v);
    return vec3(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f);
}

static int parseTime(const std::string &s) {
    int h = 0, m = 0;
    std::sscanf(s.c_str(), "%d:%d", &h, &m);
    return h * 60 + m;
}

static bool truthy(const std::string &v) { return !v.empty() && v != "0"; }

static double num(const std::string &v) { return v.empty() ? 0.0 : std::atof(v.c_str()); }

static void parseLook(Costume &c, std::istringstream &ss) {
    std::string k;
    while (ss >> k) {
        std::string v;
        if (k == "coat") { ss >> v; c.coat = hexc(v); }
        else if (k == "legs") { ss >> v; c.legs = hexc(v); }
        else if (k == "apron") { ss >> v; c.apron = hexc(v); c.apronOn = true; }
        else if (k == "sash") { ss >> v; c.sash = hexc(v); c.sashOn = true; }
        else if (k == "skin") { ss >> v; c.skin = hexc(v); }
        else if (k == "hair") { ss >> v; c.hair = hexc(v); }
        else if (k == "hat") {
            ss >> v;
            static const char *kinds[] = {"none", "hat", "cap", "coif", "helmet", "squarecap", "hood", "biretta"};
            for (int i = 0; i < 8; i++) if (v == kinds[i]) c.hatKind = i;
            std::streampos p = ss.tellg();
            std::string col;
            if (ss >> col) { if (col[0] == '#') c.hat = hexc(col); else ss.seekg(p); }
        }
        else if (k == "dress") c.dress = true;
        else if (k == "robe") c.robe = true;
        else if (k == "halberd") c.halberd = true;
        else if (k == "musket") c.weapon = 1;
        else if (k == "pike") c.weapon = 2;
        else if (k == "sword") c.weapon = 3;
        else if (k == "sabre") c.weapon = 4;
        else if (k == "child") c.size = 0.62f;
        else if (k == "lute") c.lute = true;
        else if (k == "old") c.age = 1;
        else if (k == "beard") { ss >> v; c.beard = v == "tuft" ? 1 : v == "full" ? 2 : v == "moustache" ? 3 : 0; }
        else if (k == "stout") c.stout = 1;
        else if (k == "plain") c.plain = true;
    }
}

// "KEYS [if COND]: TEXT {effect; effect}"
static bool parseTalk(const std::string &line, TalkLine &t) {
    size_t colon = line.find(": ");
    if (colon == std::string::npos) {
        if (!line.empty() && line.back() == ':') colon = line.size() - 1;
        else return false;
    }
    std::string head = trim(line.substr(0, colon)), text = colon + 2 <= line.size() ? line.substr(colon + 2) : "";
    size_t ifp = head.find(" if ");
    std::string keys = head;
    if (ifp != std::string::npos) { keys = trim(head.substr(0, ifp)); t.cond = trim(head.substr(ifp + 4)); }
    std::string k;
    std::istringstream ks(keys);
    while (std::getline(ks, k, ',')) { k = lower(trim(k)); if (!k.empty()) t.keys.push_back(k); }
    // effects in braces
    std::string out;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '{') {
            size_t j = text.find('}', i);
            if (j == std::string::npos) break;
            std::string e = text.substr(i + 1, j - i - 1), part;
            std::istringstream es(e);
            while (std::getline(es, part, ';')) { part = trim(part); if (!part.empty()) t.effects.push_back(part); }
            i = j;
        } else {
            out += text[i];
        }
    }
    t.text = trim(out);
    return !t.keys.empty();
}

static bool parseSched(const std::string &line, SchedEntry &e) {
    std::istringstream ss(line);
    std::string tm;
    if (!(ss >> tm)) return false;
    if (tm == "always") e.always = true;   // "always ACTIVITY [PLACE] if CONDS": at any hour, over the timetable
    else if (tm.find(':') == std::string::npos) return false;
    else e.minute = parseTime(tm);
    std::string rest, w;
    std::getline(ss, rest);
    rest = trim(rest);
    size_t ifp = rest.find(" if ");
    if (ifp != std::string::npos) { e.cond = trim(rest.substr(ifp + 4)); rest = trim(rest.substr(0, ifp)); }
    std::istringstream rs(rest);
    rs >> e.activity;
    if (e.activity == "sharp") { e.sharp = true; rs >> e.activity; }
    std::getline(rs, e.place);
    e.place = trim(e.place);
    return true;
}

// ------------------------------------------------------------------ loading

// A role's talk lines, made once and held by every person of the role (a role is never changed
// after roles.txt is read).
static std::shared_ptr<const std::vector<TalkLine>> sharedTalk(const Role &r) {
    if (!r.talkShared) r.talkShared = std::make_shared<const std::vector<TalkLine>>(r.talk);
    return r.talkShared;
}

bool People::loadRoles(const std::string &fn) {
    std::ifstream in(fn);
    if (!in) { std::cerr << "cannot open " << fn << "\n"; return false; }
    std::string line;
    Role *cur = nullptr;
    int mode = 0;   // 0 fields, 1 schedule, 2 talk
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        std::istringstream ss(t);
        std::string w;
        ss >> w;
        if (w == "role") {
            std::string id, title;
            ss >> id;
            std::getline(ss, title);
            roles[id] = Role();
            cur = &roles[id];
            cur->id = id;
            title = trim(title);
            if (title.size() >= 2 && title.front() == '"') title = title.substr(1, title.size() - 2);
            cur->title = title.empty() ? "a " + id : title;
            mode = 0;
            continue;
        }
        if (!cur) continue;
        if (w == "end") { if (mode == 2) mode = 0; else cur = nullptr; continue; }
        if (mode == 2) { TalkLine tl; if (parseTalk(t, tl)) cur->talk.push_back(tl); continue; }
        if (w == "schedule") { mode = 1; cur->schedule.clear(); continue; }
        if (w == "talk") { mode = 2; continue; }
        if (mode == 1) { SchedEntry e; if (parseSched(t, e)) { cur->schedule.push_back(e); continue; } mode = 0; }
        if (w == "sex") { std::string s; ss >> s; cur->sex = s.empty() ? 'm' : s[0]; }
        else if (w == "look") { parseLook(cur->look, ss); cur->hasLook = true; }
        else if (w == "work") { std::getline(ss, cur->work); cur->work = trim(cur->work); }
        else if (w == "sign") ss >> cur->sign;
        else if (w == "says") { std::getline(ss, cur->says); cur->says = trim(cur->says); }
        else if (w == "sells") { std::string id; int n; while (ss >> id >> n) cur->sells.push_back({id, n}); }
        else if (w == "money") ss >> cur->money;
        else if (w == "watch") cur->watch = true;
        else if (w == "fight") ss >> cur->fightHp >> cur->fightSkill >> cur->fightDmg >> cur->fightArmour;
        else if (w == "faction") ss >> cur->faction;
        else if (w == "recruits") ss >> cur->recruitMin;
        else if (w == "names") ss >> cur->names;
        else if (w == "hostile") { std::string h; while (ss >> h) cur->hostile.push_back(h); }
    }
    return true;
}

// a soldier's role makes the person a fighter
static void takeFight(Person &p, const Role &r) {
    if (r.fightHp <= 0) return;
    p.fighter = true;
    p.maxHp = r.fightHp; p.hp = (float)r.fightHp;
    p.skill = r.fightSkill; p.dmg = r.fightDmg; p.armour = r.fightArmour;
    p.faction = r.faction;
    p.hostile = r.hostile;
    p.recruitMin = r.recruitMin;
}

bool People::loadNames(const std::string &fn) {
    std::ifstream in(fn);
    if (!in) { std::cerr << "cannot open " << fn << "\n"; return false; }
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        size_t c = t.find(':');
        if (c == std::string::npos) continue;
        std::string key = trim(t.substr(0, c)), w;
        std::istringstream ss(t.substr(c + 1));
        while (ss >> w) names[key].push_back(w);
    }
    return true;
}

bool People::loadTown(World &w, const std::string &fn) {
    std::ifstream in(fn);
    if (!in) { std::cerr << "cannot open " << fn << "\n"; return false; }
    std::string line, town, pool = "french", country;   // country: stamped on everyone's memory
    Person *cur = nullptr;
    int mode = 0;
    bool ownSchedule = false;
    struct Crowd { int n; std::string role, town, pool, country, work; };
    std::vector<Crowd> crowds;
    auto finish = [&]() {
        if (!cur) return;
        auto it = roles.find(cur->role);
        if (it != roles.end()) {
            const Role &r = it->second;
            if (!ownSchedule) cur->schedule = r.schedule;
            if (cur->says.empty()) cur->says = r.says;
            if (cur->workKind.empty()) cur->workKind = r.work;
            if (cur->sign.empty()) cur->sign = r.sign;
            if (cur->title.empty()) cur->title = r.title;
            cur->baseTitle = cur->title;
            cur->roleTalk = sharedTalk(r);
            if (cur->sells.empty()) cur->sells = r.sells;   // their own wares replace the role's
            cur->watch = cur->watch || r.watch;
            takeFight(*cur, r);
            if (!cur->mem.count("money") && r.money) cur->mem["money"] = std::to_string(r.money);
        }
        cur->everyoneTalk = sharedTalk(roles["everyone"]);
        cur = nullptr;
    };
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        std::istringstream ss(t);
        std::string word;
        ss >> word;
        if (!cur || mode == 0) {
            if (word == "town") { std::getline(ss, town); town = trim(town); country.clear(); continue; }   // each town its own country: none without a "country" line (the herds then take France)
            if (word == "names") { ss >> pool; continue; }
            if (word == "country") { ss >> country; continue; }
            if (word == "prison") { std::string r; std::getline(ss, r); prisons[town] = trim(r); continue; }
            if (word == "guarded") {   // guarded PLACE / by ID / pass if CONDS / key ITEM / stop TEXT {EFFECTS} / shut TEXT / end
                Guarded gd;
                gd.town = town;
                std::getline(ss, gd.place);
                gd.place = trim(gd.place);
                std::string gl;
                while (std::getline(in, gl)) {
                    std::string g2 = trim(gl);
                    if (g2.empty() || g2[0] == '#') continue;
                    if (g2 == "end") break;
                    std::string k = g2.substr(0, g2.find(' ')), v = g2.find(' ') == std::string::npos ? "" : trim(g2.substr(g2.find(' ') + 1));
                    if (k == "by") gd.by = lower(town) + ":" + v;
                    else if (k == "key") gd.key = v;
                    else if (k == "shut") gd.shut = v;
                    else if (k == "stop") {
                        size_t b;
                        while ((b = v.find('{')) != std::string::npos) {   // effects, on you, when he stops you
                            size_t e = v.find('}', b);
                            if (e == std::string::npos) break;
                            gd.stopEffects.push_back(trim(v.substr(b + 1, e - b - 1)));
                            v = trim(v.substr(0, b) + v.substr(e + 1));
                        }
                        gd.stop = v;
                    } else if (k == "pass") {
                        std::string rest = v.rfind("if ", 0) == 0 ? trim(v.substr(3)) : v;
                        size_t pos;
                        while ((pos = rest.find(" and ")) != std::string::npos) { gd.pass.push_back(trim(rest.substr(0, pos))); rest = rest.substr(pos + 5); }
                        if (!rest.empty()) gd.pass.push_back(trim(rest));
                    } else std::fprintf(stderr, "%s: guarded %s: cannot read \"%s\"\n", fn.c_str(), gd.place.c_str(), g2.c_str());
                }
                guarded.push_back(gd);
                continue;
            }
            if (word == "crowd") {   // crowd N ROLE [work KIND ...]: the work replaces the role's
                int n; std::string r, wk, rest; ss >> n >> r >> wk;
                if (wk == "work") { std::getline(ss, rest); rest = trim(rest); }
                crowds.push_back({n, r, town, pool, country, rest});
                continue;
            }
        }
        if (word == "town" || word == "names") {   // a new place in the same file
            finish();
            if (word == "town") { std::getline(ss, town); town = trim(town); country.clear(); } else ss >> pool;
            continue;
        }
        if (word == "person") {
            finish();
            people.push_back(Person());
            cur = &people.back();
            std::string id;
            ss >> id;
            cur->id = lower(town) + ":" + id;
            cur->town = town;
            if (!country.empty()) cur->mem["country"] = country;
            mode = 0;
            ownSchedule = false;
            continue;
        }
        if (!cur) continue;
        if (word == "end") { if (mode == 2) mode = 0; else finish(); continue; }
        if (mode == 2) { TalkLine tl; if (parseTalk(t, tl)) cur->talk.push_back(tl); continue; }
        if (word == "schedule") { mode = 1; cur->schedule.clear(); ownSchedule = true; continue; }
        if (word == "talk") { mode = 2; continue; }
        if (mode == 1) { SchedEntry e; if (parseSched(t, e)) { cur->schedule.push_back(e); continue; } mode = 0; }
        if (word == "name") { std::getline(ss, cur->name); cur->name = trim(cur->name); }
        else if (word == "title") {   // "title TEXT", or "title if CONDS: TEXT" (Murdo Ross, whole at the muster, then wounded)
            std::string t2; std::getline(ss, t2); t2 = trim(t2);
            size_t colon = t2.find(": ");
            if (t2.rfind("if ", 0) == 0 && colon != std::string::npos) cur->titleIf.push_back({trim(t2.substr(3, colon - 3)), trim(t2.substr(colon + 2))});
            else cur->title = t2;
        }
        else if (word == "known") { std::getline(ss, cur->known); cur->known = trim(cur->known); }
        else if (word == "died") {   // died if COND | TEXT
            std::string rest; std::getline(ss, rest); rest = trim(rest);
            if (rest.rfind("if ", 0) == 0) rest = rest.substr(3);
            size_t bar = rest.find('|');
            cur->diedIf = trim(rest.substr(0, bar));
            cur->diedText = bar == std::string::npos ? "" : trim(rest.substr(bar + 1));
        }
        else if (word == "role") {
            ss >> cur->role;
            auto it = roles.find(cur->role);
            if (it != roles.end()) { cur->look = it->second.look; cur->sex = it->second.sex; }
        }
        else if (word == "sex") { std::string s; ss >> s; cur->sex = s[0]; }
        else if (word == "look") parseLook(cur->look, ss);
        else if (word == "near") { std::getline(ss, cur->nearHint); cur->nearHint = trim(cur->nearHint); }
        else if (word == "lodges") cur->mem["lodger"] = "1";   // sleeps at an inn (near its "near"), among the guests
        else if (word == "sparebed") cur->mem["spare-bed"] = "1";   // a house with a bed to spare, kept for a guest ("sleep home-of ID": Morales hidden)
        else if (word == "household") { std::string id; ss >> id; cur->mem["household"] = lower(town) + ":" + id; }   // lives in that person's house (a family)
        else if (word == "work") { std::getline(ss, cur->workKind); cur->workKind = trim(cur->workKind); }
        else if (word == "sign") ss >> cur->sign;
        else if (word == "shopname") { std::getline(ss, cur->shopName); cur->shopName = trim(cur->shopName); }
        else if (word == "says") { std::getline(ss, cur->says); cur->says = trim(cur->says); }
        else if (word == "know") { std::string k, v; ss >> k; std::getline(ss, v); v = trim(v); cur->mem[k] = v.empty() ? "1" : v; }
        else if (word == "sells") { std::string id; int n; while (ss >> id >> n) cur->sells.push_back({id, n}); }
        else if (word == "money") { long m = 0; ss >> m; cur->mem["money"] = std::to_string(m); }
    }
    finish();
    // the crowd: ordinary townspeople with names drawn from the town's pool. Their ids are
    // town:role:N, numbered on across crowd lines of the same role (two crowds of musketeers
    // once gave two men the same id)
    std::map<std::string, int> crowdNext;
    for (auto &c : crowds) {
        auto it = roles.find(c.role);
        if (it == roles.end()) { std::cerr << fn << ": no role " << c.role << "\n"; continue; }
        std::string town = c.town, pool = it->second.names.empty() ? c.pool : it->second.names;   // a role may bring its own names
        int &first = crowdNext[lower(town) + ":" + c.role];
        for (int i = first; i < first + c.n; i++) {
            Person p;
            const Role &r = it->second;
            p.id = lower(town) + ":" + c.role + ":" + std::to_string(i);
            Rng rng(hash32((uint32_t)std::hash<std::string>()(p.id)));
            p.town = town;
            p.role = c.role;
            if (!c.country.empty()) p.mem["country"] = c.country;
            p.sex = r.sex == 'x' ? (rng.chance(0.5f) ? 'm' : 'f') : r.sex;
            p.look = r.look;
            if (p.sex == 'f' && r.sex == 'x') { p.look.dress = true; p.look.hatKind = 3; p.look.hat = vec3(0.92f, 0.9f, 0.86f); }
            // a little variety in colour
            float k = 0.8f + 0.4f * rng.uni();
            p.look.coat = p.look.coat * k;
            static const vec3 hairs[4] = {{0.30f, 0.20f, 0.12f}, {0.12f, 0.09f, 0.07f}, {0.55f, 0.40f, 0.22f}, {0.45f, 0.42f, 0.40f}};
            int hk = (int)(rng.next() % 4);   // the fourth, grey, comes with age (drawLook), not by chance: another colour
            if (hk == 3) hk = (int)(Rng(hash32((uint32_t)std::hash<std::string>()(p.id + "#hair"))).next() % 3);
            p.look.hair = hairs[hk];
            p.look.skin = p.look.skin * (0.85f + 0.15f * rng.uni());
            auto &first = names[pool + " " + (p.sex == 'f' ? "female" : "male")];
            auto &last = names[pool + " surname"];
            p.name = (first.empty() ? "John" : first[rng.next() % first.size()]) + " " + (last.empty() ? "Smith" : last[rng.next() % last.size()]);
            for (auto &ch : p.name) if (ch == '_') ch = ' ';   // "de_Vries" in the pool is "de Vries"
            p.title = r.title;
            p.says = r.says;
            p.schedule = r.schedule;
            p.workKind = c.work.empty() ? r.work : c.work;
            p.sign = r.sign;
            takeFight(p, r);
            p.sells = r.sells;
            p.watch = r.watch;
            if (r.money) p.mem["money"] = std::to_string(r.money);
            p.roleTalk = sharedTalk(r);
            p.everyoneTalk = sharedTalk(roles["everyone"]);
            people.push_back(p);
        }
        first += c.n;
    }
    for (auto &p : people) p.look.woman = p.sex == 'f';   // a woman's gown is cut differently (buildFigureFine)
    (void)w;
    return true;
}

// ------------------------------------------------------------------ homes and workplaces

namespace {

struct TownInfo { const Settlement *s = nullptr; };

const Settlement *townOf(const World &w, const std::string &name) {
    for (auto &s : w.settlements) if (s.name == name) return &s;
    return nullptr;
}

// Can every open tile on the ground floor of building id be reached from its doors to the
// outside? (Furniture put into a house must not shut off a room.)
static bool allReachable(World &w, int id) {
    const Building &b = w.buildings[id - 1];
    int bw = b.x1 - b.x0 + 1, bh = b.y1 - b.y0 + 1;
    auto mine = [&](int X, int Y) { return X >= b.x0 && X <= b.x1 && Y >= b.y0 && Y <= b.y1 && w.tile(X, Y).bld == id; };
    std::vector<char> seen(bw * bh, 0);
    auto at = [&](int X, int Y) -> char & { return seen[(Y - b.y0) * bw + (X - b.x0)]; };
    std::vector<std::pair<int, int>> q;
    const int d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int X = b.x0; X <= b.x1; X++)
        for (int Y = b.y0; Y <= b.y1; Y++) {
            if (!mine(X, Y) || (w.tile(X, Y).wall != W_DOOR && w.tile(X, Y).wall != W_GATE)) continue;
            for (auto &d : d4)
                if (!mine(X + d[0], Y + d[1]) && w.passable(X + d[0], Y + d[1]) && !at(X, Y)) { at(X, Y) = 1; q.push_back({X, Y}); }
        }
    for (size_t k = 0; k < q.size(); k++) {
        int x = q[k].first, y = q[k].second;
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++) {
                if (!dx && !dy) continue;
                int nx = x + dx, ny = y + dy;
                if (!mine(nx, ny) || at(nx, ny) || !w.passable(nx, ny)) continue;
                if (dx && dy && (w.blocksDiagonal(x + dx, y) || w.blocksDiagonal(x, y + dy))) continue;
                if (dx && dy && !w.passable(x + dx, y) && !w.passable(x, y + dy)) continue;
                at(nx, ny) = 1;
                q.push_back({nx, ny});
            }
    }
    for (int X = b.x0; X <= b.x1; X++)
        for (int Y = b.y0; Y <= b.y1; Y++)
            if (mine(X, Y) && w.passable(X, Y) && !at(X, Y)) return false;
    return true;
}

bool freeTile(World &w, int X, int Y, int Z) {
    if (!w.passable(X, Y, Z) || w.linkAt(X, Y, Z)) return false;
    Tile *t = w.tileAt(X, Y, Z);
    return t && t->feature == F_NONE && t->wall == W_NONE;
}

// a tile beside a ladder, a trapdoor or stairs: the way on to them, to be kept clear of furniture
bool besideClimb(World &w, int X, int Y, int Z) {
    const int d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (auto &d : d4) {
        Tile *n = w.tileAt(X + d[0], Y + d[1], Z);
        if (n && (n->feature == F_LADDER || n->feature == F_HATCH || n->feature == F_STAIRS)) return true;
    }
    return false;
}

}  // namespace

void World::setFeature(int X, int Y, int z, uint8_t f, uint8_t var) {
    if (z != 0) {
        Tile *t = tileAt(X, Y, z);
        if (t) { t->feature = f; t->var = var; }
        return;
    }
    for (auto &s : settlements)
        if (s.inside(X, Y) && s.ch(X, Y) != ' ') { s.tileAt(X, Y).feature = f; s.tileAt(X, Y).var = var; }
    int cx = (int)std::floor(X / (float)CH), cy = (int)std::floor(Y / (float)CH);
    if (Chunk *c = chunkIfReady(cx, cy)) {
        Tile &t = c->t(X - cx * CH, Y - cy * CH);
        t.feature = f;
        t.var = var;
        c->dirty = true;
    }
}

bool World::addSign(int bld, const std::string &emblem) {
    if (bld <= 0 || bld > (int)buildings.size() || emblem.empty() || emblem == "none") return false;
    for (const Sign &s : signs) if (s.bld == bld) return false;
    const Building &b = buildings[bld - 1];
    const int d4[4][2] = {{0, -1}, {1, 0}, {-1, 0}, {0, 1}};
    auto outside = [&](int X, int Y) { Tile &t = tile(X, Y); return t.bld != bld && t.wall == W_NONE && passable(X, Y, 0); };
    for (int Y = b.y0; Y <= b.y1; Y++)
        for (int X = b.x0; X <= b.x1; X++) {
            Tile &t = tile(X, Y);
            if (t.bld != bld || t.wall != W_DOOR) continue;
            for (auto &d : d4) {
                if (!outside(X + d[0], Y + d[1])) continue;
                // from the wall beside the door, over the street; else from above the door itself
                for (int k : {1, -1}) {
                    int sx = X - k * d[1], sy = Y + k * d[0];
                    Tile &n = tile(sx, sy);
                    if (n.bld == bld && (n.wall == W_WALL || n.wall == W_WINDOW) && outside(sx + d[0], sy + d[1])) {
                        signs.push_back({sx, sy, d[0], d[1], b.ground + GF_H - 0.05f, emblem, bld});
                        return true;
                    }
                }
                signs.push_back({X, Y, d[0], d[1], b.ground + GF_H - 0.02f, emblem, bld});
                return true;
            }
        }
    return false;
}

void People::assign(World &w) {
    goalTiles.clear();   // the posts given out so far (cleared again at the first update)
    // houses of each town, with their beds
    struct House { int bld; std::vector<std::array<int, 3>> beds; int used = 0; bool workshop = false; };
    std::map<std::string, std::vector<House>> houses;
    for (size_t i = 0; i < w.buildings.size(); i++) {
        const Building &b = w.buildings[i];
        if (b.kind != "house" && b.kind != "inn") continue;
        const Settlement *town = nullptr;
        for (auto &s : w.settlements) if (s.inside(b.x0, b.y0)) town = &s;
        if (!town) continue;
        House h;
        h.bld = (int)i + 1;
        for (int z = 0; z <= 1; z++)
            for (int X = b.x0; X <= b.x1; X++)
                for (int Y = b.y0; Y <= b.y1; Y++) {
                    Tile *t = w.tileAt(X, Y, z);
                    if (t && t->feature == F_BED) h.beds.push_back({X, Y, z});
                }
        if (!h.beds.empty()) houses[town->name].push_back(h);
    }
    auto hintPoint = [&](const Person &p, int &hx, int &hy) {
        const Settlement *s = townOf(w, p.town);
        hx = s ? s->x0 + s->w / 2 : 0;
        hy = s ? s->y0 + s->h / 2 : 0;
        if (p.nearHint.empty()) return false;
        float a, b;
        if (std::sscanf(p.nearHint.c_str(), "%f,%f", &a, &b) == 2) { hx = (int)a; hy = (int)b; return true; }
        std::string h = lower(p.nearHint);
        for (auto &bd : w.buildings)
            if (!bd.name.empty() && lower(bd.name).find(h) != std::string::npos && s && s->inside(bd.x0, bd.y0)) {
                hx = (bd.x0 + bd.x1) / 2; hy = (bd.y0 + bd.y1) / 2;
                return true;
            }
        return false;
    };
    // homes: the named first, near their hints; then the crowd
    for (int pass = 0; pass < 2; pass++)
        for (auto &p : people) {
            bool named = p.id.find(':', p.id.find(':') + 1) == std::string::npos;
            if ((pass == 0) != named || p.hasBed) continue;
            auto &hs = houses[p.town];
            if (hs.empty()) continue;
            if (p.mem.count("household")) {   // a family: in the house of the one named, if it has a bed to spare
                const Person *o = byId(p.mem["household"]);
                for (auto &h : hs)
                    if (o && o->hasBed && h.bld == o->home && h.used < (int)h.beds.size()) {
                        auto bed = h.beds[h.used++];
                        p.home = h.bld; p.hasBed = true; p.bedX = bed[0]; p.bedY = bed[1]; p.bedZ = bed[2];
                        break;
                    }
                if (p.hasBed) continue;
            }
            int hx, hy;
            bool hinted = hintPoint(p, hx, hy);
            Rng rng(hash32((uint32_t)std::hash<std::string>()(p.id) + 17));
            int best = -1;
            float bd = 1e9f;
            bool lodger = p.mem.count("lodger") > 0;
            bool wantInn = p.role == "innkeeper" || lodger;
            for (size_t i = 0; i < hs.size(); i++) {
                House &h = hs[i];
                if (h.used >= (int)h.beds.size()) continue;
                if (named && h.used > 0 && !lodger) continue;   // the named live on their own, except lodgers at an inn
                if (p.mem.count("spare-bed") && (int)h.beds.size() - h.used < 2) continue;
                const Building &b = w.buildings[h.bld - 1];
                if (wantInn != (b.kind == "inn")) continue;
                float d = hinted ? std::abs((b.x0 + b.x1) / 2 - hx) + std::abs((b.y0 + b.y1) / 2 - hy) : rng.uni() * 1000;
                if (d < bd) { bd = d; best = (int)i; }
            }
            if (best < 0) continue;
            House &h = hs[best];
            p.home = h.bld;
            auto bed = h.beds[h.used++];
            p.hasBed = true;
            p.bedX = bed[0]; p.bedY = bed[1]; p.bedZ = bed[2];
            if (p.mem.count("spare-bed")) h.used = (int)h.beds.size();   // nobody else is given the spare bed: it is the guest's
            if (named && (p.workKind == "forge" || p.workKind == "shop" || p.workKind == "bakery")) h.workshop = true;
        }
    // signs: every inn's over its door (more come with the workshops below)
    for (size_t i = 0; i < w.buildings.size(); i++)
        if (w.buildings[i].kind == "inn") w.addSign((int)i + 1, "inn");
    // workplaces
    for (auto &p : people) {
        Rng rng(hash32((uint32_t)std::hash<std::string>()(p.id) + 29));
        const Settlement *s = townOf(w, p.town);
        if (!s) continue;
        std::string kind = p.workKind, arg;
        size_t sp = kind.find(' ');
        if (sp != std::string::npos) { arg = trim(kind.substr(sp + 1)); kind = kind.substr(0, sp); }
        if (kind.empty() || kind == "none") continue;
        if ((kind == "forge" || kind == "shop" || kind == "bakery") && p.home) {
            // the workshop is the ground floor of their own house
            const Building &b = w.buildings[p.home - 1];
            uint8_t big = kind == "forge" ? F_FORGE : kind == "shop" ? F_COUNTER : F_OVEN;
            uint8_t small = kind == "forge" ? F_ANVIL : kind == "shop" ? F_CHEST : F_TABLE;
            bool done = false;
            for (int Y = b.y0 + 1; Y < b.y1 && !done; Y++)
                for (int X = b.x0 + 1; X < b.x1 && !done; X++) {
                    // the big piece against a wall; the small one in front; the worker beside it
                    const int d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                    for (int k = 0; k < 4 && !done; k++) {
                        int wx = X + d4[k][0], wy = Y + d4[k][1];
                        Tile *wt = w.tileAt(wx, wy, 0);
                        if (!wt || (wt->wall != W_WALL && wt->wall != W_WINDOW)) continue;
                        int ax = X - d4[k][0], ay = Y - d4[k][1];       // in front of the big piece
                        int sx = ax - d4[k][1], sy = ay + d4[k][0];     // beside that
                        if (!freeTile(w, X, Y, 0) || !freeTile(w, ax, ay, 0) || !freeTile(w, sx, sy, 0)) continue;
                        if (besideClimb(w, X, Y, 0) || besideClimb(w, ax, ay, 0)) continue;
                        if (ax <= b.x0 || ax >= b.x1 || ay <= b.y0 || ay >= b.y1 || sx <= b.x0 || sx >= b.x1 || sy <= b.y0 || sy >= b.y1) continue;
                        bool nearDoor = false;
                        for (int dx = -1; dx <= 1; dx++)
                            for (int dy = -1; dy <= 1; dy++) {
                                Tile *n = w.tileAt(X + dx, Y + dy, 0);
                                Tile *n2 = w.tileAt(ax + dx, ay + dy, 0);
                                if ((n && n->wall == W_DOOR) || (n2 && n2->wall == W_DOOR)) nearDoor = true;
                            }
                        if (nearDoor) continue;
                        w.setFeature(X, Y, 0, big, (uint8_t)k);
                        w.setFeature(ax, ay, 0, small, 0);
                        // still possible to walk everywhere in the house, from the street door?
                        if (allReachable(w, p.home)) {
                            p.hasWork = true;
                            w.addSign(p.home, !p.sign.empty() ? p.sign : kind == "forge" ? "anvil" : kind == "bakery" ? "pretzel" : "scales");
                            if (!p.shopName.empty() && w.buildings[p.home - 1].name.empty()) w.buildings[p.home - 1].name = p.shopName;
                            p.workX = sx; p.workY = sy; p.workZ = 0;
                            done = true;
                        } else {
                            w.setFeature(X, Y, 0, F_NONE, 0);
                            w.setFeature(ax, ay, 0, F_NONE, 0);
                        }
                    }
                }
            if (done) continue;
            kind = "market";   // no room for a workshop: a stall at the market instead
        }
        p.workPlace = kind + (arg.empty() ? "" : " " + arg);
        // "the nearest inn" (church, college, theatre, landmark) is measured from where they stand, and nobody stands
        // anywhere yet (0,0: the map's corner, so London's innkeeper of the Mermaid worked at the Tabard and every
        // London beggar at Westminster Abbey): measure it from their home, or failing that from
        // their "near" place or the middle of the town (hintPoint)
        int sx0 = p.x, sy0 = p.y;
        if (p.home) { const Building &hb = w.buildings[p.home - 1]; p.x = (hb.x0 + hb.x1) / 2; p.y = (hb.y0 + hb.y1) / 2; }
        else hintPoint(p, p.x, p.y);
        int gx, gy, gz;
        bool placed = placeFor(w, p, "go", p.workPlace, gx, gy, gz);
        p.x = sx0; p.y = sy0;
        if (std::getenv("GALILEO_WORKLOG")) std::fprintf(stderr, "WORKAT %s %s | %s\n", p.id.c_str(), p.workPlace.c_str(),
            !placed ? "NONE" : w.tile(gx, gy).bld ? (w.buildings[w.tile(gx, gy).bld - 1].name + "#" + std::to_string(w.tile(gx, gy).bld)).c_str() : "street");
        if (placed) {
            p.hasWork = true;
            p.workX = gx; p.workY = gy; p.workZ = gz;
            goalTiles.insert(World::key3(gx, gy, gz));   // one post each: two guards never share one
        }
        (void)rng;
    }
}

// Who someone is, for everyone whose look does not say (the crowds, the road's people, and named people without "old",
// "beard", "stout" or "plain"): drawn from a sequence of their own, so that names and colours stay as they were. Old 1 in
// 8 (never children, students, apprentices, or your father and mother); a man's beard: a tuft 1 in 3, a moustache 1 in
// 6, full 1 in 6 (never the clergy who shave); stout 1 in 8 (never children); the height within 4% either way.
static void drawLook(Person &p) {
    Costume &c = p.look;
    if (c.plain || c.age > 0 || c.beard > 0 || c.stout > 0) return;
    c.plain = true;   // drawn once
    Rng r(hash32((uint32_t)std::hash<std::string>()(p.id + "#look")));
    float uo = r.uni(), ub = r.uni(), us = r.uni(), uh = r.uni();
    bool child = c.size < 0.9f, young = p.role == "student" || p.role == "apprentice";
    bool shaven = p.role == "priest" || p.role == "friar" || p.role == "monk" || p.role == "dominican" || p.role == "jesuit" || p.role == "cardinal";
    if (!child && !young && !p.mem.count("family") && uo < 1.0f / 8) c.age = 1;
    if (!child && !shaven && p.sex == 'm') c.beard = ub < 1.0f / 3 ? 1 : ub < 0.5f ? 3 : ub < 2.0f / 3 ? 2 : 0;
    if (!child && us < 1.0f / 8) c.stout = 1;
    c.size *= 1 + 0.08f * (uh - 0.5f);
}

bool People::load(World &w, const std::string &dir) {
    static const std::set<std::string> common = {"post", "house", "saint", "sankt", "santa", "venta", "ruins", "farm", "cottage"};
    for (const auto &s : w.settlements) {   // townWords: the distinctive words of the places' names
        std::string word;
        for (char c : lower(s.name) + " ") {
            if (std::isalpha((unsigned char)c) || (c & 0x80)) word += c;
            else { if (word.size() >= 4 && !common.count(word)) townWords.insert(word); word.clear(); }
        }
    }
    if (!loadRoles(dir + "/people/roles.txt")) return false;
    loadNames(dir + "/people/names.txt");
    std::ifstream list(dir + "/people/towns.txt");
    std::string fn;
    while (list >> fn) {
        if (fn[0] == '#') { std::getline(list, fn); continue; }
        loadTown(w, dir + "/people/" + fn);
    }
    for (auto &p : people) if (!p.mem.count("town")) p.mem["town"] = p.town;
    for (auto &p : people) drawLook(p);
    loadChats(dir + "/people/chats.txt");
    loadTrades(dir + "/people/trades.txt");
    loadEvents(dir + "/events.txt");
    loadStories(dir + "/stories.txt");
    loadEnding(dir + "/ending.txt");
    loadMarkets(w, dir + "/market.txt");
    {   // every name in a "between" must be a role or a person
        std::set<std::string> ids;
        for (auto &r : roles) ids.insert(r.first);
        for (auto &p : people) { ids.insert(p.id); ids.insert(p.id.substr(p.id.find(':') + 1)); }
        for (auto &c : chats)
            for (auto *l : {&c.a, &c.b})
                for (auto &x : *l)
                    if (x != "anyone" && !ids.count(x)) std::fprintf(stderr, "chats.txt: %s: no role or person called %s\n", c.id.c_str(), x.c_str());
    }
    assign(w);
    // everyone starts at home, or at work
    for (auto &p : people) {
        int X = 0, Y = 0, Z = 0;
        if (p.hasBed) {
            const Building &b = w.buildings[p.home - 1];
            X = (b.x0 + b.x1) / 2; Y = (b.y0 + b.y1) / 2;
        } else if (p.hasWork) {
            X = p.workX; Y = p.workY; Z = p.workZ;
        } else if (const Settlement *s = townOf(w, p.town)) {
            X = s->x0 + s->w / 2; Y = s->y0 + s->h / 2;
        }
        p.x = p.px = X; p.y = p.py = Y; p.z = p.pz = Z;
        Rng rng(hash32((uint32_t)std::hash<std::string>()(p.id) + 3));
        p.stepDur = 0.55f + 0.25f * rng.uni();   // a stroll: under two tiles a second
        p.strollDur = p.stepDur;
        p.nextSay = 5 + 30 * rng.uni();
    }
    return true;
}

// the road's people: n of a role, made like a crowd's but belonging to no town, away until the
// game puts them on a road (Game::wildStep); only the role's own lines
int People::makeWild(const std::string &role, int n, const std::string &id) {
    auto it = roles.find(role);
    if (it == roles.end()) { std::cerr << "roles.txt: no role " << role << "\n"; return -1; }
    const Role &r = it->second;
    int first = (int)people.size();
    for (int i = 0; i < n; i++) {
        Person p;
        p.id = "roads:" + id + ":" + std::to_string(i);
        Rng rng(hash32((uint32_t)std::hash<std::string>()(p.id)));
        p.role = role;
        p.sex = r.sex;
        p.look = r.look;
        p.look.woman = p.sex == 'f';
        p.look.coat = p.look.coat * (0.8f + 0.4f * rng.uni());
        static const vec3 hairs[4] = {{0.30f, 0.20f, 0.12f}, {0.12f, 0.09f, 0.07f}, {0.55f, 0.40f, 0.22f}, {0.45f, 0.42f, 0.40f}};
        int hk = (int)(rng.next() % 4);   // grey comes with age, as in a town's crowd
        if (hk == 3) hk = (int)(Rng(hash32((uint32_t)std::hash<std::string>()(p.id + "#hair"))).next() % 3);
        p.look.hair = hairs[hk];
        p.name = "";   // nobody you will ever know by name (and the talk's heading is only "A brigand")
        p.title = r.title;
        p.says = r.says;
        takeFight(p, r);
        p.roleTalk = sharedTalk(r);
        p.wild = true;
        p.away = true;
        p.active = false;
        p.placed = true;
        p.stepDur = p.strollDur = 0.5f;
        p.mem["town"] = "";
        drawLook(p);
        people.push_back(p);
    }
    return first;
}

// ------------------------------------------------------------------ route finding

// A* over tiles and floors: the same moves as the player, and ladders and stairs.
// The nodes seen are kept in flat arrays that are reused from one search to the next (pathNodes, pathIndex,
// pathHeap). The order of expansion, the neighbour order and the costs are those of the first version (a std::
// priority_queue ordered by f alone is a heap on a vector, as pathHeap is), so the paths are the same.
bool People::findPath(World &w, int sx, int sy, int sz, int gx, int gy, int gz, std::vector<std::array<int, 3>> &out, int limit) {
    out.clear();
    if (sx == gx && sy == gy && sz == gz) return true;
    struct Cmp { bool operator()(const PathHeapItem &a, const PathHeapItem &b) const { return a.f > b.f; } };
    pathNodes.clear();
    pathHeap.clear();
    pathIndex.clear();
    auto h = [&](int x, int y, int z) {
        int dx = std::abs(x - gx), dy = std::abs(y - gy);
        return (float)std::max(dx, dy) + 0.41f * std::min(dx, dy) + 6.0f * std::abs(z - gz);
    };
    // the move from (px,py,pz) by (dx,dy): where it leads and what it costs, or false if there is none
    auto step = [&](int px, int py, int pz, int dx, int dy, int &nx, int &ny, int &nz, float &cost) {
        nx = px + dx; ny = py + dy; nz = pz;
        cost = (dx && dy) ? 1.41f : 1.0f;
        if (const Link *lk = w.linkAt(nx, ny, nz)) {
            if (dx && dy) return false;
            nx = lk->x; ny = lk->y; nz = lk->z;
            cost = 3.0f;
        } else {
            if (!w.passable(nx, ny, nz)) return false;
            if (dx && dy && (w.blocksDiagonal(px + dx, py, nz) || w.blocksDiagonal(px, py + dy, nz))) return false;
            if (dx && dy && nz == 0 && !w.passable(px + dx, py, nz) && !w.passable(px, py + dy, nz)) return false;
        }
        return true;
    };
    int64_t s = World::key3(sx, sy, sz), goal = World::key3(gx, gy, gz);
    // A backward look from the goal (tried once, after kProbeAt expansions have not reached it): the tiles from
    // which the goal can be reached by the moves above, found by working backwards from it. If they are few and the
    // start is not among them, no route exists, and the search can stop now instead of running to its limit (a goal
    // behind a door that someone stands in, or in a closed court). The set made is a superset of the true one, so
    // "not there" is certain; "cannot tell" (too many tiles, or the start is among them) leaves the search as it was.
    auto unreachable = [&]() {
        const size_t kMax = 1000;
        probeSeen.clear();
        probeQueue.clear();
        if (goal == avoid) return true;   // the search never enters this tile
        probeSeen[goal] = 1;
        probeQueue.push_back({gx, gy, gz});
        for (size_t qi = 0; qi < probeQueue.size(); qi++) {
            const std::array<int, 3> v = probeQueue[qi];
            auto add = [&](int ux, int uy, int uz) {   // false: stop, cannot tell
                int64_t uk = World::key3(ux, uy, uz);
                if (uk == s) return false;
                if (uk == avoid || probeSeen.count(uk)) return true;
                probeSeen[uk] = 1;
                probeQueue.push_back({ux, uy, uz});
                return probeQueue.size() <= kMax;
            };
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++) {
                    if (!dx && !dy) continue;
                    int ux = v[0] - dx, uy = v[1] - dy, nx, ny, nz;
                    float cost;
                    if (v[2] == 0 && !w.inWorld(ux, uy) && World::key3(ux, uy, 0) != s) continue;   // the search is never off the map
                    if (step(ux, uy, v[2], dx, dy, nx, ny, nz, cost) && World::key3(nx, ny, nz) == World::key3(v[0], v[1], v[2]))
                        if (!add(ux, uy, v[2])) return false;
                }
            for (const std::array<int, 3> &ln : w.linksInto(v[0], v[1], v[2])) {   // from beside a ladder or stair that leads here
                static const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto &d : d4)
                    if (!add(ln[0] - d[0], ln[1] - d[1], ln[2])) return false;
            }
        }
        return true;
    };
    const int kProbeAt = 3000;
    bool probed = false;
    pathNodes.push_back({0.0f, -1, sx, sy, sz});
    pathIndex[s] = 1;   // node index + 1 (0: not seen)
    pathHeap.push_back({h(sx, sy, sz), 0});
    int expanded = 0;
    while (!pathHeap.empty() && expanded < limit) {
        PathHeapItem n = pathHeap.front();
        std::pop_heap(pathHeap.begin(), pathHeap.end(), Cmp());
        pathHeap.pop_back();
        const int px = pathNodes[n.node].x, py = pathNodes[n.node].y, pz = pathNodes[n.node].z;
        if (World::key3(px, py, pz) == goal) break;
        float gn = pathNodes[n.node].g;
        if (n.f - h(px, py, pz) > gn + 1e-3f) continue;   // stale
        expanded++;
        if (expanded == kProbeAt && !probed && limit > kProbeAt) { probed = true; if (unreachable()) return false; }
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++) {
                if (!dx && !dy) continue;
                int nx, ny, nz;
                float cost;
                if (!step(px, py, pz, dx, dy, nx, ny, nz, cost)) continue;
                int64_t k = World::key3(nx, ny, nz);
                if (k == avoid) continue;
                float ng = gn + cost;
                int &slot = pathIndex[k];   // node index + 1; made (0) if the tile is new
                if (slot != 0) {
                    PathNode &old = pathNodes[slot - 1];
                    if (old.g <= ng) continue;
                    old.g = ng; old.parent = n.node; old.x = nx; old.y = ny; old.z = nz;
                    pathHeap.push_back({ng + h(nx, ny, nz), slot - 1});
                } else {
                    pathNodes.push_back({ng, n.node, nx, ny, nz});
                    slot = (int)pathNodes.size();
                    pathHeap.push_back({ng + h(nx, ny, nz), slot - 1});
                }
                std::push_heap(pathHeap.begin(), pathHeap.end(), Cmp());
            }
    }
    const int *gi = pathIndex.find(goal);
    if (!gi || *gi - 1 == 0) return false;   // never seen (the start has no parent)
    for (int32_t i = *gi - 1; i != 0; i = pathNodes[i].parent) out.push_back({pathNodes[i].x, pathNodes[i].y, pathNodes[i].z});
    std::reverse(out.begin(), out.end());
    return true;
}

// ------------------------------------------------------------------ where to go

bool People::placeFor(World &w, Person &p, const std::string &activity, const std::string &place, int &gx, int &gy, int &gz) {
    Rng rng(hash32((uint32_t)std::hash<std::string>()(p.id) + (uint32_t)(lastMinutes / 30) * 7919u + (uint32_t)activity.size()));
    const Settlement *s = townOf(w, p.town);
    auto inBuilding = [&](int bld, int z, bool nearAltar, bool nearTable) {
        const Building &b = w.buildings[bld - 1];
        std::vector<std::array<int, 3>> cands;
        for (int X = b.x0; X <= b.x1; X++)
            for (int Y = b.y0; Y <= b.y1; Y++) {
                if (!freeTile(w, X, Y, z) || taken(X, Y, z)) continue;
                bool byDoor = false;   // keep the way in and out clear
                for (int dx = -1; dx <= 1 && !byDoor; dx++)
                    for (int dy = -1; dy <= 1 && !byDoor; dy++) {
                        const Tile *t = w.tileAt(X + dx, Y + dy, z);
                        if (t && t->wall == W_DOOR && (dx == 0 || dy == 0)) byDoor = true;
                    }
                if (byDoor) continue;
                bool ok = !(nearAltar || nearTable);
                for (int dx = -2; dx <= 2 && !ok; dx++)
                    for (int dy = -2; dy <= 2 && !ok; dy++) {
                        Tile *t = w.tileAt(X + dx, Y + dy, z);
                        if (!t) continue;
                        if (nearAltar && t->feature == F_ALTAR) ok = true;
                        if (nearTable && (t->feature == F_TABLE) && std::abs(dx) <= 1 && std::abs(dy) <= 1) ok = true;
                    }
                if (ok) cands.push_back({X, Y, z});
            }
        if (cands.empty()) return false;
        auto c = cands[rng.next() % cands.size()];
        gx = c[0]; gy = c[1]; gz = c[2];
        return true;
    };
    auto nearestBuilding = [&](std::function<bool(const Building &)> want) {
        int best = 0;
        float bd = 1e9f;
        // in their own town; failing that, anywhere within a day's walk (a farm's market is in town)
        for (int pass = 0; pass < 2 && !best; pass++)
            for (size_t i = 0; i < w.buildings.size(); i++) {
                const Building &b = w.buildings[i];
                if (!want(b)) continue;
                if (pass == 0 && (!s || !s->inside(b.x0, b.y0))) continue;
                float d = std::abs((b.x0 + b.x1) / 2 - p.x) + std::abs((b.y0 + b.y1) / 2 - p.y);
                if (pass == 1 && s)   // or from their own town (someone coming back from "away" has no place yet: the Alte Veste, on the ridge above Zirndorf)
                    d = std::min(d, (float)(std::abs((b.x0 + b.x1) / 2 - (s->x0 + s->w / 2)) + std::abs((b.y0 + b.y1) / 2 - (s->y0 + s->h / 2))));
                if (pass == 1 && d > 400) continue;
                d += rng.uni() * 30;
                if (d < bd) { bd = d; best = (int)i + 1; }
            }
        return best;
    };
    // a building by (part of) its name: one that is not a tower or a gate first, in the same town, so that
    // "Tre Kronor" is the castle and not the tower of Tre Kronor, and "Palazzo Vecchio" the palace and not
    // its tower (whose foot is outside: Geri Bocchineri and Princess Catherine stood in the street)
    auto namedBuilding = [&](const std::string &h) {
        auto named = [&](const Building &b) { return !b.name.empty() && lower(b.name).find(h) != std::string::npos; };
        // the name itself first: "the Alte Veste" is the ruin, not "a farm by the Alte Veste"
        if (int exact = nearestBuilding([&](const Building &b) { return !b.name.empty() && lower(b.name) == h; })) return exact;
        int any = nearestBuilding(named);
        int non = nearestBuilding([&](const Building &b) { return named(b) && b.kind != "tower" && b.kind != "gate"; });
        if (!non || !any) return any;
        const Building &ba = w.buildings[any - 1], &bn = w.buildings[non - 1];
        bool sameTown = !s || s->inside(ba.x0, ba.y0) == s->inside(bn.x0, bn.y0);
        return sameTown ? non : any;
    };
    auto tilesWhere = [&](int cx, int cy, int r, std::function<bool(int, int)> want) {
        // a few random tries first (cheap, and enough for common places), then every tile
        for (int k = 0; k < 60; k++) {
            int X = cx + rng.range(-r, r), Y = cy + rng.range(-r, r);
            if (!freeTile(w, X, Y, 0) || taken(X, Y, 0) || !want(X, Y)) continue;
            gx = X; gy = Y; gz = 0;
            return true;
        }
        std::vector<std::pair<int, int>> cands;
        for (int Y = cy - r; Y <= cy + r; Y++)
            for (int X = cx - r; X <= cx + r; X++)
                if (w.inWorld(X, Y) && want(X, Y) && freeTile(w, X, Y, 0) && !taken(X, Y, 0)) cands.push_back({X, Y});
        if (cands.empty()) return false;
        auto c = cands[rng.next() % cands.size()];
        gx = c.first; gy = c.second; gz = 0;
        return true;
    };
    std::string kind = place, arg;
    size_t sp = kind.find(' ');
    if (sp != std::string::npos) { arg = trim(kind.substr(sp + 1)); kind = kind.substr(0, sp); }
    int floorZ = 0;   // "landmark NAME upstairs": on its upper floor (Galileo in the Fiscal's rooms over the Holy Office's, April 1633)
    if (arg.size() > 9 && arg.compare(arg.size() - 9, 9, " upstairs") == 0) { arg = trim(arg.substr(0, arg.size() - 9)); floorZ = 1; }

    // "sleep PLACE", "eat PLACE": a bed nobody else has, or a place at table, in another building than
    // their home (Galileo held at the Holy Office; Morales hidden at Mona Lisabetta's), else anywhere in it
    if ((activity == "sleep" || activity == "eat") && !place.empty()) {
        int b = 0;
        if (kind == "landmark" || kind == "building") {
            std::string h = lower(arg);
            b = namedBuilding(h);
        } else if (kind == "home-of") {
            Person *q = byId(arg);
            if (!q && p.id.find(':') != std::string::npos) q = byId(p.id.substr(0, p.id.find(':') + 1) + arg);
            if (q) b = q->home;
        }
        if (b) {
            const Building &bd = w.buildings[b - 1];
            if (activity == "sleep")
                for (int pass = 0; pass < 2; pass++)   // on the floor asked for first
                    for (int z = 0; z <= 1; z++) {
                        if ((pass == 0) != (z == floorZ)) continue;
                        for (int X = bd.x0; X <= bd.x1; X++)
                            for (int Y = bd.y0; Y <= bd.y1; Y++) {
                                const Tile *t = w.tileAt(X, Y, z);
                                bool mine = (X == p.x && Y == p.y && z == p.z) || (X == p.goalX && Y == p.goalY && z == p.goalZ);   // lying in it already (after a load)
                                if (!t || t->feature != F_BED || (taken(X, Y, z) && !mine)) continue;
                                bool theirs = false;
                                for (auto &o : people) if (&o != &p && o.hasBed && o.bedX == X && o.bedY == Y && o.bedZ == z) theirs = true;
                                if (theirs) continue;
                                gx = X; gy = Y; gz = z;
                                return true;
                            }
                    }
            return inBuilding(b, floorZ, false, activity == "eat") || inBuilding(b, floorZ, false, false) || inBuilding(b, 0, false, false);
        }
    }

    if (activity == "sleep" && p.hasBed) { gx = p.bedX; gy = p.bedY; gz = p.bedZ; return true; }
    if ((activity == "home" || activity == "sleep") && p.home) return inBuilding(p.home, p.hasBed ? p.bedZ : 0, false, false) || inBuilding(p.home, 0, false, false);
    if (activity == "eat" && p.home) return inBuilding(p.home, 0, false, true) || inBuilding(p.home, 0, false, false);
    if (activity == "work" && p.hasWork) { gx = p.workX; gy = p.workY; gz = p.workZ; return true; }
    if (activity == "wander" || activity == "patrol" || ((activity == "work" || activity == "eat" || activity == "home") && !p.home)) {
        int cx = p.x, cy = p.y;
        if (activity == "patrol" && s) { cx = s->x0 + s->w / 2; cy = s->y0 + s->h / 2; }
        int r = activity == "patrol" ? (s ? std::max(s->w, s->h) / 2 : 40) : 22;
        return tilesWhere(cx, cy, r, [&](int X, int Y) {
            Tile &t = w.tile(X, Y);
            return t.bld == 0 && (t.terrain == T_STREET || t.terrain == T_SQUARE || t.terrain == T_ROAD || t.terrain == T_GRASS ||
                                  t.terrain == T_MEADOW || t.terrain == T_GARDEN || t.terrain == T_FIELD);
        });
    }
    if (activity != "go" && activity != "work" && activity != "lie") return false;
    // places
    if (kind == "inn") {
        int b = nearestBuilding([](const Building &b) { return b.kind == "inn"; });
        if (!b) return false;
        if (inBuilding(b, 0, false, true) || inBuilding(b, 0, false, false)) return true;
        // full: drinking in the street outside, near it but not at its doors
        const Building &bd = w.buildings[b - 1];
        int cx = (bd.x0 + bd.x1) / 2, cy = (bd.y0 + bd.y1) / 2, r = std::max(bd.x1 - bd.x0, bd.y1 - bd.y0) / 2 + 7;
        return tilesWhere(cx, cy, r, [&](int X, int Y) {
            const Tile &t = w.tile(X, Y);
            if (t.bld != 0) return false;
            if (X >= bd.x0 - 1 && X <= bd.x1 + 1 && Y >= bd.y0 - 1 && Y <= bd.y1 + 1) return false;   // not against its walls
            for (int dx = -3; dx <= 3; dx++)
                for (int dy = -3; dy <= 3; dy++)
                    if (w.tile(X + dx, Y + dy).wall == W_DOOR && w.tile(X + dx, Y + dy).bld == b) return false;
            return t.terrain == T_STREET || t.terrain == T_SQUARE || t.terrain == T_ROAD || t.terrain == T_GRASS || t.terrain == T_MEADOW;
        });
    }
    if (kind == "church" || kind == "altar") {
        int b = nearestBuilding([](const Building &b) { return b.kind == "church" || b.kind == "cathedral" || b.kind == "chapel"; });
        return b && (inBuilding(b, 0, kind == "altar", false) || inBuilding(b, 0, false, false));
    }
    if (kind == "college" || kind == "theatre" || kind == "fortress" || kind == "palace" || kind == "hall") {
        int b = nearestBuilding([&](const Building &b) { return b.kind == kind; });
        if (!b) return false;
        if (kind == "theatre") {
            const Building &bd = w.buildings[b - 1];
            for (int k = 0; k < 200; k++) {
                int X = rng.range(bd.x0, bd.x1), Y = rng.range(bd.y0, bd.y1);
                Tile &t = w.tile(X, Y);
                if (t.feature == F_STAGE && !taken(X, Y, 0)) { gx = X; gy = Y; gz = 0; return true; }
            }
        }
        return inBuilding(b, 0, false, false);
    }
    if (kind == "home-of") {   // "go home-of ID": in another's house (a man in hiding: Morales at Mona Lisabetta's)
        Person *q = byId(arg);
        if (!q && p.id.find(':') != std::string::npos) q = byId(p.id.substr(0, p.id.find(':') + 1) + arg);
        return q && q->home && inBuilding(q->home, 0, false, false);
    }
    if (kind == "landmark" || kind == "building") {
        std::string h = lower(arg);
        int b = namedBuilding(h);
        if (b && (w.buildings[b - 1].kind == "tower" || w.buildings[b - 1].kind == "gate")) {
            // a tower's inside is only the foot of its stair (Chiaramonti stood in the door of the
            // Leaning Tower): wait outside, near its foot, not against it
            const Building &bd = w.buildings[b - 1];
            int cx = (bd.x0 + bd.x1) / 2, cy = (bd.y0 + bd.y1) / 2, r = std::max(bd.x1 - bd.x0, bd.y1 - bd.y0) / 2 + 4;
            return tilesWhere(cx, cy, r, [&](int X, int Y) {
                if (w.tile(X, Y).bld != 0) return false;
                return !(X >= bd.x0 - 1 && X <= bd.x1 + 1 && Y >= bd.y0 - 1 && Y <= bd.y1 + 1);
            });
        }
        if (b && (inBuilding(b, floorZ, true, false) || inBuilding(b, floorZ, false, false) || inBuilding(b, 0, false, false))) return true;
        if (b) {   // no room inside (the Alte Veste, a ruin drawn solid, with only its gate): outside, under its walls
            const Building &bd = w.buildings[b - 1];
            int cx = (bd.x0 + bd.x1) / 2, cy = (bd.y0 + bd.y1) / 2, r = std::max(bd.x1 - bd.x0, bd.y1 - bd.y0) / 2 + 4;
            return tilesWhere(cx, cy, r, [&](int X, int Y) {
                if (w.tile(X, Y).bld != 0) return false;
                return !(X >= bd.x0 - 1 && X <= bd.x1 + 1 && Y >= bd.y0 - 1 && Y <= bd.y1 + 1);
            });
        }
        return false;
    }
    if (kind == "gate") {
        if (!s) return false;
        vec2 c(s->x0 + s->w * 0.5f, s->y0 + s->h * 0.5f);
        std::vector<std::array<int, 3>> cands;
        for (auto &b : w.buildings) {
            if (b.kind != "gate" || !s->inside(b.x0, b.y0)) continue;
            for (int X = b.x0 - 3; X <= b.x1 + 3; X++)
                for (int Y = b.y0 - 3; Y <= b.y1 + 3; Y++) {
                    Tile &t = w.tile(X, Y);
                    if (t.bld != 0 || !freeTile(w, X, Y, 0) || taken(X, Y, 0)) continue;
                    bool besideGate = false;
                    for (int dx = -1; dx <= 1; dx++)
                        for (int dy = -1; dy <= 1; dy++)
                            if (w.tile(X + dx, Y + dy).wall == W_GATE) besideGate = true;
                    if (besideGate && length(vec2((float)X, (float)Y) - c) < length(vec2((b.x0 + b.x1) * 0.5f, (b.y0 + b.y1) * 0.5f) - c))
                        cands.push_back({X, Y, 0});
                }
        }
        if (cands.empty()) return false;
        auto cc = cands[rng.next() % cands.size()];
        gx = cc[0]; gy = cc[1]; gz = 0;
        return true;
    }
    if (kind == "bridge") {
        // "work bridge LANDMARK": on the bridge nearest that building (the changers of Paris, on the
        // Pont au Change by the Grand Châtelet)
        std::string h = lower(arg);
        int b = namedBuilding(h);
        if (!b) return false;
        const Building &bd = w.buildings[b - 1];
        auto dist = [&](int X, int Y) { return std::max(std::max(bd.x0 - X, X - bd.x1), std::max(bd.y0 - Y, Y - bd.y1)); };
        int best = 1 << 30;
        std::vector<std::pair<int, int>> cands;
        for (int Y = bd.y0 - 24; Y <= bd.y1 + 24; Y++)
            for (int X = bd.x0 - 24; X <= bd.x1 + 24; X++)
                if (w.inWorld(X, Y) && w.tile(X, Y).terrain == T_BRIDGE && w.tile(X, Y).bld == 0 && freeTile(w, X, Y, 0) && !taken(X, Y, 0)) {
                    int d = dist(X, Y);
                    if (d < best - 4) cands.clear();
                    if (d <= best + 4) cands.push_back({X, Y});
                    best = std::min(best, d);
                }
        cands.erase(std::remove_if(cands.begin(), cands.end(), [&](const std::pair<int, int> &c) { return dist(c.first, c.second) > best + 4; }), cands.end());
        if (cands.empty()) return false;
        auto c = cands[rng.next() % cands.size()];
        gx = c.first; gy = c.second; gz = 0;
        return true;
    }
    if (kind == "market" || kind == "square" || kind == "quay" || kind == "stall") {
        if (!s) return false;
        auto want = [&](int X, int Y) {
            Tile &t = w.tile(X, Y);
            if (t.bld != 0) return false;
            if (kind == "square") return t.terrain == T_SQUARE;
            if (kind == "quay") {
                for (int dx = -1; dx <= 1; dx++)
                    for (int dy = -1; dy <= 1; dy++)
                        if (w.tile(X + dx, Y + dy).terrain == T_WATER) return t.terrain == T_STREET || t.terrain == T_SQUARE;
                return false;
            }
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++)
                    if (w.tile(X + dx, Y + dy).feature == F_STALL) return true;
            return false;
        };
        // "near BUILDING" with it: the stretch of quay or square nearest that building, so that what people say of them
        // ("on the quays, near the German church") is where they are
        if (!p.nearHint.empty())
            if (int b = namedBuilding(lower(p.nearHint))) {
                const Building &bd = w.buildings[b - 1];
                auto dist = [&](int X, int Y) { return std::max(std::max(bd.x0 - X, X - bd.x1), std::max(bd.y0 - Y, Y - bd.y1)); };
                int best = 1 << 30;
                std::vector<std::pair<int, int>> cands;
                for (int Y = bd.y0 - 60; Y <= bd.y1 + 60; Y++)
                    for (int X = bd.x0 - 60; X <= bd.x1 + 60; X++) {
                        if (!w.inWorld(X, Y) || !s->inside(X, Y) || !want(X, Y) || !freeTile(w, X, Y, 0) || taken(X, Y, 0)) continue;
                        int d = dist(X, Y);
                        if (d > best + 3) continue;
                        if (d < best - 3) cands.clear();
                        cands.push_back({X, Y});
                        best = std::min(best, d);
                    }
                cands.erase(std::remove_if(cands.begin(), cands.end(), [&](const std::pair<int, int> &c) { return dist(c.first, c.second) > best + 3; }), cands.end());
                if (!cands.empty()) {
                    auto c = cands[rng.next() % cands.size()];
                    gx = c.first; gy = c.second; gz = 0;
                    return true;
                }
            }
        return tilesWhere(s->x0 + s->w / 2, s->y0 + s->h / 2, std::max(s->w, s->h) / 2, want);
    }
    if (kind == "mine" || kind == "tunnels") {
        // a free spot in the underground workings nearest to their town
        for (const Level &l : w.levels) {
            if (!l.underground || l.w < 30 || !s) continue;
            if (l.x0 > s->x0 + s->w + 80 || l.x0 + l.w < s->x0 - 80 || l.y0 > s->y0 + s->h + 80 || l.y0 + l.h < s->y0 - 80) continue;
            for (int k = 0; k < 400; k++) {
                int X = rng.range(l.x0, l.x0 + l.w - 1), Y = rng.range(l.y0, l.y0 + l.h - 1);
                if (!freeTile(w, X, Y, l.z) || taken(X, Y, l.z)) continue;
                gx = X; gy = Y; gz = l.z;
                return true;
            }
        }
        return false;
    }
    if (kind == "camp") {   // beside a tent, a gabion or a gun of the town's camp (pitched or not yet)
        if (!s || s->campSpots.empty()) return false;
        for (int k = 0; k < 40; k++) {
            const auto &c = s->campSpots[rng.next() % s->campSpots.size()];
            int X = c.x + rng.range(-1, 1), Y = c.y + rng.range(-1, 1);
            if ((X == c.x && Y == c.y) || !freeTile(w, X, Y, 0) || taken(X, Y, 0)) continue;
            bool onSpot = false;
            for (auto &o : s->campSpots) if (o.x == X && o.y == Y) onSpot = true;
            if (onSpot) continue;
            gx = X; gy = Y; gz = 0;
            return true;
        }
        return false;
    }
    if (kind == "earthwork") {   // on the inner side of a rampart of earth (the King of Sweden's lines)
        if (!s) return false;
        int cx = s->x0 + s->w / 2, cy = s->y0 + s->h / 2;
        return tilesWhere(cx, cy, std::max(s->w, s->h) / 2, [&](int X, int Y) {
            if (!s->inside(X, Y)) return false;
            static const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto &d : d4)
                if (w.tile(X + d[0], Y + d[1]).wall == W_EARTHWORK &&
                    std::hypot((float)(X - cx), (float)(Y - cy)) < std::hypot((float)(X + d[0] - cx), (float)(Y + d[1] - cy)))
                    return true;
            return false;
        });
    }
    if (kind == "yard") {   // in the yard or the kitchen garden of their own farm (a farmer's wife, with the hens)
        if (!s) return false;
        return tilesWhere(s->x0 + s->w / 2, s->y0 + s->h / 2, std::max(s->w, s->h) / 2, [&](int X, int Y) {
            const Tile &t = w.tile(X, Y);
            return s->inside(X, Y) && t.bld == 0 && (t.terrain == T_YARD || t.terrain == T_GARDEN);
        });
    }
    if (kind == "field" || kind == "woods" || kind == "road") {
        if (!s) return false;
        int cx = s->x0 + s->w / 2, cy = s->y0 + s->h / 2;
        int hx, hy;
        if (!p.nearHint.empty() && std::sscanf(p.nearHint.c_str(), "%d,%d", &hx, &hy) == 2) { cx = hx; cy = hy; }
        int r = std::max(s->w, s->h) / 2 + (s->hamlet ? 14 : 40);   // a farm's own fields are close about it
        return tilesWhere(cx, cy, r, [&](int X, int Y) {
            if (s->inside(X, Y)) return false;
            Tile &t = w.tile(X, Y);
            if (kind == "field") return t.terrain == T_FIELD || t.terrain == T_MEADOW;
            if (kind == "road") return t.terrain == T_ROAD;
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++) {
                    int f = w.tile(X + dx, Y + dy).feature;
                    if (f == F_OAK || f == F_PINE) return true;
                }
            return false;
        });
    }
    return false;
}

// ------------------------------------------------------------------ the day

void People::plan(World &w, Person &p, double minutes) {
    // which entry of the schedule holds now? Each day a person is a little early or late
    // (up to 25 minutes, the same for the whole day, so the order of their day is kept),
    // except for entries marked "sharp".
    int now = (int)std::fmod(minutes, 1440.0);
    int day = (int)std::floor(minutes / 1440.0);
    static const bool punctual = std::getenv("GALILEO_PUNCTUAL") != nullptr;   // for tests: everyone on the minute
    int late = punctual ? 0 : (int)std::lround(50 * hash01((int)std::hash<std::string>()(p.id), day, 11) - 25);
    // The entry that holds depends only on the game minute, on whether they lie down, and on the memories the
    // conditions read: until one of those changes, the answer of the last time stands.
    PlanCache &pc = p.planCache;
    const int64_t minuteKey = (int64_t)std::floor(minutes);
    int best;
    if (pc.valid && pc.lying == p.lying && (pc.coarse ? pc.minute == minuteKey : pc.at == minutes) && planCacheHolds(p, pc)) {
        best = pc.best;
    } else {
        pc.deps.clear();
        pc.coarse = true;
        best = -1;
        int bestT = INT32_MIN, latest = -1, latestT = INT32_MIN;
        for (size_t i = 0; i < p.schedule.size(); i++) {
            const SchedEntry &e = p.schedule[i];
            if (e.always) continue;   // weighed after the timetable
            if (!e.cond.empty() && !holdsRec(p, e.cond, minutes, &pc)) continue;
            int t = e.minute + (e.sharp ? 0 : late);
            if (t <= now && t >= bestT) { best = (int)i; bestT = t; }
            if (t >= latestT) { latest = (int)i; latestT = t; }
        }
        if (best < 0) best = latest;   // before the first entry of the day: still last night's
        for (size_t i = 0; i < p.schedule.size(); i++)   // "always ... if CONDS": over the timetable, while they hold
            if (p.schedule[i].always && holdsRec(p, p.schedule[i].cond, minutes, &pc)) best = (int)i;
        pc.valid = true; pc.minute = minuteKey; pc.at = minutes; pc.lying = p.lying; pc.best = best;
    }
    if (best == p.actIndex && p.placed) return;
    p.actIndex = best;
    p.activity = best >= 0 ? p.schedule[best].activity : "wander";
    p.actPlace = best >= 0 ? p.schedule[best].place : "";
    // "away": not here at all (an army not yet come); coming back, they march in along a road
    bool wasAway = p.away;
    p.away = p.activity == "away";
    if (p.away) { p.path.clear(); p.pathPos = 0; p.target = -1; p.fleeT = 0; p.lying = false; p.placed = true; return; }
    if (wasAway) { p.placed = false; p.recruit = true; }
    if (p.activity == "travel") {
        if (p.actPlace != p.town && p.travelTo.empty()) p.travelTo = p.actPlace;
        p.activity = "wander";
    }
    if (!p.travelTo.empty()) {   // on the way out of town: to the gate
        p.activity = "go";
        p.actPlace = "gate";
    }
    int gx, gy, gz;
    if (!placeFor(w, p, p.activity, p.actPlace, gx, gy, gz) && !placeFor(w, p, "wander", "", gx, gy, gz)) {
        p.path.clear();
        p.placed = true;
        return;
    }
    p.goalX = gx; p.goalY = gy; p.goalZ = gz;
    if (p.activity == "lie") {   // "lie PLACE": on the ground there, as if fallen (a wounded Scot under the Alte Veste); just there, not walking in
        int ox = p.x, oy = p.y, oz = p.z;
        p.x = p.px = gx; p.y = p.py = gy; p.z = p.pz = gz;
        p.path.clear(); p.pathPos = 0;
        p.recruit = false; p.placed = true; p.lying = true;
        placedAt(p, ox, oy, oz);
        return;
    }
    if (p.lying) {   // get out of bed first
        p.lying = false;
        const int d8[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
        for (auto &d : d8)
            if (freeTile(w, p.x + d[0], p.y + d[1], p.z)) { p.x += d[0]; p.y += d[1]; break; }
        p.px = p.x; p.py = p.y; p.pz = p.z;
    }
    const Tile *goalT = w.tileAt(gx, gy, gz);
    bool toBed = p.activity == "sleep" && goalT && goalT->feature == F_BED;   // their own, or one elsewhere ("sleep PLACE")
    int tx = gx, ty = gy;
    if (toBed) {   // walk to the side of the bed
        const int d8[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
        for (auto &d : d8)
            if (freeTile(w, gx + d[0], gy + d[1], gz)) { tx = gx + d[0]; ty = gy + d[1]; break; }
    }
    if (!p.placed && p.recruit) {   // a recruit, or an army arriving: they come along a road, and walk
        p.recruit = false;
        p.placed = true;
        if (!arriveFromRoad(w, p, gx, gy)) { p.x = p.px = tx; p.y = p.py = ty; p.z = p.pz = gz; p.path.clear(); return; }
    } else if (!p.placed) {
        // the first time: just be there (and the tile is theirs at once, or the next one placed in
        // the same moment may be put on it too: pairs at the Pomme de Pin)
        int ox = p.x, oy = p.y, oz = p.z;
        p.x = p.px = tx; p.y = p.py = ty; p.z = p.pz = gz;
        p.path.clear();
        if (toBed) { p.x = p.px = gx; p.y = p.py = gy; p.lying = true; }
        p.placed = true;
        placedAt(p, ox, oy, oz);
        return;
    }
    if (!findPath(w, p.x, p.y, p.z, tx, ty, gz, p.path)) {
        int ox = p.x, oy = p.y, oz = p.z;
        p.path.clear();
        p.x = p.px = tx; p.y = p.py = ty; p.z = p.pz = gz;   // no way there: let them be there
        placedAt(p, ox, oy, oz);
    }
    if (toBed) p.path.push_back({gx, gy, gz});
    p.pathPos = 0;
    if (!p.path.empty()) goalTiles.insert(World::key3(gx, gy, gz));
}

// somewhere on a road 30 to 50 tiles from (gx,gy), to march in from
bool People::arriveFromRoad(World &w, Person &p, int gx, int gy) {
    uint32_t h = hash32((uint32_t)std::hash<std::string>()(p.id) + (uint32_t)lastMinutes);
    for (int r = 30; r <= 50; r += 4)
        for (int k = 0; k < 64; k++) {
            float a = 6.2832f * ((h % 64) + k) / 64.0f;
            int X = gx + (int)std::lround(r * std::cos(a)), Y = gy + (int)std::lround(r * std::sin(a));
            if (!w.inWorld(X, Y) || w.tile(X, Y).terrain != T_ROAD || !freeTile(w, X, Y, 0) || occupied(X, Y, 0)) continue;
            p.x = p.px = X; p.y = p.py = Y; p.z = p.pz = 0;
            p.path.clear(); p.pathPos = 0;
            return true;
        }
    return false;
}

// the camps of the towns with "camp EVENT [until EVENT]": up once the first has happened, struck
// once the second has (World::setFeature on each of their tents, gabions and guns)
void People::pitchCamps(World &w) {
    for (auto &s : w.settlements) {
        if (s.campEvent.empty()) continue;
        bool want = player.count("event:" + s.campEvent) && !player.count("failed:" + s.campEvent) &&
                    !(!s.campUntil.empty() && player.count("event:" + s.campUntil));
        if (want == s.campUp) continue;
        s.campUp = want;
        for (auto &c : s.campSpots) w.setFeature(c.x, c.y, 0, want ? c.feature : (uint8_t)F_NONE, c.var);
    }
}

bool People::occupied(int X, int Y, int Z) const { return occ.count(World::key3(X, Y, Z)) > 0; }

void People::placedAt(Person &p, int oldX, int oldY, int oldZ) {
    int i = (int)(&p - &people[0]);
    int *at = occ.find(World::key3(oldX, oldY, oldZ));
    if (at && *at == i) occ.erase(World::key3(oldX, oldY, oldZ));
    occ[World::key3(p.x, p.y, p.z)] = i;
}

std::string People::squeezePast(int youX, int youY, int toX, int toY, int z) {
    for (auto &p : people) {
        if (p.x != toX || p.y != toY || p.z != z || p.onRoad || !p.active) continue;
        if (p.lying || p.dead || p.target != -1 || p.chase > 0) return "";
        p.px = p.x; p.py = p.y; p.pz = p.z;
        p.x = youX; p.y = youY;
        p.stepT = 0;                       // they step aside into your place, and go on from there
        p.path.clear(); p.pathPos = 0;
        {   // and say so
            static const char *said[] = {"Excuse me!", "Pardon!", "By your leave.", "Mind your feet!", "Excuse me!", "Mind where you go!"};
            static unsigned turn = 0;
            p.bubble = said[(std::hash<std::string>()(p.id) + turn++) % 6]; p.bubbleT = 2.0f;
        }
        occ.erase(World::key3(toX, toY, z));
        occ[World::key3(youX, youY, z)] = (int)(&p - &people[0]);
        return label(p);
    }
    return "";
}

void People::update(World &w, float dt, double minutes, int ptx, int pty, int ptz) {
    lastMinutes = minutes;
    occ.clear();
    occ[World::key3(ptx, pty, ptz)] = -1;
    for (size_t i = 0; i < people.size(); i++) {
        Person &p = people[i];
        if (!p.dead && p.mem.count("dead-at")) { p.dead = true; p.deadAt = num(p.mem["dead-at"]); p.lying = true; p.hp = 0; }   // after loading a game
        if (p.dead && minutes - p.deadAt >= p.recruitMin) {   // a day later (or the role's "recruits") a recruit stands in his place
            p.dead = false; p.lying = false; p.hp = (float)p.maxHp; p.foe = false; p.target = -1;
            p.mem.erase("dead-at"); p.placed = false; p.recruit = true; p.actIndex = -2; p.path.clear();
        }
        if (!p.dead && !p.away) occ[World::key3(p.x, p.y, p.z)] = (int)i;
    }
    goalTiles.clear();
    for (const Person &p : people)
        if (p.active && !p.dead && !p.onRoad && p.pathPos < p.path.size()) goalTiles.insert(World::key3(p.goalX, p.goalY, p.goalZ));
    // a conversation between two people: one line after another
    if (chatNow.chat >= 0) {
        chatNow.t += dt;
        const Person &a = people[chatNow.a], &b = people[chatNow.b];
        if (!a.active || !b.active) endChat(minutes);
        else if (chatNow.t >= chatNow.dur) {
            chatNow.line++;
            chatNow.t = 0;
            if (chatNow.line >= chats[chatNow.chat].lines.size()) endChat(minutes);
            else chatLine(w, minutes, ptx, pty, ptz);
        }
    }
    chatClock += dt;
    if (chatClock >= 1.0f) {   // once a second: might two people near you start talking?
        chatClock = 0;
        if (chatNow.chat < 0) tryChat(w, minutes, ptx, pty, ptz);
        wantsStep(minutes, ptx, pty, ptz);
    }
    eventClock += dt;
    if (eventClock >= 0.5f) { eventClock = 0; runEvents(w, minutes, ptx, pty); noteJournal(minutes); }
    gunsStep(w, minutes, ptx, pty, ptz);
    auto fol = player.find("follower");
    const std::string followId = fol == player.end() ? std::string() : fol->second;
    for (size_t i = 0; i < people.size(); i++) {
        Person &p = people[i];
        if (!followId.empty() && p.id == followId && !p.dead) {   // walking behind you ({follow}): no day of his own; Game::followerStep moves him
            p.active = true; p.placed = true; p.away = false; p.lying = false; p.onRoad = false; p.travelTo.clear();
            if (talk.who == (int)i) continue;
            if (p.stepT < 1) {
                p.stepT = std::min(1.0f, p.stepT + dt / p.stepDur);
                p.walkPhase += dt / (p.stepDur * 2);
            } else p.walkPhase = 0;
            if (p.bubbleT > 0) p.bubbleT -= dt;
            continue;
        }
        if (p.wild) {   // the road's people: only fighting, running, or a step under way (Game::wildStep moves them)
            if (p.away) { p.active = false; continue; }
            p.active = true; p.placed = true;
            if (p.dead) {
                if (minutes - p.deadAt > 120) { p.away = true; p.dead = false; p.lying = false; p.hp = (float)p.maxHp; p.mem.erase("dead-at"); }
                continue;
            }
            if (talk.who == (int)i) continue;
            if (p.chase > 0) { chaseStep(w, p, (int)i, dt, ptx, pty, ptz); continue; }
            if (fightStep(w, p, (int)i, dt, ptx, pty, ptz, minutes)) continue;
            if (p.stepT < 1) {
                p.stepT = std::min(1.0f, p.stepT + dt / p.stepDur);
                p.walkPhase += dt / (p.stepDur * 2);
            } else p.walkPhase = 0;
            if (p.bubbleT > 0) p.bubbleT -= dt;
            continue;
        }
        // only the people near the player live from moment to moment; the others catch up
        bool near = !p.onRoad && std::abs(p.x - ptx) + std::abs(p.y - pty) < 260;
        if (!near) {
            p.active = false;
            if (!p.travelTo.empty() || p.onRoad) travelStep(w, p, minutes);   // the far-away travel too
            continue;
        }
        if (!p.active) { p.active = true; p.placed = false; p.actIndex = -2; }
        if (p.dead) continue;   // lying where he fell
        if (p.away) { plan(w, p, minutes); if (p.away) continue; }   // not here (yet)
        if (talk.who == (int)i || inChat((int)i)) continue;   // standing still while spoken to, or talking
        if (p.chase > 0) { chaseStep(w, p, (int)i, dt, ptx, pty, ptz); continue; }   // after a thief
        if (p.fighter && p.placed) { plan(w, p, minutes); if (p.away) continue; }   // a soldier's orders come first (an army leaving)
        if (p.placed && fightStep(w, p, (int)i, dt, ptx, pty, ptz, minutes)) continue;   // fighting, or running away (once at their post)
        if (p.hold > 0) {   // staying put (playing a piece)
            p.hold -= dt;
            if (p.bubbleT > 0) p.bubbleT -= dt;
            continue;
        }
        if (!p.travelTo.empty() || p.onRoad) { travelStep(w, p, minutes); if (p.onRoad) continue; }
        plan(w, p, minutes);
        if (p.bubbleT > 0) p.bubbleT -= dt;
        if (p.stepT < 1) {
            bool diag = p.x != p.px && p.y != p.py;   // diagonal steps are longer, so slower
            p.stepT = std::min(1.0f, p.stepT + dt / (p.stepDur * (diag ? 1.4f : 1.0f)));
            p.walkPhase += dt / (p.stepDur * 2);
            continue;
        }
        p.walkPhase = 0;
        // someone who comes up beside them makes them stop for a few moments
        bool beside = p.z == ptz && std::abs(p.x - ptx) <= 1 && std::abs(p.y - pty) <= 1;
        if (!beside) p.pause = 0;
        else if (p.pause < 6.0f && p.pathPos < p.path.size()) {
            p.pause += dt;
            p.facing = std::atan2(-(float)(ptx - p.x), (float)(pty - p.y));
            continue;
        }
        if (p.pathPos < p.path.size()) {
            auto nx = p.path[p.pathPos];
            const Tile *nxT = w.tileAt(nx[0], nx[1], nx[2]);
            bool bedStep = p.activity == "sleep" && p.pathPos + 1 == p.path.size() && nxT && nxT->feature == F_BED &&
                           nx[0] == p.goalX && nx[1] == p.goalY && nx[2] == p.goalZ;
            const int *it = occ.find(World::key3(nx[0], nx[1], nx[2]));
            if (it && *it != (int)i && !bedStep) {
                p.wait += dt;
                int b = *it;
                if (b == -1 && p.wait > 0.8f && p.bubbleT <= 0) { p.bubble = "Excuse me."; p.bubbleT = 2.0f; }
                // someone standing still (you, or a person stopped or talking) is soon walked
                // round; someone walking slowly ahead is overtaken after a little longer
                bool standing = b < 0 || talk.who == b || inChat(b) || people[b].pause > 0 || people[b].pathPos >= people[b].path.size();
                if (p.wait > (standing ? 0.8f : 1.2f)) {   // go round (or overtake)
                    p.wait = 0;
                    std::vector<std::array<int, 3>> alt;
                    avoid = World::key3(nx[0], nx[1], nx[2]);
                    bool found = findPath(w, p.x, p.y, p.z, p.goalX, p.goalY, p.goalZ, alt, 20000);
                    avoid = INT64_MIN;
                    if (found && !alt.empty()) {
                        p.path = alt;
                        p.pathPos = 0;
                    } else if (b >= 0 && b != talk.who && !inChat(b)) {   // never jostle people talking
                        // swap places with the other walker, as people do in a narrow lane
                        Person &o = people[b];
                        std::swap(o.x, p.x); std::swap(o.y, p.y); std::swap(o.z, p.z);
                        o.px = o.x; o.py = o.y; o.pz = o.z; p.px = p.x; p.py = p.y; p.pz = p.z;
                        p.pathPos++;
                    }
                }
                continue;
            }
            p.wait = 0;
            occ.erase(World::key3(p.x, p.y, p.z));
            p.px = p.x; p.py = p.y; p.pz = p.z;
            bool climb = nx[2] != p.z || std::abs(nx[0] - p.x) > 1 || std::abs(nx[1] - p.y) > 1;
            if (!climb) p.facing = std::atan2(-(float)(nx[0] - p.x), (float)(nx[1] - p.y));
            p.x = nx[0]; p.y = nx[1]; p.z = nx[2];
            occ[World::key3(p.x, p.y, p.z)] = (int)i;
            p.pathPos++;
            p.stepT = climb ? 1.0f : 0.0f;
            if (climb) { p.px = p.x; p.py = p.y; p.pz = p.z; }
            if (bedStep) { p.lying = true; p.stepT = 1; p.px = p.x; p.py = p.y; }
            continue;
        }
        // arrived
        if (p.activity == "wander" || p.activity == "patrol") {
            p.idle += dt;
            if (p.idle > 8 + 20 * hash01((int)i, (int)minutes / 10, 5)) {
                p.idle = 0;
                p.placed = true;
                p.actIndex = -3;   // choose a new spot
            }
        }
        // mutter (but not over a conversation)
        if (chatNow.chat < 0 && !p.lying && !p.says.empty() && std::abs(p.x - ptx) + std::abs(p.y - pty) < 14 && p.z == ptz) {
            p.nextSay -= dt;
            if (p.nextSay <= 0) {
                p.bubble = p.says;
                p.bubbleT = 3.0f;
                p.nextSay = 20 + 40 * hash01((int)i, (int)minutes, 6);
            }
        }
    }
}

// ------------------------------------------------------------------ things that happen by themselves

static long numOf(const Memory &m, const std::string &k);
static void addTo(Memory &m, const std::string &k, long d);

Person *People::byId(const std::string &id) {
    for (auto &p : people) if (p.id == id) return &p;
    return nullptr;
}

static int daysIn(int y, int m) {
    static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : d[m - 1];
}
// minutes from the start of the game (Tuesday 15 June 1632, midnight) to YYYY-MM-DD HH:MM
static long minutesOf(const std::string &date, const std::string &time);
long People::dateMinutes(const std::string &date, const std::string &time) { return minutesOf(date, time); }
static long minutesOf(const std::string &date, const std::string &time) {
    int y = 1632, m = 6, d = 15, hh = 0, mm = 0;
    std::sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d);
    if (!time.empty()) std::sscanf(time.c_str(), "%d:%d", &hh, &mm);
    long days = 0;
    int cy = 1632, cm = 6, cd = 15;
    while ((cy < y || (cy == y && (cm < m || (cm == m && cd < d)))) && days < 100000) {
        days++;
        if (++cd > daysIn(cy, cm)) { cd = 1; if (++cm > 12) { cm = 1; cy++; } }
    }
    return days * 1440 + hh * 60 + mm;
}

// the day DAY (0: Tuesday 15 June 1632) in words: "the 17th of June"
static std::string dayWords(long day) {
    static const char *MON[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    int y = 1632, m = 6, d = 15;
    for (long k = 0; k < day && k < 100000; k++)
        if (++d > daysIn(y, m)) { d = 1; if (++m > 12) { m = 1; y++; } }
    const char *suf = (d % 100 >= 11 && d % 100 <= 13) ? "th" : d % 10 == 1 ? "st" : d % 10 == 2 ? "nd" : d % 10 == 3 ? "rd" : "th";
    return "the " + std::to_string(d) + suf + " of " + MON[m - 1];
}

// event ID / on DATE [TIME] / if WHO CONDITION / effects... / end
// the fish days: Fridays (15 June 1632, the first day, was a Tuesday), and Lent, from Ash
// Wednesday to Holy Saturday, by the Gregorian Easter (Rome's; England's was ten days off)
bool fridayAt(double minutes) { return ((long)std::floor(minutes / 1440.0) + 2) % 7 == 5; }
bool lentAt(double minutes) {
    for (int y = 1632; y <= 1645; y++) {
        int a = y % 19, b = y / 100, c = y % 100, d = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
        int h = (19 * a + b - d - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
        int m = (a + 11 * h + 22 * l) / 451, month = (h + l - 7 * m + 114) / 31, day = ((h + l - 7 * m + 114) % 31) + 1;
        char buf[16];
        std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", y, month, day);
        double easter = (double)minutesOf(buf, "00:00");
        if (minutes >= easter - 46 * 1440.0 && minutes < easter) return true;
    }
    return false;
}

bool People::loadEvents(const std::string &fn) {
    std::ifstream in(fn);
    if (!in) return false;
    std::string line;
    Event *cur = nullptr;
    Cannonade *guns = nullptr;
    Musketry *musk = nullptr;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        std::istringstream ss(t);
        std::string w;
        ss >> w;
        if (w == "musketry") {   // musketry TOWN [every MINUTES] [volleys MINUTES] / if CONDS / end
            musketries.push_back(Musketry());
            musk = &musketries.back();
            cur = nullptr; guns = nullptr;
            ss >> musk->town;
            for (char &ch : musk->town) if (ch == '_') ch = ' ';
            std::string k; float v;
            while (ss >> k >> v) { if (k == "every") musk->every = v; else if (k == "volleys") musk->volleys = v; }
            continue;
        }
        if (musk) {
            if (w == "end") { musk = nullptr; continue; }
            if (w == "if") {
                std::string rest = trim(t.substr(2));
                size_t pos;
                while ((pos = rest.find(" and ")) != std::string::npos) { musk->conds.push_back(trim(rest.substr(0, pos))); rest = rest.substr(pos + 5); }
                musk->conds.push_back(trim(rest));
            }
            continue;
        }
        if (w == "guns") {   // guns TOWN [every MINUTES] [damage D] [range R] / if CONDS / end
            cannonades.push_back(Cannonade());
            guns = &cannonades.back();
            cur = nullptr; musk = nullptr;
            ss >> guns->town;
            for (char &ch : guns->town) if (ch == '_') ch = ' ';
            std::string k; float v;
            while (ss >> k >> v) { if (k == "every") guns->every = v; else if (k == "damage") guns->damage = v; else if (k == "range") guns->range = v; }
            continue;
        }
        if (guns) {
            if (w == "end") { guns = nullptr; continue; }
            if (w == "if") {
                std::string rest = trim(t.substr(2));
                size_t pos;
                while ((pos = rest.find(" and ")) != std::string::npos) { guns->conds.push_back(trim(rest.substr(0, pos))); rest = rest.substr(pos + 5); }
                guns->conds.push_back(trim(rest));
            }
            continue;
        }
        if (w == "event") { events.push_back(Event()); cur = &events.back(); ss >> cur->id; continue; }
        if (!cur) continue;
        if (w == "end") { cur = nullptr; continue; }
        if (w == "on") { std::string d, h; ss >> d >> h; cur->at = minutesOf(d, h); continue; }
        if (w == "chance") { ss >> cur->chance; continue; }
        if (w == "if") {   // atoms joined by "and", each beginning with whose: town:id or player
            std::string rest = trim(t.substr(2)), c;
            size_t pos;
            while ((pos = rest.find(" and ")) != std::string::npos) { cur->conds.push_back(trim(rest.substr(0, pos))); rest = rest.substr(pos + 5); }
            cur->conds.push_back(trim(rest));
            continue;
        }
        cur->effects.push_back(t);
    }
    // check the names
    for (auto &e : events) {
        auto check = [&](const std::string &who) {
            if (who == "player" || (who.size() > 2 && who.substr(who.size() - 2) == ":*")) return;
            if (!byId(who)) std::fprintf(stderr, "events.txt: %s: no person called %s\n", e.id.c_str(), who.c_str());
        };
        for (auto &c : e.conds) { std::istringstream cs(c); std::string who; cs >> who; if (who == "not") cs >> who; check(who); }
        for (auto &f : e.effects) {
            std::istringstream fs(f); std::string verb, a, b; fs >> verb >> a >> b;
            if (verb == "news" || verb == "wants" || verb == "makes" || verb == "forbid" || verb == "allow" || verb == "jump" || verb == "ending" || verb == "card" ||
                verb == "pass" || verb == "wake" || verb == "volley" || verb == "cue" || verb == "view") continue;
            if (verb == "carry") {   // carry X Y [beside WHO]
                std::string w4, who2; fs >> w4 >> who2;
                if (w4 == "beside") check(who2);
                continue;
            }
            check(a);
            if (verb == "buy" || verb == "give") check(b);
        }
    }
    return true;
}

// data/ending.txt: "title TEXT"; "page TITLE" begins a page; "line if CONDITIONS: TEXT" (or
// "line: TEXT") is shown on it if the conditions hold, as in events.txt
bool People::loadEnding(const std::string &fn) {
    std::ifstream in(fn);
    if (!in) return false;
    std::string line, page;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        if (t.rfind("title ", 0) == 0) { endingTitle = trim(t.substr(6)); continue; }
        if (t.rfind("page ", 0) == 0) { page = trim(t.substr(5)); continue; }
        if (t.rfind("line", 0) != 0) continue;
        size_t colon = t.find(": ");   // not the first ':', which may be in a condition ("player keeper:christina")
        if (colon == std::string::npos) colon = t.find(':');
        if (colon == std::string::npos) continue;
        EndingLine L;
        L.page = page;
        L.text = trim(t.substr(colon + 1));
        std::string head = trim(t.substr(4, colon - 4));
        if (head.rfind("if ", 0) == 0) {
            std::string rest = trim(head.substr(3));
            size_t pos;
            while ((pos = rest.find(" and ")) != std::string::npos) { L.conds.push_back(trim(rest.substr(0, pos))); rest = rest.substr(pos + 5); }
            L.conds.push_back(trim(rest));
        }
        ending.push_back(L);
    }
    return true;
}

// conditions as in events.txt: each begins with whose ("player" or town:id)
bool People::condsHold(const std::vector<std::string> &conds, double minutes) const {
    if (people.empty()) return false;
    for (std::string c : conds) {
        // "not WHO KEY" and "WHO not KEY" both mean that WHO does not remember KEY
        bool neg = false;
        if (c.rfind("not ", 0) == 0) { neg = true; c = trim(c.substr(4)); }
        std::string who = c.substr(0, c.find(' ')), rest = c.find(' ') == std::string::npos ? "" : trim(c.substr(c.find(' ') + 1));
        if (neg) rest = "not " + rest;
        bool ok;
        if (who == "player") ok = holds(people[0], rest.rfind("not ", 0) == 0 ? "not player " + trim(rest.substr(4)) : "player " + rest, minutes);
        else { const Person *p = nullptr; for (auto &q : people) if (q.id == who) { p = &q; break; } ok = p && holds(*p, rest, minutes); }
        if (!ok) return false;
    }
    return true;
}

// data/stories.txt:  story ID / title TEXT / step if CONDS: TEXT / done if CONDS: TEXT / end
bool People::loadStories(const std::string &fn) {
    std::ifstream in(fn);
    if (!in) return false;
    std::string line;
    Story *cur = nullptr;
    auto conds = [&](const std::string &s) {   // "if A and B" (or nothing)
        std::vector<std::string> out;
        std::string rest = trim(s);
        if (rest.rfind("if ", 0) != 0) return out;
        rest = trim(rest.substr(3));
        size_t pos;
        while ((pos = rest.find(" and ")) != std::string::npos) { out.push_back(trim(rest.substr(0, pos))); rest = rest.substr(pos + 5); }
        if (!rest.empty()) out.push_back(trim(rest));
        return out;
    };
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        std::string w = t.substr(0, t.find(' ')), rest = t.find(' ') == std::string::npos ? "" : trim(t.substr(t.find(' ') + 1));
        if (w == "story") { stories.push_back(Story()); cur = &stories.back(); cur->id = rest; continue; }
        if (!cur) continue;
        if (w == "end") { cur = nullptr; continue; }
        if (w == "title") { cur->title = rest; continue; }
        if (w == "main") { cur->main = true; continue; }
        if (w == "top") { cur->top = true; continue; }
        if (w == "step" || w == "done") {
            StoryStep st;
            if (rest.rfind("well ", 0) == 0) { st.mark = 1; rest = trim(rest.substr(5)); }
            else if (rest.rfind("badly ", 0) == 0) { st.mark = -1; rest = trim(rest.substr(6)); }
            size_t colon = rest.find(": ");
            std::string head = colon == std::string::npos ? "" : rest.substr(0, colon);
            st.text = colon == std::string::npos ? rest : trim(rest.substr(colon + 2));
            st.conds = conds(head);
            // the key: the conditions, hashed (FNV-1a), so that editing a step's words keeps its date
            uint32_t h = 2166136261u;
            for (char c : std::string(w == "done" ? "done|" : "") + head) { h ^= (unsigned char)c; h *= 16777619u; }
            char buf[16];
            std::snprintf(buf, sizeof buf, "%08x", h);
            st.key = buf;
            auto &list = w == "step" ? cur->steps : cur->dones;
            int same = 0;
            for (auto &o : list) if (o.key.substr(0, 8) == st.key) same++;
            if (same) st.key += "-" + std::to_string(same);
            list.push_back(st);
        }
    }
    for (auto &st : stories) {   // the names in the conditions must be people
        auto check = [&](const std::vector<std::string> &cs) {
            for (auto &c : cs) {
                std::string cc = c.rfind("not ", 0) == 0 ? trim(c.substr(4)) : c;
                std::string who = cc.substr(0, cc.find(' '));
                if (who != "player" && !byId(who)) std::fprintf(stderr, "stories.txt: %s: no person called %s\n", st.id.c_str(), who.c_str());
            }
        };
        for (auto &s : st.steps) check(s.conds);
        for (auto &s : st.dones) check(s.conds);
    }
    return true;
}

std::vector<std::pair<int, std::string>> People::journal(double minutes, bool detailed) const {
    // current: a step that goes once it is done ("not player ch1-over": "her father begs you to come
    // to Arcetri"), as against a settled fact ("Brożek keeps one at Cracow")
    struct Shown { std::string text; int mark; double at; size_t index; bool current; };
    struct Entry { const Story *st; std::vector<Shown> steps; const StoryStep *done; double at; size_t order; };
    auto firstShown = [&](const Story &st, const StoryStep &s) {   // when it was first shown (now, if never noted)
        auto it = player.find("jnl:" + st.id + ":" + s.key);
        return it == player.end() ? minutes : num(it->second);
    };
    std::vector<Entry> entries;
    for (const Story &st : stories) {
        Entry e{&st, {}, nullptr, -1e18, entries.size()};
        for (size_t k = 0; k < st.steps.size(); k++) {
            const StoryStep &s = st.steps[k];
            if (!condsHold(s.conds, minutes)) continue;
            std::string t = s.text;   // $[KEY]: what you remember under KEY ("the post house at $[post:to]")
            for (size_t a = t.find("$["); a != std::string::npos; a = t.find("$[", a)) {
                size_t b = t.find(']', a);
                if (b == std::string::npos) break;
                std::string k = t.substr(a + 2, b - a - 2), v;
                if (k.rfind("date:", 0) == 0) {   // $[date:KEY+N]: the day N days after the time KEY holds, "the 17th of June"
                    size_t plus = k.find('+');
                    auto it = player.find(k.substr(5, plus == std::string::npos ? std::string::npos : plus - 5));
                    long n = plus == std::string::npos ? 0 : std::atol(k.c_str() + plus + 1);
                    v = it == player.end() ? std::string("a day to come") : dayWords((long)std::floor(num(it->second) / 1440.0) + n);
                } else {
                    auto it = player.find(k);
                    v = it == player.end() ? std::string("somewhere") : it->second;
                }
                t.replace(a, b - a + 1, v);
                a += v.size();
            }
            double at = firstShown(st, s);
            bool current = false;
            for (auto &c : s.conds) if (c.rfind("not ", 0) == 0) current = true;
            e.steps.push_back({t, s.mark, at, k, current});
            e.at = std::max(e.at, at);
        }
        for (const StoryStep &d : st.dones)
            if (condsHold(d.conds, minutes)) { e.done = &d; e.at = std::max(e.at, firstShown(st, d)); break; }
        if (e.steps.empty() && !e.done) continue;
        entries.push_back(e);
    }
    if (detailed) {
        // the open stories, then the finished (which begin a page of their own: hud.cc); in each, the
        // main story on top; then the newest first, and on a tie (noted together,
        // or a game saved before the journal kept dates) the later in the file first, as the story runs
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
            auto rank = [](const Entry &e) { return e.st->top ? -1 : (e.done ? 2 : 0) + (e.st->main ? 0 : 1); };
            if (rank(a) != rank(b)) return rank(a) < rank(b);
            if (a.at != b.at) return a.at > b.at;
            return a.order > b.order;
        });
        // in each story the newest step first (ties: the later step first); in an open story, what is
        // still to be done before what is settled (so that a pending call, such as Maria Celeste's back to
        // Arcetri, is not buried under later steps)
        for (auto &e : entries) {
            if (e.st->top) continue;   // the end of the story reads in its own order
            bool open = !e.done;
            std::stable_sort(e.steps.begin(), e.steps.end(), [open](const Shown &a, const Shown &b) {
                if (open && a.current != b.current) return a.current;
                return a.at != b.at ? a.at > b.at : a.index > b.index;
            });
        }
    } else {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
            if (a.st->top != b.st->top) return a.st->top;
            return !a.done && b.done;
        });
    }
    auto withMark = [&](int kind, int mark) { return kind + (detailed ? (mark > 0 ? 10 : mark < 0 ? 20 : 0) : 0); };
    std::vector<std::pair<int, std::string>> out;
    for (auto &e : entries) {
        out.push_back({e.st->top ? (detailed ? 30 : 0) : withMark(e.done ? 3 : 0, e.done ? e.done->mark : 0), e.st->title});   // mark 3: no "(open)"
        if (detailed && e.done) out.push_back({withMark(2, e.done->mark), e.done->text});
        // a finished story shows how it ended and what was done well or badly, not the plain steps that led there
        // (not "waiting for the Holy Office to call him" after the sentence)
        for (auto &s : e.steps) if (!e.done || s.mark != 0) out.push_back({withMark(1, s.mark), s.text});
        if (!detailed && e.done) out.push_back({2, e.done->text});
    }
    return out;
}

void People::noteJournal(double minutes, bool force) {
    if (people.empty()) return;
    if (!force && minutes >= journalNoted && minutes - journalNoted < 5) return;
    journalNoted = minutes;
    std::string now = std::to_string((long)minutes);
    for (const Story &st : stories) {
        for (const StoryStep &s : st.steps) {
            std::string k = "jnl:" + st.id + ":" + s.key;
            if (!player.count(k) && condsHold(s.conds, minutes)) player[k] = now;
        }
        for (const StoryStep &d : st.dones) {
            if (!condsHold(d.conds, minutes)) continue;
            std::string k = "jnl:" + st.id + ":" + d.key;
            if (!player.count(k)) player[k] = now;
            break;
        }
    }
}

void People::advanceStep(World &w, double minutes, int ptx, int pty) {
    lastMinutes = minutes;
    for (auto &p : people) {   // those on a journey: as for the far-away in update (they set off at once)
        if (p.wild || (p.travelTo.empty() && !p.onRoad)) continue;
        if (!p.onRoad) p.active = false;
        travelStep(w, p, minutes);
    }
    runEvents(w, minutes, ptx, pty);
    noteJournal(minutes);
}

double People::nextEventTime(double minutes) const {
    double best = -1;
    for (auto &e : events) {
        if (e.at < 0 || e.at <= minutes || player.count("event:" + e.id)) continue;
        if (best < 0 || e.at < best) best = (double)e.at;
    }
    return best;
}

static float frand();   // (the fight's dice, below: the volley of an event)

// the guns of a camp at work (events.txt "guns"): now and then one of them fires, on the nearest of the King of Sweden's
// men in front of it within its range (hurting whoever stands where the shot falls), or, with nobody so near, at the
// King's lines far off; a boom and a puff of smoke each time (Game::takeRequests)
void People::gunsStep(World &w, double minutes, int ptx, int pty, int ptz) {
    for (auto &cn : cannonades) {
        if (cn.next >= 0 && minutes < cn.next) continue;
        if (cn.next < 0 || minutes - cn.next > 30) { cn.next = minutes + cn.every * (0.3f + 0.7f * frand()); continue; }   // (begun, or after a long wait)
        cn.next = minutes + cn.every * (0.6f + 0.8f * frand());
        if (!condsHold(cn.conds, minutes)) continue;
        const Settlement *s = nullptr;
        for (auto &st : w.settlements) if (lower(st.name) == lower(cn.town)) { s = &st; break; }
        if (!s || !s->campUp) continue;
        std::vector<const Settlement::CampSpot *> gunsHere;
        for (auto &c : s->campSpots) if (c.feature == F_CANNON) gunsHere.push_back(&c);
        if (gunsHere.empty()) continue;
        const Settlement::CampSpot &g = *gunsHere[(size_t)(frand() * gunsHere.size()) % gunsHere.size()];
        static const int dir[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};   // var: it points west, east, south, north
        int fx = dir[g.var & 3][0], fy = dir[g.var & 3][1];
        auto inFront = [&](int X, int Y, int &d) {
            int dx = X - g.x, dy = Y - g.y;
            d = std::max(std::abs(dx), std::abs(dy));
            return d >= 2 && d <= cn.range && dx * fx + dy * fy > 0;
        };
        int tx = 0, ty = 0, bd = 1 << 30, d;
        auto side = player.find("side");
        if (side != player.end() && side->second == "swedish" && !playerDown && ptz == 0 && inFront(ptx, pty, d) && d < bd) { bd = d; tx = ptx; ty = pty; }
        for (auto &o : people)
            if (!o.dead && !o.away && o.active && o.z == 0 && !o.lying && o.faction == "swedish" && inFront(o.x, o.y, d) && d < bd) { bd = d; tx = o.x; ty = o.y; }
        gunSmoke.push_back({g.x + 0.5f + 0.6f * fx, g.y + 0.5f + 0.6f * fy});
        if (bd == (1 << 30)) continue;   // nobody near in front of it: a shot at the King's lines, far off
        int lx = tx + (int)std::lround((frand() - 0.5f) * 2), ly = ty + (int)std::lround((frand() - 0.5f) * 2);   // the shot falls about there
        for (size_t i = 0; i < people.size(); i++) {
            Person &o = people[i];
            if (o.dead || o.away || o.named() || o.faction != "swedish" || o.z != 0 || std::max(std::abs(o.x - lx), std::abs(o.y - ly)) > 1) continue;
            hurt((int)i, cn.damage / 2 + frand() * cn.damage, -1, minutes);
        }
        if (side != player.end() && side->second == "swedish" && !playerDown && ptz == 0 && std::max(std::abs(ptx - lx), std::abs(pty - ly)) <= 1) {
            int dmg = (int)(cn.damage / 2 + frand() * (cn.damage / 2 + 1));
            playerHurt += (float)dmg;
            fightNotes.push_back("A shot from the gabions strikes you (" + std::to_string(dmg) + ").");
        }
    }
    // the musketry: from a soldier of the town with a musket (or its middle, if none is about)
    for (auto &mk : musketries) {
        bool shot = mk.nextShot >= 0 && minutes >= mk.nextShot, vol = mk.nextVolley >= 0 && minutes >= mk.nextVolley;
        if (mk.nextShot < 0 || minutes - mk.nextShot > 30) mk.nextShot = minutes + mk.every * frand();
        if (mk.nextVolley < 0 || minutes - mk.nextVolley > 30) mk.nextVolley = minutes + mk.volleys * (0.2f + 0.6f * frand());
        if (!shot && !vol) continue;
        if (shot) mk.nextShot = minutes + mk.every * (0.3f + 1.4f * frand());
        if (vol) mk.nextVolley = minutes + mk.volleys * (0.6f + 0.8f * frand());
        if (!condsHold(mk.conds, minutes)) continue;
        const Settlement *s = nullptr;
        for (auto &st : w.settlements) if (lower(st.name) == lower(mk.town)) { s = &st; break; }
        if (!s) continue;
        std::vector<const Person *> men;
        for (auto &o : people)
            if (!o.dead && !o.away && o.active && o.fighter && !o.lying && o.look.weapon == 1 && o.z == 0 && s->inside(o.x, o.y)) men.push_back(&o);
        float x = s->x0 + s->w / 2.0f, y = s->y0 + s->h / 2.0f;
        if (!men.empty()) { const Person *o = men[(size_t)(frand() * men.size()) % men.size()]; x = o->x + 0.5f; y = o->y + 0.5f; }
        soundsAt.push_back({vol ? "volley" : "musket", x, y});
    }
}
void People::runEvents(World &w, double minutes, int ptx, int pty) {
    if (events.empty() || people.empty()) return;
    for (auto &p : people) {   // "title if CONDS: TEXT": the title the story gives them now
        if (p.titleIf.empty()) continue;
        std::string t = p.baseTitle;
        for (auto &ti : p.titleIf) if (holds(p, ti.first, minutes)) { t = ti.second; break; }
        p.title = t;
    }
    pitchCamps(w);
    std::string here;
    for (auto &s : w.settlements) if (s.inside(ptx, pty)) { here = s.name; break; }
    for (auto &e : events) {
        std::string key = "event:" + e.id;
        if (player.count(key)) continue;
        if (e.at >= 0 && minutes < e.at) continue;
        if (!condsHold(e.conds, minutes)) continue;
        // a beat (jump): its "now" is the date it moves to, and nothing else happens in this pass,
        // so that the game has moved the calendar before any other event is weighed
        double nowM = minutes;
        bool beat = false;
        for (const std::string &f : e.effects) {
            std::istringstream js(f);
            std::string v, d, h;
            js >> v >> d >> h;
            if (v == "jump") { nowM = std::max(minutes, (double)minutesOf(d, h)); beat = true; }
        }
        player[key] = std::to_string((long)nowM);
        if (e.chance < 1) {   // a venture: it comes off, or it never will
            static std::mt19937 rng((unsigned)std::random_device{}());
            if (std::uniform_real_distribution<float>(0, 1)(rng) >= e.chance) { player["failed:" + e.id] = std::to_string((long)minutes); continue; }
        }
        marketEffects(e);
        bool told = false, wake = false;   // a news line told to you; "wake": then it ends a wait (Game::advanceClock)
        for (const std::string &f : e.effects) {
            std::istringstream fs(f);
            std::string verb, a, b, c;
            fs >> verb >> a;
            auto each = [&](const std::string &who, std::function<void(Memory &, Person *)> fn) {
                if (who == "player") { fn(player, nullptr); return; }
                if (who.size() > 2 && who.substr(who.size() - 2) == ":*") {   // everyone in a town
                    std::string town = who.substr(0, who.size() - 2);
                    for (auto &p : people) if (lower(p.town) == lower(town)) fn(p.mem, &p);
                    return;
                }
                if (Person *p = byId(who)) fn(p->mem, p);
            };
            if (verb == "stock") {   // stock SHOPKEEPER ITEM N
                fs >> b >> c;
                each(a, [&](Memory &m, Person *) { addTo(m, "stock:" + b, c.empty() ? 1 : std::atol(c.c_str())); });
            } else if (verb == "buy") {   // buy WHO SHOPKEEPER ITEM
                fs >> b >> c;
                Person *buyer = byId(a), *shop = byId(b);
                int k = items ? items->find(c) : -1;
                if (buyer && shop && k >= 0 && numOf(shop->mem, "stock:" + c) > 0) {
                    long price = items->kinds[k].price;
                    addTo(shop->mem, "stock:" + c, -1);
                    addTo(shop->mem, "money", price);
                    addTo(buyer->mem, "money", -std::min(price, numOf(buyer->mem, "money")));
                    addTo(buyer->mem, "has:" + c, 1);
                    buyer->mem.erase("wants:" + c);
                    buyer->mem["bought:" + c] = std::to_string((long)minutes);
                }
            } else if (verb == "give") {   // give FROM TO ITEM
                fs >> b >> c;
                Person *from = byId(a), *to = byId(b);
                if (from && to && numOf(from->mem, "has:" + c) > 0) { addTo(from->mem, "has:" + c, -1); addTo(to->mem, "has:" + c, 1); }
            } else if (verb == "set" || verb == "unset" || verb == "add") {   // set WHO KEY[=VALUE]
                fs >> b >> c;
                each(a, [&](Memory &m, Person *) {
                    if (verb == "unset") { m.erase(b); return; }
                    if (verb == "add") { addTo(m, b, c.empty() ? 1 : std::atol(c.c_str())); return; }
                    size_t eq = b.find('=');
                    if (eq == std::string::npos) m[b] = "1";
                    else m[b.substr(0, eq)] = b.substr(eq + 1) == "now" ? std::to_string((long)nowM) : b.substr(eq + 1);
                });
            } else if (verb == "travel") {   // travel WHO TOWN
                std::string town; std::getline(fs, town);
                if (Person *p = byId(a)) p->travelTo = trim(town);
            } else if (verb == "jump") {   // jump YYYY-MM-DD HH:MM [BOOK]: the months pass (a beat of the plot)
                fs >> b >> c;
                jumpTo = minutesOf(a, b);
                jumpPage = c;
            } else if (verb == "card") {   // card ID: a card of data/cards.txt, shown with (before) a jump's letter or scenes
                cardRequest = a;
            } else if (verb == "ending") {
                endingRequest = true;
            } else if (verb == "news") {   // news TOWN|anywhere TEXT: told you if you are there
                std::string text; std::getline(fs, text);
                text = trim(text);
                // "news except:TOWN TEXT": anywhere but there (where it was seen, and told otherwise)
                bool except = lower(a).rfind("except:", 0) == 0;
                if (except ? lower(a.substr(7)) != lower(here) : (lower(a) == "anywhere" || lower(a) == lower(here))) {
                    notice = notice.empty() ? text : notice + " " + text;   // two at once (the forgery laid, and Niccolini's letter): both told
                    heard.push_back({(int)heard.size() + 100000, minutes, here.empty() ? "on the road" : here, "news", text});
                    told = true;
                }
            } else if (verb == "cue") {   // cue TOWN|anywhere ID [LEVEL]: music (data/music.txt "cue"), heard if you are there
                fs >> b >> c;
                if (lower(a) == "anywhere" || lower(a) == lower(here)) { cueRequest = b; cueLevel = c.empty() ? 1.0f : (float)std::atof(c.c_str()); }
            } else if (verb == "view") {   // view YAW: the camera turned to look from that side (Game::takeRequests)
                viewYaw = (float)std::atof(a.c_str());
            } else if (verb == "wake") {   // wake: its news, told to you, ends a wait or a sleep (the drums of the muster)
                wake = true;
            } else if (verb == "pass") {   // pass MINUTES, or pass until HH:MM (today): the clock moves on (the march, in the dark)
                if (a == "until") {
                    fs >> b;
                    int hh = 0, mm = 0;
                    std::sscanf(b.c_str(), "%d:%d", &hh, &mm);
                    double t = std::floor(minutes / 1440.0) * 1440 + hh * 60 + mm;
                    if (t > minutes) eventPass += t - minutes;
                } else eventPass += std::atof(a.c_str());
            } else if (verb == "carry") {   // carry X Y [beside WHO]: you are taken to the world tile X,Y (Game::takeRequests)
                std::string w3, who2;
                fs >> b >> w3 >> who2;
                carryX = std::atoi(a.c_str()); carryY = std::atoi(b.c_str());
                carryBeside = w3 == "beside" ? who2 : "";
            } else if (verb == "kill") {   // kill WHO: he falls dead where he stands (Murdo Ross, on the roads of the lie)
                if (Person *p = byId(a)) if (!p->dead) die(*p, minutes);
            } else if (verb == "volley") {   // volley BUILDING R DAMAGE: guns fire from a building ("the-alte-veste"), and hurt
                // the King of Sweden's men within R tiles of it, and you if you stand there (named people are the story's)
                fs >> b >> c;
                int R = std::atoi(b.c_str()), dmg = std::atoi(c.c_str());
                const Building *B = nullptr;
                for (const Building &bb : w.buildings) {
                    std::string k = lower(bb.name);
                    for (char &ch : k) if (ch == ' ') ch = '-';
                    if (k == a) { B = &bb; break; }
                }
                if (!B) { std::fprintf(stderr, "events.txt: %s: volley: no building %s\n", e.id.c_str(), a.c_str()); continue; }
                auto within = [&](int X, int Y) {
                    int dx = X < B->x0 ? B->x0 - X : X > B->x1 ? X - B->x1 : 0, dy = Y < B->y0 ? B->y0 - Y : Y > B->y1 ? Y - B->y1 : 0;
                    return std::max(dx, dy) <= R;
                };
                for (size_t i = 0; i < people.size(); i++) {
                    Person &o = people[i];
                    if (o.dead || o.away || o.named() || o.faction != "swedish" || o.z != 0 || !within(o.x, o.y)) continue;
                    hurt((int)i, (float)(dmg / 2 + (int)(frand() * (dmg + 1))), -1, minutes);
                }
                // the smoke over the walls, where the guns stood under the brushwood (a puff of
                // smoke when a cannon fires): along each side, a little outside it
                for (int k = 0; k < 3; k++) {
                    float f = (k + 0.5f) / 3.0f;
                    gunSmoke.push_back({B->x0 + f * (B->x1 - B->x0 + 1), (float)B->y0 - 0.5f});
                    gunSmoke.push_back({B->x0 + f * (B->x1 - B->x0 + 1), (float)B->y1 + 1.5f});
                    gunSmoke.push_back({(float)B->x0 - 0.5f, B->y0 + f * (B->y1 - B->y0 + 1)});
                    gunSmoke.push_back({(float)B->x1 + 1.5f, B->y0 + f * (B->y1 - B->y0 + 1)});
                }
                if (!playerDown && within(ptx, pty)) {
                    int d = dmg / 2 + (int)(frand() * (dmg / 2 + 1));
                    playerHurt += (float)d;
                    fightNotes.push_back("Shot from the ruin strikes you (" + std::to_string(d) + ").");
                }
            }
        }
        if (wake && told) wakeRequest = true;
        if (beat) return;   // the calendar moves before anything else happens
    }
}

// an event's changes to a market (a siege: "wants Nuremberg rye 3.0"); made again when a saved
// game is loaded, for every event that has happened in it
void People::marketEffects(const Event &e) {
    for (const std::string &f : e.effects) {
        std::istringstream fs(f);
        std::string verb, town, item;
        fs >> verb >> town >> item;
        for (char &c : town) if (c == '_') c = ' ';
        if (verb == "forbid" || verb == "allow") {   // forbid TOWN ITEM | WHY: seized, searched for both ways
            Market &m = markets[lower(town)];
            if (verb == "allow") { m.forbids.erase(item); m.searchOut.erase(item); m.whyOf.erase(item); continue; }
            m.forbids.insert(item);
            m.searchOut.insert(item);
            size_t bar = f.find('|');
            if (bar != std::string::npos) m.whyOf[item] = trim(f.substr(bar + 1));
            for (auto &p : people)   // the shops of the town have none left
                if (lower(p.town) == lower(town) && p.mem.count("stock:" + item)) p.mem["stock:" + item] = "0";
            continue;
        }
        float factor = 1;
        fs >> factor;
        if (verb != "wants" && verb != "makes") continue;
        Market &m = markets[lower(town)];
        if (factor == 1) (verb == "wants" ? m.wants : m.makes).erase(item);   // back to the plain price
        else (verb == "wants" ? m.wants : m.makes)[item] = factor;
    }
}

// people who want a thing buy it when they stand near a shopkeeper who has it
void People::wantsStep(double minutes, int ptx, int pty, int ptz) {
    for (auto &p : people) {
        if (!p.active || p.lying) continue;
        for (auto it = p.mem.begin(); it != p.mem.end(); ++it) {
            if (it->first.rfind("wants:", 0) != 0) continue;
            std::string id = it->first.substr(6);
            for (auto &q : people) {
                if (&q == &p || !q.active || q.z != p.z || std::max(std::abs(q.x - p.x), std::abs(q.y - p.y)) > 4) continue;
                if (numOf(q.mem, "stock:" + id) <= 0) continue;
                int k = items ? items->find(id) : -1;
                long price = k >= 0 ? items->kinds[k].price : 0;
                addTo(q.mem, "stock:" + id, -1);
                addTo(q.mem, "money", price);
                addTo(p.mem, "money", -std::min(price, numOf(p.mem, "money")));
                addTo(p.mem, "has:" + id, 1);
                p.mem["bought:" + id] = std::to_string((long)minutes);
                p.bubble = "At last! I will take it."; p.bubbleT = 3.5f;
                q.bubble = "Sold!"; q.bubbleT = 2.5f;
                if (std::abs(p.x - ptx) + std::abs(p.y - pty) < 16 && p.z == ptz && k >= 0)
                    notice = p.name + " buys " + items->kinds[k].name + ".";
                p.mem.erase(it);
                return;   // one purchase at a time
            }
        }
    }
}

// ------------------------------------------------------------------ the watch, after a thief

bool People::startChase(int i) {
    if (i < 0 || i >= (int)people.size()) return false;
    Person &p = people[i];
    if (p.chase > 0) return false;
    if (talk.who == i) talk.over = true;
    p.chase = 0.001f;
    p.chaseSteps = 99;   // a path at once
    p.hold = 0; p.pause = 0;
    p.lying = false;
    p.bubble = "Stop, thief!";
    p.bubbleT = 2.5f;
    return true;
}

void People::stopChases() {
    for (auto &p : people)
        if (p.chase > 0) { p.chase = 0; p.actIndex = -2; p.stepDur = p.strollDur; }
}

bool People::chasing() const {
    for (auto &p : people) if (p.chase > 0) return true;
    return false;
}

// running after the player: a path to where you are, made again every few steps; the
// chase ends when they reach you (caught) or lose you (too far, or too long)
void People::chaseStep(World &w, Person &p, int i, float dt, int ptx, int pty, int ptz) {
    p.chase += dt;
    if (p.bubbleT > 0) p.bubbleT -= dt;
    if (p.stepT < 1) {
        bool diag = p.x != p.px && p.y != p.py;
        p.stepT = std::min(1.0f, p.stepT + dt / (p.stepDur * (diag ? 1.4f : 1.0f)));
        p.walkPhase += dt / (p.stepDur * 2);
        return;
    }
    p.walkPhase = 0;
    int d = std::max(std::abs(p.x - ptx), std::abs(p.y - pty));
    auto stop = [&]() { p.chase = 0; p.path.clear(); p.pathPos = 0; p.actIndex = -2; p.stepDur = p.strollDur; };
    if (p.z == ptz && d <= 1) {
        p.facing = std::atan2(-(float)(ptx - p.x), (float)(pty - p.y));
        stop();
        caught = i;
        return;
    }
    if (p.chase > 150 || d > 45) { stop(); lostBy = i; return; }
    p.stepDur = 0.24f;   // running
    if (p.pathPos >= p.path.size() || p.chaseSteps >= 4) {
        p.chaseSteps = 0;
        std::vector<std::array<int, 3>> np;
        if (findPath(w, p.x, p.y, p.z, ptx, pty, ptz, np, 20000)) { p.path = np; p.pathPos = 0; }
        else p.path.clear();
    }
    if (p.pathPos >= p.path.size()) return;   // no way to you just now
    auto nx = p.path[p.pathPos];
    if (nx[0] == ptx && nx[1] == pty && nx[2] == ptz) return;   // beside you: caught next time
    const int *it = occ.find(World::key3(nx[0], nx[1], nx[2]));
    if (it && *it != i) { p.chaseSteps = 99; return; }   // someone in the way
    occ.erase(World::key3(p.x, p.y, p.z));
    bool climb = nx[2] != p.z || std::abs(nx[0] - p.x) > 1 || std::abs(nx[1] - p.y) > 1;
    p.px = p.x; p.py = p.y; p.pz = p.z;
    if (!climb) p.facing = std::atan2(-(float)(nx[0] - p.x), (float)(nx[1] - p.y));
    p.x = nx[0]; p.y = nx[1]; p.z = nx[2];
    occ[World::key3(p.x, p.y, p.z)] = i;
    p.pathPos++;
    p.chaseSteps++;
    p.stepT = climb ? 1.0f : 0.0f;
    if (climb) { p.px = p.x; p.py = p.y; p.pz = p.z; }
}

// ------------------------------------------------------------------ fighting

static std::mt19937 &fightRng() { static std::mt19937 r((unsigned)std::random_device{}()); return r; }
static float frand() { return std::uniform_real_distribution<float>(0, 1)(fightRng()); }

// a blow: hit or miss by the two skills; the damage, less the armour (0: it glances off); -1 a miss
int People::roll(int attSkill, int dmg, int defSkill, int defArmour) {
    float chance = std::max(0.15f, std::min(0.9f, 0.55f + 0.08f * (attSkill - defSkill)));
    if (frand() >= chance) return -1;
    int d = 1 + (int)(frand() * std::max(1, dmg)) + attSkill / 2 - defArmour;
    return std::max(0, d);
}

void People::die(Person &p, double minutes) {
    p.dead = true; p.deadAt = minutes; p.lying = true; p.hp = 0;
    p.target = -1; p.fleeT = 0; p.path.clear(); p.pathPos = 0; p.stepT = 1;
    p.mem["dead-at"] = std::to_string((long)minutes);
    p.bubbleT = 0;
    occ.erase(World::key3(p.x, p.y, p.z));
    int me = (int)(&p - &people[0]);
    for (auto &q : people)   // those who fought him go back to their posts (or find another enemy)
        if (q.target == me) { q.target = -1; q.path.clear(); q.pathPos = 0; q.actIndex = -2; q.stepDur = q.strollDur; q.scanT = 1.0f; }
}

// a person wounded: soldiers fight back (and may break and run when badly hurt); anyone else
// runs; an anonymous person brought to nothing dies, a named one yields and runs
void People::hurt(int victim, float dmg, int by, double minutes) {
    if (victim < 0 || victim >= (int)people.size()) return;
    Person &o = people[victim];
    if (o.dead) return;
    if (by == -2) { o.foe = true; if (talk.who == victim) talk.over = true; }
    o.hp -= dmg;
    if (o.hp <= 0) {
        if (o.named()) {
            o.hp = 1; o.target = -1; o.fleeT = 25;
            o.bubble = "Enough! Enough!"; o.bubbleT = 3;
            if (by == -2) { std::string l = label(o); l[0] = (char)std::toupper((unsigned char)l[0]); fightNotes.push_back(l + " yields, and runs."); }
            return;
        }
        die(o, minutes);
        if (by == -2) { std::string l = label(o); l[0] = (char)std::toupper((unsigned char)l[0]); fightNotes.push_back(l + " falls, and does not get up."); }
        return;
    }
    if (!o.fighter) {   // not a soldier: run
        o.fleeT = 20; o.target = by;
        o.bubble = by == -2 ? "Help! Murder!" : "Help!"; o.bubbleT = 2.5f;
        return;
    }
    if (o.target != by) { o.target = by; o.path.clear(); o.pathPos = 0; o.chaseSteps = 99; }   // he turns on whoever wounds him
    if (o.hp < 0.25f * o.maxHp && frand() < 0.4f) {     // and some break and run
        o.fleeT = 15;
        o.bubble = "Quarter! Quarter!"; o.bubbleT = 2.5f;
    }
}

// the nearest one this person will attack: you (if "player" is among their enemies, or you
// attacked them), or a soldier of a side they are at war with; -1 nobody
int People::findFoe(const Person &p, int i, int ptx, int pty, int ptz) const {
    if (p.lying) return -1;   // a wounded man lying where he fell looks for nobody (Murdo Ross under the Alte Veste)
    int best = -1, bd = 9;
    bool hatesPlayer = p.foe;
    auto side = player.find("side");   // "player side=swedish": that day you are one of them (the storming of the Alte Veste)
    for (auto &h : p.hostile) if (h == "player" || (side != player.end() && h == side->second)) hatesPlayer = true;
    if (hatesPlayer && !playerDown && p.z == ptz) {
        int d = std::max(std::abs(p.x - ptx), std::abs(p.y - pty));
        if (d < bd) { bd = d; best = -2; }
    }
    if (p.hostile.empty()) return best;
    auto fol = player.find("follower");
    const std::string *followId = fol == player.end() ? nullptr : &fol->second;
    if (best < 0) bd = 17;   // soldiers see enemy soldiers further off than they notice you
    for (size_t j = 0; j < people.size(); j++) {
        const Person &o = people[j];
        if ((int)j == i || o.dead || o.away || !o.active || o.onRoad || o.z != p.z || o.faction.empty()) continue;
        if (o.lying || (followId && o.id == *followId)) continue;   // the fallen, and the wounded man who leans on you, are let be
        bool enemy = false;
        for (auto &h : p.hostile) if (h == o.faction) enemy = true;
        if (!enemy) continue;
        int d = std::max(std::abs(p.x - o.x), std::abs(p.y - o.y));
        if (d < bd) { bd = d; best = (int)j; }
    }
    return best;
}

// one step towards (gx,gy,gz), at a run; a path made again every few steps
void People::stepToward(World &w, Person &p, int i, int gx, int gy, int gz, float dur) {
    p.stepDur = dur;
    if (p.pathPos >= p.path.size() || p.chaseSteps >= 3) {
        p.chaseSteps = 0;
        std::vector<std::array<int, 3>> np;
        if (findPath(w, p.x, p.y, p.z, gx, gy, gz, np, 8000)) { p.path = np; p.pathPos = 0; }
        else p.path.clear();
    }
    if (p.pathPos >= p.path.size()) return;
    auto nx = p.path[p.pathPos];
    if (nx[0] == gx && nx[1] == gy && nx[2] == gz) return;   // that is where the enemy stands
    const int *it = occ.find(World::key3(nx[0], nx[1], nx[2]));
    if (it && *it != i) { p.chaseSteps = 99; return; }
    occ.erase(World::key3(p.x, p.y, p.z));
    bool climb = nx[2] != p.z || std::abs(nx[0] - p.x) > 1 || std::abs(nx[1] - p.y) > 1;
    p.px = p.x; p.py = p.y; p.pz = p.z;
    if (!climb) p.facing = std::atan2(-(float)(nx[0] - p.x), (float)(nx[1] - p.y));
    p.x = nx[0]; p.y = nx[1]; p.z = nx[2];
    occ[World::key3(p.x, p.y, p.z)] = i;
    p.pathPos++;
    p.chaseSteps++;
    p.stepT = climb ? 1.0f : 0.0f;
    if (climb) { p.px = p.x; p.py = p.y; p.pz = p.z; }
}

// fighting or running away; false when there is nothing of the kind to do
bool People::fightStep(World &w, Person &p, int i, float dt, int ptx, int pty, int ptz, double minutes) {
    if (p.strikeAnim > 0) p.strikeAnim = std::max(0.0f, p.strikeAnim - dt);
    bool busy = p.fleeT > 0 || (p.target != -1 && p.fighter);
    if (!busy && p.hp < p.maxHp) p.hp = std::min((float)p.maxHp, p.hp + dt * 0.025f);   // wounds heal: a point in twenty game minutes
    if (!p.fighter && p.fleeT <= 0) { p.target = -1; return false; }   // not a soldier: they only run
    if (!busy && !p.fighter) return false;
    if (p.stepT < 1 && busy) {   // a step under way
        bool diag = p.x != p.px && p.y != p.py;
        p.stepT = std::min(1.0f, p.stepT + dt / (p.stepDur * (diag ? 1.4f : 1.0f)));
        p.walkPhase += dt / (p.stepDur * 2);
        if (p.bubbleT > 0) p.bubbleT -= dt;
        return true;
    }
    if (p.bubbleT > 0) p.bubbleT -= dt;
    // where the enemy is
    auto where = [&](int t, int &x, int &y, int &z) {
        if (t == -2) { x = ptx; y = pty; z = ptz; return !playerDown; }
        if (t < 0 || t >= (int)people.size()) return false;
        const Person &o = people[t];
        x = o.x; y = o.y; z = o.z;
        return !o.dead && o.active && !o.onRoad;
    };
    if (p.fleeT > 0) {   // away from whoever hurt them, a step at a time
        p.fleeT -= dt;
        int ex = p.x, ey = p.y, ez = p.z;
        if (!where(p.target, ex, ey, ez)) { ex = p.x; ey = p.y; }
        int bx = 0, by = 0, bd = std::max(std::abs(p.x - ex), std::abs(p.y - ey));
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = p.x + dx, ny = p.y + dy;
                if ((!dx && !dy) || !w.passable(nx, ny, p.z) || occ.count(World::key3(nx, ny, p.z))) continue;
                int d = std::max(std::abs(nx - ex), std::abs(ny - ey));
                if (d > bd) { bd = d; bx = dx; by = dy; }
            }
        if (bx || by) {
            occ.erase(World::key3(p.x, p.y, p.z));
            p.px = p.x; p.py = p.y; p.pz = p.z;
            p.facing = std::atan2(-(float)bx, (float)by);
            p.x += bx; p.y += by;
            occ[World::key3(p.x, p.y, p.z)] = i;
            p.stepDur = 0.26f; p.stepT = 0;
        }
        if (p.fleeT <= 0) { p.target = -1; p.path.clear(); p.actIndex = -2; p.stepDur = p.strollDur; }
        return true;
    }
    p.scanT -= dt;
    if (p.target == -1 && p.scanT <= 0) {
        p.scanT = 0.4f + 0.3f * frand();
        p.target = findFoe(p, i, ptx, pty, ptz);
        if (std::getenv("GALILEO_DEBUG_FIGHT") && p.fighter) std::fprintf(stderr, "  [%s %s at %d,%d looks: target %d]\n", p.id.c_str(), p.name.c_str(), p.x, p.y, p.target);
        if (p.target != -1) { p.path.clear(); p.pathPos = 0; p.chaseSteps = 99; p.strikeT = 0.4f + 0.5f * frand(); }
    }
    if (p.target == -1) return false;
    int ex, ey, ez;
    bool alive = where(p.target, ex, ey, ez);
    int d = std::max(std::abs(p.x - ex), std::abs(p.y - ey));
    if (alive && p.scanT <= 0) {   // now and then: a much nearer enemy? (a Croat after you, with Swedes on him)
        p.scanT = 2.0f + frand();
        int n = findFoe(p, i, ptx, pty, ptz);
        int nx, ny, nz;
        if (n != -1 && n != p.target && where(n, nx, ny, nz) && std::max(std::abs(p.x - nx), std::abs(p.y - ny)) + 2 < d) {
            p.target = n; p.path.clear(); p.pathPos = 0; p.chaseSteps = 99;
            ex = nx; ey = ny; ez = nz; d = std::max(std::abs(p.x - ex), std::abs(p.y - ey));
        }
    }
    bool farFromPost = p.hasWork && std::max(std::abs(p.x - p.workX), std::abs(p.y - p.workY)) > 25;   // a soldier keeps to his post
    if (!alive || ez != p.z || d > 20 || farFromPost) {   // gone, or out of reach: back to the day's business
        p.target = -1; p.path.clear(); p.pathPos = 0; p.actIndex = -2; p.stepDur = p.strollDur;
        return false;
    }
    p.walkPhase = 0;
    if (p.parley && p.target == -2 && d <= 2) {   // a brigand come up to you: words first (Game::wildStep)
        p.facing = std::atan2(-(float)(ex - p.x), (float)(ey - p.y));
        parleyArrived = i;
        return true;
    }
    if (d <= 1) {   // beside the enemy: blows
        p.facing = std::atan2(-(float)(ex - p.x), (float)(ey - p.y));
        p.strikeT -= dt;
        if (p.strikeT <= 0) {
            p.strikeT = 1.1f + 0.5f * frand();
            p.strikeAnim = 0.35f;
            if (p.target == -2) {
                int dmg = roll(p.skill, p.dmg, playerSkill, playerArmour);
                std::string who = label(p);
                if (!who.empty()) who[0] = (char)std::toupper((unsigned char)who[0]);
                if (dmg < 0) fightNotes.push_back(who + " strikes at you, and misses.");
                else if (dmg == 0) fightNotes.push_back(who + " strikes you, and the blow glances off.");
                else { playerHurt += (float)dmg; fightNotes.push_back(who + " wounds you (" + std::to_string(dmg) + ")."); }
            } else {
                const Person &o = people[p.target];
                int dmg = roll(p.skill, p.dmg, o.skill, o.armour);
                // the blow heard, if you are near (Game::takeRequests): steel for swords and sabres, and for a blow that
                // lands on a breastplate (a pikeman's); wood for the rest, pikes and muskets swung as clubs
                if (std::abs(p.x - ptx) + std::abs(p.y - pty) <= 30 && p.z == ptz)
                    soundsAt.push_back({p.look.weapon == 3 || p.look.weapon == 4 || (dmg >= 0 && o.armour >= 2) ? "clash" : "knock", p.x + 0.5f, p.y + 0.5f});
                if (dmg > 0) hurt(p.target, (float)dmg, i, minutes);
                else if (people[p.target].target == -1) people[p.target].target = i;
            }
        }
        return true;
    }
    stepToward(w, p, i, ex, ey, ez, 0.3f);
    return true;
}

// ------------------------------------------------------------------ people talking to each other

static int cheb(const Person &p, int x, int y) { return std::max(std::abs(p.x - x), std::abs(p.y - y)); }

bool People::loadChats(const std::string &fn) {
    std::ifstream in(fn);
    if (!in) return false;
    std::string line;
    Chat *cur = nullptr;
    while (std::getline(in, line)) {
        size_t hsh = line.find('#');
        if (hsh != std::string::npos) line = line.substr(0, hsh);
        std::string t = trim(line);
        if (t.empty()) continue;
        std::istringstream ss(t);
        std::string w;
        ss >> w;
        if (!cur) {
            if (w == "chat") { chats.emplace_back(); cur = &chats.back(); ss >> cur->id; }
            else if (w == "earshot") ss >> earshot;
            else if (w == "gap") ss >> chatGap;
            else if (w == "pairgap") ss >> pairGap;
            else if (w == "chance") ss >> chatChance;
            continue;
        }
        auto list = [](const std::string &s) {
            std::vector<std::string> v;
            std::string x;
            std::istringstream ls(s);
            while (std::getline(ls, x, ',')) { x = trim(x); if (!x.empty()) v.push_back(x); }
            return v;
        };
        if (w == "end") { cur = nullptr; continue; }
        if (w == "between") { std::string a, b; ss >> a >> b; cur->a = list(a); cur->b = list(b); continue; }
        std::string rest;
        std::getline(ss, rest);
        rest = trim(rest);
        if (w == "where") { cur->where = lower(rest); continue; }
        if (w == "if") { cur->condA = rest; continue; }
        if (w == "ifb") { cur->condB = rest; continue; }
        if (w == "again") { cur->again = std::atof(rest.c_str()); continue; }
        TalkLine tl;
        if (parseTalk(t, tl) && (tl.keys[0] == "a" || tl.keys[0] == "b")) {
            ChatLine cl;
            cl.who = tl.keys[0] == "a" ? 0 : 1;
            cl.text = tl.text;
            cl.effects = tl.effects;
            cur->lines.push_back(cl);
        } else {
            std::fprintf(stderr, "chats.txt: %s: cannot read \"%s\"\n", cur->id.c_str(), t.c_str());
        }
    }
    return true;
}

bool People::fits(const Person &p, const std::vector<std::string> &who) const {
    // a person may be named by their id in the town file ("martin") or in full ("paris:martin")
    for (auto &x : who)
        if (x == "anyone" || x == p.id || x == p.role || p.id.compare(p.id.find(':') + 1, std::string::npos, x) == 0) return true;
    return false;
}

// Two people standing together near you may start one of the conversations of chats.txt:
// rarely (at most one every chatGap minutes, the same two at most once every pairGap
// minutes), and each conversation once, unless it says "again".
void People::tryChat(World &w, double minutes, int ptx, int pty, int ptz) {
    if (chats.empty() || talk.who >= 0) return;
    auto lastIt = player.find("chat:last");
    if (lastIt != player.end() && minutes - num(lastIt->second) < chatGap) return;
    std::vector<int> idle;
    for (size_t i = 0; i < people.size(); i++) {
        const Person &p = people[i];
        if (!p.active || p.onRoad || p.lying || p.z != ptz || !p.travelTo.empty()) continue;
        if (p.pathPos < p.path.size() || p.stepT < 1 || cheb(p, ptx, pty) > earshot) continue;
        idle.push_back((int)i);
    }
    struct Cand { int chat, a, b; float wt; };
    std::vector<Cand> cands;
    auto spec = [&](const std::vector<std::string> &v) {
        float s = 1;
        for (auto &x : v) s = std::max(s, x == "anyone" ? 1.0f : roles.count(x) ? 2.0f : 4.0f);
        return s;
    };
    for (size_t u = 0; u < idle.size(); u++)
        for (size_t v = u + 1; v < idle.size(); v++) {
            const Person &P = people[idle[u]], &Q = people[idle[v]];
            if (cheb(P, Q.x, Q.y) > 2) continue;
            auto pg = P.mem.find("chatted:" + Q.id);
            if (pg != P.mem.end() && minutes - num(pg->second) < pairGap) continue;
            for (size_t c = 0; c < chats.size(); c++) {
                const Chat &ch = chats[c];
                if (ch.lines.empty()) continue;
                auto hd = player.find("heard:" + ch.id);
                if (hd != player.end() && (ch.again < 0 || minutes - num(hd->second) < ch.again)) continue;
                for (int o = 0; o < 2; o++) {
                    int a = o ? idle[v] : idle[u], b = o ? idle[u] : idle[v];
                    const Person &A = people[a], &B = people[b];
                    if (!fits(A, ch.a) || !fits(B, ch.b)) continue;
                    if (!holds(A, ch.condA, minutes) || !holds(B, ch.condB, minutes)) continue;
                    if (!ch.where.empty() && lower(w.floorName(A.x, A.y, A.z)).find(ch.where) == std::string::npos) continue;
                    cands.push_back({(int)c, a, b, spec(ch.a) * spec(ch.b)});
                }
            }
        }
    if (cands.empty()) return;
    if (hash01((int)cands.size() * 7919 + idle[0], (int)(minutes * 10), 21) > chatChance) return;
    float total = 0;
    for (auto &c : cands) total += c.wt;
    float r = total * hash01((int)cands.size(), (int)(minutes * 10), 22);
    size_t k = 0;
    while (k + 1 < cands.size() && r > cands[k].wt) { r -= cands[k].wt; k++; }
    const Cand &c = cands[k];
    chatNow = ChatNow();
    chatNow.chat = c.chat; chatNow.a = c.a; chatNow.b = c.b;
    chatNow.conv = heard.empty() ? 1 : heard.back().conv + 1;
    player["heard:" + chats[c.chat].id] = std::to_string((long)minutes);
    player["chat:last"] = std::to_string((long)minutes);
    Person &A = people[c.a], &B = people[c.b];
    A.facing = std::atan2(-(float)(B.x - A.x), (float)(B.y - A.y));
    B.facing = std::atan2(-(float)(A.x - B.x), (float)(A.y - B.y));
    notice = "You overhear " + label(A) + " and " + label(B) + "." + (heard.empty() ? "   (J: what you have overheard)" : "");
    chatLine(w, minutes, ptx, pty, ptz);
}

// the next line of the conversation: said aloud, its effects, and kept if you heard it
void People::chatLine(World &w, double minutes, int ptx, int pty, int ptz) {
    const Chat &ch = chats[chatNow.chat];
    const ChatLine &l = ch.lines[chatNow.line];
    Person &sp = people[l.who == 0 ? chatNow.a : chatNow.b];
    Person &hr = people[l.who == 0 ? chatNow.b : chatNow.a];
    std::string text = fill(sp, l.text, &hr);
    size_t plain = 0;
    for (char c : text) if (c != '*') plain++;
    chatNow.dur = 2.0f + plain / 14.0f;   // time to read it
    sp.bubble = text;
    sp.bubbleT = chatNow.dur;
    hr.bubbleT = 0;
    bool near = sp.z == ptz && cheb(sp, ptx, pty) <= earshot + 4;
    for (std::string e : l.effects) {
        Person *target = &sp;
        if (e.rfind("hearer ", 0) == 0) { target = &hr; e = trim(e.substr(7)); }
        bool yours = e.rfind("player ", 0) == 0 || e == "name";
        if (yours && !near) continue;   // you were not there to hear it
        apply(*target, {e});
    }
    if (near) {
        std::string who = label(sp);
        heard.push_back({chatNow.conv, minutes, w.floorName(sp.x, sp.y, sp.z), who, text});
        if (echo) std::fprintf(stderr, "    (overheard) %s: %s\n", who.c_str(), text.c_str());
    }
}

void People::endChat(double minutes) {
    if (chatNow.chat < 0) return;
    Person &a = people[chatNow.a], &b = people[chatNow.b];
    a.mem["chatted:" + b.id] = std::to_string((long)minutes);
    b.mem["chatted:" + a.id] = std::to_string((long)minutes);
    a.bubbleT = 0;
    b.bubbleT = 0;
    chatNow = ChatNow();
}

// a bed at an inn of the person's town (for travellers)
void People::lodge(World &w, Person &p) {
    const Settlement *s = townOf(w, p.town);
    p.hasBed = false;
    p.home = 0;
    if (!s) return;
    for (size_t i = 0; i < w.buildings.size() && !p.hasBed; i++) {
        const Building &b = w.buildings[i];
        if (b.kind != "inn" || !s->inside(b.x0, b.y0)) continue;
        for (int z = 0; z <= 1 && !p.hasBed; z++)
            for (int X = b.x0; X <= b.x1 && !p.hasBed; X++)
                for (int Y = b.y0; Y <= b.y1 && !p.hasBed; Y++) {
                    Tile *t = w.tileAt(X, Y, z);
                    if (!t || t->feature != F_BED) continue;
                    bool taken = false;
                    for (auto &o : people) if (&o != &p && o.hasBed && o.bedX == X && o.bedY == Y && o.bedZ == z) taken = true;
                    if (taken) continue;
                    p.hasBed = true; p.home = (int)i + 1; p.bedX = X; p.bedY = Y; p.bedZ = z;
                }
    }
}

void People::travelStep(World &w, Person &p, double minutes) {
    if (p.onRoad) {
        if (minutes < p.arrive) return;
        // arrival: at a gate of the new town, lodging at an inn
        p.onRoad = false;
        p.town = p.travelTo;
        p.travelTo.clear();
        p.mem["town"] = p.town;
        p.mem["arrived:" + lower(p.town)] = "1";
        lodge(w, p);
        p.hasWork = false;
        if (!p.workPlace.empty()) {
            int gx, gy, gz;
            if (placeFor(w, p, "go", p.workPlace, gx, gy, gz)) { p.hasWork = true; p.workX = gx; p.workY = gy; p.workZ = gz; }
        }
        int gx = 0, gy = 0, gz = 0;
        const Settlement *s = townOf(w, p.town);
        if (s && !placeFor(w, p, "go", "gate", gx, gy, gz)) { gx = s->x0 + s->w / 2; gy = s->y0 + s->h / 2; }
        p.x = p.px = gx; p.y = p.py = gy; p.z = p.pz = gz;
        p.placed = false;
        p.actIndex = -2;
        p.path.clear();
        return;
    }
    const Settlement *from = townOf(w, p.town), *to = townOf(w, p.travelTo);
    if (!to) { p.travelTo.clear(); return; }
    // at the gate (or far from the player): set off
    bool atGoal = p.pathPos >= p.path.size() && p.placed && p.activity == "go" && p.actPlace == "gate";
    if (atGoal || !p.active) {
        float d = from ? length(vec2(from->x0 + from->w * 0.5f - to->x0 - to->w * 0.5f, from->y0 + from->h * 0.5f - to->y0 - to->h * 0.5f)) : 500;
        p.onRoad = true;
        p.arrive = minutes + d / 4.0;   // about four tiles of road a minute, on foot or by cart
        p.lying = false;
        occ.erase(World::key3(p.x, p.y, p.z));
    }
}

int People::adjacentTo(int X, int Y, int Z, float facing) const {
    int best = -1;
    float bs = -1e9f;
    vec2 f(-std::sin(facing), std::cos(facing));
    for (size_t i = 0; i < people.size(); i++) {
        const Person &p = people[i];
        if (p.dead || p.away || p.onRoad || p.z != Z || std::abs(p.x - X) > 1 || std::abs(p.y - Y) > 1 || (p.x == X && p.y == Y)) continue;
        vec2 d((float)(p.x - X), (float)(p.y - Y));
        float s = dot(f, d * (1.0f / length(d)));
        if (s > bs) { bs = s; best = (int)i; }
    }
    return best;
}

std::string People::label(const Person &p) const {
    auto fam = p.mem.find("family");   // "your father", "your mother", once you have chosen who you are
    if (fam != p.mem.end() && player.count("me:age")) return "your " + fam->second;
    auto it = player.find("knows:" + p.id);
    if (it != player.end() && truthy(it->second)) return p.name;
    return p.title;
}

// ------------------------------------------------------------------ conversation

bool People::planCacheHolds(const Person &p, const PlanCache &c) const {
    for (const PlanDep &d : c.deps) {
        const Memory &m = d.ofPlayer ? player : p.mem;
        auto it = m.find(d.key);
        if ((it != m.end()) != d.present) return false;
        if (d.present && it->second != d.value) return false;
    }
    return true;
}

bool People::holdsRec(const Person &p, const std::string &cond, double minutes, PlanCache *rec) const {
    if (cond.empty()) return true;
    // atoms joined by "and"
    std::vector<std::string> atoms;
    std::string c = cond;
    size_t pos;
    while ((pos = c.find(" and ")) != std::string::npos) { atoms.push_back(trim(c.substr(0, pos))); c = c.substr(pos + 5); }
    atoms.push_back(trim(c));
    double h = std::fmod(minutes, 1440.0) / 60.0;
    for (std::string a : atoms) {
        bool neg = false, ofPlayer = false;
        if (a.rfind("not ", 0) == 0) { neg = true; a = trim(a.substr(4)); }
        if (a.rfind("player ", 0) == 0) { ofPlayer = true; a = trim(a.substr(7)); }
        if (a.rfind("not ", 0) == 0) { neg = !neg; a = trim(a.substr(4)); }   // "player not KEY" too
        bool recent = false;   // "recent KEY": KEY holds a time (set KEY=now) within the last three days;
        double days = 3;       // "recent:N KEY": within the last N days
        if (a.rfind("recent ", 0) == 0) { recent = true; a = trim(a.substr(7)); }
        else if (a.rfind("recent:", 0) == 0 && a.find(' ') != std::string::npos) {
            recent = true;
            days = std::atof(a.c_str() + 7);
            a = trim(a.substr(a.find(' ')));
        }
        bool v;
        if (recent) {
            const Memory &m = ofPlayer ? player : p.mem;
            auto it = m.find(a);
            double t = it == m.end() ? -1 : num(it->second);
            if (rec) {
                rec->deps.push_back({ofPlayer, a, it != m.end(), it == m.end() ? std::string() : it->second});
                if (t != std::floor(t) || days * 1440 != std::floor(days * 1440)) rec->coarse = false;   // a fractional time: only this very moment
            }
            v = it != m.end() && minutes - t >= 0 && minutes - t < days * 1440;
            if (v == neg) return false;
            continue;
        }
        if (a.rfind("day:", 0) == 0 && a.find(' ') != std::string::npos) {
            // "day:N KEY": today is the Nth calendar day after the day of the time KEY holds (set KEY=now); "day:N+ KEY",
            // that day or any later one ("day:1 report-at": the day after you told Monro, the muster's)
            std::string nk = a.substr(4, a.find(' ') - 4), key = trim(a.substr(a.find(' ')));
            bool orLater = !nk.empty() && nk.back() == '+';
            if (orLater) nk.pop_back();
            const Memory &m = ofPlayer ? player : p.mem;
            auto it = m.find(key);
            if (rec) rec->deps.push_back({ofPlayer, key, it != m.end(), it == m.end() ? std::string() : it->second});
            long dk = it == m.end() ? 0 : (long)std::floor(num(it->second) / 1440.0), today = (long)std::floor(minutes / 1440.0);
            long n = std::atol(nk.c_str());
            v = it != m.end() && (orLater ? today - dk >= n : today - dk == n);
            if (v == neg) return false;
            continue;
        }
        if (a.rfind("hour", 0) == 0 && a.size() > 4 && (a[4] == '<' || a[4] == '>' || a[4] == '=')) {
            // "hour>=20", "hour<3", "hour>=9:30": the time of day
            std::string op = a.substr(4, (a.size() > 5 && a[5] == '=') ? 2 : 1), val = trim(a.substr(4 + op.size()));
            double x = std::atof(val.c_str());
            if (val.find(':') != std::string::npos) x += std::atof(val.c_str() + val.find(':') + 1) / 60.0;
            v = op == ">=" ? h >= x : op == "<=" ? h <= x : op == ">" ? h > x : op == "<" ? h < x : std::fabs(h - x) < 1e-9;
            if (v == neg) return false;
            continue;
        }
        if (a == "morning") v = h >= 5 && h < 12;
        else if (a == "working") v = p.activity == "work" && !p.away;   // at their work by the timetable (Meister Franz, sent for)
        else if (a == "afternoon") v = h >= 12 && h < 18;
        else if (a == "evening") v = h >= 18 && h < 22;
        else if (a == "night") v = h >= 22 || h < 5;
        else if (a == "asleep") v = p.lying;
        else if (a == "friday") v = fridayAt(minutes);
        else if (a == "lent") v = lentAt(minutes);
        else if (a == "fasting") v = fridayAt(minutes) || lentAt(minutes);   // a fish day: a Friday, or in Lent
        else {
            // KEY, or KEY OP NUMBER
            std::string key = a, op, val;
            static const char *ops[] = {">=", "<=", "!=", ">", "<", "="};
            for (const char *o : ops) {
                size_t q = a.find(o);
                if (q != std::string::npos) { key = trim(a.substr(0, q)); op = o; val = trim(a.substr(q + strlen(o))); break; }
            }
            if (key == "date" && !op.empty()) {   // the calendar: "date<1633-01-01" (Kepler's death, in the right words for the year)
                double t = (double)minutesOf(val, "");
                v = op == ">=" ? minutes >= t : op == "<=" ? minutes < t + 1440 : op == ">" ? minutes >= t + 1440 : op == "<" ? minutes < t
                  : op == "=" ? (minutes >= t && minutes < t + 1440) : (minutes < t || minutes >= t + 1440);
                if (rec) rec->coarse = false;
                if (v == neg) return false;
                continue;
            }
            const Memory &m = ofPlayer ? player : p.mem;
            auto it = m.find(key);
            std::string have = it == m.end() ? "" : it->second;
            if (rec) rec->deps.push_back({ofPlayer, key, it != m.end(), have});
            if (op.empty()) v = truthy(have);
            else if (op == "=") v = have == val || (!have.empty() && num(have) == num(val) && !val.empty() && std::isdigit((unsigned char)val[0]));
            else if (op == "!=") v = have != val;
            else {
                double x = num(have), y = num(val);
                v = op == ">=" ? x >= y : op == "<=" ? x <= y : op == ">" ? x > y : x < y;
            }
        }
        if (v == neg) return false;
    }
    return true;
}

void People::apply(Person &p, const std::vector<std::string> &effects) {
    for (std::string e : effects) {
        Memory *m = &p.mem;
        if (e.rfind("player ", 0) == 0) { m = &player; e = trim(e.substr(7)); }
        std::istringstream ss(e);
        std::string verb, arg, n;
        ss >> verb >> arg >> n;
        if (verb == "set") {
            size_t eq = arg.find('=');
            if (eq == std::string::npos) (*m)[arg] = "1";
            else if (arg.substr(eq + 1) == "now") (*m)[arg.substr(0, eq)] = std::to_string((long)lastMinutes);   // a time, for "recent KEY"
            else if (arg.substr(eq + 1) == "$said") {   // the words you just said, as a name: "Blanchette"
                std::string v = lastSaid.substr(0, 24);
                while (!v.empty() && (v.back() == ' ' || v.back() == '.' || v.back() == '!')) v.pop_back();
                if (!v.empty()) v[0] = (char)std::toupper((unsigned char)v[0]);
                (*m)[arg.substr(0, eq)] = v;
            }
            else (*m)[arg.substr(0, eq)] = arg.substr(eq + 1);
        } else if (verb == "unset") {
            m->erase(arg);
        } else if (verb == "add") {
            (*m)[arg] = std::to_string((long)(num((*m)[arg]) + (n.empty() ? 1 : num(n))));
        } else if (verb == "learn") {
            std::string k = "skill:" + arg;
            (*m)[k] = std::to_string((long)(num((*m)[k]) + (n.empty() ? 1 : num(n))));
        } else if (verb == "farmhand") {   // you work for the herds and hens of this person's town (Game::useAnimal)
            player["farmhand"] = p.town;
        } else if (verb == "travel") {
            std::string rest = e.substr(e.find("travel") + 6);
            p.travelTo = trim(rest);
        } else if (verb == "postjob" || verb == "postdeliver") {   // the post: a packet to carry, or one handed in
            postBy = (int)(&p - people.data());
            postDeliver = verb == "postdeliver";
            talk.over = true;
        } else if (verb == "paintportrait") {   // your portrait, painted from your own figure (Game::paintPortrait)
            portraitWanted = true;
        } else if (verb == "buycow") {   // a cow bought at a cattle market, led away on a halter (Game::cowDeal)
            cowBy = (int)(&p - people.data());
            cowPrice = arg.empty() ? 4800 : std::atol(arg.c_str());
            talk.over = true;
        } else if (verb == "hire" || verb == "buyhorse") {   // a post horse hired, or a horse bought (Game::horseDeal)
            horseBy = (int)(&p - people.data());
            horseBuy = verb == "buyhorse";
            horsePrice = arg.empty() ? (horseBuy ? 2880 : 36) : std::atol(arg.c_str());
            talk.over = true;
        } else if (verb == "buyship") {   // "buyship PRICE [KIND]": a ship of your own (Game::shipDeal)
            shipBy = (int)(&p - people.data());
            shipPrice = arg.empty() ? 48000 : std::atol(arg.c_str());
            shipKind = n.empty() ? "pinnace" : n;   // the third word
            talk.over = true;
        } else if (verb == "change") {   // a money-changer: your coin for the coin of this place, or for COIN
            changeBy = (int)(&p - people.data());
            changeTo = arg;
        } else if (verb == "passage") {   // a boat across the water (Game::takePassage)
            passageBy = (int)(&p - people.data());
            passagePrice = std::atol(arg.c_str());
            passageMinutes = std::atoi(n.c_str());
            std::string rest;
            std::getline(ss, rest);
            passageTo = trim(rest);
            talk.over = true;
        } else if (verb == "pass") {   // the time a line says it takes ("all afternoon": a lesson), in minutes
            passMinutes += std::atof(arg.c_str());
            talk.over = true;
        } else if (verb == "sleep") {   // a bed for the night, sold by this person (Game::sleepPaid)
            sleepBy = (int)(&p - people.data());
            talk.over = true;
        } else if (verb == "postride") {   // the post ride, taken (Game::postRide)
            rideBy = (int)(&p - people.data());
            talk.over = true;
        } else if (verb == "staytillword") {   // the bed kept until word comes (Game::stayTillWord)
            stayBy = (int)(&p - people.data());
            talk.over = true;
        } else if (verb == "play") {   // a piece of data/music.txt, played by this person
            playRequest = arg;
            playBy = (int)(&p - people.data());
        } else if (verb == "robbed" || verb == "bandfight" || verb == "escort" || verb == "billdraw" || verb == "billcash") {
            roadRequests.push_back({verb + (arg.empty() ? "" : " " + arg), (int)(&p - people.data())});   // Game::roadRequest
            if (verb != "billdraw" && verb != "billcash") talk.over = true;
        } else if (verb == "follow") {   // this person walks behind you from now on (Game::followerStep): a wounded man, helped down
            notice = p.name + " is leaning on you: he goes where you go.";
            player["follower"] = p.id;
            p.lying = false;
            talk.over = true;
        } else if (verb == "unfollow") {   // whoever walks behind you stops here, and goes back to the day's business
            auto f = player.find("follower");
            if (f != player.end()) {
                if (Person *q = byId(f->second)) { q->placed = true; q->actIndex = -2; q->path.clear(); q->pathPos = 0; q->planCache.valid = false; }
                player.erase(f);
            }
        } else if (verb == "name") {
            player["knows:" + p.id] = "1";
        } else if (verb == "bye") {
            talk.over = true;
        }
    }
}

std::string People::fill(const Person &p, const std::string &text, const Person *other) const {
    std::string out;
    auto firstOf = [](const std::string &n) { return n.substr(0, n.find(' ')); };
    auto lastOf = [](const std::string &n) { size_t s = n.rfind(' '); return s == std::string::npos ? n : n.substr(s + 1); };
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '$') {
            auto rep = [&](const char *k, const std::string &v) {
                size_t n = strlen(k);
                if (text.compare(i + 1, n, k) == 0) { out += v; i += n; return true; }
                return false;
            };
            if (other && (rep("otherfirst", firstOf(other->name)) || rep("otherlast", lastOf(other->name)) || rep("other", other->name))) continue;
            if (rep("He", p.sex == 'f' ? "She" : "He") || rep("he", p.sex == 'f' ? "she" : "he") || rep("name", p.name) || rep("first", firstOf(p.name)) || rep("last", lastOf(p.name)) || rep("role", p.title) || rep("town", p.town)) continue;
            {   // $me: your own name, as you chose it
                auto mi = player.find("me:name");
                if (rep("me", mi == player.end() ? std::string("stranger") : mi->second)) continue;
                auto ci = player.find("cow:name");   // $cow: your family's new cow, as you named her
                if (rep("cow", ci == player.end() ? std::string("the cow") : ci->second)) continue;
            }
            if (i + 1 < text.size() && text[i + 1] == '[') {   // $[KEY]: what they remember under KEY
                size_t close = text.find(']', i + 2);
                if (close != std::string::npos) {
                    auto it = p.mem.find(text.substr(i + 2, close - i - 2));
                    out += it == p.mem.end() ? std::string("something") : it->second;
                    i = close;
                    continue;
                }
            }
        }
        out += text[i];
    }
    return out;
}

// The numbers in what was said: each run of digits ("2*11+3*7+5=48": 2, 11, 3, 7, 5, 48; "48.5" stays whole), and
// numbers in words ("forty-eight"; "forty eight" also as "forty-eight" and each word alone).
static std::vector<std::string> numbersIn(const std::string &words) {
    static const std::set<std::string> named = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
        "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen",
        "nineteen", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety", "hundred", "thousand"};
    std::vector<std::string> out, run;   // run: number words in a row
    auto endRun = [&]() {
        if (run.size() > 1) {
            std::string dash, space;
            for (size_t i = 0; i < run.size(); i++) { dash += (i ? "-" : "") + run[i]; space += (i ? " " : "") + run[i]; }
            out.push_back(dash);
            out.push_back(space);
        }
        for (const std::string &r : run) out.push_back(r);
        run.clear();
    };
    std::istringstream ss(words);
    std::string wd;
    while (ss >> wd) {
        bool digits = false;
        std::string cur;
        for (char c : wd + " ") {
            if (std::isdigit((unsigned char)c) || (c == '.' && !cur.empty())) { cur += c; continue; }
            while (!cur.empty() && cur.back() == '.') cur.pop_back();
            if (!cur.empty()) { out.push_back(cur); digits = true; }
            cur.clear();
        }
        if (digits) { endRun(); continue; }
        std::string bare;
        for (char c : wd) if (std::isalpha((unsigned char)c) || c == '-') bare += c;
        bool number = !bare.empty();
        std::istringstream parts(bare);
        std::string part;
        int n = 0;
        while (std::getline(parts, part, '-')) { n++; if (!named.count(part)) number = false; }
        if (number && n > 0) run.push_back(bare);
        else endRun();
    }
    endRun();
    return out;
}

// keywords match as in Ultima: the first four letters are enough; but a place's name only whole (WHOLE: People::townWords)
static bool keyMatches(const std::string &word, const std::string &key, const std::set<std::string> *whole = nullptr) {
    if (!key.empty() && key[0] == '=') return word == key.substr(1);   // "=forty-eight": that word only, not "forty-seven"
    if (word == key) return true;
    if (whole && whole->count(word)) return false;
    if (word.size() >= 4 && key.size() >= 4) return word.compare(0, 4, key, 0, 4) == 0;
    return false;
}

const TalkLine *People::match(const Person &p, const std::string &word, double minutes) const {
    for (const std::vector<TalkLine> *lines : p.talkLists())
        for (const TalkLine &t : *lines)
            for (const std::string &k : t.keys)
                if (keyMatches(word, k, &townWords) && holds(p, t.cond, minutes)) return &t;
    return nullptr;
}

// Each *word* a person may say should be a keyword they answer (whatever the conditions): the
// check lists those that are not, one line per word and text, with how many people say it.
std::vector<std::string> People::unanswered() const {
    struct Miss { int n = 0; std::set<std::string> towns; std::string who; };
    std::map<std::string, Miss> miss;
    for (const Person &p : people)
      for (const std::vector<TalkLine> *own : p.talkLists())
        for (const TalkLine &t : *own)
            for (size_t a = t.text.find('*'); a != std::string::npos; a = t.text.find('*', a + 1)) {
                size_t b = t.text.find('*', a + 1);
                if (b == std::string::npos) break;
                std::string phrase = t.text.substr(a + 1, b - a - 1), word;
                bool ok = false;
                std::vector<std::string> words;
                for (char c : phrase + " ") {
                    if (std::isalpha((unsigned char)c) || (c & 0x80)) word += (char)std::tolower((unsigned char)c);
                    else if (!word.empty()) { words.push_back(word); word.clear(); }
                }
                for (const std::string &wd : words)
                    for (const std::vector<TalkLine> *lst : p.talkLists())
                        for (const TalkLine &u : *lst)
                            for (const std::string &k : u.keys)
                                if (k != "?" && keyMatches(wd, k, &townWords)) ok = true;
                if (!ok && !words.empty()) {
                    Miss &m = miss[phrase + "  |  " + t.text.substr(0, 60)];
                    m.n++;
                    m.towns.insert(p.town);
                    if (m.who.empty()) m.who = p.id;
                }
                a = b;
            }
    std::vector<std::string> out;
    for (auto &kv : miss) {
        std::string towns;
        for (auto &tw : kv.second.towns) towns += (towns.empty() ? "" : ", ") + tw;
        out.push_back("*" + kv.first + "...  (" + std::to_string(kv.second.n) + " people, " + towns + "; " + kv.second.who + ")");
    }
    return out;
}

// ------------------------------------------------------------------ buying, selling, giving

static long numOf(const Memory &m, const std::string &k) {
    auto it = m.find(k);
    return it == m.end() ? 0 : std::atol(it->second.c_str());
}
static void addTo(Memory &m, const std::string &k, long d) {
    long v = numOf(m, k) + d;
    if (v == 0 && k.rfind("has:", 0) == 0) m.erase(k);
    else m[k] = std::to_string(v);
}

float People::carriedWeight() const {
    float w = 0;
    for (auto &kv : player)
        if (kv.first.rfind("has:", 0) == 0) {
            int k = items->find(kv.first.substr(4));
            if (k >= 0) w += items->kinds[k].weight * std::atol(kv.second.c_str());
        }
    return w;
}

// a shop has its full stock again each morning ("restocked" holds the day, plus one)
void People::restock(Person &p, double minutes) {
    long day = (long)(minutes / 1440) + 1;
    if (numOf(p.mem, "restocked") == day) return;
    auto mk = markets.find(lower(p.town));
    for (auto &s : p.sells) {
        if (mk != markets.end() && mk->second.searchOut.count(s.first)) continue;   // seized: none to be had
        if (numOf(p.mem, "stock:" + s.first) < s.second) p.mem["stock:" + s.first] = std::to_string(s.second);
    }
    p.mem["restocked"] = std::to_string(day);
}

// ------------------------------------------------------------------ the market
// data/market.txt:  market TOWN / makes ITEM FACTOR ... / wants ITEM FACTOR ... / customs RATE /
// language LANGUAGE / end.  The merchants of a market town (role merchant) sell what it makes.
bool People::loadMarkets(World &w, const std::string &fn) {
    std::ifstream in(fn);
    if (!in) return false;
    std::string line, town;
    Market *cur = nullptr;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        std::istringstream ss(t);
        std::string w0;
        ss >> w0;
        if (w0 == "market") { std::getline(ss, town); town = lower(trim(town)); cur = &markets[town]; continue; }
        if (!cur) continue;
        if (w0 == "end") { cur = nullptr; continue; }
        if (w0 == "makes" || w0 == "wants") {
            std::string id; float f;
            while (ss >> id >> f) {
                if (items && items->find(id) < 0) std::fprintf(stderr, "market.txt: %s: no thing called %s\n", town.c_str(), id.c_str());
                (w0 == "makes" ? cur->makes : cur->wants)[id] = f;
                goods.insert(id);
            }
        } else if (w0 == "forbids") {
            std::string rest;
            std::getline(ss, rest);
            size_t bar = rest.find('|');
            std::istringstream is(rest.substr(0, bar));
            std::string id;
            while (is >> id) {
                if (items && items->find(id) < 0) std::fprintf(stderr, "market.txt: %s: no thing called %s\n", town.c_str(), id.c_str());
                cur->forbids.insert(id);
            }
            if (bar != std::string::npos) cur->forbidWhy = trim(rest.substr(bar + 1));
        } else if (w0 == "customs") ss >> cur->customs;
        else if (w0 == "language") ss >> cur->language;
    }
    for (auto &p : people) {   // the merchants of a market town sell what it makes
        auto it = markets.find(lower(p.town));
        if (it == markets.end() || p.role != "merchant") continue;
        for (auto &mk : it->second.makes) {
            bool has = false;
            for (auto &s : p.sells) if (s.first == mk.first) has = true;
            if (!has) p.sells.push_back({mk.first, 4});
        }
    }
    (void)w;
    return true;
}

// a town that has bought a lot of something lately pays less for more of it: each one sold
// there counts, and the count wears off by one a day
double People::glut(const std::string &town, const std::string &item, double minutes) const {
    auto a = player.find("glut:" + town + ":" + item), b = player.find("glutAt:" + town + ":" + item);
    if (a == player.end() || b == player.end()) return 0;
    return std::max(0.0, std::atof(a->second.c_str()) - (minutes - std::atof(b->second.c_str())) / 1440.0);
}

long People::priceIn(const Person &shop, int k, bool buying, double minutes) const {
    const ItemKind &K = items->kinds[k];
    double f = buying ? 1.0 : 0.5;
    auto it = markets.find(lower(shop.town));
    if (it != markets.end()) {
        const Market &m = it->second;
        auto mk = m.makes.find(K.id), wt = m.wants.find(K.id);
        if (buying) f = mk != m.makes.end() ? mk->second : wt != m.wants.end() ? wt->second : 1.0;
        else if (wt != m.wants.end()) f = wt->second * 0.9 * std::pow(0.92, glut(lower(shop.town), K.id, minutes));
        else if (mk != m.makes.end()) f = mk->second * 0.7;
        if (!m.language.empty() && numOf(player, "skill:" + m.language) <= 0) f *= buying ? 1.1 : 0.9;
    }
    if (K.id == "fish" && (fridayAt(minutes) || lentAt(minutes))) f *= 1.5;   // a fish day: a Friday, or in Lent
    return haggled(std::max(1L, (long)std::lround(K.price * f)), buying);
}

long People::customs(const std::string &town, std::string &what) {
    auto it = markets.find(lower(town));
    if (it == markets.end() || it->second.customs <= 0 || !items) return 0;
    double value = 0;
    std::vector<std::string> on;
    for (auto &g : goods) {
        long n = numOf(player, "has:" + g);
        int k = items->find(g);
        if (n <= 0 || k < 0 || it->second.forbids.count(g)) continue;   // nobody declares contraband
        value += (double)n * items->kinds[k].price;
        on.push_back(n > 1 ? items->kinds[k].plural : items->kinds[k].name);
    }
    long duty = std::min(numOf(player, "money"), (long)std::lround(value * it->second.customs));
    if (duty <= 0) return 0;
    for (size_t i = 0; i < on.size(); i++) what += (i ? (i + 1 == on.size() ? " and " : ", ") : "") + on[i];
    addTo(player, "money", -duty);
    return duty;
}

std::vector<std::pair<std::string, long>> People::contraband(const std::string &town, std::string *why, bool leaving) const {
    std::vector<std::pair<std::string, long>> out;
    auto it = markets.find(lower(town));
    if (it == markets.end()) return out;
    for (auto &g : leaving ? it->second.searchOut : it->second.forbids) {
        long n = numOf(player, "has:" + g);
        if (n > 0) out.push_back({g, n});
    }
    if (why) {
        *why = it->second.forbidWhy;
        if (!out.empty() && it->second.whyOf.count(out[0].first)) *why = it->second.whyOf.at(out[0].first);
    }
    return out;
}

bool People::eventForbidden(const std::string &town, const std::string &item) const {
    auto it = markets.find(lower(town));
    return it != markets.end() && it->second.searchOut.count(item) > 0;
}

// "buy" (what they sell, and the prices), "buy bread", "sell horseshoe", "give strings",
// "give 2 sous". Buying and selling need a shopkeeper (a "sells" line); whether a gift is
// taken is for the person's own lines to say: "give:ID" (or "give:money", or "give:?" for
// anything), with the effect "take" if they keep it.
// haggling (a skill): a tenth off what you buy, a tenth more for what you sell, per lesson (three at most)
long People::haggled(long price, bool buying) const {
    long lv = std::min(3L, numOf(player, "skill:haggling"));
    if (lv <= 0) return price;
    return std::max(1L, buying ? price * (10 - lv) / 10 : price * (10 + lv) / 10);
}

bool People::trade(Person &p, const std::string &words, double minutes) {
    std::vector<std::string> w;
    {
        std::istringstream ss(words);
        std::string x;
        while (ss >> x) {
            x.erase(std::remove_if(x.begin(), x.end(), [](char c) { return c == '?' || c == '!' || c == '.' || c == ','; }), x.end());
            if (!x.empty()) w.push_back(x);
        }
    }
    if (w.empty()) return false;
    const std::string verb = w[0];
    auto say = [&](const std::string &t) { talk.lines.push_back({true, t}); };
    bool owed = debt.kind >= 0 && debt.keeper == p.id && !debt.paid && !debt.returned;
    if (verb == "pay") {   // for something you took in front of them
        if (!owed) return false;
        long have = numOf(player, "money");
        if (have < debt.price) { say("That is " + moneyString(debt.price) + ", and you have " + (have ? "only " + moneyString(have) : std::string("no money")) + ". Then put it back."); return true; }
        addTo(player, "money", -debt.price);
        addTo(p.mem, "money", debt.price);
        debt.paid = true;
        say("Very well: " + moneyString(debt.price) + ". It is yours now.");
        return true;
    }
    if (verb == "give" && owed) {   // giving it back
        bool it = false;
        for (size_t i = 1; i < w.size() && !it; i++) it = items->match(w[i]) == debt.kind;
        if (it && numOf(player, "has:" + items->kinds[debt.kind].id) > 0) {
            addTo(player, "has:" + items->kinds[debt.kind].id, -1);
            if (--debt.count <= 0) debt.returned = true;
            say("(Taking it back.) And keep your hands to yourself.");
            return true;
        }
    }
    if (verb == "bet" && numOf(p.mem, "gambler") > 0) {   // "bet 2 sous": cards or dice with a gambler
        long amount = 0;
        for (size_t i = 1; i < w.size(); i++) {
            long unit = w[i] == "denier" || w[i] == "deniers" ? 1 : w[i] == "sou" || w[i] == "sous" ? 12 : w[i] == "livre" || w[i] == "livres" ? 240 : 0;
            if (!unit) continue;
            amount = ((i > 1 && std::isdigit((unsigned char)w[i - 1][0])) ? std::atol(w[i - 1].c_str()) : 1) * unit;
            break;
        }
        if (amount <= 0) { say("How much will you stake?"); return true; }
        if (numOf(player, "money") < amount) { say("You have not got it, my friend. Come back with money."); return true; }
        if (numOf(p.mem, "money") < amount) { say("I cannot cover that today. Something smaller."); return true; }
        // the odds: a little against you, better with each lesson in cards (three at most)
        double chance = 0.42 + 0.05 * std::min(3L, numOf(player, "skill:cards"));
        bool win = hash01((int)(minutes * 7) + (int)numOf(player, "bets"), (int)amount, 4242) < chance;
        addTo(player, "bets", 1);
        addTo(player, "money", win ? amount : -amount);
        addTo(p.mem, "money", win ? -amount : amount);
        static const char *wins[] = {"(The cards fall your way.) You win! Again? Or are you afraid?", "(You throw a seven.) Yours, curse it. Again.", "(He slaps down his cards and laughs.) The luck of the stranger. Take it."};
        static const char *losses[] = {"(He turns up the last card and smiles.) Mine, I think. Again?", "(You throw two and three.) Bad luck. The dice are honest, I swear it.", "(He sweeps up the money.) Fortune is a woman, my friend. Try her again."};
        int r = (int)(hash01((int)minutes, (int)numOf(player, "bets"), 77) * 3) % 3;
        say(std::string(win ? wins[r] : losses[r]) + " (You have " + (numOf(player, "money") ? moneyString(numOf(player, "money")) : std::string("no money")) + ".)");
        return true;
    }
    if ((verb == "buy" || verb == "wares" || verb == "sell") && !p.sells.empty() &&
        (holds(p, "recent saw-thief", minutes) || holds(p, "recent knows-thief", minutes))) {
        say("I do not deal with thieves. Be off with you.");
        return true;
    }
    // how many: "buy 4 quills", "buy two quills", "buy half a dozen quills"
    static const std::map<std::string, int> nums = {{"one", 1}, {"two", 2}, {"three", 3}, {"four", 4}, {"five", 5}, {"six", 6},
                                                    {"seven", 7}, {"eight", 8}, {"nine", 9}, {"ten", 10}, {"eleven", 11},
                                                    {"twelve", 12}, {"dozen", 12}, {"pair", 2}, {"couple", 2}};
    // a whole word anywhere in what you said before the first four letters of any ("sell
    // physicians certificate": not Aristotle's Physics); with carried, of the kinds a word names,
    // one in your pack first ("sell certificate", holding only Bellarmine's)
    auto findKind = [&](size_t from, bool carried = false) -> int {
        for (int pass = 0; pass < 2; pass++) {
            int first = -1;
            for (size_t i = from; i < w.size(); i++) {
                if (std::isdigit((unsigned char)w[i][0]) || nums.count(w[i]) || w[i] == "half") continue;   // a count, not a thing ("two": not the Two New Sciences)
                int k = items->match(w[i], pass);
                if (k < 0) continue;
                if (first < 0) first = k;
                if (!carried) return k;
                if (numOf(player, "has:" + items->kinds[k].id) > 0) return k;
                for (size_t kk = 0; kk < items->kinds.size(); kk++)   // another kind the same word names, that you have
                    if ((int)kk != k && numOf(player, "has:" + items->kinds[kk].id) > 0 && !items->kinds[kk].fixed) {
                        const ItemKind &K = items->kinds[kk];
                        std::string n = lower(K.name), x = w[i];
                        bool also = false;
                        for (auto &a : K.also) if (a == x) also = true;
                        if (also || (x.size() >= 4 && (n.find(x) != std::string::npos || lower(K.id).find(x) != std::string::npos))) return (int)kk;
                    }
            }
            if (first >= 0) return first;
        }
        return -1;
    };
    auto sellsKind = [&](int k) {
        for (auto &s : p.sells) if (s.first == items->kinds[k].id) return true;
        return numOf(p.mem, "stock:" + items->kinds[k].id) > 0;   // something come in (events.txt "stock")
    };
    if (verb == "buy" || verb == "wares") {
        if (p.sells.empty()) return false;   // not a shopkeeper: their own lines answer
        restock(p, minutes);
        int k = verb == "buy" ? findKind(1) : -1;
        if (k < 0) {
            std::vector<std::string> offers;
            std::vector<std::string> ids;
            std::vector<int> kinds;
            for (auto &s : p.sells) ids.push_back(s.first);
            for (auto &m : p.mem)   // and anything that has come in besides
                if (m.first.rfind("stock:", 0) == 0 && std::find(ids.begin(), ids.end(), m.first.substr(6)) == ids.end()) ids.push_back(m.first.substr(6));
            for (auto &id : ids) {
                int kk = items->find(id);
                if (kk < 0 || numOf(p.mem, "stock:" + id) <= 0) continue;
                offers.push_back(items->kinds[kk].plural + " at " + moneyString(priceIn(p, kk, true, minutes)));
                kinds.push_back(kk);
            }
            if (offers.empty()) { say("I have nothing left to sell today. Come back tomorrow."); return true; }
            // one thing for sale, and a number ("buy 3"): that many of it
            bool counted = false;
            for (size_t i = 1; i < w.size(); i++) if (std::isdigit((unsigned char)w[i][0]) || nums.count(w[i])) counted = true;
            if (kinds.size() == 1 && counted && verb == "buy") k = kinds[0];
            else {
                std::string money = numOf(player, "money") ? moneyString(numOf(player, "money")) : std::string("no money");
                if (kinds.size() == 1) say("I can sell you " + offers[0] + " each. How many? (You have " + money + ".)");
                else {
                    std::string list;
                    for (size_t i = 0; i < offers.size(); i++) list += (i ? (i + 1 == offers.size() ? " and " : ", ") : "") + offers[i];
                    say("I can sell you " + list + ". Which will it be? (You have " + money + ".)");
                }
                talk.pendingBuy = true;
                return true;
            }
        }
        if (verb == "buy") {
            // a word that names more than one thing this shop has ("buy book" at Landini's: a blank
            // book, or the Dialogue): the most specific word decides ("buy blank book"), else ask
            std::vector<int> amb;
            int pick = -1;
            for (size_t i = 1; i < w.size() && pick < 0; i++) {
                std::vector<int> named;
                for (size_t kk = 0; kk < items->kinds.size(); kk++) {
                    const ItemKind &K2 = items->kinds[kk];
                    if (!sellsKind((int)kk) || numOf(p.mem, "stock:" + K2.id) <= 0) continue;
                    bool names = lower(K2.id) == w[i];
                    for (auto &a : K2.also) if (a == w[i]) names = true;
                    if (names) named.push_back((int)kk);
                }
                if (named.size() == 1) pick = named[0];
                else if (named.size() >= 2 && amb.empty()) amb = named;
            }
            if (pick >= 0) k = pick;
            else if (amb.size() >= 2) {
                std::string list;
                for (size_t i = 0; i < amb.size(); i++)
                    list += (i ? (i + 1 == amb.size() ? ", or " : ", ") : "") + items->kinds[amb[i]].name + " at " + moneyString(priceIn(p, amb[i], true, minutes));
                std::string q = list;
                if (!q.empty()) q[0] = (char)std::toupper((unsigned char)q[0]);
                say(q + "? Which do you mean?");
                return true;
            }
        }
        const ItemKind &K = items->kinds[k];
        if (!sellsKind(k)) { say("I do not sell " + K.plural + "."); return true; }
        auto fm = markets.find(lower(p.town));
        if (fm != markets.end() && fm->second.searchOut.count(K.id)) {   // seized by an event (the Dialogue)
            say("(Quietly.) That is forbidden now. I may not sell it, and you had better not ask for it so loud.");
            return true;
        }
        if (numOf(p.mem, "stock:" + K.id) <= 0) { say("I have no more " + K.plural + " today. Come back tomorrow."); return true; }
        long want = 1;
        {
            bool half = false;
            for (size_t i = 1; i < w.size(); i++) {
                if (w[i] == "half") half = true;
                else if (std::isdigit((unsigned char)w[i][0])) want = std::atol(w[i].c_str());
                else { auto it = nums.find(w[i]); if (it != nums.end()) want = it->second; }
            }
            if (half && want == 12) want = 6;
            want = std::max(1L, std::min(want, 50L));
        }
        long price = priceIn(p, k, true, minutes), have = numOf(player, "money");
        if (have < price) { say("That is " + moneyString(price) + ", and you have " + (have ? "only " + moneyString(have) : std::string("no money")) + "."); return true; }
        if (carriedWeight() + K.weight > Items::kCarry) { say("You could not carry it as well as all you have."); return true; }
        long got = 0, paid = 0;
        while (got < want && numOf(p.mem, "stock:" + K.id) > 0 && numOf(player, "money") >= price && carriedWeight() + K.weight <= Items::kCarry) {
            addTo(player, "money", -price);
            addTo(p.mem, "money", price);
            addTo(p.mem, "stock:" + K.id, -1);
            addTo(player, "has:" + K.id, 1);
            addTo(p.mem, "sold", 1);
            got++; paid += price;
        }
        if (got == 1) say("Here you are: " + K.name + ", for " + moneyString(price) + "." + (want > 1 ? " That is all I can let you have." : ""));
        else {
            static const char *words[] = {"", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve"};
            std::string n = got <= 12 ? words[got] : std::to_string(got);
            say("Here you are: " + n + " " + K.plural + ", for " + moneyString(paid) + "." + (got < want ? " That is all I can let you have." : ""));
        }
        return true;
    }
    if (verb == "sell") {
        if (p.sells.empty()) return false;
        int k = findKind(1, true);
        if (k < 0) { say("What would you sell me?"); return true; }
        const ItemKind &K = items->kinds[k];
        if (numOf(player, "has:" + K.id) <= 0) {   // in your own words, as for a gift
            std::string typed;
            for (size_t i = 1; i < w.size(); i++) typed += (typed.empty() ? "" : " ") + w[i];
            say("(You have no " + (typed.empty() ? K.plural : typed) + " to sell.)");
            return true;
        }
        auto mkt = markets.find(lower(p.town));
        bool wanted = mkt != markets.end() && mkt->second.wants.count(K.id) && p.role == "merchant";
        if (!sellsKind(k) && !wanted) { say("I have no use for " + K.plural + "."); return true; }
        long offer = std::max(1L, priceIn(p, k, false, minutes));
        if (numOf(p.mem, "money") < offer) { say("I have not the money for it today."); return true; }
        addTo(player, "has:" + K.id, -1);
        addTo(player, "money", offer);
        addTo(p.mem, "money", -offer);
        addTo(p.mem, "stock:" + K.id, 1);
        if (wanted) {   // the town has one more of it: the next fetches a little less
            std::string key = lower(p.town) + ":" + K.id;
            player["glut:" + key] = std::to_string(glut(lower(p.town), K.id, minutes) + 1);
            player["glutAt:" + key] = std::to_string((long)minutes);
        }
        say("I will give you " + moneyString(offer) + " for it. Done.");
        return true;
    }
    if (verb == "give") {
        long amount = 0;   // money: "give 2 sous", "give a sou", "give 6 deniers"
        for (size_t i = 1; i < w.size(); i++) {
            long unit = 0;
            const std::string &u = w[i];
            if (u == "denier" || u == "deniers") unit = 1;
            else if (u == "sou" || u == "sous") unit = 12;
            else if (u == "livre" || u == "livres") unit = 240;
            else if (u == "money" || u == "alms") unit = 12;
            if (!unit) continue;
            long n = (i > 1 && std::isdigit((unsigned char)w[i - 1][0])) ? std::atol(w[i - 1].c_str()) : 1;
            amount = n * unit;
            break;
        }
        int k = -1;
        if (!amount) {   // first among what you carry ("give letter": the letter you have), then anything;
            // of several that fit, the one they are waiting for (carrying two of
            // Mersenne's letters, "give letter" must not offer Diodati the one for Descartes)
            std::vector<int> fit;
            for (size_t i = 1; i < w.size() && fit.empty(); i++)
                for (size_t kk = 0; kk < items->kinds.size(); kk++) {
                    const ItemKind &K = items->kinds[kk];
                    if (numOf(player, "has:" + K.id) <= 0 || w[i].size() < 3) continue;
                    bool also = false;
                    for (auto &x : K.also) if (x == w[i] || (w[i].size() >= 4 && x.compare(0, w[i].size(), w[i]) == 0)) also = true;   // "give paper": the document of 1616
                    if (also || lower(K.id).find(w[i]) != std::string::npos || lower(K.name).find(w[i]) != std::string::npos || lower(K.plural).find(w[i]) != std::string::npos) fit.push_back((int)kk);   // "give copies" too
                }
            // the person's own lines first, then their trade's and everyone's (Crenet takes Mersenne's
            // packet, his own line, before the post packet every innkeeper takes)
            auto lists = p.talkLists();
            for (size_t li = 0; li < lists.size() && k < 0; li++)
                for (int kk : fit) {
                    if (k >= 0) break;
                    for (const TalkLine &tl : *lists[li]) {
                        bool wants = false;
                        for (auto &key : tl.keys) if (key == "give:" + items->kinds[kk].id) wants = true;
                        if (wants && holds(p, tl.cond, minutes)) { k = kk; break; }
                    }
                }
            if (k < 0 && !fit.empty()) k = fit[0];
        }
        if (!amount && k < 0 && w.size() == 1) {   // "give" alone: the one thing you carry that they are waiting for
            std::vector<int> wanted;
            for (size_t kk = 0; kk < items->kinds.size(); kk++) {
                if (numOf(player, "has:" + items->kinds[kk].id) <= 0) continue;
                bool waits = false;
                for (const std::vector<TalkLine> *lst : p.talkLists())
                    for (const TalkLine &tl : *lst)
                        for (auto &key : tl.keys)
                            if (!waits && key == "give:" + items->kinds[kk].id && holds(p, tl.cond, minutes)) waits = true;
                if (waits) wanted.push_back((int)kk);
            }
            if (wanted.size() == 1) k = wanted[0];
        }
        if (!amount && k < 0) k = findKind(1, true);
        if (!amount && k < 0) {
            if (w.size() == 1) {   // the prompt asks, not the person: "give " is already typed for you
                talk.pendingGive = true;
                talk.input = "give ";
                say("(Give what?)");
            } else say("(You have nothing like that.)");
            return true;
        }
        if (amount && numOf(player, "money") < amount) { say(numOf(player, "money") ? "(You have only " + moneyString(numOf(player, "money")) + ".)" : std::string("(You have no money.)")); return true; }
        if (k >= 0 && numOf(player, "has:" + items->kinds[k].id) <= 0) {
            // in the words you used, not the first kind they name ("give letter" with no letters:
            // not "You have no letters to Bodley's Librarian")
            std::string typed;
            for (size_t i = 1; i < w.size(); i++) typed += (typed.empty() ? "" : " ") + w[i];
            say("(You have no " + (typed.empty() ? items->kinds[k].plural : typed) + " to give.)");
            return true;
        }
        std::string key = amount ? "give:money" : "give:" + items->kinds[k].id;
        player["gift"] = std::to_string(amount);   // how much, in deniers: lines may test "player gift >= 60"
        const TalkLine *t = nullptr;
        for (const std::string &want : {key, std::string("give:?")})
            for (const std::vector<TalkLine> *lst : p.talkLists())
                for (const TalkLine &tl : *lst) {
                    if (t) break;
                    for (auto &kk : tl.keys)
                        if (kk == want && holds(p, tl.cond, minutes)) { t = &tl; break; }
                }
        if (!t) { say(std::string(p.sex == 'f' ? "(She" : "(He") + " does not take it.) Thank you, but keep it."); return true; }   // it stays with you
        bool take = false;
        std::vector<std::string> eff;
        for (auto &e : t->effects) { if (e == "take") take = true; else eff.push_back(e); }
        if (take) {
            if (amount) { addTo(player, "money", -amount); addTo(p.mem, "money", amount); }
            else { addTo(player, "has:" + items->kinds[k].id, -1); addTo(p.mem, "has:" + items->kinds[k].id, 1); }
            addTo(p.mem, "given:" + std::string(amount ? "money" : items->kinds[k].id), 1);
        }
        say(fill(p, t->text));
        apply(p, eff);
        if (t->keys[0] == "bye") talk.over = true;
        return true;
    }
    return false;
}

void People::startTalk(World &w, int who, double minutes) {
    if (inChat(who)) endChat(minutes);   // you interrupt them
    talk = Conversation();
    talk.who = who;
    talk.pendingGive = false;
    Person &p = people[who];
    p.mem["talks"] = std::to_string((long)num(p.mem["talks"]) + 1);
    if (const TalkLine *t = match(p, "hello", minutes)) {
        apply(p, t->effects);
        talk.lines.push_back({true, fill(p, t->text)});
    }
    // turn to face the player
    (void)w;
}

void People::ask(World &w, const std::string &input, double minutes, int youX, int youY) {
    if (talk.who < 0) return;
    Person &p = people[talk.who];
    std::string words = lower(trim(input));
    if (words.empty()) return;
    lastSaid = trim(input);
    talk.lines.push_back({false, words});
    for (size_t at; (at = words.find("holy father")) != std::string::npos; ) words.replace(at, 11, "holiness");   // the Pope, not the Holy Office
    if (talk.pendingGive) {   // after "(Give what?)", "letter" means "give letter"
        talk.pendingGive = false;
        std::string first = words.substr(0, words.find(' '));
        static const std::set<std::string> other = {"give", "bye", "buy", "sell", "pay", "no", "nothing", "never", "name", "job"};
        if (!other.count(first)) words = "give " + words;
    }
    if (talk.pendingBuy) {   // after "How many?" or "Which will it be?": a number, or a ware of the shop's, buys it
        talk.pendingBuy = false;
        std::istringstream ws(words);
        std::string first, x;
        ws >> first;
        static const std::set<std::string> other = {"buy", "sell", "give", "bye", "pay", "bet", "no", "nothing", "never", "name", "job", "wares"};
        bool buys = false;
        if (!other.count(first) && items) {
            std::istringstream all(words);
            static const std::set<std::string> counts = {"one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
                                                         "ten", "eleven", "twelve", "dozen", "half", "pair", "couple"};
            while (!buys && all >> x) {
                x.erase(std::remove_if(x.begin(), x.end(), [](char c) { return c == '?' || c == '!' || c == '.' || c == ','; }), x.end());
                if (x.empty()) continue;
                if (std::isdigit((unsigned char)x[0]) || counts.count(x)) { buys = true; break; }
                int k = items->match(x);
                if (k < 0) continue;
                const std::string &id = items->kinds[k].id;
                for (auto &s : p.sells) if (s.first == id) buys = true;
                if (numOf(p.mem, "stock:" + id) > 0) buys = true;
            }
        }
        if (buys) words = "buy " + words;
    }
    if (rideAsk && (p.role == "innkeeper" || p.role == "ostler")) {   // the post ride: a town named
        std::string reply;
        if (rideAsk((int)(&p - people.data()), words, reply)) { talk.lines.push_back({true, reply}); return; }
    }
    if (items && trade(p, words, minutes)) return;
    const TalkLine *t = nullptr;
    std::string asked;
    // the whole phrase, then each word
    t = match(p, words, minutes);
    if (t) asked = words;
    // a number in a sentence ("I owe the post 48 sous") answers a question before the other words do ("post"); keys
    // that are numbers match only whole. A number that no line answers, while a question waits for one (a "?" line
    // with a condition: Mersenne's sum), gets that line ("No. ...").
    if (!t) {
        std::vector<std::string> nums = numbersIn(words);
        for (const std::string &n : nums) {
            for (const std::vector<TalkLine> *lines : p.talkLists()) {
                for (const TalkLine &l : *lines)
                    for (const std::string &k : l.keys)
                        if ((k == n || k == "=" + n) && holds(p, l.cond, minutes)) { t = &l; break; }
                if (t) break;
            }
            if (t) { asked = t->keys[0]; break; }
        }
        if (!t && !nums.empty())
            for (const std::vector<TalkLine> *lines : p.talkLists()) {
                for (const TalkLine &l : *lines)
                    if (l.keys[0] == "?" && !l.cond.empty() && holds(p, l.cond, minutes)) { t = &l; break; }
                if (t) break;
            }
    }
    std::string known;
    // a place or a person of the town named in full, in two words or more ("the Porte Saint-Antoine")
    if (!t && knowAnswer(w, p, words, true, youX, youY, minutes, known)) { talk.lines.push_back({true, known}); return; }
    std::istringstream ss(words);
    std::string wd;
    while (!t && ss >> wd) {
        wd.erase(std::remove_if(wd.begin(), wd.end(), [](char c) { return c == '?' || c == '!' || c == '.' || c == ','; }), wd.end());
        if (wd.empty()) continue;
        t = match(p, wd, minutes);
        if (t) asked = t->keys[0];
    }
    // nobody's lines answer: what the word for their trade means, or what they know of the town
    if (!t && (tradeAnswer(p, words, minutes, known) || knowAnswer(w, p, words, false, youX, youY, minutes, known))) {
        talk.lines.push_back({true, known});
        return;
    }
    // a word they have used to you in this conversation, with nothing more to say of it: not "I know nothing about
    // that" (as with Mersenne's "packet", Monro's "Friedland", Galileo's "letter"). Whole words, or
    // the same with an "s"; not the little words of every sentence.
    if (!t) {
        static const std::set<std::string> little = {"about", "after", "again", "also", "been", "before", "come", "does",
            "done", "from", "have", "here", "into", "just", "know", "like", "made", "make", "many", "more", "much", "must",
            "only", "said", "some", "tell", "than", "that", "them", "then", "there", "they", "this", "upon", "very", "want",
            "were", "what", "when", "where", "which", "will", "with", "would", "your", "yours"};
        auto wordsOf = [](const std::string &text) {
            std::vector<std::string> out;
            std::string cur;
            for (char c : text + " ") {
                if (std::isalpha((unsigned char)c) || (c & 0x80)) cur += (char)std::tolower((unsigned char)c);
                else { if (!cur.empty()) out.push_back(cur); cur.clear(); }
            }
            return out;
        };
        bool used = false;
        for (const std::string &a : wordsOf(words)) {
            if (a.size() < 4 || little.count(a)) continue;
            for (const auto &ln : talk.lines) {
                if (!ln.first) continue;
                for (const std::string &b : wordsOf(ln.second))
                    if (a == b || a + "s" == b || b + "s" == a || a + "es" == b || b + "es" == a) { used = true; break; }
                if (used) break;
            }
            if (used) break;
        }
        if (used) { talk.lines.push_back({true, "That is all I can tell you about it."}); return; }
    }
    if (!t) t = match(p, "?", minutes);
    if (!t) { talk.lines.push_back({true, "..."}); return; }
    std::string reply = fill(p, t->text);
    // people remember what they have told you already
    const std::string &k0 = t->keys[0];
    if (t->cond.empty() && k0 != "hello" && k0 != "bye" && k0 != "?" && truthy(p.mem["asked:" + k0]) && !reply.empty()) {
        static const char *again[] = {"As I told you: ", "I said so already. ", "Again? "};
        reply = again[hash32((uint32_t)std::hash<std::string>()(p.id + k0)) % 3] + reply;
    }
    if (!reply.empty()) talk.lines.push_back({true, reply});   // (a line of effects only: the post ride's yes)
    apply(p, t->effects);
    if (!asked.empty() && t->keys[0] != "?") p.mem["asked:" + t->keys[0]] = "1";
    if (t->keys[0] == "bye") talk.over = true;
}

void People::endTalk(double minutes) {
    if (talk.who >= 0) {
        Person &p = people[talk.who];
        p.mem["met"] = "1";
        p.mem["last"] = std::to_string((long)minutes);
    }
    talk = Conversation();
}

// ------------------------------------------------------------------ what townspeople know
// When none of a person's lines answers, someone who lives in a town can still say where its
// named places are (the world's buildings with names), and who its named people are: what they
// do, where to find them, and what the town says of them (a person's "known" line). And anyone
// can say what the word for their own trade or title means (data/people/trades.txt).

// lower case without accents: U+00C0 to U+017F folded to their letters
static const char kFold[] = "aaaaaaaceeeeiiiidnooooo ouuuuytsaaaaaaaceeeeiiiidnooooo ouuuuytyaaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiiiiijjkkkllllllllllnnnnnnnnnoooooooorrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";
static std::string foldWord(const std::string &s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { o += (char)std::tolower(c); continue; }
        if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            unsigned cp = ((c & 0x1Fu) << 6) | ((unsigned char)s[i + 1] & 0x3Fu);
            i++;
            o += (cp >= 0xC0 && cp < 0x180) ? kFold[cp - 0xC0] : ' ';
            continue;
        }
        while (i + 1 < s.size() && ((unsigned char)s[i + 1] & 0xC0) == 0x80) i++;   // other letters: a gap
        o += ' ';
    }
    return o;
}
static std::vector<std::string> wordsOf(const std::string &s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : foldWord(s) + " ") {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) cur += c;
        else { if (!cur.empty()) out.push_back(cur); cur.clear(); }
    }
    return out;
}
std::vector<std::string> People::nameWords(const std::string &s) { return wordsOf(s); }
// words that name nothing by themselves: articles, "saint", ways of asking, titles of address
static bool minorWord(const std::string &w) {
    static const std::unordered_set<std::string> m = {
        "the", "and", "des", "del", "dei", "della", "dello", "delle", "degli", "dal", "van", "von",
        "der", "den", "het", "ten", "zum", "zur", "saint", "sainte", "san", "santa", "santo", "sankt",
        "where", "wheres", "what", "whats", "who", "whos", "how", "find", "way", "get", "going",
        "can", "could", "would", "tell", "about", "please", "there", "here", "which", "this", "that",
        "know", "heard", "hear", "show", "some", "any", "him", "her", "his", "them", "they", "you",
        "your", "for", "from", "with", "are", "was", "does", "did", "one", "see", "want", "look",
        "looking", "lives", "live", "father", "pere", "padre", "fra", "frate", "brother", "sister",
        "suor", "suora", "mother", "signor", "signore", "signora", "sir", "lady", "lord", "don",
        "dona", "master", "mistress", "monsieur", "madame", "mademoiselle", "herr", "frau", "mrs",
        "doctor", "seigneur", "messer", "fray", "sor"};
    return w.size() < 3 || m.count(w) > 0;
}
static bool wordFits(const std::string &asked, const std::string &w) {
    if (asked == w) return true;
    if (asked.size() >= 4 && w.compare(0, asked.size(), asked) == 0) return true;   // "chapel": the Sainte-Chapelle
    if (w.size() >= 4 && asked.size() == w.size() + 1 && asked.back() == 's' && asked.compare(0, w.size(), w) == 0) return true;   // a plural
    return false;
}
static std::string capFirst(std::string s) {
    if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
    return s;
}
static const char *dir8(float dx, float dy) {   // world y is north
    static const char *d[8] = {"east", "north-east", "north", "north-west", "west", "south-west", "south", "south-east"};
    float a = std::atan2(dy, dx) * 180 / PI_F;
    return d[((int)std::floor((a + 360 + 22.5f) / 45.0f)) % 8];
}
static float distToBld(const Building &b, float x, float y) {
    float dx = std::max({(float)b.x0 - x, 0.0f, x - (float)(b.x1 + 1)});
    float dy = std::max({(float)b.y0 - y, 0.0f, y - (float)(b.y1 + 1)});
    return std::sqrt(dx * dx + dy * dy);
}
// the well-known building nearest a spot of the town (not `not`): "by Saint-Eustache", "near the Louvre"
static std::string nearLandmark(World &w, const Settlement &s, float x, float y, const Building *notB) {
    static const std::unordered_set<std::string> kinds = {"cathedral", "church", "chapel", "palace", "fortress", "gate",
                                                         "hall", "market", "college", "gallery", "theatre", "arena", "pavilion", "place"};
    const Building *best = nullptr;
    float bd = 1e9f;
    for (const auto *list : {&w.areas, &w.buildings})
        for (const Building &b : *list) {
            if (&b == notB || b.name.empty() || !kinds.count(b.kind) || (notB && b.name == notB->name)) continue;
            if (notB && notB->kind == "place" && b.x0 >= notB->x0 && b.x1 <= notB->x1 && b.y0 >= notB->y0 && b.y1 <= notB->y1) continue;   // part of the square itself
            if (!s.inside((b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2)) continue;
            float d = distToBld(b, x, y) + (b.kind == "place" ? -3.0f : 0.0f);   // a square is the better landmark
            if (d < bd) { bd = d; best = &b; }
        }
    if (!best || bd > 24) return "";
    return (bd < 8 ? "by " : "near ") + best->name;
}
// which part of the town, and the way from where you stand
static std::string partOfTown(const Settlement &s, float cx, float cy) {
    int i = (cx - s.x0) < s.w / 3.0f ? 0 : (cx - s.x0) < s.w * 2 / 3.0f ? 1 : 2;
    int j = (cy - s.y0) < s.h / 3.0f ? 0 : (cy - s.y0) < s.h * 2 / 3.0f ? 1 : 2;
    static const char *part[3][3] = {{"the south-west quarter", "the south", "the south-east quarter"},
                                     {"the west", "the middle", "the east"},
                                     {"the north-west quarter", "the north", "the north-east quarter"}};
    return std::string("in ") + part[j][i] + " of the town";
}
// JOINED: the end of one sentence with the part of the town ("It is in the north-east quarter
// of the town; from where you stand, go south-east"), so that the two directions do not read as a contradiction
static std::string wayFromHere(const Building &b, int youX, int youY, bool joined = false) {
    float x = youX + 0.5f, y = youY + 0.5f;
    float nx = std::clamp(x, (float)b.x0, (float)b.x1 + 1), ny = std::clamp(y, (float)b.y0, (float)b.y1 + 1);
    float d = std::hypot(nx - x, ny - y);
    const char *dir = dir8((b.x0 + b.x1 + 1) * 0.5f - x, (b.y0 + b.y1 + 1) * 0.5f - y);
    std::string here = joined ? "from where you stand" : "From here";
    if (d < 6) return std::string(joined ? "it is just there" : "It is just there") + ", a few steps to the " + dir + ".";
    if (d < 25) return here + " it is close by, to the " + dir + ".";
    if (d < 70) return here + ", go " + dir + (joined ? ": it is not far." : "; it is not far.");
    if (d < 140) return here + ", go " + dir + ": a good walk.";
    return here + " it is right across the town, to the " + dir + ".";
}

void People::loadTrades(const std::string &fn) {
    std::ifstream in(fn);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t c = line.find(':');
        if (c == std::string::npos) continue;
        std::string text = trim(line.substr(c + 1));
        std::istringstream ks(line.substr(0, c));
        std::string k;
        while (std::getline(ks, k, ',')) for (auto &x : wordsOf(k)) trades[x] = text;
    }
}

// asked a word of their own trade or title ("beadle", "ostler"): what it means
bool People::tradeAnswer(const Person &p, const std::string &words, double minutes, std::string &out) const {
    std::vector<std::string> mine;
    for (auto &x : wordsOf(p.title)) if (!minorWord(x)) mine.push_back(x);
    for (auto &a : wordsOf(words)) {
        if (minorWord(a)) continue;
        for (auto &m : mine) {
            if (!wordFits(a, m)) continue;
            auto it = trades.find(m);
            if (it != trades.end()) { out = fill(p, it->second); return true; }
            // no entry: what they say of their work, unless it is everyone's "I am a ..."
            const TalkLine *job = match(p, "job", minutes);
            if (job && job->text.find("$role") == std::string::npos) { out = fill(p, job->text); return true; }
        }
    }
    return false;
}

// ---- hours in words
static std::string spokenTime(int t, std::string *part) {   // t: minutes in the day, a multiple of 30
    static const char *hw[] = {"twelve", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven"};
    int h = t / 60, half = t % 60;
    *part = "";
    if (t == 0) return "midnight";
    if (t == 720) return "noon";
    *part = t < 720 ? "in the morning" : t <= 1020 ? "in the afternoon" : t < 1260 ? "in the evening" : "at night";   // five o'clock is still the afternoon
    return std::string(half ? "half past " : "") + hw[h % 12];
}

std::string People::spokenSpan(int from, int to) {
    if (to - from >= 20 * 60) return "at any hour";
    auto r = [](int m) { m = (int)std::lround(m / 30.0) * 30; return ((m % 1440) + 1440) % 1440; };
    std::string pa, pb;
    std::string a = spokenTime(r(from), &pa), b = spokenTime(r(to), &pb);
    if (!pa.empty() && pa == pb) return "from " + a + " till " + b + " " + pb;
    return "from " + a + (pa.empty() ? "" : " " + pa) + " till " + b + (pb.empty() ? "" : " " + pb);
}

std::string People::spokenSpans(const std::vector<std::pair<int, int>> &spans) {
    for (auto &sp : spans) if (sp.second - sp.first >= 20 * 60) return "at any hour";
    std::string out;
    for (size_t k = 0; k < spans.size(); k++) out += (k ? ", and " : "") + spokenSpan(spans[k].first, spans[k].second);
    return out;
}

// a place or a named person of the town they live in
// where someone works, in words: "at the Minims", or by the kind of work ("at the forge")
static std::string workPhrase(World &w, const Person &q, Building **wbOut = nullptr) {
    Building *wb = w.buildingAt(q.workX, q.workY);
    if (wbOut) *wbOut = wb;
    if (wb && !wb->name.empty()) return "at " + wb->name;
    static const std::map<std::string, std::string> kw = {
        {"forge", "at the forge"}, {"shop", "at the shop"}, {"bakery", "at the bakery"}, {"inn", "at the inn"},
        {"church", "in the church"}, {"altar", "in the church"}, {"college", "at the college"},
        {"theatre", "at the theatre"}, {"fortress", "at the fortress"}, {"gate", "at the gate"},
        {"market", "in the market"}, {"square", "in the square"}, {"quay", "on the quays"}, {"bridge", "on the bridge"},
        {"field", "in the fields"}, {"yard", "in the yard, with the hens"}, {"woods", "in the woods"}, {"road", "on the road"}, {"mine", "in the quarries"}};
    std::string k = q.workKind.substr(0, q.workKind.find(' '));
    auto it = kw.find(k);
    return it != kw.end() ? it->second : "at work";
}

bool People::knowAnswer(World &w, const Person &p, const std::string &words, bool fullOnly, int youX, int youY, double minutes, std::string &out) const {
    const Settlement *s = nullptr;
    for (auto &st : w.settlements) if (lower(st.name) == lower(p.town)) { s = &st; break; }
    if (!s) return false;
    std::vector<std::string> asked;
    for (auto &x : wordsOf(words)) if (!minorWord(x)) asked.push_back(x);
    if (asked.empty() || (fullOnly && asked.size() < 2)) return false;
    struct Cand { std::string name; const Building *bld = nullptr; int person = -1, matched = 0, extra = 0; float dist = 0; };
    auto score = [&](Cand &c) {
        std::vector<std::string> nw;
        for (auto &x : wordsOf(c.name)) if (!minorWord(x)) nw.push_back(x);
        for (auto &a : asked) for (auto &n : nw) if (wordFits(a, n)) { c.matched++; break; }
        c.extra = (int)nw.size() - c.matched;
        int need = fullOnly ? (int)asked.size() : std::max(1, (int)asked.size() - 1);
        return c.matched >= need;
    };
    std::vector<Cand> cands;
    for (const auto *list : {&w.buildings, &w.areas})
        for (const Building &b : *list) {
            if (b.name.empty() || !s->inside((b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2)) continue;
            Cand c; c.name = b.name; c.bld = &b; c.dist = distToBld(b, youX + 0.5f, youY + 0.5f);
            if (score(c)) cands.push_back(c);
        }
    for (size_t i = 0; i < people.size(); i++) {
        const Person &q = people[i];
        if (!q.named() || q.name.empty() || q.town != p.town || q.mem.count("family") || q.dead) continue;
        Cand c; c.name = q.name; c.person = (int)i;
        if (score(c)) cands.push_back(c);
    }
    // nothing by that name: someone of that trade ("stationer": Nicolas Le Gras, a stationer)
    bool byTrade = false;
    if (cands.empty() && !fullOnly)
        for (size_t i = 0; i < people.size(); i++) {
            const Person &q = people[i];
            if (!q.named() || q.name.empty() || q.town != p.town || q.mem.count("family") || q.dead || &q == &p) continue;
            Cand c; c.name = q.title; c.person = (int)i;
            // every word asked, for a trade: "map-seller" is not the colour-seller (at Venice)
            if (!score(c) || c.matched < (int)asked.size()) continue;
            c.name = q.name;
            cands.push_back(c);
            byTrade = true;
        }
    // nothing by that name, nor a named one's trade: one of the town's people of that trade with no name of their own,
    // the nearest ("the herdsman in the market, down in the city": Suor Maria Celeste sends you to him)
    bool byCrowd = false;
    auto nearestCrowd = [&](std::function<bool(const Person &)> want) {
        int bi = -1;
        float bd = 1e9f;
        for (size_t i = 0; i < people.size(); i++) {
            const Person &q = people[i];
            if (q.named() || q.title.empty() || q.town != p.town || q.mem.count("family") || q.dead || &q == &p || !want(q)) continue;
            float d = q.hasWork ? std::abs(q.workX - youX) + std::abs(q.workY - youY) : std::abs(q.x - youX) + std::abs(q.y - youY);
            if (d < bd) { bd = d; bi = (int)i; }
        }
        return bi;
    };
    if (cands.empty() && !fullOnly) {
        int bi = nearestCrowd([&](const Person &q) { Cand c; c.name = q.title; return score(c) && c.matched >= (int)asked.size(); });
        if (bi >= 0) { Cand c; c.name = people[bi].title; c.person = bi; cands.push_back(c); byCrowd = true; }
    }
    // nothing by that name or trade: someone of the town who sells it ("paper", "buy bread")
    bool bySale = false;
    std::string saleName;
    if (cands.empty() && !fullOnly && items) {
        for (auto &wd : asked) {
            int k = items->match(wd, 0);   // a whole word only: "venice" is not the venison
            if (k < 0) continue;
            for (size_t i = 0; i < people.size(); i++) {
                const Person &q = people[i];
                if (!q.named() || q.name.empty() || q.town != p.town || q.dead || &q == &p) continue;
                bool sells = false;
                for (auto &sl : q.sells) if (sl.first == items->kinds[k].id) sells = true;
                if (!sells) continue;
                Cand c; c.name = q.name; c.person = (int)i;
                cands.push_back(c);
            }
            if (cands.empty()) {   // nobody named sells it: one of the town's people who does ("milk": the herdsman in the market)
                int bi = nearestCrowd([&](const Person &q) { for (auto &sl : q.sells) if (sl.first == items->kinds[k].id) return true; return false; });
                if (bi >= 0) { Cand c; c.name = people[bi].title; c.person = bi; cands.push_back(c); byCrowd = true; }
            }
            if (!cands.empty()) { bySale = true; saleName = items->kinds[k].plural; break; }
        }
    }
    if (cands.empty()) {
        // nothing here: a named, living person of another town whom you already know of (never a new name)
        std::vector<Cand> far;
        for (size_t i = 0; i < people.size(); i++) {
            const Person &q = people[i];
            if (!q.named() || q.name.empty() || q.town == p.town || q.mem.count("family") || q.dead) continue;
            if (!q.diedIf.empty() && holds(q, q.diedIf, minutes)) {   // dead by the story: what is said of it, if asked by name
                Cand c; c.name = q.name; c.person = (int)i;
                if (score(c)) far.push_back(c);
                continue;
            }
            auto kn = player.find("knows:" + q.id);
            if (!((kn != player.end() && truthy(kn->second)) || numOf(q.mem, "talks") > 0)) continue;
            Cand c; c.name = q.name; c.person = (int)i;
            if (score(c)) far.push_back(c);
        }
        if (far.empty()) return false;
        int bm = 0;
        for (auto &c : far) bm = std::max(bm, c.matched);
        int be = 1 << 30;
        for (auto &c : far) if (c.matched == bm) be = std::min(be, c.extra);
        std::vector<Cand> best;
        for (auto &c : far) {
            if (c.matched != bm || c.extra != be) continue;
            bool dup = false;   // two of one name (Galileo at Arcetri, and in Rome): the one who is not away
            for (auto &b : best) if (b.name == c.name) { dup = true; if (people[b.person].away && !people[c.person].away) b = c; }
            if (!dup) best.push_back(c);
        }
        if (best.size() > 1) {
            std::string list;
            size_t n = std::min<size_t>(best.size(), 4);
            for (size_t i = 0; i < n; i++) list += (i ? (i + 1 == n && best.size() <= 4 ? " or " : ", ") : "") + best[i].name;
            if (best.size() > 4) list += ", or another";
            out = "Which do you mean: " + list + "?";
            return true;
        }
        const Person &q = people[best[0].person];
        if (!q.diedIf.empty() && holds(q, q.diedIf, minutes)) { out = q.diedText.empty() ? q.name + " is dead." : q.diedText; return true; }
        bool she = q.sex == 'f';
        std::string he = she ? "she" : "he", him = she ? "her" : "him";
        out = q.name + "? They say " + he + " is " + q.title + ", at " + q.town + ".";
        Building *wb = q.hasWork ? w.buildingAt(q.workX, q.workY) : nullptr;
        if (wb && !wb->name.empty()) out += " Ask for " + him + " at " + wb->name + ".";
        return true;
    }
    if (bySale && cands.size() > 1) {   // several sell it: the nearest
        auto d = [&](const Cand &c) { const Person &q = people[c.person]; return std::abs(q.x - youX) + std::abs(q.y - youY); };
        std::sort(cands.begin(), cands.end(), [&](const Cand &a, const Cand &b) { return d(a) < d(b); });
        cands.resize(1);
    }
    int bestM = 0;
    for (auto &c : cands) bestM = std::max(bestM, c.matched);
    int bestE = 1 << 30;
    for (auto &c : cands) if (c.matched == bestM) bestE = std::min(bestE, c.extra);
    std::vector<Cand> top;
    for (auto &c : cands) {
        if (c.matched != bestM || c.extra != bestE) continue;
        bool dup = false;
        for (auto &t : top) if (t.name == c.name) { dup = true; if (c.dist < t.dist) t = c; }
        if (!dup) top.push_back(c);
    }
    if (top.size() > 1) {   // several of that name
        std::string list;
        size_t n = std::min<size_t>(top.size(), 4);
        for (size_t i = 0; i < n; i++) list += (i ? (i + 1 == n && top.size() <= 4 ? " or " : ", ") : "") + top[i].name;
        if (top.size() > 4) list += ", or another";
        out = "Which do you mean: " + list + "?";
        return true;
    }
    const Cand &c = top[0];
    bool youHere = s->inside(youX, youY);
    if (c.bld) {
        const Building &b = *c.bld;
        std::string head = capFirst(b.name) + "? ";
        if (youHere && youX >= b.x0 && youX <= b.x1 && youY >= b.y0 && youY <= b.y1) {
            out = head + (b.kind == "gate" ? "You are standing in the gateway." : b.kind == "place" ? "You are standing in it: look about you." : "You are standing in it.");
            return true;
        }
        float cx = (b.x0 + b.x1 + 1) * 0.5f, cy = (b.y0 + b.y1 + 1) * 0.5f;
        std::string near = nearLandmark(w, *s, cx, cy, &b);
        std::string where;
        if (s->hamlet) where = "";
        else if (b.kind == "gate") where = std::string("It is the gate on the ") + dir8(cx - (s->x0 + s->w * 0.5f), cy - (s->y0 + s->h * 0.5f)) + " side of the town";
        else where = "It is " + partOfTown(*s, cx, cy);
        if (!where.empty()) where += near.empty() ? "" : ", " + near;
        if (youHere) out = head + (where.empty() ? wayFromHere(b, youX, youY) : where + "; " + wayFromHere(b, youX, youY, true));
        else out = head + (where.empty() ? "" : where + ". ") + "It is in " + s->name + ".";
        return true;
    }
    const Person &q = people[c.person];
    if (&q == &p) { out = "That is me."; return true; }
    if (!q.diedIf.empty() && holds(q, q.diedIf, minutes)) { out = q.diedText.empty() ? q.name + " is dead." : q.diedText; return true; }
    bool she = q.sex == 'f';
    std::string he = she ? "she" : "he", him = she ? "her" : "him", his = she ? "her" : "his";
    std::string say = byCrowd ? (bySale ? capFirst(saleName) + "? " + capFirst(q.title) + " sells them." : "There is " + q.title + ".")
                    : bySale ? capFirst(saleName) + "? " + q.name + " sells them: " + he + " is " + q.title + "."
                    : byTrade ? "There is " + q.name + ": " + he + " is " + q.title + "." : q.name + "? " + capFirst(he) + " is " + q.title + ".";
    if (!q.known.empty()) say += " " + q.known;
    if (q.onRoad || !q.travelTo.empty()) {
        say += " But " + he + " has gone to " + (q.travelTo.empty() ? std::string("another town") : q.travelTo) + ", they say.";
    } else if (q.away) {
        say += " But " + he + " is not here these days.";
    } else if (q.hasWork) {
        Building *wb = nullptr;
        std::string at = workPhrase(w, q, &wb);
        std::string nl = (wb && !wb->name.empty()) ? "" : nearLandmark(w, *s, q.workX + 0.5f, q.workY + 0.5f, wb);
        // the hours at the place of work, from the day as it is now (the key is as whereToFind writes it)
        std::string key = at + (nl.empty() ? "" : ", " + nl);
        std::string spans;
        for (auto &wh : dayPlaces(w, q, minutes)) if (wh.first == key) spans = spokenSpans(wh.second);
        say += " You will find " + him + " " + key + (spans.empty() ? "" : (nl.empty() ? " " : ", ") + spans) + ".";
        if (q.activity == "sleep" && q.lying) say += " At this hour " + he + " will be asleep.";
        if (youHere) {
            Building tmp; tmp.x0 = tmp.x1 = q.workX; tmp.y0 = tmp.y1 = q.workY;
            if (wb) tmp = *wb;
            std::string way = wayFromHere(tmp, youX, youY);
            if (way.rfind("It is just there", 0) == 0) way = "That is just here.";
            say += " " + way;
        }
    } else {
        std::string nl;
        if (q.home > 0 && q.home <= (int)w.buildings.size()) {
            const Building &hb = w.buildings[q.home - 1];
            nl = nearLandmark(w, *s, (hb.x0 + hb.x1 + 1) * 0.5f, (hb.y0 + hb.y1 + 1) * 0.5f, &hb);
        }
        say += nl.empty() ? " " + capFirst(he) + " is about the town, most days." : " " + capFirst(he) + " lives " + nl + ".";
    }
    (void)his;
    out = say;
    return true;
}

std::string People::townKey(const std::string &town) {
    std::string k;
    for (char c : town) k += c == ' ' ? '-' : (char)std::tolower((unsigned char)c);
    return k;
}

// the day of someone who is neither dead, away nor on the road, as it is now: the places they are
// at (in words) and the stretches of minutes they are there, from the schedule entries whose conditions hold
People::DayPlaces People::dayPlaces(World &w, const Person &q, double minutes) const {
    const Settlement *s = nullptr;
    for (auto &st : w.settlements) if (lower(st.name) == lower(q.town)) { s = &st; break; }
    auto homePhrase = [&]() {
        std::string at = "at home";
        if (s && q.home > 0 && q.home <= (int)w.buildings.size()) {
            const Building &hb = w.buildings[q.home - 1];
            if (!hb.name.empty()) return "at " + hb.name;   // Il Gioiello; or the shop he lives over
            std::string nl = nearLandmark(w, *s, (hb.x0 + hb.x1 + 1) * 0.5f, (hb.y0 + hb.y1 + 1) * 0.5f, &hb);
            if (!nl.empty()) at += ", " + nl;
        }
        return at;
    };
    // the day as it is now (the entries whose conditions hold), in order
    std::vector<const SchedEntry *> day;
    for (const SchedEntry &e : q.schedule) if (e.cond.empty() || holds(q, e.cond, minutes)) day.push_back(&e);
    std::stable_sort(day.begin(), day.end(), [](const SchedEntry *a, const SchedEntry *b) { return a->minute < b->minute; });
    DayPlaces where;   // a place, and its hours
    // where they set out from (an inn or a church is the one nearest it, as People::goal chooses)
    float rx = q.x + 0.5f, ry = q.y + 0.5f;
    auto atHome = [&]() {
        if (q.home > 0 && q.home <= (int)w.buildings.size()) { const Building &hb = w.buildings[q.home - 1]; rx = (hb.x0 + hb.x1 + 1) * 0.5f; ry = (hb.y0 + hb.y1 + 1) * 0.5f; }
    };
    atHome();
    auto nearestOf = [&](std::initializer_list<const char *> kinds) -> const Building * {
        const Building *best = nullptr;
        float bd = 1e9f;
        for (auto &b : w.buildings) {
            bool k = false;
            for (const char *x : kinds) if (b.kind == x) k = true;
            if (!k || !s || !s->inside((b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2)) continue;
            float d = distToBld(b, rx, ry);
            if (d < bd) { bd = d; best = &b; }
        }
        return best;
    };
    for (size_t i = 0; i < day.size(); i++) {
        const SchedEntry &e = *day[i];
        int from = e.minute, to = i + 1 < day.size() ? day[i + 1]->minute : day[0]->minute + 1440;
        if (to - from < 30) continue;   // a moment on the way to bed
        std::string a = e.activity, at;
        if (a == "work") {
            Building *wb = nullptr;
            at = q.hasWork ? workPhrase(w, q, &wb) : "at work";
            if (q.hasWork && (!wb || wb->name.empty()) && s) {   // no name for the place: what it is near ("at the shop, by Saint-Eustache")
                std::string nl = nearLandmark(w, *s, q.workX + 0.5f, q.workY + 0.5f, wb);
                if (!nl.empty()) at += ", " + nl;
            }
            if (q.hasWork) { rx = q.workX + 0.5f; ry = q.workY + 0.5f; }
        }
        else if (a == "sleep" && !e.place.empty()) {}   // asleep, elsewhere: nothing to say
        else if (a == "home" || a == "sleep") { atHome(); if (a == "home") at = homePhrase(); }
        else if (a == "wander") at = "about the town";
        else if (a == "go") {
            std::string pl = e.place;
            if (pl.rfind("landmark ", 0) == 0) {
                std::string nm = trim(pl.substr(9));
                bool up = nm.size() > 9 && nm.compare(nm.size() - 9, 9, " upstairs") == 0;
                if (up) nm = trim(nm.substr(0, nm.size() - 9));
                std::string want = lower(nm);
                at = "at the " + nm;
                bool named = false;
                for (int pass = 0; pass < 2 && !named; pass++)   // not a tower or a gate first, as placeFor
                    for (auto &b : w.buildings)
                        if (!b.name.empty() && lower(b.name).find(want) != std::string::npos && s && s->inside((b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2) &&
                            (pass == 1 || (b.kind != "tower" && b.kind != "gate"))) { at = "at " + b.name; named = true; break; }
                if (up && at.rfind("at ", 0) == 0) at = "upstairs in " + at.substr(3);   // "upstairs in the palace of the Holy Office"
            }
            else if (pl == "church") { const Building *b = nearestOf({"church", "cathedral", "chapel"}); at = b && !b->name.empty() ? "at " + b->name : "at church"; }
            else if (pl == "inn") { const Building *b = nearestOf({"inn"}); at = b && !b->name.empty() ? "at " + b->name : "at the inn"; }
            else if (pl == "market") at = "in the market";
            else if (pl.rfind("home-of ", 0) == 0) {   // in another's house (Morales hidden at Mona Lisabetta's)
                std::string id = trim(pl.substr(8)), pre = q.id.find(':') != std::string::npos ? q.id.substr(0, q.id.find(':') + 1) : "";
                const Person *o = nullptr;
                for (auto &x : people) if (x.id == id || x.id == pre + id) { o = &x; break; }
                at = o ? "at " + label(*o) + "'s house" : "in someone else's house";
            }
            else if (!pl.empty()) at = "at the " + pl;
        }
        if (at.empty()) continue;   // asleep, at dinner, away, on the road
        bool found = false;
        for (auto &wh : where)
            if (wh.first == at) {
                found = true;
                if (!wh.second.empty() && wh.second.back().second == from) wh.second.back().second = to;   // one stretch: the shop he lives over
                else wh.second.push_back({from, to});
            }
        if (!found) where.push_back({at, {{from, to}}});
    }
    return where;
}

std::string People::whereToFind(World &w, const Person &q, double minutes) const {
    const Settlement *s = nullptr;
    for (auto &st : w.settlements) if (lower(st.name) == lower(q.town)) { s = &st; break; }
    bool she = q.sex == 'f';
    if (q.dead) return she ? "She is dead." : "He is dead.";
    if (q.onRoad || !q.travelTo.empty()) return "Gone to " + (q.travelTo.empty() ? std::string("another town") : q.travelTo) + ", they say.";
    if (q.away) {   // away for good, if what keeps them away is a death ("0:00 away if player celeste-dead")
        const SchedEntry *e = q.actIndex >= 0 && q.actIndex < (int)q.schedule.size() ? &q.schedule[q.actIndex] : nullptr;
        if (e && e->cond.find("-dead") != std::string::npos && e->cond.rfind("not ", 0) != 0) return she ? "She is dead." : "He is dead.";
        return "Not here these days.";
    }
    auto hhmm = [](int m) { m = (int)std::lround(m / 5.0) * 5; m = ((m % 1440) + 1440) % 1440; char b[8]; std::snprintf(b, sizeof b, "%d:%02d", m / 60, m % 60); return std::string(b); };
    auto homePhrase = [&]() {
        std::string at = "at home";
        if (s && q.home > 0 && q.home <= (int)w.buildings.size()) {
            const Building &hb = w.buildings[q.home - 1];
            if (!hb.name.empty()) return "at " + hb.name;   // Il Gioiello; or the shop he lives over
            std::string nl = nearLandmark(w, *s, (hb.x0 + hb.x1 + 1) * 0.5f, (hb.y0 + hb.y1 + 1) * 0.5f, &hb);
            if (!nl.empty()) at += ", " + nl;
        }
        return at;
    };
    DayPlaces where = dayPlaces(w, q, minutes);
    std::string out;
    for (auto &wh : where) {
        std::string spans;
        for (size_t k = 0; k < wh.second.size(); k++)
            spans += (k ? (k + 1 == wh.second.size() ? " and " : ", ") : "") + hhmm(wh.second[k].first) + " to " + hhmm(wh.second[k].second);
        out += (out.empty() ? "" : "; ") + wh.first + ", " + spans;
    }
    if (!out.empty()) return capFirst(out) + ".";
    if (q.hasWork) return capFirst(workPhrase(w, q)) + ", most days.";
    return capFirst(homePhrase()) + ", or about the town.";
}

std::vector<std::pair<int, std::string>> People::townPages(World &w, double minutes, const std::string &here,
                                                           std::vector<TownPlanInfo> *plans) const {
    std::vector<const Settlement *> towns;
    for (auto &st : w.settlements)
        if (player.count("been:" + townKey(st.name)) && truthy(player.at("been:" + townKey(st.name)))) towns.push_back(&st);
    std::stable_sort(towns.begin(), towns.end(), [&](const Settlement *a, const Settlement *b) {
        if ((a->name == here) != (b->name == here)) return a->name == here;   // where you are, first
        return lower(a->name) < lower(b->name);
    });
    std::vector<std::pair<int, std::string>> out;
    std::vector<std::string> passed;   // been to, and no more: one line at the end
    for (const Settlement *s : towns) {
        std::string key = townKey(s->name);
        // the places, in the order you first went into them
        std::vector<std::pair<double, std::string>> places;
        std::string pre = "visited:" + key + ":";
        for (auto it = player.lower_bound(pre); it != player.end() && it->first.compare(0, pre.size(), pre) == 0; ++it)
            places.push_back({num(it->second), it->first.substr(pre.size())});
        std::sort(places.begin(), places.end());
        // the people you have spoken to here (the named, not the crowd), and those you only know of
        std::vector<const Person *> met, heard;
        for (const Person &q : people)
            if (q.named() && q.town == s->name) {
                bool knows = truthy(player.count("knows:" + q.id) ? player.at("knows:" + q.id) : "");
                if (numOf(q.mem, "talks") > 0 || (knows && q.mem.count("family"))) met.push_back(&q);
                else if (knows) heard.push_back(&q);
            }
        // only passed through: gathered into one line after the others, not a page each
        if (places.empty() && met.empty() && heard.empty()) { passed.push_back(s->name); continue; }
        out.push_back({4, s->name});
        TownPlanInfo plan;
        plan.town = s->name; plan.x0 = s->x0; plan.y0 = s->y0; plan.w = s->w; plan.h = s->h;
        out.push_back({8, s->name});
        if (!places.empty()) {
            out.push_back({5, "Places you have been"});
            for (auto &pl : places) {
                std::string line = std::to_string(plan.marks.size() + 1) + ". " + capFirst(pl.second);
                bool marked = false;
                for (auto &b : w.buildings)
                    if (b.name == pl.second && s->inside((b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2)) {
                        float cx = (b.x0 + b.x1 + 1) * 0.5f, cy = (b.y0 + b.y1 + 1) * 0.5f;
                        plan.marks.push_back({cx, cy});
                        marked = true;
                        std::string nl = nearLandmark(w, *s, cx, cy, &b);
                        std::string part = s->hamlet ? "" : partOfTown(*s, cx, cy);
                        std::string where = part + (part.empty() || nl.empty() ? "" : ", ") + nl;
                        if (!where.empty()) line += ": " + where;
                        break;
                    }
                if (!marked) { plan.marks.push_back({-1e9f, -1e9f}); }   // keep the numbers in step (a place no longer on the map)
                out.push_back({6, line + "."});
            }
        }
        if (!met.empty()) {
            out.push_back({5, "People you have spoken to"});
            for (const Person *q : met) {
                std::string l = label(*q);
                if (l != q->title && !q->title.empty() && !q->mem.count("family")) l += ", " + q->title;
                out.push_back({6, capFirst(l) + "."});
                out.push_back({7, whereToFind(w, *q, minutes)});
            }
        }
        if (!heard.empty()) {
            out.push_back({5, "People you have heard of"});
            for (const Person *q : heard) {
                std::string l = label(*q);
                if (l != q->title && !q->title.empty()) l += ", " + q->title;
                out.push_back({6, capFirst(l) + "."});
                out.push_back({7, whereToFind(w, *q, minutes)});
            }
        }
        if (plans) plans->push_back(plan);
    }
    if (!passed.empty()) {
        std::string line;
        for (size_t i = 0; i < passed.size(); i++) line += (i ? (i + 1 == passed.size() ? " and " : ", ") : "") + passed[i];
        out.push_back({4, "Passed through"});
        out.push_back({6, capFirst(line) + "."});
    }
    // the towns you have not been to, where you know of someone: after the others
    std::vector<const Settlement *> far;
    for (auto &st : w.settlements)
        if (!(player.count("been:" + townKey(st.name)) && truthy(player.at("been:" + townKey(st.name))))) far.push_back(&st);
    std::sort(far.begin(), far.end(), [&](const Settlement *a, const Settlement *b) { return lower(a->name) < lower(b->name); });
    for (const Settlement *s : far) {
        std::vector<const Person *> heard;
        for (const Person &q : people)
            if (q.named() && q.town == s->name && truthy(player.count("knows:" + q.id) ? player.at("knows:" + q.id) : "")) heard.push_back(&q);
        if (heard.empty()) continue;
        out.push_back({4, s->name});
        out.push_back({6, "You have not been here yet."});
        out.push_back({5, "People you have heard of"});
        for (const Person *q : heard) {
            std::string l = label(*q);
            if (l != q->title && !q->title.empty()) l += ", " + q->title;
            out.push_back({6, capFirst(l) + "."});
            out.push_back({7, whereToFind(w, *q, minutes)});
        }
    }
    return out;
}

// ------------------------------------------------------------------ saving

static std::string esc(const std::string &s) {
    std::string o;
    for (char c : s) o += (c == ' ') ? '_' : c;
    return o.empty() ? "-" : o;
}
static std::string unesc(const std::string &s) {
    if (s == "-") return "";
    std::string o;
    for (char c : s) o += (c == '_') ? ' ' : c;
    return o;
}

void People::save(std::ostream &out) const {
    for (auto &kv : player) out << "player " << esc(kv.first) << " " << esc(kv.second) << "\n";
    for (auto &h : heard) out << "heard\t" << h.conv << "\t" << (long)h.minutes << "\t" << h.where << "\t" << h.who << "\t" << h.text << "\n";
    for (auto &p : people) {
        // ids with spaces (the people of hamlets and post houses) are written with _ for a space
        out << "person " << esc(p.id) << " " << p.x << " " << p.y << " " << p.z << "\n";
        for (auto &kv : p.mem) out << "mem " << esc(p.id) << " " << esc(kv.first) << " " << esc(kv.second) << "\n";
    }
}

void People::loadSave(std::istream &in) {
    std::map<std::string, int> byId;
    for (size_t i = 0; i < people.size(); i++) byId[people[i].id] = (int)i;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("heard\t", 0) == 0) {
            std::istringstream hs(line.substr(6));
            HeardLine h;
            std::string c, m;
            std::getline(hs, c, '\t'); std::getline(hs, m, '\t');
            std::getline(hs, h.where, '\t'); std::getline(hs, h.who, '\t'); std::getline(hs, h.text);
            h.conv = std::atoi(c.c_str()); h.minutes = std::atof(m.c_str());
            heard.push_back(h);
            continue;
        }
        std::istringstream ss(line);
        std::string w;
        ss >> w;
        if (w == "player") { std::string k, v; ss >> k >> v; player[unesc(k)] = unesc(v); }
        else if (w == "mem") {
            std::string id, k, v;
            ss >> id >> k >> v;
            auto it = byId.find(unesc(id));
            if (it == byId.end()) {
                // an older save wrote ids with their spaces: the longest
                // id the rest of the line begins with, then the key and the value
                std::string rest = line.substr(4);
                size_t best = 0;
                for (auto &b : byId)
                    if (b.first.size() > best && rest.size() > b.first.size() && rest.compare(0, b.first.size(), b.first) == 0 &&
                        rest[b.first.size()] == ' ') { best = b.first.size(); it = byId.find(b.first); }
                if (best) { std::istringstream rs(rest.substr(best)); rs >> k >> v; }
            }
            if (it != byId.end()) people[it->second].mem[unesc(k)] = unesc(v);
        }
    }
    for (auto &e : events)   // what the events that have happened did to the markets
        if (player.count("event:" + e.id) && !player.count("failed:" + e.id)) marketEffects(e);
    if (std::getenv("GALILEO_DEBUG_MARKET"))
        for (auto &m : markets)
            for (auto &w : m.second.wants) std::fprintf(stderr, "market %s wants %s %.2f\n", m.first.c_str(), w.first.c_str(), w.second);
}
