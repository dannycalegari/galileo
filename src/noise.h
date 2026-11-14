// noise.h: deterministic hashing and value noise, so the world is the same every run.
#pragma once
#include <cstdint>
#include <cmath>

inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

inline uint32_t hash2i(int x, int y, uint32_t seed = 0) {
    return hash32((uint32_t)x * 0x9E3779B1U ^ hash32((uint32_t)y + 0x632BE5ABU + seed * 0x85EBCA77U));
}

// uniform in [0,1)
inline float hash01(int x, int y, uint32_t seed = 0) {
    return (hash2i(x, y, seed) >> 8) * (1.0f / 16777216.0f);
}

// value noise in [-1,1], smooth
inline float vnoise(float x, float y, uint32_t seed = 0) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - ix, fy = y - iy;
    float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    float a = hash01(ix, iy, seed), b = hash01(ix + 1, iy, seed);
    float c = hash01(ix, iy + 1, seed), d = hash01(ix + 1, iy + 1, seed);
    float v = a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
    return 2 * v - 1;
}

// fractal sum, roughly in [-1,1]
inline float fbm(float x, float y, int octaves, uint32_t seed = 0) {
    float s = 0, amp = 0.5f, norm = 0;
    for (int i = 0; i < octaves; i++) {
        s += amp * vnoise(x, y, seed + i * 101);
        norm += amp;
        x = x * 2.03f + 17.1f;
        y = y * 2.03f - 9.7f;
        amp *= 0.5f;
    }
    return s / norm;
}

// small deterministic random generator for building layouts
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(hash32(seed + 0x1234567U)) {}
    uint32_t next() { s = hash32(s + 0x9E3779B9U); return s; }
    float uni() { return (next() >> 8) * (1.0f / 16777216.0f); }
    int range(int a, int b) { return a + (int)(next() % (uint32_t)(b - a + 1)); }  // inclusive
    bool chance(float p) { return uni() < p; }
};
