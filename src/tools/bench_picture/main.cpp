// bench_picture: the cost of following the sim's buildings with the picture (ScenePicture,
// docs/app/game-shell.md, "Retained work and reclamation"; method and numbers:
// docs/perf/README.md).
//
//   bench_picture <game_dir> [--mode micro|async] [--map file] [--tier baseline|five|dense]
//                 [--islands n] [--per-island m] [--seed s] [--manifest file]
//                 [--cycles n] [--changes k] [--commit-every c] [--max-outstanding m]
//                 [--threads t] [--frames n] [--edit-every e] [--burst b] [--busy n]
//                 [--view w,h] [--drain-s s] [--verify 0|1]
//
// micro: a SYNCHRONOUS microbenchmark. Each cycle applies a batch, waits for the builds
// (wait_idle),
//        commits, and updates the chunks of the view. It is not a presentation measurement.
// async: a bounded-viewport frame loop with real chunk workers. Each frame commits what is ready
// and
//        updates the view's chunks; changes arrive on a fixed cadence (every `edit-every` frames,
//        `burst` changes) while earlier builds are still running. No wait_idle inside the loop.
//        After the last frame the picture is drained with a diagnostic timeout, and the final
//        chunks of the view are compared with a synchronous CPU reference (--verify 1).
// Workloads are deterministic: the seed fixes the buildings; the manifest records the parameters
// and a hash of the building list. Every option is validated before any asset is loaded.
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

#include <anno/app/building_delta.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/picture_check.hpp>
#include <anno/app/play.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/render/frame.hpp>

namespace fs = std::filesystem;
namespace render = anno::render;
namespace app = anno::app;
using Clock = std::chrono::steady_clock;

namespace {

[[noreturn]] void usage_error(const std::string& message) {
    std::cerr << "bench_picture: " << message << "\n";
    std::exit(2);
}

/// Parses an unsigned option value in [lo, hi]. Rejects empty, signed, non-numeric and overflowing
/// values.
std::size_t parse_count(std::string_view option, std::string_view text, std::size_t lo,
                        std::size_t hi) {
    std::size_t value = 0;
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (text.empty() || ec != std::errc{} || ptr != last) {
        usage_error(std::string(option) + " needs a whole number, got '" + std::string(text) + "'");
    }
    if (value < lo || value > hi) {
        usage_error(std::string(option) + " must be in [" + std::to_string(lo) + ", " +
                    std::to_string(hi) + "], got " + std::string(text));
    }
    return value;
}

double ms_between(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

/// Nearest-rank percentile of a sample (the caller sorts).
double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) {
        return 0;
    }
    const auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(sorted.size())));
    return sorted[std::clamp<std::size_t>(rank == 0 ? 0 : rank - 1, 0, sorted.size() - 1)];
}

struct Series {
    std::string name;
    std::vector<double> ms;
    void print() const {
        std::vector<double> s = ms;
        std::sort(s.begin(), s.end());
        std::printf("| %s | %zu | %.3f | %.3f | %.3f | %.3f |\n", name.c_str(), s.size(),
                    percentile(s, 0.50), percentile(s, 0.95), percentile(s, 0.99),
                    s.empty() ? 0.0 : s.back());
    }
};

/// A small deterministic generator (xorshift64*): the same seed gives the same workload on every
/// host.
struct Rng {
    std::uint64_t state;
    explicit Rng(std::uint64_t seed) : state(seed == 0 ? 0x9E3779B97F4A7C15ULL : seed) {}
    std::uint64_t next() {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 2685821657736338717ULL;
    }
    std::size_t below(std::size_t n) { return static_cast<std::size_t>(next() % n); }
};

std::uint64_t fnv1a(const std::vector<app::SceneBuilding>& list) {
    std::uint64_t h = 1469598103934665603ULL;
    auto mix = [&h](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= (v >> (8 * i)) & 0xFF;
            h *= 1099511628211ULL;
        }
    };
    for (const app::SceneBuilding& b : list) {
        mix(b.island);
        mix(b.type);
        mix(b.x);
        mix(b.y);
        mix(b.rotation);
        mix(b.height);
    }
    return h;
}

/// The building items (anlage, x, y) of an island's records, as the loader and the picture write
/// them.
std::multiset<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>>
building_items(const anno::formats::insel::Island& island, const app::SceneObjects& so) {
    std::multiset<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>> keys;
    for (const anno::formats::chunk::Chunk& rec : island.records) {
        if (rec.name() != "PROD2") {
            continue;
        }
        auto parsed = anno::formats::insel::Block::parse(rec.payload);
        if (!parsed) {
            continue;
        }
        const anno::formats::insel::Block& block = parsed.value();
        for (std::size_t o = 0; o + anno::formats::insel::kItemLen <= block.data.size();
             o += anno::formats::insel::kItemLen) {
            const auto obj = anno::formats::insel::PlacedObject::parse(
                block.data.subspan(o, anno::formats::insel::kItemLen), block);
            if (so.is_building(obj.anlage)) {
                keys.insert({obj.anlage, obj.x, obj.y});
            }
        }
    }
    return keys;
}

std::shared_ptr<const app::BuildingBatch> batch_of(std::uint64_t turn,
                                                   std::vector<app::BuildingDelta> d) {
    auto b = std::make_shared<app::BuildingBatch>();
    b->turn = turn;
    b->deltas = std::move(d);
    return b;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage_error("usage: bench_picture <game_dir> [options]; see the header of this file");
    }
    const fs::path dir = argv[1];
    std::string mode = "micro", tier = "five", manifest_path;
    fs::path map = "data/maps/lan5.txt";
    std::size_t islands = 5, per_island = 40, seed = 1, cycles = 200, changes = 4, commit_every = 1,
                max_outstanding = 8, threads = 2, frame_count = 600, edit_every = 10, burst = 8,
                busy = 0, drain_s = 60, verify = 1, view_w = 1024, view_h = 768;
    for (int i = 2; i < argc; ++i) {
        const std::string key = argv[i];
        if (i + 1 >= argc) {
            usage_error("option " + key + " needs a value");
        }
        const std::string_view val = argv[++i];
        if (key == "--mode") {
            if (val != "micro" && val != "async") {
                usage_error("--mode must be micro or async, got " + std::string(val));
            }
            mode = std::string(val);
        } else if (key == "--tier") {
            if (val != "baseline" && val != "five" && val != "dense") {
                usage_error("--tier must be baseline, five or dense, got " + std::string(val));
            }
            tier = std::string(val);
        } else if (key == "--map") {
            map = std::string(val);
        } else if (key == "--manifest") {
            manifest_path = std::string(val);
        } else if (key == "--islands") {
            islands = parse_count(key, val, 1, 64);
        } else if (key == "--per-island") {
            per_island = parse_count(key, val, 1, 100000);
        } else if (key == "--seed") {
            seed = parse_count(key, val, 1, std::numeric_limits<std::uint32_t>::max());
        } else if (key == "--cycles") {
            cycles = parse_count(key, val, 1, 1000000);
        } else if (key == "--changes") {
            changes = parse_count(key, val, 1, 100000);
        } else if (key == "--commit-every") {
            commit_every = parse_count(key, val, 1, 1000000);
        } else if (key == "--max-outstanding") {
            max_outstanding = parse_count(key, val, 1, 1000);
        } else if (key == "--threads") {
            threads = parse_count(key, val, 1, 64);
        } else if (key == "--frames") {
            frame_count = parse_count(key, val, 1, 10000000);
        } else if (key == "--edit-every") {
            edit_every = parse_count(key, val, 1, 1000000);
        } else if (key == "--burst") {
            burst = parse_count(key, val, 1, 100000);
        } else if (key == "--busy") {
            busy = parse_count(key, val, 0, 256);
        } else if (key == "--drain-s") {
            drain_s = parse_count(key, val, 1, 3600);
        } else if (key == "--verify") {
            verify = parse_count(key, val, 0, 1);
        } else if (key == "--view") {
            const auto comma = val.find(',');
            if (comma == std::string_view::npos) {
                usage_error("--view needs w,h");
            }
            view_w = parse_count("--view w", val.substr(0, comma), 1, 16384);
            view_h = parse_count("--view h", val.substr(comma + 1), 1, 16384);
        } else {
            usage_error("unknown option " + key);
        }
    }

    auto play = app::load_play(dir, map);
    if (!play.ok()) {
        usage_error("cannot load " + map.string() + ": " + play.error());
    }
    app::PlayLoad pl = std::move(play).value();
    std::vector<app::SceneBuilding> base = app::play_buildings(pl.world);
    std::vector<std::uint32_t> types;
    for (const app::SceneBuilding& b : base) {
        types.push_back(b.type);
    }
    if (types.empty()) {
        types.push_back(1);
    }
    app::SceneObjects objects;
    objects.is_building = [&pl](std::uint16_t id) { return app::anlage_is_building(pl.world, id); };
    objects.buildings = base;
    auto loaded = app::load_placed_scene(dir, pl.name, pl.placed, &objects);
    if (!loaded.ok()) {
        usage_error("scene: " + loaded.error());
    }
    app::LoadedScene ls = std::move(loaded).value();
    ls.scene->river_cfg.enabled = false; // the building check is the legacy look
    std::shared_ptr<const render::Scene> scene(std::move(ls.scene));
    if (scene->islands.size() < islands) {
        usage_error("the map has " + std::to_string(scene->islands.size()) +
                    " islands, --islands asks for " + std::to_string(islands));
    }

    // Coordinates are island-local, as SceneBuilding is (play_buildings) and as the scene's records
    // are. The workload: `islands` islands (the first ones in the scene order, which is the map
    // order), each with `per_island` buildings on distinct tiles inside its local box. Every tile
    // holds at most one building.
    std::vector<app::SceneBuilding> buildings;
    std::map<std::uint32_t, std::size_t> per_slot;
    {
        Rng rng(seed);
        const std::size_t count =
            tier == "baseline" ? 0 : (tier == "five" ? per_island : per_island);
        for (std::size_t s = 0; s < islands; ++s) {
            const auto& island = scene->islands[s];
            std::set<std::pair<std::uint16_t, std::uint16_t>> used;
            const std::size_t w = std::max<std::size_t>(island.width, 1),
                              h = std::max<std::size_t>(island.height, 1);
            if (count > w * h) {
                usage_error("island " + std::to_string(island.number) + " holds " +
                            std::to_string(w * h) + " tiles, --per-island asks for " +
                            std::to_string(count));
            }
            for (std::size_t k = 0; k < count && used.size() < w * h; ++k) {
                std::pair<std::uint16_t, std::uint16_t> at;
                do {
                    at = {static_cast<std::uint16_t>(rng.below(w)),
                          static_cast<std::uint16_t>(rng.below(h))};
                } while (!used.insert(at).second);
                app::SceneBuilding b;
                b.island = island.number;
                b.type = types[rng.below(types.size())];
                b.x = at.first;
                b.y = at.second;
                b.rotation = static_cast<std::uint8_t>(rng.below(4));
                b.height = 0;
                buildings.push_back(b);
            }
            per_slot[island.number] = count;
        }
    }
    // Picks go round-robin over the islands: `tracked` keeps the island-major order; the picture
    // gets it sorted.
    std::vector<app::SceneBuilding> tracked = buildings;
    const std::size_t per_pick = per_island;
    // One building per tile of an island (a legal placement). A move goes to a free tile next to
    // the building, the turn's direction first, inside the island's local box; when no neighbour is
    // free the edit is skipped.
    std::map<std::uint32_t, std::set<std::pair<std::uint16_t, std::uint16_t>>> occupied;
    for (const app::SceneBuilding& b : tracked) {
        occupied[b.island].insert({b.x, b.y});
    }
    auto propose_move = [&](const app::SceneBuilding& old, std::uint64_t t,
                            app::SceneBuilding& moved) -> bool {
        const auto island = std::find_if(scene->islands.begin(), scene->islands.end(),
                                         [&old](const auto& i) { return i.number == old.island; });
        if (island == scene->islands.end()) {
            return false;
        }
        auto& taken = occupied[old.island];
        const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        const int start = t % 2 == 0 ? 0 : 1;
        for (int d = 0; d < 4; ++d) {
            const int* step = dirs[(start + d) % 4];
            const int nx = static_cast<int>(old.x) + step[0];
            const int ny = static_cast<int>(old.y) + step[1];
            if (nx < 0 || ny < 0 || nx >= static_cast<int>(island->width) ||
                ny >= static_cast<int>(island->height)) {
                continue;
            }
            const auto target = std::pair<std::uint16_t, std::uint16_t>{
                static_cast<std::uint16_t>(nx), static_cast<std::uint16_t>(ny)};
            if (taken.count(target) != 0) {
                continue;
            }
            taken.erase({old.x, old.y});
            taken.insert(target);
            moved = old;
            moved.x = target.first;
            moved.y = target.second;
            return true;
        }
        return false;
    };
    auto pick_index = [&](std::size_t c) {
        return (c % islands) * per_pick + (c / islands) % per_pick;
    };
    app::sort_buildings(buildings);
    // Validation before the run: every building is an island the scene draws, lies inside that
    // island's local width and height, and no two buildings share a tile of one island.
    {
        std::set<std::tuple<std::uint32_t, std::uint16_t, std::uint16_t>> tiles;
        for (const app::SceneBuilding& b : buildings) {
            const auto it = std::find_if(scene->islands.begin(), scene->islands.end(),
                                         [&b](const auto& i) { return i.number == b.island; });
            if (it == scene->islands.end()) {
                usage_error("workload building on an island the scene does not draw (" +
                            std::to_string(b.island) + ")");
            }
            if (b.x >= it->width || b.y >= it->height) {
                usage_error("workload building outside island " + std::to_string(b.island) + " (" +
                            std::to_string(b.x) + "," + std::to_string(b.y) + ")");
            }
            if (!tiles.insert({b.island, b.x, b.y}).second) {
                usage_error("two workload buildings share a tile on island " +
                            std::to_string(b.island));
            }
        }
    }
    // Generation 0 must hold the workload too: the picture starts from the scene's buildings, as
    // the play loader gives them (the sim's buildings at load). The scene is loaded again with the
    // workload's list; the island geometry above came from the first load, which is the same map.
    {
        app::SceneObjects with_workload;
        with_workload.is_building = [&pl](std::uint16_t id) {
            return app::anlage_is_building(pl.world, id);
        };
        with_workload.buildings = buildings;
        auto again = app::load_placed_scene(dir, pl.name, pl.placed, &with_workload);
        if (!again.ok()) {
            usage_error("scene with the workload: " + again.error());
        }
        app::LoadedScene ls2 = std::move(again).value();
        ls2.scene->river_cfg.enabled = false;
        scene = std::shared_ptr<const render::Scene>(std::move(ls2.scene));
    }
    const std::uint64_t hash = fnv1a(buildings);
    std::size_t drawn_objects = 0;
    for (std::size_t s = 0; s < scene->islands.size(); ++s) {
        const render::IslandLists& lists = scene->island_lists(s);
        drawn_objects += lists.below.size() + lists.above.size() + lists.other.size();
    }
    if (!manifest_path.empty()) {
        if (FILE* f = std::fopen(manifest_path.c_str(), "w")) {
            std::fprintf(
                f,
                "bench_picture workload manifest\nmode=%s tier=%s map=%s seed=%zu islands=%zu "
                "per_island=%zu\n"
                "buildings=%zu building_hash=%016llx scene_islands=%zu drawn_objects_in_lists=%zu\n"
                "threads=%zu max_outstanding=%zu frames=%zu edit_every=%zu burst=%zu busy=%zu "
                "view=%zux%zu\n",
                mode.c_str(), tier.c_str(), map.string().c_str(), seed, islands, per_island,
                buildings.size(), static_cast<unsigned long long>(hash), scene->islands.size(),
                drawn_objects, threads, max_outstanding, frame_count, edit_every, burst, busy,
                view_w, view_h);
            std::fclose(f);
        }
    }

    std::atomic<bool> stop_busy{false};
    std::vector<std::thread> busy_threads;
    for (std::size_t t = 0; t < busy; ++t) {
        busy_threads.emplace_back([&stop_busy] {
            volatile std::uint64_t x = 1;
            while (!stop_busy.load(std::memory_order_relaxed)) {
                x = x * 6364136223846793005ULL + 1442695040888963407ULL;
            }
        });
    }
    auto join_busy = [&] {
        stop_busy = true;
        for (std::thread& t : busy_threads) {
            t.join();
        }
    };

    app::ScenePicture::Limits limits;
    limits.max_outstanding = max_outstanding;
    auto is_building = [&pl](std::uint16_t id) { return app::anlage_is_building(pl.world, id); };
    app::ScenePicture picture(scene, buildings, is_building, threads, limits);
    render::FrameRenderer frames(1);
    // The viewport: a bounded window centred on island 0's placement at the group-2 zoom (the draw
    // layout of the scene), so the view holds the changed content. The view is not the whole scene.
    render::Rect placed{};
    for (const render::Placement& p : scene->layouts[2]) {
        if (p.index == 0) {
            placed = p.bounds;
        }
    }
    const int cx = placed.x + placed.w / 2, cy = placed.y + placed.h / 2;
    const render::Rect view{cx - static_cast<int>(view_w) / 2, cy - static_cast<int>(view_h) / 2,
                            static_cast<int>(view_w), static_cast<int>(view_h)};

    std::printf("bench_picture: mode=%s tier=%s islands=%zu per_island=%zu seed=%zu buildings=%zu "
                "hash=%016llx "
                "drawn_objects=%zu threads=%zu max_outstanding=%zu busy=%zu view=%zux%zu\n\n",
                mode.c_str(), tier.c_str(), islands, per_island, seed, buildings.size(),
                static_cast<unsigned long long>(hash), drawn_objects, threads, max_outstanding,
                busy, view_w, view_h);

    if (mode == "micro") {
        std::printf("MICROBENCHMARK (synchronous: waits for workers each cycle; not a presentation "
                    "measurement)\n\n");
        frames.update_chunks(scene, view,
                             2); // warm the view: the stage measures only what each commit changes
        Series apply{"apply (window)", {}}, build{"build wait (apply to idle)", {}},
            commit{"commit (window)", {}},
            chunks{"chunk update after commit (window, synchronous)", {}};
        std::uint64_t turn = 1;
        std::size_t cursor = 0;
        for (std::size_t cycle = 0; cycle < cycles; ++cycle) {
            std::vector<app::BuildingDelta> deltas;
            for (std::size_t k = 0; k < changes && !tracked.empty(); ++k) {
                const std::size_t pick = pick_index(cursor++);
                const app::SceneBuilding old = tracked[pick];
                app::SceneBuilding moved{};
                if (!propose_move(old, turn, moved)) {
                    continue; // no free neighbour tile: this edit is skipped
                }
                deltas.push_back({app::BuildingDelta::Kind::Removed, old});
                deltas.push_back({app::BuildingDelta::Kind::Added, moved});
                tracked[pick] = moved;
            }
            const auto t0 = Clock::now();
            picture.apply({batch_of(turn++, std::move(deltas))});
            const auto t1 = Clock::now();
            picture.wait_idle();
            const auto t2 = Clock::now();
            apply.ms.push_back(ms_between(t0, t1));
            build.ms.push_back(ms_between(t1, t2));
            if ((cycle + 1) % commit_every == 0) {
                const auto t3 = Clock::now();
                const std::size_t committed = picture.commit(frames);
                const auto t4 = Clock::now();
                commit.ms.push_back(ms_between(t3, t4));
                if (committed > 0) {
                    const auto t5 = Clock::now();
                    frames.update_chunks(picture.scene(), view, 2);
                    chunks.ms.push_back(ms_between(t5, Clock::now()));
                }
            }
        }
        const auto drain_end = Clock::now() + std::chrono::seconds(drain_s);
        while (picture.pending() > 0) {
            if (Clock::now() > drain_end) {
                join_busy();
                usage_error("drain did not converge within --drain-s " + std::to_string(drain_s) +
                            " (pending " + std::to_string(picture.pending()) + ")");
            }
            picture.wait_idle();
            picture.commit(frames);
        }
        join_busy();
        const auto st = picture.stats();
        std::printf("| stage | samples | p50 ms | p95 ms | p99 ms | max ms "
                    "|\n|---|---:|---:|---:|---:|---:|\n");
        apply.print();
        build.print();
        commit.print();
        chunks.print();
        std::printf(
            "\ncommitted=%llu discarded=%llu coalesced=%llu deferred=%llu started_deferred=%llu "
            "rebased=%llu "
            "commit_skipped=%llu build_failures=%llu version_requeues=%llu invalidated_keys=%llu "
            "outstanding_peak=%zu reclaim_backlog_peak=%zu reclaim_queued=%llu "
            "reclaim_completed=%llu\n",
            static_cast<unsigned long long>(st.committed),
            static_cast<unsigned long long>(st.discarded),
            static_cast<unsigned long long>(st.coalesced),
            static_cast<unsigned long long>(st.deferred),
            static_cast<unsigned long long>(st.started_deferred),
            static_cast<unsigned long long>(st.rebased),
            static_cast<unsigned long long>(st.commit_skipped),
            static_cast<unsigned long long>(st.build_failures),
            static_cast<unsigned long long>(st.version_requeues),
            static_cast<unsigned long long>(st.invalidated_keys), st.outstanding_peak,
            st.reclaim_backlog_peak, static_cast<unsigned long long>(st.reclaim_queued),
            static_cast<unsigned long long>(st.reclaim_completed));
        return 0;
    }

    // async: real chunk workers; the frame loop never waits for them.
    auto owned = std::make_unique<render::ChunkWorkers>(*scene, threads);
    render::ChunkWorkers* chunk_workers = owned.get();
    frames.set_workers(std::move(owned));
    frames.update_chunks(scene, view, 2); // warm the view before the loop (not measured)
    Series apply{"apply (window)", {}}, commit{"commit (window)", {}},
        frame{"frame: update_chunks (window)", {}};
    std::uint64_t turn = 1;
    std::size_t cursor = 0;
    std::size_t edits = 0;
    const auto loop_start = Clock::now();
    for (std::size_t f = 0; f < frame_count; ++f) {
        if (f % edit_every == 0) {
            std::vector<app::BuildingDelta> deltas;
            for (std::size_t k = 0; k < burst && !tracked.empty(); ++k) {
                const std::size_t pick = pick_index(cursor++);
                const app::SceneBuilding old = tracked[pick];
                app::SceneBuilding moved{};
                if (!propose_move(old, turn, moved)) {
                    continue; // no free neighbour tile: this edit is skipped
                }
                deltas.push_back({app::BuildingDelta::Kind::Removed, old});
                deltas.push_back({app::BuildingDelta::Kind::Added, moved});
                tracked[pick] = moved;
                ++edits;
            }
            const auto t0 = Clock::now();
            picture.apply({batch_of(turn++, std::move(deltas))});
            apply.ms.push_back(ms_between(t0, Clock::now()));
        }
        const auto t1 = Clock::now();
        if (f % commit_every == 0) {
            picture.commit(frames);
        }
        commit.ms.push_back(ms_between(t1, Clock::now()));
        const auto t2 = Clock::now();
        frames.update_chunks(picture.scene(), view, 2);
        frame.ms.push_back(ms_between(t2, Clock::now()));
    }
    const double loop_ms = ms_between(loop_start, Clock::now());
    // Drain with a diagnostic: progress is reported; the bound is --drain-s.
    const auto drain_start = Clock::now();
    const auto drain_end = drain_start + std::chrono::seconds(drain_s);
    std::size_t last_pending = picture.pending();
    auto next_report = drain_start + std::chrono::seconds(5);
    while (picture.pending() > 0) {
        if (Clock::now() > drain_end) {
            join_busy();
            usage_error("drain did not converge within --drain-s " + std::to_string(drain_s) +
                        " (pending " + std::to_string(picture.pending()) + ")");
        }
        if (Clock::now() > next_report) {
            std::fprintf(stderr, "bench_picture: draining, pending %zu (was %zu)\n",
                         picture.pending(), last_pending);
            last_pending = picture.pending();
            next_report += std::chrono::seconds(5);
        }
        picture.commit(frames);
        frames.update_chunks(picture.scene(), view, 2);
        std::this_thread::yield();
    }
    const double drain_ms = ms_between(drain_start, Clock::now());
    const auto worker_timings = picture.timings();
    join_busy();
    const auto st = picture.stats();
    std::printf("ASYNC bounded-viewport frame loop with %zu chunk workers; not a presented-frame "
                "measurement\n",
                threads);
    std::printf("commit every %zu frame(s) (a delayed consumer when > 1)\n", commit_every);
    std::printf("frames=%zu edits=%zu (burst %zu every %zu frames) loop_ms=%.1f drain_ms=%.1f\n\n",
                frame_count, edits, burst, edit_every, loop_ms, drain_ms);
    std::printf("| stage | samples | p50 ms | p95 ms | p99 ms | max ms "
                "|\n|---|---:|---:|---:|---:|---:|\n");
    apply.print();
    commit.print();
    frame.print();
    Series queue{"job queue delay (worker, submit to start)", worker_timings.queue_ms};
    Series build{"job build time (worker)", worker_timings.build_ms};
    queue.print();
    build.print();
    std::printf(
        "\ncommitted=%llu discarded=%llu coalesced=%llu deferred=%llu started_deferred=%llu "
        "rebased=%llu "
        "commit_skipped=%llu build_failures=%llu version_requeues=%llu outstanding_peak=%zu "
        "outstanding_bytes_peak=%zu reclaim_backlog_peak=%zu reclaim_queued=%llu "
        "reclaim_completed=%llu "
        "failed_islands=%zu dirty_at_end=%zu\n",
        static_cast<unsigned long long>(st.committed),
        static_cast<unsigned long long>(st.discarded),
        static_cast<unsigned long long>(st.coalesced), static_cast<unsigned long long>(st.deferred),
        static_cast<unsigned long long>(st.started_deferred),
        static_cast<unsigned long long>(st.rebased),
        static_cast<unsigned long long>(st.commit_skipped),
        static_cast<unsigned long long>(st.build_failures),
        static_cast<unsigned long long>(st.version_requeues), st.outstanding_peak,
        st.outstanding_bytes_peak, st.reclaim_backlog_peak,
        static_cast<unsigned long long>(st.reclaim_queued),
        static_cast<unsigned long long>(st.reclaim_completed), st.failed_islands,
        picture.pending());
    // Counts after the run: the objects of every island in the final generation (what is drawn).
    std::size_t drawn_final = 0;
    for (std::size_t s = 0; s < picture.scene()->islands.size(); ++s) {
        const render::IslandLists& lists = picture.scene()->island_lists(s);
        drawn_final += lists.below.size() + lists.above.size() + lists.other.size();
    }
    std::printf(
        "drawn objects: at load %zu, in the final generation %zu (buildings in the picture %zu)\n",
        drawn_objects, drawn_final, picture.buildings().size());
    if (verify != 0) {
        // The loader's own build of the same placed scene with the final building list: the
        // picture's drawn objects must equal its objects (an oracle that does not use the picture's
        // code).
        {
            {
                // Legal placements have one building per tile: duplicates would make the two builds
                // differ.
                std::set<std::tuple<std::uint32_t, std::uint32_t, std::uint16_t, std::uint16_t>>
                    seen;
                std::size_t duplicates = 0;
                for (const app::SceneBuilding& b : picture.buildings()) {
                    duplicates += seen.insert({b.island, b.type, b.x, b.y}).second ? 0U : 1U;
                }
                std::printf("duplicate tiles in the final building list: %zu\n", duplicates);
                if (duplicates != 0) {
                    std::printf("VERIFY FAILED: duplicate tiles\n");
                    return 1;
                }
            }
            std::vector<app::SceneBuilding> final_list = picture.buildings();
            app::SceneObjects so;
            so.is_building = [&pl](std::uint16_t id) {
                return app::anlage_is_building(pl.world, id);
            };
            so.buildings = final_list;
            auto fresh = app::load_placed_scene(dir, pl.name, pl.placed, &so);
            if (!fresh.ok()) {
                usage_error("the loader's own build failed: " + fresh.error());
            }
            app::LoadedScene fs = std::move(fresh).value();
            std::size_t loader_objects = 0, picture_objects = 0;
            for (std::size_t s = 0; s < fs.scene->islands.size(); ++s) {
                const render::IslandLists& l = fs.scene->island_lists(s);
                loader_objects += l.below.size() + l.above.size() + l.other.size();
            }
            for (std::size_t s = 0; s < picture.scene()->islands.size(); ++s) {
                const render::IslandLists& l = picture.scene()->island_lists(s);
                picture_objects += l.below.size() + l.above.size() + l.other.size();
            }
            for (std::size_t s = 0; s < fs.scene->islands.size(); ++s) {
                const render::IslandLists& l = fs.scene->island_lists(s);
                const render::IslandLists& p2 = picture.scene()->island_lists(s);
                const std::size_t a = l.below.size() + l.above.size() + l.other.size();
                const std::size_t b = p2.below.size() + p2.above.size() + p2.other.size();
                if (a != b) {
                    std::printf("  island #%u: loader %zu, picture %zu\n",
                                fs.scene->islands[s].number, a, b);
                    const auto want = building_items(fs.scene->islands[s], so);
                    const auto got = building_items(picture.scene()->islands[s], so);
                    std::size_t shown = 0;
                    for (const auto& key : want) {
                        if (got.count(key) < want.count(key) && shown++ < 4) {
                            std::printf("    loader-only item: anlage %u at %u,%u\n",
                                        std::get<0>(key), std::get<1>(key), std::get<2>(key));
                        }
                    }
                    for (const auto& key : got) {
                        if (want.count(key) < got.count(key) && shown++ < 8) {
                            std::printf("    picture-only item: anlage %u at %u,%u\n",
                                        std::get<0>(key), std::get<1>(key), std::get<2>(key));
                        }
                    }
                }
            }
            std::printf("loader oracle: objects drawn by the loader's build of the final buildings "
                        "%zu, by the picture %zu%s\n",
                        loader_objects, picture_objects,
                        loader_objects == picture_objects ? " (equal)" : " (DIFFERENT)");
            // Counts are diagnostics only (a count-preserving error passes them): the verdict is
            // the chunk check below.
        }
        // The chunk workers settle first: the picture's convergence does not wait for them. The
        // picture's chunks of the view are then compared, layer by layer, with a fresh synchronous
        // build of an independently loaded scene that holds the final desired building list (round
        // dx, DX1).
        auto final_scene = picture.scene();
        for (int round = 0; round < 3; ++round) {
            chunk_workers->wait_idle();
            frames.update_chunks(final_scene, view, 2);
        }
        chunk_workers->wait_idle();
        app::SceneObjects fresh_objects;
        fresh_objects.is_building = [&pl](std::uint16_t id) {
            return app::anlage_is_building(pl.world, id);
        };
        fresh_objects.buildings = picture.buildings();
        auto fresh_loaded = app::load_placed_scene(dir, pl.name, pl.placed, &fresh_objects);
        if (!fresh_loaded.ok()) {
            usage_error("the reference scene failed: " + fresh_loaded.error());
        }
        app::LoadedScene fresh_ls = std::move(fresh_loaded).value();
        fresh_ls.scene->river_cfg.enabled = false;
        const std::shared_ptr<const render::Scene> reference_scene(std::move(fresh_ls.scene));
        render::FrameRenderer reference(1);
        for (int round = 0; round < 3; ++round) {
            reference.update_chunks(reference_scene, view, 2);
        }
        const std::vector<render::ChunkKey> keys =
            render::chunk_keys_touching(*reference_scene, view, 2);
        const app::ChunkDiff diff = app::compare_chunks(
            keys, [&frames](const render::ChunkKey& k) { return frames.chunk(k); },
            [&reference](const render::ChunkKey& k) { return reference.chunk(k); });
        std::printf("verify: group-2 chunks of the view compared=%zu with_objects=%zu ground=%zu "
                    "objects=%zu "
                    "terrain=%zu field=%zu missing=%zu\n",
                    diff.compared, diff.with_objects, diff.ground, diff.objects, diff.terrain,
                    diff.field, diff.missing);
        if (!diff.equal()) {
            std::printf("VERIFY FAILED\n");
            return 1;
        }
    }
    return 0;
}
