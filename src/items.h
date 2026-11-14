// items.h: things. What lies about in the world (on tables, stalls, the floor), what
// you carry, and works of art. The catalogue and the rules for where things lie are in
// data/items.txt. What you carry lives in your memory, as "has:ID" = how many, so that
// the conditions and effects of the people's files can test and change it.
#pragma once
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "mesh.h"
#include "world.h"

struct ItemKind {
    std::string id, name, plural;      // "a loaf of bread", "loaves of bread"
    std::vector<std::string> also;     // "also paper injunction": other words a player may call it by
    std::string look, title;           // what you see; for works of art, a heading
    std::string image, credit;         // works of art: a picture (full path) and where it comes from
    // a second state, once you remember afterKey ("after leda-restored image ..."): the work mended
    std::string afterKey, afterImage, afterLook, afterCredit;
    // using it (U): eat, drink (one is gone), read (a page to read), say (just the text)
    std::string useVerb, useTitle, useText;
    std::vector<std::string> useEffects;
    // skills and making things: "needs SKILL TEXT" (without it you read TEXT instead),
    // "night TEXT" (used at night), "use make RESULT [N] | TEXT" with "with ITEM N" and
    // "takes MINUTES", "use play PIECE | TEXT" (you play a piece of data/music.txt)
    std::string needs, needsText, nightText, makes, play;
    std::vector<std::string> nightEffects;   // effects of using it at night
    std::string nearKind, nearText;          // "near KIND TEXT": only within two tiles of such a thing
    std::string atPlace, atText;             // "at \"PLACE\" TEXT": only where the place's name contains PLACE
    int makesN = 1, takes = 0;
    int heals = 0;                     // "heals N": eating it mends this many health points (if you are hurt)
    std::vector<std::pair<std::string, int>> with;
    std::string shape = "box";
    vec3 color{0.6f, 0.5f, 0.4f}, color2{0.85f, 0.8f, 0.7f};
    float weight = 0.2f;               // pounds
    int price = 0;                     // deniers
    int money = 0;                     // taking it gives you this many deniers
    bool fixed = false;                // cannot be taken
    int weapon = 0;                    // "weapon N": the most damage a blow with it does (you fight with your best)
    int armour = 0;                    // "armour N": taken off every blow that strikes you (you wear your best)
    bool chained = false;              // "chained": fixed, and (with a book) read where it lies by looking at it (L)
    std::string book;                  // "book ID": it opens as a book of pages (data/books/ID.txt)
    bool boat = false;                 // shape boat or gondola: you get in it and row; ship: you sail it (main.cc)
};

// A book you can open and leaf through (data/books/ID.txt, named by an item's "book ID" line).
// Each page is in the book's own language; with that skill you read it in English instead,
// and only then do the page's effects happen (a plot flag is set only once it is understood).
struct BookPage {
    std::string orig, english;         // the text as printed, and in English
    std::string image, credit;         // a page that is a picture (full path) and where it comes from
    std::vector<std::string> effects;  // on you, when the page is read and understood
};
struct Book {
    std::string id, language = "english", title, titleEn;
    std::vector<BookPage> pages;
};

struct Thing {
    int kind = 0, count = 1;
    int count0 = -1;                   // how many the world was made with (a stack): a saved game keeps the difference
    int x = 0, y = 0, z = 0;
    float h = 0;                       // height above the floor (on a table: its top)
    float ox = 0.5f, oy = 0.5f;        // where in the tile
    float facing = 0;
    int side = -1;                     // hung on a wall: 0..3, the wall to the west, east, south, north
    int owner = 0;                     // the building it belongs to (0: nobody's)
    uint32_t key = 0;                  // things the world was made with: a name for the saved game
    bool gone = false;
};

class Items {
public:
    std::vector<ItemKind> kinds;
    std::map<std::string, int> byId;
    std::vector<Thing> things;
    int version = 0;                   // changes whenever something is taken or put down

    bool load(const std::string &fn);
    std::map<std::string, Book> books;
    bool loadBooks(const std::string &dir);   // every data/books/*.txt
    void place(World &w);              // follow the rules of where things lie
    std::vector<int> at(int X, int Y, int Z) const;   // things on this tile, bottom first
    int find(const std::string &id) const { auto it = byId.find(id); return it == byId.end() ? -1 : it->second; }
    std::string nameOf(int kind, int n) const;        // "a loaf of bread", "3 loaves of bread"
    void take(int i);                                 // the whole of thing i goes
    void put(World &w, int kind, int n, int X, int Y, int Z);   // on the floor, or on the furniture there
    void buildMesh(World &w, MeshBuf &out, int x0, int y0, int x1, int y1, int z) const;
    // the kind a word names ("bread", "strings", "wine"), as keywords match: id first, then the
    // words of its name after "of" (so "strings" is the lute strings, not "strings of onions")
    // pass -1: both passes; 0: a whole word only; 1: by the first four letters only
    int match(const std::string &word, int onlyPass = -1) const;
    static constexpr float kCarry = 60;   // pounds you can carry
    void save(std::ostream &out) const;
    void loadSave(std::istream &in);
    static float surface(int feature);                // height of a table top, a counter ...
    int boatAt(int X, int Y) const;                   // a boat on this tile of water: its thing, or -1
    void moor(int kind, int X, int Y, float facing);  // a boat left on this tile of water
    // the nearest water beside land within RADIUS of CX,CY with room along the shore for a hull, and
    // no boat on it: where a ship can lie at a quay (false: none)
    bool shoreSpot(World &w, int cx, int cy, int radius, int &bx, int &by, float &facing) const;

private:
    std::unordered_map<int64_t, std::vector<int>> index;
    std::vector<std::string> rules;
    void add(const Thing &t);
    void reindex();
};

// a boat or a gondola at p (on the water), its bow pointing along the facing
void drawBoat(MeshBuf &m, const ItemKind &k, vec3 p, float facing);
// the name of a piece of furniture, for looking at it ("a table"); "" for none
std::string featureName(int feature);
// a sum in deniers, in livres, sous and deniers ("1 livre, 3 sous")
std::string moneyString(long deniers);
