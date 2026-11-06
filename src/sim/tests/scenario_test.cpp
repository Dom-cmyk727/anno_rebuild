// The scenario corpus (docs/sim/state.md §14.5, stage S14): every Scenes/**/*.sze of
// the game directory through the adapter, the INSELLNK resolver over the game's island
// templates (stage 2: the file's seed, ScenarioSpec::seed = 1, as the cursor the draws
// start from), World::create, five simulated minutes and check_invariants; twice from
// the same spec, save/load/save. The counts per file are held against
// fixtures/synthetic/sim/scenario_corpus.txt (counts only, no content of the files),
// whose header line names the stream and PRNG timeline versions it was made at. Needs
// ANNO1503_GAME_DIR; skipped with the one message otherwise. The output names files and
// counts only.
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

#include <anno/sim/building.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>
#include <anno/test/parallel.hpp>

#include "original_component.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace fs = std::filesystem;

namespace {

/// The run of the corpus case: 3 000 advances of 100 ms = 300 000 ms = 150 000 ticks (a tick is
/// 2 ms, WorldClock::tick) = 5 game minutes.
constexpr int kAdvances = 3000;
constexpr std::uint32_t kStepMs = 100;

fs::path expectations_path() {
    return fs::path(ANNO_FIXTURES_DIR) / "sim" / "econbook1" / "current133" / "scenario_corpus.txt";
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

/// What the run of a scenario moved (T24): producers whose stocks changed and city
/// warehouses whose stock changed, each of how many.
struct Moved {
    std::uint64_t producers = 0, producers_moved = 0, warehouses = 0, warehouses_moved = 0;
};

/// One line of the expectations file: the counts of a loaded scenario, the state hash
/// of the world right after create (a digest of everything the adapter decoded:
/// settings, players, terrain, tile objects, cities; it moves with kStateHashVersion,
/// and the file is then rewritten like the goldens) and what the run moved.
std::string counts_line(const std::string& name, const sim::ScenarioCounts& c,
                        std::uint64_t create_hash, const Moved& m) {
    std::ostringstream o;
    o << name << " islands=" << c.islands << " links=" << c.links
      << " links_resolved=" << c.links_resolved << " links_unresolved=" << c.links_unresolved
      << " players=" << c.players << " cities=" << c.cities << " buildings=" << c.buildings
      << " tile_objects=" << c.tile_objects << " deposits=" << c.deposits
      << " ore_objects=" << c.ore_objects << " ore_discovered=" << c.ore_discovered;
    for (const sim::ScenarioWarningKind k : sim::all_warning_kinds()) {
        const auto it = c.warnings.find(k);
        o << " " << sim::warning_kind_name(k) << "=" << (it == c.warnings.end() ? 0 : it->second);
    }
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(create_hash));
    o << " create_hash=" << hex << " producers_moved=" << m.producers_moved << "/" << m.producers
      << " warehouses_moved=" << m.warehouses_moved << "/" << m.warehouses;
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
    std::string component_diagnostic;
    std::uint32_t link_draws = 0; // the resolver's draws
    Moved moved;
    /// P1: the residents of the created houses (`residents_64 >> 6` summed) against the
    /// records' (`resident_sum`), and the houses with residents and a non-zero stock.
    std::uint64_t created_residents = 0, record_residents = 0, houses_with_stock = 0;
    std::uint64_t houses = 0;
};

/// The residents and the stocked houses of a created world (the House payloads).
void house_totals(const sim::World& w, std::uint64_t& residents, std::uint64_t& stocked) {
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
                    const auto* h = b ? std::get_if<sim::House>(&b->payload) : nullptr;
                    if (h == nullptr) {
                        continue;
                    }
                    residents += static_cast<std::uint64_t>(h->residents_64 >> 6);
                    const bool any = std::any_of(h->stock.begin(), h->stock.end(),
                                                 [](std::uint32_t s) { return s != 0; });
                    stocked += h->residents_64 > 0 && any ? 1U : 0U;
                }
            }
        }
    }
}

/// The header line of the expectations file: the versions the hashes were made at
/// (the standing note of docs/orchestration/TASKS.md round r; tools/sim_stream.py
/// --check refuses a stale one too).
std::string version_line() {
    return "# kStateHashVersion=" + std::to_string(sim::kStateHashVersion) +
           " kPrngTimelineVersion=" + std::to_string(sim::kPrngTimelineVersion) +
           " schema=ECONBK1 component=import-only";
}

std::shared_ptr<const sim::TypeRegistry> original_registry(const fs::path& game) {
    // The shared loader (sim_load). FigurenHeader is required, as this helper's
    // REQUIRE used to be: a bad file fails the test with the parser's text.
    const anno::sim_load::OriginalRegistryLoad loaded =
        anno::sim_load::load_original_registry(game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    REQUIRE(loaded.types);
    return loaded.types;
}

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

/// Load, create and run one scenario; no doctest assertion (parallel worker).
FileResult run_file(const fs::path& game, const fs::path& p,
                    const std::shared_ptr<const sim::TypeRegistry>& types,
                    const anno::sim_load::IslandTemplates& templates, int advances) {
    FileResult r;
    r.name = rel_name(game, p);
    const auto buf = anno::test::read_file(p);
    const sim::SimConfig cfg;
    auto resolved = anno::sim_load::load_scenario_with_links(
        buf, templates, *types, cfg, {}, original_component::actor_free_units());
    if (!resolved) {
        r.load_error = resolved.error();
        return r;
    }
    for (const std::uint32_t n : resolved.value().resolution.draws) {
        r.link_draws += n;
    }
    const sim::LoadedScenario& loaded_scenario = resolved.value().loaded;
    r.counts = sim::count(loaded_scenario);
    for (const sim::ScenarioSpec::IslandSpec& is : loaded_scenario.spec.islands) {
        if (is.w % cfg.chunk_edge != 0 || is.h % cfg.chunk_edge != 0) {
            ++r.a14;
        }
    }
    for (const sim::ScenarioWarning& w : loaded_scenario.warnings) {
        r.prod2_unknown +=
            w.kind == sim::ScenarioWarningKind::UnknownObject && w.record == "PROD2" ? 1U : 0U;
    }
    const auto component = original_component::import_only(loaded_scenario.spec, types, cfg);
    if (!component) {
        r.create_error = component.error();
        return r;
    }
    r.component_diagnostic = component.value().diagnostic;
    auto run = [&](std::uint64_t& hash) -> std::string {
        auto w = sim::World::create(component.value().spec, types, cfg);
        if (!w) {
            return w.error();
        }
        sim::World world = std::move(w).value();
        r.create_hash = world.state_hash(); // actual current identity
        r.created_residents = r.houses_with_stock = 0;
        house_totals(world, r.created_residents, r.houses_with_stock);
        if (r.name.find("single36") != std::string::npos) {
            // The first loaded house with residents and a stock, for the report.
            bool shown = false;
            for (const std::optional<sim::Island>& is : world.islands()) {
                for (const std::optional<sim::City>& c :
                     is ? is->cities : std::vector<std::optional<sim::City>>{}) {
                    for (const sim::BuildingList& l :
                         c ? c->index.lists : std::vector<sim::BuildingList>{}) {
                        for (const auto& b : l.slots) {
                            const auto* h =
                                b && !shown ? std::get_if<sim::House>(&b->payload) : nullptr;
                            if (h != nullptr && h->residents_64 > 0 &&
                                std::any_of(h->stock.begin(), h->stock.end(),
                                            [](std::uint32_t s) { return s != 0; })) {
                                shown = true;
                                std::cout << "scenario corpus original-data components: a single36 "
                                             "house: type "
                                          << b->type.v << " residents " << (h->residents_64 >> 6)
                                          << " E " << h->e << " stock";
                                for (const std::uint32_t s : h->stock) {
                                    std::cout << ' ' << s;
                                }
                                std::cout << "\n";
                            }
                        }
                    }
                }
            }
        }
        r.record_residents = loaded_scenario.resident_sum;
        r.houses = loaded_scenario.houses;
        const auto before = snapshot(world);
        const auto stock_before = warehouses(world);
        for (int i = 0; i < advances; ++i) {
            world.advance(kStepMs);
        }
        const auto after = snapshot(world);
        const auto stock_after = warehouses(world);
        r.moved = Moved{};
        for (const auto& [id, b] : before) {
            const auto it = after.find(id);
            if (b.state.empty() || it == after.end()) {
                continue;
            }
            ++r.moved.producers;
            r.moved.producers_moved += it->second.state != b.state ? 1U : 0U;
        }
        for (const auto& [city, s] : stock_before) {
            const auto it = stock_after.find(city);
            ++r.moved.warehouses;
            r.moved.warehouses_moved += it != stock_after.end() && it->second != s ? 1U : 0U;
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
    r.line = counts_line(r.name, r.counts, r.create_hash, r.moved);
    return r;
}

/// The file's lines by scenario name; `versions` gets its version header line.
std::map<std::string, std::string> read_expectations(std::string& versions,
                                                     const fs::path& path = expectations_path()) {
    std::map<std::string, std::string> out;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind("# kStateHashVersion=", 0) == 0) {
            versions = line;
            continue;
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        out[line.substr(0, line.find(' '))] = line;
    }
    return out;
}

// Independent SPIELER record-presence fixture: only this known warning changes.
std::string with_recognized_player_records(std::string row) {
    static const auto presence = [] {
        std::ifstream in(fs::path(ANNO_FIXTURES_DIR) / "sim" / "script1" /
                         "player_research_record_counts.txt");
        REQUIRE(in.good());
        std::map<std::string, std::uint32_t> result;
        std::uint32_t total = 0;
        for (std::string line; std::getline(in, line);) {
            if (line.empty() || line[0] == '#')
                continue;
            std::istringstream fields(line);
            std::string name;
            std::uint32_t forsch = 0, anltrg = 0;
            REQUIRE(static_cast<bool>(fields >> name >> forsch >> anltrg));
            REQUIRE(forsch > 0);
            REQUIRE(anltrg == 0);
            REQUIRE(result.emplace(name, 1).second);
            total += forsch;
        }
        REQUIRE(result.size() == 20);
        REQUIRE(total == 28);
        return result;
    }();
    const auto found = presence.find(row.substr(0, row.find(' ')));
    if (found == presence.end())
        return row;
    const std::string key = " UnknownRecord=";
    const auto start = row.find(key);
    REQUIRE(start != std::string::npos);
    const auto digits = start + key.size();
    const auto end = row.find(' ', digits);
    const auto old = std::stoul(row.substr(digits, end - digits));
    REQUIRE(old >= found->second);
    row.replace(digits, end - digits, std::to_string(old - found->second));
    return row;
}

struct ResearchProductionDelta {
    std::string name;
    std::uint32_t island = 0, city = 0, owner = 0, type = 0, x = 0, y = 0, research = 0;
    std::uint32_t old_moved = 0, new_moved = 0, total = 0;
};
std::vector<ResearchProductionDelta> research_production_deltas() {
    std::ifstream in(fs::path(ANNO_FIXTURES_DIR) / "sim" / "script1" /
                     "research_production_delta.txt");
    REQUIRE(in.good());
    std::vector<ResearchProductionDelta> result;
    for (std::string line; std::getline(in, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        ResearchProductionDelta r;
        std::istringstream fields(line);
        REQUIRE(static_cast<bool>(fields >> r.name >> r.island >> r.city >> r.owner >> r.type >>
                                  r.x >> r.y >> r.research >> r.old_moved >> r.new_moved >>
                                  r.total));
        REQUIRE(r.new_moved == r.old_moved + 1);
        result.push_back(std::move(r));
    }
    REQUIRE(result.size() == 3);
    return result;
}
std::string with_restored_research_production(std::string row) {
    for (const auto& delta : research_production_deltas()) {
        if (row.substr(0, row.find(' ')) != delta.name)
            continue;
        const std::string key = "producers_moved=";
        const auto pos = row.find(key);
        REQUIRE(pos != std::string::npos);
        const auto slash = row.find('/', pos + key.size());
        REQUIRE(slash != std::string::npos);
        const auto end = row.find(' ', slash);
        const auto count = std::stoul(row.substr(pos + key.size(), slash - pos - key.size()));
        const auto total = std::stoul(row.substr(slash + 1, end - slash - 1));
        REQUIRE(total == delta.total);
        REQUIRE(delta.new_moved == delta.old_moved + 1);
        // The one newly researched producer is independent of supply's newly working farms.
        // Do not require the pre-supply absolute97/214/38 totals on an accepted supply129 row.
        row.replace(pos + key.size(), slash - pos - key.size(), std::to_string(count + 1));
    }
    return row;
}

bool update_requested() {
    const char* u = std::getenv("ANNO_UPDATE_GOLDEN");
    const char* ci = std::getenv("CI");
    return u != nullptr && std::string(u) == "1" && (ci == nullptr || std::string(ci).empty());
}

struct CityStatisticsImport {
    std::uint32_t removed_head_warning = 0, stat2_warning = 0, cities = 0;
    std::uint64_t sales = 0, trade_sales = 0, trade_purchases = 0;
};
std::map<std::string, CityStatisticsImport> city_statistics_imports() {
    std::ifstream in(fs::path(ANNO_FIXTURES_DIR) / "sim" / "econbook1" /
                     "city_statistics_import.txt");
    REQUIRE(in.good());
    std::map<std::string, CityStatisticsImport> result;
    for (std::string line; std::getline(in, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream fields(line);
        std::string name;
        CityStatisticsImport row;
        REQUIRE(bool(fields >> name >> row.removed_head_warning >> row.stat2_warning >> row.sales >>
                     row.trade_sales >> row.trade_purchases >> row.cities));
        REQUIRE(result.emplace(name, row).second);
    }
    REQUIRE(result.size() == 50);
    return result;
}
std::string with_imported_city_statistics(std::string row) {
    const auto inputs = city_statistics_imports();
    const auto name = row.substr(0, row.find(' '));
    REQUIRE(inputs.contains(name));
    const auto expected = inputs.at(name);
    const std::string key = "StatisticsNotModelled=";
    const auto at = row.find(key);
    REQUIRE(at != std::string::npos);
    const auto from = at + key.size();
    const auto end = row.find(' ', from);
    REQUIRE(end != std::string::npos);
    const auto old = std::stoul(row.substr(from, end - from));
    REQUIRE(old == expected.removed_head_warning + expected.stat2_warning);
    row.replace(from, end - from, std::to_string(expected.stat2_warning));
    return row;
}

} // namespace

TEST_CASE(
    "population A original-data components: created players carry every independent shipped mask") {
    // MAP-POP-IMPL-1 A. The expectation is fixtures/synthetic/sim/player_masks.txt, written by
    // tools/player_mask_ref.py from the raw SPIELER heads without any C++ (373 heads, 263 of
    // them mask 1). The created world, not the importer alone, is compared.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::map<std::string, std::string> want;
    {
        std::ifstream in(fs::path(ANNO_FIXTURES_DIR) / "sim" / "player_masks.txt");
        REQUIRE(in.good());
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            const std::size_t sp = line.find(' ');
            want[line.substr(0, sp)] = sp == std::string::npos ? "" : line.substr(sp + 1);
        }
    }
    const auto types = original_registry(*dir);
    const std::vector<fs::path> files = scenario_files(*dir);
    REQUIRE(files.size() == 50);
    REQUIRE(want.size() == 50);
    const anno::sim_load::IslandTemplates templates = anno::sim_load::scan_island_templates(*dir);
    struct Got {
        std::string name, text, error;
        std::size_t heads = 0, ones = 0;
    };
    const auto results = anno::test::parallel_indexed(
        files.size(),
        [&](std::size_t i) {
            Got g;
            g.name = rel_name(*dir, files[i]);
            const sim::SimConfig cfg;
            auto resolved = anno::sim_load::load_scenario_with_links(
                anno::test::read_file(files[i]), templates, *types, cfg, {},
                original_component::actor_free_units());
            if (!resolved) {
                g.error = resolved.error();
                return g;
            }
            const auto component =
                original_component::import_only(resolved.value().loaded.spec, types, cfg);
            if (!component) {
                g.error = component.error();
                return g;
            }
            auto w = sim::World::create(component.value().spec, types, cfg);
            if (!w) {
                g.error = w.error();
                return g;
            }
            for (const sim::ScenarioSpec::PlayerSpec& ps : resolved.value().loaded.spec.players) {
                const auto& slot = w.value().players().slots[ps.id.v];
                if (!slot.has_value()) {
                    g.error = "player slot missing";
                    return g;
                }
                char buf[32];
                std::snprintf(buf, sizeof buf, "%u:%08x", ps.id.v, slot->flags);
                g.text += (g.text.empty() ? "" : " ") + std::string(buf);
                ++g.heads;
                g.ones += slot->flags == 1U ? 1U : 0U;
            }
            return g;
        },
        anno::test::parallel_options());
    std::size_t heads = 0, ones = 0;
    for (const Got& g : results) {
        INFO(g.name);
        CHECK(g.error == "");
        REQUIRE(want.contains(g.name));
        CHECK(g.text == want[g.name]);
        heads += g.heads;
        ones += g.ones;
    }
    CHECK(heads == 373);
    CHECK(ones == 263);
}

TEST_CASE(
    "scenario corpus original-data components: every .sze loads, creates and runs five minutes") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto types = original_registry(*dir);
    const std::vector<fs::path> files = scenario_files(*dir);
    REQUIRE(files.size() == 50); // P, sze.md "Files covered"
    // The link selection's candidates (D AnnoScene 0x10007c50): 429 templates on this
    // installation (docs/re/annoscene.md §5 step 1).
    const anno::sim_load::IslandTemplates templates = anno::sim_load::scan_island_templates(*dir);
    std::cout << "scenario corpus original-data components: " << templates.summaries.size()
              << " island templates\n";
    CHECK(templates.summaries.size() == 429);
    const auto results = anno::test::parallel_indexed(
        files.size(),
        [&](std::size_t i) { return run_file(*dir, files[i], types, templates, kAdvances); },
        anno::test::parallel_options());

    std::map<sim::ScenarioWarningKind, std::uint64_t> totals;
    std::uint64_t ore_total = 0, ore_known = 0, ore_files = 0, ore_outside = 0;
    std::uint64_t islands = 0, links = 0, cities = 0, buildings = 0, objects = 0, a14 = 0,
                  with_island = 0, links_only = 0, resolved = 0, unresolved = 0, link_draws = 0,
                  link_files = 0, link_files_complete = 0;
    Moved moved;
    std::uint64_t total_houses = 0, total_record_residents = 0, total_created_residents = 0;
    std::vector<std::string> lines;
    for (const FileResult& r : results) {
        INFO(r.name);
        MESSAGE("whole-program capability " << r.name << ": " << r.component_diagnostic);
        CHECK(r.load_error == "");
        CHECK(r.create_error == "");
        CHECK(r.invariants == "");
        CHECK(r.save_error == "");
        CHECK(r.deterministic);
        CHECK(r.save_identity);
        // P1: the created houses carry exactly the residents the records name, and a house
        // that has residents in a file with group data has a stock (single36 below).
        CHECK(r.created_residents == r.record_residents);
        if (r.name.find("single36") != std::string::npos) {
            CHECK(r.record_residents > 0);
            CHECK(r.houses_with_stock > 0);
        }
        // Every PROD2 id resolves in the registry.
        CHECK(r.prod2_unknown == 0);
        lines.push_back(r.line);
        islands += r.counts.islands;
        links += r.counts.links;
        resolved += r.counts.links_resolved;
        unresolved += r.counts.links_unresolved;
        link_draws += r.link_draws;
        if (r.counts.links > 0) {
            ++link_files;
            link_files_complete += r.counts.links_resolved == r.counts.links ? 1U : 0U;
            std::cout << "scenario corpus original-data components: links " << r.name << " "
                      << r.counts.links_resolved << " of " << r.counts.links << " resolved, "
                      << r.link_draws << " draws\n";
        }
        cities += r.counts.cities;
        buildings += r.counts.buildings;
        objects += r.counts.tile_objects;
        ore_outside += r.counts.ore_outside;
        if (r.counts.links == 0) { // islands placed from templates by links are not in the oracle
            ore_total += r.counts.ore_objects;
            ore_known += r.counts.ore_discovered;
            ++ore_files;
        }
        a14 += r.a14;
        with_island += r.counts.islands > 0 ? 1U : 0U;
        links_only += r.counts.islands == 0 && r.counts.links > 0 ? 1U : 0U;
        for (const auto& [k, n] : r.counts.warnings) {
            totals[k] += n;
        }
        moved.producers += r.moved.producers;
        moved.producers_moved += r.moved.producers_moved;
        moved.warehouses += r.moved.warehouses;
        moved.warehouses_moved += r.moved.warehouses_moved;
        total_houses += r.houses;
        total_record_residents += r.record_residents;
        total_created_residents += r.created_residents;
        // T24: what the run moved, per scenario.
        std::cout << "scenario corpus original-data components: moved " << r.name << " producers "
                  << r.moved.producers_moved << " of " << r.moved.producers << ", warehouses "
                  << r.moved.warehouses_moved << " of " << r.moved.warehouses << "\n";
    }
    std::cout << "scenario corpus original-data components: moved in all: producers "
              << moved.producers_moved << " of " << moved.producers << ", warehouses "
              << moved.warehouses_moved << " of " << moved.warehouses << "\n";
    std::cout << "scenario corpus original-data components: houses " << total_houses
              << ", residents in the records " << total_record_residents
              << ", residents of the created houses " << total_created_residents
              << ", HouseResetNotApplied " << totals[sim::ScenarioWarningKind::HouseResetNotApplied]
              << "\n";
    // Package D4: the ore objects of the OCCUR records (the figure types of the file with
    // BEWOHNTYPE ERZSYMBOL) in the scenarios WITHOUT island links (their islands are the file\'s
    // own; linked islands are placed from templates by a PRNG selection): 41 files, 1727 objects,
    // 319 with a non-zero discovery mask, counted independently from the files with the Python
    // parsers (cache/map2-research5/objects/occur.py logic over tools/insel.py and figuren.py).
    // Treasures, whale and volcano symbols are not stored.
    std::cout << "scenario corpus original-data components: ore objects " << ore_total << " in "
              << ore_files << " link-free files, with a discovery mask " << ore_known << "\n";
    CHECK(ore_files == 41);
    CHECK(ore_outside == 0); // every ore object, template islands included, lies inside its island
    CHECK(ore_total == 1727);
    CHECK(ore_known == 319);
    CHECK(total_record_residents == total_created_residents);
    CHECK(totals[sim::ScenarioWarningKind::HouseResetNotApplied] == 0);
    // T23: the cities' PROD/ENTRY records place buildings.
    CHECK(buildings > 0);
    std::cout << "scenario corpus original-data components: " << results.size() << " files, "
              << with_island << " with at least one island, " << links_only
              << " with links only; islands " << islands << ", links " << links << ", cities "
              << cities << ", buildings " << buildings << ", tile objects " << objects << "\n";
    std::cout << "scenario corpus original-data components: links " << links << " in " << link_files
              << " files: " << resolved << " resolved, " << unresolved << " unresolved, "
              << link_draws << " draws; " << link_files_complete
              << " files create every linked island\n";
    std::cout << "scenario corpus original-data components: islands not a multiple of chunk_edge "
                 "(A14/F20): "
              << a14 << "\n";
    for (const sim::ScenarioWarningKind k : sim::all_warning_kinds()) {
        std::cout << "scenario corpus original-data components: warnings "
                  << sim::warning_kind_name(k) << " " << totals[k] << "\n";
    }

    // All physical counts and movement assertions remain pinned to published128.
    std::string historical_versions;
    const auto historical = read_expectations(
        historical_versions, fs::path(ANNO_FIXTURES_DIR) / "sim" / "scenario_corpus.txt");
    REQUIRE(historical.size() == results.size());
    const auto without_identity = [](std::string row) {
        const auto pos = row.find(" create_hash=");
        REQUIRE(pos != std::string::npos);
        row.erase(pos, 29); // space + create_hash= + sixteen hex digits
        return row;
    };
    for (const auto& r : results) {
        REQUIRE(historical.contains(r.name));
        CHECK(without_identity(r.line) ==
              without_identity(with_imported_city_statistics(with_restored_research_production(
                  with_recognized_player_records(historical.at(r.name))))));
    }

    // The per-file counts against the committed expectations.
    if (update_requested()) {
        std::ofstream out(expectations_path(), std::ios::binary);
        REQUIRE(out.good());
        out << "# Original-data component corpus, full physical/movement run.\n"
               "# The scenario corpus through the S14 adapter (T23: with the cities): per file\n"
               "# (relative to Scenes) the counts of the loaded ScenarioSpec, of its warnings per\n"
               "# kind, and the state hash after World::create (it moves with kStateHashVersion\n"
               "# and with what the adapter decodes). No content of the files. Rewritten by\n"
               "# scenario_test.cpp with ANNO_UPDATE_GOLDEN=1 (T9 S14 stage 2: with the links;\n"
               "# T24: with the producers and warehouses whose stocks moved in the run).\n"
            << version_line() << "\n";
        for (const std::string& l : lines) {
            out << l << "\n";
        }
        out.close();
        REQUIRE(out.good());
        MESSAGE("SCRIPT1 scenario_corpus.txt written and closed successfully");
        return;
    }
    std::string versions;
    const std::map<std::string, std::string> want = read_expectations(versions);
    // A file made at another stream or timeline version holds stale hashes: re-baseline.
    REQUIRE_MESSAGE(versions == version_line(),
                    ("scenario_corpus.txt is for '" + versions + "', this build is '" +
                     version_line() + "': re-baseline (ANNO_UPDATE_GOLDEN=1)"));
    CHECK(want.size() == results.size());
    for (const std::string& l : lines) {
        const std::string name = l.substr(0, l.find(' '));
        INFO(name);
        const auto it = want.find(name);
        REQUIRE_MESSAGE(it != want.end(), "not in scenario_corpus.txt (ANNO_UPDATE_GOLDEN=1)");
        CHECK(it->second == l);
    }
}

TEST_CASE("scenario corpus original-data components: the ten scenarios with the most buildings run "
          "20000 ticks more") {
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
    const anno::sim_load::IslandTemplates templates = anno::sim_load::scan_island_templates(*dir);
    std::vector<std::pair<std::uint64_t, std::size_t>> base, addon;
    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto buf = anno::test::read_file(files[i]);
        auto l = sim::load_scenario(buf, *types, sim::SimConfig{},
                                    original_component::actor_free_units());
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
        [&](std::size_t k) {
            return run_file(*dir, files[picked[k]], types, templates, kAdvances + 400);
        },
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

TEST_CASE("scenario corpus original-data components: a shipped town's economy runs 150000 ticks") {
    // T23 Do 3 and T24 acceptance (3): the scenario with the most buildings runs 150 000
    // ticks (3 000 advances of 100 ms); its producing buildings work (their stocks
    // move, and some gain output), and since T24 the market carts and the workshop
    // carriers move goods between buildings and warehouses (docs/sim/state.md §5.9): at
    // least one warehouse stock moves.
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
        auto l = sim::load_scenario(buf, *types, sim::SimConfig{},
                                    original_component::actor_free_units());
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
    auto component = original_component::import_only(loaded->spec, types);
    REQUIRE_MESSAGE(component, (component ? "" : component.error()));
    MESSAGE("original-data economy component: " << component.value().diagnostic);
    auto w = sim::World::create(component.value().spec, types, sim::SimConfig{});
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
    // T26 acceptance (3): the houses and residents the records carry (read for this
    // diagnostic; applied to the house payload since P1, OQ-165).
    std::cout << "scenario economy: houses " << loaded->houses << ", with residents > 0 "
              << loaded->houses_with_residents << ", sum of residents " << loaded->resident_sum
              << "\n";
    CHECK(producers > 0);
    CHECK(moved_stocks > 0);
    CHECK(gained > 0);
    CHECK(moved > 0);
}

TEST_CASE("transport search statistics (ANNO_SIM_TRANSPORT_STATS)") {
    // T27 step 4 and T29 steps 2-3, measurement only, off unless ANNO_SIM_TRANSPORT_STATS
    // is set and the game dir is present (the counters are test-only; `src/sim` holds
    // none — lint forbids getenv, cstdio and mutable statics there). For
    // `S01_07_01` and `single36`: the scenario is run 150 000 ticks (steady state); a
    // single TransportStats sink is routed through World::set_transport_stats so every
    // transport_tick call of the run accumulates its outcome and T29 diagnostic (cart
    // and carrier mixed, since one sink per TickContext); on top, the T27 per-sender
    // snapshot is taken at ticks 0, 30 000 and 150 000, split cart/carrier, by calling
    // choose_source directly off a SerialWorld (that path does not read ctx.transport_
    // stats, so the snapshot buckets stay separate from the over-run accumulator).
    if (std::getenv("ANNO_SIM_TRANSPORT_STATS") == nullptr) {
        MESSAGE("skipping: set ANNO_SIM_TRANSPORT_STATS=1 (and ANNO1503_GAME_DIR) to run");
        return;
    }
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto types = original_registry(*dir);
    const std::vector<fs::path> files = scenario_files(*dir);
    const auto take_snapshot = [&](const sim::World& world, std::uint64_t tick, const char* want) {
        sim::Effects fx;
        sim::SerialWorld serial(world.islands(), world.config().layout, world.figures(),
                                world.players(), fx);
        sim::TransportStats cart, carrier;
        std::uint64_t markets = 0, workshops = 0, no_figure_type = 0;
        for (const std::optional<sim::Island>& iso : world.islands()) {
            if (!iso) {
                continue;
            }
            const sim::Island& is = *iso;
            for (const auto& [id, key] : is.cache.by_id) {
                const sim::Building* b = is.find_building(id);
                if (b == nullptr) {
                    continue;
                }
                const sim::BuildingType* t = types->find(b->type);
                if (t == nullptr || t->prod.figure_ids.empty()) {
                    continue;
                }
                std::uint32_t job = 0;
                sim::TransportStats* stats = nullptr;
                std::uint64_t* count = nullptr;
                if (std::holds_alternative<sim::Market>(b->payload)) {
                    job = sim::kJobMarketCart;
                    stats = &cart;
                    count = &markets;
                } else if (std::holds_alternative<sim::Workshop>(b->payload)) {
                    job = sim::kJobWorkshopCarrier;
                    stats = &carrier;
                    count = &workshops;
                } else {
                    continue;
                }
                const sim::FigureType* ft = types->find_figure_type(t->prod.figure_ids[0]);
                if (ft == nullptr) {
                    ++no_figure_type; // not classified (counted apart, T27 review)
                    continue;
                }
                ++*count;
                static_cast<void>(choose_source(serial, *types, job, *b, *t, key.city,
                                                ft->capacity_88, ft->layer, stats));
            }
        }
        std::cout << "transport snapshot: " << want << " tick " << tick << " markets " << markets
                  << " (cart: no_candidate " << cart.no_candidate << " unreached " << cart.unreached
                  << " no_goal_reached " << cart.no_goal_reached << " low_score " << cart.low_score
                  << " found " << cart.found << " incomplete " << cart.incomplete
                  << " home_no_road " << cart.home_no_road << " goal_no_road " << cart.goal_no_road
                  << " edge " << cart.edge << " corner " << cart.corner << " outside_disc "
                  << cart.outside_disc << " start_outside_rect " << cart.start_outside_rect
                  << "); workshops " << workshops << " (carrier: no_candidate "
                  << carrier.no_candidate << " unreached " << carrier.unreached
                  << " no_goal_reached " << carrier.no_goal_reached << " low_score "
                  << carrier.low_score << " found " << carrier.found << " incomplete "
                  << carrier.incomplete << " home_no_road " << carrier.home_no_road
                  << " goal_no_road " << carrier.goal_no_road << " edge " << carrier.edge
                  << " corner " << carrier.corner << " outside_disc " << carrier.outside_disc
                  << " start_outside_rect " << carrier.start_outside_rect
                  << "); senders without a figure type " << no_figure_type << "\n";
    };
    for (const char* want : {"0100Kampagne01/S01_07_01.sze", "0201SingleAddOn/single36.sze"}) {
        const auto it = std::find_if(files.begin(), files.end(),
                                     [&](const fs::path& p) { return rel_name(*dir, p) == want; });
        if (it == files.end()) {
            MESSAGE("not found: " << want);
            continue;
        }
        const auto buf = anno::test::read_file(*it);
        auto l = sim::load_scenario(buf, *types, sim::SimConfig{},
                                    original_component::actor_free_units());
        REQUIRE(l);
        auto w = sim::World::create(l.value().spec, types, sim::SimConfig{});
        REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
        sim::World world = std::move(w).value();
        // T29: the over-run sink; ticks 0, 30 000 and 150 000 are the three snapshot
        // moments. 150 000 ticks = 2 ms per tick · 150 000 = 300 000 ms = kAdvances
        // (3 000) · kStepMs (100); 30 000 ticks = 600 advances.
        sim::TransportStats run_stats;
        world.set_transport_stats(&run_stats);
        take_snapshot(world, world.clock().tick(), want);
        for (int i = 0; i < 600; ++i) {
            world.advance(kStepMs);
        }
        take_snapshot(world, world.clock().tick(), want);
        for (int i = 600; i < kAdvances; ++i) {
            world.advance(kStepMs);
        }
        take_snapshot(world, world.clock().tick(), want);
        world.set_transport_stats(nullptr);
        std::cout << "transport run: " << want << " ticks " << world.clock().tick()
                  << " (choose_source: no_candidate " << run_stats.no_candidate << " unreached "
                  << run_stats.unreached << " no_goal_reached " << run_stats.no_goal_reached
                  << " low_score " << run_stats.low_score << " found " << run_stats.found
                  << " incomplete " << run_stats.incomplete << " home_no_road "
                  << run_stats.home_no_road << " goal_no_road " << run_stats.goal_no_road
                  << " edge " << run_stats.edge << " corner " << run_stats.corner
                  << " outside_disc " << run_stats.outside_disc << " start_outside_rect "
                  << run_stats.start_outside_rect << ")\n";
        // Top-15 frontier type ids by count, with prodtype, flags_word (hex), walk_class.
        std::vector<std::pair<std::uint32_t, std::uint64_t>> top(run_stats.frontier.begin(),
                                                                 run_stats.frontier.end());
        std::sort(top.begin(), top.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        if (top.size() > 15) {
            top.resize(15);
        }
        std::cout << "transport frontier: " << want << " top-" << top.size() << " of "
                  << run_stats.frontier.size()
                  << " type ids (count, prodtype, flags_word_hex, "
                     "walk_class):\n";
        for (const auto& [tid, cnt] : top) {
            std::uint32_t prodtype = 0, flags = 0, wc = 0;
            if (tid != 0) {
                const sim::BuildingType* t = types->find(sim::TypeId{tid});
                if (t != nullptr) {
                    prodtype = t->prodtype;
                    flags = t->flags_word;
                    wc = t->walk_class;
                }
            }
            char hex[12];
            std::snprintf(hex, sizeof hex, "0x%x", flags);
            std::cout << "  type " << tid << " count " << cnt << " prodtype " << prodtype
                      << " flags " << hex << " walk_class " << wc << "\n";
        }
    }
}

TEST_CASE("SCRIPT2 original-data corpus creation identities and imported masks") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto types = original_registry(*dir);
    std::string versions;
    const bool generating = update_requested();
    const auto expected = generating
                              ? read_expectations(versions, fs::path(ANNO_FIXTURES_DIR) / "sim" /
                                                                "scenario_corpus.txt")
                              : read_expectations(versions);
    if (!generating)
        REQUIRE(versions == version_line());
    std::vector<std::string> generated;
    const auto files = scenario_files(*dir);
    REQUIRE(files.size() == 50);
    const auto templates = anno::sim_load::scan_island_templates(*dir);
    std::size_t masks = 0, isolated = 0, supported = 0;
    const auto statistics = city_statistics_imports();
    for (const auto& file : files) {
        const auto name = rel_name(*dir, file);
        INFO(name);
        auto loaded = anno::sim_load::load_scenario_with_links(
            anno::test::read_file(file), templates, *types, {}, {},
            original_component::actor_free_units());
        REQUIRE(loaded);
        REQUIRE(statistics.contains(name));
        const auto& expected_statistics = statistics.at(name);
        std::uint64_t sales = 0, trade_sales = 0, trade_purchases = 0, city_count = 0;
        for (const auto& island : loaded.value().loaded.spec.islands)
            for (const auto& city : island.cities) {
                sales += city.sales;
                trade_sales += city.trade_sales;
                trade_purchases += city.trade_purchases;
                ++city_count;
            }
        CHECK(sales == expected_statistics.sales);
        CHECK(trade_sales == expected_statistics.trade_sales);
        CHECK(trade_purchases == expected_statistics.trade_purchases);
        CHECK(city_count == expected_statistics.cities);
        const auto warnings = sim::count(loaded.value().loaded);
        const auto found = warnings.warnings.find(sim::ScenarioWarningKind::StatisticsNotModelled);
        CHECK((found == warnings.warnings.end() ? 0 : found->second) ==
              expected_statistics.stat2_warning);
        auto component = original_component::import_only(loaded.value().loaded.spec, types);
        REQUIRE_MESSAGE(component, (component ? "" : component.error()));
        MESSAGE("whole-program capability " << name << ": " << component.value().diagnostic);
        isolated += component.value().isolated ? 1 : 0;
        supported += component.value().isolated ? 0 : 1;
        auto made = sim::World::create(component.value().spec, types);
        REQUIRE_MESSAGE(made, (made ? "" : made.error()));
        REQUIRE(expected.contains(name));
        const auto& row = expected.at(name);
        const auto pos = row.find("create_hash=");
        REQUIRE(pos != std::string::npos);
        if (generating) {
            // Accepted supply129 physical columns, with UNIT's reviewed research/presence
            // corrections. Creation-only generation must not import stale128 movement counts.
            auto new_row = with_imported_city_statistics(
                with_restored_research_production(with_recognized_player_records(row)));
            char hex[17];
            std::snprintf(hex, sizeof hex, "%016llx",
                          static_cast<unsigned long long>(made.value().state_hash()));
            new_row.replace(new_row.find("create_hash=") + 12, 16, hex);
            generated.push_back(std::move(new_row));
        } else {
            CHECK(made.value().state_hash() == std::stoull(row.substr(pos + 12, 16), nullptr, 16));
        }
        for (const auto& p : loaded.value().loaded.spec.players) {
            const auto* got = made.value().players().find(p.id);
            REQUIRE(got != nullptr);
            CHECK(got->flags == p.flags.value_or(1));
            ++masks;
        }
    }
    CHECK(masks == 373);
    CHECK(isolated == 37);
    CHECK(supported == 13);
    if (generating) {
        REQUIRE(expectations_path() == fs::path(ANNO_FIXTURES_DIR) / "sim" / "econbook1" /
                                           "current133" / "scenario_corpus.txt");
        std::ofstream out(expectations_path(), std::ios::binary);
        REQUIRE(out.good());
        out << "# ECONBK1 original-data component corpus. Creation identities generated here;\n"
               "# physical/movement columns copied from accepted supply129 plus UNIT corrections, "
               "not executed by this\n"
               "# creation-only generator. The normal long corpus gate validates those columns.\n"
            << version_line() << "\n";
        for (const auto& row : generated)
            out << row << "\n";
        out.close();
        REQUIRE(out.good());
        MESSAGE("SCRIPT2 creation-only generation: physical/movement acceptance remains pending");
    }
}

TEST_CASE(
    "SCRIPT1 restored original research activates exactly the three recorded weapon producers") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto types = original_registry(*dir);
    std::string accepted_supply_version;
    const auto accepted_supply = read_expectations(
        accepted_supply_version, fs::path(ANNO_FIXTURES_DIR) / "sim" / "scenario_corpus.txt");
    REQUIRE(accepted_supply_version == "# kStateHashVersion=130 kPrngTimelineVersion=5");
    const auto templates = anno::sim_load::scan_island_templates(*dir);
    for (const auto& delta : research_production_deltas()) {
        INFO(delta.name);
        auto loaded = anno::sim_load::load_scenario_with_links(
            anno::test::read_file(*dir / "Scenes" / delta.name), templates, *types, {}, {},
            original_component::actor_free_units());
        REQUIRE(loaded);
        auto component = original_component::import_only(loaded.value().loaded.spec, types);
        REQUIRE(component);
        REQUIRE(component.value().isolated);
        auto made = sim::World::create(component.value().spec, types);
        REQUIRE(made);
        sim::World with = std::move(made).value();
        sim::World without = with;
        const sim::PlayerId owner{delta.owner};
        REQUIRE(with.players().find(owner)->research.state[delta.research] == 2);
        auto& bit = sim::WorldTestAccess::players(without)
                        .slots[delta.owner]
                        ->research.state[delta.research];
        bit = static_cast<std::uint8_t>(bit & ~2u); // ONLY the one reviewed research bit
        REQUIRE_FALSE(without.players().find(owner)->research.researched(delta.research));
        const sim::CityRef city{sim::IslandId{delta.island}, sim::CityNo{delta.city}};
        const auto* building = with.building_at(city, static_cast<std::int32_t>(delta.x),
                                                static_cast<std::int32_t>(delta.y));
        REQUIRE(building != nullptr);
        REQUIRE(building->type.v == delta.type);
        const auto id = building->id.v;
        const auto* type = types->find(building->type);
        REQUIRE(type != nullptr);
        const auto initial = std::get<sim::WeaponsWorkshop>(building->payload);
        CHECK(initial.line_on == std::vector<bool>{true, false});
        CHECK_FALSE(initial.suspended);
        const bool bow = delta.type == 1006;
        CHECK(initial.out == (bow ? (delta.island == 35 ? std::vector<std::uint32_t>{0, 0}
                                                        : std::vector<std::uint32_t>{65, 0})
                                  : std::vector<std::uint32_t>{13, 72}));
        CHECK(initial.in == (bow ? (delta.island == 35 ? std::vector<std::uint32_t>{10, 11042}
                                                       : std::vector<std::uint32_t>{583, 753})
                                 : std::vector<std::uint32_t>{9, 952}));
        REQUIRE(type->output_goods.size() == 2);
        CHECK(types->goods(type->output_goods[0])->forsch == delta.research);
        // Isolated first legal kernel visit from these ORIGINAL stocks. Reset only the
        // copy's step stamp; do not add resources or alter the two simulation worlds.
        auto on = initial, off = initial;
        on.last = off.last = 0;
        sim::refresh_unlocked(on, *type, *types, sim::PlayersView(with.players()), owner);
        sim::refresh_unlocked(off, *type, *types, sim::PlayersView(without.players()), owner);
        REQUIRE(on.unlocked[0]);
        REQUIRE_FALSE(off.unlocked[0]);
        const std::uint32_t rate = bow ? 9 : 6, need0 = bow ? 8 : 6, need1 = bow ? 4 : 6;
        CHECK(type->prod.output_per_step[0] == rate);
        CHECK(type->prod.input_per_step == std::vector<std::uint32_t>{need0, need1});
        const auto produced = sim::weapons_workshop_step(on, type->prod, 500);
        CHECK(produced.stepped);
        CHECK(produced.made == std::vector<std::uint32_t>{rate, 0});
        CHECK(on.out == std::vector<std::uint32_t>{initial.out[0] + rate, initial.out[1]});
        CHECK(on.in == std::vector<std::uint32_t>{initial.in[0] - need0, initial.in[1] - need1});
        CHECK(sim::weapons_workshop_step(off, type->prod, 500).stepped);
        CHECK(off.out == initial.out);
        CHECK(off.in == initial.in);
        const auto before = snapshot(with);
        for (int i = 0; i < kAdvances; ++i) {
            with.advance(kStepMs);
            without.advance(kStepMs);
        }
        REQUIRE(with.check_invariants().empty());
        REQUIRE(without.check_invariants().empty());
        const auto after = snapshot(with), control = snapshot(without);
        std::vector<std::uint64_t> activated;
        std::uint32_t moved = 0, control_moved = 0, producers = 0;
        for (const auto& [bid, original] : before) {
            if (original.state.empty())
                continue;
            ++producers;
            const bool a = after.at(bid).state != original.state;
            const bool b = control.at(bid).state != original.state;
            moved += a ? 1u : 0u;
            control_moved += b ? 1u : 0u;
            if (a != b) {
                CHECK(a);
                CHECK_FALSE(b);
                activated.push_back(bid);
            }
        }
        CHECK(activated == std::vector<std::uint64_t>{id});
        CHECK(producers == delta.total);
        // The historical delta identifies one researched producer. Supply's farms
        // already changed the absolute totals; retain that independent accepted baseline.
        REQUIRE(accepted_supply.contains(delta.name));
        const auto& baseline = accepted_supply.at(delta.name);
        const std::string key = "producers_moved=";
        const auto begin = baseline.find(key);
        REQUIRE(begin != std::string::npos);
        const auto slash = baseline.find('/', begin + key.size());
        REQUIRE(slash != std::string::npos);
        const auto expected_control =
            std::stoul(baseline.substr(begin + key.size(), slash - begin - key.size()));
        CHECK(control_moved == expected_control);
        CHECK(moved == control_moved + 1);
        MESSAGE("research activation " << delta.name << " type=" << delta.type
                                       << " research=" << delta.research
                                       << " control=" << control_moved << " restored=" << moved);
    }
}

TEST_CASE("integration corpus: accepted supply counts and restored research compose once") {
    std::string versions;
    const auto base =
        read_expectations(versions, fs::path(ANNO_FIXTURES_DIR) / "sim" / "scenario_corpus.txt");
    REQUIRE(versions == "# kStateHashVersion=130 kPrngTimelineVersion=5");
    for (const auto& [name, expected] : std::array<std::pair<const char*, const char*>, 3>{
             {{"0100Kampagne01/S01_03_01.sze", "producers_moved=116/139"},
              {"0100Kampagne01/S01_07_01.sze", "producers_moved=269/302"},
              {"0200Single/Single_24.sze", "producers_moved=47/58"}}}) {
        REQUIRE(base.contains(name));
        const auto normalized =
            with_restored_research_production(with_recognized_player_records(base.at(name)));
        CHECK(normalized.find(expected) != std::string::npos);
        CHECK(normalized !=
              with_restored_research_production(normalized)); // applying twice is a wrong reading
    }
}
