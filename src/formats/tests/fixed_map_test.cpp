#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/fixed_map.hpp>

namespace fm = anno::formats::fixed_map;
using anno::formats::ByteBuffer;

namespace {

ByteBuffer bytes_of(std::string_view text) {
    return ByteBuffer(text.begin(), text.end());
}

std::vector<std::uint8_t> read_all(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    return {std::istreambuf_iterator<char>(in), {}};
}

void expect_error(std::string_view text, std::uint32_t line, std::string_view message) {
    auto parsed = fm::parse(bytes_of(text));
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().line == line);
    CHECK(parsed.error().message == message);
    CHECK(fm::error_line(parsed.error()) ==
          "error: line " + std::to_string(line) + ": " + std::string(message));
}

const char* kLayout = "anno1503-fixed-map 1\n"
                      "name LanTest\n"
                      "edge 1431 1431\n"
                      "base Scenes/0010SingleEndlos/03_1024_med_256.sze\n"
                      "seed 7\n"
                      "island 2 16 32 islands\\nord\\a.scp\n"
                      "raw 2 4 5\n"
                      "island 1 0 8 ISLANDS/nord/b.scp\n"
                      "ore 1 9\n"
                      "start 1 2 20 36 1\n"
                      "start 0 1 4 12 0\n";

} // namespace

TEST_CASE("fixed map: a layout parses, sorts and digests independent of line endings") {
    auto parsed = fm::parse(bytes_of(kLayout));
    REQUIRE(parsed);
    const fm::FixedMap& map = parsed.value();
    CHECK(map.name == "LanTest");
    CHECK(map.width == 1431);
    CHECK(map.height == 1431);
    CHECK(map.base == "Scenes/0010SingleEndlos/03_1024_med_256.sze");
    CHECK(map.seed == 7);
    CHECK_FALSE(map.money.has_value());
    REQUIRE(map.islands.size() == 2);
    CHECK(map.islands[0].slot == 1);
    CHECK(map.islands[0].key == "ISLANDS/NORD/B.SCP");
    CHECK(map.islands[0].ore.size() == 1);
    CHECK(map.islands[0].ore[0] == 9);
    CHECK(map.islands[1].slot == 2);
    CHECK(map.islands[1].key == "ISLANDS/NORD/A.SCP");
    CHECK(map.islands[1].x == 16);
    REQUIRE(map.islands[1].raw.size() == 2);
    CHECK(map.islands[1].raw[0] == 4);
    REQUIRE(map.starts.size() == 2);
    CHECK(map.starts[0].seat == 0);
    CHECK(map.starts[0].island == 1);
    CHECK(map.starts[1].seat == 1);
    CHECK(map.starts[1].rotation == 1);

    std::string crlf = kLayout;
    for (std::size_t i = 0; i < crlf.size(); ++i) {
        if (crlf[i] == '\n') {
            crlf.insert(i, 1, '\r');
            ++i;
        }
    }
    auto again = fm::parse(bytes_of(crlf));
    REQUIRE(again);
    CHECK(fm::canonical(map) == fm::canonical(again.value()));
    CHECK(fm::digest(map) == fm::digest(again.value()));

    std::string moved = kLayout;
    const std::string from = "island 2 16 32";
    const auto at = moved.find(from);
    REQUIRE(at != std::string::npos);
    moved.replace(at, from.size(), "island 2 24 32");
    auto shifted = fm::parse(bytes_of(moved));
    REQUIRE(shifted);
    CHECK(fm::digest(shifted.value()) != fm::digest(map));

    const std::string text = fm::summary(map);
    CHECK(text.find("money default\n") != std::string::npos);
    CHECK(text.find("island 1 0 8 ISLANDS/NORD/B.SCP\n") != std::string::npos);
    CHECK(text.find("digest ") != std::string::npos);
}

TEST_CASE("fixed map: the reader refuses a broken line") {
    expect_error("", 1, "missing version line");
    expect_error("nope\n", 1, "version line must be 'anno1503-fixed-map 1'");
    expect_error("anno1503-fixed-map 1\nfoo 1\n", 2, "unknown record 'foo'");
    expect_error("anno1503-fixed-map 1\nisland 1 2\n", 2, "record 'island' needs 4 fields, has 2");
    expect_error("anno1503-fixed-map 1\nname A B\n", 2, "record 'name' needs 1 field, has 2");
    expect_error("anno1503-fixed-map 1\nname A\nname B\n", 3, "duplicate record 'name'");
    expect_error("anno1503-fixed-map 1\nname A\nedge 64 64\nbase b.sze\nseed 1\n"
                 "island 3 0 0 A.SCP\nisland 3 8 0 B.SCP\n",
                 7, "duplicate island slot 3");
    expect_error("anno1503-fixed-map 1\nedge foo 1\n", 2, "field 1 of 'edge' is not an integer");
    expect_error("anno1503-fixed-map 1\nedge 0 1\n", 2, "field 1 of 'edge' is out of range");
    expect_error("anno1503-fixed-map 1\nedge 16385 1\n", 2, "field 1 of 'edge' is out of range");
    expect_error("anno1503-fixed-map 1\nname A\nedge 64 64\nbase b.sze\nseed 1\n"
                 "start 0 9 1 1 4\n",
                 6, "field 5 of 'start' is out of range");
}

TEST_CASE("fixed map: comments and a blank line do not change the digest") {
    const std::string commented = std::string("# head\n\n") + kLayout + "# tail\n";
    auto plain = fm::parse(bytes_of(kLayout));
    auto noted = fm::parse(bytes_of(commented));
    REQUIRE(plain);
    REQUIRE(noted);
    CHECK(fm::digest(plain.value()) == fm::digest(noted.value()));
}

TEST_CASE("fixed map: the synthetic fixtures parse") {
    const std::filesystem::path dir = std::filesystem::path(ANNO_FIXTURES_DIR) / "fixed_map";
    const std::vector<std::uint8_t> ok_bytes = read_all(dir / "ok.txt");
    auto ok = fm::parse(ok_bytes);
    REQUIRE(ok);
    // Built here, not committed: a CRLF file is trailing whitespace to git diff --check.
    std::vector<std::uint8_t> crlf_bytes;
    for (std::uint8_t b : ok_bytes) {
        if (b == '\n') {
            crlf_bytes.push_back('\r');
        }
        crlf_bytes.push_back(b);
    }
    auto crlf = fm::parse(crlf_bytes);
    REQUIRE(crlf);
    CHECK(fm::digest(ok.value()) == fm::digest(crlf.value()));
    auto bad = fm::parse(read_all(dir / "bad_unknown.txt"));
    REQUIRE_FALSE(bad);
    CHECK(bad.error().message == "unknown record 'foo'");
}
