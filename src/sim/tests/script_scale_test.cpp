#include <chrono>

#include <doctest/doctest.h>

#include <anno/sim/script_runtime.hpp>
using namespace anno::sim;
TEST_CASE("SCRIPT1 bounded scale work and idle queries") {
    for (auto large : {false, true}) {
        const std::uint32_t players_count = large ? 128 : 8, types_count = large ? 10000 : 100,
                            statements = large ? 2000 : 64;
        const auto folk = large ? 70u : 1u;
        const auto owner = PlayerId{large ? 70u : 0u};
        SimConfig cfg;
        cfg.player_slots = players_count;
        cfg.neutral_player = players_count - 1;
        TypeRegistry::Builder builder;
        builder.config(cfg);
        std::uint32_t eligible = 0;
        for (std::uint32_t i = 0; i < types_count; ++i) {
            BuildingType type;
            type.id = TypeId{100000u + i};
            type.permissions.folks = {folk};
            type.permissions.bulk_excluded = i % 7 == 0;
            eligible += type.permissions.bulk_excluded ? 0u : 1u;
            builder.add(type);
        }
        auto registry = builder.build();
        REQUIRE(registry);
        auto players = make_player_table(cfg);
        for (std::uint32_t i = 0; i < players_count; ++i)
            players.slots[i] = make_player(PlayerId{i}, cfg);
        players.find(owner)->volk = folk;
        std::string source = "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n\"ME\" " +
                             std::to_string(owner.v) +
                             "\n[SCRIPT_KERNEL]\nSET X=FALSE;\nINITBLOCK(){\n";
        for (std::uint32_t i = 0; i < statements; ++i)
            source += "X=TRUE;\n";
        source += "SETCONSTRUCTION_DRAWSTATE_ALL(\"ME\",TRUE);\n}\n[SIGNALS]\n[CAMARAPOS]\n0,0\n";
        auto runtime = prepare_script(source, *registry.value(), players, cfg, {.work = 20000});
        REQUIRE(runtime);
        auto timers = make_script_timers(cfg);
        REQUIRE(advance_script(runtime.value(), players, timers, *registry.value(), 0));
        CHECK(runtime.value().state.last_work == statements + 1 + eligible);
        CHECK(players.find(owner)->permissions.size() == eligible);
        for (unsigned i = 0; i < 1000; ++i)
            REQUIRE(advance_script(runtime.value(), players, timers, *registry.value(), 0));
        CHECK(runtime.value().state.last_work == 0);
        REQUIRE(set_construction_permission(*players.find(owner), players.find(owner)->permissions,
                                            *registry.value(), TypeId{100001}, 0));
        std::uint64_t denied = 0;
        const auto start = std::chrono::steady_clock::now();
        for (std::uint32_t q = 0; q < 250000; ++q)
            denied += stored_build_eligibility(*registry.value(), players, owner, TypeId{100001},
                                               players.find(owner)->permissions)
                              .allowed()
                          ? 0u
                          : 1u;
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();
        CHECK(denied == 250000);
        MESSAGE("SCRIPT_SCALE players=" << players_count << " types=" << types_count
                                        << " statements=" << statements << " eligible=" << eligible
                                        << " init_work=" << statements + 1 + eligible
                                        << " idle_work=0 queries=250000 elapsed_us=" << elapsed);
    }
}
