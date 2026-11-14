// UI.2 R1 "Rendering": the CPU reference composition of a HUD/construction screen, built the same
// way the UI.1 oracle (`tools/ui_layout.py compose_screen`) does, so the shipped CPU path can be
// checked against that oracle pixel by pixel (CLAUDE.md's GPU-vs-CPU-vs-oracle rule). This mirrors
// `compose_screen`'s element order and overrides exactly (docs/re/ui.md section 2.8); it is not an
// independent design, by intent - a divergence from the oracle here is a bug to fix, not a second
// opinion to reconcile.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/app/ui_layout.hpp>
#include <anno/assets/texture.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/ids.hpp>

namespace anno::app {

/// Caches every `.tex` file `draw_live_screen_xrgb` has decoded so far, across frames (ADR 0004
/// "Atlases": "textures are uploaded once... no upload per frame" - the first H1/C1 drawing slice
/// did not meet this, re-reading and re-decoding from disk on every single frame; this is the
/// fix). The window owns one for the whole session (constructed once alongside the construction
/// menu) and passes it to every `draw_live_screen_xrgb` call; `compose_screen` and the standalone
/// `anno_ui_mock` tool keep their own short-lived cache (one mock is one process, so there is
/// nothing to persist across). Opaque (pimpl): the `.tex`/`TexFile` decode details stay in
/// ui_mock.cpp, not this header.
class UiTexCache {
  public:
    explicit UiTexCache(std::filesystem::path game_dir);
    ~UiTexCache();
    UiTexCache(UiTexCache&&) noexcept;
    UiTexCache& operator=(UiTexCache&&) noexcept;
    UiTexCache(const UiTexCache&) = delete;
    UiTexCache& operator=(const UiTexCache&) = delete;

    struct Impl;
    /// For ui_mock.cpp's own use (`draw_live_screen_xrgb`'s implementation) - not part of this
    /// class's public contract, but `Impl` stays opaque to every other translation unit, so there
    /// is nothing for outside code to do with it.
    Impl& impl() { return *impl_; }

  private:
    std::unique_ptr<Impl> impl_;
};

/// Which chrome the construction tab frames show: `kHud` draws the idle HUD's frames
/// (construction 8, military 13 selected); `kConstruction` draws a construction-menu category's
/// frames (construction 12 selected, military 9) - `compose_screen`'s `spec["kind"]` branch.
enum class UiMockScreen { Hud, Construction };

/// One of the UI.1 oracle's nine named screens (`tools/ui_layout.py SCREENS`): `hud_idle`, or
/// `construction_<category>` for each of the eight categories.
struct UiMockSpec {
    UiMockScreen kind = UiMockScreen::Hud;
    /// Empty for `kHud`; one of hud.txt/construction.txt's eight category names otherwise.
    std::string category;
};

/// Parses `screen` into a UiMockSpec (e.g. "hud_idle" -> {Hud, ""}, "construction_public" ->
/// {Construction, "public"}); nullopt for any other name.
[[nodiscard]] std::optional<UiMockSpec> parse_screen_name(const std::string& screen);

/// The live numbers `draw_live_screen_xrgb` draws over the HUD/construction chrome (H2), already
/// in their displayed units: `gold` a plain count (`PlaySnapshot::money`), `tools`/`wood`/
/// `bricks`/`marble` whole tons (the city's 8.8-fixed-point stock, shifted down by `kTon`'s 8
/// bits - production.hpp). This keeps `sim::TypeRegistry`/goods-identity resolution (by name,
/// "WERKZEUG"/"HOLZ"/"ZIEGEL"/"MAMOR") in the caller (Viewer), which already owns the registry
/// through its developer tool; ui_mock.cpp only formats and draws. nullopt draws nothing for
/// that counter (no city yet, matching `PlaySnapshot::eligibility`'s own null convention).
/// `tools`/`wood`/`bricks`/`marble` are drawn only on a construction screen - OQ-538 (resolved):
/// the idle HUD bar never shows them, only the gold counter. `residents` (the city's total
/// population, `PlaySnapshot::population`/`World::city_population`) is drawn only on a
/// construction screen too - docs/re/ui.md 2.1's own note on `STATUSBAR_EINWOHNER`: "absent from
/// pass-2 idle", same visibility as the materials.
struct HudCounters {
    std::optional<std::int64_t> gold;
    std::optional<std::uint32_t> tools, wood, bricks, marble;
    std::optional<std::int64_t> residents;
};

/// UI-INFO-1: what the building info panel shows for the armed type, already in displayed units
/// (docs/re/ui.md section 2.5). The original's panel update, View.dll 0x10030140, asks the type
/// for each figure and prints it with `_itoa` radix 10 (0x1008d919): the four build goods
/// (WERKZEUG 0x29, HOLZ 0x2a, ZIEGEL 0x2b, MAMOR 0x2c, in that gadget order) as whole tons
/// (`(amount + bias) >> 8`, 0x100301e2), the gold cost, and the running cost's column 1 (the
/// active column; 0x1003026b `push 1` column 1, 0x1003026d `push 2` kind 2). The caller fills this
/// from the live type registry (ConstructionMenu::info_panel_content), never from
/// data/ui/construction.txt.
struct InfoPanelContent {
    /// The Anlagen id: the name comes from Texte.dat `[ANLAGEN]` `<id>:<name>` and the preview
    /// from Anlagen.dat's sprite set of this id.
    sim::TypeId type;
    std::int64_t gold = 0;
    std::int64_t tools = 0, wood = 0, bricks = 0, marble = 0;
    /// nullopt: no source (ConstructionCellInfo::upkeep's own contract) - the upkeep box draws
    /// nothing then, never a guessed 0.
    std::optional<std::int64_t> upkeep;
};

/// The x at which a centred text box draws a text image `text_w` pixels wide: `box_x +
/// box_w / 2 - text_w / 2`, both divisions rounding down (docs/re/ui.md section 2.5; measured
/// exactly on 15 GOG shots for 1-, 2- and 3-digit values and 15 names).
[[nodiscard]] int centred_text_x(int box_x, unsigned box_w, int text_w);

/// Composes `screen` at the layout's resolution into an RGBA canvas (alpha always 255; the oracle
/// it mirrors discards alpha on output). Reads `.tex` files from `game_dir` through the existing
/// `anno::formats::tex`/`anno::assets::to_rgba` decoders. An error names the screen, the missing
/// file, or the parse failure.
[[nodiscard]] formats::Result<assets::Rgba, std::string>
compose_screen(const HudLayout& hud, const ConstructionLayout& construction,
               const UiMockSpec& screen, const std::filesystem::path& game_dir);

/// Draws `screen` directly onto a live window-space XRGB buffer (SDL_PIXELFORMAT_XRGB8888: one
/// `std::uint32_t` 0x00RRGGBB per pixel, row-major, `dst_w` x `dst_h`, viewer.cpp's `buffer_`/
/// `blend_preview`'s own convention) instead of a fresh canvas - the live window's draw path
/// (W4-UI-2 H1/C1, second slice: the first slice, `compose_screen`, only ever wrote a standalone
/// PNG). `dst_w`/`dst_h` must equal `hud`'s own resolution (the caller already enforces the
/// exact-size guard before calling this; R3's scale is still open, so this never resamples).
/// Mirrors `compose_screen`'s element sequence exactly (docs/re/ui.md section 2.8), except: a
/// construction cell's drawn lock state reads `eligibility` (ascending by id, as
/// `World::types().types()` orders them; `PlaySnapshot::eligibility`'s own contract) when given,
/// never the cell's own static `unlocked` field (RE evidence about the original, not a live
/// rule) - so what the player sees always agrees with what a click there would do (C2's
/// authority). Opaque source pixels overwrite; transparent ones are skipped (the oracle's own
/// `blit_rgba` semantic - never alpha-blended, since HUD/menu chrome never shows the game world
/// through it). `cache` persists the decoded `.tex` files and fonts across calls (ADR 0004
/// "Atlases": no re-decode per frame) - the caller owns one instance for the whole session
/// (constructed once, with the real `game_dir`) and passes it to every call. `counters` draws the
/// live gold/goods numbers (H2; see HudCounters's own doc comment) - a counter whose value is
/// nullopt is skipped.
///
/// `placement_open` (C1/C6, the building info/placement panel): on a construction screen, the
/// grid and the info panel are never shown together (ConstructionMenu::click's own pickaxe/
/// cancel-X overlap note - the two panels occupy the same screen region, `data/ui/hud.txt`'s
/// `info_panel` rectangle (804, 302) 220 x 466 overlapping `construction.txt`'s own grid origin/
/// extent). `false` draws the grid and the pickaxe (`abrissicon`) as before; `true` draws the
/// info panel's chrome instead - its background (`info_panel`), close button (`infoexit`, the
/// same rectangle the pickaxe shares) and rotate arrows (`rotate_left`/`rotate_right`), every one
/// of them a real sprite-sad-measured position (`data/ui/hud.txt`, shot `r2_info_pioneer_house`).
/// The cost and upkeep icons are part of `INFOUNTERGRUND` itself (no separate sprite; OQ-561
/// closed). With `info` set (UI-INFO-1) the panel also draws the armed type's name, preview,
/// four material costs, gold cost and upkeep: the seven text boxes `info_name`, `info_tools`,
/// `info_wood`, `info_bricks`, `info_marble`, `info_gold`, `info_upkeep` (hud.txt, `align
/// centre`) in `MainSmall.zei` with the measured level colours, and the preview in the
/// `info_preview` box from `anlagen` (null: no preview). An element missing from hud.txt draws
/// nothing for it.
///
/// An error names the screen, the missing file, or the parse failure; nothing is drawn on one.
/// `masks` is data/ui/masks.txt (the H2 live-digit counters' own measured rectangles -
/// `gold_digits`/`baumat_{tools,wood,bricks,marble}_d`/`einwohner_digits`; a counter whose named
/// mask is missing from `masks` draws nothing for it, never a guessed position).
/// `demolish_mode` (ADR 0004 row 5, C5's own `ToggleDemolish`): swaps the pickaxe
/// (`abrissicon`) from its idle frame (tex_id 659) to its active one (660) - the only chrome
/// difference `docs/re/ui.md` records between the grid's idle and demolishing state (line 192:
/// "id 659... mean 6.7 on r2_construction_public; id 660... mean 6.3 on r2_demolish (active)").
/// Ignored while `placement_open` is true (the pickaxe is not drawn then at all - the info
/// panel's own close button, `infoexit`, shares its rectangle instead). Demolish mode's other
/// original chrome - the OS cursor swap to `tCursorAbriss`/`Abreissen_Gebaeude.tex` and the
/// yellow-green land highlight under it - is not drawn by this function; see OQ-566.
///
/// `paused` (ADR 0004 row 10): draws the parchment (`pause_parchment`) and its button
/// (`pause_button`) last, over everything else this function draws - "parchment over the map,
/// HUD chrome stays" (docs/re/ui.md line 643). The sim clock itself stops/resumes through the
/// existing path (`Viewer::toggle_pause`/`ViewerHooks::on_pause`); this flag only draws the
/// chrome for a state the caller already has.
///
/// `selected_house` (ADR 0004 row 18, W4-NEXT-2 K): draws the selected-house panel - its plate
/// (`pionier_bk`), portrait (`house_portrait`) and the one measured need icon (`need_icon_0`) -
/// only on the idle HUD (`screen.kind == Hud`; the construction menu and this panel never show
/// together, same split as `placement_open`). The caller sets this only when a building is
/// selected (`app::SelectionState::selected`) and the current snapshot's inspection of that same
/// building answers `BuildingInspection::kind == "House"` - never a guess ahead of the sim's own
/// answer. The row's other two need icons and its tab are not measured yet (OQ-569/OQ-570) and
/// are not drawn; there is no close button to draw (OQ-570: none exists in the capture).
[[nodiscard]] std::string draw_live_screen_xrgb(
    std::uint32_t* dst, int dst_w, int dst_h, const HudLayout& hud,
    const ConstructionLayout& construction, const MaskLayout& masks, const UiMockSpec& screen,
    UiTexCache& cache, const std::vector<std::pair<sim::TypeId, bool>>* eligibility,
    const HudCounters& counters = {}, bool placement_open = false, bool demolish_mode = false,
    bool paused = false, bool selected_house = false, const InfoPanelContent* info = nullptr,
    const formats::anlagen::AnlagenFile* anlagen = nullptr);

/// `draw_live_screen_xrgb`'s own state and arguments, composed instead into a fresh RGBA canvas
/// that starts fully transparent (every byte 0, not the oracle's own alpha-255 convention) - the
/// GPU path's CPU-side half: the caller uploads the result as one texture and blends it over the
/// GPU-rendered frame with a full-screen pass (`gpu.wgsl`'s `ui_overlay_fs`, not the sprite-atlas
/// path ADR 0004's "Rendering" section first guessed - `.tex`/`.zei` content and fixed
/// screen-space positions do not fit `render::SpriteDraw`'s world-tile-anchored contract).
/// Written as a separate function from `draw_live_screen_xrgb`, not shared code, but over the
/// same hud.txt/construction.txt/masks.txt element sequence and (since the earlier W4-UI-2 fix
/// this doc comment used to describe as independent) the same digit-rectangle lookups - the real
/// safety net against the two drifting apart is each path's own pixel comparison against the
/// UI.1 oracle, not independent implementation of the digit literals. `w`/`h` must equal `hud`'s
/// own resolution, same exact-size guard as the XRGB path (R3 is still open).
[[nodiscard]] formats::Result<assets::Rgba, std::string>
draw_live_screen_rgba(int w, int h, const HudLayout& hud, const ConstructionLayout& construction,
                      const MaskLayout& masks, const UiMockSpec& screen, UiTexCache& cache,
                      const std::vector<std::pair<sim::TypeId, bool>>* eligibility,
                      const HudCounters& counters = {}, bool placement_open = false,
                      bool demolish_mode = false, bool paused = false, bool selected_house = false,
                      const InfoPanelContent* info = nullptr,
                      const formats::anlagen::AnlagenFile* anlagen = nullptr);

/// `draw_live_screen_rgba` into a caller-owned `canvas`, whose pixel storage is reused between
/// calls (UI-WINDOW-1 fix round R1: the window's GPU path no longer allocates a window-sized image
/// every frame). Returns an empty string, or the error (`canvas` is then unspecified).
[[nodiscard]] std::string draw_live_screen_rgba_into(
    assets::Rgba& canvas, int w, int h, const HudLayout& hud,
    const ConstructionLayout& construction, const MaskLayout& masks, const UiMockSpec& screen,
    UiTexCache& cache, const std::vector<std::pair<sim::TypeId, bool>>* eligibility,
    const HudCounters& counters = {}, bool placement_open = false, bool demolish_mode = false,
    bool paused = false, bool selected_house = false, const InfoPanelContent* info = nullptr,
    const formats::anlagen::AnlagenFile* anlagen = nullptr);

} // namespace anno::app
