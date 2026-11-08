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
/// through it). `cache` persists the decoded `.tex` files across calls (ADR 0004 "Atlases": no
/// re-decode per frame) - the caller owns one instance for the whole session (constructed once,
/// with the real `game_dir`) and passes it to every call. An error names the screen, the missing
/// file, or the parse failure; nothing is drawn on one.
[[nodiscard]] std::string
draw_live_screen_xrgb(std::uint32_t* dst, int dst_w, int dst_h, const HudLayout& hud,
                      const ConstructionLayout& construction, const UiMockSpec& screen,
                      UiTexCache& cache,
                      const std::vector<std::pair<sim::TypeId, bool>>* eligibility);

} // namespace anno::app
