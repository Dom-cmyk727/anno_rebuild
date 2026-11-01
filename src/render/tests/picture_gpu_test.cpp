// The play picture's generations on the GPU compositor (round ea, 6a-6g; docs/app/gpu-handoff.md).
// The window's order: the bridge commits, then one owning handle of the current generation drives
// a whole frame (chunk planning, CPU and GPU composition, readiness). A commit invalidates the
// chunk keys of its change: the cached chunks keep drawing, unchanged, until their rebuilds from
// the new generation arrive, key by key. Two comparisons, kept apart:
//  - during a transition the GPU output equals the CPU composition of the same inputs (the same
//    renderer, the same cached chunks), and equals what was shown before (an old chunk is not
//    relabelled as new because the frame holds a newer scene);
//  - once every affected visible key is fresh at the same moment (the readiness boundary), the
//    GPU output equals the CPU picture of a freshly loaded scene of the desired state.
// Ordering is made with a gate on the chunk workers (ChunkWorkers::set_test_gate), never with
// sleeps. Needs the game files and a GPU adapter (skips without them, as the other GPU cases).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/legal_workload.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/picture_trace.hpp>
#include <anno/app/play.hpp>
#include <anno/app/play_follow.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/gpu.hpp>
#include <anno/render/scene.hpp>
#include <anno/render/wgpu.hpp>
#include <anno/test/game_dir.hpp>
#include <anno/test/watchdog.hpp>

namespace {

namespace fs = std::filesystem;
namespace render = anno::render;
namespace gpu = anno::render::gpu;
namespace app = anno::app;
using render::ChunkKey;
using SceneHandle = std::shared_ptr<const render::Scene>;

fs::path fixed_map() {
    return fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
}

/// A bounded wait on a condition (yields; no fixed sleep).
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

/// Holds the chunk builds `hold` names (a key and the generation the build reads) until the
/// predicate stops naming them or the gate is opened for good. Declared before the renderer whose
/// workers it gates, and opened by a GateOpener declared after it, so no worker is left blocked
/// when the workers are joined.
struct KeyGate {
    std::mutex m;
    std::condition_variable cv;
    bool open = false;
    std::function<bool(const ChunkKey&, std::uint64_t)> hold;
    std::set<std::pair<ChunkKey, std::uint64_t>> waiting;
    std::uint64_t passed = 0;

    void pass(const ChunkKey& key, std::uint64_t generation) {
        std::unique_lock lock(m);
        auto held = [&] { return !open && hold && hold(key, generation); };
        if (held()) {
            waiting.insert({key, generation});
            cv.notify_all();
            cv.wait(lock, [&] { return !held(); });
            waiting.erase({key, generation});
        }
        ++passed;
        cv.notify_all();
    }
    void set(std::function<bool(const ChunkKey&, std::uint64_t)> h) {
        const std::lock_guard lock(m);
        hold = std::move(h);
        cv.notify_all();
    }
    /// Waits (bounded) until `p` holds, evaluated under the gate's lock.
    template <class P> bool wait_for(P p) {
        std::unique_lock lock(m);
        return cv.wait_for(lock, std::chrono::seconds(60), p);
    }
    void open_for_good() {
        const std::lock_guard lock(m);
        open = true;
        cv.notify_all();
    }
};
struct GateOpener {
    KeyGate& gate;
    explicit GateOpener(KeyGate& g) : gate(g) {}
    GateOpener(const GateOpener&) = delete;
    GateOpener& operator=(const GateOpener&) = delete;
    ~GateOpener() { gate.open_for_good(); }
};

/// Clears the reclaimer's test observer (declared after what it captures).
struct ObserverReset {
    ObserverReset() = default;
    ObserverReset(const ObserverReset&) = delete;
    ObserverReset& operator=(const ObserverReset&) = delete;
    ~ObserverReset() { render::SceneReclaimer::shared().set_test_observer({}); }
};

/// The GPU device, or why there is none (a skip unless ANNO_REQUIRE_GPU is set).
std::optional<gpu::Context> open_gpu() {
    auto ctx = gpu::headless_context();
    if (!ctx) {
        if (std::getenv("ANNO_REQUIRE_GPU") != nullptr) {
            FAIL("no GPU device (ANNO_REQUIRE_GPU is set): " << ctx.error());
        }
        MESSAGE("skipping: no GPU device: " << ctx.error());
        return std::nullopt;
    }
    MESSAGE("adapter " << ctx.value().name);
    return std::move(ctx).value();
}

/// The compositor for exact comparisons: every upload in the frame it is received, open sea for a
/// missing chunk (no stand-in), no dissolve.
std::unique_ptr<gpu::GpuFrame> make_frame(const gpu::Context& ctx, gpu::GpuLimits limits = {}) {
    limits.uploads_per_frame = static_cast<std::size_t>(-1);
    limits.stand_ins = false;
    limits.chunk_fade_ms = 0;
    auto made = gpu::GpuFrame::create(ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    return std::move(made).value();
}

/// The fixed map's scene with `buildings`, loaded on its own (the oracle and the base), rivers
/// off as in the CPU verifiers (the flow is asynchronous and rebuilds chunks for its own reason).
SceneHandle scene_with(const fs::path& dir, const app::PlayLoad& play,
                       const std::vector<app::SceneBuilding>& buildings,
                       std::optional<render::ZoomLevels> levels = std::nullopt) {
    app::SceneObjects objects;
    objects.is_building = [&play](std::uint16_t id) {
        return app::anlage_is_building(play.world, id);
    };
    objects.buildings = buildings;
    auto loaded = app::load_placed_scene(dir, play.name, play.placed, &objects);
    REQUIRE(loaded.ok());
    app::LoadedScene ls = std::move(loaded).value();
    ls.scene->river_cfg.enabled = false;
    if (levels) {
        ls.scene->set_levels(*levels);
    }
    return SceneHandle(std::move(ls.scene));
}

/// A group-2 view of 1024 x 768 scene pixels centred on island-local tile (x, y) of island
/// `number` (legal_workload_test's anchor).
render::Rect view_at(const render::Scene& scene, std::uint32_t number, std::int32_t x,
                     std::int32_t y) {
    std::size_t index = scene.islands.size();
    for (std::size_t i = 0; i < scene.islands.size(); ++i) {
        if (scene.islands[i].number == number) {
            index = i;
        }
    }
    REQUIRE(index < scene.islands.size());
    for (const render::Placement& p : scene.layouts[2]) {
        if (p.index == index) {
            const auto [tw, th] = render::kTile[2];
            const int cx2 = 2 * x + 1, cy2 = 2 * y + 1;
            const int sx = (cx2 - cy2) * tw / 4 + p.offset.first;
            const int sy = (cx2 + cy2) * th / 4 + p.offset.second;
            return render::Rect{sx - 512, sy - 384, 1024, 768};
        }
    }
    FAIL("no layout for the island");
    return render::Rect{};
}

std::vector<ChunkKey> visible_keys(render::Rect view, std::size_t group) {
    std::vector<ChunkKey> keys;
    for (const auto& [cx, cy] : render::visible_chunks(view)) {
        keys.emplace_back(group, cx, cy);
    }
    return keys;
}

/// Largest per-channel difference between a CPU frame (0RGB words) and a GPU read-back (RGBA
/// bytes); the GPU cases' bound is 1 (play_gpu_test).
int channel_delta(const std::vector<std::uint32_t>& cpu, const std::vector<std::uint8_t>& gpu_px) {
    REQUIRE(gpu_px.size() == cpu.size() * 4);
    int max = 0;
    for (std::size_t i = 0; i < cpu.size(); ++i) {
        for (std::size_t k = 0; k < 3; ++k) {
            const int c = static_cast<int>((cpu[i] >> (16 - 8 * k)) & 0xFF);
            max = std::max(max, std::abs(c - int{gpu_px[4 * i + k]}));
        }
    }
    return max;
}

/// The largest per-channel difference and the pixels whose difference is above 1 and above 2.
struct Parity {
    int max = 0;
    std::size_t over = 0, over2 = 0;
};
Parity parity(const std::vector<std::uint32_t>& cpu, const std::vector<std::uint8_t>& gpu_px) {
    REQUIRE(gpu_px.size() == cpu.size() * 4);
    Parity p;
    for (std::size_t i = 0; i < cpu.size(); ++i) {
        int m = 0;
        for (std::size_t k = 0; k < 3; ++k) {
            const int c = static_cast<int>((cpu[i] >> (16 - 8 * k)) & 0xFF);
            m = std::max(m, std::abs(c - int{gpu_px[4 * i + k]}));
        }
        p.max = std::max(p.max, m);
        p.over += m > 1 ? 1U : 0U;
        p.over2 += m > 2 ? 1U : 0U;
    }
    return p;
}

/// A compositor, its renderer (chunks without ships, as the viewer's) with gated workers, and an
/// offscreen target per view size.
struct Gpu {
    std::unique_ptr<gpu::GpuFrame> frame;
    std::map<std::pair<int, int>, std::pair<gpu::Texture, gpu::TextureView>> targets;

    gpu::GpuStats draw(render::FrameRenderer& frames, const SceneHandle& gen, render::Rect view,
                       std::size_t group, render::WaterStyle style = render::WaterStyle::Original) {
        auto it = targets.find({view.w, view.h});
        if (it == targets.end()) {
            it = targets
                     .emplace(std::pair{view.w, view.h},
                              frame->offscreen_target(static_cast<std::uint32_t>(view.w),
                                                      static_cast<std::uint32_t>(view.h)))
                     .first;
        }
        return frame->render(frames, gen, view, group, 0, style, it->second.second.get());
    }
    std::vector<std::uint8_t> pixels(render::Rect view) {
        const auto& t = targets.at({view.w, view.h});
        return frame->read_back(t.first.get(), static_cast<std::uint32_t>(view.w),
                                static_cast<std::uint32_t>(view.h));
    }
};

/// The renderer's CPU composition of the same inputs the GPU just drew from: the same renderer,
/// whose visible chunks the gate keeps as they are (update_chunks takes nothing new for them).
std::vector<std::uint32_t> cpu_same_inputs(render::FrameRenderer& frames, const SceneHandle& gen,
                                           render::Rect view, std::size_t group) {
    std::vector<std::uint32_t> out(static_cast<std::size_t>(view.w) *
                                   static_cast<std::size_t>(view.h));
    frames.render(gen, view, group, 0, render::WaterStyle::Original, out.data());
    return out;
}

/// The CPU picture of `scene` alone (its own renderer, chunks built in the frame, no ships in the
/// chunks: the scene has no units, checked by the callers).
std::vector<std::uint32_t> cpu_reference(const SceneHandle& scene, render::Rect view,
                                         std::size_t group) {
    render::FrameRenderer frames(4, false);
    std::vector<std::uint32_t> out(static_cast<std::size_t>(view.w) *
                                   static_cast<std::size_t>(view.h));
    for (int round = 0; round < 3; ++round) {
        frames.render(scene, view, group, 0, render::WaterStyle::Original, out.data());
    }
    return out;
}

bool fresh(const render::FrameRenderer& frames, const ChunkKey& k) {
    return frames.chunk(k) != nullptr && frames.stale_generation(k) == 0;
}

/// Draws until every visible key is cached and fresh and the workers are idle (the gate holding
/// nothing), then once more.
void settle(Gpu& g, render::FrameRenderer& frames, render::ChunkWorkers& workers,
            const SceneHandle& gen, render::Rect view, std::size_t group) {
    const auto keys = visible_keys(view, group);
    for (int round = 0; round < 400; ++round) {
        const auto s = g.draw(frames, gen, view, group);
        if (s.chunks_missing == 0 && !frames.busy() &&
            std::all_of(keys.begin(), keys.end(),
                        [&](const ChunkKey& k) { return fresh(frames, k); })) {
            break;
        }
        workers.wait_idle();
    }
    g.draw(frames, gen, view, group);
    for (const ChunkKey& k : keys) {
        REQUIRE(fresh(frames, k));
    }
}

/// Commits everything the picture has (the reaper and the reclaimer allowed to catch up first,
/// so a commit is not skipped for a reason these cases do not test).
void drain(app::ScenePicture& picture, render::FrameRenderer& frames) {
    for (int round = 0; round < 40 && picture.pending() > 0; ++round) {
        picture.wait_idle();
        REQUIRE(eventually([&] {
            const auto st = picture.stats();
            return st.reclaim_completed == st.reclaim_queued &&
                   render::SceneReclaimer::shared().stats().waiting == 0;
        }));
        picture.commit(frames);
    }
    REQUIRE(picture.pending() == 0);
    REQUIRE(picture.error().empty());
}

/// The workload of these cases: two cities, a dozen buildings each, built through the sim.
std::unique_ptr<app::LegalWorkload> workload(const fs::path& dir) {
    app::LegalWorkloadOptions options;
    options.cities = 2;
    options.buildings_per_city = 12;
    options.settle_turns = 2;
    auto built = app::LegalWorkload::build(dir, fixed_map(), options);
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return std::move(built).value();
}

/// The visible keys at `group` of the last commit's islands (their own invalidations).
std::vector<ChunkKey> affected_visible(const app::ScenePicture& picture, render::Rect view,
                                       std::size_t group) {
    const auto visible = visible_keys(view, group);
    const std::set<ChunkKey> in_view(visible.begin(), visible.end());
    std::set<ChunkKey> keys;
    for (const auto& island : picture.last_commit()) {
        for (const ChunkKey& k : island.keys) {
            if (in_view.contains(k)) {
                keys.insert(k);
            }
        }
    }
    return {keys.begin(), keys.end()};
}
} // namespace

TEST_CASE("picture GPU: accepted builds and an accepted demolish keep the old chunks drawing, "
          "become ready only when every affected key is fresh, and then equal a fresh scene" *
          doctest::test_suite("picture_gpu")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    const auto w = workload(*dir);
    const auto state0 = w->buildings();
    // Step 1: two accepted builds of city 0 in one commit (the island's newest version), in one
    // view: the readiness of a set of several keys. Step 2: one accepted demolish of city 0.
    struct Step {
        std::vector<app::LegalEdit> edits;
        std::size_t group = 2;
        std::vector<app::SceneBuilding> state;
    };
    std::vector<Step> steps(2);
    steps[0].edits = {w->build_one(0), w->build_one(0)};
    steps[0].state = w->buildings();
    steps[1].edits = {w->demolish_one(0)};
    steps[1].state = w->buildings();
    for (const Step& st : steps) {
        for (const auto& e : st.edits) {
            REQUIRE_MESSAGE(e.accepted, e.reason);
            REQUIRE(e.batch != nullptr);
        }
    }
    CHECK(w->invariants().empty());
    SceneHandle base = scene_with(*dir, w->load(), state0);
    REQUIRE(base->units.empty()); // no ships: the ship-less CPU composition is comparable

    Gpu g{make_frame(*ctx), {}};
    std::set<ChunkKey> held;
    KeyGate gate;
    render::FrameRenderer frames(2, false);
    auto owned = std::make_unique<render::ChunkWorkers>(2);
    render::ChunkWorkers& workers = *owned;
    workers.set_test_gate([&gate](const ChunkKey& k, std::uint64_t gen) { gate.pass(k, gen); });
    frames.set_workers(std::move(owned));
    const GateOpener opener(gate);
    app::ScenePicture picture(
        base, state0, [&w](std::uint16_t id) { return app::anlage_is_building(w->world(), id); },
        1);
    base.reset(); // the picture holds generation 0 now

    bool partial_seen = false;
    for (std::size_t n = 0; n < steps.size(); ++n) {
        const Step& step = steps[n];
        CAPTURE(n);
        const std::size_t group = step.group;
        const SceneHandle before = picture.scene();
        // The view: the box of the step's edits at group 2 (their anchors, and 640 x 512 pixels
        // around them), at the step's group.
        render::Rect view{};
        {
            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            for (std::size_t i = 0; i < step.edits.size(); ++i) {
                const auto& e = step.edits[i];
                const render::Rect r = view_at(*before, e.island, e.x, e.y);
                const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
                x0 = i == 0 ? cx : std::min(x0, cx);
                y0 = i == 0 ? cy : std::min(y0, cy);
                x1 = i == 0 ? cx : std::max(x1, cx);
                y1 = i == 0 ? cy : std::max(y1, cy);
            }
            view = render::Rect{x0 - 640, y0 - 512, x1 - x0 + 1280, y1 - y0 + 1024};
            view = render::zoomed_view(view, 2, group);
        }
        settle(g, frames, workers, before, view, group);
        const auto old_px = g.pixels(view);
        // The parity of the unchanged picture at this view: what the GPU and the CPU paths
        // already differ by here, before any edit and from the same chunks. The established bound
        // is 1 (blending rounding, "GPU frames match the CPU path"), measured there on views
        // without the per-frame water sprites; a view of this workload with many of them differs
        // by 2 at a few pixels on Metal (OQ-525: two truncating CPU blends). The comparisons below
        // may not exceed what the unchanged picture shows; the transition and fresh-scene checks
        // are exact.
        const Parity baseline = parity(cpu_same_inputs(frames, before, view, group), old_px);
        MESSAGE("step " << n << ": GPU against the CPU path before the edit: max channel delta "
                        << baseline.max << ", " << baseline.over << " pixel(s) above 1, "
                        << before->sprites_in(view, group, 0, render::SpriteLayer::WaterBelow,
                                              render::WaterStyle::Original)
                                   .size() +
                               before
                                   ->sprites_in(view, group, 0, render::SpriteLayer::WaterAbove,
                                                render::WaterStyle::Original)
                                   .size()
                        << " water sprite(s) in view");
        WARN(baseline.max <= 1);
        const int bound = std::max(baseline.max, 1);

        std::vector<std::shared_ptr<const app::BuildingBatch>> batches;
        for (const auto& e : step.edits) {
            batches.push_back(e.batch);
        }
        picture.apply(batches);
        drain(picture, frames);
        const SceneHandle gen = picture.scene();
        REQUIRE(gen->generation() == before->generation() + 1); // one commit
        const auto affected = affected_visible(picture, view, group);
        REQUIRE(!affected.empty());
        {
            // Under the gate's lock: the workers evaluate the predicate that reads it.
            const std::lock_guard lock(gate.m);
            held = std::set<ChunkKey>(affected.begin(), affected.end());
        }
        const std::uint64_t g_new = gen->generation();
        gate.set([&held, g_new](const ChunkKey& k, std::uint64_t at) {
            return at >= g_new && held.contains(k);
        });

        // The transition frame: every affected key still draws its old chunk, marked stale; the
        // frame holds the new generation, but nothing it shows is labelled as rebuilt.
        g.draw(frames, gen, view, group);
        REQUIRE(gate.wait_for([&] { return !gate.waiting.empty(); }));
        g.draw(frames, gen, view, group);
        for (const ChunkKey& k : affected) {
            REQUIRE(frames.chunk(k) != nullptr);
            CHECK(frames.chunk(k)->generation < g_new);
            CHECK(frames.stale_generation(k) == g_new);
        }
        CHECK(g.pixels(view) == old_px);
        {
            const Parity t = parity(cpu_same_inputs(frames, gen, view, group), g.pixels(view));
            CHECK(t.max == baseline.max); // the same inputs as before the edit
            CHECK(t.over == baseline.over);
        }

        // Readiness: the island's affected keys, ready only when all are fresh at one poll.
        app::ReadinessTracker readiness;
        readiness.add(
            {static_cast<std::uint32_t>(picture.last_commit().front().index), g_new, affected});
        std::vector<app::ReadinessTracker::KeyEvent> log;
        auto poll = [&] {
            return readiness.poll([&frames](const ChunkKey& k) { return fresh(frames, k); }, 0,
                                  log);
        };
        CHECK(poll().empty());

        // One key at a time, in the order the workers took them.
        while (!held.empty()) {
            std::optional<ChunkKey> next;
            REQUIRE(gate.wait_for([&] {
                for (const auto& [k, at] : gate.waiting) {
                    if (at >= g_new && held.contains(k)) {
                        next = k;
                        return true;
                    }
                }
                return false;
            }));
            {
                const std::lock_guard lock(gate.m);
                held.erase(*next);
                gate.cv.notify_all();
            }
            REQUIRE(eventually([&] {
                g.draw(frames, gen, view, group);
                return fresh(frames, *next) && frames.chunk(*next)->generation == g_new;
            }));
            const auto px = g.pixels(view);
            CHECK(channel_delta(cpu_same_inputs(frames, gen, view, group), px) <= bound);
            const bool all = std::all_of(affected.begin(), affected.end(),
                                         [&](const ChunkKey& k) { return fresh(frames, k); });
            const auto ready = poll();
            CHECK(ready.empty() == !all);
            if (!all) {
                partial_seen = true; // a key fresh, another still stale: not ready
                for (const ChunkKey& k : affected) {
                    if (!fresh(frames, k)) {
                        CHECK(frames.chunk(k)->generation < g_new);
                    }
                }
                CHECK(px != old_px); // the fresh key shows its change already
            }
        }
        // Ready: the GPU equals the fresh scene of the state this generation represents, drawn by
        // a compositor of its own (exactly) and by the CPU path (within the bound).
        g.draw(frames, gen, view, group);
        const auto new_px = g.pixels(view);
        const SceneHandle oracle = scene_with(*dir, w->load(), step.state);
        {
            Gpu other{make_frame(*ctx), {}};
            render::FrameRenderer own(4, false);
            for (int round = 0; round < 3; ++round) {
                other.draw(own, oracle, view, group);
            }
            CHECK(other.pixels(view) == new_px);
        }
        CHECK(channel_delta(cpu_reference(oracle, view, group), new_px) <= bound);
        CHECK(new_px != old_px); // the edit is visible
        CHECK(g.frame->errors() == 0);
        MESSAGE("step " << n << " (" << step.edits.size() << " edit(s), group " << group
                        << "): " << affected.size() << " affected visible keys");
    }
    // A set of two or more keys was seen partly fresh, and not ready then.
    CHECK(partial_seen);
}

TEST_CASE("picture GPU: a late completion of an older generation after a newer invalidation "
          "replaces nothing and clears no mark" *
          doctest::test_suite("picture_gpu")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    const auto w = workload(*dir);
    const auto state0 = w->buildings();
    const app::LegalEdit build = w->build_one(0);
    REQUIRE_MESSAGE(build.accepted, build.reason);
    REQUIRE(build.batch != nullptr);
    const auto state1 = w->buildings();
    // The same building removed again (the batch a demolish of it publishes): generation 2 shows
    // state 0's buildings, generation 1 the building.
    auto undo = std::make_shared<app::BuildingBatch>();
    undo->turn = build.batch->turn + 1;
    for (const app::BuildingDelta& d : build.batch->deltas) {
        REQUIRE(d.kind == app::BuildingDelta::Kind::Added);
        undo->deltas.push_back({app::BuildingDelta::Kind::Removed, d.building});
    }
    SceneHandle base = scene_with(*dir, w->load(), state0);
    REQUIRE(base->units.empty());

    Gpu g{make_frame(*ctx), {}};
    std::set<std::pair<ChunkKey, std::uint64_t>> held;
    KeyGate gate;
    render::FrameRenderer frames(2, false);
    auto owned = std::make_unique<render::ChunkWorkers>(2);
    render::ChunkWorkers& workers = *owned;
    workers.set_test_gate([&gate](const ChunkKey& k, std::uint64_t gen) { gate.pass(k, gen); });
    frames.set_workers(std::move(owned));
    const GateOpener opener(gate);
    app::ScenePicture picture(
        base, state0, [&w](std::uint16_t id) { return app::anlage_is_building(w->world(), id); },
        1);
    const render::Rect view = view_at(*base, build.island, build.x, build.y);
    settle(g, frames, workers, base, view, 2);
    const auto old_px = g.pixels(view);
    base.reset();

    // Generation 1: its rebuilds start and are held.
    picture.apply({build.batch});
    drain(picture, frames);
    const SceneHandle gen1 = picture.scene();
    const auto affected = affected_visible(picture, view, 2);
    REQUIRE(!affected.empty());
    const std::set<ChunkKey> keys(affected.begin(), affected.end());
    gate.set([&keys](const ChunkKey& k, std::uint64_t at) { return at >= 1 && keys.contains(k); });
    g.draw(frames, gen1, view, 2);
    REQUIRE(gate.wait_for([&] { return !gate.waiting.empty(); }));
    std::set<ChunkKey> in_flight;
    {
        const std::lock_guard lock(gate.m);
        for (const auto& [k, at] : gate.waiting) {
            CHECK(at == 1);
            in_flight.insert(k);
        }
    }
    // Generation 2 invalidates the same keys while the generation-1 builds are in flight.
    picture.apply({undo});
    drain(picture, frames);
    const SceneHandle gen2 = picture.scene();
    REQUIRE(gen2->generation() == 2);
    const auto again = affected_visible(picture, view, 2);
    for (const ChunkKey& k : affected) {
        CHECK(std::find(again.begin(), again.end(), k) != again.end());
        CHECK(frames.stale_generation(k) == 2);
    }
    // The generation-1 builds finish now, late; generation-2 builds are held.
    const std::uint64_t discarded_before = frames.stale_discarded();
    gate.set([&keys](const ChunkKey& k, std::uint64_t at) { return at >= 2 && keys.contains(k); });
    REQUIRE(eventually([&] {
        g.draw(frames, gen2, view, 2);
        return frames.stale_discarded() >= discarded_before + in_flight.size();
    }));
    for (const ChunkKey& k : affected) {
        REQUIRE(frames.chunk(k) != nullptr);
        CHECK(frames.chunk(k)->generation == 0); // the late generation-1 chunk replaced nothing
        CHECK(frames.stale_generation(k) == 2);  // and cleared no mark
    }
    g.draw(frames, gen2, view, 2);
    const auto px = g.pixels(view);
    CHECK(px == old_px); // the GPU never showed the stale building
    const SceneHandle oracle1 = scene_with(*dir, w->load(), state1);
    CHECK(channel_delta(cpu_reference(oracle1, view, 2), px) > 1); // it would have been visible
    // The generation-2 builds complete: fresh, and equal to the fresh scene of state 0.
    gate.set({});
    REQUIRE(eventually([&] {
        g.draw(frames, gen2, view, 2);
        return std::all_of(affected.begin(), affected.end(),
                           [&](const ChunkKey& k) { return fresh(frames, k); });
    }));
    for (const ChunkKey& k : affected) {
        CHECK(frames.chunk(k)->generation == 2);
    }
    g.draw(frames, gen2, view, 2);
    const SceneHandle oracle0 = scene_with(*dir, w->load(), state0);
    CHECK(channel_delta(cpu_reference(oracle0, view, 2), g.pixels(view)) <= 1);
    CHECK(g.frame->errors() == 0);
}

TEST_CASE("picture GPU: a frame's generation stays alive across a commit while its jobs run, and "
          "its CPU chunks are freed while GPU work is outstanding" *
          doctest::test_suite("picture_gpu")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    const auto w = workload(*dir);
    const auto state0 = w->buildings();
    const app::LegalEdit build = w->build_one(0);
    REQUIRE_MESSAGE(build.accepted, build.reason);
    const auto state1 = w->buildings();
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
    auto destroyed = [&](const render::Scene* s) {
        const std::lock_guard lock(seen_mutex);
        return std::any_of(seen.begin(), seen.end(), [s](const Seen& x) { return x.scene == s; });
    };

    Gpu g{make_frame(*ctx), {}};
    KeyGate gate;
    render::FrameRenderer frames(2, false);
    auto owned = std::make_unique<render::ChunkWorkers>(2);
    render::ChunkWorkers& workers = *owned;
    workers.set_test_gate([&gate](const ChunkKey& k, std::uint64_t gen) { gate.pass(k, gen); });
    frames.set_workers(std::move(owned));
    const GateOpener opener(gate);
    SceneHandle base = scene_with(*dir, w->load(), state0);
    const render::Scene* gen0_ptr = base.get();
    std::weak_ptr<const render::Scene> gen0_weak = base;
    app::ScenePicture picture(
        base, state0, [&w](std::uint16_t id) { return app::anlage_is_building(w->world(), id); },
        1);
    const render::Rect view = view_at(*base, build.island, build.x, build.y);
    settle(g, frames, workers, base, view, 2);

    // CPU chunk lifetime: the cached chunks of the affected keys are replaced after the commit
    // while the frames submitted meanwhile are never waited for; the old CPU chunks are freed
    // (on the workers' reaper) without the GPU needing them, and the GPU shows the new pixels.
    picture.apply({build.batch});
    {
        // A pan to the other city's island, not built yet: its jobs read generation 0 and are
        // held, so generation 0 is owned by them across the commit below.
        // One job is held (the first of generation 0 to start): the other worker stays free
        // for the rebuilds of generation 1.
        const render::Rect far = view_at(*base, w->cities().at(1).island_slot, 20, 20);
        gate.set([pinned = std::optional<ChunkKey>()](const ChunkKey& k, std::uint64_t at) mutable {
            if (at != 0) {
                return false;
            }
            if (!pinned) {
                pinned = k;
            }
            return *pinned == k;
        });
        g.draw(frames, base, far, 2);
        REQUIRE(gate.wait_for([&] { return !gate.waiting.empty(); }));
    }
    drain(picture, frames); // generation 1 is current; the bridge gave generation 0 up
    const SceneHandle gen1 = picture.scene();
    REQUIRE(gen1->generation() == 1);
    const auto affected = affected_visible(picture, view, 2);
    REQUIRE(!affected.empty());
    std::vector<std::weak_ptr<const render::Chunk>> old_chunks;
    for (const ChunkKey& k : affected) {
        old_chunks.push_back(frames.chunk_shared(k));
    }
    base.reset();
    REQUIRE(eventually([&] {
        const auto st = picture.stats();
        return st.reclaim_completed == st.reclaim_queued && reclaimer.stats().waiting == 0;
    }));
    // Generation 0 is still alive: the held jobs own it; the compositor keeps no handle.
    CHECK(!gen0_weak.expired());
    CHECK(!destroyed(gen0_ptr));
    // Frames of generation 1 (the gate holds generation 0 only): no wait between submissions.
    REQUIRE(eventually([&] {
        g.draw(frames, gen1, view, 2);
        return std::all_of(affected.begin(), affected.end(),
                           [&](const ChunkKey& k) { return fresh(frames, k); });
    }));
    g.draw(frames, gen1, view, 2);
    REQUIRE(eventually([&] {
        g.draw(frames, gen1, view, 2); // the reaper frees the replaced chunks meanwhile
        return std::all_of(old_chunks.begin(), old_chunks.end(),
                           [](const auto& c) { return c.expired(); });
    }));
    const SceneHandle oracle1 = scene_with(*dir, w->load(), state1);
    CHECK(channel_delta(cpu_reference(oracle1, view, 2), g.pixels(view)) <= 1);
    CHECK(!destroyed(gen0_ptr));
    // The held jobs finish: generation 0's last owners let go, and it is destroyed on the
    // reclaimer.
    gate.set({});
    REQUIRE(eventually([&] { return destroyed(gen0_ptr); }));
    {
        const std::lock_guard lock(seen_mutex);
        const auto it = std::find_if(seen.begin(), seen.end(),
                                     [gen0_ptr](const Seen& s) { return s.scene == gen0_ptr; });
        CHECK(it->thread != std::this_thread::get_id());
    }
    CHECK(g.frame->errors() == 0);
}

TEST_CASE("picture GPU: an offscreen edit is fresh after the view returns, at its group and at a "
          "derived level" *
          doctest::test_suite("picture_gpu")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    const auto w = workload(*dir);
    const auto state0 = w->buildings();
    const app::LegalEdit build = w->build_one(0);
    REQUIRE_MESSAGE(build.accepted, build.reason);
    const auto state1 = w->buildings();
    const render::ZoomLevels levels = render::ZoomLevels::with_derived(1);
    SceneHandle base = scene_with(*dir, w->load(), state0, levels);
    REQUIRE(base->units.empty());
    Gpu g{make_frame(*ctx), {}};
    KeyGate gate; // never holds: the case orders nothing, it only follows the views
    render::FrameRenderer frames(2, false);
    auto owned = std::make_unique<render::ChunkWorkers>(2);
    render::ChunkWorkers& workers = *owned;
    frames.set_workers(std::move(owned));
    app::ScenePicture picture(
        base, state0, [&w](std::uint16_t id) { return app::anlage_is_building(w->world(), id); },
        1);
    const render::Rect here = view_at(*base, build.island, build.x, build.y);
    const render::Rect away = view_at(*base, w->cities().at(1).island_slot, 20, 20);
    const render::Rect derived = render::zoomed_view(here, 2, 3, levels);
    settle(g, frames, workers, base, here, 2);
    settle(g, frames, workers, base, derived, 3); // the derived level is cached too
    settle(g, frames, workers, base, away, 2);
    base.reset();

    picture.apply({build.batch});
    drain(picture, frames);
    const SceneHandle gen1 = picture.scene();
    // The change's keys are not in the view shown: an offscreen edit.
    CHECK(affected_visible(picture, away, 2).empty());
    std::size_t derived_marked = 0;
    for (const auto& island : picture.last_commit()) {
        for (const ChunkKey& k : island.keys) {
            derived_marked += std::get<0>(k) == 3 ? 1U : 0U;
        }
    }
    CHECK(derived_marked > 0); // the commit marked the derived level as well
    settle(g, frames, workers, gen1, away, 2);
    // The view returns: the affected keys rebuild from generation 1 and equal the fresh scene.
    const SceneHandle oracle = scene_with(*dir, w->load(), state1, levels);
    settle(g, frames, workers, gen1, here, 2);
    CHECK(channel_delta(cpu_reference(oracle, here, 2), g.pixels(here)) <= 1);
    // The derived level, reduced from the fresh children: exactly what a compositor of its own
    // shows for the fresh scene, and within the derived levels' bound of the CPU path (3: the
    // derived sea pattern is smoothed from a reduced pattern, "GPU frames at derived levels match
    // the CPU path").
    settle(g, frames, workers, gen1, derived, 3);
    for (const ChunkKey& k : visible_keys(derived, 3)) {
        CHECK(frames.stale_generation(k) == 0);
    }
    {
        Gpu other{make_frame(*ctx), {}};
        render::FrameRenderer own(4, false);
        for (int round = 0; round < 3; ++round) {
            other.draw(own, oracle, derived, 3);
        }
        CHECK(other.pixels(derived) == g.pixels(derived));
        // (Views of one size share a target: each is drawn again before it is read.)
        other.draw(own, oracle, here, 2);
        const auto other_here = other.pixels(here);
        g.draw(frames, gen1, here, 2);
        CHECK(other_here == g.pixels(here));
        g.draw(frames, gen1, derived, 3);
    }
    const Parity d = parity(cpu_reference(oracle, derived, 3), g.pixels(derived));
    MESSAGE("derived level 3 against the CPU path: max "
            << d.max << ", " << d.over << " pixel(s) above 1, " << d.over2 << " above 2");
    CHECK(d.max <= 3);
    CHECK(d.over2 <=
          static_cast<std::size_t>(derived.w) * static_cast<std::size_t>(derived.h) / 10000);
    CHECK(g.frame->errors() == 0);
}

TEST_CASE("picture GPU: the wave simulation's background preparation holds the frame's "
          "generation, not a borrowed reference" *
          doctest::test_suite("picture_gpu")) {
    // The compositor copies a depth function into the background preparation of the wave
    // simulation's window (GpuLimits::background_fluid), which can run after render() returns.
    // It must own the generation it reads (round ea finding: it captured the scene by reference,
    // so a generation reclaimed meanwhile would have been read after its destruction).
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    auto loaded = app::load_play(*dir, fixed_map());
    REQUIRE(loaded.ok());
    const auto buildings = app::play_buildings(loaded.value().world);
    const SceneHandle base = scene_with(*dir, loaded.value(), buildings);
    gpu::GpuLimits limits;
    limits.background_fluid = true;
    limits.wave_simulation = true;
    Gpu g{make_frame(*ctx, limits), {}};
    render::FrameRenderer frames(2, false);
    // A generation of its own (the same islands), released by this case below.
    SceneHandle gen = base->with_islands({});
    REQUIRE(gen->generation() == 1);
    const std::weak_ptr<const render::Scene> weak = gen;
    const auto b = base->bounds(1);
    REQUIRE(b.has_value());
    const render::Rect view{b->x + b->w / 2 - 320, b->y + b->h / 2 - 180, 640, 360};
    const auto s = g.draw(frames, gen, view, 1, render::WaterStyle::Enhanced);
    if (!s.wave_simulation || !s.fluid_setting_up) {
        MESSAGE("skipping: the wave simulation did not start a background preparation here");
        return;
    }
    gen.reset();
    // The preparation holds the generation (no other owner is left).
    CHECK(!weak.expired());
    // Later frames of another generation apply the finished preparation and let it go.
    REQUIRE(eventually([&] {
        g.draw(frames, base, view, 1, render::WaterStyle::Enhanced);
        return weak.expired();
    }));
    CHECK(g.frame->errors() == 0);
}

namespace {

using anno::test::Watchdog;

/// A scoped release of a gate: it runs once, explicitly or at scope exit (an assertion or an
/// exception included), and logs that it ran. Declared after the object whose workers the gate
/// blocks, so it is destroyed, and releases, before that object joins them.
struct ScopedRelease {
    std::function<void()> release;
    std::vector<std::string>& log;
    const char* name;
    bool done = false;
    ScopedRelease(std::function<void()> r, std::vector<std::string>& l, const char* n)
        : release(std::move(r)), log(l), name(n) {}
    ScopedRelease(const ScopedRelease&) = delete;
    ScopedRelease& operator=(const ScopedRelease&) = delete;
    void operator()() {
        if (!done) {
            done = true;
            release();
            log.emplace_back(name);
        }
    }
    ~ScopedRelease() { (*this)(); }
};
/// An owner that logs when it has been destroyed (its workers joined).
template <class T> struct Logged {
    std::unique_ptr<T> p;
    std::vector<std::string>& log;
    const char* name;
    Logged(std::unique_ptr<T> owned, std::vector<std::string>& l, const char* n)
        : p(std::move(owned)), log(l), name(n) {}
    Logged(const Logged&) = delete;
    Logged& operator=(const Logged&) = delete;
    void reset() {
        if (p) {
            p.reset();
            log.emplace_back(name);
        }
    }
    ~Logged() { reset(); }
};

/// The bridge's builder blocked in its second build until opened.
struct BuildGate {
    std::atomic<int> builds{0};
    std::atomic<bool> open{false};
    void pass() {
        if (builds.fetch_add(1) >= 1) {
            while (!open.load()) {
                std::this_thread::yield();
            }
        }
    }
};

std::size_t position(const std::vector<std::string>& log, const char* name) {
    return static_cast<std::size_t>(std::find(log.begin(), log.end(), name) - log.begin());
}

/// Teardown with a build blocked in the bridge's builder, chunk jobs held by the gate and a GPU
/// frame submitted and not waited for. `fail_early`: an exception is thrown once both gates hold
/// their workers, before any explicit release (the scoped releases must do it). Otherwise the
/// viewer's order is followed explicitly. Returns the generations it made (raw identity and weak).
struct TeardownResult {
    std::vector<const render::Scene*> generations;
    std::vector<std::weak_ptr<const render::Scene>> alive;
    std::vector<std::string> order;
};
void teardown_scenario(const fs::path& dir, const gpu::Context& ctx, const app::LegalWorkload& w,
                       const std::vector<app::SceneBuilding>& state0, const app::LegalEdit& first,
                       const app::LegalEdit& second, bool fail_early, TeardownResult& out) {
    BuildGate builds; // outlives the bridge
    KeyGate gate;     // outlives the chunk workers
    app::ScenePicture::Limits limits;
    limits.build_fault = [&builds](std::size_t) { builds.pass(); };
    SceneHandle base = scene_with(dir, w.load(), state0);
    out.generations.push_back(base.get());
    out.alive.push_back(base);
    Logged<app::ScenePicture> picture(
        std::make_unique<app::ScenePicture>(
            base, state0, [&w](std::uint16_t id) { return app::anlage_is_building(w.world(), id); },
            1, limits),
        out.order, "bridge joined");
    const render::Rect view = view_at(*base, first.island, first.x, first.y);
    base.reset();
    Gpu g{make_frame(ctx), {}};
    auto renderer = std::make_unique<render::FrameRenderer>(2, false);
    auto owned = std::make_unique<render::ChunkWorkers>(2);
    owned->set_test_gate([&gate](const ChunkKey& k, std::uint64_t gen) { gate.pass(k, gen); });
    renderer->set_workers(std::move(owned));
    Logged<render::FrameRenderer> frames(std::move(renderer), out.order, "chunk workers joined");
    // The releases, declared last: destroyed (and released) before the owners above join.
    ScopedRelease release_builder([&builds] { builds.open.store(true); }, out.order,
                                  "builder released");
    ScopedRelease release_chunks([&gate] { gate.open_for_good(); }, out.order,
                                 "chunk gate released");
    picture.p->apply({first.batch});
    drain(*picture.p, *frames.p);
    out.generations.push_back(picture.p->scene().get());
    out.alive.push_back(picture.p->scene());
    gate.set([](const ChunkKey&, std::uint64_t) { return true; });
    g.draw(*frames.p, picture.p->scene(), view, 2);
    REQUIRE(gate.wait_for([&] { return !gate.waiting.empty(); }));
    picture.p->apply({second.batch});
    REQUIRE(eventually([&] { return builds.builds.load() >= 2; }));
    if (fail_early) {
        throw std::runtime_error("injected failure after both gates hold their workers");
    }
    // The viewer's order: the frames stop (the chunk jobs end and are joined), the GPU finishes
    // and the compositor goes, then the bridge.
    release_chunks();
    frames.reset();
    g.frame->wait();
    g.targets.clear();
    g.frame.reset();
    release_builder();
    picture.reset();
}

/// After a teardown: every generation released and destroyed on the reclaimer thread, and the
/// accounted bytes back at `before`.
void check_released(const TeardownResult& r, std::mutex& seen_mutex,
                    const std::vector<std::pair<const render::Scene*, std::thread::id>>& seen,
                    const render::PayloadCounts& before) {
    for (const auto& a : r.alive) {
        CHECK(a.expired());
    }
    REQUIRE(eventually([&] {
        const std::lock_guard lock(seen_mutex);
        return std::all_of(r.generations.begin(), r.generations.end(), [&](const render::Scene* s) {
            return std::any_of(seen.begin(), seen.end(),
                               [s](const auto& x) { return x.first == s; });
        });
    }));
    {
        const std::lock_guard lock(seen_mutex);
        for (const auto& [scene, thread] : seen) {
            CHECK(thread != std::this_thread::get_id());
        }
    }
    auto& reclaimer = render::SceneReclaimer::shared();
    REQUIRE(eventually([&] {
        const auto now = render::payload_counts();
        const auto st = reclaimer.stats();
        return now.of(render::PayloadKind::Incremental) ==
                   before.of(render::PayloadKind::Incremental) &&
               now.of(render::PayloadKind::Generation) ==
                   before.of(render::PayloadKind::Generation) &&
               st.waiting == 0 && st.in_flight == 0;
    }));
}

/// The common setup of the two teardown cases: the workload's two builds, the reclaimer observer
/// and the payload baseline (taken once nothing is waiting for the reclaimer: the workload's setup
/// may still have a scene in destruction).
void teardown_case(bool fail_early) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    const auto w = workload(*dir);
    const auto state0 = w->buildings();
    const app::LegalEdit first = w->build_one(0);
    const app::LegalEdit second = w->build_one(0);
    REQUIRE_MESSAGE(first.accepted, first.reason);
    REQUIRE_MESSAGE(second.accepted, second.reason);
    std::mutex seen_mutex;
    std::vector<std::pair<const render::Scene*, std::thread::id>> seen;
    auto& reclaimer = render::SceneReclaimer::shared();
    reclaimer.set_test_observer([&seen_mutex, &seen](const render::Scene& s) {
        const std::lock_guard lock(seen_mutex);
        seen.emplace_back(&s, std::this_thread::get_id());
    });
    const ObserverReset reset;
    REQUIRE(eventually([&] {
        const auto r = reclaimer.stats();
        return r.waiting == 0 && r.in_flight == 0;
    }));
    const auto before = render::payload_counts();
    TeardownResult r;
    if (fail_early) {
        // Bounded: a scope exit that left a worker blocked would hang in its owner's join.
        const Watchdog dog(std::chrono::seconds(600), "the early-failure teardown");
        std::string what;
        try {
            teardown_scenario(*dir, *ctx, *w, state0, first, second, true, r);
        } catch (const std::runtime_error& e) {
            what = e.what();
        }
        CHECK(what == "injected failure after both gates hold their workers");
    } else {
        teardown_scenario(*dir, *ctx, *w, state0, first, second, false, r);
    }
    // Each gate was released before the owner of the workers it blocked joined them.
    INFO("order: " << [&r] {
        std::string o;
        for (const auto& e : r.order) {
            o += e + "; ";
        }
        return o;
    }());
    REQUIRE(r.order.size() == 4);
    CHECK(position(r.order, "chunk gate released") < position(r.order, "chunk workers joined"));
    CHECK(position(r.order, "builder released") < position(r.order, "bridge joined"));
    check_released(r, seen_mutex, seen, before);
}

} // namespace

TEST_CASE("picture GPU: teardown with builds, chunk jobs and GPU work outstanding releases every "
          "generation through the reclaimer" *
          doctest::test_suite("picture_gpu")) {
    teardown_case(false);
}

TEST_CASE("picture GPU: a failure after the chunk workers and the builder are held releases both "
          "gates before their owners join, and still releases every generation" *
          doctest::test_suite("picture_gpu")) {
    teardown_case(true);
}

// ---- The interactive window's constrained path (round eb, Package 3) ----------------------------
//
// The cases above upload every chunk in the frame it is received and never dissolve. The viewer's
// limits (GpuLimits::interactive) bound uploads per frame and per millisecond (a chunk then takes
// several frames, a plane at a time), show stand-ins and dissolve chunks in over them. The states
// of an affected key, kept apart: CPU-ready (cached, not stale), GPU-pending (CPU-ready but the
// compositor's copy is older, staging or dissolving) and GPU-usable (GpuFrame::chunk_state: the
// copy's serial is the CPU chunk's, not fading).

namespace {

/// Holds every build of the bridge in its build seam (PictureLimits::build_fault) until released;
/// counts the builds that entered (round ec, A2).
struct HeldBuilds {
    std::mutex m;
    std::condition_variable cv;
    bool open = false;
    int entered = 0;
    void pass() {
        std::unique_lock lock(m);
        ++entered;
        cv.notify_all();
        cv.wait(lock, [this] { return open; });
    }
    /// A bounded observation: true once `n` builds are held.
    bool wait_entered(int n) {
        std::unique_lock lock(m);
        return cv.wait_for(lock, std::chrono::seconds(60), [&] { return entered >= n; });
    }
    void release() {
        const std::lock_guard lock(m);
        open = true;
        cv.notify_all();
    }
};

bool gpu_usable(const gpu::GpuFrame& f, const render::FrameRenderer& frames, const ChunkKey& k) {
    if (!fresh(frames, k)) {
        return false;
    }
    const auto st = f.chunk_state(k);
    return st.serial == frames.chunk(k)->serial && !st.fading;
}

/// A compositor with the given upload limits and dissolve, on a fade clock the case steps by hand.
std::unique_ptr<gpu::GpuFrame> constrained_frame(const gpu::Context& ctx, std::size_t per_frame,
                                                 double budget_ms, bool stand_ins,
                                                 std::uint64_t fade_ms,
                                                 const std::shared_ptr<std::uint64_t>& clock) {
    gpu::GpuLimits limits;
    limits.uploads_per_frame = per_frame;
    limits.upload_budget_ms = budget_ms;
    limits.stand_ins = stand_ins;
    limits.chunk_fade_ms = fade_ms;
    auto made = gpu::GpuFrame::create(ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    auto f = std::move(made).value();
    f->set_fade_clock([clock] { return *clock; });
    return f;
}

/// What a compositor of its own, with no limits, shows for `scene` at `view` (the GPU oracle).
std::vector<std::uint8_t> gpu_fresh(const gpu::Context& ctx, const SceneHandle& scene,
                                    render::Rect view, std::size_t group) {
    Gpu other{make_frame(ctx), {}};
    render::FrameRenderer own(4, false);
    for (int round = 0; round < 3; ++round) {
        other.draw(own, scene, view, group);
    }
    return other.pixels(view);
}

/// What a paused loop did, for its failure message and the case's record (round ed, package 4):
/// the frames drawn, the wall time, the last frame's demand (why the loop had not idled), the
/// visible keys' GPU states, the fade clock, and the frames since the visible state last changed
/// while no CPU work was outstanding.
struct LoopReport {
    std::size_t frames = 0, visible = 0, usable = 0, fading = 0, staging = 0, uploads = 0;
    std::size_t idle_cpu_frames_unchanged = 0, max_idle_cpu_frames_unchanged = 0;
    double seconds = 0;
    bool chunks_busy = false, play_work = false, gpu_work = false;
    std::uint64_t clock = 0;
    /// How the loop ended: idle (the demand let it stop), stalled (PausedLoop::stall_frames), or
    /// the wall-clock watchdog.
    bool idle = false, stalled = false, watchdog = false;
    [[nodiscard]] std::string text() const {
        std::ostringstream o;
        o << (idle       ? "idle"
              : stalled  ? "STALLED"
              : watchdog ? "WATCHDOG"
                         : "running")
          << " frames=" << frames << " seconds=" << seconds
          << " demand: chunks_busy=" << chunks_busy << " play_work=" << play_work
          << " gpu_work=" << gpu_work << "; visible keys " << visible << ": usable=" << usable
          << " fading=" << fading << " staging=" << staging << " uploads_waiting=" << uploads
          << "; fade clock=" << clock
          << "; frames with no CPU work and no change: " << idle_cpu_frames_unchanged << " (max "
          << max_idle_cpu_frames_unchanged << ")";
        return o.str();
    }
};

/// The visible keys' GPU states after a frame (LoopReport).
void observe(LoopReport& r, const gpu::GpuFrame& f, const render::FrameRenderer& frames,
             const std::vector<ChunkKey>& keys) {
    r.visible = keys.size();
    r.usable = r.fading = r.staging = r.uploads = 0;
    for (const ChunkKey& k : keys) {
        const auto st = f.chunk_state(k);
        r.fading += st.fading ? 1U : 0U;
        r.staging += st.staging ? 1U : 0U;
        const bool cpu = fresh(frames, k);
        r.usable += cpu && st.serial == frames.chunk(k)->serial && !st.fading ? 1U : 0U;
        r.uploads += cpu && st.serial != frames.chunk(k)->serial ? 1U : 0U;
    }
}

/// How a paused loop is bounded and clocked (round ed, package 4).
struct PausedLoop {
    /// The compositor's fade clock and how far it moves per frame drawn: the viewer's clock is
    /// steady time, so it moves while the window draws (0: held where the case put it).
    std::shared_ptr<std::uint64_t> clock;
    std::uint64_t clock_ms_per_frame = 0;
    /// The work-based bound: this many frames in a row with no CPU work outstanding (no chunk
    /// job, nothing pending in the bridge) and no change in the visible keys' GPU states (usable,
    /// fading, staging, waiting for an upload) is a stall. Each frame uploads at least one plane
    /// and a dissolve ends on a moving clock, so steady progress changes the states well within
    /// it.
    std::size_t stall_frames = 3000;
    /// The wall-clock watchdog: it bounds a broken case; it never decides that the work finished.
    std::chrono::seconds watchdog{600};
};

/// The window's paused loop on the GPU path, without a window: a first frame (the viewer marks
/// itself dirty when the view or the state changes), then a frame while the production FrameDemand
/// (play_follow.hpp) wants one, until the demand lets it idle, it stalls, or the watchdog ends it.
/// `each` sees every frame.
template <class Each>
LoopReport paused_loop(Gpu& g, render::FrameRenderer& frames, app::ScenePicture& picture,
                       render::Rect view, std::size_t group, Each each, const PausedLoop& opt) {
    LoopReport r;
    const auto keys = visible_keys(view, group);
    const auto start = std::chrono::steady_clock::now();
    std::tuple<std::size_t, std::size_t, std::size_t, std::size_t> last{};
    for (;;) {
        app::FrameDemand d;
        d.dirty = r.frames == 0; // the first frame after a change of view or state, as the viewer's
        d.paused = true;
        d.chunks_busy = frames.busy();
        d.play_work = picture.pending() > 0;
        d.gpu_work = g.frame->work_pending();
        r.chunks_busy = d.chunks_busy;
        r.play_work = d.play_work;
        r.gpu_work = d.gpu_work;
        r.clock = opt.clock ? *opt.clock : 0;
        r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (!d.draw()) {
            r.idle = true;
            return r;
        }
        if (r.idle_cpu_frames_unchanged >= opt.stall_frames) {
            r.stalled = true;
            return r;
        }
        if (std::chrono::steady_clock::now() - start > opt.watchdog) {
            r.watchdog = true;
            return r;
        }
        if (opt.clock && opt.clock_ms_per_frame > 0) {
            *opt.clock += opt.clock_ms_per_frame;
        }
        picture.commit(frames);
        g.draw(frames, picture.scene(), view, group);
        ++r.frames;
        observe(r, *g.frame, frames, keys);
        const auto now = std::tuple{r.usable, r.fading, r.staging, r.uploads};
        if (!d.chunks_busy && !d.play_work && now == last) {
            r.max_idle_cpu_frames_unchanged =
                std::max(r.max_idle_cpu_frames_unchanged, ++r.idle_cpu_frames_unchanged);
        } else {
            r.idle_cpu_frames_unchanged = 0;
        }
        last = now;
        each(r.frames);
        std::this_thread::yield();
    }
}

/// paused_loop, required to idle: a stall or the watchdog fails the case with the report.
template <class Each>
std::size_t paused_frames(Gpu& g, render::FrameRenderer& frames, app::ScenePicture& picture,
                          render::Rect view, std::size_t group, Each each,
                          LoopReport* report = nullptr, const PausedLoop& opt = {}) {
    const LoopReport r = paused_loop(g, frames, picture, view, group, each, opt);
    if (report != nullptr) {
        *report = r;
    }
    REQUIRE_MESSAGE(r.idle, "the paused loop did not idle: " << r.text());
    return r.frames;
}

/// The pixels of chunk `k`'s part of `view` in a read-back of `view`.
std::vector<std::uint8_t> region(const std::vector<std::uint8_t>& px, render::Rect view,
                                 const ChunkKey& k) {
    const render::Rect c = render::chunk_rect(std::get<1>(k), std::get<2>(k));
    const int x0 = std::max(view.x, c.x), x1 = std::min(view.x + view.w, c.x + c.w);
    const int y0 = std::max(view.y, c.y), y1 = std::min(view.y + view.h, c.y + c.h);
    std::vector<std::uint8_t> out;
    for (int y = y0; y < y1; ++y) {
        const auto row = static_cast<std::size_t>(y - view.y) * static_cast<std::size_t>(view.w);
        const auto from = (row + static_cast<std::size_t>(x0 - view.x)) * 4;
        const auto to = (row + static_cast<std::size_t>(x1 - view.x)) * 4;
        out.insert(out.end(), px.begin() + static_cast<std::ptrdiff_t>(from),
                   px.begin() + static_cast<std::ptrdiff_t>(to));
    }
    return out;
}

/// Two accepted builds of city 0 in one commit and their covering group-2 view (3 affected keys
/// in the first case above), on a fresh workload.
struct TwoBuilds {
    std::unique_ptr<app::LegalWorkload> w;
    std::vector<app::SceneBuilding> state0, state1;
    std::vector<app::LegalEdit> edits;
    SceneHandle base;
    render::Rect view{};
};
TwoBuilds two_builds(const fs::path& dir) {
    TwoBuilds t;
    t.w = workload(dir);
    t.state0 = t.w->buildings();
    t.edits = {t.w->build_one(0), t.w->build_one(0)};
    for (const auto& e : t.edits) {
        REQUIRE_MESSAGE(e.accepted, e.reason);
        REQUIRE(e.batch != nullptr);
    }
    t.state1 = t.w->buildings();
    t.base = scene_with(dir, t.w->load(), t.state0);
    REQUIRE(t.base->units.empty());
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    for (std::size_t i = 0; i < t.edits.size(); ++i) {
        const render::Rect r = view_at(*t.base, t.edits[i].island, t.edits[i].x, t.edits[i].y);
        const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        x0 = i == 0 ? cx : std::min(x0, cx);
        y0 = i == 0 ? cy : std::min(y0, cy);
        x1 = i == 0 ? cx : std::max(x1, cx);
        y1 = i == 0 ? cy : std::max(y1, cy);
    }
    t.view = render::Rect{x0 - 640, y0 - 512, x1 - x0 + 1280, y1 - y0 + 1024};
    return t;
}

std::vector<std::shared_ptr<const app::BuildingBatch>> batches_of(const TwoBuilds& t) {
    std::vector<std::shared_ptr<const app::BuildingBatch>> b;
    for (const auto& e : t.edits) {
        b.push_back(e.batch);
    }
    return b;
}

/// The batch that removes again what `t`'s builds added (a demolish of them, as the sim
/// publishes it).
std::shared_ptr<const app::BuildingBatch> undo_of(const TwoBuilds& t) {
    auto undo = std::make_shared<app::BuildingBatch>();
    undo->turn = t.edits.back().batch->turn + 1;
    for (const auto& e : t.edits) {
        for (const app::BuildingDelta& d : e.batch->deltas) {
            REQUIRE(d.kind == app::BuildingDelta::Kind::Added);
            undo->deltas.push_back({app::BuildingDelta::Kind::Removed, d.building});
        }
    }
    return undo;
}

} // namespace

TEST_CASE(
    "picture GPU, constrained: one upload per frame finishes a multi-chunk edit while paused, "
    "and the paused window idles only once every affected key is GPU-usable" *
    doctest::test_suite("picture_gpu")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    TwoBuilds t = two_builds(*dir);
    const auto clock = std::make_shared<std::uint64_t>(0);
    // One chunk per frame, and at most 0.05 ms of plane uploads per frame (at least one plane):
    // a chunk takes several frames.
    Gpu g{constrained_frame(*ctx, 1, 0.05, false, 0, clock), {}};
    // Chunks are built in the frame (no workers): the CPU is never busy, so the uploads are the
    // only work left once a frame has rebuilt the affected keys, and only the compositor's pending
    // work keeps the paused window drawing (FrameDemand::gpu_work; without it the window would idle
    // with the edit not on the GPU).
    render::FrameRenderer frames(2, false);
    // The edit's build is held in the bridge's build seam (round ec, A2): the state "built nothing
    // yet" is made by the gate, not by the build being slower than the next line. The release is
    // declared after the picture, whose destruction joins the builder, so every exit releases it.
    HeldBuilds held;
    std::vector<std::string> order;
    app::ScenePicture::Limits limits;
    limits.build_fault = [&held](std::size_t) { held.pass(); };
    app::ScenePicture picture(
        t.base, t.state0,
        [&t](std::uint16_t id) { return app::anlage_is_building(t.w->world(), id); }, 1, limits);
    ScopedRelease release_build([&held] { held.release(); }, order, "build released");
    const auto visible = visible_keys(t.view, 2);
    paused_frames(g, frames, picture, t.view, 2, [](std::size_t) {});
    for (const ChunkKey& k : visible) {
        REQUIRE(gpu_usable(*g.frame, frames, k));
    }
    picture.apply(batches_of(t));
    REQUIRE(held.wait_entered(1)); // the worker is held inside the build
    // A slow test thread (the delay orders nothing: the gate keeps the build from finishing).
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(picture.commit(frames) == 0); // nothing is built yet: nothing to commit
    CHECK(picture.pending() > 0);
    CHECK(held.entered == 1);
    release_build();
    std::vector<ChunkKey> affected;
    std::size_t cpu_ready_gpu_pending = 0, staging_seen = 0;
    const std::size_t drawn = paused_frames(g, frames, picture, t.view, 2, [&](std::size_t) {
        if (affected.empty() && picture.scene()->generation() == 1) {
            affected = affected_visible(picture, t.view, 2);
        }
        for (const ChunkKey& k : affected) {
            if (fresh(frames, k) && frames.chunk(k)->generation == 1 &&
                !gpu_usable(*g.frame, frames, k)) {
                ++cpu_ready_gpu_pending; // CPU readiness is not GPU readiness
                CHECK(g.frame->work_pending());
            }
            staging_seen += g.frame->chunk_state(k).staging ? 1U : 0U;
        }
    });
    REQUIRE(affected.size() >= 2);
    MESSAGE("frames drawn while paused: " << drawn << "; frames with a CPU-ready, GPU-pending key: "
                                          << cpu_ready_gpu_pending << "; staging observations "
                                          << staging_seen);
    CHECK(cpu_ready_gpu_pending > 0);
    CHECK(staging_seen > 0);
    // Idle: every affected key is GPU-usable, and the output is the fresh scene's, exactly.
    for (const ChunkKey& k : affected) {
        CHECK(gpu_usable(*g.frame, frames, k));
        CHECK(frames.chunk(k)->generation == 1);
    }
    CHECK(!g.frame->work_pending());
    const SceneHandle oracle = scene_with(*dir, t.w->load(), t.state1);
    CHECK(g.pixels(t.view) == gpu_fresh(*ctx, oracle, t.view, 2));
    CHECK(g.frame->errors() == 0);
}

TEST_CASE("picture GPU, constrained: an edit published while uploads are outstanding ends on the "
          "newest state, every chunk shown meanwhile is one whole state, and nothing replaced is "
          "retained" *
          doctest::test_suite("picture_gpu")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    TwoBuilds t = two_builds(*dir);
    const auto clock = std::make_shared<std::uint64_t>(0);
    Gpu g{constrained_frame(*ctx, 1, 0.05, false, 0, clock), {}};
    render::FrameRenderer frames(2, false);
    frames.set_workers(std::make_unique<render::ChunkWorkers>(2));
    app::ScenePicture picture(
        t.base, t.state0,
        [&t](std::uint16_t id) { return app::anlage_is_building(t.w->world(), id); }, 1);
    paused_frames(g, frames, picture, t.view, 2, [](std::size_t) {});
    // The references: the GPU oracle of the state before (and after: the undo returns to it) and
    // of the intermediate state.
    const auto ref0 = gpu_fresh(*ctx, scene_with(*dir, t.w->load(), t.state0), t.view, 2);
    const auto ref1 = gpu_fresh(*ctx, scene_with(*dir, t.w->load(), t.state1), t.view, 2);
    CHECK(g.pixels(t.view) == ref0);
    picture.apply(batches_of(t));
    picture.wait_idle();
    REQUIRE(picture.commit(frames) == 1);
    const auto affected = affected_visible(picture, t.view, 2);
    REQUIRE(affected.size() >= 2);
    std::weak_ptr<const render::Scene> gen1 = picture.scene();
    std::vector<std::weak_ptr<const render::Chunk>> gen1_chunks;
    // Draw until a generation-1 chunk is CPU-ready while its upload is still outstanding.
    bool outstanding = false;
    for (int f = 0; f < 2000 && !outstanding; ++f) {
        g.draw(frames, picture.scene(), t.view, 2);
        for (const ChunkKey& k : affected) {
            if (fresh(frames, k) && frames.chunk(k)->generation == 1 &&
                !gpu_usable(*g.frame, frames, k)) {
                outstanding = true;
            }
        }
        std::this_thread::yield();
    }
    REQUIRE(outstanding);
    for (const ChunkKey& k : affected) {
        if (const auto c = frames.chunk_shared(k); c && c->generation == 1) {
            gen1_chunks.push_back(c);
        }
    }
    // The undo arrives now and commits generation 2 over the same keys.
    picture.apply({undo_of(t)});
    picture.wait_idle();
    REQUIRE(eventually([&] { return picture.commit(frames) == 1 || picture.pending() == 0; }));
    REQUIRE(picture.scene()->generation() == 2);
    std::size_t checked = 0;
    paused_frames(g, frames, picture, t.view, 2, [&](std::size_t) {
        const auto px = g.pixels(t.view);
        for (const ChunkKey& k : affected) {
            const auto r = region(px, t.view, k);
            // Whole states only: the compositor swaps a chunk when all its planes are made.
            CHECK((r == region(ref0, t.view, k) || r == region(ref1, t.view, k)));
            ++checked;
        }
    });
    MESSAGE("chunk regions checked during the transition: " << checked);
    for (const ChunkKey& k : affected) {
        CHECK(gpu_usable(*g.frame, frames, k));
        CHECK(frames.chunk(k)->generation == 2);
    }
    CHECK(g.pixels(t.view) == ref0);
    // Retained: the replaced generation and its CPU chunks are freed (the reaper and the
    // reclaimer run on their own threads).
    REQUIRE(eventually([&] {
        g.draw(frames, picture.scene(), t.view, 2);
        return gen1.expired() && std::all_of(gen1_chunks.begin(), gen1_chunks.end(),
                                             [](const auto& c) { return c.expired(); });
    }));
    CHECK(g.frame->errors() == 0);
}

TEST_CASE("picture GPU, constrained: a visible chunk that reaches the GPU after the fade clock was "
          "stepped dissolves on a held clock forever, and ends on a clock that moves" *
          doctest::test_suite("picture_gpu")) {
    // Round ed, package 4: round ec's deadline failure of the stand-ins case, made deterministic.
    // One visible key's chunk job is held until the clock has been stepped once (as that case did
    // after its frame cap); the chunk then reaches the GPU over its stand-in and dissolves. With
    // the clock held, the paused loop never idles though nothing is left to wait for: a stall, by
    // the work-based bound. With the clock moving, as the viewer's does, it ends on the fresh
    // scene.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    TwoBuilds t = two_builds(*dir);
    const auto clock = std::make_shared<std::uint64_t>(1000);
    constexpr std::uint64_t kFade = 150;
    Gpu g{constrained_frame(*ctx, 1, 1.0, true, kFade, clock), {}};
    const auto visible = visible_keys(t.view, 2);
    REQUIRE(visible.size() >= 2);
    const ChunkKey late = visible.front();
    KeyGate gate;
    gate.set([late](const ChunkKey& k, std::uint64_t) { return k == late; });
    render::FrameRenderer frames(2, false);
    auto workers = std::make_unique<render::ChunkWorkers>(2);
    workers->set_test_gate([&gate](const ChunkKey& k, std::uint64_t gen) { gate.pass(k, gen); });
    frames.set_workers(std::move(workers));
    const GateOpener opener(gate); // declared after the owners: released before they join
    app::ScenePicture picture(
        t.base, t.state0,
        [&t](std::uint16_t id) { return app::anlage_is_building(t.w->world(), id); }, 1);
    // Every other visible chunk reaches the GPU; the held one shows its stand-in.
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(600);
    for (std::size_t on_gpu = 0; on_gpu + 1 < visible.size();) {
        REQUIRE(std::chrono::steady_clock::now() < until);
        g.draw(frames, picture.scene(), t.view, 2);
        on_gpu = 0;
        for (const ChunkKey& k : visible) {
            const auto st = g.frame->chunk_state(k);
            on_gpu += fresh(frames, k) && st.serial == frames.chunk(k)->serial ? 1U : 0U;
        }
        std::this_thread::yield();
    }
    REQUIRE(gate.wait_for([&gate] { return !gate.waiting.empty(); }));
    CHECK(g.frame->chunk_state(late).serial == 0); // its stand-in
    // The single clock step, then the held job goes: its chunk dissolves in after the step.
    *clock += kFade;
    gate.open_for_good();
    PausedLoop bounded;
    bounded.clock = clock;
    bounded.stall_frames = 2000;
    const LoopReport held = paused_loop(g, frames, picture, t.view, 2, [](std::size_t) {}, bounded);
    MESSAGE("clock held: " << held.text());
    CHECK(held.stalled);
    CHECK(!held.chunks_busy);
    CHECK(!held.play_work);
    CHECK(held.gpu_work);
    CHECK(held.fading == 1);
    CHECK(g.frame->chunk_state(late).fading);
    // The clock moves: the dissolve ends and the loop idles, on the fresh scene.
    LoopReport moving;
    paused_frames(
        g, frames, picture, t.view, 2, [](std::size_t) {}, &moving, PausedLoop{clock, 16});
    MESSAGE("clock moving: " << moving.text());
    CHECK(gpu_usable(*g.frame, frames, late));
    CHECK(g.pixels(t.view) == gpu_fresh(*ctx, scene_with(*dir, t.w->load(), t.state0), t.view, 2));
    CHECK(g.frame->errors() == 0);
}

TEST_CASE(
    "picture GPU, constrained: with stand-ins and dissolves a rebuilt chunk is swapped whole, "
    "a chunk that replaces its stand-in dissolves in, and both end on the fresh scene" *
    doctest::test_suite("picture_gpu")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    TwoBuilds t = two_builds(*dir);
    const auto clock = std::make_shared<std::uint64_t>(1000);
    constexpr std::uint64_t kFade = 150;
    // The viewer's interactive configuration (stand-ins, 150 ms dissolve, 1 ms upload budget),
    // with one upload per frame so that transitions last several frames.
    Gpu g{constrained_frame(*ctx, 1, 1.0, true, kFade, clock), {}};
    render::FrameRenderer frames(2, false);
    frames.set_workers(std::make_unique<render::ChunkWorkers>(2));
    app::ScenePicture picture(
        t.base, t.state0,
        [&t](std::uint16_t id) { return app::anlage_is_building(t.w->world(), id); }, 1);
    // The first frames show stand-ins (nothing is built at group 1 yet): every chunk replaces one
    // and dissolves in. The clock does not move: a dissolving chunk is not GPU-usable yet.
    // Round ed (package 4): the loop ends when every visible key's CPU chunk is on the GPU
    // (dissolving or not), and must get there. Round ec ended it after 4000 frames whether or not
    // that held (its condition also waited for the margin's chunk jobs, which outlast the visible
    // ones, so it always ran to the cap): a visible chunk that reached the GPU after the single
    // clock step below then dissolved on a clock that never moved again, and the paused loop drew
    // until its 120 s deadline.
    const auto visible = visible_keys(t.view, 2);
    std::size_t fading_seen = 0;
    LoopReport first;
    const auto first_start = std::chrono::steady_clock::now();
    for (;;) {
        g.draw(frames, picture.scene(), t.view, 2);
        ++first.frames;
        std::size_t on_gpu = 0;
        for (const ChunkKey& k : visible) {
            const auto st = g.frame->chunk_state(k);
            fading_seen += st.fading ? 1U : 0U;
            if (st.fading) {
                CHECK(!gpu_usable(*g.frame, frames, k));
            }
            on_gpu += fresh(frames, k) && st.serial == frames.chunk(k)->serial ? 1U : 0U;
        }
        if (on_gpu == visible.size()) {
            break;
        }
        // The chunk jobs and the uploads progress on their own; only the watchdog bounds this.
        REQUIRE_MESSAGE(std::chrono::steady_clock::now() - first_start < std::chrono::seconds(600),
                        "the visible chunks never all reached the GPU after " << first.frames
                                                                              << " frames");
        std::this_thread::yield();
    }
    observe(first, *g.frame, frames, visible);
    MESSAGE("stand-ins: every visible chunk on the GPU after "
            << first.frames << " frames, "
            << std::chrono::duration<double>(std::chrono::steady_clock::now() - first_start).count()
            << " s; " << first.text());
    CHECK(fading_seen > 0);
    CHECK(g.frame->work_pending()); // the dissolves wait for the clock
    *clock += kFade;                // the dissolves end
    // From here the clock moves as the viewer's does while it draws (16 ms a frame), so a chunk
    // that starts to dissolve later ends too.
    const PausedLoop steady{clock, 16};
    LoopReport loop;
    paused_frames(g, frames, picture, t.view, 2, [](std::size_t) {}, &loop, steady);
    MESSAGE("stand-ins: dissolves ended in " << loop.text());
    const auto ref0 = gpu_fresh(*ctx, scene_with(*dir, t.w->load(), t.state0), t.view, 2);
    const auto ref1 = gpu_fresh(*ctx, scene_with(*dir, t.w->load(), t.state1), t.view, 2);
    CHECK(g.pixels(t.view) == ref0);
    // The edit: rebuilt chunks replace real copies, so they swap whole, without a dissolve.
    picture.apply(batches_of(t));
    picture.wait_idle();
    REQUIRE(picture.commit(frames) == 1);
    const auto affected = affected_visible(picture, t.view, 2);
    REQUIRE(affected.size() >= 2);
    std::size_t mixed_frames = 0;
    LoopReport edit;
    paused_frames(
        g, frames, picture, t.view, 2,
        [&](std::size_t) {
            const auto px = g.pixels(t.view);
            std::size_t old_n = 0, new_n = 0;
            for (const ChunkKey& k : affected) {
                CHECK(!g.frame->chunk_state(k).fading);
                const auto r = region(px, t.view, k);
                const bool old_state = r == region(ref0, t.view, k);
                const bool new_state = r == region(ref1, t.view, k);
                CHECK((old_state || new_state));
                old_n += old_state && !new_state ? 1U : 0U;
                new_n += new_state && !old_state ? 1U : 0U;
            }
            // Old and new chunks side by side across keys: allowed during a transition (contract,
            // boundary 4); within a chunk it is one state.
            mixed_frames += old_n > 0 && new_n > 0 ? 1U : 0U;
        },
        &edit, steady);
    MESSAGE("stand-ins: the edit converged in " << edit.text());
    MESSAGE("frames with old and new chunks side by side: " << mixed_frames);
    CHECK(g.pixels(t.view) == ref1);
    CHECK(g.frame->errors() == 0);
}

TEST_CASE(
    "picture GPU, constrained: leaving the view with uploads outstanding and returning ends on "
    "the fresh scene, and closing with uploads outstanding releases the chunks and the "
    "generations" *
    doctest::test_suite("picture_gpu")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    TwoBuilds t = two_builds(*dir);
    const auto clock = std::make_shared<std::uint64_t>(0);
    auto g = std::make_unique<Gpu>(Gpu{constrained_frame(*ctx, 1, 0.05, false, 0, clock), {}});
    auto frames = std::make_unique<render::FrameRenderer>(2, false);
    frames->set_workers(std::make_unique<render::ChunkWorkers>(2));
    app::ScenePicture picture(
        t.base, t.state0,
        [&t](std::uint16_t id) { return app::anlage_is_building(t.w->world(), id); }, 1);
    const render::Rect away = view_at(*t.base, t.w->cities().at(1).island_slot, 20, 20);
    paused_frames(*g, *frames, picture, t.view, 2, [](std::size_t) {});
    picture.apply(batches_of(t));
    picture.wait_idle();
    REQUIRE(picture.commit(*frames) == 1);
    const auto affected = affected_visible(picture, t.view, 2);
    // A few frames: some generation-1 chunks are built and their uploads are outstanding.
    bool outstanding = false;
    for (int f = 0; f < 2000 && !outstanding; ++f) {
        g->draw(*frames, picture.scene(), t.view, 2);
        for (const ChunkKey& k : affected) {
            outstanding = outstanding || (fresh(*frames, k) && frames->chunk(k)->generation == 1 &&
                                          !gpu_usable(*g->frame, *frames, k));
        }
        std::this_thread::yield();
    }
    REQUIRE(outstanding);
    // Away: the affected keys are not visible; their uploads in progress are dropped.
    paused_frames(*g, *frames, picture, away, 2, [](std::size_t) {});
    for (const ChunkKey& k : affected) {
        CHECK(!g->frame->chunk_state(k).staging);
    }
    // Back: everything is uploaded again and equals the fresh scene.
    paused_frames(*g, *frames, picture, t.view, 2, [](std::size_t) {});
    for (const ChunkKey& k : affected) {
        CHECK(gpu_usable(*g->frame, *frames, k));
    }
    const SceneHandle oracle = scene_with(*dir, t.w->load(), t.state1);
    CHECK(g->pixels(t.view) == gpu_fresh(*ctx, oracle, t.view, 2));
    // Close with uploads outstanding: an undo commits generation 2 and its first chunk uploads
    // start; then the compositor and the renderer go (the viewer's order) without draining.
    picture.apply({undo_of(t)});
    picture.wait_idle();
    REQUIRE(eventually([&] { return picture.commit(*frames) == 1 || picture.pending() == 0; }));
    std::weak_ptr<const render::Scene> gen2 = picture.scene();
    std::vector<std::weak_ptr<const render::Chunk>> held;
    bool staging = false;
    for (int f = 0; f < 2000 && !staging; ++f) {
        g->draw(*frames, picture.scene(), t.view, 2);
        for (const ChunkKey& k : affected) {
            if (g->frame->chunk_state(k).staging) {
                staging = true;
                held.push_back(frames->chunk_shared(k));
            }
        }
        std::this_thread::yield();
    }
    REQUIRE(staging);
    CHECK(g->frame->work_pending());
    frames.reset(); // the chunk workers are joined; the cache lets go
    g->frame->wait();
    g.reset(); // the compositor goes with an upload in progress
    for (const auto& c : held) {
        CHECK(c.expired()); // the upload's hold went with the compositor
    }
    CHECK(!gen2.expired()); // the bridge still holds its current generation
}

// ---- The water-sprite parity difference (round eb, Package 4) ----------------------------------

TEST_CASE(
    "picture GPU, water parity: the reproducer of the per-frame water sprite difference (game "
    "files)" *
    doctest::test_suite("picture_gpu_water")) {
    // OQ-525 (round eb, Package 4). A fixed camera on the plain fixed map (no workload), zoom group
    // 2, animation time 0, original water. Four pixels differ by 2 per channel, all under one water
    // object below the waterline (Anlagen 2708, frame 54) that the sea is shaded over. Cause,
    // shown by experiment (eb-runs/p4-experiment-*): the CPU reference truncates at each blend
    // (render::over, then sea::lerp4 towards the filled colour), the GPU rounds once to nearest;
    // with either CPU step rounding to nearest the view is within 1, with both it is exact. Which
    // rounding is the reference is the open question; parity is not accepted here (WARN), no
    // tolerance is raised, and the check below is the cause, not a bound.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto ctx = open_gpu();
    if (!ctx) {
        return;
    }
    auto loaded = app::load_play(*dir, fixed_map());
    REQUIRE(loaded.ok());
    auto buildings = app::play_buildings(loaded.value().world);
    app::sort_buildings(buildings);
    const SceneHandle scene = scene_with(*dir, loaded.value(), buildings);
    REQUIRE(scene->units.empty());
    // Fixed camera, zoom group 2, animation time 0, original water: the pixels of round ea.
    const render::Rect view{4171 - 64, 4574 - 48, 128, 96};
    Gpu g{make_frame(*ctx), {}};
    render::FrameRenderer own(4, false);
    for (int round = 0; round < 3; ++round) {
        g.draw(own, scene, view, 2);
    }
    const auto gpu_px = g.pixels(view);
    const auto cpu_px = cpu_reference(scene, view, 2);
    const Parity par = parity(cpu_px, gpu_px);
    MESSAGE("view " << view.x << "," << view.y << " " << view.w << "x" << view.h << ": max "
                    << par.max << ", " << par.over << " pixel(s) above 1");
    const auto below = scene->sprites_in(view, 2, 0, render::SpriteLayer::WaterBelow,
                                         render::WaterStyle::Original);
    const auto above = scene->sprites_in(view, 2, 0, render::SpriteLayer::WaterAbove,
                                         render::WaterStyle::Original);
    std::size_t over_found = 0;
    for (std::size_t i = 0; i < cpu_px.size(); ++i) {
        int m = 0;
        for (std::size_t k = 0; k < 3; ++k) {
            const int c = static_cast<int>((cpu_px[i] >> (16 - 8 * k)) & 0xFF);
            m = std::max(m, std::abs(c - int{gpu_px[4 * i + k]}));
        }
        if (m <= 1) {
            continue;
        }
        const int x = view.x + static_cast<int>(i % static_cast<std::size_t>(view.w));
        const int y = view.y + static_cast<int>(i / static_cast<std::size_t>(view.w));
        std::string covering;
        for (const auto* list : {&below, &above}) {
            for (const auto& d : *list) {
                const auto r = d.rect();
                if (x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3]) {
                    covering += std::string(list == &below ? " below" : " above") + "(anlage " +
                                std::to_string(std::get<0>(d.key)) + " shadow " +
                                std::to_string(std::get<2>(d.key) ? 1 : 0) + " frame " +
                                std::to_string(std::get<4>(d.key)) + " at " + std::to_string(r[0]) +
                                "," + std::to_string(r[1]) + " " + std::to_string(r[2]) + "x" +
                                std::to_string(r[3]) + ")";
                }
            }
        }
        // Every pixel above 1 lies under a water object below the waterline (the two truncating
        // blends); a difference anywhere else would be a new finding.
        const bool under_below = covering.find(" below(") != std::string::npos;
        CHECK_MESSAGE(under_below, "pixel " << x << "," << y << " differs by " << m
                                            << " and is under no water object below the waterline");
        ++over_found;
        MESSAGE("pixel " << x << "," << y << " cpu " << ((cpu_px[i] >> 16) & 0xFF) << ","
                         << ((cpu_px[i] >> 8) & 0xFF) << "," << (cpu_px[i] & 0xFF) << " gpu "
                         << int{gpu_px[4 * i]} << "," << int{gpu_px[4 * i + 1]} << ","
                         << int{gpu_px[4 * i + 2]} << " sprites:" << covering);
    }
    CHECK(over_found == par.over);
    WARN(par.max <= 1); // visual parity of this view: not accepted (OQ-525)
    CHECK(g.frame->errors() == 0);
}
