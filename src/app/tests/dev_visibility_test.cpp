// An applied command's change on its way to the screen, through the real pipeline (W4.4 round ee
// review, R2): lan5's developer start, the real runner, ScenePicture, PlayFollower and a
// FrameRenderer with chunk workers that this test can hold, and the CPU compositor as the frame
// (the window's CPU path: fresh = cached and not stale). The window's own wiring is begin_frame /
// frame_done around each frame, as viewer.cpp does; the GPU predicate (a usable copy, not
// dissolving) is the same call with another `usable`, exercised by the stage cases in
// visibility_test.cpp and, on the RTX, by the window runs. Present-call success is not display
// time. Skips when ANNO1503_GAME_DIR is unset.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
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
#include <anno/app/visibility.hpp>
#include <anno/render/frame.hpp>
#include <anno/test/dev_lan5.hpp>

namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;
using V = app::Visibility;

namespace {

/// The chunk workers wait here until it is opened (for good).
struct WorkerGate {
    std::mutex mu;
    std::condition_variable cv;
    bool open = false;
    void pass() {
        std::unique_lock lock(mu);
        cv.wait(lock, [this] { return open; });
    }
    void open_for_good() {
        {
            const std::lock_guard lock(mu);
            open = true;
        }
        cv.notify_all();
    }
};

struct Pipeline {
    std::vector<std::uint32_t> buffer;
    render::Rect view;
    std::size_t group = 1;
};

/// A plain (no goods) building type the picture draws: its id fits a PROD2 item and the scene's
/// Anlagen has an entry for it, so a placed one changes chunks (the footprint is the registry's).
std::optional<sim::TypeId> drawable_plain(const anno::test::Lan5& l, bool drawable = true) {
    for (const sim::TypeId t : l.setup.types) {
        const sim::BuildingType* type = l.setup.registry->find(t);
        const bool has_entry =
            t.v < app::kPictureAnlageIds && l.scene->anlagen.entry(t.v) != nullptr;
        if (type != nullptr && type->cost_goods.empty() &&
            (type->flags_word & sim::kRoadFlag) == 0 && has_entry == drawable) {
            return t;
        }
    }
    return std::nullopt;
}

bool within(std::chrono::milliseconds limit, const std::function<bool()>& done) {
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

TEST_CASE("visibility through the pipeline: held chunk workers keep a committed change not drawn, "
          "a failed present shows nothing, the presented frame equals a fresh reference, and the "
          "old object-list rule would have said drawn too early (game files)" *
          doctest::test_suite("dev_visibility")) {
    auto l = anno::test::lan5();
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto plain = drawable_plain(*l);
    REQUIRE(plain.has_value());
    const auto free = anno::test::free_tile(*l);
    REQUIRE(free.has_value());
    const sim::TileXY spot = *free;
    const auto [sx, sy] = anno::test::tile_centre(*l, spot);
    const render::Rect view{sx - 400, sy - 300, 800, 600};
    const std::size_t group = 1;
    const app::DevToolSetup setup = l->setup;
    std::vector<bool> classes(app::kPictureAnlageIds, false);
    for (std::size_t id = 0; id < classes.size(); ++id) {
        classes[id] = app::anlage_is_building(l->play.world, static_cast<std::uint16_t>(id));
    }
    app::ScenePicture picture(
        l->scene, l->buildings,
        [&classes](std::uint16_t id) { return id < classes.size() && classes[id]; }, 1);
    render::FrameRenderer frames(2);
    auto workers = std::make_unique<render::ChunkWorkers>(2);
    WorkerGate gate;
    workers->set_test_gate([&gate](const render::ChunkKey&, std::uint64_t) { gate.pass(); });
    struct Opener {
        WorkerGate& g;
        ~Opener() { g.open_for_good(); }
    } opener{gate}; // before the workers are destroyed (declared after them: destroyed first)
    frames.set_workers(std::move(workers));
    app::PlayRunner runner(std::move(l->play.world));
    app::PlayFollower follower(picture, runner);

    const auto fresh = [&frames](const render::ChunkKey& k) {
        return frames.chunk(k) != nullptr && frames.stale_generation(k) == 0;
    };
    std::vector<std::uint32_t> shown(static_cast<std::size_t>(view.w) * view.h, 0);
    // One frame as the window's CPU path draws it: the boundary, begin_frame, render, frame_done.
    const auto draw = [&](bool present_ok) {
        follower.follow(frames);
        const auto scene = picture.scene();
        follower.visibility().begin_frame(view, group);
        frames.render(scene, view, group, 0, render::WaterStyle::Original, shown.data());
        follower.visibility().frame_done(present_ok, fresh, fresh);
    };

    // Before the change: wait for the first chunks (workers open for this part), keep the frame.
    gate.open_for_good();
    for (int i = 0; i < 400 && (frames.busy() || frames.cached() == 0); ++i) {
        draw(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    draw(true);
    const std::vector<std::uint32_t> before = shown;
    // From here the workers wait again.
    {
        const std::lock_guard lock(gate.mu);
        gate.open = false;
    }

    // Command: a paid-free 2 x 2 building on the free tile, applied by turn T.
    const app::CommandId id = [&] {
        sim::BuildCmd b;
        b.city = *setup.city;
        b.placements.push_back(sim::PlacementWire{*plain, spot.x, spot.y, 0, 0, 0});
        sim::Command c;
        c.issuer = app::developer_player();
        c.body = std::move(b);
        return runner.post_command(std::move(c));
    }();
    runner.run_n(1);
    const app::CommandOutcome out = runner.outcome(id);
    REQUIRE(out.state == app::CommandOutcome::State::Applied);
    const std::uint64_t turn = out.turn;

    // The turn reaches the picture and the object list commits (the build is not gated here).
    REQUIRE(within(std::chrono::seconds(30), [&] {
        follower.follow(frames);
        return follower.visibility().classify(turn) != V::NotInPicture &&
               follower.visibility().classify(turn) != V::Committing;
    }));
    CHECK(follower.visibility().classify(turn) == V::AwaitingFrame);

    // The object-list build is committed and the picture is converged for the old rule...
    const bool old_rule_says_drawn =
        follower.followed_turn() && *follower.followed_turn() >= turn && picture.pending() == 0;
    CHECK(old_rule_says_drawn);
    // ...but a frame finds the change's chunks stale: held workers have not rebuilt them.
    draw(true);
    CHECK(follower.visibility().classify(turn) == V::ChunksStale);
    CHECK_FALSE(app::drawn_note(out, V::ChunksStale).empty());
    // The negative control: the old rule accepted exactly here (premature, checked above), the
    // new answer waits.
    CHECK(app::visibility_waiting(follower.visibility().classify(turn)));
    // A frame still shows the old picture: it equals the one before the change.
    CHECK(shown == before);

    // Open the workers. Their chunks arrive, and a failed present still shows nothing.
    gate.open_for_good();
    REQUIRE(within(std::chrono::seconds(60), [&] {
        draw(false); // every present call fails
        return follower.visibility().classify(turn) == V::NotPresented;
    }));
    CHECK(follower.visibility().classify(turn) == V::NotPresented);
    CHECK(app::visibility_waiting(follower.visibility().classify(turn)));
    // The next frame is presented: now it is shown, and only now.
    draw(true);
    CHECK(follower.visibility().classify(turn) == V::Visible);
    CHECK(app::drawn_note(out, V::Visible).empty());

    // The presented frame holds the change: it differs from the one before in a non-empty region
    // and equals a fresh reference (a new renderer, no cache, the same scene).
    std::size_t changed = 0;
    for (std::size_t i = 0; i < shown.size(); ++i) {
        changed += shown[i] != before[i] ? 1 : 0;
    }
    CHECK(changed > 100); // a 2 x 2 building's sprite
    render::FrameRenderer reference(2);
    reference.set_workers(std::make_unique<render::ChunkWorkers>(2));
    const auto scene = picture.scene();
    std::vector<std::uint32_t> expect(shown.size(), 0);
    for (int i = 0; i < 400; ++i) {
        const auto s =
            reference.render(scene, view, group, 0, render::WaterStyle::Original, expect.data());
        if (s.chunks_missing == 0 && !reference.busy()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    reference.render(scene, view, group, 0, render::WaterStyle::Original, expect.data());
    CHECK(shown == expect);
    // Sanity of the oracle: the reference is not the picture without the building.
    CHECK(expect != before);
    runner.stop();
}

TEST_CASE("visibility through the pipeline: a change outside the view is offscreen, never visible "
          "(game files)" *
          doctest::test_suite("dev_visibility")) {
    auto l = anno::test::lan5();
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto plain = drawable_plain(*l);
    REQUIRE(plain.has_value());
    const auto free = anno::test::free_tile(*l);
    REQUIRE(free.has_value());
    const app::DevToolSetup setup = l->setup;
    std::vector<bool> classes(app::kPictureAnlageIds, false);
    for (std::size_t id = 0; id < classes.size(); ++id) {
        classes[id] = app::anlage_is_building(l->play.world, static_cast<std::uint16_t>(id));
    }
    app::ScenePicture picture(
        l->scene, l->buildings,
        [&classes](std::uint16_t id) { return id < classes.size() && classes[id]; }, 1);
    render::FrameRenderer frames(2);
    app::PlayRunner runner(std::move(l->play.world));
    app::PlayFollower follower(picture, runner);
    sim::BuildCmd b;
    b.city = *setup.city;
    b.placements.push_back(sim::PlacementWire{*plain, free->x, free->y, 0, 0, 0});
    sim::Command c;
    c.issuer = app::developer_player();
    c.body = std::move(b);
    const app::CommandId id = runner.post_command(std::move(c));
    runner.run_n(1);
    const app::CommandOutcome out = runner.outcome(id);
    REQUIRE(out.state == app::CommandOutcome::State::Applied);
    // A view far from the island: nothing the change touched is in it.
    const render::Rect away{-200000, -200000, 800, 600};
    const auto fresh = [&frames](const render::ChunkKey& k) {
        return frames.chunk(k) != nullptr && frames.stale_generation(k) == 0;
    };
    std::vector<std::uint32_t> shown(static_cast<std::size_t>(away.w) * away.h, 0);
    REQUIRE(within(std::chrono::seconds(30), [&] {
        follower.follow(frames);
        follower.visibility().begin_frame(away, 1);
        frames.render(picture.scene(), away, 1, 0, render::WaterStyle::Original, shown.data());
        follower.visibility().frame_done(true, fresh, fresh);
        return !app::visibility_waiting(follower.visibility().classify(out.turn));
    }));
    CHECK(follower.visibility().classify(out.turn) == V::Offscreen);
    CHECK_FALSE(app::drawn_note(out, V::Offscreen).empty()); // it says so; it is not "shown"
    runner.stop();
}
