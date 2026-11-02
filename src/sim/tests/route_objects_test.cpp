// Object-aware, history-independent ship routing (MAP-NAV-3 package N3; docs/sim/ship-navigation.md
// 12.13). A real World with a dead-end channel through an island: the production RouteCache blocks
// the placed objects per the evidenced predicate (((h + base) & 0xff) > 19 with the tile's item
// height byte as base), and follows every change of them (placement, demolition, a height byte,
// autonomous island growth) through the tile layers' dirty rectangles without a whole-index
// rebuild. A cache that was warm before the change, a clean rebuild, a copied world and a saved and
// loaded world must agree on the same authoritative state.
//
// The mutants this file is built to kill, with different checks
// (docs/sim/ship-navigation.md 12.13):
//   M1 `ship_route_terrain_options().objects_block = false`: "route objects: ... at load" and every
//      check that expects a closed channel fail; the cached-vs-clean comparisons cannot see it
//      (both sides ignore the objects).
//   M2 `RouteCache::sync` that does nothing but build: only the checks that change the world AFTER
//      the cache is warm fail (the load-time ones pass), and the cached-vs-clean comparison fails.
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/route.hpp>
#include <anno/sim/route_cache.hpp>
#include <anno/sim/route_world.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/ship.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "command_helpers.hpp"
#include "island_helpers.hpp"
#include "ship_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
namespace ct = command_test;
using ship_test::kDonkey;
using ship_test::kScout;
using ship_test::kShip;

namespace {

constexpr std::int32_t kIslandX = 200, kIslandY = 96; // 8-aligned: the builder's block shortcut
constexpr std::uint32_t kIslandSize = 64;
constexpr std::int32_t kChannelY0 = 126, kChannelY1 = 128; // water rows of the channel
constexpr std::int32_t kWallX = 220;                       // the plug sits here
constexpr std::int32_t kStartX = 190, kStartY = 127;       // west of the island
constexpr std::int32_t kGoalX = 238, kGoalY = 127;         // inside the channel, beyond the plug

constexpr std::uint32_t kWall = 9101;    // 1 x 1, h = trunc(0.8 * 25) = 20: blocks alone
constexpr std::uint32_t kLow = 9102;     // 1 x 1, h = trunc(0.13 * 25) = 3: blocks with base >= 17
constexpr std::uint32_t kWall1x3 = 9103; // 1 x 3, h = 20
constexpr std::uint32_t kWall3x1 = 9104; // 3 x 1, h = 20
constexpr std::uint32_t kTower = 9105;   // 1 x 1, h = 50, top 50: raises its block record
constexpr std::uint32_t kCoastWall =
    9106;                              // the 1 x 3 weaver (h = 3) with the coast flag word 0x2087
constexpr std::uint32_t kGrass = 9015; // growing, h = 0 (does not block)
constexpr std::uint32_t kRipeGrass = 9016; // its successor, h = trunc(1.0 * 25) = 25 (blocks)

std::uint32_t f32_bits(float f) {
    std::uint32_t b = 0;
    static_assert(sizeof(b) == sizeof(f));
    std::memcpy(&b, &f, sizeof b);
    return b;
}

void set_scalar(sim::BuildingType& t, float f) {
    if (t.head_raw.size() < 56) {
        t.head_raw.resize(56, 0);
    }
    const std::uint32_t bits = f32_bits(f);
    for (std::uint32_t i = 0; i < 4; ++i) {
        t.head_raw[0x14 + i] = static_cast<std::uint8_t>(bits >> (8 * i));
    }
}

/// The growth types of the synthetic set, the object types above, and the navigating ship.
std::shared_ptr<const sim::TypeRegistry> registry() {
    auto base = syn::make_growth_types();
    REQUIRE_MESSAGE(base.ok(), (base.ok() ? std::string() : base.error()));
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    const sim::BuildingType* proto = nullptr;
    for (const sim::BuildingType& t : base.value().registry->types()) {
        if (t.id.v == kRipeGrass) {
            proto = &t;
        }
    }
    REQUIRE(proto != nullptr);
    for (const sim::BuildingType& t : base.value().registry->types()) {
        sim::BuildingType c = t;
        if (c.id == base.value().weaver) {
            set_scalar(c, 0.13f); // h = 3: blocks only with a base of at least 17
            c.width = 1;          // a 1 x 3 weaver fits across the channel
            c.height = 3;
            // The same building as a coast type (flag word 0x2087, n = 2: the shore test is
            // skipped), which BuildCmd can place across open water: its item height is the
            // window's level.
            sim::BuildingType coast = c;
            coast.id = sim::TypeId{kCoastWall};
            coast.flags_word = 0x2087;
            b.add(std::move(coast));
        } else if (c.id.v == kGrass) {
            set_scalar(c, 0.0f);
        } else if (c.id.v == kRipeGrass) {
            set_scalar(c, 1.0f);
        }
        b.add(std::move(c));
    }
    const struct {
        std::uint32_t id, w, h;
        float scalar;
    } objects[] = {{kWall, 1, 1, 0.8f},
                   {kLow, 1, 1, 0.13f},
                   {kWall1x3, 1, 3, 0.8f},
                   {kWall3x1, 3, 1, 0.8f},
                   {kTower, 1, 1, 2.0f}};
    for (const auto& o : objects) {
        sim::BuildingType c = *proto;
        c.id = sim::TypeId{o.id};
        c.width = o.w;
        c.height = o.h;
        set_scalar(c, o.scalar);
        b.add(std::move(c));
    }
    sim::FigureType ship{kShip, 0, 0, 0, 0};
    ship.bewohntype = 113;
    ship.nav = sim::NavFacts{154618816U, 472446400U, 6000U, 4U, 2U, 0U, 1280U};
    b.figure_type(ship)
        .figure_type(sim::FigureType{kScout, 0, 0, 0, 0})
        .figure_type(sim::FigureType{kDonkey, 0, 0, 0, 0});
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

std::int32_t tile_c(int t) {
    return t * 256 + 128;
}

struct Item {
    std::int32_t x, y;
    std::uint32_t object;
    std::uint8_t height = 0;
    std::uint8_t phase = 0;
};

/// The island is heights 30 (land) with a 3 tile wide channel from its west edge to x = 240.
/// `vertex_water` carves extra water vertices (tests that need open water).
sim::ScenarioSpec channel_spec(const std::vector<Item>& items, bool all_water = false,
                               const std::vector<sim::ScenarioSpec::BuildingSpec>& buildings = {},
                               std::uint8_t land = 30) {
    sim::ScenarioSpec spec = syn::empty_scenario(11);
    sim::ScenarioSpec::PlayerSpec p;
    p.id = sim::PlayerId{0};
    p.money = 1000;
    spec.players.push_back(p);
    sim::ScenarioSpec::FigureSpec f;
    f.kind = kShip;
    f.owner = sim::PlayerId{0};
    f.pos = sim::FixedPos{tile_c(kStartX), tile_c(kStartY)};
    f.dir16 = 8;
    spec.figures.push_back(f);
    sim::ScenarioSpec::IslandSpec is =
        syn::plain_island(0, kIslandX, kIslandY, kIslandSize, kIslandSize);
    is.owner_runs = {{0, kIslandSize * kIslandSize}};
    is.cities.push_back(syn::plain_city(0, 0));
    is.cities[0].buildings = buildings;
    std::vector<std::uint8_t> height(std::size_t{kIslandSize} * kIslandSize,
                                     all_water ? std::uint8_t{0} : land);
    if (!all_water) {
        for (std::uint32_t vz = 31; vz <= 32; ++vz) {
            for (std::uint32_t vx = 0; vx <= 40; ++vx) {
                height[vz * kIslandSize + vx] = 0;
            }
        }
    }
    is.terrain = sim::make_terrain(std::move(height), {}, {});
    for (const Item& it : items) {
        sim::ScenarioSpec::TileObjectSpec o;
        o.index = static_cast<std::uint32_t>((it.y - kIslandY) * std::int32_t{kIslandSize} +
                                             (it.x - kIslandX));
        o.object = it.object;
        o.phase = it.phase;
        o.height = it.height;
        is.objects.push_back(o);
    }
    spec.islands.push_back(std::move(is));
    return spec;
}

sim::World make_world(const sim::ScenarioSpec& spec, sim::SimConfig cfg = {}) {
    auto w = sim::World::create(spec, registry(), std::move(cfg), {}, {}, sim::serial_executor());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

sim::Island& island_of(sim::World& w) {
    return *sim::WorldTestAccess::islands(w)[0];
}

sim::RouteCache& cache_of(sim::World& w) {
    return sim::WorldTestAccess::route_cache(w);
}

sim::Slot idx_of(sim::World& w, std::int32_t x, std::int32_t y) {
    const sim::Slot i = island_of(w).tiles.index_of(x, y);
    REQUIRE(i != sim::kNoSlot);
    return i;
}

/// The route the production planner would compute for the ship's start and the goal tile, on the
/// production cache after sync (the exact call sequence of ShipMover::visit).
sim::RouteResult probe(sim::World& w, std::int32_t gx = kGoalX, std::int32_t gy = kGoalY) {
    cache_of(w).sync(w);
    sim::RouteQuery q;
    q.start_x = kStartX;
    q.start_z = kStartY;
    q.goal_x_q8 = tile_c(gx);
    q.goal_z_q8 = tile_c(gy);
    return sim::plan_route(cache_of(w).get(w), q, sim::RouteLimits{},
                           sim::RouteCostTable::original());
}

bool reaches_goal(const sim::RouteResult& r) {
    return r.status == sim::RouteStatus::Planned && r.end_x == kGoalX && r.end_z == kGoalY;
}

sim::Command demolish(std::int32_t x, std::int32_t y) {
    return sim::Command{sim::PlayerId{0},
                        sim::DemolishCmd{sim::CityRef{sim::IslandId{0}, sim::CityNo{0}}, {{x, y}}}};
}

/// The cached index equals a clean rebuild; a copied world (empty cache, built from scratch) and a
/// saved and loaded world agree with it word for word and plan the same route.
void expect_history_independent(sim::World& w) {
    const sim::RouteResult cached = probe(w);
    CHECK(cache_of(w).get(w).verify_against_clean_rebuild({}) == "");
    sim::World copy = w;
    const sim::RouteResult from_copy = probe(copy);
    CHECK(cache_of(w).get(w).same_words(cache_of(copy).get(copy)));
    CHECK(cached == from_copy);
    auto loaded = sim::World::load(w.save(), registry());
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World reloaded = std::move(loaded).value();
    CHECK(reloaded.state_hash() == w.state_hash());
    const sim::RouteResult from_load = probe(reloaded);
    CHECK(cache_of(w).get(w).same_words(cache_of(reloaded).get(reloaded)));
    CHECK(cached == from_load);
}

/// A plug of three 1 x 1 walls across the channel at x.
std::vector<Item> plug_at(std::int32_t x, std::uint32_t object = kWall, std::uint8_t height = 0) {
    std::vector<Item> v;
    for (std::int32_t y = kChannelY0; y <= kChannelY1; ++y) {
        v.push_back(Item{x, y, object, height});
    }
    return v;
}

/// Advance until the ship has stopped or `limit` calls; returns the largest x reached (Q24.8).
std::int32_t sail_and_max_x(sim::World& w, int limit) {
    std::int32_t best = 0;
    for (int i = 0; i < limit; ++i) {
        w.advance(100);
        const sim::Figure* f = sim::find_figure(sim::WorldTestAccess::figures(w), sim::FigureId{1});
        REQUIRE(f != nullptr);
        best = f->pos.x > best ? f->pos.x : best;
    }
    return best;
}

} // namespace

TEST_CASE("route objects: a channel plugged at load is closed and an empty one is open") {
    // Objects exist from the start: the cache is built once on a world that already holds them
    // (kills a production cache that ignores objects, mutant M1).
    sim::World open = make_world(channel_spec({}));
    sim::World shut = make_world(channel_spec(plug_at(kWallX)));
    const sim::RouteResult a = probe(open);
    const sim::RouteResult b = probe(shut);
    CHECK(reaches_goal(a));
    CHECK_FALSE(reaches_goal(b));
    CHECK(a != b);
    CHECK(a.runs != b.runs);
    // The plug is the reason: the tiles are blocked in the production source.
    CHECK_FALSE(cache_of(shut).get(shut).source().tile_is_water(kWallX, 127));
    CHECK(cache_of(open).get(open).source().tile_is_water(kWallX, 127));
    CHECK(cache_of(shut).get(shut).verify_against_clean_rebuild({}) == "");
    // The same through the whole stack: the ship under MoveCmd sails into the open channel and
    // stops in front of the plug.
    REQUIRE(open.apply(sim::Command{sim::PlayerId{0},
                                    sim::MoveCmd{sim::FigureId{1}, tile_c(kGoalX), tile_c(kGoalY)}})
                .ok());
    REQUIRE(shut.apply(sim::Command{sim::PlayerId{0},
                                    sim::MoveCmd{sim::FigureId{1}, tile_c(kGoalX), tile_c(kGoalY)}})
                .ok());
    const std::int32_t open_x = sail_and_max_x(open, 150);
    const std::int32_t shut_x = sail_and_max_x(shut, 150);
    CHECK(open_x > tile_c(kWallX + 4));
    CHECK(shut_x < tile_c(kWallX));
    CHECK(shut.check_invariants().empty());
}

TEST_CASE(
    "route objects: placing into a warm cache closes the channel and demolishing reopens it") {
    sim::World w = make_world(channel_spec({}));
    const sim::RouteResult open = probe(w); // the cache is warm and the channel is open
    REQUIRE(reaches_goal(open));
    REQUIRE(cache_of(w).work.index_builds == 1);
    const std::uint64_t builds = cache_of(w).work.index_builds;
    // The production placement writer puts a 1 x 3 wall across the channel (it crosses the coarse
    // cell row boundary 127 | 128: two anchors, one per block).
    const sim::BuildingType* wall = w.types().find(sim::TypeId{kWall1x3});
    REQUIRE(wall != nullptr);
    sim::Island& is = island_of(w);
    static_cast<void>(
        sim::place_object_record(is.tiles, w.types(), *wall, kWallX, kChannelY0, 0, 0));
    CHECK(is.tiles.route_dirty().any());
    const sim::RouteResult shut = probe(w);
    CHECK_FALSE(reaches_goal(shut));
    CHECK(shut != open);
    CHECK(cache_of(w).work.index_builds == builds); // incremental, not a rebuild
    CHECK(cache_of(w).work.index_rect_rebuilds >= 1);
    CHECK_FALSE(is.tiles.route_dirty().any());
    expect_history_independent(w);
    // Demolish through the production command: the clear arm removes the footprint and the cache
    // follows.
    const sim::ApplyResult r = w.apply(demolish(kWallX, 127));
    REQUIRE(r.ok());
    const sim::RouteResult reopened = probe(w);
    CHECK(reaches_goal(reopened));
    CHECK(reopened.runs == open.runs);
    CHECK(reopened.end_x == open.end_x);
    CHECK(cache_of(w).work.index_builds == builds);
    expect_history_independent(w);
}

TEST_CASE("route objects: an unrelated command does not touch the index") {
    sim::World w = make_world(channel_spec(plug_at(kWallX)));
    static_cast<void>(probe(w));
    const sim::NavWork before = cache_of(w).work;
    REQUIRE(before.index_builds == 1);
    // A demolish of an empty owned tile far from the channel succeeds as a command and changes
    // no object.
    REQUIRE(w.apply(demolish(kIslandX + 60, kIslandY + 60)).ok());
    CHECK_FALSE(island_of(w).tiles.route_dirty().any());
    // Advancing without changing anything leaves the counters alone too.
    w.advance(500);
    static_cast<void>(probe(w));
    CHECK(cache_of(w).work.index_builds == before.index_builds);
    CHECK(cache_of(w).work.index_rect_rebuilds == before.index_rect_rebuilds);
    CHECK(cache_of(w).work.index_cells_rebuilt == before.index_cells_rebuilt);
}

TEST_CASE("route objects: real economic commands rebuild nothing or only their own rectangle") {
    // The g2 world of the command tests: one island with a city, tools and wood in stock.
    const syn::SyntheticTypes t = ct::build_types();
    sim::World w = ct::make_world(t, syn::g2_scenario(t));
    sim::RouteCache& cache = cache_of(w);
    cache.sync(w);
    REQUIRE(cache.work.index_builds == 1);
    const sim::NavWork warm = cache.work;
    // Commands that touch no tile: trade settings (city level).
    const sim::GoodsId wool = ct::goods(t, "WOLLE");
    REQUIRE(
        w.apply(ct::trade_slot(0, ct::kCity0, 0, sim::kAppendIndex, wool, 7 * sim::kTon, 40)).ok());
    REQUIRE(w.apply(ct::trade_flag(0, ct::kCity0, 0, wool, 0)).ok());
    cache.sync(w);
    CHECK(cache.work == warm);
    // A real build elsewhere through the production command path: only its own rectangle.
    w.advance(100);
    const sim::ApplyResult built = w.apply(ct::build(0, ct::kCity0, {ct::at(t.weaver, 112, 115)}));
    REQUIRE(built.ok());
    REQUIRE(w.find_building(built.records[0].building) != nullptr);
    CHECK(w.islands()[0]->tiles.route_dirty().any());
    cache.sync(w);
    CHECK(cache.work.index_builds == warm.index_builds);
    CHECK(cache.work.index_rect_rebuilds == warm.index_rect_rebuilds + 1);
    // The footprint plus 8 tiles on every side: far fewer cells than the 8 x 8 of the island.
    const std::uint64_t cells = cache.work.index_cells_rebuilt - warm.index_cells_rebuilt;
    CHECK(cells >= 1);
    CHECK(cells <= 16);
    CHECK_FALSE(w.islands()[0]->tiles.route_dirty().any());
    CHECK(cache.get(w).verify_against_clean_rebuild({}) == "");
    // Demolishing it is local too.
    REQUIRE(w.apply(ct::demolish(0, ct::kCity0, {{112, 115}})).ok());
    cache.sync(w);
    CHECK(cache.work.index_builds == warm.index_builds);
    CHECK(cache.work.index_rect_rebuilds == warm.index_rect_rebuilds + 2);
    CHECK(cache.get(w).verify_against_clean_rebuild({}) == "");
}

TEST_CASE("route objects: island growth closes the channel without any command") {
    // Grass (h 0) stands across the channel; it ripens through the scheduler into the ripe type
    // (h 25), which blocks. The cache is warm before the change.
    std::vector<Item> grass;
    for (std::int32_t y = kChannelY0; y <= kChannelY1; ++y) {
        grass.push_back(Item{kWallX, y, kGrass});
    }
    sim::World w = make_world(channel_spec(grass));
    const sim::RouteResult before = probe(w);
    CHECK(reaches_goal(before)); // grass does not block
    const std::uint64_t builds = cache_of(w).work.index_builds;
    REQUIRE(builds == 1);
    const sim::Slot probe_tile = idx_of(w, kWallX, 127);
    REQUIRE(island_of(w).tiles.object(probe_tile) == kGrass);
    // The ship is ordered after the first advance: its visit runs the production sync path.
    int steps = 0;
    while (island_of(w).tiles.object(probe_tile) != kRipeGrass && steps < 600) {
        w.advance(100);
        ++steps;
    }
    REQUIRE(island_of(w).tiles.object(probe_tile) == kRipeGrass);
    // The ripe object blocks per the predicate (h = 25 > 19) wherever the sweep has reached; the
    // sweep reaches all three tiles of the channel (one chunk column), but be exact about it.
    int ripe = 0;
    for (std::int32_t y = kChannelY0; y <= kChannelY1; ++y) {
        ripe += island_of(w).tiles.object(idx_of(w, kWallX, y)) == kRipeGrass ? 1 : 0;
    }
    CHECK(ripe >= 1);
    // Fully ripened: three ripe tiles plug the channel. Advance until all three are ripe.
    while (ripe < 3 && steps < 1200) {
        w.advance(100);
        ++steps;
        ripe = 0;
        for (std::int32_t y = kChannelY0; y <= kChannelY1; ++y) {
            ripe += island_of(w).tiles.object(idx_of(w, kWallX, y)) == kRipeGrass ? 1 : 0;
        }
    }
    REQUIRE(ripe == 3);
    // Order the ship now: its visit syncs the cache (the production path), and it stays outside.
    REQUIRE(w.apply(sim::Command{sim::PlayerId{0},
                                 sim::MoveCmd{sim::FigureId{1}, tile_c(kGoalX), tile_c(kGoalY)}})
                .ok());
    const std::int32_t reached = sail_and_max_x(w, 120);
    CHECK(reached < tile_c(kWallX));
    CHECK(cache_of(w).work.index_builds == builds);
    CHECK(cache_of(w).work.index_rect_rebuilds >= 1);
    const sim::RouteResult after = probe(w);
    CHECK_FALSE(reaches_goal(after));
    CHECK(after != before);
    expect_history_independent(w);
}

TEST_CASE("route objects: an object across a coarse cell boundary") {
    sim::World w = make_world(channel_spec({}));
    REQUIRE(reaches_goal(probe(w)));
    // A 3 x 1 wall across the x boundary 223 | 224 in the middle row: not a plug, but both cells
    // see their tile blocked, and the anchor records of the two blocks differ.
    const sim::BuildingType* wall = w.types().find(sim::TypeId{kWall3x1});
    REQUIRE(wall != nullptr);
    sim::Island& is = island_of(w);
    static_cast<void>(sim::place_object_record(is.tiles, w.types(), *wall, 222, 127, 0, 0));
    REQUIRE(is.tiles.foot_anchor(idx_of(w, 222, 127)) == 1);
    REQUIRE(is.tiles.foot_anchor(idx_of(w, 224, 127)) == 1); // second block: its own anchor
    static_cast<void>(probe(w));
    const sim::CoastIndex& index = cache_of(w).get(w);
    CHECK_FALSE(index.source().tile_is_water(222, 127));
    CHECK_FALSE(index.source().tile_is_water(223, 127));
    CHECK_FALSE(index.source().tile_is_water(224, 127));
    CHECK(index.verify_against_clean_rebuild({}) == "");
    // Both cells were rebuilt: the masks are those of cells with a blocked tile row.
    CHECK(index.mask(27, 15) != 0);
    CHECK(index.mask(28, 15) != 0);
    // Remove it with the clear arm: both blocks return to the open channel.
    sim::clear_object_record(is.tiles, w.types(), 224, 127);
    static_cast<void>(probe(w));
    CHECK(cache_of(w).get(w).source().tile_is_water(223, 127));
    CHECK(cache_of(w).get(w).verify_against_clean_rebuild({}) == "");
    CHECK(reaches_goal(probe(w)));
}

TEST_CASE("route objects: the item height byte is the base of the predicate") {
    // A short object (h 3) blocks only with a base of at least 17 ((3 + 17) & 0xff = 20 > 19).
    sim::World low = make_world(channel_spec(plug_at(kWallX, kLow, 0)));
    sim::World high = make_world(channel_spec(plug_at(kWallX, kLow, 17)));
    sim::World edge = make_world(channel_spec(plug_at(kWallX, kLow, 16)));
    CHECK(reaches_goal(probe(low)));
    CHECK_FALSE(reaches_goal(probe(high)));
    CHECK(reaches_goal(probe(edge))); // 3 + 16 = 19: not above 19
    CHECK(island_of(high).tiles.item_height(idx_of(high, kWallX, 127)) == 17);
    // The byte reaches the tile of every placed footprint through the loader.
    CHECK(island_of(edge).tiles.item_height(idx_of(edge, kWallX, 126)) == 16);
    // The byte wraps: 3 + 253 = 256 -> 0 is free, 3 + 252 = 255 blocks.
    sim::World wrap = make_world(channel_spec(plug_at(kWallX, kLow, 253)));
    CHECK(reaches_goal(probe(wrap)));
    sim::World wrap2 = make_world(channel_spec(plug_at(kWallX, kLow, 252)));
    CHECK_FALSE(reaches_goal(probe(wrap2)));
    // At run time on a warm cache: raising the byte closes the channel, lowering it reopens it.
    sim::World w = make_world(channel_spec(plug_at(kWallX, kLow, 0)));
    REQUIRE(reaches_goal(probe(w)));
    const std::uint64_t builds = cache_of(w).work.index_builds;
    for (std::int32_t y = kChannelY0; y <= kChannelY1; ++y) {
        island_of(w).tiles.set_item_height(idx_of(w, kWallX, y), 20);
    }
    CHECK_FALSE(reaches_goal(probe(w)));
    expect_history_independent(w);
    for (std::int32_t y = kChannelY0; y <= kChannelY1; ++y) {
        island_of(w).tiles.set_item_height(idx_of(w, kWallX, y), 0);
    }
    CHECK(reaches_goal(probe(w)));
    CHECK(cache_of(w).work.index_builds == builds);
    // A height byte on a tile WITHOUT an object blocks nothing.
    island_of(w).tiles.set_item_height(idx_of(w, 215, 127), 200);
    CHECK(reaches_goal(probe(w)));
    CHECK(cache_of(w).get(w).verify_against_clean_rebuild({}) == "");
    // Clearing an object keeps the byte (the original's clear arm), and a later placement of a
    // short object on that tile with the kept byte blocks.
    island_of(w).tiles.set_item_height(idx_of(w, kWallX, 127), 17);
    sim::clear_object_record(island_of(w).tiles, w.types(), kWallX, 127);
    CHECK(island_of(w).tiles.item_height(idx_of(w, kWallX, 127)) == 17);
    CHECK(island_of(w).tiles.object(idx_of(w, kWallX, 127)) == 0);
}

TEST_CASE("route objects: an object next to the shore band changes the band of its neighbours") {
    // An all-water island: a tall object (top 50) raises its block record (anchor rule), the block
    // straddles the shore threshold, and the band appears in the block and its 8 neighbours.
    sim::World w = make_world(channel_spec({}, true));
    static_cast<void>(probe(w, 230, 140));
    const sim::CoastIndex& index = cache_of(w).get(w);
    for (std::int32_t cz = 15; cz <= 17; ++cz) {
        for (std::int32_t cx = 27; cx <= 29; ++cx) {
            REQUIRE(index.type(cx, cz) == 0);
        }
    }
    const std::uint64_t builds = cache_of(w).work.index_builds;
    const sim::BuildingType* tower = w.types().find(sim::TypeId{kTower});
    REQUIRE(tower != nullptr);
    static_cast<void>(
        sim::place_object_record(island_of(w).tiles, w.types(), *tower, 230, 130, 0, 0));
    static_cast<void>(probe(w, 230, 140));
    const sim::CoastIndex& after = cache_of(w).get(w);
    // The tower's block is cell (28, 16); the band covers it and the eight cells around it.
    for (std::int32_t cz = 15; cz <= 17; ++cz) {
        for (std::int32_t cx = 27; cx <= 29; ++cx) {
            CAPTURE(cx);
            CAPTURE(cz);
            CHECK(after.type(cx, cz) == 3);
        }
    }
    CHECK(after.type(26, 16) == 0);
    CHECK(after.type(28, 18) == 0);
    CHECK(after.verify_against_clean_rebuild({}) == "");
    CHECK(cache_of(w).work.index_builds == builds);
    // And back.
    sim::clear_object_record(island_of(w).tiles, w.types(), 230, 130);
    static_cast<void>(probe(w, 230, 140));
    CHECK(cache_of(w).get(w).type(28, 16) == 0);
    CHECK(cache_of(w).get(w).verify_against_clean_rebuild({}) == "");
}

TEST_CASE("route objects: replaced terrain or a replaced tile layer rebuilds the index") {
    sim::World w = make_world(channel_spec({}));
    REQUIRE(reaches_goal(probe(w)));
    REQUIRE(cache_of(w).work.index_builds == 1);
    // Terrain is immutable after load; if it is ever replaced the fingerprint rebuilds everything.
    sim::Island& is = island_of(w);
    is.terrain = sim::make_terrain(std::vector<std::uint8_t>(std::size_t{64} * 64, 30), {}, {});
    CHECK_FALSE(reaches_goal(probe(w)));
    CHECK(cache_of(w).work.index_builds == 2);
    // A whole tile layer assigned over the island marks everything dirty for the cache.
    sim::TileLayer other = is.tiles;
    other.set_object(idx_of(w, kWallX, 127), kWall);
    is.tiles = other;
    CHECK(is.tiles.route_dirty().all());
    static_cast<void>(probe(w));
    CHECK(cache_of(w).work.index_rect_rebuilds >= 1);
    CHECK(cache_of(w).get(w).verify_against_clean_rebuild({}) == "");
}

TEST_CASE("route objects: a world of unusual size and partial cells is bounded and deterministic") {
    // World 77 x 53 tiles (cells 10 x 7, the last row and column partial), an island of 21 x 13
    // tiles at (3, 5): not block aligned, not covering whole cells. The edge behaviour of the
    // original is not established (OQ-NAVC-1); this checks bounds and determinism only.
    sim::ScenarioSpec spec = syn::empty_scenario(5);
    spec.settings.world_width = 77;
    spec.settings.world_height = 53;
    sim::ScenarioSpec::PlayerSpec p;
    p.id = sim::PlayerId{0};
    spec.players.push_back(p);
    sim::ScenarioSpec::FigureSpec f;
    f.kind = kShip;
    f.owner = sim::PlayerId{0};
    f.pos = sim::FixedPos{tile_c(60), tile_c(40)};
    f.dir16 = 8;
    spec.figures.push_back(f);
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 3, 5, 21, 13);
    is.owner_runs = {{0, 21U * 13U}};
    is.cities.push_back(syn::plain_city(0, 0));
    std::vector<std::uint8_t> height(21U * 13U, 30);
    is.terrain = sim::make_terrain(std::move(height), {}, {});
    is.objects.push_back(sim::ScenarioSpec::TileObjectSpec{4, kWall, 0, 0, 3});
    is.objects.push_back(sim::ScenarioSpec::TileObjectSpec{21U * 13U - 1, kWall, 0, 0, 40});
    spec.islands.push_back(std::move(is));
    std::vector<std::uint64_t> hashes;
    std::vector<sim::RouteResult> routes;
    for (int run = 0; run < 2; ++run) {
        sim::World w = make_world(spec);
        REQUIRE(w.check_invariants().empty());
        cache_of(w).sync(w);
        sim::RouteQuery q;
        q.start_x = 60;
        q.start_z = 40;
        q.goal_x_q8 = tile_c(30);
        q.goal_z_q8 = tile_c(30);
        routes.push_back(sim::plan_route(cache_of(w).get(w), q, sim::RouteLimits{},
                                         sim::RouteCostTable::original()));
        // A tower in a partial cell, 300 ms of play, a clean-rebuild comparison (no order issued).
        const sim::BuildingType* tower = w.types().find(sim::TypeId{kTower});
        REQUIRE(tower != nullptr);
        static_cast<void>(
            sim::place_object_record(island_of(w).tiles, w.types(), *tower, 20, 15, 0, 0));
        w.advance(300);
        cache_of(w).sync(w);
        CHECK(cache_of(w).get(w).verify_against_clean_rebuild({}) == "");
        CHECK(cache_of(w).work.index_builds == 1);
        hashes.push_back(w.state_hash());
    }
    CHECK(hashes[0] == hashes[1]);
    CHECK(routes[0] == routes[1]);
}

TEST_CASE("route objects: the height byte is validated and part of the state") {
    sim::World w = make_world(channel_spec(plug_at(kWallX, kLow, 17)));
    CHECK(w.check_invariants().empty());
    const std::uint64_t h0 = w.state_hash();
    // Bits 8..31 of aux are not modelled: a state that sets one is refused with a specific message.
    sim::World bad = w;
    sim::WorldTestAccess::islands(bad)[0]->tiles.set_aux(5, 0x100);
    const std::string problem = bad.check_invariants();
    CHECK(problem.find("islands[0].tiles.aux[5]") != std::string::npos);
    CHECK(problem.find("bits 8..31") != std::string::npos);
    // The byte itself is hashed, saved and reloaded.
    sim::World changed = w;
    sim::WorldTestAccess::islands(changed)[0]->tiles.set_item_height(idx_of(changed, kWallX, 127),
                                                                     18);
    CHECK(changed.check_invariants().empty());
    CHECK(changed.state_hash() != h0);
    auto loaded = sim::World::load(changed.save(), registry());
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    CHECK(loaded.value().state_hash() == changed.state_hash());
    CHECK(loaded.value().islands()[0]->tiles.item_height(idx_of(changed, kWallX, 127)) == 18);
}

TEST_CASE("route objects: a loaded building with byte0f 0x30 changes a route") {
    // The weaver type has h = 3. A loaded building carries its byte0f as the height byte of its
    // footprint tiles: 0 leaves the channel open, 0x30 (a coast level, as a founded warehouse has
    // in the original) closes it. (A command-built building has the terrain's level, see the
    // BuildCmd cases below.)
    auto types = syn::make_growth_types();
    REQUIRE(types.ok());
    sim::ScenarioSpec::BuildingSpec low;
    low.type = types.value().weaver;
    low.x = kWallX;
    low.y = kChannelY0;
    sim::ScenarioSpec::BuildingSpec high = low;
    high.byte0f = 0x30;
    sim::World open = make_world(channel_spec({}, false, {low}));
    sim::World shut = make_world(channel_spec({}, false, {high}));
    CHECK(open.check_invariants().empty());
    CHECK(shut.check_invariants().empty());
    CHECK(island_of(shut).tiles.item_height(idx_of(shut, kWallX, 127)) == 0x30);
    CHECK(island_of(open).tiles.item_height(idx_of(open, kWallX, 127)) == 0);
    CHECK(reaches_goal(probe(open)));
    CHECK_FALSE(reaches_goal(probe(shut)));
    expect_history_independent(shut);
}

TEST_CASE(
    "route objects: many pseudo-random changes keep the incremental index equal to a clean one") {
    // Deterministic LCG; tiles biased to cell edges (local x or z = 0, 7, 8, 15, ... 63) and island
    // borders; object, anchor and height byte changes, batches of 6, a comparison after each batch.
    sim::World w = make_world(channel_spec({}));
    static_cast<void>(probe(w));
    std::uint64_t state = 0x9e3779b97f4a7c15ULL;
    const auto next = [&state]() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(state >> 33);
    };
    const std::int32_t edges[] = {0,  1,  6,  7,  8,  9,  15, 16, 23, 24,
                                  31, 32, 39, 40, 47, 48, 55, 56, 62, 63};
    const std::uint32_t objects[] = {0,        kWall,    kLow,   kTower,
                                     kWall1x3, kWall3x1, kGrass, kRipeGrass};
    const std::uint8_t heights[] = {0, 1, 16, 17, 20, 100, 255};
    sim::Island& is = island_of(w);
    std::uint64_t rebuilds = 0;
    for (int batch = 0; batch < 150; ++batch) {
        for (int k = 0; k < 6; ++k) {
            const bool edge_x = next() % 4 != 0;
            const bool edge_z = next() % 4 != 0;
            const std::int32_t lx =
                edge_x ? edges[next() % 20] : static_cast<std::int32_t>(next() % 64);
            const std::int32_t lz =
                edge_z ? edges[next() % 20] : static_cast<std::int32_t>(next() % 64);
            const sim::Slot idx = is.tiles.index_of(kIslandX + lx, kIslandY + lz);
            REQUIRE(idx != sim::kNoSlot);
            switch (next() % 3) {
            case 0:
                is.tiles.set_object(idx, objects[next() % 8]);
                break;
            case 1:
                is.tiles.set_foot_anchor(idx, static_cast<std::uint8_t>(next() % 2));
                break;
            default:
                is.tiles.set_item_height(idx, heights[next() % 7]);
                break;
            }
        }
        static_cast<void>(probe(w)); // sync
        const std::string diff = cache_of(w).get(w).verify_against_clean_rebuild({});
        INFO("batch " << batch);
        REQUIRE(diff == "");
        rebuilds = cache_of(w).work.index_rect_rebuilds;
    }
    CHECK(cache_of(w).work.index_builds == 1);
    CHECK(rebuilds >= 100);
}

TEST_CASE("route objects: two Advances that share a tick still see a change made between them") {
    // advance(1) steps the clock by one millisecond and a tick is two of them, so consecutive
    // Advances share a tick (as do advance(0) steps). The cache is warm, an object appears between
    // two such Advances, and the very next ship visit must rebuild the changed rectangle.
    for (const std::uint32_t parity : {0U, 1U}) {
        sim::World w = make_world(channel_spec({}));
        for (std::uint32_t i = 0; i < parity; ++i) {
            w.advance(1);
        }
        w.advance(1); // a ship visit: builds the index
        w.advance(0); // and one that shares its tick
        REQUIRE(cache_of(w).work.index_builds == 1);
        const std::uint64_t rects = cache_of(w).work.index_rect_rebuilds;
        for (std::int32_t y = kChannelY0; y <= kChannelY1; ++y) {
            island_of(w).tiles.set_object(idx_of(w, kWallX, y), kWall);
        }
        REQUIRE(island_of(w).tiles.route_dirty().any());
        w.advance(0); // same tick as the previous visit: must still compare and rebuild
        CHECK(cache_of(w).work.index_rect_rebuilds > rects);
        CHECK_FALSE(island_of(w).tiles.route_dirty().any());
        expect_history_independent(w);
    }
}

namespace {

sim::Command build_wall(std::int32_t x, std::int32_t y, std::uint32_t type = kCoastWall) {
    return sim::Command{sim::PlayerId{0},
                        sim::BuildCmd{sim::CityRef{sim::IslandId{0}, sim::CityNo{0}},
                                      {sim::PlacementWire{sim::TypeId{type}, x, y, 0, 0, 0}}}};
}

sim::World make_world_with(const sim::ScenarioSpec& spec, sim::Seams seams) {
    auto w = sim::World::create(spec, registry(), {}, {}, std::move(seams), sim::serial_executor());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

} // namespace

TEST_CASE(
    "route objects: a building placed by BuildCmd closes the channel through its real level") {
    // Land of level 0x30 with the water channel (vertex rows 31 and 32 are 0). The coast type
    // (1 x 3, h = 3) placed across the channel at (kWallX, 126) has the window rows 30..33, whose
    // maximum is 0x30: the command stores 0x30 in byte0f and in the three tiles, and the object
    // predicate ((3 + 0x30) & 0xff) > 19 closes the channel. Evidence for the blocking rule:
    // docs/sim/ship-navigation.md 12.13; for the height: docs/sim/ship-placement.md R5.
    const sim::ScenarioSpec spec = channel_spec({}, false, {}, 0x30);
    sim::World w = make_world(spec);
    REQUIRE(reaches_goal(probe(w)));
    const std::uint64_t builds = cache_of(w).work.index_builds;
    const sim::ApplyResult r = w.apply(build_wall(kWallX, kChannelY0));
    REQUIRE_MESSAGE(r.ok(), "got " << sim::reject_name(r.status));
    const sim::Building* b = w.find_building(r.records.at(0).building);
    REQUIRE(b != nullptr);
    CHECK(b->byte0f == 0x30);
    for (std::int32_t y = kChannelY0; y <= kChannelY1; ++y) {
        CHECK(island_of(w).tiles.item_height(idx_of(w, kWallX, y)) == 0x30);
    }
    CHECK(island_of(w).tiles.route_dirty().any());
    CHECK_FALSE(reaches_goal(probe(w)));
    CHECK(cache_of(w).work.index_builds == builds); // incremental: no whole-index rebuild
    CHECK(cache_of(w).get(w).verify_against_clean_rebuild({}) == "");
    expect_history_independent(w);

    // Control 1: a legal site that is not on the route (land beside the channel) builds with the
    // same level and leaves the route open.
    sim::World beside = make_world(spec);
    REQUIRE(reaches_goal(probe(beside)));
    REQUIRE(beside.apply(build_wall(kWallX, kIslandY + 4)).ok());
    CHECK(island_of(beside).tiles.item_height(idx_of(beside, kWallX, kIslandY + 5)) == 0x30);
    CHECK(reaches_goal(probe(beside)));
    expect_history_independent(beside);

    // Control 2 (the cause): the same command under the deliberately installed stand-in stores
    // height 0, the same building does not close the channel.
    sim::Seams stand_in;
    stand_in.placement_rule = sim::stand_in_placement_seam();
    sim::World zero = make_world_with(spec, stand_in);
    REQUIRE(reaches_goal(probe(zero)));
    const sim::ApplyResult z = zero.apply(build_wall(kWallX, kChannelY0));
    REQUIRE(z.ok());
    CHECK(zero.find_building(z.records.at(0).building)->byte0f == 0);
    CHECK(reaches_goal(probe(zero)));
}

TEST_CASE("route objects: a BuildCmd between two Advances that share a tick is seen at once") {
    // The cache is warm, advance(1) and advance(0) share a tick, a BuildCmd lands between them:
    // the next visit must rebuild exactly the dirty rectangle and the route is closed.
    for (const std::uint32_t parity : {0U, 1U}) {
        sim::World w = make_world(channel_spec({}, false, {}, 0x30));
        for (std::uint32_t i = 0; i < parity; ++i) {
            w.advance(1);
        }
        w.advance(1);
        w.advance(0);
        REQUIRE(cache_of(w).work.index_builds == 1);
        const std::uint64_t rects = cache_of(w).work.index_rect_rebuilds;
        REQUIRE(w.apply(build_wall(kWallX, kChannelY0)).ok());
        REQUIRE(island_of(w).tiles.route_dirty().any());
        w.advance(0); // same tick as the previous visit
        CHECK(cache_of(w).work.index_rect_rebuilds > rects);
        CHECK_FALSE(island_of(w).tiles.route_dirty().any());
        CHECK_FALSE(reaches_goal(probe(w)));
        expect_history_independent(w);
        // Demolition (the clear arm keeps the height byte, faithful) re-syncs the cache and equals
        // a clean rebuild.
        const std::uint64_t after_build = cache_of(w).work.index_rect_rebuilds;
        REQUIRE(w.apply(demolish(kWallX, kChannelY0)).ok());
        REQUIRE(island_of(w).tiles.route_dirty().any());
        w.advance(0);
        CHECK(cache_of(w).work.index_rect_rebuilds > after_build);
        CHECK(cache_of(w).get(w).verify_against_clean_rebuild({}) == "");
        expect_history_independent(w);
    }
}
