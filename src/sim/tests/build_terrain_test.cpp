// MAP-BUILD-1 packages C and D1: the real command path. A BuildCmd through World::apply decides
// the site by the terrain (docs/sim/ship-placement.md R1-R6) and stores the server-derived height
// as Building::byte0f and as the aux low byte of every footprint tile, never the wire byte 8.
//
// Every refusal here is a terrain (or occupancy) refusal and nothing else: the issuer owns the
// city, the funds are plentiful and the footprint is on the city's tiles, and each refused
// command has a control with the same command on legal terrain that succeeds. A refused command
// leaves the whole save unchanged.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/seams.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "command_helpers.hpp"
#include "island_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
namespace ct = command_test;

namespace {

constexpr std::int32_t kX0 = 100, kY0 = 200;
constexpr std::uint32_t kSize = 16;
constexpr sim::CityRef kC0{sim::IslandId{0}, sim::CityNo{0}};
constexpr sim::CityRef kC1{sim::IslandId{0}, sim::CityNo{1}};

/// The registry of these tests: the build types plus data-defined types of other classes, sizes
/// and flag words (all cost 100 gold and no goods, so funds are never the reason of a refusal).
struct Kit {
    syn::SyntheticTypes base = ct::build_types();
    std::shared_ptr<const sim::TypeRegistry> reg;
    sim::TypeId house{9601};    // 2 x 2, flag word 0x84, PRODTYPE 97
    sim::TypeId trapper{9602};  // 2 x 3, 0x84
    sim::TypeId farm3{9603};    // 3 x 3, 0x84
    sim::TypeId wide{9604};     // 5 x 2, 0x0084: a second data-defined ordinary type
    sim::TypeId plain4{9605};   // 2 x 2, 0x0004
    sim::TypeId coast{9606};    // 4 x 4, 0x2087
    sim::TypeId coast1{9607};   // 1 x 3, 0x2087 (n = 2: the shore test is skipped)
    sim::TypeId mountain{9608}; // 2 x 2, 0x3096: class 0x3000, Unsupported
    sim::TypeId covered{9609};  // 2 x 2, 0x0094: flag 0x10, Unsupported
    sim::TypeId odd_bit{9610};  // 2 x 2, 0x0184: a bit the rule does not know, Unsupported
    sim::TypeId fixed{9611};    // tile object, flag word 0: nothing may be built over it
    sim::TypeId stone{9612};    // tile object, flag 0x20: removable
    sim::TypeId tree{9613};     // tile object, flag 0x20: removable

    Kit() {
        sim::TypeRegistry::Builder b;
        b.walk_tables(sim::synthetic_walk_tables());
        for (const sim::BuildingType& t : base.registry->types()) {
            b.add(t);
        }
        const auto copy = [&](sim::TypeId from, sim::TypeId id, std::uint32_t w, std::uint32_t h,
                              std::uint32_t flags) {
            sim::BuildingType t = *base.registry->find(from);
            t.id = id;
            t.width = w;
            t.height = h;
            t.flags_word = flags;
            t.cost_gold = 100;
            t.cost_goods.clear();
            b.add(std::move(t));
        };
        copy(base.pioneer_house, house, 2, 2, 0x84);
        copy(base.fur_trapper, trapper, 2, 3, 0x84);
        copy(base.farm, farm3, 3, 3, 0x84);
        copy(base.farm, wide, 5, 2, 0x84);
        copy(base.pioneer_house, plain4, 2, 2, 0x0004);
        copy(base.kontor, coast, 4, 4, 0x2087);
        copy(base.kontor, coast1, 1, 3, 0x2087);
        copy(base.pioneer_house, mountain, 2, 2, 0x3096);
        copy(base.pioneer_house, covered, 2, 2, 0x0094);
        copy(base.pioneer_house, odd_bit, 2, 2, 0x0184);
        b.add_plain(fixed, 291).add_plain(stone, 323).add_plain(tree, 225);
        b.flags_word(stone, 0x3a).flags_word(tree, 0x29);
        auto built = b.build();
        REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
        reg = built.value();
    }
};

const Kit& kit() {
    static const Kit k;
    return k;
}

/// One 16 x 16 island at (100, 200) owned by city 0 of player 0 (and, with `split`, columns 8..15
/// by city 1 of player 1), flat terrain of `level` unless `height` says otherwise.
struct Site {
    std::vector<std::uint8_t> height = std::vector<std::uint8_t>(kSize * kSize, 0x30);
    bool terrain = true;
    bool split = false;
    std::vector<sim::ScenarioSpec::TileObjectSpec> objects;
    std::int64_t money = 10000;
    std::int32_t x0 = kX0, y0 = kY0;
    std::uint32_t size = kSize;
    std::uint32_t world_edge = 512;

    void set(std::uint32_t lx, std::uint32_t ly, std::uint8_t v) { height.at(ly * size + lx) = v; }
    void fill(std::uint32_t lx, std::uint32_t ly, std::uint32_t w, std::uint32_t h,
              std::uint8_t v) {
        for (std::uint32_t y = ly; y < std::min(ly + h, size); ++y) {
            for (std::uint32_t x = lx; x < std::min(lx + w, size); ++x) {
                set(x, y, v);
            }
        }
    }
    void object(std::uint32_t lx, std::uint32_t ly, sim::TypeId id) {
        sim::ScenarioSpec::TileObjectSpec o;
        o.index = ly * size + lx;
        o.object = id.v;
        objects.push_back(o);
    }

    sim::ScenarioSpec spec() const {
        sim::ScenarioSpec s = syn::empty_scenario(77);
        s.settings.world_width = s.settings.world_height = world_edge;
        for (std::uint32_t p = 0; p < (split ? 2U : 1U); ++p) {
            sim::ScenarioSpec::PlayerSpec ps;
            ps.id = sim::PlayerId{p};
            ps.money = money;
            s.players.push_back(ps);
        }
        sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, x0, y0, size, size);
        if (split) {
            for (std::uint32_t y = 0; y < size; ++y) {
                is.owner_runs.emplace_back(0U, size / 2);
                is.owner_runs.emplace_back(1U, size / 2);
            }
        } else {
            is.owner_runs = {{0U, size * size}};
        }
        is.cities.push_back(syn::plain_city(0, 0));
        if (split) {
            is.cities.push_back(syn::plain_city(1, 1));
        }
        if (terrain) {
            is.terrain = sim::make_terrain(height, std::vector<std::uint16_t>(height.size(), 0),
                                           std::vector<std::uint8_t>(height.size(), 0));
        }
        is.objects = objects;
        s.islands.push_back(std::move(is));
        return s;
    }

    sim::World world(sim::Rules rules = {}, sim::Seams seams = {}) const {
        auto w = sim::World::create(spec(), kit().reg, {}, rules, std::move(seams));
        REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
        return std::move(w).value();
    }
};

sim::Command build_at(sim::TypeId type, std::int32_t lx, std::int32_t ly, std::uint8_t rot = 0,
                      std::uint32_t issuer = 0, sim::CityRef city = kC0) {
    return ct::build(issuer, city, {ct::at(type, kX0 + lx, kY0 + ly, rot)});
}

std::int64_t money_of(const sim::World& w, std::uint32_t p = 0) {
    return w.players().find(sim::PlayerId{p})->money;
}

/// The command is refused with `want` and nothing at all changed (bytes, hash, money, events).
void refused(sim::World& w, const sim::Command& c, sim::Reject want) {
    const std::vector<std::uint8_t> before = w.save();
    const std::uint64_t hash = w.state_hash();
    const std::int64_t gold = money_of(w);
    static_cast<void>(w.drain_events());
    const sim::ApplyResult r = w.apply(c);
    CHECK_MESSAGE(r.status == want,
                  "got " << sim::reject_name(r.status) << ", expected " << sim::reject_name(want));
    CHECK(w.state_hash() == hash);
    CHECK(w.save() == before);
    CHECK(money_of(w) == gold);
    // The only event is the host's notice of the refusal (not state, never hashed or saved).
    const std::vector<sim::SimEvent> events = w.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == sim::SimEventKind::CommandRejected);
    CHECK(events[0].code == static_cast<std::uint32_t>(want));
}

/// The footprint tiles of the building (type, origin, rotation) all carry `height`, the ring of
/// tiles around it does not change, and the building's byte0f is `height`.
void check_height(const sim::World& w, sim::TypeId type, std::int32_t lx, std::int32_t ly,
                  std::uint8_t rot, std::uint8_t height, sim::CityRef city = kC0,
                  bool ring = true) {
    const sim::BuildingType& t = *kit().reg->find(type);
    const auto [fw, fh] = sim::footprint(t, rot);
    const sim::Building* b = w.building_at(city, kX0 + lx, kY0 + ly);
    REQUIRE(b != nullptr);
    CHECK(b->type == type);
    CHECK(static_cast<unsigned>(b->byte0f) == height);
    const sim::TileLayer& tiles = w.islands()[0]->tiles;
    for (std::int32_t y = -1; y <= static_cast<std::int32_t>(fh); ++y) {
        for (std::int32_t x = -1; x <= static_cast<std::int32_t>(fw); ++x) {
            const std::int32_t wx = kX0 + lx + x, wy = kY0 + ly + y;
            const sim::Slot idx = tiles.index_of(wx, wy);
            if (idx == sim::kNoSlot) {
                continue;
            }
            const bool inside = x >= 0 && y >= 0 && x < static_cast<std::int32_t>(fw) &&
                                y < static_cast<std::int32_t>(fh);
            INFO("tile " << x << "," << y << " of the footprint");
            if (inside || ring) {
                CHECK(static_cast<unsigned>(tiles.item_height(idx)) == (inside ? height : 0U));
            }
        }
    }
}

/// build_at succeeds, the record is placed with `height`, the world is consistent and a saved and
/// loaded world has the same bytes, building byte and tile bytes (the stored byte is read back,
/// never recomputed).
void built(sim::World& w, sim::TypeId type, std::int32_t lx, std::int32_t ly, std::uint8_t rot,
           std::uint8_t height, std::uint32_t issuer = 0, sim::CityRef city = kC0,
           bool ring = true) {
    const std::int64_t gold = money_of(w, issuer);
    const sim::ApplyResult r = w.apply(build_at(type, lx, ly, rot, issuer, city));
    REQUIRE_MESSAGE(r.ok(), "got " << sim::reject_name(r.status));
    CHECK(r.records.at(0).building.valid());
    CHECK(money_of(w, issuer) == gold - 100);
    check_height(w, type, lx, ly, rot, height, city, ring);
    CHECK(w.check_invariants() == "");
    auto loaded = sim::World::load(w.save(), kit().reg);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    const sim::World& l = loaded.value();
    CHECK(l.state_hash() == w.state_hash());
    check_height(l, type, lx, ly, rot, height, city, ring);
}

} // namespace

TEST_CASE("build terrain: land and sea under the same footprint") {
    // Same command, same owner, same funds, same tiles: only the level differs.
    const Kit& k = kit();
    for (const std::uint8_t level : {std::uint8_t{0x00}, std::uint8_t{0x10}}) {
        INFO("sea level " << unsigned{level});
        Site sea;
        sea.fill(0, 0, kSize, kSize, level);
        sim::World w = sea.world();
        refused(w, build_at(k.house, 3, 3), sim::Reject::TerrainRefused);
    }
    Site land;
    sim::World w = land.world();
    built(w, k.house, 3, 3, 0, 0x30);
    // The smallest accepted level a server-derived height can have is 0x20; 0x10 is sea.
    Site shallow;
    shallow.fill(0, 0, kSize, kSize, 0x20);
    sim::World s = shallow.world();
    built(s, k.house, 3, 3, 0, 0x20);
}

TEST_CASE("build terrain: the level is a multiple of 16 and the item height is that level") {
    const Kit& k = kit();
    for (const std::uint8_t level :
         {std::uint8_t{0x1f}, std::uint8_t{0x21}, std::uint8_t{0x2f}, std::uint8_t{0x31},
          std::uint8_t{0x35}, std::uint8_t{0x3f}, std::uint8_t{0x41}, std::uint8_t{0xff}}) {
        INFO("level " << unsigned{level});
        Site s;
        s.fill(0, 0, kSize, kSize, level);
        sim::World w = s.world();
        refused(w, build_at(k.house, 3, 3), sim::Reject::TerrainRefused);
    }
    for (const std::uint8_t level :
         {std::uint8_t{0x20}, std::uint8_t{0x40}, std::uint8_t{0x70}, std::uint8_t{0xf0}}) {
        INFO("level " << unsigned{level});
        Site s;
        s.fill(0, 0, kSize, kSize, level);
        sim::World w = s.world();
        built(w, k.house, 3, 3, 0, level);
    }
}

TEST_CASE("build terrain: one odd vertex of the window refuses and one just outside does not") {
    const Kit& k = kit();
    // House 2 x 2 at (3,3): the window is the vertices (3..5, 3..5).
    const struct {
        std::uint32_t vx, vy;
        std::uint8_t v;
        bool in_window;
    } rows[] = {
        {3, 3, 0x31, true},  {5, 5, 0x40, true},  {4, 4, 0x2f, true},  {5, 3, 0x00, true},
        {3, 5, 0x50, true},  {6, 3, 0x50, false}, {2, 3, 0x00, false}, {3, 6, 0x00, false},
        {3, 2, 0x70, false}, {6, 6, 0x00, false},
    };
    for (const auto& row : rows) {
        INFO("vertex " << row.vx << "," << row.vy << " = " << unsigned{row.v});
        Site s;
        s.set(row.vx, row.vy, row.v);
        sim::World w = s.world();
        if (row.in_window) {
            refused(w, build_at(k.house, 3, 3), sim::Reject::TerrainRefused);
        } else {
            built(w, k.house, 3, 3, 0, 0x30);
        }
    }
}

TEST_CASE("build terrain: the island edge reads vertex zero and the bounds are exact") {
    const Kit& k = kit();
    Site s;
    // House 2 x 2: its east window column at x = 14 + 2 = 16 = W is the zero column.
    {
        sim::World w = s.world();
        refused(w, build_at(k.house, 14, 3), sim::Reject::TerrainRefused);
        refused(w, build_at(k.house, 3, 14), sim::Reject::TerrainRefused);
        refused(w, build_at(k.house, 15, 3), sim::Reject::OutOfIsland);
        refused(w, build_at(k.house, 3, 15), sim::Reject::OutOfIsland);
        refused(w, build_at(k.house, -1, 3), sim::Reject::OutOfIsland);
        built(w, k.house, 13, 3, 0, 0x30);
        built(w, k.house, 3, 13, 0, 0x30);
        built(w, k.house, 0, 0, 0, 0x30);
    }
}

TEST_CASE("build terrain: a turned non-square footprint uses the turned window") {
    const Kit& k = kit();
    // Trapper 2 wide, 3 high; rotation 1 makes it 3 wide, 2 high. At (5,2): the unturned window is
    // the vertices x 5..7, y 2..5; the turned one x 5..8, y 2..4.
    {
        Site s;
        s.set(5, 5, 0x00); // only in the unturned window
        sim::World w = s.world();
        refused(w, build_at(k.trapper, 5, 2, 0), sim::Reject::TerrainRefused);
        built(w, k.trapper, 5, 2, 1, 0x30);
    }
    {
        Site s;
        s.set(8, 3, 0x00); // only in the turned window
        sim::World w = s.world();
        refused(w, build_at(k.trapper, 5, 2, 1), sim::Reject::TerrainRefused);
        built(w, k.trapper, 5, 2, 0, 0x30);
    }
    // The bounds follow the turn too: at x = 14 the turned footprint (3 wide) leaves the island.
    Site flat;
    sim::World w = flat.world();
    refused(w, build_at(k.trapper, 14, 2, 1), sim::Reject::OutOfIsland);
    built(w, k.trapper, 12, 2, 1, 0x30);
    built(w, k.trapper, 13, 5, 0, 0x30);
}

TEST_CASE("build terrain: scenery under the footprint, removable and not") {
    const Kit& k = kit();
    // A tree and a stone (flag 0x20) are overwritten; a fixed object, an object the registry does
    // not know and an existing building refuse.
    {
        Site s;
        s.object(4, 4, k.tree);
        s.object(5, 4, k.stone);
        sim::World w = s.world();
        built(w, k.house, 4, 4, 0, 0x30);
        const sim::TileLayer& t = w.islands()[0]->tiles;
        CHECK(t.object(t.index_of(kX0 + 4, kY0 + 4)) == k.house.v); // the building's id replaced it
    }
    {
        Site s;
        s.object(5, 5, k.fixed);
        sim::World w = s.world();
        refused(w, build_at(k.house, 4, 4), sim::Reject::Occupied);
        // The control: the same site without the fixed object.
        Site clear;
        sim::World c = clear.world();
        built(c, k.house, 4, 4, 0, 0x30);
    }
    {
        Site s;
        s.object(5, 5, sim::TypeId{4000}); // not in the registry: no default answer
        sim::World w = s.world();
        refused(w, build_at(k.house, 4, 4), sim::Reject::Unsupported);
    }
    {
        Site s;
        sim::World w = s.world();
        built(w, k.house, 4, 4, 0, 0x30);
        refused(w, build_at(k.house, 5, 5), sim::Reject::Occupied);
        refused(w, build_at(k.house, 4, 4), sim::Reject::Occupied);
        built(w, k.house, 6, 4, 0, 0x30, 0, kC0, false); // the neighbour column is free
    }
}

TEST_CASE("build terrain: another owner builds on its own terrain") {
    const Kit& k = kit();
    Site s;
    s.split = true;
    s.fill(8, 0, 8, 16, 0x40);  // city 1's half is a higher plateau
    s.fill(12, 10, 3, 3, 0x10); // with a lake
    sim::World w = s.world();
    built(w, k.house, 2, 2, 0, 0x30);
    built(w, k.house, 9, 2, 0, 0x40, 1, kC1);
    refused(w, build_at(k.house, 12, 10, 0, 1, kC1), sim::Reject::TerrainRefused);
    // The owner rule is separate: city 0 cannot build on city 1's tiles, whatever the terrain.
    refused(w, build_at(k.house, 10, 2, 0, 0, kC0), sim::Reject::TileNotOwned);
    refused(w, build_at(k.house, 2, 6, 0, 1, kC1), sim::Reject::TileNotOwned);
}

TEST_CASE("build terrain: a second data-defined type needs no engine edit") {
    const Kit& k = kit();
    Site s;
    s.fill(10, 8, 3, 6, 0x50); // a higher shelf
    {
        sim::World w = s.world();
        // Flag word 0x0084 with another size (5 x 2), 0x0004 without the 0x80 bit, a 3 x 3 farm.
        built(w, k.wide, 1, 1, 0, 0x30);
        built(w, k.plain4, 1, 4, 0, 0x30);
        built(w, k.farm3, 1, 7, 0, 0x30);
        // Turned (2 x 5 at (10,8)): the window x 10..12, y 8..13 is the shelf.
        built(w, k.wide, 10, 8, 1, 0x50);
    }
    {
        sim::World w = s.world();
        // Unturned (5 x 2 at (9,8)): the window x 9..14 mixes the shelf and the plain.
        refused(w, build_at(k.wide, 9, 8, 0), sim::Reject::TerrainRefused);
    }
    // The flag words outside the two evidenced classes are Unsupported (never a default), on
    // terrain that would be fine for an ordinary type.
    sim::World u = Site{}.world();
    for (const sim::TypeId t : {k.mountain, k.covered, k.odd_bit}) {
        INFO("type " << t.v);
        refused(u, build_at(t, 4, 4), sim::Reject::Unsupported);
    }
    built(u, k.house, 4, 4, 0, 0x30);
}

TEST_CASE("build terrain: the wire byte 8 never decides") {
    const Kit& k = kit();
    {
        Site sea;
        sea.fill(0, 0, kSize, kSize, 0x10);
        sim::World w = sea.world();
        sim::PlacementWire p = ct::at(k.house, kX0 + 3, kY0 + 3);
        p.byte8 = 0x40; // "the client says it is high land"
        p.byte7 = 0x40;
        refused(w, ct::build(0, kC0, {p}), sim::Reject::TerrainRefused);
    }
    {
        Site land;
        sim::World w = land.world();
        sim::PlacementWire p = ct::at(k.house, kX0 + 3, kY0 + 3);
        p.byte8 = 0; // the client says nothing
        REQUIRE(w.apply(ct::build(0, kC0, {p})).ok());
        check_height(w, k.house, 3, 3, 0, 0x30);
        sim::PlacementWire q = ct::at(k.house, kX0 + 8, kY0 + 8);
        q.byte8 = 0xaa;
        q.byte7 = 0x55;
        REQUIRE(w.apply(ct::build(0, kC0, {q})).ok());
        check_height(w, k.house, 8, 8, 0, 0x30);
    }
}

TEST_CASE("build terrain: coast types") {
    const Kit& k = kit();
    // 4 x 4 coast type: rotation 0 needs the whole top vertex row 0x30 and at least n / 2 = 2 of
    // the 5 vertices of the bottom row below 0x20; the maximum & 0xf0 is the height.
    const auto coast_site = [] {
        Site s;
        s.fill(0, 0, kSize, 8, 0x30);
        s.fill(0, 8, kSize, 8, 0x00); // land in the north half, water in the south half
        return s;
    };
    {
        Site s = coast_site();
        sim::World w = s.world();
        built(w, k.coast, 3, 4, 0, 0x30); // shore row y = 4..? see below
    }
    {
        // Window rows 4..8 at (3,4): the top row (y = 4) is 0x30 and the bottom row (y = 8) is 0.
        Site s = coast_site();
        s.set(5, 4, 0x20); // a wrong shore vertex
        sim::World w = s.world();
        refused(w, build_at(k.coast, 3, 4, 0), sim::Reject::TerrainRefused);
    }
    {
        Site s; // all land: the opposite row is not water
        sim::World w = s.world();
        refused(w, build_at(k.coast, 3, 4, 0), sim::Reject::TerrainRefused);
    }
    {
        Site s = coast_site();
        s.set(3, 8, 0x40); // a high vertex makes the window maximum 0x40, not 0x30
        sim::World w = s.world();
        refused(w, build_at(k.coast, 3, 4, 0), sim::Reject::TerrainRefused);
    }
    {
        // Rotation 2 wants the shore at the bottom: the same site is refused turned the wrong way.
        Site s = coast_site();
        sim::World w = s.world();
        refused(w, build_at(k.coast, 3, 4, 2), sim::Reject::TerrainRefused);
        refused(w, build_at(k.coast, 3, 4, 1), sim::Reject::TerrainRefused);
    }
    {
        // The 1-wide coast type skips the shore test and has no level threshold: open sea gives
        // the byte 0x10 (the maximum & 0xf0 of the window), flat 0x30 land gives 0x30.
        Site s;
        s.fill(0, 0, kSize, kSize, 0x10);
        sim::World w = s.world();
        built(w, k.coast1, 5, 5, 0, 0x10);
        Site l;
        sim::World lw = l.world();
        built(lw, k.coast1, 5, 5, 0, 0x30);
        // ... while an ordinary type on that open sea is refused.
        refused(w, build_at(k.house, 8, 8), sim::Reject::TerrainRefused);
    }
}

TEST_CASE("build terrain: coordinates at the extremes of the integer ranges") {
    const Kit& k = kit();
    Site s;
    sim::World w = s.world();
    for (const std::int32_t x :
         {std::numeric_limits<std::int32_t>::min(), -65536, -1, 65536 + kX0, 65530, 65532, 65535,
          65536, std::numeric_limits<std::int32_t>::max(),
          std::numeric_limits<std::int32_t>::max() - 1}) {
        for (const std::int32_t y : {std::numeric_limits<std::int32_t>::min(), kY0 + 3,
                                     std::numeric_limits<std::int32_t>::max()}) {
            INFO("x " << x << " y " << y);
            refused(w, ct::build(0, kC0, {ct::at(k.house, x, y)}), sim::Reject::OutOfIsland);
        }
    }
    // A coordinate that equals the island's modulo 2^16 or 2^32 is another tile (no wrap).
    refused(w, ct::build(0, kC0, {ct::at(k.house, kX0 + 65536, kY0 + 3)}),
            sim::Reject::OutOfIsland);
    // An island at the far edge of the largest world (x0 + W = 16384).
    Site hi;
    hi.x0 = 16379;
    hi.y0 = 16379;
    hi.size = 5;
    hi.world_edge = 16384; // sim::kWorldEdgeBound
    hi.height.assign(25, 0x30);
    sim::World h = hi.world();
    const auto at_hi = [&](std::int32_t x, std::int32_t y) {
        return ct::build(0, kC0, {ct::at(k.house, 16379 + x, 16379 + y)});
    };
    refused(h, at_hi(3, 1), sim::Reject::TerrainRefused); // the zero column W
    refused(h, at_hi(4, 1), sim::Reject::OutOfIsland);
    refused(h, at_hi(5, 1), sim::Reject::OutOfIsland);
    REQUIRE(h.apply(at_hi(2, 2)).ok());
    CHECK(h.building_at(kC0, 16381, 16381)->byte0f == 0x30);
    // Coordinates that are another island position modulo 2^16: no wrap in the comparison.
    for (const std::int32_t x : {65530, 65532, 65536 + 16381, 2 * 65536 + 16381}) {
        refused(h, ct::build(0, kC0, {ct::at(k.house, x, 16381)}), sim::Reject::OutOfIsland);
    }
}

TEST_CASE("build terrain: a world without terrain decides nothing") {
    const Kit& k = kit();
    Site none;
    none.terrain = false;
    sim::World w = none.world();
    refused(w, build_at(k.house, 3, 3), sim::Reject::Unsupported);
    refused(w, build_at(k.coast1, 3, 3), sim::Reject::Unsupported);
    // The control: the same site with terrain.
    Site with;
    sim::World c = with.world();
    built(c, k.house, 3, 3, 0, 0x30);
}

namespace {

void rewrite_to(void* ctx, const sim::BuildingType&, sim::PlacementRecord& rec) {
    rec.type = *static_cast<sim::TypeId*>(ctx);
}

sim::Seams rewriting(sim::TypeId* target) {
    sim::Seams s;
    s.upgrade_rewrite = {"test_rewrite_to", &rewrite_to, target};
    return s;
}

} // namespace

TEST_CASE("build terrain: an upgrade rewrite is placed as what it became") {
    const Kit& k = kit();
    Site s;
    s.fill(0, 0, kSize, kSize, 0x30);
    s.fill(8, 8, 8, 8, 0x40);
    sim::TypeId target = k.farm3;
    {
        // House at (5,5): window 5..7 is flat 0x30; the farm at (5,5) has the window 5..8, which
        // reaches the shelf vertex (8,8) = 0x40: refused although the house would have been legal.
        sim::World w = s.world({}, rewriting(&target));
        refused(w, build_at(k.house, 5, 5), sim::Reject::TerrainRefused);
        sim::World plain = s.world();
        built(plain, k.house, 5, 5, 0, 0x30); // the control: no rewrite, legal
        // A legal rewrite takes the height of its own window.
        sim::World ok = s.world({}, rewriting(&target));
        const sim::ApplyResult r = ok.apply(build_at(k.house, 1, 1));
        REQUIRE(r.ok());
        CHECK(ok.find_building(r.records[0].building)->type == k.farm3);
        check_height(ok, k.farm3, 1, 1, 0, 0x30);
        // On the shelf the farm gets 0x40.
        const sim::ApplyResult up = ok.apply(build_at(k.house, 10, 10));
        REQUIRE(up.ok());
        check_height(ok, k.farm3, 10, 10, 0, 0x40);
    }
    // Rewritten to an unsupported class, a road and a type on sea.
    for (const sim::TypeId bad : {k.mountain, k.covered, k.base.road}) {
        INFO("rewritten to " << bad.v);
        sim::TypeId t = bad;
        sim::World w = s.world({}, rewriting(&t));
        refused(w, build_at(k.house, 1, 1), sim::Reject::Unsupported);
    }
}

TEST_CASE("build terrain: a mixed batch is atomic or partial, never silently half-changed") {
    const Kit& k = kit();
    Site s;
    s.fill(0, 8, kSize, 8, 0x10); // sea in the south half
    const sim::Command mixed =
        ct::build(0, kC0, {ct::at(k.house, kX0 + 2, kY0 + 2), ct::at(k.house, kX0 + 2, kY0 + 10)});
    // Atomic: the legal record aborts with the illegal one; nothing at all changes.
    {
        sim::Rules atomic;
        atomic.build_atomic = true;
        sim::World w = s.world(atomic);
        const std::vector<std::uint8_t> before = w.save();
        const std::int64_t gold = money_of(w);
        const sim::ApplyResult r = w.apply(mixed);
        CHECK(r.status == sim::Reject::AtomicAbort);
        CHECK(r.records.at(0).status == sim::Reject::AtomicAbort);
        CHECK(r.records.at(1).status == sim::Reject::TerrainRefused);
        CHECK(w.save() == before);
        CHECK(money_of(w) == gold);
        CHECK(w.building_at(kC0, kX0 + 2, kY0 + 2) == nullptr);
        CHECK(w.drain_events().size() == 1); // the host's rejection notice only
        // The control: the legal record alone.
        built(w, k.house, 2, 2, 0, 0x30);
    }
    // Non-atomic: the drag build is partial, the legal record is placed with its height.
    {
        sim::World w = s.world();
        const sim::ApplyResult r = w.apply(mixed);
        CHECK(r.ok());
        CHECK(r.records.at(0).status == sim::Reject::None);
        CHECK(r.records.at(1).status == sim::Reject::TerrainRefused);
        check_height(w, k.house, 2, 2, 0, 0x30);
        CHECK(w.building_at(kC0, kX0 + 2, kY0 + 10) == nullptr);
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("build terrain: a record that overlaps an earlier one answers Occupied in both paths") {
    const Kit& k = kit();
    // Record 1 overlaps record 0 and also sits on bad terrain (the south-east vertex of its window
    // is sea): the dry run has no tile writes and the live run sees the earlier building, so the
    // occupancy answer must come first in both (ship-placement.md "Precedence").
    Site s;
    s.set(5, 5, 0x00);
    const sim::Command c =
        ct::build(0, kC0, {ct::at(k.house, kX0 + 2, kY0 + 2), ct::at(k.house, kX0 + 3, kY0 + 3)});
    sim::Rules atomic;
    atomic.build_atomic = true;
    sim::World a = s.world(atomic);
    sim::World b = s.world();
    const sim::ApplyResult dry = a.apply(c);
    const sim::ApplyResult live = b.apply(c);
    CHECK(dry.status == sim::Reject::AtomicAbort);
    CHECK(dry.records.at(0).status == sim::Reject::AtomicAbort);
    CHECK(dry.records.at(1).status == sim::Reject::Occupied);
    CHECK(live.records.at(0).status == sim::Reject::None);
    CHECK(live.records.at(1).status == sim::Reject::Occupied);
}

namespace {

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

} // namespace

TEST_CASE("build terrain: dry run and live run refuse the same record for the same reason") {
    // Random terrains and 1..4 records (overlapping ones among them), in an atomic world and in a
    // non-atomic one. Property: if every record of the live run passes, the atomic run passes too
    // and both worlds are byte-identical afterwards; otherwise the atomic command changed nothing,
    // and for each record the atomic status is the live status, or AtomicAbort where the live
    // record passed.
    const Kit& k = kit();
    const sim::TypeId types[] = {k.house, k.trapper, k.farm3, k.coast, k.coast1, k.plain4, k.wide};
    const std::uint8_t levels[] = {0x00, 0x10, 0x20, 0x30, 0x30, 0x30, 0x31, 0x40};
    Lcg rng{0x2545f4914f6cdd1dULL};
    sim::Rules atomic;
    atomic.build_atomic = true;
    std::size_t all_pass = 0, aborted = 0, occupied_first = 0, terrain_seen = 0;
    for (int round = 0; round < 250; ++round) {
        Site s;
        // A mostly legal landscape: a base level and a few patches.
        s.fill(0, 0, kSize, kSize, levels[rng.next() % 8]);
        for (int p = 0; p < 4; ++p) {
            s.fill(rng.next() % kSize, rng.next() % kSize, 1 + rng.next() % 6, 1 + rng.next() % 6,
                   levels[rng.next() % 8]);
        }
        std::vector<sim::PlacementWire> placements;
        const std::uint32_t n = 1 + rng.next() % 4;
        for (std::uint32_t i = 0; i < n; ++i) {
            const std::int32_t lx = static_cast<std::int32_t>(rng.next() % 14);
            const std::int32_t ly = static_cast<std::int32_t>(rng.next() % 14);
            placements.push_back(ct::at(types[rng.next() % 7], kX0 + lx, kY0 + ly,
                                        static_cast<std::uint8_t>(rng.next() % 4)));
            if (i > 0 && rng.next() % 3 == 0) { // overlap the previous record on purpose
                placements.back().x =
                    placements[i - 1].x + static_cast<std::int32_t>(rng.next() % 2);
                placements.back().y =
                    placements[i - 1].y + static_cast<std::int32_t>(rng.next() % 2);
            }
        }
        const sim::Command c = ct::build(0, kC0, placements);
        sim::World a = s.world(atomic);
        sim::World b = s.world();
        const std::vector<std::uint8_t> initial = a.save();
        const sim::ApplyResult dry = a.apply(c);
        const sim::ApplyResult live = b.apply(c);
        bool live_all = true;
        for (const sim::RecordResult& r : live.records) {
            live_all = live_all && r.status == sim::Reject::None;
        }
        INFO("round " << round);
        if (live_all) {
            ++all_pass;
            CHECK(dry.ok());
            // Everything but the header (which holds the differing Rules) is identical.
            const std::size_t from_a = island_test::flat_offset(a, "clock_rng");
            const std::size_t from_b = island_test::flat_offset(b, "clock_rng");
            REQUIRE(from_a != std::string::npos);
            const std::vector<std::uint8_t> fa = island_test::flat_stream(a);
            const std::vector<std::uint8_t> fb = island_test::flat_stream(b);
            CHECK(std::vector<std::uint8_t>(fa.begin() + static_cast<std::ptrdiff_t>(from_a),
                                            fa.end()) ==
                  std::vector<std::uint8_t>(fb.begin() + static_cast<std::ptrdiff_t>(from_b),
                                            fb.end()));
        } else {
            ++aborted;
            CHECK(dry.status == sim::Reject::AtomicAbort);
            CHECK(a.save() == initial);
            for (std::size_t i = 0; i < live.records.size(); ++i) {
                INFO("record " << i);
                if (live.records[i].status == sim::Reject::None) {
                    CHECK(dry.records[i].status == sim::Reject::AtomicAbort);
                } else {
                    CHECK(dry.records[i].status == live.records[i].status);
                    occupied_first += live.records[i].status == sim::Reject::Occupied ? 1U : 0U;
                    terrain_seen += live.records[i].status == sim::Reject::TerrainRefused ? 1U : 0U;
                }
            }
        }
        CHECK(a.check_invariants() == "");
        CHECK(b.check_invariants() == "");
    }
    // The generator reaches the interesting outcomes, so the loop cannot pass vacuously.
    CHECK(all_pass >= 20);
    CHECK(aborted >= 20);
    CHECK(occupied_first >= 5);
    CHECK(terrain_seen >= 20);
}

TEST_CASE("build terrain: ownership and funds are decided in their own order around the terrain") {
    // Each pair has the same command on legal land (the control) and on sea, so the terrain
    // refusal is not hidden by, and does not hide, an ownership or funds failure.
    const sim::TypeId type = kit().house; // 2 x 2, 100 gold
    for (const bool sea : {false, true}) {
        // Poor player: on legal land the funds refuse; on sea the terrain refuses first.
        Site poor;
        poor.money = 50;
        if (sea) {
            poor.fill(2, 2, 3, 3, 0x00);
        }
        sim::World w = poor.world();
        refused(w, build_at(type, 3, 3),
                sea ? sim::Reject::TerrainRefused : sim::Reject::CannotAfford);
        // Rich control: the same sites with enough gold. Land builds, sea is still refused.
        Site rich;
        if (sea) {
            rich.fill(2, 2, 3, 3, 0x00);
        }
        sim::World r = rich.world();
        if (sea) {
            refused(r, build_at(type, 3, 3), sim::Reject::TerrainRefused);
        } else {
            CHECK(r.apply(build_at(type, 3, 3)).ok());
        }
        // Foreign tiles (columns 8..15 belong to player 1's city): the owner rule answers first
        // on sea too, because it precedes the terrain rule; on legal land it answers the same.
        Site split;
        split.split = true;
        if (sea) {
            split.fill(7, 3, 3, 3, 0x00);
        }
        sim::World s = split.world();
        refused(s, build_at(type, 7, 3), sim::Reject::TileNotOwned);
    }
}
