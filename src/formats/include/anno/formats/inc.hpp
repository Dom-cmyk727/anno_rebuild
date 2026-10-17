// Texture index files (.inc): root ANNO with one ACCESS chunk holding
// NUL-terminated "<source>,<number>" records. Spec: docs/formats/tex.md
// ("ACCESS"); reference: tools/tex.py inc.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <anno/formats/bytes.hpp>

namespace anno::formats::inc {

struct AccessEntry {
    /// Source file name, raw (Windows-1252).
    Bytes source;
    std::uint32_t number = 0;
};

/// The ACCESS records of a whole .inc file; nullopt if the file does not
/// have that layout (exactly one ACCESS chunk, no trailing bytes).
std::optional<std::vector<AccessEntry>> parse_access(Bytes buf);

/// An ACCESS payload: concatenated NUL-terminated records.
std::optional<std::vector<AccessEntry>> parse_records(Bytes payload);

} // namespace anno::formats::inc
