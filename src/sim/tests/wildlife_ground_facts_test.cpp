#include <algorithm>
#include <array>
#include <cstdlib>
#include <sstream>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/sha256.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/wildlife_ground_facts.hpp>
#include <anno/test/game_dir.hpp>
#include <anno/test/wildlife_ground_facts_fixture.hpp>

namespace load = anno::sim_load;
namespace sim = anno::sim;
namespace fmt = anno::formats;
namespace gf = anno::test::ground_facts_fixture;
namespace {
constexpr auto profile = load::GroundDescriptorProfile::GogAnlagen;
struct Rig {
    fmt::ByteBuffer bytes = gf::catalogue();
    std::vector<fmt::figuren::Entry> entries;
    std::vector<fmt::ByteBuffer> extra;
    std::shared_ptr<const sim::TypeRegistry> types;
    Rig() {
        const auto parsed = fmt::anlagen::AnlagenFile::parse(bytes);
        REQUIRE(parsed);
        entries = parsed.value().entries;
        const auto registry =
            sim::TypeRegistry::build(entries, sim::original_goods(), sim::SimConfig{});
        REQUIRE(registry);
        types = registry.value();
    }
    void record(std::string_view name, fmt::ByteBuffer value) {
        extra.push_back(std::move(value));
        for (auto& r : entries.at(0).records)
            if (r.name() == name)
                r.payload = extra.back();
    }
    template <class Change> void registry(Change change) {
        sim::TypeRegistry::Builder b;
        for (const auto& old : types->types()) {
            auto t = old;
            if (t.id.v == 104)
                change(t);
            b.add(std::move(t));
        }
        const auto built = b.build();
        REQUIRE(built);
        types = built.value();
    }
};
const std::vector<load::GroundObjectFact> expected{
    {0, load::GroundProperty::NullDescriptor, 0, 0}, {104, load::GroundProperty::Value, 225, 100},
    {105, load::GroundProperty::Value, 225, 0},      {16384, load::GroundProperty::Value, 225, 20},
    {16488, load::GroundProperty::Value, 225, 60},   {90001, load::GroundProperty::Value, 321, 90}};
template <class Change> void refuses(Change change, const std::string& why) {
    Rig r;
    const std::vector<std::uint32_t> words{104};
    const auto legal = load::bind_wildlife_ground_facts(profile, words, r.entries, *r.types);
    REQUIRE(legal);
    REQUIRE(legal.value() ==
            std::vector<load::GroundObjectFact>{{104, load::GroundProperty::Value, 225, 100}});
    change(r);
    const auto bytes = r.bytes;
    const auto digest = r.types->digest();
    std::vector<fmt::ByteBuffer> records;
    for (const auto& e : r.entries)
        records.push_back(e.to_bytes());
    const auto result = load::bind_wildlife_ground_facts(profile, words, r.entries, *r.types);
    REQUIRE_FALSE(result);
    CHECK(result.error().find(why) != std::string::npos);
    CHECK(r.bytes == bytes);
    CHECK(r.types->digest() == digest);
    for (std::size_t i = 0; i < records.size(); ++i)
        CHECK(r.entries[i].to_bytes() == records[i]);
}
} // namespace

TEST_CASE(
    "wildlife ground facts: exact source fields whole words sorted ownership and zero success") {
    std::vector<load::GroundObjectFact> owned;
    {
        Rig r;
        const std::vector<std::uint32_t> words{90001, 16488, 0, 104, 16384, 105, 104, 0};
        const auto before = r.bytes;
        const auto a = load::bind_wildlife_ground_facts(profile, words, r.entries, *r.types);
        REQUIRE(a);
        CHECK(a.value() == expected);
        CHECK(r.bytes == before);
        auto reversed = words;
        std::reverse(reversed.begin(), reversed.end());
        std::reverse(r.entries.begin(), r.entries.end());
        const auto b = load::bind_wildlife_ground_facts(profile, reversed, r.entries, *r.types);
        REQUIRE(b);
        CHECK(b.value() == expected);
        owned = a.value();
        std::fill(r.bytes.begin(), r.bytes.end(), 0);
        r.entries.clear();
        r.types.reset();
    }
    CHECK(owned == expected);
}

TEST_CASE("wildlife ground facts: whole-word aliases stay distinct and defined zero is not null") {
    Rig r;
    const std::vector<std::uint32_t> words{104, 16488, 0, 16384};
    const auto facts = load::bind_wildlife_ground_facts(profile, words, r.entries, *r.types);
    REQUIRE(facts);
    CHECK(facts.value() ==
          std::vector<load::GroundObjectFact>{{0, load::GroundProperty::NullDescriptor, 0, 0},
                                              {104, load::GroundProperty::Value, 225, 100},
                                              {16384, load::GroundProperty::Value, 225, 20},
                                              {16488, load::GroundProperty::Value, 225, 60}});
    auto zero = r.entries.front();
    zero.id = 0;
    r.entries.push_back(zero);
    const auto registry =
        sim::TypeRegistry::build(r.entries, sim::original_goods(), sim::SimConfig{});
    REQUIRE(registry);
    const auto defined = load::bind_wildlife_ground_facts(profile, std::vector<std::uint32_t>{0},
                                                          r.entries, *registry.value());
    REQUIRE(defined);
    CHECK(defined.value() ==
          std::vector<load::GroundObjectFact>{{0, load::GroundProperty::Value, 225, 100}});
}

TEST_CASE("wildlife ground facts: one precondition refusals preserve source and registry") {
    refuses([](Rig& r) { r.entries.erase(r.entries.begin()); }, "missing source entry");
    refuses([](Rig& r) { r.types = sim::TypeRegistry::empty(); }, "missing registry type");
    refuses([](Rig& r) { r.entries.push_back(r.entries.front()); }, "duplicate source entry");
    refuses([](Rig& r) { r.entries.front().records.erase(r.entries.front().records.begin()); },
            "unique HEAD");
    refuses([](Rig& r) { r.entries.front().records.push_back(r.entries.front().records.front()); },
            "unique HEAD");
    refuses([](Rig& r) { r.entries.front().records.push_back(r.entries.front().records.back()); },
            "unique HEAD");
    refuses([](Rig& r) { r.record("HEAD", fmt::ByteBuffer(57, 0)); }, "HEAD length");
    refuses([](Rig& r) { r.registry([](auto& t) { t.head_raw.pop_back(); }); }, "HEAD/registry");
    refuses(
        [](Rig& r) {
            auto head = r.types->find(sim::TypeId{104})->head_raw;
            head[0x19] = 101;
            r.record("HEAD", head);
        },
        "HEAD/registry");
    refuses([](Rig& r) { r.registry([](auto& t) { ++t.prodtype; }); }, "raw kind/registry");
    refuses([](Rig& r) { r.record("PRODTYPE", {'R', 'O', 'H', 'S', 'T', 'O', 'F', 'F'}); },
            "unterminated");
    refuses([](Rig& r) { r.record("PRODTYPE", {0}); }, "empty PRODTYPE");
    refuses([](Rig& r) { r.record("PRODTYPE", {'N', 'O', 'P', 'E', 0}); }, "unsupported PRODTYPE");
    Rig r;
    const std::vector<std::uint32_t> known{104}, unknown{104, 999999};
    REQUIRE(load::bind_wildlife_ground_facts(profile, known, r.entries, *r.types));
    const auto missing = load::bind_wildlife_ground_facts(profile, unknown, r.entries, *r.types);
    REQUIRE_FALSE(missing);
    CHECK(missing.error().find("999999: missing source") != std::string::npos);
    const auto unsupported = load::bind_wildlife_ground_facts(
        static_cast<load::GroundDescriptorProfile>(255), known, r.entries, *r.types);
    REQUIRE_FALSE(unsupported);
    CHECK(unsupported.error().find("unsupported descriptor profile") != std::string::npos);
    // A source-defined zero must not be mistaken for the absent-zero null case.
    auto zero = r.entries.front();
    zero.id = 0;
    r.entries.push_back(zero);
    const auto defined = load::bind_wildlife_ground_facts(profile, std::vector<std::uint32_t>{0},
                                                          r.entries, *r.types);
    REQUIRE_FALSE(defined);
    CHECK(defined.error().find("missing registry type") != std::string::npos);
}

TEST_CASE(
    "wildlife ground facts: present short HEAD zero extends and unused entries are irrelevant") {
    Rig r;
    auto head = r.types->find(sim::TypeId{104})->head_raw;
    head.resize(25); // Property is in the zero-filled suffix, not absent/failed.
    r.record("HEAD", head);
    r.registry([](auto& t) {
        std::fill(t.head_raw.begin() + 25, t.head_raw.end(), 0);
        t.flags_word = 0;
    });
    const auto zero = load::bind_wildlife_ground_facts(profile, std::vector<std::uint32_t>{104},
                                                       r.entries, *r.types);
    REQUIRE(zero);
    CHECK(zero.value() ==
          std::vector<load::GroundObjectFact>{{104, load::GroundProperty::Value, 225, 0}});
    fmt::figuren::Entry unrelated;
    unrelated.id = 7777;
    r.entries.push_back(unrelated);
    r.entries.push_back(unrelated); // Malformed and duplicate, but unreferenced.
    const auto same = load::bind_wildlife_ground_facts(profile, std::vector<std::uint32_t>{104},
                                                       r.entries, *r.types);
    REQUIRE(same);
    CHECK(same.value() == zero.value());
    CHECK(load::bind_wildlife_ground_facts(profile, {}, r.entries, *r.types).value().empty());
}

TEST_CASE("wildlife ground facts: bound source blends both branches retains zero and skips outside "
          "kind") {
    Rig r;
    const std::vector<std::uint32_t> objects{104, 16384, 105, 90001};
    const auto facts = load::bind_wildlife_ground_facts(profile, objects, r.entries, *r.types);
    REQUIRE(facts);
    const std::vector<std::uint16_t> gfx{0, 4, 8, 12};
    const std::vector<std::uint8_t> lookup{20, 100, 20, 20}, old{167, 183, 199, 215};
    std::array<std::uint8_t, 25> kernel{};
    kernel[12] = 6;
    const auto ground = load::prepare_wildlife_ground(
        {{17, -11, 4, 1}, gfx, objects, old, lookup, kernel, facts.value()});
    REQUIRE(ground);
    // b = 8,8,2,2; floor(6*b/12) = 4,4,1,1; keep high nibbles.
    CHECK(ground.value() == std::vector<std::uint8_t>{164, 180, 193, 209});
}

TEST_CASE("wildlife ground facts: original catalogue fields agree with independent source oracle") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping ground facts original catalogue: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto bytes = anno::test::read_file(*dir / "Anlagen.dat");
    REQUIRE(fmt::sha256_hex(bytes) ==
            "b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75");
    const auto parsed = fmt::anlagen::AnlagenFile::parse(bytes);
    REQUIRE(parsed);
    const auto registry = load::load_original_registry(*dir, load::FigureEntries::Required);
    REQUIRE(registry.types);
    std::vector<std::uint32_t> words;
    for (const auto& e : parsed.value().entries)
        words.push_back(e.id);
    const auto result =
        load::bind_wildlife_ground_facts(profile, words, parsed.value().entries, *registry.types);
    REQUIRE(result);
    REQUIRE(result.value().size() == 921);
    std::string serial;
    std::size_t blends = 0, nonzero = 0;
    for (const auto& f : result.value()) {
        CHECK(f.property == load::GroundProperty::Value);
        serial += std::to_string(f.object) + " " + std::to_string(f.kind) + " V " +
                  std::to_string(f.value) + "\n";
        blends += f.kind >= 225 && f.kind <= 256;
        nonzero += f.value != 0;
    }
    CHECK(blends == 291);
    CHECK(nonzero == 214);
    CHECK(fmt::sha256_hex(fmt::ByteBuffer(serial.begin(), serial.end())) ==
          "f8206d019710ce4baf4c0765dcfae1508deb3bad04978d1f1e5f73c90c7aaf30");
    if (const auto* path = std::getenv("ANNO_WILDLIFE_GROUND_FACTS_ORACLE"); path && *path) {
        const auto oracle = anno::test::read_file(path);
        CHECK(serial == std::string(oracle.begin(), oracle.end()));
        MESSAGE("independent catalogue: 921 fields, 291 blend kinds, 214 nonzero; unsupported "
                "profiles 0 in standard source");
    } else {
        MESSAGE("private source oracle not supplied; independent digest comparison only");
    }
}
