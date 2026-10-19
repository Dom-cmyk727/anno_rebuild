// Helpers shared by the stage S6 tests (building_state_test.cpp,
// create_remove_test.cpp, world_state_test.cpp): a registry with the synthetic
// types plus a few types with a footprint, a service mask and a second mine, and
// worlds created on it.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/create.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "island_helpers.hpp"
#include "world_access.hpp"

namespace building_test {

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

/// The synthetic registry (ids 9001...9010, the same arguments as
/// make_synthetic_types) plus four types of this stage's tests.
struct Kit {
    std::shared_ptr<const sim::TypeRegistry> registry;
    sim::TypeId weaver{9001}, farm{9002}, fur_trapper{9003}, small_farm{9004}, bow_maker{9005},
        iron_mine{9006}, pioneer_house{9007}, kontor{9008}, market_stall{9009}, road{9010};
    /// PRODTYPE 98 (house group 1), 3 x 2 tiles.
    sim::TypeId big_house{9101};
    /// PRODTYPE 99 (house group 2), 2 x 12 tiles: taller than the 8 rows the
    /// original's lookup assumed (F3).
    sim::TypeId tall_house{9102};
    /// PRODTYPE 70 (the public list), 1 x 1, service mask 0x31 (bits 0, 4, 5).
    sim::TypeId church{9103};
    /// MINE producing GOLD (a mine that is always deep).
    sim::TypeId gold_mine{9104};
    /// PRODTYPE 200 would be a road; PRODTYPE 130 is a military number (list 2).
    sim::TypeId barracks{9105};
    /// PRODTYPE 12 HANDWERKDUAL: class DualWorkshop, Inert payload with a status
    /// time slot (D: class 12, +0x28).
    sim::TypeId dual_workshop{9106};
    /// PRODTYPE 78 BIBLIOTHEK (the public list), 1 x 1, service mask bit 31 only: the
    /// last of the 32 counters.
    sim::TypeId library_top{9107};
    /// PRODTYPE 110: inside the house block (list 3) but of class Plain, with the
    /// service mask 0x2: neither a house nor a public building.
    sim::TypeId house_list_plain{9108};
};

inline sim::BuildingType custom_type(sim::TypeId id, std::uint32_t prodtype, std::uint32_t w,
                                     std::uint32_t h, std::uint32_t service_mask = 0) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = prodtype;
    t.cls = sim::class_for_prodtype(prodtype).value_or(sim::RuntimeClass::Plain);
    t.width = w;
    t.height = h;
    t.service_mask = service_mask;
    t.running_cost = {0, 0};
    return t;
}

inline Kit make_kit() {
    Kit k;
    sim::TypeRegistry::Builder b;
    b.add_workshop(k.weaver, "STOFFE", "WOLLE", 6, 11, 5)
        .add_farm(k.farm, "KORN", "GETREIDE", 37, 80, 5)
        .add_fur_trapper(k.fur_trapper, "PELZE", "PELZTIER", 26, 25, 5)
        .add_small_farm(k.small_farm, "KORN", "WOLLE", "GETREIDE", "GRAS", 13, 4, 128, 50, 4)
        .add_weapons_workshop(k.bow_maker, "BOGEN", "ARMBRUST", "HOLZ", "SEILE", 9, 5)
        .add_mine(k.iron_mine, "EISENERZ", 21, 5)
        .add_mine(k.gold_mine, "GOLD", 21, 5)
        .add_plain(k.pioneer_house, 97)
        .add_plain(k.kontor, 35)
        .add_plain(k.market_stall, 161)
        .add_plain(k.road, 193)
        .add(custom_type(k.big_house, 98, 3, 2))
        .add(custom_type(k.tall_house, 99, 2, 12))
        .add(custom_type(k.church, 70, 1, 1, 0x31))
        .add(custom_type(k.barracks, 130, 1, 1))
        .add_plain(k.dual_workshop, 12)
        .add(custom_type(k.library_top, 78, 1, 1, 0x80000000U))
        .add(custom_type(k.house_list_plain, 110, 1, 1, 0x2));
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    k.registry = std::move(built.value());
    return k;
}

/// World::create on the kit's registry, required to succeed.
inline sim::World make_world(const Kit& k, const sim::ScenarioSpec& spec, sim::SimConfig cfg = {},
                             sim::Rules rules = {}) {
    auto r = sim::World::create(spec, k.registry, std::move(cfg), rules);
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

/// The error of World::create on the kit's registry (empty when it succeeded).
inline std::string create_error(const Kit& k, const sim::ScenarioSpec& spec,
                                sim::SimConfig cfg = {}, sim::Rules rules = {}) {
    auto r = sim::World::create(spec, k.registry, std::move(cfg), rules);
    return r.ok() ? std::string() : r.error();
}

/// The error of World::load on the kit's registry (empty when it succeeded).
inline std::string load_error(const Kit& k, const std::vector<std::uint8_t>& bytes) {
    auto r = sim::World::load(bytes, k.registry);
    return r.ok() ? std::string() : r.error();
}

inline sim::World load_world(const Kit& k, const std::vector<std::uint8_t>& bytes) {
    auto r = sim::World::load(bytes, k.registry);
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

inline sim::ScenarioSpec::BuildingSpec place(sim::TypeId type, std::int32_t x, std::int32_t y,
                                             std::uint8_t rotation = 0) {
    sim::ScenarioSpec::BuildingSpec b;
    b.type = type;
    b.x = x;
    b.y = y;
    b.rotation = rotation;
    return b;
}

inline sim::PlacementRecord record(sim::TypeId type, std::int32_t x, std::int32_t y,
                                   std::uint8_t rotation = 0) {
    sim::PlacementRecord r;
    r.type = type;
    r.x = x;
    r.y = y;
    r.rotation = rotation;
    return r;
}

/// The scenario of most tests: two players, island 0 at (100, 100), 64 x 64 with
/// two empty cities (0 and 1, both player 0's; the tile owners of basic_scenario
/// make the south part city 0's) and the iron deposit at (110, 120); island 3 at
/// (300, 200) as in two_island_scenario with its city 1 (player 1's).
inline sim::ScenarioSpec kit_scenario() {
    sim::ScenarioSpec spec = syn::two_island_scenario();
    spec.islands[0].cities.push_back(syn::plain_city(1, 0));
    return spec;
}

/// The kit world of the property tests: every payload alternative in a city, a
/// second city, holes in a list.
inline sim::World all_classes_world(const Kit& k) {
    sim::ScenarioSpec spec = kit_scenario();
    auto& c0 = spec.islands[0].cities[0];
    c0.buildings = {place(k.weaver, 101, 101),        place(k.farm, 103, 101),
                    place(k.fur_trapper, 105, 101),   place(k.small_farm, 107, 101),
                    place(k.bow_maker, 109, 101),     place(k.iron_mine, 110, 118),
                    place(k.pioneer_house, 101, 105), place(k.kontor, 103, 105),
                    place(k.church, 105, 105),        place(k.market_stall, 107, 105),
                    place(k.barracks, 109, 105),      place(k.big_house, 120, 120, 1)};
    auto& c1 = spec.islands[0].cities[1];
    c1.buildings = {place(k.weaver, 130, 101)};
    std::vector<std::uint32_t> stock(56, 0);
    stock[9] = 1000;
    c0.stock = stock;
    return make_world(k, spec);
}

inline sim::Island& isl(sim::World& w, std::size_t slot) {
    return *sim::WorldTestAccess::islands(w)[slot];
}
inline sim::City& city_of(sim::World& w, std::size_t island, std::size_t no) {
    return *isl(w, island).cities[no];
}

} // namespace building_test
