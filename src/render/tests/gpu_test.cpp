// GPU device, shaders and compositor (wgpu-native). Skips when the machine
// has no GPU adapter (e.g. CI runners without one) and, for the frame
// comparison, without the game files (ANNO1503_GAME_DIR).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
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
#include <anno/render/iso.hpp>
#include <anno/render/river.hpp>
#include <anno/render/scene.hpp>
#include <anno/render/sea.hpp>
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
            sprite.frames.push_back(
                f ? std::optional<render::UnitFrame>(render::to_unit_frame(*f, u.figure, false))
                  : std::nullopt);
        }
        for (const auto& f : u.shadows) {
            sprite.shadows.push_back(
                f ? std::optional<render::UnitFrame>(render::to_unit_frame(*f, u.figure, true))
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
    f.ship_water = true;
    f.fluid_origin = {-100, 200};
    f.fluid_size = 1024;
    f.fluid_on = true;
    f.fluid_cpt = 16.0F;
    f.river_flow = true;
    f.river_lambda = 2.059F;
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
    CHECK(f32_at(56) == 1.0F);
    CHECK(i32_at(64) == -100);
    CHECK(i32_at(68) == 200);
    CHECK(i32_at(72) == 1024);
    CHECK(u32_at(76) == 1);
    CHECK(f32_at(80) == 16.0F);
    CHECK(f32_at(84) == 1.0F);
    CHECK(f32_at(88) == 2.059F);
    CHECK(f32_at(92) == 0.0F);
    f.river_flow = false;
    CHECK(gpu::frame_uniform_bytes(f)[84 + 3] == 0);
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
    // with ships and soldiers loaded (the views hold water sprites but no
    // shadowed or translucent ones: none reach the GPU sprite path in the
    // 0200Single corpus; review 2026-10-18).
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
        render::FrameRenderer cpu(4), on_gpu(4, false);
        std::size_t sprites = 0, ships = 0;
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
                CHECK(st.wave_simulation == (style == render::WaterStyle::Enhanced));
                sprites += st.sprites;
                ships += st.ships;
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
        // Water objects and ships were drawn from the atlas, so the check
        // is not vacuous (Single_01's views hold none: 0 there).
        MESSAGE(std::string(name) << ": " << sprites << " sprites, " << ships << " ships drawn");
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
    // the read-back rows need padding; default upload limits (several frames
    // until the view is complete; the stand-ins are tested separately).
    gpu::GpuLimits limits;
    limits.atlas_size = 256;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    constexpr int kW = 1001, kH = 601;
    const auto target = g.offscreen_target(kW, kH);
    render::FrameRenderer cpu(4), on_gpu(4, false);
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

TEST_CASE("enhanced water without the wave simulation stays close to the CPU path (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    gpu::GpuLimits limits;
    limits.uploads_per_frame = SIZE_MAX;
    limits.wave_simulation = false;
    constexpr int kW = 1280, kH = 720;
    for (const char* name : {"Single_01.sze", "Single_24.sze"}) {
        const Loaded loaded = load_scene(*dir, name);
        const auto& scene = loaded.scene;
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
        REQUIRE(made);
        gpu::GpuFrame& g = *made.value();
        const auto target = g.offscreen_target(kW, kH);
        render::FrameRenderer cpu(4), on_gpu(4, false);
        for (const std::size_t group : {std::size_t{2}, std::size_t{1}, std::size_t{0}}) {
            const render::Rect view = digest_view(*scene, name, group, kW, kH);
            for (const std::uint64_t time : {std::uint64_t{925}, std::uint64_t{1900}}) {
                std::vector<std::uint32_t> out(std::size_t{kW} * kH);
                cpu.render(*scene, view, group, time, render::WaterStyle::Enhanced, out.data());
                const auto st = g.render(on_gpu, *scene, view, group, time,
                                         render::WaterStyle::Enhanced, target.second.get());
                CHECK_FALSE(st.wave_simulation);
                const auto px = g.read_back(target.first.get(), kW, kH);
                REQUIRE(px.size() == out.size() * 4);
                const auto [max, differ] = difference(out, px);
                MESSAGE("" << name << " group " << group << " t" << time << ": max " << max << ", "
                           << differ << " pixels > 2");
                // Shoreline pixels differ (steep alpha edge amplifies float
                // differences of the swash); the rest must match.
                CHECK(differ <= out.size() / 50);
            }
        }
        CHECK(g.errors() == 0);
    }
}

TEST_CASE("the wave simulation shows on screen (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    // Same frame with and without the simulation: they must differ (the
    // simulation is sampled).
    constexpr int kW = 1280, kH = 720;
    const char* name = "Single_24.sze";
    const Loaded loaded = load_scene(*dir, name);
    const auto& scene = loaded.scene;
    gpu::GpuLimits on, off;
    on.uploads_per_frame = off.uploads_per_frame = SIZE_MAX;
    off.wave_simulation = false;
    auto a = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, on);
    auto b = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, off);
    REQUIRE(a);
    REQUIRE(b);
    const auto ta = a.value()->offscreen_target(kW, kH);
    const auto tb = b.value()->offscreen_target(kW, kH);
    render::FrameRenderer fa(4, false), fb(4, false);
    const render::Rect view = digest_view(*scene, name, 1, kW, kH);
    for (std::uint64_t t = 1000; t <= 1000 + 33 * 30; t += 33) {
        const auto st = a.value()->render(fa, *scene, view, 1, t, render::WaterStyle::Enhanced,
                                          ta.second.get());
        CHECK(st.wave_simulation);
    }
    b.value()->render(fb, *scene, view, 1, 1000 + 33 * 30, render::WaterStyle::Enhanced,
                      tb.second.get());
    const auto pa = a.value()->read_back(ta.first.get(), kW, kH);
    const auto pb = b.value()->read_back(tb.first.get(), kW, kH);
    REQUIRE(pa.size() == pb.size());
    std::size_t differ = 0;
    for (std::size_t i = 0; i < pa.size(); i += 4) {
        int m = 0;
        for (std::size_t k = 0; k < 3; ++k) {
            m = std::max(m, std::abs(int{pa[i + k]} - int{pb[i + k]}));
        }
        differ += m > 8 ? 1U : 0U;
    }
    MESSAGE("pixels differing with/without the simulation: " << differ);
    CHECK(differ > 10000);
    CHECK(a.value()->errors() == 0);
}

namespace {

struct FluidRun {
    gpu::Context& ctx;
    gpu::FluidGpu& g;
    const anno::render::fluid::DepthFn& depth;
    void operator()(std::size_t group, std::pair<float, float> centre, float span,
                    std::uint64_t time_ms) const {
        WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
        const gpu::CommandEncoder enc(wgpuDeviceCreateCommandEncoder(ctx.device.get(), &ed));
        g.update(enc.get(), group, centre, span, time_ms, depth, 2);
        WGPUCommandBufferDescriptor cd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
        // An invalid compute pass must fail the check, not abort the binary
        // in wgpuQueueSubmit (wgpu-native 29).
        wgpuDevicePushErrorScope(ctx.device.get(), WGPUErrorFilter_Validation);
        const gpu::CommandBuffer cmd(wgpuCommandEncoderFinish(enc.get(), &cd));
        const std::string err = gpu::pop_error_scope(ctx.instance.get(), ctx.device.get());
        REQUIRE_MESSAGE(err.empty(), err);
        const WGPUCommandBuffer raw = cmd.get();
        wgpuQueueSubmit(ctx.queue.get(), 1, &raw);
    }
};

/// Largest |GPU − CPU| over the state of `g` and the largest |u|, |v|.
std::pair<float, float> compare_state(gpu::Context& ctx, gpu::FluidGpu& g,
                                      const anno::render::fluid::FluidGrid& cpu, std::uint32_t n) {
    auto frame = gpu::GpuFrame::create(ctx, WGPUTextureFormat_RGBA8Unorm);
    REQUIRE(frame);
    const auto bytes = frame.value()->read_back(g.state_texture(), n, n, 16);
    REQUIRE(bytes.size() == cpu.state.size() * 16);
    float max = 0.0F, flow = 0.0F;
    for (std::size_t i = 0; i < cpu.state.size(); ++i) {
        for (std::size_t c = 0; c < 4; ++c) {
            float v = 0.0F;
            std::memcpy(&v, bytes.data() + 16 * i + 4 * c, 4);
            max = std::max(max, std::abs(v - cpu.state[i][c]));
            if (c == 1 || c == 2) {
                flow = std::max(flow, std::abs(v));
            }
        }
    }
    return {max, flow};
}

} // namespace

TEST_CASE("the GPU wave solver: time stepping, skip-ahead, time going back, group change") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    namespace fluid = anno::render::fluid;
    fluid::FluidConfig cfg;
    cfg.size = 64;
    cfg.cells_per_tile = {4, 2, 4};
    cfg.recentre_cells = 4;
    const fluid::DepthFn depth = [](float x, float y) {
        return 16.0F - 2.0F * std::max(x - 2.0F, 0.0F) + 0.3F * std::sin(y * 0.7F);
    };
    auto made = gpu::FluidGpu::create(*ctx, cfg);
    REQUIRE(made);
    gpu::FluidGpu& g = made.value();
    const FluidRun run{*ctx, g, depth};
    const double dt = cfg.dt;
    run(0, {8.0F, 8.0F}, 0.0F, 1000);
    CHECK(g.sim_time() == doctest::Approx(1.0));
    // Normal frame: within one step behind the target, never ahead.
    run(0, {8.0F, 8.0F}, 0.0F, 1033);
    CHECK(g.sim_time() <= 1.033 + 1e-9);
    CHECK(g.sim_time() > 1.033 - dt);
    // Far behind: skips ahead, ends within one step of the target.
    run(0, {8.0F, 8.0F}, 0.0F, 61000);
    CHECK(g.sim_time() <= 61.0 + 1e-9);
    CHECK(g.sim_time() > 61.0 - dt);
    // Time went back: restarts there.
    run(0, {8.0F, 8.0F}, 0.0F, 5000);
    CHECK(g.sim_time() <= 5.0 + 1e-9);
    CHECK(g.sim_time() > 5.0 - dt);
    // Another zoom group: a new window with that group's cells per tile,
    // filled at the target time.
    run(1, {8.0F, 8.0F}, 0.0F, 7000);
    REQUIRE(g.window());
    CHECK(g.window()->cells_per_tile == 2);
    CHECK(g.sim_time() == doctest::Approx(7.0));
    fluid::FluidGrid cpu(cfg, *g.window(), depth, static_cast<float>(g.sim_time()));
    const auto [max, flow] = compare_state(*ctx, g, cpu, 64);
    MESSAGE("group-change max " << max);
    CHECK(max < 1e-4F);
    CHECK(ctx->errors->load() == 0);
}

TEST_CASE("the GPU wave solver resizes its window and keeps matching the CPU") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    namespace fluid = anno::render::fluid;
    fluid::FluidConfig cfg;
    cfg.size = 100; // not a multiple of 8 or 64: partial workgroups
    cfg.cells_per_tile = {4, 4, 4};
    cfg.recentre_cells = 4;
    cfg.sponge_cells = 8;
    const fluid::DepthFn depth = [](float x, float y) {
        return 16.0F - 2.0F * std::max(x - 6.0F, 0.0F) + 0.3F * std::sin(y * 0.7F);
    };
    auto made = gpu::FluidGpu::create(*ctx, cfg);
    REQUIRE(made);
    gpu::FluidGpu& g = made.value();
    const FluidRun run{*ctx, g, depth};
    std::uint64_t time_ms = 1000;
    // Small first (64), then a larger view: window 100 (clamped to size).
    run(0, {8.0F, 8.0F}, 0.0F, time_ms);
    REQUIRE(g.window());
    CHECK(g.window()->size == 64);
    time_ms += 33;
    run(0, {8.0F, 8.0F}, 60.0F, time_ms);
    REQUIRE(g.window());
    CHECK(g.window()->size == 100);
    fluid::FluidGrid cpu(cfg, *g.window(), depth, static_cast<float>(g.sim_time()));
    for (int frame = 0; frame < 60; ++frame) {
        const double before = g.sim_time();
        time_ms += 33;
        run(0, {8.0F, 8.0F}, 60.0F, time_ms);
        for (double t = before; t + 1e-9 < g.sim_time();) {
            t += static_cast<double>(cfg.dt);
            cpu.step(static_cast<float>(t));
        }
    }
    const auto [max, flow] = compare_state(*ctx, g, cpu, 100);
    MESSAGE("max " << max << ", largest flow " << flow);
    CHECK(max < 1e-2F);
    // The sea moved (u, v are 0 in a still sea; η alone holds the bed −h).
    CHECK(flow > 0.05F);
    CHECK(ctx->errors->load() == 0);
}

TEST_CASE("the GPU wave solver matches the CPU reference") {
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
    REQUIRE(made);
    gpu::FluidGpu& g = made.value();
    const FluidRun run{*ctx, g, depth};
    std::uint64_t time_ms = 1000;
    run(0, {8.0F, 8.0F}, 0.0F, time_ms);
    const double t0 = g.sim_time();
    std::optional<fluid::FluidGrid> cpu;
    cpu.emplace(cfg, *g.window(), depth, static_cast<float>(g.sim_time()));
    // The initial state, to show the sea changed.
    const fluid::FluidGrid initial = *cpu;
    for (int frame = 0; frame < 40; ++frame) {
        const std::pair<float, float> centre =
            frame < 20 ? std::pair{8.0F, 8.0F} : std::pair{10.0F, 9.5F};
        const double before = g.sim_time();
        run(0, centre, 0.0F, time_ms);
        const fluid::FluidWindow w = *g.window();
        if (!(cpu->window == w)) {
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
        if (frame == 20) {
            // Right after the recentre: the new strips match closely.
            const auto [m20, f20] = compare_state(*ctx, g, *cpu, 64);
            MESSAGE("after the recentre: max " << m20);
            CHECK(m20 < 1e-3F);
        }
        time_ms += 33;
    }
    // The simulation advanced with the clock (40 frames × 33 ms).
    CHECK(g.sim_time() - t0 > 1.2);
    float changed = 0.0F;
    for (std::size_t i = 0; i < initial.state.size(); ++i) {
        changed = std::max(changed, std::abs(cpu->state[i][1] - initial.state[i][1]));
    }
    CHECK(changed > 0.01F);
    const auto [max, flow] = compare_state(*ctx, g, *cpu, 64);
    CHECK(max < 1e-2F);
}

TEST_CASE("read_back reports a failed copy") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
    REQUIRE(made);
    const auto t = made.value()->offscreen_target(16, 16);
    const auto before = made.value()->errors();
    // Larger than the texture: the copy is invalid.
    const auto px = made.value()->read_back(t.first.get(), 32, 32);
    CHECK(px.empty());
    CHECK(made.value()->errors() > before);
}

TEST_CASE("a frame recorded with errors is counted, not submitted") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    const Loaded loaded = load_scene(*dir, "Single_01.sze");
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
    REQUIRE(made);
    WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    desc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size = {64, 64, 1};
    desc.format = WGPUTextureFormat_BGRA8Unorm;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    const gpu::Texture t(wgpuDeviceCreateTexture(ctx->device.get(), &desc));
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    const gpu::TextureView v(wgpuTextureCreateView(t.get(), &vd));
    render::FrameRenderer frames(2, false);
    made.value()->render(frames, *loaded.scene, {0, 0, 64, 64}, 2, 0, render::WaterStyle::Original,
                         v.get());
    // The RGBA8 pipelines cannot draw into a BGRA8 target: wgpu-native 29
    // aborts when such a command buffer is submitted; it must be dropped
    // and counted instead.
    CHECK(made.value()->errors() > 0);
}

TEST_CASE("atlas growth keeps earlier sprites, first frame at a late sea step (game files)") {
    const auto dir = anno::test::game_dir();
    auto ctx = device_or_skip();
    if (!dir || !ctx) {
        return;
    }
    const char* name = "Single_24.sze";
    const Loaded loaded = load_scene(*dir, name);
    const auto& scene = loaded.scene;
    gpu::GpuLimits limits;
    limits.atlas_size = 256;
    limits.uploads_per_frame = SIZE_MAX;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    gpu::GpuFrame& g = *made.value();
    constexpr int kW = 1280, kH = 720;
    const auto target = g.offscreen_target(kW, kH);
    render::FrameRenderer cpu(4), on_gpu(4, false);
    // Group 2 fills layer 0, group 1 grows the atlas, group 2 again reads
    // the copied layer. Times late in the animation cycle: the first frame
    // of a group must have the whole pattern.
    int k = 0;
    for (const std::size_t group : {std::size_t{2}, std::size_t{1}, std::size_t{2}}) {
        const std::uint64_t time = 2450 + 1000 * static_cast<std::uint64_t>(k++);
        const render::Rect view = digest_view(*scene, name, group, kW, kH);
        std::vector<std::uint32_t> out(std::size_t{kW} * kH);
        cpu.render(*scene, view, group, time, render::WaterStyle::Original, out.data());
        g.render(on_gpu, *scene, view, group, time, render::WaterStyle::Original,
                 target.second.get());
        const auto px = g.read_back(target.first.get(), kW, kH);
        REQUIRE(px.size() == out.size() * 4);
        const auto [max, differ] = difference(out, px);
        MESSAGE("group " << group << " t" << time << ": max " << max << ", " << differ);
        CHECK(max <= 1);
    }
    CHECK(g.errors() == 0);
}

TEST_CASE("uploads spread over frames: stand-ins first, then the real chunks (game files)") {
    const auto dir = anno::test::game_dir();
    auto ctx = device_or_skip();
    if (!dir || !ctx) {
        return;
    }
    const char* name = "Single_24.sze";
    const Loaded loaded = load_scene(*dir, name);
    const auto& scene = loaded.scene;
    gpu::GpuLimits limits;
    limits.uploads_per_frame = 1;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    gpu::GpuFrame& g = *made.value();
    constexpr int kW = 1001, kH = 601;
    const auto target = g.offscreen_target(kW, kH);
    render::FrameRenderer cpu(4), on_gpu(4, false);
    const render::Rect view = digest_view(*scene, name, 1, kW, kH);
    std::vector<std::uint32_t> out(std::size_t{kW} * kH);
    cpu.render(*scene, view, 1, 0, render::WaterStyle::Original, out.data());
    g.render(on_gpu, *scene, view, 1, 0, render::WaterStyle::Original, target.second.get());
    const auto first = g.read_back(target.first.get(), kW, kH);
    REQUIRE(first.size() == out.size() * 4);
    const auto [max0, differ0] = difference(out, first);
    MESSAGE("first frame: max " << max0 << ", " << differ0);
    // One chunk per frame: the first frame is incomplete (stand-ins).
    CHECK(differ0 > 0);
    for (int f = 0; f < 12; ++f) {
        g.render(on_gpu, *scene, view, 1, 0, render::WaterStyle::Original, target.second.get());
    }
    const auto px = g.read_back(target.first.get(), kW, kH);
    REQUIRE(px.size() == out.size() * 4);
    const auto [max, differ] = difference(out, px);
    MESSAGE("after 13 frames: max " << max << ", " << differ);
    CHECK(max <= 1);
    CHECK(g.errors() == 0);
}

TEST_CASE("a frame can be destroyed with GPU work in flight (game files)") {
    const auto dir = anno::test::game_dir();
    auto ctx = device_or_skip();
    if (!dir || !ctx) {
        return;
    }
    const Loaded loaded = load_scene(*dir, "Single_24.sze");
    for (int round = 0; round < 3; ++round) {
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
        REQUIRE(made);
        render::FrameRenderer frames(4, false);
        auto t = made.value()->offscreen_target(1920, 1080);
        const auto view = digest_view(*loaded.scene, "Single_24.sze", 0, 1920, 1080);
        for (std::uint64_t f = 0; f < 4; ++f) {
            made.value()->render(frames, *loaded.scene, view, 0, 1000 + 500 * f,
                                 render::WaterStyle::Enhanced, t.second.get());
        }
        // No wait: frame, fluid, atlas and target go while the GPU works.
        made.value().reset();
        t.second.reset();
        t.first.reset();
    }
    poll(*ctx, true);
    CHECK(ctx->errors->load() == 0);
}

TEST_CASE("stand-in chunks show the open sea (game files)") {
    const auto dir = anno::test::game_dir();
    auto ctx = device_or_skip();
    if (!dir || !ctx) {
        return;
    }
    // Single_01's digest views are open sea: a stand-in looks exactly like
    // the built chunk there.
    const char* name = "Single_01.sze";
    const Loaded loaded = load_scene(*dir, name);
    const auto& scene = loaded.scene;
    gpu::GpuLimits limits;
    limits.uploads_per_frame = 1;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    gpu::GpuFrame& g = *made.value();
    constexpr int kW = 1280, kH = 720;
    const auto target = g.offscreen_target(kW, kH);
    render::FrameRenderer cpu(4), on_gpu(4, false);
    const render::Rect view = digest_view(*scene, name, 1, kW, kH);
    std::vector<std::uint32_t> out(std::size_t{kW} * kH);
    cpu.render(*scene, view, 1, 300, render::WaterStyle::Original, out.data());
    g.render(on_gpu, *scene, view, 1, 300, render::WaterStyle::Original, target.second.get());
    const auto px = g.read_back(target.first.get(), kW, kH);
    REQUIRE(px.size() == out.size() * 4);
    const auto [max, differ] = difference(out, px);
    MESSAGE("first frame with stand-ins: max " << max << ", " << differ);
    CHECK(max <= 1);
}

TEST_CASE("device errors after every device owner is gone (use-after-free regression)") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    std::pair<gpu::Texture, gpu::TextureView> t;
    const std::weak_ptr<std::atomic<std::uint64_t>> counter = ctx->errors;
    {
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
        REQUIRE(made);
        t = made.value()->offscreen_target(16, 16);
    }
    ctx.reset();
    // A validation error on an object of the (released) device.
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    vd.format = WGPUTextureFormat_R32Float;
    // Before the fix the device's error callback wrote to the freed counter
    // (ASan: heap-use-after-free in on_uncaptured).
    REQUIRE_FALSE(counter.expired());
    const auto before = counter.lock()->load();
    const gpu::TextureView bad(wgpuTextureCreateView(t.first.get(), &vd));
    CHECK(counter.lock()->load() == before + 1);
}

TEST_CASE("FluidGpu::create refuses configurations the device cannot hold") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    namespace fluid = anno::render::fluid;
    WGPULimits lim = WGPU_LIMITS_INIT;
    wgpuDeviceGetLimits(ctx->device.get(), &lim);
    auto bad = [&](auto edit) {
        fluid::FluidConfig cfg;
        edit(cfg);
        return !gpu::FluidGpu::create(*ctx, cfg);
    };
    CHECK(bad([](fluid::FluidConfig& c) { c.size = 32; }));
    CHECK(bad([&](fluid::FluidConfig& c) { c.size = lim.maxTextureDimension2D + 1; }));
    CHECK(bad([](fluid::FluidConfig& c) { c.cells_per_tile = {4, 4}; }));
    CHECK(bad([](fluid::FluidConfig& c) { c.cells_per_tile = {4, 0, 4}; }));
    CHECK(bad([](fluid::FluidConfig& c) { c.max_steps = 0xFFFFFFFFU; }));
    fluid::FluidConfig ok;
    ok.size = 64;
    CHECK(gpu::FluidGpu::create(*ctx, ok));
    CHECK(ctx->errors->load() == 0);
}

TEST_CASE("a full sprite atlas drops new sprites without errors (game files)") {
    const auto dir = anno::test::game_dir();
    auto ctx = device_or_skip();
    if (!dir || !ctx) {
        return;
    }
    // One 256² layer cannot hold Single_24's water sprites at group 1 (the
    // growth test needs 3): later sprites are dropped, earlier ones and the
    // rest of the frame stay right.
    const char* name = "Single_24.sze";
    const Loaded loaded = load_scene(*dir, name);
    const auto& scene = loaded.scene;
    gpu::GpuLimits limits;
    limits.atlas_size = 256;
    limits.max_atlas_layers = 1;
    limits.uploads_per_frame = SIZE_MAX;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    gpu::GpuFrame& g = *made.value();
    constexpr int kW = 1280, kH = 720;
    const auto target = g.offscreen_target(kW, kH);
    render::FrameRenderer cpu(4), on_gpu(4, false);
    const render::Rect view = digest_view(*scene, name, 1, kW, kH);
    std::vector<std::uint32_t> out(std::size_t{kW} * kH);
    cpu.render(*scene, view, 1, 0, render::WaterStyle::Original, out.data());
    const auto st =
        g.render(on_gpu, *scene, view, 1, 0, render::WaterStyle::Original, target.second.get());
    const auto all =
        scene->sprites_in(view, 1, 0, render::SpriteLayer::WaterBelow, render::WaterStyle::Original)
            .size() +
        scene->sprites_in(view, 1, 0, render::SpriteLayer::WaterAbove, render::WaterStyle::Original)
            .size();
    const auto px = g.read_back(target.first.get(), kW, kH);
    REQUIRE(px.size() == out.size() * 4);
    const auto [max, differ] = difference(out, px);
    MESSAGE(st.sprites << " of " << all << " sprites drawn; " << differ << " pixels > 2");
    CHECK(st.sprites > 0);
    CHECK(st.sprites < all);
    // Only the dropped sprites differ.
    CHECK(differ > 0);
    CHECK(differ * 100 < out.size());
    CHECK(g.errors() == 0);
}

TEST_CASE("ships drawn by the compositor match the CPU path (game files)") {
    const auto dir = anno::test::game_dir();
    auto ctx = device_or_skip();
    if (!dir || !ctx) {
        return;
    }
    // The GPU path draws ships each frame (chunks without them); the CPU
    // path bakes them into the chunks after all island objects. Views
    // centred on the first ships of two scenarios, all zoom groups.
    for (const char* name : {"Single_01.sze", "Single_24.sze"}) {
        const Loaded loaded = load_scene(*dir, name);
        const auto& scene = loaded.scene;
        gpu::GpuLimits limits;
        limits.uploads_per_frame = SIZE_MAX;
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
        REQUIRE(made);
        gpu::GpuFrame& g = *made.value();
        constexpr int kW = 640, kH = 400;
        const auto target = g.offscreen_target(kW, kH);
        render::FrameRenderer cpu(4), on_gpu(4, false);
        std::size_t ships = 0, checked = 0;
        for (const auto& u : scene->units) {
            if (u.z || checked >= 3) {
                continue;
            }
            ++checked;
            for (const std::size_t group : {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
                const auto [ax, ay] = u.anchor(group, scene->lift);
                const render::Rect view{ax - kW / 2, ay - kH / 2, kW, kH};
                std::vector<std::uint32_t> out(std::size_t{kW} * kH);
                cpu.render(*scene, view, group, 300, render::WaterStyle::Original, out.data());
                const auto st = g.render(on_gpu, *scene, view, group, 300,
                                         render::WaterStyle::Original, target.second.get());
                ships += st.ships;
                const auto px = g.read_back(target.first.get(), kW, kH);
                REQUIRE(px.size() == out.size() * 4);
                const auto [max, differ] = difference(out, px);
                INFO(std::string(name) << " ship at " << u.x << "," << u.y << " group " << group
                                       << ": max " << max << ", " << differ << " pixels > 2");
                CHECK(st.ships > 0);
                CHECK(max <= 1);
            }
        }
        CHECK(checked > 0);
        MESSAGE(std::string(name) << ": " << ships << " ship sprites over " << checked * 3
                                  << " views");
        CHECK(g.errors() == 0);
    }
}

TEST_CASE("enhanced water: ships sit in the water, only at the hull bottom (game files)") {
    const auto dir = anno::test::game_dir();
    auto ctx = device_or_skip();
    if (!dir || !ctx) {
        return;
    }
    // The same enhanced frame with and without ship_waterline: they differ,
    // and only inside the ship's frame and in its lowest rows (the band
    // under SHIP_BAND); the original water never changes.
    const char* name = "Single_01.sze";
    const Loaded loaded = load_scene(*dir, name);
    const auto& scene = loaded.scene;
    const render::UnitSprite* ship = nullptr;
    for (const auto& u : scene->units) {
        if (!u.z) {
            ship = &u;
            break;
        }
    }
    REQUIRE(ship != nullptr);
    gpu::GpuLimits on, off;
    on.uploads_per_frame = off.uploads_per_frame = SIZE_MAX;
    off.ship_waterline = false;
    auto a = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, on);
    auto b = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, off);
    REQUIRE(a);
    REQUIRE(b);
    constexpr int kW = 640, kH = 480;
    const auto ta = a.value()->offscreen_target(kW, kH);
    const auto tb = b.value()->offscreen_target(kW, kH);
    render::FrameRenderer fa(4, false), fb(4, false);
    for (const std::size_t group : {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
        const auto [ax, ay] = ship->anchor(group, scene->lift);
        const render::Rect view{ax - kW / 2, ay - kH / 2, kW, kH};
        const auto& f = ship->frames[group]->frame;
        const auto* img = ship->frames[group]->image.get();
        // The frame rectangle in the view, and the band's top row.
        const int left = ax - f.anchor_x - view.x, top = ay - f.anchor_y - view.y;
        const double band_top = top + f.anchor_y - 40.0 * render::kTile[group].h / 42.0;
        for (const auto style : {render::WaterStyle::Enhanced, render::WaterStyle::Original}) {
            const auto sa =
                a.value()->render(fa, *scene, view, group, 2000, style, ta.second.get());
            b.value()->render(fb, *scene, view, group, 2000, style, tb.second.get());
            CHECK(sa.ships == 1);
            const auto pa = a.value()->read_back(ta.first.get(), kW, kH);
            const auto pb = b.value()->read_back(tb.first.get(), kW, kH);
            REQUIRE(pa.size() == pb.size());
            std::size_t differ = 0, outside = 0;
            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    const std::size_t i = (std::size_t(y) * kW + std::size_t(x)) * 4;
                    if (pa[i] == pb[i] && pa[i + 1] == pb[i + 1] && pa[i + 2] == pb[i + 2]) {
                        continue;
                    }
                    ++differ;
                    const bool in_frame = x >= left && x < left + img->width && y >= top &&
                                          y < top + img->height && y >= band_top;
                    outside += in_frame ? 0U : 1U;
                }
            }
            INFO("group " << group
                          << (style == render::WaterStyle::Enhanced ? " enhanced" : " original")
                          << ": " << differ << " pixels differ, " << outside
                          << " outside the hull band");
            if (style == render::WaterStyle::Enhanced) {
                CHECK(differ > 20);
            } else {
                CHECK(differ == 0);
            }
            CHECK(outside == 0);
        }
    }
    CHECK(a.value()->errors() == 0);
}

TEST_CASE("enhanced water meets objects standing in it (game files)") {
    const auto dir = anno::test::game_dir();
    auto ctx = device_or_skip();
    if (!dir || !ctx) {
        return;
    }
    // A river mouth of Single_01 with rocks in the water (tile 274, 447):
    // the same frame with and without object_waterline differs with
    // enhanced water (rock bases sink in) and is identical with original
    // water; no GPU errors.
    const Loaded loaded = load_scene(*dir, "Single_01.sze");
    const auto& scene = loaded.scene;
    gpu::GpuLimits on, off;
    on.uploads_per_frame = off.uploads_per_frame = SIZE_MAX;
    off.object_waterline = false;
    auto a = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, on);
    auto b = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, off);
    REQUIRE(a);
    REQUIRE(b);
    constexpr int kW = 640, kH = 400;
    const auto ta = a.value()->offscreen_target(kW, kH);
    const auto tb = b.value()->offscreen_target(kW, kH);
    render::FrameRenderer fa(4, false), fb(4, false);
    const auto [cx, cy] = render::tile_top(274, 447, 0);
    const render::Rect view{cx - kW / 2, cy - kH / 2, kW, kH};
    for (const auto style : {render::WaterStyle::Enhanced, render::WaterStyle::Original}) {
        a.value()->render(fa, *scene, view, 0, 2000, style, ta.second.get());
        b.value()->render(fb, *scene, view, 0, 2000, style, tb.second.get());
        const auto pa = a.value()->read_back(ta.first.get(), kW, kH);
        const auto pb = b.value()->read_back(tb.first.get(), kW, kH);
        REQUIRE(pa.size() == pb.size());
        std::size_t differ = 0;
        for (std::size_t i = 0; i < pa.size(); i += 4) {
            differ +=
                (pa[i] != pb[i] || pa[i + 1] != pb[i + 1] || pa[i + 2] != pb[i + 2]) ? 1U : 0U;
        }
        INFO((style == render::WaterStyle::Enhanced ? "enhanced" : "original")
             << ": " << differ << " pixels differ");
        if (style == render::WaterStyle::Enhanced) {
            CHECK(differ > 100);
        } else {
            CHECK(differ == 0);
        }
    }
    CHECK(a.value()->errors() == 0);
}

namespace {

/// Runs `run` once per `ms_step` until `done()` or `seconds` pass; true if done.
template <class Run, class Done> bool poll_until(Run&& run, Done&& done, double seconds) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    while (std::chrono::steady_clock::now() < end) {
        run();
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

} // namespace

TEST_CASE("the wave solver sets its window up in the background without blocking frames") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    namespace fluid = anno::render::fluid;
    fluid::FluidConfig cfg;
    cfg.size = 128;
    cfg.cells_per_tile = {4, 2, 4};
    cfg.recentre_cells = 8;
    cfg.sponge_cells = 8;
    const fluid::DepthFn depth = [](float x, float y) {
        return 16.0F - 2.0F * std::max(x - 6.0F, 0.0F) + 0.3F * std::sin(y * 0.7F);
    };
    auto made = gpu::FluidGpu::create(*ctx, cfg);
    REQUIRE(made);
    gpu::FluidGpu& g = made.value();
    g.set_background(true);
    // A tiny budget: the window arrives over several frames.
    g.upload_bytes_per_frame = 64 * 1024;
    const FluidRun run{*ctx, g, depth};
    const std::uint64_t time_ms = 4000;
    // The first frame has no window yet and nothing to step.
    run(0, {8.0F, 8.0F}, 20.0F, time_ms);
    CHECK_FALSE(g.window());
    CHECK(g.setting_up());
    int frames = 1;
    REQUIRE(poll_until(
        [&] {
            run(0, {8.0F, 8.0F}, 20.0F, time_ms);
            ++frames;
        },
        [&] { return g.window().has_value(); }, 20.0));
    // Uploaded over several frames (128² cells × 20 bytes in 64 KiB slices).
    CHECK(frames > 3);
    REQUIRE(g.window());
    CHECK(g.window()->cells_per_tile == 4);
    CHECK(g.window()->size == 128);
    CHECK(g.sim_time() == doctest::Approx(4.0));
    // Same time: the state is the freshly filled one, equal to the CPU's.
    fluid::FluidGrid cpu(cfg, *g.window(), depth, static_cast<float>(g.sim_time()));
    const auto [max0, flow0] = compare_state(*ctx, g, cpu, 128);
    MESSAGE("background window: max " << max0);
    CHECK(max0 < 1e-4F);
    // Another zoom group: the old window keeps serving while the new one is
    // prepared, then it is replaced.
    run(1, {8.0F, 8.0F}, 20.0F, time_ms);
    CHECK(g.setting_up());
    REQUIRE(g.window());
    if (g.window()->cells_per_tile == 4) {
        MESSAGE("old window kept while the new one is prepared");
    }
    REQUIRE(poll_until(
        [&] { run(1, {8.0F, 8.0F}, 20.0F, time_ms); },
        [&] { return g.window() && g.window()->cells_per_tile == 2 && !g.setting_up(); }, 20.0));
    CHECK(g.window()->cells_per_tile == 2);
    fluid::FluidGrid cpu1(cfg, *g.window(), depth, static_cast<float>(g.sim_time()));
    const auto [max1, flow1] = compare_state(*ctx, g, cpu1, 128);
    MESSAGE("after the group change: max " << max1);
    CHECK(max1 < 1e-4F);
    // A recentre writes strips of new cells into the grid in use.
    const fluid::FluidWindow before = *g.window();
    REQUIRE(poll_until(
        [&] { run(1, {14.0F, 8.0F}, 20.0F, time_ms); },
        [&] { return g.window() && g.window()->origin != before.origin && !g.setting_up(); },
        20.0));
    fluid::FluidGrid cpu2(cfg, before, depth, static_cast<float>(g.sim_time()));
    fluid::FluidWindow moved = before;
    const auto rects = moved.recentre({14.0F, 8.0F}, cfg.recentre_cells);
    cpu2.window = moved;
    for (const auto& r : rects) {
        cpu2.fill(r, depth, static_cast<float>(g.sim_time()));
    }
    CHECK(*g.window() == moved);
    const auto [max2, flow2] = compare_state(*ctx, g, cpu2, 128);
    MESSAGE("after the recentre: max " << max2);
    CHECK(max2 < 1e-3F);
    CHECK(ctx->errors->load() == 0);
    // Destroying the solver with a job in flight waits for its thread.
    run(0, {40.0F, 40.0F}, 20.0F, time_ms + 100);
    CHECK(g.setting_up());
}

TEST_CASE("background chunk workers deliver chunks and free evicted ones off the window thread "
          "(game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const Loaded loaded = load_scene(*dir, "Single_01.sze");
    const auto& scene = *loaded.scene;
    render::FrameRenderer frames(4, false);
    frames.spare_chunks = 2;
    frames.set_workers(std::make_unique<render::ChunkWorkers>(scene, 2));
    // Pan across the scene so that chunks are built and evicted again and
    // again; the cache stays bounded by the view, its ring and the spares.
    const auto b = *scene.bounds(2);
    std::size_t largest = 0;
    for (int step = 0; step < 24; ++step) {
        const render::Rect view{b.x + step * 300, b.y + step * 100, 1280, 720};
        poll_until([&] { frames.update_chunks(scene, view, 2); },
                   [&] { return std::get<2>(frames.update_chunks(scene, view, 2)) == 0; }, 30.0);
        largest = std::max(largest, frames.cached());
    }
    // visible (≤ 3 × 2) + ring (≤ 5 × 4) + neighbouring groups + 2 spares.
    CHECK(largest < 100);
    const render::Rect last{b.x + 23 * 300, b.y + 23 * 100, 1280, 720};
    CHECK(std::get<2>(frames.update_chunks(scene, last, 2)) == 0);
    // Destroying the workers with garbage queued is fine.
    frames.set_workers(nullptr);
}

// ---- The river flow on the GPU (docs/render/rivers.md 3.3, 8 and 16; stage S6) ----

TEST_CASE("the WGSL source declares the river bindings the compositor binds") {
    const std::string_view src = anno::render::shaders::compositor();
    auto has = [&](const std::string& text) {
        const bool found = src.find(text) != std::string_view::npos;
        CHECK_MESSAGE(found, "gpu.wgsl lacks: " << text);
        return found;
    };
    // Group 1: the three planes of the flow lattice, by the numbers gpu.cpp binds.
    has("@group(1) @binding(" + std::to_string(gpu::kFlowVelocityBinding) +
        ") var flow_uv: texture_2d<i32>;");
    has("@group(1) @binding(" + std::to_string(gpu::kFlowAuxBinding) +
        ") var flow_aux: texture_2d<u32>;");
    has("@group(1) @binding(" + std::to_string(gpu::kFlowShearBinding) +
        ") var flow_shear: texture_2d<u32>;");
    // Group 0: the layer table, and its size is iso::kMaxRiverLayers.
    has("@group(0) @binding(" + std::to_string(gpu::kRiverParamsBinding) +
        ") var<uniform> river: RiverParams;");
    has("const MAX_RIVER_LAYERS: u32 = " + std::to_string(render::kMaxRiverLayers) + "u;");
    has("layers: array<RiverLayer, " + std::to_string(render::kMaxRiverLayers) + ">,");
    // The chunk flags: bit 5 = lattice present, bits 8-15 the stride.
    has("(chunk.flags & " + std::to_string(gpu::kChunkFlagFlow) + "u) != 0u");
    has("(chunk.flags >> " + std::to_string(gpu::kChunkStrideShift) + "u) & 0xFFu");
    // Where the compositor puts the layer table is a binding of the frame group
    // (group 0), and the frame uniform keeps its size.
    CHECK(gpu::kRiverParamsBinding == 7);
    CHECK(gpu::kChunkFlagFlow == 32);
    // The tiers are the enumerators the shader's `quality` compares with.
    CHECK(static_cast<std::uint32_t>(render::RiverQuality::Low) == 0);
    CHECK(static_cast<std::uint32_t>(render::RiverQuality::Medium) == 1);
    CHECK(static_cast<std::uint32_t>(render::RiverQuality::High) == 2);
    has("river.quality != 2u");
    has("river.quality == 0u");
}

TEST_CASE("the river parameter uniform matches struct RiverParams in gpu.wgsl") {
    render::RiverParams p = render::river::RiverConfig{}.params();
    p.quality = render::RiverQuality::Medium;
    const auto b = gpu::river_params_bytes(p, 0.125F);
    REQUIRE(b.size() == gpu::kRiverParamsBytes);
    CHECK(b.size() == 32 + 8 * 32);
    auto u32_at = [&](std::size_t o) {
        return std::uint32_t{b[o]} | std::uint32_t{b[o + 1]} << 8 | std::uint32_t{b[o + 2]} << 16 |
               std::uint32_t{b[o + 3]} << 24;
    };
    auto f32_at = [&](std::size_t o) {
        const std::uint32_t v = u32_at(o);
        float x = 0;
        std::memcpy(&x, &v, 4);
        return x;
    };
    CHECK(u32_at(0) == p.count);
    CHECK(u32_at(0) == 4);
    CHECK(u32_at(4) == 1);
    CHECK(f32_at(8) == 0.125F);
    CHECK(f32_at(12) == p.rapids_base);
    CHECK(f32_at(16) == p.shear_from);
    CHECK(f32_at(20) == p.shear_to);
    CHECK(f32_at(24) == render::kLowTapGain);
    // The layers start at byte 32, 32 bytes each: scale, period, amplitude, fades, rapids.
    for (std::size_t i = 0; i < p.count; ++i) {
        const std::size_t o = 32 + 32 * i;
        CHECK(f32_at(o) == p.layers[i].scale);
        CHECK(f32_at(o + 4) == p.layers[i].period_ms);
        CHECK(f32_at(o + 8) == p.layers[i].amp);
        CHECK(f32_at(o + 12) == p.layers[i].fades);
        CHECK(f32_at(o + 16) == p.layers[i].rapids);
    }
    // The rapids layer is the last of the default table, the glints fade with the detail.
    CHECK(f32_at(32 + 32 * 3 + 16) == 1.0F);
    CHECK(f32_at(32 + 32 * 2 + 12) == 1.0F);
    CHECK(f32_at(32 + 32 * 0 + 12) == 0.0F);
    // Unused layers are zero.
    for (std::size_t o = 32 + 32 * 4; o < b.size(); ++o) {
        CHECK(b[o] == 0);
    }
    // More layers than the table holds are dropped, not written past it.
    render::RiverParams many = p;
    many.count = 100;
    CHECK(gpu::river_params_bytes(many, 0.0F).size() == gpu::kRiverParamsBytes);
    const auto bm = gpu::river_params_bytes(many, 0.0F);
    CHECK(std::uint32_t{bm[0]} == render::kMaxRiverLayers);
}

TEST_CASE("the automatic river tier is the highest one within the gate on what it adds to the "
          "legacy shimmer") {
    using Q = render::RiverQuality;
    auto costs = [](double legacy, double low, double medium, double high) {
        std::vector<gpu::RiverCost> c(4);
        c[0].quality = std::nullopt;
        c[0].ms_per_mpx = legacy;
        c[1].quality = Q::Low;
        c[1].ms_per_mpx = low;
        c[2].quality = Q::Medium;
        c[2].ms_per_mpx = medium;
        c[3].quality = Q::High;
        c[3].ms_per_mpx = high;
        return c;
    };
    // This machine's RTX 4090: everything is far inside the gate.
    CHECK(gpu::choose_river_quality(costs(0.040, 0.057, 0.066, 0.080), 1.0) == Q::High);
    // The Ryzen iGPU (Vulkan, measured 2026-10-19): even Low adds 2.3 ms, so the floor.
    CHECK(gpu::choose_river_quality(costs(8.11, 10.41, 12.14, 14.17), 1.0) == Q::Low);
    // The gate is on the cost ADDED to the legacy shimmer: a heavy baseline does not count.
    CHECK(gpu::choose_river_quality(costs(50.0, 50.2, 50.5, 50.9), 1.0) == Q::High);
    // Exactly the gate passes; a hair over does not.
    CHECK(gpu::choose_river_quality(costs(1.0, 1.4, 1.8, 2.0), 1.0) == Q::High);
    CHECK(gpu::choose_river_quality(costs(1.0, 1.4, 1.8, 2.01), 1.0) == Q::Medium);
    CHECK(gpu::choose_river_quality(costs(1.0, 1.4, 2.01, 2.5), 1.0) == Q::Low);
    // The highest tier within the gate wins, whatever the order of the list.
    auto shuffled = costs(1.0, 1.4, 1.8, 3.0);
    std::reverse(shuffled.begin(), shuffled.end());
    CHECK(gpu::choose_river_quality(shuffled, 1.0) == Q::Medium);
    // No measurements: the floor (nothing is known to fit).
    CHECK(gpu::choose_river_quality({}, 1.0) == Q::Low);
    // A driver that reports no time gives zeros: unmeasured is never "free". The
    // reference unmeasured: nothing qualifies, Low (not the dearest tier).
    CHECK(gpu::choose_river_quality(costs(0.0, 0.0, 0.0, 0.0), 1.0) == Q::Low);
    CHECK(gpu::choose_river_quality(costs(0.0, 0.5, 0.6, 0.7), 1.0) == Q::Low);
    CHECK(gpu::choose_river_quality(costs(-1.0, 0.5, 0.6, 0.7), 1.0) == Q::Low);
    // A tier unmeasured while the reference is fine is skipped, not chosen.
    CHECK(gpu::choose_river_quality(costs(1.0, 1.4, 1.8, 0.0), 1.0) == Q::Medium);
    CHECK(gpu::choose_river_quality(costs(1.0, 0.0, 0.0, 0.0), 1.0) == Q::Low);
    CHECK(gpu::choose_river_quality(costs(1.0, 1.4, 1.8, std::nan("")), 1.0) == Q::Medium);
    CHECK(gpu::choose_river_quality(costs(1.0, 1.4, 1.8, std::numeric_limits<double>::infinity()),
                                    1.0) == Q::Medium);
    // The measured flag itself.
    gpu::RiverCost rc;
    CHECK_FALSE(rc.measured());
    rc.ms_per_mpx = 0.001;
    CHECK(rc.measured());
}

TEST_CASE("ANNO_GPU_ADAPTER picks an adapter by index or by a part of the name (no device)") {
    auto adapter = [](const char* name) {
        gpu::AdapterInfo a;
        a.name = name;
        return a;
    };
    const std::vector<gpu::AdapterInfo> list = {
        adapter("NVIDIA GeForce RTX 4090"), adapter("AMD Radeon(TM) Graphics"),
        adapter("NVIDIA GeForce RTX 4090"), adapter("Microsoft Basic Render Driver")};
    CHECK(gpu::pick_adapter("0", list) == 0);
    CHECK(gpu::pick_adapter("3", list) == 3);
    CHECK(gpu::pick_adapter("4", list) == std::nullopt); // past the end
    CHECK(gpu::pick_adapter("radeon", list) == 1);       // case-insensitive part of the name
    CHECK(gpu::pick_adapter("RTX", list) == 0);          // the first match
    CHECK(gpu::pick_adapter("basic render", list) == 3);
    CHECK(gpu::pick_adapter("intel", list) == std::nullopt);
    CHECK(gpu::pick_adapter("", list) == std::nullopt);
    CHECK(gpu::pick_adapter("0", {}) == std::nullopt);
    // A number too large to parse is "no match", not an exception.
    CHECK(gpu::pick_adapter("99999999999999999999", list) == std::nullopt);
    CHECK(gpu::pick_adapter("18446744073709551616", list) == std::nullopt);
    // All digits is an index, even where it could be part of a name ("4090" of an RTX 4090);
    // digits mixed with letters are a name.
    CHECK(gpu::pick_adapter("4090", list) == std::nullopt);
    CHECK(gpu::pick_adapter("rtx 4090", list) == 0);
    CHECK(gpu::pick_adapter("1x", list) == std::nullopt);
}

namespace {

/// A scene without islands (the synthetic Anlagen.dat and the default ground
/// textures): everything a compositor frame needs except content, which the
/// tests inject as chunks.
struct EmptyScene {
    std::vector<std::uint8_t> anl_buf;
    std::unique_ptr<render::Scene> scene;
};

/// `light_gain` multiplies the ripple light of the river flow shading (the
/// default 1 keeps the calibrated +-2 % that is a level or two of 255; the tests
/// use 10, a +-20 % light, so that a wrong flow direction or a wrong phase shows).
EmptyScene empty_scene(float light_gain = 10.0F) {
    EmptyScene out;
    out.anl_buf = anno::test::read_file(std::filesystem::path(ANNO_FIXTURES_DIR) /
                                        "anlagen_synthetic" / "Anlagen.dat");
    auto anlagen = anno::formats::anlagen::AnlagenFile::parse(out.anl_buf);
    REQUIRE(anlagen);
    out.scene = std::make_unique<render::Scene>(
        "synthetic", std::vector<anno::formats::insel::Island>{},
        anno::assets::island::GroundTextures{}, std::move(anlagen).value(), render::kHeightLift);
    out.scene->river_cfg.light_gain = light_gain;
    return out;
}

/// What the synthetic chunk is made of: pure river pixels (no sea cover, far
/// from any shore) whose depth varies from 0.5 to 4 HEIGHT units (the deep-water
/// mix of the sea stays off), a band of rapids, and a flow lattice that varies
/// everywhere: direction, speed, level, shear, turbulence, a hole without flow,
/// a rim with half weight, and texels with a weight just below and just above
/// RiverConfig::min_valid (0.02) so that the legacy fallback threshold is pinned.
/// The lattice is one GLOBAL function of the texel (chunk key (cx, cy) shifts the
/// texel index by the chunk's size in texels), so neighbouring chunks agree on
/// their shared apron texels exactly, as the real bake does.
struct SyntheticRiver {
    render::Chunk chunk;
    std::uint32_t stride = 4;
};

SyntheticRiver synthetic_river(std::uint32_t stride, bool with_flow, int cx = 0, int cy = 0) {
    SyntheticRiver out;
    out.stride = stride;
    render::Chunk& c = out.chunk;
    const auto n = static_cast<std::size_t>(render::kChunk);
    c.field.resize(n * n * 4);
    for (std::size_t y = 0; y < n; ++y) {
        for (std::size_t x = 0; x < n; ++x) {
            std::uint8_t* f = &c.field[(y * n + x) * 4];
            f[0] = 1;                                             // cover
            f[1] = 255;                                           // shore: far
            f[2] = static_cast<std::uint8_t>(130 + (x * 7) % 15); // depth 0.5 .. 4 units
            // river flag, slope 15 units/tile in a band (full rapids), else 0.
            f[3] = static_cast<std::uint8_t>((y >= 300 && y < 360) ? (2 | (30 << 2)) : 2);
        }
    }
    if (!with_flow) {
        return out;
    }
    render::river::FlowPlane& fp = c.flow;
    fp.x0 = cx * render::kChunk;
    fp.y0 = cy * render::kChunk;
    fp.stride_px = stride;
    fp.nx = fp.ny = static_cast<std::uint32_t>(render::kChunk) / stride + 1;
    const std::size_t count = std::size_t{fp.nx} * fp.ny;
    fp.uv.assign(count * 2, 0);
    fp.aux.assign(count * 4, 0);
    fp.shear.assign(count, 0);
    // The chunk's texel origin in the global lattice.
    const std::uint32_t per_chunk = static_cast<std::uint32_t>(render::kChunk) / stride;
    const std::uint32_t gx0 = static_cast<std::uint32_t>(cx) * per_chunk,
                        gy0 = static_cast<std::uint32_t>(cy) * per_chunk;
    for (std::uint32_t j = 0; j < fp.ny; ++j) {
        for (std::uint32_t i = 0; i < fp.nx; ++i) {
            const std::uint32_t gi = gx0 + i, gj = gy0 + j;
            const float a =
                0.045F * static_cast<float>(gi) + 0.031F * static_cast<float>(gj) + 0.7F;
            const float speed = 0.12F + 0.85F * static_cast<float>(gi % 29) / 29.0F;
            float valid = 1.0F;
            const std::uint32_t k = 40 / (stride / 2 > 0 ? stride / 2 : 1);
            if (gi >= k && gi < k + 10 && gj >= k && gj < k + 10) {
                valid = 0.0F; // a hole: the legacy shimmer shows there
            } else if (gi >= k - 1 && gi < k + 11 && gj >= k - 1 && gj < k + 11) {
                valid = 0.5F; // a rim
            } else if (gj >= k && gj < k + 10 && gi >= k + 13 && gi < k + 15) {
                valid = 0.01F; // below min_valid: no flow here, the weight fades in around it
            } else if (gj >= k && gj < k + 10 && gi >= k + 15 && gi < k + 17) {
                valid = 0.03F; // just above min_valid
            }
            render::river::set_texel(fp, i, j, speed * std::cos(a), speed * std::sin(a), valid,
                                     0.1F + 0.4F * static_cast<float>(gi % 3) / 3.0F, 0.0F,
                                     32.0F + 2.0F * static_cast<float>(gi % 8), 8.0F,
                                     0.05F * static_cast<float>(gj % 16));
        }
    }
    return out;
}

struct RiverCompare {
    std::size_t n = 0, over2 = 0;
    int max = 0;
    [[nodiscard]] double within2() const {
        return n == 0 ? 0.0 : 1.0 - static_cast<double>(over2) / static_cast<double>(n);
    }
};

/// The pixel the CPU path gives a pure river pixel of the synthetic chunk at
/// scene pixel (sx, sy): the production shading (sea::river_pixel_shade, the
/// flow lattice of the chunk, the legacy shimmer where it has no flow) applied
/// to the open-water colour, with the waterline alpha. `flip` negates the
/// velocity (the negative control).
render::Px expected_river_pixel(const render::Scene& scene, const render::Chunk& chunk,
                                std::size_t group, int sx, int sy, std::uint64_t time_ms,
                                const render::RiverParams& params, bool flow_mode, bool flip) {
    const auto n = static_cast<std::size_t>(render::kChunk);
    const std::size_t i =
        (static_cast<std::size_t>(sy) % n * n + static_cast<std::size_t>(sx) % n) * 4;
    const std::uint8_t depth = chunk.field[i + 2], flags = chunk.field[i + 3];
    const float sd =
        (static_cast<float>(depth) - render::sea::kDepthBias) / render::sea::kDepthScale;
    const float twf = static_cast<float>(render::kTile[group].w),
                thf = static_cast<float>(render::kTile[group].h);
    const int raise = scene.lift.scaled(render::kSeaLevel, 1, group);
    const float px = static_cast<float>(sx), py = static_cast<float>(sy + raise);
    const std::pair<float, float> pos{px / twf + py / thf, py / thf - px / twf};
    render::FlowSample fs = render::river::flow_at(chunk.flow, sx, sy);
    if (flip) {
        fs.vx = -fs.vx;
        fs.vy = -fs.vy;
    }
    const render::sea::RiverShading shading{&params, scene.river_cfg.min_valid};
    const render::Shade sh = render::sea::river_pixel_shade(
        flow_mode && !chunk.flow.empty() ? &shading : nullptr, fs, sx, sy, sd,
        render::sea::slope_of(flags), time_ms, render::wave_detail(twf),
        render::sea::river_view(group, scene.lift), pos);
    return render::shade_apply(scene.textures.water(), sh);
}

/// GPU pixels of the 512 x 512 chunk at the origin against the CPU's
/// expectation, on every third pixel.
RiverCompare compare_river_chunk(const render::Scene& scene, const render::Chunk& chunk,
                                 std::size_t group, std::uint64_t time_ms,
                                 const render::RiverParams& params, bool flow_mode,
                                 const std::vector<std::uint8_t>& gpu_px, bool flip = false) {
    RiverCompare r;
    const auto n = static_cast<std::size_t>(render::kChunk);
    for (std::size_t y = 0; y < n; y += 3) {
        for (std::size_t x = 0; x < n; x += 3) {
            const render::Px e =
                expected_river_pixel(scene, chunk, group, static_cast<int>(x), static_cast<int>(y),
                                     time_ms, params, flow_mode, flip);
            int m = 0;
            for (std::size_t k = 0; k < 3; ++k) {
                m = std::max(m, std::abs(int{e[k]} - int{gpu_px[(y * n + x) * 4 + k]}));
            }
            ++r.n;
            r.over2 += m > 2 ? 1U : 0U;
            r.max = std::max(r.max, m);
        }
    }
    return r;
}

} // namespace

TEST_CASE("river flow on the GPU: the shader draws what the CPU twin draws, at every zoom group, "
          "tier and time (synthetic)") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    constexpr int kSide = render::kChunk;
    const render::Rect view{0, 0, kSide, kSide};
    for (const render::RiverQuality q :
         {render::RiverQuality::High, render::RiverQuality::Medium, render::RiverQuality::Low}) {
        gpu::GpuLimits limits;
        limits.uploads_per_frame = SIZE_MAX;
        limits.wave_simulation = false;
        limits.river_quality = q;
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
        REQUIRE_MESSAGE(made, (made ? "" : made.error()));
        gpu::GpuFrame& g = *made.value();
        CHECK(g.river_quality() == q);
        const auto target = g.offscreen_target(kSide, kSide);
        for (const std::size_t group : {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
            // The lattice stride of the group (rivers.md 3.3: 8 / 4 / 4 px).
            const std::uint32_t stride = scene.river_cfg.lattice_stride[group];
            const SyntheticRiver syn = synthetic_river(stride, true);
            render::FrameRenderer frames(1, false);
            frames.insert_chunk({group, 0, 0}, syn.chunk);
            render::RiverParams params = scene.river_cfg.params();
            params.quality = q;
            for (const std::uint64_t t : {std::uint64_t{0}, std::uint64_t{700}, std::uint64_t{1599},
                                          std::uint64_t{3599950}, std::uint64_t{3600700}}) {
                const auto st = g.render(frames, scene, view, group, t,
                                         render::WaterStyle::Enhanced, target.second.get());
                CHECK(st.river_chunks == 1);
                CHECK(st.river_quality == q);
                const auto px = g.read_back(target.first.get(), kSide, kSide);
                REQUIRE(px.size() == std::size_t{kSide} * kSide * 4);
                const RiverCompare r =
                    compare_river_chunk(scene, syn.chunk, group, t, params, true, px);
                MESSAGE("tier " << static_cast<int>(q) << " group " << group << " t " << t << ": "
                                << r.n << " pixels, " << 100.0 * r.within2()
                                << " % within 2/255, max " << r.max);
                // Rounding of the 8-bit colour and the GPU's own arithmetic: the pixels
                // agree within 2/255 on >= 99.9 %, none beyond 4 (rivers.md Q2).
                CHECK(r.within2() >= 0.999);
                CHECK(r.max <= 4);
                // Negative control: with the flow reversed the CPU's expectation is
                // another picture, so the check above would fail on a wrong direction.
                if (t == 700) {
                    const RiverCompare flipped =
                        compare_river_chunk(scene, syn.chunk, group, t, params, true, px, true);
                    CHECK(flipped.within2() < 0.6);
                }
            }
        }
        CHECK(g.errors() == 0);
    }
}

TEST_CASE("river flow on the GPU: every pixel samples the same global lattice, across chunks at "
          "non-zero keys and over their borders (synthetic)") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    constexpr int kN = render::kChunk;
    constexpr int kSide = 2 * kN;
    const render::Rect view{0, 0, kSide, kSide};
    for (const render::RiverQuality q : {render::RiverQuality::High, render::RiverQuality::Low}) {
        gpu::GpuLimits limits;
        limits.uploads_per_frame = SIZE_MAX;
        limits.wave_simulation = false;
        limits.river_quality = q;
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
        REQUIRE_MESSAGE(made, (made ? "" : made.error()));
        gpu::GpuFrame& g = *made.value();
        const auto target = g.offscreen_target(kSide, kSide);
        render::RiverParams params = scene.river_cfg.params();
        params.quality = q;
        for (const std::size_t group : {std::size_t{0}, std::size_t{1}}) {
            const std::uint32_t stride = scene.river_cfg.lattice_stride[group];
            // Four chunks: (0,0) and the three at non-zero keys (1,0), (0,1), (1,1).
            std::vector<SyntheticRiver> chunks;
            render::FrameRenderer frames(1, false);
            for (int cy = 0; cy < 2; ++cy) {
                for (int cx = 0; cx < 2; ++cx) {
                    chunks.push_back(synthetic_river(stride, true, cx, cy));
                    frames.insert_chunk({group, cx, cy}, chunks.back().chunk);
                }
            }
            for (const std::uint64_t t : {std::uint64_t{700}, std::uint64_t{1599}}) {
                const auto st = g.render(frames, scene, view, group, t,
                                         render::WaterStyle::Enhanced, target.second.get());
                CHECK(st.river_chunks == 4);
                const auto px = g.read_back(target.first.get(), kSide, kSide);
                REQUIRE(px.size() == std::size_t{kSide} * kSide * 4);
                // Every third pixel, plus every pixel within 3 of a chunk border.
                std::size_t n = 0, over2 = 0, border_n = 0, border_over2 = 0;
                int max_diff = 0;
                for (int y = 0; y < kSide; ++y) {
                    for (int x = 0; x < kSide; ++x) {
                        const bool border = std::abs(x - kN) <= 3 || std::abs(y - kN) <= 3;
                        if (!border && (x % 3 != 0 || y % 3 != 0)) {
                            continue;
                        }
                        const SyntheticRiver& owner =
                            chunks[static_cast<std::size_t>((y / kN) * 2 + x / kN)];
                        const render::Px e = expected_river_pixel(scene, owner.chunk, group, x, y,
                                                                  t, params, true, false);
                        int m = 0;
                        for (std::size_t k = 0; k < 3; ++k) {
                            m = std::max(
                                m,
                                std::abs(int{e[k]} -
                                         int{px[(std::size_t{static_cast<std::size_t>(y)} * kSide +
                                                 static_cast<std::size_t>(x)) *
                                                    4 +
                                                k]}));
                        }
                        ++n;
                        over2 += m > 2 ? 1U : 0U;
                        max_diff = std::max(max_diff, m);
                        if (border) {
                            ++border_n;
                            border_over2 += m > 2 ? 1U : 0U;
                        }
                    }
                }
                MESSAGE("tier " << static_cast<int>(q) << " group " << group << " t " << t << ": "
                                << n << " pixels, " << over2 << " beyond 2/255 (" << border_over2
                                << " of " << border_n << " at the borders), max " << max_diff);
                CHECK(n > 0);
                CHECK(static_cast<double>(over2) <= 0.001 * static_cast<double>(n));
                // The borders are no different from the middle of a chunk.
                CHECK(static_cast<double>(border_over2) <= 0.001 * static_cast<double>(border_n));
                CHECK(max_diff <= 4);
            }
        }
        CHECK(g.errors() == 0);
    }
}

TEST_CASE("river flow on the GPU: the time wraps at the hour, and a very late time looks like the "
          "same time into the hour (synthetic)") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    constexpr int kSide = render::kChunk;
    const render::Rect view{0, 0, kSide, kSide};
    gpu::GpuLimits limits;
    limits.uploads_per_frame = SIZE_MAX;
    limits.wave_simulation = false;
    limits.river_quality = render::RiverQuality::High;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    gpu::GpuFrame& g = *made.value();
    const auto target = g.offscreen_target(kSide, kSide);
    const std::size_t group = 1;
    const SyntheticRiver syn = synthetic_river(scene.river_cfg.lattice_stride[group], true);
    render::FrameRenderer frames(1, false);
    frames.insert_chunk({group, 0, 0}, syn.chunk);
    auto image_at = [&](std::uint64_t t) {
        g.render(frames, scene, view, group, t, render::WaterStyle::Enhanced, target.second.get());
        return g.read_back(target.first.get(), kSide, kSide);
    };
    constexpr std::uint64_t kHour = 3600000;
    for (const std::uint64_t t : {std::uint64_t{777}, std::uint64_t{1599}}) {
        const auto base = image_at(t);
        REQUIRE(base.size() == std::size_t{kSide} * kSide * 4);
        // The same instant one hour, two hours and a hundred hours later: the shader
        // gets time % 1 hour, so the picture is byte for byte the same. Without the
        // wrap a time of 360,000,777 ms is rounded to a multiple of 32 ms in f32 and
        // the layers (periods 640-2400 ms) jump.
        for (const std::uint64_t hours : {std::uint64_t{1}, std::uint64_t{2}, std::uint64_t{100}}) {
            CHECK(image_at(t + hours * kHour) == base);
        }
        // And a different instant is a different picture (the check above is not vacuous).
        CHECK(image_at(t + 321) != base);
    }
    CHECK(g.errors() == 0);
}

TEST_CASE("river flow on the GPU: the legacy look is kept without flow, in legacy mode and in "
          "original water (synthetic)") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    constexpr int kSide = render::kChunk;
    const render::Rect view{0, 0, kSide, kSide};
    gpu::GpuLimits limits;
    limits.uploads_per_frame = SIZE_MAX;
    limits.wave_simulation = false;
    limits.river_quality = render::RiverQuality::High;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    gpu::GpuFrame& g = *made.value();
    const auto target = g.offscreen_target(kSide, kSide);
    const std::size_t group = 1;
    const render::RiverParams params = scene.river_cfg.params();
    // The lattice is there, the mode says legacy: every pixel is the shimmer.
    {
        const SyntheticRiver syn = synthetic_river(4, true);
        render::FrameRenderer frames(1, false);
        frames.river_flow = render::RiverFlowMode::Legacy;
        frames.insert_chunk({group, 0, 0}, syn.chunk);
        g.render(frames, scene, view, group, 900, render::WaterStyle::Enhanced,
                 target.second.get());
        const auto px = g.read_back(target.first.get(), kSide, kSide);
        const RiverCompare r = compare_river_chunk(scene, syn.chunk, group, 900, params, false, px);
        CHECK(r.within2() >= 0.999);
        CHECK(r.max <= 4);
        // (The shimmer's amplitude is pinned on real rivers, where it spans several levels:
        // the Low and Medium test below; here it is a fraction of a level.)
        // ... and it is not the flow look.
        const RiverCompare as_flow =
            compare_river_chunk(scene, syn.chunk, group, 900, params, true, px);
        CHECK(as_flow.within2() < 0.9);
    }
    // No lattice in the chunk (a chunk built before its flow): the shimmer in flow mode too.
    {
        const SyntheticRiver syn = synthetic_river(4, false);
        render::FrameRenderer frames(1, false);
        frames.insert_chunk({group, 0, 0}, syn.chunk);
        const auto st = g.render(frames, scene, view, group, 900, render::WaterStyle::Enhanced,
                                 target.second.get());
        CHECK(st.river_chunks == 0);
        const auto px = g.read_back(target.first.get(), kSide, kSide);
        const RiverCompare r = compare_river_chunk(scene, syn.chunk, group, 900, params, true, px);
        CHECK(r.within2() >= 0.999);
        CHECK(r.max <= 4);
    }
    // Original water ignores rivers and the river mode: the same image either way.
    {
        const SyntheticRiver syn = synthetic_river(4, true);
        render::FrameRenderer flow_frames(1, false), legacy_frames(1, false);
        legacy_frames.river_flow = render::RiverFlowMode::Legacy;
        flow_frames.insert_chunk({group, 0, 0}, syn.chunk);
        legacy_frames.insert_chunk({group, 0, 0}, syn.chunk);
        g.render(flow_frames, scene, view, group, 900, render::WaterStyle::Original,
                 target.second.get());
        const auto a = g.read_back(target.first.get(), kSide, kSide);
        g.render(legacy_frames, scene, view, group, 900, render::WaterStyle::Original,
                 target.second.get());
        const auto b = g.read_back(target.first.get(), kSide, kSide);
        CHECK(a == b);
        // Rivers are not water in the original (no sprites here): open water colour.
        const render::Px water = scene.textures.water();
        for (std::size_t i = 0; i < a.size(); i += 4 * 977) {
            CHECK(std::abs(int{a[i]} - int{water[0]}) <= 1);
            CHECK(std::abs(int{a[i + 1]} - int{water[1]}) <= 1);
            CHECK(std::abs(int{a[i + 2]} - int{water[2]}) <= 1);
        }
    }
    CHECK(g.errors() == 0);
}

TEST_CASE("river flow on the GPU: a chunk rebuilt with its flow is uploaded again, the textures of "
          "the replaced one are reused (synthetic)") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    constexpr int kSide = render::kChunk;
    const render::Rect view{0, 0, kSide, kSide};
    gpu::GpuLimits limits;
    limits.uploads_per_frame = SIZE_MAX;
    limits.wave_simulation = false;
    limits.river_quality = render::RiverQuality::High;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    gpu::GpuFrame& g = *made.value();
    const auto target = g.offscreen_target(kSide, kSide);
    const std::size_t group = 1;
    render::FrameRenderer frames(1, false);
    // Built before its flow: the legacy shimmer.
    frames.insert_chunk({group, 0, 0}, synthetic_river(4, false).chunk);
    auto st = g.render(frames, scene, view, group, 900, render::WaterStyle::Enhanced,
                       target.second.get());
    CHECK(st.chunks_uploaded == 1);
    CHECK(st.river_chunks == 0);
    const auto before = g.read_back(target.first.get(), kSide, kSide);
    // The same frame again: nothing changed, nothing is uploaded.
    st = g.render(frames, scene, view, group, 900, render::WaterStyle::Enhanced,
                  target.second.get());
    CHECK(st.chunks_uploaded == 0);
    CHECK(g.read_back(target.first.get(), kSide, kSide) == before);
    // Rebuilt with the flow (a new serial under the same key): uploaded again, and shown.
    const SyntheticRiver rebuilt = synthetic_river(4, true);
    const std::uint64_t serial_before = frames.chunk({group, 0, 0})->serial;
    frames.insert_chunk({group, 0, 0}, rebuilt.chunk);
    CHECK(frames.chunk({group, 0, 0})->serial != serial_before);
    st = g.render(frames, scene, view, group, 900, render::WaterStyle::Enhanced,
                  target.second.get());
    CHECK(st.chunks_uploaded == 1);
    CHECK(st.river_chunks == 1);
    const auto after = g.read_back(target.first.get(), kSide, kSide);
    CHECK(after != before);
    const RiverCompare r = compare_river_chunk(scene, rebuilt.chunk, group, 900,
                                               scene.river_cfg.params(), true, after);
    CHECK(r.within2() >= 0.999);
    // And back: a rebuild without a lattice drops it again.
    frames.insert_chunk({group, 0, 0}, synthetic_river(4, false).chunk);
    st = g.render(frames, scene, view, group, 900, render::WaterStyle::Enhanced,
                  target.second.get());
    CHECK(st.chunks_uploaded == 1);
    CHECK(st.river_chunks == 0);
    CHECK(g.read_back(target.first.get(), kSide, kSide) == before);
    // Flow lattices of different sizes (strides 8 and 2) use their own textures.
    for (const std::uint32_t stride : {8U, 2U, 4U}) {
        frames.insert_chunk({group, 0, 0}, synthetic_river(stride, true).chunk);
        st = g.render(frames, scene, view, group, 900, render::WaterStyle::Enhanced,
                      target.second.get());
        CHECK(st.river_chunks == 1);
    }
    // A lattice that does not belong to the chunk (another corner) is left out, not misread.
    {
        SyntheticRiver wrong = synthetic_river(4, true);
        wrong.chunk.flow.x0 += 512;
        frames.insert_chunk({group, 0, 0}, wrong.chunk);
        st = g.render(frames, scene, view, group, 900, render::WaterStyle::Enhanced,
                      target.second.get());
        CHECK(st.river_chunks == 0);
        // Nor one whose planes are short.
        SyntheticRiver shorter = synthetic_river(4, true);
        shorter.chunk.flow.aux.pop_back();
        frames.insert_chunk({group, 0, 0}, shorter.chunk);
        st = g.render(frames, scene, view, group, 900, render::WaterStyle::Enhanced,
                      target.second.get());
        CHECK(st.river_chunks == 0);
    }
    CHECK(g.errors() == 0);
}

TEST_CASE("river flow on the GPU: the cost is measured per tier, GPU timestamps when the device "
          "has them, and the automatic tier follows the adapter") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    MESSAGE("adapter: " << ctx->name << ", timestamps "
                        << std::string(ctx->timestamps ? "yes" : "no"));
    gpu::GpuLimits fixed;
    fixed.river_quality = render::RiverQuality::High;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, fixed);
    REQUIRE(made);
    const auto costs = made.value()->measure_river_cost(3);
    REQUIRE(costs.size() == 4);
    CHECK_FALSE(costs[0].quality.has_value());
    CHECK(costs[1].quality == render::RiverQuality::Low);
    CHECK(costs[2].quality == render::RiverQuality::Medium);
    CHECK(costs[3].quality == render::RiverQuality::High);
    for (const gpu::RiverCost& c : costs) {
        MESSAGE((c.quality ? static_cast<int>(*c.quality) : -1)
                << ": " << c.ms << " ms for " << c.megapixels << " Mpx = " << c.ms_per_mpx
                << " ms per Mpx");
        CHECK(std::isfinite(c.ms));
        CHECK(c.ms > 0.0);
        CHECK(c.megapixels == doctest::Approx(1.048576));
        CHECK(c.ms_per_mpx == doctest::Approx(c.ms / c.megapixels));
        CHECK(c.gpu_timestamps == ctx->timestamps);
    }
    // With a gate only what the choice needs is measured: the dearest tier first, stopping
    // at the first that fits, and no Low after a failed Medium (the floor).
    {
        const auto fits = made.value()->measure_river_cost(2, 1.0e9);
        REQUIRE(fits.size() == 2);
        CHECK_FALSE(fits[0].quality.has_value());
        CHECK(fits[1].quality == render::RiverQuality::High);
        // A gate no measurement can meet. The cost is the difference to the legacy
        // shimmer, so on a slow, noisy device (the macOS CI runner: ~18 ms per Mpx)
        // a tier can measure more than 1 ms per Mpx *below* the baseline and meet
        // a gate of -1; the choice then rightly stops early.
        const auto none = made.value()->measure_river_cost(2, -1.0e9);
        REQUIRE(none.size() == 3);
        CHECK(none[1].quality == render::RiverQuality::High);
        CHECK(none[2].quality == render::RiverQuality::Medium);
    }
    // The wall-clock budget: a spent budget drops the tier in progress and measures no
    // more, and what is left never makes a tier look free (the choice is the floor).
    {
        const auto cut = made.value()->measure_river_cost(3, 1.0, 1.0e-9);
        CHECK(cut.empty());
        CHECK(gpu::choose_river_quality(cut, 1.0) == render::RiverQuality::Low);
        const auto fine = made.value()->measure_river_cost(2, 1.0e9, 1.0e9);
        CHECK(fine.size() == 2);
        for (const gpu::RiverCost& c : fine) {
            CHECK(c.measured());
        }
    }
    // A given tier is kept, and nothing is measured for it.
    CHECK(made.value()->river_quality() == render::RiverQuality::High);
    CHECK(made.value()->river_costs().empty());
    // The automatic choice: discrete adapters High without measuring, software adapters
    // Low without measuring, the rest by the gate on the measurements it keeps.
    auto autoq = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, {});
    REQUIRE(autoq);
    if (ctx->adapter_type == WGPUAdapterType_DiscreteGPU) {
        CHECK(autoq.value()->river_quality() == render::RiverQuality::High);
        CHECK(autoq.value()->river_costs().empty());
    } else if (ctx->adapter_type == WGPUAdapterType_CPU) {
        CHECK(autoq.value()->river_quality() == render::RiverQuality::Low);
        CHECK(autoq.value()->river_costs().empty());
    } else {
        // The legacy shimmer and at least one tier; the choice is the one the gate gives.
        CHECK(autoq.value()->river_costs().size() >= 2);
        CHECK(autoq.value()->river_costs().size() <= 4);
        CHECK_FALSE(autoq.value()->river_costs().front().quality.has_value());
        CHECK(autoq.value()->river_quality() ==
              gpu::choose_river_quality(autoq.value()->river_costs(), 1.0));
    }
    // A gate of zero and a huge one on a measured adapter: Low and High.
    if (ctx->adapter_type != WGPUAdapterType_DiscreteGPU &&
        ctx->adapter_type != WGPUAdapterType_CPU) {
        gpu::GpuLimits tight, loose;
        tight.river_gate_ms_per_mpx = -1.0;
        loose.river_gate_ms_per_mpx = 1.0e9;
        auto a = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, tight);
        auto b = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, loose);
        REQUIRE(a);
        REQUIRE(b);
        CHECK(a.value()->river_quality() == render::RiverQuality::Low);
        CHECK(b.value()->river_quality() == render::RiverQuality::High);
    }
    CHECK(made.value()->errors() == 0);
}

TEST_CASE("the sea pass is timed with GPU timestamps when asked and the device can") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    gpu::GpuLimits limits;
    limits.river_quality = render::RiverQuality::High;
    limits.uploads_per_frame = SIZE_MAX;
    limits.wave_simulation = false;
    limits.gpu_timing = true;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    gpu::GpuFrame& g = *made.value();
    const auto target = g.offscreen_target(512, 512);
    render::FrameRenderer frames(1, false);
    frames.insert_chunk({1, 0, 0}, synthetic_river(4, true).chunk);
    // Nothing rendered yet.
    CHECK_FALSE(g.sea_pass_ms().has_value());
    g.render(frames, *es.scene, {0, 0, 512, 512}, 1, 100, render::WaterStyle::Enhanced,
             target.second.get());
    // The first frame of a device may report no time (a driver that has not
    // written its first timestamps yet: the AMD Vulkan driver gave 0); from the
    // second frame on there is one.
    const auto first = g.sea_pass_ms();
    if (first) {
        CHECK(*first > 0.0);
    }
    std::optional<double> ms;
    for (std::uint64_t i = 0; i < 3; ++i) {
        g.render(frames, *es.scene, {0, 0, 512, 512}, 1, 100 + 16 * i, render::WaterStyle::Enhanced,
                 target.second.get());
        ms = g.sea_pass_ms();
    }
    if (ctx->timestamps) {
        REQUIRE(ms.has_value());
        CHECK(*ms > 0.0);
        CHECK(*ms < 1000.0);
        // Read once: asking again without a new frame gives nothing.
        CHECK_FALSE(g.sea_pass_ms().has_value());
    } else {
        CHECK_FALSE(ms.has_value());
    }
    // Without the option no timestamps are taken.
    gpu::GpuLimits off = limits;
    off.gpu_timing = false;
    auto made2 = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, off);
    REQUIRE(made2);
    const auto t2 = made2.value()->offscreen_target(512, 512);
    made2.value()->render(frames, *es.scene, {0, 0, 512, 512}, 1, 100, render::WaterStyle::Enhanced,
                          t2.second.get());
    CHECK_FALSE(made2.value()->sea_pass_ms().has_value());
    CHECK(g.errors() == 0);
}

// ---- Q2 of rivers.md: the river pixels of real scenes, GPU against CPU (game files) ----

namespace {

/// A scene of one island file (Islands/<climate>/<name>.scp) without units.
Loaded load_island_scene(const std::filesystem::path& dir, const std::string& rel) {
    Loaded out;
    const auto felder_buf = anno::test::read_file(dir / "Textures" / "Felder.tex");
    out.anl_buf = anno::test::read_file(dir / "Anlagen.dat");
    out.buf = anno::test::read_file(dir / "Scenes" / rel);
    auto anlagen = anno::formats::anlagen::AnlagenFile::parse(out.anl_buf).value();
    const auto felder = anno::formats::tex::TexFile::parse(felder_buf).value();
    auto textures = anno::assets::island::GroundTextures::load(felder).value();
    if (const auto c = render::water_colour(anlagen)) {
        textures.set_water(*c);
    }
    out.scene = std::make_unique<render::Scene>("", anno::formats::insel::islands(out.buf).value(),
                                                std::move(textures), std::move(anlagen),
                                                render::kHeightLift);
    return out;
}

struct RiverQ2 {
    /// Pure river pixels (no sea blend, none under an object): the whole pixel,
    /// and what the flow shading adds on each path (flow minus legacy). `legacy` is
    /// the whole pixel in the legacy shimmer look (the R key, chunks without flow).
    /// `shading_under_objects` is the flow shading alone over the river pixels with
    /// an object over them, which `raw` leaves out (the GPU sinks objects standing in
    /// water, the CPU does not; the water under them is shaded the same on both).
    RiverCompare raw, shading, legacy, shading_under_objects;
    std::size_t under_objects = 0, blend_zone = 0, chunks_with_flow = 0;
    /// Standard deviation of the 800 ms change of the legacy shimmer, GPU and CPU
    /// (levels; only with river_q2's legacy_change).
    double legacy_sd_gpu = 0, legacy_sd_cpu = 0;
};

/// Renders `view` on the GPU and on the CPU in the flow look and in the legacy
/// look (the same time) and compares the river pixels of the sea field.
RiverQ2 river_q2(const render::Scene& scene, gpu::GpuFrame& g, render::FrameRenderer& gpu_frames,
                 render::FrameRenderer& cpu, WGPUTexture tex, WGPUTextureView tex_view,
                 render::Rect view, std::size_t group, std::uint64_t time,
                 bool legacy_change = false) {
    const auto w = static_cast<std::uint32_t>(view.w), h = static_cast<std::uint32_t>(view.h);
    auto render_both = [&](render::RiverFlowMode mode, std::vector<std::uint32_t>& cpu_px,
                           std::vector<std::uint8_t>& gpu_px, std::uint64_t at = 0) {
        const std::uint64_t time_ = at != 0 ? at : time;
        cpu.river_flow = mode;
        gpu_frames.river_flow = mode;
        cpu_px.assign(std::size_t{w} * h, 0);
        cpu.render(scene, view, group, time_, render::WaterStyle::Enhanced, cpu_px.data());
        g.render(gpu_frames, scene, view, group, time_, render::WaterStyle::Enhanced, tex_view);
        gpu_px = g.read_back(tex, w, h);
    };
    std::vector<std::uint32_t> cpu_flow, cpu_legacy;
    std::vector<std::uint8_t> gpu_flow, gpu_legacy;
    // Every chunk of the view on the GPU (uploads are spread over frames).
    for (int i = 0; i < 3; ++i) {
        g.render(gpu_frames, scene, view, group, time, render::WaterStyle::Enhanced, tex_view);
    }
    render_both(render::RiverFlowMode::Flow, cpu_flow, gpu_flow);
    render_both(render::RiverFlowMode::Legacy, cpu_legacy, gpu_legacy);
    REQUIRE(gpu_flow.size() == std::size_t{w} * h * 4);
    REQUIRE(gpu_legacy.size() == gpu_flow.size());
    RiverQ2 q;
    std::vector<std::size_t> pure_pixels;
    for (std::size_t i = 0; i < cpu_flow.size(); ++i) {
        const int sx = view.x + static_cast<int>(i % w), sy = view.y + static_cast<int>(i / w);
        const int cx = render::div_euclid(sx, render::kChunk),
                  cy = render::div_euclid(sy, render::kChunk);
        const render::Chunk* ch = cpu.chunk({group, cx, cy});
        if (ch == nullptr || ch->field.empty()) {
            continue;
        }
        const std::size_t f = (static_cast<std::size_t>(sy - cy * render::kChunk) *
                                   static_cast<std::size_t>(render::kChunk) +
                               static_cast<std::size_t>(sx - cx * render::kChunk)) *
                              4;
        if ((ch->field[f + 3] & 2U) == 0) {
            continue;
        }
        const bool under_object = !ch->objects.empty() && ch->objects[f / 4][3] != 0;
        if (!under_object && ch->field[f + 1] < 254) {
            ++q.blend_zone;
            continue;
        }
        int raw = 0, shade = 0, leg = 0;
        for (int k = 0; k < 3; ++k) {
            const auto ku = static_cast<std::size_t>(k);
            const int c_flow = static_cast<int>((cpu_flow[i] >> (16 - 8 * k)) & 0xFF);
            const int c_leg = static_cast<int>((cpu_legacy[i] >> (16 - 8 * k)) & 0xFF);
            const int g_flow = int{gpu_flow[4 * i + ku]}, g_leg = int{gpu_legacy[4 * i + ku]};
            raw = std::max(raw, std::abs(c_flow - g_flow));
            leg = std::max(leg, std::abs(c_leg - g_leg));
            shade = std::max(shade, std::abs((g_flow - g_leg) - (c_flow - c_leg)));
        }
        if (under_object) {
            ++q.under_objects;
            ++q.shading_under_objects.n;
            q.shading_under_objects.over2 += shade > 2 ? 1U : 0U;
            q.shading_under_objects.max = std::max(q.shading_under_objects.max, shade);
            continue;
        }
        pure_pixels.push_back(i);
        ++q.legacy.n;
        q.legacy.over2 += leg > 2 ? 1U : 0U;
        q.legacy.max = std::max(q.legacy.max, leg);
        ++q.raw.n;
        q.raw.over2 += raw > 2 ? 1U : 0U;
        q.raw.max = std::max(q.raw.max, raw);
        ++q.shading.n;
        q.shading.over2 += shade > 2 ? 1U : 0U;
        q.shading.max = std::max(q.shading.max, shade);
    }
    for (const auto& [cx, cy] : render::visible_chunks(view)) {
        const render::Chunk* ch = cpu.chunk({group, cx, cy});
        q.chunks_with_flow += ch != nullptr && !ch->flow.empty() ? 1U : 0U;
    }
    if (legacy_change && !pure_pixels.empty()) {
        // The size of the legacy shimmer itself: the standard deviation of the change of the
        // green channel of the pure river pixels over 800 ms (terrain and depth cancel; the
        // light moves) on the GPU against the CPU. On real rivers it is about 1.2 levels
        // (the GPU's rounding noise adds a few thousandths), so an amplitude that is 10 %
        // off shows where a 2/255 tolerance per pixel would not.
        std::vector<std::uint32_t> cpu_later;
        std::vector<std::uint8_t> gpu_later;
        render_both(render::RiverFlowMode::Legacy, cpu_later, gpu_later, time + 800);
        double sg = 0, sg2 = 0, sc = 0, sc2 = 0;
        for (const std::size_t i : pure_pixels) {
            const double dg = static_cast<double>(gpu_later[4 * i + 1]) -
                              static_cast<double>(gpu_legacy[4 * i + 1]);
            const double dc = static_cast<double>((cpu_later[i] >> 8) & 0xFF) -
                              static_cast<double>((cpu_legacy[i] >> 8) & 0xFF);
            sg += dg;
            sg2 += dg * dg;
            sc += dc;
            sc2 += dc * dc;
        }
        const double n = static_cast<double>(pure_pixels.size());
        q.legacy_sd_gpu = std::sqrt(sg2 / n - (sg / n) * (sg / n));
        q.legacy_sd_cpu = std::sqrt(sc2 / n - (sc / n) * (sc / n));
    }
    return q;
}

} // namespace

TEST_CASE("river pixels on the GPU match the CPU path: the whole pixel and what the flow shading "
          "adds (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    // The owner's canyon (D_256x256_0045_5B, group 1 at tile 152, 97) and one island of each
    // other climate with rivers (the QA islands of rivers.md 10.2; the views of
    // tools/river_qa), every zoom group and three times each.
    struct Case {
        const char* label;
        const char* file;
        int tx, ty;
    };
    const std::array cases = {
        Case{"owner's canyon", "Islands/Dschungel/D_256x256_0045_5B.scp", 152, 97},
        Case{"Nord island", "Islands/Nord/N_256x256_0006_2B.scp", 168, 84},
        Case{"Steppe island", "Islands/Steppe/S_256x256_0004_3B_Orient.scp", 135, 157},
        Case{"Tundra island", "Islands/Tundra/T_256x256_0008_2B_Po.scp", 136, 141},
        Case{"Praerie island", "Islands/Praerie/Pr_128x128_0003_1B.scp", 67, 38},
    };
    std::size_t total_river = 0;
    for (const Case& cs : cases) {
        const Loaded loaded = load_island_scene(*dir, cs.file);
        const render::Scene& scene = *loaded.scene;
        gpu::GpuLimits limits;
        limits.uploads_per_frame = SIZE_MAX;
        limits.river_quality = render::RiverQuality::High;
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
        REQUIRE_MESSAGE(made, (made ? "" : made.error()));
        gpu::GpuFrame& g = *made.value();
        constexpr int kW = 1280, kH = 720;
        const auto target = g.offscreen_target(kW, kH);
        for (const std::size_t group : {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
            const auto [px, py] = render::tile_top(cs.tx, cs.ty, group);
            const render::Rect view{px - kW / 2, py - kH / 2, kW, kH};
            render::FrameRenderer cpu(4), on_gpu(4, false);
            for (const std::uint64_t time :
                 {std::uint64_t{0}, std::uint64_t{700}, std::uint64_t{1600}}) {
                const RiverQ2 q = river_q2(scene, g, on_gpu, cpu, target.first.get(),
                                           target.second.get(), view, group, time);
                MESSAGE(std::string(cs.label)
                        << " group " << group << " t " << time << ": " << q.raw.n
                        << " pure river pixels (" << q.under_objects << " under objects, "
                        << q.blend_zone << " in the sea blend), whole pixel "
                        << 100.0 * q.raw.within2() << " % within 2/255 max " << q.raw.max
                        << "; flow shading alone " << 100.0 * q.shading.within2() << " % max "
                        << q.shading.max << "; legacy look " << 100.0 * q.legacy.within2()
                        << " % max " << q.legacy.max << "; shading under objects ("
                        << q.shading_under_objects.n << " px) "
                        << 100.0 * q.shading_under_objects.within2() << " % max "
                        << q.shading_under_objects.max << ", " << q.chunks_with_flow
                        << " chunks with flow");
                total_river += q.raw.n;
                if (q.raw.n < 500) {
                    continue; // no river to speak of in this view
                }
                // What the flow shading adds to the picture: 99.9 % within 2/255, none beyond 4.
                CHECK(q.shading.within2() >= 0.999);
                CHECK(q.shading.max <= 4);
                // The whole pixel: the CPU path rounds down after every step of the sea
                // (sprite, openness, shading) where the GPU keeps floats, a level or two on
                // every sea pixel in the legacy look as well (measured 99.4-99.97 % within 2, max
                // 3).
                CHECK(q.raw.within2() >= 0.99);
                CHECK(q.raw.max <= 4);
                // The legacy shimmer on the GPU (the R key; chunks still without flow) against
                // the CPU's, on the same pixels: the same rounding offset, nothing more.
                CHECK(q.legacy.within2() >= 0.99);
                CHECK(q.legacy.max <= 4);
                // Under objects (38 % of the owner's river pixels at group 1 are covered, left out
                // of the whole-pixel statistics): the water under them is shaded alike, but the
                // object's own edge differs (measured 97.6-100 % within 2, max 11).
                if (q.shading_under_objects.n >= 500) {
                    CHECK(q.shading_under_objects.within2() >= 0.97);
                    CHECK(q.shading_under_objects.max <= 12);
                }
                CHECK(q.chunks_with_flow > 0);
            }
        }
        CHECK(g.errors() == 0);
    }
    // The views have rivers: the test is not vacuous.
    CHECK(total_river > 100000);
}

TEST_CASE("river pixels on the GPU match the CPU path at the Low and Medium tiers (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    // The owner's canyon at groups 0 and 1, the tiers an integrated GPU gets: the CPU
    // reference follows the tier (FrameRenderer::river_quality), the same statistics as
    // for High (bench_frame --check passes the GPU's tier to its CPU reference too).
    const Loaded loaded = load_island_scene(*dir, "Islands/Dschungel/D_256x256_0045_5B.scp");
    const render::Scene& scene = *loaded.scene;
    constexpr int kW = 1280, kH = 720;
    for (const render::RiverQuality q : {render::RiverQuality::Low, render::RiverQuality::Medium}) {
        gpu::GpuLimits limits;
        limits.uploads_per_frame = SIZE_MAX;
        limits.river_quality = q;
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
        REQUIRE_MESSAGE(made, (made ? "" : made.error()));
        gpu::GpuFrame& g = *made.value();
        CHECK(g.river_quality() == q);
        const auto target = g.offscreen_target(kW, kH);
        for (const std::size_t group : {std::size_t{0}, std::size_t{1}}) {
            const auto [px, py] = render::tile_top(152, 97, group);
            const render::Rect view{px - kW / 2, py - kH / 2, kW, kH};
            render::FrameRenderer cpu(4), on_gpu(4, false);
            cpu.river_quality = q;
            const RiverQ2 r = river_q2(scene, g, on_gpu, cpu, target.first.get(),
                                       target.second.get(), view, group, 700, true);
            MESSAGE("tier " << static_cast<int>(q) << " group " << group << ": " << r.raw.n
                            << " pure river pixels, whole pixel " << 100.0 * r.raw.within2()
                            << " % within 2/255 max " << r.raw.max << "; flow shading alone "
                            << 100.0 * r.shading.within2() << " % max " << r.shading.max);
            REQUIRE(r.raw.n > 1000);
            CHECK(r.shading.within2() >= 0.999);
            CHECK(r.shading.max <= 4);
            CHECK(r.raw.within2() >= 0.99);
            CHECK(r.raw.max <= 4);
            CHECK(r.legacy.within2() >= 0.99);
            MESSAGE("legacy shimmer, sd of the 800 ms change (green): GPU "
                    << r.legacy_sd_gpu << " CPU " << r.legacy_sd_cpu);
            REQUIRE(r.legacy_sd_cpu > 1.0);
            CHECK(r.legacy_sd_gpu / r.legacy_sd_cpu == doctest::Approx(1.0).epsilon(0.03));
            // The tier is not a no-op: against the High reference the shading differs.
            render::FrameRenderer high(4);
            std::vector<std::uint32_t> a(static_cast<std::size_t>(kW) * kH),
                b(static_cast<std::size_t>(kW) * kH);
            cpu.river_flow = render::RiverFlowMode::Flow;
            cpu.render(scene, view, group, 700, render::WaterStyle::Enhanced, a.data());
            high.render(scene, view, group, 700, render::WaterStyle::Enhanced, b.data());
            if (group == 0) {
                CHECK(a != b);
            }
        }
        CHECK(g.errors() == 0);
    }
}

// ---- Stand-in chunks (OQ-077) ----

namespace {

/// Land chunk of noise: every pixel of ground and objects differs from its
/// neighbours (objects with alpha 0, 128 and 255), the sea field says land
/// (cover 0), so a frame shows ground under objects and a wrong source pixel,
/// a shifted mapping or a missing chunk changes it.
render::Chunk noise_land_chunk(std::size_t group, int cx, int cy) {
    render::Chunk c;
    const auto n = static_cast<std::size_t>(render::kChunk);
    c.ground.resize(n * n);
    c.objects.resize(n * n);
    c.field.assign(n * n * 4, 0);
    for (std::size_t y = 0; y < n; ++y) {
        for (std::size_t x = 0; x < n; ++x) {
            const auto gx =
                static_cast<std::int64_t>(cx) * render::kChunk + static_cast<std::int64_t>(x);
            const auto gy =
                static_cast<std::int64_t>(cy) * render::kChunk + static_cast<std::int64_t>(y);
            auto h = static_cast<std::uint32_t>(gx * 73856093 ^ gy * 19349663 ^
                                                static_cast<std::int64_t>(group) * 83492791);
            h ^= h >> 13;
            h *= 0x5BD1E995U;
            h ^= h >> 15;
            const std::size_t i = y * n + x;
            c.ground[i] = h & 0xFFFFFFU;
            const std::uint32_t g = h * 2654435761U;
            const std::uint8_t a = (g >> 30) == 0 ? 0 : (g >> 30) == 1 ? 128 : 255;
            c.objects[i] =
                render::Px{static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(g >> 8),
                           static_cast<std::uint8_t>(g >> 16), a};
        }
    }
    return c;
}

/// The pixel a frame shows over land: ground, then the objects over it.
render::Px land_pixel(const render::Chunk& c, std::size_t i) {
    render::Px p{static_cast<std::uint8_t>(c.ground[i] >> 16),
                 static_cast<std::uint8_t>(c.ground[i] >> 8),
                 static_cast<std::uint8_t>(c.ground[i]), 255};
    render::over(p, c.objects[i]);
    return p;
}

/// Chunk index of a scene coordinate, written out (floor division).
int floor_chunk(int v) {
    return v >= 0 ? v / render::kChunk : -((-v + render::kChunk - 1) / render::kChunk);
}

/// Inserts all chunks of `from` that chunk (cx, cy) of `to` shows.
void insert_sources(render::FrameRenderer& fr, std::size_t to, int cx, int cy, std::size_t from) {
    for (const auto& sp : render::stand_in_spans(cx, cy, to, from)) {
        fr.insert_chunk({from, sp.src_cx, sp.src_cy}, noise_land_chunk(from, sp.src_cx, sp.src_cy));
    }
}

} // namespace

TEST_CASE("stand-in spans tile the destination chunk and map by the tile ratio of kTile") {
    for (std::size_t to = 0; to < render::kGroups; ++to) {
        for (const std::size_t from : render::stand_in_groups(to)) {
            CHECK(from != to);
            for (const auto& [cx, cy] : {std::pair{0, 0}, std::pair{1, 0}, std::pair{-1, 2},
                                         std::pair{-3, -2}, std::pair{7, 5}}) {
                const auto spans = render::stand_in_spans(cx, cy, to, from);
                CHECK(!spans.empty());
                const render::Rect chunk = render::chunk_rect(cx, cy);
                long long area = 0;
                for (const auto& sp : spans) {
                    area += static_cast<long long>(sp.dst.w) * sp.dst.h;
                    CHECK(sp.dst.intersect(chunk) == sp.dst);
                    // Every pixel of a part reads the part's source chunk.
                    for (int y = sp.dst.y; y < sp.dst.y + sp.dst.h; y += 7) {
                        for (int x = sp.dst.x; x < sp.dst.x + sp.dst.w; x += 5) {
                            const int sx = render::stand_in_source(x, render::kTile[from].w,
                                                                   render::kTile[to].w);
                            const int sy = render::stand_in_source(y, render::kTile[from].h,
                                                                   render::kTile[to].h);
                            const int chunk_x = floor_chunk(sx);
                            CHECK(chunk_x == sp.src_cx);
                            const int chunk_y = floor_chunk(sy);
                            CHECK(chunk_y == sp.src_cy);
                        }
                    }
                }
                // The parts are disjoint (distinct source chunks) and cover the chunk.
                CHECK(area == static_cast<long long>(render::kChunk) * render::kChunk);
            }
        }
    }
    CHECK(render::stand_in_source(100, render::kTile[0].w, render::kTile[1].w) ==
          (2 * 100 + 1) * render::kTile[0].w / (2 * render::kTile[1].w));
    CHECK(render::stand_in_groups(0).size() == render::kGroups - 1);
    CHECK(render::stand_in_groups(1).front() == 0);
}

TEST_CASE("CPU reference: a missing chunk shows the scaled chunks of the nearest built group") {
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    constexpr std::size_t kTo = 1;
    // Group 2 (tiles 2.1 times smaller) and group 0 (tiles twice as large).
    for (const std::size_t from : {std::size_t{2}, std::size_t{0}}) {
        render::FrameRenderer fr(2, false);
        // Background workers: the first frame cannot have the chunk built yet.
        fr.set_workers(std::make_unique<render::ChunkWorkers>(scene, 1));
        fr.stand_ins = true;
        insert_sources(fr, kTo, 0, 0, from);
        const render::Rect view = render::chunk_rect(0, 0);
        std::vector<std::uint32_t> out(static_cast<std::size_t>(view.w) *
                                       static_cast<std::size_t>(view.h));
        const auto st = fr.render(scene, view, kTo, 0, render::WaterStyle::Original, out.data());
        CHECK(st.chunks_missing == 1);
        std::size_t wrong = 0;
        for (int y = 0; y < view.h; ++y) {
            for (int x = 0; x < view.w; ++x) {
                const auto i =
                    static_cast<std::size_t>(y) * render::kChunk + static_cast<std::size_t>(x);
                // The source pixel by the tile ratio of kTile at the pixel centre,
                // written out here apart from the reference code.
                const int sx = (2 * x + 1) * render::kTile[from].w / (2 * render::kTile[kTo].w);
                const int sy = (2 * y + 1) * render::kTile[from].h / (2 * render::kTile[kTo].h);
                const render::Chunk* src =
                    fr.chunk({from, sx / render::kChunk, sy / render::kChunk});
                REQUIRE(src != nullptr);
                const render::Px e = land_pixel(
                    *src, static_cast<std::size_t>(sy % render::kChunk) * render::kChunk +
                              static_cast<std::size_t>(sx % render::kChunk));
                const std::uint32_t px = out[i];
                int m = std::abs(int{e[0]} - int((px >> 16) & 255));
                m = std::max(m, std::abs(int{e[1]} - int((px >> 8) & 255)));
                m = std::max(m, std::abs(int{e[2]} - int(px & 255)));
                wrong += m > 1 ? 1U : 0U;
            }
        }
        CHECK(wrong == 0);
        // Off: the same frame is open sea.
        render::FrameRenderer plain(2, false);
        plain.set_workers(std::make_unique<render::ChunkWorkers>(scene, 1));
        insert_sources(plain, kTo, 0, 0, from);
        std::vector<std::uint32_t> sea(out.size());
        plain.render(scene, view, kTo, 0, render::WaterStyle::Original, sea.data());
        CHECK(sea != out);
    }
}

TEST_CASE(
    "stand-in sources are kept only for visible chunks that are not built, and only when asked") {
    constexpr std::size_t kTo = 1, kFrom = 2;
    render::FrameRenderer fr(1, false);
    insert_sources(fr, kTo, 1, 0, kFrom);
    const auto spans = render::stand_in_spans(1, 0, kTo, kFrom);
    REQUIRE(!spans.empty());
    const std::vector<std::pair<int, int>> visible{{0, 0}, {1, 0}};
    // Both switches off: the eviction is the old one.
    CHECK(fr.stand_in_retain(kTo, visible).empty());
    fr.retain_stand_in_sources = true;
    auto kept = fr.stand_in_retain(kTo, visible);
    CHECK(kept.size() == spans.size());
    for (const auto& sp : spans) {
        CHECK(kept.contains({kFrom, sp.src_cx, sp.src_cy}));
    }
    // The CPU path's own switch asks for them as well.
    fr.retain_stand_in_sources = false;
    fr.stand_ins = true;
    CHECK(fr.stand_in_retain(kTo, visible).size() == spans.size());
    // Once the chunk itself is built there is nothing to stand in for.
    fr.insert_chunk({kTo, 1, 0}, noise_land_chunk(kTo, 1, 0));
    CHECK(fr.stand_in_retain(kTo, {{1, 0}}).empty());
}

TEST_CASE("GPU stand-in chunks equal the CPU reference (synthetic)") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    constexpr std::size_t kTo = 1;
    const auto side = static_cast<std::uint32_t>(render::kChunk);
    for (const std::size_t from : {std::size_t{2}, std::size_t{0}}) {
        gpu::GpuLimits limits;
        limits.uploads_per_frame = 1;
        limits.wave_simulation = false;
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
        REQUIRE_MESSAGE(made, (made ? "" : made.error()));
        gpu::GpuFrame& g = *made.value();
        // Blocking chunks: the group's own chunks exist in the CPU cache, but one
        // per frame reaches the GPU, so the other is drawn from the stand-in.
        render::FrameRenderer fr(1, false);
        insert_sources(fr, kTo, 1, 0, from);
        // The sources on the GPU first: they were visible at their own group.
        std::optional<render::Rect> src_view;
        for (const auto& sp : render::stand_in_spans(1, 0, kTo, from)) {
            const render::Rect r = render::chunk_rect(sp.src_cx, sp.src_cy);
            src_view = src_view ? src_view->unite(r) : r;
        }
        REQUIRE(src_view);
        const auto source_target = g.offscreen_target(static_cast<std::uint32_t>(src_view->w),
                                                      static_cast<std::uint32_t>(src_view->h));
        for (int f = 0; f < 12; ++f) {
            g.render(fr, scene, *src_view, from, 0, render::WaterStyle::Original,
                     source_target.second.get());
        }
        // Two destination chunks side by side: (0, 0) uploads, (1, 0) stands in.
        const render::Rect view{0, 0, 2 * render::kChunk, render::kChunk};
        const auto target = g.offscreen_target(2 * side, side);
        const auto st =
            g.render(fr, scene, view, kTo, 0, render::WaterStyle::Original, target.second.get());
        CHECK(st.chunks_uploaded == 1);
        CHECK(st.stand_in_chunks == 1);
        CHECK(st.stand_in_parts == render::stand_in_spans(1, 0, kTo, from).size());
        const auto px = g.read_back(target.first.get(), 2 * side, side);
        REQUIRE(px.size() == std::size_t{2} * side * side * 4);
        const auto ref = render::stand_in_chunk(fr, kTo, 1, 0, scene.textures.water());
        REQUIRE(ref);
        std::size_t wrong = 0;
        int worst = 0;
        for (std::size_t y = 0; y < side; ++y) {
            for (std::size_t x = 0; x < side; ++x) {
                const render::Px e = land_pixel(*ref, y * side + x);
                const std::size_t o = (y * 2 * side + side + x) * 4;
                int m = 0;
                for (std::size_t k = 0; k < 3; ++k) {
                    m = std::max(m, std::abs(int{e[k]} - int{px[o + k]}));
                }
                worst = std::max(worst, m);
                wrong += m > 1 ? 1U : 0U;
            }
        }
        MESSAGE("source group " << from << ": " << wrong << " pixels differ by more than 1, worst "
                                << worst);
        CHECK(wrong == 0);
        CHECK(g.errors() == 0);
        // Off: nothing is drawn from other groups.
        gpu::GpuLimits off = limits;
        off.stand_ins = false;
        auto made_off = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, off);
        REQUIRE(made_off);
        gpu::GpuFrame& go = *made_off.value();
        render::FrameRenderer fr2(1, false);
        insert_sources(fr2, kTo, 1, 0, from);
        const auto st_target = go.offscreen_target(static_cast<std::uint32_t>(src_view->w),
                                                   static_cast<std::uint32_t>(src_view->h));
        for (int f = 0; f < 12; ++f) {
            go.render(fr2, scene, *src_view, from, 0, render::WaterStyle::Original,
                      st_target.second.get());
        }
        const auto target2 = go.offscreen_target(2 * side, side);
        const auto st2 =
            go.render(fr2, scene, view, kTo, 0, render::WaterStyle::Original, target2.second.get());
        CHECK(st2.stand_in_chunks == 0);
        CHECK(go.errors() == 0);
    }
}

TEST_CASE("GPU stand-ins carry the sea field: cover 0 shows the open water, cover 255 the sea") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    constexpr std::size_t kTo = 1, kFrom = 2;
    const auto side = static_cast<std::uint32_t>(render::kChunk);
    // Sea chunks without ground: the field alone decides (original water, flags
    // bit 0 = sea drawn). Cover 255 and 0 in a checkerboard of 16 x 16 texels.
    auto covered = [](int lx, int ly) { return (((lx / 16) + (ly / 16)) & 1) != 0; };
    auto sea_chunk = [&] {
        render::Chunk c;
        const auto n = static_cast<std::size_t>(render::kChunk);
        c.field.assign(n * n * 4, 0);
        for (std::size_t y = 0; y < n; ++y) {
            for (std::size_t x = 0; x < n; ++x) {
                const std::size_t i = (y * n + x) * 4;
                c.field[i] = covered(static_cast<int>(x), static_cast<int>(y)) ? 255 : 0;
                c.field[i + 3] = 1;
            }
        }
        return c;
    };
    gpu::GpuLimits limits;
    limits.uploads_per_frame = 1;
    limits.wave_simulation = false;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    render::FrameRenderer fr(1, false);
    const auto spans = render::stand_in_spans(1, 0, kTo, kFrom);
    REQUIRE(!spans.empty());
    std::optional<render::Rect> src_view;
    for (const auto& sp : spans) {
        fr.insert_chunk({kFrom, sp.src_cx, sp.src_cy}, sea_chunk());
        const render::Rect r = render::chunk_rect(sp.src_cx, sp.src_cy);
        src_view = src_view ? src_view->unite(r) : r;
    }
    const auto source_target = g.offscreen_target(static_cast<std::uint32_t>(src_view->w),
                                                  static_cast<std::uint32_t>(src_view->h));
    for (int f = 0; f < 12; ++f) {
        g.render(fr, scene, *src_view, kFrom, 0, render::WaterStyle::Original,
                 source_target.second.get());
    }
    // Two destination chunks: (0, 0) uploads (an empty chunk), (1, 0) stands in.
    const render::Rect view{0, 0, 2 * render::kChunk, render::kChunk};
    const auto target = g.offscreen_target(2 * side, side);
    const auto st =
        g.render(fr, scene, view, kTo, 0, render::WaterStyle::Original, target.second.get());
    REQUIRE(st.stand_in_chunks == 1);
    const auto px = g.read_back(target.first.get(), 2 * side, side);
    REQUIRE(px.size() == std::size_t{2} * side * side * 4);
    const render::Px water = scene.textures.water();
    std::size_t open_n = 0, open_wrong = 0, sea_n = 0, sea_same = 0;
    for (const auto& sp : spans) {
        for (int y = sp.dst.y; y < sp.dst.y + sp.dst.h; ++y) {
            for (int x = sp.dst.x; x < sp.dst.x + sp.dst.w; ++x) {
                const int sx =
                    render::stand_in_source(x, render::kTile[kFrom].w, render::kTile[kTo].w);
                const int sy =
                    render::stand_in_source(y, render::kTile[kFrom].h, render::kTile[kTo].h);
                const int lx = sx - sp.src_cx * render::kChunk;
                const int ly = sy - sp.src_cy * render::kChunk;
                REQUIRE((lx >= 0 && lx < render::kChunk && ly >= 0 && ly < render::kChunk));
                const std::size_t o =
                    (static_cast<std::size_t>(y) * 2 * side + static_cast<std::size_t>(x)) * 4;
                int diff = 0;
                for (std::size_t k = 0; k < 3; ++k) {
                    diff = std::max(diff, std::abs(int{water[k]} - int{px[o + k]}));
                }
                if (covered(lx, ly)) {
                    ++sea_n;
                    sea_same += diff <= 1 ? 1U : 0U;
                } else {
                    ++open_n;
                    open_wrong += diff > 1 ? 1U : 0U;
                }
            }
        }
    }
    MESSAGE("open " << open_n << " (wrong " << open_wrong << "), sea " << sea_n
                    << " (same as water " << sea_same << ")");
    CHECK(open_n > 0);
    CHECK(sea_n > 0);
    // Where the source field has no cover the open water colour shows.
    CHECK(open_wrong == 0);
    // Where it has, the sea pattern is drawn over it: almost nowhere the plain
    // water colour (the pattern has texture; a few texels may match by chance).
    CHECK(sea_same * 10 < sea_n);
    CHECK(g.errors() == 0);
}

TEST_CASE("lookahead_rect: a ring of one chunk, grown in the direction of motion up to a cap") {
    const render::Rect view{100, 200, 1280, 800};
    const auto still = render::lookahead_rect(view, 0, 0, 0.3, 3);
    CHECK(still.x == view.x - render::kChunk);
    CHECK(still.y == view.y - render::kChunk);
    CHECK(still.w == view.w + 2 * render::kChunk);
    CHECK(still.h == view.h + 2 * render::kChunk);
    // 1000 px/s for 0.3 s: 300 px more on the side of motion only.
    const auto right = render::lookahead_rect(view, 1000, 0, 0.3, 3);
    CHECK(right.x == still.x);
    CHECK(right.w == still.w + 300);
    CHECK(right.y == still.y);
    CHECK(right.h == still.h);
    const auto up = render::lookahead_rect(view, 0, -500, 0.3, 3);
    CHECK(up.y == still.y - 150);
    CHECK(up.h == still.h + 150);
    CHECK(up.x == still.x);
    // Capped at max_chunks chunks, and 0 means the plain ring.
    const auto capped = render::lookahead_rect(view, 1e6, 0, 0.3, 3);
    CHECK(capped.w == still.w + 3 * render::kChunk);
    const auto none = render::lookahead_rect(view, 1e6, 1e6, 0.3, 0);
    CHECK(none.w == still.w);
    CHECK(none.h == still.h);
    // A fraction of a pixel rounds up: 1001 px/s for 0.3 s is 300.3 px, 301 more.
    const auto frac = render::lookahead_rect(view, 1001, 0, 0.3, 3);
    CHECK(frac.w == still.w + 301);
    CHECK(frac.x == still.x);
    // Left and down at once: the left side and the bottom grow, nothing else.
    const auto left_down = render::lookahead_rect(view, -1000, 500, 0.3, 3);
    CHECK(left_down.x == still.x - 300);
    CHECK(left_down.w == still.w + 300);
    CHECK(left_down.y == still.y);
    CHECK(left_down.h == still.h + 150);
    // The cap holds for the height as well.
    const auto tall = render::lookahead_rect(view, 0, -1e6, 0.3, 2);
    CHECK(tall.y == still.y - 2 * render::kChunk);
    CHECK(tall.h == still.h + 2 * render::kChunk);
    CHECK(tall.w == still.w);
}

TEST_CASE("the margin follows the view's velocity through update_chunks: leading side, shrinking "
          "after a stop, the same speed after a zoom step") {
    const EmptyScene es = empty_scene();
    render::FrameRenderer fr(1, false);
    auto t = std::chrono::steady_clock::time_point{};
    fr.clock = [&] { return t; };
    constexpr std::size_t kGroup = 1;
    render::Rect view{0, 0, render::kChunk, render::kChunk};
    const auto step = std::chrono::milliseconds(10);
    auto move = [&](int dx, int dy, int frames) {
        for (int i = 0; i < frames; ++i) {
            view.x += dx;
            view.y += dy;
            t += step;
            fr.update_chunks(*es.scene, view, kGroup);
        }
    };
    // The margin chunks in the column `cx`.
    auto column = [&](int cx) {
        std::size_t n = 0;
        for (const auto& k : fr.margin_keys()) {
            n += std::get<1>(k) == cx ? 1U : 0U;
        }
        return n;
    };
    fr.update_chunks(*es.scene, view, kGroup);
    // 50 px per 10 ms to the right: about 5000 px/s after the smoothing.
    move(50, 0, 40);
    const auto [vx, vy] = fr.velocity();
    CHECK(vx > 4000.0);
    CHECK(vx < 5001.0);
    CHECK(std::abs(vy) < 1.0);
    // The view covers chunks first..last; the plain ring is one more on each side.
    // Moving right at ~5000 px/s for the 0.1 s horizon adds ~500 px on the right.
    const int first = render::div_euclid(view.x, render::kChunk);
    const int last = render::div_euclid(view.x + view.w - 1, render::kChunk);
    CHECK(column(last + 2) > 0);
    CHECK(column(first - 2) == 0);
    // Standing still for two seconds: the velocity decays, the margin is the ring.
    move(0, 0, 200);
    CHECK(std::abs(fr.velocity().first) < 1.0);
    CHECK(column(last + 2) == 0);
    CHECK(column(last + 1) > 0);
    // Moving again, then a zoom step to group 2: the same speed on screen, in
    // group 2's pixels (the anchoring jump of the centre is not motion).
    move(50, 0, 40);
    const double before = fr.velocity().first;
    const render::Rect zoomed = render::zoomed_view(view, kGroup, 2);
    t += step;
    fr.update_chunks(*es.scene, zoomed, 2);
    const double ratio = static_cast<double>(render::kTile[2].w) / render::kTile[kGroup].w;
    CHECK(fr.velocity().first == doctest::Approx(before * ratio).epsilon(1e-9));
}

TEST_CASE("a jump of the view by more than its size resets the velocity of the margin") {
    const EmptyScene es = empty_scene();
    render::FrameRenderer fr(1, false);
    auto t = std::chrono::steady_clock::time_point{};
    fr.clock = [&] { return t; };
    render::Rect view{0, 0, render::kChunk, render::kChunk};
    for (int i = 0; i <= 40; ++i) {
        view.x += i > 0 ? 50 : 0;
        t += std::chrono::milliseconds(10);
        fr.update_chunks(*es.scene, view, 1);
    }
    REQUIRE(fr.velocity().first > 1000.0);
    // A minimap click 5000 px away: without the reset that reads as about
    // 37,000 px/s and pins the margin at its cap.
    view.x += 5000;
    view.y -= 3000;
    t += std::chrono::milliseconds(10);
    fr.update_chunks(*es.scene, view, 1);
    CHECK(fr.velocity().first == 0.0);
    CHECK(fr.velocity().second == 0.0);
    // A move of exactly one view width is still a drag.
    view.x += render::kChunk;
    t += std::chrono::milliseconds(10);
    fr.update_chunks(*es.scene, view, 1);
    CHECK(fr.velocity().first > 0.0);
    // Each axis against its own size: a wide view, a vertical move larger than its
    // height (but not its width) is a jump; one within its height is a drag.
    render::FrameRenderer wide(1, false);
    wide.clock = [&] { return t; };
    render::Rect w{0, 0, 2 * render::kChunk, render::kChunk / 2};
    for (int i = 0; i <= 40; ++i) {
        w.y += i > 0 ? 20 : 0;
        t += std::chrono::milliseconds(10);
        wide.update_chunks(*es.scene, w, 1);
    }
    REQUIRE(wide.velocity().second > 1000.0);
    w.y += w.h + 100;
    t += std::chrono::milliseconds(10);
    wide.update_chunks(*es.scene, w, 1);
    CHECK(wide.velocity().second == 0.0);
    w.y += w.h;
    t += std::chrono::milliseconds(10);
    wide.update_chunks(*es.scene, w, 1);
    CHECK(wide.velocity().second > 0.0);
}

TEST_CASE("adjacent_keys: the view a zoom step would show, in both neighbouring groups") {
    const EmptyScene es = empty_scene();
    render::FrameRenderer fr(1, false);
    const render::Rect view{1000, 600, 1280, 800};
    fr.update_chunks(*es.scene, view, 1);
    const auto& keys = fr.adjacent_keys();
    REQUIRE(!keys.empty());
    std::set<render::ChunkKey> got(keys.begin(), keys.end());
    CHECK(got.size() == keys.size());
    for (const std::size_t g : {std::size_t{0}, std::size_t{2}}) {
        const render::Rect next = render::zoomed_view(view, 1, g);
        for (const auto& [cx, cy] : render::visible_chunks(next)) {
            CHECK(got.contains({g, cx, cy}));
        }
    }
    for (const auto& k : keys) {
        CHECK(std::get<0>(k) != 1);
    }
    // At the first group there is no finer neighbour.
    fr.update_chunks(*es.scene, view, 0);
    for (const auto& k : fr.adjacent_keys()) {
        CHECK(std::get<0>(k) == 1);
    }
}

TEST_CASE("prefetch: built chunks of the neighbouring groups are uploaded when nothing waits, "
          "one per frame, up to the cap") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    gpu::GpuLimits limits;
    limits.wave_simulation = false;
    limits.prefetch_chunks = 2;
    limits.prefetch_per_frame = 1;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    render::FrameRenderer fr(1, false);
    const auto side = static_cast<std::uint32_t>(render::kChunk);
    const render::Rect one{0, 0, render::kChunk, render::kChunk};
    const auto target = g.offscreen_target(side, side);
    // The visible chunk uploads; after that nothing waits.
    gpu::GpuStats st;
    for (int f = 0; f < 6; ++f) {
        st = g.render(fr, *es.scene, one, 1, 0, render::WaterStyle::Original, target.second.get());
    }
    CHECK(st.chunks_uploaded == 0);
    CHECK(st.chunks_prefetched == 0); // none of the neighbours' chunks is built
    const std::vector<render::ChunkKey> keys = fr.adjacent_keys();
    REQUIRE(keys.size() >= 3);
    for (std::size_t i = 0; i < 3; ++i) {
        fr.insert_chunk(keys[i], noise_land_chunk(std::get<0>(keys[i]), std::get<1>(keys[i]),
                                                  std::get<2>(keys[i])));
    }
    std::size_t total = 0;
    for (int f = 0; f < 5; ++f) {
        st = g.render(fr, *es.scene, one, 1, 0, render::WaterStyle::Original, target.second.get());
        CHECK(st.chunks_prefetched <= 1);
        total += st.chunks_prefetched;
    }
    // The cap of two resident chunks of other groups holds although three are built.
    CHECK(total == 2);
    CHECK(g.errors() == 0);
}

TEST_CASE("a chunk that replaces the open sea it was shown as dissolves in over chunk_fade_ms of "
          "the fade clock, also while the animation clock is paused") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    const auto side = static_cast<std::uint32_t>(render::kChunk);
    // Two chunks side by side, one upload per frame: (0, 0) reaches the GPU in the
    // first frame (nothing was shown there before: no fade), (1, 0) is shown as open
    // sea in that frame and dissolves in when it arrives in the next.
    const render::Rect two{0, 0, 2 * render::kChunk, render::kChunk};
    const render::Chunk land = noise_land_chunk(1, 1, 0);
    gpu::GpuLimits limits;
    limits.wave_simulation = false;
    limits.uploads_per_frame = 1;
    limits.stand_ins = false;
    limits.chunk_fade_ms = 100;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    render::FrameRenderer fr(1, false);
    fr.insert_chunk({1, 0, 0}, noise_land_chunk(1, 0, 0));
    fr.insert_chunk({1, 1, 0}, land);
    const auto target = g.offscreen_target(2 * side, side);
    std::uint64_t fade_clock = 0;
    g.set_fade_clock([&] { return fade_clock; });
    // The pixels of chunk (1, 0), the right half, at fade time t. The animation
    // clock stands still at 1000 (the viewer paused): the fade does not follow it.
    auto frame_at = [&](std::uint64_t t) {
        fade_clock = t;
        const auto st =
            g.render(fr, scene, two, 1, 1000, render::WaterStyle::Original, target.second.get());
        const auto all = g.read_back(target.first.get(), 2 * side, side);
        std::vector<std::uint8_t> right(std::size_t{side} * side * 4);
        for (std::size_t y = 0; y < side; ++y) {
            std::copy_n(all.begin() + static_cast<std::ptrdiff_t>((y * 2 * side + side) * 4),
                        side * 4, right.begin() + static_cast<std::ptrdiff_t>(y * side * 4));
        }
        return std::pair{st, right};
    };
    // Both frames at one time: the open sea (the sea pattern) is the same in both.
    const auto [st_first, px0] = frame_at(1000);
    CHECK(st_first.chunks_uploaded == 1);
    CHECK(st_first.chunks_fading == 0); // (0, 0) was not shown as anything before
    CHECK(st_first.open_sea_chunks == 1);
    // The frame that uploads (1, 0) starts its fade: nothing of it yet.
    const auto [st0, px_start] = frame_at(1000);
    CHECK(st0.chunks_uploaded == 1);
    CHECK(st0.chunks_fading == 1);
    const auto [st1, px1] = frame_at(1050);
    CHECK(st1.chunks_fading == 1);
    const auto [st2, px2] = frame_at(1100);
    CHECK(st2.chunks_fading == 0);
    std::size_t n = 0, start_ok = 0, mid_between = 0, end_ok = 0;
    for (std::size_t i = 0; i < std::size_t{side} * side; ++i) {
        if (land.objects[i][3] != 0) {
            continue; // ground only
        }
        const render::Px ground{static_cast<std::uint8_t>(land.ground[i] >> 16),
                                static_cast<std::uint8_t>(land.ground[i] >> 8),
                                static_cast<std::uint8_t>(land.ground[i]), 255};
        // What the placeholder showed there: the open sea.
        const render::Px water{px0[i * 4], px0[i * 4 + 1], px0[i * 4 + 2], 255};
        int span = 0;
        for (std::size_t k = 0; k < 3; ++k) {
            span = std::max(span, std::abs(int{ground[k]} - int{water[k]}));
        }
        if (span < 80) {
            continue;
        }
        ++n;
        int from_water = 0, from_ground = 0, end_off = 0, start_off = 0;
        for (std::size_t k = 0; k < 3; ++k) {
            start_off = std::max(start_off, std::abs(int{px_start[i * 4 + k]} - int{water[k]}));
            from_water = std::max(from_water, std::abs(int{px1[i * 4 + k]} - int{water[k]}));
            from_ground = std::max(from_ground, std::abs(int{px1[i * 4 + k]} - int{ground[k]}));
            end_off = std::max(end_off, std::abs(int{px2[i * 4 + k]} - int{ground[k]}));
        }
        start_ok += start_off <= 2 ? 1U : 0U;
        mid_between += from_water > 5 && from_ground > 5 ? 1U : 0U;
        end_ok += end_off <= 1 ? 1U : 0U;
    }
    REQUIRE(n > 1000);
    CHECK(start_ok == n);
    CHECK(mid_between * 100 >= n * 95);
    CHECK(end_ok == n);
    // Chunk_fade_ms 0 swaps at once.
    gpu::GpuLimits plain = limits;
    plain.chunk_fade_ms = 0;
    auto made2 = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, plain);
    REQUIRE(made2);
    gpu::GpuFrame& g2 = *made2.value();
    render::FrameRenderer fr2(1, false);
    fr2.insert_chunk({1, 0, 0}, noise_land_chunk(1, 0, 0));
    fr2.insert_chunk({1, 1, 0}, land);
    const auto target2 = g2.offscreen_target(2 * side, side);
    for (const std::uint64_t t : {1000U, 1000U}) {
        const auto st =
            g2.render(fr2, scene, two, 1, t, render::WaterStyle::Original, target2.second.get());
        CHECK(st.chunks_uploaded == 1);
        CHECK(st.chunks_fading == 0);
    }
    CHECK(g.errors() == 0);
    CHECK(g2.errors() == 0);
}

TEST_CASE("a plain pan: the margin of the group shown is uploaded ahead, so a chunk coming into "
          "view appears at once, without a placeholder or a dissolve") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    const auto side = static_cast<std::uint32_t>(render::kChunk);
    gpu::GpuLimits limits;
    limits.wave_simulation = false;
    // Room for exactly the ring of one chunk around a still one-chunk view (8):
    // the neighbouring groups' chunks, built as well, must queue behind it.
    limits.prefetch_chunks = 8;
    limits.prefetch_per_frame = 1;
    limits.chunk_fade_ms = 100;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    render::FrameRenderer fr(1, false);
    const auto target = g.offscreen_target(side, side);
    const render::Rect at{0, 0, render::kChunk, render::kChunk};
    g.render(fr, scene, at, 1, 0, render::WaterStyle::Original, target.second.get());
    // The ring of one chunk around a still view, the current group only.
    const std::vector<render::ChunkKey> margin = fr.margin_keys();
    REQUIRE(margin.size() == limits.prefetch_chunks);
    for (const auto& k : margin) {
        CHECK(std::get<0>(k) == 1);
        fr.insert_chunk(k, noise_land_chunk(1, std::get<1>(k), std::get<2>(k)));
    }
    const std::vector<render::ChunkKey> adjacent = fr.adjacent_keys();
    REQUIRE(!adjacent.empty());
    for (const auto& k : adjacent) {
        fr.insert_chunk(k, noise_land_chunk(std::get<0>(k), std::get<1>(k), std::get<2>(k)));
    }
    std::size_t prefetched = 0;
    for (int f = 0; f < 20; ++f) {
        const auto st = g.render(fr, scene, at, 1, static_cast<std::uint64_t>(16 * (f + 1)),
                                 render::WaterStyle::Original, target.second.get());
        CHECK(st.chunks_prefetched <= 1);
        prefetched += st.chunks_prefetched;
    }
    // Up to the cap, which the margin fills (the move below proves it is the margin).
    CHECK(prefetched == margin.size());
    // One chunk to the right: it is on the GPU already.
    const render::Rect next{render::kChunk, 0, render::kChunk, render::kChunk};
    const auto st =
        g.render(fr, scene, next, 1, 400, render::WaterStyle::Original, target.second.get());
    CHECK(st.chunks_uploaded == 0);
    CHECK(st.placeholders == 0);
    CHECK(st.chunks_fading == 0);
    CHECK(st.open_sea_chunks == 0);
    const auto px = g.read_back(target.first.get(), side, side);
    const render::Chunk* shown = fr.chunk({1, 1, 0});
    REQUIRE(shown != nullptr);
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < std::size_t{side} * side; ++i) {
        const render::Px e = land_pixel(*shown, i);
        int m = 0;
        for (std::size_t k = 0; k < 3; ++k) {
            m = std::max(m, std::abs(int{e[k]} - int{px[i * 4 + k]}));
        }
        wrong += m > 1 ? 1U : 0U;
    }
    CHECK(wrong == 0);
    // A visible real chunk rebuilt (a new serial: its river flow arrived) swaps
    // at once: nothing but itself was on screen there.
    fr.insert_chunk({1, 1, 0}, noise_land_chunk(1, 7, 7));
    const auto rebuilt =
        g.render(fr, scene, next, 1, 416, render::WaterStyle::Original, target.second.get());
    CHECK(rebuilt.chunks_uploaded == 1);
    CHECK(rebuilt.chunks_fading == 0);
    CHECK(g.errors() == 0);
}

TEST_CASE("a chunk dissolves in over its stand-in: linear in the fade, the stand-in's sources kept "
          "with the cache at its eviction limit") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    constexpr std::size_t kTo = 1, kFrom = 2;
    const auto side = static_cast<std::uint32_t>(render::kChunk);
    gpu::GpuLimits limits;
    limits.uploads_per_frame = 1;
    limits.wave_simulation = false;
    limits.chunk_fade_ms = 100;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    std::uint64_t fade_clock = 0;
    g.set_fade_clock([&] { return fade_clock; });
    render::FrameRenderer fr(1, false);
    // Nothing kept beyond what is visible or retained: a source that is not
    // retained is evicted in the frame it stops being visible.
    fr.spare_chunks = 0;
    insert_sources(fr, kTo, 1, 0, kFrom);
    const render::Chunk real = noise_land_chunk(kTo, 1, 0);
    // The sources on the GPU first: they were visible at their own group.
    std::optional<render::Rect> src_view;
    for (const auto& sp : render::stand_in_spans(1, 0, kTo, kFrom)) {
        const render::Rect r = render::chunk_rect(sp.src_cx, sp.src_cy);
        src_view = src_view ? src_view->unite(r) : r;
    }
    REQUIRE(src_view);
    const auto source_target = g.offscreen_target(static_cast<std::uint32_t>(src_view->w),
                                                  static_cast<std::uint32_t>(src_view->h));
    for (int f = 0; f < 12; ++f) {
        g.render(fr, scene, *src_view, kFrom, 0, render::WaterStyle::Original,
                 source_target.second.get());
    }
    // The chunks of the group shown, built (inserted after the frames above: with
    // no spare chunks those frames evict everything they do not show).
    fr.insert_chunk({kTo, 0, 0}, noise_land_chunk(kTo, 0, 0));
    fr.insert_chunk({kTo, 1, 0}, real);
    // The stand-in as the CPU reference draws it (before any eviction can happen).
    const auto ref = render::stand_in_chunk(fr, kTo, 1, 0, scene.textures.water());
    REQUIRE(ref);
    const render::Rect view{0, 0, 2 * render::kChunk, render::kChunk};
    const auto target = g.offscreen_target(2 * side, side);
    auto frame_at = [&](std::uint64_t t) {
        fade_clock = t;
        const auto st =
            g.render(fr, scene, view, kTo, 0, render::WaterStyle::Original, target.second.get());
        const auto all = g.read_back(target.first.get(), 2 * side, side);
        std::vector<std::uint8_t> right(std::size_t{side} * side * 4);
        for (std::size_t y = 0; y < side; ++y) {
            std::copy_n(all.begin() + static_cast<std::ptrdiff_t>((y * 2 * side + side) * 4),
                        side * 4, right.begin() + static_cast<std::ptrdiff_t>(y * side * 4));
        }
        return std::pair{st, right};
    };
    auto sources_cached = [&] {
        for (const auto& sp : render::stand_in_spans(1, 0, kTo, kFrom)) {
            if (fr.chunk({kFrom, sp.src_cx, sp.src_cy}) == nullptr) {
                return false;
            }
        }
        return true;
    };
    // (0, 0) uploads; (1, 0) is built but waits: drawn as its stand-in.
    const auto [st_a, px_a] = frame_at(1000);
    CHECK(st_a.chunks_uploaded == 1);
    CHECK(st_a.stand_in_chunks == 1);
    CHECK(sources_cached());
    // (1, 0) uploads and starts to dissolve in over its stand-in.
    const auto [st_b, px_b] = frame_at(1000);
    CHECK(st_b.chunks_fading == 1);
    CHECK(st_b.stand_in_chunks == 1);
    // A quarter of the way: smoothstep(0.25) = 0.15625 of the chunk (linear 0.25).
    const auto [st_q, px_q] = frame_at(1025);
    CHECK(st_q.chunks_fading == 1);
    const auto [st_c, px_c] = frame_at(1050);
    CHECK(st_c.chunks_fading == 1);
    CHECK(st_c.stand_in_chunks == 1);
    CHECK(sources_cached());
    const auto [st_d, px_d] = frame_at(1100);
    CHECK(st_d.chunks_fading == 0);
    CHECK(st_d.stand_in_chunks == 0);
    std::size_t standin_wrong = 0, start_wrong = 0, mid_wrong = 0, mid_n = 0, end_wrong = 0;
    for (std::size_t i = 0; i < std::size_t{side} * side; ++i) {
        const render::Px s = land_pixel(*ref, i);
        const render::Px e = land_pixel(real, i);
        int a = 0, b = 0, d = 0;
        for (std::size_t k = 0; k < 3; ++k) {
            a = std::max(a, std::abs(int{s[k]} - int{px_a[i * 4 + k]}));
            b = std::max(b, std::abs(int{px_a[i * 4 + k]} - int{px_b[i * 4 + k]}));
            d = std::max(d, std::abs(int{e[k]} - int{px_d[i * 4 + k]}));
        }
        standin_wrong += a > 1 ? 1U : 0U;
        start_wrong += b > 1 ? 1U : 0U;
        end_wrong += d > 1 ? 1U : 0U;
        // Half-way (smoothstep at 0.5 = 0.5): the even mix of the two grounds where
        // neither has an object over it (an object is composited over the mix, not
        // mixed itself).
        if (ref->objects[i][3] != 0 || real.objects[i][3] != 0) {
            continue;
        }
        ++mid_n;
        int m = 0;
        for (std::size_t k = 0; k < 3; ++k) {
            const double expect = 0.5 * s[k] + 0.5 * e[k];
            m = std::max(m, static_cast<int>(std::lround(
                                std::abs(expect - static_cast<double>(px_c[i * 4 + k])))));
            const double quarter = 0.84375 * s[k] + 0.15625 * e[k];
            m = std::max(m, static_cast<int>(std::lround(
                                std::abs(quarter - static_cast<double>(px_q[i * 4 + k])))));
        }
        mid_wrong += m > 2 ? 1U : 0U;
    }
    CHECK(standin_wrong == 0);
    CHECK(start_wrong == 0);
    REQUIRE(mid_n > 10000);
    CHECK(mid_wrong == 0);
    CHECK(end_wrong == 0);
    CHECK(g.errors() == 0);
}

TEST_CASE("open sea is counted by the area a stand-in leaves uncovered, inside the view") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    // Group 2 shown (the smallest tiles): its chunk (1, 0) shows parts of several
    // group-1 chunks (1075 x 1075 group-1 pixels).
    constexpr std::size_t kTo = 2, kFrom = 1;
    const auto side = static_cast<std::uint32_t>(render::kChunk);
    gpu::GpuLimits limits;
    limits.uploads_per_frame = 1;
    limits.wave_simulation = false;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    render::FrameRenderer fr(1, false);
    // Chunk (0, 0) and most of (1, 0), one upload per frame: (1, 0) waits and
    // stands in; its last 100 columns are outside the view.
    const render::Rect view{0, 0, 2 * render::kChunk - 100, render::kChunk};
    // Only one of the group-1 chunks that chunk (1, 0) of group 2 shows exists,
    // none of group 0: the cover has a hole. The one whose part reaches past the
    // view's right edge, so that the part must be clipped to the view.
    const auto spans = render::stand_in_spans(1, 0, kTo, kFrom);
    REQUIRE(spans.size() > 1);
    const auto past = std::find_if(spans.begin(), spans.end(), [&](const auto& sp) {
        return sp.dst.x + sp.dst.w > view.x + view.w;
    });
    REQUIRE(past != spans.end());
    const auto& one = *past;
    fr.insert_chunk({kFrom, one.src_cx, one.src_cy},
                    noise_land_chunk(kFrom, one.src_cx, one.src_cy));
    const render::Rect src = render::chunk_rect(one.src_cx, one.src_cy);
    const auto src_target =
        g.offscreen_target(static_cast<std::uint32_t>(src.w), static_cast<std::uint32_t>(src.h));
    for (int f = 0; f < 4; ++f) {
        g.render(fr, scene, src, kFrom, 0, render::WaterStyle::Original, src_target.second.get());
    }
    const auto target = g.offscreen_target(static_cast<std::uint32_t>(view.w), side);
    const auto st =
        g.render(fr, scene, view, kTo, 0, render::WaterStyle::Original, target.second.get());
    REQUIRE(st.stand_in_chunks == 1);
    // Only the source that exists covers: the area of every span reading it (the
    // parts of one source chunk may be merged into one draw), inside the view.
    std::uint64_t covered = 0;
    for (const auto& sp : spans) {
        if (sp.src_cx == one.src_cx && sp.src_cy == one.src_cy) {
            if (const auto in = sp.dst.intersect(view)) {
                covered += static_cast<std::uint64_t>(in->w) * static_cast<std::uint64_t>(in->h);
            }
        }
    }
    const std::uint64_t visible_px = std::uint64_t{side - 100} * side;
    REQUIRE(covered > 0);
    REQUIRE(covered < visible_px);
    CHECK(st.open_sea_chunks == 1);
    CHECK(st.open_sea_px == visible_px - covered);
    CHECK(g.errors() == 0);
    // Stand-ins off: the whole visible part of the waiting chunk is open sea.
    gpu::GpuLimits off = limits;
    off.stand_ins = false;
    auto made_off = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, off);
    REQUIRE(made_off);
    gpu::GpuFrame& go = *made_off.value();
    render::FrameRenderer fr_off(1, false);
    const auto target_off = go.offscreen_target(static_cast<std::uint32_t>(view.w), side);
    const auto st_off = go.render(fr_off, scene, view, kTo, 0, render::WaterStyle::Original,
                                  target_off.second.get());
    CHECK(st_off.chunks_uploaded == 1);
    CHECK(st_off.open_sea_chunks == 1);
    CHECK(st_off.open_sea_px == visible_px);
    CHECK(go.errors() == 0);
}

/// A chunk of a smooth world: the ground colour is a ramp of the world position in
/// tiles (red along x, green along y, 100 levels per tile, wrapping), the same for
/// every zoom group, so a stand-in made from another group should match the chunk
/// built for the group shown up to the other group's pixel size.
render::Chunk ramp_chunk(std::size_t group, int cx, int cy) {
    render::Chunk c;
    const auto n = static_cast<std::size_t>(render::kChunk);
    c.ground.resize(n * n);
    for (std::size_t y = 0; y < n; ++y) {
        for (std::size_t x = 0; x < n; ++x) {
            // Pixel centre in tiles of this group.
            const double wx =
                (cx * render::kChunk + static_cast<double>(x) + 0.5) / render::kTile[group].w;
            const double wy =
                (cy * render::kChunk + static_cast<double>(y) + 0.5) / render::kTile[group].h;
            const auto r = static_cast<std::uint32_t>(std::fmod(wx * 100.0, 256.0) + 256.0) & 255U;
            const auto gch =
                static_cast<std::uint32_t>(std::fmod(wy * 100.0, 256.0) + 256.0) & 255U;
            c.ground[y * n + x] = (r << 16) | (gch << 8);
        }
    }
    return c;
}

TEST_CASE("a stand-in lines up with the chunk it stands in for (a smooth world, no shift)") {
    const EmptyScene es = empty_scene();
    const auto n = static_cast<std::size_t>(render::kChunk);
    for (std::size_t to = 0; to < render::kGroups; ++to) {
        for (const std::size_t from : render::stand_in_groups(to)) {
            render::FrameRenderer fr(1, false);
            for (const auto& [cx, cy] : {std::pair{0, 0}, std::pair{1, 0}, std::pair{-1, 2}}) {
                for (const auto& sp : render::stand_in_spans(cx, cy, to, from)) {
                    fr.insert_chunk({from, sp.src_cx, sp.src_cy},
                                    ramp_chunk(from, sp.src_cx, sp.src_cy));
                }
                const auto ref = render::stand_in_chunk(fr, to, cx, cy, es.scene->textures.water());
                REQUIRE(ref);
                const render::Chunk truth = ramp_chunk(to, cx, cy);
                // The error allowed is the ramp across one source pixel: the stand-in
                // shows the source pixel that contains the destination pixel's
                // centre. Pixels near the wrap of the ramp are left out.
                const double slope_x = 100.0 / render::kTile[from].w;
                const double slope_y = 100.0 / render::kTile[from].h;
                double bias_r = 0, bias_g = 0;
                std::size_t counted = 0, worst_r = 0, worst_g = 0;
                for (std::size_t i = 0; i < n * n; ++i) {
                    const int tr = static_cast<int>((truth.ground[i] >> 16) & 255U);
                    const int tg = static_cast<int>((truth.ground[i] >> 8) & 255U);
                    const int sr = static_cast<int>((ref->ground[i] >> 16) & 255U);
                    const int sg = static_cast<int>((ref->ground[i] >> 8) & 255U);
                    if (tr < 24 || tr > 232 || tg < 24 || tg > 232) {
                        continue;
                    }
                    ++counted;
                    bias_r += sr - tr;
                    bias_g += sg - tg;
                    worst_r = std::max(worst_r, static_cast<std::size_t>(std::abs(sr - tr)));
                    worst_g = std::max(worst_g, static_cast<std::size_t>(std::abs(sg - tg)));
                }
                REQUIRE(counted > n * n / 2);
                bias_r /= static_cast<double>(counted);
                bias_g /= static_cast<double>(counted);
                INFO("to " << to << " from " << from << " chunk " << cx << "," << cy << " bias "
                           << bias_r << "," << bias_g << " worst " << worst_r << "," << worst_g);
                // Within one source pixel. The average shift is at most half a source
                // pixel (half the ramp's step): at an exact 2x ratio (groups 0 and 1)
                // the destination pixel's centre lies on the boundary of two source
                // pixels and the rule picks the same side every time, a quarter of a
                // destination pixel; a real misalignment would be a whole source pixel.
                CHECK(static_cast<double>(worst_r) <= slope_x + 1.5);
                CHECK(static_cast<double>(worst_g) <= slope_y + 1.5);
                CHECK(std::abs(bias_r) <= 0.5 * slope_x + 0.3);
                CHECK(std::abs(bias_g) <= 0.5 * slope_y + 0.3);
            }
        }
    }
}

TEST_CASE("stand-in cover chooses the source group per part") {
    const auto all = [](const render::ChunkKey&) { return true; };
    for (std::size_t to = 0; to < render::kGroups; ++to) {
        const auto order = render::stand_in_groups(to);
        for (const auto& [cx, cy] : {std::pair{0, 0}, std::pair{1, 0}, std::pair{-1, 2}}) {
            const render::Rect chunk = render::chunk_rect(cx, cy);
            const auto full = render::stand_in_cover(cx, cy, to, all);
            long long area = 0;
            for (const auto& p : full) {
                CHECK(p.from == order.front());
                area += static_cast<long long>(p.span.dst.w) * p.span.dst.h;
            }
            CHECK(area == static_cast<long long>(render::kChunk) * render::kChunk);
            // The first source chunk of the nearest group is missing: its part
            // comes from the next group, the other parts stay.
            const auto first = render::stand_in_spans(cx, cy, to, order.front()).front();
            const render::ChunkKey gone{order.front(), first.src_cx, first.src_cy};
            const auto part = render::stand_in_cover(
                cx, cy, to, [&](const render::ChunkKey& k) { return k != gone; });
            long long covered = 0;
            bool fallback = false;
            for (const auto& p : part) {
                CHECK(p.span.dst.intersect(chunk) == p.span.dst);
                covered += static_cast<long long>(p.span.dst.w) * p.span.dst.h;
                if (p.from == order[1]) {
                    fallback = true;
                    CHECK(first.dst.intersect(p.span.dst) == p.span.dst);
                } else {
                    CHECK(p.from == order.front());
                    CHECK(render::ChunkKey{p.from, p.span.src_cx, p.span.src_cy} != gone);
                }
            }
            CHECK(fallback);
            CHECK(covered == area);
            // Nothing available anywhere: open sea.
            CHECK(render::stand_in_cover(cx, cy, to, [](const render::ChunkKey&) {
                      return false;
                  }).empty());
        }
    }
}

TEST_CASE("the CPU stand-in chunk: none without a source, the ships flag from its sources") {
    constexpr std::size_t kTo = 1;
    const auto order = render::stand_in_groups(kTo);
    REQUIRE(!order.empty());
    render::FrameRenderer fr(1, false);
    // Nothing cached in any group: no cover, open sea.
    CHECK_FALSE(render::stand_in_chunk(fr, kTo, 1, 0, render::Px{1, 2, 3, 255}));
    for (const auto& sp : render::stand_in_spans(1, 0, kTo, order.front())) {
        render::Chunk c = noise_land_chunk(order.front(), sp.src_cx, sp.src_cy);
        c.ships = false; // not the default, so a copy that was dropped shows
        fr.insert_chunk({order.front(), sp.src_cx, sp.src_cy}, std::move(c));
    }
    const auto in = render::stand_in_chunk(fr, kTo, 1, 0, render::Px{1, 2, 3, 255});
    REQUIRE(in);
    CHECK_FALSE(in->ships);
    CHECK(in->ground.size() ==
          static_cast<std::size_t>(render::kChunk) * static_cast<std::size_t>(render::kChunk));
}

TEST_CASE("GPU stand-in with a hole in the nearest group equals the CPU reference, and the real "
          "chunk replaces it") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    const auto side = static_cast<std::uint32_t>(render::kChunk);
    for (std::size_t to = 0; to < render::kGroups; ++to) {
        const auto order = render::stand_in_groups(to);
        const auto hole = render::stand_in_spans(1, 0, to, order.front()).front();
        const render::ChunkKey gone{order.front(), hole.src_cx, hole.src_cy};
        gpu::GpuLimits limits;
        limits.uploads_per_frame = 1;
        limits.wave_simulation = false;
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
        REQUIRE_MESSAGE(made, (made ? "" : made.error()));
        gpu::GpuFrame& g = *made.value();
        render::FrameRenderer fr(1, false);
        // Every source chunk of the two nearest groups but the hole.
        for (const std::size_t from : {order[0], order[1]}) {
            for (const auto& sp : render::stand_in_spans(1, 0, to, from)) {
                if (render::ChunkKey{from, sp.src_cx, sp.src_cy} != gone) {
                    fr.insert_chunk({from, sp.src_cx, sp.src_cy},
                                    noise_land_chunk(from, sp.src_cx, sp.src_cy));
                }
            }
        }
        // The fallback group must cover the hole.
        const auto cover = render::stand_in_cover(
            1, 0, to, [&](const render::ChunkKey& k) { return fr.chunk(k) != nullptr; });
        bool fallback = false;
        for (const auto& p : cover) {
            fallback = fallback || p.from == order[1];
        }
        CHECK(fallback);
        // The sources reach the GPU at their own group, one view each.
        for (const auto& p : cover) {
            const render::Rect r = render::chunk_rect(p.span.src_cx, p.span.src_cy);
            const auto t = g.offscreen_target(side, side);
            for (int f = 0; f < 2; ++f) {
                g.render(fr, scene, r, p.from, 0, render::WaterStyle::Original, t.second.get());
            }
        }
        const render::Rect view{0, 0, 2 * render::kChunk, render::kChunk};
        const auto target = g.offscreen_target(2 * side, side);
        const auto st =
            g.render(fr, scene, view, to, 0, render::WaterStyle::Original, target.second.get());
        CHECK(st.stand_in_chunks == 1);
        CHECK(st.stand_in_parts == cover.size());
        const auto px = g.read_back(target.first.get(), 2 * side, side);
        REQUIRE(px.size() == std::size_t{2} * side * side * 4);
        const auto ref = render::stand_in_chunk(fr, to, 1, 0, scene.textures.water());
        REQUIRE(ref);
        std::size_t wrong = 0;
        for (std::size_t y = 0; y < side; ++y) {
            for (std::size_t x = 0; x < side; ++x) {
                const render::Px e = land_pixel(*ref, y * side + x);
                const std::size_t o = (y * 2 * side + side + x) * 4;
                int m = 0;
                for (std::size_t k = 0; k < 3; ++k) {
                    m = std::max(m, std::abs(int{e[k]} - int{px[o + k]}));
                }
                wrong += m > 1 ? 1U : 0U;
            }
        }
        MESSAGE("target group " << to << ": " << wrong << " pixels differ by more than 1");
        CHECK(wrong == 0);
        // The next frame uploads the real chunk: no stand-in, and the frame is
        // the plain one (stand-ins off, same uploads).
        const auto st2 =
            g.render(fr, scene, view, to, 0, render::WaterStyle::Original, target.second.get());
        CHECK(st2.stand_in_chunks == 0);
        const auto after = g.read_back(target.first.get(), 2 * side, side);
        gpu::GpuLimits off = limits;
        off.stand_ins = false;
        auto made_off = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, off);
        REQUIRE(made_off);
        gpu::GpuFrame& go = *made_off.value();
        const auto target_off = go.offscreen_target(2 * side, side);
        for (int f = 0; f < 3; ++f) {
            go.render(fr, scene, view, to, 0, render::WaterStyle::Original,
                      target_off.second.get());
        }
        CHECK(go.read_back(target_off.first.get(), 2 * side, side) == after);
        CHECK(g.errors() == 0);
    }
}

TEST_CASE("the steps of a GPU frame add up to its total, and the sync steps are counted") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    gpu::GpuLimits limits;
    limits.wave_simulation = false;
    // One chunk reaches the GPU per frame; the other visible one is a placeholder.
    limits.uploads_per_frame = 1;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    render::FrameRenderer fr(1, false);
    const auto side = static_cast<std::uint32_t>(render::kChunk);
    const render::Rect two{0, 0, 2 * render::kChunk, render::kChunk};
    const render::Rect one{0, 0, render::kChunk, render::kChunk};
    const auto target = g.offscreen_target(2 * side, side);
    auto check_sum = [](const gpu::GpuStats& st) {
        double sum = 0;
        for (const auto& step : st.steps()) {
            CHECK(step.ms >= 0.0);
            sum += step.ms;
        }
        // The clock reads between two steps are the only difference.
        CHECK(std::abs(sum - st.total_ms) <= 0.05 + 0.02 * st.total_ms);
        // The coarse fields are sums of steps.
        CHECK(st.chunk_sync_ms + 1e-6 >=
              st.sync_scan_ms + st.sync_drop_ms + st.sync_real_ms + st.sync_placeholder_ms);
        CHECK(st.total_ms >= st.encode_ms);
    };
    // An outside stopwatch around the call: the steps start at the call's own
    // clock read, so this is what shows a region of render() that no step
    // covers (the steps are laps of one clock, adding up by construction).
    const auto call_start = std::chrono::steady_clock::now();
    const auto a =
        g.render(fr, scene, two, 1, 0, render::WaterStyle::Original, target.second.get());
    const double call_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - call_start)
            .count();
    check_sum(a);
    // Slack for the thread being descheduled between the call and the first clock
    // read inside it, on a loaded machine.
    CHECK(a.total_ms <= call_ms + 0.05);
    CHECK(a.total_ms >= call_ms - 25.0);
    CHECK(a.chunks_uploaded == 1);
    CHECK(a.placeholders == 1);
    CHECK(a.chunks_dropped == 0);
    CHECK(a.stand_in_ms >= 0.0);
    // Only the first chunk stays visible: the placeholder of the second is
    // dropped (it has no planes to free), nothing is uploaded or made.
    const auto b =
        g.render(fr, scene, one, 1, 0, render::WaterStyle::Original, target.second.get());
    check_sum(b);
    CHECK(b.chunks_uploaded == 0);
    CHECK(b.placeholders == 0);
    CHECK(b.chunks_dropped == 1);
    CHECK(b.planes_freed == 0);
    // Nothing changed.
    const auto c =
        g.render(fr, scene, one, 1, 0, render::WaterStyle::Original, target.second.get());
    check_sum(c);
    CHECK(c.placeholders == 0);
    CHECK(c.chunks_dropped == 0);
    CHECK(g.errors() == 0);
}

TEST_CASE("pre-created chunk planes stay within the idle plane budget") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    gpu::GpuLimits limits;
    limits.wave_simulation = false;
    limits.prewarm_chunks = 4; // 20 planes asked for
    // 4 MiB of idle planes: planes are 512 x 512 x 3 bytes on average (786432
    // bytes), so 5 fit.
    limits.idle_plane_budget_mb = 4;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    for (int i = 0; i < 2000 && g.warm_planes_ready() < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CHECK(g.warm_planes_ready() == 5);
    CHECK(g.errors() == 0);
}

TEST_CASE(
    "pre-created chunk planes are taken by the first uploads, and an early end joins the thread") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    {
        // Ending the frame while the thread is still creating planes must not
        // touch a released device.
        gpu::GpuLimits busy;
        busy.wave_simulation = false;
        busy.prewarm_chunks = 64;
        auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, busy);
        REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    }
    const EmptyScene es = empty_scene();
    gpu::GpuLimits limits;
    limits.wave_simulation = false;
    limits.prewarm_chunks = 2;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    // The thread creates 2 chunks x 5 planes.
    for (int i = 0; i < 2000 && g.warm_planes_ready() < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(g.warm_planes_ready() == 10);
    render::FrameRenderer fr(1, false);
    fr.insert_chunk({0, 0, 0}, noise_land_chunk(0, 0, 0));
    const auto side = static_cast<std::uint32_t>(render::kChunk);
    const auto target = g.offscreen_target(side, side);
    const render::Rect one{0, 0, render::kChunk, render::kChunk};
    const auto st =
        g.render(fr, *es.scene, one, 0, 0, render::WaterStyle::Original, target.second.get());
    CHECK(st.chunks_uploaded == 1);
    CHECK(st.planes_warm >= 1);
    // After the first render() the thread is paced: a frame that uploaded
    // something gives it no token (the window thread needs the driver's
    // allocator then), so the planes taken are not made up for yet ...
    const std::size_t taken = st.planes_warm;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK(g.warm_planes_ready() == 10 - taken);
    // ... and every quiet frame (nothing uploaded, no placeholder) lets it make
    // one plane, no more.
    for (std::size_t i = 0; i < taken; ++i) {
        const std::size_t before = g.warm_planes_ready();
        const auto quiet =
            g.render(fr, *es.scene, one, 0, 0, render::WaterStyle::Original, target.second.get());
        REQUIRE(quiet.chunks_uploaded == 0);
        for (int k = 0; k < 2000 && g.warm_planes_ready() == before; ++k) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        CHECK(g.warm_planes_ready() == before + 1);
    }
    CHECK(g.warm_planes_ready() == 10);
    CHECK(g.errors() == 0);
}

TEST_CASE("resampled_view covers the output and keeps the fraction of the camera") {
    // Output 100 x 50 at scale 2 centred on (100.25, 60.75): the grid spans
    // 75.25 .. 125.25 in x and 48.25 .. 73.25 in y.
    const auto r = gpu::resampled_view(100.25, 60.75, 100, 50, 2.0, gpu::ResampleFilter::Bilinear);
    CHECK(r.view.x == 75);
    CHECK(r.view.y == 48);
    CHECK(r.resample.offset_x == doctest::Approx(0.25));
    CHECK(r.resample.offset_y == doctest::Approx(0.25));
    CHECK(r.resample.scale == 2.0);
    // The far edge (+ 1 pixel for the filters' reach) is inside the view.
    CHECK(r.view.x + r.view.w >= 126 + 1);
    CHECK(r.view.y + r.view.h >= 74 + 1);
    CHECK_FALSE(r.resample.identity());
    CHECK(gpu::Resample{}.identity());
    // A zoom out: one output pixel per two grid pixels.
    const auto o = gpu::resampled_view(0.0, 0.0, 64, 64, 0.5, gpu::ResampleFilter::Box);
    CHECK(o.view.x == -64);
    CHECK(o.view.w >= 128 + 1);
    CHECK(o.resample.offset_x == 0.0);
}

TEST_CASE("resample pass: the scale, the offset and the filters (synthetic)") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    gpu::GpuLimits limits;
    limits.wave_simulation = false;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    constexpr std::size_t kGroup = 1;
    render::FrameRenderer fr(1, false);
    fr.insert_chunk({kGroup, 0, 0}, noise_land_chunk(kGroup, 0, 0));
    constexpr std::uint32_t kGrid = 64;
    // The grid as the compositor composes it, without a resample.
    const render::Rect view{0, 0, static_cast<int>(kGrid), static_cast<int>(kGrid)};
    const auto grid_target = g.offscreen_target(kGrid, kGrid);
    for (int f = 0; f < 12; ++f) {
        g.render(fr, scene, view, kGroup, 0, render::WaterStyle::Original,
                 grid_target.second.get());
    }
    const auto grid = g.read_back(grid_target.first.get(), kGrid, kGrid);
    REQUIRE(grid.size() == std::size_t{kGrid} * kGrid * 4);
    auto at = [&](int x, int y, std::size_t k) {
        x = std::clamp(x, 0, static_cast<int>(kGrid) - 1);
        y = std::clamp(y, 0, static_cast<int>(kGrid) - 1);
        return static_cast<int>(
            grid[(static_cast<std::size_t>(y) * kGrid + static_cast<std::size_t>(x)) * 4 + k]);
    };
    // Renders the same view through `resample` into an output of `ow` x `oh`.
    auto resampled = [&](std::uint32_t ow, std::uint32_t oh, const gpu::Resample& rs,
                         const render::Rect& grid_view) {
        const auto target = g.offscreen_target(ow, oh);
        g.render(fr, scene, grid_view, kGroup, 0, render::WaterStyle::Original, target.second.get(),
                 &rs);
        return g.read_back(target.first.get(), ow, oh);
    };
    // Differences from `want(x, y, k)` over an `ow` x `oh` output.
    auto worst = [&](const std::vector<std::uint8_t>& out, std::uint32_t ow, std::uint32_t oh,
                     auto&& want) {
        REQUIRE(out.size() == std::size_t{ow} * oh * 4);
        int w = 0;
        for (std::uint32_t y = 0; y < oh; ++y) {
            for (std::uint32_t x = 0; x < ow; ++x) {
                for (std::size_t k = 0; k < 3; ++k) {
                    const int have = out[(std::size_t{y} * ow + x) * 4 + k];
                    w = std::max(
                        w, std::abs(have - want(static_cast<int>(x), static_cast<int>(y), k)));
                }
            }
        }
        return w;
    };
    // The grid is not flat: neighbouring pixels differ (else the tests prove nothing).
    int steps = 0;
    for (int x = 0; x + 1 < static_cast<int>(kGrid); ++x) {
        steps += at(x, 10, 0) != at(x + 1, 10, 0) ? 1 : 0;
    }
    REQUIRE(steps > 20);
    {
        // Scale 2, nearest: every grid pixel becomes a 2 x 2 block.
        gpu::Resample rs;
        rs.scale = 2.0;
        const auto out = resampled(2 * kGrid, 2 * kGrid, rs, view);
        CHECK(worst(out, 2 * kGrid, 2 * kGrid,
                    [&](int x, int y, std::size_t k) { return at(x / 2, y / 2, k); }) == 0);
    }
    {
        // Scale 1 at offset (0.5, 0.5), nearest: the output pixel x shows the grid
        // pixel floor(0.5 + x + 0.5) = x + 1 (the offset is the output's edge).
        gpu::Resample rs;
        rs.offset_x = 0.5;
        rs.offset_y = 0.5;
        const auto out = resampled(kGrid - 2, kGrid - 2, rs, view);
        CHECK(worst(out, kGrid - 2, kGrid - 2,
                    [&](int x, int y, std::size_t k) { return at(x + 1, y + 1, k); }) == 0);
    }
    {
        // Scale 2, bilinear: the grid pixel centres are at (i + 0.5); the output
        // pixel x has its centre at (x + 0.5) / 2 grid pixels, so it blends the
        // grid pixels around f = (x + 0.5) / 2 - 0.5 = x / 2 - 0.25.
        gpu::Resample rs;
        rs.scale = 2.0;
        rs.filter = gpu::ResampleFilter::Bilinear;
        const auto out = resampled(2 * kGrid, 2 * kGrid, rs, view);
        auto bilinear = [&](int x, int y, std::size_t k) {
            const double fx = x / 2.0 - 0.25, fy = y / 2.0 - 0.25;
            const int ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy));
            const double tx = fx - ix, ty = fy - iy;
            const double top = at(ix, iy, k) * (1 - tx) + at(ix + 1, iy, k) * tx;
            const double bottom = at(ix, iy + 1, k) * (1 - tx) + at(ix + 1, iy + 1, k) * tx;
            return static_cast<int>(std::lround(top * (1 - ty) + bottom * ty));
        };
        CHECK(worst(out, 2 * kGrid, 2 * kGrid, bilinear) <= 1);
    }
    {
        // Scale 0.5, box: the mean of the 2 x 2 grid pixels under each output pixel.
        // The grid is composed on a view twice the size (the compositor's grid).
        const render::Rect big{0, 0, 2 * static_cast<int>(kGrid), 2 * static_cast<int>(kGrid)};
        const auto big_target = g.offscreen_target(2 * kGrid, 2 * kGrid);
        for (int f = 0; f < 4; ++f) {
            g.render(fr, scene, big, kGroup, 0, render::WaterStyle::Original,
                     big_target.second.get());
        }
        const auto big_px = g.read_back(big_target.first.get(), 2 * kGrid, 2 * kGrid);
        gpu::Resample rs;
        rs.scale = 0.5;
        rs.filter = gpu::ResampleFilter::Box;
        const auto out = resampled(kGrid, kGrid, rs, big);
        auto mean = [&](int x, int y, std::size_t k) {
            int sum = 0;
            for (int j = 0; j < 2; ++j) {
                for (int i = 0; i < 2; ++i) {
                    sum += big_px[(static_cast<std::size_t>(2 * y + j) * 2 * kGrid +
                                   static_cast<std::size_t>(2 * x + i)) *
                                      4 +
                                  k];
                }
            }
            return static_cast<int>(std::lround(sum / 4.0));
        };
        CHECK(worst(out, kGrid, kGrid, mean) <= 1);
    }
    CHECK(g.errors() == 0);
}

TEST_CASE("resample pass: an identity resample is the same picture as none (synthetic)") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    gpu::GpuLimits limits;
    limits.wave_simulation = false;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE(made);
    gpu::GpuFrame& g = *made.value();
    render::FrameRenderer fr(1, false);
    fr.insert_chunk({1, 0, 0}, noise_land_chunk(1, 0, 0));
    const render::Rect view{0, 0, 48, 48};
    const auto a = g.offscreen_target(48, 48);
    const auto b = g.offscreen_target(48, 48);
    const gpu::Resample none;
    for (int f = 0; f < 6; ++f) {
        g.render(fr, scene, view, 1, 0, render::WaterStyle::Original, a.second.get());
        g.render(fr, scene, view, 1, 0, render::WaterStyle::Original, b.second.get(), &none);
    }
    const auto pa = g.read_back(a.first.get(), 48, 48), pb = g.read_back(b.first.get(), 48, 48);
    REQUIRE(pa.size() == 48U * 48U * 4U);
    CHECK(pa == pb);
}
TEST_CASE("resampled_view: negative positions, fractions, stable size, limits") {
    // Negative camera: the origin is the floor, the offset the fraction in [0, 1).
    const auto n = gpu::resampled_view(-10.3, -0.7, 64, 64, 1.0, gpu::ResampleFilter::Nearest);
    CHECK(n.view.x == -43);
    CHECK(n.view.y == -33);
    CHECK(n.resample.offset_x == doctest::Approx(0.7));
    CHECK(n.resample.offset_y == doctest::Approx(0.3));
    // A fraction of at least a half: the floor, not the rounded origin (x0 = 75.75).
    const auto f = gpu::resampled_view(100.75, 60.0, 100, 50, 2.0, gpu::ResampleFilter::Nearest);
    CHECK(f.view.x == 75);
    CHECK(f.resample.offset_x == doctest::Approx(0.75));
    // For a sweep of positions and scales every output pixel's samples (the pixel
    // centre, the bilinear neighbour, the box footprint's far side) lie inside
    // the grid, and the grid's size is a multiple of the step.
    std::set<std::pair<int, int>> sizes;
    for (double cx = -40.0; cx < 40.0; cx += 7.31) {
        for (const double s : {0.7, 0.9, 1.0, 1.37, 1.45}) {
            const auto r =
                gpu::resampled_view(cx, cx * 0.5 + 3.3, 130, 70, s, gpu::ResampleFilter::Box);
            CHECK(r.view.w % gpu::kResampleGridStep == 0);
            CHECK(r.view.h % gpu::kResampleGridStep == 0);
            const double far_x = r.resample.offset_x + 130.0 / s;
            const double far_y = r.resample.offset_y + 70.0 / s;
            CHECK(far_x + 1.0 <= r.view.w);
            CHECK(far_y + 1.0 <= r.view.h);
            CHECK(r.resample.offset_x >= 0.0);
            CHECK(r.resample.offset_x < 1.0);
            sizes.insert({r.view.w, r.view.h});
        }
    }
    // The grid does not change size with the camera at one scale (textures are not
    // recreated while panning).
    std::set<std::pair<int, int>> at_one_scale;
    for (double cx = 0.0; cx < 30.0; cx += 0.37) {
        const auto r = gpu::resampled_view(cx, 11.0 + cx, 130, 70, 1.37, gpu::ResampleFilter::Box);
        at_one_scale.insert({r.view.w, r.view.h});
    }
    CHECK(at_one_scale.size() == 1);
    // The scale is limited, and raised so that the grid fits max_grid.
    const auto tiny = gpu::resampled_view(0.0, 0.0, 64, 64, 1e-12, gpu::ResampleFilter::Box);
    CHECK(tiny.resample.scale == doctest::Approx(1.0 / 64.0));
    const auto big = gpu::resampled_view(0.0, 0.0, 7680, 4320, 0.7, gpu::ResampleFilter::Box);
    CHECK(big.view.w <= 8192);
    CHECK(big.view.h <= 8192);
    CHECK(big.resample.scale > 0.7);
}

TEST_CASE("resample pass: odd scales, unequal offsets, non-square, a dirty target (synthetic)") {
    auto ctx = device_or_skip();
    if (!ctx) {
        return;
    }
    const EmptyScene es = empty_scene();
    const render::Scene& scene = *es.scene;
    gpu::GpuLimits limits;
    limits.wave_simulation = false;
    auto made = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    gpu::GpuFrame& g = *made.value();
    constexpr std::size_t kGroup = 1;
    render::FrameRenderer fr(1, false);
    fr.insert_chunk({kGroup, 0, 0}, noise_land_chunk(kGroup, 0, 0));
    // The composed grid of `view` (a plain render), settled.
    auto compose = [&](const render::Rect& view) {
        const auto w = static_cast<std::uint32_t>(view.w), h = static_cast<std::uint32_t>(view.h);
        const auto t = g.offscreen_target(w, h);
        for (int f = 0; f < 8; ++f) {
            g.render(fr, scene, view, kGroup, 0, render::WaterStyle::Original, t.second.get());
        }
        return g.read_back(t.first.get(), w, h);
    };
    // The shader's arithmetic on the CPU (float, the same operations).
    struct Case {
        gpu::Resample rs;
        std::uint32_t ow, oh;
        render::Rect view;
    };
    auto expect = [&](const Case& c, const std::vector<std::uint8_t>& grid, std::uint32_t x,
                      std::uint32_t y, std::size_t k) {
        const int gw = c.view.w, gh = c.view.h;
        auto texel = [&](int tx, int ty) {
            tx = std::clamp(tx, 0, gw - 1);
            ty = std::clamp(ty, 0, gh - 1);
            return static_cast<double>(
                grid[(static_cast<std::size_t>(ty) * static_cast<std::size_t>(gw) +
                      static_cast<std::size_t>(tx)) *
                         4 +
                     k]);
        };
        const auto s = static_cast<float>(c.rs.scale);
        const float sx = static_cast<float>(c.rs.offset_x) + (static_cast<float>(x) + 0.5F) / s;
        const float sy = static_cast<float>(c.rs.offset_y) + (static_cast<float>(y) + 0.5F) / s;
        if (c.rs.filter == gpu::ResampleFilter::Bilinear) {
            const float fx = sx - 0.5F, fy = sy - 0.5F;
            const int ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy));
            const double tx = static_cast<double>(fx) - ix, ty = static_cast<double>(fy) - iy;
            const double top = texel(ix, iy) * (1 - tx) + texel(ix + 1, iy) * tx;
            const double bottom = texel(ix, iy + 1) * (1 - tx) + texel(ix + 1, iy + 1) * tx;
            return top * (1 - ty) + bottom * ty;
        }
        if (c.rs.filter == gpu::ResampleFilter::Box) {
            const int n = std::clamp(static_cast<int>(std::ceil(1.0F / s)), 1, 8);
            const float span = 1.0F / s;
            double sum = 0;
            for (int j = 0; j < n; ++j) {
                for (int i = 0; i < n; ++i) {
                    const float dx = (static_cast<float>(i) + 0.5F) / static_cast<float>(n) - 0.5F;
                    const float dy = (static_cast<float>(j) + 0.5F) / static_cast<float>(n) - 0.5F;
                    sum += texel(static_cast<int>(std::floor(sx + dx * span)),
                                 static_cast<int>(std::floor(sy + dy * span)));
                }
            }
            return sum / (n * n);
        }
        return texel(static_cast<int>(std::floor(sx)), static_cast<int>(std::floor(sy)));
    };
    // Renders `c` into a target first filled with an unrelated picture (a stale
    // target must not show) and compares every channel, alpha included. The float
    // arithmetic of the GPU may differ from the CPU's by an ulp, which moves a
    // sample across a pixel edge in a handful of pixels: those are counted, and
    // bounded, not forbidden.
    auto run = [&](const Case& c, double allowed_bad_fraction, const char* name) {
        const auto grid = compose(c.view);
        const auto target = g.offscreen_target(c.ow, c.oh);
        g.render(fr, scene, render::Rect{200, 200, static_cast<int>(c.ow), static_cast<int>(c.oh)},
                 kGroup, 0, render::WaterStyle::Original, target.second.get());
        g.render(fr, scene, c.view, kGroup, 0, render::WaterStyle::Original, target.second.get(),
                 &c.rs);
        const auto out = g.read_back(target.first.get(), c.ow, c.oh);
        REQUIRE(out.size() == std::size_t{c.ow} * c.oh * 4);
        std::size_t bad = 0;
        double sum = 0;
        for (std::uint32_t y = 0; y < c.oh; ++y) {
            for (std::uint32_t x = 0; x < c.ow; ++x) {
                // The output is 8 bit: the mean of the expected values is rounded, so a
                // channel is right when it is within one level of the exact value.
                double worst = 0;
                for (std::size_t k = 0; k < 4; ++k) {
                    const double want = expect(c, grid, x, y, k);
                    worst =
                        std::max(worst, std::abs(out[(std::size_t{y} * c.ow + x) * 4 + k] - want));
                }
                bad += worst > 1.0 + 1e-6 ? 1U : 0U;
                sum += worst;
            }
        }
        const double total = static_cast<double>(c.ow) * c.oh;
        MESSAGE(name << ": " << bad << " of " << total << " pixels off by more than 1");
        CHECK(static_cast<double>(bad) <= allowed_bad_fraction * total);
        CHECK(sum / total < 0.6);
        CHECK(g.errors() == 0);
    };
    // Odd scales: magnifying 1.37 (nearest and bilinear), reducing 0.63 (box,
    // 2 taps over a span of 1.587), 0.3 (box, 4 taps), 1/3 (3 taps).
    {
        auto v = gpu::resampled_view(40.3, 38.6, 96, 40, 1.37, gpu::ResampleFilter::Nearest);
        run({v.resample, 96, 40, v.view}, 0.01, "1.37 nearest, non-square output");
        v = gpu::resampled_view(40.3, 38.6, 96, 40, 1.37, gpu::ResampleFilter::Bilinear);
        run({v.resample, 96, 40, v.view}, 0.01, "1.37 bilinear");
        v = gpu::resampled_view(120.4, 90.2, 70, 50, 0.63, gpu::ResampleFilter::Box);
        run({v.resample, 70, 50, v.view}, 0.01, "0.63 box");
        v = gpu::resampled_view(150.0, 150.0, 60, 40, 0.3, gpu::ResampleFilter::Box);
        run({v.resample, 60, 40, v.view}, 0.01, "0.3 box (4 taps)");
        v = gpu::resampled_view(150.0, 150.0, 60, 40, 1.0 / 3.0, gpu::ResampleFilter::Box);
        run({v.resample, 60, 40, v.view}, 0.01, "1/3 box (3 taps)");
    }
    // Unequal offsets at scale 2: x and y must not be swapped; a grid that is not
    // square (70 x 50) read to an output that is not (90 x 30).
    {
        gpu::Resample rs;
        rs.scale = 2.0;
        rs.offset_x = 0.75;
        rs.offset_y = 0.25;
        rs.filter = gpu::ResampleFilter::Nearest;
        run({rs, 90, 30, render::Rect{3, 5, 70, 50}}, 0.002, "offsets (0.75, 0.25) at scale 2");
        rs.filter = gpu::ResampleFilter::Bilinear;
        run({rs, 90, 30, render::Rect{3, 5, 70, 50}}, 0.002, "offsets (0.75, 0.25), bilinear");
    }
    // The pass runs for an offset that is almost nothing (not the identity).
    {
        gpu::Resample rs;
        rs.offset_x = 1e-9;
        REQUIRE_FALSE(rs.identity());
        run({rs, 40, 40, render::Rect{0, 0, 42, 42}}, 0.002, "offset 1e-9");
    }
}