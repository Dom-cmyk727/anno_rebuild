// anno-sim-replay: drive and diagnose a simulation replay log (docs/sim/state.md
// §8.7). Subcommands:
//   run <log>                 replay the log, report whether every checkpoint matched
//   hashes <log>              the state hash after each entry, one per line (hex)
//   first-diff <a> <b>        the first entry at which two `hashes` outputs differ
//   tree <log> --entry N      the hash tree at entry N, one "path digest" line per
//                             scope (--depth D, default 8)
//        [--dump-scope PATH]  instead, the scope's fields as text (PrintArchive); PATH
//                             as `tree` prints it or diff_trees returns it
//   golden <log>              a stable, committable fingerprint of the whole run
//   measure <scenario>        wall time of state_hash after N ticks (default
//         [--ticks N]         30000) over R reps (default 50): every chunk clean,
//         [--reps R]          then one tile write on each island. Also save()
//                             bytes and the tile and chunk counts. The clock is
//                             this tool's; src/sim has none.
//   join <scenario>           wall time of World::load on an already serialized
//         [--ticks N]         save, then of catch-up through run_turn. The age
//         [--reps R]          is the same advance(100) loop as measure. A turn
//         [--warmup W]        is run_turn of 10 steps of 10 game ms (50 ticks),
//         [--catchup T]       not one advance and not one tick. Load is timed
//         [--replay-reps N]   apart from file I/O, scenario parsing, registry
//                             creation and save generation. A hash mismatch
//                             drops the timings. synthetic needs no game files.
//
// The log carries the registry's digest, not the registry itself, so a world is
// rebuilt from the log's initial save on the code-built registry whose digest the log
// names: the empty one (the original goods table, no building types: the headless
// golden and the S9 tests), the synthetic types (synthetic::make_synthetic_types) or
// the synthetic types with build costs (synthetic::make_build_types, the golden G2 of
// S10), with growing tile objects (synthetic::make_growth_types, the golden G5 of S13) or
// with a market cart and a workshop carrier (synthetic::make_transport_types, the golden G6
// of T24).
// A log made against another registry reports a digest mismatch here (the full
// scenario path arrives with the game files, S14).
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <variant>
#include <vector>

#include <anno/sim/replay.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>

namespace {

namespace sim = anno::sim;

std::optional<std::vector<std::uint8_t>> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>());
}

std::string hex64(std::uint64_t v) {
    std::ostringstream o;
    o << std::hex << std::nouppercase;
    o.width(16);
    o.fill('0');
    o << v;
    return o.str();
}

std::optional<sim::ReplayLog> load_log(const std::string& path) {
    const auto bytes = read_file(path);
    if (!bytes) {
        std::cerr << "cannot read " << path << "\n";
        return std::nullopt;
    }
    auto log = sim::read_log(*bytes);
    if (!log.ok()) {
        std::cerr << "bad replay log: " << log.error() << "\n";
        return std::nullopt;
    }
    return std::move(log).value();
}

/// The code-built registry whose digest the log names; the empty one when none matches
/// (World::load then names the mismatch).
std::shared_ptr<const sim::TypeRegistry> registry_for(const sim::ReplayLog& log) {
    std::vector<std::shared_ptr<const sim::TypeRegistry>> known{sim::TypeRegistry::empty()};
    for (auto make : {&sim::synthetic::make_synthetic_types, &sim::synthetic::make_build_types,
                      &sim::synthetic::make_growth_types, &sim::synthetic::make_transport_types}) {
        auto t = make();
        if (t.ok()) {
            known.push_back(t.value().registry);
        }
    }
    for (const auto& r : known) {
        if (r->digest() == log.registry_digest) {
            return r;
        }
    }
    return sim::TypeRegistry::empty();
}

std::optional<sim::World> build_world(const sim::ReplayLog& log) {
    auto w = sim::World::load(log.initial_save, registry_for(log), {});
    if (!w.ok()) {
        std::cerr << "cannot rebuild the initial world from the log: " << w.error() << "\n";
        return std::nullopt;
    }
    return std::move(w).value();
}

int cmd_run(const std::string& path) {
    auto log = load_log(path);
    if (!log) {
        return 2;
    }
    auto world = build_world(*log);
    if (!world) {
        return 2;
    }
    auto outcome = sim::replay(*world, *log);
    if (!outcome.ok()) {
        std::cerr << outcome.error() << "\n";
        return 2;
    }
    const sim::ReplayOutcome& o = outcome.value();
    std::cout << "entries " << o.entries_applied << ", checkpoints " << o.checkpoints_matched << "/"
              << o.checkpoints_total << ", final tick " << o.final_tick << "\n";
    if (o.reproduced()) {
        std::cout << "reproduced: every checkpoint matched\n";
        return 0;
    }
    std::cout << "DIVERGED at entry " << *o.diverged_entry << ": expected "
              << hex64(o.expected_hash) << ", got " << hex64(o.actual_hash) << "\n";
    return 1;
}

int cmd_hashes(const std::string& path) {
    auto log = load_log(path);
    if (!log) {
        return 2;
    }
    auto world = build_world(*log);
    if (!world) {
        return 2;
    }
    for (const std::uint64_t h : sim::hash_per_entry(*world, *log)) {
        std::cout << hex64(h) << "\n";
    }
    return 0;
}

/// The whole of `text` as an unsigned number in `base`, or nullopt: an empty string, a
/// sign, a leading space, trailing characters and a value beyond u64 are all refused
/// (std::from_chars: no exceptions and no locale, unlike std::stoull, which throws on
/// "abc" and wraps "-1").
std::optional<std::uint64_t> parse_u64(const std::string& text, int base = 10) {
    std::uint64_t v = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto [end, ec] = std::from_chars(first, last, v, base);
    if (text.empty() || ec != std::errc{} || end != last) {
        return std::nullopt;
    }
    return v;
}

std::vector<std::uint64_t> read_hashes(const std::string& path, bool& ok) {
    std::vector<std::uint64_t> out;
    std::ifstream in(path);
    ok = static_cast<bool>(in);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string token;
        if (ls >> token) {
            // A line whose first token is not wholly hex (a header) is skipped.
            if (const auto h = parse_u64(token, 16)) {
                out.push_back(*h);
            }
        }
    }
    return out;
}

int cmd_first_diff(const std::string& a, const std::string& b) {
    bool oka = false;
    bool okb = false;
    const std::vector<std::uint64_t> ha = read_hashes(a, oka);
    const std::vector<std::uint64_t> hb = read_hashes(b, okb);
    if (!oka || !okb) {
        std::cerr << "cannot read one of the hash files\n";
        return 2;
    }
    const auto d = sim::first_divergent_entry(ha, hb);
    if (!d) {
        std::cout << "none (the two runs agree)\n";
        return 0;
    }
    std::cout << "first divergent entry: " << *d << "\n";
    return 1;
}

int cmd_tree(const std::string& path, std::size_t entry, std::uint32_t depth,
             const std::string& dump_scope) {
    auto log = load_log(path);
    if (!log) {
        return 2;
    }
    auto world = build_world(*log);
    if (!world) {
        return 2;
    }
    if (!dump_scope.empty()) {
        // Drive the world to the requested entry, then dump the scope's fields.
        for (std::size_t i = 0; i < log->entries.size() && i <= entry; ++i) {
            const sim::ReplayEntry& e = log->entries[i];
            if (e.kind == sim::ReplayEntry::Kind::Advance) {
                world->advance(e.ms);
            } else if (e.kind == sim::ReplayEntry::Kind::Commands) {
                for (const sim::Command& c : e.commands) {
                    world->apply(c);
                }
            }
        }
        sim::PrintArchive print(dump_scope);
        fields(print, *world);
        std::cout << print.text();
        return 0;
    }
    const sim::HashTree tree = sim::tree_at(*world, *log, entry, depth);
    // One line per node: its full scope path (the form diff_trees returns and
    // --dump-scope takes), then its digest. A line diff of two runs' trees names the
    // diverging paths directly.
    struct Walk {
        static void go(const sim::HashNode& n, const std::string& prefix) {
            std::string path =
                prefix.empty() ? std::string(n.name) : prefix + "/" + std::string(n.name);
            if (n.indexed) {
                path += "[" + std::to_string(n.index) + "]";
            }
            std::cout << path << " " << hex64(n.hash) << "\n";
            for (const sim::HashNode& c : n.children) {
                go(c, path);
            }
        }
    };
    Walk::go(tree.root, {});
    return 0;
}

int cmd_golden(const std::string& path) {
    auto log = load_log(path);
    if (!log) {
        return 2;
    }
    auto world = build_world(*log);
    if (!world) {
        return 2;
    }
    // A deterministic fingerprint of the whole run: the frame versions, the digests,
    // the PRNG timeline, then the state hash after each entry. Stable across builds
    // and platforms (no floats, no pointers), so it is committed and diffed directly.
    std::cout << "replay_log_version " << sim::kReplayLogVersion << "\n";
    std::cout << "command_wire_version " << sim::kCommandWireVersion << "\n";
    std::cout << "config_digest " << hex64(log->config_digest) << "\n";
    std::cout << "rules_digest " << hex64(log->rules_digest) << "\n";
    std::cout << "registry_digest " << hex64(log->registry_digest) << "\n";
    std::cout << "seams_digest " << hex64(log->seams_digest) << "\n";
    std::cout << "prng_timeline_version " << log->prng_timeline_version << "\n";
    for (const std::string& m : log->prng_timeline_missing) {
        std::cout << "prng_timeline_missing " << m << "\n";
    }
    std::cout << "entries " << log->entries.size() << "\n";
    for (const std::uint64_t h : sim::hash_per_entry(*world, *log)) {
        std::cout << "hash " << hex64(h) << "\n";
    }
    return 0;
}

/// The island content of World::create on all cores (docs/sim/state.md §11 item 1:
/// load-time work per island in parallel, results in table order).
class ThreadExecutor final : public sim::Executor {
  public:
    void run(std::uint64_t n, Body body, void* ctx) const override {
        const std::uint64_t threads =
            std::min<std::uint64_t>(n, std::max(1U, std::thread::hardware_concurrency()));
        std::atomic<std::uint64_t> next{0};
        std::vector<std::thread> pool;
        for (std::uint64_t t = 0; t < threads; ++t) {
            pool.emplace_back([&] {
                for (std::uint64_t i = next++; i < n; i = next++) {
                    body(ctx, i);
                }
            });
        }
        for (std::thread& th : pool) {
            th.join();
        }
    }
};

/// The registry of the game directory (ANNO1503_GAME_DIR/Anlagen.dat), or the empty
/// one (then every placed item is an UnknownObject) with a note.
std::shared_ptr<const sim::TypeRegistry> scenario_registry() {
    const char* dir = std::getenv("ANNO1503_GAME_DIR");
    if (dir == nullptr || *dir == '\0') {
        std::cout << "registry empty (ANNO1503_GAME_DIR not set)\n";
        return sim::TypeRegistry::empty();
    }
    // FigurenHeader stays optional, with the same note, then any later failure.
    const anno::sim_load::OriginalRegistryLoad loaded = anno::sim_load::load_original_registry(
        dir, anno::sim_load::FigureEntries::OptionalNote);
    if (!loaded.figure_note.empty()) {
        std::cout << loaded.figure_note << "\n";
    }
    if (!loaded.error.empty()) {
        std::cout << "registry empty (" << loaded.error << ")\n";
        return sim::TypeRegistry::empty();
    }
    return loaded.types;
}

/// The island templates of the game directory (ANNO1503_GAME_DIR/Scenes/Islands), or
/// none (then every link stays unresolved) with a note.
anno::sim_load::IslandTemplates scenario_templates() {
    const char* dir = std::getenv("ANNO1503_GAME_DIR");
    if (dir == nullptr || *dir == '\0') {
        std::cout << "island templates none (ANNO1503_GAME_DIR not set)\n";
        return {};
    }
    anno::sim_load::IslandTemplates t = anno::sim_load::scan_island_templates(dir);
    std::cout << "island templates " << t.summaries.size() << "\n";
    return t;
}

int cmd_scenario(const std::string& path, std::uint64_t ticks, std::uint32_t step_ms) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "cannot read " << path << "\n";
        return 1;
    }
    const std::vector<std::uint8_t> buf{std::istreambuf_iterator<char>(in), {}};
    const auto types = scenario_registry();
    const anno::sim_load::IslandTemplates templates = scenario_templates();
    const sim::SimConfig cfg;
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    auto resolved = anno::sim_load::load_scenario_with_links(buf, templates, *types, cfg, {});
    const auto t1 = clock::now();
    if (!resolved) {
        std::cout << "refused: " << resolved.error() << "\n";
        return 1;
    }
    const sim::LoadedScenario& loaded = resolved.value().loaded;
    const sim::LinkResolution& res = resolved.value().resolution;
    // The links as the selection resolved them (docs/re/annoscene.md §5): the
    // template, the raw-material and ore kinds, the special flag, the treasure.
    for (std::size_t i = 0; i < loaded.links.size(); ++i) {
        const sim::IslandLinkSpec& l = loaded.links[i];
        const sim::ResolvedLink& r = res.links[i];
        std::cout << "link " << i << " island " << l.number << " at " << l.x << "," << l.y << " "
                  << l.w << "x" << l.h << " climate " << l.climate << " -> "
                  << (r.candidate ? templates.summaries[*r.candidate].name
                                  : std::string("unresolved"))
                  << " (" << r.kept << " of " << r.matched << " candidates) raw";
        for (std::size_t k = 0; k < r.raw_marks.size(); ++k) {
            if (r.raw_marks[k] != 0) {
                std::cout << " " << k;
            }
        }
        std::cout << " ores";
        for (std::size_t k = 0; k < r.ore_marks.size(); ++k) {
            if (r.ore_marks[k] != 0) {
                std::cout << " " << k;
            }
        }
        std::cout << " special " << static_cast<unsigned>(r.special) << " treasure "
                  << (r.treasure ? 1 : 0) << "\n";
    }
    if (!loaded.links.empty()) {
        std::cout << "link draws";
        for (std::size_t s = 0; s < sim::kDrawSiteCount; ++s) {
            if (res.draws[s] != 0) {
                std::cout << " " << sim::draw_site_name(static_cast<sim::DrawSite>(s)) << " "
                          << res.draws[s];
            }
        }
        std::cout << "; cursor " << resolved.value().seed_before << " -> " << loaded.spec.seed
                  << "\n";
    }
    const sim::ScenarioCounts c = sim::count(loaded);
    std::cout << "islands " << c.islands << " links " << c.links << " links_resolved "
              << c.links_resolved << " links_unresolved " << c.links_unresolved << " players "
              << c.players << " cities " << c.cities << " buildings " << c.buildings
              << " tile_objects " << c.tile_objects << " deposits " << c.deposits << "\n";
    for (const sim::ScenarioWarningKind k : sim::all_warning_kinds()) {
        const auto it = c.warnings.find(k);
        std::cout << "warnings " << sim::warning_kind_name(k) << " "
                  << (it == c.warnings.end() ? 0 : it->second) << "\n";
    }
    const ThreadExecutor ex;
    auto w = sim::World::create(loaded.spec, types, cfg, {}, {}, ex);
    const auto t2 = clock::now();
    if (!w) {
        std::cout << "create refused: " << w.error() << "\n";
        return 1;
    }
    sim::World world = std::move(w).value();
    std::cout << "hash after create " << hex64(world.state_hash()) << "\n";
    // Advances of step_ms until the tick reaches `ticks` (a tick is 2 ms).
    const std::uint64_t target = world.clock().tick() + ticks;
    while (world.clock().tick() < target) {
        world.advance(step_ms);
    }
    const auto t3 = clock::now();
    std::cout << "hash after " << world.clock().tick() << " ticks " << hex64(world.state_hash())
              << "\n";
    const std::string problem = world.check_invariants();
    std::cout << "check_invariants " << (problem.empty() ? std::string("clean") : problem) << "\n";
    auto ms = [](auto d) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
    };
    std::cout << "time load " << ms(t1 - t0) << " ms, create " << ms(t2 - t1) << " ms, run "
              << ms(t3 - t2) << " ms\n";
    return problem.empty() ? 0 : 1;
}

/// One sample at the lower and upper middle, averaged, so an even count stays an
/// integer microsecond. p95 is the nearest rank: the sample at ceil(0.95 * n).
struct UsecStats {
    std::uint64_t min = 0;
    std::uint64_t median = 0;
    std::uint64_t p95 = 0;
    std::uint64_t max = 0;
};

UsecStats usec_stats(std::vector<std::uint64_t> samples) {
    std::sort(samples.begin(), samples.end());
    const std::size_t n = samples.size();
    UsecStats s;
    if (n == 0) {
        return s;
    }
    s.min = samples.front();
    s.max = samples.back();
    if (n % 2 == 0) {
        s.median = (samples[n / 2 - 1] + samples[n / 2]) / 2;
    } else {
        s.median = samples[n / 2];
    }
    const std::size_t rank = static_cast<std::size_t>((n * 95 + 99) / 100);
    s.p95 = samples[rank - 1];
    return s;
}

/// Same object id, so the saved bytes do not change. set_object still marks that
/// chunk dirty (island.cpp). state_hash() is const and does not clear the bit
/// (world.hpp; the const chunk_digest recomputes a dirty chunk on every call),
/// so one write then R hashes are all incremental.
void dirty_one_tile_per_island(sim::World& world) {
    for (const auto& slot : world.islands()) {
        if (!slot || slot->tiles.tile_count() == 0) {
            continue;
        }
        sim::Island* live = world.island_at(slot->tiles.x0(), slot->tiles.y0());
        if (live == nullptr) {
            continue;
        }
        const sim::Slot idx = live->tiles.index_of(live->tiles.x0(), live->tiles.y0());
        if (idx == sim::kNoSlot) {
            continue;
        }
        live->tiles.set_object(idx, live->tiles.object(idx));
    }
}

UsecStats time_state_hash(const sim::World& world, std::uint64_t reps) {
    std::vector<std::uint64_t> samples;
    samples.reserve(static_cast<std::size_t>(reps));
    volatile std::uint64_t sink = 0;
    for (std::uint64_t i = 0; i < reps; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        const std::uint64_t hash = world.state_hash();
        const auto t1 = std::chrono::steady_clock::now();
        sink = hash;
        samples.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()));
    }
    // A volatile store-then-load keeps the hashes in the timed region under LTO.
    // The load is the use: a store alone is -Wunused-but-set-variable.
    static volatile std::uint64_t published = 0;
    published = sink;
    const std::uint64_t kept = published;
    if (kept != published) {
        samples.push_back(kept);
    }
    return usec_stats(std::move(samples));
}

std::uint64_t count_tiles(const sim::World& world) {
    std::uint64_t n = 0;
    for (const auto& slot : world.islands()) {
        if (slot) {
            n += slot->tiles.tile_count();
        }
    }
    return n;
}

std::uint64_t count_chunks(const sim::World& world) {
    std::uint64_t n = 0;
    for (const auto& slot : world.islands()) {
        if (slot) {
            n += slot->tiles.chunk_count();
        }
    }
    return n;
}

std::uint64_t count_dirty(const sim::World& world) {
    std::uint64_t n = 0;
    for (const auto& slot : world.islands()) {
        if (slot) {
            n += slot->tiles.dirty_chunks();
        }
    }
    return n;
}

int cmd_measure(const std::string& path, std::uint64_t ticks, std::uint64_t reps) {
    // `synthetic` is the code-built two-island world (synthetic.hpp). It needs no
    // game files, so the tool test can require every field name. A path is a .sze.
    std::shared_ptr<const sim::TypeRegistry> types;
    sim::ScenarioSpec spec;
    if (path == "synthetic") {
        types = sim::TypeRegistry::empty();
        spec = sim::synthetic::two_island_scenario();
    } else {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            std::cerr << "cannot read " << path << "\n";
            return 1;
        }
        const std::vector<std::uint8_t> buf{std::istreambuf_iterator<char>(in), {}};
        types = scenario_registry();
        const anno::sim_load::IslandTemplates templates = scenario_templates();
        const sim::SimConfig load_cfg;
        auto resolved =
            anno::sim_load::load_scenario_with_links(buf, templates, *types, load_cfg, {});
        if (!resolved) {
            std::cout << "refused: " << resolved.error() << "\n";
            return 1;
        }
        spec = std::move(resolved.value().loaded.spec);
    }
    const sim::SimConfig cfg;
    const ThreadExecutor ex;
    auto w = sim::World::create(spec, types, cfg, {}, {}, ex);
    if (!w) {
        std::cout << "create refused: " << w.error() << "\n";
        return 1;
    }
    sim::World world = std::move(w).value();
    // The same step as `scenario`'s default: 100 ms per advance, and a tick is 2 ms.
    constexpr std::uint32_t kStepMs = 100;
    const std::uint64_t target = world.clock().tick() + ticks;
    while (world.clock().tick() < target) {
        world.advance(kStepMs);
    }
    // Create marks every chunk dirty, and the tick never calls prepare_hash.
    // state_hash() is const, so it recomputes those chunks on every call and
    // does not clear them (world.hpp). That is the check with no prepare.
    const std::uint64_t dirty_before = count_dirty(world);
    const UsecStats unprepared = time_state_hash(world, reps);
    // Clean: prepare_hash fills every chunk digest and clears the dirty bits.
    // The timed calls are const and only read those digests.
    world.prepare_hash(ex);
    const UsecStats clean = time_state_hash(world, reps);
    dirty_one_tile_per_island(world);
    const std::uint64_t dirty = count_dirty(world);
    const UsecStats incremental = time_state_hash(world, reps);
    const std::uint64_t save_bytes = world.save().size();
    std::cout << "measure ticks=" << world.clock().tick() << " reps=" << reps
              << " hash_unprepared_us min=" << unprepared.min << " median=" << unprepared.median
              << " p95=" << unprepared.p95 << " max=" << unprepared.max
              << " dirty_chunks_unprepared=" << dirty_before << " hash_clean_us min=" << clean.min
              << " median=" << clean.median << " p95=" << clean.p95 << " max=" << clean.max
              << " hash_incremental_us min=" << incremental.min << " median=" << incremental.median
              << " p95=" << incremental.p95 << " max=" << incremental.max
              << " save_bytes=" << save_bytes << " tiles=" << count_tiles(world)
              << " chunks=" << count_chunks(world) << " dirty_chunks=" << dirty << "\n";
    const std::string problem = world.check_invariants();
    if (!problem.empty()) {
        std::cout << "check_invariants " << problem << "\n";
        return 1;
    }
    return 0;
}

/// ADR 0003 §3 / §13 comparison lines. Not a sim default and not a packet this
/// tool writes. An encoded turn is only the back-to-back TurnEntry stream.
constexpr std::size_t kAdrPayloadMax = 1200;
constexpr std::size_t kAdrHeaderBytes = 25;
constexpr std::size_t kAdrIpv4UdpBytes = 28;
constexpr std::size_t kAdrEmptyPayloadBytes = 16;
/// Multiplayer turn at 100% (clock.hpp Timing, ADR 0003 §6): 10 steps of 10
/// game ms. One tick is 2 game ms, so the turn is 50 ticks. The age loop below
/// is a different call: World::advance(100), the same one measure uses.
constexpr std::uint32_t kMpStepMs = 10;
constexpr std::uint32_t kMpSteps = 10;
constexpr std::uint32_t kAgeStepMs = 100;
constexpr std::uint32_t kTickGameMs = 2;

const char* compiler_name() {
#if defined(_MSC_VER)
    return "msvc";
#elif defined(__clang__)
    return "clang";
#elif defined(__GNUC__)
    return "gcc";
#else
    return "unknown";
#endif
}

template <class F> std::uint64_t time_call_us(F&& fn) {
    const auto t0 = std::chrono::steady_clock::now();
    std::forward<F>(fn)();
    const auto t1 = std::chrono::steady_clock::now();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
}

std::uint64_t floor_per_s(std::uint64_t us) {
    if (us == 0) {
        return 0;
    }
    return 1000000ULL / us;
}

std::uint64_t ceil_ms(std::uint64_t us) {
    return (us + 999ULL) / 1000ULL;
}

std::string one_line(std::string text) {
    for (char& c : text) {
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return text;
}

int fail_join(const std::string& reason) {
    // No timing line. A failed check must not be quotable as a measurement.
    std::cout << "join validation=fail reason=" << one_line(reason) << "\n";
    return 1;
}

sim::Command speed_command(std::uint32_t percent) {
    sim::Command command;
    command.issuer = sim::PlayerId{0};
    command.body = sim::SetSpeedCmd{percent};
    return command;
}

std::uint32_t scripted_percent(std::uint64_t index) {
    static constexpr std::uint32_t kSpeeds[3] = {50, 100, 200};
    return kSpeeds[static_cast<std::size_t>(index % 3)];
}

struct EncodedTurn {
    std::vector<std::uint8_t> bytes;
    std::string error;
};

EncodedTurn encode_commands(const std::vector<sim::Command>& commands) {
    EncodedTurn out;
    for (std::size_t i = 0; i < commands.size(); ++i) {
        sim::TurnEntry entry;
        entry.command = commands[i];
        entry.seq = static_cast<std::uint32_t>(i);
        auto encoded = sim::encode_turn_entry(entry);
        if (!encoded.ok()) {
            out.error = encoded.error();
            out.bytes.clear();
            return out;
        }
        const std::vector<std::uint8_t>& piece = encoded.value();
        out.bytes.insert(out.bytes.end(), piece.begin(), piece.end());
    }
    return out;
}

struct Prepared {
    std::shared_ptr<const sim::TypeRegistry> types;
    std::vector<std::uint8_t> save;
    std::uint64_t source_hash = 0;
    std::uint64_t start_tick = 0;
    std::uint64_t age_tick = 0;
    std::uint64_t age_advances = 0;
    std::uint64_t io_us = 0;
    std::uint64_t registry_us = 0;
    std::uint64_t templates_us = 0;
    std::uint64_t parse_us = 0;
    std::uint64_t create_age_us = 0;
    std::uint64_t save_generation_us = 0;
    int file_io = 0;
};

struct LoadOutcome {
    std::optional<sim::World> world;
    std::string error;
};

LoadOutcome load_save(const std::vector<std::uint8_t>& bytes,
                      const std::shared_ptr<const sim::TypeRegistry>& types) {
    LoadOutcome out;
    auto loaded = sim::World::load(bytes, types, {});
    if (!loaded.ok()) {
        out.error = loaded.error();
        return out;
    }
    out.world = std::move(loaded).value();
    return out;
}

int prepare_join(const std::string& path, std::uint64_t ticks, Prepared& out) {
    sim::ScenarioSpec spec;
    if (path == "synthetic") {
        out.file_io = 0;
        out.io_us = 0;
        out.registry_us = time_call_us([&] { out.types = sim::TypeRegistry::empty(); });
        out.templates_us = 0;
        out.parse_us = time_call_us([&] { spec = sim::synthetic::two_island_scenario(); });
    } else {
        out.file_io = 1;
        std::vector<std::uint8_t> buf;
        bool opened = false;
        out.io_us = time_call_us([&] {
            std::ifstream in(path, std::ios::binary);
            opened = static_cast<bool>(in);
            if (opened) {
                buf.assign(std::istreambuf_iterator<char>(in), {});
            }
        });
        if (!opened) {
            std::cerr << "cannot read " << path << "\n";
            return 1;
        }
        out.registry_us = time_call_us([&] { out.types = scenario_registry(); });
        anno::sim_load::IslandTemplates templates;
        out.templates_us = time_call_us([&] { templates = scenario_templates(); });
        const sim::SimConfig load_cfg;
        bool parsed = false;
        std::string parse_error;
        out.parse_us = time_call_us([&] {
            auto resolved =
                anno::sim_load::load_scenario_with_links(buf, templates, *out.types, load_cfg, {});
            if (!resolved) {
                parse_error = resolved.error();
                return;
            }
            spec = std::move(resolved.value().loaded.spec);
            parsed = true;
        });
        if (!parsed) {
            return fail_join("refused: " + parse_error);
        }
    }
    const sim::SimConfig cfg;
    const ThreadExecutor ex;
    std::optional<sim::World> created;
    std::string create_error;
    out.create_age_us = time_call_us([&] {
        auto made = sim::World::create(spec, out.types, cfg, {}, {}, ex);
        if (!made) {
            create_error = made.error();
            return;
        }
        sim::World world = std::move(made).value();
        out.start_tick = world.clock().tick();
        const std::uint64_t target = out.start_tick + ticks;
        while (world.clock().tick() < target) {
            world.advance(kAgeStepMs);
            ++out.age_advances;
        }
        out.age_tick = world.clock().tick();
        created = std::move(world);
    });
    if (!created) {
        return fail_join("create refused: " + create_error);
    }
    out.save_generation_us = time_call_us([&] { out.save = created->save(); });
    out.source_hash = created->state_hash();
    return 0;
}

struct ApplyTally {
    std::uint64_t accepted = 0;
    std::uint64_t rejected = 0;
    std::uint64_t speed_ok = 0;
    std::uint64_t speed_bad = 0;
};

void tally_apply(sim::World& world, const sim::Command& command, ApplyTally& tally) {
    const sim::ApplyResult result = world.apply(command);
    if (!result.ok()) {
        ++tally.rejected;
        return;
    }
    ++tally.accepted;
    if (const auto* speed = std::get_if<sim::SetSpeedCmd>(&command.body)) {
        if (world.settings().speed_percent == speed->percent) {
            ++tally.speed_ok;
        } else {
            ++tally.speed_bad;
        }
    }
}

void advance_turn_steps(sim::World& world) {
    for (std::uint32_t step = 0; step < kMpSteps; ++step) {
        world.advance(kMpStepMs);
    }
}

void run_script_turn(sim::World& world, const std::vector<sim::Command>& commands,
                     std::uint64_t index) {
    sim::Turn turn;
    turn.index = index;
    turn.step_ms = kMpStepMs;
    turn.commands = commands;
    sim::run_turn(world, nullptr, turn, kMpSteps);
}

struct WorkloadResult {
    std::string name;
    UsecStats times;
    UsecStats payload;
    std::uint64_t samples = 0;
    std::uint64_t duration_us = 0;
    std::uint64_t commands_per_turn = 0;
    std::uint64_t container_capacity = 0;
    std::uint64_t accepted = 0;
    std::uint64_t rejected = 0;
    std::uint64_t speed_ok = 0;
    std::uint64_t speed_bad = 0;
    int over_cap = 0;
    std::string error;
};

std::vector<std::vector<sim::Command>> script_turns(const std::string& name, std::uint64_t catchup,
                                                    std::string& error) {
    std::vector<std::vector<sim::Command>> turns;
    if (name == "empty") {
        turns.assign(static_cast<std::size_t>(catchup), {});
        return turns;
    }
    if (name == "scripted") {
        turns.reserve(static_cast<std::size_t>(catchup));
        for (std::uint64_t i = 0; i < catchup; ++i) {
            turns.push_back({speed_command(scripted_percent(i))});
        }
        return turns;
    }
    // One synthetic turn whose encoded TurnEntry stream is past the ADR cap.
    // The count is however many records the wire needs. It is not a play session.
    std::vector<sim::Command> burst;
    for (;;) {
        const std::uint32_t percent = (burst.size() % 2 == 0) ? 50U : 100U;
        std::vector<sim::Command> trial = burst;
        trial.push_back(speed_command(percent));
        EncodedTurn encoded = encode_commands(trial);
        if (!encoded.error.empty()) {
            error = encoded.error;
            return {};
        }
        burst.push_back(speed_command(percent));
        if (encoded.bytes.size() > kAdrPayloadMax) {
            break;
        }
        // A record that encodes to nothing would not move the size. Stop rather
        // than loop. The SetSpeed record is longer than the cap long before this.
        if (burst.size() > 10000) {
            error = "burst did not pass the payload cap";
            return {};
        }
    }
    turns.push_back(std::move(burst));
    return turns;
}

int check_script(const Prepared& prepared, const std::vector<std::vector<sim::Command>>& turns,
                 ApplyTally& tally) {
    LoadOutcome via_run = load_save(prepared.save, prepared.types);
    if (!via_run.world) {
        return fail_join("reference load: " + via_run.error);
    }
    for (std::size_t i = 0; i < turns.size(); ++i) {
        run_script_turn(*via_run.world, turns[i], static_cast<std::uint64_t>(i));
    }
    const std::uint64_t run_hash = via_run.world->state_hash();
    via_run.world.reset();

    LoadOutcome via_apply = load_save(prepared.save, prepared.types);
    if (!via_apply.world) {
        return fail_join("apply reference load: " + via_apply.error);
    }
    for (const std::vector<sim::Command>& turn : turns) {
        for (const sim::Command& command : turn) {
            tally_apply(*via_apply.world, command, tally);
        }
        advance_turn_steps(*via_apply.world);
    }
    const std::uint64_t apply_hash = via_apply.world->state_hash();
    if (apply_hash != run_hash) {
        return fail_join("run_turn hash " + hex64(run_hash) + " differs from checked apply " +
                         hex64(apply_hash));
    }
    return 0;
}

int measure_replay(const Prepared& prepared, const std::string& name, std::uint64_t catchup,
                   std::uint64_t warmup, std::uint64_t reps, WorkloadResult& out) {
    out.name = name;
    std::string script_error;
    const std::uint64_t turn_count = name == "burst" ? 1 : catchup;
    const std::vector<std::vector<sim::Command>> turns =
        script_turns(name, turn_count, script_error);
    if (!script_error.empty()) {
        return fail_join(script_error);
    }
    if (turns.empty()) {
        return fail_join("workload " + name + " has no turns");
    }
    std::vector<std::uint64_t> sizes;
    sizes.reserve(turns.size());
    for (const std::vector<sim::Command>& turn : turns) {
        EncodedTurn encoded = encode_commands(turn);
        if (!encoded.error.empty()) {
            return fail_join(encoded.error);
        }
        sizes.push_back(static_cast<std::uint64_t>(encoded.bytes.size()));
        if (encoded.bytes.capacity() > out.container_capacity) {
            out.container_capacity = static_cast<std::uint64_t>(encoded.bytes.capacity());
        }
    }
    out.payload = usec_stats(std::move(sizes));
    out.commands_per_turn = static_cast<std::uint64_t>(turns.front().size());
    out.over_cap = out.payload.max > kAdrPayloadMax ? 1 : 0;

    ApplyTally tally;
    if (const int failed = check_script(prepared, turns, tally)) {
        return failed;
    }
    out.accepted = tally.accepted;
    out.rejected = tally.rejected;
    out.speed_ok = tally.speed_ok;
    out.speed_bad = tally.speed_bad;
    if (tally.rejected != 0 || tally.speed_bad != 0) {
        return fail_join(name + " commands rejected=" + std::to_string(tally.rejected) +
                         " speed_bad=" + std::to_string(tally.speed_bad));
    }
    if (name != "empty" && tally.accepted == 0) {
        return fail_join(name + " accepted no commands");
    }

    LoadOutcome reference = load_save(prepared.save, prepared.types);
    if (!reference.world) {
        return fail_join("replay reference load: " + reference.error);
    }
    for (std::size_t i = 0; i < turns.size(); ++i) {
        run_script_turn(*reference.world, turns[i], static_cast<std::uint64_t>(i));
    }
    const std::uint64_t reference_hash = reference.world->state_hash();
    reference.world.reset();

    std::vector<std::uint64_t> samples;
    samples.reserve(static_cast<std::size_t>(reps * turns.size()));
    for (std::uint64_t rep = 0; rep < warmup + reps; ++rep) {
        const bool keep = rep >= warmup;
        LoadOutcome loaded = load_save(prepared.save, prepared.types);
        if (!loaded.world) {
            return fail_join("replay load: " + loaded.error);
        }
        for (std::size_t i = 0; i < turns.size(); ++i) {
            const std::uint64_t us = time_call_us(
                [&] { run_script_turn(*loaded.world, turns[i], static_cast<std::uint64_t>(i)); });
            if (keep) {
                samples.push_back(us);
            }
        }
        // The hash is outside the timer. A mismatch drops every sample.
        if (loaded.world->state_hash() != reference_hash) {
            return fail_join(name + " replay hash mismatch expected " + hex64(reference_hash) +
                             " got " + hex64(loaded.world->state_hash()));
        }
    }
    out.samples = static_cast<std::uint64_t>(samples.size());
    out.duration_us = 0;
    for (const std::uint64_t us : samples) {
        out.duration_us += us;
    }
    out.times = usec_stats(std::move(samples));
    return 0;
}

void print_span(const char* label, const UsecStats& stats) {
    std::cout << label << " min=" << stats.min << " median=" << stats.median << " p95=" << stats.p95
              << " max=" << stats.max;
}

std::uint64_t per_s_from_sum(std::uint64_t samples, std::uint64_t duration_us) {
    if (duration_us == 0) {
        return 0;
    }
    return samples * 1000000ULL / duration_us;
}

int cmd_join(const std::string& path, std::uint64_t ticks, std::uint64_t warmup, std::uint64_t reps,
             std::uint64_t catchup, std::uint64_t replay_reps) {
    Prepared prepared;
    if (const int failed = prepare_join(path, ticks, prepared)) {
        return failed;
    }
    LoadOutcome checked = load_save(prepared.save, prepared.types);
    if (!checked.world) {
        return fail_join("load: " + checked.error);
    }
    if (checked.world->state_hash() != prepared.source_hash) {
        return fail_join("load hash mismatch expected " + hex64(prepared.source_hash) + " got " +
                         hex64(checked.world->state_hash()));
    }
    checked.world.reset();
    std::vector<std::uint64_t> load_samples;
    load_samples.reserve(static_cast<std::size_t>(reps));
    for (std::uint64_t rep = 0; rep < warmup + reps; ++rep) {
        LoadOutcome loaded;
        const std::uint64_t us =
            time_call_us([&] { loaded = load_save(prepared.save, prepared.types); });
        if (!loaded.world) {
            return fail_join("load: " + loaded.error);
        }
        // Hash after the timer. One mismatch invalidates the series.
        if (loaded.world->state_hash() != prepared.source_hash) {
            return fail_join("load hash mismatch expected " + hex64(prepared.source_hash) +
                             " got " + hex64(loaded.world->state_hash()));
        }
        if (rep >= warmup) {
            load_samples.push_back(us);
        }
    }
    const UsecStats load_times = usec_stats(std::move(load_samples));

    WorkloadResult empty;
    WorkloadResult scripted;
    WorkloadResult burst;
    if (const int failed = measure_replay(prepared, "empty", catchup, warmup, replay_reps, empty)) {
        return failed;
    }
    if (const int failed =
            measure_replay(prepared, "scripted", catchup, warmup, replay_reps, scripted)) {
        return failed;
    }
    if (const int failed = measure_replay(prepared, "burst", catchup, warmup, replay_reps, burst)) {
        return failed;
    }

    const unsigned threads = std::thread::hardware_concurrency();
    const std::uint32_t ticks_per_turn = (kMpStepMs * kMpSteps) / kTickGameMs;
    std::cout << "join validation=ok path=" << path << "\n";
    std::cout << "join age_tick=" << prepared.age_tick << " start_tick=" << prepared.start_tick
              << " ticks_advanced=" << (prepared.age_tick - prepared.start_tick)
              << " state_version=" << sim::kStateHashVersion
              << " save_version=" << sim::kSaveVersion
              << " wire_version=" << sim::kCommandWireVersion << "\n";
    std::cout << "join build compiler=" << compiler_name()
#if defined(_MSC_VER)
              << " msvc_ver=" << _MSC_VER
#endif
              << " ndebug="
#if defined(NDEBUG)
              << "1"
#else
              << "0"
#endif
              << " threads=" << threads
              << " create_executor=threads load_executor=none clock=steady_clock"
              << " timing_includes_hash=0\n";
    std::cout << "join phases samples=1 file_io=" << prepared.file_io << " io_us=" << prepared.io_us
              << " registry_us=" << prepared.registry_us
              << " templates_us=" << prepared.templates_us << " parse_us=" << prepared.parse_us
              << " create_age_us=" << prepared.create_age_us
              << " save_generation_us=" << prepared.save_generation_us
              << " save_bytes=" << prepared.save.size() << "\n";
    std::cout << "join load warmup=" << warmup << " reps=" << reps << " ";
    print_span("load_us", load_times);
    std::cout << " load_ms_ceil_median=" << ceil_ms(load_times.median) << "\n";
    std::cout << "join mapping tick_game_ms=" << kTickGameMs
              << " age_method=advance age_step_ms=" << kAgeStepMs
              << " age_advances=" << prepared.age_advances
              << " replay_method=run_turn step_ms=" << kMpStepMs << " steps=" << kMpSteps
              << " ticks_per_turn=" << ticks_per_turn << "\n";

    const WorkloadResult* rows[] = {&empty, &scripted, &burst};
    std::uint64_t rejected = 0;
    std::uint64_t speed_bad = 0;
    for (const WorkloadResult* row : rows) {
        rejected += row->rejected;
        speed_bad += row->speed_bad;
        std::cout << "join replay workload=" << row->name << " warmup=" << warmup
                  << " reps=" << replay_reps << " turns=" << (row->name == "burst" ? 1 : catchup)
                  << " samples=" << row->samples << " ";
        print_span("run_turn_us", row->times);
        std::cout << " duration_us=" << row->duration_us
                  << " throughput_turns_per_s_floor_median=" << floor_per_s(row->times.median)
                  << " throughput_turns_per_s_from_sum="
                  << per_s_from_sum(row->samples, row->duration_us)
                  << " commands_per_turn=" << row->commands_per_turn
                  << " commands_accepted=" << row->accepted
                  << " commands_rejected=" << row->rejected << " speed_ok=" << row->speed_ok
                  << " speed_bad=" << row->speed_bad << "\n";
        std::cout << "join encoded workload=" << row->name << " ";
        print_span("payload_bytes", row->payload);
        std::cout << " commands_per_turn=" << row->commands_per_turn
                  << " container_capacity_bytes=" << row->container_capacity
                  << " adr_header_bytes=" << kAdrHeaderBytes
                  << " adr_ipv4_udp_bytes=" << kAdrIpv4UdpBytes
                  << " adr_empty_payload_bytes=" << kAdrEmptyPayloadBytes
                  << " adr_empty_datagram_bytes="
                  << (kAdrHeaderBytes + kAdrEmptyPayloadBytes + kAdrIpv4UdpBytes)
                  << " packet_overhead=adr_proposal not_a_packet=1 cap_payload_max="
                  << kAdrPayloadMax << " over_cap=" << row->over_cap << "\n";
    }
    std::cout << "join_ok validation=ok replay_method=run_turn age_method=advance"
              << " ticks_per_turn=" << ticks_per_turn << " commands_rejected=" << rejected
              << " speed_bad=" << speed_bad << " burst_over_cap=" << burst.over_cap
              << " empty_payload_bytes=" << empty.payload.max
              << " scripted_commands=" << scripted.commands_per_turn
              << " state_version=" << sim::kStateHashVersion
              << " save_bytes=" << prepared.save.size() << "\n";
    return 0;
}

void usage() {
    std::cerr << "usage: anno-sim-replay <run|hashes|first-diff|tree|golden|scenario|measure|join> "
                 "...\n"
                 "  run <log>\n"
                 "  hashes <log>\n"
                 "  first-diff <a> <b>\n"
                 "  tree <log> [--entry N] [--depth D] [--dump-scope PATH]\n"
                 "  golden <log>\n"
                 "  scenario <file.sze> [--ticks N] [--step-ms M]   (the registry of "
                 "ANNO1503_GAME_DIR)\n"
                 "  measure <file.sze|synthetic> [--ticks N] [--reps R]\n"
                 "      N default 30000, R default 50; hash times in us, clean then\n"
                 "      one dirty chunk per island. synthetic needs no game files.\n"
                 "  join <file.sze|synthetic> [--ticks N] [--reps R] [--warmup W]\n"
                 "      [--catchup T] [--replay-reps N]\n"
                 "      Times World::load of an in-memory save, then run_turn.\n"
                 "      N ticks default 30000, R and replay reps default 5, warmup 1,\n"
                 "      catch-up turns 20. Age is advance(100). A turn is 10x10 ms.\n"
                 "      synthetic needs no game files. A failed check prints no times.\n";
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        usage();
        return 2;
    }
    const std::string& sub = args[0];
    if (sub == "run" && args.size() == 2) {
        return cmd_run(args[1]);
    }
    if (sub == "hashes" && args.size() == 2) {
        return cmd_hashes(args[1]);
    }
    if (sub == "first-diff" && args.size() == 3) {
        return cmd_first_diff(args[1], args[2]);
    }
    if (sub == "golden" && args.size() == 2) {
        return cmd_golden(args[1]);
    }
    if (sub == "tree" && args.size() >= 2) {
        std::size_t entry = ~std::size_t{0};
        std::uint32_t depth = 8;
        std::string dump_scope;
        for (std::size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "--entry" && i + 1 < args.size()) {
                // An entry beyond the log applies the whole log (tree_at), so any u64
                // is meaningful; on a 32-bit size_t a larger value saturates.
                const auto v = parse_u64(args[++i]);
                if (!v) {
                    std::cerr << "bad --entry value '" << args[i]
                              << "': expected a non-negative decimal integer\n";
                    usage();
                    return 2;
                }
                entry = static_cast<std::size_t>(
                    std::min<std::uint64_t>(*v, std::numeric_limits<std::size_t>::max()));
            } else if (args[i] == "--depth" && i + 1 < args.size()) {
                const auto v = parse_u64(args[++i]);
                if (!v || *v > std::numeric_limits<std::uint32_t>::max()) {
                    std::cerr << "bad --depth value '" << args[i]
                              << "': expected a decimal integer from 0 to 4294967295\n";
                    usage();
                    return 2;
                }
                depth = static_cast<std::uint32_t>(*v);
            } else if (args[i] == "--dump-scope" && i + 1 < args.size()) {
                dump_scope = args[++i];
            } else {
                usage();
                return 2;
            }
        }
        return cmd_tree(args[1], entry, depth, dump_scope);
    }
    if (sub == "scenario" && args.size() >= 2) {
        std::uint64_t ticks = 0;
        std::uint32_t step_ms = 100;
        for (std::size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "--ticks" && i + 1 < args.size()) {
                const auto v = parse_u64(args[++i]);
                if (!v) {
                    std::cerr << "bad --ticks value '" << args[i]
                              << "': expected a non-negative decimal integer\n";
                    usage();
                    return 2;
                }
                ticks = *v;
            } else if (args[i] == "--step-ms" && i + 1 < args.size()) {
                const auto v = parse_u64(args[++i]);
                if (!v || *v == 0 || *v > std::numeric_limits<std::uint32_t>::max()) {
                    std::cerr << "bad --step-ms value '" << args[i]
                              << "': expected a decimal integer from 1 to 4294967295\n";
                    usage();
                    return 2;
                }
                step_ms = static_cast<std::uint32_t>(*v);
            } else {
                usage();
                return 2;
            }
        }
        return cmd_scenario(args[1], ticks, step_ms);
    }
    if (sub == "measure" && args.size() >= 2) {
        std::uint64_t ticks = 30000;
        std::uint64_t reps = 50;
        for (std::size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "--ticks" && i + 1 < args.size()) {
                const auto v = parse_u64(args[++i]);
                if (!v) {
                    std::cerr << "bad --ticks value '" << args[i]
                              << "': expected a non-negative decimal integer\n";
                    usage();
                    return 2;
                }
                ticks = *v;
            } else if (args[i] == "--reps" && i + 1 < args.size()) {
                const auto v = parse_u64(args[++i]);
                if (!v || *v == 0) {
                    std::cerr << "bad --reps value '" << args[i]
                              << "': expected a decimal integer from 1\n";
                    usage();
                    return 2;
                }
                reps = *v;
            } else {
                usage();
                return 2;
            }
        }
        return cmd_measure(args[1], ticks, reps);
    }
    if (sub == "join" && args.size() >= 2) {
        std::uint64_t ticks = 30000;
        std::uint64_t reps = 5;
        std::uint64_t warmup = 1;
        std::uint64_t catchup = 20;
        std::uint64_t replay_reps = 5;
        for (std::size_t i = 2; i < args.size(); ++i) {
            const auto take = [&](const char* flag, std::uint64_t& dest, bool allow_zero) -> int {
                if (args[i] != flag) {
                    return 0;
                }
                if (i + 1 >= args.size()) {
                    usage();
                    return 2;
                }
                const auto v = parse_u64(args[++i]);
                if (!v || (!allow_zero && *v == 0)) {
                    std::cerr << "bad " << flag << " value '" << args[i] << "': expected a decimal "
                              << (allow_zero ? "integer from 0\n" : "integer from 1\n");
                    usage();
                    return 2;
                }
                dest = *v;
                return 1;
            };
            int taken = 0;
            if (args[i] == "--ticks") {
                taken = take("--ticks", ticks, true);
            } else if (args[i] == "--reps") {
                taken = take("--reps", reps, false);
            } else if (args[i] == "--warmup") {
                taken = take("--warmup", warmup, true);
            } else if (args[i] == "--catchup") {
                taken = take("--catchup", catchup, false);
            } else if (args[i] == "--replay-reps") {
                taken = take("--replay-reps", replay_reps, false);
            } else {
                usage();
                return 2;
            }
            if (taken == 2) {
                return 2;
            }
        }
        return cmd_join(args[1], ticks, warmup, reps, catchup, replay_reps);
    }
    usage();
    return 2;
}
