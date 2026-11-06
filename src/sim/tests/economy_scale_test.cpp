// ECON-BOOK-1 B6: explicitly synthetic boundary/pacing workload, not an original
// town, late-game or multiplayer performance claim. Preset bills are labelled.
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/city_booking.hpp>

#include "command_helpers.hpp"
#include "evidence_out.hpp"

namespace {
namespace sim = anno::sim;
using namespace command_test;
constexpr std::uint32_t idle_kind = 60001;
constexpr sim::TypeId unrelated_type{70001};
constexpr std::int64_t initial_cash = 1000000;
struct Workload {
    std::uint32_t cities, goods, buildings, figures;
};
sim::World scale_world(const Workload& n) {
    auto t = build_types();
    sim::SimConfig cfg;
    cfg.warehouse_goods = std::max(cfg.warehouse_goods, 26 + n.goods);
    cfg.billed_goods_last = 25 + n.goods;
    cfg.city_ring_div = 1;
    auto goods = sim::original_goods();
    while (goods.size() < cfg.warehouse_goods)
        goods.push_back(sim::GoodsInfo{"SCALE" + std::to_string(goods.size())});
    for (std::uint32_t g = 26; g < 26 + n.goods; ++g)
        goods[g].prices = sim::GoodsPrices{std::vector<std::uint32_t>(7, 256), {0, 0, 0}};
    sim::TypeRegistry::Builder builder;
    builder.config(cfg).goods(goods).walk_tables(t.registry->walk_tables());
    for (auto type : t.registry->types()) {
        if (type.id == t.weaver)
            type.running_cost = {2, 28};
        builder.add(type);
    }
    sim::BuildingType unrelated;
    unrelated.id = unrelated_type;
    unrelated.prodtype = 67;
    unrelated.cls = sim::RuntimeClass::PublicService;
    unrelated.output_goods = {26};
    unrelated.running_cost = {0, 0};
    builder.add(unrelated);
    builder.figure_type(sim::FigureType{idle_kind, 40265, 256, 0, 1000});
    auto types = builder.build();
    REQUIRE_MESSAGE(types, (types ? "" : types.error()));
    auto spec = sim::synthetic::empty_scenario(77);
    sim::ScenarioSpec::PlayerSpec player;
    player.id = sim::PlayerId{0};
    player.money = initial_cash;
    spec.players.push_back(player);
    auto island = sim::synthetic::plain_island(0, 100, 100, 64, 64);
    sim::synthetic::give_flat_terrain(island, 0x30);
    const auto stripe = 64 / n.cities;
    for (std::uint32_t i = 0; i < n.cities; ++i) {
        auto city = sim::synthetic::plain_city(i, 0);
        island.owner_runs.push_back({i, stripe * 64});
        sim::ScenarioSpec::BuildingSpec producer;
        producer.type = t.weaver;
        producer.x = 161;
        producer.y = 100 + static_cast<int>(i * stripe + stripe / 2);
        auto p = sim::make_workshop(t.registry->find(t.weaver)->prod);
        p.suspended = true;
        producer.payload = p;
        city.buildings.push_back(producer);
        island.cities.push_back(city);
    }
    for (std::uint32_t i = 0; i < n.buildings; ++i) {
        sim::ScenarioSpec::BuildingSpec b;
        b.type = unrelated_type;
        b.x = 100 + static_cast<int>(i % 60);
        b.y = 100 + static_cast<int>(i / 60);
        island.cities[0].buildings.push_back(b);
    }
    spec.islands.push_back(island);
    for (std::uint32_t i = 0; i < n.figures; ++i) {
        sim::ScenarioSpec::FigureSpec f;
        f.kind = idle_kind;
        f.owner = sim::PlayerId{0};
        f.pos = {256 * (100 + static_cast<int>(i % 64)), 256 * (100 + static_cast<int>(i / 64))};
        spec.figures.push_back(f);
    }
    sim::Rules rules;
    rules.city_booking = true;
    auto made = sim::World::create(spec, types.value(), cfg, rules);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    auto w = std::move(made).value();
    // Deliberate synthetic pre-booking counters, not a real economic-loop arm.
    for (std::uint32_t i = 0; i < n.cities; ++i) {
        auto& c = city_mut(w, sim::CityRef{sim::IslandId{0}, sim::CityNo{i}});
        CHECK(c.accounts.civil_cost == 28);
        for (std::uint32_t g = 26; g < 26 + n.goods; ++g)
            c.billed[g] = 1;
    }
    CHECK(w.figures().by_id.size() == n.figures);
    CHECK(w.check_invariants().empty());
    return w;
}
double elapsed(std::chrono::steady_clock::time_point before) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - before)
        .count();
}
} // namespace

TEST_CASE("economy scale: legal city goods object sizes bounded booking work and Release samples") {
    const std::vector<Workload> cases{
        {1, 15, 0, 0}, {4, 31, 64, 32}, {8, 63, 256, 128}, {1, 15, 64, 32}, {1, 15, 256, 128}};
    std::ostringstream table;
    table << "cities\tgoods\tunrelated_buildings\tunrelated_figures\tbooked_rows\tselector_"
             "lines\tsample\tpure_5000_ms\tworld_100_ms\tchecksum\n";
    for (const auto n : cases) {
        CAPTURE(n.cities);
        CAPTURE(n.goods);
        CAPTURE(n.buildings);
        CAPTURE(n.figures);
        auto base = scale_world(n);
        base.advance(4000);
        REQUIRE_FALSE(base.faulted());
        std::uint64_t bookings = 0, rows = 0, income = 0, upkeep = 0;
        for (const auto& e : base.last_output().events)
            if (e.booking) {
                REQUIRE(e.booking->fault == sim::AccountingFault::None);
                ++bookings;
                rows += e.booking->rows;
                income += e.booking->income;
                upkeep += e.booking->upkeep;
            }
        CHECK(bookings == n.cities);
        CHECK(rows == std::uint64_t{n.cities} * n.goods);
        CHECK(income == std::uint64_t{n.cities} * n.goods);
        CHECK(upkeep == n.cities);
        CHECK(base.players().find(sim::PlayerId{0})->money ==
              initial_cash + static_cast<std::int64_t>(n.cities) * (n.goods - 1));
        CHECK(base.players().find(sim::PlayerId{0})->upkeep_a == 0);
        CHECK(base.players().find(sim::PlayerId{0})->upkeep_b == 0);
        CHECK(base.check_invariants().empty());
        const auto* producer = base.building_at(kCity0, 161, 100 + static_cast<int>(32 / n.cities));
        REQUIRE(producer);
        const auto* type = base.types().find(producer->type);
        REQUIRE(type);
        const auto selector = sim::building_cost(base.types(), *type, *producer,
                                                 base.players().find(sim::PlayerId{0}));
        REQUIRE(selector);
        CHECK(selector.value().lines == 1);
        CHECK(selector.value().amount == 28);
        const auto good = type->output_goods[0];
        const sim::TileXY pos{producer->x, producer->y};
        auto lifecycle = base;
        REQUIRE(
            lifecycle.apply({sim::PlayerId{0}, sim::SetProductionLineCmd{pos, good, false}}).ok());
        CHECK(city(lifecycle, kCity0).accounts.civil_cost == 2);
        REQUIRE(
            lifecycle.apply({sim::PlayerId{0}, sim::SetProductionLineCmd{pos, good, true}}).ok());
        CHECK(city(lifecycle, kCity0).accounts.civil_cost == 28);
        REQUIRE(lifecycle.apply(demolish(0, kCity0, {pos})).ok());
        CHECK(city(lifecycle, kCity0).accounts.civil_cost == 0);
        CHECK(lifecycle.check_invariants().empty());
        std::vector<sim::BookingRow> prices(n.goods, sim::BookingRow{256, 0, 1});
        sim::CityAccounts accounts;
        accounts.civil_cost = 28;
        for (int sample = 0; sample < 3; ++sample) {
            std::uint64_t checksum = 0;
            auto before = std::chrono::steady_clock::now();
            for (int i = 0; i < 5000; ++i) {
                const auto result = sim::calculate_city_booking(base.config(), 2000, accounts,
                                                                prices, initial_cash);
                checksum += static_cast<std::uint64_t>(result.money);
            }
            const auto pure_ms = elapsed(before);
            CHECK(checksum == 5000ULL * static_cast<std::uint64_t>(initial_cash + n.goods - 1));
            auto world = base;
            (void)world.drain_events();
            before = std::chrono::steady_clock::now();
            for (int i = 0; i < 100; ++i) {
                world.advance(4000);
                (void)world.drain_events();
            }
            const auto world_ms = elapsed(before);
            REQUIRE_FALSE(world.faulted());
            CHECK(world.check_invariants().empty());
            CHECK(world.players().find(sim::PlayerId{0})->money ==
                  base.players().find(sim::PlayerId{0})->money -
                      100 * static_cast<std::int64_t>(n.cities));
            table << n.cities << '\t' << n.goods << '\t' << n.buildings << '\t' << n.figures << '\t'
                  << rows << '\t' << selector.value().lines << '\t' << sample << '\t'
                  << std::setprecision(9) << pure_ms << '\t' << world_ms << '\t' << checksum
                  << '\n';
        }
    }
    if (const char* path = std::getenv("ANNO_ECON_SCALE_OUT")) {
        const auto problem = sim::test::write_evidence_file(path, table.str());
        REQUIRE_MESSAGE(problem.empty(), problem);
    }
    MESSAGE("synthetic bounded samples; timing is host observation, never an assertion\n"
            << table.str());
}
