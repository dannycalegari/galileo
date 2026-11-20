// audio.h: background music. A playlist (data/music.txt) of recordings (Ogg Vorbis), played one after
// another with a pause between them, through miniaudio (src/ext). The game calls update()
// every frame; nothing else needs to know how the sound is made.
#pragma once
#include <string>
#include <vector>

struct ma_engine;
ma_engine *audioEngine();                          // the one sound device (audio.cc); nullptr if there is none
double audioFileSeconds(const std::string &file);   // a recording's length (0: unreadable)
int audioCheck(const std::string &dataDir);         // --soundcheck: every file opened and measured; 1 if any fails
int audioPlayTest(const std::string &dataDir);      // --soundtest: the music and sounds run without a device, measured

class Music {
public:
    struct Track {
        std::string file, title;
        std::vector<std::string> only, except;   // "in PLACE,..." or "except PLACE,...": towns or countries
        bool playsIn(const std::string &town, const std::string &country = "") const {
            for (auto &t : except) if (t == town || (!country.empty() && t == country)) return false;
            if (only.empty()) return true;
            for (auto &t : only) if (t == town || (!country.empty() && t == country)) return true;
            return false;
        }
    };
    std::vector<Track> tracks;
    // music for inside a church ("church FILE TITLE"); "nochoir TOWN ..." names the towns whose
    // churches have none (the Reformed, who sing only psalms, and the Ottoman towns, where the
    // buildings of kind church are mosques)
    std::vector<Track> churchTracks;
    std::vector<Track> battleTracks;   // "battle FILE TITLE": while there is fighting near you
    // music inside an inn ("inn [in|except PLACES] FILE TITLE": a lute in the taverns), and the
    // ordinary tracks can be given places too ("track in England FILE TITLE")
    std::vector<Track> innTracks;
    void setInn(bool in, const std::string &town, const std::string &country, double now);
    // music inside a theatre ("theatre [in|except PLACES] FILE TITLE": a consort at the Globe)
    std::vector<Track> theatreTracks;
    void setTheatre(bool in, const std::string &town, const std::string &country, double now);
    void setPlace(const std::string &town, const std::string &country) { placeTown = town; placeCountry = country; }
    std::vector<std::string> noChoir;
    bool choirIn(const std::string &town) const {
        for (auto &t : noChoir) if (t == town) return false;
        for (auto &t : churchTracks) if (t.playsIn(town)) return true;
        return false;
    }
    // you have gone into (or out of) a church of TOWN: the music of the other place fades away,
    // and that of this place begins after a moment
    void setChurch(bool in, const std::string &town, double now);
    void setBattle(bool on, double now);   // a fight near you begins (or is over)
    // pieces someone plays when something happens ("piece ID FILE TITLE"; the effect "play ID")
    struct Piece { std::string id, file, title; };
    std::vector<Piece> pieces;
    const Piece *findPiece(const std::string &id) const {
        for (auto &p : pieces) if (p.id == id) return &p;
        return nullptr;
    }
    // play a piece now, in place of the background music (which comes back after it); its
    // length in seconds goes to *seconds. False if there is no such piece, or music is off.
    bool playPiece(const std::string &id, double now, double *seconds = nullptr);
    // a cue ("cue ID FILE TITLE", kept with the pieces): a few bars at full level, the background
    // fading away under it (over FADESECS) instead of stopping; the background comes back after
    // it, as after a piece. False if there is no such cue, or music is off.
    bool playCue(const std::string &id, double now, double fadeSecs);
    void setPieceLevel(float level);   // 0..1: how near the player is to whoever plays it
    double pieceSeconds(const std::string &id) const;   // its length, without playing it (0: none)
    bool piecePlaying() const;
    double gap = 30;          // seconds of silence between two pieces
    float volume = 0.7f;

    bool load(const std::string &playlist);   // false if there is no playlist
    void update(double now);                  // starts the next piece when it is time
    void setOn(bool on, double now);
    bool on() const { return on_; }
    // the title of a piece that has just begun (once), for the status line
    std::string takeAnnouncement();
    ~Music();

private:
    void *player = nullptr;   // the piece playing now (a miniaudio ma_sound)
    void *piece = nullptr;    // a piece someone is playing
    void *fading = nullptr;   // one fading away (going into or out of a church)
    double fadeEnd = 0;
    bool inChurch = false, inBattle = false, inInn = false, inTheatre = false;
    int nextBattle = 0, nextInn = 0, nextTheatre = 0;
    std::string innTown, innCountry, placeTown, placeCountry, theatreTown, theatreCountry;
    int mode() const;
    void switchTo(int to, double now);
    std::string churchTown;
    int nextChurch = 0;
    float pieceLevel = 1;
    bool on_ = true;
    int next = 0;
    double nextStart = 2;     // a moment of quiet when the game begins
    std::string announce;
    void start(const Track &t);
    void stop();
};
