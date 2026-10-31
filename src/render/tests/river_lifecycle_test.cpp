// River flow lifecycle (round dv, R1; docs/render/rivers.md 3.1 and docs/app/game-shell.md):
// who owns a queued flow, what a solve holds, and what happens to a queued or running solve when
// the generation that asked for it is retired or destroyed. The interleavings are driven by a
// gate in the test (flow_build_fault, the test hook of Scene), not by timing: a solve reports that
// it has started and waits until the test lets it finish. Needs the game files (skips without).
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/tex.hpp>
#include <anno/render/river.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

namespace render = anno::render;
namespace fs = std::filesystem;

namespace {

/// The river island D_256x256_0045_5B as a new scene (the caller owns it). The scene owns the file
/// bytes its islands and Anlagen view (render::Scene::Backing, round dv R3).
render::Scene* new_river_scene(const fs::path& dir, std::size_t copies = 1) {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    bytes->push_back(
        anno::test::read_file(dir / "Scenes" / "Islands/Dschungel/D_256x256_0045_5B.scp"));
    bytes->push_back(anno::test::read_file(dir / "Anlagen.dat"));
    bytes->push_back(anno::test::read_file(dir / "Textures" / "Felder.tex"));
    auto anlagen = anno::formats::anlagen::AnlagenFile::parse((*bytes)[1]).value();
    const auto felder = anno::formats::tex::TexFile::parse((*bytes)[2]).value();
    auto textures = anno::assets::island::GroundTextures::load(felder).value();
    if (const auto c = render::water_colour(anlagen)) {
        textures.set_water(*c);
    }
    auto islands = anno::formats::insel::islands((*bytes)[0]).value();
    // Copies of the island are islands of their own (their own flow slots, the same river input).
    for (std::size_t c = 1; c < copies; ++c) {
        islands.push_back(islands.front());
        islands.back().number = static_cast<std::uint32_t>(c);
    }
    return new render::Scene("river lifecycle", std::move(islands), std::move(textures),
                             std::move(anlagen), render::kHeightLift, bytes);
}

/// The same scene, shared-owned (a with_islands generation can follow it).
std::shared_ptr<render::Scene> river_scene(const fs::path& dir) {
    return std::shared_ptr<render::Scene>(new_river_scene(dir));
}

/// A two-party gate: the solve calls enter(); the test waits in wait_entered() and then calls
/// release(), which lets the solve finish. No timing is involved.
class Gate {
  public:
    /// Called by the solve: waits until the test releases the gate. Bounded, so a test that failed
    /// before releasing cannot hang the run.
    void enter() {
        std::unique_lock lock(mutex_);
        entered_ = true;
        cv_.notify_all();
        if (!cv_.wait_for(lock, kWait, [this] { return open_; })) {
            throw std::runtime_error("gate: the solve was never released");
        }
    }
    /// True once a solve has entered the gate (bounded).
    bool wait_entered() {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, kWait, [this] { return entered_; });
    }
    void release() {
        {
            const std::lock_guard lock(mutex_);
            open_ = true;
        }
        cv_.notify_all();
    }

  private:
    static constexpr std::chrono::seconds kWait{30};
    std::mutex mutex_;
    std::condition_variable cv_;
    bool entered_ = false, open_ = false;
};

} // namespace

TEST_CASE(
    "river lifecycle: a queued flow of a retired generation is solved by its surviving sharer "
    "(game files)" *
    doctest::test_suite("river_lifecycle")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto requester = river_scene(*dir);
    requester->pause_river_flow_builders(true);
    REQUIRE(requester->river_flow(0, false).pending);
    // A successor shares the island's flow slot (its rivers are the same), then the requester is
    // retired while the request is still queued.
    std::shared_ptr<const render::Scene> sharer = requester->with_islands({});
    requester.reset();
    // The retired generation held the only other reference to the island's bytes (round dv, R3):
    // the survivor reads them itself.
    CHECK_FALSE(sharer->islands.front().head.empty());
    sharer->pause_river_flow_builders(false);
    sharer->wait_river_flow_builders();
    CHECK(sharer->river_flow_done(0));
    CHECK(sharer->river_flow(0, false).flow != nullptr);
}

TEST_CASE(
    "river lifecycle: a non-shared scene destroyed with a flow queued leaves the builders safe "
    "(game files)" *
    doctest::test_suite("river_lifecycle")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // A scene that no shared_ptr owns (the form the tests and tools use).
    std::unique_ptr<render::Scene> scene(new_river_scene(*dir));
    scene->pause_river_flow_builders(true);
    REQUIRE(scene->river_flow(0, false).pending);
    // Destroyed while its request is queued: the request is dropped, and nothing refers to the
    // scene afterwards.
    scene.reset();
    // The builders are still usable: a fresh scene's flow is solved by them.
    auto fresh = river_scene(*dir);
    fresh->pause_river_flow_builders(false);
    CHECK(fresh->river_flow(0, false).pending);
    fresh->wait_river_flow_builders();
    CHECK(fresh->river_flow_done(0));
}

TEST_CASE("river lifecycle: destroying a non-shared scene waits for its running solve "
          "(game files)" *
          doctest::test_suite("river_lifecycle")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::unique_ptr<render::Scene> scene(new_river_scene(*dir));
    Gate gate;
    scene->flow_build_fault = [&gate](std::size_t) { gate.enter(); };
    REQUIRE(scene->river_flow(0, false).pending);
    gate.wait_entered(); // the solve is running on a builder thread, inside the scene
    // The destruction starts while the solve is inside the scene: it must wait for the solve.
    std::thread destroyer([&scene] { scene.reset(); });
    gate.release();
    destroyer.join();
    CHECK(scene == nullptr);
}

TEST_CASE("river lifecycle: the last generation released by the solving builder is destroyed "
          "there, not on the caller's thread (game files)" *
          doctest::test_suite("river_lifecycle")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // The deleter reports where the scene is destroyed. It owns nothing else: the only references
    // are the test's and the solving builder's.
    std::promise<std::thread::id> destroyed_on;
    auto destroyed = destroyed_on.get_future();
    std::shared_ptr<render::Scene> scene(new_river_scene(*dir), [&destroyed_on](render::Scene* p) {
        delete p;
        destroyed_on.set_value(std::this_thread::get_id());
    });
    Gate gate;
    scene->flow_build_fault = [&gate](std::size_t) { gate.enter(); };
    REQUIRE(scene->river_flow(0, false).pending);
    gate.wait_entered();
    // The test drops its reference while the solve holds the scene: the solve keeps it alive.
    scene.reset();
    gate.release();
    // The builder releases the last reference when the solve ends: the scene is destroyed there.
    const std::thread::id caller = std::this_thread::get_id();
    CHECK(destroyed.get() != caller);
}

TEST_CASE("river lifecycle: a second sharer does not reset or requeue a slot another solver owns "
          "(game files)" *
          doctest::test_suite("river_lifecycle")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto first = river_scene(*dir);
    Gate gate;
    first->flow_build_fault = [&gate](std::size_t) { gate.enter(); };
    REQUIRE(first->river_flow(0, false).pending);
    gate.wait_entered();
    // A sharer asks while the solve is running: it is pending and must not queue a second solve.
    std::shared_ptr<const render::Scene> second = first->with_islands({});
    CHECK(second->river_flow(0, false).pending);
    gate.release();
    second->wait_river_flow_builders();
    CHECK(second->river_flow_done(0));
    CHECK(second->river_flow(0, false).flow != nullptr);
}

// Two workers, two solves of one non-shared scene (round dw, DW2a; rivers.md 3.1). Solve 0 is
// held in the gate on one worker while solve 1 completes on the other. Destroying the scene must
// wait for solve 0 (it reads the scene); the test checks that destruction is still blocked before
// it lets the solve go. Pre-fix: the single solving marker is overwritten by solve 1, so the
// destructor returns while solve 0 is inside the scene (heap-use-after-free under ASan).
namespace {

/// Releases a gate when its scope ends, whatever the outcome of the test.
struct ReleaseOnExit {
    Gate& gate;
    ~ReleaseOnExit() { gate.release(); }
};

} // namespace

TEST_CASE("river lifecycle: destruction waits for a gated solve while another worker finishes "
          "(game files, two workers)" *
          doctest::test_suite("river_lifecycle")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::unique_ptr<render::Scene> scene(new_river_scene(*dir, 2));
    scene->river_cfg.flow_threads = 2;
    Gate gate;
    scene->flow_build_fault = [&gate](std::size_t index) {
        if (index == 0) {
            gate.enter();
        }
    };
    std::future<void> destroying;
    ReleaseOnExit release_on_exit{gate};
    REQUIRE(scene->river_flow(0, false).pending);
    REQUIRE(gate.wait_entered()); // solve 0 is inside the scene, on one worker
    REQUIRE(scene->river_flow(1, false).pending);
    // Solve 1 completes on the other worker while solve 0 stays gated.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!scene->river_flow_done(1) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    REQUIRE(scene->river_flow_done(1));
    REQUIRE(!scene->river_flow_done(0));
    destroying = std::async(std::launch::async, [&scene] { scene.reset(); });
    // Destruction must stay blocked while solve 0 is inside the scene. The bound is long: a scene's
    // teardown under ASan takes hundreds of milliseconds, so a short wait would not tell "blocked"
    // from "slow" (the pre-fix trace finished the teardown in about 0.3 s after the early return).
    CHECK(destroying.wait_for(std::chrono::seconds(5)) == std::future_status::timeout);
    gate.release();
    destroying.get();
    CHECK(scene == nullptr);
}

// DW2d: one gated solve and a solve that throws, with 1, 2 and 4 workers. With one worker the other
// requests stay queued behind the gated solve; destruction then removes them and waits only for
// the solve in progress. With more workers the other solves complete (one of them throws, inside
// build_flow) while the gated one is still inside the scene.
namespace {

void destruction_waits_for_active_solve(const fs::path& dir, unsigned workers) {
    constexpr std::size_t kIslands = 4;
    std::unique_ptr<render::Scene> scene(new_river_scene(dir, kIslands));
    scene->river_cfg.flow_threads = workers;
    Gate gate;
    scene->flow_build_fault = [&gate](std::size_t index) {
        if (index == 0) {
            gate.enter();
        }
        if (index == 2) {
            throw std::runtime_error("injected solve failure");
        }
    };
    std::future<void> destroying;
    ReleaseOnExit release_on_exit{gate};
    REQUIRE(scene->river_flow(0, false).pending);
    REQUIRE(gate.wait_entered());
    for (std::size_t i = 1; i < kIslands; ++i) {
        static_cast<void>(scene->river_flow(i, false));
    }
    if (workers >= 2) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        for (std::size_t i = 1; i < kIslands; ++i) {
            while (!scene->river_flow_done(i) && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }
            CHECK(scene->river_flow_done(i));
        }
    }
    REQUIRE(!scene->river_flow_done(0));
    destroying = std::async(std::launch::async, [&scene] { scene.reset(); });
    CHECK(destroying.wait_for(std::chrono::seconds(5)) == std::future_status::timeout);
    gate.release();
    destroying.get();
    CHECK(scene == nullptr);
}

} // namespace

TEST_CASE(
    "river lifecycle: 1 worker, queued requests and a gated solve: destruction removes the queue "
    "and waits (game files)" *
    doctest::test_suite("river_lifecycle")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    destruction_waits_for_active_solve(*dir, 1);
}

TEST_CASE("river lifecycle: 2 workers, one solve throws and one is gated: destruction waits "
          "(game files)" *
          doctest::test_suite("river_lifecycle")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    destruction_waits_for_active_solve(*dir, 2);
}

TEST_CASE("river lifecycle: 4 workers, one solve throws and one is gated: destruction waits "
          "(game files)" *
          doctest::test_suite("river_lifecycle")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    destruction_waits_for_active_solve(*dir, 4);
}
