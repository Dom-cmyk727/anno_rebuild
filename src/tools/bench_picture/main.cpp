// bench_picture: the cost of following the sim's buildings (ScenePicture, docs/app/game-shell.md,
// "Retained work") on the window thread and in the workers, with the counters of the bridge.
//
//   bench_picture <game_dir> [--map file.txt] [--tier five|stress] [--density n]
//                 [--cycles n] [--changes k] [--threads t] [--max-outstanding m]
//                 [--commit-every c] [--busy n]
//
// The five tier is the fixed map as --play loads it (data/maps/lan5.txt, the two islands with
// buildings). The stress tier repeats each building `density` times at tile offsets on its island
// (synthetic: the picture does not check that the placement is legal, only the work). Each cycle
// applies `changes` moves in one batch, waits for the builds, commits every `commit-every` cycles,
// and then updates the chunks of the island's view. `--busy n` runs n threads of arithmetic to
// stand in for the simulation's share of the CPU. Output: one Markdown table of p50/p95/p99/max
// per stage, in ms, and the counters of the last cycle. Method: docs/perf/README.md.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <anno/app/building_delta.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/render/frame.hpp>

namespace fs = std::filesystem;
namespace render = anno::render;
using Clock = std::chrono::steady_clock;

namespace {

double ms_between(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

/// Percentile of a sorted sample (nearest rank).
double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) {
        return 0;
    }
    const auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(sorted.size())));
    return sorted[std::clamp<std::size_t>(rank == 0 ? 0 : rank - 1, 0, sorted.size() - 1)];
}

struct Series {
    const char* name;
    std::vector<double> ms;
    void print() const {
        std::vector<double> s = ms;
        std::sort(s.begin(), s.end());
        std::printf("| %s | %.3f | %.3f | %.3f | %.3f |\n", name, percentile(s, 0.50),
                    percentile(s, 0.95), percentile(s, 0.99), s.empty() ? 0.0 : s.back());
    }
};

anno::app::SceneBuilding offset(const anno::app::SceneBuilding& b, int copy) {
    anno::app::SceneBuilding out = b;
    // The copies walk along the island's diagonal: a deterministic, dense layout.
    out.x = static_cast<std::uint16_t>(b.x + (copy % 16) * 3);
    out.y = static_cast<std::uint16_t>(b.y + (copy / 16) * 3);
    return out;
}

std::shared_ptr<const anno::app::BuildingBatch> batch_of(std::uint64_t turn,
                                                         std::vector<anno::app::BuildingDelta> d) {
    auto b = std::make_shared<anno::app::BuildingBatch>();
    b->turn = turn;
    b->deltas = std::move(d);
    return b;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: bench_picture <game_dir> [--map f] [--tier five|stress] [--density n] "
                     "[--cycles n] [--changes k] [--threads t] [--max-outstanding m] "
                     "[--commit-every c] [--busy n]\n";
        return 2;
    }
    const fs::path dir = argv[1];
    fs::path map = "data/maps/lan5.txt";
    std::string tier = "five";
    std::size_t density = 8, cycles = 200, changes = 4, threads = 2, max_outstanding = 8,
                commit_every = 1, busy = 0;
    for (int i = 2; i + 1 < argc; i += 2) {
        const std::string key = argv[i];
        const std::string val = argv[i + 1];
        if (key == "--map") {
            map = val;
        } else if (key == "--tier") {
            tier = val;
        } else if (key == "--density") {
            density = std::stoul(val);
        } else if (key == "--cycles") {
            cycles = std::stoul(val);
        } else if (key == "--changes") {
            changes = std::stoul(val);
        } else if (key == "--threads") {
            threads = std::stoul(val);
        } else if (key == "--max-outstanding") {
            max_outstanding = std::stoul(val);
        } else if (key == "--commit-every") {
            commit_every = std::stoul(val);
        } else if (key == "--busy") {
            busy = std::stoul(val);
        } else {
            std::cerr << "unknown option " << key << "\n";
            return 2;
        }
    }
    auto play = anno::app::load_play(dir, map);
    if (!play.ok()) {
        std::cerr << "cannot load " << map << ": " << play.error() << "\n";
        return 1;
    }
    anno::app::PlayLoad pl = std::move(play).value();
    std::vector<anno::app::SceneBuilding> buildings = anno::app::play_buildings(pl.world);
    if (tier == "stress") {
        std::vector<anno::app::SceneBuilding> dense;
        for (const auto& b : buildings) {
            for (std::size_t c = 0; c < density; ++c) {
                dense.push_back(offset(b, static_cast<int>(c)));
            }
        }
        buildings = std::move(dense);
    }
    anno::app::sort_buildings(buildings);
    anno::app::SceneObjects objects;
    objects.is_building = [&pl](std::uint16_t id) {
        return anno::app::anlage_is_building(pl.world, id);
    };
    objects.buildings = buildings;
    auto loaded = anno::app::load_placed_scene(dir, pl.name, pl.placed, &objects);
    if (!loaded.ok()) {
        std::cerr << "scene: " << loaded.error() << "\n";
        return 1;
    }
    anno::app::LoadedScene ls = std::move(loaded).value();
    ls.scene->river_cfg.enabled = true;
    std::shared_ptr<const render::Scene> base(std::move(ls.scene));
    const auto view = base->bounds(2);
    if (!view) {
        std::cerr << "no island bounds\n";
        return 1;
    }

    // Simulation stand-in: arithmetic threads that compete for the cores while the window works.
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

    anno::app::ScenePicture::Limits limits;
    limits.max_outstanding = max_outstanding;
    anno::app::ScenePicture picture(
        base, buildings,
        [&pl](std::uint16_t id) { return anno::app::anlage_is_building(pl.world, id); }, threads,
        limits);
    render::FrameRenderer frames(1);
    // Warm the chunks of the view first: the stage measures only the chunks each commit changes.
    frames.update_chunks(base, *view, 2);
    std::uint64_t turn = 1;
    std::size_t cursor = 0;
    Series apply{"apply (window)", {}}, build{"build wait (apply to idle)", {}},
        commit{"commit (window)", {}}, chunks{"chunk update after commit (window)", {}};
    for (std::size_t cycle = 0; cycle < cycles; ++cycle) {
        std::vector<anno::app::BuildingDelta> deltas;
        for (std::size_t k = 0; k < changes && !buildings.empty(); ++k) {
            // Sequential picks: the changes of one batch are distinct buildings.
            const std::size_t pick = cursor++ % buildings.size();
            const anno::app::SceneBuilding old = buildings[pick];
            anno::app::SceneBuilding moved = old;
            moved.x = static_cast<std::uint16_t>(old.x + (turn % 2 == 0 ? 1 : -1));
            deltas.push_back({anno::app::BuildingDelta::Kind::Removed, old});
            deltas.push_back({anno::app::BuildingDelta::Kind::Added, moved});
            // The bench's own list follows the change, so the next pick sees the moved building.
            buildings[pick] = moved;
        }
        anno::app::sort_buildings(buildings);
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
                frames.update_chunks(picture.scene(), *view, 2);
                chunks.ms.push_back(ms_between(t5, Clock::now()));
            }
        }
    }
    while (picture.pending() > 0) {
        picture.wait_idle();
        picture.commit(frames);
    }
    stop_busy = true;
    for (std::thread& t : busy_threads) {
        t.join();
    }
    const auto st = picture.stats();
    std::printf(
        "bench_picture: tier=%s buildings=%zu islands=%zu cycles=%zu changes=%zu threads=%zu "
        "busy=%zu max_outstanding=%zu\n\n",
        tier.c_str(), buildings.size(), base->islands.size(), cycles, changes, threads, busy,
        max_outstanding);
    std::printf("| stage | p50 ms | p95 ms | p99 ms | max ms |\n|---|---:|---:|---:|---:|\n");
    apply.print();
    build.print();
    commit.print();
    chunks.print();
    std::printf(
        "\ncommitted=%llu discarded=%llu coalesced=%llu deferred=%llu started_deferred=%llu "
        "rebased=%llu commit_skipped=%llu build_failures=%llu recomputed_changes=%llu "
        "invalidated_keys=%llu outstanding_peak=%zu reclaim_backlog_peak=%zu reclaimed=%llu\n",
        static_cast<unsigned long long>(st.committed),
        static_cast<unsigned long long>(st.discarded),
        static_cast<unsigned long long>(st.coalesced), static_cast<unsigned long long>(st.deferred),
        static_cast<unsigned long long>(st.started_deferred),
        static_cast<unsigned long long>(st.rebased),
        static_cast<unsigned long long>(st.commit_skipped),
        static_cast<unsigned long long>(st.build_failures),
        static_cast<unsigned long long>(st.recomputed_changes),
        static_cast<unsigned long long>(st.invalidated_keys), st.outstanding_peak,
        st.reclaim_backlog_peak, static_cast<unsigned long long>(st.reclaimed));
    return 0;
}
