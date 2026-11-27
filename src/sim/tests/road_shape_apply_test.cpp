// ROADS-2b: the reshaper wired into the road drag's commit and its preview (World::shape_road_path,
// docs/BUILDINGSOBJECTS.md part B 10.1 items 7-8, AnnoWelt 0x100c9d90). Expected pieces are derived
// by hand from the table (mask N=1, E=2, S=4, W=8).
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/extension_context.hpp>
#include <anno/sim/road_drag.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

constexpr std::int32_t kX0 = 100;
constexpr std::int32_t kY0 = 200;
constexpr std::uint32_t kSize = 32;
constexpr sim::CityRef kCity0{sim::IslandId{0}, sim::CityNo{0}};

// The dirt family as the original numbers it: straight 2520 (flat class 0x4186), corner 2525,
// cross 2526, T 2527, end 2528 (slope class 0x1186); STRASSE 193, 5 gold.
std::shared_ptr<const sim::TypeRegistry> dirt_registry() {
    sim::TypeRegistry::Builder b;
    for (const auto& [id, flags] : std::vector<std::pair<std::uint32_t, std::uint32_t>>{
             {2520, 0x4186}, {2525, 0x1186}, {2526, 0x1186}, {2527, 0x1186}, {2528, 0x1186}}) {
        sim::BuildingType t;
        t.id = sim::TypeId{id};
        t.prodtype = 193;
        t.flags_word = flags;
        t.width = 1;
        t.height = 1;
        t.cost_gold = 5;
        t.head_raw.assign(56, 0);
        t.head_raw[0x30] = 3;
        b.add(t);
    }
    return b.build().value();
}

sim::World road_world(std::uint8_t enabled) {
    sim::ScenarioSpec s = syn::empty_scenario(77);
    s.settings.world_width = 512;
    s.settings.world_height = 512;
    s.players.resize(1);
    s.players[0].id = sim::PlayerId{0};
    s.players[0].money = 10000;
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, kX0, kY0, kSize, kSize);
    is.owner_runs = {{0U, kSize * kSize}};
    is.cities.push_back(syn::plain_city(0, 0));
    std::vector<std::uint8_t> h(kSize * kSize, 0x30); // level 48: the slope pieces' 32 is met
    is.terrain = sim::make_terrain(h, std::vector<std::uint16_t>(h.size(), 0),
                                   std::vector<std::uint8_t>(h.size(), 0));
    s.islands.push_back(std::move(is));
    sim::ExtensionContext ext;
    ext.road_rules.enabled = enabled;
    auto w = sim::World::create(s, dirt_registry(), {}, {}, {}, sim::serial_executor(), ext);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

struct Piece {
    std::uint32_t id;
    std::uint8_t rotation;
};

Piece at(const sim::World& w, std::int32_t dx, std::int32_t dy) {
    const sim::Island* is = w.island_at(kX0 + dx, kY0 + dy);
    REQUIRE(is != nullptr);
    const sim::Slot idx = is->tile_index(kX0 + dx, kY0 + dy);
    REQUIRE(idx != sim::kNoSlot);
    return {is->tiles.object(idx), is->tiles.foot_rotation(idx)};
}

sim::BuildCmd drag(sim::TileXY a, sim::TileXY b) {
    return sim::make_road_drag_cmd(kCity0, sim::TypeId{2520}, {kX0 + a.x, kY0 + a.y},
                                   {kX0 + b.x, kY0 + b.y});
}

} // namespace

TEST_CASE("road shape applied: the owner's L turns its legs and gets one corner") {
    sim::World w = road_world(1);
    // (2,2) -> (5,4): x first (|dx| 3 > |dy| 2), then south: (2,2) (3,2) (4,2) (5,2) (5,3) (5,4).
    const sim::BuildCmd cmd = drag({2, 2}, {5, 4});
    const std::vector<sim::PlacementWire> preview = w.shape_road_path(kCity0, cmd.placements);
    REQUIRE(w.apply({sim::PlayerId{0}, cmd}).ok());
    CHECK(at(w, 2, 2).id == 2528); // end cap open to the east (mask 2)
    CHECK(at(w, 2, 2).rotation == 0);
    CHECK(at(w, 3, 2).id == 2520); // E+W straight (mask 10)
    CHECK(at(w, 3, 2).rotation == 0);
    CHECK(at(w, 5, 2).id == 2525); // W+S corner (mask 12)
    CHECK(at(w, 5, 2).rotation == 3);
    CHECK(at(w, 5, 3).id == 2520); // N+S straight (mask 5)
    CHECK(at(w, 5, 3).rotation == 3);
    CHECK(at(w, 5, 4).id == 2528); // end cap open to the north (mask 1)
    CHECK(at(w, 5, 4).rotation == 1);
    // Preview == commit: every record the preview names is the piece the commit left there.
    REQUIRE(preview.size() == cmd.placements.size());
    for (const sim::PlacementWire& p : preview) {
        const Piece got = at(w, p.x - kX0, p.y - kY0);
        CHECK(got.id == p.type.v);
        CHECK(got.rotation == p.rotation);
    }
}

TEST_CASE("road shape applied: laying beside an existing road rewrites the neighbour") {
    sim::World w = road_world(1);
    REQUIRE(w.apply({sim::PlayerId{0}, drag({2, 2}, {6, 2})}).ok()); // an E-W run
    CHECK(at(w, 4, 2).id == 2520);
    // A one-tile road north of (4,2): the new tile is an end cap open to the south (mask 4), and
    // (4,2) becomes a T with W, E and N (mask 11).
    const sim::BuildCmd stub = drag({4, 1}, {4, 1});
    const std::vector<sim::PlacementWire> preview = w.shape_road_path(kCity0, stub.placements);
    REQUIRE(preview.size() == 2);
    const sim::ApplyResult r = w.apply({sim::PlayerId{0}, stub});
    REQUIRE(r.ok());
    CHECK(r.records.size() == 2);
    CHECK(at(w, 4, 1).id == 2528);
    CHECK(at(w, 4, 1).rotation == 3);
    CHECK(at(w, 4, 2).id == 2527);
    CHECK(at(w, 4, 2).rotation == 2);
    CHECK(preview[1].type.v == 2527);
}

TEST_CASE("road shape applied: without road_rules the drag places what it was given") {
    sim::World w = road_world(0);
    REQUIRE(w.apply({sim::PlayerId{0}, drag({2, 2}, {5, 4})}).ok());
    CHECK(at(w, 2, 2).id == 2520);
    CHECK(at(w, 5, 2).id == 2520);
    const sim::BuildCmd cmd = drag({8, 8}, {9, 9});
    CHECK(w.shape_road_path(kCity0, cmd.placements) == cmd.placements);
}
