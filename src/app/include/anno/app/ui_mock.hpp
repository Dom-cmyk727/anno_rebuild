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
/// The panel's own cost row and name are deliberately NOT drawn: no RE evidence yet pins their
/// pixel position (OQ-561, the gold-cost icon's own exhaustive SAD search found none above mean
/// ~120 anywhere in the panel) - this draws only the chrome evidence already supports.
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
[[nodiscard]] std::string draw_live_screen_xrgb(
    std::uint32_t* dst, int dst_w, int dst_h, const HudLayout& hud,
    const ConstructionLayout& construction, const MaskLayout& masks, const UiMockSpec& screen,
    UiTexCache& cache, const std::vector<std::pair<sim::TypeId, bool>>* eligibility,
    const HudCounters& counters = {}, bool placement_open = false, bool demolish_mode = false);

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
                      bool demolish_mode = false);

} // namespace anno::app
