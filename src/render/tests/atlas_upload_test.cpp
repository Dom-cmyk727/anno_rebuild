// RENDER-DIRECT-1c: the resident atlas pages (RENDER-DIRECT-1b) uploaded into a GPU texture
// array, proven exact by readback (step 2) and by sampling (step 3), plus the two pure functions
// (resident_atlas_groups, atlas_upload_refusal) unit-tested without any GPU (the packet's own
// acceptance item). The GPU cases are pinned to one named adapter at a time (play_gpu_test.cpp's
// own pattern: a Radeon run is a Radeon result, never the RTX one, and the RTX 4090 gate is this
// file's own "RTX 4090" cases run from the msvc build).
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/assets/island.hpp>
#include <anno/assets/texture.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/tex.hpp>
#include <anno/render/atlas_pages.hpp>
#include <anno/render/atlas_upload.hpp>
#include <anno/render/frame_table.hpp>
#include <anno/render/gpu.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>
#include <anno/render/wgpu.hpp>
#include <anno/test/atlas_pages_check.hpp>
#include <anno/test/game_dir.hpp>

namespace render = anno::render;
namespace gpu = anno::render::gpu;
using Rgba = anno::assets::Rgba;

namespace {

// ---- No-GPU: resident_atlas_groups, atlas_pages_bytes, atlas_upload_refusal -----------------

TEST_CASE("resident_atlas_groups: a discrete adapter holds every group") {
    CHECK(gpu::resident_atlas_groups(WGPUAdapterType_DiscreteGPU, 0, 3) ==
          std::vector<std::uint32_t>{0, 1, 2});
    CHECK(gpu::resident_atlas_groups(WGPUAdapterType_DiscreteGPU, 2, 3) ==
          std::vector<std::uint32_t>{0, 1, 2});
    CHECK(gpu::resident_atlas_groups(WGPUAdapterType_DiscreteGPU, 0, 1) ==
          std::vector<std::uint32_t>{0});
    CHECK(gpu::resident_atlas_groups(WGPUAdapterType_DiscreteGPU, 0, 0).empty());
}

TEST_CASE("resident_atlas_groups: any other adapter holds the adjacent pair the zoom is between") {
    for (const WGPUAdapterType t :
         {WGPUAdapterType_IntegratedGPU, WGPUAdapterType_CPU, WGPUAdapterType_Unknown}) {
        CHECK(gpu::resident_atlas_groups(t, 0, 3) == std::vector<std::uint32_t>{0, 1});
        CHECK(gpu::resident_atlas_groups(t, 1, 3) == std::vector<std::uint32_t>{1, 2});
        // group_count - 2 is the highest pair start: a zoom "at" the last group is still
        // between the last two, not past them.
        CHECK(gpu::resident_atlas_groups(t, 2, 3) == std::vector<std::uint32_t>{1, 2});
        CHECK(gpu::resident_atlas_groups(t, 99, 3) == std::vector<std::uint32_t>{1, 2});
        CHECK(gpu::resident_atlas_groups(t, 0, 1) == std::vector<std::uint32_t>{0});
        CHECK(gpu::resident_atlas_groups(t, 0, 0).empty());
        CHECK(gpu::resident_atlas_groups(t, 0, 2) == std::vector<std::uint32_t>{0, 1});
    }
}

TEST_CASE("atlas_pages_bytes: level 0 plus the one mip level, per layer") {
    render::AtlasPages pages;
    pages.page_size = 2048;
    pages.level0.resize(3);
    pages.level1.resize(3);
    // 2048^2 * 4 + 1024^2 * 4 = 16,777,216 + 4,194,304 = 20,971,520 per layer.
    CHECK(gpu::atlas_pages_bytes(pages) == 20971520ULL * 3);

    render::AtlasPages empty;
    empty.page_size = 2048;
    CHECK(gpu::atlas_pages_bytes(empty) == 0);
}

TEST_CASE("atlas_upload_refusal: the three refusal reasons, and none of them") {
    CHECK(gpu::atlas_upload_refusal(0, 2048, 8, 8192, 0, 1000).has_value());    // no layers
    CHECK(gpu::atlas_upload_refusal(9, 2048, 8, 8192, 100, 1000).has_value());  // too many layers
    CHECK(gpu::atlas_upload_refusal(2, 9000, 8, 8192, 100, 1000).has_value());  // page too large
    CHECK(gpu::atlas_upload_refusal(2, 2048, 8, 8192, 1001, 1000).has_value()); // over budget
    CHECK_FALSE(
        gpu::atlas_upload_refusal(2, 2048, 8, 8192, 1000, 1000).has_value());        // exactly fits
    CHECK_FALSE(gpu::atlas_upload_refusal(2, 2048, 8, 8192, 500, 1000).has_value()); // fits
}

// ---- GPU: readback (step 2) and sampling (step 3), pinned to one named adapter --------------

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

/// A synthetic straight-alpha frame, distinct per texel (atlas_pages_test.cpp's own pattern).
std::shared_ptr<const Rgba> make_frame(
    std::uint16_t w, std::uint16_t h,
    const std::function<std::array<std::uint8_t, 4>(std::uint32_t, std::uint32_t)>& pattern) {
    auto img = std::make_shared<Rgba>();
    img->width = w;
    img->height = h;
    img->pixels.resize(std::size_t{w} * h * 4);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const auto p = pattern(x, y);
            const std::size_t i = (std::size_t{y} * w + x) * 4;
            img->pixels[i + 0] = p[0];
            img->pixels[i + 1] = p[1];
            img->pixels[i + 2] = p[2];
            img->pixels[i + 3] = p[3];
        }
    }
    return img;
}

/// A page set small enough to build and upload at once: a 1x1, an odd-sized, an alpha-edge and a
/// larger frame, every byte position-recoverable.
render::AtlasPages synthetic_pages() {
    std::vector<render::AtlasFrameInput> frames;
    frames.push_back({0, make_frame(1, 1, [](std::uint32_t, std::uint32_t) {
                          return std::array<std::uint8_t, 4>{10, 20, 30, 128};
                      })});
    frames.push_back({1, make_frame(5, 3, [](std::uint32_t x, std::uint32_t y) {
                          return std::array<std::uint8_t, 4>{static_cast<std::uint8_t>(x * 10 + 1),
                                                             static_cast<std::uint8_t>(y * 10 + 2),
                                                             static_cast<std::uint8_t>(x + y),
                                                             static_cast<std::uint8_t>(40 + x * y)};
                      })});
    frames.push_back(
        {2, make_frame(4, 4, [](std::uint32_t x, std::uint32_t y) {
             const std::uint8_t a = (x == 0 || y == 0) ? 0 : (x == 3 || y == 3) ? 255 : 128;
             return std::array<std::uint8_t, 4>{static_cast<std::uint8_t>(50 + x),
                                                static_cast<std::uint8_t>(60 + y), 70, a};
         })});
    frames.push_back({3, make_frame(64, 48, [](std::uint32_t x, std::uint32_t y) {
                          return std::array<std::uint8_t, 4>{
                              static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y),
                              static_cast<std::uint8_t>(x ^ y), static_cast<std::uint8_t>(200)};
                      })});
    return render::build_atlas_pages(frames, 256, 2, 2);
}

std::vector<std::uint8_t> read_all(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE_MESSAGE(bool(in), "cannot open " << p.string());
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
}

/// A render::Scene from the original files directly (atlas_pages_corpus_test.cpp's own pattern,
/// duplicated here rather than shared: this file is a different binary, anno_render_gpu_tests).
render::Scene load_scene_direct(const std::filesystem::path& dir, const std::filesystem::path& file,
                                std::deque<std::vector<std::uint8_t>>& keep) {
    keep.push_back(read_all(dir / "Textures" / "Felder.tex"));
    const auto felder = anno::formats::tex::TexFile::parse(keep.back()).value();
    keep.push_back(read_all(dir / "Anlagen.dat"));
    auto anlagen = anno::formats::anlagen::AnlagenFile::parse(keep.back()).value();
    auto textures = anno::assets::island::GroundTextures::load(felder).value();
    if (const auto c = render::water_colour(anlagen)) {
        textures.set_water(*c);
    }
    keep.push_back(read_all(file));
    return render::Scene("", anno::formats::insel::islands(keep.back()).value(),
                         std::move(textures), std::move(anlagen), render::kHeightLift);
}

/// Checks every placement's level-0 and mip rectangle of `pages` against the readback of `tex`
/// (step 2) and, for `sample_too`, against a GPU-sampled draw of the same rectangle (step 3).
void check_pages_on_gpu(const gpu::Context& ctx, const render::AtlasPages& pages,
                        std::string_view what, bool sample_too) {
    auto uploaded = gpu::upload_atlas_pages(ctx, pages, gpu::atlas_pages_bytes(pages));
    REQUIRE_MESSAGE(bool(uploaded), what << ": " << (uploaded ? "" : uploaded.error()));
    const gpu::AtlasTexture& tex = uploaded.value();
    for (std::uint32_t layer = 0; layer < tex.layers; ++layer) {
        const auto l0 = gpu::read_back_level(ctx, tex, layer, 0);
        REQUIRE_MESSAGE(l0 == pages.level0[layer], what << ": layer " << layer << " level 0");
        const auto l1 = gpu::read_back_level(ctx, tex, layer, 1);
        REQUIRE_MESSAGE(l1 == pages.level1[layer], what << ": layer " << layer << " level 1");
    }
    if (!sample_too) {
        return;
    }
    for (const render::AtlasFramePlacement& place : pages.placements) {
        for (const std::uint32_t level : {0U, 1U}) {
            const auto sampled = gpu::sample_placement(ctx, tex, place, level);
            const auto& src_page =
                level == 0 ? pages.level0[place.layer] : pages.level1[place.layer];
            const std::uint32_t stride = level == 0 ? tex.page_size : tex.page_size / 2;
            const std::uint32_t ox = level == 0 ? place.x : place.mip_x;
            const std::uint32_t oy = level == 0 ? place.y : place.mip_y;
            const std::uint32_t w = level == 0 ? place.width : place.mip_width;
            const std::uint32_t h = level == 0 ? place.height : place.mip_height;
            REQUIRE_MESSAGE(sampled.size() == std::size_t{4} * w * h,
                            what << ": placement " << place.id << " level " << level);
            for (std::uint32_t y = 0; y < h; ++y) {
                for (std::uint32_t x = 0; x < w; ++x) {
                    const std::size_t si = (std::size_t{y} * w + x) * 4;
                    const std::size_t pi = (std::size_t{oy + y} * stride + (ox + x)) * 4;
                    const std::array<std::uint8_t, 4> got{sampled[si], sampled[si + 1],
                                                          sampled[si + 2], sampled[si + 3]};
                    const std::array<std::uint8_t, 4> want{src_page[pi], src_page[pi + 1],
                                                           src_page[pi + 2], src_page[pi + 3]};
                    CHECK(got == want);
                }
            }
        }
    }
    MESSAGE(what << ": " << pages.placements.size() << " placements sampled exact, both levels");
}

/// Opens the adapter named `adapter_name` (empty: skip, as play_gpu_test.cpp), runs `body` on it.
void on_named_adapter(const char* adapter_name,
                      const std::function<void(const gpu::Context&)>& body) {
    const auto adapters = gpu::list_adapters();
    if (!gpu::pick_adapter(adapter_name, adapters)) {
        std::string names;
        for (const auto& a : adapters) {
            if (!names.empty()) {
                names += "; ";
            }
            names += a.name;
        }
        if (adapters.empty() && std::getenv("ANNO_REQUIRE_GPU") != nullptr) {
            FAIL("no GPU device at all (ANNO_REQUIRE_GPU is set)");
        }
        MESSAGE("skipping: no GPU device (no adapter named " << std::string(adapter_name)
                                                             << ", among: " << names << ")");
        return;
    }
    const char* previous = std::getenv("ANNO_GPU_ADAPTER");
    const std::string saved = previous != nullptr ? previous : "";
    set_env("ANNO_GPU_ADAPTER", adapter_name);
    auto ctx = gpu::headless_context();
    set_env("ANNO_GPU_ADAPTER", saved.c_str());
    if (!ctx) {
        if (std::getenv("ANNO_REQUIRE_GPU") != nullptr) {
            FAIL("no GPU device (ANNO_REQUIRE_GPU is set): " << ctx.error());
        }
        MESSAGE("skipping: no GPU device: " << ctx.error());
        return;
    }
    MESSAGE("adapter " << ctx.value().name);
    CHECK(ctx.value().name.find(adapter_name) != std::string::npos);
    body(ctx.value());
    CHECK(ctx.value().errors->load() == 0);
}

void synthetic_and_lan5(const char* adapter_name, bool all_groups) {
    on_named_adapter(adapter_name, [&](const gpu::Context& ctx) {
        check_pages_on_gpu(ctx, synthetic_pages(), "synthetic", true);

        const auto dir = anno::test::game_dir();
        if (!dir) {
            MESSAGE("skipping lan5.txt: ANNO1503_GAME_DIR not set");
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

        const auto groups = gpu::resident_atlas_groups(ctx.adapter_type, 0,
                                                       static_cast<std::uint32_t>(render::kGroups));
        for (const std::uint32_t group :
             all_groups ? std::vector<std::uint32_t>{0, 1, 2} : groups) {
            render::SpriteCache cache;
            render::FrameTable table;
            const auto frames = anno::test::collect_static_water(scene, group, cache, table);
            const auto pages = render::build_atlas_pages(frames, 2048, 2, 4);
            check_pages_on_gpu(ctx, pages, "lan5.txt", true);
        }
    });
}

void worst_pair_scene(const char* adapter_name) {
    on_named_adapter(adapter_name, [&](const gpu::Context& ctx) {
        const auto dir = anno::test::game_dir();
        if (!dir) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        const auto file = *dir / "Scenes" / "0010SingleEndlos" / "02_1024_easy_128.sze";
        std::error_code ec;
        if (!std::filesystem::exists(file, ec)) {
            FAIL("missing named scene: " << file.string());
        }
        std::deque<std::vector<std::uint8_t>> keep;
        const render::Scene scene = load_scene_direct(*dir, file, keep);
        for (const std::uint32_t group : {0U, 1U}) {
            render::SpriteCache cache;
            render::FrameTable table;
            const auto frames = anno::test::collect_static_water(scene, group, cache, table);
            const auto pages = render::build_atlas_pages(frames, 2048, 2, 4);
            check_pages_on_gpu(ctx, pages, "02_1024_easy_128.sze (worst pair)", false);
        }
    });
}

/// RENDER-DIRECT-1c step 4: VRAM and upload/readback wall time for one page set, in Release, on a
/// quiet machine (the orchestrator's own slot). `upload wall time` is `upload_atlas_pages` plus a
/// queue fence (`wait_idle`), so it is the device's own completion, not just the CPU call
/// returning; `readback time` is every layer's every level read back and compared, as the
/// correctness cases above already do - timed here instead of only proven exact.
void measure_pages(const gpu::Context& ctx, const render::AtlasPages& pages, const char* scene) {
    const auto t0 = std::chrono::steady_clock::now();
    auto uploaded = gpu::upload_atlas_pages(ctx, pages, gpu::atlas_pages_bytes(pages));
    REQUIRE_MESSAGE(bool(uploaded), scene << ": " << (uploaded ? "" : uploaded.error()));
    gpu::wait_idle(ctx.instance.get(), ctx.device.get(), ctx.queue.get());
    const auto t1 = std::chrono::steady_clock::now();
    const gpu::AtlasTexture& tex = uploaded.value();
    for (std::uint32_t layer = 0; layer < tex.layers; ++layer) {
        for (const std::uint32_t level : {0U, 1U}) {
            const auto rb = gpu::read_back_level(ctx, tex, layer, level);
            REQUIRE(!rb.empty());
        }
    }
    const auto t2 = std::chrono::steady_clock::now();
    const double upload_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double readback_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    MESSAGE(scene << " on " << ctx.name << ": " << tex.layers << " layers, "
                  << gpu::atlas_pages_bytes(pages) << " bytes, upload " << upload_ms
                  << " ms (fenced), readback " << readback_ms << " ms");
}

void measure_adapter(const char* adapter_name) {
    on_named_adapter(adapter_name, [&](const gpu::Context& ctx) {
        const auto dir = anno::test::game_dir();
        if (!dir) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        const std::filesystem::path lan5 =
            std::filesystem::path(ANNO_REPO_DIR) / "data" / "maps" / "lan5.txt";
        std::error_code ec;
        if (std::filesystem::exists(lan5, ec)) {
            auto play = anno::app::load_play(*dir, lan5, anno::app::FixedPlayOptions{});
            REQUIRE_MESSAGE(bool(play), "load_play: " << (play ? "" : play.error()));
            auto loaded =
                anno::app::load_placed_scene(*dir, play.value().name, play.value().placed);
            REQUIRE_MESSAGE(bool(loaded), "load_placed_scene: " << (loaded ? "" : loaded.error()));
            const render::Scene& scene = *loaded.value().scene;
            const auto groups = gpu::resident_atlas_groups(
                ctx.adapter_type, 0, static_cast<std::uint32_t>(render::kGroups));
            for (const std::uint32_t group : groups) {
                render::SpriteCache cache;
                render::FrameTable table;
                const auto frames = anno::test::collect_static_water(scene, group, cache, table);
                const auto pages = render::build_atlas_pages(frames, 2048, 2, 4);
                measure_pages(ctx, pages, "lan5.txt");
            }
        }
        const auto file = *dir / "Scenes" / "0010SingleEndlos" / "02_1024_easy_128.sze";
        if (std::filesystem::exists(file, ec)) {
            std::deque<std::vector<std::uint8_t>> keep;
            const render::Scene scene = load_scene_direct(*dir, file, keep);
            for (const std::uint32_t group : {0U, 1U}) {
                render::SpriteCache cache;
                render::FrameTable table;
                const auto frames = anno::test::collect_static_water(scene, group, cache, table);
                const auto pages = render::build_atlas_pages(frames, 2048, 2, 4);
                measure_pages(ctx, pages, "02_1024_easy_128.sze (worst pair)");
            }
        }
    });
}

} // namespace

TEST_CASE("atlas upload: synthetic pages and lan5.txt are byte-exact and sample-exact on the "
          "RTX 4090 (all three groups)") {
    synthetic_and_lan5("RTX 4090", true);
}

TEST_CASE("atlas upload: synthetic pages and lan5.txt are byte-exact and sample-exact on the "
          "Radeon iGPU (the adjacent pair)") {
    synthetic_and_lan5("Radeon", false);
}

TEST_CASE("atlas upload: 02_1024_easy_128.sze's worst adjacent pair (340 MiB) is byte-exact on "
          "the RTX 4090") {
    worst_pair_scene("RTX 4090");
}

TEST_CASE("atlas upload: 02_1024_easy_128.sze's worst adjacent pair (340 MiB) is byte-exact on "
          "the Radeon iGPU") {
    worst_pair_scene("Radeon");
}

TEST_CASE("atlas upload: VRAM and upload/readback time for lan5.txt and 02_1024_easy_128.sze, "
          "RTX 4090 (step 4, a quiet machine)") {
    measure_adapter("RTX 4090");
}

TEST_CASE("atlas upload: VRAM and upload/readback time for lan5.txt and 02_1024_easy_128.sze, "
          "Radeon iGPU (step 4, a quiet machine)") {
    measure_adapter("Radeon");
}
