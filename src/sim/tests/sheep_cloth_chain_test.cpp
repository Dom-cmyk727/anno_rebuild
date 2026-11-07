// MAP-NEEDS-2 N2: grass -> animal -> Farm.raw -> wool -> weaver carrier -> cloth -> market cart ->
// city store, through the real scheduler on a synthetic miniature, and the same seam with other
// data (grain -> animal -> KORN -> mill carrier -> MEHL). The arithmetic is the vector file's; the
// time bound is docs/sim/early-needs.md section 7 item 7 (no first wool pickup before 41,500
// ticks). The geometry is the G6 layout (synthetic.cpp g6_scenario): market (110,110), roads along
// (111,113) (111..114,114), the processor at (111,115), the farm at (114,115).
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/figure.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "harvest_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
using namespace harvest_test;

namespace {

struct Config {
    sim::TypeId farm, processor, ripe;
    const char* raw_goods;   // the farm's output (the processor's input)
    const char* final_goods; // the processor's output
};

struct Run {
    std::uint64_t first_pickup_tick = 0, first_store_tick = 0;
    std::uint64_t farm_made = 0, farm_left = 0, loaded_from_farm = 0, store_final = 0,
                  store_raw = 0;
    std::uint64_t processor_out = 0, cart_final = 0, carrier_raw = 0, proc_in = 0;
    std::uint64_t cuts = 0, credited = 0, delivered = 0, lost = 0;
    std::uint64_t farm_reserved = 0, reserved_by_carriers = 0;
    bool invariants = true;
};

Run run_chain(const Config& c, std::uint64_t horizon_ticks) {
    Rig r = make_rig();
    // Ripe tiles: every free tile of the farm's disc (radius 5 around its centre (115, 116)), not
    // on a building footprint or a road, at most 40 (a finite pool: nothing regrows in this
    // fixture).
    int placed = 0;
    for (std::int32_t y = 111; y <= 121 && placed < 40; ++y) {
        for (std::int32_t x = 110; x <= 120 && placed < 40; ++x) {
            const int dx = x - 115, dy = y - 116;
            const bool in_disc = dx * dx + dy * dy <= 25;
            const bool farm_fp = x >= 114 && x <= 116 && y >= 115 && y <= 117;
            const bool proc_fp = x >= 111 && x <= 113 && y >= 115 && y <= 117;
            const bool market_fp = x >= 110 && x <= 112 && y >= 110 && y <= 112;
            const bool road = (x == 111 && y == 113) || (y == 114 && x >= 111 && x <= 114);
            if (in_disc && !farm_fp && !proc_fp && !market_fp && !road) {
                r.object(x, y, c.ripe);
                ++placed;
            }
        }
    }
    for (const auto& [x, y] :
         {std::pair{111, 113}, {111, 114}, {112, 114}, {113, 114}, {114, 114}}) {
        r.object(x, y, kRoad);
    }
    r.place(kMarket, 110, 110, std::nullopt);
    r.place(c.processor, 111, 115, std::nullopt);
    r.place(c.farm, 114, 115, sim::Farm{});
    auto types = registry();
    sim::World w = create(r, types);
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    sim::BuildingId farm_id, proc_id;
    for (auto& list : sim::WorldTestAccess::islands(w)[0]->cities[0]->index.lists) {
        for (auto& b : list.slots) {
            if (b && b->type == c.farm)
                farm_id = b->id;
            if (b && b->type == c.processor)
                proc_id = b->id;
        }
    }
    const sim::GoodsId raw = goods_id(c.raw_goods), fin = goods_id(c.final_goods);
    Run out;
    sim::City& city = *sim::WorldTestAccess::islands(w)[0]->cities[0];
    REQUIRE(city.stock.stock[raw] == 0);
    REQUIRE(city.stock.stock[fin] == 0); // nothing seeded
    for (std::uint64_t t = 0; t < horizon_ticks; t += 100) {
        w.advance(200); // 100 ticks
        const auto lf = stats.loaded_from.find(farm_id);
        if (out.first_pickup_tick == 0 && lf != stats.loaded_from.end() && lf->second > 0) {
            out.first_pickup_tick = w.clock().tick();
        }
        if (out.first_store_tick == 0 && city.stock.stock[fin] > 0) {
            out.first_store_tick = w.clock().tick();
        }
        if (w.check_invariants() != "") {
            out.invariants = false;
        }
    }
    const sim::Building* fb = w.islands()[0]->find_building(farm_id);
    const sim::Building* pb = w.islands()[0]->find_building(proc_id);
    const sim::Farm& f = std::get<sim::Farm>(fb->payload);
    const sim::Workshop& p = std::get<sim::Workshop>(pb->payload);
    out.farm_made = stats.forestry.farms.at(farm_id).made_88;
    out.farm_left = f.out;
    out.loaded_from_farm = stats.loaded_from.count(farm_id) ? stats.loaded_from.at(farm_id) : 0;
    out.store_final = city.stock.stock[fin];
    out.store_raw = city.stock.stock[raw];
    out.processor_out = p.out[0];
    out.proc_in = p.in[0];
    for (const auto& s : w.figures().list.slots) {
        if (!s || s->expiry_stamp.has_value())
            continue;
        if (s->job == sim::kJobMarketCart && s->cargo_goods == fin)
            out.cart_final += s->cargo_88;
        if (s->job == sim::kJobWorkshopCarrier && s->cargo_goods == raw)
            out.carrier_raw += s->cargo_88;
    }
    out.farm_reserved = f.reserved;
    for (const auto& s : w.figures().list.slots) {
        if (s && !s->expiry_stamp.has_value() && s->transport.target == farm_id) {
            out.reserved_by_carriers += s->transport.reserved_88;
        }
    }
    out.cuts = stats.forestry.cuts;
    out.credited = stats.forestry.raw_credited_88;
    out.delivered = stats.forestry.raw_delivered_88;
    out.lost = stats.forestry.raw_lost_88;
    return out;
}

} // namespace

TEST_CASE(
    "sheep chain: grass becomes wool, the weaver's carrier takes it, cloth reaches the store") {
    const Run a = run_chain({kSheepFarm5, kWeaver, kGrass, "WOLLE", "STOFFE"}, 400000);
    MESSAGE("first wool pickup tick "
            << a.first_pickup_tick << ", first cloth in the store " << a.first_store_tick
            << ", farm made " << a.farm_made << ", cloth in store " << a.store_final
            << ", wool in store " << a.store_raw << ", weaver in " << a.proc_in << " out "
            << a.processor_out << ", carrier cargo " << a.carrier_raw << ", farm left "
            << a.farm_left << ", loaded " << a.loaded_from_farm << ", cuts " << a.cuts);
    CHECK(a.invariants);
    // Exact measured pins of THIS MINIATURE (synthetic registry, 40 finite tiles, G6 geometry);
    // they are measurements, not predictions for the shipped town (docs/sim/early-needs.md section
    // 10).
    CHECK(a.first_pickup_tick == 57400);
    CHECK(a.first_store_tick == 103400);
    CHECK(a.farm_made == 706);
    CHECK(a.farm_left == 142);
    CHECK(a.loaded_from_farm == 564);
    CHECK(a.proc_in == 3);
    CHECK(a.processor_out == 30);
    CHECK(a.store_final == 276);
    CHECK(a.cuts == 40);
    // The reservation is released when idle: what the farm has reserved is exactly what its active
    // carriers hold.
    CHECK(a.farm_reserved == a.reserved_by_carriers);
    CHECK(a.cuts > 0);
    CHECK(a.credited == 256 * a.cuts);
    CHECK(a.delivered + a.lost <= a.credited); // the rest is in animals' hands
    // The farm's output is accounted: made = still in the farm + taken from it.
    CHECK(a.farm_made == a.farm_left + a.loaded_from_farm);
    // Nobody can take wool before the farm holds 1 t: not before the derived 41,500 ticks.
    REQUIRE(a.first_pickup_tick > 0);
    CHECK(a.first_pickup_tick >= 41500);
    // Cloth made is bounded by the recipe (6 out per 11 in, partial steps never beat it) and
    // reaches the store after the first pickup.
    const std::uint64_t cloth_total = a.store_final + a.processor_out + a.cart_final;
    CHECK(cloth_total > 0);
    CHECK(a.store_final > 0);
    CHECK(a.first_store_tick > a.first_pickup_tick);
    CHECK(cloth_total * 11 <=
          6 * a.loaded_from_farm + 6 * 11); // wool taken from the farm bounds the cloth
}

TEST_CASE(
    "alternate chain: grain becomes KORN, a mill's carrier takes it, MEHL reaches the store") {
    const Run a = run_chain({kBarnFarm, kMill, kGrain, "KORN", "MEHL"}, 400000);
    MESSAGE("first pickup tick " << a.first_pickup_tick << ", first MEHL in the store "
                                 << a.first_store_tick << ", farm made " << a.farm_made
                                 << ", MEHL in store " << a.store_final);
    CHECK(a.invariants);
    CHECK(a.cuts > 0);
    CHECK(a.farm_made == a.farm_left + a.loaded_from_farm);
    // Exact measured pins of this miniature (grain -> KORN -> mill -> MEHL).
    CHECK(a.first_pickup_tick == 45400);
    CHECK(a.first_store_tick == 260400);
    CHECK(a.farm_made == 3454);
    CHECK(a.store_final == 995);
    CHECK(a.farm_reserved == a.reserved_by_carriers);
}
