#!/usr/bin/python3
"""The scripted check that ends every stage of work on Galileo.

    /usr/bin/python3 tools/check.py [--quick] [--strict] [-v] [--only stats|lint|scenarios] [--scenario NAME]

Four parts (standard library only; prints a short report; exits 1 on any FAIL):
  1. ./galileo --stats           fails on the problems it marks (unanswerable highlighted words,
                                 overlapping buildings, missing people, ...), except the known ones
                                 listed in tools/check/stats_known.txt, which are WARNs
  2. the plot lint over data/    flags tested but never set; presence-gated beats; places reached
                                 only by sea; [[PROSE: ...]] placeholders
  3. stand-in scripts            the scenarios of tools/check/scenarios.txt, each run on a COPY of a
                                 fixture save (tools/check/fixtures/) with tools/play.sh
  4. a summary                   PASS/WARN/FAIL per part and the time taken

--quick skips part 3.  --strict makes unwritten prose placeholders FAIL.  -v lists every WARN and INFO.
It never touches saves/autosave.txt (its checksum is shown at the start and the end).
"""
import glob
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "data")
SRC = os.path.join(ROOT, "src")
CHECKDIR = os.path.join(ROOT, "tools", "check")
FIXTURES = os.path.join(CHECKDIR, "fixtures")
SCENARIOS = os.path.join(CHECKDIR, "scenarios.txt")
STATS_KNOWN = os.path.join(CHECKDIR, "stats_known.txt")
PROSE_TODO = os.path.join(ROOT, "alpha", "PROSE_TODO.md")
AUTOSAVE = os.path.join(ROOT, "saves", "autosave.txt")

VERBOSE = False
STRICT = False
CAP = 12   # lines shown per WARN/INFO group unless -v


# ------------------------------------------------------------------ reporting

class Part:
    def __init__(self, name):
        self.name = name
        self.counts = {"PASS": 0, "WARN": 0, "FAIL": 0}
        self.infos = 0
        self.t0 = time.time()
        self.secs = 0.0
        print("\n== %s" % name)

    def line(self, level, text):
        if level in self.counts:
            self.counts[level] += 1
        else:
            self.infos += 1
        print("  %-4s %s" % (level, text))

    def group(self, level, title, items):
        """One count for the group; its items listed below (capped)."""
        if not items:
            return
        self.line(level, "%s (%d)" % (title, len(items)))
        shown = items if VERBOSE or len(items) <= CAP else items[:CAP]
        for it in shown:
            print("         %s" % it)
        if len(shown) < len(items):
            print("         ... and %d more (-v lists all)" % (len(items) - len(shown)))

    def done(self):
        self.secs = time.time() - self.t0


def md5(path):
    try:
        with open(path, "rb") as f:
            return hashlib.md5(f.read()).hexdigest()
    except OSError:
        return "(none)"


def relpath(p):
    return os.path.relpath(p, ROOT)


# ------------------------------------------------------------------ part 1: --stats

STATS_MARKERS = re.compile(
    r"^\s*(ANIMAL WRONGLY PLACED|BUILDINGS OVERLAP|TRAPPED|RIVER .*DOES NOT REACH THE SEA|ROAD STOPS AT WATER|"
    r"NO ROOM INSIDE|NOT REACHABLE|NO ROAD reaches|.*\bNOT PLACED\b)")
STDERR_PROBLEMS = re.compile(r"no (person|thing|role or person|role|item) called|cannot (read|open)|unreadable|unknown ", re.I)


def read_known():
    known = []
    if os.path.exists(STATS_KNOWN):
        for ln in open(STATS_KNOWN, encoding="utf-8"):
            ln = ln.rstrip("\n")
            if ln.strip() and not ln.lstrip().startswith("#"):
                known.append(ln.strip())
    return known


def run_stats():
    part = Part("Part 1: ./galileo --stats")
    exe = os.path.join(ROOT, "galileo")
    if not os.path.exists(exe):
        part.line("FAIL", "no ./galileo binary (run make)")
        part.done()
        return part, ""
    try:
        r = subprocess.run([exe, "--stats"], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=300)
    except subprocess.TimeoutExpired:
        part.line("FAIL", "--stats did not finish in 300 s")
        part.done()
        return part, ""
    out = r.stdout.decode("utf-8", "replace")
    if r.returncode not in (0, 1):
        part.line("FAIL", "--stats crashed (exit status %d)" % r.returncode)
    known = read_known()
    new, old = [], []
    for ln in out.splitlines():
        if STATS_MARKERS.search(ln) or STDERR_PROBLEMS.search(ln):
            (old if any(k in ln for k in known) else new).append(ln.strip())
    m = re.search(r"highlighted words that the speaker cannot answer: (\d+)", out)
    if not m:
        part.line("FAIL", "--stats did not print the 'highlighted words' count (output changed?)")
    elif int(m.group(1)) > 0:
        ws = re.findall(r"highlighted words that the speaker cannot answer: \d+\n((?:    .*\n)*)", out)
        words = [w.strip() for w in (ws[0].splitlines() if ws else [])]
        part.group("FAIL", "highlighted words that the speaker cannot answer", words or ["(%s)" % m.group(1)])
    else:
        part.line("PASS", "every highlighted word can be answered")
    m = re.search(r"named buildings drawn over one another: (\d+)", out)
    if not m:
        part.line("FAIL", "--stats did not print the 'drawn over one another' count")
    elif int(m.group(1)) == 0:
        part.line("PASS", "no named buildings drawn over one another")
    # new problems fail; known ones (stats_known.txt) are warnings
    if new:
        part.group("FAIL", "problems --stats reports that are not in stats_known.txt", new)
    else:
        part.line("PASS", "no new problems reported by --stats")
    part.group("WARN", "known --stats problems (tools/check/stats_known.txt)", old)
    # unreachable parts of towns and people without beds/work: counted only
    nobed = len(re.findall(r"^    .*: no bed", out, re.M))
    nowork = len(re.findall(r"no work place '", out))
    part.line("INFO", "people with no bed: %d; with no work place found: %d" % (nobed, nowork))
    part.done()
    return part, out


# ------------------------------------------------------------------ part 2: the plot lint

def data_files():
    """Every text file of data/ that can hold conditions, effects or prose."""
    skip = ("legacy", "art", "music", "sounds")
    files = []
    for dp, dn, fn in os.walk(DATA):
        rel = os.path.relpath(dp, DATA)
        if rel.split(os.sep)[0] in skip:
            dn[:] = []
            continue
        for f in fn:
            if f.endswith(".txt") or f.endswith(".md"):
                files.append(os.path.join(dp, f))
    return sorted(files)


# words People::holds answers from the clock, whoever they are asked of
TIME_WORDS = {"morning", "afternoon", "evening", "night", "asleep", "friday", "lent", "fasting", "hour", "working"}
ATOM_KEY = re.compile(r"^([^\s<>=!]+)\s*(?:(?:>=|<=|!=|=|>|<)\s*\S+)?$")


def player_key(atom):
    """The key tested by one condition atom if it is about the player, else None."""
    a = atom.strip()
    if a.startswith("not "):
        a = a[4:].strip()
    if not a.startswith("player "):
        return None
    a = a[7:].strip()
    if a.startswith("not "):
        a = a[4:].strip()
    if a.startswith("recent "):
        a = a[7:].strip()
    elif a.startswith("recent:") or a.startswith("day:"):   # "recent:N KEY", "day:N KEY", "day:N+ KEY": KEY is a time
        sp = a.find(" ")
        if sp < 0:
            return None
        a = a[sp:].strip()
    m = ATOM_KEY.match(a)
    if not m or m.group(1) in TIME_WORDS:
        return None
    return m.group(1)


def cond_player_keys(cond):
    keys = []
    for atom in re.split(r"\s+and\s+", cond.strip()):
        k = player_key(atom)
        if k:
            keys.append(k)
    return keys


def conditions_in_line(line):
    """The condition strings a data line may hold (the caller keeps only 'player KEY' atoms)."""
    s = line.strip()
    out = []
    m = re.match(r"^(?:if|ifb)\s+(.*)$", s)
    if m:
        out.append(m.group(1))
    # a talk line, a story step, an ending line: KEYS if COND: TEXT (the first ': ' ends the head)
    ix = s.find(": ")
    head = s[:ix] if ix >= 0 else (s[:-1] if s.endswith(":") else None)
    if head is not None and " if " in head:
        out.append(head.split(" if ", 1)[1])
    elif ix < 0 and " if " in s:
        # a schedule line (H:MM go inn if COND) or a guard's "pass if COND": the condition runs to the end
        out.append(s.rsplit(" if ", 1)[1] if re.match(r"^\d+:\d+\s", s) or s.startswith("pass ") else "")
    elif head is not None and re.match(r"^\d+:\d+\s", s):
        pass
    # a schedule line has ':' in the time: "6:00 go inn if COND" has no ': ', handled above
    return [c for c in out if c]


EFFECT_SET = re.compile(r"^player\s+(set|add)\s+([^\s=]+)")
EFFECT_LEARN = re.compile(r"^player\s+learn\s+(\S+)")


def effects_in_line(line):
    """Player keys set by this line: {player set K}, {player add K N}, and events' set/add player K."""
    keys = []
    for body in re.findall(r"\{([^{}]*)\}", line):
        for part in body.split(";"):
            part = part.strip()
            m = EFFECT_SET.match(part)
            if m:
                keys.append(m.group(2))
            m = EFFECT_LEARN.match(part)
            if m:
                keys.append("skill:" + m.group(1))
    m = re.match(r"^\s*(set|add)\s+player\s+([^\s=]+)", line)
    if m:
        keys.append(m.group(2))
    return keys


BASE_FAMILIES = ["has:", "been:", "visited:", "seized:", "event:", "failed:", "knows:", "read:", "saw:", "thief:",
                 "wanted:", "glut:", "glutAt:", "jnl:", "skill:", "heard:", "chat:", "me:", "portrait:", "post:",
                 "horse:", "cow:", "led:", "ship:", "bill:", "coin:"]


def code_set_keys():
    """Keys the C++ code writes to the player's memory: (exact keys, prefix families)."""
    exact, fams = set(), set(BASE_FAMILIES)
    wr = re.compile(r"player\[((?:[^\[\]]|\[[^\]]*\])*)\]\s*(?:=(?!=)|\+=|\+\+)")
    for path in glob.glob(os.path.join(SRC, "*.cc")) + glob.glob(os.path.join(SRC, "*.mm")) + glob.glob(os.path.join(SRC, "*.h")):
        text = open(path, encoding="utf-8", errors="replace").read()
        for m in wr.finditer(text):
            inner = m.group(1)
            lits = re.findall(r'"([^"]*)"', inner)
            if "+" in inner:
                for l in lits:
                    fams.add(l)
            else:
                exact.update(lits)
        # for (const char *k : {"a", "b"}) people.player[k] = ...
        for m in re.finditer(r"for \(const char \*k : \{([^}]*)\}\)\s*(?:\n\s*)?[\w.]*player\[k\]", text):
            exact.update(re.findall(r'"([^"]*)"', m.group(1)))
        # bare string literals ending in ':' near player memory are families too
        for ln in text.splitlines():
            if "player" in ln:
                for l in re.findall(r'"([a-zA-Z\-]+:)"', ln):
                    fams.add(l)
    return exact, fams


def is_code_set(key, exact, fams):
    return key in exact or any(key.startswith(f) for f in fams)


class Occ:
    def __init__(self, path, lineno, text):
        self.path, self.lineno, self.text = path, lineno, text

    def where(self):
        return "%s:%d" % (relpath(self.path), self.lineno)


def scan_keys(files):
    tested, setby = {}, {}
    for path in files:
        if path.endswith(".md"):
            continue
        for i, line in enumerate(open(path, encoding="utf-8", errors="replace").read().splitlines(), 1):
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            for c in conditions_in_line(line):
                for k in cond_player_keys(c):
                    tested.setdefault(k, []).append(Occ(path, i, s))
            for k in effects_in_line(line):
                setby.setdefault(k, []).append(Occ(path, i, s))
    return tested, setby


def lint_flags(part, files):
    tested, setby = scan_keys(files)
    exact, fams = code_set_keys()
    never = []
    for k in sorted(tested):
        if k in setby or is_code_set(k, exact, fams):
            continue
        # the program itself sets "town" for the player and "money" etc. via the exact list above
        occ = tested[k]
        never.append("%s  (tested at %s%s)" % (k, occ[0].where(), " and %d more" % (len(occ) - 1) if len(occ) > 1 else ""))
    unused = [k for k in sorted(setby) if k not in tested and not any(k.startswith(f) for f in ("jnl:",))]
    if never:
        part.group("FAIL", "player keys tested but set nowhere (data or code)", never)
    else:
        part.line("PASS", "every player key tested is set somewhere (%d keys tested, %d set in data)" % (len(tested), len(setby)))
    if unused:
        part.line("INFO", "%d player keys set in data but never tested (the flags of plot beats are often only"
                  " read by people's memory words)" % len(unused))
        if VERBOSE:
            for k in unused:
                print("         %s  (set at %s)" % (k, setby[k][0].where()))
    return tested, setby


# ---- events, stories, talk lines

class Event:
    def __init__(self, id_, lineno):
        self.id, self.lineno, self.on, self.conds, self.news, self.effects = id_, lineno, None, [], [], []


def parse_events():
    path = os.path.join(DATA, "events.txt")
    events, cur = [], None
    for i, line in enumerate(open(path, encoding="utf-8").read().splitlines(), 1):
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        w = s.split(None, 1)
        if w[0] == "event":
            cur = Event(w[1].strip(), i)
            events.append(cur)
        elif cur is None:
            continue
        elif w[0] == "end":
            cur = None
        elif w[0] == "on":
            cur.on = w[1].split()
        elif w[0] == "if":
            cur.conds += re.split(r"\s+and\s+", w[1].strip())
        elif w[0] == "news":
            cur.news.append(w[1] if len(w) > 1 else "")
        else:
            cur.effects.append(s)
    return events


def on_key(ev):
    return " ".join(ev.on) if ev.on else ""


def story_texts():
    path = os.path.join(DATA, "stories.txt")
    out = []
    for i, line in enumerate(open(path, encoding="utf-8").read().splitlines(), 1):
        s = line.strip()
        m = re.match(r"^(step|done)\b(?:\s+(?:well|badly))?\s+(?:if\s+.*?)?: (.*)$", s)
        if m:
            out.append((i, m.group(2)))
    return out


def talk_lines(files):
    """(path, lineno, text, set-keys) for every talk/chat line with its player effects."""
    out = []
    for path in files:
        if path.endswith(".md") or os.sep + "people" + os.sep not in path:
            continue
        for i, line in enumerate(open(path, encoding="utf-8", errors="replace").read().splitlines(), 1):
            s = line.strip()
            if ": " in s and not s.startswith("#"):
                ks = effects_in_line(s)
                if ks:
                    out.append((path, i, s, ks))
    return out


def lint_presence(part, files):
    events = parse_events()
    stories = story_texts()
    talks = talk_lines(files)
    # who sets which flag, with the words said
    gated = []
    for ev in events:
        places = set()
        for c in ev.conds:
            m = re.match(r"^player\s+town\s*=\s*(.+)$", c.strip())
            if m:
                places.add(m.group(1).strip())
        if places:
            gated.append((ev, places))
    warns, oks = [], 0
    for ev, places in gated:
        flags = set()
        for c in ev.conds:
            for k in cond_player_keys(c):
                if k != "town":
                    flags.add(k)
        for place in sorted(places):
            pat = re.compile(re.escape(place), re.I)
            sources = []
            for e2 in events:
                if e2 is ev:
                    continue
                for n in e2.news:
                    if pat.search(n):
                        later = bool(ev.on and e2.on and on_key(e2) > on_key(ev))
                        sources.append(("news of %s" % e2.id, later))
                        break
            for ln, t in stories:
                if pat.search(t):
                    sources.append(("journal line events/stories.txt:%d" % ln, False))
                    break
            for path, ln, text, ks in talks:
                if flags & set(ks) and pat.search(text):
                    sources.append(("line %s:%d" % (relpath(path), ln), False))
                    break
            good = [s for s in sources if not s[1]]
            if good:
                oks += 1
                if VERBOSE:
                    print("         event %s needs %s: named by %s" % (ev.id, place, good[0][0]))
            elif sources:
                warns.append("event %s (events.txt:%d) needs you in %s; only a later letter names it (%s)"
                             % (ev.id, ev.lineno, place, sources[0][0]))
            else:
                warns.append("event %s (events.txt:%d) needs you in %s; no news line, journal step or line that sets its flags names it"
                             % (ev.id, ev.lineno, place))
    if warns:
        part.group("WARN", "presence-gated events whose place may never have been named (heuristic: read them)", warns)
    part.line("PASS" if not warns else "INFO",
              "%d presence-gated events, %d places named earlier by a news line, a journal step or a flag-setting line" % (len(gated), oks))


# ---- places reached only by sea

def parse_settlements(stats_out):
    S = {}
    for m in re.finditer(r"^\s+settlement (.*?)\|(\d+) (\d+) (\d+) (\d+)\|(\w+)$", stats_out, re.M):
        name, x, y, w, h, kind = m.groups()
        S[name] = (int(x) + int(w) / 2.0, int(y) + int(h) / 2.0, kind == "hamlet")
    return S


def parse_components(stats_out):
    comps = []
    for m in re.finditer(r"^\s+joined by road: (.*?) \(and (\d+) hamlets\)", stats_out, re.M):
        comps.append(([t.strip() for t in m.group(1).split(", ")], int(m.group(2))))
    noroad = re.findall(r"^\s+no road reaches (.*)$", stats_out, re.M)
    return comps, [n.strip() for n in noroad]


def passages_and_ships(files):
    """From the talk effects: ({passage PRICE MINUTES DEST} and {buyship PRICE KIND}) with the town of the speaker."""
    passages, ships = [], []
    for path in files:
        if path.endswith(".md") or os.sep + "people" + os.sep not in path:
            continue
        town = "?"
        for i, line in enumerate(open(path, encoding="utf-8", errors="replace").read().splitlines(), 1):
            s = line.strip()
            m = re.match(r"^town\s+(.+)$", s)
            if m:
                town = m.group(1).strip()
                continue
            for body in re.findall(r"\{([^{}]*)\}", s):
                for part in body.split(";"):
                    part = part.strip()
                    m = re.match(r"^passage\s+(\d+)\s+(\d+)\s+(.+)$", part)
                    if m:
                        passages.append((town, int(m.group(1)), int(m.group(2)), m.group(3).strip(), "%s:%d" % (relpath(path), i)))
                    m = re.match(r"^buyship\s+(\d+)", part)
                    if m:
                        ships.append((town, int(m.group(1)), "%s:%d" % (relpath(path), i)))
    return passages, ships


def lint_sea(part, files, stats_out):
    if not stats_out:
        part.line("WARN", "no --stats output: places reached only by sea not checked")
        return
    S = parse_settlements(stats_out)
    comps, noroad = parse_components(stats_out)
    if not comps or not S:
        part.line("WARN", "could not read the road components from --stats (its output changed?)")
        return
    towns = {n: v for n, v in S.items() if not v[2]}
    main = max(comps, key=lambda c: len(c[0]))
    island_of = {}   # place -> component index (non-main), hamlets by the nearest town
    groups = []
    for ci, (names, nham) in enumerate(comps):
        if names is main[0]:
            continue
        groups.append({"towns": names, "hamlets": [], "nham": nham})
    nearest_comp = {}
    for h, (x, y, ham) in S.items():
        if not ham:
            continue
        best, bd = None, 1e9
        for t, (tx, ty, _) in towns.items():
            d = ((tx - x) ** 2 + (ty - y) ** 2) ** 0.5
            if d < bd:
                best, bd = t, d
        nearest_comp[h] = (best, bd)
    lonely = []
    for h in sorted(noroad):
        if h in nearest_comp and nearest_comp[h][1] > 300:
            lonely.append(h)
    for h, (t, d) in nearest_comp.items():
        if h in noroad:
            continue
        for g in groups:
            if t in g["towns"]:
                g["hamlets"].append(h)
    for g in groups:
        if len(g["hamlets"]) != g["nham"]:
            part.line("WARN", "road component %s has %d hamlets by --stats but %d by nearest town: hamlets of it uncertain"
                      % (", ".join(g["towns"]), g["nham"], len(g["hamlets"])))
    places = [{"name": ", ".join(g["towns"]), "members": set(x.lower() for x in g["towns"] + g["hamlets"])} for g in groups]
    places += [{"name": h, "members": {h.lower()}} for h in lonely]
    onfoot = [h for h in noroad if h not in lonely]
    if onfoot:
        part.line("INFO", "no road, but walkable (within 300 tiles of a town): %s" % ", ".join(onfoot))
    passages, ships = passages_and_ships(files)
    for pl in places:
        ways_in, ways_out = [], []
        for (frm, price, mins, dest, where) in passages:
            f, d = frm.lower(), dest.lower()
            if d in pl["members"] and f not in pl["members"]:
                ways_in.append("passage from %s for %s d (%s)" % (frm, price, where))
            if f in pl["members"] and d not in pl["members"]:
                ways_out.append((price, "passage to %s for %s d (%s)" % (dest, price, where)))
        # a place no passage reaches: only a ship of one's own, if the data sells ships
        own = ["buyship %d d at %s, %s" % (p, t, w) for (t, p, w) in ships]
        label = pl["name"]
        if not ways_in and not own:
            part.line("FAIL", "%s is reached only by sea and nothing in the data goes there" % label)
            continue
        if not ways_in:
            part.line("INFO", "%s: no passage goes there; only a ship of your own (%s); the ship is also the way out" % (label, own[0]))
            part.line("PASS", "%s (sea only): reached and left by a ship of your own" % label)
            continue
        if any(p == 0 for p, _ in ways_out):
            part.line("PASS", "%s (sea only): in by %s; out free (passage priced 0: work)" % (label, ways_in[0].split(" (")[0]))
        elif ways_out:
            cheapest = min(ways_out)
            if cheapest[0] <= 240:
                part.line("PASS", "%s (sea only): in by %s; out only for a small fare (%d d, %s): a penniless newcomer must earn it first"
                          % (label, ways_in[0].split(" (")[0], cheapest[0], cheapest[1].split(" (")[1].rstrip(")")))
            else:
                part.line("WARN", "%s (sea only): the only ways out cost money (cheapest %d d, %s) and none is free or paid by work"
                          % (label, cheapest[0], cheapest[1].split(" (")[1].rstrip(")")))
        else:
            part.line("WARN", "%s (sea only): a way in (%s) but no way out in the data" % (label, ways_in[0]))
        if VERBOSE:
            for w in ways_in:
                print("         in:  %s" % w)
            for p, w in ways_out:
                print("         out: %s" % w)


# ---- prose placeholders

PLACEHOLDER = re.compile(r"\[\[PROSE:(.*?)\]\]", re.S)
TODO_HEADER = """# Prose still to be written

Each line of the game's data marked `[[PROSE: what the line must convey]]` is a line still to be
written, and must be listed here, one per line, in the form

    - data/path/file.txt:LINE: what the line must convey

`tools/check.py` lists every placeholder in data/ (a WARN: "N prose placeholders left: not ready for
a playtest"; a FAIL with `--strict`), and warns of any that is not listed here and of any entry here
that no longer matches a placeholder. An entry matches by its file:line or by the exact words of the
placeholder.

## The list

(empty)
"""


def lint_prose(part, files):
    found = []
    for path in files:
        if path.endswith(".md") and os.path.abspath(path) == os.path.abspath(PROSE_TODO):
            continue
        text = open(path, encoding="utf-8", errors="replace").read()
        for m in PLACEHOLDER.finditer(text):
            ln = text.count("\n", 0, m.start()) + 1
            found.append((relpath(path), ln, m.group(1).strip()))
    if not os.path.exists(PROSE_TODO):
        os.makedirs(os.path.dirname(PROSE_TODO), exist_ok=True)
        with open(PROSE_TODO, "w", encoding="utf-8") as f:
            f.write(TODO_HEADER)
        part.line("INFO", "created %s (empty list)" % relpath(PROSE_TODO))
    todo = open(PROSE_TODO, encoding="utf-8").read()
    entries = [ln for ln in todo.split("## Rewrites")[0].splitlines() if ln.startswith("- ")]   # the Rewrites section lists lines to rewrite, not placeholders
    unlisted = []
    for (p, ln, words) in found:
        if ("%s:%d" % (p, ln)) in todo or words in todo:
            continue
        unlisted.append("%s:%d  [[PROSE: %s]]" % (p, ln, words[:70]))
    stale = []
    for e in entries:
        m = re.match(r"^- (\S+?):(\d+)", e)
        if m and not any(p == m.group(1) and ln == int(m.group(2)) for (p, ln, _) in found):
            if not any(w and w in e for (_, _, w) in found):
                stale.append(e[:100])
    level = "FAIL" if STRICT else "WARN"
    if found:
        part.line(level, "%d prose placeholders left: not ready for a playtest" % len(found))
        shown = found if VERBOSE else found[:CAP]
        for (p, ln, w) in shown:
            print("         %s:%d  %s" % (p, ln, w[:70]))
        if len(shown) < len(found):
            print("         ... and %d more (-v lists all)" % (len(found) - len(shown)))
    else:
        part.line("PASS", "no prose placeholders left")
    part.group(level if STRICT else "WARN", "placeholders not listed in alpha/PROSE_TODO.md", unlisted)
    part.group("WARN", "alpha/PROSE_TODO.md entries that match no placeholder", stale)


def run_lint(stats_out):
    part = Part("Part 2: the plot lint")
    files = data_files()
    try:
        lint_flags(part, files)
    except Exception as e:   # a lint bug must show, not hide
        part.line("FAIL", "flags lint crashed: %r" % e)
    try:
        lint_presence(part, files)
    except Exception as e:
        part.line("FAIL", "presence lint crashed: %r" % e)
    try:
        lint_sea(part, files, stats_out)
    except Exception as e:
        part.line("FAIL", "sea lint crashed: %r" % e)
    try:
        lint_prose(part, files)
    except Exception as e:
        part.line("FAIL", "prose lint crashed: %r" % e)
    part.done()
    return part


# ------------------------------------------------------------------ part 3: stand-in scripts

class Scenario:
    def __init__(self, name, lineno):
        self.name, self.lineno, self.about, self.fixture, self.steps = name, lineno, "", "-", []


def parse_scenarios():
    scs, cur = [], None
    for i, line in enumerate(open(SCENARIOS, encoding="utf-8").read().splitlines(), 1):
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        w = s.split(None, 1)
        arg = w[1] if len(w) > 1 else ""
        if w[0] == "scenario":
            cur = Scenario(arg.strip(), i)
            scs.append(cur)
        elif cur is None:
            raise ValueError("scenarios.txt:%d: %s outside a scenario" % (i, w[0]))
        elif w[0] == "end":
            cur = None
        elif w[0] == "about":
            cur.about = arg
        elif w[0] == "fixture":
            cur.fixture = arg.strip()
        elif w[0] == "run":
            cur.steps.append(("run", arg, i))
        elif w[0] in ("expect", "absent", "expect-re", "file-expect", "file-absent", "exists"):
            if not cur.steps:
                raise ValueError("scenarios.txt:%d: %s before any run" % (i, w[0]))
            cur.steps.append((w[0], arg, i))
        else:
            raise ValueError("scenarios.txt:%d: unknown word %s" % (i, w[0]))
    return scs


def run_scenario(sc, tmp):
    """Returns a list of failure strings (empty: passed)."""
    save = os.path.join(tmp, "save.txt")
    if sc.fixture != "-":
        src = os.path.join(FIXTURES, sc.fixture)
        if not os.path.exists(src):
            return ["fixture %s is missing" % sc.fixture]
        shutil.copy(src, save)
    fails, out = [], ""
    for kind, arg, ln in sc.steps:
        if kind == "run":
            try:
                r = subprocess.run([os.path.join(ROOT, "tools", "play.sh"), save, arg], cwd=ROOT,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=240)
                out = r.stdout.decode("utf-8", "replace")
            except subprocess.TimeoutExpired:
                return fails + ["scenarios.txt:%d: the run timed out" % ln]
            if "no command" in out and "HELP lists them" in out:
                fails.append("scenarios.txt:%d: a command was not understood by the stand-in" % ln)
        elif kind == "expect":
            if arg not in out:
                fails.append("scenarios.txt:%d: expected %r in the output" % (ln, arg))
        elif kind == "absent":
            if arg in out:
                fails.append("scenarios.txt:%d: found %r, which must not appear" % (ln, arg))
        elif kind == "expect-re":
            if not re.search(arg, out, re.M):
                fails.append("scenarios.txt:%d: no match for /%s/ in the output" % (ln, arg))
        elif kind in ("file-expect", "file-absent"):
            text = open(save, encoding="utf-8", errors="replace").read() if os.path.exists(save) else ""
            hit = re.search(arg, text, re.M) is not None
            if kind == "file-expect" and not hit:
                fails.append("scenarios.txt:%d: the save has no line matching /%s/" % (ln, arg))
            if kind == "file-absent" and hit:
                fails.append("scenarios.txt:%d: the save has a line matching /%s/" % (ln, arg))
        elif kind == "exists":
            if not os.path.exists(save + arg):
                fails.append("scenarios.txt:%d: %s was not written" % (ln, "save.txt" + arg))
    return fails


def run_scenarios(only=None):
    part = Part("Part 3: stand-in scripts (tools/check/scenarios.txt)")
    try:
        scs = parse_scenarios()
    except Exception as e:
        part.line("FAIL", "scenarios.txt: %s" % e)
        part.done()
        return part
    if only:
        scs = [s for s in scs if s.name == only]
        if not scs:
            part.line("FAIL", "no scenario called %s" % only)
    for sc in scs:
        tmp = tempfile.mkdtemp(prefix="galileo-check-")
        t0 = time.time()
        try:
            fails = run_scenario(sc, tmp)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
        dt = time.time() - t0
        if fails:
            part.line("FAIL", "%s (%.0f s)%s" % (sc.name, dt, ": " + sc.about if sc.about else ""))
            for f in fails:
                print("         %s" % f)
        else:
            part.line("PASS", "%s (%.0f s)" % (sc.name, dt))
    part.done()
    return part


# ------------------------------------------------------------------ main

def main(argv):
    global VERBOSE, STRICT
    quick, only, scen = False, None, None
    args = list(argv)
    while args:
        a = args.pop(0)
        if a == "--quick":
            quick = True
        elif a == "--strict":
            STRICT = True
        elif a == "-v":
            VERBOSE = True
        elif a == "--only":
            only = args.pop(0)
        elif a == "--scenario":
            scen = args.pop(0)
            only = "scenarios"
        elif a in ("-h", "--help"):
            print(__doc__)
            return 0
        else:
            print("unknown option %s (see --help)" % a)
            return 2
    t0 = time.time()
    m0 = md5(AUTOSAVE)
    print("saves/autosave.txt md5 at the start: %s" % m0)
    parts, stats_out = [], ""
    if only in (None, "stats", "lint"):
        p1, stats_out = run_stats()
        if only != "lint":
            parts.append(p1)
    if only in (None, "lint"):
        parts.append(run_lint(stats_out))
    if only in (None, "scenarios") and not (quick and only is None):
        parts.append(run_scenarios(scen))
    m1 = md5(AUTOSAVE)
    print("\n== Summary")
    total = {"PASS": 0, "WARN": 0, "FAIL": 0}
    for p in parts:
        for k in total:
            total[k] += p.counts[k]
        print("  %-52s PASS %-3d WARN %-3d FAIL %-3d (%.0f s)" % (p.name, p.counts["PASS"], p.counts["WARN"], p.counts["FAIL"], p.secs))
    if quick and only is None:
        print("  Part 3 skipped (--quick)")
    if m1 != m0:
        print("  WARN saves/autosave.txt changed during the check (%s -> %s): the game window was used meanwhile; the check never writes it" % (m0, m1))
    else:
        print("  saves/autosave.txt unchanged (%s)" % m1)
    print("  total: PASS %d, WARN %d, FAIL %d; %.0f s" % (total["PASS"], total["WARN"], total["FAIL"], time.time() - t0))
    return 1 if total["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
