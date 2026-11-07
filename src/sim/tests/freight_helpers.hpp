#pragma once
#include <algorithm>
#include <map>

#include <doctest/doctest.h>

#include <anno/sim/freight.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/ship.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>

#include "world_access.hpp"

namespace freight_test {
namespace sim = anno::sim;
inline constexpr sim::GoodsId goods = 42;
inline constexpr std::uint32_t ship_kind = 9510;
inline constexpr sim::FigureId ship_id{1};
inline std::shared_ptr<const sim::TypeRegistry> registry(bool alternate = false,
                                                         std::uint32_t capacity = 12800,
                                                         std::uint32_t holds = 4,
                                                         bool land_warehouse = false) {
    const auto base = sim::synthetic::make_cargo_types();
    REQUIRE(base);
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    // Synthetic route-only unit fixture: no market carts are spawned. This
    // isolates the route clock from unrelated figure-ring population changes.
    for (const auto& t : base.value().registry->types()) {
        auto copy = t;
        std::fill(copy.prod.max_figures.begin(), copy.prod.max_figures.end(), 0);
        b.add(copy);
    }
    auto ship = *base.value().registry->find_figure_type(ship_kind);
    ship.load_ticks = 500;
    ship.capacity_88 = capacity;
    ship.nav->hold_count_a = holds;
    b.figure_type(ship);
    for (const auto& f : base.value().registry->figure_types())
        if (f.id != ship_kind)
            b.figure_type(f);
    if (alternate) {
        auto wh = *base.value().registry->find(sim::TypeId{1302});
        wh.id = sim::TypeId{70001};
        if (land_warehouse)
            wh.flags_word = sim::synthetic::ordinary_land_flags();
        std::fill(wh.prod.max_figures.begin(), wh.prod.max_figures.end(), 0);
        b.add(wh);
    }
    auto out = b.build();
    REQUIRE(out);
    return out.value();
}
struct Setup {
    unsigned ships = 1;
    bool alternate = false;
    bool nearby_warehouse = false, rival_nearby = false, shared_visibility = false, crew = false;
    bool landlocked_goal = false, land_warehouse = false;
    std::vector<unsigned> owners;
    std::map<unsigned, std::vector<sim::ShipHold>> holds_by_ship;
    std::map<sim::GoodsId, std::uint32_t> destination_stock;
    std::map<sim::GoodsId, std::uint32_t> nearby_stock = {{goods, 1024}}, rival_stock;
    std::uint32_t work = 64, capacity = 12800, cargo_holds = 4, store_capacity = 12800;
    std::vector<sim::ShipHold> initial_holds = std::vector<sim::ShipHold>(4);
    std::map<sim::GoodsId, std::uint32_t> stock = {{goods, 20 * 256}};
};
inline sim::World make_with(const Setup& setup) {
    const auto ships = setup.ships;
    const auto alternate = setup.alternate;
    const auto work = setup.work;
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
        if (setup.land_warehouse)
            sim::synthetic::give_flat_terrain(island, 0x30);
        auto city = sim::synthetic::plain_city(0, i == 2 ? 1 : 0);
        city.stock = std::vector<std::uint32_t>(100, 0);
        if (i == 0)
            for (const auto& [g, amount] : setup.stock)
                (*city.stock)[g] = amount;
        if (i == 1)
            for (const auto& [g, amount] : setup.destination_stock)
                (*city.stock)[g] = amount;
        if (i == 2)
            for (const auto& [g, amount] : setup.rival_stock)
                (*city.stock)[g] = amount;
        city.capacity = setup.store_capacity;
        city.buildings.push_back(
            {.type = sim::TypeId{alternate ? 70001U : 1302U}, .x = island.x0, .y = 108});
        island.cities.push_back(city);
        if (i == 0 && setup.nearby_warehouse) {
            island.owner_runs.clear();
            for (unsigned row = 0; row < 32; ++row) {
                island.owner_runs.emplace_back(0, 6);
                island.owner_runs.emplace_back(1, 26);
            }
            auto neighbor = sim::synthetic::plain_city(1, setup.rival_nearby ? 1 : 0);
            neighbor.stock = std::vector<std::uint32_t>(100, 0);
            neighbor.capacity = setup.store_capacity;
            for (const auto& [g, amount] : setup.nearby_stock)
                (*neighbor.stock)[g] = amount;
            neighbor.buildings.push_back(
                {.type = sim::TypeId{alternate ? 70001U : 1302U}, .x = 106, .y = 108});
            island.cities.push_back(neighbor);
        }
        spec.islands.push_back(island);
    }
    if (setup.landlocked_goal) {
        auto land = sim::synthetic::plain_island(3, 400, 400, 40, 40);
        sim::synthetic::give_flat_terrain(land, 0x40);
        spec.islands.push_back(std::move(land));
    }
    for (unsigned i = 0; i < ships; ++i) {
        sim::ScenarioSpec::FigureSpec f;
        f.kind = ship_kind;
        f.owner = sim::PlayerId{i < setup.owners.size() ? setup.owners[i] : 0};
        f.pos = {98 * 256 + 128, 110 * 256 + 128};
        f.dir16 = 8;
        if (setup.nearby_warehouse)
            f.pos = {105 * 256 + 128, 110 * 256 + 128};
        if (f.owner.v == 1 && !setup.nearby_warehouse)
            f.pos = {258 * 256 + 128, 110 * 256 + 128};
        f.holds = setup.holds_by_ship.contains(i) ? setup.holds_by_ship.at(i) : setup.initial_holds;
        spec.figures.push_back(f);
    }
    if (setup.crew) {
        sim::ScenarioSpec::FigureSpec scout;
        scout.kind = 717;
        scout.owner = sim::PlayerId{0};
        scout.pos = spec.figures[0].pos;
        scout.parent = 0;
        scout.crew_slot = 0;
        spec.figures.push_back(scout);
        auto donkey = scout;
        donkey.kind = 718;
        donkey.parent = static_cast<std::uint32_t>(spec.figures.size() - 1);
        donkey.crew_slot.reset();
        spec.figures.push_back(donkey);
    }
    sim::Rules rules;
    if (setup.shared_visibility)
        rules.shared_visibility = {{0, 1}};
    rules.freight.work_per_visit = work;
    sim::SimConfig config;
    config.warehouse_goods = 100;
    config.figure_min_growth = 1;
    auto out = sim::World::create(
        spec, registry(alternate, setup.capacity, setup.cargo_holds, setup.land_warehouse), config,
        rules);
    REQUIRE_MESSAGE(out, (out ? "" : out.error()));
    return std::move(out).value();
}
inline sim::World make(unsigned ships = 1, bool alternate = false, std::uint32_t work = 64) {
    Setup setup;
    setup.ships = ships;
    setup.alternate = alternate;
    setup.work = work;
    return make_with(setup);
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
            {{warehouse(w), 98 * 256 + 128, 110 * 256 + 128, {{D::LoadTarget, g, 2}}},
             {warehouse(w, 1), 178 * 256 + 128, 110 * 256 + 128, {{D::UnloadAmount, g, 2}}}}};
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
// The ship's freight route, which must exist: REQUIRE it, then use it, so that a missing route
// fails the case with a message and GCC -O3 -Werror=null-dereference sees a checked pointer.
inline const sim::FreightState& live_route(const sim::World& w, sim::FigureId id = ship_id) {
    const sim::FreightState* r = w.freight_route(id);
    REQUIRE(r != nullptr);
    return *r;
}
} // namespace freight_test
