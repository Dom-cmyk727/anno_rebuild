// The anno1503 command line (same options as the Rust viewer,
// crates/app/src/main.rs). Strings are UTF-8 on every platform; paths are
// built with utf8_path so non-ASCII file names work on Windows too.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/render/iso.hpp>
#include <anno/render/resample.hpp>
#include <anno/render/river.hpp>

namespace anno::app {

inline constexpr const char* kUsage =
    "usage: anno1503 --view <file.sze|file.scp> | --play <file.sze> [--game-dir <dir>] [--speed 50|100|200] [--group "
    "<n>] [--png "
    "<out.png> [--time <ms>] [--at <x>,<y> [--size <w>x<h>]]] [--water original|enhanced] "
    "[--river-flow legacy|flow] [--river-gain <x>] [--river-speed median|fastest] "
    "[--river-gap-patch <tiles>] [--renderer auto|gpu|cpu] [--lookahead-chunks <n>] "
    "[--zoom-mode continuous|stepped] [--zoom-step <factor>] [--zoom-ease <ms>] "
    "[--min-zoom <tile width>] [--max-derived-levels <n>] [--zoom-max-upscale <factor>]\n"
    "(--group 0-2: the built zoom levels; 3 and up: derived levels, --png only, bounded by "
    "--max-derived-levels)\n"
    "(--png only: [--zoom-factor <scale>] [--resample nearest|bilinear|box|area], the picture "
    "composed at --group and resampled by the CPU model of the GPU pass)\n"
    "environment: ANNO_GPU_ADAPTER=<index> (bench_frame --list-adapters), "
    "ANNO_VIEW_RESAMPLE=nearest|bilinear|box|area (the continuous zoom's magnifying filter; "
    "default bilinear above scale 1, box below)\n";

/// World tiles accepted by --at: INSEL5 positions are u16 (docs/formats/insel.md);
/// the margin keeps every projected coordinate inside int range.
inline constexpr int kMaxAtTile = 65536;
/// Pixels per dimension accepted by --size; larger regions exceed the 4 GiB
/// canvas limit anyway and would overflow screen coordinates.
inline constexpr int kMaxSizePx = 65535;
/// Chunks accepted by --lookahead-chunks: each one adds a strip of chunks of
/// about 4 MB in the shown and both neighbouring zoom groups (16 strips at
/// 2560 x 1440 are several GB); a guard against a typo, not a design bound.
inline constexpr std::size_t kMaxLookaheadChunks = 16;

/// Wheel-notch factors accepted by --zoom-step: above 1 (a notch must zoom in)
/// and at most 2 (about the ratio of two zoom groups: a notch then jumps a whole
/// group); a guard against a typo, not a design bound.
inline constexpr double kMinZoomStep = 1.01, kMaxZoomStep = 2.0;
/// Milliseconds accepted by --zoom-ease (0 = no easing, the zoom jumps to its
/// target); beyond two seconds the picture lags the wheel visibly.
inline constexpr unsigned kMaxZoomEaseMs = 2000;
/// --min-zoom: the tile width in screen pixels the continuous zoom may reach at
/// the far end (group 2 is 20, derived levels are below it); 0 = the scene fits
/// the window. --max-derived-levels bounds the zoom levels reduced from group 2
/// (each halves the tile size: 8 reach 0.08 px, a 512 px chunk then spans 131,072
/// group-2 pixels, far beyond any map); raise it for larger maps.
inline constexpr double kMinMinZoom = 0.01, kMaxMinZoom = 20.0;
inline constexpr unsigned kMaxDerivedLevels = 24;
/// --zoom-factor: output pixels per group pixel; resampled_view limits it to 1/64 .. 64.
inline constexpr double kMinZoomFactor = 1.0 / 64.0, kMaxZoomFactor = 64.0;
/// --zoom-max-upscale: how far a level may be magnified before the finer one is
/// composed (1 = never; 1.45 is the nearest-in-ratio look of the first stage 3
/// build). The ratio between two levels is at most 2.1, so a larger factor would
/// skip a level altogether.
inline constexpr double kMinMaxUpscale = 1.0, kMaxMaxUpscale = 2.0;

enum class Renderer { Auto, Gpu, Cpu };

/// How the wheel zooms (docs/render/iso.md "Continuous zoom"). Stepped is the
/// original: three zoom groups, one step per wheel notch. Continuous scales the
/// picture smoothly (GPU renderer; the CPU renderer stays stepped). Continuous
/// is the viewer default (owner decision 2026-10-21, OQ-095); stepped stays the
/// faithful option.
enum class ZoomMode { Stepped, Continuous };

/// The zoom settings of the viewer.
struct ZoomOptions {
    ZoomMode mode = ZoomMode::Continuous;
    /// --zoom-mode was given: only then is a continuous request that the renderer
    /// cannot serve (CPU) reported; the default falls back silently.
    bool mode_explicit = false;
    /// Continuous: the factor of one wheel notch (the keys + and - use its third
    /// power). About 10 % per notch is a slow, smooth zoom (owner proposal
    /// 2026-10-20); the owner tunes it.
    double step = 1.1;
    /// Continuous: milliseconds to cover 95 % of the way to the target (ease-out),
    /// 0 = at once. About a tenth of a second reads as smooth, not laggy.
    unsigned ease_ms = 120;
    /// Continuous: the smallest tile width shown (0 = the whole scene fits the
    /// window), and the most zoom levels derived below group 2.
    double min_zoom = 0.0;
    unsigned max_derived_levels = 8;
    /// Continuous: the most a composed level is magnified (the picture is composed
    /// on the coarsest level whose tile width is at least the zoom, so 1 = never;
    /// above the first group's tile width magnifying cannot be avoided). Larger
    /// values compose less and magnify more: for weak GPUs and the A/B against
    /// the first stage 3 look (1.45).
    double max_upscale = 1.0;
};

struct Args {
    std::filesystem::path file;
    /// --play: run the sim and build the scene from the islands it placed.
    /// --view stays the file and the stand-in rule.
    bool play = false;
    /// Multiplayer rate: fixed real-time cadence, variable game ms per step.
    unsigned play_speed = 100;
    std::optional<std::filesystem::path> game_dir;
    std::optional<std::size_t> group;
    std::optional<std::filesystem::path> png;
    /// --png --zoom-factor: the picture is composed at --group and resampled by this
    /// many output pixels per group pixel (the continuous zoom's pass, on the CPU:
    /// the headless reference at a fractional scale); 1 = no resampling.
    double zoom_factor = 1.0;
    /// --resample: the filter of that pass (default: render::resample_filter_for).
    std::optional<render::ResampleFilter> resample;
    std::uint64_t time = 0;
    std::optional<std::pair<int, int>> at;
    std::pair<int, int> size{1600, 900};
    render::WaterStyle water = render::WaterStyle::Original;
    /// Enhanced water only: how rivers are shaded (docs/render/rivers.md). The
    /// flow shading is the default; legacy keeps the shimmer that slides
    /// along fixed axes. Key R in the viewer toggles it.
    render::RiverFlowMode river_flow = render::RiverFlowMode::Flow;
    /// Multiplier of the river flow's ripple light (1 = calibrated to the
    /// legacy shimmer; larger shows the flow more clearly). The viewer shows
    /// 4 by default because 1 does not read as flow (OQ-083).
    float river_gain = 4.0F;
    /// What the river speed scale refers to (OQ-065): the median wet cell
    /// (livelier, viewer default) or the fastest cross-section (the library default).
    render::river::RiverConfig::Reference river_speed =
        render::river::RiverConfig::Reference::MedianCell;
    /// River pieces at most this many tiles apart are joined (OQ-068; 0 = the
    /// gaps of the original data). The viewer joins gaps up to 8 tiles.
    float river_gap_patch = 8.0F;
    Renderer renderer = Renderer::Auto;
    /// FrameRenderer::max_lookahead_chunks: how far (in chunks) the margin of
    /// built chunks grows ahead of a moving view. 3 is the library default;
    /// 2 saves memory (docs/perf/README.md, T1e), 0 is a plain ring.
    std::size_t lookahead_chunks = 3;
    ZoomOptions zoom;
};

/// Parses option/value pairs; nullopt for anything invalid (print kUsage,
/// exit 2). Numbers may carry a leading '+', as Rust's parse accepts.
std::optional<Args> parse_args(const std::vector<std::string>& args);

/// A path from a UTF-8 string.
std::filesystem::path utf8_path(const std::string& s);
/// A path as a UTF-8 string.
std::string to_utf8(const std::filesystem::path& p);
/// The path in environment variable `name` (wide on Windows, so any
/// character works); nullopt if unset or empty.
std::optional<std::filesystem::path> env_path(const char* name);

} // namespace anno::app
