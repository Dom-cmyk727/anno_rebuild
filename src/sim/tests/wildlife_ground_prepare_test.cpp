#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>

#include <doctest/doctest.h>

#include <anno/sim_load/wildlife_ground_prepare.hpp>

namespace load = anno::sim_load;
namespace {
struct Rig {
    anno::sim::TileRect geometry{17, -11, 1, 1};
    std::vector<std::uint16_t> gfx{0};
    std::vector<std::uint32_t> objects{0};
    std::vector<std::uint8_t> ground{167}, lookup{20}, kernel = std::vector<std::uint8_t>(25);
    std::vector<load::GroundObjectFact> facts{{}};
    Rig() { kernel[12] = 12; }
    load::WildlifeGroundInput view() const {
        return {geometry, gfx, objects, ground, lookup, kernel, facts};
    }
    friend bool operator==(const Rig&, const Rig&) = default;
};
std::vector<std::string> split(const std::string& text, char delimiter) {
    std::istringstream in(text);
    std::vector<std::string> out;
    std::string part;
    while (std::getline(in, part, delimiter))
        out.push_back(part);
    return out;
}
template <class T> T number(const std::string& text) {
    std::int64_t n = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), n);
    REQUIRE(error == std::errc{});
    REQUIRE(end == text.data() + text.size());
    REQUIRE(n >= std::numeric_limits<T>::min());
    REQUIRE(n <= std::numeric_limits<T>::max());
    return static_cast<T>(n);
}
template <class T> std::vector<T> column(const std::string& text) {
    std::vector<T> out;
    if (text != "-")
        for (const auto& s : split(text, ','))
            out.push_back(number<T>(s));
    return out;
}
std::map<std::string, std::string> fields(const std::vector<std::string>& tokens,
                                          std::size_t from) {
    std::map<std::string, std::string> out;
    for (auto i = from; i < tokens.size(); ++i) {
        const auto p = split(tokens[i], '=');
        REQUIRE(p.size() == 2);
        REQUIRE(out.emplace(p[0], p[1]).second);
    }
    return out;
}
load::GroundProperty state(const std::string& s) {
    if (s == "N")
        return load::GroundProperty::NullDescriptor;
    if (s == "A")
        return load::GroundProperty::Absent;
    if (s == "F")
        return load::GroundProperty::Unsuccessful;
    REQUIRE(s == "V");
    return load::GroundProperty::Value;
}
template <class Change> void refusal(Change change, const std::string& diagnostic) {
    Rig legal;
    const auto ok = load::prepare_wildlife_ground(legal.view());
    REQUIRE(ok);
    CHECK(ok.value() == std::vector<std::uint8_t>{162});
    change(legal);
    const auto before = legal;
    const auto refused = load::prepare_wildlife_ground(legal.view());
    REQUIRE_FALSE(refused);
    CHECK(refused.error().find(diagnostic) != std::string::npos);
    CHECK(legal == before);
}
} // namespace

TEST_CASE("wildlife ground: hand-derived full-island vectors") {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" /
                     "wildlife_ground_vectors.txt");
    REQUIRE(in);
    std::string line;
    std::size_t count = 0;
    std::map<std::string, bool> names;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        const auto arrow = line.find("=>");
        REQUIRE(arrow != std::string::npos);
        const auto left = split(line.substr(0, arrow - 1), ' ');
        const auto right = split(line.substr(arrow + 3), ' ');
        REQUIRE(left.size() == 11);
        REQUIRE(left[0] == "ground");
        REQUIRE(names.emplace(left[1], true).second);
        INFO(left[1]);
        const auto p = fields(left, 2), expected = fields(right, 0);
        REQUIRE(p.size() == 9);
        REQUIRE(expected.size() == 1);
        Rig rig;
        const auto origin = column<std::int32_t>(p.at("origin"));
        REQUIRE(origin.size() == 2);
        rig.geometry = {origin[0], origin[1], number<std::uint32_t>(p.at("w")),
                        number<std::uint32_t>(p.at("h"))};
        rig.gfx = column<std::uint16_t>(p.at("gfx"));
        rig.objects = column<std::uint32_t>(p.at("objects"));
        rig.ground = column<std::uint8_t>(p.at("ground"));
        rig.lookup = column<std::uint8_t>(p.at("lookup"));
        rig.kernel = column<std::uint8_t>(p.at("kernel"));
        rig.facts.clear();
        for (const auto& row : split(p.at("facts"), ';')) {
            const auto parts = split(row, ',');
            REQUIRE(parts.size() == 4);
            rig.facts.push_back({number<std::uint32_t>(parts[0]), state(parts[1]),
                                 number<std::uint32_t>(parts[2]), number<std::uint8_t>(parts[3])});
        }
        const auto before = rig;
        const auto result = load::prepare_wildlife_ground(rig.view());
        REQUIRE(result);
        CHECK(result.value() == column<std::uint8_t>(expected.at("ground")));
        CHECK(rig == before);
        // Origin is not an accidental stride or an index into an outside plane.
        rig.geometry.x0 = -30;
        rig.geometry.y0 = 40;
        std::reverse(rig.facts.begin(), rig.facts.end());
        const auto translated = load::prepare_wildlife_ground(rig.view());
        REQUIRE(translated);
        CHECK(translated.value() == result.value());
        ++count;
    }
    CHECK(count >= 24);
    for (const auto* name : {"two_d", "s2_terms", "s3_centre", "s7_scratch", "s10_width", "state_N",
                             "state_A", "state_F", "state_V", "whole_word"})
        CHECK(names.contains(name));
}

TEST_CASE("wildlife ground: precise validation refuses without changing inputs") {
    refusal([](Rig& r) { r.geometry.w = 0; }, "positive geometry");
    refusal([](Rig& r) { r.geometry.h = 0; }, "positive geometry");
    refusal([](Rig& r) { r.gfx.clear(); }, "plane length");
    refusal([](Rig& r) { r.objects.clear(); }, "plane length");
    refusal([](Rig& r) { r.ground.clear(); }, "plane length");
    refusal([](Rig& r) { r.kernel.pop_back(); }, "kernel");
    refusal([](Rig& r) { r.lookup.clear(); }, "lookup");
    refusal([](Rig& r) { r.gfx[0] = 4; }, "lookup");
    refusal([](Rig& r) { r.facts.clear(); }, "missing object fact");
    refusal([](Rig& r) { r.objects[0] = 1; }, "missing object fact");
    refusal([](Rig& r) { r.facts.push_back(r.facts[0]); }, "duplicate");
    refusal([](Rig& r) { r.facts[0].kind = 225; }, "null descriptor");
    refusal([](Rig& r) { r.facts[0].value = 1; }, "null descriptor");
    refusal([](Rig& r) { r.facts[0] = {0, load::GroundProperty::Absent, 225, 1}; }, "absent");
    refusal([](Rig& r) { r.facts[0] = {0, load::GroundProperty::Unsuccessful, 225, 1}; },
            "unsuccessful");
    refusal([](Rig& r) { r.facts[0].property = static_cast<load::GroundProperty>(255); },
            "unknown property");
}

TEST_CASE("wildlife ground: large geometry checked without allocating a large map") {
    refusal([](Rig& r) { r.geometry = {std::numeric_limits<std::int32_t>::max(), 0, 2, 1}; },
            "tile coordinates");
    refusal([](Rig& r) { r.geometry = {0, std::numeric_limits<std::int32_t>::max(), 1, 2}; },
            "tile coordinates");
    refusal(
        [](Rig& r) {
            r.geometry = {std::numeric_limits<std::int32_t>::min(), 0,
                          std::numeric_limits<std::uint32_t>::max(), 1};
        },
        "plane length");
    refusal(
        [](Rig& r) {
            r.geometry = {std::numeric_limits<std::int32_t>::min(),
                          std::numeric_limits<std::int32_t>::min(),
                          std::numeric_limits<std::uint32_t>::max(),
                          std::numeric_limits<std::uint32_t>::max()};
        },
        "not representable");
    Rig edge;
    edge.geometry = {std::numeric_limits<std::int32_t>::max(),
                     std::numeric_limits<std::int32_t>::min(), 1, 1};
    const auto result = load::prepare_wildlife_ground(edge.view());
    REQUIRE(result);
    CHECK(result.value() == std::vector<std::uint8_t>{162});
}

TEST_CASE("wildlife ground: output owns storage after immutable inputs expire") {
    const auto result = [] {
        Rig r;
        const auto before = r;
        auto first = load::prepare_wildlife_ground(r.view());
        REQUIRE(first);
        CHECK(r == before);
        first.value()[0] = 99;
        CHECK(r == before);
        auto second = load::prepare_wildlife_ground(r.view());
        REQUIRE(second);
        CHECK(second.value() == std::vector<std::uint8_t>{162});
        r.kernel[12] = 6;
        CHECK(second.value() == std::vector<std::uint8_t>{162});
        return second;
    }();
    REQUIRE(result);
    CHECK(result.value() == std::vector<std::uint8_t>{162});
}
