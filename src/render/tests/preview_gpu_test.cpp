// The developer tool's preview on the GPU against the CPU path (W4.4 round ee, review of C, P2-2):
// lan5's developer start at group 1, one frame with the preview as GpuFrame::render's overlay
// against the CPU frame with blend_preview over it. Skips without game files or a GPU device
// (ANNO_REQUIRE_GPU makes a missing device a failure).
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/dev_feedback.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/gpu.hpp>
#include <anno/render/resample.hpp>
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
        changed += d_shown[i] > 1 ? 1U : 0U;
        off += d_over[i] > 1 ? 1U : 0U;
    }
    std::size_t blend_changed = 0;
    for (std::size_t i = 0; i < base.size(); ++i) {
        blend_changed += base[i] != blended[i] ? 1U : 0U;
    }
    CHECK(changed > 0);
    CHECK(blend_changed > 0);
    CHECK_MESSAGE(off == 0, off << " of " << d_over.size() << " pixels differ by more than 1");
    CHECK(frame.errors() == 0);
    MESSAGE("type " << type.v << " at rotation 1: " << preview->sprites.size() << " sprites; "
                    << changed << " pixels changed on the GPU, " << blend_changed
                    << " by the CPU blend; " << off << " pixels apart (> 1)");
}

TEST_CASE("dev preview in continuous zoom: the GPU resampled frame equals the CPU frame with the "
          "blend, resampled by cpu_resample, and a wrong overlay does not") {
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
    // The first buildable type with sprites at group 1 there, at rotation 1, and the same type at
    // rotation 1 two tiles further: the wrong overlay of the control.
    std::optional<app::Preview> preview, wrong;
    for (const sim::TypeId t : l->setup.types) {
        if (preview) {
            break;
        }
        const sim::BuildingType* bt = l->setup.registry->find(t);
        if (bt == nullptr || (bt->flags_word & sim::kRoadFlag) != 0 || bt->width + bt->height < 3) {
            continue;
        }
        auto p = app::make_preview(anno::test::tool_at(*l, t, *free, 1), *l->scene, 1, 0);
        auto w = app::make_preview(
            anno::test::tool_at(*l, t, sim::TileXY{free->x + 2, free->y + 1}, 1), *l->scene, 1, 0);
        if (p && !p->sprites.empty() && w && !w->sprites.empty()) {
            preview = std::move(p);
            wrong = std::move(w);
        }
    }
    REQUIRE(preview.has_value());
    REQUIRE(wrong.has_value());
    constexpr std::uint32_t kW = 640, kH = 360;
    const auto [cx, cy] = anno::test::tile_centre(*l, *free);

    gpu::GpuLimits limits;
    limits.uploads_per_frame = static_cast<std::size_t>(-1);
    limits.stand_ins = false;
    limits.chunk_fade_ms = 0;
    auto made = gpu::GpuFrame::create(ctx.value(), WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    gpu::GpuFrame& frame = *made.value();
    render::FrameRenderer cpu(4);
    render::FrameRenderer on_gpu(4, false);
    for (const double scale : {0.8, 1.3}) {
        // A fractional camera, as a zoom step leaves it, so the resample offset is not zero.
        const auto rv = gpu::resampled_view(cx + 0.37, cy - 0.21, kW, kH, scale,
                                            gpu::resample_filter_for(scale));
        CAPTURE(scale);
        const auto gw = static_cast<std::uint32_t>(rv.view.w),
                   gh = static_cast<std::uint32_t>(rv.view.h);
        auto target = frame.offscreen_target(kW, kH);
        const auto shoot = [&](std::span<const render::SpriteDraw> overlay) {
            for (int i = 0; i < 2; ++i) {
                frame.render(on_gpu, *l->scene, rv.view, 1, 0, render::WaterStyle::Original,
                             target.second.get(), &rv.resample, overlay);
            }
            return frame.read_back(target.first.get(), kW, kH);
        };
        // The CPU reference: the grid frame, the blend on the grid (the overlay is drawn into the
        // grid before the resample pass), then the resample on the CPU.
        std::vector<std::uint32_t> grid(std::size_t{gw} * gh), grid_plain,
            ref(std::size_t{kW} * kH), ref_plain(std::size_t{kW} * kH);
        cpu.render(*l->scene, rv.view, 1, 0, render::WaterStyle::Original, grid.data());
        grid_plain = grid;
        app::blend_preview(*l->scene, *preview, 1, 0, rv.view, grid.data());
        render::cpu_resample(grid.data(), rv.view.w, rv.view.h, rv.resample, kW, kH, ref.data());
        render::cpu_resample(grid_plain.data(), rv.view.w, rv.view.h, rv.resample, kW, kH,
                             ref_plain.data());
        std::size_t ref_changed = 0;
        for (std::size_t i = 0; i < ref.size(); ++i) {
            ref_changed += ref[i] != ref_plain[i] ? 1U : 0U;
        }
        CHECK(ref_changed > 0); // the reference itself shows the preview after resampling

        const auto with = shoot(preview->sprites);
        const auto without = shoot({});
        const auto bad = shoot(wrong->sprites);
        REQUIRE(with.size() == ref.size() * 4);
        const auto count = [&](const std::vector<std::uint32_t>& want,
                               const std::vector<std::uint8_t>& have) {
            const auto d = deltas(want, have);
            std::size_t over2 = 0;
            int worst = 0;
            for (const int v : d) {
                over2 += v > 2 ? 1U : 0U;
                worst = std::max(worst, v);
            }
            return std::pair{worst, over2};
        };
        const auto [worst, over2] = count(ref, with);
        const auto [plain_worst, plain_over2] = count(ref_plain, without);
        const auto [bad_worst, bad_over2] = count(ref, bad);
        std::size_t shown = 0;
        for (std::size_t i = 0; i < ref.size(); ++i) {
            shown += (with[4 * i] != without[4 * i] || with[4 * i + 1] != without[4 * i + 1] ||
                      with[4 * i + 2] != without[4 * i + 2])
                         ? 1U
                         : 0U;
        }
        MESSAGE("scale " << scale << ": " << ref_changed << " reference pixels changed, " << shown
                         << " on the GPU; GPU vs CPU reference max " << worst << ", " << over2
                         << " pixels > 2 (plain frame: max " << plain_worst << ", " << plain_over2
                         << "); wrong overlay: max " << bad_worst << ", " << bad_over2
                         << " pixels > 2");
        // The model of the plain frame is within the existing bound (GPU vs CPU grids differ by at
        // most 3, a thousandth of the pixels by more than 2); the overlay adds nothing to that.
        CHECK(plain_worst <= 3);
        CHECK(plain_over2 <= ref.size() / 1000);
        CHECK(worst <= 3);
        CHECK(over2 <= ref.size() / 1000);
        CHECK(shown > 0); // the preview is on the resampled GPU frame
        // The control: an overlay two tiles away is not the reference. The pixels that differ
        // exceed the bound by a margin (a stricter reading would fail the right overlay too).
        CHECK(bad_over2 > 4 * (ref.size() / 1000));
    }
    CHECK(frame.errors() == 0);
}
