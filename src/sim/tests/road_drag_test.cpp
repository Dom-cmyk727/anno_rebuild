// ROADS-1 / SIM-ROADS-1 unit tests and Anlagen.dat road corpus check
// (docs/BUILDINGSOBJECTS.md §10.1, AnnoFrame 0x1004b290 / 0x1004af90).
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/extension_context.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/road_drag.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>

#include "command_helpers.hpp"
#include "tutorial_town.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

constexpr std::int32_t kX0 = 100;
constexpr std::int32_t kY0 = 200;
constexpr std::uint32_t kSize = 64;
constexpr sim::CityRef kCity0{sim::IslandId{0}, sim::CityNo{0}};

struct RoadKit {
    std::shared_ptr<const sim::TypeRegistry> reg;
    sim::TypeId dirt{9801};        // STRASSE 193, 0x4186, cost 5
    sim::TypeId dirt_corner{9802}; // STRASSE 193, 0x1186, cost 5 (slope class)
    sim::TypeId cobble{9803};      // STRASSE 193, 0x4186, cost 10
    sim::TypeId cobble_wide{9804}; // STRASSE 193, 0x4186, 2x1, cost 10
    sim::TypeId bridge{9805};      // BRUECKE 198, 0x4386, cost 500
    sim::TypeId house{9806};       // PRODTYPE 97, 2x2, 0x0084
    sim::TypeId locked_dirt{9807}; // STRASSE 193, 0x4186, cost 5, requires stage 31
    std::uint32_t soldier_kind = 9901;
};

RoadKit make_kit() {
    RoadKit k;
    sim::TypeRegistry::Builder b;
    auto add_road = [&](sim::TypeId id, std::uint32_t prodtype, std::uint32_t flags,
                        std::uint32_t w, std::uint32_t h, std::uint32_t gold,
                        std::uint32_t req_stage = 0) {
        sim::BuildingType t;
        t.id = id;
        t.prodtype = prodtype;
        t.flags_word = flags;
        t.width = w;
        t.height = h;
        t.cost_gold = gold;
        t.construction.stage = req_stage;
        t.head_raw.assign(56, 0);
        t.head_raw[0x2f] = 0;
        t.head_raw[0x30] = 3;
        b.add(t);
    };
    add_road(k.dirt, 193, 0x4186, 1, 1, 5);
    add_road(k.dirt_corner, 193, 0x1186, 1, 1, 5);
    add_road(k.cobble, 193, 0x4186, 1, 1, 10);
    add_road(k.cobble_wide, 193, 0x4186, 2, 1, 10);
    add_road(k.bridge, 198, 0x4386, 1, 1, 500);
    add_road(k.locked_dirt, 193, 0x4186, 1, 1, 5, 31);

    sim::BuildingType house;
    house.id = k.house;
    house.prodtype = 97;
    house.flags_word = 0x0084;
    house.width = 2;
    house.height = 2;
    house.cost_gold = 10;
    b.add(house);

    sim::FigureType ft;
    ft.id = k.soldier_kind;
    ft.bewohntype = 100; // in veto query range 97..128, plain_branch
    b.figure_type(ft);

    k.reg = b.build().value();
    return k;
}

sim::World make_road_world(const RoadKit& kit, sim::RoadRules rr = {}, std::int64_t money = 1000,
                           sim::Rules rules = {}) {
    sim::ScenarioSpec s = syn::empty_scenario(77);
    s.settings.world_width = 512;
    s.settings.world_height = 512;
    s.players.resize(2);
    for (std::uint32_t p = 0; p < 2; ++p) {
        s.players[p].id = sim::PlayerId{p};
        s.players[p].money = money;
    }
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, kX0, kY0, kSize, kSize);
    is.owner_runs = {{0U, kSize * kSize}};
    is.cities.push_back(syn::plain_city(0, 0));
    is.cities.push_back(syn::plain_city(1, 1));
    std::vector<std::uint8_t> h(kSize * kSize, 0x30);
    is.terrain = sim::make_terrain(h, std::vector<std::uint16_t>(h.size(), 0),
                                   std::vector<std::uint8_t>(h.size(), 0));
    s.islands.push_back(std::move(is));

    sim::ExtensionContext ext;
    ext.road_rules = rr;
    auto w = sim::World::create(s, kit.reg, {}, rules, {}, sim::serial_executor(), ext);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

sim::RoadRules default_road_rules() {
    sim::RoadRules rr;
    rr.enabled = 1;
    return rr;
}

std::uint32_t tile_object(const sim::World& w, std::int32_t x, std::int32_t y) {
    const sim::Island* is = w.island_at(x, y);
    REQUIRE(is != nullptr);
    const sim::Slot idx = is->tile_index(x, y);
    REQUIRE(idx != sim::kNoSlot);
    return is->tiles.object(idx);
}

} // namespace

TEST_CASE("road_path: L shape, longer axis first, y first on tie, cap 40") {
    // Longer x axis: x runs first, then y (AnnoFrame 0x1004b392-0x1004b51f).
    const auto p1 = sim::road_path({10, 20}, {13, 21});
    REQUIRE(p1.size() == 5);
    CHECK(p1[0] == sim::TileXY{10, 20});
    CHECK(p1[1] == sim::TileXY{11, 20});
    CHECK(p1[2] == sim::TileXY{12, 20});
    CHECK(p1[3] == sim::TileXY{13, 20});
    CHECK(p1[4] == sim::TileXY{13, 21});

    // Longer y axis (negative directions): y runs first, then x.
    const auto p2 = sim::road_path({10, 20}, {9, 17});
    REQUIRE(p2.size() == 5);
    CHECK(p2[0] == sim::TileXY{10, 20});
    CHECK(p2[1] == sim::TileXY{10, 19});
    CHECK(p2[2] == sim::TileXY{10, 18});
    CHECK(p2[3] == sim::TileXY{10, 17});
    CHECK(p2[4] == sim::TileXY{9, 17});

    // Tie (|dx| == |dy|): y runs first, NOT x first.
    const auto tie = sim::road_path({0, 0}, {2, 2});
    REQUIRE(tie.size() == 5);
    CHECK(tie[1] == sim::TileXY{0, 1});
    CHECK(tie[2] == sim::TileXY{0, 2});
    CHECK(tie[3] == sim::TileXY{1, 2});
    CHECK(tie[4] == sim::TileXY{2, 2});

    // Single tile.
    const auto single = sim::road_path({5, 6}, {5, 6});
    REQUIRE(single.size() == 1);
    CHECK(single[0] == sim::TileXY{5, 6});

    // Cap = 40 cuts the path at 40 tiles (39 steps), not reaching end.
    const auto capped = sim::road_path({0, 0}, {50, 0});
    REQUIRE(capped.size() == 40);
    CHECK(capped.front() == sim::TileXY{0, 0});
    CHECK(capped.back() == sim::TileXY{39, 0});

    // Cap = 0 returns empty.
    CHECK(sim::road_path({0, 0}, {5, 5}, 0).empty());
}

TEST_CASE("road drag: stops at first off-island tile") {
    const RoadKit kit = make_kit();
    sim::World w = make_road_world(kit, default_road_rules());

    // Start 3 tiles inside right edge (kX0 + kSize - 3 = 161) and drag past edge to 165.
    const sim::BuildCmd cmd =
        sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 61, kY0 + 10}, {kX0 + 65, kY0 + 10});
    const sim::ApplyResult res = w.apply({sim::PlayerId{0}, cmd});
    CHECK(res.ok());
    CHECK(res.records[0].status == sim::Reject::None);
    CHECK(res.records[1].status == sim::Reject::None);
    CHECK(res.records[2].status == sim::Reject::None);
    CHECK(res.records[3].status == sim::Reject::OutOfIsland);
    CHECK(res.records[4].status == sim::Reject::PrefixStopped);
    CHECK(tile_object(w, kX0 + 61, kY0 + 10) == kit.dirt.v);
    CHECK(tile_object(w, kX0 + 62, kY0 + 10) == kit.dirt.v);
    CHECK(tile_object(w, kX0 + 63, kY0 + 10) == kit.dirt.v);
}

TEST_CASE("road drag: every tile must be owned by single city") {
    const RoadKit kit = make_kit();
    sim::World w = make_road_world(kit, default_road_rules());
    {
        auto& is = *sim::WorldTestAccess::islands(w)[0];
        const std::uint32_t neutral = sim::WorldTestAccess::config(w).neutral_owner;
        // Neutral tile at (kX0 + 3, kY0 + 5).
        is.tiles.set_owner(is.tile_index(kX0 + 3, kY0 + 5), neutral);
        // Rival city tile at (kX0 + 10, kY0 + 6).
        is.tiles.set_owner(is.tile_index(kX0 + 10, kY0 + 6), 1);
    }

    // Drag across neutral tile: stops at (kX0 + 3, kY0 + 5), tail marked PrefixStopped.
    const sim::BuildCmd cmd =
        sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 1, kY0 + 5}, {kX0 + 4, kY0 + 5});
    const sim::ApplyResult res = w.apply({sim::PlayerId{0}, cmd});
    CHECK(res.ok());
    CHECK(res.records[0].status == sim::Reject::None);
    CHECK(res.records[1].status == sim::Reject::None);
    CHECK(res.records[2].status == sim::Reject::TileNotOwned);
    CHECK(res.records[3].status == sim::Reject::PrefixStopped);
    CHECK(tile_object(w, kX0 + 2, kY0 + 5) == kit.dirt.v);
    CHECK(tile_object(w, kX0 + 3, kY0 + 5) == 0);
    CHECK(tile_object(w, kX0 + 4, kY0 + 5) == 0);

    // Multi-tile (2x1) road straddling owned + neutral is refused under every_tile_territory = 1.
    sim::BuildCmd wide_cmd;
    wide_cmd.city = kCity0;
    wide_cmd.placements.push_back({kit.cobble_wide, kX0 + 2, kY0 + 5, 0, 0, 0});
    const sim::ApplyResult wide_res = w.apply({sim::PlayerId{0}, wide_cmd});
    CHECK(wide_res.status == sim::Reject::TileNotOwned);

    // Rival city tile on path stops prefix.
    const sim::BuildCmd rival_cmd =
        sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 9, kY0 + 6}, {kX0 + 11, kY0 + 6});
    const sim::ApplyResult rival_res = w.apply({sim::PlayerId{0}, rival_cmd});
    CHECK(rival_res.ok());
    CHECK(rival_res.records[1].status == sim::Reject::TileNotOwned);
    CHECK(rival_res.records[2].status == sim::Reject::PrefixStopped);
    CHECK(tile_object(w, kX0 + 9, kY0 + 6) == kit.dirt.v);
    CHECK(tile_object(w, kX0 + 10, kY0 + 6) == 0);
    CHECK(tile_object(w, kX0 + 11, kY0 + 6) == 0);
}

TEST_CASE("road drag: City::Build refusal stops prefix") {
    const RoadKit kit = make_kit();
    sim::World w = make_road_world(kit, default_road_rules());

    // Place a 2x2 house at (kX0 + 5, kY0 + 5).
    sim::BuildCmd house_cmd;
    house_cmd.city = kCity0;
    house_cmd.placements.push_back({kit.house, kX0 + 5, kY0 + 5, 0, 0, 0});
    REQUIRE(w.apply({sim::PlayerId{0}, house_cmd}).ok());

    // Drag across the house from x=3 to x=8: 3,4 placed; 5 Occupied; 6,7,8 PrefixStopped.
    const sim::BuildCmd cmd =
        sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 3, kY0 + 5}, {kX0 + 8, kY0 + 5});
    const sim::ApplyResult res = w.apply({sim::PlayerId{0}, cmd});
    CHECK(res.ok());
    CHECK(res.records[2].status == sim::Reject::Occupied);
    CHECK(res.records[3].status == sim::Reject::PrefixStopped);
    CHECK(res.records[4].status == sim::Reject::PrefixStopped);
    CHECK(res.records[5].status == sim::Reject::PrefixStopped);
    CHECK(tile_object(w, kX0 + 3, kY0 + 5) == kit.dirt.v);
    CHECK(tile_object(w, kX0 + 4, kY0 + 5) == kit.dirt.v);
    CHECK(tile_object(w, kX0 + 7, kY0 + 5) == 0);
    CHECK(tile_object(w, kX0 + 8, kY0 + 5) == 0);
}

TEST_CASE("road drag: figure veto stops prefix and rotated footprint") {
    const RoadKit kit = make_kit();
    sim::World w = make_road_world(kit, default_road_rules());

    // Add a figure standing on tile (kX0 + 4, kY0 + 10).
    {
        sim::Figure f;
        f.id = sim::FigureId{1};
        f.kind = kit.soldier_kind;
        f.owner = sim::PlayerId{0};
        f.pos = sim::FixedPos{(kX0 + 4) * 256 + 128, (kY0 + 10) * 256 + 128};
        REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
                sim::kNoSlot);
    }

    const sim::BuildCmd cmd =
        sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 2, kY0 + 10}, {kX0 + 6, kY0 + 10});
    const sim::ApplyResult res = w.apply({sim::PlayerId{0}, cmd});
    CHECK(res.ok());
    CHECK(res.records[0].status == sim::Reject::None);
    CHECK(res.records[1].status == sim::Reject::None);
    CHECK(res.records[2].status == sim::Reject::FigureInTheWay);
    CHECK(res.records[3].status == sim::Reject::PrefixStopped);
    CHECK(res.records[4].status == sim::Reject::PrefixStopped);
    CHECK(tile_object(w, kX0 + 2, kY0 + 10) == kit.dirt.v);
    CHECK(tile_object(w, kX0 + 3, kY0 + 10) == kit.dirt.v);
    CHECK(tile_object(w, kX0 + 4, kY0 + 10) == 0);
    CHECK(tile_object(w, kX0 + 5, kY0 + 10) == 0);
    CHECK(tile_object(w, kX0 + 6, kY0 + 10) == 0);

    // Rotated 2x1 road (rotation 1 => 1x2) at (kX0 + 4, kY0 + 9) covers (kX0 + 4, kY0 + 10).
    sim::BuildCmd rot_cmd;
    rot_cmd.city = kCity0;
    rot_cmd.placements.push_back({kit.cobble_wide, kX0 + 4, kY0 + 9, 1, 0, 0});
    const sim::ApplyResult rot_res = w.apply({sim::PlayerId{0}, rot_cmd});
    CHECK(rot_res.status == sim::Reject::FigureInTheWay);
}

TEST_CASE("road drag: ConstructionDenied precedes FigureInTheWay when both apply") {
    const RoadKit kit = make_kit();
    sim::World w = make_road_world(kit, default_road_rules());

    {
        sim::Figure f;
        f.id = sim::FigureId{1};
        f.kind = kit.soldier_kind;
        f.owner = sim::PlayerId{0};
        f.pos = sim::FixedPos{(kX0 + 4) * 256 + 128, (kY0 + 10) * 256 + 128};
        REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
                sim::kNoSlot);
    }

    sim::BuildCmd cmd;
    cmd.city = kCity0;
    cmd.placements.push_back({kit.locked_dirt, kX0 + 4, kY0 + 10, 0, 0, 0});
    const sim::ApplyResult res = w.apply({sim::PlayerId{0}, cmd});
    CHECK(res.status == sim::Reject::ConstructionDenied);
}

TEST_CASE("road drag: no downgrade refuses dirt over cobblestone") {
    const RoadKit kit = make_kit();
    sim::World w = make_road_world(kit, default_road_rules());

    // Lay cobblestone at (kX0 + 3, kY0 + 12).
    const sim::BuildCmd cob =
        sim::make_road_drag_cmd(kCity0, kit.cobble, {kX0 + 3, kY0 + 12}, {kX0 + 3, kY0 + 12});
    REQUIRE(w.apply({sim::PlayerId{0}, cob}).ok());

    // Drag dirt from x=1 to x=5: x=1,2 placed; x=3 refused (Occupied); x=4,5 PrefixStopped.
    const sim::BuildCmd dirt_drag =
        sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 1, kY0 + 12}, {kX0 + 5, kY0 + 12});
    const sim::ApplyResult res = w.apply({sim::PlayerId{0}, dirt_drag});
    CHECK(res.ok());
    CHECK(res.records[2].status == sim::Reject::Occupied);
    CHECK(res.records[3].status == sim::Reject::PrefixStopped);
    CHECK(res.records[4].status == sim::Reject::PrefixStopped);
    CHECK(tile_object(w, kX0 + 1, kY0 + 12) == kit.dirt.v);
    CHECK(tile_object(w, kX0 + 2, kY0 + 12) == kit.dirt.v);
    CHECK(tile_object(w, kX0 + 3, kY0 + 12) == kit.cobble.v);
    CHECK(tile_object(w, kX0 + 4, kY0 + 12) == 0);

    // Upgrade (cobblestone over dirt) succeeds and replaces.
    const sim::BuildCmd upg =
        sim::make_road_drag_cmd(kCity0, kit.cobble, {kX0 + 1, kY0 + 12}, {kX0 + 2, kY0 + 12});
    CHECK(w.apply({sim::PlayerId{0}, upg}).ok());
    CHECK(tile_object(w, kX0 + 1, kY0 + 12) == kit.cobble.v);
    CHECK(tile_object(w, kX0 + 2, kY0 + 12) == kit.cobble.v);
}

TEST_CASE("road drag: same-family re-lay free, upgrade pays, stops when unaffordable, bridges "
          "always pay") {
    const RoadKit kit = make_kit();
    sim::World w = make_road_world(kit, default_road_rules(), 25);

    // 8-tile dirt drag with 25 gold (cost 5 each): places 5 tiles (0 gold left), stops at 6th.
    const sim::BuildCmd drag =
        sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 1, kY0 + 15}, {kX0 + 8, kY0 + 15});
    const sim::ApplyResult res = w.apply({sim::PlayerId{0}, drag});
    CHECK(res.ok());
    CHECK(res.records[4].status == sim::Reject::None);
    CHECK(res.records[5].status == sim::Reject::CannotAfford);
    CHECK(res.records[6].status == sim::Reject::PrefixStopped);
    CHECK(res.records[7].status == sim::Reject::PrefixStopped);
    CHECK(w.players().find(sim::PlayerId{0})->money == 0);
    for (int i = 0; i < 5; ++i) {
        CHECK(tile_object(w, kX0 + 1 + i, kY0 + 15) == kit.dirt.v);
    }
    for (int i = 5; i < 8; ++i) {
        CHECK(tile_object(w, kX0 + 1 + i, kY0 + 15) == 0);
    }

    // Re-laying dirt over the 5 placed dirt tiles (same family, e.g. dirt_corner) is FREE even with
    // 0 gold!
    const sim::BuildCmd relay =
        sim::make_road_drag_cmd(kCity0, kit.dirt_corner, {kX0 + 1, kY0 + 15}, {kX0 + 5, kY0 + 15});
    const sim::ApplyResult relay_res = w.apply({sim::PlayerId{0}, relay});
    CHECK(relay_res.ok());
    CHECK(w.players().find(sim::PlayerId{0})->money == 0);
    CHECK(tile_object(w, kX0 + 1, kY0 + 15) == kit.dirt_corner.v);

    // Upgrading dirt to cobblestone pays 10 gold per tile.
    sim::World w2 = make_road_world(kit, default_road_rules(), 35);
    REQUIRE(
        w2.apply({sim::PlayerId{0}, sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 1, kY0 + 15},
                                                            {kX0 + 3, kY0 + 15})})
            .ok());
    CHECK(w2.players().find(sim::PlayerId{0})->money == 20);
    const sim::BuildCmd upg =
        sim::make_road_drag_cmd(kCity0, kit.cobble, {kX0 + 1, kY0 + 15}, {kX0 + 3, kY0 + 15});
    const sim::ApplyResult upg_res = w2.apply({sim::PlayerId{0}, upg});
    CHECK(upg_res.ok());
    CHECK(upg_res.records[2].status == sim::Reject::CannotAfford);
    CHECK(w2.players().find(sim::PlayerId{0})->money == 0);
    CHECK(tile_object(w2, kX0 + 1, kY0 + 15) == kit.cobble.v);
    CHECK(tile_object(w2, kX0 + 2, kY0 + 15) == kit.cobble.v);
    CHECK(tile_object(w2, kX0 + 3, kY0 + 15) == kit.dirt.v);

    // Bridges are in neither road family and always pay (same_road_family is false).
    const sim::BuildingType* br = kit.reg->find(kit.bridge);
    REQUIRE(br != nullptr);
    CHECK_FALSE(sim::is_road_family(*br));
    CHECK_FALSE(sim::same_road_family(*br, *br));
    CHECK_FALSE(sim::is_road_downgrade(*br, *br));
}

TEST_CASE("road drag: drag_cap enforced in BuildCmd (default 40 and custom)") {
    const RoadKit kit = make_kit();
    // 45 placements with default drag_cap = 40: 40 placed, 40..44 PrefixStopped.
    sim::World w = make_road_world(kit, default_road_rules(), 1000);
    sim::BuildCmd cmd45;
    cmd45.city = kCity0;
    for (int i = 0; i < 45; ++i) {
        cmd45.placements.push_back({kit.dirt, kX0 + 1 + i, kY0 + 20, 0, 0, 0});
    }
    const sim::ApplyResult r45 = w.apply({sim::PlayerId{0}, cmd45});
    CHECK(r45.ok());
    for (int i = 0; i < 40; ++i) {
        CHECK(r45.records[static_cast<std::size_t>(i)].status == sim::Reject::None);
        CHECK(tile_object(w, kX0 + 1 + i, kY0 + 20) == kit.dirt.v);
    }
    for (int i = 40; i < 45; ++i) {
        CHECK(r45.records[static_cast<std::size_t>(i)].status == sim::Reject::PrefixStopped);
        CHECK(tile_object(w, kX0 + 1 + i, kY0 + 20) == 0);
    }

    // Custom drag_cap = 3: 5 placements -> 3 placed, 3..4 PrefixStopped.
    sim::RoadRules rr3 = default_road_rules();
    rr3.drag_cap = 3;
    sim::World w3 = make_road_world(kit, rr3, 1000);
    const sim::BuildCmd cmd5 =
        sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 1, kY0 + 22}, {kX0 + 5, kY0 + 22});
    const sim::ApplyResult r5 = w3.apply({sim::PlayerId{0}, cmd5});
    CHECK(r5.ok());
    CHECK(r5.records[0].status == sim::Reject::None);
    CHECK(r5.records[1].status == sim::Reject::None);
    CHECK(r5.records[2].status == sim::Reject::None);
    CHECK(r5.records[3].status == sim::Reject::PrefixStopped);
    CHECK(r5.records[4].status == sim::Reject::PrefixStopped);
    CHECK(tile_object(w3, kX0 + 4, kY0 + 22) == 0);
}

TEST_CASE("road drag: non-road multi-building BuildCmd continues and build_atomic aborts all") {
    const RoadKit kit = make_kit();
    sim::World w = make_road_world(kit, default_road_rules(), 1000);

    // Pre-place a house at (kX0 + 10, kY0 + 10) to collide with the second building.
    sim::BuildCmd pre;
    pre.city = kCity0;
    pre.placements.push_back({kit.house, kX0 + 10, kY0 + 10, 0, 0, 0});
    REQUIRE(w.apply({sim::PlayerId{0}, pre}).ok());

    // Multi-building BuildCmd with road_rules enabled still continues past a failing building.
    sim::BuildCmd multi;
    multi.city = kCity0;
    multi.placements.push_back({kit.house, kX0 + 2, kY0 + 10, 0, 0, 0});
    multi.placements.push_back({kit.house, kX0 + 10, kY0 + 10, 0, 0, 0}); // Occupied
    multi.placements.push_back({kit.house, kX0 + 20, kY0 + 10, 0, 0, 0});
    const sim::ApplyResult multi_res = w.apply({sim::PlayerId{0}, multi});
    CHECK(multi_res.ok());
    CHECK(multi_res.records[0].status == sim::Reject::None);
    CHECK(multi_res.records[1].status == sim::Reject::Occupied);
    CHECK(multi_res.records[2].status == sim::Reject::None);

    // Rules::build_atomic with road_rules.enabled == 1 aborts the entire road drag on any refusal.
    sim::Rules atomic_rules;
    atomic_rules.build_atomic = true;
    sim::World wa = make_road_world(kit, default_road_rules(), 10, atomic_rules);
    const sim::BuildCmd drag3 =
        sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 1, kY0 + 30}, {kX0 + 3, kY0 + 30});
    const sim::ApplyResult ar = wa.apply({sim::PlayerId{0}, drag3});
    CHECK(ar.status == sim::Reject::AtomicAbort);
    CHECK(ar.records[0].status == sim::Reject::AtomicAbort);
    CHECK(ar.records[1].status == sim::Reject::AtomicAbort);
    CHECK(ar.records[2].status == sim::Reject::CannotAfford);
    CHECK(tile_object(wa, kX0 + 1, kY0 + 30) == 0);
}

TEST_CASE("road drag: fallback toggles with enabled = 1") {
    const RoadKit kit = make_kit();

    // every_tile_territory = 0 allows multi-tile road straddling owned + neutral.
    {
        sim::RoadRules rr = default_road_rules();
        rr.every_tile_territory = 0;
        sim::World w = make_road_world(kit, rr);
        auto& is = *sim::WorldTestAccess::islands(w)[0];
        is.tiles.set_owner(is.tile_index(kX0 + 3, kY0 + 5),
                           sim::WorldTestAccess::config(w).neutral_owner);
        sim::BuildCmd cmd;
        cmd.city = kCity0;
        cmd.placements.push_back({kit.cobble_wide, kX0 + 2, kY0 + 5, 0, 0, 0});
        CHECK(w.apply({sim::PlayerId{0}, cmd}).ok());
    }

    // no_downgrade = 0 allows dirt over cobblestone.
    {
        sim::RoadRules rr = default_road_rules();
        rr.no_downgrade = 0;
        sim::World w = make_road_world(kit, rr);
        REQUIRE(w.apply({sim::PlayerId{0},
                         sim::make_road_drag_cmd(kCity0, kit.cobble, {kX0 + 2, kY0 + 5},
                                                 {kX0 + 2, kY0 + 5})})
                    .ok());
        CHECK(
            w.apply({sim::PlayerId{0}, sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 2, kY0 + 5},
                                                               {kX0 + 2, kY0 + 5})})
                .ok());
        CHECK(tile_object(w, kX0 + 2, kY0 + 5) == kit.dirt.v);
    }

    // free_same_family = 0 charges for dirt over dirt.
    {
        sim::RoadRules rr = default_road_rules();
        rr.free_same_family = 0;
        sim::World w = make_road_world(kit, rr, 20);
        REQUIRE(
            w.apply({sim::PlayerId{0}, sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 2, kY0 + 5},
                                                               {kX0 + 2, kY0 + 5})})
                .ok());
        CHECK(w.players().find(sim::PlayerId{0})->money == 15);
        REQUIRE(
            w.apply({sim::PlayerId{0}, sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 2, kY0 + 5},
                                                               {kX0 + 2, kY0 + 5})})
                .ok());
        CHECK(w.players().find(sim::PlayerId{0})->money == 10);
    }

    // stop_at_first_refusal = 0 continues past a refusal.
    {
        sim::RoadRules rr = default_road_rules();
        rr.stop_at_first_refusal = 0;
        sim::World w = make_road_world(kit, rr);
        auto& is = *sim::WorldTestAccess::islands(w)[0];
        is.tiles.set_owner(is.tile_index(kX0 + 2, kY0 + 5),
                           sim::WorldTestAccess::config(w).neutral_owner);
        const sim::ApplyResult res =
            w.apply({sim::PlayerId{0}, sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 1, kY0 + 5},
                                                               {kX0 + 3, kY0 + 5})});
        CHECK(res.ok());
        CHECK(res.records[0].status == sim::Reject::None);
        CHECK(res.records[1].status == sim::Reject::TileNotOwned);
        CHECK(res.records[2].status == sim::Reject::None);
        CHECK(tile_object(w, kX0 + 3, kY0 + 5) == kit.dirt.v);
    }

    // figure_veto = 0 allows placing a road under a figure.
    {
        sim::RoadRules rr = default_road_rules();
        rr.figure_veto = 0;
        sim::World w = make_road_world(kit, rr);
        sim::Figure f;
        f.id = sim::FigureId{1};
        f.kind = kit.soldier_kind;
        f.owner = sim::PlayerId{0};
        f.pos = sim::FixedPos{(kX0 + 2) * 256 + 128, (kY0 + 5) * 256 + 128};
        REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
                sim::kNoSlot);
        CHECK(
            w.apply({sim::PlayerId{0}, sim::make_road_drag_cmd(kCity0, kit.dirt, {kX0 + 2, kY0 + 5},
                                                               {kX0 + 2, kY0 + 5})})
                .ok());
    }
}

TEST_CASE("road drag: kind-9 prefix commit and tag 2 persistence") {
    const RoadKit kit = make_kit();
    sim::World w_default = make_road_world(kit, sim::RoadRules{});
    sim::World w_enabled = make_road_world(kit, default_road_rules());

    CHECK_FALSE(w_default.road_drag_enabled());
    CHECK(w_enabled.road_drag_enabled());
    CHECK(w_default.state_hash() != w_enabled.state_hash());

    const auto bytes = w_enabled.save();
    const auto loaded = sim::World::load(bytes, kit.reg);
    REQUIRE(loaded.ok());
    CHECK(loaded.value().road_drag_enabled());
    CHECK(loaded.value().road_rules() == default_road_rules());
    CHECK(loaded.value().state_hash() == w_enabled.state_hash());

    sim::RoadRules bad_cap = default_road_rules();
    bad_cap.drag_cap = 0;
    CHECK_FALSE(sim::RoadRulesExtension::validate(bad_cap).empty());
    sim::RoadRules bad_flag = default_road_rules();
    bad_flag.no_downgrade = 2;
    CHECK_FALSE(sim::RoadRulesExtension::validate(bad_flag).empty());
    sim::RoadRules bad_disabled;
    bad_disabled.drag_cap = 5;
    CHECK_FALSE(sim::RoadRulesExtension::validate(bad_disabled).empty());
}

TEST_CASE("road drag: Anlagen.dat road corpus classification") {
    const auto o = tutorial_town::load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::BuildingType* cobble = o->types->find(sim::TypeId{2500});
    const sim::BuildingType* dirt = o->types->find(sim::TypeId{2520});
    REQUIRE(cobble != nullptr);
    REQUIRE(dirt != nullptr);
    CHECK(sim::is_road_family(*cobble));
    CHECK(sim::is_road_family(*dirt));
    CHECK(sim::is_road_downgrade(*cobble, *dirt));
    CHECK_FALSE(sim::is_road_downgrade(*dirt, *cobble));
    CHECK_FALSE(sim::same_road_family(*cobble, *dirt));

    // Check all road/ramp/bridge entries 2500..2537.
    for (std::uint32_t id = 2500; id <= 2537; ++id) {
        const sim::BuildingType* t = o->types->find(sim::TypeId{id});
        if (t == nullptr) {
            continue;
        }
        if (t->prodtype == 198) { // BRUECKE
            CHECK_FALSE(sim::is_road_family(*t));
            CHECK_FALSE(sim::same_road_family(*t, *t));
        } else if ((t->flags_word & 0x100U) != 0) {
            CHECK(sim::is_road_family(*t));
            if (id < 2520) {
                CHECK(sim::same_road_family(*t, *cobble));
                CHECK(sim::is_road_downgrade(*t, *dirt));
            } else {
                CHECK(sim::same_road_family(*t, *dirt));
                CHECK_FALSE(sim::is_road_downgrade(*dirt, *t));
            }
        }
    }
}
