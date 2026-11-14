// world.h: the tile world of Europe, generated from the old map plus hand-made towns.
//
// Coordinates: tile (X,Y) covers the square [X,X+1] x [Y,Y+1]; X grows east, Y grows
// north, z is height. Heights are stored at tile corners ("vertices").
#pragma once
#include <cstdint>
#include <set>
#include <map>
#include <string>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "vmath.h"

enum Terrain : uint8_t {
    T_DEEP, T_WATER, T_SAND, T_MARSH, T_GRASS, T_MEADOW, T_SCRUB, T_ROCK, T_SNOW,
    T_FIELD, T_ROAD, T_STREET, T_SQUARE, T_GARDEN, T_YARD,
    T_FLOOR_WOOD, T_FLOOR_STONE, T_FLOOR_TILE, T_BRIDGE,
    T_VOID,          // on an upper floor or underground: nothing here
    T_COUNT
};

enum Feature : uint8_t {
    F_NONE, F_OAK, F_PINE, F_FRUIT, F_POPLAR, F_BUSH, F_BOULDER,
    F_PILLAR, F_TABLE, F_HEARTH, F_BED, F_BENCH, F_ALTAR, F_WELL, F_FOUNTAIN,
    F_STALL, F_CROSS, F_CHEST, F_HAY, F_BARREL, F_STATUE, F_STAGE,
    F_LADDER,        // a ladder against a wall (var: 0..3, the side of the wall: west, east, south, north)
    F_HATCH,         // a trapdoor in the floor, with a ladder going down
    F_STAIRS,        // stone stairs (in towers)
    F_SHAFT,         // a quarry shaft with its windlass
    F_BELL,          // a bell in a frame
    F_ANVIL, F_FORGE, F_COUNTER, F_OVEN,
    F_OBELISK,       // an Egyptian obelisk on a pedestal (Rome)
    F_COLUMN,        // a triumphal column with a statue on top
    F_MOORING,       // a mooring post in the water (var 1: a boat is tied up here when the world is made)
    F_TENT,          // a soldier's tent of canvas (var & 1: the ridge north-south)
    F_GABION,        // gabions: wicker baskets filled with earth, a field fortification
    F_CANNON,        // a gun on its carriage (var 0..3: it points west, east, south, north)
    F_COUNT
};
bool featureBlocksWalk(int feature);   // trees, tables, beds ... (world.cc)


enum WallKind : uint8_t {
    W_NONE, W_WALL, W_WINDOW, W_DOOR, W_CITYWALL, W_TOWER, W_GATE, W_PARAPET, W_HEDGE,
    W_EARTHWORK,     // a rampart of earth, turfed (the King of Sweden's lines round Nuremberg)
    W_COUNT
};

struct Tile {
    uint8_t terrain = T_GRASS;
    uint8_t feature = F_NONE;
    uint8_t wall = W_NONE;
    uint8_t var = 0;       // variation: crop type, colour jitter, facing of a feature
    uint16_t bld = 0;      // building this tile belongs to (0 = none)
};

// A piece of a building's above-ground shape. Pieces marked `upper` vanish when the
// player is inside that building.
enum PartKind : uint8_t {
    P_BOX, P_GABLE, P_HIP, P_MANSARD, P_PYRAMID, P_PRISM, P_CONE, P_DOME,
    P_STEPGABLE, P_CRENEL, P_WINDOWS, P_ARCH, P_BUTTRESS, P_ROSE, P_TIMBER,
    P_BALUSTRADE,   // rail on posts around a flat roof: a = (x0,y0,z), b = (x1,y1,z+height)
    P_RING,         // ring roof of a round building: a = centre (z = eaves), b = (outer r, inner r, ridge z)
    P_RINGWALL,     // inward-facing wall of a ring: a = centre (z0), b = (r, -, z1)
    P_ELLRING       // an elliptical ring wall with rows of arches (an amphitheatre): a = centre (z0),
                    // b = (rx, ry, height), p1 = thickness, p2 = how ruined (0..1), sides = segments
};

struct Part {
    PartKind kind;
    vec3 a, b;          // box corners (min, max); for round things: centre and (radius, -, height)
    vec3 color;
    vec3 color2{0.85f, 0.8f, 0.7f};  // gable ends, window frames, ...
    int axis = 0;       // 0: ridge along x, 1: ridge along y
    int sides = 8;      // for prisms and cones
    float p1 = 0, p2 = 0;  // extra shape parameters
    bool upper = true;  // hidden when the player is inside
};

struct Style;

struct Building {
    std::string name;   // empty for ordinary houses
    std::string kind;   // house, cathedral, church, palace, fortress, ...
    int x0, y0, x1, y1; // footprint in world tiles, inclusive
    float ground;       // floor height
    int style = 0;
    vec3 wallColor, trimColor, roofColor;
    std::vector<Part> parts;
    bool home = false;  // your family's farmhouse (map legend option "home"): its beds and things are yours
    // map legend option ruined=0..1 (a plain chapel: the cisterns of Hippo): roofless, its walls
    // broken to uneven heights, ruinH above the floor where whole (ruinTop)
    float ruined = 0, ruinH = 0;
};

// how high a ruined building's wall stands at tile X,Y above its floor: a smooth line along each
// wall (value noise in X+Y, a few tiles to a bump) with a little ragged edge, the corners
// standing longest (city.cc chapel, mesh.cc wallTop)
inline float ruinTop(const Building &b, int X, int Y) {
    auto h01 = [](int x, int y, uint32_t s) {
        uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float)((h ^ (h >> 16)) & 0xffffffu) / 16777215.0f;
    };
    float u = (X + Y) / 3.5f;
    int i = (int)u - (u < 0);
    float f = u - i, sm = f * f * (3 - 2 * f);
    bool side = (X == b.x0 || X == b.x1);   // the two walls of a corner get different lines
    float line = h01(i, side, 91) * (1 - sm) + h01(i + 1, side, 91) * sm;
    float r = 0.8f * line + 0.2f * h01(X, Y, 92);
    bool corner = (X == b.x0 || X == b.x1) && (Y == b.y0 || Y == b.y1);
    if (corner) r *= 0.3f;
    float t = b.ruinH * (1.0f - b.ruined * r);
    return t < 0.25f ? 0.25f : t;
}

struct Style {
    std::string name;
    std::vector<vec3> walls;     // wall colours to choose from
    vec3 trim{0.9f, 0.88f, 0.82f};
    std::vector<vec3> roofs;     // roof colours
    std::string roof = "gable";  // gable, hip, mansard, flat
    std::vector<std::string> roofTypes{"gable"};  // chosen at random, house by house
    float timberChance = 0;      // chance that a house is half-timbered
    int minStoreys = 2, maxStoreys = 3;
    float storey = 1.25f;        // height of an upper storey
    float roofPitch = 1.0f;      // roof height / half-width
    bool timber = false;         // half-timbered upper storeys
    bool dormers = false;
    bool gableFront = false;     // the gable end faces the street (the ridge runs away from it)
    bool jetty = false;          // each upper storey juts out over the one below
    bool balustrade = false;     // flat roofs get a balustrade
    vec3 chimney{0, 0, 0};       // chimney colour (0: the wall colour)
    float chimneyTall = 0.7f;    // how far chimneys rise above the ridge
    bool funnel = false;         // chimneys end in a wide funnel (Venice)
    bool stepGable = false;      // stepped gable ends (Amsterdam)
    float portico = 0;           // chance that a house carries its upper storeys over the street on an arcade (Padua)
    int lotWMin = 6, lotWMax = 9, lotDMin = 6, lotDMax = 9;   // house lots: widths, depths (tiles)
};

struct Settlement {
    std::string name;
    std::string file;
    int x0 = 0, y0 = 0, w = 0, h = 0;   // rectangle in world tiles (x0,y0 = south-west corner)
    float ground = 0.8f;                // ground height in town
    float margin = 14;                  // width of the band where terrain blends to the ground
    int style = 0;
    std::vector<char> chars;            // the text map, row-major with row 0 = north edge
    std::vector<Tile> tiles;            // tiles written by the town (valid where chars != ' ')
    std::vector<std::pair<int, int>> waterExits;  // where its rivers leave the map (world coords)
    bool hamlet = false;                // a farm, a hut: not a town
    // a hamlet line's options (world.txt): the template drawn in another style, its landmarks
    // renamed ("style=german I=the_Golden_Eagle": the post houses)
    std::string styleOverride;
    std::map<char, std::string> legendNames;
    // a camp pitched later (the town file's "camp EVENT"): its tents, gabions and guns ('n' 'v'
    // 'u' in the map) stand only once that event has happened (People::pitchCamps)
    std::string campEvent, campUntil;
    struct CampSpot { int x, y; uint8_t feature, var; };
    std::vector<CampSpot> campSpots;
    bool campUp = false;
    bool inside(int X, int Y) const { return X >= x0 && X < x0 + w && Y >= y0 && Y < y0 + h; }
    char ch(int X, int Y) const {   // character at world tile, ' ' outside
        if (!inside(X, Y)) return ' ';
        int col = X - x0, row = (y0 + h - 1) - Y;
        return chars[row * w + col];
    }
    Tile &tileAt(int X, int Y) {
        int col = X - x0, row = (y0 + h - 1) - Y;
        return tiles[row * w + col];
    }
};

constexpr int CH = 32;  // chunk size in tiles

// sizes relative to a person, who is about 1.1 tall and one tile wide
constexpr float GF_H = 2.2f;        // ground-floor walls
constexpr float DOOR_H = 1.6f;      // doorways
constexpr float TOWNWALL_H = 4.6f;  // town walls

struct Chunk {
    int cx, cy;
    Tile tiles[CH * CH];
    float vh[(CH + 1) * (CH + 1)];   // land heights at tile corners
    bool dirty = true;               // drawing must be rebuilt
    Tile &t(int lx, int ly) { return tiles[ly * CH + lx]; }
    float &h(int lx, int ly) { return vh[ly * (CH + 1) + lx]; }
};

// An upper floor, a tower top, a wall walk, a cellar or a tunnel network: a small floor
// plan at floor number z (1, 2 above the ground; -1, -2 below), over or under the map.
struct Level {
    std::string name;
    int bld = 0;               // the building it belongs to, if any
    int z = 1;
    int x0 = 0, y0 = 0, w = 0, h = 0;
    float floor = 0;           // height of the floor
    float wallH = GF_H;        // height of its walls
    bool hideBuilding = true;  // hide the building's upper storeys and roof while here
    bool drawFloor = true;     // draw the floor (tower tops and wall walks stand on the roof itself)
    bool drawWalls = true;
    bool underground = false;
    bool parapet = false;      // a tower top: beyond its edge is the parapet (drawn by the building)
    vec3 wallColor{0.8f, 0.76f, 0.68f};
    std::vector<Tile> tiles;   // T_VOID where there is nothing
    bool inside(int X, int Y) const { return X >= x0 && Y >= y0 && X < x0 + w && Y < y0 + h; }
    Tile &at(int X, int Y) { return tiles[(Y - y0) * w + (X - x0)]; }
    void init(int X0, int Y0, int X1, int Y1) {
        x0 = X0; y0 = Y0; w = X1 - X0 + 1; h = Y1 - Y0 + 1;
        Tile v; v.terrain = T_VOID;
        tiles.assign((size_t)w * h, v);
    }
};

// Walking into the ladder (or stair, trapdoor, shaft) at (x,y,z) takes you to the given
// place on another floor.
struct Link {
    int x, y, z;
    std::string verb;   // "climb up", "climb down", ...
};

struct Legacy {
    int w = 0, h = 0;
    std::vector<int8_t> geo, ff;
    bool load(const std::string &dir);
    // redraw cells: rows[0] is row V0 (the northernmost), rows[k][c] the cell (U0 + c, V0 - k);
    // '~' water, '.' low land, '-' land, '=' high land, 't' / 'T' the same with trees, ' ' as it was
    void edit(int u0, int v0, const std::vector<std::string> &rows);
    void derive();   // after editing: rivers made 4-connected, water and tree densities
    int geoAt(int i, int j) const {
        i = std::max(0, std::min(w - 1, i)); j = std::max(0, std::min(h - 1, j));
        return geo[i * h + j];
    }
    int ffAt(int i, int j) const {
        i = std::max(0, std::min(w - 1, i)); j = std::max(0, std::min(h - 1, j));
        return ff[i * h + j];
    }
};

// a stretch of coast where the land ends in cliffs (chalk at Dover)
struct Cliff {
    std::string name;
    float x0, y0, x1, y1;   // world tiles
    float height;
    vec3 color;
};

// a sign hung over the street from the wall beside a door, showing the trade of the house: an
// inn's tankard and lantern, a smith's anvil (people.cc puts them up; mesh.cc draws them)
struct Sign { int x, y, ox, oy; float z; std::string emblem; int bld; };

struct World {
    int S = 10;             // tiles per legacy cell
    float clockRate = 0.5f;   // world.txt "clock": game minutes for each second of real time
    float journeyMinutes = 6; // world.txt "journey": game minutes for each step on the open road
    std::set<std::string> inlandTowns;   // world.txt "inland": towns whose rivers join old-map water that never reaches the sea
    float kS = 2;           // S / 5: sizes of fields, forests, wiggles grow with the scale
    float kH = 1.5f;        // heights of the land, relative to a person
    int W = 0, H = 0;       // world size in tiles
    Legacy L;
    std::vector<Style> styles;
    std::vector<Settlement> settlements;
    std::vector<Building> buildings;     // building id = index + 1
    std::vector<Building> areas;      // named open places that are not buildings (the Place Royale): name, kind "place", rectangle; for people's directions
    std::vector<Sign> signs;
    std::vector<Cliff> cliffs;
    std::vector<Level> levels;
    std::unordered_map<int64_t, std::vector<int>> levelIndex;   // (chunk, floor) -> levels
    std::unordered_map<int64_t, Link> links;
    // links, found without the hash map (linkAt is asked about every neighbour a route search looks at): an
    // open-addressing table of (key3, pointer into `links`)
    struct LinkSlot { int64_t key; const Link *link; };
    std::vector<LinkSlot> linkTab;                 // size a power of two (or 0); key INT64_MIN = empty
    std::vector<uint64_t> linkBits;                // one bit for each tile of the map: a link starts here, on some floor (a quick no)
    std::vector<std::array<int, 3>> linkSources;   // the tiles that hold links (once each), in the order made
    // the link tiles that lead to (X,Y,z): for a route search that works backwards from its goal (made when first asked)
    const std::vector<std::array<int, 3>> &linksInto(int X, int Y, int z);
    std::unordered_map<int64_t, std::vector<std::array<int, 3>>> linksIntoMap;
    size_t linksIntoFor = (size_t)-1;              // linkSources.size() when linksIntoMap was made
    std::unordered_set<int64_t> roads;   // road tiles
    // roads that stop short at water (too wide to bridge), for --stats: where, and the line
    struct RoadStop { vec2 at; std::string line; };
    std::vector<RoadStop> roadStops;
    std::unordered_set<int64_t> carved;  // river channels joining town rivers to the land's rivers
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> chunks;
    // a flat index in front of `chunks` for the chunks of the world (and a margin): chunkFlat[(cy - flatY0) * flatW + cx - flatX0]
    // is the chunk or nullptr; chunk() also remembers the last one it returned
    std::vector<Chunk *> chunkFlat;
    int flatX0 = 0, flatY0 = 0, flatW = 0, flatH = 0;
    Chunk *lastChunk = nullptr;
    void clearChunks();                            // forget every chunk (also the index and the last-chunk cache)
    vec2 start{0, 0};
    std::string dataDir;

    static int64_t key(int X, int Y) { return ((int64_t)X << 32) ^ (uint32_t)Y; }
    static int64_t key3(int X, int Y, int z) { return ((int64_t)X << 36) ^ ((int64_t)(Y & 0xFFFFFF) << 8) ^ (int64_t)(z + 64); }

    bool init(const std::string &dataDir, int scale);
    bool loadWorldFile(const std::string &fn);
    bool loadStyles(const std::string &fn);
    int styleIndex(const std::string &name) const;

    // generation
    float landFrac(float X, float Y) const;      // > 0.5 means land, before towns
    float cliffAt(float X, float Y, const Cliff **which = nullptr) const;  // 0..1, how much of a cliff coast
    float seaNear(float X, float Y) const;       // fraction of sea around, from the old map
    float baseElev(float X, float Y) const;      // rough height from the old map
    bool waterAt(int X, int Y) const;            // water after towns and carving
    Tile baseTile(int X, int Y) const;           // terrain and vegetation
    // the same, looking only at a list of settlements (in the order of `settlements`) that holds every
    // one that matters near the point: chunk() makes the list once per chunk (see nearSettlements)
    using SettlementList = std::vector<const Settlement *>;
    bool waterAtIn(int X, int Y, const SettlementList &near) const;
    Tile baseTileIn(int X, int Y, const SettlementList &near) const;
    SettlementList nearSettlements(int X0, int Y0, int X1, int Y1) const;
    Chunk *chunk(int cx, int cy) {               // generated on demand
        if (lastChunk && lastChunk->cx == cx && lastChunk->cy == cy) return lastChunk;
        return chunkSlow(cx, cy);
    }
    Chunk *chunkSlow(int cx, int cy);
    Chunk *chunkIfReady(int cx, int cy);

    // queries
    bool inWorld(int X, int Y) const { return X >= 0 && Y >= 0 && X < W && Y < H; }
    Tile &tile(int X, int Y);
    float cornerH(int X, int Y);                 // stored vertex height
    float groundZ(float x, float y);             // height of the surface you stand on
    bool passable(int X, int Y);
    bool blocksDiagonal(int X, int Y);
    std::string placeName(int X, int Y, int *bldOut = nullptr);
    Building *buildingAt(int X, int Y);
    void setFeature(int X, int Y, int z, uint8_t f, uint8_t var);   // change the world after it is built
    bool addSign(int bld, const std::string &emblem);   // one a building, beside its street door

    // floors
    int addLevel(Level &&L);
    Level *levelAt(int X, int Y, int z);
    Tile *tileAt(int X, int Y, int z);          // nullptr where there is nothing
    bool passable(int X, int Y, int z);
    bool blocksDiagonal(int X, int Y, int z);
    float floorAt(float x, float y, int z);      // the height you stand at
    void addLink(int X, int Y, int z, int X2, int Y2, int z2, const std::string &verb);
    const Link *linkAt(int X, int Y, int z) const;
    std::string floorName(int X, int Y, int z);

    // roads
    bool addRoad(const std::vector<vec2> &pts, int width, const std::string &line = "");   // false: stopped at water
    // the length of the way by road from settlement `from` to each settlement (-1: no road joins
    // them), going along the road tiles and through the towns on the way (the post, --stats)
    std::vector<float> roadDistances(int from);
    // the shortest road from settlement FROM to settlement TO, tile by tile with the road distance reached at each
    // (a stretch through a town between its road tiles is one jump); false if no road joins them
    struct RoadPt { int x, y; float d; };
    bool roadRoute(int from, int to, std::vector<RoadPt> &out);
    std::vector<std::vector<int64_t>> roadTouch;   // per settlement: the road tiles at its edge (built once)
    void carveChannel(vec2 a, vec2 b, int width);
    void carvePath(const std::vector<vec2> &pts, int width);
    void joinRivers();
    void buildUnderground(const std::string &name, int z, int X0, int Y0, int X1, int Y1, int sx, int sy);
    struct UndergroundSpec { std::string name; int z, x0, y0, x1, y1, sx, sy; };
    std::vector<UndergroundSpec> undergroundSpecs;
    // follow water from (X,Y); true if it reaches the open sea within maxTiles tiles
    bool reachesSea(int X, int Y, int maxTiles, int &seaX, int &seaY, int &visited);
};

bool loadSettlement(World &w, Settlement &s, const std::string &fn, int cx, int cy);
