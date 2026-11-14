// vmath.h: small vector and matrix library (column-major 4x4 matrices, as OpenGL).
#pragma once
#include <cmath>
#include <algorithm>

struct vec2 {
    float x = 0, y = 0;
    vec2() {}
    vec2(float a, float b) : x(a), y(b) {}
};
inline vec2 operator+(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline vec2 operator-(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline vec2 operator*(vec2 a, float s) { return {a.x * s, a.y * s}; }
inline float dot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length(vec2 a) { return std::sqrt(dot(a, a)); }

struct vec3 {
    float x = 0, y = 0, z = 0;
    vec3() {}
    vec3(float a, float b, float c) : x(a), y(b), z(c) {}
};
inline vec3 operator+(vec3 a, vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline vec3 operator-(vec3 a, vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline vec3 operator-(vec3 a) { return {-a.x, -a.y, -a.z}; }
inline vec3 operator*(vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline vec3 operator*(float s, vec3 a) { return {a.x * s, a.y * s, a.z * s}; }
inline vec3 mul(vec3 a, vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline vec3 cross(vec3 a, vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(vec3 a) { return std::sqrt(dot(a, a)); }
inline vec3 normalize(vec3 a) {
    float l = length(a);
    return l > 0 ? a * (1.0f / l) : vec3(0, 0, 1);
}
inline vec3 lerp(vec3 a, vec3 b, float t) { return a + (b - a) * t; }

struct vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    vec4() {}
    vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
};

struct mat4 {
    float m[16];  // column-major: m[col*4+row]
    mat4() { for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f; }
    float &at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
};

inline mat4 operator*(const mat4 &a, const mat4 &b) {
    mat4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.at(i, k) * b.at(k, j);
            r.at(i, j) = s;
        }
    return r;
}

inline vec4 operator*(const mat4 &a, vec4 v) {
    float in[4] = {v.x, v.y, v.z, v.w}, out[4];
    for (int i = 0; i < 4; i++) {
        out[i] = 0;
        for (int k = 0; k < 4; k++) out[i] += a.at(i, k) * in[k];
    }
    return {out[0], out[1], out[2], out[3]};
}

inline mat4 ortho(float l, float r, float b, float t, float n, float f) {
    mat4 M;
    M.at(0, 0) = 2 / (r - l);
    M.at(1, 1) = 2 / (t - b);
    M.at(2, 2) = -2 / (f - n);
    M.at(0, 3) = -(r + l) / (r - l);
    M.at(1, 3) = -(t + b) / (t - b);
    M.at(2, 3) = -(f + n) / (f - n);
    return M;
}

inline mat4 lookAt(vec3 eye, vec3 target, vec3 up) {
    vec3 f = normalize(target - eye);
    vec3 s = normalize(cross(f, up));
    vec3 u = cross(s, f);
    mat4 M;
    M.at(0, 0) = s.x; M.at(0, 1) = s.y; M.at(0, 2) = s.z;
    M.at(1, 0) = u.x; M.at(1, 1) = u.y; M.at(1, 2) = u.z;
    M.at(2, 0) = -f.x; M.at(2, 1) = -f.y; M.at(2, 2) = -f.z;
    M.at(0, 3) = -dot(s, eye);
    M.at(1, 3) = -dot(u, eye);
    M.at(2, 3) = dot(f, eye);
    return M;
}

inline mat4 translate(vec3 t) {
    mat4 M;
    M.at(0, 3) = t.x; M.at(1, 3) = t.y; M.at(2, 3) = t.z;
    return M;
}

inline mat4 scaleM(float k) {
    mat4 M;
    M.at(0, 0) = k; M.at(1, 1) = k; M.at(2, 2) = k;
    return M;
}

inline mat4 rotateZ(float a) {
    mat4 M;
    float c = std::cos(a), s = std::sin(a);
    M.at(0, 0) = c; M.at(0, 1) = -s;
    M.at(1, 0) = s; M.at(1, 1) = c;
    return M;
}

inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float smoothstep(float a, float b, float x) {
    float t = clampf((x - a) / (b - a), 0, 1);
    return t * t * (3 - 2 * t);
}
inline float mixf(float a, float b, float t) { return a + (b - a) * t; }

constexpr float PI_F = 3.14159265358979f;
