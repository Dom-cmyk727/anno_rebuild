#include "original_component.hpp"
// MAP-POP-IMPL-1 E: original-data component canary; unsupported complete scripts are
// explicitly refused then omitted in a NEW Missing-context fixture. The original data feeds the
// imported players, houses, group tables and config into the pure House kernel; the extraction
// reads the world through const interfaces only, save/reload must reproduce the extracted inputs
// and the calculations at the same supplied time, and the House tick dispatch stays a no-op. An
// unchanged world hash is not offered as proof of calculation: the calculations are counted and
// their domain outcomes tallied. With ANNO_POP_CANARY_OUT set, the same inputs and outputs are
// written as a vector file that tools/house_kernel_ref.py re-checks independently (the F gate runs
// it).
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/house_kernel.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>

namespace {

namespace sim = anno::sim;
namespace fs = std::filesystem;

/// One House read out of a world with everything the kernel is given.
struct Extracted {
    std::string file;
    std::uint32_t group = 0;
    std::uint32_t mask = 0;
    std::uint16_t last = 0;
    std::int32_t residents_64 = 0;
    std::uint32_t e = 0;
    std::vector<std::uint32_t> stock;
    bool operator==(const Extracted&) const = default;
};

std::vector<Extracted> extract(const sim::World& w, const sim::TypeRegistry& types,
                               const std::string& file) {
    std::vector<Extracted> out;
    for (const std::optional<sim::Island>& is : w.islands()) {
        if (!is) {
            continue;
        }
        for (const std::optional<sim::City>& c : is->cities) {
            if (!c) {
                continue;
            }
            const auto& player = w.players().slots[c->owner.v];
            if (!player.has_value()) {
                continue;
            }
            for (const sim::BuildingList& l : c->index.lists) {
                for (const auto& b : l.slots) {
                    const auto* h = b ? std::get_if<sim::House>(&b->payload) : nullptr;
                    if (h == nullptr) {
                        continue;
                    }
                    const sim::BuildingType* type = types.find(b->type);
                    const auto group = type ? sim::house_group(w.config(), type->prodtype)
                                            : std::optional<std::uint32_t>{};
                    if (!group || *group >= types.house_groups().size()) {
                        continue;
                    }
                    out.push_back(Extracted{file, *group, player->flags, h->last, h->residents_64,
                                            h->e, h->stock});
                }
            }
        }
    }
    return out;
}

struct Tally {
    std::size_t calculations = 0, skipped = 0, evaluated = 0, refused = 0;
    std::size_t nontrivial_mask = 0, zero_residents = 0;
    std::map<std::string, std::size_t> reasons;
    std::map<std::uint32_t, std::size_t> masks;
    std::vector<std::string> vector_lines;
};

/// Runs the kernel on every extracted house at `now` and tallies the outcome; also appends the
/// reference vector line.
void calculate(const std::vector<Extracted>& houses, const sim::TypeRegistry& types,
               const sim::SimConfig& cfg, std::uint32_t now, Tally& t) {
    for (const Extracted& x : houses) {
        const auto view = types.population_view(x.group, cfg);
        REQUIRE(view.ok());
        sim::HouseKernelInputs in;
        in.view = &view.value();
        in.player_mask = x.mask;
        in.now = now;
        in.last = x.last;
        in.gate_period = 2500;
        in.residents_64 = x.residents_64;
        in.e = x.e;
        in.stock = x.stock;
        in.previous_wohnmax = view.value().previous_wohnmax;
        const sim::HouseKernelResult r = sim::house_kernel(in);
        ++t.calculations;
        t.masks[x.mask]++;
        t.zero_residents += x.residents_64 == 0 ? 1U : 0U;
        std::ostringstream line;
        line << "vec c" << t.calculations << " group=g" << x.group << " now=" << now
             << " last=" << x.last << " period=2500 mask=" << x.mask << " e=" << x.e
             << " res=" << x.residents_64 << " stock=";
        for (std::size_t i = 0; i < x.stock.size(); ++i) {
            line << (i ? "," : "") << x.stock[i];
        }
        if (view.value().previous_wohnmax.has_value()) {
            line << " prev=" << *view.value().previous_wohnmax;
        }
        line << " =>";
        if (r.status == sim::HouseKernelStatus::Skipped) {
            ++t.skipped;
            line << " status=skipped last=" << x.last;
        } else if (r.status == sim::HouseKernelStatus::Evaluated) {
            ++t.evaluated;
            t.nontrivial_mask += x.mask != 1U ? 1U : 0U;
            REQUIRE(r.residents_64.has_value());
            REQUIRE(r.mood.has_value());
            line << " status=evaluated last=" << *r.last << " e=" << *r.e << " mood=" << *r.mood
                 << " res=" << *r.residents_64 << " csub=" << *r.counter_sub
                 << " cadd=" << *r.counter_add << " lowest=" << *r.lowest << " stock=";
            for (std::size_t i = 0; i < r.stock.size(); ++i) {
                line << (i ? "," : "") << r.stock[i];
            }
        } else {
            ++t.refused;
            t.reasons[std::to_string(static_cast<int>(r.reason))]++;
            line << " status=not_evaluated";
        }
        t.vector_lines.push_back(line.str());
    }
}

std::string group_lines(const sim::TypeRegistry& types) {
    std::ostringstream o;
    const auto goods = types.goods_table();
    std::uint32_t idx = 0;
    for (const sim::HouseGroup& g : types.house_groups()) {
        const auto table = [&](const char* n, const std::array<std::int8_t, 21>& v) {
            o << "table g" << idx << n << " ";
            for (std::size_t i = 0; i < v.size(); ++i) {
                o << (i ? "," : "") << static_cast<int>(v[i]);
            }
            o << "\n";
        };
        table("n", g.nahrung);
        table("b", g.bedarf);
        table("i", g.infra);
        o << "group g" << idx << " wohnmax=" << g.wohnmax << " nodown=" << (g.no_downgrade ? 1 : 0)
          << " vermehr=";
        for (std::size_t i = 0; i < g.vermehr.size(); ++i) {
            o << (i ? "," : "") << static_cast<int>(g.vermehr[i]);
        }
        o << " nahrung=g" << idx << "n bedarf=g" << idx << "b infra=g" << idx << "i needs=";
        std::string bau;
        for (std::size_t i = 0; i < g.needs.size(); ++i) {
            o << (i ? "," : "") << g.needs[i].ware << ":" << (g.needs[i].force ? 1 : 0) << ":"
              << g.needs[i].menge;
            bau += (i ? "," : "") + std::to_string(g.needs[i].ware) + ":" +
                   std::to_string(goods[g.needs[i].ware].bau_first);
        }
        o << " bau=" << bau << "\n";
        ++idx;
    }
    return o.str();
}

bool write_vectors(std::ostream& out, const std::string& groups,
                   const std::vector<std::string>& vectors) {
    out << "# real-input canary vectors (generated by population_canary_test.cpp)\n" << groups;
    for (const auto& line : vectors) {
        out << line << '\n';
    }
    out.flush();
    return out.good();
}

} // namespace

TEST_CASE("population canary: evidence output errors fail the writer") {
    const std::vector<std::string> vectors{"vec control => status=skipped last=0"};
    std::ostringstream good;
    REQUIRE(write_vectors(good, "group control\n", vectors));
    CHECK(good.str() == "# real-input canary vectors (generated by population_canary_test.cpp)\n"
                        "group control\nvec control => status=skipped last=0\n");
    std::ofstream unopened;
    CHECK_FALSE(write_vectors(unopened, "group control\n", vectors));
    struct FlushFailure : std::stringbuf {
        int sync() override { return -1; }
    } full;
    std::ostream failing(&full);
    CHECK_FALSE(write_vectors(failing, "group control\n", vectors));
}

TEST_CASE("population canary: original-data components feed the pure House kernel") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    const auto types = loaded.types;
    const sim::SimConfig cfg;
    REQUIRE(types->derived_groups().size() == 5);

    std::vector<fs::path> files;
    for (const auto& e : fs::recursive_directory_iterator(*dir / "Scenes")) {
        if (e.is_regular_file() && e.path().extension() == ".sze") {
            files.push_back(e.path());
        }
    }
    std::sort(files.begin(), files.end());
    // single36 (the shipped town whose houses carry stocks) first, then at most 11 more files in
    // name order until a house owner with a stage mask other than 1 has been calculated.
    std::stable_sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
        return (a.string().find("single36") != std::string::npos) >
               (b.string().find("single36") != std::string::npos);
    });
    const anno::sim_load::IslandTemplates templates = anno::sim_load::scan_island_templates(*dir);

    Tally taken, skipped;
    std::size_t files_with_houses = 0, examined = 0, worlds_reloaded = 0;
    bool noop_checked = false;
    for (const fs::path& file : files) {
        if (examined >= 12 || (files_with_houses >= 3 && taken.nontrivial_mask > 0)) {
            break;
        }
        ++examined;
        auto resolved = anno::sim_load::load_scenario_with_links(anno::test::read_file(file),
                                                                 templates, *types, cfg, {});
        REQUIRE(resolved);
        auto component = original_component::import_only(resolved.value().loaded.spec, types, cfg);
        REQUIRE_MESSAGE(component, (component ? "" : component.error()));
        MESSAGE("original-data pure-House component: " << file.filename().string() << " "
                                                       << component.value().diagnostic);
        auto created = sim::World::create(component.value().spec, types, cfg);
        REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
        sim::World world = std::move(created).value();
        const std::vector<Extracted> houses = extract(world, *types, file.filename().string());
        if (houses.empty()) {
            continue;
        }
        ++files_with_houses;
        // Const reads change nothing (and the extraction is repeatable).
        const std::uint64_t before = world.state_hash();
        CHECK(extract(world, *types, file.filename().string()) == houses);
        CHECK(world.state_hash() == before);
        // One skipped gate (t = 1000, last = 0) and one taken gate (t = 2500) for every house.
        calculate(houses, *types, cfg, 1000, skipped);
        calculate(houses, *types, cfg, 2500, taken);
        CHECK(world.state_hash() == before);
        // Save / reload reproduces the inputs and the calculations at the same time.
        auto reloaded = sim::World::load(world.save(), types);
        REQUIRE_MESSAGE(reloaded.ok(), (reloaded.ok() ? std::string() : reloaded.error()));
        const std::vector<Extracted> again =
            extract(reloaded.value(), *types, file.filename().string());
        CHECK(again == houses);
        Tally redo;
        calculate(again, *types, cfg, 2500, redo);
        Tally orig;
        calculate(houses, *types, cfg, 2500, orig);
        CHECK(redo.vector_lines.size() == orig.vector_lines.size());
        for (std::size_t i = 0; i < redo.vector_lines.size(); ++i) {
            // the line names differ by their running index only: compare after the first space
            CHECK(redo.vector_lines[i].substr(redo.vector_lines[i].find(' ', 4)) ==
                  orig.vector_lines[i].substr(orig.vector_lines[i].find(' ', 4)));
        }
        ++worlds_reloaded;
        // The House dispatch is a no-op: thirty simulated seconds (far past the 2500 gate) change
        // no house field and no `last`.
        if (!noop_checked) {
            for (int i = 0; i < 3000; ++i) {
                world.advance(10);
            }
            const std::vector<Extracted> later = extract(world, *types, file.filename().string());
            REQUIRE(later.size() == houses.size());
            for (std::size_t i = 0; i < later.size(); ++i) {
                CHECK(later[i].last == houses[i].last);
                CHECK(later[i].residents_64 == houses[i].residents_64);
                CHECK(later[i].e == houses[i].e);
                CHECK(later[i].stock == houses[i].stock);
            }
            noop_checked = true;
        }
    }
    std::cout << "population canary: " << examined << " files examined, " << files_with_houses
              << " with houses; " << taken.calculations + skipped.calculations
              << " calculations: taken-gate " << taken.evaluated << " evaluated, " << taken.refused
              << " not evaluated; " << skipped.skipped << " skipped; masks:";
    for (const auto& [m, n] : taken.masks) {
        std::cout << " 0x" << std::hex << m << std::dec << "x" << n;
    }
    std::cout << "; zero-residents houses " << taken.zero_residents << "; reasons";
    for (const auto& [r, n] : taken.reasons) {
        std::cout << " " << r << "x" << n;
    }
    std::cout << "\n";
    CHECK(files_with_houses >= 1);
    CHECK(taken.evaluated + taken.refused == taken.calculations);
    CHECK(taken.evaluated > 0);
    CHECK(skipped.skipped == skipped.calculations); // t = 1000 never opens the gate
    CHECK(taken.skipped == 0);                      // t = 2500 always does (last = 0)
    CHECK(taken.nontrivial_mask > 0);
    CHECK(worlds_reloaded == files_with_houses);
    CHECK(noop_checked);
    if (const char* out = std::getenv("ANNO_POP_CANARY_OUT")) {
        std::ofstream f(out);
        REQUIRE_MESSAGE(f.is_open(), "cannot open population evidence output: " << out);
        REQUIRE_MESSAGE(write_vectors(f, group_lines(*types), taken.vector_lines),
                        "cannot write population evidence output: " << out);
        f.close();
        REQUIRE_MESSAGE(!f.fail(), "cannot close population evidence output: " << out);
    }
}
