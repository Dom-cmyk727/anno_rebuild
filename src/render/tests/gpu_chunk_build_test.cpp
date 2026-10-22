// GPU-built chunks (T11 stage 2a): the binned compute pass of
// docs/render/gpu-chunks.md 5.1 reproduces the CPU blitter exactly.
//
// The corpus case (game files, ANNO1503_GAME_DIR) builds the objects and base
// planes of every chunk of Single_01.sze, Single_24.sze and the canyon island
// D_256x256_0045_5B.scp at every zoom group on the GPU and checks they equal the
// CPU replay (chunk_draws.hpp) byte for byte. The synthetic cases (no game
// files) exercise what the original corpus never sends through the sprite path
// (gpu-chunks.md 7.3): shadows over translucent sprites, compounding shadows,
// the WZ checkerboard, negative anchors, images wider than the frame box, clip
// edges, two overlapping islands with a shadow-only second island (the base
// reset), and a soldier interleaved between objects. Every mutation of the pass
// in gpu-chunks.md 7.2 must fail a case; see gpu_chunk_build_mutation.
#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/figuren.hpp>
#include <anno/render/chunk_draws.hpp>
#include <anno/render/frame_table.hpp>
#include <anno/render/gpu.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

#include "chunk_draws_model.hpp"
#include "corpus_scene.hpp"

namespace render = anno::render;
namespace gpu = anno::render::gpu;
using render::Px;
using render::Rect;

namespace {

std::optional<gpu::Context> device_or_skip() {
    auto ctx = gpu::headless_context();
    if (ctx) {
        return std::move(ctx).value();
    }
    if (std::getenv("ANNO_REQUIRE_GPU") != nullptr) {
        FAIL("no GPU device (ANNO_REQUIRE_GPU is set): " << ctx.error());
    }
    MESSAGE("skipping: " << ctx.error());
    return std::nullopt;
}

/// The CPU canvas pixel at index i as a Px.
Px canvas_px(const render::Canvas& c, std::size_t i) {
    return Px{c.pixels[i * 4], c.pixels[i * 4 + 1], c.pixels[i * 4 + 2], c.pixels[i * 4 + 3]};
}

/// Compares GPU-built planes to the CPU replay of the same list; returns the
/// number of pixels that differ in objects or base.
std::size_t diff(const gpu::ChunkPlanes& got, const render::Canvas& cpu) {
    std::size_t bad = 0;
    for (std::size_t i = 0; i < got.objects.size(); ++i) {
        if (got.objects[i] != canvas_px(cpu, i)) {
            ++bad;
        }
    }
    if (!cpu.base.empty()) {
        for (std::size_t i = 0; i < cpu.base.size(); ++i) {
            if (i >= got.base.size() || got.base[i] != cpu.base[i]) {
                ++bad;
            }
        }
    }
    return bad;
}

} // namespace

TEST_CASE("GPU-built chunk objects and base planes equal the CPU replay (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    MESSAGE("adapter: " << ctx->name);
    const render::test::Game game(*dir);
    for (const auto& sf : render::test::kScenes) {
        const auto loaded = game.scene(*dir / sf.path);
        auto frame = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
        REQUIRE(frame);
        auto& g = *frame.value();
        std::size_t chunks = 0, records = 0, mismatches = 0;
        for (std::size_t group = 0; group < render::kGroups; ++group) {
            for (const auto& [cx, cy] : render::test::busy_chunks(*loaded, group)) {
                const Rect rect = render::chunk_rect(cx, cy);
                // The GPU compositor's chunks: ships drawn per frame, base plane on.
                const render::ChunkDraws d = loaded->objects_draws(rect, group, false, true);
                if (d.object_count() == 0) {
                    continue;
                }
                const render::Canvas cpu = render::replay(d, loaded->frame_table);
                const auto got = g.build_chunk_planes(d, loaded->frame_table);
                REQUIRE(got);
                const std::size_t bad = diff(*got, cpu);
                if (bad != 0 && mismatches == 0) {
                    INFO("first mismatch at " << sf.name << " group " << group << " chunk (" << cx
                                              << ", " << cy << "): " << bad << " px");
                    CHECK(bad == 0);
                }
                mismatches += bad;
                ++chunks;
                records += d.object_count();
            }
        }
        MESSAGE(std::string(sf.name) << ": " << chunks << " chunks, " << records << " records, "
                                     << loaded->frame_table.size() << " frames, " << mismatches
                                     << " mismatched pixels");
        CHECK(mismatches == 0);
    }
}
