// The game a seat starts (docs/plan/lan-endless.md W1, T36). Integer and enum
// fields only. Not part of World and not in the state stream: the handshake
// and the save take it later (W4).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/archive.hpp>

namespace anno::sim {

/// A seat at the table. `team` is unused in v1.
enum class SeatKind : std::uint8_t { Human, Computer };

struct Seat {
    SeatKind kind = SeatKind::Human;
    std::string name;
    /// Index into the colour table.
    std::uint32_t colour = 0;
    std::uint32_t team = 0;
};

/// Island size and quantity are the new setup's three steps. They are not the
/// two words the original's mission pages use (docs/formats/endless-presets.md).
enum class IslandSize : std::uint8_t { Small, Medium, Large };
enum class IslandQuantity : std::uint8_t { Less, Default, Many };

/// R15. Together is the original's one start point.
enum class StartMode : std::uint8_t { Together, Spread };

/// The three pirate words on the base mission pages. Not COMP TYP (OQ-222).
/// `None` is the value-initialised enumerator, not a decided menu default.
enum class PirateLevel : std::uint8_t { None, NotVeryAggressive, Aggressive };

/// The four money words on the base mission pages. A word is not one human
/// HEAD+0x04 amount (OQ-221): lots is 50000 or 45000, little is 25000 or
/// 20000, sufficient is 30000, more than enough is 500000. `Little` is the
/// value-initialised enumerator, not a decided menu default.
enum class StartMoney : std::uint8_t { Little, Sufficient, Lots, MoreThanEnough };

/// D11. `Enhanced` is the default and the value-initialised enumerator.
enum class RulesPreset : std::uint8_t { Enhanced, Original };

/// `Small`, `Less` and `Together` are the value-initialised enumerators so a
/// default-constructed setup is inside the ranges `validate` accepts. They are
/// not a decided menu default. `base_edge_4p` is the specified 4-player edge
/// (W2 / D1).
struct WorldOptions {
    std::uint32_t base_edge_4p = 1280;
    IslandSize size = IslandSize::Small;
    IslandQuantity quantity = IslandQuantity::Less;
    StartMode start = StartMode::Together;
};

struct GameSetup {
    std::vector<Seat> seats;
    WorldOptions world{};
    PirateLevel pirates = PirateLevel::None;
    StartMoney money = StartMoney::Little;
    std::uint64_t seed = 0;
    RulesPreset rules = RulesPreset::Enhanced;
};

/// Bounds `validate` applies. `max_seats` is this release's default maximum
/// (D11), not a hard cap: raise the field. `max_name_bytes` is the setup's own
/// limit (the original player-name field is 48 bytes, sze.md); raise the field.
struct SetupLimits {
    std::uint32_t max_seats = 6;
    std::uint32_t max_name_bytes = 32;
};

template <> struct EnumInfo<SeatKind> : ContiguousEnum<2> {};
template <> struct EnumInfo<IslandSize> : ContiguousEnum<3> {};
template <> struct EnumInfo<IslandQuantity> : ContiguousEnum<3> {};
template <> struct EnumInfo<StartMode> : ContiguousEnum<2> {};
template <> struct EnumInfo<PirateLevel> : ContiguousEnum<3> {};
template <> struct EnumInfo<StartMoney> : ContiguousEnum<4> {};
template <> struct EnumInfo<RulesPreset> : ContiguousEnum<2> {};

/// Rejects no seats, more seats than `limits.max_seats`, a duplicate colour,
/// a colour `>= colour_count`, a name longer than `limits.max_name_bytes`, and
/// an enumerator outside its range. The first failure is the error string.
[[nodiscard]] formats::Result<std::monostate, std::string>
validate(const GameSetup& setup, const SetupLimits& limits, std::uint32_t colour_count);

/// FNV-1a 64 of `fields` in raw mode. Not the world state hash.
[[nodiscard]] std::uint64_t digest(const GameSetup& setup);

template <class A, class S>
    requires Same<S, Seat>
void fields(A& a, S& s) {
    a.enumv(s.kind);
    a.str(s.name);
    a.u32(s.colour);
    a.u32(s.team);
}

template <class A, class S>
    requires Same<S, WorldOptions>
void fields(A& a, S& w) {
    a.u32(w.base_edge_4p);
    a.enumv(w.size);
    a.enumv(w.quantity);
    a.enumv(w.start);
}

template <class A, class S>
    requires Same<S, GameSetup>
void fields(A& a, S& s) {
    // Kind byte, empty-string length, colour and team. A shorter seat is not written.
    ar::seq(a, s.seats, std::uint64_t{17}, [&](auto& seat) { fields(a, seat); });
    fields(a, s.world);
    a.enumv(s.pirates);
    a.enumv(s.money);
    a.u64(s.seed);
    a.enumv(s.rules);
}

template <class A, class S>
    requires Same<S, SetupLimits>
void fields(A& a, S& limits) {
    a.u32(limits.max_seats);
    a.u32(limits.max_name_bytes);
}

} // namespace anno::sim
