// render.cc: shaders and passes.
#include "render.h"
#include <iostream>
#include <cstddef>

static const char *VS_MAIN = R"(#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec3 aCol;
layout(location=3) in float aTag;
layout(location=4) in float aFlags;
uniform mat4 uVP;
uniform mat4 uLightVP;
out vec3 vPos;
out vec3 vNrm;
out vec3 vCol;
out vec4 vLight;
out vec4 vClip;
flat out float vTag;
flat out float vFlags;
void main() {
    vPos = aPos; vNrm = aNrm; vCol = aCol; vTag = aTag; vFlags = aFlags;
    vLight = uLightVP * vec4(aPos, 1.0);
    gl_Position = uVP * vec4(aPos, 1.0);
    vClip = gl_Position;
}
)";

static const char *FS_MAIN = R"(#version 410 core
in vec3 vPos;
in vec3 vNrm;
in vec3 vCol;
in vec4 vLight;
in vec4 vClip;
flat in float vTag;
flat in float vFlags;
uniform sampler2DShadow uShadow;
uniform vec3 uSunDir;
uniform vec3 uSunCol;
uniform vec3 uAmb;
uniform float uHide;
uniform float uKeep;
uniform vec2 uPlayerNdc;
uniform float uPlayerDepth;
uniform float uPlayerZ;
uniform float uCutR;
uniform float uAspect;
uniform vec3 uTorch;
uniform float uTorchK;
uniform float uTorchR;
uniform float uTime;
uniform float uShadowOn;
uniform float uTexel;
uniform float uNight;
out vec4 frag;
void main() {
    if (vTag > 0.5 && abs(vTag - uKeep) >= 0.5 && (abs(vTag - uHide) < 0.5 || uHide < -1.5)) discard;
    vec3 ndc = vClip.xyz / vClip.w;
    if (vFlags < 1.5 && uCutR > 0.0) {
        vec2 d = ndc.xy - uPlayerNdc;
        d.x *= uAspect;
        float r = length(d);
        if (r < uCutR && ndc.z < uPlayerDepth - 0.0015 && vPos.z > uPlayerZ + 0.3) {
            float edge = smoothstep(uCutR * 0.7, uCutR, r);
            ivec2 q = ivec2(gl_FragCoord.xy) & 3;
            int bayer[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
            float dither = (float(bayer[q.y * 4 + q.x]) + 0.5) / 16.0;
            if (dither > edge) discard;
        }
    }
    vec3 n = normalize(vNrm);
    if (!gl_FrontFacing) n = -n;
    vec3 col = vCol;
    float spec = 0.0;
    if (vFlags > 2.5) {
        float w1 = sin(vPos.x * 1.9 + uTime * 1.1 + sin(vPos.y * 0.7)) ;
        float w2 = sin(vPos.y * 1.6 - uTime * 0.8 + cos(vPos.x * 0.5));
        n = normalize(vec3(0.10 * w1, 0.10 * w2, 1.0));
        col *= 0.95 + 0.05 * w1 * w2;
    }
    float sh = 1.0;
    vec3 lc = vLight.xyz / vLight.w * 0.5 + 0.5;
    if (uShadowOn > 0.5 && lc.x > 0.0 && lc.x < 1.0 && lc.y > 0.0 && lc.y < 1.0 && lc.z < 1.0) {
        float ndl = max(dot(n, uSunDir), 0.0);
        float bias = 0.0006 + 0.0012 * (1.0 - ndl);
        sh = 0.0;
        for (int i = -1; i <= 1; i++)
            for (int j = -1; j <= 1; j++)
                sh += texture(uShadow, vec3(lc.xy + vec2(i, j) * uTexel, lc.z - bias));
        sh /= 9.0;
    }
    float diff = max(dot(n, uSunDir), 0.0);
    if (vFlags > 2.5) {
        vec3 h = normalize(uSunDir + vec3(0.0, -0.6, 0.8));
        spec = pow(max(dot(n, h), 0.0), 60.0) * 0.5 * sh;
    }
    vec3 light = uAmb + uSunCol * diff * sh;
    if (uTorchK > 0.0) {
        vec3 tl = uTorch - vPos;
        float dt = length(tl);
        float k = max(0.0, 1.0 - dt / uTorchR);
        light += uTorchK * vec3(1.0, 0.72, 0.42) * k * k * (0.45 + 0.55 * max(dot(n, tl / max(dt, 0.001)), 0.0));
    }
    vec3 c = col * light + uSunCol * spec;
    if (vFlags > 0.5 && vFlags < 1.5) c = col * (1.15 + 0.15 * sin(uTime * 11.0 + vPos.x * 7.0 + vPos.y * 3.0));
    if (vFlags > 3.5 && uNight > 0.0) {
        // about half the windows are lit at night, by candle or fire
        float h = fract(sin(dot(floor(vPos * 1.7 + 0.3), vec3(12.9898, 78.233, 37.719))) * 43758.5453);
        if (h < 0.5) c = mix(c, vec3(1.0, 0.72, 0.38) * (0.75 + 0.5 * h), uNight);
    }
    frag = vec4(c, 1.0);
}
)";

static const char *VS_SHADOW = R"(#version 410 core
layout(location=0) in vec3 aPos;
layout(location=3) in float aTag;
uniform mat4 uLightVP;
flat out float vTag;
void main() { vTag = aTag; gl_Position = uLightVP * vec4(aPos, 1.0); }
)";

static const char *FS_SHADOW = R"(#version 410 core
flat in float vTag;
uniform float uHide;
uniform float uKeep;
void main() { if (vTag > 0.5 && abs(vTag - uKeep) >= 0.5 && (abs(vTag - uHide) < 0.5 || uHide < -1.5)) discard; }
)";

static const char *VS_OVERLAY = R"(#version 410 core
layout(location=0) in vec2 aPos;
out vec2 vUv;
void main() { vUv = vec2(aPos.x * 0.5 + 0.5, 0.5 - aPos.y * 0.5); gl_Position = vec4(aPos, 0.0, 1.0); }
)";

static const char *FS_OVERLAY = R"(#version 410 core
in vec2 vUv;
uniform sampler2D uTex;
out vec4 frag;
void main() { frag = texture(uTex, vUv); }
)";

static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::cerr << "shader error:\n" << log << "\n";
    }
    return s;
}

static GLuint link(const char *vs, const char *fs) {
    GLuint p = glCreateProgram();
    GLuint a = compile(GL_VERTEX_SHADER, vs), b = compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, a);
    glAttachShader(p, b);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        std::cerr << "link error:\n" << log << "\n";
    }
    glDeleteShader(a);
    glDeleteShader(b);
    return p;
}

void GpuMesh::upload(const MeshBuf &m) {
    if (!vao) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, m.v.size() * sizeof(Vertex), m.v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, px));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, nx));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, r));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, tag));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, flags));
    count = (int)m.v.size();
    glBindVertexArray(0);
}

void GpuMesh::draw() const {
    if (!count) return;
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, count);
}

void GpuMesh::release() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = 0;
    count = 0;
}

bool Renderer::init() {
    prog = link(VS_MAIN, FS_MAIN);
    shadowProg = link(VS_SHADOW, FS_SHADOW);
    overlayProg = link(VS_OVERLAY, FS_OVERLAY);

    glGenTextures(1, &shadowTex);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize, shadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenFramebuffers(1, &shadowFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) std::cerr << "shadow framebuffer incomplete\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    float quad[12] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &overlayVao);
    glGenBuffers(1, &overlayVbo);
    glBindVertexArray(overlayVao);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);
    glGenTextures(1, &overlayTex);
    return true;
}

void Renderer::shadowPassBegin(const FrameParams &f) {
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glViewport(0, 0, shadowSize, shadowSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 3.0f);
    glUseProgram(shadowProg);
    glUniformMatrix4fv(glGetUniformLocation(shadowProg, "uLightVP"), 1, GL_FALSE, f.lightViewProj.m);
    glUniform1f(glGetUniformLocation(shadowProg, "uHide"), f.hideTag);
    glUniform1f(glGetUniformLocation(shadowProg, "uKeep"), f.keepTag);
}

void Renderer::shadowPassEnd() {
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void Renderer::mainPassBegin(const FrameParams &f, int vpW, int vpH) {
    glViewport(0, 0, vpW, vpH);
    glClearColor(f.sky.x, f.sky.y, f.sky.z, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUseProgram(prog);
    auto U = [&](const char *n) { return glGetUniformLocation(prog, n); };
    glUniformMatrix4fv(U("uVP"), 1, GL_FALSE, f.viewProj.m);
    glUniformMatrix4fv(U("uLightVP"), 1, GL_FALSE, f.lightViewProj.m);
    glUniform3f(U("uSunDir"), f.sunDir.x, f.sunDir.y, f.sunDir.z);
    glUniform3f(U("uSunCol"), f.sunCol.x, f.sunCol.y, f.sunCol.z);
    glUniform3f(U("uAmb"), f.ambient.x, f.ambient.y, f.ambient.z);
    glUniform1f(U("uHide"), f.hideTag);
    glUniform1f(U("uKeep"), f.keepTag);
    glUniform2f(U("uPlayerNdc"), f.playerNdc.x, f.playerNdc.y);
    glUniform1f(U("uPlayerDepth"), f.playerDepth);
    glUniform1f(U("uPlayerZ"), f.playerZ);
    glUniform1f(U("uCutR"), f.cutRadius);
    glUniform1f(U("uAspect"), f.aspect);
    glUniform3f(U("uTorch"), f.torch.x, f.torch.y, f.torch.z);
    glUniform1f(U("uTorchK"), f.torchStrength);
    glUniform1f(U("uTorchR"), f.torchRadius);
    glUniform1f(U("uTime"), f.time);
    glUniform1f(U("uShadowOn"), f.shadows ? 1.0f : 0.0f);
    glUniform1f(U("uTexel"), 1.0f / shadowSize);
    glUniform1f(U("uNight"), f.night);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glUniform1i(U("uShadow"), 0);
}

void Renderer::setWaterMode(bool on) {
    if (on) glDisable(GL_CULL_FACE);
    else glEnable(GL_CULL_FACE);
}

void Renderer::overlay(const unsigned char *bgra, int w, int h, int stride, bool changed) {
    glBindTexture(GL_TEXTURE_2D, overlayTex);
    if (changed || w != overlayW || h != overlayH) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, bgra);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        overlayW = w;
        overlayH = h;
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(overlayProg);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(overlayProg, "uTex"), 0);
    glBindVertexArray(overlayVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
