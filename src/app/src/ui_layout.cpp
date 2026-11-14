#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>

#include <anno/app/ui_layout.hpp>

namespace anno::app {

namespace {

/// A line split on ASCII space/tab (data/ui/README.md grammar), comments and blank lines
/// already dropped by the caller.
struct Tokens {
    std::vector<std::string> words;
    std::size_t next = 0;

    [[nodiscard]] bool more() const { return next < words.size(); }
    [[nodiscard]] const std::string& peek() const { return words[next]; }
    std::string take() { return words[next++]; }
};

Tokens tokenize(const std::string& line) {
    Tokens t;
    std::istringstream in(line);
    std::string w;
    while (in >> w) {
        t.words.push_back(w);
    }
    return t;
}

std::optional<int> to_int(const std::string& s) {
    if (s.empty())
        return std::nullopt;
    int value = 0;
    const char* begin = s.data();
    const char* end = s.data() + s.size();
    const auto res = std::from_chars(begin, end, value);
    if (res.ec != std::errc{} || res.ptr != end)
        return std::nullopt;
    return value;
}

std::optional<std::int64_t> to_i64(const std::string& s) {
    if (s.empty())
        return std::nullopt;
    std::int64_t value = 0;
    const char* begin = s.data();
    const char* end = s.data() + s.size();
    const auto res = std::from_chars(begin, end, value);
    if (res.ec != std::errc{} || res.ptr != end)
        return std::nullopt;
    return value;
}

std::string fail(std::size_t line_no, const std::string& what) {
    return "line " + std::to_string(line_no) + ": " + what;
}

/// UI-WINDOW-1: an `anchor <value>` field (data/ui/README.md).
std::optional<UiAnchor> to_anchor(const std::string& s) {
    if (s == "right")
        return UiAnchor::Right;
    if (s == "bottom")
        return UiAnchor::Bottom;
    if (s == "right-bottom")
        return UiAnchor::RightBottom;
    if (s == "after-statusbar")
        return UiAnchor::AfterStatusbar;
    return std::nullopt;
}

/// data/ui/README.md: "`-` as a whole field means absent".
std::string opt_str(std::string s) {
    return s == "-" ? std::string() : s;
}

/// Splits `text` into (line number, tokens) pairs, dropping blank lines and `#` comments, per
/// data/ui/README.md's grammar (CRLF/LF equivalent, line numbers count every physical line).
std::vector<std::pair<std::size_t, Tokens>> split_lines(const std::string& text) {
    std::vector<std::pair<std::size_t, Tokens>> out;
    std::size_t pos = 0;
    std::size_t line_no = 0;
    while (pos <= text.size()) {
        std::size_t nl = text.find('\n', pos);
        const std::size_t end = (nl == std::string::npos) ? text.size() : nl;
        std::string line = text.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        ++line_no;
        std::size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line[first] != '#') {
            out.emplace_back(line_no, tokenize(line));
        }
        if (nl == std::string::npos)
            break;
        pos = nl + 1;
    }
    return out;
}

} // namespace

const UiElement* HudLayout::element(const std::string& name) const {
    for (const auto& e : elements) {
        if (e.name == name)
            return &e;
    }
    return nullptr;
}

bool point_in_hud_panel(const HudLayout& hud, double px, double py) {
    const UiElement* panel = hud.element("panel");
    if (panel == nullptr || !panel->has_pos || !panel->has_size) {
        return false;
    }
    return px >= panel->pos_x && px < panel->pos_x + static_cast<double>(panel->size_w) &&
           py >= panel->pos_y && py < panel->pos_y + static_cast<double>(panel->size_h);
}

std::pair<int, int> UiPlacement::place(UiAnchor anchor, int x, int y) const {
    switch (anchor) {
    case UiAnchor::TopLeft:
        return {x, y};
    case UiAnchor::Right:
        return {x + dx, y};
    case UiAnchor::Bottom:
        return {x, y + dy};
    case UiAnchor::RightBottom:
        return {x + dx, y + dy};
    case UiAnchor::AfterStatusbar:
        return {statusbar_end, y + dy};
    }
    return {x, y};
}

UiPlacement place_hud(const HudLayout& hud, int ui_w, int ui_h) {
    UiPlacement p;
    p.ref_w = hud.resolution_w;
    p.ref_h = hud.resolution_h;
    p.ui_w = std::max(ui_w, p.ref_w);
    p.ui_h = std::max(ui_h, p.ref_h);
    p.dx = p.ui_w - p.ref_w;
    p.dy = p.ui_h - p.ref_h;
    // The status bar's span: the original's own graphic for this width when there is one
    // (AnnoFrame.dll 0x10063125..0x10063153; hud.txt `for_width`), else the 1152/1280 graphics'
    // rule, width - panel - question mark (data/ui/hud.txt comments, docs/re/ui.md 5.1).
    for (const UiElement& e : hud.elements) {
        if (e.for_width && *e.for_width == p.ui_w && e.has_size) {
            p.statusbar_end = static_cast<int>(e.size_w);
            return p;
        }
    }
    const UiElement* panel = hud.element("panel");
    const UiElement* mark = hud.element("fragezeichen");
    if (panel != nullptr && panel->has_size && mark != nullptr && mark->has_size) {
        p.statusbar_end = p.ui_w - static_cast<int>(panel->size_w) - static_cast<int>(mark->size_w);
    } else if (const UiElement* bar = hud.element("statusbar"); bar != nullptr && bar->has_size) {
        p.statusbar_end = static_cast<int>(bar->size_w);
    }
    return p;
}

UiHit ui_hit(const HudLayout& hud, const UiPlacement& placement, int ux, int uy, bool paused,
             const std::string& shown_panel) {
    UiHit hit;
    if (ux < 0 || uy < 0 || ux >= placement.ui_w || uy >= placement.ui_h)
        return hit;
    const auto inside = [](int px, int py, int x, int y, int w, int h) {
        return px >= x && py >= y && px < x + w && py < y + h;
    };
    // The pause parchment, drawn last and over everything (ui_mock.cpp), so it is checked first.
    if (paused) {
        if (const UiElement* e = hud.element("pause_parchment");
            e != nullptr && e->has_pos && e->has_size) {
            const auto [x, y] = placement.place(e->anchor, e->pos_x, e->pos_y);
            if (inside(ux, uy, x, y, static_cast<int>(e->size_w), static_cast<int>(e->size_h))) {
                hit.chrome = true;
                hit.ref = std::pair{ux - (x - e->pos_x), uy - (y - e->pos_y)};
                return hit;
            }
        }
    }
    // The shown tab panel's elements (fix round R3): its sub-tabs stick out left of the column.
    if (!shown_panel.empty()) {
        for (const UiElement& e : hud.elements) {
            if (!e.has_pos || !e.has_size ||
                std::find(e.panels.begin(), e.panels.end(), shown_panel) == e.panels.end())
                continue;
            const auto [x, y] = placement.place(e.anchor, e.pos_x, e.pos_y);
            if (inside(ux, uy, x, y, static_cast<int>(e.size_w), static_cast<int>(e.size_h))) {
                hit.chrome = true;
                hit.ref = std::pair{ux - (x - e.pos_x), uy - (y - e.pos_y)};
                return hit;
            }
        }
    }
    // The right chrome column: the panel's x and width, from the panel's top to the layer's
    // bottom (the panel, then the filler under it).
    if (const UiElement* panel = hud.element("panel");
        panel != nullptr && panel->has_pos && panel->has_size) {
        const auto [x, y] = placement.place(panel->anchor, panel->pos_x, panel->pos_y);
        if (inside(ux, uy, x, y, static_cast<int>(panel->size_w), placement.ui_h - y)) {
            hit.chrome = true;
            const int rx = ux - (x - panel->pos_x), ry = uy - (y - panel->pos_y);
            if (ry < placement.ref_h)
                hit.ref = std::pair{rx, ry};
            return hit;
        }
    }
    // The status bar row: the bar's span, then the question mark.
    const UiElement* bar = hud.element("statusbar");
    const UiElement* mark = hud.element("fragezeichen");
    if (bar != nullptr && bar->has_pos && bar->has_size) {
        const auto [bx, by] = placement.place(bar->anchor, bar->pos_x, bar->pos_y);
        const int mark_w = mark != nullptr && mark->has_size ? static_cast<int>(mark->size_w) : 0;
        if (inside(ux, uy, bx, by, placement.statusbar_end + mark_w - bx,
                   static_cast<int>(bar->size_h))) {
            hit.chrome = true;
            const int ry = uy - (by - bar->pos_y);
            if (ux >= placement.statusbar_end) {
                if (mark != nullptr && mark->has_pos)
                    hit.ref = std::pair{mark->pos_x + ux - placement.statusbar_end, ry};
            } else if (ux - (bx - bar->pos_x) < bar->pos_x + static_cast<int>(bar->size_w)) {
                hit.ref = std::pair{ux - (bx - bar->pos_x), ry};
            }
            return hit;
        }
    }
    return hit;
}

const ConstructionCategory* ConstructionLayout::category(const std::string& name) const {
    for (const auto& c : categories) {
        if (c.name == name)
            return &c;
    }
    return nullptr;
}

const UiMask* MaskLayout::mask(const std::string& name) const {
    for (const auto& m : masks) {
        if (m.name == name)
            return &m;
    }
    return nullptr;
}

const ConstructionCell* ConstructionLayout::cell(const std::string& cat, int col, int row) const {
    for (const auto& c : cells) {
        if (c.category == cat && c.col == col && c.row == row)
            return &c;
    }
    return nullptr;
}

std::optional<std::pair<int, int>> ConstructionGrid::click_centre(int col, int row) const {
    if (col < 0 || row < 0 || static_cast<std::size_t>(col) >= click_x.size() ||
        static_cast<std::size_t>(row) >= click_y.size())
        return std::nullopt;
    return std::make_pair(click_x[static_cast<std::size_t>(col)],
                          click_y[static_cast<std::size_t>(row)]);
}

formats::Result<HudLayout, std::string> parse_hud_layout(const std::string& text) {
    HudLayout out;
    bool saw_version = false;
    for (auto& [line_no, tok] : split_lines(text)) {
        if (!tok.more())
            continue;
        const std::string kind = tok.take();
        if (!saw_version) {
            if (kind != "anno1503-ui-hud")
                return fail(line_no, "expected version line 'anno1503-ui-hud 1'");
            saw_version = true;
            continue;
        }
        if (kind == "resolution") {
            if (!tok.more())
                return fail(line_no, "resolution: missing width");
            const auto w = to_int(tok.take());
            if (!tok.more())
                return fail(line_no, "resolution: missing height");
            const auto h = to_int(tok.take());
            if (!w || !h)
                return fail(line_no, "resolution: not integers");
            out.resolution_w = *w;
            out.resolution_h = *h;
        } else if (kind == "shot") {
            if (!tok.more())
                return fail(line_no, "shot: missing name");
            out.shot = tok.take();
        } else if (kind == "element") {
            if (!tok.more())
                return fail(line_no, "element: missing name");
            UiElement e;
            e.name = tok.take();
            while (tok.more()) {
                const std::string key = tok.take();
                if (key == "pos") {
                    if (tok.words.size() < tok.next + 2)
                        return fail(line_no, "pos: missing x/y");
                    const auto x = to_int(tok.take());
                    const auto y = to_int(tok.take());
                    if (!x || !y)
                        return fail(line_no, "pos: not integers");
                    e.has_pos = true;
                    e.pos_x = *x;
                    e.pos_y = *y;
                } else if (key == "size") {
                    if (tok.words.size() < tok.next + 2)
                        return fail(line_no, "size: missing w/h");
                    const auto w = to_int(tok.take());
                    const auto h = to_int(tok.take());
                    if (!w || *w < 0 || !h || *h < 0)
                        return fail(line_no, "size: not non-negative integers");
                    e.has_size = true;
                    e.size_w = static_cast<unsigned>(*w);
                    e.size_h = static_cast<unsigned>(*h);
                } else if (key == "file") {
                    if (!tok.more())
                        return fail(line_no, "file: missing value");
                    e.file = opt_str(tok.take());
                } else if (key == "access") {
                    if (!tok.more())
                        return fail(line_no, "access: missing value");
                    e.access = opt_str(tok.take());
                } else if (key == "tex_id" || key == "segment" || key == "frame_idle" ||
                           key == "frame_selected" || key == "frame_normal" ||
                           key == "frame_active") {
                    if (!tok.more())
                        return fail(line_no, key + ": missing value");
                    const auto v = to_int(tok.take());
                    if (!v)
                        return fail(line_no, key + ": not an integer");
                    if (key == "tex_id")
                        e.tex_id = v;
                    else if (key == "segment")
                        e.segment = v;
                    else if (key == "frame_idle")
                        e.frame_idle = v;
                    else if (key == "frame_selected")
                        e.frame_selected = v;
                    else if (key == "frame_normal")
                        e.frame_normal = v;
                    else
                        e.frame_active = v;
                } else if (key == "hotkey") {
                    if (!tok.more())
                        return fail(line_no, "hotkey: missing value");
                    const std::string v = tok.take();
                    if (v.size() != 1)
                        return fail(line_no, "hotkey: not one character");
                    e.hotkey = v[0];
                } else if (key == "method") {
                    if (!tok.more())
                        return fail(line_no, "method: missing value");
                    e.method = opt_str(tok.take());
                } else if (key == "shot") {
                    if (!tok.more())
                        return fail(line_no, "shot: missing value");
                    e.shot = opt_str(tok.take());
                } else if (key == "align") {
                    if (!tok.more())
                        return fail(line_no, "align: missing value");
                    e.align = tok.take();
                    if (e.align != "centre")
                        return fail(line_no, "align: only 'centre' is defined");
                } else if (key == "anchor") {
                    if (!tok.more())
                        return fail(line_no, "anchor: missing value");
                    const auto a = to_anchor(tok.take());
                    if (!a)
                        return fail(line_no, "anchor: not right, bottom, right-bottom or "
                                             "after-statusbar");
                    e.anchor = *a;
                } else if (key == "panel") {
                    if (!tok.more())
                        return fail(line_no, "panel: missing value");
                    std::string list = tok.take();
                    for (std::size_t at = 0; at <= list.size();) {
                        const std::size_t comma = std::min(list.find(',', at), list.size());
                        if (comma == at)
                            return fail(line_no, "panel: empty name");
                        e.panels.push_back(list.substr(at, comma - at));
                        at = comma + 1;
                    }
                } else if (key == "for_width") {
                    if (!tok.more())
                        return fail(line_no, "for_width: missing value");
                    const auto v = to_int(tok.take());
                    if (!v || *v <= 0)
                        return fail(line_no, "for_width: not a positive integer");
                    e.for_width = v;
                } else {
                    return fail(line_no, "element: unknown key '" + key + "'");
                }
            }
            out.elements.push_back(std::move(e));
        } else if (kind == "click") {
            if (!tok.more())
                return fail(line_no, "click: missing name");
            UiClick c;
            c.name = tok.take();
            if (!tok.more())
                return fail(line_no, "click: missing value");
            if (tok.peek() == "pos") {
                tok.take();
                if (tok.words.size() < tok.next + 2)
                    return fail(line_no, "click: missing x/y");
                const auto x = to_int(tok.take());
                const auto y = to_int(tok.take());
                if (!x || !y)
                    return fail(line_no, "click: pos not integers");
                c.pos_x = *x;
                c.pos_y = *y;
                c.has_y = true;
            } else {
                // A single-coordinate click (e.g. `click category_column_x 782`,
                // docs/re/ui.md section 2.7), not a rectangle centre.
                const auto x = to_int(tok.take());
                if (!x)
                    return fail(line_no, "click: value not an integer");
                c.pos_x = *x;
                c.has_y = false;
            }
            out.clicks.push_back(c);
        } else {
            return fail(line_no, "unknown line kind '" + kind + "'");
        }
    }
    if (!saw_version)
        return std::string("empty file: missing version line");
    return out;
}

formats::Result<ConstructionLayout, std::string>
parse_construction_layout(const std::string& text) {
    ConstructionLayout out;
    bool saw_version = false;
    for (auto& [line_no, tok] : split_lines(text)) {
        if (!tok.more())
            continue;
        const std::string kind = tok.take();
        if (!saw_version) {
            if (kind != "anno1503-ui-construction")
                return fail(line_no, "expected version line 'anno1503-ui-construction 1'");
            saw_version = true;
            continue;
        }
        if (kind == "resolution") {
            if (tok.words.size() < tok.next + 2)
                return fail(line_no, "resolution: missing width/height");
            const auto w = to_int(tok.take());
            const auto h = to_int(tok.take());
            if (!w || !h)
                return fail(line_no, "resolution: not integers");
            out.resolution_w = *w;
            out.resolution_h = *h;
        } else if (kind == "open_key") {
            if (!tok.more())
                return fail(line_no, "open_key: missing value");
            const std::string v = tok.take();
            if (v.size() != 1)
                return fail(line_no, "open_key: not one character");
            out.open_key = v[0];
        } else if (kind == "leave_key") {
            if (!tok.more())
                return fail(line_no, "leave_key: missing value");
            out.leave_key = opt_str(tok.take());
        } else if (kind == "shot") {
            if (!tok.more())
                return fail(line_no, "shot: missing value");
            out.shot = opt_str(tok.take());
        } else if (kind == "grid") {
            ConstructionGrid g;
            while (tok.more()) {
                const std::string key = tok.take();
                if (key == "cols" || key == "rows" || key == "cell_px" || key == "origin_x" ||
                    key == "origin_y" || key == "sprite") {
                    if (!tok.more())
                        return fail(line_no, key + ": missing value");
                    const auto v = to_int(tok.take());
                    if (!v)
                        return fail(line_no, key + ": not an integer");
                    if (key == "cols")
                        g.cols = *v;
                    else if (key == "rows")
                        g.rows = *v;
                    else if (key == "cell_px")
                        g.cell_px = *v;
                    else if (key == "origin_x")
                        g.origin_x = *v;
                    else if (key == "origin_y")
                        g.origin_y = *v;
                    else
                        g.sprite = *v;
                } else if (key == "method") {
                    if (!tok.more())
                        return fail(line_no, "method: missing value");
                    g.method = opt_str(tok.take());
                } else if (key == "shot") {
                    if (!tok.more())
                        return fail(line_no, "shot: missing value");
                    g.shot = opt_str(tok.take());
                } else if (key == "anchor") {
                    if (!tok.more())
                        return fail(line_no, "anchor: missing value");
                    const auto a = to_anchor(tok.take());
                    if (!a)
                        return fail(line_no, "anchor: not right, bottom, right-bottom or "
                                             "after-statusbar");
                    g.anchor = *a;
                } else {
                    return fail(line_no, "grid: unknown key '" + key + "'");
                }
            }
            out.grid.cols = g.cols;
            out.grid.rows = g.rows;
            out.grid.cell_px = g.cell_px;
            out.grid.origin_x = g.origin_x;
            out.grid.origin_y = g.origin_y;
            out.grid.sprite = g.sprite;
            out.grid.method = g.method;
            out.grid.shot = g.shot;
            out.grid.anchor = g.anchor;
        } else if (kind == "grid_click_x" || kind == "grid_click_y") {
            std::vector<int> values;
            while (tok.more()) {
                const auto v = to_int(tok.take());
                if (!v)
                    return fail(line_no, kind + ": not an integer");
                values.push_back(*v);
            }
            if (kind == "grid_click_x")
                out.grid.click_x = std::move(values);
            else
                out.grid.click_y = std::move(values);
        } else if (kind == "category") {
            if (!tok.more())
                return fail(line_no, "category: missing name");
            ConstructionCategory c;
            c.name = tok.take();
            while (tok.more()) {
                const std::string key = tok.take();
                if (key == "index" || key == "y" || key == "thumb_tex" || key == "records") {
                    if (!tok.more())
                        return fail(line_no, key + ": missing value");
                    const auto v = to_int(tok.take());
                    if (!v)
                        return fail(line_no, key + ": not an integer");
                    if (key == "index")
                        c.index = *v;
                    else if (key == "y")
                        c.y = *v;
                    else if (key == "thumb_tex")
                        c.thumb_tex = *v;
                    else
                        c.records = *v;
                } else if (key == "banner_key" || key == "tooltip_section" || key == "thumbnail" ||
                           key == "table" || key == "shot") {
                    if (!tok.more())
                        return fail(line_no, key + ": missing value");
                    const std::string v = opt_str(tok.take());
                    if (key == "banner_key")
                        c.banner_key = v;
                    else if (key == "tooltip_section")
                        c.tooltip_section = v;
                    else if (key == "thumbnail")
                        c.thumbnail = v;
                    else if (key == "table")
                        c.table = v;
                    else
                        c.shot = v;
                } else {
                    return fail(line_no, "category: unknown key '" + key + "'");
                }
            }
            out.categories.push_back(std::move(c));
        } else if (kind == "cell") {
            if (tok.words.size() < tok.next + 3)
                return fail(line_no, "cell: missing category/col/row");
            ConstructionCell c;
            c.category = tok.take();
            const auto col = to_int(tok.take());
            const auto row = to_int(tok.take());
            if (!col || !row)
                return fail(line_no, "cell: col/row not integers");
            c.col = *col;
            c.row = *row;
            while (tok.more()) {
                const std::string key = tok.take();
                if (key == "id" || key == "tools" || key == "wood" || key == "bricks" ||
                    key == "marble" || key == "upkeep") {
                    if (!tok.more())
                        return fail(line_no, key + ": missing value");
                    const auto v = to_int(tok.take());
                    if (!v)
                        return fail(line_no, key + ": not an integer");
                    if (key == "id") {
                        if (*v < 0)
                            return fail(line_no, "id: negative");
                        c.id = static_cast<std::uint32_t>(*v);
                    } else if (key == "tools")
                        c.tools = *v;
                    else if (key == "wood")
                        c.wood = *v;
                    else if (key == "bricks")
                        c.bricks = *v;
                    else if (key == "marble")
                        c.marble = *v;
                    else
                        c.upkeep = *v;
                } else if (key == "gold") {
                    if (!tok.more())
                        return fail(line_no, "gold: missing value");
                    const auto v = to_i64(tok.take());
                    if (!v)
                        return fail(line_no, "gold: not an integer");
                    c.gold = *v;
                } else if (key == "state") {
                    if (!tok.more())
                        return fail(line_no, "state: missing value");
                    const std::string v = tok.take();
                    if (v == "unlocked")
                        c.unlocked = true;
                    else if (v == "locked")
                        c.unlocked = false;
                    else
                        return fail(line_no, "state: not 'unlocked' or 'locked'");
                } else if (key == "inframin" || key == "source" || key == "shot") {
                    if (!tok.more())
                        return fail(line_no, key + ": missing value");
                    const std::string v = opt_str(tok.take());
                    if (key == "inframin")
                        c.inframin = v;
                    else if (key == "source")
                        c.source = v;
                    else
                        c.shot = v;
                } else {
                    return fail(line_no, "cell: unknown key '" + key + "'");
                }
            }
            out.cells.push_back(std::move(c));
        } else if (kind == "occupancy") {
            // RE evidence trail only (docs/re/ui.md section 3.2): every cell already carries its
            // own `unlocked` from `state`. Consumed here only to accept the line.
            continue;
        } else if (kind == "demolish") {
            ConstructionDemolish d;
            while (tok.more()) {
                const std::string key = tok.take();
                if (key == "pos") {
                    if (tok.words.size() < tok.next + 2)
                        return fail(line_no, "pos: missing x/y");
                    const auto x = to_int(tok.take());
                    const auto y = to_int(tok.take());
                    if (!x || !y)
                        return fail(line_no, "pos: not integers");
                    d.pos_x = *x;
                    d.pos_y = *y;
                } else if (key == "shot" || key == "access") {
                    if (!tok.more())
                        return fail(line_no, key + ": missing value");
                    const std::string v = opt_str(tok.take());
                    if (key == "shot")
                        d.shot = v;
                    else
                        d.access = v;
                } else {
                    return fail(line_no, "demolish: unknown key '" + key + "'");
                }
            }
            out.demolish = d;
        } else {
            return fail(line_no, "unknown line kind '" + kind + "'");
        }
    }
    if (!saw_version)
        return std::string("empty file: missing version line");
    return out;
}

formats::Result<MaskLayout, std::string> parse_mask_layout(const std::string& text) {
    MaskLayout out;
    bool saw_version = false;
    for (auto& [line_no, tok] : split_lines(text)) {
        if (!tok.more())
            continue;
        const std::string kind = tok.take();
        if (!saw_version) {
            if (kind != "anno1503-ui-masks")
                return fail(line_no, "expected version line 'anno1503-ui-masks 1'");
            saw_version = true;
            continue;
        }
        if (kind == "resolution") {
            if (tok.words.size() < tok.next + 2)
                return fail(line_no, "resolution: missing width/height");
            const auto w = to_int(tok.take());
            const auto h = to_int(tok.take());
            if (!w || !h)
                return fail(line_no, "resolution: not integers");
            out.resolution_w = *w;
            out.resolution_h = *h;
        } else if (kind == "mask") {
            if (!tok.more())
                return fail(line_no, "mask: missing name");
            UiMask m;
            m.name = tok.take();
            while (tok.more()) {
                const std::string key = tok.take();
                if (key == "pos") {
                    if (tok.words.size() < tok.next + 2)
                        return fail(line_no, "pos: missing x/y");
                    const auto x = to_int(tok.take());
                    const auto y = to_int(tok.take());
                    if (!x || !y)
                        return fail(line_no, "pos: not integers");
                    m.pos_x = *x;
                    m.pos_y = *y;
                } else if (key == "size") {
                    if (tok.words.size() < tok.next + 2)
                        return fail(line_no, "size: missing w/h");
                    const auto w = to_int(tok.take());
                    const auto h = to_int(tok.take());
                    if (!w || *w < 0 || !h || *h < 0)
                        return fail(line_no, "size: not non-negative integers");
                    m.size_w = static_cast<unsigned>(*w);
                    m.size_h = static_cast<unsigned>(*h);
                } else if (key == "reason") {
                    if (!tok.more())
                        return fail(line_no, "reason: missing value");
                    m.reason = opt_str(tok.take());
                } else if (key == "anchor") {
                    if (!tok.more())
                        return fail(line_no, "anchor: missing value");
                    const auto a = to_anchor(tok.take());
                    if (!a)
                        return fail(line_no, "anchor: not right, bottom, right-bottom or "
                                             "after-statusbar");
                    m.anchor = *a;
                } else {
                    return fail(line_no, "mask: unknown key '" + key + "'");
                }
            }
            out.masks.push_back(std::move(m));
        } else {
            return fail(line_no, "unknown line kind '" + kind + "'");
        }
    }
    if (!saw_version)
        return std::string("empty file: missing version line");
    return out;
}

namespace {
std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}
} // namespace

formats::Result<HudLayout, std::string> load_hud_layout(const std::filesystem::path& path) {
    const auto text = read_file(path);
    if (!text)
        return "cannot read " + path.string();
    return parse_hud_layout(*text);
}

formats::Result<ConstructionLayout, std::string>
load_construction_layout(const std::filesystem::path& path) {
    const auto text = read_file(path);
    if (!text)
        return "cannot read " + path.string();
    return parse_construction_layout(*text);
}

formats::Result<MaskLayout, std::string> load_mask_layout(const std::filesystem::path& path) {
    const auto text = read_file(path);
    if (!text)
        return "cannot read " + path.string();
    return parse_mask_layout(*text);
}

} // namespace anno::app
