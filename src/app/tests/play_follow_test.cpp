// The window's play scheduling without a window (round eb, 1A): a turn already running when the
// window pauses publishes an accepted building change after the window went idle. The production
// follow step (PlayFollower) and idle demand (FrameDemand) that the viewer calls must notice it
// with no input event, finish the change's chunks while paused, keep the sim paused, and return to
// idle. A window that was hidden takes the retained (collapsed) batches when it is shown again.
// Needs the game files (skips without them).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/commands_file.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/picture_check.hpp>
#include <anno/app/play.hpp>
#include <anno/app/play_follow.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/app/window_trace.hpp>
#include <anno/render/frame.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;
namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;

namespace {

fs::path fixed_map() {
    return fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
}

template <class F> bool eventually(F done, int seconds = 60) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (!done()) {
        if (std::chrono::steady_clock::now() > until) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

/// Holds one turn in flight until released (or opened for good by the opener at scope exit).
struct TurnGate {
    std::mutex m;
    std::condition_variable cv;
    std::uint64_t hold = 0;
    bool entered = false, released = false;
    void pass(std::uint64_t turn) {
        if (turn != hold) {
            return;
        }
        std::unique_lock lock(m);
        entered = true;
        cv.notify_all();
        cv.wait(lock, [this] { return released; });
    }
    void release() {
        const std::lock_guard lock(m);
        released = true;
        cv.notify_all();
    }
};
struct TurnGateOpener {
    TurnGate& gate;
    explicit TurnGateOpener(TurnGate& g) : gate(g) {}
    TurnGateOpener(const TurnGateOpener&) = delete;
    TurnGateOpener& operator=(const TurnGateOpener&) = delete;
    ~TurnGateOpener() { gate.release(); }
};

/// The fixed map as --play loads it, the window's inputs taken before the world moves into the
/// runner (as main.cpp does), and commands the sim accepts: a build of the cheapest building type
/// with no goods cost, at tiles of the developer city's island.
struct Play {
    fs::path dir;
    app::PlayLoad load;
    std::vector<app::SceneBuilding> buildings;
    std::vector<bool> classes;
    std::shared_ptr<const render::Scene> scene;
    sim::CityRef city{};
    sim::TileXY origin{};
    std::uint32_t type = 0;

    [[nodiscard]] sim::Command build_at(std::int32_t x, std::int32_t y) const {
        sim::Command cmd;
        cmd.issuer = app::developer_player();
        sim::BuildCmd build;
        build.city = city;
        sim::PlacementWire p;
        p.type = sim::TypeId{type};
        p.x = origin.x + x;
        p.y = origin.y + y;
        p.rotation = 0;
        build.placements.push_back(p);
        cmd.body = std::move(build);
        return cmd;
    }
};

std::optional<Play> load_play() {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return std::nullopt;
    }
    app::FixedPlayOptions fixed;
    fixed.stand_in_placement = true; // island-relative tiles, not chosen for terrain
    auto loaded = app::load_play(*dir, fixed_map(), fixed);
    REQUIRE(loaded.ok());
    Play p{*dir, std::move(loaded).value(), {}, {}, nullptr, {}, {}, 0};
    sim::World& world = p.load.world;
    p.buildings = app::play_buildings(world);
    app::sort_buildings(p.buildings);
    p.classes.assign(0x1000, false);
    for (std::size_t id = 0; id < p.classes.size(); ++id) {
        p.classes[id] = app::anlage_is_building(world, static_cast<std::uint16_t>(id));
    }
    bool found = false;
    for (std::size_t i = 0; i < world.islands().size() && !found; ++i) {
        if (!world.islands()[i]) {
            continue;
        }
        for (const auto& c : world.islands()[i]->cities) {
            if (c && c->owner == app::developer_player() && !found) {
                p.city = sim::CityRef{world.islands()[i]->index, c->number};
                p.origin =
                    sim::TileXY{world.islands()[i]->tiles.x0(), world.islands()[i]->tiles.y0()};
                found = true;
            }
        }
    }
    REQUIRE(found);
    std::int64_t cheapest = 0;
    for (std::uint32_t id = 1; id <= 0xFFFFu; ++id) {
        const sim::BuildingType* t = world.types().find(sim::TypeId{id});
        if (t == nullptr || !t->cost_goods.empty() || id >= p.classes.size() || !p.classes[id]) {
            continue;
        }
        if (p.type == 0 || static_cast<std::int64_t>(t->cost_gold) < cheapest) {
            p.type = id;
            cheapest = static_cast<std::int64_t>(t->cost_gold);
        }
    }
    REQUIRE(p.type != 0);
    app::SceneObjects objects;
    objects.is_building = [&p](std::uint16_t id) { return id < p.classes.size() && p.classes[id]; };
    objects.buildings = p.buildings;
    auto scene = app::load_placed_scene(p.dir, p.load.name, p.load.placed, &objects);
    REQUIRE(scene.ok());
    app::LoadedScene ls = std::move(scene).value();
    ls.scene->river_cfg.enabled = false; // the flow rebuilds chunks for its own reason
    p.scene = std::shared_ptr<const render::Scene>(std::move(ls.scene));
    return p;
}

/// A fresh scene of `buildings` (the oracle).
std::shared_ptr<const render::Scene> fresh_scene(const Play& p,
                                                 const std::vector<app::SceneBuilding>& buildings) {
    app::SceneObjects objects;
    objects.is_building = [&p](std::uint16_t id) { return id < p.classes.size() && p.classes[id]; };
    objects.buildings = buildings;
    auto loaded = app::load_placed_scene(p.dir, p.load.name, p.load.placed, &objects);
    REQUIRE(loaded.ok());
    app::LoadedScene ls = std::move(loaded).value();
    ls.scene->river_cfg.enabled = false;
    return std::shared_ptr<const render::Scene>(std::move(ls.scene));
}

/// A group-2 view of 1024 x 768 around island-local tile (x, y) of island `number`.
render::Rect view_at(const render::Scene& scene, std::uint32_t number, std::int32_t x,
                     std::int32_t y) {
    for (std::size_t i = 0; i < scene.islands.size(); ++i) {
        if (scene.islands[i].number != number) {
            continue;
        }
        for (const render::Placement& pl : scene.layouts[2]) {
            if (pl.index == i) {
                const auto [tw, th] = render::kTile[2];
                const int cx2 = 2 * x + 1, cy2 = 2 * y + 1;
                const int sx = (cy2 - cx2) * tw / 4 + pl.offset.first;
                const int sy = (cx2 + cy2) * th / 4 + pl.offset.second;
                return render::Rect{sx - 512, sy - 384, 1024, 768};
            }
        }
    }
    FAIL("no layout for the island");
    return render::Rect{};
}

/// One iteration of the viewer's loop as viewer.cpp runs it, without the window: the demand from
/// the production predicates, then, if a frame is due, the frame boundary and the frame's chunk
/// update from one owning handle. Returns whether a frame was drawn.
struct Loop {
    app::PlayFollower& follower;
    render::FrameRenderer& frames;
    render::Rect view;
    bool paused = false;
    bool can_draw = true;
    std::size_t frames_drawn = 0;

    [[nodiscard]] app::FrameDemand demand() const {
        app::FrameDemand d;
        d.can_draw = can_draw;
        d.paused = paused;
        d.chunks_busy = frames.busy();
        d.play_work = follower.has_work();
        return d;
    }
    bool step() {
        if (!demand().draw()) {
            return false;
        }
        follower.follow(frames);
        const auto gen = follower.picture().scene();
        frames.update_chunks(gen, view, 2);
        ++frames_drawn;
        return true;
    }
    /// Steps until the demand is idle (bounded), yielding between steps.
    bool until_idle() {
        return eventually([this] {
            step();
            std::this_thread::yield();
            return !demand().draw();
        });
    }
};

} // namespace

TEST_CASE("play follow: a turn in flight when the window pauses publishes after the window went "
          "idle, and the batch is shown while paused, the sim stays paused and the window idles "
          "again (game files)" *
          doctest::test_suite("play_follow")) {
    auto p = load_play();
    if (!p) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const std::vector<app::SceneBuilding> before = p->buildings;
    app::ScenePicture picture(
        p->scene, p->buildings,
        [&p](std::uint16_t id) { return id < p->classes.size() && p->classes[id]; }, 1);
    render::FrameRenderer frames(2);
    frames.set_workers(std::make_unique<render::ChunkWorkers>(2));
    TurnGate gate;
    gate.hold = 2;
    std::atomic<int> wakes{0};
    auto runner = std::make_unique<app::PlayRunner>(std::move(p->load.world));
    runner->set_script({{2, p->build_at(4, 4)}});
    runner->set_turn_gate([&gate](std::uint64_t turn) { gate.pass(turn); });
    runner->set_wake_listener([&wakes] { wakes.fetch_add(1); });
    app::PlayFollower follower(picture, *runner);
    const TurnGateOpener opener(gate);
    runner->start();

    // Turn 2, the build's turn, is in flight. The window pauses (the viewer's on_pause hook) and
    // draws until nothing is left: picture and chunk work idle.
    {
        std::unique_lock lock(gate.m);
        REQUIRE(gate.cv.wait_for(lock, std::chrono::seconds(60), [&] { return gate.entered; }));
    }
    runner->set_paused(true);
    const render::Rect view =
        view_at(*p->scene, static_cast<std::uint32_t>(p->city.island.v), 4, 4);
    Loop loop{follower, frames, view, true, true, 0};
    REQUIRE(loop.until_idle());
    CHECK(!loop.demand().animating());
    CHECK(!follower.has_work());
    CHECK(picture.scene()->generation() == 0);
    const std::size_t idle_frames = loop.frames_drawn;

    // The turn finishes now and publishes the accepted build: no input event follows.
    gate.release();
    REQUIRE(eventually([&] { return wakes.load() >= 1; })); // the window's wake-up
    CHECK(runner->batches_waiting());
    CHECK(loop.demand().draw()); // the idle check sees the batch by itself
    // The window draws until it is idle again: the batch is committed and its chunks are fresh.
    REQUIRE(loop.until_idle());
    CHECK(loop.frames_drawn > idle_frames);
    CHECK(!runner->batches_waiting());
    CHECK(picture.pending() == 0);
    REQUIRE(picture.scene()->generation() == 1);
    REQUIRE(picture.buildings().size() == before.size() + 1);
    CHECK(!loop.demand().animating());
    // Its visible result equals a fresh scene of the new list (all keys of the view).
    const auto fresh = fresh_scene(*p, picture.buildings());
    render::FrameRenderer ref(2);
    for (int round = 0; round < 3; ++round) {
        ref.update_chunks(fresh, view, 2);
    }
    const auto keys = render::chunk_keys_touching(*fresh, view, 2);
    const app::ChunkDiff diff = app::compare_chunks(
        keys, [&frames](const render::ChunkKey& k) { return frames.chunk(k); },
        [&ref](const render::ChunkKey& k) { return ref.chunk(k); });
    CHECK(diff.equal());
    CHECK(diff.missing == 0);
    CHECK(diff.with_objects > 0);
    // Drawing frames did not run the paused sim: the last turn is still the build's.
    const auto turn_after = runner->snapshot()->turn;
    CHECK(turn_after == 2);
    for (int i = 0; i < 20; ++i) {
        loop.step();
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // wall time for turns to run
    }
    CHECK(runner->snapshot()->turn == turn_after);
    CHECK(runner->snapshot()->paused);
    runner->set_wake_listener({});
    runner->stop();
}

TEST_CASE("play follow: a hidden window keeps the mailbox bounded and takes the collapsed batches "
          "when shown again (game files)" *
          doctest::test_suite("play_follow")) {
    auto p = load_play();
    if (!p) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const std::vector<app::SceneBuilding> before = p->buildings;
    app::ScenePicture picture(
        p->scene, p->buildings,
        [&p](std::uint16_t id) { return id < p->classes.size() && p->classes[id]; }, 1);
    render::FrameRenderer frames(2);
    frames.set_workers(std::make_unique<render::ChunkWorkers>(2));
    TurnGate gate;
    gate.hold = 6; // the window is shown again while turn 6 is held: the sim stops there
    auto runner = std::make_unique<app::PlayRunner>(std::move(p->load.world));
    runner->set_batch_bound(2);
    runner->set_script({{1, p->build_at(4, 4)},
                        {2, p->build_at(7, 4)},
                        {3, p->build_at(10, 4)},
                        {4, p->build_at(13, 4)}});
    runner->set_turn_gate([&gate](std::uint64_t turn) { gate.pass(turn); });
    app::PlayFollower follower(picture, *runner);
    const TurnGateOpener opener(gate);
    const render::Rect view =
        view_at(*p->scene, static_cast<std::uint32_t>(p->city.island.v), 8, 4);
    Loop loop{follower, frames, view, false, false, 0}; // hidden: nothing is drawn
    runner->start();
    {
        std::unique_lock lock(gate.m);
        REQUIRE(gate.cv.wait_for(lock, std::chrono::seconds(60), [&] { return gate.entered; }));
    }
    // Four accepted builds were published while hidden; the bound collapsed them into one resync.
    CHECK(!loop.step());
    CHECK(loop.frames_drawn == 0);
    CHECK(loop.demand().play_work);
    const auto waiting = runner->pending_batch_turns();
    CHECK(waiting.size() <= 2);
    REQUIRE(!waiting.empty());
    CHECK(waiting.back() == 4);
    // Shown again (and paused, so that it can go idle: an unpaused window animates): the window
    // takes the retained work and converges to the sim's list.
    loop.can_draw = true;
    loop.paused = true;
    REQUIRE(loop.until_idle());
    CHECK(picture.pending() == 0);
    REQUIRE(picture.buildings().size() == before.size() + 4);
    std::size_t added = 0;
    for (const auto& b : picture.buildings()) {
        added += b.type == p->type ? 1U : 0U;
    }
    std::size_t had = 0;
    for (const auto& b : before) {
        had += b.type == p->type ? 1U : 0U;
    }
    CHECK(added == had + 4);
    const auto fresh = fresh_scene(*p, picture.buildings());
    render::FrameRenderer ref(2);
    for (int round = 0; round < 3; ++round) {
        ref.update_chunks(fresh, view, 2);
    }
    const app::ChunkDiff diff = app::compare_chunks(
        render::chunk_keys_touching(*fresh, view, 2),
        [&frames](const render::ChunkKey& k) { return frames.chunk(k); },
        [&ref](const render::ChunkKey& k) { return ref.chunk(k); });
    CHECK(diff.equal());
    CHECK(diff.missing == 0);
    gate.release();
    runner->stop();
}

TEST_CASE("play follow: the window trace records each turn's outcome from the sim's rejections, a "
          "batch's publication before its take, and the generation after the commit (game files)" *
          doctest::test_suite("play_follow")) {
    // Round ec, B2 and C: the production runner's observer and the follower's trace events.
    auto p = load_play();
    if (!p) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    app::ScenePicture picture(
        p->scene, p->buildings,
        [&p](std::uint16_t id) { return id < p->classes.size() && p->classes[id]; }, 1);
    render::FrameRenderer frames(2);
    frames.set_workers(std::make_unique<render::ChunkWorkers>(2));
    app::WindowTrace trace(10000, 100, 100);
    auto runner = std::make_unique<app::PlayRunner>(std::move(p->load.world));
    // Turn 1 builds; turn 2 builds on the same tile (the sim refuses it); turn 3 has no command.
    runner->set_script({{1, p->build_at(4, 4)}, {2, p->build_at(4, 4)}});
    std::vector<app::TurnObservation> seen;
    runner->set_turn_observer([&](const app::TurnObservation& o) {
        seen.push_back(o);
        const double at = trace.ms_at(o.published);
        if (o.batch) {
            trace.event(at, app::TraceKind::BatchPublished, 0, o.turn);
        }
        trace.turn({o.turn, at, o.applied, o.rejected, o.reasons, o.batch, o.deltas});
    });
    runner->run_n(4);
    REQUIRE(seen.size() == 4);
    CHECK(seen[0].applied == 0);
    CHECK(app::turn_outcome(trace.turns()[0]) == "none");
    CHECK(seen[1].applied == 1);
    CHECK(seen[1].rejected == 0);
    CHECK(seen[1].batch);
    CHECK(seen[1].deltas == 1);
    CHECK(app::turn_outcome(trace.turns()[1]) == "accepted");
    CHECK(seen[2].applied == 1);
    CHECK(seen[2].rejected == 1);
    CHECK(!seen[2].reasons.empty());
    CHECK(!seen[2].batch); // refused: nothing changed, nothing published
    CHECK(app::turn_outcome(trace.turns()[2]) == "rejected");
    CHECK(seen[3].applied == 0);
    // The refused command's own outcome names the same reason.
    const app::OutcomeRead read = runner->outcomes_since(1);
    CHECK(read.lost == 0);
    std::size_t refusals = 0;
    for (const app::CommandOutcome& o : read.outcomes) {
        if (o.state == app::CommandOutcome::State::Refused) {
            ++refusals;
            CHECK(o.turn == seen[2].turn);
            CHECK(seen[2].reasons == sim::reject_name(o.status));
        }
    }
    CHECK(refusals == 1);
    // The window's side: the take, then the commit of turn 1's build.
    app::PlayFollower follower(picture, *runner);
    static_cast<void>(follower.follow(frames, &trace));
    REQUIRE(eventually([&] {
        follower.follow(frames, &trace);
        return picture.pending() == 0;
    }));
    const auto events = trace.events();
    std::size_t published = 0, taken = 0, generations = 0;
    double t_published = -1, t_taken = -1;
    for (const auto& e : events) {
        if (e.kind == app::TraceKind::BatchPublished) {
            ++published;
            t_published = e.t_ms;
            CHECK(e.version == 1);
        } else if (e.kind == app::TraceKind::BatchTaken) {
            ++taken;
            t_taken = e.t_ms;
            CHECK(e.version == 1);
        } else if (e.kind == app::TraceKind::Generation) {
            ++generations;
            CHECK(e.version == picture.scene()->generation());
        }
    }
    CHECK(published == 1);
    CHECK(taken == 1);
    CHECK(generations == 1);
    CHECK(t_published <= t_taken);
}
