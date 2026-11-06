// MAP-POP-LIVE-1 L4: the live House update through the production scheduler
// (docs/sim/population-live.md sections 1, 6, 12, 19) with Rules::house_updates on.
//
// Every number below is hand-derived (tools/house_live_ref.py and fixtures/synthetic/sim/
// house_live_vectors.txt carry the chain and the stage pass independently). Setup shared by the
// tests: island 0 at (100, 100) 32 x 32, city 0 of player 0, house type 9501 (PRODTYPE 97, walker
// maximum 2, figure ids 203..205) at (110, 110) with serial 1, a stall at (114, 110). The world
// PRNG table is the 16 values 7 i + 1 with cursor 13 (seed 77 mod 16), so the draws are T[14] = 99,
// T[15] = 106, T[0] = 1, ... The city ring visits city 0 on advance number 1 + 16 k, so the House
// of city 0 is visited at ticks t = 1 + 16 k (the first advance, d = 1, one ring slot per advance);
// the first taken gate (u16(t - last) >= 2500, last = 0) is therefore t = 2513 and the next one,
// with last = 2500, is t = 5009.
//
// Spawn variant: k = ((r & 1) + y + x + serial) mod 3 = ((r & 1) + 221) mod 3: r = 99 (odd) -> 222
// mod 3 = 0 -> figure 203; r = 106 (even) -> 221 mod 3 = 2 -> figure 205.
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/create.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/house_tick.hpp>
#include <anno/sim/house_transition.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/resident_job.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "slice_helpers.hpp"
#include "world_access.hpp"

namespace {

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

constexpr sim::TypeId kP{9501}; // group 0, variants 9501 / 9503 (count 2, step 2), walker max 2
constexpr sim::TypeId kP3{9503};
constexpr sim::TypeId kPnoEnt{9505}; // group 0 without an entrance point
constexpr sim::TypeId kS0{9520}; // group 1, variants 9520..9522, walker max 1, costs tools + wood
constexpr sim::TypeId kS1{9521};
constexpr sim::TypeId kS2{9522};
constexpr sim::TypeId kStall{9530};
constexpr sim::GoodsId kFood = 26, kOther = 27;
constexpr std::uint32_t kTools = 0x29, kWood = 0x2a;

struct Opt {
    bool idsec = false;           // goods 26 -> 27 (the replay of a delivery)
    bool low_negative = false;    // group 0 food table: -2 below index 8, else 2 (else all 2)
    bool bau27 = false;           // goods 27 is enabled by stage 1 only
    bool stages = false;          // [BAUSTUFEN]: stage 1 = group 0 with 5 residents, stage 2 = 50
    std::uint32_t high_stage = 0; // [BAUSTUFEN]: this one stage (24..28) = group 0 with 5 residents
    bool stall_next = false;      // house type 9560 (group 0) whose ladder next 9580 is a stall
    bool declare_special = false; // C3: ... and 9560 is declared a special successor
    bool orphan = false;          // an extra House type whose PRODTYPE names no group of the table
    std::uint16_t g1_menge_food =
        0; // MAP-POP-CLOSE-1 C2: group 1 food Menge and Wohnmax 65535 (0 = shipped)
};
constexpr sim::TypeId kPioneer{
    9560}; // PRODTYPE 97, ladder next 9580 = a market stall (real: 3852 -> 3860)
constexpr sim::TypeId kPioneerNext{9580};
constexpr sim::TypeId kOrphan{9540}; // PRODTYPE 99: group 2, not in the two-group table

sim::BuildingType house_t(sim::TypeId id, std::uint32_t prodtype, std::uint32_t walkers,
                          std::uint8_t head11, bool entrance = true) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = prodtype;
    t.cls = sim::RuntimeClass::House;
    t.width = t.height = 2;
    t.action_radius = 8;
    t.head_raw.assign(56, 0);
    t.flags_word =
        0x4; // ordinary land: a Build command can place it (placement.hpp ordinary_class)
    t.head_raw[11] = head11;
    if (entrance) {
        t.entrances = {{3, 3}};
    }
    t.has_prod = true;
    t.prod.kind = sim::BuildingClass::Other;
    t.prod.max_figures = {walkers, 0, 0};
    t.prod.figure_type_count = {3, 0, 0};
    t.prod.figure_ids = {203, 204, 205, 0, 0, 0, 0, 0, 0};
    return t;
}

std::shared_ptr<const sim::TypeRegistry> registry(Opt o = {},
                                                  std::vector<sim::ClassInfo> classes = {},
                                                  bool group0_downgradable = false,
                                                  bool empty_group = false) {
    sim::HouseGroup g0;
    g0.number = 0;
    g0.wohnmax = 8;
    g0.no_downgrade = !group0_downgradable;
    g0.needs = {{kFood, true, 41}, {kOther, true, 16}};
    g0.vermehr = {-16, -9, 0, 12, 32};
    for (std::size_t i = 0; i < 21; ++i) {
        g0.nahrung[i] = o.low_negative && i < 8 ? -2 : 2;
        g0.bedarf[i] = 2;
        g0.infra[i] = 2;
    }
    sim::HouseGroup g1;
    g1.number = 1;
    g1.wohnmax = 15;
    g1.needs = {{kFood, true, 60}, {kOther, true, 40}};
    if (empty_group) {
        g1.needs.clear();
    }
    g1.vermehr = g0.vermehr;
    if (o.g1_menge_food != 0) {
        g1.wohnmax = 65535;
        g1.needs[0].menge = o.g1_menge_food;
    }
    for (std::size_t i = 0; i < 21; ++i) {
        g1.nahrung[i] = i < 10 ? -2 : 2;
        g1.bedarf[i] = 2;
        g1.infra[i] = 2;
    }
    std::vector<sim::GoodsInfo> goods = sim::original_goods();
    if (o.idsec) {
        goods[kFood].id_sec = kOther;
    }
    if (o.bau27) {
        goods[kOther].bau_first = 1;
    }
    sim::TypeRegistry::Builder b;
    b.config(syn::population_config());
    b.walk_tables(sim::synthetic_walk_tables());
    b.goods(goods);
    if (!classes.empty()) {
        b.classes(classes);
    }
    b.house_groups({g0, g1});
    if (o.stages) {
        b.stage_table({{1, 0, 0, 5}, {2, 0, 0, 50}});
    }
    if (o.high_stage != 0) {
        b.stage_table({{o.high_stage, 0, 0, 5}});
    }
    if (o.orphan) {
        b.add(house_t(kOrphan, 99, 2, 0));
    }
    if (o.stall_next) {
        b.add(house_t(kPioneer, 97, 2, 0));
        if (o.declare_special) {
            b.special_successor(kPioneer);
        }
        sim::BuildingType next_stall;
        next_stall.id = kPioneerNext;
        next_stall.prodtype = 161;
        next_stall.cls = sim::RuntimeClass::MarketStall;
        next_stall.entrances = {{3, 3}};
        next_stall.output_goods = {kFood};
        b.add(next_stall);
    }
    b.add(house_t(kP, 97, 2, static_cast<std::uint8_t>(2 | (2 << 6))));
    b.add(house_t(kP3, 97, 2, 0));
    b.add(house_t(kPnoEnt, 97, 2, 0, false));
    sim::BuildingType s0 = house_t(kS0, 98, 1, static_cast<std::uint8_t>(3 | (1 << 6)));
    s0.cost_goods = {{kTools, 512}, {kWood, 1024}};
    b.add(s0);
    b.add(house_t(kS1, 98, 1, 0));
    b.add(house_t(kS2, 98, 1, 0));
    sim::BuildingType stall;
    stall.id = kStall;
    stall.prodtype = 161;
    stall.cls = sim::RuntimeClass::MarketStall;
    stall.entrances = {{3, 3}};
    stall.output_goods = {kFood};
    b.add(stall);
    for (const std::uint32_t id : {203U, 204U, 205U}) {
        b.figure_type(sim::FigureType{id, 805300, 3 * 256, 0, 1000});
    }
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

sim::House make_house(sim::TypeId type, std::int32_t residents_64, std::uint32_t e,
                      std::vector<std::uint32_t> stock, std::uint16_t last = 0) {
    sim::House h;
    h.last = last;
    h.residents_64 = residents_64;
    h.e = e;
    h.stock = std::move(stock);
    h.walkers.assign(type == kP || type == kP3 || type == kPnoEnt ? 2 : 1, sim::FigureId{});
    return h;
}

struct Rig {
    sim::ScenarioSpec spec;
    void place(sim::TypeId type, std::int32_t x, std::int32_t y,
               std::optional<sim::Payload> payload = std::nullopt) {
        sim::ScenarioSpec::BuildingSpec b;
        b.type = type;
        b.x = x;
        b.y = y;
        b.payload = std::move(payload);
        spec.islands[0].cities[0].buildings.push_back(std::move(b));
    }
};

Rig make_rig() {
    Rig r;
    r.spec = syn::empty_scenario(77);
    std::vector<std::uint16_t> table;
    for (std::uint16_t i = 0; i < 16; ++i) {
        table.push_back(static_cast<std::uint16_t>(7 * i + 1));
    }
    r.spec.rng_table = table;
    sim::ScenarioSpec::PlayerSpec p;
    p.id = sim::PlayerId{0};
    r.spec.players.push_back(p);
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 100, 100, 32, 32);
    is.owner_runs = {{0, 32U * 32U}};
    is.cities.push_back(syn::plain_city(0, 0));
    r.spec.islands.push_back(std::move(is));
    return r;
}

sim::World create(const Rig& r, const std::shared_ptr<const sim::TypeRegistry>& types,
                  bool live = true) {
    sim::Rules rules;
    rules.house_updates = live;
    auto w = sim::World::create(r.spec, types, syn::population_config(), rules);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

sim::City& city0(sim::World& w) {
    return *sim::WorldTestAccess::islands(w)[0]->cities[0];
}
const sim::Building* at(const sim::World& w, std::int32_t x, std::int32_t y) {
    return w.islands()[0]->building_at(w.config().layout, sim::CityNo{0}, x, y);
}
const sim::House& house_of(const sim::World& w) {
    const sim::Building* b = at(w, 110, 110);
    REQUIRE(b != nullptr);
    const auto* h = std::get_if<sim::House>(&b->payload);
    REQUIRE(h != nullptr);
    return *h;
}
bool same_house(const sim::House& a, const sim::House& b) {
    return a.last == b.last && a.residents_64 == b.residents_64 && a.e == b.e &&
           a.stock == b.stock && a.walkers == b.walkers;
}
std::uint32_t object_at(const sim::World& w, std::int32_t x, std::int32_t y) {
    const sim::Island& is = *w.islands()[0];
    return is.tiles.object(is.tiles.index_of(x, y));
}
std::size_t figure_count(const sim::World& w) {
    std::size_t n = 0;
    for (const auto& s : w.figures().list.slots) {
        n += s.has_value() ? 1U : 0U;
    }
    return n;
}
std::vector<const sim::Figure*> walkers(const sim::World& w) {
    std::vector<const sim::Figure*> out;
    for (const auto& s : w.figures().list.slots) {
        if (s && s->job == sim::kJobResident) {
            out.push_back(&*s);
        }
    }
    return out;
}
bool ended(const sim::World& w, sim::FigureId id) {
    const sim::Figure* f = sim::find_figure(w.figures(), id);
    return f == nullptr || f->expiry_stamp.has_value();
}

/// One advance of one tick; the invariants every 64 ticks (and at once when `always`).
void step(sim::World& w, bool always = false) {
    w.advance(2);
    if (always || w.clock().tick() % 64 == 0) {
        REQUIRE_MESSAGE(w.check_invariants() == "", w.check_invariants());
    }
}

/// Advances until `done` (at most `limit` ticks).
template <class F> void until(sim::World& w, F done, std::uint64_t limit = 20000) {
    for (std::uint64_t i = 0; i < limit; ++i) {
        step(w);
        if (done()) {
            return;
        }
    }
    FAIL("the condition did not hold within the limit");
}

std::size_t draws_at(const sim::World& w, sim::DrawSite site) {
    for (const sim::DrawRecord& r : w.last_output().draws) {
        if (r.site == site) {
            return r.count;
        }
    }
    return 0;
}

/// The standard chain world: house (110, 110) with E 640, 5 residents, empty stock; a stall; the
/// city store of food = `store`.
sim::World chain_world(const std::shared_ptr<const sim::TypeRegistry>& types,
                       std::uint32_t store = 100) {
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 320, 640, {0, 0}));
    r.place(kStall, 114, 110);
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = store;
    return w;
}

} // namespace

TEST_CASE(
    "house live: shortage, spawn, purchase, delivery and the next gate through the scheduler") {
    // Chain K1 of house_live_vectors.txt (goods 26 -> 27 IdSec, store 100). Hand derivation:
    //  gate 1 (t = 2513, last 0): E' = 635 + 5 = 640, stock stays 0, lowest 0 < 102 -> ONE draw
    //   (T[14] = 99, figure 203); mood = min(-2, 2, 2) = -2 -> residents 320 - 16 = 304; last 2500.
    //  purchase: R = 4, threshold trunc(4*41*256/4096) = 10, store 100 > 10, demand
    //   trunc(41*640*256/44236) = 151 -> amount min(100, 151, 768) = 100: city store 100 -> 0,
    //   billed +100, house stock +100*16 = 1600 on slot 0 and, by the IdSec replay, on slot 1.
    //  gate 2 (t = 5009, last 2500): E' = trunc(127*640/128) + 4 = 639, stock 1587 / 1587
    //   (trunc(127*1600/128)), s0 = 93 (< 102: ONE draw, T[15] = 106, figure 205), mood
    //   (2 + 2 + 2 + 1) / 3 = 2 -> residents 304 + 32 = 336.
    Opt o;
    o.idsec = true;
    o.low_negative = true;
    const auto types = registry(o);
    sim::World w = chain_world(types, 100);
    REQUIRE(w.rng().cursor() == 13);
    REQUIRE(w.rng().table()[14] == 99);
    REQUIRE(w.rng().table()[15] == 106);
    REQUIRE(sim::serial_of(at(w, 110, 110)->id) == 1);
    const sim::House initial = house_of(w);

    // The skipped gates: every visit before t = 2513 (t = 1, 17, ..., 2497) changes nothing and
    // draws nothing.
    while (w.clock().tick() < 2512) {
        step(w);
        REQUIRE(same_house(house_of(w), initial));
        REQUIRE(w.rng().cursor() == 13);
    }
    CHECK(figure_count(w) == 0);
    CHECK(w.last_output().draws.empty());

    // The taken gate.
    until(w, [&] { return house_of(w).last != 0; }, 100);
    CHECK(w.clock().tick() == 2513);
    CHECK(house_of(w).last == 2500); // last += period, not = t
    CHECK(house_of(w).e == 640);
    CHECK((house_of(w).stock == std::vector<std::uint32_t>{0, 0}));
    CHECK(house_of(w).residents_64 == 304);
    REQUIRE(w.last_output().draws.size() == 1);
    CHECK(w.last_output().draws[0].site == sim::DrawSite::HouseWalkerTarget);
    CHECK(w.last_output().draws[0].count == 1);
    CHECK(w.last_output().rng_cursor_before == 13);
    CHECK(w.last_output().rng_cursor_after == 14);
    REQUIRE(walkers(w).size() == 1);
    const sim::FigureId first = walkers(w)[0]->id;
    CHECK(walkers(w)[0]->kind == 203);
    CHECK(walkers(w)[0]->home == at(w, 110, 110)->id);
    CHECK(house_of(w).walkers[0] == first); // the slot is taken at the merge
    CHECK(house_of(w).walkers[1] == sim::FigureId{});

    // The walker's life: purchase, walk home, delivery, end, slot released.
    until(w, [&] { return ended(w, first); });
    const sim::Figure* f = sim::find_figure(w.figures(), first);
    REQUIRE(f != nullptr);
    CHECK(f->resident->cargo[0] == sim::ResidentCargo{kFood, 100});
    CHECK(f->resident->cargo[1] == sim::ResidentCargo{});
    CHECK(city0(w).stock.stock[kFood] == 0); // down by the bought amount
    CHECK(city0(w).billed[kFood] == 100);    // up by the requested amount
    CHECK((house_of(w).stock == std::vector<std::uint32_t>{1600, 1600}));
    CHECK(house_of(w).walkers[0] == sim::FigureId{});
    CHECK(house_of(w).last == 2500); // the purchase does not touch the gate

    // The next taken gate sees the new stock.
    until(w, [&] { return house_of(w).last != 2500; });
    CHECK(w.clock().tick() == 5009);
    CHECK(house_of(w).last == 5000);
    CHECK(house_of(w).e == 639);
    CHECK((house_of(w).stock == std::vector<std::uint32_t>{1587, 1587}));
    CHECK(house_of(w).residents_64 == 336);
    REQUIRE(w.last_output().draws.size() == 1);
    CHECK(w.last_output().draws[0].site == sim::DrawSite::HouseWalkerTarget);
    CHECK(w.last_output().rng_cursor_after == 15);
    REQUIRE(walkers(w).size() == 2); // the first one is still in the registry (expiry), the new one
    // The new walker (the store is empty: no stand) has ended within its first call; it was
    // announced with the figure type of T[15] = 106 (even): 205.
    bool announced = false;
    for (const sim::SimEvent& e : w.last_output().events) {
        announced = announced || (e.kind == sim::SimEventKind::FigureSpawned && e.code == 205);
    }
    CHECK(announced);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("house live: a refused send still consumes the draw") {
    // X14: the house type without an entrance point draws once (T[14]) and sends nothing.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kPnoEnt, 110, 110, make_house(kPnoEnt, 320, 640, {0, 0}));
    sim::World w = create(r, types);
    until(w, [&] { return house_of(w).last != 0; }, 3000);
    CHECK(w.clock().tick() == 2513);
    CHECK(draws_at(w, sim::DrawSite::HouseWalkerTarget) == 1);
    CHECK(w.last_output().rng_cursor_after == 14);
    CHECK(walkers(w).empty());
    CHECK(house_of(w).walkers[0] == sim::FigureId{});
}

TEST_CASE(
    "house live: the upgrade tick draws the spawn BEFORE the variant, and every effect lands") {
    // A group-0 house at the cap: residents 512, E 640, stock {2000, 500}, tools 1000, wood 2000.
    //  E' = 635 + 8 = 643; stock {1984, 496}; s0 = 115, s1 = 74 -> lowest 74 < 102: spawn draw
    //  T[14] = 99 (figure 203). Mood 2 -> residents 544 > 512: cap -> 512, UpgradeCheck: food
    //  trunc(1536*1984/(643*60)) = 78 -> table[(78+4)>>3 = 10] = 2 > 0, mean 74, infra 160:
    //  eligible; materials (512 tools, 1024 wood) available -> Upgrade: spend (tools 488, wood
    //  976), variant draw T[15] = 106: 106 mod 3 = 1 -> type 9521 (the other order would draw 99 ->
    //  9520 and give the walker figure 205). SetGroup (upgrade): satisfaction 115 / 74, new stock
    //  trunc(60*115*643/1536) = 2888 and trunc(40*74*643/1536) = 1239; E and residents are kept.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 512, 640, {2000, 500}));
    sim::World w = create(r, types);
    city0(w).stock.stock[kTools] = 1000;
    city0(w).stock.stock[kWood] = 2000;
    REQUIRE(house_of(w).walkers.size() == 2);
    until(w, [&] { return house_of(w).last != 0; }, 3000);
    CHECK(w.clock().tick() == 2513);
    const sim::Building& b = *at(w, 110, 110);
    CHECK(b.type == kS1);
    CHECK(object_at(w, 110, 110) == kS1.v); // the footprint is re-stamped
    CHECK(object_at(w, 111, 111) == kS1.v);
    CHECK(house_of(w).last == 2500);
    CHECK(house_of(w).e == 643);
    CHECK(house_of(w).residents_64 == 512);
    CHECK((house_of(w).stock == std::vector<std::uint32_t>{2888, 1239}));
    CHECK(city0(w).stock.stock[kTools] == 488);
    CHECK(city0(w).stock.stock[kWood] == 976);
    CHECK(city0(w).build_goods[0].spent == 512);
    CHECK(city0(w).build_goods[1].spent == 1024);
    CHECK(city0(w).index.house_counts[0] == 0);
    CHECK(city0(w).index.house_counts[1] == 1);
    // Two draws, in this order: the spawn (T[14]) and the variant (T[15]).
    CHECK(draws_at(w, sim::DrawSite::HouseWalkerTarget) == 1);
    CHECK(draws_at(w, sim::DrawSite::HouseVariant) == 1);
    CHECK(w.last_output().rng_cursor_before == 13);
    CHECK(w.last_output().rng_cursor_after == 15);
    REQUIRE(walkers(w).size() == 1);
    CHECK(walkers(w)[0]->kind == 203); // spawn used T[14] = 99 (odd), not 106
    // (the walker finds no stand and walks home within its first call: it has ended already)
    bool announced = false;
    for (const sim::SimEvent& e : w.last_output().events) {
        announced = announced || (e.kind == sim::SimEventKind::FigureSpawned &&
                                  e.building == b.id && e.code == 203);
    }
    CHECK(announced);
    CHECK(house_of(w).walkers.size() == 1); // lazy trim of the empty trailing slot
    CHECK(w.check_invariants() == "");
}

TEST_CASE(
    "house live: a satisfied house draws nothing and a refused upgrade changes only the gate") {
    // stock {5000, 5000}: lowest 160 -> no spawn draw; the upgrade has no materials in the store.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 512, 640, {5000, 5000}));
    sim::World w = create(r, types);
    until(w, [&] { return house_of(w).last != 0; }, 3000);
    CHECK(w.last_output().draws.empty());
    CHECK(at(w, 110, 110)->type == kP); // UpgradeRefusedMaterials: nothing spent, nothing drawn
    CHECK(house_of(w).residents_64 == 512);
    CHECK(house_of(w).e == 643);
    CHECK(walkers(w).empty());
}

TEST_CASE("house live: downgrade and removal, with the spawn draw first") {
    Opt o;
    o.low_negative = true;
    const auto types = registry(o);
    SUBCASE("downgrade of a group-1 house: variant of 9501 after the spawn draw") {
        // residents 300, E 512, stock {1000, 800}: E' = 508 + 4 = 512, stock {992, 793}, s0 = 49,
        // s1 = 59, lowest 49: spawn T[14] (figure 203). Group 1 food table -2 below index 10: mood
        // -2
        // -> residents 284 < floor 48 * 8 = 384 -> Downgrade. Variant of 9501 (count 2, step 2):
        // T[15] = 106 even -> idx 0 -> 9501. SetGroup (downgrade): E = 4 << 7 = 512, stock
        // trunc(41*49*512/1536) = 669 and trunc(16*59*512/1536) = 314.
        Rig r = make_rig();
        r.place(kS0, 110, 110, make_house(kS0, 300, 512, {1000, 800}));
        sim::World w = create(r, types);
        until(w, [&] { return house_of(w).last != 0; }, 3000);
        CHECK(w.clock().tick() == 2513);
        CHECK(at(w, 110, 110)->type == kP);
        CHECK(house_of(w).e == 512);
        CHECK(house_of(w).residents_64 == 284);
        CHECK((house_of(w).stock == std::vector<std::uint32_t>{669, 314}));
        CHECK(city0(w).index.house_counts[1] == 0);
        CHECK(city0(w).index.house_counts[0] == 1);
        CHECK(draws_at(w, sim::DrawSite::HouseWalkerTarget) == 1);
        CHECK(draws_at(w, sim::DrawSite::HouseVariant) == 1);
        REQUIRE(walkers(w).size() == 1);
        CHECK(walkers(w)[0]->kind == 203);
        CHECK(house_of(w).walkers.size() == 2); // grown to the new maximum
        CHECK(w.check_invariants() == "");
    }
    SUBCASE("removal of a group-0 house below the floor: the spawn draw, no figure") {
        // residents 70, E 512 -> E' = 508 + 1 = 509, lowest 0: spawn draw T[14]; mood -2 ->
        // residents 54 < floor 64 (NoDowngrade) -> Remove. The request names a house that is gone
        // by the merge: no figure.
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 70, 512, {0, 0}));
        sim::World w = create(r, types);
        until(w, [&] { return at(w, 110, 110) == nullptr; }, 3000);
        CHECK(w.clock().tick() == 2513);
        CHECK(object_at(w, 110, 110) == 0);
        CHECK(city0(w).index.house_counts[0] == 0);
        CHECK(draws_at(w, sim::DrawSite::HouseWalkerTarget) == 1);
        CHECK(draws_at(w, sim::DrawSite::HouseVariant) == 0);
        CHECK(w.last_output().rng_cursor_after == 14);
        CHECK(walkers(w).empty());
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("house live: the stage pass runs after the building ticks and unlocks a need") {
    // Stage 1 = 5 residents of group 0 and above; goods 27 is enabled by stage 1 only (bau 1).
    // House: residents 320 (5), E 640, stock {0, 1000}; player mask 1.
    //  t = 2513: the House tick (mask 1: need 27 NOT selected, stock stays 1000), then the pass:
    //   residents 352 -> 5 >= 5: city mask 0x2, stamp 2513; player 1 < 2 -> flags 0x3.
    //  t = 5009: the House tick sees mask 3: stock1 = trunc(127*1000/128) = 992; the pass is gated
    //   (2496 < 2500, the stamp stays 2513); t = 5025: the pass runs again (stamp 5025).
    Opt o;
    o.bau27 = true;
    o.stages = true;
    const auto types = registry(o);
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 320, 640, {0, 1000}));
    sim::World w = create(r, types);
    CHECK(city0(w).stage_mask == 0);
    CHECK(city0(w).stage_stamp == 0);
    until(w, [&] { return house_of(w).last != 0; }, 3000);
    CHECK(w.clock().tick() == 2513);
    CHECK((house_of(w).stock == std::vector<std::uint32_t>{0, 1000})); // not decayed: pass is later
    CHECK(house_of(w).residents_64 == 352);
    CHECK(city0(w).stage_mask == 0x2);
    CHECK(city0(w).stage_stamp == 2513); // the stamp is t, not stamp + 2500
    CHECK(w.players().find(sim::PlayerId{0})->flags == 0x3);
    until(w, [&] { return house_of(w).last != 2500; });
    CHECK(w.clock().tick() == 5009);
    CHECK(
        (house_of(w).stock == std::vector<std::uint32_t>{0, 992})); // the unlock reached the kernel
    CHECK(city0(w).stage_stamp == 2513);                            // gated: 2496 < 2500
    until(w, [&] { return city0(w).stage_stamp != 2513; });
    CHECK(w.clock().tick() == 5025);
    CHECK(city0(w).stage_stamp == 5025);
    CHECK(city0(w).stage_mask == 0x2); // stage 2 needs 50 residents
}

TEST_CASE("house live: the player-mask rule compares the low 23 bits (X16)") {
    Opt o;
    o.stages = true;
    const auto types = registry(o);
    struct Case {
        std::uint32_t player, city_mask, want;
    };
    // The pass sets stage 1 (5 residents) on top of the preset city mask 0x8 -> 0xa.
    for (const Case c : {Case{0x10, 0x8, 0x10}, // 0x10 >= 0xa: skipped (an OR would give 0x1a)
                         Case{0x04, 0x8, 0x0e}, // 4 < 0xa: performed (full dword OR)
                         Case{0xff800001U, 0x8, 0xff80000bU}, // high bits ignored by the compare
                         Case{0x0a, 0x8, 0x0a}}) {            // equal: skipped
        INFO("player " << c.player);
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {5000, 5000}));
        sim::World w = create(r, types);
        city0(w).stage_mask = c.city_mask;
        sim::Player* const p0 = sim::WorldTestAccess::players(w).find(sim::PlayerId{0});
        REQUIRE(p0 != nullptr);
        p0->flags = c.player;
        until(w, [&] { return city0(w).stage_stamp != 0; }, 3000);
        CHECK(city0(w).stage_mask == (c.city_mask | 0x2));
        CHECK(w.players().find(sim::PlayerId{0})->flags == c.want);
    }
}

TEST_CASE(
    "house live: a crafted house (E = 0, residents 0) fails the advance with the typed message") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 0, 0, {5, 5}));
    sim::World w = create(r, types);
    until(w, [&] { return w.faulted(); }, 3000);
    CHECK(w.clock().tick() == 2513);
    CHECK(w.fault() == "house fault not_evaluated building 1 (kernel: zero_divisor)");
    CHECK(w.check_invariants() ==
          "world fault: house fault not_evaluated building 1 (kernel: zero_divisor)");
    // Nothing of the house changed (no commit of a NotEvaluated tick) and no draw happened.
    CHECK(same_house(house_of(w), make_house(kP, 0, 0, {5, 5})));
    CHECK(w.last_output().draws.empty());
    // The world is stopped: a further advance changes nothing, not even the clock.
    const std::uint64_t hash = w.state_hash();
    const std::uint64_t tick = w.clock().tick();
    CHECK(w.advance(2) == tick);
    CHECK(w.state_hash() == hash);
    // The flag is state: it survives a save and load (the text does not).
    auto back = sim::World::load(w.save(), types);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    CHECK(back.value().faulted());
    CHECK(back.value().fault().empty());
    CHECK(back.value().check_invariants() == ""); // the text is gone; the world stays loadable
    CHECK(back.value().advance(2) == tick);
    CHECK(back.value().advance(2) == tick);
    CHECK(back.value().state_hash() == hash);
}

TEST_CASE("house live: world creation refuses what the live update cannot run (P-2)") {
    const Rig r = [] {
        Rig x = make_rig();
        x.place(kP, 110, 110, make_house(kP, 320, 640, {0, 0}));
        return x;
    }();
    sim::Rules live;
    live.house_updates = true;
    const auto create_with = [&](const std::shared_ptr<const sim::TypeRegistry>& t,
                                 sim::Rules rules) {
        return sim::World::create(r.spec, t, syn::population_config(), rules);
    };
    SUBCASE("a gate period outside 1..65535 names the class") {
        std::vector<sim::ClassInfo> classes = sim::original_class_table();
        for (sim::ClassInfo& c : classes) {
            if (c.cls == sim::RuntimeClass::House) {
                c.gate_period = 70000;
            }
        }
        const auto t = registry({}, classes);
        const auto refused = create_with(t, live);
        REQUIRE_FALSE(refused.ok());
        CHECK(refused.error().find("House class gate period 70000") != std::string::npos);
        CHECK(create_with(t, sim::Rules{}).ok()); // off: accepted as before
    }
    SUBCASE("a group without a need names the group") {
        const auto t = registry({}, {}, false, true);
        const auto refused = create_with(t, live);
        REQUIRE_FALSE(refused.ok());
        CHECK(refused.error().find("house group 1 has no need") != std::string::npos);
        CHECK(create_with(t, sim::Rules{}).ok());
    }
    SUBCASE("group 0 without NoDowngrade names the group") {
        const auto t = registry({}, {}, true);
        const auto refused = create_with(t, live);
        REQUIRE_FALSE(refused.ok());
        CHECK(refused.error().find("house group 0 does not set NoDowngrade") != std::string::npos);
        CHECK(refused.error().find("PreviousCapacityUnavailable") != std::string::npos);
        CHECK(create_with(t, sim::Rules{}).ok());
    }
}

TEST_CASE("house live: the stamp of a created house, the cadence and the loaded last (P-6, X15)") {
    const auto types = registry();
    sim::PlacementRecord rec;
    rec.type = kP;
    rec.x = 120;
    rec.y = 120;
    SUBCASE("a created house stamps u16(tick) only with the rule on") {
        for (const bool live : {true, false}) {
            Rig r = make_rig();
            sim::World w = create(r, types, live);
            const auto made = sim::WorldTestAccess::create(w, 0, 0, rec, 70000);
            REQUIRE(made.ok());
            const auto* h = std::get_if<sim::House>(&at(w, 120, 120)->payload);
            REQUIRE(h != nullptr);
            CHECK(h->last == (live ? 4464 : 0)); // u16(70000) = 4464
        }
    }
    SUBCASE("scenario import keeps last 0 and own saves keep the saved last") {
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {5000, 5000}, 1234));
        r.place(kP3, 114, 114);
        sim::World w = create(r, types);
        CHECK(house_of(w).last == 1234);
        const auto* d = std::get_if<sim::House>(&at(w, 114, 114)->payload);
        REQUIRE(d != nullptr);
        CHECK(d->last == 0);
        auto back = sim::World::load(w.save(), types);
        REQUIRE(back.ok());
        CHECK(house_of(back.value()).last == 1234);
    }
    SUBCASE("a stamped house waits its period; an unstamped one takes at once (t = 70000)") {
        // Stamped (last 4464): the clock is set to t = 69999, so the first advance (t = 70000, d =
        // 70000) visits every ring slot and city 0 is visited again at t = 70001 + 16 k. The first
        // visit with elapsed >= 2500 (t >= 72500) is k = 157, t = 72513; the next one needs t >=
        // 75000: k = 313, t = 75009.
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {20000, 20000}, 4464));
        sim::World w = create(r, types);
        sim::WorldTestAccess::clock(w).ms = 2 * 70000 - 2;
        sim::WorldTestAccess::clock(w).last_tick = 69999;
        std::vector<std::uint64_t> takes;
        std::uint16_t last = house_of(w).last;
        while (w.clock().tick() < 75100) {
            w.advance(2);
            if (house_of(w).last != last) {
                takes.push_back(w.clock().tick());
                last = house_of(w).last;
            }
        }
        CHECK((takes == std::vector<std::uint64_t>{72513, 75009}));
        CHECK(house_of(w).last == 9464);
        CHECK(w.check_invariants() == "");
        // Unstamped (0): takes at the first visit.
        Rig r0 = make_rig();
        r0.place(kP, 110, 110, make_house(kP, 320, 640, {20000, 20000}, 0));
        sim::World w0 = create(r0, types);
        sim::WorldTestAccess::clock(w0).ms = 2 * 70000 - 2;
        w0.advance(2);
        CHECK(house_of(w0).last == 2500);
    }
    SUBCASE("overdue: t = 5000 with last 0 takes one step per call") {
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {20000, 20000}));
        sim::World w = create(r, types);
        sim::WorldTestAccess::clock(w).ms = 2 * 5000 - 2;
        w.advance(2); // t = 5000: the first call, every ring slot is due
        CHECK(house_of(w).last == 2500);
        w.advance(2); // t = 5001: city 0 again (the ring wrapped)
        CHECK(house_of(w).last == 5000);
        for (int i = 0; i < 40; ++i) {
            w.advance(2);
        }
        CHECK(house_of(w).last == 5000); // 5000 -> not due again
    }
    SUBCASE("minimal cadence: repeated calls at one tick take once") {
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {20000, 20000}));
        sim::World w = create(r, types);
        sim::WorldTestAccess::clock(w).ms = 2 * 2500;
        for (int i = 0; i < 40; ++i) {
            w.advance(0);
        }
        CHECK(house_of(w).last == 2500);
    }
    SUBCASE("a loaded nonzero last continues without a reset") {
        // last 1234, clock t = 2999 then one advance (t = 3000, every ring slot): city 0 is visited
        // again at t = 3001 + 16 k; the first take is the first visit with t - 1234 >= 2500, i.e.
        // t >= 3734: k = 46, t = 3737, last 3734; the second needs t >= 6234: k = 203, t = 6249.
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {20000, 20000}, 1234));
        sim::World w = create(r, types);
        sim::WorldTestAccess::clock(w).ms = 2 * 3000 - 2;
        w.advance(2);
        until(w, [&] { return house_of(w).last != 1234; }, 2000);
        CHECK(w.clock().tick() == 3737);
        CHECK(house_of(w).last == 3734);
        auto back = sim::World::load(w.save(), types);
        REQUIRE(back.ok());
        sim::World w2 = std::move(back).value();
        until(w, [&] { return house_of(w).last != 3734; }, 4000);
        until(w2, [&] { return house_of(w2).last != 3734; }, 4000);
        CHECK(w.clock().tick() == 6249);
        CHECK(w2.clock().tick() == 6249);
        CHECK(house_of(w).last == 6234); // 3734 + 2500
        CHECK(w.state_hash() == w2.state_hash());
    }
}

TEST_CASE("house live: with the rule off the same worlds do not tick houses at all") {
    Opt o;
    o.idsec = true;
    o.low_negative = true;
    const auto types = registry(o);
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 320, 640, {0, 0}));
    r.place(kStall, 114, 110);
    sim::World w = create(r, types, false);
    city0(w).stock.stock[kFood] = 100;
    const sim::House initial = house_of(w);
    for (int i = 0; i < 6000; ++i) {
        step(w);
    }
    CHECK(same_house(house_of(w), initial));
    CHECK(w.rng().cursor() == 13);
    CHECK(figure_count(w) == 0);
    CHECK(city0(w).stage_mask == 0);
    CHECK(city0(w).stage_stamp == 0);
    CHECK(w.players().find(sim::PlayerId{0})->flags == 1);
    CHECK(city0(w).stock.stock[kFood] == 100);
    CHECK_FALSE(w.faulted());
}

TEST_CASE("house live: save and reload at every cut of the chain, and a replay") {
    Opt o;
    o.idsec = true;
    o.low_negative = true;
    const auto types = registry(o);
    constexpr std::uint64_t kHorizon = 5100;
    sim::World ref = chain_world(types);
    while (ref.clock().tick() < kHorizon) {
        ref.advance(2);
    }
    REQUIRE(ref.check_invariants() == "");
    const std::uint64_t want = ref.state_hash();
    CHECK(house_of(ref).last == 5000);

    // Cuts: before the first gate, right after it (the walker is Start/ToStall), while it waits and
    // while it walks home with the cargo, after its delivery, and after the second gate.
    struct Cut {
        const char* name;
        std::function<bool(const sim::World&)> at;
    };
    const auto step_is = [](const sim::World& w, sim::ResidentStep s) {
        for (const sim::Figure* f : walkers(w)) {
            if (f->resident && f->resident->step == s && !f->expiry_stamp.has_value()) {
                return true;
            }
        }
        return false;
    };
    const std::vector<Cut> cuts = {
        {"before the gate", [](const sim::World& w) { return w.clock().tick() == 2000; }},
        {"gate taken", [](const sim::World& w) { return house_of(w).last == 2500; }},
        {"to stall", [&](const sim::World& w) { return step_is(w, sim::ResidentStep::ToStall); }},
        {"wait", [&](const sim::World& w) { return step_is(w, sim::ResidentStep::Wait); }},
        {"to home with cargo",
         [&](const sim::World& w) { return step_is(w, sim::ResidentStep::ToHome); }},
        {"delivered", [](const sim::World& w) { return house_of(w).stock[0] == 1600; }},
        {"second gate", [](const sim::World& w) { return house_of(w).last == 5000; }},
    };
    for (const Cut& c : cuts) {
        INFO(c.name);
        sim::World w = chain_world(types);
        while (!c.at(w) && w.clock().tick() < kHorizon) {
            w.advance(2);
        }
        REQUIRE(w.clock().tick() < kHorizon);
        REQUIRE(w.check_invariants() == "");
        const std::vector<std::uint8_t> bytes = w.save();
        auto back = sim::World::load(bytes, types);
        REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
        sim::World w2 = std::move(back).value();
        CHECK(w2.state_hash() == w.state_hash());
        CHECK(w2.save() == bytes);
        while (w2.clock().tick() < kHorizon) {
            w2.advance(2);
        }
        CHECK(w2.check_invariants() == "");
        CHECK(w2.state_hash() == want);
    }

    // Through a transition: cut right after the upgrade tick.
    {
        const auto t2 = registry();
        const auto upgrade_world = [&] {
            Rig r = make_rig();
            r.place(kP, 110, 110, make_house(kP, 512, 640, {2000, 500}));
            sim::World w = create(r, t2);
            city0(w).stock.stock[kTools] = 1000;
            city0(w).stock.stock[kWood] = 2000;
            return w;
        };
        sim::World a = upgrade_world();
        while (a.clock().tick() < 4000) {
            a.advance(2);
        }
        sim::World b = upgrade_world();
        while (b.clock().tick() < 2513) {
            b.advance(2);
        }
        REQUIRE(at(b, 110, 110)->type == kS1);
        auto back = sim::World::load(b.save(), t2);
        REQUIRE(back.ok());
        sim::World c = std::move(back).value();
        while (c.clock().tick() < 4000) {
            c.advance(2);
        }
        CHECK(c.check_invariants() == "");
        CHECK(c.state_hash() == a.state_hash());
    }

    // The recorded session replays to the same checkpoints.
    {
        sim::World w = chain_world(types);
        auto rec = sim::Recorder::start(w);
        REQUIRE(rec.ok());
        rec.value().checkpoint();
        for (int i = 0; i < 3000; ++i) {
            rec.value().advance(2);
            if (i % 500 == 0) {
                rec.value().checkpoint();
            }
        }
        rec.value().checkpoint();
        sim::World fresh = chain_world(types);
        auto outcome = sim::replay(fresh, rec.value().log());
        REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
        CHECK(outcome.value().reproduced());
        CHECK(outcome.value().checkpoints_matched == outcome.value().checkpoints_total);
        CHECK(outcome.value().checkpoints_total >= 7);
    }
}

TEST_CASE("house live: SetProperty 0x21 also sets bit 26 for stages 25, 27 and 28 (D 0x10020d51)") {
    // The stored value: bits 25, 27 and 28 imply bit 26; 24 and 26 alone are kept; the low bits
    // pass through. Hand-derived from the disassembly: the +0x97 bit 5 restore (stage 25) and the
    // two "or eax, 0x4000000" at 0x10020ea8 / 0x10020eb7 (stages 27 and 28).
    CHECK(sim::player_flags_set(0x1) == 0x1);
    CHECK(sim::player_flags_set(0x1000001) == 0x1000001);
    CHECK(sim::player_flags_set(0x2000000) == 0x6000000);
    CHECK(sim::player_flags_set(0x4000000) == 0x4000000);
    CHECK(sim::player_flags_set(0x8000000) == 0xc000000);
    CHECK(sim::player_flags_set(0x10000000) == 0x14000000);
    CHECK(sim::player_flags_set(0x1b000001) == 0x1f000001);
    CHECK(sim::player_flags_set(sim::player_flags_set(0x8000000)) == 0xc000000); // idempotent
    // Through the stage pass: house of 5 residents, one stage record (stage s = group 0, count 5),
    // player flags 1 -> the unlock sets city bit s, the player value is the rule's result.
    struct Case {
        std::uint32_t stage, want;
    };
    for (const Case c : {Case{24, 0x1000001}, Case{25, 0x6000001}, Case{26, 0x4000001},
                         Case{27, 0xc000001}, Case{28, 0x14000001}}) {
        INFO("stage " << c.stage);
        Opt o;
        o.high_stage = c.stage;
        const auto types = registry(o);
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {5000, 5000}));
        sim::World w = create(r, types);
        until(w, [&] { return city0(w).stage_stamp != 0; }, 3000);
        CHECK(city0(w).stage_mask == (std::uint32_t{1} << c.stage));
        CHECK(w.players().find(sim::PlayerId{0})->flags == c.want);
    }
}

TEST_CASE("house live: typed faults of the House tick and the P-2 type refusal") {
    Opt o;
    o.orphan = true;
    const auto types_orphan = registry(o);
    const auto types = registry();
    SUBCASE("NoSerialContext: the live tick without a serial context fails loudly") {
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {5000, 5000}));
        sim::World w = create(r, types);
        slice_test::Tick t(w, 5000);
        sim::Effects fx;
        sim::Building& b = const_cast<sim::Building&>(*at(w, 110, 110));
        const std::uint64_t before = w.state_hash();
        sim::house_tick(t.ctx, *sim::WorldTestAccess::islands(w)[0], city0(w), b,
                        *types->find(b.type), fx);
        REQUIRE(fx.faults.size() == 1);
        CHECK(fx.faults[0].house == sim::HouseFault::NoSerialContext);
        CHECK(fx.faults[0].building == b.id);
        CHECK(w.state_hash() == before);
        CHECK(sim::fault_message(fx.faults[0]) == "house fault no_serial_context building 1");
        // The stage pass without a serial context after a stage was reached: the same fault.
    }
    SUBCASE("NoSuchHouse and MissingGroup from a direct call, CountMismatch through the world") {
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {5000, 5000}));
        sim::World w = create(r, types);
        slice_test::Tick t(w, 5000);
        sim::SerialResources serial{&sim::WorldTestAccess::players(w),
                                    &sim::WorldTestAccess::figures(w)};
        t.ctx.serial = &serial;
        sim::Building& b = const_cast<sim::Building&>(*at(w, 110, 110));
        sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
        {
            // The island's id index lost the house: the commit finds no key.
            sim::World copy = w;
            slice_test::Tick tc(copy, 5000);
            sim::SerialResources sc{&sim::WorldTestAccess::players(copy),
                                    &sim::WorldTestAccess::figures(copy)};
            tc.ctx.serial = &sc;
            sim::Island& ic = *sim::WorldTestAccess::islands(copy)[0];
            sim::Building& bc = const_cast<sim::Building&>(*at(copy, 110, 110));
            ic.cache.by_id.erase(bc.id);
            sim::Effects fx;
            sim::house_tick(tc.ctx, ic, city0(copy), bc, *types->find(bc.type), fx);
            REQUIRE(fx.faults.size() == 1);
            CHECK(fx.faults[0].house == sim::HouseFault::NoSuchHouse);
        }
        {
            // A House type whose group is not in the table (world made with the rule off).
            Rig ro = make_rig();
            ro.place(kOrphan, 110, 110, [] {
                sim::House h = make_house(kP, 320, 640, {});
                return h;
            }());
            sim::World wo = create(ro, types_orphan, false);
            slice_test::Tick to(wo, 5000);
            sim::SerialResources so{&sim::WorldTestAccess::players(wo),
                                    &sim::WorldTestAccess::figures(wo)};
            to.ctx.serial = &so;
            sim::Building& bo = const_cast<sim::Building&>(*at(wo, 110, 110));
            sim::Effects fx;
            sim::house_tick(to.ctx, *sim::WorldTestAccess::islands(wo)[0], city0(wo), bo,
                            *types_orphan->find(bo.type), fx);
            REQUIRE(fx.faults.size() == 1);
            CHECK(fx.faults[0].house == sim::HouseFault::MissingGroup);
        }
        static_cast<void>(b);
        static_cast<void>(is);
    }
    SUBCASE("CountMismatch: a broken derived house count stops the world before anything changes") {
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 512, 640, {2000, 500}));
        sim::World w = create(r, types);
        city0(w).stock.stock[kTools] = 1000;
        city0(w).stock.stock[kWood] = 2000;
        city0(w).index.house_counts[0] = 0; // the upgrade's old group count
        for (int i = 0; i < 3000 && !w.faulted(); ++i) {
            w.advance(2); // (no per-step invariants: the derived index is broken on purpose)
        }
        REQUIRE(w.faulted());
        CHECK(w.fault() == "house fault count_mismatch building 1");
        CHECK(at(w, 110, 110)->type == kP); // not upgraded
        CHECK(city0(w).stock.stock[kTools] == 1000);
    }
    SUBCASE("a House type that names no group is refused at creation with the rule on (P-2)") {
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {0, 0}));
        sim::Rules live;
        live.house_updates = true;
        const auto refused =
            sim::World::create(r.spec, types_orphan, syn::population_config(), live);
        REQUIRE_FALSE(refused.ok());
        CHECK(refused.error().find("house type 9540 (PRODTYPE 99) names no house group") !=
              std::string::npos);
        CHECK(
            sim::World::create(r.spec, types_orphan, syn::population_config(), sim::Rules{}).ok());
    }
}

TEST_CASE("house live: a house built by a command stamps last with the world tick (P-6)") {
    const auto types = registry();
    Rig r = make_rig();
    syn::give_flat_terrain(r.spec); // a legal site (ship-placement.md R4)
    sim::World w = create(r, types);
    sim::WorldTestAccess::clock(w).ms = 2 * 70000 - 2;
    w.advance(2); // tick 70000
    REQUIRE(w.clock().tick() == 70000);
    const sim::ApplyResult res = w.apply(
        sim::Command{sim::PlayerId{0}, sim::BuildCmd{sim::CityRef{sim::IslandId{0}, sim::CityNo{0}},
                                                     {sim::PlacementWire{kP, 120, 120, 0, 0, 0}}}});
    REQUIRE_MESSAGE(res.ok(), std::string(sim::reject_name(res.status)));
    const auto* h = std::get_if<sim::House>(&at(w, 120, 120)->payload);
    REQUIRE(h != nullptr);
    CHECK(h->last == 4464); // u16(70000), not 0
}

TEST_CASE("house live: a next type that is no house of the next group is not eligible (P-14)") {
    // The real PIONIER houses (3851-3854, 3960, 3972) have a non-house at id - id % 20 + 20. A
    // house at its cap would fault with MissingGroup; under P-14 the check is "not eligible": the
    // house keeps its type, takes its gate, nothing is spent, no variant is drawn and the world
    // runs on.
    Opt o;
    o.stall_next = true;
    o.declare_special = true; // C3: an irregular successor must be declared
    const auto types = registry(o);
    Rig r = make_rig();
    r.place(kPioneer, 110, 110, make_house(kP, 512, 640, {2000, 500}));
    sim::World w = create(r, types);
    city0(w).stock.stock[kTools] = 1000;
    city0(w).stock.stock[kWood] = 2000;
    until(w, [&] { return house_of(w).last != 0; }, 3000);
    CHECK_FALSE(w.faulted());
    CHECK(at(w, 110, 110)->type == kPioneer);
    CHECK(house_of(w).residents_64 == 512);
    CHECK(city0(w).stock.stock[kTools] == 1000);
    CHECK(draws_at(w, sim::DrawSite::HouseVariant) == 0);
    for (int i = 0; i < 2600; ++i) { // a second gate
        w.advance(2);
    }
    CHECK_FALSE(w.faulted());
    CHECK(w.check_invariants() == "");
}

// ------------------------------------------------------------------ MAP-POP-CLOSE-1 C1

namespace {

/// The review's F1 world: kP (110, 110) residents 320, E 640, stocks {0, 0}, a stall at (114, 110),
/// city food 5000, the billed counter of food preset.
sim::World f1_world(const std::shared_ptr<const sim::TypeRegistry>& types, std::uint32_t billed) {
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 320, 640, {0, 0}));
    r.place(kStall, 114, 110);
    sim::Rules rules;
    rules.house_updates = true;
    auto w = sim::World::create(r.spec, types, syn::population_config(), rules);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    sim::World world = std::move(w).value();
    city0(world).stock.stock[kFood] = 5000;
    city0(world).billed[kFood] = billed;
    return world;
}

/// Advances until the first taken gate has passed (t > 2600) or a fault.
void f1_run(sim::World& w) {
    for (int i = 0; i < 4000 && !w.faulted(); ++i) {
        w.advance(2);
    }
}

void reseal_save(std::vector<std::uint8_t>& save) {
    REQUIRE(save.size() > 8);
    sim::StateHasher h;
    h.bytes(std::span<const std::uint8_t>(save.data(), save.size() - 8));
    const std::uint64_t v = h.value();
    for (std::size_t i = 0; i < 8; ++i) {
        save[save.size() - 8 + i] = static_cast<std::uint8_t>(v >> (8 * i));
    }
}

std::vector<std::uint8_t> rules_bytes(const sim::Rules& rules) {
    sim::WriteArchive wa;
    fields(wa, rules);
    return wa.take();
}

} // namespace

TEST_CASE("house live F1: house updates with unsaved billed goods are refused at create and load") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 320, 640, {0, 0}));
    // The refused combination, at create: the message names both rules.
    sim::Rules bad;
    bad.house_updates = true;
    bad.save_billed_goods = false;
    auto refused = sim::World::create(r.spec, types, syn::population_config(), bad);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().find("save_billed_goods") != std::string::npos);
    CHECK(refused.error().find("house updates") != std::string::npos);
    // Legal controls: each rule alone, and both defaults.
    sim::Rules only_unsaved;
    only_unsaved.save_billed_goods = false;
    CHECK(sim::World::create(r.spec, types, syn::population_config(), only_unsaved).ok());
    sim::Rules both_on;
    both_on.house_updates = true;
    CHECK(sim::World::create(r.spec, types, syn::population_config(), both_on).ok());
    // At load: a valid, correctly checksummed save of (house off, billed unsaved) is patched to
    // house on. The checksum is valid, so the rules check is what refuses it.
    auto base = sim::World::create(r.spec, types, syn::population_config(), only_unsaved);
    REQUIRE(base.ok());
    std::vector<std::uint8_t> save = base.value().save();
    REQUIRE(sim::World::load(save, types).ok()); // the unpatched control loads
    const std::vector<std::uint8_t> rb = rules_bytes(only_unsaved);
    const auto at = std::search(save.begin(), save.end(), rb.begin(), rb.end());
    REQUIRE(at != save.end());
    // Independent stream133 tail: house_updates, save_billed_goods, city_booking.
    REQUIRE(rb.size() >= 3);
    REQUIRE(rb[rb.size() - 3] == 0);
    REQUIRE(rb[rb.size() - 2] == 0);
    REQUIRE(rb.back() == 0);
    *(at + static_cast<std::ptrdiff_t>(rb.size()) - 3) = 1;
    reseal_save(save);
    auto loaded = sim::World::load(save, types);
    REQUIRE_FALSE(loaded.ok());
    CHECK(loaded.error().find("save_billed_goods") != std::string::npos);
    CHECK(loaded.error().find("house updates") != std::string::npos);
    // The cause control: the same patch with a stale checksum is a checksum error instead.
    std::vector<std::uint8_t> stale = save;
    stale[stale.size() - 1] ^= 1;
    auto stale_r = sim::World::load(stale, types);
    REQUIRE_FALSE(stale_r.ok());
    CHECK(stale_r.error().find("save_billed_goods") == std::string::npos);
}

TEST_CASE(
    "house live F1: saved billed counters agree after reload below the bound and at overflow") {
    const auto types = registry();
    // Below the bound: billed food at max - 1000, the purchase books at most 151 (hand: demand
    // trunc(41*640*256/44236) = 151, stall store 5000, threshold 10): the purchase succeeds, no
    // fault.
    {
        sim::World live = f1_world(types, 0xFFFFFFFFU - 1000U);
        auto back = sim::World::load(live.save(), types);
        REQUIRE(back.ok());
        sim::World reloaded = std::move(back).value();
        CHECK(reloaded.state_hash() == live.state_hash());
        f1_run(live);
        f1_run(reloaded);
        CHECK_FALSE(live.faulted());
        CHECK_FALSE(reloaded.faulted());
        CHECK(city0(live).billed[kFood] == 0xFFFFFFFFU - 1000U + 151U);
        CHECK(city0(live).stock.stock[kFood] == 5000U - 151U);
        CHECK(live.state_hash() == reloaded.state_hash());
    }
    // At overflow: saved counters fault identically before and after the reload.
    {
        sim::World live = f1_world(types, 0xFFFFFFFFU);
        auto back = sim::World::load(live.save(), types);
        REQUIRE(back.ok());
        sim::World reloaded = std::move(back).value();
        CHECK(city0(reloaded).billed[kFood] == 0xFFFFFFFFU); // saved, not reset
        f1_run(live);
        f1_run(reloaded);
        CHECK(live.faulted());
        CHECK(reloaded.faulted());
        CHECK(live.fault() == reloaded.fault());
        CHECK(live.state_hash() == reloaded.state_hash());
        CHECK(live.fault().find("billed_overflow") != std::string::npos);
    }
}

TEST_CASE("goods G2: a cyclic IdSec is a registry error and a legal chain is not") {
    const auto build = [&](sim::GoodsId a, sim::GoodsId b, bool needs_both) {
        std::vector<sim::GoodsInfo> goods = sim::original_goods();
        goods[26].id_sec = a;
        if (b != 0) {
            goods[27].id_sec = b;
        }
        sim::HouseGroup g0;
        g0.number = 0;
        g0.wohnmax = 8;
        g0.no_downgrade = true;
        g0.needs = {{26, true, 41}};
        if (needs_both) {
            g0.needs.push_back({27, true, 16});
        }
        sim::HouseGroup g1 = g0;
        g1.number = 1;
        g1.no_downgrade = false;
        sim::TypeRegistry::Builder bld;
        bld.config(syn::population_config());
        bld.walk_tables(sim::synthetic_walk_tables());
        bld.goods(goods);
        bld.house_groups({g0, g1});
        bld.add(house_t(kP, 97, 2, 0));
        return bld.build();
    };
    // 26 -> 27 -> 26 with both needed: a cycle; 26 -> 26: a self-loop; both refused with the goods.
    auto cyc = build(27, 26, true);
    REQUIRE_FALSE(cyc.ok());
    CHECK(cyc.error().find("IdSec cycle") != std::string::npos);
    auto self = build(26, 0, true);
    REQUIRE_FALSE(self.ok());
    CHECK(self.error().find("IdSec cycle") != std::string::npos);
    // Legal controls: the chain 26 -> 27 -> 28 (28 not needed ends it), a chain 26 -> 27, and the
    // cycle 26 <-> 27 when 27 is not a need (the walk ends at the slotless substitute).
    CHECK(build(27, 28, true).ok());
    CHECK(build(27, 0, true).ok());
    CHECK(build(27, 26, false).ok());
}

TEST_CASE("goods G2: a shared need slot through IdSec substitution is legal and a cycle through it "
          "is not") {
    // KAPELLE (82) is the only need; goods 83 (KIRCHE) has no slot of its own, so the derived
    // lookup gives it KAPELLE's slot 0 (the one-pass IdSec substitution, AnnoGame 0x10014b78). Two
    // goods then share one slot: legal. 82 <-> 83 closes a cycle through the shared slot: refused.
    const auto build = [&](sim::GoodsId sec82, sim::GoodsId sec83) {
        std::vector<sim::GoodsInfo> goods = sim::original_goods();
        goods[82].id_sec = sec82;
        goods[83].id_sec = sec83;
        sim::HouseGroup g0;
        g0.number = 0;
        g0.wohnmax = 8;
        g0.no_downgrade = true;
        g0.needs = {{26, true, 41}, {82, true, 41}};
        sim::HouseGroup g1 = g0;
        g1.number = 1;
        g1.no_downgrade = false;
        sim::TypeRegistry::Builder bld;
        bld.config(syn::population_config());
        bld.walk_tables(sim::synthetic_walk_tables());
        bld.goods(goods);
        bld.house_groups({g0, g1});
        bld.add(house_t(kP, 97, 2, 0));
        return bld.build();
    };
    auto legal = build(0, 82); // 83 -> 82: two goods, one slot
    REQUIRE_MESSAGE(legal.ok(), (legal.ok() ? std::string() : legal.error()));
    CHECK(legal.value()->derived_groups()[0].lookup[83] == 1);
    CHECK(legal.value()->derived_groups()[0].lookup[82] == 1);
    auto cyc = build(83, 82); // 82 -> 83 -> 82 through the shared slot
    REQUIRE_FALSE(cyc.ok());
    CHECK(cyc.error().find("IdSec cycle") != std::string::npos);
}

// ------------------------------------------------------------------ MAP-POP-CLOSE-1 C2 (G1)

namespace {

constexpr std::uint16_t kEdgeOk = 6144, kEdgeBad = 6145;

sim::ApplyResult build_cmd(sim::World& w, std::vector<sim::PlacementWire> placements) {
    return w.apply(
        sim::Command{sim::PlayerId{0}, sim::BuildCmd{sim::CityRef{sim::IslandId{0}, sim::CityNo{0}},
                                                     std::move(placements)}});
}

sim::World build_world(const std::shared_ptr<const sim::TypeRegistry>& types, bool live,
                       bool atomic = false) {
    Rig r = make_rig();
    syn::give_flat_terrain(r.spec);
    sim::Rules rules;
    rules.house_updates = live;
    rules.build_atomic = atomic;
    auto w = sim::World::create(r.spec, types, syn::population_config(), rules);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    sim::World world = std::move(w).value();
    city0(world).stock.stock[kTools] = 5000;
    city0(world).stock.stock[kWood] = 5000;
    return world;
}

const sim::House& house_at(const sim::World& w, std::int32_t x, std::int32_t y) {
    const sim::Building* b = at(w, x, y);
    REQUIRE(b != nullptr);
    const auto* h = std::get_if<sim::House>(&b->payload);
    REQUIRE(h != nullptr);
    return *h;
}

} // namespace

TEST_CASE("house create G1: BuildCmd runs first assignment for a group 1 house") {
    // PRNG table T[i] = 7 i + 1, cursor 13: the draws are T[14] = 99 and T[15] = 106. Group 1
    // (Wohnmax 15, forced food Menge 60, forced goods 27 Menge 40): residents_64 = 15 << 6 = 960,
    // E = 15 << 7 = 1920; sat = 128 - (99 & 31) = 125 and 128 - (106 & 31) = 118; stock =
    // trunc(60*125*1920/1536) = 9375 and trunc(40*118*1920/1536) = 5900. Payment: tools 512 and
    // wood 1024 once; the draws follow the creation (original: Place charge, then Create).
    const auto types = registry();
    sim::World w = build_world(types, true);
    const std::size_t cur = w.rng().cursor();
    REQUIRE(cur == 13);
    sim::ApplyResult res = build_cmd(w, {sim::PlacementWire{kS0, 120, 120, 0, 0, 0}});
    REQUIRE_MESSAGE(res.ok(), std::string(sim::reject_name(res.status)));
    const sim::House& h = house_at(w, 120, 120);
    CHECK(h.residents_64 == 960);
    CHECK(h.e == 1920);
    CHECK((h.stock == std::vector<std::uint32_t>{9375, 5900}));
    CHECK(w.rng().cursor() == cur + 2);
    CHECK(city0(w).stock.stock[kTools] == 5000 - 512);
    CHECK(city0(w).stock.stock[kWood] == 5000 - 1024);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("house create G1: group 0 keeps the constructor numbers and gets its food stock") {
    // Group 0 skips the draw block and the E / residents stores (320 / 640 are the constructor's);
    // slot 0 (food) gets trunc(41 * 640 / 12) = 2186, the unforced others 0. No draw.
    const auto types = registry();
    sim::World w = build_world(types, true);
    const std::size_t cur = w.rng().cursor();
    REQUIRE(build_cmd(w, {sim::PlacementWire{kP, 120, 120, 0, 0, 0}}).ok());
    const sim::House& h = house_at(w, 120, 120);
    CHECK(h.residents_64 == 320);
    CHECK(h.e == 640);
    CHECK((h.stock == std::vector<std::uint32_t>{2186, 0}));
    CHECK(w.rng().cursor() == cur);
}

TEST_CASE("house create G1: rule off and imported payloads are unchanged") {
    const auto types = registry();
    // Rule off: the baseline numbers, no draw.
    {
        sim::World w = build_world(types, false);
        REQUIRE(build_cmd(w, {sim::PlacementWire{kS0, 120, 120, 0, 0, 0}}).ok());
        const sim::House& h = house_at(w, 120, 120);
        CHECK(h.residents_64 == 320);
        CHECK(h.e == 640);
        CHECK((h.stock == std::vector<std::uint32_t>{0, 0}));
        CHECK(w.rng().cursor() == 13);
    }
    // Rule on, scenario houses: an explicit payload and a payload-less record are both imported.
    {
        Rig r = make_rig();
        r.place(kS0, 110, 110, make_house(kS0, 300, 512, {1000, 800}));
        r.place(kS0, 120, 110); // no payload: the constructor default, not a built house
        sim::World w = create(r, types);
        CHECK(house_at(w, 110, 110).residents_64 == 300);
        CHECK(house_at(w, 110, 110).e == 512);
        CHECK((house_at(w, 110, 110).stock == std::vector<std::uint32_t>{1000, 800}));
        CHECK(house_at(w, 120, 110).residents_64 == 320);
        CHECK(house_at(w, 120, 110).e == 640);
        CHECK(w.rng().cursor() == 13);
        // An own save keeps the numbers.
        auto back = sim::World::load(w.save(), types);
        REQUIRE(back.ok());
        CHECK(back.value().state_hash() == w.state_hash());
    }
}

TEST_CASE(
    "house create G1: a refused initialization draws nothing charges nothing and takes no id") {
    Opt bad;
    bad.g1_menge_food = kEdgeBad;
    Opt ok;
    ok.g1_menge_food = kEdgeOk;
    // The overflowing mod input (edge Menge 6145 with Wohnmax 65535): refused, state untouched.
    {
        const auto types = registry(bad);
        sim::World w = build_world(types, true);
        const std::uint64_t hash = w.state_hash();
        const std::uint32_t serial = w.islands()[0]->next_serial;
        const sim::ApplyResult res = build_cmd(w, {sim::PlacementWire{kS0, 120, 120, 0, 0, 0}});
        CHECK_FALSE(res.ok());
        CHECK(res.status == sim::Reject::HouseDomain);
        CHECK(w.rng().cursor() == 13);
        CHECK(w.state_hash() == hash);
        CHECK(w.islands()[0]->next_serial == serial);
        CHECK(city0(w).stock.stock[kTools] == 5000);
        CHECK(at(w, 120, 120) == nullptr);
        // A group 0 house of the same registry is still accepted.
        CHECK(build_cmd(w, {sim::PlacementWire{kP, 120, 120, 0, 0, 0}}).ok());
    }
    // The neighbouring input (Menge 6144): accepted, two draws, charged once.
    {
        const auto types = registry(ok);
        sim::World w = build_world(types, true);
        REQUIRE(build_cmd(w, {sim::PlacementWire{kS0, 120, 120, 0, 0, 0}}).ok());
        CHECK(w.rng().cursor() == 15);
        const sim::House& h = house_at(w, 120, 120);
        CHECK(h.residents_64 == 65535 * 64);
        CHECK(h.e == 65535U * 128U);
        CHECK(city0(w).stock.stock[kTools] == 5000 - 512);
    }
}

TEST_CASE("house create G1: batches and the atomic dry run draw and charge once") {
    Opt bad;
    bad.g1_menge_food = kEdgeBad;
    const auto good_types = registry();
    const auto bad_types = registry(bad);
    // Atomic success: two group 1 houses draw 2 + 2 in record order, charged once each.
    {
        sim::World w = build_world(good_types, true, true);
        REQUIRE(build_cmd(w, {sim::PlacementWire{kS0, 120, 120, 0, 0, 0},
                              sim::PlacementWire{kS0, 126, 120, 0, 0, 0}})
                    .ok());
        CHECK(w.rng().cursor() == 1); // 13 + 4 draws on a 16-entry table
        // second house: T[0] = 1 -> sat 128 - 1 = 127 and T[1] = 8 -> 120 (cursor wraps after 15)
        // stock0 = trunc(60 * 127 * 1920 / 1536) = 9525, stock1 = trunc(40 * 120 * 1920 / 1536) =
        // 6000
        CHECK((house_at(w, 126, 120).stock == std::vector<std::uint32_t>{9525, 6000}));
        CHECK(city0(w).stock.stock[kTools] == 5000 - 2 * 512);
    }
    // Atomic abort: the refused second record aborts the first; nothing drawn, charged or taken.
    {
        sim::World w = build_world(bad_types, true, true);
        const std::uint64_t hash = w.state_hash();
        const sim::ApplyResult res = build_cmd(w, {sim::PlacementWire{kP, 120, 120, 0, 0, 0},
                                                   sim::PlacementWire{kS0, 126, 120, 0, 0, 0}});
        CHECK(res.status == sim::Reject::AtomicAbort);
        CHECK(res.records[0].status == sim::Reject::AtomicAbort);
        CHECK(res.records[1].status == sim::Reject::HouseDomain);
        CHECK(w.state_hash() == hash);
        CHECK(w.rng().cursor() == 13);
    }
    // Non-atomic: the legal record is built, the refused one is not and costs nothing.
    {
        sim::World w = build_world(bad_types, true, false);
        const sim::ApplyResult res = build_cmd(w, {sim::PlacementWire{kP, 120, 120, 0, 0, 0},
                                                   sim::PlacementWire{kS0, 126, 120, 0, 0, 0}});
        CHECK(res.ok());
        CHECK(res.records[0].status == sim::Reject::None);
        CHECK(res.records[1].status == sim::Reject::HouseDomain);
        CHECK(at(w, 126, 120) == nullptr);
        CHECK(w.rng().cursor() == 13);
        CHECK(city0(w).stock.stock[kTools] == 5000);
    }
}

TEST_CASE(
    "house create G1: the built house is stamped with the world tick and an own save keeps it") {
    // P-6 replacement (population-live.md 21.5): the engine stamps the clock tick at the command.
    const auto types = registry();
    sim::World w = build_world(types, true);
    sim::WorldTestAccess::clock(w).ms = 2 * 3000 - 2;
    w.advance(2); // tick 3000
    REQUIRE(w.clock().tick() == 3000);
    REQUIRE(build_cmd(w, {sim::PlacementWire{kS0, 120, 120, 0, 0, 0}}).ok());
    CHECK(house_at(w, 120, 120).last == 3000);
    auto back = sim::World::load(w.save(), types);
    REQUIRE(back.ok());
    CHECK(back.value().state_hash() == w.state_hash());
}

TEST_CASE("house create G1: one public BuildCmd leads to the first resident delivery") {
    // The causal chain a package reviewer traces: BuildCmd (group 1 house, payment, first
    // assignment: two draws) -> the first taken gate -> the spawn draw -> the walker buys at the
    // stall (city store debit, billed) -> the delivery raises the house stock -> the walker slot is
    // released.
    const auto types = registry();
    Rig r = make_rig();
    syn::give_flat_terrain(r.spec);
    r.place(kStall, 114, 110);
    sim::Rules rules;
    rules.house_updates = true;
    auto made = sim::World::create(r.spec, types, syn::population_config(), rules);
    REQUIRE(made.ok());
    sim::World w = std::move(made).value();
    city0(w).stock.stock[kFood] = 5000;
    city0(w).stock.stock[kTools] = 5000;
    city0(w).stock.stock[kWood] = 5000;
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    // 1. The build: payment once, E / residents / stocks from SetGroup, two draws (T[14], T[15]).
    REQUIRE(build_cmd(w, {sim::PlacementWire{kS0, 110, 110, 0, 0, 0}}).ok());
    CHECK(city0(w).stock.stock[kTools] == 5000 - 512);
    CHECK(city0(w).stock.stock[kWood] == 5000 - 1024);
    CHECK(w.rng().cursor() == 15);
    CHECK((house_at(w, 110, 110).stock == std::vector<std::uint32_t>{9375, 5900}));
    const sim::BuildingId home = at(w, 110, 110)->id;
    // 2. Run until the walker that the house sent has bought and delivered.
    std::size_t spawned = 0;
    for (int i = 0; i < 200000 && !w.faulted(); ++i) {
        w.advance(2);
        spawned = std::max(spawned, walkers(w).size());
        if (city0(w).billed[kFood] > 0 && walkers(w).empty()) {
            break;
        }
    }
    REQUIRE_FALSE(w.faulted());
    CHECK(spawned >= 1);
    // 3. The accounting closes from authoritative state: what the stall sold left the city store
    // and was billed (the requested amount equals the sold one here), nothing was manufactured.
    const std::uint32_t sold = 5000 - city0(w).stock.stock[kFood];
    CHECK(sold > 0);
    CHECK(city0(w).billed[kFood] == sold);
    CHECK(stats.residents.purchases >= 1);
    CHECK(stats.residents.deliveries >= 1);
    CHECK(stats.residents.lost_88 == 0);
    // 4. The walker slot was released exactly once and the world is consistent.
    const auto* h = std::get_if<sim::House>(&at(w, 110, 110)->payload);
    REQUIRE(h != nullptr);
    CHECK(at(w, 110, 110)->id == home);
    for (const sim::FigureId f : h->walkers) {
        CHECK_FALSE(f.valid());
    }
    CHECK(w.check_invariants() == "");
}

// ------------------------------------------------------------------ MAP-POP-CLOSE-1 C3 (F2, F3)

TEST_CASE("house live F2: a downgrade record carries the committed residents 300 to 284") {
    // Hand derivation (the same fixture as the downgrade case above): group 1 food table is -2
    // below index 10, mood -2, VermehrTab[0] = -16: 300 - 16 = 284, below the floor 48 * 8 = 384,
    // so the house downgrades and SetGroup keeps the residents. The record must say 284, the number
    // the resulting House holds, and the growth record must agree with the cap of group 1 (15 *
    // 64).
    Opt o;
    o.low_negative = true;
    const auto types = registry(o);
    Rig r = make_rig();
    r.place(kS0, 110, 110, make_house(kS0, 300, 512, {1000, 800}));
    sim::World w = create(r, types);
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    until(w, [&] { return !stats.population.transitions.empty(); }, 3000);
    REQUIRE(stats.population.transitions.size() == 1);
    const sim::HouseTransitionRecord& t = stats.population.transitions.front();
    CHECK(t.kind == sim::HouseTransitionRecord::Kind::Downgrade);
    CHECK(t.residents_before == 300);
    CHECK(t.residents_after == 300 - 16);
    CHECK(t.residents_after == house_of(w).residents_64); // the authoritative state
    REQUIRE(stats.population.growth.size() == 1);
    const sim::HouseGrowthRecord& g = stats.population.growth.front();
    CHECK(g.residents_before == 300);
    CHECK(g.vermehr == -16);
    CHECK(g.residents_after == 284);
    CHECK(g.residents_committed == house_of(w).residents_64);
    CHECK(g.cap_64 == 15 * 64);
    CHECK_FALSE(g.removed);
}

TEST_CASE("house live F2: a cap clip is recorded against its bound and a removal separately") {
    // Cap clip: group 0 (Wohnmax 8, cap 512), residents 500, mood 2 (+32): 532 > 512, no materials
    // in the store, so the upgrade is refused and the committed value is the cap 512 (not 532).
    {
        const auto types = registry();
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 500, 640, {2000, 500}));
        sim::World w = create(r, types);
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        until(w, [&] { return house_of(w).last != 0; }, 3000);
        REQUIRE(stats.population.growth.size() == 1);
        const sim::HouseGrowthRecord& g = stats.population.growth.front();
        CHECK(g.residents_before == 500);
        CHECK(g.vermehr == 32);
        CHECK(g.residents_before + g.vermehr > g.cap_64); // 532 > 512: the clip applies
        CHECK(g.cap_64 == 512);
        CHECK(g.residents_after == 512);
        CHECK(g.residents_committed == 512);
        CHECK(house_of(w).residents_64 == 512);
        REQUIRE(stats.population.transitions.size() == 1);
        CHECK(stats.population.transitions.front().kind ==
              sim::HouseTransitionRecord::Kind::UpgradeRefused);
        CHECK(stats.population.transitions.front().residents_after == 512);
    }
    // Removal (floor 64, NoDowngrade): the house is gone; the growth record says so and the
    // transition record counts it as removed with no residents left.
    {
        Opt o;
        o.low_negative = true;
        const auto types = registry(o);
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 70, 512, {0, 0}));
        sim::World w = create(r, types);
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        until(w, [&] { return at(w, 110, 110) == nullptr; }, 3000);
        REQUIRE(stats.population.growth.size() == 1);
        CHECK(stats.population.growth.front().removed);
        CHECK(stats.population.growth.front().residents_committed == 0);
        CHECK(stats.population.growth.front().residents_before == 70);
        CHECK(stats.population.growth.front().residents_after == 54);
        REQUIRE(stats.population.transitions.size() == 1);
        const sim::HouseTransitionRecord& t = stats.population.transitions.front();
        CHECK(t.kind == sim::HouseTransitionRecord::Kind::Remove);
        CHECK(t.residents_after == 0);
        CHECK(t.counter_old_after == t.counter_old_before - 1); // trunc(70/64) = 1 resident gone
    }
}

TEST_CASE("house live F3: stage-bit vectors through the engine agree with the reference file") {
    // The reference rows `stage hb-*` (house_live_vectors.txt): one stage record (stage s, group 0,
    // count 5), a house of 5 residents, the player's flags as given; the real stage pass must store
    // the file's value. A missing bit-26 implication fails here AND in house_live_ref.py.
    std::ifstream in(std::string(ANNO_FIXTURES_DIR) + "/sim/house_live_vectors.txt");
    REQUIRE(in.good());
    std::size_t rows = 0;
    for (std::string line; std::getline(in, line);) {
        if (line.rfind("stage hb-", 0) != 0) {
            continue;
        }
        std::istringstream ls(line);
        std::string kind, name, tok;
        ls >> kind >> name;
        std::string table, player, want_player;
        bool arrow = false;
        while (ls >> tok) {
            if (tok == "=>") {
                arrow = true;
            } else if (!arrow && tok.rfind("table=", 0) == 0) {
                table = tok.substr(6);
            } else if (!arrow && tok.rfind("player=", 0) == 0) {
                player = tok.substr(7);
            } else if (arrow && tok.rfind("player=", 0) == 0) {
                want_player = tok.substr(7);
            }
        }
        const std::uint32_t stage =
            static_cast<std::uint32_t>(std::stoul(table.substr(0, table.find(':'))));
        INFO(name);
        Opt o;
        o.high_stage = stage;
        const auto types = registry(o);
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {5000, 5000}));
        sim::World w = create(r, types);
        sim::Player* const p0 = sim::WorldTestAccess::players(w).find(sim::PlayerId{0});
        REQUIRE(p0 != nullptr);
        p0->flags = static_cast<std::uint32_t>(std::stoul(player, nullptr, 0));
        until(w, [&] { return city0(w).stage_stamp != 0; }, 3000);
        CHECK(w.players().find(sim::PlayerId{0})->flags ==
              static_cast<std::uint32_t>(std::stoul(want_player, nullptr, 0)));
        ++rows;
    }
    CHECK(rows == 7);
}

// ------------------------------------------------------------------ MAP-POP-CLOSE-1 C3 (P-14)

TEST_CASE("house live P-14: the successor of a House type is classified Ladder EndOfLadder Special "
          "or Malformed") {
    const sim::SimConfig cfg = syn::population_config();
    // Ladder: 9501 (group 0) -> 9520 (group 1, the same 2 x 2 footprint). EndOfLadder: 9520 -> 9540
    // does not exist. Special: 9560 -> 9580 (a stall) when declared. Malformed: the same
    // undeclared.
    Opt plain;
    const auto t1 = registry(plain);
    CHECK(sim::house_successor(cfg, *t1, *t1->find(kP)) == sim::HouseSuccessor::Ladder);
    CHECK(sim::house_successor(cfg, *t1, *t1->find(kS0)) == sim::HouseSuccessor::EndOfLadder);
    Opt undeclared;
    undeclared.stall_next = true;
    // an undeclared irregular successor is a content error at world creation (see below), but the
    // pure classifier answers Malformed
    {
        sim::TypeRegistry::Builder b;
        const auto types = registry(undeclared); // the registry itself builds
        CHECK(sim::house_successor(cfg, *types, *types->find(kPioneer)) ==
              sim::HouseSuccessor::Malformed);
    }
    Opt declared;
    declared.stall_next = true;
    declared.declare_special = true;
    const auto t3 = registry(declared);
    CHECK(sim::house_successor(cfg, *t3, *t3->find(kPioneer)) == sim::HouseSuccessor::Special);
    // the declaration is digested only when made
    CHECK(t3->digest() != registry(undeclared)->digest());
}

TEST_CASE(
    "house live P-14: an undeclared irregular successor is a content error at creation and load") {
    Opt o;
    o.stall_next = true; // 9560 -> 9580 (a market stall), not declared
    const auto types = registry(o);
    Rig r = make_rig();
    r.place(kPioneer, 110, 110, make_house(kP, 512, 640, {2000, 500}));
    sim::Rules rules;
    rules.house_updates = true;
    auto refused = sim::World::create(r.spec, types, syn::population_config(), rules);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().find("9560") != std::string::npos);
    CHECK(refused.error().find("9580") != std::string::npos);
    CHECK(refused.error().find("PRODTYPE 161") != std::string::npos);
    CHECK(refused.error().find("declared as a special successor") != std::string::npos);
    // With the rule off the registry is accepted (nothing evaluates the ladder).
    CHECK(sim::World::create(r.spec, types, syn::population_config(), sim::Rules{}).ok());
}

TEST_CASE("house live P-14: end of ladder and a declared special successor are named outcomes") {
    // End of ladder: kS0 (group 1, Wohnmax 15) at its cap with a satisfied table: no type 9540.
    // Special: kPioneer at its cap with the declared stall successor. Both keep their type, spend
    // nothing, draw no variant, do not fault, and are counted under their own name.
    {
        const auto types = registry();
        Rig r = make_rig();
        r.place(kS0, 110, 110, make_house(kS0, 960, 1920, {20000, 20000}));
        sim::World w = create(r, types);
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        city0(w).stock.stock[kTools] = 1000;
        city0(w).stock.stock[kWood] = 2000;
        until(w, [&] { return house_of(w).last != 0; }, 3000);
        CHECK_FALSE(w.faulted());
        CHECK(at(w, 110, 110)->type == kS0);
        CHECK(stats.population.total().upgrade_terminal >= 1);
        CHECK(stats.population.total().upgrade_special == 0);
        CHECK(city0(w).stock.stock[kTools] == 1000);
        CHECK(draws_at(w, sim::DrawSite::HouseVariant) == 0);
    }
    {
        Opt o;
        o.stall_next = true;
        o.declare_special = true;
        const auto types = registry(o);
        Rig r = make_rig();
        r.place(kPioneer, 110, 110, make_house(kP, 512, 640, {2000, 500}));
        sim::World w = create(r, types);
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        city0(w).stock.stock[kTools] = 1000;
        city0(w).stock.stock[kWood] = 2000;
        until(w, [&] { return house_of(w).last != 0; }, 3000);
        CHECK_FALSE(w.faulted());
        CHECK(at(w, 110, 110)->type == kPioneer);
        CHECK(stats.population.total().upgrade_special >= 1);
        CHECK(stats.population.total().upgrade_terminal == 0);
        CHECK(city0(w).stock.stock[kTools] == 1000);
        CHECK(draws_at(w, sim::DrawSite::HouseVariant) == 0);
    }
}

TEST_CASE("city booking: an earlier actual house fault suppresses due booking") {
    const auto types = registry();
    for (bool mismatch : {false, true}) {
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 512, 640, {2000, 500}));
        sim::Rules rules;
        rules.house_updates = true;
        rules.city_booking = true;
        auto made = sim::World::create(r.spec, types, syn::population_config(), rules);
        REQUIRE(made);
        auto w = std::move(made).value();
        city0(w).stock.stock[kTools] = 1000;
        city0(w).stock.stock[kWood] = 2000;
        city0(w).billed[kFood] = 12;
        if (mismatch)
            city0(w).index.house_counts[0] = 0; // labelled broken derived index
        const auto money = w.players().find(sim::PlayerId{0})->money;
        w.advance(6000); // first House gate and booking both due at tick3000
        if (mismatch) {
            REQUIRE(w.faulted());
            CHECK(w.fault().find("house fault count_mismatch") != std::string::npos);
            CHECK(city0(w).billed[kFood] == 12);
            CHECK(city0(w).accounts.stamp == 0);
            CHECK(w.players().find(sim::PlayerId{0})->money == money);
            CHECK(std::none_of(w.last_output().events.begin(), w.last_output().events.end(),
                               [](const auto& e) { return e.booking.has_value(); }));
        } else {
            REQUIRE_FALSE(w.faulted());
            CHECK(at(w, 110, 110)->type != kP);
            CHECK(city0(w).accounts.stamp == 3000);
            CHECK(city0(w).billed[kFood] == 0);
            CHECK(std::any_of(w.last_output().events.begin(), w.last_output().events.end(),
                              [](const auto& e) { return e.booking.has_value(); }));
        }
    }
}
