// UI.2 R1 (ADR 0004 "Widgets"): the HUD and construction-menu layout, read from `data/ui/hud.txt`
// and `data/ui/construction.txt` (docs/re/ui.md, data/ui/README.md). No coordinate, type id or
// Texte.dat section lives in code: the widget tree is this parsed data, and a screen or an element
// changes by editing `data/ui/`, not by recompiling. These files hold only derived facts (ids,
// coordinates, sizes, file names, ACCESS names, indices) per the grammar in data/ui/README.md:
// never original pixels, decoded images or UI strings.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <anno/formats/result.hpp>

namespace anno::app {

/// One `element` line of hud.txt: a rectangle (when `has_pos`/`has_size`), its source texture and
/// how it was bound (docs/re/ui.md section 2.8's `method` column).
struct UiElement {
    std::string name;
    bool has_pos = false;
    int pos_x = 0, pos_y = 0;
    bool has_size = false;
    unsigned size_w = 0, size_h = 0;
    std::string file;
    std::string access;
    std::optional<int> tex_id;
    std::optional<int> frame_idle, frame_selected, frame_normal, frame_active;
    std::optional<int> segment;
    std::optional<char> hotkey;
    std::string method;
    std::string shot;
};

/// One `click` line of hud.txt: either a rectangle centre (`click <name> pos <x> <y>`) in drawable
/// pixels at the 1024 x 768 reference (data/ui/hud.txt header comment; not a screens.json
/// fraction), or a single coordinate (`click <name> <x>`, e.g. `category_column_x`: docs/re/ui.md
/// section 2.7) when `has_y` is false.
struct UiClick {
    std::string name;
    int pos_x = 0, pos_y = 0;
    bool has_y = false;
};

struct HudLayout {
    int resolution_w = 0, resolution_h = 0;
    std::string shot;
    std::vector<UiElement> elements;
    std::vector<UiClick> clicks;

    /// nullptr when no element of that name exists.
    [[nodiscard]] const UiElement* element(const std::string& name) const;
};

/// Whether (`px`,`py`) HUD-reference drawable pixels fall inside `hud`'s own "panel" rectangle
/// (ADR 0004 "Selection", finding 2 of the 2026-11-11 review): the right-side chrome hud.txt
/// places at (779,0) 245x733 (`MAINUNTERGRUND`, method `atlas-right`), which covers every element
/// drawn within it (the tabs, the selected-building panel and its need icons, the close button).
/// A pure geometry test, not a drawing decision: true whether or not that panel is currently
/// shown, since the right chrome is drawn at every HUD-idle screen regardless (docs/re/ui.md,
/// `hud_idle`). False when `hud` has no "panel" element at all (no HUD layout loaded, e.g. a
/// window without --construction-menu): a click can never land on drawn UI that doesn't exist.
[[nodiscard]] bool point_in_hud_panel(const HudLayout& hud, double px, double py);

/// One `category` line of construction.txt (docs/re/ui.md section 3.1).
struct ConstructionCategory {
    std::string name;
    int index = 0;
    int y = 0;
    std::string banner_key;
    /// `Texte.dat` section name (e.g. "TOOLTIP:BAUMENU_DIVERSES"), never the strings themselves.
    std::string tooltip_section;
    std::string thumbnail;
    int thumb_tex = 0;
    /// The View.dll table's virtual address, as a hex string (evidence trail, not used to
    /// address memory): e.g. "0x100a7330".
    std::string table;
    int records = 0;
    std::string shot;
};

/// One `cell` line of construction.txt (docs/re/ui.md section 3.3): a grid cell's Anlagen type id
/// and the original's lock state and HEAD/PROD3 costs.
struct ConstructionCell {
    std::string category;
    int col = 0, row = 0;
    std::uint32_t id = 0;
    /// true for `state unlocked`, false for `state locked` (docs/re/ui.md section 3.2).
    bool unlocked = false;
    std::string inframin;
    std::int64_t gold = 0;
    int tools = 0, wood = 0, bricks = 0, marble = 0;
    int upkeep = 0;
    std::string source;
    std::string shot;
};

/// The `grid` line of construction.txt: the 4 x 8 cell geometry and its click centres
/// (docs/re/ui.md section 2.4/3).
struct ConstructionGrid {
    int cols = 0, rows = 0, cell_px = 0, origin_x = 0, origin_y = 0, sprite = 0;
    std::string method, shot;
    std::vector<int> click_x, click_y;

    /// The click centre of (col, row), or nullopt outside the grid.
    [[nodiscard]] std::optional<std::pair<int, int>> click_centre(int col, int row) const;
};

/// The `demolish` line of construction.txt (the pickaxe).
struct ConstructionDemolish {
    int pos_x = 0, pos_y = 0;
    std::string shot;
    std::string access;
};

/// One `mask` line of masks.txt (docs/re/ui.md's pixel-comparison exclusions): a sprite-sad-
/// measured rectangle and why it is excluded from the oracle comparison (`reason`, evidence only -
/// never read for drawing logic itself). The live-digit masks (`gold_digits`,
/// `baumat_{tools,wood,bricks,marble}_d`, `einwohner_digits`) are the only real measured rectangle
/// for where the HUD's live numbers belong (H2) - draw_live_screen_xrgb/_rgba read them from here
/// rather than carrying their own copy of the coordinates (ADR 0004 R1's "no coordinate lives in
/// code", the same rule category_at() follows for hud.txt's `viewbautab_0`).
struct UiMask {
    std::string name;
    int pos_x = 0, pos_y = 0;
    unsigned size_w = 0, size_h = 0;
    std::string reason;
};

struct MaskLayout {
    int resolution_w = 0, resolution_h = 0;
    std::vector<UiMask> masks;

    /// nullptr when no mask of that name exists.
    [[nodiscard]] const UiMask* mask(const std::string& name) const;
};

struct ConstructionLayout {
    int resolution_w = 0, resolution_h = 0;
    std::optional<char> open_key;
    std::string leave_key;
    std::string shot;
    ConstructionGrid grid;
    std::vector<ConstructionCategory> categories;
    std::vector<ConstructionCell> cells;
    ConstructionDemolish demolish;

    /// nullptr when no category of that name exists.
    [[nodiscard]] const ConstructionCategory* category(const std::string& name) const;
    /// nullptr when (category, col, row) is empty or not in the file.
    [[nodiscard]] const ConstructionCell* cell(const std::string& category, int col, int row) const;
};

/// Parses hud.txt's text (the version line `anno1503-ui-hud 1` is required). An error names the
/// line number and what was wrong.
[[nodiscard]] formats::Result<HudLayout, std::string> parse_hud_layout(const std::string& text);
/// Parses construction.txt's text (`anno1503-ui-construction 1`).
[[nodiscard]] formats::Result<ConstructionLayout, std::string>
parse_construction_layout(const std::string& text);
/// Parses masks.txt's text (`anno1503-ui-masks 1`).
[[nodiscard]] formats::Result<MaskLayout, std::string> parse_mask_layout(const std::string& text);

/// Reads `path` and parses it as hud.txt / construction.txt / masks.txt. An error also names the
/// file when it cannot be read.
[[nodiscard]] formats::Result<HudLayout, std::string>
load_hud_layout(const std::filesystem::path& path);
[[nodiscard]] formats::Result<ConstructionLayout, std::string>
load_construction_layout(const std::filesystem::path& path);
[[nodiscard]] formats::Result<MaskLayout, std::string>
load_mask_layout(const std::filesystem::path& path);

} // namespace anno::app
