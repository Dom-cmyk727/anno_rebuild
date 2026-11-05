// The developer tool while the picture is held at a barrier (W4.4 round ee, E4): lan5's developer
// start, the real runner on its own thread, the real ScenePicture and the window's follow step
// (PlayFollower). Every picture build waits at a gate (PictureLimits::build_fault, called on the
// worker outside the picture's lock). Input, the sim's turns and the outcomes go on; the follow
// step never waits; an applied building is not drawn yet and the line says so; opening the gate
// draws it. Skips when ANNO1503_GAME_DIR is unset.
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/dev_feedback.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/play.hpp>
#include <anno/app/play_follow.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/render/frame.hpp>
#include <anno/test/dev_lan5.hpp>

namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;

namespace {

/// The picture's builds wait here until it is opened (for good).
struct BuildGate {
    std::mutex mu;
    std::condition_variable cv;
    bool open = false;
    std::size_t waiting = 0;
    void pass() {
        std::unique_lock lock(mu);
        ++waiting;
        cv.wait(lock, [this] { return open; });
        --waiting;
    }
    void open_for_good() {
        {
            const std::lock_guard lock(mu);
            open = true;
        }
        cv.notify_all();
    }
    std::size_t waiters() {
        const std::lock_guard lock(mu);
        return waiting;
    }
};

/// Opens the gate before the picture is destroyed (declared after it), so no worker is left
/// waiting when the picture joins its threads.
struct GateOpener {
    BuildGate& gate;
    explicit GateOpener(BuildGate& g) : gate(g) {}
    GateOpener(const GateOpener&) = delete;
    GateOpener& operator=(const GateOpener&) = delete;
    ~GateOpener() { gate.open_for_good(); }
};

template <class F> bool within(std::chrono::milliseconds limit, F done) {
    const auto end = std::chrono::steady_clock::now() + limit;
    while (!done()) {
        if (std::chrono::steady_clock::now() > end) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
}

} // namespace

TEST_CASE("dev tool at a picture barrier: input, turns and outcomes go on, an applied building is "
          "shown as not drawn yet, and the open gate draws it (game files)" *
          doctest::test_suite("dev_barrier")) {
    auto l = anno::test::lan5();
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::optional<sim::TypeId> plain;
    for (const sim::TypeId t : l->setup.types) {
        const sim::BuildingType* type = l->setup.registry->find(t);
        if (!plain && type != nullptr && type->cost_goods.empty() && type->width == 1 &&
            type->height == 1 && (type->flags_word & sim::kRoadFlag) == 0) {
            plain = t;
        }
    }
    REQUIRE(plain.has_value());
    const auto free = anno::test::free_tile(*l);
    REQUIRE(free.has_value());
    app::DevTool tool = anno::test::tool_at(*l, *plain, *free);
    const app::DevToolSetup setup = l->setup;
    std::vector<bool> classes(app::kPictureAnlageIds, false);
    for (std::size_t id = 0; id < classes.size(); ++id) {
        classes[id] = app::anlage_is_building(l->play.world, static_cast<std::uint16_t>(id));
    }

    BuildGate gate;
    app::PictureLimits limits;
    limits.build_fault = [&gate](std::size_t) { gate.pass(); };
    app::ScenePicture picture(
        l->scene, l->buildings,
        [&classes](std::uint16_t id) { return id < classes.size() && classes[id]; }, 1, limits);
    const GateOpener opener(gate);
    render::FrameRenderer frames(2);
    app::PlayRunner runner(std::move(l->play.world));
    app::PlayFollower follower(picture, runner);
    app::PlacementKnowledge knows(setup);
    const auto before = picture.scene()->generation();
    runner.start();

    const auto build = [&](sim::TileXY at) {
        sim::Command cmd;
        cmd.issuer = setup.issuer;
        sim::BuildCmd b;
        b.city = *setup.city;
        b.placements.push_back(sim::PlacementWire{*plain, at.x, at.y, 0, 0, 0});
        cmd.body = std::move(b);
        return cmd;
    };
    const app::CommandId first = runner.post_command(build(*free));
    REQUIRE(within(std::chrono::seconds(20), [&] {
        return runner.outcome(first).state == app::CommandOutcome::State::Applied;
    }));
    const app::CommandOutcome applied = runner.outcome(first);
    // Before the window's next follow step: the command's turn is not in the picture.
    const std::string not_in =
        app::drawn_note(applied, follower.visibility().classify(applied.turn));
    CHECK_MESSAGE(not_in.find("not drawn yet") != std::string::npos, not_in);
    // The follow step takes the batch and returns at once although every build waits at the gate.
    std::size_t committed = 0;
    std::chrono::duration<double, std::milli> slowest{0};
    REQUIRE(within(std::chrono::seconds(20), [&] {
        const auto t0 = std::chrono::steady_clock::now();
        committed += follower.follow(frames);
        slowest = std::max(slowest, std::chrono::duration<double, std::milli>(
                                        std::chrono::steady_clock::now() - t0));
        return follower.followed_turn() && *follower.followed_turn() >= applied.turn &&
               gate.waiters() > 0;
    }));
    CHECK(committed == 0);
    CHECK(picture.pending() > 0);
    CHECK(picture.scene()->generation() == before);
    // Desired, not committed: the object list build waits at the gate (the Committing stage).
    CHECK(follower.visibility().classify(applied.turn) == app::Visibility::Committing);
    const std::string held = app::drawn_note(applied, follower.visibility().classify(applied.turn));
    CHECK_MESSAGE(held.find("still being rebuilt") != std::string::npos, held);
    // The occupancy the window knows already has it (the desired state), so the same tile is a
    // known refusal; the sim agrees when asked.
    CHECK(knows.update(picture.buildings(), picture.buildings_version(), *picture.scene()));
    CHECK(knows.occupied(*free));
    // Input goes on: the tool moves, the next command is executed and answered while held.
    static_cast<void>(tool.handle({app::ToolInput::Kind::Rotate, 0, 0}, nullptr));
    CHECK(tool.input().rotation() == 1);
    const app::CommandId again = runner.post_command(build(*free));
    REQUIRE(within(std::chrono::seconds(20), [&] {
        return runner.outcome(again).state == app::CommandOutcome::State::Refused;
    }));
    CHECK(runner.outcome(again).status == sim::Reject::Occupied);
    const auto snap = runner.snapshot();
    const std::string title =
        app::dev_title(*snap, tool, knows.check(tool, snap.get()), applied, held);
    CHECK(title.find("command #" + std::to_string(first.n) + ": Applied") != std::string::npos);
    CHECK(title.find("not drawn yet") != std::string::npos);
    MESSAGE("held: " << title << "; slowest follow step " << slowest.count() << " ms");
    CHECK(slowest.count() < 1000.0);

    // The gate opens: the picture commits, the line no longer says pending.
    gate.open_for_good();
    REQUIRE(within(std::chrono::seconds(30), [&] {
        committed += follower.follow(frames);
        return picture.pending() == 0 && committed > 0;
    }));
    CHECK(picture.scene()->generation() > before);
    // Committed is not drawn (round ee review, R2): no frame has looked at the change, so the
    // line still says so. The frames that show it are in dev_visibility_test.cpp.
    CHECK(follower.visibility().classify(applied.turn) == app::Visibility::AwaitingFrame);
    CHECK_FALSE(app::drawn_note(applied, follower.visibility().classify(applied.turn)).empty());
    runner.stop();
}
