// ScenePicture publication under the Monotonic policy, the default since round dz
// (docs/app/game-shell.md, "Publication policy"). Synthetic islands with no Anlagen, so no game
// files are needed. The invariants every case checks through the trace seam: per island, the
// committed versions only increase, and each is no newer than the island's desired version when it
// was committed. Gates are opened by RAII openers declared after the picture, so a failed assertion
// releases them before the picture's destructor joins its threads.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/building_delta.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/picture_check.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>

namespace app = anno::app;
namespace render = anno::render;

namespace {

/// `count` synthetic islands numbered 0..count-1 of 8 x 8 tiles, each with a 64-byte head and no
/// records.
std::shared_ptr<render::Scene> islands_scene(std::size_t count) {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    for (std::size_t i = 0; i < count; ++i) {
        bytes->push_back(std::vector<std::uint8_t>(64, 0));
    }
    std::vector<anno::formats::insel::Island> islands;
    for (std::size_t i = 0; i < count; ++i) {
        anno::formats::insel::Island island;
        island.head = (*bytes)[i];
        island.number = static_cast<std::uint32_t>(i);
        island.width = 8;
        island.height = 8;
        islands.push_back(island);
    }
    return render::Scene::create("publish", std::move(islands),
                                 anno::assets::island::GroundTextures{},
                                 anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
}

/// One building added on island `island` at (x, y).
std::shared_ptr<const app::BuildingBatch> added(std::uint64_t turn, std::uint32_t island,
                                                std::uint16_t x, std::uint16_t y = 1) {
    auto b = std::make_shared<app::BuildingBatch>();
    b->turn = turn;
    b->deltas.push_back(
        {app::BuildingDelta::Kind::Added, app::SceneBuilding{island, 1, x, y, 0, 0}});
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

/// The record payloads a fresh build of island `index` from the desired state makes.
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

/// The buildings island `index` shows in the committed generation.
std::size_t shown(const app::ScenePicture& picture, std::size_t index) {
    return app::building_items(picture.scene()->islands[index]).size();
}

bool drain(app::ScenePicture& picture, render::FrameRenderer& frames, int rounds) {
    for (int round = 0; round < rounds && picture.pending() > 0; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    return picture.pending() == 0;
}

/// Waits (bounded) until `done` holds; returns whether it did.
template <class F> bool eventually(F done, int seconds = 10) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (!done()) {
        if (std::chrono::steady_clock::now() > until) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

/// Waits (bounded) until the picture's reaper and the process reclaimer have released everything
/// they were handed, so a commit is not skipped for reclamation the case does not test.
bool settled(const app::ScenePicture& picture) {
    return eventually([&picture] {
        const auto s = picture.stats();
        const auto r = render::SceneReclaimer::shared().stats();
        return s.reclaim_completed == s.reclaim_queued && r.waiting == 0 && r.in_flight == 0;
    });
}

/// A gate a seam blocks in until it is opened.
struct Gate {
    std::atomic<bool> open{false};
    std::atomic<int> entered{0};
    void pass() {
        entered.fetch_add(1);
        while (!open.load()) {
            std::this_thread::yield();
        }
    }
};
/// A gate that lets builds through one at a time: build n (from 0) waits until `released` > n.
struct Turnstile {
    std::atomic<bool> open{false};
    std::atomic<int> entered{0}, released{0};
    void pass() {
        const int me = entered.fetch_add(1);
        while (!open.load() && released.load() <= me) {
            std::this_thread::yield();
        }
    }
};
/// Opens a gate when it goes out of scope (declared after the picture: it goes first).
template <class G> struct Opener {
    G& gate;
    Opener(const Opener&) = delete;
    Opener& operator=(const Opener&) = delete;
    explicit Opener(G& g) : gate(g) {}
    ~Opener() { gate.open.store(true); }
};

/// Clears the reclaimer's test observer when it goes out of scope.
struct ObserverReset {
    ObserverReset() = default;
    ObserverReset(const ObserverReset&) = delete;
    ObserverReset& operator=(const ObserverReset&) = delete;
    ~ObserverReset() { render::SceneReclaimer::shared().set_test_observer({}); }
};

/// The trace seam's events, and the publication invariants over them.
struct Recorder {
    struct Event {
        app::PictureEvent kind;
        std::size_t index;
        std::uint64_t version;
    };
    std::mutex mutex;
    std::vector<Event> events;

    [[nodiscard]] std::function<void(app::PictureEvent, std::size_t, std::uint64_t)> seam() {
        return [this](app::PictureEvent e, std::size_t index, std::uint64_t version) {
            const std::lock_guard lock(mutex);
            events.push_back({e, index, version});
        };
    }
    /// The versions of island `index` in commit order.
    std::vector<std::uint64_t> committed(std::size_t index) {
        const std::lock_guard lock(mutex);
        std::vector<std::uint64_t> out;
        for (const Event& e : events) {
            if (e.kind == app::PictureEvent::Committed && e.index == index) {
                out.push_back(e.version);
            }
        }
        return out;
    }
    std::size_t count(app::PictureEvent kind) {
        const std::lock_guard lock(mutex);
        return static_cast<std::size_t>(std::count_if(
            events.begin(), events.end(), [kind](const Event& e) { return e.kind == kind; }));
    }
    /// Empty when every island's commits only advance and none is newer than the island's desired
    /// version at that moment (Desired and Committed are both window-thread events, so their order
    /// in the trace is the order they happened in); else the first violation.
    std::string violation() {
        const std::lock_guard lock(mutex);
        std::map<std::size_t, std::uint64_t> desired, committed;
        for (const Event& e : events) {
            if (e.kind == app::PictureEvent::Desired) {
                desired[e.index] = e.version;
            } else if (e.kind == app::PictureEvent::Committed) {
                if (e.version <= committed[e.index]) {
                    return "island " + std::to_string(e.index) + " committed " +
                           std::to_string(e.version) + " after " +
                           std::to_string(committed[e.index]);
                }
                if (e.version > desired[e.index]) {
                    return "island " + std::to_string(e.index) + " committed " +
                           std::to_string(e.version) + " beyond desired " +
                           std::to_string(desired[e.index]);
                }
                committed[e.index] = e.version;
            }
        }
        return {};
    }
};

} // namespace

TEST_CASE("publish: a stream of edits moves the picture forward while it lasts; LatestOnly does "
          "not" *
          doctest::test_suite("scene_picture_publish")) {
    // One worker; builds pass a turnstile one at a time. Each round: a new edit arrives while the
    // previous version's build is held, then that build is released and the next one is held. The
    // desired version is always ahead of every finished build, as under a sustained stream.
    // Monotonic publishes the finished build when it is still valid for the committed lists: a
    // build that ran while the island's previous version was committed is requeued (its change
    // set is against lists the generation no longer has), so with one worker every second
    // intermediate build commits (versions 1, 3, 5, 7) and the others are requeued. LatestOnly
    // publishes nothing until the stream stops.
    constexpr std::uint64_t kLast = 8;
    for (const auto policy : {app::PublishPolicy::Monotonic, app::PublishPolicy::LatestOnly}) {
        const bool monotonic = policy == app::PublishPolicy::Monotonic;
        CAPTURE(monotonic);
        auto scene = islands_scene(1);
        std::vector<app::SceneBuilding> none;
        Turnstile turnstile;
        Recorder rec;
        app::ScenePicture::Limits limits;
        limits.publish = policy;
        limits.build_fault = [&turnstile](std::size_t) { turnstile.pass(); };
        limits.trace = rec.seam();
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
        const Opener opener(turnstile);
        render::FrameRenderer frames(1);
        picture.apply({added(1, 0, 0)}); // version k adds a building at x = k - 1
        REQUIRE(eventually([&] { return turnstile.entered.load() == 1; }));
        std::vector<std::uint64_t> during;
        for (std::uint64_t k = 2; k <= kLast; ++k) {
            CAPTURE(k);
            picture.apply({added(k, 0, static_cast<std::uint16_t>(k - 1))});
            turnstile.released.fetch_add(1); // version k - 1 finishes; version k is held
            REQUIRE(eventually([&] { return turnstile.entered.load() == static_cast<int>(k); }));
            REQUIRE(settled(picture));
            if (picture.commit(frames) > 0) {
                during.push_back(rec.committed(0).back());
                // An intermediate version: its own buildings, and the newer edits still owed.
                CHECK(shown(picture, 0) == during.back());
                CHECK(picture.pending() == 1);
            }
        }
        if (monotonic) {
            CHECK(during == std::vector<std::uint64_t>{1, 3, 5, 7});
            CHECK(picture.stats().version_requeues == 3);
        } else {
            CHECK(during.empty());
            CHECK(shown(picture, 0) == 0);
            CHECK(picture.stats().version_requeues == 0);
        }
        // The stream stops: both converge to the last version.
        turnstile.open.store(true);
        CHECK(drain(picture, frames, 40));
        CHECK(picture.error().empty());
        CHECK(shown(picture, 0) == kLast);
        CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
        CHECK(rec.committed(0).back() == kLast);
        CHECK(rec.violation().empty());
        CHECK(picture.stats().outstanding == 0);
        CHECK(picture.stats().outstanding_bytes == 0);
    }
}

TEST_CASE("publish: a newer job queued behind an intermediate commit is moved onto it and builds "
          "the desired state" *
          doctest::test_suite("scene_picture_publish")) {
    // One worker. Island 0's version 1 is held in its build; island 1's job and island 0's version
    // 2 queue behind it, in that order. Version 1 finishes and the worker takes island 1's job, so
    // version 2 is still queued when the commit publishes version 1: the queued job moves to the
    // new generation and is built from it, with no requeue.
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    Turnstile turnstile;
    Recorder rec;
    app::ScenePicture::Limits limits;
    limits.build_fault = [&turnstile](std::size_t) { turnstile.pass(); };
    limits.trace = rec.seam();
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
    const Opener opener(turnstile);
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    REQUIRE(eventually([&] { return turnstile.entered.load() == 1; }));
    picture.apply({added(2, 1, 1)});
    picture.apply({added(3, 0, 2)});
    turnstile.released.fetch_add(1); // version 1 finishes; island 1's build is held
    REQUIRE(eventually([&] { return turnstile.entered.load() == 2; }));
    REQUIRE(settled(picture));
    const auto rebased = picture.stats().rebased;
    CHECK(picture.commit(frames) == 1);
    CHECK(shown(picture, 0) == 1);
    CHECK(picture.stats().rebased == rebased + 1);
    turnstile.open.store(true);
    CHECK(drain(picture, frames, 40));
    CHECK(picture.error().empty());
    CHECK(picture.stats().version_requeues == 0);
    CHECK(rec.committed(0) == std::vector<std::uint64_t>{1, 2});
    CHECK(shown(picture, 0) == 2);
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK(committed_payloads(*picture.scene(), i) == fresh_payloads(*scene, i, picture));
    }
    CHECK(rec.violation().empty());
}

TEST_CASE("publish: a late older result is rejected after a newer commit, and an older result "
          "finishing first is replaced" *
          doctest::test_suite("scene_picture_publish")) {
    // Two workers. Version 1's build is held while version 2 is built on the other worker.
    for (const bool commit_first : {true, false}) {
        CAPTURE(commit_first);
        auto scene = islands_scene(1);
        std::vector<app::SceneBuilding> none;
        Gate gate;
        Recorder rec;
        app::ScenePicture::Limits limits;
        limits.build_fault = [&gate](std::size_t) {
            if (gate.entered.load() == 0) {
                gate.pass();
            }
        };
        limits.trace = rec.seam();
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2, limits);
        const Opener opener(gate);
        render::FrameRenderer frames(1);
        picture.apply({added(1, 0, 1)});
        REQUIRE(eventually([&] { return gate.entered.load() == 1; }));
        picture.apply({added(2, 0, 2)});
        REQUIRE(eventually([&] { return rec.count(app::PictureEvent::Built) == 1; }));
        if (commit_first) {
            // Version 2 is committed; version 1 finishes after it and is older than the committed
            // version: rejected on its worker, its credit released, nothing published.
            REQUIRE(settled(picture));
            CHECK(picture.commit(frames) == 1);
            CHECK(shown(picture, 0) == 2);
            gate.open.store(true);
            picture.wait_idle();
            CHECK(picture.stats().discarded == 1);
            CHECK(rec.count(app::PictureEvent::Superseded) == 1);
        } else {
            // Version 1 finishes while version 2's result waits: the newer result is kept, the
            // older one destroyed.
            gate.open.store(true);
            picture.wait_idle();
            CHECK(picture.stats().coalesced == 1);
            REQUIRE(settled(picture));
            CHECK(picture.commit(frames) == 1);
        }
        CHECK(picture.stats().outstanding == 0);
        CHECK(picture.pending() == 0);
        CHECK(rec.committed(0) == std::vector<std::uint64_t>{2});
        CHECK(shown(picture, 0) == 2);
        CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
        CHECK(rec.violation().empty());
    }
}

TEST_CASE("publish: one, two and four workers each hold a version ahead at a barrier, and the "
          "newest is published" *
          doctest::test_suite("scene_picture_publish")) {
    // The Monotonic counterpart of "accounting: one, two and four workers each hold a superseded
    // generation at a barrier". Island 0's builds block at the barrier, each from a different
    // generation (island 1 commits between them). All are ahead of the committed version, so none
    // is discarded: when they finish, the newest finished result replaces the older ones
    // (coalesced) and is the one published. Each build keeps its credit and its generation until
    // it finishes.
    for (const std::size_t workers : {std::size_t{1}, std::size_t{2}, std::size_t{4}}) {
        CAPTURE(workers);
        auto scene = islands_scene(2);
        std::vector<app::SceneBuilding> none;
        Gate gate;
        Recorder rec;
        std::mutex seen_mutex;
        std::vector<std::uint64_t> destroyed;
        auto& reclaimer = render::SceneReclaimer::shared();
        reclaimer.set_test_observer([&seen_mutex, &destroyed](const render::Scene& s) {
            const std::lock_guard lock(seen_mutex);
            destroyed.push_back(s.generation());
        });
        const ObserverReset reset;
        {
            app::ScenePicture::Limits limits;
            limits.max_outstanding = workers + 1;
            limits.build_fault = [&gate](std::size_t index) {
                if (index == 0) {
                    gate.pass();
                }
            };
            limits.trace = rec.seam();
            app::ScenePicture picture(
                scene, none, [](std::uint16_t) { return true; }, workers, limits);
            const Opener opener(gate);
            render::FrameRenderer frames(1);
            std::uint64_t turn = 1;
            std::vector<std::uint64_t> held;
            for (std::size_t i = 0; i < workers; ++i) {
                held.push_back(picture.scene()->generation());
                picture.apply({added(turn++, 0, static_cast<std::uint16_t>(i + 1))});
                REQUIRE(eventually([&] { return gate.entered.load() == static_cast<int>(i + 1); }));
                if (i + 1 < workers) {
                    picture.apply({added(turn++, 1, static_cast<std::uint16_t>(i + 1))});
                    REQUIRE(eventually([&] { return picture.timings().build_ms.size() == i + 1; }));
                    REQUIRE(settled(picture));
                    REQUIRE(picture.commit(frames) == 1);
                }
            }
            CHECK(picture.stats().outstanding == workers);
            {
                const std::lock_guard lock(seen_mutex);
                for (std::size_t i = 0; i + 1 < held.size(); ++i) {
                    CHECK(std::find(destroyed.begin(), destroyed.end(), held[i]) ==
                          destroyed.end());
                }
            }
            gate.open.store(true);
            picture.wait_idle();
            CHECK(picture.stats().discarded == 0);
            CHECK(picture.stats().coalesced == workers - 1);
            CHECK(drain(picture, frames, 40));
            CHECK(picture.error().empty());
            CHECK(picture.stats().outstanding == 0);
            CHECK(rec.committed(0) == std::vector<std::uint64_t>{workers});
            CHECK(rec.violation().empty());
            CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
            CHECK(committed_payloads(*picture.scene(), 1) == fresh_payloads(*scene, 1, picture));
            if (held.size() > 2) {
                REQUIRE(eventually([&] {
                    const std::lock_guard lock(seen_mutex);
                    return std::all_of(held.begin() + 1, held.end() - 1, [&](std::uint64_t g) {
                        return std::find(destroyed.begin(), destroyed.end(), g) != destroyed.end();
                    });
                }));
            }
        }
        CHECK(eventually([] {
            const auto r = render::SceneReclaimer::shared().stats();
            return r.waiting == 0 && r.in_flight == 0;
        }));
    }
}

TEST_CASE("publish: competing islands under one admission both advance during the stream" *
          doctest::test_suite("scene_picture_publish")) {
    // Two islands edited in every round, island 0 twice, with one admission: by count, or by a
    // byte budget every build exceeds alone. Each round: the admitted build finishes, island 0
    // changes and is built, then island 1 and island 0 change again and wait for the admission.
    // The finished build is still ahead of the committed version, so every round publishes one
    // island; the commit's credit is released before the commit starts the deferred builds, so the
    // waiting island is admitted there, in the order it waited, and is not passed over by the next
    // round's edit of island 0. Both advance during the stream, alternating; no build runs beside
    // another; both converge.
    struct Config {
        const char* name;
        std::size_t max_outstanding, max_outstanding_bytes;
    };
    for (const Config config :
         {Config{"one credit", 1, std::size_t{64} << 20}, Config{"byte budget", 8, 1}}) {
        for (const std::size_t workers : {std::size_t{1}, std::size_t{2}, std::size_t{4}}) {
            CAPTURE(config.name);
            CAPTURE(workers);
            auto scene = islands_scene(2);
            std::vector<app::SceneBuilding> none;
            Recorder rec;
            app::ScenePicture::Limits limits;
            limits.max_outstanding = config.max_outstanding;
            limits.max_outstanding_bytes = config.max_outstanding_bytes;
            limits.trace = rec.seam();
            app::ScenePicture picture(
                scene, none, [](std::uint16_t) { return true; }, workers, limits);
            render::FrameRenderer frames(1);
            constexpr std::uint64_t kRounds = 12;
            std::uint64_t turn = 1;
            std::size_t rounds_committing = 0;
            for (std::uint64_t round = 0; round < kRounds; ++round) {
                const auto x = static_cast<std::uint16_t>(round + 1);
                picture.wait_idle();
                picture.apply({added(turn++, 0, x, 1)});
                picture.wait_idle();
                picture.apply({added(turn++, 1, x, 1)});
                picture.apply({added(turn++, 0, x, 2)});
                picture.wait_idle();
                REQUIRE(settled(picture));
                rounds_committing += picture.commit(frames) > 0 ? 1U : 0U;
            }
            // Every round published; both islands advanced while the edits went on.
            CHECK(rounds_committing == kRounds);
            CHECK(rec.committed(0).size() == kRounds / 2);
            CHECK(rec.committed(1).size() == kRounds / 2);
            CHECK(picture.stats().outstanding_peak == 1);
            CHECK(drain(picture, frames, 80));
            CHECK(picture.error().empty());
            CHECK(rec.violation().empty());
            for (std::size_t i = 0; i < 2; ++i) {
                CHECK(committed_payloads(*picture.scene(), i) ==
                      fresh_payloads(*scene, i, picture));
            }
            CHECK(picture.stats().outstanding == 0);
            CHECK(picture.stats().outstanding_bytes == 0);
        }
    }
}

TEST_CASE("publish: a stalled reaper holds an intermediate result back, which is published after "
          "the release" *
          doctest::test_suite("scene_picture_publish")) {
    // The Monotonic counterpart of "picture recovery: a result of an older version is never
    // published after a newer change". The reaper is stalled; island 0's version 2 finishes, its
    // commit is skipped, and version 3 arrives while the reaper is behind: the island in flight is
    // deferred, not replaced. After the release, version 2 is published (newer than the committed
    // version 1, not newer than the desired 3), then version 3. Nothing older than the committed
    // version is ever published.
    auto scene = islands_scene(2);
    std::atomic<bool> open{false};
    std::vector<app::SceneBuilding> none;
    Recorder rec;
    app::ScenePicture::Limits limits;
    limits.max_reclaim_backlog = 0;
    limits.reclaim_gate = [&open] {
        while (!open.load()) {
            std::this_thread::yield();
        }
    };
    limits.trace = rec.seam();
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 1);
    picture.apply({added(2, 0, 2)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 0); // skipped: the reaper is behind
    const auto skipped = picture.stats().commit_skipped;
    CHECK(skipped >= 1);
    picture.apply({added(3, 0, 3)}); // deferred while the reaper is behind
    CHECK(picture.commit(frames) == 0);
    CHECK(picture.stats().commit_skipped > skipped);
    CHECK(shown(picture, 0) == 1); // the last valid state stays
    open.store(true);
    REQUIRE(eventually(
        [&] { return picture.stats().reclaim_completed == picture.stats().reclaim_queued; }));
    REQUIRE(settled(picture));
    CHECK(picture.commit(frames) == 1); // version 2, the intermediate result
    CHECK(shown(picture, 0) == 2);
    CHECK(picture.pending() == 1);
    CHECK(drain(picture, frames, 40));
    CHECK(shown(picture, 0) == 3);
    CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
    CHECK(rec.committed(0) == std::vector<std::uint64_t>{1, 2, 3});
    CHECK(rec.violation().empty());
    CHECK(picture.stats().outstanding_bytes == 0);
}

TEST_CASE("publish: transient and permanent failures under a stream; the healthy island advances "
          "and the failed one is reported after the edits stop" *
          doctest::test_suite("scene_picture_publish")) {
    // Island 0's first three builds throw (transient); every build of island 1 throws until the
    // fault is lifted (permanent). Both are edited in every round, island 0 twice. A failure counts
    // only for the desired version (an ahead build that throws is discarded); a change clears the
    // count. Island 0 converges; island 1 is reported failed once the edits stop, keeps its last
    // good state, and converges by an explicit retry after the fault is lifted.
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    std::atomic<int> transient{3};
    std::atomic<bool> permanent{true};
    Recorder rec;
    app::ScenePicture::Limits limits;
    limits.max_attempts = 3;
    limits.build_fault = [&transient, &permanent](std::size_t index) {
        if (index == 0 && transient.fetch_sub(1) > 0) {
            throw std::runtime_error("injected transient failure");
        }
        if (index == 1 && permanent.load()) {
            throw std::runtime_error("injected permanent failure");
        }
    };
    limits.trace = rec.seam();
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2, limits);
    render::FrameRenderer frames(1);
    std::uint64_t turn = 1;
    for (std::uint16_t round = 1; round <= 10; ++round) {
        picture.apply({added(turn++, 0, round, 1)});
        picture.apply({added(turn++, 1, round, 1)});
        picture.apply({added(turn++, 0, round, 2)});
        picture.wait_idle();
        REQUIRE(settled(picture));
        picture.commit(frames);
    }
    CHECK(!rec.committed(0).empty()); // island 0 advanced during the stream
    CHECK(rec.committed(1).empty());
    // The edits stop. Island 0 converges; island 1 runs out of attempts and is reported.
    for (int round = 0; round < 60; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    CHECK(picture.pending() == 1);
    CHECK(picture.stats().failed_islands == 1);
    CHECK(picture.error().find("island 1: injected permanent failure") != std::string::npos);
    CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
    CHECK(shown(picture, 1) == 0); // its last good state
    permanent.store(false);
    CHECK(picture.retry_failed() == 1);
    CHECK(drain(picture, frames, 40));
    CHECK(picture.error().empty());
    CHECK(committed_payloads(*picture.scene(), 1) == fresh_payloads(*scene, 1, picture));
    CHECK(rec.violation().empty());
}

TEST_CASE("publish: a frame-held intermediate generation and shutdown with work in flight are "
          "released on the reclaimer" *
          doctest::test_suite("scene_picture_publish")) {
    // One worker. Version 1 is published as an intermediate generation while version 2 is held in
    // its build; a frame and a handle keep that generation. Island 1's build is queued behind. The
    // bridge is destroyed with the build running, the job queued and the change owed; then the
    // frame and the handle let go. The intermediate generation is destroyed on the reclaimer after
    // that release, and the accounted bytes return to what they were before the picture existed.
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    struct Seen {
        const render::Scene* scene;
        std::thread::id thread;
    };
    std::mutex seen_mutex;
    std::vector<Seen> seen;
    auto& reclaimer = render::SceneReclaimer::shared();
    reclaimer.set_test_observer([&seen_mutex, &seen](const render::Scene& s) {
        const std::lock_guard lock(seen_mutex);
        seen.push_back({&s, std::this_thread::get_id()});
    });
    const ObserverReset reset;
    const auto before = render::payload_counts();
    Turnstile turnstile;
    std::shared_ptr<const render::Scene> held;
    const render::Scene* held_ptr = nullptr;
    auto frames = std::make_unique<render::FrameRenderer>(1);
    {
        app::ScenePicture::Limits limits;
        limits.build_fault = [&turnstile](std::size_t) { turnstile.pass(); };
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
        const Opener opener(turnstile);
        picture.apply({added(1, 0, 1)});
        REQUIRE(eventually([&] { return turnstile.entered.load() == 1; }));
        picture.apply({added(2, 0, 2)});
        turnstile.released.fetch_add(1);
        REQUIRE(eventually([&] { return turnstile.entered.load() == 2; }));
        REQUIRE(settled(picture));
        REQUIRE(picture.commit(*frames) == 1);
        held = picture.scene();
        held_ptr = held.get();
        CHECK(shown(picture, 0) == 1); // intermediate: version 1 of 2
        CHECK(picture.pending() == 1);
        const auto view = held->bounds(2);
        REQUIRE(view.has_value());
        frames->update_chunks(held, *view, 2);
        picture.apply({added(3, 1, 1)}); // queued behind the held build
        CHECK(picture.stats().outstanding == 2);
    } // the opener releases the build first; the destructor joins and hands the rest over
    {
        const std::lock_guard lock(seen_mutex);
        CHECK(std::none_of(seen.begin(), seen.end(),
                           [held_ptr](const Seen& s) { return s.scene == held_ptr; }));
    }
    frames.reset();
    held.reset(); // the last owner
    REQUIRE(eventually([&] {
        const std::lock_guard lock(seen_mutex);
        return std::any_of(seen.begin(), seen.end(),
                           [held_ptr](const Seen& s) { return s.scene == held_ptr; });
    }));
    {
        const std::lock_guard lock(seen_mutex);
        const auto it = std::find_if(seen.begin(), seen.end(),
                                     [held_ptr](const Seen& s) { return s.scene == held_ptr; });
        CHECK(it->thread != std::this_thread::get_id());
    }
    REQUIRE(eventually([&] {
        const auto now = render::payload_counts();
        return now.of(render::PayloadKind::Incremental) ==
                   before.of(render::PayloadKind::Incremental) &&
               now.of(render::PayloadKind::Generation) ==
                   before.of(render::PayloadKind::Generation) &&
               reclaimer.stats().in_flight == 0;
    }));
    CHECK(reclaimer.stats().bytes_in_flight == 0);
}
