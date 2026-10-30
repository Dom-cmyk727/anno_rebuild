// The --commands file (docs/app/game-shell.md, "The command file") and the
// determinism check of W4.3 A6(d): the same commands through the runner and
// directly through run_turn give the same state hash at every turn. The cases
// load lan5 and skip when ANNO1503_GAME_DIR is unset.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/commands_file.hpp>
#include <anno/app/play.hpp>
#include <anno/formats/fixed_map.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim_load/fixed_map.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;

namespace {

// The lan5 world, built as the play test builds it (fixed map, two seats). The
// developer's test resources are on unless `resources` is false.
std::optional<anno::sim::World> lan5_world(const fs::path& dir, bool resources = true) {
    const fs::path path =
        fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    auto parsed = anno::formats::fixed_map::parse(bytes);
    if (!parsed) {
        return std::nullopt;
    }
    const auto registry =
        anno::sim_load::load_original_registry(dir, anno::sim_load::FigureEntries::Required);
    if (!registry.types) {
        return std::nullopt;
    }
    anno::sim_load::FixedMapOptions options;
    options.resources.enabled = resources;
    auto loaded = anno::sim_load::load_fixed_map(
        parsed.value(),
        std::vector<anno::sim::Seat>{anno::sim::Seat{anno::sim::SeatKind::Human, "P0", 0, 0},
                                     anno::sim::Seat{anno::sim::SeatKind::Human, "P1", 1, 0}},
        dir, *registry.types, {}, options);
    if (!loaded) {
        return std::nullopt;
    }
    auto created = anno::sim::World::create(loaded.value().spec, registry.types);
    if (!created) {
        return std::nullopt;
    }
    return std::move(created).value();
}

// The first island that has a city of the developer player: its origin tile.
std::optional<anno::sim::TileXY> developer_origin(const anno::sim::World& world) {
    const auto& islands = world.islands();
    for (std::size_t i = 0; i < islands.size(); ++i) {
        if (!islands[i]) {
            continue;
        }
        for (const auto& city : islands[i]->cities) {
            if (city && city->owner == anno::app::developer_player()) {
                return anno::sim::TileXY{islands[i]->tiles.x0(), islands[i]->tiles.y0()};
            }
        }
    }
    return std::nullopt;
}

// The cheapest building type (lowest gold, no goods cost): the one a developer
// start can afford, so the scripted builds reach the placement rule.
std::optional<std::uint32_t> first_building_type(const anno::sim::World& world) {
    std::optional<std::uint32_t> best;
    std::int64_t best_gold = 0;
    for (std::uint32_t id = 1; id <= 0xFFFFu; ++id) {
        if (!anno::app::anlage_is_building(world, static_cast<std::uint16_t>(id))) {
            continue;
        }
        const anno::sim::BuildingType* type = world.types().find(anno::sim::TypeId{id});
        if (type == nullptr || !type->cost_goods.empty()) {
            continue;
        }
        const auto gold = static_cast<std::int64_t>(type->cost_gold);
        if (!best || gold < best_gold) {
            best = id;
            best_gold = gold;
        }
    }
    return best;
}

} // namespace

TEST_CASE("commands file: errors name the line") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto world = lan5_world(*dir);
    REQUIRE(world.has_value());
    const auto origin = developer_origin(*world);
    REQUIRE(origin.has_value());
    const auto type = first_building_type(*world);
    REQUIRE(type.has_value());
    const std::string t = std::to_string(*type);
    const std::string x = std::to_string(origin->x + 4);
    const std::string y = std::to_string(origin->y + 4);

    // Each bad script names the first bad line.
    const auto expect_line = [&](const std::string& text, const std::string& line) {
        auto parsed = anno::app::parse_commands(text, *world, anno::app::developer_player());
        REQUIRE_MESSAGE(!parsed, text);
        CHECK_MESSAGE(parsed.error().find(line) != std::string::npos, parsed.error());
    };
    expect_line("turn 1 build " + t + " " + x + " " + y + " 0\nturn 1 nope 1 2 3\n", "line 2");
    expect_line("turn 3 build " + t + " " + x + " " + y + " 0\nturn 2 demolish " + x + " " + y +
                    "\n",
                "line 2");
    expect_line("turn 1 build " + t + " " + x + " " + y + " 4\n", "line 1");
    expect_line("turn 1 demolish -90000 -90000\n", "line 1");
    expect_line("turn x build " + t + " " + x + " " + y + " 0\n", "line 1");
    expect_line("turn 1 build " + t + " " + x + " " + y + "\n", "line 1");
}

TEST_CASE("play: scripted commands match run_turn turn by turn (A6 d)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto world_a = lan5_world(*dir);
    auto world_b = lan5_world(*dir);
    REQUIRE(world_a.has_value());
    REQUIRE(world_b.has_value());
    const auto origin = developer_origin(*world_a);
    REQUIRE(origin.has_value());
    const auto type = first_building_type(*world_a);
    REQUIRE(type.has_value());
    const std::string t = std::to_string(*type);
    const std::string x = std::to_string(origin->x + 4);
    const std::string y = std::to_string(origin->y + 4);
    const std::string x2 = std::to_string(origin->x + 6);
    const std::string script = "# three turns of commands\n"
                               "turn 1 build " +
                               t + " " + x + " " + y +
                               " 0\n"
                               "turn 3 demolish " +
                               x + " " + y +
                               "\n"
                               "turn 5 build " +
                               t + " " + x2 + " " + y + " 1 " + x2 + " " +
                               std::to_string(origin->y + 7) + " 1\n";
    auto parsed = anno::app::parse_commands(script, *world_a, anno::app::developer_player());
    const bool parsed_ok = parsed.ok();
    const std::string parse_error = parsed_ok ? std::string() : parsed.error();
    REQUIRE_MESSAGE(parsed_ok, parse_error);
    const std::vector<anno::app::ScriptedCommand> commands = parsed.value();
    REQUIRE(commands.size() == 3);

    // The runner, with the script.
    anno::app::PlayRunner runner(std::move(*world_a));
    runner.set_script(commands);

    // The same commands applied directly, through run_turn, turn by turn.
    constexpr std::uint64_t kTurns = 12;
    const std::uint32_t steps = anno::sim::Timing{}.mp_steps_per_turn;
    const std::uint32_t step_ms = runner.step_ms();
    std::size_t changed_turns = 0;
    for (std::uint64_t index = 0; index < kTurns; ++index) {
        anno::sim::Turn turn;
        turn.index = index;
        turn.step_ms = step_ms;
        for (const anno::app::ScriptedCommand& c : commands) {
            if (c.turn == index) {
                turn.commands.push_back(c.command);
            }
        }
        try {
            std::vector<anno::app::SceneBuilding> before = anno::app::play_buildings(*world_b);
            anno::app::sort_buildings(before);
            anno::sim::run_turn(*world_b, nullptr, turn, steps);
            std::vector<anno::app::SceneBuilding> after = anno::app::play_buildings(*world_b);
            anno::app::sort_buildings(after);
            const bool changed = !anno::app::diff_sorted_buildings(before, after).empty();
            changed_turns += changed ? 1 : 0;
            for (const anno::sim::SimEvent& e : world_b->drain_events()) {
                if (e.kind == anno::sim::SimEventKind::CommandRejected) {
                    MESSAGE("turn " << index << " rejected: "
                                    << anno::sim::reject_name(static_cast<anno::sim::Reject>(e.code)));
                }
            }
            runner.run_n(1);
            // One publication (W4.3 correction 2): the snapshot of this turn is visible
            // only together with its batch, so the batch is waiting when the turn is.
            CHECK_MESSAGE(runner.snapshot()->turn == index, "snapshot turn " << index);
            const auto pending = runner.pending_batch_turns();
            const bool waiting = std::find(pending.begin(), pending.end(), index) != pending.end();
            CHECK_MESSAGE(waiting == changed, "batch for turn " << index << " changed " << changed);
        } catch (const std::exception& e) {
            const std::string what =
                "runner threw at turn " + std::to_string(index) + ": " + e.what();
            CHECK_MESSAGE(false, what);
            return;
        }
        CHECK_MESSAGE(runner.state_hash() == world_b->state_hash(), "turn " << index);
    }
    MESSAGE("turns that changed a building: " << changed_turns << " of " << kTurns);
}

TEST_CASE("play: --play takes the fixed map and places its islands") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path path =
        fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
    auto loaded = anno::app::load_play(*dir, path);
    const bool loaded_ok = loaded.ok();
    const std::string why = loaded_ok ? std::string() : loaded.error();
    REQUIRE_MESSAGE(loaded_ok, why);
    CHECK(loaded.value().fixed_map);
    CHECK(loaded.value().name == "lan5");
    CHECK(!loaded.value().placed.empty());
    // Every placed template is an island the sim placed.
    CHECK(loaded.value().placed.size() <= loaded.value().islands.size());
}

TEST_CASE("developer resources: a Build too expensive for the start is refused without them") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // The most expensive building type (gold, goods not considered) and the cheapest
    // one with no goods cost, on the same tile of the developer start.
    auto off = lan5_world(*dir, false);
    auto on = lan5_world(*dir, true);
    REQUIRE(off.has_value());
    REQUIRE(on.has_value());
    const auto origin = developer_origin(*off);
    REQUIRE(origin.has_value());
    std::optional<std::uint32_t> dearest;
    std::int64_t dearest_gold = -1;
    for (std::uint32_t id = 1; id <= 0xFFFFu; ++id) {
        if (!anno::app::anlage_is_building(*off, static_cast<std::uint16_t>(id))) {
            continue;
        }
        const anno::sim::BuildingType* type = off->types().find(anno::sim::TypeId{id});
        if (type != nullptr && static_cast<std::int64_t>(type->cost_gold) > dearest_gold) {
            dearest = id;
            dearest_gold = static_cast<std::int64_t>(type->cost_gold);
        }
    }
    REQUIRE(dearest.has_value());
    const auto cheapest = first_building_type(*on);
    REQUIRE(cheapest.has_value());

    const auto build = [&](anno::sim::World& world, std::uint32_t type) {
        anno::sim::Command cmd;
        cmd.issuer = anno::app::developer_player();
        anno::sim::BuildCmd body;
        // The developer player's city on the island of the origin.
        bool found = false;
        for (const auto& island : world.islands()) {
            if (!island || found) {
                continue;
            }
            for (const auto& city : island->cities) {
                if (city && city->owner == anno::app::developer_player() && !found) {
                    body.city = anno::sim::CityRef{island->index, city->number};
                    found = true;
                }
            }
        }
        REQUIRE(found);
        anno::sim::PlacementWire placement;
        placement.type = anno::sim::TypeId{type};
        placement.x = origin->x + 4;
        placement.y = origin->y + 4;
        placement.rotation = 0;
        body.placements.push_back(placement);
        cmd.body = std::move(body);
        return world.apply(cmd);
    };
    const anno::sim::ApplyResult refused = build(*off, *dearest);
    CHECK_MESSAGE(refused.status == anno::sim::Reject::CannotAfford,
                  "without resources the dearest build is refused as too expensive");
    const anno::sim::ApplyResult accepted = build(*on, *cheapest);
    CHECK_MESSAGE(accepted.ok(), "with resources the cheapest build is affordable");
}
