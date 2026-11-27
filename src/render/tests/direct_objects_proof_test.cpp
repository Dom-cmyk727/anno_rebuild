// RENDER-DIRECT-3 stage 3a proof (docs/render/direct-object-pass-3a.md,
// docs/orchestration/tasks/render-direct-3.md). `GpuFrame::draw_direct_objects_test` (gpu.cpp)
// draws Class::None/shadow static instances through the existing sprite pipeline, sourced from the
// residency atlas, into a cleared target - this file is the "switch": nothing in the normal
// render() path calls it, so render() keeps giving the unmodified baked picture (0 digest change,
// by construction - no main-pass wiring exists yet). Compared here against the CPU replay
// (chunk_draws.hpp's replay(), the spec for the baked compute pass too) of the identical filtered
// record list. Scope: shadows only (3a); real objects (Class::Z) move in 3a2.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/render/chunk_draws.hpp>
#include <anno/render/chunk_instances.hpp>
#include <anno/render/depth.hpp>
#include <anno/render/direct_instances.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/gpu.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

#include "corpus_scene.hpp"

namespace render = anno::render;
namespace gpu = anno::render::gpu;
using render::Px;
using render::Rect;
using render::test::Game;
using render::test::kScenes;

namespace {

std::optional<gpu::Context> device_or_skip() {
    auto ctx = gpu::headless_context();
    if (ctx) {
        return std::move(ctx).value();
    }
    if (std::getenv("ANNO_REQUIRE_GPU") != nullptr) {
        FAIL("no GPU device (ANNO_REQUIRE_GPU is set): " << ctx.error());
    }
    MESSAGE("skipping: no GPU device: " << ctx.error());
    return std::nullopt;
}

constexpr std::size_t kGroup = 0;
constexpr std::uint32_t kTrack = 0; // object_flags::kTrackBase or 0; 3a tracks no base plane

/// Island `island_i`'s Class::None/shadow static instances at kGroup, bucketed by chunk (its
/// placement offset at kGroup applied) - 3a's own scope (render-direct-3.md).
render::ChunkInstanceBuckets shadow_buckets(const render::Scene& scene, std::size_t island_i) {
    const auto& list = scene.island_lists(island_i).other;
    std::pair<int, int> offset{0, 0};
    for (const auto& p : scene.layouts[kGroup]) {
        if (p.index == island_i) {
            offset = p.offset;
            break;
        }
    }
    const auto statics = render::resolve_static_instances(list, scene.anlagen, kGroup, scene.lift);
    const render::depth::Params dp = render::depth::params(kGroup, scene.lift);
    std::vector<render::StaticInstance> shadows;
    for (const auto& inst : statics) {
        const auto rec = scene.object_record(inst.draw, {0, 0}, kTrack, scene.depth_test, dp);
        if (rec && render::object_flags::depth_rule(rec->flags).cls == render::depth::Class::None) {
            shadows.push_back(inst);
        }
    }
    return render::bucket_by_chunk(shadows, offset);
}

/// The CPU replay oracle for exactly `instances` (painter order, no clip, no depth test - 3a's own
/// scope): the chunk-build compute pass's own spec (chunk_draws.cpp) for this filtered list.
render::Canvas cpu_oracle(const render::Scene& scene,
                          const std::vector<render::StaticInstance>& instances, Rect rect) {
    render::ChunkDraws d;
    d.rect = rect;
    d.group = kGroup;
    const render::depth::Params dp = render::depth::params(kGroup, scene.lift);
    std::vector<render::FrameId> frames;
    for (const auto& inst : instances) {
        const auto rec = scene.object_record(inst.draw, {0, 0}, kTrack, scene.depth_test, dp);
        REQUIRE(rec);
        render::ObjectRecord r = *rec;
        r.clip = render::kClipNone; // no island clip in this filtered-only oracle (3a has none)
        render::append_object_record(d.objects, r);
        frames.push_back(r.frame);
    }
    std::sort(frames.begin(), frames.end());
    frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
    d.frame_set = frames;
    return render::replay(d, scene.frame_table);
}

/// Per-channel max/mean difference between two same-size straight-alpha RGBA buffers, and the
/// count of pixels over `threshold` in any channel. MESSAGEs nothing itself (the caller reports
/// the numbers - ADR 0007 decision 8 wants them measured, not guessed).
struct DiffStats {
    std::array<int, 4> max_diff{};
    std::array<double, 4> mean_diff{};
    std::size_t over = 0;
};

DiffStats diff_stats(const std::vector<Px>& got, const render::Canvas& cpu, int threshold) {
    DiffStats s;
    REQUIRE(got.size() == cpu.pixels.size() / 4);
    std::array<std::uint64_t, 4> sum{};
    for (std::size_t i = 0; i < got.size(); ++i) {
        bool bad = false;
        for (std::size_t c = 0; c < 4; ++c) {
            const int a = got[i][c];
            const int b = cpu.pixels[i * 4 + c];
            const int d = std::abs(a - b);
            s.max_diff[c] = std::max(s.max_diff[c], d);
            sum[c] += static_cast<std::uint64_t>(d);
            bad = bad || d > threshold;
        }
        s.over += bad ? 1U : 0U;
    }
    for (std::size_t c = 0; c < 4; ++c) {
        s.mean_diff[c] =
            got.empty() ? 0.0 : static_cast<double>(sum[c]) / static_cast<double>(got.size());
    }
    return s;
}

/// Draws every island's Class::None/shadow instances, chunk by chunk, through the direct pass and
/// compares each against the CPU replay of the same filtered records. MESSAGEs the measured
/// tolerance (ADR 0007 decision 8: report the real numbers, don't guess them) and CHECKs it is
/// exactly 0 (bit-exact, as measured on the 4090 - see the test cases below).
void measure_tolerance(const render::Scene& scene, gpu::GpuFrame& g, const char* label,
                       const char* adapter_name) {
    std::size_t chunks_checked = 0, instances_checked = 0, total_over = 0;
    std::array<int, 4> worst_max{};
    std::array<double, 4> mean_sum{};
    for (std::size_t island_i = 0; island_i < scene.islands.size(); ++island_i) {
        for (const auto& [key, instances] : shadow_buckets(scene, island_i)) {
            if (instances.empty()) {
                continue;
            }
            const Rect rect = render::chunk_rect(key.first, key.second);
            render::ChunkInstanceBuckets one_bucket;
            one_bucket[key] = instances;
            const std::array<std::pair<int, int>, 1> visible = {key};
            const auto got = g.draw_direct_objects_test(
                scene, one_bucket, visible, kGroup, scene.depth_test, rect, WGPUColor{0, 0, 0, 0},
                std::numeric_limits<std::uint64_t>::max());
            REQUIRE(got);
            const render::Canvas cpu = cpu_oracle(scene, instances, rect);
            const DiffStats s = diff_stats(*got, cpu, 2);
            for (std::size_t c = 0; c < 4; ++c) {
                worst_max[c] = std::max(worst_max[c], s.max_diff[c]);
                mean_sum[c] += s.mean_diff[c];
            }
            total_over += s.over;
            ++chunks_checked;
            instances_checked += instances.size();
        }
    }
    REQUIRE(chunks_checked > 0);
    // doctest prints a runtime const char* as its address, not its text (only a literal gets
    // the string treatment) - std::string forces the text.
    MESSAGE(std::string(label) << ": " << chunks_checked << " chunks, " << instances_checked
                               << " shadow instances, max per-channel diff R=" << worst_max[0]
                               << " G=" << worst_max[1] << " B=" << worst_max[2]
                               << " A=" << worst_max[3] << ", mean per-channel diff R="
                               << mean_sum[0] / static_cast<double>(chunks_checked)
                               << " G=" << mean_sum[1] / static_cast<double>(chunks_checked)
                               << " B=" << mean_sum[2] / static_cast<double>(chunks_checked)
                               << " A=" << mean_sum[3] / static_cast<double>(chunks_checked) << ", "
                               << total_over << " pixels over threshold (2), adapter "
                               << std::string(adapter_name));
    CHECK(total_over == 0);
}

#ifdef _WIN32
void set_env(const char* key, const char* value) {
    _putenv_s(key, value);
}
#else
void set_env(const char* key, const char* value) {
    if (value[0] == '\0') {
        unsetenv(key);
    } else {
        setenv(key, value, 1);
    }
}
#endif

/// Opens the adapter named `adapter_name` (atlas_upload_test.cpp's own pattern: "RTX 4090",
/// "Radeon"); nullopt (MESSAGEd, not failed) when this host has none by that name.
std::optional<gpu::Context> named_context(const char* adapter_name) {
    const auto adapters = gpu::list_adapters();
    if (!gpu::pick_adapter(adapter_name, adapters)) {
        MESSAGE("skipping: no adapter named " << std::string(adapter_name) << " on this host");
        return std::nullopt;
    }
    const char* previous = std::getenv("ANNO_GPU_ADAPTER");
    const std::string saved = previous != nullptr ? previous : "";
    set_env("ANNO_GPU_ADAPTER", adapter_name);
    auto ctx = gpu::headless_context();
    set_env("ANNO_GPU_ADAPTER", saved.c_str());
    if (!ctx) {
        if (std::getenv("ANNO_REQUIRE_GPU") != nullptr) {
            FAIL("adapter " << std::string(adapter_name)
                            << " named but no device: " << ctx.error());
        }
        MESSAGE("skipping: adapter " << std::string(adapter_name)
                                     << " named but no device: " << ctx.error());
        return std::nullopt;
    }
    return std::move(ctx).value();
}

} // namespace

TEST_CASE("RENDER-DIRECT-3 3a proof: direct shadow draw matches the CPU replay, within a measured "
          "tolerance (Single_01.sze, game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const Game game(*dir);
    const auto loaded = game.scene(*dir / kScenes[0].path); // Single_01.sze
    auto frame = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
    REQUIRE(frame);
    measure_tolerance(*loaded.scene, *frame.value(), "Single_01.sze", ctx->name.c_str());
}

TEST_CASE("RENDER-DIRECT-3 3a proof: direct shadow draw matches the CPU replay, within a measured "
          "tolerance (lan5.txt, both named GPUs, game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const std::filesystem::path lan5 =
        std::filesystem::path(ANNO_REPO_DIR) / "data" / "maps" / "lan5.txt";
    std::error_code ec;
    if (!std::filesystem::exists(lan5, ec)) {
        FAIL("lan5.txt is not at the expected repo path: " << lan5.string());
    }
    auto play = anno::app::load_play(*dir, lan5, anno::app::FixedPlayOptions{});
    REQUIRE_MESSAGE(bool(play), "load_play: " << (play ? "" : play.error()));
    auto loaded = anno::app::load_placed_scene(*dir, play.value().name, play.value().placed);
    REQUIRE_MESSAGE(bool(loaded), "load_placed_scene: " << (loaded ? "" : loaded.error()));
    const render::Scene& scene = *loaded.value().scene;

    for (const char* adapter_name : {"RTX 4090", "Radeon"}) {
        auto ctx = named_context(adapter_name);
        if (!ctx) {
            continue;
        }
        auto frame = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
        REQUIRE(frame);
        measure_tolerance(scene, *frame.value(), "lan5.txt", adapter_name);
    }
}

TEST_CASE("RENDER-DIRECT-3 3a proof: a wrong instance (mutation control) exceeds the tolerance "
          "(Single_01.sze, game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const Game game(*dir);
    const auto loaded = game.scene(*dir / kScenes[0].path);
    const render::Scene& scene = *loaded.scene;
    auto frame = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
    REQUIRE(frame);
    auto& g = *frame.value();

    // The first chunk with 2+ shadow instances (so shifting one leaves the chunk non-trivial).
    for (std::size_t island_i = 0; island_i < scene.islands.size(); ++island_i) {
        for (const auto& [key, instances] : shadow_buckets(scene, island_i)) {
            if (instances.size() < 2) {
                continue;
            }
            const Rect rect = render::chunk_rect(key.first, key.second);
            const render::Canvas cpu = cpu_oracle(scene, instances, rect);

            // The mutation: one instance's draw shifted 5 px right - a wrong upload, not a wrong
            // comparison (the oracle is built from the original, unshifted instances).
            render::ChunkInstanceBuckets mutated;
            std::vector<render::StaticInstance> shifted = instances;
            shifted.front().draw.anchor.first += 5;
            mutated[key] = shifted;
            const std::array<std::pair<int, int>, 1> visible = {key};
            const auto got = g.draw_direct_objects_test(
                scene, mutated, visible, kGroup, scene.depth_test, rect, WGPUColor{0, 0, 0, 0},
                std::numeric_limits<std::uint64_t>::max());
            REQUIRE(got);
            const DiffStats s = diff_stats(*got, cpu, 2);
            MESSAGE("mutation control: " << s.over << " pixels over threshold (expect > 0)");
            CHECK(s.over > 0); // the shift must be caught, not absorbed by the tolerance
            return;
        }
    }
    // Not a missing prerequisite (Single_01.sze is an original game file, game/app, not in git -
    // measured by the tolerance test above to have 45,315 shadow instances across 1056 chunks) -
    // a scene with no chunk of 2+ would mean the mutation control silently never ran, so this
    // fails loudly.
    FAIL("no chunk with 2+ shadow instances found on Single_01.sze - the mutation control did not "
         "run");
}

TEST_CASE("RENDER-DIRECT-3 3a proof: the per-chunk scissor stops a spanning instance from "
          "blending twice (seam proof, Single_01.sze, game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const Game game(*dir);
    const auto loaded = game.scene(*dir / kScenes[0].path);
    const render::Scene& scene = *loaded.scene;
    auto frame = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
    REQUIRE(frame);
    auto& g = *frame.value();

    // A shadow instance whose reach() spans two chunks: bucket_by_chunk_into lists it under both
    // (chunk_instances.hpp) - the exact case the scissor exists for.
    for (std::size_t island_i = 0; island_i < scene.islands.size(); ++island_i) {
        const auto buckets = shadow_buckets(scene, island_i);
        std::map<anno::formats::insel::PlacedObject, std::vector<std::pair<int, int>>> seen;
        for (const auto& [key, instances] : buckets) {
            for (const auto& inst : instances) {
                seen[inst.key].push_back(key);
            }
        }
        for (const auto& [obj_key, chunks] : seen) {
            if (chunks.size() < 2) {
                continue;
            }
            const auto c0 = chunks[0], c1 = chunks[1];
            // Only the spanning instance, under both its chunk keys - a natural second shadow
            // nearby would muddy "darkest pixel" with its own, unrelated single-blend deficit.
            render::ChunkInstanceBuckets only;
            for (const auto& inst : buckets.at(c0)) {
                if (inst.key == obj_key) {
                    only[c0].push_back(inst);
                }
            }
            for (const auto& inst : buckets.at(c1)) {
                if (inst.key == obj_key) {
                    only[c1].push_back(inst);
                }
            }
            const Rect view = render::chunk_rect(c0.first, c0.second)
                                  .unite(render::chunk_rect(c1.first, c1.second));
            const std::array<std::pair<int, int>, 2> visible = {c0, c1};
            const WGPUColor white = {1, 1, 1, 1};
            const auto with_scissor =
                g.draw_direct_objects_test(scene, only, visible, kGroup, scene.depth_test, view,
                                           white, std::numeric_limits<std::uint64_t>::max(), true);
            const auto without_scissor =
                g.draw_direct_objects_test(scene, only, visible, kGroup, scene.depth_test, view,
                                           white, std::numeric_limits<std::uint64_t>::max(), false);
            REQUIRE(with_scissor);
            REQUIRE(without_scissor);
            // The shadow's own darkest pixel: with the scissor, white blended once (0.625 of
            // white); without it, blended twice under both chunks (0.625^2) - visibly darker.
            std::size_t darkest_with = 0, darkest_without = 0;
            for (std::size_t i = 0; i < with_scissor->size(); ++i) {
                darkest_with = std::max<std::size_t>(darkest_with, 255U - (*with_scissor)[i][0]);
                darkest_without =
                    std::max<std::size_t>(darkest_without, 255U - (*without_scissor)[i][0]);
            }
            MESSAGE("seam proof: darkest R-deficit with scissor " << darkest_with << ", without "
                                                                  << darkest_without
                                                                  << " (expect roughly 96 vs 159)");
            CHECK(darkest_with > 0);                     // not vacuous: the shadow did draw
            CHECK(darkest_without > darkest_with + 20U); // the double blend is visibly darker
            return;
        }
    }
    // Not a missing prerequisite (Single_01.sze is an original game file, game/app, not in git):
    // a scene with no chunk-spanning shadow would mean the seam proof silently never ran, so this
    // fails loudly.
    FAIL("no chunk-spanning shadow instance found on Single_01.sze - the seam proof did not run");
}
