// hud.h: text and the overview map, drawn with cairo into an overlay image.
#pragma once
#include <cairo.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "world.h"

struct HudLabel {
    float x, y;          // screen pixels
    std::string text;
    bool bubble;         // something said, rather than a name
};

struct HudPuff { float x, y, r, a; int seed; };   // a puff of gun smoke: screen pixels, radius, opacity

// how far the conversation's box can scroll, in rows (from its first place), and a boxful's rows: hud.cc sets them
// each time it draws a conversation; the keys (main.cc) keep within them
extern int hudTalkScrollMin, hudTalkScrollMax, hudTalkPageRows;

struct HudState {
    std::vector<HudPuff> puffs;
    std::set<std::string> beenTowns;   // the towns you have been to ("paris", "the-post-house-at-cambrai"): the map names only these
    float fade = 0;             // 0..1: the screen going to black and back (a night's sleep, Game::restFor)
    std::string place, date, coords, message;
    float health = 1;           // your health, 0..1, shown as a bar while it is not full (or you fight)
    bool showHealth = false;
    bool help = false, map = false;
    bool testing = false;       // --testing: the help lists the keys for testing (N, ] [, Tab)
    float px = 0, py = 0;   // player, world tiles
    std::vector<HudLabel> labels;
    // a conversation
    bool talking = false;
    std::string talkTitle, talkInput;
    std::vector<std::pair<bool, std::string>> talkLines;   // (spoken by them?, text)
    int talkScroll = 0;   // rows of text looked back (Up) or on (Down) from where a conversation's box first stands
    // a panel: 1 what you carry (lines, and the text of the one chosen), 2 reading (title, text)
    int panel = 0;
    std::string panelTitle, panelText, panelHint, panelImage, panelCredit;   // image: a PNG beside the text
    std::vector<std::string> panelLines;
    // panel 3: an open book, two pages at a time (pages bookFirst and bookFirst + 1, counted from 0)
    std::string bookTitle, bookNote, bookLeft, bookRight, bookLeftImage, bookRightImage, bookLeftCredit, bookRightCredit;
    int bookFirst = 0, bookPages = 0;
    // what you have overheard (J)
    bool journal = false;
    std::vector<std::pair<bool, std::string>> journalLines;  // (a heading?, text)
    int journalTab = 0;                                       // 0 the stories you are in, 1 what you have overheard
    std::vector<std::pair<int, std::string>> storyLines;      // People::journal (KIND % 10, and its mark KIND / 10)
    bool journalDetailed = false;                             // marked, the newest first (Tab)
    int journalPage = 0;                                      // which page of the stories (the arrows)
    struct TownPlan {                                         // a town's plan in the journal (Hud::setPlan has its picture)
        std::string town;
        int x0 = 0, y0 = 0, w = 0, h = 0;                     // world tiles, x0, y0 the south-west corner
        std::vector<std::pair<float, float>> marks;           // the places you have been, numbered from 1
        float youX = -1e9f, youY = -1e9f;                     // you, if you are in that town
    };
    std::vector<TownPlan> townPlans;
    // choosing who you are, at the start of a new game
    bool creating = false;
    int createRow = 0;
    std::vector<std::pair<std::string, std::string>> createRows;   // (what, your choice)
    // a still scene at the start of a new game (data/scenes.txt): its words, which of how many
    std::string caption;
    int sceneNo = 0, sceneCount = 0;
    // a card (data/cards.txt): the whole screen, dark, with these lines centred (true: large type)
    std::vector<std::pair<std::string, bool>> cardLines;
    bool sceneOpening = true;   // the opening scenes (Esc: begin at once), or a beat's (Esc: skip them)
};

struct Hud {
    cairo_surface_t *surf = nullptr;
    // the conversation's highlighted words as last drawn, where they stand (0..1 of the window): a click on one asks it
    struct Word { float x0, y0, x1, y1; std::string text; };
    std::vector<Word> talkWords;
    cairo_surface_t *mapImg = nullptr;
    int w = 0, h = 0;
    float scale = 1;
    std::string lastKey, lastStatic;
    std::map<std::string, cairo_surface_t *> pictures;   // loaded once, kept
    void forgetPicture(const std::string &file) {        // a picture made again (your portrait): load it afresh
        auto it = pictures.find(file);
        if (it == pictures.end()) return;
        if (it->second) cairo_surface_destroy(it->second);
        pictures.erase(it);
    }
    double lastDraw = -1;
    int journalPages = 1;                                // how many pages the stories took, at the last drawing
    std::vector<int> journalJumps;                       // the pages where a part begins (the stories, the finished, each town): up and down
    std::map<std::string, cairo_surface_t *> planImgs;   // each town's plan, one pixel a tile, north up (made once)
    bool hasPlan(const std::string &town) const { return planImgs.count(town) > 0; }
    void setPlan(const std::string &town, int w, int h, const std::vector<uint32_t> &px);
    void makeMapImage(const World &world);
    // returns true if the image changed
    bool draw(const HudState &s, const World &world, int w, int h, float scale, double now = 0);
    unsigned char *data() { return cairo_image_surface_get_data(surf); }
    int stride() { return cairo_image_surface_get_stride(surf); }
};
