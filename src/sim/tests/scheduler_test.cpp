// The island phase of World::advance (docs/sim/state.md §5.4-§5.8, stage S7): the
// sweeps, the object manager, the city ring, the city tick, the building scheduler,
// the 300 s event with its drought, and the order of it all. The functions are
// called directly at a chosen tick where that isolates one rule, and through
// advance where the order is the point. Every expected number is derived from the
// formulas of the spec by hand (never from the code under test).
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/draw.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/island_tick.hpp>
#include <anno/sim/world.hpp>

#include "slice_helpers.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using namespace slice_test;
using building_test::Kit;
using building_test::make_kit;
using building_test::place;

namespace {

std::string num(std::uint64_t v) {
    return std::to_string(v);
}

/// Seams that write one line per call to the Log behind their ctx.
void install_fauna(sim::Seams& s, Log* log) {
    s.wildlife_sweep.name = "rec_fauna";
    s.wildlife_sweep.ctx = log;
    s.wildlife_sweep.fn = [](void* ctx, const sim::TickContext&, sim::Island&, std::uint32_t x,
                             std::uint32_t y, sim::Effects&) {
        static_cast<Log*>(ctx)->add("fauna " + num(x) + " " + num(y));
    };
}

void install_pass_city(sim::Seams& s, std::size_t index, Log* log) {
    s.city_pass[index].name = "rec_pass";
    s.city_pass[index].ctx = log;
    s.city_pass[index].fn = [](void* ctx, const sim::TickContext&, sim::Island&, sim::City& c,
                               sim::Effects&) {
        static_cast<Log*>(ctx)->add("city " + num(c.number.v));
    };
}

} // namespace

// ---------------------------------------------------------------- the sweeps

TEST_CASE("sweep cursor steps along a row and wraps at the ends of the row and the island") {
    struct Row {
        std::uint32_t x, y, w, h, edge, ex, ey;
    };
    const std::vector<Row> rows = {
        {0, 0, 64, 64, 8, 8, 0},
        {48, 0, 64, 64, 8, 56, 0},
        {56, 0, 64, 64, 8, 0, 8},
        {56, 48, 64, 64, 8, 0, 56},
        {56, 56, 64, 64, 8, 0, 0},
        // Islands whose size is not a multiple of the edge: the row ends at the first
        // x at or beyond the width.
        {16, 0, 30, 20, 8, 24, 0},
        {24, 0, 30, 20, 8, 0, 8},
        {24, 8, 30, 20, 8, 0, 16},
        {24, 16, 30, 20, 8, 0, 0},
        // x + edge equal to the width wraps (>=, not >) ...
        {24, 0, 32, 20, 8, 0, 8},
        // ... and so does y + edge equal to the height.
        {56, 8, 64, 16, 8, 0, 0},
        {56, 0, 64, 16, 8, 0, 8},
        {0, 0, 64, 16, 8, 8, 0},
        // Another edge.
        {0, 0, 40, 40, 16, 16, 0},
        {32, 0, 40, 40, 16, 0, 16},
        {32, 32, 40, 40, 16, 0, 0},
        // A hostile position near the end of u32 wraps instead of overflowing.
        {0xFFFFFFF0U, 0xFFFFFFF0U, 100, 100, 0x20, 0, 0},
    };
    for (const Row& r : rows) {
        INFO("from (" << r.x << ", " << r.y << ") in " << r.w << " x " << r.h << " edge "
                      << r.edge);
        std::uint32_t x = r.x;
        std::uint32_t y = r.y;
        sim::advance_sweep_cursor(x, y, r.w, r.h, r.edge);
        CHECK(x == r.ex);
        CHECK(y == r.ey);
    }
}

TEST_CASE("island tick sweeps visit min(chunks d / period + 1, chunks) chunks each") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    install_fauna(seams, &log);
    sim::World w = make_world_with(k, cities_scenario(1), seams);
    sim::Island& is = island(w);
    REQUIRE(is.tiles.chunk_count() == 64);
    sim::Effects fx;
    {
        // d = 1000 from the sweep stamp 0: growth 64 * 1000 / 2000 + 1 = 33 chunks, fauna
        // 64 * 1000 / 15000 + 1 = 5 chunks.
        Tick t(w, 1000);
        sim::island_tick(t.ctx, is, fx);
        CHECK(is.sweep.stamp == 1000);
        // The growth cursor is chunk 33 of 8 per row: (8, 32).
        CHECK(is.sweep.grow_x == 8);
        CHECK(is.sweep.grow_y == 32);
        // The fauna seam saw the first five chunks in row-major order; the cursor is
        // chunk 5.
        REQUIRE(log.lines.size() == 5);
        CHECK(log.lines[0] == "fauna 0 0");
        CHECK(log.lines[1] == "fauna 8 0");
        CHECK(log.lines[2] == "fauna 16 0");
        CHECK(log.lines[3] == "fauna 24 0");
        CHECK(log.lines[4] == "fauna 32 0");
        CHECK(is.sweep.fauna_x == 40);
        CHECK(is.sweep.fauna_y == 0);
    }
    log.lines.clear();
    {
        // d = 0: one chunk each (the + 1).
        Tick t(w, 1000);
        sim::island_tick(t.ctx, is, fx);
        CHECK(is.sweep.grow_x == 16);
        CHECK(is.sweep.grow_y == 32);
        REQUIRE(log.lines.size() == 1);
        CHECK(log.lines[0] == "fauna 40 0");
        CHECK(is.sweep.fauna_x == 48);
    }
    log.lines.clear();
    {
        // d = 65535: both counts are capped at the 64 chunks, which is one full lap,
        // so the cursors come back to where they were (without the cap the growth
        // sweep would make 2098 steps and the fauna sweep 280).
        Tick t(w, 1000 + 65535);
        sim::island_tick(t.ctx, is, fx);
        CHECK(log.lines.size() == 64);
        CHECK(is.sweep.grow_x == 16);
        CHECK(is.sweep.grow_y == 32);
        CHECK(is.sweep.fauna_x == 48);
        CHECK(is.sweep.fauna_y == 0);
        CHECK(is.sweep.stamp == static_cast<std::uint16_t>(1000 + 65535));
    }
}

TEST_CASE("island tick sweeps on an island that is not a multiple of the chunk edge") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    install_fauna(seams, &log);
    sim::World w = make_world_with(k, syn::two_island_scenario(), seams);
    sim::Island& is = island(w, 3); // 30 x 20: 4 x 3 chunks
    REQUIRE(is.tiles.chunk_count() == 12);
    sim::Effects fx;
    Tick t(w, 1000);
    sim::island_tick(t.ctx, is, fx);
    // Growth: 12 * 1000 / 2000 + 1 = 7 chunks, the 8th chunk is row 1, column 3.
    CHECK(is.sweep.grow_x == 24);
    CHECK(is.sweep.grow_y == 8);
    // Fauna: 12 * 1000 / 15000 + 1 = 1 chunk.
    CHECK(is.sweep.fauna_x == 8);
    CHECK(is.sweep.fauna_y == 0);
    // The other island was not touched by this call.
    CHECK(island(w, 0).sweep.grow_x == 0);
}

TEST_CASE("sweep periods are configuration") {
    const Kit k = make_kit();
    sim::SimConfig cfg;
    cfg.grow_period = 100;
    cfg.fauna_period = 400;
    cfg.chunk_edge = 16; // 4 x 4 chunks
    sim::World w = make_world_with(k, cities_scenario(1), named_seams(), cfg);
    sim::Island& is = island(w);
    REQUIRE(is.tiles.chunk_count() == 16);
    sim::Effects fx;
    Tick t(w, 50);
    sim::island_tick(t.ctx, is, fx);
    // Growth: 16 * 50 / 100 + 1 = 9 chunks of 4 per row: column 1, row 2, in tiles
    // (16, 32); fauna: 16 * 50 / 400 + 1 = 3 chunks: (48, 0).
    CHECK(is.sweep.grow_x == 16);
    CHECK(is.sweep.grow_y == 32);
    CHECK(is.sweep.fauna_x == 48);
    CHECK(is.sweep.fauna_y == 0);
}

TEST_CASE("zero chunk island still runs the ring the 300 s event and the stamps") {
    // A1: only the two sweeps depend on the chunk count; with zero chunks both clamp
    // to 0 and everything after them still runs.
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    install_fauna(seams, &log);
    install_pass_city(seams, 0, &log);
    sim::World w = make_world_with(k, cities_scenario(1), seams);
    sim::Island& is = island(w);
    REQUIRE(is.tiles.reset(100, 100, 0, 0, 8));
    REQUIRE(is.tiles.chunk_count() == 0);
    sim::Effects fx;
    {
        Tick t(w, 1000);
        sim::island_tick(t.ctx, is, fx);
        CHECK(is.sweep.grow_x == 0);
        CHECK(is.sweep.fauna_x == 0);
        CHECK(log.count("fauna") == 0);
        CHECK(log.count("city 0") == 1); // the ring ran
        CHECK(is.ring.stamp == 1000);
        CHECK(is.objmgr.stamp == 1000);
        CHECK(is.sweep.stamp == 1000);
        CHECK(t.draw.counts[static_cast<std::size_t>(sim::DrawSite::Event300)] == 0);
    }
    {
        // The 300 s event gate (game second 300) opens: its draw is made.
        Tick t(w, 150000);
        sim::island_tick(t.ctx, is, fx);
        CHECK(t.draw.counts[static_cast<std::size_t>(sim::DrawSite::Event300)] == 1);
        CHECK(is.event300_stamp == 300);
    }
}

// ------------------------------------------------------------ object manager

namespace {

sim::ScenarioSpec deposits_scenario(std::size_t n) {
    sim::ScenarioSpec spec = cities_scenario(1);
    spec.islands[0].deposits.clear();
    for (std::size_t i = 0; i < n; ++i) {
        sim::ScenarioSpec::DepositSpec d;
        d.goods = "EISENERZ";
        d.x = 110 + static_cast<std::int32_t>(i);
        d.y = 120;
        spec.islands[0].deposits.push_back(d);
    }
    return spec;
}

void install_objmgr(sim::Seams& s, Log* log) {
    s.object_manager.name = "rec_objmgr";
    s.object_manager.ctx = log;
    s.object_manager.fn = [](void* ctx, const sim::TickContext&, sim::Island& is, sim::Effects&) {
        static_cast<Log*>(ctx)->add("objmgr " + num(is.objmgr.cursor));
    };
}

} // namespace

TEST_CASE("object manager stand-in on empty deposits") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    install_objmgr(seams, &log);
    sim::World w = make_world_with(k, deposits_scenario(0), seams);
    sim::Island& is = island(w);
    REQUIRE(is.deposits.empty());
    sim::Effects fx;
    Tick t(w, 4000);
    sim::object_manager_tick(t.ctx, is, fx);
    // Nothing is visited (the original's do-while is open, O-12), the cursor stays and
    // the stamp is written.
    CHECK(log.lines.empty());
    CHECK(is.objmgr.cursor == 0);
    CHECK(is.objmgr.stamp == 4000);
}

TEST_CASE("object manager visits min(d n / 5000 + 1, n) deposits from its cursor") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    install_objmgr(seams, &log);
    sim::World w = make_world_with(k, deposits_scenario(3), seams);
    sim::Island& is = island(w);
    sim::Effects fx;
    {
        // d = 1700: 1700 * 3 / 5000 = 1, + 1 = 2 visits, the seam sees cursors 0 and 1.
        Tick t(w, 1700);
        sim::object_manager_tick(t.ctx, is, fx);
        REQUIRE(log.lines.size() == 2);
        CHECK(log.lines[0] == "objmgr 0");
        CHECK(log.lines[1] == "objmgr 1");
        CHECK(is.objmgr.cursor == 2);
        CHECK(is.objmgr.stamp == 1700);
    }
    log.lines.clear();
    {
        // d = 4000: 4000 * 3 / 5000 = 2, + 1 = 3 visits from cursor 2: 2, 0, 1.
        Tick t(w, 5700);
        sim::object_manager_tick(t.ctx, is, fx);
        REQUIRE(log.lines.size() == 3);
        CHECK(log.lines[0] == "objmgr 2");
        CHECK(log.lines[1] == "objmgr 0");
        CHECK(log.lines[2] == "objmgr 1");
        CHECK(is.objmgr.cursor == 2);
    }
    log.lines.clear();
    {
        // d = 0: one visit.
        Tick t(w, 5700);
        sim::object_manager_tick(t.ctx, is, fx);
        CHECK(log.lines.size() == 1);
        CHECK(is.objmgr.cursor == 0);
    }
    log.lines.clear();
    {
        // d large: capped at the 3 deposits.
        Tick t(w, 5700 + 60000);
        sim::object_manager_tick(t.ctx, is, fx);
        CHECK(log.lines.size() == 3);
        CHECK(is.objmgr.cursor == 0);
    }
}

TEST_CASE("object manager divisor is configuration and a null seam is skipped") {
    const Kit k = make_kit();
    sim::SimConfig cfg;
    cfg.objmgr_div = 100;
    sim::World w = make_world_with(k, deposits_scenario(4), named_seams(), cfg);
    sim::Island& is = island(w);
    sim::Effects fx;
    // d = 60: 60 * 4 / 100 = 2, + 1 = 3 visits: the cursor moves with no seam installed.
    Tick t(w, 60);
    sim::object_manager_tick(t.ctx, is, fx);
    CHECK(is.objmgr.cursor == 3);
    CHECK(is.objmgr.stamp == 60);
}

// ------------------------------------------------------------------ city ring

TEST_CASE("city ring visits min(d slots / 50 + 1, slots) slots from its cursor") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    install_pass_city(seams, 0, &log);
    // Cities 0 .. 14; slot 15 is the neutral owner and holds no city.
    sim::World w = make_world_with(k, cities_scenario(15), seams);
    sim::Island& is = island(w);
    REQUIRE(is.cities.size() == 16);
    REQUIRE_FALSE(is.cities[15].has_value());
    sim::Effects fx;
    struct Case {
        std::uint32_t d;
        std::size_t visits;
    };
    // d * 16 / 50 + 1, capped at 16: d 0 -> 1, 3 -> 1 (48/50 = 0), 4 -> 2 (64/50 = 1),
    // 5 -> 2, 49 -> 16 (784/50 = 15), 50 -> 16 (800/50 = 16, + 1 = 17, capped).
    for (const Case& c : std::vector<Case>{{0, 1}, {3, 1}, {4, 2}, {5, 2}, {49, 16}, {50, 16}}) {
        INFO("d " << c.d);
        is.ring.stamp = 0;
        is.ring.cursor = 0;
        log.lines.clear();
        Tick t(w, c.d);
        sim::city_ring_tick(t.ctx, is, fx);
        // Slot 15 is empty and visited without a call.
        const std::size_t calls = c.visits > 15 ? 15 : c.visits;
        REQUIRE(log.lines.size() == calls);
        for (std::size_t i = 0; i < calls; ++i) {
            CHECK(log.lines[i] == "city " + num(i));
        }
        CHECK(is.ring.cursor == c.visits % 16);
        CHECK(is.ring.stamp == c.d);
    }
}

TEST_CASE("city ring cursor wraps and skips empty slots") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    install_pass_city(seams, 0, &log);
    sim::World w = make_world_with(k, cities_scenario(15), seams);
    sim::Island& is = island(w);
    sim::Effects fx;
    // From slot 14: d = 20 gives 20 * 16 / 50 + 1 = 7 visits: slots 14, 15 (empty), 0, 1,
    // 2, 3, 4.
    is.ring.cursor = 14;
    Tick t(w, 20);
    sim::city_ring_tick(t.ctx, is, fx);
    REQUIRE(log.lines.size() == 6);
    CHECK(log.lines[0] == "city 14");
    CHECK(log.lines[1] == "city 0");
    CHECK(log.lines[5] == "city 4");
    CHECK(is.ring.cursor == 5);
    // Each visited city got its stamp, an unvisited one did not.
    CHECK(city(w, 14).tick_stamp == 20);
    CHECK(city(w, 4).tick_stamp == 20);
    CHECK(city(w, 5).tick_stamp == 0);
}

TEST_CASE("city ring divisor and slot count are configuration") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    install_pass_city(seams, 0, &log);
    sim::SimConfig cfg;
    cfg.city_ring_div = 10;
    cfg.city_slots = 8;
    cfg.neutral_owner = 7;
    sim::World w = make_world_with(k, cities_scenario(7), seams, cfg);
    sim::Island& is = island(w);
    REQUIRE(is.cities.size() == 8);
    sim::Effects fx;
    // d = 5: 5 * 8 / 10 + 1 = 5 visits of the 8 slots.
    {
        Tick t(w, 5);
        sim::city_ring_tick(t.ctx, is, fx);
        CHECK(log.lines.size() == 5);
        CHECK(is.ring.cursor == 5);
    }
    // The cursor wraps at the configured slot count (8), not at 16: from slot 6, five
    // visits are 6, 7 (no city there), 0, 1, 2.
    log.lines.clear();
    is.ring.cursor = 6;
    is.ring.stamp = 0;
    {
        Tick t(w, 5);
        sim::city_ring_tick(t.ctx, is, fx);
        REQUIRE(log.lines.size() == 4);
        CHECK(log.lines[0] == "city 6");
        CHECK(log.lines[1] == "city 0");
        CHECK(log.lines[3] == "city 2");
        CHECK(is.ring.cursor == 3);
    }
}

TEST_CASE("city ring with a cursor outside the table visits nothing and writes the stamp") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    install_pass_city(seams, 0, &log);
    sim::World w = make_world_with(k, cities_scenario(3), seams);
    sim::Island& is = island(w);
    is.ring.cursor = 40; // inconsistent (A8); check_invariants reports it
    sim::Effects fx;
    Tick t(w, 500);
    sim::city_ring_tick(t.ctx, is, fx);
    CHECK(log.lines.empty());
    CHECK(is.ring.cursor == 40);
    CHECK(is.ring.stamp == 500);
}

// ------------------------------------------------ city tick, building scheduler

TEST_CASE("city tick runs the buildings, then the eight passes in order, then the stamp") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    for (std::size_t i = 0; i < 8; ++i) {
        seams.city_pass[i].name = "rec_pass";
        seams.city_pass[i].ctx = &log;
        seams.city_pass[i].fn = [](void* ctx, const sim::TickContext&, sim::Island&, sim::City& c,
                                   sim::Effects&) {
            // The pass index is not passed: the buildings' output and the stamps tell
            // the order of the phases.
            const auto& slot = c.index.lists[4].slots[0];
            const auto& w = std::get<sim::Workshop>(slot->payload);
            static_cast<Log*>(ctx)->add("pass out=" + num(w.out[0]) + " index_stamp=" +
                                        num(c.index.stamp) + " tick_stamp=" + num(c.tick_stamp));
        };
    }
    sim::ScenarioSpec spec = cities_scenario(1);
    spec.islands[0].cities[0].buildings = {weaver_at(k, 101, 101, 0)};
    sim::World w = make_world_with(k, spec, seams);
    sim::Island& is = island(w);
    sim::Effects fx;
    // t = 500: d = 500 from the index stamp 0 gives min(500 * 1 / 500 + 1, 1) = 1 visit,
    // the weaver's gate is open (last 0), it makes 6 and uses 11 of the wool.
    Tick t(w, 500);
    sim::city_tick(t.ctx, is, city(w, 0), fx);
    REQUIRE(log.lines.size() == 8);
    for (const std::string& line : log.lines) {
        CHECK(line == "pass out=6 index_stamp=500 tick_stamp=0");
    }
    CHECK(city(w, 0).tick_stamp == 500);
    CHECK(city(w, 0).index.stamp == 500);
}

TEST_CASE("city passes are called in their original order with their own contexts") {
    const Kit k = make_kit();
    Log a, b;
    sim::Seams seams = named_seams();
    seams.city_pass[2].name = "rec_two";
    seams.city_pass[2].ctx = &a;
    seams.city_pass[2].fn = [](void* ctx, const sim::TickContext&, sim::Island&, sim::City&,
                               sim::Effects&) { static_cast<Log*>(ctx)->add("two"); };
    seams.city_pass[5].name = "rec_five";
    seams.city_pass[5].ctx = &b;
    seams.city_pass[5].fn = [](void* ctx, const sim::TickContext&, sim::Island&, sim::City&,
                               sim::Effects&) { static_cast<Log*>(ctx)->add("five"); };
    sim::World w = make_world_with(k, cities_scenario(1), seams);
    sim::Effects fx;
    Tick t(w, 10);
    sim::city_tick(t.ctx, island(w), city(w, 0), fx);
    CHECK(a.lines == std::vector<std::string>{"two"});
    CHECK(b.lines == std::vector<std::string>{"five"});
}

TEST_CASE(
    "building scheduler gives every scheduled list the same visit count and one shared stamp") {
    const Kit k = make_kit();
    sim::SimConfig cfg;
    cfg.list_initial_size = 10; // every list of the city starts with 10 null slots
    sim::World w = make_world_with(k, cities_scenario(1), named_seams(), cfg);
    sim::Island& is = island(w);
    sim::City& c = city(w, 0);
    sim::Effects fx;
    {
        // d = 100: 100 * 10 / 500 + 1 = 3 visits in each of the lists 5, 1, 4, 3, 2.
        Tick t(w, 100);
        sim::building_scheduler(t.ctx, is, c, fx);
        const std::vector<std::uint32_t> want = {0, 3, 3, 3, 3, 3}; // list 0 is the stalls
        for (std::size_t l = 0; l < 6; ++l) {
            INFO("list " << l);
            CHECK(c.index.lists[l].cursor == want[l]);
        }
        CHECK(c.index.stamp == 100);
    }
    {
        // d = 30: 30 * 10 / 500 + 1 = 1 visit in each list, from the cursor each list kept.
        Tick t(w, 130);
        sim::building_scheduler(t.ctx, is, c, fx);
        const std::vector<std::uint32_t> want = {0, 4, 4, 4, 4, 4};
        for (std::size_t l = 0; l < 6; ++l) {
            INFO("list " << l);
            CHECK(c.index.lists[l].cursor == want[l]);
        }
        CHECK(c.index.stamp == 130);
    }
    // The 16-bit stamp: d is the difference modulo 2^16.
    {
        c.index.stamp = 65530;
        for (auto& l : c.index.lists) {
            l.cursor = 0;
        }
        Tick t(w, 65536 + 20); // d = 26: 26 * 10 / 500 + 1 = 1
        sim::building_scheduler(t.ctx, is, c, fx);
        CHECK(c.index.lists[4].cursor == 1);
        CHECK(c.index.stamp == 20);
    }
}

TEST_CASE("building scheduler divisor is configuration") {
    const Kit k = make_kit();
    sim::SimConfig cfg;
    cfg.list_initial_size = 10;
    cfg.building_div = 50;
    sim::World w = make_world_with(k, cities_scenario(1), named_seams(), cfg);
    sim::Effects fx;
    Tick t(w, 12); // 12 * 10 / 50 + 1 = 3
    sim::building_scheduler(t.ctx, island(w), city(w, 0), fx);
    CHECK(city(w, 0).index.lists[4].cursor == 3);
}

TEST_CASE("building scheduler visits holes without ticking them and ticks the rest") {
    const Kit k = make_kit();
    sim::ScenarioSpec spec = cities_scenario(1);
    // last 65036: at tick 100 the 16-bit difference (100 - 65036) mod 65536 is 600,
    // which is at least the 500 of the gate, so a visited weaver steps.
    for (int i = 0; i < 4; ++i) {
        spec.islands[0].cities[0].buildings.push_back(weaver_at(k, 101 + i, 101, 65036));
    }
    sim::World w = make_world_with(k, spec, named_seams());
    sim::City& c = city(w, 0);
    REQUIRE(sim::WorldTestAccess::remove(w, {{sim::IslandId{0}, sim::CityNo{0}}, 4, 1}));
    REQUIRE(sim::WorldTestAccess::remove(w, {{sim::IslandId{0}, sim::CityNo{0}}, 4, 2}));
    REQUIRE(c.index.lists[4].slots.size() == 10);
    auto last_of = [&](std::size_t slot) {
        return std::get<sim::Workshop>(c.index.lists[4].slots[slot]->payload).last;
    };
    sim::Effects fx;
    {
        // d = 100: 100 * 10 / 500 + 1 = 3 visits: slot 0 (a weaver, steps), slots 1 and 2
        // (holes).
        Tick t(w, 100);
        sim::building_scheduler(t.ctx, island(w), c, fx);
        CHECK(c.index.lists[4].cursor == 3);
        CHECK(last_of(0) == static_cast<std::uint16_t>(65036 + 500));
        CHECK(last_of(3) == 65036); // not reached yet
    }
    {
        // d = 40: one visit: slot 3.
        Tick t(w, 140);
        sim::building_scheduler(t.ctx, island(w), c, fx);
        CHECK(c.index.lists[4].cursor == 4);
        CHECK(last_of(3) == static_cast<std::uint16_t>(65036 + 500));
    }
    // The wool was used by exactly the two steps.
    CHECK(std::get<sim::Workshop>(c.index.lists[4].slots[0]->payload).in[0] == 5 * sim::kTon - 11);
}

TEST_CASE("building scheduler covers a list of 40000 slots without a 32 bit product") {
    const Kit k = make_kit();
    sim::ScenarioSpec spec = cities_scenario(1);
    spec.islands[0].cities[0].buildings = {weaver_at(k, 101, 101, 0)};
    sim::World w = make_world_with(k, spec, named_seams());
    sim::City& c = city(w, 0);
    auto& slots = production_slots(c);
    slots.resize(40000);
    slots[20000] = slots[0];
    slots[39999] = slots[0];
    auto last_of = [&](std::size_t slot) {
        return std::get<sim::Workshop>(slots[slot]->payload).last;
    };
    sim::Effects fx;
    {
        // d = 60000 and n = 40000: the product 2.4e9 is beyond 2^31 (the original's signed
        // 32-bit product is negative from d = 53688), the widened count is the full
        // list. All three weavers (gate open since last 0) step; the cursor lapped to 0.
        Tick t(w, 60000);
        sim::building_scheduler(t.ctx, island(w), c, fx);
        CHECK(last_of(0) == 500);
        CHECK(last_of(20000) == 500);
        CHECK(last_of(39999) == 500);
        CHECK(c.index.lists[4].cursor == 0);
        CHECK(c.index.stamp == 60000);
    }
    {
        // d = 30: 30 * 40000 / 500 + 1 = 2401 visits: slot 0 yes, slot 20000 no.
        std::get<sim::Workshop>(slots[0]->payload).last = 0;
        slots[20000]->payload = slots[0]->payload;
        Tick t(w, 60030);
        sim::building_scheduler(t.ctx, island(w), c, fx);
        CHECK(c.index.lists[4].cursor == 2401);
        CHECK(last_of(0) == 500);   // stepped again (60030 - 0 >= 500 in 16 bits)
        CHECK(last_of(20000) == 0); // not reached
    }
}

TEST_CASE("building scheduler never visits the list outside the schedule") {
    const Kit k = make_kit();
    // A layout that puts the production block into the stall list 0, which the
    // schedule {5, 1, 4, 3, 2} leaves out: a workshop there never steps.
    sim::SimConfig cfg;
    cfg.layout.block_to_list = {0, 5, 1, 3, 2, 4};
    sim::ScenarioSpec spec = cities_scenario(1);
    spec.islands[0].cities[0].buildings = {weaver_at(k, 101, 101, 0)};
    sim::World w = make_world_with(k, spec, named_seams(), cfg);
    sim::City& c = city(w, 0);
    REQUIRE(c.index.lists[0].slots[0].has_value()); // it sits in list 0
    sim::Effects fx;
    Tick t(w, 600);
    sim::building_scheduler(t.ctx, island(w), c, fx);
    const auto& wk = std::get<sim::Workshop>(c.index.lists[0].slots[0]->payload);
    CHECK(wk.last == 0);
    CHECK(wk.in[0] == 5 * sim::kTon);
    CHECK(c.index.lists[0].cursor == 0);
    // The same building in the production list steps.
    sim::World normal = make_world_with(k, spec, named_seams());
    Tick t2(normal, 600);
    sim::building_scheduler(t2.ctx, island(normal), city(normal, 0), fx);
    CHECK(std::get<sim::Workshop>(city(normal, 0).index.lists[4].slots[0]->payload).last == 500);
}

TEST_CASE("building scheduler visits the lists in the order of the schedule") {
    const Kit k = make_kit();
    // One PRODTYPE per block: the weaver (1) is in list 4, the farm (2) in list 5, so the
    // order of the schedule decides which of the two ticks first and therefore which
    // spawn request comes first.
    auto spawn_order = [&](std::vector<sim::ListId> schedule) {
        sim::SimConfig cfg;
        cfg.layout.block_size = 1;
        cfg.layout.block_to_list = {4, 5};
        cfg.layout.schedule = std::move(schedule);
        sim::ScenarioSpec spec = cities_scenario(1);
        sim::ScenarioSpec::BuildingSpec farm = place(k.farm, 103, 101);
        sim::Farm f;
        f.last = 65036;
        farm.payload = f;
        spec.islands[0].cities[0].buildings = {weaver_at(k, 101, 101, 65036), farm};
        sim::World w = make_world_with(k, spec, named_seams(), cfg);
        REQUIRE(city(w, 0).index.lists[4].slots[0].has_value());
        REQUIRE(city(w, 0).index.lists[5].slots[0].has_value());
        std::get<sim::Workshop>(city(w, 0).index.lists[4].slots[0]->payload).in[0] = 0;
        sim::Effects fx;
        Tick t(w, 100);
        sim::building_scheduler(t.ctx, island(w), city(w, 0), fx);
        std::vector<sim::BuildingId> order;
        for (const sim::SpawnRequest& s : fx.spawns) {
            order.push_back(s.home);
        }
        return order;
    };
    const sim::BuildingId weaver = sim::make_building_id(sim::IslandId{0}, 1);
    const sim::BuildingId farm = sim::make_building_id(sim::IslandId{0}, 2);
    CHECK(spawn_order({5, 1, 4, 3, 2}) == std::vector<sim::BuildingId>{farm, weaver});
    CHECK(spawn_order({4, 5, 1, 3, 2}) == std::vector<sim::BuildingId>{weaver, farm});
    CHECK(spawn_order({4}) == std::vector<sim::BuildingId>{weaver});
}

TEST_CASE("building scheduler follows a modded schedule") {
    const Kit k = make_kit();
    sim::SimConfig cfg;
    cfg.list_initial_size = 10;
    cfg.layout.schedule = {1, 0}; // the public list and the stalls only
    sim::World w = make_world_with(k, cities_scenario(1), named_seams(), cfg);
    sim::City& c = city(w, 0);
    sim::Effects fx;
    Tick t(w, 100);
    sim::building_scheduler(t.ctx, island(w), c, fx);
    CHECK(c.index.lists[1].cursor == 3);
    CHECK(c.index.lists[0].cursor == 3);
    CHECK(c.index.lists[4].cursor == 0);
    CHECK(c.index.lists[5].cursor == 0);
    CHECK(c.index.stamp == 100);
}

// ------------------------------------------------------------- the 300 s event

namespace {

/// A scenario for the 300 s event: the island of cities_scenario(cities) at climate
/// `climate`, a custom PRNG table that gives `draws` in order after the seed cursor 0.
sim::ScenarioSpec event_scenario(const Kit& k, std::uint32_t cities, std::uint32_t climate,
                                 std::vector<std::uint16_t> table) {
    static_cast<void>(k);
    sim::ScenarioSpec spec = cities_scenario(cities);
    spec.islands[0].climate = climate;
    spec.rng_table = std::move(table);
    spec.seed = 0;
    return spec;
}

std::uint32_t event_draws(const Tick& t) {
    return t.draw.counts[static_cast<std::size_t>(sim::DrawSite::Event300)];
}

} // namespace

TEST_CASE("300 s event waits for the gate and leaves the stamp alone before it") {
    const Kit k = make_kit();
    // A table whose next draw is 0 (a hit if the other tests pass).
    sim::World w = make_world_with(k, event_scenario(k, 1, 2, {5, 0, 0, 0}), named_seams());
    sim::Island& is = island(w);
    sim::Effects fx;
    {
        // game second 299 (t = 149999): u16(299 - 0) < 300, no draw, the stamp stays.
        Tick t(w, 149999);
        sim::event_300s(t.ctx, is, fx);
        CHECK(event_draws(t) == 0);
        CHECK(is.event300_stamp == 0);
        CHECK_FALSE(is.drought.has_value());
    }
    {
        // second 300 (t = 150000): the gate opens, exactly one draw.
        Tick t(w, 150000);
        sim::event_300s(t.ctx, is, fx);
        CHECK(event_draws(t) == 1);
        CHECK(is.event300_stamp == 300);
    }
    {
        // The next gate is 300 seconds later: 299 s after the stamp nothing happens, at
        // 300 s again one draw.
        Tick t(w, 150000 + 149999);
        sim::event_300s(t.ctx, is, fx);
        CHECK(event_draws(t) == 0);
        CHECK(is.event300_stamp == 300);
        Tick t2(w, 300000);
        sim::event_300s(t2.ctx, is, fx);
        CHECK(event_draws(t2) == 1);
        CHECK(is.event300_stamp == 600);
    }
}

TEST_CASE("300 s event gate is a 16 bit difference of game seconds") {
    const Kit k = make_kit();
    sim::World w = make_world_with(k, event_scenario(k, 1, 2, {5, 7, 7, 7}), named_seams());
    sim::Island& is = island(w);
    is.event300_stamp = 65500;
    sim::Effects fx;
    // Game second 131100 = 2 * 65536 + 28 against the stamp 65500: the 32-bit difference
    // is 65600, the 16-bit one 64, so the gate is closed ...
    Tick t(w, 131100U * 500U);
    sim::event_300s(t.ctx, is, fx);
    CHECK(event_draws(t) == 0);
    CHECK(is.event300_stamp == 65500);
    // ... and it opens at second 131336 (difference 300 mod 2^16), where the stamp
    // becomes the 16-bit second 264.
    Tick t2(w, 131336U * 500U);
    sim::event_300s(t2.ctx, is, fx);
    CHECK(event_draws(t2) == 1);
    CHECK(is.event300_stamp == 264);
    // One second earlier it is still closed (difference 299).
    is.event300_stamp = 65500;
    Tick t3(w, 131335U * 500U);
    sim::event_300s(t3.ctx, is, fx);
    CHECK(event_draws(t3) == 0);
}

TEST_CASE("300 s event gate and modulus are configuration") {
    const Kit k = make_kit();
    sim::SimConfig cfg;
    cfg.event300_gate_seconds = 10;
    cfg.event300_modulus = 3;
    for (const std::uint16_t draw : std::vector<std::uint16_t>{9, 10, 12, 20}) {
        INFO("draw " << draw);
        sim::ScenarioSpec spec = event_scenario(k, 1, 2, {1, draw, 0, 0});
        spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 110, 110)};
        sim::World w = make_world_with(k, spec, named_seams(), cfg);
        sim::Island& is = island(w);
        sim::Effects fx;
        Tick closed(w, 9 * 500);
        sim::event_300s(closed.ctx, is, fx);
        CHECK(event_draws(closed) == 0);
        Tick open(w, 10 * 500);
        sim::event_300s(open.ctx, is, fx);
        CHECK(event_draws(open) == 1);
        CHECK(is.event300_stamp == 10);
        // A drought exactly when the draw is a multiple of the modulus 3.
        CHECK(is.drought.has_value() == (draw % 3 == 0));
    }
}

TEST_CASE("300 s event with property 6 bit 0x04 makes no draw and writes the stamp") {
    const Kit k = make_kit();
    sim::World w = make_world_with(k, event_scenario(k, 1, 2, {5, 0, 0, 0}), named_seams());
    sim::WorldTestAccess::settings(w).prop6 = 0x04;
    sim::Island& is = island(w);
    sim::Effects fx;
    Tick t(w, 150000);
    sim::event_300s(t.ctx, is, fx);
    CHECK(event_draws(t) == 0);
    CHECK(w.rng().cursor() == 0);
    CHECK(is.event300_stamp == 300);
    CHECK_FALSE(is.drought.has_value());
    // Other bits of property 6 do not suppress it (0x02 volcano, 0x08 city events).
    sim::WorldTestAccess::settings(w).prop6 = 0x0b;
    is.event300_stamp = 0;
    Tick t2(w, 150000);
    sim::event_300s(t2.ctx, is, fx);
    CHECK(event_draws(t2) == 1);
}

TEST_CASE("300 s event draws once even when the climate or the cities rule out a drought") {
    const Kit k = make_kit();
    // The draw is 0 (a hit by the modulus): only the climate and the cities can stop it.
    for (const std::uint32_t climate : {0U, 2U}) {
        for (const std::uint32_t cities : {0U, 1U}) {
            INFO("climate " << climate << " cities " << cities);
            sim::ScenarioSpec spec = event_scenario(k, cities, climate, {5, 0, 0, 0});
            if (cities > 0) {
                spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 110, 110)};
            }
            sim::World w = make_world_with(k, spec, named_seams());
            sim::Island& is = island(w);
            sim::Effects fx;
            Tick t(w, 150000);
            sim::event_300s(t.ctx, is, fx);
            CHECK(event_draws(t) == 1);
            CHECK(w.rng().cursor() == 1);
            CHECK(is.event300_stamp == 300);
            CHECK(is.drought.has_value() == (climate != 0 && cities != 0));
        }
    }
}

TEST_CASE("300 s event starts a drought only on a draw divisible by the modulus") {
    const Kit k = make_kit();
    for (const std::uint16_t draw : std::vector<std::uint16_t>{0, 1, 19, 20, 21, 40, 41}) {
        INFO("draw " << draw);
        sim::ScenarioSpec spec = event_scenario(k, 1, 2, {5, draw, 0, 0});
        spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 110, 110)};
        sim::World w = make_world_with(k, spec, named_seams());
        sim::Island& is = island(w);
        sim::Effects fx;
        Tick t(w, 150000);
        sim::event_300s(t.ctx, is, fx);
        CHECK(is.drought.has_value() == (draw % 20 == 0));
        CHECK(is.event300_stamp == 300);
    }
}

TEST_CASE(
    "300 s event centres the drought on the first market building floored to the chunk edge") {
    const Kit k = make_kit();
    sim::ScenarioSpec spec = event_scenario(k, 2, 4, {5, 0, 0, 0});
    // City 0 has none; city 1 has two Kontors: the first by slot order is at (121, 133).
    spec.islands[0].cities[1].buildings = {building_test::place(k.kontor, 121, 133),
                                           building_test::place(k.kontor, 105, 105)};
    sim::World w = make_world_with(k, spec, named_seams());
    sim::Island& is = island(w);
    sim::Effects fx;
    Tick t(w, 150000);
    sim::event_300s(t.ctx, is, fx);
    REQUIRE(is.drought.has_value());
    const sim::Drought& d = *is.drought;
    CHECK(d.x == 120);         // 121 - 121 % 8
    CHECK(d.y == 128);         // 133 - 133 % 8
    CHECK(d.target_ring == 6); // 0x32 / 8
    CHECK(d.ring == 0);
    CHECK(d.city_mask == std::vector<std::uint32_t>{0});
    CHECK(d.phase == 0);
    CHECK(d.step == 0);
    CHECK(d.state == 1);
    CHECK(d.counter == 0);
    CHECK(d.stamp == 150000 % 65536);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("300 s event drought centre and radius follow the chunk edge") {
    const Kit k = make_kit();
    sim::SimConfig cfg;
    cfg.chunk_edge = 16;
    sim::ScenarioSpec spec = event_scenario(k, 1, 3, {5, 0, 0, 0});
    spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 121, 133)};
    sim::World w = make_world_with(k, spec, named_seams(), cfg);
    sim::Island& is = island(w);
    sim::Effects fx;
    Tick t(w, 150000);
    sim::event_300s(t.ctx, is, fx);
    REQUIRE(is.drought.has_value());
    CHECK(is.drought->x == 112);         // 121 - 121 % 16
    CHECK(is.drought->y == 128);         // 133 - 133 % 16
    CHECK(is.drought->target_ring == 3); // 0x32 / 16
}

TEST_CASE("the random drought centre rule is reserved and the faithful centre stays the default") {
    // OQ-042: the spec does not define the alternative centre, so the option is rejected
    // (like the other reserved values) instead of being accepted and doing nothing. A world
    // cannot be created with it, and the default rules give the faithful centre and one draw.
    const Kit k = make_kit();
    sim::Rules rules;
    CHECK_FALSE(rules.drought_random_centre);
    CHECK(rules.validate() == "");
    rules.drought_random_centre = true;
    CHECK(rules.validate() == "Rules.drought_random_centre: reserved: not implemented (OQ-042)");
    sim::ScenarioSpec spec = event_scenario(k, 1, 3, {5, 0, 0, 0});
    spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 121, 133)};
    auto refused = sim::World::create(spec, k.registry, sim::SimConfig{}, rules, named_seams());
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().find("drought_random_centre") != std::string::npos);
    sim::World w = make_world_with(k, spec, named_seams());
    sim::Effects fx;
    Tick t(w, 150000);
    sim::event_300s(t.ctx, island(w), fx);
    REQUIRE(island(w).drought.has_value());
    CHECK(island(w).drought->x == 120);
    CHECK(island(w).drought->y == 128);
    CHECK(event_draws(t) == 1);
}

TEST_CASE("300 s event drought radius is configuration") {
    const Kit k = make_kit();
    sim::SimConfig cfg;
    cfg.drought_radius = 100;
    sim::ScenarioSpec spec = event_scenario(k, 1, 3, {5, 0, 0, 0});
    spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 121, 133)};
    sim::World w = make_world_with(k, spec, named_seams(), cfg);
    sim::Effects fx;
    Tick t(w, 150000);
    sim::event_300s(t.ctx, island(w), fx);
    REQUIRE(island(w).drought.has_value());
    CHECK(island(w).drought->target_ring == 12); // 100 / 8
}

TEST_CASE("300 s event centre is the first building of the market role list") {
    const Kit k = make_kit();
    // Default roles: the Kontor (PRODTYPE 35) is in list 5, the church (PRODTYPE 70) in
    // list 1. A city with both: the market list decides.
    sim::ScenarioSpec spec = event_scenario(k, 1, 2, {5, 0, 0, 0});
    spec.islands[0].cities[0].buildings = {building_test::place(k.church, 105, 105),
                                           building_test::place(k.kontor, 121, 133)};
    {
        sim::World w = make_world_with(k, spec, named_seams());
        sim::Effects fx;
        Tick t(w, 150000);
        sim::event_300s(t.ctx, island(w), fx);
        REQUIRE(island(w).drought.has_value());
        CHECK(island(w).drought->x == 120);
        CHECK(island(w).drought->y == 128);
    }
    {
        // The role names the public list instead: the church is the centre.
        sim::SimConfig cfg;
        cfg.layout.roles.market_list = 1;
        sim::World w = make_world_with(k, spec, named_seams(), cfg);
        sim::Effects fx;
        Tick t(w, 150000);
        sim::event_300s(t.ctx, island(w), fx);
        REQUIRE(island(w).drought.has_value());
        CHECK(island(w).drought->x == 104);
        CHECK(island(w).drought->y == 104);
    }
    {
        // A city without a building in the market list gives no centre: no drought, but
        // the draw was made and the stamp written.
        sim::ScenarioSpec none = event_scenario(k, 1, 2, {5, 0, 0, 0});
        none.islands[0].cities[0].buildings = {building_test::place(k.church, 105, 105)};
        sim::World w = make_world_with(k, none, named_seams());
        sim::Effects fx;
        Tick t(w, 150000);
        sim::event_300s(t.ctx, island(w), fx);
        CHECK_FALSE(island(w).drought.has_value());
        CHECK(event_draws(t) == 1);
        CHECK(island(w).event300_stamp == 300);
    }
}

TEST_CASE(
    "300 s event centre skips the holes of the market list and the cities without a building") {
    const Kit k = make_kit();
    const auto key = [](std::uint32_t city, std::uint32_t list, std::uint32_t slot) {
        return sim::BuildingKey{sim::CityRef{sim::IslandId{0}, sim::CityNo{city}}, list, slot};
    };
    // Two buildings of the market list (the Kontor, list 5) in one city; the first one is
    // removed, which leaves a hole in front of the second (A5: holes are skipped).
    {
        sim::ScenarioSpec spec = event_scenario(k, 1, 2, {5, 0, 0, 0});
        spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 121, 133),
                                               building_test::place(k.kontor, 145, 157)};
        sim::World w = make_world_with(k, spec, named_seams());
        REQUIRE(city(w, 0).index.lists[5].slots[0].has_value());
        REQUIRE(city(w, 0).index.lists[5].slots[1].has_value());
        REQUIRE(sim::WorldTestAccess::remove(w, key(0, 5, 0)));
        REQUIRE_FALSE(city(w, 0).index.lists[5].slots[0].has_value());
        sim::Effects fx;
        Tick t(w, 150000);
        sim::event_300s(t.ctx, island(w), fx);
        REQUIRE(island(w).drought.has_value());
        CHECK(island(w).drought->x == 144); // 145 - 145 % 8
        CHECK(island(w).drought->y == 152); // 157 - 157 % 8
    }
    // A city whose market list holds only holes is passed over: the next city with a
    // building gives the centre.
    {
        sim::ScenarioSpec spec = event_scenario(k, 2, 2, {5, 0, 0, 0});
        spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 121, 133)};
        spec.islands[0].cities[1].buildings = {building_test::place(k.kontor, 145, 157)};
        sim::World w = make_world_with(k, spec, named_seams());
        REQUIRE(sim::WorldTestAccess::remove(w, key(0, 5, 0)));
        sim::Effects fx;
        Tick t(w, 150000);
        sim::event_300s(t.ctx, island(w), fx);
        REQUIRE(island(w).drought.has_value());
        CHECK(island(w).drought->x == 144);
        CHECK(island(w).drought->y == 152);
    }
}

TEST_CASE(
    "300 s event gate scans the cities below the neutral owner and the start scans all slots") {
    const Kit k = make_kit();
    // Default: neutral_owner 15, city_slots 16. A city in slot 14 opens the gate and
    // is the centre.
    {
        sim::ScenarioSpec spec = event_scenario(k, 15, 2, {5, 0, 0, 0});
        spec.islands[0].cities[14].buildings = {building_test::place(k.kontor, 121, 133)};
        sim::World w = make_world_with(k, spec, named_seams());
        sim::Effects fx;
        Tick t(w, 150000);
        sim::event_300s(t.ctx, island(w), fx);
        REQUIRE(island(w).drought.has_value());
        CHECK(island(w).drought->x == 120);
    }
    // neutral_owner 4 of 16 slots: a city only in slot 10 (beyond the gate scan) does
    // not open the gate ...
    {
        sim::SimConfig cfg;
        cfg.neutral_owner = 4;
        sim::ScenarioSpec spec = event_scenario(k, 0, 2, {5, 0, 0, 0});
        spec.islands[0].cities.push_back(syn::plain_city(10, 0));
        spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 121, 133)};
        sim::World w = make_world_with(k, spec, named_seams(), cfg);
        sim::Effects fx;
        Tick t(w, 150000);
        sim::event_300s(t.ctx, island(w), fx);
        CHECK(event_draws(t) == 1);
        CHECK_FALSE(island(w).drought.has_value());
    }
    // ... a city in slot 3 does (the last slot of the scan), and the start then finds
    // the market building in slot 10 (the start scans every slot).
    {
        sim::SimConfig cfg;
        cfg.neutral_owner = 4;
        sim::ScenarioSpec spec = event_scenario(k, 0, 2, {5, 0, 0, 0});
        spec.islands[0].cities.push_back(syn::plain_city(3, 0));
        spec.islands[0].cities.push_back(syn::plain_city(10, 0));
        spec.islands[0].cities[1].buildings = {building_test::place(k.kontor, 121, 133)};
        sim::World w = make_world_with(k, spec, named_seams(), cfg);
        sim::Effects fx;
        Tick t(w, 150000);
        sim::event_300s(t.ctx, island(w), fx);
        REQUIRE(island(w).drought.has_value());
        CHECK(island(w).drought->x == 120);
        CHECK(island(w).drought->y == 128);
    }
    // City slots beyond neutral_owner but inside the table: slot 5 only, neutral_owner 5:
    // the gate scans 0 .. 4.
    {
        sim::SimConfig cfg;
        cfg.neutral_owner = 5;
        sim::ScenarioSpec spec = event_scenario(k, 0, 2, {5, 0, 0, 0});
        spec.islands[0].cities.push_back(syn::plain_city(6, 0));
        spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 121, 133)};
        sim::World w = make_world_with(k, spec, named_seams(), cfg);
        sim::Effects fx;
        Tick t(w, 150000);
        sim::event_300s(t.ctx, island(w), fx);
        CHECK_FALSE(island(w).drought.has_value());
    }
}

TEST_CASE("start drought is refused while a drought runs and sizes the mask from the city slots") {
    const Kit k = make_kit();
    sim::ScenarioSpec spec = event_scenario(k, 1, 2, {5, 0, 0, 0});
    spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 121, 133)};
    {
        sim::World w = make_world_with(k, spec, named_seams());
        sim::Island& is = island(w);
        Tick t(w, 150000);
        CHECK(sim::start_drought(t.ctx, is));
        REQUIRE(is.drought.has_value());
        is.drought->counter = 5; // a running drought keeps its state
        CHECK_FALSE(sim::start_drought(t.ctx, is));
        CHECK(is.drought->counter == 5);
        // A drought that is running also keeps the 300 s event from replacing it.
        is.event300_stamp = 0;
        sim::Effects fx;
        sim::event_300s(t.ctx, is, fx);
        CHECK(is.drought->counter == 5);
    }
    {
        // O-55 (widened in S13): the city mask has ceil(city_slots / 32) words, so a drought
        // starts with any number of city slots.
        for (const std::uint32_t slots : {32U, 33U, 64U, 65U}) {
            INFO("city_slots " << slots);
            sim::SimConfig cfg;
            cfg.city_slots = slots;
            sim::World w = make_world_with(k, spec, named_seams(), cfg);
            Tick t(w, 150000);
            CHECK(sim::start_drought(t.ctx, island(w)));
            REQUIRE(island(w).drought.has_value());
            CHECK(island(w).drought->city_mask ==
                  std::vector<std::uint32_t>((slots + 31U) / 32U, 0U));
            CHECK(w.check_invariants() == "");
        }
    }
}

TEST_CASE("start drought at builds the record of the constructor") {
    const Kit k = make_kit();
    sim::World w = make_world_with(k, cities_scenario(1), named_seams());
    sim::Island& is = island(w);
    sim::start_drought_at(w.config(), is, 144, 152, 50, 70000);
    REQUIRE(is.drought.has_value());
    const sim::Drought& d = *is.drought;
    CHECK(d.x == 144);
    CHECK(d.y == 152);
    CHECK(d.target_ring == 6);
    CHECK(d.ring == 0);
    CHECK(d.city_mask == std::vector<std::uint32_t>{0});
    CHECK(d.phase == 0);
    CHECK(d.step == 0);
    CHECK(d.state == 1);
    CHECK(d.counter == 0);
    CHECK(d.stamp == 70000 - 65536);
    // The radius is divided by the chunk edge.
    is.drought.reset();
    sim::start_drought_at(w.config(), is, 144, 152, 63, 5);
    CHECK(is.drought->target_ring == 7);
    CHECK(is.drought->stamp == 5);
    // The constructor sets the record unconditionally: a second call replaces it (it is
    // start_drought that asks first).
    sim::start_drought_at(w.config(), is, 8, 16, 8, 9);
    CHECK(is.drought->x == 8);
    CHECK(is.drought->y == 16);
    CHECK(is.drought->target_ring == 1);
    CHECK(is.drought->stamp == 9);
}

// ------------------------------------------------------------------ the order

TEST_CASE("island tick order is fauna, objects, ring with the cities, 300 s event, stamp") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    seams.wildlife_sweep.name = "rec_fauna";
    seams.wildlife_sweep.ctx = &log;
    seams.wildlife_sweep.fn = [](void* ctx, const sim::TickContext&, sim::Island& is, std::uint32_t,
                                 std::uint32_t, sim::Effects&) {
        static_cast<Log*>(ctx)->add("fauna objmgr_stamp=" + num(is.objmgr.stamp) +
                                    " sweep_stamp=" + num(is.sweep.stamp));
    };
    seams.object_manager.name = "rec_objmgr";
    seams.object_manager.ctx = &log;
    seams.object_manager.fn = [](void* ctx, const sim::TickContext&, sim::Island& is,
                                 sim::Effects&) {
        static_cast<Log*>(ctx)->add("objmgr ring_stamp=" + num(is.ring.stamp));
    };
    seams.city_pass[0].name = "rec_pass";
    seams.city_pass[0].ctx = &log;
    seams.city_pass[0].fn = [](void* ctx, const sim::TickContext&, sim::Island& is, sim::City&,
                               sim::Effects&) {
        static_cast<Log*>(ctx)->add(
            "city objmgr_stamp=" + num(is.objmgr.stamp) + " ring_stamp=" + num(is.ring.stamp) +
            " e300=" + num(is.event300_stamp) + " sweep_stamp=" + num(is.sweep.stamp));
    };
    sim::World w = make_world_with(k, event_scenario(k, 1, 2, {5, 1, 1, 1}), seams);
    sim::Island& is = island(w);
    sim::Effects fx;
    // One tick after the three stamps (d = 1): one chunk of each sweep, one deposit, one
    // ring slot, and the 300 s gate is open (game second 300).
    const std::uint16_t old16 = 149999 % 65536;
    is.sweep.stamp = old16;
    is.objmgr.stamp = old16;
    is.ring.stamp = old16;
    Tick t(w, 150000);
    sim::island_tick(t.ctx, is, fx);
    const std::uint16_t t16 = 150000 % 65536;
    REQUIRE(log.lines.size() == 3);
    // Fauna: before the object manager and before the sweep stamp.
    CHECK(log.lines[0] == "fauna objmgr_stamp=" + num(old16) + " sweep_stamp=" + num(old16));
    // The object manager runs before the ring and has not written its stamp.
    CHECK(log.lines[1] == "objmgr ring_stamp=" + num(old16));
    // The city pass is in the ring: the object manager has written its stamp, the ring
    // and the 300 s event have not, nor the sweep stamp.
    CHECK(log.lines[2] == "city objmgr_stamp=" + num(t16) + " ring_stamp=" + num(old16) +
                              " e300=0 sweep_stamp=" + num(old16));
    // After the tick all four stamps are written.
    CHECK(is.objmgr.stamp == t16);
    CHECK(is.ring.stamp == t16);
    CHECK(is.event300_stamp == 300);
    CHECK(is.sweep.stamp == t16);
}

TEST_CASE("recording seams carry state through ctx and are called with their own") {
    const Kit k = make_kit();
    Log fauna, objmgr, pass0, pass7;
    sim::Seams seams = named_seams();
    seams.wildlife_sweep.name = "rec_fauna";
    seams.wildlife_sweep.ctx = &fauna;
    seams.wildlife_sweep.fn = [](void* ctx, const sim::TickContext&, sim::Island&, std::uint32_t,
                                 std::uint32_t,
                                 sim::Effects&) { static_cast<Log*>(ctx)->add("f"); };
    seams.object_manager.name = "rec_objmgr";
    seams.object_manager.ctx = &objmgr;
    seams.object_manager.fn = [](void* ctx, const sim::TickContext&, sim::Island&, sim::Effects&) {
        static_cast<Log*>(ctx)->add("o");
    };
    seams.city_pass[0].name = "rec_pass0";
    seams.city_pass[0].ctx = &pass0;
    seams.city_pass[0].fn = [](void* ctx, const sim::TickContext&, sim::Island&, sim::City&,
                               sim::Effects&) { static_cast<Log*>(ctx)->add("p0"); };
    seams.city_pass[7].name = "rec_pass7";
    seams.city_pass[7].ctx = &pass7;
    seams.city_pass[7].fn = [](void* ctx, const sim::TickContext&, sim::Island&, sim::City&,
                               sim::Effects&) { static_cast<Log*>(ctx)->add("p7"); };
    sim::World w = make_world_with(k, cities_scenario(1), seams);
    w.advance(10);
    CHECK(fauna.lines == std::vector<std::string>{"f"});
    CHECK(objmgr.lines == std::vector<std::string>{"o"});
    CHECK(pass0.lines == std::vector<std::string>{"p0"});
    CHECK(pass7.lines == std::vector<std::string>{"p7"});
    // The ctx pointer is not state: the same world with other pointers hashes the same.
    Log other;
    sim::Seams seams2 = seams;
    seams2.wildlife_sweep.ctx = &other;
    seams2.object_manager.ctx = &other;
    seams2.city_pass[0].ctx = &other;
    seams2.city_pass[7].ctx = &other;
    sim::World w2 = make_world_with(k, cities_scenario(1), seams2);
    w2.advance(10);
    CHECK(w2.state_hash() == w.state_hash());
    CHECK(other.lines.size() == 4);
}

TEST_CASE("advance runs the islands in table order and then the players") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    seams.city_pass[0].name = "rec_pass";
    seams.city_pass[0].ctx = &log;
    seams.city_pass[0].fn = [](void* ctx, const sim::TickContext&, sim::Island& is, sim::City& c,
                               sim::Effects&) {
        static_cast<Log*>(ctx)->add("island " + num(is.index.v) + " city " + num(c.number.v));
    };
    seams.player_groups.name = "rec_groups";
    seams.player_groups.ctx = &log;
    seams.player_groups.fn = [](void* ctx, const sim::TickContext&, sim::SerialWorld&,
                                sim::Player& p) {
        static_cast<Log*>(ctx)->add("groups " + num(p.index.v));
    };
    sim::ScenarioSpec spec = syn::two_island_scenario();
    spec.islands[0].cities.push_back(syn::plain_city(1, 0));
    sim::World w = make_world_with(k, spec, seams);
    w.advance(10);
    // Island 0: the ring visits slots 0 and 1 (d = 5: 5 * 16 / 50 + 1 = 2), island 3 its
    // slot 0 (empty) and slot 1. Then the player table visits min(5 * 32 / 50 + 1, 32)
    // = 4 slots: 0 and 1 hold a player.
    REQUIRE(log.lines.size() == 5);
    CHECK(log.lines[0] == "island 0 city 0");
    CHECK(log.lines[1] == "island 0 city 1");
    CHECK(log.lines[2] == "island 3 city 1");
    CHECK(log.lines[3] == "groups 0");
    CHECK(log.lines[4] == "groups 1");
}

TEST_CASE("advance zero ms still runs every subsystem") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    install_pass_city(seams, 0, &log);
    install_fauna(seams, &log);
    sim::World w = make_world_with(k, cities_scenario(2), seams);
    const std::uint64_t ms = w.clock().ms;
    CHECK(w.advance(0) == 0);
    CHECK(w.clock().ms == ms);
    CHECK(w.clock().last_tick == 0);
    // d = 0: one slot of the ring (city 0), one chunk of each sweep, one player slot.
    CHECK(log.count("city 0") == 1);
    CHECK(log.count("city 1") == 0);
    CHECK(log.count("fauna 0 0") == 1);
    CHECK(island(w).ring.cursor == 1);
    CHECK(island(w).sweep.grow_x == 8);
    CHECK(w.players().cursor == 1);
    CHECK(w.advance(0) == 0);
    CHECK(log.count("city 1") == 1);
    CHECK(island(w).ring.cursor == 2);
}

TEST_CASE("advance is not step size invariant but is deterministic for one step sequence") {
    const Kit k = make_kit();
    auto run = [&](const std::vector<std::uint32_t>& steps) {
        sim::ScenarioSpec spec = syn::two_island_scenario();
        spec.islands[0].cities[0].buildings = {weaver_at(k, 101, 101, 0)};
        sim::World w = make_world_with(k, spec, named_seams());
        for (const std::uint32_t ms : steps) {
            w.advance(ms);
        }
        return w.state_hash();
    };
    CHECK(run({5, 5, 5, 5}) == run({5, 5, 5, 5}));
    CHECK(run({10, 10}) == run({10, 10}));
    // 2 x 5 ms and 1 x 10 ms reach the same tick but not the same state.
    CHECK(run({5, 5}) != run({10}));
    CHECK(run({1, 9}) != run({9, 1}));
}

TEST_CASE("advance copy is an independent snapshot of the scheduler state") {
    const Kit k = make_kit();
    sim::ScenarioSpec spec = syn::two_island_scenario();
    spec.islands[0].cities[0].buildings = {weaver_at(k, 101, 101, 0)};
    sim::World a = make_world_with(k, spec, named_seams());
    for (int i = 0; i < 50; ++i) {
        a.advance(10);
    }
    sim::World b = a;
    const std::uint64_t before = a.state_hash();
    for (int i = 0; i < 50; ++i) {
        b.advance(10);
    }
    CHECK(a.state_hash() == before);
    CHECK(b.state_hash() != before);
    for (int i = 0; i < 50; ++i) {
        a.advance(10);
    }
    CHECK(a.state_hash() == b.state_hash());
}

TEST_CASE("the 300 s event runs inside advance at game second 300 with the world stream") {
    const Kit k = make_kit();
    // Three islands' worth of draws are not needed: one island, table {5, 0, ...}.
    sim::ScenarioSpec spec = event_scenario(k, 1, 2, {5, 0, 0, 0});
    spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 121, 133)};
    sim::World w = make_world_with(k, spec, named_seams());
    sim::WorldTestAccess::clock(w).ms = 2ULL * 149999; // the next advance of 2 ms is tick 150000
    w.advance(2);
    CHECK(w.rng().cursor() == 1);
    REQUIRE(island(w).drought.has_value());
    CHECK(island(w).drought->x == 120);
    CHECK(island(w).event300_stamp == 300);
    // Telemetry: one Event300 draw in this advance, the cursor 0 -> 1.
    const sim::SimOutput& out = w.last_output();
    REQUIRE(out.draws.size() == 1);
    CHECK(out.draws[0].site == sim::DrawSite::Event300);
    CHECK(out.draws[0].count == 1);
    CHECK(out.rng_cursor_before == 0);
    CHECK(out.rng_cursor_after == 1);
    // The next advance makes none and clears the telemetry.
    w.advance(2);
    CHECK(w.last_output().draws.empty());
    CHECK(w.last_output().rng_cursor_before == 1);
    CHECK(w.last_output().rng_cursor_after == 1);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("draws of several islands are counted per site and consumed in table order") {
    const Kit k = make_kit();
    // Two islands, each with a city and climate NORD; the table gives the first island
    // the draw 0 (a hit) and the second the draw 1 (a miss). Each island has a Kontor.
    sim::ScenarioSpec spec = syn::two_island_scenario();
    spec.rng_table = std::vector<std::uint16_t>{9, 0, 1, 7};
    spec.seed = 0;
    spec.islands[0].cities[0].buildings = {building_test::place(k.kontor, 121, 133)};
    spec.islands[1].cities[0].buildings = {building_test::place(k.kontor, 305, 205)};
    spec.islands[1].climate = 2;
    sim::World w = make_world_with(k, spec, named_seams());
    sim::WorldTestAccess::clock(w).ms = 2ULL * 149999;
    w.advance(2);
    CHECK(island(w, 0).drought.has_value());
    CHECK_FALSE(island(w, 3).drought.has_value());
    REQUIRE(w.last_output().draws.size() == 1);
    CHECK(w.last_output().draws[0].count == 2);
    CHECK(w.rng().cursor() == 2);
}
