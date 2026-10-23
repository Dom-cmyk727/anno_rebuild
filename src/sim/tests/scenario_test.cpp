// The scenario corpus (docs/sim/state.md §14.5, stage S14): every Scenes/**/*.sze of
// the game directory through the adapter, World::create, five simulated minutes and
// check_invariants; twice from the same spec, save/load/save. The counts per file are
// held against fixtures/synthetic/sim/scenario_corpus.txt (counts only, no content of
// the files). Needs ANNO1503_GAME_DIR; skipped with the one message otherwise. The
// output names files and counts only.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
#include <anno/test/game_dir.hpp>
#include <anno/test/parallel.hpp>

namespace sim = anno::sim;
namespace fs = std::filesystem;

namespace {

/// The run of the corpus case: 3 000 advances of 100 ms = 300 000 ms = 150 000 ticks (a tick is
/// 2 ms, WorldClock::tick) = 5 game minutes.
constexpr int kAdvances = 3000;
constexpr std::uint32_t kStepMs = 100;

fs::path expectations_path() {
    return fs::path(ANNO_FIXTURES_DIR) / "sim" / "scenario_corpus.txt";
}

std::vector<fs::path> scenario_files(const fs::path& game) {
    std::vector<fs::path> out;
    for (const auto& e : fs::recursive_directory_iterator(game / "Scenes")) {
        if (!e.is_regular_file()) {
            continue;
        }
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".sze") {
            out.push_back(e.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

/// The file's name relative to Scenes, with forward slashes.
std::string rel_name(const fs::path& game, const fs::path& p) {
    return fs::relative(p, game / "Scenes").generic_string();
}

/// One line of the expectations file: the counts of a loaded scenario and the state
/// hash of the world right after create (a digest of everything the adapter decoded:
/// settings, players, terrain, tile objects, cities; it moves with kStateHashVersion,
/// and the file is then rewritten like the goldens).
std::string counts_line(const std::string& name, const sim::ScenarioCounts& c,
                        std::uint64_t create_hash) {
    std::ostringstream o;
    o << name << " islands=" << c.islands << " links=" << c.links << " players=" << c.players
      << " cities=" << c.cities << " buildings=" << c.buildings
      << " tile_objects=" << c.tile_objects << " deposits=" << c.deposits;
    for (const sim::ScenarioWarningKind k : sim::all_warning_kinds()) {
        const auto it = c.warnings.find(k);
        o << " " << sim::warning_kind_name(k) << "=" << (it == c.warnings.end() ? 0 : it->second);
    }
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(create_hash));
    o << " create_hash=" << hex;
    return o.str();
}

struct FileResult {
    std::string name;
    std::string load_error, create_error, invariants, save_error;
    std::string line;
    sim::ScenarioCounts counts;
    std::uint64_t a14 = 0;           // islands whose w or h is not a multiple of chunk_edge
    std::uint64_t prod2_unknown = 0; // UnknownObject warnings on PROD2 items
    bool deterministic = false, save_identity = false;
    std::uint64_t hash = 0, create_hash = 0;
};

std::shared_ptr<const sim::TypeRegistry> original_registry(const fs::path& game) {
    const auto buf = anno::test::read_file(game / "Anlagen.dat");
    auto file = anno::formats::anlagen::AnlagenFile::parse(buf);
    REQUIRE(file);
    auto r =
        sim::TypeRegistry::build(file.value().entries, sim::original_goods(), sim::SimConfig{});
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return r.value();
}

/// Load, create and run one scenario; no doctest assertion (parallel worker).
FileResult run_file(const fs::path& game, const fs::path& p,
                    const std::shared_ptr<const sim::TypeRegistry>& types, int advances) {
    FileResult r;
    r.name = rel_name(game, p);
    const auto buf = anno::test::read_file(p);
    const sim::SimConfig cfg;
    auto loaded = sim::load_scenario(buf, *types, cfg);
    if (!loaded) {
        r.load_error = loaded.error();
        return r;
    }
    r.counts = sim::count(loaded.value());
    for (const sim::ScenarioSpec::IslandSpec& is : loaded.value().spec.islands) {
        if (is.w % cfg.chunk_edge != 0 || is.h % cfg.chunk_edge != 0) {
            ++r.a14;
        }
    }
    for (const sim::ScenarioWarning& w : loaded.value().warnings) {
        r.prod2_unknown +=
            w.kind == sim::ScenarioWarningKind::UnknownObject && w.record == "PROD2" ? 1U : 0U;
    }
    auto run = [&](std::uint64_t& hash) -> std::string {
        auto w = sim::World::create(loaded.value().spec, types, cfg);
        if (!w) {
            return w.error();
        }
        sim::World world = std::move(w).value();
        r.create_hash = world.state_hash();
        for (int i = 0; i < advances; ++i) {
            world.advance(kStepMs);
        }
        r.invariants = world.check_invariants();
        hash = world.state_hash();
        const std::vector<std::uint8_t> saved = world.save();
        auto back = sim::World::load(saved, types, {});
        if (!back) {
            r.save_error = back.error();
        } else {
            r.save_identity = back.value().save() == saved;
        }
        return {};
    };
    std::uint64_t h1 = 0, h2 = 0;
    r.create_error = run(h1);
    if (r.create_error.empty()) {
        r.create_error = run(h2);
        r.deterministic = h1 == h2;
        r.hash = h1;
    }
    r.line = counts_line(r.name, r.counts, r.create_hash);
    return r;
}

std::map<std::string, std::string> read_expectations() {
    std::map<std::string, std::string> out;
    std::ifstream in(expectations_path());
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        out[line.substr(0, line.find(' '))] = line;
    }
    return out;
}

bool update_requested() {
    const char* u = std::getenv("ANNO_UPDATE_GOLDEN");
    const char* ci = std::getenv("CI");
    return u != nullptr && std::string(u) == "1" && (ci == nullptr || std::string(ci).empty());
}

} // namespace

TEST_CASE("scenario corpus: every .sze loads, creates and runs five minutes") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto types = original_registry(*dir);
    const std::vector<fs::path> files = scenario_files(*dir);
    REQUIRE(files.size() == 50); // P, sze.md "Files covered"
    const auto results = anno::test::parallel_indexed(
        files.size(), [&](std::size_t i) { return run_file(*dir, files[i], types, kAdvances); },
        anno::test::parallel_options());

    std::map<sim::ScenarioWarningKind, std::uint64_t> totals;
    std::uint64_t islands = 0, links = 0, cities = 0, buildings = 0, objects = 0, a14 = 0,
                  with_island = 0, links_only = 0;
    std::vector<std::string> lines;
    for (const FileResult& r : results) {
        INFO(r.name);
        CHECK(r.load_error == "");
        CHECK(r.create_error == "");
        CHECK(r.invariants == "");
        CHECK(r.save_error == "");
        CHECK(r.deterministic);
        CHECK(r.save_identity);
        // Every PROD2 id resolves in the registry.
        CHECK(r.prod2_unknown == 0);
        lines.push_back(r.line);
        islands += r.counts.islands;
        links += r.counts.links;
        cities += r.counts.cities;
        buildings += r.counts.buildings;
        objects += r.counts.tile_objects;
        a14 += r.a14;
        with_island += r.counts.islands > 0 ? 1U : 0U;
        links_only += r.counts.islands == 0 && r.counts.links > 0 ? 1U : 0U;
        for (const auto& [k, n] : r.counts.warnings) {
            totals[k] += n;
        }
    }
    // T23: the cities' PROD/ENTRY records place buildings.
    CHECK(buildings > 0);
    std::cout << "scenario corpus: " << results.size() << " files, " << with_island
              << " with at least one island, " << links_only << " with links only; islands "
              << islands << ", links " << links << ", cities " << cities << ", buildings "
              << buildings << ", tile objects " << objects << "\n";
    std::cout << "scenario corpus: islands not a multiple of chunk_edge (A14/F20): " << a14 << "\n";
    for (const sim::ScenarioWarningKind k : sim::all_warning_kinds()) {
        std::cout << "scenario corpus: warnings " << sim::warning_kind_name(k) << " " << totals[k]
                  << "\n";
    }

    // The per-file counts against the committed expectations.
    if (update_requested()) {
        std::ofstream out(expectations_path(), std::ios::binary);
        out << "# The scenario corpus through the S14 adapter (T23: with the cities): per file\n"
               "# (relative to Scenes) the counts of the loaded ScenarioSpec, of its warnings per\n"
               "# kind, and the state hash after World::create (it moves with kStateHashVersion\n"
               "# and with what the adapter decodes). No content of the files. Rewritten by\n"
               "# scenario_test.cpp with ANNO_UPDATE_GOLDEN=1.\n";
        for (const std::string& l : lines) {
            out << l << "\n";
        }
        MESSAGE("scenario_corpus.txt rewritten");
        return;
    }
    const std::map<std::string, std::string> want = read_expectations();
    CHECK(want.size() == results.size());
    for (const std::string& l : lines) {
        const std::string name = l.substr(0, l.find(' '));
        INFO(name);
        const auto it = want.find(name);
        REQUIRE_MESSAGE(it != want.end(), "not in scenario_corpus.txt (ANNO_UPDATE_GOLDEN=1)");
        CHECK(it->second == l);
    }
}

TEST_CASE("scenario corpus: the ten scenarios with the most buildings run 20000 ticks more") {
    // §15 S14: the 5 add-on and the 5 base scenarios with the most buildings run longer;
    // under the asan preset this is the UBSan run. Since T23 "most buildings" ranks the
    // buildings the cities' PROD/ENTRY records place.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto types = original_registry(*dir);
    const std::vector<fs::path> files = scenario_files(*dir);
    std::vector<std::pair<std::uint64_t, std::size_t>> base, addon;
    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto buf = anno::test::read_file(files[i]);
        auto l = sim::load_scenario(buf, *types, sim::SimConfig{});
        REQUIRE(l);
        const auto c = sim::count(l.value());
        (l.value().spec.settings.prop14 == 3 ? addon : base).push_back({c.buildings, i});
    }
    auto top5 = [](auto& v) {
        std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
            return a.first != b.first ? a.first > b.first : a.second < b.second;
        });
        v.resize(std::min<std::size_t>(v.size(), 5));
    };
    top5(base);
    top5(addon);
    std::vector<std::size_t> picked;
    for (const auto& [n, i] : base) {
        picked.push_back(i);
    }
    for (const auto& [n, i] : addon) {
        picked.push_back(i);
    }
    REQUIRE(picked.size() == 10);
    // 20 000 ticks more than the corpus case: 400 more advances of 100 ms (50 ticks each).
    const auto results = anno::test::parallel_indexed(
        picked.size(),
        [&](std::size_t k) { return run_file(*dir, files[picked[k]], types, kAdvances + 400); },
        anno::test::parallel_options());
    for (const FileResult& r : results) {
        INFO(r.name);
        std::cout << "scenario long run: " << r.name << "\n";
        CHECK(r.create_error == "");
        CHECK(r.invariants == "");
        CHECK(r.deterministic);
        CHECK(r.save_identity);
    }
}

namespace {

/// The stocks of a producing building: output, input and raw stocks (not `last`,
/// which every scheduled step moves, producing or not); empty for a payload that does
/// not produce.
std::vector<std::uint32_t> production_state(const sim::Payload& p) {
    std::vector<std::uint32_t> v;
    auto add = [&v](const auto& xs) { v.insert(v.end(), xs.begin(), xs.end()); };
    if (const auto* w = std::get_if<sim::Workshop>(&p)) {
        add(w->out);
        add(w->in);
    } else if (const auto* x = std::get_if<sim::WeaponsWorkshop>(&p)) {
        add(x->out);
        add(x->in);
    } else if (const auto* f = std::get_if<sim::Farm>(&p)) {
        v = {f->out, f->raw};
    } else if (const auto* s = std::get_if<sim::SmallFarm>(&p)) {
        add(s->out);
        add(s->raw);
    } else if (const auto* m = std::get_if<sim::Mine>(&p)) {
        v = {m->out};
    }
    return v;
}

/// The sum of a producing building's output stocks (8.8).
std::uint64_t output_sum(const sim::Payload& p) {
    std::uint64_t n = 0;
    auto add = [&n](const auto& xs) {
        for (const auto x : xs) {
            n += x;
        }
    };
    if (const auto* w = std::get_if<sim::Workshop>(&p)) {
        add(w->out);
    } else if (const auto* x = std::get_if<sim::WeaponsWorkshop>(&p)) {
        add(x->out);
    } else if (const auto* f = std::get_if<sim::Farm>(&p)) {
        n = f->out;
    } else if (const auto* s = std::get_if<sim::SmallFarm>(&p)) {
        add(s->out);
    } else if (const auto* m = std::get_if<sim::Mine>(&p)) {
        n = m->out;
    }
    return n;
}

struct BuildingSnapshot {
    std::vector<std::uint32_t> state;
    std::uint64_t output = 0;
    std::size_t kind = 0; ///< the payload's alternative (sim::Payload index)
};

/// Every building of every city, by id: its production state and output sum.
std::map<std::uint64_t, BuildingSnapshot> snapshot(const sim::World& w) {
    std::map<std::uint64_t, BuildingSnapshot> out;
    for (const std::optional<sim::Island>& is : w.islands()) {
        if (!is) {
            continue;
        }
        for (const std::optional<sim::City>& c : is->cities) {
            if (!c) {
                continue;
            }
            for (const sim::BuildingList& l : c->index.lists) {
                for (const auto& b : l.slots) {
                    if (b) {
                        out[b->id.v] = {production_state(b->payload), output_sum(b->payload),
                                        b->payload.index()};
                    }
                }
            }
        }
    }
    return out;
}

/// Per city (island slot, city number): the warehouse stock.
std::map<std::pair<std::size_t, std::uint32_t>, std::vector<std::uint32_t>>
warehouses(const sim::World& w) {
    std::map<std::pair<std::size_t, std::uint32_t>, std::vector<std::uint32_t>> out;
    for (std::size_t i = 0; i < w.islands().size(); ++i) {
        if (!w.islands()[i]) {
            continue;
        }
        for (const std::optional<sim::City>& c : w.islands()[i]->cities) {
            if (c) {
                out[{i, c->number.v}] = c->stock.stock;
            }
        }
    }
    return out;
}

} // namespace

TEST_CASE("scenario corpus: a shipped town's economy runs 150000 ticks") {
    // T23 Do 3: the scenario with the most buildings runs 150 000 ticks (3 000 advances
    // of 100 ms); its producing buildings work (their stocks move, and some gain
    // output). The warehouse stocks are reported: no ported code moves goods between a
    // building and a warehouse yet (carriers are a figure behaviour seam, houses do not
    // consume: docs/sim/state.md §10 "As implemented in T23"), so the delta is printed,
    // not asserted.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto types = original_registry(*dir);
    const std::vector<fs::path> files = scenario_files(*dir);
    std::uint64_t most = 0;
    std::size_t pick = files.size();
    std::optional<sim::LoadedScenario> loaded;
    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto buf = anno::test::read_file(files[i]);
        auto l = sim::load_scenario(buf, *types, sim::SimConfig{});
        REQUIRE(l);
        const std::uint64_t n = sim::count(l.value()).buildings;
        if (n > most) {
            most = n;
            pick = i;
            loaded = std::move(l).value();
        }
    }
    REQUIRE(loaded.has_value());
    INFO(rel_name(*dir, files[pick]));
    auto w = sim::World::create(loaded->spec, types, sim::SimConfig{});
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    sim::World world = std::move(w).value();
    const auto before = snapshot(world);
    const auto stock_before = warehouses(world);
    for (int i = 0; i < kAdvances; ++i) {
        world.advance(kStepMs);
    }
    CHECK(world.check_invariants() == "");
    CHECK(world.clock().tick() == 150000);
    const auto after = snapshot(world);
    const auto stock_after = warehouses(world);
    REQUIRE(after.size() == before.size());
    std::uint64_t producers = 0, moved_stocks = 0, gained = 0;
    // Per payload alternative (Workshop, WeaponsWorkshop, Farm, SmallFarm, Mine):
    // producing, with stocks that moved.
    std::map<std::size_t, std::pair<std::uint64_t, std::uint64_t>> by_kind;
    for (const auto& [id, b] : before) {
        const BuildingSnapshot& a = after.at(id);
        if (b.state.empty()) {
            continue;
        }
        ++producers;
        moved_stocks += a.state != b.state ? 1U : 0U;
        gained += a.output > b.output ? 1U : 0U;
        ++by_kind[b.kind].first;
        by_kind[b.kind].second += a.state != b.state ? 1U : 0U;
    }
    const char* const kind_names[] = {"Inert", "Workshop",  "WeaponsWorkshop",
                                      "Farm",  "SmallFarm", "Mine"};
    for (const auto& [k, n] : by_kind) {
        std::cout << "scenario economy: " << (k < std::size(kind_names) ? kind_names[k] : "?")
                  << " " << n.first << " producing, " << n.second << " with stocks that moved\n";
    }
    std::uint64_t moved = 0;
    for (const auto& [city, s] : stock_before) {
        moved += stock_after.at(city) != s ? 1U : 0U;
    }
    std::cout << "scenario economy: " << rel_name(*dir, files[pick]) << ": " << before.size()
              << " buildings, " << producers << " producing; after 150000 ticks " << moved_stocks
              << " with stocks that moved, " << gained
              << " with more output in stock; warehouses that moved: " << moved << " of "
              << stock_before.size() << "\n";
    CHECK(producers > 0);
    CHECK(moved_stocks > 0);
    CHECK(gained > 0);
}
