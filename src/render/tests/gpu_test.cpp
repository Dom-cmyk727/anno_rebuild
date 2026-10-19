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
#include <memory>
#include <optional>
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
    f.ship_water = true;
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
    CHECK(f32_at(56) == 1.0F);
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
