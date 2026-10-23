// The script timers, the monopoly table and the 7.5 minute event (docs/sim/state.md
// §5.10, stage S12; evidence: AnnoWelt.dll 0x1009cd30, 0x1009d2a0, 0x1009d210,
// 0x100ae140, read instruction by instruction for this stage).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <anno/sim/timers.hpp>

namespace anno::sim {

ScriptTimers make_script_timers(const SimConfig& cfg) {
    ScriptTimers t;
    t.active.assign(cfg.script_timers, 0);
    t.counter.assign(cfg.script_timers, 0);
    return t;
}

MonopolyTable make_monopoly_table(const SimConfig& cfg) {
    MonopolyTable m;
    m.entries.assign(cfg.monopoly_entries, MonopolyEntry{});
    return m;
}

bool timers_tick(ScriptTimers& timers, std::uint32_t t, bool clamp) {
    // D 0x1009cd39..0x1009cd3b: dt = t - [+4], u32.
    const std::uint32_t dt = t - timers.last_tick;
    bool expired = false;
    const std::size_t n = std::min(timers.active.size(), timers.counter.size());
    for (std::size_t k = 0; k < n; ++k) {
        if (timers.active[k] == 0) { // 0x1009cd41: a zero byte is a stopped timer
            continue;
        }
        if (k > 0) {
            timers.counter[k] += dt; // 0x1009cd4b, u32 wrap as the original's add
        } else if (clamp && dt >= timers.counter[0]) {
            // Rules::timer0_clamp (F5, OQ-133): an overshoot stops the timer at 0.
            timers.counter[0] = 0;
            timers.active[0] = 0;
            expired = true;
        } else {
            // 0x1009cd51: u32 `sub`, wrapping on an overshoot; 0x1009cd54..0x1009cd59:
            // the timer stops only when the result is exactly 0.
            timers.counter[0] -= dt;
            if (timers.counter[0] == 0) {
                timers.active[0] = 0;
                expired = true;
            }
        }
    }
    timers.last_tick = t; // 0x1009cd62
    return expired;
}

std::optional<PlayerId> monopoly_owner(const Island& island, const PlayerTable& players) {
    // D 0x1009d210: the island's cities in number order (0x1009d239..0x1009d27a); a city
    // qualifies when its owner's Volk is 1 (the player query 0x1c at 0x1009d25c, its
    // second dword compared with 1). A second qualifying city ends the search with
    // "none" (0x1009d266 / 0x1009d28b).
    std::optional<PlayerId> found;
    for (const std::optional<City>& city : island.cities) {
        if (!city.has_value()) {
            continue;
        }
        const Player* p = players.find(city->owner);
        if (p == nullptr || p->volk != 1) {
            continue;
        }
        if (found.has_value()) {
            return std::nullopt;
        }
        found = city->owner;
    }
    return found;
}

void monopoly_body(MonopolyTable& table, const std::vector<std::optional<Island>>& islands,
                   const PlayerTable& players, const SimConfig& cfg, std::uint32_t game_seconds) {
    // D 0x1009d2d4..0x1009d40a: s = 0 .. 19 (0x1009d3fd: s + 0x38 <= 0x4b), the island
    // scan inside (0x1009d2d9..0x1009d3e6).
    // monopoly_raw_slots <= min(monopoly_entries, 32) is validated (§3.1); the bounds
    // here keep an unvalidated table defined (s is a shift count into a u32 mask).
    const std::size_t slots =
        std::min<std::size_t>({cfg.monopoly_raw_slots, table.entries.size(), std::size_t{32}});
    for (std::size_t s = 0; s < slots; ++s) {
        MonopolyEntry& entry = table.entries[s];
        for (const std::optional<Island>& is : islands) {
            if (!is.has_value()) {
                continue; // 0x1009d314
            }
            if (is->climate >= cfg.monopoly_climate_mask.size()) {
                continue; // 0x1009d32b: a climate above the jump table is skipped
            }
            // 0x1009d338..0x1009d3aa: the climate's mask has bit s.
            if (((cfg.monopoly_climate_mask[is->climate] >> s) & 1U) == 0) {
                continue;
            }
            const std::optional<PlayerId> owner = monopoly_owner(*is, players);
            if (!owner.has_value()) {
                // 0x1009d3ee..0x1009d3f5: clear the entry and end the scan for s.
                entry = MonopolyEntry{};
                break;
            }
            if (entry.player != owner) {     // 0x1009d3bf
                entry.time_s = game_seconds; // 0x1009d3cc: world property 7
                entry.player = owner;
            }
        }
    }
}

bool monopoly_refresh(MonopolyTable& table, std::uint32_t dt_ms, const SimConfig& cfg,
                      const Rules& rules, const std::vector<std::optional<Island>>& islands,
                      const PlayerTable& players, std::uint32_t game_seconds) {
    if (rules.monopoly_every_30s) {
        // D5 option (OQ-040): a refresh every monopoly_ms of game time.
        table.acc += dt_ms;
        if (table.acc < cfg.monopoly_ms) {
            return false;
        }
        table.acc -= cfg.monopoly_ms;
    } else {
        // D 0x1009d2a9..0x1009d2bc: acc != 0 and acc < 30000 (`jb`) returns; the add
        // (0x1009d2cb..0x1009d2ce) is inside the body's path only.
        if (table.acc != 0 && table.acc < cfg.monopoly_ms) {
            return false;
        }
        table.acc += dt_ms;
    }
    monopoly_body(table, islands, players, cfg, game_seconds);
    return true;
}

std::optional<IslandId> event750(std::uint32_t& acc, std::uint32_t dt_ms, const SimConfig& cfg,
                                 const Rules& rules, const WorldSettings& settings,
                                 const std::vector<std::optional<Island>>& islands,
                                 DrawSource& draw) {
    acc += dt_ms; // D 0x100ae14d..0x100ae15c, u32
    if (acc < cfg.event750_ms) {
        return std::nullopt; // 0x100ae157 `cmp 0x6ddd0`, `jb`
    }
    // 0x100ae16e: acc = 0, before the property test (D10 option keeps the overshoot).
    acc = rules.event750_keep_overshoot ? acc - cfg.event750_ms : 0U;
    if (settings.event750_off()) {
        return std::nullopt; // 0x100ae17b: property 6 bit 0x80, no draw
    }
    const std::uint32_t r1 = draw.next(DrawSite::Event750Gate); // 0x100ae187
    if (r1 % cfg.event750_modulus != 0) {
        return std::nullopt; // 0x100ae194 `div 10`
    }
    if (!settings.addon()) {
        return std::nullopt; // 0x100ae1a7: property 14 bit 2; the draw was consumed
    }
    const std::uint32_t r2 = draw.next(DrawSite::Event750Island); // 0x100ae1b3
    if (islands.empty()) {
        return std::nullopt; // F6: 0x100ae1bb divides by the table length
    }
    const auto slot = static_cast<std::size_t>(r2 % islands.size());
    if (!islands[slot].has_value()) {
        return std::nullopt; // 0x100ae1c8
    }
    return IslandId{static_cast<std::uint32_t>(slot)};
}

std::string validate_timers(const ScriptTimers& timers, const SimConfig& cfg) {
    if (timers.active.size() != cfg.script_timers) {
        return "timers.active: has " + std::to_string(timers.active.size()) +
               " entries, the configuration says " + std::to_string(cfg.script_timers);
    }
    if (timers.counter.size() != cfg.script_timers) {
        return "timers.counter: has " + std::to_string(timers.counter.size()) +
               " entries, the configuration says " + std::to_string(cfg.script_timers);
    }
    return {};
}

std::string validate_monopoly(const MonopolyTable& table, const SimConfig& cfg) {
    if (table.entries.size() != cfg.monopoly_entries) {
        return "monopoly.entries: has " + std::to_string(table.entries.size()) +
               " entries, the configuration says " + std::to_string(cfg.monopoly_entries);
    }
    for (std::size_t i = 0; i < table.entries.size(); ++i) {
        const std::optional<PlayerId>& p = table.entries[i].player;
        if (p.has_value() && p->v >= cfg.player_slots) {
            return "monopoly.entries[" + std::to_string(i) + "].player: " + std::to_string(p->v) +
                   " is not a slot of the player table (" + std::to_string(cfg.player_slots) +
                   " slots)";
        }
    }
    return {};
}

} // namespace anno::sim
