// One play scene, GPU against the CPU path, on the RTX 4090 when it is present.
// The windowed play-versus-view check lives in the app tests; this one names
// the adapter the compositor actually opened.
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/play.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/gpu.hpp>
#include <anno/render/wgpu.hpp>
#include <anno/test/game_dir.hpp>

namespace {

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

/// Largest per-channel difference between a CPU frame (0RGB words) and a GPU
/// read-back (RGBA bytes). Same bound as the frame-digest GPU cases: 1.
int channel_delta(const std::vector<std::uint32_t>& cpu, const std::vector<std::uint8_t>& gpu_px) {
    int max = 0;
    for (std::size_t i = 0; i < cpu.size(); ++i) {
        for (std::size_t k = 0; k < 3; ++k) {
            const int c = static_cast<int>((cpu[i] >> (16 - 8 * k)) & 0xFF);
            max = std::max(max, std::abs(c - int{gpu_px[4 * i + k]}));
        }
    }
    return max;
}

} // namespace

TEST_CASE("play: GPU matches the CPU path on one scene") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto adapters = anno::render::gpu::list_adapters();
    if (!anno::render::gpu::pick_adapter("RTX 4090", adapters)) {
        std::string names;
        for (const auto& adapter : adapters) {
            if (!names.empty()) {
                names += "; ";
            }
            names += adapter.name;
        }
        if (std::getenv("ANNO_REQUIRE_GPU") != nullptr) {
            FAIL("no RTX 4090 (ANNO_REQUIRE_GPU is set): " << names);
        }
        MESSAGE("skipping: no RTX 4090 among: " << names);
        return;
    }
    const char* previous = std::getenv("ANNO_GPU_ADAPTER");
    const std::string saved = previous != nullptr ? previous : "";
    set_env("ANNO_GPU_ADAPTER", "RTX 4090");
    auto ctx = anno::render::gpu::headless_context();
    set_env("ANNO_GPU_ADAPTER", saved.c_str());
    if (!ctx) {
        if (std::getenv("ANNO_REQUIRE_GPU") != nullptr) {
            FAIL("no GPU device (ANNO_REQUIRE_GPU is set): " << ctx.error());
        }
        MESSAGE("skipping: " << ctx.error());
        return;
    }
    MESSAGE("adapter " << ctx.value().name);
    CHECK(ctx.value().name.find("RTX 4090") != std::string::npos);

    const auto file = *dir / "Scenes" / "0200Single" / "Single_01.sze";
    auto play = anno::app::load_play(*dir, file);
    REQUIRE(play);
    const auto buildings = anno::app::play_buildings(play.value().world);
    anno::app::SceneObjects objects;
    objects.is_building = [&](std::uint16_t id) {
        return anno::app::anlage_is_building(play.value().world, id);
    };
    objects.buildings = buildings;
    auto scene = anno::app::load_scene(*dir, file, play.value().links, &objects);
    REQUIRE(scene);
    const auto& drawn = *scene.value().scene;
    const auto& layout = drawn.layouts[2];
    REQUIRE_FALSE(layout.empty());
    const anno::render::Placement* largest = &layout.front();
    for (const anno::render::Placement& place : layout) {
        if (place.bounds.w * place.bounds.h > largest->bounds.w * largest->bounds.h) {
            largest = &place;
        }
    }
    constexpr int kW = 640;
    constexpr int kH = 360;
    const anno::render::Rect view{largest->bounds.x + largest->bounds.w / 2 - kW / 2,
                                  largest->bounds.y + largest->bounds.h / 2 - kH / 2, kW, kH};

    anno::render::gpu::GpuLimits limits;
    limits.uploads_per_frame = static_cast<std::size_t>(-1);
    auto made =
        anno::render::gpu::GpuFrame::create(ctx.value(), WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    auto target = made.value()->offscreen_target(kW, kH);
    anno::render::FrameRenderer cpu(4);
    anno::render::FrameRenderer on_gpu(4, false);
    std::vector<std::uint32_t> cpu_px(static_cast<std::size_t>(kW) * kH);
    cpu.render(drawn, view, 2, 0, anno::render::WaterStyle::Original, cpu_px.data());
    const auto stats = made.value()->render(
        on_gpu, drawn, view, 2, 0, anno::render::WaterStyle::Original, target.second.get());
    CHECK(stats.chunks_missing == 0);
    const auto gpu_px = made.value()->read_back(target.first.get(), kW, kH);
    REQUIRE(gpu_px.size() == cpu_px.size() * 4);
    const int delta = channel_delta(cpu_px, gpu_px);
    CHECK(delta <= 1);
    CHECK(made.value()->errors() == 0);
    MESSAGE("Single_01 group 2 " << kW << "x" << kH << " max channel delta " << delta << " on "
                                 << ctx.value().name);
}
