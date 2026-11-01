// The developer tool's preview on the GPU against the CPU path (W4.4 round ee, review of C, P2-2):
// lan5's developer start at group 1, one frame with the preview as GpuFrame::render's overlay
// against the CPU frame with blend_preview over it. Skips without game files or a GPU device
// (ANNO_REQUIRE_GPU makes a missing device a failure).
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/dev_feedback.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/gpu.hpp>
#include <anno/render/wgpu.hpp>
#include <anno/test/dev_lan5.hpp>

namespace app = anno::app;
namespace gpu = anno::render::gpu;
namespace render = anno::render;
namespace sim = anno::sim;

namespace {

/// Per pixel, the largest channel difference between a CPU frame (0RGB words) and a GPU read-back
/// (RGBA bytes).
std::vector<int> deltas(const std::vector<std::uint32_t>& cpu, const std::vector<std::uint8_t>& g) {
    std::vector<int> out(cpu.size(), 0);
    for (std::size_t i = 0; i < cpu.size(); ++i) {
        for (std::size_t k = 0; k < 3; ++k) {
            const int c = static_cast<int>((cpu[i] >> (16 - 8 * k)) & 0xFF);
            out[i] = std::max(out[i], std::abs(c - int{g[4 * i + k]}));
        }
    }
    return out;
}

} // namespace

TEST_CASE("dev preview: the GPU overlay draws the preview as the CPU blend does, on top of the "
          "frame") {
    auto l = anno::test::lan5(false);
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto ctx = gpu::headless_context();
    if (!ctx) {
        if (std::getenv("ANNO_REQUIRE_GPU") != nullptr) {
            FAIL("no GPU device (ANNO_REQUIRE_GPU is set): " << ctx.error());
        }
        MESSAGE("skipping: no GPU device: " << ctx.error());
        return;
    }
    MESSAGE("adapter " << ctx.value().name);
    const auto free = anno::test::free_tile(*l);
    REQUIRE(free.has_value());
    // The first buildable type with sprites at group 1 there, at two rotations.
    std::optional<app::Preview> preview;
    sim::TypeId type;
    for (const sim::TypeId t : l->setup.types) {
        if (preview) {
            break;
        }
        const sim::BuildingType* bt = l->setup.registry->find(t);
        if (bt == nullptr || (bt->flags_word & sim::kRoadFlag) != 0 || bt->width + bt->height < 3) {
            continue; // a building of more than one tile: a preview of some extent
        }
        auto p = app::make_preview(anno::test::tool_at(*l, t, *free, 1), *l->scene, 1, 0);
        if (p && !p->sprites.empty()) {
            preview = std::move(p);
            type = t;
        }
    }
    REQUIRE(preview.has_value());
    constexpr int kW = 640, kH = 360;
    const auto [cx, cy] = anno::test::tile_centre(*l, *free);
    const render::Rect view{cx - kW / 2, cy - kH / 2, kW, kH};

    gpu::GpuLimits limits;
    limits.uploads_per_frame = static_cast<std::size_t>(-1);
    limits.stand_ins = false;
    limits.chunk_fade_ms = 0;
    auto made = gpu::GpuFrame::create(ctx.value(), WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    gpu::GpuFrame& frame = *made.value();
    auto target = frame.offscreen_target(kW, kH);
    render::FrameRenderer cpu(4);
    render::FrameRenderer on_gpu(4, false);
    std::vector<std::uint32_t> base(static_cast<std::size_t>(kW) * kH);
    cpu.render(*l->scene, view, 1, 0, render::WaterStyle::Original, base.data());
    std::vector<std::uint32_t> blended = base;
    app::blend_preview(*l->scene, *preview, 1, 0, view, blended.data());

    // Without the overlay: the GPU frame is the CPU frame (the comparison's own control).
    const auto plain = frame.render(on_gpu, *l->scene, view, 1, 0, render::WaterStyle::Original,
                                    target.second.get());
    CHECK(plain.chunks_missing == 0);
    const auto plain_px = frame.read_back(target.first.get(), kW, kH);
    REQUIRE(plain_px.size() == base.size() * 4);
    const auto d_plain = deltas(base, plain_px);
    CHECK(*std::max_element(d_plain.begin(), d_plain.end()) <= 1);
    // With it: the GPU frame is the CPU frame with the blend, and the preview changed pixels.
    frame.render(on_gpu, *l->scene, view, 1, 0, render::WaterStyle::Original, target.second.get(),
                 nullptr, preview->sprites);
    const auto over_px = frame.read_back(target.first.get(), kW, kH);
    const auto d_over = deltas(blended, over_px);
    const auto d_shown = deltas(base, over_px);
    std::size_t changed = 0, off = 0;
    for (std::size_t i = 0; i < d_over.size(); ++i) {
        changed += d_shown[i] > 1 ? 1 : 0;
        off += d_over[i] > 1 ? 1 : 0;
    }
    std::size_t blend_changed = 0;
    for (std::size_t i = 0; i < base.size(); ++i) {
        blend_changed += base[i] != blended[i] ? 1 : 0;
    }
    CHECK(changed > 0);
    CHECK(blend_changed > 0);
    CHECK_MESSAGE(off == 0, off << " of " << d_over.size() << " pixels differ by more than 1");
    CHECK(frame.errors() == 0);
    MESSAGE("type " << type.v << " at rotation 1: " << preview->sprites.size() << " sprites; "
                    << changed << " pixels changed on the GPU, " << blend_changed
                    << " by the CPU blend; " << off << " pixels apart (> 1)");
}
