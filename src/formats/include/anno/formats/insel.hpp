// Islands (INSEL5, legacy INSEL4) in island templates (.scp) and scenarios
// (.sze). Spec: docs/formats/insel.md; reference: tools/insel.py.
//
// - INSEL5 payload = 12-byte head (u16 x, u16 y, u16 width, u16 height,
//   u32 number; position and number 0 in templates) + chunk records.
// - INSEL4 (21 *_DUMMY.scp): 8-byte payload (u16 width, u16 height, u32 0);
//   the records follow as siblings in the root.
// - HEIGHT and GFXID hold the terrain in 32 × 32 blocks: 6-byte header
//   (u16 x, u16 y, u8 width, u8 height) + width × height cells of 1
//   (HEIGHT) or 2 (GFXID) bytes, row-major; blocks tile the island exactly.
// - FRUCHTBAR: one byte per tile, row-major.
// - GFXID = texture << 8 | u << 5 | v << 2 | r (GroundCell).
// - GELAENDE, PROD2, FLUESSE blocks hold 6-byte items placing Anlagen.dat
//   objects (PlacedObject).
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::insel {

inline constexpr std::size_t kBlock = 32;
inline constexpr std::size_t kBlockHeaderLen = 6;
inline constexpr std::size_t kInsel5HeadLen = 12;
inline constexpr std::size_t kInsel4HeadLen = 8;
inline constexpr std::size_t kItemLen = 6;
/// Ground cells per texture edge (D: AnnoWelt.dll UV table 0x100DCF44).
inline constexpr std::size_t kCellsPerTexture = 6;
inline constexpr std::size_t kGroundTexturePx = 256;

enum class Version { Insel4, Insel5 };

/// One block record (HEIGHT, GFXID, GELAENDE, PROD2, FLUESSE).
struct Block {
    std::uint16_t x = 0, y = 0;
    std::uint8_t width = 0, height = 0;
    /// Everything after the 6-byte header.
    Bytes data;

    static Result<Block, std::string> parse(Bytes payload);
    [[nodiscard]] std::array<std::uint8_t, kBlockHeaderLen> header_bytes() const;
};

/// The ground a GFXID value selects (docs/formats/insel.md, "Ground").
struct GroundCell {
    /// Textures/Felder.tex texture id; 0 = open water.
    std::uint8_t texture = 0;
    std::uint8_t u = 0, v = 0;
    /// Quarter turns (0–3).
    std::uint8_t rotation = 0;

    static GroundCell from_gfx_id(std::uint16_t gfx);
    [[nodiscard]] std::uint16_t to_gfx_id() const;
    /// Texture pixel shown at tile position (s/n, t/n).
    [[nodiscard]] std::pair<std::size_t, std::size_t> texel(std::size_t s, std::size_t t,
                                                            std::size_t n) const;
};

/// One 6-byte block item: an Anlagen.dat object placed on the island.
struct PlacedObject {
    /// ANLAGEN entry id (bits 0–11 of the first u16).
    std::uint16_t anlage = 0;
    /// Quarter turns (bits 14–15); odd values swap the footprint.
    std::uint8_t rotation = 0;
    /// Bits 12–13; 0 in the whole corpus.
    std::uint8_t reserved = 0;
    /// height << 8 | phase.
    std::uint16_t state = 0;
    /// Island tile of the footprint's top-left corner.
    std::uint16_t x = 0, y = 0;

    static PlacedObject parse(Bytes item, const Block& block);
    /// Height in HEIGHT units (D: AnnoWelt.dll 0x1007aa40).
    [[nodiscard]] std::uint8_t height() const { return static_cast<std::uint8_t>(state >> 8); }
    /// Animation phase (low 6 bits of state).
    [[nodiscard]] std::uint8_t phase() const { return static_cast<std::uint8_t>(state & 0x3F); }
    /// (width, height) on the island for an unrotated footprint (w, h).
    [[nodiscard]] std::pair<std::uint8_t, std::uint8_t> footprint(std::uint8_t w,
                                                                  std::uint8_t h) const {
        return rotation % 2 == 1 ? std::pair{h, w} : std::pair{w, h};
    }
    auto operator<=>(const PlacedObject&) const = default;
};

struct Island {
    Version version = Version::Insel5;
    /// The 12-byte INSEL5 head or the 8-byte INSEL4 payload.
    Bytes head;
    std::uint16_t x = 0, y = 0, width = 0, height = 0;
    std::uint32_t number = 0;
    /// Records (INFO, HEIGHT, GFXID, ...) in file order.
    std::vector<chunk::Chunk> records;

    static Result<Island, std::string> from_insel5(Bytes payload);
    static Result<Island, std::string> from_insel4(const std::vector<chunk::Chunk>& root_children);

    /// All records called `name`, parsed as blocks, in file order.
    [[nodiscard]] Result<std::vector<Block>, std::string> blocks(std::string_view name) const;
    /// Payload of the first record called `name`.
    [[nodiscard]] std::optional<Bytes> record(std::string_view name) const;
    /// The HEIGHT grid, one byte per tile, row-major.
    [[nodiscard]] Result<std::vector<std::uint8_t>, std::string> heights() const;
    /// The GFXID grid, one u16 per tile, row-major.
    [[nodiscard]] Result<std::vector<std::uint16_t>, std::string> gfx_ids() const;
    /// FRUCHTBAR if present with width × height bytes.
    [[nodiscard]] std::optional<Bytes> fertility() const;
    /// All items of the block records called `name`, duplicates included.
    [[nodiscard]] Result<std::vector<PlacedObject>, std::string>
    objects(std::string_view name) const;
};

/// Splits a row-major grid of `cell`-byte values into block payloads in
/// file order (rows of 32 × 32 blocks), as written for HEIGHT and GFXID.
std::vector<ByteBuffer> encode_blocks(Bytes grid, std::size_t width, std::size_t height,
                                      std::size_t cell);

/// All islands of an .scp or .sze buffer in file order.
Result<std::vector<Island>, std::string> islands(Bytes buf);

} // namespace anno::formats::insel
