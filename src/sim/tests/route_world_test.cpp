// The route engine on island terrain (MAP-NAV-2 package C, parts c and d): the adapter's land rules
// on synthetic islands (four-corner average, zero boundary vertices, object predicate, the
// builder's block shortcut and its quirk), a coast index over the real shipped islands of the three
// ship-start-v2 candidates, routes between the seats walked with the independent legality check,
// the 15 saved geometric witness routes re-evaluated under the original rules, and the measurements
// that the default limits rest on (docs/sim/ship-route-limits.md,
// docs/sim/ship-route-witnesses.md). The real-data cases skip with a message when ANNO1503_GAME_DIR
// is unset.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/ship_manifest.hpp>
#include <anno/sim/route.hpp>
#include <anno/sim/route_world.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/ship_start.hpp>
#include <anno/test/game_dir.hpp>

#include "route_test_support.hpp"

namespace sim = anno::sim;
namespace sl = anno::sim_load;
namespace sm = anno::formats::ship_manifest;
namespace fs = std::filesystem;
using namespace route_test;

namespace {

std::uint32_t f32_bits(float f) {
    std::uint32_t b = 0;
    static_assert(sizeof(b) == sizeof(f));
    std::memcpy(&b, &f, sizeof b);
    return b;
}

/// A registry with plain object types whose HEAD f32 at 0x14 is the given scalar.
std::shared_ptr<const sim::TypeRegistry>
object_registry(const std::vector<std::pair<std::uint32_t, float>>& objects) {
    sim::TypeRegistry::Builder b;
    for (const auto& [id, f] : objects) {
        sim::BuildingType t;
        t.id = sim::TypeId{id};
        t.prodtype = 0;
        t.cls = sim::RuntimeClass::Plain;
        t.head_raw.assign(56, 0);
        const std::uint32_t bits = f32_bits(f);
        for (std::uint32_t i = 0; i < 4; ++i) {
            t.head_raw[0x14 + i] = static_cast<std::uint8_t>(bits >> (8 * i));
        }
        b.add(std::move(t));
    }
    auto r = b.build();
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return r.value();
}

sim::Island make_island(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h,
                        std::uint8_t fill) {
    sim::Island is;
    REQUIRE(is.tiles.reset(x0, y0, w, h, 8));
    is.terrain = sim::make_terrain(std::vector<std::uint8_t>(std::size_t{w} * h, fill), {}, {});
    return is;
}

void set_height(sim::Island& is, std::uint32_t x, std::uint32_t y, std::uint8_t v) {
    auto t = std::make_shared<sim::Terrain>(*is.terrain);
    t->height[std::size_t{y} * is.tiles.width() + x] = v;
    t->digest = sim::terrain_digest(*t);
    is.terrain = t;
}

void put_object(sim::Island& is, std::int32_t x, std::int32_t y, std::uint32_t id, bool anchor) {
    const sim::Slot idx = is.tiles.index_of(x, y);
    REQUIRE(idx != sim::kNoSlot);
    is.tiles.set_object(idx, id);
    is.tiles.set_foot_anchor(idx, anchor ? 1 : 0);
}

} // namespace

TEST_CASE("route world: object height math is exact (floor of f32 x 25, f32 / 0.04f)") {
    // {f32 bits, trunc(f * 25), trunc(f / 0.04f)} computed with exact rationals outside the code.
    const struct {
        std::uint32_t bits;
        std::int64_t h, top;
    } rows[] = {
        {0x00000000, 0, 0},     {0x3df5c28f, 2, 3},     {0x3e051eb8, 3, 3},
        {0x3e23d70a, 3, 4},     {0x3e2e147b, 4, 4},     {0x3f23d70a, 15, 16},
        {0x3f428f5c, 18, 19},   {0x3f451eb8, 19, 19},   {0x3f4ccccd, 20, 20},
        {0x3fb33333, 34, 35},   {0x40266666, 64, 64},   {0x40333333, 69, 70},
        {0x40400000, 75, 75},   {0x40666666, 89, 89},   {0x40933333, 114, 115},
        {0x3f000000, 12, 12},   {0x4123d70a, 255, 256}, {0xbda3d70a, -1, -2},
        {0xbff5c28f, -47, -48}, {0x40cccccd, 160, 160},
    };
    for (const auto& r : rows) {
        CAPTURE(r.bits);
        CHECK(sim::object_height_units(r.bits) == r.h);
        CHECK(sim::object_top_units(r.bits) == r.top);
    }
    CHECK(sim::object_height_units(0x7f800000) == 0); // infinity
    CHECK(sim::object_top_units(0x7fc00000) == 0);    // NaN
}

TEST_CASE("route world: the object blocking predicate against the objtile / objfoot fixtures") {
    std::uint32_t n = 0;
    for (const std::string& line : fixture_lines("object_shore_cases.txt")) {
        const auto parts = split(line, '|');
        if (line.rfind("objtile", 0) == 0) {
            const auto a = words(parts[0]);
            const std::int64_t h = std::stoll(a[1]);
            const std::uint32_t base = static_cast<std::uint32_t>(std::stoul(a[2]));
            CAPTURE(line);
            CHECK(sim::object_tile_blocks(h, base) == (parts[1] == "blocked"));
            ++n;
        } else if (line.rfind("objfoot", 0) == 0) {
            const auto a = words(parts[0]);
            const std::int64_t h = std::stoll(a[1]);
            const std::uint32_t base = static_cast<std::uint32_t>(std::stoul(a[2]));
            const std::int64_t tiles = std::stoll(a[3]) * std::stoll(a[4]);
            CAPTURE(line);
            CHECK((sim::object_tile_blocks(h, base) ? tiles : 0) == std::stoll(parts[1]));
            ++n;
        }
    }
    CHECK(n >= 16);
}

TEST_CASE("route world: terrain rule is the floor of the four-corner average, boundary vertices "
          "read zero") {
    sim::Island is = make_island(8, 8, 16, 16, 10);
    // Tile (2,2): corners 19, 20, 20, 20 -> sum 79 -> avg 19 -> water; (4,2): 20 x 4 -> land.
    set_height(is, 2, 2, 19);
    for (const auto& [x, y] : {std::pair{3, 2}, std::pair{2, 3}, std::pair{3, 3}}) {
        set_height(is, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 20);
    }
    for (const auto& [x, y] :
         {std::pair{4, 2}, std::pair{5, 2}, std::pair{4, 3}, std::pair{5, 3}}) {
        set_height(is, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 20);
    }
    // The last column: HEIGHT 40 at x = 15 -> its tile averages (40 + 0 + 40 + 0) / 4 = 20 -> land;
    // 36 -> 18 -> water (column W reads the zero padding).
    set_height(is, 15, 5, 40);
    set_height(is, 15, 6, 40);
    set_height(is, 15, 9, 36);
    set_height(is, 15, 10, 36);
    // The last row: row H reads zero too.
    set_height(is, 3, 15, 40);
    set_height(is, 4, 15, 40);
    const auto types = object_registry({});
    const std::vector<const sim::Island*> islands = {&is};
    const sim::WorldTileSource src(islands, *types, 64, 64);
    CHECK(src.tile_is_water(8 + 2, 8 + 2)); // 79 / 4 = 19
    CHECK_FALSE(src.tile_is_water(8 + 4, 8 + 2));
    CHECK_FALSE(src.tile_is_water(8 + 15, 8 + 5)); // (40 + 0 + 40 + 0) / 4 = 20
    CHECK(src.tile_is_water(8 + 15, 8 + 9));       // (36 + 0 + 36 + 0) / 4 = 18
    CHECK_FALSE(src.tile_is_water(8 + 3, 8 + 15)); // bottom row tile: (40 + 40 + 0 + 0) / 4 = 20
    CHECK(src.tile_is_water(8 + 8, 8 + 8));
    // Outside every island: open sea; outside the world: not water.
    CHECK(src.tile_is_water(0, 0));
    CHECK(src.tile_is_water(63, 63));
    CHECK_FALSE(src.tile_is_water(-1, 0));
    CHECK_FALSE(src.tile_is_water(64, 5));
    CHECK_FALSE(src.tile_is_water(5, 64));
}

TEST_CASE(
    "route world: object tiles block iff ((h + base) & 0xff) > 19, footprints are not exempt") {
    const auto types = object_registry({{9001, 0.77f}, {9002, 0.8f}, {9003, 0.13f}, {9004, 0.17f}});
    sim::Island is = make_island(0, 0, 32, 32, 0);
    put_object(is, 3, 3, 9001, true);  // h 19
    put_object(is, 5, 3, 9002, true);  // h 20
    put_object(is, 7, 3, 9003, false); // h 3, base 16 -> 19
    put_object(is, 9, 3, 9004, false); // h 4, base 16 -> 20
    put_object(is, 11, 3, 9999, true); // unknown type: no blocking
    const std::vector<const sim::Island*> islands = {&is};
    sim::RouteTerrainOptions opt;
    opt.item_base = [](const sim::Island&, std::uint64_t) { return std::uint8_t{16}; };
    const sim::WorldTileSource with_base(islands, *types, 32, 32, opt);
    CHECK_FALSE(with_base.tile_is_water(3, 3)); // 19 + 16 = 35 > 19
    CHECK_FALSE(with_base.tile_is_water(5, 3)); // 20 + 16 = 36
    CHECK(with_base.tile_is_water(7, 3));       // 3 + 16 = 19: free
    CHECK_FALSE(with_base.tile_is_water(9, 3)); // 4 + 16 = 20: blocked
    const sim::WorldTileSource zero_base(islands, *types, 32, 32);
    CHECK(zero_base.tile_is_water(3, 3));       // 19
    CHECK_FALSE(zero_base.tile_is_water(5, 3)); // 20
    CHECK(zero_base.tile_is_water(7, 3));       // 3
    CHECK(zero_base.tile_is_water(9, 3));       // 4
    CHECK(zero_base.tile_is_water(11, 3));      // unknown type
    sim::RouteTerrainOptions off;
    off.objects_block = false;
    const sim::WorldTileSource ignore(islands, *types, 32, 32, off);
    CHECK(ignore.tile_is_water(5, 3));
}

TEST_CASE(
    "route world: the builder's block shortcut and its quirk (an object across a block edge)") {
    // One aligned island 64 x 64 at (0,0); all vertices 0 (open water). An object at tiles (7,4)
    // and (8,4): the ANCHOR tile (7,4) lies in cell (0,0), the second tile (8,4) in cell (1,0).
    // Height 20.
    const auto types = object_registry({{9002, 0.8f}});
    sim::Island is = make_island(0, 0, 64, 64, 0);
    put_object(is, 7, 4, 9002, true);
    put_object(is, 8, 4, 9002, false);
    const std::vector<const sim::Island*> islands = {&is};
    const sim::WorldTileSource shortcut(islands, *types, 64, 64);
    CHECK(shortcut.cell_uses_shortcut(0, 0));
    CHECK_FALSE(shortcut.tile_is_water(7, 4));
    CHECK_FALSE(shortcut.tile_is_water(8, 4));
    // The anchor's top (20) raises cell (0,0)'s max: Mixed. Cell (1,0) has no anchor: its block
    // record says all water although the tile (8,4) is blocked - the original's quirk, kept.
    CHECK(shortcut.classify_cell(0, 0) == sim::CellClass::Mixed);
    CHECK(shortcut.classify_cell(1, 0) == sim::CellClass::AllWater);
    CHECK(shortcut.classify_cell(2, 2) == sim::CellClass::AllWater);
    // Scanning tiles sees both.
    sim::RouteTerrainOptions scan;
    scan.block_shortcut = false;
    const sim::WorldTileSource tiles(islands, *types, 64, 64, scan);
    CHECK(tiles.classify_cell(0, 0) == sim::CellClass::Mixed);
    CHECK(tiles.classify_cell(1, 0) == sim::CellClass::Mixed);
    // The coast index built on the shortcut calls cell (1,0) open (0xfff); the tile-scanning one
    // packs it.
    const sim::CoastIndex quirk(shortcut, {});
    const sim::CoastIndex exact(tiles, {});
    CHECK(quirk.mask(1, 0) == 0xfff);
    CHECK(exact.mask(1, 0) == 0xfff); // one inner blocked tile keeps every edge connected
    // Terrain: a hill block (all vertices >= 20) is AllLand, a block with one vertex 19 is Mixed.
    sim::Island hill = make_island(0, 0, 64, 64, 0);
    for (std::uint32_t y = 8; y < 16; ++y) {
        for (std::uint32_t x = 16; x < 24; ++x) {
            set_height(hill, x, y, 20);
        }
    }
    // The block's 9 x 9 vertex window also covers column 24 and row 16 (zero): not AllLand.
    const std::vector<const sim::Island*> hills = {&hill};
    const sim::WorldTileSource hs(hills, *types, 64, 64);
    CHECK(hs.classify_cell(2, 1) == sim::CellClass::Mixed);
    for (std::uint32_t y = 8; y <= 16; ++y) {
        for (std::uint32_t x = 16; x <= 24; ++x) {
            if (x < 64 && y < 64) {
                set_height(hill, x, y, 20);
            }
        }
    }
    const sim::WorldTileSource hs2(hills, *types, 64, 64);
    CHECK(hs2.classify_cell(2, 1) == sim::CellClass::AllLand);
    CHECK(hs2.classify_cell(0, 0) == sim::CellClass::AllWater);
    set_height(hill, 20, 12, 19);
    const sim::WorldTileSource hs3(hills, *types, 64, 64);
    CHECK(hs3.classify_cell(2, 1) == sim::CellClass::Mixed);
}

TEST_CASE("route world: a coast index over an island equals a clean rebuild and routes around it") {
    const auto types = object_registry({{9002, 0.8f}});
    sim::Island is = make_island(16, 16, 32, 32, 0);
    for (std::uint32_t y = 4; y < 28; ++y) {
        for (std::uint32_t x = 12; x < 20; ++x) {
            set_height(is, x, y, 30);
        }
    }
    const std::vector<const sim::Island*> islands = {&is};
    sim::RouteTerrainOptions scan;
    scan.block_shortcut = false;
    const sim::WorldTileSource src(islands, *types, 96, 64, scan);
    const std::vector<sim::CellCoord> band = {{4, 3}};
    const sim::CoastIndex index(src, band);
    CHECK(index.verify_against_clean_rebuild(band).empty());
    // The same terrain as a plain tile map gives the identical coarse words.
    sim::TileMapSource copy(96, 64);
    for (std::int32_t z = 0; z < 64; ++z) {
        for (std::int32_t x = 0; x < 96; ++x) {
            copy.set_water(x, z, src.tile_is_water(x, z));
        }
    }
    CHECK(index.same_words(sim::CoastIndex(copy, band)));
    sim::RouteQuery q;
    q.start_x = 4;
    q.start_z = 30;
    q.goal_x_q8 = 90 * 256 + 128;
    q.goal_z_q8 = 30 * 256 + 128;
    const auto res = sim::plan_route(index, q, sim::RouteLimits{}, sim::RouteCostTable::original());
    REQUIRE(res.status == sim::RouteStatus::Planned);
    const Walk w = walk_runs(src, 4, 30, res.runs, 10);
    CHECK(w.problem.empty());
    CHECK(w.end_x == 90);
    // The same query on the tile-map copy is identical to the run.
    const auto res2 = sim::plan_route(sim::CoastIndex(copy, band), q, sim::RouteLimits{},
                                      sim::RouteCostTable::original());
    CHECK(runs_to_string(res2.runs) == runs_to_string(res.runs));
    // Incremental update after a terrain change inside the island: the edited island is a copy (the
    // terrain is immutable and shared), a new source is bound to the index, then the rectangle is
    // rebuilt.
    sim::Island edited = is;
    for (std::uint32_t y = 0; y < 9; ++y) {
        for (std::uint32_t x = 0; x < 9; ++x) {
            set_height(edited, x, y, 30);
        }
    }
    const std::vector<const sim::Island*> edited_islands = {&edited};
    const sim::WorldTileSource edited_src(edited_islands, *types, 96, 64, scan);
    sim::CoastIndex moved = index;
    moved.rebind(edited_src);
    CHECK_FALSE(
        moved.verify_against_clean_rebuild(band).empty()); // stale until the rectangle is rebuilt
    moved.rebuild_rect({16, 16, 25, 25}, band);
    CHECK(moved.verify_against_clean_rebuild(band).empty());
    CHECK(moved.same_words(sim::CoastIndex(edited_src, band)));
    CHECK(index.verify_against_clean_rebuild(band)
              .empty()); // the original index still matches its source
}

// ---------------------------------------------------------------------------------------------
// the real shipped islands
// ---------------------------------------------------------------------------------------------

namespace {

std::vector<std::uint8_t> read_all(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

struct Witness {
    std::string candidate;
    int seat = 0, island = 0, tool_cost = 0;
    int spawn_x = 0, spawn_z = 0, site_x = 0, site_z = 0, water_x = 0, water_z = 0;
};

std::vector<Witness> load_witnesses() {
    std::vector<Witness> out;
    for (const std::string& line : fixture_lines("candidate_witnesses.txt")) {
        const auto p = split(line, '|');
        Witness w;
        w.candidate = words(p[0])[1];
        w.seat = std::stoi(words(p[1])[1]);
        w.island = std::stoi(words(p[2])[1]);
        w.tool_cost = std::stoi(words(p[3])[1]);
        const auto sp = words(p[4]);
        const auto si = words(p[5]);
        const auto wa = words(p[6]);
        w.spawn_x = std::stoi(sp[1]);
        w.spawn_z = std::stoi(sp[2]);
        w.site_x = std::stoi(si[1]);
        w.site_z = std::stoi(si[2]);
        w.water_x = std::stoi(wa[1]);
        w.water_z = std::stoi(wa[2]);
        out.push_back(w);
    }
    return out;
}

std::uint64_t total_steps(const std::vector<sim::RouteRun>& runs) {
    std::uint64_t n = 0;
    for (const auto& r : runs) {
        n += r.steps;
    }
    return n;
}

void emit(const std::string& line) {
    const char* out = std::getenv("ANNO_ROUTE_REAL_OUT");
    if (out != nullptr && *out != '\0') {
        std::ofstream f(out, std::ios::app);
        f << line << '\n';
    }
}

} // namespace

TEST_CASE(
    "route on the real islands: coast index, seat-to-seat routes, the 15 saved witness routes" *
    doctest::test_suite("heavy")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sl::OriginalRegistryLoad loaded_types =
        sl::load_original_registry(*dir, sl::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded_types.error.empty(), loaded_types.error);
    const auto types = loaded_types.types;
    const auto names = sl::load_city_names(*dir);
    REQUIRE_MESSAGE(names.ok(), (names.ok() ? std::string() : names.error()));
    sim::Rules rules;
    rules.city_names = names.value();
    const std::vector<Witness> witnesses = load_witnesses();
    REQUIRE(witnesses.size() == 15);

    std::uint64_t max_coarse = 0, max_fine = 0, max_bytes = 0;
    std::uint32_t seat_routes = 0, witness_found = 0;
    for (const char cand : {'a', 'b', 'c'}) {
        const fs::path manifest_path = fs::path(ANNO_REPO_DIR) / "data" / "maps" / "candidates" /
                                       "ship-start-v2" /
                                       (std::string("lan5-ship-") + cand + ".json");
        const auto parsed = sm::parse(read_all(manifest_path));
        REQUIRE(parsed.ok());
        auto loaded = sl::load_ship_start(parsed.value(), *dir, types, {}, rules);
        REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
        const sim::World w = std::move(loaded).value();
        INFO("candidate " << cand);

        const auto t0 = std::chrono::steady_clock::now();
        const sim::WorldTileSource src(w);
        const sim::CoastIndex index(src, {});
        const auto t1 = std::chrono::steady_clock::now();
        CHECK(index.verify_against_clean_rebuild({}).empty());
        MESSAGE(
            "candidate " << cand << ": world " << src.width() << " x " << src.height() << ", "
                         << index.cells_x() << " x " << index.cells_z() << " cells, "
                         << index.allocated_sectors() << " real sectors, index build "
                         << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                         << " ms");
        CHECK(index.allocated_sectors() > 0);

        // A cell-for-cell comparison with the tile-scanning classification (no block shortcut): the
        // differences are cells where the shortcut and the tiles disagree about all-water /
        // all-land.
        sim::RouteTerrainOptions scan;
        scan.block_shortcut = false;
        const sim::WorldTileSource scan_src(w, scan);
        sim::RouteTerrainOptions terrain_only;
        terrain_only.objects_block = false;
        const sim::WorldTileSource terrain_src(w, terrain_only);
        const sim::CoastIndex scan_index(scan_src, {});
        std::uint64_t differing = 0, shortcut_cells = 0, differing_words = 0;
        for (std::int32_t cz = 0; cz < index.cells_z(); ++cz) {
            for (std::int32_t cx = 0; cx < index.cells_x(); ++cx) {
                shortcut_cells += src.cell_uses_shortcut(cx, cz) ? 1 : 0;
                differing += src.classify_cell(cx, cz) == scan_src.classify_cell(cx, cz) ? 0 : 1;
                differing_words += index.mask(cx, cz) == scan_index.mask(cx, cz) ? 0 : 1;
            }
        }
        MESSAGE("candidate " << cand << ": " << shortcut_cells << " cells from the block shortcut, "
                             << differing << " classified differently from their tiles, "
                             << differing_words << " with a different coarse mask");
        emit(std::string("index\t") + cand + "\t" + std::to_string(index.allocated_sectors()) +
             "\t" + std::to_string(shortcut_cells) + "\t" + std::to_string(differing) + "\t" +
             std::to_string(differing_words));

        // Seat-to-seat routes between the five ships (all in exterior sea).
        std::vector<std::pair<std::int32_t, std::int32_t>> ships;
        for (std::uint32_t seat = 0; seat < 5; ++seat) {
            const sim::Figure* f = sim::find_figure(w.figures(), sim::FigureId{3 * seat + 1});
            REQUIRE(f != nullptr);
            ships.push_back({f->pos.x, f->pos.y});
            CHECK(src.tile_is_water(f->pos.x >> 8, f->pos.y >> 8));
        }
        for (std::size_t i = 0; i < ships.size(); ++i) {
            for (std::size_t j = i + 1; j < ships.size(); ++j) {
                sim::RouteQuery q;
                q.start_x = ships[i].first >> 8;
                q.start_z = ships[i].second >> 8;
                q.goal_x_q8 = ships[j].first;
                q.goal_z_q8 = ships[j].second;
                const auto t2 = std::chrono::steady_clock::now();
                const auto res =
                    sim::plan_route(index, q, sim::RouteLimits{}, sim::RouteCostTable::original());
                const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - t2)
                                    .count();
                CAPTURE(i);
                CAPTURE(j);
                REQUIRE(res.status == sim::RouteStatus::Planned);
                const Walk walk = walk_runs(src, q.start_x, q.start_z, res.runs, 10);
                CHECK(walk.problem.empty());
                CHECK(walk.end_x == res.end_x);
                CHECK(walk.end_z == res.end_z);
                CHECK(res.score > 0); // the end tile is within the goal radius
                max_coarse = std::max(max_coarse, res.work.coarse_pops);
                max_fine = std::max(max_fine, res.work.fine_pops);
                max_bytes = std::max(max_bytes, res.work.bytes_high);
                ++seat_routes;
                emit(std::string("seat\t") + cand + "\t" + std::to_string(i) + "\t" +
                     std::to_string(j) + "\t" + std::to_string(res.work.coarse_pops) + "\t" +
                     std::to_string(res.work.fine_pops) + "\t" +
                     std::to_string(res.work.bytes_high) + "\t" + std::to_string(res.work.learned) +
                     "\t" + std::to_string(total_steps(res.runs)) + "\t" + std::to_string(ms));
            }
        }

        // The saved geometric witness routes of this candidate, re-evaluated under the original
        // rules.
        for (const Witness& wit : witnesses) {
            if (wit.candidate != std::string(1, cand)) {
                continue;
            }
            sim::RouteQuery q;
            q.start_x = wit.spawn_x;
            q.start_z = wit.spawn_z;
            q.goal_x_q8 = wit.water_x * 256 + 128;
            q.goal_z_q8 = wit.water_z * 256 + 128;
            const auto t2 = std::chrono::steady_clock::now();
            const auto res =
                sim::plan_route(index, q, sim::RouteLimits{}, sim::RouteCostTable::original());
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - t2)
                                .count();
            CAPTURE(wit.seat);
            CHECK(src.tile_is_water(wit.spawn_x, wit.spawn_z));
            const std::string goal_state =
                src.tile_is_water(wit.water_x, wit.water_z)
                    ? "water"
                    : (terrain_src.tile_is_water(wit.water_x, wit.water_z) ? "object" : "terrain");
            const Walk walk = walk_runs(src, q.start_x, q.start_z, res.runs, 10);
            CHECK(walk.problem.empty());
            if (res.status == sim::RouteStatus::Planned ||
                res.status == sim::RouteStatus::Arrived) {
                ++witness_found;
                CHECK(walk.end_x == res.end_x);
                CHECK(walk.end_z == res.end_z);
            }
            max_coarse = std::max(max_coarse, res.work.coarse_pops);
            max_fine = std::max(max_fine, res.work.fine_pops);
            max_bytes = std::max(max_bytes, res.work.bytes_high);
            const std::int64_t dx = static_cast<std::int64_t>(res.end_x) - wit.water_x;
            const std::int64_t dz = static_cast<std::int64_t>(res.end_z) - wit.water_z;
            emit(std::string("witness\t") + cand + "\t" + std::to_string(wit.seat) + "\t" +
                 std::to_string(wit.island) + "\t" + std::to_string(wit.tool_cost) + "\t" +
                 std::to_string(static_cast<int>(res.status)) + "\t" +
                 std::to_string(total_steps(res.runs)) + "\t" + std::to_string(res.runs.size()) +
                 "\t" + std::to_string(res.end_x) + "\t" + std::to_string(res.end_z) + "\t" +
                 std::to_string(wit.water_x) + "\t" + std::to_string(wit.water_z) + "\t" +
                 std::to_string(dx * dx + dz * dz) + "\t" + goal_state + "\t" +
                 std::to_string(res.work.coarse_pops) + "\t" + std::to_string(res.work.fine_pops) +
                 "\t" + std::to_string(res.work.bytes_high) + "\t" +
                 std::to_string(res.work.learned) + "\t" + std::to_string(ms));
        }
    }
    CHECK(seat_routes == 30);
    CHECK(witness_found == 15);
    // The default limits carry at least 8x the largest legal query measured here.
    const sim::RouteLimits defaults;
    CHECK(defaults.coarse_budget >= 8 * max_coarse);
    CHECK(defaults.fine_budget >= 8 * max_fine);
    CHECK(defaults.memory_bytes >= 8 * max_bytes);
    MESSAGE("largest legal query on the real candidates: " << max_coarse << " coarse pops, "
                                                           << max_fine << " fine pops, "
                                                           << max_bytes << " bytes");
    MESSAGE("witness routes found under the original rules: " << witness_found << " of 15");
}

TEST_CASE("route world: open-sea cells are answered without scanning islands, partial cells scan "
          "only the islands touching them") {
    const auto types = object_registry({});
    // 40 islands of 16 x 16 tiles on a 256-tile row, plus one unaligned 5 x 5 island at (101, 3)
    // that covers only part of its 8 x 8 cells.
    std::vector<sim::Island> store;
    for (int i = 0; i < 40; ++i) {
        store.push_back(make_island(i * 16, 64, 16, 16, 0));
    }
    store.push_back(make_island(101, 3, 5, 5, 0));
    std::vector<const sim::Island*> islands;
    for (const auto& is : store) {
        islands.push_back(&is);
    }
    const sim::WorldTileSource src(islands, *types, 1024, 256);
    // Open sea far from every rectangle: no island rectangle is examined at all.
    CHECK(src.tile_is_water(500, 200));
    CHECK(src.tile_is_water(3, 3));
    CHECK(src.entries_examined() == 0);
    // A tile on a whole-covered cell is a direct hit.
    CHECK(src.tile_is_water(10, 70));
    CHECK(src.entries_examined() == 0);
    // A tile in the partly covered cell: only the one island touching it is examined (not 41), and
    // the answer is what a full scan gives (island tile -> terrain rules; neighbour -> open sea).
    CHECK(src.tile_is_water(102, 4));
    CHECK(src.entries_examined() == 1);
    CHECK(src.tile_is_water(107, 4)); // same cell, outside the island: still examined once more
    CHECK(src.entries_examined() == 2);
}
