// Textures (.tex): palettes (COLOR) and 8-bit indexed images (TEXTURE).
// Spec: docs/formats/tex.md, docs/formats/palette.md; reference: tools/tex.py.
//
// - Root ANNO holds COLOR and TEXTURE chunks, or is empty (338 files).
// - Both start with an 8-byte prefix `u32 flag, u32 id`, then sub-chunks.
//   COLOR = BODY with 256 × [B, G, R, 0]. TEXTURE = HEAD (36 bytes), BODY
//   (pixels), optional NAME (source file name, NUL-terminated).
// - Pixel formats: BSH_RGB / BSH_RGBH = row RLE, TEX_RGB = raw w × h bytes.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::tex {

/// Entries in a palette.
inline constexpr std::size_t kPaletteLen = 256;
/// Size of a TEXTURE/HEAD payload.
inline constexpr std::size_t kHeadLen = 36;
/// RLE marker: end of the current row.
inline constexpr std::uint8_t kRleEndOfRow = 0xFE;
/// RLE marker: end of the image (also ends the last row).
inline constexpr std::uint8_t kRleEndOfImage = 0xFF;
/// Largest skip + count of one RLE run (P: corpus maximum).
inline constexpr std::size_t kRleMaxRun = 253;

using Rgb = std::array<std::uint8_t, 3>;

/// A 256-colour palette; `entries` is the raw 1,024-byte table, 4 bytes per
/// entry in file order [B, G, R, 0] (Windows RGBQUAD).
struct Palette {
    std::uint32_t id = 0;
    /// The u32 before the id (0, or 2 for 10 palettes in fenster(8).tex; OQ-021).
    std::uint32_t flag = 0;
    Bytes entries;
    /// Entry `index` as [R, G, B].
    [[nodiscard]] Rgb rgb(std::uint8_t index) const {
        const std::size_t o = std::size_t{index} * 4;
        return {entries[o + 2], entries[o + 1], entries[o]};
    }
};

enum class PixelFormat { BshRgb, BshRgbH, TexRgb };

/// The 36-byte TEXTURE/HEAD payload (offsets in docs/formats/tex.md).
struct Head {
    std::uint16_t width = 0, height = 0, palette_id = 0, unknown_06 = 0, unknown_08 = 0;
    std::array<std::uint8_t, 10> format_raw{};
    std::array<std::uint8_t, 4> color_key{};
    std::uint16_t unknown_18 = 0, unknown_1a = 0;
    std::array<std::uint8_t, 8> unknown_1c{};

    static std::optional<Head> parse(Bytes raw);
    [[nodiscard]] std::array<std::uint8_t, kHeadLen> to_bytes() const;
    [[nodiscard]] std::optional<PixelFormat> format() const;
};

/// Decoded pixels: palette indices, nullopt = transparent.
using Pixels = std::vector<std::optional<std::uint8_t>>;

struct Texture {
    std::uint32_t id = 0;
    std::uint32_t flag = 0;
    Head head;
    /// Encoded pixel data (BODY payload).
    Bytes body;
    /// NAME payload, raw (Windows-1252, NUL-terminated), if present.
    std::optional<Bytes> name;

    /// Decodes to width × height indices; the error text names the problem.
    [[nodiscard]] Result<Pixels, std::string> decode() const;
};

struct TexFile {
    /// Entries in file order; each is a palette or a texture.
    std::vector<Palette> palettes;
    std::vector<Texture> textures;
    /// File order of the entries: true = palette (index into palettes).
    std::vector<std::pair<bool, std::size_t>> order;

    static Result<TexFile, std::string> parse(Bytes buf);
    [[nodiscard]] const Palette* palette(std::uint32_t id) const;
};

/// Decodes BSH_RGB row RLE: per row `(skip, count, count × index)` runs;
/// 0xFE ends a row, 0xFF the last row and the image.
Result<Pixels, std::string> decode_rle(Bytes data, std::size_t width, std::size_t height);

/// Encodes pixels as BSH_RGB row RLE, byte-identical to the original encoder
/// on all 5,029 RLE textures (rules in docs/formats/tex.md).
ByteBuffer encode_rle(const Pixels& px, std::size_t width, std::size_t height);

} // namespace anno::formats::tex
