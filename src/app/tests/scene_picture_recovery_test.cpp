// ScenePicture recovery and convergence (round dw, DW3c/d and DW4c/f; docs/app/game-shell.md,
// "Retained work and reclamation"). Synthetic: islands of 8x8 with no Anlagen, so no game files are
// needed. The buildings are added to the islands' records, so a committed generation shows its
// changes as record counts: an island's record count must equal the number of buildings the desired
// state has on it.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/building_delta.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>

namespace app = anno::app;
namespace render = anno::render;

namespace {

/// `count` synthetic islands numbered 0..count-1, each with an empty record list.
std::shared_ptr<render::Scene> islands_scene(std::size_t count) {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    std::vector<anno::formats::insel::Island> islands;
    for (std::size_t i = 0; i < count; ++i) {
        bytes->push_back(std::vector<std::uint8_t>(64, 0));
    }
    for (std::size_t i = 0; i < count; ++i) {
        anno::formats::insel::Island island;
        island.head = (*bytes)[i];
        island.number = static_cast<std::uint32_t>(i);
        island.width = 8;
        island.height = 8;
        islands.push_back(island);
    }
    return render::Scene::create("recovery", std::move(islands),
                                 anno::assets::island::GroundTextures{},
                                 anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
}

/// The desired building list: `per_island[i]` buildings on island i, in batch order.
std::shared_ptr<const app::BuildingBatch> added(std::uint64_t turn, std::uint32_t island,
                                                std::uint16_t x) {
    auto b = std::make_shared<app::BuildingBatch>();
    b->turn = turn;
    b->deltas.push_back(
        {app::BuildingDelta::Kind::Added, app::SceneBuilding{island, 1, x, 1, 0, 0}});
    return b;
}

/// The record payloads of island `index` in the committed generation.
std::vector<std::vector<std::uint8_t>> committed_payloads(const render::Scene& scene,
                                                          std::size_t index) {
    std::vector<std::vector<std::uint8_t>> out;
    for (const anno::formats::chunk::Chunk& rec : scene.islands[index].records) {
        out.emplace_back(rec.payload.begin(), rec.payload.end());
    }
    return out;
}

/// The record payloads of island `index` as a fresh build from the desired state makes them: the
/// loader's per-island function on a copy of the base island (the same steps the picture's worker
/// takes).
std::vector<std::vector<std::uint8_t>> fresh_payloads(const render::Scene& base, std::size_t index,
                                                      const app::ScenePicture& picture) {
    const std::uint32_t number = base.islands[index].number;
    std::vector<app::SceneBuilding> mine;
    for (const app::SceneBuilding& b : picture.buildings()) {
        if (b.island == number) {
            mine.push_back(b);
        }
    }
    anno::formats::insel::Island island = base.islands[index];
    std::vector<std::vector<std::uint8_t>> storage;
    storage.push_back(std::vector<std::uint8_t>(island.head.begin(), island.head.end()));
    island.head = anno::formats::Bytes(storage.back().data(), storage.back().size());
    for (anno::formats::chunk::Chunk& rec : island.records) {
        storage.push_back(std::vector<std::uint8_t>(rec.payload.begin(), rec.payload.end()));
        rec.payload = anno::formats::Bytes(storage.back().data(), storage.back().size());
    }
    app::SceneObjects objects;
    objects.is_building = [](std::uint16_t id) { return id < 0x1000; };
    std::vector<std::string> notes;
    std::vector<app::OmittedObject> omitted;
    app::apply_island_buildings(island, mine, objects, storage, notes, omitted);
    std::vector<std::vector<std::uint8_t>> out;
    for (const anno::formats::chunk::Chunk& rec : island.records) {
        out.emplace_back(rec.payload.begin(), rec.payload.end());
    }
    return out;
}

/// The buildings of the desired state on island `number`.
std::size_t desired_on(const app::ScenePicture& picture, std::uint32_t number) {
    std::size_t n = 0;
    for (const app::SceneBuilding& b : picture.buildings()) {
        n += b.island == number ? 1U : 0U;
    }
    return n;
}

app::ScenePicture::Limits fault_once_limits(std::atomic<int>& failures, std::uint32_t island) {
    app::ScenePicture::Limits limits;
    limits.build_fault = [&failures, island](std::size_t index) {
        if (static_cast<std::uint32_t>(index) == island && failures.fetch_sub(1) > 0) {
            throw std::runtime_error("injected build failure");
        }
    };
    return limits;
}

/// Commits until nothing is pending, bounded (a hot loop or a stuck island fails the case).
bool drain(app::ScenePicture& picture, render::FrameRenderer& frames, int rounds) {
    for (int round = 0; round < rounds && picture.pending() > 0; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    return picture.pending() == 0;
}

} // namespace

TEST_CASE("picture recovery: a one-shot build failure is retried from the desired state with no "
          "new batch" *
          doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(2);
    std::atomic<int> failures{1};
    std::vector<app::SceneBuilding> none;
    app::ScenePicture picture(
        scene, none, [](std::uint16_t) { return true; }, 1, fault_once_limits(failures, 0));
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 0); // the failed build commits nothing
    CHECK(picture.pending() == 1);      // the change is still owed: no false idle
    // No new batch: the window's commit calls alone recover the island.
    CHECK(drain(picture, frames, 20));
    REQUIRE(!fresh_payloads(*scene, 0, picture).empty()); // the oracle is not two empty lists
    CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
    CHECK(picture.stats().build_failures == 1);
    CHECK(picture.stats().build_retries >= 1);
    CHECK(picture.stats().failed_islands == 0);
    CHECK(picture.error().empty());
}

TEST_CASE("picture recovery: a persistent failure stops after the attempts and is reported" *
          doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    app::ScenePicture::Limits limits;
    limits.max_attempts = 3;
    std::atomic<int> attempts{0};
    limits.build_fault = [&attempts](std::size_t) {
        attempts.fetch_add(1);
        throw std::runtime_error("injected persistent failure");
    };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    // Many commit calls: the builds stop at the attempt limit, the island stays owed and reported.
    for (int round = 0; round < 40; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    CHECK(attempts.load() == 3);
    CHECK(picture.stats().build_failures == 3);
    CHECK(picture.stats().failed_islands == 1);
    CHECK(picture.pending() == 1); // not idle: the change is owed
    CHECK(picture.error().find("injected persistent failure") != std::string::npos);
}

TEST_CASE("picture recovery: a newer change during a retry is built from the newest desired state" *
          doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(2);
    std::atomic<int> failures{1};
    std::vector<app::SceneBuilding> none;
    app::ScenePicture picture(
        scene, none, [](std::uint16_t) { return true; }, 1, fault_once_limits(failures, 0));
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    picture.wait_idle();
    picture.commit(frames); // the failure; the retry is waiting
    picture.apply({added(2, 0, 2)});
    CHECK(drain(picture, frames, 20));
    CHECK(committed_payloads(*picture.scene(), 0) ==
          fresh_payloads(*scene, 0, picture)); // both changes, from the newest desired state
    CHECK(desired_on(picture, 0) == 2);
}

TEST_CASE("picture recovery: shutdown with a retry waiting returns and releases the state" *
          doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    app::ScenePicture::Limits limits;
    limits.build_fault = [](std::size_t) { throw std::runtime_error("injected"); };
    {
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2, limits);
        render::FrameRenderer frames(1);
        picture.apply({added(1, 0, 1)});
        picture.wait_idle();
        picture.commit(frames); // a retry is now waiting
    }
    CHECK(true); // reaching this line: the destructor joined the workers and the retry did not run
}

TEST_CASE("picture recovery: a result of an older version is never published after a newer change" *
          doctest::test_suite("scene_picture_recovery")) {
    // The reaper is stalled, so commits are skipped while it is behind. A build finishes, a newer
    // change arrives while the reaper is still behind, and the reaper is released. The old result
    // must not be published: the commit drops it and the newer change is built.
    auto scene = islands_scene(2);
    std::atomic<bool> open{false};
    std::vector<app::SceneBuilding> none;
    app::ScenePicture::Limits limits;
    limits.max_reclaim_backlog = 0;
    limits.reclaim_gate = [&open] {
        while (!open.load()) {
            std::this_thread::yield();
        }
    };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
    render::FrameRenderer frames(1);
    // Generation 1 (island 0: one building); its replaced generation keeps the reaper busy.
    picture.apply({added(1, 0, 1)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 1);
    // Island 0 is built again (version 2, one building more) and the build finishes; the commit is
    // skipped while the reaper is behind, so the finished result waits.
    picture.apply({added(3, 0, 2)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 0);
    CHECK(picture.stats().commit_skipped >= 1);
    // A newer change for island 0 arrives while its old result is waiting and the reaper is behind.
    picture.apply({added(4, 0, 3)});
    open.store(true); // the reaper catches up
    // Wait until the reaper has released everything it was handed (its own counters, bounded). A
    // commit is skipped while the reaper is behind, so the drain below must start after the
    // catch-up, not race it.
    const auto caught_up = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (picture.stats().reclaim_completed < picture.stats().reclaim_queued &&
           std::chrono::steady_clock::now() < caught_up) {
        std::this_thread::yield();
    }
    REQUIRE(picture.stats().reclaim_completed == picture.stats().reclaim_queued);
    CHECK(drain(picture, frames, 20));
    // The old result (two buildings) was never published as the newest state: island 0 has three.
    CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
    CHECK(desired_on(picture, 0) == 3);
    CHECK(picture.stats().discarded >= 1);
}

TEST_CASE(
    "picture recovery: sustained changes over many islands converge and stay within the limit" *
    doctest::test_suite("scene_picture_recovery")) {
    constexpr std::size_t kIslands = 12;
    auto scene = islands_scene(kIslands);
    std::vector<app::SceneBuilding> none;
    app::ScenePicture::Limits limits;
    limits.max_outstanding = 3;
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2, limits);
    render::FrameRenderer frames(1);
    std::map<std::uint32_t, std::size_t> counts;
    for (std::uint64_t turn = 1; turn <= 240; ++turn) {
        // Changes arrive faster than the commits: three islands per turn, rotating.
        std::vector<std::shared_ptr<const app::BuildingBatch>> batches;
        for (std::uint32_t k = 0; k < 3; ++k) {
            const auto island = static_cast<std::uint32_t>((turn * 3 + k) % kIslands);
            batches.push_back(added(turn, island, static_cast<std::uint16_t>(turn)));
            ++counts[island];
        }
        picture.apply(batches);
        if (turn % 4 == 0) {
            picture.wait_idle();
            picture.commit(frames);
        }
    }
    CHECK(drain(picture, frames, 400));
    const auto st = picture.stats();
    CHECK(st.outstanding_peak <= limits.max_outstanding);
    CHECK(st.committed >= kIslands);
    for (std::uint32_t island = 0; island < kIslands; ++island) {
        CHECK(committed_payloads(*picture.scene(), island) ==
              fresh_payloads(*scene, island, picture));
    }
    CHECK(st.dirty_islands == 0);
}

TEST_CASE(
    "picture recovery: one over-budget island still makes progress; the budget holds otherwise" *
    doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(4);
    std::vector<app::SceneBuilding> none;
    app::ScenePicture::Limits limits;
    limits.max_outstanding_bytes = 1; // every island is over the budget on its own
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2, limits);
    render::FrameRenderer frames(1);
    for (std::uint32_t island = 0; island < 4; ++island) {
        picture.apply({added(island + 1, island, 1)});
    }
    CHECK(drain(picture, frames, 40));
    for (std::uint32_t island = 0; island < 4; ++island) {
        CHECK(committed_payloads(*picture.scene(), island) ==
              fresh_payloads(*scene, island, picture));
    }
    // One build at a time under the budget: the peak never has two builds in flight.
    CHECK(picture.stats().outstanding_peak == 1);
    CHECK(picture.stats().budget_deferrals >= 1);
}

TEST_CASE("picture recovery: the same convergence with one, two and four workers" *
          doctest::test_suite("scene_picture_recovery")) {
    for (std::size_t workers : {std::size_t{1}, std::size_t{2}, std::size_t{4}}) {
        auto scene = islands_scene(6);
        std::vector<app::SceneBuilding> none;
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, workers);
        render::FrameRenderer frames(1);
        for (std::uint64_t turn = 1; turn <= 30; ++turn) {
            picture.apply({added(turn, static_cast<std::uint32_t>(turn % 6),
                                 static_cast<std::uint16_t>(turn))});
        }
        CHECK(drain(picture, frames, 100));
        std::size_t total = 0;
        for (std::uint32_t island = 0; island < 6; ++island) {
            total += desired_on(picture, island);
            CHECK(committed_payloads(*picture.scene(), island) ==
                  fresh_payloads(*scene, island, picture));
        }
        CHECK(total == 30);
    }
}

TEST_CASE("picture recovery: a generation that outlives the bridge is destroyed on the reclaimer" *
          doctest::test_suite("scene_picture_recovery")) {
    // The bridge is destroyed while a commit's generation is still held by the test (a consumer
    // that outlived the picture). The final release is the test's, on this thread; the destruction
    // must run on the reclaimer, and the bridge's own references must be gone with the bridge.
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    std::shared_ptr<const render::Scene> kept;
    const std::uint64_t before = render::SceneReclaimer::shared().stats().completed;
    {
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2);
        render::FrameRenderer frames(1);
        picture.apply({added(1, 0, 1)});
        picture.wait_idle();
        REQUIRE(picture.commit(frames) == 1);
        kept = picture.scene();
    }
    scene.reset(); // the base generation and the bridge's references are gone; `kept` is the last
                   // owner
    CHECK(render::SceneReclaimer::shared().stats().completed == before);
    kept.reset();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (render::SceneReclaimer::shared().stats().completed < before + 2 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    // Both generations (the base and the committed one) were destroyed by the reclaimer.
    REQUIRE(render::SceneReclaimer::shared().stats().completed >= before + 2);
    CHECK(render::SceneReclaimer::shared().last_thread() != std::this_thread::get_id());
}

// DX3: the failures stay visible per island, and a retry restarts the failed islands from the
// desired state. The fault is a bitmask of the islands that fail on every build (a test seam); the
// messages name the island.
namespace {

app::ScenePicture::Limits failing_islands(std::atomic<unsigned>& mask, std::uint32_t max_attempts) {
    app::ScenePicture::Limits limits;
    limits.max_attempts = max_attempts;
    limits.build_fault = [&mask](std::size_t index) {
        if ((mask.load() & (1U << index)) != 0) {
            throw std::runtime_error("boom " + std::to_string(index));
        }
    };
    return limits;
}

} // namespace

TEST_CASE("picture failures: two failed islands stay reported through an unrelated healthy edit" *
          doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(3);
    std::vector<app::SceneBuilding> none;
    std::atomic<unsigned> mask{0b011};
    app::ScenePicture picture(
        scene, none, [](std::uint16_t) { return true; }, 2, failing_islands(mask, 1));
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1), added(1, 1, 1)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 0);
    CHECK(picture.stats().failed_islands == 2);
    const std::string both = picture.error();
    CHECK(both.find("island 0: boom 0") != std::string::npos);
    CHECK(both.find("island 1: boom 1") != std::string::npos);
    // A healthy edit on island 2 commits; the other two failures stay reported (no global clear).
    picture.apply({added(2, 2, 1)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 1);
    CHECK(picture.stats().failed_islands == 2);
    CHECK(picture.error() == both);
}

TEST_CASE(
    "picture failures: one island recovers by an explicit retry while the other stays failed" *
    doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    std::atomic<unsigned> mask{0b11};
    app::ScenePicture picture(
        scene, none, [](std::uint16_t) { return true; }, 2, failing_islands(mask, 1));
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1), added(1, 1, 1)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 0);
    CHECK(picture.stats().failed_islands == 2);
    // The fault on island 0 goes away; island 1 still fails. Nothing is replayed: the retry reads
    // the desired state.
    mask.store(0b10);
    CHECK(picture.retry_failed() == 2);
    picture.wait_idle();
    CHECK(picture.commit(frames) == 1); // island 0 recovers and is committed
    CHECK(picture.stats().failed_islands == 1);
    CHECK(picture.error().find("island 1: boom 1") != std::string::npos);
    CHECK(picture.error().find("island 0") == std::string::npos);
    CHECK(picture.pending() == 1); // island 1 is still owed
    // The last good picture of island 1 is still the base's (nothing was committed for it).
    CHECK(committed_payloads(*picture.scene(), 1).empty());
}

TEST_CASE("picture failures: an obsolete failure is not evidence about a newer change" *
          doctest::test_suite("scene_picture_recovery")) {
    // Reaper overload defers the newer change for island 0 while its older build is still running.
    // The older build then fails. Its failure belongs to a version that is no longer desired: it is
    // not counted, and the newer change is built.
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    std::atomic<int> island0_calls{0};
    std::atomic<bool> entered{false}, release_build{false}, release_reaper{false};
    app::ScenePicture::Limits limits;
    limits.max_attempts = 1;
    limits.max_reclaim_backlog = 0;
    limits.build_fault = [&](std::size_t index) {
        if (index != 0) {
            return;
        }
        // The first build of island 0 (the older version) fails once it is released; later builds
        // succeed.
        if (island0_calls.fetch_add(1) == 0) {
            entered.store(true);
            while (!release_build.load()) {
                std::this_thread::yield();
            }
            throw std::runtime_error("boom for the older version");
        }
    };
    limits.reclaim_gate = [&] {
        while (!release_reaper.load()) {
            std::this_thread::yield();
        }
    };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2, limits);
    render::FrameRenderer frames(1);
    // Island 1 commits first: its replaced generation keeps the reaper behind.
    picture.apply({added(1, 1, 1)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 1);
    // Island 0's older build starts and blocks; the newer change then arrives while the reaper is
    // behind.
    picture.apply({added(2, 0, 1)});
    while (!entered.load()) {
        std::this_thread::yield();
    }
    picture.apply({added(3, 0, 2)});
    REQUIRE(picture.stats().deferred >=
            1); // the newer change waits: it does not replace the running build
    release_build.store(true);
    picture.wait_idle();
    CHECK(picture.stats().failed_islands ==
          0); // the older failure is not counted against the newer change
    CHECK(picture.error().empty());
    release_reaper.store(true);
    // The commits wait for the reaper: drain after its catch-up, counted by its own counters
    // (bounded).
    const auto caught_up = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (picture.stats().reclaim_completed < picture.stats().reclaim_queued &&
           std::chrono::steady_clock::now() < caught_up) {
        std::this_thread::yield();
    }
    REQUIRE(picture.stats().reclaim_completed == picture.stats().reclaim_queued);
    CHECK(drain(picture, frames, 40));
    CHECK(picture.stats().failed_islands == 0);
    CHECK(picture.error().empty());
    CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
}

TEST_CASE("picture failures: a persistent failure is bounded per retry and stays reported" *
          doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(1);
    std::vector<app::SceneBuilding> none;
    std::atomic<unsigned> mask{0b1};
    app::ScenePicture picture(
        scene, none, [](std::uint16_t) { return true; }, 1, failing_islands(mask, 2));
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    for (int round = 0; round < 20; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    CHECK(picture.stats().build_failures == 2); // max_attempts: the builds stop at the limit
    CHECK(picture.stats().failed_islands == 1);
    CHECK(picture.pending() == 1);
    // An explicit retry restarts the island: two more builds, then the same terminal state.
    CHECK(picture.retry_failed() == 1);
    picture.wait_idle();
    for (int round = 0; round < 20; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    CHECK(picture.stats().build_failures == 4);
    CHECK(picture.stats().failed_islands == 1);
    CHECK(picture.error().find("boom 0") != std::string::npos);
}

TEST_CASE("picture credits: a superseded running build keeps its credit until its worker finishes" *
          doctest::test_suite("scene_picture_recovery")) {
    // Island 0 (64 bytes) is blocked in its first build, and a newer change arrives while it runs.
    // The running build keeps its credit, so the replacement does not fit the byte budget beside it
    // and is deferred. When the build finishes, the credit goes with it.
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    std::atomic<bool> open{false};
    std::atomic<bool> entered{false};
    app::ScenePicture::Limits limits;
    limits.max_outstanding_bytes = 96; // one 64-byte build fits, two do not
    limits.build_fault = [&open, &entered](std::size_t) {
        entered.store(true);
        while (!open.load()) {
            std::this_thread::yield();
        }
    };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    while (!entered.load()) {
        std::this_thread::yield();
    }
    CHECK(picture.stats().outstanding_bytes == 64);
    picture.apply({added(2, 0, 2)}); // the running build is superseded
    CHECK(picture.stats().outstanding_bytes == 64);
    CHECK(picture.stats().budget_deferrals >= 1);
    open.store(true);
    picture.wait_idle(); // the superseded build has finished and released its credit
    CHECK(picture.stats().outstanding_bytes == 0);
    CHECK(drain(picture, frames, 20));
    CHECK(picture.error().empty());
    CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
    CHECK(desired_on(picture, 0) == 2);
    CHECK(picture.stats().discarded >= 1);
    CHECK(picture.stats().outstanding_bytes == 0);
}

TEST_CASE("picture credits: a dropped job keeps its credit until the reaper has destroyed it" *
          doctest::test_suite("scene_picture_recovery")) {
    // One worker, blocked in island 0's build, and island 1's build queued behind it. A newer
    // change to island 1 drops the queued job to the reaper, which is stalled: the dropped job's
    // credit is still held, with island 0's running build and the new job of island 1. When the
    // reaper and the worker are released, the credits of the dropped job and of the finished builds
    // are released.
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    std::atomic<bool> build_open{false};
    std::atomic<bool> reaper_open{false};
    std::atomic<bool> entered{false};
    app::ScenePicture::Limits limits;
    limits.build_fault = [&build_open, &entered](std::size_t index) {
        if (index == 0) {
            entered.store(true);
            while (!build_open.load()) {
                std::this_thread::yield();
            }
        }
    };
    limits.reclaim_gate = [&reaper_open] {
        while (!reaper_open.load()) {
            std::this_thread::yield();
        }
    };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    while (!entered.load()) {
        std::this_thread::yield();
    }
    picture.apply({added(2, 1, 1)}); // queued behind island 0's build
    picture.apply({added(3, 1, 2)}); // drops island 1's queued job to the reaper, starts a new one
    CHECK(picture.stats().outstanding_bytes == 192);
    reaper_open.store(true);
    build_open.store(true);
    picture.wait_idle();
    // The reaper has destroyed the dropped job (its own counters, bounded); the two builds are held
    // as results.
    const auto caught_up = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (picture.stats().reclaim_completed < picture.stats().reclaim_queued &&
           std::chrono::steady_clock::now() < caught_up) {
        std::this_thread::yield();
    }
    REQUIRE(picture.stats().reclaim_completed == picture.stats().reclaim_queued);
    CHECK(picture.stats().outstanding_bytes == 128);
    CHECK(drain(picture, frames, 20));
    CHECK(picture.error().empty());
    CHECK(committed_payloads(*picture.scene(), 1) == fresh_payloads(*scene, 1, picture));
    CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
    CHECK(picture.stats().outstanding_bytes == 0);
}

TEST_CASE("picture reclaim: a stalled process reclaimer holds the commits back, and the picture "
          "converges after the release" *
          doctest::test_suite("scene_picture_recovery")) {
    // The process reclaimer is stalled: every replaced generation waits for it. The commits stop
    // while it is behind, so the waiting generations stay bounded, the picture keeps its last valid
    // state, and after the release it converges with no bytes left waiting.
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    auto open = std::make_shared<std::atomic<bool>>(false);
    auto& reclaimer = render::SceneReclaimer::shared();
    reclaimer.set_test_gate([open] {
        while (!open->load()) {
            std::this_thread::yield();
        }
    });
    const auto before = reclaimer.stats();
    {
        app::ScenePicture::Limits limits;
        limits.max_outstanding = 1;
        limits.max_reclaim_backlog = 1;
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
        render::FrameRenderer frames(1);
        std::size_t committed = 0;
        for (std::uint64_t turn = 1; turn <= 40; ++turn) {
            picture.apply({added(turn, 0, static_cast<std::uint16_t>(turn))});
            picture.wait_idle();
            committed += picture.commit(frames);
            // The reaper releases the replaced generation to the reclaimer (its own counters).
            const auto caught_up = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (picture.stats().reclaim_completed < picture.stats().reclaim_queued &&
                   std::chrono::steady_clock::now() < caught_up) {
                std::this_thread::yield();
            }
            // A commit waits while the reclaimer holds more than the backlog limit: at most the
            // limit plus the generation of the commit before it.
            CHECK(reclaimer.stats().waiting - before.waiting <= limits.max_reclaim_backlog + 1);
        }
        CHECK(picture.stats().commit_skipped >= 1);
        CHECK(committed < 40);
        CHECK(picture.pending() >= 1);
        CHECK(picture.error().empty());
        open->store(true);
        const auto released = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (reclaimer.stats().waiting > before.waiting &&
               std::chrono::steady_clock::now() < released) {
            std::this_thread::yield();
        }
        CHECK(drain(picture, frames, 80));
        CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
        CHECK(desired_on(picture, 0) == 40);
        CHECK(picture.stats().outstanding_bytes == 0);
    }
    // The destruction of the picture releases what it still held; the reclaimer returns to its
    // baseline once it has destroyed them.
    const auto settled = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (reclaimer.stats().waiting > before.waiting &&
           std::chrono::steady_clock::now() < settled) {
        std::this_thread::yield();
    }
    CHECK(reclaimer.stats().waiting == before.waiting);
    CHECK(reclaimer.stats().bytes_waiting == before.bytes_waiting);
    reclaimer.set_test_gate({});
}
