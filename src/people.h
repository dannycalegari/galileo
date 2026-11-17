// people.h: the people of the world, their memories, their days, and what they say.
//
// Everything a person knows, can do or has done lives in one open-ended memory: a map
// from words to values ("met" -> "1", "skill:smithing" -> "8", "has-string" -> "1").
// The data files (data/people/) test and change these words in conditions and effects;
// the program gives no word a fixed meaning except a few it keeps itself (met, talks,
// last, asked:TOPIC), so new skills, facts and, later, items need no new code.
#pragma once
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <unordered_set>
#include <set>
#include <string>
#include <vector>
#include "vmath.h"
#include "flatmap.h"
#include "world.h"

#include "mesh.h"
using Costume = FigureLook;
class Items;

using Memory = std::map<std::string, std::string>;

struct TalkLine {
    std::vector<std::string> keys;     // keywords; "hello", "bye" and "?" (anything else) are special
    std::string cond;                  // "" always
    std::string text;                  // *word* is shown highlighted; $name, $first, $role, $town are filled in
    std::vector<std::string> effects;  // "set KEY", "set KEY=VALUE", "add KEY N", "learn SKILL", "player ...", "name", "bye", "play PIECE"
};

struct SchedEntry {
    int minute = 0;                    // minutes after midnight
    std::string activity, place, cond; // e.g. "work", "", "" or "go", "inn", "skill:music"
    bool sharp = false;                // "HH:MM sharp ...": on the minute, for everyone (a play, a mass)
    bool always = false;               // "always ACTIVITY [PLACE] if CONDS": whenever its conditions hold, at any hour,
                                       // it wins over the timetable (the last such that holds); the muster, the storming
};

struct Role {
    std::string id, title;             // "blacksmith", "a smith"
    char sex = 'm';
    Costume look;
    bool hasLook = false;
    std::string work, says;
    std::string sign;                  // "sign EMBLEM": hung over the door of their workshop
    std::vector<SchedEntry> schedule;
    std::vector<TalkLine> talk;
    // the same lines, shared by every person of the role (made when the first person is finished)
    mutable std::shared_ptr<const std::vector<TalkLine>> talkShared;
    std::vector<std::pair<std::string, int>> sells;   // what they sell, and how many a day
    long money = 0;                                     // deniers they start with
    bool watch = false;                                 // of the watch: runs after a thief
    // a soldier ("fight HP SKILL DAMAGE ARMOUR"), of a side ("faction NAME"), attacking on sight
    // those of the sides in "hostile NAME ..." ("player": you)
    int fightHp = 0, fightSkill = 0, fightDmg = 0, fightArmour = 0;
    int recruitMin = 1440;                              // "recruits MINUTES": a dead one's place filled after so long (a battle's fresh men)
    std::string faction;
    std::vector<std::string> hostile;
    std::string names;                                  // "names POOL": their names from this pool, not the town's
};

bool fridayAt(double minutes);   // a Friday (people.cc)
bool lentAt(double minutes);     // in Lent

// What plan() read of a person's memory, and the answer it gave, so that it need not look again until the game
// minute changes or one of those memories does (People::plan)
struct PlanDep { bool ofPlayer; std::string key; bool present; std::string value; };
struct PlanCache {
    bool valid = false;
    bool coarse = true;      // the answer holds for the whole game minute (false: only for the exact time `at`)
    int64_t minute = 0;      // floor of the game minute asked at
    double at = 0;
    bool lying = false;      // the condition "asleep"
    int best = -1;           // the schedule entry that held
    std::vector<PlanDep> deps;
};

struct Person {
    std::string id, name, role, title, town;
    // "title if CONDS: TEXT": the first whose conditions hold is the title (People::update refreshes it); none, baseTitle
    std::vector<std::pair<std::string, std::string>> titleIf;
    std::string baseTitle;
    std::string known;                 // what the town says of them (a person's "known" line), when asked by name
    std::string diedIf, diedText;      // "died if COND | TEXT": dead by the story (Maria Celeste, Morales); then asked of, TEXT, not the way to them
    char sex = 'm';
    Costume look;
    std::string says;
    std::vector<SchedEntry> schedule;
    std::vector<TalkLine> talk;        // their own lines; then the role's (roleTalk), then everyone's (everyoneTalk): talkLists()
    std::shared_ptr<const std::vector<TalkLine>> roleTalk, everyoneTalk;   // shared with the Role, never written
    // the three lists in the order a line is chosen: own, role, everyone
    std::array<const std::vector<TalkLine> *, 3> talkLists() const {
        static const std::vector<TalkLine> none;
        return {&talk, roleTalk ? roleTalk.get() : &none, everyoneTalk ? everyoneTalk.get() : &none};
    }
    std::vector<std::pair<std::string, int>> sells;   // own first, then the role's
    Memory mem;
    // places
    std::string nearHint, workKind, workPlace;
    std::string sign;                  // the emblem on the sign over their workshop's door (roles.txt "sign")
    std::string shopName;              // "shopname NAME": their workshop's house is called so (in AROUND, WALKTO, directions)
    int home = 0;
    bool hasBed = false;
    int bedX = 0, bedY = 0, bedZ = 0;
    bool hasWork = false;
    int workX = 0, workY = 0, workZ = 0;
    // where and what
    int x = 0, y = 0, z = 0, px = 0, py = 0, pz = 0;
    float stepT = 1, stepDur = 0.33f;
    std::vector<std::array<int, 3>> path;
    size_t pathPos = 0;
    int actIndex = -2;
    PlanCache planCache;
    std::string activity, actPlace;
    int goalX = 0, goalY = 0, goalZ = 0;
    bool lying = false, active = false, placed = false;
    // travelling to another town ("travel TOWN"): first to a gate, then along the road
    std::string travelTo;
    bool onRoad = false;
    double arrive = 0;
    float wait = 0, idle = 0, nextSay = 10, facing = 0, walkPhase = 0, bubbleT = 0;
    float pause = 0;   // how long they have stood still because you came up beside them
    float hold = 0;    // seconds to stay where they are (playing a piece, say)
    std::string bubble;
    bool watch = false;   // of the watch (the role's "watch")
    float chase = 0;      // > 0: running after the player, for this many seconds so far
    int chaseSteps = 0;
    float strollDur = 0.66f;   // their own walking pace, back after a chase
    // fighting (main.cc, Game::fight; People::fightStep). Soldiers have the role's "fight"; anyone
    // else, attacked, runs away. Only the anonymous can die: they lie where they fell for two
    // hours, then are gone, and a day later a recruit stands in their place (memory dead-at).
    // A named person brought down yields instead, and runs.
    bool fighter = false;
    int recruitMin = 1440;     // minutes until a recruit stands in his place, dead (the role's "recruits")
    int maxHp = 12, skill = 0, dmg = 2, armour = 0;
    float hp = 12;
    std::string faction;
    std::vector<std::string> hostile;
    int target = -1;           // whom they fight: a person, -2 you, -1 nobody
    float strikeT = 0, strikeAnim = 0, scanT = 0, fleeT = 0;
    bool foe = false;          // you attacked them
    bool dead = false;
    double deadAt = 0;
    bool named() const { return id.find(':', id.find(':') + 1) == std::string::npos; }
    bool away = false;         // the schedule says "away": not here at all (an army that has not come)
    bool recruit = false;      // next placed: they march in along a road (a recruit, or arriving)
    // met on the roads (People::makeWild; Game::wildStep in main.cc): brigands, a hired escort, a
    // gamekeeper. Away until the game puts them somewhere; no timetable, no home, no town
    bool wild = false;
    bool parley = false;       // a brigand coming up to you to talk first (stand and deliver), not to strike
};

// Conversations between people, overheard now and then (data/people/chats.txt).
struct ChatLine {
    int who = 0;                       // 0: A speaks, 1: B speaks
    std::string text;
    std::vector<std::string> effects;  // the speaker's; "hearer ..." the other's; "player ..." yours
};
struct Chat {
    std::string id;
    std::vector<std::string> a, b;     // who may have it: role or person ids, or "anyone"
    std::string where, condA, condB;   // where: part of the place's name
    double again = -1;                 // may be overheard again this many minutes later; < 0: once
    std::vector<ChatLine> lines;
};
struct HeardLine { int conv = 0; double minutes = 0; std::string where, who, text; };

// Things that happen by themselves (data/events.txt): on a day and hour, or as soon as
// their conditions hold, whether or not you are there; each happens once.
struct Event {
    std::string id;
    long at = -1;                      // minutes from the start of the game, or -1: whenever the conditions hold
    std::vector<std::string> conds;    // "WHO CONDITION": a person (town:id) or player
    std::vector<std::string> effects;  // stock, buy, give, set, unset, add, travel, news, wants, makes (see events.txt)
    float chance = 1;                  // "chance P": when its time comes it happens with this chance, or
                                       // never (then the player remembers failed:ID)
};

// A town's plan in the journal: its rectangle, and the places you have been, numbered in order
struct TownPlanInfo {
    std::string town;
    int x0 = 0, y0 = 0, w = 0, h = 0;                // world tiles; x0, y0 the south-west corner
    std::vector<std::pair<float, float>> marks;      // the centres of the places, 1, 2, 3 ...
};

// The stories you are in (data/stories.txt), for the journal: each step is shown once its
// conditions hold (as in events.txt: "player KEY", "town:id KEY", joined by "and"). A step or an
// ending may be marked "well" or "badly" for the main story ("step badly if ...", "done well if
// ..."); a story may have several endings, and the first whose conditions hold is the one shown.
struct StoryStep {
    std::vector<std::string> conds;
    std::string text;
    int mark = 0;          // 1 well, -1 badly, 0 neither
    std::string key;       // in your memory, jnl:STORY:KEY is when it was first shown (the detailed journal)
};
struct Story {
    std::string id, title;
    bool main = false;     // "main": part of the main story (first in the detailed journal while open)
    bool top = false;      // "top": first of all, in both views, and never "(open)" (the end of the story)
    std::vector<StoryStep> steps, dones;
};

// What a town makes (cheap there) and wants (dear there), its customs, its language
// (data/market.txt). Prices: People::priceIn.
struct Market {
    std::map<std::string, float> makes, wants;   // item id -> a factor on its price
    float customs = 0;                            // a part of the value of the goods you bring in
    std::string language;                         // skill:LANGUAGE; without it you pay a tenth more and get a tenth less
    std::set<std::string> forbids;                // goods that may not come in at all (smuggled, or seized)
    std::string forbidWhy;                        // what the searchers say when they find them
    std::map<std::string, std::string> whyOf;     // ... for one thing (an event's "forbid TOWN ITEM | WHY")
    std::set<std::string> searchOut;              // forbidden by an event: searched for going out too, and
                                                  // the shops of the town sell none (the Dialogue seized)
};

struct Conversation {
    int who = -1;
    std::vector<std::pair<bool, std::string>> lines;   // (spoken by the person?, text)
    std::string input;
    bool over = false;
    bool pendingGive = false;   // the prompt asked "(Give what?)": your next words are the thing ("letter")
    bool pendingBuy = false;    // a shop asked "How many?" or "Which will it be?": a number or a ware's name buys
};

class People {
public:
    std::vector<Story> stories;
    std::map<std::string, Market> markets;   // by town, in lower case
    std::set<std::string> goods;             // every thing some market makes or wants: trade goods
    bool loadMarkets(World &w, const std::string &fn);
    long priceIn(const Person &shop, int kind, bool buying, double minutes) const;
    double glut(const std::string &town, const std::string &item, double minutes) const;
    // the customs on entering a town with goods: the duty taken (deniers) and what it was on
    long customs(const std::string &town, std::string &what);
    // the forbidden goods you carry into TOWN (item id, how many), and why they are forbidden
    // (leaving: only what may not go OUT of the town either, the event-forbidden)
    std::vector<std::pair<std::string, long>> contraband(const std::string &town, std::string *why = nullptr, bool leaving = false) const;
    // forbidden by an event ("forbid TOWN ITEM | WHY", searched for both ways), not by market.txt
    bool eventForbidden(const std::string &town, const std::string &item) const;
    // the journal: (KIND, text). KIND % 10: 0 a story's title, 1 a step, 2 how it ended, 3 the title
    // of a finished story; KIND / 10: 0 unmarked, 1 well, 2 badly (marks only in the detailed one),
    // 3 a "top" story's title (no "(open)").
    // Plain: open stories in the file's order, then the finished. Detailed: the open stories, then the
    // finished, the main story on top in each, each newest first, and in each story its newest step first.
    std::vector<std::pair<int, std::string>> journal(double minutes, bool detailed = false) const;
    // remember when each step and ending was first shown (jnl:STORY:KEY), for the detailed journal;
    // at most every five game minutes unless forced
    void noteJournal(double minutes, bool force = false);
    // the journal's pages of places and people, at its back: for each town you have been to (here
    // first, then the others by name), the named places you have stepped into and where they are,
    // and the people you have spoken to there, with where and when they are usually to be found.
    // (KIND, text): 4 a town, 8 its plan (the town's name; the plan in *plans), 5 a heading within
    // it, 6 a line (the places numbered as on the plan), 7 where and when to find the person above.
    std::vector<std::pair<int, std::string>> townPages(World &w, double minutes, const std::string &here,
                                                       std::vector<struct TownPlanInfo> *plans = nullptr) const;
    // where and when someone is usually to be found, from their timetable ("at the Minims, 7:00 to 12:00 ...")
    std::string whereToFind(World &w, const Person &q, double minutes) const;
    // the places of someone's day as it is now, with the stretches of minutes they are there
    typedef std::vector<std::pair<std::string, std::vector<std::pair<int, int>>>> DayPlaces;
    DayPlaces dayPlaces(World &w, const Person &q, double minutes) const;
    // hours in words: "from nine till eleven in the morning"; several stretches "..., and ..."; 20 hours or more "at any hour"
    static std::string spokenSpan(int from, int to);
    static std::string spokenSpans(const std::vector<std::pair<int, int>> &spans);
    // "been:KEY", "visited:KEY:...": a town's name in your memory ("saint-denis")
    static std::string townKey(const std::string &town);
    double journalNoted = -1e18;
    bool condsHold(const std::vector<std::string> &conds, double minutes) const;
    static long dateMinutes(const std::string &date, const std::string &time);   // "1633-04-12", "10:00": game minutes
    // a check (--stats): highlighted *words* that the person saying them has no line for
    std::vector<std::string> unanswered() const;
    std::vector<Person> people;
    std::map<std::string, Role> roles;
    std::map<std::string, std::vector<std::string>> names;
    Memory player;                      // what the player knows and has done
    const Items *items = nullptr;       // the catalogue, for buying, selling and giving
    Conversation talk;
    std::vector<Chat> chats;
    std::vector<HeardLine> heard;       // everything the player has overheard, to read again
    std::string notice;                 // for the status line ("You overhear ..."); the game clears it
    std::string playRequest;            // "play ID" in a conversation: a piece to play; the game takes it
    int postBy = -1;                    // "postjob" / "postdeliver": a packet of the post given or taken in (Game::postJob)
    bool postDeliver = false;
    int horseBy = -1;                   // "hire PRICE" or "buyhorse PRICE": a horse dealt by this person (Game::horseDeal)
    int shipBy = -1;                    // "buyship PRICE": a ship of your own sold by this person (Game::shipDeal)
    long shipPrice = 0;
    std::string shipKind = "pinnace";   // the item of the ship sold (items.txt, shape ship)
    double passMinutes = 0;             // "pass MINUTES": the time a line says it takes (a lesson all afternoon)
    int cowBy = -1;                     // "buycow PRICE": a cow sold you by this person, to lead away (Game::cowDeal)
    bool portraitWanted = false;        // "paintportrait": your portrait is painted (Game::paintPortrait; Marguerite Bahuche's thanks)
    long cowPrice = 0;
    std::string lastSaid;               // the words last said to anyone ("set KEY=$said" keeps them: the cow's name)
    bool horseBuy = false;
    long horsePrice = 0;
    int sleepBy = -1;                   // "sleep" in a conversation (an innkeeper's bed): the game takes it
    int stayBy = -1;                    // "staytillword": the bed kept until word comes (Game::stayTillWord)
    int rideBy = -1;                    // "postride": the post ride offered by this innkeeper or ostler is taken (Game::postRide)
    // an innkeeper's or ostler's words about the post ride, before any line is chosen (Game::rideOffer): true with
    // the reply when the words name a town and ask for, or answer the offer of, a ride
    std::function<bool(int who, const std::string &words, std::string &reply)> rideAsk;
    static std::vector<std::string> nameWords(const std::string &s);   // lower case, unaccented, as words
    // what the clock drives, once, for a coarse step of the clock (Game::advanceClock): far-away travel,
    // the events and the journal; people are not walked
    void advanceStep(World &w, double minutes, int ptx, int pty);
    double nextEventTime(double minutes) const;   // the hour of the next "on" event not yet done, or -1
    // "passage PRICE MINUTES PLACE": a boat across to PLACE (the Dover packet), for PRICE deniers
    // and as much again for your horse, taking MINUTES (Game::takePassage)
    int changeBy = -1;                  // "change [COIN]": a money-changer changes your coin (Game::changeMoney)
    std::string changeTo;
    int passageBy = -1;
    int ferryBy = -1;   // a waterman rows you across the river ({ferry}: Game::ferry)
    long passagePrice = 0;
    int passageMinutes = 0;
    std::string passageTo;
    int playBy = -1;                    // who plays it
    // something you took in front of its keeper (main.cc, Game::takeThing): "pay" or
    // "give ITEM" to the keeper settles it
    struct Debt { std::string keeper; int kind = -1, count = 0; long price = 0; bool paid = false, returned = false; } debt;
    int caught = -1;                    // the one of the watch who has caught you (the game deals with it)
    int lostBy = -1;                    // ... or who has given up the chase
    std::map<std::string, std::string> prisons;   // town -> "NAME | what happens" (a town file's "prison")
    // A place you may enter only with leave (a town file's "guarded NAME ... end", see roles.txt):
    // while its guard is in the building he turns you back unless the "pass" conditions hold;
    // while he is away it is locked. Carrying its key opens it either way (main.cc, Game::mayEnter).
    struct Guarded {
        std::string town, place, by, key, stop, shut;
        std::vector<std::string> pass, stopEffects;
    };
    std::vector<Guarded> guarded;
    std::vector<Event> events;
    // the story's beats: an event's "jump YYYY-MM-DD HH:MM [BOOK]" moves the calendar on to then
    // (never back) and opens BOOK, a letter (Game::takeRequests); "ending" shows the ending,
    // made from data/ending.txt: pages of lines, each shown if its conditions hold
    long jumpTo = -1;
    std::string jumpPage;
    std::string scenesRequest;   // events.txt "scenes ID": still scenes (data/scenes/ID.txt) seen now, the clock kept
    std::string cardRequest;   // events.txt "card ID": a chapter or date card (data/cards.txt) is to be shown
    bool endingRequest = false;
    // events.txt "pass MINUTES" (the clock moves on), "carry X Y [beside WHO]" (you are taken to the world tile X,Y, and
    // WHO is put beside you: the march to the Alte Veste); "wake" (a news line of the event, told to you, ends a wait or
    // a night's sleep: the drums). Game::takeRequests carries them out, Game::advanceClock stops for the wake
    double eventPass = 0;
    int carryX = -1, carryY = -1;
    std::string carryBeside;
    bool wakeRequest = false;
    // events.txt "cue TOWN|anywhere ID [LEVEL]": a cue of data/music.txt played if you are there (the drums of the
    // muster: the English March), at LEVEL (0..1) of full; Game::takeRequests plays it
    std::string cueRequest;
    float cueLevel = 1;
    // events.txt "view YAW": the camera turned to look from that side (the march: the storming seen from the north, the
    // town behind it); Z turns it back. Game::takeRequests
    float viewYaw = -1000;
    std::vector<std::array<float, 2>> gunSmoke;   // where guns have just fired (a volley): Game makes the boom and the smoke
    // events.txt "guns TOWN every MINUTES damage D range R / if CONDS / end": while the conditions hold, the guns of the
    // town's camp fire now and then, each on the nearest of the King of Sweden's men (or you, on his side) in front of it
    // within R tiles: People::gunsStep
    struct Cannonade { std::string town; float every = 2.5f, damage = 9, range = 14; std::vector<std::string> conds; double next = -1; };
    std::vector<Cannonade> cannonades;
    // events.txt "musketry TOWN every MINUTES volleys MINUTES / if CONDS / end": while they hold, a musket goes off now
    // and then among the soldiers of the town, and now and then a rank fires a ragged volley (sound only: the fights
    // are hand to hand)
    struct Musketry { std::string town; float every = 0.8f, volleys = 8; std::vector<std::string> conds; double nextShot = -1, nextVolley = -1; };
    std::vector<Musketry> musketries;
    // sounds of the fighting for Game to play, from where they happen: "musket", "volley", "clash" (steel), "knock" (wood)
    struct SoundAt { std::string kind; float x, y; };
    std::vector<SoundAt> soundsAt;
    void gunsStep(World &w, double minutes, int ptx, int pty, int ptz);
    struct EndingLine { std::string page; std::vector<std::string> conds; std::string text; };
    std::vector<EndingLine> ending;
    std::string endingTitle = "What became of them";
    bool loadEnding(const std::string &fn);
    void marketEffects(const Event &e);   // its "wants TOWN ITEM F" / "makes TOWN ITEM F" / "forbid TOWN ITEM | WHY" /
                                          // "allow TOWN ITEM" (again, after loading a save)
    Person *byId(const std::string &id);
    bool startChase(int i);
    void stopChases();
    bool chasing() const;
    bool echo = false;                  // print what is overheard (tests)
    float earshot = 8, chatChance = 0.05f;
    double chatGap = 180, pairGap = 1440;

    // fighting: blows struck at you, for the game to take from your health; what happened
    float playerHurt = 0;
    std::vector<std::string> fightNotes;
    bool playerDown = false;            // you lie beaten: nobody attacks you
    // the road's people (Game::wildStep): their indices; a brigand who has come up to you to talk
    std::vector<int> band;
    int escortIdx = -1, keeperIdx = -1;
    int parleyArrived = -1;
    int makeWild(const std::string &role, int n, const std::string &id);   // n of a role, away; the first index
    // what a conversation asked the game to do on the roads: "robbed", "bandfight", "escort PRICE",
    // "billdraw", "billcash" (with the person's index)
    std::vector<std::pair<std::string, int>> roadRequests;
    int playerSkill = 0, playerArmour = 0;   // set by the game: your fencing and your armour
    int roll(int attSkill, int dmg, int defSkill, int defArmour);   // a blow: the damage, or -1 a miss
    void hurt(int victim, float dmg, int by, double minutes);       // by: a person, or -2 you
    void die(Person &p, double minutes);
    bool load(World &w, const std::string &dir);
    void update(World &w, float dt, double minutes, int ptx, int pty, int ptz);
    int adjacentTo(int X, int Y, int Z, float facing) const;   // a person beside the player, or -1
    bool occupied(int X, int Y, int Z) const;                  // someone standing here?
    // you squeeze past the one standing at (toX,toY): they step into your place (youX,youY);
    // their label, or "" if nobody there can be passed (asleep, in bed, fighting, a soldier on guard)
    std::string squeezePast(int youX, int youY, int toX, int toY, int z);
    std::string label(const Person &p) const;                   // name if known, else trade

    // conversation
    void startTalk(World &w, int who, double minutes);
    void ask(World &w, const std::string &words, double minutes, int youX = 0, int youY = 0);
    // what townspeople know, when none of a person's lines answers: where the town's named places
    // are, who its named people are, and what the word for their own trade means
    std::map<std::string, std::string> trades;   // data/people/trades.txt: word -> what it means
    void loadTrades(const std::string &fn);
    bool knowAnswer(World &w, const Person &p, const std::string &words, bool fullOnly, int youX, int youY, double minutes, std::string &out) const;
    bool tradeAnswer(const Person &p, const std::string &words, double minutes, std::string &out) const;
    void endTalk(double minutes);

    // saving
    void save(std::ostream &out) const;
    void loadSave(std::istream &in);

private:
    struct ChatNow { int chat = -1, a = -1, b = -1; size_t line = 0; float t = 0, dur = 0; int conv = 0; };
    ChatNow chatNow;
    float chatClock = 0;
    bool inChat(int i) const { return chatNow.chat >= 0 && (chatNow.a == i || chatNow.b == i); }
    bool loadChats(const std::string &fn);
    bool fits(const Person &p, const std::vector<std::string> &who) const;
    void tryChat(World &w, double minutes, int ptx, int pty, int ptz);
    bool fightStep(World &w, Person &p, int i, float dt, int ptx, int pty, int ptz, double minutes);
    int findFoe(const Person &p, int i, int ptx, int pty, int ptz) const;
    bool arriveFromRoad(World &w, Person &p, int gx, int gy);
    void pitchCamps(World &w);
    void stepToward(World &w, Person &p, int i, int gx, int gy, int gz, float dur);
    void chatLine(World &w, double minutes, int ptx, int pty, int ptz);
    void endChat(double minutes);
    FlatIntMap occ{2048};         // who stands where: key3 -> index of the person (-1: you); rebuilt at each update, then kept as they step
    FlatIntMap goalTiles{2048};   // where people are walking to: not chosen by another (rebuilt each update)
    bool taken(int X, int Y, int Z) const { int64_t k = World::key3(X, Y, Z); return occ.count(k) > 0 || goalTiles.count(k) > 0; }
    void placedAt(Person &p, int oldX, int oldY, int oldZ);   // put straight somewhere: the tile is theirs at once
    int64_t avoid = INT64_MIN;          // a tile findPath must not use (someone standing in the way)
    // findPath's working memory, kept between searches: the nodes seen (cost so far, the node it came from,
    // where it is), a table from tile key to node, and the heap of nodes to look at
    struct PathNode { float g; int32_t parent; int32_t x, y, z; };
    struct PathHeapItem { float f; int32_t node; };
    std::vector<PathNode> pathNodes;
    std::vector<PathHeapItem> pathHeap;
    FlatIntMap pathIndex{1 << 14};
    FlatIntMap probeSeen{1 << 12};                 // findPath's backward probe: the tiles that can lead to the goal
    std::vector<std::array<int, 3>> probeQueue;
    double lastMinutes = -1;
    bool trade(Person &p, const std::string &words, double minutes);   // buy, sell, give; true if answered
    long haggled(long price, bool buying) const;
    void restock(Person &p, double minutes);                            // a shop's stock each new day
    float carriedWeight() const;
    bool loadRoles(const std::string &fn);
    bool loadNames(const std::string &fn);
    bool loadTown(World &w, const std::string &fn);
    void assign(World &w);
    void lodge(World &w, Person &p);    // a bed at an inn of their (new) town
    void travelStep(World &w, Person &p, double minutes);
    void chaseStep(World &w, Person &p, int i, float dt, int ptx, int pty, int ptz);
    bool loadEvents(const std::string &fn);
    bool loadStories(const std::string &fn);
    void runEvents(World &w, double minutes, int ptx, int pty);
    void wantsStep(double minutes, int ptx, int pty, int ptz);
    float eventClock = 0;
    void plan(World &w, Person &p, double minutes);
    bool findPath(World &w, int sx, int sy, int sz, int gx, int gy, int gz, std::vector<std::array<int, 3>> &out, int limit = 60000);
    bool placeFor(World &w, Person &p, const std::string &activity, const std::string &place, int &gx, int &gy, int &gz);
    bool holds(const Person &p, const std::string &cond, double minutes) const { return holdsRec(p, cond, minutes, nullptr); }
    // the same; with `rec`, it also notes each memory it looked at (and whether the time was used finely)
    bool holdsRec(const Person &p, const std::string &cond, double minutes, PlanCache *rec) const;
    bool planCacheHolds(const Person &p, const PlanCache &c) const;   // nothing it read has changed
    void apply(Person &p, const std::vector<std::string> &effects);
    std::string fill(const Person &p, const std::string &text, const Person *other = nullptr) const;
    const TalkLine *match(const Person &p, const std::string &word, double minutes) const;
    // the words of the places' names, four letters or more (load): a talk key takes them only whole,
    // never by their first four letters ("venice" is not "venison", "cambrai" not "cambridge")
    std::set<std::string> townWords;
};
