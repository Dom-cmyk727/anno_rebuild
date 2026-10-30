// Developer input (W4.4 B6): the command queue keeps order and never blocks the window
// (measured); the picker turns a synthetic event sequence into the expected commands;
// a recorded session replays through --commands to the same state hash. The game
// cases load lan5 and the original registry and skip when ANNO1503_GAME_DIR is unset.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/command_queue.hpp>
#include <anno/app/commands_file.hpp>
#include <anno/app/dev_input.hpp>
#include <anno/app/play.hpp>
#include <anno/formats/fixed_map.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim_load/fixed_map.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;
namespace sim = anno::sim;

namespace {

sim::Command demolish_at(std::int32_t x, std::int32_t y) {
    sim::Command cmd;
    cmd.issuer = anno::app::developer_player();
    cmd.body =
        sim::DemolishCmd{sim::CityRef{sim::IslandId{0}, sim::CityNo{0}}, {sim::TileXY{x, y}}};
    return cmd;
}

std::int32_t tile_x(const sim::Command& cmd) {
    return std::get<sim::DemolishCmd>(cmd.body).tiles.front().x;
}

} // namespace

TEST_CASE("command queue: order is push order, across takes") {
    anno::app::CommandQueue queue;
    constexpr std::int32_t kCount = 5000;
    std::vector<std::int32_t> taken;
    std::thread window([&] {
        for (std::int32_t i = 0; i < kCount; ++i) {
            queue.push(demolish_at(i, 0));
        }
    });
    // The sim side takes while the window pushes; every take is in push order.
    while (taken.size() < static_cast<std::size_t>(kCount)) {
        for (const sim::Command& cmd : queue.take()) {
            taken.push_back(tile_x(cmd));
        }
    }
    window.join();
    for (const sim::Command& cmd : queue.take()) {
        taken.push_back(tile_x(cmd));
    }
    REQUIRE(taken.size() == static_cast<std::size_t>(kCount));
    bool in_order = true;
    for (std::size_t i = 0; i < taken.size(); ++i) {
        in_order = in_order && taken[i] == static_cast<std::int32_t>(i);
    }
    CHECK(in_order);
}

TEST_CASE("command queue: a push never waits for a turn (measured)") {
    anno::app::CommandQueue queue;
    // The sim side takes once per 2 ms, as a turn's start would. The lock is held only
    // for a swap, so a push waits for at most that.
    std::atomic<bool> stop{false};
    std::thread sim([&] {
        while (!stop.load()) {
            (void)queue.take();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    std::vector<double> us;
    us.reserve(20000);
    for (std::int32_t i = 0; i < 20000; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        queue.push(demolish_at(i, 1));
        us.push_back(
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                .count());
    }
    stop.store(true);
    sim.join();
    std::sort(us.begin(), us.end());
    const double p99 = us[us.size() * 99 / 100];
    MESSAGE("push, 20000 pushes against a take every 2 ms: p50 "
            << us[us.size() / 2] << " us, p99 " << p99 << " us, max " << us.back() << " us");
    // A turn is 100 ms: a push that waited for a turn would be far above this.
    CHECK(us.back() < 20000.0);
}

TEST_CASE("picker: a synthetic event sequence gives the expected commands") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto registry =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types != nullptr, registry.error);
    // A non-road type and a road type from the registry, in id order.
    std::optional<sim::TypeId> plain, road;
    for (std::uint32_t id = 1; id <= 0xFFFFu && !(plain && road); ++id) {
        const sim::BuildingType* type = registry.types->find(sim::TypeId{id});
        if (type == nullptr) {
            continue;
        }
        const bool is_road = (type->flags_word & sim::kRoadFlag) != 0;
        if (is_road && !road) {
            road = sim::TypeId{id};
        } else if (!is_road && !plain) {
            plain = sim::TypeId{id};
        }
    }
    REQUIRE(plain.has_value());
    if (!road) {
        MESSAGE("skipping the road part: the registry has no road type");
        return;
    }
    const sim::CityRef city{sim::IslandId{0}, sim::CityNo{1}};
    anno::app::DevInput input(registry.types, {*plain, *road}, city, anno::app::developer_player());
    using K = anno::app::DevEvent::Kind;
    const auto ev = [](K kind, std::int32_t x = 0, std::int32_t y = 0) {
        anno::app::DevEvent e;
        e.kind = kind;
        e.tile = sim::TileXY{x, y};
        return e;
    };
    std::vector<sim::Command> got;
    const auto feed = [&](const anno::app::DevEvent& e) {
        for (sim::Command& cmd : input.handle(e)) {
            got.push_back(std::move(cmd));
        }
    };

    feed(ev(K::NextType));      // the road
    feed(ev(K::Rotate));        // rotation 1
    feed(ev(K::Press, 10, 10)); // a click on the road: one placement
    feed(ev(K::Release, 10, 10));
    feed(ev(K::Press, 20, 5)); // a road drag along x: 4 placements in stream order
    feed(ev(K::Release, 23, 5));
    feed(ev(K::Press, 30, 30)); // a road drag along y, upwards
    feed(ev(K::Release, 30, 27));
    feed(ev(K::PrevType));      // the plain type
    feed(ev(K::Press, 40, 40)); // a drag of a non-road: refused with a message
    feed(ev(K::Release, 42, 40));
    CHECK(got.size() == 3);
    CHECK_MESSAGE(!input.message().empty(), "a non-road drag names the refusal");
    feed(ev(K::ToggleDemolish)); // demolish mode: a click is one DemolishCmd
    feed(ev(K::Press, 5, 5));
    feed(ev(K::Release, 5, 5));
    feed(ev(K::Cancel));      // leaves the mode
    feed(ev(K::Press, 6, 6)); // a plain click places a plain building
    feed(ev(K::Release, 6, 6));
    REQUIRE(got.size() == 5);

    const auto& build0 = std::get<sim::BuildCmd>(got[0].body);
    CHECK(build0.placements.size() == 1);
    CHECK(build0.placements.front().type == *road);
    CHECK(build0.placements.front().rotation == 1);
    CHECK(build0.placements.front().x == 10);

    const auto& build1 = std::get<sim::BuildCmd>(got[1].body);
    REQUIRE(build1.placements.size() == 4);
    CHECK(build1.placements[0].x == 20);
    CHECK(build1.placements[3].x == 23);
    CHECK(build1.placements[0].y == 5);

    const auto& build2 = std::get<sim::BuildCmd>(got[2].body);
    REQUIRE(build2.placements.size() == 4);
    CHECK(build2.placements[0].y == 30);
    CHECK(build2.placements[3].y == 27);

    const auto& demolish = std::get<sim::DemolishCmd>(got[3].body);
    REQUIRE(demolish.tiles.size() == 1);
    CHECK(demolish.tiles.front().x == 5);
    CHECK(demolish.tiles.front().y == 5);
    CHECK(demolish.city == city);

    const auto& plain_build = std::get<sim::BuildCmd>(got[4].body);
    CHECK(plain_build.placements.size() == 1);
    CHECK(plain_build.placements.front().type == *plain);
}

TEST_CASE("replay: a recorded session replays through --commands to the same state hash") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path path =
        fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
    auto first = anno::app::load_play(*dir, path);
    auto second = anno::app::load_play(*dir, path);
    REQUIRE(first.ok());
    REQUIRE(second.ok());
    // The developer player's city on the start island, and a cheap building type.
    const auto& islands = first.value().world.islands();
    std::optional<sim::CityRef> city;
    std::optional<sim::TileXY> origin;
    for (std::size_t i = 0; i < islands.size() && !city; ++i) {
        if (!islands[i]) {
            continue;
        }
        for (const auto& c : islands[i]->cities) {
            if (c && c->owner == anno::app::developer_player() && !city) {
                city = sim::CityRef{islands[i]->index, c->number};
                origin = sim::TileXY{islands[i]->tiles.x0(), islands[i]->tiles.y0()};
            }
        }
    }
    REQUIRE(city.has_value());
    std::optional<std::uint32_t> cheap;
    std::int64_t cheap_gold = 0;
    for (std::uint32_t id = 1; id <= 0xFFFFu; ++id) {
        const sim::BuildingType* type = first.value().world.types().find(sim::TypeId{id});
        if (type == nullptr || !type->cost_goods.empty() ||
            !anno::app::anlage_is_building(first.value().world, static_cast<std::uint16_t>(id))) {
            continue;
        }
        if (!cheap || static_cast<std::int64_t>(type->cost_gold) < cheap_gold) {
            cheap = id;
            cheap_gold = static_cast<std::int64_t>(type->cost_gold);
        }
    }
    REQUIRE(cheap.has_value());

    anno::app::PlayRunner runner(std::move(first.value().world));
    const auto post_build = [&](std::int32_t x, std::int32_t y) {
        sim::Command cmd;
        cmd.issuer = anno::app::developer_player();
        sim::BuildCmd build;
        build.city = *city;
        sim::PlacementWire placement;
        placement.type = sim::TypeId{*cheap};
        placement.x = origin->x + x;
        placement.y = origin->y + y;
        placement.rotation = 0;
        build.placements.push_back(placement);
        cmd.body = std::move(build);
        return cmd;
    };
    // The window's input: a build, two turns later a demolish of the same tile.
    std::vector<std::uint64_t> hashes;
    runner.post_command(post_build(4, 4));
    runner.run_n(1);
    hashes.push_back(runner.state_hash());
    runner.run_n(1);
    hashes.push_back(runner.state_hash());
    sim::Command demolish;
    demolish.issuer = anno::app::developer_player();
    demolish.body = sim::DemolishCmd{*city, {sim::TileXY{origin->x + 4, origin->y + 4}}};
    runner.post_command(std::move(demolish));
    runner.run_n(1);
    hashes.push_back(runner.state_hash());
    runner.run_n(1);
    hashes.push_back(runner.state_hash());

    // The record, as text, parsed back against the second world.
    const std::vector<anno::app::ScriptedCommand> record = runner.applied_commands();
    REQUIRE(record.size() == 2);
    auto text = anno::app::format_commands(record);
    const bool text_ok = text.ok();
    const std::string text_error = text_ok ? std::string() : text.error().message;
    REQUIRE_MESSAGE(text_ok, text_error);
    auto parsed = anno::app::parse_commands(text.value(), second.value().world,
                                            anno::app::developer_player());
    const bool parsed_ok = parsed.ok();
    const std::string parsed_error = parsed_ok ? std::string() : parsed.error();
    REQUIRE_MESSAGE(parsed_ok, parsed_error);
    REQUIRE(parsed.value().size() == record.size());
    for (std::size_t i = 0; i < record.size(); ++i) {
        CHECK(parsed.value()[i].turn == record[i].turn);
        CHECK(parsed.value()[i].command == record[i].command);
    }

    // Replayed directly, turn by turn, the same hashes.
    const std::uint32_t steps = sim::Timing{}.mp_steps_per_turn;
    const std::uint32_t step_ms = runner.step_ms();
    std::vector<std::uint64_t> replay;
    for (std::uint64_t index = 0; index < hashes.size(); ++index) {
        sim::Turn turn;
        turn.index = index;
        turn.step_ms = step_ms;
        for (const anno::app::ScriptedCommand& c : parsed.value()) {
            if (c.turn == index) {
                turn.commands.push_back(c.command);
            }
        }
        sim::run_turn(second.value().world, nullptr, turn, steps);
        replay.push_back(second.value().world.state_hash());
    }
    CHECK(replay == hashes);
}
