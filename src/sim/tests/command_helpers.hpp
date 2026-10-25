// Shared helpers of the stage S10 command tests (command_build_test.cpp,
// property_test.cpp): worlds on the synthetic types with build costs
// (synthetic::make_build_types), command makers and read-back helpers.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace command_test {

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

inline constexpr sim::CityRef kCity0{sim::IslandId{0}, sim::CityNo{0}};
inline constexpr sim::CityRef kCity1{sim::IslandId{0}, sim::CityNo{1}};

inline syn::SyntheticTypes build_types() {
    auto t = syn::make_build_types();
    REQUIRE_MESSAGE(t.ok(), (t.ok() ? std::string() : t.error()));
    return std::move(t).value();
}

/// g2_scenario with a second player (slot 1, 25 000 gold) and a second city on island
/// 0: rows 0-1 neutral (y 100, 101), rows 2-31 city 0 (y 102..131), rows 32-63 city 1
/// of player 1 (y 132..163), which holds a weaver at (110,140) and the same stock as
/// city 0 (13 t tools, 30 t wood).
inline sim::ScenarioSpec two_city_spec(const syn::SyntheticTypes& t) {
    sim::ScenarioSpec spec = syn::g2_scenario(t);
    sim::ScenarioSpec::PlayerSpec p;
    p.id = sim::PlayerId{1};
    p.money = 25000;
    spec.players.push_back(p);
    sim::ScenarioSpec::IslandSpec& is = spec.islands[0];
    is.owner_runs = {{15, 128}, {0, 64U * 30U}, {1, 64U * 32U}};
    sim::ScenarioSpec::CitySpec c1 = syn::plain_city(1, 1);
    c1.stock = is.cities[0].stock;
    sim::ScenarioSpec::BuildingSpec w;
    w.type = t.weaver;
    w.x = 110;
    w.y = 140;
    c1.buildings.push_back(w);
    is.cities.push_back(c1);
    return spec;
}

inline sim::World make_world(const syn::SyntheticTypes& t, const sim::ScenarioSpec& spec,
                             sim::Rules rules = {}, sim::Seams seams = {},
                             sim::SimConfig cfg = {}) {
    auto w = sim::World::create(spec, t.registry, std::move(cfg), rules, std::move(seams));
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

inline sim::PlacementWire at(sim::TypeId type, std::int32_t x, std::int32_t y,
                             std::uint8_t rotation = 0) {
    return sim::PlacementWire{type, x, y, rotation, 0, 0};
}

inline sim::Command build(std::uint32_t issuer, sim::CityRef city,
                          std::vector<sim::PlacementWire> placements) {
    return sim::Command{sim::PlayerId{issuer}, sim::BuildCmd{city, std::move(placements)}};
}

inline sim::Command demolish(std::uint32_t issuer, sim::CityRef city,
                             std::vector<sim::TileXY> tiles) {
    return sim::Command{sim::PlayerId{issuer}, sim::DemolishCmd{city, std::move(tiles)}};
}

inline sim::Command trade_flag(std::uint32_t issuer, sim::CityRef city, std::uint16_t mode,
                               sim::GoodsId goods, std::uint32_t value) {
    return sim::Command{sim::PlayerId{issuer}, sim::SetTradeFlagCmd{city, mode, goods, value}};
}

inline sim::Command trade_slot(std::uint32_t issuer, sim::CityRef city, std::uint32_t list,
                               std::uint32_t index, sim::GoodsId goods, std::uint32_t amount_88 = 0,
                               std::uint32_t price = 0) {
    return sim::Command{sim::PlayerId{issuer},
                        sim::SetTradeSlotCmd{city, list, index, goods, amount_88, price}};
}

inline const sim::City& city(const sim::World& w, sim::CityRef ref) {
    const sim::City* c = w.islands().at(ref.island.v)->city(ref.city);
    REQUIRE(c != nullptr);
    return *c;
}

inline sim::City& city_mut(sim::World& w, sim::CityRef ref) {
    sim::City* c = sim::WorldTestAccess::islands(w).at(ref.island.v)->city(ref.city);
    REQUIRE(c != nullptr);
    return *c;
}

inline sim::GoodsId goods(const syn::SyntheticTypes& t, const char* name) {
    const auto g = t.registry->goods_by_name(name);
    REQUIRE(g.has_value());
    return *g;
}

/// The place of a building (the island's by_id cache).
inline sim::BuildingKey key_of(const sim::World& w, sim::BuildingId id) {
    const sim::Island& is = *w.islands().at(sim::island_of(id).v);
    const auto it = is.cache.by_id.find(id);
    REQUIRE(it != is.cache.by_id.end());
    return it->second;
}

/// The production list of a city (the list of the weaver's type).
inline const sim::BuildingList& production_list(const sim::World& w, sim::CityRef ref,
                                                const syn::SyntheticTypes& t) {
    const auto list = w.config().layout.list_for(t.registry->find(t.weaver)->prodtype);
    REQUIRE(list.has_value());
    return city(w, ref).index.lists.at(*list);
}

} // namespace command_test
