// Extension points as named function pointers (spec: docs/sim/state.md §4.12,
// §12). Seams are configuration, not state: installed when the world is
// created, copied with it, and their NAMES are hashed in the header scope
// (state_fields.hpp), so a replay recorded with one implementation cannot
// silently run with another. A null function is the documented default
// (faithful where known, inert otherwise).
//
// Stage S3 declares the seam table and its names; the types the signatures
// mention (TickContext, Island, City, Effects, SerialWorld, ...) are defined by
// the stages that own them (Player S4, Island S5, City S6, TickContext, Effects
// and SerialWorld S7, Command S8) and are only named here, which is enough to
// declare the function pointer types. The scheduler calls city_pass,
// wildlife_sweep, object_manager (S7), player_groups and defeat (bound to the
// PlayerHooks of the player table, S7); the others are called by later stages. Signatures take
// contexts, never a mutable World&. `ctx` carries the seam's own configuration (a recording seam in
// a test, an AI's parameter block); it is NOT state: it is not hashed, saved or copied deep, and a
// seam must not let anything behind it influence sim state (a review item; the seam's name is what
// is hashed).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <anno/sim/config.hpp>
#include <anno/sim/events.hpp>

namespace anno::sim {

// Types the signatures below name; defined by the stages that own them.
struct TickContext;
class SerialWorld;
struct Effects;
struct Island;
struct City;
struct Figure;
struct Player;
struct Command;
struct Building;
struct BuildingType;
struct PlacementRecord;
enum class Reject : std::uint16_t;

/// The name of a seam without a function. validate_seams refuses an installed
/// function with this name: the name is the only identity of an implementation
/// that the header hash sees, so a function named like the default would hash
/// exactly like no function at all, and two peers running different logic
/// would not diverge at tick 0.
inline constexpr const char* kDefaultSeamName = "default";

/// One extension point: the name that is hashed, the function (null = the
/// default) and an opaque context for the function. Call as `fn(ctx, ...)`.
/// The name is the identity of the implementation: two different functions
/// that share a name are indistinguishable to the hash, so a name must be
/// unique per implementation (and per version of its behaviour).
template <class Fn> struct Seam {
    const char* name = kDefaultSeamName;
    Fn fn = nullptr;
    void* ctx = nullptr;
};

struct Seams {
    using CityPass = void (*)(void* ctx, const TickContext&, Island&, City&, Effects&);
    /// Size SimConfig::city_pass_count: D 0x10027400, 0x10029610, 0x1002d350,
    /// 0x10029950 (economy), 0x1002b670, 0x1002ba80 (random events, PRNG),
    /// 0x1002a5e0, 0x1002d3b0.
    std::vector<Seam<CityPass>> city_pass = std::vector<Seam<CityPass>>(kOriginalCityPassCount);

    using ChunkPass = void (*)(void* ctx, const TickContext&, Island&, std::uint32_t x,
                               std::uint32_t y, Effects&);
    /// D 0x10093480 (O-12).
    Seam<ChunkPass> wildlife_sweep;

    using IslandPass = void (*)(void* ctx, const TickContext&, Island&, Effects&);
    /// D 0x10095d40 object bodies (O-12).
    Seam<IslandPass> object_manager;
    /// D 0x1008aeb0 tile effects (O-15).
    Seam<IslandPass> drought_tiles;

    using FigureTick = void (*)(void* ctx, const TickContext&, SerialWorld&, Figure&);
    /// Serial figure phase (§5.9), D unit slot +0x74 (O-8); goods traffic via
    /// Effects::reserves / deliveries / goods_moves.
    Seam<FigureTick> figure_tick;

    using PlayerPass = void (*)(void* ctx, const TickContext&, SerialWorld&, Player&);
    /// D 0x10062a40 (O-20).
    Seam<PlayerPass> player_groups;
    /// D 0x10020550 (O-22).
    Seam<PlayerPass> defeat;

    using Ai = void (*)(void* ctx, const TickContext&, const SerialWorld&,
                        std::vector<Command>& out);
    /// CompLogic, every advance (O-1); the commands are applied at once in
    /// order and NOT logged (A15, §8.2). Draws through the DrawSource
    /// (DrawSite::Ai).
    Seam<Ai> ai;

    using EventHandler = void (*)(void* ctx, const TickContext&, SerialWorld&, const TimedEvent&);
    /// Run inside the serial event phase: D 0x100adae0, 0x100adb80 (O-18, O-19).
    Seam<EventHandler> tile_cleanup;
    Seam<EventHandler> delayed_hit;

    using PlaceHook = void (*)(void* ctx, const TickContext&, Island&, City&, Building&, Effects&);
    /// O-4, O-5: PRODTYPE hooks 0x44/0x49/0x4e/0x41, land claim, goods handlers.
    Seam<PlaceHook> on_placed;
    Seam<PlaceHook> on_removed;

    using PlacementRuleFn = Reject (*)(void* ctx, const Island&, const BuildingType&,
                                       const PlacementRecord&);
    /// D 0x1007c790 (O-27); the default rule is §8.4.
    Seam<PlacementRuleFn> placement_rule;

    using Upgrade = void (*)(void* ctx, const BuildingType&, PlacementRecord&);
    /// D 0x100276a0 for PRODTYPE 0x43/0x48 (O-28).
    Seam<Upgrade> upgrade_rewrite;

    using CityEmpty = bool (*)(void* ctx, const City&);
    /// D city slot +0x78 = 0x1002f7f0 (O-6); the default is false.
    Seam<CityEmpty> city_is_empty;

    /// A default seam table sized for `cfg` (city_pass_count entries).
    static Seams sized_for(const SimConfig& cfg) {
        Seams s;
        s.city_pass.assign(cfg.city_pass_count, Seam<CityPass>{});
        return s;
    }
};

/// The seam names in Seams declaration order, city_pass first: what the header
/// scope hashes and a load compares. A null name counts as "".
inline std::vector<std::string> seam_names(const Seams& s) {
    std::vector<std::string> out;
    auto add = [&out](const char* name) { out.emplace_back(name != nullptr ? name : ""); };
    for (const auto& p : s.city_pass) {
        add(p.name);
    }
    add(s.wildlife_sweep.name);
    add(s.object_manager.name);
    add(s.drought_tiles.name);
    add(s.figure_tick.name);
    add(s.player_groups.name);
    add(s.defeat.name);
    add(s.ai.name);
    add(s.tile_cleanup.name);
    add(s.delayed_hit.name);
    add(s.on_placed.name);
    add(s.on_removed.name);
    add(s.placement_rule.name);
    add(s.upgrade_rewrite.name);
    add(s.city_is_empty.name);
    return out;
}

/// The seam consistency checks of docs/sim/state.md §3.1, run by World::create
/// and World::load next to the config validation: city_pass has exactly
/// SimConfig::city_pass_count entries and every installed (non-null) function
/// has a name that is neither empty nor kDefaultSeamName. Empty when valid,
/// else one message.
inline std::string validate_seams(const Seams& s, const SimConfig& cfg) {
    if (s.city_pass.size() != cfg.city_pass_count) {
        return "Seams.city_pass: has " + std::to_string(s.city_pass.size()) +
               " entries, SimConfig.city_pass_count is " + std::to_string(cfg.city_pass_count);
    }
    auto named = [](const char* name) {
        return name != nullptr && *name != '\0' && std::string_view(name) != kDefaultSeamName;
    };
    constexpr const char* kNeedsName =
        ": an installed function needs a name of its own (not empty, not \"default\")";
    for (std::size_t i = 0; i < s.city_pass.size(); ++i) {
        if (s.city_pass[i].fn != nullptr && !named(s.city_pass[i].name)) {
            return "Seams.city_pass[" + std::to_string(i) + "]" + kNeedsName;
        }
    }
    auto check = [&](const char* field, const char* name, bool installed) -> std::string {
        if (installed && !named(name)) {
            return std::string("Seams.") + field + kNeedsName;
        }
        return {};
    };
    const std::string problems[] = {
        check("wildlife_sweep", s.wildlife_sweep.name, s.wildlife_sweep.fn != nullptr),
        check("object_manager", s.object_manager.name, s.object_manager.fn != nullptr),
        check("drought_tiles", s.drought_tiles.name, s.drought_tiles.fn != nullptr),
        check("figure_tick", s.figure_tick.name, s.figure_tick.fn != nullptr),
        check("player_groups", s.player_groups.name, s.player_groups.fn != nullptr),
        check("defeat", s.defeat.name, s.defeat.fn != nullptr),
        check("ai", s.ai.name, s.ai.fn != nullptr),
        check("tile_cleanup", s.tile_cleanup.name, s.tile_cleanup.fn != nullptr),
        check("delayed_hit", s.delayed_hit.name, s.delayed_hit.fn != nullptr),
        check("on_placed", s.on_placed.name, s.on_placed.fn != nullptr),
        check("on_removed", s.on_removed.name, s.on_removed.fn != nullptr),
        check("placement_rule", s.placement_rule.name, s.placement_rule.fn != nullptr),
        check("upgrade_rewrite", s.upgrade_rewrite.name, s.upgrade_rewrite.fn != nullptr),
        check("city_is_empty", s.city_is_empty.name, s.city_is_empty.fn != nullptr),
    };
    for (const std::string& p : problems) {
        if (!p.empty()) {
            return p;
        }
    }
    return {};
}

} // namespace anno::sim
