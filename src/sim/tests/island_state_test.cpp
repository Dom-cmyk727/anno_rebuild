// Islands in the world (docs/sim/state.md §4.8, §9.3, §9.5, §10, stage S5):
// World::create from a scenario, the islands scope of the stream (layout, hash
// tree, perturbation, validation), the save frame and its refusals.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "golden.hpp"
#include "island_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using island_test::create_error;
using island_test::make_world;
using island_test::Stream;

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

bool starts_with(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

sim::Island& isl(sim::World& w, std::size_t slot) {
    return *sim::WorldTestAccess::islands(w)[slot];
}

/// The hashes of the top-level scopes of the world (header, clock_rng, settings, ids,
/// players, islands, figures, events).
std::vector<std::uint64_t> scope_hashes(const sim::World& w) {
    sim::HashTree t;
    static_cast<void>(w.state_hash(t, 0));
    std::vector<std::uint64_t> out;
    for (const sim::HashNode& c : t.root.children) {
        out.push_back(c.hash);
    }
    return out;
}

/// A world whose island has all-zero tile columns, a terrain and two deposits:
/// every persistent field of the island can then be perturbed on its own.
sim::ScenarioSpec zero_island_scenario() {
    sim::ScenarioSpec spec = syn::empty_scenario();
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(2, 300, 200, 30, 20);
    is.owner_runs = {{0, 600}};
    is.terrain = sim::make_terrain({1, 2, 3}, {4, 5}, {6});
    sim::ScenarioSpec::DepositSpec d;
    d.goods = "GOLD";
    d.x = 301;
    d.y = 202;
    spec.islands.push_back(is);
    spec.islands[0].deposits = {d, d};
    spec.islands[0].deposits[1].goods = "EISENERZ";
    island_test::add_owner_cities(spec);
    return spec;
}

} // namespace

// ---------------------------------------------------------------- create

TEST_CASE("world create builds the synthetic scenarios") {
    const sim::World w = make_world(syn::basic_scenario());
    CHECK(w.clock().ms == 0);
    CHECK(w.clock().last_tick == 0);
    CHECK(w.rng().cursor() == 1234);
    REQUIRE(w.players().slots.size() == 32);
    REQUIRE(w.players().slots[0].has_value());
    CHECK(w.players().slots[0]->money == 10000);
    CHECK(w.players().slots[0]->default_colour == sim::kNoColour);
    for (std::size_t i = 1; i < 32; ++i) {
        CHECK_FALSE(w.players().slots[i].has_value());
    }
    REQUIRE(w.islands().size() == 1);
    REQUIRE(w.islands()[0].has_value());
    const sim::Island& is = *w.islands()[0];
    CHECK(is.index.v == 0);
    CHECK(is.rect() == sim::TileRect{100, 100, 64, 64});
    CHECK(is.tiles.chunk_edge() == 8);
    CHECK(is.tiles.chunk_count() == 64);
    CHECK(is.climate == 2);
    CHECK(is.next_serial == 1);
    CHECK(is.terrain == nullptr);
    CHECK_FALSE(is.drought.has_value());
    CHECK(is.sweep.stamp == 0);
    CHECK(is.sweep.grow_x == 0);
    CHECK(is.event300_stamp == 0);
    CHECK(is.reserved_object_count == 0);
    // Two neutral rows (owner 15), then city 0 to the last tile.
    CHECK(is.tiles.owner(0) == 15);
    CHECK(is.tiles.owner(127) == 15);
    CHECK(is.tiles.owner(128) == 0);
    CHECK(is.tiles.owner(4095) == 0);
    // Scenery and growing items: object and phase, the others untouched.
    CHECK(is.tiles.object(200) == 1201);
    CHECK(is.tiles.phase(200) == 0);
    CHECK(is.tiles.object(201) == 1202);
    CHECK(is.tiles.phase(201) == 3);
    CHECK(is.tiles.object(4095) == 1203);
    CHECK(is.tiles.phase(4095) == 7);
    CHECK(is.tiles.object(202) == 0);
    CHECK(is.tiles.phase(202) == 0);
    CHECK(is.tiles.aux(200) == 0);
    REQUIRE(is.deposits.size() == 1);
    CHECK(is.deposits[0].goods == "EISENERZ");
    CHECK(is.deposits[0].x == 110);
    CHECK(is.deposits[0].y == 120);
    CHECK(is.deposits[0].remaining == 2000 * sim::kTon);
    CHECK(is.deposits[0].reserve);
    // Lookups by world tile.
    CHECK(is.tile_index(110, 120) == 20U * 64U + 10U);
    CHECK(is.tile_index(99, 100) == sim::kNoSlot);
    CHECK(w.island_at(100, 100) == &is);
    CHECK(w.island_at(163, 163) == &is);
    CHECK(w.island_at(164, 100) == nullptr);
    CHECK(w.island_at(100, 164) == nullptr);
    CHECK(w.island_at(99, 99) == nullptr);
    CHECK(sim::validate_islands(w.islands(), w.config(), w.settings(), w.types()) == "");
}

TEST_CASE("world create builds two islands with gaps in the table") {
    const sim::World w = make_world(syn::two_island_scenario(77));
    CHECK(w.rng().cursor() == 77);
    REQUIRE(w.islands().size() == 4);
    CHECK(w.islands()[0].has_value());
    CHECK_FALSE(w.islands()[1].has_value());
    CHECK_FALSE(w.islands()[2].has_value());
    REQUIRE(w.islands()[3].has_value());
    const sim::Island& b = *w.islands()[3];
    CHECK(b.index.v == 3);
    CHECK(b.rect() == sim::TileRect{300, 200, 30, 20});
    CHECK(b.climate == 6);
    // 30 x 20 with chunks of 8: 4 x 3 chunks, the edge ones clipped.
    CHECK(b.tiles.chunks_x() == 4);
    CHECK(b.tiles.chunks_y() == 3);
    REQUIRE(b.terrain != nullptr);
    CHECK(b.terrain->height.size() == 600);
    CHECK(b.terrain->digest == sim::terrain_digest(*b.terrain));
    REQUIRE(b.deposits.size() == 2);
    CHECK(b.deposits[0].goods == "GOLD");
    CHECK(b.deposits[0].remaining == 123456);
    CHECK_FALSE(b.deposits[0].reserve);
    CHECK(b.deposits[1].goods == "STEINE");
    // Owner run for city 1 over the first 300 tiles, the rest neutral.
    CHECK(b.tiles.owner(0) == 1);
    CHECK(b.tiles.owner(299) == 1);
    CHECK(b.tiles.owner(300) == 15);
    CHECK(b.tiles.owner(599) == 15);
    CHECK(b.tiles.object(0) == 2001);
    CHECK(b.tiles.object(599) == 2002);
    CHECK(b.tiles.phase(599) == 1);
    // The second player.
    REQUIRE(w.players().slots[1].has_value());
    const sim::Player& p = *w.players().slots[1];
    CHECK(p.index.v == 1);
    CHECK(p.kind == 12);
    CHECK(p.money == 25000);
    CHECK(p.name == std::vector<std::uint8_t>{'A', 'I'});
    // Lookup across both islands.
    CHECK(w.island_at(329, 219) == &b);
    CHECK(w.island_at(330, 219) == nullptr);
    CHECK(w.island_at(300, 200) == &b);
    CHECK(w.island_at(110, 110) == &*w.islands()[0]);
    CHECK(w.island_at(250, 150) == nullptr);
    // The tiles are fresh: every chunk dirty, no digest computed yet.
    CHECK(b.tiles.dirty_chunks() == 12);
}

TEST_CASE("world create without islands and with an unused table") {
    const sim::World w = make_world(syn::empty_scenario(5));
    CHECK(w.islands().empty());
    CHECK(w.island_at(0, 0) == nullptr);
    CHECK(w.rng().cursor() == 5);
    CHECK(w.players().slots.size() == 32);
    for (const auto& p : w.players().slots) {
        CHECK_FALSE(p.has_value());
    }
    // The settings are the scenario's.
    CHECK(w.settings().world_width == 512);
    // The seed is taken modulo the table length.
    const sim::World big = make_world(syn::empty_scenario(2048 + 9));
    CHECK(big.rng().cursor() == 9);
    // A custom PRNG table.
    sim::ScenarioSpec spec = syn::empty_scenario(1234);
    spec.rng_table = sim::msvc_rand_sequence(100, 5);
    const sim::World custom = make_world(spec);
    CHECK(custom.rng().size() == 100);
    CHECK(custom.rng().cursor() == 34);
    CHECK(custom.rng().table_digest() != w.rng().table_digest());
}

TEST_CASE("scenario players carry their fields into the table") {
    sim::ScenarioSpec spec = syn::empty_scenario();
    sim::ScenarioSpec::PlayerSpec a;
    a.id = sim::PlayerId{7};
    a.kind = 11;
    a.volk = 2;
    a.money = -50;
    a.flags_a0 = 8;
    a.colour = 3;
    a.name = {'B', 'o', 'b'};
    a.research.assign(65, 0);
    a.research[5] = 3;
    a.research[64] = 1;
    sim::ScenarioSpec::PlayerSpec b;
    b.id = sim::PlayerId{31};
    spec.players = {a, b};
    const sim::World w = make_world(spec);
    REQUIRE(w.players().slots[7].has_value());
    const sim::Player& p = *w.players().slots[7];
    CHECK(p.index.v == 7);
    CHECK(p.kind == 11);
    CHECK(p.volk == 2);
    CHECK(p.money == -50);
    CHECK(p.flags_a0 == 8);
    CHECK(p.default_colour == 3);
    CHECK(p.colour_slot == 0);
    CHECK(p.name == std::vector<std::uint8_t>{'B', 'o', 'b'});
    REQUIRE(p.research.state.size() == 65);
    CHECK(p.research.state[5] == 3);
    CHECK(p.research.state[64] == 1);
    CHECK(p.research.state[4] == 0);
    CHECK(p.research.progress.size() == 65);
    CHECK(p.research.researched(5));
    CHECK_FALSE(p.research.researched(64));
    // An empty research list is all zero; colour 0xff is "none".
    REQUIRE(w.players().slots[31].has_value());
    CHECK(w.players().slots[31]->default_colour == sim::kNoColour);
    CHECK(w.players().slots[31]->research.state == std::vector<std::uint8_t>(65, 0));
    CHECK(w.players().slots[31]->money == 10000);
    CHECK(w.players().slots[31]->kind == 8);
    CHECK(w.players().slots[31]->volk == 1);
    CHECK(sim::validate_players(w.players(), w.config()) == "");
}

TEST_CASE("owner runs expand to tiles") {
    sim::ScenarioSpec spec = syn::empty_scenario();
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 10, 20, 4, 3);
    // Runs: 3 tiles of city 2, an empty run, 4 of city 5; the last 5 tiles are neutral.
    is.owner_runs = {{2, 3}, {6, 0}, {5, 4}};
    spec.islands.push_back(is);
    island_test::add_owner_cities(spec);
    {
        const sim::World w = make_world(spec);
        const sim::TileLayer& t = w.islands()[0]->tiles;
        const std::vector<std::uint32_t> want = {2, 2, 2, 5, 5, 5, 5, 15, 15, 15, 15, 15};
        for (std::uint64_t i = 0; i < 12; ++i) {
            INFO("tile " << i);
            CHECK(t.owner(i) == want[i]);
        }
    }
    SUBCASE("the neutral owner is configuration") {
        sim::SimConfig cfg;
        cfg.neutral_owner = 7;
        const sim::World w = make_world(spec, cfg);
        const sim::TileLayer& t = w.islands()[0]->tiles;
        CHECK(t.owner(0) == 2);
        CHECK(t.owner(7) == 7);
        CHECK(t.owner(11) == 7);
    }
    SUBCASE("runs that fill the island exactly, and one more tile") {
        spec.islands[0].owner_runs = {{1, 5}, {2, 7}};
        island_test::add_owner_cities(spec);
        const sim::World w = make_world(spec);
        CHECK(w.islands()[0]->tiles.owner(4) == 1);
        CHECK(w.islands()[0]->tiles.owner(5) == 2);
        CHECK(w.islands()[0]->tiles.owner(11) == 2);
        spec.islands[0].owner_runs = {{1, 5}, {2, 8}};
        CHECK(contains(create_error(spec), "islands[0].owner_runs[1]: the runs cover more"));
    }
    SUBCASE("the neutral owner and the one below it are valid, one above is not") {
        spec.islands[0].owner_runs = {{14, 1}, {15, 1}};
        island_test::add_owner_cities(spec);
        const sim::World w = make_world(spec);
        CHECK(w.islands()[0]->tiles.owner(0) == 14);
        CHECK(w.islands()[0]->tiles.owner(1) == 15);
        spec.islands[0].owner_runs = {{16, 1}};
        CHECK(contains(create_error(spec), "islands[0].owner_runs[0]: owner 16"));
    }
}

TEST_CASE("owner column holds values above 65535") {
    // city_slots 65536 is the largest table (kSlotTableBound); the neutral owner
    // may equal it. A u16 column would truncate both values below.
    sim::SimConfig cfg;
    cfg.city_slots = sim::kSlotTableBound;
    cfg.neutral_owner = sim::kSlotTableBound;
    sim::ScenarioSpec spec = syn::empty_scenario();
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 0, 0, 5, 2);
    is.owner_runs = {{65535, 1}, {65536, 1}, {3, 1}};
    spec.islands.push_back(is);
    island_test::add_owner_cities(spec, sim::kSlotTableBound);
    const sim::World w = make_world(spec, cfg);
    const sim::TileLayer& t = w.islands()[0]->tiles;
    CHECK(t.owner(0) == 65535);
    CHECK(t.owner(1) == 65536);
    CHECK(t.owner(2) == 3);
    CHECK(t.owner(3) == 65536); // neutral fill
    CHECK(t.owner(9) == 65536);
    // And through the stream and the save.
    sim::World back = island_test::load_world(w.save());
    CHECK(back.islands()[0]->tiles.owner(1) == 65536);
    CHECK(back.islands()[0]->tiles.owner(9) == 65536);
    CHECK(back.state_hash() == w.state_hash());
    sim::World flat(1);
    CHECK(island_test::read_flat(flat, island_test::flat_stream(w)) == "");
    CHECK(flat.islands()[0]->tiles.owner(1) == 65536);
}

TEST_CASE("world create refuses an invalid config") {
    const sim::ScenarioSpec spec = syn::basic_scenario();
    CHECK(create_error(spec) == "");
    SUBCASE("configuration") {
        sim::SimConfig c;
        c.building_div = 0;
        CHECK(starts_with(create_error(spec, c), "SimConfig.building_div"));
        c = {};
        c.island_slots = 0;
        CHECK(starts_with(create_error(spec, c), "SimConfig.island_slots"));
        c = {};
        c.chunk_edge = 0;
        CHECK(starts_with(create_error(spec, c), "SimConfig.chunk_edge"));
        c = {};
        c.player_slots = 0xFFFFFFFFU;
        CHECK(starts_with(create_error(spec, c), "SimConfig.player_slots"));
        c = {};
        c.layout.block_size = 0;
        CHECK(contains(create_error(spec, c), "block_size"));
    }
    SUBCASE("rules") {
        sim::Rules r;
        r.prng_mode = sim::PrngMode::PerIsland;
        CHECK(starts_with(create_error(spec, {}, r), "Rules.prng_mode"));
        r = {};
        r.spatial_order = sim::SpatialOrder::Original0x10027000;
        CHECK(starts_with(create_error(spec, {}, r), "Rules.spatial_order"));
        r = {};
        r.events_max_dispatch = 0;
        CHECK(starts_with(create_error(spec, {}, r), "Rules.events_max_dispatch"));
    }
    SUBCASE("seams") {
        sim::Seams s;
        s.city_pass.resize(7);
        CHECK(contains(create_error(spec, {}, {}, s), "city_pass"));
        sim::Seams named;
        named.city_is_empty.fn = [](void*, const sim::City&) { return false; };
        // An installed function under the default name hashes like no function.
        CHECK(contains(create_error(spec, {}, {}, named), "city_is_empty"));
        named.city_is_empty.name = "my_empty_rule";
        CHECK(create_error(spec, {}, {}, named) == "");
    }
    SUBCASE("settings") {
        sim::ScenarioSpec bad = spec;
        bad.settings.prop10 = 3;
        CHECK(contains(create_error(bad), "prop10"));
        bad = spec;
        bad.settings.world_width = sim::kWorldEdgeBound + 1;
        CHECK(contains(create_error(bad), "world_width"));
    }
    SUBCASE("registry") {
        auto r = sim::World::create(spec, nullptr);
        REQUIRE_FALSE(r.ok());
        CHECK(starts_with(r.error(), "registry"));
    }
}

TEST_CASE("scenario validation names the path") {
    using Spec = sim::ScenarioSpec;
    struct Row {
        const char* name;
        void (*mutate)(Spec&);
        const char* expect;
    };
    const std::vector<Row> rows = {
        {"player slot out of range", [](Spec& s) { s.players[1].id = sim::PlayerId{32}; },
         "players[1].id: is not a slot below SimConfig.player_slots (32)"},
        {"player id invalid", [](Spec& s) { s.players[0].id = sim::PlayerId{}; },
         "players[0].id: is not a slot"},
        {"player slot used twice", [](Spec& s) { s.players[1].id = sim::PlayerId{0}; },
         "players[1].id: slot 0 is used twice"},
        {"research length", [](Spec& s) { s.players[0].research.assign(3, 1); },
         "players[0].research: 3 entries, SimConfig.research_entries is 65"},
        {"island slot out of range", [](Spec& s) { s.islands[0].slot = sim::IslandId{256}; },
         "islands[0].slot: is not a slot below SimConfig.island_slots (256)"},
        {"island slot used twice", [](Spec& s) { s.islands[1].slot = sim::IslandId{0}; },
         "islands[1].slot: 0 is used twice"},
        {"empty island width", [](Spec& s) { s.islands[1].w = 0; },
         "islands[1]: the island rectangle is empty (0 x 20)"},
        {"empty island height", [](Spec& s) { s.islands[1].h = 0; },
         "islands[1]: the island rectangle is empty (30 x 0)"},
        {"too many tiles",
         [](Spec& s) {
             s.settings.world_width = 16384;
             s.settings.world_height = 16384;
             s.islands[0].w = 9000;
             s.islands[0].h = 9000;
             s.islands[0].owner_runs.clear();
             s.islands[0].objects.clear();
             s.islands[0].deposits.clear();
         },
         "islands[0]: 81000000 tiles, more than 67108864"},
        {"climate", [](Spec& s) { s.islands[1].climate = 7; },
         "islands[1].climate: 7 has no entry in SimConfig.monopoly_climate_mask"},
        {"terrain digest",
         [](Spec& s) {
             auto t = std::make_shared<sim::Terrain>(*s.islands[1].terrain);
             t->digest += 1;
             s.islands[1].terrain = t;
         },
         "islands[1].terrain: the digest does not match the columns"},
        {"owner run owner", [](Spec& s) { s.islands[1].owner_runs[0].first = 16; },
         "islands[1].owner_runs[0]: owner 16 is above neutral_owner 15"},
        {"owner runs too long", [](Spec& s) { s.islands[1].owner_runs = {{1, 599}, {2, 2}}; },
         "islands[1].owner_runs[1]: the runs cover more than the 600 tiles"},
        {"object tile outside", [](Spec& s) { s.islands[1].objects[1].index = 600; },
         "islands[1].objects[1]: tile index 600 is outside the island"},
        {"object tile twice", [](Spec& s) { s.islands[1].objects[1].index = 0; },
         "islands[1].objects: a tile index is given twice"},
        {"deposit outside", [](Spec& s) { s.islands[1].deposits[1].x = 330; },
         "islands[1].deposits[1]: the position is outside the island"},
        {"deposit below", [](Spec& s) { s.islands[1].deposits[1].y = 220; },
         "islands[1].deposits[1]: the position is outside the island"},
        {"deposit above", [](Spec& s) { s.islands[1].deposits[0].y = 199; },
         "islands[1].deposits[0]: the position is outside the island"},
        {"deposit goods", [](Spec& s) { s.islands[1].deposits[0].goods = "NOSUCHGOODS"; },
         "islands[1].deposits[0].goods: unknown goods name 'NOSUCHGOODS'"},
        {"island outside the world on the right",
         [](Spec& s) {
             s.islands[1].x0 = 483;
             s.islands[1].deposits.clear();
         },
         "islands[3].tiles: the rectangle is not inside the world (512 x 512)"},
        {"island with a negative origin",
         [](Spec& s) {
             s.islands[1].y0 = -1;
             s.islands[1].deposits.clear();
         },
         "islands[3].tiles: the rectangle is not inside the world"},
        {"islands overlap",
         [](Spec& s) {
             s.islands[1].x0 = 163;
             s.islands[1].y0 = 160;
             s.islands[1].deposits.clear();
             s.islands[1].objects.clear();
         },
         "islands[3].tiles: the rectangle overlaps islands[0]"},
        {"empty PRNG table", [](Spec& s) { s.rng_table = std::vector<std::uint16_t>{}; },
         "rng_table: must not be empty"},
    };
    for (const Row& row : rows) {
        INFO("row " << row.name);
        Spec spec = syn::two_island_scenario();
        row.mutate(spec);
        const std::string err = create_error(spec);
        CHECK(starts_with(err, row.expect));
    }
    // The untouched scenario is accepted, and so are the edges of every rule.
    CHECK(create_error(syn::two_island_scenario()) == "");
    {
        Spec spec = syn::two_island_scenario();
        spec.islands[1].x0 = 482; // 482 + 30 == 512: touches the world edge
        spec.islands[1].deposits[0].x = 482;
        spec.islands[1].deposits[1].x = 511;
        spec.islands[1].deposits[1].y = 219;
        CHECK(create_error(spec) == "");
        spec.islands[1].x0 = 164; // adjacent to island 0 (100..163), not overlapping
        spec.islands[1].y0 = 100;
        spec.islands[1].deposits[0] = {"GOLD", 164, 100, 1, true};
        spec.islands[1].deposits[1] = {"GOLD", 193, 119, 1, true};
        CHECK(create_error(spec) == "");
        spec.islands[1].y0 = 164; // below island 0
        spec.islands[1].x0 = 100;
        spec.islands[1].deposits[0] = {"GOLD", 100, 164, 1, true};
        spec.islands[1].deposits[1] = {"GOLD", 129, 183, 1, true};
        CHECK(create_error(spec) == "");
    }
    // The island at the table's highest slot, and any order of the slots.
    {
        Spec spec = syn::two_island_scenario();
        spec.islands[1].slot = sim::IslandId{255};
        const sim::World w = make_world(spec);
        CHECK(w.islands().size() == 256);
        CHECK(w.islands()[255].has_value());
        std::swap(spec.islands[0], spec.islands[1]);
        const sim::World swapped = make_world(spec);
        CHECK(swapped.state_hash() == w.state_hash());
    }
}

TEST_CASE("world create keeps the context it was given") {
    sim::SimConfig cfg;
    cfg.chunk_edge = 16;
    cfg.player_slots = 40;
    cfg.island_slots = 8;
    cfg.research_entries = 70;
    sim::Rules rules;
    rules.player_table_full_tick = true;
    sim::Seams seams;
    seams.city_is_empty.fn = [](void*, const sim::City&) { return false; };
    seams.city_is_empty.name = "kept_rule";
    auto types = syn::make_synthetic_types();
    REQUIRE(types.ok());
    sim::ScenarioSpec spec = syn::basic_scenario();
    spec.settings.prop6 = 0x84;
    spec.players[0].id = sim::PlayerId{35};
    spec.islands[0].cities[0].owner = sim::PlayerId{35};
    auto r = sim::World::create(spec, types.value().registry, cfg, rules, seams);
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    const sim::World& w = r.value();
    CHECK(w.config().chunk_edge == 16);
    CHECK(w.config().player_slots == 40);
    CHECK(w.rules().player_table_full_tick);
    CHECK(sim::seam_names(w.seams())[sim::seam_names(w.seams()).size() - 1] == "kept_rule");
    CHECK(&w.types() == types.value().registry.get());
    CHECK(w.settings().prop6 == 0x84);
    CHECK(w.settings().world_width == 512);
    // The tables are sized from the configuration given.
    CHECK(w.players().slots.size() == 40);
    REQUIRE(w.players().slots[35].has_value());
    CHECK(w.players().slots[35]->research.state.size() == 70);
    CHECK(w.islands()[0]->tiles.chunk_edge() == 16);
    CHECK(w.islands()[0]->tiles.chunks_x() == 4);
    // The island table is bounded by island_slots: slot 8 does not exist.
    spec.islands[0].slot = sim::IslandId{8};
    CHECK(contains(create_error(spec, cfg),
                   "islands[0].slot: is not a slot below SimConfig.island_slots (8)"));
    spec.islands[0].slot = sim::IslandId{7};
    spec.players[0].id = sim::PlayerId{0};
    spec.islands[0].cities[0].owner = sim::PlayerId{0};
    auto ok = sim::World::create(spec, types.value().registry, cfg, rules, seams);
    REQUIRE(ok.ok());
    CHECK(ok.value().islands().size() == 8);
}

TEST_CASE("world create does not depend on the executor") {
    sim::ScenarioSpec spec = syn::two_island_scenario();
    for (std::uint32_t k = 0; k < 6; ++k) {
        sim::ScenarioSpec::IslandSpec is =
            syn::plain_island(4 + k, static_cast<std::int32_t>(40 * k), 300, 16 + k, 16);
        is.owner_runs = {{k, 40}};
        is.objects = {{3, 100 + k, 1}};
        spec.islands.push_back(is);
    }
    island_test::add_owner_cities(spec);
    const sim::World serial = make_world(spec);
    for (const unsigned threads : {1U, 2U, 8U}) {
        island_test::ThreadExecutor ex(threads);
        auto r = sim::World::create(spec, sim::TypeRegistry::empty(), {}, {}, {}, ex);
        REQUIRE(r.ok());
        CHECK(ex.jobs() == 8); // one job per island
        CHECK(ex.calls() == 1);
        CHECK(r.value().state_hash() == sim::World(serial).state_hash());
        CHECK(r.value().save() == serial.save());
    }
}

// ---------------------------------------------------------------- the stream

TEST_CASE("island fields are counted") {
    // The canaries of state_fields.hpp, restated: a member added to a persistent
    // island type without a line in its fields() function fails the build there.
    CHECK(sim::field_count<sim::Island>() == 14);
    CHECK(sim::field_count<sim::Drought>() == 10);
    CHECK(sim::field_count<sim::SweepState>() == 5);
    CHECK(sim::field_count<sim::CityRing>() == 2);
    CHECK(sim::field_count<sim::ObjectManagerState>() == 2);
    CHECK(sim::field_count<sim::Deposit>() == 5);
    // Every field of an island with a drought and a deposit is one primitive of the
    // trace: 7 header + 1 digest + sweep 5 + 1 + objmgr 2 + ring 2 + presence 1 +
    // drought 9 + mask 2 + deposits (count 1 + str 2 + 4) + reserved 1 + tiles scope
    // (3 dims + 4 columns of 6).
    sim::Island is;
    is.drought.emplace();
    is.deposits.emplace_back();
    REQUIRE(is.tiles.reset(0, 0, 3, 2, 8));
    sim::TraceArchive trace;
    fields(trace, std::as_const(is));
    std::size_t prims = 0;
    for (const sim::TraceEntry& e : trace.entries()) {
        if (e.kind != sim::Prim::ScopeOpen && e.kind != sim::Prim::ScopeClose) {
            ++prims;
        }
    }
    // index, x0, y0, w, h, climate, next_serial (7), terrain digest (1), sweep (5),
    // event300 (1), objmgr (2), ring (2), drought presence + 9 + the city mask (count +
    // one word, stream 108), deposits count +
    // goods length + goods bytes + x + y + remaining + reserve (7), reserved (1),
    // tiles: 3 dims + 6 x 4 columns, cities: the slot count (no slot in this island).
    CHECK(prims == 7 + 1 + 5 + 1 + 2 + 2 + 12 + 7 + 1 + 3 + 24 + 1);
}

TEST_CASE("islands scope stream layout") {
    const sim::World w = make_world(syn::two_island_scenario());
    const std::vector<std::uint8_t> bytes = island_test::flat_stream(w);
    // The scope is followed only by the figures and the events scopes of a world
    // without either: u64 slot count, then per slot a presence byte and the island;
    // the tiles of an island are 13 bytes each.
    Stream want;
    want.u64(4);
    auto island = [&](const sim::Island& is, std::int32_t x0, std::int32_t y0, std::uint32_t wd,
                      std::uint32_t ht, std::uint32_t city_no, std::uint32_t city_owner) {
        want.u8(1);
        want.u32(is.index.v);
        want.i32(x0);
        want.i32(y0);
        want.u32(wd);
        want.u32(ht);
        want.u32(is.climate);
        want.u32(1);
        want.u64(is.terrain ? is.terrain->digest : 0);
        // sweep (u16 stamp, 4 x u32), event300 (u16), objmgr (u16, u32), ring (u16,
        // u32): all zero.
        for (int i = 0; i < 4; ++i) {
            want.u16(0);
        }
        want.u32(0);
        want.u32(0);
        want.u32(0);
        want.u32(0);
        want.u32(0);
        want.u32(0);
        want.u8(0); // no drought
        want.u64(is.deposits.size());
        for (const sim::Deposit& d : is.deposits) {
            want.str(d.goods);
            want.i32(d.x);
            want.i32(d.y);
            want.u32(d.remaining);
            want.boolean(d.reserve);
        }
        want.u64(0);
        want.u32(wd);
        want.u32(ht);
        want.u32(8);
        for (std::uint64_t i = 0; i < is.tiles.tile_count(); ++i) {
            want.u32(is.tiles.object(i));
        }
        for (std::uint64_t i = 0; i < is.tiles.tile_count(); ++i) {
            want.u8(is.tiles.phase(i));
        }
        for (std::uint64_t i = 0; i < is.tiles.tile_count(); ++i) {
            want.u32(is.tiles.owner(i));
        }
        for (std::uint64_t i = 0; i < is.tiles.tile_count(); ++i) {
            want.u32(is.tiles.aux(i));
        }
        // The scope cities: 16 slots, the island's one city present.
        want.u64(16);
        for (std::uint32_t slot = 0; slot < 16; ++slot) {
            if (slot == city_no) {
                want.u8(1);
                island_test::append_empty_city(want, city_no, city_owner);
            } else {
                want.u8(0);
            }
        }
    };
    island(*w.islands()[0], 100, 100, 64, 64, 0, 0);
    want.u8(0);
    want.u8(0);
    island(*w.islands()[3], 300, 200, 30, 20, 1, 1);
    REQUIRE(bytes.size() > want.v.size());
    CHECK(std::equal(want.v.begin(), want.v.end(),
                     bytes.end() -
                         static_cast<std::ptrdiff_t>(island_test::kTailBytes + want.v.size())));
    // 13 bytes per tile: the two islands hold 4096 + 600 tiles.
    CHECK(want.v.size() > (4096 + 600) * sim::kTileBytes);
    // The scope opens where the trace says.
    const std::size_t scope_at = bytes.size() - island_test::kTailBytes - want.v.size();
    CHECK(island_test::flat_offset(w, "islands") == scope_at);
    CHECK(island_test::flat_offset(w, "island", 0) == scope_at + 8);
    CHECK(island_test::flat_offset(w, "figures") == scope_at + want.v.size());
    CHECK(island_test::flat_offset(w, "island", 3) != std::string::npos);
    CHECK(island_test::flat_offset(w, "island", 4) == std::string::npos);
}

TEST_CASE("the raw hash of a world is the FNV of its stream") {
    // The invariant of S1 (HashArchive raw mode == WriteArchive bytes == Trace bytes)
    // extends to islands and tiles.
    const sim::World w = make_world(syn::two_island_scenario());
    const std::vector<std::uint8_t> bytes = island_test::flat_stream(w);
    sim::StateHasher fnv;
    fnv.bytes(bytes);
    sim::HashArchive raw(sim::HashOptions{nullptr, 0, true});
    fields(raw, w);
    CHECK(raw.finish() == fnv.value());
    sim::TraceArchive trace;
    fields(trace, w);
    CHECK(trace.bytes_written() == bytes);
}

TEST_CASE("islands scope hash tree names the island") {
    sim::World w = make_world(syn::two_island_scenario());
    sim::HashTree tree;
    CHECK(w.state_hash(tree, 2) == w.state_hash());
    const sim::HashNode* islands = nullptr;
    for (const sim::HashNode& c : tree.root.children) {
        if (c.name == "islands") {
            islands = &c;
        }
    }
    REQUIRE(islands != nullptr);
    // One scope per table slot, present or not, indexed by the slot; the tiles
    // and the cities of a present island are child scopes.
    REQUIRE(islands->children.size() == 4);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(islands->children[i].name == "island");
        CHECK(islands->children[i].indexed);
        CHECK(islands->children[i].index == i);
    }
    CHECK(islands->children[0].children.size() == 2);
    CHECK(islands->children[1].children.empty());
    CHECK(islands->children[2].children.empty());
    REQUIRE(islands->children[3].children.size() == 2);
    CHECK(islands->children[3].children[0].name == "tiles");
    CHECK_FALSE(islands->children[3].children[0].indexed);
    CHECK(islands->children[3].children[1].name == "cities");
    CHECK_FALSE(islands->children[3].children[1].indexed);
    // An absent slot hashes as the single byte 0.
    sim::StateHasher absent;
    absent.u8(0);
    CHECK(islands->children[1].hash == absent.value());
    // At depth 1 the island has no children yet.
    sim::HashTree shallow;
    static_cast<void>(w.state_hash(shallow, 1));
    for (const sim::HashNode& c : shallow.root.children) {
        if (c.name == "islands") {
            REQUIRE(c.children.size() == 4);
            CHECK(c.children[3].children.empty());
        }
    }
    // A tile write moves its tiles scope, its island, the table and the root, and
    // nothing else.
    isl(w, 3).tiles.set_object(5, 77);
    sim::HashTree changed;
    CHECK(w.state_hash(changed, 2) != tree.root.hash);
    const sim::HashNode* islands2 = nullptr;
    for (const sim::HashNode& c : changed.root.children) {
        if (c.name == "islands") {
            islands2 = &c;
        }
    }
    REQUIRE(islands2 != nullptr);
    CHECK(islands2->hash != islands->hash);
    CHECK(islands2->children[3].hash != islands->children[3].hash);
    CHECK(islands2->children[3].children[0].hash != islands->children[3].children[0].hash);
    CHECK(islands2->children[0].hash == islands->children[0].hash);
    CHECK(islands2->children[1].hash == islands->children[1].hash);
    for (std::size_t i = 0; i < tree.root.children.size(); ++i) {
        if (tree.root.children[i].name != "islands") {
            CHECK(changed.root.children[i].hash == tree.root.children[i].hash);
        }
    }
}

TEST_CASE("island perturbation moves the hash") {
    using Mut = void (*)(sim::Island&);
    const std::vector<std::pair<const char*, Mut>> mutations = {
        {"index", [](sim::Island& i) { i.index = sim::IslandId{9}; }},
        {"x0", [](sim::Island& i) { i.tiles.reset(i.tiles.x0() + 1, i.tiles.y0(), 30, 20, 8); }},
        {"y0", [](sim::Island& i) { i.tiles.reset(i.tiles.x0(), i.tiles.y0() - 1, 30, 20, 8); }},
        {"width", [](sim::Island& i) { i.tiles.reset(i.tiles.x0(), i.tiles.y0(), 20, 30, 8); }},
        {"height", [](sim::Island& i) { i.tiles.reset(i.tiles.x0(), i.tiles.y0(), 30, 19, 8); }},
        {"chunk edge",
         [](sim::Island& i) { i.tiles.reset(i.tiles.x0(), i.tiles.y0(), 30, 20, 4); }},
        {"climate", [](sim::Island& i) { i.climate = 3; }},
        {"next_serial", [](sim::Island& i) { i.next_serial = 2; }},
        {"terrain removed", [](sim::Island& i) { i.terrain.reset(); }},
        {"terrain replaced", [](sim::Island& i) { i.terrain = sim::make_terrain({9}, {}, {}); }},
        {"sweep stamp", [](sim::Island& i) { i.sweep.stamp = 1; }},
        {"sweep grow_x", [](sim::Island& i) { i.sweep.grow_x = 1; }},
        {"sweep grow_y", [](sim::Island& i) { i.sweep.grow_y = 1; }},
        {"sweep fauna_x", [](sim::Island& i) { i.sweep.fauna_x = 1; }},
        {"sweep fauna_y", [](sim::Island& i) { i.sweep.fauna_y = 1; }},
        {"event300_stamp", [](sim::Island& i) { i.event300_stamp = 1; }},
        {"objmgr stamp", [](sim::Island& i) { i.objmgr.stamp = 1; }},
        {"objmgr cursor", [](sim::Island& i) { i.objmgr.cursor = 1; }},
        {"ring stamp", [](sim::Island& i) { i.ring.stamp = 1; }},
        {"ring cursor", [](sim::Island& i) { i.ring.cursor = 1; }},
        {"drought present", [](sim::Island& i) { i.drought.emplace(); }},
        {"deposit goods", [](sim::Island& i) { i.deposits[0].goods = "EISEN"; }},
        {"deposit x", [](sim::Island& i) { i.deposits[0].x += 1; }},
        {"deposit y", [](sim::Island& i) { i.deposits[1].y += 1; }},
        {"deposit remaining", [](sim::Island& i) { i.deposits[1].remaining -= 1; }},
        {"deposit reserve", [](sim::Island& i) { i.deposits[0].reserve = false; }},
        {"deposit added", [](sim::Island& i) { i.deposits.emplace_back(); }},
        {"deposit removed", [](sim::Island& i) { i.deposits.pop_back(); }},
        {"deposits swapped", [](sim::Island& i) { std::swap(i.deposits[0], i.deposits[1]); }},
        {"reserved_object_count", [](sim::Island& i) { i.reserved_object_count = 1; }},
        {"first tile object", [](sim::Island& i) { i.tiles.set_object(0, 1); }},
        {"first tile phase", [](sim::Island& i) { i.tiles.set_phase(0, 1); }},
        {"first tile owner", [](sim::Island& i) { i.tiles.set_owner(0, 1); }},
        {"first tile aux", [](sim::Island& i) { i.tiles.set_aux(0, 1); }},
        {"last tile object", [](sim::Island& i) { i.tiles.set_object(599, 1); }},
        {"last tile phase", [](sim::Island& i) { i.tiles.set_phase(599, 1); }},
        {"last tile owner", [](sim::Island& i) { i.tiles.set_owner(599, 1); }},
        {"last tile aux", [](sim::Island& i) { i.tiles.set_aux(599, 1); }},
    };
    const sim::World base = make_world(zero_island_scenario());
    const std::uint64_t base_hash = sim::World(base).state_hash();
    const std::vector<std::uint64_t> base_scopes = scope_hashes(base);
    REQUIRE(base_scopes.size() == 9);
    for (const auto& [name, mutate] : mutations) {
        INFO("mutation " << name);
        sim::World w = base;
        mutate(isl(w, 2));
        CHECK(w.state_hash() != base_hash);
        const std::vector<std::uint64_t> s = scope_hashes(w);
        for (std::size_t k = 0; k < s.size(); ++k) {
            if (k == 5) {
                CHECK(s[k] != base_scopes[k]); // islands
            } else {
                CHECK(s[k] == base_scopes[k]);
            }
        }
    }
    // The drought record, field by field, from a world that has one.
    sim::World with_drought = base;
    isl(with_drought, 2).drought.emplace();
    const std::uint64_t drought_hash = sim::World(with_drought).state_hash();
    CHECK(drought_hash != base_hash);
    const std::vector<std::pair<const char*, void (*)(sim::Drought&)>> drought = {
        {"x", [](sim::Drought& d) { d.x = 1; }},
        {"y", [](sim::Drought& d) { d.y = 1; }},
        {"target_ring", [](sim::Drought& d) { d.target_ring = 7; }},
        {"ring", [](sim::Drought& d) { d.ring = -1; }},
        {"city_mask", [](sim::Drought& d) { d.city_mask = {1}; }},
        {"city_mask words", [](sim::Drought& d) { d.city_mask = {0, 0}; }},
        {"phase", [](sim::Drought& d) { d.phase = 1; }},
        {"step", [](sim::Drought& d) { d.step = 1; }},
        {"state", [](sim::Drought& d) { d.state = 0; }},
        {"counter", [](sim::Drought& d) { d.counter = 1; }},
        {"stamp", [](sim::Drought& d) { d.stamp = 1; }},
    };
    for (const auto& [name, mutate] : drought) {
        INFO("drought " << name);
        sim::World w = with_drought;
        mutate(*isl(w, 2).drought);
        CHECK(w.state_hash() != drought_hash);
    }
    // The table itself: the presence of a slot and its length.
    sim::World gone = base;
    sim::WorldTestAccess::islands(gone)[2].reset();
    CHECK(gone.state_hash() != base_hash);
    sim::World longer = base;
    sim::WorldTestAccess::islands(longer).resize(4);
    CHECK(longer.state_hash() != base_hash);
    sim::World moved = base;
    std::swap(sim::WorldTestAccess::islands(moved)[2], sim::WorldTestAccess::islands(moved)[0]);
    CHECK(moved.state_hash() != base_hash);
}

TEST_CASE("advance ticks the islands but writes no tile") {
    // Stage S7: advance ticks the islands (the sweep cursors, the object manager, the
    // city ring: stamps and cursors move), but the growth sweep has no tile body
    // before stage S13, so no tile column changes. Stage S12: the world-level steps
    // move the two accumulators (ids), the registry's stamp (figures) and the timers'
    // last tick (timers_monopoly).
    sim::World w = make_world(syn::two_island_scenario());
    const std::vector<std::uint64_t> before = scope_hashes(w);
    const sim::World created = w;
    for (int i = 0; i < 300; ++i) {
        w.advance(10);
    }
    const std::vector<std::uint64_t> after = scope_hashes(w);
    CHECK(after[5] != before[5]); // islands: the stamps and cursors moved
    CHECK(after[1] != before[1]); // clock
    CHECK(after[4] != before[4]); // players: the table's stamp and cursor moved
    CHECK(after[0] == before[0]); // header
    CHECK(after[2] == before[2]); // settings
    CHECK(after[3] != before[3]); // ids: nothing was spawned, the accumulators moved
    CHECK(w.ids().next_figure == created.ids().next_figure);
    CHECK(w.ids().next_event_seq == created.ids().next_event_seq);
    CHECK(after[6] != before[6]); // figures: no figure, the registry's stamp moved
    CHECK(w.figures().by_id.empty());
    CHECK(after[7] == before[7]); // events
    CHECK(after[8] != before[8]); // timers_monopoly: the timers' last tick moved
    for (std::size_t slot : {std::size_t{0}, std::size_t{3}}) {
        const sim::TileLayer& now = w.islands()[slot]->tiles;
        const sim::TileLayer& then = created.islands()[slot]->tiles;
        REQUIRE(now.tile_count() == then.tile_count());
        bool same = true;
        for (std::uint64_t i = 0; i < now.tile_count(); ++i) {
            same = same && now.object(i) == then.object(i) && now.phase(i) == then.phase(i) &&
                   now.owner(i) == then.owner(i) && now.aux(i) == then.aux(i);
        }
        CHECK(same);
        CHECK(now.cache_consistent());
        CHECK(now.ignored_writes() == 0);
    }
}

TEST_CASE("world copy is an independent snapshot with islands") {
    sim::World a = make_world(syn::two_island_scenario());
    const std::uint64_t h = sim::World(a).state_hash();
    sim::World b = a;
    // The copies hold the same terrain object (immutable, shared) but their own tiles.
    CHECK(a.islands()[3]->terrain == b.islands()[3]->terrain);
    isl(b, 3).tiles.set_owner(1, 4);
    isl(b, 0).deposits[0].remaining = 5;
    sim::WorldTestAccess::islands(b)[3]->deposits.clear();
    CHECK(sim::World(a).state_hash() == h);
    CHECK(b.state_hash() != h);
    CHECK(a.islands()[3]->tiles.owner(1) == 1);
    CHECK(a.islands()[3]->deposits.size() == 2);
    // Assigning back restores the hash, also for a moved-from source.
    b = a;
    CHECK(b.state_hash() == h);
    sim::World c(std::move(b));
    CHECK(c.state_hash() == h);
}

// ------------------------------------------------- reading what was written

namespace {

struct ReadRow {
    const char* name;
    void (*mutate)(sim::World&);
    const char* expect;
};

/// A re-dimensioned layer is all zero, and owner 0 is not a city of the island
/// (stage S6): make every tile neutral so the rows below meet their own fact.
void neutral_owners(sim::Island& is) {
    for (std::uint64_t i = 0; i < is.tiles.tile_count(); ++i) {
        is.tiles.set_owner(i, 15);
    }
}

std::vector<ReadRow> invalid_island_rows() {
    return {
        {"index is not the slot", [](sim::World& w) { isl(w, 3).index = sim::IslandId{2}; },
         "islands[3].index: is 2, the slot is 3"},
        {"empty rectangle", [](sim::World& w) { isl(w, 3).tiles.reset(300, 200, 0, 20, 8); },
         "islands[3].tiles: the island rectangle is empty"},
        {"empty rectangle by height",
         [](sim::World& w) { isl(w, 3).tiles.reset(300, 200, 30, 0, 8); },
         "islands[3].tiles: the island rectangle is empty"},
        {"chunk edge is not the configuration's",
         [](sim::World& w) { isl(w, 3).tiles.reset(300, 200, 30, 20, 4); },
         "islands[3].tiles: chunk_edge is 4, SimConfig.chunk_edge is 8"},
        {"outside the world on the right",
         [](sim::World& w) { isl(w, 3).tiles.reset(500, 200, 30, 20, 8); },
         "islands[3].tiles: the rectangle is not inside the world (512 x 512)"},
        {"outside the world below",
         [](sim::World& w) { isl(w, 3).tiles.reset(300, 493, 30, 20, 8); },
         "islands[3].tiles: the rectangle is not inside the world"},
        {"negative origin x", [](sim::World& w) { isl(w, 3).tiles.reset(-1, 200, 30, 20, 8); },
         "islands[3].tiles: the rectangle is not inside the world"},
        {"negative origin y", [](sim::World& w) { isl(w, 3).tiles.reset(300, -5, 30, 20, 8); },
         "islands[3].tiles: the rectangle is not inside the world"},
        {"overlap",
         [](sim::World& w) {
             isl(w, 3).tiles.reset(163, 163, 30, 20, 8);
             neutral_owners(isl(w, 3));
             isl(w, 3).deposits.clear();
         },
         "islands[3].tiles: the rectangle overlaps islands[0]"},
        {"overlap with the same rectangle",
         [](sim::World& w) {
             isl(w, 3).tiles.reset(100, 100, 64, 64, 8);
             neutral_owners(isl(w, 3));
             isl(w, 3).deposits.clear();
         },
         "islands[3].tiles: the rectangle overlaps islands[0]"},
        {"climate", [](sim::World& w) { isl(w, 3).climate = 7; },
         "islands[3].climate: 7 has no entry"},
        {"next_serial 0", [](sim::World& w) { isl(w, 3).next_serial = 0; },
         "islands[3].next_serial: must be at least 1"},
        {"owner above neutral", [](sim::World& w) { isl(w, 3).tiles.set_owner(5, 16); },
         "islands[3].tiles: tile 5 has owner 16, above neutral_owner 15"},
        {"grow_x", [](sim::World& w) { isl(w, 3).sweep.grow_x = 30; },
         "islands[3].sweep: a sweep position is outside the island"},
        {"grow_y", [](sim::World& w) { isl(w, 3).sweep.grow_y = 20; },
         "islands[3].sweep: a sweep position is outside the island"},
        {"fauna_x", [](sim::World& w) { isl(w, 3).sweep.fauna_x = 30; },
         "islands[3].sweep: a sweep position is outside the island"},
        {"fauna_y", [](sim::World& w) { isl(w, 3).sweep.fauna_y = 20; },
         "islands[3].sweep: a sweep position is outside the island"},
        {"grow_x is not a chunk multiple", [](sim::World& w) { isl(w, 3).sweep.grow_x = 4; },
         "islands[3].sweep: a sweep position is not a multiple of SimConfig.chunk_edge"},
        {"grow_y is not a chunk multiple", [](sim::World& w) { isl(w, 3).sweep.grow_y = 12; },
         "islands[3].sweep: a sweep position is not a multiple of SimConfig.chunk_edge"},
        {"fauna_x is not a chunk multiple", [](sim::World& w) { isl(w, 3).sweep.fauna_x = 1; },
         "islands[3].sweep: a sweep position is not a multiple of SimConfig.chunk_edge"},
        {"fauna_y is not a chunk multiple", [](sim::World& w) { isl(w, 3).sweep.fauna_y = 19; },
         "islands[3].sweep: a sweep position is not a multiple of SimConfig.chunk_edge"},
        {"reserved objects", [](sim::World& w) { isl(w, 3).reserved_object_count = 1; },
         "islands[3].reserved_object_count: is 1, must be 0 until the island objects are ported"},
        {"ring cursor", [](sim::World& w) { isl(w, 3).ring.cursor = 16; },
         "islands[3].ring.cursor: 16 is outside the 16 city slots"},
        {"object manager cursor", [](sim::World& w) { isl(w, 3).objmgr.cursor = 2; },
         "islands[3].objmgr.cursor: 2 is outside the 2 deposits"},
        {"object manager cursor without deposits",
         [](sim::World& w) {
             isl(w, 3).deposits.clear();
             isl(w, 3).objmgr.cursor = 1;
         },
         "islands[3].objmgr.cursor: 1 is outside the 0 deposits"},
        {"deposit left of the island", [](sim::World& w) { isl(w, 3).deposits[0].x = 299; },
         "islands[3].deposits[0]: the position is outside the island"},
        {"deposit right of the island", [](sim::World& w) { isl(w, 3).deposits[1].x = 330; },
         "islands[3].deposits[1]: the position is outside the island"},
        {"deposit above the island", [](sim::World& w) { isl(w, 3).deposits[0].y = 199; },
         "islands[3].deposits[0]: the position is outside the island"},
        {"deposit below the island", [](sim::World& w) { isl(w, 3).deposits[1].y = 220; },
         "islands[3].deposits[1]: the position is outside the island"},
        {"deposit goods", [](sim::World& w) { isl(w, 3).deposits[1].goods = "NOSUCH"; },
         "islands[3].deposits[1].goods: unknown goods name 'NOSUCH'"},
        {"drought state",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->state = 2;
         },
         "islands[3].drought.state: 2 is not 0, 1 or 255"},
        // phase, step and counter are strictly below their limits: the machine
        // resets them in the tick that reaches the limit (jb, D 0x1008b4f7 ...).
        {"drought phase at its limit",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->phase = 4;
         },
         "islands[3].drought.phase: 4 is not below SimConfig.drought_phases"},
        {"drought phase",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->phase = 5;
         },
         "islands[3].drought.phase: 5 is not below SimConfig.drought_phases"},
        {"drought step at its limit",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->step = 64;
         },
         "islands[3].drought.step: 64 is not below SimConfig.drought_steps * chunk_edge"},
        {"drought step",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->step = 65;
         },
         "islands[3].drought.step: 65 is not below"},
        {"drought counter at its limit",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->counter = 120;
         },
         "islands[3].drought.counter: 120 is not below SimConfig.drought_plateau_count"},
        {"drought counter",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->counter = 121;
         },
         "islands[3].drought.counter: 121 is not below"},
        {"drought x is not a chunk multiple",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->x = 4;
         },
         "islands[3].drought.x/y: 4, 0 are not multiples of SimConfig.chunk_edge"},
        {"drought y is not a chunk multiple",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->y = 9;
         },
         "islands[3].drought.x/y: 0, 9 are not multiples of SimConfig.chunk_edge"},
        {"drought x outside the world",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->x = 512;
         },
         "islands[3].drought.x/y: 512, 0 are outside the world"},
        {"drought y outside the world",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->y = 512;
         },
         "islands[3].drought.x/y: 0, 512 are outside the world"},
        {"drought ring below 0",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->ring = -1;
         },
         "islands[3].drought.ring: -1 is not in 0 .. target_ring (6)"},
        {"drought ring above the target",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->ring = 7;
         },
         "islands[3].drought.ring: 7 is not in 0 .. target_ring (6)"},
        {"drought target ring beyond i32",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->target_ring = 0xFFFFFFFFU;
             isl(w, 3).drought->ring = -1;
         },
         "islands[3].drought.target_ring: 4294967295 is above 2147483647 (the ring is an i32)"},
        {"drought ring below 0 under the largest target",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->target_ring = 0x7FFFFFFFU;
             isl(w, 3).drought->ring = -1;
         },
         "islands[3].drought.ring: -1 is not in 0 .. target_ring (2147483647)"},
        {"drought city mask beyond the city slots",
         [](sim::World& w) {
             isl(w, 3).drought.emplace();
             isl(w, 3).drought->city_mask = {1U << 16};
         },
         "islands[3].drought.city_mask: bits beyond the 16 city slots are set"},
        {"too many slots", [](sim::World& w) { sim::WorldTestAccess::islands(w).resize(257); },
         "island_slots"},
        {"first island fails first",
         [](sim::World& w) {
             isl(w, 0).climate = 9;
             isl(w, 3).climate = 9;
         },
         "islands[0].climate"},
    };
}

} // namespace

TEST_CASE("island rects outside the world or overlapping are rejected") {
    // Outside the world: past the right edge, below it, left of or above the origin.
    struct Rect {
        std::int32_t x0, y0, w, h;
    };
    for (const Rect& r : {Rect{500, 0, 13, 10}, Rect{0, 500, 10, 13}, Rect{-1, 0, 10, 10},
                          Rect{0, -1, 10, 10}, Rect{2147483647, 0, 10, 10}}) {
        sim::ScenarioSpec out = syn::empty_scenario();
        out.islands.push_back(syn::plain_island(0, r.x0, r.y0, static_cast<std::uint32_t>(r.w),
                                                static_cast<std::uint32_t>(r.h)));
        INFO("island at " << r.x0 << "," << r.y0);
        CHECK(
            contains(create_error(out), "islands[0].tiles: the rectangle is not inside the world"));
    }
    // Exactly to the edge is inside.
    {
        sim::ScenarioSpec edge = syn::empty_scenario();
        edge.islands.push_back(syn::plain_island(0, 500, 500, 12, 12));
        CHECK(create_error(edge) == "");
    }
    // Sorted by x0 the islands are 0 (x 0..99, y 0..9), 1 (x 10..19, y 50..59), 2 (x 30..39,
    // y 5..7): 2 overlaps 0, and island 1 lies between them in the order.
    sim::ScenarioSpec spec = syn::empty_scenario();
    spec.islands.push_back(syn::plain_island(0, 0, 0, 100, 10));
    spec.islands.push_back(syn::plain_island(1, 10, 50, 10, 10));
    CHECK(create_error(spec) == "");
    spec.islands.push_back(syn::plain_island(2, 30, 5, 10, 3));
    CHECK(starts_with(create_error(spec), "islands[2].tiles: the rectangle overlaps islands[0]"));
    // The same in another table order (the sweep order is by position, not slot).
    std::swap(spec.islands[0].slot, spec.islands[2].slot);
    CHECK(starts_with(create_error(spec), "islands[2].tiles: the rectangle overlaps islands[0]"));
    // One tile apart in every direction: no overlap. The islands touch the world
    // origin and the far corner.
    sim::ScenarioSpec ok = syn::empty_scenario();
    ok.islands.push_back(syn::plain_island(0, 0, 0, 100, 10));
    ok.islands.push_back(syn::plain_island(1, 100, 0, 10, 10));   // right of 0
    ok.islands.push_back(syn::plain_island(2, 0, 10, 100, 10));   // below 0
    ok.islands.push_back(syn::plain_island(3, 100, 10, 10, 10));  // diagonal
    ok.islands.push_back(syn::plain_island(4, 502, 502, 10, 10)); // the far corner
    // Touching vertically with partly shared x ranges, in both orders of the x sort.
    ok.islands.push_back(syn::plain_island(5, 5, 40, 10, 10));
    ok.islands.push_back(syn::plain_island(6, 0, 50, 10, 10)); // starts where 5 ends
    ok.islands.push_back(syn::plain_island(7, 200, 60, 10, 10));
    ok.islands.push_back(syn::plain_island(8, 205, 70, 10, 10)); // starts where 7 ends
    CHECK(create_error(ok) == "");
    // A row of thin islands, each sharing the x range of the next in a long list.
    sim::ScenarioSpec thin = syn::empty_scenario();
    for (std::uint32_t i = 0; i < 40; ++i) {
        thin.islands.push_back(
            syn::plain_island(i, 20, static_cast<std::int32_t>(10 * i), 400, 10));
    }
    CHECK(create_error(thin) == "");
    thin.islands[17].h = 11;
    CHECK(starts_with(create_error(thin), "islands[18].tiles: the rectangle overlaps islands[17]"));
}

TEST_CASE("islands scope refuses a slot count the table cannot hold") {
    // An absent slot is one byte on the wire and a whole optional<Island> in memory:
    // a count with the bytes to back it (a million zero bytes) is refused by the
    // table bound before a single slot is made.
    const sim::World none = make_world(syn::empty_scenario());
    Stream count;
    count.u64(1000000);
    count.v.insert(count.v.end(), std::size_t{1000000}, std::uint8_t{0});
    const std::vector<std::uint8_t> bytes = island_test::with_island_table(none, count.v);
    sim::World target(5);
    const std::string err = island_test::read_flat(target, bytes);
    CHECK(starts_with(err, "islands"));
    CHECK(contains(err, "slot count 1000000 is above SimConfig.island_slots 256"));
    CHECK(target.islands().empty());
    // The bound is the configuration's: 256 absent slots are a valid table, 257 are not.
    for (const std::uint64_t n : {std::uint64_t{256}, std::uint64_t{257}}) {
        Stream c;
        c.u64(n);
        c.v.insert(c.v.end(), static_cast<std::size_t>(n), std::uint8_t{0});
        const std::vector<std::uint8_t> b = island_test::with_island_table(none, c.v);
        sim::World t2(5);
        const std::string e = island_test::read_flat(t2, b);
        if (n == 256) {
            CHECK(e == "");
            CHECK(t2.islands().size() == 256);
        } else {
            CHECK(contains(e, "slot count 257 is above"));
        }
    }
}

TEST_CASE("drought record stream layout") {
    sim::Drought d;
    d.x = 0x01010101;
    d.y = 0x02020202;
    d.target_ring = 0x03030303;
    d.ring = -4;
    d.city_mask = {0x05060708, 0x0F101112};
    d.phase = 0x09;
    d.step = 0x0A;
    d.state = 0xFF;
    d.counter = 0x0B0C;
    d.stamp = 0x0D0E;
    Stream want;
    want.u32(0x01010101);
    want.u32(0x02020202);
    want.u32(0x03030303);
    want.i32(-4);
    want.u64(2); // the city mask: u64 count + u32 words (O-55, stream 108)
    want.u32(0x05060708);
    want.u32(0x0F101112);
    want.u8(0x09);
    want.u8(0x0A);
    want.u8(0xFF);
    want.u16(0x0B0C);
    want.u16(0x0D0E);
    sim::WriteArchive wa;
    fields(wa, std::as_const(d));
    CHECK(wa.data() == want.v);
    CHECK(want.v.size() == 4 + 4 + 4 + 4 + 8 + 4 + 4 + 1 + 1 + 1 + 2 + 2);
    // Inside an island it follows the ring behind a presence byte of 1.
    sim::Island is;
    REQUIRE(is.tiles.reset(0, 0, 1, 1, 8));
    is.drought = d;
    sim::WriteArchive ia;
    fields(ia, std::as_const(is));
    const std::vector<std::uint8_t>& b = ia.data();
    // index, x0, y0, w, h, climate, next_serial, terrain digest, sweep (18), event300,
    // objmgr (6), ring (6): 4 * 7 + 8 + 18 + 2 + 6 + 6 = 68 bytes, then the presence byte.
    REQUIRE(b.size() > 69 + want.v.size());
    CHECK(b[68] == 1);
    CHECK(std::equal(want.v.begin(), want.v.end(), b.begin() + 69));
    // Read back, every field.
    sim::Island back;
    sim::ReadArchive r(b);
    fields(r, back);
    REQUIRE_FALSE(r.failed());
    REQUIRE(back.drought.has_value());
    CHECK(back.drought->x == d.x);
    CHECK(back.drought->y == d.y);
    CHECK(back.drought->target_ring == d.target_ring);
    CHECK(back.drought->ring == -4);
    CHECK(back.drought->city_mask == d.city_mask);
    CHECK(back.drought->phase == 0x09);
    CHECK(back.drought->step == 0x0A);
    CHECK(back.drought->state == 0xFF);
    CHECK(back.drought->counter == 0x0B0C);
    CHECK(back.drought->stamp == 0x0D0E);
}

TEST_CASE("island validation sees a stale chunk digest") {
    sim::World w = make_world(syn::two_island_scenario());
    w.prepare_hash(sim::serial_executor());
    CHECK(sim::validate_islands(w.islands(), w.config(), w.settings(), w.types()) == "");
    island_test::CorruptingArchive a;
    a.at = 3;
    sim::TileIO::columns(a, isl(w, 3).tiles);
    CHECK(contains(sim::validate_islands(w.islands(), w.config(), w.settings(), w.types()),
                   "islands[3].tiles: a cached chunk digest is out of date"));
}

TEST_CASE("world create checks that the registry fits the configuration") {
    // A type whose service mask uses bit 16 needs service_bits >= 17.
    sim::BuildingType t;
    t.id = sim::TypeId{5};
    t.prodtype = 65;
    t.cls = sim::RuntimeClass::PublicService;
    t.service_mask = 1U << 16;
    sim::TypeRegistry::Builder b;
    b.add(t);
    auto built = b.build();
    REQUIRE(built.ok());
    const auto registry = built.value();
    CHECK(sim::World::create(syn::basic_scenario(), registry).ok());
    sim::SimConfig narrow;
    narrow.service_bits = 16;
    auto r = sim::World::create(syn::basic_scenario(), registry, narrow);
    REQUIRE_FALSE(r.ok());
    CHECK(starts_with(r.error(), "registry does not fit the configuration"));
    // The same refusal on load: the configuration comes from the save, and the
    // registry the caller supplies must fit it.
    const sim::World nw = make_world(syn::basic_scenario(), narrow);
    auto back = sim::World::load(nw.save(), registry);
    REQUIRE_FALSE(back.ok());
    CHECK(contains(back.error(), "registry does not fit the configuration"));
}

TEST_CASE("islands scope reads back and validates what it read") {
    const sim::World w = make_world(syn::two_island_scenario());
    const std::vector<std::uint8_t> bytes = island_test::flat_stream(w);
    SUBCASE("a fresh world reads it back to the same hash and the same bytes") {
        sim::World other(5);
        REQUIRE(island_test::read_flat(other, bytes) == "");
        CHECK(other.state_hash() == sim::World(w).state_hash());
        REQUIRE(other.islands().size() == 4);
        CHECK_FALSE(other.islands()[1].has_value());
        const sim::Island& b = *other.islands()[3];
        CHECK(b.rect() == sim::TileRect{300, 200, 30, 20});
        CHECK(b.tiles.owner(299) == 1);
        CHECK(b.tiles.object(599) == 2002);
        CHECK(b.deposits[0].goods == "GOLD");
        // The terrain is a placeholder that carries the digest (the blobs travel in
        // the save frame), and the stream is identical.
        REQUIRE(b.terrain != nullptr);
        CHECK(b.terrain->digest == w.islands()[3]->terrain->digest);
        CHECK(island_test::flat_stream(other) == bytes);
    }
    SUBCASE("every prefix fails") {
        // The tail of the stream is where the islands are; shorter prefixes fail in
        // the earlier scopes. Sample the whole stream, and every cut inside the
        // islands scope.
        const std::size_t from = island_test::flat_offset(w, "islands");
        for (std::size_t n = 0; n < bytes.size();
             n += (n < from || (n > from + 400 && n + 400 < bytes.size())) ? std::size_t{37}
                                                                           : std::size_t{1}) {
            sim::World target(5);
            const std::vector<std::uint8_t> prefix(bytes.begin(),
                                                   bytes.begin() + static_cast<std::ptrdiff_t>(n));
            REQUIRE(island_test::read_flat(target, prefix) != "");
        }
    }
    SUBCASE("trailing bytes are not part of the stream") {
        std::vector<std::uint8_t> longer = bytes;
        longer.push_back(0);
        sim::World target(5);
        CHECK(island_test::read_flat(target, longer) == "trailing bytes");
    }
    SUBCASE("each broken fact is refused and names the island") {
        for (const ReadRow& row : invalid_island_rows()) {
            INFO("row " << row.name);
            sim::World bad = w;
            row.mutate(bad);
            sim::World target(5);
            const std::uint64_t before = target.state_hash();
            const std::string err = island_test::read_flat(target, island_test::flat_stream(bad));
            CHECK(starts_with(err, "islands"));
            CHECK(contains(err, row.expect));
            // A failed read leaves the target unchanged.
            CHECK(target.state_hash() == before);
            CHECK(target.islands().empty());
            // The same refusal through the save frame.
            const std::string frame = island_test::load_error(bad.save());
            CHECK(contains(frame, row.expect));
            // ... and the frame says where in the file the failing section starts (the
            // byte offsets inside the message count from there).
            const std::size_t start =
                island_test::frame_offset(island_test::flat_offset(w, "islands"), 5);
            CHECK(contains(frame, "section islands (starts at file byte " + std::to_string(start) +
                                      "): "));
        }
    }
    SUBCASE("the edges of every bound are accepted") {
        using Edit = void (*)(sim::World&);
        const std::vector<std::pair<const char*, Edit>> edges = {
            {"drought at its limits",
             [](sim::World& x) {
                 isl(x, 3).drought.emplace();
                 isl(x, 3).drought->phase = 3;
                 isl(x, 3).drought->step = 63;
                 isl(x, 3).drought->counter = 119;
                 isl(x, 3).drought->state = 0xff;
                 isl(x, 3).drought->x = 504;
                 isl(x, 3).drought->y = 504;
                 isl(x, 3).drought->ring = 6;
                 isl(x, 3).drought->city_mask = {0xFFFF};
             }},
            {"drought ring and target 0",
             [](sim::World& x) {
                 isl(x, 3).drought.emplace();
                 isl(x, 3).drought->target_ring = 0;
                 isl(x, 3).drought->ring = 0;
             }},
            {"drought ring at the i32 end",
             [](sim::World& x) {
                 isl(x, 3).drought.emplace();
                 isl(x, 3).drought->target_ring = 0x7FFFFFFFU; // kMaxDroughtRing (S13)
                 isl(x, 3).drought->ring = 0x7FFFFFFF;
             }},
            {"drought plateau",
             [](sim::World& x) {
                 isl(x, 3).drought.emplace();
                 isl(x, 3).drought->state = 0;
             }},
            {"sweep in the last chunk",
             [](sim::World& x) { isl(x, 3).sweep = {7, 24, 16, 24, 16}; }},
            {"ring cursor in the last slot", [](sim::World& x) { isl(x, 3).ring.cursor = 15; }},
            {"object manager in the last deposit",
             [](sim::World& x) { isl(x, 3).objmgr.cursor = 1; }},
            {"next serial 1", [](sim::World& x) { isl(x, 3).next_serial = 1; }},
            {"owner at neutral", [](sim::World& x) { isl(x, 3).tiles.set_owner(0, 15); }},
            {"island at the world corner",
             [](sim::World& x) {
                 isl(x, 3).tiles.reset(482, 492, 30, 20, 8);
                 neutral_owners(isl(x, 3));
                 isl(x, 3).deposits.clear();
                 isl(x, 3).objmgr.cursor = 0;
             }},
            {"island at the world origin",
             [](sim::World& x) {
                 isl(x, 3).tiles.reset(0, 0, 30, 20, 8);
                 neutral_owners(isl(x, 3));
                 // Deposits on the top-left and bottom-right tiles of the rectangle.
                 isl(x, 3).deposits[0].x = 0;
                 isl(x, 3).deposits[0].y = 0;
                 isl(x, 3).deposits[1].x = 29;
                 isl(x, 3).deposits[1].y = 19;
             }},
            {"deposit on the top-left tile",
             [](sim::World& x) {
                 isl(x, 3).deposits[0].x = 300;
                 isl(x, 3).deposits[0].y = 200;
             }},
            {"adjacent islands",
             [](sim::World& x) {
                 isl(x, 3).tiles.reset(164, 100, 30, 20, 8);
                 neutral_owners(isl(x, 3));
                 isl(x, 3).deposits[0].x = 164;
                 isl(x, 3).deposits[0].y = 100;
                 isl(x, 3).deposits[1].x = 193;
                 isl(x, 3).deposits[1].y = 119;
             }},
            {"all 256 slots", [](sim::World& x) { sim::WorldTestAccess::islands(x).resize(256); }},
        };
        for (const auto& [name, edit] : edges) {
            INFO("edge " << name);
            sim::World ok = w;
            edit(ok);
            sim::World target(5);
            CHECK(island_test::read_flat(target, island_test::flat_stream(ok)) == "");
            CHECK(island_test::load_error(ok.save()) == "");
        }
    }
}

// ----------------------------------------------------- rectangles before build

TEST_CASE("world create checks the rectangles before it builds anything") {
    // A tile costs 13 bytes. The island of every row below is within the per-island
    // bound but its rectangle cannot lie in the 512 x 512 world, so the error has to
    // come before any tile column is sized: the executor that builds the islands is
    // never asked to run, and the test itself allocates nothing.
    struct Row {
        const char* name;
        std::int32_t x0;
        std::uint32_t w, h;
        const char* expect;
    };
    const std::vector<Row> rows = {
        {"exactly the tile bound, 1 x 2^26", 0, 1, 1U << 26,
         "islands[0].tiles: the rectangle is not inside the world (512 x 512)"},
        {"exactly the tile bound, 8192 x 8192", 0, 8192, 8192,
         "islands[0].tiles: the rectangle is not inside the world (512 x 512)"},
        {"2^26 x 1, wider than the world", 0, 1U << 26, 1,
         "islands[0].tiles: the rectangle is not inside the world (512 x 512)"},
        {"one tile above the bound", 0, 1, (1U << 26) + 1,
         "islands[0]: 67108865 tiles, more than 67108864"},
        {"inside the bound but off the right edge", 400, 8192, 4,
         "islands[0].tiles: the rectangle is not inside the world (512 x 512)"},
    };
    for (const Row& row : rows) {
        INFO("row " << row.name);
        sim::ScenarioSpec spec = syn::empty_scenario();
        spec.islands.push_back(syn::plain_island(0, row.x0, 0, row.w, row.h));
        island_test::ThreadExecutor ex(2);
        auto r = sim::World::create(spec, sim::TypeRegistry::empty(), sim::SimConfig{},
                                    sim::Rules{}, sim::Seams{}, ex);
        REQUIRE_FALSE(r.ok());
        CHECK(starts_with(r.error(), row.expect));
        CHECK(ex.calls() == 0);
        CHECK(ex.jobs() == 0);
    }
    // Many islands of the largest size: each is within the bound, together they are
    // far beyond the world, and none is built.
    {
        sim::ScenarioSpec spec = syn::empty_scenario();
        for (std::uint32_t i = 0; i < 8; ++i) {
            spec.islands.push_back(syn::plain_island(i, 0, 0, 8192, 8192));
        }
        island_test::ThreadExecutor ex(2);
        auto r = sim::World::create(spec, sim::TypeRegistry::empty(), sim::SimConfig{},
                                    sim::Rules{}, sim::Seams{}, ex);
        REQUIRE_FALSE(r.ok());
        CHECK(starts_with(r.error(), "islands[0].tiles: the rectangle is not inside the world"));
        CHECK(ex.calls() == 0);
    }
    // Rectangles that are inside the world but together cover more than its area
    // overlap by necessity; the count is checked before the sweep.
    {
        sim::ScenarioSpec spec = syn::empty_scenario();
        spec.islands.push_back(syn::plain_island(0, 0, 0, 512, 512));
        spec.islands.push_back(syn::plain_island(1, 0, 0, 512, 512));
        island_test::ThreadExecutor ex(2);
        auto r = sim::World::create(spec, sim::TypeRegistry::empty(), sim::SimConfig{},
                                    sim::Rules{}, sim::Seams{}, ex);
        REQUIRE_FALSE(r.ok());
        CHECK(r.error() ==
              "islands: the rectangles cover 524288 tiles, more than the 262144 of the world");
        CHECK(ex.calls() == 0);
    }
    // Overlapping rectangles are found before the build as well.
    {
        sim::ScenarioSpec spec = syn::empty_scenario();
        spec.islands.push_back(syn::plain_island(0, 10, 10, 50, 50));
        spec.islands.push_back(syn::plain_island(1, 59, 59, 50, 50));
        island_test::ThreadExecutor ex(2);
        auto r = sim::World::create(spec, sim::TypeRegistry::empty(), sim::SimConfig{},
                                    sim::Rules{}, sim::Seams{}, ex);
        REQUIRE_FALSE(r.ok());
        CHECK(starts_with(r.error(), "islands[1].tiles: the rectangle overlaps islands[0]"));
        CHECK(ex.calls() == 0);
    }
    // The boundary of the area rule: four islands that tile the world exactly are
    // accepted (the sum equals the world's area), one tile less per island too, and
    // the executor is used once the checks pass.
    {
        sim::ScenarioSpec spec = syn::empty_scenario();
        spec.islands.push_back(syn::plain_island(0, 0, 0, 256, 256));
        spec.islands.push_back(syn::plain_island(1, 256, 0, 256, 256));
        spec.islands.push_back(syn::plain_island(2, 0, 256, 256, 256));
        spec.islands.push_back(syn::plain_island(3, 256, 256, 256, 256));
        island_test::ThreadExecutor ex(2);
        auto r = sim::World::create(spec, sim::TypeRegistry::empty(), sim::SimConfig{},
                                    sim::Rules{}, sim::Seams{}, ex);
        REQUIRE(r.ok());
        CHECK(ex.calls() >= 1);
        CHECK(ex.jobs() >= 4);
        CHECK(r.value().islands().size() == 4);
    }
    // The same rectangle rules apply to the built table (the loader's path): a
    // table of rectangles that cover more than the world is refused with the same
    // message.
    {
        sim::World w = make_world(syn::two_island_scenario());
        isl(w, 3).tiles.reset(0, 0, 512, 512, 8);
        isl(w, 3).deposits.clear();
        isl(w, 0).tiles.reset(0, 0, 512, 512, 8);
        neutral_owners(isl(w, 3));
        neutral_owners(isl(w, 0));
        const std::string err =
            sim::validate_islands(w.islands(), w.config(), w.settings(), w.types());
        CHECK(err ==
              "islands: the rectangles cover 524288 tiles, more than the 262144 of the world");
    }
}

TEST_CASE("world create drops no tile write") {
    for (const sim::ScenarioSpec& spec : {syn::basic_scenario(), syn::two_island_scenario()}) {
        const sim::World w = make_world(spec);
        for (const std::optional<sim::Island>& is : w.islands()) {
            if (is.has_value()) {
                CHECK(is->tiles.ignored_writes() == 0);
            }
        }
    }
}

TEST_CASE("a drought record needs the city mask to hold the city slots") {
    // O-55 (stage S13): ceil(city_slots / 32) words, bit k % 32 of word k / 32 for city
    // slot k, no bit at or beyond city_slots.
    auto drought_error = [](std::uint32_t city_slots, std::vector<std::uint32_t> mask) {
        sim::SimConfig cfg;
        cfg.city_slots = city_slots;
        // neutral_owner must be at most city_slots; every tile is neutral here.
        cfg.neutral_owner = std::min<std::uint32_t>(cfg.neutral_owner, city_slots);
        sim::ScenarioSpec spec = syn::two_island_scenario();
        for (sim::ScenarioSpec::IslandSpec& is : spec.islands) {
            is.owner_runs.clear();
            is.cities.clear();
        }
        sim::World w = make_world(spec, cfg);
        isl(w, 3).drought.emplace();
        isl(w, 3).drought->city_mask = std::move(mask);
        return sim::validate_islands(w.islands(), w.config(), w.settings(), w.types());
    };
    CHECK(drought_error(32, {0xFFFFFFFFU}) == "");
    CHECK(drought_error(31, {0x7FFFFFFFU}) == "");
    CHECK(contains(drought_error(31, {0x80000000U}),
                   "islands[3].drought.city_mask: bits beyond the 31 city slots are set"));
    CHECK(drought_error(1, {1}) == "");
    CHECK(contains(drought_error(1, {2}),
                   "islands[3].drought.city_mask: bits beyond the 1 city slots are set"));
    CHECK(drought_error(16, {0xFFFF}) == "");
    // 33 slots: two words, the second holds slot 32 only.
    CHECK(drought_error(33, {0xFFFFFFFFU, 1}) == "");
    CHECK(contains(drought_error(33, {0, 2}),
                   "islands[3].drought.city_mask: bits beyond the 33 city slots are set"));
    CHECK(contains(drought_error(33, {0}),
                   "islands[3].drought.city_mask: has 1 words, SimConfig.city_slots 33 needs 2"));
    CHECK(contains(drought_error(16, {0, 0}), "has 2 words, SimConfig.city_slots 16 needs 1"));
    CHECK(contains(drought_error(16, {}), "has 0 words, SimConfig.city_slots 16 needs 1"));
    // 64 slots: two full words; 300 slots: ten words, the last holds slots 288..299.
    CHECK(drought_error(64, {0xFFFFFFFFU, 0xFFFFFFFFU}) == "");
    std::vector<std::uint32_t> ten(10, 0);
    ten[9] = 0xFFFU;
    CHECK(drought_error(300, ten) == "");
    ten[9] = 0x1000U;
    CHECK(contains(drought_error(300, ten), "bits beyond the 300 city slots are set"));
    {
        // Through the save frame: a record sized for 33 slots saves and loads with the bit
        // of slot 32; one word is refused.
        sim::SimConfig cfg;
        cfg.city_slots = 33;
        sim::World w = make_world(syn::two_island_scenario(), cfg);
        CHECK(island_test::load_error(w.save()) == "");
        isl(w, 3).drought.emplace();
        isl(w, 3).drought->city_mask = {0, 1};
        CHECK(island_test::load_error(w.save()) == "");
        isl(w, 3).drought->city_mask = {0};
        CHECK(contains(island_test::load_error(w.save()), "has 1 words"));
    }
}

// ----------------------------------------------------------------- terrain

TEST_CASE("terrain digest covers the three columns and their lengths") {
    const auto a = sim::make_terrain({1, 2}, {3}, {4});
    CHECK(a->digest != 0);
    CHECK(sim::terrain_matches(*a));
    CHECK(sim::terrain_digest(*a) == a->digest);
    CHECK(sim::make_terrain({1, 2}, {3}, {4})->digest == a->digest);
    // Every value matters.
    CHECK(sim::make_terrain({1, 3}, {3}, {4})->digest != a->digest);
    CHECK(sim::make_terrain({1, 2}, {5}, {4})->digest != a->digest);
    CHECK(sim::make_terrain({1, 2}, {3}, {5})->digest != a->digest);
    // The u16 column is two bytes per value.
    CHECK(sim::make_terrain({1, 2}, {0x0103}, {4})->digest != a->digest);
    CHECK(sim::make_terrain({1, 2}, {0x0300}, {4})->digest != a->digest);
    // The columns are not interchangeable and the boundaries between them matter.
    CHECK(sim::make_terrain({1}, {2}, {3})->digest != sim::make_terrain({1}, {3}, {2})->digest);
    CHECK(sim::make_terrain({1, 2}, {}, {4})->digest != sim::make_terrain({1}, {2}, {4})->digest);
    CHECK(sim::make_terrain({}, {}, {1, 2})->digest != sim::make_terrain({1, 2}, {}, {})->digest);
    CHECK(sim::make_terrain({1, 2}, {}, {})->digest != sim::make_terrain({1}, {}, {2})->digest);
    // 0 is the stream's "no terrain": a raw FNV result of 0 is stored as 1, any
    // other result as it is.
    CHECK(sim::nonzero_digest(0) == 1);
    CHECK(sim::nonzero_digest(1) == 1);
    CHECK(sim::nonzero_digest(2) == 2);
    CHECK(sim::nonzero_digest(0xFFFFFFFFFFFFFFFFULL) == 0xFFFFFFFFFFFFFFFFULL);
    // An empty terrain still has a (non-zero) digest: 0 means "no terrain".
    CHECK(sim::make_terrain({}, {}, {})->digest != 0);
    // A stored digest that is not the columns' is detected.
    sim::Terrain t = *a;
    t.digest += 1;
    CHECK_FALSE(sim::terrain_matches(t));
    t = *a;
    t.height[0] = 9;
    CHECK_FALSE(sim::terrain_matches(t));
}

// ------------------------------------------------------------------ the frame

TEST_CASE("save load save identical with islands") {
    sim::World w = make_world(syn::two_island_scenario());
    for (int i = 0; i < 100; ++i) {
        w.advance(10);
    }
    isl(w, 3).drought.emplace();
    isl(w, 3).drought->x = 8;
    isl(w, 3).drought->state = 0xff;
    isl(w, 3).next_serial = 12;
    isl(w, 0).objmgr.stamp = 5;
    const std::vector<std::uint8_t> bytes = w.save();
    CHECK(bytes[0] == 'A');
    CHECK(bytes[1] == '3');
    CHECK(bytes[2] == 'S');
    CHECK(bytes[3] == 'V');
    CHECK(island_test::get32(bytes, 4) == sim::kSaveVersion);
    CHECK(island_test::get32(bytes, 8) == sim::kStateHashVersion);
    CHECK(bytes[12] == 0); // the original PRNG table
    sim::World back = island_test::load_world(bytes);
    CHECK(back.state_hash() == w.state_hash());
    CHECK(back.save() == bytes);
    CHECK(back.clock().ms == w.clock().ms);
    CHECK(back.clock().last_tick == w.clock().last_tick);
    CHECK(back.rng().cursor() == w.rng().cursor());
    REQUIRE(back.islands().size() == 4);
    CHECK_FALSE(back.islands()[1].has_value());
    const sim::Island& b = *back.islands()[3];
    REQUIRE(b.terrain != nullptr);
    CHECK(b.terrain->height == w.islands()[3]->terrain->height);
    CHECK(b.terrain->gfx == w.islands()[3]->terrain->gfx);
    CHECK(b.terrain->fertility == w.islands()[3]->terrain->fertility);
    CHECK(b.terrain->digest == w.islands()[3]->terrain->digest);
    CHECK(sim::terrain_matches(*b.terrain));
    REQUIRE(b.drought.has_value());
    CHECK(b.drought->x == 8);
    CHECK(b.drought->state == 0xff);
    CHECK(b.next_serial == 12);
    CHECK(back.islands()[0]->objmgr.stamp == 5);
}

TEST_CASE("save load continues identically") {
    // advance() moves the clock, the PRNG-free player table and nothing on the
    // islands until S7, so this pins the continuation of the clock and the players;
    // the continuation of island state (sweeps, growth, the drought) is tested from
    // S7 on, where an island has something to continue.
    sim::World w = make_world(syn::two_island_scenario());
    for (int i = 0; i < 50; ++i) {
        w.advance(7);
    }
    sim::World back = island_test::load_world(w.save());
    for (int i = 0; i < 400; ++i) {
        w.advance(10);
        back.advance(10);
        REQUIRE(back.state_hash() == w.state_hash());
    }
    // And a world loaded twice from one save agrees with itself.
    sim::World again = island_test::load_world(w.save());
    CHECK(again.state_hash() == w.state_hash());
}

TEST_CASE("save load with a custom PRNG table and without islands") {
    sim::ScenarioSpec spec = syn::basic_scenario(1234);
    spec.rng_table = sim::msvc_rand_sequence(100, 5);
    const sim::World w = make_world(spec);
    const std::vector<std::uint8_t> bytes = w.save();
    CHECK(bytes[12] == 1);
    CHECK(island_test::get32(bytes, 13) == 100); // u64 count, low word
    sim::World back = island_test::load_world(bytes);
    CHECK(back.rng().size() == 100);
    CHECK(back.rng().cursor() == 34);
    CHECK(back.rng().table_digest() == w.rng().table_digest());
    CHECK(back.state_hash() == w.state_hash());
    CHECK(back.save() == bytes);
    // The draws agree.
    sim::World a = w;
    for (int i = 0; i < 5; ++i) {
        CHECK(sim::WorldTestAccess::rng(a).next() == sim::WorldTestAccess::rng(back).next());
    }
    // A world without islands, and the default empty world.
    const sim::World none = make_world(syn::empty_scenario(3));
    const std::vector<std::uint8_t> nb = none.save();
    sim::World nback = island_test::load_world(nb);
    CHECK(nback.islands().empty());
    CHECK(nback.save() == nb);
    sim::World def;
    sim::World dback = island_test::load_world(def.save());
    CHECK(dback.state_hash() == def.state_hash());
}

TEST_CASE("load refuses a damaged frame") {
    // A small world keeps the loops below cheap: one 8 x 8 island with a terrain.
    sim::ScenarioSpec spec = syn::empty_scenario(9);
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(1, 40, 40, 8, 8);
    is.terrain = sim::make_terrain({1, 2, 3}, {4, 5}, {6, 7, 8, 9});
    is.owner_runs = {{1, 10}};
    spec.islands.push_back(is);
    island_test::add_owner_cities(spec);
    const sim::World w = make_world(spec);
    const std::vector<std::uint8_t> good = w.save();
    REQUIRE(island_test::load_error(good) == "");
    SUBCASE("too short") {
        CHECK(contains(island_test::load_error({}), "too short"));
        CHECK(contains(island_test::load_error({'A', '3', 'S', 'V'}), "too short"));
        CHECK(contains(island_test::load_error(std::vector<std::uint8_t>(11, 0)), "too short"));
        // Twelve bytes are a frame without a body: the checksum is what fails.
        CHECK(contains(island_test::load_error(std::vector<std::uint8_t>(12, 0)), "checksum"));
    }
    SUBCASE("every byte is under the checksum") {
        for (std::size_t i = 0; i < good.size(); ++i) {
            std::vector<std::uint8_t> bad = good;
            bad[i] ^= 0x01;
            INFO("byte " << i);
            REQUIRE(contains(island_test::load_error(bad), "checksum"));
        }
    }
    SUBCASE("every strict prefix fails, also with a valid checksum") {
        for (std::size_t n = 0; n < good.size(); ++n) {
            std::vector<std::uint8_t> bad(good.begin(),
                                          good.begin() + static_cast<std::ptrdiff_t>(n));
            INFO("prefix " << n);
            REQUIRE(island_test::load_error(bad) != "");
            if (n > 13) {
                island_test::reseal(bad);
                REQUIRE(island_test::load_error(bad) != "");
            }
        }
    }
    SUBCASE("the magic") {
        std::vector<std::uint8_t> bad = good;
        bad[0] = 'B';
        island_test::reseal(bad);
        CHECK(contains(island_test::load_error(bad), "not a save"));
    }
    SUBCASE("the save version") {
        std::vector<std::uint8_t> bad = good;
        island_test::put32(bad, 4, sim::kSaveVersion + 1);
        island_test::reseal(bad);
        const std::string err = island_test::load_error(bad);
        CHECK(contains(err, "version 2 is not supported"));
        CHECK(contains(err, "this build reads 1"));
    }
    SUBCASE("the stream version") {
        std::vector<std::uint8_t> bad = good;
        island_test::put32(bad, 8, sim::kStateHashVersion - 1);
        island_test::reseal(bad);
        const std::string err = island_test::load_error(bad);
        CHECK(contains(err, "state stream version " + std::to_string(sim::kStateHashVersion - 1) +
                                " is not supported"));
    }
    SUBCASE("the PRNG table kind") {
        std::vector<std::uint8_t> bad = good;
        bad[12] = 2;
        island_test::reseal(bad);
        CHECK(contains(island_test::load_error(bad), "unknown PRNG table kind 2"));
        bad[12] = 1; // claims a custom table the bytes do not hold
        island_test::reseal(bad);
        CHECK(island_test::load_error(bad) != "");
    }
    SUBCASE("section lengths") {
        // The header section's length is the u64 at byte 13.
        std::vector<std::uint8_t> bad = good;
        island_test::put32(bad, 13, 0xFFFFFFFFU);
        island_test::reseal(bad);
        CHECK(contains(island_test::load_error(bad), "claims"));
        bad = good;
        island_test::put32(bad, 17, 0x7FFFFFFFU); // the high word: more than a size
        island_test::reseal(bad);
        CHECK(contains(island_test::load_error(bad), "claims"));
        // A section one byte too long swallows a byte of the next length.
        bad = good;
        island_test::put32(bad, 13, island_test::get32(good, 13) + 1);
        island_test::reseal(bad);
        CHECK(contains(island_test::load_error(bad), "bytes after its last field"));
        // One byte too short ends the section inside its last field.
        bad = good;
        island_test::put32(bad, 13, island_test::get32(good, 13) - 1);
        island_test::reseal(bad);
        CHECK(island_test::load_error(bad) != "");
    }
    SUBCASE("terrain blobs") {
        // The last 8 bytes are the checksum, the 4 bytes before it the fertility
        // column, before that its count; the presence byte of the island is in front
        // of the three columns (8 + 3, 8 + 4, 8 + 4 bytes).
        const std::size_t blob = 1 + (8 + 3) + (8 + 4) + (8 + 4);
        const std::size_t present_at = good.size() - 8 - blob;
        REQUIRE(good[present_at] == 1);
        std::vector<std::uint8_t> bad = good;
        bad[good.size() - 8 - 1] ^= 0x10; // a fertility value
        island_test::reseal(bad);
        CHECK(contains(island_test::load_error(bad), "do not match the digest"));
        bad = good;
        bad[present_at] = 0; // the stream says there is a terrain
        island_test::reseal(bad);
        CHECK(contains(island_test::load_error(bad), "is absent but the island's terrain digest"));
        bad = good;
        bad[present_at] = 2; // not a bool
        island_test::reseal(bad);
        CHECK(island_test::load_error(bad) != "");
        // Bytes after the last blob.
        bad = good;
        bad.insert(bad.end() - 8, 0);
        island_test::reseal(bad);
        CHECK(contains(island_test::load_error(bad), "bytes after the last blob"));
    }
    SUBCASE("a terrain blob for an island that has none") {
        sim::ScenarioSpec none = syn::empty_scenario(9);
        none.islands.push_back(syn::plain_island(0, 40, 40, 8, 8));
        const sim::World nw = make_world(none);
        std::vector<std::uint8_t> bad = nw.save();
        const std::size_t present_at = bad.size() - 8 - 1;
        REQUIRE(bad[present_at] == 0);
        bad[present_at] = 1;
        island_test::reseal(bad);
        CHECK(contains(island_test::load_error(bad), "is present but the island's terrain digest"));
    }
}

TEST_CASE("the terrain blobs of the save frame have the documented layout") {
    // Transcribed from docs/sim/state.md §9.5: per present island in slot order a
    // presence bool and, when present, height (u64 n + bytes), gfx (u64 n + u16s),
    // fertility (u64 n + bytes). The columns have different counts and widths, so a
    // writer and a reader that swapped two of them consistently would show here.
    sim::ScenarioSpec spec = syn::empty_scenario(4);
    sim::ScenarioSpec::IslandSpec a = syn::plain_island(1, 0, 0, 10, 10);
    a.terrain = sim::make_terrain({1, 2, 3}, {0x0405, 0x0607}, {8});
    sim::ScenarioSpec::IslandSpec b = syn::plain_island(3, 50, 0, 10, 10);
    sim::ScenarioSpec::IslandSpec c = syn::plain_island(5, 100, 0, 10, 10);
    c.terrain = sim::make_terrain({9}, {}, {10, 11, 12, 13});
    spec.islands = {a, b, c};
    const sim::World w = make_world(spec);
    const std::vector<std::uint8_t> bytes = w.save();
    Stream want;
    want.boolean(true);
    want.u64(3);
    want.u8(1);
    want.u8(2);
    want.u8(3);
    want.u64(2);
    want.u16(0x0405);
    want.u16(0x0607);
    want.u64(1);
    want.u8(8);
    want.boolean(false);
    want.boolean(true);
    want.u64(1);
    want.u8(9);
    want.u64(0);
    want.u64(4);
    want.u8(10);
    want.u8(11);
    want.u8(12);
    want.u8(13);
    REQUIRE(bytes.size() > want.v.size() + 8);
    const auto from = bytes.end() - 8 - static_cast<std::ptrdiff_t>(want.v.size());
    CHECK(std::equal(want.v.begin(), want.v.end(), from));
    // The reader takes the same layout back, column by column.
    const sim::World back = island_test::load_world(bytes);
    REQUIRE(back.islands().size() == 6);
    const sim::Terrain& ta = *back.islands()[1]->terrain;
    CHECK(ta.height == std::vector<std::uint8_t>{1, 2, 3});
    CHECK(ta.gfx == std::vector<std::uint16_t>{0x0405, 0x0607});
    CHECK(ta.fertility == std::vector<std::uint8_t>{8});
    CHECK(back.islands()[3]->terrain == nullptr);
    const sim::Terrain& tc = *back.islands()[5]->terrain;
    CHECK(tc.height == std::vector<std::uint8_t>{9});
    CHECK(tc.gfx.empty());
    CHECK(tc.fertility == std::vector<std::uint8_t>{10, 11, 12, 13});
}

TEST_CASE("a custom PRNG table of the original length is saved as custom") {
    // The table is identified by its contents, not by its length: 2048 entries of
    // another seed (the likeliest mod) must not be written as "the original".
    sim::ScenarioSpec spec = syn::basic_scenario(77);
    spec.rng_table = sim::msvc_rand_sequence(2048, 99);
    const sim::World w = make_world(spec);
    REQUIRE(w.rng().size() == 2048);
    const std::vector<std::uint8_t> bytes = w.save();
    CHECK(bytes[12] == 1);
    CHECK(island_test::get32(bytes, 13) == 2048);
    const sim::World back = island_test::load_world(bytes);
    CHECK(back.rng().size() == 2048);
    CHECK(back.rng().table_digest() == w.rng().table_digest());
    CHECK(back.rng().cursor() == w.rng().cursor());
    CHECK(back.state_hash() == w.state_hash());
    CHECK(back.save() == bytes);
    // The original table, given explicitly, is the compact form.
    sim::ScenarioSpec orig = syn::basic_scenario(77);
    orig.rng_table = sim::msvc_rand_sequence(2048, 1);
    const sim::World ow = make_world(orig);
    CHECK(ow.save()[12] == 0);
    CHECK(island_test::load_world(ow.save()).state_hash() == ow.state_hash());
}

TEST_CASE("load refuses a PRNG table count the remaining bytes cannot back") {
    sim::ScenarioSpec spec = syn::basic_scenario(1234);
    spec.rng_table = sim::msvc_rand_sequence(100, 5);
    const std::vector<std::uint8_t> good = make_world(spec).save();
    REQUIRE(island_test::load_error(good) == "");
    // The u64 count is at byte 13, the entries (u16) follow it at byte 21.
    const std::size_t remaining = good.size() - 8 - 21;
    // Each entry takes two bytes: a count of half the remaining bytes plus one is
    // too much, although as a byte count it would still fit (n <= remaining).
    for (const std::size_t n : {remaining / 2 + 1, remaining}) {
        std::vector<std::uint8_t> bad = good;
        island_test::put32(bad, 13, static_cast<std::uint32_t>(n));
        island_test::reseal(bad);
        INFO("claimed entries " << n);
        CHECK(contains(island_test::load_error(bad), "count exceeds the remaining input"));
    }
}

TEST_CASE("load refuses a header whose config has a zero divisor before any container is sized") {
    const sim::World w = make_world(syn::basic_scenario());
    const std::vector<std::uint8_t> good = w.save();
    // The header section starts at byte 21 (13 + its u64 length): u32 stream
    // version, u32 PRNG timeline version, then SimConfig. island_slots is its
    // first field, chunk_edge its 16th, building_div its 20th.
    constexpr std::size_t kConfig = 21 + 8;
    REQUIRE(island_test::get32(good, kConfig) == 256);
    REQUIRE(island_test::get32(good, kConfig + 4 * 19) == 500);
    SUBCASE("a zero divisor") {
        std::vector<std::uint8_t> bad = good;
        island_test::put32(bad, kConfig + 4 * 19, 0);
        island_test::reseal(bad);
        const std::string err = island_test::load_error(bad);
        CHECK(contains(err, "SimConfig.building_div"));
        CHECK(contains(err, "header"));
        CHECK(contains(err, "section header (starts at file byte 21): "));
    }
    SUBCASE("a table size that would ask for gigabytes") {
        for (const std::size_t field : {std::size_t{0}, std::size_t{1}, std::size_t{3}}) {
            std::vector<std::uint8_t> bad = good;
            island_test::put32(bad, kConfig + 4 * field, 0xFFFFFFFFU);
            island_test::reseal(bad);
            const std::string err = island_test::load_error(bad);
            INFO("field " << field);
            CHECK(contains(err, "SimConfig."));
            CHECK(contains(err, "must be in"));
        }
    }
    SUBCASE("a chunk edge of zero") {
        std::vector<std::uint8_t> bad = good;
        island_test::put32(bad, kConfig + 4 * 15, 0);
        island_test::reseal(bad);
        CHECK(contains(island_test::load_error(bad), "SimConfig.chunk_edge"));
    }
    SUBCASE("another registry") {
        auto types = syn::make_synthetic_types();
        REQUIRE(types.ok());
        auto r = sim::World::load(good, types.value().registry);
        REQUIRE_FALSE(r.ok());
        CHECK(contains(r.error(), "registry digest differs"));
    }
    SUBCASE("other seam names") {
        sim::Seams seams;
        seams.city_is_empty.fn = [](void*, const sim::City&) { return false; };
        seams.city_is_empty.name = "other_rule";
        auto r = sim::World::load(good, sim::TypeRegistry::empty(), seams);
        REQUIRE_FALSE(r.ok());
        CHECK(contains(r.error(), "seam names differ"));
    }
    SUBCASE("a null registry") {
        auto r = sim::World::load(good, nullptr);
        REQUIRE_FALSE(r.ok());
        CHECK(contains(r.error(), "registry is null"));
    }
    SUBCASE("a save made with seams loads with the same seams") {
        sim::Seams seams;
        seams.city_is_empty.fn = [](void*, const sim::City&) { return false; };
        seams.city_is_empty.name = "my_rule";
        auto created =
            sim::World::create(syn::basic_scenario(), sim::TypeRegistry::empty(), {}, {}, seams);
        REQUIRE(created.ok());
        const std::vector<std::uint8_t> bytes = created.value().save();
        CHECK(contains(island_test::load_error(bytes), "seam names differ"));
        auto r = sim::World::load(bytes, sim::TypeRegistry::empty(), seams);
        REQUIRE(r.ok());
        CHECK(r.value().state_hash() == created.value().state_hash());
    }
}

TEST_CASE("load rejects tile dimensions beyond remaining bytes") {
    const sim::World w = make_world(syn::two_island_scenario());
    const std::vector<std::uint8_t> good = w.save();
    const std::size_t island0 = island_test::flat_offset(w, "island", 0);
    const std::size_t tiles0 = island_test::flat_offset(w, "tiles", 0);
    // The island scope starts with the presence byte, then u32 index, i32 x0, i32 y0.
    const std::size_t w_at = island_test::frame_offset(island0 + 1 + 12, 5);
    const std::size_t h_at = island_test::frame_offset(island0 + 1 + 16, 5);
    const std::size_t tw_at = island_test::frame_offset(tiles0, 5);
    const std::size_t th_at = island_test::frame_offset(tiles0 + 4, 5);
    const std::size_t te_at = island_test::frame_offset(tiles0 + 8, 5);
    REQUIRE(island_test::get32(good, w_at) == 64);
    REQUIRE(island_test::get32(good, h_at) == 64);
    REQUIRE(island_test::get32(good, tw_at) == 64);
    REQUIRE(island_test::get32(good, th_at) == 64);
    REQUIRE(island_test::get32(good, te_at) == 8);
    auto patched = [&](std::vector<std::pair<std::size_t, std::uint32_t>> edits) {
        std::vector<std::uint8_t> bad = good;
        for (const auto& [at, v] : edits) {
            island_test::put32(bad, at, v);
        }
        island_test::reseal(bad);
        return island_test::load_error(bad);
    };
    // 60 million tiles are inside kIslandTileBound but the section does not hold them.
    CHECK(contains(patched({{w_at, 30000}, {tw_at, 30000}, {h_at, 2000}, {th_at, 2000}}),
                   "tile columns need 60000000 x 13 bytes"));
    // Above the bound.
    CHECK(contains(patched({{w_at, 16384}, {tw_at, 16384}, {h_at, 16384}, {th_at, 16384}}),
                   "more than 67108864 tiles"));
    // w * h would overflow u32 (and u64 for the byte count if it were formed carelessly).
    CHECK(contains(
        patched(
            {{w_at, 0xFFFFFFFFU}, {tw_at, 0xFFFFFFFFU}, {h_at, 0xFFFFFFFFU}, {th_at, 0xFFFFFFFFU}}),
        "more than"));
    // The tile layer's dimensions differ from the island's.
    CHECK(contains(patched({{tw_at, 65}}), "differs from the island's"));
    CHECK(contains(patched({{th_at, 63}}), "differs from the island's"));
    CHECK(contains(patched({{w_at, 65}}), "differs from the island's"));
    // chunk_edge 0.
    CHECK(contains(patched({{te_at, 0}}), "chunk_edge is 0"));
    // A chunk_edge that is not the configuration's.
    CHECK(contains(patched({{te_at, 16}}), "chunk_edge is 16, SimConfig.chunk_edge is 8"));
    // A hostile slot count in the islands scope.
    {
        std::vector<std::uint8_t> bad = good;
        const std::size_t count_at =
            island_test::frame_offset(island_test::flat_offset(w, "islands"), 5);
        island_test::put32(bad, count_at, 0xFFFFFFFFU);
        island_test::put32(bad, count_at + 4, 0x00FFFFFFU);
        island_test::reseal(bad);
        const std::string err = island_test::load_error(bad);
        CHECK(contains(err, "count exceeds the remaining input"));
        island_test::put32(bad, count_at, 257);
        island_test::put32(bad, count_at + 4, 0);
        island_test::reseal(bad);
        CHECK(island_test::load_error(bad) != "");
    }
}

// ------------------------------------------------------------------ archives

TEST_CASE("archive skip and the raw flag") {
    const std::vector<std::uint8_t> data = {1, 2, 3, 4, 5, 6};
    sim::ReadArchive r(data);
    r.skip(2);
    CHECK_FALSE(r.failed());
    CHECK(r.position() == 2);
    std::uint8_t v = 0;
    r.u8(v);
    CHECK(v == 3);
    r.skip(3);
    CHECK_FALSE(r.failed());
    CHECK(r.remaining() == 0);
    r.skip(0);
    CHECK_FALSE(r.failed());
    r.skip(1);
    CHECK(r.failed());
    CHECK(r.error().find("unexpected end") != std::string::npos);
    // After an error skip does nothing.
    sim::ReadArchive q(data);
    q.skip(7);
    CHECK(q.failed());
    CHECK(q.position() == 0);
    q.skip(1);
    CHECK(q.position() == 0);
    // A skip of a huge count fails without overflowing the position.
    sim::ReadArchive big(data);
    big.skip(0xFFFFFFFFFFFFFFFFULL);
    CHECK(big.failed());
    CHECK(big.position() == 0);
    CHECK_FALSE(sim::HashArchive().raw());
    CHECK(sim::HashArchive(sim::HashOptions{nullptr, 0, true}).raw());
}

// -------------------------------------------------------------------- goldens

TEST_CASE("golden s5 islands") {
    sim::World w = make_world(syn::two_island_scenario(1234));
    int done = 0;
    for (const int checkpoint : {0, 100}) {
        while (done < checkpoint) {
            w.advance(10);
            ++done;
        }
        const std::string name =
            checkpoint == 0 ? "s5_islands_create" : "s5_islands_step" + std::to_string(checkpoint);
        std::string update_line;
        const std::string problem =
            anno::test::check_golden(name, "S5", w.state_hash(), update_line);
        if (anno::test::update_mode()) {
            MESSAGE("golden line: " << update_line);
        }
        INFO(name);
        CHECK(problem == "");
    }
    // The semantic side of the golden: the islands are as built, whatever the hash says.
    CHECK(w.islands()[3]->tiles.owner(299) == 1);
    CHECK(w.islands()[0]->tiles.object(4095) == 1203);
    CHECK(w.clock().ms == 1000);
}
