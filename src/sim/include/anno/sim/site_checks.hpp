// Isolated geometry and site predicates of the ship-founding placement (task MAP-2 U2;
// research: docs/re/ship-founding.md sections 8-12, contract: docs/sim/ship-founding.md
// P7, P8, P10, P11, section 13).
//
// These are pure functions of explicit inputs. None of them is "founding allowed": they
// do not look at the ship, the owner, the figure veto, the city ring, the diplomacy or
// the climate, and none of them changes state. Each answers one question the original
// answers inside its placement validator and says, per result, whether the answer is
// ORIGINAL (verified against the DLL or its emulation) or whether the input lies in a
// domain this helper does not decide ("Unsupported"): an unsupported answer is never a
// silent accept or reject.
//
// Limits: every count and distance is a parameter or a widened integer; nothing here
// caps a footprint or a distance (the original's byte-sized footprint is a file-layout
// fact of BuildingType, not of these functions).
#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <anno/sim/building.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim::site {

/// A tile coordinate in world space. 64-bit so that centring and bounds never wrap.
struct Origin {
    std::int64_t x = 0, y = 0;
    friend bool operator==(const Origin&, const Origin&) = default;
};

/// The origin the original derives from a click: the footprint centred on the click,
/// clamped at 0 per axis (ORIGINAL, re 8.x: origin = max(0, click - (d - 1) / 2) with
/// C truncating division). d is the rotation-adjusted footprint size. A d of 0 gives
/// the click itself ((0 - 1) / 2 truncates to 0).
[[nodiscard]] Origin centred_origin(std::int64_t click_x, std::int64_t click_y,
                                    std::uint32_t footprint_w, std::uint32_t footprint_h);

/// The rotations tried, in order: the cursor's rotation r, then r+1, r+2, r+3, each
/// modulo 4 (ORIGINAL). ADAPTATION: a rotation above 3 is reduced modulo 4 here; the
/// caller validates the command's rotation (contract P2) before using it.
[[nodiscard]] std::array<std::uint8_t, 4> rotation_trials(std::uint8_t first);

/// The first rotation of rotation_trials(first) for which `accepts(rotation)` is true,
/// nullopt when none is. The origin stays fixed across the trials (ORIGINAL).
template <class Accepts>
[[nodiscard]] std::optional<std::uint8_t> first_accepted_rotation(std::uint8_t first,
                                                                  Accepts&& accepts) {
    for (const std::uint8_t r : rotation_trials(first)) {
        if (accepts(r)) {
            return r;
        }
    }
    return std::nullopt;
}

/// Supported domain of the footprint helpers: any layer TileLayer::reset accepts (the far edge may
/// lie above INT32_MAX) and any i64 origin; tiles are addressed by local offset, never by a
/// narrowed 32-bit world coordinate (src/local_tiles.hpp). The footprint (origin, w, h) lies inside
/// the tile layer on both axes: lower bound (x >= x0, y >= y0) and upper bound (x + w <= x0 + W, y
/// + h <= y0 + H). ORIGINAL (contract P7). Run it for the first rotation as the initial rejection,
/// and for each rotation trial with that trial's own dimensions.
[[nodiscard]] bool footprint_inside(const TileLayer& tiles, Origin origin, std::uint32_t w,
                                    std::uint32_t h);

/// Distance rule parameters. The original's value is 12 tiles (ORIGINAL, re 8.3);
/// it is data here, not a constant of the rule.
struct DistanceRule {
    std::uint32_t max_distance = 12;
};

/// trunc(sqrt(dx^2 + dy^2)) <= max_distance, with dx = trunc_toward_zero(click_x -
/// ship_x) and dy likewise, computed in integers: the click in whole tiles, the ship in
/// Q24.8 (figure.hpp). Equivalent to the original's f32 computation only in the verified
/// domain of contract C2 (ship positions exactly representable in Q24.8 and f32, small
/// coordinates); outside it the original's behaviour at the boundary is not claimed.
/// Total: a click whose scaled value could overflow 64 bits is outside any distance
/// (false), as is |dx| or |dy| above max_distance (rejected before squaring).
[[nodiscard]] bool within_range(std::int64_t click_x, std::int64_t click_y, FixedPos ship,
                                const DistanceRule& rule = {});

/// Ownership of the footprint's tiles (contract P8).
enum class Ownership : std::uint8_t {
    AllNeutral,    ///< every tile is on the island and owned by the neutral owner
    OwnedTile,     ///< some tile is owned by a city (own, rival or other)
    OutsideIsland, ///< some tile lies outside the layer (checked first)
};
[[nodiscard]] Ownership footprint_ownership(const TileLayer& tiles, std::uint32_t neutral_owner,
                                            Origin origin, std::uint32_t w, std::uint32_t h);

/// How the placed type regards what already stands on the footprint.
enum class Compat : std::uint8_t {
    Compatible,   ///< ORIGINAL: the placement-compatibility slot (type +0x6c) returns 1
    Incompatible, ///< ORIGINAL: it returns 0
    Unsupported,  ///< footprint_occupancy: an object id the registry does not know, a tile outside
                  ///< the layer
};

/// Bit 8 of the type word +0x14 = HEAD byte 0x2e bit 0 (slot +0xd0, AnnoGame 0x10009432): the
/// existing type may be replaced by a placed type of the same PRODTYPE. Derived from
/// BuildingType::head_raw (not a registry field, so the registry digest is unchanged); a record
/// shorter than 0x2f bytes reads 0.
[[nodiscard]] bool type_replaceable(const BuildingType& type);

/// Type slot +0x6c (AnnoGame 0x10008920) for the calls that pass neither 0x10000000 nor 0x10 in
/// their flags (founding 0x8000000, the BuildCmd apply 0x1000000), docs/re/ship-placement.md
/// section 4. May `placed` stand on the object `existing`?
///  - existing flag word bit 0x20 (growing/raw objects, ruins, stones): Compatible;
///  - placed group 0x81: only PRODTYPE 0x81 over a group 0xc1 or 0x101 object;
///  - placed group 1, 0x21 or 0x61: the same PRODTYPE over a replaceable existing type
///    (type_replaceable); else, with the placed flag bit 0x10 clear, Incompatible; with it set,
///    Compatible over group 0x141 only;
///  - placed group 0xc1: over PRODTYPE 0xc6 only PRODTYPE 0xc6, else over group 0xc1;
///  - placed group 0x101: over group 0xc1 or 0x101; group 0x141: over group 0x141;
///  - any other group (0x41, 0xa1, 0, ...): Incompatible (the original's fall-through).
/// The group is ((prodtype - 1) & ~0x1f) + 1 in 32-bit unsigned arithmetic. Never Unsupported
/// (the enumerator is kept for footprint_occupancy: an object id the registry does not know).
[[nodiscard]] Compat overlap_with_existing(const BuildingType& placed,
                                           const BuildingType& existing);

/// Occupancy of the whole footprint: the worst result over its tiles (Incompatible
/// before Unsupported before Compatible). A tile with object id 0 is empty. An object
/// id unknown to the registry is Unsupported, as is any tile outside the layer.
[[nodiscard]] Compat footprint_occupancy(const TileLayer& tiles, const TypeRegistry& types,
                                         const BuildingType& placed, Origin origin, std::uint32_t w,
                                         std::uint32_t h);

// --- coast-class height rule (ORIGINAL/E, re 11.x / contract P11.4) -------------------

enum class TerrainResult : std::uint8_t {
    Accepted,    ///< result_byte holds the record byte 8 (max & 0xf0)
    Rejected,    ///< the rule returned -1
    Unsupported, ///< the type's flag word is not in the decoded class
    Unreadable,  ///< a vertex of the window could not be read (missing data, out of range)
};

struct TerrainVerdict {
    TerrainResult result = TerrainResult::Unsupported;
    std::uint8_t result_byte = 0;
    friend bool operator==(const TerrainVerdict&, const TerrainVerdict&) = default;
};

/// The decoded class: coast types with flag word bits (f & 0x7017) == 0x2007, which is
/// the shipped 0x2087 (1302, 1304, 1306 and 27 more types; mode (f & 3) == 3). Road
/// (0x4000), mountain (0x1000), plain and 0x3000 types are not founding types and are
/// not decoded.
[[nodiscard]] bool coast_class_supported(std::uint32_t flags_word);

/// Vertex-window height rule. The rotation must be 0..3 (contract P2, validated by the caller);
/// ADAPTATION: a larger value is reduced modulo 4 here, whereas the original compares the
/// byte for exactly 0..3. type_w and type_h are the type's UNROTATED width and
/// height; n = type_w + 1 and m = type_h + 1 (ORIGINAL: the edge lengths do not follow
/// the rotation). The window is (fw + 1) x (fh + 1) vertices from the origin, fw x fh
/// the rotated footprint. height_at(dx, dy) -> std::optional<std::uint8_t> reads the
/// vertex (origin + (dx, dy)); a nullopt makes the verdict Unreadable.
///  1. maximum over the window; if n <= 2 or m <= 2 the rule accepts without further
///     test (the shortcut for one-tile-wide or -deep types) with max & 0xf0;
///  2. otherwise max & 0xf0 must be 0x30, the shore edge (rotation 0 top row y = 0, 1
///     left column x = 0, 2 bottom row y = m - 1, 3 right column x = m - 1; n vertices)
///     must be exactly 0x30 everywhere, and at least n / 2 vertices of the opposite edge
///     must be below 0x20.
template <class HeightAt>
[[nodiscard]] TerrainVerdict coast_height_rule(std::uint32_t type_w, std::uint32_t type_h,
                                               std::uint8_t rotation, HeightAt&& height_at) {
    const bool odd = (rotation & 1U) != 0;
    const std::uint64_t fw = odd ? type_h : type_w;
    const std::uint64_t fh = odd ? type_w : type_h;
    std::uint8_t mx = 0;
    for (std::uint64_t y = 0; y <= fh; ++y) {
        for (std::uint64_t x = 0; x <= fw; ++x) {
            const std::optional<std::uint8_t> v = height_at(x, y);
            if (!v) {
                return {TerrainResult::Unreadable, 0};
            }
            mx = *v > mx ? *v : mx;
        }
    }
    const std::uint64_t n = std::uint64_t{type_w} + 1;
    const std::uint64_t m = std::uint64_t{type_h} + 1;
    const auto byte8 = static_cast<std::uint8_t>(mx & 0xf0U);
    if (n <= 2 || m <= 2) {
        return {TerrainResult::Accepted, byte8};
    }
    if (byte8 != 0x30) {
        return {TerrainResult::Rejected, 0};
    }
    // Edge vertex i of the shore (first) and opposite (second) edge.
    const auto shore = [&](std::uint64_t i) {
        switch (rotation & 3U) {
        case 0:
            return height_at(i, 0);
        case 1:
            return height_at(0, i);
        case 2:
            return height_at(i, m - 1);
        default:
            return height_at(m - 1, i);
        }
    };
    const auto opposite = [&](std::uint64_t i) {
        switch (rotation & 3U) {
        case 0:
            return height_at(i, m - 1);
        case 1:
            return height_at(m - 1, i);
        case 2:
            return height_at(i, 0);
        default:
            return height_at(0, i);
        }
    };
    for (std::uint64_t i = 0; i < n; ++i) {
        const std::optional<std::uint8_t> v = shore(i);
        if (!v) {
            return {TerrainResult::Unreadable, 0};
        }
        if (*v != 0x30) {
            return {TerrainResult::Rejected, 0};
        }
    }
    std::uint64_t low = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
        const std::optional<std::uint8_t> v = opposite(i);
        if (!v) {
            return {TerrainResult::Unreadable, 0};
        }
        low += *v < 0x20 ? 1U : 0U;
    }
    if (low < n / 2) {
        return {TerrainResult::Rejected, 0};
    }
    return {TerrainResult::Accepted, byte8};
}

/// The rule over an island's vertex grid: Unsupported when the type's class is not
/// decoded; Unreadable when the grid cannot answer (no terrain, placeholder, absent or
/// wrong-sized column, or a window outside x0..x0+W / y0..y0+H).
[[nodiscard]] TerrainVerdict coast_height_check(const VertexGrid& grid, const BuildingType& type,
                                                std::uint8_t rotation, Origin origin);

} // namespace anno::sim::site
