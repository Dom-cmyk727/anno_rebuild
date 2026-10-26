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
#include <vector>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/annowelt.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
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
    const std::filesystem::path p = std::filesystem::path(dir) / "Anlagen.dat";
    std::ifstream in(p, std::ios::binary);
    const std::vector<std::uint8_t> buf{std::istreambuf_iterator<char>(in), {}};
    auto file = anno::formats::anlagen::AnlagenFile::parse(buf);
    if (!file) {
        std::cout << "registry empty (cannot read " << p.string() << ")\n";
        return sim::TypeRegistry::empty();
    }
    // The figure types (T24): FigurenHeader.dat next to Anlagen.dat; without it the
    // registry has none and no cart moves. The entries view `fbuf`, which therefore
    // lives until the registry is built.
    std::vector<anno::formats::figuren::Entry> figure_entries;
    const std::filesystem::path fp = std::filesystem::path(dir) / "FigurenHeader.dat";
    std::ifstream fin(fp, std::ios::binary);
    const std::vector<std::uint8_t> fbuf{std::istreambuf_iterator<char>(fin), {}};
    auto header = anno::formats::figuren::FigurenHeader::parse(fbuf);
    if (header) {
        figure_entries = std::move(header.value().entries);
    } else {
        std::cout << "no figure types (cannot read " << fp.string() << ")\n";
    }
    std::ifstream win(std::filesystem::path(dir) / "AnnoWelt.dll", std::ios::binary);
    const std::vector<std::uint8_t> wbuf{std::istreambuf_iterator<char>(win), {}};
    auto walk = anno::formats::annowelt::read_walk_tables(wbuf);
    if (!walk) {
        std::cout << "registry empty (AnnoWelt.dll: "
                  << anno::formats::annowelt::error_message(walk.error()) << ")\n";
        return sim::TypeRegistry::empty();
    }
    auto r = sim::TypeRegistry::build(
        file.value().entries, sim::original_goods(), sim::SimConfig{}, figure_entries,
        std::make_shared<const anno::formats::annowelt::WalkTables>(std::move(walk).value()));
    if (!r) {
        std::cout << "registry empty (" << r.error() << ")\n";
        return sim::TypeRegistry::empty();
    }
    return r.value();
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

void usage() {
    std::cerr << "usage: anno-sim-replay <run|hashes|first-diff|tree|golden|scenario|measure> "
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
                 "      one dirty chunk per island. synthetic needs no game files.\n";
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
    usage();
    return 2;
}
