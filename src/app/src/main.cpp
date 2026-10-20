// anno1503: the rebuild's entry point; for now the M1.6 island viewer.
//
//   anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group <0|1|2>]
//            [--png <out.png> [--time <ms>] [--at <x>,<y> [--size <w>x<h>]]]
//            [--water original|enhanced] [--river-flow legacy|flow] [--river-gain <x>]
//            [--river-speed median|fastest] [--river-gap-patch <tiles>]
//            [--renderer auto|gpu|cpu]
//
// --view shows every island of a scenario (.sze) at its INSEL5 world
// position (INSELLNK islands as stand-ins of the same climate and size,
// OQ-028) or the single island of a template (.scp), with ground, animated
// sea, objects, shadows, and a scenario's ships and soldiers (rules:
// docs/render/iso.md).
//
// - Window (default): drag with the left mouse button, arrow keys or WASD to
//   pan; mouse wheel or + / - to switch zoom groups 2 (far) / 1 / 0 (near);
//   Esc or Q quits; Space pauses the animation; E toggles enhanced water
//   (not in the original; off by default); R toggles the river flow
//   shading of enhanced water (--river-flow legacy|flow, flow by default;
//   the GPU compositor shades it itself, its cost tier is automatic or set by
//   ANNO_RIVER_QUALITY=low|medium|high, rivers.md 16).
//   The title shows frames per
//   second; stderr gets a per-stage frame-time breakdown.
// - --png <out.png>: headless; writes the whole scene at --group (default
//   2) as an RGBA PNG, or with --at <x>,<y> a region of --size pixels
//   (default 1600x900) centred on world tile (x, y), and prints one line to
//   stderr:
//   `<file>: group <g> <w>x<h> px, <n> islands drawn (<k> without ground),
//   objects <d> drawn, <i> invisible, <f> failed, <u> units drawn`.
//   Exit code 1 if any object failed.
//
// The game directory is --game-dir, else ANNO1503_GAME_DIR, else the nearest
// ancestor of the file that contains Anlagen.dat. Arguments are UTF-8 (wide
// on Windows). --renderer auto (default) composes on the GPU when one is
// available (build option ANNO_BUILD_GPU), else on the CPU; gpu fails
// without a GPU, cpu never uses it.
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include <anno/app/cli.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/png.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>

#ifdef ANNO_WITH_VIEWER
#include "viewer.hpp"
#endif

namespace {

namespace fs = std::filesystem;
using anno::app::Args;
using anno::app::Renderer;
using anno::render::Rect;
using anno::render::WaterStyle;

/// Headless --png: renders `region`, or the whole scene, at `group`.
int write_scene_png(const anno::render::Scene& scene, std::size_t group, std::optional<Rect> region,
                    const fs::path& out, std::uint64_t time, WaterStyle style,
                    anno::render::RiverFlowMode river_flow) {
    const auto rect = region ? region : scene.bounds(group);
    if (!rect) {
        std::fprintf(stderr, "%s: no islands\n", scene.name.c_str());
        return 1;
    }
    // Unsigned: w and h are positive ints, so w · h · 4 < 2⁶⁴.
    const std::uint64_t bytes = std::uint64_t(rect->w) * std::uint64_t(rect->h) * 4;
    if (bytes > (std::uint64_t{1} << 32)) {
        std::fprintf(stderr,
                     "%s: scene is %d x %d px at group %zu (%llu MiB); use a higher group\n",
                     scene.name.c_str(), rect->w, rect->h, group,
                     static_cast<unsigned long long>(bytes >> 20));
        return 1;
    }
    anno::render::Layers layers = anno::render::Layers::all();
    layers.time_ms = time;
    layers.water_style = style;
    layers.river_flow = river_flow;
    const auto [canvas, st] = scene.render(*rect, group, layers);
    std::fprintf(stderr,
                 "%s: group %zu %dx%d px, %zu islands drawn (%zu without ground), objects %zu "
                 "drawn, %zu invisible, %zu failed, %zu units drawn\n",
                 scene.name.c_str(), group, rect->w, rect->h, st.islands, st.no_ground,
                 st.objects.drawn, st.objects.invisible, st.objects.failed, st.units);
    const std::string err =
        anno::app::write_png(out, static_cast<std::uint32_t>(canvas.width),
                             static_cast<std::uint32_t>(canvas.height), canvas.pixels);
    if (!err.empty()) {
        std::fprintf(stderr, "%s: %s\n", scene.name.c_str(), err.c_str());
        return 1;
    }
    return st.objects.failed == 0 ? 0 : 1;
}

int run(const std::vector<std::string>& raw) {
    const auto args = anno::app::parse_args(raw);
    if (!args) {
        std::fputs(anno::app::kUsage, stderr);
        return 2;
    }
    const std::string name = anno::app::to_utf8(args->file);
    const auto dir = anno::app::find_game_dir(args->game_dir, args->file,
                                              anno::app::env_path("ANNO1503_GAME_DIR"));
    if (!dir) {
        std::fprintf(stderr, "%s: %s\n", name.c_str(), dir.error().c_str());
        return 1;
    }
    auto loaded = anno::app::load_scene(dir.value(), args->file);
    if (!loaded) {
        std::fprintf(stderr, "%s: %s\n", name.c_str(), loaded.error().c_str());
        return 1;
    }
    for (const auto& n : loaded.value().notes) {
        std::fprintf(stderr, "%s\n", n.c_str());
    }
    anno::render::Scene& scene = *loaded.value().scene;
    scene.river_cfg.light_gain = args->river_gain;
    scene.river_cfg.reference = args->river_speed;
    scene.river_cfg.gap_patch_tiles = args->river_gap_patch;
    if (args->png) {
        const std::size_t group = args->group.value_or(2);
        std::optional<Rect> region;
        if (args->at) {
            // --at is bounded (cli.hpp), so the projection stays in int range.
            const auto [cx, cy] = anno::render::tile_top(args->at->first, args->at->second, group);
            region = Rect{cx - args->size.first / 2, cy - args->size.second / 2, args->size.first,
                          args->size.second};
        }
        return write_scene_png(scene, group, region, *args->png, args->time, args->water,
                               args->river_flow);
    }
#ifdef ANNO_WITH_VIEWER
    const std::string err =
        anno::app::run_viewer(scene, args->group.value_or(2), args->renderer, args->river_flow);
    if (!err.empty()) {
        std::fprintf(stderr, "%s: %s\n", name.c_str(), err.c_str());
        return 1;
    }
    return 0;
#else
    std::fputs("built without the window (ANNO_BUILD_VIEWER=OFF); use --png\n", stderr);
    return 1;
#endif
}

} // namespace

#ifdef _WIN32
// Wide arguments, converted to UTF-8, so every file name can be opened.
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> raw;
    for (int i = 1; i < argc; ++i) {
        raw.push_back(anno::app::to_utf8(fs::path(argv[i])));
    }
    return run(raw);
}
#else
int main(int argc, char** argv) {
    return run(std::vector<std::string>(argv + 1, argv + argc));
}
#endif
