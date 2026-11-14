// sound.mm: the sound effects through AVFoundation's AVAudioPlayer (see sound.h).
#import <AVFoundation/AVFoundation.h>
#include "sound.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <fstream>
#include <sstream>

static AVAudioPlayer *makePlayer(const std::string &file) {
    NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:file.c_str()]];
    NSError *err = nil;
    AVAudioPlayer *p = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&err];
    if (!p) return nil;
    [p prepareToPlay];
    return p;
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
            if (w == "loop") {
                AVAudioPlayer *p = makePlayer(dir + f);
                if (!p) continue;
                p.numberOfLoops = -1;
                p.enableRate = YES;
                p.volume = 0;
                Loop L;
                L.player = (__bridge_retained void *)p;
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
    AVAudioPlayer *use = nil;
    for (void *q : v) {
        AVAudioPlayer *p = (__bridge AVAudioPlayer *)q;
        if (!p.playing) { use = p; break; }
    }
    if (!use && v.size() < 6) {
        AVAudioPlayer *p = makePlayer(file);
        if (p) p.enableRate = YES;
        if (!p) return;
        v.push_back((__bridge_retained void *)p);
        use = p;
    }
    if (!use) return;
    static const bool log = std::getenv("GALILEO_SOUNDLOG") != nullptr;
    if (log) std::fprintf(stderr, "  sound %s at %.2f, pan %.2f, rate %.2f (%s)\n", key.c_str(), volume, pan, rate, file.substr(file.rfind('/') + 1).c_str());
    use.volume = std::min(1.0f, volume * master);
    use.pan = std::max(-1.0f, std::min(1.0f, pan));
    use.rate = rate;
    use.currentTime = 0;
    [use play];
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
    if (it != loops.end()) ((__bridge AVAudioPlayer *)it->second.player).rate = std::max(0.5f, std::min(2.0f, rate));
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
        AVAudioPlayer *p = (__bridge AVAudioPlayer *)L.player;
        p.volume = L.level * L.gain * master;
        static const bool log = std::getenv("GALILEO_SOUNDLOG") != nullptr;
        if (L.level > 0.003f && !p.playing) { [p play]; if (log) std::fprintf(stderr, "  loop %s begins\n", kv.first.c_str()); }
        else if (L.level <= 0.003f && p.playing) { [p pause]; if (log) std::fprintf(stderr, "  loop %s ends\n", kv.first.c_str()); }
    }
}

void Sounds::setOn(bool on) { on_ = on; }

Sounds::~Sounds() {
    for (auto &kv : loops) { AVAudioPlayer *p = (__bridge_transfer AVAudioPlayer *)kv.second.player; [p stop]; }
    for (auto &kv : pool)
        for (void *q : kv.second) { AVAudioPlayer *p = (__bridge_transfer AVAudioPlayer *)q; [p stop]; }
}
