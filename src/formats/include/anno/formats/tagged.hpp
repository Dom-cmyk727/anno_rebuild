// Tagged records: the small record container inside scenario players
// (SCENE/SPIELER), SCENE/COMPPROFIL2 and Anlagen.dat COLORS/ENTRY.
// Spec: docs/formats/sze.md ("Tagged records"); reference: tools/sze.py.
//
//   0x00 6 bytes name    [A-Z][A-Z0-9_]{0,5}, NUL-padded with zeros
//   0x06 u16     length  payload length, or >= 0xFF00 = long form
//   0x08 length  payload
//
// Long form: the low byte of `length` is bits 16–23 of the real length, the
// u16 at 0x08 bits 0–15, the payload starts at 0x0A (D: AnnoWelt.dll
// 0x100b4f50, a 24-bit length escape).
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::tagged {

inline constexpr std::size_t kNameLen = 6;
inline constexpr std::size_t kHeaderLen = kNameLen + 2;
inline constexpr std::size_t kLongHeaderLen = kHeaderLen + 2;
/// Smallest length value that announces the long form.
inline constexpr std::uint16_t kLongMarker = 0xFF00;
/// Largest long-form payload (24-bit length).
inline constexpr std::uint32_t kMaxLongLength = 0xFFFFFF;

struct Record {
    /// The name without padding (validated ASCII).
    std::string_view name;
    /// Offset of the header: relative to the parsed sequence plus `base`.
    std::size_t offset = 0;
    /// Whether the header uses the long form.
    bool long_form = false;
    Bytes payload;
    [[nodiscard]] std::size_t encoded_len() const {
        return (long_form ? kLongHeaderLen : kHeaderLen) + payload.size();
    }
};

/// Parses `data` as an exact record sequence; `base` is added to offsets.
Result<std::vector<Record>, std::string> parse_records(Bytes data, std::size_t base = 0);

/// Appends one record; false if `name` is invalid or the payload does not
/// fit the chosen form.
bool write_record(std::string_view name, bool long_form, Bytes payload, ByteBuffer& out);

/// Re-encodes a parsed sequence.
bool write_records(const std::vector<Record>& records, ByteBuffer& out);

/// The first record called `name`, or nullptr.
const Record* find(const std::vector<Record>& records, std::string_view name);

} // namespace anno::formats::tagged
