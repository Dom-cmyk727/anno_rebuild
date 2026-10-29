#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/fixed_map.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/fixed_map.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

namespace fm = anno::formats::fixed_map;
namespace sl = anno::sim_load;

namespace {

std::filesystem::path game_dir() {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "fixed_map_game";
}

const char* kLayout = "anno1503-fixed-map 1\n"
                      "name synth\n"
                      "edge 128 128\n"
                      "base Scenes/base.sze\n"
                      "seed 42\n"
                      "island 0 0 0 NORD/N_A.SCP\n"
                      "raw 0 3 1\n"
                      "island 1 64 0 DSCHUNGEL/D_A.SCP\n"
                      "island 2 0 64 NORD/N_A.SCP\n"
                      "start 0 0 1 1 0\n"
                      "start 1 1 65 1 0\n"
                      "start 2 2 1 65 0\n";

fm::FixedMap layout_of(std::string_view text) {
    auto parsed = fm::parse(std::vector<std::uint8_t>(text.begin(), text.end()));
    REQUIRE(parsed);
    return std::move(parsed).value();
}

sl::FixedMapOptions with_kontor(const anno::sim::synthetic::SyntheticTypes& types) {
    sl::FixedMapOptions options;
    options.kontor_type = types.kontor.v;
    return options;
}

std::vector<anno::sim::Seat> two_seats() {
    anno::sim::Seat a;
    a.name = "Ada";
    a.colour = 4;
    anno::sim::Seat b;
    b.name = "Bea";
    b.colour = 5;
    return {a, b};
}

const anno::sim::ScenarioSpec::IslandSpec* island_slot(const anno::sim::ScenarioSpec& spec,
                                                       std::uint32_t slot) {
    for (const auto& island : spec.islands) {
        if (island.slot.v == slot) {
            return &island;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("fixed map: the loader builds the seated world from the base scenario") {
    auto types = anno::sim::synthetic::make_build_types();
    REQUIRE(types);
    const fm::FixedMap map = layout_of(kLayout);
    auto loaded = sl::load_fixed_map(map, two_seats(), game_dir(), *types.value().registry, {},
                                     with_kontor(types.value()));
    if (!loaded) {
        const std::string message = loaded.error();
        REQUIRE_MESSAGE(false, message);
    }
    const anno::sim::ScenarioSpec& spec = loaded.value().spec;

    CHECK(spec.settings.world_width == 128);
    CHECK(spec.settings.world_height == 128);
    CHECK(spec.settings.prop0 == 4);
    CHECK(spec.settings.prop2 == 9);
    CHECK(spec.settings.prop14 == 1);
    CHECK(spec.seed == 42);
    REQUIRE(spec.players.size() == 2);
    CHECK(spec.players[0].id.v == 0);
    CHECK(spec.players[0].kind == 8);
    CHECK(spec.players[0].volk == 1);
    CHECK(spec.players[0].money == 25000);
    CHECK(spec.players[0].colour == 4);
    CHECK(spec.players[1].money == 25000);
    for (const auto& player : spec.players) {
        CHECK(player.id.v != 31);
    }

    const auto* nord = island_slot(spec, 0);
    const auto* jungle = island_slot(spec, 1);
    const auto* spare = island_slot(spec, 2);
    REQUIRE(nord);
    REQUIRE(jungle);
    REQUIRE(spare);
    CHECK(nord->climate == 2);
    CHECK(jungle->climate == 4);
    CHECK(nord->deposits.empty());
    CHECK(jungle->deposits.empty());
    REQUIRE(nord->cities.size() == 1);
    CHECK(nord->cities[0].no.v == 0);
    CHECK(nord->cities[0].owner.v == 0);
    REQUIRE(nord->cities[0].buildings.size() == 1);
    CHECK(nord->cities[0].buildings[0].type.v == types.value().kontor.v);
    CHECK(nord->cities[0].buildings[0].x == 1);
    REQUIRE(nord->owner_runs.size() == 1);
    CHECK(nord->owner_runs[0].first == 0);
    CHECK(nord->owner_runs[0].second == nord->w * nord->h);
    CHECK(spare->cities.empty());
    CHECK(spare->owner_runs.empty());

    bool raw_warned = false;
    for (const auto& warning : loaded.value().warnings) {
        if (warning.kind == anno::sim::ScenarioWarningKind::LinkRawMaterials) {
            raw_warned = true;
        }
    }
    CHECK(raw_warned);

    auto first = anno::sim::World::create(spec, types.value().registry);
    auto second = anno::sim::World::create(spec, types.value().registry);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first.value().state_hash() == second.value().state_hash());
    CHECK(first.value().players().find(anno::sim::PlayerId{31}) == nullptr);
    CHECK(first.value().players().find(anno::sim::PlayerId{0})->money == 25000);
    CHECK(first.value().players().find(anno::sim::PlayerId{0})->default_colour == 4);
    REQUIRE(first.value().islands()[0]);
    CHECK(first.value().islands()[0]->tiles.owner(0) == 0);
    REQUIRE(first.value().islands()[2]);
    CHECK(first.value().islands()[2]->tiles.owner(0) == 15);
    CHECK(first.value().islands()[2]->city(anno::sim::CityNo{0}) == nullptr);

    for (int i = 0; i < 3000; ++i) {
        first.value().advance(100);
        second.value().advance(100);
    }
    CHECK(first.value().check_invariants().empty());
    CHECK(second.value().check_invariants().empty());
    CHECK(first.value().state_hash() == second.value().state_hash());

    auto builder = anno::sim::World::create(spec, types.value().registry);
    REQUIRE(builder);
    anno::sim::Command cmd;
    cmd.issuer = anno::sim::PlayerId{0};
    cmd.body = anno::sim::BuildCmd{anno::sim::CityRef{anno::sim::IslandId{0}, anno::sim::CityNo{0}},
                                   {anno::sim::PlacementWire{types.value().bow_maker, 10, 10, 0}}};
    const anno::sim::ApplyResult built = builder.value().apply(cmd);
    CHECK(built.status != anno::sim::Reject::TileNotOwned);
    CHECK(built.ok());
}

TEST_CASE("fixed map: money in the layout replaces the base scenario's") {
    auto types = anno::sim::synthetic::make_build_types();
    REQUIRE(types);
    std::string text = kLayout;
    const auto at = text.find("seed 42\n");
    REQUIRE(at != std::string::npos);
    text.insert(at, "money 12345\n");
    auto loaded = sl::load_fixed_map(layout_of(text), two_seats(), game_dir(),
                                     *types.value().registry, {}, with_kontor(types.value()));
    REQUIRE(loaded);
    CHECK(loaded.value().spec.players[0].money == 12345);
}

TEST_CASE("fixed map: the loader refuses a layout the world would also refuse") {
    auto types = anno::sim::synthetic::make_build_types();
    REQUIRE(types);
    const auto refuse = [&](std::string_view text, std::string_view fragment) {
        auto loaded = sl::load_fixed_map(layout_of(text), two_seats(), game_dir(),
                                         *types.value().registry, {}, with_kontor(types.value()));
        REQUIRE_FALSE(loaded);
        CHECK(loaded.error().find(fragment) != std::string::npos);
    };
    refuse("anno1503-fixed-map 1\nname o\nedge 128 128\nbase Scenes/base.sze\nseed 1\n"
           "island 0 0 0 NORD/N_A.SCP\nisland 1 0 0 DSCHUNGEL/D_A.SCP\n"
           "start 0 0 1 1 0\nstart 1 1 1 1 0\n",
           "overlaps");
    refuse("anno1503-fixed-map 1\nname o\nedge 128 128\nbase Scenes/base.sze\nseed 1\n"
           "island 0 100 0 NORD/N_A.SCP\nisland 1 0 64 DSCHUNGEL/D_A.SCP\n"
           "start 0 0 101 1 0\nstart 1 1 1 65 0\n",
           "outside the world");
    refuse("anno1503-fixed-map 1\nname o\nedge 128 128\nbase Scenes/base.sze\nseed 1\n"
           "island 0 0 0 NOPE.SCP\nisland 1 64 0 DSCHUNGEL/D_A.SCP\n"
           "start 0 0 1 1 0\nstart 1 1 65 1 0\n",
           "unknown template");
    refuse("anno1503-fixed-map 1\nname o\nedge 128 128\nbase Scenes/base.sze\nseed 1\n"
           "island 0 0 0 NORD/N_A.SCP\nstart 0 9 1 1 0\nstart 1 0 1 1 0\n",
           "is not in the layout");
    refuse("anno1503-fixed-map 1\nname o\nedge 128 128\nbase Scenes/base.sze\nseed 1\n"
           "island 0 0 0 NORD/N_A.SCP\nisland 1 64 0 DSCHUNGEL/D_A.SCP\n"
           "start 0 0 1 1 0\nstart 2 1 65 1 0\n",
           "without gaps");
    refuse("anno1503-fixed-map 1\nname o\nedge 128 128\nbase Scenes/base.sze\nseed 1\n"
           "island 0 0 0 NORD/N_A.SCP\nisland 1 64 0 DSCHUNGEL/D_A.SCP\n"
           "start 0 0 40 40 0\nstart 1 1 65 1 0\n",
           "kontor footprint is outside");
    refuse("anno1503-fixed-map 1\nname o\nedge 128 128\nbase Scenes/base.sze\nseed 1\n"
           "island 0 0 0 NORD/N_A.SCP\nisland 1 64 0 DSCHUNGEL/D_A.SCP\n"
           "start 0 0 1 1 0\nstart 1 0 4 4 0\n",
           "used twice");

    anno::sim::Seat only;
    only.name = "Ada";
    auto too_few = sl::load_fixed_map(layout_of(kLayout), std::span{&only, 1}, game_dir(),
                                      *types.value().registry, {}, with_kontor(types.value()));
    REQUIRE_FALSE(too_few);
    CHECK(too_few.error().find("is not from 2 to") != std::string::npos);
}

TEST_CASE("fixed map: the developer start can be turned off") {
    auto types = anno::sim::synthetic::make_build_types();
    REQUIRE(types);
    sl::FixedMapOptions options = with_kontor(types.value());
    options.developer_start = false;
    auto loaded = sl::load_fixed_map(layout_of(kLayout), two_seats(), game_dir(),
                                     *types.value().registry, {}, options);
    REQUIRE(loaded);
    for (const auto& island : loaded.value().spec.islands) {
        CHECK(island.cities.empty());
        CHECK(island.owner_runs.empty());
    }
}

TEST_CASE("fixed map: lan5 loads for two to five seats") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sl::OriginalRegistryLoad registry =
        sl::load_original_registry(*dir, sl::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.error.empty(), registry.error);
    REQUIRE(registry.types);

    const std::filesystem::path path =
        std::filesystem::path(ANNO_SOURCE_DIR) / "data" / "maps" / "lan5.txt";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    auto parsed = fm::parse(bytes);
    REQUIRE(parsed);
    const fm::FixedMap layout = std::move(parsed).value();

    std::vector<anno::sim::Seat> seats;
    for (std::uint32_t i = 0; i < 5; ++i) {
        anno::sim::Seat seat;
        seat.name = "P" + std::to_string(i);
        seat.colour = i;
        seats.push_back(std::move(seat));
    }

    std::uint64_t hash = 0;
    for (std::size_t n = 2; n <= 5; ++n) {
        auto loaded = sl::load_fixed_map(layout, std::span<const anno::sim::Seat>(seats.data(), n),
                                         *dir, *registry.types, {});
        if (!loaded) {
            const std::string message = loaded.error();
            REQUIRE_MESSAGE(false, message);
        }
        const anno::sim::ScenarioSpec& spec = loaded.value().spec;
        CHECK(spec.settings.world_width == 1431);
        CHECK(spec.settings.world_height == 1431);
        REQUIRE(spec.players.size() == n);
        CHECK(spec.players[0].money == 30000);
        std::size_t cities = 0;
        for (const auto& island : spec.islands) {
            cities += island.cities.size();
            if (!island.cities.empty()) {
                CHECK(island.cities.size() == 1);
                CHECK(island.cities[0].buildings.size() == 1);
                CHECK(island.cities[0].buildings[0].type.v == 1302);
            }
        }
        CHECK(cities == n);
        auto world = anno::sim::World::create(spec, registry.types);
        if (!world) {
            const std::string message = world.error();
            REQUIRE_MESSAGE(false, message);
        }
        CHECK(world.value().check_invariants().empty());
        if (n != 5) {
            continue;
        }
        auto second = anno::sim::World::create(spec, registry.types);
        REQUIRE(second);
        for (int step = 0; step < 3000; ++step) {
            world.value().advance(100);
            second.value().advance(100);
        }
        CHECK(world.value().check_invariants().empty());
        CHECK(second.value().check_invariants().empty());
        CHECK(world.value().state_hash() == second.value().state_hash());
        hash = world.value().state_hash();
    }
    std::cout << "lan5 five-minute hash " << std::hex << hash << std::dec << '\n';
}
