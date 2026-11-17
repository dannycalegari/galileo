// mesh.cc: low-poly geometry for terrain, walls, vegetation, furniture and buildings.
#include "animals.h"
#include "mesh.h"
#include "items.h"
#include "noise.h"

// ------------------------------------------------------------------ primitives

void MeshBuf::box(vec3 lo, vec3 hi, vec3 col, bool bottom, bool top) {
    vec3 c = (lo + hi) * 0.5f;
    vec3 p[8] = {
        {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
        {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    (void)c;
    quad(p[0], p[1], p[5], p[4], col, vec3(0, -1, 0));
    quad(p[1], p[2], p[6], p[5], col, vec3(1, 0, 0));
    quad(p[2], p[3], p[7], p[6], col, vec3(0, 1, 0));
    quad(p[3], p[0], p[4], p[7], col, vec3(-1, 0, 0));
    if (top) quad(p[4], p[5], p[6], p[7], col, vec3(0, 0, 1));
    if (bottom) quad(p[0], p[1], p[2], p[3], col, vec3(0, 0, -1));
}

void MeshBuf::prism(float cx, float cy, float z0, float z1, float r, int n, vec3 col, float rot, bool top) {
    for (int i = 0; i < n; i++) {
        float a0 = rot + 2 * PI_F * i / n, a1 = rot + 2 * PI_F * (i + 1) / n;
        vec3 p0(cx + r * std::cos(a0), cy + r * std::sin(a0), z0);
        vec3 p1(cx + r * std::cos(a1), cy + r * std::sin(a1), z0);
        vec3 out((p0.x + p1.x) * 0.5f - cx, (p0.y + p1.y) * 0.5f - cy, 0);
        quad(p0, p1, vec3(p1.x, p1.y, z1), vec3(p0.x, p0.y, z1), col, out);
        if (top) tri(vec3(cx, cy, z1), vec3(p0.x, p0.y, z1), vec3(p1.x, p1.y, z1), col, vec3(0, 0, 1));
    }
}

void MeshBuf::cone(float cx, float cy, float z0, float z1, float r, int n, vec3 col, float rot) {
    for (int i = 0; i < n; i++) {
        float a0 = rot + 2 * PI_F * i / n, a1 = rot + 2 * PI_F * (i + 1) / n;
        vec3 p0(cx + r * std::cos(a0), cy + r * std::sin(a0), z0);
        vec3 p1(cx + r * std::cos(a1), cy + r * std::sin(a1), z0);
        vec3 out((p0.x + p1.x) * 0.5f - cx, (p0.y + p1.y) * 0.5f - cy, r * 0.3f);
        tri(p0, p1, vec3(cx, cy, z1), col, out);
    }
}

void MeshBuf::blob(vec3 c, vec3 r, int seg, vec3 col, uint32_t seed, float jit) {
    const float lat[4] = {-PI_F / 2, -0.55f, 0.45f, PI_F / 2};
    std::vector<vec3> ring[4];
    for (int k = 0; k < 4; k++) {
        int n = (k == 0 || k == 3) ? 1 : seg;
        for (int i = 0; i < n; i++) {
            float a = 2 * PI_F * (i + 0.5f * (k % 2)) / seg;
            float cl = std::cos(lat[k]), sl = std::sin(lat[k]);
            float j = 1.0f + jit * (hash01(i, k, seed) - 0.5f);
            ring[k].push_back(vec3(c.x + r.x * cl * std::cos(a) * j, c.y + r.y * cl * std::sin(a) * j, c.z + r.z * sl * j));
        }
    }
    for (int k = 0; k < 3; k++) {
        for (int i = 0; i < seg; i++) {
            vec3 shade = col * (0.9f + 0.2f * hash01(i, k, seed + 1));
            if (k == 0) {
                vec3 a = ring[1][i], b = ring[1][(i + 1) % seg];
                tri(ring[0][0], a, b, shade, (a + b) * 0.5f - c);
            } else if (k == 2) {
                vec3 a = ring[2][i], b = ring[2][(i + 1) % seg];
                tri(a, b, ring[3][0], shade, (a + b) * 0.5f - c);
            } else {
                vec3 a = ring[1][i], b = ring[1][(i + 1) % seg], d = ring[2][i], e = ring[2][(i + 1) % seg];
                // rings are offset by half a step: two triangles per step
                tri(a, b, e, shade, (a + b + e) * (1.0f / 3) - c);
                tri(a, e, d, shade * 0.97f, (a + e + d) * (1.0f / 3) - c);
            }
        }
    }
}

// ------------------------------------------------------------------ colours

static vec3 terrainColor(int terrain, int var, int X, int Y) {
    vec3 c;
    switch (terrain) {
        case T_DEEP: c = vec3(0.10f, 0.22f, 0.40f); break;
        case T_WATER: c = vec3(0.17f, 0.34f, 0.48f); break;
        case T_SAND: c = vec3(0.84f, 0.78f, 0.56f); break;
        case T_MARSH: c = vec3(0.36f, 0.45f, 0.30f); break;
        case T_GRASS: c = vec3(0.37f, 0.50f, 0.25f); break;
        case T_MEADOW: c = vec3(0.45f, 0.56f, 0.30f); break;
        case T_SCRUB: c = vec3(0.56f, 0.54f, 0.33f); break;
        case T_ROCK: c = vec3(0.54f, 0.52f, 0.49f); break;
        case T_SNOW: c = vec3(0.93f, 0.94f, 0.96f); break;
        case T_FIELD: {
            static const vec3 crops[5] = {{0.84f, 0.72f, 0.36f}, {0.74f, 0.70f, 0.42f}, {0.48f, 0.60f, 0.26f},
                                          {0.52f, 0.40f, 0.27f}, {0.62f, 0.66f, 0.36f}};
            c = crops[var % 5];
            break;
        }
        case T_ROAD: c = vec3(0.60f, 0.51f, 0.38f); break;
        case T_STREET: c = vec3(0.56f, 0.54f, 0.50f); break;
        case T_SQUARE: c = vec3(0.70f, 0.67f, 0.60f); break;
        case T_GARDEN: c = vec3(0.38f, 0.53f, 0.28f); break;
        case T_YARD: c = vec3(0.55f, 0.47f, 0.36f); break;
        case T_FLOOR_WOOD: c = vec3(0.56f, 0.39f, 0.23f); break;
        case T_FLOOR_STONE: c = vec3(0.68f, 0.66f, 0.61f); break;
        case T_FLOOR_TILE: c = ((X + Y) & 1) ? vec3(0.64f, 0.36f, 0.27f) : vec3(0.82f, 0.78f, 0.70f); break;
        case T_BRIDGE: c = vec3(0.66f, 0.62f, 0.55f); break;
        default: c = vec3(1, 0, 1);
    }
    float j = 0.95f + 0.10f * hash01(X, Y, 77);
    if (terrain == T_FIELD) j = 0.97f + 0.06f * hash01(X, Y, 77);
    return c * j;
}

// ------------------------------------------------------------------ features

static bool featureFine(MeshBuf &m, int f, int var, float cx, float cy, float z, uint32_t seed);
static bool gFineFeatures2();
static void feature(MeshBuf &m, int f, int var, float cx, float cy, float z, uint32_t seed) {
    if (gFineFeatures2() && featureFine(m, f, var, cx, cy, z, seed)) return;
    float s = 0.85f + 0.3f * (var / 255.0f);
    vec3 trunk(0.38f, 0.27f, 0.17f);
    switch (f) {
        case F_OAK: {
            float k = 2.0f * s;
            m.prism(cx, cy, z, z + 0.75f * k, 0.16f, 6, trunk);
            vec3 g(0.24f + 0.06f * hash01(var, 1, seed), 0.42f + 0.08f * hash01(var, 2, seed), 0.17f);
            m.blob(vec3(cx, cy, z + 1.25f * k), vec3(0.58f, 0.58f, 0.5f) * k, 8, g, seed, 0.35f);
            m.blob(vec3(cx + 0.18f * k, cy - 0.12f * k, z + 1.65f * k), vec3(0.38f, 0.38f, 0.34f) * k, 7, g * 1.08f, seed + 9, 0.3f);
            m.blob(vec3(cx - 0.25f * k, cy + 0.15f * k, z + 1.35f * k), vec3(0.32f, 0.32f, 0.3f) * k, 6, g * 0.94f, seed + 17, 0.3f);
            break;
        }
        case F_FRUIT: {
            float k = 1.6f * s;
            m.prism(cx, cy, z, z + 0.55f * k, 0.11f, 5, trunk);
            vec3 g(0.34f, 0.52f, 0.22f);
            m.blob(vec3(cx, cy, z + 0.95f * k), vec3(0.45f, 0.45f, 0.38f) * k, 7, g, seed, 0.3f);
            vec3 fruit = (var & 1) ? vec3(0.85f, 0.25f, 0.15f) : vec3(0.92f, 0.62f, 0.15f);
            for (int q = 0; q < 7; q++) {
                float a = q * 0.9f + var;
                vec3 p(cx + 0.4f * k * std::cos(a), cy + 0.4f * k * std::sin(a), z + (0.85f + 0.2f * (q % 2)) * k);
                m.box(p - vec3(0.07f, 0.07f, 0.07f), p + vec3(0.07f, 0.07f, 0.07f), fruit);
            }
            break;
        }
        case F_PINE: {
            float k = 2.0f * s;
            m.prism(cx, cy, z, z + 0.5f * k, 0.13f, 5, trunk);
            vec3 g(0.15f, 0.32f + 0.06f * hash01(var, 3, seed), 0.20f);
            float rot = var * 0.1f;
            m.cone(cx, cy, z + 0.35f * k, z + 1.25f * k, 0.55f * k, 8, g, rot);
            m.cone(cx, cy, z + 0.85f * k, z + 1.75f * k, 0.43f * k, 8, g * 1.07f, rot + 0.4f);
            m.cone(cx, cy, z + 1.35f * k, z + 2.25f * k, 0.30f * k, 8, g * 1.14f, rot + 0.8f);
            break;
        }
        case F_POPLAR: {
            float k = 2.0f * s;
            m.prism(cx, cy, z, z + 0.5f * k, 0.11f, 5, trunk);
            m.blob(vec3(cx, cy, z + 1.5f * k), vec3(0.26f, 0.26f, 1.1f) * k, 7, vec3(0.28f, 0.46f, 0.2f), seed, 0.2f);
            break;
        }
        case F_BUSH:
            m.blob(vec3(cx, cy, z + 0.3f), vec3(0.45f, 0.42f, 0.34f) * s, 6, vec3(0.32f, 0.44f, 0.22f), seed, 0.35f);
            break;
        case F_BOULDER:
            m.blob(vec3(cx, cy, z + 0.15f), vec3(0.45f, 0.4f, 0.32f) * s, 5, vec3(0.5f, 0.49f, 0.46f), seed, 0.45f);
            break;
        case F_PILLAR:
            if (var == 1) {   // a timber post (a byre's: halle ... timber in a town map)
                m.box(vec3(cx - 0.13f, cy - 0.13f, z), vec3(cx + 0.13f, cy + 0.13f, z + 3.45f), vec3(0.42f, 0.3f, 0.18f));
                break;
            }
            m.prism(cx, cy, z, z + 0.2f, 0.36f, 8, vec3(0.78f, 0.75f, 0.67f));
            m.prism(cx, cy, z, z + 4.4f, 0.26f, 8, vec3(0.84f, 0.81f, 0.73f));
            m.prism(cx, cy, z + 4.25f, z + 4.45f, 0.36f, 8, vec3(0.78f, 0.75f, 0.67f));
            break;
        case F_TABLE: {
            vec3 wood(0.48f, 0.32f, 0.18f);
            m.box(vec3(cx - 0.42f, cy - 0.3f, z + 0.48f), vec3(cx + 0.42f, cy + 0.3f, z + 0.56f), wood);
            for (int i = 0; i < 4; i++) {
                float lx = cx + ((i & 1) ? 0.34f : -0.34f), ly = cy + ((i & 2) ? 0.22f : -0.22f);
                m.box(vec3(lx - 0.04f, ly - 0.04f, z), vec3(lx + 0.04f, ly + 0.04f, z + 0.48f), wood * 0.85f);
            }
            break;
        }
        case F_HEARTH: {
            vec3 st(0.6f, 0.57f, 0.52f);
            // var 0: against west wall, 3: against north wall
            bool north = var == 3;
            float bx0 = north ? cx - 0.45f : cx - 0.5f, bx1 = north ? cx + 0.45f : cx - 0.05f;
            float by0 = north ? cy + 0.05f : cy - 0.45f, by1 = north ? cy + 0.5f : cy + 0.45f;
            m.box(vec3(bx0, by0, z), vec3(bx1, by1, z + 1.1f), st);
            vec3 inner = north ? vec3(cx, cy + 0.06f, z) : vec3(cx - 0.06f, cy, z);
            m.box(inner - vec3(north ? 0.25f : 0.02f, north ? 0.02f : 0.25f, 0) + vec3(0, 0, 0.05f),
                  inner + vec3(north ? 0.25f : 0.02f, north ? 0.02f : 0.25f, 0.45f), vec3(0.08f, 0.06f, 0.05f));
            m.flags = 1;
            m.blob(vec3(north ? cx : cx - 0.12f, north ? cy - 0.05f : cy, z + 0.15f), vec3(0.14f, 0.14f, 0.16f), 5,
                   vec3(1.0f, 0.55f, 0.15f), seed, 0.4f);
            m.flags = 0;
            break;
        }
        case F_BED: {
            static const vec3 blankets[4] = {{0.55f, 0.15f, 0.12f}, {0.2f, 0.3f, 0.5f}, {0.3f, 0.45f, 0.25f}, {0.6f, 0.5f, 0.3f}};
            vec3 wood(0.45f, 0.3f, 0.17f);
            m.box(vec3(cx - 0.38f, cy - 0.45f, z), vec3(cx + 0.38f, cy + 0.45f, z + 0.3f), wood);
            m.box(vec3(cx - 0.36f, cy - 0.43f, z + 0.3f), vec3(cx + 0.36f, cy + 0.2f, z + 0.4f), blankets[var % 4]);
            m.box(vec3(cx - 0.3f, cy + 0.2f, z + 0.3f), vec3(cx + 0.3f, cy + 0.42f, z + 0.42f), vec3(0.92f, 0.9f, 0.85f));
            m.box(vec3(cx - 0.38f, cy + 0.43f, z), vec3(cx + 0.38f, cy + 0.5f, z + 0.7f), wood);
            break;
        }
        case F_BENCH: {
            vec3 wood(0.5f, 0.34f, 0.2f);
            m.box(vec3(cx - 0.4f, cy - 0.15f, z + 0.3f), vec3(cx + 0.4f, cy + 0.15f, z + 0.36f), wood);
            m.box(vec3(cx - 0.36f, cy - 0.12f, z), vec3(cx - 0.3f, cy + 0.12f, z + 0.3f), wood * 0.85f);
            m.box(vec3(cx + 0.3f, cy - 0.12f, z), vec3(cx + 0.36f, cy + 0.12f, z + 0.3f), wood * 0.85f);
            break;
        }
        case F_ALTAR: {
            m.box(vec3(cx - 0.45f, cy - 0.32f, z), vec3(cx + 0.45f, cy + 0.32f, z + 0.15f), vec3(0.7f, 0.68f, 0.62f));
            m.box(vec3(cx - 0.3f, cy - 0.4f, z + 0.15f), vec3(cx + 0.3f, cy + 0.4f, z + 0.65f), vec3(0.9f, 0.88f, 0.84f));
            m.box(vec3(cx - 0.31f, cy - 0.41f, z + 0.45f), vec3(cx + 0.31f, cy + 0.41f, z + 0.66f), vec3(0.6f, 0.12f, 0.12f));
            vec3 gold(0.85f, 0.7f, 0.3f);
            m.box(vec3(cx - 0.03f, cy - 0.03f, z + 0.66f), vec3(cx + 0.03f, cy + 0.03f, z + 1.1f), gold);
            m.box(vec3(cx - 0.03f, cy - 0.12f, z + 0.92f), vec3(cx + 0.03f, cy + 0.12f, z + 0.97f), gold);
            m.flags = 1;
            for (int k = -1; k <= 1; k += 2)
                m.box(vec3(cx - 0.02f, cy + k * 0.3f - 0.02f, z + 0.66f), vec3(cx + 0.02f, cy + k * 0.3f + 0.02f, z + 0.82f), vec3(1, 0.85f, 0.5f));
            m.flags = 0;
            break;
        }
        case F_WELL: {
            vec3 st(0.62f, 0.6f, 0.55f);
            m.prism(cx, cy, z, z + 0.45f, 0.4f, 8, st, 0, false);
            m.prism(cx, cy, z, z + 0.4f, 0.3f, 8, vec3(0.1f, 0.15f, 0.2f), 0, true);
            vec3 wood(0.42f, 0.3f, 0.18f);
            m.box(vec3(cx - 0.36f, cy - 0.04f, z + 0.4f), vec3(cx - 0.3f, cy + 0.04f, z + 1.1f), wood);
            m.box(vec3(cx + 0.3f, cy - 0.04f, z + 0.4f), vec3(cx + 0.36f, cy + 0.04f, z + 1.1f), wood);
            m.box(vec3(cx - 0.4f, cy - 0.04f, z + 1.05f), vec3(cx + 0.4f, cy + 0.04f, z + 1.1f), wood);
            m.quad(vec3(cx - 0.45f, cy - 0.3f, z + 1.05f), vec3(cx + 0.45f, cy - 0.3f, z + 1.05f),
                   vec3(cx + 0.45f, cy, z + 1.3f), vec3(cx - 0.45f, cy, z + 1.3f), vec3(0.55f, 0.3f, 0.2f), vec3(0, -1, 1));
            m.quad(vec3(cx - 0.45f, cy + 0.3f, z + 1.05f), vec3(cx + 0.45f, cy + 0.3f, z + 1.05f),
                   vec3(cx + 0.45f, cy, z + 1.3f), vec3(cx - 0.45f, cy, z + 1.3f), vec3(0.55f, 0.3f, 0.2f), vec3(0, 1, 1));
            break;
        }
        case F_FOUNTAIN: {
            vec3 st(0.78f, 0.75f, 0.68f);
            m.prism(cx, cy, z, z + 0.4f, 0.75f, 8, st, PI_F / 8, false);
            m.prism(cx, cy, z, z + 0.33f, 0.66f, 8, vec3(0.3f, 0.5f, 0.62f), PI_F / 8, true);
            m.prism(cx, cy, z, z + 1.3f, 0.11f, 6, st);
            m.prism(cx, cy, z + 1.0f, z + 1.09f, 0.32f, 8, st);
            m.blob(vec3(cx, cy, z + 1.36f), vec3(0.1f, 0.1f, 0.14f), 5, st, seed, 0.1f);
            break;
        }
        case F_STALL: {
            vec3 wood(0.45f, 0.32f, 0.2f);
            for (int i = 0; i < 4; i++) {
                float lx = cx + ((i & 1) ? 0.4f : -0.4f), ly = cy + ((i & 2) ? 0.35f : -0.35f);
                m.box(vec3(lx - 0.03f, ly - 0.03f, z), vec3(lx + 0.03f, ly + 0.03f, z + 1.05f + ((i & 2) ? 0.2f : 0)), wood);
            }
            m.box(vec3(cx - 0.42f, cy - 0.3f, z + 0.5f), vec3(cx + 0.42f, cy + 0.3f, z + 0.56f), wood);
            static const vec3 cloth[3] = {{0.75f, 0.2f, 0.15f}, {0.2f, 0.35f, 0.65f}, {0.8f, 0.65f, 0.2f}};
            vec3 c1 = cloth[var % 3], c2(0.92f, 0.9f, 0.84f);
            for (int k = 0; k < 4; k++) {
                float x0 = cx - 0.48f + k * 0.24f, x1 = x0 + 0.24f;
                m.quad(vec3(x0, cy - 0.45f, z + 1.05f), vec3(x1, cy - 0.45f, z + 1.05f), vec3(x1, cy + 0.42f, z + 1.28f),
                       vec3(x0, cy + 0.42f, z + 1.28f), (k & 1) ? c2 : c1, vec3(0, -0.3f, 1));
            }
            break;
        }
        case F_TENT: {   // a ridge tent of canvas, its ridge along x (or y, var & 1)
            vec3 canvas = vec3(0.86f, 0.82f, 0.70f) * (0.9f + 0.1f * hash01(var, 3, seed)), pole(0.4f, 0.3f, 0.2f);
            bool ns = var & 1;
            float h = 1.15f, a = 0.44f, b = 0.4f;   // half length along the ridge, half width
            auto P = [&](float u, float v, float zz) { return ns ? vec3(cx + v, cy + u, zz) : vec3(cx + u, cy + v, zz); };
            vec3 n1 = ns ? vec3(1, 0, 0.8f) : vec3(0, 1, 0.8f), n2 = ns ? vec3(-1, 0, 0.8f) : vec3(0, -1, 0.8f);
            // swapping the axes mirrors the tent, so its faces are given in the other order
            auto Q = [&](vec3 p0, vec3 p1, vec3 p2, vec3 p3, vec3 col, vec3 n) { if (ns) m.quad(p3, p2, p1, p0, col, n); else m.quad(p0, p1, p2, p3, col, n); };
            Q(P(-a, b, z), P(a, b, z), P(a, 0, z + h), P(-a, 0, z + h), canvas, n1);
            Q(P(a, -b, z), P(-a, -b, z), P(-a, 0, z + h), P(a, 0, z + h), canvas * 0.9f, n2);
            vec3 e1 = ns ? vec3(0, -1, 0) : vec3(-1, 0, 0);
            Q(P(-a, -b, z), P(-a, b, z), P(-a, 0, z + h), P(-a, 0, z + h), canvas * 0.8f, e1);   // the ends
            Q(P(a, b, z), P(a, -b, z), P(a, 0, z + h), P(a, 0, z + h), canvas * 0.8f, e1 * -1.0f);
            m.box(P(-a - 0.04f, -0.02f, z) - vec3(0.02f, 0.02f, 0), P(-a - 0.04f, 0.02f, z + h + 0.15f) + vec3(0.02f, 0.02f, 0), pole);
            break;
        }
        case F_GABION: {   // three wicker baskets of earth, side by side
            vec3 wicker(0.50f, 0.38f, 0.22f), earth(0.36f, 0.30f, 0.20f);
            for (int k = -1; k <= 1; k++) {
                float ox = (var & 1) ? 0 : k * 0.32f, oy = (var & 1) ? k * 0.32f : 0;
                m.prism(cx + ox, cy + oy, z, z + 0.95f, 0.16f, 8, wicker * (0.9f + 0.1f * k));
                m.prism(cx + ox, cy + oy, z + 0.95f, z + 1.0f, 0.13f, 8, earth);
            }
            break;
        }
        case F_CANNON: {   // a gun on a wooden carriage, pointing west, east, south or north (var 0..3)
            vec3 wood(0.42f, 0.30f, 0.18f), bronze(0.55f, 0.44f, 0.24f), dark(0.2f, 0.18f, 0.16f);
            int d = var & 3;
            float dx = d == 0 ? -1.0f : d == 1 ? 1.0f : 0.0f, dy = d == 2 ? -1.0f : d == 3 ? 1.0f : 0.0f;
            auto P = [&](float u, float v, float zz) { return vec3(cx + dx * u - dy * v, cy + dy * u + dx * v, zz); };
            auto bx = [&](float u0, float v0, float u1, float v1, float z0, float z1, vec3 col) {
                vec3 a = P(u0, v0, z + z0), b = P(u1, v1, z + z1);
                m.box(vec3(std::min(a.x, b.x), std::min(a.y, b.y), a.z), vec3(std::max(a.x, b.x), std::max(a.y, b.y), b.z), col);
            };
            bx(-0.38f, -0.14f, 0.2f, 0.14f, 0.12f, 0.34f, wood);         // the carriage
            bx(-0.1f, -0.22f, 0.0f, -0.16f, 0.0f, 0.4f, dark);          // the wheels
            bx(-0.1f, 0.16f, 0.0f, 0.22f, 0.0f, 0.4f, dark);
            bx(-0.3f, -0.07f, 0.55f, 0.07f, 0.34f, 0.48f, bronze);      // the barrel
            bx(0.48f, -0.09f, 0.55f, 0.09f, 0.32f, 0.5f, bronze * 0.85f);
            break;
        }
        case F_CROSS: {
            vec3 st(0.72f, 0.7f, 0.64f);
            m.box(vec3(cx - 0.3f, cy - 0.3f, z), vec3(cx + 0.3f, cy + 0.3f, z + 0.2f), st * 0.9f);
            m.box(vec3(cx - 0.06f, cy - 0.06f, z + 0.2f), vec3(cx + 0.06f, cy + 0.06f, z + 1.5f), st);
            m.box(vec3(cx - 0.28f, cy - 0.06f, z + 1.1f), vec3(cx + 0.28f, cy + 0.06f, z + 1.22f), st);
            break;
        }
        case F_CHEST:
            m.box(vec3(cx - 0.3f, cy - 0.2f, z), vec3(cx + 0.3f, cy + 0.2f, z + 0.35f), vec3(0.45f, 0.28f, 0.14f));
            m.box(vec3(cx - 0.31f, cy - 0.21f, z + 0.28f), vec3(cx + 0.31f, cy + 0.21f, z + 0.32f), vec3(0.3f, 0.3f, 0.3f));
            break;
        case F_HAY:
            m.box(vec3(cx - 0.4f, cy - 0.3f, z), vec3(cx + 0.4f, cy + 0.3f, z + 0.45f), vec3(0.85f, 0.75f, 0.4f));
            m.blob(vec3(cx, cy, z + 0.45f), vec3(0.42f, 0.32f, 0.15f), 6, vec3(0.88f, 0.78f, 0.42f), seed, 0.2f);
            break;
        case F_BARREL:
            m.prism(cx, cy, z, z + 0.55f, 0.22f, 8, vec3(0.5f, 0.33f, 0.18f));
            m.prism(cx, cy, z + 0.12f, z + 0.16f, 0.235f, 8, vec3(0.3f, 0.3f, 0.3f), 0, false);
            m.prism(cx, cy, z + 0.4f, z + 0.44f, 0.235f, 8, vec3(0.3f, 0.3f, 0.3f), 0, false);
            break;
        case F_LADDER: {
            static const float hs[4] = {GF_H + 0.35f, TOWNWALL_H + 0.45f, 2.95f, 2.9f};
            int side = var & 3;
            float H = hs[(var >> 2) & 3];
            vec2 d = side == 0 ? vec2(-1, 0) : side == 1 ? vec2(1, 0) : side == 2 ? vec2(0, -1) : vec2(0, 1);
            vec2 pp(-d.y, d.x);
            vec3 wood(0.5f, 0.34f, 0.18f);
            vec2 bot = vec2(cx, cy) - d * 0.12f, top = vec2(cx, cy) + d * 0.44f;
            int nseg = (int)(H / 0.25f);
            m.flags = 2;   // never cut away by the see-through circle round you (it took the middle out of a ladder beside you)
            for (int r = -1; r <= 1; r += 2)
                for (int k = 0; k < nseg; k++) {
                    float t = (k + 0.5f) / nseg;
                    vec2 q = bot + (top - bot) * t + pp * (0.2f * r);
                    float zz = z + H * t;
                    m.box(vec3(q.x - 0.04f, q.y - 0.04f, zz - 0.14f), vec3(q.x + 0.04f, q.y + 0.04f, zz + 0.14f), wood);
                }
            for (float zz = 0.3f; zz < H - 0.1f; zz += 0.35f) {
                float t = zz / H;
                vec2 q = bot + (top - bot) * t;
                vec2 a = q - pp * 0.2f, b = q + pp * 0.2f;
                m.box(vec3(std::min(a.x, b.x) - 0.025f, std::min(a.y, b.y) - 0.025f, z + zz - 0.025f),
                      vec3(std::max(a.x, b.x) + 0.025f, std::max(a.y, b.y) + 0.025f, z + zz + 0.025f), wood * 0.9f);
            }
            m.flags = 0;
            break;
        }
        case F_HATCH: {
            vec3 wood(0.42f, 0.28f, 0.15f);
            m.box(vec3(cx - 0.42f, cy - 0.42f, z), vec3(cx + 0.42f, cy - 0.34f, z + 0.06f), wood);
            m.box(vec3(cx - 0.42f, cy + 0.34f, z), vec3(cx + 0.42f, cy + 0.42f, z + 0.06f), wood);
            m.box(vec3(cx - 0.42f, cy - 0.34f, z), vec3(cx - 0.34f, cy + 0.34f, z + 0.06f), wood);
            m.box(vec3(cx + 0.34f, cy - 0.34f, z), vec3(cx + 0.42f, cy + 0.34f, z + 0.06f), wood);
            m.quad(vec3(cx - 0.34f, cy - 0.34f, z + 0.02f), vec3(cx + 0.34f, cy - 0.34f, z + 0.02f), vec3(cx + 0.34f, cy + 0.34f, z + 0.02f),
                   vec3(cx - 0.34f, cy + 0.34f, z + 0.02f), vec3(0.03f, 0.025f, 0.02f), vec3(0, 0, 1));
            // the tops of the ladder's rails
            int side = var & 3;
            vec2 d = side == 0 ? vec2(-1, 0) : side == 1 ? vec2(1, 0) : side == 2 ? vec2(0, -1) : vec2(0, 1);
            vec2 pp(-d.y, d.x);
            for (int r = -1; r <= 1; r += 2) {
                vec2 q = vec2(cx, cy) + d * 0.28f + pp * (0.2f * r);
                m.box(vec3(q.x - 0.04f, q.y - 0.04f, z - 0.3f), vec3(q.x + 0.04f, q.y + 0.04f, z + 0.55f), vec3(0.5f, 0.34f, 0.18f));
            }
            break;
        }
        case F_STAIRS: {
            vec3 st(0.72f, 0.68f, 0.6f);
            m.prism(cx, cy, z, z + 2.2f, 0.09f, 6, st * 0.9f);
            for (int k = 0; k < 8; k++) {
                float a = k * 0.8f;
                float px = cx + 0.27f * std::cos(a), py = cy + 0.27f * std::sin(a);
                float zz = z + 0.22f * (k + 1);
                m.box(vec3(px - 0.17f, py - 0.17f, zz - 0.12f), vec3(px + 0.17f, py + 0.17f, zz), st * (0.92f + 0.02f * k));
            }
            break;
        }
        case F_SHAFT: {
            vec3 st(0.66f, 0.63f, 0.56f), wood(0.45f, 0.3f, 0.17f);
            m.prism(cx, cy, z, z + 0.55f, 0.48f, 10, st, 0, false);
            m.prism(cx, cy, z - 0.2f, z + 0.5f, 0.38f, 10, vec3(0.02f, 0.02f, 0.02f), 0, true);
            m.box(vec3(cx - 0.5f, cy - 0.05f, z), vec3(cx - 0.42f, cy + 0.05f, z + 1.3f), wood);
            m.box(vec3(cx + 0.42f, cy - 0.05f, z), vec3(cx + 0.5f, cy + 0.05f, z + 1.3f), wood);
            m.prism(cx, cy, z + 1.0f, z + 1.12f, 0.5f, 8, wood * 0.85f);   // the windlass drum
            m.box(vec3(cx - 0.015f, cy - 0.015f, z + 0.3f), vec3(cx + 0.015f, cy + 0.015f, z + 1.05f), vec3(0.75f, 0.68f, 0.5f));
            break;
        }
        case F_BELL: {
            vec3 wood(0.4f, 0.27f, 0.15f), bronze(0.62f, 0.48f, 0.25f);
            m.box(vec3(cx - 0.5f, cy - 0.06f, z), vec3(cx - 0.4f, cy + 0.06f, z + 1.8f), wood);
            m.box(vec3(cx + 0.4f, cy - 0.06f, z), vec3(cx + 0.5f, cy + 0.06f, z + 1.8f), wood);
            m.box(vec3(cx - 0.5f, cy - 0.07f, z + 1.7f), vec3(cx + 0.5f, cy + 0.07f, z + 1.85f), wood);
            m.cone(cx, cy, z + 0.6f, z + 1.6f, 0.42f, 10, bronze);
            m.prism(cx, cy, z + 0.6f, z + 0.68f, 0.45f, 10, bronze * 0.9f);
            m.box(vec3(cx - 0.04f, cy - 0.04f, z + 0.45f), vec3(cx + 0.04f, cy + 0.04f, z + 0.65f), bronze * 0.7f);
            break;
        }
        case F_ANVIL: {
            vec3 iron(0.22f, 0.22f, 0.24f), wood(0.42f, 0.28f, 0.15f);
            m.prism(cx, cy, z, z + 0.45f, 0.22f, 8, wood);
            m.box(vec3(cx - 0.32f, cy - 0.13f, z + 0.45f), vec3(cx + 0.28f, cy + 0.13f, z + 0.62f), iron);
            m.cone(cx + 0.28f, cy, z + 0.52f, z + 0.58f, 0.1f, 4, iron);
            m.box(vec3(cx + 0.28f, cy - 0.06f, z + 0.5f), vec3(cx + 0.48f, cy + 0.06f, z + 0.6f), iron);
            break;
        }
        case F_FORGE: {
            vec3 brick(0.55f, 0.3f, 0.24f);
            m.box(vec3(cx - 0.45f, cy - 0.45f, z), vec3(cx + 0.45f, cy + 0.45f, z + 0.7f), brick);
            m.box(vec3(cx - 0.2f, cy - 0.2f, z + 0.7f), vec3(cx + 0.2f, cy + 0.2f, z + 2.4f), brick * 0.85f);   // chimney hood
            m.flags = 1;
            m.blob(vec3(cx, cy, z + 0.75f), vec3(0.25f, 0.25f, 0.1f), 6, vec3(1.0f, 0.45f, 0.1f), seed, 0.4f);
            m.flags = 0;
            break;
        }
        case F_COUNTER: {
            vec3 wood(0.5f, 0.34f, 0.2f);
            m.box(vec3(cx - 0.48f, cy - 0.3f, z), vec3(cx + 0.48f, cy + 0.3f, z + 0.75f), wood);
            m.box(vec3(cx - 0.5f, cy - 0.32f, z + 0.75f), vec3(cx + 0.5f, cy + 0.32f, z + 0.8f), wood * 0.85f);
            break;
        }
        case F_OVEN: {
            vec3 st(0.62f, 0.55f, 0.48f);
            m.box(vec3(cx - 0.46f, cy - 0.46f, z), vec3(cx + 0.46f, cy + 0.46f, z + 0.5f), st);
            m.blob(vec3(cx, cy, z + 0.5f), vec3(0.45f, 0.45f, 0.45f), 8, st * 1.05f, seed, 0.1f);
            m.flags = 1;
            m.box(vec3(cx - 0.15f, cy - 0.47f, z + 0.25f), vec3(cx + 0.15f, cy - 0.44f, z + 0.5f), vec3(1.0f, 0.5f, 0.15f));
            m.flags = 0;
            break;
        }
        case F_STAGE: {
            vec3 wood(0.5f, 0.36f, 0.22f);
            m.box(vec3(cx - 0.5f, cy - 0.5f, z), vec3(cx + 0.5f, cy + 0.5f, z + 0.12f), wood * (0.95f + 0.1f * (var & 1)));
            break;
        }
        case F_OBELISK: {
            vec3 st(0.78f, 0.74f, 0.66f), gr(0.72f, 0.58f, 0.45f);
            m.box(vec3(cx - 0.55f, cy - 0.55f, z), vec3(cx + 0.55f, cy + 0.55f, z + 0.9f), st);
            m.box(vec3(cx - 0.42f, cy - 0.42f, z + 0.9f), vec3(cx + 0.42f, cy + 0.42f, z + 1.2f), st * 0.92f);
            m.cone(cx, cy, z + 1.2f, z + 8.5f, 0.42f, 4, gr, PI_F / 4);   // the granite shaft, tapering to its point
            m.box(vec3(cx - 0.05f, cy - 0.05f, z + 7.6f), vec3(cx + 0.05f, cy + 0.05f, z + 9.0f), vec3(0.75f, 0.62f, 0.3f));
            m.box(vec3(cx - 0.22f, cy - 0.04f, z + 8.55f), vec3(cx + 0.22f, cy + 0.04f, z + 8.63f), vec3(0.75f, 0.62f, 0.3f));
            break;
        }
        case F_COLUMN: {
            vec3 st(0.86f, 0.83f, 0.76f), br(0.36f, 0.40f, 0.34f);
            m.box(vec3(cx - 0.6f, cy - 0.6f, z), vec3(cx + 0.6f, cy + 0.6f, z + 1.4f), st * 0.92f);
            m.prism(cx, cy, z + 1.4f, z + 9.0f, 0.42f, 12, st);
            for (float zz = z + 2.0f; zz < z + 9.0f; zz += 0.9f) m.prism(cx, cy, zz, zz + 0.12f, 0.45f, 12, st * 0.88f, 0, false);   // the spiral of reliefs
            m.box(vec3(cx - 0.5f, cy - 0.5f, z + 9.0f), vec3(cx + 0.5f, cy + 0.5f, z + 9.3f), st);
            m.prism(cx, cy, z + 9.3f, z + 10.4f, 0.18f, 6, br);     // the saint on top
            m.blob(vec3(cx, cy, z + 10.55f), vec3(0.13f, 0.13f, 0.15f), 5, br, seed, 0.1f);
            break;
        }
        case F_MOORING: {
            // a post driven into the bed of the canal; Venetian ones are striped
            vec3 a(0.36f, 0.25f, 0.16f), b = (seed & 1) ? vec3(0.85f, 0.82f, 0.75f) : vec3(0.22f, 0.32f, 0.55f);
            for (int k = 0; k < 4; k++) m.prism(cx + 0.35f, cy + 0.35f, z - 0.4f + k * 0.55f, z + 0.15f + k * 0.55f, 0.09f, 6, (k & 1) ? b : a);
            if (var >= 2) {   // a ferry waits here (city.cc ferries): var 2,3 a gondola, 4,5 a rowing boat; odd: lying east-west
                static ItemKind gondola, rowing;
                if (gondola.shape != "gondola") {
                    gondola.shape = "gondola"; gondola.color = vec3(0.09f, 0.085f, 0.1f); gondola.color2 = vec3(0.6f, 0.17f, 0.14f);
                    rowing.shape = "boat"; rowing.color = vec3(0.43f, 0.29f, 0.17f); rowing.color2 = vec3(0.6f, 0.48f, 0.32f);
                }
                drawBoat(m, var < 4 ? gondola : rowing, vec3(cx, cy, z + 0.02f), (var & 1) ? 1.5708f : 0.0f);
            }
            break;
        }
        case F_STATUE: {
            vec3 st(0.75f, 0.73f, 0.66f), br(0.26f, 0.33f, 0.28f);
            if (var == 1) {
                // a pedestal ('y'): the statue standing on it is a work of art (items.cc: giant, group)
                m.box(vec3(cx - 0.45f, cy - 0.45f, z), vec3(cx + 0.45f, cy + 0.45f, z + 0.14f), st * 0.9f);
                m.box(vec3(cx - 0.36f, cy - 0.36f, z + 0.14f), vec3(cx + 0.36f, cy + 0.36f, z + 1.1f), st);
                m.box(vec3(cx - 0.42f, cy - 0.42f, z + 1.1f), vec3(cx + 0.42f, cy + 0.42f, z + 1.2f), st * 0.92f);
                break;
            }
            // an equestrian bronze on a stone pedestal
            m.box(vec3(cx - 0.45f, cy - 0.7f, z), vec3(cx + 0.45f, cy + 0.7f, z + 1.4f), st);
            m.box(vec3(cx - 0.5f, cy - 0.75f, z + 1.35f), vec3(cx + 0.5f, cy + 0.75f, z + 1.45f), st * 0.92f);
            float b = z + 1.45f;
            m.box(vec3(cx - 0.16f, cy - 0.5f, b + 0.45f), vec3(cx + 0.16f, cy + 0.42f, b + 0.8f), br);  // horse
            m.box(vec3(cx - 0.1f, cy + 0.36f, b + 0.7f), vec3(cx + 0.1f, cy + 0.6f, b + 1.1f), br);    // neck, head
            for (int q = 0; q < 4; q++) {
                float lx = cx + ((q & 1) ? 0.1f : -0.1f), ly = cy + ((q & 2) ? 0.32f : -0.4f);
                m.box(vec3(lx - 0.05f, ly - 0.05f, b), vec3(lx + 0.05f, ly + 0.05f, b + 0.48f), br);
            }
            m.box(vec3(cx - 0.12f, cy - 0.15f, b + 0.8f), vec3(cx + 0.12f, cy + 0.08f, b + 1.25f), br);  // rider
            m.box(vec3(cx - 0.07f, cy - 0.1f, b + 1.25f), vec3(cx + 0.07f, cy + 0.04f, b + 1.42f), br);
            break;
        }
        default:
            break;
    }
}

// ------------------------------------------------------------------ chunks

static float wallTop(World &w, int X, int Y) {
    Tile &t = w.tile(X, Y);
    float g = t.bld ? w.buildings[t.bld - 1].ground : w.cornerH(X, Y);
    switch (t.wall) {
        case W_WALL: case W_WINDOW:
            if (t.bld && w.buildings[t.bld - 1].ruined > 0) return g + std::min(GF_H, ruinTop(w.buildings[t.bld - 1], X, Y));
            return g + GF_H;
        case W_CITYWALL: return g + TOWNWALL_H;
        case W_HEDGE: return w.cornerH(X, Y) + 1.0f;
        case W_EARTHWORK: return w.cornerH(X, Y) + 1.05f;
        default: return -1e9f;
    }
}

static vec3 wallColorOf(World &w, const Tile &t) {
    if (t.wall == W_CITYWALL) return vec3(0.76f, 0.72f, 0.62f);
    if (t.wall == W_HEDGE) return vec3(0.24f, 0.40f, 0.18f);
    if (t.wall == W_EARTHWORK) return vec3(0.46f, 0.40f, 0.26f) * (0.92f + 0.1f * (t.var / 255.0f));
    if (t.bld) return w.buildings[t.bld - 1].wallColor * 0.93f;
    return vec3(0.75f, 0.7f, 0.6f);
}

static bool isWet(int terrain) { return terrain == T_WATER || terrain == T_DEEP || terrain == T_BRIDGE; }

// ------------------------------------------------------------------ signs

// a sign over the street: an iron bracket from the wall, a framed board on two chains, the
// emblem of the trade on both faces; an inn's also has a lantern with a lit candle
static void drawSign(MeshBuf &m, const Sign &sg) {
    vec3 o((float)sg.ox, (float)sg.oy, 0), p((float)-sg.oy, (float)sg.ox, 0);
    vec3 F(sg.x + 0.5f + 0.5f * sg.ox, sg.y + 0.5f + 0.5f * sg.oy, 0);   // the wall's face
    // a box in the wall's axes: u out from the wall, w along it, v up from the bracket
    auto B = [&](float u0, float u1, float w0, float w1, float v0, float v1, vec3 col) {
        vec3 a = F + o * u0 + p * w0, b = F + o * u1 + p * w1;
        m.box(vec3(std::min(a.x, b.x), std::min(a.y, b.y), sg.z + v0), vec3(std::max(a.x, b.x), std::max(a.y, b.y), sg.z + v1), col);
    };
    // on a wall facing north or south the board hangs parallel to it, from a crossbar at the end of
    // the bracket, so that it is seen face on from the camera's side; else across the bracket
    bool para = sg.oy != 0;
    vec3 A = para ? p : o, N = para ? o : p, O = para ? F + o * 0.7f : F + o * 0.14f;
    const float A0 = para ? -0.46f : 0.0f, A1 = para ? 0.46f : 0.92f, V0 = -0.84f, V1 = -0.14f;   // the board
    auto Bb = [&](float a0, float a1, float n0, float n1, float v0, float v1, vec3 col) {
        vec3 a = O + A * a0 + N * n0, b = O + A * a1 + N * n1;
        m.box(vec3(std::min(a.x, b.x), std::min(a.y, b.y), sg.z + v0), vec3(std::max(a.x, b.x), std::max(a.y, b.y), sg.z + v1), col);
    };
    // the emblem: (a, b) from 0 to 1 across and up the board, on both faces
    auto E = [&](float a0, float a1, float b0, float b1, vec3 col) {
        for (float sd : {1.0f, -1.0f})
            Bb(A0 + (A1 - A0) * a0, A0 + (A1 - A0) * a1, sd * 0.035f, sd * 0.055f, V0 + (V1 - V0) * b0, V0 + (V1 - V0) * b1, col);
    };
    vec3 iron(0.14f, 0.13f, 0.12f);
    float reach = para ? 0.74f : 1.2f;
    B(0, reach, -0.03f, 0.03f, -0.035f, 0.035f, iron);              // the bracket
    B(0, 0.05f, -0.05f, 0.05f, -0.4f, 0.05f, iron);                 // its plate on the wall
    B(0.05f, 0.4f, -0.015f, 0.015f, -0.36f, -0.3f, iron);           // and its stay
    if (para) Bb(A0 - 0.04f, A1 + 0.04f, -0.025f, 0.025f, -0.03f, 0.03f, iron);   // the crossbar
    Bb(A0 + 0.12f, A0 + 0.15f, -0.012f, 0.012f, V1, 0, iron);       // the chains
    Bb(A1 - 0.15f, A1 - 0.12f, -0.012f, 0.012f, V1, 0, iron);
    const std::string &e = sg.emblem;
    vec3 board = e == "inn" ? vec3(0.17f, 0.34f, 0.2f) : e == "anvil" ? vec3(0.46f, 0.12f, 0.1f) : e == "pretzel" ? vec3(0.15f, 0.24f, 0.45f)
               : e == "scales" ? vec3(0.13f, 0.19f, 0.36f) : e == "book" ? vec3(0.6f, 0.48f, 0.24f) : e == "goblet" ? vec3(0.1f, 0.28f, 0.27f)
               : e == "palette" ? vec3(0.26f, 0.2f, 0.18f) : e == "ale" ? vec3(0.42f, 0.27f, 0.1f) : e == "horseshoe" ? vec3(0.2f, 0.3f, 0.17f)
               : e == "dividers" ? vec3(0.14f, 0.16f, 0.3f) : vec3(0.4f, 0.28f, 0.16f);
    Bb(A0 - 0.035f, A1 + 0.035f, -0.025f, 0.025f, V0 - 0.035f, V1 + 0.035f, vec3(0.86f, 0.74f, 0.42f));   // a gilt frame
    Bb(A0, A1, -0.035f, 0.035f, V0, V1, board);
    if (e == "inn" || e == "ale") {   // a tankard of ale, its froth
        vec3 pewter(0.66f, 0.66f, 0.64f);
        E(0.3f, 0.58f, 0.15f, 0.74f, pewter);
        E(0.28f, 0.6f, 0.74f, 0.88f, vec3(0.96f, 0.94f, 0.86f));
        E(0.58f, 0.72f, 0.28f, 0.38f, pewter); E(0.66f, 0.72f, 0.28f, 0.66f, pewter); E(0.58f, 0.72f, 0.56f, 0.66f, pewter);
    }
    if (e == "inn") {
        // the lantern at the bracket's end, a candle burning in it (it glows at night)
        float c = para ? 0.34f : 1.12f;   // the lantern's middle, out from the wall
        B(c - 0.08f, c + 0.08f, -0.08f, 0.08f, -0.12f, -0.07f, iron);
        B(c - 0.08f, c + 0.08f, -0.08f, 0.08f, -0.46f, -0.42f, iron);
        for (float a : {c - 0.08f, c + 0.065f}) for (float b : {-0.08f, 0.065f}) B(a, a + 0.015f, b, b + 0.015f, -0.42f, -0.12f, iron);
        B(c - 0.025f, c + 0.025f, -0.025f, 0.025f, -0.42f, -0.28f, vec3(0.95f, 0.92f, 0.82f));
        m.flags = 1;
        B(c - 0.018f, c + 0.018f, -0.018f, 0.018f, -0.28f, -0.19f, vec3(1.0f, 0.78f, 0.32f));
        m.flags = 0;
        B(c - 0.006f, c + 0.006f, -0.006f, 0.006f, -0.07f, 0.0f, iron);
    } else if (e == "horseshoe") {
        vec3 k(0.16f, 0.16f, 0.17f);
        E(0.3f, 0.7f, 0.18f, 0.3f, k); E(0.3f, 0.41f, 0.18f, 0.78f, k); E(0.59f, 0.7f, 0.18f, 0.78f, k);
    } else if (e == "dividers") {
        vec3 br(0.86f, 0.69f, 0.3f);
        E(0.3f, 0.36f, 0.14f, 0.36f, br); E(0.36f, 0.42f, 0.34f, 0.56f, br); E(0.42f, 0.48f, 0.54f, 0.76f, br);
        E(0.64f, 0.7f, 0.14f, 0.36f, br); E(0.58f, 0.64f, 0.34f, 0.56f, br); E(0.52f, 0.58f, 0.54f, 0.76f, br);
        E(0.43f, 0.57f, 0.72f, 0.84f, br); E(0.38f, 0.62f, 0.42f, 0.46f, br);
    } else if (e == "anvil") {
        vec3 k(0.1f, 0.1f, 0.11f);
        E(0.4f, 0.6f, 0.14f, 0.3f, k); E(0.44f, 0.56f, 0.3f, 0.46f, k); E(0.2f, 0.72f, 0.46f, 0.64f, k); E(0.72f, 0.86f, 0.5f, 0.58f, k);
    } else if (e == "pretzel") {
        vec3 d(0.76f, 0.48f, 0.2f);
        E(0.25f, 0.75f, 0.18f, 0.28f, d); E(0.25f, 0.35f, 0.18f, 0.72f, d); E(0.65f, 0.75f, 0.18f, 0.72f, d);
        E(0.25f, 0.75f, 0.62f, 0.72f, d); E(0.45f, 0.55f, 0.28f, 0.62f, d);
    } else if (e == "scales") {
        vec3 br(0.86f, 0.69f, 0.3f);
        E(0.48f, 0.52f, 0.14f, 0.82f, br); E(0.16f, 0.84f, 0.74f, 0.8f, br);
        E(0.19f, 0.21f, 0.46f, 0.74f, br); E(0.79f, 0.81f, 0.46f, 0.74f, br);
        E(0.1f, 0.3f, 0.4f, 0.47f, br); E(0.7f, 0.9f, 0.4f, 0.47f, br); E(0.36f, 0.64f, 0.12f, 0.18f, br);
    } else if (e == "book") {
        vec3 pg(0.96f, 0.94f, 0.86f), cv(0.55f, 0.1f, 0.1f), ink(0.3f, 0.28f, 0.26f);
        E(0.16f, 0.84f, 0.24f, 0.32f, cv); E(0.47f, 0.53f, 0.24f, 0.8f, cv);
        E(0.18f, 0.47f, 0.3f, 0.78f, pg); E(0.53f, 0.82f, 0.3f, 0.78f, pg);
        for (float b : {0.42f, 0.54f, 0.66f}) { E(0.23f, 0.42f, b, b + 0.03f, ink); E(0.58f, 0.77f, b, b + 0.03f, ink); }
    } else if (e == "goblet") {
        vec3 gl(0.74f, 0.9f, 0.84f);
        E(0.3f, 0.7f, 0.5f, 0.82f, gl); E(0.47f, 0.53f, 0.24f, 0.5f, gl); E(0.34f, 0.66f, 0.16f, 0.24f, gl);
    } else if (e == "palette") {
        E(0.18f, 0.82f, 0.22f, 0.78f, vec3(0.8f, 0.64f, 0.42f));
        E(0.28f, 0.37f, 0.58f, 0.68f, vec3(0.8f, 0.15f, 0.1f)); E(0.44f, 0.53f, 0.62f, 0.72f, vec3(0.15f, 0.3f, 0.75f));
        E(0.6f, 0.69f, 0.55f, 0.65f, vec3(0.92f, 0.8f, 0.2f)); E(0.62f, 0.71f, 0.3f, 0.4f, vec3(0.97f, 0.97f, 0.95f));
        E(0.28f, 0.37f, 0.3f, 0.4f, board);
    }
}

void buildChunkMesh(World &w, Chunk &c, MeshBuf &S, MeshBuf &Wt) {
    const int X0 = c.cx * CH, Y0 = c.cy * CH;
    const int dx4[4] = {1, -1, 0, 0}, dy4[4] = {0, 0, 1, -1};
    for (int ly = 0; ly < CH; ly++)
        for (int lx = 0; lx < CH; lx++) {
            Tile &t = c.t(lx, ly);
            int X = X0 + lx, Y = Y0 + ly;
            float h00 = c.h(lx, ly), h10 = c.h(lx + 1, ly), h01 = c.h(lx, ly + 1), h11 = c.h(lx + 1, ly + 1);
            vec3 p00(X, Y, h00), p10(X + 1, Y, h10), p01(X, Y + 1, h01), p11(X + 1, Y + 1, h11);

            if (isWet(t.terrain)) {
                vec3 col = terrainColor(t.terrain == T_BRIDGE ? T_WATER : t.terrain, t.var, X, Y);
                Wt.flags = 3;
                Wt.quad(vec3(X, Y, 0), vec3(X + 1, Y, 0), vec3(X + 1, Y + 1, 0), vec3(X, Y + 1, 0), col, vec3(0, 0, 1));
                if (t.terrain == T_BRIDGE) {
                    float zd = std::max({h00, h10, h01, h11}) + 0.12f;
                    vec3 deck = terrainColor(T_BRIDGE, t.var, X, Y);
                    S.flags = 2;
                    S.quad(vec3(X, Y, zd), vec3(X + 1, Y, zd), vec3(X + 1, Y + 1, zd), vec3(X, Y + 1, zd), deck, vec3(0, 0, 1));
                    S.flags = 0;
                    bool city = zd > 0.9f;
                    vec3 side = city ? vec3(0.66f, 0.62f, 0.54f) : vec3(0.45f, 0.34f, 0.22f);
                    for (int k = 0; k < 4; k++) {
                        Tile &n = w.tile(X + dx4[k], Y + dy4[k]);
                        if (n.terrain == T_BRIDGE) continue;
                        bool water = isWet(n.terrain);
                        if (!water) continue;
                        // side face of the deck and a parapet
                        float ax = k == 0 ? X + 1 : X, bx = k == 1 ? X : X + 1;
                        float ay = k == 2 ? Y + 1 : Y, by = k == 3 ? Y : Y + 1;
                        if (k <= 1) { ax = bx = (k == 0 ? X + 1 : X); ay = Y; by = Y + 1; }
                        else { ay = by = (k == 2 ? Y + 1 : Y); ax = X; bx = X + 1; }
                        vec3 out(dx4[k], dy4[k], 0);
                        S.quad(vec3(ax, ay, zd - (city ? 1.3f : 0.35f)), vec3(bx, by, zd - (city ? 1.3f : 0.35f)), vec3(bx, by, zd), vec3(ax, ay, zd), side, out);
                        float th = 0.2f;
                        vec3 lo(std::min(ax, bx), std::min(ay, by), zd), hi(std::max(ax, bx), std::max(ay, by), zd + (city ? 0.6f : 0.4f));
                        if (k == 0) lo.x -= th; if (k == 1) hi.x += th; if (k == 2) lo.y -= th; if (k == 3) hi.y += th;
                        S.box(lo, hi, city ? vec3(0.72f, 0.68f, 0.6f) : vec3(0.42f, 0.3f, 0.18f));
                    }
                    if (city && ((X + Y) % 3 == 0)) {
                        S.box(vec3(X + 0.15f, Y + 0.15f, -0.6f), vec3(X + 0.85f, Y + 0.85f, zd - 1.2f), vec3(0.6f, 0.57f, 0.5f));
                    }
                }
                if (t.feature != F_NONE) feature(S, t.feature, t.var, X + 0.5f, Y + 0.5f, 0.0f, hash2i(X, Y, 1));
                continue;
            }

            // land surface
            vec3 col = terrainColor(t.terrain, t.var, X, Y);
            S.flags = 2;
            bool subdiv = t.terrain == T_STREET || t.terrain == T_SQUARE || t.terrain == T_FIELD || t.terrain == T_FLOOR_WOOD ||
                          t.terrain == T_FLOOR_STONE;
            if (!subdiv) {
                if (std::fabs(h00 - h11) < std::fabs(h10 - h01)) {
                    S.tri(p00, p10, p11, col, vec3(0, 0, 1));
                    S.tri(p00, p11, p01, col * 0.985f, vec3(0, 0, 1));
                } else {
                    S.tri(p00, p10, p01, col, vec3(0, 0, 1));
                    S.tri(p10, p11, p01, col * 0.985f, vec3(0, 0, 1));
                }
            } else {
                // a few strips or cobbles, for texture
                int n = (t.terrain == T_FIELD) ? 4 : 2;
                bool alongY = (t.terrain == T_FIELD) ? (hash2i(X / 7, Y / 5, 83) & 16) : false;
                auto H = [&](float u, float v) {
                    return (h00 * (1 - u) + h10 * u) * (1 - v) + (h01 * (1 - u) + h11 * u) * v;
                };
                for (int i = 0; i < n; i++)
                    for (int j = 0; j < (t.terrain == T_FIELD ? 1 : n); j++) {
                        float u0, u1, v0, v1;
                        if (t.terrain == T_FIELD) {
                            if (alongY) { u0 = (float)i / n; u1 = (float)(i + 1) / n; v0 = 0; v1 = 1; }
                            else { v0 = (float)i / n; v1 = (float)(i + 1) / n; u0 = 0; u1 = 1; }
                        } else {
                            u0 = (float)i / n; u1 = (float)(i + 1) / n; v0 = (float)j / n; v1 = (float)(j + 1) / n;
                        }
                        float shade;
                        if (t.terrain == T_FIELD) shade = (i & 1) ? 0.92f : 1.0f;
                        else if (t.terrain == T_FLOOR_WOOD) shade = 0.93f + 0.1f * hash01(X * 2 + (alongY ? i : j), Y, 13);
                        else shade = 0.92f + 0.12f * hash01(X * 2 + i, Y * 2 + j, 12);
                        vec3 cc = col * shade;
                        vec3 a(X + u0, Y + v0, H(u0, v0)), b(X + u1, Y + v0, H(u1, v0)), d(X + u1, Y + v1, H(u1, v1)), e(X + u0, Y + v1, H(u0, v1));
                        S.quad(a, b, d, e, cc, vec3(0, 0, 1));
                    }
            }
            // banks and quays where land meets water
            for (int k = 0; k < 4; k++) {
                int nx = X + dx4[k], ny = Y + dy4[k];
                if (!w.inWorld(nx, ny)) continue;
                Tile &n = w.tile(nx, ny);
                if (!isWet(n.terrain)) continue;
                vec3 a, b;
                if (k == 0) { a = p10; b = p11; } else if (k == 1) { a = p00; b = p01; }
                else if (k == 2) { a = p01; b = p11; } else { a = p00; b = p10; }
                bool quay = std::max(a.z, b.z) > 0.7f;
                vec3 bank = quay ? vec3(0.64f, 0.6f, 0.52f) : vec3(0.42f, 0.35f, 0.25f);
                const Cliff *cl = nullptr;
                bool cliff = std::min(a.z, b.z) > 1.5f && w.cliffAt(X + 0.5f, Y + 0.5f, &cl) > 0;
                if (cliff) bank = cl->color * (0.94f + 0.06f * hash01(X, Y, 96));
                vec3 out(dx4[k], dy4[k], 0);
                S.quad(vec3(a.x, a.y, -0.7f), vec3(b.x, b.y, -0.7f), b, a, bank, out);
                if (cliff) {
                    // bands of flint in the chalk
                    float lo = std::min(a.z, b.z);
                    for (float z = 0.6f + 0.25f * hash01(X / 5, Y / 5, 97); z < lo - 0.2f; z += 1.1f) {
                        vec3 o = out * 0.01f;
                        S.quad(vec3(a.x, a.y, z) + o, vec3(b.x, b.y, z) + o, vec3(b.x, b.y, z + 0.09f) + o, vec3(a.x, a.y, z + 0.09f) + o,
                               bank * 0.62f, out);
                    }
                    // a little rubble at the foot
                    if (hash01(X, Y, 98) < 0.35f) {
                        vec3 mid = (a + b) * 0.5f + out * 0.3f;
                        S.blob(vec3(mid.x, mid.y, -0.1f), vec3(0.35f, 0.3f, 0.22f), 5, bank * 0.9f, hash2i(X, Y, 99), 0.4f);
                    }
                }
            }
            S.flags = 0;

            // walls
            if (t.wall != W_NONE && t.wall != W_TOWER && t.wall != W_GATE) {
                float g = t.bld ? w.buildings[t.bld - 1].ground : std::max({h00, h10, h01, h11});
                float zb = std::min({h00, h10, h01, h11}) - 0.05f;
                vec3 wc = wallColorOf(w, t);
                if (t.wall == W_DOOR) {
                    // lintel, jambs and an open door leaf
                    bool wallEW = w.tile(X - 1, Y).wall != W_NONE || w.tile(X + 1, Y).wall != W_NONE;
                    float zt = g + GF_H;
                    S.box(vec3(X, Y, g + DOOR_H), vec3(X + 1, Y + 1, zt), wc);
                    vec3 wood(0.36f, 0.22f, 0.12f), frame = wc * 0.72f;
                    if (wallEW) {
                        S.box(vec3(X, Y, zb), vec3(X + 0.12f, Y + 1, g + DOOR_H), wc);
                        S.box(vec3(X + 0.88f, Y, zb), vec3(X + 1, Y + 1, g + DOOR_H), wc);
                        // a dark frame around the doorway, so doors are easy to see
                        S.box(vec3(X + 0.06f, Y - 0.04f, g + DOOR_H - 0.12f), vec3(X + 0.94f, Y + 1.04f, g + DOOR_H + 0.1f), frame);
                        bool inN = w.tile(X, Y + 1).bld == t.bld && w.tile(X, Y + 1).wall == W_NONE;
                        float y0 = inN ? Y + 0.5f : Y - 0.2f, y1 = inN ? Y + 1.2f : Y + 0.5f;
                        S.box(vec3(X + 0.12f, y0, g), vec3(X + 0.18f, y1, g + DOOR_H - 0.03f), wood);
                    } else {
                        S.box(vec3(X, Y, zb), vec3(X + 1, Y + 0.12f, g + DOOR_H), wc);
                        S.box(vec3(X, Y + 0.88f, zb), vec3(X + 1, Y + 1, g + DOOR_H), wc);
                        S.box(vec3(X - 0.04f, Y + 0.06f, g + DOOR_H - 0.12f), vec3(X + 1.04f, Y + 0.94f, g + DOOR_H + 0.1f), frame);
                        bool inE = w.tile(X + 1, Y).bld == t.bld && w.tile(X + 1, Y).wall == W_NONE;
                        float x0 = inE ? X + 0.5f : X - 0.2f, x1 = inE ? X + 1.2f : X + 0.5f;
                        S.box(vec3(x0, Y + 0.12f, g), vec3(x1, Y + 0.18f, g + DOOR_H - 0.03f), wood);
                    }
                } else {
                    float zt = wallTop(w, X, Y);
                    // top
                    S.quad(vec3(X, Y, zt), vec3(X + 1, Y, zt), vec3(X + 1, Y + 1, zt), vec3(X, Y + 1, zt), wc * 0.82f, vec3(0, 0, 1));
                    for (int k = 0; k < 4; k++) {
                        int nx = X + dx4[k], ny = Y + dy4[k];
                        float nt = wallTop(w, nx, ny);
                        if (nt >= zt - 1e-3f) continue;
                        float z0 = std::max(zb, nt);
                        vec3 a, b;
                        if (k == 0) { a = vec3(X + 1, Y, 0); b = vec3(X + 1, Y + 1, 0); }
                        else if (k == 1) { a = vec3(X, Y, 0); b = vec3(X, Y + 1, 0); }
                        else if (k == 2) { a = vec3(X, Y + 1, 0); b = vec3(X + 1, Y + 1, 0); }
                        else { a = vec3(X, Y, 0); b = vec3(X + 1, Y, 0); }
                        vec3 out(dx4[k], dy4[k], 0);
                        S.quad(vec3(a.x, a.y, z0), vec3(b.x, b.y, z0), vec3(b.x, b.y, zt), vec3(a.x, a.y, zt), wc, out);
                        if (t.wall == W_WINDOW && nt < g) {
                            vec3 o = out * 0.012f;
                            vec3 dir = b - a;
                            vec3 wa = a + dir * 0.3f + o, wb = a + dir * 0.7f + o;
                            S.flags = 4;
                            S.quad(vec3(wa.x, wa.y, g + 0.85f), vec3(wb.x, wb.y, g + 0.85f), vec3(wb.x, wb.y, g + 1.75f),
                                   vec3(wa.x, wa.y, g + 1.75f), vec3(0.16f, 0.2f, 0.28f), out);
                            S.flags = 0;
                            vec3 sa = a + dir * 0.25f + out * 0.03f, sb = a + dir * 0.75f + out * 0.03f;
                            S.quad(vec3(sa.x, sa.y, g + 0.76f), vec3(sb.x, sb.y, g + 0.76f), vec3(sb.x, sb.y, g + 0.85f),
                                   vec3(sa.x, sa.y, g + 0.85f), vec3(0.9f, 0.88f, 0.82f), out);
                        }
                    }
                    if (t.wall == W_CITYWALL) {
                        for (int i = 0; i < 2; i++)
                            for (int j = 0; j < 2; j++)
                                if (((X * 2 + i) + (Y * 2 + j)) % 2 == 0)
                                    S.box(vec3(X + i * 0.5f + 0.06f, Y + j * 0.5f + 0.06f, zt), vec3(X + i * 0.5f + 0.44f, Y + j * 0.5f + 0.44f, zt + 0.5f), wc);
                    }
                }
            }

            // features
            if (t.feature != F_NONE) {
                float z = std::min({h00, h10, h01, h11});
                if (t.bld) z = w.buildings[t.bld - 1].ground;
                feature(S, t.feature, t.var, X + 0.5f, Y + 0.5f, z, hash2i(X, Y, 1));
            }
        }
    for (const Sign &sg : w.signs)
        if (sg.x >= X0 && sg.x < X0 + CH && sg.y >= Y0 && sg.y < Y0 + CH) drawSign(S, sg);
}

// ------------------------------------------------------------------ floors

void buildLevelMesh(World &w, Level &L, MeshBuf &S) {
    const int dx4[4] = {1, -1, 0, 0}, dy4[4] = {0, 0, 1, -1};
    auto get = [&](int X, int Y) -> Tile * {
        if (!L.inside(X, Y)) return nullptr;
        Tile &t = L.at(X, Y);
        return t.terrain == T_VOID ? nullptr : &t;
    };
    auto isWall = [&](const Tile *t) { return t && t->wall != W_NONE && t->wall != W_DOOR; };
    float f = L.floor;
    for (int Y = L.y0; Y < L.y0 + L.h; Y++)
        for (int X = L.x0; X < L.x0 + L.w; X++) {
            Tile *t = get(X, Y);
            if (!t) continue;
            // the floor, with the edge of its boards
            bool platform = !L.drawFloor && (t->feature == F_LADDER || t->feature == F_HATCH);
            bool underWall = L.drawWalls && t->wall != W_NONE && t->wall != W_DOOR;
            if ((L.drawFloor || platform) && !underWall) {   // under a wall, the wall stands on the floor below
                vec3 col = terrainColor(t->terrain, t->var, X, Y);
                if (L.underground && t->terrain == T_YARD) col = vec3(0.42f, 0.38f, 0.32f) * (0.92f + 0.12f * hash01(X, Y, 63));
                S.flags = 2;
                S.quad(vec3(X, Y, f), vec3(X + 1, Y, f), vec3(X + 1, Y + 1, f), vec3(X, Y + 1, f), col, vec3(0, 0, 1));
                if (!L.underground)
                    for (int k = 0; k < 4; k++) {
                        if (get(X + dx4[k], Y + dy4[k])) continue;
                        vec3 a, b;
                        if (k == 0) { a = vec3(X + 1, Y, 0); b = vec3(X + 1, Y + 1, 0); }
                        else if (k == 1) { a = vec3(X, Y, 0); b = vec3(X, Y + 1, 0); }
                        else if (k == 2) { a = vec3(X, Y + 1, 0); b = vec3(X + 1, Y + 1, 0); }
                        else { a = vec3(X, Y, 0); b = vec3(X + 1, Y, 0); }
                        S.quad(vec3(a.x, a.y, f - 0.3f), vec3(b.x, b.y, f - 0.3f), vec3(b.x, b.y, f), vec3(a.x, a.y, f), col * 0.7f,
                               vec3((float)dx4[k], (float)dy4[k], 0));
                    }
                S.flags = 0;
            }
            // walls
            if (L.drawWalls && t->wall != W_NONE) {
                vec3 wc = L.wallColor * (L.underground ? (0.85f + 0.2f * hash01(X, Y, 64)) : 0.95f);
                float top = f + L.wallH;
                if (t->wall == W_DOOR) {
                    bool wallEW = isWall(get(X - 1, Y)) || isWall(get(X + 1, Y));
                    S.box(vec3(X, Y, f + DOOR_H), vec3(X + 1, Y + 1, top), wc);
                    if (wallEW) {
                        S.box(vec3(X, Y, f), vec3(X + 0.12f, Y + 1, f + DOOR_H), wc);
                        S.box(vec3(X + 0.88f, Y, f), vec3(X + 1, Y + 1, f + DOOR_H), wc);
                    } else {
                        S.box(vec3(X, Y, f), vec3(X + 1, Y + 0.12f, f + DOOR_H), wc);
                        S.box(vec3(X, Y + 0.88f, f), vec3(X + 1, Y + 1, f + DOOR_H), wc);
                    }
                } else {
                    vec3 topc = L.underground ? wc * 0.08f : wc * 0.82f;
                    S.quad(vec3(X, Y, top), vec3(X + 1, Y, top), vec3(X + 1, Y + 1, top), vec3(X, Y + 1, top), topc, vec3(0, 0, 1));
                    for (int k = 0; k < 4; k++) {
                        Tile *n = get(X + dx4[k], Y + dy4[k]);
                        if (isWall(n)) continue;
                        bool rock = L.underground && !n;   // the outside of a cellar: dark earth
                        if (rock && L.w > 60) continue;    // solid rock beyond the tunnels
                        vec3 a, b;
                        if (k == 0) { a = vec3(X + 1, Y, 0); b = vec3(X + 1, Y + 1, 0); }
                        else if (k == 1) { a = vec3(X, Y, 0); b = vec3(X, Y + 1, 0); }
                        else if (k == 2) { a = vec3(X, Y + 1, 0); b = vec3(X + 1, Y + 1, 0); }
                        else { a = vec3(X, Y, 0); b = vec3(X + 1, Y, 0); }
                        vec3 out((float)dx4[k], (float)dy4[k], 0);
                        // upstairs, the wall below carries on the face; a cellar's walls stand in the earth
                        float z0 = (n || !L.underground) ? f : f - 0.3f;
                        S.quad(vec3(a.x, a.y, z0), vec3(b.x, b.y, z0), vec3(b.x, b.y, top), vec3(a.x, a.y, top), rock ? wc * 0.15f : wc, out);
                        if (t->wall == W_WINDOW) {
                            vec3 o = out * 0.012f, dir = b - a;
                            vec3 wa = a + dir * 0.3f + o, wb = a + dir * 0.7f + o;
                            float g0 = f + L.wallH * 0.3f, g1 = f + L.wallH * 0.8f;
                            S.flags = 4;
                            S.quad(vec3(wa.x, wa.y, g0), vec3(wb.x, wb.y, g0), vec3(wb.x, wb.y, g1), vec3(wa.x, wa.y, g1), vec3(0.16f, 0.2f, 0.28f), out);
                            S.flags = 0;
                        }
                    }
                }
            }
            // (the ladder up to a town wall stands on the ground and is drawn there, its top above
            // the wall walk: on the walk's landing it is only the way down, not drawn again, else
            // a second ladder rose from the landing as if to another floor)
            bool wallLadderTop = t->feature == F_LADDER && ((t->var >> 2) & 3) == 1;
            if (t->feature != F_NONE && !wallLadderTop) feature(S, t->feature, t->var, X + 0.5f, Y + 0.5f, f, hash2i(X, Y, 1));
        }
}

// ------------------------------------------------------------------ building parts

static void gableRoof(MeshBuf &m, const Part &p) {
    float x0 = p.a.x, y0 = p.a.y, x1 = p.b.x, y1 = p.b.y, ze = p.a.z, zr = p.b.z;
    if (p.axis == 0) {
        float ym = (y0 + y1) * 0.5f;
        m.quad(vec3(x0, y0, ze), vec3(x1, y0, ze), vec3(x1, ym, zr), vec3(x0, ym, zr), p.color, vec3(0, -1, 1));
        m.quad(vec3(x0, y1, ze), vec3(x1, y1, ze), vec3(x1, ym, zr), vec3(x0, ym, zr), p.color, vec3(0, 1, 1));
        float in = 0.0f;
        m.tri(vec3(x0 + in, y0 + 0.12f, ze), vec3(x0 + in, y1 - 0.12f, ze), vec3(x0 + in, ym, zr - 0.1f), p.color2, vec3(-1, 0, 0));
        m.tri(vec3(x1 - in, y0 + 0.12f, ze), vec3(x1 - in, y1 - 0.12f, ze), vec3(x1 - in, ym, zr - 0.1f), p.color2, vec3(1, 0, 0));
    } else {
        float xm = (x0 + x1) * 0.5f;
        m.quad(vec3(x0, y0, ze), vec3(x0, y1, ze), vec3(xm, y1, zr), vec3(xm, y0, zr), p.color, vec3(-1, 0, 1));
        m.quad(vec3(x1, y0, ze), vec3(x1, y1, ze), vec3(xm, y1, zr), vec3(xm, y0, zr), p.color, vec3(1, 0, 1));
        m.tri(vec3(x0 + 0.12f, y0, ze), vec3(x1 - 0.12f, y0, ze), vec3(xm, y0, zr - 0.1f), p.color2, vec3(0, -1, 0));
        m.tri(vec3(x0 + 0.12f, y1, ze), vec3(x1 - 0.12f, y1, ze), vec3(xm, y1, zr - 0.1f), p.color2, vec3(0, 1, 0));
    }
}

static void hipRoof(MeshBuf &m, float x0, float y0, float x1, float y1, float ze, float zr, int axis, vec3 col) {
    if (axis == 0) {
        float half = (y1 - y0) * 0.5f, ym = (y0 + y1) * 0.5f;
        float xa = x0 + half, xb = x1 - half;
        if (xa > xb) xa = xb = (x0 + x1) * 0.5f;
        m.quad(vec3(x0, y0, ze), vec3(x1, y0, ze), vec3(xb, ym, zr), vec3(xa, ym, zr), col, vec3(0, -1, 1));
        m.quad(vec3(x0, y1, ze), vec3(x1, y1, ze), vec3(xb, ym, zr), vec3(xa, ym, zr), col, vec3(0, 1, 1));
        m.tri(vec3(x0, y0, ze), vec3(x0, y1, ze), vec3(xa, ym, zr), col, vec3(-1, 0, 1));
        m.tri(vec3(x1, y0, ze), vec3(x1, y1, ze), vec3(xb, ym, zr), col, vec3(1, 0, 1));
    } else {
        float half = (x1 - x0) * 0.5f, xm = (x0 + x1) * 0.5f;
        float ya = y0 + half, yb = y1 - half;
        if (ya > yb) ya = yb = (y0 + y1) * 0.5f;
        m.quad(vec3(x0, y0, ze), vec3(x0, y1, ze), vec3(xm, yb, zr), vec3(xm, ya, zr), col, vec3(-1, 0, 1));
        m.quad(vec3(x1, y0, ze), vec3(x1, y1, ze), vec3(xm, yb, zr), vec3(xm, ya, zr), col, vec3(1, 0, 1));
        m.tri(vec3(x0, y0, ze), vec3(x1, y0, ze), vec3(xm, ya, zr), col, vec3(0, -1, 1));
        m.tri(vec3(x0, y1, ze), vec3(x1, y1, ze), vec3(xm, yb, zr), col, vec3(0, 1, 1));
    }
}

// windows on the four faces of a box
static void windowRows(MeshBuf &m, const Part &p) {
    float x0 = p.a.x, y0 = p.a.y, x1 = p.b.x, y1 = p.b.y, z0 = p.a.z, z1 = p.b.z;
    float storey = p.p1 > 0 ? p.p1 : 1.3f, spacing = p.sides > 0 ? p.sides / 100.0f : 1.0f, tall = p.p2 > 0 ? p.p2 : 0.55f;
    struct Face { vec3 a, b, out; };
    Face faces[4] = {{vec3(x0, y0, 0), vec3(x1, y0, 0), vec3(0, -1, 0)}, {vec3(x1, y0, 0), vec3(x1, y1, 0), vec3(1, 0, 0)},
                     {vec3(x1, y1, 0), vec3(x0, y1, 0), vec3(0, 1, 0)}, {vec3(x0, y1, 0), vec3(x0, y0, 0), vec3(-1, 0, 0)}};
    for (auto &f : faces) {
        vec3 d = f.b - f.a;
        float L = length(d);
        int count = (int)std::floor(L / spacing);
        if (count <= 0) continue;
        vec3 dir = d * (1.0f / L);
        float start = (L - count * spacing) * 0.5f + spacing * 0.5f;
        float ww = std::min(0.72f, spacing * 0.45f);
        for (float zb = z0; zb + storey <= z1 + 0.02f; zb += storey) {
            float wh = tall * storey, wz0 = zb + (storey - wh) * 0.5f, wz1 = wz0 + wh;
            for (int i = 0; i < count; i++) {
                vec3 c = f.a + dir * (start + i * spacing);
                vec3 o = f.out * 0.012f;
                vec3 a = c - dir * (ww * 0.5f) + o, b = c + dir * (ww * 0.5f) + o;
                float oldFlags = m.flags;
                m.flags = 4;
                m.quad(vec3(a.x, a.y, wz0), vec3(b.x, b.y, wz0), vec3(b.x, b.y, wz1), vec3(a.x, a.y, wz1), p.color, f.out);
                m.flags = oldFlags;
                vec3 o2 = f.out * 0.025f;
                vec3 sa = c - dir * (ww * 0.5f + 0.08f) + o2, sb = c + dir * (ww * 0.5f + 0.08f) + o2;
                m.quad(vec3(sa.x, sa.y, wz0 - 0.1f), vec3(sb.x, sb.y, wz0 - 0.1f), vec3(sb.x, sb.y, wz0),
                       vec3(sa.x, sa.y, wz0), p.color2, f.out);
            }
        }
    }
}

static void timberFrame(MeshBuf &m, const Part &p) {
    float x0 = p.a.x, y0 = p.a.y, x1 = p.b.x, y1 = p.b.y, z0 = p.a.z, z1 = p.b.z;
    float storey = p.p1 > 0 ? p.p1 : 1.3f;
    struct Face { vec3 a, b, out; };
    Face faces[4] = {{vec3(x0, y0, 0), vec3(x1, y0, 0), vec3(0, -1, 0)}, {vec3(x1, y0, 0), vec3(x1, y1, 0), vec3(1, 0, 0)},
                     {vec3(x1, y1, 0), vec3(x0, y1, 0), vec3(0, 1, 0)}, {vec3(x0, y1, 0), vec3(x0, y0, 0), vec3(-1, 0, 0)}};
    vec3 beam = p.color2;
    for (auto &f : faces) {
        vec3 d = f.b - f.a;
        float L = length(d);
        vec3 dir = d * (1.0f / L), o = f.out * 0.008f;
        for (float zb = z0; zb < z1 - 0.01f; zb += storey) {
            float zt = std::min(z1, zb + storey);
            vec3 a = f.a + o, b = f.b + o;
            m.quad(vec3(a.x, a.y, zb), vec3(b.x, b.y, zb), vec3(b.x, b.y, zb + 0.13f), vec3(a.x, a.y, zb + 0.13f), beam, f.out);
            int n = std::max(1, (int)std::round(L / 0.8f));
            for (int i = 0; i <= n; i++) {
                vec3 c = f.a + dir * (L * i / n) + o;
                vec3 ca = c - dir * 0.065f, cb = c + dir * 0.065f;
                m.quad(vec3(ca.x, ca.y, zb), vec3(cb.x, cb.y, zb), vec3(cb.x, cb.y, zt), vec3(ca.x, ca.y, zt), beam, f.out);
            }
            // braces at the ends
            for (int e = 0; e < 2; e++) {
                vec3 s0 = (e == 0 ? f.a : f.b) + o;
                vec3 s1 = s0 + dir * (e == 0 ? 0.8f : -0.8f);
                vec3 q0(s0.x, s0.y, zb + 0.13f), q1(s1.x, s1.y, zt);
                vec3 off(0, 0, 0.09f);
                m.quad(q0 - off, q0 + off, q1 + off, q1 - off, beam, f.out);
            }
        }
        vec3 a = f.a + o, b = f.b + o;
        m.quad(vec3(a.x, a.y, z1 - 0.13f), vec3(b.x, b.y, z1 - 0.13f), vec3(b.x, b.y, z1), vec3(a.x, a.y, z1), beam, f.out);
    }
}

void buildPartsMesh(const Building &bd, int id, MeshBuf &m) {
    for (const Part &p : bd.parts) {
        m.tag = p.upper ? (float)id : 0.0f;
        m.flags = 0;
        switch (p.kind) {
            case P_BOX:
                m.box(p.a, p.b, p.color);
                break;
            case P_GABLE:
                gableRoof(m, p);
                break;
            case P_HIP:
                hipRoof(m, p.a.x, p.a.y, p.b.x, p.b.y, p.a.z, p.b.z, p.axis, p.color);
                break;
            case P_PYRAMID: {
                float cx = (p.a.x + p.b.x) * 0.5f, cy = (p.a.y + p.b.y) * 0.5f;
                vec3 ap(cx, cy, p.b.z);
                vec3 c[4] = {vec3(p.a.x, p.a.y, p.a.z), vec3(p.b.x, p.a.y, p.a.z), vec3(p.b.x, p.b.y, p.a.z), vec3(p.a.x, p.b.y, p.a.z)};
                for (int i = 0; i < 4; i++) {
                    vec3 e = (c[i] + c[(i + 1) % 4]) * 0.5f;
                    m.tri(c[i], c[(i + 1) % 4], ap, p.color, vec3(e.x - cx, e.y - cy, 0.3f));
                }
                break;
            }
            case P_MANSARD: {
                float x0 = p.a.x, y0 = p.a.y, x1 = p.b.x, y1 = p.b.y, ze = p.a.z, zr = p.b.z;
                float half = 0.5f * std::min(x1 - x0, y1 - y0);
                float in = 0.26f * half, zm = ze + 0.7f * (zr - ze);
                vec3 o[4] = {vec3(x0, y0, ze), vec3(x1, y0, ze), vec3(x1, y1, ze), vec3(x0, y1, ze)};
                vec3 i4[4] = {vec3(x0 + in, y0 + in, zm), vec3(x1 - in, y0 + in, zm), vec3(x1 - in, y1 - in, zm), vec3(x0 + in, y1 - in, zm)};
                vec3 outs[4] = {vec3(0, -1, 0.3f), vec3(1, 0, 0.3f), vec3(0, 1, 0.3f), vec3(-1, 0, 0.3f)};
                for (int i = 0; i < 4; i++) m.quad(o[i], o[(i + 1) % 4], i4[(i + 1) % 4], i4[i], p.color, outs[i]);
                hipRoof(m, x0 + in, y0 + in, x1 - in, y1 - in, zm, zr, p.axis, p.color * 0.92f);
                if (p.p1 > 0) {
                    // dormers along the long sides
                    bool alongX = p.axis == 0;
                    float L = alongX ? x1 - x0 : y1 - y0;
                    int n = std::max(1, (int)std::floor((L - 1.0f) / 2.0f));
                    float step = L / n;
                    for (int s = 0; s < 2; s++)
                        for (int k = 0; k < n; k++) {
                            float c = (alongX ? x0 : y0) + step * (k + 0.5f);
                            float dz0 = ze + 0.25f, dz1 = ze + 1.2f;
                            vec3 lo, hi, out;
                            float hw = 0.38f, dp = 0.65f;
                            if (alongX) {
                                float yy = s == 0 ? y0 + 0.2f : y1 - 0.2f;
                                lo = vec3(c - hw, s == 0 ? yy : yy - dp, dz0); hi = vec3(c + hw, s == 0 ? yy + dp : yy, dz1);
                                out = vec3(0, s == 0 ? -1.0f : 1.0f, 0);
                            } else {
                                float xx = s == 0 ? x0 + 0.2f : x1 - 0.2f;
                                lo = vec3(s == 0 ? xx : xx - dp, c - hw, dz0); hi = vec3(s == 0 ? xx + dp : xx, c + hw, dz1);
                                out = vec3(s == 0 ? -1.0f : 1.0f, 0, 0);
                            }
                            m.box(lo, hi, p.color2);
                            // glass on the front face
                            vec3 fc = (lo + hi) * 0.5f;
                            vec3 fo = out * (dp * 0.5f) + out * 0.012f;
                            vec3 side = alongX ? vec3(0.22f, 0, 0) : vec3(0, 0.22f, 0);
                            vec3 base = fc + fo;
                            float of = m.flags;
                            m.flags = 4;
                            m.quad(vec3(base.x, base.y, dz0 + 0.15f) - side, vec3(base.x, base.y, dz0 + 0.15f) + side,
                                   vec3(base.x, base.y, dz1 - 0.12f) + side, vec3(base.x, base.y, dz1 - 0.12f) - side, vec3(0.16f, 0.2f, 0.28f), out);
                            m.flags = of;
                            Part gp; gp.kind = P_GABLE; gp.color = p.color; gp.color2 = p.color2;
                            gp.a = vec3(lo.x - 0.06f, lo.y - 0.06f, dz1); gp.b = vec3(hi.x + 0.06f, hi.y + 0.06f, dz1 + 0.4f);
                            gp.axis = alongX ? 1 : 0;
                            gableRoof(m, gp);
                        }
                }
                break;
            }
            case P_PRISM:   // p1 = 1: open at the top (a ring wall)
                m.prism(p.a.x, p.a.y, p.a.z, p.b.z, p.b.x, p.sides, p.color, 0, p.p1 < 0.5f);
                break;
            case P_CONE:
                m.cone(p.a.x, p.a.y, p.a.z, p.b.z, p.b.x, p.sides, p.color);
                break;
            case P_DOME: {
                // p1: height over radius (1 round; more: pointed, as Brunelleschi's); p2: white ribs
                float r = p.b.x, hz = p.p1 > 0 ? p.p1 : 1.0f;
                int n = p.sides;
                const float lats[5] = {0, 0.4f, 0.8f, 1.2f, PI_F / 2};
                auto P = [&](float a, float lat) {
                    return vec3(p.a.x + r * std::cos(lat) * std::cos(a), p.a.y + r * std::cos(lat) * std::sin(a), p.a.z + r * hz * std::sin(lat));
                };
                for (int k = 0; k < 4; k++)
                    for (int i = 0; i < n; i++) {
                        float a0 = 2 * PI_F * i / n, a1 = 2 * PI_F * (i + 1) / n;
                        vec3 q0 = P(a0, lats[k]), q1 = P(a1, lats[k]), q2 = P(a1, lats[k + 1]), q3 = P(a0, lats[k + 1]);
                        vec3 out = (q0 + q2) * 0.5f - p.a;
                        if (k == 3) m.tri(q0, q1, q2, p.color, out);
                        else m.quad(q0, q1, q2, q3, p.color, out);
                    }
                int ribs = (int)p.p2;
                for (int i = 0; i < ribs; i++) {   // a white rib on each edge of the dome
                    float a = 2 * PI_F * i / std::max(1, ribs), da = 0.06f;
                    for (int k = 0; k < 4; k++) {
                        vec3 q0 = P(a - da, lats[k]) * 1.0f, q1 = P(a + da, lats[k]), q2 = P(a + da, lats[k + 1]), q3 = P(a - da, lats[k + 1]);
                        vec3 c(p.a.x, p.a.y, p.a.z);
                        auto lift = [&](vec3 q) { vec3 d = q - c; return q + d * (0.03f / std::max(0.01f, length(d))); };
                        m.quad(lift(q0), lift(q1), lift(q2), lift(q3), p.color2, (q0 + q2) * 0.5f - c);
                    }
                }
                break;
            }
            case P_CRENEL: {
                float x0 = p.a.x, y0 = p.a.y, x1 = p.b.x, y1 = p.b.y, z = p.a.z, h = p.b.z - p.a.z;
                float ms = 0.28f, step = 0.56f;
                for (float x = x0; x + ms <= x1 + 1e-3f; x += step) {
                    m.box(vec3(x, y0, z), vec3(x + ms, y0 + ms, z + h), p.color);
                    m.box(vec3(x, y1 - ms, z), vec3(x + ms, y1, z + h), p.color);
                }
                for (float y = y0 + step; y + ms <= y1 - step + 1e-3f; y += step) {
                    m.box(vec3(x0, y, z), vec3(x0 + ms, y + ms, z + h), p.color);
                    m.box(vec3(x1 - ms, y, z), vec3(x1, y + ms, z + h), p.color);
                }
                break;
            }
            case P_WINDOWS:
                windowRows(m, p);
                break;
            case P_TIMBER:
                timberFrame(m, p);
                break;
            case P_BUTTRESS: {
                float w2 = p.p1 * 0.5f;
                float yo = p.a.y, yi = p.b.y;
                float dir = yi > yo ? 1.0f : -1.0f;
                // pier
                float py0 = std::min(yo, yo + dir * 0.5f), py1 = std::max(yo, yo + dir * 0.5f);
                m.box(vec3(p.a.x - w2, py0, p.p2), vec3(p.a.x + w2, py1, p.a.z + 0.3f), p.color);
                m.cone(p.a.x, (py0 + py1) * 0.5f, p.a.z + 0.3f, p.a.z + 0.9f, w2 * 1.2f, 4, p.color, PI_F / 4);
                // flyer
                vec3 f0(p.a.x, yo + dir * 0.25f, p.a.z), f1(p.b.x, yi, p.b.z);
                vec3 up(0, 0, 0.18f);
                vec3 sx(w2 * 0.7f, 0, 0);
                m.quad(f0 + up - sx, f0 + up + sx, f1 + up + sx, f1 + up - sx, p.color, vec3(0, -dir * 0.5f, 1));
                m.quad(f0 - sx, f0 + sx, f1 + sx, f1 - sx, p.color * 0.85f, vec3(0, dir * 0.5f, -1));
                m.quad(f0 - sx, f1 - sx, f1 - sx + up, f0 - sx + up, p.color, vec3(-1, 0, 0));
                m.quad(f0 + sx, f1 + sx, f1 + sx + up, f0 + sx + up, p.color, vec3(1, 0, 0));
                break;
            }
            case P_BALUSTRADE: {
                float x0 = p.a.x, y0 = p.a.y, x1 = p.b.x, y1 = p.b.y, z = p.a.z, h = p.b.z - p.a.z;
                float t = 0.12f;
                m.box(vec3(x0, y0, z + h - t), vec3(x1, y0 + t, z + h), p.color);
                m.box(vec3(x0, y1 - t, z + h - t), vec3(x1, y1, z + h), p.color);
                m.box(vec3(x0, y0, z + h - t), vec3(x0 + t, y1, z + h), p.color);
                m.box(vec3(x1 - t, y0, z + h - t), vec3(x1, y1, z + h), p.color);
                const float step = 0.4f, q = 0.05f;
                for (float x = x0 + 0.2f; x < x1 - 0.1f; x += step) {
                    m.box(vec3(x - q, y0 + 0.01f, z), vec3(x + q, y0 + 0.11f, z + h - t), p.color);
                    m.box(vec3(x - q, y1 - 0.11f, z), vec3(x + q, y1 - 0.01f, z + h - t), p.color);
                }
                for (float y = y0 + 0.2f; y < y1 - 0.1f; y += step) {
                    m.box(vec3(x0 + 0.01f, y - q, z), vec3(x0 + 0.11f, y + q, z + h - t), p.color);
                    m.box(vec3(x1 - 0.11f, y - q, z), vec3(x1 - 0.01f, y + q, z + h - t), p.color);
                }
                break;
            }
            case P_RING: {
                // a ring of roof: up from the outer eaves to a ridge, down to the inner eaves
                float ro = p.b.x, ri = p.b.y, ze = p.a.z, zr = p.b.z, rr = (ro + ri) * 0.5f;
                int n = p.sides;
                for (int i = 0; i < n; i++) {
                    float a0 = 2 * PI_F * i / n, a1 = 2 * PI_F * (i + 1) / n;
                    auto P = [&](float r, float a, float z) { return vec3(p.a.x + r * std::cos(a), p.a.y + r * std::sin(a), z); };
                    vec3 om((std::cos(a0) + std::cos(a1)) * 0.5f, (std::sin(a0) + std::sin(a1)) * 0.5f, 0);
                    m.quad(P(ro, a0, ze), P(ro, a1, ze), P(rr, a1, zr), P(rr, a0, zr), p.color, om + vec3(0, 0, 1));
                    m.quad(P(ri, a0, ze), P(ri, a1, ze), P(rr, a1, zr), P(rr, a0, zr), p.color * 0.9f, -om + vec3(0, 0, 1));
                }
                break;
            }
            case P_RINGWALL: {
                float r = p.b.x;
                int n = p.sides;
                for (int i = 0; i < n; i++) {
                    float a0 = 2 * PI_F * i / n, a1 = 2 * PI_F * (i + 1) / n;
                    vec3 q0(p.a.x + r * std::cos(a0), p.a.y + r * std::sin(a0), p.a.z), q1(p.a.x + r * std::cos(a1), p.a.y + r * std::sin(a1), p.a.z);
                    vec3 in(-(std::cos(a0) + std::cos(a1)), -(std::sin(a0) + std::sin(a1)), 0);
                    m.quad(q0, q1, vec3(q1.x, q1.y, p.b.z), vec3(q0.x, q0.y, p.b.z), p.color, in);
                    // gallery rails
                    for (float z = p.a.z + 1.0f; z < p.b.z; z += 1.3f) {
                        vec3 o = in * 0.02f;
                        m.quad(q0 + o + vec3(0, 0, z - p.a.z - 0.9f), q1 + o + vec3(0, 0, z - p.a.z - 0.9f),
                               q1 + o + vec3(0, 0, z - p.a.z - 0.3f), q0 + o + vec3(0, 0, z - p.a.z - 0.3f), p.color2, in);
                    }
                }
                break;
            }
            case P_ELLRING: {
                float rx = p.b.x, ry = p.b.y, H = p.b.z, t = p.p1, z0 = p.a.z;
                int n = std::max(12, p.sides);
                auto hAt = [&](int i) {   // the height at segment i: a stretch of the outer ring is fallen
                    float a = 2 * PI_F * (i + 0.5f) / n;
                    float fall = p.p2 > 0 && std::sin(a - 2.4f) > 0.35f ? p.p2 * (0.55f + 0.25f * std::sin(i * 2.7f)) : 0.0f;
                    return H * (1.0f - fall);
                };
                for (int i = 0; i < n; i++) {
                    float a0 = 2 * PI_F * i / n, a1 = 2 * PI_F * (i + 1) / n;
                    float h = hAt(i);
                    vec3 o0(p.a.x + rx * std::cos(a0), p.a.y + ry * std::sin(a0), z0), o1(p.a.x + rx * std::cos(a1), p.a.y + ry * std::sin(a1), z0);
                    vec3 i0(p.a.x + (rx - t) * std::cos(a0), p.a.y + (ry - t) * std::sin(a0), z0), i1(p.a.x + (rx - t) * std::cos(a1), p.a.y + (ry - t) * std::sin(a1), z0);
                    vec3 out((std::cos(a0) + std::cos(a1)) * 0.5f, (std::sin(a0) + std::sin(a1)) * 0.5f, 0);
                    vec3 up(0, 0, h);
                    m.quad(o0, o1, o1 + up, o0 + up, p.color, out);
                    m.quad(i0, i1, i1 + up, i0 + up, p.color * 0.9f, -out);
                    m.quad(o0 + up, o1 + up, i1 + up, i0 + up, p.color * 1.04f, vec3(0, 0, 1));
                    // arches: a dark opening in each bay of each storey, on the outer face
                    float storey = 2.2f;
                    for (float z = 0.25f; z + 1.2f < h; z += storey) {
                        float zt = std::min(h - 0.3f, z + storey * 0.62f);
                        vec3 e = (o1 - o0) * 0.2f, eo = out * 0.03f;
                        vec3 q0 = o0 + e + eo + vec3(0, 0, z), q1 = o1 - e + eo + vec3(0, 0, z);
                        m.quad(q0, q1, q1 + vec3(0, 0, zt - z), q0 + vec3(0, 0, zt - z), p.color2, out);
                    }
                }
                break;
            }
            case P_ROSE: {
                float r = p.b.x;
                vec3 n, u, v;
                switch (p.axis) {
                    case 0: n = vec3(-1, 0, 0); u = vec3(0, 1, 0); break;
                    case 1: n = vec3(1, 0, 0); u = vec3(0, 1, 0); break;
                    case 2: n = vec3(0, -1, 0); u = vec3(1, 0, 0); break;
                    default: n = vec3(0, 1, 0); u = vec3(1, 0, 0); break;
                }
                v = vec3(0, 0, 1);
                const int N = 16;
                auto P = [&](float rad, float a, float off) { return p.a + u * (rad * std::cos(a)) + v * (rad * std::sin(a)) + n * off; };
                for (int i = 0; i < N; i++) {
                    float a0 = 2 * PI_F * i / N, a1 = 2 * PI_F * (i + 1) / N;
                    m.quad(P(r, a0, 0.01f), P(r + 0.14f, a0, 0.01f), P(r + 0.14f, a1, 0.01f), P(r, a1, 0.01f), p.color2, n);
                    m.tri(P(0, 0, 0.015f), P(r, a0, 0.015f), P(r, a1, 0.015f), (i & 1) ? p.color : p.color * 1.35f, n);
                }
                for (int i = 0; i < 8; i++) {
                    float a = 2 * PI_F * i / 8;
                    vec3 s0 = P(0.12f, a, 0.02f), s1 = P(r, a, 0.02f);
                    vec3 side = (u * (-std::sin(a)) + v * std::cos(a)) * 0.035f;
                    m.quad(s0 - side, s1 - side, s1 + side, s0 + side, p.color2, n);
                }
                for (int i = 0; i < 8; i++) {
                    float a0 = 2 * PI_F * i / 8, a1 = 2 * PI_F * (i + 1) / 8;
                    m.tri(P(0, 0, 0.025f), P(0.14f, a0, 0.025f), P(0.14f, a1, 0.025f), p.color2, n);
                }
                break;
            }
            default:
                break;
        }
    }
    m.tag = 0;
}

// ------------------------------------------------------------------ figures

static void obox(MeshBuf &m, const mat4 &M, vec3 lo, vec3 hi, vec3 col) {
    vec3 p[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                 {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    vec3 q[8];
    for (int i = 0; i < 8; i++) {
        vec4 r = M * vec4(p[i].x, p[i].y, p[i].z, 1);
        q[i] = vec3(r.x, r.y, r.z);
    }
    vec4 cc = M * vec4((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f, 1);
    vec3 c(cc.x, cc.y, cc.z);
    int f[6][4] = {{0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}, {4, 5, 6, 7}, {0, 3, 2, 1}};
    for (auto &fa : f) {
        vec3 fc = (q[fa[0]] + q[fa[1]] + q[fa[2]] + q[fa[3]]) * 0.25f;
        m.quad(q[fa[0]], q[fa[1]], q[fa[2]], q[fa[3]], col, fc - c);
    }
}

static mat4 rotX(float a) {
    mat4 M;
    float c = std::cos(a), s = std::sin(a);
    M.at(1, 1) = c; M.at(1, 2) = -s;
    M.at(2, 1) = s; M.at(2, 2) = c;
    return M;
}

// ------------------------------------------------------------------ finer figures, close up
static bool gFine = false;
void setFineFigures(bool on) { gFine = on; }
bool fineFigures() { return gFine; }
// the middle level: the finer figure's code with about half the sides (gLo, while it is built) and without its small
// features (eyes, brows, nose, mouth, ears, buttons, the walking knee, the elbow, the hat's band)
static bool gMid = false, gLo = false;
void setMidFigures(bool on) { gMid = on; }
static int fewer(int n, int least) { return std::min(n, std::max(least, (n + 1) / 2)); }

static vec3 xf(const mat4 &M, vec3 p) { vec4 r = M * vec4(p.x, p.y, p.z, 1); return vec3(r.x, r.y, r.z); }

// a tapered n-sided tube along the local z axis, from z0 (radii rx0,ry0) to z1 (rx1,ry1), centred on
// (cx,cy); caps closed. Flat facets, as everything else in the game.
static void ofrustum(MeshBuf &m, const mat4 &M, float z0, float z1, float rx0, float ry0, float rx1, float ry1, int n, vec3 col,
                     float cx = 0, float cy = 0, bool cap0 = true, bool cap1 = true) {
    if (gLo) n = fewer(n, 5);
    vec3 c0 = xf(M, vec3(cx, cy, z0)), c1 = xf(M, vec3(cx, cy, z1)), mid = (c0 + c1) * 0.5f;
    for (int i = 0; i < n; i++) {
        float a0 = 2 * PI_F * i / n, a1 = 2 * PI_F * (i + 1) / n;
        vec3 p0 = xf(M, vec3(cx + rx0 * std::cos(a0), cy + ry0 * std::sin(a0), z0));
        vec3 p1 = xf(M, vec3(cx + rx0 * std::cos(a1), cy + ry0 * std::sin(a1), z0));
        vec3 q0 = xf(M, vec3(cx + rx1 * std::cos(a0), cy + ry1 * std::sin(a0), z1));
        vec3 q1 = xf(M, vec3(cx + rx1 * std::cos(a1), cy + ry1 * std::sin(a1), z1));
        m.quad(p0, p1, q1, q0, col, (p0 + p1 + q0 + q1) * 0.25f - mid);
        if (cap0) m.tri(c0, p1, p0, col, c0 - c1);
        if (cap1) m.tri(c1, q0, q1, col, c1 - c0);
    }
}

// an ellipsoid at c with radii r, nLon around and nLat from the south pole to the north; only the
// latitudes from lat0 to lat1 (radians, -pi/2..pi/2) if given (a cap of hair, a hood)
static void oellip(MeshBuf &m, const mat4 &M, vec3 c, vec3 r, int nLon, int nLat, vec3 col,
                   float lat0 = -PI_F / 2, float lat1 = PI_F / 2, float lon0 = 0, float lon1 = 2 * PI_F) {
    if (gLo) { nLon = fewer(nLon, 5); nLat = fewer(nLat, 2); }
    vec3 cw = xf(M, c);
    auto P = [&](int i, int j) {
        float lat = lat0 + (lat1 - lat0) * j / nLat, lon = lon0 + (lon1 - lon0) * i / nLon;
        return xf(M, vec3(c.x + r.x * std::cos(lat) * std::cos(lon), c.y + r.y * std::cos(lat) * std::sin(lon), c.z + r.z * std::sin(lat)));
    };
    for (int j = 0; j < nLat; j++)
        for (int i = 0; i < nLon; i++) {
            vec3 a = P(i, j), b = P(i + 1, j), d = P(i, j + 1), e = P(i + 1, j + 1);
            vec3 o = (a + b + d + e) * 0.25f - cw;
            if (j == 0 && lat0 <= -PI_F / 2 + 1e-4f) m.tri(a, e, d, col, o);
            else if (j == nLat - 1 && lat1 >= PI_F / 2 - 1e-4f) m.tri(a, b, d, col, o);
            else m.quad(a, b, e, d, col, o);
        }
}

// a superellipsoid: an ellipsoid whose cosines and sines are raised to the power p (p < 1: fuller, a rounded
// barrel; p = 1: the ellipsoid), with the same arguments as oellip
static float spow(float x, float p) { return x < 0 ? -std::pow(-x, p) : std::pow(x, p); }
static void osuper(MeshBuf &m, const mat4 &M, vec3 c, vec3 r, int nLon, int nLat, vec3 col, float p,
                   float lat0 = -PI_F / 2, float lat1 = PI_F / 2, float lon0 = 0, float lon1 = 2 * PI_F) {
    if (gLo) { nLon = fewer(nLon, 5); nLat = fewer(nLat, 2); }
    vec3 cw = xf(M, c);
    auto P = [&](int i, int j) {
        float lat = lat0 + (lat1 - lat0) * j / nLat, lon = lon0 + (lon1 - lon0) * i / nLon;
        float cl = spow(std::cos(lat), p);
        return xf(M, vec3(c.x + r.x * cl * spow(std::cos(lon), p), c.y + r.y * cl * spow(std::sin(lon), p), c.z + r.z * spow(std::sin(lat), p)));
    };
    for (int j = 0; j < nLat; j++)
        for (int i = 0; i < nLon; i++) {
            vec3 a = P(i, j), b = P(i + 1, j), d = P(i, j + 1), e = P(i + 1, j + 1);
            vec3 o = (a + b + d + e) * 0.25f - cw;
            if (j == 0 && lat0 <= -PI_F / 2 + 1e-4f) m.tri(a, e, d, col, o);
            else if (j == nLat - 1 && lat1 >= PI_F / 2 - 1e-4f) m.tri(a, b, d, col, o);
            else m.quad(a, b, e, d, col, o);
        }
}

void meshTube(MeshBuf &m, const mat4 &M, float z0, float z1, float rx0, float ry0, float rx1, float ry1, int n, vec3 col,
              float cx, float cy, bool cap0, bool cap1) { ofrustum(m, M, z0, z1, rx0, ry0, rx1, ry1, n, col, cx, cy, cap0, cap1); }
void meshEllipsoid(MeshBuf &m, const mat4 &M, vec3 c, vec3 r, int nLon, int nLat, vec3 col, float lat0, float lat1, float lon0, float lon1) {
    oellip(m, M, c, r, nLon, nLat, col, lat0, lat1, lon0, lon1);
}
static bool gFineFeatures = false;
static bool gFineFeatures2() { return gFineFeatures; }
void setFineFeatures(bool on) { gFineFeatures = on; }
bool fineFeatures() { return gFineFeatures; }
static bool featureFine(MeshBuf &m, int f, int var, float cx, float cy, float z, uint32_t seed);   // false: no finer drawing of it

static void buildFigureFine(MeshBuf &m, const mat4 &base0, float swing, const FigureLook &f, bool lying);
static const float kKneelSink = 0.22f;   // kneeling (FigureLook::kneel): the body is lowered this far, to the knees
static const float kSitSink = 0.2f;      // seated (FigureLook::sit): the hips on a chair at the height of the knees
// a chair of dark walnut with a leather seat and back, its back behind the sitter (-y); M is on the floor
static void buildChair(MeshBuf &m, const mat4 &M) {
    vec3 wood(0.30f, 0.18f, 0.10f), leather(0.42f, 0.21f, 0.12f);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            obox(m, M, vec3(sx * 0.19f - 0.022f, sy * 0.17f - 0.022f, 0.0f), vec3(sx * 0.19f + 0.022f, sy * 0.17f + 0.022f, sy < 0 ? 0.80f : 0.17f), wood);
    obox(m, M, vec3(-0.21f, -0.2f, 0.15f), vec3(0.21f, 0.2f, 0.19f), wood);           // the seat's frame
    obox(m, M, vec3(-0.19f, -0.18f, 0.19f), vec3(0.19f, 0.18f, 0.21f), leather);      // the seat
    obox(m, M, vec3(-0.17f, -0.185f, 0.36f), vec3(0.17f, -0.16f, 0.74f), leather);    // the back
    for (int sx = -1; sx <= 1; sx += 2)   // finials on the back posts
        obox(m, M, vec3(sx * 0.19f - 0.03f, -0.2f, 0.80f), vec3(sx * 0.19f + 0.03f, -0.14f, 0.85f), wood);
}
static void buildAnimalFine(MeshBuf &m, const AnimalLook &a, vec3 pos, float facing, float phase, float headDown, bool lying);

void buildFigure(MeshBuf &m, vec3 pos, float facing, float phase, vec3 coat, vec3 hose, vec3 hat) {
    FigureLook f;
    f.coat = coat; f.legs = hose; f.hat = hat;
    buildFigure(m, pos, facing, phase, f, false);
}

void buildFigure(MeshBuf &m, vec3 pos, float facing, float phase, const FigureLook &f, bool lying) {
    mat4 base = translate(pos) * rotateZ(facing);
    // lying on the tile at pos, head towards the facing (a bed's pillow is at +y), the body centred on the tile
    if (lying) base = translate(pos + vec3(0, 0, 0.42f)) * rotateZ(facing) * translate(vec3(0, -0.55f, 0)) * rotX(-PI_F / 2) * translate(vec3(0, 0, -0.05f));
    if (f.size != 1) base = base * scaleM(f.size);
    bool kneeling = f.kneel > 0 && !lying, sitting = f.sit && !lying && !kneeling;
    if (kneeling) base = base * translate(vec3(0, 0, -kKneelSink));   // the body down to the height of the knees
    if (sitting) { buildChair(m, base); base = base * translate(vec3(0, 0, -kSitSink)); }
    float swing = lying || kneeling || sitting ? 0.0f : std::sin(phase * 2 * PI_F) * 0.55f;
    if (gFine || gMid) { gLo = !gFine; buildFigureFine(m, base, swing, f, lying); gLo = false; return; }
    vec3 boots(0.30f, 0.20f, 0.12f), white(0.95f, 0.94f, 0.9f);
    if (f.statue) { boots = f.legs; white = f.coat; }
    bool longGown = f.dress || f.robe;
    if (kneeling) {   // on both knees: thighs upright, shins and feet along the floor behind
        mat4 fb = base * translate(vec3(0, 0, kKneelSink));   // the floor
        if (longGown) {   // the gown falls to the floor about the knees and spreads back over the shins
            vec3 skirt = f.robe ? f.coat : f.legs;
            obox(m, fb, vec3(-0.18f, -0.15f, 0.0f), vec3(0.18f, 0.14f, 0.31f), skirt * 0.95f);
            obox(m, fb, vec3(-0.165f, -0.38f, 0.0f), vec3(0.165f, -0.15f, 0.12f), skirt * 0.88f);
            for (int s = -1; s <= 1; s += 2)   // the soles of the feet, at the back
                obox(m, fb, vec3(s * 0.07f - 0.045f, -0.42f, 0.0f), vec3(s * 0.07f + 0.045f, -0.38f, 0.07f), boots);
        } else {
            for (int s = -1; s <= 1; s += 2) {
                obox(m, base, vec3(s * 0.07f - 0.05f, -0.055f, kKneelSink + 0.02f), vec3(s * 0.07f + 0.05f, 0.055f, 0.46f), f.legs);   // the thigh
                obox(m, fb, vec3(s * 0.07f - 0.05f, -0.30f, 0.0f), vec3(s * 0.07f + 0.05f, 0.055f, 0.1f), f.legs);                  // the shin
                obox(m, fb, vec3(s * 0.07f - 0.055f, -0.40f, 0.0f), vec3(s * 0.07f + 0.055f, -0.28f, 0.11f), boots);
            }
            obox(m, base, vec3(-0.15f, -0.09f, 0.40f), vec3(0.15f, 0.09f, 0.52f), f.coat * 0.9f);
        }
    } else if (sitting) {   // on the chair: thighs forward, shins down to the floor
        mat4 fb = base * translate(vec3(0, 0, kSitSink));   // the floor
        if (longGown) {   // the gown over the lap, and falling from the knees to the floor
            vec3 skirt = f.robe ? f.coat : f.legs;
            obox(m, fb, vec3(-0.18f, -0.13f, 0.19f), vec3(0.18f, 0.31f, 0.33f), skirt * 0.95f);
            obox(m, fb, vec3(-0.18f, 0.14f, 0.0f), vec3(0.18f, 0.32f, 0.2f), skirt * 0.9f);
            for (int s = -1; s <= 1; s += 2)   // the toes of the shoes
                obox(m, fb, vec3(s * 0.07f - 0.05f, 0.3f, 0.0f), vec3(s * 0.07f + 0.05f, 0.38f, 0.05f), boots);
        } else {
            for (int s = -1; s <= 1; s += 2) {
                obox(m, base, vec3(s * 0.07f - 0.05f, -0.06f, 0.40f), vec3(s * 0.07f + 0.05f, 0.28f, 0.50f), f.legs);   // the thigh
                obox(m, fb, vec3(s * 0.07f - 0.05f, 0.18f, 0.0f), vec3(s * 0.07f + 0.05f, 0.28f, 0.27f), f.legs);       // the shin
                obox(m, fb, vec3(s * 0.07f - 0.055f, 0.16f, 0.0f), vec3(s * 0.07f + 0.055f, 0.35f, 0.1f), boots);
            }
            obox(m, base, vec3(-0.15f, -0.09f, 0.40f), vec3(0.15f, 0.09f, 0.52f), f.coat * 0.9f);
        }
    } else if (longGown) {
        // a skirt or gown to the ankles, widening towards the hem
        vec3 skirt = f.robe ? f.coat : f.legs;
        obox(m, base, vec3(-0.17f, -0.13f, 0.02f), vec3(0.17f, 0.13f, 0.25f), skirt * 0.92f);
        obox(m, base, vec3(-0.155f, -0.115f, 0.25f), vec3(0.155f, 0.115f, 0.52f), skirt);
        for (int s = -1; s <= 1; s += 2)
            obox(m, base, vec3(s * 0.07f - 0.05f, -0.06f + 0.06f * s * swing, 0), vec3(s * 0.07f + 0.05f, 0.08f + 0.06f * s * swing, 0.05f), boots);
    } else {
        for (int s = -1; s <= 1; s += 2) {
            mat4 L = base * translate(vec3(s * 0.07f, 0, 0.46f)) * rotX(s * swing);
            obox(m, L, vec3(-0.05f, -0.055f, -0.46f), vec3(0.05f, 0.055f, 0), f.legs);
            obox(m, L, vec3(-0.055f, -0.07f, -0.46f), vec3(0.055f, 0.09f, -0.34f), boots);
        }
        obox(m, base, vec3(-0.15f, -0.09f, 0.40f), vec3(0.15f, 0.09f, 0.52f), f.coat * 0.9f);
    }
    // body, apron, collar
    obox(m, base, vec3(-0.14f, -0.085f, 0.50f), vec3(0.14f, 0.085f, 0.82f), f.coat);
    if (!f.robe && !f.dress) obox(m, base, vec3(-0.16f, -0.02f, 0.70f), vec3(0.16f, 0.1f, 0.84f), f.coat * 0.7f);
    if (f.apronOn) obox(m, base, vec3(-0.13f, 0.08f, longGown ? 0.12f : 0.3f), vec3(0.13f, 0.1f, 0.74f), f.apron);
    if (f.sashOn) {   // round the waist, and its ends hanging at the left hip
        obox(m, base, vec3(-0.155f, -0.1f, 0.52f), vec3(0.155f, 0.1f, 0.6f), f.sash);
        obox(m, base, vec3(-0.17f, -0.03f, 0.36f), vec3(-0.145f, 0.04f, 0.56f), f.sash * 0.85f);
    }
    obox(m, base, vec3(-0.09f, -0.08f, 0.80f), vec3(0.09f, 0.08f, 0.86f), f.robe ? f.coat * 0.6f : white);
    // a blow: the right arm swings up and forward, and comes down
    float raise = f.strike > 0 ? std::sin(std::min(1.0f, f.strike) * PI_F) * 2.0f : 0.0f;
    vec3 wood(0.40f, 0.27f, 0.15f), steel(0.72f, 0.74f, 0.78f), iron(0.25f, 0.25f, 0.27f);
    for (int s = -1; s <= 1; s += 2) {
        float a = -s * swing * 0.7f;
        if (s > 0 && f.weapon) a = f.weapon == 1 || f.weapon == 2 ? -0.5f : -0.25f;   // holding it
        if (f.carry == 1) a = 1.15f;               // both arms round the baby, in front
        if (f.carry == 4) a = 0.7f;                // holding the book out, in front
        if (f.carry == 2) a = -0.15f;              // steadying the pails
        if (f.carry == 3 && s > 0) a = 0.55f;      // the cap held out a little, in front
        if (sitting && !f.carry && !(s > 0 && f.weapon)) a = 0.45f;   // seated: the hands in the lap
        if (s > 0) a -= raise;
        mat4 A = base * translate(vec3(s * 0.185f, 0, 0.80f)) * rotX(a);
        obox(m, A, vec3(-0.045f, -0.045f, -0.34f), vec3(0.045f, 0.045f, 0), f.coat * 0.95f);
        obox(m, A, vec3(-0.035f, -0.035f, -0.40f), vec3(0.035f, 0.035f, -0.34f), f.skin);
        if (f.carry == 3 && s > 0) {   // the cap, held by its brim, hanging from the hand
            mat4 C = A * translate(vec3(0, 0.03f, -0.46f)) * rotX(PI_F / 2);
            ofrustum(m, C, -0.007f, 0.007f, 0.082f, 0.082f, 0.082f, 0.082f, 10, f.hat);
            oellip(m, C, vec3(0, 0, 0.007f), vec3(0.052f, 0.052f, 0.04f), 8, 3, f.hat * 0.9f, 0, PI_F / 2);
        }
        if (s < 0 || !f.weapon) continue;
        // the weapon, in the hand (it moves with the arm)
        mat4 H = A * translate(vec3(0, 0, -0.38f));
        switch (f.weapon) {
            case 1:   // a musket: stock and barrel, held forward like a club when striking
                obox(m, H, vec3(-0.025f, -0.18f, -0.04f), vec3(0.025f, 0.12f, 0.03f), wood);
                obox(m, H, vec3(-0.014f, 0.12f, -0.02f), vec3(0.014f, 1.05f, 0.01f), iron);
                break;
            case 2:   // a pike, very long
                obox(m, H, vec3(-0.02f, -0.6f, -0.02f), vec3(0.02f, 2.6f, 0.02f), wood);
                obox(m, H, vec3(-0.025f, 2.6f, -0.025f), vec3(0.025f, 2.8f, 0.025f), steel);
                break;
            case 3:   // a sword: grip, guard and a straight blade
                obox(m, H, vec3(-0.015f, -0.02f, -0.06f), vec3(0.015f, 0.02f, 0.04f), wood);
                obox(m, H, vec3(-0.07f, -0.015f, -0.08f), vec3(0.07f, 0.015f, -0.06f), steel);
                obox(m, H, vec3(-0.012f, -0.006f, -0.72f), vec3(0.012f, 0.006f, -0.08f), steel);
                break;
            case 4:   // a sabre: curved, drawn as two pieces
                obox(m, H, vec3(-0.015f, -0.02f, -0.06f), vec3(0.015f, 0.02f, 0.04f), wood);
                obox(m, H, vec3(-0.05f, -0.015f, -0.08f), vec3(0.05f, 0.015f, -0.06f), steel);
                obox(m, H, vec3(-0.014f, -0.006f, -0.42f), vec3(0.014f, 0.006f, -0.08f), steel);
                obox(m, H, vec3(-0.014f, 0.0f, -0.66f), vec3(0.014f, 0.08f, -0.42f), steel);
                break;
        }
    }
    if (f.halberd) {
        mat4 A = base * translate(vec3(0.24f, 0.05f, 0));
        obox(m, A, vec3(-0.02f, -0.02f, 0.0f), vec3(0.02f, 0.02f, 1.7f), vec3(0.4f, 0.28f, 0.16f));
        obox(m, A, vec3(-0.015f, -0.12f, 1.45f), vec3(0.015f, 0.04f, 1.62f), vec3(0.7f, 0.72f, 0.75f));
        obox(m, A, vec3(-0.01f, -0.01f, 1.62f), vec3(0.01f, 0.01f, 1.85f), vec3(0.7f, 0.72f, 0.75f));
    }
    if (f.lute) {
        mat4 A = base * translate(vec3(-0.05f, 0.12f, 0.62f)) * rotateZ(0.3f);
        obox(m, A, vec3(-0.11f, -0.04f, -0.12f), vec3(0.11f, 0.04f, 0.1f), vec3(0.62f, 0.4f, 0.2f));
        obox(m, A, vec3(0.11f, -0.02f, -0.02f), vec3(0.36f, 0.02f, 0.02f), vec3(0.35f, 0.22f, 0.12f));
    }
    if (f.carry == 4) {   // a little book of sewn pages, held out in both hands
        obox(m, base, vec3(-0.075f, 0.2f, 0.49f), vec3(0.075f, 0.3f, 0.525f), vec3(0.45f, 0.3f, 0.17f));
        obox(m, base, vec3(-0.068f, 0.205f, 0.525f), vec3(0.068f, 0.295f, 0.535f), vec3(0.93f, 0.9f, 0.8f));
    }
    if (f.carry == 1) {   // a swaddled baby across the arms, its head to the right
        obox(m, base, vec3(-0.12f, 0.16f, 0.6f), vec3(0.09f, 0.3f, 0.71f), vec3(0.92f, 0.9f, 0.84f));
        obox(m, base, vec3(0.09f, 0.18f, 0.61f), vec3(0.17f, 0.28f, 0.71f), f.babySkin);
        obox(m, base, vec3(0.09f, 0.18f, 0.69f), vec3(0.17f, 0.28f, 0.72f), vec3(0.92f, 0.9f, 0.84f));   // its cap
    }
    if (f.carry == 2) {   // a yoke on the shoulders, two pails hanging from it
        vec3 wood(0.45f, 0.32f, 0.18f), pail(0.62f, 0.5f, 0.34f), rope(0.55f, 0.48f, 0.36f);
        obox(m, base, vec3(-0.5f, -0.04f, 0.84f), vec3(0.5f, 0.04f, 0.88f), wood);
        for (int s = -1; s <= 1; s += 2) {
            obox(m, base, vec3(s * 0.46f - 0.01f, -0.01f, 0.56f), vec3(s * 0.46f + 0.01f, 0.01f, 0.84f), rope);
            obox(m, base, vec3(s * 0.46f - 0.08f, -0.08f, 0.36f), vec3(s * 0.46f + 0.08f, 0.08f, 0.56f), pail);
        }
    }
    // head and hair (a small child's head is larger for its size: about the neck)
    mat4 hb = f.headK != 1 ? base * translate(vec3(0, 0, 0.86f)) * scaleM(f.headK) * translate(vec3(0, 0, -0.86f)) : base;
    if (f.asshead) {   // the stage's ass's head: a block with a muzzle and two ears, or a hood with the muzzle out of it
        vec3 ash(0.52f, 0.47f, 0.42f);
        obox(m, hb, vec3(-0.085f, -0.09f, 0.86f), vec3(0.085f, 0.08f, 1.06f), ash);
        obox(m, hb, vec3(-0.05f, 0.06f, 0.85f), vec3(0.05f, 0.2f, 0.95f), vec3(0.8f, 0.76f, 0.7f));
        if (f.hatKind == 6) obox(m, hb, vec3(-0.105f, -0.13f, 0.84f), vec3(0.105f, 0.09f, 1.1f), f.hat);
        else
            for (int s = -1; s <= 1; s += 2) obox(m, hb, vec3(s * 0.045f - 0.015f, -0.035f, 1.05f), vec3(s * 0.045f + 0.015f, -0.01f, 1.2f), ash);
        return;
    }
    obox(m, hb, vec3(-0.075f, -0.075f, 0.86f), vec3(0.075f, 0.075f, 1.02f), f.skin);
    obox(m, hb, vec3(-0.08f, -0.08f, 0.94f), vec3(0.08f, 0.02f, 1.03f), f.hair);
    if (f.longHair) obox(m, hb, vec3(-0.082f, -0.092f, 0.76f), vec3(0.082f, -0.035f, 0.97f), f.hair);
    if (f.fringe) {
        // so that the hair shows under a hat, from in front: a fringe, and locks over the ears
        obox(m, hb, vec3(-0.08f, 0.06f, 0.955f), vec3(0.08f, 0.083f, 1.025f), f.hair);
        float low = f.longHair ? 0.74f : 0.90f;
        for (int s = -1; s <= 1; s += 2)
            obox(m, hb, vec3(s > 0 ? 0.05f : -0.092f, -0.085f, low), vec3(s > 0 ? 0.092f : -0.05f, 0.082f, 1.025f), f.hair);
    }
    {
    const mat4 &base = hb;   // the hats sit on the head
    switch (f.hatKind) {
        case 1:   // a broad-brimmed hat
            obox(m, base, vec3(-0.17f, -0.17f, 1.02f), vec3(0.17f, 0.17f, 1.045f), f.hat);
            obox(m, base, vec3(-0.085f, -0.085f, 1.04f), vec3(0.085f, 0.085f, 1.16f), f.hat);
            obox(m, base, vec3(-0.087f, -0.087f, 1.05f), vec3(0.087f, 0.087f, 1.075f), vec3(0.7f, 0.6f, 0.3f));
            break;
        case 2:   // a cap
            obox(m, base, vec3(-0.085f, -0.085f, 1.0f), vec3(0.085f, 0.09f, 1.07f), f.hat);
            break;
        case 3:   // a white coif
            obox(m, base, vec3(-0.088f, -0.09f, 0.9f), vec3(0.088f, 0.05f, 1.06f), f.hat);
            break;
        case 4:   // a morion helmet
            obox(m, base, vec3(-0.14f, -0.1f, 1.01f), vec3(0.14f, 0.1f, 1.04f), f.hat);
            obox(m, base, vec3(-0.08f, -0.08f, 1.04f), vec3(0.08f, 0.08f, 1.13f), f.hat);
            obox(m, base, vec3(-0.015f, -0.09f, 1.1f), vec3(0.015f, 0.09f, 1.19f), f.hat);
            break;
        case 5:   // a square scholar's cap
            obox(m, base, vec3(-0.08f, -0.08f, 1.01f), vec3(0.08f, 0.08f, 1.06f), f.hat);
            obox(m, base, vec3(-0.12f, -0.12f, 1.06f), vec3(0.12f, 0.12f, 1.08f), f.hat);
            break;
        case 6:   // a hood
            obox(m, base, vec3(-0.095f, -0.1f, 0.84f), vec3(0.095f, 0.06f, 1.08f), f.hat);
            break;
        case 7:   // a biretta
            obox(m, base, vec3(-0.08f, -0.08f, 1.01f), vec3(0.08f, 0.08f, 1.1f), f.hat);
            obox(m, base, vec3(-0.02f, -0.02f, 1.1f), vec3(0.02f, 0.02f, 1.14f), f.hat);
            break;
        default:
            break;
    }
    }
}

// ------------------------------------------------------------------ the finer figure
// A frame with axes X, Y, Z and origin O.
static mat4 frameM(vec3 X, vec3 Y, vec3 Z, vec3 O) {
    mat4 r = translate(O);
    r.m[0] = X.x; r.m[1] = X.y; r.m[2] = X.z;
    r.m[4] = Y.x; r.m[5] = Y.y; r.m[6] = Y.z;
    r.m[8] = Z.x; r.m[9] = Z.y; r.m[10] = Z.z;
    return r;
}
// The frame of a limb's bone from O along `bone` (unit): its -z runs along the bone, its y forward where it can.
static mat4 boneFrame(vec3 bone, vec3 O) {
    vec3 Z = bone * -1.0f, ref = std::fabs(Z.x) > 0.95f ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 Y = normalize(cross(Z, ref)), X = cross(Y, Z);
    if (Y.y < 0 && ref.x > 0) { Y = Y * -1.0f; X = X * -1.0f; }
    return frameM(X, Y, Z, O);
}
// A two-bone reach: the arm from the shoulder P puts the hand's centre at T (both in the body's frame), the upper
// arm L1 long and the forearm L2, the elbow bent towards `pole` but never in front of the line from the shoulder to
// the hand (an elbow bends backwards). Gives the frames of the upper arm and of the forearm.
static void armReach(vec3 P, vec3 T, vec3 pole, float L1, float L2, mat4 &A, mat4 &F) {
    vec3 d = T - P;
    float dl = std::max(1e-4f, length(d));
    vec3 dn = d * (1.0f / dl);
    float dc = std::min(dl, L1 + L2 - 1e-4f);
    float ca = std::max(-1.0f, std::min(1.0f, (L1 * L1 + dc * dc - L2 * L2) / (2 * L1 * dc)));
    float al = std::acos(ca);
    vec3 pp = pole - dn * dot(pole, dn);
    if (pp.y > 0) {   // the elbow would come forward: the nearest direction level with the line instead
        vec3 w = cross(dn, vec3(0, 1, 0));
        if (length(w) > 1e-4f) { w = normalize(w); pp = dot(w, pp) >= 0 ? w : w * -1.0f; }
        else pp = vec3(0, 0, -1);
    }
    pp = normalize(pp);
    vec3 u = dn * std::cos(al) + pp * std::sin(al);
    vec3 E = P + u * L1;
    A = boneFrame(u, P);
    F = boneFrame(normalize(T - E), E);
}
// Where the hands go when they hold something (in the body's frame; s: +1 the right hand, -1 the left).
static bool reachTarget(const FigureLook &f, int s, bool sitting, bool holding, vec3 &T) {
    if (f.carry == 4) { T = vec3(s * 0.088f, 0.235f, 0.55f); return true; }   // the book, by its two edges
    if (f.carry == 1) { T = s > 0 ? vec3(0.10f, 0.24f, 0.60f) : vec3(-0.11f, 0.24f, 0.61f); return true; }   // under the baby's head and feet
    if (f.carry == 2) { T = vec3(s * 0.24f, 0.16f, 0.62f); return true; }   // the ropes of the yoke, drawn forward
    if (sitting && !holding) { T = vec3(s * 0.08f, 0.16f, 0.545f); return true; }   // on the thighs
    if (f.halberd && s > 0) { T = vec3(0.2f, 0.07f, 0.8f); return true; }   // round the staff
    if (f.lute) { T = s > 0 ? vec3(0.19f, 0.21f, 0.6f) : vec3(-0.06f, 0.2f, 0.62f); return true; }   // the neck; the strings at the rose
    return false;
}

// The same figure as buildFigure, part for part and at the same places, only rounder, with jointed
// limbs: drawn when the camera is close. The hands reach to what they hold.
// tilted sideways: +z leans towards +x for a > 0
static mat4 rotY(float a) {
    mat4 M;
    float c = std::cos(a), s = std::sin(a);
    M.at(0, 0) = c; M.at(0, 2) = s;
    M.at(2, 0) = -s; M.at(2, 2) = c;
    return M;
}

// the stage's ass's head, of painted canvas, worn over the whole head (hb: the head's frame): a round skull, a long
// muzzle forward and down with a pale nose, painted eyes, a dark mane down the back and two tall ears; under a hood
// (hooded) the ears are folded away and the muzzle sticks out of it
static void assHead(MeshBuf &m, const mat4 &hb, bool hooded, vec3 hood) {
    vec3 ash(0.52f, 0.47f, 0.42f), pale(0.80f, 0.76f, 0.70f), dark(0.18f, 0.15f, 0.13f);
    oellip(m, hb, vec3(0, -0.01f, 0.975f), vec3(0.092f, 0.1f, 0.105f), 12, 7, ash);   // the skull
    mat4 M = hb * translate(vec3(0, 0.03f, 0.95f)) * rotX(-(PI_F / 2 + 0.45f));      // along the muzzle, forward and down
    ofrustum(m, M, 0.0f, 0.17f, 0.062f, 0.07f, 0.048f, 0.052f, 10, ash, 0, 0, false, false);
    oellip(m, M, vec3(0, 0, 0.17f), vec3(0.05f, 0.055f, 0.036f), 10, 4, pale);       // the nose
    if (!gLo)
        for (int s = -1; s <= 1; s += 2) {
            oellip(m, M, vec3(s * 0.02f, 0.012f, 0.2f), vec3(0.008f, 0.01f, 0.006f), 6, 3, dark);   // the nostrils
            oellip(m, hb, vec3(s * 0.076f, 0.04f, 0.995f), vec3(0.012f, 0.017f, 0.013f), 6, 3, dark);   // the painted eyes
        }
    if (hooded) {
        oellip(m, hb, vec3(0, -0.035f, 0.97f), vec3(0.112f, 0.118f, 0.135f), 12, 6, hood, -0.7f, PI_F / 2);
        return;
    }
    oellip(m, hb, vec3(0, -0.075f, 1.0f), vec3(0.022f, 0.05f, 0.09f), 8, 4, dark);   // the mane
    for (int s = -1; s <= 1; s += 2) {   // the ears, tall, leaning out a little and back
        mat4 E = hb * translate(vec3(s * 0.05f, -0.025f, 1.06f)) * rotY(s * 0.3f) * rotX(0.2f);
        oellip(m, E, vec3(0, 0, 0.07f), vec3(0.024f, 0.014f, 0.085f), 8, 4, ash);
        if (!gLo) oellip(m, E, vec3(0, 0.008f, 0.07f), vec3(0.013f, 0.008f, 0.06f), 6, 3, dark);   // inside the ear
    }
}

static void buildFigureFine(MeshBuf &m, const mat4 &base, float swing, const FigureLook &f, bool lying) {
    vec3 boots(0.30f, 0.20f, 0.12f), white(0.95f, 0.94f, 0.9f), dark(0.10f, 0.08f, 0.08f);
    if (f.statue) { boots = f.legs; white = f.coat; dark = f.skin * 0.8f; }
    bool longGown = f.dress || f.robe;
    bool kneeling = f.kneel > 0 && !lying, sitting = f.sit && !lying && !kneeling;
    bool wm = f.woman && f.dress;   // a woman's gown: a bodice, the skirt from a higher waist, a broad collar
    float st = wm ? 0.0f : f.stout;   // a man's paunch: the coat's skirts, a gown, a sash, an apron and the buttons follow it
    // the paunch: an ellipsoid in front of the body, low, a little wider at the sides; pf(z): how far forward it reaches
    vec3 pc(0, 0.035f * st, 0.56f), pr(0.13f + 0.015f * st, 0.082f + 0.023f * st, 0.13f);
    auto pf = [&](float z) { float t = (z - pc.z) / pr.z; return st > 0 && std::fabs(t) < 1 ? pc.y + pr.y * std::sqrt(1 - t * t) : 0.0f; };
    // what hangs from it reaches forward to it with its back unmoved: half the reach added to its depth, half to its centre
    float ca = std::max(0.0f, (pf(0.535f) - 0.093f) * 0.5f), ga = wm ? 0.0f : std::max(0.0f, (pf(0.53f) - 0.104f) * 0.5f);
    // ub: the frame of everything above the hips; it leans forward about them with age (a stoop), and a little
    // when kneeling in a gown
    float lean = (kneeling && longGown ? 0.12f : 0.0f) + (lying ? 0.0f : 0.2f * f.age);
    mat4 ub = lean > 0 ? base * translate(vec3(0, 0, 0.5f)) * rotX(-lean) * translate(vec3(0, 0, -0.5f)) : base;
    vec3 hair = f.age > 0 ? f.hair * (1 - 0.85f * f.age) + vec3(0.80f, 0.79f, 0.76f) * (0.85f * f.age) : f.hair;   // greying
    if (sitting) {   // on a chair (buildFigure drew it, and lowered the body): thighs forward, shins down
        mat4 fb = base * translate(vec3(0, 0, kSitSink));   // the floor
        if (longGown) {   // the gown over the lap, and falling from the knees to the floor
            vec3 skirt = f.robe ? f.coat : f.legs;
            mat4 L = base * translate(vec3(0, -0.06f, 0.46f)) * rotX(-PI_F / 2);   // along the thighs, forward
            ofrustum(m, L, 0.0f, 0.34f, 0.19f, 0.09f, 0.185f, 0.085f, 14, skirt * 0.96f);
            ofrustum(m, fb * translate(vec3(0, 0.2f, 0)), 0.0f, 0.3f, 0.2f, 0.12f, 0.185f, 0.085f, 14, skirt * 0.92f);
            ofrustum(m, fb * translate(vec3(0, 0.2f, 0)), 0.0f, 0.05f, 0.207f, 0.127f, 0.203f, 0.123f, 14, skirt * 0.8f, 0, 0, false, false);   // the hem
            for (int s = -1; s <= 1; s += 2)   // the toes of the shoes, peeping out
                oellip(m, fb, vec3(s * 0.07f, 0.33f, 0.026f), vec3(0.042f, 0.055f, 0.026f), 8, 4, boots);
        } else {
            for (int s = -1; s <= 1; s += 2) {
                mat4 T = base * translate(vec3(s * 0.07f, 0, 0.46f)) * rotX(-PI_F / 2);   // forward
                ofrustum(m, T, 0.0f, 0.225f, 0.058f, 0.061f, 0.045f, 0.047f, 8, f.legs);                   // the thigh
                mat4 K = base * translate(vec3(s * 0.07f, 0.225f, 0.46f));
                oellip(m, K, vec3(0, 0, 0), vec3(0.046f, 0.048f, 0.04f), 8, 4, f.legs);                    // the knee
                ofrustum(m, K, -0.12f, 0.0f, 0.039f, 0.041f, 0.045f, 0.047f, 8, f.legs);                   // the calf
                ofrustum(m, K, -0.228f, -0.105f, 0.047f, 0.05f, 0.052f, 0.055f, 8, boots);                 // the boot
                oellip(m, K, vec3(0, 0.03f, -0.214f), vec3(0.047f, 0.085f, 0.03f), 8, 4, boots * 0.85f);   // the foot
            }
            ofrustum(m, base, 0.38f, 0.535f, 0.168f, 0.108f + ca, 0.148f, 0.093f + ca, 12, f.coat * 0.9f, 0, ca);   // the coat's skirts
        }
    } else if (kneeling) {   // on both knees (buildFigure has lowered the body): thighs upright, shins along the floor
        mat4 fb = base * translate(vec3(0, 0, kKneelSink));   // the floor
        if (longGown) {   // the gown falls to the floor about the knees and spreads back over the shins
            vec3 skirt = f.robe ? f.coat : f.legs;
            ofrustum(m, fb, 0.0f, 0.32f, 0.215f, 0.19f, 0.15f, 0.104f, 14, skirt * 0.96f);
            ofrustum(m, fb, 0.0f, 0.05f, 0.222f, 0.197f, 0.217f, 0.192f, 14, skirt * 0.8f, 0, 0, false, false);   // the hem
            oellip(m, fb, vec3(0, -0.13f, 0.0f), vec3(0.205f, 0.29f, 0.13f), 14, 3, skirt * 0.93f, 0.0f, PI_F / 2);   // over the shins and the heels
        } else {
            for (int s = -1; s <= 1; s += 2) {
                mat4 T = base * translate(vec3(s * 0.07f, 0, 0.46f));
                ofrustum(m, T, -(0.46f - kKneelSink - 0.03f), 0.0f, 0.045f, 0.047f, 0.058f, 0.061f, 8, f.legs);   // the thigh, upright
                mat4 K = fb * translate(vec3(s * 0.07f, 0, 0.045f));
                oellip(m, K, vec3(0, 0, 0), vec3(0.047f, 0.05f, 0.045f), 8, 4, f.legs);                         // the knee, on the floor
                mat4 S = K * rotX(PI_F / 2);                                                                     // along the floor, behind
                ofrustum(m, S, 0.0f, 0.2f, 0.045f, 0.044f, 0.039f, 0.036f, 8, f.legs);                         // the shin
                ofrustum(m, S, 0.19f, 0.3f, 0.05f, 0.045f, 0.05f, 0.042f, 8, boots);                           // the boot
                oellip(m, S, vec3(0, -0.01f, 0.31f), vec3(0.047f, 0.034f, 0.05f), 8, 4, boots * 0.85f);       // the foot
            }
            ofrustum(m, base, 0.38f, 0.535f, 0.168f, 0.108f + ca, 0.148f, 0.093f + ca, 12, f.coat * 0.9f, 0, ca);   // the coat's skirts
        }
    } else if (longGown) {
        vec3 skirt = f.robe ? f.coat : f.legs;
        if (wm) ofrustum(m, base, 0.02f, 0.60f, 0.215f, 0.175f, 0.125f, 0.09f, 14, skirt * 0.96f);   // from a higher waist
        else ofrustum(m, base, 0.02f, 0.53f, 0.205f, 0.165f, 0.15f, 0.104f + ga, 14, skirt * 0.96f, 0, ga);   // out in front to a paunch
        float hk = wm ? 0.01f : 0.0f;
        ofrustum(m, base, 0.015f, 0.065f, 0.212f + hk, 0.172f + hk, 0.207f + hk, 0.167f + hk, 14, skirt * 0.8f, 0, ga, false, false);   // the hem
        for (int s = -1; s <= 1; s += 2)   // the toes of the shoes, peeping out
            oellip(m, base, vec3(s * 0.07f, 0.12f + 0.05f * s * swing, 0.026f), vec3(0.042f, 0.055f, 0.026f), 8, 4, boots);
    } else {
        for (int s = -1; s <= 1; s += 2) {
            mat4 T = base * translate(vec3(s * 0.07f, 0, 0.46f)) * rotX(s * swing);
            ofrustum(m, T, -0.225f, 0.0f, 0.045f, 0.047f, 0.058f, 0.061f, 8, f.legs);                 // the thigh
            float kb = lying ? 0.0f : -0.7f * std::max(0.0f, -s * swing);                           // the knee bends behind
            mat4 K = T * translate(vec3(0, 0, -0.225f)) * rotX(kb);
            if (!gLo) oellip(m, K, vec3(0, 0, 0), vec3(0.046f, 0.048f, 0.04f), 8, 4, f.legs);       // the knee
            ofrustum(m, K, -0.12f, 0.0f, 0.039f, 0.041f, 0.045f, 0.047f, 8, f.legs);                // the calf
            ofrustum(m, K, -0.228f, -0.105f, 0.047f, 0.05f, 0.052f, 0.055f, 8, boots);              // the boot
            oellip(m, K, vec3(0, 0.03f, -0.214f), vec3(0.047f, 0.085f, 0.03f), 8, 4, boots * 0.85f);  // the foot
        }
        ofrustum(m, base, 0.38f, 0.535f, 0.168f, 0.108f + ca, 0.148f, 0.093f + ca, 12, f.coat * 0.9f, 0, ca);       // the coat's skirts
    }
    // the body: a waist, a chest, and shoulders sloping from over the tops of the arms to the neck
    if (wm) {   // a bodice, narrow at the waist and fuller at the chest (from lower down when the gown is over the lap or the knees)
        ofrustum(m, ub, sitting || kneeling ? 0.50f : 0.56f, 0.71f, 0.11f, 0.076f, 0.14f, 0.097f, 12, f.coat, 0, 0, true, false);
        ofrustum(m, ub, 0.71f, 0.80f, 0.14f, 0.097f, 0.145f, 0.09f, 12, f.coat, 0, 0, false, false);
    } else {
        ofrustum(m, ub, 0.50f, 0.80f, 0.13f, 0.082f, 0.152f, 0.093f, 12, f.coat);
        if (st > 0) oellip(m, ub, pc, pr, 12, 6, f.coat);   // a paunch in front
    }
    ofrustum(m, ub, 0.77f, 0.86f, 0.215f * (wm ? 0.92f : 1.0f), 0.097f, 0.075f, 0.062f, 12, f.coat);   // a woman's a little narrower
    if (!f.robe && !f.dress) {
        ofrustum(m, ub, 0.68f, 0.84f, 0.16f, std::max(0.1f, pf(0.68f) + 0.004f), 0.135f, 0.085f, 12, f.coat * 0.75f, 0, 0.004f, false, false);   // the doublet
        if (!f.statue && !gLo)
            for (int k = 0; k < 4; k++) {   // buttons down the front, on a paunch's curve
                float z = 0.56f + k * 0.065f;
                if (f.apronOn && z < 0.74f) continue;   // under the apron
                oellip(m, ub, vec3(0, std::max(0.094f, pf(z) + 0.004f), z), vec3(0.011f, 0.008f, 0.011f), 6, 3, vec3(0.72f, 0.62f, 0.34f));
            }
    }
    // a piece of an apron: a thin flat tube from (z0, y0) to (z1, y1) in front of the body, 4 mm out, w0 to w1 wide
    auto panel = [&](const mat4 &M, float z0, float z1, float y0, float y1, float w0, float w1) {
        float ang = std::atan2(y0 - y1, z1 - z0), half = 0.5f * std::sqrt((z1 - z0) * (z1 - z0) + (y1 - y0) * (y1 - y0));
        ofrustum(m, M * translate(vec3(0, 0.5f * (y0 + y1) + 0.004f, 0.5f * (z0 + z1))) * rotX(ang), -half, half, w0, 0.006f, w1, 0.005f, 6, f.apron);
    };
    if (f.apronOn && longGown && !kneeling && !sitting) {   // on a gown: lying on the skirt's slope from the waist down, a bib on the bodice
        if (wm) {   // a woman's skirt (0.02-0.60) and bodice (from 0.56)
            auto skirtY = [](float z) { return 0.175f + (0.09f - 0.175f) * (z - 0.02f) / 0.58f; };
            auto bodyY = [](float z) { return 0.076f + (0.097f - 0.076f) * (z - 0.56f) / 0.15f; };
            panel(base, 0.10f, 0.60f, skirtY(0.10f), skirtY(0.60f), 0.135f, 0.105f);
            panel(ub, 0.60f, 0.71f, bodyY(0.60f), bodyY(0.71f), 0.08f, 0.075f);
        } else {
            auto skirtY = [](float z) { return 0.165f + (0.104f - 0.165f) * (z - 0.02f) / 0.51f; };
            auto bodyY = [](float z) { return 0.082f + (0.093f - 0.082f) * (z - 0.5f) / 0.3f; };
            panel(base, 0.12f, 0.53f, skirtY(0.12f), skirtY(0.53f), 0.135f, 0.105f);
            panel(ub, 0.53f, 0.72f, bodyY(0.53f), bodyY(0.72f), 0.08f, 0.075f);
        }
    } else if (f.apronOn) {   // a board in front, from the thighs (a gown's knees) to the chest
        float a0 = longGown ? 0.12f : 0.3f;
        if (st > 0) {   // over a paunch: hanging straight down in front of it and of the coat's skirts, and lying on it above
            float yb = std::max(pf(0.56f), 0.108f + 2 * ca) + 0.006f, w = 0.15f - 0.03f * (0.56f - a0) / (0.74f - a0);
            panel(base, a0, 0.56f, yb, yb, 0.15f, w);
            panel(ub, 0.56f, 0.74f, yb, 0.093f, w, 0.12f);
        } else if (lean > 0) {   // above the waist it leans with the body
            float k = (0.5f - a0) / (0.74f - a0), rw = 0.15f - 0.03f * k, rd = 0.012f - 0.002f * k;
            ofrustum(m, base, a0, 0.5f, 0.15f, 0.012f, rw, rd, 6, f.apron, 0, 0.095f, true, false);
            ofrustum(m, ub, 0.5f, 0.74f, rw, rd, 0.12f, 0.01f, 6, f.apron, 0, 0.095f, false, true);
        } else ofrustum(m, base, a0, 0.74f, 0.15f, 0.012f, 0.12f, 0.01f, 6, f.apron, 0, 0.095f);
    }
    if (f.sashOn) {   // round the waist, over the coat (and out over a paunch), and its ends hanging at the left hip
        float sa = std::max(0.0f, (pf(0.56f) + 0.006f - 0.092f) * 0.5f);
        ofrustum(m, base, 0.52f, 0.6f, 0.142f + 0.009f * st, 0.092f + sa, 0.147f + 0.009f * st, 0.094f + sa, 12, f.sash, 0, sa, false, false);
        ofrustum(m, base, 0.36f, 0.55f, 0.016f, 0.03f, 0.02f, 0.034f, 6, f.sash * 0.85f, -0.15f - 0.01f * st, 0.0f);
    }
    // the falling collar (a monk's dark; a woman's broad, over the shoulders), and the neck
    if (wm) ofrustum(m, ub, 0.785f, 0.865f, 0.17f, 0.11f, 0.072f, 0.062f, 14, white);
    else ofrustum(m, ub, 0.80f, 0.865f, 0.135f, 0.098f, 0.072f, 0.062f, 14, f.robe ? f.coat * 0.6f : white);
    ofrustum(m, ub, 0.84f, 0.89f, 0.036f, 0.036f, 0.034f, 0.034f, 8, f.skin);
    // the arms: shoulder, upper arm, elbow, forearm, cuff, hand; the shoulder tucked under the slope of the body
    float raise = f.strike > 0 ? std::sin(std::min(1.0f, f.strike) * PI_F) * 2.0f : 0.0f;
    vec3 wood(0.40f, 0.27f, 0.15f), steel(0.72f, 0.74f, 0.78f), iron(0.25f, 0.25f, 0.27f);
    for (int s = -1; s <= 1; s += 2) {
        float a = -s * swing * 0.7f;
        bool holding = (s > 0 && f.weapon) || f.carry;
        if (s > 0 && f.weapon) a = f.weapon == 1 || f.weapon == 2 ? -0.5f : -0.25f;
        if (f.carry == 1) a = 1.15f;
        if (f.carry == 2) a = -0.15f;
        if (f.carry == 3 && s > 0) a = 0.55f;
        if (f.carry == 4) a = 0.7f;
        if (sitting && !holding) a = 0.3f;   // seated: the hands in the lap
        if (s > 0) a -= raise;
        a += lean;   // leaning, the arms still hang plumb
        vec3 P(s * 0.165f * (wm ? 0.93f : 1.0f), 0, 0.785f);   // the shoulder
        mat4 A = ub * translate(P) * rotX(a);
        float eb = holding || lying ? 0.0f : 0.22f + 0.35f * std::max(0.0f, a);   // the elbow, a little bent
        mat4 F = A * translate(vec3(0, 0, -0.17f)) * rotX(eb);
        vec3 T;
        if (!lying && reachTarget(f, s, sitting, holding, T)) {   // the hand goes to what it holds
            mat4 Al, Fl;
            armReach(P, T, vec3(s * 0.7f, -0.3f, -1.0f), 0.17f, 0.205f, Al, Fl);
            A = ub * Al; F = ub * Fl;
        }
        oellip(m, A, vec3(0, 0, -0.015f), vec3(0.05f, 0.053f, 0.042f), 8, 4, f.coat);
        ofrustum(m, A, -0.17f, 0.0f, 0.042f, 0.044f, 0.05f, 0.052f, 8, f.coat);
        if (!gLo) oellip(m, F, vec3(0, 0, 0), vec3(0.044f, 0.046f, 0.04f), 8, 3, f.coat);   // the elbow
        ofrustum(m, F, -0.165f, 0.0f, 0.036f, 0.038f, 0.043f, 0.045f, 8, f.coat);
        ofrustum(m, F, -0.175f, -0.15f, 0.044f, 0.046f, 0.043f, 0.045f, 8, f.robe ? f.coat * 0.85f : white, 0, 0, false, false);   // the cuff
        oellip(m, F, vec3(0, 0.006f, -0.205f), vec3(0.03f, 0.034f, 0.04f), 8, 4, f.skin);   // the hand
        if (f.carry == 3 && s > 0) {   // the cap, held by its brim, hanging from the hand (it was a ball: "a lump")
            mat4 C = F * translate(vec3(0, 0.03f, -0.265f)) * rotX(PI_F / 2);
            ofrustum(m, C, -0.007f, 0.007f, 0.082f, 0.082f, 0.082f, 0.082f, 14, f.hat);   // the brim
            oellip(m, C, vec3(0, 0, 0.007f), vec3(0.052f, 0.052f, 0.04f), 10, 4, f.hat * 0.9f, 0, PI_F / 2);   // the crown
        }
        if (s < 0 || !f.weapon) continue;
        mat4 H = F * translate(vec3(0, 0, -0.21f));
        switch (f.weapon) {
            case 1:
                obox(m, H, vec3(-0.025f, -0.18f, -0.04f), vec3(0.025f, 0.12f, 0.03f), wood);
                ofrustum(m, H * translate(vec3(0, 0, -0.005f)) * rotX(-PI_F / 2), -1.05f, -0.12f, 0.014f, 0.014f, 0.016f, 0.016f, 6, iron);
                break;
            case 2:
                ofrustum(m, H * rotX(-PI_F / 2), -2.6f, 0.6f, 0.02f, 0.02f, 0.022f, 0.022f, 6, wood);
                ofrustum(m, H * rotX(-PI_F / 2), -2.85f, -2.6f, 0.002f, 0.002f, 0.03f, 0.012f, 4, steel);
                break;
            case 3:
                obox(m, H, vec3(-0.015f, -0.02f, -0.06f), vec3(0.015f, 0.02f, 0.04f), wood);
                obox(m, H, vec3(-0.07f, -0.015f, -0.08f), vec3(0.07f, 0.015f, -0.06f), steel);
                obox(m, H, vec3(-0.012f, -0.006f, -0.72f), vec3(0.012f, 0.006f, -0.08f), steel);
                break;
            case 4:
                obox(m, H, vec3(-0.015f, -0.02f, -0.06f), vec3(0.015f, 0.02f, 0.04f), wood);
                obox(m, H, vec3(-0.05f, -0.015f, -0.08f), vec3(0.05f, 0.015f, -0.06f), steel);
                obox(m, H, vec3(-0.014f, -0.006f, -0.42f), vec3(0.014f, 0.006f, -0.08f), steel);
                obox(m, H, vec3(-0.014f, 0.0f, -0.66f), vec3(0.014f, 0.08f, -0.42f), steel);
                break;
        }
    }
    if (f.halberd) {   // its staff through the right hand
        mat4 A = ub * translate(vec3(0.2f, 0.07f, 0));
        ofrustum(m, A, 0.0f, 1.7f, 0.02f, 0.02f, 0.018f, 0.018f, 6, vec3(0.4f, 0.28f, 0.16f));
        obox(m, A, vec3(-0.012f, -0.13f, 1.45f), vec3(0.012f, 0.04f, 1.62f), vec3(0.7f, 0.72f, 0.75f));
        ofrustum(m, A, 1.62f, 1.86f, 0.012f, 0.012f, 0.001f, 0.001f, 4, vec3(0.7f, 0.72f, 0.75f));
    }
    if (f.lute) {
        mat4 A = ub * translate(vec3(-0.05f, 0.12f, 0.62f)) * rotateZ(0.3f);
        oellip(m, A, vec3(-0.01f, 0, -0.01f), vec3(0.12f, 0.045f, 0.11f), 10, 5, vec3(0.62f, 0.4f, 0.2f), -PI_F / 2, PI_F / 2);
        oellip(m, A, vec3(-0.01f, 0.044f, -0.01f), vec3(0.025f, 0.002f, 0.025f), 8, 2, vec3(0.2f, 0.12f, 0.06f));   // the rose
        obox(m, A, vec3(0.1f, -0.02f, -0.02f), vec3(0.36f, 0.02f, 0.02f), vec3(0.35f, 0.22f, 0.12f));
        obox(m, A, vec3(0.34f, -0.025f, -0.03f), vec3(0.42f, 0.025f, 0.03f), vec3(0.3f, 0.18f, 0.1f));   // the pegbox
    }
    if (f.carry == 4) {   // a little book of sewn pages, held out in both hands by its edges
        obox(m, ub, vec3(-0.075f, 0.19f, 0.53f), vec3(0.075f, 0.29f, 0.565f), vec3(0.45f, 0.3f, 0.17f));
        obox(m, ub, vec3(-0.068f, 0.195f, 0.565f), vec3(0.068f, 0.285f, 0.575f), vec3(0.93f, 0.9f, 0.8f));
    }
    if (f.carry == 1) {   // a swaddled baby across the arms
        oellip(m, ub, vec3(-0.02f, 0.23f, 0.655f), vec3(0.11f, 0.07f, 0.055f), 10, 5, vec3(0.92f, 0.9f, 0.84f));
        oellip(m, ub, vec3(0.12f, 0.23f, 0.665f), vec3(0.045f, 0.045f, 0.045f), 8, 5, f.babySkin);
        oellip(m, ub, vec3(0.125f, 0.225f, 0.685f), vec3(0.048f, 0.048f, 0.035f), 8, 3, vec3(0.92f, 0.9f, 0.84f), 0, PI_F / 2);
    }
    if (f.carry == 2) {   // a yoke across the shoulders behind the neck; its ropes drawn forward by the hands, the pails hanging from them
        vec3 wood(0.45f, 0.32f, 0.18f), pail(0.62f, 0.5f, 0.34f), rope(0.55f, 0.48f, 0.36f);
        ofrustum(m, ub * translate(vec3(0, -0.075f, 0.855f)) * rotateZ(PI_F / 2) * rotX(PI_F / 2), -0.34f, 0.34f, 0.022f, 0.02f, 0.022f, 0.02f, 6, wood);
        for (int s = -1; s <= 1; s += 2) {
            vec3 top(s * 0.30f, -0.075f, 0.845f), hand(s * 0.24f, 0.16f, 0.62f), d = hand - top;
            ofrustum(m, ub * boneFrame(normalize(d), top), -length(d), 0.0f, 0.008f, 0.008f, 0.008f, 0.008f, 4, rope);
            ofrustum(m, ub * translate(vec3(hand.x, hand.y, 0)), 0.55f, 0.62f, 0.008f, 0.008f, 0.008f, 0.008f, 4, rope);
            ofrustum(m, ub * translate(vec3(hand.x, hand.y, 0)), 0.36f, 0.56f, 0.065f, 0.065f, 0.08f, 0.08f, 10, pail);
            ofrustum(m, ub * translate(vec3(hand.x, hand.y, 0)), 0.40f, 0.42f, 0.071f, 0.071f, 0.072f, 0.072f, 10, pail * 0.6f, 0, 0, false, false);
        }
    }
    // the head (larger for a small child's size, about the neck; lifted again when the body leans), the face, the hair
    mat4 hb = ub * translate(vec3(0, 0, 0.86f)) * rotX(0.6f * lean) * scaleM(f.headK) * translate(vec3(0, 0, -0.86f));
    if (f.asshead) { assHead(m, hb, f.hatKind == 6, f.hat); return; }   // the ass's head over all of it (the hats come last)
    oellip(m, hb, vec3(0, 0.004f, 0.945f), vec3(0.07f, 0.077f, 0.086f), 12, 8, f.skin);
    for (int s = -1; s <= 1 && !gLo; s += 2) {
        oellip(m, hb, vec3(s * 0.069f, -0.004f, 0.94f), vec3(0.012f, 0.021f, 0.028f), 6, 3, f.skin * 0.94f);   // the ears
        // the eyes and the brows: flattened, half sunk in the face where it curves (flat boxes stood out
        // from the round head at their outer ends, like the rims of goggles)
        oellip(m, hb, vec3(s * 0.026f, 0.0735f, 0.955f), vec3(0.0085f, 0.004f, 0.0065f), 6, 3, dark);   // the eyes
        oellip(m, hb, vec3(s * 0.027f, 0.0705f, 0.97f), vec3(0.014f, 0.004f, 0.0032f), 6, 3, f.statue ? dark : hair * 0.8f);   // the brows
    }
    if (!gLo) obox(m, hb, vec3(-0.01f, 0.074f, 0.918f), vec3(0.01f, 0.094f, 0.952f), f.skin * 0.96f);   // the nose
    if (!gLo) obox(m, hb, vec3(-0.021f, 0.069f, 0.894f), vec3(0.021f, 0.078f, 0.902f), f.statue ? dark : vec3(0.62f, 0.32f, 0.3f) * (0.4f + 0.6f * f.skin.x));   // the mouth
    // hair: a cap over the top and the back of the head, full enough on top that the scalp does not show through
    // (old and bareheaded, a man is thin on top)
    bool bald = f.age > 0.6f && !f.woman && f.hatKind == 0;
    oellip(m, hb, vec3(0, -0.009f, 0.962f), vec3(0.077f, 0.086f, 0.078f), 12, bald ? 2 : 4, hair, -0.05f, bald ? 0.6f : PI_F / 2);
    oellip(m, hb, vec3(0, -0.03f, 0.93f), vec3(0.074f, 0.062f, 0.062f), 10, 5, hair, -0.9f, 0.9f);   // down the back of the head
    if (f.longHair) oellip(m, hb, vec3(0, -0.058f, 0.86f), vec3(0.08f, 0.04f, 0.115f), 10, 5, hair);
    vec3 beardc = hair * 0.85f;
    if (f.beard >= 1 && !gLo)   // a moustache: two wings over the mouth
        for (int s = -1; s <= 1; s += 2)
            oellip(m, hb * translate(vec3(s * 0.016f, 0.079f, 0.907f)) * rotateZ(-s * 0.15f), vec3(0, 0, 0), vec3(0.018f, 0.0065f, 0.006f), 6, 3, beardc);
    if (f.beard == 1)   // a pointed tuft from under the lip
        ofrustum(m, hb * translate(vec3(0, 0.066f, 0.888f)) * rotX(0.35f), -0.045f, 0.0f, 0.003f, 0.003f, 0.017f, 0.012f, 6, beardc);
    if (f.beard == 2)   // a full beard round the jaw and the chin
        oellip(m, hb, vec3(0, 0.032f, 0.866f), vec3(0.066f, 0.054f, 0.045f), 10, 5, beardc, -PI_F / 2, 0.55f);
    if (f.fringe) {   // the fringe a band of the head's own curve over the forehead; the locks at the sides tapering to
                      // the jaw (short hair) or the shoulders (long)
        oellip(m, hb, vec3(0, 0.0f, 0.952f), vec3(0.079f, 0.087f, 0.088f), 10, 3, hair, 0.18f, 0.95f, PI_F / 2 - 1.05f, PI_F / 2 + 1.05f);
        float low = f.longHair ? 0.76f : 0.885f;
        for (int s = -1; s <= 1; s += 2)
            ofrustum(m, hb, low, 1.0f, 0.012f, 0.03f, 0.022f, 0.06f, 8, hair, s * 0.07f, -0.012f, true, false);
    }
    mat4 hl = hb * translate(vec3(0, 0, -0.025f));   // a brimmed hat or a helmet sits down on the head
    switch (f.hatKind) {
        case 1:   // a broad-brimmed hat, a round crown and a band
            ofrustum(m, hl, 1.02f, 1.042f, 0.178f, 0.178f, 0.175f, 0.175f, 16, f.hat);
            ofrustum(m, hl, 1.04f, 1.16f, 0.084f, 0.084f, 0.072f, 0.072f, 12, f.hat);
            if (!gLo) ofrustum(m, hl, 1.05f, 1.076f, 0.087f, 0.087f, 0.085f, 0.085f, 12, vec3(0.7f, 0.6f, 0.3f), 0, 0, false, false);
            break;
        case 2:   // a knitted cap
            oellip(m, hb, vec3(0, 0.0f, 0.995f), vec3(0.083f, 0.089f, 0.07f), 12, 4, f.hat, 0.0f, PI_F / 2);
            ofrustum(m, hb, 0.985f, 1.01f, 0.085f, 0.091f, 0.085f, 0.091f, 12, f.hat * 0.85f, 0, 0, false, false);
            break;
        case 3: {   // a white coif, worn back on the head and over the ears: open at the face, a band of her own hair
                    // showing under its front edge; a little fuller than the hair under it, so that its edge is clean.
                    // On a man it is a turban, round the whole head
            if (!f.woman) { oellip(m, hb, vec3(0, -0.012f, 0.955f), vec3(0.082f, 0.087f, 0.095f), 12, 6, f.hat, -0.45f, PI_F / 2); break; }
            mat4 hc = hb * translate(vec3(0, -0.012f, 0.955f)) * rotX(0.5f) * translate(vec3(0, 0.012f, -0.955f));
            const float w = 0.9f, lt = 0.3f;
            vec3 cr = vec3(0.082f, 0.087f, 0.095f) * 1.1f;
            oellip(m, hc, vec3(0, -0.012f, 0.955f), cr, 12, 3, f.hat, lt, PI_F / 2);
            oellip(m, hc, vec3(0, -0.012f, 0.955f), cr, 10, 3, f.hat, -0.45f, lt, PI_F / 2 + w, PI_F / 2 - w + 2 * PI_F);
            oellip(m, hc, vec3(0, 0.004f, 0.952f), vec3(0.075f, 0.082f, 0.09f), 8, 2, hair, 0.12f, lt + 0.08f, PI_F / 2 - w - 0.05f, PI_F / 2 + w + 0.05f);
            break;
        }
        case 4:   // a morion: a brim turned up front and back, a round crown, a comb
            ofrustum(m, hl, 1.01f, 1.035f, 0.15f, 0.12f, 0.145f, 0.115f, 16, f.hat);
            oellip(m, hl, vec3(0, 0, 1.03f), vec3(0.082f, 0.085f, 0.09f), 12, 4, f.hat, 0.0f, PI_F / 2);
            obox(m, hl, vec3(-0.01f, -0.085f, 1.07f), vec3(0.01f, 0.085f, 1.18f), f.hat * 0.9f);
            break;
        case 5:   // a square scholar's cap
            obox(m, hb, vec3(-0.08f, -0.08f, 1.01f), vec3(0.08f, 0.08f, 1.06f), f.hat);
            obox(m, hb, vec3(-0.12f, -0.12f, 1.06f), vec3(0.12f, 0.12f, 1.08f), f.hat);
            break;
        case 6:   // a hood, the face showing
            oellip(m, hb, vec3(0, -0.03f, 0.95f), vec3(0.094f, 0.1f, 0.12f), 12, 6, f.hat, -0.7f, PI_F / 2);
            break;
        case 7:   // a biretta, and its tuft
            obox(m, hb, vec3(-0.08f, -0.08f, 1.01f), vec3(0.08f, 0.08f, 1.1f), f.hat);
            oellip(m, hb, vec3(0, 0, 1.115f), vec3(0.022f, 0.022f, 0.02f), 6, 3, f.hat);
            break;
        default:
            break;
    }
}

// ------------------------------------------------------------------ animals

// an animal, facing +y before it is turned to `facing`; phase: the walk; headDown 0..1: grazing,
// or pecking; lying: asleep (a bird sits)
void buildAnimal(MeshBuf &m, const AnimalLook &a, vec3 pos, float facing, float phase, float headDown, bool lying) {
    if (gFine || gMid) { gLo = !gFine; buildAnimalFine(m, a, pos, facing, phase, headDown, lying); gLo = false; return; }
    mat4 base = translate(pos) * rotateZ(facing);
    vec3 col = a.col, headc = a.head.x >= 0 ? a.head : col, dark = col * 0.55f;
    float sw = std::sin(phase * 2 * PI_F) * 0.45f;
    if (a.bird) {
        float L = a.len, H = a.ht, legH = lying ? 0.0f : H * 0.38f, bz = legH + (lying ? 0.02f : 0.0f);
        obox(m, base, vec3(-L * 0.28f, -L * 0.45f, bz), vec3(L * 0.28f, L * 0.4f, bz + H * 0.42f), col);
        obox(m, base, vec3(-L * 0.18f, -L * 0.6f, bz + H * 0.18f), vec3(L * 0.18f, -L * 0.42f, bz + H * 0.48f), col * 0.92f);   // the tail
        float nh = a.longNeck ? H * 0.55f : H * 0.2f;
        mat4 N = base * translate(vec3(0, L * 0.34f, bz + H * 0.32f)) * rotX(-headDown * 1.1f);   // pecking: leans forward
        obox(m, N, vec3(-L * 0.07f, -L * 0.06f, 0), vec3(L * 0.07f, L * 0.08f, nh), col);
        obox(m, N, vec3(-L * 0.1f, -L * 0.06f, nh), vec3(L * 0.1f, L * 0.16f, nh + H * 0.17f), headc);
        obox(m, N, vec3(-L * 0.045f, L * 0.16f, nh + H * 0.04f), vec3(L * 0.045f, L * 0.3f, nh + H * 0.1f), vec3(0.92f, 0.56f, 0.14f));   // the beak
        if (a.comb) obox(m, N, vec3(-0.015f, -L * 0.02f, nh + H * 0.17f), vec3(0.015f, L * 0.12f, nh + H * 0.29f), vec3(0.8f, 0.12f, 0.1f));
        if (!lying)
            for (int s = -1; s <= 1; s += 2) {
                mat4 Lg = base * translate(vec3(s * L * 0.12f, 0, legH)) * rotX(sw * s);
                obox(m, Lg, vec3(-0.02f, -0.02f, -legH), vec3(0.02f, 0.02f, 0.02f), vec3(0.88f, 0.52f, 0.15f));
                obox(m, Lg, vec3(-0.05f, -0.02f, -legH), vec3(0.05f, 0.08f, -legH + 0.02f), vec3(0.88f, 0.52f, 0.15f));
            }
        return;
    }
    float L = a.len, H = a.ht, W = L * (a.snout ? 0.42f : 0.33f);
    float legH = lying ? 0.0f : H * 0.55f, bodyH = H * 0.45f, bz = lying ? 0.03f : legH;
    // the body (a sheep's in its fleece), and patches on it
    if (a.wool) obox(m, base, vec3(-W * 0.62f, -L * 0.5f, bz - bodyH * 0.12f), vec3(W * 0.62f, L * 0.44f, bz + bodyH * 1.12f), col);
    else obox(m, base, vec3(-W * 0.5f, -L * 0.5f, bz), vec3(W * 0.5f, L * 0.44f, bz + bodyH), col);
    if (a.patch.x >= 0) {
        uint32_t h = hash32((uint32_t)(col.x * 1000 + col.y * 100));
        float o = (h % 7) * 0.03f;
        obox(m, base, vec3(-W * 0.51f, -L * (0.28f - o), bz + bodyH * 0.3f), vec3(W * 0.51f, L * (0.02f + o), bz + bodyH * 0.92f), a.patch);
        obox(m, base, vec3(-W * 0.51f, L * 0.18f, bz + bodyH * 0.12f), vec3(W * 0.51f, L * 0.36f, bz + bodyH * 0.6f), a.patch);
    }
    if (a.saddle) {   // a red saddle cloth, and the saddle on it
        obox(m, base, vec3(-W * 0.54f, -L * 0.14f, bz + bodyH * 0.45f), vec3(W * 0.54f, L * 0.16f, bz + bodyH * 1.01f), vec3(0.55f, 0.12f, 0.1f));
        obox(m, base, vec3(-W * 0.32f, -L * 0.12f, bz + bodyH), vec3(W * 0.32f, L * 0.13f, bz + bodyH + 0.09f), vec3(0.32f, 0.2f, 0.11f));
        obox(m, base, vec3(-W * 0.32f, L * 0.08f, bz + bodyH), vec3(W * 0.32f, L * 0.13f, bz + bodyH + 0.17f), vec3(0.32f, 0.2f, 0.11f));   // the pommel
    }
    // the legs: diagonal pairs swing together; folded under it when it lies down
    float lw = std::max(0.035f, W * 0.13f);
    vec3 legc = a.wool ? headc : col * 0.88f;
    for (int fr = -1; fr <= 1; fr += 2)
        for (int sd = -1; sd <= 1; sd += 2) {
            if (lying) { obox(m, base, vec3(sd * W * 0.45f - lw, fr * L * 0.3f - lw * 2, 0), vec3(sd * W * 0.45f + lw, fr * L * 0.3f + lw * 2, lw * 1.5f), legc); continue; }
            mat4 Lg = base * translate(vec3(sd * W * 0.32f, fr * L * 0.36f, legH)) * rotX(sw * fr * sd);
            obox(m, Lg, vec3(-lw, -lw, -legH), vec3(lw, lw, 0.03f), legc);
            obox(m, Lg, vec3(-lw * 1.08f, -lw * 1.1f, -legH), vec3(lw * 1.08f, lw * 1.2f, -legH + legH * 0.12f), dark * 0.6f);   // the hoof
        }
    // the tail: hanging, or up (a cat's, a dog's)
    {
        mat4 T = base * translate(vec3(0, -L * 0.5f, bz + bodyH * 0.85f));
        if (a.tailUp) obox(m, T * rotX(-0.45f), vec3(-0.025f, -0.035f, 0), vec3(0.025f, 0.02f, H * 0.55f), col);
        else obox(m, T * rotX(0.12f), vec3(-0.03f, -0.05f, -bodyH * 1.1f), vec3(0.03f, 0.0f, 0), a.mane ? dark * 0.7f : col * 0.9f);
    }
    // the neck and the head, up and forward; down to the grass when it grazes
    float up = a.mane ? 0.8f : a.snout ? 0.05f : a.ears ? 0.6f : 0.3f;
    float th = lying ? up * 0.4f : up - headDown * (up + 0.85f);
    float nl = a.mane ? L * 0.42f : a.snout ? L * 0.08f : a.wool ? L * 0.18f : L * 0.24f, nw = W * (a.mane ? 0.32f : 0.4f);
    mat4 N = base * translate(vec3(0, L * 0.38f, bz + bodyH * (a.snout ? 0.45f : 0.7f))) * rotX(th);
    obox(m, N, vec3(-nw, 0, -bodyH * 0.3f), vec3(nw, nl, bodyH * 0.3f), a.wool ? headc : col);
    if (a.mane) obox(m, N, vec3(-nw * 0.4f, -0.02f, bodyH * 0.25f), vec3(nw * 0.4f, nl, bodyH * 0.42f), dark * 0.7f);
    float hl = a.snout ? L * 0.26f : a.mane ? L * 0.3f : a.ears ? L * 0.24f : L * 0.27f, hw = W * (a.snout ? 0.38f : 0.34f), hh = bodyH * (a.snout ? 0.55f : 0.5f);
    mat4 Hd = N * translate(vec3(0, nl, 0)) * rotX(-th * 0.6f - (a.mane ? 0.5f : 0.15f));
    obox(m, Hd, vec3(-hw, 0, -hh * 0.5f), vec3(hw, hl, hh * 0.5f), headc);
    if (a.snout) obox(m, Hd, vec3(-hw * 0.6f, hl, -hh * 0.35f), vec3(hw * 0.6f, hl + 0.06f, hh * 0.25f), headc * 1.1f);   // the pig's snout
    else obox(m, Hd, vec3(-hw * 0.75f, hl * 0.7f, -hh * 0.55f), vec3(hw * 0.75f, hl * 1.02f, hh * 0.1f), headc * 0.8f);   // the muzzle
    if (a.horns)
        for (int s = -1; s <= 1; s += 2)
            obox(m, Hd * translate(vec3(s * hw * 0.7f, hl * 0.15f, hh * 0.5f)) * rotX(-0.5f), vec3(-0.02f, -0.02f, 0), vec3(0.02f, 0.02f, hh * 0.7f), vec3(0.86f, 0.82f, 0.7f));
    if (a.antlers)   // a stag's: a beam back and out, with two tines forward
        for (int s = -1; s <= 1; s += 2) {
            vec3 ac(0.62f, 0.52f, 0.38f);
            mat4 A = Hd * translate(vec3(s * hw * 0.55f, hl * 0.12f, hh * 0.5f)) * rotateZ(s * 0.5f) * rotX(-0.4f);
            obox(m, A, vec3(-0.016f, -0.016f, 0), vec3(0.016f, 0.016f, 0.34f), ac);
            obox(m, A * translate(vec3(0, 0, 0.1f)) * rotX(1.0f), vec3(-0.012f, -0.012f, 0), vec3(0.012f, 0.012f, 0.13f), ac);
            obox(m, A * translate(vec3(0, 0, 0.22f)) * rotX(0.85f), vec3(-0.011f, -0.011f, 0), vec3(0.011f, 0.011f, 0.11f), ac);
        }
    if (a.ears || a.mane || a.snout)
        for (int s = -1; s <= 1; s += 2)
            obox(m, Hd, vec3(s * hw * 0.55f - 0.025f, hl * 0.05f, hh * 0.45f), vec3(s * hw * 0.55f + 0.025f, hl * 0.18f, hh * 0.45f + (a.ears ? 0.09f : 0.06f)), headc * 0.9f);
}

static void buildAnimalFine(MeshBuf &m, const AnimalLook &a, vec3 pos, float facing, float phase, float headDown, bool lying) {
    // the same animal as buildAnimal, part for part, only rounder: a barrel of a body, jointed legs,
    // a shaped head with a muzzle, eyes and ears, tapered horns and tail
    mat4 base = translate(pos) * rotateZ(facing);
    vec3 col = a.col, headc = a.head.x >= 0 ? a.head : col, dark = col * 0.55f, eye(0.08f, 0.06f, 0.05f);
    float sw = std::sin(phase * 2 * PI_F) * 0.45f;
    const float HALF = PI_F / 2;
    if (a.bird) {
        float L = a.len, H = a.ht, legH = lying ? 0.0f : H * 0.38f, bz = legH + (lying ? 0.02f : 0.0f);
        oellip(m, base, vec3(0, -L * 0.03f, bz + H * 0.22f), vec3(L * 0.28f, L * 0.45f, H * 0.23f), 10, 6, col);
        obox(m, base * translate(vec3(0, -L * 0.42f, bz + H * 0.3f)) * rotX(-0.5f), vec3(-L * 0.16f, -L * 0.16f, -H * 0.05f), vec3(L * 0.16f, L * 0.02f, H * 0.1f), col * 0.92f);   // the tail
        float nh = a.longNeck ? H * 0.55f : H * 0.2f;
        mat4 N = base * translate(vec3(0, L * 0.34f, bz + H * 0.32f)) * rotX(-headDown * 1.1f);
        ofrustum(m, N, 0.0f, nh, L * 0.075f, L * 0.08f, L * 0.06f, L * 0.065f, 8, col);
        oellip(m, N, vec3(0, L * 0.04f, nh + H * 0.085f), vec3(L * 0.1f, L * 0.115f, H * 0.095f), 8, 5, headc);
        ofrustum(m, N * translate(vec3(0, L * 0.13f, nh + H * 0.07f)) * rotX(-HALF), 0.0f, L * 0.16f, L * 0.045f, H * 0.03f, 0.004f, 0.004f, 6, vec3(0.92f, 0.56f, 0.14f));   // the beak
        for (int s = -1; s <= 1; s += 2) oellip(m, N, vec3(s * L * 0.085f, L * 0.09f, nh + H * 0.11f), vec3(0.012f, 0.012f, 0.012f), 5, 3, eye);
        if (a.comb) obox(m, N, vec3(-0.012f, -L * 0.02f, nh + H * 0.16f), vec3(0.012f, L * 0.12f, nh + H * 0.29f), vec3(0.8f, 0.12f, 0.1f));
        if (!lying)
            for (int s = -1; s <= 1; s += 2) {
                mat4 Lg = base * translate(vec3(s * L * 0.12f, 0, legH)) * rotX(sw * s);
                ofrustum(m, Lg, -legH, 0.02f, 0.014f, 0.014f, 0.02f, 0.02f, 6, vec3(0.88f, 0.52f, 0.15f));
                for (int t = -1; t <= 1; t++)   // three toes forward
                    obox(m, Lg * translate(vec3(0, 0, -legH)) * rotateZ(t * 0.45f), vec3(-0.008f, -0.01f, 0), vec3(0.008f, 0.07f, 0.012f), vec3(0.88f, 0.52f, 0.15f));
            }
        return;
    }
    float L = a.len, H = a.ht, W = L * (a.snout ? 0.42f : 0.33f);
    float legH = lying ? 0.0f : H * 0.55f, bodyH = H * 0.45f, bz = lying ? 0.03f : legH;
    // the body: a barrel; a sheep's in its fleece, in lumps
    if (a.wool) {
        oellip(m, base, vec3(0, -L * 0.03f, bz + bodyH * 0.5f), vec3(W * 0.64f, L * 0.5f, bodyH * 0.66f), 12, 6, col);
        for (int k = 0; k < 6; k++) {
            float y = -L * 0.32f + k * L * 0.13f, x = (k % 2 ? 1 : -1) * W * 0.18f;
            oellip(m, base, vec3(x, y, bz + bodyH * (0.8f + 0.08f * (k % 3))), vec3(W * 0.42f, L * 0.16f, bodyH * 0.38f), 8, 4, col * (0.95f + 0.05f * (k % 2)));
        }
    } else {
        osuper(m, base, vec3(0, -L * 0.03f, bz + bodyH * 0.52f), vec3(W * 0.53f, L * 0.486f, bodyH * 0.58f), 14, 7, col, 0.7f);   // a full barrel, round at the chest and rump
    }
    if (a.patch.x >= 0 && !a.wool) {   // patches: pieces of the body's own surface, a hair outside it
        uint32_t h = hash32((uint32_t)(col.x * 1000 + col.y * 100));
        float o = (h % 7) * 0.06f;
        vec3 c(0, -L * 0.03f, bz + bodyH * 0.52f), r(W * 0.53f * 1.012f, L * 0.486f * 1.012f, bodyH * 0.58f * 1.012f);
        for (int sd = 0; sd < 2; sd++) {
            float mid = sd ? PI_F : 0, dir = sd ? -1.0f : 1.0f;   // lon 0: the right flank, pi: the left
            osuper(m, base, c, r, 5, 4, a.patch, 0.7f, -0.15f, 0.95f, mid - dir * (0.55f + o), mid + dir * 0.15f);
            osuper(m, base, c, r, 3, 3, a.patch, 0.7f, -0.6f, 0.25f, mid + dir * 0.75f, mid + dir * (1.15f - o * 0.5f));
        }
    }
    if (a.saddle) {
        obox(m, base, vec3(-W * 0.56f, -L * 0.14f, bz + bodyH * 0.45f), vec3(W * 0.56f, L * 0.16f, bz + bodyH * 1.06f), vec3(0.55f, 0.12f, 0.1f));
        obox(m, base, vec3(-W * 0.32f, -L * 0.12f, bz + bodyH * 1.05f), vec3(W * 0.32f, L * 0.13f, bz + bodyH * 1.05f + 0.09f), vec3(0.32f, 0.2f, 0.11f));
        obox(m, base, vec3(-W * 0.32f, L * 0.08f, bz + bodyH * 1.05f), vec3(W * 0.32f, L * 0.13f, bz + bodyH * 1.05f + 0.17f), vec3(0.32f, 0.2f, 0.11f));
    }
    // the legs: thigh, knee (or hock), shank and hoof; diagonal pairs swing together
    float lw = std::max(0.035f, W * 0.13f);
    vec3 legc = a.wool ? headc : col * 0.88f;
    for (int fr = -1; fr <= 1; fr += 2)
        for (int sd = -1; sd <= 1; sd += 2) {
            if (lying) {
                oellip(m, base, vec3(sd * W * 0.45f, fr * L * 0.3f + lw, lw * 0.8f), vec3(lw * 1.1f, lw * 2.6f, lw * 0.9f), 8, 4, legc);
                continue;
            }
            float ang = sw * fr * sd;
            mat4 Lg = base * translate(vec3(sd * W * 0.27f, fr * L * 0.33f, legH)) * rotX(ang);
            float up = legH * 0.48f;
            // the thigh (or the forearm), thick where it joins the body and lost inside it
            ofrustum(m, Lg, -up, bodyH * 0.35f, lw * 0.95f, lw * 1.0f, lw * 1.55f, lw * 1.85f, 8, legc);
            float kb = (fr > 0 ? 1.0f : -1.0f) * 0.5f * std::max(0.0f, -ang * fr);                                // fore knee forward, hind hock back
            mat4 K = Lg * translate(vec3(0, 0, -up)) * rotX(kb);
            oellip(m, K, vec3(0, 0, 0), vec3(lw * 0.92f, lw * 0.98f, lw * 0.9f), 8, 4, legc);
            ofrustum(m, K, -(legH - up) + legH * 0.09f, 0.0f, lw * 0.62f, lw * 0.66f, lw * 0.8f, lw * 0.85f, 8, legc);   // the shank, slender
            ofrustum(m, K, -(legH - up), -(legH - up) + legH * 0.09f, lw * 0.9f, lw * 0.98f, lw * 0.68f, lw * 0.72f, 8, dark * 0.6f);   // the hoof
        }
    // the tail
    {
        mat4 T = base * translate(vec3(0, -L * 0.5f, bz + bodyH * 0.85f));
        if (a.tailUp) {
            mat4 T1 = T * rotX(-0.45f);
            ofrustum(m, T1, 0.0f, H * 0.3f, 0.028f, 0.028f, 0.022f, 0.022f, 6, col);
            ofrustum(m, T1 * translate(vec3(0, 0, H * 0.3f)) * rotX(-0.6f), 0.0f, H * 0.25f, 0.022f, 0.022f, 0.012f, 0.012f, 6, col);
        } else if (a.mane) {   // a horse's: long and full
            ofrustum(m, T * rotX(0.25f), -bodyH * 1.2f, 0.0f, 0.06f, 0.05f, 0.035f, 0.035f, 8, dark * 0.7f);
        } else {
            ofrustum(m, T * rotX(0.12f), -bodyH * 1.1f, 0.0f, 0.015f, 0.015f, 0.025f, 0.025f, 6, col * 0.9f);
            oellip(m, T * rotX(0.12f), vec3(0, 0, -bodyH * 1.1f), vec3(0.035f, 0.035f, 0.06f), 6, 3, a.wool ? col : dark * 0.8f);   // the switch
        }
    }
    // the neck and the head, up and forward; down to the grass when it grazes
    float up = a.mane ? 0.8f : a.snout ? 0.05f : a.ears ? 0.6f : 0.3f;
    float th = lying ? up * 0.4f : up - headDown * (up + 0.85f);
    float nl = a.mane ? L * 0.42f : a.snout ? L * 0.08f : a.wool ? L * 0.18f : L * 0.24f, nw = W * (a.mane ? 0.32f : 0.4f);
    mat4 N = base * translate(vec3(0, L * 0.38f, bz + bodyH * (a.snout ? 0.45f : 0.6f))) * rotX(th);
    if (a.snout) ofrustum(m, N * rotX(-HALF), -0.1f, nl + 0.05f, nw * 1.05f, bodyH * 0.42f, nw * 0.68f, bodyH * 0.27f, 12, col, 0, 0, false, false);
    else   // from well inside the body, no wider than the body where it comes out, and closed at its back end
        ofrustum(m, N * rotX(-HALF), -0.3f, nl + 0.05f, nw * 0.8f, bodyH * 0.32f, nw * 0.68f, bodyH * 0.27f, 12, a.wool ? headc : col, 0, 0, true, false);
    if (!a.wool && !a.snout && !a.mane && !a.ears)   // a cow's dewlap, under the neck
        oellip(m, N, vec3(0, nl * 0.45f, -bodyH * 0.28f), vec3(nw * 0.5f, nl * 0.55f, bodyH * 0.22f), 8, 4, col * 0.95f);
    if (a.mane) obox(m, N, vec3(-nw * 0.25f, -0.02f, bodyH * 0.26f), vec3(nw * 0.25f, nl, bodyH * 0.44f), dark * 0.7f);
    float hl = a.snout ? L * 0.26f : a.mane ? L * 0.3f : a.ears ? L * 0.24f : L * 0.27f, hw = W * (a.snout ? 0.38f : 0.34f), hh = bodyH * (a.snout ? 0.55f : 0.5f);
    mat4 Hd = N * translate(vec3(0, nl, 0)) * rotX(-th * 0.6f - (a.mane ? 0.5f : 0.15f));
    oellip(m, Hd, vec3(0, hl * 0.42f, 0.02f * hh), vec3(hw * 1.08f, hl * 0.56f, hh * 0.58f), 12, 6, headc);
    if (a.snout) {   // the pig's snout, a disc
        ofrustum(m, Hd * rotX(-HALF), hl * 0.86f, hl + 0.06f, hw * 0.55f, hh * 0.4f, hw * 0.5f, hh * 0.36f, 10, headc * 1.1f);
        for (int s = -1; s <= 1; s += 2) oellip(m, Hd, vec3(s * hw * 0.2f, hl + 0.06f, -hh * 0.02f), vec3(0.015f, 0.006f, 0.02f), 5, 3, dark * 0.5f);
    } else {
        oellip(m, Hd, vec3(0, hl * 0.86f, -hh * 0.2f), vec3(hw * 0.74f, hl * 0.26f, hh * 0.34f), 10, 5, headc * 0.82f);   // the muzzle
        for (int s = -1; s <= 1; s += 2) oellip(m, Hd, vec3(s * hw * 0.3f, hl * 1.09f, -hh * 0.16f), vec3(0.014f, 0.006f, 0.012f), 5, 3, dark * 0.4f);   // nostrils
    }
    for (int s = -1; s <= 1; s += 2) oellip(m, Hd, vec3(s * hw * 0.86f, hl * 0.48f, hh * 0.16f), vec3(0.018f, 0.02f, 0.02f), 6, 3, eye);   // the eyes
    if (a.horns)
        for (int s = -1; s <= 1; s += 2) {
            mat4 H1 = Hd * translate(vec3(s * hw * 0.62f, hl * 0.2f, hh * 0.42f)) * rotateZ(s * 0.9f) * rotX(-0.35f);
            ofrustum(m, H1, 0.0f, hh * 0.36f, 0.024f, 0.024f, 0.018f, 0.018f, 6, vec3(0.86f, 0.82f, 0.7f));
            ofrustum(m, H1 * translate(vec3(0, 0, hh * 0.36f)) * rotX(-0.7f), 0.0f, hh * 0.32f, 0.018f, 0.018f, 0.004f, 0.004f, 6, vec3(0.8f, 0.76f, 0.64f));
        }
    if (a.antlers)   // a stag's: a beam back and out, with two tines forward
        for (int s = -1; s <= 1; s += 2) {
            vec3 ac(0.62f, 0.52f, 0.38f);
            mat4 A = Hd * translate(vec3(s * hw * 0.55f, hl * 0.15f, hh * 0.42f)) * rotateZ(s * 0.5f) * rotX(-0.4f);
            ofrustum(m, A, 0.0f, 0.34f, 0.018f, 0.018f, 0.01f, 0.01f, 6, ac);
            ofrustum(m, A * translate(vec3(0, 0, 0.1f)) * rotX(1.0f), 0.0f, 0.13f, 0.012f, 0.012f, 0.004f, 0.004f, 5, ac);
            ofrustum(m, A * translate(vec3(0, 0, 0.22f)) * rotX(0.85f), 0.0f, 0.11f, 0.011f, 0.011f, 0.004f, 0.004f, 5, ac);
        }
    if (a.ears || a.mane || a.snout)
        for (int s = -1; s <= 1; s += 2) {
            float eh = a.ears ? 0.1f : a.snout ? 0.07f : 0.08f;
            mat4 E = Hd * translate(vec3(s * hw * 0.55f, hl * 0.12f, hh * 0.42f)) * rotateZ(s * 0.25f) * rotX(a.snout ? 0.5f : -0.15f);
            ofrustum(m, E, 0.0f, eh, 0.03f, 0.016f, 0.004f, 0.004f, 5, headc * 0.9f);
        }
    else if (!a.wool)   // a cow's ears, out to the sides
        for (int s = -1; s <= 1; s += 2)
            oellip(m, Hd, vec3(s * hw * 1.05f, hl * 0.18f, hh * 0.28f), vec3(0.05f, 0.022f, 0.028f), 6, 3, headc * 0.9f);
}

// ------------------------------------------------------------------ finer furniture, close up
static bool featureFine(MeshBuf &m, int f, int var, float cx, float cy, float z, uint32_t seed) {
    // the same pieces of furniture as feature(), in the same places and sizes, only shaped:
    // turned legs, posts and pillows, staves and hoops, a lid that curves, logs in the fire
    mat4 M = translate(vec3(cx, cy, z));
    const float HALF = PI_F / 2;
    auto turnedLeg = [&](float x, float y, float h, float r, vec3 col) {   // a turned leg: foot, shaft, a ball, a neck
        ofrustum(m, M, 0.0f, 0.035f, r * 1.3f, r * 1.3f, r * 1.1f, r * 1.1f, 8, col * 0.9f, x, y);
        ofrustum(m, M, 0.035f, h * 0.45f, r, r, r * 0.8f, r * 0.8f, 8, col, x, y, false, false);
        oellip(m, M, vec3(x, y, h * 0.52f), vec3(r * 1.45f, r * 1.45f, h * 0.09f), 8, 4, col);
        ofrustum(m, M, h * 0.58f, h, r * 0.8f, r * 0.8f, r * 1.05f, r * 1.05f, 8, col, x, y, false, true);
    };
    switch (f) {
        case F_TABLE: {
            vec3 wood(0.48f, 0.32f, 0.18f);
            m.box(vec3(cx - 0.44f, cy - 0.31f, z + 0.5f), vec3(cx + 0.44f, cy + 0.31f, z + 0.56f), wood, true);   // the top
            m.box(vec3(cx - 0.37f, cy - 0.25f, z + 0.43f), vec3(cx + 0.37f, cy + 0.25f, z + 0.5f), wood * 0.8f);    // the frame under it
            for (int i = 0; i < 4; i++)
                turnedLeg((i & 1) ? 0.34f : -0.34f, (i & 2) ? 0.22f : -0.22f, 0.5f, 0.032f, wood * 0.85f);
            m.box(vec3(cx - 0.34f, cy - 0.015f, z + 0.08f), vec3(cx + 0.34f, cy + 0.015f, z + 0.11f), wood * 0.75f);   // the stretcher
            for (int sd = -1; sd <= 1; sd += 2) m.box(vec3(cx + sd * 0.34f - 0.015f, cy - 0.22f, z + 0.08f), vec3(cx + sd * 0.34f + 0.015f, cy + 0.22f, z + 0.11f), wood * 0.75f);
            return true;
        }
        case F_BENCH: {
            vec3 wood(0.5f, 0.34f, 0.2f);
            m.box(vec3(cx - 0.41f, cy - 0.15f, z + 0.3f), vec3(cx + 0.41f, cy + 0.15f, z + 0.36f), wood, true);
            for (int sd = -1; sd <= 1; sd += 2)   // splayed legs, a pair at each end
                for (int fb = -1; fb <= 1; fb += 2) {
                    mat4 L = M * translate(vec3(sd * 0.3f, fb * 0.07f, 0.3f)) * rotX(fb * 0.22f);
                    ofrustum(m, L, -0.31f, 0.0f, 0.022f, 0.022f, 0.028f, 0.028f, 6, wood * 0.85f);
                }
            m.box(vec3(cx - 0.3f, cy - 0.015f, z + 0.12f), vec3(cx + 0.3f, cy + 0.015f, z + 0.15f), wood * 0.75f);   // the stretcher
            return true;
        }
        case F_BED: {
            static const vec3 blankets[4] = {{0.55f, 0.15f, 0.12f}, {0.2f, 0.3f, 0.5f}, {0.3f, 0.45f, 0.25f}, {0.6f, 0.5f, 0.3f}};
            vec3 wood(0.45f, 0.3f, 0.17f), linen(0.92f, 0.9f, 0.85f), bl = blankets[var % 4];
            m.box(vec3(cx - 0.38f, cy - 0.45f, z + 0.12f), vec3(cx + 0.38f, cy + 0.45f, z + 0.26f), wood);          // the frame
            for (int i = 0; i < 4; i++) {   // four posts, taller at the head
                float x = (i & 1) ? 0.36f : -0.36f, y = (i & 2) ? 0.46f : -0.43f;
                turnedLeg(x, y, (i & 2) ? 0.8f : 0.42f, 0.03f, wood * 0.9f);
            }
            m.box(vec3(cx - 0.36f, cy + 0.44f, z + 0.26f), vec3(cx + 0.36f, cy + 0.48f, z + 0.68f), wood * 0.85f);   // the headboard
            m.box(vec3(cx - 0.355f, cy - 0.43f, z + 0.26f), vec3(cx + 0.355f, cy + 0.44f, z + 0.33f), linen * 0.96f);   // the mattress
            m.box(vec3(cx - 0.365f, cy - 0.44f, z + 0.3f), vec3(cx + 0.365f, cy + 0.15f, z + 0.355f), bl);             // the blanket
            for (int sd = -1; sd <= 1; sd += 2)   // hanging down the sides, and the foot
                m.box(vec3(cx + sd * 0.365f - (sd > 0 ? 0.0f : 0.012f), cy - 0.44f, z + 0.17f), vec3(cx + sd * 0.365f + (sd > 0 ? 0.012f : 0.0f), cy + 0.15f, z + 0.355f), bl * 0.9f);
            m.box(vec3(cx - 0.377f, cy - 0.452f, z + 0.17f), vec3(cx + 0.377f, cy - 0.44f, z + 0.355f), bl * 0.85f);
            ofrustum(m, M * translate(vec3(0, 0.16f, 0.35f)) * rotateZ(HALF) * rotX(HALF), -0.36f, 0.36f, 0.026f, 0.026f, 0.026f, 0.026f, 8, linen);   // the sheet turned down over it
            oellip(m, M, vec3(0, 0.33f, 0.37f), vec3(0.25f, 0.085f, 0.05f), 10, 5, linen);                           // the pillow
            return true;
        }
        case F_CHEST: {
            vec3 wood(0.45f, 0.28f, 0.14f), iron(0.28f, 0.28f, 0.3f);
            m.box(vec3(cx - 0.3f, cy - 0.2f, z), vec3(cx + 0.3f, cy + 0.2f, z + 0.27f), wood);
            oellip(m, M, vec3(0, 0, 0.27f), vec3(0.3f, 0.2f, 0.09f), 16, 3, wood * 1.05f, 0.0f, HALF);   // the curved lid
            for (int k = -1; k <= 1; k += 2) {   // iron bands over it, and down the front
                m.box(vec3(cx + k * 0.2f - 0.018f, cy - 0.205f, z), vec3(cx + k * 0.2f + 0.018f, cy + 0.205f, z + 0.27f), iron);
                oellip(m, M, vec3(k * 0.2f, 0, 0.27f), vec3(0.02f, 0.205f, 0.093f), 3, 3, iron, 0.0f, HALF, 0.0f, 2 * PI_F);
            }
            m.box(vec3(cx - 0.04f, cy - 0.212f, z + 0.17f), vec3(cx + 0.04f, cy - 0.2f, z + 0.26f), iron * 1.3f);   // the lock
            return true;
        }
        case F_BARREL: {
            vec3 wood(0.5f, 0.33f, 0.18f), hoop(0.3f, 0.3f, 0.3f);
            ofrustum(m, M, 0.0f, 0.275f, 0.2f, 0.2f, 0.235f, 0.235f, 14, wood, 0, 0, true, false);   // the staves swell
            ofrustum(m, M, 0.275f, 0.55f, 0.235f, 0.235f, 0.2f, 0.2f, 14, wood * 0.96f, 0, 0, false, true);
            for (float hz : {0.05f, 0.16f, 0.39f, 0.5f}) {
                float r = hz < 0.275f ? 0.2f + 0.035f * hz / 0.275f : 0.235f - 0.035f * (hz - 0.275f) / 0.275f;
                ofrustum(m, M, hz - 0.02f, hz + 0.02f, r + 0.008f, r + 0.008f, r + 0.008f, r + 0.008f, 14, hoop, 0, 0, false, false);
            }
            ofrustum(m, M, 0.545f, 0.555f, 0.17f, 0.17f, 0.17f, 0.17f, 12, wood * 0.75f);   // the head
            return true;
        }
        case F_HEARTH: {
            vec3 st(0.6f, 0.57f, 0.52f), soot(0.08f, 0.06f, 0.05f), wood(0.36f, 0.24f, 0.13f);
            bool north = var == 3;
            float bx0 = north ? cx - 0.45f : cx - 0.5f, bx1 = north ? cx + 0.45f : cx - 0.05f;
            float by0 = north ? cy + 0.05f : cy - 0.45f, by1 = north ? cy + 0.5f : cy + 0.45f;
            // the jambs, the lintel and the hood, the opening black with soot
            if (north) {
                m.box(vec3(bx0, by0, z), vec3(bx0 + 0.12f, by1, z + 0.75f), st);
                m.box(vec3(bx1 - 0.12f, by0, z), vec3(bx1, by1, z + 0.75f), st);
                m.box(vec3(bx0 - 0.04f, by0 - 0.05f, z + 0.75f), vec3(bx1 + 0.04f, by1, z + 0.86f), st * 1.05f);   // the mantel
                m.box(vec3(bx0 + 0.05f, by0 + 0.1f, z + 0.86f), vec3(bx1 - 0.05f, by1, z + 1.2f), st * 0.95f);
                m.box(vec3(bx0 + 0.12f, by0 + 0.2f, z), vec3(bx1 - 0.12f, by1, z + 0.75f), soot);
            } else {
                m.box(vec3(bx0, by0, z), vec3(bx1, by0 + 0.12f, z + 0.75f), st);
                m.box(vec3(bx0, by1 - 0.12f, z), vec3(bx1, by1, z + 0.75f), st);
                m.box(vec3(bx0, by0 - 0.04f, z + 0.75f), vec3(bx1 + 0.05f, by1 + 0.04f, z + 0.86f), st * 1.05f);
                m.box(vec3(bx0, by0 + 0.05f, z + 0.86f), vec3(bx1 - 0.1f, by1 - 0.05f, z + 1.2f), st * 0.95f);
                m.box(vec3(bx0, by0 + 0.12f, z), vec3(bx1 - 0.2f, by1 - 0.12f, z + 0.75f), soot);
            }
            vec3 fc = north ? vec3(cx, cy + 0.1f, z) : vec3(cx - 0.17f, cy, z);
            for (int k = -1; k <= 1; k += 2) {   // two logs, crossed
                mat4 Lg = translate(fc + vec3(0, 0, 0.04f)) * rotateZ(k * 0.5f + (north ? 0.0f : HALF)) * rotX(HALF);
                ofrustum(m, Lg, -0.17f, 0.17f, 0.03f, 0.03f, 0.03f, 0.03f, 6, wood);
            }
            m.flags = 1;
            m.blob(fc + vec3(0, 0, 0.16f), vec3(0.12f, 0.12f, 0.15f), 6, vec3(1.0f, 0.55f, 0.15f), seed, 0.45f);
            m.blob(fc + vec3(0.03f, -0.02f, 0.25f), vec3(0.06f, 0.06f, 0.09f), 5, vec3(1.0f, 0.78f, 0.3f), seed + 3, 0.4f);
            m.flags = 0;
            return true;
        }
        case F_ALTAR: {
            vec3 stone(0.7f, 0.68f, 0.62f), cloth(0.93f, 0.91f, 0.86f), red(0.6f, 0.12f, 0.12f), gold(0.85f, 0.7f, 0.3f);
            m.box(vec3(cx - 0.47f, cy - 0.34f, z), vec3(cx + 0.47f, cy + 0.34f, z + 0.08f), stone * 0.92f);   // the step
            m.box(vec3(cx - 0.43f, cy - 0.3f, z + 0.08f), vec3(cx + 0.43f, cy + 0.3f, z + 0.15f), stone);
            m.box(vec3(cx - 0.3f, cy - 0.4f, z + 0.15f), vec3(cx + 0.3f, cy + 0.4f, z + 0.62f), stone * 1.05f);   // the altar
            m.box(vec3(cx - 0.32f, cy - 0.42f, z + 0.6f), vec3(cx + 0.32f, cy + 0.42f, z + 0.66f), cloth);       // the cloth over it
            m.box(vec3(cx - 0.31f, cy - 0.41f, z + 0.42f), vec3(cx + 0.31f, cy + 0.41f, z + 0.6f), red);         // the frontal
            m.box(vec3(cx - 0.315f, cy - 0.415f, z + 0.42f), vec3(cx + 0.315f, cy + 0.415f, z + 0.44f), gold);   // its fringe
            // the crucifix
            ofrustum(m, M, 0.66f, 0.7f, 0.06f, 0.06f, 0.04f, 0.04f, 8, gold);
            m.box(vec3(cx - 0.012f, cy - 0.012f, z + 0.7f), vec3(cx + 0.012f, cy + 0.012f, z + 1.1f), gold);
            m.box(vec3(cx - 0.012f, cy - 0.1f, z + 0.95f), vec3(cx + 0.012f, cy + 0.1f, z + 0.98f), gold);
            for (int k = -1; k <= 1; k += 2) {   // two candlesticks, turned, and their candles
                float y = k * 0.3f;
                ofrustum(m, M, 0.66f, 0.68f, 0.045f, 0.045f, 0.035f, 0.035f, 10, gold, 0, y);
                ofrustum(m, M, 0.68f, 0.8f, 0.012f, 0.012f, 0.01f, 0.01f, 8, gold, 0, y, false, false);
                oellip(m, M, vec3(0, y, 0.78f), vec3(0.026f, 0.026f, 0.014f), 8, 3, gold);
                ofrustum(m, M, 0.79f, 0.93f, 0.014f, 0.014f, 0.013f, 0.013f, 6, cloth, 0, y);
                m.flags = 1;
                oellip(m, M, vec3(0, y, 0.95f), vec3(0.01f, 0.01f, 0.022f), 6, 3, vec3(1, 0.85f, 0.5f));
                m.flags = 0;
            }
            return true;
        }
        case F_WELL: {
            vec3 st(0.62f, 0.6f, 0.55f), wood(0.42f, 0.3f, 0.18f), dark(0.1f, 0.15f, 0.2f);
            for (int k = 0; k < 16; k++) {   // the wall, stone by stone
                float a0 = 2 * PI_F * k / 16, a1 = 2 * PI_F * (k + 1) / 16;
                vec3 sh = st * (0.9f + 0.12f * hash01(k, var, seed));
                vec3 p0(cx + 0.42f * std::cos(a0), cy + 0.42f * std::sin(a0), z), p1(cx + 0.42f * std::cos(a1), cy + 0.42f * std::sin(a1), z);
                vec3 q0(cx + 0.31f * std::cos(a0), cy + 0.31f * std::sin(a0), z), q1(cx + 0.31f * std::cos(a1), cy + 0.31f * std::sin(a1), z);
                vec3 up(0, 0, 0.46f + 0.03f * (k % 2));
                vec3 o((p0.x + p1.x) * 0.5f - cx, (p0.y + p1.y) * 0.5f - cy, 0);
                m.quad(p0, p1, p1 + up, p0 + up, sh, o);
                m.quad(q1, q0, q0 + up, q1 + up, sh * 0.8f, o * -1.0f);
                m.quad(p0 + up, p1 + up, q1 + up, q0 + up, sh * 1.05f, vec3(0, 0, 1));
            }
            ofrustum(m, M, 0.0f, 0.3f, 0.31f, 0.31f, 0.31f, 0.31f, 16, dark, 0, 0, false, true);   // the water, far down
            for (int sd = -1; sd <= 1; sd += 2) m.box(vec3(cx + sd * 0.33f - 0.035f, cy - 0.035f, z + 0.4f), vec3(cx + sd * 0.33f + 0.035f, cy + 0.035f, z + 1.1f), wood);
            ofrustum(m, M * translate(vec3(0, 0, 0.92f)) * rotateZ(HALF) * rotX(HALF), -0.3f, 0.3f, 0.04f, 0.04f, 0.04f, 0.04f, 8, wood * 0.85f);   // the roller
            m.box(vec3(cx - 0.006f, cy - 0.006f, z + 0.52f), vec3(cx + 0.006f, cy + 0.006f, z + 0.9f), vec3(0.62f, 0.55f, 0.4f));   // the rope
            ofrustum(m, M, 0.44f, 0.56f, 0.06f, 0.06f, 0.07f, 0.07f, 10, wood * 0.9f);                     // the bucket
            m.box(vec3(cx - 0.4f, cy - 0.04f, z + 1.05f), vec3(cx + 0.4f, cy + 0.04f, z + 1.1f), wood);
            m.quad(vec3(cx - 0.45f, cy - 0.3f, z + 1.05f), vec3(cx + 0.45f, cy - 0.3f, z + 1.05f),
                   vec3(cx + 0.45f, cy, z + 1.3f), vec3(cx - 0.45f, cy, z + 1.3f), vec3(0.55f, 0.3f, 0.2f), vec3(0, -1, 1));
            m.quad(vec3(cx - 0.45f, cy + 0.3f, z + 1.05f), vec3(cx + 0.45f, cy + 0.3f, z + 1.05f),
                   vec3(cx + 0.45f, cy, z + 1.3f), vec3(cx - 0.45f, cy, z + 1.3f), vec3(0.55f, 0.3f, 0.2f), vec3(0, 1, 1));
            return true;
        }
        case F_COUNTER: {
            vec3 wood(0.5f, 0.34f, 0.2f);
            m.box(vec3(cx - 0.48f, cy - 0.28f, z), vec3(cx + 0.48f, cy + 0.28f, z + 0.75f), wood);
            m.box(vec3(cx - 0.5f, cy - 0.31f, z), vec3(cx + 0.5f, cy + 0.31f, z + 0.06f), wood * 0.7f);       // the plinth
            for (int k = -1; k <= 1; k++)   // panels on its front and back
                for (int sd = -1; sd <= 1; sd += 2)
                    m.box(vec3(cx + k * 0.31f - 0.12f, cy + sd * 0.28f - 0.012f, z + 0.15f), vec3(cx + k * 0.31f + 0.12f, cy + sd * 0.28f + 0.012f, z + 0.62f), wood * 0.88f);
            m.box(vec3(cx - 0.52f, cy - 0.33f, z + 0.75f), vec3(cx + 0.52f, cy + 0.33f, z + 0.8f), wood * 0.85f, true);
            return true;
        }
        case F_ANVIL: {
            vec3 iron(0.22f, 0.22f, 0.24f), wood(0.42f, 0.28f, 0.15f);
            ofrustum(m, M, 0.0f, 0.42f, 0.24f, 0.24f, 0.21f, 0.21f, 12, wood);                 // the stump
            ofrustum(m, M, 0.42f, 0.44f, 0.2f, 0.2f, 0.2f, 0.2f, 12, wood * 1.15f);             // its cut top, paler
            m.box(vec3(cx - 0.18f, cy - 0.1f, z + 0.44f), vec3(cx + 0.16f, cy + 0.1f, z + 0.5f), iron * 0.9f);    // the foot
            m.box(vec3(cx - 0.12f, cy - 0.07f, z + 0.5f), vec3(cx + 0.1f, cy + 0.07f, z + 0.56f), iron * 0.85f);  // the waist
            m.box(vec3(cx - 0.3f, cy - 0.12f, z + 0.56f), vec3(cx + 0.26f, cy + 0.12f, z + 0.65f), iron);         // the face
            ofrustum(m, M * translate(vec3(0.26f, 0, 0.61f)) * rotateZ(-HALF) * rotX(HALF), 0.0f, 0.22f, 0.1f, 0.045f, 0.004f, 0.004f, 8, iron);   // the horn
            return true;
        }
        case F_FOUNTAIN: {
            vec3 st(0.78f, 0.75f, 0.68f), water(0.3f, 0.5f, 0.62f);
            ofrustum(m, M, 0.0f, 0.36f, 0.76f, 0.76f, 0.74f, 0.74f, 20, st, 0, 0, false, false);   // the basin
            ofrustum(m, M, 0.36f, 0.42f, 0.79f, 0.79f, 0.79f, 0.79f, 20, st * 1.05f, 0, 0, true, false);   // its rim
            ofrustum(m, M, 0.0f, 0.42f, 0.66f, 0.66f, 0.66f, 0.66f, 20, st * 0.85f, 0, 0, false, false);
            ofrustum(m, M, 0.0f, 0.33f, 0.66f, 0.66f, 0.66f, 0.66f, 20, water, 0, 0, false, true);
            ofrustum(m, M, 0.33f, 1.0f, 0.12f, 0.12f, 0.09f, 0.09f, 10, st);                       // the pillar
            oellip(m, M, vec3(0, 0, 1.03f), vec3(0.32f, 0.32f, 0.08f), 16, 3, st, -HALF, 0.0f);    // the upper bowl
            ofrustum(m, M, 1.03f, 1.07f, 0.33f, 0.33f, 0.33f, 0.33f, 16, st * 1.05f, 0, 0, false, false);
            ofrustum(m, M, 1.0f, 1.06f, 0.28f, 0.28f, 0.28f, 0.28f, 16, water, 0, 0, false, true);
            oellip(m, M, vec3(0, 0, 1.25f), vec3(0.06f, 0.06f, 0.2f), 8, 5, st);                   // the finial
            return true;
        }
        default:
            return false;
    }
}
