// Colour tables in Anlagen.dat: ANNO/COLORS/ENTRY (440 entries).
// Spec: docs/formats/palette.md ("Anlagen.dat/COLORS"); reference: tools/tex.py colors.
//
//   0x000 u32 flag (table kind, OQ-022 a), 0x004 u32 id,
//   0x008 tagged record BODY (name BODY\0\0, u16 1024), 0x010 1024-byte palette,
//   0x410 tagged record NAME (u16 n), 0x418 n-byte name ending in its only NUL.
#pragma once

#include <string>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>
#include <anno/formats/tex.hpp>

namespace anno::formats::colors {

/// Offset of the palette in an ENTRY payload.
inline constexpr std::size_t kPaletteOffset = 0x10;
/// Offset of the name string in an ENTRY payload.
inline constexpr std::size_t kNameOffset = 0x418;

struct ColorTable {
    tex::Palette palette;
    /// Name including the terminating NUL, raw (Windows-1252).
    Bytes name;
    /// The ENTRY payload rebuilt from the fields (byte-identical).
    [[nodiscard]] ByteBuffer to_bytes() const;
};

/// Parses one COLORS/ENTRY payload; the error names the failed check.
Result<ColorTable, std::string> parse_entry(Bytes payload);

/// All colour tables of an Anlagen.dat buffer, in file order.
Result<std::vector<ColorTable>, std::string> parse_anlagen(Bytes buf);

} // namespace anno::formats::colors
