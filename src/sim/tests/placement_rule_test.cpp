// The BuildCmd placement rule (docs/sim/ship-placement.md R1-R6) against its independent oracle:
// every case of fixtures/synthetic/sim/placement_vectors.txt (written by hand from the contract and
// reproduced by tools/placement_ref.py and by the original code's emulation) is parsed here and
// run through anno::sim::placement_rule. The expected accept/reject/byte come from the file, never
// from this code. The file also checks the full overlap table (site::overlap_with_existing) against
// hand-derived rows of docs/re/ship-placement.md section 4, and the named stand-in seam (its own
// verdict, the terrain's height) against a hand-derived row per vector.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/placement.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/types.hpp>

namespace sim = anno::sim;

namespace {

struct VectorCase {
    std::string name;
    std::int32_t x0 = 0, y0 = 0;
    std::uint32_t w = 0, h = 0;
    std::map<std::uint32_t, std::vector<std::uint8_t>> rows; // vertex rows 0..H, W+1 values each
    std::map<std::pair<std::int32_t, std::int32_t>, std::uint32_t> objs;
    std::uint32_t type = 0;
    std::int32_t x = 0, y = 0;
    std::uint32_t rot = 0;
    std::vector<std::string> expect;
};

std::vector<std::uint8_t> hex_bytes(const std::string& s) {
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i + 1 < s.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
    }
    return out;
}

struct Vectors {
    std::vector<sim::BuildingType> types;
    std::vector<VectorCase> cases;
};

Vectors read_vectors() {
    Vectors v;
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "placement_vectors.txt");
    REQUIRE_MESSAGE(in.good(), "placement_vectors.txt not found");
    std::string line;
    while (std::getline(in, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line.erase(hash);
        }
        std::istringstream ss(line);
        std::string k;
        if (!(ss >> k)) {
            continue;
        }
        if (k == "type") {
            std::uint32_t id = 0, w = 0, h = 0, prod = 0, repl = 0;
            std::string flags;
            ss >> id >> w >> h >> prod >> flags >> repl;
            sim::BuildingType t;
            t.id = sim::TypeId{id};
            t.width = w;
            t.height = h;
            t.prodtype = prod;
            t.flags_word = static_cast<std::uint32_t>(std::stoul(flags, nullptr, 16));
            if (repl != 0) {
                t.head_raw.assign(56, 0);
                t.head_raw[0x2e] = 1; // the replaceable bit the loader reads (HEAD[0x2e] bit 0)
            }
            v.types.push_back(std::move(t));
        } else if (k == "case") {
            v.cases.emplace_back();
            ss >> v.cases.back().name;
        } else if (k == "island") {
            VectorCase& c = v.cases.back();
            ss >> c.x0 >> c.y0 >> c.w >> c.h;
        } else if (k == "v") {
            std::uint32_t row = 0;
            std::string hex;
            ss >> row >> hex;
            v.cases.back().rows[row] = hex_bytes(hex);
        } else if (k == "obj") {
            std::int32_t x = 0, y = 0;
            std::uint32_t id = 0;
            ss >> x >> y >> id;
            v.cases.back().objs[{x, y}] = id;
        } else if (k == "place") {
            VectorCase& c = v.cases.back();
            std::string flags;
            std::uint32_t rec8 = 0;
            ss >> c.type >> c.x >> c.y >> c.rot >> flags >> rec8; // flags and rec8: not inputs here
        } else if (k == "expect") {
            std::string word;
            while (ss >> word) {
                v.cases.back().expect.push_back(word);
            }
        }
    }
    return v;
}

/// The runtime vertex grid of the case as an Island: Terrain::height is W x H bytes (column W and
/// row H do not exist there and read 0, as in the original grid).
sim::Island island_of(const VectorCase& c) {
    sim::Island is;
    REQUIRE(is.tiles.reset(c.x0, c.y0, c.w, c.h, 8));
    std::vector<std::uint8_t> height(std::size_t{c.w} * c.h, 0);
    for (const auto& [row, bytes] : c.rows) {
        if (row >= c.h) {
            continue; // row H is the zero row
        }
        for (std::uint32_t x = 0; x < c.w && x < bytes.size(); ++x) {
            height[std::size_t{row} * c.w + x] = bytes[x];
        }
    }
    is.terrain = sim::make_terrain(std::move(height), {}, {});
    for (const auto& [xy, id] : c.objs) {
        const sim::Slot idx = is.tiles.index_of(xy.first, xy.second);
        REQUIRE(idx != sim::kNoSlot);
        is.tiles.set_object(idx, id);
    }
    return is;
}

sim::Reject reject_for(const std::string& reason) {
    if (reason == "BOUNDS") {
        return sim::Reject::OutOfIsland;
    }
    if (reason == "OCCUPIED") {
        return sim::Reject::Occupied;
    }
    if (reason == "TERRAIN" || reason == "LOWLEVEL") {
        return sim::Reject::TerrainRefused;
    }
    FAIL("unknown reason " << reason);
    return sim::Reject::None;
}

} // namespace

TEST_CASE("placement rule: every vector of the oracle file") {
    const Vectors v = read_vectors();
    REQUIRE(v.cases.size() == 50);
    sim::TypeRegistry::Builder b;
    for (const sim::BuildingType& t : v.types) {
        b.add(t);
    }
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    const sim::TypeRegistry& reg = *built.value();
    std::size_t accepts = 0, rejects = 0, unsupported = 0;
    for (const VectorCase& c : v.cases) {
        INFO("vector " << c.name);
        REQUIRE(!c.expect.empty());
        const sim::BuildingType* t = reg.find(sim::TypeId{c.type});
        REQUIRE(t != nullptr);
        const sim::Island is = island_of(c);
        sim::PlacementRecord rec;
        rec.type = t->id;
        rec.x = c.x;
        rec.y = c.y;
        rec.rotation = static_cast<std::uint8_t>(c.rot);
        // byte0f is a result, never an input: pass the hostile value the oracle's flag column
        // names.
        rec.byte0f = 0x40;
        const sim::PlacementResult r = sim::placement_rule(is, reg, *t, rec);
        if (c.expect[0] == "accept") {
            ++accepts;
            CHECK(r.reject == sim::Reject::None);
            CHECK(static_cast<unsigned>(r.height) == std::stoul(c.expect.at(1)));
        } else if (c.expect[0] == "reject") {
            ++rejects;
            CHECK(r.reject == reject_for(c.expect.at(1)));
            CHECK(r.height == 0);
        } else {
            REQUIRE(c.expect[0] == "unsupported");
            ++unsupported;
            CHECK(r.reject == sim::Reject::Unsupported);
            CHECK(r.height == 0);
        }
    }
    // The file's mix, so a parser that silently drops cases cannot pass.
    CHECK(accepts + rejects + unsupported == 50);
    CHECK(accepts >= 15);
    CHECK(rejects >= 15);
    CHECK(unsupported >= 4);
}

TEST_CASE("stand-in seam: its own verdict and the terrain's height on every vector") {
    // The stand-in (docs/sim/ship-placement.md section 6, "The stand-in's height"): Occupied when
    // an object lies under the footprint (clipped to the island); else accepted, with
    // placement_rule's byte where that rule accepts, else with the highest readable vertex of the
    // footprint's window (column W and row H read 0, vertices beyond them are skipped). Every row
    // below was derived by hand from the grids of placement_vectors.txt; the old stand-in stored 0
    // for all of them.
    const struct {
        const char* name;
        bool occupied;
        unsigned height;
    } rows[] = {
        // The file's accepts: placement_rule's byte.
        {"land_flat_30", false, 48},
        {"level_20_is_the_threshold", false, 32},
        {"level_40", false, 64},
        {"wire_byte_is_ignored_high", false, 48},
        {"slope_just_outside_window", false, 48},
        {"one_tile_from_east_edge", false, 48},
        {"outside_north_west_origin_ok", false, 48},
        {"wide_footprint_fits", false, 48},
        {"wide_footprint_turned_fits", false, 48},
        {"same_vertex_outside_rot1_window", false, 48},
        {"object_outside_footprint_ignored", false, 48},
        {"alternate_type_farm", false, 48},
        {"alternate_flag_word_0004", false, 32},
        {"coast_valid_rot0", false, 48},
        {"coast_rot2_shore_is_bottom_row", false, 48},
        {"coast_rot1_shore_is_left_column", false, 48},
        {"coast_two_water_vertices_enough", false, 48},
        {"coast_narrow_type_skips_the_shore_test", false, 16},
        {"coast_alternate_type_2x2_one_water_vertex", false, 48},
        {"u16_edge_accept", false, 48},
        // The verdict stays the stand-in's: any object under the footprint refuses, including
        // the scenery and the replaceable building placement_rule accepts.
        {"scenery_tree_under_footprint", true, 0},
        {"scenery_stone_under_footprint", true, 0},
        {"replaceable_same_prodtype_accepts", true, 0},
        {"building_under_footprint", true, 0},
        {"unknown_object_id_unsupported", true, 0},
        // Flat windows placement_rule refuses (level, quantum): the flat level itself.
        {"sea_level_10_same_footprint", false, 16},
        {"wire_byte_is_ignored_low", false, 16},
        {"sea_level_00", false, 0},
        {"level_1f_not_multiple", false, 31},
        {"level_2f_not_multiple", false, 47},
        {"level_35_not_multiple", false, 53},
        // Uneven windows: the highest vertex inside the window.
        {"dip_inside", false, 48},                         // 0x2f at (4,3), the rest 0x30
        {"slope_one_unit_inside", false, 49},              // 0x31 at (4,4)
        {"slope_step_16_inside", false, 64},               // 0x40 at (5,5)
        {"spoiled_vertex_in_rot0_window_only", false, 80}, // 0x50 at (9,2), window 5..9 x 2..5
        {"east_edge_window_reads_zero_column", false, 48}, // column 10 reads 0
        {"south_edge_window_reads_zero_row", false, 48},   // row 10 reads 0
        // Coast windows the shore test refuses: the highest vertex.
        {"coast_one_water_vertex_only", false, 48},
        {"coast_rot0_but_shore_at_bottom", false, 48},
        {"coast_shore_vertex_wrong", false, 48},
        {"coast_window_max_4x_rejected", false, 64}, // 0x40 at (5,5)
        {"same_flat_land_for_the_coast_type", false, 48},
        {"coast_alternate_type_2x2_no_water", false, 48},
        // Classes placement_rule does not decide: the window over flat 0x30 land.
        {"class_3000_unsupported", false, 48},
        {"flag_0x10_type_unsupported", false, 48},
        {"road_type_not_a_build_class", false, 48},
        {"wall_type_not_a_build_class", false, 48},
        // Out of bounds (footprint_owner_rule refuses these before any seam in a command): the
        // stand-in clips; the window's vertices beyond column W are skipped.
        {"outside_east", false, 48},
        {"wide_footprint_pokes_out", false, 48},
        {"u16_edge_bounds", false, 48},
    };
    const Vectors v = read_vectors();
    REQUIRE(v.cases.size() == std::size(rows));
    sim::TypeRegistry::Builder b;
    for (const sim::BuildingType& t : v.types) {
        b.add(t);
    }
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    const sim::TypeRegistry& reg = *built.value();
    std::map<std::string, const VectorCase*> by_name;
    for (const VectorCase& c : v.cases) {
        by_name.emplace(c.name, &c);
    }
    const sim::Seam<sim::Seams::PlacementRuleFn> seam = sim::stand_in_placement_seam();
    CHECK(std::string(seam.name) == "stand_in_placement_terrain_height");
    std::size_t equal_to_rule = 0;
    for (const auto& row : rows) {
        INFO("vector " << row.name);
        REQUIRE(by_name.count(row.name) == 1);
        const VectorCase& c = *by_name.at(row.name);
        const sim::BuildingType* t = reg.find(sim::TypeId{c.type});
        REQUIRE(t != nullptr);
        const sim::Island is = island_of(c);
        sim::PlacementRecord rec;
        rec.type = t->id;
        rec.x = c.x;
        rec.y = c.y;
        rec.rotation = static_cast<std::uint8_t>(c.rot);
        rec.byte0f = 0x40; // a result, never an input
        const sim::PlacementResult r = seam.fn(seam.ctx, is, reg, *t, rec);
        CHECK(r.reject == (row.occupied ? sim::Reject::Occupied : sim::Reject::None));
        CHECK(static_cast<unsigned>(r.height) == row.height);
        const sim::PlacementResult real = sim::placement_rule(is, reg, *t, rec);
        if (r.reject == sim::Reject::None && real.reject == sim::Reject::None) {
            ++equal_to_rule;
            CHECK(r.height == real.height);
            CHECK(c.expect.at(0) == "accept");
            CHECK(static_cast<unsigned>(r.height) == std::stoul(c.expect.at(1)));
        }
    }
    CHECK(equal_to_rule == 20);
}

TEST_CASE("stand-in seam: a world without terrain gets height 0, a terrain gets its height") {
    sim::TypeRegistry::Builder b;
    sim::BuildingType t;
    t.id = sim::TypeId{1};
    t.prodtype = 97;
    t.width = t.height = 2;
    t.flags_word = 0x84;
    b.add(t);
    auto built = b.build();
    REQUIRE(built.ok());
    const sim::TypeRegistry& reg = *built.value();
    const sim::BuildingType& ty = *reg.find(sim::TypeId{1});
    sim::PlacementRecord rec;
    rec.type = ty.id;
    rec.x = 2;
    rec.y = 2;
    sim::Island is;
    REQUIRE(is.tiles.reset(0, 0, 8, 8, 8));
    // No terrain: the stand-in still accepts (it never read the terrain) and nothing gives a
    // height; placement_rule answers Unsupported here.
    const sim::PlacementResult none = sim::stand_in_placement_seam_fn(nullptr, is, reg, ty, rec);
    CHECK(none.reject == sim::Reject::None);
    CHECK(none.height == 0);
    // A digest-only placeholder reads nothing either.
    auto ph = std::make_shared<sim::Terrain>();
    ph->digest = 12345;
    is.terrain = ph;
    CHECK(sim::stand_in_placement_seam_fn(nullptr, is, reg, ty, rec) ==
          sim::PlacementResult{sim::Reject::None, 0});
    // Flat 0x30 under the 3 x 3 window (2..4, 2..4) and 0x70 at (5,5) just outside it: 0x30, the
    // byte placement_rule derives.
    std::vector<std::uint8_t> height(64, 0x30);
    height[5 * 8 + 5] = 0x70;
    is.terrain = sim::make_terrain(height, {}, {});
    CHECK(sim::placement_rule(is, reg, ty, rec) == sim::PlacementResult{sim::Reject::None, 0x30});
    CHECK(sim::stand_in_placement_seam_fn(nullptr, is, reg, ty, rec) ==
          sim::PlacementResult{sim::Reject::None, 0x30});
    // The same vertex inside the window (4,4): placement_rule refuses, the stand-in accepts at
    // 0x70.
    height[5 * 8 + 5] = 0x30;
    height[4 * 8 + 4] = 0x70;
    is.terrain = sim::make_terrain(height, {}, {});
    CHECK(sim::placement_rule(is, reg, ty, rec).reject == sim::Reject::TerrainRefused);
    CHECK(sim::stand_in_placement_seam_fn(nullptr, is, reg, ty, rec) ==
          sim::PlacementResult{sim::Reject::None, 0x70});
    CHECK(sim::stand_in_placement_height(is, reg, ty, rec) == 0x70);
}

TEST_CASE("placement rule: no terrain, placeholder terrain and a wrong column are Unsupported") {
    sim::TypeRegistry::Builder b;
    sim::BuildingType t;
    t.id = sim::TypeId{1};
    t.prodtype = 97;
    t.width = t.height = 2;
    t.flags_word = 0x84;
    b.add(t);
    auto built = b.build();
    REQUIRE(built.ok());
    const sim::TypeRegistry& reg = *built.value();
    const sim::BuildingType& ty = *reg.find(sim::TypeId{1});
    sim::PlacementRecord rec;
    rec.type = ty.id;
    rec.x = 2;
    rec.y = 2;
    sim::Island is;
    REQUIRE(is.tiles.reset(0, 0, 8, 8, 8));
    // No terrain at all.
    CHECK(sim::placement_rule(is, reg, ty, rec).reject == sim::Reject::Unsupported);
    // A terrain of the wrong size.
    is.terrain = sim::make_terrain(std::vector<std::uint8_t>(10, 0x30), {}, {});
    CHECK(sim::placement_rule(is, reg, ty, rec).reject == sim::Reject::Unsupported);
    // Digest-only placeholder (what a world read from the flat stream holds).
    auto ph = std::make_shared<sim::Terrain>();
    ph->digest = 12345;
    is.terrain = ph;
    CHECK(sim::placement_rule(is, reg, ty, rec).reject == sim::Reject::Unsupported);
    // The control: the right column.
    is.terrain = sim::make_terrain(std::vector<std::uint8_t>(64, 0x30), {}, {});
    const sim::PlacementResult ok = sim::placement_rule(is, reg, ty, rec);
    CHECK(ok.reject == sim::Reject::None);
    CHECK(ok.height == 0x30);
}

TEST_CASE("placement rule: the PRODTYPE group 0x141 is exempt from the level threshold") {
    // Oracle place(): `rec8 < 32` is not tested for group 0x141 (0x1007cbe4); the window must
    // still be one multiple of 16. A data-defined type, no engine edit. PRODTYPE 321 is group
    // 0x141.
    sim::TypeRegistry::Builder b;
    sim::BuildingType exempt;
    exempt.id = sim::TypeId{1};
    exempt.prodtype = 321;
    exempt.width = exempt.height = 2;
    exempt.flags_word = 0x84;
    b.add(exempt);
    sim::BuildingType ordinary = exempt;
    ordinary.id = sim::TypeId{2};
    ordinary.prodtype = 97;
    b.add(ordinary);
    auto built = b.build();
    REQUIRE(built.ok());
    const sim::TypeRegistry& reg = *built.value();
    sim::Island is;
    REQUIRE(is.tiles.reset(0, 0, 8, 8, 8));
    std::vector<std::uint8_t> height(64, 0x10);
    is.terrain = sim::make_terrain(height, {}, {});
    sim::PlacementRecord rec;
    rec.x = 2;
    rec.y = 2;
    rec.type = sim::TypeId{1};
    const sim::PlacementResult low = sim::placement_rule(is, reg, *reg.find(rec.type), rec);
    CHECK(low.reject == sim::Reject::None);
    CHECK(low.height == 0x10);
    rec.type = sim::TypeId{2};
    CHECK(sim::placement_rule(is, reg, *reg.find(rec.type), rec).reject ==
          sim::Reject::TerrainRefused);
    // Level 0: the exempt type is accepted with height 0 (the byte is a result, not a refusal).
    is.terrain = sim::make_terrain(std::vector<std::uint8_t>(64, 0), {}, {});
    rec.type = sim::TypeId{1};
    const sim::PlacementResult zero = sim::placement_rule(is, reg, *reg.find(rec.type), rec);
    CHECK(zero.reject == sim::Reject::None);
    CHECK(zero.height == 0);
    // An unequal window still refuses.
    height[2 * 8 + 3] = 0x20;
    is.terrain = sim::make_terrain(height, {}, {});
    CHECK(sim::placement_rule(is, reg, *reg.find(rec.type), rec).reject ==
          sim::Reject::TerrainRefused);
}

TEST_CASE("placement rule: the ordinary class predicate is flag word minus bit 0x80 equals 4") {
    sim::Island is;
    REQUIRE(is.tiles.reset(0, 0, 8, 8, 8));
    is.terrain = sim::make_terrain(std::vector<std::uint8_t>(64, 0x30), {}, {});
    const struct {
        std::uint32_t flags;
        sim::Reject want;
    } rows[] = {
        {0x0084, sim::Reject::None},         {0x0004, sim::Reject::None},
        {0x0000, sim::Reject::Unsupported},  {0x0080, sim::Reject::Unsupported},
        {0x000c, sim::Reject::Unsupported},  {0x0044, sim::Reject::Unsupported},
        {0x8084, sim::Reject::Unsupported},  {0x0184, sim::Reject::Unsupported},
        {0x0094, sim::Reject::Unsupported},  {0x1084, sim::Reject::Unsupported},
        {0x0085, sim::Reject::Unsupported},  {0x0086, sim::Reject::Unsupported},
        {0x10084, sim::Reject::Unsupported},
    };
    for (const auto& row : rows) {
        INFO("flag word " << row.flags);
        sim::TypeRegistry::Builder b;
        sim::BuildingType t;
        t.id = sim::TypeId{1};
        t.prodtype = 97;
        t.width = t.height = 2;
        t.flags_word = row.flags;
        b.add(t);
        auto built = b.build();
        REQUIRE(built.ok());
        sim::PlacementRecord rec;
        rec.type = t.id;
        rec.x = 2;
        rec.y = 2;
        const sim::PlacementResult r =
            sim::placement_rule(is, *built.value(), *built.value()->find(t.id), rec);
        CHECK(r.reject == row.want);
        CHECK(r.height == (row.want == sim::Reject::None ? 0x30 : 0));
    }
}

TEST_CASE("placement rule: the overlap table rows of the original slot 0x6c") {
    const auto type_of = [](std::uint32_t prod, std::uint32_t flags, bool replaceable = false) {
        sim::BuildingType t;
        t.id = sim::TypeId{prod};
        t.prodtype = prod;
        t.flags_word = flags;
        if (replaceable) {
            t.head_raw.assign(56, 0);
            t.head_raw[0x2e] = 1;
        }
        return t;
    };
    const auto compat = [](const sim::BuildingType& p, const sim::BuildingType& e) {
        return sim::site::overlap_with_existing(p, e) == sim::site::Compat::Compatible;
    };
    // Flag 0x20 existing: always.
    CHECK(compat(type_of(500, 0), type_of(323, 0x3a)));
    // Group 1/0x21/0x61: same PRODTYPE over a replaceable type, not over a plain one.
    CHECK(compat(type_of(34, 0x84), type_of(34, 0x84, true)));
    CHECK_FALSE(compat(type_of(34, 0x84), type_of(34, 0x84, false)));
    CHECK_FALSE(compat(type_of(34, 0x84), type_of(35, 0x84, true)));
    // The groups the old table left undecided now answer 0: 0x41 (65..96) and 0xa1 (161..192).
    CHECK_FALSE(compat(type_of(70, 0x84), type_of(34, 0)));
    CHECK_FALSE(compat(type_of(161, 0x84), type_of(34, 0)));
    // Group 0x81 (129..160): PRODTYPE 129 over a group 0xc1 or 0x101 object only.
    CHECK(compat(type_of(129, 0x84), type_of(193, 0)));
    CHECK(compat(type_of(129, 0x84), type_of(257, 0)));
    CHECK_FALSE(compat(type_of(130, 0x84), type_of(193, 0)));
    CHECK_FALSE(compat(type_of(129, 0x84), type_of(97, 0)));
    // Group 0xc1: over 0xc6 only 0xc6, else over group 0xc1.
    CHECK(compat(type_of(198, 0), type_of(198, 0)));
    CHECK_FALSE(compat(type_of(193, 0), type_of(198, 0)));
    CHECK(compat(type_of(193, 0), type_of(194, 0)));
    CHECK_FALSE(compat(type_of(193, 0), type_of(257, 0)));
    // Group 0x101 over 0xc1 or 0x101; group 0x141 over 0x141 only.
    CHECK(compat(type_of(257, 0), type_of(193, 0)));
    CHECK(compat(type_of(257, 0), type_of(258, 0)));
    CHECK(compat(type_of(321, 0), type_of(330, 0)));
    CHECK_FALSE(compat(type_of(321, 0), type_of(193, 0)));
    // Placed flag 0x10 over group 0x141 (groups 1/0x21/0x61 only).
    CHECK(compat(type_of(98, 0x94), type_of(321, 0)));
    CHECK_FALSE(compat(type_of(98, 0x84), type_of(321, 0)));
}
