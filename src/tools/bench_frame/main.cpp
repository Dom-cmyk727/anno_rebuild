// bench_frame: runs the viewer's frame pipeline (render::FrameRenderer)
// headless along scripted camera paths and prints frame-time percentiles and
// the time per stage as one Markdown table (method: docs/perf/README.md).
//
//   bench_frame <game_dir> <file.sze|.scp> [--threads n]
//       [--res 1920x1080,2560x1440,3840x2160] [--groups 0,1,2]
//       [--styles original,enhanced] [--chunks blocking,async]
//       [--renderer cpu,gpu] [--check] [--island n] [--at dx,dy] [--t0 ms] [--warmup ms] [--quick]
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
             const gpu::GpuFrame& g, WGPUTexture tex, std::size_t missing) {
    const auto w = static_cast<std::uint32_t>(view.w), h = static_cast<std::uint32_t>(view.h);
    const std::vector<std::uint8_t> gpu_px = g.read_back(tex, w, h);
    const char* style = c.style == render::WaterStyle::Original ? "Original" : "Enhanced";
    if (gpu_px.size() != std::size_t{w} * h * 4) {
        std::fprintf(stderr, "check %dx%d group %zu %s: GPU read-back failed\n", view.w, view.h,
                     c.group, style);
        return;
    }
    render::FrameRenderer cpu(c.threads);
    std::vector<std::uint32_t> out(std::size_t{w} * h);
    cpu.render(scene, view, c.group, time, c.style, out.data());
    std::size_t differ = 0;
    int max = 0;
    for (std::size_t i = 0; i < out.size(); ++i) {
        int m = 0;
        for (int k = 0; k < 3; ++k) {
            const int cpu_v = static_cast<int>((out[i] >> (16 - 8 * k)) & 0xFF);
            m = std::max(m, std::abs(cpu_v - int{gpu_px[4 * i + static_cast<std::size_t>(k)]}));
        }
        max = std::max(max, m);
        differ += m > 2 ? 1U : 0U;
    }
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
}
#else
namespace gpu {
/// Stand-in when built without the GPU compositor (never instantiated).
struct GpuFrame {};
} // namespace gpu
#endif

void run(const render::Scene& scene, const Config& c, gpu::GpuFrame* g) {
    render::FrameRenderer fr(c.threads);
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
    auto frame = [&](render::Rect view, std::uint64_t t, bool wait_gpu) -> Row {
#ifdef ANNO_BENCH_GPU
        if (g != nullptr) {
            const auto st = g->render(fr, scene, view, c.group, t, c.style, target->second.get());
            if (!wait_gpu) {
                return Row{st.total_ms, st.chunks_built, st.chunks_ms, st.chunks_missing, {}};
            }
            const auto w0 = std::chrono::steady_clock::now();
            g->wait();
            const double wait =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - w0)
                    .count();
            return Row{st.total_ms + wait,
                       st.chunks_built,
                       st.chunks_ms,
                       st.chunks_missing,
                       {st.upload_ms, st.sprites_ms, st.encode_ms, wait}};
        }
#endif
        (void)wait_gpu;
        const auto st = fr.render(scene, view, c.group, t, c.style, out.data());
        return Row{st.total_ms,
                   st.chunks_built,
                   st.chunks_ms,
                   st.chunks_missing,
                   {st.ground_ms, st.water_objects_ms, st.sea_ms, st.objects_ms}};
    };
    for (; time < c.t0; time += 16) {
        frame(view0, time, false);
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
                compare(scene, c, view, time, *g, target->first.get(), rows.back().missing);
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
    "[--island n] [--at dx,dy] [--t0 ms] [--warmup ms] [--quick]\n";

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
    const auto started = std::chrono::steady_clock::now();
    auto loaded = anno::app::load_scene(dir, file);
    if (!loaded) {
        std::fprintf(stderr, "%s: %s\n", file.string().c_str(), loaded.error().c_str());
        return 1;
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
        auto f = gpu::GpuFrame::create(*ctx, WGPUTextureFormat_RGBA8Unorm);
        if (!f) {
            std::fprintf(stderr, "GPU: %s\n", f.error().c_str());
            return 1;
        }
        frame_gpu = std::move(f).value();
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
