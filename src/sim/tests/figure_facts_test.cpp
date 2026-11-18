// Figure-type facts the founding veto reads, as the registry loader produces them from
// FigurenHeader entries (task MAP-2 package A2): the hull half-extent is a class-specific fact.
// Ship-class types (BEWOHNTYPE SHIP, 113) read HEAD f32 at byte 0x10; every finite value below 2^31
// tiles is kept SIGNED and exact (the shipped type 391 holds the uninitialised fill 0xCCCCCCCC =
// -107374176.0, the original does float arithmetic with it); a HEAD without the field, a NaN or
// an infinity, or a value beyond the bound leaves the hull ABSENT and the veto refuses when it
// needs it. Other classes have no hull. Expected values are derived by hand.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/figuren.hpp>
#include <anno/sim/archive.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/figure_veto.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/types.hpp>
#include <anno/test/game_dir.hpp>

namespace sim = anno::sim;
namespace fmt = anno::formats;

namespace {

/// The chunk views `payload` (a span): the caller's vector must outlive the chunk.
fmt::chunk::Chunk record(const char* name, const std::vector<std::uint8_t>& payload) {
    fmt::chunk::Chunk c;
    std::copy_n(name, std::strlen(name), c.name_field.begin());
    c.payload = payload;
    return c;
}

/// A figure entry with `kind_name`, a HEAD of `head_len` bytes whose f32 at 0x10 (when it fits) is
/// `bits`, and LOGIC with byte 0x25 = `range`.
std::shared_ptr<const sim::TypeRegistry> registry_of(const char* kind_name, std::size_t head_len,
                                                     std::uint32_t bits, std::string& error) {
    std::vector<std::uint8_t> head(head_len, 0);
    for (int i = 0; i < 4 && 0x10 + i < static_cast<int>(head_len); ++i) {
        head[static_cast<std::size_t>(0x10 + i)] = static_cast<std::uint8_t>(bits >> (8 * i));
    }
    std::vector<std::uint8_t> name(kind_name, kind_name + std::strlen(kind_name) + 1);
    std::vector<std::uint8_t> logic(44, 0);
    logic[0x25] = 128;
    fmt::figuren::Entry e;
    e.id = 1502;
    e.records = {record("HEAD", head), record("BEWOHNTYPE", name), record("LOGIC", logic)};
    auto built = sim::TypeRegistry::build({}, sim::original_goods(), {}, std::span(&e, 1));
    if (!built.ok()) {
        error = built.error();
        return nullptr;
    }
    return built.value();
}

std::optional<std::int64_t> hull_of(const char* kind, std::size_t head_len, float value) {
    std::string error;
    const auto reg = registry_of(kind, head_len, std::bit_cast<std::uint32_t>(value), error);
    REQUIRE_MESSAGE(reg != nullptr, error);
    return reg->find_figure_type(1502)->hull_half_88;
}

} // namespace

TEST_CASE(
    "figure facts: the ship hull half-extent is exact, signed, or absent, never a silent zero") {
    // Valid original data: 1.55 -> 396.8 -> 397; 2.35 -> 601.6 -> 602 (1501); 0 is a valid hull.
    CHECK(hull_of("SHIP", 20, 1.55F) == std::optional<std::int64_t>(397));
    CHECK(hull_of("SHIP", 20, 2.35F) == std::optional<std::int64_t>(602));
    CHECK(hull_of("SHIP", 20, 0.0F) == std::optional<std::int64_t>(0));
    // 1/512 tile = 0.5 raw rounds to 1 (llround away from zero); one raw unit is 1/256.
    CHECK(hull_of("SHIP", 20, 1.0F / 512.0F) == std::optional<std::int64_t>(1));
    // Negative values are kept exactly (an inverted box in the original's float arithmetic).
    CHECK(hull_of("SHIP", 20, -1.0F) == std::optional<std::int64_t>(-256));
    // The uninitialised fill of the shipped type 391: -107374176.0 tiles * 256.
    CHECK(hull_of("SHIP", 20, std::bit_cast<float>(0xCCCCCCCCU)) ==
          std::optional<std::int64_t>(-27487789056LL));
    // Large but inside the bound: 65536 tiles.
    CHECK(hull_of("SHIP", 20, 65536.0F) == std::optional<std::int64_t>(16777216));
    // Absent: the HEAD has no such field (8 and 16 bytes), NaN, infinities, beyond 2^31 tiles.
    CHECK_FALSE(hull_of("SHIP", 8, 1.55F).has_value());
    CHECK_FALSE(hull_of("SHIP", 16, 1.55F).has_value());
    CHECK_FALSE(hull_of("SHIP", 20, std::numeric_limits<float>::quiet_NaN()).has_value());
    CHECK_FALSE(hull_of("SHIP", 20, std::numeric_limits<float>::infinity()).has_value());
    CHECK_FALSE(hull_of("SHIP", 20, -std::numeric_limits<float>::infinity()).has_value());
    CHECK_FALSE(hull_of("SHIP", 20, 4294967296.0F).has_value()); // 2^32 tiles
    // Just inside the bound (2^30 tiles) is kept.
    CHECK(hull_of("SHIP", 20, 1073741824.0F) == std::optional<std::int64_t>(1073741824LL * 256));
    // A class without a hull never reads the field, whatever is there: absent, load succeeds.
    CHECK_FALSE(hull_of("ERKUNDER", 20, std::numeric_limits<float>::quiet_NaN()).has_value());
    CHECK_FALSE(hull_of("ERKUNDER", 20, 1.55F).has_value());
    CHECK_FALSE(hull_of("ESEL", 16, 0.0F).has_value());
}

TEST_CASE("figure facts: the veto's behaviour for present, negative, zero and absent hulls") {
    // Footprint [50,54) x [30,34); an own ship at (55.5, 31.5) is outside the rectangle.
    const auto verdict = [](std::optional<std::int64_t> hull, std::int32_t x_raw) {
        sim::veto::NearbyFigure f;
        f.registration = sim::veto::Registration::SectorCell;
        f.kind = 113;
        f.pos = sim::FixedPos{x_raw, 31 * 256 + 128};
        f.owner = {sim::veto::Fact::True, sim::PlayerId{0}};
        f.hull_half_88 = hull;
        return sim::veto::nearby_figures(sim::PlayerId{0}, {50, 30}, 4, 4,
                                         std::span<const sim::veto::NearbyFigure>(&f, 1))
            .verdict;
    };
    const std::int32_t outside = 55 * 256 + 128;
    CHECK(verdict(397, outside) == sim::veto::Verdict::Vetoed);     // 53.95 -> 53 < 54: overlaps
    CHECK(verdict(0, outside) == sim::veto::Verdict::Clear);        // a zero hull: point only
    CHECK(verdict(-7 * 256, outside) == sim::veto::Verdict::Clear); // inverted box
    CHECK(verdict(-27487789056LL, outside) == sim::veto::Verdict::Clear); // the type-391 fill
    CHECK(verdict(std::nullopt, outside) == sim::veto::Verdict::MissingFact);
    // The point test needs no hull: inside the rectangle an absent hull still vetoes.
    CHECK(verdict(std::nullopt, 53 * 256 + 128) == sim::veto::Verdict::Vetoed);
}

namespace {

sim::FigureType ship_type(std::uint32_t id, std::optional<std::int64_t> hull) {
    sim::FigureType t;
    t.id = id;
    t.bewohntype = 113;
    t.range_88 = 16 * 256;
    t.hull_half_88 = hull;
    return t;
}

/// Seals a registry through the public Builder with one ship type of the given hull.
anno::formats::Result<std::shared_ptr<const sim::TypeRegistry>, std::string>
seal_with(std::optional<std::int64_t> hull) {
    sim::TypeRegistry::Builder b;
    b.figure_type(ship_type(1502, hull));
    return b.build();
}

sim::veto::Verdict veto_with(std::int64_t hull, std::int32_t x, std::int32_t y) {
    sim::veto::NearbyFigure f;
    f.registration = sim::veto::Registration::SectorCell;
    f.kind = 113;
    f.pos = sim::FixedPos{x, y};
    f.owner = {sim::veto::Fact::True, sim::PlayerId{0}};
    f.hull_half_88 = hull;
    return sim::veto::nearby_figures(sim::PlayerId{0}, {50, 30}, 4, 4,
                                     std::span<const sim::veto::NearbyFigure>(&f, 1))
        .verdict;
}

} // namespace

TEST_CASE("figure facts: the builder enforces the same hull domain as the file decoder") {
    const std::int64_t k = sim::kMaxHullHalf88;
    REQUIRE(k == (std::int64_t{1} << 39));
    // Inside the domain (both edges, zero, ordinary, the shipped negative): accepted, exact.
    for (const std::int64_t ok :
         {std::int64_t{0}, std::int64_t{397}, k - 1, -(k - 1), std::int64_t{-27487789056LL}}) {
        const auto r = seal_with(ok);
        REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
        CHECK(r.value()->find_figure_type(1502)->hull_half_88 == std::optional<std::int64_t>(ok));
    }
    // Absent stays absent.
    {
        const auto r = seal_with(std::nullopt);
        REQUIRE(r.ok());
        CHECK_FALSE(r.value()->find_figure_type(1502)->hull_half_88.has_value());
    }
    // Outside it: a specific refusal naming the type and the value, never a registry.
    for (const std::int64_t bad : {k, -k, k + 1, -k - 1, std::numeric_limits<std::int64_t>::max(),
                                   std::numeric_limits<std::int64_t>::min()}) {
        const auto r = seal_with(bad);
        REQUIRE_FALSE(r.ok());
        CHECK(r.error().find("figure type 1502 hull half-extent " + std::to_string(bad)) !=
              std::string::npos);
        CHECK(r.error().find("outside the supported domain") != std::string::npos);
    }
    // The decoder's bound is the same one: just under 2^31 tiles is kept, 2^31 tiles is absent.
    CHECK(hull_of("SHIP", 20, 1073741824.0F).has_value());
    CHECK_FALSE(hull_of("SHIP", 20, 2147483648.0F).has_value());
}

TEST_CASE("figure facts: the public veto is safe for any int64 hull, with the exact decision") {
    const std::int32_t outside = 55 * 256 + 128;
    const std::int32_t y = 31 * 256 + 128;
    constexpr auto big = std::numeric_limits<std::int64_t>::max();
    constexpr auto small = std::numeric_limits<std::int64_t>::min();
    // A positive hull this large covers every rectangle; a negative one is an inverted box.
    for (const std::int64_t h : {big, big - 1, std::int64_t{1} << 62}) {
        CHECK(veto_with(h, outside, y) == sim::veto::Verdict::Vetoed);
        // A figure far outside the query rectangle is not a candidate whatever its hull.
        CHECK(veto_with(h, std::numeric_limits<std::int32_t>::max(),
                        std::numeric_limits<std::int32_t>::min()) == sim::veto::Verdict::Clear);
    }
    for (const std::int64_t h : {small, small + 1, -(std::int64_t{1} << 62)}) {
        CHECK(veto_with(h, outside, y) == sim::veto::Verdict::Clear);
        CHECK(veto_with(h, std::numeric_limits<std::int32_t>::min(),
                        std::numeric_limits<std::int32_t>::max()) == sim::veto::Verdict::Clear);
    }
    // Controls at the edge of the supported domain give the same answers as the extremes.
    CHECK(veto_with(sim::kMaxHullHalf88 - 1, outside, y) == sim::veto::Verdict::Vetoed);
    CHECK(veto_with(-(sim::kMaxHullHalf88 - 1), outside, y) == sim::veto::Verdict::Clear);
}

namespace {

struct DigestCase {
    std::string name;
    std::uint64_t want = 0;
    std::vector<sim::FigureType> figures;
};

/// Reads fixtures/synthetic/sim/figure_digest_vectors.txt, generated by the independent
/// tools/figure_digest_ref.py (checked by `figure_digest_ref.py --check`).
std::vector<DigestCase> read_vectors() {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" /
                     "figure_digest_vectors.txt");
    REQUIRE(in.good());
    std::vector<DigestCase> out;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tag;
        if (!(ls >> tag) || tag[0] == '#') {
            continue;
        }
        if (tag == "case") {
            DigestCase c;
            std::string hex;
            ls >> c.name >> hex;
            c.want = std::stoull(hex, nullptr, 16);
            out.push_back(std::move(c));
        } else if (tag == "fig") {
            REQUIRE(!out.empty());
            sim::FigureType f;
            std::string hull;
            ls >> f.id >> f.speed_q24 >> f.capacity_88 >> f.layer >> f.load_ticks >> f.bewohntype >>
                f.range_88 >> hull;
            if (hull != "-") {
                f.hull_half_88 = std::stoll(hull);
            }
            std::string tail_tag;
            while (ls >> tail_tag) {
                if (tail_tag == "nav") {
                    sim::NavFacts n;
                    ls >> n.speed_q32 >> n.turn_step_q32 >> n.max_hp >> n.hold_count_a >>
                        n.hold_count_b >> n.nav_class >> n.stamp_radius_88;
                    f.nav = n;
                } else if (tail_tag == "ware") {
                    ls >> f.ware; // stream 135
                } else {
                    REQUIRE(tail_tag == "route"); // stream 126
                    ls >> f.route;
                }
            }
            out.back().figures.push_back(f);
        }
    }
    return out;
}

} // namespace

TEST_CASE("figure digest: the registry identity of figure facts equals the independent reference") {
    const std::vector<DigestCase> cases = read_vectors();
    REQUIRE(cases.size() == 27); // tools/figure_digest_ref.py CASES (two route cases, stream 126;
                                 // two ware cases, stream 135)
    std::vector<std::uint64_t> seen;
    for (const DigestCase& c : cases) {
        INFO("case " << c.name);
        const std::uint64_t got = sim::TypeRegistry::digest_of({}, {}, {}, c.figures, {});
        CHECK(got == c.want);
        seen.push_back(got);
    }
    // Every changed field changes the identity: all vectors are pairwise different.
    std::sort(seen.begin(), seen.end());
    CHECK(std::adjacent_find(seen.begin(), seen.end()) == seen.end());
}

TEST_CASE("figure digest: the documented order matters to the identity") {
    const std::vector<DigestCase> cases = read_vectors();
    const auto it = std::find_if(cases.begin(), cases.end(),
                                 [](const DigestCase& c) { return c.name == "two_ascending"; });
    REQUIRE(it != cases.end());
    std::vector<sim::FigureType> reversed(it->figures.rbegin(), it->figures.rend());
    CHECK(sim::TypeRegistry::digest_of({}, {}, {}, it->figures, {}) == it->want);
    CHECK(sim::TypeRegistry::digest_of({}, {}, {}, reversed, {}) != it->want);
}

TEST_CASE("figure digest: the sealed registry orders figures by ascending id, whatever the input") {
    const std::vector<DigestCase> cases = read_vectors();
    const auto it = std::find_if(cases.begin(), cases.end(),
                                 [](const DigestCase& c) { return c.name == "three_ascending"; });
    REQUIRE(it != cases.end());
    REQUIRE(it->figures.size() == 3);
    const std::vector<std::vector<std::size_t>> orders = {{0, 1, 2}, {2, 1, 0}, {1, 2, 0}};
    std::uint64_t first = 0;
    for (std::size_t k = 0; k < orders.size(); ++k) {
        sim::TypeRegistry::Builder b;
        for (const std::size_t i : orders[k]) {
            b.figure_type(it->figures[i]);
        }
        const auto r = b.build();
        REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
        const auto figs = r.value()->figure_types();
        REQUIRE(figs.size() == 3);
        CHECK((figs[0].id < figs[1].id && figs[1].id < figs[2].id));
        if (k == 0) {
            first = r.value()->digest();
        }
        CHECK(r.value()->digest() == first);
    }
}

TEST_CASE("ore objects: the discovery-set wire bytes, derived by hand") {
    // OreObject{type 1000, x -3, y 70000, amount 0x1234, discovered {2, 4}}, little endian:
    //   u32 1000 = E8 03 00 00 | i32 -3 = FD FF FF FF | i32 70000 = 0x11170 = 70 11 01 00 |
    //   u32 0x1234 = 34 12 00 00 | u64 count 2 | u32 2 | u32 4.
    sim::OreObject o;
    o.type = 1000;
    o.x = -3;
    o.y = 70000;
    o.amount = 0x1234;
    o.discovered = {sim::PlayerId{2}, sim::PlayerId{4}};
    const std::vector<std::uint8_t> want = {0xE8, 0x03, 0x00, 0x00, 0xFD, 0xFF, 0xFF, 0xFF,
                                            0x70, 0x11, 0x01, 0x00, 0x34, 0x12, 0x00, 0x00,
                                            0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00};
    sim::WriteArchive w;
    fields(w, std::as_const(o));
    CHECK(w.data() == want);
    // It reads back, and re-writes the same bytes.
    sim::ReadArchive r(w.data());
    sim::OreObject back;
    fields(r, back);
    CHECK_FALSE(r.failed());
    CHECK(back == o);
    // The island's list of them: u64 count first (two objects: this one and an empty-set one).
    sim::OreObject empty;
    empty.type = 1;
    std::vector<sim::OreObject> list = {o, empty};
    sim::WriteArchive lw;
    std::uint64_t n = list.size();
    lw.count(n);
    for (auto& x : list) {
        fields(lw, x);
    }
    std::vector<std::uint8_t> want_list = {2, 0, 0, 0, 0, 0, 0, 0};
    want_list.insert(want_list.end(), want.begin(), want.end());
    // u32 1 | i32 0 | i32 0 | u32 0 | u64 0 = 24 bytes.
    std::vector<std::uint8_t> empty_bytes(24, 0);
    empty_bytes[0] = 1;
    want_list.insert(want_list.end(), empty_bytes.begin(), empty_bytes.end());
    CHECK(lw.data() == want_list);
}

// An eleventh FigureType member must reach the digest, the reference and its vectors.
static_assert(sim::field_count<sim::FigureType>() == 11,
              "a FigureType member needs a line in digest_registry, figure_digest_ref.py and the "
              "vectors");

namespace {
struct BewohnExpectation {
    const char* name;
    std::uint32_t number;
};
// Independently transcribed from AnnoGame.dll 0x10024cb8..<0x10024fb8, SHA256
// ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc.
// Numeric indices include sparse holes; these are not compact vector ordinals.
constexpr BewohnExpectation expected_bewohn[] = {
    {"WUESTE", 0},         {"MAEHER", 1},      {"WEIDETIER", 2},      {"JAEGER", 3},
    {"FISCHER", 4},        {"STEINMETZ", 5},   {"MAEHERSPEZ", 6},     {"SCHIFFWARE", 7},
    {"TRAEGER", 17},       {"MARKTLER", 18},   {"DIEB", 19},          {"ZIVILIST", 33},
    {"HIRSCH", 49},        {"RAUBTIER", 50},   {"HASE", 51},          {"KROKODIL", 52},
    {"ESEL", 53},          {"WAL", 54},        {"FISCH", 65},         {"DELPHIN", 66},
    {"VOGEL", 81},         {"PELIKAN", 82},    {"ERKUNDER", 97},      {"TRAGTIER", 98},
    {"SCHWERT", 99},       {"BOGEN", 100},     {"ARMBRUST", 101},     {"LANZE", 102},
    {"MUSKETIER", 103},    {"KAVALARIE", 104}, {"GEWEHR", 105},       {"MEDICUS", 106},
    {"BEDIENUNG", 107},    {"KANONIER", 108},  {"MOERSER", 109},      {"BELAGERTURM", 110},
    {"KATAPULT", 111},     {"SHIP", 113},      {"VERKAEUFER", 129},   {"TAENZER", 130},
    {"ERZSYMBOL", 145},    {"SCHATZ", 146},    {"WALSYMBOL", 147},    {"PRODSYMBOL", 148},
    {"SEUCHE", 149},       {"FLAGGE", 150},    {"FIRE", 151},         {"VULKAN", 152},
    {"VULKANSYMBOL", 153}, {"SOUND", 154},     {"JOKE", 155},         {"PFEIL", 161},
    {"EXPLOSION", 162},    {"BLINK", 163},     {"WASSERSTRAHL", 164}, {"EISSCHOLLE", 177},
    {"WALFAENGER", 178},
};

std::shared_ptr<const sim::TypeRegistry> authored_bewohn(const char* name,
                                                         std::uint32_t id = 4000000000U) {
    std::vector<std::uint8_t> payload;
    auto u32 = [&](std::uint32_t v) {
        for (unsigned n = 0; n < 4; ++n)
            payload.push_back(static_cast<std::uint8_t>(v >> (8 * n)));
    };
    u32(0);
    u32(id); // ENTRY flag and asset id: original file layout, not class number.
    auto tagged = [&](const char* tag, const std::vector<std::uint8_t>& bytes) {
        const std::string key(tag);
        payload.insert(payload.end(), key.begin(), key.end());
        payload.resize(payload.size() + 12 - key.size(), 0);
        u32(static_cast<std::uint32_t>(bytes.size()));
        payload.insert(payload.end(), bytes.begin(), bytes.end());
    };
    tagged("HEAD", std::vector<std::uint8_t>(8));
    tagged("LOGIC", std::vector<std::uint8_t>(44));
    if (name != nullptr)
        tagged("BEWOHNTYPE", {name, name + std::strlen(name) + 1});
    auto parsed = fmt::figuren::Entry::parse(record("ENTRY", payload));
    REQUIRE_MESSAGE(parsed.ok(), (parsed ? std::string{} : parsed.error()));
    auto built =
        sim::TypeRegistry::build({}, sim::original_goods(), {}, std::span(&parsed.value(), 1));
    REQUIRE_MESSAGE(built.ok(), (built ? std::string{} : built.error()));
    return built.value();
}

const BewohnExpectation* expected_named(std::string_view name) {
    for (const auto& e : expected_bewohn)
        if (name == e.name)
            return &e;
    return nullptr;
}
} // namespace

TEST_CASE("figure identities: all 57 sparse names retain exact numbers through the public parser") {
    CHECK(std::size(expected_bewohn) == 57);
    for (const auto& e : expected_bewohn) {
        INFO("BEWOHNTYPE " << e.name);
        CHECK(sim::bewohntype_by_name(e.name) == std::optional<std::uint32_t>(e.number));
        const auto r = authored_bewohn(e.name);
        REQUIRE(r->find_figure_type(4000000000U) != nullptr);
        CHECK(r->find_figure_type(4000000000U)->bewohntype == e.number);
    }
    // Named zero is an engaged optional; unknown/empty/UNUSED are not that name.
    CHECK(sim::bewohntype_by_name("WUESTE").has_value());
    CHECK(sim::bewohntype_by_name("WUESTE").value() == 0);
    CHECK(sim::bewohntype_by_name("SCHIFFWARE") == std::optional<std::uint32_t>(7));
    CHECK(sim::bewohntype_by_name("TRAEGER") == std::optional<std::uint32_t>(17)); // gap 8..16
}

TEST_CASE(
    "figure identities: absent unknown empty and UNUSED preserve zero without becoming WUESTE") {
    for (const char* name : std::array<const char*, 6>{nullptr, "", "UNUSED", "UNKNOWN_CLASS",
                                                       "hirsch", "HIRSCH_EXTRA"}) {
        INFO("spelling " << (name == nullptr ? "<absent>" : name));
        if (name != nullptr)
            CHECK_FALSE(sim::bewohntype_by_name(name).has_value());
        const auto r = authored_bewohn(name);
        REQUIRE(r->find_figure_type(4000000000U) != nullptr);
        CHECK(r->find_figure_type(4000000000U)->bewohntype == 0);
    }
}

TEST_CASE("figure identities corpus: every original header entry agrees with the independent "
          "sparse table") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::ifstream in(*game / "FigurenHeader.dat", std::ios::binary);
    REQUIRE(in.good());
    const std::vector<std::uint8_t> data(std::istreambuf_iterator<char>(in), {});
    const auto parsed = fmt::figuren::FigurenHeader::parse(data);
    REQUIRE_MESSAGE(parsed.ok(), (parsed ? std::string{} : parsed.error()));
    auto built = sim::TypeRegistry::build({}, sim::original_goods(), {}, parsed.value().entries);
    REQUIRE_MESSAGE(built.ok(), (built ? std::string{} : built.error()));
    std::size_t known = 0, zero = 0, absent = 0, unknown = 0;
    std::map<std::string, std::size_t> names;
    for (const auto& e : parsed.value().entries) {
        const auto source = e.record_str("BEWOHNTYPE");
        if (!source) {
            ++absent;
            continue;
        }
        ++names[std::string(*source)];
        const auto* want = expected_named(*source);
        if (want == nullptr) {
            ++unknown;
            continue;
        }
        ++known;
        zero += want->number == 0 ? 1U : 0U;
        const auto* f = built.value()->find_figure_type(e.id);
        REQUIRE(f != nullptr);
        CHECK_MESSAGE(f->bewohntype == want->number, "figure " << e.id << " " << *source);
    }
    std::cout << "figure identities corpus: inspected=" << parsed.value().entries.size()
              << " known=" << known << " legitimate-zero=" << zero << " absent=" << absent
              << " unknown=" << unknown << " distinct=" << names.size() << '\n';
    CHECK(parsed.value().entries.size() == 368);
    CHECK(known == 368);
    CHECK(zero == 0);
    CHECK(absent == 0);
    CHECK(unknown == 0);
    CHECK(names.size() == 50);
}

TEST_CASE("figure identities: changed content refuses old native and replay registry identities") {
    const auto parsed = authored_bewohn("HIRSCH");
    const auto* real = parsed->find_figure_type(4000000000U);
    REQUIRE(real != nullptr);
    REQUIRE(real->bewohntype == 49);
    // Use the same registry construction path for both identities, including walking defaults.
    sim::TypeRegistry::Builder current_builder;
    current_builder.figure_type(*real);
    auto current = current_builder.build();
    REQUIRE(current.ok());
    const auto now = current.value();
    auto previous = *real;
    previous.bewohntype = 0; // the evidenced pre-correction fallback
    sim::TypeRegistry::Builder builder;
    builder.figure_type(previous);
    auto old = builder.build();
    REQUIRE(old.ok());
    CHECK(old.value()->digest() != now->digest());
    auto made = sim::World::create(sim::ScenarioSpec{}, old.value());
    REQUIRE(made.ok());
    auto before = std::move(made).value();
    const auto bytes = before.save();
    REQUIRE(sim::World::load(bytes, old.value()).ok()); // legal control before counterexample
    auto native = sim::World::load(bytes, now);
    REQUIRE_FALSE(native.ok());
    CHECK_MESSAGE(native.error().find("registry") != std::string::npos, native.error());
    auto recorder = sim::Recorder::start(before);
    REQUIRE(recorder.ok());
    recorder.value().checkpoint();
    auto log = sim::write_log(recorder.value().log());
    REQUIRE(log.ok());
    auto decoded = sim::read_log(log.value());
    REQUIRE(decoded.ok());
    auto same = sim::World::create(sim::ScenarioSpec{}, old.value());
    REQUIRE(same.ok());
    REQUIRE(sim::replay(same.value(), decoded.value()).ok());
    auto fresh = sim::World::create(sim::ScenarioSpec{}, now);
    REQUIRE(fresh.ok());
    auto replay = sim::replay(fresh.value(), decoded.value());
    REQUIRE_FALSE(replay.ok());
    CHECK(replay.error() == "replay refused: type registry digest differs from the log's");
}
