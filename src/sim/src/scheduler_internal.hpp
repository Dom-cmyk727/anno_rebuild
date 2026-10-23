// The scheduler's internal door, shared by scheduler.cpp and apply.cpp (the commands
// of stage S10 run the Place and Remove seams and merge their Effects). Not public
// API: the header lives next to the sources, not in include/.
#pragma once

#include <cstdint>

#include <anno/sim/ids.hpp>

namespace anno::sim {

class World;
struct Effects;
struct Player;
struct Island;
struct City;
struct TickContext;
struct CountingDraw;
struct PopResult;

/// The friend of World that runs advance and merges Effects (docs/sim/state.md §5.3).
struct Scheduler {
    static std::uint64_t advance(World& w, std::uint32_t game_ms);
    static void release_counter(World& w, BuildingId home, std::uint32_t home_counter);
    /// Merges `fx` in the order of §5.3 (spawns, releases, money, goods moves,
    /// reserves, deliveries, events, UI) at `tick`. Inside an advance (`in_advance`)
    /// the UI events and the draws are also part of the advance's SimOutput; between
    /// two advances (a seam that a command runs, S10) the UI events go to the host
    /// queue only and no SimOutput is touched (it belongs to the last advance).
    static void merge(World& w, Effects& fx, std::uint64_t tick, std::uint32_t t,
                      bool in_advance = true);
    /// Step 2 of advance (events.cpp, docs/sim/state.md §5.7): one dispatch pass of the
    /// timed-event queue with the two handler seams, each handler's Effects merged
    /// right after it returns (C2).
    static PopResult run_events(World& w, const TickContext& ctx, CountingDraw& draw);
    /// Step 3 (docs/sim/state.md §5.9): the figure registry's phases B and C (phase D
    /// serves the pooled sector records, not modelled).
    static void run_figures(World& w, const TickContext& ctx, CountingDraw& draw);
    /// Destroys the figure `id` (phase C): frees its slot and takes it off the expiry
    /// list. Releases nothing (the counter went back at the figure's end, end_figure).
    static void destroy_figure(World& w, FigureId id);
    static void run_player_pass(void* raw, Player& p);
    static void run_ai(void* raw);
    static Island* island_of_id(World& w, IslandId id);
    static City* city_of_ref(World& w, CityRef ref);
};

} // namespace anno::sim
