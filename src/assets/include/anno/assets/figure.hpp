// Figure frames (Figuren*.dat) as 8-bit RGBA images, and player colours.
//
// Mapping (spec: docs/formats/figuren.md): a covered pixel of a sprite frame
// with palette index i becomes [R, G, B, 255] from the entry's colour table
// Frame::palette; a covered pixel of a BSH_ALPHA shadow frame becomes
// [0, 0, 0, 255] (a coverage mask); uncovered pixels become [0, 0, 0, 0].
// The 2-bit level of A formats becomes the alpha (kLevelAlpha); the z byte
// (a depth offset) is not applied.
//
// Player colours: docs/render/iso.md, "Player colours".
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <anno/assets/texture.hpp>
#include <anno/formats/bytes.hpp>
#include <anno/formats/figuren.hpp>

namespace anno::assets::figure {

using Rgb = std::array<std::uint8_t, 3>;

/// Opacity of the 2-bit level of A formats: 25 %, 50 %, 75 %, opaque
/// (D: AnnoDisplay.dll 0x10018890, 0x100193f0, 0x100199a0;
/// docs/re/annodisplay.md §3).
inline constexpr std::array<std::uint8_t, 4> kLevelAlpha = {64, 128, 191, 255};

/// Alpha of a decoded pixel of `format`.
std::uint8_t pixel_alpha(formats::figuren::FrameFormat format, std::uint8_t level);

/// The eight player colours of the original, as RGB (D: AnnoDisplay.dll
/// 0x1003be70, eight 0x00RRGGBB words; colour variant 0 there means "no
/// recolouring"). Defaults only: callers pass their own table, so more
/// players or other colours are configuration.
inline constexpr std::array<Rgb, 8> kOriginalPlayerColours = {{
    {0, 187, 255},
    {255, 68, 0},
    {255, 244, 10},
    {0, 255, 176},
    {255, 138, 0},
    {47, 254, 41},
    {248, 57, 255},
    {255, 244, 222},
}};

/// Figure types (BEWOHNTYPE) whose colour tables get player-colour variants:
/// type numbers 97–111, 150 and 53 of the game's type-name table
/// (D: AnnoGame.dll figure colour loader 0x10011570, test at 0x100115b7;
/// names at 0x10024cb8; number 112 has no name). 69 of the 368 figure
/// entries; ships (SHIP, 113) are not among them.
inline constexpr std::array<std::string_view, 17> kPlayerColouredTypes = {
    "ERKUNDER",  "TRAGTIER",    "SCHWERT",  "BOGEN",   "ARMBRUST",  "LANZE",
    "MUSKETIER", "KAVALARIE",   "GEWEHR",   "MEDICUS", "BEDIENUNG", "KANONIER",
    "MOERSER",   "BELAGERTURM", "KATAPULT", "FLAGGE",  "ESEL",
};

/// Whether `entry` is drawn in its owner's colour (kPlayerColouredTypes).
bool player_coloured(const formats::figuren::Entry& entry);

/// A palette colour in the variant for `player` (D: AnnoDisplay.dll
/// 0x10007e60 and 0x10008980, identical tests). A colour whose green
/// dominates, G² > trunc(1.5 · (R² + B²)), becomes the player colour scaled
/// by its brightness: v = trunc(0.3333·R + 0.3333·G + 0.3333·B) (summed term
/// by term, left to right), w = min(trunc(3 · v), 255), each channel
/// player · w / 255 (integer division). Other colours stay. 0.3333 is the
/// original's f32 constant; the products and sums of 8-bit values with it
/// are exact in double, so this reproduces the original bit for bit.
Rgb recolour(Rgb rgb, Rgb player);

/// Decodes `frame` of `entry`; `volume` is the concatenated volume data of
/// the frame's zoom group.
Result<Rgba, Error> to_rgba(const formats::figuren::Entry& entry,
                            const formats::figuren::Frame& frame, formats::Bytes volume);

/// Decodes `frame` of `entry` from its pixel data alone (frame.length bytes
/// from frame.offset of the group's volume), with the palette colours in
/// `player`'s variant (recolour); nullopt draws the palette as it is.
Result<Rgba, Error> decode(const formats::figuren::Entry& entry,
                           const formats::figuren::Frame& frame, formats::Bytes data,
                           std::optional<Rgb> player = std::nullopt);

/// The frame's bytes in `volume`, or nullopt if they lie outside it.
std::optional<formats::Bytes> frame_data(const formats::figuren::Frame& frame,
                                         formats::Bytes volume);

/// RGBA of decoded pixels: uncovered [0, 0, 0, 0]; covered without a
/// palette (shadows) [0, 0, 0, 255]; else the palette colour (recoloured
/// for `player`) with pixel_alpha. Shared by figures and buildings.
std::vector<std::uint8_t> pixels_to_rgba(const formats::figuren::Pixels& px,
                                         formats::figuren::FrameFormat format,
                                         const formats::tex::Palette* palette,
                                         std::optional<Rgb> player);

} // namespace anno::assets::figure
