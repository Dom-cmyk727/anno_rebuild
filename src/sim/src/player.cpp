#include <limits>

#include <anno/sim/player.hpp>
#include <anno/sim/scheduler.hpp>

namespace anno::sim {

Player make_player(PlayerId index, const SimConfig& cfg) {
    Player p;
    p.index = index;
    p.research.state.assign(cfg.research_entries, 0);
    p.research.progress.assign(cfg.research_entries, 0);
    return p;
}

PlayerTable make_player_table(const SimConfig& cfg) {
    PlayerTable t;
    t.slots.resize(cfg.player_slots);
    return t;
}

std::string validate_players(const PlayerTable& table, const SimConfig& cfg) {
    if (table.slots.size() != cfg.player_slots) {
        return "players.slots: has " + std::to_string(table.slots.size()) +
               " entries, SimConfig.player_slots is " + std::to_string(cfg.player_slots);
    }
    if (table.cursor >= table.slots.size()) {
        return "players.cursor: " + std::to_string(table.cursor) + " is not below the slot count " +
               std::to_string(table.slots.size());
    }
    for (std::size_t i = 0; i < table.slots.size(); ++i) {
        if (!table.slots[i].has_value()) {
            continue;
        }
        const Player& p = *table.slots[i];
        const std::string where = "players.slots[" + std::to_string(i) + "]";
        if (p.index.v != i) {
            return where + ".index: is " + std::to_string(p.index.v) + ", not the slot";
        }
        if (p.research.state.size() != cfg.research_entries) {
            return where + ".research.state: has " + std::to_string(p.research.state.size()) +
                   " entries, SimConfig.research_entries is " +
                   std::to_string(cfg.research_entries);
        }
        if (p.research.progress.size() != cfg.research_entries) {
            return where + ".research.progress: has " + std::to_string(p.research.progress.size()) +
                   " entries, SimConfig.research_entries is " +
                   std::to_string(cfg.research_entries);
        }
        // The remainder of S / upkeep_slices is below upkeep_slices in every state
        // play reaches (a validated configuration has upkeep_slices >= 1).
        if (p.upkeep_rem >= cfg.upkeep_slices) {
            return where + ".upkeep_rem: " + std::to_string(p.upkeep_rem) +
                   " is not below SimConfig.upkeep_slices " + std::to_string(cfg.upkeep_slices);
        }
    }
    return {};
}

namespace {

/// stat * num / den: in u32 like the original's register (the product wraps), or
/// in 64 bits capped at 0xFFFFFFFF with Rules::stat_decay_wide (D21, OQ-057).
/// The caller has checked that den is not zero.
std::uint32_t decay(std::uint32_t stat, const SimConfig& cfg, const Rules& rules) {
    if (rules.stat_decay_wide) {
        const std::uint64_t wide =
            static_cast<std::uint64_t>(stat) * cfg.stat_decay_num / cfg.stat_decay_den;
        return wide > 0xFFFFFFFFULL ? 0xFFFFFFFFU : static_cast<std::uint32_t>(wide);
    }
    return stat * cfg.stat_decay_num / cfg.stat_decay_den;
}

/// Whether the grace period is over: t > deadline as unsigned 32-bit numbers (D:
/// 0x1001b94d, jbe), or with Rules::bankrupt_deadline_wrap_safe as the sign of the
/// 32-bit difference (D22, OQ-057).
bool deadline_passed(std::uint32_t t, std::uint32_t deadline, const Rules& rules) {
    if (rules.bankrupt_deadline_wrap_safe) {
        const std::uint32_t diff = t - deadline;
        return diff != 0 && diff < 0x80000000U;
    }
    return t > deadline;
}

/// Step 2 of the player tick (D: 0x1001b840).
void book_upkeep(Player& p, std::uint32_t t, const SimConfig& cfg, const Rules& rules) {
    if (static_cast<std::uint16_t>(t - p.upkeep_stamp) < cfg.upkeep_period) {
        return;
    }
    // The sum is a u32 and wraps, as the original's register does.
    const std::uint32_t sum = p.upkeep_a + p.upkeep_b + p.upkeep_rem;
    if (cfg.upkeep_slices != 0) {
        const std::int64_t charge = static_cast<std::int64_t>(sum / cfg.upkeep_slices);
        // Saturating: a loaded balance near the lowest i64 must not overflow (F28).
        const std::int64_t lowest = std::numeric_limits<std::int64_t>::min();
        p.money = p.money < lowest + charge ? lowest : p.money - charge;
        p.upkeep_rem = sum % cfg.upkeep_slices;
    }
    // The stamp becomes t, it is not advanced by the period.
    p.upkeep_stamp = static_cast<std::uint16_t>(t);
    if (cfg.stat_decay_den != 0) {
        p.stat_a = decay(p.stat_a, cfg, rules);
        p.stat_b = decay(p.stat_b, cfg, rules);
    }
}

} // namespace

void player_tick(Player& p, std::uint32_t t, const SimConfig& cfg, const Rules& rules,
                 const PlayerHooks& hooks) {
    if (hooks.groups != nullptr) {
        hooks.groups(hooks.groups_ctx, p);
    }
    book_upkeep(p, t, cfg, rules);
    if (p.index.v == cfg.neutral_player || p.state != PlayerState::Alive) {
        return;
    }
    if (p.money <= cfg.bankrupt_money) {
        if (p.bankrupt_deadline == 0) {
            p.bankrupt_deadline = t + cfg.bankrupt_grace_ticks;
            if (rules.bankrupt_deadline_wrap_safe && p.bankrupt_deadline == 0) {
                // 0 means "not running": a sum of exactly 2^32 must not look like that.
                p.bankrupt_deadline = 1;
            }
        } else if (deadline_passed(t, p.bankrupt_deadline, rules)) {
            p.state = PlayerState::Bankrupt;
        }
    } else {
        p.bankrupt_deadline = 0;
    }
    if (p.defeat_pending) {
        p.defeat_pending = false;
        if (hooks.defeat != nullptr) {
            hooks.defeat(hooks.defeat_ctx, p);
        }
    }
}

void player_table_tick(PlayerTable& table, std::uint32_t t, const SimConfig& cfg,
                       const Rules& rules, const PlayerHooks& hooks) {
    const std::uint64_t n = table.slots.size();
    if (n != 0 && table.cursor < n) {
        const std::uint64_t count =
            rules.player_table_full_tick ? n : visit_count(t, table.stamp, n, cfg.player_div);
        std::uint64_t cursor = table.cursor;
        for (std::uint64_t i = 0; i < count; ++i) {
            std::optional<Player>& slot = table.slots[static_cast<std::size_t>(cursor)];
            if (slot.has_value()) {
                player_tick(*slot, t, cfg, rules, hooks);
            }
            if (++cursor >= n) {
                cursor = 0;
            }
        }
        table.cursor = static_cast<std::uint32_t>(cursor);
    }
    // The AI tick of the original runs here, before the stamp (Seams::ai, O-1).
    if (hooks.ai != nullptr) {
        hooks.ai(hooks.ai_ctx);
    }
    table.stamp = static_cast<std::uint16_t>(t);
}

} // namespace anno::sim
