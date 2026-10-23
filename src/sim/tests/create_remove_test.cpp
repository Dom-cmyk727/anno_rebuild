// Creating and removing buildings, the queries, and the scenario loader's cities
// and buildings (docs/sim/state.md §7.3, §10, §14.2 "create/remove", stage S6).
#include <algorithm>
#include <climits>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/create.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "building_helpers.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using namespace building_test;

namespace {

const sim::Building& at_key(const sim::World& w, const sim::BuildingKey& k) {
    const sim::Island& is = *w.islands()[k.city.island.v];
    return *is.cities[k.city.city.v]->index.lists[k.list].slots[k.slot];
}

sim::BuildingKey create_ok(sim::World& w, std::uint32_t island, std::uint32_t city,
                           const sim::PlacementRecord& rec, std::uint32_t now = 0) {
    auto r = sim::WorldTestAccess::create(w, island, city, rec, now);
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : std::string(sim::reject_name(r.error()))));
    return r.value();
}

/// `last` and the status time of any payload alternative.
std::pair<std::uint16_t, std::uint16_t> last_and_time(const sim::Building& b) {
    return std::visit(
        [](const auto& p) {
            return std::pair<std::uint16_t, std::uint16_t>{p.last, p.status.time};
        },
        b.payload);
}

/// The tiles of the 8 x 8 neighbourhood around (x, y) that hold a building of the
/// city in any list, as "x,y" strings in scan order.
std::vector<std::pair<int, int>> occupied(const sim::World& w, std::uint32_t island,
                                          std::uint32_t city, int x0, int y0, int w_tiles,
                                          int h_tiles) {
    std::vector<std::pair<int, int>> out;
    for (int y = y0; y < y0 + h_tiles; ++y) {
        for (int x = x0; x < x0 + w_tiles; ++x) {
            if (w.building_at(sim::CityRef{sim::IslandId{island}, sim::CityNo{city}}, x, y) !=
                nullptr) {
                out.emplace_back(x, y);
            }
        }
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------- create

TEST_CASE("create stamps last and the status time with the placement tick") {
    // F1: the placement tick in single and multiplayer (the original wrote the
    // stale net-client tick or 0). Only the low 16 bits of the tick are kept.
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    // D: property 0x23 (0x10073380) always writes `last`; the status-time slot behind it
    // (+0x120) exists only in the production classes and in class 12 (DualWorkshop):
    // for the base, house, market and public classes it is `ret 4` (0x10037590), so an
    // Inert building keeps the status time 0 (a hashed field without a counterpart in
    // the original would be a phantom).
    const std::vector<sim::TypeId> with_time = {k.weaver,     k.farm,         k.fur_trapper,
                                                k.small_farm, k.bow_maker,    k.iron_mine,
                                                k.gold_mine,  k.dual_workshop};
    const std::vector<sim::TypeId> without_time = {k.pioneer_house, k.kontor, k.market_stall,
                                                   k.church, k.barracks};
    int x = 101;
    for (const sim::TypeId t : with_time) {
        INFO("type " << t.v);
        const sim::BuildingKey key = create_ok(w, 0, 0, record(t, x, 101), 0x12345U);
        x += 2;
        const auto [last, time] = last_and_time(at_key(w, key));
        CHECK(last == 0x2345);
        CHECK(time == 0x2345);
    }
    x = 101;
    for (const sim::TypeId t : without_time) {
        INFO("type " << t.v);
        const sim::BuildingKey key = create_ok(w, 0, 0, record(t, x, 103), 0x12345U);
        x += 2;
        const auto [last, time] = last_and_time(at_key(w, key));
        CHECK(last == 0x2345);
        CHECK(time == 0);
    }
    // Tick 0 (the loader) leaves them at 0, and 65535 + 1 wraps to 0.
    const auto zero = create_ok(w, 0, 0, record(k.weaver, 101, 105), 0);
    CHECK(last_and_time(at_key(w, zero)) == std::pair<std::uint16_t, std::uint16_t>{0, 0});
    const auto wrap = create_ok(w, 0, 0, record(k.weaver, 103, 105), 0x10000U);
    CHECK(last_and_time(at_key(w, wrap)) == std::pair<std::uint16_t, std::uint16_t>{0, 0});
    const auto max = create_ok(w, 0, 0, record(k.weaver, 105, 105), 0xFFFFFFFFU);
    CHECK(last_and_time(at_key(w, max)) == std::pair<std::uint16_t, std::uint16_t>{0xFFFF, 0xFFFF});
    CHECK(w.check_invariants() == "");
}

TEST_CASE("create builds the payload of the class and files it in the list of its type") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    struct Row {
        sim::TypeId type;
        std::size_t alternative; // Inert 0, Workshop 1, WeaponsWorkshop 2, Farm 3, SmallFarm 4,
                                 // Mine 5, Market 6
        sim::ListId list;
    };
    const std::vector<Row> rows = {
        {k.weaver, 1, 4},    {k.farm, 3, 4},         {k.fur_trapper, 3, 4}, {k.small_farm, 4, 4},
        {k.bow_maker, 2, 4}, {k.iron_mine, 5, 4},    {k.gold_mine, 5, 4},   {k.pioneer_house, 0, 3},
        {k.kontor, 6, 5},    {k.market_stall, 0, 0}, {k.church, 0, 1},      {k.barracks, 0, 2},
    };
    int x = 101;
    for (const Row& row : rows) {
        INFO("type " << row.type.v);
        const sim::BuildingKey key = create_ok(w, 0, 0, record(row.type, x, 101));
        x += 2;
        const sim::Building& b = at_key(w, key);
        CHECK(b.payload.index() == row.alternative);
        CHECK(key.list == row.list);
        CHECK(key.city == sim::CityRef{sim::IslandId{0}, sim::CityNo{0}});
        CHECK(b.type == row.type);
        CHECK(b.rotation == 0);
    }
    // The vectors are sized from the type (the step functions index by them).
    const sim::BuildingKey weaver_key{{sim::IslandId{0}, sim::CityNo{0}}, 4, 0};
    const sim::Building& weaver = at_key(w, weaver_key);
    const auto& ws = std::get<sim::Workshop>(weaver.payload);
    CHECK(ws.out.size() == 1);
    CHECK(ws.in.size() == 1);
    CHECK(ws.activity.size() == 1);
    CHECK(ws.switched_on);
    const auto& bow = std::get<sim::WeaponsWorkshop>(
        at_key(w, sim::BuildingKey{{sim::IslandId{0}, sim::CityNo{0}}, 4, 4}).payload);
    CHECK(bow.line_on.size() == 2);
    CHECK(bow.out.size() == 2);
    CHECK(bow.in.size() == 2);
    CHECK(bow.activity.size() == 2);
    const auto& farm3 = std::get<sim::SmallFarm>(
        at_key(w, sim::BuildingKey{{sim::IslandId{0}, sim::CityNo{0}}, 4, 3}).payload);
    CHECK(farm3.line_on.size() == 2);
    CHECK(farm3.figures_out.size() == 5);
    // The iron mine is shallow without the deep-mine research, the gold mine deep.
    CHECK_FALSE(std::get<sim::Mine>(
                    at_key(w, sim::BuildingKey{{sim::IslandId{0}, sim::CityNo{0}}, 4, 5}).payload)
                    .deep);
    CHECK(std::get<sim::Mine>(
              at_key(w, sim::BuildingKey{{sim::IslandId{0}, sim::CityNo{0}}, 4, 6}).payload)
              .deep);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("create sets the flag bytes: byte0e by class, byte0f from the record") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    sim::PlacementRecord house = record(k.pioneer_house, 101, 101);
    house.byte0f = 7;
    const sim::BuildingKey house_key = create_ok(w, 0, 0, house);
    const sim::Building& h = at_key(w, house_key);
    CHECK(h.byte0e == 0x82); // the house base constructor
    CHECK(h.byte0f == 7);
    // Every other list keeps 0x86.
    int x = 103;
    for (const sim::TypeId t : {k.weaver, k.kontor, k.market_stall, k.church, k.barracks}) {
        INFO("type " << t.v);
        const sim::BuildingKey key = create_ok(w, 0, 0, record(t, x, 103));
        const sim::Building& b = at_key(w, key);
        x += 2;
        CHECK(b.byte0e == 0x86);
        CHECK(b.byte0f == 0);
    }
    // D: only the house base constructor 0x10075c60 (via 0x10077220, kinds 97..101 =
    // the House class) writes 0x82; the numbers 102..128 sit in the house block but
    // get the plain constructor 0x100734c0 and its 0x86. The byte follows the class,
    // not the list (the house counts follow the class too): a layout whose house list
    // is another list does not move the 0x82 ...
    sim::SimConfig cfg;
    cfg.layout.roles.house_list = 5;
    sim::World w2 = make_world(k, kit_scenario(), cfg);
    CHECK(at_key(w2, create_ok(w2, 0, 0, record(k.kontor, 101, 101))).byte0e == 0x86);
    CHECK(at_key(w2, create_ok(w2, 0, 0, record(k.pioneer_house, 103, 101))).byte0e == 0x82);
    // ... and a plain type inside the house block of the default layout stays 0x86.
    const auto plain = create_ok(w, 0, 0, record(k.house_list_plain, 111, 101));
    CHECK(plain.list == 3);
    CHECK(at_key(w, plain).byte0e == 0x86);
}

TEST_CASE("create keeps a given payload and refuses one that does not fit the type") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    sim::Workshop given = sim::make_workshop(k.registry->find(k.weaver)->prod);
    given.in[0] = 12345;
    given.last = 77;
    given.status.time = 88;
    given.carriers_out = 2;
    const sim::Payload payload = given;
    auto r = sim::WorldTestAccess::create(w, 0, 0, record(k.weaver, 101, 101), 0x4321, &payload);
    REQUIRE(r.ok());
    const auto& kept = std::get<sim::Workshop>(at_key(w, r.value()).payload);
    CHECK(kept.in[0] == 12345);
    CHECK(kept.last == 77); // not replaced by the placement tick
    CHECK(kept.status.time == 88);
    CHECK(kept.carriers_out == 2);
    // A payload of the wrong alternative, or with a vector of the wrong size, is
    // refused and nothing changes.
    const std::uint64_t before = w.state_hash();
    const sim::Payload wrong = sim::Farm{};
    CHECK(sim::WorldTestAccess::create(w, 0, 0, record(k.weaver, 103, 101), 0, &wrong).error() ==
          sim::Reject::BadValue);
    sim::Workshop short_in = given;
    short_in.in.clear();
    const sim::Payload sized = short_in;
    CHECK(sim::WorldTestAccess::create(w, 0, 0, record(k.weaver, 103, 101), 0, &sized).error() ==
          sim::Reject::BadValue);
    CHECK(w.state_hash() == before);
    // A mine without a deposit in a given payload still finds one at attach.
    sim::Mine mine;
    mine.deep = false;
    const sim::Payload mp = mine;
    auto m = sim::WorldTestAccess::create(w, 0, 0, record(k.iron_mine, 110, 118), 0, &mp);
    REQUIRE(m.ok());
    CHECK(std::get<sim::Mine>(at_key(w, m.value()).payload).deposit ==
          std::optional<std::size_t>{0});

    // A given mine KEEPS the deposit it names (the loader's payload is stored as given);
    // only a mine without one searches. The shallow search of this island would take
    // deposit 1 (1700 t with the reserve flag leaves 100 t, 1650 t is the larger).
    // Which of the two the original leaves after its class Load, which runs after
    // Create (0x100264a0), is unread (O-58).
    sim::ScenarioSpec spec = kit_scenario();
    using DS = sim::ScenarioSpec::DepositSpec;
    spec.islands[0].deposits = {DS{"EISENERZ", 110, 120, 1700 * sim::kTon, true},
                                DS{"EISENERZ", 112, 120, 1650 * sim::kTon, false}};
    sim::World w3 = make_world(k, spec);
    sim::Mine named;
    named.deposit = 0;
    const sim::Payload named_payload = named;
    auto kept_mine =
        sim::WorldTestAccess::create(w3, 0, 0, record(k.iron_mine, 101, 101), 0, &named_payload);
    REQUIRE(kept_mine.ok());
    CHECK(std::get<sim::Mine>(at_key(w3, kept_mine.value()).payload).deposit ==
          std::optional<std::size_t>{0});
    const auto searched = create_ok(w3, 0, 0, record(k.iron_mine, 110, 118));
    CHECK(std::get<sim::Mine>(at_key(w3, searched).payload).deposit ==
          std::optional<std::size_t>{1});
    // A deposit the island does not have is refused and nothing changes.
    sim::Mine beyond;
    beyond.deposit = 2;
    const sim::Payload beyond_payload = beyond;
    const std::uint64_t before_beyond = w3.state_hash();
    CHECK(sim::WorldTestAccess::create(w3, 0, 0, record(k.iron_mine, 103, 101), 0, &beyond_payload)
              .error() == sim::Reject::BadValue);
    CHECK(w3.state_hash() == before_beyond);
    CHECK(w3.check_invariants() == "");
}

TEST_CASE("a mine finds its deposit at attach") {
    const Kit k = make_kit();
    sim::ScenarioSpec spec = kit_scenario();
    using DS = sim::ScenarioSpec::DepositSpec;
    // Shallow (the iron mine without TIEFE_ERZMINE): 1700 t with the reserve flag
    // leaves 100 t, 1650 t without it leaves 1650 t, so the second one is taken;
    // a deep mine would take the first (1700 > 1650). A GOLD deposit is ignored by
    // the iron mine however large it is.
    spec.islands[0].deposits = {DS{"EISENERZ", 110, 120, 1700 * sim::kTon, true},
                                DS{"EISENERZ", 112, 120, 1650 * sim::kTon, false},
                                DS{"GOLD", 111, 119, 9000 * sim::kTon, false}};
    sim::World w = make_world(k, spec);
    const auto iron = create_ok(w, 0, 0, record(k.iron_mine, 110, 118));
    const auto& im = std::get<sim::Mine>(at_key(w, iron).payload);
    CHECK_FALSE(im.deep);
    CHECK(im.deposit == std::optional<std::size_t>{1});
    // The gold mine is always deep: it takes the larger deposit of GOLD, whichever
    // of the two is shallow-small.
    spec.islands[0].deposits = {DS{"GOLD", 110, 120, 1700 * sim::kTon, true},
                                DS{"GOLD", 112, 120, 1650 * sim::kTon, false},
                                DS{"EISENERZ", 111, 119, 9000 * sim::kTon, false}};
    sim::World w2 = make_world(k, spec);
    const auto gold = create_ok(w2, 0, 0, record(k.gold_mine, 110, 118));
    const auto& gm = std::get<sim::Mine>(at_key(w2, gold).payload);
    CHECK(gm.deep);
    CHECK(gm.deposit == std::optional<std::size_t>{0});
    // The position is set before the search: far from every deposit there is none.
    const auto far = create_ok(w2, 0, 0, record(k.gold_mine, 110, 101));
    CHECK_FALSE(std::get<sim::Mine>(at_key(w2, far).payload).deposit.has_value());
    // The reach is deposit_in_reach (production.hpp): 7 rows beyond the footprint with a
    // half-width of 2 columns is still in reach, 8 rows is not.
    const auto edge = create_ok(w2, 0, 0, record(k.gold_mine, 108, 113));
    CHECK(std::get<sim::Mine>(at_key(w2, edge).payload).deposit == std::optional<std::size_t>{0});
    const auto beyond = create_ok(w2, 0, 0, record(k.gold_mine, 108, 112));
    CHECK_FALSE(std::get<sim::Mine>(at_key(w2, beyond).payload).deposit.has_value());
    CHECK(w.check_invariants() == "");
    CHECK(w2.check_invariants() == "");
}

TEST_CASE("rotation swaps the footprint and an odd rotation turns it") {
    const Kit k = make_kit();
    for (std::uint8_t rot = 0; rot < 4; ++rot) {
        INFO("rotation " << int{rot});
        sim::World w = make_world(k, kit_scenario());
        const auto key = create_ok(w, 0, 0, record(k.big_house, 110, 140, rot));
        const bool odd = (rot & 1U) != 0;
        // 3 x 2 tiles, or 2 x 3 turned.
        std::vector<std::pair<int, int>> want;
        for (int y = 140; y < 140 + (odd ? 3 : 2); ++y) {
            for (int x = 110; x < 110 + (odd ? 2 : 3); ++x) {
                want.emplace_back(x, y);
            }
        }
        CHECK(occupied(w, 0, 0, 106, 136, 10, 10) == want);
        // The objects of the tiles are the type id, and nowhere else around it.
        const sim::TileLayer& t = w.islands()[0]->tiles;
        for (int y = 136; y < 146; ++y) {
            for (int x = 106; x < 116; ++x) {
                const bool in =
                    std::find(want.begin(), want.end(), std::pair<int, int>{x, y}) != want.end();
                CHECK(t.object(t.index_of(x, y)) == (in ? k.big_house.v : 0U));
            }
        }
        CHECK(at_key(w, key).rotation == rot);
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("a footprint taller than 8 rows is found everywhere") {
    // F3: the original's tile lookup assumed footprints of at most 8 rows.
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    const auto key = create_ok(w, 0, 0, record(k.tall_house, 120, 130));
    const sim::CityRef city{sim::IslandId{0}, sim::CityNo{0}};
    for (int dy = 0; dy < 12; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            INFO("dx " << dx << " dy " << dy);
            const sim::Building* b = w.building_at(city, 120 + dx, 130 + dy);
            REQUIRE(b != nullptr);
            CHECK(b->id == at_key(w, key).id);
        }
    }
    CHECK(w.building_at(city, 120, 142) == nullptr);
    CHECK(w.building_at(city, 122, 130) == nullptr);
    CHECK(w.building_at(city, 120, 129) == nullptr);
    // Turned, it is 12 wide and 2 tall.
    sim::World turned = make_world(k, kit_scenario());
    create_ok(turned, 0, 0, record(k.tall_house, 120, 130, 1));
    CHECK(turned.building_at(city, 131, 131) != nullptr);
    CHECK(turned.building_at(city, 131, 132) == nullptr);
    CHECK(turned.building_at(city, 132, 130) == nullptr);
}

TEST_CASE("a creation that is refused changes nothing") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    create_ok(w, 0, 0, record(k.weaver, 105, 105));
    const std::uint64_t before = w.state_hash();
    const std::uint32_t serial = w.islands()[0]->next_serial;
    auto refuse = [&](const sim::PlacementRecord& rec, sim::Reject want) {
        INFO("type " << rec.type.v << " at " << rec.x << "," << rec.y << " rotation "
                     << int{rec.rotation});
        auto r = sim::WorldTestAccess::create(w, 0, 0, rec);
        REQUIRE_FALSE(r.ok());
        CHECK(sim::reject_name(r.error()) == sim::reject_name(want));
        CHECK(w.state_hash() == before);
        CHECK(w.islands()[0]->next_serial == serial);
        CHECK(w.check_invariants() == "");
    };
    refuse(record(sim::TypeId{424242}, 110, 110), sim::Reject::UnknownType);
    refuse(record(sim::TypeId{0}, 110, 110), sim::Reject::UnknownType);
    refuse(record(k.weaver, 110, 110, 4), sim::Reject::BadRotation);
    refuse(record(k.weaver, 110, 110, 255), sim::Reject::BadRotation);
    refuse(record(k.road, 110, 110), sim::Reject::NotABuilding); // PRODTYPE 193: a tile object
    // The footprint must lie on the island: 100..163 in both directions.
    refuse(record(k.weaver, 99, 110), sim::Reject::OutOfIsland);
    refuse(record(k.weaver, 110, 99), sim::Reject::OutOfIsland);
    refuse(record(k.weaver, 164, 110), sim::Reject::OutOfIsland);
    refuse(record(k.weaver, 110, 164), sim::Reject::OutOfIsland);
    refuse(record(k.weaver, INT_MAX, 110), sim::Reject::OutOfIsland);
    refuse(record(k.weaver, 110, INT_MIN), sim::Reject::OutOfIsland);
    refuse(record(k.tall_house, 110, 153), sim::Reject::OutOfIsland); // rows 153..164
    refuse(record(k.tall_house, 163, 110), sim::Reject::OutOfIsland); // columns 163..164
    // The last tile on the island is accepted: 2 x 12 at columns 162..163, rows
    // 152..163 (and turned, 12 x 2 at columns 150..161, rows 162..163).
    CHECK(sim::WorldTestAccess::create(w, 0, 0, record(k.tall_house, 162, 152)).ok());
    CHECK(sim::WorldTestAccess::create(w, 0, 0, record(k.tall_house, 150, 162, 1)).ok());
    CHECK(w.check_invariants() == "");
}

TEST_CASE("a footprint on another building is refused in any list") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    create_ok(w, 0, 0, record(k.big_house, 110, 110)); // 110..112 x 110..111, the house list
    const std::uint64_t before = w.state_hash();
    auto occupied_by = [&](sim::TypeId t, int x, int y, std::uint8_t rot = 0) {
        auto r = sim::WorldTestAccess::create(w, 0, 0, record(t, x, y, rot));
        REQUIRE_FALSE(r.ok());
        CHECK(sim::reject_name(r.error()) == "Occupied");
        CHECK(w.state_hash() == before);
    };
    occupied_by(k.weaver, 110, 110);        // another list, the corner tile
    occupied_by(k.weaver, 112, 111);        // the opposite corner
    occupied_by(k.kontor, 111, 110);        // the market list
    occupied_by(k.pioneer_house, 112, 110); // the same list
    occupied_by(k.tall_house, 111, 100);    // 2 x 12 reaching the house from above
    occupied_by(k.tall_house, 100, 111, 1); // 12 x 2 turned, reaching it from the left
    // Another city of the same island is no exception: footprints are disjoint
    // across the whole island.
    {
        auto r = sim::WorldTestAccess::create(w, 0, 1, record(k.weaver, 111, 111));
        REQUIRE_FALSE(r.ok());
        CHECK(sim::reject_name(r.error()) == "Occupied");
    }
    // The tiles next to it are free, and another island is independent.
    CHECK(sim::WorldTestAccess::create(w, 0, 0, record(k.weaver, 113, 110)).ok());
    CHECK(sim::WorldTestAccess::create(w, 0, 0, record(k.weaver, 109, 111)).ok());
    CHECK(sim::WorldTestAccess::create(w, 0, 0, record(k.weaver, 110, 112)).ok());
    CHECK(sim::WorldTestAccess::create(w, 0, 0, record(k.weaver, 110, 109)).ok());
    CHECK(sim::WorldTestAccess::create(w, 3, 1, record(k.big_house, 310, 205)).ok());
    CHECK(w.check_invariants() == "");
}

TEST_CASE("serials count per island and are never reused, exhaustion is refused") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    const auto a = create_ok(w, 0, 0, record(k.weaver, 101, 101));
    const auto b = create_ok(w, 0, 1, record(k.weaver, 103, 101));
    const auto c = create_ok(w, 3, 1, record(k.weaver, 301, 201));
    CHECK(at_key(w, a).id == sim::make_building_id(sim::IslandId{0}, 1));
    CHECK(at_key(w, b).id == sim::make_building_id(sim::IslandId{0}, 2)); // one counter per island
    CHECK(at_key(w, c).id == sim::make_building_id(sim::IslandId{3}, 1));
    CHECK(at_key(w, a).id.v == 1);
    CHECK(at_key(w, c).id.v == (std::uint64_t{3} << 32) + 1);
    CHECK(w.islands()[0]->next_serial == 3);
    CHECK(w.islands()[3]->next_serial == 2);
    // A removed building's serial is gone for good.
    CHECK(sim::WorldTestAccess::remove(w, a));
    const auto d = create_ok(w, 0, 0, record(k.weaver, 101, 101));
    CHECK(at_key(w, d).id == sim::make_building_id(sim::IslandId{0}, 3));
    CHECK(w.find_building(sim::make_building_id(sim::IslandId{0}, 1)) == nullptr);
    // The last serial but one is usable; 0xffffffff is the refusal (serial 0 is
    // "none", so a counter at 0 is refused too).
    sim::WorldTestAccess::islands(w)[0]->next_serial = 0xfffffffeU;
    const auto e = create_ok(w, 0, 0, record(k.weaver, 105, 101));
    CHECK(sim::serial_of(at_key(w, e).id) == 0xfffffffeU);
    CHECK(w.islands()[0]->next_serial == 0xffffffffU);
    const std::uint64_t before = w.state_hash();
    auto r = sim::WorldTestAccess::create(w, 0, 0, record(k.weaver, 107, 101));
    REQUIRE_FALSE(r.ok());
    CHECK(sim::reject_name(r.error()) == "SerialExhausted");
    CHECK(w.state_hash() == before);
    sim::WorldTestAccess::islands(w)[0]->next_serial = 0;
    auto z = sim::WorldTestAccess::create(w, 0, 0, record(k.weaver, 107, 101));
    REQUIRE_FALSE(z.ok());
    CHECK(sim::reject_name(z.error()) == "SerialExhausted");
}

// ---------------------------------------------------------------- remove

TEST_CASE("removing a turned building clears the turned footprint and nothing else") {
    // big_house is 3 x 2: on an odd rotation it covers 2 x 3 tiles (D 0x100279e6). The
    // remove must use the rotation the building was created with: with the unturned
    // footprint it would leave stale tiles and grid cells on the turned one and wipe
    // the tile of a neighbour on the other.
    const Kit k = make_kit();
    for (const std::uint8_t rot :
         {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3}}) {
        INFO("rotation " << int{rot});
        sim::World w = make_world(k, kit_scenario());
        const sim::BuildingType& house = *k.registry->find(k.big_house);
        const auto [fw, fh] = sim::footprint(house, rot);
        const auto [ow, oh] = sim::footprint(house, static_cast<std::uint8_t>(rot ^ 1U));
        const auto key = create_ok(w, 0, 0, record(k.big_house, 110, 140, rot));
        // A weaver on a tile of the OTHER orientation's footprint that the real one
        // does not cover (x + 2 when the house is turned, y + 2 when it is not).
        const int nx = 110 + (ow > fw ? 2 : 0);
        const int ny = 140 + (oh > fh ? 2 : 0);
        const auto neighbour = create_ok(w, 0, 0, record(k.weaver, nx, ny));
        REQUIRE(sim::WorldTestAccess::remove(w, key));
        for (std::uint32_t dy = 0; dy < fh; ++dy) {
            for (std::uint32_t dx = 0; dx < fw; ++dx) {
                const int x = 110 + static_cast<int>(dx);
                const int y = 140 + static_cast<int>(dy);
                INFO("tile " << x << "," << y);
                CHECK(w.islands()[0]->tiles.object(w.islands()[0]->tile_index(x, y)) == 0);
                CHECK(w.building_at(sim::CityRef{sim::IslandId{0}, sim::CityNo{0}}, x, y) ==
                      nullptr);
            }
        }
        // The neighbour is untouched.
        CHECK(w.islands()[0]->tiles.object(w.islands()[0]->tile_index(nx, ny)) == k.weaver.v);
        CHECK(w.find_building(at_key(w, neighbour).id) != nullptr);
        CHECK(w.building_at(sim::CityRef{sim::IslandId{0}, sim::CityNo{0}}, nx, ny) != nullptr);
        const std::string problem = w.check_invariants();
        CHECK(problem == "");
    }
}

TEST_CASE("remove empties the slot and leaves the hint, the cursor and the length") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    std::vector<sim::BuildingKey> keys;
    for (int i = 0; i < 3; ++i) {
        keys.push_back(create_ok(w, 0, 0, record(k.weaver, 101 + 2 * i, 101)));
    }
    sim::City& city = city_of(w, 0, 0);
    city.index.lists[4].cursor = 2;
    REQUIRE(city.index.lists[4].slots.size() == 10);
    REQUIRE(city.index.lists[4].hint == 2);
    const sim::BuildingId removed = at_key(w, keys[1]).id;
    REQUIRE(sim::WorldTestAccess::remove(w, keys[1]));
    const sim::BuildingList& after = city_of(w, 0, 0).index.lists[4];
    CHECK(after.slots.size() == 10);
    CHECK(after.hint == 2);
    CHECK(after.cursor == 2);
    CHECK_FALSE(after.slots[1].has_value());
    CHECK(after.slots[0].has_value());
    CHECK(after.slots[2].has_value());
    CHECK(after.live() == 2);
    // The id, the footprint and the tile are gone, the others stay.
    CHECK(w.find_building(removed) == nullptr);
    CHECK(w.islands()[0]->cache.by_id.count(removed) == 0);
    const sim::CityRef c0{sim::IslandId{0}, sim::CityNo{0}};
    CHECK(w.building_at(c0, 103, 101) == nullptr);
    CHECK(w.islands()[0]->tiles.object(w.islands()[0]->tile_index(103, 101)) == 0);
    CHECK(w.building_at(c0, 101, 101) != nullptr);
    CHECK(w.islands()[0]->tiles.object(w.islands()[0]->tile_index(101, 101)) == k.weaver.v);
    CHECK(w.check_invariants() == "");
    // A second removal, a stale key and keys that name nothing do nothing.
    const std::uint64_t h = w.state_hash();
    CHECK_FALSE(sim::WorldTestAccess::remove(w, keys[1]));
    sim::BuildingKey bad = keys[0];
    bad.slot = 9;
    CHECK_FALSE(sim::WorldTestAccess::remove(w, bad));
    bad.slot = 100;
    CHECK_FALSE(sim::WorldTestAccess::remove(w, bad));
    bad = keys[0];
    bad.list = 3;
    CHECK_FALSE(sim::WorldTestAccess::remove(w, bad));
    bad.list = 99;
    CHECK_FALSE(sim::WorldTestAccess::remove(w, bad));
    bad = keys[0];
    bad.city.city = sim::CityNo{5};
    CHECK_FALSE(sim::WorldTestAccess::remove(w, bad));
    bad.city.city = sim::CityNo{1}; // a city that exists but holds no such building
    CHECK_FALSE(sim::WorldTestAccess::remove(w, bad));
    bad = keys[0];
    bad.city.island = sim::IslandId{3};
    CHECK_FALSE(sim::WorldTestAccess::remove(w, bad));
    CHECK(w.state_hash() == h);
}

TEST_CASE("a freed slot is reused only when the scan from the hint reaches it") {
    // D1: the scan starts after the slot stored last and wraps; the lowest free slot
    // is the opt-in.
    const Kit k = make_kit();
    for (const bool lowest : {false, true}) {
        INFO("lowest free " << lowest);
        sim::Rules rules;
        if (lowest) {
            rules.slot_policy = sim::SlotPolicy::LowestFree;
        }
        sim::World w = make_world(k, kit_scenario(), {}, rules);
        std::vector<sim::BuildingKey> keys;
        for (int i = 0; i < 4; ++i) {
            keys.push_back(create_ok(w, 0, 0, record(k.weaver, 101 + 2 * i, 101)));
        }
        for (int i = 0; i < 4; ++i) {
            CHECK(keys[static_cast<std::size_t>(i)].slot == static_cast<sim::Slot>(i));
        }
        REQUIRE(sim::WorldTestAccess::remove(w, keys[0]));
        const auto e = create_ok(w, 0, 0, record(k.weaver, 111, 101));
        CHECK(e.slot == (lowest ? 0U : 4U));
        CHECK(city_of(w, 0, 0).index.lists[4].hint == (lowest ? 0 : 4));
        // With the original policy slot 0 is reached only after the scan wraps: fill
        // up to slot 9, then the next creation takes it.
        if (!lowest) {
            for (int i = 0; i < 5; ++i) {
                const auto f = create_ok(w, 0, 0, record(k.weaver, 113 + 2 * i, 101));
                CHECK(f.slot == static_cast<sim::Slot>(5 + i));
            }
            const auto g = create_ok(w, 0, 0, record(k.weaver, 101, 103));
            CHECK(g.slot == 0); // wrapped round to the hole
            CHECK(city_of(w, 0, 0).index.lists[4].slots.size() == 10);
            const auto h = create_ok(w, 0, 0, record(k.weaver, 103, 103));
            CHECK(h.slot == 10); // full: the list grows 10 -> 12, the new slot is the old length
            CHECK(city_of(w, 0, 0).index.lists[4].slots.size() == 12);
        }
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("the first buildings of a list grow it from 1 to 10") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    REQUIRE(city_of(w, 0, 0).index.lists[4].slots.size() == 1);
    CHECK(city_of(w, 0, 0).index.lists[4].hint == -1);
    const auto a = create_ok(w, 0, 0, record(k.weaver, 101, 101));
    CHECK(a.slot == 0);
    CHECK(city_of(w, 0, 0).index.lists[4].slots.size() == 1);
    const auto b = create_ok(w, 0, 0, record(k.farm, 103, 101));
    CHECK(b.slot == 1);
    CHECK(city_of(w, 0, 0).index.lists[4].slots.size() == 10);
    CHECK(city_of(w, 0, 0).index.lists[4].hint == 1);
    // The other lists are untouched.
    for (const sim::ListId l : {0U, 1U, 2U, 3U, 5U}) {
        CHECK(city_of(w, 0, 0).index.lists[l].slots.size() == 1);
        CHECK(city_of(w, 0, 0).index.lists[l].hint == -1);
    }
    // A configured growth is honoured.
    sim::SimConfig cfg;
    cfg.list_min_growth = 4;
    sim::World w4 = make_world(k, kit_scenario(), cfg);
    create_ok(w4, 0, 0, record(k.weaver, 101, 101));
    create_ok(w4, 0, 0, record(k.weaver, 103, 101));
    CHECK(city_of(w4, 0, 0).index.lists[4].slots.size() == 4);
}

TEST_CASE("the derived counters follow creation and removal") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    auto counters = [&](std::uint32_t city) { return city_of(w, 0, city).index.service_counters; };
    auto houses = [&](std::uint32_t city) { return city_of(w, 0, city).index.house_counts; };
    const std::vector<std::uint32_t> zero_service(32, 0);
    const std::vector<std::uint32_t> zero_houses(5, 0);
    CHECK(counters(0) == zero_service);
    CHECK(houses(0) == zero_houses);
    // Service bits 0, 4 and 5 (mask 0x31): one counter each per building.
    const auto c1 = create_ok(w, 0, 0, record(k.church, 101, 101));
    const auto c2 = create_ok(w, 0, 0, record(k.church, 103, 101));
    std::vector<std::uint32_t> want = zero_service;
    want[0] = 2;
    want[4] = 2;
    want[5] = 2;
    CHECK(counters(0) == want);
    CHECK(counters(1) == zero_service); // per city
    // House groups: PRODTYPE 97, 98, 99 are groups 0, 1, 2.
    create_ok(w, 0, 0, record(k.pioneer_house, 101, 105));
    const auto big1 = create_ok(w, 0, 0, record(k.big_house, 101, 107));
    create_ok(w, 0, 0, record(k.big_house, 106, 107));
    const auto t1 = create_ok(w, 0, 0, record(k.tall_house, 101, 120));
    create_ok(w, 0, 0, record(k.tall_house, 105, 120));
    create_ok(w, 0, 0, record(k.tall_house, 109, 120));
    CHECK(houses(0) == std::vector<std::uint32_t>{1, 2, 3, 0, 0});
    // A building without a service mask or a house class changes neither.
    create_ok(w, 0, 0, record(k.weaver, 101, 140));
    create_ok(w, 0, 0, record(k.kontor, 103, 140));
    CHECK(counters(0) == want);
    CHECK(houses(0) == std::vector<std::uint32_t>{1, 2, 3, 0, 0});
    CHECK(w.check_invariants() == "");
    // D: Create (0x10024e18..0x1002500f) and Remove (0x10025810..0x100258b0) raise and
    // lower the service counters in the public block only: a type of another list with
    // a service mask raises none (here PRODTYPE 110, mask 0x2, in the house block, and it
    // is no House either, so no house count), and the last of the 32 counters is
    // reachable (a library with the mask 1 << 31).
    const auto plain = create_ok(w, 0, 0, record(k.house_list_plain, 105, 140));
    CHECK(counters(0) == want);
    CHECK(houses(0) == std::vector<std::uint32_t>{1, 2, 3, 0, 0});
    const auto top = create_ok(w, 0, 0, record(k.library_top, 107, 140));
    std::vector<std::uint32_t> want_top = want;
    want_top[31] = 1;
    CHECK(counters(0) == want_top);
    CHECK(w.check_invariants() == "");
    REQUIRE(sim::WorldTestAccess::remove(w, plain));
    CHECK(counters(0) == want_top);
    REQUIRE(sim::WorldTestAccess::remove(w, top));
    CHECK(counters(0) == want);
    CHECK(w.check_invariants() == "");
    // Removal is the exact inverse.
    REQUIRE(sim::WorldTestAccess::remove(w, c1));
    want = zero_service;
    want[0] = 1;
    want[4] = 1;
    want[5] = 1;
    CHECK(counters(0) == want);
    REQUIRE(sim::WorldTestAccess::remove(w, c2));
    CHECK(counters(0) == zero_service);
    REQUIRE(sim::WorldTestAccess::remove(w, big1));
    REQUIRE(sim::WorldTestAccess::remove(w, t1));
    CHECK(houses(0) == std::vector<std::uint32_t>{1, 1, 2, 0, 0});
    CHECK(w.check_invariants() == "");
    // A smaller house table: group 2 has no counter and nothing breaks.
    sim::SimConfig cfg;
    cfg.house_groups = 2;
    sim::World w2 = make_world(k, kit_scenario(), cfg);
    create_ok(w2, 0, 0, record(k.tall_house, 101, 120));
    create_ok(w2, 0, 0, record(k.big_house, 101, 107));
    CHECK(city_of(w2, 0, 0).index.house_counts == std::vector<std::uint32_t>{0, 1});
    CHECK(w2.check_invariants() == "");
    // The public list is data: with the house list as the public list the plain type of
    // the house block counts (bit 1) and the church (list 1) no longer does.
    sim::SimConfig cfg3;
    cfg3.layout.roles.public_list = 3;
    sim::World w3 = make_world(k, kit_scenario(), cfg3);
    create_ok(w3, 0, 0, record(k.house_list_plain, 101, 101));
    create_ok(w3, 0, 0, record(k.church, 103, 101));
    std::vector<std::uint32_t> only_bit1(32, 0);
    only_bit1[1] = 1;
    CHECK(city_of(w3, 0, 0).index.service_counters == only_bit1);
    CHECK(w3.check_invariants() == "");
}

TEST_CASE("removing a building of the market list sets defeat_pending of the owner") {
    // D: player byte +0x295, set by Remove for the market/Kontor group (0x100255c0).
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    const auto kontor = create_ok(w, 0, 0, record(k.kontor, 101, 101));
    const auto house = create_ok(w, 0, 0, record(k.pioneer_house, 103, 101));
    const auto church = create_ok(w, 0, 0, record(k.church, 105, 101));
    const auto stall = create_ok(w, 0, 0, record(k.market_stall, 107, 101));
    const auto foreign = create_ok(w, 3, 1, record(k.kontor, 301, 201));
    auto flag = [&](std::uint32_t player) { return w.players().slots[player]->defeat_pending; };
    CHECK_FALSE(flag(0));
    REQUIRE(sim::WorldTestAccess::remove(w, house));
    REQUIRE(sim::WorldTestAccess::remove(w, church));
    REQUIRE(sim::WorldTestAccess::remove(w, stall)); // the stall list is not the market list
    CHECK_FALSE(flag(0));
    REQUIRE(sim::WorldTestAccess::remove(w, kontor));
    CHECK(flag(0));
    CHECK_FALSE(flag(1));
    // The owner of the city is the one flagged: city 1 of island 3 is player 1's.
    REQUIRE(sim::WorldTestAccess::remove(w, foreign));
    CHECK(flag(1));
    // The roles are data: with the house list as the market list the house counts.
    sim::SimConfig cfg;
    cfg.layout.roles.market_list = 3;
    sim::World w2 = make_world(k, kit_scenario(), cfg);
    const auto h2 = create_ok(w2, 0, 0, record(k.pioneer_house, 101, 101));
    const auto k2 = create_ok(w2, 0, 0, record(k.kontor, 103, 101));
    REQUIRE(sim::WorldTestAccess::remove(w2, k2));
    CHECK_FALSE(w2.players().slots[0]->defeat_pending);
    REQUIRE(sim::WorldTestAccess::remove(w2, h2));
    CHECK(w2.players().slots[0]->defeat_pending);
}

// ---------------------------------------------------------------- queries

TEST_CASE("rect query canonical order and list masks") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    // Created in a scrambled order so that neither the creation order nor the id
    // order is the canonical one: (list in rect order {1, 3, 4, 2, 0, 5}, y, x, id).
    const auto kontor = create_ok(w, 0, 0, record(k.kontor, 105, 100));   // list 5
    const auto farm = create_ok(w, 0, 0, record(k.farm, 103, 102));       // list 4, y 102
    const auto weaver_b = create_ok(w, 0, 0, record(k.weaver, 103, 103)); // list 4, y 103, x 103
    const auto house = create_ok(w, 0, 0, record(k.pioneer_house, 101, 105)); // list 3
    const auto weaver_a = create_ok(w, 0, 0, record(k.weaver, 102, 103));    // list 4, y 103, x 102
    const auto church = create_ok(w, 0, 0, record(k.church, 108, 108));      // list 1
    const auto barracks = create_ok(w, 0, 0, record(k.barracks, 100, 108));  // list 2
    const auto stall = create_ok(w, 0, 0, record(k.market_stall, 107, 104)); // list 0
    const auto other_city = create_ok(w, 0, 1, record(k.weaver, 104, 104));  // another city
    const auto far = create_ok(w, 0, 0, record(k.weaver, 150, 150));         // outside the rect
    const sim::CityRef c0{sim::IslandId{0}, sim::CityNo{0}};
    auto id = [&](const sim::BuildingKey& key) { return at_key(w, key).id; };
    std::vector<sim::BuildingId> out{sim::BuildingId{12345}}; // cleared by the call
    w.rect_query(c0, sim::TileRect{100, 100, 10, 10}, ~0U, out);
    const std::vector<sim::BuildingId> want = {id(church),   id(house),    id(farm),  id(weaver_a),
                                               id(weaver_b), id(barracks), id(stall), id(kontor)};
    CHECK(out == want);
    // The other city's building is not in the result of city 0, and city 1 sees only its own.
    std::vector<sim::BuildingId> other;
    w.rect_query(sim::CityRef{sim::IslandId{0}, sim::CityNo{1}}, sim::TileRect{100, 100, 10, 10},
                 ~0U, other);
    CHECK(other == std::vector<sim::BuildingId>{id(other_city)});
    // List masks: production 0x08, market 0x02, public 0x01, house 0x04, military
    // 0x10, stall 0x20; a mask of 0 or of no list returns nothing.
    auto query = [&](std::uint32_t mask) {
        std::vector<sim::BuildingId> r;
        w.rect_query(c0, sim::TileRect{100, 100, 10, 10}, mask, r);
        return r;
    };
    CHECK(query(0x08) == std::vector<sim::BuildingId>{id(farm), id(weaver_a), id(weaver_b)});
    CHECK(query(0x02) == std::vector<sim::BuildingId>{id(kontor)});
    CHECK(query(0x01) == std::vector<sim::BuildingId>{id(church)});
    CHECK(query(0x04) == std::vector<sim::BuildingId>{id(house)});
    CHECK(query(0x10) == std::vector<sim::BuildingId>{id(barracks)});
    CHECK(query(0x20) == std::vector<sim::BuildingId>{id(stall)});
    CHECK(query(0x09) ==
          std::vector<sim::BuildingId>{id(church), id(farm), id(weaver_a), id(weaver_b)});
    CHECK(query(0).empty());
    CHECK(query(0x40).empty());
    // The rectangle decides: a building counts when its footprint shares a tile.
    auto in = [&](std::int32_t x, std::int32_t y, std::uint32_t wd, std::uint32_t ht) {
        std::vector<sim::BuildingId> r;
        w.rect_query(c0, sim::TileRect{x, y, wd, ht}, ~0U, r);
        return r;
    };
    CHECK(in(103, 103, 1, 1) == std::vector<sim::BuildingId>{id(weaver_b)});
    CHECK(in(104, 103, 1, 1).empty());
    CHECK(in(102, 103, 2, 1) == std::vector<sim::BuildingId>{id(weaver_a), id(weaver_b)});
    CHECK(in(150, 150, 1, 1) == std::vector<sim::BuildingId>{id(far)});
    CHECK(in(0, 0, 100, 100).empty()); // touches the island's corner at most
    CHECK(in(0, 0, 101, 101).empty()); // 100,100 is a free tile
    CHECK(in(90, 90, 12, 12).empty());
    CHECK(in(105, 100, 1, 1) == std::vector<sim::BuildingId>{id(kontor)});
    CHECK(in(106, 100, 5, 5) == std::vector<sim::BuildingId>{id(stall)});
    // A rectangle larger than the island and one with a zero side are defined.
    CHECK(in(-1000, -1000, 5000, 5000).size() == 9);
    CHECK(in(100, 100, 0, 10).empty());
    CHECK(in(100, 100, 10, 0).empty());
    // A building with a big footprint appears once however many of its tiles are in
    // the rectangle.
    const auto tall = create_ok(w, 0, 0, record(k.tall_house, 120, 120));
    CHECK(in(119, 119, 5, 20) == std::vector<sim::BuildingId>{id(tall)});
    CHECK(in(120, 125, 1, 1) == std::vector<sim::BuildingId>{id(tall)});
    // An island, a city or a rectangle that does not exist give an empty result.
    std::vector<sim::BuildingId> none{sim::BuildingId{1}};
    w.rect_query(sim::CityRef{sim::IslandId{1}, sim::CityNo{0}}, sim::TileRect{100, 100, 10, 10},
                 ~0U, none);
    CHECK(none.empty());
    w.rect_query(sim::CityRef{sim::IslandId{900}, sim::CityNo{0}}, sim::TileRect{0, 0, 10, 10}, ~0U,
                 none);
    CHECK(none.empty());
    w.rect_query(sim::CityRef{sim::IslandId{0}, sim::CityNo{9}}, sim::TileRect{100, 100, 10, 10},
                 ~0U, none);
    CHECK(none.empty());
}

TEST_CASE("building_at and find_building honour the city, the list mask and the id") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    const auto a = create_ok(w, 0, 0, record(k.big_house, 110, 110));
    const auto b = create_ok(w, 0, 1, record(k.weaver, 120, 110));
    const sim::CityRef c0{sim::IslandId{0}, sim::CityNo{0}};
    const sim::CityRef c1{sim::IslandId{0}, sim::CityNo{1}};
    const sim::BuildingId ida = at_key(w, a).id;
    const sim::BuildingId idb = at_key(w, b).id;
    // Every tile of the footprint, only for its own city.
    for (int y = 110; y < 112; ++y) {
        for (int x = 110; x < 113; ++x) {
            INFO("tile " << x << "," << y);
            REQUIRE(w.building_at(c0, x, y) != nullptr);
            CHECK(w.building_at(c0, x, y)->id == ida);
            CHECK(w.building_at(c1, x, y) == nullptr);
        }
    }
    CHECK(w.building_at(c1, 120, 110)->id == idb);
    CHECK(w.building_at(c0, 120, 110) == nullptr);
    // The list mask: the house list is 0x04, the production list 0x08.
    CHECK(w.building_at(c0, 110, 110, 0x04) != nullptr);
    CHECK(w.building_at(c0, 110, 110, 0x08) == nullptr);
    CHECK(w.building_at(c0, 110, 110, ~0x04U) == nullptr);
    CHECK(w.building_at(c1, 120, 110, 0x08) != nullptr);
    CHECK(w.building_at(c1, 120, 110, 0x04) == nullptr);
    // Outside the island, the world, and unknown islands and cities.
    CHECK(w.building_at(c0, 99, 110) == nullptr);
    CHECK(w.building_at(c0, INT_MIN, INT_MAX) == nullptr);
    CHECK(w.building_at(sim::CityRef{sim::IslandId{2}, sim::CityNo{0}}, 110, 110) == nullptr);
    CHECK(w.building_at(sim::CityRef{sim::IslandId{}, sim::CityNo{0}}, 110, 110) == nullptr);
    CHECK(w.building_at(sim::CityRef{sim::IslandId{0}, sim::CityNo{200}}, 110, 110) == nullptr);
    // find_building by id: the island is in the id.
    CHECK(w.find_building(ida)->type == k.big_house);
    CHECK(w.find_building(idb)->type == k.weaver);
    CHECK(w.find_building(sim::make_building_id(sim::IslandId{0}, 99)) == nullptr);
    CHECK(w.find_building(sim::make_building_id(sim::IslandId{3}, 1)) == nullptr);
    CHECK(w.find_building(sim::make_building_id(sim::IslandId{1}, 1)) == nullptr);
    CHECK(w.find_building(sim::make_building_id(sim::IslandId{5000}, 1)) == nullptr);
    CHECK(w.find_building(sim::BuildingId{}) == nullptr);
    // City::find_building answers only for its own buildings.
    const sim::Island& is = *w.islands()[0];
    CHECK(is.cities[0]->find_building(is, ida) != nullptr);
    CHECK(is.cities[0]->find_building(is, idb) == nullptr);
    CHECK(is.cities[1]->find_building(is, idb) != nullptr);
    // A removed building is gone for every query.
    REQUIRE(sim::WorldTestAccess::remove(w, a));
    CHECK(w.find_building(ida) == nullptr);
    CHECK(w.building_at(c0, 110, 110) == nullptr);
}

TEST_CASE("building_at searches the lists in the layout's lookup order") {
    // The footprints of one island are disjoint (A13), so the order of the search is
    // observable only in a state that check_invariants refuses; the test makes one by
    // hand: the grid of the church's list also claims the tile of a weaver (D:
    // FindByTile 0x10023c80 visits {4, 1, 5, 3, 0, 2}; the rect order {1, 3, 4, 2, 0, 5}
    // puts the church first, so a search by the wrong order would return it).
    const Kit k = make_kit();
    for (const bool church_first : {false, true}) {
        INFO("church first " << church_first);
        sim::SimConfig cfg;
        if (church_first) {
            cfg.layout.lookup_order = {1, 4, 5, 3, 0, 2};
        }
        sim::World w = make_world(k, kit_scenario(), cfg);
        const auto weaver = create_ok(w, 0, 0, record(k.weaver, 101, 101));
        const auto church = create_ok(w, 0, 0, record(k.church, 103, 101));
        sim::Island& is = isl(w, 0);
        REQUIRE(church.list < is.cache.grids.size());
        is.cache.grids[church.list][is.tile_index(101, 101)] = sim::serial_of(at_key(w, church).id);
        CHECK(w.check_invariants() != ""); // the injected overlap is not a legal state
        const sim::Building* hit =
            w.building_at(sim::CityRef{sim::IslandId{0}, sim::CityNo{0}}, 101, 101);
        REQUIRE(hit != nullptr);
        CHECK(hit->id == at_key(w, church_first ? church : weaver).id);
        // A list mask that excludes the first list finds the other.
        const sim::Building* only_weaver =
            w.building_at(sim::CityRef{sim::IslandId{0}, sim::CityNo{0}}, 101, 101, 0x08);
        REQUIRE(only_weaver != nullptr);
        CHECK(only_weaver->id == at_key(w, weaver).id);
    }
}

TEST_CASE("island and city query accessors equal the world members") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    create_ok(w, 0, 0, record(k.big_house, 110, 110));
    create_ok(w, 0, 0, record(k.weaver, 114, 110));
    create_ok(w, 0, 1, record(k.farm, 120, 110));
    create_ok(w, 0, 0, record(k.tall_house, 130, 120));
    const sim::Island& is = *w.islands()[0];
    const sim::ListLayout& layout = w.config().layout;
    for (std::uint32_t no = 0; no < 2; ++no) {
        const sim::CityRef ref{sim::IslandId{0}, sim::CityNo{no}};
        const sim::City& city = *is.cities[no];
        for (int y = 108; y < 135; ++y) {
            for (int x = 108; x < 135; ++x) {
                for (const std::uint32_t mask : {~0U, 0x08U, 0x04U}) {
                    const sim::Building* from_world = w.building_at(ref, x, y, mask);
                    CHECK(is.building_at(layout, sim::CityNo{no}, x, y, mask) == from_world);
                    CHECK(city.building_at(is, layout, x, y, mask) == from_world);
                }
            }
        }
        for (const std::uint32_t mask : {~0U, 0x08U, 0x04U, 0x0CU}) {
            std::vector<sim::BuildingId> a, b, c;
            const sim::TileRect rect{105, 105, 30, 30};
            w.rect_query(ref, rect, mask, a);
            is.rect_query(layout, sim::CityNo{no}, rect, mask, b);
            city.rect_query(is, layout, rect, mask, c);
            CHECK(a == b);
            CHECK(a == c);
            if (mask == ~0U) {
                CHECK_FALSE(a.empty());
            }
        }
    }
    CHECK(is.find_building(
              w.building_at(sim::CityRef{sim::IslandId{0}, sim::CityNo{0}}, 110, 110)->id) ==
          w.building_at(sim::CityRef{sim::IslandId{0}, sim::CityNo{0}}, 110, 110));
}

// ---------------------------------------------------------------- the loader

TEST_CASE("world create places the cities and buildings of a scenario in file order") {
    auto types = syn::make_synthetic_types();
    REQUIRE(types.ok());
    auto created = sim::World::create(syn::g1_scenario(types.value()), types.value().registry);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    const sim::World& w = created.value();
    const sim::Island& is = *w.islands()[0];
    REQUIRE(is.cities.size() == 16);
    for (std::size_t c = 1; c < 16; ++c) {
        CHECK_FALSE(is.cities[c].has_value());
    }
    REQUIRE(is.cities[0].has_value());
    const sim::City& city = *is.cities[0];
    CHECK(city.number.v == 0);
    CHECK(city.owner.v == 0);
    CHECK(city.tick_stamp == 0);
    CHECK(city.index.stamp == 0);
    // Production list: the weaver in slot 0, the farm in slot 1 (the list grew from 1
    // to 10 with the second), hint 1, cursor 0.
    const sim::BuildingList& prod = city.index.lists[4];
    CHECK(prod.slots.size() == 10);
    CHECK(prod.hint == 1);
    CHECK(prod.cursor == 0);
    REQUIRE(prod.slots[0].has_value());
    REQUIRE(prod.slots[1].has_value());
    CHECK(prod.live() == 2);
    CHECK(prod.slots[0]->id == sim::make_building_id(sim::IslandId{0}, 1));
    CHECK(prod.slots[1]->id == sim::make_building_id(sim::IslandId{0}, 2));
    CHECK(prod.slots[0]->type == types.value().weaver);
    CHECK(prod.slots[1]->type == types.value().farm);
    CHECK(prod.slots[0]->x == 110);
    CHECK(prod.slots[0]->y == 110);
    CHECK(prod.slots[1]->x == 120);
    // The given payloads were kept: 5 t of wool, 5 t of raw material, created at tick 0.
    const auto& weaver = std::get<sim::Workshop>(prod.slots[0]->payload);
    CHECK(weaver.in == std::vector<std::uint32_t>{5 * sim::kTon});
    CHECK(weaver.out == std::vector<std::uint32_t>{0});
    CHECK(weaver.last == 0);
    CHECK(std::get<sim::Farm>(prod.slots[1]->payload).raw == 5 * sim::kTon);
    CHECK(is.next_serial == 3);
    // The other lists are fresh; the footprint tiles carry the type ids.
    for (const sim::ListId l : {0U, 1U, 2U, 3U, 5U}) {
        CHECK(city.index.lists[l].slots.size() == 1);
        CHECK(city.index.lists[l].live() == 0);
    }
    CHECK(is.tiles.object(is.tile_index(110, 110)) == types.value().weaver.v);
    CHECK(is.tiles.object(is.tile_index(120, 110)) == types.value().farm.v);
    CHECK(w.check_invariants() == "");
    // Nothing was charged: the player still has the money of the scenario.
    CHECK(w.players().slots[0]->money == 10000);
    CHECK(w.clock().ms == 0);
}

TEST_CASE("world create forwards the rotation and byte0f of a scenario building") {
    const Kit k = make_kit();
    sim::ScenarioSpec spec = kit_scenario();
    auto turned = place(k.big_house, 110, 140, 1); // 3 x 2 turned to 2 x 3
    turned.byte0f = 5;
    auto flagged = place(k.weaver, 101, 101);
    flagged.byte0f = 0x7f;
    spec.islands[0].cities[0].buildings = {turned, flagged};
    sim::World w = make_world(k, spec);
    const sim::City& city = city_of(w, 0, 0);
    const sim::Building& house = *city.index.lists[3].slots[0];
    CHECK(house.rotation == 1);
    CHECK(house.byte0f == 5);
    CHECK(house.byte0e == 0x82);
    const sim::Building& weaver = *city.index.lists[4].slots[0];
    CHECK(weaver.rotation == 0);
    CHECK(weaver.byte0f == 0x7f);
    // The footprint is the turned one.
    const sim::CityRef c0{sim::IslandId{0}, sim::CityNo{0}};
    CHECK(w.building_at(c0, 111, 142) != nullptr);
    CHECK(w.building_at(c0, 110, 140) != nullptr);
    CHECK(w.building_at(c0, 112, 140) == nullptr);
    CHECK(w.building_at(c0, 110, 143) == nullptr);
    CHECK(w.check_invariants() == "");
}

/// The first two cities of island 0 in the other order. A copy, not std::swap:
/// GCC 13 at -O3 reports -Wmaybe-uninitialized for the moved-from
/// optional<vector> inside std::swap of a CitySpec (the release CI job).
static void swap_first_two_cities(sim::ScenarioSpec& spec) {
    auto& cities = spec.islands[0].cities;
    const std::vector<sim::ScenarioSpec::CitySpec> other = {cities[1], cities[0]};
    cities = other;
}

TEST_CASE("world create makes cities in ascending number and keeps the file order inside") {
    const Kit k = make_kit();
    sim::ScenarioSpec spec = kit_scenario();
    // City 1 is listed before city 0; the serials follow the city number.
    spec.islands[0].cities[1].buildings = {place(k.weaver, 101, 101), place(k.farm, 103, 101)};
    spec.islands[0].cities[0].buildings = {place(k.church, 105, 101), place(k.weaver, 107, 101)};
    swap_first_two_cities(spec);
    REQUIRE(spec.islands[0].cities[0].no.v == 1);
    sim::World w = make_world(k, spec);
    CHECK(city_of(w, 0, 0).index.lists[1].slots[0]->id ==
          sim::make_building_id(sim::IslandId{0}, 1));
    CHECK(city_of(w, 0, 0).index.lists[4].slots[0]->id ==
          sim::make_building_id(sim::IslandId{0}, 2));
    CHECK(city_of(w, 0, 1).index.lists[4].slots[0]->id ==
          sim::make_building_id(sim::IslandId{0}, 3));
    CHECK(city_of(w, 0, 1).index.lists[4].slots[1]->id ==
          sim::make_building_id(sim::IslandId{0}, 4));
    // The result does not depend on the order of the cities in the scenario.
    sim::ScenarioSpec swapped = spec;
    swap_first_two_cities(swapped);
    CHECK(make_world(k, swapped).state_hash() == w.state_hash());
}

TEST_CASE("world create stocks and refuses what a scenario city cannot hold") {
    const Kit k = make_kit();
    sim::ScenarioSpec spec = kit_scenario();
    std::vector<std::uint32_t> stock(56, 0);
    stock[9] = 7 * sim::kTon;
    stock[41] = 3 * sim::kTon;
    spec.islands[0].cities[0].stock = stock;
    sim::World w = make_world(k, spec);
    CHECK(city_of(w, 0, 0).stock.stock == stock);
    CHECK(city_of(w, 0, 1).stock.stock == std::vector<std::uint32_t>(56, 0));
    CHECK(city_of(w, 0, 0).stock.capacity == 50 * sim::kTon);
    CHECK(city_of(w, 0, 0).goods.size() == 56);
    CHECK(city_of(w, 0, 0).build_goods.size() == 5);
    CHECK(city_of(w, 0, 0).trade_lists.size() == 2);
    CHECK(city_of(w, 0, 0).index.lists.size() == 6);
}

TEST_CASE("world create names the path of a city or building it refuses") {
    const Kit k = make_kit();
    using Spec = sim::ScenarioSpec;
    struct Row {
        const char* name;
        void (*mutate)(Spec&, const Kit&);
        const char* expect;
    };
    const Row rows[] = {
        {"city number beyond the slots",
         [](Spec& s, const Kit&) { s.islands[0].cities[1].no = sim::CityNo{16}; },
         "islands[0].cities[1].no: is not a slot below SimConfig.city_slots (16)"},
        {"invalid city number",
         [](Spec& s, const Kit&) { s.islands[0].cities[1].no = sim::CityNo{}; },
         "islands[0].cities[1].no: is not a slot below SimConfig.city_slots (16)"},
        {"the neutral owner is no city number",
         [](Spec& s, const Kit&) { s.islands[0].cities[1].no = sim::CityNo{15}; },
         "islands[0].cities[1].no: 15 is SimConfig.neutral_owner"},
        {"city number twice",
         [](Spec& s, const Kit&) { s.islands[0].cities[1].no = sim::CityNo{0}; },
         "islands[0].cities[1].no: city 0 is used twice"},
        {"city without a player",
         [](Spec& s, const Kit&) { s.islands[0].cities[0].owner = sim::PlayerId{7}; },
         "islands[0].cities[0].owner: there is no player in slot 7"},
        {"city with an invalid owner",
         [](Spec& s, const Kit&) { s.islands[0].cities[0].owner = sim::PlayerId{}; },
         "islands[0].cities[0].owner: there is no player in slot (invalid)"},
        {"stock of the wrong size",
         [](Spec& s, const Kit&) { s.islands[0].cities[0].stock = std::vector<std::uint32_t>(55); },
         "islands[0].cities[0].stock: 55 entries, SimConfig.warehouse_goods is 56"},
        {"unknown building type",
         [](Spec& s, const Kit&) {
             s.islands[0].cities[0].buildings = {place(sim::TypeId{424242}, 101, 101)};
         },
         "islands[0].cities[0].buildings[0].type: 424242 is not a type of the registry"},
        {"building type 0",
         [](Spec& s, const Kit&) {
             s.islands[0].cities[0].buildings = {place(sim::TypeId{0}, 101, 101)};
         },
         "islands[0].cities[0].buildings[0].type: 0 is not a type of the registry"},
        {"rotation 4",
         [](Spec& s, const Kit& k2) {
             s.islands[0].cities[0].buildings = {place(k2.weaver, 101, 101),
                                                 place(k2.weaver, 103, 101, 4)};
         },
         "islands[0].cities[0].buildings[1].rotation: 4 is not below 4"},
        {"a road is no building",
         [](Spec& s, const Kit& k2) {
             s.islands[0].cities[1].buildings = {place(k2.road, 101, 101)};
         },
         "islands[0].cities[1].buildings[0].type: 9010 (PRODTYPE 193) is not a building"},
        {"payload of the wrong class",
         [](Spec& s, const Kit& k2) {
             auto b = place(k2.weaver, 101, 101);
             b.payload = sim::Farm{};
             s.islands[0].cities[0].buildings = {b};
         },
         "islands[0].cities[0].buildings[0].payload: holds Farm, the class needs Workshop"},
        {"footprint left of the island",
         [](Spec& s, const Kit& k2) {
             s.islands[0].cities[0].buildings = {place(k2.weaver, 99, 101)};
         },
         "islands[0].cities[0].buildings[0]: the footprint is not inside the island"},
        {"footprint beyond the island",
         [](Spec& s, const Kit& k2) {
             s.islands[0].cities[0].buildings = {place(k2.tall_house, 101, 153)};
         },
         "islands[0].cities[0].buildings[0]: the footprint is not inside the island"},
        {"two buildings on one tile",
         [](Spec& s, const Kit& k2) {
             s.islands[0].cities[0].buildings = {place(k2.weaver, 101, 101),
                                                 place(k2.kontor, 101, 101)};
         },
         "islands[0].cities[0].buildings[1]: the footprint overlaps another building"},
        {"overlap across cities",
         [](Spec& s, const Kit& k2) {
             s.islands[0].cities[0].buildings = {place(k2.big_house, 110, 110)};
             s.islands[0].cities[1].buildings = {place(k2.weaver, 112, 111)};
         },
         "islands[0].cities[1].buildings[0]: the footprint overlaps another building"},
        {"tile owner without a city",
         [](Spec& s, const Kit&) { s.islands[0].cities.erase(s.islands[0].cities.begin()); },
         "islands[0].owner_runs[1]: owner 0 is not a city of the island (islands[0].cities)"},
        {"an error on the second island names it by its position in the scenario",
         [](Spec& s, const Kit& k2) {
             s.islands[1].cities[0].buildings = {place(k2.weaver, 299, 201)};
         },
         "islands[1].cities[0].buildings[0]: the footprint is not inside the island"},
    };
    for (const Row& row : rows) {
        INFO("row " << row.name);
        sim::ScenarioSpec spec = kit_scenario();
        row.mutate(spec, k);
        const std::string err = create_error(k, spec);
        CHECK(err.rfind(row.expect, 0) == 0);
        if (err.rfind(row.expect, 0) != 0) {
            MESSAGE("got: " << err);
        }
    }
    CHECK(create_error(k, kit_scenario()) == "");
    // A tile owner that is a city: fine; a run of length 0 names no tile, so no city.
    sim::ScenarioSpec ok = kit_scenario();
    ok.islands[0].owner_runs = {{15, 100}, {7, 0}, {0, 5}};
    CHECK(create_error(k, ok) == "");
}

// ------------------------------------------------- facts the mutation pass found

TEST_CASE("a type with the id 0 is no type") {
    // Build skips id 0 (spec 8.4), and a building of id 0 would leave a zero object on
    // its tiles, which check_invariants reads as "no object".
    sim::TypeRegistry::Builder b;
    b.add_plain(sim::TypeId{0}, 97).add_plain(sim::TypeId{5}, 97);
    auto reg = b.build();
    REQUIRE(reg.ok());
    auto created = sim::World::create(syn::basic_scenario(), reg.value());
    REQUIRE(created.ok());
    sim::World w = std::move(created).value();
    auto r = sim::WorldTestAccess::create(w, 0, 0, record(sim::TypeId{0}, 110, 110));
    REQUIRE_FALSE(r.ok());
    CHECK(sim::reject_name(r.error()) == "UnknownType");
    CHECK(sim::WorldTestAccess::create(w, 0, 0, record(sim::TypeId{5}, 110, 110)).ok());
    CHECK(w.check_invariants() == "");
    // The scenario names the same refusal with its path.
    sim::ScenarioSpec spec = syn::basic_scenario();
    spec.islands[0].cities[0].buildings = {place(sim::TypeId{0}, 110, 110)};
    auto bad = sim::World::create(spec, reg.value());
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().rfind(
              "islands[0].cities[0].buildings[0].type: 0 is not a type of the registry", 0) == 0);
}

TEST_CASE("remove_building refuses a key of another island") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    const auto key = create_ok(w, 3, 1, record(k.weaver, 301, 201));
    const std::uint64_t before = w.state_hash();
    // The key names island 3; handing it island 0 must not touch island 0's city 1.
    create_ok(w, 0, 1, record(k.weaver, 101, 101));
    const std::uint64_t with_other = w.state_hash();
    CHECK_FALSE(sim::remove_building(w.config(), w.types(), sim::WorldTestAccess::players(w),
                                     isl(w, 0), key));
    CHECK(w.state_hash() == with_other);
    CHECK(with_other != before);
    CHECK(sim::remove_building(w.config(), w.types(), sim::WorldTestAccess::players(w), isl(w, 3),
                               key));
}

TEST_CASE("house groups come from the layout's house block and the house class") {
    sim::SimConfig cfg;
    CHECK(sim::house_group(cfg, 97) == std::optional<std::uint32_t>{0});
    CHECK(sim::house_group(cfg, 101) == std::optional<std::uint32_t>{4});
    CHECK_FALSE(sim::house_group(cfg, 102).has_value()); // the sixth group does not exist
    CHECK_FALSE(sim::house_group(cfg, 96).has_value());  // the block below
    CHECK_FALSE(sim::house_group(cfg, 0).has_value());
    CHECK_FALSE(sim::house_group(cfg, 5000).has_value());
    cfg.house_groups = 2;
    CHECK(sim::house_group(cfg, 98) == std::optional<std::uint32_t>{1});
    CHECK_FALSE(sim::house_group(cfg, 99).has_value()); // group 2 with two groups
    // A layout whose house block is another block moves the numbers with it: the
    // production block (PRODTYPE 1..32) as the house list gives group = PRODTYPE - 1.
    sim::SimConfig moved;
    moved.layout.roles.house_list = 4;
    CHECK(sim::house_group(moved, 1) == std::optional<std::uint32_t>{0});
    CHECK(sim::house_group(moved, 3) == std::optional<std::uint32_t>{2});
    CHECK_FALSE(sim::house_group(moved, 97).has_value());
    // A layout that maps two blocks to the house list numbers the groups on across
    // them, in block order: PRODTYPE 33..64 (block 1) are groups 0..31, 97..128 (block
    // 3) are groups 32..63. The first block that does not contain the number does not
    // end the search.
    sim::SimConfig two;
    two.layout.block_to_list[1] = 3;
    two.house_groups = 64;
    REQUIRE(two.validate() == "");
    CHECK(sim::house_group(two, 33) == std::optional<std::uint32_t>{0});
    CHECK(sim::house_group(two, 64) == std::optional<std::uint32_t>{31});
    CHECK(sim::house_group(two, 97) == std::optional<std::uint32_t>{32});
    CHECK(sim::house_group(two, 101) == std::optional<std::uint32_t>{36});
    CHECK(sim::house_group(two, 128) == std::optional<std::uint32_t>{63});
    CHECK_FALSE(sim::house_group(two, 129).has_value()); // block 4 is the military list
    CHECK_FALSE(sim::house_group(two, 32).has_value());  // block 0 is the production list
    two.house_groups = 40;                               // groups 40.. have no counter
    CHECK(sim::house_group(two, 103) == std::optional<std::uint32_t>{38});
    CHECK_FALSE(sim::house_group(two, 105).has_value());
    // The houses of the second house block are counted (the group is 32 + 0 here).
    {
        const Kit k = make_kit();
        sim::SimConfig cfg2 = two;
        cfg2.house_groups = 64;
        sim::World w2 = make_world(k, kit_scenario(), cfg2);
        create_ok(w2, 0, 0, record(k.pioneer_house, 101, 101)); // PRODTYPE 97
        std::vector<std::uint32_t> want(64, 0);
        want[32] = 1;
        CHECK(city_of(w2, 0, 0).index.house_counts == want);
        CHECK(w2.check_invariants() == "");
    }
    // The counter follows the class, not the number: a type of a house PRODTYPE whose
    // class is not House is counted nowhere.
    sim::TypeRegistry::Builder b;
    sim::BuildingType odd = custom_type(sim::TypeId{9300}, 99, 1, 1);
    odd.cls = sim::RuntimeClass::Plain;
    b.add(odd).add(custom_type(sim::TypeId{9301}, 99, 1, 1));
    auto reg = b.build();
    REQUIRE(reg.ok());
    auto created = sim::World::create(syn::basic_scenario(), reg.value());
    REQUIRE(created.ok());
    sim::World w = std::move(created).value();
    REQUIRE(sim::WorldTestAccess::create(w, 0, 0, record(sim::TypeId{9300}, 110, 110)).ok());
    CHECK(city_of(w, 0, 0).index.house_counts == std::vector<std::uint32_t>(5, 0));
    REQUIRE(sim::WorldTestAccess::create(w, 0, 0, record(sim::TypeId{9301}, 112, 110)).ok());
    CHECK(city_of(w, 0, 0).index.house_counts == std::vector<std::uint32_t>{0, 0, 1, 0, 0});
    CHECK(w.check_invariants() == "");
}

TEST_CASE("a mine searches from its rotated footprint") {
    // A 1 x 4 mine: the deposit 7 rows below its footprint and 2 columns to the right is
    // in reach (half-width 2 in the last row); with width and height swapped the same
    // deposit would be 10 rows away.
    sim::TypeRegistry::Builder base;
    base.add_mine(sim::TypeId{1}, "GOLD", 21, 5);
    auto first = base.build();
    REQUIRE(first.ok());
    sim::BuildingType tall = *first.value()->find(sim::TypeId{1});
    tall.id = sim::TypeId{2};
    tall.height = 4;
    sim::TypeRegistry::Builder b;
    b.add(tall);
    auto reg = b.build();
    REQUIRE(reg.ok());
    sim::ScenarioSpec spec = syn::basic_scenario();
    spec.islands[0].deposits = {{"GOLD", 112, 111, 2000 * sim::kTon, false}};
    auto created = sim::World::create(spec, reg.value());
    REQUIRE(created.ok());
    sim::World w = std::move(created).value();
    // Rotation 0: footprint 1 wide, 4 tall at (110, 101).
    const auto upright = sim::WorldTestAccess::create(w, 0, 0, record(sim::TypeId{2}, 110, 101));
    REQUIRE(upright.ok());
    CHECK(std::get<sim::Mine>(at_key(w, upright.value()).payload).deposit ==
          std::optional<std::size_t>{0});
    // Rotation 1 turns it to 4 wide, 1 tall, at (120, 102): 9 rows above the deposit.
    const auto turned = sim::WorldTestAccess::create(w, 0, 0, record(sim::TypeId{2}, 120, 102, 1));
    REQUIRE(turned.ok());
    CHECK(sim::mine_area(at_key(w, turned.value()), *reg.value()->find(sim::TypeId{2})).w == 4);
    CHECK_FALSE(std::get<sim::Mine>(at_key(w, turned.value()).payload).deposit.has_value());
}

TEST_CASE("find_building does not trust a stale id map") {
    const Kit k = make_kit();
    sim::World w = make_world(k, kit_scenario());
    const auto a = create_ok(w, 0, 0, record(k.weaver, 101, 101));
    const auto b = create_ok(w, 0, 0, record(k.weaver, 103, 101));
    const sim::BuildingId ida = at_key(w, a).id;
    const sim::BuildingId idb = at_key(w, b).id;
    REQUIRE(w.find_building(ida) != nullptr);
    // The cache says A sits where B sits: the slot holds B, so A is not found there.
    isl(w, 0).cache.by_id[ida] = b;
    CHECK(w.find_building(ida) == nullptr);
    CHECK(w.find_building(idb) != nullptr);
}

TEST_CASE("reject names and results") {
    CHECK(sim::reject_name(sim::Reject::None) == "None");
    CHECK(sim::reject_name(sim::Reject::OutOfIsland) == "OutOfIsland");
    CHECK(sim::reject_name(sim::Reject::ListFull) == "ListFull");
    CHECK(sim::reject_name(sim::Reject::Unencodable) == "Unencodable");
    CHECK(sim::reject_name(static_cast<sim::Reject>(999)) == "?");
    // Every enumerator has a name of its own (Unencodable is the last).
    std::vector<std::string> names;
    for (std::uint16_t v = 0; v <= static_cast<std::uint16_t>(sim::Reject::Unencodable); ++v) {
        names.emplace_back(sim::reject_name(static_cast<sim::Reject>(v)));
        CHECK(names.back() != "?");
    }
    std::sort(names.begin(), names.end());
    CHECK(std::adjacent_find(names.begin(), names.end()) == names.end());
    sim::ApplyResult ok;
    CHECK(ok.ok());
    CHECK(ok.records.empty());
    sim::ApplyResult bad;
    bad.status = sim::Reject::NotHost;
    bad.records.push_back(sim::RecordResult{sim::Reject::Occupied, sim::BuildingId{4}});
    CHECK_FALSE(bad.ok());
    CHECK(bad.records[0].status == sim::Reject::Occupied);
    CHECK(bad.records[0].building.v == 4);
    CHECK_FALSE(sim::RecordResult{}.building.valid());
}
