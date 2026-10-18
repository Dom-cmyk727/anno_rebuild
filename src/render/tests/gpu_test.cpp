// GPU device, shaders and compositor (wgpu-native). Skips when the machine
// has no GPU adapter (e.g. CI runners without one) and, for the frame
// comparison, without the game files (ANNO1503_GAME_DIR).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/figure.hpp>
#include <anno/assets/island.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/formats/tex.hpp>
#include <anno/render/fluid.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/gpu.hpp>
#include <anno/render/gpu_fluid.hpp>
#include <anno/render/scene.hpp>
#include <anno/render/shaders.hpp>
#include <anno/render/wgpu.hpp>
#include <anno/test/game_dir.hpp>

namespace gpu = anno::render::gpu;
namespace render = anno::render;
namespace fig = anno::formats::figuren;

TEST_CASE("the shared WGSL shaders compile on the device") {
    auto c = gpu::headless_context();
    if (!c) {
        MESSAGE("skipping: " << c.error());
        return;
    }
    MESSAGE("adapter: " << c.value().name);
    CHECK(anno::render::shaders::compositor().size() > 1000);
    auto a = gpu::create_shader(c.value(), anno::render::shaders::compositor(), "gpu.wgsl");
    CHECK_MESSAGE(a, (a ? "" : a.error()));
    auto b = gpu::create_shader(c.value(), anno::render::shaders::fluid(), "fluid.wgsl");
    CHECK_MESSAGE(b, (b ? "" : b.error()));
    // A broken shader reports the compiler's error instead of failing later.
    auto bad = gpu::create_shader(c.value(), "fn main( {", "broken.wgsl");
    REQUIRE_FALSE(bad);
    CHECK(bad.error().find("broken.wgsl") == 0);
}

namespace {

/// Largest per-channel difference and the number of pixels differing by more
/// than 2 between a CPU frame (0RGB words) and a GPU read-back (RGBA bytes).
std::pair<int, std::size_t> difference(const std::vector<std::uint32_t>& cpu,
                                       const std::vector<std::uint8_t>& gpu_px) {
    int max = 0;
    std::size_t differ = 0;
    for (std::size_t i = 0; i < cpu.size(); ++i) {
        int m = 0;
        for (std::size_t k = 0; k < 3; ++k) {
            const int c = static_cast<int>((cpu[i] >> (16 - 8 * k)) & 0xFF);
            m = std::max(m, std::abs(c - int{gpu_px[4 * i + k]}));
        }
        max = std::max(max, m);
        differ += m > 2 ? 1U : 0U;
    }
    return {max, differ};
}

/// A scene and the file bytes it points into (Scene and AnlagenFile keep
/// views of them; the Rust Scene<'a> borrows them).
struct Loaded {
    std::vector<std::uint8_t> anl_buf, buf;
    std::unique_ptr<render::Scene> scene;
};

/// The Scene of scenario `name` with its ships and soldiers (as
/// render_corpus_test's scene renders).
Loaded load_scene(const std::filesystem::path& dir, const char* name) {
    Loaded out;
    const auto felder_buf = anno::test::read_file(dir / "Textures" / "Felder.tex");
    out.anl_buf = anno::test::read_file(dir / "Anlagen.dat");
    const auto& anl_buf = out.anl_buf;
    const auto hbuf = anno::test::read_file(dir / "FigurenHeader.dat");
    const auto header = fig::FigurenHeader::parse(hbuf).value();
    std::vector<std::vector<std::uint8_t>> vols(3);
    for (std::uint32_t g = 0; g < 3; ++g) {
        for (std::uint32_t part = 0;; ++part) {
            const auto p = dir / fig::volume_file_name(g, part);
            if (!std::filesystem::exists(p)) {
                break;
            }
            const auto d = anno::test::read_file(p);
            vols[g].insert(vols[g].end(), d.begin(), d.end());
        }
    }
    out.buf = anno::test::read_file(dir / "Scenes" / "0200Single" / name);
    const auto& buf = out.buf;
    auto anlagen = anno::formats::anlagen::AnlagenFile::parse(anl_buf).value();
    const auto felder = anno::formats::tex::TexFile::parse(felder_buf).value();
    auto textures = anno::assets::island::GroundTextures::load(felder).value();
    if (const auto c = render::water_colour(anlagen)) {
        textures.set_water(*c);
    }
    auto scene = std::make_unique<render::Scene>("", anno::formats::insel::islands(buf).value(),
                                                 std::move(textures), std::move(anlagen),
                                                 render::kHeightLift);
    const auto s = anno::formats::sze::Scene::parse(buf);
    REQUIRE(s);
    const anno::assets::unit::VolumeReader read =
        [&vols](std::uint32_t g, std::uint64_t o,
                std::uint64_t l) -> std::optional<std::vector<std::uint8_t>> {
        if (g >= vols.size() || o + l > vols[g].size()) {
            return std::nullopt;
        }
        const auto b = vols[g].begin() + static_cast<std::ptrdiff_t>(o);
        return std::vector<std::uint8_t>(b, b + static_cast<std::ptrdiff_t>(l));
    };
    const std::array kinds = {anno::assets::unit::UnitKind::Ship,
                              anno::assets::unit::UnitKind::Soldier};
    auto units = anno::assets::unit::units(s.value(), header, kinds,
                                           anno::assets::figure::kOriginalPlayerColours, read)
                     .value();
    for (auto& u : units.units) {
        std::optional<float> z;
        if (u.kind == anno::assets::unit::UnitKind::Soldier) {
            z = scene->ground_height(u.x, u.y);
            if (!z) {
                continue;
            }
        }
        render::UnitSprite sprite{u.x, u.y, z, {}, {}};
        for (const auto& f : u.frames) {
            sprite.frames.push_back(f ? std::optional<render::UnitFrame>({f->frame, f->image})
                                      : std::nullopt);
        }
        for (const auto& f : u.shadows) {
            sprite.shadows.push_back(f ? std::optional<render::UnitFrame>({f->frame, f->image})
                                       : std::nullopt);
        }
        scene->units.push_back(std::move(sprite));
    }
    out.scene = std::move(scene);
    return out;
}

/// A device, or nullopt (skip) when there is none; with ANNO_REQUIRE_GPU
/// set a missing device fails the test instead.
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

/// The view of `group` used by the CPU frame digests (render_corpus_test).
render::Rect digest_view(const render::Scene& scene, const char* name, std::size_t group, int w,
                         int h) {
    const auto b = *scene.bounds(group);
    const int dx = std::string_view(name) == "Single_24.sze" && group == 0 ? 2000 : 0;
    return {b.x + b.w / 2 - w / 2 + dx, b.y + b.h / 2 - h / 2, w, h};
}

} // namespace

TEST_CASE("the frame uniform matches struct Frame in gpu.wgsl") {
    gpu::FrameUniform f;
    f.view = {-5, 7, 1280, 720};
    f.pattern_size = {256, 128};
    f.step0 = 3;
    f.step1 = 4;
    f.frac = 0.25F;
    f.time_ms = 1500.0F;
    f.enhanced = true;
    f.tile_w = 64.0F;
    f.tile_h = 32.0F;
    f.raise = 9.0F;
    f.fluid_origin = {-100, 200};
    f.fluid_size = 1024;
    f.fluid_on = true;
    f.fluid_cpt = 16.0F;
    const auto b = gpu::frame_uniform_bytes(f);
    REQUIRE(b.size() == 96);
    auto u32_at = [&](std::size_t o) {
        return std::uint32_t{b[o]} | std::uint32_t{b[o + 1]} << 8 | std::uint32_t{b[o + 2]} << 16 |
               std::uint32_t{b[o + 3]} << 24;
    };
    auto i32_at = [&](std::size_t o) { return static_cast<std::int32_t>(u32_at(o)); };
    auto f32_at = [&](std::size_t o) {
        const std::uint32_t v = u32_at(o);
        float x = 0;
        std::memcpy(&x, &v, 4);
        return x;
    };
    // WGSL uniform layout of struct Frame (offsets by the alignment rules).
    CHECK(i32_at(0) == -5);
    CHECK(i32_at(4) == 7);
    CHECK(u32_at(8) == 1280);
    CHECK(u32_at(12) == 720);
    CHECK(i32_at(16) == 256);
    CHECK(i32_at(20) == 128);
    CHECK(u32_at(24) == 3);
    CHECK(u32_at(28) == 4);
    CHECK(f32_at(32) == 0.25F);
    CHECK(f32_at(36) == 1500.0F);
    CHECK(u32_at(40) == 1);
    CHECK(f32_at(44) == 64.0F);
    CHECK(f32_at(48) == 32.0F);
    CHECK(f32_at(52) == 9.0F);
    CHECK(i32_at(64) == -100);
    CHECK(i32_at(68) == 200);
    CHECK(i32_at(72) == 1024);
    CHECK(u32_at(76) == 1);
    CHECK(f32_at(80) == 16.0F);
    // A zero-sized view is sent as 1 × 1 (the shader divides by it).
    f.view = {0, 0, 0, 0};
    const auto z = gpu::frame_uniform_bytes(f);
    CHECK(z[8] == 1);
    CHECK(z[12] == 1);
}

TEST_CASE("GPU frames match the CPU path (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    // Same scenes and views as the CPU frame digests (render_corpus_test),
    // with ships and soldiers (sprites with shadows).
    // Original water: within 1 per channel of the CPU path (blending
    // rounding; docs/render/iso.md "Fidelity"). Enhanced water is not
    // compared with the CPU: the GPU shades it with the wave simulation, the
    // CPU path keeps the parametric model, so they differ by design (the
    // numbers are printed); it must animate and raise no GPU error.
    gpu::GpuLimits limits;
    // Every visible chunk on the GPU in the first frame (the viewer spreads
    // uploads over frames; here the frame must be complete).
    limits.uploads_per_frame = SIZE_MAX;
    constexpr int kW = 1280, kH = 720;
    for (const char* name : {"Single_01.sze", "Single_24.sze"}) {
        const Loaded loaded = load_scene(*dir, name);
        const auto& scene = loaded.scene;
        // A compositor caches per scene (atlas, sea, chunks): one per scene.
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
        REQUIRE_MESSAGE(made, (made ? "" : made.error()));
        gpu::GpuFrame& g = *made.value();
        const auto target = g.offscreen_target(kW, kH);
        render::FrameRenderer cpu(4), on_gpu(4);
        std::size_t sprites = 0;
        for (const std::size_t group : {std::size_t{2}, std::size_t{1}, std::size_t{0}}) {
            const render::Rect view = digest_view(*scene, name, group, kW, kH);
            std::vector<std::uint8_t> enhanced;
            for (const auto& [time, style] :
                 {std::pair{std::uint64_t{0}, render::WaterStyle::Original},
                  std::pair{std::uint64_t{900}, render::WaterStyle::Original},
                  std::pair{std::uint64_t{900}, render::WaterStyle::Enhanced},
                  std::pair{std::uint64_t{1900}, render::WaterStyle::Enhanced}}) {
                std::vector<std::uint32_t> out(std::size_t{kW} * kH);
                cpu.render(*scene, view, group, time, style, out.data());
                const auto st =
                    g.render(on_gpu, *scene, view, group, time, style, target.second.get());
                CHECK(st.chunks_missing == 0);
                sprites += st.sprites;
                const auto px = g.read_back(target.first.get(), kW, kH);
                REQUIRE(px.size() == out.size() * 4);
                const auto [max, differ] = difference(out, px);
                INFO(std::string(name)
                     << " group " << group << " t" << time << " "
                     << (style == render::WaterStyle::Original ? "original" : "enhanced")
                     << ": max " << max << ", " << differ << " pixels > 2");
                if (style == render::WaterStyle::Original) {
                    CHECK(max <= 1);
                } else if (enhanced.empty()) {
                    MESSAGE(std::string(name) << " group " << group << " enhanced: max " << max
                                              << ", " << differ << " pixels differ by more than 2");
                    enhanced = px;
                } else {
                    // The sea moves between 900 and 1900 ms.
                    CHECK(px != enhanced);
                }
            }
        }
        // Water objects and units were drawn from the atlas, so the check
        // is not vacuous (Single_01's views hold none: 0 there).
        MESSAGE(std::string(name) << ": " << sprites << " sprites drawn");
        if (std::string_view(name) == "Single_24.sze") {
            CHECK(sprites > 0);
        }
        CHECK(g.errors() == 0);
    }
}

TEST_CASE("GPU frames: atlas growth, odd sizes and spread uploads (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const char* name = "Single_24.sze";
    const Loaded loaded = load_scene(*dir, name);
    const auto& scene = loaded.scene;
    // A 256² atlas forces new layers (copying the old ones); 1001 × 601 makes
    // the read-back rows need padding; default upload limits fill the view
    // over several frames (stand-ins first).
    gpu::GpuLimits limits;
    limits.atlas_size = 256;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    constexpr int kW = 1001, kH = 601;
    const auto target = g.offscreen_target(kW, kH);
    render::FrameRenderer cpu(4), on_gpu(4);
    for (const std::size_t group : {std::size_t{2}, std::size_t{1}}) {
        const render::Rect view = digest_view(*scene, name, group, kW, kH);
        std::vector<std::uint32_t> out(std::size_t{kW} * kH);
        cpu.render(*scene, view, group, 0, render::WaterStyle::Original, out.data());
        std::size_t sprites = 0;
        // 4–6 chunks per view, 6 uploads per frame: complete after a few.
        for (int f = 0; f < 8; ++f) {
            sprites = g.render(on_gpu, *scene, view, group, 0, render::WaterStyle::Original,
                               target.second.get())
                          .sprites;
        }
        const auto px = g.read_back(target.first.get(), kW, kH);
        REQUIRE(px.size() == out.size() * 4);
        const auto [max, differ] = difference(out, px);
        INFO("group " << group << ": max " << max << ", " << differ << " pixels > 2, " << sprites
                      << " sprites");
        CHECK(sprites > 0);
        CHECK(max <= 1);
    }
    CHECK(g.errors() == 0);
    // Limits outside the device's range are refused.
    gpu::GpuLimits bad;
    bad.atlas_size = 0;
    CHECK_FALSE(gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, bad));
    CHECK_FALSE(gpu::GpuFrame::create(*ctx, WGPUTextureFormat_BGRA8UnormSrgb));
}

TEST_CASE("the GPU wave solver matches the CPU reference") {
    // Port of gpu_fluid::tests::gpu_matches_cpu_reference: a beach, 40
    // frames with a recentre after 20, state within 0.01 of FluidGrid.
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    namespace fluid = anno::render::fluid;
    fluid::FluidConfig cfg;
    cfg.size = 64;
    cfg.cells_per_tile = {4, 4, 4};
    cfg.recentre_cells = 4;
    const fluid::DepthFn depth = [](float x, float y) {
        return 16.0F - 2.0F * std::max(x - 2.0F, 0.0F) + 0.3F * std::sin(y * 0.7F);
    };
    auto made = gpu::FluidGpu::create(*ctx, cfg);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::FluidGpu& g = made.value();
    const WGPUDevice d = ctx->device.get();
    const WGPUQueue q = ctx->queue.get();
    auto run = [&](std::pair<float, float> centre, std::uint64_t time_ms) {
        WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
        const gpu::CommandEncoder enc(wgpuDeviceCreateCommandEncoder(d, &ed));
        g.update(enc.get(), 0, centre, 0.0F, time_ms, depth, 2);
        WGPUCommandBufferDescriptor cd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
        const gpu::CommandBuffer cmd(wgpuCommandEncoderFinish(enc.get(), &cd));
        const WGPUCommandBuffer raw = cmd.get();
        wgpuQueueSubmit(q, 1, &raw);
    };
    std::uint64_t time_ms = 1000;
    // The first update creates the window at the current time.
    run({8.0F, 8.0F}, time_ms);
    REQUIRE(g.window());
    std::optional<fluid::FluidGrid> cpu;
    cpu.emplace(cfg, *g.window(), depth, static_cast<float>(g.sim_time()));
    for (int frame = 0; frame < 40; ++frame) {
        // Move the view after 20 frames so new strips are filled.
        const std::pair<float, float> centre =
            frame < 20 ? std::pair{8.0F, 8.0F} : std::pair{10.0F, 9.5F};
        const double before = g.sim_time();
        run(centre, time_ms);
        const fluid::FluidWindow w = *g.window();
        if (!(cpu->window == w)) {
            // Same strips as the GPU fill.
            fluid::FluidWindow old = cpu->window;
            const auto rects = old.recentre(centre, cfg.recentre_cells);
            cpu->window = old;
            for (const auto& r : rects) {
                cpu->fill(r, depth, static_cast<float>(before));
            }
        }
        for (double t = before; t + 1e-9 < g.sim_time();) {
            t += static_cast<double>(cfg.dt);
            cpu->step(static_cast<float>(t));
        }
        time_ms += 33;
    }
    auto frame = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
    REQUIRE(frame);
    const auto bytes = frame.value()->read_back(g.state_texture(), 64, 64, 16);
    REQUIRE(bytes.size() == cpu->state.size() * 16);
    float max = 0.0F, largest = 0.0F;
    for (std::size_t i = 0; i < cpu->state.size(); ++i) {
        for (std::size_t c = 0; c < 4; ++c) {
            float v = 0.0F;
            std::memcpy(&v, bytes.data() + 16 * i + 4 * c, 4);
            max = std::max(max, std::abs(v - cpu->state[i][c]));
            largest = std::max(largest, std::abs(cpu->state[i][c]));
        }
    }
    MESSAGE("max difference " << max << ", largest state value " << largest);
    CHECK(max < 1e-2F);
    // The waves moved: a still sea would make the comparison trivial.
    CHECK(largest > 0.1F);
}
