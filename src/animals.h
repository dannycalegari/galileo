// Animals: herds and flocks on the grass outside the towns, hens and geese round farms and
// villages, horses tied up by the doors of inns, dogs and cats in the streets. A light system of
// its own (not the people's): each animal has a home ground, walks to a spot in it, grazes or
// sits there a while, and lies down at night. What they look like, and where they live, is in
// data/animals.txt. A town's animals are put out when you first come near it.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "vmath.h"

struct World;
struct MeshBuf;

struct AnimalLook {
    bool bird = false;
    float len = 1.4f, ht = 1.1f;          // the body's length, the height at the shoulder (tiles)
    vec3 col{0.5f, 0.35f, 0.2f};
    vec3 patch{-1, -1, -1}, head{-1, -1, -1};   // patches on the body, the head's colour (-1: none, the body's)
    bool horns = false, wool = false, mane = false, snout = false, ears = false, longNeck = false, comb = false, tailUp = false;
    bool saddle = false;                  // a horse you have hired or bought
    bool antlers = false;                 // a stag's
};

// drawn in mesh.cc, facing +y before turning to `facing`
void buildAnimal(MeshBuf &m, const AnimalLook &a, vec3 pos, float facing, float phase, float headDown, bool lying);

struct AnimalKind {
    std::string id, name, plural, look;   // look: what L says of one
    AnimalLook body;
    std::vector<vec3> colours;            // one of these, animal by animal
    float speed = 0.5f;                   // tiles a second, walking
    bool flock = false;                   // keeps close to the others of its herd
    bool lies = true;                     // lies down at night (else stands, or sits)
    bool roams = false;                   // walks about the streets (dogs, cats): short waits
    // the wild ones (wolves, bears, deer): never put out by the towns' rules, but met in the
    // country (Game::wildStep in main.cc), and gone again once you are far from them
    bool wild = false;
    float hostile = 0;                    // "hostile N": goes for you once you are within N tiles (0: never)
    float shy = 0;                        // "shy N": runs from you within N tiles (deer)
    float run = 0;                        // "run N": tiles a second, at a run (chasing, or running away)
    int hp = 0, skill = 0, dmg = 0;       // "fight HP SKILL DAMAGE": it can be fought (and killed)
    float flees = 0.4f;                   // "flees F": runs off when its health is below F of the whole
    bool night = false;                   // "night": about at night, and lies up by day (wolves)
    float every = 1.4f;                   // "every S": seconds between its bites or blows
    float bout = 0;                       // "bout S": it breaks off after S seconds at you (a bear's charge; 0: never)
    int biters = 0;                       // "biters N": no more than N of its herd at you at once (a pack: the rest circle)
    // what you can do with one (U and a click): the first use whose "needs" you carry
    struct Use {
        std::string verb, needs, gives, text, done;   // done: what you read if it was done too lately
        int every = 0;                                // minutes before it can be done again (0: any time)
        bool dead = false;                            // "dead": only to one that has been killed (skin it)
    };
    std::vector<Use> uses;
};

struct Animal {
    int kind = 0;
    float x = 0, y = 0, fx = 0, fy = 0;   // where it is, where it is going
    float hx = 0, hy = 0, radius = 5;     // its home ground
    float facing = 0, phase = 0, wait = 0, headDown = 0;
    bool tethered = false, lying = false, moving = false;
    int herd = -1;                        // the first animal of its herd (flocks keep near it)
    vec3 col;
    uint32_t seed = 0;
    std::string owner;                    // whose it is ("" nobody's): "a herdsman of Oxford", "the Mitre"
    std::map<std::string, double> lastUse;   // use -> when (game minutes)
    bool mine = false, hired = false;     // your own horse, or a post horse you have hired
    bool led = false;                     // yours and on a halter: it walks behind you (Game::leadAlong)
    // the wild ones (AnimalKind::wild)
    bool wildOne = false, gone = false;   // gone: a slot for the next one met
    bool dead = false, after = false;     // after: going for you
    float hp = 0, strikeT = 0, fleeT = 0, strikeAnim = 0;
    double deadAt = 0;
    float afterT = 0;                     // seconds it has been at you
    std::vector<std::pair<int, int>> path;   // the way to you, round what is in the way (tiles)
    size_t pathPos = 0;
    float repath = 0;
};

struct Animals {
    std::vector<AnimalKind> kinds;
    std::vector<Animal> list;
    struct Rule { std::string where, kind, building; int lo = 1, hi = 1; std::vector<std::string> in; };
    std::vector<Rule> rules;
    std::map<std::string, std::string> country;   // town -> country (from the people's files)
    std::vector<bool> placed;                     // settlement by settlement
    float placeTimer = 0;
    int ridden = -1;                              // the horse you are on (drawn with you, not here)
    // the wild ones' fighting: what they did to you this frame, for Game::fight (as People's)
    float playerHurt = 0;
    std::vector<std::string> notes;
    int playerSkill = 0, playerArmour = 0;
    bool playerDown = false;
    int kindOf(const std::string &id) const;
    int spawn(int kind, float x, float y, uint32_t seed);   // a wild one met in the country: its index
    void hurt(int i, float dmg, double minutes);           // you struck it (or shot it)
    void startle(float x, float y, float r);               // a shot: the shy ones within r run
    void wildStep(World &w, Animal &a, float dt, double minutes, int ptx, int pty);

    bool load(const std::string &path);
    void placeTown(World &w, int s);                               // put out one settlement's animals
    void placeNear(World &w, float px, float py, float reach);     // those of towns within reach, once
    void update(World &w, float dt, double minutes, int ptx, int pty);
    void draw(MeshBuf &m, World &w, int minx, int miny, int maxx, int maxy);
    int nearest(float x, float y, float maxd) const;              // the animal nearest a point, or -1
    std::string label(int i) const { return kinds[list[i].kind].name; }
};
