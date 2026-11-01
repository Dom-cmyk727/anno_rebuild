// anno1503: the rebuild's entry point; for now the M1.6 island viewer.
//
//   anno1503 --view <file.sze|file.scp> | --play <file.sze> [--game-dir <dir>] [--group <0|1|2>]
//            [--png <out.png> [--time <ms>] [--at <x>,<y> [--size <w>x<h>]]
//                              [--zoom-factor <scale>] [--resample nearest|bilinear|box|area]]
//            [--water original|enhanced] [--river-flow legacy|flow] [--river-gain <x>]
//            [--river-speed median|fastest] [--river-gap-patch <tiles>]
//            [--renderer auto|gpu|cpu] [--lookahead-chunks <n>]
//            [--zoom-mode continuous|stepped] [--zoom-step <factor>] [--zoom-ease <ms>]
//            [--min-zoom <tile width>] [--max-derived-levels <n>]
//            [--zoom-max-upscale <factor>]
//
// --view shows every island of a scenario (.sze) at its INSEL5 world
// position (INSELLNK islands as stand-ins of the same climate and size,
// OQ-028) or the single island of a template (.scp), with ground, animated
// sea, objects, shadows, and a scenario's ships and soldiers (rules:
// docs/render/iso.md).
//
// - Window (default): drag with the left mouse button, arrow keys or WASD to
//   pan; mouse wheel zooms smoothly (--zoom-mode continuous, the default on the
//   GPU renderer; Home or 0 back to the default zoom), + / - likewise; with
//   --zoom-mode stepped (or the CPU renderer) they switch zoom groups
//   2 (far) / 1 / 0 (near);
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
#include <ctime>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <anno/app/cli.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/app/png.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/app/still.hpp>
#include <anno/app/window_trace.hpp>
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
                    anno::render::RiverFlowMode river_flow, double zoom_factor,
                    std::optional<anno::render::ResampleFilter> filter) {
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
    // --zoom-factor: `rect` is the picture (in output pixels); the grid it shows is composed
    // at `group` and resampled by the CPU model of the GPU pass (the reference at a
    // fractional scale, iso.md "Continuous zoom").
    std::optional<anno::render::ResampledView> zoomed;
    if (zoom_factor != 1.0 || filter) {
        zoomed = anno::render::resampled_view(
            rect->x + rect->w / 2.0, rect->y + rect->h / 2.0, static_cast<std::uint32_t>(rect->w),
            static_cast<std::uint32_t>(rect->h), zoom_factor,
            anno::render::resample_filter_for(zoom_factor, filter));
    }
    const anno::render::Rect drawn = zoomed ? zoomed->view : *rect;
    anno::render::Canvas canvas;
    std::optional<anno::render::SceneStats> built;
    anno::render::FrameStats frames;
    if (group < anno::render::kGroups) {
        auto [picture, st] = scene.render(drawn, group, layers);
        canvas = std::move(picture);
        built = st;
    } else {
        // A derived level: the viewer's chunk pipeline (still.hpp).
        auto picture = anno::app::derived_view(scene, drawn, group, layers, frames);
        if (!picture) {
            std::fprintf(stderr, "%s: the chunks of the view were not built\n", scene.name.c_str());
            return 1;
        }
        canvas = std::move(*picture);
    }
    if (zoomed) {
        anno::render::Canvas shown;
        shown.width = static_cast<std::size_t>(rect->w);
        shown.height = static_cast<std::size_t>(rect->h);
        shown.pixels.resize(shown.width * shown.height * 4);
        anno::render::cpu_resample(canvas.pixels.data(), zoomed->view.w, zoomed->view.h,
                                   zoomed->resample, static_cast<std::uint32_t>(rect->w),
                                   static_cast<std::uint32_t>(rect->h), shown.pixels.data());
        canvas = std::move(shown);
    }
    if (built) {
        const auto& st = *built;
        std::fprintf(stderr,
                     "%s: group %zu %dx%d px, %zu islands drawn (%zu without ground), objects %zu "
                     "drawn, %zu invisible, %zu failed, %zu units drawn\n",
                     scene.name.c_str(), group, rect->w, rect->h, st.islands, st.no_ground,
                     st.objects.drawn, st.objects.invisible, st.objects.failed, st.units);
    } else {
        std::fprintf(stderr, "%s: group %zu (derived) %dx%d px, %zu chunks built\n",
                     scene.name.c_str(), group, rect->w, rect->h, frames.chunks_built);
    }
    const std::string err =
        anno::app::write_png(out, static_cast<std::uint32_t>(canvas.width),
                             static_cast<std::uint32_t>(canvas.height), canvas.pixels);
    if (!err.empty()) {
        std::fprintf(stderr, "%s: %s\n", scene.name.c_str(), err.c_str());
        return 1;
    }
    return built && built->objects.failed != 0 ? 1 : 0;
}

int run(const std::vector<std::string>& raw) {
    const auto args = anno::app::parse_args(raw);
    if (!args) {
        std::fputs(anno::app::kUsage, stderr);
        return 2;
    }
    if (!args->png && args->group && *args->group >= anno::render::kGroups) {
        std::fprintf(stderr, "--group %zu: derived zoom levels are for --png\n", *args->group);
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
    std::optional<anno::app::PlayLoad> play;
    std::vector<anno::app::SceneBuilding> buildings;
    std::vector<anno::app::ScriptedCommand> commands;
    // The loader's classification of Anlagen ids as a table, taken from the world before it moves
    // into the runner (the ids below 0x1000, those ScenePicture classifies): nothing on the window
    // side reads the world afterwards.
    std::vector<bool> building_classes;
    anno::app::SceneObjects objects;
    const anno::app::SceneObjects* objects_ptr = nullptr;
    std::span<const anno::app::LinkTemplate> links;
    if (args->commands && !args->play) {
        std::fprintf(stderr, "%s: --commands needs --play\n", name.c_str());
        return 2;
    }
    if (args->play) {
        auto opened = anno::app::load_play(dir.value(), args->file);
        if (!opened) {
            std::fprintf(stderr, "%s: %s\n", name.c_str(), opened.error().c_str());
            return 1;
        }
        play = std::move(opened).value();
        if (args->commands) {
            // Parsed against the loaded world, before the world moves into the runner.
            auto parsed = anno::app::load_commands(*args->commands, play->world,
                                                   anno::app::developer_player());
            if (!parsed) {
                std::fprintf(stderr, "%s: %s\n", name.c_str(), parsed.error().c_str());
                return 1;
            }
            commands = std::move(parsed).value();
        }
        buildings = anno::app::play_buildings(play->world);
        building_classes.assign(0x1000, false);
        for (std::size_t id = 0; id < building_classes.size(); ++id) {
            building_classes[id] =
                anno::app::anlage_is_building(play->world, static_cast<std::uint16_t>(id));
        }
        objects.is_building = [&building_classes](std::uint16_t id) {
            return id < building_classes.size() && building_classes[id];
        };
        objects.buildings = buildings;
        objects_ptr = &objects;
        links = play->links;
    }
    // A fixed map is drawn from the islands the sim placed (play->placed); a scenario
    // file goes through the viewer's loader with the sim's link templates.
    auto loaded =
        play && play->fixed_map
            ? anno::app::load_placed_scene(dir.value(), play->name, play->placed, objects_ptr)
            : anno::app::load_scene(dir.value(), args->file, links, objects_ptr);
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
        if (const std::string err =
                anno::app::set_derived_levels(scene, group, args->zoom.max_derived_levels);
            !err.empty()) {
            std::fprintf(stderr, "%s: %s\n", name.c_str(), err.c_str());
            return 2;
        }
        std::optional<Rect> region;
        if (args->at) {
            // --at is bounded (cli.hpp), so the projection stays in int range. A derived level
            // is its built ancestor's picture halved per level, as Scene::bounds reduces it.
            const auto [ax, ay] = anno::render::tile_top(args->at->first, args->at->second,
                                                         scene.levels().ancestor(group));
            const int r = 1 << scene.levels()[group].shift;
            const int cx = anno::render::div_euclid(ax, r), cy = anno::render::div_euclid(ay, r);
            region = Rect{cx - args->size.first / 2, cy - args->size.second / 2, args->size.first,
                          args->size.second};
        }
        return write_scene_png(scene, group, region, *args->png, args->time, args->water,
                               args->river_flow, args->zoom_factor, args->resample);
    }
#ifdef ANNO_WITH_VIEWER
    if (args->zoom.mode == anno::app::ZoomMode::Continuous &&
        args->renderer != anno::app::Renderer::Cpu) {
        // Zoom levels below group 2, reduced from it, down to the smallest tile
        // the window can ask for: --min-zoom, else the point where the scene fits
        // a small window (the viewer stops at the actual window's fit).
        if (const auto b = scene.bounds(2)) {
            const auto& l = anno::render::ZoomLevels::original();
            std::size_t n = 0;
            if (args->zoom.min_zoom > 0.0) {
                while (n < args->zoom.max_derived_levels &&
                       l.tile_w(2) / static_cast<double>(std::uint64_t{1} << n) >
                           args->zoom.min_zoom) {
                    ++n;
                }
            } else {
                n = anno::render::ZoomLevels::derived_to_fit(b->w, b->h, 640.0, 360.0,
                                                             args->zoom.max_derived_levels);
            }
            scene.set_levels(anno::render::ZoomLevels::with_derived(n));
        }
    }
    // The window's owning handle: the loader made the scene with render::Scene::create, so its
    // last release hands the destruction to the reclaimer. Nothing changes the scene from here on.
    std::shared_ptr<const anno::render::Scene> shown = std::move(loaded.value().scene);
    std::unique_ptr<anno::app::ScenePicture> picture;
    std::unique_ptr<anno::app::PlayRunner> runner;
    anno::app::ViewerHooks hooks;
    const anno::app::ViewerHooks* hooks_ptr = nullptr;
    std::unique_ptr<anno::app::PlayFollower> follow;
    // The developer building tool (round ee; B in the window): its setup is read from the world
    // before the runner takes it.
    std::optional<anno::app::DevTool> dev;
    // The window trace (docs/app/window-trace.md): ANNO_VIEW_TRACE=<directory that does not
    // exist>, with --play and the GPU renderer. Bounded (ANNO_VIEW_TRACE_EVENTS, _FRAMES), written
    // once after the window, the sim thread and the builders have stopped.
    std::unique_ptr<anno::app::WindowTrace> trace;
    fs::path trace_dir;
    std::ostringstream trace_manifest;
    if (const char* td = std::getenv("ANNO_VIEW_TRACE"); td != nullptr && *td != 0) {
        trace_dir = fs::path(td);
        std::error_code ec;
        if (!play) {
            std::fprintf(stderr, "ANNO_VIEW_TRACE needs --play\n");
            return 2;
        }
        if (fs::exists(trace_dir, ec) || !fs::create_directories(trace_dir, ec)) {
            std::fprintf(stderr, "ANNO_VIEW_TRACE=%s: not a new directory\n", td);
            return 2;
        }
        auto bound = [](const char* name, std::size_t fallback) {
            const char* v = std::getenv(name);
            unsigned long long n = 0;
            return v != nullptr && std::sscanf(v, "%llu", &n) == 1 && n > 0
                       ? static_cast<std::size_t>(n)
                       : fallback;
        };
        const std::size_t max_events = bound("ANNO_VIEW_TRACE_EVENTS", 2000000);
        const std::size_t max_frames = bound("ANNO_VIEW_TRACE_FRAMES", 200000);
        const std::size_t max_turns = bound("ANNO_VIEW_TRACE_TURNS", 200000);
        trace = std::make_unique<anno::app::WindowTrace>(max_events, max_frames, max_turns);
        hooks.trace = trace.get();
        trace_manifest << "window_trace 1\nmap " << name << "\n";
        for (const std::string& a : raw) {
            trace_manifest << "arg " << a << "\n";
        }
        const std::time_t now = std::time(nullptr);
        char when[64];
        std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        trace_manifest << "started " << when << "\n";
    }
    if (play) {
        // The picture that follows the sim's buildings (docs/app/game-shell.md), made from the
        // scene as loaded and the buildings at tick 0, before the world moves into the runner.
        // Two builders (PictureLimits holds the other bounds with their reasons).
        anno::app::sort_buildings(buildings);
        anno::app::PictureLimits limits;
        if (trace) {
            // The bridge's steps, from the window thread and the builders (WindowTrace is
            // thread-safe).
            limits.trace = [t = trace.get()](anno::app::PictureEvent e, std::size_t index,
                                             std::uint64_t version) {
                t->event_now(anno::app::trace_kind_of(e), static_cast<std::uint32_t>(index),
                             version);
            };
        }
        picture = std::make_unique<anno::app::ScenePicture>(shown, buildings, objects.is_building,
                                                            2, std::move(limits));
        dev.emplace(anno::app::dev_tool_setup(*play));
        hooks.dev = &*dev;
        runner = std::make_unique<anno::app::PlayRunner>(std::move(play->world), args->play_speed);
        if (trace) {
            // The sim thread's record of every turn: its outcome, and the batch's publication.
            runner->set_turn_observer([t = trace.get()](const anno::app::TurnObservation& o) {
                const double at = t->ms_at(o.published);
                if (o.batch) {
                    t->event(at, anno::app::TraceKind::BatchPublished, 0, o.turn);
                }
                t->turn({o.turn, at, o.applied, o.rejected, o.reasons, o.batch, o.deltas});
            });
        }
        trace_manifest << "commands " << commands.size() << "\n";
        runner->set_script(std::move(commands));
        runner->start();
        hooks.title = [&] { return runner->title_line(); };
        hooks.on_pause = [&](bool paused) { runner->set_paused(paused); };
        hooks_ptr = &hooks;
        follow = std::make_unique<anno::app::PlayFollower>(*picture, *runner);
    }
    const std::string err = anno::app::run_viewer(
        std::move(shown), args->group.value_or(2), args->renderer, args->river_flow, args->water,
        args->lookahead_chunks, args->zoom, hooks_ptr, follow.get());
    // Teardown: the producer stops first (the sim thread is joined), then the bridge, whose
    // destructor joins its builders and hands what it holds to its reaper; the generations are
    // destroyed by the reclaimer.
    follow.reset();
    if (runner) {
        runner->stop();
    }
    picture.reset();
    int status = err.empty() ? 0 : 1;
    if (trace) {
        // Nothing records from here on: the window returned, the sim thread is joined and the
        // builders are stopped. The run's own status is the window's.
        const anno::app::EvidenceOutcome saved =
            trace->finalize(trace_dir, trace_manifest.str(),
                            err.empty() ? "" : "window_error " + err + "\n", status);
        std::fprintf(stderr, "window trace: %s (dropped %llu, run_end status %d)\n",
                     trace_dir.string().c_str(), static_cast<unsigned long long>(trace->dropped()),
                     saved.status);
        for (const std::string& u : saved.unsaved) {
            std::fprintf(stderr, "window trace: not saved: %s\n", u.c_str());
        }
        // The process ends with the evidence's own status when the window did not fail: 5 for
        // drops or a file not saved, 7 for a trace that contradicts itself (run_end carries it).
        if (status == 0 && (!saved.trace_saved || !saved.unsaved.empty() || trace->dropped() > 0)) {
            status = anno::app::kEvidenceNotSaved;
        }
        if (status == 0 && saved.status != 0) {
            status = saved.status;
        }
    }
    if (!err.empty()) {
        std::fprintf(stderr, "%s: %s\n", name.c_str(), err.c_str());
        return 1;
    }
    return status;
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
