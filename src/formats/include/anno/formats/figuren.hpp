// Figures and sprites: FigurenHeader.dat (index) and Figuren<g><nn>.dat
// (pixel volumes); the ENTRY layout is shared with Anlagen.dat ANLAGEN.
// Spec: docs/formats/figuren.md, docs/formats/anlagen.md; reference: tools/figuren.py.
//
// - FigurenHeader.dat = root header ANNO (its size also counts the volume
//   files), one FIGUREN chunk with ENTRY chunks, then 24-byte volume
//   headers (TEXTURES, u32 size, u32 group, u32 frame count).
// - ENTRY = 8-byte prefix (u32 flag, u32 id) + records: VERSION2, HEAD,
//   BEWOHNTYPE, LOGIC, PATH, COLORS, HAEUSER, ...
// - COLORS = ENTRY chunks with the Anlagen.dat colour-table layout, or (in
//   ANLAGEN) a list of u32 ids of the global tables.
// - HAEUSER = ENTRY chunks (sprite sets): 16-byte prefix + BODY2, ANIM,
//   SOUND2, GFXDATA, TEXTURES × 3 (zoom groups), optional SHADOW (8-byte
//   prefix + the same records).
// - TEXTURES = u32 group, u32 n, n × 36-byte Frame.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/colors.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::figuren {

inline constexpr std::size_t kFrameLen = 36;
inline constexpr std::size_t kVolumeHeaderLen = 24;
inline constexpr std::size_t kEntryPrefixLen = 8;
/// Sprite-set prefix (OQ-023).
inline constexpr std::size_t kGfxPrefixLen = 16;
/// SHADOW prefix, 03 00 00 00 00 00 00 00 in all 1,047.
inline constexpr std::size_t kShadowPrefixLen = 8;

/// Frame pixel encodings, named by Frame::format_raw.
enum class FrameFormat {
    RgbV,  ///< BSH_RGBV: 1 byte per pixel
    RgbH,  ///< BSH_RGBH: 1 byte per pixel (flat decal)
    RgbZ,  ///< BSH_RGBZ: [z, index]
    RgbAV, ///< BSH_RGBAV: 1 byte; count byte carries a 2-bit level
    RgbAZ, ///< BSH_RGBAZ: [z, index]; count byte carries a level
    Alpha, ///< BSH_ALPHA: shadow coverage, no pixel bytes
    RgbWZ, ///< BSH_RGBWZ (Anlagen.dat): [z, index], every second column
    RgbZM, ///< BSH_RGBZM (Anlagen.dat): [z, index]
    RgbAH, ///< BSH_RGBAH (Anlagen.dat): 1 byte; count byte carries a level
};

std::optional<FrameFormat> frame_format(Bytes raw);
/// Bytes stored per opaque pixel.
std::size_t bytes_per_pixel(FrameFormat f);
/// True if the run count byte is `level << 6 | count`.
bool has_level(FrameFormat f);

/// One 36-byte frame header from a TEXTURES record.
struct Frame {
    std::uint16_t width = 0, height = 0;
    /// Index into the entry's colour tables.
    std::uint16_t palette = 0;
    std::uint16_t unknown_06 = 0;
    std::array<std::uint8_t, 12> format_raw{};
    /// The frame pixel that lands on the object's projected point; signed
    /// (D: AnnoDisplay.dll 0x1000ac80).
    std::int16_t anchor_x = 0, anchor_y = 0;
    /// Byte range in the group's concatenated volume; length 0 = empty frame.
    std::uint32_t offset = 0, length = 0;
    std::uint32_t unknown_20 = 0;

    static std::optional<Frame> parse(Bytes raw);
    [[nodiscard]] std::array<std::uint8_t, kFrameLen> to_bytes() const;
    [[nodiscard]] std::optional<FrameFormat> format() const { return frame_format(format_raw); }
    /// The format name up to the first NUL.
    [[nodiscard]] std::string format_name() const;
};

/// A TEXTURES record: all frames of one zoom group.
struct FrameSet {
    /// 0 = largest, 1 = half, 2 = quarter size; names the volume files.
    std::uint32_t group = 0;
    std::vector<Frame> frames;
    static Result<FrameSet, std::string> parse(Bytes payload);
    [[nodiscard]] ByteBuffer to_bytes() const;
};

/// A sprite set (HAEUSER/ENTRY) or its SHADOW.
struct GfxSet {
    Bytes prefix;
    /// All records in file order.
    std::vector<chunk::Chunk> records;
    /// TEXTURES records in file order.
    std::vector<FrameSet> frame_sets;
    std::shared_ptr<const GfxSet> shadow;

    static Result<GfxSet, std::string> parse(Bytes payload, std::size_t prefix_len);
    [[nodiscard]] ByteBuffer to_bytes() const;
};

/// One FIGUREN/ENTRY (or Anlagen.dat ANLAGEN/ENTRY).
struct Entry {
    /// Absolute offset of the ENTRY chunk header (when known).
    std::size_t offset = 0;
    std::uint32_t flag = 0;
    std::uint32_t id = 0;
    std::vector<chunk::Chunk> records;
    /// COLORS as colour tables (FigurenHeader.dat).
    std::vector<colors::ColorTable> palettes;
    /// COLORS as a list of global colour-table ids (Anlagen.dat ANLAGEN).
    std::vector<std::uint32_t> palette_ids;
    std::vector<GfxSet> gfx;

    /// Parses one ENTRY chunk.
    static Result<Entry, std::string> parse(const chunk::Chunk& c);
    /// Payload of the first record called `name`.
    [[nodiscard]] std::optional<Bytes> record(std::string_view name) const;
    /// A string record up to its first NUL (Latin-1 bytes as chars).
    [[nodiscard]] std::optional<std::string> record_str(std::string_view name) const;
    /// The ENTRY payload rebuilt from the parsed fields.
    [[nodiscard]] ByteBuffer to_bytes() const;
};

/// One 24-byte volume header at the end of FigurenHeader.dat.
struct VolumeHeader {
    chunk::NameField name_field{};
    /// 8 + total size of the group's volume files.
    std::uint32_t size = 0;
    std::uint32_t group = 0;
    std::uint32_t frames = 0;
};

struct FigurenHeader {
    /// Raw 16-byte ANNO header (its size counts the volume files; kept).
    Bytes root_header;
    chunk::NameField figuren_name{};
    std::vector<Entry> entries;
    std::vector<VolumeHeader> volumes;

    static Result<FigurenHeader, std::string> parse(Bytes buf);
    [[nodiscard]] ByteBuffer to_bytes() const;
    [[nodiscard]] const Entry* entry(std::uint32_t id) const;
};

/// File name of volume part `part` of zoom group `group`, e.g. Figuren003.dat.
std::string volume_file_name(std::uint32_t group, std::uint32_t part);

/// A piece of a byte range in the volume parts: (part, start, end).
struct Piece {
    std::size_t part;
    std::uint64_t start, end;
};
/// Maps offset .. offset + length of a group's concatenated volume to pieces,
/// given the sizes of parts 0, 1, ...; nullopt if it runs past the last part.
std::optional<std::vector<Piece>> locate(const std::vector<std::uint64_t>& part_sizes,
                                         std::uint64_t offset, std::uint64_t length);

/// One decoded frame pixel; fields a format does not store are 0.
struct Pixel {
    std::uint8_t index = 0;
    /// First byte of a Z pixel pair: signed depth offset (docs/re/annodisplay.md §3).
    std::uint8_t z = 0;
    /// Top two bits of the run count in A formats: 25/50/75/100 % opacity.
    std::uint8_t level = 0;
    bool operator==(const Pixel&) const = default;
};

using Pixels = std::vector<std::optional<Pixel>>;

/// Decodes a frame to width × height pixels (nullopt = transparent). Row RLE
/// as .tex: `(skip, count, data)` runs, 0xFE ends a row, 0xFF the image; in
/// A formats the count byte is `level << 6 | count`; in BSH_RGBWZ stored
/// pixel x of row y lands on column 2x + (y & 1). Empty data = empty frame.
Result<Pixels, std::string> decode_frame(FrameFormat f, Bytes data, std::size_t width,
                                         std::size_t height);

/// As decode_frame, but pixels past the width are dropped; also returns the
/// number dropped.
Result<std::pair<Pixels, std::size_t>, std::string>
decode_frame_clipped(FrameFormat f, Bytes data, std::size_t width, std::size_t height);

/// As decode_frame, but the image is widened to the rightmost pixel any row
/// writes (the original draws pixels past the width, OQ-025); returns the
/// pixels and the image width.
Result<std::pair<Pixels, std::size_t>, std::string>
decode_frame_extended(FrameFormat f, Bytes data, std::size_t width, std::size_t height);

} // namespace anno::formats::figuren
