// MAP-SUPPLY-1 S3: the small farm's animals (job 0x2e, kinds 107/109/110) on the shared harvest
// body (docs/sim/small-farm-supply.md 2.3, 9). The animal differs from the worker in two data facts
// only: its cargo goods are the farm's input 1 (GRAS) and its tile choice goes on past an unclaimed
// goal while 20 > score. Nothing is seeded: the grass fields are placed tile objects; the raw
// stock, the output and every counter start at zero; no timer grants stock.
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/harvest.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/walk_search.hpp>
#include <anno/sim/world.hpp>

#include "harvest_helpers.hpp"
#include "tutorial_town.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
using namespace harvest_test;

namespace {

struct Pasture {
    std::shared_ptr<const sim::TypeRegistry> types;
    sim::World w;
    sim::TransportStats stats;
};

Pasture make(const std::function<void(Rig&)>& fill) {
    Rig r = make_rig();
    fill(r);
    auto types = registry(3 * 256);
    Pasture p{types, create(r, types), {}};
    p.w.set_transport_stats(&p.stats);
    return p;
}

const sim::SmallFarm& farm_of(const sim::World& w, std::size_t island = 0,
                              std::uint32_t serial = 1) {
    const sim::Building* b = w.islands()[island]->find_building(
        sim::make_building_id(sim::IslandId{static_cast<std::uint32_t>(island)}, serial));
    REQUIRE(b != nullptr);
    return std::get<sim::SmallFarm>(b->payload);
}

std::uint32_t object_at(const sim::World& w, std::int32_t x, std::int32_t y,
                        std::size_t island = 0) {
    const sim::Island& is = *w.islands()[island];
    return is.tiles.object(is.tile_index(x, y));
}

template <class Pred> bool run_until(sim::World& w, Pred pred, int max_advances = 6000) {
    for (int i = 0; i < max_advances && !pred(); ++i) {
        w.advance(10);
    }
    return pred();
}

std::vector<const sim::Figure*> animals_of(const sim::World& w) {
    std::vector<const sim::Figure*> out;
    for (const auto& s : w.figures().list.slots) {
        if (s.has_value() && sim::animal_job(s->job) && !s->expiry_stamp.has_value()) {
            out.push_back(&*s);
        }
    }
    return out;
}

/// The animal counters (indices 1..4; index 0 is the line-0 worker).
std::uint32_t animal_counters(const sim::SmallFarm& f) {
    std::uint32_t n = 0;
    for (std::size_t i = 1; i < f.figures_out.size(); ++i) {
        n += f.figures_out[i];
    }
    return n;
}

void reload_agrees(Pasture& p) {
    auto loaded = sim::World::load(p.w.save(), p.types);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World& c = loaded.value();
    CHECK(c.state_hash() == p.w.state_hash());
    CHECK(c.check_invariants() == "");
    for (int i = 0; i < 400; ++i) {
        p.w.advance(10);
        c.advance(10);
        if (i % 100 == 99) {
            CHECK_MESSAGE(c.state_hash() == p.w.state_hash(),
                          "continuation diverged at advance " << i);
        }
    }
    CHECK(c.check_invariants() == "");
}

void grass_ring(Rig& r, std::initializer_list<std::pair<int, int>> tiles) {
    for (const auto& [x, y] : tiles) {
        r.object(x, y, kGrass);
    }
}

} // namespace

TEST_CASE("small farm animals: grass is grazed, credited to raw 1 and made into NAHRUNG, counters "
          "released") {
    // The farm (3 x 3 at (110,110), radius 3, window 108..114) with five ripe grass fields outside
    // its footprint; no grain, so the worker finds nothing. Line 1 (GRAS -> NAHRUNG: 50 raw per
    // step, 4 made).
    Pasture p = make([](Rig& r) {
        grass_ring(r, {{113, 111}, {113, 112}, {111, 113}, {112, 113}, {109, 113}});
        r.place(kSmallFarm, 110, 110,
                sim::make_small_farm(registry(3 * 256)->find(kSmallFarm)->prod));
    });
    std::set<std::uint32_t> kinds;
    std::uint32_t max_out = 0;
    for (int i = 0; i < 40000;
         ++i) { // 200 000 ticks: five fields, 5000-tick grazes, the line works the credit off
        p.w.advance(10);
        for (const sim::Figure* a : animals_of(p.w)) {
            kinds.insert(a->kind);
            CHECK(a->cargo_goods == goods_id("GRAS")); // the farm's input goods 1
        }
        max_out = std::max(max_out, animal_counters(farm_of(p.w)));
    }
    const sim::ForestryStats& fs = p.stats.forestry;
    const sim::SmallFarm& sf = farm_of(p.w);
    CHECK(fs.cuts == 5);
    CHECK(fs.cuts_failed == 0);
    CHECK(fs.raw_credited_88 == 5 * 256);
    CHECK(fs.raw_delivered_88 == 5 * 256);
    CHECK(fs.raw_lost_88 == 0);
    for (const auto [x, y] : std::vector<std::pair<int, int>>{
             {113, 111}, {113, 112}, {111, 113}, {112, 113}, {109, 113}}) {
        CHECK(object_at(p.w, x, y) == kGrassYoung.v);
    }
    // the kinds are the original table's (107, 109, 110) and only those
    CHECK_FALSE(kinds.empty());
    for (const std::uint32_t k : kinds) {
        CHECK((k == 107 || k == 109 || k == 110));
    }
    // line 0 did nothing (no grain): raw 0 empty; line 1: nothing is lost between credit and
    // product
    CHECK(sf.raw[0] == 0);
    CHECK(sf.out[0] == 0);
    // The credit is worked off: raw 1 is below the starvation threshold (25 = ceil(64 * 50 / 128)),
    // every full step used 50 and made 4; at most one partial tail step made up to 3 more.
    CHECK(sf.raw[1] < 25);
    const std::uint32_t used = 5U * 256U - sf.raw[1];
    CHECK(sf.out[1] >= 4U * (used / 50U));
    CHECK(sf.out[1] <= 4U * (used / 50U) + 3U);
    MESSAGE("raw1 " << sf.raw[1] << " out1 " << sf.out[1] << " used " << used);
    // every animal ended: all five counters are zero, none leaked, none above the per-kind limits
    CHECK(animal_counters(sf) == 0);
    CHECK(sf.figures_out[0] <= 1);
    CHECK(max_out <= 3); // max_figures(0) over the animal counters
    CHECK(animals_of(p.w).empty());
    CHECK(p.w.check_invariants() == "");
    reload_agrees(p);
}

TEST_CASE("small farm animals: the search goes on past an unclaimed goal while 20 > score (the "
          "worker takes the first)") {
    // A radius-8 farm; grass fields along a row at growing distance. The animal's first target must
    // be what the callback of D 0x10058a40 ends with on the real visit order: harvest_pick_tile(20,
    // visits), computed here from an independent run of the same walk search from the animal's own
    // position.
    Pasture p = make([](Rig& r) {
        grass_ring(r, {{114, 111},
                       {115, 111},
                       {116, 111},
                       {117, 111},
                       {118, 111},
                       {118, 112},
                       {105, 111},
                       {104, 111}});
        r.place(kSmallFarm4, 110, 110,
                sim::make_small_farm(registry(3 * 256)->find(kSmallFarm4)->prod));
    });
    // the first animal and its first claimed target
    REQUIRE(run_until(p.w, [&] {
        const auto a = animals_of(p.w);
        return !a.empty() && a.front()->harvest &&
               a.front()->harvest->step == sim::HarvestStep::ToTree;
    }));
    const sim::Figure* a = animals_of(p.w).front();
    const sim::TileXY target = a->harvest->target;
    // The visit list comes from a second run of the SAME walk search (not independent of the
    // engine's search; it checks that the callback's use of it equals the pure function). Costs:
    // this rig's synthetic tables, every cost 1 (scores <= 7), so the stop at 20 does not fire here
    // and the animal ends at the last goal popped.
    const sim::Island& is = *p.w.islands()[0];
    const sim::Building* home = is.find_building(a->home);
    REQUIRE(home != nullptr);
    const sim::BuildingType& ht = *p.types->find(home->type);
    const sim::SearchArea area = sim::search_area(*home, ht);
    const auto& tables = p.types->walk_tables();
    REQUIRE(tables != nullptr);
    sim::WalkCells cells(is, *p.types, *tables, area.square(), 4, area);
    std::set<sim::TileXY> grass;
    std::vector<sim::WalkPoint> goals;
    for (const sim::TileXY t : std::vector<sim::TileXY>{{114, 111},
                                                        {115, 111},
                                                        {116, 111},
                                                        {117, 111},
                                                        {118, 111},
                                                        {118, 112},
                                                        {105, 111},
                                                        {104, 111}}) {
        grass.insert(t);
        for (std::uint32_t row = 0; row < 3; ++row) {
            for (std::uint32_t col = 0; col < 3; ++col) {
                if ((is.tiles.walk_mask(is.tile_index(t.x, t.y)) & 0x1ffU &
                     (1U << (row * 3 + col))) == 0) {
                    goals.push_back(sim::WalkPoint{t, col, row});
                }
            }
        }
    }
    std::vector<sim::HarvestGoalNode> nodes;
    const std::array starts{sim::walk_point_of(
        sim::FixedPos{a->harvest->route.empty() ? a->pos.x : a->pos.x, a->pos.y})};
    (void)starts;
    // the start of the animal's own search is its spawn position, the home's first entrance point
    const auto entrance = sim::entrance_walk_points(*home, ht);
    REQUIRE_FALSE(entrance.empty());
    const sim::SearchResult all =
        sim::search(cells, *tables, p.types->find_figure_type(a->kind)->layer, entrance, goals,
                    [&](const sim::SearchVisit& v) {
                        if (grass.contains(v.point.tile)) {
                            nodes.push_back({v.point.tile.x, v.point.tile.y, v.score, false});
                        }
                        return sim::SearchDecision{};
                    });
    REQUIRE(all.complete);
    REQUIRE_FALSE(nodes.empty());
    std::uint16_t max_score = 0;
    for (const auto& n : nodes) {
        max_score = std::max(max_score, n.score);
    }
    const auto expected = sim::harvest_pick_tile(sim::kAnimalSearchLimit, nodes);
    const auto first = sim::harvest_pick_tile(0, nodes);
    REQUIRE(expected.has_value());
    MESSAGE("target " << target.x << "," << target.y << " animal pick " << expected->x << ","
                      << expected->y << " worker pick " << first->x << "," << first->y
                      << " max score " << max_score << " visits " << nodes.size());
    CHECK((target.x == expected->x && target.y == expected->y));
    // the animal does not simply take the nearest field: it differs from the worker's first-goal
    // rule
    CHECK((expected->x != first->x || expected->y != first->y));
    CHECK(p.w.check_invariants() == "");
}

TEST_CASE("small farm animals: the home destroyed with animals out, in each phase, leaks nothing") {
    for (const bool carrying : {false, true}) {
        Pasture p = make([](Rig& r) {
            grass_ring(r, {{113, 111}, {113, 112}, {111, 113}});
            r.place(kSmallFarm, 110, 110,
                    sim::make_small_farm(registry(3 * 256)->find(kSmallFarm)->prod));
        });
        if (carrying) {
            REQUIRE(run_until(p.w, [&] {
                const auto a = animals_of(p.w);
                return !a.empty() && a.front()->cargo_88 >= 256;
            }));
        } else {
            REQUIRE(run_until(p.w, [&] {
                const auto a = animals_of(p.w);
                return !a.empty() && a.front()->harvest &&
                       a.front()->harvest->step != sim::HarvestStep::Start;
            }));
        }
        std::uint64_t carried = 0;
        for (const sim::Figure* a : animals_of(p.w)) {
            carried += a->cargo_88;
        }
        sim::Command demolish;
        demolish.issuer = sim::PlayerId{0};
        demolish.body = sim::DemolishCmd{sim::CityRef{sim::IslandId{0}, sim::CityNo{0}},
                                         {sim::TileXY{110, 110}}};
        REQUIRE(p.w.apply(demolish).ok());
        CHECK(p.w.check_invariants() == "");
        reload_agrees(p); // animals outlive their home: a valid state, and the continuation agrees
        REQUIRE(run_until(p.w, [&] { return animals_of(p.w).empty(); }, 9000));
        CHECK(p.stats.forestry.raw_delivered_88 == 0);
        CHECK(p.stats.forestry.raw_lost_88 >= carried);
        const sim::Island& is = *p.w.islands()[0];
        for (const auto [x, y] :
             std::vector<std::pair<int, int>>{{113, 111}, {113, 112}, {111, 113}}) {
            CHECK((is.tiles.aux(is.tile_index(x, y)) & sim::kTileClaim) == 0);
        }
        CHECK(p.w.check_invariants() == "");
        reload_agrees(p);
    }
}

TEST_CASE("small farm animals: reload mid-trip at each phase continues identically") {
    for (const sim::HarvestStep phase : {sim::HarvestStep::Start, sim::HarvestStep::ToTree,
                                         sim::HarvestStep::Cut, sim::HarvestStep::ToHome}) {
        Pasture p = make([](Rig& r) {
            grass_ring(r, {{113, 111}, {113, 112}, {111, 113}});
            r.place(kSmallFarm, 110, 110,
                    sim::make_small_farm(registry(3 * 256)->find(kSmallFarm)->prod));
        });
        REQUIRE(run_until(
            p.w,
            [&] {
                for (const sim::Figure* a : animals_of(p.w)) {
                    if (a->harvest.has_value() && a->harvest->step == phase) {
                        return true;
                    }
                }
                return false;
            },
            12000));
        reload_agrees(p);
        CHECK(p.w.check_invariants() == "");
    }
}

TEST_CASE("small farm animals: the per-kind and sum limits hold while the pasture feeds them") {
    // Plenty of grass: at no step are more than max_figures(0) = 3 animals out, and no kind exceeds
    // its limit (107: 2, 109: 1, 110: 1) by more than the producing-step allowance.
    Pasture p = make([](Rig& r) {
        grass_ring(r, {{113, 111},
                       {113, 112},
                       {113, 113},
                       {112, 113},
                       {111, 113},
                       {110, 113},
                       {109, 113},
                       {108, 113},
                       {108, 112},
                       {108, 111},
                       {108, 110},
                       {108, 109}});
        r.place(kSmallFarm, 110, 110,
                sim::make_small_farm(registry(3 * 256)->find(kSmallFarm)->prod));
    });
    std::map<std::uint32_t, std::uint32_t> max_by_kind;
    std::uint32_t max_sum = 0;
    for (int i = 0; i < 12000; ++i) {
        p.w.advance(10);
        std::map<std::uint32_t, std::uint32_t> now;
        for (const sim::Figure* a : animals_of(p.w)) {
            ++now[a->kind];
        }
        std::uint32_t sum = 0;
        for (const auto& [k, n] : now) {
            max_by_kind[k] = std::max(max_by_kind[k], n);
            sum += n;
        }
        max_sum = std::max(max_sum, sum);
    }
    CHECK(max_sum <= 3);
    // the per-kind test is `limit <= counter - (a producing step)` (D 0x1006f855-0x1006f86b): one
    // more than the limit while the line made something this step, never more
    CHECK(max_by_kind[107] <= 3);
    CHECK(max_by_kind[109] <= 2);
    CHECK(max_by_kind[110] <= 2);
    CHECK(max_sum >= 1);
    CHECK(p.w.check_invariants() == "");
}

TEST_CASE(
    "small farm animals on original data: the Tutorial town's farms graze real grass, and the "
    "stop at score 20 fires") {
    // Original scenario objects and the ORIGINAL walk-cost tables (kinds 107/109/110 walk on layer
    // 4; ground costs 10 straight / 13 diagonal): the two small farms of the player-2 town (island
    // 3) at (212,250) and (211,240) have 28 and 11 ripe grass objects in their windows and no
    // potatoes. Rules::house_updates false.
    const auto o = tutorial_town::load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World w = tutorial_town::tutorial_world(*o, false);
    sim::Island* isl = nullptr;
    for (auto& i : sim::WorldTestAccess::islands(w)) {
        isl = i && i->index.v == 3 ? &*i : isl;
    }
    REQUIRE(isl != nullptr);
    const sim::Building* fb = isl->building_at(w.config().layout, sim::CityNo{0}, 212, 250);
    REQUIRE(fb != nullptr);
    REQUIRE(fb->type.v == 809);
    const sim::BuildingId fid = fb->id;
    const std::vector<std::uint32_t> grass_ids{2U,  21U, 23U, 25U, 27U, 69U,
                                               71U, 73U, 79U, 81U, 83U, 355U};
    const auto is_grass = [&](std::uint32_t id) {
        return std::find(grass_ids.begin(), grass_ids.end(), id) != grass_ids.end();
    };
    const auto grass_ripe = [&] {
        std::uint32_t n = 0;
        for (int y = 248; y <= 254; ++y) {
            for (int x = 210; x <= 216; ++x) {
                n += is_grass(isl->tiles.object(isl->tile_index(x, y))) ? 1U : 0U;
            }
        }
        return n;
    };
    const std::uint32_t ripe0 = grass_ripe();
    CHECK(ripe0 == 28);
    // the first animal's first target, against the visit list of the real tables from its start
    const auto animals_now = [&] {
        std::vector<const sim::Figure*> out;
        for (const auto& s : w.figures().list.slots) {
            if (s.has_value() && sim::animal_job(s->job) && s->home == fid &&
                !s->expiry_stamp.has_value()) {
                out.push_back(&*s);
            }
        }
        return out;
    };
    const sim::Figure* first = nullptr;
    for (int i = 0; i < 4000 && first == nullptr; ++i) {
        w.advance(10);
        for (const sim::Figure* a : animals_now()) {
            if (a->harvest && a->harvest->step == sim::HarvestStep::ToTree) {
                first = a;
            }
        }
    }
    REQUIRE(first != nullptr);
    const sim::TileXY target = first->harvest->target;
    const sim::Building* home = isl->find_building(fid);
    const sim::BuildingType& ht = *o->types->find(home->type);
    const sim::SearchArea area = sim::search_area(*home, ht);
    const auto& tables = o->types->walk_tables();
    REQUIRE(tables != nullptr);
    sim::WalkCells cells(*isl, *o->types, *tables, area.square(), 4, area);
    std::set<sim::TileXY> grass;
    std::vector<sim::WalkPoint> goals;
    const sim::TileRect sq = area.square();
    for (int y = sq.y0; y < sq.y0 + static_cast<int>(sq.h); ++y) {
        for (int x = sq.x0; x < sq.x0 + static_cast<int>(sq.w); ++x) {
            if (!is_grass(isl->tiles.object(isl->tile_index(x, y)))) {
                continue;
            }
            const sim::TileXY t{x, y};
            grass.insert(t);
            for (std::uint32_t row = 0; row < 3; ++row) {
                for (std::uint32_t col = 0; col < 3; ++col) {
                    if ((isl->tiles.walk_mask(isl->tile_index(x, y)) & 0x1ffU &
                         (1U << (row * 3 + col))) == 0) {
                        goals.push_back(sim::WalkPoint{t, col, row});
                    }
                }
            }
        }
    }
    std::vector<sim::HarvestGoalNode> nodes;
    const auto layer = o->types->find_figure_type(first->kind)->layer;
    const sim::SearchResult all =
        sim::search(cells, *tables, layer, sim::entrance_walk_points(*home, ht), goals,
                    [&](const sim::SearchVisit& v) {
                        if (grass.contains(v.point.tile)) {
                            nodes.push_back({v.point.tile.x, v.point.tile.y, v.score, false});
                        }
                        return sim::SearchDecision{};
                    });
    REQUIRE(all.complete);
    // the real scores of the ripe grass in reach (a second run of the same search: not independent
    // of it)
    std::set<std::uint16_t> scores;
    for (const auto& n : nodes) {
        scores.insert(n.score);
    }
    const auto pick = sim::harvest_pick_tile(sim::kAnimalSearchLimit, nodes);
    REQUIRE(pick.has_value());
    std::uint16_t stop_score = 0;
    for (const auto& n : nodes) {
        if (n.x == pick->x && n.y == pick->y) {
            stop_score = n.score; // the (first) visit of the picked tile
        }
    }
    MESSAGE("layer " << layer << " target " << target.x << "," << target.y << " pick " << pick->x
                     << "," << pick->y << " stop score " << stop_score << " distinct scores "
                     << scores.size() << " min " << *scores.begin() << " max " << *scores.rbegin());
    CHECK((target.x == pick->x && target.y == pick->y));
    CHECK(*scores.rbegin() >= sim::kAnimalSearchLimit); // the stop can fire on the real tables
    CHECK(*scores.begin() <
          sim::kAnimalSearchLimit); // and the search first goes on past cheaper goals
    CHECK(stop_score >= sim::kAnimalSearchLimit);
    // Pinned from the real tables (a legitimate change of the walk costs re-pins them): the search
    // stops at the goal of EXACT score 20 (`20 > 20` is false), so `>=` instead of `>` would run on
    // to the next goal; the pure function above shares the predicate, so only these literals
    // separate the two.
    CHECK(stop_score == 20);
    CHECK(target.x == 212);
    CHECK(target.y == 248);
    CHECK(scores.size() == 9);
    CHECK(*scores.begin() == 10);
    CHECK(*scores.rbegin() == 36);
    // the worker's rule (limit 0) would have taken the first goal popped instead
    const auto worker = sim::harvest_pick_tile(0, nodes);
    CHECK((worker->x != pick->x || worker->y != pick->y));
    // the run
    std::set<std::uint32_t> kinds;
    const auto farm_now = [&]() -> const sim::SmallFarm& {
        return std::get<sim::SmallFarm>(isl->find_building(fid)->payload);
    };
    for (int i = 0; i < 8000 && farm_now().out[1] < 40; ++i) {
        w.advance(100);
        for (const sim::Figure* a : animals_now()) {
            kinds.insert(a->kind);
        }
    }
    const sim::SmallFarm& sf = farm_now();
    MESSAGE("tick " << w.clock().tick() << " ripe " << ripe0 << " -> " << grass_ripe() << " raw1 "
                    << sf.raw[1] << " out1 " << sf.out[1] << " kinds " << kinds.size());
    // measured and pinned: 9 of the 28 ripe objects cut, all three animal kinds seen
    // The tick depends on the stepping schedule (the advance(10) loop above, then advance(100)):
    // chunking changes state hashes (pre-existing, docs/sim/state.md); do not re-pin by changing
    // the loops.
    CHECK(w.clock().tick() == 26085);
    CHECK(sf.out[1] == 40);
    CHECK(grass_ripe() == ripe0 - 9); // real grass objects were cut (id - 1)
    CHECK(kinds.size() == 3);
    CHECK(sf.out[0] == 0); // no potatoes in reach: line 0 makes nothing
    CHECK(w.check_invariants() == "");
}

TEST_CASE("small farm animals: a small-farm type without input goods 1 sends no animal") {
    // driver.cpp consume: the animal branch needs the farm's input goods 1 for the cargo (D
    // type+0xb0(1)); a type that lacks it sends nothing and raises no counter (no invented
    // fallback). The kernel still takes its draw and decides; only the send is refused. The type is
    // a copy of 9330 with its second input goods dropped.
    sim::BuildingType one_input = *registry(3 * 256)->find(kSmallFarm);
    one_input.id = sim::TypeId{9333};
    one_input.input_goods.resize(1);
    auto types = registry(3 * 256, {one_input});
    Rig r = make_rig();
    grass_ring(r, {{113, 111}, {113, 112}});
    r.place(sim::TypeId{9333}, 110, 110,
            sim::make_small_farm(types->find(sim::TypeId{9333})->prod));
    Pasture p{types, create(r, types), {}};
    p.w.set_transport_stats(&p.stats);
    std::uint32_t draws = 0;
    for (int i = 0; i < 3000; ++i) {
        p.w.advance(10);
        draws += draws_at(p.w.last_output(), sim::DrawSite::SmallFarmAnimal);
        CHECK(animals_of(p.w).empty());
    }
    CHECK(draws > 0); // line 1 ran and drew, the send was refused
    const sim::SmallFarm& sf = farm_of(p.w);
    for (std::size_t i = 1; i < sf.figures_out.size(); ++i) {
        CHECK(sf.figures_out[i] == 0); // no animal counter was raised
    }
    CHECK(p.stats.forestry.cuts == 0);
    CHECK(p.w.check_invariants() == "");
}
