// Read-only facts about a mine's source (MAP-EXTRACT-1 X5, docs/sim/salt-extraction.md section 23):
// for a UI readout, a test or a tool. Nothing here changes the world; the functions take a const World.
#pragma once

#include <cstdint>
#include <optional>

#include <anno/sim/ids.hpp>

namespace anno::sim {

class World;

/// What a mine is doing, derived from fields the Mine already has (no state is added).
enum class MineState : std::uint8_t {
    /// No deposit is bound: an imported mine (the loader makes no deposits). A command-placed mine is
    /// never in this state (policy B refuses it).
    Unbound,
    /// Switched off by its owner.
    SwitchedOff,
    /// Bound and on, its output store full (Mine::blocked).
    Blocked,
    /// Bound, on, not blocked.
    Working
};

struct MineFacts {
    bool bound = false;
    /// Mine::deposit: the index into Island::deposits.
    std::optional<std::uint64_t> deposit_index;
    /// The tile of the ore object the deposit was made from (absolute world tile).
    std::int32_t ore_x = 0, ore_y = 0;
    /// Remaining amount in 8.8 tons (/ 256 = tons) and the reserve flag of the deposit.
    std::uint32_t remaining_88 = 0;
    bool reserve = false;
    /// Whether THIS mine uses the deposit up: deposit_extract takes from a flagged deposit only for a
    /// shallow mine in the depleting edition setting, and from an unflagged deposit whenever the
    /// setting depletes. A salt mine is deep, so this is false for it even in the add-on edition.
    bool depletes = false;
    /// Mine::deep.
    bool deep = false;
    /// The mine's output store now, 8.8 tons.
    std::uint32_t output_88 = 0;
    /// What the production steps made SINCE THE SINK WAS INSTALLED, 8.8 tons (not since the mine was built:
    /// a sink installed later, or a world loaded from a save, starts at 0), from the diagnostic ledger of the
    /// installed TransportStats sink; nullopt without a sink. NOT saved state: the Mine keeps no production total
    /// (adding one would change the 135 identity).
    std::optional<std::uint64_t> produced_88;
    MineState state = MineState::Unbound;
};

/// The facts of the mine `id`, or nullopt when `id` is not a building of the world or not a mine.
[[nodiscard]] std::optional<MineFacts> mine_facts(const World& world, BuildingId id);

} // namespace anno::sim
