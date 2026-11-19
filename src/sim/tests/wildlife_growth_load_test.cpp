#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include <doctest/doctest.h>

#include <anno/formats/sha256.hpp>
#include <anno/formats/text.hpp>
#include <anno/sim_load/wildlife_ground_prepare.hpp>
#include <anno/sim_load/wildlife_growth_load.hpp>
#include <anno/test/game_dir.hpp>

namespace load = anno::sim_load;
namespace fmt = anno::formats;
namespace {
fmt::ByteBuffer bytes(const std::string& s) {
    return {s.begin(), s.end()};
}
std::vector<std::string> split(const std::string& text, char delimiter) {
    std::istringstream in(text);
    std::vector<std::string> out;
    std::string token;
    while (std::getline(in, token, delimiter))
        out.push_back(token);
    return out;
}
std::uint64_t number(const std::string& s) {
    std::uint64_t n = 0;
    const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), n);
    REQUIRE(error == std::errc{});
    REQUIRE(end == s.data() + s.size());
    return n;
}
std::map<std::string, std::string> fields(const std::vector<std::string>& tokens,
                                          std::size_t start) {
    std::map<std::string, std::string> out;
    for (auto i = start; i < tokens.size(); ++i) {
        const auto pair = split(tokens[i], '=');
        REQUIRE(pair.size() == 2);
        REQUIRE(out.emplace(pair[0], pair[1]).second);
    }
    return out;
}
std::filesystem::path fixture(const char* name) {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / name;
}
const std::string legal = "[WACHSTUM]\nSize: 1,1\nId: 1\nProz: 20\n[END]\n";
void refusal(const std::string& old, const std::string& replacement, const std::string& why) {
    const auto control = load::load_wildlife_growth(bytes(legal));
    REQUIRE(control);
    CHECK(control.value().values[64] == 20);
    auto changed = legal;
    const auto at = changed.find(old);
    REQUIRE(at != std::string::npos);
    REQUIRE(changed.find(old, at + old.size()) == std::string::npos);
    changed.replace(at, old.size(), replacement);
    REQUIRE(changed != legal);
    const auto source = bytes(changed), before = source;
    const auto result = load::load_wildlife_growth(source);
    REQUIRE_FALSE(result);
    CHECK(result.error().find(why) != std::string::npos);
    CHECK(source == before);
}
void numerical_integration(const load::GrowthLookup& lookup, std::uint16_t gfx,
                           std::uint8_t expected) {
    const std::array<std::uint16_t, 1> graphics{gfx};
    const std::array<std::uint32_t, 1> objects{0};
    const std::array<std::uint8_t, 1> ground{167};
    std::array<std::uint8_t, 25> kernel{};
    kernel[12] = 12;
    const std::array<load::GroundObjectFact, 1> facts{{{}}};
    const auto result = load::prepare_wildlife_ground(
        {{17, -11, 1, 1}, graphics, objects, ground, lookup.values, kernel, facts});
    REQUIRE(result);
    CHECK(result.value() == std::vector<std::uint8_t>{expected});
    CHECK(ground[0] == 167);
}
} // namespace

TEST_CASE("wildlife growth: authored full-map vectors") {
    std::ifstream in(fixture("wildlife_growth_vectors.txt"));
    REQUIRE(in);
    std::string line;
    std::size_t count = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        const auto arrow = line.find("=>");
        REQUIRE(arrow != std::string::npos);
        const auto left = split(line.substr(0, arrow - 1), ' ');
        const auto right = fields(split(line.substr(arrow + 3), ' '), 0);
        REQUIRE(left.size() == 3);
        REQUIRE(left[0] == "growth");
        INFO(left[1]);
        const auto input = fields(left, 2);
        const auto source = anno::test::read_file(fixture(input.at("source").c_str()));
        const auto before = source;
        const auto result = load::load_wildlife_growth(source);
        REQUIRE(result);
        std::vector<std::uint8_t> expected(16384, 0);
        if (right.at("bytes") != "-")
            for (const auto& pair : split(right.at("bytes"), ',')) {
                const auto p = split(pair, ':');
                REQUIRE(p.size() == 2);
                const auto index = number(p[0]), value = number(p[1]);
                REQUIRE(index < expected.size());
                REQUIRE(value <= 255);
                expected[static_cast<std::size_t>(index)] = static_cast<std::uint8_t>(value);
            }
        CHECK(result.value().values == expected); // Includes EVERY untouched zero region.
        CHECK(result.value().records.size() == number(right.at("ids")));
        CHECK(result.value().sections == number(right.at("sections")));
        CHECK(result.value().short_rows.size() == number(right.at("short")));
        CHECK(result.value().bridge_rows == number(right.at("bridge")));
        CHECK(result.value().climate_rows == number(right.at("climate")));
        CHECK(result.value().source_sha256 == fmt::sha256_hex(source));
        CHECK(source == before);
        if (left[1] == "comma_stop" || left[1] == "short") {
            REQUIRE(result.value().short_rows.size() == 1);
            const auto& row = result.value().short_rows[0];
            CHECK(row.id == 2);
            CHECK(row.row == 0);
            CHECK(row.width == 3);
            CHECK(row.parsed == 1);
            CHECK(row.stopped == (left[1] == "comma_stop"));
        }
        ++count;
    }
    CHECK(count == 13);
}

TEST_CASE("wildlife growth: checked refusal changes one precondition") {
    refusal("Size: 1,1", "Size: 0,1", "staging domain");
    refusal("Size: 1,1", "Size: 257,1", "staging domain");
    refusal("Size: 1,1", "Size: 2147483647,2147483647", "staging domain");
    refusal("Size: 1,1", "Size: 1,2", "incomplete record");
    refusal("Proz: 20", "Proz: 2147483648", "out of range");
    refusal("Proz: 20", "Proz: -2147483649", "out of range");
    refusal("Proz: 20", "Proz: 0x14", "numeric syntax");
    refusal("Proz: 20", "Proz: 020", "numeric syntax");
    refusal("Proz: 20", "Proz: ++20", "integer required");
    refusal("Proz: 20", "Proz: 20,", "integer required");
    refusal("Proz: 20", "Proz: 20,1", "too many");
    refusal("Proz: 20", "Proz: 20\nProz: 1", "extra Proz");
    refusal("Proz: 20", "Proz: 20 bad", "numeric tail");
    refusal("Id: 1\n", "", "Id required");
    refusal("Id: 1", "Size: 1,1\nId: 1", "Size only once");
    refusal("Proz: 20", "Size: 1,1\nProz: 20", "Size only once");
    refusal("Proz:", "Other:", "unknown key");
    refusal("[WACHSTUM]", "[OTHER]", "WACHSTUM required");
    refusal("[END]\n", "", "END and");
    refusal("[END]\n", "[END]", "newline-terminated");
    refusal("Id: 1", "; [ comment\nId: 1", "bracket in comment");
    refusal("Proz: 20", "Proz: \"20\"", "quoted code");
    refusal("[END]", "Klima: POLAR,POLAR,POLAR\n[END]", "Klima sidecar");
    refusal("[END]", "[END]\nId: 2", "content after END");
    refusal("Proz: 20", std::string("Proz: 20\0", 9), "without NUL");
    CHECK_FALSE(load::load_wildlife_growth({}));
}

TEST_CASE("wildlife growth: ownership CRLF diagnostics and generic parser remain independent") {
    const auto result = [] {
        auto source = bytes(legal);
        const auto before = source;
        auto loaded = load::load_wildlife_growth(source);
        REQUIRE(loaded);
        CHECK(source == before);
        CHECK(loaded.value().records.size() == 1);
        CHECK(loaded.value().records[0].line == 3);
        source.assign(1, 0);
        return loaded;
    }();
    REQUIRE(result);
    CHECK(result.value().values[64] == 20);
    CHECK(result.value().records[0].id == 1);
    std::string crlf;
    for (const auto c : legal) {
        if (c == '\n')
            crlf += '\r';
        crlf += c;
    }
    const auto windows = load::load_wildlife_growth(bytes(crlf));
    REQUIRE(windows);
    CHECK(windows.value().values == result.value().values);
    CHECK(windows.value().source_sha256 != result.value().source_sha256);
    // Existing generic diagnostic intentionally sees six, unlike the original growth list.
    const auto generic = fmt::text::ints(bytes("100,100,100 10,20,50"));
    REQUIRE(generic);
    CHECK(generic->size() == 6);
    numerical_integration(result.value(), 259, 162); // Id 1, rotation 3; p20 -> b2 -> a2.
}

TEST_CASE("wildlife growth: original GOG source full lookup and authored numerical integration") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const auto source = anno::test::read_file(*dir / "Data/Wachstum.dat");
    const auto before = source;
    const auto result = load::load_wildlife_growth(source);
    REQUIRE(result);
    const auto& value = result.value();
    CHECK(source == before);
    CHECK(value.source_sha256 ==
          "0b8c4416dd43fb76ea2dca6e82bfd822b0b861f15440e83479a8aa56b8d92e24");
    CHECK(value.records.size() == 146);
    CHECK(value.sections == 1);
    CHECK(value.bridge_rows == 30);
    CHECK(value.climate_rows == 92);
    REQUIRE(value.short_rows.size() == 1);
    CHECK(value.short_rows[0].line == 765);
    CHECK(value.short_rows[0].parsed == 3);
    CHECK(value.short_rows[0].width == 6);
    CHECK(value.short_rows[0].stopped);
    // Independently computed from the original producer before C++ implementation.
    CHECK(fmt::sha256_hex(value.values) ==
          "760314eb744a0f64eff6bd827be8876153f7cf69e39739e6d4ef8f53642608a7");
    if (const auto* oracle = std::getenv("ANNO_WILDLIFE_GROWTH_ORACLE")) {
        const auto expected = anno::test::read_file(oracle);
        REQUIRE(expected.size() == 16384);
        CHECK(value.values == expected);
        std::cout << "Growth independent full-byte comparison: 16384 bytes agree\n";
    } else {
        std::cout << "External full-byte oracle unavailable; known GOG full-map SHA checked\n";
    }
    // Only the growth source is real here; object/kernel/position/ground are AUTHORED.
    // Id 11 cell 0 is p100 in this exact file. Rotation 3 does not change lookup index.
    numerical_integration(value, 2819, 170);
}
