// UI.2 R1 "Rendering": the CPU reference composition of a HUD/construction screen, built the same
// way the UI.1 oracle (`tools/ui_layout.py compose_screen`) does, so the shipped CPU path can be
// checked against that oracle pixel by pixel (CLAUDE.md's GPU-vs-CPU-vs-oracle rule). This mirrors
// `compose_screen`'s element order and overrides exactly (docs/re/ui.md section 2.8); it is not an
// independent design, by intent - a divergence from the oracle here is a bug to fix, not a second
// opinion to reconcile.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include <anno/app/ui_layout.hpp>
#include <anno/assets/texture.hpp>
#include <anno/formats/result.hpp>

namespace anno::app {

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

} // namespace anno::app
