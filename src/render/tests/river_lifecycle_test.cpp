// River flow lifecycle (round dv, R1; docs/render/rivers.md 3.1 and docs/app/game-shell.md):
// who owns a queued flow, what a solve holds, and what happens to a queued or running solve when
// the generation that asked for it is retired or destroyed. The interleavings are driven by a
// gate in the test (flow_build_fault, the test hook of Scene), not by timing: a solve reports that
// it has started and waits until the test lets it finish. Needs the game files (skips without).
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

/// A scene and the file buffers its islands point into. The buffers are kept by the test (the
/// generation's own ownership of its backing bytes is a separate contract, round dv R3).
struct Island {
    std::shared_ptr<std::vector<std::uint8_t>> island_buf, anl_buf, felder_buf;
};

/// The river island D_256x256_0045_5B as a new scene (the caller owns it). `buffers` keeps its
/// bytes alive for the test.
render::Scene* new_river_scene(const fs::path& dir, Island& buffers) {
    buffers.island_buf = std::make_shared<std::vector<std::uint8_t>>(
        anno::test::read_file(dir / "Scenes" / "Islands/Dschungel/D_256x256_0045_5B.scp"));
    buffers.anl_buf =
        std::make_shared<std::vector<std::uint8_t>>(anno::test::read_file(dir / "Anlagen.dat"));
    buffers.felder_buf = std::make_shared<std::vector<std::uint8_t>>(
        anno::test::read_file(dir / "Textures" / "Felder.tex"));
    auto anlagen = anno::formats::anlagen::AnlagenFile::parse(*buffers.anl_buf).value();
    const auto felder = anno::formats::tex::TexFile::parse(*buffers.felder_buf).value();
    auto textures = anno::assets::island::GroundTextures::load(felder).value();
    if (const auto c = render::water_colour(anlagen)) {
        textures.set_water(*c);
    }
    return new render::Scene("river lifecycle",
                             anno::formats::insel::islands(*buffers.island_buf).value(),
                             std::move(textures), std::move(anlagen), render::kHeightLift);
}

/// The same scene, shared-owned (a with_islands generation can follow it).
std::shared_ptr<render::Scene> river_scene(const fs::path& dir, Island& buffers) {
    return std::shared_ptr<render::Scene>(new_river_scene(dir, buffers));
}

/// A two-party gate: the solve calls enter(); the test waits in wait_entered() and then calls
/// release(), which lets the solve finish. No timing is involved.
class Gate {
  public:
    void enter() {
        std::unique_lock lock(mutex_);
        entered_ = true;
        cv_.notify_all();
        cv_.wait(lock, [this] { return open_; });
    }
    void wait_entered() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return entered_; });
    }
    void release() {
        {
            const std::lock_guard lock(mutex_);
            open_ = true;
        }
        cv_.notify_all();
    }

  private:
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
    Island buffers;
    auto requester = river_scene(*dir, buffers);
    requester->pause_river_flow_builders(true);
    REQUIRE(requester->river_flow(0, false).pending);
    // A successor shares the island's flow slot (its rivers are the same), then the requester is
    // retired while the request is still queued.
    std::shared_ptr<const render::Scene> sharer = requester->with_islands({});
    requester.reset();
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
    Island buffers;
    // A scene that no shared_ptr owns (the form the tests and tools use).
    std::unique_ptr<render::Scene> scene(new_river_scene(*dir, buffers));
    scene->pause_river_flow_builders(true);
    REQUIRE(scene->river_flow(0, false).pending);
    // Destroyed while its request is queued: the request is dropped, and nothing refers to the
    // scene afterwards.
    scene.reset();
    // The builders are still usable: a fresh scene's flow is solved by them.
    Island again;
    auto fresh = river_scene(*dir, again);
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
    Island buffers;
    std::unique_ptr<render::Scene> scene(new_river_scene(*dir, buffers));
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
    Island buffers;
    // The deleter reports where the scene is destroyed. It owns nothing else: the only references
    // are the test's and the solving builder's.
    std::promise<std::thread::id> destroyed_on;
    auto destroyed = destroyed_on.get_future();
    std::shared_ptr<render::Scene> scene(new_river_scene(*dir, buffers),
                                         [&destroyed_on](render::Scene* p) {
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
    Island buffers;
    auto first = river_scene(*dir, buffers);
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
