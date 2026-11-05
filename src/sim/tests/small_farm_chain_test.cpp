// MAP-SUPPLY-1 S4: the recurring supply chain on ORIGINAL data. The slice: Tutorial/02Trade01.sze,
// island 7, city 1 (player 0), IMPORTED buildings (nothing is built by command): four small farms
// 809 (one with 32 potato objects in reach at (261,344), two with grass at (307,349) and (292,333),
// one with neither at (259,352)), one Inn 1335 (301,351), one food stall 1310 (305,339), market
// halls and a Kontor, 13 houses. Warehouse alcohol starts at 0. Rules::house_updates is true in the
// test world only. Nothing is seeded, relocated or granted: the alcohol the Inn sells is made from
// potatoes cut by the farm's own worker. Docs: docs/sim/small-farm-supply.md section 10.
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "tutorial_town.hpp"
#include "world_access.hpp"

namespace {

namespace sim = anno::sim;
using namespace tutorial_town;

constexpr std::uint32_t kIsland7 = 7, kCity1 = 1;
constexpr sim::GoodsId kFood = 26, kAlcohol = 29;
constexpr std::uint64_t kCheckpoint =
    150000; // before the first regrowth can complete (>= 150000 after a cut)

struct Chain {
    sim::World w;
    sim::TransportStats stats;
    sim::Island* isl = nullptr;
    sim::City* city = nullptr;
    sim::CityRef ref;
    sim::BuildingId farm_potato, farm_empty, farm_grass1, farm_grass2, inn;
    explicit Chain(sim::World&& world) : w(std::move(world)) {}
    Chain(const Chain&) = delete;
    Chain& operator=(const Chain&) = delete;
    [[nodiscard]] const sim::Building* at(sim::BuildingId id) const {
        return isl->find_building(id);
    }
    [[nodiscard]] const sim::SmallFarm& farm(sim::BuildingId id) const {
        return std::get<sim::SmallFarm>(at(id)->payload);
    }
    [[nodiscard]] const sim::Inn& inn_state() const { return std::get<sim::Inn>(at(inn)->payload); }
};

std::unique_ptr<Chain> start(const Original& o, bool producers) {
    auto c = std::make_unique<Chain>(tutorial_world(o, true));
    for (auto& i : sim::WorldTestAccess::islands(c->w)) {
        if (i && i->index.v == kIsland7) {
            c->isl = &*i;
            for (auto& ci : i->cities) {
                if (ci && ci->number.v == kCity1) {
                    c->city = &*ci;
                }
            }
        }
    }
    REQUIRE(c->city != nullptr);
    c->ref = sim::CityRef{c->isl->index, c->city->number};
    const auto id_at = [&](int x, int y, std::uint32_t type) {
        const sim::Building* b = c->isl->building_at(c->w.config().layout, c->city->number, x, y);
        REQUIRE(b != nullptr);
        REQUIRE(b->type.v == type);
        return b->id;
    };
    c->farm_potato = id_at(261, 344, 809);
    c->farm_empty = id_at(259, 352, 809);
    c->farm_grass1 = id_at(307, 349, 809);
    c->farm_grass2 = id_at(292, 333, 809);
    c->inn = id_at(301, 351, 1335);
    c->w.set_transport_stats(&c->stats);
    if (!producers) {
        std::vector<sim::TileXY> tiles{{261, 344}, {259, 352}, {307, 349}, {292, 333}};
        sim::Command demolish;
        demolish.issuer = c->city->owner;
        demolish.body = sim::DemolishCmd{c->ref, tiles};
        const auto res = c->w.apply(demolish);
        REQUIRE_MESSAGE(res.ok(), std::string(sim::reject_name(res.status)));
    }
    return c;
}

std::uint32_t potatoes_ripe(const Chain& c) {
    std::uint32_t n = 0;
    for (int y = 342; y <= 348; ++y) {
        for (int x = 259; x <= 265; ++x) {
            n += c.isl->tiles.object(c.isl->tile_index(x, y)) == 709 ? 1U : 0U;
        }
    }
    return n;
}

/// The cargo of every living figure of `goods` whose home is a building of this island's city (the
/// other town of the scenario has its own carts and carriers; ended figures' cargo is history, not
/// stock).
std::uint64_t cargo_in_flight(const Chain& c, sim::GoodsId goods) {
    std::uint64_t n = 0;
    for (const auto& s : c.w.figures().list.slots) {
        if (s.has_value() && !s->expiry_stamp.has_value() && s->cargo_goods == goods &&
            s->home.valid() && c.isl->find_building(s->home) != nullptr) {
            n += s->cargo_88;
        }
    }
    return n;
}

} // namespace

TEST_CASE("supply chain K4/S4: the Tutorial island-7 town, farms -> carriers and carts -> Inn and "
          "stall -> households") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto A = start(*o, true);
    Chain& c = *A;
    const std::uint32_t N0 = c.city->stock.stock[kFood], A0 = c.city->stock.stock[kAlcohol];
    const std::uint32_t inn0 = c.inn_state().tavern;
    CHECK(A0 ==
          0); // the warehouse holds no alcohol: every unit the Inn sells is made after the start
    MESSAGE("N0 " << N0 << " A0 " << A0 << " Inn0 " << inn0 << " farm outs "
                  << c.farm(c.farm_potato).out[0] << "," << c.farm(c.farm_grass1).out[1] << ","
                  << c.farm(c.farm_grass2).out[1]);
    CHECK(c.farm(c.farm_potato).out[0] == 0);
    CHECK(c.farm(c.farm_potato).raw[0] == 0);
    const std::uint32_t ripe0 = potatoes_ripe(c);
    CHECK(ripe0 == 32);
    // the houses and what they hold before the first step
    struct HouseWatch {
        std::uint32_t tavern_slot = 0, food_slot = 0;
        std::uint32_t tavern0 = 0, food_prev = 0;
        bool tavern_up = false, food_up = false;
    };
    std::map<sim::BuildingId, HouseWatch> watch;
    for (auto& l : c.city->index.lists) {
        for (auto& b : l.slots) {
            if (const auto* h = b ? std::get_if<sim::House>(&b->payload) : nullptr) {
                const sim::BuildingType* t = o->types->find(b->type);
                const auto g = sim::house_group(c.w.config(), t->prodtype);
                REQUIRE(g.has_value());
                const auto view = o->types->population_view(*g, c.w.config());
                REQUIRE(view.ok());
                HouseWatch hw;
                hw.tavern_slot =
                    view.value().derived->lookup[o->types->population_roles().tavern_goods];
                hw.food_slot = view.value().derived->lookup[kFood];
                hw.tavern0 = h->stock[hw.tavern_slot];
                hw.food_prev = h->stock[hw.food_slot];
                watch[b->id] = hw;
            }
        }
    }
    CHECK(watch.size() == 13);
    // ---- run to the checkpoint ----
    std::uint32_t ripe_min = ripe0, ripe_up = 0, ripe_prev = ripe0, max_store_alcohol = 0;
    while (c.w.clock().tick() < kCheckpoint) {
        c.w.advance(100);
        REQUIRE_FALSE(c.w.faulted());
        max_store_alcohol = std::max(max_store_alcohol, c.city->stock.stock[kAlcohol]);
        for (auto& [id, hw] : watch) {
            const auto* h = std::get_if<sim::House>(&c.at(id)->payload);
            if (h == nullptr) {
                continue;
            }
            hw.tavern_up = hw.tavern_up || h->stock[hw.tavern_slot] > hw.tavern0;
            hw.food_up = hw.food_up || h->stock[hw.food_slot] > hw.food_prev;
            hw.food_prev = h->stock[hw.food_slot];
        }
        const std::uint32_t ripe = potatoes_ripe(c);
        ripe_up += ripe > ripe_prev ? 1U : 0U;
        ripe_min = std::min(ripe_min, ripe);
        ripe_prev = ripe;
    }
    const sim::ResidentStats& rs = c.stats.residents;
    const sim::ForestryStats& fs = c.stats.forestry;
    const auto& led_potato = fs.small_farms.at(c.farm_potato);
    const std::uint64_t cuts = ripe0 - potatoes_ripe(c);
    // ---- the alcohol ledger (every unexplained term zero) ----
    const sim::SmallFarm& fp = c.farm(c.farm_potato);
    std::uint64_t worker_cargo = 0;
    for (const auto& s : c.w.figures().list.slots) {
        if (s.has_value() && !s->expiry_stamp.has_value() && s->home == c.farm_potato &&
            s->job == 0x2a) {
            worker_cargo += s->cargo_88;
        }
    }
    const std::uint64_t inn_taken =
        rs.inn_taken_by_building.count(c.inn) ? rs.inn_taken_by_building.at(c.inn) : 0U;
    const std::uint64_t alcohol_in_flight = cargo_in_flight(c, kAlcohol);
    MESSAGE("tick " << c.w.clock().tick() << " cuts " << cuts << " raw0 " << fp.raw[0]
                    << " consumed0 " << led_potato.consumed_88[0] << " made0 "
                    << led_potato.made_88[0] << " out0 " << fp.out[0] << " store A "
                    << c.city->stock.stock[kAlcohol] << " (max " << max_store_alcohol << ") inn "
                    << c.inn_state().tavern << " inn_taken " << inn_taken << " in-flight alcohol "
                    << alcohol_in_flight << " worker cargo " << worker_cargo);
    // Measured and pinned (a legitimate change of the walk or timing model re-pins them): 24 fields
    // cut in 150 000 ticks = 6144 potatoes = 48 full steps, 624 alcohol made, 312 of it sold by the
    // Inn, 312 still in the farm.
    CHECK(cuts == 24);
    CHECK(led_potato.made_88[0] == 624);
    CHECK(inn_taken == 312);
    CHECK(fp.out[0] == 312);
    CHECK(max_store_alcohol ==
          312); // a cart took a load to the warehouse and the Inn's carrier fetched it from there
    CHECK(cuts > 0);
    CHECK(256U * cuts == fp.raw[0] + led_potato.consumed_88[0] +
                             worker_cargo); // potatoes: cut = in the raw + used + carried
    // Alcohol is conserved over the whole path, every term observed on its own: made by the farm
    // line = still in the farm + in the warehouse + in the Inn + sold by the Inn + on the way in a
    // cart or carrier of this town. The start holds none of it anywhere (farm out 0, warehouse 0,
    // Inn 0), so nothing here was imported.
    CHECK(led_potato.made_88[0] == fp.out[0] + (c.city->stock.stock[kAlcohol] - A0) +
                                       (c.inn_state().tavern - inn0) + inn_taken +
                                       alcohol_in_flight);
    CHECK(max_store_alcohol >= c.city->stock.stock[kAlcohol]);
    CHECK(inn_taken > 0); // residents bought tavern goods that did not exist at the start
    // households: tavern goods reached at least three houses that held none at the start
    std::uint32_t tavern_houses = 0, food_houses = 0, zero_start = 0;
    for (const auto& [id, hw] : watch) {
        tavern_houses +=
            (hw.tavern_up && hw.tavern0 == 0) ? 1U : 0U; // only houses that started with none
        food_houses += hw.food_up ? 1U : 0U;
        zero_start += hw.tavern0 == 0 ? 1U : 0U;
    }
    MESSAGE("houses with a tavern delivery "
            << tavern_houses << " (of " << zero_start
            << " that held none at the start), with a food delivery " << food_houses);
    CHECK(tavern_houses >= 3);
    CHECK(food_houses >= 3);
    // ---- the food ledger ----
    const std::uint64_t stall_taken = rs.merged_taken_by_city.count({c.ref, kFood})
                                          ? rs.merged_taken_by_city.at({c.ref, kFood})
                                          : 0U;
    std::uint64_t food_loaded = 0, food_made = 0, food_out = 0;
    for (const sim::BuildingId f : {c.farm_grass1, c.farm_grass2, c.farm_potato, c.farm_empty}) {
        food_loaded += c.stats.loaded_from.count(f) ? c.stats.loaded_from.at(f) : 0U;
        food_made += fs.small_farms.count(f) ? fs.small_farms.at(f).made_88[1] : 0U;
        food_out += c.farm(f).out[1];
    }
    const std::uint64_t food_in_flight = cargo_in_flight(c, kFood);
    // the potato farm's out[0] (alcohol) was loaded too: remove it from the loaded sum
    const std::uint64_t food_loaded_only =
        food_loaded -
        (c.stats.loaded_from.count(c.farm_potato) ? c.stats.loaded_from.at(c.farm_potato) : 0U);
    MESSAGE("food: made " << food_made << " out " << food_out << " loaded " << food_loaded_only
                          << " in flight " << food_in_flight << " stall took " << stall_taken
                          << " N " << N0 << " -> " << c.city->stock.stock[kFood]);
    CHECK(food_made == food_out + food_loaded_only);
    CHECK(static_cast<std::int64_t>(c.city->stock.stock[kFood]) - static_cast<std::int64_t>(N0) ==
          static_cast<std::int64_t>(food_loaded_only) - static_cast<std::int64_t>(food_in_flight) -
              static_cast<std::int64_t>(stall_taken));
    CHECK(stall_taken == 757);
    CHECK(c.city->stock.stock[kFood] == 17250);
    CHECK(stall_taken > 0);
    CHECK(fs.raw_lost_88 == 0);
    CHECK(c.w.check_invariants() == "");
    const std::uint32_t N_checkpoint = c.city->stock.stock[kFood];
    const std::uint64_t taken_checkpoint = stall_taken;
    // ---- repeated cycles: the potato fields grow back and are cut again (bounded run to 600 000
    // ticks) ----
    while (c.w.clock().tick() < 600000) {
        c.w.advance(100);
        const std::uint32_t ripe = potatoes_ripe(c);
        ripe_up += ripe > ripe_prev ? 1U : 0U;
        ripe_min = std::min(ripe_min, ripe);
        ripe_prev = ripe;
    }
    const std::uint64_t potatoes_used_total =
        fs.small_farms.at(c.farm_potato).consumed_88[0] + c.farm(c.farm_potato).raw[0];
    const std::uint64_t potatoes_in_hand = potatoes_used_total;
    MESSAGE("600000 ticks: ripe min " << ripe_min << " increases " << ripe_up
                                      << " potatoes used+held " << potatoes_used_total
                                      << " (initial ripe stock " << 256U * ripe0 << ") Inn sold "
                                      << c.stats.residents.inn_taken_by_building.at(c.inn));
    CHECK(ripe_min == 7);
    CHECK(ripe_up == 43);
    CHECK(potatoes_in_hand == 24576);
    CHECK(ripe_up > 0); // a young field became ripe again: the growth cycle ran
    CHECK(
        potatoes_in_hand >
        256U *
            ripe0); // more potatoes cut than the ripe stock the fields held at the start: regrowth
    // ---- the producer-removed control: the same town, the four farms demolished by command at
    // tick 0 ----
    auto B = start(*o, false);
    Chain& d = *B;
    const std::uint32_t Nb0 = d.city->stock.stock[kFood];
    CHECK(d.inn_state().tavern == inn0);
    while (d.w.clock().tick() < kCheckpoint) {
        d.w.advance(100);
        REQUIRE_FALSE(d.w.faulted());
    }
    const std::uint64_t b_taken = d.stats.residents.merged_taken_by_city.count({d.ref, kFood})
                                      ? d.stats.residents.merged_taken_by_city.at({d.ref, kFood})
                                      : 0U;
    const std::uint64_t b_inn_taken = d.stats.residents.inn_taken_by_building.count(d.inn)
                                          ? d.stats.residents.inn_taken_by_building.at(d.inn)
                                          : 0U;
    MESSAGE("control: N " << Nb0 << " -> " << d.city->stock.stock[kFood] << " stall took "
                          << b_taken << " Inn " << inn0 << " -> " << d.inn_state().tavern
                          << " sold " << b_inn_taken << " (run with farms: N -> " << N_checkpoint
                          << " took " << taken_checkpoint << ")");
    CHECK(Nb0 == N0);
    CHECK(static_cast<std::int64_t>(d.city->stock.stock[kFood]) - static_cast<std::int64_t>(Nb0) ==
          -static_cast<std::int64_t>(b_taken)); // nothing comes in
    CHECK(d.city->stock.stock[kAlcohol] == 0);
    CHECK(d.inn_state().tavern == inn0); // no alcohol was made anywhere: the Inn never receives any
    CHECK(b_inn_taken == 0);
    CHECK(d.city->stock.stock[kFood] == 16654);
    CHECK(d.city->stock.stock[kFood] < N_checkpoint); // with the farms the store ends higher
    CHECK(d.w.check_invariants() == "");
}
