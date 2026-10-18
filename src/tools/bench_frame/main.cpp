// bench_frame: runs the viewer's frame pipeline (render::FrameRenderer)
// headless along scripted camera paths and prints frame-time percentiles and
// the time per stage as one Markdown table (method: docs/perf/README.md).
//
//   bench_frame <game_dir> <file.sze|.scp> [--threads n]
//       [--res 1920x1080,2560x1440,3840x2160] [--groups 0,1,2]
//       [--styles original,enhanced] [--chunks blocking,async]
//       [--renderer cpu] [--island n] [--at dx,dy] [--t0 ms] [--warmup ms] [--quick]
//
// Per resolution, zoom group, water style and chunk mode, from a cold cache
// centred on the largest island: cold (1 frame), static (120), pan slow (240,
// 6 × 3 px per frame), pan fast (120, 48 × 24 px). The animation clock
// advances 16 ms per frame; frames run back to back. Same paths and table as
// the Rust benchmark (crates/render/examples/bench_frame.rs), except that the
// scene is loaded as the viewer loads it (stand-ins, ships, soldiers). The GPU
// renderer follows with the C++ GPU path (ADR 0001).
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
    std::pair<int, int> at{0, 0};
    std::optional<std::size_t> island;
    std::uint64_t t0 = 0, warmup = 0;
    bool quick = false;
};

std::vector<std::string_view> split(std::string_view s, char sep) {
    std::vector<std::string_view> out;
    for (std::size_t start = 0;;) {
        const std::size_t k = s.find(sep, start);
        out.push_back(s.substr(start, k == std::string_view::npos ? std::string_view::npos : k - start));
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
};

void report(const Config& c, const char* mode, const char* name, const std::vector<Row>& s) {
    std::vector<double> total;
    for (const auto& r : s) {
        total.push_back(r.total);
    }
    std::sort(total.begin(), total.end());
    auto pct = [&](double p) {
        return total[static_cast<std::size_t>(std::lround((static_cast<double>(total.size()) - 1.0) * p))];
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
        within += r.total <= 16.7 ? 1 : 0;
    }
    std::printf("| %d×%d | %zu | %s | cpu | %s | %s | %zu | %.1f | %.1f | %.1f | %.1f | %.0f %% | %zu | "
                "%.1f | %.1f | %.1f | %.1f | %.1f | %.1f |\n",
                c.w, c.h, c.group, c.style == render::WaterStyle::Original ? "original" : "enhanced",
                mode, name, s.size(), pct(0.5), pct(0.95), pct(0.99), total.back(),
                100.0 * static_cast<double>(within) / n, built,
                mean([](const Row& r) { return r.chunk_ms; }),
                mean([](const Row& r) { return static_cast<double>(r.missing); }),
                mean([](const Row& r) { return r.stages[0]; }),
                mean([](const Row& r) { return r.stages[1]; }),
                mean([](const Row& r) { return r.stages[2]; }),
                mean([](const Row& r) { return r.stages[3]; }));
}

void run(const render::Scene& scene, const Config& c) {
    render::FrameRenderer fr(c.threads);
    if (c.async_chunks) {
        fr.set_workers(std::make_unique<render::ChunkWorkers>(scene, (c.threads + 1) / 2));
    }
    std::vector<std::uint32_t> out(static_cast<std::size_t>(c.w) * static_cast<std::size_t>(c.h));
    std::pair<int, int> cam{c.centre.first - c.w / 2, c.centre.second - c.h / 2};
    std::uint64_t time = c.t0 > c.warmup ? c.t0 - c.warmup : 0;
    const render::Rect view0{cam.first, cam.second, c.w, c.h};
    for (; time < c.t0; time += 16) {
        fr.render(scene, view0, c.group, time, c.style, out.data());
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
            const auto st = fr.render(scene, view, c.group, time, c.style, out.data());
            rows.push_back(Row{st.total_ms, st.chunks_built, st.chunks_ms, st.chunks_missing,
                               {st.ground_ms, st.water_objects_ms, st.sea_ms, st.objects_ms}});
            time += 16;
            cam = {cam.first + p.dx, cam.second + p.dy};
        }
        report(c, c.async_chunks ? "async" : "blocking", p.name, rows);
    }
    fr.set_workers(nullptr);
}

constexpr const char* kUsage =
    "usage: bench_frame <game_dir> <file> [--threads n] [--res WxH,..] [--groups 0,1,2] "
    "[--styles original,enhanced] [--chunks blocking,async] [--renderer cpu] [--island n] "
    "[--at dx,dy] [--t0 ms] [--warmup ms] [--quick]\n";

std::optional<Options> parse_options(const std::vector<std::string>& args) {
    Options o;
    for (std::size_t i = 2; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--quick") {
            o.quick = true;
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
            ok = v == "cpu";
            if (!ok) {
                std::fputs("only --renderer cpu: the GPU path is not ported yet (ADR 0001)\n",
                           stderr);
            }
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
    std::fprintf(stderr, "scene loaded in %.0f ms\n",
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
        for (std::size_t k = 0; k < scene.islands.size(); ++k) {
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
    std::printf("| resolution | group | water | renderer | chunks | path | frames | p50 ms | p95 ms | "
                "p99 ms | max ms | ≤16.7 ms | built | chunk ms | missing | stage 1 | stage 2 | "
                "stage 3 | stage 4 |\n");
    std::printf("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    const std::size_t scale = o->quick ? 4 : 1;
    for (const auto& [w, h] : o->resolutions) {
        for (const std::size_t group : o->groups) {
            for (const auto style : o->styles) {
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
                    run(scene, c);
                }
            }
        }
    }
    return 0;
}
