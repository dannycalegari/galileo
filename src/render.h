// render.h: OpenGL drawing: shadow map, lit flat-shaded meshes, water, and a 2D overlay.
#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "vmath.h"
#include "mesh.h"

struct GpuMesh {
    GLuint vao = 0, vbo = 0;
    int count = 0;
    void upload(const MeshBuf &m);
    void draw() const;
    void release();
};

struct FrameParams {
    mat4 viewProj, lightViewProj;
    vec3 sunDir, sunCol, ambient, sky;
    float hideTag = -1;           // building whose upper parts are hidden
    float keepTag = -1;           // a building drawn whole even with the roofs lifted (the tower you stand on)
    vec2 playerNdc;               // for the see-through circle
    float playerDepth = 0, playerZ = 0, cutRadius = 0, aspect = 1;
    vec3 torch;                   // light carried by the player at night
    float torchStrength = 0, torchRadius = 7;
    float night = 0;              // 0 by day, 1 at night: lit windows
    float time = 0;
    bool shadows = true;
};

struct Renderer {
    GLuint prog = 0, shadowProg = 0, overlayProg = 0;
    GLuint shadowFbo = 0, shadowTex = 0;
    int shadowSize = 4096;
    GLuint overlayTex = 0, overlayVao = 0, overlayVbo = 0;
    int overlayW = 0, overlayH = 0;

    bool init();
    void shadowPassBegin(const FrameParams &f);
    void shadowPassEnd();
    void mainPassBegin(const FrameParams &f, int vpW, int vpH);
    void setWaterMode(bool on);
    // upload a premultiplied BGRA image (cairo ARGB32) and draw it over the frame
    void overlay(const unsigned char *bgra, int w, int h, int stride, bool changed);
};
