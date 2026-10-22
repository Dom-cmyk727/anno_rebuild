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

// ---- The synthetic scene (no game files, gpu-chunks.md 7.3) -----------------

namespace {

using anno::assets::Rgba;
using anno::formats::figuren::Frame;
namespace flags = render::object_flags;

/// A w x h image of one straight-alpha colour.
Rgba solid(std::uint16_t w, std::uint16_t h, Px c) {
    Rgba img{w, h, {}};
    img.pixels.resize(std::size_t{w} * h * 4);
    for (std::size_t i = 0; i < std::size_t{w} * h; ++i) {
        img.pixels[i * 4 + 0] = c[0];
        img.pixels[i * 4 + 1] = c[1];
        img.pixels[i * 4 + 2] = c[2];
        img.pixels[i * 4 + 3] = c[3];
    }
    return img;
}

/// A column-checkerboard (the decoded WZ pattern): odd columns alpha 0, even
/// columns the opaque colour, with the row toggling the phase.
Rgba checker(std::uint16_t w, std::uint16_t h, Px c) {
    Rgba img = solid(w, h, c);
    for (std::uint16_t y = 0; y < h; ++y) {
        for (std::uint16_t x = 0; x < w; ++x) {
            if (((x + y) & 1) != 0) {
                img.pixels[(std::size_t{y} * w + x) * 4 + 3] = 0;
            }
        }
    }
    return img;
}

Frame header(std::int16_t ax, std::int16_t ay, std::uint16_t w, std::uint16_t h) {
    Frame f;
    f.width = w;
    f.height = h;
    f.anchor_x = ax;
    f.anchor_y = ay;
    return f;
}

/// Interns a hand-made frame and returns its id (keyed by a unique integer).
render::FrameId add(render::FrameTable& ft, std::uint32_t id, Rgba img, Frame hdr) {
    auto image = std::make_shared<Rgba>(std::move(img));
    return *ft.intern(render::FrameKey::unit({true, id, 0, render::UnitFrameId::kNoColour}, 0, false),
                      [&] { return std::pair{hdr, image}; });
}

/// Builds the planes on the GPU and checks they equal replay() and the binned
/// CPU model (chunk_draws_model.hpp) byte for byte. Returns the planes.
gpu::ChunkPlanes check_equal(gpu::GpuFrame& g, const render::ChunkDraws& d,
                             const render::FrameTable& ft) {
    const render::Canvas cpu = render::replay(d, ft);
    std::vector<Px> bpx;
    std::vector<std::uint8_t> bbase;
    render::test::replay_binned(d, ft, bpx, bbase);
    // The two CPU references agree (the model is the pass's spec).
    REQUIRE(bpx.size() == cpu.pixels.size() / 4);
    for (std::size_t i = 0; i < bpx.size(); ++i) {
        REQUIRE(bpx[i] == canvas_px(cpu, i));
        if (!cpu.base.empty()) {
            REQUIRE(bbase[i] == cpu.base[i]);
        }
    }
    auto got = g.build_chunk_planes(d, ft);
    REQUIRE(got);
    CHECK(diff(*got, cpu) == 0);
    return std::move(*got);
}

} // namespace

TEST_CASE("GPU chunk build: synthetic frames the original corpus never sends (no game files)") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    MESSAGE("adapter: " << ctx->name);
    auto frame = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
    REQUIRE(frame);
    auto& g = *frame.value();

    // Width a multiple of 256: the R8Uint base-plane copy's bytesPerRow (= width)
    // must be, and the base packs four pixels per u32 (width % 4). kChunk (512)
    // satisfies both; the live path only ever builds 512 x 512 planes.
    const Rect rect{0, 0, 256, 256};
    const Px white{255, 255, 255, 255};
    const Px opaque_red{200, 40, 40, 255};

    SUBCASE("shadows over translucent sprites and translucent over shadow, on clear and opaque") {
        render::FrameTable ft;
        const auto opaque = add(ft, 1, solid(8, 8, white), header(0, 8, 8, 8));
        const auto a64 = add(ft, 2, solid(8, 8, Px{255, 0, 0, 64}), header(0, 8, 8, 8));
        const auto a128 = add(ft, 3, solid(8, 8, Px{0, 255, 0, 128}), header(0, 8, 8, 8));
        const auto a191 = add(ft, 4, solid(8, 8, Px{0, 0, 255, 191}), header(0, 8, 8, 8));
        render::ChunkDrawsBuilder b(rect, 0, true, 0, 16);
        b.begin_unclipped();
        // (a) shadow over translucent, over clear: at (0,0).
        b.add({0, 0, a128, flags::kTrackBase, 0, 0, 0});
        b.add({0, 0, opaque, flags::kShadow, 0, 0, 0});
        // (b) translucent over shadow, over clear: at (16,0).
        b.add({16, 0, opaque, flags::kShadow, 0, 0, 0});
        b.add({16, 0, a191, flags::kTrackBase, 0, 0, 0});
        // (c) shadow over translucent, over opaque: at (0,16).
        b.add({0, 16, opaque, flags::kTrackBase, 0, 0, 0});
        b.add({0, 16, a64, flags::kTrackBase, 0, 0, 0});
        b.add({0, 16, opaque, flags::kShadow, 0, 0, 0});
        const render::ChunkDraws d = b.finish(ft);
        check_equal(g, d, ft);
    }

    SUBCASE("three compounding shadows on one pixel") {
        render::FrameTable ft;
        const auto opaque = add(ft, 1, solid(8, 8, white), header(0, 8, 8, 8));
        render::ChunkDrawsBuilder b(rect, 0, true, 0, 16);
        b.begin_unclipped();
        b.add({4, 4, opaque, flags::kTrackBase, 0, 0, 0});
        for (int i = 0; i < 3; ++i) {
            b.add({4, 4, opaque, flags::kShadow, 0, 0, 0});
        }
        const render::ChunkDraws d = b.finish(ft);
        const auto got = check_equal(g, d, ft);
        // Pinned by hand (iso.cpp over(), floor): opaque white, then the shadow
        // (0,0,0,96) three times: 158, then 98, then 61 per channel, alpha 255.
        CHECK(got.objects[static_cast<std::size_t>(5) * 256 + 5] == Px{61, 61, 61, 255});
    }

    SUBCASE("a column checkerboard frame (the WZ pattern)") {
        render::FrameTable ft;
        const auto chk = add(ft, 1, checker(16, 16, opaque_red), header(0, 16, 16, 16));
        render::ChunkDrawsBuilder b(rect, 0, true, 0, 16);
        b.begin_unclipped();
        b.add({10, 10, chk, flags::kTrackBase, 0, 0, 0});
        const render::ChunkDraws d = b.finish(ft);
        check_equal(g, d, ft);
    }

    SUBCASE("negative and large anchor_y drive the base contact branch") {
        render::FrameTable ft;
        // anchor_y negative: keel + base_band >= anchor_y is always true (base
        // tracks). anchor_y large: never true (base stays 255).
        const auto low = add(ft, 1, solid(8, 8, white), header(0, -5, 8, 8));
        const auto high = add(ft, 2, solid(8, 8, white), header(0, 1000, 8, 8));
        render::ChunkDrawsBuilder b(rect, 0, true, 3, 16);
        b.begin_unclipped();
        b.add({0, 0, low, flags::kTrackBase, 0, 0, 0});
        b.add({20, 0, high, flags::kTrackBase, 0, 0, 0});
        const render::ChunkDraws d = b.finish(ft);
        check_equal(g, d, ft);
    }

    SUBCASE("an image wider than its frame box (OQ-025)") {
        render::FrameTable ft;
        // header width 4, image 12 wide: the frame table uses the image size.
        const auto wide = add(ft, 1, solid(12, 6, opaque_red), header(0, 6, 4, 6));
        render::ChunkDrawsBuilder b(rect, 0, true, 0, 16);
        b.begin_unclipped();
        b.add({5, 5, wide, flags::kTrackBase, 0, 0, 0});
        const render::ChunkDraws d = b.finish(ft);
        check_equal(g, d, ft);
    }

    SUBCASE("boxes crossing the chunk edge and an island clip edge") {
        render::FrameTable ft;
        const auto blk = add(ft, 1, solid(20, 20, opaque_red), header(0, 20, 20, 20));
        render::ChunkDrawsBuilder b(rect, 0, true, 0, 16);
        // Clip rect cuts the sprite; part of the box is also off the chunk.
        b.begin_island(Rect{8, 8, 40, 40});
        b.add({-6, -6, blk, flags::kTrackBase, 0, 0, 0}); // crosses chunk top-left and clip
        b.add({50, 50, blk, flags::kTrackBase, 0, 0, 0}); // crosses chunk bottom-right and clip
        const render::ChunkDraws d = b.finish(ft);
        check_equal(g, d, ft);
    }

    SUBCASE("two islands overlap; the second is a shadow only, resetting base to 255") {
        render::FrameTable ft;
        const auto obj = add(ft, 1, solid(16, 16, white), header(0, 16, 16, 16));
        render::ChunkDrawsBuilder b(rect, 0, true, 0, 16);
        b.begin_island(Rect{0, 0, 40, 40});
        b.add({8, 8, obj, flags::kTrackBase, 0, 0, 0}); // island 1 sets base
        b.begin_island(Rect{0, 0, 40, 40});
        b.add({8, 8, obj, flags::kShadow, 0, 0, 0}); // island 2 shadow only: base -> 255
        const render::ChunkDraws d = b.finish(ft);
        const auto got = check_equal(g, d, ft);
        // Where island 2's shadow lands over island 1's object, the base is 255
        // (the shadow-only composite reset, scene.cpp composite()).
        CHECK(got.base[static_cast<std::size_t>(10) * 256 + 10] == 255);
    }

    SUBCASE("colour and depth fields are ignored (reserved)") {
        render::FrameTable ft;
        const auto obj = add(ft, 1, solid(8, 8, white), header(0, 8, 8, 8));
        render::ChunkDrawsBuilder plain(rect, 0, true, 0, 16);
        plain.begin_unclipped();
        plain.add({4, 4, obj, flags::kTrackBase, 0, 0, 0});
        render::ChunkDrawsBuilder fancy(rect, 0, true, 0, 16);
        fancy.begin_unclipped();
        fancy.add({4, 4, obj, flags::kTrackBase, 0, /*colour=*/0x7788AA, /*depth=*/123});
        const auto a = check_equal(g, plain.finish(ft), ft);
        const auto b2 = check_equal(g, fancy.finish(ft), ft);
        CHECK(a.objects == b2.objects);
        CHECK(a.base == b2.base);
    }
}
