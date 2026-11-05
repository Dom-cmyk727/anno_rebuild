#include <chrono>

#include <doctest/doctest.h>

#include <anno/sim/script_runtime.hpp>

#include "alloc_probe.hpp"
using namespace anno::sim;
TEST_CASE(
    "SCRIPT2 indexed scale charges build and query work with mod players areas and statements") {
    struct Case {
        std::uint32_t players, areas, statements, unrelated;
    };
    for (auto test : {Case{8, 4, 16, 100}, Case{64, 32, 128, 10000}, Case{128, 128, 512, 50000}}) {
        SimConfig cfg;
        cfg.player_slots = test.players;
        cfg.neutral_player = test.players - 1;
        const auto owner = PlayerId{test.players == 128 ? 70u : test.players - 2};
        TypeRegistry::Builder builder;
        builder.config(cfg);
        FigureType ship;
        ship.id = 900000;
        ship.bewohntype = 113;
        builder.figure_type(ship);
        auto civilian = ship;
        civilian.id = 900001;
        civilian.bewohntype = 53;
        builder.figure_type(civilian);
        auto registry = builder.build();
        REQUIRE(registry);
        auto players = make_player_table(cfg);
        for (std::uint32_t i = 0; i < test.players; ++i)
            players.slots[i] = make_player(PlayerId{i}, cfg);
        auto figures = make_figure_registry(cfg);
        for (std::uint32_t i = 0; i <= test.unrelated; ++i) {
            Figure f;
            f.id = FigureId{i + 1};
            f.owner = owner;
            f.kind = i == test.unrelated ? 900000u : 900001u;
            f.pos =
                i == test.unrelated ? FixedPos{10 * 256, 30 * 256} : FixedPos{500 * 256, 500 * 256};
            REQUIRE(add_figure(figures, f, cfg, Rules{}) != kNoSlot);
        }
        std::string source = "[OBJECT_DEFINE]\n[AREA_DEFINE]\n";
        for (std::uint32_t i = 0; i < test.areas; ++i)
            source += "\"MOD_AREA_" + std::to_string(i) + "\" 30,10,20,40\n";
        source += "[PLAYERMAPPER_DEFINE]\n\"MOD_OWNER\" " + std::to_string(owner.v) +
                  "\n[SCRIPT_KERNEL]\nSET HIT=FALSE;\n";
        for (std::uint32_t i = 0; i < test.areas; ++i) {
            source += "IF AREA \"MOD_AREA_" + std::to_string(i) +
                      "\" TOUCHED_BY_SHIP_FROM \"MOD_OWNER\" THEN {\n";
            for (std::uint32_t j = 0; j < test.statements / test.areas; ++j)
                source += "HIT=TRUE;\n";
            source += "}\n";
        }
        source += "[SIGNALS]\n[CAMARAPOS]\n0,0\n";
        auto runtime = prepare_script(source, *registry.value(), players, cfg);
        REQUIRE(runtime);
        auto timers = make_script_timers(cfg);
        WorldSettings settings;
        settings.world_width = settings.world_height = 1024;
        std::uint64_t allocated = 0;
        const auto begin = std::chrono::steady_clock::now();
        {
            anno::test::AllocScope allocations;
            for (int repeat = 0; repeat < 64; ++repeat)
                REQUIRE(advance_script(runtime.value(), players, timers, *registry.value(), 250,
                                       &figures, &settings));
            allocated = allocations.bytes();
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - begin)
                                 .count();
        const auto& stats = runtime.value().query_stats;
        CHECK(runtime.value().state.signals[1] == 1);
        CHECK(stats.slots == figures.list.slots.size());
        CHECK(stats.inserted == 1);
        CHECK(stats.rows == test.areas);
        CHECK(stats.cells == test.areas);
        CHECK(stats.candidates == test.areas);
        CHECK(stats.crew == 0);
        CHECK(runtime.value().state.last_work ==
              stats.slots + 1 + 5 * test.areas + test.statements);
        MESSAGE("SCRIPT2_SCALE players="
                << test.players << " owner=" << owner.v << " areas=" << test.areas << " statements="
                << test.statements << " unrelated=" << test.unrelated << " visits=64"
                << " slots_per_visit=" << stats.slots << " insertions_per_visit=" << stats.inserted
                << " rows_per_visit=" << stats.rows << " cells_per_visit=" << stats.cells
                << " candidates_per_visit=" << stats.candidates
                << " work_per_visit=" << runtime.value().state.last_work
                << " allocation_bytes_sum64=" << allocated << " elapsed_us64=" << elapsed);
    }
}
