// B0 baseline before availability enforcement: original later-stage type, public build/import.
#include <doctest/doctest.h>
#include <anno/sim/world.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>
#include "command_helpers.hpp"
namespace sim = anno::sim;
TEST_CASE("bootstrap B0: original locked build baseline and scenario exception (game files)") {
    const auto game = anno::test::game_dir();
    if (!game) { MESSAGE("SKIP: ANNO1503_GAME_DIR is unset"); return; }
    const auto loaded = anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    const auto* inn = loaded.types->find(sim::TypeId{1335});
    REQUIRE(inn != nullptr);
    for (const auto mask : {1U, 5U}) {
        auto spec = sim::synthetic::empty_scenario(77);
        sim::ScenarioSpec::PlayerSpec p;
        p.id = sim::PlayerId{0}; p.money = 30000; p.flags = mask;
        spec.players.push_back(p);
        auto is = sim::synthetic::plain_island(0, 100, 100, 32, 32);
        is.owner_runs = {{0, 32 * 32}};
        std::vector<std::uint8_t> heights(32 * 32, 0x30);
        is.terrain = sim::make_terrain(heights, std::vector<std::uint16_t>(heights.size()), std::vector<std::uint8_t>(heights.size()));
        auto city = sim::synthetic::plain_city(0, 0);
        city.stock = std::vector<std::uint32_t>(sim::SimConfig{}.warehouse_goods, 0);
        for (auto g : {41U, 42U, 43U, 44U, 45U}) (*city.stock)[g] = 50U * 256U;
        sim::ScenarioSpec::BuildingSpec imported;
        imported.type = sim::TypeId{1335}; imported.x = 102; imported.y = 102;
        city.buildings.push_back(imported);
        is.cities.push_back(city); spec.islands.push_back(is);
        auto made = sim::World::create(spec, loaded.types);
        REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string() : made.error()));
        auto world = std::move(made).value();
        REQUIRE(world.building_at(command_test::kCity0, 102, 102) != nullptr);
        const auto result = world.apply(command_test::build(0, command_test::kCity0, {command_test::at(sim::TypeId{1335}, 112, 112)}));
        CHECK_MESSAGE(result.ok(), sim::reject_name(result.status));
    }
}
