// .tex textures as 8-bit RGBA images, and the error type of anno_assets.
//
// Mapping (spec: docs/formats/tex.md, docs/formats/palette.md): an opaque
// pixel with palette index i becomes [R, G, B, 255] from the texture's
// palette; a transparent pixel (gap in BSH_RGB RLE) becomes [0, 0, 0, 0].
// HEAD.color_key is not applied: whether the engine treats that colour as
// transparent is open (OQ-021).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/formats/tex.hpp>

namespace anno::assets {

using formats::Result;

/// A decoded image, rows top to bottom, 4 bytes per pixel (R, G, B, A).
struct Rgba {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> pixels;
    bool operator==(const Rgba&) const = default;
};

/// Why a texture or frame could not be decoded.
struct Error {
    enum class Kind {
        /// The texture's HEAD.palette_id names no COLOR in the file (`id`).
        MissingTexturePalette,
        /// Frame::format_raw is not a known format.
        UnknownFormat,
        /// Frame::palette (`id`) names no colour table of the entry.
        MissingPalette,
        /// offset .. offset + length lies outside the group's volume data.
        OutsideVolume,
        /// The pixel data is malformed (`detail` names the problem).
        Decode,
    };
    Kind kind = Kind::Decode;
    std::uint16_t id = 0;
    std::string detail;

    [[nodiscard]] std::string message() const;
    bool operator==(const Error&) const = default;
};

/// Decodes `texture` with its palette from `file`.
Result<Rgba, Error> to_rgba(const formats::tex::TexFile& file,
                            const formats::tex::Texture& texture);

} // namespace anno::assets
