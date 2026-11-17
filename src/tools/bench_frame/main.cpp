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
//   bench_frame <game_dir> <file> --renderer gpu --scale-cost group:scale[:filter],..
//                                (GPU time of continuous zoom frames, compositor + resample)
//   bench_frame <game_dir> <file> --renderer gpu --limits interactive --derived n --zoom-sweep
//   seconds
//                                (the continuous zoom as the viewer runs it: per level the window
//                                thread's and the GPU's time per frame, chunks shown, cache MB)
//   bench_frame <game_dir> <file> --zoom-factor f [--resample nearest|bilinear|box|area] ...
//                                (every frame composes the grid of the group and resamples it
//                                to the window at f output pixels per grid pixel; CPU:
//                                cpu_resample)
//   bench_frame <game_dir> <file> --derived n --groups 2,3,..,2+n ...   (the paths above at the
//                                derived zoom levels after group 2: each is the level above reduced
//                                2x)
//   bench_frame <game_dir> <file> --fill-time [--derive old|cache] [--threads n] [--res WxH]
//                                (the first zoom-out: ms until the window's chunks are cached,
//                                levels 3-5)
//   bench_frame --list-adapters
//   bench_frame <game_dir> <file> --river-cost      (GPU cost of the river shading, synthetic)
//   bench_frame <game_dir> <file|-> --atlas-report out.tsv [--atlas-gpu-limits]
//                                (RENDER-DIRECT-1a: what keeping every sprite the direct renderer
//                                draws resident would cost - sizing only, no GPU needed)
//   bench_frame <game_dir> <file|-> --atlas-build out.tsv
//                                (RENDER-DIRECT-1b step 5: actually builds <file>'s resident atlas
//                                pages - wall time, the real byte cost with its one mip level, peak
//                                process memory; CPU only, no GPU needed)
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
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

#include <anno/app/cli.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/opening.hpp>
#include <anno/app/picture_trace.hpp>
#include <anno/app/play.hpp>
#include <anno/app/png.hpp>
#include <anno/app/zoom.hpp>
#include <anno/assets/building.hpp>
#include <anno/assets/figure.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/render/atlas_pages.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/frame_table.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/profile.hpp>
#include <anno/render/resample.hpp>
#include <anno/render/residency.hpp>
#include <anno/render/scene.hpp>
// windows.h/unistd.h unconditionally, not only under ANNO_BENCH_GPU: --atlas-build's own
// peak-memory sampling (RENDER-DIRECT-1b step 5) needs them with no GPU build at all.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// windows.h first.
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif
#ifdef ANNO_BENCH_GPU
#include <anno/render/gpu.hpp>
#endif

namespace {

namespace fs = std::filesystem;
namespace render = anno::render;
namespace app = anno::app;
namespace formats = anno::formats;
namespace assets = anno::assets;
namespace fig = anno::formats::figuren;

#ifdef ANNO_BENCH_GPU
/// System RAM in bytes, without SDL (bench_frame links anno_app_core, not anno_app_sdl, so
/// SDL_GetSystemRAM is not available here; ZOOM-SMOOTH-1c F2): GlobalMemoryStatusEx on Windows,
/// sysconf on POSIX. 0 if the query fails, which default_chunk_budget's own clamp floors safely.
std::uint64_t system_ram_bytes() {
#ifdef _WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) {
        return 0;
    }
    return status.ullTotalPhys;
#else
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages <= 0 || page_size <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(page_size);
#endif
}
#endif

/// The process's peak resident set so far, in bytes; -1 when the host does not report one
/// (RENDER-DIRECT-1b step 5, --atlas-build's own peak-memory column). Both branches ask the host
/// for its own already-tracked historical peak, not a value this function samples and maxes
/// itself: Windows' PeakWorkingSetSize (GetProcessMemoryInfo, the same call anno_test_support's
/// peak_launcher.cpp uses on a child process) and Linux's VmHWM ("/proc/self/status", the kernel's
/// own peak-RSS counter) are both already a running peak, updated by the kernel as the process
/// grows - calling this once at the end of a run reports the true peak over the whole run, with
/// no periodic sampling loop needed (unlike bench_picture's own sample_memory(), which reads
/// /proc/self/statm's current RSS and is sampled repeatedly by its caller to approximate a peak).
long long peak_rss_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<long long>(pmc.PeakWorkingSetSize);
    }
    return -1;
#elif defined(__linux__)
    if (std::FILE* f = std::fopen("/proc/self/status", "r")) {
        char line[256];
        long long kb = -1;
        while (std::fgets(line, sizeof line, f) != nullptr) {
            if (std::sscanf(line, "VmHWM: %lld kB", &kb) == 1) {
                break;
            }
        }
        std::fclose(f);
        if (kb >= 0) {
            return kb * 1024;
        }
    }
    return -1;
#else
    return -1;
#endif
}

/// Total user+system CPU time the whole process has consumed so far, in milliseconds; -1 when
/// the host does not report it (RENDER-DIRECT-1b step 5's own wall-vs-CPU question: whether a
/// phase's wall time reflects several worker threads actually running at once, or one thread
/// doing all the work). A delta of two calls across a phase is that phase's own CPU time: at
/// `threads == 1` CPU time ~= wall time; the more it exceeds wall time, the more of `threads`
/// workers were actually busy at once (GetProcessTimes on Windows, getrusage(RUSAGE_SELF) on
/// Linux - both process-wide sums across every thread, not one thread's own time).
double cpu_time_ms() {
#if defined(_WIN32)
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        auto to_ms = [](const FILETIME& ft) {
            const auto t = (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
            return static_cast<double>(t) / 10000.0; // 100 ns units -> ms
        };
        return to_ms(kernel) + to_ms(user);
    }
    return -1.0;
#elif defined(__linux__) || defined(__APPLE__)
    rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        auto to_ms = [](const timeval& tv) {
            return static_cast<double>(tv.tv_sec) * 1000.0 +
                   static_cast<double>(tv.tv_usec) / 1000.0;
        };
        return to_ms(ru.ru_utime) + to_ms(ru.ru_stime);
    }
    return -1.0;
#else
    return -1.0;
#endif
}

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
    /// GpuLimits::prefetch_budget_ms (nullopt = the limits' default; --prefetch-budget-ms 0 turns
    /// off ZOOM-SMOOTH-1d's prefetch-while-waiting share, isolating it from the ring for
    /// measurement).
    std::optional<double> prefetch_budget_ms;
    /// GpuLimits::chunk_fade_ms (nullopt = the limits' default).
    std::optional<std::uint64_t> fade;
    /// FrameRenderer::max_lookahead_chunks (nullopt = the library default).
    std::optional<std::size_t> lookahead;
    /// Directory for the frames after zoom steps (empty = none).
    std::string zoom_dump;
    /// Frames after a zoom step that --zoom-dump writes (0 = the step's own frame).
    std::vector<std::size_t> zoom_dump_after{0};
    /// --chunk-profile: the builder thread counts to profile (empty = the frame paths).
    std::vector<std::size_t> profile_threads;
    /// --chunk-build cpu|gpu: the GpuList builder (T11 stage 2a) on the GPU paths.
    render::ChunkBuilder chunk_builder = render::ChunkBuilder::Cpu;
    /// --bin-px: the ChunkDraws bin edge (gpu-chunks.md 9; the bin_px sweep).
    std::uint32_t bin_px = render::kDefaultBinPx;
    /// --chunk-limit: the chunks nearest the island's centre that are built. 96 is
    /// about four 2560 x 1440 views at group 0 (24 chunks each), a few minutes on
    /// one thread at the measured 100-250 ms per chunk; 0 = all of the island.
    std::size_t chunk_limit = 96;
    /// --scale-cost: the continuous zoom points to time (zoom group, scale, filter).
    struct ScalePoint {
        std::size_t group = 0;
        double scale = 1.0;
        std::string filter;
    };
    std::vector<ScalePoint> scale_cost;
    /// --cost-frames n: the frames timed per --scale-cost point (after the settle).
    int cost_frames = 60;
    /// --wave-sim on|off: GpuLimits::wave_simulation (enhanced water's wave simulation).
    std::optional<bool> wave_sim;
    /// --fluid-cells-per-px x: GpuLimits::fluid_max_cells_per_px (0 = no bound; default:
    /// fluid::kDefaultMaxCellsPerPx on every adapter).
    std::optional<double> fluid_cells;
    /// --cost-png dir: --scale-cost writes the last timed frame of every point as a PNG
    /// (<scene>_<w>x<h>_<style>_L<group>_s<scale>.png).
    std::string cost_png;
    /// --fill-time: the first zoom-out (levels 3-5 from group 2), and --derive old|cache.
    bool fill_time = false, derive_from_cache = true;
    /// --depth on|off: render::Scene::depth_test (docs/render/depth.md; off = before W4-DEPTH-1).
    bool depth = true;
    /// --shore h: render::Scene::shore_height, 0..255 (kShoreHeight 26.5; 16.5 the default of
    /// 2026-11-03).
    float shore = render::kShoreHeight;
    /// --derived n: n derived zoom levels after group 2 (the groups 3.. of --groups).
    std::size_t derived = 0;
    /// --zoom-sweep seconds: the continuous zoom run as the viewer does (GPU).
    double zoom_sweep = 0.0;
    /// --zoom-sweep-mode (ZOOM-MEASURE-1): legs (the original continuous log-interpolated
    /// zoom with a 300 px/s pan, unchanged) is the default; wheel is discrete human-rate
    /// notches (ZoomOptions::step, no pan); pan is a fast reversing pan at a fixed zoom level
    /// (no zoom change); zoompan is legs' zoom at a fast pan speed ("a zoom during a pan").
    enum class SweepMode { Legs, Wheel, Pan, ZoomPan };
    SweepMode zoom_sweep_mode = SweepMode::Legs;
    /// --zoom-sweep-wheel-rate n: wheel mode's notches per second (a precision-wheel click is
    /// one factor of ZoomOptions::step; a human rate is a handful per second).
    double zoom_sweep_wheel_rate = 8.0;
    /// --zoom-sweep-pan-px n: pan/zoompan modes' pan speed in screen pixels per second (legs
    /// keeps its own 300 px/s regardless of this option).
    double zoom_sweep_pan_px = 1200.0;
    /// --zoom-sweep-warm (ZOOM-MEASURE-1 Proof's "everything resident" control): runs the
    /// whole sweep once, unmeasured, before the measured pass, on the same FrameRenderer (so
    /// its CPU cache and the GPU's uploads carry over) instead of a cold one. Only correctness
    /// if the whole sweep's working set fits the cache (gpu-chunks.md's spare_chunks budget);
    /// otherwise the second pass still misses and that is itself the evidence.
    bool zoom_sweep_warm = false;
    /// --zoom-sweep-skip n: the measured pass's first n frames are excluded from every
    /// aggregate (the table, the baseline summary, the busy-time check): a frame straight
    /// after the pass's own start (pipeline/driver caches settling) is not a readiness
    /// failure, and the Proof's contract states this count and its reason instead of
    /// silently absorbing it into a looser threshold.
    std::size_t zoom_sweep_skip = 0;
    /// --chunk-budget-mb n: overrides the chunk memory budget --zoom-sweep configures from the
    /// chosen adapter and SDL_GetSystemRAM (render::default_chunk_budget, ZOOM-SMOOTH-1b step
    /// 2, the same function the viewer uses); nullopt = the computed default. 0 turns the
    /// budget off (today's exact count-based behaviour).
    std::optional<std::size_t> chunk_budget_mb;
    /// --resident-prewarm off: --zoom-sweep's FrameRenderer::prewarm_far_levels off (nullopt or
    /// --resident-prewarm on = on, the sweep's own default since ZOOM-SMOOTH-1b). For isolating
    /// the resident-level background prewarm's own cost from everything else the budget changed
    /// (ZOOM-SMOOTH-1c, the pan/iGPU busy-time regression): --chunk-budget-mb 0 does not
    /// isolate it, since prewarm_far_levels stays on and still fills resident_level_chunks'
    /// floor.
    std::optional<bool> prewarm_far_levels;
    /// --zoom-ring on: --zoom-sweep's FrameRenderer::zoom_ring_enabled on (nullopt or
    /// --zoom-ring off = off, the default since ZOOM-SMOOTH-1d measured a net regression - see
    /// FrameRenderer::zoom_ring_enabled's own comment). Kept for measuring the zoom-out ring
    /// (ZOOM-SMOOTH-1c2) again after a future packet gives it its own prefetch budget share.
    std::optional<bool> zoom_ring_enabled;
    /// --wave-fade-hold k (0..1): with --scale-cost, the simulated waves' weight held at k
    /// (GpuLimits::wave_fade_ms on a frozen clock), to time the cross-fade (OQ-076).
    double wave_fade_hold = -1.0;
    /// --zoom-factor f [--resample filter]: every frame composes the grid of the group
    /// and resamples it onto the window at f output pixels per grid pixel (GPU: the
    /// resample pass; CPU: render::cpu_resample, in the frame's time).
    double zoom_factor = 1.0;
    std::optional<render::ResampleFilter> resample;
    /// --atlas-report out.tsv (RENDER-DIRECT-1a): measures what keeping every sprite the direct
    /// renderer draws resident on the GPU would cost, and writes the table to out.tsv. <file> "-"
    /// walks the whole scene corpus (<game_dir>/Scenes plus the repo's own fixed-map/ship-start
    /// files, resolved against the current working directory - run from the repo root); any other
    /// <file> measures only that one scene (a quick check; a scene that fails to load is reported
    /// the same way as in the full corpus, not silently skipped). Empty = off.
    std::string atlas_report;
    /// --atlas-gpu-limits (RENDER-DIRECT-1a step 6): with --atlas-report, also queries the open
    /// adapter's maxTextureDimension2D/maxTextureArrayLayers/maxBufferSize (a few seconds, needs a
    /// real GPU device: tell the orchestrator first, as for any other GPU run). Off by default so
    /// the rest of the report needs no GPU at all.
    bool atlas_gpu_limits = false;
    /// --atlas-build out.tsv (RENDER-DIRECT-1b step 5): actually builds <file>'s resident atlas
    /// pages (render::build_atlas_pages, no sizing-only estimate) and reports wall time, the real
    /// byte cost with the one mip level actually built, and the process's peak memory. <file> "-"
    /// walks the whole corpus once (discover_scenes); any other <file> measures just that one
    /// scene. No GPU needed (CPU build only). Empty = off.
    std::string atlas_build;
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
    /// GPU-built chunks (T11 2a): the resident chunk atlas (MiB), chunks built by
    /// the compute pass, and CPU-fallback builds (GpuStats).
    double chunk_atlas_mb = 0;
    std::size_t gpu_built = 0, cpu_fallback = 0;
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
    double zoom_factor = 1.0;
    std::optional<render::ResampleFilter> resample;
    int w = 0, h = 0;
    std::size_t group = 0;
    render::WaterStyle style = render::WaterStyle::Original;
    render::ChunkBuilder chunk_builder = render::ChunkBuilder::Cpu;
    std::uint32_t bin_px = render::kDefaultBinPx;
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
                        const std::size_t a = static_cast<std::size_t>(y * w + x) * 4;
                        const std::size_t b =
                            static_cast<std::size_t>((y + py_) * w + (x + px_)) * 4;
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
        // GPU-built chunks (T11 2a): the resident chunk atlas and the build counts.
        double peak_atlas = 0;
        std::size_t total_gpu_built = 0, total_fallback = 0;
        for (const auto& r : rows) {
            peak_atlas = std::max(peak_atlas, r.chunk_atlas_mb);
            total_gpu_built += r.gpu_built;
            total_fallback += r.cpu_fallback;
        }
        if (peak_atlas > 0 || total_gpu_built > 0) {
            std::printf("GPU chunk atlas: peak resident %.1f MiB; %zu chunks built by the compute "
                        "pass, %zu CPU fallbacks\n",
                        peak_atlas, total_gpu_built, total_fallback);
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
    // GpuList builder only on the GPU path (the CPU path replays the list).
    if (g != nullptr) {
        fr.chunk_builder = c.chunk_builder;
        fr.bin_px = c.bin_px;
    }
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
    std::vector<std::uint32_t> grid_buffer;
    auto frame = [&](render::Rect view, std::uint64_t t, bool wait_gpu) -> Row {
        // --zoom-factor: the grid composed for the window at the factor, then resampled.
        std::optional<render::ResampledView> zoomed;
        if (c.zoom_factor != 1.0 || c.resample) {
            zoomed = render::resampled_view(view.x + view.w / 2.0, view.y + view.h / 2.0,
                                            static_cast<std::uint32_t>(c.w),
                                            static_cast<std::uint32_t>(c.h), c.zoom_factor,
                                            render::resample_filter_for(c.zoom_factor, c.resample));
        }
        const render::Rect grid_view = zoomed ? zoomed->view : view;
#ifdef ANNO_BENCH_GPU
        if (g != nullptr) {
            const auto st = g->render(fr, scene, grid_view, frame_group, t, c.style,
                                      target->second.get(), zoomed ? &zoomed->resample : nullptr);
            Row r;
            r.total = st.total_ms;
            r.built = st.chunks_built;
            r.chunk_ms = st.chunks_ms;
            r.missing = st.chunks_missing;
            r.group = frame_group;
            r.stand = st.stand_in_chunks;
            r.open = st.open_sea_chunks;
            r.open_built = st.open_sea_built;
            r.fading = st.chunks_fading;
            r.open_px = st.open_sea_px;
            r.cache_chunks = fr.cached();
            r.cache_bytes = fr.cached_bytes();
            r.chunk_atlas_mb = st.chunk_atlas_mb;
            r.gpu_built = st.gpu_built_chunks;
            r.cpu_fallback = st.chunks_cpu_fallback;
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
        double resample_ms = 0;
        render::FrameStats st;
        if (zoomed) {
            grid_buffer.resize(static_cast<std::size_t>(grid_view.w) *
                               static_cast<std::size_t>(grid_view.h));
            st = fr.render(scene, grid_view, frame_group, t, c.style, grid_buffer.data());
            const auto t0 = std::chrono::steady_clock::now();
            render::cpu_resample(grid_buffer.data(), grid_view.w, grid_view.h, zoomed->resample,
                                 static_cast<std::uint32_t>(c.w), static_cast<std::uint32_t>(c.h),
                                 out.data());
            resample_ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                    .count();
        } else {
            st = fr.render(scene, view, frame_group, t, c.style, out.data());
        }
        Row r;
        r.total = st.total_ms + resample_ms;
        r.built = st.chunks_built;
        r.chunk_ms = st.chunks_ms;
        r.missing = st.chunks_missing;
        r.stages = {st.ground_ms, st.water_objects_ms, st.sea_ms, st.objects_ms};
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
        for (std::size_t grp = 0; grp < scene.levels().size(); ++grp) {
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

/// The bounds of island `island` at zoom level `group` of the scene: its placement at
/// the built level, and at a derived level that rectangle reduced by 2^shift, rounded
/// outwards (as Scene::bounds does for the whole scene).
std::optional<render::Rect> island_bounds(const render::Scene& scene, std::size_t group,
                                          std::size_t island) {
    const render::ZoomLevels& levels = scene.levels();
    if (group >= levels.size()) {
        return std::nullopt;
    }
    const std::size_t base = levels.ancestor(group);
    if (base >= render::kGroups) {
        return std::nullopt;
    }
    for (const auto& q : scene.layouts[base]) {
        if (q.index != island) {
            continue;
        }
        const int r = 1 << levels[group].shift;
        const int x0 = render::div_euclid(q.bounds.x, r), y0 = render::div_euclid(q.bounds.y, r);
        const int x1 = -render::div_euclid(-(q.bounds.x + q.bounds.w), r);
        const int y1 = -render::div_euclid(-(q.bounds.y + q.bounds.h), r);
        return render::Rect{x0, y0, x1 - x0, y1 - y0};
    }
    return std::nullopt;
}

/// --fill-time: the first zoom-out. A window of `--res` (the first) at group 2 on the
/// island is built with `--threads` chunk workers (until its chunks are cached; the
/// prefetch goes on, as in the viewer); then the same window
/// is shown at levels 3, 4 and 5 in turn (derived, centred on the same scene point) and
/// the wall time until every shown chunk is cached is printed per level, with the
/// chunks built on the way (`--derive old` = every derived chunk builds its descendants
/// from nothing, as the first stage 3 build did; `cache` = from the cached chunks).
int fill_time(const render::Scene& scene, const Options& o, std::size_t island) {
    const auto [w, h] = o.resolutions.front();
    const render::Placement* p = nullptr;
    for (const auto& q : scene.layouts[2]) {
        if (q.index == island) {
            p = &q;
        }
    }
    if (p == nullptr) {
        std::fputs("no placement for that island\n", stderr);
        return 1;
    }
    render::FrameRenderer fr(o.threads, false);
    fr.chunk_builder = o.chunk_builder;
    fr.bin_px = o.bin_px;
    fr.derive_from_cache = o.derive_from_cache;
    fr.set_workers(std::make_unique<render::ChunkWorkers>(scene, o.threads));
    // The sea patterns are not needed to build chunks.
    const render::Rect v2{p->bounds.x + p->bounds.w / 2 - w / 2,
                          p->bounds.y + p->bounds.h / 2 - h / 2, w, h};
    // Until every chunk the view shows is cached; the prefetch (neighbouring levels, the
    // margin) goes on in the background, as it does while the owner zooms: the next step
    // starts at once, with whatever the prefetch has done by then.
    auto settle = [&](render::Rect view, std::size_t level) {
        const auto t0 = std::chrono::steady_clock::now();
        std::size_t missing = 1;
        while (missing != 0) {
            missing = std::get<2>(fr.update_chunks(scene, view, level));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (std::chrono::steady_clock::now() - t0 > std::chrono::minutes(10)) {
                break;
            }
        }
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
            .count();
    };
    auto builds = [&] {
        std::size_t n = 0;
        // Keys seen by the cache: every level, a generous range around the view.
        for (std::size_t level = 0; level < scene.levels().size(); ++level) {
            for (int cy = -64; cy < 64; ++cy) {
                for (int cx = -64; cx < 64; ++cx) {
                    n += fr.chunk_builds({level, cx, cy});
                }
            }
        }
        return n;
    };
    std::printf(
        "| threads | derive | step | window | chunks shown | ms | chunks built in all | cached |\n"
        "|---|---|---|---|---|---|---|---|\n");
    const double ms2 = settle(v2, 2);
    std::printf("| %zu | %s | group 2 | %dx%d | %zu | %.0f | %zu | %zu |\n", o.threads,
                o.derive_from_cache ? "cache" : "old", w, h, render::visible_chunks(v2).size(), ms2,
                builds(), fr.cached());
    render::Rect view = v2;
    std::size_t from = 2;
    for (const std::size_t level : {std::size_t{3}, std::size_t{4}, std::size_t{5}}) {
        view = render::zoomed_view(view, from, level, scene.levels());
        from = level;
        const double ms = settle(view, level);
        std::printf("| %zu | %s | level %zu | %dx%d | %zu | %.0f | %zu | %zu |\n", o.threads,
                    o.derive_from_cache ? "cache" : "old", level, w, h,
                    render::visible_chunks(view).size(), ms, builds(), fr.cached());
    }
    fr.set_workers(nullptr);
    return 0;
}

#ifdef ANNO_BENCH_GPU
/// --scale-cost g:scale[:filter],..: the GPU time of a continuous zoom frame (the
/// compositor and the resample pass, from the start of the first pass to the end of
/// the last, timestamps) for each point: the scene grid of zoom group g composed for
/// the window at `scale`, resampled with `filter` (default as the viewer: box below
/// 1, area from 1). Per point the chunks the grid shows, the chunks cached, the
/// chunks missing and dropped over the measured frames (a visible chunk trimmed
/// shows as missing), after a settle until everything is built and uploaded.
int scale_cost(const render::Scene& scene, const Options& o, gpu::GpuFrame& g, std::size_t island,
               const std::string& scene_name) {
    auto filter_of = [](std::string_view s, double scale) {
        if (s == "nearest") {
            return gpu::ResampleFilter::Nearest;
        }
        if (s == "bilinear") {
            return gpu::ResampleFilter::Bilinear;
        }
        if (s == "box") {
            return gpu::ResampleFilter::Box;
        }
        if (s == "area") {
            return gpu::ResampleFilter::Area;
        }
        return scale < 1.0 ? gpu::ResampleFilter::Box : gpu::ResampleFilter::Area;
    };
    auto name_of = [](gpu::ResampleFilter f) {
        return f == gpu::ResampleFilter::Nearest    ? "nearest"
               : f == gpu::ResampleFilter::Bilinear ? "bilinear"
               : f == gpu::ResampleFilter::Box      ? "box"
                                                    : "area";
    };
    std::printf(
        "| window | style | group | scale | filter | grid | chunks shown | cached | missing | "
        "dropped | pass | GPU ms p50 | p95 | p99 | max | frames timed |\n"
        "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (const auto& [w, h] : o.resolutions) {
        for (const render::WaterStyle style : o.styles) {
            for (const auto& pt : o.scale_cost) {
                const auto b = island_bounds(scene, pt.group, island);
                if (!b) {
                    std::fputs("no placement for that island\n", stderr);
                    return 1;
                }
                const double cx = b->x + b->w / 2.0 + o.at.first;
                const double cy = b->y + b->h / 2.0 + o.at.second;
                const auto filter = filter_of(pt.filter, pt.scale);
                const auto rv =
                    gpu::resampled_view(cx, cy, static_cast<std::uint32_t>(w),
                                        static_cast<std::uint32_t>(h), pt.scale, filter);
                render::FrameRenderer fr(o.threads, false);
                fr.chunk_builder = o.chunk_builder;
                fr.bin_px = o.bin_px;
                fr.set_workers(std::make_unique<render::ChunkWorkers>(scene, (o.threads + 1) / 2));
                const auto target = g.offscreen_target(static_cast<std::uint32_t>(w),
                                                       static_cast<std::uint32_t>(h));
                std::uint64_t t = 0;
                auto frame = [&] {
                    const auto st = g.render(fr, scene, rv.view, pt.group, t, style,
                                             target.second.get(), &rv.resample);
                    t += 16;
                    return st;
                };
                // --wave-fade-hold: the window appears at clock 1000 and the weight of the
                // simulation is then held at k.
                std::uint64_t fade_clock = 1000;
                g.set_fade_clock([&] { return fade_clock; });
                // Settle: until every shown chunk is built and uploaded (30 s at most).
                const auto settle_start = std::chrono::steady_clock::now();
                std::size_t quiet = 0;
                while (quiet < 20 &&
                       std::chrono::steady_clock::now() - settle_start < std::chrono::seconds(30)) {
                    const auto st = frame();
                    quiet =
                        st.chunks_missing == 0 && st.chunks_uploaded == 0 && st.placeholders == 0
                            ? quiet + 1
                            : 0;
                    std::this_thread::sleep_for(std::chrono::milliseconds(8));
                }
                if (o.wave_fade_hold >= 0.0) {
                    fade_clock = 1000 + static_cast<std::uint64_t>(o.wave_fade_hold * 1000.0);
                }
                // Per pass (GpuPassTimes) and the whole frame; a pass that did not run in a
                // frame (no wave step due) has no sample there.
                constexpr std::array<const char*, 7> kPassNames = {
                    "fluid", "terrain", "under", "sea", "objects", "resample", "frame"};
                std::array<std::vector<double>, 7> pass_ms;
                std::vector<double> cpu_ms;
                std::size_t missing = 0, dropped = 0, no_reading = 0;
                std::uint64_t fluid_steps = 0, fluid_cells = 0;
                std::uint32_t fluid_cpt = 0;
                gpu::GpuStats last;
                for (int i = 0; i < o.cost_frames; ++i) {
                    const auto st = frame();
                    missing += st.chunks_missing;
                    dropped += st.chunks_dropped;
                    cpu_ms.push_back(st.total_ms);
                    fluid_steps += st.fluid_steps;
                    fluid_cells = st.fluid_cells;
                    fluid_cpt = st.fluid_cells_per_tile;
                    last = st;
                    const auto sea = g.sea_pass_ms(); // waits for the GPU and reads the ticks
                    const auto passes = g.pass_gpu_ms();
                    if (!sea || !passes || !passes->frame) {
                        ++no_reading;
                        continue;
                    }
                    const std::array<std::optional<double>, 7> v = {
                        passes->fluid,   passes->terrain,  passes->under, passes->sea,
                        passes->objects, passes->resample, passes->frame};
                    for (std::size_t k = 0; k < v.size(); ++k) {
                        if (v[k]) {
                            pass_ms[k].push_back(*v[k]);
                        }
                    }
                }
                auto pct = [](const std::vector<double>& in, double q) {
                    if (in.empty()) {
                        return 0.0;
                    }
                    std::vector<double> v;
                    v.resize(in.size());
                    std::copy(in.begin(), in.end(), v.begin());
                    std::sort(v.begin(), v.end());
                    return v[static_cast<std::size_t>(
                        std::lround(q * static_cast<double>(v.size() - 1)))];
                };
                const char* style_name =
                    style == render::WaterStyle::Enhanced ? "enhanced" : "original";
                for (std::size_t k = 0; k < kPassNames.size(); ++k) {
                    std::printf(
                        "| %dx%d | %s | %zu | %.3f | %s | %dx%d | %zu | %zu | %zu | %zu | %s | ", w,
                        h, style_name, pt.group, rv.resample.scale, name_of(filter), rv.view.w,
                        rv.view.h, render::visible_chunks(rv.view).size(), fr.cached(), missing,
                        dropped, kPassNames[k]);
                    if (pass_ms[k].empty()) {
                        std::printf("- | - | - | - | 0 |\n");
                    } else {
                        std::printf("%.2f | %.2f | %.2f | %.2f | %zu |\n", pct(pass_ms[k], 0.5),
                                    pct(pass_ms[k], 0.95), pct(pass_ms[k], 0.99),
                                    pct(pass_ms[k], 1.0), pass_ms[k].size());
                    }
                }
                std::fprintf(stderr,
                             "%dx%d %s group %zu scale %.3f: window thread ms p50 %.2f; wave "
                             "window %llu cells (%.2f per window pixel, %u per tile), %.2f "
                             "steps per frame\n",
                             w, h, style_name, pt.group, rv.resample.scale, pct(cpu_ms, 0.5),
                             static_cast<unsigned long long>(fluid_cells),
                             static_cast<double>(fluid_cells) / (static_cast<double>(w) * h),
                             fluid_cpt,
                             static_cast<double>(fluid_steps) / static_cast<double>(o.cost_frames));
                std::fprintf(stderr,
                             "  last frame: %zu stand-in chunks (%zu parts), %zu chunks with open "
                             "sea (%llu px), %zu chunks fading\n",
                             last.stand_in_chunks, last.stand_in_parts, last.open_sea_chunks,
                             static_cast<unsigned long long>(last.open_sea_px), last.chunks_fading);
                if (no_reading > 0) {
                    std::fprintf(stderr, "%zu of %d frames without a GPU reading\n", no_reading,
                                 o.cost_frames);
                }
                if (!o.cost_png.empty()) {
                    const auto px = g.read_back(target.first.get(), static_cast<std::uint32_t>(w),
                                                static_cast<std::uint32_t>(h));
                    char name[160];
                    std::snprintf(name, sizeof name, "%s_%dx%d_%s_L%zu_s%.3f.png",
                                  scene_name.c_str(), w, h, style_name, pt.group,
                                  rv.resample.scale);
                    const auto err = anno::app::write_png(std::filesystem::path(o.cost_png) / name,
                                                          static_cast<std::uint32_t>(w),
                                                          static_cast<std::uint32_t>(h), px);
                    if (!err.empty()) {
                        std::fprintf(stderr, "%s\n", err.c_str());
                    }
                }
                fr.set_workers(nullptr);
            }
        }
    }
    return 0;
}
#endif

#ifdef ANNO_BENCH_GPU
/// --zoom-sweep seconds: the continuous zoom as the viewer runs it, on the GPU: from tile
/// width 42 out to the smallest level, in to the largest zoom, out again to the smallest
/// and back to 42, `seconds` per leg, the target moving evenly in the logarithm, eased
/// (120 ms), with a pan of 300 screen pixels per second reversing every leg and the cursor
/// 25 % / 15 % of the window off the centre as the anchor; frames paced at 500 Hz
/// (2 ms) so that the chunk builders work in real time. `--derived n` adds the levels
/// below group 2. Per zoom level shown: the window thread's time per frame (p50 / p95 /
/// p99 / max), the GPU time of the frame (timestamps; compositor and resample), the
/// most chunks the grid showed, the most megabytes the chunk cache held, the frames above
/// 8 ms and what the worst of them spent its time on.
int zoom_sweep(const render::Scene& scene, const Options& o, gpu::GpuFrame& g, std::size_t island,
               double seconds, const fs::path& file, WGPUAdapterType adapter_type) {
    const render::ZoomLevels& levels = scene.levels();
    for (std::size_t grp = 0; grp < levels.size(); ++grp) {
        scene.sea(grp);
    }
    const auto b2 = island_bounds(scene, 2, island);
    if (!b2) {
        std::fputs("no placement for that island\n", stderr);
        return 1;
    }
    const render::WaterStyle style = o.styles.front();
    constexpr double kFrameMs = 2.0;
    constexpr double kBusyMs = 8.0;
    for (const auto& [w, h] : o.resolutions) {
        render::FrameRenderer fr(o.threads, false);
        fr.chunk_builder = o.chunk_builder;
        fr.bin_px = o.bin_px;
        // chunk_worker_threads and apply_frame_config (ZOOM-SMOOTH-1 step 1, review finding
        // B1): the same functions the viewer configures FrameRenderer with, so this sweep
        // cannot drift from what the window it stands in for actually runs. Before this the
        // sweep used (threads + 1) / 2 workers against the viewer's hw * 3 / 4, and never set
        // prewarm_far_levels or resident_level_chunks at all.
        fr.set_workers(
            std::make_unique<render::ChunkWorkers>(scene, render::chunk_worker_threads(o.threads)));
        render::FrameConfig cfg;
        cfg.prewarm_far_levels = o.prewarm_far_levels.value_or(true);
        cfg.zoom_ring_enabled = o.zoom_ring_enabled.value_or(false);
        // ZOOM-SMOOTH-1b step 2: the same default_chunk_budget the viewer computes from the
        // adapter and system RAM, overridable with --chunk-budget-mb (0 = off) so the sweep can
        // be compared across budgets in one invocation.
        const render::ChunkBudgetAdapter adapter_kind =
            adapter_type == WGPUAdapterType_DiscreteGPU ? render::ChunkBudgetAdapter::Discrete
            : adapter_type == WGPUAdapterType_CPU ? render::ChunkBudgetAdapter::Software
                                                  : render::ChunkBudgetAdapter::IntegratedOrUnknown;
        // bench_frame does not link SDL3 (it links anno_app_core, not anno_app_sdl), so there is
        // no SDL_GetSystemRAM here; system_ram_bytes() above reads it directly instead
        // (ZOOM-SMOOTH-1c F2: a non-discrete adapter was getting the 256 MiB floor here while
        // the viewer gets RAM/8 capped at 2 GiB, so the iGPU sweep measured a different
        // configuration from what it stands in for).
        cfg.budget = render::default_chunk_budget(adapter_kind, system_ram_bytes());
        if (o.chunk_budget_mb) {
            cfg.budget.total_bytes =
                static_cast<std::size_t>(static_cast<std::uint64_t>(*o.chunk_budget_mb) *
                                         std::uint64_t{1024} * std::uint64_t{1024});
        }
        render::apply_frame_config(fr, cfg);
        const auto target =
            g.offscreen_target(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
        auto ready = [&](std::size_t grp) { return g.group_ready(scene, grp); };
        // Start as the viewer does: group 1 on the island, its tile width.
        anno::app::ZoomState s;
        s.group = 1;
        s.now = s.target = levels.tile_w(1);
        const auto b1 = *island_bounds(scene, 1, island);
        s.cam_x = b1.x + b1.w / 2.0;
        s.cam_y = b1.y + b1.h / 2.0;
        s.anchor_x = 0.25 * w;
        s.anchor_y = 0.15 * h;
        const double lo =
            std::max(levels.tile_w(levels.size() - 1) / anno::app::kZoomEndFactor, 0.5);
        const double hi = levels.tile_w(0) * anno::app::kZoomEndFactor;
        const std::array<std::pair<double, double>, 4> legs = {
            {{levels.tile_w(1), lo}, {lo, hi}, {hi, lo}, {lo, levels.tile_w(1)}}};
        struct Frame {
            std::size_t group;
            double total, gpu, scale;
            std::size_t visible;
            /// ZOOM-MEASURE-1: readiness counters read from the same GpuStats the window's
            /// per-second report reads (gpu.hpp), not recomputed.
            std::size_t missing = 0;     // GpuStats::chunks_missing: a visible chunk not ready.
            std::size_t stand_parts = 0; // GpuStats::stand_in_parts: stand-in draws this frame.
            std::size_t fading = 0;      // GpuStats::chunks_fading: chunks dissolving in.
            std::size_t open = 0;        // GpuStats::open_sea_chunks: placeholder/open-sea draws.
            bool wave_fading = false;    // GpuStats::fluid_fade < 1: inside a wave cross-fade.
            std::uint64_t fluid_windows = 0; // GpuStats::fluid_windows (cumulative).
            // ZOOM-SMOOTH-1 step 7: the window-thread chunks step's own time (GpuStats::
            // chunks_ms: plan_margin, request_chunks, trim, every frame, not only busy ones) so
            // the proof can report its p99/max separately from the frame's total busy time —
            // the planning cost this packet's wider wanted set might grow (review finding S4).
            double chunks_ms = 0;
            std::vector<std::pair<const char*, double>> steps;
        };
        std::vector<Frame> frames;
        std::vector<std::size_t> cache_mb(levels.size(), 0);
        std::uint64_t t_ms = 0;
        // --zoom-sweep-warm (the Proof's "everything resident" control): an unmeasured pass
        // over the same FrameRenderer first, so its CPU cache and the GPU's uploads are
        // already warm for the measured pass — correctness only if the sweep's whole working
        // set fits the cache; a still-nonzero measured pass after this is itself the evidence
        // that it does not (reported, not hidden).
        const int passes = o.zoom_sweep_warm ? 2 : 1;
        for (int pass = 0; pass < passes; ++pass) {
            frames.clear();
            std::fill(cache_mb.begin(), cache_mb.end(), 0);
            s.group = 1;
            s.now = s.target = levels.tile_w(1);
            s.cam_x = b1.x + b1.w / 2.0;
            s.cam_y = b1.y + b1.h / 2.0;
            auto last = std::chrono::steady_clock::now();
            std::size_t index = 0;
            for (std::size_t leg = 0; leg < legs.size(); ++leg) {
                const auto [from, to] = legs[leg];
                const auto frames_in_leg = static_cast<std::size_t>(seconds * 1000.0 / kFrameMs);
                for (std::size_t i = 0; i < frames_in_leg; ++i, ++index) {
                    const auto frame_start = std::chrono::steady_clock::now();
                    const double dt_ms =
                        std::chrono::duration<double, std::milli>(frame_start - last).count();
                    last = frame_start;
                    const double u_raw =
                        static_cast<double>(i) / static_cast<double>(frames_in_leg);
                    // ZOOM-MEASURE-1: the sweep profile. legs (default) keeps today's continuous
                    // log-interpolated target, unchanged. wheel quantises u to human-rate notches
                    // (a precision-wheel click is ZoomOptions::step; a few per second is a human
                    // rate). pan holds ONE level (the start level, never the leg's own `from`) for
                    // the whole sweep, so --zoom-sweep-pan-px 0 is a true camera-at-rest control
                    // (zoom-smoothness.md's own "any level, camera at rest: 0" row) — not just
                    // "no zoom change within a leg", which would still cross levels leg to leg.
                    // zoompan keeps legs' continuous target at pan's fast speed ("a zoom during a
                    // pan"). Only legs and zoompan actually move s.target.
                    double zoom_target = levels.tile_w(1);
                    if (o.zoom_sweep_mode == Options::SweepMode::Wheel) {
                        const double frames_per_notch =
                            std::max(1.0, 1000.0 / (o.zoom_sweep_wheel_rate * kFrameMs));
                        const double u = std::min(
                            1.0, std::floor(static_cast<double>(i) / frames_per_notch) *
                                     frames_per_notch / static_cast<double>(frames_in_leg));
                        zoom_target = from * std::pow(to / from, u);
                    } else if (o.zoom_sweep_mode != Options::SweepMode::Pan) {
                        zoom_target = from * std::pow(to / from, u_raw);
                    }
                    s.target = zoom_target;
                    s = anno::app::zoom_advance(s, dt_ms, 120, ready, levels);
                    const double scale = s.now / levels.tile_w(s.group);
                    // The pan: legs keeps its original 300 px/s; wheel has none (a pure zoom test);
                    // pan/zoompan use --zoom-sweep-pan-px (default 1200, "fast"), reversing every
                    // leg.
                    const double pan_px_per_s = o.zoom_sweep_mode == Options::SweepMode::Wheel ? 0.0
                                                : o.zoom_sweep_mode == Options::SweepMode::Legs
                                                    ? 300.0
                                                    : o.zoom_sweep_pan_px;
                    const double pan =
                        (leg % 2 == 0 ? 1.0 : -1.0) * pan_px_per_s * dt_ms / 1000.0 / scale;
                    s.cam_x += pan;
                    if (const auto bnd = scene.bounds(s.group)) {
                        s.cam_x = std::clamp(s.cam_x, static_cast<double>(bnd->x),
                                             static_cast<double>(bnd->x) + bnd->w);
                        s.cam_y = std::clamp(s.cam_y, static_cast<double>(bnd->y),
                                             static_cast<double>(bnd->y) + bnd->h);
                    }
                    const auto rv = render::resampled_view(
                        s.cam_x, s.cam_y, static_cast<std::uint32_t>(w),
                        static_cast<std::uint32_t>(h), scale, render::resample_filter_for(scale));
                    fr.motion_centre = anno::app::pan_centre(s);
                    // zoom_anchor_at (ZOOM-SMOOTH-1 step 1, review finding B1): the sweep's own
                    // off-centre anchor (s.anchor_x/y, set below island_bounds above) through the
                    // same formula the viewer uses for its cursor, so the neighbouring groups'
                    // chunks are requested around the anchor here too; before this fr.zoom_anchor
                    // was never set, so the zoom-out ring (step 4) and resident levels (step 3)
                    // could not be exercised by this proof.
                    fr.zoom_anchor =
                        render::zoom_anchor_at(s.cam_x, s.cam_y, s.anchor_x, s.anchor_y, scale);
                    // The zoom's own direction (ZOOM-SMOOTH-1c2 step 1): negative while
                    // zooming out (s.now shrinking toward s.target), matching the viewer's own
                    // zoom_target_ - zoom_now_.
                    fr.zoom_direction = s.target - s.now;
                    const auto st = g.render(fr, scene, rv.view, s.group, t_ms, style,
                                             target.second.get(), &rv.resample);
                    t_ms += 2;
                    Frame f;
                    f.group = s.group;
                    f.total = st.total_ms;
                    f.scale = scale;
                    f.visible = render::visible_chunks(rv.view).size();
                    f.missing = st.chunks_missing;
                    f.stand_parts = st.stand_in_parts;
                    f.fading = st.chunks_fading;
                    f.open = st.open_sea_chunks;
                    f.wave_fading = st.fluid_fade < 1.0F;
                    f.fluid_windows = st.fluid_windows;
                    f.chunks_ms = st.chunks_ms;
                    const auto sea = g.sea_pass_ms(); // waits for the GPU
                    f.gpu = sea && g.frame_gpu_ms() ? *g.frame_gpu_ms() : -1.0;
                    if (f.total > kBusyMs) {
                        for (const auto& step : st.steps()) {
                            f.steps.emplace_back(step.name, step.ms);
                        }
                    }
                    if (index % 64 == 0) {
                        cache_mb[s.group] = std::max(cache_mb[s.group], fr.cached_bytes() >> 20);
                    }
                    frames.push_back(std::move(f));
                    const auto spent = std::chrono::steady_clock::now() - frame_start;
                    const auto pace = std::chrono::duration<double, std::milli>(kFrameMs);
                    if (spent < pace) {
                        std::this_thread::sleep_for(pace - spent);
                    }
                }
            }
        } // pass (the warm pass's frames/cache_mb are overwritten by the measured one above)
        // --zoom-sweep-skip: drop the measured pass's own first n frames from every aggregate
        // below (table, baseline summary, busy-time check), stated and justified in the Proof,
        // not a silently looser threshold.
        if (o.zoom_sweep_skip > 0 && o.zoom_sweep_skip < frames.size()) {
            frames.erase(frames.begin(),
                         frames.begin() + static_cast<std::ptrdiff_t>(o.zoom_sweep_skip));
        }
        const char* mode_name = o.zoom_sweep_mode == Options::SweepMode::Wheel     ? "wheel"
                                : o.zoom_sweep_mode == Options::SweepMode::Pan     ? "pan"
                                : o.zoom_sweep_mode == Options::SweepMode::ZoomPan ? "zoompan"
                                                                                   : "legs";
        std::printf("\n%dx%d, %zu frames, sweep 42 -> %.1f -> %.1f -> %.1f -> 42 tile width, "
                    "%.0f s per leg, %s water, %s mode%s\n\n",
                    w, h, frames.size(), lo, hi, lo, seconds,
                    style == render::WaterStyle::Enhanced ? "enhanced" : "original", mode_name,
                    o.zoom_sweep_warm ? " (warm pass 2 of 2, measured)" : "");
        std::printf(
            "| level | scale range | frames | thread ms p50 | p95 | p99 | max | GPU ms p50 | "
            "GPU ms max | chunks shown (max) | cache MB (max) | non-ready frames | most "
            "non-ready at once | stand-in draws | dissolves | open-sea draws | frames in wave "
            "fade | frames > %.0f ms | frames > %.0f ms | chunks step p99 | chunks step max |\n"
            "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"
            "---|\n",
            kFrameMs, kBusyMs);
        auto pct = [](std::vector<double> v, double q) {
            if (v.empty()) {
                return 0.0;
            }
            std::sort(v.begin(), v.end());
            return v[static_cast<std::size_t>(std::lround(q * static_cast<double>(v.size() - 1)))];
        };
        std::vector<std::pair<double, std::string>> worst;
        // ZOOM-MEASURE-1: the sweep's own baseline totals, across every level (the packet's
        // Proof table is one row per map/water/resolution/adapter combination; the per-level
        // breakdown above is this run's detail, this is its one-line summary).
        std::size_t total_frames = 0, total_non_ready = 0, total_frame_over_2 = 0,
                    total_frame_over_busy = 0, most_missing_at_once = 0;
        std::size_t total_stand_draws = 0, total_dissolves = 0, total_open_draws = 0,
                    total_wave_fade_frames = 0;
        // Wave switches: summed positive steps of GpuStats::fluid_windows in time order, not
        // last - first (a mid-run reset of the simulation's own serial, e.g. a group ready to
        // nothing, would otherwise wrap a std::uint64_t subtraction negative).
        std::uint64_t wave_switches = 0, prev_fluid_windows = 0;
        bool have_prev_fluid = false;
        for (const Frame& f : frames) {
            if (have_prev_fluid && f.fluid_windows > prev_fluid_windows) {
                wave_switches += f.fluid_windows - prev_fluid_windows;
            }
            prev_fluid_windows = f.fluid_windows;
            have_prev_fluid = true;
        }
        std::vector<double> total_ms_all;
        for (std::size_t grp = 0; grp < levels.size(); ++grp) {
            std::vector<double> total, gpu_ms;
            // ZOOM-SMOOTH-1 step 7, review finding S4: the window-thread "chunks" step's own
            // time (plan_margin, request_chunks, trim), every frame, not only busy ones — the
            // wanted set this packet grows must not raise this at the 99th percentile.
            std::vector<double> chunks_step_ms;
            double smin = 1e9, smax = 0;
            std::size_t visible = 0, busy = 0, busy2 = 0, count = 0;
            std::size_t non_ready = 0, most_missing = 0, stand_draws = 0, dissolves = 0,
                        open_draws = 0, wave_fade_frames = 0;
            for (const Frame& f : frames) {
                if (f.group != grp) {
                    continue;
                }
                ++count;
                total.push_back(f.total);
                total_ms_all.push_back(f.total);
                chunks_step_ms.push_back(f.chunks_ms);
                if (f.gpu >= 0) {
                    gpu_ms.push_back(f.gpu);
                }
                smin = std::min(smin, f.scale);
                smax = std::max(smax, f.scale);
                visible = std::max(visible, f.visible);
                if (f.missing > 0) {
                    ++non_ready;
                }
                most_missing = std::max(most_missing, f.missing);
                stand_draws += f.stand_parts;
                dissolves += f.fading;
                open_draws += f.open;
                if (f.wave_fading) {
                    ++wave_fade_frames;
                }
                if (f.total > kFrameMs) {
                    ++busy2;
                }
                if (f.total > kBusyMs) {
                    ++busy;
                    std::string what;
                    for (const auto& [name, ms] : f.steps) {
                        if (ms >= 1.0) {
                            char buf[64];
                            std::snprintf(buf, sizeof buf, "%s %.2f, ", name, ms);
                            what += buf;
                        }
                    }
                    char head[96];
                    std::snprintf(head, sizeof head, "level %zu, scale %.2f, %.2f ms: ", grp,
                                  f.scale, f.total);
                    worst.emplace_back(f.total, head + what);
                }
            }
            if (count == 0) {
                continue;
            }
            std::printf("| %zu | %.2f - %.2f | %zu | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f | "
                        "%zu | %zu | %zu | %zu | %zu | %zu | %zu | %zu | %zu | %zu | %.3f | "
                        "%.3f |\n",
                        grp, smin, smax, count, pct(total, 0.5), pct(total, 0.95), pct(total, 0.99),
                        pct(total, 1.0), pct(gpu_ms, 0.5), pct(gpu_ms, 1.0), visible, cache_mb[grp],
                        non_ready, most_missing, stand_draws, dissolves, open_draws,
                        wave_fade_frames, busy2, busy, pct(chunks_step_ms, 0.99),
                        pct(chunks_step_ms, 1.0));
            total_frames += count;
            total_non_ready += non_ready;
            most_missing_at_once = std::max(most_missing_at_once, most_missing);
            total_stand_draws += stand_draws;
            total_dissolves += dissolves;
            total_open_draws += open_draws;
            total_wave_fade_frames += wave_fade_frames;
            total_frame_over_2 += busy2;
            total_frame_over_busy += busy;
        }
        std::printf("\nbaseline: %s, %dx%d, %s water, %s mode: frames %zu, non-ready frames "
                    "%zu, most non-ready at once %zu, stand-in draws %zu, dissolves %zu, "
                    "open-sea draws %zu, wave switches %llu, frames in wave fade %zu, busy ms "
                    "p50 %.2f, p99 %.2f, max %.2f, frames > %.0f ms %zu, frames > %.0f ms %zu\n",
                    file.filename().string().c_str(), w, h,
                    style == render::WaterStyle::Enhanced ? "enhanced" : "original", mode_name,
                    total_frames, total_non_ready, most_missing_at_once, total_stand_draws,
                    total_dissolves, total_open_draws,
                    static_cast<unsigned long long>(wave_switches), total_wave_fade_frames,
                    pct(total_ms_all, 0.5), pct(total_ms_all, 0.99), pct(total_ms_all, 1.0),
                    kFrameMs, total_frame_over_2, kBusyMs, total_frame_over_busy);
        std::sort(worst.begin(), worst.end(), std::greater<>());
        std::printf("\nthe busiest frames above %.0f ms (%zu in all):\n", kBusyMs, worst.size());
        for (std::size_t i = 0; i < worst.size() && i < 8; ++i) {
            std::printf("- %s\n", worst[i].second.c_str());
        }
        fr.set_workers(nullptr);
    }
    return 0;
}
#endif

/// --chunk-profile: builds the chunks of island `island` per group, as the
/// viewer's workers do (render_chunk without ships, the river flow waited for),
/// on each thread count, from a freshly loaded scene (cold: sprite decodes, the
/// island water and the river flow solved on first use) and again on the same
/// scene (warm), and prints the time per stage (anno/render/profile.hpp).
int chunk_profile(const fs::path& dir, const fs::path& file, const Options& o, std::size_t island) {
    namespace profile = render::profile;
    struct Record {
        double total = 0;
        profile::Times stages;
        bool land = false;
    };
    auto pct = [](std::vector<double> v, double p) {
        if (v.empty()) {
            return 0.0;
        }
        std::sort(v.begin(), v.end());
        const auto k = static_cast<std::size_t>(p * static_cast<double>(v.size() - 1) + 0.5);
        return v[std::min(k, v.size() - 1)];
    };
    std::printf("%s, island %zu, %u hardware threads\n\n", file.string().c_str(), island,
                std::thread::hardware_concurrency());
    for (const std::size_t group : o.groups) {
        for (const std::size_t threads : o.profile_threads) {
            auto loaded = anno::app::load_scene(dir, file);
            if (!loaded) {
                std::fprintf(stderr, "%s: %s\n", file.string().c_str(), loaded.error().c_str());
                return 1;
            }
            loaded.value().scene->river_cfg.light_gain = o.river_gain;
            loaded.value().scene->depth_test = o.depth;
            loaded.value().scene->shore_height = o.shore;
            const render::Scene& scene = *loaded.value().scene;
            const auto place =
                std::find_if(scene.layouts[group].begin(), scene.layouts[group].end(),
                             [&](const render::Placement& p) { return p.index == island; });
            if (place == scene.layouts[group].end()) {
                std::fprintf(stderr, "island %zu is not in group %zu\n", island, group);
                return 1;
            }
            const render::Rect b = place->bounds;
            auto floor_div = [](int a, int d) { return a >= 0 ? a / d : -((-a + d - 1) / d); };
            std::vector<std::pair<int, int>> keys;
            for (int cy = floor_div(b.y, render::kChunk);
                 cy <= floor_div(b.y + b.h - 1, render::kChunk); ++cy) {
                for (int cx = floor_div(b.x, render::kChunk);
                     cx <= floor_div(b.x + b.w - 1, render::kChunk); ++cx) {
                    keys.emplace_back(cx, cy);
                }
            }
            // Nearest the island's centre first (ties in row order: deterministic).
            const double mx = b.x + b.w / 2.0, my = b.y + b.h / 2.0;
            auto dist = [&](const std::pair<int, int>& k) {
                const double x = (k.first + 0.5) * render::kChunk - mx;
                const double y = (k.second + 0.5) * render::kChunk - my;
                return x * x + y * y;
            };
            std::stable_sort(keys.begin(), keys.end(),
                             [&](const auto& a, const auto& c) { return dist(a) < dist(c); });
            if (o.chunk_limit != 0 && keys.size() > o.chunk_limit) {
                keys.resize(o.chunk_limit);
            }
            for (const char* pass : {"cold", "warm"}) {
                std::vector<Record> records(keys.size());
                std::atomic<std::size_t> next{0};
                const auto start = std::chrono::steady_clock::now();
                std::vector<std::thread> pool;
                for (std::size_t t = 0; t < threads; ++t) {
                    pool.emplace_back([&] {
                        for (;;) {
                            const std::size_t i = next.fetch_add(1);
                            if (i >= keys.size()) {
                                return;
                            }
                            Record& r = records[i];
                            profile::attach(&r.stages);
                            const auto t0 = std::chrono::steady_clock::now();
                            const render::Chunk c = render::render_chunk(
                                scene, group, keys[i].first, keys[i].second, false, true);
                            r.total = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - t0)
                                          .count();
                            profile::attach(nullptr);
                            r.land = !c.ground.empty();
                        }
                    });
                }
                for (auto& t : pool) {
                    t.join();
                }
                const double wall = std::chrono::duration<double, std::milli>(
                                        std::chrono::steady_clock::now() - start)
                                        .count();
                std::vector<double> totals;
                profile::Times sum, worst;
                double other = 0;
                for (const auto& r : records) {
                    if (!r.land) {
                        continue;
                    }
                    totals.push_back(r.total);
                    double staged = 0;
                    for (std::size_t s = 0; s < sum.ms.size(); ++s) {
                        sum.ms[s] += r.stages.ms[s];
                        worst.ms[s] = std::max(worst.ms[s], r.stages.ms[s]);
                        staged += r.stages.ms[s];
                    }
                    other += r.total - staged;
                }
                const double n = std::max<double>(1.0, static_cast<double>(totals.size()));
                double mean_total = 0;
                for (const double v : totals) {
                    mean_total += v / n;
                }
                std::printf("### group %zu, %zu thread(s), %s: %zu chunks (%zu with land) in "
                            "%.0f ms, %.1f chunks/s\n\n",
                            group, threads, pass, keys.size(), totals.size(), wall,
                            1000.0 * static_cast<double>(keys.size()) / wall);
                std::printf("land chunk: mean %.1f ms, p50 %.1f, p95 %.1f, max %.1f\n\n",
                            mean_total, pct(totals, 0.5), pct(totals, 0.95), pct(totals, 1.0));
                std::printf("| stage | mean ms | share | max ms |\n|---|---|---|---|\n");
                for (std::size_t s = 0; s < sum.ms.size(); ++s) {
                    std::printf("| %.*s | %.2f | %.1f %% | %.1f |\n",
                                static_cast<int>(profile::kStageNames[s].size()),
                                profile::kStageNames[s].data(), sum.ms[s] / n,
                                mean_total > 0 ? 100.0 * sum.ms[s] / n / mean_total : 0.0,
                                worst.ms[s]);
                }
                std::printf("| outside the stages | %.2f | %.1f %% | |\n\n", other / n,
                            mean_total > 0 ? 100.0 * other / n / mean_total : 0.0);
                std::fflush(stdout);
            }
        }
    }
    return 0;
}

constexpr const char* kUsage =
    "usage: bench_frame <game_dir> <file> [--threads n] [--res WxH,..] [--groups 0,1,2] "
    "[--styles original,enhanced] [--chunks blocking,async] [--renderer cpu,gpu] "
    "[--chunk-build cpu|gpu] [--bin-px n] [--check] "
    "[--island n] [--at dx,dy] [--t0 ms] [--warmup ms] [--quick] [--zoom k] [--zoom-frames n] "
    "[--pace ms] [--spike ms] [--limits default|interactive] [--settle ms] "
    "[--river-flow legacy|flow] [--river-quality auto|low|medium|high] [--river-gain x] "
    "[--river-pass] [--stand-ins on|off] [--prewarm chunks] [--prefetch chunks] [--fade ms] "
    "[--lookahead-chunks n] [--depth on|off] [--shore h] "
    "[--zoom-dump dir]\n"
    "       bench_frame <game_dir> <file> --renderer gpu --scale-cost group:scale[:filter],.. "
    "[--res ..] [--styles ..] [--cost-frames n] [--derived n] [--wave-sim on|off] "
    "[--fluid-cells-per-px x] [--cost-png dir]\n"
    "       bench_frame <game_dir> <file> --chunk-profile threads,.. [--chunk-limit n] "
    "[--groups ..] [--island n] [--depth on|off] [--shore h]\n"
    "       bench_frame <game_dir> <file> --zoom-factor f [--resample nearest|bilinear|box|area] "
    "[--renderer cpu,gpu] [--groups ..]\n"
    "       bench_frame <game_dir> <file> --renderer cpu,gpu --derived n --groups 0,..,2+n "
    "(derived zoom levels after group 2)\n"
    "       bench_frame <game_dir> <file> --fill-time [--derive old|cache] [--threads n] "
    "[--res WxH]\n"
    "       bench_frame --list-adapters\n"
    "       bench_frame <game_dir> <file> --river-cost\n"
    "       bench_frame <game_dir> - --atlas-report out.tsv [--atlas-gpu-limits] (- walks "
    "<game_dir>/Scenes and the repo's own fixed-map/ship-start files, resolved against the "
    "current working directory; a real <file> measures only that one scene)\n";

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
        if (a == "--fill-time") {
            o.fill_time = true;
            continue;
        }
        if (a == "--zoom-sweep-warm") {
            o.zoom_sweep_warm = true;
            continue;
        }
        if (a == "--atlas-gpu-limits") {
            o.atlas_gpu_limits = true;
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
                // A derived level (--derived) is checked once all options are read.
                ok = ok && parse(g, n);
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
        } else if (a == "--chunk-build") {
            ok = v == "cpu" || v == "gpu";
            o.chunk_builder =
                v == "gpu" ? render::ChunkBuilder::GpuList : render::ChunkBuilder::Cpu;
        } else if (a == "--bin-px") {
            std::uint32_t n = 0;
            ok = parse(v, n) && n > 0;
            o.bin_px = n;
        } else if (a == "--island") {
            std::size_t n = 0;
            ok = parse(v, n);
            o.island = n;
        } else if (a == "--at") {
            const auto xy = split(v, ',');
            ok = xy.size() == 2 && parse(xy[0], o.at.first) && parse(xy[1], o.at.second);
        } else if (a == "--settle") {
            ok = parse(v, o.settle_ms);
        } else if (a == "--depth") {
            ok = v == "on" || v == "off";
            o.depth = v == "on";
        } else if (a == "--shore") {
            ok = parse(v, o.shore) && o.shore >= 0.0F && o.shore <= 255.0F;
        } else if (a == "--river-flow") {
            ok = v == "legacy" || v == "flow";
            o.river_flow =
                v == "legacy" ? render::RiverFlowMode::Legacy : render::RiverFlowMode::Flow;
        } else if (a == "--cost-frames") {
            ok = parse(v, o.cost_frames) && o.cost_frames > 0;
        } else if (a == "--cost-png") {
            o.cost_png = std::string(v);
        } else if (a == "--fluid-cells-per-px") {
            double x = 0.0;
            ok = parse(v, x) && x >= 0.0;
            o.fluid_cells = x;
        } else if (a == "--wave-sim") {
            ok = v == "on" || v == "off";
            o.wave_sim = v == "on";
        } else if (a == "--wave-fade-hold") {
            ok = parse(v, o.wave_fade_hold) && o.wave_fade_hold >= 0.0 && o.wave_fade_hold <= 1.0;
        } else if (a == "--zoom-sweep") {
            ok = parse(v, o.zoom_sweep) && o.zoom_sweep > 0.0;
        } else if (a == "--zoom-sweep-mode") {
            ok = v == "legs" || v == "wheel" || v == "pan" || v == "zoompan";
            o.zoom_sweep_mode = v == "wheel"     ? Options::SweepMode::Wheel
                                : v == "pan"     ? Options::SweepMode::Pan
                                : v == "zoompan" ? Options::SweepMode::ZoomPan
                                                 : Options::SweepMode::Legs;
        } else if (a == "--zoom-sweep-wheel-rate") {
            ok = parse(v, o.zoom_sweep_wheel_rate) && o.zoom_sweep_wheel_rate > 0.0;
        } else if (a == "--zoom-sweep-pan-px") {
            ok = parse(v, o.zoom_sweep_pan_px) && o.zoom_sweep_pan_px >= 0.0;
        } else if (a == "--zoom-sweep-skip") {
            ok = parse(v, o.zoom_sweep_skip);
        } else if (a == "--chunk-budget-mb") {
            std::size_t mb = 0;
            ok = parse(v, mb);
            if (ok) {
                o.chunk_budget_mb = mb;
            }
        } else if (a == "--resident-prewarm") {
            ok = v == "on" || v == "off";
            if (ok) {
                o.prewarm_far_levels = v == "on";
            }
        } else if (a == "--zoom-ring") {
            ok = v == "on" || v == "off";
            if (ok) {
                o.zoom_ring_enabled = v == "on";
            }
        } else if (a == "--zoom-factor") {
            ok = parse(v, o.zoom_factor) && o.zoom_factor >= 1.0 / 64.0 && o.zoom_factor <= 64.0;
        } else if (a == "--resample") {
            ok = v == "nearest" || v == "bilinear" || v == "box" || v == "area";
            o.resample = v == "nearest"    ? render::ResampleFilter::Nearest
                         : v == "bilinear" ? render::ResampleFilter::Bilinear
                         : v == "box"      ? render::ResampleFilter::Box
                                           : render::ResampleFilter::Area;
        } else if (a == "--derived") {
            ok = parse(v, o.derived) && o.derived <= 24;
        } else if (a == "--derive") {
            ok = v == "old" || v == "cache";
            o.derive_from_cache = v == "cache";
        } else if (a == "--scale-cost") {
            for (const auto item : split(v, ',')) {
                const auto f = split(item, ':');
                Options::ScalePoint pt;
                ok = ok && (f.size() == 2 || f.size() == 3) && parse(f[0], pt.group) &&
                     parse(f[1], pt.scale) && pt.scale > 0.0;
                if (f.size() == 3) {
                    pt.filter = std::string(f[2]);
                    ok = ok && (pt.filter == "nearest" || pt.filter == "bilinear" ||
                                pt.filter == "box" || pt.filter == "area");
                }
                o.scale_cost.push_back(pt);
            }
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
            // The viewer's bound (anno::app::kMaxLookaheadChunks): far beyond it the
            // margin's pixel reach no longer fits lookahead_rect's int rectangle.
            std::size_t n = 0;
            ok = parse(v, n) && n <= 16;
            o.lookahead = n;
        } else if (a == "--fade") {
            std::uint64_t n = 0;
            ok = parse(v, n);
            o.fade = n;
        } else if (a == "--prefetch") {
            std::size_t n = 0;
            ok = parse(v, n);
            o.prefetch = n;
        } else if (a == "--prefetch-budget-ms") {
            double n = 0.0;
            ok = parse(v, n) && n >= 0.0;
            o.prefetch_budget_ms = n;
        } else if (a == "--prewarm") {
            std::size_t n = 0;
            ok = parse(v, n);
            o.prewarm = n;
        } else if (a == "--chunk-profile") {
            o.profile_threads.clear();
            for (const auto s : split(v, ',')) {
                std::size_t n = 0;
                ok = ok && parse(s, n) && n > 0;
                o.profile_threads.push_back(n);
            }
        } else if (a == "--chunk-limit") {
            ok = parse(v, o.chunk_limit);
        } else if (a == "--atlas-report") {
            ok = !v.empty();
            o.atlas_report = std::string(v);
        } else if (a == "--atlas-build") {
            ok = !v.empty();
            o.atlas_build = std::string(v);
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

/// ZOOM-MEASURE-1: the sweep's own loader for the ship-start map (zoom-smoothness.md §2's
/// second profile, the owner's own session). `load_scene` reads raw island-format bytes
/// (insel::islands) and cannot read a `.txt` fixed-map layout or a `.json` ship-start
/// manifest; both route through the same app-level loaders `main.cpp`/`opening.cpp` use for
/// the real window, so no placement or template-resolution logic is duplicated here. A
/// scenario file (.sze/.scp) keeps today's load_scene path unchanged.
formats::Result<app::LoadedScene, std::string> load_any_scene(const fs::path& dir,
                                                              const fs::path& file) {
    std::string ext = file.extension().string();
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (ext == ".txt") {
        auto play = app::load_play(dir, file, app::FixedPlayOptions{});
        if (!play) {
            return play.error();
        }
        return app::load_placed_scene(dir, play.value().name, play.value().placed);
    }
    if (ext == ".json") {
        auto opening = app::load_opening(dir, file, 0, std::nullopt, false);
        if (!opening) {
            return opening.error();
        }
        return app::load_placed_scene(dir, opening.value().play.name, opening.value().play.placed);
    }
    return app::load_scene(dir, file);
}

// --- RENDER-DIRECT-1a: --atlas-report ---------------------------------------------------------
// ADR 0007 step 1, first half (docs/orchestration/tasks/render-direct-1a.md): measures what
// keeping every sprite the direct renderer draws resident on the GPU would cost. Measurement and
// document only - nothing here is read by any render path.

/// Frames and their total decoded bytes (header sizes, no decode).
struct FrameTotals {
    std::uint64_t frames = 0, bytes = 0;
};

/// A distinct resident-atlas frame and its decoded size, keyed as render::SpriteKey (ANLAGEN or
/// FigurenHeader id, variant, shadow, group, frame index) so objects that share a frame (the same
/// id, rotation block and step) are counted once, SpriteCache's own granularity (iso.hpp
/// "SpriteCache", "SpriteKey").
struct AtlasFrames {
    std::map<render::SpriteKey, std::pair<std::uint32_t, std::uint32_t>> frames;
    std::uint64_t failed = 0;

    [[nodiscard]] FrameTotals totals() const {
        FrameTotals t;
        t.frames = frames.size();
        for (const auto& [key, wh] : frames) {
            t.bytes += std::uint64_t{wh.first} * wh.second * 4;
        }
        return t;
    }
};

/// Every frame placed object `p` can ever show at `group`, added to `out` (shadow included when
/// its set has one). Mirrors place_prepared's own frame selection (iso.hpp "place_prepared";
/// iso.cpp animation()/frame_index(), lines 331-357) by calling the SAME public animation()
/// function, not a second implementation of it: frame_index's formula is
/// `step = (time_ms/ms + phase) % n` (ms == 0: step is the constant `phase % n`, no animation;
/// ms > 0: as time_ms grows without bound, step takes every value 0..n-1, so the full set an
/// object can show does not depend on sampling any particular time_ms). `id == kSurfAnlage`'s
/// position-derived phase is the one case frame_index's own caller special-cases (iso.hpp
/// place_prepared); every other object uses its own stored phase byte.
void add_object_frames(const render::Prepared& p, const formats::anlagen::AnlagenFile& anlagen,
                       std::uint32_t group, AtlasFrames& out) {
    const auto& o = p.first;
    const std::uint32_t id = o.anlage;
    const fig::Entry* entry = anlagen.entry(id);
    if (entry == nullptr || entry->gfx.empty()) {
        ++out.failed;
        return;
    }
    const fig::GfxSet& set = entry->gfx.front();
    const fig::FrameSet* frames = nullptr;
    for (const auto& fs : set.frame_sets) {
        if (fs.group == group) {
            frames = &fs;
            break;
        }
    }
    if (frames == nullptr) {
        ++out.failed;
        return;
    }
    const std::size_t n = std::max<std::size_t>(render::animation(set).first, 1);
    const std::uint64_t ms = render::animation(set).second;
    const std::size_t total = frames->frames.size();
    const std::size_t rotations = std::max<std::size_t>(total / n, 1);
    const std::size_t block = (std::size_t{o.rotation} % rotations) * n;
    const std::size_t phase = id == render::kSurfAnlage
                                  ? (3 * std::size_t{o.y} + 5 * std::size_t{o.x}) % 32
                                  : std::size_t{o.phase()};
    const bool animated = ms != 0 && n > 1;
    const std::size_t step_count = animated ? n : 1;
    const std::size_t first_step = phase % n;
    const fig::FrameSet* shadow_frames = nullptr;
    if (set.shadow) {
        for (const auto& fs : set.shadow->frame_sets) {
            if (fs.group == group) {
                shadow_frames = &fs;
                break;
            }
        }
    }
    for (std::size_t i = 0; i < step_count; ++i) {
        const std::size_t step = animated ? i : first_step;
        const std::size_t k = block + step;
        if (k >= total || frames->frames[k].length == 0) {
            continue;
        }
        const fig::Frame& f = frames->frames[k];
        out.frames.emplace(render::SpriteKey{id, 0, false, group, k},
                           std::pair<std::uint32_t, std::uint32_t>{f.width, f.height});
        if (shadow_frames != nullptr && k < shadow_frames->frames.size() &&
            shadow_frames->frames[k].length != 0) {
            const fig::Frame& sf = shadow_frames->frames[k];
            out.frames.emplace(render::SpriteKey{id, 0, true, group, k},
                               std::pair<std::uint32_t, std::uint32_t>{sf.width, sf.height});
        }
    }
}

/// `entry`'s own gfx set and its shadow, each with whether it is the shadow (mirrors
/// sprite_fidelity_test.cpp's own `sets()` helper, a trivial traversal, not scene/object
/// preparation).
std::vector<std::pair<const fig::GfxSet*, bool>> gfx_sets_of(const fig::Entry& entry) {
    std::vector<std::pair<const fig::GfxSet*, bool>> out;
    for (const auto& s : entry.gfx) {
        out.emplace_back(&s, false);
        if (s.shadow) {
            out.emplace_back(s.shadow.get(), true);
        }
    }
    return out;
}

/// The whole FigurenHeader.dat catalog per group (frames unconditionally resident, scene-
/// independent, docs/orchestration/tasks/render-direct-1a.md step 4) and, of those, the frames of
/// player-coloured types (step 5): `assets::figure::player_coloured` is the only place that checks
/// this (figure.cpp), and only non-shadow frames are ever recoloured (BSH_ALPHA shadows carry no
/// palette, figure.hpp's own header comment). `kGroups` groups (render::kGroups).
std::array<FrameTotals, render::kGroups>
whole_figure_catalog(const fig::FigurenHeader& header,
                     std::array<FrameTotals, render::kGroups>& player_colourable) {
    std::array<FrameTotals, render::kGroups> totals{};
    for (const auto& e : header.entries) {
        const bool coloured = assets::figure::player_coloured(e);
        for (const auto& [set, shadow] : gfx_sets_of(e)) {
            for (const auto& fs : set->frame_sets) {
                if (fs.group >= render::kGroups) {
                    continue;
                }
                for (const auto& f : fs.frames) {
                    if (f.length == 0) {
                        continue;
                    }
                    const std::uint64_t b = std::uint64_t{f.width} * f.height * 4;
                    totals[fs.group].frames += 1;
                    totals[fs.group].bytes += b;
                    if (coloured && !shadow) {
                        player_colourable[fs.group].frames += 1;
                        player_colourable[fs.group].bytes += b;
                    }
                }
            }
        }
    }
    return totals;
}

/// Pixels and frames of the whole Anlagen.dat catalog that the decoder's own player-colour test
/// would match, measured by decoding (not assumed from `player_coloured`/`recolour` never being
/// called on this path): `figure::recolour`'s test is `G^2 > trunc(1.5 (R^2 + B^2))` on a decoded
/// pixel's palette colour (figure.cpp); a pixel the test matches changes when recolour() is given
/// a real player colour and stays the same otherwise ("other colours stay", figure.hpp), so
/// comparing the decoded colour against `recolour(colour, test_player)` applies the real test
/// without reimplementing its formula. Decodes with `assets::building::to_rgba`, not
/// `figure::decode`: a building entry's palette is `entry.palette_ids[frame.palette]` against the
/// file-wide `AnlagenFile::colors` table, not `entry.palettes` (figure.hpp's per-entry COLORS,
/// FigurenHeader.dat's own convention) - the same function iso.cpp's SpriteCache::get calls for
/// every static frame it decodes (iso.cpp:322), confirming the decode path, not just the palette
/// lookup, is identical to the real one. `building::to_rgba` also has no colour parameter at all:
/// further, structural evidence that static frames are never recoloured. Shadows are skipped:
/// BSH_ALPHA frames carry no palette (figure.hpp's own header comment), so the test cannot apply.
struct ColourEligibility {
    std::uint64_t frames_checked = 0, frames_with_eligible_pixels = 0;
    std::uint64_t pixels_checked = 0, pixels_eligible = 0;
    std::uint64_t decode_failed = 0;
};

ColourEligibility static_player_colour_eligibility(const formats::anlagen::AnlagenFile& anlagen) {
    ColourEligibility result;
    const assets::figure::Rgb test_player = assets::figure::kOriginalPlayerColours[0];
    for (const auto& e : anlagen.entries) {
        for (const auto& [set, shadow] : gfx_sets_of(e)) {
            if (shadow) {
                continue;
            }
            for (const auto& fs : set->frame_sets) {
                for (const auto& frame : fs.frames) {
                    if (frame.length == 0) {
                        continue;
                    }
                    const auto decoded = assets::building::to_rgba(anlagen, e, fs.group, frame);
                    if (!decoded) {
                        ++result.decode_failed;
                        continue;
                    }
                    ++result.frames_checked;
                    bool frame_has_eligible = false;
                    const assets::Rgba& img = decoded.value();
                    for (std::size_t i = 0; i + 3 < img.pixels.size(); i += 4) {
                        if (img.pixels[i + 3] == 0) {
                            continue; // not covered: no palette colour to test.
                        }
                        ++result.pixels_checked;
                        const assets::figure::Rgb rgb{img.pixels[i], img.pixels[i + 1],
                                                      img.pixels[i + 2]};
                        if (assets::figure::recolour(rgb, test_player) != rgb) {
                            ++result.pixels_eligible;
                            frame_has_eligible = true;
                        }
                    }
                    if (frame_has_eligible) {
                        ++result.frames_with_eligible_pixels;
                    }
                }
            }
        }
    }
    return result;
}

/// The figure (ship/soldier) frames `scene.units` actually uses at `group`, distinct by
/// render::FrameKey::unit (frame_table.hpp), the engine's own identity for a unit frame - the same
/// key the frame table itself dedups by, so this counts exactly the atlas entries those units
/// would need. Empty when `scene.units` was never populated (load_placed_scene, unlike load_scene,
/// does not call load_units - app/src/loader.cpp): reported as "n/a", not zero.
FrameTotals figures_used_by_scene(const render::Scene& scene, std::uint32_t group) {
    FrameTotals t;
    std::map<render::FrameKey, std::uint32_t> seen; // -> bytes
    for (const auto& u : scene.units) {
        auto add = [&](const std::vector<std::optional<render::UnitFrame>>& frames, bool shadow) {
            if (group >= frames.size() || !frames[group]) {
                return;
            }
            const render::UnitFrame& f = *frames[group];
            seen.emplace(render::FrameKey::unit(f.id, group, shadow),
                         std::uint32_t{f.frame.width} * f.frame.height * 4);
        };
        add(u.frames, false);
        add(u.shadows, true);
    }
    for (const auto& [key, bytes] : seen) {
        t.frames += 1;
        t.bytes += bytes;
    }
    return t;
}

/// `only_file` alone, when it names a real file (not "-"): --atlas-report then measures just that
/// one scene (a quick check), instead of walking the whole corpus. Otherwise every `.sze`/`.scp`
/// under `game_dir/Scenes` (recursive: no fixed subfolder list, so a mod's own scenario folder is
/// covered the same way), then the repo's own lan5 fixed map and its three ship-start-v2
/// candidates, resolved against the current working directory (the report must be run from the
/// repo root).
std::vector<fs::path> discover_scenes(const fs::path& game_dir, const fs::path& only_file) {
    if (!only_file.empty() && only_file != "-") {
        return {only_file};
    }
    std::vector<fs::path> out;
    std::error_code ec;
    const fs::path scenes_dir = game_dir / "Scenes";
    for (auto it = fs::recursive_directory_iterator(
             scenes_dir, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            continue;
        }
        std::string ext = it->path().extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext == ".sze" || ext == ".scp") {
            out.push_back(it->path());
        }
    }
    std::sort(out.begin(), out.end());
    out.push_back("data/maps/lan5.txt");
    for (const char* name : {"lan5-ship-a.json", "lan5-ship-b.json", "lan5-ship-c.json"}) {
        out.push_back(fs::path("data/maps/candidates/ship-start-v2") / name);
    }
    return out;
}

/// `frames` packed into layers of `page_size`^2 texels, `padding` px added to each frame's width
/// and height (2 * padding: both sides), with `AtlasResidency` (src/render/include/anno/render/
/// residency.hpp), the shelf packer RENDER-DIRECT-1b is to use (task step 3). One `ensure()` call
/// packs the whole set at once (no eviction: the budget is sized for every layer `ensure()` could
/// ever need), so `stats().layers` is the real page count, not an estimate. A frame that does not
/// fit one page at all (`EnsureStatus::Overflow` on its own) is reported separately, not silently
/// dropped.
struct PackResult {
    std::uint64_t pages = 0, bytes = 0;
    /// Texel bytes of the packed rectangles themselves (ResidencyStats::resident_bytes, already
    /// padding-inflated): `bytes - rect_bytes` is shelf waste, not something a reader has to
    /// recompute to see whether a page count is a tight fit or has headroom to spare.
    std::uint64_t rect_bytes = 0;
    std::vector<std::uint32_t> oversized_ids; // padded w/h > page_size
};

PackResult
pack_frames(const std::map<render::SpriteKey, std::pair<std::uint32_t, std::uint32_t>>& frames,
            std::uint32_t page_size, std::uint32_t padding) {
    PackResult result;
    std::vector<render::FrameRef> refs;
    refs.reserve(frames.size());
    std::uint32_t next_id = 0;
    for (const auto& [key, wh] : frames) {
        const std::uint32_t w = wh.first + 2 * padding, h = wh.second + 2 * padding;
        if (w > page_size || h > page_size) {
            result.oversized_ids.push_back(next_id);
        }
        refs.push_back(render::FrameRef{next_id, w, h});
        ++next_id;
    }
    // Budget: as many layers as every frame could ever need, one per frame in the worst case
    // (never reached in practice), so ensure() never evicts within this single call.
    const std::uint64_t budget =
        std::uint64_t{page_size} * page_size * 4 * (refs.empty() ? 1 : refs.size());
    render::AtlasResidency residency(page_size, budget);
    const auto ensured = residency.ensure(refs, render::AtlasClass::Static);
    (void)ensured;
    const auto stats = residency.stats();
    result.pages = stats.layers;
    result.bytes = stats.layer_bytes;
    result.rect_bytes = stats.resident_bytes;
    return result;
}

/// One extra mip level's GPU byte cost, on top of the base page bytes `pack_frames` reports: a
/// level at half resolution in each dimension is a quarter of the base area, so +25%. Not a full
/// chain: a mip level n needs 2^n px of padding on every packed frame or its neighbours in the
/// atlas bleed into it when the GPU samples past the edge, so with the padding this task measures
/// (2 px, 4 px) only one level (2^1 = 2 px) is ever valid without more padding than either
/// candidate gives - a genuine full chain down to 1x1 for a 2048 px page would need 11 levels,
/// i.e. 2^11 = 2048 px of padding, not a usable candidate. One level is also most of the benefit
/// the zoom groups themselves give: their tile widths (84/42/20/10 px, docs/app/window-trace.md
/// "Zoom groups and their tile widths") never shrink more than about 2.1x before the next group
/// takes over, so a single half-resolution mip covers nearly all of the minification a zoom
/// between groups needs. This is a formula, not a measurement: flagged as one, not a guess.
inline constexpr double kOneMipLevelFactor = 1.25;

/// Whole file, no decode (mirrors app/src/loader.cpp's own private read_all: plain file I/O).
std::optional<std::vector<std::uint8_t>> atlas_read_all(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
}

#ifdef ANNO_BENCH_GPU
/// `maxTextureDimension2D`/`maxTextureArrayLayers`/`maxBufferSize` of the adapter `which` names
/// (list_adapters()'s order, or a case-insensitive part of its name - render::pick_adapter),
/// opened through ANNO_GPU_ADAPTER (render::headless_context's own env var; wgpu.hpp), the same
/// mechanism --list-adapters and every other GPU path here already uses. A few seconds: one
/// device open, one wgpuDeviceGetLimits call (the pattern at src/render/src/gpu.cpp
/// GpuFrame::create), no frame rendered.
struct GpuLimitsRow {
    std::string adapter;
    std::uint32_t max_texture_dimension_2d = 0, max_texture_array_layers = 0;
    std::uint64_t max_buffer_size = 0;
    std::string error;
};

GpuLimitsRow query_gpu_limits(std::string_view which) {
    GpuLimitsRow row;
    row.adapter = std::string(which);
#ifdef _WIN32
    _putenv_s("ANNO_GPU_ADAPTER", std::string(which).c_str());
#else
    setenv("ANNO_GPU_ADAPTER", std::string(which).c_str(), 1);
#endif
    auto cx = gpu::headless_context();
    if (!cx) {
        row.error = cx.error();
        return row;
    }
    row.adapter = cx.value().name;
    WGPULimits limits = WGPU_LIMITS_INIT;
    wgpuDeviceGetLimits(cx.value().device.get(), &limits);
    row.max_texture_dimension_2d = limits.maxTextureDimension2D;
    row.max_texture_array_layers = limits.maxTextureArrayLayers;
    row.max_buffer_size = limits.maxBufferSize;
    return row;
}
#endif

/// Runs RENDER-DIRECT-1a's measurement over discover_scenes(game_dir, file) and writes the table
/// to `o.atlas_report`. Returns the process exit code.
int run_atlas_report(const fs::path& game_dir, const fs::path& file, const Options& o) {
    const auto started = std::chrono::steady_clock::now();
    std::string tsv;
    // "group" is a zoom group (0/1/2) for a per-group row, or one of "0+1+2"/"0+1"/"1+2" for a
    // row summing that group's own frame sets (F1: ADR 0007 keeps all three resident at once; a
    // zoom crosses one adjacent pair). A sum row's descriptive columns (static_frames ..
    // figures_used_bytes) are 0: those facts are per group, not summable into one number.
    tsv += "# scenes\n";
    tsv += "scene\tgroup\tstatic_frames\tstatic_bytes\tstatic_instances\twater_frames\t"
           "water_bytes\twater_shadow_frames\twater_shadow_bytes\tenhanced_extra_frames\t"
           "enhanced_extra_bytes\tfigures_used_frames\tfigures_used_bytes\tpage_size\t"
           "padding_px\tmips\tpack_pages\tpack_bytes\trect_bytes\tfill_ratio\terror\n";

    struct PageSpec {
        std::uint32_t page_size, padding;
    };
    static constexpr std::array<PageSpec, 4> kSpecs = {
        {{2048, 2}, {2048, 4}, {4096, 2}, {4096, 4}}};
    static constexpr std::array<const char*, 2> kMipNames = {"none", "one_level"};

    const std::vector<fs::path> scenes = discover_scenes(game_dir, file);
    std::size_t loaded_ok = 0, refused = 0;
    std::uint64_t worst_combined_bytes = 0;
    std::string worst_scene;
    std::uint32_t worst_group = 0;

    std::vector<std::string> oversized_notes;
    for (const fs::path& scene_file : scenes) {
        std::string ext = scene_file.extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        const bool is_placed = ext == ".txt" || ext == ".json"; // load_placed_scene: no units.
        auto loaded = load_any_scene(game_dir, scene_file);
        if (!loaded) {
            // 19 empty fields (group .. fill_ratio) between scene and error: one tab per field
            // boundary, so 20 tabs for 19 empty fields plus the one before error. Built by
            // repetition, not counted by eye (TRACE-COLS-1's own lesson).
            tsv += scene_file.string() + std::string(20, '\t') + loaded.error() + "\n";
            ++refused;
            continue;
        }
        ++loaded_ok;
        const render::Scene& scene = *loaded.value().scene;
        const std::size_t group_count =
            std::min<std::size_t>(scene.levels().size(), render::kGroups);
        // F1 (orchestrator review, 2026-11-17): ADR 0007 keeps all three groups' atlases resident
        // at once (decision 2), and the owner's seamless-zoom rule means a zoom crossing two
        // groups needs both resident together, not one evicted while the zoom runs. Kept per
        // group here so the sums below are computed from the same frame sets, not re-walked.
        std::array<std::map<render::SpriteKey, std::pair<std::uint32_t, std::uint32_t>>,
                   render::kGroups>
            combined_by_group;
        std::array<bool, render::kGroups> group_present{};
        for (std::size_t g = 0; g < group_count; ++g) {
            const auto group = static_cast<std::uint32_t>(g);
            AtlasFrames static_frames, water_frames, enhanced_eligible;
            std::uint64_t static_instances = 0;
            for (const auto& island : scene.islands) {
                const auto statics = render::prepare_objects(
                    island, scene.anlagen, render::kObjectRecords, render::Objects::NotWater);
                static_instances += statics.size();
                for (const auto& p : statics) {
                    add_object_frames(p, scene.anlagen, group, static_frames);
                }
                const auto below = render::prepare_objects(
                    island, scene.anlagen, render::kObjectRecords, render::Objects::WaterBelowSea);
                for (const auto& p : below) {
                    add_object_frames(p, scene.anlagen, group, water_frames);
                    const std::uint32_t id = p.first.anlage;
                    const bool is_fall = std::find(render::kRiverFallAnlagen.begin(),
                                                   render::kRiverFallAnlagen.end(),
                                                   id) != render::kRiverFallAnlagen.end();
                    if (id != render::kSurfAnlage && !is_fall) {
                        add_object_frames(p, scene.anlagen, group, enhanced_eligible);
                    }
                }
                const auto above = render::prepare_objects(
                    island, scene.anlagen, render::kObjectRecords, render::Objects::WaterAboveSea);
                for (const auto& p : above) {
                    add_object_frames(p, scene.anlagen, group, water_frames);
                    const std::uint32_t id = p.first.anlage;
                    const bool is_river =
                        std::find(render::kRiverAnlagen.begin(), render::kRiverAnlagen.end(), id) !=
                        render::kRiverAnlagen.end();
                    const bool is_fall = std::find(render::kRiverFallAnlagen.begin(),
                                                   render::kRiverFallAnlagen.end(),
                                                   id) != render::kRiverFallAnlagen.end();
                    if (!is_river && !is_fall) {
                        add_object_frames(p, scene.anlagen, group, enhanced_eligible);
                    }
                }
            }
            const FrameTotals static_totals = static_frames.totals();
            std::uint64_t water_nf = 0, water_nb = 0, water_sf = 0, water_sb = 0;
            for (const auto& [key, wh] : water_frames.frames) {
                const std::uint64_t b = std::uint64_t{wh.first} * wh.second * 4;
                if (std::get<2>(key)) {
                    ++water_sf;
                    water_sb += b;
                } else {
                    ++water_nf;
                    water_nb += b;
                }
            }
            std::uint64_t extra_f = 0, extra_b = 0;
            for (const auto& [key, wh] : enhanced_eligible.frames) {
                if (!std::get<2>(key)) {
                    ++extra_f;
                    extra_b += std::uint64_t{wh.first} * wh.second * 4;
                }
            }
            const FrameTotals figs =
                is_placed ? FrameTotals{} : figures_used_by_scene(scene, group);

            std::map<render::SpriteKey, std::pair<std::uint32_t, std::uint32_t>> combined =
                static_frames.frames;
            combined.insert(water_frames.frames.begin(), water_frames.frames.end());
            combined_by_group[g] = combined;
            group_present[g] = true;

            for (const PageSpec& spec : kSpecs) {
                const PackResult packed = pack_frames(combined, spec.page_size, spec.padding);
                if (!packed.oversized_ids.empty()) {
                    oversized_notes.push_back(scene_file.string() + " group " +
                                              std::to_string(group) + " page " +
                                              std::to_string(spec.page_size) + " padding " +
                                              std::to_string(spec.padding) + ": " +
                                              std::to_string(packed.oversized_ids.size()) +
                                              " frame(s) too large for one page even alone");
                }
                if (packed.bytes > worst_combined_bytes) {
                    worst_combined_bytes = packed.bytes;
                    worst_scene = scene_file.string();
                    worst_group = group;
                }
                for (std::size_t mip = 0; mip < kMipNames.size(); ++mip) {
                    const std::uint64_t bytes =
                        mip == 0 ? packed.bytes
                                 : static_cast<std::uint64_t>(static_cast<double>(packed.bytes) *
                                                              kOneMipLevelFactor);
                    // fill_ratio is always against the un-mipped page bytes: mips are extra GPU
                    // storage on top of the same pages, not a second packing of the rectangles.
                    const double fill_ratio = packed.bytes == 0
                                                  ? 0.0
                                                  : static_cast<double>(packed.rect_bytes) /
                                                        static_cast<double>(packed.bytes);
                    char line[1024];
                    std::snprintf(
                        line, sizeof line,
                        "%s\t%u\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t"
                        "%llu\t%u\t%u\t%s\t%llu\t%llu\t%llu\t%.4f\t\n",
                        scene_file.string().c_str(), group,
                        static_cast<unsigned long long>(static_totals.frames),
                        static_cast<unsigned long long>(static_totals.bytes),
                        static_cast<unsigned long long>(static_instances),
                        static_cast<unsigned long long>(water_nf),
                        static_cast<unsigned long long>(water_nb),
                        static_cast<unsigned long long>(water_sf),
                        static_cast<unsigned long long>(water_sb),
                        static_cast<unsigned long long>(extra_f),
                        static_cast<unsigned long long>(extra_b),
                        static_cast<unsigned long long>(figs.frames),
                        static_cast<unsigned long long>(figs.bytes), spec.page_size, spec.padding,
                        kMipNames[mip], static_cast<unsigned long long>(packed.pages),
                        static_cast<unsigned long long>(bytes),
                        static_cast<unsigned long long>(packed.rect_bytes), fill_ratio);
                    tsv += line;
                }
            }
        }

        // F1: the sums ADR 0007's own "all three resident" rule and the owner's seamless-zoom
        // rule need - all three groups together, and the two adjacent pairs a zoom crosses - from
        // the same per-group frame sets above, not a second corpus walk. Descriptive columns
        // (static_frames, water_frames, ...) are per-group facts with no single meaning for a
        // sum, so they are left at 0; only the packing columns are. "group" names the sum.
        struct GroupSum {
            const char* label;
            std::array<std::size_t, 3> groups;
            std::size_t count;
        };
        static constexpr std::array<GroupSum, 3> kSums = {
            {{"0+1+2", {0, 1, 2}, 3}, {"0+1", {0, 1, 0}, 2}, {"1+2", {1, 2, 0}, 2}}};
        for (const GroupSum& sum : kSums) {
            bool all_present = true;
            for (std::size_t i = 0; i < sum.count; ++i) {
                if (sum.groups[i] >= group_count || !group_present[sum.groups[i]]) {
                    all_present = false;
                    break;
                }
            }
            if (!all_present) {
                continue;
            }
            std::map<render::SpriteKey, std::pair<std::uint32_t, std::uint32_t>> merged;
            for (std::size_t i = 0; i < sum.count; ++i) {
                merged.insert(combined_by_group[sum.groups[i]].begin(),
                              combined_by_group[sum.groups[i]].end());
            }
            for (const PageSpec& spec : kSpecs) {
                const PackResult packed = pack_frames(merged, spec.page_size, spec.padding);
                if (!packed.oversized_ids.empty()) {
                    oversized_notes.push_back(scene_file.string() + " groups " + sum.label +
                                              " page " + std::to_string(spec.page_size) +
                                              " padding " + std::to_string(spec.padding) + ": " +
                                              std::to_string(packed.oversized_ids.size()) +
                                              " frame(s) too large for one page even alone");
                }
                for (std::size_t mip = 0; mip < kMipNames.size(); ++mip) {
                    const std::uint64_t bytes =
                        mip == 0 ? packed.bytes
                                 : static_cast<std::uint64_t>(static_cast<double>(packed.bytes) *
                                                              kOneMipLevelFactor);
                    const double fill_ratio = packed.bytes == 0
                                                  ? 0.0
                                                  : static_cast<double>(packed.rect_bytes) /
                                                        static_cast<double>(packed.bytes);
                    char line[1024];
                    std::snprintf(line, sizeof line,
                                  "%s\t%s\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t%u\t%u\t%s\t%llu\t"
                                  "%llu\t%llu\t%.4f\t\n",
                                  scene_file.string().c_str(), sum.label, spec.page_size,
                                  spec.padding, kMipNames[mip],
                                  static_cast<unsigned long long>(packed.pages),
                                  static_cast<unsigned long long>(bytes),
                                  static_cast<unsigned long long>(packed.rect_bytes), fill_ratio);
                    tsv += line;
                }
            }
        }
    }

    tsv += "\n# figures (whole FigurenHeader.dat catalog, scene-independent)\n";
    tsv += "group\tframes\tbytes\tplayer_colourable_frames\tplayer_colourable_bytes\n";
    auto header_buf = atlas_read_all(game_dir / "FigurenHeader.dat");
    if (header_buf) {
        auto header = fig::FigurenHeader::parse(*header_buf);
        if (header) {
            std::array<FrameTotals, render::kGroups> coloured{};
            const auto whole = whole_figure_catalog(header.value(), coloured);
            for (std::size_t g = 0; g < render::kGroups; ++g) {
                char line[256];
                std::snprintf(line, sizeof line, "%zu\t%llu\t%llu\t%llu\t%llu\n", g,
                              static_cast<unsigned long long>(whole[g].frames),
                              static_cast<unsigned long long>(whole[g].bytes),
                              static_cast<unsigned long long>(coloured[g].frames),
                              static_cast<unsigned long long>(coloured[g].bytes));
                tsv += line;
            }
        } else {
            tsv += "# FigurenHeader.dat: " + header.error() + "\n";
        }
    } else {
        tsv += "# cannot read FigurenHeader.dat\n";
    }

    tsv += "\n# static_player_colour (whole Anlagen.dat catalog, decoded, scene-independent: "
           "pixels/frames the decoder's own test would recolour, not assumed from the engine\n"
           "# never calling recolour() on this path)\n";
    tsv += "frames_checked\tframes_with_eligible_pixels\tpixels_checked\tpixels_eligible\t"
           "decode_failed\n";
    {
        auto anlagen_buf = atlas_read_all(game_dir / "Anlagen.dat");
        if (anlagen_buf) {
            auto anlagen = formats::anlagen::AnlagenFile::parse(*anlagen_buf);
            if (anlagen) {
                const auto elig = static_player_colour_eligibility(anlagen.value());
                char line[256];
                std::snprintf(line, sizeof line, "%llu\t%llu\t%llu\t%llu\t%llu\n",
                              static_cast<unsigned long long>(elig.frames_checked),
                              static_cast<unsigned long long>(elig.frames_with_eligible_pixels),
                              static_cast<unsigned long long>(elig.pixels_checked),
                              static_cast<unsigned long long>(elig.pixels_eligible),
                              static_cast<unsigned long long>(elig.decode_failed));
                tsv += line;
            } else {
                tsv += "# Anlagen.dat: " + anlagen.error() + "\n";
            }
        } else {
            tsv += "# cannot read Anlagen.dat\n";
        }
    }

#ifdef ANNO_BENCH_GPU
    if (o.atlas_gpu_limits) {
        tsv += "\n# gpu_limits\n";
        tsv += "adapter\tmax_texture_dimension_2d\tmax_texture_array_layers\tmax_buffer_size\t"
               "error\n";
        // Not "4090": pick_adapter treats a numeric string as an index, never a substring match
        // (wgpu.hpp), and 4090 is not a valid index into list_adapters() (--list-adapters: 0
        // "NVIDIA GeForce RTX 4090 (Vulkan)" .. 5). "RTX"/"Radeon" match by name instead.
        for (std::string_view which : {"RTX", "Radeon"}) {
            const auto row = query_gpu_limits(which);
            char line[512];
            std::snprintf(line, sizeof line, "%s\t%u\t%u\t%llu\t%s\n", row.adapter.c_str(),
                          row.max_texture_dimension_2d, row.max_texture_array_layers,
                          static_cast<unsigned long long>(row.max_buffer_size), row.error.c_str());
            tsv += line;
        }
    }
#endif

    if (!oversized_notes.empty()) {
        tsv += "\n# oversized (a frame too large for one page even alone: its row's pack_pages/"
               "pack_bytes/rect_bytes/fill_ratio reflect the frames placed before ensure()\n"
               "# returned Overflow, not the true need)\n";
        for (const std::string& note : oversized_notes) {
            tsv += "# " + note + "\n";
        }
    }

    const std::string err = app::save_whole(o.atlas_report, tsv);
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    if (!oversized_notes.empty()) {
        std::fprintf(stderr,
                     "atlas-report: %zu oversized-frame case(s), see the TSV's "
                     "# oversized section\n",
                     oversized_notes.size());
    }
    std::fprintf(stderr,
                 "atlas-report: %zu scenes loaded, %zu refused, %.0f ms\n"
                 "worst combined (static+water) packed bytes: %llu (%s, group %u)\n",
                 loaded_ok, refused, ms, static_cast<unsigned long long>(worst_combined_bytes),
                 worst_scene.c_str(), worst_group);
    if (!err.empty()) {
        std::fprintf(stderr, "atlas-report: %s\n", err.c_str());
        return 1;
    }
    std::fprintf(stderr, "atlas-report: wrote %s\n", o.atlas_report.c_str());
    return 0;
}

// --- RENDER-DIRECT-1b step 5: --atlas-build ---------------------------------------------------
// Actually builds a scene's resident atlas pages (render::build_atlas_pages), rather than only
// sizing them as --atlas-report does: real wall time, the real byte cost of the one mip level
// that build actually produces (not kOneMipLevelFactor's formula), and the process's peak memory.

/// `scene`'s static+water frames across every group below `group_count`, decoded and deduped into
/// one shared FrameTable + SpriteCache (the engine's own decode path - assets::building::to_rgba,
/// iso.cpp's SpriteCache::get - no second decoder): the real combined set a session holds at once
/// (ADR 0007 decision 2, all three groups resident together), not one group at a time. Mirrors
/// atlas_pages_corpus_test.cpp's own collect_static_water, folded over every group into one table
/// instead of called once per group with its own: `already` is declared once, outside the group
/// loop, so a frame a later group's own object also reaches (a different SpriteKey, since the key
/// carries `group`) is still only pushed into `out` once per distinct FrameId, and a frame two
/// objects within the SAME group share (the dedup `add_object_frames`/this project's add_object
/// itself already relies on) is not pushed twice either.
std::vector<render::AtlasFrameInput> collect_static_water_all_groups(const render::Scene& scene,
                                                                     std::size_t group_count,
                                                                     render::SpriteCache& cache,
                                                                     render::FrameTable& table) {
    auto add_object = [&](const render::Prepared& p, std::uint32_t group, bool enhanced_eligible) {
        const std::uint32_t id = p.first.anlage;
        const fig::Entry* entry = scene.anlagen.entry(id);
        if (entry == nullptr || entry->gfx.empty()) {
            return;
        }
        const fig::GfxSet& set = entry->gfx.front();
        const fig::FrameSet* frames = nullptr;
        for (const auto& fs : set.frame_sets) {
            if (fs.group == group) {
                frames = &fs;
                break;
            }
        }
        if (frames == nullptr) {
            return;
        }
        const std::size_t n = std::max<std::size_t>(render::animation(set).first, 1);
        const std::uint64_t ms = render::animation(set).second;
        const std::size_t total = frames->frames.size();
        const std::size_t rotations = std::max<std::size_t>(total / n, 1);
        const std::size_t block = (std::size_t{p.first.rotation} % rotations) * n;
        const std::size_t phase =
            id == render::kSurfAnlage
                ? (3 * std::size_t{p.first.y} + 5 * std::size_t{p.first.x}) % 32
                : std::size_t{p.first.phase()};
        const bool animated = ms != 0 && n > 1;
        const std::size_t step_count = animated ? n : 1;
        const std::size_t first_step = phase % n;
        for (std::size_t i = 0; i < step_count; ++i) {
            const std::size_t step = animated ? i : first_step;
            const std::size_t k = block + step;
            if (k >= total || frames->frames[k].length == 0) {
                continue;
            }
            const render::SpriteKey key{id, 0, false, group, k};
            table.intern_building(scene.anlagen, cache, key, frames->frames[k]);
            if (enhanced_eligible) {
                const render::SpriteKey smoothed{id, render::kSmoothed, false, group, k};
                table.intern_building(scene.anlagen, cache, smoothed, frames->frames[k]);
            }
        }
    };
    for (std::size_t g = 0; g < group_count; ++g) {
        const auto group = static_cast<std::uint32_t>(g);
        for (const auto& island : scene.islands) {
            const auto statics = render::prepare_objects(
                island, scene.anlagen, render::kObjectRecords, render::Objects::NotWater);
            for (const auto& p : statics) {
                add_object(p, group, false);
            }
            const auto below = render::prepare_objects(
                island, scene.anlagen, render::kObjectRecords, render::Objects::WaterBelowSea);
            for (const auto& p : below) {
                const std::uint32_t id = p.first.anlage;
                const bool is_fall =
                    std::find(render::kRiverFallAnlagen.begin(), render::kRiverFallAnlagen.end(),
                              id) != render::kRiverFallAnlagen.end();
                add_object(p, group, id != render::kSurfAnlage && !is_fall);
            }
            const auto above = render::prepare_objects(
                island, scene.anlagen, render::kObjectRecords, render::Objects::WaterAboveSea);
            for (const auto& p : above) {
                const std::uint32_t id = p.first.anlage;
                const bool is_river =
                    std::find(render::kRiverAnlagen.begin(), render::kRiverAnlagen.end(), id) !=
                    render::kRiverAnlagen.end();
                const bool is_fall =
                    std::find(render::kRiverFallAnlagen.begin(), render::kRiverFallAnlagen.end(),
                              id) != render::kRiverFallAnlagen.end();
                add_object(p, group, !is_river && !is_fall);
            }
        }
    }
    std::vector<render::AtlasFrameInput> out;
    for (render::FrameId id = 0; id < static_cast<render::FrameId>(table.size()); ++id) {
        if (const auto data = table.get(id)) {
            out.push_back(render::AtlasFrameInput{id, data->image});
        }
    }
    return out;
}

/// `scene.units`' figure frames across every group below `group_count`, deduped into one shared
/// FrameTable (render::FrameKey::unit, the frame table's own identity for a unit frame). Empty
/// for a placed scene (fixed map / ship-start): load_placed_scene does not populate scene.units
/// (F3, 2026-11-17 RENDER-DIRECT-1a review) - the caller reports this as "n/a", not measured as a
/// true zero build cost.
std::vector<render::AtlasFrameInput> collect_figures_all_groups(const render::Scene& scene,
                                                                std::size_t group_count,
                                                                render::FrameTable& table) {
    std::vector<render::AtlasFrameInput> out;
    std::map<render::FrameId, bool> already;
    auto add = [&](const std::vector<std::optional<render::UnitFrame>>& frames, std::uint32_t group,
                   bool shadow) {
        if (group >= frames.size() || !frames[group]) {
            return;
        }
        const render::UnitFrame& f = *frames[group];
        const auto id = table.intern(render::FrameKey::unit(f.id, group, shadow),
                                     [&] { return std::pair{f.frame, f.image}; });
        if (id && !already.contains(*id)) {
            already[*id] = true;
            out.push_back(render::AtlasFrameInput{*id, f.image});
        }
    };
    for (const auto& u : scene.units) {
        for (std::size_t g = 0; g < group_count; ++g) {
            const auto group = static_cast<std::uint32_t>(g);
            add(u.frames, group, false);
            add(u.shadows, group, true);
        }
    }
    return out;
}

/// Total bytes actually written across every page's level 0 (or level 1) buffer - the real size
/// of the vectors `build_atlas_pages` allocated, not page_count * page_size^2 * 4 recomputed (the
/// two agree exactly today, since every page is pre-sized to the full page_size^2 * 4 regardless
/// of how full it is, but summing the real buffers is the fact this function should report, not
/// an arithmetic restatement of page_size that would silently go stale if that changed).
std::uint64_t total_bytes(const std::vector<std::vector<std::uint8_t>>& layers) {
    std::uint64_t total = 0;
    for (const auto& layer : layers) {
        total += layer.size();
    }
    return total;
}

/// Runs RENDER-DIRECT-1b step 5's measurement over discover_scenes(game_dir, file) and writes the
/// table to `o.atlas_build`. Returns the process exit code.
int run_atlas_build(const fs::path& game_dir, const fs::path& file, const Options& o) {
    const auto started = std::chrono::steady_clock::now();
    std::string tsv;
    // "collect" is decoding + dedup (sequential: one object/frame at a time, no worker split -
    // see collect_static_water_all_groups/collect_figures_all_groups); "pack" is
    // render::build_atlas_pages itself, split over `threads` workers. Reported separately (not
    // only their sum) because the two have very different parallelism, and a reader asking "is
    // this wall time with every worker busy, or one thread doing it all" needs the split, not
    // just the total - `*_cpu_ms` (process-wide user+sys time, cpu_time_ms()) answers that
    // directly: close to wall time at `threads == 1`, higher the more workers ran at once.
    tsv += "scene\tthreads\tstatic_frames\tstatic_pages\tstatic_level0_bytes\t"
           "static_level1_bytes\tstatic_bytes\tstatic_oversized\tstatic_collect_ms\t"
           "static_pack_ms\tstatic_ms\tstatic_cpu_ms\tfigures_frames\tfigures_pages\t"
           "figures_level0_bytes\tfigures_level1_bytes\tfigures_bytes\tfigures_oversized\t"
           "figures_collect_ms\tfigures_pack_ms\tfigures_ms\tfigures_cpu_ms\ttotal_bytes\t"
           "total_ms\terror\n";

    struct BuildRow {
        std::string scene;
        std::uint64_t total_bytes = 0;
        double total_ms = 0.0;
    };
    std::vector<BuildRow> rows;
    std::size_t loaded_ok = 0, refused = 0;

    const std::vector<fs::path> scenes = discover_scenes(game_dir, file);
    for (const fs::path& scene_file : scenes) {
        std::string ext = scene_file.extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        const bool is_placed = ext == ".txt" || ext == ".json"; // load_placed_scene: no units.
        auto loaded = load_any_scene(game_dir, scene_file);
        if (!loaded) {
            // 24 empty fields (threads .. total_ms) between scene and error: built by
            // repetition, not counted by eye (TRACE-COLS-1's own lesson).
            tsv += scene_file.string() + std::string(24, '\t') + loaded.error() + "\n";
            ++refused;
            continue;
        }
        ++loaded_ok;
        const render::Scene& scene = *loaded.value().scene;
        const std::size_t group_count =
            std::min<std::size_t>(scene.levels().size(), render::kGroups);

        render::SpriteCache cache;
        render::FrameTable static_table;
        const auto static_collect_t0 = std::chrono::steady_clock::now();
        const double static_cpu0 = cpu_time_ms();
        const auto static_frames =
            collect_static_water_all_groups(scene, group_count, cache, static_table);
        const double static_collect_ms = std::chrono::duration<double, std::milli>(
                                             std::chrono::steady_clock::now() - static_collect_t0)
                                             .count();
        const auto static_pack_t0 = std::chrono::steady_clock::now();
        const render::AtlasPages static_pages =
            render::build_atlas_pages(static_frames, 2048, 2, o.threads);
        const double static_pack_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - static_pack_t0)
                                          .count();
        const double static_ms = static_collect_ms + static_pack_ms;
        const double static_cpu_ms = cpu_time_ms() - static_cpu0;
        const std::uint64_t static_l0 = total_bytes(static_pages.level0);
        const std::uint64_t static_l1 = total_bytes(static_pages.level1);

        bool have_figures = !is_placed;
        render::FrameTable figure_table;
        std::vector<render::AtlasFrameInput> figure_frames;
        render::AtlasPages figure_pages;
        double figures_collect_ms = 0.0, figures_pack_ms = 0.0, figures_cpu_ms = 0.0;
        if (have_figures) {
            const auto figures_collect_t0 = std::chrono::steady_clock::now();
            const double figures_cpu0 = cpu_time_ms();
            figure_frames = collect_figures_all_groups(scene, group_count, figure_table);
            figures_collect_ms = std::chrono::duration<double, std::milli>(
                                     std::chrono::steady_clock::now() - figures_collect_t0)
                                     .count();
            const auto figures_pack_t0 = std::chrono::steady_clock::now();
            figure_pages = render::build_atlas_pages(figure_frames, 2048, 2, o.threads);
            figures_pack_ms = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - figures_pack_t0)
                                  .count();
            figures_cpu_ms = cpu_time_ms() - figures_cpu0;
        }
        const double figures_ms = figures_collect_ms + figures_pack_ms;
        const std::uint64_t figures_l0 = have_figures ? total_bytes(figure_pages.level0) : 0;
        const std::uint64_t figures_l1 = have_figures ? total_bytes(figure_pages.level1) : 0;
        const std::uint64_t static_bytes = static_l0 + static_l1;
        const std::uint64_t figures_bytes = figures_l0 + figures_l1;
        const std::uint64_t scene_total_bytes = static_bytes + figures_bytes;
        const double scene_total_ms = static_ms + figures_ms;

        char line[1536];
        if (have_figures) {
            std::snprintf(
                line, sizeof line,
                "%s\t%zu\t%zu\t%llu\t%llu\t%llu\t%llu\t%zu\t%.3f\t%.3f\t%.3f\t%.3f\t%zu\t%llu\t"
                "%llu\t%llu\t%llu\t%zu\t%.3f\t%.3f\t%.3f\t%.3f\t%llu\t%.3f\t\n",
                scene_file.string().c_str(), o.threads, static_frames.size(),
                static_cast<unsigned long long>(static_pages.level0.size()),
                static_cast<unsigned long long>(static_l0),
                static_cast<unsigned long long>(static_l1),
                static_cast<unsigned long long>(static_bytes), static_pages.oversized.size(),
                static_collect_ms, static_pack_ms, static_ms, static_cpu_ms, figure_frames.size(),
                static_cast<unsigned long long>(figure_pages.level0.size()),
                static_cast<unsigned long long>(figures_l0),
                static_cast<unsigned long long>(figures_l1),
                static_cast<unsigned long long>(figures_bytes), figure_pages.oversized.size(),
                figures_collect_ms, figures_pack_ms, figures_ms, figures_cpu_ms,
                static_cast<unsigned long long>(scene_total_bytes), scene_total_ms);
        } else {
            std::snprintf(
                line, sizeof line,
                "%s\t%zu\t%zu\t%llu\t%llu\t%llu\t%llu\t%zu\t%.3f\t%.3f\t%.3f\t%.3f\tn/a\tn/a\t"
                "n/a\tn/a\tn/a\tn/a\tn/a\tn/a\tn/a\tn/a\t%llu\t%.3f\t\n",
                scene_file.string().c_str(), o.threads, static_frames.size(),
                static_cast<unsigned long long>(static_pages.level0.size()),
                static_cast<unsigned long long>(static_l0),
                static_cast<unsigned long long>(static_l1),
                static_cast<unsigned long long>(static_bytes), static_pages.oversized.size(),
                static_collect_ms, static_pack_ms, static_ms, static_cpu_ms,
                static_cast<unsigned long long>(scene_total_bytes), scene_total_ms);
        }
        tsv += line;
        rows.push_back(BuildRow{scene_file.string(), scene_total_bytes, scene_total_ms});
    }

    const long long peak_rss = peak_rss_bytes();
    tsv += "\n# summary\n";
    tsv += "metric\tvalue\n";
    if (!rows.empty()) {
        std::vector<BuildRow> by_bytes = rows, by_ms = rows;
        std::sort(by_bytes.begin(), by_bytes.end(), [](const BuildRow& a, const BuildRow& b) {
            return a.total_bytes < b.total_bytes;
        });
        std::sort(by_ms.begin(), by_ms.end(),
                  [](const BuildRow& a, const BuildRow& b) { return a.total_ms < b.total_ms; });
        const BuildRow& worst_bytes = by_bytes.back();
        const BuildRow& worst_ms = by_ms.back();
        const BuildRow& median_bytes = by_bytes[by_bytes.size() / 2];
        const BuildRow& median_ms = by_ms[by_ms.size() / 2];
        char line[512];
        std::snprintf(line, sizeof line, "worst_total_bytes\t%llu (%s)\n",
                      static_cast<unsigned long long>(worst_bytes.total_bytes),
                      worst_bytes.scene.c_str());
        tsv += line;
        std::snprintf(line, sizeof line, "median_total_bytes\t%llu (%s)\n",
                      static_cast<unsigned long long>(median_bytes.total_bytes),
                      median_bytes.scene.c_str());
        tsv += line;
        std::snprintf(line, sizeof line, "worst_total_ms\t%.3f (%s)\n", worst_ms.total_ms,
                      worst_ms.scene.c_str());
        tsv += line;
        std::snprintf(line, sizeof line, "median_total_ms\t%.3f (%s)\n", median_ms.total_ms,
                      median_ms.scene.c_str());
        tsv += line;
    }
    {
#if defined(_WIN32)
        const char* source = "PeakWorkingSetSize";
#elif defined(__linux__)
        const char* source = "VmHWM";
#else
        const char* source = "unavailable";
#endif
        char line[128];
        std::snprintf(line, sizeof line, "peak_rss_bytes\t%lld\n", peak_rss);
        tsv += line;
        tsv += "peak_rss_source\t" + std::string(peak_rss < 0 ? "unavailable" : source) + "\n";
    }

    const std::string err = app::save_whole(o.atlas_build, tsv);
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    std::fprintf(stderr,
                 "atlas-build: %zu scenes loaded, %zu refused, %.0f ms total, peak rss %lld "
                 "bytes\n",
                 loaded_ok, refused, ms, peak_rss);
    if (!err.empty()) {
        std::fprintf(stderr, "atlas-build: %s\n", err.c_str());
        return 1;
    }
    std::fprintf(stderr, "atlas-build: wrote %s\n", o.atlas_build.c_str());
    return 0;
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
    if (!o->atlas_report.empty()) {
        return run_atlas_report(dir, file, *o);
    }
    if (!o->atlas_build.empty()) {
        return run_atlas_build(dir, file, *o);
    }
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
    auto loaded = load_any_scene(dir, file);
    if (!loaded) {
        std::fprintf(stderr, "%s: %s\n", file.string().c_str(), loaded.error().c_str());
        return 1;
    }
    loaded.value().scene->river_cfg.light_gain = o->river_gain;
    loaded.value().scene->depth_test = o->depth;
    loaded.value().scene->shore_height = o->shore;
    if (o->fill_time || o->derived > 0) {
        loaded.value().scene->set_levels(render::ZoomLevels::with_derived(
            std::max<std::size_t>(o->derived, o->fill_time ? 3 : 0)));
    }
    for (const std::size_t g : o->groups) {
        if (g >= loaded.value().scene->levels().size() ||
            (!o->profile_threads.empty() && g >= render::kGroups)) {
            std::fprintf(stderr,
                         "--groups %zu: no such zoom level (%zu levels; --derived n adds "
                         "levels after group 2; --chunk-profile needs built groups)\n",
                         g, loaded.value().scene->levels().size());
            return 2;
        }
    }
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
    if (!o->profile_threads.empty()) {
        return chunk_profile(dir, file, *o, largest);
    }
    if (o->fill_time) {
        return fill_time(scene, *o, largest);
    }
    std::printf("%s — %u threads available — %zu threads%s\n\n", file.string().c_str(),
                std::thread::hardware_concurrency(), o->threads, o->quick ? " — quick" : "");
    if (o->scale_cost.empty() && o->zoom_sweep <= 0.0) {
        std::printf(
            "| resolution | group | water | renderer | chunks | path | frames | p50 ms | p95 ms | "
            "p99 ms | max ms | ≤16.7 ms | built | chunk ms | missing | stage 1 | stage 2 | "
            "stage 3 | stage 4 |\n");
        std::printf(
            "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    }
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
        limits.gpu_timing = o->river_pass || !o->scale_cost.empty() || o->zoom_sweep > 0.0;
        if (o->wave_fade_hold >= 0.0) {
            limits.wave_fade_ms = 1000;
        }
        if (o->zoom_sweep > 0.0 && limits.prewarm_chunks > 0) {
            // As the viewer: a view's worth of chunks of the window, never fewer.
            limits.prewarm_chunks =
                gpu::prewarm_plan(o->resolutions.front().first, o->resolutions.front().second,
                                  limits.idle_plane_budget_mb)
                    .chunks;
        }
        if (o->fade) {
            limits.chunk_fade_ms = *o->fade;
        }
        if (o->prefetch) {
            limits.prefetch_chunks = *o->prefetch;
        }
        if (o->prefetch_budget_ms) {
            limits.prefetch_budget_ms = *o->prefetch_budget_ms;
        }
        if (o->prewarm) {
            limits.prewarm_chunks = *o->prewarm;
        }
        if (o->stand_ins) {
            limits.stand_ins = *o->stand_ins;
        }
        if (o->wave_sim) {
            limits.wave_simulation = *o->wave_sim;
        }
        if (o->fluid_cells) {
            limits.fluid_max_cells_per_px = *o->fluid_cells;
        }
        auto f = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm, limits);
        if (!f) {
            std::fprintf(stderr, "GPU: %s\n", f.error().c_str());
            return 1;
        }
        frame_gpu = std::move(f).value();
        std::fprintf(stderr, "wave simulation bound: %.3f cells per screen pixel (0 = none)\n",
                     frame_gpu->fluid_max_cells_per_px());
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
#ifdef ANNO_BENCH_GPU
    if (o->zoom_sweep > 0.0) {
        if (!frame_gpu) {
            std::fputs("--zoom-sweep needs --renderer gpu\n", stderr);
            return 2;
        }
        return zoom_sweep(scene, *o, *frame_gpu, largest, o->zoom_sweep, file, ctx->adapter_type);
    }
    if (!o->scale_cost.empty()) {
        if (!frame_gpu) {
            std::fputs("--scale-cost needs --renderer gpu\n", stderr);
            return 2;
        }
        for (const auto& pt : o->scale_cost) {
            scene.sea(pt.group);
        }
        return scale_cost(scene, *o, *frame_gpu, largest, file.stem().string());
    }
#endif
    for (const auto& [w, h] : o->resolutions) {
        for (const std::size_t group : o->groups) {
            for (const auto style : o->styles) {
                for (const bool on_gpu : o->renderers) {
                    for (const bool async_chunks : o->modes) {
                        const auto b = island_bounds(scene, group, largest);
                        if (!b) {
                            std::fputs("no placement for that island\n", stderr);
                            return 1;
                        }
                        Config c;
                        c.chunk_builder = o->chunk_builder;
                        c.bin_px = o->bin_px;
                        c.w = w;
                        c.h = h;
                        c.group = group;
                        c.style = style;
                        c.async_chunks = async_chunks;
                        c.centre = {b->x + b->w / 2 + o->at.first, b->y + b->h / 2 + o->at.second};
                        c.frames = 120 / scale;
                        c.t0 = o->t0;
                        c.warmup = o->warmup;
                        c.threads = o->threads;
                        c.check = o->check;
                        c.settle_ms = o->settle_ms;
                        c.river_flow = o->river_flow;
                        c.river_pass = o->river_pass;
                        c.zoom_factor = o->zoom_factor;
                        c.resample = o->resample;
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
