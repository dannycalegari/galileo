// sound.h: the sound effects (data/sounds.txt, the files in data/sounds/, made by
// tools/make_sounds.py), through miniaudio like the music (src/ext). Two kinds: sounds played
// once (a bell's stroke, coins, the anvil), at a loudness and from a side; and loops (the
// hooves, the river), whose loudness the game sets every frame and which fade to it.
// The ambient sounds (birds, crickets, voices, waves, gusts) are sets of short variants, one
// played now and then at random, at a random loudness, side and pitch, never the same twice
// running. When each is heard is the game's business (main.cc, Game::soundStep).
#pragma once
#include <map>
#include <string>
#include <vector>

class Sounds {
public:
    bool load(const std::string &file);   // false if there is no data/sounds.txt
    void play(const std::string &id, float volume, float pan = 0);   // once; pan -1 left .. 1 right
    void loop(const std::string &id, float level);   // the loudness wanted now (0: silent), of a
                                                     // loop or of an ambient sound
    void setRate(const std::string &id, float rate);  // a loop played faster (the hooves at a gallop)
    void update(float dt);                // the loops fade towards their levels
    void setOn(bool on);
    bool on() const { return on_; }
    bool has(const std::string &id) const { return files.count(id) > 0; }
    int check(const std::string &file);   // --soundcheck: every file opened and measured (none played); 1 if any fails
    float master = 0.8f;
    int bellEvery = 1;                    // hours between the bells (sounds.txt "bells every N")
    ~Sounds();

private:
    struct Loop { void *player = nullptr; float level = 0, want = 0, gain = 1; };
    // an ambient sound: one of its variants now and then, at random (sounds.txt "ambient")
    struct Ambient { std::vector<std::string> files; float gain = 1, lo = 5, hi = 15, want = 0; double nextAt = -1; int last = -1; };
    std::map<std::string, Ambient> ambients;
    double clock = 0;
    unsigned rng = 12345;
    float rnd01() { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0f; }
    void playFile(const std::string &key, const std::string &file, float volume, float pan, float rate);
    std::map<std::string, std::string> files;   // id -> file
    std::map<std::string, float> gains;         // id -> its loudness in sounds.txt
    std::map<std::string, std::vector<void *>> pool;   // one-shots: a few players each, for overlaps
    std::map<std::string, Loop> loops;
    bool on_ = true;
    bool noPlayers = false;   // check(): read the list only
};
