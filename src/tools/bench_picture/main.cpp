// bench_picture: the cost of following the sim's buildings with the picture (ScenePicture) on a
// legal city workload, CPU stages only (round dy, Packages C/D; method, definitions and numbers:
// docs/perf/README.md, "Picture on a legal city workload").
//
//   bench_picture <game_dir> --scenario NAME --out DIR [options]
//   bench_picture --aggregate DIR/events.tsv
//
// Scenarios (each runs one case; the matrix script runs several, tools/bench/picture_matrix.sh):
//   idle       no edit; the frame loop runs for --duration-s
//   one        one build in city 0, inside the view
//   burst      --edits edits (default 50) across the cities, published in one frame
//   stream     an edit every 1 / --rate seconds of wall time, --edits of them (paced), or one edit
//              per frame (saturation); round-robin over the cities, build and demolish alternating
//   delayed    a stream with commits every --commit-every frames, and the process reclaimer
//              stalled for --stall-ms after the first commit (a delayed consumer and reclaimer)
//   offscreen  --edits edits on the cities outside the view; after the drain the view pans to the
//              last edited island at group 2, then zooms out to the first derived level
// Timing: --timing paced (a frame every --frame-ms of wall time; the loop sleeps to the deadline)
// or saturation (frames back to back, no sleep). Edits are generated through the sim before the
// measured loop (LegalWorkload: one Build or Demolish command and one turn each), so the loop only
// publishes their batches when they are due; --sim-contention 1 runs a second, independent world
// through the same production turn path on its own thread while the loop runs (the real headless
// simulation), --busy n adds arithmetic threads (a labelled proxy, not a simulation).
// Every case ends with the independent oracle: the committed generation against a freshly loaded
// scene of the final desired state (every island's PROD2 items, and the chunk layers of the final
// view; for offscreen also the panned view and its derived level).
// Raw files in --out: manifest.txt, events.tsv (picture_trace.hpp), frames.tsv, chunks.tsv,
// memory.tsv, sim.tsv (with --sim-contention), summary.txt. Exit status: 0 the case ran and
// verified; 1 verification failed; 2 bad options; 3 the drain did not converge; 4 the workload or
// the scene could not be built; 5 an evidence file could not be saved (each one is named). The
// trace ends with run_end and the verdict; --aggregate exits 0 only for such a complete trace of a
// successful run (6 for an incomplete or failed one, 2 for an unreadable or malformed one).
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
// Process and host memory counters where the host has them; elsewhere they are reported as
// unavailable, never as zero.
#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#define ANNO_BENCH_RUSAGE 1
#endif
#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

#include <anno/app/building_delta.hpp>
#include <anno/app/legal_workload.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/picture_check.hpp>
#include <anno/app/picture_trace.hpp>
#include <anno/app/play.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>

namespace fs = std::filesystem;
namespace render = anno::render;
namespace app = anno::app;
using Clock = std::chrono::steady_clock;

namespace {

[[noreturn]] void fail(int code, const std::string& message) {
    std::cerr << "bench_picture: " << message << "\n";
    std::exit(code);
}

std::size_t parse_count(std::string_view option, std::string_view text, std::size_t lo,
                        std::size_t hi) {
    std::size_t value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || ec != std::errc{} || ptr != text.data() + text.size()) {
        fail(2, std::string(option) + " needs a whole number, got '" + std::string(text) + "'");
    }
    if (value < lo || value > hi) {
        fail(2, std::string(option) + " must be in [" + std::to_string(lo) + ", " +
                    std::to_string(hi) + "], got " + std::string(text));
    }
    return value;
}

double parse_real(std::string_view option, std::string_view text, double lo, double hi) {
    const std::string s(text);
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (s.empty() || end != s.c_str() + s.size() || !(v >= lo && v <= hi)) {
        fail(2, std::string(option) + " must be a number in [" + std::to_string(lo) + ", " +
                    std::to_string(hi) + "], got '" + s + "'");
    }
    return v;
}

double ms_between(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

/// The process's memory as the host reports it; a field the host does not give is -1.
struct MemorySample {
    long long rss = -1, footprint = -1, majflt = -1, minflt = -1, pressure = -1;
    long long sys_pageins = -1, sys_pageouts = -1, sys_swapins = -1, sys_swapouts = -1;
};

MemorySample sample_memory() {
    MemorySample m;
#if defined(ANNO_BENCH_RUSAGE)
    rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        m.majflt = ru.ru_majflt;
        m.minflt = ru.ru_minflt;
    }
#endif
#if defined(__linux__)
    // Resident pages (the second field of statm) times the page size; no footprint, pressure or
    // host paging counters are read on Linux here.
    if (std::FILE* f = std::fopen("/proc/self/statm", "r")) {
        long long size = 0, resident = 0;
        if (std::fscanf(f, "%lld %lld", &size, &resident) == 2) {
            m.rss = resident * static_cast<long long>(sysconf(_SC_PAGESIZE));
        }
        std::fclose(f);
    }
#endif
#if defined(__APPLE__)
    task_vm_info_data_t vm{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&vm), &count) ==
        KERN_SUCCESS) {
        m.rss = static_cast<long long>(vm.resident_size);
        m.footprint = static_cast<long long>(vm.phys_footprint);
    }
    int level = 0;
    std::size_t size = sizeof level;
    if (sysctlbyname("kern.memorystatus_vm_pressure_level", &level, &size, nullptr, 0) == 0) {
        m.pressure = level;
    }
    vm_statistics64_data_t host{};
    mach_msg_type_number_t hcount = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&host),
                          &hcount) == KERN_SUCCESS) {
        m.sys_pageins = static_cast<long long>(host.pageins);
        m.sys_pageouts = static_cast<long long>(host.pageouts);
        m.sys_swapins = static_cast<long long>(host.swapins);
        m.sys_swapouts = static_cast<long long>(host.swapouts);
    }
#endif
    return m;
}

/// A group-`group` view of w x h scene pixels centred on island-local tile (x, y) of the island
/// numbered `number`.
render::Rect view_at(const render::Scene& scene, std::uint32_t number, std::int32_t x,
                     std::int32_t y, int w, int h, std::size_t group) {
    for (std::size_t i = 0; i < scene.islands.size(); ++i) {
        if (scene.islands[i].number != number) {
            continue;
        }
        for (const render::Placement& p : scene.layouts[group]) {
            if (p.index == i) {
                const auto [tw, th] = render::kTile[group];
                const int cx2 = 2 * x + 1, cy2 = 2 * y + 1;
                const int sx = (cx2 - cy2) * tw / 4 + p.offset.first;
                const int sy = (cx2 + cy2) * th / 4 + p.offset.second;
                return render::Rect{sx - w / 2, sy - h / 2, w, h};
            }
        }
    }
    fail(4, "no island " + std::to_string(number) + " in the scene's layout");
}

/// A view at derived level `level` covering the same scene area as `view` at group 2 (each
/// derived level halves the scene pixels).
render::Rect at_level(render::Rect view, std::size_t level) {
    const int div = 1 << static_cast<int>(level - 2);
    return render::Rect{view.x / div, view.y / div, std::max(1, view.w / div),
                        std::max(1, view.h / div)};
}

std::vector<render::ChunkKey> keys_at(const render::Scene& scene, render::Rect view,
                                      std::size_t level) {
    // chunk_keys_touching takes a box at the built group 2 and lists every level below it.
    const int div = 1 << static_cast<int>(level - 2);
    const render::Rect box{view.x * div, view.y * div, view.w * div, view.h * div};
    std::vector<render::ChunkKey> out;
    for (const render::ChunkKey& k : render::chunk_keys_touching(scene, box, 2)) {
        if (static_cast<std::size_t>(std::get<0>(k)) == level) {
            out.push_back(k);
        }
    }
    return out;
}

bool keys_ready(const render::FrameRenderer& frames, const std::vector<render::ChunkKey>& keys) {
    return std::all_of(keys.begin(), keys.end(), [&frames](const render::ChunkKey& k) {
        return frames.chunk(k) != nullptr && frames.stale_generation(k) == 0;
    });
}

/// A counter as written to the raw files and the summary: "unavailable" when the host does not
/// report it (-1), never zero.
std::string counter(long long v) {
    return v < 0 ? std::string("unavailable") : std::to_string(v);
}
/// The difference of two samples of one counter, unavailable when either is.
std::string delta(long long before, long long after) {
    return before < 0 || after < 0 ? std::string("unavailable") : std::to_string(after - before);
}

/// The events of the run: the bridge's seam (workers and window) and the bench's own.
class Recorder {
  public:
    explicit Recorder(Clock::time_point t0) : t0_(t0) {}
    void add(app::TraceKind k, std::uint32_t island, std::uint64_t version) {
        const std::lock_guard lock(mutex_);
        events_.push_back({ms_between(t0_, Clock::now()), k, island, version});
    }
    /// Starts the clock again (the measured loop's start): the edit schedule and every event time
    /// count from here, not from the setup before it.
    void restart() {
        const std::lock_guard lock(mutex_);
        t0_ = Clock::now();
    }
    [[nodiscard]] std::vector<app::TraceEvent> events() const {
        const std::lock_guard lock(mutex_);
        return events_;
    }
    [[nodiscard]] double now_ms() const {
        const std::lock_guard lock(mutex_);
        return ms_between(t0_, Clock::now());
    }

  private:
    Clock::time_point t0_;
    mutable std::mutex mutex_;
    std::vector<app::TraceEvent> events_;
};

app::TraceKind kind_of(app::PictureEvent e) {
    switch (e) {
    case app::PictureEvent::Desired:
        return app::TraceKind::Desired;
    case app::PictureEvent::Admitted:
        return app::TraceKind::Admitted;
    case app::PictureEvent::Deferred:
        return app::TraceKind::Deferred;
    case app::PictureEvent::Started:
        return app::TraceKind::Started;
    case app::PictureEvent::Built:
        return app::TraceKind::Built;
    case app::PictureEvent::Failed:
        return app::TraceKind::Failed;
    case app::PictureEvent::Superseded:
        return app::TraceKind::Superseded;
    case app::PictureEvent::Dropped:
        return app::TraceKind::Dropped;
    case app::PictureEvent::Requeued:
        return app::TraceKind::Requeued;
    case app::PictureEvent::Committed:
        return app::TraceKind::Committed;
    }
    return app::TraceKind::Desired;
}

struct PlannedEdit {
    double at_ms = 0;
    app::LegalEdit edit;
};

std::shared_ptr<const render::Scene> scene_of(const fs::path& dir, const app::PlayLoad& play,
                                              const std::vector<app::SceneBuilding>& buildings) {
    app::SceneObjects objects;
    objects.is_building = [&play](std::uint16_t id) {
        return app::anlage_is_building(play.world, id);
    };
    objects.buildings = buildings;
    auto loaded = app::load_placed_scene(dir, play.name, play.placed, &objects);
    if (!loaded.ok()) {
        fail(4, "scene: " + loaded.error());
    }
    app::LoadedScene ls = std::move(loaded).value();
    // Rivers off: the building check is the legacy look (river flow is asynchronous and tested
    // apart); two derived levels below group 2 for the zoom-out case.
    ls.scene->river_cfg.enabled = false;
    ls.scene->set_levels(render::ZoomLevels::with_derived(2));
    return std::shared_ptr<const render::Scene>(std::move(ls.scene));
}

/// The oracle on one view at one level: the frame renderer's chunks against a fresh synchronous
/// build of `fresh`. Returns the diff (all keys compared, objects in view).
app::ChunkDiff check_view(const render::FrameRenderer& frames, const render::Scene& fresh,
                          render::Rect view, std::size_t level) {
    render::FrameRenderer reference(1);
    const int mul = 1 << static_cast<int>(level - 2);
    const render::Rect base_view{view.x * mul, view.y * mul, view.w * mul, view.h * mul};
    for (int round = 0; round < 4; ++round) {
        reference.update_chunks(fresh, base_view, 2);
        for (std::size_t l = 3; l <= level; ++l) {
            reference.update_chunks(fresh, at_level(base_view, l), l);
        }
    }
    const auto keys = keys_at(fresh, view, level);
    return app::compare_chunks(
        keys, [&frames](const render::ChunkKey& k) { return frames.chunk(k); },
        [&reference](const render::ChunkKey& k) { return reference.chunk(k); });
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--aggregate") {
        // Exit 0 only for a complete trace of a successful run; an incomplete or failed run is
        // still summarized (for inspection) but exits 6; an unreadable or malformed trace exits 2.
        const app::TextFile file = app::read_text(argv[2]);
        if (!file.ok) {
            fail(2, file.error);
        }
        const auto events = app::parse_tsv(file.text);
        if (!events.ok()) {
            fail(2, std::string(argv[2]) + ": " + events.error());
        }
        const app::TraceSummary summary = app::summarize(events.value());
        std::cout << app::format_summary(summary);
        return summary.succeeded() ? 0 : 6;
    }
    if (argc < 2) {
        fail(2, "usage: bench_picture <game_dir> --scenario NAME --out DIR [options] (see the "
                "header of src/tools/bench_picture/main.cpp)");
    }
    const fs::path dir = argv[1];
    std::string scenario, timing = "paced";
    fs::path out, map = "data/maps/lan5.txt";
    app::LegalWorkloadOptions wl;
    std::size_t edits = 0, threads = 2, chunk_threads = 2, max_outstanding = 8, commit_every = 1,
                stall_ms = 0, busy = 0, sim_contention = 0, sim_unpaced = 0, drain_s = 120,
                publish_progress = 0, view_w = 1920, view_h = 1080, frames_max = 100000;
    double frame_ms = 1000.0 / 60.0, duration_s = 5, rate = 10;
    bool edits_given = false;
    for (int i = 2; i < argc; ++i) {
        const std::string key = argv[i];
        if (i + 1 >= argc) {
            fail(2, "option " + key + " needs a value");
        }
        const std::string_view val = argv[++i];
        if (key == "--scenario") {
            const std::set<std::string_view> known{"idle",   "one",     "burst",
                                                   "stream", "delayed", "offscreen"};
            if (known.count(val) == 0) {
                fail(2, "--scenario: unknown " + std::string(val));
            }
            scenario = std::string(val);
        } else if (key == "--timing") {
            if (val != "paced" && val != "saturation") {
                fail(2, "--timing must be paced or saturation");
            }
            timing = std::string(val);
        } else if (key == "--out") {
            out = std::string(val);
        } else if (key == "--map") {
            map = std::string(val);
        } else if (key == "--cities") {
            wl.cities = parse_count(key, val, 1, 64);
        } else if (key == "--per-city") {
            wl.buildings_per_city = parse_count(key, val, 1, 100000);
        } else if (key == "--unrelated-extra") {
            wl.unrelated_extra = parse_count(key, val, 0, 100000);
        } else if (key == "--attempts") {
            wl.attempts_per_city = parse_count(key, val, 1, 100000000);
        } else if (key == "--goods-tons") {
            wl.goods_tons = static_cast<std::uint32_t>(parse_count(key, val, 1, 1000000));
        } else if (key == "--seed") {
            wl.seed = parse_count(key, val, 1, 0xFFFFFFFFULL);
        } else if (key == "--edits") {
            edits = parse_count(key, val, 0, 1000000);
            edits_given = true;
        } else if (key == "--rate") {
            rate = parse_real(key, val, 0.01, 100000);
        } else if (key == "--frame-ms") {
            frame_ms = parse_real(key, val, 0.1, 1000);
        } else if (key == "--duration-s") {
            duration_s = parse_real(key, val, 0.1, 3600);
        } else if (key == "--frames-max") {
            frames_max = parse_count(key, val, 1, 100000000);
        } else if (key == "--threads") {
            threads = parse_count(key, val, 1, 64);
        } else if (key == "--chunk-threads") {
            chunk_threads = parse_count(key, val, 1, 64);
        } else if (key == "--max-outstanding") {
            max_outstanding = parse_count(key, val, 1, 1000);
        } else if (key == "--commit-every") {
            commit_every = parse_count(key, val, 1, 100000);
        } else if (key == "--stall-ms") {
            stall_ms = parse_count(key, val, 0, 600000);
        } else if (key == "--busy") {
            busy = parse_count(key, val, 0, 256);
        } else if (key == "--sim-contention") {
            sim_contention = parse_count(key, val, 0, 1);
        } else if (key == "--publish-progress") {
            publish_progress = parse_count(key, val, 0, 1);
        } else if (key == "--sim-unpaced") {
            sim_unpaced = parse_count(key, val, 0, 1);
        } else if (key == "--drain-s") {
            drain_s = parse_count(key, val, 1, 3600);
        } else if (key == "--view") {
            const auto comma = val.find(',');
            if (comma == std::string_view::npos) {
                fail(2, "--view needs w,h");
            }
            view_w = parse_count("--view w", val.substr(0, comma), 64, 16384);
            view_h = parse_count("--view h", val.substr(comma + 1), 64, 16384);
        } else {
            fail(2, "unknown option " + key);
        }
    }
    if (scenario.empty() || out.empty()) {
        fail(2, "--scenario and --out are required");
    }
    if (!edits_given) {
        edits = scenario == "idle" ? 0 : (scenario == "one" ? 1 : 50);
    }
    if (scenario == "idle") {
        edits = 0;
    } else if (scenario == "one") {
        edits = 1;
    }
    std::error_code ec;
    if (fs::exists(out / "events.tsv", ec) || fs::exists(out / "summary.txt", ec)) {
        // An earlier run's evidence is never overwritten silently.
        fail(2, "--out " + out.string() + " already holds a run's evidence; use another directory");
    }
    fs::create_directories(out, ec);
    if (ec) {
        fail(2, "--out " + out.string() + ": " + ec.message());
    }

    // The workload, then the edits through the sim, before anything is timed.
    const auto build_start = Clock::now();
    auto built = app::LegalWorkload::build(dir, map, wl);
    if (!built.ok()) {
        fail(4, built.error());
    }
    const auto workload = std::move(built).value();
    const double build_ms = ms_between(build_start, Clock::now());
    const std::vector<app::SceneBuilding> initial = workload->buildings();
    const std::string manifest_initial = workload->manifest();
    const std::size_t cities = workload->cities().size();
    // The view: city 0's island, centred on the first edit there (or on its Kontor).
    std::uint32_t view_island = workload->cities()[0].island_slot;
    std::int32_t view_x = 0, view_y = 0;
    for (const app::SceneBuilding& b : initial) {
        if (b.island == view_island) {
            view_x = b.x;
            view_y = b.y;
            break;
        }
    }
    std::vector<PlannedEdit> plan;
    std::size_t rejected_edits = 0;
    for (std::size_t i = 0; i < edits; ++i) {
        std::size_t city = 0;
        bool build = true;
        if (scenario == "offscreen") {
            city = cities > 1 ? 1 + i % (cities - 1) : 0;
            build = (i / std::max<std::size_t>(cities - 1, 1)) % 2 == 0;
        } else if (scenario != "one") {
            city = i % cities;
            build = (i / cities) % 2 == 0;
        }
        app::LegalEdit e = build ? workload->build_one(city) : workload->demolish_one(city);
        if (!e.accepted || !e.batch) {
            ++rejected_edits;
            continue;
        }
        if (!workload->invariants().empty()) {
            fail(4, "invariants after an edit: " + workload->invariants());
        }
        if (plan.empty() && e.island == view_island) {
            view_x = e.x;
            view_y = e.y;
        }
        PlannedEdit p;
        p.edit = std::move(e);
        if (scenario == "burst" || scenario == "one") {
            p.at_ms = 0;
        } else if (timing == "paced") {
            p.at_ms = 1000.0 * static_cast<double>(plan.size()) / rate;
        } else {
            p.at_ms = 0; // saturation: one per frame, see the loop
        }
        plan.push_back(std::move(p));
    }
    const std::vector<app::SceneBuilding> desired_final = workload->buildings();

    // The scene of the initial state (generation 0) and the picture.
    const auto base = scene_of(dir, workload->load(), initial);
    const auto view = view_at(*base, view_island, view_x, view_y, static_cast<int>(view_w),
                              static_cast<int>(view_h), 2);
    const auto loop_start = Clock::now();
    Recorder rec(loop_start);
    app::ScenePicture::Limits limits;
    limits.max_outstanding = max_outstanding;
    limits.publish_progress = publish_progress != 0;
    limits.trace = [&rec](app::PictureEvent e, std::size_t index, std::uint64_t version) {
        rec.add(kind_of(e), static_cast<std::uint32_t>(index), version);
    };
    std::atomic<bool> reclaim_open{true};
    auto& reclaimer = render::SceneReclaimer::shared();
    reclaimer.set_test_observer(
        [&rec](const render::Scene& s) { rec.add(app::TraceKind::Reclaimed, 0, s.generation()); });
    reclaimer.set_test_gate([&reclaim_open] {
        while (!reclaim_open.load()) {
            std::this_thread::yield();
        }
    });
    const app::PlayLoad& play = workload->load();
    auto is_building = [&play](std::uint16_t id) {
        return app::anlage_is_building(play.world, id);
    };
    auto picture = std::make_unique<app::ScenePicture>(base, initial, is_building, threads, limits);
    render::FrameRenderer frames(1);
    auto workers_owned = std::make_unique<render::ChunkWorkers>(*base, chunk_threads);
    render::ChunkWorkers* chunk_workers = workers_owned.get();
    frames.set_workers(std::move(workers_owned));
    // Warm the view (not measured): the loop measures what the edits change.
    for (int round = 0; round < 200 && !keys_ready(frames, keys_at(*base, view, 2)); ++round) {
        frames.update_chunks(base, view, 2);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Load beside the loop: a second world through the production turn path, and/or a proxy.
    std::atomic<bool> stop_side{false};
    std::vector<std::thread> side;
    std::mutex sim_mutex;
    std::ostringstream sim_rows;
    std::size_t sim_turns = 0, sim_accepted = 0, sim_refused = 0, sim_no_candidate = 0;
    // The side load starts with the measured loop, not during the setup before it.
    std::atomic<bool> side_go{false};
    std::uint64_t sim_hash = 0;
    std::string sim_manifest, sim_invariants;
    std::unique_ptr<app::LegalWorkload> sim_world;
    if (sim_contention != 0) {
        auto second = app::LegalWorkload::build(dir, map, wl);
        if (!second.ok()) {
            fail(4, "the contention world: " + second.error());
        }
        sim_world = std::move(second).value();
        sim_world->set_check_invariants(false);
        sim_rows << "t_ms\tkind\taccepted\treason\tturn_ms\tdiff_ms\tcall_ms\n";
        side.emplace_back([&] {
            while (!side_go.load() && !stop_side.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            std::size_t i = 0;
            auto next = Clock::now();
            while (!stop_side.load()) {
                const std::size_t c = i % sim_world->cities().size();
                const auto t0 = Clock::now();
                const app::LegalEdit e = (i / sim_world->cities().size()) % 2 == 0
                                             ? sim_world->build_one(c, 400)
                                             : sim_world->demolish_one(c);
                const double call_ms = ms_between(t0, Clock::now());
                {
                    const std::lock_guard lock(sim_mutex);
                    sim_rows << rec.now_ms() - call_ms << "\t" << e.what << "\t"
                             << (e.accepted ? 1 : 0) << "\t"
                             << (e.reason.empty() ? std::string("-") : e.reason) << "\t"
                             << e.turn_ms << "\t" << e.diff_ms << "\t" << call_ms << "\n";
                    ++sim_turns;
                    if (e.accepted) {
                        ++sim_accepted;
                    } else if (e.reason == "no candidate" || e.reason == "no building") {
                        // The fixture found nothing to place or remove: no command was issued.
                        ++sim_no_candidate;
                    } else {
                        ++sim_refused;
                    }
                }
                ++i;
                if (sim_unpaced == 0) {
                    // The play runner's cadence: one turn of 100 ms of sim time per 100 ms.
                    next += std::chrono::milliseconds(100);
                    std::this_thread::sleep_until(next);
                }
            }
        });
    }
    for (std::size_t t = 0; t < busy; ++t) {
        side.emplace_back([&stop_side, &side_go] {
            while (!side_go.load() && !stop_side.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            volatile std::uint64_t x = 1;
            while (!stop_side.load(std::memory_order_relaxed)) {
                x = x * 6364136223846793005ULL + 1442695040888963407ULL;
            }
        });
    }

    // The frame loop.
    std::ostringstream frame_rows, chunk_rows, memory_rows;
    frame_rows << "frame\tt_ms\tphase\tpublished\tapply_ms\tapply_list_ms\treindex_ms\tdispatch_ms"
                  "\tcommit_ms\tcommitted\tupdate_ms\tchunks_built\tpending\toutstanding"
                  "\toutstanding_bytes\tchunk_build_ms\n";
    chunk_rows << "t_ms\tisland\tgeneration\tlevel\tx\ty\tstate\n";
    memory_rows << "t_ms\trss\tfootprint\tmajflt\tminflt\tpressure\tsys_pageins\tsys_pageouts"
                   "\tsys_swapins\tsys_swapouts\toutstanding_bytes\tincremental\tbaseline\tbacking"
                   "\tgeneration_meta\treferenced_by_current\tretired_payload\treclaim_waiting"
                   "\treclaim_bytes_waiting\treclaim_bytes_in_flight\n";
    // Visible readiness per committed island and generation: every key of the island's change
    // within the view, fresh at the same poll (ReadinessTracker; round dz, 2b/2d).
    app::ReadinessTracker readiness;
    std::vector<app::ReadinessTracker::KeyEvent> key_events;
    bool converged_seen = false;
    std::size_t peak_incremental = 0, peak_retired = 0, peak_reclaim_bytes = 0;
    std::size_t peak_outstanding_bytes = 0;
    long long peak_rss = -1, peak_footprint = -1;
    const MemorySample mem_start = sample_memory();
    auto sample = [&](double t) {
        const MemorySample m = sample_memory();
        const auto pc = render::payload_counts();
        const auto rs = reclaimer.stats();
        const auto st = picture->stats();
        const std::size_t referenced = picture->scene()->referenced_payload_bytes();
        const std::size_t live =
            pc.of(render::PayloadKind::Incremental) + pc.of(render::PayloadKind::Baseline);
        const std::size_t retired = live > referenced ? live - referenced : 0;
        peak_incremental = std::max(peak_incremental, pc.of(render::PayloadKind::Incremental));
        peak_retired = std::max(peak_retired, retired);
        peak_reclaim_bytes = std::max(peak_reclaim_bytes, rs.bytes_waiting + rs.bytes_in_flight);
        peak_outstanding_bytes = std::max(peak_outstanding_bytes, st.outstanding_bytes_peak);
        peak_rss = std::max(peak_rss, m.rss);
        peak_footprint = std::max(peak_footprint, m.footprint);
        memory_rows << t << "\t" << counter(m.rss) << "\t" << counter(m.footprint) << "\t"
                    << counter(m.majflt) << "\t" << counter(m.minflt) << "\t" << counter(m.pressure)
                    << "\t" << counter(m.sys_pageins) << "\t" << counter(m.sys_pageouts) << "\t"
                    << counter(m.sys_swapins) << "\t" << counter(m.sys_swapouts) << "\t"
                    << st.outstanding_bytes << "\t" << pc.of(render::PayloadKind::Incremental)
                    << "\t" << pc.of(render::PayloadKind::Baseline) << "\t"
                    << pc.of(render::PayloadKind::Backing) << "\t"
                    << pc.of(render::PayloadKind::Generation) << "\t" << referenced << "\t"
                    << retired << "\t" << rs.waiting << "\t" << rs.bytes_waiting << "\t"
                    << rs.bytes_in_flight << "\n";
    };
    std::size_t next_edit = 0, frame = 0;
    std::uint64_t current_generation = base->generation();
    bool stalled = false, stall_released = stall_ms == 0;
    Clock::time_point stall_until{};
    auto run_frame = [&](const char* phase, render::Rect v, std::size_t level) {
        const auto t_start = Clock::now();
        const double t = rec.now_ms();
        // Publish the edits that are due: paced by wall time, one per frame in saturation, all at
        // once in a burst.
        std::size_t published = 0;
        double apply_ms = 0, list_ms = 0, reindex_ms = 0, dispatch_ms = 0;
        while (next_edit < plan.size() &&
               (timing == "saturation" && scenario != "burst" ? published == 0
                                                              : plan[next_edit].at_ms <= t)) {
            const auto a0 = Clock::now();
            picture->apply({plan[next_edit].edit.batch});
            apply_ms += ms_between(a0, Clock::now());
            const auto st = picture->stats();
            list_ms += st.last_apply_list_ms;
            reindex_ms += st.last_reindex_ms;
            dispatch_ms += st.last_dispatch_ms;
            ++next_edit;
            ++published;
        }
        const auto c0 = Clock::now();
        std::size_t committed = 0;
        if (frame % commit_every == 0) {
            committed = picture->commit(frames);
        }
        const double commit_ms = ms_between(c0, Clock::now());
        if (committed > 0) {
            const std::uint64_t g = picture->scene()->generation();
            rec.add(app::TraceKind::Generation, 0, g);
            rec.add(app::TraceKind::Retired, 0, current_generation);
            current_generation = g;
            const auto visible = keys_at(*picture->scene(), v, level);
            const std::set<render::ChunkKey> in_view(visible.begin(), visible.end());
            for (const app::ScenePicture::CommittedIsland& c : picture->last_commit()) {
                app::ReadinessTracker::Set set{static_cast<std::uint32_t>(c.index), g, {}};
                for (const render::ChunkKey& k : c.keys) {
                    if (in_view.count(k) != 0) {
                        set.keys.push_back(k);
                    }
                }
                if (set.keys.empty()) {
                    rec.add(app::TraceKind::NoVisibleChange, set.island, g);
                } else {
                    readiness.add(std::move(set));
                }
            }
            if (stall_ms > 0 && !stalled && !stall_released) {
                // The delayed reclaimer: stall it from the first commit for stall_ms.
                stalled = true;
                reclaim_open.store(false);
                stall_until = Clock::now() + std::chrono::milliseconds(stall_ms);
            }
        }
        if (stalled && Clock::now() >= stall_until) {
            reclaim_open.store(true);
            stalled = false;
            stall_released = true;
        }
        const auto u0 = Clock::now();
        const auto [built_chunks, unused_ms, unused_n] =
            frames.update_chunks(picture->scene(), v, level);
        static_cast<void>(unused_ms);
        static_cast<void>(unused_n);
        const double update_ms = ms_between(u0, Clock::now());
        const double t_end = rec.now_ms();
        const std::size_t logged = key_events.size();
        for (const app::ReadinessTracker::Set& ready : readiness.poll(
                 [&frames](const render::ChunkKey& k) {
                     return frames.chunk(k) != nullptr && frames.stale_generation(k) == 0;
                 },
                 t_end, key_events)) {
            rec.add(app::TraceKind::ChunksReady, ready.island, ready.generation);
        }
        for (std::size_t e = logged; e < key_events.size(); ++e) {
            const auto& k = key_events[e];
            chunk_rows << k.t_ms << "\t" << k.island << "\t" << k.generation << "\t"
                       << std::get<0>(k.key) << "\t" << std::get<1>(k.key) << "\t"
                       << std::get<2>(k.key) << "\t" << (k.fresh ? "fresh" : "stale") << "\n";
        }
        if (!converged_seen && next_edit >= plan.size() && picture->pending() == 0 &&
            readiness.waiting() == 0) {
            // Eventual convergence: every desired version committed, no visible readiness left.
            rec.add(app::TraceKind::Converged, 0, 0);
            converged_seen = true;
        }
        const auto st = picture->stats();
        frame_rows << frame << "\t" << t << "\t" << phase << "\t" << published << "\t" << apply_ms
                   << "\t" << list_ms << "\t" << reindex_ms << "\t" << dispatch_ms << "\t"
                   << commit_ms << "\t" << committed << "\t" << update_ms << "\t" << built_chunks
                   << "\t" << picture->pending() << "\t" << st.outstanding << "\t"
                   << st.outstanding_bytes << "\t" << chunk_workers->build_ms() << "\n";
        if (frame % 30 == 0) {
            sample(t_end);
        }
        ++frame;
        if (timing == "paced") {
            std::this_thread::sleep_until(t_start +
                                          std::chrono::duration_cast<Clock::duration>(
                                              std::chrono::duration<double, std::milli>(frame_ms)));
        }
    };
    // The measured loop: until every edit is published and the duration has passed (paced), or
    // every edit is published (saturation), within frames_max. The clock, the edit schedule and the
    // side load start here, after the setup (the view's warm-up, the contention world).
    const double duration_ms = 1000.0 * duration_s;
    rec.restart();
    side_go.store(true);
    while (frame < frames_max) {
        const bool all_published = next_edit >= plan.size();
        if (all_published && (timing == "saturation" || rec.now_ms() >= duration_ms ||
                              scenario == "burst" || scenario == "one")) {
            break;
        }
        run_frame("loop", view, 2);
    }
    // The drain: frames go on until the picture converges and the visible chunks are ready.
    rec.add(app::TraceKind::DrainStart, 0, 0);
    const auto drain_end = Clock::now() + std::chrono::seconds(drain_s);
    while (picture->pending() > 0 || readiness.waiting() > 0 || stalled) {
        if (Clock::now() > drain_end) {
            reclaim_open.store(true);
            break;
        }
        run_frame("drain", view, 2);
    }
    rec.add(app::TraceKind::DrainEnd, 0, 0);
    const bool converged = picture->pending() == 0 && readiness.waiting() == 0;
    reclaim_open.store(true);
    // The offscreen case: pan to the last edited island at group 2, then zoom out one level.
    double pan_ready_ms = -1, zoom_ready_ms = -1;
    render::Rect pan_view = view, zoom_view{};
    if (scenario == "offscreen" && !plan.empty()) {
        const app::LegalEdit& last = plan.back().edit;
        pan_view = view_at(*picture->scene(), last.island, last.x, last.y, static_cast<int>(view_w),
                           static_cast<int>(view_h), 2);
        const auto pan_keys = keys_at(*picture->scene(), pan_view, 2);
        const double p0 = rec.now_ms();
        while (rec.now_ms() - p0 < 1000.0 * static_cast<double>(drain_s)) {
            run_frame("pan", pan_view, 2);
            if (keys_ready(frames, pan_keys)) {
                pan_ready_ms = rec.now_ms() - p0;
                break;
            }
        }
        zoom_view = at_level(pan_view, 3);
        const auto zoom_keys = keys_at(*picture->scene(), zoom_view, 3);
        const double z0 = rec.now_ms();
        while (rec.now_ms() - z0 < 1000.0 * static_cast<double>(drain_s)) {
            run_frame("zoom", zoom_view, 3);
            if (keys_ready(frames, zoom_keys)) {
                zoom_ready_ms = rec.now_ms() - z0;
                break;
            }
        }
    }
    sample(rec.now_ms());
    const MemorySample mem_end = sample_memory();
    stop_side.store(true);
    for (std::thread& t : side) {
        t.join();
    }
    if (sim_world) {
        sim_hash = sim_world->state_hash();
        sim_manifest = sim_world->manifest();
        sim_invariants = sim_world->world().check_invariants();
    }

    // The oracle.
    int verdict = 0;
    std::ostringstream verify;
    if (!converged) {
        verdict = 3;
        verify << "drain did not converge within " << drain_s << " s: pending "
               << picture->pending() << ", visible island changes waiting " << readiness.waiting()
               << "\n";
    }
    if (picture->buildings() != desired_final) {
        verdict = 1;
        verify << "VERIFY FAILED: the picture's desired list is not the sim's final list\n";
    }
    const auto fresh = scene_of(dir, play, desired_final);
    const auto committed_scene = picture->scene();
    std::size_t differing = 0;
    for (std::size_t i = 0; i < fresh->islands.size() && i < committed_scene->islands.size(); ++i) {
        differing += app::building_items(committed_scene->islands[i]) ==
                             app::building_items(fresh->islands[i])
                         ? 0U
                         : 1U;
    }
    verify << "verify island items: islands_differing=" << differing << " of "
           << fresh->islands.size() << "\n";
    if (differing != 0) {
        verdict = 1;
    }
    auto settle = [&](render::Rect v, std::size_t level) {
        for (int round = 0; round < 400; ++round) {
            frames.update_chunks(committed_scene, v, level);
            if (keys_ready(frames, keys_at(*committed_scene, v, level))) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };
    std::vector<std::pair<std::string, std::pair<render::Rect, std::size_t>>> views{
        {"final view", {view, 2}}};
    if (scenario == "offscreen" && !plan.empty()) {
        views.push_back({"panned view", {pan_view, 2}});
        views.push_back({"panned view, derived level 3", {zoom_view, 3}});
    }
    for (const auto& [name, vl] : views) {
        settle(vl.first, vl.second);
        const app::ChunkDiff d = check_view(frames, *fresh, vl.first, vl.second);
        verify << "verify " << name << ": compared=" << d.compared
               << " with_objects=" << d.with_objects << " ground=" << d.ground
               << " objects=" << d.objects << " terrain=" << d.terrain << " field=" << d.field
               << " missing=" << d.missing << "\n";
        if (!d.equal() || d.compared == 0) {
            verdict = verdict == 0 ? 1 : verdict;
        }
    }
    if (sim_world && !sim_invariants.empty()) {
        verdict = verdict == 0 ? 1 : verdict;
        verify << "the contention world's invariants do not hold: " << sim_invariants << "\n";
    }
    verify << (verdict == 0 ? "VERIFY OK\n" : "VERIFY FAILED\n");
    const auto st = picture->stats();
    picture.reset();
    reclaimer.set_test_gate({});
    // The observer refers to the recorder: it is cleared once the picture's generations are gone.
    for (int round = 0; round < 2000; ++round) {
        const auto rs = reclaimer.stats();
        if (rs.waiting == 0 && rs.in_flight == 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    reclaimer.set_test_observer({});

    // The raw files and the summary. The trace ends with run_end and the verdict, so a trace
    // without it is incomplete evidence. Every file's open, write, flush and close is checked.
    rec.add(app::TraceKind::RunEnd, 0, static_cast<std::uint64_t>(verdict));
    const auto events = rec.events();
    std::vector<std::string> unsaved;
    auto save = [&unsaved](const fs::path& path, const std::string& text) {
        if (std::string error = app::save_text(path, text); !error.empty()) {
            unsaved.push_back(std::move(error));
        }
    };
    save(out / "events.tsv", app::to_tsv(events));
    save(out / "frames.tsv", frame_rows.str());
    save(out / "chunks.tsv", chunk_rows.str());
    save(out / "memory.tsv", memory_rows.str());
    if (sim_world) {
        const std::lock_guard lock(sim_mutex);
        save(out / "sim.tsv", sim_rows.str());
    }
    std::ostringstream manifest;
    manifest << "bench_picture case\nscenario=" << scenario << " timing=" << timing
             << " edits_planned=" << edits << " edits_published=" << plan.size()
             << " edits_refused_by_sim=" << rejected_edits << " rate_per_s=" << rate
             << " frame_ms=" << frame_ms << " duration_s=" << duration_s << " threads=" << threads
             << " chunk_threads=" << chunk_threads << " max_outstanding=" << max_outstanding
             << " commit_every=" << commit_every << " stall_ms=" << stall_ms << " busy=" << busy
             << " sim_contention=" << sim_contention << " sim_unpaced=" << sim_unpaced
             << " publish_progress=" << publish_progress << " view=" << view_w << "x" << view_h
             << " view_island=" << view_island << " view_tile=" << view_x << "," << view_y
             << " map=" << map.string() << " workload_build_ms=" << build_ms << "\n";
    manifest << "--- workload at the start of the loop\n" << manifest_initial;
    manifest << "--- workload after the edits\n" << workload->manifest();
    if (sim_world) {
        manifest << "--- contention world after the run\n" << sim_manifest;
    }
    save(out / "manifest.txt", manifest.str());
    std::ostringstream summary;
    summary << app::format_summary(app::summarize(events));
    summary << "frames=" << frame << " converged=" << (converged ? 1 : 0)
            << " pan_ready_ms=" << pan_ready_ms << " zoom_ready_ms=" << zoom_ready_ms << "\n";
    summary << "stats committed=" << st.committed << " discarded=" << st.discarded
            << " coalesced=" << st.coalesced << " deferred=" << st.deferred
            << " budget_deferrals=" << st.budget_deferrals
            << " commit_skipped=" << st.commit_skipped
            << " outstanding_peak=" << st.outstanding_peak
            << " outstanding_bytes_peak=" << st.outstanding_bytes_peak
            << " result_bytes_peak=" << st.result_bytes_peak
            << " estimate_shortfalls=" << st.estimate_shortfalls
            << " estimate_shortfall_max=" << st.estimate_shortfall_max
            << " copied_buildings=" << st.copied_buildings
            << " invalidated_keys=" << st.invalidated_keys
            << " reclaim_backlog_peak=" << st.reclaim_backlog_peak << "\n";
    summary << "memory peak_incremental_payload=" << peak_incremental
            << " peak_retired_payload=" << peak_retired
            << " peak_reclaim_queue_bytes=" << peak_reclaim_bytes
            << " peak_reservation_bytes=" << peak_outstanding_bytes
            << " peak_rss=" << counter(peak_rss) << " peak_footprint=" << counter(peak_footprint)
            << " majflt_delta=" << delta(mem_start.majflt, mem_end.majflt)
            << " sys_pageins_delta=" << delta(mem_start.sys_pageins, mem_end.sys_pageins)
            << " sys_pageouts_delta=" << delta(mem_start.sys_pageouts, mem_end.sys_pageouts)
            << " sys_swapins_delta=" << delta(mem_start.sys_swapins, mem_end.sys_swapins)
            << " sys_swapouts_delta=" << delta(mem_start.sys_swapouts, mem_end.sys_swapouts)
            << " pressure_start=" << counter(mem_start.pressure)
            << " pressure_end=" << counter(mem_end.pressure) << "\n";
    if (sim_world) {
        summary << "sim turns=" << sim_turns << " accepted=" << sim_accepted
                << " refused_by_sim=" << sim_refused << " no_candidate=" << sim_no_candidate
                << " state_hash=" << std::hex << sim_hash << std::dec
                << " invariants=" << (sim_invariants.empty() ? "ok" : sim_invariants) << "\n";
    }
    summary << verify.str();
    save(out / "summary.txt", summary.str());
    std::cout << summary.str();
    if (!unsaved.empty()) {
        // The evidence is incomplete: exit 5 whatever the verdict was, naming every file.
        for (const std::string& e : unsaved) {
            std::cerr << "bench_picture: evidence not saved: " << e << "\n";
        }
        std::cerr << "bench_picture: the case's verdict was " << verdict << "\n";
        return 5;
    }
    return verdict;
}
