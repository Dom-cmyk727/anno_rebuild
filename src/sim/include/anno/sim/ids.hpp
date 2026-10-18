// Identity of simulated things (spec: docs/sim/state.md §2.1, §4.1).
//
// The original names things by table slot (island, city, player) or by
// list + slot (building; I: inferred from the list code, not read from the
// building: the constructor at AnnoWelt.dll 0x100734c0 never writes +0x04,
// docs/sim/state.md F2, §2.1). The rebuild keeps those slot numbers where the
// original rules depend on them (the city number is the owner value written
// to tiles, the player slot is the player table index) and adds ids that
// survive slot reuse (building, figure, event sequence), assigned only in the
// serial merge phase so they do not depend on how island ticks interleave.
// Widths are wider than the original's (u8 slots, u16 list slots): the
// original's limits are data, not types (PLAN §0.1).
#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace anno::sim {

/// A strongly typed id. The default value is invalid and never aliases slot
/// 0, so an uninitialised id cannot silently mean "the first one".
template <class Tag, class Rep = std::uint32_t> struct Id {
    using rep = Rep;
    Rep v = std::numeric_limits<Rep>::max();
    constexpr Id() = default;
    constexpr explicit Id(Rep x) : v(x) {}
    [[nodiscard]] constexpr bool valid() const { return v != std::numeric_limits<Rep>::max(); }
    friend constexpr auto operator<=>(const Id&, const Id&) = default;
};

struct PlayerTag {};
struct IslandTag {};
struct CityTag {};
struct TypeTag {};
struct BuildingTag {};
struct FigureTag {};

/// Slot in the player table (original u8; 31 is the neutral player by config).
using PlayerId = Id<PlayerTag>;
/// Slot in the island table (original u8).
using IslandId = Id<IslandTag>;
/// City number on its island = the owner value written to tiles.
using CityNo = Id<CityTag>;
/// Anlagen.dat ANLAGEN id.
using TypeId = Id<TypeTag>;
/// `(island << 32) + serial`, the serial counted per island, from 1, never reused.
using BuildingId = Id<BuildingTag, std::uint64_t>;
/// World counter, never reused (the original names figures by registry slot, u16).
using FigureId = Id<FigureTag, std::uint64_t>;

/// Index of the AnnoGame goods tables (0..0x8e in the original).
using GoodsId = std::uint32_t;
/// Index into a ListLayout (docs/sim/state.md §3).
using ListId = std::uint32_t;
/// Slot in a building list (the original's list slots are u16, I).
using Slot = std::uint32_t;
/// "No slot": an own sentinel (N), the maximum of the slot type.
inline constexpr Slot kNoSlot = 0xffffffffU;

/// The id of the building with `serial` on `island`.
constexpr BuildingId make_building_id(IslandId island, std::uint32_t serial) {
    return BuildingId{(std::uint64_t{island.v} << 32) + serial};
}
/// The island a building id was made on.
constexpr IslandId island_of(BuildingId id) {
    return IslandId{static_cast<std::uint32_t>(id.v >> 32)};
}
/// The per-island serial of a building id.
constexpr std::uint32_t serial_of(BuildingId id) {
    return static_cast<std::uint32_t>(id.v & 0xffffffffU);
}

struct CityRef {
    IslandId island;
    CityNo city;
    friend constexpr auto operator<=>(const CityRef&, const CityRef&) = default;
};
/// Where a building sits: city, list, slot. The order identity of the
/// original (a building has no other id there).
struct BuildingKey {
    CityRef city;
    ListId list = 0;
    Slot slot = kNoSlot;
    friend constexpr auto operator<=>(const BuildingKey&, const BuildingKey&) = default;
};
struct TileXY {
    std::int32_t x = 0, y = 0;
    friend constexpr auto operator<=>(const TileXY&, const TileXY&) = default;
};
struct TileRect {
    std::int32_t x0 = 0, y0 = 0;
    std::uint32_t w = 0, h = 0;
    friend constexpr auto operator<=>(const TileRect&, const TileRect&) = default;
};

/// World counters for ids assigned in the serial merge phase (hashed).
struct IdCounters {
    std::uint64_t next_figure = 1, next_event_seq = 1;
};

} // namespace anno::sim
