// Test-only door to the private members of World (docs/sim/state.md §14.4,
// "WorldTestAccess"): the tests that perturb a field, corrupt a copy or install
// configuration, rules, seams, a registry or the player table directly use it.
// It exists from stage S3 (the first stage with private members); later stages
// add members (the island table, stage S5).
#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <anno/sim/world.hpp>

namespace anno::sim {

struct WorldTestAccess {
    static SimConfig& config(World& w) { return w.config_; }
    static Rules& rules(World& w) { return w.rules_; }
    static WorldSettings& settings(World& w) { return w.settings_; }
    static Seams& seams(World& w) { return w.seams_; }
    static PlayerTable& players(World& w) { return w.players_; }
    static std::vector<std::optional<Island>>& islands(World& w) { return w.islands_; }
    static void set_types(World& w, std::shared_ptr<const TypeRegistry> t) {
        w.types_ = RegistryRef(std::move(t));
    }
};

} // namespace anno::sim
