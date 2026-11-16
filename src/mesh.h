// mesh.h: turning tiles and building parts into flat-shaded triangles.
#pragma once
#include <vector>
#include "vmath.h"
#include "world.h"

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b;
    float tag;    // building id for pieces hidden when the player is inside it, else 0
    float flags;  // 0 ordinary, 1 glowing (fire, candles), 2 terrain (never cut away), 3 water, 4 window glass
};

struct MeshBuf {
    std::vector<Vertex> v;
    float tag = 0, flags = 0;
    void clear() { v.clear(); }
    void vert(vec3 p, vec3 n, vec3 c) {
        v.push_back({p.x, p.y, p.z, n.x, n.y, n.z, c.x, c.y, c.z, tag, flags});
    }
    // triangle with its normal pointing along `out` (any rough outward direction)
    void tri(vec3 a, vec3 b, vec3 c, vec3 col, vec3 out) {
        vec3 n = cross(b - a, c - a);
        if (length(n) < 1e-9f) return;
        if (dot(n, out) < 0) { std::swap(b, c); n = -n; }
        n = normalize(n);
        vert(a, n, col); vert(b, n, col); vert(c, n, col);
    }
    void quad(vec3 a, vec3 b, vec3 c, vec3 d, vec3 col, vec3 out) {
        tri(a, b, c, col, out);
        tri(a, c, d, col, out);
    }
    // axis-aligned box; `bottom` adds the underside
    void box(vec3 lo, vec3 hi, vec3 col, bool bottom = false, bool top = true);
    // n-gon prism around (cx,cy), from z0 to z1
    void prism(float cx, float cy, float z0, float z1, float r, int n, vec3 col, float rot = 0, bool top = true);
    void cone(float cx, float cy, float z0, float z1, float r, int n, vec3 col, float rot = 0);
    void blob(vec3 c, vec3 r, int seg, vec3 col, uint32_t seed, float jitter);
};

// chunk geometry: everything solid, and the water surface
void buildChunkMesh(World &w, Chunk &c, MeshBuf &solid, MeshBuf &water);
// an upper floor, a tower top, a cellar or a tunnel network
void buildLevelMesh(World &w, Level &L, MeshBuf &out);
// the above-ground parts of a building
void buildPartsMesh(const Building &b, int id, MeshBuf &out);
// what a figure wears
struct FigureLook {
    vec3 coat{0.42f, 0.12f, 0.12f}, legs{0.18f, 0.18f, 0.22f}, hat{0.1f, 0.09f, 0.09f}, apron{0.85f, 0.82f, 0.75f};
    vec3 skin{0.90f, 0.74f, 0.60f}, hair{0.30f, 0.20f, 0.12f};
    int hatKind = 1;   // 0 none, 1 broad hat, 2 cap, 3 coif, 4 helmet, 5 square cap, 6 hood, 7 biretta
    bool dress = false, robe = false, apronOn = false, halberd = false, lute = false;
    bool sashOn = false;   // a sash round the waist, knotted at the left hip (the Emperor's soldiers: red, their field sign)
    vec3 sash{0.62f, 0.10f, 0.10f};
    bool statue = false;   // all of one material: no leather boots, no white collar
    int weapon = 0;        // carried in the right hand: 0 nothing, 1 musket, 2 pike, 3 sword, 4 sabre
    float strike = 0;      // 0..1 while a blow is struck (set each frame by whoever draws the figure)
    float size = 1;        // a child is smaller ("child" in a look: 0.62)
    bool longHair = false; // hair down the back to the shoulders (the player may choose it)
    bool fringe = false;   // hair seen from the front too, under a hat: a fringe and locks at the sides (the player)
    // for the still scenes at the start of a new game (data/scenes.txt)
    float headK = 1;       // the head, larger for its size (a small child: 1.3)
    int carry = 0;         // 1 a swaddled baby in the arms, 2 a yoke with two pails, 3 a cap in the hand, 4 a little book held out
    vec3 babySkin{0.90f, 0.74f, 0.60f};
    float kneel = 0;       // above 0: kneeling on both knees (the body lowered to the knees, shins along the floor)
    bool sit = false;      // seated on a chair (drawn with the figure): thighs forward, shins down
    // who the person is (the look's words "old", "beard KIND", "stout"; woman from the person's sex)
    float age = 0;         // 0 in the prime of life .. 1 old: a stoop, grey hair, a bareheaded man thin on top
    int beard = 0;         // 0 none, 1 a moustache and a pointed tuft, 2 a full beard, 3 a moustache
    float stout = 0;       // 0..1 a belly, the coat and its buttons following it
    bool woman = false;    // in a gown (dress): a bodice, the skirt from a higher waist, a broad collar, narrower shoulders
    bool plain = false;    // "plain": no age, beard, stoutness or height drawn at random (People's drawLook)
};

// close up (the camera near: main.cc sets it each frame), people and animals are drawn finer:
// rounded heads with faces, tapered limbs bent at the knee and elbow, hands, shoes, round hats
void setFineFigures(bool on);
bool fineFigures();
// a little further out, the same figures with fewer sides and without the small features (main.cc sets it only
// while the people and animals about are built)
void setMidFigures(bool on);
// close up, furniture (tables, beds, barrels...) is drawn finer too: blocks of ground and upper
// floors are rebuilt when this changes (main.cc ensureChunk, the levels)
void setFineFeatures(bool on);
bool fineFeatures();
// the rounded pieces of the finer drawings, for items.cc as well: a tapered tube along M's z axis,
// and an ellipsoid (or a band of one)
void meshTube(MeshBuf &m, const mat4 &M, float z0, float z1, float rx0, float ry0, float rx1, float ry1, int n, vec3 col,
              float cx = 0, float cy = 0, bool cap0 = true, bool cap1 = true);
void meshEllipsoid(MeshBuf &m, const mat4 &M, vec3 c, vec3 r, int nLon, int nLat, vec3 col,
                   float lat0 = -1.5707963f, float lat1 = 1.5707963f, float lon0 = 0, float lon1 = 6.2831853f);

// a walking figure at pos, facing `facing`; phase in [0,1) animates the legs
void buildFigure(MeshBuf &out, vec3 pos, float facing, float phase, vec3 coat, vec3 hose, vec3 hat);
void buildFigure(MeshBuf &out, vec3 pos, float facing, float phase, const FigureLook &look, bool lying);
