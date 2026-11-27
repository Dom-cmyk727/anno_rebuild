// ROADS-2a: the road reshaper (road_shape.hpp; docs/re/annowelt.md "The road reshaper's
// classifier"; oracle tools/road_shape.py). Expected values are derived by hand from the table at
// AnnoWelt 0x100c9d90 (mask N=1, E=2, S=4, W=8).
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/insel.hpp>
#include <anno/sim/road_shape.hpp>
#include <anno/test/game_dir.hpp>

namespace {

using anno::sim::RoadAt;
using anno::sim::RoadObject;
using anno::sim::RoadShapeTable;
using anno::sim::ShapedRoad;
using anno::sim::TileXY;

const RoadShapeTable kTable{};
constexpr std::uint32_t kDirt = 2520;

RoadAt none() {
    return [](TileXY) -> std::optional<RoadObject> { return std::nullopt; };
}

RoadAt world(std::map<TileXY, RoadObject> objs) {
    return [objs = std::move(objs)](TileXY p) -> std::optional<RoadObject> {
        const auto it = objs.find(p);
        if (it == objs.end()) {
            return std::nullopt;
        }
        return it->second;
    };
}

std::vector<std::uint32_t> dirt(std::size_t n) {
    return std::vector<std::uint32_t>(n, kDirt);
}

} // namespace

TEST_CASE("road shape: the table is the original's, folded per family") {
    CHECK(anno::sim::road_shape(kTable, 2500, 10).id == 2500); // E+W straight
    CHECK(anno::sim::road_shape(kTable, 2500, 10).rotation == 0);
    CHECK(anno::sim::road_shape(kTable, 2500, 5).rotation == 3); // N+S straight
    CHECK(anno::sim::road_shape(kTable, kDirt, 6).id == 2525);   // E+S corner, dirt
    CHECK(anno::sim::road_shape(kTable, kDirt, 15).id == 2526);  // cross
    CHECK(anno::sim::road_shape(kTable, 2522, 2).id == 2528);    // a dirt variant's end cap
    CHECK(anno::sim::road_shaped_base(kTable, 2523) == std::optional<std::uint32_t>{2500});
    CHECK_FALSE(anno::sim::road_shaped_base(kTable, 2510).has_value()); // a ramp
    CHECK(anno::sim::road_connects(kTable, 2510));                      // connects all the same
    CHECK_FALSE(anno::sim::road_connects(kTable, 2550));                // zeroed at 0x1007f0c0
}

TEST_CASE("road shape: an L turns its legs and places one corner") {
    // (0,0) -> (2,0) east, then (2,1) -> (2,2) south: the owner's L.
    const std::vector<TileXY> path{{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}};
    const auto r = anno::sim::reshape_road_path(kTable, path, dirt(5), none());
    REQUIRE(r.size() == 5);
    CHECK(r[0] == ShapedRoad{{0, 0}, 2528, 0});  // end cap, open to the east (mask 2)
    CHECK(r[1] == ShapedRoad{{1, 0}, kDirt, 0}); // E+W straight
    CHECK(r[2] == ShapedRoad{{2, 0}, 2525, 3});  // W+S corner: mask 12 -> rotation 3
    CHECK(r[3] == ShapedRoad{{2, 1}, kDirt, 3}); // N+S straight
    CHECK(r[4] == ShapedRoad{{2, 2}, 2528, 1});  // end cap, open to the north (mask 1)
}

TEST_CASE("road shape: a T and a cross") {
    // A row (0,1)-(2,1) and a stub (1,0) north of its middle: T at (1,1), mask E+W+N = 11.
    const auto t =
        anno::sim::reshape_road_path(kTable, {{0, 1}, {1, 1}, {2, 1}, {1, 0}}, dirt(4), none());
    REQUIRE(t.size() == 4);
    CHECK(t[1] == ShapedRoad{{1, 1}, 2527, 2});
    // A plus: the centre has all four neighbours.
    const auto c = anno::sim::reshape_road_path(kTable, {{1, 0}, {1, 1}, {1, 2}, {0, 1}, {2, 1}},
                                                dirt(5), none());
    CHECK(c[1] == ShapedRoad{{1, 1}, 2526, 3});
}

TEST_CASE("road shape: a lone tile and a straight keep their cosmetic variant") {
    const auto lone = anno::sim::reshape_road_path(kTable, {{5, 5}}, {2522}, none());
    REQUIRE(lone.size() == 1);
    CHECK(lone[0] == ShapedRoad{{5, 5}, 2522, 3});
    const auto run =
        anno::sim::reshape_road_path(kTable, {{0, 0}, {1, 0}, {2, 0}}, {2521, 2523, 2524}, none());
    CHECK(run[1] == ShapedRoad{{1, 0}, 2523, 0});
}

TEST_CASE("road shape: laying beside an existing road rewrites the neighbour") {
    // An existing N-S dirt straight at (1,0)-(1,2); a new road (0,1) joins its middle from the
    // west.
    const RoadAt w = world({{{1, 0}, {2528, 3}}, {{1, 1}, {kDirt, 3}}, {{1, 2}, {2528, 1}}});
    const auto r = anno::sim::reshape_road_path(kTable, {{0, 1}}, dirt(1), w);
    REQUIRE(r.size() == 2);
    CHECK(r[0] == ShapedRoad{{0, 1}, 2528, 0}); // the new tile: end cap open to the east
    CHECK(r[1] == ShapedRoad{{1, 1}, 2527, 3}); // the neighbour: N+S+W, mask 13 -> T rotation 3
}

TEST_CASE("road shape: cobblestone stays cobblestone, and a ramp connects but is not rewritten") {
    const RoadAt w = world({{{1, 0}, {2510, 0}}});
    const auto r = anno::sim::reshape_road_path(kTable, {{0, 0}}, {2500}, w);
    REQUIRE(r.size() == 1);
    CHECK(r[0] == ShapedRoad{{0, 0}, 2508, 0}); // cobblestone end cap toward the ramp
}

TEST_CASE("road shape: agrees with the oracle over every shipped road tile" *
          doctest::test_suite("heavy")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const std::filesystem::path digest =
        std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "road_shape_corpus.txt";
    std::ifstream in(digest);
    REQUIRE_MESSAGE(in.good(), "missing " << digest.string());
    std::map<std::string, std::string> want;
    for (std::string line; std::getline(in, line);) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto cut = line.find(" shaped ");
        want[line.substr(0, cut)] = line.substr(cut);
    }
    std::vector<std::filesystem::path> files;
    for (const auto& e : std::filesystem::recursive_directory_iterator(*dir / "Scenes")) {
        if (e.is_regular_file() && e.path().extension() == ".sze") {
            files.push_back(std::filesystem::relative(e.path(), *dir));
        }
    }
    std::sort(files.begin(), files.end());
    std::uint64_t total = 0, agree = 0, lines = 0;
    for (const auto& rel : files) {
        const auto buf = anno::test::read_file(*dir / rel);
        auto isls = anno::formats::insel::islands(buf);
        if (!isls) {
            continue;
        }
        for (std::size_t n = 0; n < isls.value().size(); ++n) {
            std::map<TileXY, RoadObject> roads;
            auto blocks = isls.value()[n].blocks("PROD2");
            REQUIRE(blocks);
            for (const auto& b : blocks.value()) {
                for (std::size_t k = 0; k + 6 <= b.data.size(); k += 6) {
                    const auto o =
                        anno::formats::insel::PlacedObject::parse(b.data.subspan(k, 6), b);
                    if (anno::sim::road_connects(kTable, o.anlage)) {
                        roads[{o.x, o.y}] = {o.anlage, o.rotation};
                    }
                }
            }
            std::uint64_t t = 0, a = 0;
            const RoadAt at = world(roads);
            for (const auto& [p, obj] : roads) {
                const auto base = anno::sim::road_shaped_base(kTable, obj.id);
                if (!base) {
                    continue;
                }
                const auto r = anno::sim::reshape_road_path(kTable, {p}, {obj.id}, [&](TileXY q) {
                    return q == p ? std::optional<RoadObject>{} : at(q);
                });
                const auto want_base = anno::sim::road_shaped_base(kTable, r[0].id);
                ++t;
                const bool straight = want_base == std::optional<std::uint32_t>{2500};
                const bool cross = want_base == std::optional<std::uint32_t>{2506};
                const bool rot = cross || (straight ? r[0].rotation % 2 == obj.rotation % 2
                                                    : r[0].rotation == obj.rotation);
                a += (want_base == base && rot) ? std::uint64_t{1} : std::uint64_t{0};
            }
            if (t != 0) {
                std::string key = rel.generic_string() + " " + std::to_string(n);
                std::ostringstream got;
                got << " shaped " << t << " agree " << a;
                CHECK_MESSAGE(want[key] == got.str(), key);
                total += t;
                agree += a;
                ++lines;
            }
        }
    }
    CHECK(lines + 1 == want.size()); // every island line plus TOTAL
    CHECK(want["TOTAL"] == " shaped " + std::to_string(total) + " agree " + std::to_string(agree));
    MESSAGE("road shape corpus: " << agree << " of " << total << " shaped tiles agree");
}
