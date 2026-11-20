// audio.cc: background music through miniaudio (src/ext; see audio.h). Each piece is streamed from its file (Ogg
// Vorbis), one ma_sound at a time, with a second one fading away when the music changes.
#include "audio.h"
#include "ext/miniaudio.h"
#define STB_VORBIS_HEADER_ONLY
#include "ext/stb_vorbis.c"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

// the one sound device of the game, opened the first time music or a sound needs it; nullptr if there is none (then
// everything is silent and nothing else changes)
static bool gNoDevice = false;   // --soundtest: an engine without a device, whose output is read and measured
ma_engine *audioEngine() {
    static ma_engine engine;
    static int state = 0;   // 0 not tried, 1 open, -1 failed
    if (state == 0) {
        ma_engine_config cfg = ma_engine_config_init();
        if (gNoDevice) { cfg.noDevice = MA_TRUE; cfg.channels = 2; cfg.sampleRate = 44100; }
        state = ma_engine_init(&cfg, &engine) == MA_SUCCESS ? 1 : -1;
        if (state < 0) std::fprintf(stderr, "sound: no sound device; the game is silent\n");
    }
    return state > 0 ? &engine : nullptr;
}

// a file's length in seconds (0 if it cannot be read): an Ogg's from stb_vorbis, which knows it at once, the others'
// from miniaudio's decoder
double audioFileSeconds(const std::string &file) {
    if (file.size() > 4 && file.compare(file.size() - 4, 4, ".ogg") == 0) {
        stb_vorbis *v = stb_vorbis_open_filename(file.c_str(), nullptr, nullptr);
        if (!v) return 0;
        double s = stb_vorbis_stream_length_in_seconds(v);
        stb_vorbis_close(v);
        return s;
    }
    ma_decoder d;
    if (ma_decoder_init_file(file.c_str(), nullptr, &d) != MA_SUCCESS) return 0;
    ma_uint64 frames = 0;
    ma_decoder_get_length_in_pcm_frames(&d, &frames);
    double s = d.outputSampleRate ? (double)frames / d.outputSampleRate : 0;
    ma_decoder_uninit(&d);
    return s;
}

// a piece of music, streamed from its file and started at once (nullptr if it cannot be)
static ma_sound *startStream(const std::string &file, float vol) {
    ma_engine *e = audioEngine();
    if (!e) return nullptr;
    ma_sound *s = new ma_sound;
    if (ma_sound_init_from_file(e, file.c_str(), MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr, s) != MA_SUCCESS) {
        delete s;
        return nullptr;
    }
    ma_sound_set_volume(s, vol);
    ma_sound_start(s);
    return s;
}

static void endStream(void *&p) {
    if (!p) return;
    ma_sound *s = (ma_sound *)p;
    ma_sound_stop(s);
    ma_sound_uninit(s);
    delete s;
    p = nullptr;
}

static bool streamPlaying(void *p) { return p && ma_sound_is_playing((ma_sound *)p); }

// fade a piece out over SECS from where its loudness is now (it is ended later, by update)
static void fadeOut(void *p, double secs) {
    if (p) ma_sound_set_fade_in_milliseconds((ma_sound *)p, -1, 0, (ma_uint64)(secs * 1000));
}

bool Music::load(const std::string &playlist) {
    std::ifstream in(playlist);
    if (!in) return false;
    std::string dir = playlist.substr(0, playlist.rfind('/') + 1), line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string w;
        ss >> w;
        if (w == "gap") ss >> gap;
        else if (w == "volume") ss >> volume;
        else if (w == "piece" || w == "cue") {
            Piece p;
            ss >> p.id >> p.file;
            std::getline(ss, p.title);
            size_t a = p.title.find_first_not_of(" \t");
            p.title = a == std::string::npos ? "" : p.title.substr(a);
            p.file = dir + p.file;
            pieces.push_back(p);
        }
        else if (w == "nochoir") { std::string t; while (ss >> t) noChoir.push_back(t); }
        else if (w == "track" || w == "church" || w == "battle" || w == "inn" || w == "theatre") {
            Track t;
            ss >> t.file;
            while (t.file == "in" || t.file == "except") {   // church [in|except TOWN,TOWN] FILE TITLE
                std::string list, one;
                ss >> list;
                std::istringstream ls(list);
                while (std::getline(ls, one, ',')) (t.file == "in" ? t.only : t.except).push_back(one);
                ss >> t.file;
            }
            std::getline(ss, t.title);
            size_t a = t.title.find_first_not_of(" \t");
            t.title = a == std::string::npos ? "" : t.title.substr(a);
            t.file = dir + t.file;
            (w == "church" ? churchTracks : w == "battle" ? battleTracks : w == "inn" ? innTracks : w == "theatre" ? theatreTracks : tracks).push_back(t);
        }
    }
    return !tracks.empty() || !pieces.empty() || !churchTracks.empty() || !battleTracks.empty() || !innTracks.empty() || !theatreTracks.empty();
}

bool Music::playPiece(const std::string &id, double now, double *seconds) {
    const Piece *pc = findPiece(id);
    if (!pc || !on_) return false;
    stop();   // the background music gives way
    endStream(piece);
    ma_sound *p = startStream(pc->file, volume * pieceLevel);
    if (!p) { std::fprintf(stderr, "music: cannot play %s\n", pc->file.c_str()); return false; }
    piece = p;
    double len = audioFileSeconds(pc->file);
    if (seconds) *seconds = len;
    nextStart = now + len + gap;
    return true;
}

bool Music::playCue(const std::string &id, double now, double fadeSecs) {
    const Piece *pc = findPiece(id);
    if (!pc || !on_) return false;
    if (player) {   // the background fades away under the cue
        endStream(fading);
        fadeOut(player, fadeSecs);
        fading = player;   // it passes to fading
        player = nullptr;
        fadeEnd = now + fadeSecs;
    }
    pieceLevel = 1;   // heard where you are, not from someone's corner
    if (std::getenv("GALILEO_SOUNDLOG")) std::fprintf(stderr, "  cue: %s\n", pc->title.c_str());
    return playPiece(id, now);
}

double Music::pieceSeconds(const std::string &id) const {
    const Piece *pc = findPiece(id);
    return pc ? audioFileSeconds(pc->file) : 0;
}

void Music::setPieceLevel(float level) {
    pieceLevel = level;
    if (piece) ma_sound_set_volume((ma_sound *)piece, volume * level);
}

bool Music::piecePlaying() const { return streamPlaying(piece); }

void Music::start(const Track &t) {
    stop();
    ma_sound *p = startStream(t.file, volume);
    if (!p) { if (audioEngine()) std::fprintf(stderr, "music: cannot play %s\n", t.file.c_str()); return; }
    player = p;
    announce = t.title;
    if (std::getenv("GALILEO_SOUNDLOG")) std::fprintf(stderr, "  music: %s\n", t.title.c_str());
}

// which music is wanted: 2 a fight near you ("battle"), 1 inside a church ("church"), 3 inside
// an inn ("inn"), 4 inside a theatre ("theatre"), 0 the ordinary
int Music::mode() const {
    if (inBattle && !battleTracks.empty()) return 2;
    if (inChurch && !churchTracks.empty()) return 1;
    if (inInn) for (auto &t : innTracks) if (t.playsIn(innTown, innCountry)) return 3;
    if (inTheatre) for (auto &t : theatreTracks) if (t.playsIn(theatreTown, theatreCountry)) return 4;
    return 0;
}

void Music::setTheatre(bool in, const std::string &town, const std::string &country, double now) {
    if (in) { theatreTown = town; theatreCountry = country; }
    if (in == inTheatre) return;
    int was = mode();
    inTheatre = in;
    if (mode() != was) switchTo(mode(), now);
}

void Music::setInn(bool in, const std::string &town, const std::string &country, double now) {
    if (in) { innTown = town; innCountry = country; }
    if (in == inInn) return;
    int was = mode();
    inInn = in;
    if (mode() != was) switchTo(mode(), now);
}

// from one kind of music to another: what plays fades away, and the other begins after a moment
// (a fight's music at once; a church's after a breath; the ordinary after a longer pause)
void Music::switchTo(int to, double now) {
    if (piece || !on_) return;   // someone is playing (or music is off): the change waits for the next piece
    double secs = to == 2 ? 0.8 : to == 1 || to >= 3 ? 2.0 : 3.0, after = to == 2 ? 0.2 : to == 1 || to >= 3 ? 1.0 : 8.0;
    if (player) {
        endStream(fading);
        fadeOut(player, secs);
        fading = player;          // it passes to fading
        player = nullptr;
        fadeEnd = now + secs;
        nextStart = now + secs + after;
    } else nextStart = std::min(nextStart, now + after + 0.5);
}

void Music::setChurch(bool in, const std::string &town, double now) {
    if (in) churchTown = town;
    if (in == inChurch) return;
    int was = mode();
    inChurch = in;
    if (mode() != was) switchTo(mode(), now);
}

void Music::setBattle(bool on, double now) {
    if (on == inBattle) return;
    int was = mode();
    inBattle = on;
    if (mode() != was) switchTo(mode(), now);
}

void Music::stop() { endStream(player); }

void Music::update(double now) {
    if (fading && now >= fadeEnd) endStream(fading);
    if (piece) {   // someone is playing: the background waits until a while after the piece
        if (streamPlaying(piece)) return;
        endStream(piece);
        nextStart = now + gap;
        return;
    }
    int m = mode();
    bool church = m == 1;
    const std::vector<Track> &list = m == 2 ? battleTracks : church ? churchTracks : m == 3 ? innTracks : m == 4 ? theatreTracks : tracks;
    if (!on_ || list.empty()) return;
    if (player) {
        if (streamPlaying(player)) return;
        stop();                       // it has ended: a pause, then the next piece (a fight's music goes on at once)
        nextStart = now + (m == 2 ? 1.0 : gap);
        return;
    }
    if (now < nextStart) return;
    int &n = m == 2 ? nextBattle : church ? nextChurch : m == 3 ? nextInn : m == 4 ? nextTheatre : next;
    n %= (int)list.size();
    // the next piece heard here: a church's sung in this town, an inn's played in this country,
    // an ordinary one for this place (a track without places is heard everywhere)
    const std::string &town = church ? churchTown : m == 3 ? innTown : m == 4 ? theatreTown : placeTown;
    const std::string &country = church ? std::string() : m == 3 ? innCountry : m == 4 ? theatreCountry : placeCountry;
    size_t k = 0;
    for (; k < list.size() && !list[n].playsIn(town, country); k++) n = (n + 1) % (int)list.size();
    if (k == list.size()) return;   // nothing for this place
    start(list[n]);
    n = (n + 1) % (int)list.size();
}

void Music::setOn(bool on, double now) {
    on_ = on;
    if (!on) {
        stop();
        endStream(fading);
        endStream(piece);
    }
    else nextStart = now + 0.5;
}

std::string Music::takeAnnouncement() {
    std::string a = announce;
    announce.clear();
    return a;
}

Music::~Music() {
    stop();
    endStream(fading);
    endStream(piece);
}

// --soundcheck: every file of the music and the sounds opened and measured (no sound is played)
int audioCheck(const std::string &dataDir) {
    Music m;
    int bad = 0, n = 0;
    if (!m.load(dataDir + "/music.txt")) { std::printf("no data/music.txt\n"); return 1; }
    std::vector<std::pair<std::string, std::string>> all;
    for (auto *list : {&m.tracks, &m.churchTracks, &m.battleTracks, &m.innTracks, &m.theatreTracks})
        for (auto &t : *list) all.push_back({t.file, t.title});
    for (auto &p : m.pieces) all.push_back({p.file, p.title});
    std::sort(all.begin(), all.end());
    all.erase(std::unique(all.begin(), all.end(), [](auto &a, auto &b) { return a.first == b.first; }), all.end());
    for (auto &f : all) {
        double s = audioFileSeconds(f.first);
        n++;
        if (s <= 0) bad++;
        std::printf("%s %7.1f s  %s\n", s > 0 ? "  " : "!!", s, f.first.c_str());
    }
    std::printf("music: %d files, %d unreadable\n", n, bad);
    return bad ? 1 : 0;
}

// --soundtest: the music and the sounds run as in the game, on an engine with no device, its output read in steps of
// 50 ms, at the pace of real time (the music is streamed, and decoded as it is played), and measured (dBFS RMS): a piece begins, a fight's music fades the other out and begins, music off is
// silence, a sound is heard, a loop fades in and out, and a cue ends and is known to have ended. 0 if all is as
// expected.
#include "sound.h"
#include <chrono>
#include <cmath>
#include <thread>
int audioPlayTest(const std::string &dataDir) {
    gNoDevice = true;
    ma_engine *e = audioEngine();
    if (!e) { std::printf("no engine\n"); return 1; }
    Music m;
    Sounds snd;
    if (!m.load(dataDir + "/music.txt") || !snd.load(dataDir + "/sounds.txt")) { std::printf("no music.txt or sounds.txt\n"); return 1; }
    m.setPlace("London", "England");
    std::vector<float> buf(2205 * 2);
    double t = 0;
    int bad = 0;
    auto step = [&](double until, const char *what, bool wantSound, double from) {
        // run to UNTIL; between FROM and UNTIL the output must be sounding (or silent); WHAT empty: not checked
        double lo = 1e9, hi = -1e9;
        for (; t < until - 1e-9; t += 0.05) {
            m.update(t);
            snd.update(0.05f);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ma_uint64 got = 0;
            ma_engine_read_pcm_frames(e, buf.data(), 2205, &got);
            double sum = 0;
            for (ma_uint64 i = 0; i < got * 2; i++) sum += (double)buf[i] * buf[i];
            double db = 10 * std::log10(std::max(1e-12, sum / std::max<ma_uint64>(1, got * 2)));
            if (t >= from) { lo = std::min(lo, db); hi = std::max(hi, db); }
        }
        if (!*what) return;
        bool ok = wantSound ? lo > -60 : hi < -90;
        std::printf("%s %-58s %6.1f to %6.1f dBFS (%.2f-%.2f s)\n", ok ? "ok" : "!!", what, lo, hi, from, until);
        if (!ok) bad++;
    };
    step(1.9, "quiet at the start (the music waits 2 s)", false, 0);
    step(6.0, "a piece begins and goes on", true, 2.6);
    std::string a = m.takeAnnouncement();
    std::printf("   (it is %s)\n", a.c_str());
    m.setBattle(true, t);
    step(t + 0.4, "a fight: the piece fades out over 0.8 s, heard at first", true, t);
    step(t + 0.55, "and gone at its end, before the fight's music", false, t + 0.4);
    step(t + 3.0, "the fight's music begins (0.2 s after)", true, t + 0.3);
    a = m.takeAnnouncement();
    std::printf("   (it is %s)\n", a.c_str());
    m.setOn(false, t);
    step(t + 1.0, "music off: silence", false, t + 0.1);
    snd.play("coins", 1.0f);
    step(t + 0.3, "a sound: the coins", true, t + 0.05);
    step(t + 2.5, "and silence after it", false, t + 1.8);
    snd.loop("river", 1.0f);
    step(t + 3.0, "a loop fades in: the river", true, t + 1.0);
    snd.loop("river", 0.0f);
    step(t + 9.0, "and fades out (over some 7 s), and stops", false, t + 8.0);
    m.setOn(true, t);
    double secs = 0;
    bool cue = m.playCue("sleep", t, 1.0);
    secs = m.pieceSeconds("sleep");
    double start = t;
    step(t + secs - 3.0, "a cue (the sleep cue) plays", true, t + 0.2);   // (then its last chord dies away)
    step(start + secs - 0.5, "", true, t);
    bool during = m.piecePlaying();
    step(start + secs + 1.0, "and ends", false, start + secs + 0.6);
    bool after = m.piecePlaying();
    std::printf("%s the cue known to be playing while it plays, and over after it (%s, %s)\n", cue && during && !after ? "ok" : "!!", during ? "playing" : "not playing", after ? "playing" : "over");
    if (!(cue && during && !after)) bad++;
    std::printf("%s\n", bad ? "SOME CHECKS FAILED" : "all as expected");
    return bad ? 1 : 0;
}
