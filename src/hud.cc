// hud.cc
#include "hud.h"
#include <sstream>

void Hud::makeMapImage(const World &world) {
    const Legacy &L = world.L;
    mapImg = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, L.w, L.h);
    unsigned char *d = cairo_image_surface_get_data(mapImg);
    int st = cairo_image_surface_get_stride(mapImg);
    for (int row = 0; row < L.h; row++)
        for (int i = 0; i < L.w; i++) {
            int j = L.h - 1 - row;
            int c = L.geoAt(i, j);
            int f = L.ffAt(i, j);
            unsigned r, g, b;
            switch (c) {
                case 0: r = 38; g = 70; b = 112; break;
                case 1: r = 120; g = 150; b = 84; break;
                case 2: r = 138; g = 162; b = 92; break;
                case 3: r = 170; g = 150; b = 100; break;
                case 4: r = 150; g = 140; b = 128; break;
                case 5: r = 175; g = 170; b = 162; break;
                default: r = 225; g = 225; b = 228; break;
            }
            if (f == 0 || f == 1) { r = r * 7 / 10; g = g * 8 / 10; b = b * 7 / 10; }
            uint32_t *px = (uint32_t *)(d + row * st) + i;
            *px = 0xff000000u | (r << 16) | (g << 8) | b;
        }
    cairo_surface_mark_dirty(mapImg);
}

static void text(cairo_t *cr, double x, double y, const std::string &s, double size, bool bold,
                 double r = 1, double g = 0.97, double b = 0.88) {
    cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_NORMAL, bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    // shadow for legibility
    cairo_set_source_rgba(cr, 0, 0, 0, 0.75);
    cairo_move_to(cr, x + size * 0.07, y + size * 0.07);
    cairo_show_text(cr, s.c_str());
    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, s.c_str());
}

// text with *highlighted* words, wrapped to a width; returns the height used
int hudTalkScrollMin = -100000, hudTalkScrollMax = 100000, hudTalkPageRows = 8;   // set as the conversation is drawn

static double richText(cairo_t *cr, double x, double y, double width, const std::string &s, double size, double r, double g,
                       double b, bool draw = true) {
    cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    double cx = x, cy = y, line = size * 1.3;
    bool hi = false;
    std::string word;
    auto flush = [&](bool space) {
        if (word.empty() && !space) return;
        cairo_text_extents_t ex;
        std::string piece = word + (space ? " " : "");
        cairo_text_extents(cr, piece.c_str(), &ex);
        if (cx + ex.x_advance > x + width && cx > x) { cx = x; cy += line; }
        if (draw) {
            if (hi) cairo_set_source_rgb(cr, 1.0, 0.82, 0.35); else cairo_set_source_rgb(cr, r, g, b);
            cairo_move_to(cr, cx, cy);
            cairo_show_text(cr, piece.c_str());
        }
        cx += ex.x_advance;
        word.clear();
    };
    for (char c : s) {
        if (c == '\n') { flush(false); cx = x; cy += line; continue; }   // a new line (a blank one between paragraphs)
        if (c == '*') { flush(false); hi = !hi; continue; }
        if (c == ' ') { flush(true); continue; }
        word += c;
    }
    flush(false);
    return cy - y + line;
}

void Hud::setPlan(const std::string &town, int w, int h, const std::vector<uint32_t> &px) {
    if (w <= 0 || h <= 0 || (int)px.size() < w * h) return;
    cairo_surface_t *img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    unsigned char *d = cairo_image_surface_get_data(img);
    int st = cairo_image_surface_get_stride(img);
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++) *((uint32_t *)(d + r * st) + c) = px[r * w + c];
    cairo_surface_mark_dirty(img);
    auto it = planImgs.find(town);
    if (it != planImgs.end() && it->second) cairo_surface_destroy(it->second);
    planImgs[town] = img;
}

bool Hud::draw(const HudState &s, const World &world, int W, int H, float sc, double now) {
    std::ostringstream k, kl;
    k << s.place << "|" << s.date << "|" << s.coords << "|" << s.message << "|" << s.help << s.map << "|" << W << "x" << H
      << "|" << s.showHealth << (int)(s.health * 100);
    if (s.map) k << "|" << (int)s.px << "," << (int)s.py << "|" << s.beenTowns.size();
    k << "|" << s.talking << s.talkTitle << "|" << s.talkInput << "|" << s.talkLines.size() << "/" << s.talkScroll;
    k << "|" << s.journal << s.journalTab << s.journalDetailed << s.journalPage << s.journalLines.size() << s.storyLines.size();
    if (s.journal && s.journalTab == 0) for (auto &l : s.storyLines) k << l.first << l.second.size() << ",";   // the order changes, not only the count
    if (s.journal && s.journalTab == 0) for (auto &p : s.townPlans) k << p.town << p.marks.size() << (int)p.youX << (int)p.youY << ";";
    k << "|" << s.panel << s.panelTitle << s.panelText << s.panelImage;
    k << "|" << s.bookFirst << s.bookTitle << s.bookNote << s.bookLeft.size() << s.bookRight.size();
    for (auto &l : s.panelLines) k << l << ";";
    k << "|" << s.creating << s.createRow;   // the choosing screen, and the still scenes
    for (auto &r : s.createRows) k << r.second << ";";
    k << "|" << s.sceneNo << "/" << s.sceneCount << s.caption;
    for (auto &l : s.cardLines) k << "|C" << l.second << l.first;
    k << "|F" << (int)(s.fade * 60);
    for (auto &l : s.labels) kl << (int)l.x / 2 << "," << (int)l.y / 2 << l.text << ";";
    for (auto &p : s.puffs) kl << "P" << (int)p.x / 2 << "," << (int)p.y / 2 << "," << (int)p.r << "," << (int)(p.a * 40) << ";";
    std::string key = k.str() + "#" + kl.str();
    if (surf && key == lastKey) return false;
    // names move with their people: redraw for them at most thirty times a second
    if (surf && k.str() == lastStatic && now - lastDraw < 1.0 / 30) return false;
    lastKey = key;
    lastStatic = k.str();
    lastDraw = now;
    if (!surf || W != w || H != h) {
        if (surf) cairo_surface_destroy(surf);
        surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
        w = W; h = H;
    }
    scale = sc;
    cairo_t *cr = cairo_create(surf);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    double u = sc;

    // a card: a dark warm ground over everything, its lines centred in parchment colour
    if (!s.cardLines.empty()) {
        cairo_set_source_rgb(cr, 0x16 / 255.0, 0x11 / 255.0, 0x0C / 255.0);
        cairo_paint(cr);
        double smallS = 24 * u, largeS = 74 * u, maxW = W * 0.86;
        auto sizeOf = [&](const std::string &t, bool large) {   // as large as asked, or smaller to fit the width
            double fs = large ? largeS : smallS;
            cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, fs);
            cairo_text_extents_t ex;
            cairo_text_extents(cr, t.c_str(), &ex);
            return ex.x_advance > maxW ? fs * maxW / ex.x_advance : fs;
        };
        double total = 0;
        std::vector<double> sz;
        for (auto &l : s.cardLines) { double f = sizeOf(l.first, l.second); sz.push_back(f); total += f * (l.second ? 1.55 : 1.9); }
        double y = (H - total) / 2;
        for (size_t i = 0; i < s.cardLines.size(); i++) {
            double f = sz[i];
            cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, f);
            cairo_text_extents_t ex;
            cairo_text_extents(cr, s.cardLines[i].first.c_str(), &ex);
            y += f * (s.cardLines[i].second ? 1.2 : 1.45);
            cairo_set_source_rgb(cr, 0xE8 / 255.0, 0xDC / 255.0, 0xC0 / 255.0);
            cairo_move_to(cr, (W - ex.x_advance) / 2, y);
            cairo_show_text(cr, s.cardLines[i].first.c_str());
            y += f * (s.cardLines[i].second ? 0.35 : 0.45);
        }
        cairo_destroy(cr);
        cairo_surface_flush(surf);
        return true;
    }

    // a still scene: only its words, in a band along the bottom (no status panel, no names)
    if (!s.caption.empty()) {
        double bw = std::min(W - 60 * u, 980 * u), bx = (W - bw) / 2;
        double fs = 19 * u, tw = bw - 70 * u;
        double th = richText(cr, 0, 0, tw, s.caption, fs, 1, 1, 1, false);
        double bh = th + 74 * u, by = H - bh - 34 * u;
        cairo_set_source_rgba(cr, 0.06, 0.045, 0.03, 0.78);
        cairo_rectangle(cr, bx, by, bw, bh);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.75, 0.6, 0.35, 0.8);
        cairo_set_line_width(cr, 1.2 * u);
        cairo_rectangle(cr, bx + 0.5, by + 0.5, bw - 1, bh - 1);
        cairo_stroke(cr);
        richText(cr, bx + 35 * u, by + 36 * u, tw, s.caption, fs, 1, 0.96, 0.86);
        char buf[96];
        std::snprintf(buf, sizeof buf, "%d of %d.   Click, or Space: on.%s   %s", s.sceneNo, s.sceneCount,
                      s.sceneNo > 1 ? "   Left: back." : "", s.sceneOpening ? "Esc: begin at once." : "Esc: skip the rest.");
        text(cr, bx + 35 * u, by + bh - 16 * u, buf, 12 * u, false, 0.7, 0.65, 0.55);
        cairo_destroy(cr);
        cairo_surface_flush(surf);
        return true;
    }

    // status panel, top left (not while you choose who you are: you are not yet anywhere)
    if (!s.creating) {
    cairo_set_source_rgba(cr, 0.08, 0.06, 0.04, 0.55);
    cairo_rectangle(cr, 10 * u, 10 * u, 430 * u, 74 * u);
    cairo_fill(cr);
    text(cr, 20 * u, 36 * u, s.place, 19 * u, true);
    text(cr, 20 * u, 58 * u, s.date, 14 * u, false);
    text(cr, 20 * u, 76 * u, s.coords, 11 * u, false, 0.8, 0.78, 0.7);
    }
    if (s.showHealth && !s.creating) {   // your health: a bar at the right of the panel
        double bx = 300 * u, by = 47 * u, bw = 128 * u, bh = 11 * u, f = std::max(0.0f, std::min(1.0f, s.health));
        text(cr, bx, by - 3 * u, "Health", 11 * u, false, 0.85, 0.8, 0.7);
        cairo_set_source_rgba(cr, 0.2, 0.05, 0.04, 0.85);
        cairo_rectangle(cr, bx, by, bw, bh);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, f > 0.5 ? 0.35 : 0.75, f > 0.5 ? 0.6 : (f > 0.25 ? 0.5 : 0.15), 0.2, 0.95);
        cairo_rectangle(cr, bx, by, bw * f, bh);
        cairo_fill(cr);
    }

    if (!s.message.empty()) {
        cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 16 * u);
        // split into lines no wider than most of the window
        std::vector<std::string> lines(1);
        double maxw = std::min(W - 80 * u, 900 * u), widest = 0;
        {
            std::istringstream ms(s.message);
            std::string wd;
            while (ms >> wd) {
                std::string tryl = lines.back().empty() ? wd : lines.back() + " " + wd;
                cairo_text_extents_t e;
                cairo_text_extents(cr, tryl.c_str(), &e);
                if (e.x_advance > maxw && !lines.back().empty()) lines.push_back(wd);
                else lines.back() = tryl;
            }
            for (auto &l : lines) { cairo_text_extents_t e; cairo_text_extents(cr, l.c_str(), &e); widest = std::max(widest, e.x_advance); }
        }
        double lh = 22 * u, y = H - 30 * u - lh * (lines.size() - 1);
        cairo_set_source_rgba(cr, 0.08, 0.06, 0.04, 0.55);
        cairo_rectangle(cr, (W - widest) / 2 - 14 * u, y - 22 * u, widest + 28 * u, 32 * u + lh * (lines.size() - 1));
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 0.95, 0.82);
        for (size_t i = 0; i < lines.size(); i++) {
            cairo_text_extents_t e;
            cairo_text_extents(cr, lines[i].c_str(), &e);
            cairo_move_to(cr, (W - e.x_advance) / 2, y + lh * i);
            cairo_show_text(cr, lines[i].c_str());
        }
    }

    if (s.help) {
        static const std::set<std::string> forTesting = {"]  [", "N   ", "Tab "};   // shown only with --testing
        const char *all[] = {
            "arrows, WASD or keypad   walk (two keys together: diagonal)",
            "U, click a ladder        climb it (also stairs, trapdoors, shafts)",
            "walk into a boat         get in and row; walk onto land to get out",
            "shift + direction        run",
            "Q  E                     turn the view",
            "R  F                     tilt the view",
            "=  -  (or scroll)        zoom",
            "Z                        reset the view",
            "V                        lift all the roofs (and put them back)",
            "T                        talk to someone beside you",
            "  (in talk)              buy, buy bread, sell candle, give book,",
            "                         give 2 sous, pay, bet 2 sous, bye",
            "G, then click            get something within reach",
            "L, then click            look at something (L twice: what is in front of you)",
            "C, then click            fight someone, or a wolf (C again: stop)",
            "I                        what you carry (a number, then U: use it,",
            "                         D and a click: put it down)",
            "U, then click            use something: a bed, a bench, a well, an altar,",
            "                         an animal (milk with a pail, shear with shears, eggs);",
            "                         your horse: get on, or off (innkeepers hire and sell horses)",
            "                         a deer, a wolf: shoot it (a fowling piece, and shot);",
            "                         a dead one: skin it (a knife); water: fish (a rod)",
            "                         (U twice: something you carry: eat, drink, read)",
            "J                        what you have overheard",
            "]  [                     an hour later / earlier",
            "M                        map of Europe",
            "N                        travel to the next town",
            "Tab                      gallop (fast travel, for testing)",
            "Ctrl+M                   music off / on",
            "K                        sound effects off / on",
            "P                        screenshot (saved in shots/)",
            "H                        this help",
            "Esc                      close the map, or quit",
        };
        std::vector<const char *> lines;
        for (const char *l : all)
            if (s.testing || !forTesting.count(std::string(l).substr(0, 4))) lines.push_back(l);
        int n = (int)lines.size();
        cairo_select_font_face(cr, "Menlo", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12.5 * u);
        double widest = 0;   // the panel as wide as its longest line
        for (const char *l : lines) {
            cairo_text_extents_t e;
            cairo_text_extents(cr, l, &e);
            widest = std::max(widest, e.x_advance);
        }
        double pw = widest + 28 * u, x0 = W - pw - 10 * u, y0 = 10 * u;
        cairo_set_source_rgba(cr, 0.08, 0.06, 0.04, 0.7);
        cairo_rectangle(cr, x0, y0, pw, (24 + n * 20) * u);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 0.95, 0.85);
        for (int i = 0; i < n; i++) {
            cairo_move_to(cr, x0 + 14 * u, y0 + (28 + i * 20) * u);
            cairo_show_text(cr, lines[i]);
        }
    } else {
        text(cr, W - 150 * u, 28 * u, "H: help", 13 * u, false, 0.9, 0.88, 0.8);
    }

    // gun smoke: each puff a few soft grey clouds, overlapping
    for (auto &p : s.puffs) {
        uint32_t h = (uint32_t)p.seed * 2654435761u;
        for (int k = 0; k < 5; k++) {
            h = h * 1664525u + 1013904223u;
            double ox = ((h >> 8) % 1000 / 1000.0 - 0.5) * p.r * 1.1, oy = ((h >> 18) % 1000 / 1000.0 - 0.5) * p.r * 0.7;
            double r = p.r * (0.55 + 0.25 * ((h >> 4) % 100) / 100.0);
            cairo_pattern_t *g = cairo_pattern_create_radial(p.x + ox, p.y + oy, 0, p.x + ox, p.y + oy, r);
            cairo_pattern_add_color_stop_rgba(g, 0, 0.93, 0.92, 0.89, p.a * 0.85);
            cairo_pattern_add_color_stop_rgba(g, 0.55, 0.86, 0.85, 0.82, p.a * 0.55);
            cairo_pattern_add_color_stop_rgba(g, 1, 0.78, 0.77, 0.74, 0);
            cairo_set_source(cr, g);
            cairo_arc(cr, p.x + ox, p.y + oy, r, 0, 2 * M_PI);
            cairo_fill(cr);
            cairo_pattern_destroy(g);
        }
    }

    // names over heads, and then what people say, on top
    for (int pass = 0; pass < 2; pass++)
    for (auto &l : s.labels) {
        if (l.bubble != (pass == 1)) continue;
        cairo_select_font_face(cr, "Palatino", l.bubble ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        double fs = (l.bubble ? 14 : 12) * u;
        cairo_set_font_size(cr, fs);
        cairo_text_extents_t ex;
        cairo_text_extents(cr, l.text.c_str(), &ex);
        double x = l.x - ex.width / 2, y = l.y;
        if (l.bubble) {
            // wrapped to a few words a line, growing upwards; *words* in red
            std::vector<std::string> words;
            {
                std::istringstream ws(l.text);
                std::string wd;
                while (ws >> wd) words.push_back(wd);
            }
            auto width = [&](const std::string &wd) {
                std::string plain;
                for (char c : wd) if (c != '*') plain += c;
                cairo_text_extents_t e;
                cairo_text_extents(cr, plain.c_str(), &e);
                return e.x_advance;
            };
            cairo_text_extents_t sp;
            cairo_text_extents(cr, "x x", &sp);
            cairo_text_extents_t sx;
            cairo_text_extents(cr, "xx", &sx);
            double space = sp.x_advance - sx.x_advance, maxw = 300 * u;
            std::vector<std::vector<std::string>> lines(1);
            std::vector<double> widths(1, 0);
            for (auto &wd : words) {
                double ww = width(wd);
                if (!lines.back().empty() && widths.back() + space + ww > maxw) { lines.emplace_back(); widths.push_back(0); }
                widths.back() += (lines.back().empty() ? 0 : space) + ww;
                lines.back().push_back(wd);
            }
            double bw = 0;
            for (double v : widths) bw = std::max(bw, v);
            double lh = fs * 1.25, px = 6 * u, py = 4 * u;
            double top = y - lh * (lines.size() - 1) - fs * 0.8;
            cairo_set_source_rgba(cr, 0.98, 0.96, 0.9, 0.92);
            cairo_rectangle(cr, l.x - bw / 2 - px, top - py, bw + 2 * px, lh * (lines.size() - 1) + fs * 1.1 + 2 * py);
            cairo_fill(cr);
            bool hi = false;
            for (size_t i = 0; i < lines.size(); i++) {
                double cx = l.x - widths[i] / 2, cy = y - lh * (lines.size() - 1 - i);
                for (size_t k = 0; k < lines[i].size(); k++) {
                    std::string piece;
                    auto flush = [&]() {
                        if (piece.empty()) return;
                        if (hi) cairo_set_source_rgb(cr, 0.62, 0.12, 0.08); else cairo_set_source_rgb(cr, 0.15, 0.1, 0.05);
                        cairo_move_to(cr, cx, cy);
                        cairo_show_text(cr, piece.c_str());
                        cairo_text_extents_t e;
                        cairo_text_extents(cr, piece.c_str(), &e);
                        cx += e.x_advance;
                        piece.clear();
                    };
                    for (char c : lines[i][k]) {
                        if (c == '*') { flush(); hi = !hi; } else piece += c;
                    }
                    flush();
                    cx += space;
                }
            }
        } else {
            cairo_set_source_rgba(cr, 0, 0, 0, 0.7);
            cairo_move_to(cr, x + u, y + u);
            cairo_show_text(cr, l.text.c_str());
            cairo_set_source_rgb(cr, 1, 0.97, 0.88);
            cairo_move_to(cr, x, y);
            cairo_show_text(cr, l.text.c_str());
        }
    }

    // choosing who you are (a new game): the rows, the one being changed lit
    if (s.creating) {
        double bw = 430 * u, bx = 40 * u, by = 110 * u, rowH = 34 * u;
        double bh = 120 * u + rowH * s.createRows.size();
        cairo_set_source_rgba(cr, 0.1, 0.07, 0.05, 0.88);
        cairo_rectangle(cr, bx, by, bw, bh);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.75, 0.6, 0.35, 0.9);
        cairo_set_line_width(cr, 1.5 * u);
        cairo_rectangle(cr, bx + 0.5, by + 0.5, bw - 1, bh - 1);
        cairo_stroke(cr);
        text(cr, bx + 18 * u, by + 32 * u, "Who are you?", 20 * u, true, 1, 0.85, 0.55);
        text(cr, bx + 18 * u, by + 54 * u, "Seventeen years old, of the farm at La Chapelle, near Paris.", 12.5 * u, false, 0.85, 0.8, 0.7);
        for (size_t i = 0; i < s.createRows.size(); i++) {
            double ry = by + 80 * u + rowH * i;
            bool lit = (int)i == s.createRow;
            if (lit) {
                cairo_set_source_rgba(cr, 0.75, 0.6, 0.35, 0.25);
                cairo_rectangle(cr, bx + 10 * u, ry - 20 * u, bw - 20 * u, rowH - 4 * u);
                cairo_fill(cr);
            }
            text(cr, bx + 22 * u, ry, s.createRows[i].first, 15 * u, false, 0.8, 0.75, 0.65);
            std::string v = s.createRows[i].second;
            if (lit && i == 0) v += "_";
            else if (lit) v = "<  " + v + "  >";
            text(cr, bx + 150 * u, ry, v, 15 * u, lit, 1, 0.95, 0.85);
        }
        text(cr, bx + 18 * u, by + bh - 34 * u, "Up and down: choose a line.  Left and right: change it.", 12 * u, false, 0.75, 0.7, 0.6);
        text(cr, bx + 18 * u, by + bh - 16 * u, "Type your name on the first line.  Enter: begin.", 12 * u, false, 0.75, 0.7, 0.6);
    }

    // a conversation
    if (s.talking) {
        double bw = std::min(W - 40 * u, 760 * u), bh = 300 * u, bx = (W - bw) / 2, by = H - bh - 20 * u;
        cairo_set_source_rgba(cr, 0.1, 0.07, 0.05, 0.86);
        cairo_rectangle(cr, bx, by, bw, bh);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.75, 0.6, 0.35, 0.9);
        cairo_set_line_width(cr, 1.5 * u);
        cairo_rectangle(cr, bx + 0.5, by + 0.5, bw - 1, bh - 1);
        cairo_stroke(cr);
        text(cr, bx + 16 * u, by + 26 * u, s.talkTitle, 17 * u, true, 1, 0.85, 0.55);
        // the conversation, scrolled by rows of text (Up/Down: a row; Page Up/Down: a boxful): it ends at the latest
        // line, or, when the latest line is taller than the box, at that line's start, with the rest below
        double fs = 14.5 * u, tx = bx + 16 * u, tw = bw - 32 * u, top = by + 40 * u, bottom = by + bh - 44 * u;
        double avail = bottom - top, row = fs * 1.3;
        std::vector<double> lh(s.talkLines.size());
        double total = 0;
        for (size_t i = 0; i < s.talkLines.size(); i++) {
            const auto &ln = s.talkLines[i];
            lh[i] = richText(cr, tx, 0, tw, ln.first ? ln.second : "> " + ln.second, fs, 1, 1, 1, false) + 4 * u;
            total += lh[i];
        }
        double most = std::max(0.0, total - avail);                                            // the most hidden below
        double first = lh.empty() ? 0 : std::min(most, std::max(0.0, lh.back() - avail));     // hidden below at first
        double below = std::min(most, std::max(0.0, first + s.talkScroll * row));
        hudTalkScrollMin = -(int)std::ceil(first / row);
        hudTalkScrollMax = (int)std::ceil((most - first) / row);
        hudTalkPageRows = std::max(1, (int)(avail / row) - 1);
        double viewTop = std::max(0.0, total - below - avail);   // the box's top, measured from the first line's top
        cairo_save(cr);
        cairo_rectangle(cr, bx + 4 * u, top - 2 * u, bw - 8 * u, avail + 4 * u);
        cairo_clip(cr);
        double y0 = 0;
        for (size_t i = 0; i < s.talkLines.size(); y0 += lh[i], i++) {
            if (y0 + lh[i] < viewTop || y0 > viewTop + avail) continue;
            const auto &ln = s.talkLines[i];
            double y = top + fs + (y0 - viewTop);
            if (ln.first) richText(cr, tx, y, tw, ln.second, fs, 0.96, 0.93, 0.85);
            else richText(cr, tx, y, tw, "> " + ln.second, fs, 0.6, 0.8, 0.95);
        }
        cairo_restore(cr);
        // what you are typing
        cairo_set_source_rgba(cr, 1, 1, 1, 0.08);
        cairo_rectangle(cr, bx + 10 * u, by + bh - 36 * u, bw - 20 * u, 26 * u);
        cairo_fill(cr);
        text(cr, bx + 16 * u, by + bh - 17 * u, "> " + s.talkInput + "_", 15 * u, false, 0.75, 0.9, 1.0);
        // lines out of sight: below (looked back, or the rest of a long line), or above
        std::string more = below > 0.5 ? "more below: Down" : viewTop > 0.5 ? "earlier lines: Up" : "";
        if (!more.empty()) {
            cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 11 * u);
            cairo_set_source_rgb(cr, 0.85, 0.7, 0.4);
            cairo_text_extents_t mx;
            cairo_text_extents(cr, more.c_str(), &mx);
            cairo_move_to(cr, bx + bw - mx.width - 18 * u, by + bh - 19 * u);
            cairo_show_text(cr, more.c_str());
        }
        cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11 * u);
        cairo_set_source_rgb(cr, 0.7, 0.65, 0.55);
        const char *hint = "type a word and press Return: name, job, bye ...   Esc to leave";
        cairo_text_extents_t ex;
        cairo_text_extents(cr, hint, &ex);
        cairo_move_to(cr, bx + bw - ex.width - 14 * u, by + 26 * u);
        cairo_show_text(cr, hint);
    }

    // an open book: two pages of paper on a leather board, the text in ink, shrunk to fit its page
    auto picture = [&](const std::string &file) -> cairo_surface_t * {
        if (file.empty()) return nullptr;
        auto it = pictures.find(file);
        if (it == pictures.end()) {
            cairo_surface_t *p = cairo_image_surface_create_from_png(file.c_str());
            if (cairo_surface_status(p) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(p); p = nullptr; }
            it = pictures.emplace(file, p).first;
        }
        return it->second;
    };
    if (s.panel == 3 && !s.talking) {
        double bw = std::min(W - 40 * u, 1120 * u), bh = std::min(H - 90 * u, 700 * u);
        double bx = (W - bw) / 2, by = (H - bh) / 2 - 8 * u;
        cairo_set_source_rgba(cr, 0.24, 0.13, 0.07, 0.97);   // the binding
        cairo_rectangle(cr, bx, by, bw, bh);
        cairo_fill(cr);
        double m = 12 * u, pw = (bw - 2 * m) / 2, ph = bh - 2 * m;
        for (int side = 0; side < 2; side++) {
            double px = bx + m + side * pw, py = by + m;
            cairo_set_source_rgb(cr, 0.93, 0.89, 0.79);
            cairo_rectangle(cr, px, py, pw, ph);
            cairo_fill(cr);
            // the shadow of the gutter
            cairo_pattern_t *g = cairo_pattern_create_linear(side ? px : px + pw, 0, side ? px + 26 * u : px + pw - 26 * u, 0);
            cairo_pattern_add_color_stop_rgba(g, 0, 0.35, 0.25, 0.15, 0.35);
            cairo_pattern_add_color_stop_rgba(g, 1, 0.35, 0.25, 0.15, 0);
            cairo_set_source(cr, g);
            cairo_rectangle(cr, px, py, pw, ph);
            cairo_fill(cr);
            cairo_pattern_destroy(g);
            int pageNo = s.bookFirst + side + 1;
            if (pageNo > s.bookPages) continue;
            const std::string &txt = side ? s.bookRight : s.bookLeft;
            const std::string &img = side ? s.bookRightImage : s.bookLeftImage;
            const std::string &cred = side ? s.bookRightCredit : s.bookLeftCredit;
            double ix = px + 34 * u, iw = pw - 68 * u, iy = py + 44 * u, ibottom = py + ph - 40 * u;
            // the running title, small, at the head of the page
            cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 10.5 * u);
            cairo_set_source_rgb(cr, 0.45, 0.32, 0.2);
            std::string head = s.bookTitle.size() > 70 ? s.bookTitle.substr(0, s.bookTitle.rfind(' ', 66)) + " ..." : s.bookTitle;
            cairo_text_extents_t ex;
            cairo_text_extents(cr, head.c_str(), &ex);
            cairo_move_to(cr, px + (pw - ex.width) / 2, py + 24 * u);
            cairo_show_text(cr, head.c_str());
            std::string num = std::to_string(pageNo);
            cairo_set_font_size(cr, 11 * u);
            cairo_text_extents(cr, num.c_str(), &ex);
            cairo_move_to(cr, px + (pw - ex.width) / 2, py + ph - 16 * u);
            cairo_show_text(cr, num.c_str());
            if (cairo_surface_t *pic = picture(img)) {   // a picture, with its caption under it
                double capH = txt.empty() ? 0 : richText(cr, 0, 0, iw, txt, 12.5 * u, 0, 0, 0, false);
                double credH = cred.empty() ? 0 : richText(cr, 0, 0, iw, cred, 9.5 * u, 0, 0, 0, false);
                double sw = cairo_image_surface_get_width(pic), sh = cairo_image_surface_get_height(pic);
                double sc2 = std::min(iw / sw, (ibottom - iy - capH - credH - 16 * u) / sh);
                double dw = sw * sc2, dh = sh * sc2, dx = ix + (iw - dw) / 2;
                cairo_save(cr);
                cairo_translate(cr, dx, iy);
                cairo_scale(cr, sc2, sc2);
                cairo_set_source_surface(cr, pic, 0, 0);
                cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
                cairo_paint(cr);
                cairo_restore(cr);
                double y = iy + dh + 14 * u;
                if (!txt.empty()) y += richText(cr, ix, y, iw, txt, 12.5 * u, 0.2, 0.14, 0.08);
                if (!cred.empty()) richText(cr, ix, y + 2 * u, iw, cred, 9.5 * u, 0.45, 0.38, 0.3);
                continue;
            }
            double fs = 16.5 * u;   // the largest size at which the page fits
            while (fs > 9 * u && richText(cr, 0, 0, iw, txt, fs, 0, 0, 0, false) > ibottom - iy) fs -= 0.5 * u;
            richText(cr, ix, iy + fs, iw, txt, fs, 0.17, 0.12, 0.07);
        }
        // under the book: the language, and how to turn the pages
        cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12 * u);
        cairo_set_source_rgb(cr, 0.9, 0.82, 0.62);
        std::string hint = s.bookNote + (s.bookNote.empty() ? "" : "    ") + "Left and Right arrows turn the pages   Esc to close";
        cairo_text_extents_t ex;
        cairo_text_extents(cr, hint.c_str(), &ex);
        cairo_set_source_rgba(cr, 0.1, 0.07, 0.05, 0.85);
        cairo_rectangle(cr, (W - ex.width) / 2 - 12 * u, by + bh + 4 * u, ex.width + 24 * u, 24 * u);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.9, 0.82, 0.62);
        cairo_move_to(cr, (W - ex.width) / 2, by + bh + 20 * u);
        cairo_show_text(cr, hint.c_str());
    }

    // a work of art with its picture: the picture on the left, the text beside it
    cairo_surface_t *pic = nullptr;
    if (s.panel == 2 && !s.panelImage.empty()) {
        auto it = pictures.find(s.panelImage);
        if (it == pictures.end()) {
            cairo_surface_t *p = cairo_image_surface_create_from_png(s.panelImage.c_str());
            if (cairo_surface_status(p) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(p); p = nullptr; }
            it = pictures.emplace(s.panelImage, p).first;
        }
        pic = it->second;
    }
    if (s.panel == 2 && pic && !s.talking) {
        double iw = cairo_image_surface_get_width(pic), ih = cairo_image_surface_get_height(pic);
        double bw = std::min(W - 40 * u, 1060 * u), maxH = H - 120 * u;
        double picW = std::min(bw * 0.5, iw), sc = picW / iw;
        if (ih * sc > maxH - 90 * u) { sc = (maxH - 90 * u) / ih; picW = iw * sc; }
        double picH = ih * sc;
        double fs = 14.5 * u, tw = bw - picW - 60 * u;
        double hTitle = richText(cr, 0, 0, tw, s.panelTitle, 17 * u, 1, 1, 1, false);
        double hText = richText(cr, 0, 0, tw, s.panelText, fs, 1, 1, 1, false);
        double hCredit = s.panelCredit.empty() ? 0 : richText(cr, 0, 0, picW, s.panelCredit, 10.5 * u, 1, 1, 1, false);
        double bh = std::max(picH + hCredit + 10 * u, hTitle + hText + 30 * u) + 56 * u;
        double bx = (W - bw) / 2, by = (H - bh) / 2;
        cairo_set_source_rgba(cr, 0.1, 0.07, 0.05, 0.94);
        cairo_rectangle(cr, bx, by, bw, bh);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.75, 0.6, 0.35, 0.9);
        cairo_set_line_width(cr, 1.5 * u);
        cairo_rectangle(cr, bx + 0.5, by + 0.5, bw - 1, bh - 1);
        cairo_stroke(cr);
        double px = bx + 20 * u, py = by + 20 * u;
        cairo_save(cr);
        cairo_translate(cr, px, py);
        cairo_scale(cr, sc, sc);
        cairo_set_source_surface(cr, pic, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
        cairo_paint(cr);
        cairo_restore(cr);
        cairo_set_source_rgba(cr, 0.75, 0.6, 0.35, 0.6);
        cairo_rectangle(cr, px - 0.5, py - 0.5, picW + 1, picH + 1);
        cairo_stroke(cr);
        if (hCredit > 0) richText(cr, px, py + picH + 16 * u, picW, s.panelCredit, 10.5 * u, 0.65, 0.6, 0.52);
        double tx = px + picW + 22 * u, ty = by + 40 * u;
        richText(cr, tx, ty, tw, s.panelTitle, 17 * u, 1, 0.85, 0.55);
        richText(cr, tx, ty + hTitle + 8 * u, tw, s.panelText, fs, 0.96, 0.93, 0.85);
        cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11 * u);
        cairo_set_source_rgb(cr, 0.7, 0.65, 0.55);
        const char *hint = "L or Esc to close";
        cairo_text_extents_t ex;
        cairo_text_extents(cr, hint, &ex);
        cairo_move_to(cr, bx + bw - ex.width - 14 * u, by + bh - 10 * u);
        cairo_show_text(cr, hint);
    } else if (s.panel && s.panel != 3 && !s.talking) {
        bool reading = s.panel == 2;
        double bw = std::min(W - 40 * u, (reading ? 640 : 560) * u), bx = (W - bw) / 2;
        double fs = 14.5 * u, tw = bw - 40 * u;
        // measure, to size the panel
        double hText = s.panelText.empty() ? 0 : richText(cr, 0, 0, tw, s.panelText, fs, 1, 1, 1, false);
        double hLines = 0;
        for (auto &l : s.panelLines) hLines += l.empty() ? 8 * u : richText(cr, 0, 0, tw, l, fs, 1, 1, 1, false) + 2 * u;
        double bh = 60 * u + hLines + (hText > 0 ? hText + 18 * u : 0) + 16 * u;
        double by = (H - bh) / 2;
        cairo_set_source_rgba(cr, 0.1, 0.07, 0.05, 0.92);
        cairo_rectangle(cr, bx, by, bw, bh);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.75, 0.6, 0.35, 0.9);
        cairo_set_line_width(cr, 1.5 * u);
        cairo_rectangle(cr, bx + 0.5, by + 0.5, bw - 1, bh - 1);
        cairo_stroke(cr);
        double hy = richText(cr, bx + 20 * u, by + 30 * u, tw, s.panelTitle, 17 * u, 1, 0.85, 0.55, false);
        richText(cr, bx + 20 * u, by + 30 * u, tw, s.panelTitle, 17 * u, 1, 0.85, 0.55);
        double y = by + 30 * u + hy + 6 * u;
        for (auto &l : s.panelLines) {
            if (l.empty()) { y += 8 * u; continue; }
            y += richText(cr, bx + 20 * u, y, tw, l, fs, 0.96, 0.93, 0.85) + 2 * u;
        }
        if (hText > 0) {
            y += reading ? 0 : 10 * u;
            richText(cr, bx + 20 * u, y, tw, s.panelText, fs, reading ? 0.96 : 0.8, reading ? 0.93 : 0.78, reading ? 0.85 : 0.7);
        }
        cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11 * u);
        cairo_set_source_rgb(cr, 0.7, 0.65, 0.55);
        std::string hint = reading ? "L or Esc to close" : s.panelHint;
        cairo_text_extents_t ex;
        cairo_text_extents(cr, hint.c_str(), &ex);
        cairo_move_to(cr, bx + bw - ex.width - 14 * u, by + bh - 10 * u);
        cairo_show_text(cr, hint.c_str());
    }

    if (s.journal && !s.talking && s.journalTab == 0) {   // the stories you are in, in pages
        double bw = std::min(W - 40 * u, 820 * u), bh = std::min(H - 120 * u, 560 * u), bx = (W - bw) / 2, by = (H - bh) / 2;
        cairo_set_source_rgba(cr, 0.1, 0.07, 0.05, 0.97);
        cairo_rectangle(cr, bx, by, bw, bh);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.75, 0.6, 0.35, 0.9);
        cairo_set_line_width(cr, 1.5 * u);
        cairo_rectangle(cr, bx + 0.5, by + 0.5, bw - 1, bh - 1);
        cairo_stroke(cr);
        double fs = 14 * u, tx = bx + 16 * u, tw = bw - 32 * u, top = by + 44 * u + fs, bottom = by + bh - 34 * u;
        bool det = s.journalDetailed;
        // every line as it will be drawn, measured; then the pages: as many as it takes. The finished
        // stories begin a page of their own, and so does each town of the places and people at the
        // back (kinds 4, 5, 6: People::townPages); a title or a heading never ends a page without its
        // first line, and a story or a town carried over to the next page is named again, "(continued)"
        struct JL { int kind, mark; std::string t; double sz, ind, h; bool title; size_t head; };
        std::vector<JL> ls;
        size_t head = 0, firstDone = std::string::npos, firstTown = std::string::npos;
        for (const auto &ln : s.storyLines) {
            int kind = ln.first % 10, mark = ln.first / 10;   // mark: 1 well, 2 badly (the detailed journal)
            bool title = kind == 0 || kind == 3 || kind == 4;
            std::string t = ln.second;
            if (title && det && mark != 3) t += kind == 0 ? "  (open)" : mark == 1 ? "  (done: well)" : mark == 2 ? "  (done: badly)" : "  (done)";
            else if (kind == 3) t += "  (done)";
            else if (!title && det && mark) t = (mark == 1 ? "+ " : "\u00D7 ") + t;
            double ind = title ? 0 : kind == 5 ? 6 * u : kind == 7 ? 30 * u : 14 * u, sz = kind == 4 ? fs * 1.2 : title ? fs * 1.05 : kind == 5 ? fs : kind == 7 ? fs * 0.88 : fs * 0.95;
            double h = kind == 8 ? 0 : richText(cr, tx + ind, 0, tw - ind, t, sz, 1, 1, 1, false) + (title ? 6 : kind == 5 ? 5 : 3) * u;
            if (kind == 8) {   // the town's plan: as wide as the page allows, at most a third of its height... or so
                const HudState::TownPlan *tp = nullptr;
                for (auto &p : s.townPlans) if (p.town == t) tp = &p;
                if (tp && tp->w > 0 && tp->h > 0) {
                    double scale = std::min({(tw - 20 * u) / tp->w, (bottom - top) * 0.5 / tp->h, 5.0 * u});
                    h = tp->h * scale + 12 * u;
                }
            }
            if (title) head = ls.size();
            if (kind == 3 && firstDone == std::string::npos) firstDone = ls.size();
            if (kind == 4 && firstTown == std::string::npos) firstTown = ls.size();
            ls.push_back({kind, mark, t, sz, ind, h, title, head});
        }
        auto contH = [&](size_t i) {   // the title said again at the top of a page that begins inside a story
            const JL &hd = ls[ls[i].head];
            return richText(cr, tx, 0, tw, hd.t + "  (continued)", hd.sz, 1, 1, 1, false) + 6 * u;
        };
        std::vector<size_t> starts{0};
        double y = top;
        for (size_t i = 0; i < ls.size(); i++) {
            bool first = i == starts.back();
            double gap = ls[i].title && !first ? 6 * u : 0;
            bool full = y + gap + ls[i].h - fs > bottom;
            // a title, a heading, a name: not alone at the foot of a page, nor with only another of
            // them under it ("People you have spoken to", then a name without its hours)
            auto isLead = [&](size_t k) { return ls[k].title || ls[k].kind == 5 || (ls[k].kind == 6 && k + 1 < ls.size() && ls[k + 1].kind == 7); };
            bool lead = isLead(i);
            double keep = 0;
            for (size_t k = i + 1; k < ls.size() && !ls[k].title; k++) { keep += ls[k].h; if (!isLead(k)) break; }
            bool orphan = lead && i + 1 < ls.size() && !ls[i + 1].title && y + gap + ls[i].h + keep - fs > bottom;
            if (!first && (full || orphan || i == firstDone || ls[i].kind == 4)) {
                starts.push_back(i);
                y = top + (ls[i].title ? 0 : contH(i));
                gap = 0;
            }
            y += gap + ls[i].h;
        }
        int pages = (int)starts.size();
        journalPages = pages;
        journalJumps.clear();   // where the up and down arrows go: the first page of each part, and of each town
        for (int q = 0; q < pages; q++)
            if (q == 0 || starts[q] == firstDone || (starts[q] < ls.size() && ls[starts[q]].kind == 4)) journalJumps.push_back(q);
        int pg = std::clamp(s.journalPage, 0, pages - 1);
        size_t from = ls.empty() ? 0 : starts[pg], to = pg + 1 < pages ? starts[pg + 1] : ls.size();
        bool finishedPage = firstDone != std::string::npos && from >= firstDone;
        bool townPage = firstTown != std::string::npos && from >= firstTown;
        std::string heading = townPage ? "Places and people" : finishedPage ? "The stories you have finished" : "The stories you are in";
        if (det && !townPage) heading += ", the newest first";
        text(cr, bx + 16 * u, by + 26 * u, heading, 17 * u, true, 1, 0.85, 0.55);
        y = top;
        if (from < to && !ls[from].title) {
            const JL &hd = ls[ls[from].head];
            richText(cr, tx, y, tw, hd.t + "  (continued)", hd.sz, 0.85, 0.72, 0.5);
            y += contH(from);
        }
        for (size_t i = from; i < to; i++) {
            const JL &l = ls[i];
            if (l.title && i != from) y += 6 * u;
            bool well = det && l.mark == 1, badly = det && l.mark == 2;
            if (l.kind == 8) {
                const HudState::TownPlan *tp = nullptr;
                for (auto &p : s.townPlans) if (p.town == l.t) tp = &p;
                auto img = tp ? planImgs.find(tp->town) : planImgs.end();
                if (tp && img != planImgs.end() && img->second && tp->w > 0 && tp->h > 0) {
                    double scale = std::min({(tw - 20 * u) / tp->w, (bottom - top) * 0.5 / tp->h, 5.0 * u});
                    double pw = tp->w * scale, ph = tp->h * scale, px = tx + (tw - pw) / 2, py = y - fs + 4 * u;
                    cairo_save(cr);
                    cairo_translate(cr, px, py);
                    cairo_scale(cr, scale, scale);
                    cairo_set_source_surface(cr, img->second, 0, 0);
                    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
                    cairo_paint(cr);
                    cairo_restore(cr);
                    cairo_set_source_rgba(cr, 0.75, 0.6, 0.35, 0.9);
                    cairo_set_line_width(cr, 1 * u);
                    cairo_rectangle(cr, px + 0.5, py + 0.5, pw - 1, ph - 1);
                    cairo_stroke(cr);
                    auto at = [&](float X, float Y, double &ox, double &oy) {   // world tiles to the page (north up)
                        ox = px + (X - tp->x0) * scale;
                        oy = py + (tp->y0 + tp->h - Y) * scale;
                    };
                    for (size_t m = 0; m < tp->marks.size(); m++) {   // the places you have been, numbered as in the list
                        if (tp->marks[m].first < -1e8f) continue;
                        double ox, oy;
                        at(tp->marks[m].first, tp->marks[m].second, ox, oy);
                        double r = 6.5 * u;
                        cairo_new_path(cr);   // not joined to the last number drawn
                        cairo_arc(cr, ox, oy, r, 0, 2 * M_PI);
                        cairo_set_source_rgba(cr, 0.55, 0.12, 0.1, 0.92);
                        cairo_fill_preserve(cr);
                        cairo_set_source_rgba(cr, 0.95, 0.9, 0.8, 1);
                        cairo_set_line_width(cr, 1 * u);
                        cairo_stroke(cr);
                        std::string num = std::to_string(m + 1);
                        cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                        cairo_set_font_size(cr, (num.size() > 1 ? 7.5 : 9) * u);
                        cairo_text_extents_t te;
                        cairo_text_extents(cr, num.c_str(), &te);
                        cairo_move_to(cr, ox - te.width / 2 - te.x_bearing, oy - te.height / 2 - te.y_bearing);
                        cairo_show_text(cr, num.c_str());
                    }
                    if (tp->youX > -1e8f) {   // you, in your yellow hat
                        double ox, oy;
                        at(tp->youX, tp->youY, ox, oy);
                        cairo_new_path(cr);
                        cairo_arc(cr, ox, oy, 4.5 * u, 0, 2 * M_PI);
                        cairo_set_source_rgba(cr, 0.95, 0.78, 0.14, 1);
                        cairo_fill_preserve(cr);
                        cairo_set_source_rgba(cr, 0.2, 0.15, 0.1, 1);
                        cairo_set_line_width(cr, 1 * u);
                        cairo_stroke(cr);
                    }
                }
            }
            else if (l.kind == 0 || l.kind == 4) richText(cr, tx, y, tw, l.t, l.sz, 1.0, 0.85, 0.55);
            else if (l.kind == 5) richText(cr, tx + l.ind, y, tw - l.ind, l.t, l.sz, 0.85, 0.72, 0.5);
            else if (l.kind == 7) richText(cr, tx + l.ind, y, tw - l.ind, l.t, l.sz, 0.78, 0.75, 0.68);
            else if (l.kind == 3) richText(cr, tx, y, tw, l.t, l.sz, well ? 0.55 : badly ? 0.8 : 0.62, well ? 0.72 : badly ? 0.45 : 0.58, well ? 0.5 : badly ? 0.42 : 0.5);
            else if (l.kind == 2) richText(cr, tx + l.ind, y, tw - l.ind, l.t, l.sz, well ? 0.6 : badly ? 0.85 : 0.66, well ? 0.8 : badly ? 0.5 : 0.62, well ? 0.55 : badly ? 0.46 : 0.54);
            else richText(cr, tx + l.ind, y, tw - l.ind, l.t, l.sz, well ? 0.7 : badly ? 1.0 : 0.96, well ? 0.92 : badly ? 0.62 : 0.93, well ? 0.65 : badly ? 0.56 : 0.85);
            y += l.h;
        }
        cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11 * u);
        cairo_set_source_rgb(cr, 0.7, 0.65, 0.55);
        const char *hint = "J: what you have overheard; Tab: plain or detailed; Esc to close";
        cairo_text_extents_t ex;
        cairo_text_extents(cr, hint, &ex);
        cairo_move_to(cr, bx + bw - ex.width - 14 * u, by + 26 * u);
        cairo_show_text(cr, hint);
        if (pages > 1) {   // which page, and how to turn them
            std::string foot = "Page " + std::to_string(pg + 1) + " of " + std::to_string(pages) +
                               (pg == 0 ? ". The right arrow turns the page" : pg + 1 == pages ? ". The left arrow turns back" : ". Left and right turn the pages") +
                               (journalJumps.size() > 1 ? "; up and down go to the next part, or town." : ".");
            cairo_set_font_size(cr, 12 * u);
            cairo_text_extents(cr, foot.c_str(), &ex);
            cairo_move_to(cr, bx + bw - ex.width - 14 * u, by + bh - 14 * u);
            cairo_show_text(cr, foot.c_str());
        }
    }

    if (s.journal && !s.talking && s.journalTab == 1) {
        double bw = std::min(W - 40 * u, 820 * u), bh = std::min(H - 120 * u, 520 * u), bx = (W - bw) / 2, by = (H - bh) / 2;
        cairo_set_source_rgba(cr, 0.1, 0.07, 0.05, 0.97);
        cairo_rectangle(cr, bx, by, bw, bh);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.75, 0.6, 0.35, 0.9);
        cairo_set_line_width(cr, 1.5 * u);
        cairo_rectangle(cr, bx + 0.5, by + 0.5, bw - 1, bh - 1);
        cairo_stroke(cr);
        text(cr, bx + 16 * u, by + 26 * u, "What you have overheard", 17 * u, true, 1, 0.85, 0.55);
        double fs = 14 * u, tx = bx + 16 * u, tw = bw - 32 * u, top = by + 44 * u, bottom = by + bh - 16 * u;
        std::vector<std::pair<double, size_t>> shown;   // the latest, from the bottom up
        double used = 0;
        for (size_t i = s.journalLines.size(); i-- > 0;) {
            const auto &ln = s.journalLines[i];
            double h = richText(cr, tx, 0, tw, ln.second, ln.first ? fs * 0.85 : fs, 1, 1, 1, false) + (ln.first ? 8 : 3) * u;
            if (used + h > bottom - top) break;
            used += h;
            shown.push_back({h, i});
        }
        double y = top + fs;
        for (size_t k = shown.size(); k-- > 0;) {
            const auto &ln = s.journalLines[shown[k].second];
            if (ln.first) richText(cr, tx, y + 4 * u, tw, ln.second, fs * 0.85, 0.75, 0.68, 0.55);
            else richText(cr, tx + 12 * u, y, tw - 12 * u, ln.second, fs, 0.96, 0.93, 0.85);
            y += shown[k].first;
        }
        cairo_select_font_face(cr, "Palatino", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11 * u);
        cairo_set_source_rgb(cr, 0.7, 0.65, 0.55);
        const char *hint = "J or Esc to close";
        cairo_text_extents_t ex;
        cairo_text_extents(cr, hint, &ex);
        cairo_move_to(cr, bx + bw - ex.width - 14 * u, by + 26 * u);
        cairo_show_text(cr, hint);
    }

    if (s.map && mapImg) {
        double mw = world.L.w, mh = world.L.h;
        double k2 = std::min((W - 80 * u) / mw, (H - 80 * u) / mh);
        double ox = (W - mw * k2) / 2, oy = (H - mh * k2) / 2;
        cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
        cairo_rectangle(cr, ox - 8 * u, oy - 8 * u, mw * k2 + 16 * u, mh * k2 + 16 * u);
        cairo_fill(cr);
        cairo_save(cr);
        cairo_translate(cr, ox, oy);
        cairo_scale(cr, k2, k2);
        cairo_set_source_surface(cr, mapImg, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
        cairo_paint(cr);
        cairo_restore(cr);
        auto toMap = [&](double X, double Y, double &mx, double &my) {
            mx = ox + (X / world.S) * k2;
            my = oy + (mh - Y / world.S) * k2;
        };
        // the towns as gold dots, named once you have been there; the post houses and ventas as small
        // squares, never named; a farm or a ruin only once you have been there (so the map stays
        // readable)
        auto keyOf = [](const std::string &n) { std::string k; for (char c : n) k += c == ' ' ? '-' : (char)std::tolower((unsigned char)c); return k; };
        for (auto &t : world.settlements) {
            double mx, my;
            toMap(t.x0 + t.w * 0.5, t.y0 + t.h * 0.5, mx, my);
            bool been = s.beenTowns.count(keyOf(t.name)) > 0;
            if (t.hamlet) {
                bool post = t.file.find("posthouse") != std::string::npos;
                if (!post && !been) continue;
                // a small pale square with a dark edge (it must not melt into the land); a farm or a ruin you have found, the same in green
                // a post house you have not been through is faint; once you have, solid (the map fills in behind you)
                double a = been ? 1.0 : 0.38;
                cairo_rectangle(cr, mx - 2.6 * u, my - 2.6 * u, 5.2 * u, 5.2 * u);
                if (post) cairo_set_source_rgba(cr, 0.96, 0.93, 0.84, a); else cairo_set_source_rgba(cr, 0.75, 0.92, 0.7, a);
                cairo_fill_preserve(cr);
                cairo_set_source_rgba(cr, 0.2, 0.15, 0.1, a);
                cairo_set_line_width(cr, 1.0 * u);
                cairo_stroke(cr);
                continue;
            }
            cairo_set_source_rgba(cr, 0.95, 0.85, 0.4, been ? 1.0 : 0.4);   // a town: faint until you have been there
            cairo_arc(cr, mx, my, 4 * u, 0, 6.2832);
            cairo_fill(cr);
            if (been) text(cr, mx + 7 * u, my + 4 * u, t.name, 13 * u, true);
        }
        double mx, my;
        toMap(s.px, s.py, mx, my);
        cairo_set_source_rgb(cr, 0.9, 0.1, 0.1);
        cairo_arc(cr, mx, my, 5 * u, 0, 6.2832);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_line_width(cr, 1.5 * u);
        cairo_arc(cr, mx, my, 9 * u, 0, 6.2832);
        cairo_stroke(cr);
        text(cr, ox + 10 * u, oy + 24 * u, "Europe, 1632", 18 * u, true);
    }
    if (s.fade > 0) {   // into the dark and out again, over everything
        cairo_set_source_rgba(cr, 0, 0, 0, std::min(1.0f, s.fade));
        cairo_paint(cr);
    }
    cairo_destroy(cr);
    cairo_surface_flush(surf);
    return true;
}
