// Ships and soldiers of a scenario as decoded figure frames, ready to draw.
//
// Rules (spec: docs/formats/sze.md, "Units"; docs/render/iso.md, "Scene
// composition", "Ships", "Soldiers", "Player colours"): a SCHIFF or SOLDAT
// unit names its figure type (`figure`, a FigurenHeader.dat entry) and its
// world position. Each unit is drawn with sprite set 0 of its entry (ships:
// sails furled; soldiers: standing, 8 frames), frame `attr mod n` of each
// zoom group, and the same frame of the set's SHADOW if it has one. `attr`
// is the starting facing (ships 16 steps, soldiers 8; D:
// docs/re/fire-and-ships.md §3.3); that frame k of a set shows facing k at
// the default view rotation is an assumption (the facing → frame mapping is
// not traced). Figures of the player-coloured types
// (figure::kPlayerColouredTypes) are decoded in their owner's colour
// (figure::recolour). A unit belongs to the SPIELER block that holds it
// (the unit's own owner byte is never read, D: docs/re/annoscene.md §7);
// the players' colours are assigned as at the start of a new single-player
// game (start_colours).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <anno/assets/figure.hpp>
#include <anno/assets/texture.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/sze.hpp>

namespace anno::assets::unit {

/// A frame header and its decoded image (shared by equal units).
struct DecodedFrame {
    formats::figuren::Frame frame;
    std::shared_ptr<const Rgba> image;
};

/// Which SPIELER record a unit comes from.
enum class UnitKind {
    /// SCHIFF, drawn on the sea surface.
    Ship,
    /// SOLDAT, drawn on the ground.
    Soldier,
};

/// The record name of `kind` (SCHIFF, SOLDAT).
std::string_view record_name(UnitKind kind);

/// One unit: world position in tiles and, per zoom group (index = group),
/// its frame and shadow frame (nullopt where the entry has none or decoding
/// failed).
struct UnitFrames {
    UnitKind kind = UnitKind::Ship;
    std::uint16_t figure = 0;
    float x = 0, y = 0;
    std::vector<std::optional<DecodedFrame>> frames;
    std::vector<std::optional<DecodedFrame>> shadows;
};

/// Counts of units().
struct UnitStats {
    std::size_t units = 0;
    /// Units whose figure entry or sprite set is missing.
    std::size_t missing = 0;
    /// Frames that could not be read or decoded.
    std::size_t failed_frames = 0;
    bool operator==(const UnitStats&) const = default;
};

/// Player colours at the start of a new single-player game: colour slot
/// (1–8) per player index, and the human slots the game removes.
struct StartColours {
    std::map<std::uint32_t, std::uint8_t> slots;
    /// Human slots without a colour other than the first: deleted with their
    /// objects at game start.
    std::set<std::uint32_t> removed;
    bool operator==(const StartColours&) const = default;
};

/// The colour assignment of a new single-player game (D: AnnoFrame.dll game
/// start 0x10029100 and colour screen 0x1001d180, docs/re/annoscene.md §9):
/// a HEAD colour 1–8 is kept; the first human slot (types 8, 11; never
/// player 31) gets the colour the player picks on the colour screen — here,
/// as a stand-in, the lowest colour of the pool — which leaves the pool;
/// every other player still at 0xFF is a human slot (removed), a computer
/// (type 12: the lowest colour left in the pool) or another type (no
/// colour). Preset computer colours stay in the pool, as in the original, so
/// two players can share a colour. `colours` is the number of colour slots
/// (8 in the original). HEAD stores the slot in one byte and 0xFF means
/// "assign at game start", so at most 254 slots are possible; larger values
/// are treated as 254.
Result<StartColours, std::string> start_colours(const formats::sze::Scene& scene,
                                                std::uint32_t colours = 8);

/// Reads `length` bytes from `offset` of zoom group `group`'s concatenated
/// figure volume (Figuren<group><nn>.dat), or nullopt.
using VolumeReader = std::function<std::optional<std::vector<std::uint8_t>>(
    std::uint32_t group, std::uint64_t offset, std::uint64_t length)>;

struct Units {
    std::vector<UnitFrames> units;
    UnitStats stats;
};

/// The units of `kinds` of all players of `scene`, in file order, with
/// player colours from `palette` (e.g. figure::kOriginalPlayerColours, slot
/// n = palette[n - 1]) assigned by start_colours with palette.size() slots
/// (with fewer than the original's 8 colours, a preset colour beyond the
/// palette is ignored: the first human then gets the lowest pool colour, a
/// computer none; the Rust version kept the slot and drew the units
/// uncoloured); units of removed human slots are left out.
Result<Units, std::string> units(const formats::sze::Scene& scene,
                                 const formats::figuren::FigurenHeader& header,
                                 std::span<const UnitKind> kinds,
                                 std::span<const figure::Rgb> palette, const VolumeReader& read);

} // namespace anno::assets::unit
