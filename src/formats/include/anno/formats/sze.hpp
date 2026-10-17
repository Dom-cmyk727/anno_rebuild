// Scenarios (.sze): the SCENE chunk, its players (SPIELER), their ships
// (SCHIFF) and soldiers (SOLDAT), and island links (INSELLNK).
// Spec: docs/formats/sze.md; reference: tools/sze.py.
//
// - Root ANNO (base game) or ANNOADDON1 (add-on) holds one SCENE container.
// - SPIELER payload = u32 0, u32 index, then tagged records. Two series per
//   scenario: header blocks (with HEAD), then state blocks (SCHIFF, SOLDAT,
//   STADT2, ...).
// - SCHIFF/SOLDAT payload = 16-byte unit prefix, then tagged records.
#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/result.hpp>
#include <anno/formats/tagged.hpp>

namespace anno::formats::sze {

inline constexpr std::size_t kPlayerPrefixLen = 8;
inline constexpr std::size_t kUnitPrefixLen = 16;
inline constexpr std::size_t kPlayerHeadLen = 108;

/// Records whose payload is a fixed prefix of this many bytes followed by a
/// nested record sequence, keyed by (parent record, record); the top level
/// is "SPIELER". Table and counts: docs/formats/sze.md, "Nesting".
std::optional<std::size_t> nested_prefix(std::string_view parent, std::string_view name);

enum class Edition { Base, Addon };

/// An island placed by reference: the INSEL5 head, then the NUL-terminated
/// path of an INSEL4 *_dummy.scp template (replaced by a random real
/// template at game start, docs/re/annoscene.md §3–5).
struct IslandLink {
    std::uint16_t x = 0, y = 0, width = 0, height = 0;
    std::uint32_t number = 0;
    /// The path without its NUL (Windows-1252, `\` separators).
    Bytes path;
    static Result<IslandLink, std::string> parse(Bytes payload);
    [[nodiscard]] ByteBuffer to_bytes() const;
};

/// The known fields of a player's HEAD record (D: AnnoWelt.dll
/// 0x1001d2b9–0x1001d430, docs/re/annoscene.md §6).
struct PlayerHead {
    /// 8, 11 human; 12 computer; 14 native peoples; 15 pirates; 16 Venetians;
    /// 17 god/gang; 2 player 31.
    std::uint32_t kind = 0;
    std::uint32_t money = 0;
    /// Up to the first NUL of 48 bytes.
    Bytes name;
    std::array<std::uint8_t, 16> id{};
    /// Colour slot 1–8, 0xFF = assigned at game start, 0 = none.
    std::uint8_t colour = 0;

    static std::optional<PlayerHead> parse(Bytes raw);
    [[nodiscard]] bool is_human() const { return kind == 8 || kind == 11; }
    [[nodiscard]] bool is_computer() const { return kind == 12; }
};

/// A ship (SCHIFF) or soldier (SOLDAT): 16-byte prefix + records.
struct Unit {
    std::uint16_t id = 0;
    /// FigurenHeader.dat entry id.
    std::uint16_t figure = 0;
    /// Written but never read by the game; equals the holding block's index.
    std::uint8_t owner = 0;
    /// Starting facing: ships 16 steps, soldiers 8 (D: AnnoWelt.dll 0x1001e01d).
    std::uint8_t attr = 0;
    std::uint16_t reserved = 0;
    /// IEEE 754 bits of x and y (world tiles), kept raw for the round trip.
    std::uint32_t x_bits = 0, y_bits = 0;
    std::vector<tagged::Record> records;

    static Result<Unit, std::string> parse(Bytes payload, std::size_t offset = 0);
    [[nodiscard]] float x() const;
    [[nodiscard]] float y() const;
    [[nodiscard]] std::optional<ByteBuffer> to_bytes() const;
};

/// One SPIELER block.
struct Player {
    std::uint32_t flag = 0;
    std::uint32_t index = 0;
    std::vector<tagged::Record> records;

    static Result<Player, std::string> parse(Bytes payload, std::size_t offset = 0);
    [[nodiscard]] std::optional<ByteBuffer> to_bytes() const;
    /// The HEAD record (header series only).
    [[nodiscard]] std::optional<PlayerHead> head() const;
    /// The records called `name` (SCHIFF, SOLDAT) parsed as units.
    [[nodiscard]] Result<std::vector<Unit>, std::string> units(std::string_view name) const;
};

/// A parsed scenario: the children of its SCENE chunk.
struct Scene {
    Edition edition = Edition::Base;
    std::vector<chunk::Chunk> children;

    static Result<Scene, std::string> parse(Bytes buf);
    /// All SCENE children called `name`, in file order.
    [[nodiscard]] std::vector<const chunk::Chunk*> chunks(std::string_view name) const;
    [[nodiscard]] Result<std::vector<IslandLink>, std::string> island_links() const;
    /// All SPIELER blocks in file order (both series).
    [[nodiscard]] Result<std::vector<Player>, std::string> players() const;
};

} // namespace anno::formats::sze
