// Test-only door to the private members of World (docs/sim/state.md §14.4,
// "WorldTestAccess"): the tests that perturb a field, corrupt a copy or install
// configuration, rules, seams, a registry or the player table directly use it.
// It exists from stage S3 (the first stage with private members); later stages
// add members (the island table, stage S5; create and remove, stage S6; the id
// counters, the figure registry and the event queue, stage S7).
#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/create.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

struct WorldTestAccess {
    static WorldClock& clock(World& w) { return w.clock_; }
    static WorldRng& rng(World& w) { return w.rng_; }
    static HostEventQueue& host_events(World& w) { return w.host_events_; }
    static SimConfig& config(World& w) { return w.config_; }
    static Rules& rules(World& w) { return w.rules_; }
    static WorldSettings& settings(World& w) { return w.settings_; }
    static Seams& seams(World& w) { return w.seams_; }
    static PlayerTable& players(World& w) { return w.players_; }
    static std::vector<std::optional<Island>>& islands(World& w) { return w.islands_; }
    static IdCounters& ids(World& w) { return w.ids_; }
    static FigureRegistry& figures(World& w) { return w.figures_; }
    static EventQueue& events(World& w) { return w.events_; }
    static ScriptTimers& timers(World& w) { return w.timers_; }
    static MonopolyTable& monopoly(World& w) { return w.monopoly_; }
    static std::uint32_t& event750_acc(World& w) { return w.event750_acc_; }
    static bool& faulted(World& w) { return w.faulted_; }
    static SimOutput& output(World& w) { return w.out_; }
    static void set_types(World& w, std::shared_ptr<const TypeRegistry> t) {
        w.types_ = RegistryRef(std::move(t));
    }
    /// create_building on city `city` of island slot `island` with the world's own
    /// configuration, rules and registry (stage S6). The island and the city must
    /// exist.
    static formats::Result<BuildingKey, Reject>
    create(World& w, std::uint32_t island, std::uint32_t city, const PlacementRecord& rec,
           std::uint32_t now = 0, const Payload* initial = nullptr) {
        Island& is = *w.islands_[island];
        return create_building(w.config_, w.rules_, *w.types_, is, *is.cities[city], rec, now,
                               initial);
    }
    /// remove_building with the world's own configuration, registry and players.
    static bool remove(World& w, const BuildingKey& key) {
        return remove_building(w.config_, w.rules_, *w.types_, w.players_,
                               *w.islands_[key.city.island.v], key);
    }
};

} // namespace anno::sim
