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
//
// The log carries the registry's digest, not the registry itself, so a world is
// rebuilt from the log's initial save on the empty type registry (the original goods
// table, no building types): enough for the headless golden and the S9 tests. A log
// made against another registry reports a digest mismatch here (the full scenario
// path arrives with the game files, S14).
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <anno/sim/replay.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>

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

std::optional<sim::World> build_world(const sim::ReplayLog& log) {
    auto w = sim::World::load(log.initial_save, sim::TypeRegistry::empty(), {});
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

void usage() {
    std::cerr << "usage: anno-sim-replay <run|hashes|first-diff|tree|golden> ...\n"
                 "  run <log>\n"
                 "  hashes <log>\n"
                 "  first-diff <a> <b>\n"
                 "  tree <log> [--entry N] [--depth D] [--dump-scope PATH]\n"
                 "  golden <log>\n";
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
    usage();
    return 2;
}
