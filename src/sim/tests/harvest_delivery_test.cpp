// MAP-ECON-1 E4: existing tree -> worker cargo -> Farm.raw -> recipe -> HOLZ output -> cart cargo
// -> warehouse receipt, with a material ledger in real units (docs/sim/forestry.md 7). The
// fixture's raw and output stores start at zero; nothing is seeded. The miniature is derived by
// hand below and every equation's unexplained term must be zero, not merely printed.
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/figure.hpp>
#include <anno/sim/harvest.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "harvest_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
using namespace harvest_test;

namespace {

/// The hand-derived miniature (docs/sim/forestry.md E-6, 7): three mature trees, nothing else to
/// harvest within the horizon (regrowth needs >= 456 s = 228 000 ticks, the run is shorter).
/// credited = 3 cuts * 256 = 768 raw (BAUM, 8.8); every delivery is 256 and arrives at a raw store
/// that only ever holds multiples of 32, so every production step that runs consumes 32 at ratio
/// 128 and makes 14: 768 / 32 = 24 steps -> 24 * 14 = 336 HOLZ (1.3125 t = 3 * 0.4375 t; wood is
/// not conserved across the recipe). The cart takes the forester's output once it holds 1 t (256)
/// and carries it to the market; what it unloads below the city's capacity is received in full.
constexpr std::uint32_t kTrees = 3;
constexpr std::uint64_t kCredited = kTrees * 256; // 768 raw
constexpr std::uint64_t kSteps = kCredited / 32;  // 24 full steps
constexpr std::uint64_t kMade = kSteps * 14;      // 336 HOLZ

struct Chain {
    std::shared_ptr<const sim::TypeRegistry> types;
    sim::World w;
    sim::TransportStats stats;
    sim::BuildingId forester_id, market_id;
};

/// Forester at (110, 110) (entrance on its anchor tile (110, 110)), market at (109, 104) (its
/// entrance tile (110, 106)), a road (110, 107..109) joining the two for the cart (layer 3 does not
/// admit open land, code 2, in these walk tables), trees at the given tiles.
Chain make_chain(const std::vector<std::pair<std::int32_t, std::int32_t>>& trees,
                 bool road = true) {
    Rig r = make_rig();
    for (const auto& [x, y] : trees) {
        r.object(x, y, kTree);
    }
    if (road) {
        for (std::int32_t y = 107; y <= 109; ++y) {
            r.object(110, y, kRoad);
        }
    }
    r.place(kForester, 110, 110, sim::Farm{});
    r.place(kMarket, 109, 104, std::nullopt);
    auto types = registry();
    Chain c{types,
            create(r, types),
            {},
            sim::make_building_id(sim::IslandId{0}, 1),
            sim::make_building_id(sim::IslandId{0}, 2)};
    return c;
}

const sim::Farm& farm_of(const sim::World& w, sim::BuildingId id) {
    const sim::Building* b = w.islands()[0]->find_building(id);
    REQUIRE(b != nullptr);
    return std::get<sim::Farm>(b->payload);
}

std::uint32_t city_wood(const sim::World& w) {
    return w.islands()[0]->cities[0]->stock.stock[goods_id("HOLZ")];
}

/// The ledger at one boundary (docs/sim/forestry.md 7). Every quantity is a disjoint physical
/// amount: reservations are inside Farm.out, cart loading is not receipt, ended figures' cargo is
/// history.
struct Ledger {
    std::uint64_t cuts = 0, credited = 0, worker_active = 0, delivered = 0, lost_raw = 0;
    std::uint64_t raw = 0, consumed = 0, made = 0, out = 0;
    std::uint64_t loaded = 0, cart_active = 0, offered = 0, lost_cart = 0;
    std::uint64_t reserved = 0, reserved_by_carts = 0;
    std::int64_t received = 0;
};

Ledger ledger(const Chain& c, std::uint32_t wood0) {
    Ledger l;
    const sim::ForestryStats& fs = c.stats.forestry;
    l.cuts = fs.cuts;
    l.credited = fs.raw_credited_88;
    l.delivered = fs.raw_delivered_88;
    l.lost_raw = fs.raw_lost_88;
    const auto it = fs.farms.find(c.forester_id);
    if (it != fs.farms.end()) {
        l.consumed = it->second.consumed_88;
        l.made = it->second.made_88;
    }
    const sim::Farm& f = farm_of(c.w, c.forester_id);
    l.raw = f.raw;
    l.out = f.out;
    l.reserved = f.reserved;
    const auto lt = c.stats.loaded_from.find(c.forester_id);
    l.loaded = lt != c.stats.loaded_from.end() ? lt->second : 0;
    const auto at = [](const std::map<sim::GoodsId, std::uint64_t>& m, sim::GoodsId g) {
        const auto i = m.find(g);
        return i != m.end() ? i->second : std::uint64_t{0};
    };
    l.offered = at(c.stats.unload_offered, goods_id("HOLZ"));
    l.lost_cart = at(c.stats.cart_lost, goods_id("HOLZ"));
    for (const auto& s : c.w.figures().list.slots) {
        if (!s || s->expiry_stamp.has_value()) {
            continue; // an ended figure's cargo fields are history, not goods
        }
        if (s->harvest.has_value()) {
            l.worker_active += s->cargo_88;
        } else if (s->job == sim::kJobMarketCart) {
            if (s->cargo_goods == goods_id("HOLZ")) {
                l.cart_active += s->cargo_88;
            }
            if (s->transport.target == c.forester_id) {
                l.reserved_by_carts += s->transport.reserved_88; // inside Farm.out, not a store
            }
        }
    }
    l.received = std::int64_t{city_wood(c.w)} - wood0;
    return l;
}

/// Every equation of docs/sim/forestry.md 7 with its unexplained term, which must be zero.
void check_balanced(const Ledger& l) {
    CHECK(l.credited == 256 * l.cuts);                                   // credit follows the cut
    CHECK(l.credited - l.worker_active - l.delivered - l.lost_raw == 0); // raw in workers
    CHECK(std::int64_t(l.delivered) - std::int64_t(l.consumed) - std::int64_t(l.raw) == 0); // farm
    CHECK(l.consumed * 14 == l.made * 32); // full steps only here
    CHECK(std::int64_t(l.made) - std::int64_t(l.out) - std::int64_t(l.loaded) == 0); // output
    CHECK(l.reserved == l.reserved_by_carts); // every reservation names an active cart's claim
    CHECK(l.reserved <= l.out);               // and lies inside the output store
    CHECK(std::int64_t(l.loaded) - std::int64_t(l.cart_active) - std::int64_t(l.offered) -
              std::int64_t(l.lost_cart) ==
          0);                                         // carts
    CHECK(std::int64_t(l.offered) - l.received == 0); // no clip below capacity
}

} // namespace

TEST_CASE("forester chain: three trees make 336 HOLZ and the cart delivers new wood (E4)") {
    Chain c = make_chain({{115, 111}, {111, 115}, {107, 111}});
    c.w.set_transport_stats(&c.stats);
    const std::uint32_t wood0 = city_wood(c.w);
    CHECK(wood0 == 0);
    CHECK(farm_of(c.w, c.forester_id).raw == 0);
    CHECK(farm_of(c.w, c.forester_id).out == 0);
    // Check the ledger at many boundaries along the way (every 2 000 ticks) and at the end.
    for (int i = 0; i < 60; ++i) {
        for (int k = 0; k < 200; ++k) {
            c.w.advance(20); // 10 ticks
        }
        INFO("after " << (i + 1) * 2000 << " ticks");
        check_balanced(ledger(c, wood0));
        REQUIRE(c.w.check_invariants() == "");
    }
    const Ledger l = ledger(c, wood0);
    // The hand-derived totals: all three trees cut, all raw delivered and consumed, 336 made.
    CHECK(l.cuts == kTrees);
    CHECK(l.credited == kCredited);
    CHECK(l.delivered == kCredited);
    CHECK(l.consumed == kCredited);
    CHECK(l.raw == 0);
    CHECK(l.made == kMade);
    // New wood reached the warehouse: a positive receipt, all of it explained by `made`.
    CHECK(l.received > 0);
    CHECK(l.loaded >= 256); // the cart takes from 1 t on
    CHECK(std::int64_t(l.out) + std::int64_t(l.cart_active) + l.received == std::int64_t(kMade));
    CHECK(c.stats.forestry.cut_mismatches == 0);
}

TEST_CASE("forester chain controls: no tree, unreachable trees, no road, a full warehouse") {
    SUBCASE("no resource: nothing cut, nothing made, nothing received") {
        Chain c = make_chain({});
        c.w.set_transport_stats(&c.stats);
        for (int k = 0; k < 6000; ++k) {
            c.w.advance(20);
        }
        const Ledger l = ledger(c, 0);
        check_balanced(l);
        CHECK(l.cuts == 0);
        CHECK(l.made == 0);
        CHECK(l.received == 0);
        CHECK(c.stats.forestry.searches_failed > 0);
    }
    SUBCASE("unreachable resource: a tree walled in by closed tiles is never claimed") {
        // Code-3 tiles are closed on every layer of these walk tables: a ring around (115, 111).
        Rig r = make_rig();
        r.object(115, 111, kTree);
        auto types = registry();
        for (std::int32_t y = 110; y <= 112; ++y) {
            for (std::int32_t x = 114; x <= 116; ++x) {
                if (x != 115 || y != 111) {
                    r.object(x, y, kWall);
                }
            }
        }
        r.place(kForester, 110, 110, sim::Farm{});
        sim::World w = create(r, types);
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        for (int k = 0; k < 6000; ++k) {
            w.advance(20);
        }
        CHECK(stats.forestry.searches > 0);
        CHECK(stats.forestry.searches_failed == stats.forestry.searches);
        CHECK(stats.forestry.cuts == 0);
        const sim::Island& is = *w.islands()[0];
        CHECK(is.tiles.object(is.tile_index(115, 111)) == kTree.v);
        CHECK_FALSE(is.tiles.claimed(is.tile_index(115, 111)));
    }
    SUBCASE("disconnected road: wood is made but no cart reaches it") {
        Chain c = make_chain({{115, 111}, {111, 115}, {107, 111}}, false);
        c.w.set_transport_stats(&c.stats);
        for (int k = 0; k < 12000; ++k) {
            c.w.advance(20);
        }
        const Ledger l = ledger(c, 0);
        check_balanced(l);
        CHECK(l.made == kMade);
        CHECK(l.loaded == 0);
        CHECK(l.received == 0);
        CHECK(l.out == kMade);
    }
    SUBCASE("a warehouse at its capacity: the clip is a named term, never silent") {
        // The city's wood starts 128 below its capacity (the default 50 t, CityStock::capacity).
        Chain c = make_chain({{115, 111}, {111, 115}, {107, 111}});
        sim::City& city = *sim::WorldTestAccess::islands(c.w)[0]->cities[0];
        city.stock.stock[goods_id("HOLZ")] = city.stock.capacity - 128;
        c.w.set_transport_stats(&c.stats);
        const std::uint32_t wood0 = city_wood(c.w);
        for (int k = 0; k < 12000; ++k) {
            c.w.advance(20);
        }
        const Ledger l = ledger(c, wood0);
        CHECK(l.made == kMade);
        CHECK(l.offered > 0);
        CHECK(l.received == 128); // filled to the capacity
        const std::int64_t clip = std::int64_t(l.offered) - l.received;
        CHECK(clip > 0); // the named loss term of the city clip (transport.cpp unload)
        CHECK(std::int64_t(l.made) - std::int64_t(l.out) - std::int64_t(l.loaded) == 0);
        // Upstream of the clip everything balances, so the clip is not absorbing another error.
        CHECK(std::int64_t(l.loaded) - std::int64_t(l.cart_active) - std::int64_t(l.offered) -
                  std::int64_t(l.lost_cart) ==
              0);
        CHECK(l.lost_cart == 0);
        CHECK(l.reserved == l.reserved_by_carts);
        CHECK(std::int64_t(l.delivered) - std::int64_t(l.consumed) - std::int64_t(l.raw) == 0);
    }
    SUBCASE("a full output store: production stops, harvesting and raw delivery go on") {
        // A control, not the chain: the output starts 10 below the capacity (1280) and no road
        // lets a cart empty it. The first step with raw makes 14 (out 1284, `cap <= out` is tested
        // before adding, E-5); every later step is StorageFull: nothing is consumed, the raw stock
        // keeps the deliveries, and the dispatch stays eligible while raw / 32 <= 27 (E-5, 3.1).
        Chain c = make_chain({{115, 111}, {111, 115}, {107, 111}}, false);
        sim::Island& is = *sim::WorldTestAccess::islands(c.w)[0];
        const sim::BuildingKey key = is.cache.by_id.at(c.forester_id);
        sim::Farm& f = std::get<sim::Farm>(
            is.cities[key.city.city.v]->index.lists[key.list].slots[key.slot]->payload);
        f.out = 1270;
        c.w.set_transport_stats(&c.stats);
        for (int k = 0; k < 12000; ++k) {
            c.w.advance(20);
        }
        const Ledger l = ledger(c, 0);
        CHECK(l.cuts == kTrees);
        CHECK(l.delivered == kCredited);
        CHECK(l.made == 14);
        CHECK(l.consumed == 32);
        CHECK(l.out == 1284);
        CHECK(l.raw == kCredited - 32);
        CHECK(std::int64_t(l.delivered) - std::int64_t(l.consumed) - std::int64_t(l.raw) == 0);
    }
}
