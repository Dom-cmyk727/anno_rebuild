// Island ground as a top-down RGBA image.
//
// Mapping (spec: docs/formats/insel.md, "Ground"): tile (tx, ty) covers
// scale × scale output pixels; output pixel (i, j) of the tile shows
// GroundCell::texel(i, j, scale) of the tile's Textures/Felder.tex texture
// through its palette. Open water (texture 0) becomes the water colour
// (kWater unless set); a texture id missing from Felder.tex becomes
// kMissing. This is a top-down inspection view, not the game's isometric
// projection, and HEIGHT is not applied.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <anno/assets/texture.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/tex.hpp>

namespace anno::assets::island {

using Colour = std::array<std::uint8_t, 4>;

/// Colour for open water (texture 0), which has no ground texture.
inline constexpr Colour kWater = {0, 0, 80, 255};
/// Colour for a texture id that Felder.tex does not contain.
inline constexpr Colour kMissing = {255, 0, 255, 255};

/// Decoded ground textures by id: RGB per pixel, row-major 256 × 256.
class GroundTextures {
  public:
    /// Decodes every 256 × 256 texture of Felder.tex that has its palette.
    static Result<GroundTextures, std::string> load(const formats::tex::TexFile& felder);

    /// Sets the colour drawn for open water (texture 0), e.g. the average
    /// colour of the sea sprite.
    void set_water(Colour rgba) { water_ = rgba; }
    /// The colour drawn for open water (texture 0).
    [[nodiscard]] Colour water() const { return water_; }
    /// Average colour of texture `id`; nullopt if Felder.tex lacks it.
    [[nodiscard]] std::optional<Colour> average(std::uint16_t id) const;
    /// Colour shown at tile position (s/n, t/n) of a tile with ground `cell`.
    [[nodiscard]] Colour cell_pixel(formats::insel::GroundCell cell, std::size_t s, std::size_t t,
                                    std::size_t n) const;

  private:
    std::map<std::uint32_t, std::vector<formats::tex::Rgb>> textures_;
    Colour water_ = kWater;
};

/// Renders the island's GFXID grid with `scale` pixels per tile, clamped to
/// 1–32 (a limit of this inspection view, as in the Rust tool, to keep images
/// small; not from the original). Fails if the image would exceed 65,535
/// pixels in either direction (Rgba holds u16 sizes; the Rust version panics).
Result<Rgba, std::string> ground_top_down(const formats::insel::Island& island,
                                          const GroundTextures& textures, std::uint16_t scale);

} // namespace anno::assets::island
