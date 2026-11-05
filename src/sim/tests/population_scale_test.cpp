// MAP-POP-LIVE-1 L5: scale and proportionality of the live House path, and a MOD-DEFINED group,
// goods roles, figure ids and supplier (docs/sim/population-live.md section 20). Synthetic
// registry; no shipped id is assumed anywhere: the house group has the made-up PRODTYPE 99, the
// needs are goods 29 and 30, the walkers are the figure types 7001..7003, the supplier is the
// made-up type 8890.
//
// The counters (PopulationStats/HouseWork) are diagnostic, never state. The tests assert operation
// COUNTS, not wall time; the timings are printed only.
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/create.hpp>
#include <anno/sim/house_transition.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace {

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

constexpr sim::TypeId kHouse{8801};  // PRODTYPE 99 = group 2 of a three-group table (made up)
constexpr sim::TypeId kStall{8890};  // class MarketStall, PRODTYPE 161, offers goods 29 and 30
constexpr sim::TypeId kHouse0{8701}; // group 0 (standard stand-in so the table is complete)
constexpr sim::TypeId kHouse1{8721}; // group 1
constexpr sim::GoodsId kG29 = 29, kG30 = 30;

sim::BuildingType house_type(sim::TypeId id, std::uint32_t prodtype, std::uint32_t first_figure) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = prodtype;
    t.cls = sim::RuntimeClass::House;
    t.width = t.height = 2;
    t.action_radius = 8;
    t.head_raw.assign(56, 0);
    t.entrances = {{3, 3}};
    t.has_prod = true;
    t.prod.kind = sim::BuildingClass::Other;
    t.prod.max_figures = {2, 0, 0};
    t.prod.figure_type_count = {3, 0, 0};
    t.prod.figure_ids = {first_figure, first_figure + 1, first_figure + 2, 0, 0, 0, 0, 0, 0};
    return t;
}

sim::HouseGroup group(std::uint32_t number, std::uint32_t wohnmax, sim::GoodsId a, sim::GoodsId b) {
    sim::HouseGroup g;
    g.number = number;
    g.wohnmax = static_cast<std::uint16_t>(wohnmax);
    g.no_downgrade = number == 0;
    g.needs = {{a, true, 30}, {b, true, 20}};
    g.vermehr = {-16, -9, 0, 12, 32};
    for (std::size_t i = 0; i < 21; ++i) {
        g.nahrung[i] = 2;
        g.bedarf[i] = 2;
        g.infra[i] = 2;
    }
    return g;
}

sim::SimConfig mod_config() {
    sim::SimConfig cfg = syn::population_config();
    cfg.house_groups = 3;
    return cfg;
}

std::shared_ptr<const sim::TypeRegistry> mod_registry() {
    sim::TypeRegistry::Builder b;
    b.config(mod_config());
    b.walk_tables(sim::synthetic_walk_tables());
    b.goods(sim::original_goods());
    b.house_groups({group(0, 8, 26, 27), group(1, 15, 26, 27), group(2, 20, kG29, kG30)});
    b.add(house_type(kHouse0, 97, 7001));
    b.add(house_type(kHouse1, 98, 7001));
    b.add(house_type(kHouse, 99, 7001));
    sim::BuildingType stall;
    stall.id = kStall;
    stall.prodtype = 161;
    stall.cls = sim::RuntimeClass::MarketStall;
    stall.entrances = {{3, 3}};
    stall.output_goods = {kG29, kG30};
    b.add(stall);
    for (const std::uint32_t id : {7001U, 7002U, 7003U}) {
        b.figure_type(sim::FigureType{id, 805300, 3 * 256, 0, 1000});
    }
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

sim::House house_payload() {
    sim::House h;
    h.residents_64 = 320;
    h.e = 640;
    h.stock = {0, 0};
    h.walkers.assign(2, sim::FigureId{});
    return h;
}

/// A scenario of blocks: each block is one house and its stall four tiles east. Blocks sit 24 tiles
/// apart (the candidate square of a house is +/- 8 tiles), so a block's work is its own.
struct Scene {
    sim::ScenarioSpec spec;
    std::uint32_t blocks = 0;

    explicit Scene(std::uint32_t seed = 5) {
        spec = syn::empty_scenario(seed);
        sim::ScenarioSpec::PlayerSpec p;
        p.id = sim::PlayerId{0};
        spec.players.push_back(p);
    }
    std::size_t add_island(std::uint32_t slot, std::int32_t x0, std::int32_t y0, std::uint32_t w,
                           std::uint32_t h) {
        sim::ScenarioSpec::IslandSpec is = syn::plain_island(slot, x0, y0, w, h);
        is.owner_runs = {{0, w * h}};
        sim::ScenarioSpec::CitySpec c = syn::plain_city(0, 0);
        c.capacity = 100000000;
        std::vector<std::uint32_t> stock(mod_config().warehouse_goods, 0);
        REQUIRE(stock.size() > kG30);
        stock[kG29] = stock[kG30] = 50000000;
        c.stock = stock;
        is.cities.push_back(std::move(c));
        spec.islands.push_back(std::move(is));
        return spec.islands.size() - 1;
    }
    void block(std::size_t island, std::int32_t x, std::int32_t y) {
        sim::ScenarioSpec::BuildingSpec h;
        h.type = kHouse;
        h.x = x;
        h.y = y;
        h.payload = house_payload();
        spec.islands[island].cities[0].buildings.push_back(h);
        sim::ScenarioSpec::BuildingSpec s;
        s.type = kStall;
        s.x = x + 4;
        s.y = y;
        spec.islands[island].cities[0].buildings.push_back(s);
        ++blocks;
    }
};

sim::World make_world(const Scene& s, const std::shared_ptr<const sim::TypeRegistry>& types) {
    sim::Rules rules;
    rules.house_updates = true;
    auto w = sim::World::create(s.spec, types, mod_config(), rules);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

/// The grid of blocks on island 0 (origin 110, 110): n blocks, row-major, `per_row` to a row.
void grid(Scene& s, std::size_t island, std::int32_t x0, std::int32_t y0, std::uint32_t n,
          std::uint32_t per_row) {
    for (std::uint32_t k = 0; k < n; ++k) {
        s.block(island, x0 + 24 * static_cast<std::int32_t>(k % per_row),
                y0 + 24 * static_cast<std::int32_t>(k / per_row));
    }
}

const sim::HouseWork& work(const sim::TransportStats& st, sim::BuildingId id) {
    const auto it = st.population.houses.find(id);
    REQUIRE(it != st.population.houses.end());
    return it->second;
}

} // namespace

TEST_CASE("population scale: a mod-defined group, goods, figure ids and supplier work end to end") {
    const auto types = mod_registry();
    Scene s;
    const std::size_t is = s.add_island(0, 100, 100, 64, 64);
    s.block(is, 110, 110);
    sim::World w = make_world(s, types);
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    const sim::BuildingId house = sim::make_building_id(sim::IslandId{0}, 1);
    for (int i = 0; i < 20000 && stats.residents.merged_delivered_by_goods.empty(); ++i) {
        w.advance(2);
        REQUIRE_MESSAGE(!w.faulted(), w.fault());
    }
    const sim::ResidentStats& r = stats.residents;
    REQUIRE_FALSE(r.merged_delivered_by_goods.empty());
    // The walker is one of the made-up figure types, bought the made-up goods at the made-up
    // supplier, and the house's stock rose at the made-up need slots.
    const auto fig = std::find_if(w.figures().list.slots.begin(), w.figures().list.slots.end(),
                                  [](const auto& slot) { return slot && slot->resident; });
    REQUIRE(fig != w.figures().list.slots.end());
    CHECK((*fig)->kind >= 7001);
    CHECK((*fig)->kind <= 7003);
    CHECK(r.purchases >= 1);
    for (const auto& [g, n] : r.bought_by_goods) {
        CHECK((g == kG29 || g == kG30));
        CHECK(r.taken_by_goods.at(g) == n);  // a stall: store taken = bought
        CHECK(r.billed_by_goods.at(g) == n); // billed = the requested amount
    }
    for (const auto& [g, n] : r.merged_delivered_by_goods) {
        CHECK((g == kG29 || g == kG30));
        CHECK(r.stock_primary_by_goods.at(g) == 16 * n); // 16 stock units per amount unit
    }
    CHECK(work(stats, house).kernel_calls == work(stats, house).visits);
    CHECK(work(stats, house).spawn_draws >= 1);
    CHECK(w.check_invariants() == "");
    w.set_transport_stats(nullptr);
}

TEST_CASE("population scale: unrelated content does not change a house's per-search work") {
    const auto types = mod_registry();
    // A: four blocks. B: the same four plus 36 more blocks of the same city far away and a whole
    // second island with 20 blocks (ten times the content).
    Scene a;
    {
        const std::size_t is = a.add_island(0, 100, 100, 256, 256);
        grid(a, is, 110, 110, 4, 2);
    }
    Scene b = a;
    {
        // 36 more blocks in the same city: the grid continues two columns to the right and rows
        // below
        for (std::uint32_t k = 0; k < 36; ++k) {
            b.block(0, 110 + 48 + 24 * static_cast<std::int32_t>(k % 6),
                    110 + 24 * static_cast<std::int32_t>(k / 6));
        }
        const std::size_t is2 = b.add_island(1, 360, 100, 140, 140);
        grid(b, is2, 370, 110, 20, 5);
    }
    REQUIRE(a.blocks == 4);
    REQUIRE(b.blocks == 60);
    const sim::BuildingId x = sim::make_building_id(sim::IslandId{0}, 1);
    const auto run_to_first_search = [&](const Scene& scene) {
        sim::World w = make_world(scene, types);
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        std::uint64_t ticks = 0;
        while (stats.population.houses[x].searches < 1 && ticks < 60000) {
            w.advance(2);
            ++ticks;
            REQUIRE_MESSAGE(!w.faulted(), w.fault());
        }
        w.set_transport_stats(nullptr);
        return std::pair{stats.population.houses[x], ticks};
    };
    const auto [wa, ticks_a] = run_to_first_search(a);
    const auto [wb, ticks_b] = run_to_first_search(b);
    std::cout << "first search of house 1: A at tick " << ticks_a << ", B at tick " << ticks_b
              << "\nA: rect " << wa.rect_queries << " hits " << wa.rect_hits << " scored "
              << wa.candidates_scored << " accepted " << wa.candidates_accepted << " walks "
              << wa.walk_searches << " expansions " << wa.walk_expansions << "\nB: rect "
              << wb.rect_queries << " hits " << wb.rect_hits << " scored " << wb.candidates_scored
              << " accepted " << wb.candidates_accepted << " walks " << wb.walk_searches
              << " expansions " << wb.walk_expansions << "\n";
    REQUIRE(wa.searches == 1);
    REQUIRE(wb.searches == 1);
    // The ten-fold content changes WHEN the house is visited, never what one search costs.
    CHECK(wa.rect_queries == 1);
    CHECK(wa.rect_queries == wb.rect_queries);
    CHECK(wa.rect_hits == wb.rect_hits);
    CHECK(wa.candidates_scored == wb.candidates_scored);
    CHECK(wa.candidates_accepted == wb.candidates_accepted);
    CHECK(wa.walk_searches == wb.walk_searches);
    CHECK(wa.walk_expansions == wb.walk_expansions);
    CHECK(wa.upgrade_scores == wb.upgrade_scores);
    // One candidate (its own stall) is all the square holds: no scan of the 59 other stalls.
    CHECK(wa.candidates_scored == 1);
    CHECK(wb.candidates_scored == 1);
    CHECK(wa.rect_hits == 1);
    // One kernel call per visit, whatever the size of the world.
    CHECK(wa.kernel_calls == wa.visits);
    CHECK(wb.kernel_calls == wb.visits);
}

TEST_CASE("population scale: total work grows linearly with the active houses") {
    const auto types = mod_registry();
    struct Row {
        std::uint32_t blocks = 0;
        sim::HouseWork total;
        double seconds = 0;
    };
    const auto measure = [&](std::uint32_t n) {
        Scene s;
        const std::size_t is = s.add_island(0, 100, 100, 256, 256);
        grid(s, is, 110, 110, n, 4);
        sim::World w = make_world(s, types);
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 240; ++i) { // 12 000 ticks: about four gates of every house
            w.advance(100);
            REQUIRE_MESSAGE(!w.faulted(), w.fault());
        }
        Row r;
        r.blocks = n;
        r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        r.total = stats.population.total();
        w.set_transport_stats(nullptr);
        return r;
    };
    const Row n8 = measure(8);
    const Row n16 = measure(16);
    const auto line = [](const Row& r) {
        std::cout << r.blocks << " houses: visits " << r.total.visits << " kernel "
                  << r.total.kernel_calls << " taken " << r.total.kernel_taken << " draws "
                  << r.total.spawn_draws << " searches " << r.total.searches << " rect "
                  << r.total.rect_queries << " scored " << r.total.candidates_scored << " walks "
                  << r.total.walk_searches << " expansions " << r.total.walk_expansions
                  << " scores " << r.total.upgrade_scores << " wall " << r.seconds << " s\n";
    };
    line(n8);
    line(n16);
    const auto ratio_ok = [](std::uint64_t base, std::uint64_t doubled) {
        // twice the houses, twice the work, within 10 percent (the visit phase differs a little)
        return doubled * 10 >= base * 2 * 9 && doubled * 10 <= base * 2 * 11;
    };
    CHECK(n8.total.visits > 100);
    // The building scheduler visits clamp(d * n / building_div + 1, n) slots per city visit (the
    // original's rule, SimConfig::building_div = 500): at this size the "+1" dominates, so VISITS
    // grow slower than the houses. The WORK a visit causes (gates taken, spawns, searches,
    // candidates, expansions) is what grows with the houses.
    CHECK(n16.total.visits <= 2 * n8.total.visits);
    CHECK(ratio_ok(n8.total.kernel_taken, n16.total.kernel_taken));
    CHECK(ratio_ok(n8.total.spawn_draws, n16.total.spawn_draws));
    CHECK(ratio_ok(n8.total.searches, n16.total.searches));
    CHECK(ratio_ok(n8.total.candidates_scored, n16.total.candidates_scored));
    CHECK(ratio_ok(n8.total.walk_expansions, n16.total.walk_expansions));
    // The per-visit and per-search work is constant: exactly one kernel call per visit, one
    // rectangle query per search, one candidate per search, and the expansions per search are
    // equal.
    for (const Row* r : {&n8, &n16}) {
        CHECK(r->total.kernel_calls == r->total.visits);
        CHECK(r->total.rect_queries == r->total.searches);
        CHECK(r->total.candidates_scored == r->total.searches);
        CHECK(r->total.walk_searches == r->total.searches);
    }
    CHECK(n8.total.walk_expansions * n16.total.searches ==
          n16.total.walk_expansions * n8.total.searches);
}

TEST_CASE("population scale: the stage pass counts every group in one pass over the city") {
    // residents_at_or_above_all (one pass, used by the stage pass) equals residents_at_or_above per
    // group (the definition), for houses of all three groups and a house of no residents.
    const auto types = mod_registry();
    Scene s;
    const std::size_t is = s.add_island(0, 100, 100, 128, 128);
    const struct {
        sim::TypeId type;
        std::int32_t res64;
        std::int32_t x;
    } houses[] = {{kHouse0, 130, 110},
                  {kHouse1, 640, 130},
                  {kHouse, 64 * 7 + 3, 150},
                  {kHouse, 0, 170},
                  {kHouse0, 64, 190}};
    for (const auto& h : houses) {
        sim::ScenarioSpec::BuildingSpec b;
        b.type = h.type;
        b.x = h.x;
        b.y = 110;
        sim::House p = house_payload();
        p.residents_64 = h.res64;
        b.payload = p;
        s.spec.islands[is].cities[0].buildings.push_back(b);
    }
    sim::World w = make_world(s, types);
    const sim::City& city = *w.islands()[0]->cities[0];
    const std::vector<std::int64_t> all =
        sim::residents_at_or_above_all(mod_config(), *types, city);
    REQUIRE(all.size() == 3);
    for (std::uint32_t g = 0; g < 3; ++g) {
        CHECK(all[g] == sim::residents_at_or_above(mod_config(), *types, city, g));
    }
    // trunc(130/64) + trunc(640/64) + trunc(451/64) + 0 + trunc(64/64) = 2 + 10 + 7 + 0 + 1
    CHECK(all[0] == 20);
    CHECK(all[1] == 17); // groups 1 and 2: 10 + 7 + 0
    CHECK(all[2] == 7);
}
