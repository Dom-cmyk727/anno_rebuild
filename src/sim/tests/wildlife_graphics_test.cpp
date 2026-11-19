#include <charconv>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim_load/wildlife_graphics.hpp>

namespace sim = anno::sim;
namespace load = anno::sim_load;
namespace {
std::uint32_t number(const std::string& text) {
    std::uint32_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    REQUIRE(error == std::errc{});
    REQUIRE(end == text.data() + text.size());
    return value;
}
template <class T> std::vector<T> column(const std::string& text) {
    std::vector<T> out;
    std::istringstream input(text);
    std::string token;
    while (std::getline(input, token, ',')) {
        const auto value = number(token);
        REQUIRE(value <= std::numeric_limits<T>::max());
        out.push_back(static_cast<T>(value));
    }
    return out;
}
std::map<std::string, std::string> fields(std::istringstream& input) {
    std::map<std::string, std::string> out;
    std::string token;
    while (input >> token) {
        const auto equals = token.find('=');
        REQUIRE(equals != std::string::npos);
        REQUIRE(out.emplace(token.substr(0, equals), token.substr(equals + 1)).second);
    }
    return out;
}
sim::Island island(std::uint32_t w, std::uint32_t h, std::vector<std::uint8_t> heights,
                   std::vector<std::uint16_t> graphics, std::vector<std::uint8_t> fertility = {}) {
    sim::Island out;
    REQUIRE(out.tiles.reset(-13, 27, w, h, 3)); // Nonzero origin, non-default chunks.
    out.terrain = sim::make_terrain(std::move(heights), std::move(graphics), std::move(fertility));
    return out;
}
void equal(const sim::Terrain& a, const sim::Terrain& b) {
    CHECK(a.height == b.height);
    CHECK(a.gfx == b.gfx);
    CHECK(a.fertility == b.fertility);
    CHECK(a.digest == b.digest);
}
} // namespace

TEST_CASE("wildlife graphics: every hand vector compares the full plane and digest") {
    std::ifstream file(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" /
                       "wildlife_postload_vectors.txt");
    REQUIRE(file);
    std::string line;
    std::size_t ran = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        const auto arrow = line.find("=>");
        REQUIRE(arrow != std::string::npos);
        std::istringstream left(line.substr(0, arrow)), right(line.substr(arrow + 2));
        std::string kind, label;
        left >> kind >> label;
        if (kind == "fill")
            continue;
        REQUIRE(kind == "gfx");
        CAPTURE(label);
        const auto in = fields(left), expected = fields(right);
        REQUIRE(in.size() == 4);
        REQUIRE(expected.size() == 2);
        const auto w = number(in.at("w")), h = number(in.at("h"));
        auto source =
            island(w, h, column<std::uint8_t>(in.at("height")), column<std::uint16_t>(in.at("gfx")),
                   std::vector<std::uint8_t>(std::uint64_t{w} * h, 0xad));
        const auto snapshot = *source.terrain;
        const auto result = load::finalize_wildlife_graphics(source);
        REQUIRE(result);
        const auto& out = *result.value().terrain;
        CHECK(out.gfx == column<std::uint16_t>(expected.at("gfx")));
        CHECK(result.value().cleared == number(expected.at("cleared")));
        CHECK(out.height == snapshot.height);
        CHECK(out.fertility == snapshot.fertility);
        CHECK(sim::terrain_matches(out));
        CHECK((out.digest != snapshot.digest) == (result.value().cleared != 0));
        CHECK(result.value().terrain.get() != source.terrain.get());
        equal(snapshot, *source.terrain);
        const auto repeated = load::finalize_wildlife_graphics(source);
        REQUIRE(repeated);
        equal(out, *repeated.value().terrain);
        source.terrain = result.value().terrain;
        const auto idempotent = load::finalize_wildlife_graphics(source);
        REQUIRE(idempotent);
        CHECK(idempotent.value().cleared == 0);
        equal(out, *idempotent.value().terrain);
        ++ran;
    }
    CHECK(file.eof());
    CHECK(ran >= 12); // The 12 independently authored cases are mandatory coverage.
}

TEST_CASE("wildlife graphics: owned output survives destruction and preserves other channels") {
    const auto output = [] {
        auto input =
            island(3, 2, {0, 0, 0, 0, 0, 0}, {0x20ab, 0x6001, 0x20ff, 0x2101, 0x1000, 0x2000},
                   {0x91, 2, 3, 4, 5, 0xff});
        // Geometry/object channels are inputs to neither the predicate nor its writes.
        input.tiles.set_object(0, 104);
        const auto digest = std::as_const(input.tiles).chunk_digest(0);
        const auto result = load::finalize_wildlife_graphics(input);
        REQUIRE(result);
        CHECK(std::as_const(input.tiles).chunk_digest(0) == digest);
        CHECK(result.value().cleared == 3);
        return result.value().terrain;
    }();
    REQUIRE(output);
    CHECK(output->gfx == std::vector<std::uint16_t>{0, 0x6001, 0, 0x2101, 0x1000, 0});
    CHECK(output->height == std::vector<std::uint8_t>(6, 0));
    CHECK(output->fertility == std::vector<std::uint8_t>{0x91, 2, 3, 4, 5, 0xff});
    CHECK(sim::terrain_matches(*output));
}

TEST_CASE("wildlife graphics: malformed inputs refuse without touching source") {
    auto input = island(1, 1, {0}, {0x20ab});
    const auto reject = [&](const std::string& reason) {
        const auto snapshot = input.terrain ? *input.terrain : sim::Terrain{};
        const auto result = load::finalize_wildlife_graphics(input);
        REQUIRE_FALSE(result);
        CHECK(result.error() == "wildlife graphics: " + reason);
        if (input.terrain)
            equal(snapshot, *input.terrain);
    };
    input.terrain.reset();
    reject("terrain absent");
    input.terrain = sim::make_terrain({}, {0x20ab}, {});
    reject("HEIGHT length must equal width * height");
    input.terrain = sim::make_terrain({0, 1}, {0x20ab}, {});
    reject("HEIGHT length must equal width * height"); // No physical padding accepted.
    input.terrain = sim::make_terrain({0}, {}, {});
    reject("GFXID length must equal width * height");
    input.terrain = sim::make_terrain({0}, {0x20ab}, {1, 2});
    reject("fertility must be absent or width * height");
    auto stale = std::make_shared<sim::Terrain>();
    stale->height = {0};
    stale->gfx = {0x20ab};
    stale->digest = sim::terrain_digest(*stale) ^ 1U;
    input.terrain = stale;
    reject("terrain digest does not match columns");
    input.terrain = sim::make_terrain({0}, {0x20ab}, {});
    const auto absent_fertility = load::finalize_wildlife_graphics(input);
    REQUIRE(absent_fertility);
    CHECK(absent_fertility.value().terrain->fertility.empty());
    REQUIRE(input.tiles.reset(0, 0, 0, 1, 1));
    reject("positive tile geometry required");
    REQUIRE(input.tiles.reset(0, 0, 1, 0, 1));
    reject("positive tile geometry required");
}
