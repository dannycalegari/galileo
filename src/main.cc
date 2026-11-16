// main.cc: Galileo, a walkable Europe of 1632.
//
//   galileo                         play
//   galileo --shot out.png [...]    render one picture and exit (see usage())
//   galileo --stats                 build the world, check it, print a summary
#define GLFW_INCLUDE_NONE
#include <dirent.h>
#include <GLFW/glfw3.h>
#include "render.h"
#include "hud.h"
#include "world.h"
#include "mesh.h"
#include "noise.h"
#include "people.h"
#include "animals.h"
#include "items.h"
#include "audio.h"
#include "sound.h"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <deque>
#include <chrono>
#include <set>
#include <cctype>
#include <array>

static void usage() {
    std::printf(
        "usage: galileo [options]\n"
        "  --data DIR        data folder (default: data)\n"
        "  --scale N         tiles per cell of the old map (default 10)\n"
        "  --town NAME       start in this town (default from data/world.txt)\n"
        "  --at X,Y          start at this world tile\n"
        "  --offset DX,DY    shift the start by this many tiles\n"
        "  --time HH:MM      time of day\n"
        "  --yaw D --pitch D --zoom Z   view (degrees, degrees, half-height in tiles)\n"
        "  --size WxH        window or picture size\n"
        "  --shot FILE.png   render one picture and exit\n"
        "  --help-overlay    show the key help in the picture\n"
        "  --map             show the map of Europe in the picture\n"
        "  --roofs-off       start with all roofs lifted (key V)\n"
        "  --creator         open the screen for choosing who you are (a new game opens on it)\n"
        "  --scene N         show the Nth still scene of data/scenes.txt (they follow the choosing)\n"
        "  --bld N           start inside building N (see --stats)\n"
        "  --floor Z         start on floor Z (with --bld, or --at): 1 upstairs, -1 a cellar, -2 the quarries\n"
        "  --walk \"N5 E3\"    walk these steps first (N, NE, E, SE, S, SW, W, NW and a count)\n"
        "  --save FILE       read and write this saved game instead of saves/autosave.txt (tests)\n"
        "  --play FILE       with --walk: play by typed commands, the game kept in FILE (a new game if\n"
        "                    there is none); HELP lists the commands\n"
        "  --fps N           at most N pictures a second (60; 0 for as many as the screen allows)\n"
        "  --quit-after S    close the window after S seconds\n"
        "  --new             start afresh, not from the saved game (saves/autosave.txt)\n"
        "  --no-music        no background music\n"
        "  --testing         the keys for testing: N the next town, ] [ an hour later / earlier, Tab gallop\n"
        "  --autosave SECS   the window saves the game every SECS real seconds (default 300; 0 = off)\n"
        "  --stats           build the world, check it and print a summary\n");
}

// An autosave is due when the interval (seconds; 0 = off) has passed since the last one and the
// game is in a state that can be saved (Game::saveIsSafe); if it is not safe, it waits for the next frame.
static bool autosaveDue(double clockNow, double lastSave, double interval, bool safe) {
    return interval > 0 && clockNow - lastSave >= interval && safe;
}

struct Options {
    std::string data = "data", shot, town, portraitOut;   // portraitOut: --portrait FILE (tests)
    int w = 1440, h = 900, scale = 10;
    bool atSet = false, stats = false, help = false, map = false;
    float ax = 0, ay = 0, dx = 0, dy = 0, hour = 10.0f;
    float yaw = 22, pitch = 52, zoom = 20;
    int bld = 0, floor = 0;
    std::string walk;
    bool roofsOff = false;
    float quitAfter = 0;
    float autosave = 300;  // --autosave SECONDS: the window saves the game this often (0: never); the save on leaving is always made
    bool fresh = false;   // --new: do not continue from the saved game
    bool creator = false; // --creator: open the screen for choosing who you are (pictures, tests)
    int scene = 0;        // --scene N: show the Nth still scene of data/scenes.txt (pictures, tests)
    std::string sequence; // --sequence ID: the scenes of data/scenes/ID.txt instead (all of them, whatever their conditions)
    bool noMusic = false;
    bool testing = false; // --testing: the keys N (the next town), ] and [ (the clock), Tab (the gallop)
    std::string save;     // --save FILE: read and write this saved game instead of saves/autosave.txt
    std::string play;     // --play FILE --walk "COMMANDS": play by commands (for testing), the game kept in FILE
    float fps = 60;       // --fps N: at most N pictures a second (0: as many as the screen shows; a cap keeps the machine cool)
    bool idle = true;     // in the background (another window in front, or hidden) it draws only a few a second
};

struct ChunkGfx {
    GpuMesh solid, water;
    uint64_t used = 0;   // the frame in which it was last in view (Game::trimGfx)
};

// ------------------------------------------------------------------ calendar

static const char *MONTHS[12] = {"January", "February", "March", "April", "May", "June", "July",
                                 "August", "September", "October", "November", "December"};
static const char *DAYS[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

static bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int dim(int y, int m) {
    static const int D[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return D[m - 1] + (m == 2 && leap(y));
}
static int weekday(int y, int m, int d) {  // Gregorian, 0 = Sunday
    static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static std::string dateString(double minutes) {
    int y = 1632, m = 6, d = 15;
    long total = (long)std::floor(minutes);
    long days = total / 1440;
    int mins = (int)(total % 1440);
    for (long k = 0; k < days; k++) {
        if (++d > dim(y, m)) { d = 1; if (++m > 12) { m = 1; y++; } }
    }
    char buf[128];
    std::snprintf(buf, sizeof buf, "%s %d %s %d, %02d:%02d", DAYS[weekday(y, m, d)], d, MONTHS[m - 1], y, mins / 60, mins % 60);
    return buf;
}

static int monthAt(double minutes) {   // 1..12
    int y = 1632, m = 6, d = 15;
    long days = (long)std::floor(minutes / 1440.0);
    for (long k = 0; k < days; k++)
        if (++d > dim(y, m)) { d = 1; if (++m > 12) { m = 1; y++; } }
    return m;
}

static int yearAt(double minutes) {
    int y = 1632, m = 6, d = 15;
    long days = (long)std::floor(minutes / 1440.0);
    for (long k = 0; k < days; k++)
        if (++d > dim(y, m)) { d = 1; if (++m > 12) { m = 1; y++; } }
    return y;
}

// ------------------------------------------------------------------ the game

static std::string trimS(const std::string &s) {
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
static std::string lowerS(std::string s) {
    for (auto &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

struct Game {
    Options opt;
    World world;
    Renderer R;
    Hud hud;
    std::unordered_map<int64_t, ChunkGfx> gfx;
    std::vector<GpuMesh> townMesh;
    std::vector<std::pair<int, int>> townBox;  // building index ranges per town
    GpuMesh figure, crowd;
    People people;
    Animals animals;   // herds, flocks, horses at the inns, dogs and cats (data/animals.txt)
    int riding = -1;   // the horse you are on (an index into animals.list), or -1
    double horseDay = -1;   // the last morning your own horse's night was reckoned
    // things
    Items items;
    Music music;                   // background music (data/music.txt); Ctrl+M turns it off and on
    Sounds sounds;                 // sound effects (data/sounds.txt); K turns them off and on
    bool soundsLoaded = false;
    bool musicLoaded = false;
    int musicBy = -1;              // the person playing the piece now, if any
    bool testMode = false;         // --walk: print what would be played
    // a conversation has asked for a piece ("play ID"): start it, from whoever plays it
    void startRequestedPiece() {
        if (!people.roadRequests.empty()) {   // the roads: robbed, a fight, an escort, a bill of exchange
            auto rq = people.roadRequests;
            people.roadRequests.clear();
            for (auto &r : rq) roadRequest(r.first, r.second);
        }
        if (people.postBy >= 0) {   // the post: a packet to carry, or one delivered
            int k = people.postBy;
            people.postBy = -1;
            people.endTalk(minutes);
            talking = false;
            if (people.postDeliver) postDelivered(k); else postJob(k);
        }
        if (people.portraitWanted) {   // your portrait: painted now if a picture can be drawn, else as soon as one can
            people.portraitWanted = false;
            people.player["portrait:file"] = portraitPath();
            people.player["portrait:pending"] = "1";
            paintPendingPortrait();
        }
        if (people.cowBy >= 0) {   // a drover at a cattle market sells you a cow
            int k = people.cowBy;
            people.cowBy = -1;
            people.endTalk(minutes);
            talking = false;
            cowDeal(k, people.cowPrice);
        }
        if (people.horseBy >= 0) {   // an innkeeper or an ostler lets you have a horse
            int k = people.horseBy;
            people.horseBy = -1;
            people.endTalk(minutes);
            talking = false;
            horseDeal(k, people.horseBuy, people.horsePrice);
        }
        if (people.shipBy >= 0) {   // a ship of your own, sold at Amsterdam and Seville
            int k = people.shipBy;
            people.shipBy = -1;
            people.endTalk(minutes);
            talking = false;
            shipDeal(k, people.shipPrice, people.shipKind);
        }
        if (people.changeBy >= 0) {   // a money-changer at his table
            int k = people.changeBy;
            people.changeBy = -1;
            changeMoney(k, people.changeTo);
        }
        if (people.passageBy >= 0) {   // a boat across (the Dover packet)
            int k = people.passageBy;
            people.passageBy = -1;
            people.endTalk(minutes);
            talking = false;
            takePassage(k, people.passagePrice, people.passageMinutes, people.passageTo);
        }
        if (people.rideBy >= 0) {   // the post ride, taken from an innkeeper or an ostler
            int k = people.rideBy;
            people.rideBy = -1;
            people.endTalk(minutes);
            talking = false;
            postRide(k);
        }
        if (people.passMinutes > 0) {   // a lesson all afternoon (people's "pass MINUTES"): the clock moves on
            minutes += people.passMinutes;
            people.passMinutes = 0;
            people.endTalk(minutes);
            talking = false;
        }
        if (people.sleepBy >= 0) {   // an innkeeper has sold you a bed
            int k = people.sleepBy;
            people.sleepBy = -1;
            people.endTalk(minutes);
            talking = false;
            int b = bldAt(people.people[k].workX, people.people[k].workY, people.people[k].workZ);
            sleepPaid(b ? world.buildings[b - 1].name : "the inn", k);
        }
        if (people.stayBy >= 0) {   // an innkeeper has sold you a bed until word comes
            int k = people.stayBy;
            people.stayBy = -1;
            people.endTalk(minutes);
            talking = false;
            int b = bldAt(people.people[k].workX, people.people[k].workY, people.people[k].workZ);
            stayTillWord(k, b ? world.buildings[b - 1].name : "the inn");
        }
        if (people.playRequest.empty()) return;
        std::string id = people.playRequest;
        int who = people.playBy;
        people.playRequest.clear();
        people.playBy = -1;
        const Music::Piece *pc = music.findPiece(id);
        if (testMode) {
            double secs = music.pieceSeconds(id);
            std::fprintf(stderr, "  (%s plays %s: %s; %.0f s)\n", who >= 0 ? people.people[who].name.c_str() : who == -2 ? "you" : "someone", id.c_str(),
                         pc ? pc->title.c_str() : "NOT IN data/music.txt", secs);
            if (who >= 0) people.people[who].hold = (float)secs;   // as in the game: they stay to play it
            return;
        }
        double secs = 0;
        if (!musicLoaded || !music.playPiece(id, now, &secs)) return;
        musicBy = who;
        if (who >= 0) people.people[who].hold = (float)secs;
        if (who >= 0 && pc) say(people.label(people.people[who]) == people.people[who].name ? people.people[who].name + " plays " + pc->title + "." : "The musician plays " + pc->title + ".", 7.0);
    }
    // ---- SOUND EFFECTS: what you hear here and now. The church bells strike the hours (the
    // nearest church within 140 tiles, a cathedral's great bell or a parish church's; none in the
    // Ottoman towns, whose churches are mosques); the country has birds by day, the night
    // crickets; a town by day its voices; the sea its waves, a river its water, the mountains
    // their wind; indoors all of it is muffled, underground silent; coins clink when money
    // changes hands, a working smith's anvil rings nearby, your horse's hooves as you ride
    double lastStepAt = -1;  // when your last step began (the hooves go on between steps)
    double waterCheckAt = 0;
    float waterFrac = 0, seaWide = 0;
    bool nearWater = false;
    double bellHour = -1;   // the last hour struck (or passed)
    int bellStrokes = 0;
    double nextStroke = 0, nextAnvil = 0;
    int anvilRun = 0;
    std::string bellId;
    float bellVol = 0, bellPan = 0;
    long moneySeen = -1;
    float panTo(float x, float y) const {   // -1 left .. 1 right, as the camera looks
        float dx = x - tx, dy = y - ty, L = std::hypot(dx, dy);
        if (L < 1) return 0;
        float sx = dx * std::cos(yaw * PI_F / 180) - dy * std::sin(yaw * PI_F / 180);
        return std::max(-0.8f, std::min(0.8f, sx / L));
    }
    void soundStep(float dt) {
        if (!soundsLoaded) return;
        double h = std::fmod(minutes, 1440.0) / 60.0;
        bool night = h >= 21 || h < 4.5, day = h >= 5 && h < 20.5;
        const Settlement *st = nullptr;
        for (auto &s : world.settlements) if (s.inside(tx, ty)) { st = &s; break; }
        bool inTown = st && !st->hamlet;
        bool indoors = inside > 0 && inside <= (int)world.buildings.size() && world.buildings[inside - 1].kind != "gate";
        float muffle = tz < 0 ? 0.0f : indoors ? 0.25f : tz > 0 ? 0.6f : 1.0f;
        // how much of the ground within 24 tiles is water (twice a second): the sea, or a river
        if (now >= waterCheckAt) {
            waterCheckAt = now + 0.5;
            int n = 0, wet = 0;
            nearWater = false;
            for (int dy = -24; dy <= 24; dy += 4)
                for (int dx = -24; dx <= 24; dx += 4) {
                    if (dx * dx + dy * dy > 576) continue;
                    const Tile &t = world.tile(tx + dx, ty + dy);
                    bool w = t.terrain == T_WATER || t.terrain == T_DEEP;
                    n++; wet += w;
                    if (w && std::abs(dx) <= 8 && std::abs(dy) <= 8) nearWater = true;
                }
            waterFrac = n ? (float)wet / n : 0;
            // the sea, not a river: water in 11 x 11 cells of the old map round you (a river town
            // has a quarter of it or less, a coast nearly a half or more)
            int ci = (int)std::floor((float)tx / world.S), cj = (int)std::floor((float)ty / world.S), cw = 0;
            for (int di = -5; di <= 5; di++)
                for (int dj = -5; dj <= 5; dj++) cw += world.L.geoAt(ci + di, cj + dj) <= 0;
            seaWide = cw / 121.0f;
        }
        float sea = waterFrac;
        static const bool slog = std::getenv("GALILEO_SOUNDLOG") != nullptr;
        static double lastLog = -10;
        if (slog && now - lastLog > 2.5) { lastLog = now; std::fprintf(stderr, "  [water %.2f%s, sea %.2f, height %.1f]\n", waterFrac, nearWater ? ", near" : "", seaWide, inTown ? 0.0f : world.baseElev((float)tx, (float)ty)); }
        bool water = nearWater;
        float elev = inTown ? 0.0f : world.baseElev((float)tx, (float)ty);
        auto smooth = [](float a, float b, float x) { float t = std::max(0.0f, std::min(1.0f, (x - a) / (b - a))); return t * t * (3 - 2 * t); };
        sounds.loop("birds", (day && !inTown ? 1.0f : 0.0f) * muffle * (1 - 0.6f * smooth(2.5f, 4.5f, elev)));
        sounds.loop("crickets", (night ? (inTown ? 0.45f : 1.0f) : 0.0f) * muffle);
        sounds.loop("town", (inTown && h >= 7 && h < 21 ? 1.0f : 0.0f) * muffle);
        float waves = smooth(0.36f, 0.5f, seaWide) * smooth(0.04f, 0.2f, sea);
        sounds.loop("waves", waves * muffle);
        sounds.loop("river", (water && waves < 0.3f ? 1.0f : 0.0f) * muffle);
        sounds.loop("wind", smooth(2.5f, 4.8f, elev) * muffle);
        bool moving = riding >= 0 && (stepT < 1 || now - lastStepAt < 0.25);
        sounds.loop("hooves", moving ? 1.0f : 0.0f);
        if (moving) sounds.setRate("hooves", std::max(0.8f, std::min(2.0f, 0.075f / std::max(0.01f, stepDur))));
        // the hours: when the clock passes one, the nearest church strikes it
        double hourNow = std::floor(minutes / 60.0);
        if (bellHour < 0 || hourNow < bellHour || hourNow - bellHour > 1.5) bellHour = hourNow;   // started, or the clock moved by hand
        else if (hourNow > bellHour) {
            bellHour = hourNow;
            int hr = (int)std::fmod(hourNow, 24.0);
            if (hr % sounds.bellEvery == 0 && tz >= 0) {
                int best = -1;
                float bd = 140;
                for (size_t b = 0; b < world.buildings.size(); b++) {
                    const Building &B = world.buildings[b];
                    if (B.kind != "church" && B.kind != "cathedral" && B.kind != "chapel") continue;
                    float cx = (B.x0 + B.x1) * 0.5f, cy = (B.y0 + B.y1) * 0.5f;
                    float d = std::hypot(cx - tx, cy - ty);
                    if (d >= bd || countryOf(townAt(B.x0, B.y0)) == "Ottoman") continue;
                    bd = d; best = (int)b;
                }
                if (best >= 0) {
                    const Building &B = world.buildings[best];
                    bellStrokes = hr % 12 == 0 ? 12 : hr % 12;
                    bellId = B.kind == "cathedral" ? "bell-big" : "bell-small";
                    bellVol = std::max(0.08f, std::min(1.0f, 1.0f - (bd - 12) / 130)) * (indoors && inside != best + 1 ? 0.6f : 1.0f);
                    bellPan = panTo((B.x0 + B.x1) * 0.5f, (B.y0 + B.y1) * 0.5f);
                    nextStroke = now;
                }
            }
        }
        if (bellStrokes > 0 && now >= nextStroke) {
            sounds.play(bellId, bellVol, bellPan);
            bellStrokes--;
            nextStroke = now + (bellId == "bell-big" ? 2.2 : 1.7);
        }
        // coins, when money changes hands
        long m = money();
        if (moneySeen >= 0 && m != moneySeen) sounds.play("coins", 0.9f);
        moneySeen = m;
        // a smith at his anvil, near you
        if (now >= nextAnvil) {
            float bestD = 24;
            const Person *smith = nullptr;
            for (auto &pp : people.people) {
                if (pp.role != "blacksmith" || !pp.active || pp.dead || pp.onRoad || pp.z != tz || pp.activity != "work" || pp.pathPos < pp.path.size()) continue;
                float d = std::hypot((float)(pp.x - tx), (float)(pp.y - ty));
                if (d < bestD) { bestD = d; smith = &pp; }
            }
            if (smith) {
                float v = (1.0f - bestD / 24.0f) * (indoors && bldAt(smith->x, smith->y, smith->z) != inside ? 0.4f : 1.0f);
                sounds.play("anvil", v, panTo((float)smith->x, (float)smith->y));
                anvilRun++;
                nextAnvil = now + (anvilRun % 8 == 0 ? 2.5 + (anvilRun % 3) : (anvilRun % 3 == 1 ? 0.32 : 0.62));
            } else nextAnvil = now + 0.5;
        }
        sounds.update(dt);
    }
    // the piece is heard from where its player is: full within 4 tiles, gone at 30
    void pieceLevel() {
        if (!musicLoaded) return;
        if (!music.piecePlaying()) { musicBy = -1; return; }
        float level = 1;
        if (musicBy >= 0 && musicBy < (int)people.people.size()) {
            const Person &pp = people.people[musicBy];
            float d = std::hypot((float)(pp.x - tx), (float)(pp.y - ty));
            level = std::max(0.0f, std::min(1.0f, 1.0f - (d - 4.0f) / 26.0f));
            if (pp.z != tz) level *= 0.25f;   // through a floor or a wall of earth
        }
        music.setPieceLevel(level);
    }
    GpuMesh thingsMesh;
    int thingsVersion = -1, thingsX = 0, thingsY = 0, thingsZ = 99;
    bool thingsFine = false;   // the things' drawing was made close up (finer: items.cc drawThingFine)
    int panel = 0;                 // 0 none, 1 what you carry, 2 reading (a work of art)
    std::string readTitle, readText, readImage, readCredit;
    std::string bookLearnt;        // a skill the open book has just taught you (shown in its note)
    std::string bookId;            // panel 3: the book open, and its first page shown (always even)
    int bookFirst = 0;
    int invPick = -1;              // the thing chosen in the list of what you carry
    bool useMode = false;          // the list opened by U: a number uses that thing
    int pick = 0;                  // waiting for a click: 1 on what to get, 2 on where to put pickKind, 3 on what to look at
    int pickKind = -1;
    double mouseX = -1, mouseY = -1;   // the mouse, as a fraction of the window (0..1, from the top left)
    float viewAspect = 1.6f;
    GLFWcursor *crossCursor = nullptr;
    bool talking = false;
    bool swallowChar = false;   // the T that began a conversation is not the first letter typed
    std::string savePath = "saves/autosave.txt";
    GLFWwindow *win = nullptr;

    // player
    int tx = 0, ty = 0, tz = 0;   // tz: the floor (0 the ground, above 0 upstairs, below 0 underground)
    std::vector<GpuMesh> levelGfx;
    std::vector<char> levelBuilt;
    vec2 from, to;
    float stepT = 1, stepDur = 0.14f;
    float facing = 0, facingTarget = 0, walkPhase = 0;
    bool gallop = false;
    int boat = -1;                // the kind of boat you are in (data/items.txt), or -1 on foot

    // view
    float yaw = 22, pitch = 52, zoom = 20, yawT = 22, pitchT = 52, zoomT = 20;
    bool roofsOff = false;
    vec3 camTarget;

    double minutes = 10 * 60;
    std::string message;
    double messageUntil = 0;
    bool messageMinor = false;   // the message is a passing remark (note), which the next one may replace
    int inside = 0;
    std::string place;
    bool help = false, showMap = false, journal = false;   // journal (J): the stories you are in, and what you have overheard
    int journalTab = 0;
    int journalPage = 0;   // the page of the stories (the left and right arrows; Hud::journalPages of them)
    int townCycle = 0;
    double now = 0;
    float blockedCooldown = 0;
    int pushX = -1, pushY = -1;   // the tile of the one in your way, when you last bumped into them
    double pushAt = 0;

    void say(const std::string &s, double secs = 3.0) {
        message = s; messageUntil = now + secs; messageMinor = false;
        if (testMode) std::fprintf(stderr, "  {%s}\n", s.c_str());   // --walk: what the status line would say
    }
    // a passing remark ("You enter the bakery", a gate's name): it gives way to a message still
    // being read, which it would otherwise replace after a step or two
    void note(const std::string &s) {
        if (!message.empty() && !messageMinor && now < messageUntil) return;
        say(s);
        messageMinor = true;
    }

    // ---- things: what is in front of you, what you carry
    void frontTile(int &X, int &Y) const {
        X = tx + (int)std::lround(-std::sin(facingTarget));
        Y = ty + (int)std::lround(std::cos(facingTarget));
    }
    std::vector<std::pair<int, int>> carried() const {   // (kind, how many), in the catalogue's order
        std::vector<std::pair<int, int>> out;
        for (size_t k = 0; k < items.kinds.size(); k++) {
            auto it = people.player.find("has:" + items.kinds[k].id);
            if (it != people.player.end() && std::atoi(it->second.c_str()) > 0) out.push_back({(int)k, std::atoi(it->second.c_str())});
        }
        return out;
    }
    float carriedWeight() const {
        float w = 0;
        for (auto &c : carried()) w += items.kinds[c.first].weight * c.second;
        return w;
    }
    long money() const {
        auto it = people.player.find("money");
        return it == people.player.end() ? 0 : std::atol(it->second.c_str());
    }
    static std::string moneyString(long d) { return ::moneyString(d); }
    // ---- coins (data/coins.txt). Your purse is kept coin by coin in your memory (coin:ID, its
    // worth in deniers of Paris), and squared with "money" after anything that pays or is paid
    // (squareCoins): money that comes in is in the coin of the place; money that goes out comes
    // from that coin first, then from foreign coin, taken a tenth below its worth
    struct Coin { std::string id, name, look; std::vector<std::string> in; std::vector<std::pair<std::vector<std::string>, float>> dear; };
    std::vector<Coin> coins;
    void loadCoins(const std::string &fn) {
        std::ifstream in(fn);
        if (!in) return;
        std::string line;
        Coin *cur = nullptr;
        auto places = [](const std::string &t) {
            std::vector<std::string> out;
            std::string x;
            std::istringstream ps(t);
            while (std::getline(ps, x, ',')) { for (auto &c : x) if (c == '_') c = ' '; out.push_back(trimS(x)); }
            return out;
        };
        while (std::getline(in, line)) {
            std::string t = trimS(line);
            if (t.empty() || t[0] == '#') continue;
            std::istringstream ss(t);
            std::string w0;
            ss >> w0;
            if (w0 == "coin") { coins.push_back(Coin()); cur = &coins.back(); ss >> cur->id; continue; }
            if (!cur) continue;
            std::string rest;
            std::getline(ss, rest);
            rest = trimS(rest);
            if (w0 == "name") cur->name = rest;
            else if (w0 == "look") cur->look = rest;
            else if (w0 == "in") cur->in = places(rest);
            else if (w0 == "dear") {
                size_t sp = rest.find_last_of(' ');
                if (sp != std::string::npos) cur->dear.push_back({places(rest.substr(0, sp)), (float)std::atof(rest.c_str() + sp + 1)});
            } else if (w0 == "end") cur = nullptr;
        }
    }
    std::string countryOf(const std::string &town) const {
        auto it = animals.country.find(town);
        return it == animals.country.end() || it->second.empty() ? "France" : it->second;
    }
    const Coin *coinById(const std::string &id) const {
        for (auto &c : coins) if (c.id == id) return &c;
        return nullptr;
    }
    std::string coinOf(const std::string &town) const {
        if (coins.empty()) return "";
        for (auto &c : coins) for (auto &p : c.in) if (p == town) return c.id;
        std::string ctry = countryOf(town);
        for (auto &c : coins) for (auto &p : c.in) if (p == ctry) return c.id;
        return coins[0].id;
    }
    std::string hereCoin() const {   // the coin of the town you are in, or of the nearest one
        if (!curTown.empty()) return coinOf(curTown);
        std::string best;
        float bd = 1e9f;
        for (auto &st : world.settlements) {
            float dx = std::max({(float)st.x0 - tx, 0.0f, (float)tx - (st.x0 + st.w)});
            float dy = std::max({(float)st.y0 - ty, 0.0f, (float)ty - (st.y0 + st.h)});
            float d = std::hypot(dx, dy);
            if (d < bd) { bd = d; best = st.name; }
        }
        return coinOf(best);
    }
    // what a changer in TOWN gives for a denier's worth of coin ID (1, or more where it is dear)
    float coinWorth(const std::string &id, const std::string &town) const {
        const Coin *c = coinById(id);
        if (!c) return 1;
        std::string ctry = countryOf(town);
        for (auto &d : c->dear) for (auto &p : d.first) if (p == town || p == ctry) return d.second;
        return 1;
    }
    long coinHeld(const std::string &id) const { return numMem(people.player, "coin:" + id); }
    void setCoin(const std::string &id, long v) {
        if (v > 0) people.player["coin:" + id] = std::to_string(v); else people.player.erase("coin:" + id);
    }
    void squareCoins() {
        if (coins.empty()) return;
        long m = money(), sum = 0;
        bool any = false;
        for (auto &c : coins) if (people.player.count("coin:" + c.id)) { sum += coinHeld(c.id); any = true; }
        if (!any) { if (m > 0) setCoin(hereCoin(), m); return; }   // a purse from before coins: the coin of where you are
        long d = m - sum;
        if (d == 0) return;
        if (m <= 0) { for (auto &c : coins) setCoin(c.id, 0); return; }
        std::string here = hereCoin();
        if (d > 0) { setCoin(here, coinHeld(here) + d); return; }   // paid to you: the coin of the place
        long need = -d, extra = 0;
        std::string usedNames;   // the foreign coin that paid
        long t = std::min(need, coinHeld(here));
        setCoin(here, coinHeld(here) - t);
        need -= t;
        for (int guard = 0; need > 0 && guard < 40; guard++) {   // then foreign coin, the largest purse first
            std::string best;
            long bv = 0;
            for (auto &c : coins) if (c.id != here && coinHeld(c.id) > bv) { bv = coinHeld(c.id); best = c.id; }
            if (best.empty()) break;
            long want = (need * 10 + 8) / 9, take = std::min(want, bv), paid = take * 9 / 10;
            if (paid <= 0 && take > 0) paid = std::min(need, take);
            setCoin(best, bv - take);
            need -= paid;
            extra += take - paid;
            if (const Coin *bc = coinById(best)) if (usedNames.find(bc->name) == std::string::npos) usedNames += (usedNames.empty() ? "" : " and ") + bc->name;
        }
        long now = 0;
        for (auto &c : coins) now += coinHeld(c.id);
        people.player["money"] = std::to_string(now);
        if (extra > 0) {
            std::string note = "(You paid part of it in " + (usedNames.empty() ? std::string("foreign coin") : usedNames) +
                               ", which is foreign here and taken a tenth below its worth: " + moneyString(extra) + " more. A money-changer would do better.)";
            say(note, 4.5);
        }
    }
    std::string purseString() const {
        std::string out;
        int n = 0;
        for (auto &c : coins) if (coinHeld(c.id) > 0) n++;
        if (n <= 1) {
            for (auto &c : coins) if (coinHeld(c.id) > 0) out = "all in " + c.name;
            return out;
        }
        for (auto &c : coins)
            if (coinHeld(c.id) > 0) out += (out.empty() ? "" : ", ") + moneyString(coinHeld(c.id)) + " in " + c.name;
        return out;
    }
    // a money-changer: everything you carry into the coin of his town (or into COIN), weighed and
    // tried on his touchstone, for three in the hundred; where a coin is dear he pays more for it
    void changeMoney(int changer, const std::string &into) {
        const Person &p = people.people[changer];
        std::string to = into.empty() ? coinOf(p.town) : into;
        const Coin *tc = coinById(to);
        if (!tc) return;
        double value = 0;
        std::string gave;
        for (auto &c : coins) {
            if (c.id == to || coinHeld(c.id) <= 0) continue;
            value += coinHeld(c.id) * coinWorth(c.id, p.town);
            gave += (gave.empty() ? "" : ", ") + c.name;
        }
        if (gave.empty()) { say("\"You have nothing but " + tc->name + ": there is nothing to change.\"", 4.0); return; }
        value /= coinWorth(to, p.town);
        long fee = std::max(1L, (long)std::lround(value * 0.03));
        long got = std::max(0L, (long)std::lround(value) - fee);
        for (auto &c : coins) if (c.id != to) setCoin(c.id, 0);
        setCoin(to, coinHeld(to) + got);
        long now = 0;
        for (auto &c : coins) now += coinHeld(c.id);
        people.player["money"] = std::to_string(now);
        Person &cp = people.people[changer];
        cp.mem["money"] = std::to_string(std::atol(cp.mem["money"].c_str()) + fee);
        people.player["changed"] = std::to_string(numMem(people.player, "changed") + 1);
        say("He weighs your " + gave + " on his little scales, tries one on his touchstone, and counts you out " +
            moneyString(got) + " in " + tc->name + ". His fee: " + moneyString(fee) + ".", 7.0);
    }
    // using a thing (U): eat or drink it (one is gone), read it (a page), or what it says
    void useThing(int kind) {
        const ItemKind &k = items.kinds[kind];
        std::string key = "has:" + k.id;
        int n = std::atoi(people.player[key].c_str());
        if (n <= 0) return;
        if (!k.book.empty() && items.books.count(k.book)) { openBook(k.book); return; }   // a book: open it, whatever language it is in
        if (k.useVerb.empty()) { say("You can find no use for " + k.name + ".", 2.5); return; }
        if (!k.needs.empty() && numMem(people.player, "skill:" + k.needs) <= 0) {   // a skill you have not got
            say(k.needsText.empty() ? "You do not know how to use " + k.name + "." : k.needsText, 5.0);
            return;
        }
        if (k.useVerb == "shoot") { shootNearest(); return; }   // the fowling piece
        if (k.useVerb == "fish") { goFishing(); return; }       // the rod
        if (k.useVerb == "bill") {                              // the bill of exchange: what it says
            long amount = numMem(people.player, "bill:amount");
            std::string from = people.player.count("bill:town") ? people.player["bill:town"] : std::string("a changer's table");
            say("A bill of exchange, drawn at " + from + ", for " + moneyString(amount) + ": any money-changer in another town pays it to you (give it to him, or say \"cash\").", 6.0);
            return;
        }
        double h = std::fmod(minutes, 1440.0) / 60.0;
        bool night = h >= 21 || h < 4.5;
        if (!k.nearKind.empty()) {   // only beside a certain thing (a verse on Pasquino)
            int nk = items.find(k.nearKind);
            bool close = false;
            for (const Thing &t : items.things)
                if (!t.gone && t.kind == nk && t.z == tz && std::max(std::abs(t.x - tx), std::abs(t.y - ty)) <= 2) close = true;
            if (!close) { say(k.nearText.empty() ? "Not here." : k.nearText, 5.0); return; }
        }
        if (!k.atPlace.empty() && place.find(k.atPlace) == std::string::npos) {   // only in a certain place (the top of a tower)
            say(k.atText.empty() ? "Not here." : k.atText, 5.0);
            return;
        }
        if (k.useVerb != "make")   // what must go with it (two balls to drop together), kept
            for (auto &wi : k.with) {
                int wk = items.find(wi.first);
                if (wk >= 0 && numMem(people.player, "has:" + wi.first) < wi.second) {
                    say("For that you need " + (wi.second > 1 ? std::to_string(wi.second) + " " + items.kinds[wk].plural : items.kinds[wk].name) + " as well.", 4.0);
                    return;
                }
            }
        if (k.useVerb == "make") {   // grinding a lens, making a spyglass: this and what goes with it become something else
            int r = items.find(k.makes);
            if (r < 0) return;
            for (auto &wi : k.with) {
                int wk = items.find(wi.first);
                if (wk >= 0 && numMem(people.player, "has:" + wi.first) < wi.second + (wi.first == k.id ? 1 : 0)) {
                    say("For that you need " + (wi.second > 1 ? std::to_string(wi.second) + " " + items.kinds[wk].plural : items.kinds[wk].name) + " as well.", 4.0);
                    return;
                }
            }
            auto takeOne = [&](const std::string &id, int m) {
                long have = numMem(people.player, "has:" + id) - m;
                if (have <= 0) people.player.erase("has:" + id); else people.player["has:" + id] = std::to_string(have);
            };
            takeOne(k.id, 1);
            for (auto &wi : k.with) takeOne(wi.first, wi.second);
            people.player["has:" + k.makes] = std::to_string(numMem(people.player, "has:" + k.makes) + k.makesN);
            minutes += k.takes > 0 ? k.takes : 30;
            say(k.useText.empty() ? "You make " + items.nameOf(r, k.makesN) + "." : k.useText, 6.0);
            panel = 0;
        } else if (k.useVerb == "play") {   // you play a piece yourself
            people.playRequest = k.play;
            people.playBy = -2;
            say(k.useText, 5.0);
            panel = 0;
        } else if (k.useVerb == "eat" || k.useVerb == "drink") {
            // not in a fight: food mends between fights, it is no potion in the middle of one
            if (fightWith >= 0 || fightBeast >= 0 || bandState == 3) {
                say(k.useVerb == "eat" ? "Not now: you have a fight on your hands." : "Not now: you have a fight on your hands, and no hand for a cup.", 3.0);
                return;
            }
            if (n == 1) people.player.erase(key); else people.player[key] = std::to_string(n - 1);
            people.player["last-meal"] = std::to_string((long)minutes);
            minutes += k.useVerb == "eat" ? 10 : 5;
            std::string eaten = k.useText;
            if (k.heals > 0 && hpNow() < (float)maxHp()) {   // eating mends a little, if you were hurt
                setHp(hpNow() + (float)k.heals);
                eaten += " (It does you good.)";
            }
            say(eaten, 5.0);
        } else if (k.useVerb == "read") {
            panel = 2;
            readTitle = k.useTitle.empty() ? k.name : k.useTitle;
            readText = k.useText;
            readImage = k.image;   // a drawing, a map: shown with the page
            if (readImage.size() >= 9 && readImage.compare(readImage.size() - 9, 9, "$portrait") == 0) {   // the one painted of you
                auto pf = people.player.find("portrait:file");
                readImage = pf == people.player.end() ? std::string() : pf->second;
                if (readImage.empty() || !std::ifstream(readImage)) readImage.clear();
            }
            readCredit = k.credit;
            minutes += 15;
        } else {
            say(night && !k.nightText.empty() ? k.nightText : k.useText, k.useText.size() > 120 ? 8.0 : 4.0);
        }
        std::vector<std::string> effs = k.useEffects;
        if (night) effs.insert(effs.end(), k.nightEffects.begin(), k.nightEffects.end());
        applyEffects(effs);
    }
    void openBook(const std::string &id) {
        panel = 3; bookId = id; bookFirst = 0; bookLearnt.clear();
        minutes += 5;
        readBookPages();
    }
    // a book: can you read its language? (English needs no skill)
    bool understands(const Book &b) {
        return b.language == "english" || numMem(people.player, "skill:" + b.language) > 0;
    }
    // the two pages now open: if you understand them, what they do to you happens (once per page)
    void readBookPages() {
        auto it = items.books.find(bookId);
        if (it == items.books.end() || !understands(it->second)) return;
        for (int p = bookFirst; p < bookFirst + 2 && p < (int)it->second.pages.size(); p++) {
            const BookPage &pg = it->second.pages[p];
            if (pg.effects.empty()) continue;
            std::string mark = "read:" + bookId + ":" + std::to_string(p + 1);
            if (people.player.count(mark)) continue;
            people.player[mark] = "1";
            std::vector<std::string> effs;
            for (const std::string &e : pg.effects) {   // "learn SKILL" from a book: once, and you are told
                std::istringstream es(e);
                std::string a, b, c;
                es >> a >> b >> c;
                if (a == "player") { a = b; b = c; }
                if (a == "learn") {
                    if (numMem(people.player, "skill:" + b) > 0) continue;
                    bookLearnt = b;
                }
                effs.push_back(e);
            }
            applyEffects(effs);
        }
    }
    void printBook() {   // for --walk tests
        auto it = items.books.find(bookId);
        if (it == items.books.end()) return;
        const Book &b = it->second;
        bool u = understands(b);
        std::fprintf(stderr, "  [%s, pages %d-%d of %zu, %s]\n", (u ? b.titleEn : b.title).c_str(), bookFirst + 1, bookFirst + 2, b.pages.size(), u ? "understood" : ("in " + b.language).c_str());
        for (int p = bookFirst; p < bookFirst + 2 && p < (int)b.pages.size(); p++) {
            const BookPage &pg = b.pages[p];
            std::string t = u ? pg.english : pg.orig;
            std::fprintf(stderr, "    %d: %s%s\n", p + 1, pg.image.empty() ? "" : "[picture] ", t.c_str());   // the whole page (it was cut at 400 letters)
        }
        if (!bookLearnt.empty()) std::fprintf(stderr, "    (learnt from it: %s)\n", bookLearnt.c_str());
    }
    void turnPages(int d) {
        auto it = items.books.find(bookId);
        if (it == items.books.end()) return;
        int n = (int)it->second.pages.size(), f = bookFirst + 2 * d;
        if (f < 0 || f >= n) return;
        bookFirst = f;
        minutes += 2;
        readBookPages();
    }
    void applyEffects(const std::vector<std::string> &effs) {
        for (std::string e : effs) {   // set KEY[=V], unset KEY, add KEY N, learn SKILL: on what you know
            std::istringstream es(e);
            std::vector<std::string> tk;
            for (std::string x; es >> x;) tk.push_back(x);
            if (!tk.empty() && tk[0] == "player") tk.erase(tk.begin());   // effects of things are on you anyway
            std::string verb = tk.size() > 0 ? tk[0] : "", arg = tk.size() > 1 ? tk[1] : "", num = tk.size() > 2 ? tk[2] : "";
            Memory &m = people.player;
            if (verb == "set") {
                size_t q = arg.find('=');
                if (q == std::string::npos) m[arg] = "1";
                else m[arg.substr(0, q)] = arg.substr(q + 1) == "now" ? std::to_string((long)minutes) : arg.substr(q + 1);
            }
            else if (verb == "unset") m.erase(arg);
            else if (verb == "add") m[arg] = std::to_string(std::atol(m[arg].c_str()) + (num.empty() ? 1 : std::atol(num.c_str())));
            else if (verb == "learn") m["skill:" + arg] = std::to_string(std::atol(m["skill:" + arg].c_str()) + 1);
            else if (verb == "take-one") {   // the thing is used up (a verse pasted up)
                long have = std::atol(m["has:" + arg].c_str()) - 1;
                if (have <= 0) m.erase("has:" + arg); else m["has:" + arg] = std::to_string(have);
            }
        }
    }
    static constexpr float kCarry = 60;   // pounds
    // ---- what is whose. A thing in a building belongs to the people who live or work
    // there; a thing on a stall to whoever keeps the stall. Taken in front of someone, it
    // must be paid for or put back; carried off, it is theft, and the watch comes.
    struct Owed { int kind = -1, count = 0, bld = 0, x = 0, y = 0, z = 0, witness = -1, hadAfter = 0; long price = 0; bool stolen = false; std::string town; } owed;
    std::string townAt(int X, int Y) const {
        for (auto &s : world.settlements) if (s.inside(X, Y)) return s.name;
        return "";
    }
    int bldAt(int X, int Y, int Z) { const Tile *t = world.tileAt(X, Y, Z); return t ? t->bld : 0; }
    std::string homeTown() const {   // the hamlet of your family's farmhouse (the "home" of its map legend)
        for (auto &b : world.buildings) if (b.home) return townAt(b.x0, b.y0);
        return "";
    }
    std::vector<int> keepersOf(const Thing &t) {
        std::vector<int> out;
        const Tile *tl = world.tileAt(t.x, t.y, t.z);
        bool stall = tl && tl->feature == F_STALL;
        for (size_t i = 0; i < people.people.size(); i++) {
            const Person &p = people.people[i];
            if (t.owner && (p.home == t.owner || (p.hasWork && bldAt(p.workX, p.workY, p.workZ) == t.owner))) out.push_back((int)i);
            else if (stall && p.hasWork && std::max(std::abs(p.workX - t.x), std::abs(p.workY - t.y)) <= 1) out.push_back((int)i);
        }
        return out;
    }
    // awake people who can see you: in the same building, or near you out of doors
    std::vector<int> watchers(int bld) {
        std::vector<int> out;
        for (size_t i = 0; i < people.people.size(); i++) {
            const Person &p = people.people[i];
            if (!p.active || p.lying || p.z != tz || p.onRoad) continue;
            int d = std::max(std::abs(p.x - tx), std::abs(p.y - ty));
            int b = bldAt(p.x, p.y, p.z);
            if (bld ? (b == bld && d <= 12) : (b == 0 && d <= 7)) out.push_back((int)i);
        }
        return out;
    }
    long numMem(const Memory &m, const std::string &k) const { auto it = m.find(k); return it == m.end() ? 0 : std::atol(it->second.c_str()); }
    void noticeTaking(const Thing &t, int count) {
        const ItemKind &k = items.kinds[t.kind];
        std::vector<int> keepers = keepersOf(t);
        if (!t.owner && keepers.empty()) return;   // nobody's
        if (t.owner && world.buildings[t.owner - 1].home && chosen()) return;   // your family's: yours too
        long price = (k.money ? k.money : k.price) * count;
        std::vector<int> seen = watchers(t.owner);
        std::string nowS = std::to_string((long)minutes);
        if (seen.empty()) {   // nobody saw: it is yours, but it will be missed
            for (int i : keepers) { people.people[i].mem["robbed"] = k.name; people.people[i].mem["robbed-at"] = nowS; }
            people.player["thefts-unseen"] = std::to_string(numMem(people.player, "thefts-unseen") + 1);
            return;
        }
        int wi = seen[0];
        for (int i : seen) if (std::find(keepers.begin(), keepers.end(), i) != keepers.end()) { wi = i; break; }
        bool keeper = std::find(keepers.begin(), keepers.end(), wi) != keepers.end();
        Person &w = people.people[wi];
        owed = Owed();
        owed.kind = t.kind; owed.count = count; owed.bld = t.owner; owed.x = t.x; owed.y = t.y; owed.z = t.z;
        owed.witness = wi; owed.price = price; owed.town = townAt(tx, ty);
        owed.hadAfter = k.money ? 0 : (int)numMem(people.player, "has:" + k.id);
        people.debt = People::Debt();
        people.debt.keeper = keeper ? w.id : "";
        people.debt.kind = t.kind; people.debt.count = count; people.debt.price = price;
        const std::string &kind = t.owner ? world.buildings[t.owner - 1].kind : "";
        bool church = kind == "church" || kind == "cathedral" || kind == "chapel" || kind == "basilica" || kind == "abbey";
        std::string line = church ? "In the house of God? Put that back!"
                         : !keeper ? "Here! That is not yours. Put it back."
                         : k.money ? "That is my purse! Give it back, or I call the watch!"
                         : "That is mine! Put it back, or pay me " + moneyString(price) + ".";
        w.bubble = line; w.bubbleT = 4.5f; w.hold = std::max(w.hold, 3.0f);
        w.facing = std::atan2(-(float)(tx - w.x), (float)(ty - w.y));
        say(people.label(w) + ": \"" + line + "\"", 5.0);
    }
    void clearOwed() { owed = Owed(); people.debt = People::Debt(); }
    // carried off in front of someone: the cry goes up, and the watch comes
    void theft() {
        owed.stolen = true;
        std::string town = owed.town, nowS = std::to_string((long)minutes);
        if (owed.witness >= 0 && owed.witness < (int)people.people.size()) {
            Person &w = people.people[owed.witness];
            w.bubble = "Thief! Stop, thief!"; w.bubbleT = 4.0f;
            say("\"Thief! Stop, thief!\" cries " + people.label(w) + ".", 4.0);
        }
        for (auto &p : people.people)   // whoever is near hears it, and remembers
            if (p.active && !p.lying && p.z == tz && std::max(std::abs(p.x - tx), std::abs(p.y - ty)) <= 14) { p.mem["saw-thief"] = nowS; p.mem["knows-thief"] = nowS; }
        people.player["thief:" + town] = nowS;
        people.player["wanted:" + town] = nowS;
        people.player["thefts"] = std::to_string(numMem(people.player, "thefts") + 1);
        int best = -1, bd = 140;
        for (size_t i = 0; i < people.people.size(); i++) {
            const Person &p = people.people[i];
            if (!p.watch || !p.active || p.lying || p.town != town || p.onRoad) continue;
            int d = std::abs(p.x - tx) + std::abs(p.y - ty);
            if (d < bd) { bd = d; best = (int)i; }
        }
        if (best >= 0 && people.startChase(best)) {
            const Person &g = people.people[best];
            std::string who = people.label(g);
            if (!who.empty()) who[0] = (char)std::toupper((unsigned char)who[0]);
            say(message + " " + who + " comes running" + (bd > 40 ? ", from some way off." : "!"), 5.0);
            if (std::getenv("GALILEO_DEBUG_WATCH")) std::fprintf(stderr, "  [the watch: %s at %d,%d, %d tiles off]\n", g.name.c_str(), g.x, g.y, bd);
        } else people.notice = "Nobody of the watch is near. But they will remember you.";
    }
    std::string lawOf(const std::string &town) const {
        if (town == "Paris" || town == "Saint-Denis" || town == "London") return "In the King's name!";
        if (town == "Venice") return "In the name of the Most Serene Republic!";
        if (town == "Rome") return "In the name of His Holiness!";
        if (town == "Florence" || town == "Arcetri") return "In the name of the Grand Duke!";
        if (town == "Amsterdam") return "In the name of the Burgomasters!";
        return "In the name of the law!";
    }
    // the watch has you: the thing back, a fine, or a night in prison
    void arrest(int j) {
        Person &g = people.people[j];
        std::string town = townAt(tx, ty);
        if (town.empty()) town = owed.town;
        g.bubble = lawOf(town); g.bubbleT = 4.0f; g.hold = 4.0f;
        if (talking) { talking = false; people.endTalk(minutes); }
        std::string back;
        long fine = 60;   // five sous at the least
        if (owed.kind >= 0) {
            const ItemKind &k = items.kinds[owed.kind];
            fine = std::max(60L, 2 * owed.price);
            if (k.money) { fine += owed.price; back = "the purse"; }
            else {
                long have = numMem(people.player, "has:" + k.id), n = std::min<long>(have, owed.count);
                if (n > 0) {
                    if (have - n <= 0) people.player.erase("has:" + k.id); else people.player["has:" + k.id] = std::to_string(have - n);
                    items.put(world, owed.kind, (int)n, owed.x, owed.y, owed.z);
                    items.things.back().owner = owed.bld;
                    back = items.nameOf(owed.kind, (int)n);
                }
            }
        }
        long m = money();
        people.player.erase("wanted:" + town);
        people.player["arrests"] = std::to_string(numMem(people.player, "arrests") + 1);
        clearOwed();
        if (m >= fine) {
            people.player["money"] = std::to_string(m - fine);
            say(people.label(g) + ": \"" + lawOf(town) + "\" " + (back.empty() ? "" : "He takes back " + back + ", and y") + (back.empty() ? "Y" : "") + "ou pay a fine of " + moneyString(fine) + ".", 7.0);
            return;
        }
        people.player["money"] = "0";
        prison(town, back);
    }
    void prison(const std::string &town, const std::string &back) {
        auto it = people.prisons.find(town);
        std::string name, text;
        if (it != people.prisons.end()) {
            size_t bar = it->second.find('|');
            name = lowerS(trimS(it->second.substr(0, bar)));
            text = bar == std::string::npos ? "" : trimS(it->second.substr(bar + 1));
        }
        int id = 0;
        for (size_t b = 0; b < world.buildings.size() && !name.empty(); b++)
            if (lowerS(world.buildings[b].name).find(name) != std::string::npos && townAt(world.buildings[b].x0, world.buildings[b].y0) == town) { id = (int)b + 1; break; }
        std::string took = back.empty() ? "" : "They take back " + back + ". ";
        if (!id) { say(took + "You cannot pay the fine; they empty your pockets and let you go with a kick.", 7.0); return; }
        const Building &B = world.buildings[id - 1];
        int px = -1, py = -1;
        for (int Y = B.y0; Y <= B.y1 && px < 0; Y++)
            for (int X = B.x0; X <= B.x1 && px < 0; X++)
                if (bldAt(X, Y, 0) == id && world.passable(X, Y, 0)) { px = X; py = Y; }
        if (px >= 0) { tx = px; ty = py; tz = 0; from = to = vec2(px + 0.5f, py + 0.5f); stepT = 1; updatePlace(true); }
        double day = std::floor(minutes / 1440.0), t = day * 1440 + 7 * 60;
        if (t <= minutes) t += 1440;
        minutes = t;   // the night passes
        for (auto &p : people.people) { p.actIndex = -2; p.placed = false; p.path.clear(); }
        say(took + "You cannot pay the fine. " + (text.empty() ? "You spend the night in prison; in the morning they let you go." : text), 10.0);
    }
    // ---- fighting (C, then a click on whom). Your health is what you remember as "hp" (full
    // when there is none); you strike with the best weapon you carry and wear your best armour;
    // fencing (skill:fencing) helps you hit, and gives you more health. Beaten, you come round
    // in an inn, robbed, the next morning (defeated).
    int fightWith = -1;   // the person you are fighting, or -1
    int fightBeast = -1;  // or the wild beast (animals.list)
    float battleHold = 0; // seconds of fight music still to come (Music::setBattle)
    float swingT = 0, myStrike = 0;
    double lastHurt = -1e9, healFrom = -1;
    int maxHp() const { return 20 + 4 * (int)numMem(people.player, "skill:fencing"); }
    float hpNow() const {
        auto it = people.player.find("hp");
        return it == people.player.end() ? (float)maxHp() : (float)std::atof(it->second.c_str());
    }
    void setHp(float h) {
        h = std::max(0.0f, std::min((float)maxHp(), h));
        char buf[32]; std::snprintf(buf, sizeof buf, "%.1f", h);
        people.player["hp"] = buf;
    }
    int bestWeapon(std::string *name = nullptr) const {
        int d = 2; if (name) *name = "your fists";
        for (auto &c : carried()) { const ItemKind &k = items.kinds[c.first]; if (k.weapon > d) { d = k.weapon; if (name) *name = k.name; } }
        return d;
    }
    int bestArmour() const {
        int a = 0;
        for (auto &c : carried()) a = std::max(a, items.kinds[c.first].armour);
        return a;
    }
    std::string capital(std::string t) { if (!t.empty()) t[0] = (char)std::toupper((unsigned char)t[0]); return t; }
    void startFight(int who) {
        if (who < 0 || who >= (int)people.people.size()) return;
        Person &o = people.people[who];
        if (o.dead) { say(capital(people.label(o)) + " is dead.", 2.0); return; }
        // On the day you are one of a side ("player side=", the storming of the Alte Veste), a first strike at a man of
        // that side does not land: it says who he is; a second go at the same man strikes him all the same. Who was
        // asked about is kept in the saved game ("player own-side-asked ID"), so that it survives a reload.
        auto side = people.player.find("side");
        auto asked = people.player.find("own-side-asked");
        bool askedHim = asked != people.player.end() && asked->second == o.id;
        if (side != people.player.end() && !side->second.empty() && o.faction == side->second && !o.foe && !askedHim) {
            people.player["own-side-asked"] = o.id;
            std::string whose = o.role.rfind("scot", 0) == 0 ? "one of Monro's" : "one of the King's";
            say("That is " + people.label(o) + ", " + whose + ": your own side. (C on him again: strike him all the same.)", 4.0);
            return;
        }
        people.player.erase("own-side-asked");
        if (people.talk.who == who) { people.endTalk(minutes); talking = false; }
        fightWith = who;
        swingT = 0.3f;
        o.foe = true;
        if (o.fighter) o.target = -2;
        else { o.fleeT = 20; o.target = -2; o.bubble = "Help! Murder!"; o.bubbleT = 2.5f; }
        std::string w;
        bestWeapon(&w);
        say("You go for " + people.label(o) + " with " + w + ". (C again: stop fighting.)", 3.0);
    }
    void startFightBeast(int i) {
        const AnimalKind &k = animals.kinds[animals.list[i].kind];
        if (people.talk.who >= 0) { people.endTalk(minutes); talking = false; }
        fightBeast = i;
        fightWith = -1;
        swingT = 0.3f;
        std::string w;
        bestWeapon(&w);
        if (k.hostile > 0) animals.list[i].after = true;
        say("You go for the " + k.name.substr(k.name.find(' ') + 1) + " with " + w + ". (C again: stop fighting.)", 3.0);
    }
    void fight(float dt) {
        {   // "player under-attack": someone is going for you now, within a dozen steps (the wounded Scot will not be
            // helped up while the Croats are about: data/people/zirndorf.txt)
            bool attacked = false;
            for (const Person &p : people.people)
                if (!p.dead && !p.away && p.target == -2 && p.fleeT <= 0 && p.z == tz && std::max(std::abs(p.x - tx), std::abs(p.y - ty)) <= 12) { attacked = true; break; }
            if (attacked != (people.player.count("under-attack") > 0)) {
                if (attacked) people.player["under-attack"] = "1"; else people.player.erase("under-attack");
            }
        }
        followerStep();
        int fencing = (int)numMem(people.player, "skill:fencing");
        people.playerSkill = fencing;
        people.playerArmour = bestArmour();
        animals.playerSkill = fencing; animals.playerArmour = people.playerArmour; animals.playerDown = people.playerDown;
        if (animals.playerHurt > 0) { people.playerHurt += animals.playerHurt; animals.playerHurt = 0; }   // bitten
        for (auto &n : animals.notes) people.fightNotes.push_back(n);
        animals.notes.clear();
        if (fightBeast >= 0) {   // you go for a wolf, a bear, a deer: a blow a second while beside it
            Animal &a = animals.list[fightBeast];
            const AnimalKind &k = animals.kinds[a.kind];
            float d = std::max(std::fabs(a.x - (tx + 0.5f)), std::fabs(a.y - (ty + 0.5f)));
            if (a.gone || a.dead || d > 12) fightBeast = -1;
            else if (d <= 1.6f && stepT >= 1) {
                facingTarget = std::atan2(-(a.x - (tx + 0.5f)), a.y - (ty + 0.5f));
                swingT -= dt;
                if (swingT <= 0) {
                    swingT = 1.0f;
                    myStrike = 0.35f;
                    lastHurt = minutes;
                    int dmg = people.roll(fencing, bestWeapon(), k.skill, 0);
                    std::string what = "the " + k.name.substr(k.name.find(' ') + 1);
                    if (soundsLoaded && dmg >= 0) sounds.play("sword", dmg > 0 ? 1.0f : 0.7f);
                    if (dmg <= 0) say("You strike at " + what + ", and miss.", 2.5);
                    else {
                        animals.hurt(fightBeast, (float)dmg, minutes);
                        say(a.dead ? "You strike " + what + " down, and it does not get up." : "You wound " + what + " (" + std::to_string(dmg) + ").", 2.5);
                        if (a.dead && k.id == "wolf") people.player["wolves-fought"] = std::to_string(numMem(people.player, "wolves-fought") + 1);
                        if (a.dead || a.fleeT > 0) fightBeast = -1;
                    }
                }
            }
        }
        if (people.playerHurt > 0) {
            if (soundsLoaded) sounds.play("sword", 0.9f);
            setHp(hpNow() - people.playerHurt);
            people.playerHurt = 0;
            lastHurt = minutes;
        }
        for (auto &n : people.fightNotes) say(n, 3.0);
        people.fightNotes.clear();
        if (hpNow() <= 0) { defeated(); return; }
        if (myStrike > 0) myStrike = std::max(0.0f, myStrike - dt);
        if (fightWith >= 0) {
            Person &o = people.people[fightWith];
            int d = std::max(std::abs(o.x - tx), std::abs(o.y - ty));
            if (o.dead || o.z != tz || d > 12) { fightWith = -1; }
            else if (d <= 1 && stepT >= 1) {
                facingTarget = std::atan2(-(float)(o.x - tx), (float)(o.y - ty));
                swingT -= dt;
                if (swingT <= 0) {
                    swingT = 1.0f;
                    myStrike = 0.35f;
                    lastHurt = minutes;   // fighting: no healing
                    int dmg = people.roll(fencing, bestWeapon(), o.skill, o.armour);
                    if (soundsLoaded && dmg >= 0) sounds.play("sword", dmg > 0 ? 1.0f : 0.7f);
                    std::string who = people.label(o);
                    if (dmg < 0) say("You strike at " + who + ", and miss.", 2.5);
                    else if (dmg == 0) say("Your blow glances off " + who + "'s armour.", 2.5);
                    else {
                        say("You wound " + who + " (" + std::to_string(dmg) + ").", 2.5);
                        people.hurt(fightWith, (float)dmg, -2, minutes);
                        for (auto &n : people.fightNotes) say(n, 3.5);
                        people.fightNotes.clear();
                        if (o.dead || o.fleeT > 0) fightWith = -1;
                    }
                }
            }
        }
        // healing: a point of health every twenty minutes once nothing has hurt you for ten
        if (healFrom < 0 || minutes < healFrom) healFrom = minutes;
        float gain = (float)(minutes - healFrom) / 20.0f;
        healFrom = minutes;
        if (hpNow() < maxHp() && minutes - lastHurt > 10 && fightWith < 0) setHp(hpNow() + gain);
    }
    // beaten: you come round in an inn the next morning, a barber-surgeon's bandage on your
    // head, robbed of half your money and of the trade goods you carried (never of the things
    // of a story)
    void defeated() {
        fightWith = -1;
        fightBeast = -1;
        bool byBand = bandState != 0;   // brigands take all you had, not half
        if (bandState) disband();
        for (auto &a : animals.list) if (a.wildOne && !a.dead) a.gone = true;   // the wolves, the bear: gone off
        for (auto &p : people.people) if (p.target == -2) { p.target = -1; p.path.clear(); p.actIndex = -2; p.foe = false; }
        people.player.erase("under-attack");
        auto cd = people.player.find("carried-down");   // "carried-down=TOWN": beaten there on the day of the storming of the
        // Alte Veste, or by the Croats after it (events.txt alte-veste), your own side carries you down to the hospital, and
        // nothing is taken (because you showed courage, and you were presumably carried to the
        // hospital by your own side)
        if (cd != people.player.end() && lowerS(cd->second) == lowerS(people.player["town"])) {
            int hb = -1;
            for (size_t b = 0; b < world.buildings.size(); b++) if (lowerS(world.buildings[b].name) == "the hospital of the holy ghost") { hb = (int)b; break; }
            if (hb >= 0) {
                const Building &B = world.buildings[hb];
                int px = -1, py = -1;
                for (int Y = B.y0; Y <= B.y1 && px < 0; Y++)
                    for (int X = B.x0; X <= B.x1 && px < 0; X++)
                        if (bldAt(X, Y, 0) == hb + 1 && world.passable(X, Y, 0) && !people.occupied(X, Y, 0)) { px = X; py = Y; }
                if (px >= 0) {
                    tx = px; ty = py; tz = 0; from = to = vec2(px + 0.5f, py + 0.5f); stepT = 1; updatePlace(true);
                    double day = std::floor(minutes / 1440.0), t = day * 1440 + 8 * 60;
                    if (t <= minutes + 8 * 60) t += 1440;
                    minutes = t;
                    healFrom = minutes; lastHurt = -1e9;
                    setHp(maxHp() * 0.5f);
                    for (auto &p : people.people) { p.actIndex = -2; p.placed = false; p.path.clear(); }
                    people.player.erase("side");
                    people.player["carried-down-at"] = std::to_string((long)minutes);
                    say("Everything goes dark. You come round on straw in the hospital of the Holy Ghost, in Nuremberg, the next morning, among the wounded of the storming, with your head bound up. The Scots carried you down off the hill, and nobody has touched your purse.", 12.0);
                    people.player["defeats"] = std::to_string(numMem(people.player, "defeats") + 1);
                    return;
                }
            }
        }
        long m = money(), lost = byBand ? m : m / 2;
        people.player["money"] = std::to_string(m - lost);
        std::string goods;
        std::set<std::string> trade;
        for (auto &mk : people.markets) { for (auto &x : mk.second.makes) trade.insert(x.first); for (auto &x : mk.second.wants) trade.insert(x.first); }
        for (auto &c : carried()) {
            const ItemKind &k = items.kinds[c.first];
            if (!trade.count(k.id)) continue;
            people.player.erase("has:" + k.id);
            goods += (goods.empty() ? "" : ", ") + items.nameOf(c.first, c.second);
        }
        // the nearest inn: in this town, or in the nearest
        int best = -1; long bd = 1L << 60;
        for (size_t b = 0; b < world.buildings.size(); b++) {
            const Building &B = world.buildings[b];
            if (B.kind != "inn") continue;
            long dx = (B.x0 + B.x1) / 2 - tx, dy = (B.y0 + B.y1) / 2 - ty, d = dx * dx + dy * dy;
            if (d < bd) { bd = d; best = (int)b; }
        }
        std::string where = "a cart by the road";
        if (best >= 0) {
            const Building &B = world.buildings[best];
            int px = -1, py = -1;
            for (int Y = B.y0; Y <= B.y1 && px < 0; Y++)
                for (int X = B.x0; X <= B.x1 && px < 0; X++)
                    if (bldAt(X, Y, 0) == best + 1 && world.passable(X, Y, 0) && !people.occupied(X, Y, 0)) { px = X; py = Y; }
            if (px >= 0) { tx = px; ty = py; tz = 0; from = to = vec2(px + 0.5f, py + 0.5f); stepT = 1; updatePlace(true); where = B.name; }
        }
        double day = std::floor(minutes / 1440.0), t = day * 1440 + 8 * 60;
        if (t <= minutes + 8 * 60) t += 1440;
        minutes = t;
        healFrom = minutes; lastHurt = -1e9;
        setHp(maxHp() * 0.5f);
        for (auto &p : people.people) { p.actIndex = -2; p.placed = false; p.path.clear(); }
        say("Everything goes dark. You come round in " + where + ", the next morning, with your head bound up: a barber-surgeon has dressed your wounds, and been paid for it out of your purse. " +
            (lost > 0 ? "You have lost " + moneyString(lost) + (goods.empty() ? "." : ", and " + goods + ".") : goods.empty() ? "" : "You have lost " + goods + "."), 12.0);
        people.player["defeats"] = std::to_string(numMem(people.player, "defeats") + 1);
    }
    // ---- the country (data/wild.txt): brigands on the roads, wolves and bears, deer to hunt.
    // Every 25 tiles you travel in the open country (not in a town, a post house or a boat) the
    // game weighs what you might meet there, by the hour, the season, the woods and hills about
    // you, your company and the country (its nearest town's); the wild beasts are Animals with
    // AnimalKind::wild, the brigands, your escort and the gamekeeper People with Person::wild.
    struct WildLand { float brigands = 1, wolves = 1, bears = 0.5f; };
    struct Royal { std::string town, owner, name; float radius = 0; };
    float wBrigDay = 1.5f, wBrigNight = 6, wWolfNight = 5, wWolfDusk = 2.5f, wBears = 1.2f, wDeerDay = 12, wDeerDusk = 24, wEscort = 0.15f;
    std::map<std::string, WildLand> wildLands;
    std::vector<Royal> royals;
    void loadWild(const std::string &fn) {
        std::ifstream in(fn);
        if (!in) return;
        std::string line;
        while (std::getline(in, line)) {
            std::string t = trimS(line);
            if (t.empty() || t[0] == '#') continue;
            std::istringstream ss(t);
            std::string w0;
            ss >> w0;
            if (w0 == "brigands") ss >> wBrigDay >> wBrigNight;
            else if (w0 == "wolves") ss >> wWolfNight >> wWolfDusk;
            else if (w0 == "bears") ss >> wBears;
            else if (w0 == "deer") ss >> wDeerDay >> wDeerDusk;
            else if (w0 == "escort") ss >> wEscort;
            else if (w0 == "land") {
                std::string name, k;
                ss >> name;
                for (auto &c : name) if (c == '_') c = ' ';
                WildLand L;
                float v;
                while (ss >> k >> v) { if (k == "brigands") L.brigands = v; else if (k == "wolves") L.wolves = v; else if (k == "bears") L.bears = v; }
                wildLands[name] = L;
            } else if (w0 == "royal") {   // royal TOWN RADIUS OWNER | NAME
                Royal r;
                ss >> r.town >> r.radius;
                for (auto &c : r.town) if (c == '_') c = ' ';
                std::string rest;
                std::getline(ss, rest);
                size_t bar = rest.find('|');
                r.owner = trimS(rest.substr(0, bar));
                r.name = bar == std::string::npos ? r.owner : trimS(rest.substr(bar + 1));
                if (!settlementNamed(r.town)) std::fprintf(stderr, "wild.txt: no town called %s\n", r.town.c_str());
                else royals.push_back(r);
            }
        }
    }
    float wildTiles = 0;                 // tiles travelled in the open country since the last weighing
    int wildLastX = -99999, wildLastY = 0, wildPrevX = 0, wildPrevY = 0;
    double lastBand = -1e9, lastWolves = -1e9, lastBear = -1e9;
    int bandState = 0;                   // 0 none, 1 coming up the road, 2 talking, 3 fighting, 4 going off
    double bandSince = 0;
    bool escortBack = false;             // your escort put back beside you after loading a game
    float wuni() { static std::mt19937 r((unsigned)std::random_device{}()); return std::uniform_real_distribution<float>(0, 1)(r); }
    bool inCountry() const { return curTown.empty() && tz == 0 && boat < 0 && sceneNow < 0 && !creating; }
    std::string nearestPlace(int X, int Y, float &dist) const {   // the nearest town or post house, and how far its edge is
        std::string best;
        dist = 1e9f;
        for (auto &st : world.settlements) {
            float dx = std::max({(float)st.x0 - X, 0.0f, (float)X - (st.x0 + st.w)});
            float dy = std::max({(float)st.y0 - Y, 0.0f, (float)Y - (st.y0 + st.h)});
            float d = std::hypot(dx, dy);
            if (d < dist) { dist = d; best = st.name; }
        }
        return best;
    }
    int treesNear(int X, int Y, int r) {
        int n = 0;
        for (int y = Y - r; y <= Y + r; y++)
            for (int x = X - r; x <= X + r; x++)
                if (world.inWorld(x, y)) { int f = world.tile(x, y).feature; if (f == F_OAK || f == F_PINE) n++; }
        return n;
    }
    bool hilly(int X, int Y) {   // rock, snow or scrub about you, or high ground
        int n = 0;
        for (int y = Y - 6; y <= Y + 6; y += 2)
            for (int x = X - 6; x <= X + 6; x += 2)
                if (world.inWorld(x, y)) { int t = world.tile(x, y).terrain; if (t == T_ROCK || t == T_SNOW || t == T_SCRUB) n++; }
        return n >= 6 || world.groundZ(X + 0.5f, Y + 0.5f) > kHillHeight;
    }
    static constexpr float kHillHeight = 6.0f;
    const Royal *royalAt(int X, int Y) const {   // the royal grounds you are in, if any
        for (auto &r : royals) {
            const Settlement *s = settlementNamed(r.town);
            if (!s) continue;
            float dx = std::max({(float)s->x0 - X, 0.0f, (float)X - (s->x0 + s->w)});
            float dy = std::max({(float)s->y0 - Y, 0.0f, (float)Y - (s->y0 + s->h)});
            if (std::hypot(dx, dy) < r.radius) return &r;
        }
        return nullptr;
    }
    // a spot to put something you meet: open ground, not in a building, between rmin and rmax
    // tiles from you, ahead of you if ahead
    bool wildSpot(int rmin, int rmax, bool ahead, int &X, int &Y) {
        float face = facing;   // where you look: facing 0 is +y
        for (int tries = 0; tries < 80; tries++) {
            float ang = ahead ? face + (wuni() * 2 - 1) * 0.7f : wuni() * 6.2832f, d = rmin + wuni() * (rmax - rmin);
            int x = tx + (int)std::lround(-std::sin(ang) * d), y = ty + (int)std::lround(std::cos(ang) * d);
            if (!world.inWorld(x, y) || !world.passable(x, y, 0) || world.tile(x, y).bld || people.occupied(x, y, 0)) continue;
            int t = world.tile(x, y).terrain;
            if (t == T_WATER || t == T_DEEP || t == T_STREET || t == T_SQUARE) continue;
            bool inTown = false;
            for (auto &st : world.settlements) if (st.inside(x, y)) inTown = true;
            if (inTown) continue;
            X = x; Y = y;
            return true;
        }
        return false;
    }
    int spawnWild(const std::string &kind, int n, int rmin, int rmax, bool after) {
        int k = animals.kindOf(kind);
        if (k < 0) return 0;
        int cx, cy;
        if (!wildSpot(rmin, rmax, false, cx, cy)) return 0;
        int made = 0, first = -1;
        for (int i = 0, tries = 0; i < n && tries < 40; tries++) {
            int x = cx + (int)std::lround((wuni() * 2 - 1) * 2.5f), y = cy + (int)std::lround((wuni() * 2 - 1) * 2.5f);
            if (!world.inWorld(x, y) || !world.passable(x, y, 0) || world.tile(x, y).bld) continue;
            int a = animals.spawn(k, x + 0.5f, y + 0.5f, (uint32_t)(wuni() * 4e9f));
            Animal &an = animals.list[a];
            if (first < 0) first = a;
            an.herd = first;
            an.hx = cx + 0.5f; an.hy = cy + 0.5f;
            an.after = after;
            an.facing = std::atan2(-(tx + 0.5f - an.x), ty + 0.5f - an.y);
            i++; made++;
        }
        return made;
    }
    // what you might meet, weighed every 25 tiles of country
    // Setting out from a town with a lot of money: once a day, a word on the roads' dangers and their
    // remedies (the bill of exchange, the hired sword, the horse). Not in the prologue, where nothing on the roads robs you.
    std::string purseWarning() {
        bool prologue = people.player.count("me:age") && !people.player.count("gave-mersenne-dialogue");
        long m = money();
        if (prologue || m < 4800 || escortActive()) return "";
        auto it = people.player.find("purse-warned-at");
        if (it != people.player.end() && minutes - std::atof(it->second.c_str()) < 1440) return "";
        people.player["purse-warned-at"] = std::to_string((long)minutes);
        return "You have " + moneyString(m) + " on you, and the roads are not safe. A money-changer would write you a bill of exchange for most of it, and an innkeeper can find you a hired sword. " +
               std::string(riding >= 0 ? "And if men are waiting on the road, keep your distance: on a horse you can leave them behind." : "On a horse, you could keep your distance from men waiting on the road; on foot, they will catch you.");
    }
    void wildRoll(bool force = false, const std::string &only = "") {
        double h = std::fmod(minutes, 1440.0) / 60.0;
        bool night = h >= 21 || h < 5, dusk = (h >= 5 && h < 7.5) || (h >= 18 && h < 21);
        int month = monthAt(minutes);
        bool winter = month >= 11 || month <= 2;
        float dTown;
        std::string town = nearestPlace(tx, ty, dTown);
        std::string land = countryOf(town);
        WildLand L = wildLands.count(land) ? wildLands[land] : WildLand();
        bool woods = treesNear(tx, ty, 8) >= 4, hills = hilly(tx, ty);
        bool road = false;
        for (int y = ty - 2; y <= ty + 2 && !road; y++)
            for (int x = tx - 2; x <= tx + 2 && !road; x++)
                if (world.inWorld(x, y) && world.tile(x, y).terrain == T_ROAD) road = true;
        bool escorted = escortActive();
        auto roll = [&](float per100) { return force || wuni() < per100 / 400.0f; };
        // in the prologue (Mersenne's courier, until he has his Dialogue) nothing on the roads takes
        // the money its errands need: no brigands, wolves or bears yet (deer and fish, yes)
        bool prologue = people.player.count("me:age") && !people.player.count("gave-mersenne-dialogue");
        if (std::getenv("GALILEO_DEBUG_WILD"))
            std::fprintf(stderr, "  [the country at %d,%d: %s, %.0f tiles from %s, %s%s%s%s%s, ground %.1f]\n", tx, ty, land.c_str(), dTown, town.c_str(),
                         night ? "night" : dusk ? "dusk" : "day", woods ? ", woods" : "", hills ? ", hills" : "", road ? ", road" : "", escorted ? ", escorted" : "",
                         world.groundZ(tx + 0.5f, ty + 0.5f));
        if ((only.empty() || only == "band") && bandState == 0 && (force || (!prologue && dTown > 30 && minutes - lastBand > 240))) {
            float c = (night ? wBrigNight : wBrigDay) * L.brigands * ((woods || hills) ? 1.4f : 1.0f) * (road ? 1.0f : 0.5f) * (escorted ? wEscort : 1.0f);
            if (roll(c)) { spawnBand(land, night); return; }
        }
        if ((only.empty() || only == "wolf") && (force || (!prologue && dTown > 25 && (woods || hills) && minutes - lastWolves > 180 && (night || (winter && dusk))))) {
            float c = (night ? wWolfNight : wWolfDusk) * L.wolves * (escorted ? 0.5f : 1.0f) * (riding >= 0 ? 0.6f : 1.0f);
            if (roll(c) && spawnWild("wolf", 2 + (int)(wuni() * 3) + (winter ? 1 : 0), 16, 22, true)) {
                lastWolves = minutes;
                say(night ? "Somewhere in the dark a wolf howls, and another answers it, nearer." : "A wolf howls in the trees, close by, and another answers it.", 5.0);
                return;
            }
        }
        if ((only.empty() || only == "bear") && (force || (!prologue && dTown > 40 && hills && minutes - lastBear > 600))) {
            if (roll(wBears * L.bears) && spawnWild("bear", 1, 9, 15, false)) {
                lastBear = minutes;
                say("A bear is among the rocks not far off, turning over stones. It has not seen you yet.", 5.0);
                return;
            }
        }
        if ((only.empty() || only == "deer") && (force || (dTown > 15 && woods && !night))) {
            int live = 0;
            for (auto &a : animals.list) if (a.wildOne && !a.gone && !a.dead && animals.kinds[a.kind].id == "deer") live++;
            if (live < 6 && roll(dusk ? wDeerDusk : wDeerDay) && spawnWild("deer", 2 + (int)(wuni() * 4), 12, 24, false))
                say("Deer are grazing at the edge of the trees, a little way off.", 4.0);
        }
    }
    // every moment: the tiles you travel in the country, and what comes of them
    void wildStep(float dt) {
        (void)dt;
        if (wildLastX < -9999) { wildLastX = tx; wildLastY = ty; wildPrevX = tx; wildPrevY = ty; }
        int moved = std::max(std::abs(tx - wildLastX), std::abs(ty - wildLastY));
        if (moved > 0) {
            if (moved <= 3) {
                wildPrevX = wildLastX; wildPrevY = wildLastY;
                if (inCountry()) wildTiles += (float)moved;
                escortFollow();
            } else if (escortActive()) placeEscortBeside();   // put somewhere else (a passage, a night in an inn): he comes too
            wildLastX = tx; wildLastY = ty;
        }
        while (wildTiles >= 25) { wildTiles -= 25; if (inCountry()) wildRoll(); }
        if (!curTown.empty() && bandState == 1) {   // into a town or a post house: they do not follow
            disband();
            say("The men on the road behind you stop at the edge of the houses, and go back.", 4.0);
        }
        bandStep();
        escortStep();
        double h = std::fmod(minutes, 1440.0) / 60.0;
        bool day = h >= 7.5 && h < 18;
        for (size_t i = 0; i < animals.list.size(); i++) {   // the wild beasts: gone once far behind you, the dead after a while
            Animal &a = animals.list[i];
            if (!a.wildOne || a.gone) continue;
            const AnimalKind &k = animals.kinds[a.kind];
            float d = std::max(std::fabs(a.x - tx), std::fabs(a.y - ty));
            bool far = d > 90 || (a.dead && minutes - a.deadAt > 240) || (k.night && day && !a.after && !a.dead && d > 20);
            if (far) { a.gone = true; if (fightBeast == (int)i) fightBeast = -1; continue; }
            if (!curTown.empty() && a.after) { a.after = false; a.fleeT = 8; }   // not into a town
        }
    }
    // ---- brigands: a band comes up the road; their leader talks first (pay, or not)
    void spawnBand(const std::string &land, bool night) {
        if (people.band.empty()) return;
        int cx, cy;
        if (!wildSpot(13, 19, true, cx, cy)) return;
        int n = std::min((int)people.band.size(), 2 + (int)(wuni() * 2) + (night ? 1 : 0));
        int placed = 0;
        std::set<std::pair<int, int>> taken;
        for (int i = 0; i < (int)people.band.size(); i++) {
            Person &p = people.people[people.band[i]];
            p.away = true;
            if (placed >= n) continue;
            int x = -1, y = -1;
            for (int tries = 0; tries < 30 && x < 0; tries++) {
                int qx = cx + (int)std::lround((wuni() * 2 - 1) * 2), qy = cy + (int)std::lround((wuni() * 2 - 1) * 2);
                if (world.inWorld(qx, qy) && world.passable(qx, qy, 0) && !world.tile(qx, qy).bld && !people.occupied(qx, qy, 0) && !taken.count({qx, qy})) { x = qx; y = qy; }
            }
            if (x < 0) continue;
            taken.insert({x, y});
            p.away = false; p.dead = false; p.lying = false; p.hp = (float)p.maxHp;
            p.x = p.px = x; p.y = p.py = y; p.z = p.pz = 0; p.stepT = 1;
            p.target = -2; p.parley = true; p.foe = false; p.fleeT = 0; p.chase = 0;
            p.path.clear(); p.pathPos = 0; p.chaseSteps = 99; p.strikeT = 0.6f;
            p.facing = std::atan2(-(float)(tx - x), (float)(ty - y));
            p.mem["land"] = land;
            p.mem.erase("dead-at");
            // the look of the country's robbers: a soldier's buff coat in the Empire, a wide hat in Spain
            static const vec3 coats[3] = {{0.29f, 0.24f, 0.19f}, {0.36f, 0.29f, 0.2f}, {0.24f, 0.22f, 0.2f}};
            p.look.coat = coats[i % 3];
            p.look.weapon = i == 1 ? 1 : (i == 0 ? 4 : 3);
            if (land == "Empire" || land == "Bohemia") p.look.coat = vec3(0.55f, 0.42f, 0.25f) * (0.8f + 0.1f * i);
            if (land == "Ottoman") { p.look.coat = vec3(0.45f, 0.14f, 0.12f) * (0.8f + 0.1f * i); p.look.hatKind = 2; }
            placed++;
        }
        if (!placed) return;
        bandState = 1;
        bandSince = minutes;
        lastBand = minutes;
        people.parleyArrived = -1;
        say(night ? "Shapes move on the road ahead of you in the dark: men, waiting." : "Men step out onto the road ahead of you, and wait.", 5.0);
    }
    bool inBand(int i) const { return std::find(people.band.begin(), people.band.end(), i) != people.band.end(); }
    void bandAttack(const std::string &cry) {
        if (talking) { people.endTalk(minutes); talking = false; }
        bool first = true;
        for (int i : people.band) {
            Person &p = people.people[i];
            if (p.away || p.dead) continue;
            p.parley = false; p.foe = true; p.target = -2; p.path.clear(); p.pathPos = 0; p.chaseSteps = 99;
            if (first && !cry.empty()) { p.bubble = cry; p.bubbleT = 3.0f; first = false; }
        }
        bandState = 3;
        bandSince = minutes;
    }
    void bandLeave() {   // off into the trees with what they took
        if (talking) { people.endTalk(minutes); talking = false; }
        for (int i : people.band) {
            Person &p = people.people[i];
            if (p.away || p.dead) continue;
            p.parley = false; p.foe = false; p.target = -2; p.fleeT = 14;   // running from you
        }
        bandState = 4;
        bandSince = minutes;
    }
    void disband() {
        for (int i : people.band) {
            Person &p = people.people[i];
            if (p.dead) continue;
            p.away = true; p.parley = false; p.target = -1; p.foe = false; p.fleeT = 0; p.chase = 0;
        }
        bandState = 0;
        people.parleyArrived = -1;
    }
    void bandStep() {
        if (bandState == 0) { people.parleyArrived = -1; return; }
        int alive = 0, far = 0, after = 0, fighting = 0;
        for (int i : people.band) {
            const Person &p = people.people[i];
            if (p.away || p.dead) continue;
            alive++;
            if (std::max(std::abs(p.x - tx), std::abs(p.y - ty)) > 24) far++;
            if (p.target == -2 && p.fleeT <= 0) after++;
            if (p.target >= 0) fighting++;   // at your escort
        }
        if (bandState == 1) {   // coming up the road
            if (fighting) { bandAttack("At them!"); return; }
            if (people.parleyArrived >= 0 && inBand(people.parleyArrived)) {
                int who = people.parleyArrived;
                people.parleyArrived = -1;
                if (talking) people.endTalk(minutes);
                people.startTalk(world, who, minutes);
                talking = true;
                bandState = 2;
                if (testMode) {
                    std::fprintf(stderr, "  [%s stops you on the road]\n", people.label(people.people[who]).c_str());
                    for (auto &l : people.talk.lines) std::fprintf(stderr, "    %s\n", l.second.c_str());
                }
                return;
            }
            if (alive == 0 || (after == 0 && far == alive) || minutes - bandSince > 90) {
                if (alive) say("The men on the road give up, and go back into the trees.", 4.0);
                disband();
            }
            return;
        }
        if (bandState == 2) {   // talking: walk off without paying, and they take it off you
            if (fighting) { bandAttack("At them!"); return; }
            if (!talking || people.talk.who < 0 || people.talk.over) bandAttack("Then we take it off you!");
            return;
        }
        if (bandState == 3) {   // fighting, until they are dead or run, or you are gone
            int standing = 0;
            for (int i : people.band) {
                const Person &p = people.people[i];
                if (!p.away && !p.dead && p.fleeT <= 0 && std::max(std::abs(p.x - tx), std::abs(p.y - ty)) <= 24) standing++;
            }
            if (standing == 0) {
                int dead = 0;
                for (int i : people.band) if (people.people[i].dead) dead++;
                if (dead) people.player["brigands-killed"] = std::to_string(numMem(people.player, "brigands-killed") + dead);
                people.player["bands-beaten"] = std::to_string(numMem(people.player, "bands-beaten") + 1);
                say(alive > 0 && far < alive ? "The last of them runs off into the trees." : "The road is quiet again.", 4.0);
                for (int i : people.band) { Person &p = people.people[i]; if (!p.dead) { p.away = true; p.target = -1; p.foe = false; p.fleeT = 0; } }
                bandState = 0;
            }
            return;
        }
        if (bandState == 4 && (far == alive || minutes - bandSince > 20)) disband();   // gone, with your money
    }
    // ---- your escort: a hired sword who walks behind you for a day, and fights the brigands
    bool escortActive() const {
        if (people.escortIdx < 0) return false;
        const Person &e = people.people[people.escortIdx];
        return !e.away && !e.dead && minutes < numMem(people.player, "escort-until");
    }
    void placeEscortBeside() {
        if (people.escortIdx < 0) return;
        placeBeside(people.people[people.escortIdx]);
    }
    void placeBeside(Person &e) {   // your escort, or whoever walks behind you, put on a free tile next to you
        int bx = -1, by = -1;
        for (int r = 1; r <= 2 && bx < 0; r++)
            for (int dy = -r; dy <= r && bx < 0; dy++)
                for (int dx = -r; dx <= r && bx < 0; dx++)
                    if ((dx || dy) && world.passable(tx + dx, ty + dy, tz) && !people.occupied(tx + dx, ty + dy, tz)) { bx = tx + dx; by = ty + dy; }
        if (bx < 0) return;
        e.away = false; e.dead = false; e.lying = false; e.target = -1; e.fleeT = 0; e.foe = false; e.chase = 0;
        e.x = e.px = bx; e.y = e.py = by; e.z = e.pz = tz; e.stepT = 1; e.path.clear(); e.pathPos = 0;
        e.facing = facing;
    }
    void escortFollow() {   // when you step, he steps into the tile you left
        if (!escortActive()) return;
        Person &e = people.people[people.escortIdx];
        if (e.target != -1 || e.fleeT > 0 || people.talk.who == people.escortIdx) return;
        int d = std::max(std::abs(e.x - tx), std::abs(e.y - ty));
        if (d <= 1 && e.z == tz) return;
        if (d > 6 || e.z != tz) { placeEscortBeside(); return; }
        if (!world.passable(wildPrevX, wildPrevY, tz) || people.occupied(wildPrevX, wildPrevY, tz)) return;
        e.px = e.x; e.py = e.y; e.pz = e.z;
        e.facing = std::atan2(-(float)(wildPrevX - e.x), (float)(wildPrevY - e.y));
        e.x = wildPrevX; e.y = wildPrevY;
        e.stepT = 0; e.stepDur = std::max(0.15f, stepDur);
    }
    // ---- someone who is not a hired sword walking behind you ({follow} in data/people: a wounded Scot helped
    // down off the Alte Veste, until Meister Franz takes him: {unfollow}). Called from fight(), which runs
    // wherever time passes, in the game and in every loop of the stand-in.
    int followLastX = INT32_MIN, followLastY = 0, followPrevX = 0, followPrevY = 0;
    Person *follower() {
        auto f = people.player.find("follower");
        if (f == people.player.end()) return nullptr;
        Person *p = people.byId(f->second);
        return p && !p->dead ? p : nullptr;
    }
    void followerStep() {
        if (followLastX == INT32_MIN || std::max(std::abs(tx - followLastX), std::abs(ty - followLastY)) > 3) {
            followLastX = followPrevX = tx; followLastY = followPrevY = ty;
        } else if (tx != followLastX || ty != followLastY) {
            followPrevX = followLastX; followPrevY = followLastY;
            followLastX = tx; followLastY = ty;
        }
        Person *f = follower();
        if (!f || people.talk.who == (int)(f - people.people.data())) return;
        int d = std::max(std::abs(f->x - tx), std::abs(f->y - ty));
        if (d > 6 || f->z != tz) { placeBeside(*f); return; }
        if (d <= 1 || f->stepT < 1) return;
        if (!world.passable(followPrevX, followPrevY, tz) || people.occupied(followPrevX, followPrevY, tz)) return;
        f->px = f->x; f->py = f->y; f->pz = f->z;
        f->facing = std::atan2(-(float)(followPrevX - f->x), (float)(followPrevY - f->y));
        f->x = followPrevX; f->y = followPrevY;
        f->stepT = 0; f->stepDur = std::max(0.2f, stepDur * 1.3f);   // he hops: a little slower than you
    }
    void escortStep() {
        if (people.escortIdx < 0) return;
        Person &e = people.people[people.escortIdx];
        double until = numMem(people.player, "escort-until");
        if (until <= 0) return;
        if (e.dead) { people.player.erase("escort-until"); say("Your hired sword is dead.", 4.0); return; }
        if (minutes >= until) {
            people.player.erase("escort-until");
            if (!e.away) say("Your hired sword touches his hat: his day is up. He turns back the way you came.", 5.0);
            e.away = true; e.target = -1;
            return;
        }
        if (e.away && !escortBack) { escortBack = true; placeEscortBeside(); }   // after loading a game
    }
    // ---- the gamekeeper: a shot in the royal grounds may bring him, and a deer about you is poaching
    void keeperHears(const Royal *r) {
        if (!r || people.keeperIdx < 0) return;
        Person &k = people.people[people.keeperIdx];
        if (!k.away || wuni() > 0.65f) return;
        int x, y;
        if (!wildSpot(16, 24, false, x, y)) return;
        k.away = false; k.dead = false; k.lying = false; k.hp = (float)k.maxHp; k.target = -1; k.fleeT = 0; k.foe = false;
        k.x = k.px = x; k.y = k.py = y; k.z = k.pz = 0; k.stepT = 1; k.path.clear(); k.pathPos = 0;
        k.mem["owner"] = r->owner;
        k.mem["grounds"] = r->name;
        people.startChase(people.keeperIdx);
        k.bubble = "Hold there, in " + r->owner + " name!";
        k.bubbleT = 3.0f;
        people.player["keeper-grounds"] = r->name;
        say("A man in green is running towards you through the trees, with a dog at his heels: a gamekeeper, who heard the shot.", 5.0);
    }
    void keeperCaught() {   // he has you: the deer, or not
        Person &k = people.people[people.keeperIdx];
        std::string owner = k.mem.count("owner") ? k.mem["owner"] : std::string("the King's");
        bool deer = numMem(people.player, "has:venison") > 0;
        for (auto &a : animals.list)
            if (a.wildOne && !a.gone && a.dead && animals.kinds[a.kind].id == "deer" && std::max(std::abs(a.x - tx), std::abs(a.y - ty)) < 7) deer = true;
        if (talking) { people.endTalk(minutes); talking = false; }
        k.away = true; k.chase = 0;
        if (!deer) {
            say(people.label(k) + " looks at your gun, and at the ground, and at you. \"A shot, in " + owner + " woods. If I find a deer with your ball in it, I will find you.\" He whistles up his dog and goes.", 7.0);
            return;
        }
        people.player.erase("has:venison");
        people.player.erase("has:fowling-piece");
        people.player["poached"] = std::to_string(numMem(people.player, "poached") + 1);
        long fine = 2400, m = money();
        std::string grounds = k.mem.count("grounds") ? k.mem["grounds"] : std::string("the royal grounds");
        if (m >= fine) {
            people.player["money"] = std::to_string(m - fine);
            say(people.label(k) + ": \"That is " + owner + " deer.\" He takes the venison and your gun, and writes your name in his book: a fine of " + moneyString(fine) +
                " for poaching in " + grounds + ", paid on the spot.", 9.0);
            return;
        }
        people.player["money"] = "0";
        float d;
        std::string town = nearestPlace(tx, ty, d);
        for (auto &r : royals) if (r.name == grounds) town = r.town;
        say(people.label(k) + ": \"That is " + owner + " deer, and you cannot pay for it.\" He takes the venison, your gun and your purse.", 6.0);
        prison(town, "the venison and your gun");
    }
    // ---- the fowling piece: the nearest wild beast in sight, within sixteen tiles
    void shootAt(int i) {
        Animal &a = animals.list[i];
        const AnimalKind &k = animals.kinds[a.kind];
        if (numMem(people.player, "has:shot") <= 0) { say("You have no powder and shot. A smith sells charges of it.", 3.0); return; }
        float d = std::hypot(a.x - (tx + 0.5f), a.y - (ty + 0.5f));
        if (d > 16) { say("It is too far off for a shot.", 2.5); return; }
        long s = numMem(people.player, "has:shot") - 1;
        if (s <= 0) people.player.erase("has:shot"); else people.player["has:shot"] = std::to_string(s);
        facingTarget = std::atan2(-(a.x - (tx + 0.5f)), a.y - (ty + 0.5f));
        if (soundsLoaded) sounds.play("sword", 1.0f);
        minutes += 1;
        std::string what = k.name.substr(k.name.find(' ') + 1);
        float chance = std::max(0.2f, 0.92f - 0.04f * d);
        if (wuni() < chance) {
            float dmg = 6 + wuni() * 9;
            animals.hurt(i, dmg, minutes);
            say(a.dead ? "The gun kicks and roars, and the " + what + " goes down." : "The gun kicks and roars: the " + what + " is hit, and runs.", 3.5);
            if (a.dead && k.id == "deer") people.player["deer-shot"] = std::to_string(numMem(people.player, "deer-shot") + 1);
        } else say("The gun kicks and roars, and the smoke clears on nothing: a miss.", 3.0);
        animals.startle(a.x, a.y, 30);
        if (inCountry()) keeperHears(royalAt(tx, ty));
    }
    void shootNearest() {
        if (numMem(people.player, "has:fowling-piece") <= 0) return;
        int best = -1;
        float bd = 16.5f;
        for (size_t i = 0; i < animals.list.size(); i++) {
            const Animal &a = animals.list[i];
            if (!a.wildOne || a.gone || a.dead) continue;
            float d = std::hypot(a.x - (tx + 0.5f), a.y - (ty + 0.5f));
            if (d < bd) { bd = d; best = (int)i; }
        }
        if (best < 0) { say("There is nothing in sight worth a shot.", 2.5); return; }
        shootAt(best);
    }
    // ---- fishing: with a rod, at the water's edge; an hour or less, and a fish, or not
    void goFishing() {
        bool sea = false, water = false;
        for (int y = ty - 2; y <= ty + 2; y++)
            for (int x = tx - 2; x <= tx + 2; x++) {
                if (!world.inWorld(x, y)) continue;
                int t = world.tile(x, y).terrain;
                if (t == T_WATER || t == T_DEEP) water = true;
                if (t == T_DEEP) sea = true;
            }
        if (!water || tz != 0) { say("You need water to fish in: go to the edge of a river, a lake or the sea.", 3.5); return; }
        double h = std::fmod(minutes, 1440.0) / 60.0;
        float chance = (h >= 5 && h < 8.5) || (h >= 17.5 && h < 21) ? 0.75f : (h >= 21 || h < 5) ? 0.25f : 0.45f;
        float d;
        std::string land = countryOf(nearestPlace(tx, ty, d));
        int wait = 15 + (int)(wuni() * 30);
        minutes += wait;
        if (wuni() >= chance) {
            say(std::string("You sit with the rod for ") + std::to_string(wait) + " minutes. Nothing bites" + (h >= 10 && h < 16 ? ": in the middle of the day they are wiser than you." : "."), 4.0);
            return;
        }
        static const char *river[] = {"a carp", "a perch", "a pike", "a bream", "an eel", "a tench"};
        static const char *salt[] = {"a mackerel", "a sea bream", "a mullet", "a gurnard"};
        bool hills = hilly(tx, ty);
        std::string fish = sea ? salt[(int)(wuni() * 4) % 4] : hills ? "a trout" : river[(int)(wuni() * 6) % 6];
        people.player["has:fish"] = std::to_string(numMem(people.player, "has:fish") + 1);
        people.player["fish-caught"] = std::to_string(numMem(people.player, "fish-caught") + 1);
        say("After " + std::to_string(wait) + " minutes the line jerks, and you land " + fish + ". Fish sell best on Fridays and in Lent; the friars and the nuns are always glad of one.", 5.0);
        (void)land;
    }
    // ---- what a conversation asked for on the roads: robbed, a fight, an escort, a bill of exchange
    void roadRequest(const std::string &rq, int who) {
        std::istringstream ss(rq);
        std::string verb, arg;
        ss >> verb >> arg;
        if (verb == "robbed") {
            long m = money();
            if (m < 12) {
                say("He turns out your pockets, and spits. \"A beggar! Go on, before we change our minds.\" They go back into the trees.", 6.0);
            } else {
                // the toll: a quarter of what you carry, at most ten livres, the same share of
                // every coin; a fight lost still costs you more (Game::defeated)
                long toll = std::max(1L, std::min(m / 4, 2400L)), taken = 0;
                for (auto &c : coins) {
                    long h = coinHeld(c.id);
                    if (h <= 0) continue;
                    long t = std::min(h, (long)std::llround((double)h * toll / m));
                    setCoin(c.id, h - t);
                    taken += t;
                }
                people.player["money"] = std::to_string(m - taken);
                people.player["robbed"] = std::to_string(numMem(people.player, "robbed") + 1);
                bool bill = numMem(people.player, "has:bill-of-exchange") > 0;
                say("He counts " + moneyString(taken) + " out of your purse into his hat, and hands you back the rest. \"The toll, friend. Go with God.\"" +
                    (bill ? " Your bill of exchange he holds up to the light, and throws back at you: a name on a paper is no use to him." : "") +
                    " Then they are gone into the trees.", 8.0);
            }
            bandLeave();
        } else if (verb == "bandfight") {
            bandAttack("");
        } else if (verb == "escort") {
            long price = arg.empty() ? 120 : std::atol(arg.c_str());
            if (people.escortIdx < 0) return;
            if (money() < price) { say("You have not the money for him.", 3.0); return; }
            people.player["money"] = std::to_string(money() - price);
            people.player["escort-until"] = std::to_string((long)(minutes + 1440));
            people.player["escorts"] = std::to_string(numMem(people.player, "escorts") + 1);
            placeEscortBeside();
            escortBack = true;
            if (talking) { people.endTalk(minutes); talking = false; }
            say("You pay " + moneyString(price) + ". The hired sword will walk behind you until this time tomorrow; the gentlemen of the road look for lonelier travellers.", 6.0);
        } else if (verb == "billdraw") {
            const Person &c = people.people[who];
            long m = money(), keep = std::min(m, 240L), amount = m - keep;
            if (amount < 240) { say("\"For so little? Keep it in your purse.\"", 3.0); return; }
            long fee = std::max(1L, amount * 2 / 100);
            for (auto &cn : coins) setCoin(cn.id, 0);
            setCoin(coinOf(c.town), keep);
            people.player["money"] = std::to_string(keep);
            long had = numMem(people.player, "bill:amount");
            people.player["bill:amount"] = std::to_string(had + amount - fee);
            people.player["bill:town"] = c.town;
            people.player["has:bill-of-exchange"] = "1";
            say("He counts your coin, writes a short letter, signs it with a flourish, and sands it: a bill of exchange for " + moneyString(had + amount - fee) +
                ", to be paid to the bearer by any changer in another town. His fee: " + moneyString(fee) + ". You keep " + moneyString(keep) + " for the road.", 8.0);
        } else if (verb == "billcash") {
            const Person &c = people.people[who];
            long amount = numMem(people.player, "bill:amount");
            if (amount <= 0 || numMem(people.player, "has:bill-of-exchange") <= 0) { say("\"What bill? Show me the paper.\"", 3.0); return; }
            std::string from = people.player.count("bill:town") ? people.player["bill:town"] : std::string();
            setCoin(coinOf(c.town), coinHeld(coinOf(c.town)) + amount);
            people.player["money"] = std::to_string(money() + amount);
            people.player.erase("bill:amount"); people.player.erase("bill:town"); people.player.erase("has:bill-of-exchange");
            if (from == c.town) say("\"My own bill? Then here is your money back, and I keep my fee.\" He counts out " + moneyString(amount) + ".", 5.0);
            else say("He reads the bill from " + (from.empty() ? std::string("another town") : from) + ", knows the signature, and counts out " + moneyString(amount) + " in the coin of the place.", 6.0);
        }
    }
    // every moment: is something owed settled, carried off, or are you seen by the watch?
    void watchStep() {
        if (people.caught >= 0) { int j = people.caught; people.caught = -1; if (j == people.keeperIdx) keeperCaught(); else arrest(j); return; }
        if (people.lostBy >= 0 && people.lostBy == people.keeperIdx) {
            people.lostBy = -1;
            people.people[people.keeperIdx].away = true;
            say("The gamekeeper has lost you among the trees. He will know you again.", 5.0);
        }
        if (people.lostBy >= 0) { people.lostBy = -1; say("You have given the watch the slip. They will know you again, for a day or so.", 5.0); }
        if (owed.kind >= 0 && !owed.stolen) {
            const ItemKind &k = items.kinds[owed.kind];
            if (people.debt.paid || people.debt.returned) { clearOwed(); return; }
            if (!k.money && numMem(people.player, "has:" + k.id) <= owed.hadAfter - owed.count) {   // put down again
                if (!items.things.empty() && items.things.back().kind == owed.kind && bldAt(items.things.back().x, items.things.back().y, items.things.back().z) == owed.bld)
                    items.things.back().owner = owed.bld;   // theirs again
                if (owed.witness >= 0 && owed.witness < (int)people.people.size()) {
                    Person &w = people.people[owed.witness];
                    w.bubble = "Good. Keep your hands to yourself."; w.bubbleT = 3.0f;
                }
                clearOwed();
                return;
            }
            bool away = owed.bld ? bldAt(tx, ty, tz) != owed.bld : std::max(std::abs(tx - owed.x), std::abs(ty - owed.y)) > 8;
            if (away) theft();
        }
        // wanted in this town: the watch knows you again
        std::string town = townAt(tx, ty);
        if (town.empty() || people.chasing()) return;
        long w = numMem(people.player, "wanted:" + town);
        if (!people.player.count("wanted:" + town)) return;
        if (minutes - w > 1440) { people.player.erase("wanted:" + town); return; }
        for (size_t i = 0; i < people.people.size(); i++) {
            const Person &p = people.people[i];
            if (p.watch && p.active && !p.lying && p.z == tz && std::max(std::abs(p.x - tx), std::abs(p.y - ty)) <= 5) {
                people.startChase((int)i);
                people.people[i].bubble = "You there! Stop, thief!";
                break;
            }
        }
    }
    void takeThing(int i) {
        Thing &t = items.things[i];
        const ItemKind &k = items.kinds[t.kind];
        std::string what = items.nameOf(t.kind, t.count);
        if (k.chained) { say("It is chained to the desk.", 2.0); return; }
        if (k.fixed) { say("You cannot take that.", 2.0); return; }
        Thing before = t;
        if (k.money) {
            long got = (long)k.money * t.count;
            people.player["money"] = std::to_string(money() + got);
            items.take(i);
            say("You take " + what + ". It holds " + moneyString(got) + ".", 3.5);
            noticeTaking(before, before.count);
            return;
        }
        if (carriedWeight() + k.weight * t.count > kCarry) { say("You cannot carry any more.", 2.5); return; }
        std::string key = "has:" + k.id;
        people.player[key] = std::to_string(std::atoi(people.player[key].c_str()) + t.count);
        items.take(i);
        say("You take " + what + ".", 2.5);
        noticeTaking(before, before.count);
    }
    // can something be put down here? (within reach: your tile or one beside it)
    bool canPut(int X, int Y) {
        if (std::max(std::abs(X - tx), std::abs(Y - ty)) > 1) return false;
        const Tile *t = world.tileAt(X, Y, tz);
        if (!t || world.linkAt(X, Y, tz) || items.at(X, Y, tz).size() >= 4) return false;
        if (!(X == tx && Y == ty) && people.occupied(X, Y, tz)) return false;
        return (world.passable(X, Y, tz) && t->feature == F_NONE) || Items::surface(t->feature) > 0;
    }
    void putThing(int kind, int X, int Y) {
        std::string key = "has:" + items.kinds[kind].id;
        int n = std::atoi(people.player[key].c_str());
        if (n <= 0) return;
        const Tile *t = world.tileAt(X, Y, tz);
        std::string where = t && Items::surface(t->feature) > 0 ? " on " + featureName(t->feature) : X == tx && Y == ty ? " at your feet" : "";
        items.put(world, kind, 1, X, Y, tz);
        if (n == 1) people.player.erase(key); else people.player[key] = std::to_string(n - 1);
        say(where.empty() ? "You put down " + items.kinds[kind].name + "." : "You put " + items.kinds[kind].name + where + ".", 2.5);
    }

    // ---- choosing with the mouse
    mat4 viewProjFor(float aspect) const {
        float yr = yaw * PI_F / 180, pr = pitch * PI_F / 180;
        vec3 e(-std::sin(yr) * std::cos(pr), -std::cos(yr) * std::cos(pr), std::sin(pr));
        mat4 view = lookAt(camTarget + e * 300.0f, camTarget, vec3(0, 0, 1));
        mat4 proj = ortho(-zoom * aspect, zoom * aspect, -zoom, zoom, 1, 700);
        return proj * view;
    }
    // where a point of the world is on the screen, as a fraction of the window
    void screenOf(const mat4 &vp, vec3 p, float &sx, float &sy) const {
        vec4 c = vp * vec4(p.x, p.y, p.z, 1);
        sx = c.x / c.w * 0.5f + 0.5f;
        sy = 0.5f - c.y / c.w * 0.5f;
    }
    // how far apart two points of the screen are, in tiles
    float tilesApart(float ax, float ay, float bx, float by) const { return std::hypot((ax - bx) * viewAspect, ay - by) * 2 * zoom; }
    vec3 thingPoint(const Thing &t) {
        float z = world.floorAt(t.x + 0.5f, t.y + 0.5f, t.z) + t.h + 0.1f;
        const std::string &s = items.kinds[t.kind].shape;
        if (s == "painting") z += 1.1f;
        else if (s == "window") z += 1.3f;   // in the wall, over the altar
        else if (s == "statue" || s == "canopy" || s == "tomb") z += 0.7f;
        else if (s == "bookdesk" || s == "openbook" || s == "lectern") z += 0.8f;   // the book on the desk
        return vec3(t.x + t.ox, t.y + t.oy, z);
    }
    // the thing nearest the mouse (-1: none near it); whether it is within reach
    int thingUnder(float mx, float my, bool &inReach) {
        mat4 vp = viewProjFor(viewAspect);
        int best = -1;
        float bd = 0.7f;
        for (int Y = ty - 5; Y <= ty + 5; Y++)
            for (int X = tx - 5; X <= tx + 5; X++)
                for (int i : items.at(X, Y, tz)) {
                    float sx, sy;
                    screenOf(vp, thingPoint(items.things[i]), sx, sy);
                    float d = tilesApart(sx, sy, mx, my);
                    if (d < bd) { bd = d; best = i; }
                }
        inReach = best >= 0 && std::max(std::abs(items.things[best].x - tx), std::abs(items.things[best].y - ty)) <= 1;
        return best;
    }
    // the tile within reach nearest the mouse
    bool spotUnder(float mx, float my, int &X, int &Y) {
        mat4 vp = viewProjFor(viewAspect);
        float bd = 0.9f;
        bool found = false;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int x = tx + dx, y = ty + dy;
                const Tile *t = world.tileAt(x, y, tz);
                if (!t) continue;
                float sx, sy;
                screenOf(vp, vec3(x + 0.5f, y + 0.5f, world.floorAt(x + 0.5f, y + 0.5f, tz) + Items::surface(t->feature)), sx, sy);
                float d = tilesApart(sx, sy, mx, my);
                if (d < bd) { bd = d; X = x; Y = y; found = true; }
            }
        return found;
    }
    void startPick(int mode, int kind = -1) {
        pick = mode;
        pickKind = kind;
        panel = 0;
        if (mode == 1) say("Get what? Click on it.   (Esc: never mind)", 600);
        else if (mode == 3) say("Look at what? Click on it.   (L again: what is in front of you; Esc: never mind)", 600);
        else if (mode == 4) say("Fight whom? Click on them.   (Esc: never mind)", 600);
        else if (mode == 5) say("Use what? Click on it: a bed, a bench, a well...   (U again: something you carry; Esc: never mind)", 600);
        else say("Put " + items.kinds[kind].name + " where? Click on the place.   (Esc: never mind)", 600);
        if (win) {
            if (!crossCursor) crossCursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
            glfwSetCursor(win, crossCursor);
        }
    }
    void endPick() {
        pick = 0;
        pickKind = -1;
        if (win) glfwSetCursor(win, nullptr);
    }
    void click(float mx, float my) {
        int mode = pick, kind = pickKind;
        endPick();
        say("", 0);
        if (mode == 1) {
            bool reach = false;
            int i = thingUnder(mx, my, reach);
            if (i < 0) say("There is nothing there to take.", 2.0);
            else if (!reach) say("That is out of reach.", 2.0);
            else { takeThing(i); minutes += 0.5; }
        } else if (mode == 5) {
            useAround(mx, my);
        } else if (mode == 4) {
            float dp, da;
            int p = personNear(mx, my, dp), an = animalNear(mx, my, da);
            if (an >= 0 && (p < 0 || da < dp) && animals.list[an].wildOne && !animals.list[an].dead) startFightBeast(an);
            else if (p < 0) say("There is nobody there.", 2.0);
            else startFight(p);
        } else if (mode == 3) {
            std::string seen = lookAtMouse(mx, my);
            if (!panel) say(seen, 6.0);
        } else if (mode == 2) {
            int X, Y;
            if (!spotUnder(mx, my, X, Y)) say("That is out of reach.", 2.0);
            else if (!canPut(X, Y)) say("You cannot put it there.", 2.0);
            else { putThing(kind, X, Y); minutes += 0.5; }
        }
    }
    // a work of art or a chained book: open it (the picture and its page, or the book reader) and
    // return its title; "" for anything else
    std::string openWork(int i) {
        const ItemKind &k = items.kinds[items.things[i].kind];
        if (k.chained && !k.book.empty() && items.books.count(k.book)) {   // a chained book: read it where it lies
            openBook(k.book);
            const Book &b = items.books.at(k.book);
            return understands(b) ? b.titleEn : b.title;
        }
        if (k.fixed && !k.title.empty()) {
            panel = 2; readTitle = k.title; readText = k.look; readImage = k.image; readCredit = k.credit;
            people.player["saw:" + k.id] = "1";   // you have seen it (people's lines and the journal can ask: "player saw:the-may")
            if (!k.afterKey.empty() && numMem(people.player, k.afterKey) > 0) {   // the work mended (items.txt "after")
                if (!k.afterImage.empty()) readImage = k.afterImage;
                if (!k.afterLook.empty()) readText = k.afterLook;
                if (!k.afterCredit.empty()) readCredit = k.afterCredit;
            }
            // a work you can read only with a skill (the Alhambra's walls, in Arabic): without it, its needs text
            if (!k.needs.empty() && numMem(people.player, "skill:" + k.needs) <= 0 && !k.needsText.empty()) readText = k.needsText;
            return k.title;
        }
        return "";
    }
    // ---- looking with the mouse (L, then a click): a thing, a person or a piece of furniture
    // within a few tiles, on your floor; no need to face it or stand at the right distance
    int thingNear(float mx, float my, float &dist) {   // the thing nearest the mouse on the screen
        mat4 vp = viewProjFor(viewAspect);
        int best = -1;
        dist = 0.7f;
        for (int Y = ty - 6; Y <= ty + 6; Y++)
            for (int X = tx - 6; X <= tx + 6; X++)
                for (int i : items.at(X, Y, tz)) {
                    float sx, sy;
                    screenOf(vp, thingPoint(items.things[i]), sx, sy);
                    float d = tilesApart(sx, sy, mx, my);
                    if (d < dist) { dist = d; best = i; }
                }
        return best;
    }
    int personNear(float mx, float my, float &dist) {
        mat4 vp = viewProjFor(viewAspect);
        int best = -1;
        dist = 0.8f;
        for (size_t i = 0; i < people.people.size(); i++) {
            const Person &pp = people.people[i];
            if (pp.onRoad || pp.away || pp.z != tz || std::max(std::abs(pp.x - tx), std::abs(pp.y - ty)) > 6) continue;
            float sx, sy;
            screenOf(vp, vec3(pp.x + 0.5f, pp.y + 0.5f, world.floorAt(pp.x + 0.5f, pp.y + 0.5f, tz) + (pp.lying ? 0.6f : 1.1f)), sx, sy);
            float d = tilesApart(sx, sy, mx, my);
            if (d < dist) { dist = d; best = (int)i; }
        }
        return best;
    }
    int animalNear(float mx, float my, float &dist) {   // an animal under the mouse, within six tiles
        mat4 vp = viewProjFor(viewAspect);
        int best = -1;
        dist = 0.8f;
        if (tz != 0) return -1;
        bool gun = numMem(people.player, "has:fowling-piece") > 0;
        for (size_t i = 0; i < animals.list.size(); i++) {
            const Animal &a = animals.list[i];
            if (a.gone) continue;
            if (std::max(std::abs(a.x - (tx + 0.5f)), std::abs(a.y - (ty + 0.5f))) > (gun && a.wildOne && !a.dead ? 16 : 6)) continue;
            const AnimalKind &k = animals.kinds[a.kind];
            float sx, sy;
            screenOf(vp, vec3(a.x, a.y, world.groundZ(a.x, a.y) + (a.lying ? 0.25f : k.body.ht * 0.7f)), sx, sy);
            float d = tilesApart(sx, sy, mx, my);
            if (d < dist) { dist = d; best = (int)i; }
        }
        return best;
    }
    // U and a click on an animal: the first of its uses you have the means for
    void useAnimal(int i) {
        Animal &a = animals.list[i];
        const AnimalKind &k = animals.kinds[a.kind];
        if (a.wildOne && !a.dead) {   // a wild one, alive: shoot it, or nothing
            if (numMem(people.player, "has:fowling-piece") > 0) { shootAt(i); return; }
            std::string what = k.name.substr(k.name.find(' ') + 1);
            say(k.hostile > 0 ? "Not with your bare hands: fight it (C), or shoot it with a fowling piece." : "The " + what + " will not let you near it. With a fowling piece you might have it.", 3.5);
            return;
        }
        if (std::max(std::abs(a.x - (tx + 0.5f)), std::abs(a.y - (ty + 0.5f))) > 2.2f) { say("Go closer.", 2.0); return; }
        if (a.mine && k.id != "horse") {   // a beast of yours on a halter (the cow): led, or let stand
            a.led = !a.led;
            a.moving = false;
            say(a.led ? "You take up her halter again. She comes after you, at her own pace." : "You let go of the halter. She stands where she is, and starts to graze.", 3.0);
            return;
        }
        if (a.mine) {   // your own horse, or the one you hired: up, or down
            if (riding == i) { dismount(); return; }
            if (riding >= 0) { say("You are on a horse already.", 2.0); return; }
            if (tz != 0) { say("Go down to it first.", 2.0); return; }
            int hx = (int)std::floor(a.x), hy = (int)std::floor(a.y);
            if (!world.passable(hx, hy)) { say("You cannot get up on it there.", 2.0); return; }
            riding = i; animals.ridden = i;
            tx = hx; ty = hy;
            from = to = vec2(tx + 0.5f, ty + 0.5f);
            stepT = 1;
            a.moving = false; a.tethered = false;
            facing = facingTarget = a.facing;
            say("You put your foot in the stirrup and swing up into the saddle.", 3.0);
            return;
        }
        const AnimalKind::Use *u = nullptr, *lacking = nullptr;
        for (auto &c : k.uses) {
            if (c.dead != a.dead) continue;   // skinning is for the dead, scratching for the living
            if (!c.needs.empty() && numMem(people.player, "has:" + c.needs) <= 0) { if (!lacking) lacking = &c; continue; }
            u = &c;
            break;
        }
        if (!u) {
            if (lacking) {
                int nk = items.find(lacking->needs);
                say("To " + lacking->verb + " " + k.name.substr(k.name.find(' ') + 1) + " you need " + (nk >= 0 ? items.kinds[nk].name : lacking->needs) + ".", 3.0);
            } else say("You can find nothing to do with " + k.name + ".", 2.5);
            return;
        }
        facingTarget = std::atan2(-(a.x - (tx + 0.5f)), a.y - (ty + 0.5f));
        if (u->every > 0 && a.lastUse.count(u->verb) && minutes - a.lastUse[u->verb] < u->every) {
            say(u->done.empty() ? "Not again so soon." : u->done, 3.0);
            return;
        }
        // working for the herds and hens of a town (a herdsman or a farmer took you on): theirs are yours to milk
        std::string hand = people.player.count("farmhand") ? people.player.at("farmhand") : "";
        bool allowed = !hand.empty() && (a.owner == "a herdsman of " + hand || a.owner == hand);
        if (chosen() && !a.owner.empty() && a.owner == homeTown()) allowed = true;   // your family's hens
        if (!u->gives.empty() && !a.owner.empty() && !allowed) {   // what you take from somebody's animal
            std::vector<int> seen = watchers(0);
            if (!seen.empty()) {
                Person &w = people.people[seen[0]];
                std::string what = k.name.substr(k.name.find(' ') + 1);
                std::string line = "Here! Leave that " + what + " alone. It is not yours.";
                w.bubble = line; w.bubbleT = 4.5f; w.hold = std::max(w.hold, 3.0f);
                w.facing = std::atan2(-(float)(tx - w.x), (float)(ty - w.y));
                say(people.label(w) + ": \"" + line + "\"", 5.0);
                return;
            }
            people.player["thefts-unseen"] = std::to_string(numMem(people.player, "thefts-unseen") + 1);
        }
        if (!u->gives.empty()) people.player["has:" + u->gives] = std::to_string(numMem(people.player, "has:" + u->gives) + 1);
        a.lastUse[u->verb] = minutes;
        a.moving = false; a.wait = std::max(a.wait, 4.0f);   // it stands for it
        minutes += u->gives.empty() ? 1 : 5;
        say(u->text, 4.0);
    }
    int innNear(int X, int Y, int r) {   // an inn with its door within r tiles, or 0
        for (int y = Y - r; y <= Y + r; y++)
            for (int x = X - r; x <= X + r; x++) {
                const Tile &t = world.tile(x, y);
                if (t.wall == W_DOOR && t.bld && world.buildings[t.bld - 1].kind == "inn") return t.bld;
            }
        return 0;
    }
    void syncRidden() {   // the horse under you goes where you go
        if (riding < 0) return;
        Animal &h = animals.list[riding];
        vec2 pp = playerPos();
        h.x = h.fx = pp.x; h.y = h.fy = pp.y; h.facing = facing; h.moving = stepT < 1; h.phase = walkPhase * 2;
    }
    void dismount() {
        if (riding < 0) return;
        Animal &a = animals.list[riding];
        riding = -1; animals.ridden = -1;
        a.x = a.fx = tx + 0.5f; a.y = a.fy = ty + 0.5f; a.moving = false; a.radius = 0; a.facing = facing;
        if (a.hired) {   // beside an inn: the post horse is handed in
            int inn = innNear(tx, ty, 5);
            if (inn > 0) {
                a.mine = a.hired = false; a.tethered = true;
                a.owner = world.buildings[inn - 1].name.empty() ? "the inn" : world.buildings[inn - 1].name;
                people.player.erase("horse:hired");
                say("You get down and hand the post horse to the ostler of " + a.owner + ", who ties it up by the door.", 4.0);
                return;
            }
        }
        say("You get down. The horse stands and waits for you.", 3.0);
    }
    // THE POST: an innkeeper gives you a packet for the inn of a town not far off, by road; you
    // are paid there, in full if it comes in time (a fast horse, or running all the way), half if
    // late. The time and the pay go by the distance (time passes with real time here)
    // where a packet goes: "the inn at Paris", or the post house itself ("the post house at Ulm")
    std::string postPlace(const std::string &town) const {
        return town.rfind("the ", 0) == 0 ? town : "the inn at " + town;
    }
    void postJob(int keeper) {
        const Person &p = people.people[keeper];
        if (numMem(people.player, "has:post-packet") > 0) {
            say("You have a packet of the post already, for " + postPlace(people.player["post:to"]) + ".", 3.5);
            return;
        }
        int home = -1;
        for (size_t i = 0; i < world.settlements.size(); i++) if (world.settlements[i].name == p.town) home = (int)i;
        if (home < 0) return;
        std::set<std::string> keepers;
        for (auto &q : people.people) if (q.role == "innkeeper") keepers.insert(q.town);
        // by road only, to a town or a post house a short way off (the sea is not on any road)
        auto dist = world.roadDistances(home);
        std::vector<std::pair<float, int>> cand;
        for (size_t j = 0; j < world.settlements.size(); j++)
            if ((int)j != home && keepers.count(world.settlements[j].name) && dist[j] > 80 && dist[j] < 1600)
                cand.push_back({dist[j], (int)j});
        if (cand.empty()) { say("\"Nothing for the post today.\"", 3.0); return; }
        std::sort(cand.begin(), cand.end());
        cand.resize(std::min<size_t>(cand.size(), 6));
        // the towns more often than the post houses
        int total = 0;
        for (auto &c : cand) total += world.settlements[c.second].hamlet ? 1 : 3;
        Rng r(hash32((uint32_t)(minutes / 240) * 977 + (uint32_t)keeper));
        int pickW = (int)(r.next() % (uint32_t)total), pick = 0;
        for (size_t c = 0; c < cand.size(); c++) {
            pickW -= world.settlements[cand[c].second].hamlet ? 1 : 3;
            if (pickW < 0) { pick = (int)c; break; }
        }
        float D = cand[pick].first;
        const Settlement &to = world.settlements[cand[pick].second];
        long pay = 60 + (long)(D * 0.55f);
        // the time allowed is time ON THE ROAD (postClock): on foot a road tile takes some 0.075 game
        // minutes, on horseback half that, so on foot it is too slow beyond some 330 tiles; hunting
        // for a horse or for the inn, in a town, costs nothing
        double allow = 10 + D * 0.045;
        bool far = D > 330;
        bool horse = riding >= 0;
        for (auto &an : animals.list) if (an.mine && !an.hired && animals.kinds[an.kind].id == "horse") horse = true;
        people.player["has:post-packet"] = "1";
        people.player["post:to"] = to.name;
        people.player["post:allow"] = std::to_string(allow);
        people.player["post:road"] = "0";
        people.player.erase("post:by");
        if (far) people.player["post:far"] = "1"; else people.player.erase("post:far");
        people.player["post:pay"] = std::to_string(pay);
        postSeen = minutes;
        std::string place = postPlace(to.name);
        place[0] = (char)std::toupper((unsigned char)place[0]);
        std::string words = !far ? "\"" + place + ". An easy walk, if you do not dawdle on the road."
                          : horse ? "\"" + place + ". Too far for your legs, in time; on your horse you will be there with time to spare."
                                  : "\"" + place + ". That one is too far for your legs, in time: on a horse you will be there with time to spare. I can hire you one.";
        if (far && !horse) people.people[keeper].mem["offered-horse"] = std::to_string((long)minutes);   // "hire", and he does
        // the time allowed, and the walk, said plainly
        auto span = [](double m) {
            if (m < 75) return std::to_string(std::max(1L, (long)std::lround(m))) + " minutes";
            long h2 = std::lround(m / 30);   // in half hours
            if (h2 <= 2) return std::string("an hour");
            if (h2 == 3) return std::string("an hour and a half");
            return std::to_string(h2 / 2) + (h2 % 2 ? " and a half" : "") + " hours";
        };
        say(words + " Take as long as you like in the towns: nobody minds that. It is the road the merchants count.\" (A packet of the post: " +
            moneyString(pay) + " when you hand it to an innkeeper there; late, half. The post allows " + span(allow) +
            " on the road, and the way takes " + span(D * 0.075) + " on foot, " + span(D * 0.03) + " on a horse.)", 9.0);
    }
    // the post's clock: only the time you spend on the road counts against a packet, not the time in a
    // town finding a horse or the inn. Each frame (and after each typed command) the minutes gone by
    // are added to post:road if you carry a packet and stand in no town; a jump of more than half a
    // day (a beat of the plot, the test keys) is not counted
    double postSeen = -1;
    void postClock() {
        double dm = minutes - postSeen;
        postSeen = minutes;
        if (numMem(people.player, "has:post-packet") <= 0 || dm <= 0 || dm > 720) return;
        for (auto &st : world.settlements) if (st.inside(tx, ty)) return;
        auto it = people.player.find("post:road");
        double road = it == people.player.end() ? 0 : std::atof(it->second.c_str());
        people.player["post:road"] = std::to_string(road + dm);
    }
    void postDelivered(int keeper) {
        const Person &p = people.people[keeper];
        if (numMem(people.player, "has:post-packet") <= 0) return;
        std::string to = people.player["post:to"];
        if (p.town != to) { say("(He hands it back.) \"This is for " + postPlace(to) + ", not here.\"", 4.0); return; }
        long pay = numMem(people.player, "post:pay");
        bool late;
        if (people.player.count("post:allow"))   // the time on the road, against the time allowed for it (postClock)
            late = std::atof(people.player["post:road"].c_str()) > std::atof(people.player["post:allow"].c_str());
        else late = minutes > numMem(people.player, "post:by");   // a packet taken before the road clock
        if (late) pay /= 2;
        people.player["money"] = std::to_string(money() + pay);
        people.player.erase("has:post-packet");
        for (const char *k : {"post:to", "post:by", "post:pay", "post:allow", "post:road", "post:far"}) people.player.erase(k);
        people.player["post:delivered"] = std::to_string(numMem(people.player, "post:delivered") + 1);
        say(late ? "\"Late, and the merchants have been asking. Half: " + moneyString(pay) + ".\""
                 : "\"In good time. The post thanks you.\" He pays you " + moneyString(pay) + ".", 4.5);
    }
    // a boat across the water to PLACE (the packet between Dover and Calais): your own horse
    // comes too, for as much again; a hired one is handed in here
    void takePassage(int master, long price, int mins, const std::string &place) {
        int dest = -1;
        for (size_t i = 0; i < world.settlements.size(); i++) if (world.settlements[i].name == place) dest = (int)i;
        if (dest < 0) { say("\"Not today: the wind is wrong.\"", 3.0); return; }
        int own = -1;
        for (size_t i = 0; i < animals.list.size(); i++) {
            const Animal &an = animals.list[i];
            if (an.mine && !an.hired && animals.kinds[an.kind].id == "horse" && std::hypot(an.x - tx, an.y - ty) < 14) own = (int)i;
        }
        long fare = price * (own >= 0 ? 2 : 1);
        if (money() < fare) { say("You have not the money: it is " + moneyString(fare) + (own >= 0 ? " with the horse." : "."), 3.5); return; }
        people.player["money"] = std::to_string(money() - fare);
        squareCoins();   // paid here, in the coin of here: not squared on the far shore, where it is foreign
        Person &mp = people.people[master];
        mp.mem["money"] = std::to_string(std::atol(mp.mem["money"].c_str()) + fare);
        if (riding >= 0 && animals.list[riding].hired) dismount();   // the post horse stays this side
        for (auto &an : animals.list)
            if (an.mine && an.hired) {   // handed in to the inn here
                an.mine = an.hired = false; an.tethered = true; an.owner = "the inn";
                people.player.erase("horse:hired");
            }
        // ashore on the other side: in the yard of the post house there
        const Settlement &s = world.settlements[dest];
        int bx = -1, by = -1;
        float bd = 1e9f;
        for (int Y = s.y0; Y < s.y0 + s.h; Y++)
            for (int X = s.x0; X < s.x0 + s.w; X++) {
                if (!world.passable(X, Y, 0) || world.tile(X, Y).bld) continue;
                float d = std::hypot(X - (s.x0 + s.w * 0.5f), Y - (float)(s.y0 + 2));
                if (d < bd) { bd = d; bx = X; by = Y; }
            }
        if (bx < 0) { say("\"Not today: the wind is wrong.\"", 3.0); return; }
        tx = bx; ty = by; tz = 0;
        from = to = vec2(tx + 0.5f, ty + 0.5f);
        stepT = 1;
        minutes += mins;
        for (auto &pp : people.people) { pp.actIndex = -2; pp.placed = false; pp.path.clear(); }
        animals.placeNear(world, (float)tx, (float)ty, 150);
        if (own >= 0) {
            Animal &h = animals.list[own];
            if (riding == own) syncRidden();
            else { h.x = h.fx = tx + 1.5f; h.y = h.fy = ty + 0.5f; h.moving = false; }
        }
        people.player["crossings"] = std::to_string(numMem(people.player, "crossings") + 1);
        updatePlace(true);
        camTarget = vec3(from.x, from.y, world.floorAt(from.x, from.y, tz));
        // the voyage, seen (data/scenes/passage-FROM-TO.txt, FROM and TO without "the post house at"):
        // the ship at its quay, and the other shore; the clock and the place are kept as they are now
        auto shortName = [](std::string n) {
            const std::string ph = "the post house at ";
            if (n.rfind(ph, 0) == 0) n = n.substr(ph.size());
            for (auto &c : n) c = c == ' ' ? '-' : (char)std::tolower((unsigned char)c);
            return n;
        };
        std::string voyage = "passage-" + shortName(people.people[master].town) + "-" + shortName(s.name);
        if (sequences.count(voyage)) { startSequence(voyage); seqKeepClock = seqMode; }
        if (mins < 24 * 60)   // a short crossing: the Channel packet
            say("You pay " + moneyString(fare) + ". The packet beats across the water" + (own >= 0 ? " with your horse in the hold" : "") +
                ", and puts you ashore at " + s.name + ", a little green.", 6.0);
        else {                // days at sea: the Baltic post ship
            int days = (int)std::lround(mins / 1440.0);
            static const char *nums[] = {"", "one", "two", "three", "four", "five", "six", "seven"};
            std::string d = days <= 7 ? nums[days] : std::to_string(days);
            if (!d.empty()) d[0] = (char)std::toupper((unsigned char)d[0]);
            say("You pay " + moneyString(fare) + ". " + d + (days == 1 ? " day" : " days") + " at sea" + (own >= 0 ? ", with your horse in the hold" : "") +
                ", and the ship puts you ashore at " + s.name + ", stiff and salted.", 6.0);
        }
    }
    // an innkeeper (or an ostler) lets you have a horse: hired to the next inn, or yours to keep
    void horseDeal(int keeper, bool buy, long price) {
        const Person &p = people.people[keeper];
        int b = bldAt(p.workX, p.workY, p.workZ);
        std::string place = b ? (world.buildings[b - 1].name.empty() ? "the inn" : world.buildings[b - 1].name) : "";
        for (auto &an : animals.list)
            if (an.mine && animals.kinds[an.kind].id == "horse") { say(an.hired ? "You have a post horse already." : "You have a horse of your own already.", 3.0); return; }
        animals.placeNear(world, (float)tx, (float)ty, 150);
        int best = -1;
        float bd = 1e9f;
        for (size_t i = 0; i < animals.list.size(); i++) {
            const Animal &an = animals.list[i];
            if (an.mine || animals.kinds[an.kind].id != "horse" || an.owner != place) continue;
            float d = std::abs(an.x - p.x) + std::abs(an.y - p.y);
            if (d < bd) { bd = d; best = (int)i; }
        }
        if (best < 0) { say("There is no horse to be had here today.", 3.0); return; }
        if (money() < price) { say("You have not the money: it is " + moneyString(price) + ".", 3.5); return; }
        people.player["money"] = std::to_string(money() - price);
        Person &kp = people.people[keeper];
        kp.mem["money"] = std::to_string(std::atol(kp.mem["money"].c_str()) + price);
        Animal &h = animals.list[best];
        h.mine = true; h.hired = !buy; h.tethered = false; h.radius = 0; h.owner = "you"; h.moving = false;
        people.player[buy ? "horse:own" : "horse:hired"] = "1";
        people.player["horse:hungry"] = "0";
        say("You pay " + moneyString(price) + (buy ? ". The horse is yours, saddle, bridle and all; it stands by the door."
                                                   : " for a post horse, saddled and waiting by the door, to be handed in at the next inn on your road."), 5.0);
    }
    // each morning at six: your own horse's night. Beside an inn it was stabled and fed (three sous);
    // anywhere else it went hungry, and if it was left alone far from you, somebody may have taken it
    void horseNight() {
        double day = std::floor((minutes - 360) / 1440.0);
        if (horseDay < 0 || day < horseDay) { horseDay = day; return; }
        if (day == horseDay) return;
        horseDay = day;
        for (size_t i = 0; i < animals.list.size(); i++) {
            Animal &h = animals.list[i];
            if (!h.mine || h.hired || animals.kinds[h.kind].id != "horse") continue;
            int X = (int)i == riding ? tx : (int)std::floor(h.x), Y = (int)i == riding ? ty : (int)std::floor(h.y);
            int inn = innNear(X, Y, 6);
            if (inn > 0 && money() >= 36) {
                people.player["money"] = std::to_string(money() - 36);
                people.player["horse:hungry"] = "0";
                say("The ostler of " + (world.buildings[inn - 1].name.empty() ? std::string("the inn") : world.buildings[inn - 1].name) +
                    " has stabled and fed your horse for the night: three sous.", 4.5);
                continue;
            }
            people.player["horse:hungry"] = std::to_string(numMem(people.player, "horse:hungry") + 1);
            Rng r(hash32((uint32_t)day * 31 + 7));
            bool away = (int)i != riding && std::max(std::abs(X - tx), std::abs(Y - ty)) > 40;
            if (away && r.chance(0.25f)) {   // stolen in the night; you find out when you come back
                people.player["horse:stolen-x"] = std::to_string(X);
                people.player["horse:stolen-y"] = std::to_string(Y);
                people.player.erase("horse:own");
                h.mine = false; h.owner = "a horse thief"; h.x = h.fx = h.y = h.fy = -100000;
            }
        }
    }
    // ---- the saved game: one writer for the window and the stand-in. It writes PATH.tmp, checks that
    // every byte reached the disk, keeps the old file as PATH.bak (a second name for it, made before
    // the old one is replaced) and renames the new one over it; on any failure the old file is
    // untouched. The clock is written to the thousandth of a minute (the default six figures round it
    // from 1634 on). Music and sound lines: what the player set, or, when they were not loaded (the
    // stand-in), what the saved game said when it was read.
    bool savedMusicOff = false, savedSoundsOff = false;   // as read from the saved game
    bool writeSave(const std::string &path, bool withTalking) {
        auto fail = [&](const std::string &why) {
            std::fprintf(stderr, "COULD NOT SAVE THE GAME in %s (%s): the old saved game is untouched\n", path.c_str(), why.c_str());
            return false;
        };
        size_t sl = path.rfind('/');
        if (sl != std::string::npos && sl > 0) mkdir(path.substr(0, sl).c_str(), 0755);
        rememberHorse();
        std::string tmp = path + ".tmp";
        {
            std::ofstream out(tmp, std::ios::trunc);
            if (!out) return fail("cannot write " + tmp);
            char tbuf[40]; std::snprintf(tbuf, sizeof tbuf, "%.3f", minutes);
            out << "time " << tbuf << "\n" << "at " << tx << " " << ty << " " << tz << "\n";
            out << "view " << yawT << " " << pitchT << " " << zoomT << "\n";   // the camera's turn, tilt and distance, as you left them
            if (withTalking && people.talk.who >= 0 && !people.talk.over) out << "talking " << people.people[people.talk.who].id << "\n";
            if (musicLoaded ? !music.on() : savedMusicOff) out << "music off\n";
            if (soundsLoaded ? !sounds.on() : savedSoundsOff) out << "sounds off\n";
            people.save(out);
            items.save(out);
            out.flush();
            if (!out.good()) { out.close(); std::remove(tmp.c_str()); return fail("the disk refused the data"); }
            out.close();
            if (out.fail()) { std::remove(tmp.c_str()); return fail("closing failed"); }
        }
        std::string bak = path + ".bak";
        if (access(path.c_str(), F_OK) == 0) {
            std::remove(bak.c_str());
            if (link(path.c_str(), bak.c_str()) != 0) std::fprintf(stderr, "  (no %s kept: %s)\n", bak.c_str(), std::strerror(errno));
        }
        if (std::rename(tmp.c_str(), path.c_str()) != 0) { std::string e = std::strerror(errno); std::remove(tmp.c_str()); return fail(e); }
        return true;
    }
    // Is the game in a state that can be saved without a half-done moment in the file? Not on the
    // choosing screen, in the scenes (a beat's too), mid-talk or a talk's typing, mid-step (position
    // between tiles), in a fight or an attack by brigands, with a purchase unsettled (owed), with a
    // plot beat or ending still to be carried out (jumpTo, endingRequest), waiting for a click
    // (pick), or while a book or a work of art is open (panel 2 or 3, opened by a beat).
    bool saveIsSafe() const {
        return !creating && sceneNow < 0 && !seqMode && !talking && people.talk.who < 0 && stepT >= 1 &&
               fightWith < 0 && fightBeast < 0 && bandState == 0 && owed.kind < 0 && people.jumpTo < 0 && people.cardRequest.empty() && !cardUp && fadeT < 0 &&
               !people.endingRequest && pick == 0 && panel != 2 && panel != 3;
    }
    void rememberHorse() {   // into what is saved
        rememberLed();
        for (auto &h : animals.list) {
            if (!h.mine || animals.kinds[h.kind].id != "horse") continue;
            bool on = &h == (riding >= 0 ? &animals.list[riding] : nullptr);
            people.player["horse:x"] = std::to_string(on ? tx : (int)std::floor(h.x));
            people.player["horse:y"] = std::to_string(on ? ty : (int)std::floor(h.y));
            people.player["horse:seed"] = std::to_string(h.seed);
            people.player["horse:ridden"] = on ? "1" : "0";   // in the saddle when the game was saved
        }
    }
    void restoreHorse() {   // out of what was saved: your horse where you left it
        bool own = numMem(people.player, "horse:own") > 0, hired = numMem(people.player, "horse:hired") > 0;
        if ((!own && !hired) || !people.player.count("horse:x")) return;
        int k = -1;
        for (size_t i = 0; i < animals.kinds.size(); i++) if (animals.kinds[i].id == "horse") k = (int)i;
        if (k < 0) return;
        Animal h;
        h.kind = k;
        h.x = h.fx = h.hx = numMem(people.player, "horse:x") + 0.5f;
        h.y = h.fy = h.hy = numMem(people.player, "horse:y") + 0.5f;
        h.radius = 0; h.mine = true; h.hired = hired && !own; h.owner = "you";
        h.seed = (uint32_t)std::strtoul(people.player["horse:seed"].c_str(), nullptr, 10);
        h.col = animals.kinds[k].colours[h.seed % animals.kinds[k].colours.size()];
        h.herd = (int)animals.list.size();
        animals.list.push_back(h);
        if (numMem(people.player, "horse:ridden") > 0) {   // saved in the saddle: still in it (the stand-in's
            // runs begin on foot beside the horse, so "get down" must not put you up again)
            riding = (int)animals.list.size() - 1; animals.ridden = riding;
            animals.list[riding].moving = false; animals.list[riding].tethered = false;
        }
    }

    // ---- a cow of your own: bought at a cattle market ({buycow PRICE}), led
    // home on a halter (she walks after you, one tile behind, on open ground; not through doorways
    // or up stairs, where she waits), and given to your family when you bring her into the Cottage
    // at La Chapelle. U on her: let go of the halter, or take it up again.
    int cowKind() const {
        for (size_t i = 0; i < animals.kinds.size(); i++) if (animals.kinds[i].id == "cow") return (int)i;
        return -1;
    }
    bool byreOfHome(float &bx, float &by) const {   // the middle of the byre at the Cottage at La Chapelle
        const Settlement *st = settlementNamed("the Cottage at La Chapelle");
        if (!st) return false;
        for (auto &b : world.buildings)
            if (b.name == "the byre" && st->inside(b.x0, b.y0)) { bx = (b.x0 + b.x1) * 0.5f + 0.5f; by = (b.y0 + b.y1) * 0.5f + 0.5f; return true; }
        return false;
    }
    void cowDeal(int seller, long price) {
        int ck = cowKind();
        if (ck < 0) return;
        for (auto &an : animals.list)
            if (an.mine && an.kind == ck) { say("You have a cow on a halter already: one is enough to lead.", 3.0); return; }
        const Person &p = people.people[seller];
        animals.placeNear(world, (float)tx, (float)ty, 150);
        int best = -1;
        float bd = 1e9f;
        for (size_t i = 0; i < animals.list.size(); i++) {
            const Animal &an = animals.list[i];
            if (an.mine || an.kind != ck) continue;
            float d = std::abs(an.x - p.x) + std::abs(an.y - p.y);
            if (d < 40 && d < bd) { bd = d; best = (int)i; }
        }
        if (best < 0) { say("There is no cow to be had here today.", 3.0); return; }
        if (money() < price) { say("You have not the money: it is " + moneyString(price) + ".", 3.5); return; }
        people.player["money"] = std::to_string(money() - price);
        Person &sp = people.people[seller];
        sp.mem["money"] = std::to_string(std::atol(sp.mem["money"].c_str()) + price);
        Animal &c = animals.list[best];
        c.mine = true; c.led = true; c.tethered = false; c.radius = 0; c.owner = "you"; c.moving = false; c.herd = best;
        int X = tx + 1, Y = ty;
        findOpen(X, Y);
        c.x = c.fx = X + 0.5f; c.y = c.fy = Y + 0.5f;
        people.player["cow:seed"] = std::to_string(c.seed);
        people.player["cow:bought"] = std::to_string((long)minutes);
        say("You count out " + moneyString(price) + ", and he puts the end of her halter into your hand. She looks at you, and then at the road. (U on her: let go of the halter, or take it up again.)", 7.0);
    }
    void leadAlong(int ox, int oy) {   // you have stepped off (ox, oy): what you lead steps onto it
        for (auto &a : animals.list) {
            if (!a.mine || !a.led) continue;
            int ob = world.tile(ox, oy).bld;   // not into houses (she waits outside, and below), but through an open hall (the market, the byre)
            if (tz != 0 || (ob && world.buildings[ob - 1].kind != "market") || !world.passable(ox, oy)) continue;
            if (a.moving) { a.x = a.fx; a.y = a.fy; }   // where she was going she is now (the walk only smooths the step)
            if (std::max(std::abs(a.x - (ox + 0.5f)), std::abs(a.y - (oy + 0.5f))) > 6) {   // left behind: she stands until you come back for her
                if (!a.moving && people.player["cow:behind"] != "1") { people.player["cow:behind"] = "1"; say("Your cow cannot keep up, and has stopped. Go back for her.", 3.5); }
                continue;
            }
            people.player.erase("cow:behind");
            a.fx = ox + 0.5f; a.fy = oy + 0.5f; a.moving = true;
            cowHome(a);
        }
    }
    void cowHome(Animal &a) {   // brought into the Cottage at La Chapelle: she is your family's
        const Settlement *st = settlementNamed("the Cottage at La Chapelle");
        if (!st || a.kind != cowKind() || !st->inside((int)std::floor(a.fx), (int)std::floor(a.fy))) return;
        float bx, by;
        a.mine = false; a.led = false; a.owner = homeTown();
        if (byreOfHome(bx, by)) { a.hx = bx; a.hy = by; } else { a.hx = a.fx; a.hy = a.fy; }
        a.radius = 2.5f;
        people.player["cow-home"] = std::to_string((long)minutes);
        for (const char *k : {"led:x", "led:y", "led:on", "cow:behind"}) people.player.erase(k);
        say("You lead her in at the gate. Your mother, when she sees what is on the end of the halter, stops where she is; then she runs.", 7.0);
    }
    void bringLed() {   // a journey by road (the typed TRAVEL): what you lead comes with you
        for (auto &a : animals.list) {
            if (!a.mine || !a.led) continue;
            int X = tx - 1, Y = ty;
            findOpen(X, Y);
            a.x = a.fx = X + 0.5f; a.y = a.fy = Y + 0.5f; a.moving = false;
            cowHome(a);
        }
    }
    void rememberLed() {   // into what is saved
        for (const char *k : {"led:x", "led:y", "led:on"}) people.player.erase(k);
        for (auto &a : animals.list) {
            if (!a.mine || a.kind != cowKind()) continue;
            people.player["led:x"] = std::to_string((int)std::floor(a.x));
            people.player["led:y"] = std::to_string((int)std::floor(a.y));
            people.player["led:on"] = a.led ? "1" : "0";
        }
    }
    void restoreLed() {   // out of what was saved: the cow on her halter, or in your family's byre
        int ck = cowKind();
        if (ck < 0) return;
        Animal c;
        c.kind = ck;
        c.seed = (uint32_t)std::strtoul(people.player["cow:seed"].c_str(), nullptr, 10);
        c.col = animals.kinds[ck].colours[c.seed % animals.kinds[ck].colours.size()];
        c.herd = (int)animals.list.size();
        float bx, by;
        if (numMem(people.player, "cow-home") > 0 && byreOfHome(bx, by)) {
            c.x = c.fx = c.hx = bx; c.y = c.fy = c.hy = by;
            c.radius = 2.5f; c.owner = homeTown();
        } else if (people.player.count("led:x")) {
            c.x = c.fx = c.hx = numMem(people.player, "led:x") + 0.5f;
            c.y = c.fy = c.hy = numMem(people.player, "led:y") + 0.5f;
            c.radius = 0; c.mine = true; c.led = people.player["led:on"] == "1"; c.owner = "you";
        } else return;
        animals.list.push_back(c);
    }
    bool tileNear(float mx, float my, int &X, int &Y) {   // a tile with furniture (or a wall) under the mouse
        mat4 vp = viewProjFor(viewAspect);
        float bd = 0.8f;
        bool found = false;
        for (int y = ty - 6; y <= ty + 6; y++)
            for (int x = tx - 6; x <= tx + 6; x++) {
                const Tile *t = world.tileAt(x, y, tz);
                if (!t || (featureName(t->feature).empty() && t->wall == W_NONE)) continue;
                float sx, sy;
                float h = t->wall != W_NONE ? 1.0f : std::max(0.3f, Items::surface(t->feature));
                screenOf(vp, vec3(x + 0.5f, y + 0.5f, world.floorAt(x + 0.5f, y + 0.5f, tz) + h), sx, sy);
                float d = tilesApart(sx, sy, mx, my);
                if (d < bd) { bd = d; X = x; Y = y; found = true; }
            }
        return found;
    }
    std::string personSeen(const Person &pp) {
        std::string l = people.label(pp);
        return "You see " + (l == pp.name ? pp.name + ", " + pp.title : l) + (pp.dead ? ", dead." : pp.lying ? ", asleep." : ".");
    }
    // what the mouse is over, for the label while you choose: a name, or ""
    std::string lookLabel(float mx, float my) {
        float dt, dp;
        int i = thingNear(mx, my, dt), p = personNear(mx, my, dp);
        float da;
        int an = animalNear(mx, my, da);
        if (an >= 0 && (p < 0 || da < dp) && (i < 0 || da < dt)) return animals.label(an) + (animals.list[an].dead ? " (dead)" : animals.list[an].lying ? " (asleep)" : "");
        if (p >= 0 && (i < 0 || dp < dt)) return people.label(people.people[p]);
        if (i >= 0) return items.nameOf(items.things[i].kind, items.things[i].count);
        int X, Y;
        if (tileNear(mx, my, X, Y)) {
            const Tile *t = world.tileAt(X, Y, tz);
            if (t && featureName(t->feature).size()) return featureName(t->feature);
        }
        return "";
    }
    // the painted signs over shop and inn doors (World::signs): which is nearest the mouse on the screen,
    // and what you see when you look at one
    int signNear(float mx, float my, float &dist) {
        mat4 vp = viewProjFor(viewAspect);
        int best = -1;
        dist = 0.8f;
        if (tz != 0) return -1;
        for (size_t i = 0; i < world.signs.size(); i++) {
            const Sign &sg = world.signs[i];
            if (std::max(std::abs(sg.x - tx), std::abs(sg.y - ty)) > 8) continue;
            float sx, sy;
            screenOf(vp, vec3(sg.x + 0.5f + sg.ox * 0.6f, sg.y + 0.5f + sg.oy * 0.6f, sg.z - 0.3f), sx, sy);
            float d = tilesApart(sx, sy, mx, my);
            if (d < dist) { dist = d; best = (int)i; }
        }
        return best;
    }
    std::string signSeen(const Sign &sg) {
        static const std::map<std::string, std::string> what = {
            {"inn", "a tankard of ale, under a lantern"}, {"ale", "a tankard of ale"}, {"horseshoe", "a horseshoe"},
            {"dividers", "a pair of dividers"}, {"anvil", "an anvil"}, {"pretzel", "a pretzel"}, {"scales", "a pair of scales"},
            {"book", "a book"}, {"goblet", "a goblet"}, {"palette", "a painter's palette"}};
        auto it = what.find(sg.emblem);
        std::string out = "A painted sign hangs over the door: " + (it != what.end() ? it->second : std::string("an emblem of the trade")) + ".";
        if (sg.bld > 0 && sg.bld <= (int)world.buildings.size()) {
            const Building &b = world.buildings[sg.bld - 1];
            if (!b.name.empty()) return out + " It is " + b.name + ".";
            for (const Person &pp : people.people)
                if (pp.home == sg.bld && !pp.dead && !pp.mem.count("family")) return out + " It is the shop of " + people.label(pp) + ".";
        }
        return out;
    }
    std::string lookAtMouse(float mx, float my) {
        float dt, dp;
        int i = thingNear(mx, my, dt), p = personNear(mx, my, dp);
        {
            float da;
            int an = animalNear(mx, my, da);
            if (an >= 0 && (p < 0 || da < dp) && (i < 0 || da < dt)) {
                const AnimalKind &k = animals.kinds[animals.list[an].kind];
                return animals.list[an].dead ? "You see " + k.name + ", dead." : animals.list[an].lying ? "You see " + k.name + ", asleep." : k.look.empty() ? "You see " + k.name + "." : k.look;
            }
        }
        {
            float ds;
            int sg = signNear(mx, my, ds);
            if (sg >= 0 && (p < 0 || ds < dp) && (i < 0 || ds < dt)) return signSeen(world.signs[sg]);
        }
        if (p >= 0 && (i < 0 || dp < dt)) return personSeen(people.people[p]);
        if (i >= 0) {
            const Thing &t = items.things[i];
            const ItemKind &k = items.kinds[t.kind];
            if (k.chained && !k.book.empty() && std::max(std::abs(t.x - tx), std::abs(t.y - ty)) > 3)
                return "That is " + k.name + ", too far away to read. Go closer.";
            std::string w = openWork(i);
            if (!w.empty()) return w;
            const Tile *tl = world.tileAt(t.x, t.y, t.z);
            std::string on = tl && featureName(tl->feature).size() ? " on " + featureName(tl->feature) : "";
            return "You see " + items.nameOf(t.kind, t.count) + on + ". " + k.look;
        }
        int X, Y;
        if (tileNear(mx, my, X, Y)) {
            const Tile *t = world.tileAt(X, Y, tz);
            if (t && featureName(t->feature).size()) return "You see " + featureName(t->feature) + ".";
            if (t && t->wall != W_NONE) return "You see a wall.";
        }
        return "You see nothing special there.";
    }
    // ---- using what is around you (U, then a click): a bed, a bench, a well, an altar, a bell...
    // Sleeping heals: a bed at an inn costs two sous; at night you sleep until seven, by day two
    // hours. Other people's beds are not yours. (U twice: something you carry, as before.)
    void useAround(float mx, float my) {
        float dt;
        int i = thingNear(mx, my, dt);
        {
            float da;
            int an = animalNear(mx, my, da);
            if (an >= 0 && (i < 0 || da < dt)) { useAnimal(an); return; }
        }
        if (i >= 0) {
            const Thing &t = items.things[i];
            const ItemKind &k = items.kinds[t.kind];
            if (std::max(std::abs(t.x - tx), std::abs(t.y - ty)) > 2) { say("Go closer.", 2.0); return; }
            std::string w = k.chained ? openWork(i) : "";
            if (!w.empty()) return;
            if (k.fixed) { say("There is nothing to do with " + items.nameOf(t.kind, t.count) + " but look at it (L).", 3.0); return; }
            if (!k.useVerb.empty() || !k.book.empty()) { say("Take it first (G), then use it.", 3.0); return; }
            say("You can find no use for " + items.nameOf(t.kind, t.count) + ".", 2.5);
            return;
        }
        int X, Y;
        if (!tileNear(mx, my, X, Y)) { say("There is nothing there to use.", 2.0); return; }
        const Tile *t = world.tileAt(X, Y, tz);
        if (t && (t->terrain == T_WATER || t->terrain == T_DEEP) && numMem(people.player, "has:rod") > 0) { goFishing(); return; }   // a cast into the water
        if (!t || t->feature == F_NONE) { say("There is nothing there to use.", 2.0); return; }
        if (std::max(std::abs(X - tx), std::abs(Y - ty)) > 2) { say("Go closer.", 2.0); return; }
        useFeature(X, Y, t->feature);
    }
    // a rest of an hour or more, in the window: the screen goes dark, a moment of dark, then the new hour
    // fades in (a bed does not take you to morning in an instant)
    float fadeT = -1;   // seconds into the fade; -1: none
    double fadeMins = 0; float fadeHeal = 0; std::string fadeText; bool fadeDone = false;
    std::function<void()> fadeAct;   // what is done while the screen is dark, if not applyRest (a stay until word comes)
    // the screen begins to go dark: a few bars of music over it ("cue sleep" in data/music.txt, the last cadence
    // of "Flow, my tears"), the background fading away under them as the screen does
    void fadeBegins() {
        fadeT = 0; fadeDone = false;
        if (musicLoaded && music.playCue("sleep", now, 0.8)) musicBy = -1;
    }
    float fadeAlpha() const {
        if (fadeT < 0) return 0;
        if (fadeT < 0.8f) return fadeT / 0.8f;
        if (fadeT < 1.9f) return 1;
        return std::max(0.0f, 1 - (fadeT - 1.9f) / 1.0f);
    }
    // ---- THE CLOCK PRIMITIVE. Moves the clock on by up to MAXMIN game minutes in coarse steps and runs what
    // the clock drives at each one: the events (news, beats, timers), the post clock, the far-away travel,
    // horseNight, takeRequests. People are not walked: at the end they are put where the hour puts them, as
    // after a beat's jump (actIndex = -2, placed = false). It stops early, if STOPON has the bit:
    //   CLK_NEWS  a news line was shown to you (clockNews has it);
    //   CLK_BEAT  a beat began (a jump, a card, a scene, the ending); a beat that brings a scene or a card
    //             always stops it, whether or not STOPON asks.
    // It returns CLK_LIMIT, CLK_NEWS or CLK_BEAT. The limit counts the steps taken, not a beat's jump.
    // The step is 10 game minutes: no timer of the data is shorter than an hour (the shortest "recent" is
    // 0.05 day = 72 minutes, "not recent:0.03" 43), and the stand-in's fine WAIT runs the events only every
    // 0.25 minutes; a step also ends at the hour of the next "on" event, so that it happens at its hour.
    enum { CLK_LIMIT = 0, CLK_NEWS = 1, CLK_BEAT = 2 };
    std::string clockNews;
    int advanceClock(double maxMin, int stopOn) {
        const double STEP = 10;
        int why = CLK_LIMIT;
        double left = maxMin, began = minutes;
        clockNews.clear();
        people.wakeRequest = false;   // the drums heard before the wait began do not end it
        while (left > 1e-9) {
            double st = std::min(STEP, left);
            double nx = people.nextEventTime(minutes);
            left -= st;
            if (nx > minutes && nx - minutes < st) { minutes = nx; } else minutes += st;
            postClock();
            people.advanceStep(world, minutes, tx, ty);
            horseNight();
            bool news = !people.notice.empty();
            if (news) { clockNews = people.notice; people.notice.clear(); say(clockNews, 8.0); }
            bool beat = people.jumpTo >= 0 || !people.cardRequest.empty() || people.endingRequest;
            bool woken = people.wakeRequest || people.carryX >= 0;   // the drums (events.txt "wake"), or the march
            people.wakeRequest = false;
            takeRequests();
            if (sceneNow >= 0 || cardUp || (beat && (stopOn & CLK_BEAT))) { why = CLK_BEAT; break; }
            if (news && (stopOn & CLK_NEWS)) { why = CLK_NEWS; break; }
            if (woken) { why = CLK_NEWS; break; }   // whatever the wait: you are woken
        }
        if (minutes > began) for (auto &p : people.people) { p.actIndex = -2; p.placed = false; p.path.clear(); }
        return why;
    }
    // ---- a bed kept until word comes (the innkeeper's "stay"): the first night is paid now, two sous at
    // each night after it (a night is 24 hours from your lying down); you wake at the first news told to you,
    // at the start of a beat, after fourteen days, or when the money for another night is gone. In the
    // window the clock moves while the screen is dark (restFor's fade).
    void stayTillWord(int keeper, const std::string &innName) {
        const long night = 24;
        if (money() < night) { say("A bed is two sous, and you have not got them.", 3.5); return; }
        people.player["money"] = std::to_string(money() - night);
        if (keeper >= 0) { Person &p = people.people[keeper]; p.mem["money"] = std::to_string(std::atol(p.mem["money"].c_str()) + night); }
        auto act = [this, keeper]() {
            double start = minutes, deadline = start + 14 * 1440, nextPay = start + 1440;
            std::string msg;
            while (true) {
                int r = advanceClock(std::min(deadline, nextPay) - minutes, CLK_NEWS | CLK_BEAT);
                if (r == CLK_BEAT) break;   // a beat simply begins
                if (r == CLK_NEWS) { msg = "You wake to a knock at the door: word has come. " + clockNews; break; }
                if (minutes >= deadline - 1e-6) { msg = "Fourteen days, and no word. The innkeeper has begun to look at you."; break; }
                if (money() < night) { msg = "Your money is gone, and so is the innkeeper's patience: no word has come."; break; }
                people.player["money"] = std::to_string(money() - night);
                if (keeper >= 0) { Person &p = people.people[keeper]; p.mem["money"] = std::to_string(std::atol(p.mem["money"].c_str()) + night); }
                nextPay += 1440;
            }
            setHp((float)maxHp());
            healFrom = minutes;
            fightWith = -1;
            if (!msg.empty()) say(msg, 9.0);
        };
        (void)innName;
        if (win && fadeT < 0) { fadeBegins(); fadeAct = act; return; }
        act();
    }
    void restFor(double mins, float heal, const std::string &text) {
        if (win && mins >= 60 && fadeT < 0) { fadeBegins(); fadeMins = mins; fadeHeal = heal; fadeText = text; return; }
        applyRest(mins, heal, text);
    }
    void applyRest(double mins, float heal, const std::string &text) {
        if (mins >= 60) advanceClock(mins, 0);   // the events of the night run (a beat's scene stops it); not walked
        else minutes += mins;
        setHp(hpNow() + heal);
        healFrom = minutes;
        fightWith = -1;
        if (mins >= 60) for (auto &p : people.people) { p.actIndex = -2; p.placed = false; p.path.clear(); }
        // a letter that came in the night is told after the waking, not hidden by it
        if (mins >= 60 && !clockNews.empty()) say(text + " " + clockNews, 9.0);
        else say(text, 5.0);
    }
    void useFeature(int X, int Y, int f) {
        switch (f) {
            case F_BED: sleepIn(X, Y); return;
            case F_BENCH: case F_TABLE:
                restFor(60, 6, f == F_BENCH ? "You sit on the bench and rest for an hour." : "You sit down at the table and rest for an hour.");
                return;
            case F_WELL: case F_FOUNTAIN:
                restFor(2, 1, f == F_WELL ? "You draw a bucket and drink. The water is cold, and tastes of iron." : "You drink from the fountain, out of your hands.");
                return;
            case F_ALTAR: restFor(15, 0, "You kneel at the altar and pray for a while."); return;
            case F_HEARTH: case F_OVEN: case F_FORGE: restFor(10, 0, "You warm your hands."); return;
            case F_BELL: say("You pull the rope, and the bell speaks over the whole town. A few people look up, crossly.", 4.0); return;
            case F_CANNON: say("A gun on its carriage, loaded for the gunners. You have no orders to fire it, and no wish to explain yourself afterwards.", 4.0); return;
            case F_TENT: say("Somebody's tent. From inside, somebody snores.", 3.0); return;
            case F_GABION: say("Wicker baskets filled with earth. They would stop a cannonball, mostly.", 3.0); return;
            case F_STALL: case F_COUNTER: say("Talk to whoever keeps it (T), to buy or to sell.", 3.0); return;
            case F_CHEST: say("It is shut, and it is not yours.", 2.5); return;
            case F_BARREL: say("You lift the lid: herrings, in brine. You put the lid back.", 3.0); return;
            case F_LADDER: case F_HATCH: case F_STAIRS: case F_SHAFT: {
                if (riding >= 0) { say("Not on horseback: get down first.", 2.5); return; }
                const Link *lk = world.linkAt(X, Y, tz);
                if (!lk) { say("It leads nowhere you can follow.", 2.5); return; }
                if (std::max(std::abs(X - tx), std::abs(Y - ty)) > 1) { say("Go closer.", 2.0); return; }
                followLink(*lk, X - tx, Y - ty);
                return;
            }
            default: say("You can find no use for " + featureName(f) + ".", 2.5); return;
        }
    }
    void sleepIn(int X, int Y) {
        if (people.occupied(X, Y, tz)) { say("Somebody is asleep in it already.", 2.5); return; }
        int b = bldAt(X, Y, tz);
        const Building *B = b ? &world.buildings[b - 1] : nullptr;
        if (B && B->home && chosen()) {   // your own bed at home, for nothing
            double h = std::fmod(minutes, 1440.0) / 60.0;
            if (h >= 18 || h < 5) {
                double day = std::floor(minutes / 1440.0), until = day * 1440 + 5 * 60 + 30;
                if (until <= minutes) until += 1440;
                restFor(until - minutes, (float)maxHp(), "You sleep in your own bed under the eaves, and wake at cockcrow, whole and rested.");
            } else {
                restFor(120, 0.5f * (maxHp() - hpNow()), "You lie down on your own bed for a couple of hours. The house smells of smoke and bread, as it always did.");
            }
            return;
        }
        if (!B || B->kind != "inn") { say("It is somebody's bed. You would rather not be found in it.", 3.0); return; }
        int keeper = -1;
        for (size_t k = 0; k < people.people.size(); k++)
            if (people.people[k].hasWork && bldAt(people.people[k].workX, people.people[k].workY, people.people[k].workZ) == b) { keeper = (int)k; break; }
        sleepPaid(B->name, keeper);
    }
    // a night (or a nap) at an inn, for two sous: from a bed there, or the innkeeper's word
    void sleepPaid(const std::string &innName, int keeper) {
        long cost = 24;   // two sous a night
        // a night on someone's account (player memory account:KEEPER = who pays: Father Mersenne
        // for the courier's first night at the Pomme de Pin): once, at night
        std::string payer;
        double h = std::fmod(minutes, 1440.0) / 60.0;
        bool night = h >= 18 || h < 5;
        if (keeper >= 0 && night) {
            auto it = people.player.find("account:" + people.people[keeper].id);
            if (it != people.player.end() && !it->second.empty()) {
                payer = it->second; cost = 0;
                for (auto &c : payer) if (c == '_') c = ' ';
            }
        }
        if (money() < cost) { say("A bed is two sous, and you have not got them.", 3.5); return; }
        people.player["money"] = std::to_string(money() - cost);
        if (keeper >= 0) { Person &p = people.people[keeper]; p.mem["money"] = std::to_string(std::atol(p.mem["money"].c_str()) + cost); }
        if (!payer.empty()) { people.player.erase("account:" + people.people[keeper].id); people.player["slept-on-account"] = "1"; }
        struct { std::string name; } Bn{innName};
        const auto *B = &Bn;
        std::string pay = payer.empty() ? "You pay two sous and sleep" : "You sleep, on " + payer + "'s account,";
        if (night) {
            double day = std::floor(minutes / 1440.0), until = day * 1440 + 7 * 60;
            if (until <= minutes) until += 1440;
            restFor(until - minutes, (float)maxHp(), pay + " in a bed of " + B->name + " until morning, and wake whole and rested.");
        } else {
            restFor(120, 0.5f * (maxHp() - hpNow()), pay + " for two hours in a bed of " + B->name + ".");
        }
    }
    std::string lookAhead() {   // what you see in front of you (or at your feet); opens a work of art
        int fx, fy;
        frontTile(fx, fy);
        auto names = [&](const std::vector<int> &v) {
            std::string s;
            for (size_t j = 0; j < v.size(); j++) {
                const Thing &t = items.things[v[j]];
                if (j) s += j + 1 == v.size() ? " and " : ", ";
                s += items.nameOf(t.kind, t.count);
            }
            return s;
        };
        std::vector<int> v = items.at(fx, fy, tz);
        for (int i : v) {
            std::string w = openWork(i);
            if (!w.empty()) return w;
        }
        const Tile *t = world.tileAt(fx, fy, tz);
        std::string on = t && featureName(t->feature).size() ? " on " + featureName(t->feature) : "";
        if (v.size() == 1) {
            const ItemKind &k = items.kinds[items.things[v[0]].kind];
            return "You see " + names(v) + on + ". " + k.look;
        }
        if (!v.empty()) return "You see " + names(v) + on + ".";
        for (auto &pp : people.people)
            if (pp.x == fx && pp.y == fy && pp.z == tz && !pp.onRoad) return personSeen(pp);
        if (t && featureName(t->feature).size()) return "You see " + featureName(t->feature) + ".";
        std::vector<int> feet = items.at(tx, ty, tz);
        if (!feet.empty()) return "At your feet: " + names(feet) + ".";
        if (t && (t->wall == W_WALL || t->wall == W_WINDOW || t->wall == W_CITYWALL)) return "You see a wall.";
        return "You see nothing special.";
    }

    vec2 playerPos() const {
        float t = smoothstep(0, 1, stepT);
        return from + (to - from) * t;
    }

    bool findOpen(int &X, int &Y) {
        for (int r = 0; r < 40; r++)
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    if (world.passable(X + dx, Y + dy) && world.tile(X + dx, Y + dy).wall == W_NONE &&
                        world.tile(X + dx, Y + dy).bld == 0) {
                        X += dx; Y += dy;
                        return true;
                    }
                }
        return false;
    }

    void placePlayer(int X, int Y) {
        findOpen(X, Y);
        tx = X; ty = Y; tz = 0;
        from = to = vec2(X + 0.5f, Y + 0.5f);
        stepT = 1;
        vec2 p = playerPos();
        camTarget = vec3(p.x, p.y, world.groundZ(p.x, p.y));
        updatePlace(true);
    }

    // put the player on floor z at (or near) X,Y
    bool placeOnFloor(int X, int Y, int z) {
        for (int r = 0; r < 60; r++)
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    if (!world.passable(X + dx, Y + dy, z)) continue;
                    tx = X + dx; ty = Y + dy; tz = z;
                    from = to = vec2(tx + 0.5f, ty + 0.5f);
                    stepT = 1;
                    camTarget = vec3(from.x, from.y, world.floorAt(from.x, from.y, tz));
                    updatePlace(true);
                    return true;
                }
        return false;
    }

    // ---- who you are: chosen at the start of a new game, kept in what you know
    // (me:name, me:sex, me:skin, me:hair, me:hairlong, me:clothes). A game begun before there was a
    // choice keeps the old look.
    bool creating = false;
    bool savedGameRead = false;   // set when a saved game was continued
    int createRow = 0;
    static constexpr int kSkins = 6, kHairs = 7, kClothes = 5;
    static const char *skinName(int i) { static const char *n[] = {"fair", "light", "olive", "brown", "dark brown", "dark"}; return n[i]; }
    static vec3 skinCol(int i) {
        static const vec3 c[] = {{0.96f, 0.82f, 0.70f}, {0.90f, 0.74f, 0.60f}, {0.80f, 0.63f, 0.47f}, {0.66f, 0.48f, 0.34f}, {0.50f, 0.34f, 0.22f}, {0.36f, 0.24f, 0.16f}};
        return c[i];
    }
    static const char *hairName(int i) { static const char *n[] = {"black", "dark brown", "brown", "chestnut", "red", "fair", "flaxen"}; return n[i]; }
    static vec3 hairCol(int i) {
        static const vec3 c[] = {{0.07f, 0.06f, 0.05f}, {0.19f, 0.12f, 0.07f}, {0.32f, 0.21f, 0.12f}, {0.45f, 0.24f, 0.12f}, {0.64f, 0.27f, 0.10f}, {0.80f, 0.66f, 0.38f}, {0.91f, 0.84f, 0.60f}};
        return c[i];
    }
    static const char *clothesName(int i) { static const char *n[] = {"undyed grey", "brown", "russet", "faded blue", "faded green"}; return n[i]; }
    static vec3 clothesCol(int i) {
        static const vec3 c[] = {{0.55f, 0.52f, 0.47f}, {0.40f, 0.30f, 0.20f}, {0.52f, 0.25f, 0.17f}, {0.33f, 0.39f, 0.50f}, {0.35f, 0.41f, 0.28f}};
        return c[i];
    }
    int meNum(const char *k, int def, int n) const {
        auto it = people.player.find(k);
        int v = it == people.player.end() ? def : std::atoi(it->second.c_str());
        return std::max(0, std::min(n - 1, v));
    }
    bool chosen() const { return people.player.count("me:age") > 0; }   // set when the choosing screen is finished
    std::string wearing() const {   // what you have on (the inventory): the coat is worn, not carried
        if (!chosen()) return "";
        bool courier = people.player.count("courier-coat") > 0;
        return std::string("your mother's yellow hat, and ") + (courier ? "Father Mersenne's blue courier's coat" : "your homespun farm clothes");
    }
    FigureLook playerLook() const {
        FigureLook me;   // the old look, for a game begun before you could choose
        if (!chosen() && !creating) return me;
        bool woman = people.player.count("me:sex") && people.player.at("me:sex") == "f";
        me.skin = skinCol(meNum("me:skin", 1, kSkins));
        me.hair = hairCol(meNum("me:hair", 2, kHairs));
        me.longHair = meNum("me:hairlong", woman ? 1 : 0, 2) == 1;
        me.fringe = true;
        bool courier = people.player.count("courier-coat") > 0;   // Mersenne's coat, later
        vec3 c = courier ? vec3(0.14f, 0.18f, 0.34f) : clothesCol(meNum("me:clothes", 0, kClothes));
        me.coat = c;
        me.legs = courier ? vec3(0.12f, 0.12f, 0.15f) : c * 0.7f;
        me.dress = woman;
        me.woman = woman;
        me.apronOn = woman && !courier;   // a farm girl's apron
        me.apron = vec3(0.86f, 0.83f, 0.74f);
        me.hatKind = 1;                   // your mother's yellow hat: nobody else wears yellow
        me.hat = vec3(0.95f, 0.78f, 0.14f);
        return me;
    }
    void beginCreating() {
        creating = true;
        createRow = 0;
        if (!people.player.count("me:sex")) people.player["me:sex"] = "m";
        zoomT = 1.9f;
        pitchT = 20;   // low, to see your face and clothes
        facing = facingTarget = 3.14159265f - yaw * 3.14159265f / 180.0f;   // turned to face you
        vec2 p = playerPos();
        camTarget = vec3(p.x, p.y, playerZ(p) + 0.6f);
    }
    void changeChoice(int d) {
        auto bump = [&](const char *k, int def, int n) { people.player[k] = std::to_string(((meNum(k, def, n) + d) % n + n) % n); };
        bool woman = people.player["me:sex"] == "f";
        switch (createRow) {
            case 1:
                people.player["me:sex"] = woman ? "m" : "f";
                people.player["me:hairlong"] = woman ? "0" : "1";   // a new choice of hair to go with it (it can be changed)
                break;
            case 2: bump("me:skin", 1, kSkins); break;
            case 3: bump("me:hair", 2, kHairs); break;
            case 4: bump("me:hairlong", woman ? 1 : 0, 2); break;
            case 5: bump("me:clothes", 0, kClothes); break;
            case 6: people.player["begin-at-gates"] = people.player["begin-at-gates"] == "1" ? "0" : "1"; break;
            case 7: people.player["journal-detailed"] = journalDetailed() ? "0" : "1"; break;
        }
    }
    void finishCreating() {
        std::string &n = people.player["me:name"];
        while (!n.empty() && n.back() == ' ') n.pop_back();
        if (n.empty()) n = people.player["me:sex"] == "f" ? "Jeanne" : "Jean";
        for (const char *k : {"me:skin", "me:hair", "me:clothes"}) if (!people.player.count(k)) people.player[k] = k == std::string("me:skin") ? "1" : k == std::string("me:hair") ? "2" : "0";
        if (!people.player.count("me:hairlong")) people.player["me:hairlong"] = people.player["me:sex"] == "f" ? "1" : "0";
        people.player["me:age"] = "17";
        people.player["money"] = "36";   // the three sous your father gave you (data/scenes.txt, scene 8)
        for (auto &p : people.people) if (p.mem.count("family")) people.player["knows:" + p.id] = "1";   // your father and mother
        bool skip = people.player["begin-at-gates"] == "1";
        people.player.erase("begin-at-gates");
        creating = false;
        zoomT = 20;
        pitchT = 52;
        say("You are " + n + ", seventeen years old, of the farm at La Chapelle, near Paris. Your mother gave you a yellow hat.", 7.0);
        if (skip) { beginAtTheGates(); return; }
        if (!showCard("prologue", -1, "opening", !testMode)) startScenes();   // the card, then the still scenes (data/scenes.txt), and you arrive at Paris
    }
    // "Begin at the gates of Paris": no scenes and no errands. You are Mersenne's
    // courier already, in the coat, at the Porte Saint-Denis, with his letter for Galileo and the money
    // for the road and for two copies of the Dialogue; the journal says where to go. Errands of the
    // prologue that are not Mersenne's own (the beadle's parchment, Margot's salt, the gentleman in
    // grey, the post) stay open in Paris.
    void beginAtTheGates() {
        for (const char *k : {"courier-met", "courier-tested", "courier-ruled", "courier-hired", "delivered-diodati", "courier-coat",
                              "courier-stationer", "stationer-done", "supper-on-account", "slept-on-account", "day2-home",
                              "saw-coat-mother", "courier-dutch", "dutch-done", "courier-lendit", "lendit-done", "courier-packet",
                              "posted-packet", "packet-done", "courier-letters", "posted-letters", "letters-done", "courier-thief",
                              "got-peiresc-letter", "thief-done", "courier-reading", "reading-done", "courier-censor", "censor-struck",
                              "censor-done", "heard-dialogue",
                              "heard-mersenne-wants", "skipped-prologue"})
            people.player[k] = "1";
        people.player["courier-florence"] = std::to_string((long)minutes);
        for (auto &p : people.people) if (p.id == "paris:mersenne") people.player["knows:" + p.id] = "1";
        people.player["has:mersenne-to-galileo"] = "1";
        people.player["money"] = std::to_string(36 + 3120);   // your father's three sous; Mersenne's ten livres for the road and three for the books
        squareCoins();
        sceneKeepMinutes = minutes;
        sceneKeepRoofs = roofsOff;
        endScenes(true);
    }
    // the detailed journal: marks for the main story, newest first. Chosen
    // on the choosing screen; Tab switches it while the journal is open. Kept in your memory.
    bool journalDetailed() const {
        auto it = people.player.find("journal-detailed");
        return it != people.player.end() && it->second == "1";
    }
    std::vector<std::pair<std::string, std::string>> createRows() const {
        bool woman = people.player.count("me:sex") && people.player.at("me:sex") == "f";
        auto it = people.player.find("me:name");
        return {{"Your name", it == people.player.end() ? std::string() : it->second},
                {"You are", woman ? "a young woman" : "a young man"},
                {"Your skin", skinName(meNum("me:skin", 1, kSkins))},
                {"Your hair", hairName(meNum("me:hair", 2, kHairs))},
                {"Worn", meNum("me:hairlong", woman ? 1 : 0, 2) ? "long" : "short"},
                {"Your clothes", std::string("homespun, ") + clothesName(meNum("me:clothes", 0, kClothes))},
                {"Begin", people.player.count("begin-at-gates") && people.player.at("begin-at-gates") == "1" ? "at the gates of Paris" : "at home, on the farm"},
                {"Your journal", journalDetailed() ? "detailed, newest first" : "plain"}};
    }

    // ---- the still scenes (data/scenes.txt): after you choose who you are, views
    // of the world held still, each with a few lines. The clock waits, the light is the scene's own
    // hour, and only the scene's figures are drawn (not the people of the town). Click, Space or
    // Enter: the next; Esc: all of them skipped. At the end you stand outside the Porte Saint-Denis.
    struct SceneFig {
        std::string who;             // you, cow, coins, or a person's id
        vec2 at{0, 0};
        std::string face = "cam";    // N, NE, ... NW, cam (towards the camera), or to:WHO
        float facing = 0;
        std::string age = "youth";   // you: child or youth (a baby is in its mother's arms: "carries baby")
        int carry = 0;               // 1 baby, 2 pails, 3 cap
        bool bareheaded = false;     // "bareheaded": the hat taken off (a cap in the hand did not read as one)
        float kneel = 0;
        bool hat = false, lying = false;
        bool seated = false;         // "seated": on a chair (the cardinals at the Minerva)
        bool robeSet = false;        // "robe #RRGGBB": dressed in a long robe of that colour (a penitent's white)
        vec3 robe{1, 1, 1};
    };
    struct Scene {
        std::string id;                   // "id NAME": names its stored picture, data/art/scenes/NAME.png (else FILE-N)
        std::string town, caption;
        std::vector<std::string> conds;   // "if CONDITIONS" (as in stories.txt): shown only when they hold
        long date = -1;                   // "date YYYY-MM-DD": the day of the scene (a beat's scenes span months)
        double hour = 10;
        vec2 look{0, 0};
        float yaw = 22, pitch = 35, zoom = 5;
        bool roofs = false;
        bool leaveSet = false;            // "leave PLACE": where you stand when a beat's scenes end here
        vec2 leave{0, 0};
        std::vector<SceneFig> figs;
    };
    // ---- the cards (data/cards.txt): a full-screen card at the jumps of the calendar, shown with the
    // clock already moved and before the jump's letter or scenes. In the window it waits for a click
    // or Space; the stand-in prints its lines and goes on.
    struct CardLine { bool large = false, date = false, place = false, dayOnly = false; std::vector<std::string> conds; std::string text; };
    std::map<std::string, std::vector<CardLine>> cards;
    std::vector<std::pair<std::string, bool>> cardLines;   // what the card shows now (true: large type)
    bool cardUp = false;
    std::string cardPage;   // what follows the card: "opening" (the still scenes), "scenes:ID", or a book
    void loadCards(const std::string &fn) {
        std::ifstream in(fn);
        std::string line, cur;
        while (std::getline(in, line)) {
            line = trimS(line);
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            std::string w;
            ss >> w;
            if (w == "card") { ss >> cur; cards[cur]; continue; }
            if (w == "end") { cur.clear(); continue; }
            if (cur.empty()) continue;
            CardLine L;
            std::string rest = trimS(line.substr(w.size()));
            if (w == "date") L.date = true;
            else if (w == "day") { L.date = true; L.dayOnly = true; }   // the date line without "N weeks later" (the march)
            else if (w == "small" || w == "large" || w == "place") { L.large = w == "large"; L.place = w == "place"; }
            else { std::fprintf(stderr, "cards.txt: card %s: what is \"%s\"?\n", cur.c_str(), w.c_str()); continue; }
            if (rest.rfind("if ", 0) == 0) {
                size_t arrow = rest.find("=>");
                if (arrow == std::string::npos) { std::fprintf(stderr, "cards.txt: card %s: no => after the condition\n", cur.c_str()); continue; }
                std::string c = trimS(rest.substr(3, arrow - 3)), t;
                size_t pos;
                while ((pos = c.find(" and ")) != std::string::npos) { L.conds.push_back(trimS(c.substr(0, pos))); c = c.substr(pos + 5); }
                L.conds.push_back(trimS(c));
                rest = trimS(rest.substr(arrow + 2));
            }
            L.text = rest;
            cards[cur].push_back(L);
        }
    }
    // "Four months later." from the days between two times (nothing for less than a day)
    static std::string intervalWords(double before, double after) {
        long d = (long)std::floor(after / 1440.0) - (long)std::floor(before / 1440.0);
        if (d <= 0) return "";
        static const char *words[] = {"", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven",
                                      "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty"};
        auto num = [&](long n) { return n <= 20 ? std::string(words[n]) : std::to_string(n); };
        std::string r;
        long months = (long)std::lround(d / 30.4375);
        if (d == 1) return "The next day.";
        if (d <= 13) r = num(d) + " days later.";
        else if (d <= 55) r = num((long)std::lround(d / 7.0)) + " weeks later.";
        else if (months < 24) r = num(months) + " months later.";
        else r = num((long)std::lround(d / 365.25)) + " years later.";
        r[0] = (char)std::toupper((unsigned char)r[0]);
        return r;
    }
    // Shows card ID; BEFORE is the clock before the jump (-1: since the last card, if there was one).
    // Returns true if the card is up and waiting (page then follows it: afterCard); false if it was only
    // printed (the stand-in), or there is no such card: the caller goes on at once.
    bool showCard(const std::string &id, double before, const std::string &page, bool modal) {
        auto it = cards.find(id);
        if (it == cards.end()) { std::fprintf(stderr, "no card called %s (data/cards.txt)\n", id.c_str()); return false; }
        cardLines.clear();
        std::string place = curTown;
        bool placed = false;
        for (auto &L : it->second) {
            if (!L.conds.empty() && !people.condsHold(L.conds, minutes)) continue;
            if (L.place) { if (!placed) { place = L.text; placed = true; } continue; }
            if (L.date) {
                std::string d = dateString(minutes);
                d = d.substr(0, d.rfind(','));   // no hour
                // only at a jump of the calendar: a card with none (chapter I's, when Mersenne sends you) has the
                // place and the date alone (so that "Four weeks later" does not read as a skip)
                std::string iv = before < 0 || L.dayOnly ? "" : intervalWords(before, minutes);
                cardLines.push_back({(place.empty() ? "" : place + ", ") + d + "." + (iv.empty() ? "" : " " + iv), false});
            } else cardLines.push_back({L.text, L.large});
        }
        people.player["card:last"] = std::to_string((long)minutes);
        if (cardLines.empty()) return false;
        if (!modal) {
            for (auto &l : cardLines) std::fprintf(stderr, "  [card] %s\n", l.first.c_str());
            cardLines.clear();
            return false;
        }
        if (talking) { people.endTalk(minutes); talking = false; }
        cardUp = true;
        cardPage = page;
        return true;
    }
    void afterCard() {   // a click on the card: what the jump brought comes next
        cardUp = false;
        cardLines.clear();
        std::string page = cardPage;
        cardPage.clear();
        openPage(page);
    }
    void openPage(const std::string &page) {
        if (page == "opening") startScenes();
        else if (page.rfind("scenes:", 0) == 0) startSequence(page.substr(7));   // the trial: seen, not read
        else if (!page.empty() && items.books.count(page)) { openBook(page); if (testMode) printBook(); }
    }

    std::vector<Scene> scenes;          // the scenes being shown (the opening's, or a beat's)
    std::vector<Scene> openingScenes;   // data/scenes.txt: after you choose who you are
    std::map<std::string, std::vector<Scene>> sequences;   // data/scenes/ID.txt: played by a beat ("jump DATE TIME scenes:ID")
    bool seqKeepClock = false;   // these scenes (a voyage's) leave the clock as it was when they began
    bool sceneTownQuiet = false; // the post ride's stills: staged in other towns, they change nothing in curTown (no customs, no searchers)
    std::string afterSeqSay;     // said when the scenes of a sequence end
    bool seqMode = false;               // a beat's scenes: afterwards you are where you were
    int seqTx = 0, seqTy = 0, seqTz = 0;
    float seqYaw = 22, seqPitch = 52, seqZoom = 20, seqFacing = 0;
    int sceneNow = -1;
    bool portrait = false;         // drawing your portrait (paintPortrait): only you, close, before a dark ground
    bool playByCommands = false;   // tools/play.sh: a player at the typed stand-in (no test output for the scenes)
    double sceneKeepMinutes = 0;
    bool sceneKeepRoofs = false;
    std::string arriveTown, arrivePos, arriveFace = "S";
    const Settlement *settlementNamed(const std::string &n) const {
        for (auto &s : world.settlements) if (s.name == n) return &s;
        return nullptr;
    }
    // a place in a town: COL,ROW of its map (row 0 at the top), door:NAME (outside a door of that
    // building), outside:NAME (beyond the side of it away from the town's middle), or FEATURE:NAME
    // (the first table, hearth, bed... in it); then +DX,DY in columns and rows. _ for a space.
    bool scenePos(const std::string &town, const std::string &tok, vec2 &out) {
        const Settlement *S = settlementNamed(town);
        if (!S) return false;
        std::string base = tok, off;
        size_t plus = tok.find('+');
        if (plus != std::string::npos) { base = tok.substr(0, plus); off = tok.substr(plus + 1); }
        float dc = 0, dr = 0;
        if (!off.empty()) std::sscanf(off.c_str(), "%f,%f", &dc, &dr);
        size_t colon = base.find(':');
        if (colon == std::string::npos) {
            float c, r;
            if (std::sscanf(base.c_str(), "%f,%f", &c, &r) != 2) return false;
            out = vec2(S->x0 + c + 0.5f, S->y0 + S->h - 1 - r + 0.5f);
            return true;
        }
        std::string what = base.substr(0, colon), name = base.substr(colon + 1);
        for (auto &c : name) if (c == '_') c = ' ';
        const Building *B = nullptr;
        int bid = 0;
        for (size_t i = 0; i < world.buildings.size(); i++)
            if (world.buildings[i].name == name && S->inside(world.buildings[i].x0, world.buildings[i].y0)) { B = &world.buildings[i]; bid = (int)i + 1; break; }
        if (!B) return false;
        bool found = false;
        if (what == "outside") {
            float mx = S->x0 + S->w * 0.5f, my = S->y0 + S->h * 0.5f, cx = (B->x0 + B->x1 + 1) * 0.5f, cy = (B->y0 + B->y1 + 1) * 0.5f;
            float ex = cx - mx, ey = cy - my;
            if (std::fabs(ex) > std::fabs(ey)) out = vec2(ex > 0 ? B->x1 + 2.5f : B->x0 - 1.5f, cy);
            else out = vec2(cx, ey > 0 ? B->y1 + 2.5f : B->y0 - 1.5f);
            found = true;
        } else if (what == "door") {
            static const int d4[4][2] = {{0, -1}, {1, 0}, {-1, 0}, {0, 1}};
            for (int Y = B->y0; Y <= B->y1 && !found; Y++)
                for (int X = B->x0; X <= B->x1 && !found; X++) {
                    const Tile &t = world.tile(X, Y);
                    if (t.bld != bid || t.wall != W_DOOR) continue;
                    for (auto &d : d4) {
                        const Tile &n = world.tile(X + d[0], Y + d[1]);
                        if (n.bld == bid || n.wall != W_NONE || !world.passable(X + d[0], Y + d[1], 0)) continue;
                        out = vec2(X + d[0] + 0.5f, Y + d[1] + 0.5f);
                        found = true;
                        break;
                    }
                }
        } else {
            static const std::map<std::string, int> feats = {{"table", F_TABLE}, {"hearth", F_HEARTH}, {"bed", F_BED}, {"well", F_WELL}, {"bench", F_BENCH}, {"altar", F_ALTAR}};
            auto f = feats.find(what);
            if (f == feats.end()) return false;
            for (int Y = B->y0; Y <= B->y1 && !found; Y++)
                for (int X = B->x0; X <= B->x1 && !found; X++)
                    if (world.tile(X, Y).bld == bid && world.tile(X, Y).feature == f->second) { out = vec2(X + 0.5f, Y + 0.5f); found = true; }
        }
        if (found) out = out + vec2(dc, -dr);
        return found;
    }
    void loadScenes(const std::string &fn) {   // the opening (data/scenes.txt), and the beats' (data/scenes/*.txt)
        loadSceneFile(fn, openingScenes, true);
        scenes = openingScenes;
        std::string dir = fn.substr(0, fn.rfind('/') + 1) + "scenes";
        if (DIR *d = opendir(dir.c_str())) {
            while (dirent *e = readdir(d)) {
                std::string n = e->d_name;
                if (n.size() < 5 || n.compare(n.size() - 4, 4, ".txt") != 0) continue;
                loadSceneFile(dir + "/" + n, sequences[n.substr(0, n.size() - 4)], false);
            }
            closedir(d);
        }
    }
    void loadSceneFile(const std::string &fn, std::vector<Scene> &into, bool opening) {
        std::ifstream in(fn);
        if (!in) return;
        std::string line;
        Scene *cur = nullptr;
        std::string shortName = fn.substr(fn.rfind('/') + 1);
        auto bad = [&](const std::string &why) { std::fprintf(stderr, "%s: %s: %s\n", shortName.c_str(), why.c_str(), line.c_str()); };
        while (std::getline(in, line)) {
            std::string t = trimS(line);
            if (t.empty() || t[0] == '#') continue;
            std::istringstream ss(t);
            std::string w;
            ss >> w;
            if (w == "scene") { into.push_back(Scene()); cur = &into.back(); continue; }
            if (w == "arrive" && opening) { ss >> arriveTown >> arrivePos; std::string f; if (ss >> f && f == "facing") ss >> arriveFace; for (auto &c : arriveTown) if (c == '_') c = ' '; continue; }
            if (!cur) continue;
            if (w == "end") { cur = nullptr; continue; }
            if (w == "id") { ss >> cur->id; continue; }
            if (w == "in") { std::getline(ss, cur->town); cur->town = trimS(cur->town); }
            else if (w == "time") { int h = 10, m = 0; std::string v; ss >> v; std::sscanf(v.c_str(), "%d:%d", &h, &m); cur->hour = h + m / 60.0; }
            else if (w == "caption") { std::getline(ss, cur->caption); cur->caption = trimS(cur->caption); }
            else if (w == "if") {   // CONDITIONS joined by "and"
                std::string rest; std::getline(ss, rest); rest = trimS(rest);
                size_t pos;
                while ((pos = rest.find(" and ")) != std::string::npos) { cur->conds.push_back(trimS(rest.substr(0, pos))); rest = rest.substr(pos + 5); }
                if (!rest.empty()) cur->conds.push_back(rest);
            }
            else if (w == "date") { std::string d; ss >> d; cur->date = People::dateMinutes(d, "00:00"); }
            else if (w == "leave") { std::string pos; ss >> pos; if (scenePos(cur->town, pos, cur->leave)) cur->leaveSet = true; else bad("no such place"); }
            else if (w == "look") {
                std::string pos, k;
                ss >> pos;
                if (!scenePos(cur->town, pos, cur->look)) bad("no such place");
                while (ss >> k) {
                    if (k == "yaw") ss >> cur->yaw; else if (k == "pitch") ss >> cur->pitch; else if (k == "zoom") ss >> cur->zoom;
                    else if (k == "roofs") cur->roofs = true;
                }
            } else {   // a figure: you, cow, coins, or a person's id; its place; then its words
                SceneFig f;
                f.who = w;
                std::string pos, k;
                ss >> pos;
                if (!scenePos(cur->town, pos, f.at)) { bad("no such place"); continue; }
                while (ss >> k) {
                    if (k == "facing") ss >> f.face;
                    else if (k == "child" || k == "youth") f.age = k;
                    else if (k == "kneeling") f.kneel = 1;
                    else if (k == "seated") f.seated = true;
                    else if (k == "hat") f.hat = true;
                    else if (k == "lying") f.lying = true;
                    else if (k == "robe") { std::string c; ss >> c; unsigned v = 0; std::sscanf(c.c_str() + (c[0] == '#'), "%x", &v); f.robe = vec3(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f); f.robeSet = true; }
                    else if (k == "bareheaded") f.bareheaded = true;
                    else if (k == "carries") { std::string c; ss >> c; f.carry = c == "baby" ? 1 : c == "pails" ? 2 : c == "cap" ? 3 : c == "book" ? 4 : 0; }
                }
                if (w.rfind("role:", 0) == 0) {   // somebody of that trade, nobody in particular (the cardinals)
                    bool any = false;
                    for (auto &p : people.people) if (p.role == w.substr(5)) { any = true; break; }
                    if (!any) { bad("nobody of the trade " + w.substr(5)); continue; }
                } else if (w != "you" && w != "cow" && w != "coins" && w != "horse") {
                    std::string id = w;
                    for (auto &c : id) if (c == '_') c = ' ';
                    if (!people.byId(id)) { bad("nobody called " + id); continue; }
                    f.who = id;
                }
                cur->figs.push_back(f);
            }
        }
        // a scene without an "id" line is named by its file and its place in it (opening-3, trial-1633-19)
        {
            std::string stem = opening ? std::string("opening") : shortName.substr(0, shortName.rfind('.'));
            for (size_t i = 0; i < into.size(); i++) if (into[i].id.empty()) into[i].id = stem + "-" + std::to_string(i + 1);
        }
        // facings: a compass point, the camera, or towards another figure
        for (auto &S : into)
            for (auto &f : S.figs) {
                static const std::map<std::string, float> compass = {{"N", 0}, {"NE", 45}, {"E", 90}, {"SE", 135}, {"S", 180}, {"SW", 225}, {"W", 270}, {"NW", 315}};
                auto c = compass.find(f.face);
                if (c != compass.end()) f.facing = -c->second * 3.14159265f / 180.0f;
                else if (f.face.rfind("to:", 0) == 0) {
                    std::string id = f.face.substr(3);
                    for (auto &ch : id) if (ch == '_') ch = ' ';
                    for (auto &o : S.figs)
                        if (o.who == id || (id == "you" && o.who == "you") || (id == "cow" && o.who == "cow")) {
                            vec2 v = o.at - f.at;
                            f.facing = std::atan2(-v.x, v.y);
                        }
                } else f.facing = 3.14159265f - S.yaw * 3.14159265f / 180.0f;   // towards the camera
            }
    }
    void startScenes() {
        scenes = openingScenes;
        seqMode = false;
        if (scenes.empty()) return;
        sceneKeepMinutes = minutes;
        sceneKeepRoofs = roofsOff;
        sceneNow = 0;
        applyScene();
    }
    // a beat's scenes (the trial): those whose conditions hold now, then back to where you were
    void startSequence(const std::string &id) {
        auto it = sequences.find(id);
        if (it == sequences.end()) { std::fprintf(stderr, "no scenes called %s (data/scenes/%s.txt)\n", id.c_str(), id.c_str()); return; }
        std::vector<Scene> list;
        for (auto &S : it->second) if (S.conds.empty() || people.condsHold(S.conds, minutes)) list.push_back(S);
        startSceneList(list);
    }
    // the scenes of a list, shown as a beat's: afterwards you are where you were (the post ride's stills are built in code)
    void startSceneList(const std::vector<Scene> &list) {
        scenes = list;
        if (scenes.empty()) return;
        if (talking) { people.endTalk(minutes); talking = false; }
        seqMode = true;
        seqTx = tx; seqTy = ty; seqTz = tz;
        seqYaw = yawT; seqPitch = pitchT; seqZoom = zoomT; seqFacing = facing;
        sceneKeepMinutes = minutes;
        sceneKeepRoofs = roofsOff;
        sceneNow = 0;
        applyScene();
        if (testMode) std::fprintf(stderr, "  [scene 1 of %zu] %s\n", scenes.size(), scenes[0].caption.c_str());
    }
    void applyScene() {
        const Scene &S = scenes[sceneNow];
        minutes = (S.date >= 0 ? (double)S.date : std::floor(sceneKeepMinutes / 1440.0) * 1440) + S.hour * 60;   // the light of that hour
        roofsOff = S.roofs;
        vec2 at = S.look;
        for (auto &f : S.figs) if (f.who == "you") at = f.at;
        tx = (int)std::floor(at.x); ty = (int)std::floor(at.y); tz = 0;
        from = to = vec2(tx + 0.5f, ty + 0.5f);
        stepT = 1;
        updatePlace(true);
        if (!S.roofs) inside = 0;   // no roof cut away, unless the scene asks for it
        camTarget = vec3(S.look.x, S.look.y, world.floorAt(S.look.x, S.look.y, 0) + 0.6f);
        yaw = yawT = S.yaw; pitch = pitchT = S.pitch; zoom = zoomT = S.zoom;
        if (testMode && !playByCommands) describeScene();
    }
    void describeScene() {   // where the figures of the scene stand (tests: --scene N, or PRESS in a walk)
        const Scene &S = scenes[sceneNow];
            for (auto &f : S.figs) {
                const Tile &t = world.tile((int)std::floor(f.at.x), (int)std::floor(f.at.y));
                std::fprintf(stderr, "  [scene %d: %s at %.1f,%.1f%s%s]\n", sceneNow + 1, f.who.c_str(), f.at.x, f.at.y,
                             t.bld ? (" in " + world.buildings[t.bld - 1].name).c_str() : "", t.wall != W_NONE || !world.passable((int)std::floor(f.at.x), (int)std::floor(f.at.y), 0) ? " (NOT OPEN GROUND)" : "");
            }
    }
    void nextScene() {
        if (sceneNow < 0) return;
        if (++sceneNow >= (int)scenes.size()) endScenes();
        else applyScene();
    }
    void prevScene() {   // the scene before (the first has none)
        if (sceneNow <= 0) return;
        --sceneNow;
        applyScene();
    }
    void endScenes(bool skipped = false) {
        sceneNow = -1;
        minutes = sceneKeepMinutes;
        roofsOff = sceneKeepRoofs;
        if (seqMode) {   // a beat's scenes are over: you are where you were, or where the last of them leaves you,
            seqMode = false;   // and the clock has gone on to the hour of the last of them (skipped or not)
            sceneTownQuiet = false;
            const Scene &L = scenes.back();
            double endM = (L.date >= 0 ? (double)L.date : std::floor(sceneKeepMinutes / 1440.0) * 1440) + L.hour * 60;
            minutes = seqKeepClock ? sceneKeepMinutes : std::max(sceneKeepMinutes, endM);   // a voyage's scenes: the true hour of landing
            seqKeepClock = false;
            if (minutes > sceneKeepMinutes + 60) for (auto &pp : people.people) { pp.actIndex = -2; pp.placed = false; }   // everyone where the hour puts them
            tx = seqTx; ty = seqTy; tz = seqTz;
            for (auto it = scenes.rbegin(); it != scenes.rend(); ++it)
                if (it->leaveSet) {
                    int lx = (int)std::floor(it->leave.x), ly = (int)std::floor(it->leave.y);
                    if (world.passable(lx, ly, 0)) { tx = lx; ty = ly; tz = 0; }
                    break;
                }
            from = to = vec2(tx + 0.5f, ty + 0.5f);
            stepT = 1;
            facing = facingTarget = seqFacing;
            yaw = yawT = seqYaw; pitch = pitchT = seqPitch; zoom = zoomT = seqZoom;
            updatePlace(true);
            vec2 p = playerPos();
            camTarget = vec3(p.x, p.y, playerZ(p));
            scenes = openingScenes;
            if (!afterSeqSay.empty()) { std::string m = afterSeqSay; afterSeqSay.clear(); say(m, 9.0); }   // what the scenes lead to (the post ride's arrival)
            return;
        }
        vec2 at;
        if (!arriveTown.empty() && scenePos(arriveTown, arrivePos, at)) {
            tx = (int)std::floor(at.x); ty = (int)std::floor(at.y); tz = 0;
            from = to = vec2(tx + 0.5f, ty + 0.5f);
            static const std::map<std::string, float> compass = {{"N", 0}, {"E", 90}, {"S", 180}, {"W", 270}};
            auto c = compass.find(arriveFace);
            facing = facingTarget = c == compass.end() ? 0.0f : -c->second * 3.14159265f / 180.0f;
            updatePlace(true);
        }
        vec2 p = playerPos();
        camTarget = vec3(p.x, p.y, playerZ(p));
        yaw = yawT = 22; pitch = pitchT = 52; zoom = zoomT = 20;
        if (skipped) say("The morning of Tuesday the 15th of June, 1632. You are Father Mersenne's courier, in his coat, at the Porte Saint-Denis. In your satchel are his letter for Galileo, at Arcetri by Florence, and thirteen livres for the road and for two copies of Galileo's book. The journal (J) says the rest.", 25.0);
        else say("The morning of Tuesday the 15th of June, 1632. Before you is the Porte Saint-Denis, and Paris. Father Mersenne lives at the Minims, by the Place Royale.", 25.0);
    }
    // a figure of a scene, drawn into m
    void drawSceneFig(MeshBuf &m, const SceneFig &f) {
        float z = world.floorAt(f.at.x, f.at.y, 0);   // the floor of a house, or the ground
        if (f.who == "coins") {   // the week's milk money in piles, on a table
            for (int i = 0; i < 4; i++) {
                float cx = f.at.x - 0.24f + 0.16f * i, cy = f.at.y + 0.05f * (i % 2), h = 0.03f + 0.012f * (i * 7 % 4);
                m.prism(cx, cy, z + 0.56f, z + 0.56f + h, 0.035f, 7, i == 3 ? vec3(0.85f, 0.72f, 0.3f) : vec3(0.72f, 0.7f, 0.66f));
            }
            return;
        }
        if (f.who == "horse") {   // a post horse (the post ride's stills); carry picks the colour
            for (auto &k : animals.kinds)
                if (k.id == "horse") {
                    AnimalLook a = k.body;
                    a.col = k.colours.empty() ? a.col : k.colours[(size_t)f.carry % k.colours.size()];
                    buildAnimal(m, a, vec3(f.at.x, f.at.y, z), f.facing, 0, 0.3f, false);
                }
            return;
        }
        if (f.who == "cow") {
            for (auto &k : animals.kinds)
                if (k.id == "paquerette") {
                    AnimalLook a = k.body;
                    a.col = k.colours.empty() ? a.col : k.colours[0];
                    buildAnimal(m, a, vec3(f.at.x, f.at.y, z), f.facing, 0, f.lying ? 0.0f : 0.3f, f.lying);
                }
            return;
        }
        FigureLook lk;
        if (f.who == "you") {
            lk = playerLook();
            bool woman = people.player.count("me:sex") && people.player.at("me:sex") == "f";
            vec3 c = clothesCol(meNum("me:clothes", 0, kClothes));   // the farm clothes, not yet the courier's coat
            lk.coat = c; lk.legs = c * 0.7f; lk.apronOn = woman;
            if (!f.hat) lk.hatKind = 0;   // the yellow hat comes in the last scene
            if (f.age == "child") { lk.size = 0.62f; lk.headK = 1.3f; lk.apronOn = false; }
        } else {
            const Person *p = nullptr;
            if (f.who.rfind("role:", 0) == 0) { for (auto &q : people.people) if (q.role == f.who.substr(5)) { p = &q; break; } }   // anyone of that trade
            else p = people.byId(f.who);
            if (!p) return;
            lk = p->look;
            if (p->mem.count("family") && chosen()) {
                lk.skin = skinCol(meNum("me:skin", 1, kSkins));
                lk.hair = hairCol(meNum("me:hair", 2, kHairs)) * 0.75f + vec3(0.55f, 0.53f, 0.5f) * 0.25f;
            }
            if (f.carry == 3 || f.bareheaded) lk.hatKind = 0;   // bareheaded (or the cap in his hand)
        }
        if (f.robeSet) { lk.coat = lk.legs = f.robe; lk.robe = true; lk.hatKind = 0; }
        lk.carry = f.carry;
        lk.babySkin = skinCol(meNum("me:skin", 1, kSkins));
        lk.kneel = f.kneel;
        lk.sit = f.seated;
        buildFigure(m, vec3(f.at.x, f.at.y, z), f.facing, 0, lk, false);
    }

    // ---- the post ride as a ticket: an innkeeper or an ostler sells it.
    // rideOffer answers the words that name a town (the fare, the hour of arrival); postRide takes the ride:
    // two stills, the clock moved by the riding time, you set down at the destination's inn; now and then
    // (about one ride in 12, one in 6 where it is dangerous) it stops on the open road and puts you there on foot.
    struct RideInfo { bool ok = false; float roadD = 0, d = 0; long fare = 0; double ridingMin = 0; };
    int settlementIndex(const std::string &name) const {
        for (size_t i = 0; i < world.settlements.size(); i++) if (world.settlements[i].name == name) return (int)i;
        return -1;
    }
    RideInfo rideInfo(int from, int to) {
        RideInfo r;
        auto dist = world.roadDistances(from);
        float d = dist[to];
        if (!(d > 0 && d < 1e8f)) return r;
        const Settlement &st = world.settlements[to], &sf = world.settlements[from];
        r.roadD = d;
        r.d = d + (sf.w + sf.h) * 0.25f + (st.w + st.h) * 0.25f;   // as TRAVEL counts it
        r.fare = std::max(10L, (long)std::lround(r.d * 10.0 / 500.0)) * 12;   // 10 sous for each 500 steps (as MAP counts them), to the nearest sou, at least 10 sous
        r.ridingMin = r.d * 0.075 * 0.5;   // what TRAVEL charges on horseback
        r.ok = true;
        return r;
    }
    static std::string rideWhen(double nowMin, double arriveMin) {   // "this morning", "tomorrow" ...: said in the offer
        long d0 = (long)std::floor(nowMin / 1440.0), d1 = (long)std::floor(arriveMin / 1440.0);
        if (d1 > d0) return d1 - d0 == 1 ? "tomorrow" : "in " + std::to_string(d1 - d0) + " days";
        double h = std::fmod(arriveMin, 1440.0) / 60.0;
        return h < 5 ? "tonight" : h < 11.5 ? "this morning" : h < 13 ? "by noon" : h < 17 ? "this afternoon" : h < 20 ? "by evening" : "tonight";
    }
    static std::string rideWhen2(double arriveMin) {   // ... and in the message of arrival
        double h = std::fmod(arriveMin, 1440.0) / 60.0;
        return h < 5 ? "at night" : h < 11.5 ? "this morning" : h < 13 ? "at noon" : h < 17 ? "this afternoon" : h < 20 ? "towards evening" : "at night";
    }
    bool rideOffer(int who, const std::string &words, std::string &reply) {
        Person &p = people.people[who];
        static const std::set<std::string> keys = {"ride", "riding", "journey", "travel", "trip"};
        auto w = People::nameWords(words);
        bool key = false;
        for (auto &x : w) if (keys.count(x)) key = true;
        auto it = p.mem.find("offered-ride");
        bool recent = it != p.mem.end() && minutes - std::atof(it->second.c_str()) <= 0.05 * 1440;
        if (!key && !recent) return false;
        int dest = -1;
        size_t bestLen = 0;
        for (size_t i = 0; i < world.settlements.size(); i++) {   // a town, a post house or a village of the roads named in the words
            const Settlement &st = world.settlements[i];
            auto full = People::nameWords(st.name);
            std::vector<std::vector<std::string>> forms = {full};
            for (size_t k = full.size(); k-- > 0;)   // "the post house at Brussels": also "Brussels"
                if (full[k] == "at" && k + 1 < full.size()) { forms.push_back(std::vector<std::string>(full.begin() + k + 1, full.end())); break; }
            for (auto &nw : forms) {
                if (nw.empty() || nw.size() <= bestLen || nw.size() > w.size()) continue;
                for (size_t a = 0; a + nw.size() <= w.size(); a++)
                    if (std::equal(nw.begin(), nw.end(), w.begin() + a)) { dest = (int)i; bestLen = nw.size(); break; }
            }
        }
        int from = settlementIndex(p.town);
        if (dest < 0 || from < 0 || dest == from) return false;
        const std::string &name = world.settlements[dest].name;
        RideInfo ri = rideInfo(from, dest);
        for (const char *k : {"offered-bed", "offered-post", "offered-own", "offered-horse", "offered-escort"}) p.mem.erase(k);
        p.mem["offered-ride"] = std::to_string((long)minutes);
        if (!ri.ok) {
            p.mem.erase("offered-ride-at");
            reply = "No post goes to " + name + " from here: there is no road, only the sea.";
            return true;
        }
        p.mem["offered-ride-at"] = std::to_string((long)minutes);
        p.mem["offered-ride-to"] = name;
        reply = "The post to " + name + ": " + moneyString(ri.fare) + ", and you are there " + rideWhen(minutes, minutes + ri.ridingMin) +
                ", with your bag behind the postilion. Will you *ride*?";
        if (!people.contraband(name).empty()) reply += " They turn out the post's bags at the gate of " + name + ", by daylight.";
        return true;
    }
    // the land a place (or a tile near it) is in, for the stills and the dangers: France, Alps are made by postRide
    std::string rideLandOf(const std::string &place) const {
        std::string c = countryOf(place);
        if (place == "London") c = "England";
        if (c == "Italy") return place == "Venice" || place == "Padua" || place.find("Ferrara") != std::string::npos ? "ItalyN" : "ItalyS";
        if ((c == "Empire" || c == "Bohemia") && (place == "Vienna" || place.find("Raab") != std::string::npos || place.find("Znaim") != std::string::npos)) return "Austria";
        if (c == "Ragusa") return "Ottoman";
        return c;
    }
    // where a door of building BID opens: the tile outside it, and the way out (dx, dy)
    bool doorOutside(int bid, int &ox, int &oy, int &dx, int &dy) {
        if (bid <= 0) return false;
        const Building &B = world.buildings[bid - 1];
        static const int d4[4][2] = {{0, -1}, {1, 0}, {-1, 0}, {0, 1}};
        for (int Y = B.y0; Y <= B.y1; Y++)
            for (int X = B.x0; X <= B.x1; X++) {
                const Tile &t = world.tile(X, Y);
                if (t.bld != bid || t.wall != W_DOOR) continue;
                for (auto &d : d4) {
                    const Tile &n = world.tile(X + d[0], Y + d[1]);
                    if (n.bld == bid || n.wall != W_NONE || !world.passable(X + d[0], Y + d[1], 0)) continue;
                    ox = X + d[0]; oy = Y + d[1]; dx = d[0]; dy = d[1];
                    return true;
                }
            }
        return false;
    }
    SceneFig rideFig(const std::string &who, vec2 at, float yaw, int carry = 0, float turn = 0) {
        SceneFig f;
        f.who = who; f.at = at; f.carry = carry; f.hat = true;
        f.facing = 3.14159265f - yaw * 3.14159265f / 180.0f + turn;
        return f;
    }
    // a still of the ride: you, the postilion (an ostler of the post) and two horses about the point AT, along (ax, ay)
    Scene rideStill(const std::string &id, vec2 at, vec2 along, double hour, const std::string &caption) {
        Scene S;
        S.id = id; S.caption = caption; S.hour = hour; S.look = at + along * 1.2f;
        S.yaw = 25; S.pitch = 38; S.zoom = 4.5f;
        vec2 perp(-along.y, along.x);
        S.figs.push_back(rideFig("you", at, S.yaw));
        S.figs.back().hat = false;
        S.figs.push_back(rideFig("role:ostler", at + along * 1.5f + perp * 1.1f, S.yaw));
        S.figs.push_back(rideFig("horse", at + along * 2.4f + perp * 0.3f, S.yaw, 0, 0.7f));
        S.figs.push_back(rideFig("horse", at + along * 2.0f - perp * 1.2f, S.yaw, 3, 0.4f));
        return S;
    }
    void postRide(int k) {
        Person &kp = people.people[k];
        std::string destName = kp.mem.count("offered-ride-to") ? kp.mem["offered-ride-to"] : "";
        kp.mem.erase("offered-ride-at");
        int dest = settlementIndex(destName), from = settlementIndex(kp.town);
        if (dest < 0 || from < 0 || dest == from) return;
        RideInfo ri = rideInfo(from, dest);
        std::vector<World::RoadPt> route;
        if (!ri.ok || !world.roadRoute(from, dest, route) || route.empty()) {   // (a short road, Paris to Saint-Denis, has few points)
            say("No post goes to " + destName + " from here: there is no road, only the sea.", 5.0);
            return;
        }
        for (auto &a : animals.list)
            if (a.mine && a.led) { say("Not on the post, friend: the cow walks, and so do you.", 5.0); return; }
        if (money() < ri.fare) {
            say("That is " + moneyString(ri.fare) + ", and you have " + (money() ? "only " + moneyString(money()) : std::string("no money")) + ".", 5.0);
            return;
        }
        if (!curTown.empty()) {   // setting out from a town that searches what goes out: the warning comes before the ride (as TRAVEL)
            std::string out = searchers(curTown, true, true);
            if (edgeBack) { edgeBack = false; say(out, 9.0); return; }
            int px = tx, py = ty;   // going on anyway, or by night: they open the post's bags at the gate going out
            std::string gone = searchers(curTown, true);
            if (tx != px || ty != py) return;   // fined beyond your purse: prison
            if (!gone.empty()) say(gone, 9.0);
        }
        const Settlement &stD = world.settlements[dest];
        // the lands the road crosses (by the nearest town or post house of each tile), steps in each
        std::map<std::string, float> share;
        float total = 0;
        for (size_t i = 1; i < route.size(); i++) {
            float dd = std::max(0.0f, route[i].d - route[i - 1].d);
            float dm;
            share[rideLandOf(nearestPlace(route[i].x, route[i].y, dm))] += dd;
            total += dd;
        }
        if (total <= 0) total = 1;
        auto frac = [&](std::initializer_list<const char *> ls) { float t = 0; for (auto l : ls) t += share.count(l) ? share[l] : 0.0f; return t / total; };
        std::string fromLand = rideLandOf(world.settlements[from].name), toLand = rideLandOf(stD.name);
        auto isItaly = [](const std::string &l) { return l == "ItalyN" || l == "ItalyS"; };
        auto isFS = [](const std::string &l) { return l == "France" || l == "Switzerland"; };
        bool alps = (isFS(fromLand) && isItaly(toLand)) || (isItaly(fromLand) && isFS(toLand));
        std::string land = "France";   // the land with the most steps
        float most = -1;
        for (auto &e : share) if (e.second > most) { most = e.second; land = e.first; }
        static const std::map<std::string, std::string> captions = {
            {"France", "The post goes on at a canter through the villages of France, the postilion blowing his horn at every turning, and fresh horses at every post house."},
            {"Alps", "Over the Alps the horses give way to mules, and the post goes up the pass at a walk, between walls of rock."},
            {"ItalyN", "Down into the plain of the Po, by long straight roads between poplars and ditches, with a health officer at every city gate."},
            {"ItalyS", "Over the Apennines and down through the oak woods, the postilion with his pistol primed, and the towers of the hill towns standing up on either side."},
            {"Spain", "Over the Sierra Morena by the pass of Despe\xC3\xB1" "aperros, with an escort of the Holy Brotherhood, and down into the olive groves of Andalusia."},
            {"Netherlands", "Across the flat country by causeway and ferry, the windmills turning on every side, and the steeples standing up out of the polders."},
            {"England", "Along the English roads from inn to inn, the hedges high on either side, the post horses changed at every market town."},
            {"Empire", "Through Germany, past villages the war has burned, where the post goes in company and the postmasters ask for news before they give you horses."},
            {"Bohemia", "Through the forests of Bohemia, mile after mile of dark firs, the road cut deep in ruts by the armies' guns."},
            {"Austria", "Along the Danube, broad and grey beside the road, the post houses full of officers coming and going from the Emperor's war."},
            {"Poland", "Out across the plain of Poland under a great sky, past the inns at the crossroads and the carts going slowly to market."},
            {"Ottoman", "By the old Roman road through the Sultan's country, from caravanserai to caravanserai, with the Sultan's couriers galloping past you on the way to the City."}};
        std::string stillLand = alps ? "Alps" : captions.count(land) ? land : "France";
        // the rare event
        const char *forced = std::getenv("GALILEO_POSTEVENT");   // tests: brigands, lame, cordon, ford, or none
        std::string kind;
        {
            Rng rr(hash32((uint32_t)minutes + 977u * (uint32_t)numMem(people.player, "rides") + (uint32_t)from * 131u + (uint32_t)dest * 17u));
            int yr = yearAt(minutes);
            bool risky = yr >= 1631 && yr <= 1634 && (frac({"Spain"}) >= 0.25f || frac({"ItalyS"}) >= 0.25f || frac({"Empire", "Bohemia"}) >= 0.25f);
            bool happens = rr.chance(risky ? 1.0f / 6 : 1.0f / 12);
            std::vector<std::string> kinds = {"brigands", "lame", "ford"};
            if (yr >= 1630 && yr <= 1633 && frac({"ItalyN", "ItalyS"}) >= 0.1f) kinds.push_back("cordon");
            if (forced && *forced) { kind = std::string(forced) == "none" ? "" : forced; }
            else if (happens) kind = kinds[rr.next() % kinds.size()];
            float fr = 0.25f + 0.5f * rr.uni();
            people.player["rides"] = std::to_string(numMem(people.player, "rides") + 1);
            if (!kind.empty()) kind += ":" + std::to_string(fr);
        }
        float fr = 1;
        if (!kind.empty()) { fr = (float)std::atof(kind.substr(kind.find(':') + 1).c_str()); kind = kind.substr(0, kind.find(':')); }
        // a point on the open road at that fraction of the way
        int evX = -1, evY = -1;
        if (!kind.empty()) {
            float target = route.back().d * fr, bd = 1e9f;
            for (auto &pt : route) {
                bool inTown = false;
                for (auto &s : world.settlements) if (s.inside(pt.x, pt.y)) { inTown = true; break; }
                if (inTown || !world.passable(pt.x, pt.y, 0) || world.tile(pt.x, pt.y).bld) continue;
                if (std::fabs(pt.d - target) < bd) { bd = std::fabs(pt.d - target); evX = pt.x; evY = pt.y; }
            }
            if (evX < 0) kind.clear();
        }
        // the inn here, and the stills (built before anything moves)
        int myInn = bldAt(kp.workX, kp.workY, kp.workZ);
        double h0 = std::fmod(minutes, 1440.0) / 60.0;
        std::vector<Scene> stills;
        {
            int ox, oy, dx, dy;
            vec2 at, along(0, 1);
            if (doorOutside(myInn, ox, oy, dx, dy)) { at = vec2(ox + 0.5f, oy + 0.5f); along = vec2((float)dx, (float)dy); }
            else at = vec2(tx + 0.5f, ty + 0.5f);
            stills.push_back(rideStill("post-start", at, along, h0, "The post horses are brought out into the yard, stamping. The postilion straps your bag behind his saddle, blows his horn, and you are away."));
        }
        if (kind.empty()) {   // the still on the road: a tile of the land named, nearest the middle of the way
            float mid = route.back().d * 0.5f, bd = 1e9f;
            int rx = -1, ry = -1;
            size_t ri2 = 0;
            for (int pass = 0; pass < 2 && rx < 0; pass++)
                for (size_t i = 1; i + 1 < route.size(); i++) {
                    auto &pt = route[i];
                    bool inTown = false;
                    for (auto &s : world.settlements) if (s.inside(pt.x, pt.y)) { inTown = true; break; }
                    if (inTown || !world.passable(pt.x, pt.y, 0) || world.tile(pt.x, pt.y).bld) continue;
                    if (pass == 0 && !alps) { float dm; if (rideLandOf(nearestPlace(pt.x, pt.y, dm)) != land) continue; }
                    if (std::fabs(pt.d - mid) < bd) { bd = std::fabs(pt.d - mid); rx = pt.x; ry = pt.y; ri2 = i; }
                }
            if (rx >= 0) {
                vec2 al((float)(route[std::min(ri2 + 3, route.size() - 1)].x - route[ri2 - 1].x), (float)(route[std::min(ri2 + 3, route.size() - 1)].y - route[ri2 - 1].y));
                float L = std::hypot(al.x, al.y);
                al = L > 0.1f ? al * (1.0f / L) : vec2(0, 1);
                stills.push_back(rideStill("post-road-" + stillLand, vec2(rx + 0.5f, ry + 0.5f), al, std::fmod(minutes + ri.ridingMin * 0.5, 1440.0) / 60.0, captions.at(stillLand)));
            }
        }
        // your horses: a post horse is handed in here; your own stays at this inn's stable
        std::string horseMsg;
        if (riding >= 0 && animals.list[riding].hired) dismount();
        for (auto &an : animals.list)
            if (an.mine && an.hired) { an.mine = an.hired = false; an.tethered = true; an.owner = "the inn"; people.player.erase("horse:hired"); }
        for (size_t i = 0; i < animals.list.size(); i++) {
            Animal &an = animals.list[i];
            if (!an.mine || an.hired || animals.kinds[an.kind].id != "horse") continue;
            if ((int)i != riding && std::hypot(an.x - tx, an.y - ty) >= 14) continue;   // left elsewhere: he stays where he is
            if ((int)i == riding) dismount();
            int ox, oy, dx, dy, X = tx, Y = ty;
            if (doorOutside(myInn, ox, oy, dx, dy)) { X = ox + 2 * dx; Y = oy + 2 * dy; }
            findOpen(X, Y);
            an.x = an.fx = X + 0.5f; an.y = an.fy = Y + 0.5f; an.moving = false; an.radius = 0; an.tethered = false;
            an.owner = "you";
            horseMsg = "Your horse will be well looked after here until you come back for him.";
        }
        rememberHorse();
        // the map fills in behind you: every town and post house the road passes (before the stop, if it is stopped)
        {
            float limit = kind.empty() ? 1e30f : route.back().d * fr;
            for (auto &pt : route) {
                if (pt.d > limit) break;
                for (auto &st : world.settlements) {
                    int m = st.hamlet ? 12 : 3;   // as World::roadDistances counts a road touching a place
                    if (pt.x >= st.x0 - m && pt.x < st.x0 + st.w + m && pt.y >= st.y0 - m && pt.y < st.y0 + st.h + m)
                        people.player["been:" + People::townKey(st.name)] = "1";
                }
            }
            if (kind.empty()) people.player["been:" + People::townKey(stD.name)] = "1";
        }
        // the fare
        people.player["money"] = std::to_string(money() - ri.fare);
        squareCoins();   // paid here, in the coin of here, before the ride: squared at Venice, Paris's fare was French coin "a tenth below its worth"
        kp.mem["money"] = std::to_string(std::atol(kp.mem["money"].c_str()) + ri.fare);
        std::string msg;
        double began = minutes;
        bool warnedBeats = false;
        auto runClock = [&](double mins) {
            double target = minutes + mins;
            for (int guard = 0; guard < 400 && target - minutes > 1e-6; guard++) {
                advanceClock(target - minutes, 0);
                if (sceneNow >= 0 || cardUp) warnedBeats = true;   // a beat's scenes began on the way: the stills give way to them
            }
        };
        if (!kind.empty()) {
            runClock(ri.ridingMin * fr);
            long refund = (long)(ri.fare * (1.0f - fr));
            people.player["money"] = std::to_string(money() + refund);
            kp.mem["money"] = std::to_string(std::max(0L, std::atol(kp.mem["money"].c_str()) - refund));
            placePlayer(evX, evY);
            if (kind == "brigands") {
                wildRoll(true, "band");
                msg = "Men step out of the trees across the road with muskets, and the postilion pulls up. \"Down, all of you, and your purses with you.\"";
            } else if (kind == "lame") {
                msg = "Your horse goes lame on a stone, and the postilion swears. \"The next post house is not far: walk on, and I will follow with him.\" He gives you back the rest of the fare.";
            } else if (kind == "cordon") {
                msg = "Soldiers across the road, and a health officer at a table: nobody passes from the plague country without a bill of health. The post turns back, and the postilion gives you back the rest of the fare; you may go round by the fields, on your own feet, and your own risk.";
            } else {
                msg = "The river is over the ford, brown and fast, and the post will not cross today. There is a bridge upstream, the postilion says, a long way round on foot; and he gives you back the rest of the fare.";
            }
        } else {
            runClock(ri.ridingMin);
            // the destination's inn nearest the road you came by
            int bestInn = 0, bx = -1, by = -1;
            float bd = 1e9f;
            for (size_t i = 0; i < world.buildings.size(); i++) {
                const Building &B = world.buildings[i];
                if (B.kind != "inn" || !stD.inside(B.x0, B.y0)) continue;
                int ox, oy, dx, dy;
                if (!doorOutside((int)i + 1, ox, oy, dx, dy)) continue;
                float d = (float)std::hypot(ox - route.back().x, oy - route.back().y);
                if (d < bd) { bd = d; bestInn = (int)i + 1; bx = ox; by = oy; }
            }
            if (!bestInn) { bx = stD.x0 + stD.w / 2; by = stD.y0 + stD.h / 2; }
            for (auto &f : people.contraband(destName)) people.player["warned:" + f.first] = "1";   // the offer warned you
            message.clear();
            placePlayer(bx, by);
            std::string extra = message.empty() || messageMinor ? std::string() : message;
            msg = "You come in with the post to " + destName + " " + rideWhen2(minutes) + ", stiff in every bone.";
            if (!extra.empty()) msg += " " + extra;
        }
        if (!horseMsg.empty()) msg = horseMsg + " " + msg;
        (void)began;
        for (auto &pp : people.people) { pp.actIndex = -2; pp.placed = false; pp.path.clear(); }
        animals.placeNear(world, (float)tx, (float)ty, 150);
        updatePlace(true);
        vec2 pc = playerPos();
        camTarget = vec3(pc.x, pc.y, playerZ(pc));
        if (!warnedBeats && sceneNow < 0 && !cardUp) {
            if (!kind.empty()) stills.resize(1);   // the road still is not shown when the ride is stopped
            afterSeqSay = msg;
            sceneTownQuiet = true;
            seqKeepClock = true;
            startSceneList(stills);
            if (sceneNow < 0) { sceneTownQuiet = false; seqKeepClock = false; std::string m = afterSeqSay; afterSeqSay.clear(); say(m, 9.0); }
        } else say(msg, 9.0);
    }

    // the beats of the plot: the calendar moved on (a letter to read), and the ending
    // gun smoke: where a volley was fired (events.txt "volley"), when (game minutes); drawn as puffs over the world
    struct Smoke { float x, y; double born; int seed; };
    std::vector<Smoke> smokes;
    int filmFrame = -1;                               // the stand-in's FILM: the frame being made, and the booms in it
    std::vector<std::pair<int, float>> filmBooms;
    struct FilmCue { int frame; std::string id; float level; };
    struct FilmSound { int frame; std::string id; float vol; };
    std::vector<FilmSound> filmSounds;                // and the musketry and blows heard (People::soundsAt)
    std::map<std::string, int> heardCount;            // the stand-in's HEARD: the fighting's sounds heard since the last
    std::vector<FilmCue> filmCues;                    // and the music an event began (events.txt "cue")
    void takeRequests() {
        if (people.viewYaw > -999) {   // an event turns the camera (events.txt "view"): at once, so that it is so when a card closes
            yaw = yawT = people.viewYaw;
            if (testMode && std::fabs(people.viewYaw - 22) < 0.5f) std::fprintf(stderr, "  [the view turns back to the usual one, 22 degrees]\n");
            else if (testMode) std::fprintf(stderr, "  [the view turns to %.0f degrees (Z: back to the usual view)]\n", people.viewYaw);
            people.viewYaw = -1000;
        }
        if (!people.cueRequest.empty()) {   // an event's music (events.txt "cue"): the background fades away under it
            if (musicLoaded && music.playCue(people.cueRequest, now, 1.5)) { music.setPieceLevel(people.cueLevel); musicBy = -1; }
            if (testMode) std::fprintf(stderr, "  [music: %s, at %.0f%% of full]\n", people.cueRequest.c_str(), people.cueLevel * 100);
            if (filmFrame >= 0) filmCues.push_back({filmFrame, people.cueRequest, people.cueLevel});
            people.cueRequest.clear();
            people.cueLevel = 1;
        }
        if (!people.soundsAt.empty()) {   // the fighting heard: musketry from afar, the blows of soldiers near you
            int blows = 0;
            for (auto &sa : people.soundsAt) {
                float d = std::hypot(sa.x - (tx + 0.5f), sa.y - (ty + 0.5f));
                bool gun = sa.kind == "musket" || sa.kind == "volley";
                float vol = gun ? std::min(1.0f, (sa.kind == "volley" ? 1.15f : 1.0f) - d / (sa.kind == "volley" ? 160.0f : 120.0f))
                                : std::min(0.85f, 0.9f - d / 24.0f);
                if (vol <= 0.05f || (!gun && ++blows > 3)) continue;   // at most three blows at once: the nearest are first heard
                int variants = sa.kind == "musket" ? 6 : 4;
                std::string id = sa.kind + "-" + std::to_string(1 + (int)(wuni() * variants) % variants);
                if (soundsLoaded) sounds.play(id, vol, panTo(sa.x, sa.y));
                heardCount[sa.kind]++;
                if (filmFrame >= 0) filmSounds.push_back({filmFrame, id, vol});
            }
            people.soundsAt.clear();
        }
        if (!people.gunSmoke.empty()) {   // the guns fire: a boom, louder the nearer you are, and their smoke
            float d = 1e9f;
            for (auto &g : people.gunSmoke) {
                smokes.push_back({g[0], g[1], minutes, (int)smokes.size() * 7 + (int)(g[0] * 13 + g[1] * 5)});
                d = std::min(d, std::hypot(g[0] - (tx + 0.5f), g[1] - (ty + 0.5f)));
            }
            float vol = std::max(0.15f, std::min(1.0f, 1.15f - d / 90.0f));
            if (d < 250 && soundsLoaded) sounds.play("cannon", vol);
            if (filmFrame >= 0 && d < 250) filmBooms.push_back({filmFrame, vol});
            if (testMode) {
                if (people.gunSmoke.size() > 2) std::fprintf(stderr, "  [the guns fire: smoke in %zu places, %.0f tiles off; the boom at volume %.2f]\n", people.gunSmoke.size(), d, d < 250 ? vol : 0.0f);
                else std::fprintf(stderr, "  [a gun fires from the gabions, %.0f tiles off]\n", d);
            }
            people.gunSmoke.clear();
        }
        if (people.eventPass > 0 || people.carryX >= 0) {   // events.txt "pass MINUTES" and "carry X Y [beside WHO]": the march
            minutes += people.eventPass;
            people.eventPass = 0;
            healFrom = minutes;
            if (talking) { people.endTalk(minutes); talking = false; }
            fightWith = -1; fightBeast = -1;
            if (people.carryX >= 0) {
                int bx = -1, by = -1;
                for (int r = 0; r <= 6 && bx < 0; r++)
                    for (int dy = -r; dy <= r && bx < 0; dy++)
                        for (int dx = -r; dx <= r && bx < 0; dx++)
                            if (std::max(std::abs(dx), std::abs(dy)) == r && world.passable(people.carryX + dx, people.carryY + dy, 0) &&
                                !people.occupied(people.carryX + dx, people.carryY + dy, 0)) { bx = people.carryX + dx; by = people.carryY + dy; }
                if (bx < 0) { bx = people.carryX; by = people.carryY; }
                tx = bx; ty = by; tz = 0; from = to = vec2(bx + 0.5f, by + 0.5f); stepT = 1;
                // everyone where the hour puts them, at once: nobody marches in along a road (they came with you)
                for (auto &pp : people.people) {
                    pp.actIndex = -2; pp.placed = false; pp.path.clear();
                    if (!pp.wild && std::abs(pp.x - tx) + std::abs(pp.y - ty) < 260) { pp.recruit = false; pp.away = false; }
                }
                people.update(world, 0.01f, minutes, tx, ty, tz);
                if (!people.carryBeside.empty())
                    if (Person *b = people.byId(people.carryBeside)) if (!b->dead) { placeBeside(*b); b->placed = true; b->actIndex = -2; }
                updatePlace(true);
                vec2 pc = playerPos();
                camTarget = vec3(pc.x, pc.y, playerZ(pc));
            } else for (auto &pp : people.people) { pp.actIndex = -2; pp.placed = false; pp.path.clear(); }
            people.carryX = people.carryY = -1;
            people.carryBeside.clear();
        }
        // a card, or a jump of the calendar, asked for while you talk waits until you leave the conversation, so
        // that what was said can be read (Mersenne sending you to Florence, then the card of chapter I); the
        // stand-in prints cards without ending the conversation, and takes them at once
        bool waitTalk = talking && !testMode && (people.jumpTo >= 0 || !people.cardRequest.empty());
        std::string cardId = waitTalk ? std::string() : people.cardRequest;
        if (!waitTalk) people.cardRequest.clear();
        bool cardShown = false;
        if (people.jumpTo >= 0 && !waitTalk) {
            long to = people.jumpTo;
            std::string page = people.jumpPage;
            people.jumpTo = -1; people.jumpPage.clear();
            if (to > minutes) {   // (a jump to a time already reached moves nothing, and says nothing)
                double before = minutes;
                minutes = to;
                for (auto &pp : people.people) { pp.actIndex = -2; pp.placed = false; }
                horseNight();
                say("The months pass. It is " + dateString(minutes) + ".", 6.0);
                if (!cardId.empty()) { if (showCard(cardId, before, page, !testMode)) page.clear(); }
            }
            cardShown = true;   // (a jump that moved nothing shows no card)
            openPage(page);
        }
        if (!cardId.empty() && !cardShown && showCard(cardId, -1, "", !testMode)) {}   // a card with no jump (chapter I)
        if (people.endingRequest) {
            people.endingRequest = false;
            Book b;
            b.id = "ending"; b.language = "english"; b.title = b.titleEn = people.endingTitle;
            std::string cur = "\x01";
            for (auto &L : people.ending) {
                if (!L.conds.empty() && !people.condsHold(L.conds, minutes)) continue;
                if (L.page != cur) {
                    BookPage pg;
                    pg.orig = pg.english = L.page.empty() ? "" : L.page + " //\n\n";
                    b.pages.push_back(pg);
                    cur = L.page;
                }
                BookPage &pg = b.pages.back();
                if (pg.orig.size() && pg.orig.back() != '\n') pg.orig += "\n\n";
                std::string text = L.text;   // $[KEY]: what you remember under KEY (how many keepers)
                for (size_t a; (a = text.find("$[")) != std::string::npos;) {
                    size_t z = text.find(']', a);
                    if (z == std::string::npos) break;
                    auto it = people.player.find(text.substr(a + 2, z - a - 2));
                    text.replace(a, z - a + 1, it == people.player.end() ? "no" : it->second);
                }
                pg.orig += text;
                pg.english = pg.orig;
            }
            if (!b.pages.empty()) {
                items.books["ending"] = b;
                openBook("ending");
                if (testMode)   // every page, for the tests
                    for (size_t q = 0; q < b.pages.size(); q++) std::fprintf(stderr, "  [ending %zu] %s\n", q + 1, b.pages[q].orig.c_str());
            }
            people.player["ending-seen"] = std::to_string((long)minutes);
            say("Your story is over. What became of them is at the top of your journal (J); the world goes on.", 12.0);
        }
    }

    // a town's plan for the journal, one pixel a tile, north up, in the journal's inks: streets and
    // squares pale, houses buff, churches and the great buildings darker, the walls darkest, water
    // blue, gardens and fields green (Hud::setPlan keeps it)
    std::vector<uint32_t> planPixels(const TownPlanInfo &p) {
        std::vector<uint32_t> px((size_t)p.w * p.h);
        auto rgb = [](int r, int g, int b) { return 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b; };
        for (int r = 0; r < p.h; r++)
            for (int c = 0; c < p.w; c++) {
                int X = p.x0 + c, Y = p.y0 + p.h - 1 - r;
                const Tile &t = world.tile(X, Y);
                uint32_t col;
                if (t.wall == W_CITYWALL || t.wall == W_TOWER || t.wall == W_GATE || t.wall == W_PARAPET) col = rgb(70, 58, 48);
                else if (t.bld) {
                    const Building &b = world.buildings[t.bld - 1];
                    const std::string &k = b.kind;
                    if (k == "church" || k == "cathedral" || k == "chapel") col = rgb(128, 84, 70);
                    else if (k == "palace" || k == "hall" || k == "college" || k == "fortress" || k == "tower" || k == "gallery" || k == "theatre" || k == "mansion") col = rgb(150, 112, 84);
                    else if (k == "market" || k == "place") col = rgb(222, 208, 176);
                    else col = rgb(184, 160, 124);
                }
                else switch (t.terrain) {
                    case T_DEEP: case T_WATER: col = rgb(96, 126, 150); break;
                    case T_BRIDGE: col = rgb(170, 150, 118); break;
                    case T_STREET: case T_ROAD: col = rgb(226, 214, 186); break;
                    case T_SQUARE: col = rgb(234, 224, 198); break;
                    case T_GARDEN: case T_YARD: col = rgb(170, 176, 120); break;
                    case T_FIELD: col = rgb(196, 188, 128); break;
                    case T_MARSH: col = rgb(150, 160, 128); break;
                    case T_SAND: col = rgb(220, 206, 160); break;
                    default: col = rgb(186, 190, 136); break;   // grass, meadow, scrub
                }
                if (t.wall == W_HEDGE) col = rgb(120, 140, 90);
                px[(size_t)r * p.w + c] = col;
            }
        return px;
    }
    // a ship of your own (amsterdam.txt groen, "buyship PRICE"): a pinnace moored on the nearest water
    // beside a quay. Yours is the ship at player ship:at (X,Y) of kind ship:kind, kept up to date when
    // you moor her; no other sea-going ship can be boarded (the passages carry passengers)
    void shipDeal(int seller, long price, const std::string &kind) {
        if (people.player.count("ship:at")) { say("\"You have a ship of your own already: one is enough for any man.\"", 3.5); return; }
        if (money() < price) { say("You have not the money: she is " + moneyString(price) + ".", 3.5); return; }
        int k = items.find(kind);
        if (k < 0) return;
        Person &sp = people.people[seller];
        int bx = 0, by = 0;
        float bf = 0;
        if (!items.shoreSpot(world, sp.x, sp.y, 80, bx, by, bf)) { say("\"She is out in the roads; come back tomorrow.\"", 3.0); return; }
        people.player["money"] = std::to_string(money() - price);
        sp.mem["money"] = std::to_string(std::atol(sp.mem["money"].c_str()) + price);
        items.moor(k, bx, by, bf);
        people.player["ship:at"] = std::to_string(bx) + "," + std::to_string(by);
        people.player["ship:kind"] = kind;
        say((price > 0 ? "You pay " + moneyString(price) + ". " : std::string()) + capital(items.kinds[k].name) + " of your own lies at the quay: walk aboard her to sail, and wherever you drop anchor, she waits for you.", 7.0);
    }
    // a named place you have stepped into, by town (player visited:TOWNKEY:NAME = when), for the
    // journal's pages of places and people (People::townPages)
    void noteVisit(int bld) {
        if (bld <= 0 || bld > (int)world.buildings.size()) return;
        const Building &b = world.buildings[bld - 1];
        if (b.name.empty()) return;
        std::string town;
        for (auto &s : world.settlements) if (s.inside(tx, ty)) { town = s.name; break; }
        if (town.empty()) return;
        std::string k = "visited:" + People::townKey(town) + ":" + b.name;
        if (!people.player.count(k)) people.player[k] = std::to_string((long)minutes);
    }
    void updatePlace(bool quiet = false) {
        if (tz != 0) {
            Level *lv = world.levelAt(tx, ty, tz);
            inside = lv ? lv->bld : 0;
            place = world.floorName(tx, ty, tz);
            noteVisit(inside);
            lastTx = tx; lastTy = ty; lastTz = tz;
            return;
        }
        int b = world.tile(tx, ty).bld;
        Building *bd = b ? &world.buildings[b - 1] : nullptr;
        int newInside = 0;
        if (bd) newInside = b;
        if (newInside != inside) {
            if (!quiet && bd && !bd->name.empty() && bd->kind != "gate") note("You enter " + bd->name + ".");
            if (!quiet && bd && !bd->name.empty() && bd->kind == "gate") note(bd->name);
            inside = newInside;
        }
        noteVisit(inside);
        place = world.placeName(tx, ty);
        {   // the building you are in, for the events ("player inside=the-palace-of-the-holy-office": the closet)
            int b = 0;
            if (tz == 0) b = world.tile(tx, ty).bld;
            else if (Level *lv = world.levelAt(tx, ty, tz)) b = lv->bld;
            if (b > 0 && b <= (int)world.buildings.size() && !world.buildings[b - 1].name.empty()) people.player["inside"] = People::townKey(world.buildings[b - 1].name);
            else people.player.erase("inside");
            // standing in a town's square ("player on-square": at the muster of Monro's Scots, events.txt the-march)
            if (tz == 0 && b == 0 && world.tile(tx, ty).terrain == T_SQUARE) people.player["on-square"] = "1";
            else people.player.erase("on-square");
        }
        // into a town from outside: its customs (data/market.txt) on the goods you carry
        std::string town;
        for (auto &s : world.settlements) if (s.inside(tx, ty)) { town = s.name; break; }
        if (town != curTown && !sceneTownQuiet) {
            if (!curTown.empty() && !quiet) {   // out of a town that lets nothing forbidden out (the Dialogue seized)
                std::string out = searchers(curTown, true);
                if (!out.empty()) say(out, 9.0);
                if (edgeBack) { edgeBack = false; turnBacks++; turnTown = curTown; turnBack(false); return; }
                if (town.empty() && out.empty()) {   // out into the open country, with a lot of money
                    std::string warn = purseWarning();
                    if (!warn.empty()) say(warn, 10.0);
                }
            }
            if (!town.empty()) {
                std::string what;
                long duty = people.customs(town, what);
                std::string msg = duty > 0 ? "At the edge of " + town + " the customs officers look through what you carry and take " + moneyString(duty) + " on " + what + "." : "";
                std::string smug = searchers(town);
                if (edgeBack) { edgeBack = false; turnBacks++; turnTown = town; say(smug, 9.0); turnBack(true); return; }
                if (!smug.empty()) msg += (msg.empty() ? "" : " ") + smug;
                if (!msg.empty()) say(msg, smug.empty() ? 6.0 : 9.0);
            }
            curTown = town;
            people.player["town"] = town;
            if (!town.empty()) {   // the towns you have been to, for people's lines ("player been:florence")
                people.player["been:" + People::townKey(town)] = "1";
            }
        }
        lastTx = tx; lastTy = ty; lastTz = tz;
    }
    std::string curTown;   // the town you are in, for the customs ("" outside)
    int walkTb = 0;         // turnBacks when the stand-in's current command began
    int turnBacks = 0;      // how many times the searchers' warning has turned you back (the stand-in's walks stop at one)
    std::string turnTown;   // and at which town's edge
    bool edgeBack = false;   // searchers() only warned: put the player back (see turnBack)
    int lastTx = -1, lastTy = -1, lastTz = 0;   // the tile of the previous updatePlace
    // After a warning at a town's edge: back to the last tile before it (a step), or, for a journey
    // by road that arrived at the town, onto a road tile just outside it, nearest where you came from.
    void turnBack(bool entering) {
        bool adj = lastTx >= 0 && lastTz == tz && std::abs(tx - lastTx) <= 1 && std::abs(ty - lastTy) <= 1;
        if (adj || (!entering && lastTx >= 0)) { tx = lastTx; ty = lastTy; tz = lastTz; }
        else if (entering) {
            const Settlement *st = nullptr;
            for (auto &s : world.settlements) if (s.inside(tx, ty)) { st = &s; break; }
            if (st) {
                int bx = -1, by = -1, bd = 1 << 30;
                for (int pass = 0; pass < 2 && bx < 0; pass++)
                    for (int r = 1; r <= 12 && bx < 0; r++)
                        for (int y = st->y0 - r; y < st->y0 + st->h + r; y++)
                            for (int x = st->x0 - r; x < st->x0 + st->w + r; x++) {
                                if (st->inside(x, y) || !world.inWorld(x, y) || !world.passable(x, y)) continue;
                                if (std::max(std::max(st->x0 - x, x - (st->x0 + st->w - 1)), std::max(st->y0 - y, y - (st->y0 + st->h - 1))) != r) continue;
                                if (pass == 0 && world.tile(x, y).terrain != T_ROAD) continue;
                                int d = lastTx >= 0 ? std::abs(x - lastTx) + std::abs(y - lastTy) : 0;
                                if (d < bd) { bd = d; bx = x; by = y; }
                            }
                if (bx >= 0) { tx = bx; ty = by; tz = 0; }
            }
        }
        from = to = vec2(tx + 0.5f, ty + 0.5f);
        stepT = 1;
        if (entering) { curTown = ""; people.player["town"] = ""; }
        vec2 p = playerPos();
        camTarget = vec3(p.x, p.y, world.groundZ(p.x, p.y));
        updatePlace(true);
    }
    // SMUGGLING: goods a town forbids (market.txt "forbids") are found by the searchers at its
    // edge, most times by day (85 in 100), a third of the time at night (22:00 to 4:30), and much
    // less often in a pack with a false bottom (item false-pack); found, they are seized and you
    // are fined their worth, or spend the night in prison. Returns what happened, for the message.
    std::string searchers(const std::string &town, bool leaving = false, bool warnOnly = false) {
        std::string why;
        auto found = people.contraband(town, &why, leaving);
        if (found.empty()) return "";
        // passed the searchers here within the hour (a walk along a town's edge crosses it again and
        // again, at Venice most of all): they do not search you a second time
        {
            auto it = people.player.find("passed-searchers:" + town);
            if (it != people.player.end() && minutes - std::atof(it->second.c_str()) < 60) return "";
        }
        // the dark is the dark you see (when the sky is black the searchers must not
        // search as by day)
        bool night = darkNow();
        float chance = night ? 0.35f : 0.85f;
        bool hidden = numMem(people.player, "has:false-pack") > 0;
        if (hidden) chance = night ? 0.0f : chance * 0.3f;   // a false bottom at night is foolproof
        if (const char *e = std::getenv("GALILEO_SEARCH")) chance = (float)std::atof(e);   // tests: 1 always found, 0 never
        std::string what;
        long value = 0;
        for (size_t i = 0; i < found.size(); i++) {
            int k = items.find(found[i].first);
            if (k < 0) continue;
            value += items.kinds[k].price * found[i].second;
            what += (what.empty() ? "" : i + 1 == found.size() ? " and " : ", ") + items.nameOf(k, (int)found[i].second);
        }
        // By daylight the searchers are seen from the road. Every time you bring a forbidden
        // thing (the Dialogue, the manuscript, or market goods such as salt) to a town's edge by day, you step
        // back unsearched, and are put back on the side you came from; unless you come on again within the hour
        // of being turned back here ("go on anyway"): then they search you. In the dark they are not seen: only
        // the first time you ever carry a kind of thing to a gate are you warned; after that the night is your
        // gamble (warned:ITEM: you have been told; turned-back:TOWN: when you last stepped back there).
        bool firstOfKind = false;
        for (auto &f : found)
            if (!people.player.count("warned:" + f.first)) firstOfKind = true;
        bool goingOn = false;
        {
            auto it = people.player.find("turned-back:" + town);
            goingOn = it != people.player.end() && minutes - std::atof(it->second.c_str()) < 60;
        }
        bool warnNow = night ? firstOfKind && !goingOn : !goingOn;
        if (warnNow) {
            for (auto &f : found) people.player["warned:" + f.first] = "1";
            people.player["turned-back:" + town] = std::to_string((long)minutes);
            edgeBack = true;
            if (night)
                return "In the dark at the edge of " + town + " a searcher is turning out a pack by the light of a lantern, and you remember what is in yours: " +
                       what + ". You " + (leaving ? "turn back" : "step back") + " before he sees you. By day they search everyone; in the dark they are fewer, and lazier, but not blind.";
            return "At the edge of " + town + " the searchers are turning out every pack and basket that " + (leaving ? "goes out" : "goes in") +
                   ", and you remember what is in yours: " + what + ". You " + (leaving ? "turn back" : "step back") +
                   " before they see you. They search everyone by day, and find nearly everything. " + (leaving ? "Go on out now" : "Go on into the town now") +
                   " and you take your chance with them; or wait for the dark, when they are fewer, and lazier, but not blind.";
        }
        if (warnOnly) return "";
        std::string onAnyway = goingOn && !night ? "You go on, and take your chance. " : "";
        Rng r(hash32((uint32_t)minutes * 2654435761u ^ (uint32_t)std::hash<std::string>()(town)));
        float roll = (r.next() % 10000) / 10000.0f;
        if (roll >= chance) {
            people.player["smuggled"] = std::to_string(numMem(people.player, "smuggled") + 1);
            people.player["passed-searchers:" + town] = std::to_string((long)minutes);
            return onAnyway + std::string(night ? "In the dark" : "At the edge of " + town) + " the searchers " +
                   (hidden ? "turn out your pack and find nothing under its false bottom: you carry " : "wave you through: you carry ") + what + (leaving ? " out of " : " into ") + town + " unseen.";
        }
        for (auto &f : found) {   // seized; and remembered, for the people of the plot (seized:ITEM, how many)
            people.player["seized:" + f.first] = std::to_string(numMem(people.player, "seized:" + f.first) + f.second);
            people.player.erase("has:" + f.first);
        }
        people.player["seized-at"] = std::to_string((long)minutes);
        people.player["smuggle-caught"] = std::to_string(numMem(people.player, "smuggle-caught") + 1);
        bool many = found.size() > 1 || found[0].second > 1;
        std::string text = onAnyway + "The searchers at the edge of " + town + " find " + what + " in your pack, and take " + (many ? "them" : "it") + ". \"" + why + "\"";
        if (value <= 0) return text;   // a thing with no price (Galileo's manuscript): only taken
        if (money() >= value) {
            people.player["money"] = std::to_string(money() - value);
            return text + " You are fined " + moneyString(value) + ", what it was worth.";
        }
        people.player["money"] = "0";
        prison(town, "");
        return "";
    }

    std::string whatBlocks(int X, int Y) {
        if (!world.inWorld(X, Y)) return "the edge of the world";
        Tile *tp = world.tileAt(X, Y, tz);
        if (!tp) {
            Level *cur = tz > 0 ? world.levelAt(this->tx, this->ty, tz) : nullptr;
            if (cur && cur->parapet && !cur->inside(X, Y)) return "the parapet";
            return tz < 0 ? "solid rock" : "a long drop";
        }
        Tile &t = *tp;
        if (tz < 0 && t.wall != W_NONE) return "rock";
        if (t.terrain == T_WATER || t.terrain == T_DEEP) return "water";
        if (t.terrain == T_ROCK || t.terrain == T_SNOW) return "the mountains";
        if (t.wall == W_CITYWALL || t.wall == W_TOWER) return "the town wall";
        if (t.wall == W_HEDGE) return "a hedge";
        if (t.wall == W_EARTHWORK) return "a rampart of earth";
        if (t.wall != W_NONE) return "a wall";
        switch (t.feature) {
            case F_OAK: case F_PINE: case F_FRUIT: case F_POPLAR: return "a tree";
            case F_BOULDER: return "a boulder";
            case F_PILLAR: return "a pillar";
            case F_TABLE: return "a table";
            case F_HEARTH: return "the hearth";
            case F_BED: return "a bed";
            case F_ALTAR: return "the altar";
            case F_WELL: return "a well";
            case F_FOUNTAIN: return "a fountain";
            case F_STALL: return "a market stall";
            case F_BELL: return "the bell";
            case F_BARREL: return "a barrel";
            case F_CHEST: return "a chest";
            default: return "something";
        }
    }

    // ---- boats: in one, you go where there is water, under bridges too, and step ashore
    // onto any land, leaving it tied up where it is. Which boat you are in is remembered
    // ("boat" in what the player knows), so it is saved with the game.
    bool boatWater(int X, int Y) {
        if (tz != 0 || !world.inWorld(X, Y)) return false;
        const Tile &t = world.tile(X, Y);
        if (t.terrain != T_WATER && t.terrain != T_DEEP && t.terrain != T_BRIDGE) return false;
        if (t.terrain == T_BRIDGE && boat >= 0 && items.kinds[boat].shape == "ship") return false;   // a ship's masts do not pass under a bridge
        if (t.terrain == T_DEEP && boat >= 0 && items.kinds[boat].shape != "ship") return false;    // a rowing boat keeps inshore: rivers, harbours, the coast
        if (t.feature == F_MOORING && t.var >= 2) return false;   // a ferry is waiting there
        return items.boatAt(X, Y) < 0;
    }
    float playerZ(vec2 p) { return boat >= 0 ? 0.0f : world.floorAt(p.x, p.y, tz); }
    void syncBoat() {
        auto it = people.player.find("boat");
        boat = it == people.player.end() ? -1 : items.find(it->second);
    }
    std::string theBoat() const {
        if (boat >= 0 && people.player.count("boat-mine")) return "your pinnace";   // the ship you bought (shipDeal)
        std::string n = boat >= 0 ? items.kinds[boat].name : "the boat";
        if (n.rfind("a ", 0) == 0) n = "the " + n.substr(2);
        else if (n.rfind("an ", 0) == 0) n = "the " + n.substr(3);
        return n;
    }
    // a step in a boat, or into one or out of one. True if the step is dealt with.
    bool boatStep(int dx, int dy, bool run) {
        int nx = tx + dx, ny = ty + dy;
        auto go = [&](float dur) {
            from = vec2(tx + 0.5f, ty + 0.5f);
            tx = nx; ty = ny;
            to = vec2(tx + 0.5f, ty + 0.5f);
            stepDur = dur * ((dx && dy) ? 1.3f : 1.0f);
            stepT = 0;
            facingTarget = std::atan2(-(float)dx, (float)dy);
            minutes += roadMinutes();   // rowing between towns takes time, as walking does
            updatePlace();
        };
        if (boat < 0) {
            if (tz != 0 || (dx && dy)) return false;
            int bi = items.boatAt(nx, ny);
            if (bi < 0) return false;
            const Thing &t = items.things[bi];
            if (items.kinds[t.kind].shape == "ship") {   // a sea-going ship: only your own (shipDeal); the rest have masters and crews
                auto at = people.player.find("ship:at"), kd = people.player.find("ship:kind");
                bool mine = at != people.player.end() && at->second == std::to_string(t.x) + "," + std::to_string(t.y) &&
                            kd != people.player.end() && kd->second == items.kinds[t.kind].id;
                if (!mine) { say("Her crew look down at you from the rail. She is not yours to sail: passengers take a passage, from a ship's master.", 4.0); return false; }
                people.player["boat-mine"] = "1";
            }
            boat = t.kind;
            people.player["boat"] = items.kinds[boat].id;
            items.take(bi);
            go(0.3f);
            facing = facingTarget = t.facing;   // you sit in it the way it lies
            if (items.kinds[boat].shape == "ship") say("You go aboard " + theBoat() + ". (Walk onto land to go ashore.)", 4.0);
            else say("You step down into " + theBoat() + ". (Walk onto land to step out.)", 4.0);
            return true;
        }
        bool ship = items.kinds[boat].shape == "ship";
        bool ok = boatWater(nx, ny);
        if (ok && dx && dy && !boatWater(tx + dx, ty) && !boatWater(tx, ty + dy)) ok = false;   // not across a corner of the bank
        if (ok && ship) {   // sailing: twice as fast as running, and a step at sea costs less than half a step on the road
            float nav = 1.0f - 0.15f * (float)std::min(2L, numMem(people.player, "skill:navigation"));   // a pilot's lessons
            go((run ? 0.045f : 0.075f) * nav);
            minutes -= roadMinutes() * 0.55;
            return true;
        }
        if (ok) {   // rowing: a little faster for each lesson (two at most)
            float skill = 1.0f - 0.15f * (float)std::min(2L, numMem(people.player, "skill:rowing"));
            go((run ? 0.12f : 0.2f) * skill);
            return true;
        }
        if (!(dx && dy) && world.passable(nx, ny, 0) && !people.occupied(nx, ny, 0)) {
            items.moor(boat, tx, ty, facing);
            if (ship && people.player.count("boat-mine")) {   // where your ship waits now
                people.player["ship:at"] = std::to_string(tx) + "," + std::to_string(ty);
                people.player.erase("boat-mine");
            }
            say(ship ? "You drop anchor and go ashore." : "You tie up " + theBoat() + " and step ashore.", 3.0);
            boat = -1;
            people.player.erase("boat");
            return false;   // and now the ordinary step onto the land
        }
        if (blockedCooldown <= 0) {
            bool deep = !ship && world.inWorld(nx, ny) && world.tile(nx, ny).terrain == T_DEEP;
            say(items.boatAt(nx, ny) >= 0 ? "There is a boat in the way." : ship ? "You cannot sail there." :
                deep ? "Beyond here is the open sea, and its swell would swamp her: a rowing boat keeps to rivers, harbours and the shore." : "You cannot row there.", deep ? 3.5 : 1.5);
            blockedCooldown = 0.6f;
        }
        facingTarget = std::atan2(-(float)dx, (float)dy);
        return true;
    }

    bool canStep(int dx, int dy, bool pastPeople = false) {
        int nx = tx + dx, ny = ty + dy;
        if (!world.passable(nx, ny, tz)) return false;
        if (!pastPeople && people.occupied(nx, ny, tz)) return false;
        if (dx && dy) {
            bool a = world.blocksDiagonal(tx + dx, ty, tz), b = world.blocksDiagonal(tx, ty + dy, tz);
            // a lane pinched shut where two separate things meet only at a corner (a house and the town
            // wall, or two houses): you squeeze through it (rather than going all the way
            // round a house)
            bool pinch = false;
            if (tz == 0 && !world.passable(tx + dx, ty, tz) && !world.passable(tx, ty + dy, tz)) {
                const Tile &ta = world.tile(tx + dx, ty), &tb = world.tile(tx, ty + dy);
                pinch = world.tile(tx, ty).bld == 0 && world.tile(nx, ny).bld == 0 && ta.bld != tb.bld;
            }
            if (!pinch && (a || b)) return false;
            if (!pinch && tz == 0 && (!world.passable(tx + dx, ty, tz) && !world.passable(tx, ty + dy, tz))) return false;
        }
        return true;
    }

    // a guarded place (a town file's "guarded"): may you go up (or down) into it by this link?
    // Its guard, while he is in the building, turns you back unless you have leave; while he is
    // away it is locked. Its key opens it either way. Leaving it is always allowed.
    bool mayEnter(const Link &lk) {
        Level *to = lk.z != 0 ? world.levelAt(lk.x, lk.y, lk.z) : nullptr;
        if (!to) return true;
        Level *here = tz != 0 ? world.levelAt(tx, ty, tz) : nullptr;
        if (here == to) return true;
        for (const auto &gd : people.guarded) {
            if (lowerS(gd.place) != lowerS(to->name)) continue;
            if (!gd.key.empty() && numMem(people.player, "has:" + gd.key) > 0) return true;
            const Person *p = nullptr;
            for (auto &q : people.people) if (q.id == gd.by) { p = &q; break; }
            bool there = false;
            if (p && !p->lying && !p->onRoad) {
                if (p->z == 0) { const Tile *t = world.tileAt(p->x, p->y, 0); there = t && t->bld == to->bld; }
                else { Level *pl = world.levelAt(p->x, p->y, p->z); there = pl && pl->bld == to->bld; }
            }
            if (there && people.condsHold(gd.pass, minutes)) return true;
            if (there) applyEffects(gd.stopEffects);
            say(there ? gd.stop : gd.shut, 8.0);
            if (walkEcho) std::fprintf(stderr, "  [%s] %s\n", there ? "stopped" : "shut", (there ? gd.stop : gd.shut).c_str());
            return false;
        }
        return true;
    }
    bool walkEcho = false;   // tests: print what stops you
    // a ladder, a stair, a trapdoor or a shaft: climbed only when you use it (U and a click), so
    // that you never climb one by walking past it
    bool isClimb(int X, int Y, int z) {
        const Tile *t = world.tileAt(X, Y, z);
        return t && (t->feature == F_LADDER || t->feature == F_HATCH || t->feature == F_STAIRS || t->feature == F_SHAFT);
    }
    void followLink(Link lk, int dx, int dy) {
        facingTarget = std::atan2(-(float)dx, (float)dy);
        if (!mayEnter(lk)) return;
        tx = lk.x; ty = lk.y; tz = lk.z;
        from = to = vec2(tx + 0.5f, ty + 0.5f);
        stepT = 1;
        minutes += 1.0;
        say(lk.verb, 3.5);
        updatePlace(true);
        camTarget = vec3(from.x, from.y, world.floorAt(from.x, from.y, tz));
    }
    void tryStep(int dx, int dy, bool run) {
        int sx = dx, sy = dy;
        if (riding >= 0) {   // on horseback: not through a doorway, up a ladder or into a ferry
            const Tile &nt = world.tile(tx + dx, ty + dy);
            if (tz != 0 || world.linkAt(tx + dx, ty + dy, tz) || nt.wall == W_DOOR) {
                facingTarget = std::atan2(-(float)dx, (float)dy);
                if (blockedCooldown <= 0) { say("Not on horseback: get down first.", 2.5); blockedCooldown = 0.8f; }
                return;
            }
        }
        if (riding < 0 && boatStep(dx, dy, run)) return;
        if (const Link *lk = world.linkAt(tx + dx, ty + dy, tz)) {
            if (isClimb(tx + dx, ty + dy, tz)) {      // you stop at the foot of it
                facingTarget = std::atan2(-(float)dx, (float)dy);
                if (blockedCooldown <= 0) { say("To climb it, use it: U, then click on it.", 2.5); blockedCooldown = 0.8f; }
                return;
            }
            followLink(*lk, dx, dy);                  // a ferry: walk into the boat
            return;
        }
        if (!canStep(dx, dy)) {
            if (dx && dy && canStep(dx, 0)) dy = 0;
            else if (dx && dy && canStep(0, dy)) dx = 0;
            else {
                // pushing on into someone who is in your way: you squeeze past (so that a crowd,
                // as at the Pomme de Pin at supper, cannot hold you)
                bool pushing = pushX == tx + sx && pushY == ty + sy && now - pushAt < 1.5;
                if (people.occupied(tx + sx, ty + sy, tz) && pushing && blockedCooldown <= 0 && canStep(sx, sy, true)) {
                    std::string l = people.squeezePast(tx, ty, tx + sx, ty + sy, tz);
                    if (!l.empty()) {
                        say("\"Excuse me,\" you say, and squeeze past " + l + ".", 1.5);
                        pushX = pushY = -1;
                        dx = sx; dy = sy;
                        goto step;
                    }
                }
                if (people.occupied(tx + sx, ty + sy, tz) && blockedCooldown <= 0) {
                    pushX = tx + sx; pushY = ty + sy; pushAt = now;
                    for (auto &pp : people.people)
                        if (pp.x == tx + sx && pp.y == ty + sy && pp.z == tz && !pp.onRoad) {
                            std::string l = people.label(pp);
                            l[0] = (char)std::toupper((unsigned char)l[0]);
                            say(l + " is in your way. (T to talk.)", 2.0);
                        }
                    blockedCooldown = 0.6f;
                    facingTarget = std::atan2(-(float)sx, (float)sy);
                    return;
                }
                if (blockedCooldown <= 0) {
                    say("Blocked by " + whatBlocks(tx + sx, ty + sy) + ".", 1.5);
                    blockedCooldown = 0.6f;
                }
                facingTarget = std::atan2(-(float)sx, (float)sy);
                return;
            }
        }
    step:
        from = vec2(tx + 0.5f, ty + 0.5f);
        tx += dx; ty += dy;
        leadAlong(tx - dx, ty - dy);
        to = vec2(tx + 0.5f, ty + 0.5f);
        bool hungry = riding >= 0 && !animals.list[riding].hired && numMem(people.player, "horse:hungry") >= 2;
        stepDur = gallop ? 0.03f : riding >= 0 ? (run ? 0.045f : 0.075f) * (hungry ? 1.6f : 1.0f) : (run ? 0.075f : 0.15f);
        if (dx && dy) stepDur *= 1.3f;
        stepT = 0;
        lastStepAt = now;
        facingTarget = std::atan2(-(float)dx, (float)dy);
        minutes += roadMinutes() * (gallop ? 0.2 : riding >= 0 ? (hungry ? 0.6 : 0.4) : 1.0);
        updatePlace();
    }
    // Out on the road a tile stands for most of a kilometre (the land between the towns is
    // squeezed), so each step there costs time (world.txt "journey"); in a town, only real time.
    double roadMinutes() const {
        if (tz != 0) return 0;
        for (auto &s : world.settlements) if (s.inside(tx, ty)) return 0;
        return world.journeyMinutes;
    }

    void update(float dt) {
        if (fadeT >= 0) {   // a night's sleep: dark, the clock moves on in the dark, then the morning fades in
            fadeT += dt;
            if (!fadeDone && fadeT >= 1.1f) {
                fadeDone = true;
                if (fadeAct) { auto act = fadeAct; fadeAct = nullptr; act(); }
                else applyRest(fadeMins, fadeHeal, fadeText);
            }
            if (fadeT >= 2.9f) fadeT = -1;
            else if (fadeT < 1.9f) { now += dt; if (musicLoaded) music.update(now); return; }   // nothing moves till it lightens
        }
        if (sceneNow >= 0 || cardUp) { now += dt; if (musicLoaded) music.update(now); return; }   // a still scene, or a card: nothing moves, but the music plays
        if (creating) {   // the world waits while you choose who you are; the view looks at you
            now += dt;
            float e = std::min(1.0f, dt * 8);
            zoom += (zoomT - zoom) * e;
            yaw += (yawT - yaw) * e;
            pitch += (pitchT - pitch) * e;
            facing = facingTarget = 3.14159265f - yaw * 3.14159265f / 180.0f;   // turned to face you
            vec2 p = playerPos();
            camTarget = vec3(p.x, p.y, playerZ(p) + 0.6f);
            if (musicLoaded) music.update(now);   // the music plays while you choose
            return;
        }
        now += dt;
        blockedCooldown -= dt;
        // time goes on with real time (world.txt "clock"; a step on the open road adds more), but not while
        // you talk, read, or have the journal, what you carry or the map open (so that talk
        // does not eat the day); each thing you say in a talk costs a minute instead (the Return key, below)
        if (!talking && panel == 0 && !journal && !showMap) minutes += dt * world.clockRate;
        postClock();
        if (stepT < 1) {
            stepT = std::min(1.0f, stepT + dt / stepDur);
            walkPhase += dt / (stepDur * 2.0f);
        } else {
            walkPhase = 0;
        }
        // facing: shortest rotation
        float d = facingTarget - facing;
        while (d > PI_F) d -= 2 * PI_F;
        while (d < -PI_F) d += 2 * PI_F;
        facing += d * std::min(1.0f, dt * 14);

        // held movement keys
        people.update(world, dt, minutes, tx, ty, tz);
        takeRequests();
        animals.placeTimer -= dt;
        if (animals.placeTimer <= 0) { animals.placeNear(world, (float)tx, (float)ty, 150); animals.placeTimer = 2; }
        animals.update(world, dt, minutes, tx, ty);
        syncRidden();
        horseNight();
        if (people.player.count("horse:stolen-x") &&
            std::max(std::abs(tx - numMem(people.player, "horse:stolen-x")), std::abs(ty - numMem(people.player, "horse:stolen-y"))) < 8) {
            people.player.erase("horse:stolen-x"); people.player.erase("horse:stolen-y");
            say("Your horse is gone from where you left it. Somebody has taken it in the night.", 5.0);
        }
        watchStep();
        fight(dt);
        startRequestedPiece();
        wildStep(dt);
        squareCoins();
        soundStep(dt);
        if (musicLoaded) {
            pieceLevel();
            // inside a church (or up its tower) of a town with a choir: the church music
            bool church = false;
            if (inside > 0 && inside <= (int)world.buildings.size()) {
                const std::string &k = world.buildings[inside - 1].kind;
                church = (k == "church" || k == "cathedral" || k == "chapel") && music.choirIn(curTown);
            }
            music.setChurch(church, curTown, now);
            // inside an inn (a tavern, a post house): its music, where there is some
            bool inn = inside > 0 && inside <= (int)world.buildings.size() && world.buildings[inside - 1].kind == "inn";
            music.setInn(inn, curTown, countryOf(curTown), now);
            static const bool mlog = std::getenv("GALILEO_SOUNDLOG") != nullptr;
            static int lastInside = -2;
            if (mlog && inside != lastInside) { lastInside = inside; std::fprintf(stderr, "  [inside %d: %s]\n", inside, inside > 0 && inside <= (int)world.buildings.size() ? (world.buildings[inside - 1].name + " (" + world.buildings[inside - 1].kind + ")").c_str() : "outdoors"); }
            music.setPlace(curTown, curTown.empty() ? std::string() : countryOf(curTown));
            // a fight near you: you fighting, someone after you, or soldiers at it within 14 tiles;
            // the music stays eight seconds after the last of it
            bool fighting = fightWith >= 0;
            for (auto &pp : people.people) {
                if (fighting) break;
                if (!pp.active || pp.dead || pp.away || pp.z != tz || pp.target == -1) continue;
                if (pp.target == -2 || (pp.fighter && std::max(std::abs(pp.x - tx), std::abs(pp.y - ty)) <= 14)) fighting = true;
            }
            battleHold = fighting ? 8.0f : std::max(0.0f, battleHold - dt);
            music.setBattle(battleHold > 0, now);
            music.update(now);
            std::string piece = music.takeAnnouncement();
            if (!piece.empty() && message.empty()) say("Music: " + piece + ".", 5.0);
        }
        if (!people.notice.empty()) { say(people.notice, 5.0); people.notice.clear(); }
        if (win && stepT >= 1 && !talking && panel != 3 && !(journal && journalTab == 0)) {   // not while a book or the journal is open: the arrows turn their pages
            auto K = [&](int k) { return glfwGetKey(win, k) == GLFW_PRESS; };
            float sx = 0, sy = 0;
            if (K(GLFW_KEY_UP) || K(GLFW_KEY_W) || K(GLFW_KEY_KP_8)) sy += 1;
            if (K(GLFW_KEY_DOWN) || K(GLFW_KEY_S) || K(GLFW_KEY_KP_2)) sy -= 1;
            if (K(GLFW_KEY_RIGHT) || K(GLFW_KEY_D) || K(GLFW_KEY_KP_6)) sx += 1;
            if (K(GLFW_KEY_LEFT) || K(GLFW_KEY_A) || K(GLFW_KEY_KP_4)) sx -= 1;
            if (K(GLFW_KEY_KP_9)) { sx += 1; sy += 1; }
            if (K(GLFW_KEY_KP_7)) { sx -= 1; sy += 1; }
            if (K(GLFW_KEY_KP_3)) { sx += 1; sy -= 1; }
            if (K(GLFW_KEY_KP_1)) { sx -= 1; sy -= 1; }
            if ((sx != 0 || sy != 0) && !showMap) {
                float yr = yaw * PI_F / 180;
                vec2 up(std::sin(yr), std::cos(yr)), right(std::cos(yr), -std::sin(yr));
                vec2 v = right * sx + up * sy;
                float a = std::atan2(v.y, v.x);
                int k = (int)std::lround(a / (PI_F / 4));
                int dx = (int)std::lround(std::cos(k * PI_F / 4)), dy = (int)std::lround(std::sin(k * PI_F / 4));
                bool run = K(GLFW_KEY_LEFT_SHIFT) || K(GLFW_KEY_RIGHT_SHIFT);
                tryStep(dx, dy, run);
            }
        }
        if (now > messageUntil) message.clear();

        // view easing
        float e = std::min(1.0f, dt * 8);
        yaw += (yawT - yaw) * e;
        pitch += (pitchT - pitch) * e;
        zoom += (zoomT - zoom) * e;
        vec2 p = playerPos();
        vec3 tgt(p.x, p.y, playerZ(p));
        camTarget = camTarget + (tgt - camTarget) * std::min(1.0f, dt * 10);
    }

    // ---------------------------------------------------------- sun and sky
    // the sun's elevation and azimuth in degrees at the clock's hour (the same every day of the year)
    void sunAt(double mins, float &el, float &az) const {
        double h = std::fmod(mins, 1440.0) / 60.0;
        const float rise = 4.9f, set = 20.1f;
        float frac = (float)((h - rise) / (set - rise));
        if (frac >= 0 && frac <= 1) {
            el = std::sin(PI_F * frac) * 64.0f;
            az = 50.0f + 260.0f * frac;
        } else {
            // below the horizon: carry on smoothly so dusk fades
            float nf = frac < 0 ? frac : frac - 1;
            el = -std::min(18.0f, std::fabs(nf) * 64.0f * 3);
            az = frac < 0 ? 50.0f : 310.0f;
        }
    }
    // dark as the sky is dark: no daylight left at all (about 20:35 to 4:25)
    bool darkNow() const { float el, az; sunAt(minutes, el, az); return el <= -6.0f; }
    void sky(vec3 &sunDir, vec3 &sunCol, vec3 &amb, vec3 &skyc, float &torch) {
        float el, az;
        sunAt(minutes, el, az);
        float day = smoothstep(-6.0f, 10.0f, el);
        float low = 1.0f - smoothstep(4.0f, 25.0f, el);
        float azr = az * PI_F / 180, elr = std::max(el, 6.0f) * PI_F / 180;
        vec3 sd(std::sin(azr) * std::cos(elr), std::cos(azr) * std::cos(elr), std::sin(elr));
        vec3 md = normalize(vec3(-0.3f, -0.6f, 0.75f));
        sunDir = normalize(lerp(md, sd, day));
        vec3 noon(0.93f, 0.89f, 0.81f), dusk(1.0f, 0.62f, 0.36f), moon(0.16f, 0.2f, 0.32f);
        vec3 sc = lerp(noon, dusk, low) * 1.05f;
        sunCol = lerp(moon, sc, day);
        vec3 aDay(0.37f, 0.40f, 0.47f), aDusk(0.33f, 0.28f, 0.34f), aNight(0.07f, 0.09f, 0.16f);
        amb = lerp(aNight, lerp(aDay, aDusk, low), day);
        skyc = lerp(vec3(0.03f, 0.04f, 0.08f), lerp(vec3(0.55f, 0.68f, 0.82f), vec3(0.75f, 0.55f, 0.45f), low), day);
        torch = (1.0f - day) * 1.4f;
    }

    // ---------------------------------------------------------- drawing
    // close up, furniture is drawn finer (mesh.cc featureFine): a block of ground built at the other
    // detail is built again (the one round you first, a few a frame)
    bool detailFine = false, detailSet = false;   // detailSet: after the first frame
    bool midFigures = false;   // people and animals at the middle level (mesh.cc setMidFigures)
    std::unordered_set<int64_t> fineBuilt;   // blocks of ground built with the finer furniture
    bool chunkWanted(int cx, int cy) const {
        int64_t k = World::key(cx, cy);
        if (!gfx.count(k) || (fineBuilt.count(k) > 0) != detailFine) return true;
        // a tile edited since the mesh was built (World::setFeature: a camp pitched or struck in
        // view)
        auto it = world.chunks.find(k);
        return it != world.chunks.end() && it->second && it->second->dirty;
    }
    void ensureChunk(int cx, int cy) {
        int64_t k = World::key(cx, cy);
        if (!chunkWanted(cx, cy)) return;
        Chunk *c = world.chunk(cx, cy);
        MeshBuf s, w;
        setFineFeatures(detailFine);
        buildChunkMesh(world, *c, s, w);
        setFineFeatures(false);
        ChunkGfx &g = gfx[k];
        g.solid.upload(s);
        g.water.upload(w);
        if (detailFine) fineBuilt.insert(k); else fineBuilt.erase(k);
        c->dirty = false;
        gfxBuilt++;
    }

    // The meshes of chunks out of view are dropped, oldest first, when more than kGfxKeep of them are held
    // (about 1 MB each on the card); the Chunk's tiles (which hold the edits) stay, and a dropped mesh is
    // built again by ensureChunk when the chunk comes back into view.
    uint64_t gfxFrame = 0, gfxBuilt = 0;   // gfxBuilt: meshes built so far (DRAW says it)
    static constexpr size_t kGfxKeep = 64;
    void trimGfx(int cx0, int cx1, int cy0, int cy1) {
        gfxFrame++;
        size_t inView = 0;
        for (int cy = cy0; cy <= cy1; cy++)
            for (int cx = cx0; cx <= cx1; cx++) {
                auto it = gfx.find(World::key(cx, cy));
                if (it != gfx.end()) { it->second.used = gfxFrame; inView++; }
            }
        if (gfx.size() <= inView + kGfxKeep) return;
        std::vector<std::pair<uint64_t, int64_t>> out;
        for (auto &e : gfx) if (e.second.used != gfxFrame) out.push_back({e.second.used, e.first});
        std::sort(out.begin(), out.end());
        size_t drop = gfx.size() - inView - kGfxKeep;
        for (size_t i = 0; i < drop && i < out.size(); i++) {
            auto it = gfx.find(out[i].second);
            it->second.solid.release();
            it->second.water.release();
            gfx.erase(it);
            fineBuilt.erase(out[i].second);
        }
    }

    void buildTowns() {
        townMesh.resize(world.settlements.size());
        std::vector<MeshBuf> bufs(world.settlements.size());
        for (size_t i = 0; i < world.buildings.size(); i++) {
            const Building &b = world.buildings[i];
            for (size_t t = 0; t < world.settlements.size(); t++)
                if (world.settlements[t].inside(b.x0, b.y0) || world.settlements[t].inside(b.x1, b.y1)) {
                    buildPartsMesh(b, (int)i + 1, bufs[t]);
                    break;
                }
        }
        for (size_t t = 0; t < bufs.size(); t++) townMesh[t].upload(bufs[t]);
    }

    void render(int W, int H, float uiScale, bool synchronous) {
        // close up, people and animals are drawn finer (mesh.cc buildFigureFine), and the furniture too: zoom is
        // the half-height of the view in tiles, 20 by default; the still scenes are 1.5 to 6. Finer below 7, plain
        // again above 8, so that zooming about the line does not rebuild the furniture each time; the first frame
        // goes by 7.5. Out to 13, people and animals are drawn at the middle level; beyond, the plain figures.
        bool coarse = std::getenv("GALILEO_COARSE") != nullptr;   // GALILEO_COARSE: tests, the old figures and furniture
        if (coarse) detailFine = false;
        else if (!detailSet) detailFine = zoom < 7.5f;
        else if (zoom < 7.0f) detailFine = true;
        else if (zoom > 8.0f) detailFine = false;
        detailSet = true;
        midFigures = !coarse && !detailFine && zoom < 13.0f;
        setFineFigures(detailFine);
        float aspect = (float)W / H;
        viewAspect = aspect;
        float yr = yaw * PI_F / 180, pr = pitch * PI_F / 180;
        vec3 e(-std::sin(yr) * std::cos(pr), -std::cos(yr) * std::cos(pr), std::sin(pr));   // towards the camera
        FrameParams f;
        f.viewProj = viewProjFor(aspect);
        float torch;
        sky(f.sunDir, f.sunCol, f.ambient, f.sky, torch);
        f.time = (float)now;
        bool under = tz < 0;
        if (creating) {   // choosing who you are: not yet in the world, only you, on a plain ground before a plain backdrop
            f.sky = vec3(0.17f, 0.13f, 0.1f);
            f.sunDir = normalize(vec3(-0.35f, -0.55f, 0.75f));
            f.sunCol = vec3(1.0f, 0.94f, 0.84f);
            f.ambient = vec3(0.36f, 0.33f, 0.3f);
            torch = 0;
        }
        if (portrait) {   // a painter's light: high, from the upper left of the picture, on a dark ground (paintPortrait)
            f.sky = vec3(0.12f, 0.09f, 0.06f);
            vec3 rgt(std::cos(yr), -std::sin(yr), 0.0f);   // the picture's right, on the ground
            f.sunDir = normalize(e * 0.55f - rgt * 0.75f + vec3(0, 0, 0.9f));
            f.sunCol = vec3(1.05f, 0.95f, 0.8f);
            f.ambient = vec3(0.22f, 0.19f, 0.17f);
            torch = 0;
        }
        if (under) {
            // underground: no sun, no sky; only the lantern
            f.sunCol = vec3(0, 0, 0);
            f.ambient = vec3(0.02f, 0.02f, 0.025f);
            f.sky = vec3(0, 0, 0);
            torch = 2.0f;
            f.shadows = false;
        }

        // which chunks are in view
        vec3 fdir = -e;
        vec3 right = normalize(cross(fdir, vec3(0, 0, 1)));
        vec3 upv = cross(right, fdir);
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2) {
                vec3 P = camTarget + right * (sx * zoom * aspect) + upv * (sy * zoom);
                for (float zc : {-1.0f, 14.0f}) {
                    float t = (zc - P.z) / fdir.z;
                    vec3 Q = P + fdir * t;
                    minx = std::min(minx, Q.x); maxx = std::max(maxx, Q.x);
                    miny = std::min(miny, Q.y); maxy = std::max(maxy, Q.y);
                }
            }
        int cx0 = (int)std::floor((minx - 3) / CH), cx1 = (int)std::floor((maxx + 3) / CH);
        int cy0 = (int)std::floor((miny - 3) / CH), cy1 = (int)std::floor((maxy + 3) / CH);
        // build missing chunks, nearest first
        std::vector<std::pair<float, std::pair<int, int>>> need;
        for (int cy = cy0; cy <= cy1; cy++)
            for (int cx = cx0; cx <= cx1; cx++)
                if (chunkWanted(cx, cy)) {
                    float d = length(vec2(cx * CH + CH * 0.5f - camTarget.x, cy * CH + CH * 0.5f - camTarget.y));
                    need.push_back({d, {cx, cy}});
                }
        std::sort(need.begin(), need.end(), [](auto &a, auto &b) { return a.first < b.first; });
        int budget = synchronous ? 100000 : 3;
        for (auto &n : need) {
            if (budget-- <= 0) break;
            ensureChunk(n.second.first, n.second.second);
        }
        trimGfx(cx0, cx1, cy0, cy1);

        // light
        vec3 L = f.sunDir;
        float r = zoom * aspect * 1.3f + 10;
        vec3 lup = std::fabs(L.z) > 0.98f ? vec3(0, 1, 0) : vec3(0, 0, 1);
        mat4 lview = lookAt(camTarget + L * 250.0f, camTarget, lup);
        mat4 lproj = ortho(-r, r, -r, r, 1, 600);
        f.lightViewProj = lproj * lview;

        // player
        vec2 p = playerPos();
        float pz = playerZ(p);
        MeshBuf fig;
        setMidFigures(midFigures);   // for you, your horse and the people and animals about; not for statues
        float phase = stepT < 1 ? walkPhase : 0;
        vec3 figAt(p.x, p.y, pz);
        if (boat >= 0) {   // in a boat: it rocks a little; in a gondola you stand at the stern
            const ItemKind &k = items.kinds[boat];
            float bob = 0.02f * std::sin((float)now * 1.7f);
            drawBoat(fig, k, vec3(p.x, p.y, bob), facing);
            float back = k.shape == "gondola" ? -1.1f : k.shape == "ship" ? -1.55f : -0.1f;
            float up = k.shape == "gondola" ? 0.24f : k.shape == "ship" ? 1.35f : 0.12f;   // on a ship: at the helm, on the poop
            figAt = vec3(p.x - std::sin(facing) * back, p.y + std::cos(facing) * back, bob + up);
            phase = 0;
        }
        if (riding >= 0) {   // on horseback: the horse under you, you in the saddle (neither cut away by the
            const Animal &h = animals.list[riding];   // see-through circle, which takes what is above your head)
            AnimalLook hl = animals.kinds[h.kind].body;
            hl.col = h.col; hl.saddle = true;
            fig.flags = 2;
            buildAnimal(fig, hl, figAt, facing, stepT < 1 ? walkPhase * 2 : 0, 0, false);
            figAt.z += hl.ht - 0.4f;
            phase = 0;
        }
        if (sceneNow >= 0) {   // a still scene: its figures, and not you as you are now
            for (auto &sf : scenes[sceneNow].figs) drawSceneFig(fig, sf);
        } else {
            FigureLook me = playerLook();   // who you chose to be (the old colours for an older game)
            if (fightWith >= 0 && bestWeapon() > 2) me.weapon = 3;
            if (myStrike > 0) me.strike = 1.0f - myStrike / 0.35f;
            buildFigure(fig, figAt, facing, phase, me, false);
            if (creating) meshTube(fig, translate(figAt), -0.08f, 0.0f, 0.8f, 0.8f, 0.8f, 0.8f, 40, vec3(0.36f, 0.29f, 0.22f));   // the ground you stand on
            fig.flags = 0;
        }
        // the people about
        std::vector<HudLabel> labels;
        std::vector<HudPuff> puffs;
        {
            MeshBuf cm;
            cm.flags = 2;   // people and animals are not cut away by the see-through circle (nor is your horse)
            int playerBld = tz == 0 ? world.tile(tx, ty).bld : 0;
            for (auto &pp : people.people) {
                if (sceneNow >= 0) break;   // a still scene has only its own figures
                if (pp.onRoad || pp.z != tz || !pp.active) continue;
                if (pp.away || (pp.dead && minutes - pp.deadAt > 120)) continue;   // not here; the dead are carried away after two hours
                if (pp.x < minx - 4 || pp.x > maxx + 4 || pp.y < miny - 4 || pp.y > maxy + 4) continue;
                float t = smoothstep(0, 1, pp.stepT);
                vec2 q = vec2(pp.px + 0.5f, pp.py + 0.5f) + (vec2(pp.x + 0.5f, pp.y + 0.5f) - vec2(pp.px + 0.5f, pp.py + 0.5f)) * t;
                float qz = world.floorAt(q.x, q.y, tz);
                if (pp.lying) qz = world.floorAt(pp.x + 0.5f, pp.y + 0.5f, tz) + 0.05f;
                FigureLook lk = pp.look;
                if (pp.mem.count("family") && chosen()) {   // your father and mother: your skin, and your hair going grey
                    lk.skin = skinCol(meNum("me:skin", 1, kSkins));
                    lk.hair = hairCol(meNum("me:hair", 2, kHairs)) * 0.75f + vec3(0.55f, 0.53f, 0.5f) * 0.25f;
                }
                if (pp.strikeAnim > 0) lk.strike = 1.0f - pp.strikeAnim / 0.35f;   // a blow
                if (pp.dead) lk.weapon = 0;
                buildFigure(cm, vec3(q.x, q.y, qz), pp.lying && !pp.dead ? 0.0f : pp.facing, pp.stepT < 1 ? pp.walkPhase : 0, lk, pp.lying);   // in bed: head on the pillow
                // a name over the head, for those near and in sight
                int pb = tz == 0 ? world.tile(pp.x, pp.y).bld : 0;
                bool seen = pb == 0 || pb == playerBld || roofsOff || tz != 0;
                if (!seen || std::abs(pp.x - tx) + std::abs(pp.y - ty) > 9) continue;
                vec4 c = f.viewProj * vec4(q.x, q.y, qz + (pp.lying ? 0.6f : 1.35f), 1);
                float sx2 = (c.x / c.w * 0.5f + 0.5f) * W, sy2 = (0.5f - c.y / c.w * 0.5f) * H;
                labels.push_back({sx2, sy2, people.label(pp) + (pp.dead ? " (dead)" : pp.lying ? " (asleep)" : pp.hp < pp.maxHp * 0.35f ? " (badly wounded)" : pp.hp < pp.maxHp * 0.8f ? " (wounded)" : ""), false});
                if (pp.bubbleT > 0 && !pp.bubble.empty()) labels.push_back({sx2, sy2 - 18 * uiScale, pp.bubble, true});
            }
            animals.draw(cm, world, (int)minx, (int)miny, (int)maxx, (int)maxy);
            // gun smoke (a volley: People::gunSmoke): a puff rises, spreads and thins over three game minutes
            for (size_t k = 0; k < smokes.size();) {
                const Smoke &sm = smokes[k];
                double age = minutes - sm.born;
                const double L = 3.0;
                if (age > L || age < -0.01) { smokes.erase(smokes.begin() + k); continue; }
                k++;
                if (sceneNow >= 0 || tz != 0 || std::abs(sm.x - tx) + std::abs(sm.y - ty) > 60) continue;
                float t = (float)(age / L);
                float gz = world.floorAt(sm.x, sm.y, 0) + 2.2f + 2.5f * t;
                vec4 c = f.viewProj * vec4(sm.x + 1.2f * t, sm.y + 0.6f * t, gz, 1);
                vec4 c2 = f.viewProj * vec4(sm.x + 1.2f * t + 1.0f, sm.y + 0.6f * t, gz, 1);
                if (c.w <= 0 || c2.w <= 0) continue;
                float sx = (c.x / c.w * 0.5f + 0.5f) * W, sy = (0.5f - c.y / c.w * 0.5f) * H;
                float sx2 = (c2.x / c2.w * 0.5f + 0.5f) * W, sy2 = (0.5f - c2.y / c2.w * 0.5f) * H;
                float perTile = std::hypot(sx2 - sx, sy2 - sy);
                float a = 0.95f * std::pow(1.0f - t, 1.3f) * (0.6f + 0.4f * std::min(1.0f, (float)age / 0.03f));
                puffs.push_back({sx, sy, perTile * (1.6f + 2.8f * t), a, sm.seed});
            }
            if (testMode && std::getenv("GALILEO_ZOO")) {   // tests: one of every kind of animal in a row east of you
                vec2 pp = playerPos();
                int k0 = std::atoi(std::getenv("GALILEO_ZOO"));   // from this kind on (animals.txt order)
                static const vec3 tint[] = {{0.45f, 0.3f, 0.2f}, {0.77f, 0.6f, 0.42f}, {0.92f, 0.9f, 0.84f}, {0.55f, 0.45f, 0.35f}, {0.88f, 0.66f, 0.6f},
                                            {0.42f, 0.26f, 0.15f}, {0.35f, 0.28f, 0.2f}, {0.6f, 0.45f, 0.3f}, {0.95f, 0.95f, 0.92f}, {0.65f, 0.38f, 0.18f}};
                for (size_t k = (size_t)std::max(0, k0), j = 0; k < animals.kinds.size() && j < 4; k++, j++) {
                    float ax = pp.x + 1.2f + 1.3f * (float)j, ay = pp.y + 0.5f;
                    AnimalLook lk = animals.kinds[k].body;
                    if (k < 10 && lk.col.x == 0.5f && lk.col.y == 0.35f) lk.col = tint[k];
                    buildAnimal(cm, lk, vec3(ax, ay, world.groundZ(ax, ay)), 3.14159265f - yaw * 3.14159265f / 180.0f + 1.2f, 0.25f, 0, false);
                }
            }
            crowd.upload(cm);
        }
        setMidFigures(false);
        // choosing with the mouse: name what is under it
        if (pick && mouseX >= 0) {
            if (pick == 3 || pick == 5) {
                std::string what = lookLabel((float)mouseX, (float)mouseY);
                if (!what.empty()) {
                    double cx = mouseX * W, cy = mouseY * H - 22 * uiScale;
                    labels.push_back({(float)cx, (float)cy, what, false});
                }
            } else if (pick == 1) {
                bool reach = false;
                int i = thingUnder((float)mouseX, (float)mouseY, reach);
                if (i >= 0) {
                    float sx, sy;
                    vec3 q = thingPoint(items.things[i]);
                    screenOf(f.viewProj, q + vec3(0, 0, 0.25f), sx, sy);
                    const Thing &t = items.things[i];
                    labels.push_back({sx * W, sy * H, items.nameOf(t.kind, t.count) + (reach ? "" : " (out of reach)"), false});
                }
            } else {
                int X, Y;
                if (spotUnder((float)mouseX, (float)mouseY, X, Y)) {
                    const Tile *t = world.tileAt(X, Y, tz);
                    float sx, sy;
                    screenOf(f.viewProj, vec3(X + 0.5f, Y + 0.5f, world.floorAt(X + 0.5f, Y + 0.5f, tz) + Items::surface(t ? t->feature : 0) + 0.3f), sx, sy);
                    std::string what = !canPut(X, Y) ? "not here" : t && Items::surface(t->feature) > 0 ? "on " + featureName(t->feature) : X == tx && Y == ty ? "at your feet" : "here";
                    labels.push_back({sx * W, sy * H, what, false});
                }
            }
        }
        figure.upload(fig);
        if (items.version != thingsVersion || tz != thingsZ || std::abs(tx - thingsX) > 24 || std::abs(ty - thingsY) > 24 || thingsFine != detailFine) {
            thingsFine = detailFine;
            MeshBuf tm;
            items.buildMesh(world, tm, tx - 90, ty - 90, tx + 90, ty + 90, tz);
            thingsMesh.upload(tm);
            thingsVersion = items.version; thingsZ = tz; thingsX = tx; thingsY = ty;
        }
        vec4 pc = f.viewProj * vec4(p.x, p.y, pz + 0.55f, 1);
        f.playerNdc = vec2(pc.x / pc.w, pc.y / pc.w);
        f.playerDepth = pc.z / pc.w;
        f.playerZ = pz;
        f.cutRadius = sceneNow >= 0 ? 0.0f : 2.7f / zoom;   // no see-through circle in a still scene
        f.aspect = aspect;
        f.torch = vec3(p.x, p.y, pz + 1.3f);
        f.torchStrength = torch;
        f.torchRadius = under ? 10.0f : 7.0f;
        f.night = std::min(1.0f, torch / 1.4f * 1.3f);
        f.hideTag = roofsOff ? -2.0f : (inside ? (float)inside : -1.0f);
        // floors to draw: underground, every tunnel and cellar at this depth in view; upstairs, the floor you are on
        std::vector<int> showLevels;
        Level *here = world.levelAt(tx, ty, tz);
        if (tz > 0) {
            if (here) showLevels.push_back((int)(here - world.levels.data()));
            f.hideTag = roofsOff ? -2.0f : ((here && here->hideBuilding) ? (float)here->bld : -1.0f);
            // on a tower top or a roof, the roofs lifted (V) must not take away what you stand on
            if (here && !here->hideBuilding && here->bld > 0) f.keepTag = (float)here->bld;
        } else if (under) {
            for (size_t i = 0; i < world.levels.size(); i++) {
                const Level &l = world.levels[i];
                if (l.z != tz || l.x0 > maxx + 4 || l.x0 + l.w < minx - 4 || l.y0 > maxy + 4 || l.y0 + l.h < miny - 4) continue;
                showLevels.push_back((int)i);
            }
        }
        if (levelGfx.size() < world.levels.size()) { levelGfx.resize(world.levels.size()); levelBuilt.resize(world.levels.size(), 0); }
        for (int i : showLevels)
            if (levelBuilt[i] != (detailFine ? 2 : 1)) {   // 1: built plain, 2: with the finer furniture
                MeshBuf mb;
                setFineFeatures(detailFine);
                buildLevelMesh(world, world.levels[i], mb);
                setFineFeatures(false);
                levelGfx[i].upload(mb);
                levelBuilt[i] = detailFine ? 2 : 1;
            }

        auto drawAll = [&](bool water) {
            if (creating || portrait) { if (!water) figure.draw(); return; }   // the choosing screen, a portrait: you, and nothing of the world
            if (under) {
                if (!water) {
                    for (int i : showLevels) levelGfx[i].draw();
                    figure.draw();
                    crowd.draw();
                    thingsMesh.draw();
                }
                return;
            }
            if (!water) for (int i : showLevels) levelGfx[i].draw();
            for (int cy = cy0; cy <= cy1; cy++)
                for (int cx = cx0; cx <= cx1; cx++) {
                    auto it = gfx.find(World::key(cx, cy));
                    if (it == gfx.end()) continue;
                    if (water) it->second.water.draw();
                    else it->second.solid.draw();
                }
            if (!water) {
                for (size_t t = 0; t < townMesh.size(); t++) {
                    const Settlement &s = world.settlements[t];
                    if (s.x0 > maxx + 10 || s.x0 + s.w < minx - 10 || s.y0 > maxy + 10 || s.y0 + s.h < miny - 10) continue;
                    townMesh[t].draw();
                }
                figure.draw();
                crowd.draw();
                thingsMesh.draw();
            }
        };

        GLint target;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target);
        R.shadowPassBegin(f);
        if (!under) drawAll(false);
        R.shadowPassEnd();
        glBindFramebuffer(GL_FRAMEBUFFER, target);
        R.mainPassBegin(f, W, H);
        drawAll(false);
        drawAll(true);

        if (portrait) return;   // a picture of you alone: no panels, no labels
        // overlay
        HudState hs;
        hs.place = place;
        hs.date = dateString(minutes);
        hs.health = hpNow() / maxHp();
        hs.showHealth = hs.health < 0.999f || fightWith >= 0;
        {
            char buf[128];
            std::snprintf(buf, sizeof buf, "tile %d, %d   (old map %.1f, %.1f)%s", tx, ty, tx / (float)world.S, ty / (float)world.S,
                          gallop ? "   galloping" : "");
            hs.coords = buf;
        }
        hs.message = message;
        hs.labels = labels;
        hs.puffs = puffs;
        if (creating) { hs.creating = true; hs.createRow = createRow; hs.createRows = createRows(); hs.labels.clear(); }
        if (cardUp) { hs.cardLines = cardLines; hs.labels.clear(); }
        hs.fade = fadeAlpha();
        if (sceneNow >= 0) { hs.caption = scenes[sceneNow].caption; hs.sceneNo = sceneNow + 1; hs.sceneCount = (int)scenes.size(); hs.sceneOpening = !seqMode; hs.labels.clear(); }
        if (talking && people.talk.who >= 0) {
            const Person &pp = people.people[people.talk.who];
            hs.talking = true;
            std::string l = people.label(pp);
            hs.talkTitle = l == pp.name ? pp.name + ", " + pp.title : (std::string(1, (char)std::toupper((unsigned char)l[0])) + l.substr(1));
            hs.talkLines = people.talk.lines;
            hs.talkInput = people.talk.over ? "(press Return)" : people.talk.input;
        }
        hs.help = help;
        hs.testing = opt.testing;
        hs.map = showMap;
        if (showMap)   // the towns you have been to: the map names only these
            for (auto it = people.player.lower_bound("been:"); it != people.player.end() && it->first.compare(0, 5, "been:") == 0; ++it)
                if (!it->second.empty() && it->second != "0") hs.beenTowns.insert(it->first.substr(5));
        hs.journal = journal && !panel;
        if (panel == 2) { hs.panel = 2; hs.panelTitle = readTitle; hs.panelText = readText; hs.panelImage = readImage; hs.panelCredit = readCredit; }
        else if (panel == 3 && items.books.count(bookId)) {
            const Book &b = items.books.at(bookId);
            bool u = understands(b);
            hs.panel = 3;
            hs.bookTitle = u ? b.titleEn : b.title;
            hs.bookFirst = bookFirst;
            hs.bookPages = (int)b.pages.size();
            auto side = [&](int p, std::string &text, std::string &img, std::string &cred) {
                if (p >= (int)b.pages.size()) return;
                text = u ? b.pages[p].english : b.pages[p].orig;
                img = b.pages[p].image;
                cred = b.pages[p].credit;
            };
            side(bookFirst, hs.bookLeft, hs.bookLeftImage, hs.bookLeftCredit);
            side(bookFirst + 1, hs.bookRight, hs.bookRightImage, hs.bookRightCredit);
            std::string lang = b.language;
            if (!lang.empty()) lang[0] = (char)std::toupper((unsigned char)lang[0]);
            hs.bookNote = b.language == "english" ? "" : u ? "You read it in " + lang + "." : "It is in " + lang + ", which you cannot read.";
            if (!bookLearnt.empty()) {   // a book that has just taught you something
                static const std::map<std::string, std::string> nice = {{"latin", "to read Latin"}, {"italian", "to read Tuscan"},
                    {"arabic", "to read Arabic"}, {"optics", "how lenses bend the light"}};
                auto it = nice.find(bookLearnt);
                std::string what = it != nice.end() ? it->second : bookLearnt;
                hs.bookNote = (hs.bookNote.empty() ? "" : hs.bookNote + " ") + "From this book you have learnt " + what + ".";
            }
        }
        else if (panel == 1) {
            hs.panel = 1;
            hs.panelTitle = useMode ? "Use what?" : "What you carry";
            auto c = carried();
            int n = 0;
            for (auto &it : c) {
                n++;
                std::string line = (n <= 9 ? std::to_string(n) : n == 10 ? std::string("0") : std::string(" ")) + ".  " + items.nameOf(it.first, it.second);
                if (n - 1 == invPick) line = "*" + line + "*";
                hs.panelLines.push_back(line);
            }
            if (c.empty()) hs.panelLines.push_back("Nothing.");
            char wbuf[96];
            std::snprintf(wbuf, sizeof wbuf, "Money: %s.   Weight: %g of %g pounds.", moneyString(money()).c_str(), carriedWeight(), kCarry);
            if (money() > 0 && purseString().rfind("all in ", 0) != 0) hs.panelLines.push_back("Your purse: " + purseString() + ".");
            {   // what you have learnt
                std::string sk;
                for (auto &m : people.player)
                    if (m.first.rfind("skill:", 0) == 0 && std::atol(m.second.c_str()) > 0) {
                        long lv = std::atol(m.second.c_str());
                        sk += (sk.empty() ? "" : ", ") + m.first.substr(6) + (lv > 1 ? " (" + std::to_string(lv) + ")" : "");
                    }
                if (!sk.empty()) hs.panelLines.push_back("You know: " + sk + ".");
            }
            if (!wearing().empty()) hs.panelLines.push_back("You wear " + wearing() + ".");
            hs.panelLines.push_back("");
            hs.panelLines.push_back(wbuf);
            {
                std::string purse = purseString();
                if (!purse.empty()) hs.panelLines.push_back("Your purse: " + purse + ".");
            }
            if (invPick >= 0 && invPick < (int)c.size()) hs.panelText = items.kinds[c[invPick].first].look;
            hs.panelHint = c.empty() ? "I or Esc to close" : useMode ? "a number uses that thing; Esc to close"
                                                                   : "a number chooses; U uses it; D puts it down (then click where); I or Esc to close";
        }
        hs.journalTab = journalTab;
        hs.journalDetailed = journalDetailed();
        hs.journalPage = journalPage;
        if (journal && journalTab == 0) {
            people.noteJournal(minutes);
            hs.storyLines = people.journal(minutes, hs.journalDetailed);
            std::vector<TownPlanInfo> plans;
            for (auto &l : people.townPages(world, minutes, curTown, &plans)) hs.storyLines.push_back(l);   // at the back: places and people
            hs.townPlans.clear();
            for (auto &p : plans) {
                HudState::TownPlan tp;
                tp.town = p.town; tp.x0 = p.x0; tp.y0 = p.y0; tp.w = p.w; tp.h = p.h; tp.marks = p.marks;
                if (p.town == curTown) { tp.youX = tx + 0.5f; tp.youY = ty + 0.5f; }
                hs.townPlans.push_back(tp);
                if (!hud.hasPlan(p.town)) hud.setPlan(p.town, p.w, p.h, planPixels(p));   // made once a session
            }
        }
        if (journal && journalTab == 1) {
            int conv = -1;
            for (const HeardLine &h : people.heard) {
                if (h.conv != conv) { conv = h.conv; hs.journalLines.push_back({true, dateString(h.minutes) + ", " + h.where}); }
                hs.journalLines.push_back({false, h.who + ": " + h.text});
            }
        }
        hs.px = p.x; hs.py = p.y;
        bool changed = hud.draw(hs, world, W, H, uiScale, now);
        R.overlay(hud.data(), W, H, hud.stride(), changed);
    }

    // ---- your portrait (Marguerite Bahuche's thanks): your own figure as you chose
    // to be, the yellow hat and the coat, alone and close before a dark ground, head and shoulders,
    // turned a little aside, rendered offscreen; then painted over (paintOver) and written to OUT
    bool glReady = false;   // a picture can be drawn (the window or the picture's context is made)
    std::function<bool()> openGL;   // text-only runs (--walk without --shot): makes the hidden context when a picture is first needed
    std::string portraitPath() const {   // beside the saved game: "saves/portrait-Jeanne.png"
        std::string dir = savePath.find('/') == std::string::npos ? "." : savePath.substr(0, savePath.rfind('/'));
        std::string n;
        auto it = people.player.find("me:name");
        for (char c : it == people.player.end() ? std::string("you") : it->second) if (std::isalnum((unsigned char)c)) n += c;
        return dir + "/portrait-" + (n.empty() ? std::string("you") : n) + ".png";
    }
    void paintPendingPortrait() {
        if (!people.player.count("portrait:pending")) return;
        if (!glReady && !(openGL && openGL())) return;
        std::string out = people.player["portrait:file"];
        if (out.empty()) out = people.player["portrait:file"] = portraitPath();
        if (paintPortrait(out)) { people.player.erase("portrait:pending"); hud.forgetPicture(out); }
    }
    bool paintPortrait(const std::string &out) {
        const int W = 720, H = 900;
        GLint prevFbo = 0, prevView[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
        glGetIntegerv(GL_VIEWPORT, prevView);
        float kYaw = yaw, kYawT = yawT, kPitch = pitch, kPitchT = pitchT, kZoom = zoom, kZoomT = zoomT;
        float kFacing = facing, kFacingT = facingTarget, kStepT = stepT;
        vec3 kCam = camTarget;
        portrait = true;
        yaw = yawT = 22; pitch = pitchT = 6; zoom = zoomT = 0.36f;
        stepT = 1;
        vec2 p = playerPos();
        camTarget = vec3(p.x, p.y, playerZ(p) + 0.9f);
        facing = facingTarget = 3.14159265f - yaw * 3.14159265f / 180.0f + 0.42f;   // three-quarters, looking a little past the painter
        GLuint msFbo, msColor, msDepth, fbo, color;
        glGenFramebuffers(1, &msFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
        glGenRenderbuffers(1, &msColor);
        glBindRenderbuffer(GL_RENDERBUFFER, msColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, W, H);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor);
        glGenRenderbuffers(1, &msDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, msDepth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, W, H);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenRenderbuffers(1, &color);
        glBindRenderbuffer(GL_RENDERBUFFER, color);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, W, H);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
        glViewport(0, 0, W, H);
        render(W, H, 1.0f, true);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        glBlitFramebuffer(0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        std::vector<unsigned char> px((size_t)W * H * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
        glViewport(prevView[0], prevView[1], prevView[2], prevView[3]);
        glDeleteFramebuffers(1, &msFbo); glDeleteFramebuffers(1, &fbo);
        glDeleteRenderbuffers(1, &msColor); glDeleteRenderbuffers(1, &msDepth); glDeleteRenderbuffers(1, &color);
        portrait = false;
        yaw = kYaw; yawT = kYawT; pitch = kPitch; pitchT = kPitchT; zoom = kZoom; zoomT = kZoomT;
        facing = kFacing; facingTarget = kFacingT; stepT = kStepT; camTarget = kCam;
        // rows bottom-up from GL: turn them over, then paint
        std::vector<float> img((size_t)W * H * 3);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                for (int c = 0; c < 3; c++) img[((size_t)y * W + x) * 3 + c] = px[((size_t)(H - 1 - y) * W + x) * 4 + c] / 255.0f;
        paintOver(img, W, H, (uint32_t)std::hash<std::string>()(people.player["me:name"]));
        cairo_surface_t *out_s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
        unsigned char *d = cairo_image_surface_get_data(out_s);
        int st = cairo_image_surface_get_stride(out_s);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const float *q = &img[((size_t)y * W + x) * 3];
                unsigned char *o = d + (size_t)y * st + x * 4;
                auto b8 = [](float v) { return (unsigned char)std::lround(std::min(1.0f, std::max(0.0f, v)) * 255); };
                o[0] = b8(q[2]); o[1] = b8(q[1]); o[2] = b8(q[0]); o[3] = 255;
            }
        cairo_surface_mark_dirty(out_s);
        bool ok = cairo_surface_write_to_png(out_s, out.c_str()) == CAIRO_STATUS_SUCCESS;
        cairo_surface_destroy(out_s);
        return ok;
    }
    // a rendered picture made to look painted in oils: the ground laid in with broad strokes, the
    // forms in small flat touches (a Kuwahara filter), a warm old varnish, the light gathered on the
    // face (the edges darkened), the weave of the canvas, and a fine net of cracks
    static void paintOver(std::vector<float> &img, int W, int H, uint32_t seed) {
        auto at = [&](int x, int y) -> float * { x = std::max(0, std::min(W - 1, x)); y = std::max(0, std::min(H - 1, y)); return &img[((size_t)y * W + x) * 3]; };
        auto hashf = [&](int a, int b, uint32_t k) { uint32_t h = (uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u ^ (seed + k) * 83492791u; h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return (h & 0xffffff) / 16777215.0f; };
        auto vnoise = [&](float x, float y, uint32_t k) {   // smooth value noise
            int xi = (int)std::floor(x), yi = (int)std::floor(y);
            float fx = x - xi, fy = y - yi;
            fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
            float a = hashf(xi, yi, k), b = hashf(xi + 1, yi, k), c = hashf(xi, yi + 1, k), d = hashf(xi + 1, yi + 1, k);
            return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
        };
        // 1. the ground: wherever the backdrop shows, a dark warm ground, lighter towards the light (upper left), brushed
        const float bg[3] = {0.12f, 0.09f, 0.06f};
        std::vector<unsigned char> ground((size_t)W * H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                float *q = at(x, y);
                bool isBg = std::fabs(q[0] - bg[0]) < 0.035f && std::fabs(q[1] - bg[1]) < 0.035f && std::fabs(q[2] - bg[2]) < 0.035f;
                ground[(size_t)y * W + x] = isBg;
                if (!isBg) continue;
                float u = (float)x / W, v = (float)y / H;
                float light = 0.55f + 0.6f * std::max(0.0f, 1.0f - std::hypot(u - 0.3f, v - 0.3f) * 1.4f);
                float stroke = 0.75f + 0.5f * vnoise(x / 70.0f + y / 160.0f, y / 22.0f, 1) * vnoise(x / 9.0f, y / 40.0f, 2);
                float k = light * stroke;
                q[0] = 0.16f * k + 0.02f; q[1] = 0.12f * k + 0.015f; q[2] = 0.075f * k + 0.01f;
            }
        // 2. painted in strokes (after Hertzmann's "painterly rendering"): three passes, broad to fine;
        //    each lays short strokes of the colour of a blurred copy of the picture, turned across the
        //    slope of its light (so they follow the forms), and the finer passes paint only where the
        //    canvas still differs from the picture
        {
            auto blurred = [&](int rad) {   // three box blurs of radius rad: about a Gaussian
                std::vector<float> b = img, t((size_t)W * H * 3);
                for (int pass = 0; pass < 3; pass++) {
                    for (int y = 0; y < H; y++) {   // across
                        float acc[3] = {0, 0, 0};
                        for (int k = -rad; k <= rad; k++) { int xx = std::max(0, std::min(W - 1, k)); for (int c = 0; c < 3; c++) acc[c] += b[((size_t)y * W + xx) * 3 + c]; }
                        for (int x = 0; x < W; x++) {
                            for (int c = 0; c < 3; c++) t[((size_t)y * W + x) * 3 + c] = acc[c] / (2 * rad + 1);
                            int xo = std::max(0, std::min(W - 1, x - rad)), xi = std::max(0, std::min(W - 1, x + rad + 1));
                            for (int c = 0; c < 3; c++) acc[c] += b[((size_t)y * W + xi) * 3 + c] - b[((size_t)y * W + xo) * 3 + c];
                        }
                    }
                    for (int x = 0; x < W; x++) {   // and down
                        float acc[3] = {0, 0, 0};
                        for (int k = -rad; k <= rad; k++) { int yy = std::max(0, std::min(H - 1, k)); for (int c = 0; c < 3; c++) acc[c] += t[((size_t)yy * W + x) * 3 + c]; }
                        for (int y = 0; y < H; y++) {
                            for (int c = 0; c < 3; c++) b[((size_t)y * W + x) * 3 + c] = acc[c] / (2 * rad + 1);
                            int yo = std::max(0, std::min(H - 1, y - rad)), yi = std::max(0, std::min(H - 1, y + rad + 1));
                            for (int c = 0; c < 3; c++) acc[c] += t[((size_t)yi * W + x) * 3 + c] - t[((size_t)yo * W + x) * 3 + c];
                        }
                    }
                }
                return b;
            };
            std::vector<float> canvas((size_t)W * H * 3);
            for (size_t i = 0; i < canvas.size(); i += 3) { canvas[i] = 0.1f; canvas[i + 1] = 0.075f; canvas[i + 2] = 0.05f; }   // the brown ground of the canvas
            uint32_t rs = seed * 2654435761u + 12345u;
            auto rnd = [&]() { rs = rs * 1664525u + 1013904223u; return (rs >> 8) / 16777216.0f; };
            for (int R : {14, 7, 3}) {
                std::vector<float> ref = blurred(std::max(1, R / 3));
                auto rp = [&](int x, int y) -> const float * { x = std::max(0, std::min(W - 1, x)); y = std::max(0, std::min(H - 1, y)); return &ref[((size_t)y * W + x) * 3]; };
                auto lum = [&](int x, int y) { const float *q = rp(x, y); return 0.3f * q[0] + 0.59f * q[1] + 0.11f * q[2]; };
                int step = std::max(2, (R * 3) / 4);
                std::vector<std::pair<int, int>> pts;
                for (int y = 0; y < H; y += step)
                    for (int x = 0; x < W; x += step) pts.push_back({x + (int)(rnd() * step), y + (int)(rnd() * step)});
                for (size_t i = pts.size(); i > 1; i--) std::swap(pts[i - 1], pts[(size_t)(rnd() * i) % i]);   // in no order
                for (auto [x, y] : pts) {
                    if (x >= W || y >= H) continue;
                    if (R != 14) {   // only where the canvas is still far from the picture
                        float err = 0; int n = 0;
                        for (int yy = y - R / 2; yy <= y + R / 2; yy += 2)
                            for (int xx = x - R / 2; xx <= x + R / 2; xx += 2) {
                                if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
                                const float *a = &canvas[((size_t)yy * W + xx) * 3], *b = rp(xx, yy);
                                err += std::fabs(a[0] - b[0]) + std::fabs(a[1] - b[1]) + std::fabs(a[2] - b[2]); n++;
                            }
                        if (n == 0 || err / n < 0.07f) continue;
                    }
                    const float *col = rp(x, y);
                    float gx = lum(x + 1, y) - lum(x - 1, y), gy = lum(x, y + 1) - lum(x, y - 1);
                    float dx, dy;
                    if (std::hypot(gx, gy) < 0.004f) { dx = 0.45f; dy = 0.89f; }   // no slope: a plain slanting stroke
                    else { float gl = std::hypot(gx, gy); dx = -gy / gl; dy = gx / gl; }   // across the slope, along the form
                    float len = R * (1.1f + 0.9f * rnd()), wid = R * 0.5f;
                    float jit = 1.0f + 0.07f * (rnd() - 0.5f);
                    float c0 = col[0] * jit, c1 = col[1] * jit, c2 = col[2] * jit;
                    int bx0 = (int)(x - len - wid), bx1 = (int)(x + len + wid), by0 = (int)(y - len - wid), by1 = (int)(y + len + wid);
                    for (int yy = std::max(0, by0); yy <= std::min(H - 1, by1); yy++)
                        for (int xx = std::max(0, bx0); xx <= std::min(W - 1, bx1); xx++) {
                            float px = xx - x, py = yy - y;
                            float t = std::max(-len, std::min(len, px * dx + py * dy));   // along the stroke
                            float ex = px - t * dx, ey = py - t * dy, d = std::hypot(ex, ey) / wid;
                            if (d >= 1) continue;
                            float a = d < 0.65f ? 0.92f : 0.92f * (1 - d) / 0.35f;   // a soft edge
                            float *o = &canvas[((size_t)yy * W + xx) * 3];
                            o[0] += (c0 - o[0]) * a; o[1] += (c1 - o[1]) * a; o[2] += (c2 - o[2]) * a;
                        }
                }
            }
            img.swap(canvas);
        }
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                float *q = at(x, y);
                float l = 0.3f * q[0] + 0.59f * q[1] + 0.11f * q[2];
                for (int c = 0; c < 3; c++) q[c] = q[c] * 0.85f + l * 0.15f;   // a little less bright in colour, as old paint is
                q[0] *= 1.05f; q[1] *= 0.97f; q[2] *= 0.8f;                     // a warm, yellowed varnish
                for (int c = 0; c < 3; c++) { float v = q[c]; q[c] = v * v * (3 - 2 * v) * 0.35f + v * 0.65f; }   // deeper shadows, fuller lights
                float u = (x - W * 0.47f) / (W * 0.62f), v = (y - H * 0.4f) / (H * 0.66f);
                float vig = 1.0f - 0.6f * std::min(1.0f, std::max(0.0f, std::sqrt(u * u + v * v) - 0.45f) / 0.6f);
                float weave = 1.0f + 0.03f * std::sin(x * 2.1f) * std::sin(y * 2.1f) + 0.025f * (hashf(x, y, 3) - 0.5f);
                for (int c = 0; c < 3; c++) q[c] *= vig * weave;
            }
        // 4. craquelure: the borders of irregular cells, faintly dark, broken in places
        {
            const float cell = 34.0f;
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    int cx = (int)(x / cell), cy = (int)(y / cell);
                    float d1 = 1e9f, d2 = 1e9f;
                    for (int j = -1; j <= 1; j++)
                        for (int i = -1; i <= 1; i++) {
                            float px = (cx + i + hashf(cx + i, cy + j, 5)) * cell, py = (cy + j + hashf(cx + i, cy + j, 6)) * cell;
                            float d = std::hypot(px - x, py - y);
                            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
                        }
                    float edge = d2 - d1;
                    if (edge < 0.9f && vnoise(x / 23.0f, y / 23.0f, 7) > 0.45f) {
                        float *q = at(x, y);
                        float k = 0.88f + 0.09f * (edge / 0.9f);
                        for (int c = 0; c < 3; c++) q[c] *= k;
                    }
                }
        }
    }
    void savePng(const std::string &fn, int W, int H) {
        cairo_surface_t *img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
        unsigned char *d = cairo_image_surface_get_data(img);
        int st = cairo_image_surface_get_stride(img);
        std::vector<unsigned char> tmp((size_t)W * H * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, W, H, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, tmp.data());
        for (int y = 0; y < H; y++) {
            unsigned char *row = d + (size_t)(H - 1 - y) * st;
            std::memcpy(row, tmp.data() + (size_t)y * W * 4, (size_t)W * 4);
            for (int x = 0; x < W; x++) row[x * 4 + 3] = 255;
        }
        cairo_surface_mark_dirty(img);
        cairo_surface_write_to_png(img, fn.c_str());
        cairo_surface_destroy(img);
    }
};

static Game *G = nullptr;

static void endConversation(Game &g) {
    g.people.endTalk(g.minutes);
    g.talking = false;
}

static void onChar(GLFWwindow *, unsigned int cp) {
    Game &g = *G;
    if (g.creating) {   // your name, on the first line
        if (g.createRow != 0 || cp < 32) return;
        std::string &n = g.people.player["me:name"];
        if (n.size() >= 24) return;
        if (cp < 0x80) { if (std::isalpha((int)cp) || cp == ' ' || cp == '-' || cp == '\'') n += (char)cp; }
        else if (cp < 0x800) { n += (char)(0xC0 | (cp >> 6)); n += (char)(0x80 | (cp & 0x3F)); }
        return;
    }
    if (!g.talking || g.people.talk.over) return;
    if (g.swallowChar) { g.swallowChar = false; if (cp == 't' || cp == 'T') return; }
    if (cp < 32) return;
    std::string &in = g.people.talk.input;
    if (in.size() > 60) return;
    if (cp < 0x80) in += (char)cp;
    else if (cp < 0x800) { in += (char)(0xC0 | (cp >> 6)); in += (char)(0x80 | (cp & 0x3F)); }
    else { in += (char)(0xE0 | (cp >> 12)); in += (char)(0x80 | ((cp >> 6) & 0x3F)); in += (char)(0x80 | (cp & 0x3F)); }
}

static void onKey(GLFWwindow *w, int key, int, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    Game &g = *G;
    if (g.fadeT >= 0) return;   // asleep: the keys wait for the morning
    if (g.creating) {   // choosing who you are
        int rows = (int)g.createRows().size();
        if (key == GLFW_KEY_UP) g.createRow = (g.createRow + rows - 1) % rows;
        else if (key == GLFW_KEY_DOWN || key == GLFW_KEY_TAB) g.createRow = (g.createRow + 1) % rows;
        else if (key == GLFW_KEY_LEFT && g.createRow > 0) g.changeChoice(-1);
        else if ((key == GLFW_KEY_RIGHT || key == GLFW_KEY_SPACE) && g.createRow > 0) g.changeChoice(1);
        else if (key == GLFW_KEY_BACKSPACE && g.createRow == 0) {
            std::string &n = g.people.player["me:name"];
            while (!n.empty() && (n.back() & 0xC0) == 0x80) n.pop_back();
            if (!n.empty()) n.pop_back();
        } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) g.finishCreating();
        return;
    }
    if (g.cardUp) {   // a card: on
        if (key == GLFW_KEY_SPACE || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_ESCAPE) g.afterCard();
        return;
    }
    if (g.sceneNow >= 0) {   // the still scenes: on, or all skipped
        if (key == GLFW_KEY_ESCAPE) g.endScenes();
        else if (key == GLFW_KEY_SPACE || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_RIGHT) g.nextScene();
        else if (key == GLFW_KEY_LEFT) g.prevScene();
        return;
    }
    if (g.talking) {
        std::string &in = g.people.talk.input;
        if (key == GLFW_KEY_ESCAPE) endConversation(g);
        else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            if (g.people.talk.over) endConversation(g);
            else if (!in.empty()) { std::string q = in; in.clear(); g.minutes += 1; g.people.ask(g.world, q, g.minutes, g.tx, g.ty); }   // a minute a thing said (the clock stands while you talk)
        } else if (key == GLFW_KEY_BACKSPACE && !in.empty()) {
            while (!in.empty() && (in.back() & 0xC0) == 0x80) in.pop_back();
            if (!in.empty()) in.pop_back();
        }
        return;
    }
    if (g.panel == 3) {   // an open book: turn the pages, or close it
        if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_L || key == GLFW_KEY_ENTER || key == GLFW_KEY_U) { g.panel = 0; g.invPick = -1; g.useMode = false; return; }
        if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_PAGE_DOWN || key == GLFW_KEY_SPACE || key == GLFW_KEY_PERIOD) g.turnPages(1);
        if (key == GLFW_KEY_LEFT || key == GLFW_KEY_PAGE_UP || key == GLFW_KEY_BACKSPACE || key == GLFW_KEY_COMMA) g.turnPages(-1);
        return;
    }
    if (g.panel) {   // what you carry, or a work of art you are looking at
        if (key == GLFW_KEY_ESCAPE || (g.panel == 1 && key == GLFW_KEY_I) || (g.panel == 2 && (key == GLFW_KEY_L || key == GLFW_KEY_ENTER))) { g.panel = 0; g.invPick = -1; g.useMode = false; return; }
        if (g.panel == 1) {
            auto c = g.carried();
            int d = -1;
            if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) d = key - GLFW_KEY_1;
            if (key == GLFW_KEY_0) d = 9;
            if (d >= 0 && d < (int)c.size()) {
                g.invPick = d;
                if (g.useMode) { g.panel = 0; g.useMode = false; g.invPick = -1; g.useThing(c[d].first); return; }
            }
            if (key == GLFW_KEY_U && g.invPick >= 0 && g.invPick < (int)c.size()) {
                int kind = c[g.invPick].first;
                g.panel = 0; g.invPick = -1;
                g.useThing(kind);
                return;
            }
            if (key == GLFW_KEY_D && g.invPick >= 0 && g.invPick < (int)c.size()) {
                int kind = c[g.invPick].first;
                g.invPick = -1;
                g.startPick(2, kind);
            }
        }
        return;
    }
    if (g.pick) {   // waiting for a click: Esc gives up; the view can still be turned and zoomed
        if (key == GLFW_KEY_ESCAPE) { g.endPick(); g.say("Never mind.", 1.5); return; }
        if (key == GLFW_KEY_C && g.pick == 4) { g.endPick(); g.say("Never mind.", 1.5); return; }
        if (key == GLFW_KEY_U && g.pick == 5) {   // U twice: something you carry
            g.endPick();
            g.say("", 0);
            if (g.carried().empty()) { g.say("You carry nothing to use.", 2.0); return; }
            g.panel = 1; g.invPick = -1; g.useMode = true; g.journal = false;
            return;
        }
        if (key == GLFW_KEY_L && g.pick == 3) {   // L twice: what is in front of you, as before
            g.endPick();
            std::string s = g.lookAhead();
            if (!g.panel) g.say(s, 6.0);
            return;
        }
        if (key == GLFW_KEY_G || key == GLFW_KEY_I || key == GLFW_KEY_L || key == GLFW_KEY_T || key == GLFW_KEY_J ||
            key == GLFW_KEY_M || key == GLFW_KEY_N || key == GLFW_KEY_TAB) return;
    }
    switch (key) {
        case GLFW_KEY_ESCAPE:
            if (g.journal) g.journal = false;
            else if (g.showMap) g.showMap = false;
            else glfwSetWindowShouldClose(w, 1);
            break;
        case GLFW_KEY_Q: g.yawT -= 15; break;
        case GLFW_KEY_E: g.yawT += 15; break;
        case GLFW_KEY_R: g.pitchT = std::min(80.0f, g.pitchT + 5); break;
        case GLFW_KEY_F: g.pitchT = std::max(25.0f, g.pitchT - 5); break;
        case GLFW_KEY_EQUAL: case GLFW_KEY_KP_ADD: g.zoomT = std::max(5.0f, g.zoomT / 1.2f); break;
        case GLFW_KEY_MINUS: case GLFW_KEY_KP_SUBTRACT: g.zoomT = std::min(150.0f, g.zoomT * 1.2f); break;
        case GLFW_KEY_Z: g.yawT = 22; g.pitchT = 52; g.zoomT = 20; break;
        case GLFW_KEY_V: g.roofsOff = !g.roofsOff; g.say(g.roofsOff ? "Roofs lifted." : "Roofs back on."); break;
        case GLFW_KEY_RIGHT_BRACKET: if (g.opt.testing) g.minutes += 60; break;
        case GLFW_KEY_LEFT_BRACKET: if (g.opt.testing) { g.minutes -= 60; if (g.minutes < 0) g.minutes += 1440; } break;
        case GLFW_KEY_T: {
            int who = g.people.adjacentTo(g.tx, g.ty, g.tz, g.facing);
            if (who < 0) { g.say("There is no one beside you to talk to.", 2.0); break; }
            Person &pp = g.people.people[who];
            pp.facing = std::atan2(-(float)(g.tx - pp.x), (float)(g.ty - pp.y));
            g.people.startTalk(g.world, who, g.minutes);
            g.talking = true;
            g.swallowChar = true;
            break;
        }
        case GLFW_KEY_M:
            if (mods & GLFW_MOD_CONTROL) {   // Ctrl+M: the music off and on (not a single key, which is too easily hit)
                if (!g.musicLoaded) { g.say("There is no music (data/music.txt).", 2.0); break; }
                g.music.setOn(!g.music.on(), g.now);
                g.say(g.music.on() ? "Music on." : "Music off. (Ctrl+M: on again.)", 3.0);
                break;
            }
            g.showMap = !g.showMap; break;
        case GLFW_KEY_H: g.help = !g.help; break;
        case GLFW_KEY_G: g.startPick(1); break;
        case GLFW_KEY_L: g.startPick(3); break;   // then a click on what to look at (or L again)
        case GLFW_KEY_C:   // fight: then a click on whom; C again, while fighting, stops
            if (g.fightWith >= 0 || g.fightBeast >= 0) { g.fightWith = -1; g.fightBeast = -1; g.say("You lower your weapon.", 2.0); }
            else g.startPick(4);
            break;
        case GLFW_KEY_I: g.panel = 1; g.invPick = -1; g.useMode = false; g.journal = false; break;
        case GLFW_KEY_U: g.startPick(5); break;   // then a click on something around you (or U again: something you carry)
        case GLFW_KEY_J: {   // the journal: the stories you are in, then what you have overheard, then closed
            bool stories = !g.people.journal(g.minutes).empty(), heard = !g.people.heard.empty();
            if (!g.journal) {
                if (stories) { g.journal = true; g.journalTab = 0; g.journalPage = 0; }
                else if (heard) { g.journal = true; g.journalTab = 1; }
                else g.say("Your journal is empty: you are in no story yet, and have overheard nothing.", 2.5);
            } else if (g.journalTab == 0 && heard) g.journalTab = 1;
            else g.journal = false;
            break;
        }
        case GLFW_KEY_RIGHT:   // the journal's pages (walking is held while it is open)
            if (g.journal && g.journalTab == 0) g.journalPage = std::min(g.journalPage + 1, std::max(0, g.hud.journalPages - 1));
            break;
        case GLFW_KEY_LEFT:
            if (g.journal && g.journalTab == 0) g.journalPage = std::max(0, g.journalPage - 1);
            break;
        case GLFW_KEY_DOWN:   // the journal: the first page of the next part (the finished stories, a town)
            if (g.journal && g.journalTab == 0)
                for (int q : g.hud.journalJumps) if (q > g.journalPage) { g.journalPage = q; break; }
            break;
        case GLFW_KEY_UP: {   // the first page of this part, or of the last one
            if (!g.journal || g.journalTab != 0) break;
            int to = 0;
            for (int q : g.hud.journalJumps) if (q < g.journalPage) to = q;
            g.journalPage = to;
            break;
        }
        case GLFW_KEY_TAB:
            if (g.journal && g.journalTab == 0 && !g.panel) {   // the journal: plain, or detailed
                g.people.player["journal-detailed"] = g.journalDetailed() ? "0" : "1";
                g.journalPage = 0;
                break;
            }
            if (!g.opt.testing) break;
            if (g.boat >= 0) { g.say("Not in a boat."); break; }
            g.gallop = !g.gallop; g.say(g.gallop ? "Galloping." : "Walking."); break;
        case GLFW_KEY_K:
            if (!g.soundsLoaded) { g.say("There are no sound effects (data/sounds.txt).", 2.0); break; }
            g.sounds.setOn(!g.sounds.on());
            g.say(g.sounds.on() ? "Sounds on." : "Sounds off.", 2.0);
            break;
        case GLFW_KEY_N: {
            if (!g.opt.testing || g.world.settlements.empty()) break;
            // the next town (the post houses on the roads are left out: there are 56 of them)
            for (size_t k = 0; k < g.world.settlements.size(); k++) {
                g.townCycle = (g.townCycle + 1) % (int)g.world.settlements.size();
                if (g.world.settlements[g.townCycle].file.find("posthouse") == std::string::npos) break;
            }
            const Settlement &s = g.world.settlements[g.townCycle];
            g.placePlayer(s.x0 + s.w / 2, s.y0 + s.h / 2);
            g.bringLed();   // a cow on her halter comes too
            g.say("You travel to " + s.name + ".");
            break;
        }
        case GLFW_KEY_P: {
            int fw, fh;
            glfwGetFramebufferSize(w, &fw, &fh);
            std::time_t t = std::time(nullptr);
            char buf[64];
            std::strftime(buf, sizeof buf, "shots/galileo-%Y%m%d-%H%M%S.png", std::localtime(&t));
            glReadBuffer(GL_BACK);
            g.savePng(buf, fw, fh);
            g.say(std::string("Saved ") + buf);
            break;
        }
        default: break;
    }
}

static void onCursor(GLFWwindow *w, double x, double y) {
    int ww, wh;
    glfwGetWindowSize(w, &ww, &wh);
    G->mouseX = x / std::max(1, ww);
    G->mouseY = y / std::max(1, wh);
}

static void onMouse(GLFWwindow *w, int button, int action, int) {
    if (action != GLFW_PRESS) return;
    Game &g = *G;
    if (g.fadeT >= 0) return;
    if (g.cardUp) { if (button == GLFW_MOUSE_BUTTON_LEFT) g.afterCard(); return; }
    if (g.sceneNow >= 0) { if (button == GLFW_MOUSE_BUTTON_LEFT) g.nextScene(); return; }   // a click: the next scene
    if (!g.pick) return;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) { g.endPick(); g.say("Never mind.", 1.5); return; }
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    double x, y;
    int ww, wh;
    glfwGetCursorPos(w, &x, &y);
    glfwGetWindowSize(w, &ww, &wh);
    g.click((float)(x / std::max(1, ww)), (float)(y / std::max(1, wh)));
}

static void onScroll(GLFWwindow *, double, double dy) {
    if (dy > 0) G->zoomT = std::max(5.0f, G->zoomT / 1.1f);
    if (dy < 0) G->zoomT = std::min(150.0f, G->zoomT * 1.1f);
}

// ------------------------------------------------------------------ checks

static int runStats(Game &g) {
    World &w = g.world;
    for (auto &gd : g.people.guarded) {   // guarded places: the floor, the guard and the key must exist
        int nl = 0;
        for (auto &L : w.levels) if (lowerS(L.name) == lowerS(gd.place)) nl++;
        bool by = false;
        for (auto &p : g.people.people) if (p.id == gd.by) by = true;
        std::printf("guarded %s (%s): %d floor%s, guard %s%s%s\n", gd.place.c_str(), gd.town.c_str(), nl, nl == 1 ? "" : "s",
                    gd.by.c_str(), by ? "" : " NOT FOUND", !gd.key.empty() && g.items.find(gd.key) < 0 ? ", KEY NOT FOUND" : "");
        if (!nl) std::printf("  NO FLOOR called %s\n", gd.place.c_str());
    }
    std::map<std::string, int> kinds;
    for (auto &b : w.buildings) kinds[b.kind]++;
    std::printf("world %d x %d tiles (scale %d), %zu towns, %zu buildings\n", w.W, w.H, w.S, w.settlements.size(), w.buildings.size());
    for (auto &k : kinds) std::printf("  %-10s %d\n", k.first.c_str(), k.second);
    {   // things
        std::map<std::string, int> n;
        for (auto &t : g.items.things) n[g.items.kinds[t.kind].id] += t.count;
        std::printf("things: %zu, of %zu kinds\n", g.items.things.size(), n.size());
        for (auto &k : n) if (!g.items.kinds[g.items.find(k.first)].fixed) std::printf("  %-12s %d\n", k.first.c_str(), k.second);
        for (auto &t : g.items.things)
            if (g.items.kinds[t.kind].fixed)
                std::printf("  %s %-18s at %d,%d,%d in %s\n", g.items.kinds[t.kind].boat ? "boat" : "art", g.items.kinds[t.kind].id.c_str(), t.x, t.y, t.z, w.floorName(t.x, t.y, t.z).c_str());
        for (auto &k : g.items.kinds)
            if (k.fixed && !n.count(k.id)) std::printf("  art %-18s NOT PLACED\n", k.id.c_str());
        for (auto &k : g.items.kinds)
            if (!k.book.empty() && !g.items.books.count(k.book)) std::printf("  %s: NO BOOK called %s\n", k.id.c_str(), k.book.c_str());
        if (std::getenv("GALILEO_FURNITURE")) {   // what furniture each kind of building has
            std::map<std::string, std::map<std::string, int>> fk;
            for (auto &st : w.settlements)
                for (int Y = st.y0; Y < st.y0 + st.h; Y++)
                    for (int X = st.x0; X < st.x0 + st.w; X++) {
                        if (st.ch(X, Y) == ' ') continue;
                        const Tile &t = st.tileAt(X, Y);
                        if (t.feature && t.bld) fk[w.buildings[t.bld - 1].kind][featureName(t.feature)]++;
                    }
            for (auto &L : w.levels)
                for (auto &t : L.tiles)
                    if (t.feature && t.terrain != T_VOID && L.bld) fk[w.buildings[L.bld - 1].kind + " (floor " + std::to_string(L.z) + ")"][featureName(t.feature)]++;
            for (auto &a : fk) { std::printf("  %s:", a.first.c_str()); for (auto &b : a.second) std::printf(" %s %d,", b.first.c_str(), b.second); std::printf("\n"); }
        }
    }
    for (size_t i = 0; i < w.buildings.size(); i++)
        if (!w.buildings[i].name.empty())
            std::printf("  building %zu: %s (%s)\n", i + 1, w.buildings[i].name.c_str(), w.buildings[i].kind.c_str());
    {
        std::map<std::string, int> byEmblem;
        for (const Sign &sg : w.signs) byEmblem[sg.emblem]++;
        {   // the animals, every town's put out at once
            g.animals.placeNear(w, 0, 0, 1e9f);
            std::map<std::string, int> byKind;
            int wrong = 0;
            for (auto &a : g.animals.list) {
                byKind[g.animals.kinds[a.kind].id]++;
                int X = (int)std::floor(a.x), Y = (int)std::floor(a.y);
                bool penned = false;   // a pen "NAME" rule (animals.txt) keeps them inside that building on purpose
                if (int bi = w.tile(X, Y).bld)
                    for (auto &r : g.animals.rules)
                        if (r.where == "pen" && w.buildings[bi - 1].name.find(r.building) != std::string::npos) penned = true;
                if (!w.passable(X, Y) || (w.tile(X, Y).bld && !penned)) {
                    wrong++;
                    std::printf("  ANIMAL WRONGLY PLACED: %s at (%d,%d)\n", g.animals.kinds[a.kind].name.c_str(), X, Y);
                }
            }
            if (getenv("GALILEO_LIST_ANIMALS"))
                for (auto &an : g.animals.list) std::printf("  animal %-6s at %.0f,%.0f%s\n", g.animals.kinds[an.kind].id.c_str(), an.x, an.y, an.tethered ? " (tied up)" : "");
            std::printf("animals: %zu:", g.animals.list.size());
            for (auto &kv : byKind) std::printf(" %s %d", kv.first.c_str(), kv.second);
            std::printf("%s\n", wrong ? "" : " (all on open ground)");
        }
        std::printf("signs: %zu:", w.signs.size());
        for (auto &kv : byEmblem) std::printf(" %s %d", kv.first.c_str(), kv.second);
        std::printf("\n");
        if (getenv("GALILEO_LIST_SIGNS"))
            for (const Sign &sg : w.signs)
                std::printf("  sign %-8s at %d,%d facing %d,%d on %s\n", sg.emblem.c_str(), sg.x, sg.y, sg.ox, sg.oy,
                            w.buildings[sg.bld - 1].name.empty() ? "a house" : w.buildings[sg.bld - 1].name.c_str());
    }
    if (getenv("GALILEO_LIST_FERRIES"))   // where each ferry waits, and where it goes
        for (auto &kv : w.links)
            if (kv.second.verb.find("ferryman") != std::string::npos) {
                int64_t k = kv.first;
                (void)k;
                std::printf("  ferry to (%d,%d): %s\n", kv.second.x, kv.second.y, kv.second.verb.c_str());
            }
    if (const char *wm = getenv("GALILEO_WATERMAP")) {   // X0,Y0,X1,Y1,STEP: '~' water, '=' bridge, '_' road, '#' town
        int x0, y0, x1, y1, st = 1;
        if (std::sscanf(wm, "%d,%d,%d,%d,%d", &x0, &y0, &x1, &y1, &st) >= 4 && st > 0)
            for (int y = y1; y >= y0; y -= st) {
                std::string row;
                for (int x = x0; x <= x1; x += st) {
                    int tr = w.tile(x, y).terrain;
                    bool town = false;
                    for (auto &s : w.settlements) if (s.inside(x, y)) town = true;
                    row += tr == T_WATER || tr == T_DEEP ? '~' : tr == T_BRIDGE ? '=' : tr == T_ROAD ? '_' : town ? '#' : '.';
                }
                std::printf("  %5d %s\n", y, row.c_str());
            }
    }
    if (getenv("GALILEO_LIST_FLOORS")) {
        std::map<std::string, int> shown;
        for (const Level &l : w.levels)
            if (shown[l.name]++ < 4)
                std::printf("  floor %d \"%s\" of building %d at (%d,%d) %dx%d\n", l.z, l.name.c_str(), l.bld, l.x0, l.y0, l.w, l.h);
    }
    if (const char *dump = getenv("GALILEO_DUMP_BLD")) {
        int id = std::atoi(dump);
        const Building &b = w.buildings[id - 1];
        std::printf("building %d %s (%d,%d)-(%d,%d)\n", id, b.name.c_str(), b.x0, b.y0, b.x1, b.y1);
        for (int y = b.y1 + 2; y >= b.y0 - 2; y--) {
            for (int x = b.x0 - 2; x <= b.x1 + 2; x++) {
                Tile &t = w.tile(x, y);
                char c = '.';
                if (t.terrain == T_WATER || t.terrain == T_DEEP) c = '~';
                if (t.terrain == T_BRIDGE) c = '=';
                if (t.wall == W_CITYWALL) c = '#'; else if (t.wall == W_TOWER) c = 'O'; else if (t.wall == W_GATE) c = 'G';
                else if (t.wall == W_WALL || t.wall == W_WINDOW) c = 'w'; else if (t.wall == W_DOOR) c = 'd';
                else if (t.feature == F_STAIRS) c = 'S'; else if (t.feature != F_NONE) c = getenv("GALILEO_DUMP_CODES") ? (char)('a' + t.feature - 1) : 'f';
                std::putchar(c);
            }
            std::putchar('\n');
        }
    }
    {
        std::map<std::string, std::array<int, 4>> byTown;   // people, with a bed, with work, named
        for (auto &p : g.people.people) {
            auto &c = byTown[p.town];
            c[0]++;
            if (p.hasBed) c[1]++;
            if (p.hasWork) c[2]++;
            if (std::count(p.id.begin(), p.id.end(), ':') == 1) c[3]++;
        }
        std::printf("people:\n");
        if (getenv("GALILEO_DEBUG_WORK")) {   // which roles found no place of work, town by town
            std::map<std::string, int> miss;
            for (auto &p : g.people.people)
                if (!p.hasWork && !p.workKind.empty() && p.workKind != "none") miss[p.town + ": " + p.role + " (" + p.workKind + ")"]++;
            for (auto &kv : miss) std::printf("    no work: %s x %d\n", kv.first.c_str(), kv.second);
        }
        for (auto &kv : byTown)
            std::printf("  %-26s %3d people (%d named), %d with a bed, %d with work\n", kv.first.c_str(), kv.second[0], kv.second[3],
                        kv.second[1], kv.second[2]);
        for (auto &p : g.people.people)
            if (std::count(p.id.begin(), p.id.end(), ':') == 1 && (!p.hasBed || (!p.hasWork && !p.workKind.empty() && p.workKind != "none")))
                std::printf("    %s (%s): %s%s\n", p.name.c_str(), p.role.c_str(), p.hasBed ? "" : "no bed ",
                            (!p.hasWork && !p.workKind.empty()) ? ("no work place '" + p.workKind + "'").c_str() : "");
    }
    {   // two named buildings whose footprints overlap were drawn one over the other (a town
        // map's landmark block that is not a rectangle takes in its neighbour: St Bride's inside
        // Somerset House); round ones (rotunda) only touch their neighbours' corners, and gates are part
        // of the walls (the Porta Fiorentina under the Cittadella Nuova at Pisa): both left out
        int overlaps = 0;
        const auto &B = g.world.buildings;
        for (size_t i = 0; i < B.size(); i++) {
            if (B[i].name.empty() || B[i].kind == "rotunda" || B[i].kind == "gate") continue;
            for (size_t j = i + 1; j < B.size(); j++) {
                if (B[j].name.empty() || B[j].kind == "rotunda" || B[j].kind == "gate") continue;
                if (B[i].x0 <= B[j].x1 && B[j].x0 <= B[i].x1 && B[i].y0 <= B[j].y1 && B[j].y0 <= B[i].y1) {
                    std::printf("  BUILDINGS OVERLAP: %s (%s) and %s (%s)\n", B[i].name.c_str(), B[i].kind.c_str(), B[j].name.c_str(), B[j].kind.c_str());
                    overlaps++;
                }
            }
        }
        std::printf("named buildings drawn over one another: %d\n", overlaps);
    }
    {
        auto un = g.people.unanswered();
        std::printf("highlighted words that the speaker cannot answer: %d\n", (int)un.size());
        for (auto &u : un) std::printf("    %s\n", u.c_str());
    }
    // GALILEO_DEBUG_TILES=X,Y,R: a map of the ground floor about a point (# wall, C town or fortress
    // wall, T tower, G gate, D door, W window, S stairs, x furniture or blocked, + in a building, . open)
    if (const char *dt = std::getenv("GALILEO_DEBUG_TILES")) {
        int cx = 0, cy = 0, r = 8;
        std::sscanf(dt, "%d,%d,%d", &cx, &cy, &r);
        for (int Y = cy + r; Y >= cy - r; Y--) {
            std::string row;
            for (int X = cx - r; X <= cx + r; X++) {
                const Tile &t = w.tile(X, Y);
                char c = t.wall == W_CITYWALL ? 'C' : t.wall == W_TOWER ? 'T' : t.wall == W_GATE ? 'G' : t.wall == W_DOOR ? 'D' :
                         t.wall == W_WINDOW ? 'W' : t.wall != W_NONE ? '#' : t.feature == F_STAIRS ? 'S' : t.feature == F_ALTAR ? 'A' : t.feature == F_BENCH ? 'b' : t.feature == F_TABLE ? 't' : !w.passable(X, Y, 0) ? 'x' : t.bld ? '+' : '.';
                if (X == cx && Y == cy) c = '@';
                row += c;
            }
            std::printf("  tiles %5d: %s\n", Y, row.c_str());
        }
        for (auto &b : w.buildings)   // and the buildings in that square, with their corners
            if (b.x1 >= cx - r && b.x0 <= cx + r && b.y1 >= cy - r && b.y0 <= cy + r)
                std::printf("  tiles: %s (%s) from %d,%d to %d,%d\n", b.name.empty() ? "a house" : b.name.c_str(), b.kind.c_str(), b.x0, b.y0, b.x1, b.y1);
    }
    int bad = 0;
    for (auto &s : w.settlements) {
        // flood fill over passable tiles from the town centre's nearest open tile, inside a margin
        int X = s.x0 + s.w / 2, Y = s.y0 + s.h / 2;
        g.findOpen(X, Y);
        int m = 30;
        int bx0 = s.x0 - m, by0 = s.y0 - m, bw = s.w + 2 * m, bh = s.h + 2 * m;
        std::vector<uint8_t> seen((size_t)bw * bh, 0);
        std::unordered_set<int64_t> seenUp;   // tiles on other floors
        struct N { int x, y, z; };
        std::deque<N> q{{X, Y, 0}};
        seen[(Y - by0) * bw + (X - bx0)] = 1;
        auto visit = [&](int nx, int ny, int nz) {
            if (nz == 0) {
                if (nx < bx0 || ny < by0 || nx >= bx0 + bw || ny >= by0 + bh) return;
                if (seen[(ny - by0) * bw + (nx - bx0)]) return;
                seen[(ny - by0) * bw + (nx - bx0)] = 1;
            } else {
                if (!seenUp.insert(World::key3(nx, ny, nz)).second) return;
            }
            q.push_back({nx, ny, nz});
        };
        while (!q.empty()) {
            N n = q.front(); q.pop_front();
            int x = n.x, y = n.y, z = n.z;
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++) {
                    if (!dx && !dy) continue;
                    int nx = x + dx, ny = y + dy;
                    if (const Link *lk = w.linkAt(nx, ny, z)) { visit(lk->x, lk->y, lk->z); continue; }
                    if (!w.passable(nx, ny, z)) continue;
                    if (dx && dy && (w.blocksDiagonal(x + dx, y, z) || w.blocksDiagonal(x, y + dy, z))) continue;
                    if (dx && dy && z == 0 && !w.passable(x + dx, y, z) && !w.passable(x, y + dy, z)) continue;
                    visit(nx, ny, z);
                }
        }
        // on foot only (no ladders, stairs or boats): from wherever a way down lands you, can you
        // walk out of doors? (Carfax's stair once stood where you had to pass to leave)
        {
            std::vector<int> comp((size_t)bw * bh, 0);
            std::vector<uint8_t> outdoors(1, 0);
            auto walkable = [&](int x, int y) { return !w.linkAt(x, y, 0) && w.passable(x, y, 0); };
            auto label = [&](int sx, int sy) {
                int id = (int)outdoors.size();
                outdoors.push_back(0);
                std::deque<std::pair<int, int>> fq{{sx, sy}};
                comp[(sy - by0) * bw + (sx - bx0)] = id;
                while (!fq.empty()) {
                    auto [x, y] = fq.front(); fq.pop_front();
                    if (!w.tile(x, y).bld) outdoors[id] = 1;
                    for (int dx = -1; dx <= 1; dx++)
                        for (int dy = -1; dy <= 1; dy++) {
                            if (!dx && !dy) continue;
                            int nx = x + dx, ny = y + dy;
                            if (nx < bx0 || ny < by0 || nx >= bx0 + bw || ny >= by0 + bh || comp[(ny - by0) * bw + (nx - bx0)]) continue;
                            if (!walkable(nx, ny)) continue;
                            if (dx && dy && (w.blocksDiagonal(x + dx, y, 0) || w.blocksDiagonal(x, y + dy, 0))) continue;
                            if (dx && dy && !w.passable(x + dx, y, 0) && !w.passable(x, y + dy, 0)) continue;
                            comp[(ny - by0) * bw + (nx - bx0)] = id;
                            fq.push_back({nx, ny});
                        }
                }
                return id;
            };
            for (auto &kv : w.links) {
                const Link &k = kv.second;
                if (k.z != 0 || k.x < s.x0 || k.y < s.y0 || k.x >= s.x0 + s.w || k.y >= s.y0 + s.h || !walkable(k.x, k.y)) continue;
                int &c = comp[(k.y - by0) * bw + (k.x - bx0)];
                int id = c ? c : label(k.x, k.y);
                if (!outdoors[id]) {
                    const Tile &tt = w.tile(k.x, k.y);
                    std::printf("  TRAPPED: \"%s\" lands you at (%d,%d) in %s, and you cannot walk out of doors from there\n", k.verb.c_str(), k.x, k.y,
                                !tt.bld ? "the open" : w.buildings[tt.bld - 1].name.empty() ? "a house" : w.buildings[tt.bld - 1].name.c_str());
                    bad++;
                }
            }
        }
        // the floors: how many can be reached, and are all their tiles reachable?
        int floors = 0, floorsOk = 0, floorsPart = 0, floorsNone = 0;
        std::map<std::string, int> missingByName;
        for (const Level &l : w.levels) {
            bool mine = l.x0 >= s.x0 - m && l.y0 >= s.y0 - m && l.x0 + l.w <= s.x0 + s.w + m && l.y0 + l.h <= s.y0 + s.h + m;
            if (!mine && l.underground && l.w > 60) mine = !(l.x0 > s.x0 + s.w + m || l.x0 + l.w < s.x0 - m || l.y0 > s.y0 + s.h + m || l.y0 + l.h < s.y0 - m);
            if (mine && l.bld) {   // a building's floors belong to the town whose map the building stands on, not a neighbour's margin
                int cx = l.x0 + l.w / 2, cy = l.y0 + l.h / 2;
                mine = cx >= s.x0 && cy >= s.y0 && cx < s.x0 + s.w && cy < s.y0 + s.h;
            }
            if (!mine) continue;
            int tot = 0, got = 0;
            for (int yy = l.y0; yy < l.y0 + l.h; yy++)
                for (int xx = l.x0; xx < l.x0 + l.w; xx++)
                    if (w.passable(xx, yy, l.z)) { tot++; if (seenUp.count(World::key3(xx, yy, l.z))) got++; }
            if (tot == 0) continue;
            floors++;
            if (got == tot) floorsOk++;
            else if (got > 0) { floorsPart++; missingByName[l.name + " (part)"]++; }
            else {
                floorsNone++; missingByName[l.name]++;
                if (getenv("GALILEO_DEBUG_FLOORS")) {
                    std::printf("    %s at (%d,%d) %dx%d z=%d bld=%d\n", l.name.c_str(), l.x0, l.y0, l.w, l.h, l.z, l.bld);
                    for (auto &kv : w.links) {
                        const Link &k = kv.second;
                        if (k.z == l.z && l.inside(k.x, k.y)) std::printf("      a link arrives at (%d,%d,%d): %s\n", k.x, k.y, k.z, k.verb.c_str());
                    }
                    for (int x = l.x0 - 1; x <= l.x0 + l.w; x++)      // where the ways up start, and whether the flood got there
                        for (int y = l.y0 - 1; y <= l.y0 + l.h; y++)
                            if (const Link *lk = w.linkAt(x, y, 0))
                                if (lk->z == l.z) {
                                    int near = 0;
                                    for (int dx = -1; dx <= 1; dx++)
                                        for (int dy = -1; dy <= 1; dy++) {
                                            int nx = x + dx, ny = y + dy;
                                            if (nx >= bx0 && ny >= by0 && nx < bx0 + bw && ny < by0 + bh && seen[(ny - by0) * bw + (nx - bx0)]) near++;
                                        }
                                    std::printf("      a way up starts at (%d,%d,0), %d reached tiles round it\n", x, y, near);
                                }
                }
            }
            if ((l.underground && l.w > 60) || l.name == "the town walls")
                std::printf("  %s: %d of %d tiles reachable\n", l.name.c_str(), got, tot);
        }
        int open = 0, reached = 0;
        std::map<int, int> unreachedIn;   // building (0: outdoors) -> open tiles not reached
        for (int y = s.y0; y < s.y0 + s.h; y++)
            for (int x = s.x0; x < s.x0 + s.w; x++)
                if (w.passable(x, y)) {
                    open++;
                    if (seen[(y - by0) * bw + (x - bx0)]) reached++;
                    else if (getenv("GALILEO_UNREACHED")) { unreachedIn[w.tile(x, y).bld]++; if (!w.tile(x, y).bld) std::printf("    unreached outdoors (%d,%d)\n", x, y); }
                }
        for (auto &kv : unreachedIn)
            if (kv.first) std::printf("    unreached in building %d (%s %s): %d tiles\n", kv.first, w.buildings[kv.first - 1].kind.c_str(), w.buildings[kv.first - 1].name.c_str(), kv.second);
        // buildings: is each one with a door reachable?
        int withDoor = 0, reachable = 0, sealed = 0;
        std::vector<std::string> unreachedNamed;
        for (size_t i = 0; i < w.buildings.size(); i++) {
            const Building &b = w.buildings[i];
            if (!s.inside(b.x0, b.y0)) continue;
            if (b.kind == "tower" || b.kind == "gate" || b.kind == "pavilion") continue;
            bool door = false, reach = false;
            for (int x = b.x0; x <= b.x1; x++)
                for (int y = b.y0; y <= b.y1; y++) {
                    Tile &t = w.tile(x, y);
                    if (t.wall == W_DOOR || t.wall == W_GATE) {
                        door = true;
                        if (seen[(y - by0) * bw + (x - bx0)]) reach = true;
                    }
                }
            if (b.kind == "market") { door = reach = true; }
            if (b.kind == "arena") {          // open arches, no doors: reachable if any tile of it is
                door = true;
                for (int x = b.x0; x <= b.x1 && !reach; x++)
                    for (int y = b.y0; y <= b.y1 && !reach; y++)
                        if (w.tile(x, y).bld == (int)i + 1 && seen[(y - by0) * bw + (x - bx0)]) reach = true;
            }
            if (door) withDoor++; else sealed++;
            if (reach) reachable++;
            else if (!b.name.empty()) unreachedNamed.push_back(b.name);
            if (door && b.kind != "market" && b.kind != "arena") {   // a door with nowhere to go
                int room = 0;
                for (int x = b.x0; x <= b.x1; x++)
                    for (int y = b.y0; y <= b.y1; y++) {
                        Tile &t = w.tile(x, y);
                        if (t.bld == (int)i + 1 && t.wall == W_NONE && !featureBlocksWalk(t.feature)) room++;
                    }
                if (room == 0) { std::printf("  NO ROOM INSIDE: %s (%s)\n", b.name.empty() ? b.kind.c_str() : b.name.c_str(), b.kind.c_str()); bad++; }
            }
        }
        std::printf("%s: %d x %d tiles at (%d,%d); open tiles reachable from the centre: %d of %d (%.1f%%)\n",
                    s.name.c_str(), s.w, s.h, s.x0, s.y0, reached, open, 100.0 * reached / std::max(1, open));
        std::printf("  floors above and below: %d, all of each reachable: %d, partly: %d, not at all: %d\n", floors, floorsOk, floorsPart, floorsNone);
        for (auto &mn : missingByName) std::printf("    not all reachable: %s x %d\n", mn.first.c_str(), mn.second);
        std::printf("  buildings with a door %d (reachable %d), without a door %d\n", withDoor, reachable, sealed);
        for (auto &n : unreachedNamed) { std::printf("  NOT REACHABLE: %s\n", n.c_str()); bad++; }
        for (auto &e : s.waterExits) {
            int sx = 0, sy = 0, n = 0;
            bool ok = w.reachesSea(e.first, e.second, 3000000, sx, sy, n);
            if (ok) std::printf("  river leaving the map at (%d,%d) reaches the sea at (%d,%d) (old map %.0f,%.0f)\n",
                                e.first, e.second, sx, sy, sx / (float)w.S, sy / (float)w.S);
            else if (w.inlandTowns.count(s.name))
                std::printf("  river leaving the map at (%d,%d) joins inland water (%d tiles; world.txt \"inland\")\n", e.first, e.second, n);
            else { std::printf("  RIVER leaving the map at (%d,%d) DOES NOT REACH THE SEA (%d tiles of water searched)\n", e.first, e.second, n); bad++; }
        }
    }
    extern std::unordered_map<std::string, vec2> g_gates;
    for (auto &gt : g_gates) std::printf("  road head %s at (%.1f, %.1f)\n", gt.first.c_str(), gt.second.x, gt.second.y);
    for (auto &s : w.settlements)
        std::printf("  settlement %s|%d %d %d %d|%s\n", s.name.c_str(), s.x0, s.y0, s.w, s.h, s.hamlet ? "hamlet" : "town");
    // the towns the roads join: from each town not yet reached, the ones its roads lead to
    {
        std::vector<int> comp(w.settlements.size(), -1);
        int nc = 0;
        for (size_t i = 0; i < w.settlements.size(); i++) {
            if (comp[i] >= 0 || w.settlements[i].hamlet) continue;
            auto d = w.roadDistances((int)i);
            std::string towns, far;
            float farD = 0;
            int hamlets = 0;
            for (size_t j = 0; j < d.size(); j++) {
                if (d[j] < 0) continue;
                comp[j] = nc;
                if (w.settlements[j].hamlet) { hamlets++; continue; }
                towns += (towns.empty() ? "" : ", ") + w.settlements[j].name;
                if (d[j] > farD) { farD = d[j]; far = w.settlements[j].name; }
            }
            std::printf("  joined by road: %s%s (and %d hamlets)", towns.c_str(), "", hamlets);
            if (!far.empty() && far != w.settlements[i].name) std::printf("; from %s the farthest is %s, %.0f tiles by road", w.settlements[i].name.c_str(), far.c_str(), farD);
            std::printf("\n");
            nc++;
        }
        for (size_t i = 0; i < w.settlements.size(); i++) {
            if (comp[i] >= 0) continue;
            bool post = w.settlements[i].file.find("posthouse") != std::string::npos;
            std::printf("  %s reaches %s\n", post ? "NO ROAD" : "no road", w.settlements[i].name.c_str());
            if (post) bad++;
        }
    }
    for (auto &st : w.roadStops) {
        std::printf("  ROAD STOPS AT WATER at old map (%.1f,%.1f): road %s\n", st.at.x / w.S, st.at.y / w.S, st.line.c_str());
        bad++;
    }
    std::printf("roads: %zu tiles, river channels: %zu tiles\n", w.roads.size(), w.carved.size());
    return bad ? 1 : 0;
}

// ------------------------------------------------------------------ main

int main(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--data") o.data = next();
        else if (a == "--scale") o.scale = std::atoi(next().c_str());
        else if (a == "--town") o.town = next();
        else if (a == "--at") { std::string v = next(); std::sscanf(v.c_str(), "%f,%f", &o.ax, &o.ay); o.atSet = true; }
        else if (a == "--offset") { std::string v = next(); std::sscanf(v.c_str(), "%f,%f", &o.dx, &o.dy); }
        else if (a == "--time") { std::string v = next(); int hh = 0, mm = 0; std::sscanf(v.c_str(), "%d:%d", &hh, &mm); o.hour = hh + mm / 60.0f; }
        else if (a == "--yaw") o.yaw = std::atof(next().c_str());
        else if (a == "--pitch") o.pitch = std::atof(next().c_str());
        else if (a == "--zoom") o.zoom = std::atof(next().c_str());
        else if (a == "--size") { std::string v = next(); std::sscanf(v.c_str(), "%dx%d", &o.w, &o.h); }
        else if (a == "--shot") o.shot = next();
        else if (a == "--portrait") { o.portraitOut = next(); if (o.shot.empty()) o.shot = o.portraitOut; }   // tests: your painted portrait, written there
        else if (a == "--help-overlay") o.help = true;
        else if (a == "--map") o.map = true;
        else if (a == "--roofs-off") o.roofsOff = true;
        else if (a == "--stats") o.stats = true;
        else if (a == "--new") o.fresh = true;
        else if (a == "--creator") o.creator = true;
        else if (a == "--scene") o.scene = std::atoi(next().c_str());
        else if (a == "--sequence") o.sequence = next();
        else if (a == "--no-music") o.noMusic = true;
        else if (a == "--testing") o.testing = true;
        else if (a == "--bld") o.bld = std::atoi(next().c_str());
        else if (a == "--floor") o.floor = std::atoi(next().c_str());
        else if (a == "--walk") o.walk = next();
        else if (a == "--save") o.save = next();
        else if (a == "--play") { o.play = next(); o.save = o.play; }
        else if (a == "--fps") o.fps = (float)std::atof(next().c_str());
        else if (a == "--autosave") o.autosave = (float)std::atof(next().c_str());
        else if (a == "--quit-after") o.quitAfter = std::atof(next().c_str());
        else { usage(); return a == "--help" ? 0 : 2; }
    }

    Game g;
    G = &g;
    g.opt = o;
    auto t0 = std::chrono::steady_clock::now();
    if (!g.world.init(o.data, o.scale)) { std::cerr << "could not build the world\n"; return 1; }
    auto t1 = std::chrono::steady_clock::now();
    std::fprintf(stderr, "world built in %.2f s\n", std::chrono::duration<double>(t1 - t0).count());
    if (!g.people.load(g.world, o.data)) { std::cerr << "could not load the people\n"; return 1; }
    {   // the road's people (main.cc, Game::wildStep): a band of brigands, an escort to hire, a gamekeeper
        int b = g.people.makeWild("brigand", 4, "brigand");
        for (int i = 0; b >= 0 && i < 4; i++) g.people.band.push_back(b + i);
        g.people.escortIdx = g.people.makeWild("escort", 1, "escort");
        g.people.keeperIdx = g.people.makeWild("gamekeeper", 1, "gamekeeper");
    }
    if (!g.items.load(o.data + "/items.txt")) std::cerr << "could not load the things (items.txt)\n";
    g.items.loadBooks(o.data + "/books");
    g.items.place(g.world);
    g.people.items = &g.items;
    g.people.rideAsk = [&g](int who, const std::string &words, std::string &reply) { return g.rideOffer(who, words, reply); };
    g.animals.load(o.data + "/animals.txt");
    g.loadScenes(o.data + "/scenes.txt");
    g.loadCards(o.data + "/cards.txt");
    g.loadCoins(o.data + "/coins.txt");
    g.loadWild(o.data + "/wild.txt");
    for (auto &k : g.animals.kinds)   // what their uses need and give must be in the catalogue
        for (auto &u : k.uses)
            for (const std::string *id : {&u.needs, &u.gives})
                if (!id->empty() && g.items.find(*id) < 0) std::fprintf(stderr, "animals.txt: %s's \"%s\" names %s, which is not in items.txt\n", k.id.c_str(), u.verb.c_str(), id->c_str());
    for (auto &pp : g.people.people)   // each town's country, for its herds
        if (pp.mem.count("country")) g.animals.country[pp.town] = pp.mem.at("country");
    for (auto &pp : g.people.people)   // what shopkeepers sell must be in the catalogue
        for (auto &s : pp.sells)
            if (g.items.find(s.first) < 0) { std::fprintf(stderr, "%s sells %s, which is not in items.txt\n", pp.name.c_str(), s.first.c_str()); }
    if (o.stats) return runStats(g);

    // start position
    vec2 st = g.world.start;
    if (!o.town.empty())
        for (size_t i = 0; i < g.world.settlements.size(); i++) {
            auto &s = g.world.settlements[i];
            if (s.name == o.town) { st = vec2(s.x0 + s.w * 0.5f, s.y0 + s.h * 0.5f); g.townCycle = (int)i; }
        }
    if (o.atSet) st = vec2(o.ax, o.ay);
    st = st + vec2(o.dx, o.dy);
    g.minutes = o.hour * 60;
    g.yaw = g.yawT = o.yaw;
    g.pitch = g.pitchT = o.pitch;
    g.zoom = g.zoomT = o.zoom;
    g.help = o.help;
    g.showMap = o.map;
    g.roofsOff = o.roofsOff;

    bool shot = !o.shot.empty();
    // a text-only run (--walk without --shot, which is what tools/play.sh runs) draws nothing, so it has no
    // window and no GL context until something has to be drawn (a portrait: Game::openGL below)
    const bool textOnly = !shot && !o.walk.empty();
    auto makeContext = [&]() -> bool {
        if (!glfwInit()) { std::cerr << "glfwInit failed\n"; return false; }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);
        if (shot || textOnly) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        g.win = glfwCreateWindow(shot || textOnly ? 64 : o.w, shot || textOnly ? 64 : o.h, "Galileo", nullptr, nullptr);
        if (!g.win) { std::cerr << "could not open a window\n"; return false; }
        glfwMakeContextCurrent(g.win);
        glfwSwapInterval(1);
        g.R.init();
        g.glReady = true;
        return true;
    };
    if (textOnly) {
        // the lazy context draws only a portrait (nothing else is drawn in a text-only run, so no town meshes)
        g.openGL = [&]() { return makeContext(); };
    } else if (!makeContext()) return 1;
    g.hud.makeMapImage(g.world);
    // background music, only in the window (not for pictures, tests or checks)
    if (o.shot.empty() && o.walk.empty() && !o.noMusic) g.musicLoaded = g.music.load(o.data + "/music.txt");
    if (o.shot.empty() && o.walk.empty() && !o.noMusic) g.soundsLoaded = g.sounds.load(o.data + "/sounds.txt");
    if (!textOnly) g.buildTowns();
    g.placePlayer((int)std::floor(st.x), (int)std::floor(st.y));
    // continue the saved game: the time, where you were, and what everyone remembers
    // a test walk without a picture is a test too: it never reads (or writes) the saved game
    if (!o.save.empty()) g.savePath = o.save;
    bool shotMode = (!o.shot.empty() || !o.walk.empty()) && o.play.empty();
    std::string talkingId;   // --play: the person you were talking to when the last run ended
    if (!shotMode && !o.fresh) {
        std::ifstream in(g.savePath);
        if (in) {
            std::string line;
            std::stringstream rest;
            bool posGiven = o.atSet || !o.town.empty() || o.bld;
            int savedX = 0, savedY = 0;
            bool savedAt = false;
            // places renamed since a game was saved: their names, and the ids and keys made from them
            static const std::vector<std::pair<std::string, std::string>> renamed = {
                {"the_farm_of_la_chapelle", "watt's_farm"}, {"the_Farm_of_La_Chapelle", "Watt's_Farm"},
                {"the-farm-of-la-chapelle", "watt's-farm"}, {"the Farm of La Chapelle", "Watt's Farm"}};
            while (std::getline(in, line)) {
                for (auto &rn : renamed)
                    for (size_t at = line.find(rn.first); at != std::string::npos; at = line.find(rn.first, at + rn.second.size()))
                        line.replace(at, rn.first.size(), rn.second);
                std::istringstream ls(line);
                std::string w;
                ls >> w;
                if (w == "time") ls >> g.minutes;
                else if (w == "view") { float a, b, c; if (ls >> a >> b >> c) { g.yaw = g.yawT = a; g.pitch = g.pitchT = b; g.zoom = g.zoomT = c; } }
                else if (w == "talking") { std::getline(ls, talkingId); talkingId = trimS(talkingId); }
                else if (w == "music") { std::string v; ls >> v; if (v == "off") { g.music.setOn(false, 0); g.savedMusicOff = true; } }
                else if (w == "sounds") { std::string v; ls >> v; if (v == "off") { g.sounds.setOn(false); g.savedSoundsOff = true; } }
                else if (w == "at" && !posGiven) {
                    int X, Y, Z;
                    ls >> X >> Y >> Z;
                    g.placePlayer(X, Y);
                    if (Z != 0) g.placeOnFloor(X, Y, Z);
                    else if (!o.play.empty() && g.world.passable(X, Y, 0) && (g.tx != X || g.ty != Y)) {   // playing: inside, where you were
                        g.tx = X; g.ty = Y; g.from = g.to = vec2(X + 0.5f, Y + 0.5f);
                        vec2 pp = g.playerPos();
                        g.camTarget = vec3(pp.x, pp.y, g.world.groundZ(pp.x, pp.y));
                        g.updatePlace(true);
                    }
                    savedX = X; savedY = Y; savedAt = true;
                }
                else rest << line << "\n";
            }
            std::string text = rest.str();
            std::istringstream r1(text), r2(text);
            g.people.loadSave(r1);
            g.items.loadSave(r2);
            g.syncBoat();
            if (g.boat >= 0) {
                if (savedAt && g.world.tile(savedX, savedY).terrain != T_WATER && g.world.tile(savedX, savedY).terrain != T_DEEP &&
                    g.world.tile(savedX, savedY).terrain != T_BRIDGE) savedAt = false;
                if (savedAt) {   // back in the boat, out on the water where you were
                    g.tx = savedX; g.ty = savedY; g.tz = 0;
                    g.from = g.to = vec2(savedX + 0.5f, savedY + 0.5f);
                    g.camTarget = vec3(g.from.x, g.from.y, 0);
                    g.updatePlace(true);
                } else {   // you start somewhere else (--at, --town ...): on foot
                    g.people.player.erase("boat");
                    g.boat = -1;
                }
            }
            g.restoreHorse();
            g.restoreLed();
            g.savedGameRead = true;
            std::fprintf(stderr, "continuing the saved game (start afresh with --new)\n");
        }
    }
    if (!g.people.player.count("money")) g.people.player["money"] = "240";   // a traveller has a livre about him
    // a new game begins with choosing who you are; --creator shows it for a picture
    if (o.creator || (!g.savedGameRead && o.walk.empty() && o.shot.empty() && !g.chosen())) g.beginCreating();
    if (o.creator) { g.zoom = g.zoomT; g.pitch = g.pitchT; }
    if (!o.sequence.empty()) {
        if (!g.sequences.count(o.sequence)) std::fprintf(stderr, "no scenes called %s\n", o.sequence.c_str());
        else {
            g.scenes = g.sequences[o.sequence];
            g.seqMode = true; g.seqTx = g.tx; g.seqTy = g.ty; g.seqTz = g.tz;
            g.sceneKeepMinutes = g.minutes; g.sceneKeepRoofs = g.roofsOff;
            g.sceneNow = std::max(0, std::min((int)g.scenes.size() - 1, o.scene - 1));
            g.applyScene(); g.describeScene();
        }
    } else if (o.scene > 0 && o.scene <= (int)g.scenes.size()) { g.startScenes(); g.sceneNow = o.scene - 1; g.applyScene(); g.describeScene(); }
    if (o.bld > 0 && o.bld <= (int)g.world.buildings.size()) {
        const Building &b = g.world.buildings[o.bld - 1];
        for (int y = (b.y0 + b.y1) / 2, k = 0; k < 2; k++)
            for (int x = b.x0; x <= b.x1; x++)
                if (g.world.passable(x, y) && g.world.tile(x, y).wall == W_NONE) {
                    g.tx = x; g.ty = y; g.from = g.to = vec2(x + 0.5f, y + 0.5f);
                    vec2 p = g.playerPos();
                    g.camTarget = vec3(p.x, p.y, g.world.groundZ(p.x, p.y));
                    g.updatePlace(true);
                    k = 2;
                    break;
                }
    }

    if (o.floor != 0) {
        bool ok = false;
        // --at X,Y --floor Z: that very tile, if it is floor (the ground placement above may have
        // moved you out of the building)
        if (o.atSet && !o.bld && g.world.passable((int)o.ax, (int)o.ay, o.floor)) ok = g.placeOnFloor((int)o.ax, (int)o.ay, o.floor);
        for (const Level &l : g.world.levels) {
            if (ok) break;
            if (l.z == o.floor && (o.bld ? l.bld == o.bld : l.inside(g.tx, g.ty) || (!o.bld && o.atSet && l.inside((int)o.ax, (int)o.ay)))) {
                ok = g.placeOnFloor(o.bld ? l.x0 + l.w / 2 : g.tx, o.bld ? l.y0 + l.h / 2 : g.ty, o.floor);
                if (ok) break;
            }
        }
        if (!ok) {
            // any floor at this depth near the player
            for (const Level &l : g.world.levels)
                if (l.z == o.floor && std::abs(l.x0 + l.w / 2 - g.tx) + std::abs(l.y0 + l.h / 2 - g.ty) < 400 &&
                    (ok = g.placeOnFloor(l.x0 + l.w / 2, l.y0 + l.h / 2, o.floor)))
                    break;
        }
        std::fprintf(stderr, ok ? "on floor %d: %s\n" : "no floor %d here\n", o.floor, g.place.c_str());
    }

    if (!o.walk.empty()) {
        g.walkEcho = true;
        std::istringstream ws(o.walk);
        g.people.echo = true;   // print what is overheard
        g.testMode = true;
        g.playByCommands = !o.play.empty();
        g.music.load(o.data + "/music.txt");   // read, to check pieces asked for (nothing is played)
        if (o.w > 0 && o.h > 0) g.viewAspect = (float)o.w / o.h;
        std::string tok;
        int lastWho = -1;
        std::string lastWalkWant;   // what the last WALKTO asked for ("merchant#2")
        if (!o.play.empty()) {   // playing: everyone in their place before the first command
            for (int q = 0; q < 30; q++) { g.people.update(g.world, 0.1f, g.minutes, g.tx, g.ty, g.tz); g.animals.update(g.world, 0.1f, g.minutes, g.tx, g.ty); g.takeRequests(); }   // a beat that fires here moves the calendar now
            for (size_t i = 0; i < g.people.people.size() && !talkingId.empty(); i++)   // and still in the conversation
                if (g.people.people[i].id == talkingId) {
                    g.people.talk = Conversation();
                    g.people.talk.who = (int)i;
                    g.talking = true;
                    std::fprintf(stderr, "  [your conversation with %s goes on from last time; BYE ends it]\n", g.people.label(g.people.people[i]).c_str());
                }
        }
        static const std::map<std::string, std::pair<int, int>> D = {
            {"N", {0, 1}}, {"NE", {1, 1}}, {"E", {1, 0}}, {"SE", {1, -1}}, {"S", {0, -1}}, {"SW", {-1, -1}}, {"W", {-1, 0}}, {"NW", {-1, 1}}};
        auto printChoosing = [&]() {   // the choosing screen, as the player sees it (a new game)
            std::fprintf(stderr, "  [Who are you? Seventeen years old, of the farm at La Chapelle, near Paris.]\n");
            auto rows = g.createRows();
            for (size_t i = 0; i < rows.size(); i++)
                std::fprintf(stderr, "  %s %-14s %s\n", (int)i == g.createRow ? ">" : " ", rows[i].first.c_str(), rows[i].second.c_str());
            std::fprintf(stderr, "  [TYPE:yourname on the first line; PRESS:DOWN and PRESS:UP choose a line, PRESS:RIGHT and PRESS:LEFT change it; PRESS:ENTER begins]\n");
        };
        if (!o.play.empty() && g.creating) printChoosing();
        g.postSeen = g.minutes;
        // a band of brigands in the stand-in: on foot they come up to
        // you and stop you until you pay (a quarter of your purse) or refuse and fight; on horseback you ride wide of
        // them; and left standing at the end of a run, they are there at the start of the next
        auto bandParley = [&]() -> bool {
            if (g.bandState != 1) return false;
            int best = -1, bd = 1 << 30;
            for (int i : g.people.band) {
                const Person &pp = g.people.people[i];
                if (pp.away || pp.dead) continue;
                int d = std::max(std::abs(pp.x - g.tx), std::abs(pp.y - g.ty));
                if (d < bd) { bd = d; best = i; }
            }
            if (best < 0) return false;
            Person &pp = g.people.people[best];
            for (int dy = -1; dy <= 1 && bd > 1; dy++)
                for (int dx = -1; dx <= 1 && bd > 1; dx++) {
                    int x = g.tx + dx, y = g.ty + dy;
                    if ((dx || dy) && g.world.passable(x, y, 0) && !g.people.occupied(x, y, 0)) {
                        pp.x = pp.px = x; pp.y = pp.py = y; pp.z = pp.pz = 0; pp.path.clear(); pp.pathPos = 0; bd = 1;
                    }
                }
            g.people.parleyArrived = best;
            g.bandStep();
            return g.bandState == 2;
        };
        if (!o.play.empty() && g.people.player.count("band-waiting")) {
            g.people.player.erase("band-waiting");
            g.message.clear();
            g.wildRoll(true, "band");
            if (g.bandState == 1) {
                std::fprintf(stderr, "  [the men from the road are still here]\n");
                if (g.riding < 0) bandParley();
            }
        }
        while (ws >> tok) {
            if (g.sceneNow >= 0 && g.seqMode && tok.rfind("PRESS:", 0) != 0) {   // a beat's scenes, read through, before anything else is done
                for (int i = g.sceneNow + 1; i < (int)g.scenes.size(); i++)
                    std::fprintf(stderr, "  [scene %d of %zu] %s\n", i + 1, g.scenes.size(), g.scenes[i].caption.c_str());
                g.endScenes();
            }
            g.walkTb = g.turnBacks;
            g.squareCoins();   // the purse squared after whatever the last step paid or was paid
            g.postClock();     // the post's road time, after whatever the last command did
            if (tok == "PURSE") { std::fprintf(stderr, "  [money %s: %s]\n", Game::moneyString(g.money()).c_str(), g.purseString().c_str()); continue; }
            if (tok == "CLIMB") {
                // use the nearest ladder or stair from beside it, the way a player would: one of the building you
                // are in, or outdoors one out of doors (so that from inside the abbey of Saint-Denis it does not go
                // up the town walls, nor from the Bodleian up Carfax tower)
                bool done = false;
                auto bldOf = [&](int x, int y, int z) -> int {
                    if (z == 0) return g.world.tile(x, y).bld;
                    Level *lv = g.world.levelAt(x, y, z);
                    return lv ? lv->bld : 0;
                };
                int myBld = bldOf(g.tx, g.ty, g.tz);
                for (int r = 0; r < 30 && !done; r++)
                    for (int dy = -r; dy <= r && !done; dy++)
                        for (int dx = -r; dx <= r && !done; dx++) {
                            int lx = g.tx + dx, ly = g.ty + dy;
                            if (!g.world.linkAt(lx, ly, g.tz)) continue;
                            if (bldOf(lx, ly, g.tz) != myBld) continue;
                            for (int ddx = -1; ddx <= 1 && !done; ddx++)
                                for (int ddy = -1; ddy <= 1 && !done; ddy++) {
                                    if ((!ddx && !ddy) || (ddx && ddy)) continue;
                                    int sx = lx - ddx, sy = ly - ddy;
                                    if (!g.world.passable(sx, sy, g.tz)) continue;
                                    g.tx = sx; g.ty = sy;
                                    int z0 = g.tz;
                                    if (g.isClimb(lx, ly, g.tz)) g.useFeature(lx, ly, g.world.tileAt(lx, ly, g.tz)->feature);
                                    else g.tryStep(ddx, ddy, false);
                                    std::fprintf(stderr, "  climb from floor %d to floor %d: %s\n", z0, g.tz, g.message.c_str());
                                    done = true;
                                }
                        }
                if (!done) std::fprintf(stderr, myBld ? "  nothing to climb in this building\n" : "  nothing to climb near here\n");
                continue;
            }
            // tests of the people: TALKTO:name SAY:word BYE WAIT:minutes WHERE:name
            auto findPerson = [&](const std::string &n) {
                if (n == "@") return lastWho;   // whoever the last NEXTTO chose
                if (n.rfind("role:", 0) == 0) {   // role:beggar: the first person of that trade (role:merchant@Venice: in that town)
                    std::string r = n.substr(5), town;
                    if (r.find('@') != std::string::npos) { town = r.substr(r.find('@') + 1); r = r.substr(0, r.find('@')); }
                    for (char &c : town) if (c == '_') c = ' ';   // role:innkeeper@the_post_house_at_Ulm
                    for (size_t i = 0; i < g.people.people.size(); i++)
                        if (g.people.people[i].role == r && (town.empty() || g.people.people[i].town == town)) return (int)i;
                }
                if (n == "*")   // anyone with a long way still to walk
                    for (size_t i = 0; i < g.people.people.size(); i++) {
                        auto &pp = g.people.people[i];
                        if (pp.active && !pp.onRoad && pp.pathPos + 8 < pp.path.size()) return (int)i;
                    }
                for (size_t i = 0; i < g.people.people.size(); i++)   // an id: padua:debtor
                    if (g.people.people[i].id == n) return (int)i;
                std::string nn = n;
                for (char &c : nn) if (c == '_') c = ' ';
                for (size_t i = 0; i < g.people.people.size(); i++)
                    if (g.people.people[i].name.find(nn) != std::string::npos) return (int)i;
                return -1;
            };
            // ---- playing by commands (--play): what a player sees and does, in words
            // lower case, '_' as a space, and the accents of the names folded away ("WALKTO:Ecu" finds
            // the Écu de France, "Brozek" Brożek), for matching what is typed against names
            auto lowerOf = [](std::string x) {
                static const std::map<std::string, char> fold = {
                    {"à", 'a'}, {"á", 'a'}, {"â", 'a'}, {"ä", 'a'}, {"ã", 'a'}, {"å", 'a'}, {"ą", 'a'},
                    {"À", 'a'}, {"Á", 'a'}, {"Â", 'a'}, {"Ä", 'a'}, {"ç", 'c'}, {"ć", 'c'}, {"č", 'c'}, {"Ç", 'c'}, {"Č", 'c'},
                    {"è", 'e'}, {"é", 'e'}, {"ê", 'e'}, {"ë", 'e'}, {"ę", 'e'}, {"ě", 'e'}, {"È", 'e'}, {"É", 'e'}, {"Ê", 'e'},
                    {"ì", 'i'}, {"í", 'i'}, {"î", 'i'}, {"ï", 'i'}, {"ı", 'i'}, {"İ", 'i'}, {"ñ", 'n'}, {"ń", 'n'}, {"ň", 'n'},
                    {"ò", 'o'}, {"ó", 'o'}, {"ô", 'o'}, {"ö", 'o'}, {"õ", 'o'}, {"ø", 'o'}, {"ő", 'o'}, {"Ö", 'o'}, {"Ó", 'o'},
                    {"ù", 'u'}, {"ú", 'u'}, {"û", 'u'}, {"ü", 'u'}, {"ů", 'u'}, {"ű", 'u'}, {"Ü", 'u'}, {"ý", 'y'}, {"ÿ", 'y'},
                    {"ł", 'l'}, {"Ł", 'l'}, {"ś", 's'}, {"š", 's'}, {"ş", 's'}, {"ș", 's'}, {"Ś", 's'}, {"Š", 's'},
                    {"ź", 'z'}, {"ż", 'z'}, {"ž", 'z'}, {"Ż", 'z'}, {"Ž", 'z'}, {"ř", 'r'}, {"ť", 't'}, {"ţ", 't'}, {"ț", 't'},
                    {"ğ", 'g'}, {"ď", 'd'}};
                std::string o;
                for (size_t i = 0; i < x.size(); i++) {
                    unsigned char c = (unsigned char)x[i];
                    if (c >= 0xC0 && i + 1 < x.size()) {
                        auto f = fold.find(x.substr(i, 2));
                        if (f != fold.end()) { o += f->second; i++; continue; }
                    }
                    o += c == '_' ? ' ' : (char)std::tolower(c);
                }
                return o;
            };
            auto dirWord = [](int dx, int dy) {
                if (!dx && !dy) return std::string("here");
                double a = std::atan2((double)dy, (double)dx) * 180 / 3.14159265;
                static const char *n[8] = {"east", "north-east", "north", "north-west", "west", "south-west", "south", "south-east"};
                int k = (int)std::lround(a / 45.0);
                return std::string(n[((k % 8) + 8) % 8]);
            };
            auto curSettlement = [&]() {
                for (size_t i = 0; i < g.world.settlements.size(); i++) if (g.world.settlements[i].inside(g.tx, g.ty)) return (int)i;
                return -1;
            };
            // walk there step by step by the shortest way to any tile the goal accepts (the watch and
            // the searchers see you as you go); a few tries, for people standing in the way.
            // quietWalk: no "no way there" line (WALKTO trying the ferry boats one after another)
            bool quietWalk = false;
            auto walkTo = [&](std::function<bool(int, int)> goal, int limit) {
                if (g.turnBacks != g.walkTb) return false;   // turned back at a town's edge in this command: no more walking
                for (int attempt = 0; attempt < 6; attempt++) {
                    bool roundPeople = attempt < 2;   // first round them; then through them, waiting for them to move
                    if (goal(g.tx, g.ty)) return true;
                    std::map<std::pair<int, int>, std::pair<int, int>> prev;
                    std::deque<std::pair<int, int>> q{{g.tx, g.ty}};
                    prev[{g.tx, g.ty}] = {INT32_MIN, 0};
                    std::pair<int, int> end{INT32_MIN, 0};
                    while (!q.empty() && end.first == INT32_MIN && (int)prev.size() < limit) {
                        auto [x, y] = q.front(); q.pop_front();
                        for (int dy = -1; dy <= 1 && end.first == INT32_MIN; dy++)
                            for (int dx = -1; dx <= 1 && end.first == INT32_MIN; dx++) {
                                if (!dx && !dy) continue;
                                int nx = x + dx, ny = y + dy;
                                if (prev.count({nx, ny}) || !g.world.passable(nx, ny, g.tz)) continue;
                                if (dx && dy && (!g.world.passable(x + dx, y, g.tz) || !g.world.passable(x, y + dy, g.tz))) continue;
                                if (roundPeople && !goal(nx, ny) && g.people.occupied(nx, ny, g.tz) && std::max(std::abs(nx - g.tx), std::abs(ny - g.ty)) <= 6) continue;   // round people standing near you
                                prev[{nx, ny}] = {x, y};
                                if (goal(nx, ny)) end = {nx, ny};
                                q.push_back({nx, ny});
                            }
                    }
                    if (end.first == INT32_MIN) {
                        if (roundPeople) { attempt = 1; continue; }   // no way round the crowd: through it
                        if (!quietWalk) std::fprintf(stderr, "  [you cannot see a way there from here]\n");
                        return false;
                    }
                    std::vector<std::pair<int, int>> route;
                    for (std::pair<int, int> c = end; c.first != INT32_MIN; c = prev[c]) route.push_back(c);
                    std::reverse(route.begin(), route.end());
                    bool stuck = false;
                    std::string lastSaid;
                    for (size_t r = 1; r < route.size(); r++) {
                        int bx = g.tx, by = g.ty, bz = g.tz;
                        g.blockedCooldown = 0;
                        int tb0 = g.turnBacks;
                        g.tryStep(route[r].first - g.tx, route[r].second - g.ty, false);
                        g.stepT = 1; g.from = g.to;
                        for (int q2 = 0; q2 < 2; q2++) { g.people.update(g.world, 0.1f, g.minutes, g.tx, g.ty, g.tz); g.watchStep(); g.fight(0.1f); }
                        g.takeRequests();
                        if (g.turnBacks != tb0) {   // the warning at a town's edge: the walk stops, the next command is yours
                            if (!g.message.empty()) { std::fprintf(stderr, "  %s\n", g.message.c_str()); g.message.clear(); }
                            std::fprintf(stderr, "  [you turn back at the edge of %s (any walk into it within the hour goes on past the searchers and takes your chance; or wait for the dark)]\n", g.turnTown.c_str());
                            return false;
                        }
                        if (!g.people.notice.empty()) { g.say(g.people.notice); g.people.notice.clear(); }
                        if (!g.message.empty()) { if (g.message != lastSaid) std::fprintf(stderr, "  %s\n", g.message.c_str()); lastSaid = g.message; g.message.clear(); }
                        if (g.tx == bx && g.ty == by) { stuck = true; break; }
                        // a step that took you up or down (stairs, a ladder), or somewhere else: the walk ends there
                        // (on an upper floor too: Galileo in the Fiscal's rooms, upstairs at the Holy Office)
                        if (g.tz != bz || std::abs(g.tx - route[r].first) + std::abs(g.ty - route[r].second) > 0) return goal(g.tx, g.ty);
                    }
                    if (!stuck) return true;
                    for (int q = 0; q < 20; q++) g.people.update(g.world, 0.2f, g.minutes, g.tx, g.ty, g.tz);   // let whoever is in the way move on
                }
                return goal(g.tx, g.ty);
            };
            // The ferry boats within 40 steps (city.cc ferries: a link on the water tile where the boat waits,
            // "The ferryman rows you across to X."): nearest first. AROUND lists them as "a ferry boat to X"
            // and WALKTO:ferry, WALKTO:boat or WALKTO:X walks you into the nearest one that fits.
            // FerryNear: the boat's water tile (wx, wy), where it goes (dest), and the name of its own landing (at:
            // the boat at the far end rows you back to this landing, so its words name this one), the far
            // landing it puts you on (fx, fy). ferriesWithin(R): every ferry boat within R steps, from inside a
            // building too (WALKTO walks you out to it); nearFerries: within 40, outdoors, as AROUND lists them.
            struct FerryNear { int d, wx, wy; std::string dest, at; int fx, fy; };
            auto ferriesWithin = [&](int range) {
                std::vector<FerryNear> v;
                if (g.tz != 0) return v;
                static const std::string pre = "The ferryman rows you across to ";
                struct End { int sx, sy, tx, ty; std::string dest; };
                std::vector<End> ends;
                for (auto &s : g.world.linkSources) {
                    if (s[2] != 0) continue;
                    const Link *lk = g.world.linkAt(s[0], s[1], 0);
                    if (!lk || lk->verb.compare(0, pre.size(), pre) != 0) continue;
                    std::string dest = lk->verb.substr(pre.size());
                    if (!dest.empty() && dest.back() == '.') dest.pop_back();
                    ends.push_back({s[0], s[1], lk->x, lk->y, dest});
                }
                for (size_t i = 0; i < ends.size(); i++) {
                    const End &e = ends[i];
                    int d = std::max(std::abs(e.sx - g.tx), std::abs(e.sy - g.ty));
                    if (d > range) continue;
                    std::string at;
                    int bestD = 1 << 30;
                    for (size_t j = 0; j < ends.size(); j++) {
                        int dd = std::max(std::abs(ends[j].tx - e.sx), std::abs(ends[j].ty - e.sy));
                        if (j != i && dd < bestD) { bestD = dd; at = ends[j].dest; }
                    }
                    if (bestD > 12) at.clear();
                    v.push_back({d, e.sx, e.sy, e.dest, at, e.tx, e.ty});
                }
                std::sort(v.begin(), v.end(), [](const FerryNear &a, const FerryNear &b) { return a.d < b.d; });
                return v;
            };
            auto nearFerries = [&]() {
                if (g.world.tile(g.tx, g.ty).bld) return std::vector<FerryNear>();
                return ferriesWithin(40);
            };
            // walk to the landing beside the boat and step in; true if the boat took you
            auto takeFerry = [&](const FerryNear &f) {
                int bx = g.tx, by = g.ty;
                bool ok = walkTo([&](int x, int y) { return std::abs(x - f.wx) + std::abs(y - f.wy) == 1 && g.world.passable(x, y, 0); }, 250000);
                if (!ok) { std::fprintf(stderr, "  [you could not get to the ferry boat to %s]\n", f.dest.c_str()); return false; }
                g.blockedCooldown = 0;
                g.tryStep(f.wx - g.tx, f.wy - g.ty, false);
                g.stepT = 1; g.from = g.to;
                for (int q2 = 0; q2 < 2; q2++) { g.people.update(g.world, 0.1f, g.minutes, g.tx, g.ty, g.tz); g.watchStep(); g.fight(0.1f); }
                g.takeRequests();
                if (!g.message.empty()) { std::fprintf(stderr, "  %s\n", g.message.c_str()); g.message.clear(); }
                bool crossed = std::max(std::abs(g.tx - bx), std::abs(g.ty - by)) > 2;
                std::fprintf(stderr, "  [%s the ferry boat to %s: you are now at %d,%d (%s)]\n", crossed ? "you took" : "you could not board", f.dest.c_str(), g.tx, g.ty, g.place.c_str());
                return crossed;
            };
            // the first ferry boat of the list that fits and that you can walk to: take it (true); none: false, and
            // nothing said (the ones across the water from you are tried quietly)
            auto takeFirstFerry = [&](const std::vector<FerryNear> &fl, std::function<bool(const FerryNear &)> fits) {
                for (const FerryNear &f : fl) {
                    if (!fits(f)) continue;
                    quietWalk = true;
                    bool there = walkTo([&](int x, int y) { return std::abs(x - f.wx) + std::abs(y - f.wy) == 1 && g.world.passable(x, y, 0); }, 250000);
                    quietWalk = false;
                    if (there) { takeFerry(f); return true; }
                }
                return false;
            };
            // Who AROUND lists, shared with WALKTO and TALK so that "nun#2" is one person in all three.
            // Outdoors: the people within 14 steps on your floor (and not behind a wall of a building).
            // Inside a building: everyone in it on your floor, whatever the distance. Nearest first (then
            // by id); a label worn by several is numbered " #1", " #2"... in that order.
            struct Seen { int d, idx; std::string label; int k; };   // k: the number shown, 0 when the label is unique
            auto curBld = [&]() -> int {
                if (g.tz == 0) return g.world.tile(g.tx, g.ty).bld;
                Level *lv = g.world.levelAt(g.tx, g.ty, g.tz);
                return lv ? lv->bld : 0;
            };
            auto bldOfPerson = [&](const Person &pp) -> int {
                if (pp.z == 0) return g.world.tile(pp.x, pp.y).bld;
                Level *lv = g.world.levelAt(pp.x, pp.y, pp.z);
                return lv ? lv->bld : 0;
            };
            auto aroundPeople = [&]() {
                std::vector<Seen> v;
                int cb = curBld();
                bool whole = cb > 0;   // in a building: all of it, on this floor
                int myBld = g.tz == 0 ? cb : -1;   // outdoors, as before: the floor's tiles outside any building
                bool roomless = false;   // a building with no room inside (the Alte Veste: walls and a gate): the people just outside it too
                if (whole && g.tz == 0) {
                    const Building &b = g.world.buildings[cb - 1];
                    int open = 0;
                    for (int Y = b.y0; Y <= b.y1 && open <= 4; Y++)
                        for (int X = b.x0; X <= b.x1 && open <= 4; X++)
                            if (g.world.tile(X, Y).bld == cb && g.world.passable(X, Y, 0)) open++;
                    roomless = open <= 4;
                }
                for (size_t i = 0; i < g.people.people.size(); i++) {
                    auto &pp = g.people.people[i];
                    if (pp.away || pp.dead || !pp.placed || pp.z != g.tz) continue;
                    int d = std::max(std::abs(pp.x - g.tx), std::abs(pp.y - g.ty));
                    if (whole) { if (bldOfPerson(pp) != cb && !(roomless && d <= 14 && g.world.tile(pp.x, pp.y).bld == 0)) continue; }
                    else {
                        if (d > 14) continue;
                        if (myBld >= 0 && g.world.tile(pp.x, pp.y).bld != myBld) continue;   // behind a wall
                    }
                    v.push_back({d, (int)i, g.people.label(pp), 0});
                }
                std::sort(v.begin(), v.end(), [](const Seen &a, const Seen &b) { return a.d != b.d ? a.d < b.d : a.idx < b.idx; });
                std::map<std::string, int> count, seen;
                for (auto &e : v) count[e.label]++;
                for (auto &e : v) if (count[e.label] > 1) e.k = ++seen[e.label];
                return v;
            };
            // "want#nth": the person AROUND shows whose label holds WANT and whose shown number is NTH;
            // when no one is numbered so, the NTH match in AROUND's order; -1 if AROUND does not show one.
            auto aroundNth = [&](const std::string &want, int nth) -> int {
                auto v = aroundPeople();
                int n = 0, match = -1;
                for (auto &e : v) {
                    const Person &pp = g.people.people[e.idx];
                    if (!want.empty() && lowerOf(e.label).find(want) == std::string::npos && lowerOf(pp.title).find(want) == std::string::npos) continue;
                    if (e.k == nth) return e.idx;
                    if (++n == nth && match < 0) match = e.idx;
                }
                return match;
            };
            // the test commands are not in HELP, so that players do not see them:
            // ROAD:none|band|wolf|bear|chance, SEARCH:1|0, POSTEVENT:..., MONEY:, GIVEME:, SKIP:, KNOW, MEM:, WHERE:,
            // USEAT:, TOWATER, LOOKPICK, MOUSE: (each is marked "tests" where it is read, below)
            if (tok == "HELP") {
                std::fprintf(stderr,
                    "  [commands, one or more at a time, separated by spaces; use _ for a space inside one]\n"
                    "    AROUND            what you see: the time, the place, people, buildings, things (outdoors those\n"
                    "                      within 14 steps; in a building everyone in it on your floor, and its things)\n"
                    "    WALKTO:name       walk to a person, building or thing you can see (part of its name)\n"
                    "    OUT               out of the building you are in, by its door\n"
                    "    N5 E3 SW2 ...     walk that many steps north, east, south-west...\n"
                    "    MAP               the towns, which way they lie and how far by road\n"
                    "    TRAVEL:Town       go by road to another town or post house, stage by stage (time passes; no road\n"
                    "                      over the sea: for that you need a boat or a ship). Wolves or a bear may stop you\n"
                    "                      on the open road: TRAVEL again to go on. A band of brigands stops you on foot\n"
                    "                      until you SAY:pay (a quarter of your purse) or SAY:no and FIGHT:brigand; on\n"
                    "                      horseback you ride wide of them\n"
                    "    POST:Town         the post ride: from an innkeeper or ostler within a few steps, ask for the ride to\n"
                    "                      Town, hear the fare, and take it (two stills, the clock moves, you arrive at its inn;\n"
                    "                      now and then the ride is stopped on the road). TRAVEL is the full road, at no fare\n"
                    "    TALK:name         start talking to someone beside you (part of their name or trade;\n"
                    "                      nun#2 is the nun AROUND numbers 2; WALKTO:nun#2 too)\n"
                    "    SAY:words         say words to them, e.g. SAY:name, SAY:job, SAY:buy_bread,\n"
                    "                      SAY:give_letter, SAY:give_2_sous (one word at a time works best)\n"
                    "    BYE               end the conversation\n"
                    "    GET:thing  DROP:thing  USE:thing  LOOKAT:thing   (a thing's name or id)\n"
                    "    INV               what you carry     STORIES   your journal of stories\n"
                    "    STORIES:detailed  the journal marked (open, done well or badly), the newest first; STORIES:plain\n"
                    "    TURN  TURNBACK    the next or last page of an open book, or of the journal (for its picture)\n"
                    "    PLACES            the towns you have been to: the places you went into, the people you spoke to\n"
                    "    CLIMB             use the nearest ladder or stair\n"
                    "    WAIT:minutes      let time pass (a wait of more than an hour goes in coarse steps: the events\n"
                    "                      run, people are put where the hour puts them, not walked)\n"
                    "    WAIT:news         let time pass until word comes (a news line told to you), at most 14 days\n"
                    "    PRESS:KEY TYPE:words   on the choosing screen of a new game, and in its opening\n"
                    "                      scenes (PRESS:ENTER: the next scene; PRESS:ESCAPE: skip them)\n"
                    "    HELP              this list\n");
                continue;
            }
            if (tok == "MAP") {   // what the map (M) shows: the towns, how far by road, and which way
                g.showMap = true;   // and opens it, for a picture taken after (as STORIES opens the journal)
                int from = curSettlement();
                if (from < 0) {
                    int bd2 = 1 << 30;
                    for (size_t i = 0; i < g.world.settlements.size(); i++) {
                        auto &st = g.world.settlements[i];
                        int d = std::max(std::abs(st.x0 + st.w / 2 - g.tx), std::abs(st.y0 + st.h / 2 - g.ty));
                        if (d < bd2) { bd2 = d; from = (int)i; }
                    }
                }
                auto dist = g.world.roadDistances(from);
                std::vector<std::pair<float, std::string>> tl;
                for (size_t i = 0; i < g.world.settlements.size(); i++) {
                    auto &st = g.world.settlements[i];
                    if ((int)i == from) continue;
                    int straight = std::max(std::abs(st.x0 + st.w / 2 - g.tx), std::abs(st.y0 + st.h / 2 - g.ty));
                    // hamlets (farms, villages, a market) only near by; the post houses on the roads not at all
                    if (st.hamlet && (st.file.find("posthouse") != std::string::npos || straight > 600)) continue;
                    float d = dist[i];
                    if (d > 0 && d < 1e8f) d += (g.world.settlements[from].w + g.world.settlements[from].h) * 0.25f + (st.w + st.h) * 0.25f;   // as TRAVEL counts it
                    std::string w = dirWord(st.x0 + st.w / 2 - g.tx, st.y0 + st.h / 2 - g.ty);
                    std::string how = d > 0 && d < 1e8f ? std::to_string((int)d) + " steps by road"
                                    : straight <= 200 ? "no road: on foot across the country, about " + std::to_string(straight) + " steps"
                                    : std::string("no road: over the sea");
                    tl.push_back({d > 0 && d < 1e8f ? d : straight <= 200 ? (float)straight : 1e9f, st.name + " (" + w + ", " + how + ")"});
                }
                std::sort(tl.begin(), tl.end());
                std::fprintf(stderr, "  [the map] From %s: ", g.world.settlements[from].name.c_str());
                for (size_t i = 0; i < tl.size(); i++) std::fprintf(stderr, "%s%s", i ? "; " : "", tl[i].second.c_str());
                std::fprintf(stderr, ".\n  (Post houses and villages on the roads have names too; AROUND shows the nearest when you are in the country.)\n");
                continue;
            }
            if (tok == "AROUND") {
                int si = curSettlement();
                std::string where = g.place.empty() ? "outdoors" : "in " + g.place;
                if (si >= 0 && g.place == g.world.settlements[si].name) where = "in the street";
                bool named = si >= 0 && where.find(g.world.settlements[si].name) != std::string::npos;
                std::fprintf(stderr, "  [around] %s. You are %s%s%s.\n", dateString(g.minutes).c_str(), where.c_str(),
                             g.tz > 0 ? " (upstairs)" : g.tz < 0 ? " (below ground)" : "",
                             si >= 0 && !named ? (", " + g.world.settlements[si].name).c_str() : "");
                std::vector<std::pair<int, std::string>> ppl;
                int myBld = curBld();
                bool inBld = myBld > 0;   // in a building: the whole of it on this floor
                for (auto &e : aroundPeople()) {
                    const Person &pp = g.people.people[e.idx];
                    ppl.push_back({e.d, e.label + (e.k ? " #" + std::to_string(e.k) : "") + " (" + std::to_string(e.d) + " " + (e.d == 1 ? "step" : "steps") + " " + dirWord(pp.x - g.tx, pp.y - g.ty) + (pp.lying ? ", lying down" : "") + ")"});
                }
                size_t pcap = inBld ? 20 : 12;
                if (ppl.size() > pcap) { std::fprintf(stderr, "  (%zu people here; the nearest %zu are listed)\n", ppl.size(), pcap); }
                std::string pl;
                for (size_t i = 0; i < ppl.size() && i < pcap; i++) pl += (i ? "; " : "") + ppl[i].second;
                std::fprintf(stderr, "  People near you: %s.\n", pl.empty() ? "nobody" : pl.c_str());
                std::vector<std::pair<int, std::string>> bl;
                std::map<std::string, int> nearestOf;   // a building drawn in parts (a bridge's houses): once, the nearest part
                for (size_t i = 0; i < g.world.buildings.size(); i++) {
                    const Building &b = g.world.buildings[i];
                    if (b.name.empty()) continue;
                    int d0 = std::max(std::abs((b.x0 + b.x1) / 2 - g.tx), std::abs((b.y0 + b.y1) / 2 - g.ty));
                    auto it = nearestOf.find(b.name);
                    if (it == nearestOf.end() || d0 < it->second) nearestOf[b.name] = d0;
                }
                for (size_t i = 0; i < g.world.buildings.size(); i++) {
                    const Building &b = g.world.buildings[i];
                    if (b.name.empty()) continue;
                    if (std::max(std::abs((b.x0 + b.x1) / 2 - g.tx), std::abs((b.y0 + b.y1) / 2 - g.ty)) != nearestOf[b.name]) continue;
                    nearestOf[b.name] = -1;   // and only one part at that distance
                    int cx = (b.x0 + b.x1) / 2, cy = (b.y0 + b.y1) / 2;
                    int d = std::max(std::abs(cx - g.tx), std::abs(cy - g.ty));
                    if (d > 45) continue;
                    bool in = g.tx >= b.x0 && g.tx <= b.x1 && g.ty >= b.y0 && g.ty <= b.y1;
                    // a building of another place than the one you are in, with that place ("the farmhouse, at La
                    // Chapelle", seen from the Porte Saint-Denis)
                    std::string of;
                    for (const auto &st : g.world.settlements)
                        if (st.inside(cx, cy)) {
                            if (lowerS(st.name) != lowerS(g.curTown)) {
                                of = st.name;
                                size_t at = of.rfind(" at ");
                                if (at != std::string::npos) of = of.substr(at + 4);
                                of = ", at " + of;
                            }
                            break;
                        }
                    bl.push_back({d, b.name + of + (in ? " (you are in it)" : " (" + std::to_string(d) + " steps " + dirWord(cx - g.tx, cy - g.ty) + ")")});
                }
                std::sort(bl.begin(), bl.end());
                std::string bs;
                for (size_t i = 0; i < bl.size() && i < 10; i++) bs += (i ? "; " : "") + bl[i].second;
                std::fprintf(stderr, "  Buildings you can see: %s.\n", bs.empty() ? "none with a name" : bs.c_str());
                std::string th;
                std::set<std::string> seenK;
                {
                    int X0 = g.tx - 4, X1 = g.tx + 4, Y0 = g.ty - 4, Y1 = g.ty + 4;
                    Level *myLv = g.tz != 0 ? g.world.levelAt(g.tx, g.ty, g.tz) : nullptr;
                    if (inBld) {   // the whole building, on this floor
                        if (g.tz == 0) { const Building &bb = g.world.buildings[myBld - 1]; X0 = bb.x0; X1 = bb.x1; Y0 = bb.y0; Y1 = bb.y1; }
                        else if (myLv) { X0 = myLv->x0; X1 = myLv->x0 + myLv->w - 1; Y0 = myLv->y0; Y1 = myLv->y0 + myLv->h - 1; }
                    }
                    for (int Y = Y0; Y <= Y1; Y++)
                        for (int X = X0; X <= X1; X++) {
                            if (inBld && g.tz != 0 && g.world.levelAt(X, Y, g.tz) != myLv) continue;
                            for (int i : g.items.at(X, Y, g.tz)) {
                                const ItemKind &k = g.items.kinds[g.items.things[i].kind];
                                if (seenK.count(k.id)) continue;
                                if (g.tz == 0 && myBld >= 0 && g.world.tile(X, Y).bld != myBld) continue;   // not in this room
                                seenK.insert(k.id);
                                th += (th.empty() ? "" : "; ") + k.name + " [" + k.id + "]" + (k.fixed ? " (fixed)" : "");
                            }
                        }
                }
                for (auto &f : nearFerries()) {   // ferry boats: only the nearest to each place
                    std::string one = "a ferry boat to " + f.dest + " (" + std::to_string(f.d) + (f.d == 1 ? " step " : " steps ") + dirWord(f.wx - g.tx, f.wy - g.ty) + ")";
                    if (th.find("a ferry boat to " + f.dest + " (") == std::string::npos) th += (th.empty() ? "" : "; ") + one;
                }
                std::fprintf(stderr, "  Things near you: %s.\n", th.empty() ? "nothing you can pick out" : th.c_str());
                if (si < 0) {   // in the country: the towns nearest
                    std::vector<std::pair<int, std::string>> tl;
                    for (auto &st : g.world.settlements) {
                        int d = std::max(std::abs(st.x0 + st.w / 2 - g.tx), std::abs(st.y0 + st.h / 2 - g.ty));
                        tl.push_back({d, st.name + " (" + std::to_string(d) + " steps " + dirWord(st.x0 + st.w / 2 - g.tx, st.y0 + st.h / 2 - g.ty) + ")"});
                    }
                    std::sort(tl.begin(), tl.end());
                    std::string ts;
                    for (size_t i = 0; i < tl.size() && i < 4; i++) ts += (i ? "; " : "") + tl[i].second;
                    std::fprintf(stderr, "  Nearest places: %s.\n", ts.c_str());
                }
                std::fprintf(stderr, "  You have %s.%s\n", g.money() ? Game::moneyString(g.money()).c_str() : "no money", g.riding >= 0 ? " You are on horseback." : "");
                continue;
            }
            if (tok == "OUT") {   // out of the building you are in, by its nearest door, into the street
                if (g.tz != 0) { std::fprintf(stderr, "  [you are upstairs (or below): CLIMB down first]\n"); continue; }
                if (!g.world.tile(g.tx, g.ty).bld) { std::fprintf(stderr, "  [you are out of doors already]\n"); continue; }
                if (g.people.talk.who >= 0) { g.people.endTalk(g.minutes); g.talking = false; }
                if (walkTo([&](int x, int y) { return !g.world.tile(x, y).bld; }, 20000)) std::fprintf(stderr, "  [you are outside, in the street]\n");
                continue;
            }
            if (tok.rfind("WALKTO:", 0) == 0) {
                std::string want = lowerOf(tok.substr(7));
                lastWalkWant = want;
                int nth = 1;   // "nun#2": the second nearest nun
                bool numbered = want.find('#') != std::string::npos;
                if (numbered) { nth = std::max(1, std::atoi(want.c_str() + want.find('#') + 1)); want = want.substr(0, want.find('#')); }
                if (g.people.talk.who >= 0) { g.people.endTalk(g.minutes); g.talking = false; }
                // a person you can see, then a building, then a thing
                int best = -1, bd = 1 << 30;
                if (numbered) best = aroundNth(want, nth);   // the one AROUND shows with that number
                if (best >= 0) bd = std::max(std::abs(g.people.people[best].x - g.tx), std::abs(g.people.people[best].y - g.ty));
                else {
                    std::vector<std::pair<int, int>> m;
                    for (size_t i = 0; i < g.people.people.size(); i++) {
                        auto &pp = g.people.people[i];
                        if (pp.away || pp.dead || !pp.placed || pp.z != g.tz) continue;
                        if (lowerOf(g.people.label(pp)).find(want) == std::string::npos && lowerOf(pp.title).find(want) == std::string::npos) continue;
                        bool viaPlace = false;   // "orso" names the inn, not "a maid of the Osteria dell'Orso": the building, then
                        for (const Building &bw : g.world.buildings)
                            if (!bw.name.empty() && lowerOf(bw.name).find(want) != std::string::npos && (lowerOf(g.people.label(pp)).find(lowerOf(bw.name)) != std::string::npos || lowerOf(pp.title).find(lowerOf(bw.name)) != std::string::npos) &&
                                lowerOf(pp.name).find(want) == std::string::npos) { viaPlace = true; break; }
                        if (viaPlace) continue;
                        int d = std::max(std::abs(pp.x - g.tx), std::abs(pp.y - g.ty));
                        // one whose title or name BEGINS with the words first ("King" is the King of Sweden, not "a Scots
                        // officer of the King of Sweden" standing nearer), or whose name holds them
                        auto bare = [](std::string n) {
                            for (const char *a : {"the ", "a ", "an "}) if (n.rfind(a, 0) == 0) return n.substr(std::strlen(a));
                            return n;
                        };
                        bool begins = bare(lowerOf(g.people.label(pp))).rfind(want, 0) == 0 || bare(lowerOf(pp.title)).rfind(want, 0) == 0 ||
                                      (!pp.name.empty() && lowerOf(pp.name).find(want) != std::string::npos && g.people.label(pp) == pp.name);
                        if (d <= 60) m.push_back({d + (begins ? 0 : 1000), (int)i});
                    }
                    std::sort(m.begin(), m.end());
                    if ((int)m.size() >= nth) { best = m[nth - 1].second; bd = std::max(std::abs(g.people.people[best].x - g.tx), std::abs(g.people.people[best].y - g.ty)); }
                }
                bool ok = false;
                if (best >= 0) {
                    int who = best;
                    lastWho = who;   // TALK then means this one, even if another like them is nearer
                    quietWalk = true;
                    for (int tries = 0; tries < 3; tries++) {   // a crowd can stop a walk half way (the Orso of an evening): on from there
                        int sx = g.tx, sy = g.ty, sz = g.tz;
                        ok = walkTo([&](int x, int y) { auto &pp = g.people.people[who]; return std::max(std::abs(pp.x - x), std::abs(pp.y - y)) <= 1; }, 250000);
                        if (ok || (g.tx == sx && g.ty == sy && g.tz == sz)) break;
                    }
                    quietWalk = false;
                    {   // not beside, but within TALK's reach (two steps: someone or something between): say so, not "could not get to"
                        auto &pw = g.people.people[who];
                        int dw = std::max(std::abs(pw.x - g.tx), std::abs(pw.y - g.ty));
                        if (!ok && dw <= 2 && pw.z == g.tz) std::fprintf(stderr, "  [you are near %s: near enough to talk]\n", g.people.label(pw).c_str());
                        else std::fprintf(stderr, "  [%s %s]\n", ok ? "you are beside" : "you could not get to", g.people.label(pw).c_str());
                    }
                    continue;
                }
                // a ferry boat: "ferry", the words AROUND shows ("a ferry boat to the Lido"), or where it goes
                std::string fwant = want;
                for (const char *pre : {"a ferry boat to ", "the ferry boat to ", "ferry boat to ", "the ferry to ", "ferry to ", "a boat to ", "the boat to ", "boat to "})
                    if (fwant.rfind(pre, 0) == 0) { fwant = fwant.substr(std::strlen(pre)); break; }
                bool boatWords = fwant != want;   // "a ferry boat to X": the boat, not the landing of X
                auto goesTo = [&](const FerryNear &f) { return fwant.size() >= 3 && lowerOf(f.dest).find(fwant) != std::string::npos; };
                // by a ferry's names, among the boats FL: a landing of that name you can walk to (said: "WALKTO:riva",
                // or "WALKTO:murano" on Murano), else a boat you can walk to that goes to a place of that name (taken)
                auto ferryByName = [&](const std::vector<FerryNear> &fl) {
                    if (fwant.size() < 3) return false;
                    if (!boatWords)
                        for (const FerryNear &f : fl) {
                            if (f.at.empty() || lowerOf(f.at).find(fwant) == std::string::npos) continue;
                            quietWalk = true;
                            bool there = walkTo([&](int x, int y) { return std::abs(x - f.wx) + std::abs(y - f.wy) == 1 && g.world.passable(x, y, 0); }, 250000);
                            quietWalk = false;
                            if (there) { std::fprintf(stderr, "  [you are at %s, by the ferry boat to %s]\n", f.at.c_str(), f.dest.c_str()); return true; }
                        }
                    return takeFirstFerry(fl, goesTo);
                };
                if (want == "ferry" || want == "boat" || want == "ferry boat" || want == "ferryboat") {   // the nearest ferry boat
                    auto fl = ferriesWithin(40);   // from inside a building too: you walk out to it
                    std::vector<std::pair<int, int>> boats;   // "boat" is a rowing boat or a gondola at its post too, if nearer
                    if (want == "boat")
                        for (size_t i = 0; i < g.items.things.size(); i++) {
                            const Thing &t = g.items.things[i];
                            if (t.gone || t.z != g.tz || !g.items.kinds[t.kind].boat || g.items.kinds[t.kind].shape == "ship") continue;
                            int d = std::max(std::abs(t.x - g.tx), std::abs(t.y - g.ty));
                            if (d <= 40 && (fl.empty() || d < fl[0].d)) boats.push_back({d, (int)i});
                        }
                    std::sort(boats.begin(), boats.end());
                    bool done = false;
                    for (auto &bq : boats) {   // the nearest you can walk to
                        int bi = bq.second;
                        quietWalk = true;
                        bool there = walkTo([&](int x, int y) { const Thing &t = g.items.things[bi]; return std::max(std::abs(t.x - x), std::abs(t.y - y)) <= 1; }, 100000);
                        quietWalk = false;
                        if (!there) continue;
                        const Thing &t = g.items.things[bi];
                        std::fprintf(stderr, "  [you are beside %s, one step %s: step into it to get in]\n", g.items.kinds[t.kind].name.c_str(), dirWord(t.x - g.tx, t.y - g.ty).c_str());
                        done = true;
                        break;
                    }
                    if (done) continue;
                    if (takeFirstFerry(fl, [](const FerryNear &) { return true; })) continue;
                }
                // a ferry boat near you, by its names ("WALKTO:lido" at the Riva degli Schiavoni)
                if (ferryByName(nearFerries())) continue;
                int bb = -1; bd = 1 << 30;
                int here = curSettlement();
                for (size_t i = 0; i < g.world.buildings.size(); i++) {
                    const Building &b = g.world.buildings[i];
                    if (b.name.empty() || lowerOf(b.name).find(want) == std::string::npos) continue;
                    int bx = (b.x0 + b.x1) / 2, by = (b.y0 + b.y1) / 2;
                    int d = std::max(std::abs(bx - g.tx), std::abs(by - g.ty));
                    // in this town, or one near by in sight (the farmhouse at La Chapelle, from the Porte Saint-Denis), this town's first
                    if (here >= 0 && !g.world.settlements[here].inside(bx, by)) { if (d > 50) continue; d += 60; }
                    if (d > 120) continue;
                    // a building that is not a tower or a gate first ("Tre_Kronor" is the castle, not the tower of Tre Kronor)
                    int rank = d + ((b.kind == "tower" || b.kind == "gate") ? 200 : 0);
                    {   // the name itself first, with or without its article ("Alte_Veste" is the ruin, not "a farm by the Alte Veste")
                        auto bare = [](std::string n) {
                            for (const char *a : {"the ", "a ", "an "}) if (n.rfind(a, 0) == 0) return n.substr(std::strlen(a));
                            return n;
                        };
                        if (bare(lowerOf(b.name)) == bare(want)) rank -= 1000;
                    }
                    if (rank < bd) { bd = rank; bb = (int)i; }
                }
                if (bb >= 0) {
                    bool in = false;
                    auto walkIn = [&]() {
                        auto inside = [&](int x, int y) { return g.tz == 0 && g.world.tile(x, y).bld == bb + 1; };
                        ok = walkTo(inside, 250000);
                        in = ok;
                        if (!ok) {   // beside it; then in again from there (a far building's door can lie beyond the first search's reach)
                            ok = walkTo([&](int x, int y) { const Building &b = g.world.buildings[bb]; return x >= b.x0 - 1 && x <= b.x1 + 1 && y >= b.y0 - 1 && y <= b.y1 + 1; }, 250000);
                            if (ok) in = walkTo(inside, 250000);
                        }
                    };
                    quietWalk = true;   // across the water ("Santi Maria e Donato" from Venice): said below if no boat goes
                    for (int tries = 0; tries < 3; tries++) {   // a crowd can stop a walk half way (a market street in Rome): on from there
                        int sx = g.tx, sy = g.ty;
                        walkIn();
                        if (ok || g.tz != 0 || (g.tx == sx && g.ty == sy)) break;
                    }
                    quietWalk = false;
                    if (!ok) {   // the ferry boats of this town whose far landing is nearer the building than their own, nearest it first; then on
                        const Building &b = g.world.buildings[bb];
                        int bx = (b.x0 + b.x1) / 2, by = (b.y0 + b.y1) / 2;
                        auto fl = ferriesWithin(200);
                        auto farD = [&](const FerryNear &f) { return std::max(std::abs(f.fx - bx), std::abs(f.fy - by)); };
                        std::vector<FerryNear> over;
                        for (auto &f : fl)
                            if (farD(f) <= 80 && farD(f) < std::max(std::abs(f.wx - bx), std::abs(f.wy - by))) over.push_back(f);
                        std::sort(over.begin(), over.end(), [&](const FerryNear &p, const FerryNear &q) { return farD(p) < farD(q); });
                        if (takeFirstFerry(over, [](const FerryNear &) { return true; })) walkIn();
                        else if (g.tz != 0) std::fprintf(stderr, "  [you are upstairs (or below): CLIMB down first]\n");
                        else if (g.riding >= 0) std::fprintf(stderr, "  [you are on horseback, and nobody rides indoors: USEANIMAL:horse gets you down]\n");
                        else std::fprintf(stderr, "  [you cannot see a way there from here]\n");
                    }
                    std::fprintf(stderr, "  [%s %s%s]\n", in ? "you are in" : ok ? "you are beside" : "you could not get to", g.world.buildings[bb].name.c_str(),
                                 !in && ok ? " (you found no way in: it may be locked, or its door is on a side you cannot reach from here)" : "");
                    continue;
                }
                // a ferry boat anywhere in this town, by its names ("WALKTO:fondamenta_nuove", "WALKTO:murano")
                if (ferryByName(ferriesWithin(200))) continue;
                int ti = -1; bd = 1 << 30;
                for (size_t i = 0; i < g.items.things.size(); i++) {
                    const Thing &t = g.items.things[i];
                    if (t.gone || t.z != g.tz) continue;
                    const ItemKind &k = g.items.kinds[t.kind];
                    if (lowerOf(k.name).find(want) == std::string::npos && lowerOf(k.id).find(want) == std::string::npos) continue;
                    int d = std::max(std::abs(t.x - g.tx), std::abs(t.y - g.ty));
                    if (d < bd && d <= 40) { bd = d; ti = (int)i; }
                }
                if (ti >= 0) {
                    ok = walkTo([&](int x, int y) { const Thing &t = g.items.things[ti]; return std::max(std::abs(t.x - x), std::abs(t.y - y)) <= 1; }, 100000);
                    std::fprintf(stderr, "  [%s %s]\n", ok ? "you are beside" : "you could not get to", g.items.kinds[g.items.things[ti].kind].name.c_str());
                    continue;
                }
                std::fprintf(stderr, "  [you see nobody and nothing called \"%s\" near here]\n", want.c_str());
                continue;
            }
            if (tok.rfind("TALK:", 0) == 0) {
                std::string want = lowerOf(tok.substr(5));
                int nth = 0;   // "nun#2": the second nearest nun, counted as AROUND counts them
                if (want.find('#') != std::string::npos) { nth = std::max(1, std::atoi(want.c_str() + want.find('#') + 1)); want = want.substr(0, want.find('#')); }
                int best = -1, bd = 3;
                {
                    std::vector<std::pair<int, int>> m;
                    for (size_t i = 0; i < g.people.people.size(); i++) {
                        auto &pp = g.people.people[i];
                        if (pp.away || pp.dead || !pp.placed || pp.z != g.tz) continue;
                        if (!want.empty() && lowerOf(g.people.label(pp)).find(want) == std::string::npos && lowerOf(pp.title).find(want) == std::string::npos) continue;
                        int d = std::max(std::abs(pp.x - g.tx), std::abs(pp.y - g.ty));
                        if (d <= 14) m.push_back({d, (int)i});
                    }
                    std::sort(m.begin(), m.end());
                    bool justWalked = false;   // beside the one WALKTO:same words brought you to: he stays him, though AROUND now numbers the people round you anew
                    for (auto &e : m) if (lowerOf(tok.substr(5)) == lastWalkWant && e.second == lastWho && e.first < bd) justWalked = true;
                    if (nth > 0 && !justWalked) {   // "nun#2": the one AROUND numbers 2, if he is within reach
                        int an = aroundNth(want, nth);
                        if (an >= 0) {
                            m.clear();
                            auto &q = g.people.people[an];
                            m.push_back({std::max(std::abs(q.x - g.tx), std::abs(q.y - g.ty)), an});
                            nth = 1;
                        }
                    }
                    // the one you have just walked to, if it answers to the name; also by the same
                    // number ("WALKTO:merchant#2 TALK:merchant#2": beside him he is now the nearest)
                    for (auto &e : m)
                        if ((nth == 0 || lowerOf(tok.substr(5)) == lastWalkWant) && e.second == lastWho && e.first < bd) { best = e.second; break; }
                    if (best >= 0) {}
                    else if (nth == 0) { if (!m.empty() && m[0].first < bd) best = m[0].second; }
                    else if ((int)m.size() >= nth && m[nth - 1].first < bd) best = m[nth - 1].second;
                    else if ((int)m.size() >= nth) { std::fprintf(stderr, "  [that one is %d steps away: WALKTO:%s#%d first]\n", m[nth - 1].first, want.c_str(), nth); continue; }
                }
                if (best < 0) { std::fprintf(stderr, "  [nobody like that is beside you: WALKTO them first]\n"); continue; }
                Person &pp = g.people.people[best];
                g.people.startTalk(g.world, best, g.minutes);
                g.talking = true;
                std::fprintf(stderr, "  [talking to %s]\n", g.people.label(pp).c_str());
                for (auto &l : g.people.talk.lines) std::fprintf(stderr, "    %s\n", l.second.c_str());
                bool ended = g.people.talk.over;
                g.startRequestedPiece();   // what a greeting asks for (a portrait painted, a piece played)
                if (ended || g.people.talk.who < 0) std::fprintf(stderr, "  [that ends the conversation: TALK again to say more]\n");
                continue;
            }
            if (tok.rfind("POST:", 0) == 0) {   // the post ride, as said to an innkeeper or an ostler: ride TOWN, then ride
                std::string town = tok.substr(5);
                for (auto &c : town) if (c == '_') c = ' ';
                int best = -1, bd = 6;
                for (size_t i = 0; i < g.people.people.size(); i++) {
                    auto &pp = g.people.people[i];
                    if ((pp.role != "innkeeper" && pp.role != "ostler") || pp.away || pp.dead || !pp.placed || pp.z != g.tz) continue;
                    int d = std::max(std::abs(pp.x - g.tx), std::abs(pp.y - g.ty));
                    if (d < bd) { bd = d; best = (int)i; }
                }
                if (best < 0) { std::fprintf(stderr, "  [no innkeeper or ostler within a few steps: WALKTO:innkeeper first]\n"); continue; }
                if (g.people.talk.who >= 0) { g.people.endTalk(g.minutes); g.talking = false; }
                g.people.startTalk(g.world, best, g.minutes);
                g.talking = true;
                std::fprintf(stderr, "  [talking to %s]\n", g.people.label(g.people.people[best]).c_str());
                for (const std::string &say : {"ride " + town, std::string("ride")}) {
                    if (g.people.talk.who < 0 || g.people.talk.over) break;
                    size_t before = g.people.talk.lines.size();
                    g.people.ask(g.world, say, g.minutes, g.tx, g.ty);
                    for (size_t k = before; k < g.people.talk.lines.size(); k++)
                        std::fprintf(stderr, "    %s%s\n", g.people.talk.lines[k].first ? "" : "> ", g.people.talk.lines[k].second.c_str());
                    g.startRequestedPiece();
                }
                if (g.sceneNow >= 0 && g.seqMode) {   // the stills, read through
                    for (int i = g.sceneNow + 1; i < (int)g.scenes.size(); i++)
                        std::fprintf(stderr, "  [scene %d of %zu] %s\n", i + 1, g.scenes.size(), g.scenes[i].caption.c_str());
                    g.endScenes();
                }
                if (g.people.talk.who >= 0) { g.people.endTalk(g.minutes); g.talking = false; }
                std::fprintf(stderr, "  [you are at %s, on %s]\n", g.place.empty() ? "the open road" : g.place.c_str(), dateString(g.minutes).c_str());
                if (!g.message.empty()) { std::fprintf(stderr, "  %s\n", g.message.c_str()); g.message.clear(); }
                continue;
            }
            if (tok.rfind("TRAVEL:", 0) == 0) {
                if (g.bandState >= 1 && g.bandState <= 3) {   // men on the road: a rider gets away, one on foot does not
                    if (g.riding >= 0) { g.disband(); std::fprintf(stderr, "  [you turn your horse off the road and ride wide of the men: they do not follow a horse]\n"); }
                    else { std::fprintf(stderr, "  [the men on the road are in your way: pay them (SAY:pay), or refuse (SAY:no) and FIGHT:brigand]\n"); continue; }
                }
                auto bare = [&](const std::string &x) {   // letters and digits only: "Saint_Denis" is Saint-Denis
                    std::string o2;
                    for (char c : lowerOf(x)) if (std::isalnum((unsigned char)c) || (c & 0x80)) o2 += c;
                    return o2;
                };
                std::string want = bare(tok.substr(7));
                int to = -1;
                for (size_t i = 0; i < g.world.settlements.size() && to < 0; i++) if (bare(g.world.settlements[i].name) == want) to = (int)i;
                for (size_t i = 0; i < g.world.settlements.size() && to < 0; i++) if (bare(g.world.settlements[i].name).find(want) != std::string::npos) to = (int)i;
                if (to < 0) { std::fprintf(stderr, "  [no town or post house called \"%s\"]\n", want.c_str()); continue; }
                int from = curSettlement();
                if (from < 0) {   // in the country: from the nearest place
                    int bd2 = 1 << 30;
                    for (size_t i = 0; i < g.world.settlements.size(); i++) {
                        auto &st = g.world.settlements[i];
                        int d = std::max(std::abs(st.x0 + st.w / 2 - g.tx), std::abs(st.y0 + st.h / 2 - g.ty));
                        if (d < bd2) { bd2 = d; from = (int)i; }
                    }
                }
                if (from == to && curSettlement() == to) { std::fprintf(stderr, "  [you are there already]\n"); continue; }
                bool lastLeg = from == to;   // in the country, and the nearest place is where you are going (a journey stopped near its end): on foot
                auto dist = g.world.roadDistances(from);
                float d = dist[to];
                if (lastLeg || !(d > 0 && d < 1e8f)) {
                    const Settlement &st = g.world.settlements[to];
                    int straight = std::max(std::abs(st.x0 + st.w / 2 - g.tx), std::abs(st.y0 + st.h / 2 - g.ty));
                    if (lastLeg || straight <= 200) {   // a farm or a village off the road: on foot, across the country
                        if (g.people.talk.who >= 0) { g.people.endTalk(g.minutes); g.talking = false; }
                        std::fprintf(stderr, lastLeg ? "  [%s is near: you walk the rest of the way]\n" : "  [no road goes there: you walk across the country to %s]\n", st.name.c_str());
                        auto there = [&](int x, int y) { return g.tz == 0 && st.inside(x, y) && !g.world.tile(x, y).bld; };
                        bool ok = false;
                        for (int tries = 0; tries < 6 && !ok; tries++) ok = walkTo(there, 600000);   // a gate or a crowd can turn you aside: on from there
                        if (ok) std::fprintf(stderr, "  [you are at %s]\n", st.name.c_str());
                        else std::fprintf(stderr, "  [you could not find the way across the country to %s from here]\n", st.name.c_str());
                        continue;
                    }
                    std::fprintf(stderr, "  [no road goes from %s to %s: over the sea you need a boat or a ship]\n", g.world.settlements[from].name.c_str(), g.world.settlements[to].name.c_str());
                    continue;
                }
                if (!g.curTown.empty()) {   // setting out from a town that searches what goes out: the warning comes before the road
                    std::string out = g.searchers(g.curTown, true, true);
                    if (g.edgeBack) { g.edgeBack = false; std::fprintf(stderr, "  %s\n  [you turn back: the journey is not made (any journey out of %s within the hour goes on past the searchers and takes your chance; or wait for the dark)]\n", out.c_str(), g.curTown.c_str()); continue; }
                    // going on anyway, or by night: the search at the gate going out, as walking out meets it
                    // (the journey's arrival places you quietly, which passes no gate behind you)
                    int px = g.tx, py = g.ty;
                    std::string gone = g.searchers(g.curTown, true);
                    if (!gone.empty()) std::fprintf(stderr, "  %s\n", gone.c_str());
                    if (g.tx != px || g.ty != py) { if (!g.message.empty()) { std::fprintf(stderr, "  %s\n", g.message.c_str()); g.message.clear(); } continue; }   // fined beyond your purse: prison
                    std::string warn = g.purseWarning();   // setting out with a lot of money (as walking out of the gate)
                    if (!warn.empty()) std::fprintf(stderr, "  %s\n", warn.c_str());
                }
                if (g.people.talk.who >= 0) { g.people.endTalk(g.minutes); g.talking = false; }
                const Settlement &st = g.world.settlements[to];
                const Settlement &sf = g.world.settlements[from];
                const double pace = (g.riding >= 0 ? 0.075 : 0.15) * 0.5;   // the game's own pace (Game::step): seconds a step x half a game minute a second
                // the road, settlement by settlement; one set out from the country joins it at the point nearest you
                std::vector<World::RoadPt> route;
                g.world.roadRoute(from, to, route);
                size_t i0 = 0;
                bool joined = false;
                if (curSettlement() < 0 && route.size() > 2) {
                    int bd3 = 41;
                    for (size_t i = 0; i < route.size(); i++) {
                        int dd3 = std::max(std::abs(route[i].x - g.tx), std::abs(route[i].y - g.ty));
                        if (dd3 < bd3) { bd3 = dd3; i0 = i; joined = true; }
                    }
                }
                float d0 = joined && i0 < route.size() ? route[i0].d : 0.0f;
                float roadD = d - d0;   // the road between the towns' edges: what the post's clock counts
                d = roadD + (st.w + st.h) * 0.25f + (joined ? 0.0f : (sf.w + sf.h) * 0.25f);   // the road runs edge to edge: and across each town to its middle
                double startMin = g.minutes;
                double startCross = joined ? 0.0 : (sf.w + sf.h) * 0.25f;
                // the road's dangers (as Game::wildStep weighs them): one weighing for every 25 steps of open road
                // (outside the towns and post houses), at the hour and in the country where it falls
                auto inPlace = [&](int x, int y) { for (auto &s2 : g.world.settlements) if (s2.inside(x, y)) return true; return false; };
                struct RollPt { size_t i; double min; };
                std::vector<RollPt> rolls;
                {
                    float acc = 0;
                    for (size_t i = i0 + 1; i < route.size(); i++) {
                        int stp = std::max(std::abs(route[i].x - route[i - 1].x), std::abs(route[i].y - route[i - 1].y));
                        if (stp > 2 || inPlace(route[i].x, route[i].y) || inPlace(route[i - 1].x, route[i - 1].y)) continue;   // across a town
                        acc += (float)stp;
                        while (acc >= 25) { acc -= 25; rolls.push_back({i, startMin + (startCross + route[i].d - d0) * pace}); }
                    }
                }
                // GALILEO_ROAD (tests): none = no dangers; band, wolf or bear = that one, at the middle of the open road
                const char *roadEnv = std::getenv("GALILEO_ROAD");
                std::string roadMode = roadEnv ? roadEnv : "";
                bool noDanger = roadMode == "none" || roadMode == "off" || roadMode == "0";
                bool force = roadMode == "band" || roadMode == "wolf" || roadMode == "bear";
                size_t stopRoll = rolls.size();   // the weighing at which something happens
                std::string happened;
                if (!noDanger && !rolls.empty()) {
                    size_t forcedAt = rolls.size() / 2;
                    for (size_t r = 0; r < rolls.size(); r++) {
                        const World::RoadPt &pt = route[rolls[r].i];
                        const World::RoadPt &pa = route[rolls[r].i >= i0 + 3 ? rolls[r].i - 3 : i0], &pb = route[std::min(rolls[r].i + 3, route.size() - 1)];
                        g.tx = pt.x; g.ty = pt.y; g.tz = 0;
                        g.facing = g.facingTarget = std::atan2(-(float)(pb.x - pa.x), (float)(pb.y - pa.y));
                        g.minutes = rolls[r].min;
                        int bs = g.bandState;
                        double lw = g.lastWolves, lb = g.lastBear;
                        if (force) {
                            if (r != forcedAt) continue;
                            g.wildRoll(true, roadMode);
                        } else {
                            for (const char *kind : {"band", "wolf", "bear"}) {
                                g.wildRoll(false, kind);
                                if (g.bandState != bs || g.lastWolves != lw || g.lastBear != lb) break;
                            }
                        }
                        if (g.bandState != bs) happened = "a band of brigands";
                        else if (g.lastWolves != lw) happened = "wolves";
                        else if (g.lastBear != lb) happened = "a bear";
                        if (!happened.empty()) { stopRoll = r; break; }
                    }
                }
                bool stopped = stopRoll < rolls.size();
                size_t iEnd = stopped ? rolls[stopRoll].i : route.size();   // the route points passed: i0 .. iEnd
                double carried = stopped ? (route[iEnd].d - d0) : roadD;   // steps of the road
                if (!stopped) g.minutes = startMin + d * pace;
                // every town and post house the road passes is marked, as walking through it would
                for (size_t i = i0; i < std::min(iEnd + (stopped ? 1 : 0), route.size()); i++)
                    for (auto &s2 : g.world.settlements) {
                        int m = s2.hamlet ? 12 : 3;   // as World::roadDistances counts a road touching a place
                        if (route[i].x >= s2.x0 - m && route[i].x < s2.x0 + s2.w + m && route[i].y >= s2.y0 - m && route[i].y < s2.y0 + s2.h + m)
                            g.people.player["been:" + People::townKey(s2.name)] = "1";
                    }
                if (!stopped) g.people.player["been:" + People::townKey(st.name)] = "1";
                if (g.numMem(g.people.player, "has:post-packet") > 0) {   // the road part only (Game::postClock)
                    double road = std::atof(g.people.player["post:road"].c_str()) + carried * pace;
                    g.people.player["post:road"] = std::to_string(road);
                }
                int X = st.x0 + st.w / 2, Y = st.y0 + st.h / 2;
                if (stopped) { X = route[iEnd].x; Y = route[iEnd].y; }
                g.placePlayer(X, Y);
                if (g.riding >= 0) { g.animals.list[g.riding].x = g.tx + 0.5f; g.animals.list[g.riding].y = g.ty + 0.5f; }
                g.bringLed();
                for (auto &pp : g.people.people) { pp.actIndex = -2; pp.placed = false; }
                for (int q = 0; q < 20; q++) { g.people.update(g.world, 0.1f, g.minutes, g.tx, g.ty, g.tz); g.takeRequests(); }
                g.updatePlace(false);
                if (g.riding >= 0) { g.animals.list[g.riding].x = g.tx + 0.5f; g.animals.list[g.riding].y = g.ty + 0.5f; }
                if (!g.people.notice.empty()) { g.say(g.people.notice); g.people.notice.clear(); }
                if (stopped) std::fprintf(stderr, "  [you travel %d steps by road from %s towards %s, and meet %s on %s]\n", (int)(startCross + carried), sf.name.c_str(), st.name.c_str(), happened.c_str(), dateString(g.minutes).c_str());
                else std::fprintf(stderr, "  [you travel %d steps by road from %s to %s, and arrive on %s]\n", (int)d, g.world.settlements[from].name.c_str(), st.name.c_str(), dateString(g.minutes).c_str());
                if (!g.message.empty()) { std::fprintf(stderr, "  %s\n", g.message.c_str()); g.message.clear(); }
                if (stopped) std::fprintf(stderr, "  [the journey stops here: TRAVEL again to go on]\n");
                if (stopped && happened == "a band of brigands") {
                    if (g.riding >= 0) { g.disband(); std::fprintf(stderr, "  [on horseback you ride wide of them, and they do not follow a horse]\n"); }
                    else if (bandParley()) std::fprintf(stderr, "  [on foot they will not let you by: SAY:pay (a quarter of your purse), or SAY:no and FIGHT:brigand]\n");
                }
                continue;
            }
            if (tok.rfind("TALKTO:", 0) == 0) {
                int who = findPerson(tok.substr(7));
                if (who < 0) { std::fprintf(stderr, "  no one called %s\n", tok.c_str() + 7); continue; }
                Person &pp = g.people.people[who];
                g.tz = pp.z;
                g.placeOnFloor(pp.x + 1, pp.y, pp.z);
                g.people.update(g.world, 0.01f, g.minutes, g.tx, g.ty, g.tz);
                g.people.startTalk(g.world, who, g.minutes);
                g.talking = true;
                std::fprintf(stderr, "  [%s, %s, at %d,%d,%d, %s %s]\n", pp.name.c_str(), pp.title.c_str(), pp.x, pp.y, pp.z, pp.activity.c_str(), pp.actPlace.c_str());
                for (auto &l : g.people.talk.lines) std::fprintf(stderr, "    %s\n", l.second.c_str());
                continue;
            }
            if (tok.rfind("SAY:", 0) == 0) {
                if (!o.play.empty() && g.people.talk.who < 0) { std::fprintf(stderr, "  [you are not talking to anyone: TALK first]\n"); continue; }
                if (!o.play.empty() && g.people.talk.over) { std::fprintf(stderr, "  [that conversation is over: TALK again]\n"); continue; }
                std::string q = tok.substr(4);
                for (auto &c : q) if (c == '_') c = ' ';
                size_t before = g.people.talk.lines.size();
                g.people.ask(g.world, q, g.minutes, g.tx, g.ty);
                for (size_t k = before; k < g.people.talk.lines.size(); k++)   // what was said, before any deal it makes ends the talk
                    std::fprintf(stderr, "    %s%s\n", g.people.talk.lines[k].first ? "" : "> ", g.people.talk.lines[k].second.c_str());
                bool ended = g.people.talk.over;
                g.startRequestedPiece();
                if (ended || g.people.talk.who < 0) std::fprintf(stderr, "  [that ends the conversation: TALK again to say more]\n");
                continue;
            }
            if (tok == "BYE") {   // said, so that a silent BYE is not taken for one that failed
                bool was = g.people.talk.who >= 0;
                g.people.endTalk(g.minutes); g.talking = false;
                std::fprintf(stderr, was ? "  [you end the conversation]\n" : "  [you were not talking to anyone]\n");
                continue;
            }
            // CHATGAP:minutes shortens the time between overheard conversations (tests)
            if (tok.rfind("CHATGAP:", 0) == 0) { g.people.chatGap = std::atof(tok.c_str() + 8); continue; }
            if (tok == "JOURNAL") { g.journal = true; g.journalTab = 1; continue; }
            if (tok == "PLACES") {   // the journal's pages at the back: the towns you have been to, their places and people
                for (auto &ln : g.people.townPages(g.world, g.minutes, g.curTown))
                    if (ln.first != 8) std::printf("  %s%s\n", ln.first == 4 ? "" : ln.first == 5 ? "  " : ln.first == 7 ? "          " : "      ", ln.second.c_str());
                continue;
            }
            if (tok == "STORIES" || tok == "STORIES:detailed" || tok == "STORIES:plain") {   // the journal's stories: printed, and open for the picture
                g.journal = true; g.journalTab = 0; g.journalPage = 0;
                if (tok != "STORIES") g.people.player["journal-detailed"] = tok == "STORIES:detailed" ? "1" : "0";   // as Tab does in the journal
                bool det = g.journalDetailed();
                // the events first, as the window runs them every half second, so that the journal is current
                g.people.update(g.world, 0.5f, g.minutes, g.tx, g.ty, g.tz);   // half a second: one event pass
                g.takeRequests();
                if (!g.people.notice.empty()) { g.say(g.people.notice); g.people.notice.clear(); }
                g.people.noteJournal(g.minutes, true);
                for (auto &ln : g.people.journal(g.minutes, det)) {
                    int kind = ln.first % 10, mark = ln.first / 10;
                    bool title = kind == 0 || kind == 3;
                    const char *tail = !title || !det ? (kind == 3 ? "  (done)" : "")
                                     : mark == 3 ? "" : kind == 0 ? "  (open)" : mark == 1 ? "  (done: well)" : mark == 2 ? "  (done: badly)" : "  (done)";
                    const char *lead = title ? "" : mark == 1 ? "    + " : mark == 2 ? "    x " : "    ";
                    std::printf("  %s%s%s\n", lead, ln.second.c_str(), tail);
                }
                continue;
            }
            // things: NEAR:id stands beside the first one of that kind, facing it; GET, LOOK,
            // INV (what you carry, and the panel for the picture), DROP:id, FACE:N|NE|E|...
            if (tok.rfind("NEAR:", 0) == 0) {   // NEAR:id, or NEAR:id@NAME (in a building whose name has NAME)
                std::string what = tok.substr(5), inB;
                size_t at = what.find('@');
                if (at != std::string::npos) { inB = what.substr(at + 1); what = what.substr(0, at); for (auto &c : inB) if (c == '_') c = ' '; }
                int k = g.items.find(what);
                bool done = false;
                for (size_t i = 0; i < g.items.things.size() && !done && k >= 0; i++) {
                    const Thing &t = g.items.things[i];
                    if (t.gone || t.kind != k) continue;
                    if (!inB.empty() && (!t.owner || g.world.buildings[t.owner - 1].name.find(inB) == std::string::npos)) continue;
                    static const int d8[8][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
                    for (auto &d : d8) {
                        int X = t.x + d[0], Y = t.y + d[1];
                        if (!g.world.passable(X, Y, t.z) || g.world.linkAt(X, Y, t.z) || g.people.occupied(X, Y, t.z)) continue;
                        g.tx = X; g.ty = Y; g.tz = t.z;
                        g.from = g.to = vec2(X + 0.5f, Y + 0.5f);
                        g.stepT = 1;
                        g.camTarget = vec3(X + 0.5f, Y + 0.5f, g.world.floorAt(X + 0.5f, Y + 0.5f, t.z));
                        g.updatePlace(true);
                        g.facing = g.facingTarget = std::atan2(-(float)(t.x - X), (float)(t.y - Y));
                        std::fprintf(stderr, "  beside %s at %d,%d,%d (%s)\n", g.items.nameOf(t.kind, t.count).c_str(), t.x, t.y, t.z, g.world.floorName(t.x, t.y, t.z).c_str());
                        done = true;
                        break;
                    }
                }
                if (!done) std::fprintf(stderr, "  no %s to stand beside\n", tok.c_str() + 5);
                continue;
            }
            if (tok.rfind("FACE:", 0) == 0) {
                auto it = D.find(tok.substr(5));
                if (it != D.end()) g.facing = g.facingTarget = std::atan2(-(float)it->second.first, (float)it->second.second);
                continue;
            }
            // GET:id clicks (through the same code as the mouse) on a thing of that kind within
            // reach; PUT:id:DX,DY clicks on the tile DX,DY from you to put one down there
            if (tok.rfind("GET:", 0) == 0) {
                int k = g.items.find(tok.substr(4));
                float mx = -1, my = -1;
                for (int Y = g.ty - 1; Y <= g.ty + 1 && mx < 0; Y++)
                    for (int X = g.tx - 1; X <= g.tx + 1 && mx < 0; X++)
                        for (int i : g.items.at(X, Y, g.tz))
                            if (g.items.things[i].kind == k) g.screenOf(g.viewProjFor(g.viewAspect), g.thingPoint(g.items.things[i]), mx, my);
                if (mx < 0) { std::fprintf(stderr, "  no %s within reach\n", tok.c_str() + 4); continue; }
                g.startPick(1);
                g.click(mx, my);
                std::fprintf(stderr, "  (click at %.3f,%.3f) %s\n", mx, my, g.message.c_str());
                continue;
            }
            if (tok.rfind("PUT:", 0) == 0) {
                std::string rest = tok.substr(4);
                size_t c = rest.find(':');
                int k = g.items.find(rest.substr(0, c)), dx = 0, dy = 0;
                if (c != std::string::npos) std::sscanf(rest.c_str() + c + 1, "%d,%d", &dx, &dy);
                if (k < 0) continue;
                int X = g.tx + dx, Y = g.ty + dy;
                const Tile *t = g.world.tileAt(X, Y, g.tz);
                float mx, my;
                g.screenOf(g.viewProjFor(g.viewAspect), vec3(X + 0.5f, Y + 0.5f, g.world.floorAt(X + 0.5f, Y + 0.5f, g.tz) + Items::surface(t ? t->feature : 0)), mx, my);
                g.startPick(2, k);
                g.click(mx, my);
                std::fprintf(stderr, "  (click at %.3f,%.3f) %s\n", mx, my, g.message.c_str());
                continue;
            }
            if (tok == "PICK") { g.startPick(1); continue; }   // for a picture of choosing
            if (tok == "LOOKAT:sign") {   // the nearest shop or inn sign within 8 steps
                int best = -1, bd = 1 << 30;
                for (size_t i = 0; i < g.world.signs.size(); i++) {
                    const Sign &sg = g.world.signs[i];
                    int d = std::max(std::abs(sg.x - g.tx), std::abs(sg.y - g.ty));
                    if (d <= 8 && d < bd) { bd = d; best = (int)i; }
                }
                std::fprintf(stderr, "  %s\n", best < 0 ? "[no sign near you]" : g.signSeen(g.world.signs[best]).c_str());
                continue;
            }
            if (tok.rfind("LOOKAT:", 0) == 0) {                 // L, then a click on the nearest thing of that kind
                int k = g.items.find(tok.substr(7));
                int best = -1, bd = 1 << 30;
                for (size_t i = 0; i < g.items.things.size(); i++) {
                    const Thing &t = g.items.things[i];
                    if (t.kind != k || t.gone || t.z != g.tz) continue;
                    int d = std::max(std::abs(t.x - g.tx), std::abs(t.y - g.ty));
                    if (d < bd) { bd = d; best = (int)i; }
                }
                // a thing you carry (the window's pack: look at it there): its look, and how to read it. It wins over
                // one lying out of reach (a book of hours in your pack, before a book 24 steps away)
                int ck = -1;
                {
                    std::string w = tok.substr(7);
                    for (char &c : w) if (c == '_') c = ' ';
                    std::vector<int> tries = {k, g.items.match(w), g.items.match(lowerOf(w))};
                    {   // and word by word ("Galileo's_list": "galileo", "list"), as the names are matched
                        std::istringstream ws(lowerOf(w));
                        std::string one;
                        while (ws >> one) tries.push_back(g.items.match(one));
                    }
                    for (int c : tries)
                        if (c >= 0 && g.numMem(g.people.player, "has:" + g.items.kinds[c].id) > 0) { ck = c; break; }
                }
                if (best < 0 || (bd > 6 && ck >= 0)) {
                    if (ck < 0) { std::fprintf(stderr, "  (no %s here, and you carry none)\n", tok.substr(7).c_str()); continue; }
                    const ItemKind &K = g.items.kinds[ck];
                    std::fprintf(stderr, "  (you carry it) %s%s\n", K.look.c_str(),
                                 K.useVerb.empty() ? "" : ("  [USE:" + K.id + " to " + K.useVerb + " it]").c_str());
                    continue;
                }
                if (bd > 6) {   // the window looks at things within 6 steps (Game::thingNear): say so, not "nothing special"
                    const ItemKind &K = g.items.kinds[g.items.things[best].kind];
                    if (bd <= 40) std::fprintf(stderr, "  (%s is %d steps away, too far to look at: walk nearer, WALKTO:%s)\n", K.name.c_str(), bd, K.id.c_str());
                    else std::fprintf(stderr, "  (%s: none near you)\n", K.name.c_str());
                    continue;
                }
                float mx, my;
                g.screenOf(g.viewProjFor(g.viewAspect), g.thingPoint(g.items.things[best]), mx, my);
                g.mouseX = mx; g.mouseY = my;
                g.message.clear();
                g.startPick(3);
                g.click(mx, my);
                std::fprintf(stderr, "  (look, click at %.3f,%.3f, %d tile%s away) %s\n", mx, my, bd, bd == 1 ? "" : "s", g.panel == 2 ? (g.readTitle + ": " + g.readText).c_str() : g.message.c_str());
                if (g.panel == 3) g.printBook();
                continue;
            }
            if (tok == "LOOKPICK") { g.startPick(3); continue; }   // for a picture of choosing what to look at
            if (tok.rfind("USEAT:", 0) == 0) {   // USEAT:bed: step beside the nearest such furniture and use it (tests)
                std::string w = tok.substr(6);
                int bx = 0, by = 0, bd = 1 << 30;
                for (int Y = g.ty - 30; Y <= g.ty + 30; Y++)
                    for (int X = g.tx - 30; X <= g.tx + 30; X++) {
                        const Tile *t = g.world.tileAt(X, Y, g.tz);
                        if (!t || t->feature == F_NONE || featureName(t->feature).find(w) == std::string::npos) continue;
                        int d = std::abs(X - g.tx) + std::abs(Y - g.ty);
                        if (d < bd) { bd = d; bx = X; by = Y; }
                    }
                if (bd == (1 << 30)) { std::fprintf(stderr, "  (no %s near)\n", w.c_str()); continue; }
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        if ((dx || dy) && g.world.passable(bx + dx, by + dy, g.tz) && !g.people.occupied(bx + dx, by + dy, g.tz)) { g.tx = bx + dx; g.ty = by + dy; dx = dy = 2; }
                g.from = g.to = vec2(g.tx + 0.5f, g.ty + 0.5f);
                g.updatePlace(true);
                g.message.clear();
                g.useFeature(bx, by, g.world.tileAt(bx, by, g.tz)->feature);
                std::fprintf(stderr, "  (use %s at %d,%d, %s) %s\n", w.c_str(), bx, by, dateString(g.minutes).c_str(), g.message.c_str());
                continue;
            }
            if (tok.rfind("MOUSE:", 0) == 0) {                  // the mouse over the first thing of that kind near you
                int k = g.items.find(tok.substr(6));
                for (int Y = g.ty - 2; Y <= g.ty + 2; Y++)
                    for (int X = g.tx - 2; X <= g.tx + 2; X++)
                        for (int i : g.items.at(X, Y, g.tz))
                            if (g.items.things[i].kind == k) {
                                float mx, my;
                                g.screenOf(g.viewProjFor(g.viewAspect), g.thingPoint(g.items.things[i]), mx, my);
                                g.mouseX = mx; g.mouseY = my;
                            }
                continue;
            }
            if (tok == "LOOK") { std::string s = g.lookAhead(); std::fprintf(stderr, "  %s%s\n", s.c_str(), g.panel == 2 ? (": " + g.readText).c_str() : ""); if (g.panel == 3) g.printBook(); if (!g.panel) g.say(s, 6); continue; }
            if (tok == "INV") {
                for (auto &c : g.carried()) std::fprintf(stderr, "  carrying %s\n", g.items.nameOf(c.first, c.second).c_str());
                std::fprintf(stderr, "  money: %s; weight %.1f pounds\n", Game::moneyString(g.money()).c_str(), g.carriedWeight());
                if (!g.wearing().empty()) std::fprintf(stderr, "  wearing %s\n", g.wearing().c_str());
                if (g.money() > 0) std::fprintf(stderr, "  your purse: %s\n", g.purseString().c_str());
                g.panel = 1;
                continue;
            }
            if (tok == "CLOSE") { g.panel = 0; continue; }
            if (tok.rfind("USE:", 0) == 0) {
                int k = g.items.find(tok.substr(4));
                if (k < 0) {   // by its name, among what you carry ("bread" for the loaf)
                    std::string want = tok.substr(4);
                    for (char &c : want) { if (c == '_') c = ' '; c = (char)std::tolower((unsigned char)c); }
                    for (size_t kk = 0; kk < g.items.kinds.size() && k < 0; kk++) {
                        const ItemKind &K = g.items.kinds[kk];
                        if (g.numMem(g.people.player, "has:" + K.id) <= 0) continue;
                        std::string n = K.name;
                        for (char &c : n) c = (char)std::tolower((unsigned char)c);
                        if (n.find(want) != std::string::npos) k = (int)kk;
                    }
                    if (k < 0) { std::fprintf(stderr, "  [you carry nothing called \"%s\": INV lists what you carry]\n", want.c_str()); continue; }
                }
                g.panel = 0;
                g.message.clear();
                if (k >= 0) g.useThing(k);
                if (g.panel == 3) g.printBook();
                else std::fprintf(stderr, "  %s%s\n", g.panel == 2 ? (g.readTitle + ": " + g.readText).c_str() : g.message.c_str(), "");
                continue;
            }
            if (tok.rfind("FIGHT:", 0) == 0) {   // FIGHT:id-or-role: go for the nearest such person (tests)
                std::string w = tok.substr(6), lw = lowerOf(w);
                for (char &c : lw) if (c == '_') c = ' ';
                int best = -1, bd = 1 << 30;
                for (size_t i = 0; i < g.people.people.size(); i++) {
                    const Person &pp = g.people.people[i];
                    if (pp.dead || pp.away || pp.z != g.tz) continue;
                    bool named = pp.id.find(w) != std::string::npos || pp.role == w || lowerOf(pp.id).find(lw) != std::string::npos ||
                                 (pp.placed && (lowerOf(g.people.label(pp)).find(lw) != std::string::npos || lowerOf(pp.title).find(lw) != std::string::npos));   // as AROUND names them
                    if (!named) continue;
                    int d = std::max(std::abs(pp.x - g.tx), std::abs(pp.y - g.ty));
                    auto asked = g.people.player.find("own-side-asked");   // the man of your own side just asked about: "C on him again"
                    if (asked != g.people.player.end() && asked->second == pp.id) { bd = d; best = (int)i; break; }
                    if (d < bd) { bd = d; best = (int)i; }
                }
                if (best < 0) {   // a wild beast of that kind, then
                    float bd2 = 1e9f;
                    for (size_t i = 0; i < g.animals.list.size(); i++) {
                        const Animal &a = g.animals.list[i];
                        if (!a.wildOne || a.gone || a.dead || g.animals.kinds[a.kind].id != w) continue;
                        float d = std::hypot(a.x - g.tx, a.y - g.ty);
                        if (d < bd2) { bd2 = d; best = (int)i; }
                    }
                    if (best >= 0) { g.startFightBeast(best); std::fprintf(stderr, "  %s (%.0f tile%s away)\n", g.message.c_str(), bd2, std::lround(bd2) == 1 ? "" : "s"); continue; }
                }
                if (best < 0) { std::fprintf(stderr, "  (nobody called %s here)\n", w.c_str()); continue; }
                g.startFight(best);
                std::fprintf(stderr, "  %s (%d tile%s away)\n", g.message.c_str(), bd, bd == 1 ? "" : "s");
                continue;
            }
            if (tok.rfind("WILD:", 0) == 0) {   // WILD:band|wolf|bear|deer: meet one now, wherever you are (tests)
                g.message.clear();
                g.wildRoll(true, tok.substr(5));
                std::fprintf(stderr, "  [met: %s] %s\n", tok.c_str() + 5, g.message.c_str());
                for (auto &a : g.animals.list)
                    if (a.wildOne && !a.gone) std::fprintf(stderr, "    %s at %.1f,%.1f%s%s\n", g.animals.kinds[a.kind].name.c_str(), a.x, a.y, a.dead ? " (dead)" : "", a.after ? " (after you)" : "");
                for (int i : g.people.band) { const Person &p = g.people.people[i]; if (!p.away) std::fprintf(stderr, "    %s at %d,%d\n", p.title.c_str(), p.x, p.y); }
                continue;
            }
            if (tok == "COUNTRY") { setenv("GALILEO_DEBUG_WILD", "1", 1); g.wildRoll(false, "none"); unsetenv("GALILEO_DEBUG_WILD"); continue; }   // where you are, as the game weighs it
            if (tok == "TOWATER") {   // to the nearest open ground beside water (tests: fishing)
                int bx = -1, by = -1, bd = 1 << 30;
                for (int y = g.ty - 80; y <= g.ty + 80; y++)
                    for (int x = g.tx - 80; x <= g.tx + 80; x++) {
                        if (!g.world.inWorld(x, y) || !g.world.passable(x, y, 0) || g.world.tile(x, y).bld) continue;
                        bool wet = false;
                        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) { int t = g.world.tile(x + ox, y + oy).terrain; if (t == T_WATER || t == T_DEEP) wet = true; }
                        int d = std::abs(x - g.tx) + std::abs(y - g.ty);
                        if (wet && d < bd) { bd = d; bx = x; by = y; }
                    }
                if (bx < 0) { std::fprintf(stderr, "  no water near\n"); continue; }
                g.tx = bx; g.ty = by; g.tz = 0; g.from = g.to = vec2(bx + 0.5f, by + 0.5f); g.updatePlace(true);
                std::fprintf(stderr, "  by the water at %d,%d (%s)\n", bx, by, g.place.c_str());
                continue;
            }
            if (tok == "SHOOT") { g.message.clear(); g.shootNearest(); std::fprintf(stderr, "  %s\n", g.message.c_str()); continue; }
            if (tok == "FISH") { g.message.clear(); g.goFishing(); std::fprintf(stderr, "  %s\n", g.message.c_str()); continue; }
            if (tok == "BEASTS") {   // the wild ones about you (tests)
                for (auto &a : g.animals.list)
                    if (a.wildOne && !a.gone) std::fprintf(stderr, "    %s at %.1f,%.1f, %.0f tiles off, health %.0f%s%s%s\n", g.animals.kinds[a.kind].name.c_str(), a.x, a.y,
                                                         std::hypot(a.x - g.tx, a.y - g.ty), a.hp, a.dead ? " (dead)" : "", a.after ? " (after you)" : "", a.fleeT > 0 ? " (running)" : "");
                for (int i : g.people.band) { const Person &p = g.people.people[i]; if (!p.away) std::fprintf(stderr, "    %s at %d,%d, health %.0f%s%s\n", p.title.c_str(), p.x, p.y, p.hp, p.dead ? " (dead)" : "", p.fleeT > 0 ? " (running)" : ""); }
                if (g.people.escortIdx >= 0 && !g.people.people[g.people.escortIdx].away) { const Person &e = g.people.people[g.people.escortIdx]; std::fprintf(stderr, "    your escort at %d,%d, health %.0f\n", e.x, e.y, e.hp); }
                std::fprintf(stderr, "    [band state %d]\n", g.bandState);
                continue;
            }
            if (tok == "HEARD") {   // tests: the sounds of the fighting heard since the last HEARD (musket, volley, clash, knock)
                std::string l;
                for (auto &h : g.heardCount) l += (l.empty() ? "" : ", ") + h.first + " " + std::to_string(h.second);
                std::fprintf(stderr, "  [heard: %s]\n", l.empty() ? "nothing of the fighting" : l.c_str());
                g.heardCount.clear();
                continue;
            }
            if (tok == "HP") { std::fprintf(stderr, "  [health %.1f of %d; money %s]\n", g.hpNow(), g.maxHp(), Game::moneyString(g.money()).c_str()); continue; }
            if (tok.rfind("HP:", 0) == 0) { g.setHp(std::atof(tok.c_str() + 3)); g.lastHurt = g.minutes; continue; }   // tests: your health set so (a defeat at hand)
            if (tok.rfind("MEM:", 0) == 0) {   // MEM:key prints what you remember under it (tests)
                std::string k = tok.substr(4);
                std::fprintf(stderr, "  [you remember %s = %s; now %.0f]\n", k.c_str(), g.people.player.count(k) ? g.people.player[k].c_str() : "(nothing)", g.minutes);
                continue;
            }
            if (tok.rfind("KNOW:", 0) == 0) {   // KNOW:skill:italian=1 sets something you know (a test's shortcut)
                std::string kv = tok.substr(5);
                size_t q = kv.find('=');
                g.people.player[kv.substr(0, q)] = q == std::string::npos ? "1" : kv.substr(q + 1);
                continue;
            }
            if (tok.rfind("CARD:", 0) == 0) {   // CARD:ID puts the card up as the window would (for a picture); PRESS:SPACE goes on
                g.showCard(tok.substr(5), -1, "", true);
                continue;
            }
            if (tok.rfind("PRESS:", 0) == 0 && g.cardUp) { g.afterCard(); continue; }
            if (tok.rfind("PRESS:", 0) == 0 || tok.rfind("TYPE:", 0) == 0) {   // keys and letters on the choosing screen (tests)
                if (!g.creating && g.sceneNow < 0) { std::fprintf(stderr, "  (PRESS and TYPE work only on the choosing screen and in the scenes)\n"); continue; }
                if (tok[0] == 'T') { for (unsigned char c : tok.substr(5)) if (c < 0x80) onChar(nullptr, c == '_' ? ' ' : c); continue; }
                static const std::map<std::string, int> keys = {{"UP", GLFW_KEY_UP}, {"DOWN", GLFW_KEY_DOWN}, {"LEFT", GLFW_KEY_LEFT},
                    {"RIGHT", GLFW_KEY_RIGHT}, {"ENTER", GLFW_KEY_ENTER}, {"BACKSPACE", GLFW_KEY_BACKSPACE}, {"TAB", GLFW_KEY_TAB},
                    {"SPACE", GLFW_KEY_SPACE}, {"ESCAPE", GLFW_KEY_ESCAPE}};
                auto k = keys.find(tok.substr(6));
                if (k == keys.end()) { std::fprintf(stderr, "  (PRESS takes UP DOWN LEFT RIGHT TAB ENTER BACKSPACE SPACE ESCAPE)\n"); continue; }
                int before = g.sceneNow;
                onKey(nullptr, k->second, 0, GLFW_PRESS, 0);
                if (g.creating && !o.play.empty()) printChoosing();
                if (g.sceneNow >= 0) std::fprintf(stderr, "  [%sscene %d of %zu] %s\n", g.sceneNow < before ? "back to " : "", g.sceneNow + 1, g.scenes.size(), g.scenes[g.sceneNow].caption.c_str());
                continue;
            }
            if (tok == "TURN" || tok == "TURNBACK") {   // the next (or the last) two pages of the open book
                if (g.panel == 3) { g.turnPages(tok == "TURN" ? 1 : -1); g.printBook(); }
                else if (g.journal && g.journalTab == 0) {   // or a page of the journal (for its picture)
                    g.journalPage = std::max(0, g.journalPage + (tok == "TURN" ? 1 : -1));
                    std::fprintf(stderr, "  (the journal, page %d)\n", g.journalPage + 1);
                }
                else std::fprintf(stderr, "  (no book open)\n");
                continue;
            }
            if (tok.rfind("USEANIMAL:", 0) == 0) {   // USEANIMAL:cow: stand beside the nearest such animal and use it (tests)
                std::string id = tok.substr(10);
                g.animals.placeNear(g.world, (float)g.tx, (float)g.ty, 150);
                int best = -1;
                float bd = 1e9f;
                for (size_t i = 0; i < g.animals.list.size(); i++) {
                    const Animal &a = g.animals.list[i];
                    if (a.gone || g.animals.kinds[a.kind].id != id) continue;
                    // only one near you, as the window's player must walk up to it (not your own horse left in another
                    // town, with you put beside it); your own first
                    if (std::max(std::abs(a.x - g.tx), std::abs(a.y - g.ty)) > 20 && (int)i != g.riding) continue;
                    float d = std::abs(a.x - g.tx) + std::abs(a.y - g.ty) - (a.mine ? 1e6f : 0);
                    if (d < bd) { bd = d; best = (int)i; }
                }
                // on horseback, USEANIMAL:horse means the one you ride (its place lags behind you, and a horse
                // tied at an inn door near by would otherwise be "nearer": you mounted again instead of getting down)
                if (g.riding >= 0 && g.riding < (int)g.animals.list.size() && g.animals.kinds[g.animals.list[g.riding].kind].id == id) best = g.riding;
                if (best < 0) {
                    std::string mine;
                    for (const Animal &a : g.animals.list)
                        if (!a.gone && a.mine && g.animals.kinds[a.kind].id == id) mine = " (yours is at " + std::to_string((int)a.x) + "," + std::to_string((int)a.y) + ")";
                    std::fprintf(stderr, "  no %s near you%s\n", id.c_str(), mine.c_str());
                    continue;
                }
                Animal &a = g.animals.list[best];
                int ax = (int)std::floor(a.x), ay = (int)std::floor(a.y);
                for (int r = 1; r <= 2 && best != g.riding; r++) {
                    bool moved = false;
                    for (int dy = -r; dy <= r && !moved; dy++)
                        for (int dx = -r; dx <= r && !moved; dx++)
                            if ((dx || dy) && g.world.passable(ax + dx, ay + dy, 0)) { g.tx = ax + dx; g.ty = ay + dy; g.tz = 0; moved = true; }
                    if (moved) break;
                }
                g.from = g.to = vec2(g.tx + 0.5f, g.ty + 0.5f);
                g.message.clear();
                g.useAnimal(best);
                std::fprintf(stderr, "  (%s, %s, at %.0f,%.0f; %zu people outdoors within 7 tiles) %s\n", g.animals.kinds[a.kind].name.c_str(),
                             a.owner.empty() ? "nobody's" : a.owner.c_str(), a.x, a.y, g.watchers(0).size(), g.message.c_str());
                continue;
            }
            if (tok.rfind("GIVEME:", 0) == 0) {   // GIVEME:id puts one in what you carry (tests)
                std::string id = tok.substr(7);
                g.people.player["has:" + id] = std::to_string(std::atoi(g.people.player["has:" + id].c_str()) + 1);
                continue;
            }
            if (tok.rfind("DROP:", 0) == 0) {
                int k = g.items.find(tok.substr(5));
                if (k >= 0) { int fx, fy; g.frontTile(fx, fy); if (g.canPut(fx, fy)) g.putThing(k, fx, fy); else g.putThing(k, g.tx, g.ty); std::fprintf(stderr, "  %s\n", g.message.c_str()); }
                continue;
            }
            // SAVE:file and LOAD:file write and read what people remember (tests)
            if (tok.rfind("SAVE:", 0) == 0) { std::ofstream out(tok.substr(5)); g.people.save(out); g.items.save(out); continue; }
            if (tok.rfind("LOAD:", 0) == 0) {
                std::ifstream in(tok.substr(5));
                std::stringstream all;
                all << in.rdbuf();
                std::istringstream r1(all.str()), r2(all.str());
                g.people.loadSave(r1);
                g.items.loadSave(r2);
                g.syncBoat();
                int put = 0, gone = 0;
                for (auto &t : g.items.things) { if (!t.key && !t.gone) put++; if (t.key && t.gone) gone++; }
                std::fprintf(stderr, "  loaded: %zu lines overheard, %zu things you know, %d things put down, %d taken\n",
                             g.people.heard.size(), g.people.player.size(), put, gone);
                continue;
            }
            // DRAW draws one picture of what you see (into a hidden window) and says how many chunk meshes are held (tests of Game::trimGfx)
            if (tok == "DRAW") {
                if (!g.glReady && !(g.openGL && g.openGL())) { std::fprintf(stderr, "  [no picture can be drawn]\n"); continue; }
                vec2 pp = g.playerPos();
                g.camTarget = vec3(pp.x, pp.y, g.world.groundZ(pp.x, pp.y));
                g.render(o.w, o.h, 1.0f, true);
                std::fprintf(stderr, "  [drawn: %zu chunk meshes held, %llu built so far]\n", g.gfx.size(), (unsigned long long)g.gfxBuilt);
                continue;
            }
            // FILM:SECONDS:FPS:DIR (with a picture: tools/play.sh's third argument, or --shot): the game goes on for SECONDS of
            // the window's time and a picture is made at each frame, DIR/f0000.png ...; DIR/booms.txt has the frames where
            // guns were heard, and how loud (a film of the battle, with its sound: made into a movie outside the game)
            if (tok.rfind("FILM:", 0) == 0) {
                double secs = 0, fps = 20; char dir[512] = {0};
                if (std::sscanf(tok.c_str() + 5, "%lf:%lf:%511s", &secs, &fps, dir) < 3 || !g.glReady) { std::fprintf(stderr, "  [FILM:SECONDS:FPS:DIR needs a picture to be drawn]\n"); continue; }
                int W = o.w, H = o.h;
                GLuint msFbo, msColor, msDepth, fbo, color;
                glGenFramebuffers(1, &msFbo); glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
                glGenRenderbuffers(1, &msColor); glBindRenderbuffer(GL_RENDERBUFFER, msColor);
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, W, H);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor);
                glGenRenderbuffers(1, &msDepth); glBindRenderbuffer(GL_RENDERBUFFER, msDepth);
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, W, H);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth);
                glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                glGenRenderbuffers(1, &color); glBindRenderbuffer(GL_RENDERBUFFER, color);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, W, H);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
                int frames = (int)std::lround(secs * fps);
                double dt = 1.0 / fps;
                g.filmBooms.clear();
                g.filmCues.clear();
                g.filmSounds.clear();
                if (const char *z = std::getenv("GALILEO_FILM_ZOOM")) g.zoom = g.zoomT = (float)std::atof(z);   // closer, for a film
                if (const char *y = std::getenv("GALILEO_FILM_YAW")) g.yaw = g.yawT = (float)std::atof(y);     // from another side (22: as the game begins)
                for (int fr = 0; fr < frames; fr++) {
                    g.filmFrame = fr;
                    for (double t = 0; t < dt - 1e-9; t += 0.05) {   // the window's time, in steps of at most a twentieth of a second
                        double st = std::min(0.05, dt - t);
                        g.now += st;
                        g.minutes += st * g.world.clockRate;
                        g.postClock();
                        g.people.update(g.world, (float)st, g.minutes, g.tx, g.ty, g.tz);
                        g.animals.update(g.world, (float)st, g.minutes, g.tx, g.ty);
                        g.takeRequests();
                        g.watchStep(); g.fight((float)st); g.wildStep((float)st);
                        if (!g.people.notice.empty()) { g.say(g.people.notice, 8.0); g.people.notice.clear(); }
                    }
                    vec2 pp = g.playerPos();
                    float cx = pp.x, cy = pp.y;
                    if (const char *at = std::getenv("GALILEO_FILM_AT")) std::sscanf(at, "%f,%f", &cx, &cy);   // the camera on a place, not on you
                    g.camTarget = vec3(cx, cy, g.world.groundZ(cx, cy));
                    glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
                    glViewport(0, 0, W, H);
                    g.render(W, H, W / 1440.0f, true);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
                    glBlitFramebuffer(0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                    char fn[600]; std::snprintf(fn, sizeof fn, "%s/f%04d.png", dir, fr);
                    g.savePng(fn, W, H);
                }
                g.filmFrame = -1;
                {
                    std::string bf = std::string(dir) + "/booms.txt";
                    if (FILE *f = std::fopen(bf.c_str(), "w")) { std::fprintf(f, "# frame volume (at %.0f frames a second)\n", fps); for (auto &b : g.filmBooms) std::fprintf(f, "%d %.3f\n", b.first, b.second); std::fclose(f); }
                    std::string sf = std::string(dir) + "/sounds.txt";   // the musketry and blows: frame, sound, volume
                    if (FILE *f = std::fopen(sf.c_str(), "w")) { std::fprintf(f, "# frame sound volume (at %.0f frames a second)\n", fps); for (auto &c : g.filmSounds) std::fprintf(f, "%d %s %.3f\n", c.frame, c.id.c_str(), c.vol); std::fclose(f); }
                    std::string cf = std::string(dir) + "/cues.txt";   // the music begun by events: frame, cue, level
                    if (FILE *f = std::fopen(cf.c_str(), "w")) { std::fprintf(f, "# frame cue level (at %.0f frames a second)\n", fps); for (auto &c : g.filmCues) std::fprintf(f, "%d %s %.2f\n", c.frame, c.id.c_str(), c.level); std::fclose(f); }
                }
                glDeleteFramebuffers(1, &msFbo); glDeleteFramebuffers(1, &fbo);
                glDeleteRenderbuffers(1, &msColor); glDeleteRenderbuffers(1, &msDepth); glDeleteRenderbuffers(1, &color);
                std::fprintf(stderr, "  [filmed %d frames, %zu booms, until %s]\n", frames, g.filmBooms.size(), dateString(g.minutes).c_str());
                continue;
            }
            // NEXTTO:name stands beside someone without speaking; PACE:seconds measures how fast people walk
            if (tok.rfind("NEXTTO:", 0) == 0) {
                int who = findPerson(tok.substr(7));
                if (who < 0) { std::fprintf(stderr, "  no one to stand beside\n"); continue; }
                lastWho = who;
                Person &pp = g.people.people[who];
                g.tz = pp.z;
                g.placeOnFloor(pp.x + 1, pp.y, pp.z);
                std::fprintf(stderr, "  beside %s at %d,%d,%d\n", pp.name.c_str(), pp.x, pp.y, pp.z);
                continue;
            }
            if (tok.rfind("PACE:", 0) == 0) {
                double secs = std::atof(tok.c_str() + 5);
                std::vector<float> moved(g.people.people.size(), 0), walking(g.people.people.size(), 0);
                double stuck = 0, walkers = 0; int samples = 0;
                for (double t = 0; t < secs; t += 0.02) {
                    std::vector<std::array<int, 2>> was;
                    for (auto &pp : g.people.people) was.push_back({pp.x, pp.y});
                    g.minutes += 0.01;
                    g.people.update(g.world, 0.02f, g.minutes, g.tx, g.ty, g.tz);
                    for (size_t i = 0; i < g.people.people.size(); i++) {
                        auto &pp = g.people.people[i];
                        if (!pp.active || pp.onRoad) continue;
                        int dx = pp.x - was[i][0], dy = pp.y - was[i][1];
                        if (std::abs(dx) <= 1 && std::abs(dy) <= 1) moved[i] += std::sqrt((float)(dx * dx + dy * dy));
                        if (pp.pathPos < pp.path.size()) walking[i] += 0.02f;
                        if (pp.pathPos < pp.path.size()) { walkers++; if (pp.wait > 0) stuck++; }
                        if (std::getenv("GALILEO_STUCK") && pp.pathPos < pp.path.size() && pp.wait > 0 && samples % 250 == 0) {
                            auto nx = pp.path[pp.pathPos];
                            int b = -2;
                            for (size_t j = 0; j < g.people.people.size(); j++) { auto &o = g.people.people[j]; if (o.x == nx[0] && o.y == nx[1] && o.z == nx[2]) b = (int)j; }
                            if (g.tx == nx[0] && g.ty == nx[1] && g.tz == nx[2]) b = -1;
                            std::fprintf(stderr, "    stuck %s at %d,%d,%d (%s) going %s %s, behind %s%s\n", pp.title.c_str(), pp.x, pp.y, pp.z,
                                         g.world.floorName(pp.x, pp.y, pp.z).c_str(), pp.activity.c_str(), pp.actPlace.c_str(),
                                         b == -1 ? "you" : b < 0 ? "nobody?" : g.people.people[b].title.c_str(),
                                         b >= 0 ? (g.people.people[b].pathPos < g.people.people[b].path.size() ? " (walking)" : (" (standing, " + g.people.people[b].activity + ")").c_str()) : "");
                        }
                    }
                    samples++;
                }
                double sum = 0; int n = 0;
                for (size_t i = 0; i < moved.size(); i++)
                    if (walking[i] > secs * 0.3) { sum += moved[i] / walking[i]; n++; }
                std::fprintf(stderr, "  %d people walked for a third of %.0f s or more, at %.2f tiles a second on average;\n"
                                     "  on average %.1f people were walking, %.1f of them stuck behind someone\n",
                             n, secs, n ? sum / n : 0.0, walkers / samples, stuck / samples);
                continue;
            }
            if (tok.rfind("WAIT:", 0) == 0) {
                bool toNews = tok == "WAIT:news";   // until word comes (a news line told to you), at most 14 days
                double mins = toNews ? 14 * 1440 : std::atof(tok.c_str() + 5);
                std::string last = g.message;
                g.animals.placeNear(g.world, (float)g.tx, (float)g.ty, 150);
                if (toNews || mins > 60) {   // a long wait: the clock primitive, in coarse steps (Game::advanceClock)
                    double t0 = g.minutes;
                    // a card or a jump of the calendar ends the wait, as a card on the screen does in the window (so that
                    // a long wait does not run on past the Chapter II card)
                    int why = g.advanceClock(mins, (toNews ? Game::CLK_NEWS : 0) | Game::CLK_BEAT);
                    for (int q = 0; q < 10 && g.sceneNow < 0; q++) {   // a few moments, for the people to take their places
                        g.people.update(g.world, 0.1f, g.minutes, g.tx, g.ty, g.tz);
                        g.animals.update(g.world, 0.1f, g.minutes, g.tx, g.ty);
                        g.takeRequests();
                        if (!g.people.notice.empty()) { g.say(g.people.notice); g.people.notice.clear(); }
                    }
                    if (why == Game::CLK_BEAT) std::fprintf(stderr, "  [the wait ends at the card: the calendar is now %s]\n", dateString(g.minutes).c_str());
                    else if (toNews) std::fprintf(stderr, "  %s after %.0f minutes\n", why == Game::CLK_NEWS ? "word came" : "no word came", g.minutes - t0);
                    std::fprintf(stderr, "  waited until %s\n", dateString(g.minutes).c_str());
                    continue;
                }
                for (double t = 0; t < mins * 2; t += 0.1) {   // two seconds of real time to a game minute
                    if (g.sceneNow >= 0) break;   // a beat's scenes began: nothing moves while they are shown
                    g.minutes += 0.05;
                    g.postClock();
                    g.people.update(g.world, 0.1f, g.minutes, g.tx, g.ty, g.tz);
                    g.animals.update(g.world, 0.1f, g.minutes, g.tx, g.ty);
                    g.takeRequests();
                    g.horseNight();
                    g.watchStep(); g.fight(0.1f); g.wildStep(0.1f);
                    if (!g.people.notice.empty()) { g.say(g.people.notice); g.people.notice.clear(); }
                    if (g.message != last && !g.message.empty()) { std::fprintf(stderr, "  %s\n", g.message.c_str()); last = g.message; }
                    // someone going for you, and you not fighting back: the wait stops, as a player at the window would stop
                    // waiting (the Croats under the Alte Veste)
                    if (g.people.player.count("under-attack") && g.fightWith < 0 && g.fightBeast < 0) {
                        int who = -1, wd = 1 << 30;
                        for (size_t i = 0; i < g.people.people.size(); i++) {
                            const Person &pp = g.people.people[i];
                            if (pp.dead || pp.away || pp.target != -2 || pp.fleeT > 0) continue;
                            int d = std::max(std::abs(pp.x - g.tx), std::abs(pp.y - g.ty));
                            if (d < wd) { wd = d; who = (int)i; }
                        }
                        if (who < 0) continue;   // nobody after all (the memory from before a defeat)
                        std::string l = who >= 0 ? g.people.label(g.people.people[who]) : std::string("someone");
                        std::fprintf(stderr, "  [%s is coming at you, %d step%s %s: the wait stops (FIGHT back, or get away)]\n", l.c_str(), wd, wd == 1 ? "" : "s",
                                     who >= 0 ? dirWord(g.people.people[who].x - g.tx, g.people.people[who].y - g.ty).c_str() : "off");
                        break;
                    }
                }
                std::fprintf(stderr, "  waited until %s\n", dateString(g.minutes).c_str());
                continue;
            }
            if (tok.rfind("SKIP:", 0) == 0) {   // tests: jump the clock on, then a few moments pass
                g.minutes += std::atof(tok.c_str() + 5);
                for (auto &pp : g.people.people) { pp.actIndex = -2; pp.placed = false; }
                std::string last = g.message;
                g.horseNight();
                for (int q = 0; q < 30; q++) {
                    if (g.sceneNow >= 0) break;   // as in the game: nothing moves while a beat's scenes are shown
                    g.people.update(g.world, 0.1f, g.minutes, g.tx, g.ty, g.tz);
                    g.takeRequests();
                    g.watchStep(); g.fight(0.1f); g.wildStep(0.1f);
                    if (!g.people.notice.empty()) { g.say(g.people.notice); g.people.notice.clear(); }
                    if (g.message != last && !g.message.empty()) { std::fprintf(stderr, "  %s\n", g.message.c_str()); last = g.message; }
                }
                std::fprintf(stderr, "  skipped to %s\n", dateString(g.minutes).c_str());
                continue;
            }
            if (tok.rfind("ROAD:", 0) == 0) { setenv("GALILEO_ROAD", tok.substr(5).c_str(), 1); continue; }
            if (tok.rfind("SEARCH:", 0) == 0) { setenv("GALILEO_SEARCH", tok.substr(7).c_str(), 1); continue; }   // tests: the searchers' chance (1 always find, 0 never)   // tests: the dangers of TRAVEL's road: none, band, wolf, bear, or chance
            if (tok.rfind("POSTEVENT:", 0) == 0) { setenv("GALILEO_POSTEVENT", tok.substr(10).c_str(), 1); continue; }   // tests: force the post ride's rare event (brigands, lame, cordon, ford) or none
            if (tok.rfind("MONEY:", 0) == 0) { g.people.player["money"] = tok.substr(6); continue; }   // tests: what you have, in deniers
            if (tok.rfind("GOTO:", 0) == 0) {   // GOTO:X,Y walks there by the shortest way, step by step (the watch looks)
                int gx = 0, gy = 0;
                std::sscanf(tok.c_str() + 5, "%d,%d", &gx, &gy);
                // aboard a boat it sails, over water only (and onto the shore, if that is X,Y); on foot,
                // X,Y may be a boat's tile, to step aboard her
                bool aboard = g.boat >= 0;
                auto open = [&](int x, int y) {
                    if (x == gx && y == gy) return aboard ? true : (g.world.passable(x, y, g.tz) || g.world.waterAt(x, y));
                    return aboard ? g.boatWater(x, y) : g.world.passable(x, y, g.tz);
                };
                std::map<std::pair<int, int>, std::pair<int, int>> prev;
                std::deque<std::pair<int, int>> q{{g.tx, g.ty}};
                prev[{g.tx, g.ty}] = {INT32_MIN, 0};
                bool found = false;
                while (!q.empty() && !found && prev.size() < (aboard ? 600000u : 200000u)) {
                    auto [x, y] = q.front(); q.pop_front();
                    for (int dy = -1; dy <= 1 && !found; dy++)
                        for (int dx = -1; dx <= 1 && !found; dx++) {
                            if (!dx && !dy) continue;
                            int nx = x + dx, ny = y + dy;
                            if (prev.count({nx, ny}) || !open(nx, ny)) continue;
                            if (dx && dy && (!open(x + dx, y) || !open(x, y + dy))) continue;
                            prev[{nx, ny}] = {x, y};
                            if (nx == gx && ny == gy) found = true;
                            q.push_back({nx, ny});
                        }
                }
                if (!found) { std::fprintf(stderr, "  no way to %d,%d\n", gx, gy); continue; }
                std::vector<std::pair<int, int>> route;
                for (std::pair<int, int> c{gx, gy}; c.first != INT32_MIN; c = prev[c]) route.push_back(c);
                std::reverse(route.begin(), route.end());
                for (size_t r = 1; r < route.size(); r++) {
                    int bx = g.tx, by = g.ty;
                    g.blockedCooldown = 0;
                    int tb0 = g.turnBacks;
                    g.tryStep(route[r].first - g.tx, route[r].second - g.ty, false);
                    g.stepT = 1; g.from = g.to;
                    for (int q2 = 0; q2 < 2; q2++) { g.people.update(g.world, 0.1f, g.minutes, g.tx, g.ty, g.tz); g.watchStep(); g.fight(0.1f); g.wildStep(0.1f); } g.takeRequests();
                    if (g.turnBacks != tb0) {
                        if (!g.message.empty()) { std::fprintf(stderr, "  %s\n", g.message.c_str()); g.message.clear(); }
                        std::fprintf(stderr, "  [you turn back at the edge of %s (any walk into it within the hour goes on past the searchers and takes your chance; or wait for the dark)]\n", g.turnTown.c_str());
                        break;
                    }
                    if (!g.people.notice.empty()) { g.say(g.people.notice); g.people.notice.clear(); }
                    if (!g.message.empty()) { std::fprintf(stderr, "  %s\n", g.message.c_str()); g.message.clear(); }
                    if (g.tx == bx && g.ty == by) { std::fprintf(stderr, "  stopped at %d,%d\n", g.tx, g.ty); break; }
                    if (g.tz != 0 || std::abs(g.tx - route[r].first) + std::abs(g.ty - route[r].second) > 0) break;   // moved elsewhere (prison, a link)
                }
                std::fprintf(stderr, "  now at %d,%d (%s)\n", g.tx, g.ty, g.place.c_str());
                continue;
            }
            if (tok == "SHIPS") {   // every ship, with the column and row on the nearest town's map (for staging scenes; tests)
                for (const Thing &th : g.items.things) {
                    if (th.gone || g.items.kinds[th.kind].shape != "ship") continue;
                    const Settlement *best = nullptr;
                    float bd = 1e9f;
                    for (auto &s : g.world.settlements) {
                        float d = std::hypot(th.x - (s.x0 + s.w * 0.5f), th.y - (s.y0 + s.h * 0.5f));
                        if (d < bd) { bd = d; best = &s; }
                    }
                    std::fprintf(stderr, "  %s at %d,%d", g.items.kinds[th.kind].id.c_str(), th.x, th.y);
                    if (best) std::fprintf(stderr, ": %s col %d row %d (map %dx%d)", best->name.c_str(), th.x - best->x0, best->y0 + best->h - 1 - th.y, best->w, best->h);
                    std::fprintf(stderr, "\n");
                }
                continue;
            }
            if (tok.rfind("WHERE:", 0) == 0) {
                int who = findPerson(tok.substr(6));
                if (who < 0) continue;
                Person &pp = g.people.people[who];
                std::fprintf(stderr, "  %s is at %d,%d,%d (%s), %s %s%s, %s\n", pp.name.c_str(), pp.x, pp.y, pp.z,
                             g.world.floorName(pp.x, pp.y, pp.z).c_str(), pp.activity.c_str(), pp.actPlace.c_str(),
                             pp.onRoad ? " (on the road)" : "", pp.away ? "AWAY" : pp.dead ? "DEAD" : pp.lying ? "lying down" : (pp.pathPos < pp.path.size() ? "walking" : "there"));
                if (pp.fighter) std::fprintf(stderr, "    health %.0f of %d%s%s\n", pp.hp, pp.maxHp, pp.target != -1 ? ", fighting" : "", pp.hasWork ? "" : ", NO POST");
                continue;
            }
            size_t k = 0;
            while (k < tok.size() && std::isalpha((unsigned char)tok[k])) k++;
            auto it = D.find(tok.substr(0, k));
            int n = k < tok.size() ? std::atoi(tok.c_str() + k) : 1;
            if (it == D.end()) { std::fprintf(stderr, o.play.empty() ? "bad step %s\n" : "  [no command %s: HELP lists them]\n", tok.c_str()); continue; }
            for (int i = 0; i < n; i++) {
                int bx = g.tx, by = g.ty;
                g.blockedCooldown = 0;
                g.tryStep(it->second.first, it->second.second, false);
                g.stepT = 1;
                g.from = g.to;
                g.syncRidden();
                if (g.tx == bx && g.ty == by) { std::fprintf(stderr, "  step %s %d: %s\n", tok.c_str(), i + 1, g.message.c_str()); break; }
                for (int q = 0; q < 2; q++) {   // a step takes a little time: people move, the watch looks
                    g.people.update(g.world, 0.1f, g.minutes, g.tx, g.ty, g.tz);
                    g.watchStep(); g.fight(0.1f);
                }
                g.takeRequests();   // a beat of the plot: the calendar moves on
                if (!g.people.notice.empty()) { g.say(g.people.notice); g.people.notice.clear(); }
                if (!g.message.empty()) { std::fprintf(stderr, "  %s\n", g.message.c_str()); g.message.clear(); }
            }
        }
        vec2 p = g.playerPos();
        g.camTarget = vec3(p.x, p.y, g.world.groundZ(p.x, p.y));
        if (!o.play.empty()) g.takeRequests();   // a beat still waiting is carried out before the game is saved
        if (!o.play.empty()) {   // a played run ends with nobody on the choosing screen or in the scenes
            int firstUnread = g.sceneNow + 1;
            if (g.creating) {   // still choosing: begun as you are
                std::fprintf(stderr, "  [the run ends on the choosing screen: you begin as you are]\n");
                g.finishCreating();
                firstUnread = 0;
            }
            if (g.sceneNow >= 0) {   // the rest of the opening scenes, read through, and you arrive
                for (int i = firstUnread; i < (int)g.scenes.size(); i++)
                    std::fprintf(stderr, "  [scene %d of %zu] %s\n", i + 1, g.scenes.size(), g.scenes[i].caption.c_str());
                g.endScenes();
            }
        }
        if (!o.play.empty() && g.bandState >= 1 && g.bandState <= 3) g.people.player["band-waiting"] = "1";   // still on the road next time
        std::fprintf(stderr, "after the walk: tile %d,%d, %s\n", g.tx, g.ty, g.place.c_str());
        if (!o.play.empty()) {   // playing by commands: the game goes on next time from here
            if (g.panel == 1) g.panel = 0;   // the picture shows the world, not what you carry
            g.journal = (tok.rfind("STORIES", 0) == 0 || tok == "TURN" || tok == "TURNBACK") && g.journal;   // or the journal, if STORIES (and its pages) came last
            if (!g.writeSave(g.savePath, true)) { glfwTerminate(); return 1; }
        }
        if (!shot) { glfwTerminate(); return 0; }   // a test only: no window, no saving
    }

    if (shot) {
        int W = o.w, H = o.h;
        GLuint msFbo, msColor, msDepth, fbo, color;
        glGenFramebuffers(1, &msFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
        glGenRenderbuffers(1, &msColor);
        glBindRenderbuffer(GL_RENDERBUFFER, msColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, W, H);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor);
        glGenRenderbuffers(1, &msDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, msDepth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, W, H);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { std::cerr << "picture framebuffer incomplete\n"; return 1; }
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenRenderbuffers(1, &color);
        glBindRenderbuffer(GL_RENDERBUFFER, color);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, W, H);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
        g.paintPendingPortrait();   // one asked for during the walk, when no picture could be drawn
        if (!o.portraitOut.empty()) {
            g.creating = false;   // a portrait of you as you chose to be, not of the choosing screen
            bool ok = g.paintPortrait(o.portraitOut);
            std::fprintf(stderr, ok ? "wrote the portrait %s\n" : "could not write the portrait %s\n", o.portraitOut.c_str());
            glfwTerminate();
            return ok ? 0 : 1;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
        g.render(W, H, W / 1440.0f * 1.0f + 0.0f, true);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        glBlitFramebuffer(0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        g.savePng(o.shot, W, H);
        GLenum err = glGetError();
        if (err) std::fprintf(stderr, "GL error 0x%x\n", err);
        std::fprintf(stderr, "wrote %s (%zu chunks drawn)\n", o.shot.c_str(), g.gfx.size());
        glfwTerminate();
        return 0;
    }

    glfwSetKeyCallback(g.win, onKey);
    glfwSetCharCallback(g.win, onChar);
    glfwSetScrollCallback(g.win, onScroll);
    glfwSetCursorPosCallback(g.win, onCursor);
    glfwSetMouseButtonCallback(g.win, onMouse);
    double last = glfwGetTime(), start = last;
    double lastAutosave = last;
    int frames = 0;
    while (!glfwWindowShouldClose(g.win)) {
        g.paintPendingPortrait();   // (a portrait asked for when no picture could be drawn)
        if (o.quitAfter > 0 && glfwGetTime() - start > o.quitAfter) {
            std::fprintf(stderr, "%d frames in %.1f s (%.0f per second), %zu chunks drawn\n", frames, glfwGetTime() - start,
                         frames / (glfwGetTime() - start), g.gfx.size());
            break;
        }
        frames++;
        glfwPollEvents();
        double t = glfwGetTime();
        float dt = (float)std::min(0.1, t - last);
        last = t;
        g.update(dt);
        if (autosaveDue(t, lastAutosave, o.autosave, g.saveIsSafe())) {
            lastAutosave = t;
            if (g.writeSave(g.savePath, false)) std::fprintf(stderr, "autosaved the game in %s\n", g.savePath.c_str());
        }
        int fw, fh, ww, wh;
        glfwGetFramebufferSize(g.win, &fw, &fh);
        glfwGetWindowSize(g.win, &ww, &wh);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        g.render(fw, fh, (float)fw / std::max(1, ww), false);
        glfwSwapBuffers(g.win);
        // rest until the next picture is due: at most o.fps a second, and only 4 a second
        // while the window is in the background or hidden (keys and the mouse still wake it)
        bool away = !glfwGetWindowAttrib(g.win, GLFW_FOCUSED) || glfwGetWindowAttrib(g.win, GLFW_ICONIFIED);
        float cap = away ? 4.0f : o.fps;
        if (cap > 0) {
            double due = t + 1.0 / cap;
            for (double now2 = glfwGetTime(); now2 < due && !glfwWindowShouldClose(g.win); now2 = glfwGetTime())
                glfwWaitEventsTimeout(due - now2);
        }
    }
    {
        if (g.sceneNow >= 0) g.endScenes();   // closed during the scenes: you have arrived
        if (g.writeSave(g.savePath, false)) std::fprintf(stderr, "saved the game in %s\n", g.savePath.c_str());
    }
    glfwTerminate();
    return 0;
}
