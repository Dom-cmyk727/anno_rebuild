// World settings: the scenario's game properties (spec: docs/sim/state.md §3;
// evidence: AnnoWelt.dll world property getter 0x100a3380 and setter
// 0x100a3230, AnnoScene.dll scene HEAD reader 0x10009cc0).
//
// Properties 4 (tick) and 7 (seconds) derive from the clock, property 8 (PRNG
// cursor) is WorldRng::set_cursor; the rest live here and are hashed in the
// `settings` scope (state_fields.hpp).
#pragma once

#include <cstdint>
#include <string>

namespace anno::sim {

struct WorldSettings {
    /// HEAD[0..7] of the scene: world size in tiles.
    std::uint32_t world_width = 0, world_height = 0;
    /// 0 endless, 1 mission, 2 campaign in the corpus (I).
    std::uint8_t prop0 = 0;
    /// Never read by the sim.
    std::uint8_t prop1 = 1;
    /// Difficulty-like (I).
    std::uint8_t prop2 = 2;
    /// Time-limit-like, 2550 = none (I). The original stores value/10 in a
    /// byte (F8: loses precision and wraps above 2559); u16 here.
    std::uint16_t prop3 = 2550;
    /// Mine deposit search, ship code (the original stores it inverted).
    bool prop5 = true;
    /// Feature-off mask: 0x02 volcano, 0x04 drought, 0x08 city events, 0x80
    /// event 7.5 min (bit 0x20 is read by CompLogic only).
    std::uint32_t prop6 = 0;
    /// Game seconds = tick / 500 + offset.
    std::uint32_t prop7_offset = 0;
    /// AI not loaded when non-zero.
    std::uint8_t prop9 = 0;
    /// Tax level 0..2 (economy, CompLogic); WorldSettings::validate rejects
    /// larger values because the economy indexes its tax tables by it.
    std::uint8_t prop10 = 0;
    std::uint32_t prop11 = 0;
    bool prop12 = false, prop13 = false;
    /// Bit 1 (0x2) = add-on edition (A11, D 0x100a3aed: the World constructor
    /// stores 3; AnnoScene 0x10009cc0 sets 1 for the base game or 3 for
    /// ANNOADDON1).
    std::uint32_t prop14 = 3;
    /// The agreed game speed in percent (SetSpeed; own design, no property).
    std::uint32_t speed_percent = 100;

    /// docs/sim/state.md §3.1. The settings arrive from a save or a resync
    /// snapshot, so they are validated like the configuration: empty when valid,
    /// else one message naming the first offending field ("WorldSettings.
    /// world_width: ..."). world_width and world_height must be at most
    /// kWorldEdgeBound (config.hpp: the derived island lookup grid and every
    /// island rect are bounded by them; their product is formed in u64 and
    /// cannot wrap) and prop10 must be 0..2.
    [[nodiscard]] std::string validate() const;

    [[nodiscard]] bool addon() const { return (prop14 & 2U) != 0; }
    /// Deposits deplete only in the add-on edition (OQ-035, D 0x10063686).
    [[nodiscard]] bool depletes() const { return addon(); }
    [[nodiscard]] bool volcano_off() const { return (prop6 & 0x02U) != 0; }
    [[nodiscard]] bool drought_off() const { return (prop6 & 0x04U) != 0; }
    [[nodiscard]] bool city_events_off() const { return (prop6 & 0x08U) != 0; }
    [[nodiscard]] bool event750_off() const { return (prop6 & 0x80U) != 0; }
};

} // namespace anno::sim
