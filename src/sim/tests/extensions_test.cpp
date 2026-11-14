// Seven SAVE-SECTIONS-1b proofs (spec: state.md §9.8); no test type enters anno_sim.
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/state_fields.hpp>

#include "island_helpers.hpp"
namespace sim = anno::sim;
namespace {
template <std::uint16_t Tag> struct TagOnly {
    static constexpr std::uint16_t tag = Tag;
};
struct SyntheticState {
    std::uint16_t small = 0;
    std::uint32_t large = 0;
    friend bool operator==(const SyntheticState&, const SyntheticState&) = default;
};
struct SyntheticExtension {
    using Payload = SyntheticState;
    static constexpr std::uint16_t tag = 0xf001;
    static constexpr const char* name = "synthetic_extension";
    static constexpr Payload default_value() { return {}; }
    template <class Context> static auto& state(Context& context) { return context.synthetic; }
    template <class A, class P> static void fields(A& a, P& payload) {
        a.u16(payload.small);
        a.u32(payload.large);
    }
    static std::string validate(const Payload& payload) {
        return payload.large <= 1000000 ? "" : "large exceeds 1000000";
    }
};
struct TestContext {
    SyntheticState synthetic;
    template <sim::StateSection Section>
    using List = std::conditional_t<Section == sim::StateSection::Figures,
                                    sim::TestExtensionList<SyntheticExtension>,
                                    sim::ExtensionContext::List<Section>>;
    friend bool operator==(const TestContext&, const TestContext&) = default;
};
TestContext present() {
    return {{0x1234, 0x89ab}};
}
std::vector<std::uint8_t> save(const sim::World& world, const TestContext& context) {
    return sim::StateIO::save_with_extensions(world, context);
}
auto load(const std::vector<std::uint8_t>& bytes, TestContext& context) {
    return sim::StateIO::load_with_extensions(bytes, sim::TypeRegistry::empty(), sim::Seams{},
                                              context);
}
std::uint64_t hash(const sim::World& world, const TestContext& context,
                   sim::HashTree* tree = nullptr) {
    sim::HashArchive archive({tree, 1, false});
    sim::StateIO::world(archive, world, context);
    return archive.finish();
}
std::vector<std::uint8_t> flat(const sim::World& world, const TestContext& context) {
    sim::WriteArchive archive;
    sim::StateIO::world(archive, world, context);
    return archive.take();
}
struct Range {
    std::size_t length_at, begin, end;
};
std::array<Range, 10> ranges(const std::vector<std::uint8_t>& bytes) {
    REQUIRE(bytes.at(20) == 0);
    std::size_t at = 21;
    std::array<Range, 10> out{};
    for (auto& range : out) {
        REQUIRE(at + 8 <= bytes.size());
        std::uint64_t length = 0;
        for (std::size_t i = 0; i < 8; ++i)
            length |= std::uint64_t{bytes[at + i]} << (8 * i);
        REQUIRE(length <= bytes.size() - at - 8);
        range = {at, at + 8, at + 8 + static_cast<std::size_t>(length)};
        at = range.end;
    }
    return out;
}
std::vector<std::uint8_t> flatten(const std::vector<std::uint8_t>& bytes) {
    std::vector<std::uint8_t> out;
    for (const auto& r : ranges(bytes))
        out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(r.begin),
                   bytes.begin() + static_cast<std::ptrdiff_t>(r.end));
    return out;
}
struct Entry {
    std::uint16_t tag = SyntheticExtension::tag;
    std::uint32_t length = 6;
    std::vector<std::uint8_t> payload{0x34, 0x12, 0xab, 0x89, 0, 0};
};
std::vector<std::uint8_t> altered(const std::vector<std::uint8_t>& good,
                                  const std::vector<Entry>& entries) {
    const auto r = ranges(good)[6];
    // version4 + default FigureRegistry base31, per §9.8. Replace just its extension list.
    REQUIRE(r.end - r.begin == 55);
    sim::WriteArchive list;
    list.count(entries.size());
    for (const auto& entry : entries) {
        list.u16(entry.tag);
        list.u32(entry.length);
        list.bytes(entry.payload);
    }
    const auto first = good.begin() + static_cast<std::ptrdiff_t>(r.begin + 35);
    std::vector<std::uint8_t> bad(good.begin(), first);
    bad.insert(bad.end(), list.data().begin(), list.data().end());
    bad.insert(bad.end(), good.begin() + static_cast<std::ptrdiff_t>(r.end), good.end());
    const std::size_t length = 35 + list.data().size();
    for (std::size_t i = 0; i < 8; ++i)
        bad[r.length_at + i] = static_cast<std::uint8_t>(length >> (8 * i));
    island_test::reseal(bad);
    REQUIRE(bad != good);
    return bad;
}
} // namespace

TEST_CASE("extensions round trip: native and flat context commits with the world and absent means "
          "default") {
    sim::World world;
    const auto context = present();
    const auto bytes = save(world, context);
    TestContext restored{{7, 9}};
    auto loaded = load(bytes, restored);
    REQUIRE(loaded.ok());
    CHECK(restored == context);
    CHECK(save(loaded.value(), restored) == bytes);
    CHECK(hash(loaded.value(), restored) == hash(world, context));
    auto raw = flat(world, context);
    sim::World target(7);
    TestContext read_context{{9, 12}};
    sim::ReadArchive archive(raw);
    sim::StateIO::world(archive, target, read_context);
    REQUIRE_FALSE(archive.failed());
    CHECK(archive.remaining() == 0);
    CHECK(read_context == context);
    CHECK(flat(target, read_context) == raw);
    // Loading an absent entry resets a previously present value, in both readers.
    auto absent = load(world.save(), restored);
    REQUIRE(absent.ok());
    CHECK(restored == TestContext{});
    raw = flat(world, TestContext{});
    sim::ReadArchive absent_reader(raw);
    sim::StateIO::world(absent_reader, target, read_context);
    REQUIRE_FALSE(absent_reader.failed());
    CHECK(read_context == TestContext{});
}

TEST_CASE("extensions default: registering an absent feature changes no byte or hash") {
    const sim::World world;
    const TestContext context;
    CHECK(save(world, context) == world.save());
    CHECK(hash(world, context) == world.state_hash());
    sim::WriteArchive production;
    sim::fields(production, world);
    CHECK(flat(world, context) == production.data());
}

TEST_CASE("extensions present: only figures moves and bytes and digest agree with the independent "
          "oracle") {
    const sim::World world;
    const auto context = present();
    sim::HashTree old_tree, new_tree;
    static_cast<void>(world.state_hash(old_tree, 1));
    CHECK(hash(world, context, &new_tree) != world.state_hash());
    REQUIRE(old_tree.root.children.size() == 10);
    REQUIRE(new_tree.root.children.size() == 10);
    for (std::size_t i = 0; i < 10; ++i) {
        INFO(i);
        CHECK((old_tree.root.children[i].hash == new_tree.root.children[i].hash) == (i != 6));
    }
    std::ifstream in(ANNO_EXTENSION_ORACLE_FILE);
    std::string name, hex, digest;
    REQUIRE(bool(in >> name >> hex >> digest));
    CHECK(name == "synthetic_figures");
    std::vector<std::uint8_t> expected;
    for (std::size_t i = 0; i < hex.size(); i += 2)
        expected.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    const auto bytes = save(world, context);
    const auto r = ranges(bytes)[6];
    CHECK(std::vector<std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(r.begin),
                                    bytes.begin() + static_cast<std::ptrdiff_t>(r.end)) ==
          expected);
    CHECK(new_tree.root.children[6].hash == std::stoull(digest, nullptr, 16));
    // Raw hashing, tracing and writing visit exactly the same primitive sequence.
    sim::HashArchive raw({nullptr, 0, true});
    sim::StateIO::world(raw, world, context);
    sim::StateHasher fnv;
    const auto flat_bytes = flat(world, context);
    fnv.bytes(flat_bytes);
    CHECK(raw.finish() == fnv.value());
    sim::TraceArchive trace;
    sim::StateIO::world(trace, world, context);
    CHECK(trace.bytes_written() == flat_bytes);
}

TEST_CASE("extensions tree: the figures child names the feature") {
    const sim::World world;
    sim::HashTree tree;
    static_cast<void>(hash(world, present(), &tree));
    const auto& children = tree.root.children[6].children;
    REQUIRE(std::any_of(children.begin(), children.end(),
                        [](const auto& child) { return child.name == SyntheticExtension::name; }));
}

TEST_CASE("extensions refusals: every malformed list leaves native and flat targets unchanged") {
    const sim::World world;
    const auto good = save(world, present());
    struct Row {
        const char* name;
        std::vector<Entry> entries;
        std::string message;
        std::uint16_t tag;
    };
    Entry smaller;
    smaller.tag = 0xf000;
    Entry short_payload;
    short_payload.length = 5;
    short_payload.payload.pop_back();
    Entry long_payload;
    long_payload.length = 7;
    long_payload.payload.push_back(0);
    Entry zero;
    zero.payload.assign(6, 0);
    Entry invalid;
    invalid.payload = {1, 0, 0x41, 0x42, 0x0f, 0}; // 1000001
    Entry oversized;
    oversized.length = 100000;
    Entry unknown;
    unknown.tag = 0xf002;
    const std::vector<Row> rows{
        {"out of order", {Entry{}, smaller}, "out of order", smaller.tag},
        {"duplicate", {Entry{}, Entry{}}, "out of order", SyntheticExtension::tag},
        {"one short", {short_payload}, "payload:", SyntheticExtension::tag},
        {"one long", {long_payload}, "unconsumed bytes", SyntheticExtension::tag},
        {"default", {zero}, "decodes to default", SyntheticExtension::tag},
        {"validation", {invalid}, "large exceeds 1000000", SyntheticExtension::tag},
        {"length bound",
         {oversized},
         "length exceeds the remaining input",
         SyntheticExtension::tag},
        {"unknown", {unknown}, "unknown extension tag", unknown.tag}};
    for (const auto& row : rows) {
        INFO(row.name);
        const auto bad = altered(good, row.entries);
        TestContext context{{8, 15}};
        const auto before = context;
        const auto result = load(bad, context);
        REQUIRE_FALSE(result.ok());
        CHECK(result.error().find(row.message) != std::string::npos);
        CHECK(result.error().find("section figures") != std::string::npos);
        CHECK(result.error().find(std::to_string(row.tag)) != std::string::npos);
        CHECK(context == before);
        sim::World target(7);
        target.advance(20);
        const auto saved = target.save();
        const auto raw = flatten(bad);
        sim::ReadArchive reader(raw);
        sim::StateIO::world(reader, target, context);
        REQUIRE(reader.failed());
        CHECK(reader.error().find(row.message) != std::string::npos);
        CHECK(context == before);
        CHECK(target.save() == saved);
    }
    // A failure after the extension has parsed must not commit its context either.
    auto trailing = good;
    trailing.insert(trailing.end() - 8, 0);
    island_test::reseal(trailing);
    TestContext context{{9, 17}};
    const auto before = context;
    REQUIRE_FALSE(load(trailing, context).ok());
    CHECK(context == before);
}

TEST_CASE("extensions production: a synthetic native is refused as an unknown figures tag") {
    const sim::World world;
    const auto result = sim::World::load(save(world, present()), sim::TypeRegistry::empty());
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().find("section figures: unknown extension tag 61441") != std::string::npos);
}

TEST_CASE(
    "extensions schema: production lists are empty sorted unique and exclude the test range") {
    static_assert(sim::valid_extension_list<false, TagOnly<1>, TagOnly<2>>());
    static_assert(!sim::valid_extension_list<false, TagOnly<2>, TagOnly<1>>());
    static_assert(!sim::valid_extension_list<false, TagOnly<0>>());
    static_assert(!sim::valid_extension_list<false, TagOnly<0xf000>>());
    static_assert(sim::kKnownExtensions.empty());
    static_assert(sim::valid_extension_list<true, SyntheticExtension>());
    static_assert(!sim::valid_extension_list<false, SyntheticExtension>());
    static_assert(!sim::valid_extension_list<true, SyntheticExtension, SyntheticExtension>());
    constexpr auto schema = sim::extension_schema<TestContext>();
    static_assert(schema.size() == 1);
    static_assert(schema[0].section == sim::StateSection::Figures);
    static_assert(schema[0].tag == 0xf001);
    static_assert(schema[0].name == "synthetic_extension");
    CHECK(sim::kStateHashVersion == 139);
    for (const auto& section : sim::kSectionVersions)
        CHECK(section.version == 1);
    CHECK(sim::kKnownExtensions.empty());
}
