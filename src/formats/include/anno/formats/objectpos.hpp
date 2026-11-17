// The OBJECTPOS record of an Anlagen.dat entry: a list of sub-records, each a 6-byte name, a u16
// size and `size` data bytes (FIRE, SCHUSS, AUSPOS, HIMAP, WPLANE, CONPO2, CONPOS in the GOG
// file). CONPO2 holds the type's entrance arrows, 3 bytes each: x and y in thirds of a tile from
// the footprint corner, then the footprint side (AnnoGame.dll loader 0x10009e3e..0x10009edf:
// count = size / 3, records at type +0x22c; docs/formats/anlagen.md "OBJECTPOS",
// docs/re/ui.md 2.5). Every other sub-record is kept as raw bytes, so the record writes back
// byte for byte.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::objectpos {

/// The sub-record header: 6 name bytes (NUL-padded) and the u16 size.
inline constexpr std::size_t kHeaderLen = 8;
inline constexpr std::size_t kNameLen = 6;
/// One CONPO2 arrow record.
inline constexpr std::size_t kArrowLen = 3;

/// One CONPO2 record. `side` is the file's byte as is: 0..3 (-x, +y, +x, -y) in every type that
/// is built; types 261 and 1571 carry 215, which the original indexes past its side table with.
struct Arrow {
    std::uint8_t x3 = 0, y3 = 0, side = 0;
    bool operator==(const Arrow&) const = default;
};

struct SubRecord {
    std::array<std::uint8_t, kNameLen> name{};
    ByteBuffer data;
    /// The name up to the first NUL.
    [[nodiscard]] std::string name_str() const;
    bool operator==(const SubRecord&) const = default;
};

struct ObjectPos {
    std::vector<SubRecord> records;

    /// Splits `record` into its sub-records; an error when a header or a sub-record's data runs
    /// past the end, or when a CONPO2 size is not a multiple of 3.
    static Result<ObjectPos, std::string> parse(Bytes record);
    /// The record bytes, equal to the parsed input.
    [[nodiscard]] ByteBuffer to_bytes() const;
    /// The arrows of the CONPO2 sub-record, in file order; empty without one. With more than one
    /// (no type has two) the last wins, as the loader re-reads +0x22c for each. The old two-byte
    /// CONPOS form is not decoded (its loader 0x10009d96..0x10009e39 drops y and never writes the
    /// side); it stays in `records`.
    [[nodiscard]] std::vector<Arrow> conpo2() const;
};

} // namespace anno::formats::objectpos
