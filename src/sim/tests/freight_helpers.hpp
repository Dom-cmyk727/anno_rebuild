#pragma once
#include <algorithm>

#include <doctest/doctest.h>

#include <anno/sim/freight.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/ship.hpp>
#include <anno/sim/synthetic.hpp>

#include "world_access.hpp"

namespace freight_test {
namespace sim = anno::sim;
inline constexpr sim::GoodsId goods = 42;
inline constexpr std::uint32_t ship_kind = 9510;
inline constexpr sim::FigureId ship_id{1};
inline std::shared_ptr<const sim::TypeRegistry> registry(bool alternate = false) {
    const auto base = sim::synthetic::make_cargo_types();
    REQUIRE(base);
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    for (const auto& t : base.value().registry->types())
        b.add(t);
    auto ship = *base.value().registry->find_figure_type(ship_kind);
    ship.load_ticks = 500;
    b.figure_type(ship);
    if (alternate) {
        auto wh = *base.value().registry->find(sim::TypeId{1302});
        wh.id = sim::TypeId{70001};
        b.add(wh);
    }
    auto out = b.build();
    REQUIRE(out);
    return out.value();
}
inline sim::World make(unsigned ships = 1, bool alternate = false, std::uint32_t work = 64) {
    auto spec = sim::synthetic::empty_scenario(11);
    for (unsigned p = 0; p < 2; ++p) {
        sim::ScenarioSpec::PlayerSpec ps;
        ps.id = sim::PlayerId{p};
        ps.money = 5000;
        spec.players.push_back(ps);
    }
    for (unsigned i = 0; i < 3; ++i) {
        auto island = sim::synthetic::plain_island(i, 100 + 80 * static_cast<int>(i), 100, 32, 32);
        island.owner_runs = {{0, 1024}};
        auto city = sim::synthetic::plain_city(0, i == 2 ? 1 : 0);
        city.stock = std::vector<std::uint32_t>(100, 0);
        (*city.stock)[goods] = i == 0 ? 20 * 256 : 0;
        city.capacity = 50 * 256;
        city.buildings.push_back(
            {.type = sim::TypeId{alternate ? 70001U : 1302U}, .x = island.x0, .y = 108});
        island.cities.push_back(city);
        spec.islands.push_back(island);
    }
    for (unsigned i = 0; i < ships; ++i) {
        sim::ScenarioSpec::FigureSpec f;
        f.kind = ship_kind;
        f.owner = sim::PlayerId{0};
        f.pos = {95 * 256, 110 * 256};
        f.dir16 = 8;
        f.holds = std::vector<sim::ShipHold>(4);
        spec.figures.push_back(f);
    }
    sim::Rules rules;
    rules.freight.work_per_visit = work;
    sim::SimConfig config;
    config.warehouse_goods = 100;
    auto out = sim::World::create(spec, registry(alternate), config, rules);
    REQUIRE_MESSAGE(out, (out ? "" : out.error()));
    return std::move(out).value();
}
inline sim::BuildingId warehouse(const sim::World& w, unsigned island = 0) {
    const auto* b = w.building_at({sim::IslandId{island}, sim::CityNo{0}},
                                  100 + 80 * static_cast<int>(island), 108);
    REQUIRE(b != nullptr);
    return b->id;
}
inline sim::AssignFreightRouteCmd route(const sim::World& w, sim::GoodsId g = goods) {
    using D = sim::FreightDirection;
    return {ship_id,
            {{warehouse(w), 95 * 256, 110 * 256, {{D::LoadTarget, g, 2}}},
             {warehouse(w, 1), 175 * 256, 110 * 256, {{D::UnloadAmount, g, 2}}}}};
}
inline sim::ApplyResult apply(sim::World& w, sim::Body body, unsigned issuer = 0) {
    auto wire = sim::encode_record(body);
    REQUIRE(wire);
    auto decoded = sim::decode_record(wire.value());
    REQUIRE(decoded);
    return w.apply({sim::PlayerId{issuer}, std::move(decoded).value()});
}
inline sim::Figure& ship(sim::World& w, sim::FigureId id = ship_id) {
    auto* f = sim::find_figure(sim::WorldTestAccess::figures(w), id);
    REQUIRE(f != nullptr);
    return *f;
}
} // namespace freight_test
