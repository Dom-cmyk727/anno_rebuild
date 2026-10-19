// The read-only views and contexts of one advance (spec: docs/sim/state.md
// §4.11). Stage S4 creates the file with the players view only; TickContext,
// IslandsView, Effects, SerialWorld and SimOutput follow with stage S7.
#pragma once

#include <cstdint>

#include <anno/sim/ids.hpp>
#include <anno/sim/player.hpp>

namespace anno::sim {

/// The players as a tick sees them: read-only, and deliberately without a money
/// accessor (money changes only through Effects::money, merged by the world, so
/// that no island tick can read a balance that a serial phase is about to
/// change). A default-constructed view is empty: it holds no player. Every
/// query is total: an invalid id, a slot out of range or an empty slot is a
/// player that is not there, never an error.
class PlayersView {
  public:
    PlayersView() = default;
    explicit PlayersView(const PlayerTable& table) : table_(&table) {}

    [[nodiscard]] bool present(PlayerId id) const { return find(id) != nullptr; }
    /// The player's Volk (D: +0x7a); 0 for a player that is not there.
    [[nodiscard]] std::uint32_t volk(PlayerId id) const {
        const Player* p = find(id);
        return p != nullptr ? p->volk : 0;
    }
    /// Whether research `research_id` is done (bit 1 of its state byte); false
    /// for a player that is not there and for an id beyond the table.
    [[nodiscard]] bool researched(PlayerId id, std::uint32_t research_id) const {
        const Player* p = find(id);
        return p != nullptr && p->research.researched(research_id);
    }
    /// O-9: whether the player's build cost is waived; false when not there.
    [[nodiscard]] bool free_build(PlayerId id) const {
        const Player* p = find(id);
        return p != nullptr && p->free_build;
    }
    /// The player's state; Defeated for a player that is not there (an empty
    /// slot takes no part in the game, like a defeated one). Ask present()
    /// first where the difference matters.
    [[nodiscard]] PlayerState state(PlayerId id) const {
        const Player* p = find(id);
        return p != nullptr ? p->state : PlayerState::Defeated;
    }

  private:
    [[nodiscard]] const Player* find(PlayerId id) const {
        return table_ != nullptr ? table_->find(id) : nullptr;
    }
    const PlayerTable* table_ = nullptr;
};

} // namespace anno::sim
