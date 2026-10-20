// bench_frame: runs the viewer's frame pipeline (render::FrameRenderer)
// headless along scripted camera paths and prints frame-time percentiles and
// the time per stage as one Markdown table (method: docs/perf/README.md).
//
//   bench_frame <game_dir> <file.sze|.scp> [--threads n]
//       [--res 1920x1080,2560x1440,3840x2160] [--groups 0,1,2]
//       [--styles original,enhanced] [--chunks blocking,async]
//       [--renderer cpu,gpu] [--check] [--island n] [--at dx,dy] [--t0 ms] [--warmup ms] [--quick]
//       [--zoom k] [--zoom-frames n] [--pace ms] [--spike ms] [--limits default|interactive]
//       [--settle ms] [--river-flow legacy|flow] [--river-quality auto|low|medium|high]
//       [--river-pass] [--stand-ins on|off] [--prewarm chunks] [--zoom-dump <dir>]
//       [--zoom-dump-after a,b,...] [--zoom-pan dx,dy[,frames]] [--pop on|off]
//   bench_frame --list-adapters
//   bench_frame <game_dir> <file> --river-cost      (GPU cost of the river shading, synthetic)
//
// Per resolution, zoom group, water style and chunk mode, from a cold cache
// centred on the largest island: cold (1 frame), static (120), pan slow (240,
// 6 × 3 px per frame), pan fast (120, 48 × 24 px). The animation clock
// advances 16 ms per frame; frames run back to back. Same paths and table as
// the Rust benchmark (crates/render/examples/bench_frame.rs), except that the
// scene is loaded as the viewer loads it (stand-ins, ships, soldiers).
//
// `--renderer gpu` composes on the graphics card (render::gpu::GpuFrame, built
// with ANNO_BUILD_GPU) into an offscreen target; frame time then includes
// waiting for the GPU. `--check` compares the last static frame of each
// configuration with the CPU path pixel by pixel and prints the result to
// stderr (BENCH_DUMP=<dir> also writes both images as raw RGBA).
//
// `--zoom k` replaces the paths above by a zoom path: after a static warm-up
// the zoom group changes every k frames (2, 1, 0, 1, 2, ... like the mouse
// wheel; the scene point at the window centre stays fixed) for `--zoom-frames`
// frames (default 240), each frame paced to at least `--pace` ms (default 2,
// a 500 Hz display) so that the chunk workers get real time. Frames slower
// than `--spike` ms (default 8) are listed with their phase breakdown. The
// frame time of this path is the window thread's CPU time (the viewer does
// not wait for the GPU); the GPU wait is listed separately. `--zoom 0` with
// `--zoom-pan dx,dy` runs the same path as a plain pan without zoom steps (the
// line "chunks dissolving in after the first second" should then say 0).
//
// `--stand-ins off` draws chunks that are not built yet as open sea (the
// behaviour before the stand-in chunks of OQ-077); the default is the library's
// (on for the GPU compositor, off for the CPU path). `--zoom-dump <dir>` (GPU)
// writes the first frames after zoom steps as raw RGBA (`zoom_<n>_g<group>_<w>x<h>.rgba`)
// (`--zoom-dump-after a,b` writes the frames a and b frames after each of the first
// steps instead of the step's own frame, named `zoom_<step>_f<frames>_g<group>_...`)
// and prints, after the table, how many frames had chunks still missing and
// how many of those were covered by stand-ins.
//
// `--river-cost` prints the GPU cost per million river pixels of the river flow
// shading at each tier (GpuFrame::measure_river_cost: a synthetic 1024 x 1024
// view of pure river pixels). `--river-pass` times the sea pass of the real
// frame with the legacy shimmer and with the flow shading (GPU timestamps) and
// divides the difference by the visible river pixels. The adapter is chosen by
// ANNO_GPU_ADAPTER (a number of --list-adapters or a part of its name).
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <anno/app/loader.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>
#ifdef ANNO_BENCH_GPU
#include <fstream>

#include <anno/render/gpu.hpp>
#endif

namespace {

namespace fs = std::filesystem;
namespace render = anno::render;

struct Options {
    std::size_t threads = std::max(1U, std::thread::hardware_concurrency());
    std::vector<std::pair<int, int>> resolutions = {{1920, 1080}, {2560, 1440}, {3840, 2160}};
    std::vector<std::size_t> groups = {0, 1, 2};
    std::vector<render::WaterStyle> styles = {render::WaterStyle::Original,
                                              render::WaterStyle::Enhanced};
    /// false = blocking, true = async.
    std::vector<bool> modes = {false, true};
    /// false = CPU, true = GPU.
    std::vector<bool> renderers = {false};
    bool check = false;
    std::pair<int, int> at{0, 0};
    std::optional<std::size_t> island;
    std::uint64_t t0 = 0, warmup = 0;
    bool quick = false;
    std::size_t zoom_every = 0, zoom_frames = 240;
    /// Zoom path: the view moves by this many scene pixels (of the group shown) per
    /// frame, reversing every zoom_pan_period frames: panning while zooming
    /// (--zoom-pan dx,dy).
    std::pair<int, int> zoom_pan{0, 0};
    /// Zoom path (GPU): compare consecutive frames of one group without a pan and
    /// report how many pixels jump (--pop): the pop of a chunk swap.
    bool pop = false;
    /// Frames the pan walks one way before it reverses (--zoom-pan dx,dy[,frames]).
    std::size_t zoom_pan_period = 400;
    double pace_ms = 2.0, spike_ms = 8.0;
    /// GpuLimits::interactive() instead of the defaults.
    bool interactive = false;
    /// Render paced frames for this many ms before the paths (background
    /// work such as the wave simulation window needs wall-clock time).
    std::size_t settle_ms = 0;
    /// Enhanced rivers: the shading of the CPU path (--river-flow legacy|flow).
    render::RiverFlowMode river_flow = render::RiverFlowMode::Flow;
    /// GpuLimits::river_quality (nullopt = automatic).
    std::optional<render::RiverQuality> river_quality;
    /// RiverConfig::light_gain (--river-gain): 1 = calibrated, larger shows the flow.
    float river_gain = 1.0F;
    bool river_cost = false, river_pass = false;
    /// GpuLimits::stand_ins / FrameRenderer::stand_ins (nullopt = library defaults).
    std::optional<bool> stand_ins;
    /// GpuLimits::prewarm_chunks (nullopt = the limits' default).
    std::optional<std::size_t> prewarm;
    /// GpuLimits::prefetch_chunks (nullopt = the limits' default).
    std::optional<std::size_t> prefetch;
    /// GpuLimits::chunk_fade_ms (nullopt = the limits' default).
    std::optional<std::uint64_t> fade;
    /// FrameRenderer::max_lookahead_chunks (nullopt = the library default).
    std::optional<std::size_t> lookahead;
    /// Directory for the frames after zoom steps (empty = none).
    std::string zoom_dump;
    /// Frames after a zoom step that --zoom-dump writes (0 = the step's own frame).
    std::vector<std::size_t> zoom_dump_after{0};
};

std::vector<std::string_view> split(std::string_view s, char sep) {
    std::vector<std::string_view> out;
    for (std::size_t start = 0;;) {
        const std::size_t k = s.find(sep, start);
        out.push_back(
            s.substr(start, k == std::string_view::npos ? std::string_view::npos : k - start));
        if (k == std::string_view::npos) {
            return out;
        }
        start = k + 1;
    }
}

template <class T> bool parse(std::string_view s, T& v) {
    const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    return ec == std::errc() && p == s.data() + s.size();
}

/// One measured frame: total, chunk work and the four CPU stages.
struct Row {
    double total = 0;
    std::size_t built = 0;
    double chunk_ms = 0;
    std::size_t missing = 0;
    std::array<double, 4> stages{};
    /// Zoom path: group, GPU wait and the compositor's phase timers.
    std::size_t group = 0;
    double wait = 0;
    double sea = 0, sync = 0, decode = 0, atlas = 0, fluid = 0, encode = 0, sprites = 0;
    std::size_t uploaded = 0, sprites_new = 0;
    double create = 0, write = 0;
    std::size_t frame_no = 0;
    /// Visible chunks drawn from another group's chunks (GPU).
    std::size_t stand = 0;
    /// Visible chunks that showed open sea: none built, no source in any group (GPU).
    std::size_t open = 0, open_built = 0;
    /// Visible chunks drawn dissolving in over their placeholder (GPU).
    std::size_t fading = 0;
    /// Pixels of the view showing open sea (GpuStats::open_sea_px).
    std::uint64_t open_px = 0;
    /// The CPU chunk cache after the frame: chunks and bytes (outside `total`).
    std::size_t cache_chunks = 0, cache_bytes = 0;
    /// GPU: every window-thread step of the frame (GpuStats::steps, they add up
    /// to the compositor's total), and the counts behind the sync steps.
    std::vector<std::pair<std::string, double>> steps;
    std::size_t dropped = 0, planes_freed = 0, placeholders = 0, planes_warm = 0;
    bool pieces_realloc = false;
    /// Zoom path: wave-window job in the background; wall time since the path started.
    bool fluid_setting_up = false;
    double wall_ms = 0;
};

struct Config {
    int w = 0, h = 0;
    std::size_t group = 0;
    render::WaterStyle style = render::WaterStyle::Original;
    bool async_chunks = false;
    std::pair<int, int> centre{0, 0};
    std::size_t frames = 120;
    std::uint64_t t0 = 0, warmup = 0;
    std::size_t threads = 1;
    bool check = false;
    std::size_t zoom_every = 0, zoom_frames = 240;
    /// Zoom path: the view moves by this many scene pixels (of the group shown) per
    /// frame, reversing every zoom_pan_period frames: panning while zooming
    /// (--zoom-pan dx,dy).
    std::pair<int, int> zoom_pan{0, 0};
    /// Zoom path (GPU): compare consecutive frames of one group without a pan and
    /// report how many pixels jump (--pop): the pop of a chunk swap.
    bool pop = false;
    /// Frames the pan walks one way before it reverses (--zoom-pan dx,dy[,frames]).
    std::size_t zoom_pan_period = 400;
    double pace_ms = 2.0, spike_ms = 8.0;
    std::size_t settle_ms = 0;
    render::RiverFlowMode river_flow = render::RiverFlowMode::Flow;
    bool river_pass = false;
    std::optional<bool> stand_ins;
    std::optional<std::size_t> lookahead;
    std::string zoom_dump;
    /// Frames after a zoom step that --zoom-dump writes (0 = the step's own frame).
    std::vector<std::size_t> zoom_dump_after{0};
};

void report(const Config& c, const char* renderer, const char* mode, const char* name,
            const std::vector<Row>& s) {
    std::vector<double> total;
    for (const auto& r : s) {
        total.push_back(r.total);
    }
    std::sort(total.begin(), total.end());
    auto pct = [&](double p) {
        return total[static_cast<std::size_t>(
            std::lround((static_cast<double>(total.size()) - 1.0) * p))];
    };
    const double n = static_cast<double>(s.size());
    auto mean = [&](auto f) {
        double sum = 0;
        for (const auto& r : s) {
            sum += f(r);
        }
        return sum / n;
    };
    std::size_t built = 0, within = 0;
    for (const auto& r : s) {
        built += r.built;
        within += r.total <= 16.7 ? 1U : 0U;
    }
    std::printf(
        "| %d×%d | %zu | %s | %s | %s | %s | %zu | %.1f | %.1f | %.1f | %.1f | %.0f %% | %zu | "
        "%.1f | %.1f | %.1f | %.1f | %.1f | %.1f |\n",
        c.w, c.h, c.group, c.style == render::WaterStyle::Original ? "original" : "enhanced",
        renderer, mode, name, s.size(), pct(0.5), pct(0.95), pct(0.99), total.back(),
        100.0 * static_cast<double>(within) / n, built,
        mean([](const Row& r) { return r.chunk_ms; }),
        mean([](const Row& r) { return static_cast<double>(r.missing); }),
        mean([](const Row& r) { return r.stages[0]; }),
        mean([](const Row& r) { return r.stages[1]; }),
        mean([](const Row& r) { return r.stages[2]; }),
        mean([](const Row& r) { return r.stages[3]; }));
}

#ifdef ANNO_BENCH_GPU
namespace gpu = render::gpu;

/// Compares the GPU image of `view` with the CPU path (blocking chunks).
/// `missing`: chunks the GPU frame still lacked (async mode), which then
/// show open sea and differ by design.
void compare(const render::Scene& scene, const Config& c, render::Rect view, std::uint64_t time,
             gpu::GpuFrame& g, render::FrameRenderer& gpu_frames, WGPUTexture tex,
             WGPUTextureView tex_view, std::size_t missing) {
    const auto w = static_cast<std::uint32_t>(view.w), h = static_cast<std::uint32_t>(view.h);
    const std::vector<std::uint8_t> gpu_px = g.read_back(tex, w, h);
    const char* style = c.style == render::WaterStyle::Original ? "Original" : "Enhanced";
    if (gpu_px.size() != std::size_t{w} * h * 4) {
        std::fprintf(stderr, "check %dx%d group %zu %s: GPU read-back failed\n", view.w, view.h,
                     c.group, style);
        return;
    }
    render::FrameRenderer cpu(c.threads);
    // The CPU path of the same river look (rivers.md S6: the GPU shades the
    // flow lattice itself).
    cpu.river_flow = c.river_flow;
    // ... and of the tier the GPU used (Low and Medium differ from High by design).
    cpu.river_quality = g.river_quality();
    std::vector<std::uint32_t> out(std::size_t{w} * h);
    cpu.render(scene, view, c.group, time, c.style, out.data());
    // The same frame in the other river look on both paths: the CPU path
    // quantises to 8 bits after every step of the sea (sprite, openness, shading)
    // where the GPU keeps floats, so every sea pixel of the two differs by a
    // level or two whatever the river look (the legacy look has the same offset).
    // What the flow shading adds on each path (flow minus legacy) is compared
    // too: that difference of differences leaves out the offset.
    std::vector<std::uint32_t> out_other;
    std::vector<std::uint8_t> gpu_other;
    if (c.style == render::WaterStyle::Enhanced) {
        const auto other = c.river_flow == render::RiverFlowMode::Flow
                               ? render::RiverFlowMode::Legacy
                               : render::RiverFlowMode::Flow;
        cpu.river_flow = other;
        out_other.resize(out.size());
        cpu.render(scene, view, c.group, time, c.style, out_other.data());
        gpu_frames.river_flow = other;
        g.render(gpu_frames, scene, view, c.group, time, c.style, tex_view);
        g.wait();
        gpu_other = g.read_back(tex, w, h);
        gpu_frames.river_flow = c.river_flow;
        cpu.river_flow = c.river_flow;
    }
    std::size_t differ = 0;
    int max = 0;
    // River pixels (field flag bit 1): pure river (shore byte >= 254: no blend
    // into the sea's look, no wave simulation, no 4/2/2 shading grid on the CPU)
    // and the blend zone; counted pixels, those off by more than 2 and the
    // largest difference.
    struct RiverStat {
        std::size_t n = 0, over2 = 0;
        int max = 0;
        /// Pixels per difference 0, 1, 2, 3, 4 and more.
        std::array<std::size_t, 6> hist{};
    } pure, blend, shading, shading_covered;
    std::size_t covered = 0;
    // What the flow shading adds (flow minus legacy) on each path at pixel i, the largest
    // channel difference of the two.
    auto shading_diff = [&](std::size_t i) {
        int e = 0;
        for (int k = 0; k < 3; ++k) {
            const auto ku = static_cast<std::size_t>(k);
            const int d_cpu = static_cast<int>((out[i] >> (16 - 8 * k)) & 0xFF) -
                              static_cast<int>((out_other[i] >> (16 - 8 * k)) & 0xFF);
            const int d_gpu = int{gpu_px[4 * i + ku]} - int{gpu_other[4 * i + ku]};
            e = std::max(e, std::abs(d_gpu - d_cpu));
        }
        return e;
    };
    const bool have_other = !out_other.empty() && gpu_other.size() == gpu_px.size();
    for (std::size_t i = 0; i < out.size(); ++i) {
        int m = 0;
        for (int k = 0; k < 3; ++k) {
            const int cpu_v = static_cast<int>((out[i] >> (16 - 8 * k)) & 0xFF);
            m = std::max(m, std::abs(cpu_v - int{gpu_px[4 * i + static_cast<std::size_t>(k)]}));
        }
        max = std::max(max, m);
        differ += m > 2 ? 1U : 0U;
        const int sx = view.x + static_cast<int>(i % w), sy = view.y + static_cast<int>(i / w);
        const int cx = render::div_euclid(sx, render::kChunk),
                  cy = render::div_euclid(sy, render::kChunk);
        const render::Chunk* ch = cpu.chunk({c.group, cx, cy});
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
        // Pixels under an object (trees, rocks, ...): the GPU sinks objects that
        // stand in water into it (object_in_water), the CPU path does not.
        if (!ch->objects.empty() && ch->objects[f / 4][3] != 0) {
            ++covered;
            if (have_other && ch->field[f + 1] >= 254) {
                // The water under the object is shaded the same on both paths even though the
                // GPU sinks the object: the shading alone can be compared there too.
                const int e = shading_diff(i);
                ++shading_covered.n;
                shading_covered.over2 += e > 2 ? 1U : 0U;
                shading_covered.max = std::max(shading_covered.max, e);
                ++shading_covered.hist[static_cast<std::size_t>(std::min(e, 5))];
            }
            continue;
        }
        RiverStat& rs = ch->field[f + 1] >= 254 ? pure : blend;
        ++rs.n;
        rs.over2 += m > 2 ? 1U : 0U;
        rs.max = std::max(rs.max, m);
        ++rs.hist[static_cast<std::size_t>(std::min(m, 5))];
        if (&rs == &pure && have_other) {
            const int e = shading_diff(i);
            ++shading.n;
            shading.over2 += e > 2 ? 1U : 0U;
            shading.max = std::max(shading.max, e);
            ++shading.hist[static_cast<std::size_t>(std::min(e, 5))];
        }
    }
    auto river_line = [](const char* what, const RiverStat& s) {
        if (s.n == 0) {
            return;
        }
        std::fprintf(
            stderr, "  %s river pixels: %zu, %.3f %% within 2/255 (%zu off by more), max %d\n",
            what, s.n, 100.0 * (1.0 - static_cast<double>(s.over2) / static_cast<double>(s.n)),
            s.over2, s.max);
        std::fprintf(stderr,
                     "    difference 0 / 1 / 2 / 3 / 4 / more: %zu / %zu / %zu / %zu / %zu / %zu\n",
                     s.hist[0], s.hist[1], s.hist[2], s.hist[3], s.hist[4], s.hist[5]);
    };
    if (const char* dir = std::getenv("BENCH_DUMP")) {
        const std::string name = "g" + std::to_string(c.group) + "_" + style;
        std::vector<std::uint8_t> cpu_rgba;
        cpu_rgba.reserve(out.size() * 4);
        for (const std::uint32_t p : out) {
            cpu_rgba.insert(cpu_rgba.end(),
                            {static_cast<std::uint8_t>(p >> 16), static_cast<std::uint8_t>(p >> 8),
                             static_cast<std::uint8_t>(p), 255});
        }
        auto write = [](const fs::path& path, const std::vector<std::uint8_t>& bytes) {
            std::ofstream f(path, std::ios::binary);
            f.write(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
        };
        write(fs::path(dir) / (name + "_cpu.rgba"), cpu_rgba);
        write(fs::path(dir) / (name + "_gpu.rgba"), gpu_px);
    }
    std::fprintf(stderr,
                 "check %dx%d group %zu %s: %zu of %zu pixels differ by more than 2, max "
                 "difference %d%s; %llu GPU errors\n",
                 view.w, view.h, c.group, style, differ, out.size(), max,
                 missing > 0 ? " (chunks still missing on the GPU frame: not comparable)" : "",
                 static_cast<unsigned long long>(g.errors()));
    river_line("pure", pure);
    river_line("blend-zone", blend);
    river_line("shading-only (flow minus legacy, both paths), pure", shading);
    river_line("shading-only, under objects, pure", shading_covered);
    if (covered > 0) {
        std::fprintf(stderr,
                     "  (%zu river pixels under objects are left out of the whole-pixel "
                     "statistics: the GPU sinks objects standing in water, the CPU path does "
                     "not)\n",
                     covered);
    }
}

/// The sea pass of the real frame with the legacy shimmer and with the flow
/// shading, timed with GPU timestamps; the difference per million visible river
/// pixels is the cost of the shading on this scene and device.
void river_pass(const render::Scene& scene, const Config& c, gpu::GpuFrame& g,
                render::FrameRenderer& fr, WGPUTextureView target, render::Rect view) {
    std::uint64_t t = c.t0;
    // Every visible chunk on the GPU (uploads are spread over frames).
    for (int i = 0; i < 80; ++i) {
        g.render(fr, scene, view, c.group, t, render::WaterStyle::Enhanced, target);
        t += 16;
    }
    g.wait();
    // Visible river pixels (field flag bit 1) from the CPU chunks.
    std::size_t river_px = 0;
    std::size_t river_chunks = 0;
    for (const auto& [cx, cy] : render::visible_chunks(view)) {
        const render::Chunk* ch = fr.chunk({c.group, cx, cy});
        const auto r = render::chunk_rect(cx, cy).intersect(view);
        if (ch == nullptr || ch->field.empty() || !r) {
            continue;
        }
        river_chunks += ch->flow.empty() ? 0U : 1U;
        for (int y = r->y; y < r->y + r->h; ++y) {
            for (int x = r->x; x < r->x + r->w; ++x) {
                const std::size_t f = (static_cast<std::size_t>(y - cy * render::kChunk) *
                                           static_cast<std::size_t>(render::kChunk) +
                                       static_cast<std::size_t>(x - cx * render::kChunk)) *
                                      4;
                river_px += (ch->field[f + 3] & 2U) != 0 ? 1U : 0U;
            }
        }
    }
    auto sea_ms = [&](render::RiverFlowMode mode) -> std::optional<double> {
        fr.river_flow = mode;
        std::vector<double> v;
        for (int i = 0; i < 25; ++i) {
            g.render(fr, scene, view, c.group, t, render::WaterStyle::Enhanced, target);
            t += 16;
            if (const auto ms = g.sea_pass_ms(); ms && i >= 3) {
                v.push_back(*ms);
            }
        }
        if (v.empty()) {
            return std::nullopt;
        }
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    std::printf(
        "| view | group | tier | river px | chunks with flow | sea pass legacy ms | flow ms | "
        "difference ms | per million river pixels |\n|---|---|---|---|---|---|---|---|---|\n");
    for (int round = 0; round < 2; ++round) {
        const auto legacy = sea_ms(render::RiverFlowMode::Legacy);
        const auto flow = sea_ms(render::RiverFlowMode::Flow);
        const char* tier = g.river_quality() == render::RiverQuality::High     ? "high"
                           : g.river_quality() == render::RiverQuality::Medium ? "medium"
                                                                               : "low";
        if (!legacy || !flow) {
            std::printf(
                "| %dx%d | %zu | %s | %zu | %zu | no GPU timestamps on this device | | | |\n",
                view.w, view.h, c.group, tier, river_px, river_chunks);
            return;
        }
        const double mpx = static_cast<double>(river_px) / 1e6;
        std::printf("| %dx%d | %zu | %s | %zu | %zu | %.3f | %.3f | %.3f | %.2f |\n", view.w,
                    view.h, c.group, tier, river_px, river_chunks, *legacy, *flow, *flow - *legacy,
                    mpx > 0 ? (*flow - *legacy) / mpx : 0.0);
    }
}
#else
namespace gpu {
/// Stand-in when built without the GPU compositor (never instantiated).
struct GpuFrame {};
} // namespace gpu
#endif

/// The zoom path (see the header): wheel-like group switches every
/// c.zoom_every frames, paced frames, a list of the slow frames.
template <class Frame, class Ready, class Dump, class Grab>
void run_zoom(const Config& c, Frame&& frame, Ready&& ready, Dump&& dump, Grab&& grab,
              std::size_t& group, std::pair<int, int> cam, std::uint64_t time) {
    using SC = std::chrono::steady_clock;
    // Warm-up: static frames until the first group is built. The first
    // frames carry the one-time costs (pipelines, first uploads); slow ones
    // are listed after the table.
    std::vector<Row> warm;
    for (std::size_t i = 0; i < 60; ++i) {
        Row r = frame(render::Rect{cam.first, cam.second, c.w, c.h}, time, true);
        r.total -= r.wait;
        r.frame_no = i;
        warm.push_back(r);
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
        time += 8;
    }
    // Group sequence as the wheel: 2, 1, 0, 1, 2, ...
    static constexpr std::array<std::size_t, 4> kCycle = {2, 1, 0, 1};
    std::size_t at = 0;
    while (kCycle[at] != group) {
        at = (at + 1) % kCycle.size();
    }
    std::vector<Row> rows;
    const auto start = SC::now();
    auto next = start;
    // As the viewer: a zoom step waits until the sea pattern of its group is
    // rendered and on the GPU (only in the first moments after the start).
    bool switch_due = false, switched = false;
    std::size_t deferred = 0, steps = 0, last_step = 0;
    std::vector<std::uint8_t> prev_px;
    std::size_t pop_frames = 0, pop_over = 0, pop_worst_at = 0;
    double pop_sum = 0, pop_worst = 0;
    bool any_step = false;
    for (std::size_t i = 0; i < c.zoom_frames; ++i) {
        switched = false;
        if (c.zoom_every > 0 && i > 0 && i % c.zoom_every == 0) {
            switch_due = true;
        }
        if (switch_due && !ready(kCycle[(at + 1) % kCycle.size()])) {
            ++deferred;
        } else if (switch_due) {
            switch_due = false;
            at = (at + 1) % kCycle.size();
            const std::size_t to = kCycle[at];
            // Keep the scene point at the window centre fixed.
            const auto ot = render::kTile[group], nt = render::kTile[to];
            const double cx = cam.first + c.w / 2.0, cy = cam.second + c.h / 2.0;
            const double nx = cx * nt.w / ot.w, ny = cy * nt.h / ot.h;
            cam = {static_cast<int>(std::lround(nx - c.w / 2.0)),
                   static_cast<int>(std::lround(ny - c.h / 2.0))};
            group = to;
            switched = true;
            any_step = true;
            last_step = i;
            ++steps;
        }
        // Panning while zooming: the view walks one way for zoom_pan_period frames,
        // then back.
        const int dir = (i / c.zoom_pan_period) % 2 == 0 ? 1 : -1;
        cam.first += dir * c.zoom_pan.first;
        cam.second += dir * c.zoom_pan.second;
        const render::Rect view{cam.first, cam.second, c.w, c.h};
        Row r = frame(view, time, true);
        if (c.pop) {
            // Consecutive frames of one group without a pan: what changes besides
            // the water's own motion is a chunk swap (the pop this measures).
            auto now_px = grab();
            if (!prev_px.empty() && now_px.size() == prev_px.size() && !switched && i > 30) {
                // The view moved by `pan` since the previous frame, so pixel (x, y) of
                // this frame is (x + pan_x, y + pan_y) of the previous one (the
                // overlap is compared; nothing else changed unless a chunk swapped).
                const int px_ = dir * c.zoom_pan.first, py_ = dir * c.zoom_pan.second;
                const int w = c.w, h = c.h;
                std::size_t big = 0, counted = 0;
                for (int y = std::max(0, -py_); y < std::min(h, h - py_); ++y) {
                    for (int x = std::max(0, -px_); x < std::min(w, w - px_); ++x) {
                        const std::size_t a = (static_cast<std::size_t>(y) * w + x) * 4;
                        const std::size_t b =
                            (static_cast<std::size_t>(y + py_) * w + (x + px_)) * 4;
                        int d = 0;
                        for (std::size_t q = 0; q < 3; ++q) {
                            d = std::max(d, std::abs(int{now_px[a + q]} - int{prev_px[b + q]}));
                        }
                        big += d > 40 ? 1U : 0U;
                        ++counted;
                    }
                }
                const double frac = 100.0 * static_cast<double>(big) /
                                    static_cast<double>(std::max<std::size_t>(counted, 1));
                ++pop_frames;
                pop_sum += frac;
                pop_over += frac > 0.5 ? 1U : 0U;
                if (frac > pop_worst) {
                    pop_worst = frac;
                    pop_worst_at = i;
                }
            }
            prev_px = std::move(now_px);
        }
        if (any_step && steps <= 8 &&
            std::find(c.zoom_dump_after.begin(), c.zoom_dump_after.end(), i - last_step) !=
                c.zoom_dump_after.end()) {
            dump(steps - 1, i - last_step, group);
        }
        // Window-thread time only: the viewer does not wait for the GPU (the
        // wait is still in the last stage column and in the `wait` column).
        r.total -= r.wait;
        r.frame_no = i;
        r.wall_ms = std::chrono::duration<double, std::milli>(SC::now() - start).count();
        rows.push_back(r);
        // Pace like a display: the window thread sleeps out the rest of the
        // frame interval (workers run meanwhile).
        next += std::chrono::duration_cast<SC::duration>(
            std::chrono::duration<double, std::milli>(c.pace_ms));
        std::this_thread::sleep_until(next);
        time = static_cast<std::uint64_t>(
                   std::chrono::duration<double, std::milli>(SC::now() - start).count()) +
               1000;
    }
    if (c.pop) {
        std::printf("swap pop: %zu frame pairs of one group compared, pixels changing by more "
                    "than 40 levels: mean %.3f %%, worst %.3f %% (frame %zu), %zu pairs above "
                    "0.5 %%\n",
                    pop_frames, pop_frames > 0 ? pop_sum / static_cast<double>(pop_frames) : 0.0,
                    pop_worst, pop_worst_at, pop_over);
    }
    report(c, "zoom", c.async_chunks ? "async" : "blocking",
           ("zoom/" + std::to_string(c.zoom_every)).c_str(), rows);
    auto slow = [&](const char* title, const std::vector<Row>& list) {
        std::printf("\n%s (> %.1f ms), %dx%d, %s, %s chunks:\n\n", title, c.spike_ms, c.w, c.h,
                    c.style == render::WaterStyle::Original ? "original" : "enhanced",
                    c.async_chunks ? "async" : "blocking");
        std::printf("| # | group | total | chunks | built | uploaded | sea | chunk sync | create | "
                    "write | sprites | decode | atlas | new sprites | fluid | encode | wait |\n");
        std::printf("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
        for (const auto& r : list) {
            if (r.total <= c.spike_ms) {
                continue;
            }
            std::printf("| %zu | %zu | %.1f | %.1f | %zu | %zu | %.1f | %.1f | %.1f | %.1f | %.1f "
                        "| %.1f | %.1f | %zu | %.1f | %.1f | %.1f |\n",
                        r.frame_no, r.group, r.total, r.chunk_ms, r.built, r.uploaded, r.sea,
                        r.sync, r.create, r.write, r.sprites, r.decode, r.atlas, r.sprites_new,
                        r.fluid, r.encode, r.wait);
        }
        // Every step of the slow frames (GPU): the steps add up to the compositor's
        // total, so the time of a spike is named here.
        bool any = false;
        for (const auto& r : list) {
            if (r.total <= c.spike_ms || r.steps.empty()) {
                continue;
            }
            if (!any) {
                std::printf("\nsteps of the slow frames (ms; steps below 0.05 left out; dropped "
                            "= GPU chunks dropped, freed = planes freed on this thread, "
                            "placeholders = chunks made without content, realloc = piece buffer "
                            "grown):\n\n");
                any = true;
            }
            double sum = 0;
            std::string line;
            char buf[96];
            for (const auto& [name, ms] : r.steps) {
                sum += ms;
                if (ms >= 0.05) {
                    std::snprintf(buf, sizeof buf, "%s%s %.2f", line.empty() ? "" : ", ",
                                  name.c_str(), ms);
                    line += buf;
                }
            }
            std::printf("- #%zu (group %zu, total %.1f, wait %.1f): %s; sum %.2f; dropped %zu, "
                        "freed %zu, placeholders %zu, warm planes %zu, realloc %s\n",
                        r.frame_no, r.group, r.total, r.wait, line.c_str(), sum, r.dropped,
                        r.planes_freed, r.placeholders, r.planes_warm,
                        r.pieces_realloc ? "yes" : "no");
        }
    };
    std::printf("\nzoom steps deferred until the target group was ready: %zu frames\n", deferred);
    {
        // Frames with visible chunks not built, and how many of those frames drew
        // stand-ins for some chunk (GPU; the frame right after a zoom step is the
        // one that used to show open sea).
        std::size_t missing = 0, covered = 0, after = 0, after_covered = 0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const bool first = i > 0 && rows[i].group != rows[i - 1].group;
            missing += rows[i].missing > 0 ? 1U : 0U;
            covered += rows[i].missing > 0 && rows[i].stand > 0 ? 1U : 0U;
            after += first && rows[i].missing > 0 ? 1U : 0U;
            after_covered += first && rows[i].missing > 0 && rows[i].stand > 0 ? 1U : 0U;
        }
        std::printf("frames with chunks not built: %zu of %zu (%zu drew stand-ins); first frames "
                    "after a zoom step with chunks not built: %zu (%zu drew stand-ins)\n",
                    missing, rows.size(), covered, after, after_covered);
        // Open sea in the view: part of a visible chunk that is not on the GPU and
        // has no source in any zoom group (by area: a hole in a stand-in counts).
        // The longest run of such frames, in frames and wall time, is what a
        // viewer sees as a patch that pops in.
        std::size_t open_frames = 0, run = 0, longest = 0, longest_at = 0, open_chunks = 0,
                    open_built = 0;
        std::uint64_t open_px = 0, open_px_worst = 0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].open > 0) {
                ++open_frames;
                open_chunks += rows[i].open;
                open_built += rows[i].open_built;
                open_px += rows[i].open_px;
                open_px_worst = std::max(open_px_worst, rows[i].open_px);
                if (++run > longest) {
                    longest = run;
                    longest_at = i;
                }
            } else {
                run = 0;
            }
        }
        const double longest_ms =
            longest > 0 ? rows[longest_at].wall_ms - rows[longest_at + 1 - longest].wall_ms : 0.0;
        std::printf("frames showing open sea (a visible chunk with no source in any group): %zu "
                    "of %zu, %zu chunk-frames in all (%zu built on the CPU, waiting for the "
                    "upload); longest run %zu frames / %.0f ms; open-sea pixels %llu in all, "
                    "%.2f %% of the view at worst\n",
                    open_frames, rows.size(), open_chunks, open_built, longest, longest_ms,
                    static_cast<unsigned long long>(open_px),
                    100.0 * static_cast<double>(open_px_worst) /
                        (static_cast<double>(c.w) * static_cast<double>(c.h)));
        // Dissolves after the first second (a cold start fills the view with them):
        // on a plain pan none should happen, the margin is uploaded ahead.
        std::size_t fade_frames = 0, fade_chunks = 0;
        for (const auto& r : rows) {
            if (r.wall_ms >= 1000.0 && r.fading > 0) {
                ++fade_frames;
                fade_chunks += r.fading;
            }
        }
        std::printf("frames with chunks dissolving in after the first second: %zu (%zu "
                    "chunk-frames)\n",
                    fade_frames, fade_chunks);
        // The resident CPU chunk cache (GPU path: sampled after every frame).
        std::size_t peak_bytes = 0, peak_chunks = 0, sum_bytes = 0;
        for (const auto& r : rows) {
            peak_bytes = std::max(peak_bytes, r.cache_bytes);
            peak_chunks = std::max(peak_chunks, r.cache_chunks);
            sum_bytes += r.cache_bytes;
        }
        if (peak_chunks > 0) {
            std::printf("CPU chunk cache: peak %.0f MB, mean %.0f MB, peak %zu chunks\n",
                        static_cast<double>(peak_bytes) / 1048576.0,
                        static_cast<double>(sum_bytes) / static_cast<double>(rows.size()) /
                            1048576.0,
                        peak_chunks);
        }
    }
    if (c.zoom_every > 0) {
        // How long each zoom step's transient lasts (wall ms since the step's frame):
        // chunks missing from the CPU cache, stand-in chunks drawn on the GPU (built
        // but not yet uploaded count too), and the wave window being prepared.
        std::printf("\ntransients per zoom step (frames / ms after the step until the last "
                    "frame that still had them):\n\n| step | to group | missing | stand-ins | "
                    "wave window |\n|---|---|---|---|---|\n");
        std::size_t step = 0;
        for (std::size_t i = 1; i < rows.size(); ++i) {
            if (rows[i].group == rows[i - 1].group) {
                continue;
            }
            auto last = [&](auto&& has) {
                std::size_t n = 0;
                bool any = false;
                for (std::size_t j = i; j < rows.size() && rows[j].group == rows[i].group; ++j) {
                    if (has(rows[j])) {
                        n = j - i + 1;
                        any = true;
                    }
                }
                char b[48];
                if (!any) {
                    return std::string("-");
                }
                std::snprintf(b, sizeof b, "%zu fr / %.0f ms", n,
                              rows[i + n - 1].wall_ms - rows[i].wall_ms);
                return std::string(b);
            };
            std::printf("| %zu | %zu | %s | %s | %s |\n", step++, rows[i].group,
                        last([](const Row& r) { return r.missing > 0; }).c_str(),
                        last([](const Row& r) { return r.stand > 0; }).c_str(),
                        last([](const Row& r) { return r.fluid_setting_up; }).c_str());
        }
    }
    slow("slow warm-up frames", warm);
    slow("slow frames of the zoom path", rows);
    std::printf("\n");
}

void run(const render::Scene& scene, const Config& c, gpu::GpuFrame* g) {
    // The GPU compositor draws the ships itself: chunks without them.
    render::FrameRenderer fr(c.threads, g == nullptr);
    fr.river_flow = c.river_flow;
    if (c.lookahead) {
        fr.max_lookahead_chunks = *c.lookahead;
    }
    if (c.stand_ins && g == nullptr) {
        fr.stand_ins = *c.stand_ins;
    }
    if (c.async_chunks) {
        fr.set_workers(std::make_unique<render::ChunkWorkers>(scene, (c.threads + 1) / 2));
    }
    std::vector<std::uint32_t> out(static_cast<std::size_t>(c.w) * static_cast<std::size_t>(c.h));
    std::pair<int, int> cam{c.centre.first - c.w / 2, c.centre.second - c.h / 2};
    std::uint64_t time = c.t0 > c.warmup ? c.t0 - c.warmup : 0;
    const render::Rect view0{cam.first, cam.second, c.w, c.h};
#ifdef ANNO_BENCH_GPU
    std::optional<std::pair<gpu::Texture, gpu::TextureView>> target;
    if (g != nullptr) {
        target =
            g->offscreen_target(static_cast<std::uint32_t>(c.w), static_cast<std::uint32_t>(c.h));
    }
#endif
    // One frame on the selected renderer; measured GPU frames wait for the
    // card (warm-up frames do not, as the Rust benchmark).
    std::size_t frame_group = c.group;
    auto frame = [&](render::Rect view, std::uint64_t t, bool wait_gpu) -> Row {
#ifdef ANNO_BENCH_GPU
        if (g != nullptr) {
            const auto st =
                g->render(fr, scene, view, frame_group, t, c.style, target->second.get());
            Row r{st.total_ms, st.chunks_built, st.chunks_ms, st.chunks_missing, {}};
            r.group = frame_group;
            r.stand = st.stand_in_chunks;
            r.open = st.open_sea_chunks;
            r.open_built = st.open_sea_built;
            r.fading = st.chunks_fading;
            r.open_px = st.open_sea_px;
            r.cache_chunks = fr.cached();
            r.cache_bytes = fr.cached_bytes();
            r.fluid_setting_up = st.fluid_setting_up;
            r.sea = st.sea_ms;
            r.sync = st.chunk_sync_ms;
            r.decode = st.decode_ms;
            r.atlas = st.atlas_ms;
            r.fluid = st.fluid_ms;
            r.encode = st.encode_ms;
            r.sprites = st.sprites_ms;
            r.uploaded = st.chunks_uploaded;
            r.sprites_new = st.sprites_new;
            r.create = st.upload_create_ms;
            r.write = st.upload_write_ms;
            for (const auto& step : st.steps()) {
                r.steps.emplace_back(step.name, step.ms);
            }
            r.dropped = st.chunks_dropped;
            r.planes_freed = st.planes_freed;
            r.placeholders = st.placeholders;
            r.planes_warm = st.planes_warm;
            r.pieces_realloc = st.pieces_realloc;
            if (!wait_gpu) {
                return r;
            }
            const auto w0 = std::chrono::steady_clock::now();
            g->wait();
            const double wait =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - w0)
                    .count();
            r.total = st.total_ms + wait;
            r.wait = wait;
            r.stages = {st.upload_ms, st.sprites_ms, st.encode_ms, wait};
            return r;
        }
#endif
        (void)wait_gpu;
        const auto st = fr.render(scene, view, frame_group, t, c.style, out.data());
        Row r{st.total_ms,
              st.chunks_built,
              st.chunks_ms,
              st.chunks_missing,
              {st.ground_ms, st.water_objects_ms, st.sea_ms, st.objects_ms}};
        r.group = frame_group;
        return r;
    };
    for (; time < c.t0; time += 16) {
        frame(view0, time, false);
    }
    // Paced frames in wall-clock time, so that background work finishes.
    for (std::size_t waited = 0; waited < c.settle_ms; waited += 8) {
        frame(view0, time, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
#ifdef ANNO_BENCH_GPU
    if (c.river_pass && g != nullptr) {
        river_pass(scene, c, *g, fr, target->second.get(), view0);
        fr.set_workers(nullptr);
        return;
    }
#endif
    // A pan without zoom steps (--zoom 0 --zoom-pan dx,dy) runs the same path.
    if (c.zoom_every > 0 || c.zoom_pan != std::pair{0, 0}) {
        // The viewer renders the sea patterns of all groups on background
        // threads from the start; so does the zoom path.
        std::vector<std::thread> seas;
        for (std::size_t grp = 0; grp < render::kGroups; ++grp) {
            seas.emplace_back([&scene, grp] { scene.sea(grp); });
        }
        auto ready = [&](std::size_t grp) {
#ifdef ANNO_BENCH_GPU
            if (g != nullptr) {
                return g->group_ready(scene, grp);
            }
#endif
            return scene.sea_built(grp);
        };
        // The frame as RGBA bytes (GPU), for --pop.
        auto grab = [&]() -> std::vector<std::uint8_t> {
#ifdef ANNO_BENCH_GPU
            if (g != nullptr) {
                return g->read_back(target->first.get(), static_cast<std::uint32_t>(c.w),
                                    static_cast<std::uint32_t>(c.h));
            }
#endif
            return {};
        };
        auto dump = [&](std::size_t n, std::size_t after, std::size_t grp) {
#ifdef ANNO_BENCH_GPU
            if (g == nullptr || c.zoom_dump.empty()) {
                return;
            }
            const auto px = g->read_back(target->first.get(), static_cast<std::uint32_t>(c.w),
                                         static_cast<std::uint32_t>(c.h));
            const std::string name = c.zoom_dump + "/zoom_" + std::to_string(n) + "_f" +
                                     std::to_string(after) + "_g" + std::to_string(grp) + "_" +
                                     std::to_string(c.w) + "x" + std::to_string(c.h) + ".rgba";
            std::ofstream f(name, std::ios::binary);
            f.write(reinterpret_cast<const char*>(px.data()),
                    static_cast<std::streamsize>(px.size()));
#else
            (void)n;
            (void)after;
            (void)grp;
#endif
        };
        run_zoom(c, frame, ready, dump, grab, frame_group, cam, time);
        for (auto& t : seas) {
            t.join();
        }
        fr.set_workers(nullptr);
        return;
    }
    struct Path {
        const char* name;
        std::size_t n;
        int dx, dy;
    };
    const std::array<Path, 4> paths = {{{"cold", 1, 0, 0},
                                        {"static", c.frames, 0, 0},
                                        {"pan slow", 2 * c.frames, 6, 3},
                                        {"pan fast", c.frames, 48, 24}}};
    for (const auto& p : paths) {
        std::vector<Row> rows;
        for (std::size_t i = 0; i < p.n; ++i) {
            const render::Rect view{cam.first, cam.second, c.w, c.h};
            rows.push_back(frame(view, time, true));
#ifdef ANNO_BENCH_GPU
            if (c.check && g != nullptr && std::string_view(p.name) == "static" && i + 1 == p.n) {
                compare(scene, c, view, time, *g, fr, target->first.get(), target->second.get(),
                        rows.back().missing);
            }
#endif
            time += 16;
            cam = {cam.first + p.dx, cam.second + p.dy};
        }
        report(c, g != nullptr ? "gpu" : "cpu", c.async_chunks ? "async" : "blocking", p.name,
               rows);
    }
    fr.set_workers(nullptr);
}

constexpr const char* kUsage =
    "usage: bench_frame <game_dir> <file> [--threads n] [--res WxH,..] [--groups 0,1,2] "
    "[--styles original,enhanced] [--chunks blocking,async] [--renderer cpu,gpu] [--check] "
    "[--island n] [--at dx,dy] [--t0 ms] [--warmup ms] [--quick] [--zoom k] [--zoom-frames n] "
    "[--pace ms] [--spike ms] [--limits default|interactive] [--settle ms] "
    "[--river-flow legacy|flow] [--river-quality auto|low|medium|high] [--river-gain x] "
    "[--river-pass] [--stand-ins on|off] [--prewarm chunks] [--prefetch chunks] [--fade ms] "
    "[--lookahead-chunks n] "
    "[--zoom-dump dir]\n"
    "       bench_frame --list-adapters\n"
    "       bench_frame <game_dir> <file> --river-cost\n";

std::optional<Options> parse_options(const std::vector<std::string>& args) {
    Options o;
    for (std::size_t i = 2; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--quick") {
            o.quick = true;
            continue;
        }
        if (a == "--check") {
            o.check = true;
            continue;
        }
        if (a == "--river-cost") {
            o.river_cost = true;
            continue;
        }
        if (a == "--river-pass") {
            o.river_pass = true;
            continue;
        }
        if (i + 1 >= args.size()) {
            std::fprintf(stderr, "%s needs a value\n", a.c_str());
            return std::nullopt;
        }
        const std::string_view v = args[++i];
        bool ok = true;
        if (a == "--threads") {
            ok = parse(v, o.threads) && o.threads > 0;
        } else if (a == "--res") {
            o.resolutions.clear();
            for (const auto r : split(v, ',')) {
                const auto wh = split(r, 'x');
                int w = 0, h = 0;
                ok = ok && wh.size() == 2 && parse(wh[0], w) && parse(wh[1], h) && w > 0 && h > 0;
                o.resolutions.emplace_back(w, h);
            }
        } else if (a == "--groups") {
            o.groups.clear();
            for (const auto g : split(v, ',')) {
                std::size_t n = 0;
                ok = ok && parse(g, n) && n < render::kGroups;
                o.groups.push_back(n);
            }
        } else if (a == "--styles") {
            o.styles.clear();
            for (const auto s : split(v, ',')) {
                ok = ok && (s == "original" || s == "enhanced");
                o.styles.push_back(s == "enhanced" ? render::WaterStyle::Enhanced
                                                   : render::WaterStyle::Original);
            }
        } else if (a == "--chunks") {
            o.modes.clear();
            for (const auto s : split(v, ',')) {
                ok = ok && (s == "blocking" || s == "async");
                o.modes.push_back(s == "async");
            }
        } else if (a == "--renderer") {
            o.renderers.clear();
            for (const auto s : split(v, ',')) {
                ok = ok && (s == "cpu" || s == "gpu");
                o.renderers.push_back(s == "gpu");
            }
#ifndef ANNO_BENCH_GPU
            if (ok &&
                std::find(o.renderers.begin(), o.renderers.end(), true) != o.renderers.end()) {
                std::fputs("--renderer gpu: built without the GPU compositor (ANNO_BUILD_GPU)\n",
                           stderr);
                ok = false;
            }
#endif
        } else if (a == "--island") {
            std::size_t n = 0;
            ok = parse(v, n);
            o.island = n;
        } else if (a == "--at") {
            const auto xy = split(v, ',');
            ok = xy.size() == 2 && parse(xy[0], o.at.first) && parse(xy[1], o.at.second);
        } else if (a == "--settle") {
            ok = parse(v, o.settle_ms);
        } else if (a == "--river-flow") {
            ok = v == "legacy" || v == "flow";
            o.river_flow =
                v == "legacy" ? render::RiverFlowMode::Legacy : render::RiverFlowMode::Flow;
        } else if (a == "--river-gain") {
            ok = parse(v, o.river_gain) && o.river_gain >= 0.0F;
        } else if (a == "--river-quality") {
            ok = v == "auto" || v == "low" || v == "medium" || v == "high";
            if (v == "low") {
                o.river_quality = render::RiverQuality::Low;
            } else if (v == "medium") {
                o.river_quality = render::RiverQuality::Medium;
            } else if (v == "high") {
                o.river_quality = render::RiverQuality::High;
            }
        } else if (a == "--stand-ins") {
            ok = v == "on" || v == "off";
            o.stand_ins = v == "on";
        } else if (a == "--lookahead-chunks") {
            std::size_t n = 0;
            ok = parse(v, n);
            o.lookahead = n;
        } else if (a == "--fade") {
            std::uint64_t n = 0;
            ok = parse(v, n);
            o.fade = n;
        } else if (a == "--prefetch") {
            std::size_t n = 0;
            ok = parse(v, n);
            o.prefetch = n;
        } else if (a == "--prewarm") {
            std::size_t n = 0;
            ok = parse(v, n);
            o.prewarm = n;
        } else if (a == "--zoom-dump") {
            ok = !v.empty();
            o.zoom_dump = std::string(v);
        } else if (a == "--zoom-dump-after") {
            o.zoom_dump_after.clear();
            std::string_view rest = v;
            ok = !rest.empty();
            while (ok && !rest.empty()) {
                const auto comma = rest.find(',');
                std::size_t n = 0;
                ok = parse(rest.substr(0, comma), n);
                o.zoom_dump_after.push_back(n);
                rest =
                    comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
            }
        } else if (a == "--limits") {
            ok = v == "default" || v == "interactive";
            o.interactive = v == "interactive";
        } else if (a == "--zoom") {
            // 0: no zoom steps (with --zoom-pan: a plain pan on the zoom path).
            ok = parse(v, o.zoom_every);
        } else if (a == "--zoom-frames") {
            ok = parse(v, o.zoom_frames) && o.zoom_frames > 0;
        } else if (a == "--pop") {
            ok = v == "on" || v == "off";
            o.pop = v == "on";
        } else if (a == "--zoom-pan") {
            const auto parts = split(v, ',');
            ok = (parts.size() == 2 || parts.size() == 3) && parse(parts[0], o.zoom_pan.first) &&
                 parse(parts[1], o.zoom_pan.second) &&
                 (parts.size() == 2 ||
                  (parse(parts[2], o.zoom_pan_period) && o.zoom_pan_period > 0));
        } else if (a == "--pace") {
            ok = parse(v, o.pace_ms);
        } else if (a == "--spike") {
            ok = parse(v, o.spike_ms);
        } else if (a == "--t0") {
            ok = parse(v, o.t0);
        } else if (a == "--warmup") {
            ok = parse(v, o.warmup);
        } else {
            ok = false;
        }
        if (!ok) {
            std::fprintf(stderr, "bad option %s %.*s\n", a.c_str(), static_cast<int>(v.size()),
                         v.data());
            return std::nullopt;
        }
    }
    return o;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
#ifdef ANNO_BENCH_GPU
    if (args.size() == 1 && args[0] == "--list-adapters") {
        const auto list = gpu::list_adapters();
        for (std::size_t i = 0; i < list.size(); ++i) {
            const auto& a = list[i];
            std::printf("%zu: %s (%s, %s, vendor 0x%04x)\n", i, a.name.c_str(),
                        a.backend == WGPUBackendType_Vulkan   ? "Vulkan"
                        : a.backend == WGPUBackendType_D3D12  ? "DirectX 12"
                        : a.backend == WGPUBackendType_Metal  ? "Metal"
                        : a.backend == WGPUBackendType_OpenGL ? "OpenGL"
                                                              : "other",
                        a.type == WGPUAdapterType_DiscreteGPU     ? "discrete"
                        : a.type == WGPUAdapterType_IntegratedGPU ? "integrated"
                        : a.type == WGPUAdapterType_CPU           ? "software"
                                                                  : "unknown",
                        a.vendor_id);
        }
        return list.empty() ? 1 : 0;
    }
#endif
    if (args.size() < 2) {
        std::fputs(kUsage, stderr);
        return 2;
    }
    const auto o = parse_options(args);
    if (!o) {
        std::fputs(kUsage, stderr);
        return 2;
    }
    const fs::path dir = args[0], file = args[1];
#ifdef ANNO_BENCH_GPU
    if (o->river_cost) {
        auto cx = gpu::headless_context();
        if (!cx) {
            std::fprintf(stderr, "GPU: %s\n", cx.error().c_str());
            return 1;
        }
        gpu::GpuLimits limits;
        limits.river_quality = render::RiverQuality::High;
        auto f = gpu::GpuFrame::create(cx.value(), WGPUTextureFormat_RGBA8Unorm, limits);
        if (!f) {
            std::fprintf(stderr, "GPU: %s\n", f.error().c_str());
            return 1;
        }
        std::printf("%s (%s, timestamps %s)\n\n", cx.value().name.c_str(),
                    cx.value().adapter_type == WGPUAdapterType_DiscreteGPU     ? "discrete"
                    : cx.value().adapter_type == WGPUAdapterType_IntegratedGPU ? "integrated"
                    : cx.value().adapter_type == WGPUAdapterType_CPU           ? "software"
                                                                               : "unknown",
                    cx.value().timestamps ? "yes" : "no: wall clock");
        std::printf("| tier | ms for %.2f Mpx | ms per million river pixels |\n|---|---|---|\n",
                    1.048576);
        for (const auto& rc : f.value()->measure_river_cost(9)) {
            std::printf("| %s | %.3f | %.3f |\n",
                        !rc.quality                                   ? "legacy shimmer"
                        : *rc.quality == render::RiverQuality::High   ? "high"
                        : *rc.quality == render::RiverQuality::Medium ? "medium"
                                                                      : "low",
                        rc.ms, rc.ms_per_mpx);
        }
        return 0;
    }
#endif
    const auto started = std::chrono::steady_clock::now();
    auto loaded = anno::app::load_scene(dir, file);
    if (!loaded) {
        std::fprintf(stderr, "%s: %s\n", file.string().c_str(), loaded.error().c_str());
        return 1;
    }
    loaded.value().scene->river_cfg.light_gain = o->river_gain;
    const render::Scene& scene = *loaded.value().scene;
    std::fprintf(
        stderr, "scene loaded in %.0f ms\n",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count());
    for (const std::size_t g : o->groups) {
        const auto t = std::chrono::steady_clock::now();
        scene.sea(g);
        std::fprintf(stderr, "sea pattern group %zu: %.0f ms\n", g,
                     std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t)
                         .count());
    }
    std::size_t largest = 0;
    if (o->island) {
        largest = *o->island;
    } else {
        std::uint32_t best = 0;
        // The file's own islands only, as the Rust benchmark (not stand-ins).
        for (std::size_t k = 0; k < loaded.value().file_islands; ++k) {
            const std::uint32_t a = std::uint32_t{scene.islands[k].width} * scene.islands[k].height;
            // Rust max_by_key keeps the last maximum.
            if (a >= best) {
                best = a;
                largest = k;
            }
        }
    }
    std::printf("%s — %u threads available — %zu threads%s\n\n", file.string().c_str(),
                std::thread::hardware_concurrency(), o->threads, o->quick ? " — quick" : "");
    std::printf(
        "| resolution | group | water | renderer | chunks | path | frames | p50 ms | p95 ms | "
        "p99 ms | max ms | ≤16.7 ms | built | chunk ms | missing | stage 1 | stage 2 | "
        "stage 3 | stage 4 |\n");
    std::printf("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    const std::size_t scale = o->quick ? 4 : 1;
#ifdef ANNO_BENCH_GPU
    std::optional<gpu::Context> ctx;
    std::unique_ptr<gpu::GpuFrame> frame_gpu;
    if (std::find(o->renderers.begin(), o->renderers.end(), true) != o->renderers.end()) {
        auto c = gpu::headless_context();
        if (!c) {
            std::fprintf(stderr, "GPU: %s\n", c.error().c_str());
            return 1;
        }
        ctx = std::move(c).value();
        std::fprintf(stderr, "GPU: %s\n", ctx->name.c_str());
        // One compositor for all configurations, as the Rust benchmark.
        gpu::GpuLimits limits = o->interactive ? gpu::GpuLimits::interactive() : gpu::GpuLimits{};
        limits.river_quality = o->river_quality;
        limits.gpu_timing = o->river_pass;
        if (o->fade) {
            limits.chunk_fade_ms = *o->fade;
        }
        if (o->prefetch) {
            limits.prefetch_chunks = *o->prefetch;
        }
        if (o->prewarm) {
            limits.prewarm_chunks = *o->prewarm;
        }
        if (o->stand_ins) {
            limits.stand_ins = *o->stand_ins;
        }
        auto f = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
        if (!f) {
            std::fprintf(stderr, "GPU: %s\n", f.error().c_str());
            return 1;
        }
        frame_gpu = std::move(f).value();
        // The viewer's window is open for seconds before the first zoom: wait
        // (5 s at most) for the background plane pre-creation, so the benchmark
        // sees the state of a window that has been idle.
        const auto warm_start = std::chrono::steady_clock::now();
        while (frame_gpu->warm_planes_ready() < 5 * limits.prewarm_chunks &&
               std::chrono::steady_clock::now() - warm_start < std::chrono::seconds(5)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        std::fprintf(
            stderr, "pre-created planes ready: %zu after %.0f ms\n", frame_gpu->warm_planes_ready(),
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - warm_start)
                .count());
        if (!frame_gpu->river_tier_note().empty()) {
            std::fprintf(stderr, "%s\n", frame_gpu->river_tier_note().c_str());
        }
    }
#endif
    for (const auto& [w, h] : o->resolutions) {
        for (const std::size_t group : o->groups) {
            for (const auto style : o->styles) {
                for (const bool on_gpu : o->renderers) {
                    for (const bool async_chunks : o->modes) {
                        const render::Placement* p = nullptr;
                        for (const auto& q : scene.layouts[group]) {
                            if (q.index == largest) {
                                p = &q;
                            }
                        }
                        if (p == nullptr) {
                            std::fputs("no placement for that island\n", stderr);
                            return 1;
                        }
                        Config c;
                        c.w = w;
                        c.h = h;
                        c.group = group;
                        c.style = style;
                        c.async_chunks = async_chunks;
                        c.centre = {p->bounds.x + p->bounds.w / 2 + o->at.first,
                                    p->bounds.y + p->bounds.h / 2 + o->at.second};
                        c.frames = 120 / scale;
                        c.t0 = o->t0;
                        c.warmup = o->warmup;
                        c.threads = o->threads;
                        c.check = o->check;
                        c.settle_ms = o->settle_ms;
                        c.river_flow = o->river_flow;
                        c.river_pass = o->river_pass;
                        c.zoom_every = o->zoom_every;
                        c.zoom_frames = o->zoom_frames;
                        c.pace_ms = o->pace_ms;
                        c.spike_ms = o->spike_ms;
                        c.stand_ins = o->stand_ins;
                        c.zoom_pan = o->zoom_pan;
                        c.pop = o->pop;
                        c.zoom_pan_period = o->zoom_pan_period;
                        c.zoom_dump = o->zoom_dump;
                        c.zoom_dump_after = o->zoom_dump_after;
                        c.lookahead = o->lookahead;
                        gpu::GpuFrame* g = nullptr;
#ifdef ANNO_BENCH_GPU
                        if (on_gpu) {
                            g = frame_gpu.get();
                        }
#else
                        (void)on_gpu;
#endif
                        run(scene, c, g);
                    }
                }
            }
        }
    }
    return 0;
}
