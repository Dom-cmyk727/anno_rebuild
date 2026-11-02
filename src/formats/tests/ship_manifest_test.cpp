#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/ship_manifest.hpp>

namespace sm = anno::formats::ship_manifest;
namespace fs = std::filesystem;

namespace {

fs::path fixtures() {
    return fs::path(ANNO_FIXTURES_DIR) / "map2_manifest";
}

std::vector<std::uint8_t> read_all(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in, path.string());
    return {std::istreambuf_iterator<char>(in), {}};
}

template <class R> std::string why(const R& r) {
    return r.ok() ? std::string() : r.error().message;
}

struct Case {
    std::string name;
    std::string engine; // "reject" or "ok:<hash>"
    bool baseline = false;
};

std::vector<Case> read_cases() {
    std::ifstream in(fixtures() / "cases.txt");
    REQUIRE(in);
    std::vector<Case> out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        // name | ref=... | engine=... | baseline=yes/no
        std::vector<std::string> cols;
        std::stringstream ss(line);
        std::string col;
        while (std::getline(ss, col, '|')) {
            const auto b = col.find_first_not_of(' ');
            const auto e = col.find_last_not_of(' ');
            cols.push_back(b == std::string::npos ? "" : col.substr(b, e - b + 1));
        }
        REQUIRE(cols.size() == 4);
        Case c;
        c.name = cols[0];
        REQUIRE(cols[2].rfind("engine=", 0) == 0);
        c.engine = cols[2].substr(7);
        c.baseline = cols[3] == "baseline=yes";
        out.push_back(std::move(c));
    }
    return out;
}

} // namespace

TEST_CASE("ship manifest: every cross-check case agrees with the Python reference") {
    const std::vector<Case> cases = read_cases();
    CHECK(cases.size() >= 80);
    for (const Case& c : cases) {
        CAPTURE(c.name);
        const auto bytes = read_all(fixtures() / "cases" / (c.name + ".json"));
        const auto parsed = sm::parse(bytes);
        if (c.engine == "reject") {
            CHECK_FALSE(parsed.ok());
            continue;
        }
        REQUIRE_MESSAGE(parsed.ok(), why(parsed));
        CHECK("ok:" + parsed.value().content_hash == c.engine);
        CHECK(sm::baseline_ok(parsed.value()) == c.baseline);
    }
}

TEST_CASE("ship manifest: the three unchanged candidates parse, hash as stated and are baseline") {
    const fs::path dir = fs::path(ANNO_REPO_DIR) / "data" / "maps" / "candidates" / "ship-start-v2";
    for (const char* name : {"lan5-ship-a.json", "lan5-ship-b.json", "lan5-ship-c.json"}) {
        CAPTURE(name);
        const auto parsed = sm::parse(read_all(dir / name));
        REQUIRE_MESSAGE(parsed.ok(), why(parsed));
        const sm::Manifest& m = parsed.value();
        // The file states a hash; parse() already compared it with the computed one.
        REQUIRE(m.stated_hash.has_value());
        CHECK(*m.stated_hash == m.content_hash);
        CHECK(sm::baseline_ok(m));
        CHECK(m.spawns.size() == 5);
        CHECK(m.edge_w == 1431);
        CHECK(m.loadout.holds.size() == 4);
        CHECK(m.loadout.holds[2].goods == "tools");
        CHECK(m.loadout.holds[3].goods == "tools");
    }
}

TEST_CASE("ship manifest: a schema-valid alternative loadout is valid but not the baseline") {
    const auto bytes = read_all(fixtures() / "cases" / "alternative_loadout.json");
    const auto parsed = sm::parse(bytes);
    REQUIRE(parsed.ok());
    CHECK(parsed.value().loadout.gold == 25000);
    CHECK_FALSE(sm::baseline_ok(parsed.value()));
}

TEST_CASE("ship manifest: the canonical bytes are ASCII, sorted and without content_hash") {
    const auto parsed = sm::parse(read_all(fixtures() / "cases" / "escaped_unicode_name.json"));
    REQUIRE(parsed.ok());
    const std::string bytes = sm::canonical_bytes(parsed.value());
    for (const char c : bytes) {
        CHECK(static_cast<unsigned char>(c) < 0x80);
    }
    CHECK(bytes.rfind("{\"base\":", 0) == 0);
    CHECK(bytes.find("content_hash") == std::string::npos);
    CHECK(bytes.find("caf\\u00e9 \\ud83d\\ude00") != std::string::npos);
}
