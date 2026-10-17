// Buildings and objects: Anlagen.dat. Spec: docs/formats/anlagen.md;
// reference: tools/figuren.py anlagen.
//
// - Root ANNO → COLORS (440 colour tables), ANLAGEN (921 ENTRY), three
//   TEXTURES pixel volumes (zoom groups 0, 1, 2).
// - ANLAGEN/ENTRY has the FigurenHeader.dat entry layout (figuren::Entry).
// - TEXTURES payload = u32 group, u32 frames, then the frame pixel data;
//   frame offsets count from the byte after `frames`.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/colors.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::anlagen {

inline constexpr std::size_t kVolumePrefixLen = 8;

/// One inline TEXTURES pixel volume.
struct Volume {
    std::uint32_t group = 0;
    std::uint32_t frames = 0;
    /// Pixel data; frame offsets index into it.
    Bytes data;
};

struct AnlagenFile {
    std::vector<colors::ColorTable> colors;
    /// The raw ANLAGEN/ENTRY chunks, for byte-exact writing.
    std::vector<chunk::Chunk> entry_chunks;
    std::vector<figuren::Entry> entries;
    std::vector<Volume> volumes;

    static Result<AnlagenFile, std::string> parse(Bytes buf);
    [[nodiscard]] const Volume* volume(std::uint32_t group) const;
    [[nodiscard]] const figuren::Entry* entry(std::uint32_t id) const;
    /// Footprint (width, height) in tiles: HEAD bytes 0 and 1.
    [[nodiscard]] std::optional<std::pair<std::uint8_t, std::uint8_t>>
    footprint(std::uint32_t id) const;
};

} // namespace anno::formats::anlagen
