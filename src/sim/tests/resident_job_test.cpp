// MAP-POP-LIVE-1 L2: the resident fetch job 0x35 (docs/sim/population-live.md sections 1 step 3, 2,
// 3). The spawn block (resident_dispatch) against hand-derived draw traces, and the job end to end
// on a small synthetic island: spawn, search, walk, purchase, walk home, delivery, end, with the
// lifecycle controls (blocked route, no stock, competing buyers, removed buildings, save/reload).
// The House tick does not call the dispatch yet (L4): a test seam in the city pass does.
//
// Hand derivations (all numbers follow from the contract, not from the code):
//  * house type 97 at (110, 110), serial 1, E = 640, residents_64 = 320 (5 residents), one need
//    (goods 26, Menge 41, forced): demand = trunc(41*640*256/44236) = trunc(6717440/44236) = 151,
//    urgency with stock 0 = 128, threshold trunc(5*41*256/4096) = 12 (a supplier needs > 12).
//  * a delivery of 151 adds 151*16 = 2416 to the stock.
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/resident_job.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace {

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

constexpr sim::TypeId kHouse{9501}; // PRODTYPE 97, walker max 2, figure group 0 = {203, 204, 205}
constexpr sim::TypeId kHouseNoEnt{9502}; // the same without an entrance point
constexpr sim::TypeId kStall{9503};      // PRODTYPE 161, WARE[0] = goods 26
constexpr sim::TypeId kStallB{9504};     // PRODTYPE 161, WARE[0] = goods 27
constexpr sim::TypeId kChapel{9505};     // PRODTYPE 67 (PublicService), WARE[0] = goods 27
constexpr sim::TypeId kWall{9506};       // a tile object closed on every layer
constexpr sim::TypeId kInn{9507};        // PRODTYPE 66 (public block), class Inn, WARE[0] = goods 26, input 27

constexpr sim::GoodsId kFood = 26;
constexpr sim::GoodsId kOther = 27;

sim::BuildingType tile_object_wall() {
    sim::BuildingType t;
    t.id = kWall;
    t.prodtype = 225;
    t.cls = sim::RuntimeClass::Plain;
    t.head_raw.assign(56, 0);
    t.head_raw[0x2f] = 3; // closed on every layer of the synthetic walk tables
    return t;
}

sim::BuildingType house_type(sim::TypeId id, bool entrance) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = 97;
    t.cls = sim::RuntimeClass::House;
    t.action_radius = 8;
    if (entrance) {
        t.entrances = {{3, 3}};
    }
    t.has_prod = true;
    t.prod.kind = sim::BuildingClass::Other;
    t.prod.max_figures = {2, 0, 0};
    t.prod.figure_type_count = {3, 0, 0};
    t.prod.figure_ids = {203, 204, 205, 0, 0, 0, 0, 0, 0};
    return t;
}

sim::BuildingType supplier_type(sim::TypeId id, std::uint32_t prodtype, sim::RuntimeClass cls,
                                sim::GoodsId out) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = prodtype;
    t.cls = cls;
    t.entrances = {{3, 3}};
    t.output_goods = {out};
    return t;
}

/// Group 0 needs goods 26 (forced); group 1 is a copy (the config has two groups).
std::vector<sim::HouseGroup> groups(bool second_need = false, bool third_need = false) {
    sim::HouseGroup g;
    g.number = 0;
    g.wohnmax = 8;
    g.no_downgrade = true;
    g.needs = {{kFood, true, 41}};
    if (second_need) {
        g.needs.push_back({kOther, true, 16});
    }
    if (third_need) {
        g.needs.push_back({28, true, 16});
    }
    g.vermehr = {-16, -9, 0, 12, 32};
    sim::HouseGroup g1 = g;
    g1.number = 1;
    g1.no_downgrade = false;
    return {g, g1};
}

std::shared_ptr<const sim::TypeRegistry> registry(bool second_need = false, bool third_need = false,
                                                  std::vector<sim::GoodsInfo> goods = {}) {
    sim::TypeRegistry::Builder b;
    b.config(syn::population_config());
    b.walk_tables(sim::synthetic_walk_tables());
    if (!goods.empty()) {
        b.goods(std::move(goods));
    }
    b.house_groups(groups(second_need, third_need));
    b.add(house_type(kHouse, true));
    b.add(house_type(kHouseNoEnt, false));
    b.add(supplier_type(kStall, 161, sim::RuntimeClass::MarketStall, kFood));
    b.add(supplier_type(kStallB, 161, sim::RuntimeClass::MarketStall, kOther));
    b.add(supplier_type(kChapel, 67, sim::RuntimeClass::PublicService, kOther));
    b.add(tile_object_wall());
    {
        sim::BuildingType inn = supplier_type(kInn, 66, sim::RuntimeClass::Inn, kFood);
        inn.input_goods = {kOther};
        b.add(inn);
    }
    // 203..205: speed 0.012 -> 40265, capacity 3 t, layer 0 (open ground).
    b.figure_type(sim::FigureType{203, 40265, 3 * 256, 0, 1000});
    b.figure_type(sim::FigureType{204, 40265, 3 * 256, 0, 1000});
    b.figure_type(sim::FigureType{205, 40265, 3 * 256, 0, 1000});
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

sim::House fresh_house(std::size_t needs = 1) {
    sim::House h;
    h.residents_64 = 5 * 64;
    h.e = 5 * 128;
    h.stock.assign(needs, 0);
    h.walkers.assign(2, sim::FigureId{});
    return h;
}

/// Island 0 at (100, 100), 32 x 32, all city 0 of player 0.
struct Rig {
    sim::ScenarioSpec spec;
    sim::ScenarioSpec::CitySpec& city() { return spec.islands[0].cities[0]; }
    void place(sim::TypeId type, std::int32_t x, std::int32_t y,
               std::optional<sim::Payload> payload = std::nullopt) {
        sim::ScenarioSpec::BuildingSpec b;
        b.type = type;
        b.x = x;
        b.y = y;
        b.payload = std::move(payload);
        city().buildings.push_back(std::move(b));
    }
    void wall(std::int32_t x, std::int32_t y) {
        spec.islands[0].objects.push_back(sim::ScenarioSpec::TileObjectSpec{
            static_cast<std::uint32_t>((y - 100) * 32 + (x - 100)), kWall.v, 0});
    }
};

Rig make_rig() {
    Rig r;
    r.spec = syn::empty_scenario(77);
    sim::ScenarioSpec::PlayerSpec p;
    p.id = sim::PlayerId{0};
    r.spec.players.push_back(p);
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 100, 100, 32, 32);
    is.owner_runs = {{0, 32U * 32U}};
    is.cities.push_back(syn::plain_city(0, 0));
    r.spec.islands.push_back(std::move(is));
    return r;
}

sim::World create(const Rig& r, const std::shared_ptr<const sim::TypeRegistry>& types) {
    auto w = sim::World::create(r.spec, types, syn::population_config());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

sim::City& city0(sim::World& w) {
    return *sim::WorldTestAccess::islands(w)[0]->cities[0];
}

const sim::Building* at(const sim::World& w, std::int32_t x, std::int32_t y) {
    return w.islands()[0]->building_at(w.config().layout, sim::CityNo{0}, x, y);
}

const sim::House& house_of(const sim::World& w, std::int32_t x = 110, std::int32_t y = 110) {
    const sim::Building* b = at(w, x, y);
    REQUIRE(b != nullptr);
    const auto* h = std::get_if<sim::House>(&b->payload);
    REQUIRE(h != nullptr);
    return *h;
}

/// A scripted world PRNG: the values it hands out, and the sites it was asked for.
struct Script final : sim::DrawSource {
    std::vector<std::uint16_t> values;
    std::vector<sim::DrawSite> sites;
    std::uint16_t next(sim::DrawSite site) override {
        sites.push_back(site);
        REQUIRE(sites.size() <= values.size());
        return values[sites.size() - 1];
    }
};

/// The test seam: a city pass that sends one walker from `house` once armed (the House tick is not
/// wired yet; this is what the L4 tick will do with the kernel's `lowest`).
struct Sender {
    sim::BuildingId house;
    std::int64_t lowest = 50;
    int armed = 0; // walkers to send
    int sent = 0;
    sim::ResidentRefusal last_refusal = sim::ResidentRefusal::None;
};

void send_pass(void* raw, const sim::TickContext& ctx, sim::Island& island, sim::City& city,
               sim::Effects& fx) {
    auto& s = *static_cast<Sender*>(raw);
    if (s.armed <= 0) {
        return;
    }
    const sim::Building* b = island.find_building(s.house);
    const sim::BuildingType* t = b != nullptr ? ctx.types.find(b->type) : nullptr;
    if (b == nullptr || t == nullptr || island.cache.by_id.at(s.house).city.city != city.number) {
        return;
    }
    const auto* h = std::get_if<sim::House>(&b->payload);
    const sim::ResidentDispatch d = sim::resident_dispatch(
        ctx.draw, island, city, *b, *t, ctx.types, s.lowest, sim::resident_walkers_used(*h));
    s.last_refusal = d.refusal;
    --s.armed;
    if (d.spawn) {
        fx.spawns.push_back(*d.spawn);
        ++s.sent;
    }
}

void install(sim::World& w, Sender& s, std::size_t index = 0) {
    sim::Seams& seams = sim::WorldTestAccess::seams(w);
    seams.city_pass[index].name = index == 0 ? "resident_test_sender_a" : "resident_test_sender_b";
    seams.city_pass[index].fn = &send_pass;
    seams.city_pass[index].ctx = &s;
}
void uninstall(sim::World& w, std::size_t index = 0) {
    sim::Seams& seams = sim::WorldTestAccess::seams(w);
    seams.city_pass[index] = sim::Seam<sim::Seams::CityPass>{};
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

/// Advances one tick at a time until `done` holds (at most `limit` ticks); fails otherwise. The
/// invariants are checked every 64 ticks.
template <class F> void until(sim::World& w, F done, std::uint64_t limit = 40000) {
    for (std::uint64_t i = 0; i < limit; ++i) {
        w.advance(2);
        if (i % 64 == 0) {
            REQUIRE_MESSAGE(w.check_invariants() == "", w.check_invariants());
        }
        if (done()) {
            return;
        }
    }
    FAIL("the condition did not hold within the limit");
}

bool ended(const sim::World& w, sim::FigureId id) {
    const sim::Figure* f = sim::find_figure(w.figures(), id);
    return f == nullptr || f->expiry_stamp.has_value();
}

/// The step of a walker, or Done for a figure that is gone.
sim::ResidentStep step_of(const sim::World& w, sim::FigureId id) {
    const sim::Figure* f = sim::find_figure(w.figures(), id);
    return f != nullptr && f->resident ? f->resident->step : sim::ResidentStep::Done;
}

/// Sends one walker from the house at (110, 110) and returns its id (the world is advanced until
/// the walker exists; the walkers that already exist are not counted).
sim::FigureId spawn_one(sim::World& w, Sender& s) {
    const std::size_t before = walkers(w).size();
    s.house = at(w, 110, 110)->id;
    s.armed = 1;
    install(w, s);
    until(w, [&] { return walkers(w).size() > before; });
    uninstall(w);
    sim::FigureId newest{0};
    for (const sim::Figure* f : walkers(w)) {
        newest = std::max(newest, f->id);
    }
    return newest;
}

/// The standard world: house (110, 110), stall (114, 110).
Rig standard_rig() {
    Rig r = make_rig();
    r.place(kHouse, 110, 110, fresh_house());
    r.place(kStall, 114, 110);
    return r;
}

} // namespace

TEST_CASE("resident job: the stand lists derive from the layout roles") {
    const sim::ListLayout orig = sim::ListLayout::original();
    CHECK(sim::resident_stand_mask(orig) == 0x21); // stall list 0x20 + public list 0x01 (D)
    sim::ListLayout other = orig;
    other.roles.public_list = 5; // a modded layout: the mask follows the roles
    CHECK(sim::resident_stand_mask(other) == (0x20U | 0x02U));
}

TEST_CASE("resident dispatch: the draw trace of the spawn block") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kHouse, 110, 110, fresh_house());
    r.place(kHouseNoEnt, 115, 115, fresh_house());
    sim::World w = create(r, types);
    const sim::Island& is = *w.islands()[0];
    const sim::City& city = city0(w);
    const sim::Building& house = *at(w, 110, 110);
    const sim::BuildingType& type = *types->find(kHouse);
    REQUIRE(sim::serial_of(house.id) == 1); // the hand derivations below use serial 1

    SUBCASE("lowest 101, room: exactly one draw, k = ((r & 1) + y + x + serial) mod 3") {
        // x = 110, y = 110, serial 1: 221 + (r & 1); 221 mod 3 = 2, 222 mod 3 = 0.
        Script s;
        s.values = {5}; // odd: 222 mod 3 = 0 -> figure id 203
        const auto d = sim::resident_dispatch(s, is, city, house, type, *types, 101, 0);
        CHECK(d.condition);
        CHECK(d.drew);
        CHECK(d.draw == 5);
        CHECK(d.variant == 0);
        REQUIRE(s.sites.size() == 1);
        CHECK(s.sites[0] == sim::DrawSite::HouseWalkerTarget);
        REQUIRE(d.spawn.has_value());
        CHECK(d.spawn->figure_kind == 203);
        CHECK(d.spawn->job == 0x35);
        CHECK(d.spawn->resident);
        CHECK(d.spawn->home == house.id);
        CHECK(d.spawn->home_counter == 0);
        CHECK(d.spawn->owner == city.owner);
        CHECK(d.spawn->pos == sim::spawn_position(house, type));
        // The even draw: 221 mod 3 = 2 -> figure id 205 (the (r & 1) term matters).
        Script s2;
        s2.values = {4};
        const auto d2 = sim::resident_dispatch(s2, is, city, house, type, *types, 101, 0);
        CHECK(d2.variant == 2);
        REQUIRE(d2.spawn.has_value());
        CHECK(d2.spawn->figure_kind == 205);
        // Only the low bit of the draw counts: 0xfffe is even too.
        Script s3;
        s3.values = {0xfffe};
        CHECK(sim::resident_dispatch(s3, is, city, house, type, *types, 101, 0).variant == 2);
    }
    SUBCASE("a false condition draws nothing") {
        Script s; // no value: any draw would fail the script
        auto d = sim::resident_dispatch(s, is, city, house, type, *types, 102, 0);
        CHECK_FALSE(d.condition);
        CHECK_FALSE(d.drew);
        CHECK_FALSE(d.spawn.has_value());
        CHECK(s.sites.empty());
        d = sim::resident_dispatch(s, is, city, house, type, *types, 50, 2); // 2 of 2 walkers out
        CHECK_FALSE(d.condition);
        CHECK(s.sites.empty());
        d = sim::resident_dispatch(s, is, city, house, type, *types, 0x7fffffff, 0);
        CHECK(s.sites.empty());
        // One walker out of two: room.
        Script s1;
        s1.values = {1};
        d = sim::resident_dispatch(s1, is, city, house, type, *types, 50, 1);
        CHECK(d.condition);
        CHECK(s1.sites.size() == 1);
    }
    SUBCASE("a refused send still consumes exactly one draw (X14)") {
        const sim::Building& bare = *at(w, 115, 115); // the type without an entrance point
        const sim::BuildingType& bare_type = *types->find(kHouseNoEnt);
        Script s;
        s.values = {9};
        const auto d = sim::resident_dispatch(s, is, city, bare, bare_type, *types, 50, 0);
        CHECK(d.condition);
        CHECK(d.drew);
        CHECK(s.sites.size() == 1);
        CHECK(d.refusal == sim::ResidentRefusal::NoEntrance);
        CHECK_FALSE(d.spawn.has_value());
        // Inconsistent count (the caller says room, the payload has no empty slot): refused after
        // the draw as well.
        sim::Building full = house;
        std::get<sim::House>(full.payload).walkers = {sim::FigureId{7}, sim::FigureId{8}};
        Script s2;
        s2.values = {9};
        const auto d2 = sim::resident_dispatch(s2, is, city, full, type, *types, 50, 0);
        CHECK(s2.sites.size() == 1);
        CHECK(d2.refusal == sim::ResidentRefusal::NoFreeSlot);
        CHECK_FALSE(d2.spawn.has_value());
    }
    SUBCASE("x, y and the serial are taken as u16") {
        sim::Building shifted = house;
        shifted.x = 110 + 65536;
        shifted.y = 110 + 2 * 65536;
        shifted.id = sim::make_building_id(sim::IslandId{0}, 1 + 65536);
        Script s;
        s.values = {4};
        const auto d = sim::resident_dispatch(s, is, city, shifted, type, *types, 50, 0);
        CHECK(d.variant == 2); // the same as the unshifted building
        // A different x changes the figure: x = 111 -> 222 mod 3 = 0.
        sim::Building moved = house;
        moved.x = 111;
        Script s2;
        s2.values = {4};
        CHECK(sim::resident_dispatch(s2, is, city, moved, type, *types, 50, 0).variant == 0);
    }
    SUBCASE("the first empty walker slot is taken") {
        sim::Building b = house;
        std::get<sim::House>(b.payload).walkers = {sim::FigureId{7}, sim::FigureId{}};
        Script s;
        s.values = {4};
        const auto d = sim::resident_dispatch(s, is, city, b, type, *types, 50, 1);
        REQUIRE(d.spawn.has_value());
        CHECK(d.spawn->home_counter == 1);
    }
}

TEST_CASE("resident job: a walker fetches food at a stall and the house gains exactly amount times 16") {
    const auto types = registry();
    Rig r = standard_rig();
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = 5000;
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    Sender sender;
    const sim::FigureId id = spawn_one(w, sender);

    const sim::Figure* f = sim::find_figure(w.figures(), id);
    REQUIRE(f != nullptr);
    CHECK(f->job == 0x35);
    CHECK(f->kind >= 203);
    CHECK(f->kind <= 205);
    CHECK(f->home == at(w, 110, 110)->id);
    CHECK(f->home_counter == 0);
    REQUIRE(f->resident.has_value());
    CHECK(f->resident->cargo.size() == 3);
    CHECK(house_of(w).walkers[0] == id); // the slot is taken at the merge
    CHECK(house_of(w).walkers[1] == sim::FigureId{});

    until(w, [&] { return ended(w, id); });

    f = sim::find_figure(w.figures(), id);
    REQUIRE(f != nullptr);
    REQUIRE(f->resident.has_value());
    CHECK(f->resident->step == sim::ResidentStep::Done);
    CHECK(f->resident->cargo[0] == sim::ResidentCargo{kFood, 151}); // min(5000, 151, 768)
    CHECK(f->resident->cargo[1] == sim::ResidentCargo{});           // the second pass buys nothing
    CHECK(city0(w).stock.stock[kFood] == 5000 - 151);               // the real supplier debit
    CHECK(city0(w).billed[kFood] == 151);                           // the requested amount
    CHECK(house_of(w).stock[0] == 151 * 16);                        // 2416
    CHECK(house_of(w).walkers[0] == sim::FigureId{});               // the slot is released
    CHECK(f->home == sim::BuildingId{});
    CHECK(w.check_invariants() == "");
    CHECK(stats.residents.searches == 1);
    CHECK(stats.residents.arrivals == 1);
    CHECK(stats.residents.purchases == 1);
    CHECK(stats.residents.bought_88 == 151);
    CHECK(stats.residents.deliveries == 1);
    CHECK(stats.residents.delivered_88 == 151);
    CHECK(stats.residents.lost_88 == 0);
    // The figure is destroyed by the expiry list later and the slot stays free.
    until(w, [&] { return sim::find_figure(w.figures(), id) == nullptr; }, 20000);
    CHECK(house_of(w).walkers[0] == sim::FigureId{});
    CHECK(w.check_invariants() == "");
}

TEST_CASE("resident job: the IdSec replay of the delivery follows the chain") {
    // Goods 26 -> 27 -> 28: a need for all three, the walker fetches 26 only (the stall), and the
    // delivery raises all three slots by amount*16 (the recursive deliver, review P1-1).
    std::vector<sim::GoodsInfo> goods = sim::original_goods();
    goods[26].id_sec = 27;
    goods[27].id_sec = 28;
    const auto types = registry(true, true, goods);
    Rig r = make_rig();
    r.place(kHouse, 110, 110, fresh_house(3));
    r.place(kStall, 114, 110);
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = 5000;
    Sender sender;
    const sim::FigureId id = spawn_one(w, sender);
    until(w, [&] { return ended(w, id); });
    CHECK(sim::find_figure(w.figures(), id)->resident->cargo[0] == sim::ResidentCargo{kFood, 151});
    CHECK(house_of(w).stock[0] == 2416);
    CHECK(house_of(w).stock[1] == 2416); // 26 -> 27
    CHECK(house_of(w).stock[2] == 2416); // 27 -> 28
    CHECK(w.check_invariants() == "");
}

TEST_CASE("resident job: a public building supplies without consuming the store") {
    const auto types = registry(true);
    Rig r = make_rig();
    r.place(kHouse, 110, 110, fresh_house(2));
    r.place(kChapel, 114, 110);
    sim::World w = create(r, types);
    city0(w).stock.stock[kOther] = 5000;
    Sender sender;
    const sim::FigureId id = spawn_one(w, sender);
    until(w, [&] { return ended(w, id); });
    // Goods 27, Menge 16: demand = trunc(16*640*256/44236) = 59; avail 256: min(256, 59, 768) = 59.
    CHECK(sim::find_figure(w.figures(), id)->resident->cargo[0] == sim::ResidentCargo{kOther, 59});
    CHECK(city0(w).stock.stock[kOther] == 5000); // not consumed
    CHECK(city0(w).billed[kOther] == 59);        // billed (27 is in the billing range 26..40)
    CHECK(house_of(w).stock[1] == 59 * 16);
}

namespace {

/// The outcome of a walker that never reached a purchase.
void check_no_purchase(sim::World& w, sim::FigureId id, const sim::TransportStats& stats,
                       std::uint32_t store) {
    const sim::Figure* f = sim::find_figure(w.figures(), id);
    REQUIRE(f != nullptr);
    CHECK(f->resident->cargo[0] == sim::ResidentCargo{});
    CHECK(city0(w).stock.stock[kFood] == store);
    CHECK(city0(w).billed[kFood] == 0);
    CHECK(house_of(w).stock[0] == 0);
    CHECK(house_of(w).walkers[0] == sim::FigureId{}); // released all the same
    CHECK(stats.residents.purchases == 0);
    CHECK(stats.residents.arrivals == 0); // it never reached a stand
    CHECK(w.check_invariants() == "");
}

} // namespace

TEST_CASE("resident job: controls that stay short of the purchase") {
    const auto types = registry();
    SUBCASE("blocked route: the stall is walled in, the search fails, the walker goes home") {
        Rig r = standard_rig();
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                if (dx != 0 || dy != 0) {
                    r.wall(114 + dx, 110 + dy);
                }
            }
        }
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 5000;
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        until(w, [&] { return ended(w, id); });
        CHECK(stats.residents.searches == 1);
        CHECK(stats.residents.searches_failed == 1); // the intended failure: no path
        check_no_purchase(w, id, stats, 5000);
    }
    SUBCASE("no stock: a supplier at the threshold (12) is no candidate") {
        Rig r = standard_rig();
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 12; // thr = 12, the offer must be strictly greater
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        until(w, [&] { return ended(w, id); });
        CHECK(stats.residents.searches_failed == 1); // the intended failure: no candidate
        check_no_purchase(w, id, stats, 12);
    }
    SUBCASE("one unit more (13) is a candidate and is bought whole") {
        Rig r = standard_rig();
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 13;
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        until(w, [&] { return ended(w, id); });
        CHECK(stats.residents.searches_failed == 0);
        CHECK(sim::find_figure(w.figures(), id)->resident->cargo[0] ==
              sim::ResidentCargo{kFood, 13}); // min(13, 151, 768)
        CHECK(city0(w).stock.stock[kFood] == 0);
        CHECK(house_of(w).stock[0] == 13 * 16);
    }
    SUBCASE("a stall of goods the house does not need is no candidate") {
        Rig r = make_rig();
        r.place(kHouse, 110, 110, fresh_house());
        r.place(kStallB, 114, 110); // sells goods 27: the house needs only 26
        sim::World w = create(r, types);
        city0(w).stock.stock[kOther] = 5000;
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        until(w, [&] { return ended(w, id); });
        CHECK(stats.residents.searches_failed == 1);
        CHECK(city0(w).stock.stock[kOther] == 5000);
    }
}

TEST_CASE(
    "resident job: the stand is the highest score, not the nearest, and a tie keeps the first") {
    const auto types = registry(true);
    SUBCASE("scores differ: the far, higher-score stall wins") {
        // The house needs goods 26 (stock 0: urgency 128) and 27 (stock 400: s = trunc(1536*400/
        // (640*16)) = 60, urgency 68). The near stall (east, 3 tiles) sells 27 (68), the far stall
        // (south, 6 tiles) sells 26 (128).
        sim::House h = fresh_house(2);
        h.stock = {0, 400};
        Rig r = make_rig();
        r.place(kHouse, 110, 110, h);
        r.place(kStallB, 113, 110);
        r.place(kStall, 110, 116);
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 5000;
        city0(w).stock.stock[kOther] = 5000;
        const sim::BuildingId far_id = at(w, 110, 116)->id;
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        sim::BuildingId seen;
        until(w, [&] {
            const sim::Figure* f = sim::find_figure(w.figures(), id);
            if (f != nullptr && f->resident && f->resident->target.valid()) {
                seen = f->resident->target;
            }
            return ended(w, id);
        });
        CHECK(seen == far_id);
        const sim::Figure* f = sim::find_figure(w.figures(), id);
        CHECK(f->resident->cargo[0].goods == kFood); // bought at the far stall
        CHECK(city0(w).stock.stock[kFood] < 5000);
        CHECK(city0(w).stock.stock[kOther] == 5000); // the near stall was passed by
    }
    SUBCASE("equal scores: the first discovery (the nearer stall) keeps the tie") {
        Rig r = make_rig();
        r.place(kHouse, 110, 110, fresh_house(2));
        r.place(kStall, 113, 110); // near, east
        r.place(kStall, 110, 116); // far, south
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 5000;
        const sim::BuildingId near_id = at(w, 113, 110)->id;
        const sim::BuildingId far_id = at(w, 110, 116)->id;
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        sim::BuildingId seen;
        until(w, [&] {
            const sim::Figure* f = sim::find_figure(w.figures(), id);
            if (f != nullptr && f->resident && f->resident->target.valid()) {
                seen = f->resident->target;
            }
            return ended(w, id);
        });
        CHECK(seen == near_id);
        CHECK(seen != far_id);
    }
    SUBCASE("equal scores: path-cost discovery, not candidate-list order, keeps the tie") {
        // rect_query orders candidates by (y, x, id): the FAR stall (110, 104) sorts first and has
        // the lower id; the NEAR stall (113, 110) is discovered first by the walk search.
        Rig r = make_rig();
        r.place(kHouse, 110, 110, fresh_house(2));
        r.place(kStall, 110, 104); // far, north, first in (y, x, id) order, lowest id
        r.place(kStall, 113, 110); // near, east
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 5000;
        const sim::BuildingId far_id = at(w, 110, 104)->id;
        const sim::BuildingId near_id = at(w, 113, 110)->id;
        CHECK(far_id.v < near_id.v);
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        sim::BuildingId seen;
        until(w, [&] {
            const sim::Figure* f = sim::find_figure(w.figures(), id);
            if (f != nullptr && f->resident && f->resident->target.valid()) {
                seen = f->resident->target;
            }
            return ended(w, id);
        });
        CHECK(seen == near_id);
        CHECK(seen != far_id);
    }
}

TEST_CASE("resident job: two competing buyers, the second sees the reduced store") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kHouse, 108, 110, fresh_house());
    r.place(kHouse, 112, 108, fresh_house());
    r.place(kStall, 110, 112);
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = 200;
    Sender s1, s2;
    s1.house = at(w, 108, 110)->id;
    s2.house = at(w, 112, 108)->id;
    s1.armed = s2.armed = 1;
    install(w, s1, 0);
    install(w, s2, 1);
    until(w, [&] { return walkers(w).size() == 2; });
    uninstall(w, 0);
    uninstall(w, 1);
    const sim::FigureId a = walkers(w)[0]->id;
    const sim::FigureId b = walkers(w)[1]->id;
    until(w, [&] { return ended(w, a) && ended(w, b); });
    const std::uint32_t ca = sim::find_figure(w.figures(), a)->resident->cargo[0].amount_88;
    const std::uint32_t cb = sim::find_figure(w.figures(), b)->resident->cargo[0].amount_88;
    // One bought the demand (151) from 200, the other what was left (49): 49 > 12 is a candidate.
    CHECK(std::min(ca, cb) == 49);
    CHECK(std::max(ca, cb) == 151);
    CHECK(city0(w).stock.stock[kFood] == 0);
    CHECK(city0(w).billed[kFood] == 200); // the requested amounts: 151 and 49
    CHECK(w.check_invariants() == "");
}

TEST_CASE("resident job: removed buildings") {
    const auto types = registry();
    auto remove_at = [](sim::World& w, std::int32_t x, std::int32_t y) {
        const sim::Building* b = at(w, x, y);
        REQUIRE(b != nullptr);
        const sim::BuildingKey key = w.islands()[0]->cache.by_id.at(b->id);
        REQUIRE(sim::WorldTestAccess::remove(w, key));
    };
    SUBCASE("the supplier is removed before the arrival: nothing is bought, the walker goes home") {
        Rig r = standard_rig();
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 5000;
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        until(w, [&] { return step_of(w, id) == sim::ResidentStep::ToStall; });
        remove_at(w, 114, 110);
        until(w, [&] { return ended(w, id); });
        CHECK(stats.residents.arrivals == 1);
        CHECK(stats.residents.arrivals_empty == 1);
        CHECK(stats.residents.purchases == 0);
        CHECK(city0(w).stock.stock[kFood] == 5000);
        CHECK(house_of(w).stock[0] == 0);
        CHECK(house_of(w).walkers[0] == sim::FigureId{});
        CHECK(w.check_invariants() == "");
    }
    SUBCASE("the supplier is removed after the purchase: the delivery still happens") {
        Rig r = standard_rig();
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 5000;
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        until(w, [&] { return step_of(w, id) == sim::ResidentStep::ToHome; });
        remove_at(w, 114, 110);
        until(w, [&] { return ended(w, id); });
        CHECK(city0(w).stock.stock[kFood] == 5000 - 151);
        CHECK(house_of(w).stock[0] == 2416);
        CHECK(w.check_invariants() == "");
    }
    SUBCASE("a replacement building on the supplier's tile is used (D: the building on the tile)") {
        Rig r = standard_rig();
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 5000;
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        until(w, [&] { return step_of(w, id) == sim::ResidentStep::ToStall; });
        const sim::BuildingId old_id = at(w, 114, 110)->id;
        remove_at(w, 114, 110);
        sim::PlacementRecord rec;
        rec.type = kStall; // another building of the same type: a new id, the same tile
        rec.x = 114;
        rec.y = 110;
        REQUIRE(sim::WorldTestAccess::create(w, 0, 0, rec, 0).ok());
        REQUIRE(at(w, 114, 110) != nullptr);
        CHECK(at(w, 114, 110)->id != old_id);
        until(w, [&] { return ended(w, id); });
        CHECK(sim::find_figure(w.figures(), id)->resident->cargo[0] ==
              sim::ResidentCargo{kFood, 151});
        CHECK(city0(w).stock.stock[kFood] == 5000 - 151);
        CHECK(w.check_invariants() == "");
    }
    SUBCASE("the home is removed during the walk to the stall: no purchase, the job ends") {
        Rig r = standard_rig();
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 5000;
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        until(w, [&] { return step_of(w, id) == sim::ResidentStep::ToStall; });
        remove_at(w, 110, 110);
        until(w, [&] { return ended(w, id); });
        CHECK(stats.residents.purchases == 0);
        CHECK(city0(w).stock.stock[kFood] == 5000);
        CHECK(sim::find_figure(w.figures(), id)->resident->cargo[0] == sim::ResidentCargo{});
        CHECK(w.check_invariants() == "");
    }
    SUBCASE("the home is removed while carrying: the cargo is lost, nothing dangles") {
        Rig r = standard_rig();
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 5000;
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        until(w, [&] { return step_of(w, id) == sim::ResidentStep::ToHome; });
        remove_at(w, 110, 110);
        until(w, [&] { return ended(w, id); });
        CHECK(stats.residents.deliveries == 0);
        CHECK(stats.residents.lost_88 == 151);
        CHECK(city0(w).stock.stock[kFood] == 5000 - 151); // the purchase had happened
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("resident job: the end path finishes the job and releases once") {
    sim::Figure f;
    f.id = sim::FigureId{5};
    f.home = sim::make_building_id(sim::IslandId{0}, 3);
    f.home_counter = 1;
    f.job = 0x35;
    f.resident = sim::ResidentState{};
    f.resident->step = sim::ResidentStep::ToHome;
    f.resident->route = {sim::FixedPos{1, 1}, sim::FixedPos{2, 2}};
    f.resident->next = 1;
    f.resident->along = 9;
    f.resident->target = sim::make_building_id(sim::IslandId{0}, 4);
    f.resident->cargo = {sim::ResidentCargo{26, 5}, {}, {}};
    sim::Effects fx;
    CHECK(sim::end_figure(f, 77, fx));
    REQUIRE(fx.releases.size() == 1);
    CHECK(fx.releases[0].home == sim::make_building_id(sim::IslandId{0}, 3));
    CHECK(fx.releases[0].home_counter == 1);
    CHECK(fx.releases[0].holder == sim::FigureId{5});
    CHECK(f.home == sim::BuildingId{});
    CHECK(f.resident->step == sim::ResidentStep::Done); // an end mid-job finishes the job
    CHECK(f.resident->route.empty());
    CHECK(f.resident->next == 0);
    CHECK(f.resident->along == 0);
    CHECK(f.resident->target == sim::BuildingId{});
    CHECK(f.resident->cargo[0] == sim::ResidentCargo{26, 5}); // the cargo stays as history
    CHECK_FALSE(sim::end_figure(f, 78, fx));                  // called twice: nothing more
    CHECK(fx.releases.size() == 1);
}

namespace {

/// A figure_tick seam that issues one stale release (a ReleaseCounter naming `holder`) and then
/// does nothing; the stale holder is an older walker of the slot.
struct StaleRelease {
    sim::BuildingId home;
    sim::FigureId holder;
    bool fired = false;
};

void stale_release(void* raw, const sim::TickContext&, sim::SerialWorld& w, sim::Figure&) {
    auto& s = *static_cast<StaleRelease*>(raw);
    if (!s.fired) {
        s.fired = true;
        sim::ReleaseCounter r;
        r.home = s.home;
        r.home_counter = 0;
        r.holder = s.holder;
        w.fx.releases.push_back(r);
    }
}

} // namespace

TEST_CASE("resident job: an old job never clears a newer owner's walker slot") {
    const auto types = registry();
    Rig r = standard_rig();
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = 5000;
    Sender sender;
    const sim::FigureId first = spawn_one(w, sender);
    until(w, [&] { return ended(w, first); });
    CHECK(house_of(w).walkers[0] == sim::FigureId{});
    // The delivered food satisfied the house; empty its stock again so that a new walker has a
    // need.
    std::get<sim::House>(
        sim::find_building_mut(*sim::WorldTestAccess::islands(w)[0], at(w, 110, 110)->id)->payload)
        .stock[0] = 0;
    const sim::FigureId second = spawn_one(w, sender); // the freed slot goes to a new walker
    CHECK(second != first);
    CHECK(house_of(w).walkers[0] == second);
    // A stale release that names the first walker as holder leaves the newer owner's slot alone.
    StaleRelease stale;
    stale.home = at(w, 110, 110)->id;
    stale.holder = first;
    sim::Seams& seams = sim::WorldTestAccess::seams(w);
    seams.figure_tick.name = "resident_test_stale_release";
    seams.figure_tick.fn = &stale_release;
    seams.figure_tick.ctx = &stale;
    until(w, [&] { return stale.fired; });
    seams.figure_tick = sim::Seam<sim::Seams::FigureTick>{};
    CHECK(house_of(w).walkers[0] == second);
    // The control: the same release naming the real holder clears it.
    stale = StaleRelease{};
    stale.home = at(w, 110, 110)->id;
    stale.holder = second;
    seams.figure_tick.name = "resident_test_stale_release";
    seams.figure_tick.fn = &stale_release;
    seams.figure_tick.ctx = &stale;
    until(w, [&] { return stale.fired; });
    seams.figure_tick = sim::Seam<sim::Seams::FigureTick>{};
    CHECK(house_of(w).walkers[0] == sim::FigureId{});
}

TEST_CASE("resident job: save and reload at every step, and after the end") {
    const auto types = registry();
    auto finish = [&](sim::World& world, sim::FigureId id) {
        until(world, [&] { return ended(world, id); });
        return std::pair{world.state_hash(), world.check_invariants()};
    };
    auto fresh = [&] {
        Rig r = standard_rig();
        sim::World w = create(r, types);
        city0(w).stock.stock[kFood] = 5000;
        return w;
    };
    sim::World ref = fresh();
    Sender sender;
    const auto reference = finish(ref, spawn_one(ref, sender));
    CHECK(reference.second == "");
    // Start is resolved inside the visit that follows the spawn (no tick ends in it), so the
    // observable steps are the walks and the wait.
    for (const sim::ResidentStep s :
         {sim::ResidentStep::ToStall, sim::ResidentStep::Wait, sim::ResidentStep::ToHome}) {
        sim::World w = fresh();
        Sender snd;
        const sim::FigureId id = spawn_one(w, snd);
        until(w, [&] { return step_of(w, id) == s; });
        REQUIRE(w.check_invariants() == "");
        const std::vector<std::uint8_t> bytes = w.save();
        auto back = sim::World::load(bytes, types);
        REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
        sim::World w2 = std::move(back).value();
        CHECK(w2.check_invariants() == "");
        CHECK(w2.state_hash() == w.state_hash());
        CHECK(w2.save() == bytes);
        const auto got = finish(w2, id);
        CHECK(got.second == "");
        CHECK(got.first == reference.first); // the reload changed nothing
    }
    // The terminal state reloads as a valid world.
    const std::vector<std::uint8_t> bytes = ref.save();
    auto back = sim::World::load(bytes, types);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    CHECK(back.value().check_invariants() == "");
    CHECK(back.value().state_hash() == ref.state_hash());
    CHECK(house_of(back.value()).stock[0] == 2416);
    CHECK(house_of(back.value()).walkers[0] == sim::FigureId{});
}

TEST_CASE("resident job: the figure validators name a broken job fact") {
    const auto types = registry();
    Rig r = standard_rig();
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = 5000;
    Sender sender;
    const sim::FigureId id = spawn_one(w, sender);
    REQUIRE(w.check_invariants() == "");
    auto broken = [&](auto&& mutate) {
        sim::World c = w;
        mutate(*sim::find_figure(sim::WorldTestAccess::figures(c), id));
        return c.check_invariants();
    };
    const auto has = [](const std::string& s, const char* part) {
        return s.find(part) != std::string::npos;
    };
    CHECK(has(broken([](sim::Figure& f) { f.job = 0x34; }), "only the house walker job 0x35"));
    CHECK(has(broken([](sim::Figure& f) { f.resident->wait = 3; }), ".wait"));
    CHECK(has(broken([](sim::Figure& f) { f.resident->route.clear(); }), "no route, but next"));
    {
        const std::string m = broken([](sim::Figure& f) {
            f.resident->route.clear();
            f.resident->next = 0;
            f.resident->step = sim::ResidentStep::Done;
        });
        CHECK(has(m, "Done but the figure has not ended"));
    }
    CHECK(has(broken([](sim::Figure& f) { f.resident->step = static_cast<sim::ResidentStep>(9); }),
              "is no step"));
}

// ------------------------------------------------------------------ direct resident_tick calls

namespace {

/// One direct call of the resident body on a COPY of a figure at world tick `t`; the effects go
/// to `fx` and are never merged, so the world stays as it is.
bool direct_call(sim::World& w, sim::Figure& fig, std::uint32_t t, sim::Effects& fx) {
    sim::GlobalDraw global(sim::WorldTestAccess::rng(w));
    sim::CountingDraw draw(global);
    const sim::TickContext ctx{t,
                               t,
                               w.config(),
                               w.rules(),
                               w.settings(),
                               w.types(),
                               sim::PlayersView(w.players()),
                               sim::IslandsView(w.islands()),
                               draw,
                               w.seams()};
    sim::SerialWorld serial(sim::WorldTestAccess::islands(w), w.config().layout, w.figures(),
                            w.players(), fx);
    return sim::resident_tick(ctx, serial, fig);
}

struct Snap {
    std::optional<sim::ResidentState> resident;
    sim::FixedPos pos;
    std::uint16_t stamp = 0;
    bool operator==(const Snap&) const = default;
};
Snap snap(const sim::Figure& f) {
    return Snap{f.resident, f.pos, f.stamp};
}

/// A fresh walker (stock 5000, stall adjacent east) and a copy of it with the stamp at `stamp`.
struct Direct {
    sim::World w;
    sim::Figure fig;
};
Direct direct_rig(std::uint16_t stamp) {
    const auto types = registry();
    Rig r = standard_rig();
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = 5000;
    Sender sender;
    const sim::FigureId id = spawn_one(w, sender);
    sim::Figure f = *sim::find_figure(w.figures(), id);
    f.stamp = stamp;
    return Direct{std::move(w), std::move(f)};
}

} // namespace

TEST_CASE("resident job: direct calls, zero elapsed ticks change nothing") {
    Direct d = direct_rig(1000);
    const Snap before = snap(d.fig);
    const std::uint64_t hash = d.w.state_hash();
    sim::Effects fx;
    CHECK(direct_call(d.w, d.fig, 1000, fx));
    CHECK(direct_call(d.w, d.fig, 1000, fx));
    CHECK(snap(d.fig) == before);
    CHECK(fx.goods_moves.empty());
    CHECK(fx.house_deliveries.empty());
    CHECK(fx.releases.empty());
    CHECK(d.w.state_hash() == hash);
}

TEST_CASE("resident job: direct calls are partition invariant, also across the u16 wrap") {
    for (const std::uint16_t base : {std::uint16_t{1000}, std::uint16_t{65500}}) {
        CAPTURE(base);
        // One tick at a time until the walker is Done.
        Direct a = direct_rig(base);
        sim::Effects fa;
        std::uint32_t done_at = 0;
        for (std::uint32_t k = 1; k <= 60000 && done_at == 0; ++k) {
            REQUIRE(direct_call(a.w, a.fig, base + k, fa));
            if (a.fig.resident->step == sim::ResidentStep::Done) {
                done_at = k;
            }
        }
        REQUIRE(done_at > 2);
        // One large call at the same final tick (and a split in two) gives the same state.
        for (const std::uint32_t split : {0U, 1U, done_at / 2, done_at - 1}) {
            CAPTURE(split);
            Direct b = direct_rig(base);
            sim::Effects fb;
            if (split != 0) {
                REQUIRE(direct_call(b.w, b.fig, base + split, fb));
            }
            REQUIRE(direct_call(b.w, b.fig, base + done_at, fb));
            CHECK(snap(b.fig) == snap(a.fig));
            CHECK(fb.goods_moves.size() == fa.goods_moves.size());
            CHECK(fb.house_deliveries.size() == fa.house_deliveries.size());
            CHECK(fb.billed_deltas.size() == fa.billed_deltas.size());
        }
        // The minimum: one tick fewer is not Done in a single call either.
        Direct c = direct_rig(base);
        sim::Effects fc;
        REQUIRE(direct_call(c.w, c.fig, base + done_at - 1, fc));
        CHECK(c.fig.resident->step != sim::ResidentStep::Done);
        // Exactly one purchase and one delivery over the whole run.
        CHECK(fa.goods_moves.size() == 1);
        CHECK(fa.house_deliveries.size() == 1);
        CHECK(a.fig.resident->cargo[0].goods == kFood);
    }
}

TEST_CASE("resident job: a Wait split across calls buys exactly once") {
    Direct a = direct_rig(2000);
    sim::Effects fx;
    bool saw_wait = false;
    std::size_t moves_in_wait = 0;
    for (std::uint32_t k = 1; k <= 60000 && a.fig.resident->step != sim::ResidentStep::Done; ++k) {
        REQUIRE(direct_call(a.w, a.fig, 2000 + k, fx));
        if (a.fig.resident->step == sim::ResidentStep::Wait) {
            // The wait survives the call boundary and nothing was bought yet. It is the figure
            // type's load wait (D step B start 0x10056330 -> +0xe0 = 0x10035260: round(500 * d)
            // ticks, here 1000 = 2.0 for figure types 203..205) and shrinks by exactly the
            // ticks of each call.
            CHECK(a.fig.resident->wait <= 1000);
            CHECK(a.fig.resident->wait >= 1);
            moves_in_wait = fx.goods_moves.size();
            saw_wait = true;
        }
    }
    CHECK(saw_wait);
    CHECK(moves_in_wait == 0);
    CHECK(fx.goods_moves.size() == 1);
    CHECK(a.fig.resident->cargo[0].goods == kFood);
    CHECK(a.fig.resident->cargo[1] == sim::ResidentCargo{});
}

TEST_CASE("resident job: a crafted house (E == 0) is a counted fault and changes nothing") {
    const auto types = registry();
    Rig r = standard_rig();
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = 5000;
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    {
        sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
        const sim::BuildingKey key = is.cache.by_id.at(at(w, 110, 110)->id);
        auto& house = std::get<sim::House>(
            is.cities[0]->index.lists[key.list].slots[key.slot].value().payload);
        house.e = 0; // crafted: the choice divides by E * Menge
    }
    Sender sender;
    sender.house = at(w, 110, 110)->id;
    sender.armed = 1;
    install(w, sender);
    // (not spawn_one: its periodic invariant check would meet the very fault under test)
    // P-1/P-3 (L4): the fault is loud: the world stops at the merge of the walker's call.
    for (int k = 0; k < 40000 && !w.faulted(); ++k) {
        w.advance(2);
    }
    REQUIRE(w.faulted());
    CHECK(w.fault().find("resident fault choice_divisor") != std::string::npos);
    CHECK(w.check_invariants().find("world fault: resident fault choice_divisor") == 0);
    const std::uint64_t frozen = w.state_hash();
    w.advance(2); // a faulted world changes nothing, not even the clock
    CHECK(w.state_hash() == frozen);
    CHECK(stats.residents.faults == 1);
    CHECK(stats.residents.purchases == 0);
    CHECK(stats.residents.deliveries == 0);
    CHECK(city0(w).stock.stock[kFood] == 5000);
    CHECK(city0(w).billed[kFood] == 0);
    CHECK(house_of(w).stock[0] == 0);
}

namespace {
struct Inject {
    sim::BuildingId home;
    sim::FigureId figure;
    bool armed = false;
};
void inject_pass(void* raw, const sim::TickContext&, sim::Island&, sim::City&, sim::Effects& fx) {
    auto& in = *static_cast<Inject*>(raw);
    if (in.armed) {
        in.armed = false;
        fx.house_deliveries.push_back(sim::HouseDelivery{in.home, in.figure, kFood, 100});
    }
}
} // namespace

TEST_CASE("resident job: a delivery is applied only while the walker slot names the figure") {
    // L2 review P2-3: the slot-ownership check of the delivery merge (scheduler.cpp). A delivery
    // for a live figure whose house slot does not name it (the slot was cleared behind its back) is
    // refused; one for the slot's owner is applied. The effect is injected through a city pass.
    const auto types = registry();
    Rig r = standard_rig();
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = 5000; // the walker is still away when the effect arrives
    Sender sender;
    const sim::FigureId id = spawn_one(w, sender);
    Inject in;
    in.home = at(w, 110, 110)->id;
    in.figure = id;
    sim::Seams& seams = sim::WorldTestAccess::seams(w);
    seams.city_pass[1].name = "resident_test_inject";
    seams.city_pass[1].fn = &inject_pass;
    seams.city_pass[1].ctx = &in;
    // The slot names the figure: applied (100 * 16).
    in.armed = true;
    for (int k = 0; k < 64 && in.armed; ++k) {
        w.advance(2);
    }
    REQUIRE_FALSE(in.armed);
    CHECK(house_of(w).stock[0] == 1600);
    // The slot is cleared (the figure still names the house): refused, the stock stays.
    {
        sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
        const sim::BuildingKey key = is.cache.by_id.at(at(w, 110, 110)->id);
        std::get<sim::House>(is.cities[0]->index.lists[key.list].slots[key.slot].value().payload)
            .walkers[0] = sim::FigureId{};
    }
    in.armed = true;
    for (int k = 0; k < 64 && in.armed; ++k) {
        w.advance(2);
    }
    REQUIRE_FALSE(in.armed);
    CHECK(house_of(w).stock[0] == 1600);
}

// ------------------------------------------------------------------ MAP-POP-CLOSE-1 C2 (the stall wait)

namespace {

/// Single-tick calls until the walker is in Wait; returns the tick of that call.
std::uint32_t to_wait(Direct& d, sim::Effects& fx, std::uint32_t base) {
    for (std::uint32_t k = 1; k <= 60000; ++k) {
        REQUIRE(direct_call(d.w, d.fig, base + k, fx));
        if (d.fig.resident->step == sim::ResidentStep::Wait) {
            return base + k;
        }
        REQUIRE(d.fig.resident->step != sim::ResidentStep::Done);
    }
    FAIL("the walker never reached the wait");
    return 0;
}

} // namespace

TEST_CASE("resident job wait: the wait is the figure type load wait and shrinks by the elapsed ticks") {
    // Figure types 203..205 of this rig have load_ticks 1000 (2.0 * 500). The arrival tick sets
    // wait = 1000 with no tick left, so the first observation is the full wait.
    Direct d = direct_rig(1000);
    sim::Effects fx;
    const std::uint32_t t = to_wait(d, fx, 1000);
    CHECK(d.fig.resident->wait == 1000);
    // Zero elapsed ticks: nothing changes (state and world hash).
    const Snap before = snap(d.fig);
    const std::uint64_t hash = d.w.state_hash();
    CHECK(direct_call(d.w, d.fig, t, fx));
    CHECK(snap(d.fig) == before);
    CHECK(d.w.state_hash() == hash);
    // The smallest positive visit: one tick leaves 999 and buys nothing yet.
    CHECK(direct_call(d.w, d.fig, t + 1, fx));
    CHECK(d.fig.resident->step == sim::ResidentStep::Wait);
    CHECK(d.fig.resident->wait == 999);
    CHECK(fx.goods_moves.empty());
    // One tick before the end: 1 left, still no purchase; the last tick buys (once).
    CHECK(direct_call(d.w, d.fig, t + 999, fx));
    CHECK(d.fig.resident->step == sim::ResidentStep::Wait);
    CHECK(d.fig.resident->wait == 1);
    CHECK(fx.goods_moves.empty());
    CHECK(direct_call(d.w, d.fig, t + 1000, fx));
    CHECK(d.fig.resident->step == sim::ResidentStep::ToHome);
    CHECK(fx.goods_moves.size() == 1);
}

TEST_CASE("resident job wait: a split delay with surplus equals one call and the surplus walks home") {
    // From Wait (1000 ticks left): 1000 + 37 ticks in one call versus 600 then 437.
    Direct a = direct_rig(1000);
    Direct b = direct_rig(1000);
    sim::Effects fa, fb;
    const std::uint32_t ta = to_wait(a, fa, 1000);
    const std::uint32_t tb = to_wait(b, fb, 1000);
    REQUIRE(ta == tb);
    REQUIRE(direct_call(a.w, a.fig, ta + 1037, fa));
    REQUIRE(direct_call(b.w, b.fig, tb + 600, fb));
    CHECK(b.fig.resident->step == sim::ResidentStep::Wait);
    CHECK(b.fig.resident->wait == 400);
    REQUIRE(direct_call(b.w, b.fig, tb + 1037, fb));
    CHECK(snap(a.fig) == snap(b.fig));
    CHECK(fa.goods_moves.size() == 1);
    CHECK(fb.goods_moves.size() == 1);
    // The 37 surplus ticks were walked: the figure left the stand and is on the way home.
    CHECK(a.fig.resident->step == sim::ResidentStep::ToHome);
    Direct c = direct_rig(1000);
    sim::Effects fc;
    const std::uint32_t tc = to_wait(c, fc, 1000);
    REQUIRE(direct_call(c.w, c.fig, tc + 1000, fc));
    CHECK_FALSE(snap(c.fig) == snap(a.fig)); // the same purchase one call earlier: not yet moved
}

TEST_CASE("resident job wait: save and reload in the middle of the wait continue identically") {
    const auto types = registry();
    Rig r = standard_rig();
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = 5000;
    Sender sender;
    const sim::FigureId id = spawn_one(w, sender);
    until(w, [&] {
        const sim::Figure* f = sim::find_figure(w.figures(), id);
        return f != nullptr && f->resident && f->resident->step == sim::ResidentStep::Wait &&
               f->resident->wait < 600;
    });
    const std::uint32_t left = sim::find_figure(w.figures(), id)->resident->wait;
    REQUIRE(left > 0);
    auto back = sim::World::load(w.save(), types);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    sim::World w2 = std::move(back).value();
    CHECK(sim::find_figure(w2.figures(), id)->resident->wait == left);
    CHECK(w2.state_hash() == w.state_hash());
    for (int i = 0; i < 4000; ++i) {
        w.advance(2);
        w2.advance(2);
    }
    CHECK(w2.state_hash() == w.state_hash());
    CHECK(city0(w2).stock.stock[kFood] == city0(w).stock.stock[kFood]);
    CHECK(city0(w).stock.stock[kFood] < 5000); // the purchase happened after the reload point
}

// ------------------------------------------------------------------ MAP-POP-CLOSE-1 C2 (the Inn)

namespace {

/// The standard world with the stall replaced by an Inn holding `tavern` (payload imported as a
/// scenario record would give it).
sim::World inn_world(const std::shared_ptr<const sim::TypeRegistry>& types, std::uint32_t tavern) {
    Rig r = make_rig();
    r.place(kHouse, 110, 110, fresh_house());
    sim::Inn inn;
    inn.tavern = tavern;
    r.place(kInn, 114, 110, sim::Payload{inn});
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = 5000;
    return w;
}

const sim::Inn& inn_of(const sim::World& w) {
    const sim::Building* b = at(w, 114, 110);
    REQUIRE(b != nullptr);
    const auto* inn = std::get_if<sim::Inn>(&b->payload);
    REQUIRE(inn != nullptr);
    return *inn;
}

} // namespace

TEST_CASE("resident job inn: a walker buys from the tavern stock and the Inn is debited") {
    // House kHouse: E 640, residents 320 (R = 5), need goods 26 Menge 41. demand(26) =
    // trunc(41 * 640 * 256 / 44236) = 151; the Inn offers its tavern stock 200 (> threshold
    // trunc(5 * 41 * 256 / 4096) = 12): amount = min(200, 151, 768) = 151. The bill (D 0x100756e0):
    // t = min(200, 151) = 151, tavern 200 -> 49, taken 0 -> 151, and the INPUT goods 27 is billed
    // 151 (inside 26..40). The city's food store is not touched. The delivery adds 151 * 16.
    const auto types = registry();
    sim::World w = inn_world(types, 200);
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    Sender sender;
    const sim::FigureId id = spawn_one(w, sender);
    until(w, [&] { return ended(w, id); });
    const sim::Figure* f = sim::find_figure(w.figures(), id);
    REQUIRE(f != nullptr);
    CHECK(f->resident->cargo[0] == sim::ResidentCargo{kFood, 151});
    CHECK(inn_of(w).tavern == 49);
    CHECK(inn_of(w).taken == 151);
    CHECK(city0(w).billed[kOther] == 151);
    CHECK(city0(w).billed[kFood] == 0);
    CHECK(city0(w).stock.stock[kFood] == 5000);
    CHECK(house_of(w).stock[0] == 151 * 16);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("resident job inn: an empty tavern never supplies and a small one is clipped") {
    const auto types = registry();
    {
        sim::World w = inn_world(types, 0);
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        until(w, [&] { return ended(w, id); });
        CHECK(sim::find_figure(w.figures(), id)->resident->cargo[0] == sim::ResidentCargo{});
        CHECK(inn_of(w).tavern == 0);
        CHECK(inn_of(w).taken == 0);
        CHECK(house_of(w).stock[0] == 0);
    }
    {
        // tavern 100 < demand 151: amount 100, tavern 0, taken 100, billed 100.
        sim::World w = inn_world(types, 100);
        Sender sender;
        const sim::FigureId id = spawn_one(w, sender);
        until(w, [&] { return ended(w, id); });
        CHECK(sim::find_figure(w.figures(), id)->resident->cargo[0] == sim::ResidentCargo{kFood, 100});
        CHECK(inn_of(w).tavern == 0);
        CHECK(inn_of(w).taken == 100);
        CHECK(city0(w).billed[kOther] == 100);
    }
}

TEST_CASE("resident job inn: the tavern stock survives save and reload") {
    const auto types = registry();
    sim::World w = inn_world(types, 200);
    Sender sender;
    const sim::FigureId id = spawn_one(w, sender);
    until(w, [&] { return ended(w, id); });
    REQUIRE(inn_of(w).tavern == 49);
    auto back = sim::World::load(w.save(), types);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    CHECK(back.value().state_hash() == w.state_hash());
    CHECK(inn_of(back.value()).tavern == 49);
    CHECK(inn_of(back.value()).taken == 151);
}

namespace {
struct InjectDebit {
    std::vector<sim::TavernDebit> debits;
    bool armed = false;
};
void inject_debit_pass(void* raw, const sim::TickContext&, sim::Island&, sim::City&,
                       sim::Effects& fx) {
    auto& in = *static_cast<InjectDebit*>(raw);
    if (in.armed) {
        in.armed = false;
        fx.tavern_debits = in.debits;
    }
}
void run_debits(sim::World& w, InjectDebit& in) {
    sim::Seams& seams = sim::WorldTestAccess::seams(w);
    seams.city_pass[1].name = "resident_test_inject_debit";
    seams.city_pass[1].fn = &inject_debit_pass;
    seams.city_pass[1].ctx = &in;
    in.armed = true;
    for (int k = 0; k < 64 && in.armed; ++k) {
        w.advance(2);
    }
    REQUIRE_FALSE(in.armed);
}
} // namespace

TEST_CASE("resident job inn: the TavernDebit merge clips at the stock and saturates the statistic") {
    const auto types = registry();
    // Clip: a debit of 250 against a stock of 100 leaves 0 (never below), taken rises by the
    // REQUESTED 250 (the resident job only issues what the Inn gave; the merge adds what it is told).
    {
        sim::World w = inn_world(types, 100);
        InjectDebit in;
        in.debits = {sim::TavernDebit{at(w, 114, 110)->id, 250}};
        run_debits(w, in);
        CHECK(inn_of(w).tavern == 0);
        CHECK(inn_of(w).taken == 250);
    }
    // Saturation: taken at max - 10, a debit of 100 -> max (no wrap).
    {
        sim::World w = inn_world(types, 100);
        {
            sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
            const sim::BuildingKey key = is.cache.by_id.at(at(w, 114, 110)->id);
            std::get<sim::Inn>(is.cities[0]->index.lists[key.list].slots[key.slot].value().payload)
                .taken = 0xFFFFFFFFU - 10U;
        }
        InjectDebit in;
        in.debits = {sim::TavernDebit{at(w, 114, 110)->id, 100}};
        run_debits(w, in);
        CHECK(inn_of(w).tavern == 0);
        CHECK(inn_of(w).taken == 0xFFFFFFFFU);
    }
    // Two debits in one buffer apply in order.
    {
        sim::World w = inn_world(types, 100);
        InjectDebit in;
        const sim::BuildingId id = at(w, 114, 110)->id;
        in.debits = {sim::TavernDebit{id, 60}, sim::TavernDebit{id, 60}};
        run_debits(w, in);
        CHECK(inn_of(w).tavern == 0);
        CHECK(inn_of(w).taken == 120);
    }
}

TEST_CASE("resident job inn: a debit that names no Inn changes nothing") {
    const auto types = registry();
    sim::World w = inn_world(types, 100);
    // The house (110, 110) is not an Inn; an unknown id names no building.
    InjectDebit in;
    in.debits = {sim::TavernDebit{at(w, 110, 110)->id, 50},
                 sim::TavernDebit{sim::BuildingId{0x7777000000000001ULL}, 50}};
    const std::uint64_t before_inn_tavern = inn_of(w).tavern;
    run_debits(w, in);
    CHECK(inn_of(w).tavern == before_inn_tavern);
    CHECK(inn_of(w).taken == 0);
    CHECK(house_of(w).stock[0] == 0);
    CHECK(w.check_invariants() == "");
}
