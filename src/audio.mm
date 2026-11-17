// audio.mm: background music through AVFoundation's AVAudioPlayer (see audio.h).
#import <AVFoundation/AVFoundation.h>
#include "audio.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

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
    if (piece) { AVAudioPlayer *old = (__bridge_transfer AVAudioPlayer *)piece; [old stop]; piece = nullptr; }
    NSError *err = nil;
    AVAudioPlayer *p = [[AVAudioPlayer alloc] initWithContentsOfURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:pc->file.c_str()]] error:&err];
    if (!p) { std::fprintf(stderr, "music: cannot play %s\n", pc->file.c_str()); return false; }
    p.volume = volume * pieceLevel;
    [p play];
    piece = (__bridge_retained void *)p;
    if (seconds) *seconds = p.duration;
    nextStart = now + p.duration + gap;
    return true;
}

bool Music::playCue(const std::string &id, double now, double fadeSecs) {
    const Piece *pc = findPiece(id);
    if (!pc || !on_) return false;
    if (player) {   // the background fades away under the cue
        if (fading) { AVAudioPlayer *f = (__bridge_transfer AVAudioPlayer *)fading; [f stop]; fading = nullptr; }
        [(__bridge AVAudioPlayer *)player setVolume:0 fadeDuration:fadeSecs];
        fading = player;   // the reference passes to fading
        player = nullptr;
        fadeEnd = now + fadeSecs;
    }
    pieceLevel = 1;   // heard where you are, not from someone's corner
    if (std::getenv("GALILEO_SOUNDLOG")) std::fprintf(stderr, "  cue: %s\n", pc->title.c_str());
    return playPiece(id, now);
}

double Music::pieceSeconds(const std::string &id) const {
    const Piece *pc = findPiece(id);
    if (!pc) return 0;
    AVAudioPlayer *p = [[AVAudioPlayer alloc] initWithContentsOfURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:pc->file.c_str()]] error:nil];
    return p ? p.duration : 0;
}

void Music::setPieceLevel(float level) {
    pieceLevel = level;
    if (piece) ((__bridge AVAudioPlayer *)piece).volume = volume * level;
}

bool Music::piecePlaying() const { return piece && ((__bridge AVAudioPlayer *)piece).playing; }

void Music::start(const Track &t) {
    stop();
    NSString *path = [NSString stringWithUTF8String:t.file.c_str()];
    NSError *err = nil;
    AVAudioPlayer *p = [[AVAudioPlayer alloc] initWithContentsOfURL:[NSURL fileURLWithPath:path] error:&err];
    if (!p) { std::fprintf(stderr, "music: cannot play %s\n", t.file.c_str()); return; }
    p.volume = volume;
    [p play];
    player = (__bridge_retained void *)p;
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
        if (fading) { AVAudioPlayer *f = (__bridge_transfer AVAudioPlayer *)fading; [f stop]; fading = nullptr; }
        AVAudioPlayer *p = (__bridge AVAudioPlayer *)player;
        [p setVolume:0 fadeDuration:secs];
        fading = player;          // the reference passes to fading
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

void Music::stop() {
    if (!player) return;
    AVAudioPlayer *p = (__bridge_transfer AVAudioPlayer *)player;
    [p stop];
    player = nullptr;
}

void Music::update(double now) {
    if (fading && now >= fadeEnd) { AVAudioPlayer *f = (__bridge_transfer AVAudioPlayer *)fading; [f stop]; fading = nullptr; }
    if (piece) {   // someone is playing: the background waits until a while after the piece
        if (((__bridge AVAudioPlayer *)piece).playing) return;
        AVAudioPlayer *p = (__bridge_transfer AVAudioPlayer *)piece;
        [p stop];
        piece = nullptr;
        nextStart = now + gap;
        return;
    }
    int m = mode();
    bool church = m == 1;
    const std::vector<Track> &list = m == 2 ? battleTracks : church ? churchTracks : m == 3 ? innTracks : m == 4 ? theatreTracks : tracks;
    if (!on_ || list.empty()) return;
    if (player) {
        AVAudioPlayer *p = (__bridge AVAudioPlayer *)player;
        if (p.playing) return;
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
        if (fading) { AVAudioPlayer *f = (__bridge_transfer AVAudioPlayer *)fading; [f stop]; fading = nullptr; }
        if (piece) { AVAudioPlayer *p = (__bridge_transfer AVAudioPlayer *)piece; [p stop]; piece = nullptr; }
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
    if (fading) { AVAudioPlayer *f = (__bridge_transfer AVAudioPlayer *)fading; [f stop]; fading = nullptr; }
    if (piece) { AVAudioPlayer *p = (__bridge_transfer AVAudioPlayer *)piece; [p stop]; piece = nullptr; }
}
