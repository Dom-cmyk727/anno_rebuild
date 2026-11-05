// MAP-POP-LIVE-1 L5: the live House update on the ORIGINAL registry and an ORIGINAL scenario
// (docs/sim/population-live.md section 20). Rules::house_updates on, no grants, no relocations, no
// candidate rewrite, no synthetic fixture: only the loaded stocks and normal production.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/house_tick.hpp>
#include <anno/sim/house_transition.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>

#include "evidence_out.hpp"
#include "world_access.hpp"

namespace {

namespace sim = anno::sim;
namespace fs = std::filesystem;

struct Probe {
    std::string file;
    std::size_t houses = 0;
    std::uint64_t ticks = 0;
    sim::TransportStats stats;
    std::string fault;
    double seconds = 0;
};

std::size_t count_houses(const sim::World& w) {
    std::size_t n = 0;
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
                    n += b && std::holds_alternative<sim::House>(b->payload) ? 1U : 0U;
                }
            }
        }
    }
    return n;
}

sim::Rules live_rules() {
    sim::Rules r;
    r.house_updates = true;
    return r;
}

} // namespace

// ---- the real canary ---------------------------------------------------------------------

namespace {

constexpr std::uint32_t kStepMs = 100; // one advance = 50 ticks (a tick is 2 ms)

struct Original {
    std::shared_ptr<const sim::TypeRegistry> types;
    sim::SimConfig cfg;
    anno::sim_load::IslandTemplates templates;
    fs::path dir;
};

std::optional<Original> load_original() {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return std::nullopt;
    }
    static const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    Original o;
    o.types = loaded.types;
    o.templates = anno::sim_load::scan_island_templates(*dir);
    o.dir = *dir;
    return o;
}

sim::ScenarioSpec scenario_spec(const Original& o, const std::string& relative) {
    auto resolved = anno::sim_load::load_scenario_with_links(
        anno::test::read_file(o.dir / "Scenes" / relative), o.templates, *o.types, o.cfg, {});
    REQUIRE(resolved);
    return resolved.value().loaded.spec;
}

sim::World make_world(const Original& o, const sim::ScenarioSpec& spec, bool live = true) {
    sim::Rules rules;
    rules.house_updates = live;
    auto created = sim::World::create(spec, o.types, o.cfg, rules);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    return std::move(created).value();
}

/// Everything one advance of the canary run is observed through: draws by site, the first step a
/// walker waits or carries cargo, the first transition.
struct Watch {
    std::map<sim::DrawSite, std::uint64_t> draws;
    std::optional<std::uint64_t> wait_step, cargo_step, transition_step;
    std::uint64_t steps = 0;
    std::vector<std::uint64_t> hashes; // every 10th step
};

void observe(const sim::World& w, const sim::TransportStats* stats, Watch& watch) {
    for (const sim::DrawRecord& r : w.last_output().draws) {
        watch.draws[r.site] += r.count;
    }
    ++watch.steps;
    if (stats != nullptr && !watch.transition_step && !stats->population.transitions.empty()) {
        watch.transition_step = watch.steps;
    }
    if (!watch.wait_step || !watch.cargo_step) {
        for (const auto& slot : w.figures().list.slots) {
            if (!slot || !slot->resident || slot->expiry_stamp) {
                continue;
            }
            const sim::ResidentState& r = *slot->resident;
            if (!watch.wait_step &&
                (r.step == sim::ResidentStep::Wait || r.step == sim::ResidentStep::ToStall)) {
                watch.wait_step = watch.steps;
            }
            const bool cargo =
                std::any_of(r.cargo.begin(), r.cargo.end(),
                            [](const sim::ResidentCargo& c) { return c.goods != 0; });
            if (!watch.cargo_step && cargo && r.step == sim::ResidentStep::ToHome) {
                watch.cargo_step = watch.steps;
            }
        }
    }
}

void step_world(sim::World& w, const sim::TransportStats* stats, Watch* watch) {
    w.advance(kStepMs);
    REQUIRE_MESSAGE(!w.faulted(), w.fault());
    if (watch != nullptr) {
        observe(w, stats, *watch);
        if (watch->steps % 10 == 0) {
            watch->hashes.push_back(w.state_hash());
        }
    }
}

/// One line per consumer goods and a verdict, appended to `out`.
void ledger_text(const sim::World& w, const sim::ResidentStats& r, std::ostringstream& out) {
    std::set<sim::GoodsId> goods;
    for (const auto* m :
         {&r.bought_by_goods, &r.taken_by_goods, &r.billed_by_goods, &r.delivered_by_goods,
          &r.merged_taken_by_goods, &r.merged_billed_by_goods, &r.merged_delivered_by_goods,
          &r.stock_primary_by_goods, &r.stock_replay_by_goods, &r.merge_dropped_by_goods,
          &r.lost_by_goods}) {
        for (const auto& [g, n] : *m) {
            goods.insert(g);
        }
    }
    const auto at = [](const std::map<sim::GoodsId, std::uint64_t>& m, sim::GoodsId g) {
        const auto it = m.find(g);
        return it == m.end() ? std::uint64_t{0} : it->second;
    };
    out << "ledger (8.8 amounts; stock units are x16):\n";
    for (const sim::GoodsId g : goods) {
        out << "  goods " << g << " id_sec " << w.types().goods_table()[g].id_sec << ": bought "
            << at(r.bought_by_goods, g) << " taken(job) " << at(r.taken_by_goods, g)
            << " taken(merge) " << at(r.merged_taken_by_goods, g) << " billed(job) "
            << at(r.billed_by_goods, g) << " billed(merge) " << at(r.merged_billed_by_goods, g)
            << " delivered(job) " << at(r.delivered_by_goods, g) << " delivered(merge) "
            << at(r.merged_delivered_by_goods, g) << " dropped " << at(r.merge_dropped_by_goods, g)
            << " lost " << at(r.lost_by_goods, g) << " stock_primary "
            << at(r.stock_primary_by_goods, g) << " stock_replay " << at(r.stock_replay_by_goods, g)
            << "\n";
    }
}

struct RunResult {
    std::string text;
    Watch watch;
    sim::HouseWork total, main_total;
    sim::ResidentStats main_res;
    std::size_t main_growth = 0, main_transitions = 0, main_unlocks = 0;
    std::size_t up = 0, down = 0, removed = 0, refused = 0;
};

void check_ledger(const sim::World& w, const sim::ResidentStats& r);

/// The canary on `relative`: `steps` advances with stats, then a drain on a copy (no new walker:
/// every empty walker slot is filled) until every walker has ended, then the ledger.
RunResult run_canary(const Original& o, const std::string& relative, std::uint64_t steps,
                     sim::TransportStats& stats) {
    RunResult res;
    const sim::ScenarioSpec spec = scenario_spec(o, relative);
    sim::World w = make_world(o, spec);
    REQUIRE(w.check_invariants() == "");
    w.set_transport_stats(&stats);
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint64_t i = 0; i < steps; ++i) {
        step_world(w, &stats, &res.watch);
        if (i % 250 == 249) {
            REQUIRE_MESSAGE(w.check_invariants() == "", w.check_invariants());
        }
    }
    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::ostringstream out;
    out << "file " << relative << " steps " << steps << " ticks " << w.clock().tick() << " wall "
        << secs << " s\n";
    // The drain: a copy in which every empty house walker slot holds a dummy id, so no house
    // spawns, runs until all walkers ended (their cargo is delivered or lost, the ledger closes).
    res.main_total = stats.population.total();
    res.main_res = stats.residents;
    res.main_growth = stats.population.growth.size();
    res.main_transitions = stats.population.transitions.size();
    res.main_unlocks = stats.population.unlocks.size();
    sim::World drain = w;
    drain.set_transport_stats(&stats); // the same sink: the ledger continues
    w.set_transport_stats(nullptr);
    const auto fill = [&drain] {
        for (auto& is : sim::WorldTestAccess::islands(drain)) {
            if (!is) {
                continue;
            }
            for (auto& c : is->cities) {
                if (!c) {
                    continue;
                }
                for (auto& l : c->index.lists) {
                    for (auto& b : l.slots) {
                        auto* h = b ? std::get_if<sim::House>(&b->payload) : nullptr;
                        if (h == nullptr) {
                            continue;
                        }
                        for (sim::FigureId& slot : h->walkers) {
                            if (!slot.valid()) {
                                slot = sim::FigureId{0xfffffff0ULL};
                            }
                        }
                    }
                }
            }
        }
    };
    fill();
    std::uint64_t drained = 0;
    const auto alive = [&] {
        for (const auto& slot : drain.figures().list.slots) {
            if (slot && slot->resident && !slot->expiry_stamp) {
                return true;
            }
        }
        return false;
    };
    while (alive() && drained < 2000) {
        fill(); // a type change can add empty walker slots
        drain.advance(kStepMs);
        REQUIRE_MESSAGE(!drain.faulted(), drain.fault());
        ++drained;
    }
    if (alive()) {
        for (const auto& slot : drain.figures().list.slots) {
            if (slot && slot->resident && !slot->expiry_stamp) {
                std::cout << "STUCK fig " << slot->id.v << " kind " << slot->kind << " step "
                          << static_cast<int>(slot->resident->step) << " wait "
                          << slot->resident->wait << " route " << slot->resident->route.size()
                          << " next " << slot->resident->next << " pos " << (slot->pos.x >> 8)
                          << "," << (slot->pos.y >> 8) << " home " << slot->home.v << " born "
                          << slot->born << " stamp " << slot->stamp << "\n";
            }
        }
    }
    REQUIRE_FALSE(alive());
    out << "drain steps " << drained << "\n";
    res.total = stats.population.total();
    for (const auto& t : stats.population.transitions) {
        using K = sim::HouseTransitionRecord::Kind;
        res.up += t.kind == K::Upgrade;
        res.down += t.kind == K::Downgrade;
        res.removed += t.kind == K::Remove;
        res.refused += t.kind == K::UpgradeRefused;
    }
    ledger_text(drain, stats.residents, out);
    check_ledger(drain, stats.residents);
    res.text = out.str();
    drain.set_transport_stats(nullptr);
    return res;
}

/// The ledger equations (docs/sim/population-live.md section 20).
void check_ledger(const sim::World& w, const sim::ResidentStats& r) {
    const auto at = [](const std::map<sim::GoodsId, std::uint64_t>& m, sim::GoodsId g) {
        const auto it = m.find(g);
        return it == m.end() ? std::uint64_t{0} : it->second;
    };
    std::set<sim::GoodsId> goods;
    for (const auto& [g, n] : r.bought_by_goods) {
        goods.insert(g);
    }
    CHECK_FALSE(goods.empty());
    for (const sim::GoodsId g : goods) {
        INFO("goods " << g);
        // 1. every bought amount ends delivered or lost
        CHECK(at(r.bought_by_goods, g) == at(r.delivered_by_goods, g) + at(r.lost_by_goods, g));
        // 2. the store a stall took is what the merge removed
        CHECK(at(r.taken_by_goods, g) == at(r.merged_taken_by_goods, g));
        CHECK(at(r.taken_by_goods, g) <= at(r.bought_by_goods, g));
        // 3. billing: the requested amount, equal to the bought one when nothing clips
        CHECK(at(r.billed_by_goods, g) == at(r.merged_billed_by_goods, g));
        // inside the billed range (the consumer goods) the requested amount is the bought one
        // when nothing clips; outside it nothing is billed (D 0x1002dac0)
        if (g >= w.config().billed_goods_first && g <= w.config().billed_goods_last) {
            CHECK(at(r.billed_by_goods, g) == at(r.bought_by_goods, g));
        } else {
            CHECK(at(r.billed_by_goods, g) == 0);
        }
        // 4. deliveries: handed over = applied + dropped at the merge
        CHECK(at(r.delivered_by_goods, g) ==
              at(r.merged_delivered_by_goods, g) + at(r.merge_dropped_by_goods, g));
        // 5. the house stock rose by 16 x the applied amount for every entry of the IdSec chain
        // the delivery walked (the goods' own slot and the replays; a substitute that shares the
        // slot adds to it twice, as the original's recursive deliver does)
        CHECK(at(r.stock_primary_by_goods, g) + at(r.stock_replay_by_goods, g) ==
              at(r.stock_expected_by_goods, g));
        CHECK(at(r.stock_expected_by_goods, g) >= 16 * at(r.merged_delivered_by_goods, g));
        // 6. a goods without an IdSec: exactly one add, no replay
        if (w.types().goods_table()[g].id_sec == 0) {
            CHECK(at(r.stock_replay_by_goods, g) == 0);
            CHECK(at(r.stock_expected_by_goods, g) == 16 * at(r.merged_delivered_by_goods, g));
            CHECK(at(r.stock_primary_by_goods, g) == 16 * at(r.merged_delivered_by_goods, g));
        }
    }
}

} // namespace

TEST_CASE("population live canary: bounded scenario search table (ANNO_POP_LIVE_SEARCH)") {
    const char* want = std::getenv("ANNO_POP_LIVE_SEARCH");
    if (want == nullptr || *want == '\0') {
        return; // a one-off tool, its result is pinned in the canary below and in the doc
    }
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    const std::uint64_t budget = std::strtoull(want, nullptr, 10);
    const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    const auto types = loaded.types;
    const sim::SimConfig cfg;
    const anno::sim_load::IslandTemplates templates = anno::sim_load::scan_island_templates(*dir);
    std::vector<fs::path> files;
    for (const auto& e : fs::recursive_directory_iterator(*dir / "Scenes")) {
        if (e.is_regular_file() && e.path().extension() == ".sze") {
            files.push_back(e.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
        auto resolved = anno::sim_load::load_scenario_with_links(anno::test::read_file(file),
                                                                 templates, *types, cfg, {});
        if (!resolved) {
            continue;
        }
        auto created = sim::World::create(resolved.value().loaded.spec, types, cfg, live_rules());
        if (!created.ok()) {
            std::cout << "SEARCH " << file.filename().string()
                      << " create refused: " << created.error() << "\n";
            continue;
        }
        sim::World w = std::move(created).value();
        const std::size_t houses = count_houses(w);
        if (houses == 0) {
            continue;
        }
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        const auto t0 = std::chrono::steady_clock::now();
        for (std::uint64_t t = 0; t < budget && !w.faulted(); t += 50) {
            w.advance(100);
        }
        const double secs =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const sim::HouseWork tot = stats.population.total();
        std::size_t up = 0, down = 0, rem = 0, refused = 0;
        for (const auto& t : stats.population.transitions) {
            using K = sim::HouseTransitionRecord::Kind;
            up += t.kind == K::Upgrade;
            down += t.kind == K::Downgrade;
            rem += t.kind == K::Remove;
            refused += t.kind == K::UpgradeRefused;
        }
        const auto& r = stats.residents;
        std::cout << "SEARCH " << file.parent_path().filename().string() << "/"
                  << file.filename().string() << " houses=" << houses
                  << " ticks=" << w.clock().tick() << " visits=" << tot.visits
                  << " taken=" << tot.kernel_taken << " spawn_draws=" << tot.spawn_draws
                  << " searches=" << r.searches << " failed=" << r.searches_failed
                  << " purchases=" << r.purchases << " bought=" << r.bought_88
                  << " deliveries=" << r.deliveries << " growth=" << stats.population.growth.size()
                  << " up=" << up << " down=" << down << " remove=" << rem << " refused=" << refused
                  << " unlocks=" << stats.population.unlocks.size()
                  << " faulted=" << (w.faulted() ? w.fault() : "no") << " secs=" << secs << "\n";
        if (w.faulted()) {
            const std::string text = w.fault();
            const auto at = text.find("building ");
            if (at != std::string::npos) {
                const sim::BuildingId bid{std::strtoull(text.c_str() + at + 9, nullptr, 10)};
                const sim::Island* is = w.islands()[sim::island_of(bid).v].has_value()
                                            ? &*w.islands()[sim::island_of(bid).v]
                                            : nullptr;
                const sim::Building* b = is != nullptr ? is->find_building(bid) : nullptr;
                if (b != nullptr) {
                    const sim::BuildingType* t = types->find(b->type);
                    const sim::TypeLadder lad = sim::house_type_ladder(cfg, *types, *t);
                    const auto* hp = std::get_if<sim::House>(&b->payload);
                    std::cout << "SEARCH   ladder next=" << (lad.next ? lad.next->id.v : 0)
                              << " prev=" << (lad.previous ? lad.previous->id.v : 0)
                              << " next_prod=" << (lad.next ? lad.next->prodtype : 0)
                              << " prev_prod=" << (lad.previous ? lad.previous->prodtype : 0)
                              << " res64=" << (hp ? hp->residents_64 : -1)
                              << " e=" << (hp ? hp->e : 0)
                              << " stock=" << (hp ? hp->stock.size() : 0)
                              << " group=" << sim::house_group(cfg, t->prodtype).value_or(99)
                              << "\n";
                    std::cout << "SEARCH   fault building type " << b->type.v << " prodtype "
                              << (t != nullptr ? t->prodtype : 0U) << " at " << b->x << "," << b->y
                              << "\n";
                }
            }
        }
        w.set_transport_stats(nullptr);
    }
}

namespace {

constexpr const char* kPrimary = "0100Kampagne01/S01_01_01.sze";
constexpr const char* kDowngrade = "0100Kampagne01/S01_07_01.sze";
constexpr std::uint64_t kPrimarySteps = 3000;   // 150 000 ticks = 300 s of game time
constexpr std::uint64_t kDowngradeSteps = 1830; // 91 500 ticks: just past the first mass downgrade

void print_run(const RunResult& r, const sim::TransportStats& stats) {
    std::cout << r.text;
    std::cout << "draws:";
    for (const auto& [site, n] : r.watch.draws) {
        std::cout << " " << sim::draw_site_name(site) << "=" << n;
    }
    std::cout << "\nfirst walker in ToStall/Wait: step " << r.watch.wait_step.value_or(0)
              << ", first walker carrying cargo home: step " << r.watch.cargo_step.value_or(0)
              << ", first transition: step " << r.watch.transition_step.value_or(0) << "\n";
    std::cout << "MAIN res: searches " << r.main_res.searches << " failed "
              << r.main_res.searches_failed << " arrivals " << r.main_res.arrivals << " purchases "
              << r.main_res.purchases << " bought_88 " << r.main_res.bought_88 << " deliveries "
              << r.main_res.deliveries << " delivered_88 " << r.main_res.delivered_88 << " lost "
              << r.main_res.lost_88 << " no_path " << r.main_res.no_path_home << " adapted "
              << r.main_res.endpoint_adapted << " rejected " << r.main_res.reverse_rejected
              << " faults " << r.main_res.faults << " growth " << r.main_growth << " transitions "
              << r.main_transitions << " unlocks " << r.main_unlocks << "\n";
    for (const auto& t : stats.population.transitions) {
        std::cout << "transition kind " << static_cast<int>(t.kind) << " house " << t.building.v
                  << " tick " << t.tick << " type " << t.from.v << "->" << t.to.v << " groups "
                  << t.old_group << "->" << t.new_group << " res64 " << t.residents_before << "->"
                  << t.residents_after << " houses_old " << t.houses_old_before << "->"
                  << t.houses_old_after << " houses_new " << t.houses_new_before << "->"
                  << t.houses_new_after << " counter_old " << t.counter_old_before << "->"
                  << t.counter_old_after << " counter_new " << t.counter_new_before << "->"
                  << t.counter_new_after << " variant " << t.variant_drawn << "/" << t.variant_draw
                  << "/" << t.variant_index << " spent";
        for (const std::int64_t v : t.spent_by_good) {
            std::cout << " " << v;
        }
        std::cout << "\n";
    }
    for (const auto& u : stats.population.unlocks) {
        std::cout << "unlock tick " << u.tick << " city " << u.city.island.v << "." << u.city.city.v
                  << " stage " << u.stage << " group " << u.group << " count " << u.count
                  << " have " << u.residents_at_or_above << " city_mask " << u.city_mask_before
                  << "->" << u.city_mask_after << " player " << u.player_flags_before << "->"
                  << u.player_flags_after << "\n";
    }
}

/// The unlock rule of section 6/5a for every record: the counter reached the count, the city bit
/// is new, the player value follows the 23-bit compare and player_flags_set.
void check_unlocks(const sim::TransportStats& stats) {
    // the player value is stored once per pass: the records of one (tick, city) share it, and the
    // rule is applied to the city mask the pass ended with
    std::map<std::pair<std::uint32_t, sim::CityRef>, std::uint32_t> final_mask;
    for (const auto& u : stats.population.unlocks) {
        std::uint32_t& m = final_mask[{u.tick, u.city}];
        m = std::max(m, u.city_mask_after);
    }
    for (const auto& u : stats.population.unlocks) {
        INFO("stage " << u.stage << " tick " << u.tick);
        CHECK(u.residents_at_or_above >= static_cast<std::int64_t>(u.count));
        CHECK((u.city_mask_before & (std::uint32_t{1} << u.stage)) == 0);
        CHECK((u.city_mask_after & (std::uint32_t{1} << u.stage)) != 0);
        const std::uint32_t mask = final_mask.at({u.tick, u.city});
        const std::uint32_t want = (u.player_flags_before & 0x7fffff) < mask
                                       ? sim::player_flags_set(u.player_flags_before | mask)
                                       : u.player_flags_before;
        CHECK(u.player_flags_after == want);
    }
}

/// Growth records: the step moved residents by the group's table entry unless a cap or floor
/// intervened; counts both kinds.
void check_growth(const sim::TransportStats& stats, std::size_t& exact, std::size_t& clipped) {
    exact = clipped = 0;
    for (const auto& g : stats.population.growth) {
        REQUIRE(g.residents_after != g.residents_before);
        const bool is_exact = g.residents_after == g.residents_before + g.vermehr;
        (is_exact ? exact : clipped) += 1;
        if (!is_exact) {
            // F2: every claimed clip is checked against its applicable bound: the cap (the step
            // would have crossed Wohnmax * 64 and the kernel stops at it) or the below-64
            // correction (the floor 64 of the downgrade arm).
            const bool cap_clip =
                g.residents_before + g.vermehr > g.cap_64 && g.residents_after == g.cap_64;
            // K2 (review P3): the floor applies only when the step really fell below 64.
            const bool floor_clip = g.residents_before + g.vermehr < 64 && g.residents_after == 64;
            CHECK_MESSAGE((cap_clip || floor_clip), "unexplained delta of house " << g.building.v);
        }
        // The kernel number is what the House holds unless the tick removed the house.
        CHECK(
            (g.removed ? g.residents_committed == 0 : g.residents_committed == g.residents_after));
    }
}

} // namespace

TEST_CASE("population live canary: S01_01_01 live run, ledger, growth, removal and stage unlock") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::TransportStats stats;
    const RunResult r = run_canary(*o, kPrimary, kPrimarySteps, stats);
    print_run(r, stats);
    if (const char* dir = std::getenv("ANNO_POP_LIVE_OUT_DIR")) {
        const std::string e = sim::test::write_evidence_file(
            (fs::path(dir) / "canary_S01_01_01.txt").string(), r.text);
        REQUIRE_MESSAGE(e.empty(), e);
    }
    // (a) the real supply chain, counted (pinned to this build; any change of the House path or of
    // the scenario shows here first).
    CHECK(r.main_res.faults == 0);
    CHECK(r.main_res.lost_88 == 0); // nothing is lost any more: every walker finds its way home
    CHECK(r.main_total.visits == 18784);
    CHECK(r.main_total.kernel_taken == 2629);
    CHECK(r.main_total.spawn_draws == 1963);
    CHECK(r.main_total.spawn_requests == 1963);
    CHECK(r.watch.draws.at(sim::DrawSite::HouseWalkerTarget) == 1963); // draws by site = counter
    CHECK(r.watch.draws.count(sim::DrawSite::HouseVariant) == 0);      // no upgrade or downgrade
    CHECK(r.main_res.searches == 1953);
    CHECK(r.main_res.searches_failed == 1743);
    CHECK(r.main_res.arrivals == 193);
    CHECK(r.main_res.purchases == 137);
    CHECK(r.main_res.bought_88 == 4339); // the city store the stalls gave up
    CHECK(r.main_res.deliveries == 136);
    CHECK(r.main_res.delivered_88 == 7957);
    CHECK(r.main_res.no_path_home == 0); // the resident-specific home path (section 20.2)
    // P-15 (C3): every reversed route passed the forward replay; 8 needed the endpoint adapter
    // (leaving the stand / entering the home), none was refused (measured, S01_01_01).
    CHECK(r.main_res.reverse_rejected == 0);
    CHECK(r.main_res.endpoint_adapted == 8);
    CHECK(r.main_total.rect_queries == r.main_res.searches);
    CHECK(r.main_total.candidates_scored == 5676);
    CHECK(r.main_total.candidates_accepted == 692);
    CHECK(r.main_total.walk_searches == 479);
    CHECK(r.main_total.walk_expansions == 371287);
    CHECK(r.main_total.upgrade_scores == 0); // no house reached its cap
    // the ledger (closed by the drain; the equations are checked in run_canary): the stall goods
    // came out of the city store and the public goods did not
    CHECK(stats.residents.taken_by_goods.at(26) == stats.residents.bought_by_goods.at(26));
    CHECK(stats.residents.taken_by_goods.count(82) == 0);
    CHECK(stats.residents.bought_by_goods.at(82) > 0);
    // (b) growth: every record moved the residents; the table value explains each exact step
    std::size_t exact = 0, clipped = 0;
    check_growth(stats, exact, clipped);
    std::cout << "growth records " << stats.population.growth.size() << " exact " << exact
              << " clipped " << clipped << "\n";
    CHECK(r.main_growth == 171);
    CHECK(exact >= 1);
    const sim::HouseGrowthRecord& g0 = stats.population.growth.front();
    CHECK(g0.building.v == 133143986195ULL);
    CHECK(g0.tick == 2500);
    CHECK(g0.mood == -2);
    CHECK(g0.vermehr == -16);
    CHECK(g0.residents_before == 64);
    CHECK(g0.residents_after == 48);
    // (c) removals: floor 64 reached, the first one is exactly the house of growth record 0
    CHECK(r.up == 0);
    CHECK(r.down == 0);
    CHECK(r.removed == 10);
    CHECK(r.main_transitions == 10);
    const sim::HouseTransitionRecord& t0 = stats.population.transitions.front();
    CHECK(t0.kind == sim::HouseTransitionRecord::Kind::Remove);
    CHECK(t0.building.v == 133143986195ULL);
    CHECK(t0.tick == 2500);
    CHECK(t0.from.v == 3205);
    CHECK(t0.old_group == 0);
    CHECK(t0.residents_before == 64);
    CHECK(t0.houses_old_before == 13);
    CHECK(t0.houses_old_after == 12);
    CHECK(t0.counter_old_before == 67);
    CHECK(t0.counter_old_after == 66);
    CHECK_FALSE(t0.variant_drawn);
    // the stage unlock: city bits only (the player's value already holds them: the 23-bit quirk)
    CHECK(r.main_unlocks == 10);
    check_unlocks(stats);
    const sim::StageUnlockRecord& u0 = stats.population.unlocks.front();
    CHECK(u0.tick == 2500);
    CHECK(u0.city.island.v == 16);
    CHECK(u0.city.city.v == 0);
    CHECK(u0.stage == 0);
    CHECK(u0.city_mask_after == 1);
    CHECK(u0.player_flags_before == 1761607711U);
    CHECK(u0.player_flags_after == 1761607711U);
    // cut markers used by the save/reload case
    CHECK(r.watch.wait_step.value_or(0) == 50);
    CHECK(r.watch.cargo_step.value_or(0) == 127);
    CHECK(r.watch.transition_step.value_or(0) == 50);
}

TEST_CASE("population live canary: S01_07_01 live run, ledger, downgrade with variant draws") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::TransportStats stats;
    const RunResult r = run_canary(*o, kDowngrade, kDowngradeSteps, stats);
    print_run(r, stats);
    if (const char* dir = std::getenv("ANNO_POP_LIVE_OUT_DIR")) {
        const std::string e = sim::test::write_evidence_file(
            (fs::path(dir) / "canary_S01_07_01.txt").string(), r.text);
        REQUIRE_MESSAGE(e.empty(), e);
    }
    CHECK(r.main_res.faults == 0);
    CHECK(r.main_res.lost_88 == 0);
    CHECK(r.main_res.reverse_rejected == 0);   // P-15 (C3): no reversed route refused by the replay
    CHECK(r.main_res.endpoint_adapted == 142); // accepted with the endpoint adapter (measured)
    CHECK(r.main_total.spawn_draws == 2278);
    CHECK(r.watch.draws.at(sim::DrawSite::HouseWalkerTarget) == 2278);
    CHECK(r.main_res.searches == 2278);
    CHECK(r.main_res.purchases == 619);
    CHECK(r.main_res.bought_88 == 36430);
    CHECK(r.main_res.deliveries == 548);
    CHECK(r.main_res.delivered_88 == 102897);
    CHECK(r.main_total.candidates_scored == 20928);
    CHECK(r.main_total.walk_expansions == 1727788);
    CHECK(r.main_total.upgrade_scores > 0); // caps were crossed; no house was eligible to upgrade
    CHECK(r.up == 1);                       // K2: the one upgrade below
    CHECK(r.refused == 0);
    CHECK(r.down == 14);
    CHECK(r.main_transitions == 9);
    // a downgrade draws exactly one HouseVariant each
    CHECK(r.watch.draws.at(sim::DrawSite::HouseVariant) == 9);
    // K2: the first REAL pioneer -> settler upgrade of an original scenario. With the goal cells
    // carrying their key nibble (OQ-463) the isolated-cell stands (1314 leather and cloth, ...) are
    // reachable, the Inn supplies the tavern, the chapel the church need, and PIONIER house 509
    // (a mapper-0 free-cell type) upgrades at tick 72550.
    const auto& trs = stats.population.transitions;
    const auto up_it =
        std::find_if(trs.begin(), trs.end(), [](const sim::HouseTransitionRecord& x) {
            return x.kind == sim::HouseTransitionRecord::Kind::Upgrade;
        });
    REQUIRE(up_it != trs.end());
    CHECK(up_it->building.v == 227633266728ULL);
    CHECK(up_it->tick == 72550);
    CHECK(up_it->from.v == 509);
    CHECK(up_it->to.v ==
          526); // next type id - id % 20 + 20, plus the variant index (6 at 130, 2 at 129)
    CHECK(up_it->old_group == 0);
    CHECK(up_it->new_group == 1);
    CHECK(up_it->residents_before == 512);
    CHECK(up_it->residents_after == 512);
    CHECK(up_it->houses_old_before == 2);
    CHECK(up_it->houses_old_after == 1);
    CHECK(up_it->houses_new_before == 0);
    CHECK(up_it->houses_new_after == 1);
    CHECK(up_it->counter_old_before == 536);
    CHECK(up_it->counter_old_after == 536);
    CHECK(up_it->counter_new_before == 520); // 528 - the house's 8 (derived, diagnostic only)
    CHECK(up_it->counter_new_after == 528);
    CHECK(up_it->variant_drawn);
    CHECK(up_it->variant_draw == 29350);
    CHECK(up_it->variant_index == 6);
    // the exact material debit of type 520, read from the registry (not hardcoded): the type's cost
    // entries, nothing else
    {
        const sim::BuildingType* next = o->types->find(up_it->to);
        REQUIRE(next != nullptr);
        std::vector<std::int64_t> want(up_it->spent_by_good.size(), 0);
        for (const sim::CostEntry& e : next->cost_goods) {
            REQUIRE(e.goods >= o->cfg.build_goods_first);
            REQUIRE(e.goods - o->cfg.build_goods_first < want.size());
            want[e.goods - o->cfg.build_goods_first] += e.amount_88;
        }
        CHECK(up_it->spent_by_good == want);
        CHECK(want[0] == 256);  // WERKZEUG 1 t, the value this equals today
        CHECK(want[1] == 1024); // HOLZ 4 t
    }
    // exact identity of the first downgrade
    const auto dn_it =
        std::find_if(trs.begin(), trs.end(), [](const sim::HouseTransitionRecord& x) {
            return x.kind == sim::HouseTransitionRecord::Kind::Downgrade;
        });
    REQUIRE(dn_it != trs.end());
    const sim::HouseTransitionRecord& t = *dn_it;
    CHECK(t.building.v == 219043332203ULL);
    CHECK(t.tick == 90250);
    CHECK(t.from.v == 560);
    CHECK(t.to.v == 545);
    CHECK(t.old_group == 3);
    CHECK(t.new_group == 2);
    CHECK(t.residents_before == 1358);
    // F2: the House holds 1320 after the tick (the growth step of the group-3 mood -2 entry,
    // VermehrTab -38, ran BEFORE SetGroup, which keeps the residents).
    CHECK(t.residents_after == 1358 - 38);
    // The pins the K2 re-record had dropped (review P2), re-measured on the new deterministic run:
    // the first downgrade is still house ...203 at tick 90250 (560 -> 542), the counters and the
    // variant draw moved with the changed purchases (houses_old 54 -> 53 as before; counter_old
    // 1690/1669 -> 1665/1644; HouseVariant draw 27446 -> 1338, index 6 -> 2, because the draws
    // before it differ).
    CHECK(t.houses_old_before == 54);
    CHECK(t.houses_old_after == 53);
    CHECK(t.houses_new_before == 0);
    CHECK(t.houses_new_after == 1);
    CHECK(t.counter_old_before == 1670);
    CHECK(t.counter_old_after == 1649);
    CHECK(t.counter_new_before ==
          1670); // 1670 - (20 - 21): SetGroup took the house from 1358 to 1320
    CHECK(t.counter_new_after == 1669);
    CHECK(t.variant_drawn);
    CHECK(t.variant_draw == 14989);
    CHECK(t.variant_index == 5);
    CHECK(std::all_of(t.spent_by_good.begin(), t.spent_by_good.end(),
                      [](std::int64_t v) { return v == 0; })); // a downgrade spends nothing
    for (const auto& tr : trs) {
        if (tr.kind == sim::HouseTransitionRecord::Kind::Upgrade) {
            CHECK(tr.new_group == tr.old_group + 1);
        } else {
            CHECK(tr.kind == sim::HouseTransitionRecord::Kind::Downgrade);
            CHECK(tr.new_group + 1 == tr.old_group);
        }
        CHECK(tr.variant_drawn);
    }
    check_unlocks(stats);
    CHECK(r.main_unlocks == 71);
}

namespace {

/// A world at the start of the primary file, run to the end by single steps; the hash list of
/// every tenth step is the reference.
struct Reference {
    std::vector<std::uint64_t> hashes;
    std::uint64_t final_hash = 0;
    std::vector<std::uint64_t> transition_steps; // step numbers (1-based) of every transition
};

} // namespace

TEST_CASE("population live canary: save and reload at the cuts of the real run") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::ScenarioSpec spec = scenario_spec(*o, kPrimary);
    // Reference: uninterrupted, with the stats sink to find the steps of the cuts.
    Reference ref;
    {
        sim::World w = make_world(*o, spec);
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        Watch watch;
        std::size_t seen = 0;
        for (std::uint64_t i = 0; i < kPrimarySteps; ++i) {
            step_world(w, &stats, &watch);
            if (stats.population.transitions.size() != seen) {
                seen = stats.population.transitions.size();
                ref.transition_steps.push_back(watch.steps);
            }
        }
        ref.hashes = watch.hashes;
        ref.final_hash = w.state_hash();
        w.set_transport_stats(nullptr);
        CHECK(watch.wait_step.value_or(0) == 50);
        CHECK(watch.cargo_step.value_or(0) == 127);
        REQUIRE(ref.transition_steps.size() ==
                6); // measured at 130: steps 50, 52, 54, 56, 1806, 2151 (seven at 129)
        // a walker waiting or walking to a stand at the end of step 50, a walker carrying cargo
        // at the end of step 127, the first transition inside step 50, a later one inside step 1806
        CHECK(ref.transition_steps.front() == 50);
        CHECK(std::count(ref.transition_steps.begin(), ref.transition_steps.end(), 1806U) == 1);
    }
    // The cuts: before and after the first transition (49, 50: step 50 also holds a walker in
    // ToStall), a walker carrying cargo (127), before and after a late transition (1805, 1806).
    const std::set<std::uint64_t> cuts{49, 50, 127, 1805, 1806};
    sim::World w = make_world(*o, spec);
    Watch watch;
    std::size_t reloads = 0;
    for (std::uint64_t i = 0; i < kPrimarySteps; ++i) {
        step_world(w, nullptr, &watch);
        if (cuts.count(watch.steps) != 0) {
            // the cut state contains what the name says
            if (watch.steps == 50 || watch.steps == 127) {
                bool wait = false, cargo = false;
                for (const auto& slot : w.figures().list.slots) {
                    if (!slot || !slot->resident || slot->expiry_stamp) {
                        continue;
                    }
                    wait = wait || slot->resident->step == sim::ResidentStep::ToStall ||
                           slot->resident->step == sim::ResidentStep::Wait;
                    cargo = cargo ||
                            (slot->resident->step == sim::ResidentStep::ToHome &&
                             std::any_of(slot->resident->cargo.begin(), slot->resident->cargo.end(),
                                         [](const sim::ResidentCargo& c) { return c.goods != 0; }));
                }
                CHECK((watch.steps == 50 ? wait : cargo));
            }
            const std::uint64_t before = w.state_hash();
            const std::vector<std::uint8_t> bytes = w.save();
            auto back = sim::World::load(bytes, o->types);
            REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
            CHECK(back.value().state_hash() == before);
            CHECK(back.value().save() == bytes);
            w = std::move(back).value();
            ++reloads;
        }
    }
    CHECK(reloads == cuts.size());
    // every tenth step after every reload matches the uninterrupted run, and so does the end
    REQUIRE(watch.hashes.size() == ref.hashes.size());
    CHECK(watch.hashes == ref.hashes);
    CHECK(w.state_hash() == ref.final_hash);
    std::cout << "reload cuts " << reloads << ", final hash " << std::hex << ref.final_hash
              << std::dec << ", hashes compared " << ref.hashes.size() << "\n";
}

namespace {

std::string hex64(std::uint64_t v) {
    std::ostringstream o;
    o << std::hex << v;
    return o.str();
}

void export_log(const char* name, const sim::ReplayLog& log) {
    const char* dir = std::getenv("ANNO_POP_LIVE_OUT_DIR");
    if (dir == nullptr || *dir == '\0') {
        return;
    }
    const auto bytes = sim::write_log(log);
    REQUIRE(bytes.ok());
    const std::string e = sim::test::write_evidence_file(
        (fs::path(dir) / name).string(),
        std::string_view(reinterpret_cast<const char*>(bytes.value().data()),
                         bytes.value().size()));
    REQUIRE_MESSAGE(e.empty(), e);
}

} // namespace

TEST_CASE("population live canary: recorded log replays, and a changed log or input diverges") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::ScenarioSpec spec = scenario_spec(*o, kPrimary);
    constexpr std::uint64_t kSteps = 400; // 20 000 ticks: past the first removals and walkers
    sim::World w = make_world(*o, spec);
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    for (std::uint64_t i = 0; i < kSteps; ++i) {
        rec.advance(kStepMs);
        if (i % 40 == 39) {
            rec.checkpoint();
        }
    }
    const sim::ReplayLog& log = rec.log();
    std::size_t checkpoints = 0;
    for (const sim::ReplayEntry& e : log.entries) {
        checkpoints += e.kind == sim::ReplayEntry::Kind::Checkpoint ? 1U : 0U;
    }
    REQUIRE(checkpoints == 10);
    // the header carries the rule: House walker and variant sites are not in the missing list
    CHECK(std::find(log.prng_timeline_missing.begin(), log.prng_timeline_missing.end(),
                    "HouseWalkerTarget") == log.prng_timeline_missing.end());
    CHECK(std::find(log.prng_timeline_missing.begin(), log.prng_timeline_missing.end(),
                    "HouseFirstAssignment") == log.prng_timeline_missing.end()); // C2: ported
    CHECK(log.rules_digest == w.rules().digest());
    export_log("pop_live.log", log);

    // 1. through bytes, in a fresh world
    const auto bytes = sim::write_log(log);
    REQUIRE(bytes.ok());
    const auto read = sim::read_log(bytes.value());
    REQUIRE(read.ok());
    {
        auto fresh = sim::World::load(read.value().initial_save, o->types, {});
        REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
        auto outcome = sim::replay(fresh.value(), read.value());
        REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
        CHECK(outcome.value().checkpoints_total == 10);
        CHECK(outcome.value().checkpoints_matched == 10);
        CHECK(outcome.value().reproduced());
        CHECK(fresh.value().state_hash() == w.state_hash());
    }
    // 2. one changed byte of the log: an Advance entry by 1 ms. The two files differ in one byte.
    {
        sim::ReplayLog changed = read.value();
        std::size_t index = 0;
        for (std::size_t i = 0; i < changed.entries.size(); ++i) {
            if (changed.entries[i].kind == sim::ReplayEntry::Kind::Advance && i > 100) {
                index = i;
                break;
            }
        }
        REQUIRE(index != 0);
        changed.entries[index].ms += 1;
        const auto a = sim::write_log(read.value());
        const auto b = sim::write_log(changed);
        REQUIRE((a.ok() && b.ok() && a.value().size() == b.value().size()));
        std::size_t differing = 0;
        for (std::size_t i = 0; i < a.value().size(); ++i) {
            differing += a.value()[i] != b.value()[i] ? 1U : 0U;
        }
        CHECK(differing >= 1);
        CHECK(differing <= 2); // the ms field and (when its tick changed) nothing else of the frame
        auto fresh = sim::World::load(changed.initial_save, o->types, {});
        REQUIRE(fresh.ok());
        auto outcome = sim::replay(fresh.value(), changed);
        REQUIRE(outcome.ok());
        REQUIRE_FALSE(outcome.value().reproduced());
        std::cout << "one-byte log change at entry " << index << ": diverged at entry "
                  << *outcome.value().diverged_entry << " (expected "
                  << hex64(outcome.value().expected_hash) << ", got "
                  << hex64(outcome.value().actual_hash) << ")\n";
        CHECK(*outcome.value().diverged_entry > index);
        export_log("pop_live_bytechanged.log", changed);
    }
    // 3. a changed starting input: one house's food stock raised by one unit (16 stock units)
    {
        sim::World other = make_world(*o, spec);
        bool done = false;
        for (auto& is : sim::WorldTestAccess::islands(other)) {
            if (!is) {
                continue;
            }
            for (auto& c : is->cities) {
                if (!c) {
                    continue;
                }
                for (auto& l : c->index.lists) {
                    for (auto& b : l.slots) {
                        auto* h = b ? std::get_if<sim::House>(&b->payload) : nullptr;
                        if (h != nullptr && !h->stock.empty() && !done) {
                            h->stock[0] += 16;
                            done = true;
                        }
                    }
                }
            }
        }
        REQUIRE(done);
        sim::ReplayLog changed = read.value();
        changed.initial_save = other.save();
        auto fresh = sim::World::load(changed.initial_save, o->types, {});
        REQUIRE(fresh.ok());
        auto outcome = sim::replay(fresh.value(), changed);
        REQUIRE(outcome.ok());
        REQUIRE_FALSE(outcome.value().reproduced());
        std::cout << "changed house stock: diverged at entry " << *outcome.value().diverged_entry
                  << "\n";
        CHECK(*outcome.value().diverged_entry <= 80);
        export_log("pop_live_stockchanged.log", changed);
    }
    // 4. Rules::house_updates off in the starting input
    {
        sim::World off = make_world(*o, spec, false);
        sim::ReplayLog changed = read.value();
        changed.initial_save = off.save();
        auto fresh = sim::World::load(changed.initial_save, o->types, {});
        REQUIRE(fresh.ok());
        auto refused = sim::replay(fresh.value(), changed);
        REQUIRE_FALSE(refused.ok()); // the rules digest of the log names another world
        std::cout << "rule off, header unchanged: " << refused.error() << "\n";
        CHECK(refused.error().find("replay refused") != std::string::npos);
        // with a header that matches the rule-off world the replay runs and diverges at once
        changed.rules_digest = off.rules().digest();
        changed.prng_timeline_missing = sim::prng_timeline_missing(false);
        auto fresh2 = sim::World::load(changed.initial_save, o->types, {});
        REQUIRE(fresh2.ok());
        auto outcome = sim::replay(fresh2.value(), changed);
        REQUIRE(outcome.ok());
        REQUIRE_FALSE(outcome.value().reproduced());
        std::cout << "rule off, matching header: diverged at entry "
                  << *outcome.value().diverged_entry << "\n";
        CHECK(*outcome.value().diverged_entry <= 80);
        export_log("pop_live_ruleoff.log", changed);
    }
}

TEST_CASE("population live P-14: the successor classes of the shipped house types") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::SimConfig cfg;
    std::map<int, std::size_t> count;
    std::set<std::uint32_t> special;
    for (const sim::BuildingType& t : o->types->types()) {
        if (t.prodtype < 97 || t.prodtype > 128) {
            continue;
        }
        const sim::HouseSuccessor c = sim::house_successor(cfg, *o->types, t);
        ++count[static_cast<int>(c)];
        if (c == sim::HouseSuccessor::Special) {
            special.insert(t.id.v);
            CHECK(t.special_successor); // declared by the original loader
        } else {
            CHECK_FALSE(t.special_successor);
        }
        if (c == sim::HouseSuccessor::Ladder) {
            const sim::TypeLadder l = sim::house_type_ladder(cfg, *o->types, t);
            REQUIRE(l.next != nullptr);
            CHECK(l.next->width ==
                  t.width); // a ladder pair never lands in Special by its footprint
            CHECK(l.next->height == t.height);
        }
    }
    CHECK(count[static_cast<int>(sim::HouseSuccessor::Ladder)] == 30);      // 501-513 ... 560-567
    CHECK(count[static_cast<int>(sim::HouseSuccessor::EndOfLadder)] == 39); // 33 scenery + 580-585
    CHECK(count[static_cast<int>(sim::HouseSuccessor::Special)] == 12);
    CHECK(count[static_cast<int>(sim::HouseSuccessor::Malformed)] == 0);
    // The audited irregular families (docs 21.3): 3851-3854 -> 3860 (PRODTYPE 289), 3951-3954 ->
    // 3960 (a group-0 house), 3960 / 3969 / 3972 / 3979 -> 3980 (a stall). 3960 is both a successor
    // and itself irregular: the declaration is per type.
    CHECK(special == std::set<std::uint32_t>{3851, 3852, 3853, 3854, 3951, 3952, 3953, 3954, 3960,
                                             3969, 3972, 3979});
    // The world accepts the registry with house updates on (every type is classified).
    CHECK(sim::house_updates_problem(
              cfg,
              [] {
                  sim::Rules r;
                  r.house_updates = true;
                  return r;
              }(),
              *o->types)
              .empty());
}

// ------------------------------------------------------------------ MAP-POP-CLOSE-1 C4 (PARTIAL)
//
// A fresh home in a real original scenario under an explicitly enabled headless setup
// (Rules::house_updates on in this test only; the global default stays false). The upgrade of a
// pioneer house to settlers is NOT reached here: it needs the forced infrastructure need WIRTSHAUS
// (76) satisfied above the InfraTab threshold, and the only supplier of goods 76 is an Inn with a
// tavern stock; no shipped scenario carries one (docs/sim/population-live.md 21.7). Nothing here
// manufactures a stock to get past that.

namespace {

constexpr const char* kClose = "0011SingleEndlosAddon/endless03.sze";
constexpr std::uint32_t kCloseOwner = 4; // flags 0x1 in the scenario setup, 28 houses of its own
constexpr std::int32_t kCloseX = 303,
                       kCloseY = 234;   // a free 4 x 4 site beside the city's first stall
constexpr sim::TypeId kCloseHouse{501}; // PIONIER, PRODTYPE 97, group 0, Wohnmax 8
constexpr std::uint64_t kCloseSteps =
    1000; // 50 000 ticks: the first delivery needs food below 116 %

struct Close {
    sim::World w;
    sim::Island* island = nullptr;
    sim::City* city = nullptr;
    sim::CityRef ref;
};

Close close_world(const Original& o, const sim::ScenarioSpec& spec) {
    Close c{make_world(o, spec), nullptr, nullptr, {}};
    for (auto& isl : sim::WorldTestAccess::islands(c.w)) {
        if (!isl) {
            continue;
        }
        for (auto& city : isl->cities) {
            if (city && city->owner.v == kCloseOwner) {
                bool houses = false;
                for (const auto& l : city->index.lists) {
                    for (const auto& b : l.slots) {
                        houses = houses || (b && std::holds_alternative<sim::House>(b->payload));
                    }
                }
                if (houses) {
                    c.island = &*isl;
                    c.city = &*city;
                }
            }
        }
    }
    REQUIRE(c.city != nullptr);
    c.ref = sim::CityRef{c.island->index, c.city->number};
    return c;
}

sim::Command close_build(const Close& c, std::uint8_t rotation = 0) {
    return sim::Command{
        c.city->owner,
        sim::BuildCmd{c.ref, {sim::PlacementWire{kCloseHouse, kCloseX, kCloseY, rotation, 0, 0}}}};
}

const sim::Building* close_house(const Close& c) {
    return c.island->building_at(c.w.config().layout, c.city->number, kCloseX, kCloseY);
}

} // namespace

TEST_CASE("population close C4: a fresh home in a real scenario, built by BuildCmd, supplied by "
          "walkers") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::ScenarioSpec spec = scenario_spec(*o, kClose);
    Close c = close_world(*o, spec);
    sim::World& w = c.w;
    const sim::Player* player = w.players().find(c.city->owner);
    REQUIRE(player != nullptr);
    CHECK(player->flags == 0x1); // the scenario setup, not injected

    // Before: the cost, the counters and the cursor.
    const sim::BuildingType& ty = *o->types->find(kCloseHouse);
    REQUIRE_FALSE(ty.cost_goods.empty());
    std::map<sim::GoodsId, std::uint32_t> store_before;
    std::map<sim::GoodsId, std::uint64_t> spent_before;
    for (const sim::CostEntry& e : ty.cost_goods) {
        store_before[e.goods] = c.city->stock.stock[e.goods];
        spent_before[e.goods] = c.city->build_goods[e.goods - o->cfg.build_goods_first].spent;
        REQUIRE(store_before[e.goods] >= e.amount_88); // affordable from the scenario's own store
    }
    const std::uint32_t money_before = static_cast<std::uint32_t>(player->money);
    const std::uint32_t houses_before = c.city->index.house_counts[0];
    const std::size_t cursor_before = w.rng().cursor();
    CHECK(close_house(c) == nullptr);

    // The build (legal control), then the same site again (refused: Occupied).
    const sim::ApplyResult built = w.apply(close_build(c));
    REQUIRE_MESSAGE(built.ok(), std::string(sim::reject_name(built.status)));
    const sim::ApplyResult twice = w.apply(close_build(c));
    CHECK_FALSE(twice.ok());
    CHECK(twice.records.at(0).status == sim::Reject::Occupied);
    const sim::Building* home = close_house(c);
    REQUIRE(home != nullptr);
    const sim::BuildingId hid = home->id;
    CHECK(home->type == kCloseHouse);
    // tile identity: the footprint carries the type id
    CHECK(c.island->tiles.object(c.island->tiles.index_of(kCloseX, kCloseY)) == kCloseHouse.v);
    CHECK(c.island->tiles.object(c.island->tiles.index_of(kCloseX + 3, kCloseY + 3)) ==
          kCloseHouse.v);
    // first assignment, group 0 (hand: constructor 5 residents / E 640, slot 0 = trunc(41*640/12)
    // = 2186, no other need stocked, no draw), exact payment once, counters, no gold
    const auto* h0 = std::get_if<sim::House>(&home->payload);
    REQUIRE(h0 != nullptr);
    CHECK(h0->residents_64 == 320);
    CHECK(h0->e == 640);
    REQUIRE(h0->stock.size() == 6);
    CHECK(h0->stock[0] == 2186);
    for (std::size_t j = 1; j < h0->stock.size(); ++j) {
        CHECK(h0->stock[j] == 0);
    }
    CHECK(w.rng().cursor() == cursor_before); // group 0 draws nothing
    for (const sim::CostEntry& e : ty.cost_goods) {
        CHECK(c.city->stock.stock[e.goods] == store_before[e.goods] - e.amount_88);
        CHECK(c.city->build_goods[e.goods - o->cfg.build_goods_first].spent ==
              spent_before[e.goods] + e.amount_88);
    }
    CHECK(static_cast<std::uint32_t>(w.players().find(c.city->owner)->money) == money_before);
    CHECK(c.city->index.house_counts[0] == houses_before + 1);
    CHECK(w.check_invariants() == "");

    // The run.
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    std::vector<std::uint32_t> stock0_trace;
    for (std::uint64_t i = 0; i < kCloseSteps; ++i) {
        step_world(w, &stats, nullptr);
        const sim::Building* b = close_house(c);
        REQUIRE(b != nullptr);
        REQUIRE(b->id == hid);
        stock0_trace.push_back(std::get<sim::House>(b->payload).stock[0]);
    }
    CHECK(w.check_invariants() == "");
    CHECK_FALSE(w.faulted());
    const auto& house = std::get<sim::House>(close_house(c)->payload);

    // Positive growth: only this house's records, never a decline, each step explained by the
    // table entry or by the cap, the end value is the cap Wohnmax * 64 = 512.
    std::vector<std::int32_t> trace{320};
    for (const sim::HouseGrowthRecord& g : stats.population.growth) {
        if (g.building != hid) {
            continue;
        }
        CHECK(g.residents_after > g.residents_before);
        CHECK(g.vermehr > 0);
        const bool exact = g.residents_after == g.residents_before + g.vermehr;
        const bool cap_clip =
            g.residents_before + g.vermehr > g.cap_64 && g.residents_after == g.cap_64;
        CHECK((exact || cap_clip));
        CHECK(g.residents_committed == g.residents_after);
        CHECK(g.cap_64 == 512);
        trace.push_back(g.residents_after);
    }
    CHECK(trace ==
          std::vector<std::int32_t>{320, 352, 384, 416, 428, 440, 452, 464, 476, 488, 500, 512});
    CHECK(house.residents_64 == 512);

    // Real supply: the house sent walkers (draws), the walkers bought at a stall and delivered; a
    // delivery shows as a rise of the food stock between two steps (the decay only lowers it).
    const auto work = stats.population.houses.find(hid);
    REQUIRE(work != stats.population.houses.end());
    CHECK(work->second.spawn_draws >= 1);
    CHECK(work->second.searches >= 1);
    CHECK(work->second.rect_queries <= work->second.searches);
    std::size_t rises = 0;
    for (std::size_t i = 1; i < stock0_trace.size(); ++i) {
        rises += stock0_trace[i] > stock0_trace[i - 1] ? 1U : 0U;
    }
    CHECK(rises >= 1);
    // The first delivery needs the food satisfaction at or below 116 % (score = 128 - s >= 12):
    // from 2186 (127.9 %) the decay of 1/128 per gate needs at least 12 gates of 2500 ticks, so no
    // rise can come before step 500 (25 000 ticks; a lower bound derived from the decay).
    std::size_t first_rise = 0;
    for (std::size_t i = 1; i < stock0_trace.size() && first_rise == 0; ++i) {
        first_rise = stock0_trace[i] > stock0_trace[i - 1] ? i : 0;
    }
    CHECK(first_rise > 500);
    CHECK(stats.residents.purchases >= 1);
    CHECK(stats.residents.deliveries >= 1);
    CHECK(stats.residents.lost_88 == 0);
    CHECK(stats.residents.faults == 0);
    CHECK(stats.residents.no_path_home == 0);
    // The ledger of the job side and the merge side agree for every goods (nothing manufactured).
    for (const auto& [g, taken] : stats.residents.taken_by_goods) {
        CHECK(stats.residents.merged_taken_by_goods[g] == taken);
        CHECK(stats.residents.merged_billed_by_goods[g] == stats.residents.billed_by_goods[g]);
    }

    // The stage pass changed the player's mask: the scenario setup holds 0x1; at the first pass the
    // city has 45 residents (40 loaded + 5 of the new home) against stage 0 (count 0) and stage 1
    // (count 30): city mask 0 -> 3, X16: low 23 bits 1 < 3 -> the OR is performed, player 0x1 ->
    // 0x3.
    bool saw_stage1 = false;
    for (const sim::StageUnlockRecord& u : stats.population.unlocks) {
        if (u.city.island.v != c.ref.island.v || u.city.city.v != c.ref.city.v) {
            continue;
        }
        if (u.stage == 1) {
            saw_stage1 = true;
            CHECK(u.count == 30);
            CHECK(u.residents_at_or_above >= 30);
            CHECK(u.city_mask_before == 0);
            CHECK(u.city_mask_after == 3);
            CHECK(u.player_flags_before == 0x1);
            CHECK(u.player_flags_after == 0x3);
        }
    }
    CHECK(saw_stage1);
    CHECK(w.players().find(c.city->owner)->flags == 0x3);
}

TEST_CASE("population close C4: save and reload cuts of the fresh-home run continue identically") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::ScenarioSpec spec = scenario_spec(*o, kClose);
    // Reference: one uninterrupted run, a hash after the build and every 10th step.
    std::vector<std::uint64_t> ref_hashes;
    std::uint64_t ref_final = 0;
    std::set<std::uint64_t> cuts;
    {
        Close c = close_world(*o, spec);
        REQUIRE(c.w.apply(close_build(c)).ok());
        ref_hashes.push_back(c.w.state_hash());
        std::int32_t last_res = 320;
        bool cargo_cut = false;
        for (std::uint64_t i = 1; i <= kCloseSteps; ++i) {
            step_world(c.w, nullptr, nullptr);
            if (i % 10 == 0) {
                ref_hashes.push_back(c.w.state_hash());
            }
            const std::int32_t res = std::get<sim::House>(close_house(c)->payload).residents_64;
            if (res != last_res && cuts.size() < 2) {
                cuts.insert(i); // right after the first and second growth step
            }
            last_res = res;
            for (const auto& slot : c.w.figures().list.slots) {
                if (!cargo_cut && slot && slot->resident && !slot->expiry_stamp &&
                    slot->home == close_house(c)->id &&
                    slot->resident->step == sim::ResidentStep::ToHome &&
                    std::any_of(slot->resident->cargo.begin(), slot->resident->cargo.end(),
                                [](const sim::ResidentCargo& g) { return g.goods != 0; })) {
                    cargo_cut = true;
                    cuts.insert(i); // a walker of the new home carries its cargo
                }
            }
        }
        ref_final = c.w.state_hash();
        REQUIRE(cargo_cut);
    }
    cuts.insert(0);   // right after the build
    cuts.insert(540); // just before the cap
    std::size_t done = 0;
    for (const std::uint64_t cut : cuts) {
        INFO("cut " << cut);
        Close c = close_world(*o, spec);
        REQUIRE(c.w.apply(close_build(c)).ok());
        for (std::uint64_t i = 1; i <= cut; ++i) {
            step_world(c.w, nullptr, nullptr);
        }
        const std::uint64_t before = c.w.state_hash();
        const std::vector<std::uint8_t> bytes = c.w.save();
        auto back = sim::World::load(bytes, o->types);
        REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
        sim::World w2 = std::move(back).value();
        CHECK(w2.state_hash() == before);
        CHECK(w2.save() == bytes);
        for (std::uint64_t i = cut + 1; i <= kCloseSteps; ++i) {
            step_world(w2, nullptr, nullptr);
            if (i % 10 == 0) {
                CHECK(w2.state_hash() == ref_hashes[i / 10]);
            }
        }
        CHECK(w2.state_hash() == ref_final);
        ++done;
    }
    CHECK(done == cuts.size());
    // {0 (the build), the first and the second growth step, the cargo cut, 540 (before the cap)}
    CHECK(cuts.size() == 5);
}

TEST_CASE("population close C4: a recorded fresh-home run replays, and an altered build diverges") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::ScenarioSpec spec = scenario_spec(*o, kClose);
    Close c = close_world(*o, spec);
    auto started = sim::Recorder::start(c.w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    REQUIRE(rec.submit(close_build(c)).ok());
    for (std::uint64_t i = 0; i < 300; ++i) {
        rec.advance(kStepMs);
        if (i % 30 == 29) {
            rec.checkpoint();
        }
    }
    const sim::ReplayLog& log = rec.log();
    export_log("pop_close.log", log);
    const auto bytes = sim::write_log(log);
    REQUIRE(bytes.ok());
    const auto read = sim::read_log(bytes.value());
    REQUIRE(read.ok());
    {
        auto fresh = sim::World::load(read.value().initial_save, o->types, {});
        REQUIRE(fresh.ok());
        auto outcome = sim::replay(fresh.value(), read.value());
        REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
        CHECK(outcome.value().checkpoints_total == 10);
        CHECK(outcome.value().checkpoints_matched == 10);
        CHECK(outcome.value().reproduced());
        CHECK(fresh.value().state_hash() == c.w.state_hash());
    }
    // The independently altered input: the same build with rotation 1 (the 4 x 4 footprint still
    // fits, the building's rotation differs): the first checkpoint after the build diverges.
    {
        sim::ReplayLog changed = read.value();
        bool altered = false;
        for (sim::ReplayEntry& e : changed.entries) {
            if (e.kind == sim::ReplayEntry::Kind::Commands) {
                for (sim::Command& cmd : e.commands) {
                    if (auto* b = std::get_if<sim::BuildCmd>(&cmd.body)) {
                        b->placements.at(0).rotation = 1;
                        altered = true;
                    }
                }
            }
        }
        REQUIRE(altered);
        export_log("pop_close_rot1.log", changed);
        auto fresh = sim::World::load(changed.initial_save, o->types, {});
        REQUIRE(fresh.ok());
        auto outcome = sim::replay(fresh.value(), changed);
        REQUIRE(outcome.ok());
        CHECK_FALSE(outcome.value().reproduced());
        REQUIRE(outcome.value().diverged_entry.has_value());
        // entry 0 = the Commands entry, entries 1..30 the advances, entry 31 the first checkpoint
        CHECK(*outcome.value().diverged_entry == 31);
    }
}

TEST_CASE("population close C4: the stage unlock is scenario state, not caused by the build") {
    // Control without a build: the same world, no BuildCmd. At the first gate the city's 40 loaded
    // pioneers meet stage 1 (count 30): city mask 0 -> 3 and the player's 0x1 -> 0x3 (X16), exactly
    // as with the new home (which only adds 5 to the 45 residents of the build run).
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::ScenarioSpec spec = scenario_spec(*o, kClose);
    Close c = close_world(*o, spec);
    sim::TransportStats stats;
    c.w.set_transport_stats(&stats);
    CHECK(c.w.players().find(c.city->owner)->flags == 0x1);
    for (std::uint64_t i = 0; i < 60; ++i) { // 3000 ticks: past the first gate at 2500
        step_world(c.w, &stats, nullptr);
    }
    bool saw = false;
    for (const sim::StageUnlockRecord& u : stats.population.unlocks) {
        if (u.city.island.v == c.ref.island.v && u.city.city.v == c.ref.city.v && u.stage == 1) {
            saw = true;
            CHECK(u.residents_at_or_above == 40);
            CHECK(u.player_flags_before == 0x1);
            CHECK(u.player_flags_after == 0x3);
        }
    }
    CHECK(saw);
    CHECK(c.w.players().find(c.city->owner)->flags == 0x3);
}
