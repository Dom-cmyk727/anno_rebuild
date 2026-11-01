// Command outcomes (W4.4 round ee, package A; docs/app/game-shell.md, "Command outcomes"): every
// command has a receipt, the turn and position it ran at, and the result sim::run_turn returned
// for it; the bounded store says what a slow reader lost. The runner cases load lan5 and the
// original registry and skip when ANNO1503_GAME_DIR is unset.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/command_outcome.hpp>
#include <anno/app/commands_file.hpp>
#include <anno/app/play.hpp>
#include <anno/sim/command.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;
namespace sim = anno::sim;
namespace app = anno::app;
using State = app::CommandOutcome::State;

namespace {

app::CommandOutcome ran(app::CommandOrigin origin, std::uint64_t n, std::uint64_t turn,
                        std::size_t records = 0) {
    app::CommandOutcome o;
    o.id = {origin, n};
    o.state = State::Applied;
    o.turn = turn;
    o.records.assign(records, sim::Reject::None);
    return o;
}

/// lan5 with the developer start, the developer player's city on its start island and that
/// island's origin, and the cheapest building type that needs no goods.
struct DevWorld {
    app::PlayLoad load;
    sim::CityRef city;
    sim::TileXY origin;
    sim::TypeId cheap;
};

std::optional<DevWorld> dev_world() {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return std::nullopt;
    }
    const fs::path path =
        fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
    auto loaded = app::load_play(*dir, path);
    REQUIRE(loaded.ok());
    DevWorld d{std::move(loaded.value()), {}, {}, {}};
    bool found = false;
    for (const auto& island : d.load.world.islands()) {
        if (!island || found) {
            continue;
        }
        for (const auto& c : island->cities) {
            if (c && c->owner == app::developer_player() && !found) {
                d.city = sim::CityRef{island->index, c->number};
                d.origin = sim::TileXY{island->tiles.x0(), island->tiles.y0()};
                found = true;
            }
        }
    }
    REQUIRE(found);
    std::int64_t gold = -1;
    for (std::uint32_t id = 1; id <= 0xFFFFu; ++id) {
        const sim::BuildingType* type = d.load.world.types().find(sim::TypeId{id});
        if (type == nullptr || !type->cost_goods.empty() ||
            !app::anlage_is_building(d.load.world, static_cast<std::uint16_t>(id))) {
            continue;
        }
        if (gold < 0 || static_cast<std::int64_t>(type->cost_gold) < gold) {
            d.cheap = sim::TypeId{id};
            gold = static_cast<std::int64_t>(type->cost_gold);
        }
    }
    REQUIRE(gold >= 0);
    return d;
}

sim::Command build(const DevWorld& d, std::int32_t x, std::int32_t y,
                   std::optional<sim::TypeId> type = std::nullopt) {
    sim::BuildCmd b;
    b.city = d.city;
    sim::PlacementWire p;
    p.type = type.value_or(d.cheap);
    p.x = d.origin.x + x;
    p.y = d.origin.y + y;
    b.placements.push_back(p);
    sim::Command c;
    c.issuer = app::developer_player();
    c.body = std::move(b);
    return c;
}

sim::Command demolish(const DevWorld& d, std::int32_t x, std::int32_t y) {
    sim::Command c;
    c.issuer = app::developer_player();
    c.body = sim::DemolishCmd{d.city, {sim::TileXY{d.origin.x + x, d.origin.y + y}}};
    return c;
}

template <class F> bool eventually(F done, int seconds = 20) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (!done()) {
        if (std::chrono::steady_clock::now() > until) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

} // namespace

TEST_CASE("outcome log: a reader that falls behind is told exactly what it lost, and every "
          "receipt has a state" *
          doctest::test_suite("command_outcome")) {
    app::OutcomeLog log(4, 100);
    log.issued(app::CommandOrigin::Window, 11);
    for (std::uint64_t n = 1; n <= 10; ++n) {
        log.record(ran(app::CommandOrigin::Window, n, n));
    }
    const app::OutcomeRead behind = log.since(1);
    CHECK(behind.lost_from == 1);
    CHECK(behind.lost == 6);
    REQUIRE(behind.outcomes.size() == 4);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(behind.outcomes[i].seq == 7 + i);
        CHECK(behind.outcomes[i].id.n == 7 + i);
    }
    CHECK(behind.next == 11);
    // A reader that keeps up loses nothing.
    const app::OutcomeRead current = log.since(9);
    CHECK(current.lost == 0);
    CHECK(current.outcomes.size() == 2);
    CHECK(log.since(11).outcomes.empty());
    // Each receipt's state now.
    CHECK(log.find({app::CommandOrigin::Window, 1}).state == State::Lost);
    CHECK(log.find({app::CommandOrigin::Window, 10}).state == State::Applied);
    CHECK(log.find({app::CommandOrigin::Window, 11}).state == State::Queued);
    CHECK(log.find({app::CommandOrigin::Window, 12}).state == State::Unknown);
    CHECK(log.find({app::CommandOrigin::Window, 0}).state == State::Unknown);
    CHECK(log.find({app::CommandOrigin::Script, 1}).state == State::Unknown);
    log.issued(app::CommandOrigin::Script, 2);
    CHECK(log.find({app::CommandOrigin::Script, 1}).state == State::Queued);
}

TEST_CASE("outcome log: the per-record bound evicts the oldest, and the newest is always kept" *
          doctest::test_suite("command_outcome")) {
    app::OutcomeLog log(100, 10);
    log.record(ran(app::CommandOrigin::Window, 1, 0, 4));
    log.record(ran(app::CommandOrigin::Window, 2, 0, 4));
    CHECK(log.since(1).lost == 0);
    log.record(ran(app::CommandOrigin::Window, 3, 0, 4)); // 12 records: the first goes
    const app::OutcomeRead r = log.since(1);
    CHECK(r.lost == 1);
    CHECK(r.outcomes.size() == 2);
    log.record(ran(app::CommandOrigin::Window, 4, 0, 50)); // larger than the bound alone
    const app::OutcomeRead big = log.since(1);
    REQUIRE(big.outcomes.size() == 1);
    CHECK(big.outcomes[0].id.n == 4);
    CHECK(big.lost == 3);
    // Narrower bounds later evict what no longer fits, the same way.
    log.set_bounds(1, 0);
    CHECK(log.since(4).outcomes.size() == 1);
}

TEST_CASE("command outcomes: one turn's mixed outcomes, identical commands and script before "
          "window order, each with its receipt, position and reason" *
          doctest::test_suite("command_outcome")) {
    auto d = dev_world();
    if (!d) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const DevWorld w = *d;
    app::PlayRunner runner(std::move(d->load.world));
    // Turn 0: the script's build at (4, 4), then the window's: the same build again (refused:
    // the tile is taken now), a build elsewhere, a type that does not exist, and the same build
    // elsewhere again (refused). Identical commands, different outcomes.
    runner.set_script({app::ScriptedCommand{0, build(w, 4, 4)}});
    const app::CommandId same = runner.post_command(build(w, 4, 4));
    const app::CommandId other = runner.post_command(build(w, 12, 4));
    const app::CommandId unknown = runner.post_command(build(w, 20, 4, sim::TypeId{0xFFFF}));
    const app::CommandId repeat = runner.post_command(build(w, 12, 4));
    CHECK(same == app::CommandId{app::CommandOrigin::Window, 1});
    CHECK(repeat == app::CommandId{app::CommandOrigin::Window, 4});
    CHECK(runner.outcome(same).state == State::Queued);
    runner.run_n(1);
    const app::OutcomeRead read = runner.outcomes_since(1);
    CHECK(read.lost == 0);
    REQUIRE(read.outcomes.size() == 5);
    const std::vector<app::CommandId> ids{
        {app::CommandOrigin::Script, 1}, same, other, unknown, repeat};
    const std::vector<State> states{State::Applied, State::Refused, State::Applied, State::Refused,
                                    State::Refused};
    for (std::size_t i = 0; i < read.outcomes.size(); ++i) {
        CAPTURE(i);
        const app::CommandOutcome& o = read.outcomes[i];
        CHECK(o.id == ids[i]);
        CHECK(o.state == states[i]);
        CHECK(o.turn == 0);
        CHECK(o.position == i);
        CHECK(o.seq == i + 1);
        CHECK(runner.outcome(ids[i]).seq == o.seq);
        REQUIRE(o.records.size() == 1);
        CHECK((o.records[0] == sim::Reject::None) == (o.state == State::Applied));
        CHECK(o.status == (o.state == State::Applied ? sim::Reject::None : o.records[0]));
    }
    CHECK(read.outcomes[3].status == sim::Reject::UnknownType);
    // The two refused repeats name the same reason, a taken tile, not the unknown type.
    CHECK(read.outcomes[1].status == read.outcomes[4].status);
    CHECK(read.outcomes[1].status != sim::Reject::UnknownType);
    CHECK(runner.snapshot()->outcomes_next == 6);
    MESSAGE("a repeated build is refused as " << sim::reject_name(read.outcomes[1].status));
}

TEST_CASE("command outcomes: more outcomes than the bound, and a reader that waits, lose only "
          "what they are told they lost" *
          doctest::test_suite("command_outcome")) {
    auto d = dev_world();
    if (!d) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const DevWorld w = *d;
    app::PlayRunner runner(std::move(d->load.world));
    runner.set_outcome_bounds(4, 1000);
    std::vector<app::CommandId> ids;
    for (std::int32_t i = 0; i < 10; ++i) {
        ids.push_back(runner.post_command(build(w, 4 + 6 * (i % 5), 4 + 6 * (i / 5))));
        runner.run_n(1); // one command a turn; the reader waits until the end
    }
    const app::OutcomeRead read = runner.outcomes_since(1);
    CHECK(read.lost_from == 1);
    CHECK(read.lost == 6);
    REQUIRE(read.outcomes.size() == 4);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(read.outcomes[i].id == ids[6 + i]);
        CHECK(read.outcomes[i].turn == 6 + i);
        CHECK(read.outcomes[i].seq == 7 + i);
    }
    CHECK(runner.outcome(ids[0]).state == State::Lost);
    CHECK(runner.outcome(ids[5]).state == State::Lost);
    CHECK(runner.outcome(ids[9]).state != State::Lost);
    const app::CommandId waiting = runner.post_command(build(w, 30, 30));
    CHECK(runner.outcome(waiting).state == State::Queued);
    CHECK(runner.outcome({app::CommandOrigin::Window, waiting.n + 1}).state == State::Unknown);
}

TEST_CASE("command outcomes: a command posted while a turn is held at its gate waits for the "
          "next turn, the post does not wait, and outcomes wake the window" *
          doctest::test_suite("command_outcome")) {
    auto d = dev_world();
    if (!d) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const DevWorld w = *d;
    app::PlayRunner runner(std::move(d->load.world));
    std::mutex m;
    std::condition_variable cv;
    bool held = false, open = false;
    std::optional<std::uint64_t> held_turn;
    runner.set_turn_gate([&](std::uint64_t turn) {
        std::unique_lock lock(m);
        if (held_turn || open) {
            return;
        }
        held_turn = turn;
        held = true;
        cv.notify_all();
        cv.wait(lock, [&] { return open; });
    });
    std::atomic<int> wakes{0};
    runner.set_wake_listener([&wakes] { wakes.fetch_add(1); });
    struct Release {
        std::mutex& m;
        std::condition_variable& cv;
        bool& open;
        app::PlayRunner& runner;
        ~Release() {
            {
                const std::lock_guard lock(m);
                open = true;
            }
            cv.notify_all();
            runner.stop();
        }
    } release{m, cv, open, runner};
    const app::CommandId first = runner.post_command(build(w, 4, 4));
    runner.start();
    {
        std::unique_lock lock(m);
        REQUIRE(cv.wait_for(lock, std::chrono::seconds(20), [&] { return held; }));
    }
    // Turn `held_turn` is in flight with the first command; the second arrives now.
    const auto t0 = std::chrono::steady_clock::now();
    const app::CommandId second = runner.post_command(build(w, 12, 4));
    const auto post_us =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0)
            .count();
    CHECK(post_us < 100000); // the turn is held; the post must not wait for it
    CHECK(runner.outcome(first).state == State::Queued); // taken, not run yet
    CHECK(runner.outcome(second).state == State::Queued);
    {
        const std::lock_guard lock(m);
        open = true;
    }
    cv.notify_all();
    REQUIRE(eventually([&] { return runner.outcome(second).state != State::Queued; }));
    const app::CommandOutcome a = runner.outcome(first), b = runner.outcome(second);
    CHECK(a.state == State::Applied);
    CHECK(b.state == State::Applied);
    CHECK(a.turn == *held_turn);
    CHECK(b.turn > a.turn);
    CHECK(a.seq + 1 == b.seq);
    CHECK(wakes.load() >= 2);
    MESSAGE("post while held: " << post_us << " us; turns " << a.turn << ", " << b.turn);
}

TEST_CASE("command outcomes: a command with no wire form is refused as Unencodable and changes "
          "nothing; a refusal and a no-op change no picture and still wake the window" *
          doctest::test_suite("command_outcome")) {
    auto d = dev_world();
    auto twin = dev_world();
    if (!d || !twin) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const DevWorld w = *d;
    app::PlayRunner runner(std::move(d->load.world));
    app::PlayRunner control(std::move(twin->load.world));
    int wakes = 0;
    runner.set_wake_listener([&wakes] { ++wakes; });
    // 4 096 placements: one more than a Build record holds.
    sim::Command huge = build(w, 4, 4);
    auto& placements = std::get<sim::BuildCmd>(huge.body).placements;
    placements.assign(4096, placements.front()); // has_wire_form: at most 4 095
    REQUIRE_FALSE(sim::has_wire_form(huge.body));
    const app::CommandId too_long = runner.post_command(huge);
    runner.run_n(1);
    control.run_n(1);
    const app::CommandOutcome o = runner.outcome(too_long);
    CHECK(o.state == State::Refused);
    CHECK(o.status == sim::Reject::Unencodable);
    CHECK(o.records.empty());
    CHECK(runner.state_hash() == control.state_hash());
    CHECK(runner.applied_commands().empty()); // never given to the sim
    CHECK(runner.take_building_batches().empty());
    CHECK(wakes == 1);
    // A refusal: nothing changes, no batch; the window still wakes.
    const app::CommandId placed = runner.post_command(build(w, 4, 4));
    runner.run_n(1);
    REQUIRE(runner.outcome(placed).state == State::Applied);
    CHECK(runner.take_building_batches().size() == 1);
    const int before = wakes;
    const app::CommandId refused = runner.post_command(build(w, 4, 4));
    runner.run_n(1);
    CHECK(runner.outcome(refused).state == State::Refused);
    CHECK(runner.take_building_batches().empty());
    CHECK(wakes == before + 1);
    // A demolish of an owned tile with nothing on it: accepted, nothing removed, no batch, a wake.
    const app::CommandId nothing = runner.post_command(demolish(w, 30, 4));
    runner.run_n(1);
    const app::CommandOutcome n = runner.outcome(nothing);
    CHECK(n.state == State::Applied);
    REQUIRE(n.records.size() == 1);
    CHECK(n.records[0] != sim::Reject::None);
    CHECK(n.records_done() == 0);
    CHECK(runner.take_building_batches().empty());
    CHECK(wakes == before + 2);
    // A turn without commands and without a change does not wake it.
    runner.run_n(1);
    CHECK(wakes == before + 2);
    MESSAGE("empty-tile demolish record: " << sim::reject_name(n.records[0]));
}
