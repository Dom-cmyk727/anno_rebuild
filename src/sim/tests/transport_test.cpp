// The transport body (docs/sim/state.md §5.9 "Transport", task T24; evidence
// docs/re/transport.md): the searcher's disc, the producers' slots, the workshop's
// priority and offer, and the market cart and the workshop carrier end to end on a
// small synthetic island. Every expected value follows from the reading (the D
// addresses in the comments) and is derived by hand in the comment next to it.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "golden.hpp"
#include "island_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

constexpr sim::TypeId kMarket{9201};
constexpr sim::TypeId kWeaver{9202};    // STOFFE from WOLLE, no figures of its own
constexpr sim::TypeId kRoad{9203};      // PRODTYPE 193 with the road flag 0x4000
constexpr sim::TypeId kFarm{9205};      // KORN (goods 13, a raw goods) from GETREIDE, 7 t
constexpr sim::TypeId kCarrierWs{9206}; // STOFFE from WOLLE, one carrier of figure 203
constexpr sim::TypeId kStore{9207};     // a market that sends no cart (max_figures(0) = 0)
constexpr sim::TypeId kFarm1t{9208};    // KORN from GETREIDE with a store of 1 t
constexpr sim::TypeId kSheep{9209};     // WOLLE (goods 9, a raw goods) from GETREIDE, 5 t
constexpr sim::TypeId kObstacle{9210};  // a non-road tile object (prodtype 193 without
                                        // kRoadFlag -> walk_class 0, T29 case (a))

constexpr std::uint32_t kCart = 220;
constexpr std::uint32_t kCarrier = 203;
// The file's speeds (P: 220 0.016, 203 0.012) at 0.2f per tick in 2^-24 tile:
// round(0.016f · 0.2f · 2^24) = 53687, round(0.012f · 0.2f · 2^24) = 40265.
constexpr std::uint32_t kCartSpeed = 53687;
constexpr std::uint32_t kCarrierSpeed = 40265;

std::shared_ptr<const sim::TypeRegistry> registry() {
    sim::TypeRegistry::Builder b;
    // A market of 3 x 3 with one cart and radius 8, its entrance point (1.5, 2.5) =
    // (9/6, 15/6): the centre of its south-middle tile.
    b.add_market(kMarket, kCart, 1, 8)
        .entrances(kMarket, {{9, 15}})
        .add_workshop(kWeaver, "STOFFE", "WOLLE", 6, 11, 5, 0)
        .entrances(kWeaver, {{3, 3}})
        .add_plain(kRoad, 193)
        .flags_word(kRoad, sim::kRoadFlag)
        .add_farm(kFarm, "KORN", "GETREIDE", 37, 80, 7, 0)
        .entrances(kFarm, {{3, 3}})
        .add_market(kStore, kCart, 0, 8)
        .entrances(kStore, {{3, 3}})
        .add_farm(kFarm1t, "KORN", "GETREIDE", 37, 80, 1, 0)
        .entrances(kFarm1t, {{3, 3}})
        .add_farm(kSheep, "WOLLE", "GETREIDE", 37, 80, 5, 0)
        .entrances(kSheep, {{3, 3}})
        .add_workshop(kCarrierWs, "STOFFE", "WOLLE", 6, 11, 5, 1)
        .entrances(kCarrierWs, {{3, 3}})
        .action_radius(kCarrierWs, 16) // P: the weaver 1001 has 16
        .figure_ids(kCarrierWs, {kCarrier})
        .add_plain(kObstacle, 193) // same prodtype as kRoad, but no kRoadFlag -> walk_class 0
        .figure_type(sim::FigureType{kCart, kCartSpeed, 5 * 256, 3, 500})
        .figure_type(sim::FigureType{kCarrier, kCarrierSpeed, 3 * 256, 0, 1000});
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

/// Island 0 at (100, 100), 32 x 32 tiles, all city 0's (player 0), with road tiles.
struct Rig {
    sim::ScenarioSpec spec;
    sim::ScenarioSpec::CitySpec& city() { return spec.islands[0].cities[0]; }
    void road(std::int32_t x, std::int32_t y) { tile_object(x, y, kRoad); }
    void tile_object(std::int32_t x, std::int32_t y, sim::TypeId type) {
        spec.islands[0].objects.push_back(sim::ScenarioSpec::TileObjectSpec{
            static_cast<std::uint32_t>((y - 100) * 32 + (x - 100)), type.v, 0});
    }
    void place(sim::TypeId type, std::int32_t x, std::int32_t y,
               std::optional<sim::Payload> payload = std::nullopt) {
        sim::ScenarioSpec::BuildingSpec b;
        b.type = type;
        b.x = x;
        b.y = y;
        b.payload = std::move(payload);
        city().buildings.push_back(std::move(b));
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

sim::Payload weaver_with(std::uint32_t out) {
    sim::Workshop w = sim::make_workshop(registry()->find(kWeaver)->prod);
    w.out[0] = out;
    return w;
}

sim::Payload farm_with(std::uint32_t out) {
    sim::Farm f;
    f.out = out;
    return f;
}

sim::World create(const Rig& r, const std::shared_ptr<const sim::TypeRegistry>& types) {
    auto w = sim::World::create(r.spec, types, sim::SimConfig{});
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

const sim::Building& at(const sim::World& w, std::int32_t x, std::int32_t y) {
    const sim::Island& is = *w.islands()[0];
    const sim::Building* b = is.building_at(w.config().layout, sim::CityNo{0}, x, y);
    REQUIRE(b != nullptr);
    return *b;
}

const sim::City& city0(const sim::World& w) {
    return *w.islands()[0]->cities[0];
}

/// The live figures of a job.
std::vector<const sim::Figure*> figures_of(const sim::World& w, std::uint32_t job) {
    std::vector<const sim::Figure*> out;
    for (const auto& s : w.figures().list.slots) {
        if (s && s->job == job && !s->expiry_stamp.has_value()) {
            out.push_back(&*s);
        }
    }
    return out;
}

/// Advances one tick at a time (2 ms) until `done` holds, at most `limit` ticks; the
/// tick it held at.
template <class F> std::uint64_t until(sim::World& w, F done, std::uint64_t limit = 20000) {
    for (std::uint64_t i = 0; i < limit; ++i) {
        w.advance(2);
        if (done()) {
            return w.clock().tick();
        }
    }
    FAIL("the condition did not hold within the limit");
    return 0;
}

} // namespace

TEST_CASE("transport disc half-widths follow the midpoint steps of 0x10002a60") {
    // Worked through the loop by hand (edi = r², esi = (r - 1)·r, d = 2r; edi -= 2i + 1;
    // when edi <= esi (unsigned): table[j] = i, j -= 1, d -= 2, esi -= d), then rows
    // r - (r + 1) / 4 .. r widened by one for r <= 3.
    CHECK(sim::disc_half_widths(0) == std::vector<std::uint32_t>{1});
    CHECK(sim::disc_half_widths(1) == std::vector<std::uint32_t>{1, 1});
    CHECK(sim::disc_half_widths(2) == std::vector<std::uint32_t>{2, 2, 2});
    CHECK(sim::disc_half_widths(3) == std::vector<std::uint32_t>{3, 3, 3, 2});
    CHECK(sim::disc_half_widths(4) == std::vector<std::uint32_t>{4, 4, 3, 3, 1});
    const std::vector<std::uint32_t> r8 = sim::disc_half_widths(8);
    REQUIRE(r8.size() == 9);
    CHECK(r8.front() == 8);
    for (std::size_t i = 1; i < r8.size(); ++i) {
        CHECK(r8[i] <= r8[i - 1]); // a disc narrows outwards
    }
}

TEST_CASE("transport area: the square and the disc around the footprint centre") {
    // A 3 x 3 market at (110, 110), radius 8: centre (111, 111) (x + (w - 1) / 2), one
    // centre column and row (w - 1 even); the square 103 .. 119 (2r + 1 = 17 wide).
    const auto types = registry();
    sim::Building b;
    b.type = kMarket;
    b.x = 110;
    b.y = 110;
    const sim::SearchArea a = sim::search_area(b, *types->find(kMarket));
    CHECK(a.cx0 == 111);
    CHECK(a.cx1 == 111);
    CHECK(a.cy0 == 111);
    CHECK(a.cy1 == 111);
    CHECK(a.square() == sim::TileRect{103, 103, 17, 17});
    CHECK(a.contains(111, 103));  // row distance 8, half-width of row 8 >= 0
    CHECK(!a.contains(111, 102)); // row distance 9 > r
    CHECK(!a.contains(103, 103)); // the corner of the square is outside the disc
    // An even footprint (2 x 2 at (110, 110)) has two centre columns and rows.
    sim::BuildingType even = *types->find(kMarket);
    even.width = 2;
    even.height = 2;
    const sim::SearchArea e = sim::search_area(b, even);
    CHECK(e.cx0 == 110);
    CHECK(e.cx1 == 111);
    CHECK(e.square() == sim::TileRect{102, 102, 18, 18});
}

TEST_CASE("transport segment lengths: axis, diagonal by sqrt 2, and the rest") {
    CHECK(sim::segment_length_q24({0, 0}, {256, 0}) == 16777216);
    CHECK(sim::segment_length_q24({0, 0}, {0, -512}) == 2 * 16777216);
    // 1.41421354f · 2^24 = 23726566 per tile on a diagonal (0x100c3130).
    CHECK(sim::segment_length_q24({0, 0}, {256, 256}) == 23726566);
    CHECK(sim::segment_length_q24({0, 0}, {128, -128}) == 11863283);
    // Neither: isqrt(384² + 896²) = isqrt(950272) = 974 (1/256 tile), << 16.
    CHECK(sim::segment_length_q24({0, 0}, {384, 896}) == std::uint64_t{974} << 16);
}

TEST_CASE("transport available and store lines by class") {
    const auto types = registry();
    sim::Building b;
    b.type = kWeaver;
    sim::Workshop w = sim::make_workshop(types->find(kWeaver)->prod);
    w.out[0] = 1000;
    w.reserved[0] = 300;
    b.payload = w;
    // 0x10068880: out - reserved, the goods not read.
    CHECK(sim::available(b, *types->find(kWeaver), 33) == 700);
    CHECK(sim::available(b, *types->find(kWeaver), 0) == 700);
    // A market: the city's stock of the goods (0x10025490).
    sim::Building m;
    m.type = kMarket;
    m.payload = sim::Market{};
    sim::City c;
    c.stock.stock.assign(56, 0);
    c.stock.stock[9] = 4321;
    CHECK(sim::available(m, *types->find(kMarket), 9, &c) == 4321);
    CHECK(sim::available(m, *types->find(kMarket), 9, nullptr) == 0);
    // Inert: nothing.
    sim::Building i;
    i.payload = sim::Inert{};
    CHECK(sim::available(i, *types->find(kMarket), 9, &c) == 0);
}

TEST_CASE("a market cart takes cloth from a weaver on the road and the warehouse gains it") {
    // Market at (110, 110) (3 x 3, entrance at the centre of (111, 112)), a weaver at
    // (111, 115) with 5 t of cloth (STOFFE = goods 33), roads (111, 113) and (111, 114).
    const auto types = registry();
    Rig r = make_rig();
    r.place(kMarket, 110, 110);
    r.place(kWeaver, 111, 115, weaver_with(5 * 256));
    r.road(111, 113);
    r.road(111, 114);
    sim::World w = create(r, types);
    // The market's gate opens and it sends one cart (D 0x1006b6d0, max_figures(0) = 1).
    const std::uint64_t spawn =
        until(w, [&] { return !figures_of(w, sim::kJobMarketCart).empty(); });
    const auto& market = std::get<sim::Market>(at(w, 110, 110).payload);
    CHECK(market.carts_out == 1);
    {
        const sim::Figure& cart = *figures_of(w, sim::kJobMarketCart)[0];
        CHECK(cart.kind == kCart);
        // The spawn at the entrance point (111.5, 112.5) in Q24.8.
        CHECK(cart.pos.x == 111 * 256 + 128);
        CHECK(cart.pos.y == 112 * 256 + 128);
        // Step A ran in the spawn's advance (dt 0): the weaver is the one candidate,
        // its score 1280 · 128 / 1280 = 128 (goods 33 is not raw), and it reserved
        // min(5 t capacity, 1280 available) = 1280 (0x10068460).
        CHECK(cart.transport.step == sim::TransportStep::ToSource);
        CHECK(cart.transport.target == at(w, 111, 115).id);
        CHECK(cart.transport.reserved_88 == 1280);
        CHECK(cart.cargo_goods == 33);
        // The route: the spot, then the centres of (111, 113), (111, 114), (111, 115).
        REQUIRE(cart.transport.route.size() == 4);
        CHECK(cart.transport.route[3].x == 111 * 256 + 128);
        CHECK(cart.transport.route[3].y == 115 * 256 + 128);
    }
    CHECK(std::get<sim::Workshop>(at(w, 111, 115).payload).reserved[0] == 1280);
    // The walk: 3 tiles = 3 · 2^24 = 50331648; ceil(50331648 / 53687) = 938 ticks (one
    // call per tick: the registry has one slot, so phase B visits it every advance).
    // Then the load wait of 500 ticks (LOGIC 1.0 at 0.01 per 0.2-tick unit); the 26758
    // left over at the arrival are less than a tick (OQ-162).
    const std::uint64_t load =
        until(w, [&] { return std::get<sim::Workshop>(at(w, 111, 115).payload).out[0] == 0; });
    CHECK(load - spawn == 938 + 500);
    CHECK(std::get<sim::Workshop>(at(w, 111, 115).payload).reserved[0] == 0);
    {
        const sim::Figure& cart = *figures_of(w, sim::kJobMarketCart)[0];
        CHECK(cart.cargo_88 == 1280);
        CHECK(cart.transport.step == sim::TransportStep::ToHome);
    }
    // Home: the same 3 tiles back to (111, 112), 938 ticks; the city adds 1280 (50 t of
    // room) and the cart ends: its counter goes back, it waits on the expiry list.
    const std::uint64_t unload = until(w, [&] { return city0(w).stock.stock[33] != 0; });
    CHECK(unload - load == 938);
    CHECK(city0(w).stock.stock[33] == 1280);
    CHECK(figures_of(w, sim::kJobMarketCart).empty());
    CHECK(std::get<sim::Market>(at(w, 110, 110).payload).carts_out == 0);
    CHECK(w.figures().expiry.size() == 1);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("a cart finds no producer off the road net and ends at once") {
    // The same weaver without the roads: no path, so no score above 0 (the reached-goal
    // callback is never called); step A fails, step C starts inside the market's
    // footprint and ends in the same call (0x1005d380, 0x10057ae0).
    const auto types = registry();
    Rig r = make_rig();
    r.place(kMarket, 110, 110);
    r.place(kWeaver, 111, 115, weaver_with(5 * 256));
    sim::World w = create(r, types);
    until(w, [&] { return !w.figures().expiry.empty(); });
    CHECK(figures_of(w, sim::kJobMarketCart).empty());
    CHECK(std::get<sim::Workshop>(at(w, 111, 115).payload).reserved[0] == 0);
    CHECK(std::get<sim::Workshop>(at(w, 111, 115).payload).out[0] == 1280);
    CHECK(std::get<sim::Market>(at(w, 110, 110).payload).carts_out == 0);
    CHECK(city0(w).stock.stock[33] == 0);
}

TEST_CASE("F11: the cart takes the producer with the better score, not the nearer") {
    // Weaver A at (111, 115) with 200 cloth, weaver B at (113, 115) with 300, both on the
    // road. "Other goods": score = avail · 128 / 1280: A 20, B 30. A is reached first
    // (cost 15 against 25) and becomes the best (20 > 0), then B (30 > 20). A weaver
    // with 255 is no candidate (the 1 t filter, 0x1005aca0).
    const auto types = registry();
    for (const bool b_has_more : {true, false}) {
        INFO("B has more: " << b_has_more);
        Rig r = make_rig();
        r.place(kMarket, 110, 110);
        r.place(kWeaver, 111, 115, weaver_with(b_has_more ? 200 : 300));
        r.place(kWeaver, 113, 115, weaver_with(b_has_more ? 300 : 255));
        for (const auto& [x, y] : {std::pair{111, 113}, {111, 114}, {112, 114}, {113, 114}}) {
            r.road(x, y);
        }
        sim::World w = create(r, types);
        until(w, [&] { return !figures_of(w, sim::kJobMarketCart).empty(); });
        const sim::Figure& cart = *figures_of(w, sim::kJobMarketCart)[0];
        CHECK(cart.transport.target == at(w, b_has_more ? 113 : 111, 115).id);
        CHECK(cart.transport.reserved_88 == 300);
    }
}

TEST_CASE("F12: a raw goods producer below 75 percent is never chosen") {
    // KORN is goods 13 (raw): q = 1792 · 3 / 4 = 1344; A with 900 scores
    // (900 - 1344) · 128 / 448 = -126 (towards zero), B with 1400 scores 16. Alone, A is
    // never chosen: the best score starts at 0 (0x10003dbf) and -126 is not above it.
    const auto types = registry();
    for (const bool with_b : {true, false}) {
        INFO("with B: " << with_b);
        Rig r = make_rig();
        r.place(kMarket, 110, 110);
        r.place(kFarm, 111, 115, farm_with(900));
        if (with_b) {
            r.place(kFarm, 113, 115, farm_with(1400));
        }
        for (const auto& [x, y] : {std::pair{111, 113}, {111, 114}, {112, 114}, {113, 114}}) {
            r.road(x, y);
        }
        sim::World w = create(r, types);
        until(w, [&] { return w.figures().list.slots[0].has_value(); });
        const auto carts = figures_of(w, sim::kJobMarketCart);
        if (with_b) {
            REQUIRE(carts.size() == 1);
            CHECK(carts[0]->transport.target == at(w, 113, 115).id);
            CHECK(carts[0]->cargo_goods == 13);
        } else {
            CHECK(carts.empty()); // ended at once
            CHECK(std::get<sim::Farm>(at(w, 111, 115).payload).reserved == 0);
        }
    }
}

TEST_CASE("the cart's choice: the 75 percent line, the 1 t filter, the strict best, the room") {
    // Each sub-case: the market at (110, 110), producers on the road (111, 113), (111, 114),
    // (112, 114), (113, 114); the cart's target right after its spawn (step A ran in the
    // spawn's advance), or no cart when step A found nothing (it ended at once).
    const auto types = registry();
    auto run = [&](auto&& setup) {
        Rig r = make_rig();
        r.place(sim::TypeId{kMarket}, 110, 110);
        for (const auto& [x, y] : {std::pair{111, 113}, {111, 114}, {112, 114}, {113, 114}}) {
            r.road(x, y);
        }
        setup(r);
        sim::World w = create(r, types);
        until(w, [&] { return w.figures().list.slots[0].has_value(); });
        return w;
    };
    SUBCASE("a raw goods producer between 1/2 and 3/4 full is never chosen") {
        // KORN 7 t: cap 1792, q = 3 · 1792 / 4 = 1344 (0x1005a9b3); 1200 scores
        // (1200 - 1344) · 128 / 448 = -41, not above the start 0.
        sim::World w = run([](Rig& r) { r.place(kFarm, 111, 115, farm_with(1200)); });
        CHECK(figures_of(w, sim::kJobMarketCart).empty());
        CHECK(std::get<sim::Farm>(at(w, 111, 115).payload).reserved == 0);
    }
    SUBCASE("a full raw goods producer scores 128 and stops the search") {
        // KORN 7 t: (1792 - 1344) · 128 / (1792 - 1344) = 128 >= 128, and 2 · 1792 >= the
        // city's 0 corn: the search stops at the first farm (0x1005aa59), although the
        // farther one holds more (2000 would score (2000 - 1344) · 128 / 448 = 187).
        sim::World w = run([](Rig& r) {
            r.place(kFarm, 111, 115, farm_with(1792));
            r.place(kFarm, 113, 115, farm_with(2000));
        });
        const auto carts = figures_of(w, sim::kJobMarketCart);
        REQUIRE(carts.size() == 1);
        CHECK(carts[0]->transport.target == at(w, 111, 115).id);
        CHECK(carts[0]->transport.reserved_88 == 1280); // min(5 t, 1792)
    }
    SUBCASE("a producer with less than 1 t is no candidate (0x1005aca0)") {
        // KORN 1 t: cap 256, q = 192; 200 would score (200 - 192) · 128 / 64 = 16 in the
        // callback, but the filter keeps only 256 and more.
        sim::World w = run([](Rig& r) { r.place(kFarm1t, 111, 115, farm_with(200)); });
        CHECK(figures_of(w, sim::kJobMarketCart).empty());
        CHECK(std::get<sim::Farm>(at(w, 111, 115).payload).reserved == 0);
    }
    SUBCASE("of two equal scores the first reached stays the best (strict <)") {
        // Both weavers 300: 300 · 128 / 1280 = 30 each; (111, 115) is reached first.
        sim::World w = run([](Rig& r) {
            r.place(kWeaver, 111, 115, weaver_with(300));
            r.place(kWeaver, 113, 115, weaver_with(300));
        });
        const auto carts = figures_of(w, sim::kJobMarketCart);
        REQUIRE(carts.size() == 1);
        CHECK(carts[0]->transport.target == at(w, 111, 115).id);
    }
    SUBCASE("a city without room for the goods takes none (capacity > stock)") {
        // City capacity 1000 and 1000 cloth: 1000 > 1000 fails, the goods are skipped.
        sim::World w = run([](Rig& r) {
            r.place(kWeaver, 111, 115, weaver_with(5 * 256));
            std::vector<std::uint32_t> stock(56, 0);
            stock[33] = 1000;
            r.city().stock = stock;
            r.city().capacity = 1000;
        });
        CHECK(figures_of(w, sim::kJobMarketCart).empty());
        CHECK(std::get<sim::Workshop>(at(w, 111, 115).payload).reserved[0] == 0);
    }
    SUBCASE("the reservation is the cart's capacity when the producer has more") {
        // 2000 cloth: score 2000 · 128 / 1280 = 200 (stop); reserved min(1280, 2000).
        sim::World w = run([](Rig& r) { r.place(kWeaver, 111, 115, weaver_with(2000)); });
        const auto carts = figures_of(w, sim::kJobMarketCart);
        REQUIRE(carts.size() == 1);
        CHECK(carts[0]->transport.reserved_88 == 1280);
        CHECK(std::get<sim::Workshop>(at(w, 111, 115).payload).reserved[0] == 1280);
    }
}

TEST_CASE("the workshop's offer: priority plus avail times 192 over the source's capacity") {
    // 0x100728f0 with the priority 0x80 (no wool in stock): the market (output 0 ALL)
    // offers 0x80 + 5000 · 192 / 1280 = 878 (the market type's capacity 5 t); a sheep farm
    // with 600 wool 0x80 + 600 · 192 / 1280 = 218; one with 255 wool nothing (1 t).
    const auto types = registry();
    Rig r = make_rig();
    r.place(kCarrierWs, 111, 115);
    r.place(kStore, 110, 110);
    r.place(kSheep, 115, 115, farm_with(600));
    r.place(kSheep, 117, 115, farm_with(255));
    std::vector<std::uint32_t> stock(56, 0);
    stock[9] = 5000;
    r.city().stock = stock;
    sim::World w = create(r, types);
    sim::Effects fx;
    sim::SerialWorld serial(w.islands(), w.config().layout, w.figures(), w.players(), fx);
    const sim::Building& ws = at(w, 111, 115);
    const sim::BuildingType& wt = *types->find(kCarrierWs);
    sim::GoodsId g = 0;
    CHECK(sim::workshop_offer(serial, *types, ws, wt, at(w, 110, 110), &city0(w), g) == 878);
    CHECK(g == 9);
    g = 0;
    CHECK(sim::workshop_offer(serial, *types, ws, wt, at(w, 115, 115), &city0(w), g) == 218);
    CHECK(g == 9);
    CHECK(sim::workshop_offer(serial, *types, ws, wt, at(w, 117, 115), &city0(w), g) == -1);
}

TEST_CASE("the carrier stops at a source with 3/4 of its capacity, not at the best") {
    // The carrier workshop at (111, 115) (radius 16, open land): sheep A at (111, 118)
    // with 600 wool (offer 218), sheep B at (111, 121) with 1200 (0x80 + 1200 · 192 / 1280
    // = 308). A is reached first and 600 >= 3 · 768 / 4 = 576 stops the search
    // (0x1005a6a7): A is the source although B offers more; reserved min(768, 600).
    const auto types = registry();
    Rig r = make_rig();
    r.place(kCarrierWs, 111, 115);
    r.place(kSheep, 111, 118, farm_with(600));
    r.place(kSheep, 111, 121, farm_with(1200));
    sim::World w = create(r, types);
    until(w, [&] { return !figures_of(w, sim::kJobWorkshopCarrier).empty(); });
    const sim::Figure& c = *figures_of(w, sim::kJobWorkshopCarrier)[0];
    CHECK(c.transport.target == at(w, 111, 118).id);
    CHECK(c.transport.reserved_88 == 600);
    CHECK(c.cargo_goods == 9);
}

TEST_CASE("a full warehouse clips the cart's load and the rest is lost") {
    // City capacity 1000 with 900 cloth in stock: the cart may come (1000 > 900), loads
    // 1280 and the add (0x1002da50) clips at 1000; the 1180 left are not read again.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kMarket, 110, 110);
    r.place(kWeaver, 111, 115, weaver_with(5 * 256));
    r.road(111, 113);
    r.road(111, 114);
    std::vector<std::uint32_t> stock(56, 0);
    stock[33] = 900;
    r.city().stock = stock;
    r.city().capacity = 1000;
    sim::World w = create(r, types);
    until(w, [&] { return city0(w).stock.stock[33] != 900; });
    CHECK(city0(w).stock.stock[33] == 1000);
    CHECK(std::get<sim::Workshop>(at(w, 111, 115).payload).out[0] == 0);
    CHECK(figures_of(w, sim::kJobMarketCart).empty());
}

TEST_CASE("the search expands its neighbours in the direction-code order") {
    // Roads (111, 113), (111, 114), then two equal branches: west (110, 114), (110, 115),
    // (110, 116) and east (112, 114), (112, 115), (112, 116), to a weaver at (111, 116).
    // At (111, 114) direction 0 (-x) is expanded before 4 (+x) (0x1002ff20 order), so the
    // west branch reaches the weaver first and is the path.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kMarket, 110, 110);
    r.place(kWeaver, 111, 116, weaver_with(5 * 256));
    for (const auto& [x, y] : {std::pair{111, 113},
                               {111, 114},
                               {110, 114},
                               {110, 115},
                               {110, 116},
                               {112, 114},
                               {112, 115},
                               {112, 116}}) {
        r.road(x, y);
    }
    sim::World w = create(r, types);
    until(w, [&] { return !figures_of(w, sim::kJobMarketCart).empty(); });
    const sim::Figure& cart = *figures_of(w, sim::kJobMarketCart)[0];
    // The route: the spot, then (111, 113), (111, 114), (110, 114), (110, 115),
    // (110, 116), (111, 116).
    REQUIRE(cart.transport.route.size() == 7);
    CHECK(cart.transport.route[3].x == 110 * 256 + 128); // (110, 114)
    CHECK(cart.transport.route[5].x == 110 * 256 + 128); // (110, 116)
}

TEST_CASE("a workshop carrier fetches wool from the warehouse over open land") {
    // A workshop (one carrier of figure 203) at (111, 115) with no wool; the market at
    // (110, 110) whose city holds 5000 wool (goods 9). The carrier walks open land
    // (layer 0, OQ-160): no roads needed. Priority: 0 steps of wool -> 0x80; the market's
    // offer 0x80 + 5000 · 192 / 1280 = 878 (0x100728f0); 5000 >= 3/4 of 768 stops the
    // search there. The load takes min(768, 5000) from the city (0x1006b7f0); the home
    // adds it to input 0 (0x100683f0).
    // The market sends no cart here, so the carrier is the registry's one figure and
    // phase B visits it every tick.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kStore, 110, 110);
    r.place(kCarrierWs, 111, 115);
    std::vector<std::uint32_t> stock(56, 0);
    stock[9] = 5000;
    r.city().stock = stock;
    sim::World w = create(r, types);
    const std::uint64_t spawn =
        until(w, [&] { return !figures_of(w, sim::kJobWorkshopCarrier).empty(); });
    {
        const sim::Figure& c = *figures_of(w, sim::kJobWorkshopCarrier)[0];
        CHECK(c.kind == kCarrier);
        CHECK(c.transport.step == sim::TransportStep::ToSource);
        CHECK(c.transport.target == at(w, 110, 110).id);
        CHECK(c.transport.reserved_88 == 0); // a market reserves nothing (0x100a3930)
        CHECK(c.cargo_goods == 9);
        // The decoded first-discovery search uses only orthogonal tile nodes and its
        // table geometry; this synthetic all-open mapper yields six tile edges.
        REQUIRE(c.transport.route.size() == 7);
    }
    // The walk: the decoded search follows tile centres (six orthogonal tile edges,
    // not a straight line): 6 * 2^24 / 40265 ≈ 2500 ticks; then the wait of 2.0 = 1000 ticks.
    // Per-tick rounding adds one tick.
    const std::uint64_t load = until(w, [&] { return city0(w).stock.stock[9] != 5000; });
    CHECK(load - spawn == 2501 + 1000);
    CHECK(city0(w).stock.stock[9] == 5000 - 768);
    // Back: 6 orthogonal tile edges from the market back to the workshop: same as outbound.
    const std::uint64_t unload =
        until(w, [&] { return std::get<sim::Workshop>(at(w, 111, 115).payload).in[0] != 0; });
    CHECK(unload - load == 2501);
    CHECK(std::get<sim::Workshop>(at(w, 111, 115).payload).in[0] == 768);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("the workshop priority counts its carriers and steps of need") {
    // 0x10068500: (stock + reserved + carried of its carriers with the goods) / need:
    // need 11 per step; 300 wool = 27 steps -> 0x80; 500 = 45 -> 0x60; 700 = 63 -> -1;
    // goods that are no input -> -1.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kCarrierWs, 111, 115);
    sim::World w = create(r, types);
    sim::Effects fx;
    sim::SerialWorld serial(w.islands(), w.config().layout, w.figures(), w.players(), fx);
    auto with_in = [&](std::uint32_t wool) {
        sim::Building b = at(w, 111, 115);
        std::get<sim::Workshop>(b.payload).in[0] = wool;
        return sim::workshop_priority(serial, b, *types->find(kCarrierWs), 9);
    };
    CHECK(with_in(300) == 0x80);
    CHECK(with_in(329) == 0x80); // 29 steps
    CHECK(with_in(330) == 0x60); // 30 steps
    CHECK(with_in(500) == 0x60);
    CHECK(with_in(659) == 0x60); // 59 steps
    CHECK(with_in(660) == -1);   // 60 steps
    CHECK(sim::workshop_priority(serial, at(w, 111, 115), *types->find(kCarrierWs), 33) == -1);
}

TEST_CASE("transport runs are deterministic and survive a save mid-walk") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kMarket, 110, 110);
    r.place(kWeaver, 111, 115, weaver_with(5 * 256));
    r.place(kCarrierWs, 113, 115);
    r.road(111, 113);
    r.road(111, 114);
    std::vector<std::uint32_t> stock(56, 0);
    stock[9] = 5000;
    r.city().stock = stock;
    sim::World a = create(r, types);
    sim::World b = create(r, types);
    for (int i = 0; i < 1200; ++i) {
        a.advance(2);
        b.advance(2);
    }
    CHECK(a.state_hash() == b.state_hash());
    // The executor (World::create's island jobs, the hash preparation) does not change
    // the run: 1, 2 and 8 threads give the serial world's hash.
    for (const unsigned threads : {1U, 2U, 8U}) {
        INFO("threads " << threads);
        island_test::ThreadExecutor ex(threads);
        auto made = sim::World::create(r.spec, types, sim::SimConfig{}, {}, {}, ex);
        REQUIRE(made.ok());
        sim::World d = std::move(made).value();
        for (int i = 0; i < 1200; ++i) {
            d.advance(2);
        }
        d.prepare_hash(ex);
        CHECK(d.state_hash() == a.state_hash());
    }
    // Both figures are on their way: a save and a load continue the same run.
    CHECK(!figures_of(a, sim::kJobMarketCart).empty());
    CHECK(!figures_of(a, sim::kJobWorkshopCarrier).empty());
    auto back = sim::World::load(a.save(), types);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    sim::World c = std::move(back).value();
    CHECK(c.state_hash() == a.state_hash());
    for (int i = 0; i < 3000; ++i) {
        a.advance(10);
        c.advance(10);
    }
    CHECK(c.state_hash() == a.state_hash());
    CHECK(a.check_invariants() == "");
}

namespace {

std::filesystem::path g6_path() {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "g6.log";
}

/// Finds the market's TypeId's search area and runs choose_source on `w` for every
/// market whose job matches, collecting a single call's stats; returns the stats of
/// the one market at `(mx, my)`. Keeps the test cases below readable.
sim::TransportStats stats_of_cart(const sim::World& w,
                                  const std::shared_ptr<const sim::TypeRegistry>& types,
                                  std::int32_t mx, std::int32_t my) {
    sim::Effects fx;
    sim::SerialWorld serial(w.islands(), w.config().layout, w.figures(), w.players(), fx);
    const sim::Building& m = at(w, mx, my);
    const sim::BuildingType* t = types->find(m.type);
    REQUIRE(t != nullptr);
    REQUIRE(!t->prod.figure_ids.empty());
    const sim::FigureType* ft = types->find_figure_type(t->prod.figure_ids[0]);
    REQUIRE(ft != nullptr);
    const sim::Island& is = *w.islands()[0];
    const auto key = is.cache.by_id.at(m.id);
    sim::TransportStats stats;
    static_cast<void>(sim::choose_source(serial, *types, sim::kJobMarketCart, m, *t, key.city,
                                         ft->capacity_88, ft->layer, &stats));
    return stats;
}

} // namespace

TEST_CASE("decoded layer 3 cannot cross open ground to a producer") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kMarket, 110, 110);                       // 3 x 3, footprint (110..112, 110..112)
    r.place(kWeaver, 111, 115, weaver_with(5 * 256)); // the one candidate, unreached
    r.tile_object(111, 113, kObstacle);               // 8-neighbour of (110..112, 112)
    sim::World w = create(r, types);
    const sim::TransportStats s = stats_of_cart(w, types, 110, 110);
    CHECK(s.unreached == 1);
    CHECK(s.found == 0);
    CHECK(s.low_score == 0);
    CHECK(s.no_candidate == 0);
    CHECK(s.incomplete == 0);
    // The old stand-in frontier/diagonal diagnostics are retired by T33.4.
    CHECK(s.frontier.empty());
    CHECK(s.edge == 0);
    CHECK(s.outside_disc == 0);
    CHECK(s.corner == 0);
    // goal_no_road is 1: the weaver's 4-neighbours hold no road. home_no_road is 1:
    // the market's 4-neighbours hold no road either (the only object around is
    // kObstacle, which has walk_class 0, not road).
    CHECK(s.goal_no_road == 1);
    CHECK(s.home_no_road == 1);
}

TEST_CASE("decoded layer 3 road to the disc edge does not reach a disconnected producer") {
    // Market at (110, 110), 3 x 3; the search square is (103..119, 103..119) (radius 8,
    // disc_half_widths(8)). A road run from the home footprint south then east to (119,
    // 113): (111, 113), (112, 113), (113, 113), ..., (119, 113). The far tile (119,
    // 113) is on the grid's east border. A weaver at (111, 118), with 5 t cloth, is a
    // candidate but is not reached: no road connects it to the home. The search ends
    // `unreached`; T33.4 no longer exposes the old stand-in's frontier.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kMarket, 110, 110);
    r.place(kWeaver, 111, 118, weaver_with(5 * 256)); // inside the disc, no road nearby
    for (std::int32_t x = 111; x <= 119; ++x) {
        r.road(x, 113);
    }
    sim::World w = create(r, types);
    const sim::TransportStats s = stats_of_cart(w, types, 110, 110);
    CHECK(s.unreached == 1);
    CHECK(s.found == 0);
    CHECK(s.low_score == 0);
    CHECK(s.incomplete == 0);
    CHECK(s.edge == 0);
    CHECK(s.corner == 0);
    // outside_disc is 0: the weaver's footprint (111, 118) lies inside the disc.
    CHECK(s.outside_disc == 0);
    // home_no_road is 0: (111, 113) is a road 4-neighbour of (111, 112).
    CHECK(s.home_no_road == 0);
    // goal_no_road is 1: the weaver's 4-neighbours hold no road.
    CHECK(s.goal_no_road == 1);
}

TEST_CASE("G6 a market cart and a workshop carrier through 300000 ticks equal golden") {
    // G6 (docs/sim/state.md §14.3, task T24): synthetic::g6_scenario (seed 624) on
    // make_transport_types, 6 000 single-player advances of 100 ms (tick 300 000), a
    // checkpoint before the first and after every 300th advance, recorded as
    // fixtures/synthetic/sim/g6.log.
    auto made = syn::make_transport_types();
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string() : made.error()));
    const syn::SyntheticTypes t = made.value();
    auto created = sim::World::create(syn::g6_scenario(t), t.registry, sim::SimConfig{});
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World w = std::move(created).value();
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    rec.checkpoint();
    sim::StateHasher checkpoints;
    std::optional<sim::BuildingId> first_target, second_target;
    std::uint32_t other_draws = 0;
    for (int i = 1; i <= 6000; ++i) {
        rec.advance(100);
        for (const sim::DrawRecord& r : w.last_output().draws) {
            // The world's own draws: the 300 s event (once per 150 000 ticks for the one
            // island) and the 7.5 minute event's gate (once per 225 000 ticks).
            other_draws += r.site != sim::DrawSite::Event300 &&
                                   r.site != sim::DrawSite::Event750Gate &&
                                   r.site != sim::DrawSite::Event750Island
                               ? r.count
                               : 0U;
        }
        for (const auto& s : w.figures().list.slots) {
            if (s && s->job == sim::kJobMarketCart && s->transport.target.valid()) {
                if (!first_target) {
                    first_target = s->transport.target;
                } else if (!second_target && s->transport.target != *first_target) {
                    second_target = s->transport.target;
                }
            }
        }
        if (i % 300 == 0) {
            rec.checkpoint();
            checkpoints.u64(w.state_hash());
            REQUIRE(w.check_invariants() == "");
        }
    }
    REQUIRE(w.clock().tick() == 300000);
    CHECK(other_draws == 0); // the transport draws nothing
    // The first cart takes the weaver's cloth: reached at cost 15 with the score
    // 1280 · 128 / 1280 = 128 (not a raw goods: the early stop, 0x1005aa83), before the
    // farm (cost 30). The next takes the farm's corn: KORN is raw (13), q = 960, score
    // (1280 - 960) · 128 / 320 = 128, and 2 · 1280 >= the city's 0 corn: the stop again.
    const sim::Island& is = *w.islands()[0];
    const sim::Building* weaver = is.building_at(w.config().layout, sim::CityNo{0}, 111, 115);
    const sim::Building* farm = is.building_at(w.config().layout, sim::CityNo{0}, 114, 115);
    const sim::Building* carrier_ws = is.building_at(w.config().layout, sim::CityNo{0}, 106, 114);
    REQUIRE(weaver != nullptr);
    REQUIRE(farm != nullptr);
    REQUIRE(carrier_ws != nullptr);
    REQUIRE(first_target);
    CHECK(*first_target == weaver->id);
    REQUIRE(second_target);
    CHECK(*second_target == farm->id);
    const sim::City& c = *is.cities[0];
    // The cloth and the corn reached the warehouse (5 t each at least).
    CHECK(c.stock.stock[33] >= 5 * 256);
    CHECK(c.stock.stock[13] >= 5 * 256);
    // The carrier fetched wool in loads of 3 t (768) from 30 t (0x1006b7f0, its capacity).
    const std::uint32_t wool_taken = 30 * 256 - c.stock.stock[9];
    CHECK(wool_taken > 0);
    CHECK(wool_taken % 768 == 0);
    // ... which the carrier weaver turned into cloth (F2: 6 per step from 11).
    const auto& cw = std::get<sim::Workshop>(carrier_ws->payload);
    CHECK(cw.activity[0] > 0);
    CHECK(w.check_invariants() == "");

    // The log is byte-identical to the committed one (ANNO_UPDATE_GOLDEN=1 rewrites it).
    const sim::ReplayLog log = rec.log();
    auto written = sim::write_log(log);
    REQUIRE_MESSAGE(written.ok(), (written.ok() ? std::string() : written.error()));
    if (anno::test::update_mode()) {
        std::ofstream out(g6_path(), std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(written.value().data()),
                  static_cast<std::streamsize>(written.value().size()));
        MESSAGE("wrote " << g6_path().string());
    }
    std::ifstream in(g6_path(), std::ios::binary);
    const std::vector<std::uint8_t> committed((std::istreambuf_iterator<char>(in)),
                                              std::istreambuf_iterator<char>());
    CHECK_MESSAGE(committed == written.value(), "the recorded G6 log differs from "
                                                    << g6_path().string()
                                                    << " (ANNO_UPDATE_GOLDEN=1 rewrites it)");
    // The committed log replays on a world rebuilt from its initial save: 21 of 21.
    auto read = sim::read_log(committed);
    REQUIRE_MESSAGE(read.ok(), (read.ok() ? std::string() : read.error()));
    auto fresh = sim::World::load(read.value().initial_save, t.registry);
    REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
    sim::World replayed = std::move(fresh).value();
    auto outcome = sim::replay(replayed, read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(outcome.value().checkpoints_total == 21);
    CHECK(outcome.value().checkpoints_matched == 21);
    CHECK(replayed.state_hash() == w.state_hash());

    // Goldens (self-baselined, goldens.txt; the asserts above do not depend on them).
    for (const auto& [name, value] : std::vector<std::pair<std::string, std::uint64_t>>{
             {"t24_g6_checkpoints", checkpoints.value()}, {"t24_g6_end", w.state_hash()}}) {
        std::string line;
        const std::string problem = anno::test::check_golden(name, "T24", value, line);
        if (anno::test::update_mode()) {
            MESSAGE("golden line: " << line);
        }
        CHECK_MESSAGE(problem.empty(), problem);
    }
}
