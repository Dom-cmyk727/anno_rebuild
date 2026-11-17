// FERTILITY-1 stage 1a: the placement class of the single-quad tile objects (flag word & 3 == 1;
// the shipped crop objects, trees and scenery carry 0x29), docs/sim/ship-placement.md section 2a,
// docs/re/ship-placement.md section 3a. The unit cases are derived by hand from the disassembly
// (AnnoGame 0x10008fa1-0x10008fd9, AnnoGame 0x10008920, AnnoWelt 0x1007cbc2-0x1007cc06); the
// corpus case compares placement_rule over every tile of every shipped island with the digest
// lines tools/placement_ref.py tilecorpus wrote (fixtures/synthetic/sim/tile_object_corpus.txt).
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/insel.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

namespace sim = anno::sim;
namespace fs = std::filesystem;

namespace {

sim::BuildingType type_of(std::uint32_t id, std::uint32_t w, std::uint32_t h,
                          std::uint32_t prodtype, std::uint32_t flags) {
    sim::BuildingType t;
    t.id = sim::TypeId{id};
    t.width = w;
    t.height = h;
    t.prodtype = prodtype;
    t.flags_word = flags;
    return t;
}

// Type ids of the hand-made registry.
constexpr std::uint32_t kCrop = 1;       // ROHSTWACHS-like 1 x 1, 0x29 (group 0xe1)
constexpr std::uint32_t kTree3 = 2;      // ROHSTOFF 3215-like 3 x 3, 0x29
constexpr std::uint32_t kStone = 3;      // STEIN-like, 0x3a: flag 0x20, overlappable
constexpr std::uint32_t kHouse = 4;      // an ordinary building, 0x84
constexpr std::uint32_t kRoad = 5;       // a land road, 0x4186 (no 0x20)
constexpr std::uint32_t kRoadQuad = 6;   // class 0x4000 tile object (data-defined), 0x4185
constexpr std::uint32_t kCoastQuad = 7;  // class 0x2000 tile object (data-defined), 0x2029
constexpr std::uint32_t kRockQuad = 8;   // PRODTYPE 321 (group 0x141) tile object, 0x29
constexpr std::uint32_t kLong = 9;       // 3 x 1 tile object (data-defined), 0x29
constexpr std::uint32_t kCoverQuad = 10; // tile object with the coverage bit 0x10: 0x39
constexpr std::uint32_t kCrop2 = 11;     // a second 0x29 crop object, existing on the tile

const sim::TypeRegistry& registry() {
    static const std::shared_ptr<const sim::TypeRegistry> reg = [] {
        sim::TypeRegistry::Builder b;
        b.add(type_of(kCrop, 1, 1, 226, 0x29));
        b.add(type_of(kTree3, 3, 3, 225, 0x29));
        b.add(type_of(kStone, 1, 1, 323, 0x3a));
        b.add(type_of(kHouse, 2, 2, 97, 0x84));
        b.add(type_of(kRoad, 1, 1, 193, 0x4186));
        b.add(type_of(kRoadQuad, 1, 1, 226, 0x4185));
        b.add(type_of(kCoastQuad, 1, 1, 226, 0x2029));
        b.add(type_of(kRockQuad, 1, 1, 321, 0x29));
        b.add(type_of(kLong, 3, 1, 226, 0x29));
        b.add(type_of(kCoverQuad, 1, 1, 226, 0x39));
        b.add(type_of(kCrop2, 1, 1, 226, 0x29));
        auto built = b.build();
        REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
        return built.value();
    }();
    return *reg;
}

/// A 10 x 10 island at (0, 0) whose tile heights are `level` (row H and column W of the vertex
/// grid read 0 as in the original), with the given vertex overrides and objects.
sim::Island island(std::uint8_t level,
                   const std::vector<std::pair<std::pair<int, int>, std::uint8_t>>& vertices = {},
                   const std::vector<std::pair<std::pair<int, int>, std::uint32_t>>& objects = {}) {
    constexpr std::uint32_t kW = 10;
    sim::Island is;
    REQUIRE(is.tiles.reset(0, 0, kW, kW, 8));
    std::vector<std::uint8_t> height(std::size_t{kW} * kW, level);
    for (const auto& [xy, v] : vertices) {
        height.at(static_cast<std::size_t>(xy.second) * kW + static_cast<std::size_t>(xy.first)) =
            v;
    }
    is.terrain = sim::make_terrain(std::move(height), {}, {});
    for (const auto& [xy, id] : objects) {
        is.tiles.set_object(is.tiles.index_of(xy.first, xy.second), id);
    }
    return is;
}

sim::PlacementResult place(const sim::Island& is, std::uint32_t type, std::int32_t x,
                           std::int32_t y, std::uint8_t rot = 0,
                           const sim::PlacementParams& params = {}) {
    const sim::BuildingType* t = registry().find(sim::TypeId{type});
    REQUIRE(t != nullptr);
    sim::PlacementRecord rec;
    rec.type = t->id;
    rec.x = x;
    rec.y = y;
    rec.rotation = rot;
    rec.byte0f = 0xff; // a result, never an input
    return sim::placement_rule(is, registry(), *t, rec, params);
}

constexpr sim::PlacementResult accept(unsigned byte) {
    return {sim::Reject::None, static_cast<std::uint8_t>(byte)};
}
constexpr sim::PlacementResult kLow{sim::Reject::TerrainRefused, 0};
constexpr sim::PlacementResult kOccupied{sim::Reject::Occupied, 0};
constexpr sim::PlacementResult kBounds{sim::Reject::OutOfIsland, 0};
constexpr sim::PlacementResult kUnsupported{sim::Reject::Unsupported, 0};

} // namespace

TEST_CASE("tile object: the byte is the floor of the four-corner mean, accepted from 32") {
    // Corners of tile (3, 3): vertices (3,3) (4,3) (3,4) (4,4). AnnoGame 0x10008fd3: sum >> 2.
    const auto quad = [](std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
        return island(0x10, {{{3, 3}, a}, {{4, 3}, b}, {{3, 4}, c}, {{4, 4}, d}});
    };
    CHECK(place(quad(0x20, 0x20, 0x20, 0x20), kCrop, 3, 3) == accept(32)); // 128 / 4 = 32
    CHECK(place(quad(0x20, 0x20, 0x20, 0x1f), kCrop, 3, 3) == kLow);       // 127 >> 2 = 31
    CHECK(place(quad(0x20, 0x20, 0x20, 0x23), kCrop, 3, 3) == accept(32)); // 131 >> 2 = 32 (floor)
    CHECK(place(quad(0x00, 0x00, 0x40, 0x40), kCrop, 3, 3) == accept(32)); // a slope is fine
    CHECK(place(quad(0x31, 0x32, 0x33, 0x34), kCrop, 3, 3) == accept(50)); // no 0xf0 quantisation
    CHECK(place(quad(0xff, 0xff, 0xff, 0xff), kCrop, 3, 3) == accept(0xff));
    // The neighbouring vertices do not count: (5, 3) is outside the quad of tile (3, 3).
    CHECK(place(island(0x30, {{{5, 3}, 0x00}}), kCrop, 3, 3) == accept(48));
    // The east column: vertex (10, y) reads 0 (the original's boundary vertex).
    CHECK(place(island(0x40), kCrop, 9, 3) == accept(32)); // (64 + 0 + 64 + 0) >> 2
    CHECK(place(island(0x30), kCrop, 9, 3) == kLow);       // (48 + 48) >> 2 = 24
    // The threshold is read from the parameters (PlacementParams::min_level), not a literal.
    sim::PlacementParams strict;
    strict.min_level = 33;
    CHECK(place(quad(0x20, 0x20, 0x20, 0x20), kCrop, 3, 3, 0, strict) == kLow);
}

TEST_CASE("tile object: the overlap row of type slot +0x6c for a placed group 0xe1") {
    // AnnoGame 0x10008931: an existing object whose flag word has 0x20 shares the tile; a placed
    // group 0xe1 has no row of its own (0x10008a3e), so every other existing object refuses.
    CHECK(place(island(0x30, {}, {{{3, 3}, kStone}}), kCrop, 3, 3) == accept(48));
    CHECK(place(island(0x30, {}, {{{3, 3}, kCrop2}}), kCrop, 3, 3) == accept(48));
    CHECK(place(island(0x30, {}, {{{3, 3}, kHouse}}), kCrop, 3, 3) == kOccupied);
    CHECK(place(island(0x30, {}, {{{3, 3}, kRoad}}), kCrop, 3, 3) == kOccupied);
    // Occupancy precedes the height (0x1007c8ad before 0x1007cb50): a building on low land is
    // Occupied, not TerrainRefused.
    CHECK(place(island(0x10, {}, {{{3, 3}, kHouse}}), kCrop, 3, 3) == kOccupied);
    // A multi-tile tile object reads its whole footprint for occupancy (the house at the far
    // corner of the 3 x 3).
    CHECK(place(island(0x30, {}, {{{5, 5}, kHouse}}), kTree3, 3, 3) == kOccupied);
    CHECK(place(island(0x30, {}, {{{5, 5}, kStone}}), kTree3, 3, 3) == accept(48));
}

TEST_CASE("tile object: the skipped classes accept below the threshold") {
    // AnnoWelt 0x1007cbc2-0x1007cbe9: class bits 0x4000 or 0x2000, or the group 0x141, accept the
    // byte without the level test. On land 0x10 the byte is 16.
    const sim::Island low = island(0x10);
    CHECK(place(low, kCrop, 3, 3) == kLow); // the control: class 0, group 0xe1
    CHECK(place(low, kRoadQuad, 3, 3) == accept(16));
    CHECK(place(low, kCoastQuad, 3, 3) == accept(16));
    CHECK(place(low, kRockQuad, 3, 3) == accept(16));
    // The class list is data: without it the two classes take the level test, the group stays.
    sim::PlacementParams none;
    none.level_exempt_classes.clear();
    CHECK(place(low, kRoadQuad, 3, 3, 0, none) == kLow);
    CHECK(place(low, kCoastQuad, 3, 3, 0, none) == kLow);
    CHECK(place(low, kRockQuad, 3, 3, 0, none) == accept(16));
    sim::PlacementParams no_group;
    no_group.exempt_group = 0;
    CHECK(place(low, kRockQuad, 3, 3, 0, no_group) == kLow);
}

TEST_CASE("tile object: rotation turns the footprint, never the quad") {
    // A 3 x 1 tile object: at x = 8 on a 10-wide island rotations 0 and 2 poke out (8 + 3 > 10);
    // rotations 1 and 3 are 1 x 3 and fit. The height is the quad of the origin tile (8, 3) in
    // every rotation: vertex (9, 3) = 0 gives (48 + 0 + 48 + 48) >> 2 = 36; the vertex (8, 6) under
    // the far end of the turned footprint is not read.
    const sim::Island is = island(0x30, {{{9, 3}, 0x00}, {{8, 6}, 0x00}});
    CHECK(place(is, kLong, 8, 3, 0) == kBounds);
    CHECK(place(is, kLong, 8, 3, 2) == kBounds);
    CHECK(place(is, kLong, 8, 3, 1) == accept(36));
    CHECK(place(is, kLong, 8, 3, 3) == accept(36));
    // Turned at a free spot, the same quad and the same byte.
    CHECK(place(is, kLong, 5, 5, 0) == place(is, kLong, 5, 5, 1));
}

TEST_CASE("tile object: a 3 x 3 footprint reads only its origin quad") {
    // ROHSTOFF 3215 (3 x 3, 0x29): AnnoGame 0x10008fae reads the four vertices at the pointer only.
    std::vector<std::pair<std::pair<int, int>, std::uint8_t>> origin_high;
    for (int y = 3; y <= 4; ++y) {
        for (int x = 3; x <= 4; ++x) {
            origin_high.push_back({{x, y}, 0x30});
        }
    }
    CHECK(place(island(0x10, origin_high), kTree3, 3, 3) == accept(48));
    std::vector<std::pair<std::pair<int, int>, std::uint8_t>> origin_low;
    for (int y = 3; y <= 4; ++y) {
        for (int x = 3; x <= 4; ++x) {
            origin_low.push_back({{x, y}, 0x10});
        }
    }
    CHECK(place(island(0x30, origin_low), kTree3, 3, 3) == kLow);
    CHECK(place(island(0x30), kTree3, 8, 3) == kBounds);
}

TEST_CASE("tile object: what the class does not decide stays Unsupported") {
    // No terrain: never an accept and never a height of 0.
    sim::Island bare;
    REQUIRE(bare.tiles.reset(0, 0, 10, 10, 8));
    CHECK(place(bare, kCrop, 3, 3) == kUnsupported);
    // The coverage bit 0x10 with the single-quad mode: the coverage test is evidenced for the
    // mountain class only.
    CHECK(place(island(0x30), kCoverQuad, 3, 3) == kUnsupported);
    // An existing id the registry does not know.
    CHECK(place(island(0x30, {}, {{{3, 3}, 999}}), kCrop, 3, 3) == kUnsupported);
}

namespace {

std::string digest_line(const std::string& rel, std::size_t k, std::uint32_t w, std::uint32_t h,
                        std::uint32_t type, const std::uint64_t (&n)[5], std::uint64_t fnv) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(fnv));
    return rel + " " + std::to_string(k) + " " + std::to_string(w) + "x" + std::to_string(h) +
           " type " + std::to_string(type) + " accept " + std::to_string(n[0]) + " low " +
           std::to_string(n[1]) + " occupied " + std::to_string(n[2]) + " bounds " +
           std::to_string(n[3]) + " unsupported " + std::to_string(n[4]) + " fnv " + buf;
}

} // namespace

TEST_CASE("tile object: placement_rule agrees with the oracle over every shipped island tile") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // The oracle's lines (tools/placement_ref.py tilecorpus --write).
    std::vector<std::string> want;
    {
        std::ifstream in(fs::path(ANNO_FIXTURES_DIR) / "sim" / "tile_object_corpus.txt");
        REQUIRE_MESSAGE(in.good(), "tile_object_corpus.txt not found");
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty() && line[0] != '#') {
                want.push_back(line);
            }
        }
    }
    REQUIRE(want.size() > 1000);
    const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::OptionalNote);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    const sim::TypeRegistry& reg = *loaded.types;
    // The two types of placement_ref.TILE_CORPUS_TYPES.
    const std::uint32_t kTypes[] = {1, 3215};
    for (const std::uint32_t id : kTypes) {
        const sim::BuildingType* t = reg.find(sim::TypeId{id});
        REQUIRE(t != nullptr);
        REQUIRE(t->flags_word == 0x29U);
    }
    // Files sorted by their '/'-joined path relative to the game directory, as the oracle.
    std::vector<std::string> files;
    for (const auto& e : fs::recursive_directory_iterator(*dir / "Scenes")) {
        if (!e.is_regular_file()) {
            continue;
        }
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".scp" || ext == ".sze") {
            files.push_back(fs::relative(e.path(), *dir).generic_string());
        }
    }
    std::sort(files.begin(), files.end());
    std::vector<std::string> got;
    std::uint64_t totals[5] = {};
    for (const std::string& rel : files) {
        INFO(rel);
        const auto buf = anno::test::read_file(*dir / rel);
        auto isls = anno::formats::insel::islands(buf);
        REQUIRE(isls);
        for (std::size_t k = 0; k < isls.value().size(); ++k) {
            const anno::formats::insel::Island& raw = isls.value()[k];
            const std::uint32_t w = raw.width, h = raw.height;
            auto heights = raw.heights();
            if (!heights) {
                got.push_back(rel + " " + std::to_string(k) + " " + std::to_string(w) + "x" +
                              std::to_string(h) + " no-height");
                continue;
            }
            sim::Island is;
            REQUIRE(is.tiles.reset(0, 0, w, h, 8));
            // The corpus's occupancy model (placement_ref.tile_corpus_islands): GELAENDE, FLUESSE
            // and PROD2 in file order, every tile of a known item's rotated footprint, first wins.
            for (const anno::formats::chunk::Chunk& rec : raw.records) {
                const std::string_view name = rec.name();
                if (name != "GELAENDE" && name != "FLUESSE" && name != "PROD2") {
                    continue;
                }
                auto block = anno::formats::insel::Block::parse(rec.payload);
                REQUIRE(block);
                const auto items = block.value().data;
                const std::size_t whole = items.size() - items.size() % 6;
                for (std::size_t i = 0; i < whole; i += 6) {
                    const auto o = anno::formats::insel::PlacedObject::parse(items.subspan(i, 6),
                                                                             block.value());
                    const sim::BuildingType* t = reg.find(sim::TypeId{o.anlage});
                    if (o.anlage == 0 || t == nullptr) {
                        continue;
                    }
                    const auto [fw, fh] = o.footprint(static_cast<std::uint8_t>(t->width),
                                                      static_cast<std::uint8_t>(t->height));
                    for (std::uint32_t ty = o.y; ty < std::min<std::uint32_t>(o.y + fh, h); ++ty) {
                        for (std::uint32_t tx = o.x; tx < std::min<std::uint32_t>(o.x + fw, w);
                             ++tx) {
                            const std::uint64_t idx = std::uint64_t{ty} * w + tx;
                            if (is.tiles.object(idx) == 0) {
                                is.tiles.set_object(idx, o.anlage);
                            }
                        }
                    }
                }
            }
            is.terrain = sim::make_terrain(std::move(heights.value()), {}, {});
            for (const std::uint32_t id : kTypes) {
                const sim::BuildingType& t = *reg.find(sim::TypeId{id});
                std::uint64_t n[5] = {};
                std::uint64_t fnv = 0xcbf29ce484222325ULL;
                for (std::uint32_t y = 0; y < h; ++y) {
                    for (std::uint32_t x = 0; x < w; ++x) {
                        sim::PlacementRecord pr;
                        pr.type = t.id;
                        pr.x = static_cast<std::int32_t>(x);
                        pr.y = static_cast<std::int32_t>(y);
                        const sim::PlacementResult r = sim::placement_rule(is, reg, t, pr);
                        unsigned code = 4;
                        switch (r.reject) {
                        case sim::Reject::None:
                            code = 0;
                            break;
                        case sim::Reject::TerrainRefused:
                            code = 1;
                            break;
                        case sim::Reject::Occupied:
                            code = 2;
                            break;
                        case sim::Reject::OutOfIsland:
                            code = 3;
                            break;
                        default:
                            code = 4;
                            break;
                        }
                        ++n[code];
                        for (const unsigned byte : {code, code == 0 ? unsigned{r.height} : 0U}) {
                            fnv = (fnv ^ byte) * 0x100000001b3ULL;
                        }
                    }
                }
                for (std::size_t c = 0; c < 5; ++c) {
                    totals[c] += n[c];
                }
                got.push_back(digest_line(rel, k, w, h, id, n, fnv));
            }
        }
    }
    MESSAGE("tile corpus: " << got.size() << " lines; accept " << totals[0] << " low " << totals[1]
                            << " occupied " << totals[2] << " bounds " << totals[3]
                            << " unsupported " << totals[4]);
    CHECK(got.size() == want.size());
    std::size_t differ = 0;
    for (std::size_t i = 0; i < std::min(got.size(), want.size()); ++i) {
        if (got[i] != want[i]) {
            if (++differ <= 5) {
                MESSAGE("file:   " << want[i] << "\nengine: " << got[i]);
            }
        }
    }
    CHECK(differ == 0);
    // Every class of verdict occurs, so the digest is not trivially equal.
    CHECK(totals[0] > 0);
    CHECK(totals[1] > 0);
    CHECK(totals[2] > 0);
    CHECK(totals[3] > 0);
}
