// anno1503: the rebuild's entry point; for now the M1.6 island viewer.
//
//   anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group <0|1|2>]
//            [--png <out.png> [--time <ms>] [--at <x>,<y> [--size <w>x<h>]]]
//            [--water original|enhanced] [--renderer auto|gpu|cpu]
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
//   (not in the original; off by default). The title shows frames per
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
// ancestor of the file that contains Anlagen.dat. The C++ build composes
// frames on the CPU; --renderer gpu is refused until the GPU compositor is
// ported (ADR 0001), auto uses the CPU.
#include <charconv>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <anno/app/loader.hpp>
#include <anno/app/png.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>

#ifdef ANNO_WITH_VIEWER
#include "viewer.hpp"
#endif

namespace {

namespace fs = std::filesystem;
using anno::render::Rect;
using anno::render::WaterStyle;

constexpr const char* kUsage =
    "usage: anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group <0|1|2>] [--png "
    "<out.png> [--time <ms>] [--at <x>,<y> [--size <w>x<h>]]] [--water original|enhanced] "
    "[--renderer auto|gpu|cpu]\n";

enum class Renderer { Auto, Gpu, Cpu };

struct Args {
    fs::path file;
    std::optional<fs::path> game_dir;
    std::optional<std::size_t> group;
    std::optional<fs::path> png;
    std::uint64_t time = 0;
    std::optional<std::pair<int, int>> at;
    std::pair<int, int> size{1600, 900};
    WaterStyle water = WaterStyle::Original;
    Renderer renderer = Renderer::Auto;
};

template <class T> std::optional<T> number(std::string_view s) {
    T v{};
    const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc() || p != s.data() + s.size()) {
        return std::nullopt;
    }
    return v;
}

std::optional<std::pair<int, int>> pair_of(std::string_view s, char sep) {
    const auto k = s.find(sep);
    if (k == std::string_view::npos) {
        return std::nullopt;
    }
    const auto a = number<int>(s.substr(0, k)), b = number<int>(s.substr(k + 1));
    if (!a || !b) {
        return std::nullopt;
    }
    return std::pair{*a, *b};
}

std::optional<Args> parse_args(const std::vector<std::string>& args) {
    Args out;
    bool have_file = false;
    for (std::size_t i = 0; i < args.size(); i += 2) {
        if (i + 1 >= args.size()) {
            return std::nullopt;
        }
        const std::string& a = args[i];
        const std::string& v = args[i + 1];
        if (a == "--view") {
            out.file = v;
            have_file = true;
        } else if (a == "--game-dir") {
            out.game_dir = fs::path(v);
        } else if (a == "--group") {
            const auto g = number<std::size_t>(v);
            if (!g || *g >= anno::render::kGroups) {
                return std::nullopt;
            }
            out.group = g;
        } else if (a == "--png") {
            out.png = fs::path(v);
        } else if (a == "--time") {
            const auto t = number<std::uint64_t>(v);
            if (!t) {
                return std::nullopt;
            }
            out.time = *t;
        } else if (a == "--at") {
            out.at = pair_of(v, ',');
            if (!out.at) {
                return std::nullopt;
            }
        } else if (a == "--size") {
            const auto s = pair_of(v, 'x');
            if (!s || s->first <= 0 || s->second <= 0) {
                return std::nullopt;
            }
            out.size = *s;
        } else if (a == "--water") {
            if (v == "original") {
                out.water = WaterStyle::Original;
            } else if (v == "enhanced") {
                out.water = WaterStyle::Enhanced;
            } else {
                return std::nullopt;
            }
        } else if (a == "--renderer") {
            if (v == "auto") {
                out.renderer = Renderer::Auto;
            } else if (v == "gpu") {
                out.renderer = Renderer::Gpu;
            } else if (v == "cpu") {
                out.renderer = Renderer::Cpu;
            } else {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    if (!have_file) {
        return std::nullopt;
    }
    return out;
}

/// Headless --png: renders `region`, or the whole scene, at `group`.
int write_scene_png(const anno::render::Scene& scene, std::size_t group, std::optional<Rect> region,
                    const fs::path& out, std::uint64_t time, WaterStyle style) {
    const auto rect = region ? region : scene.bounds(group);
    if (!rect) {
        std::fprintf(stderr, "%s: no islands\n", scene.name.c_str());
        return 1;
    }
    const std::int64_t bytes = std::int64_t{rect->w} * rect->h * 4;
    if (bytes > (std::int64_t{1} << 32)) {
        std::fprintf(
            stderr, "%s: scene is %d x %d px at group %zu (%lld MiB); use a higher group\n",
            scene.name.c_str(), rect->w, rect->h, group, static_cast<long long>(bytes >> 20));
        return 1;
    }
    anno::render::Layers layers = anno::render::Layers::all();
    layers.time_ms = time;
    layers.water_style = style;
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
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    return st.objects.failed == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> raw(argv + 1, argv + argc);
    const auto args = parse_args(raw);
    if (!args) {
        std::fputs(kUsage, stderr);
        return 2;
    }
    const auto dir = anno::app::find_game_dir(args->game_dir, args->file);
    if (!dir) {
        std::fprintf(stderr, "%s: %s\n", args->file.string().c_str(), dir.error().c_str());
        return 1;
    }
    auto loaded = anno::app::load_scene(dir.value(), args->file);
    if (!loaded) {
        std::fprintf(stderr, "%s: %s\n", args->file.string().c_str(), loaded.error().c_str());
        return 1;
    }
    for (const auto& n : loaded.value().notes) {
        std::fprintf(stderr, "%s\n", n.c_str());
    }
    const anno::render::Scene& scene = *loaded.value().scene;
    if (args->png) {
        const std::size_t group = args->group.value_or(2);
        std::optional<Rect> region;
        if (args->at) {
            const auto [cx, cy] = anno::render::tile_top(args->at->first, args->at->second, group);
            region = Rect{cx - args->size.first / 2, cy - args->size.second / 2, args->size.first,
                          args->size.second};
        }
        return write_scene_png(scene, group, region, *args->png, args->time, args->water);
    }
    if (args->renderer == Renderer::Gpu) {
        std::fputs("the GPU renderer is not ported to the C++ build yet (ADR 0001); use "
                   "--renderer cpu or auto\n",
                   stderr);
        return 1;
    }
#ifdef ANNO_WITH_VIEWER
    const std::string err = anno::app::run_viewer(scene, args->group.value_or(2));
    if (!err.empty()) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    return 0;
#else
    std::fputs("built without the window (ANNO_BUILD_VIEWER=OFF); use --png\n", stderr);
    return 1;
#endif
}
