// The BuildCmd placement rule (docs/sim/ship-placement.md R1-R6) against its independent oracle:
// every case of fixtures/synthetic/sim/placement_vectors.txt (written by hand from the contract and
// reproduced by tools/placement_ref.py and by the original code's emulation) is parsed here and
// run through anno::sim::placement_rule. The expected accept/reject/byte come from the file, never
// from this code. The file also checks the full overlap table (site::overlap_with_existing) against
// hand-derived rows of docs/re/ship-placement.md section 4.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
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
    REQUIRE(v.cases.size() == 77);
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
    CHECK(accepts + rejects + unsupported == 77);
    CHECK(accepts >= 15);
    CHECK(rejects >= 15);
    CHECK(unsupported >= 4);
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
