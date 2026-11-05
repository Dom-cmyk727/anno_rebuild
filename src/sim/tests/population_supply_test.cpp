// MAP-POP-CLOSE-2 K2 B: the supply paths on ORIGINAL data (docs/sim/population-live.md section 24).
// The Tutorial town (Tutorial/02Trade01.sze island 3, player 2, section 22.4) holds a market hall,
// a Kontor, the leather and cloth stand 1314, the food and salt stands 1310, the chapel 1320 and an
// Inn. With the goal cells carrying their key nibble (OQ-463) every one of them is reached by a
// house walker. Checks: (1) the eight existing houses and eight BuildCmd-built 1314 stands reach
// every stand like the 1310 control; (2) a fresh pioneer house built by BuildCmd is served by the
// stalls (LEDER, STOFFE, SALZ out of the city store, take 1) and by the chapel (the KAPELLE slot,
// billed with take 0, the store untouched); (3) the exact ledger of those purchases.
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/walk_search.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>

#include "tutorial_town.hpp"

namespace {

namespace sim = anno::sim;
namespace fs = std::filesystem;

using namespace tutorial_town;

/// Builds `type` on a 3 x 3 grid of sites two tiles apart centred on (sx, sy), the centre excluded,
/// so that no stand is enclosed by other stands (a mapper-0 tile is passable only as a goal, never
/// crossed); returns the number built.
int build_cluster(sim::World& w, const Town& t, sim::TypeId type, int sx, int sy) {
    int built = 0;
    for (int dy = -2; dy <= 2; dy += 2) {
        for (int dx = -2; dx <= 2; dx += 2) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            const auto res = w.apply(sim::Command{
                t.city->owner,
                sim::BuildCmd{t.ref, {sim::PlacementWire{type, sx + dx, sy + dy, 0, 0, 0}}}});
            built += res.ok() ? 1 : 0;
        }
    }
    return built;
}

/// The stands (stall, public and Inn classes) a house's walker search reaches, by building origin.
std::set<std::pair<int, int>> reached_stands(const Original& o, const Town& t,
                                             const sim::Building& house) {
    const sim::BuildingType& ht = *o.types->find(house.type);
    const auto& tables = o.types->walk_tables();
    REQUIRE(tables != nullptr);
    const sim::SearchArea area = sim::search_area(house, ht);
    sim::WalkCells cells(*t.island, *o.types, *tables, area.square(), 4, area);
    struct G {
        sim::WalkPoint p;
        int x, y;
    };
    std::vector<G> goals;
    for (const auto& l : t.city->index.lists) {
        for (const auto& b : l.slots) {
            if (!b) {
                continue;
            }
            const sim::BuildingType* bt = o.types->find(b->type);
            if (bt == nullptr || (bt->cls != sim::RuntimeClass::MarketStall &&
                                  bt->cls != sim::RuntimeClass::PublicService &&
                                  bt->cls != sim::RuntimeClass::Inn)) {
                continue;
            }
            for (const sim::WalkPoint& p : sim::entrance_walk_points(*b, *bt)) {
                goals.push_back({p, b->x, b->y});
            }
        }
    }
    std::vector<sim::WalkPoint> gp;
    for (const G& g : goals) {
        gp.push_back(g.p);
    }
    std::set<std::pair<int, int>> reached;
    const auto starts = sim::entrance_walk_points(house, ht);
    const sim::SearchResult found =
        sim::search(cells, *tables, 0, starts, gp, [&](const sim::SearchVisit& v) {
            for (const G& g : goals) {
                if (g.p == v.point) {
                    reached.insert({g.x, g.y});
                }
            }
            return sim::SearchDecision{sim::GoalAction::Expand, false};
        });
    REQUIRE(found.complete);
    return reached;
}

std::vector<const sim::Building*> houses_of(const Town& t) {
    std::vector<const sim::Building*> out;
    for (const auto& l : t.city->index.lists) {
        for (const auto& b : l.slots) {
            if (b && std::holds_alternative<sim::House>(b->payload)) {
                out.push_back(&*b);
            }
        }
    }
    return out;
}

} // namespace

TEST_CASE("supply K2: every stand of the Tutorial town is reached, and eight built 1314 stands "
          "like eight 1310") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::size_t stands_seen[2] = {0, 0};
    std::size_t index = 0;
    for (const sim::TypeId stall : {sim::TypeId{1314}, sim::TypeId{1310}}) {
        sim::World w = tutorial_world(*o, false);
        const Town t = find_town(w);
        // the original stands: 1314 (222,255), 1310 (223,255) and (224,255), the chapel (228,255),
        // the Inn (219,238); the eight settler houses all reach all five (before K2: 4 of 5, never
        // the 1314)
        const auto houses = houses_of(t);
        REQUIRE(houses.size() == 8);
        for (const sim::Building* h : houses) {
            const auto reached = reached_stands(*o, t, *h);
            CHECK(reached.count({222, 255}) == 1);
            CHECK(reached.count({223, 255}) == 1);
            CHECK(reached.count({224, 255}) == 1);
            CHECK(reached.count({228, 255}) == 1);
            CHECK(reached.count({219, 238}) == 1);
        }
        const int built = build_cluster(w, t, stall, 233, 246);
        CHECK(built == 8);
        const auto reached = reached_stands(*o, t, *houses_of(t)[0]);
        stands_seen[index++] = reached.size();
        for (const auto& l : t.city->index.lists) {
            for (const auto& b : l.slots) {
                if (b && b->type == stall && reached.count({b->x, b->y}) == 0) {
                    MESSAGE("stand " << stall.v << " at (" << b->x << "," << b->y
                                     << ") is NOT reached");
                }
            }
        }
        CHECK(reached.size() == 5 + 8); // the five originals and every built stand
        CHECK(w.check_invariants() == "");
    }
    CHECK(stands_seen[0] == stands_seen[1]); // 1314 like 1310
}

TEST_CASE("supply K2: a fresh pioneer house is served by the stalls and the chapel with the exact "
          "ledger") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World w = tutorial_world(*o, true);
    const Town t = find_town(w);
    constexpr int kX = 226, kY = 251;
    const auto res = w.apply(sim::Command{
        t.city->owner, sim::BuildCmd{t.ref, {sim::PlacementWire{kPioneer, kX, kY, 0, 0, 0}}}});
    REQUIRE_MESSAGE(res.ok(), std::string(sim::reject_name(res.status)));
    const std::uint32_t leder_before = t.city->stock.stock[kLeder];
    const std::uint32_t stoffe_before = t.city->stock.stock[kStoffe];
    const std::uint32_t salz_before = t.city->stock.stock[kSalz];
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    // The fresh house's slots 1 (LEDER), 2 (STOFFE), 3 (SALZ) and 5 (KAPELLE) start at 0; each
    // fills only by a walker's delivery. One advance(100) is 100 ms = 50 ticks (a tick is 2 ms):
    // 2400 advances = 120000 ticks (checked below against the clock). The ledger below is TOWN-WIDE
    // (the eight settler houses buy too); what is specific to the fresh house is the slot
    // assertions.
    std::array<bool, 6> filled{};
    for (int i = 0; i < 2400; ++i) {
        w.advance(100);
        REQUIRE_FALSE(w.faulted());
        const sim::Building* b = t.island->building_at(w.config().layout, t.city->number, kX, kY);
        REQUIRE(b != nullptr);
        const auto* h = std::get_if<sim::House>(&b->payload);
        REQUIRE(h != nullptr);
        for (std::size_t j = 1; j < h->stock.size() && j < filled.size(); ++j) {
            filled[j] = filled[j] || h->stock[j] > 0;
        }
    }
    CHECK(w.clock().tick() == 120000);
    CHECK(filled[1]); // LEDER by a 1314 walker
    CHECK(filled[2]); // STOFFE by a 1314 walker
    CHECK(filled[3]); // SALZ by a 1310 walker
    CHECK(filled[5]); // KAPELLE by the chapel
    const sim::ResidentStats& rs = stats.residents;
    for (const sim::GoodsId g : {kLeder, kStoffe, kSalz}) {
        // a stall takes the store (take 1): bought == taken, and billed raised by the requested
        // amount
        CHECK(rs.bought_by_goods.count(g) == 1);
        CHECK(rs.taken_by_goods.at(g) == rs.bought_by_goods.at(g));
        // the request is billed in full and the stall returns the clipped amount: here the stall
        // store always covers the request, so billed == bought exactly (measured, town-wide)
        CHECK(rs.billed_by_goods.at(g) == rs.bought_by_goods.at(g));
    }
    // the store fell by exactly what the stalls took. The Tutorial town holds no producer of LEDER
    // or SALZ. Since MAP-NEEDS-2 N2 its sheep farm (animals now graze) and weaver make cloth, so
    // for STOFFE the fall is the stalls' take minus an inflow, bounded below by 0 and above by what
    // any cart offered.
    const std::uint64_t cloth_offered =
        stats.unload_offered.count(kStoffe) != 0 ? stats.unload_offered.at(kStoffe) : 0U;
    CHECK(t.city->stock.stock[kLeder] == leder_before - rs.taken_by_goods.at(kLeder));
    // STOFFE: the stats are world-wide (the scenario has other towns with their own cloth carts),
    // so the inflow into THIS store is bounded, not equated: 0 <= stock - (before - taken) <=
    // offered by any cart.
    const std::int64_t cloth_in =
        std::int64_t{t.city->stock.stock[kStoffe]} -
        (std::int64_t{stoffe_before} - std::int64_t(rs.taken_by_goods.at(kStoffe)));
    // The measured inflow into this store in this run is 288 raw (1.125 t of cloth from the
    // Tutorial town's own sheep farm and weaver); the world-wide offered cloth bounds it from
    // above.
    CHECK(cloth_in == 288);
    CHECK(cloth_in <= std::int64_t(cloth_offered));
    CHECK(t.city->stock.stock[kSalz] == salz_before - rs.taken_by_goods.at(kSalz));
    // the chapel bills with take 0: bought > 0, nothing taken, the store has no entry for it
    CHECK(rs.bought_by_goods.count(kKapelle) == 1);
    CHECK(rs.bought_by_goods.at(kKapelle) > 0);
    CHECK(rs.taken_by_goods.count(kKapelle) == 0);
    CHECK(rs.delivered_by_goods.at(kKapelle) > 0);
    CHECK(rs.faults == 0);
    CHECK(rs.lost_88 == 0);
    CHECK(rs.no_path_home == 0);
    CHECK(w.check_invariants() == "");
}
