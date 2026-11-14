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

/// UI-WINDOW-1: how a rectangle measured at the 1024 x 768 reference moves when the UI layer is
/// larger (`anchor` in data/ui/hud.txt and masks.txt, the `grid` line of construction.txt;
/// data/ui/README.md). The evidence for each element's anchor is in hud.txt's comments and
/// docs/re/ui.md section 5.1.
enum class UiAnchor {
    /// No `anchor` field: the rectangle stays where it was measured.
    TopLeft,
    /// x moves with the layer's right edge (x + ui_w - reference width), y stays.
    Right,
    /// y moves with the layer's bottom edge (y + ui_h - reference height), x stays.
    Bottom,
    /// Both.
    RightBottom,
    /// x is where the status bar's span ends (UiPlacement::statusbar_end), y moves with the bottom
    /// edge: the question mark that closes the status bar.
    AfterStatusbar,
};

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
    /// `align centre` (UI-INFO-1): a text box. The text image (assets::render_text's canvas, as
    /// wide as its glyph advances) is drawn at x = pos_x + size_w / 2 - text_w / 2 (both
    /// divisions integer, rounding down), y = pos_y (docs/re/ui.md section 2.5). Empty for every
    /// sprite element.
    std::string align;
    /// UI-WINDOW-1: how `pos` moves in a larger layer (UiAnchor).
    UiAnchor anchor = UiAnchor::TopLeft;
    /// UI-WINDOW-1: the layer width this status-bar graphic is the original's choice for
    /// (AnnoFrame.dll 0x10063125..0x10063153 picks the bar by display width); absent for every
    /// other element.
    std::optional<int> for_width;
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
    /// UI-WINDOW-1: how the grid moves in a larger layer (it lies in the right chrome).
    UiAnchor anchor = UiAnchor::TopLeft;
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
    /// UI-WINDOW-1: how the rectangle moves in a larger layer (the live digits sit in the status
    /// bar, `anchor bottom`).
    UiAnchor anchor = UiAnchor::TopLeft;
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

/// UI-WINDOW-1: where the 1024 x 768 reference layout lands in a UI layer of `ui_w` x `ui_h`
/// pixels (at least the reference; ui_transform.hpp). Built by `place_hud` from hud.txt alone.
struct UiPlacement {
    int ref_w = 0, ref_h = 0;
    int ui_w = 0, ui_h = 0;
    /// ui_w - ref_w and ui_h - ref_h: how far a right- and a bottom-anchored rectangle move.
    int dx = 0, dy = 0;
    /// The status bar's span [0, statusbar_end) and the question mark's x. At a width some
    /// status-bar graphic is the original's choice for (`for_width`) it is that graphic's width
    /// (746 at 1024); otherwise ui_w - panel width - question-mark width (the 1152 and 1280
    /// graphics' rule, 873 = 1152 - 245 - 34, 1001 = 1280 - 245 - 34; the History Edition's
    /// question mark at 2542 x 1351 sits at 2263 = 2542 - 279, sprite-sad 0.0).
    int statusbar_end = 0;

    /// Where `x`, `y` (measured at the reference) lands under `anchor`.
    [[nodiscard]] std::pair<int, int> place(UiAnchor anchor, int x, int y) const;
};

/// The placement of `hud` in a `ui_w` x `ui_h` layer. A layer smaller than the reference in
/// either axis is treated as the reference (the transform never makes one).
[[nodiscard]] UiPlacement place_hud(const HudLayout& hud, int ui_w, int ui_h);

/// What a UI-layer pixel is (UI-WINDOW-1, the click side).
struct UiHit {
    /// On drawn chrome: a click here never reaches the map.
    bool chrome = false;
    /// The same point in the 1024 x 768 reference frame, when it lies on a rectangle that has
    /// one (the right chrome's top `ref_h` rows, the status bar's measured part): what
    /// ConstructionMenu::click and the tab hit test take. nullopt on the parts a larger layer adds
    /// (the panel column below the reference, the status bar's added span).
    std::optional<std::pair<int, int>> ref;
};

/// Classifies UI-layer pixel (`ux`, `uy`): the right chrome column (hud.txt `panel`'s x and
/// width, the layer's full height - the panel, then the filler below it), the status bar row
/// (`statusbar`'s y and height, x from 0 to statusbar_end plus `fragezeichen`'s width) and, with
/// `paused`, the pause parchment (`pause_parchment`) are chrome. Everything else is the map.
[[nodiscard]] UiHit ui_hit(const HudLayout& hud, const UiPlacement& placement, int ux, int uy,
                           bool paused = false);

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
