// sound.cc: the sound effects through miniaudio (src/ext; see sound.h). The files are decoded once into memory
// (miniaudio's resource manager shares them between the players of one file).
#include "sound.h"
#include "audio.h"
#include "ext/miniaudio.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <fstream>
#include <sstream>

static ma_sound *makePlayer(const std::string &file) {
    ma_engine *e = audioEngine();
    if (!e) return nullptr;
    ma_sound *s = new ma_sound;
    if (ma_sound_init_from_file(e, file.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr, s) != MA_SUCCESS) {
        delete s;
        return nullptr;
    }
    return s;
}

static void freePlayer(void *p) {
    if (!p) return;
    ma_sound *s = (ma_sound *)p;
    ma_sound_stop(s);
    ma_sound_uninit(s);
    delete s;
}

bool Sounds::load(const std::string &file) {
    std::ifstream in(file);
    if (!in) return false;
    rng = (unsigned)time(nullptr) * 2654435761u;   // a different order of sounds every time you play
    std::string dir = file.substr(0, file.rfind('/') + 1) + "sounds/", line;
    while (std::getline(in, line)) {
        size_t h = line.find('#');
        if (h != std::string::npos) line = line.substr(0, h);
        std::istringstream ss(line);
        std::string w;
        if (!(ss >> w)) continue;
        if (w == "volume") ss >> master;
        else if (w == "bells") { std::string every; ss >> every >> bellEvery; bellEvery = std::max(1, bellEvery); }
        else if (w == "ambient") {
            // ambient ID STEM COUNT LOUDNESS MIN MAX: STEM_1.wav .. STEM_COUNT.wav, one every MIN to MAX seconds
            std::string id, stem;
            int count = 0;
            Ambient A;
            ss >> id >> stem >> count >> A.gain >> A.lo >> A.hi;
            for (int k = 1; k <= count; k++) A.files.push_back(dir + stem + "_" + std::to_string(k) + ".wav");
            if (!A.files.empty()) { ambients[id] = A; files[id] = A.files[0]; }
        }
        else if (w == "sound" || w == "loop") {
            std::string id, f;
            float g = 1;
            ss >> id >> f >> g;
            files[id] = dir + f;
            gains[id] = g;
            if (w == "loop" && !noPlayers) {
                ma_sound *p = makePlayer(dir + f);
                if (!p) continue;
                ma_sound_set_looping(p, MA_TRUE);
                ma_sound_set_volume(p, 0);
                Loop L;
                L.player = p;
                L.gain = g;
                loops[id] = L;
            }
        }
    }
    return !files.empty();
}

void Sounds::play(const std::string &id, float volume, float pan) {
    if (!on_ || volume <= 0.001f) return;
    auto f = files.find(id);
    if (f == files.end() || loops.count(id) || ambients.count(id)) return;
    playFile(id, f->second, volume * gains[id], pan, 1.0f);
}

void Sounds::playFile(const std::string &key, const std::string &file, float volume, float pan, float rate) {
    auto &v = pool[file];
    ma_sound *use = nullptr;
    for (void *q : v)
        if (!ma_sound_is_playing((ma_sound *)q)) { use = (ma_sound *)q; break; }
    if (!use && v.size() < 6) {   // a few players a file, for overlaps
        ma_sound *p = makePlayer(file);
        if (!p) return;
        v.push_back(p);
        use = p;
    }
    if (!use) return;
    static const bool log = std::getenv("GALILEO_SOUNDLOG") != nullptr;
    if (log) std::fprintf(stderr, "  sound %s at %.2f, pan %.2f, rate %.2f (%s)\n", key.c_str(), volume, pan, rate, file.substr(file.rfind('/') + 1).c_str());
    ma_sound_set_volume(use, std::min(1.0f, volume * master));
    ma_sound_set_pan(use, std::max(-1.0f, std::min(1.0f, pan)));
    ma_sound_set_pitch(use, rate);   // faster and higher together, as a recording played faster
    ma_sound_seek_to_pcm_frame(use, 0);
    ma_sound_start(use);
}

void Sounds::loop(const std::string &id, float level) {
    level = std::max(0.0f, std::min(1.0f, level));
    auto it = loops.find(id);
    if (it != loops.end()) it->second.want = level;
    auto a = ambients.find(id);
    if (a != ambients.end()) a->second.want = level;
}

void Sounds::setRate(const std::string &id, float rate) {
    auto it = loops.find(id);
    if (it != loops.end()) ma_sound_set_pitch((ma_sound *)it->second.player, std::max(0.5f, std::min(2.0f, rate)));
}

void Sounds::update(float dt) {
    clock += dt;
    // the ambient sounds: when one is wanted, a variant now and then; not wanted, the next one
    // waits a while after it is wanted again (so that it never starts the moment you arrive)
    for (auto &kv : ambients) {
        Ambient &A = kv.second;
        if (!on_ || A.want < 0.03f) { A.nextAt = -1; continue; }
        if (A.nextAt < 0) { A.nextAt = clock + A.lo * 0.3 + rnd01() * (A.hi - A.lo) * 0.6; continue; }
        if (clock < A.nextAt) continue;
        int k = (int)(rnd01() * A.files.size()) % (int)A.files.size();
        if (k == A.last && A.files.size() > 1) k = (k + 1) % (int)A.files.size();
        A.last = k;
        float vol = A.want * A.gain * (0.35f + 0.65f * rnd01());
        float pan = (rnd01() * 2 - 1) * 0.7f;
        float rate = 0.92f + 0.16f * rnd01();
        playFile(kv.first, A.files[k], vol, pan, rate);
        float r = rnd01();
        A.nextAt = clock + A.lo + (A.hi - A.lo) * r * r;   // mostly nearer the short end, now and then a long quiet
    }
    for (auto &kv : loops) {
        Loop &L = kv.second;
        float want = on_ ? L.want : 0;
        float k = std::min(1.0f, dt / 1.2f);   // over a second or so
        L.level += (want - L.level) * k;
        if (std::fabs(want - L.level) < 0.002f) L.level = want;
        ma_sound *p = (ma_sound *)L.player;
        ma_sound_set_volume(p, L.level * L.gain * master);
        static const bool log = std::getenv("GALILEO_SOUNDLOG") != nullptr;
        bool playing = ma_sound_is_playing(p);
        if (L.level > 0.003f && !playing) { ma_sound_start(p); if (log) std::fprintf(stderr, "  loop %s begins\n", kv.first.c_str()); }
        else if (L.level <= 0.003f && playing) { ma_sound_stop(p); if (log) std::fprintf(stderr, "  loop %s ends\n", kv.first.c_str()); }   // stopped where it is: it goes on from there
    }
}

void Sounds::setOn(bool on) { on_ = on; }

// --soundcheck: every file of data/sounds.txt opened and measured; how many cannot be read
int Sounds::check(const std::string &file) {
    noPlayers = true;
    if (!load(file)) { std::printf("no data/sounds.txt\n"); return 1; }
    std::vector<std::string> all;
    for (auto &kv : files) all.push_back(kv.second);
    for (auto &kv : ambients) for (auto &f : kv.second.files) all.push_back(f);
    std::sort(all.begin(), all.end());
    all.erase(std::unique(all.begin(), all.end()), all.end());
    int bad = 0;
    for (auto &f : all) if (audioFileSeconds(f) <= 0) { std::printf("!! cannot read %s\n", f.c_str()); bad++; }
    std::printf("sounds: %zu files, %d unreadable\n", all.size(), bad);
    return bad ? 1 : 0;
}

Sounds::~Sounds() {
    for (auto &kv : loops) freePlayer(kv.second.player);
    for (auto &kv : pool)
        for (void *q : kv.second) freePlayer(q);
}
