// The type registry, the class table, the name tables and the goods table
// (docs/sim/state.md §4.4, §14.2, §14.5, Appendices A–C; stage S3). The first
// half needs no game files: entries are built from bytes. The last cases read
// Anlagen.dat and skip without ANNO1503_GAME_DIR.
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/test/game_dir.hpp>

namespace sim = anno::sim;
namespace fmt = anno::formats;

namespace {

using Buffer = std::vector<std::uint8_t>;

void put_u16(Buffer& b, std::size_t at, std::uint32_t v) {
    b[at] = static_cast<std::uint8_t>(v);
    b[at + 1] = static_cast<std::uint8_t>(v >> 8);
}

void put_f32(Buffer& b, std::size_t at, float v) {
    const auto bits = std::bit_cast<std::uint32_t>(v);
    for (std::size_t i = 0; i < 4; ++i) {
        b[at + i] = static_cast<std::uint8_t>(bits >> (8 * i));
    }
}

Buffer text(const std::string& s) {
    Buffer b(s.begin(), s.end());
    b.push_back(0);
    return b;
}

/// A HEAD record with recognisable values in every byte the type reader decodes
/// (docs/re/production.md §2.4, fire-and-ships.md §2.1): a 3 x 2 footprint,
/// costs 7 bricks / 11 wood / 13 tools / 17 marble, 300 gold, ...
Buffer head_bytes(std::size_t size = 52) {
    Buffer h(size, 0);
    auto set = [&](std::size_t at, std::uint32_t v) {
        if (at < h.size()) {
            h[at] = static_cast<std::uint8_t>(v);
        }
    };
    set(0, 3);    // width
    set(1, 2);    // height
    set(2, 0x44); // HEAD[2] (+0x74, no consumer): a neighbour of width and height
    set(3, 7);    // ZIEGEL (bricks)
    set(4, 11);   // HOLZ (wood)
    set(5, 13);   // WERKZEUG (tools)
    set(6, 17);   // MAMOR (marble)
    set(7, 0x55); // HEAD[7]: neighbour of the costs
    set(8, 300 & 0xff);
    set(9, 300 >> 8);
    set(0x0a, 5);    // rubble pattern, a u8
    set(0x0b, 0x33); // neighbours of the rubble byte: it must not be read as a u16 or u32
    set(0x0c, 0x44);
    set(0x10, 0x21); // stage count (production data), neighbour of the radius
    set(0x11, 9);    // action radius
    set(0x12, 0x22); // neighbour of the radius
    set(0x1c, 0x00);
    set(0x1d, 0x20); // flags word 0x2000 (coast), a u16
    set(0x1e, 0x66); // the bytes after the u16 must not leak into it
    set(0x2c, 0xc4);
    set(0x2d, 0x09); // max hp 2500, a u16
    set(0x2e, 0x77);
    set(0x30, 0x88); // neighbours of can burn
    set(0x31, 3);    // can burn, a u8
    set(0x32, 0x99);
    return h;
}

/// A PROD3 record (68 bytes) of the weaver's hut: 43 s per ton (rate 6), 1.9 t
/// of wool per t of cloth (need 11), 5 t of storage, one carrier.
Buffer weaver_prod3() {
    Buffer p(68, 0);
    put_u16(p, 0x00, 10); // running cost A
    put_u16(p, 0x02, 15); // running cost B
    p[0x04] = 5;
    put_f32(p, 0x08, 43.0F);
    put_f32(p, 0x10, 1.9F);
    p[0x28] = 1;
    p[0x29] = 1;
    return p;
}

struct EntrySpec {
    std::uint32_t id = 1;
    std::optional<Buffer> head = head_bytes();
    // A house: its class does not tick, so an entry needs no production data.
    std::optional<std::string> prodtype = "PIONIER";
    std::optional<std::string> forsch;
    std::optional<Buffer> ware, rohstoff, prod3;
};

/// Owns the bytes the parsed entries point into.
struct Entries {
    std::vector<Buffer> buffers;
    std::vector<fmt::figuren::Entry> entries;

    void add(const EntrySpec& spec) {
        Buffer payload;
        fmt::put_u32_le(payload, 0);
        fmt::put_u32_le(payload, spec.id);
        auto record = [&](const char* name, const Buffer& data) {
            fmt::chunk::Chunk c;
            c.name_field = *fmt::chunk::name_field(name);
            c.payload = data;
            REQUIRE(fmt::chunk::write_chunk(c, payload));
        };
        if (spec.head) {
            record("HEAD", *spec.head);
        }
        if (spec.prodtype) {
            record("PRODTYPE", text(*spec.prodtype));
        }
        if (spec.forsch) {
            record("FORSCHTYPE", text(*spec.forsch));
        }
        if (spec.prod3) {
            record("PROD3", *spec.prod3);
        }
        if (spec.ware) {
            record("WARE", *spec.ware);
        }
        if (spec.rohstoff) {
            record("ROHSTOFF", *spec.rohstoff);
        }
        Buffer file;
        fmt::chunk::Chunk entry;
        entry.name_field = *fmt::chunk::name_field("ENTRY");
        entry.payload = payload;
        REQUIRE(fmt::chunk::write_chunk(entry, file));
        buffers.push_back(std::move(file));
        auto chunks = fmt::chunk::parse_chunks(buffers.back());
        REQUIRE(chunks.has_value());
        REQUIRE(chunks->size() == 1);
        auto parsed = fmt::figuren::Entry::parse((*chunks)[0]);
        REQUIRE(parsed);
        entries.push_back(std::move(parsed).value());
    }

    fmt::Result<std::shared_ptr<const sim::TypeRegistry>, std::string>
    build(const sim::SimConfig& cfg = sim::SimConfig{}) const {
        return sim::TypeRegistry::build(entries, sim::original_goods(), cfg);
    }
};

Buffer names(std::initializer_list<const char*> list) {
    Buffer b;
    for (const char* s : list) {
        for (const char* p = s; *p != '\0'; ++p) {
            b.push_back(static_cast<std::uint8_t>(*p));
        }
        b.push_back(0);
    }
    return b;
}

/// The registry of one entry, or a failed REQUIRE.
std::shared_ptr<const sim::TypeRegistry> build_one(const EntrySpec& spec) {
    Entries e;
    e.add(spec);
    auto r = e.build();
    REQUIRE(r);
    return r.value();
}

} // namespace

TEST_CASE("class table spot checks") {
    using C = sim::RuntimeClass;
    // Appendix C, by PRODTYPE number.
    struct Range {
        std::uint32_t lo, hi;
        std::optional<C> cls;
    };
    const std::vector<Range> ranges = {{0, 0, std::nullopt},
                                       {1, 1, C::Workshop},
                                       {2, 2, C::Farm},
                                       {3, 3, C::Mine},
                                       {4, 4, C::Farm},
                                       {5, 5, C::Hunter},
                                       {6, 6, C::Farm},
                                       {7, 7, C::Shipyard},
                                       {8, 8, C::Farm},
                                       {9, 9, C::SmallFarm},
                                       {10, 10, C::Farm},
                                       {11, 11, C::Cannonworks},
                                       {12, 12, C::DualWorkshop},
                                       {13, 14, C::Quarry},
                                       {15, 15, C::Whaler},
                                       {16, 16, C::FurTrapper},
                                       {17, 17, C::WeaponsWorkshop},
                                       {18, 32, C::Plain},
                                       {33, 35, C::Market},
                                       {36, 36, C::MarketCombi},
                                       {37, 65, C::Plain},
                                       {66, 66, C::Inn},
                                       {67, 70, C::PublicService},
                                       {71, 71, C::Doctor},
                                       {72, 73, C::School},
                                       {74, 74, C::Castle},
                                       {75, 76, C::Plain},
                                       {77, 77, C::FireBrigade},
                                       {78, 79, C::PublicService},
                                       {80, 96, C::Plain},
                                       {97, 101, C::House},
                                       {102, 128, C::Plain},
                                       {129, 129, C::Tower},
                                       {130, 130, C::Cannonworks},
                                       {131, 131, C::CannonTower},
                                       {132, 160, C::Plain},
                                       {161, 161, C::MarketStall},
                                       {162, 192, C::Plain},
                                       {193, 416, std::nullopt}};
    std::uint32_t covered = 0;
    for (const Range& r : ranges) {
        for (std::uint32_t p = r.lo; p <= r.hi; ++p) {
            INFO("prodtype " << p);
            const auto got = sim::class_for_prodtype(p);
            REQUIRE(got.has_value() == r.cls.has_value());
            if (r.cls) {
                CHECK(*got == *r.cls);
            }
            ++covered;
        }
    }
    CHECK(covered == 417); // every number 0..416 exactly once
    // The names of the entries the spec mentions resolve to their classes.
    CHECK(*sim::class_for_prodtype(*sim::prodtype_by_name("UNIVERSAL")) == C::SmallFarm);
    CHECK(*sim::class_for_prodtype(*sim::prodtype_by_name("PELZJAEGER")) == C::FurTrapper);
    CHECK(*sim::class_for_prodtype(*sim::prodtype_by_name("FISCHER")) == C::Farm);
    CHECK(*sim::class_for_prodtype(*sim::prodtype_by_name("MARKTSTAND")) == C::MarketStall);
    CHECK(*sim::class_for_prodtype(*sim::prodtype_by_name("PIONIER")) == C::House);
    CHECK(*sim::class_for_prodtype(*sim::prodtype_by_name("ARISTOKRATEN")) == C::House);
    CHECK_FALSE(sim::class_for_prodtype(*sim::prodtype_by_name("STRASSE")).has_value());
    CHECK_FALSE(sim::class_for_prodtype(*sim::prodtype_by_name("UNUSED")).has_value());
    // The class of a number agrees with the list it goes to (Appendix C):
    // houses in the house list, markets in the market list, stalls in the stall
    // list, public buildings in the public list, towers in the military list.
    const sim::ListLayout layout = sim::ListLayout::original();
    for (std::uint32_t p = 1; p <= 192; ++p) {
        const C c = *sim::class_for_prodtype(p);
        const sim::ListId list = *layout.list_for(p);
        INFO("prodtype " << p);
        if (c == C::House) {
            CHECK(list == layout.roles.house_list);
        } else if (c == C::Market || c == C::MarketCombi) {
            CHECK(list == layout.roles.market_list);
        } else if (c == C::MarketStall) {
            CHECK(list == layout.roles.stall_list);
        } else if (c == C::Inn || c == C::PublicService || c == C::Doctor || c == C::School ||
                   c == C::Castle || c == C::FireBrigade) {
            CHECK(list == layout.roles.public_list);
        } else if (c == C::Tower || c == C::CannonTower) {
            CHECK(list == layout.roles.military_list);
        } else if (p <= 32) {
            CHECK(list == layout.roles.production_list);
        }
    }
}

TEST_CASE("class table payload tags") {
    const std::vector<sim::ClassInfo> table = sim::original_class_table();
    REQUIRE(table.size() == static_cast<std::size_t>(sim::RuntimeClass::Count_));
    CHECK(static_cast<std::size_t>(sim::RuntimeClass::Count_) == 25);
    // The save tags of Building::payload (docs/sim/state.md §9.3) are fixed.
    static_assert(static_cast<std::uint8_t>(sim::PayloadTag::Inert) == 0);
    static_assert(static_cast<std::uint8_t>(sim::PayloadTag::Workshop) == 1);
    static_assert(static_cast<std::uint8_t>(sim::PayloadTag::WeaponsWorkshop) == 2);
    static_assert(static_cast<std::uint8_t>(sim::PayloadTag::Farm) == 3);
    static_assert(static_cast<std::uint8_t>(sim::PayloadTag::SmallFarm) == 4);
    static_assert(static_cast<std::uint8_t>(sim::PayloadTag::Mine) == 5);
    using C = sim::RuntimeClass;
    using T = sim::TickKind;
    using P = sim::PayloadTag;
    struct Expect {
        C cls;
        T tick;
        P payload;
        std::uint32_t gate;
    };
    const Expect expect[] = {{C::Plain, T::None, P::Inert, 0},
                             {C::Workshop, T::Workshop, P::Workshop, 500},
                             {C::DualWorkshop, T::None, P::Inert, 500},
                             {C::WeaponsWorkshop, T::WeaponsWorkshop, P::WeaponsWorkshop, 500},
                             {C::Farm, T::Farm, P::Farm, 500},
                             {C::FurTrapper, T::FurTrapper, P::Farm, 500},
                             {C::Mine, T::Mine, P::Mine, 500},
                             {C::SmallFarm, T::SmallFarm, P::SmallFarm, 500},
                             {C::Quarry, T::None, P::Inert, 0},
                             {C::Hunter, T::None, P::Inert, 500},
                             {C::Whaler, T::None, P::Inert, 500},
                             {C::Shipyard, T::None, P::Inert, 500},
                             {C::Cannonworks, T::None, P::Inert, 500},
                             {C::Market, T::Market, P::Market, 500},
                             {C::MarketCombi, T::None, P::Inert, 500},
                             {C::MarketStall, T::None, P::Inert, 5000},
                             {C::Inn, T::None, P::Inert, 5000},
                             {C::PublicService, T::None, P::Inert, 500},
                             {C::Doctor, T::None, P::Inert, 500},
                             {C::School, T::None, P::Inert, 500},
                             {C::Castle, T::None, P::Inert, 0},
                             {C::FireBrigade, T::None, P::Inert, 500},
                             {C::House, T::None, P::Inert, 2500},
                             {C::Tower, T::None, P::Inert, 166},
                             {C::CannonTower, T::None, P::Inert, 166}};
    for (std::size_t i = 0; i < table.size(); ++i) {
        INFO("class " << i);
        CHECK(table[i].cls == expect[i].cls);
        CHECK(table[i].cls == static_cast<C>(i)); // indexed by class
        CHECK(table[i].tick == expect[i].tick);
        CHECK(table[i].payload == expect[i].payload);
        CHECK(table[i].gate_period == expect[i].gate);
    }
    // Every ported tick has its own payload alternative; every other class is inert.
    for (const sim::ClassInfo& c : table) {
        CHECK((c.tick == T::None) == (c.payload == P::Inert));
    }
    // The registry serves the same table.
    const auto registry = sim::TypeRegistry::empty();
    for (std::size_t i = 0; i < table.size(); ++i) {
        const auto& got = registry->class_info(static_cast<C>(i));
        CHECK(got.payload == table[i].payload);
        CHECK(got.gate_period == table[i].gate_period);
    }
    // A class outside the enum reads the first entry instead of out of bounds.
    CHECK(registry->class_info(static_cast<C>(200)).cls == C::Plain);
}

TEST_CASE("prodtype and research name tables") {
    // PRODTYPE: blocks of 32 (D AnnoGame 0x100138e0).
    CHECK(*sim::prodtype_by_name("HANDWERK") == 1);
    CHECK(*sim::prodtype_by_name("PLANTAGE") == 2);
    CHECK(*sim::prodtype_by_name("MINE") == 3);
    CHECK(*sim::prodtype_by_name("UNIVERSAL") == 9);
    CHECK(*sim::prodtype_by_name("PELZJAEGER") == 16);
    CHECK(*sim::prodtype_by_name("HANDWERKWAFFEN") == 17);
    CHECK(*sim::prodtype_by_name("HAUPT") == 33);
    CHECK(*sim::prodtype_by_name("KONTOR") == 35);
    CHECK(*sim::prodtype_by_name("MARKTKOMBI") == 36);
    CHECK(*sim::prodtype_by_name("BRUNNEN") == 65);
    CHECK(*sim::prodtype_by_name("PAVILLION") == 79);
    CHECK(*sim::prodtype_by_name("PIONIER") == 97);
    CHECK(*sim::prodtype_by_name("ARISTOKRATEN") == 101);
    CHECK(*sim::prodtype_by_name("WACHTURM") == 129);
    CHECK(*sim::prodtype_by_name("MILITAR") == 130);
    CHECK(*sim::prodtype_by_name("KANONENTURM") == 131);
    CHECK(*sim::prodtype_by_name("MARKTSTAND") == 161);
    CHECK(*sim::prodtype_by_name("STRASSE") == 193);
    CHECK(*sim::prodtype_by_name("ZAUN") == 201);
    CHECK(*sim::prodtype_by_name("ROHSTOFF") == 225);
    CHECK(*sim::prodtype_by_name("ROHSTWACHS") == 226);
    CHECK(*sim::prodtype_by_name("ROHSTDUERR") == 227);
    CHECK(*sim::prodtype_by_name("MAUER") == 257);
    CHECK(*sim::prodtype_by_name("TOR") == 259);
    CHECK(*sim::prodtype_by_name("DENKMAL") == 289);
    CHECK(*sim::prodtype_by_name("ZIERDE") == 291);
    CHECK(*sim::prodtype_by_name("FELSEN") == 321);
    CHECK(*sim::prodtype_by_name("TIERHOEHLE") == 325);
    CHECK(*sim::prodtype_by_name("FLUSS") == 353);
    CHECK(*sim::prodtype_by_name("WASSERFALL") == 355);
    CHECK(*sim::prodtype_by_name("RUINE") == 385);
    CHECK(*sim::prodtype_by_name("UNUSED") == 0);
    CHECK(*sim::prodtype_by_name("") == 0);
    CHECK_FALSE(sim::prodtype_by_name("handwerk").has_value());
    CHECK_FALSE(sim::prodtype_by_name("NOPE").has_value());
    // Number -> name and back for every named number.
    std::size_t named = 0;
    for (std::uint32_t n = 1; n <= 416; ++n) {
        const std::string_view name = sim::prodtype_name(n);
        if (!name.empty()) {
            ++named;
            CHECK(*sim::prodtype_by_name(name) == n);
        }
    }
    CHECK(named == 72);
    CHECK(sim::prodtype_name(0).empty());
    CHECK(sim::prodtype_name(18).empty());
    CHECK(sim::prodtype_name(9) == "UNIVERSAL");
    // FORSCHTYPE ids (Appendix B, D AnnoGame 0x10013cf0).
    const std::pair<const char*, std::uint32_t> research[] = {{"WAFFEN_FERTIGUNG_01", 1},
                                                              {"KATAPULT", 2},
                                                              {"WAFFEN_FERTIGUNG_02", 3},
                                                              {"BOGNER", 4},
                                                              {"BELAGERUNGSMASCHINE", 5},
                                                              {"KAVALLERIE", 6},
                                                              {"GEHAERTETE_SPITZEN", 7},
                                                              {"GEFIEDERTER_SCHAFT", 8},
                                                              {"ARMBRUST", 9},
                                                              {"ARMBRUST_REICHWEITE", 10},
                                                              {"BRANDPFEILE", 11},
                                                              {"MUSKETE", 12},
                                                              {"RADSCHLOSS", 13},
                                                              {"RUESTUNG_INFAN_01", 17},
                                                              {"RUESTUNG_KAVAL_01", 18},
                                                              {"RUESTUNG_INFAN_02", 19},
                                                              {"RUESTUNG_KAVAL_02", 20},
                                                              {"KANONE", 21},
                                                              {"KANONE_GENORMT_KALIBER", 22},
                                                              {"KANONE_GENORMT_SPRENG", 23},
                                                              {"KANONE_MOERSER", 24},
                                                              {"KANONE_MOERSER_PULVER", 25},
                                                              {"KANONE_MOERSER_BRAND", 26},
                                                              {"MITTLERES_HANDEL", 33},
                                                              {"MITTLERES_KRIEG", 34},
                                                              {"GROSSES_HANDEL", 35},
                                                              {"GROSSES_KRIEG", 36},
                                                              {"VERSTAERKTER_RUMPF", 37},
                                                              {"KANONE_SHIP", 38},
                                                              {"BRUNNEN", 49},
                                                              {"GR_WEBER", 50},
                                                              {"FEUERWEHR", 51},
                                                              {"TIEFER_BRUNNEN", 52},
                                                              {"STEIN_BRUECKE", 53},
                                                              {"MEDICUS", 54},
                                                              {"BIBLIOTHEK", 55},
                                                              {"SCHNELLE_HEILUNG", 56},
                                                              {"TIEFE_ERZMINE", 57},
                                                              {"GERICHT", 58}};
    for (const auto& [name, id] : research) {
        INFO(name);
        REQUIRE(sim::research_by_name(name).has_value());
        CHECK(*sim::research_by_name(name) == id);
    }
    CHECK(*sim::research_by_name("UNUSED") == 0);
    CHECK(*sim::research_by_name("") == 0);
    CHECK_FALSE(sim::research_by_name("NOPE").has_value());
    CHECK_FALSE(sim::research_by_name("tiefe_erzmine").has_value());
}

TEST_CASE("prodtype names match AnnoGame") {
    // Every named number of the PRODTYPE table (D AnnoGame 0x100138e0, read from
    // the pointer tables 2026-10-19); the 72 names and no other.
    // clang-format off
    const std::pair<std::uint32_t, const char*> table[] = {
        {1, "HANDWERK"}, {2, "PLANTAGE"}, {3, "MINE"}, {4, "WEIDETIER"}, {5, "JAGDHAUS"},
        {6, "FISCHER"}, {7, "WERFT"}, {8, "STEINMETZ"}, {9, "UNIVERSAL"}, {10, "SCHAFFARM"},
        {11, "KANONENBAU"}, {12, "HANDWERKDUAL"}, {13, "STEINBRUCH"}, {14, "EISLOCH"},
        {15, "WALFAENGER"}, {16, "PELZJAEGER"}, {17, "HANDWERKWAFFEN"}, {33, "HAUPT"},
        {34, "MARKT"}, {35, "KONTOR"}, {36, "MARKTKOMBI"}, {65, "BRUNNEN"}, {66, "WIRT"},
        {67, "KAPELLE"}, {68, "KIRCHE"}, {69, "BADEHAUS"}, {70, "THEATER"}, {71, "ARZT"},
        {72, "SCHULE"}, {73, "HOCHSCHULE"}, {74, "SCHLOSS"}, {75, "GALGEN"}, {76, "GERICHT"},
        {77, "FEUERWEHR"}, {78, "BIBLIOTHEK"}, {79, "PAVILLION"}, {97, "PIONIER"},
        {98, "SIEDLER"}, {99, "BUERGER"}, {100, "KAUFLEUTE"}, {101, "ARISTOKRATEN"},
        {129, "WACHTURM"}, {130, "MILITAR"}, {131, "KANONENTURM"}, {161, "MARKTSTAND"},
        {193, "STRASSE"}, {194, "RAMPE"}, {195, "RAMPELOW"}, {196, "RAMPEHIGH"}, {197, "PIER"},
        {198, "BRUECKE"}, {199, "PLATZ"}, {200, "GAUKLERPLATZ"}, {201, "ZAUN"},
        {225, "ROHSTOFF"}, {226, "ROHSTWACHS"}, {227, "ROHSTDUERR"}, {257, "MAUER"},
        {258, "MAUERUP"}, {259, "TOR"}, {289, "DENKMAL"}, {290, "TRIUMPH"}, {291, "ZIERDE"},
        {321, "FELSEN"}, {322, "KLIPPE"}, {323, "STEIN"}, {324, "ROHSTSPEZ"},
        {325, "TIERHOEHLE"}, {353, "FLUSS"}, {354, "MEER"}, {355, "WASSERFALL"}, {385, "RUINE"}};
    // clang-format on
    CHECK(std::size(table) == 72);
    std::map<std::uint32_t, std::string> by_number;
    for (const auto& [number, name] : table) {
        by_number[number] = name;
        INFO(name);
        CHECK(sim::prodtype_name(number) == name);
        REQUIRE(sim::prodtype_by_name(name).has_value());
        CHECK(*sim::prodtype_by_name(name) == number);
    }
    for (std::uint32_t n = 0; n <= 420; ++n) {
        if (by_number.count(n) == 0) {
            INFO("number " << n);
            CHECK(sim::prodtype_name(n).empty());
        }
    }
}

TEST_CASE("goods table matches Appendix A") {
    const std::vector<sim::GoodsInfo> goods = sim::original_goods();
    REQUIRE(goods.size() == 0x8f);
    const std::pair<std::uint32_t, const char*> named[] = {{1, "EISENERZ"},
                                                           {2, "GOLD"},
                                                           {3, "SALZSTEIN"},
                                                           {4, "EDELSTEINE"},
                                                           {5, "STEINE"},
                                                           {6, "MAMORSTEIN"},
                                                           {9, "WOLLE"},
                                                           {10, "ZUCKER"},
                                                           {11, "TABAK"},
                                                           {12, "FLEISCH"},
                                                           {13, "KORN"},
                                                           {14, "MEHL"},
                                                           {15, "EISEN"},
                                                           {16, "FLACHS"},
                                                           {17, "HOPFEN"},
                                                           {18, "TIERHAEUTE"},
                                                           {19, "SEILE"},
                                                           {20, "SEIDE"},
                                                           {21, "FARBSTOFFE"},
                                                           {22, "WALSPECK"},
                                                           {23, "HOLZKOHLE"},
                                                           {26, "NAHRUNG"},
                                                           {27, "LEDER"},
                                                           {28, "SALZ"},
                                                           {29, "ALKOHOL"},
                                                           {30, "TABAKWAREN"},
                                                           {31, "GEWUERZE"},
                                                           {32, "HEILKRAEUTER"},
                                                           {33, "STOFFE"},
                                                           {34, "SEIDESTOFFE"},
                                                           {35, "LAMPENOEL"},
                                                           {36, "KLEIDUNG"},
                                                           {37, "SCHMUCK"},
                                                           {38, "PELZE"},
                                                           {39, "WEIN"},
                                                           {41, "WERKZEUG"},
                                                           {42, "HOLZ"},
                                                           {43, "ZIEGEL"},
                                                           {44, "MAMOR"},
                                                           {46, "SCHWERT"},
                                                           {47, "BOGEN"},
                                                           {48, "ARMBRUST"},
                                                           {49, "MUSKETE"},
                                                           {50, "RUESTUNG"},
                                                           {51, "LANZE"},
                                                           {52, "AXT"},
                                                           {53, "KANONESHIP"},
                                                           {56, "GETREIDE"},
                                                           {57, "TABAKBAUM"},
                                                           {58, "GEWUERZBAUM"},
                                                           {59, "FLACHSBAUM"},
                                                           {60, "ZUCKERROHR"},
                                                           {61, "BAUMWOLLE"},
                                                           {62, "WEINTRAUBEN"},
                                                           {63, "KAKAOBAUM"},
                                                           {64, "HOPFENBAUM"},
                                                           {65, "SEIDENRAUPE"},
                                                           {66, "FARBBAUM"},
                                                           {67, "KR\xC4UTER"},
                                                           {68, "KARTOFFEL"},
                                                           {69, "GRAS"},
                                                           {70, "BAUM"},
                                                           {71, "FISCH"},
                                                           {72, "EISFISCH"},
                                                           {76, "WIRTSHAUS"},
                                                           {77, "BADEHAUS"},
                                                           {78, "THEATER"},
                                                           {79, "KLINIK"},
                                                           {80, "SCHULE"},
                                                           {81, "HOCHSCHULE"},
                                                           {82, "KAPELLE"},
                                                           {83, "KIRCHE"},
                                                           {84, "KATHETRALE"},
                                                           {85, "PARK"},
                                                           {86, "BIBLIOTHEK"},
                                                           {87, "FEUERWEHR"},
                                                           {88, "GERICHT"},
                                                           {89, "GALGEN"},
                                                           {96, "SPIESSTRAEGER"},
                                                           {97, "SCHWERTKAEMPFER"},
                                                           {98, "BOGENSCHUETZE"},
                                                           {99, "LANZENKNECHT"},
                                                           {100, "KAVALERIE"},
                                                           {101, "ARMBRUSTSCHUETZE"},
                                                           {102, "MUSKETENSCHUETZE"},
                                                           {103, "MUSKETIER"},
                                                           {104, "PIONIER"},
                                                           {105, "BEDIENUNG"},
                                                           {106, "MEDICUS"},
                                                           {107, "TRAGTIER"},
                                                           {108, "KATAPULT"},
                                                           {109, "KATAPULT_GRP"},
                                                           {110, "KANONE"},
                                                           {111, "KANONE_GRP"},
                                                           {112, "MOERSER"},
                                                           {113, "MOERSER_GRP"},
                                                           {114, "BELAGERTURM"},
                                                           {115, "BELAGERTURM_GRP"},
                                                           {116, "STATIST"},
                                                           {117, "WELTBEHERRSCHER"},
                                                           {122, "HANDELSCHIFF1"},
                                                           {123, "HANDELSCHIFF2"},
                                                           {124, "HANDELSCHIFF3"},
                                                           {125, "KRIEGSCHIFF1"},
                                                           {126, "KRIEGSCHIFF2"},
                                                           {127, "KRIEGSCHIFF3"},
                                                           {128, "TRADERSCHIFF"},
                                                           {129, "PIRATSCHIFF"},
                                                           {132, "SCHATZ"},
                                                           {137, "FLEISCHTIER"},
                                                           {138, "PELZTIER"},
                                                           {139, "WALFISCH"},
                                                           {142, "ALL"}};
    std::map<std::uint32_t, std::string> by_id;
    for (const auto& [id, name] : named) {
        by_id[id] = name;
        INFO("goods " << id);
        CHECK(goods[id].name == name);
    }
    // Every id of Appendix A's unnamed list is UNUSED, and so is id 0.
    for (const std::uint32_t id :
         {0U,   7U,   8U,   24U,  25U,  40U,  45U,  54U,  55U,  73U,  74U,  75U, 90U,
          118U, 119U, 120U, 121U, 130U, 131U, 133U, 134U, 135U, 136U, 140U, 141U}) {
        INFO("goods " << id);
        CHECK(goods[id].name == "UNUSED");
    }
    // Every other id is UNUSED: the named ones are exactly the list above.
    for (std::uint32_t id = 0; id < goods.size(); ++id) {
        if (by_id.count(id) == 0) {
            INFO("goods " << id);
            CHECK(goods[id].name == "UNUSED");
        }
    }
    // Research requirements (BGruppen [WAREN] Forsch, OQ-034).
    for (std::uint32_t id = 0; id < goods.size(); ++id) {
        std::uint32_t want = 0;
        switch (id) {
        case 46:
            want = 1;
            break;
        case 47:
            want = 4;
            break;
        case 48:
            want = 9;
            break;
        case 49:
            want = 12;
            break;
        case 50:
            want = 1;
            break;
        case 51:
            want = 3;
            break;
        case 52:
            want = 6;
            break;
        case 53:
            want = 38;
            break;
        default:
            break;
        }
        INFO("goods " << id);
        CHECK(goods[id].forsch == want);
    }
    // The special spelling of goods 67 is the Latin-1 byte, as in Anlagen.dat.
    CHECK(goods[67].name.size() == 7);
    CHECK(static_cast<unsigned char>(goods[67].name[2]) == 0xC4);
    // The name lookup (the registry's goods_by_name) returns the first match.
    const auto registry = sim::TypeRegistry::empty();
    CHECK(*registry->goods_by_name("UNUSED") == 0);
    CHECK(*registry->goods_by_name("WOLLE") == 9);
    CHECK(*registry->goods_by_name("ALL") == 142);
    CHECK(*registry->goods_by_name("KR\xC4UTER") == 67);
    CHECK_FALSE(registry->goods_by_name("KRAEUTER").has_value());
    CHECK_FALSE(registry->goods_by_name("").has_value());
    CHECK(registry->goods(9)->name == "WOLLE");
    CHECK(registry->goods(0x8e)->name == "ALL");
    CHECK(registry->goods(0x8f) == nullptr);
    CHECK(registry->goods(0xFFFFFFFFU) == nullptr);
}

TEST_CASE("service mask of public goods") {
    // D AnnoGame 0x1000a600: 1 << (g - 0x4c) in 0x4c..0x5a; 0x51 -> 0x30,
    // 0x53 -> 0xc0, 0x54 -> 0x1c0.
    CHECK(sim::service_mask_for_goods(0x4b) == 0);
    CHECK(sim::service_mask_for_goods(0x4c) == 0x1);   // WIRTSHAUS
    CHECK(sim::service_mask_for_goods(0x4d) == 0x2);   // BADEHAUS
    CHECK(sim::service_mask_for_goods(0x4e) == 0x4);   // THEATER
    CHECK(sim::service_mask_for_goods(0x4f) == 0x8);   // KLINIK
    CHECK(sim::service_mask_for_goods(0x50) == 0x10);  // SCHULE
    CHECK(sim::service_mask_for_goods(0x51) == 0x30);  // HOCHSCHULE
    CHECK(sim::service_mask_for_goods(0x52) == 0x40);  // KAPELLE
    CHECK(sim::service_mask_for_goods(0x53) == 0xc0);  // KIRCHE
    CHECK(sim::service_mask_for_goods(0x54) == 0x1c0); // KATHETRALE
    CHECK(sim::service_mask_for_goods(0x55) == 0x200); // PARK
    CHECK(sim::service_mask_for_goods(0x56) == 0x400);
    CHECK(sim::service_mask_for_goods(0x57) == 0x800);
    CHECK(sim::service_mask_for_goods(0x58) == 0x1000);
    CHECK(sim::service_mask_for_goods(0x59) == 0x2000);
    CHECK(sim::service_mask_for_goods(0x5a) == 0x4000);
    CHECK(sim::service_mask_for_goods(0x5b) == 0);
    CHECK(sim::service_mask_for_goods(0) == 0);
    CHECK(sim::service_mask_for_goods(0xFFFFFFFFU) == 0);
}

TEST_CASE("builder defaults") {
    sim::TypeRegistry::Builder b;
    b.add_plain(sim::TypeId{5}, 97);
    auto built = b.build();
    REQUIRE(built);
    const sim::TypeRegistry& r = *built.value();
    REQUIRE(r.types().size() == 1);
    const sim::BuildingType& t = *r.find(sim::TypeId{5});
    CHECK(t.id == sim::TypeId{5});
    CHECK(t.prodtype == 97);
    CHECK(t.cls == sim::RuntimeClass::House);
    CHECK(t.width == 1);
    CHECK(t.height == 1);
    CHECK(t.research_id == 0);
    CHECK(t.cost_gold == 0);
    CHECK(t.cost_goods.empty());
    CHECK(t.max_hp == 0);
    CHECK(t.can_burn == 0);
    CHECK(t.action_radius == 0);
    CHECK(t.flags_word == 0);
    CHECK(t.rubble == 0);
    CHECK(t.service_mask == 0);
    CHECK(t.running_cost.empty());
    CHECK(t.output_goods.empty());
    CHECK(t.input_goods.empty());
    CHECK(t.head_raw.empty());
    CHECK_FALSE(t.has_prod);
    // The original goods and class tables, 1 x 1 footprint.
    CHECK(r.goods_table().size() == 0x8f);
    CHECK(r.class_table().size() == 25);
    CHECK(r.max_footprint() == 1);
    // A number that is not a building is a Plain type.
    sim::TypeRegistry::Builder road;
    road.add_plain(sim::TypeId{6}, 193).add_plain(sim::TypeId{7}, 0);
    auto roads = road.build();
    REQUIRE(roads);
    CHECK(roads.value()->find(sim::TypeId{6})->cls == sim::RuntimeClass::Plain);
    CHECK(roads.value()->find(sim::TypeId{7})->prodtype == 0);
    // An empty builder makes the empty registry.
    auto none = sim::TypeRegistry::Builder().build();
    REQUIRE(none);
    CHECK(none.value()->digest() == sim::TypeRegistry::empty()->digest());
    CHECK(none.value()->types().empty());
    CHECK(none.value()->max_footprint() == 1);
}

TEST_CASE("builder helpers fill the production types") {
    sim::TypeRegistry::Builder b;
    b.add_workshop(sim::TypeId{1}, "STOFFE", "WOLLE", 6, 11, 5, 2)
        .add_workshop(sim::TypeId{2}, "WERKZEUG", "", 3, 0, 4)
        .add_weapons_workshop(sim::TypeId{3}, "BOGEN", "ARMBRUST", "HOLZ", "SEILE", 9, 5)
        .add_farm(sim::TypeId{4}, "KORN", "GETREIDE", 37, 80, 5)
        .add_fur_trapper(sim::TypeId{5}, "PELZE", "PELZTIER", 26, 25, 5)
        .add_small_farm(sim::TypeId{6}, "KORN", "WOLLE", "GETREIDE", "GRAS", 13, 4, 128, 50, 4)
        .add_mine(sim::TypeId{7}, "EISENERZ", 21, 5);
    auto built = b.build();
    REQUIRE(built);
    const sim::TypeRegistry& r = *built.value();
    using C = sim::RuntimeClass;
    using K = sim::BuildingClass;
    const sim::BuildingType& weaver = *r.find(sim::TypeId{1});
    CHECK(weaver.cls == C::Workshop);
    CHECK(weaver.prodtype == 1);
    REQUIRE(weaver.has_prod);
    CHECK(weaver.prod.kind == K::Workshop);
    CHECK(weaver.prod.prodtype == "HANDWERK");
    CHECK(weaver.prod.id == 1);
    CHECK(weaver.prod.outputs == std::vector<std::string>{"STOFFE"});
    CHECK(weaver.prod.inputs == std::vector<std::string>{"WOLLE"});
    CHECK(weaver.output_goods == std::vector<sim::GoodsId>{33});
    CHECK(weaver.input_goods == std::vector<sim::GoodsId>{9});
    CHECK(weaver.prod.output_per_step == std::vector<std::uint32_t>{6, 0});
    CHECK(weaver.prod.input_per_step == std::vector<std::uint32_t>{11, 0});
    CHECK(weaver.prod.capacity == std::vector<std::uint32_t>{1280, 0});
    CHECK(weaver.prod.max_figures == std::vector<std::uint32_t>{2, 0, 0});
    CHECK(weaver.prod.figure_ids.size() == 9);
    CHECK(weaver.running_cost == std::vector<std::uint32_t>{0, 0});
    const sim::BuildingType& tools = *r.find(sim::TypeId{2});
    CHECK(tools.prod.inputs.empty());
    CHECK(tools.input_goods.empty());
    CHECK(tools.output_goods == std::vector<sim::GoodsId>{41});
    const sim::BuildingType& bows = *r.find(sim::TypeId{3});
    CHECK(bows.cls == C::WeaponsWorkshop);
    CHECK(bows.prodtype == 17);
    CHECK(bows.prod.kind == K::WeaponsWorkshop);
    CHECK(bows.output_goods == std::vector<sim::GoodsId>{47, 48});
    CHECK(bows.input_goods == std::vector<sim::GoodsId>{42, 19});
    CHECK(bows.prod.output_per_step == std::vector<std::uint32_t>{9, 9});
    CHECK(bows.prod.capacity == std::vector<std::uint32_t>{1280, 1280});
    CHECK(bows.prod.input_per_step == std::vector<std::uint32_t>{8, 4});
    CHECK(bows.prod.max_figures[0] == 1);
    const sim::BuildingType& farm = *r.find(sim::TypeId{4});
    CHECK(farm.cls == C::Farm);
    CHECK(farm.prodtype == 2);
    CHECK(farm.prod.kind == K::Farm);
    CHECK(farm.prod.output_per_step[0] == 37);
    CHECK(farm.prod.input_per_step[0] == 80);
    CHECK(farm.output_goods == std::vector<sim::GoodsId>{13});
    CHECK(farm.input_goods == std::vector<sim::GoodsId>{56});
    const sim::BuildingType& trapper = *r.find(sim::TypeId{5});
    CHECK(trapper.cls == C::FurTrapper);
    CHECK(trapper.prodtype == 16);
    CHECK(trapper.prod.kind == K::FurTrapper);
    CHECK(trapper.prod.output_per_step[0] == 26);
    CHECK(trapper.prod.input_per_step[0] == 25);
    CHECK(trapper.prod.capacity[0] == 1280);
    CHECK(trapper.prod.max_figures[0] == 1);
    CHECK(trapper.output_goods == std::vector<sim::GoodsId>{38});
    CHECK(trapper.input_goods == std::vector<sim::GoodsId>{138});
    const sim::BuildingType& small_farm = *r.find(sim::TypeId{6});
    CHECK(small_farm.cls == C::SmallFarm);
    CHECK(small_farm.prodtype == 9);
    CHECK(small_farm.prod.kind == K::SmallFarm);
    CHECK(small_farm.prod.outputs.size() == 2);
    CHECK(small_farm.prod.output_per_step == std::vector<std::uint32_t>{13, 4});
    CHECK(small_farm.prod.input_per_step == std::vector<std::uint32_t>{128, 50});
    CHECK(small_farm.prod.capacity == std::vector<std::uint32_t>{1024, 1024});
    CHECK(small_farm.prod.max_figures[0] == 3);
    const sim::BuildingType& mine = *r.find(sim::TypeId{7});
    CHECK(mine.cls == C::Mine);
    CHECK(mine.prodtype == 3);
    CHECK(mine.prod.kind == K::Mine);
    CHECK(mine.prod.output_per_step[0] == 21);
    CHECK(mine.prod.capacity[0] == 1280);
    CHECK(mine.output_goods == std::vector<sim::GoodsId>{1});
    CHECK(mine.input_goods.empty());
    // A public-need output sets the service mask.
    sim::TypeRegistry::Builder pub;
    pub.add_workshop(sim::TypeId{8}, "KATHETRALE", "", 1, 0, 1);
    CHECK(pub.build().value()->find(sim::TypeId{8})->service_mask == 0x1c0);
    // An unknown goods name is reported by build(), once, by name.
    sim::TypeRegistry::Builder bad;
    bad.add_workshop(sim::TypeId{9}, "NOSUCHGOODS", "WOLLE", 1, 1, 1);
    auto failed = bad.build();
    REQUIRE_FALSE(failed);
    CHECK(failed.error().find("NOSUCHGOODS") != std::string::npos);
    // A custom goods table is used for the resolution.
    sim::TypeRegistry::Builder custom;
    custom.goods({sim::GoodsInfo{"UNUSED", 0}, sim::GoodsInfo{"MYGOODS", 0}});
    custom.add_workshop(sim::TypeId{10}, "MYGOODS", "", 1, 0, 1);
    auto custom_built = custom.build();
    REQUIRE(custom_built);
    CHECK(custom_built.value()->find(sim::TypeId{10})->output_goods ==
          std::vector<sim::GoodsId>{1});
    CHECK(custom_built.value()->goods_table().size() == 2);
}

TEST_CASE("registry duplicate id fails") {
    sim::TypeRegistry::Builder b;
    b.add_plain(sim::TypeId{7}, 97).add_plain(sim::TypeId{3}, 97).add_plain(sim::TypeId{7}, 98);
    auto r = b.build();
    REQUIRE_FALSE(r);
    CHECK(r.error().find("duplicate") != std::string::npos);
    CHECK(r.error().find("7") != std::string::npos);
    // The same id twice from entries.
    Entries e;
    EntrySpec spec;
    spec.id = 42;
    e.add(spec);
    e.add(spec);
    auto from_entries = e.build();
    REQUIRE_FALSE(from_entries);
    CHECK(from_entries.error().find("42") != std::string::npos);
    // A class table of the wrong size is refused.
    sim::TypeRegistry::Builder wrong;
    wrong.classes({sim::ClassInfo{}});
    auto bad_classes = wrong.build();
    REQUIRE_FALSE(bad_classes);
    CHECK(bad_classes.error().find("class table") != std::string::npos);
}

TEST_CASE("registry lookups") {
    sim::TypeRegistry::Builder b;
    // Inserted out of order: the registry sorts by id.
    for (const std::uint32_t id : {900U, 5U, 70000U, 1U, 4294967290U, 33U}) {
        sim::BuildingType t;
        t.id = sim::TypeId{id};
        t.width = id % 5 + 1;
        t.height = id % 3 + 1;
        b.add(t);
    }
    auto built = b.build();
    REQUIRE(built);
    const sim::TypeRegistry& r = *built.value();
    std::vector<std::uint32_t> ids;
    for (const sim::BuildingType& t : r.types()) {
        ids.push_back(t.id.v);
    }
    CHECK(ids == std::vector<std::uint32_t>{1, 5, 33, 900, 70000, 4294967290U});
    for (const std::uint32_t id : ids) {
        const sim::BuildingType* t = r.find(sim::TypeId{id});
        REQUIRE(t != nullptr);
        CHECK(t->id.v == id);
    }
    for (const std::uint32_t missing : {0U, 2U, 6U, 34U, 899U, 901U, 70001U, 4294967291U}) {
        CHECK(r.find(sim::TypeId{missing}) == nullptr);
    }
    CHECK(r.find(sim::TypeId{}) == nullptr); // the invalid id
    // max_footprint is the largest of every width and height (F3).
    std::uint32_t largest = 1;
    for (const sim::BuildingType& t : r.types()) {
        largest = std::max({largest, t.width, t.height});
    }
    CHECK(r.max_footprint() == largest);
    sim::TypeRegistry::Builder big;
    sim::BuildingType wide;
    wide.id = sim::TypeId{1};
    wide.width = 12;
    wide.height = 2;
    sim::BuildingType tall;
    tall.id = sim::TypeId{2};
    tall.width = 3;
    tall.height = 20;
    big.add(wide).add(tall);
    CHECK(big.build().value()->max_footprint() == 20);
    sim::TypeRegistry::Builder wide_only;
    wide_only.add(wide);
    CHECK(wide_only.build().value()->max_footprint() == 12);
}

TEST_CASE("registry find_growing returns the growing item types") {
    sim::TypeRegistry::Builder b;
    sim::BuildingType growing;
    growing.id = sim::TypeId{712};
    growing.prodtype = 226;
    growing.has_prod = true;
    growing.prod.kind = sim::BuildingClass::GrowingItem;
    sim::BuildingType dry = growing;
    dry.id = sim::TypeId{714};
    sim::BuildingType not_growing;
    not_growing.id = sim::TypeId{713};
    not_growing.has_prod = true;
    not_growing.prod.kind = sim::BuildingClass::Other;
    sim::BuildingType no_prod;
    no_prod.id = sim::TypeId{715};
    b.add(growing).add(dry).add(not_growing).add(no_prod);
    auto built = b.build();
    REQUIRE(built);
    const sim::TypeRegistry& r = *built.value();
    CHECK(r.find_growing(712) == r.find(sim::TypeId{712}));
    CHECK(r.find_growing(714) == r.find(sim::TypeId{714}));
    CHECK(r.find_growing(713) == nullptr);
    CHECK(r.find_growing(715) == nullptr);
    CHECK(r.find_growing(716) == nullptr);
    CHECK(r.find_growing(0) == nullptr);
}

TEST_CASE("registry from entries decodes the HEAD record") {
    EntrySpec spec;
    spec.id = 1001;
    spec.head = head_bytes();
    spec.prodtype = "HANDWERK";
    spec.prod3 = weaver_prod3();
    spec.ware = names({"STOFFE", "UNUSED"});
    spec.rohstoff = names({"WOLLE", "UNUSED"});
    const auto registry = build_one(spec);
    const sim::BuildingType& t = *registry->find(sim::TypeId{1001});
    CHECK(t.prodtype == 1);
    CHECK(t.cls == sim::RuntimeClass::Workshop);
    CHECK(t.width == 3);
    CHECK(t.height == 2);
    CHECK(t.cost_gold == 300);
    // HEAD[5] tools -> goods 0x29, [4] wood -> 0x2a, [3] bricks -> 0x2b,
    // [6] marble -> 0x2c, each << 8, in ascending goods order (D 0x1000a5a0).
    REQUIRE(t.cost_goods.size() == 4);
    CHECK(t.cost_goods[0].goods == 0x29);
    CHECK(t.cost_goods[0].amount_88 == 13U << 8);
    CHECK(t.cost_goods[1].goods == 0x2a);
    CHECK(t.cost_goods[1].amount_88 == 11U << 8);
    CHECK(t.cost_goods[2].goods == 0x2b);
    CHECK(t.cost_goods[2].amount_88 == 7U << 8);
    CHECK(t.cost_goods[3].goods == 0x2c);
    CHECK(t.cost_goods[3].amount_88 == 17U << 8);
    CHECK(t.rubble == 5);
    CHECK(t.action_radius == 9);
    CHECK(t.flags_word == 0x2000);
    CHECK(t.max_hp == 2500);
    CHECK(t.can_burn == 3);
    CHECK(t.research_id == 0);
    CHECK(t.service_mask == 0);
    CHECK(t.output_goods == std::vector<sim::GoodsId>{33});
    CHECK(t.input_goods == std::vector<sim::GoodsId>{9});
    CHECK(t.running_cost == std::vector<std::uint32_t>{10, 15});
    REQUIRE(t.has_prod);
    CHECK(t.prod.output_per_step[0] == 6);
    CHECK(t.prod.input_per_step[0] == 11);
    CHECK(t.prod.capacity[0] == 1280);
    CHECK(t.prod.max_figures[0] == 1);
    CHECK(t.prod.outputs == std::vector<std::string>{"STOFFE"});
    // The whole HEAD record is kept in a 56-byte buffer, shorter records
    // zero-filled (D 0x10009334).
    REQUIRE(t.head_raw.size() == 56);
    CHECK(t.head_raw[0] == 3);
    CHECK(t.head_raw[0x2c] == 0xc4);
    CHECK(t.head_raw[0x31] == 3);
    CHECK(t.head_raw[52] == 0);
    CHECK(t.head_raw[55] == 0);
    CHECK(registry->max_footprint() == 3);
    // Costs of zero are left out; the gold is a u16.
    EntrySpec cheap;
    cheap.id = 2;
    Buffer h = head_bytes();
    h[6] = 0;
    h[3] = 0;
    h[8] = 0x34;
    h[9] = 0x12;
    cheap.head = h;
    const auto cheap_registry = build_one(cheap);
    const sim::BuildingType& c = *cheap_registry->find(sim::TypeId{2});
    CHECK(c.cost_gold == 0x1234);
    REQUIRE(c.cost_goods.size() == 2);
    CHECK(c.cost_goods[0].goods == 0x29);
    CHECK(c.cost_goods[1].goods == 0x2a);
    // No PROD3: no production data, running costs {0, 0}.
    CHECK_FALSE(c.has_prod);
    CHECK(c.running_cost == std::vector<std::uint32_t>{0, 0});
}

TEST_CASE("registry from entries head buffer is 56 bytes") {
    // A short HEAD is zero-filled: the hit points at 0x2c read as 0.
    EntrySpec shorter;
    shorter.head = head_bytes(0x12);
    auto r = build_one(shorter);
    const sim::BuildingType& s = *r->find(sim::TypeId{1});
    CHECK(s.width == 3);
    CHECK(s.cost_gold == 300);
    CHECK(s.rubble == 5);
    CHECK(s.action_radius == 9);
    CHECK(s.max_hp == 0);
    CHECK(s.can_burn == 0);
    CHECK(s.flags_word == 0); // 0x1c..0x1d are past the record
    CHECK(s.head_raw.size() == 56);
    // A HEAD of exactly 0x2e bytes has the hit points and nothing after.
    EntrySpec exact;
    exact.head = head_bytes(0x2e);
    r = build_one(exact);
    CHECK(r->find(sim::TypeId{1})->max_hp == 2500);
    CHECK(r->find(sim::TypeId{1})->can_burn == 0);
    // A longer HEAD is cut to the reader's 56-byte buffer.
    EntrySpec longer;
    longer.head = head_bytes(80);
    (*longer.head)[60] = 0x77;
    r = build_one(longer);
    CHECK(r->find(sim::TypeId{1})->head_raw.size() == 56);
    // Without a HEAD record there is no type to decode.
    EntrySpec headless;
    headless.id = 77;
    headless.head.reset();
    Entries e;
    e.add(headless);
    auto failed = e.build();
    REQUIRE_FALSE(failed);
    CHECK(failed.error().find("77") != std::string::npos);
    CHECK(failed.error().find("HEAD") != std::string::npos);
}

TEST_CASE("registry from entries resolves PRODTYPE research and goods names") {
    using C = sim::RuntimeClass;
    const std::pair<const char*, C> prodtypes[] = {{"HANDWERK", C::Workshop},
                                                   {"PLANTAGE", C::Farm},
                                                   {"MINE", C::Mine},
                                                   {"UNIVERSAL", C::SmallFarm},
                                                   {"PELZJAEGER", C::FurTrapper},
                                                   {"HANDWERKWAFFEN", C::WeaponsWorkshop},
                                                   {"FISCHER", C::Farm},
                                                   {"KONTOR", C::Market},
                                                   {"PIONIER", C::House},
                                                   {"MARKTSTAND", C::MarketStall},
                                                   {"WACHTURM", C::Tower},
                                                   {"STRASSE", C::Plain},
                                                   {"UNUSED", C::Plain}};
    for (const auto& [name, cls] : prodtypes) {
        EntrySpec spec;
        spec.prodtype = name;
        // The classes that tick need their production data (a Mine also an output).
        spec.prod3 = weaver_prod3();
        spec.ware = names({"EISENERZ", "UNUSED"});
        const auto r = build_one(spec);
        INFO(name);
        CHECK(r->find(sim::TypeId{1})->cls == cls);
        CHECK(r->find(sim::TypeId{1})->prodtype == *sim::prodtype_by_name(name));
    }
    // No PRODTYPE record reads as UNUSED (0).
    EntrySpec none;
    none.prodtype.reset();
    CHECK(build_one(none)->find(sim::TypeId{1})->prodtype == 0);
    // FORSCHTYPE.
    EntrySpec mine;
    mine.prodtype = "MINE";
    mine.prod3 = weaver_prod3();
    mine.ware = names({"EISENERZ", "UNUSED"});
    mine.forsch = "TIEFE_ERZMINE";
    CHECK(build_one(mine)->find(sim::TypeId{1})->research_id == 57);
    mine.forsch = "UNUSED";
    CHECK(build_one(mine)->find(sim::TypeId{1})->research_id == 0);
    mine.forsch.reset();
    CHECK(build_one(mine)->find(sim::TypeId{1})->research_id == 0);
    // WARE and ROHSTOFF: trailing UNUSED dropped, an inner UNUSED is goods 0.
    EntrySpec goods;
    goods.ware = names({"UNUSED", "STOFFE"});
    goods.rohstoff = names({"WOLLE", "SEILE"});
    const auto r = build_one(goods);
    CHECK(r->find(sim::TypeId{1})->output_goods == std::vector<sim::GoodsId>{0, 33});
    CHECK(r->find(sim::TypeId{1})->input_goods == std::vector<sim::GoodsId>{9, 19});
    EntrySpec trailing;
    trailing.ware = names({"STOFFE", "UNUSED"});
    CHECK(build_one(trailing)->find(sim::TypeId{1})->output_goods == std::vector<sim::GoodsId>{33});
    EntrySpec all_unused;
    all_unused.ware = names({"UNUSED", "UNUSED"});
    CHECK(build_one(all_unused)->find(sim::TypeId{1})->output_goods.empty());
    // The public-need masks follow WARE[0].
    const std::pair<const char*, std::uint32_t> masks[] = {
        {"WIRTSHAUS", 0x1},    {"HOCHSCHULE", 0x30}, {"KIRCHE", 0xc0},
        {"KATHETRALE", 0x1c0}, {"PARK", 0x200},      {"WOLLE", 0}};
    for (const auto& [name, mask] : masks) {
        EntrySpec spec;
        spec.ware = names({name, "UNUSED"});
        INFO(name);
        CHECK(build_one(spec)->find(sim::TypeId{1})->service_mask == mask);
    }
    // Only WARE[0] counts.
    EntrySpec second;
    second.ware = names({"WOLLE", "KIRCHE"});
    CHECK(build_one(second)->find(sim::TypeId{1})->service_mask == 0);
    // The Latin-1 spelling of goods 67.
    EntrySpec herbs;
    herbs.ware = names({"KR\xC4UTER", "UNUSED"});
    CHECK(build_one(herbs)->find(sim::TypeId{1})->output_goods == std::vector<sim::GoodsId>{67});
}

TEST_CASE("registry from entries refuses what it cannot resolve") {
    auto error_of = [](const EntrySpec& spec, const sim::SimConfig& cfg = sim::SimConfig{}) {
        Entries e;
        e.add(spec);
        auto r = e.build(cfg);
        REQUIRE_FALSE(r);
        return r.error();
    };
    EntrySpec unknown_prodtype;
    unknown_prodtype.id = 5;
    unknown_prodtype.prodtype = "NOSUCHTYPE";
    std::string err = error_of(unknown_prodtype);
    CHECK(err.find("entry 5") != std::string::npos);
    CHECK(err.find("PRODTYPE") != std::string::npos);
    CHECK(err.find("NOSUCHTYPE") != std::string::npos);
    EntrySpec unknown_research;
    unknown_research.id = 6;
    unknown_research.forsch = "NO_RESEARCH";
    err = error_of(unknown_research);
    CHECK(err.find("entry 6") != std::string::npos);
    CHECK(err.find("FORSCHTYPE") != std::string::npos);
    EntrySpec unknown_ware;
    unknown_ware.id = 7;
    unknown_ware.ware = names({"NOSUCHGOODS"});
    err = error_of(unknown_ware);
    CHECK(err.find("entry 7") != std::string::npos);
    CHECK(err.find("WARE") != std::string::npos);
    CHECK(err.find("NOSUCHGOODS") != std::string::npos);
    EntrySpec unknown_input;
    unknown_input.id = 8;
    unknown_input.rohstoff = names({"WOLLE", "NOSUCHINPUT"});
    err = error_of(unknown_input);
    CHECK(err.find("ROHSTOFF") != std::string::npos);
    CHECK(err.find("NOSUCHINPUT") != std::string::npos);
    // A PROD3 record of 68 bytes needs a HEAD of at least 0x1a bytes (the
    // production data reads the stage bytes there).
    EntrySpec short_head;
    short_head.id = 9;
    short_head.head = head_bytes(8);
    short_head.prod3 = weaver_prod3();
    err = error_of(short_head);
    CHECK(err.find("entry 9") != std::string::npos);
    // The service mask must fit the counters of BuildingIndex.
    EntrySpec cathedral;
    cathedral.id = 10;
    cathedral.ware = names({"KATHETRALE", "UNUSED"}); // 0x1c0: bits 6..8
    sim::SimConfig narrow;
    narrow.service_bits = 8;
    err = error_of(cathedral, narrow);
    CHECK(err.find("service") != std::string::npos);
    CHECK(err.find("10") != std::string::npos);
    narrow.service_bits = 9;
    Entries e;
    e.add(cathedral);
    CHECK(e.build(narrow));
    // 32 counters accept every mask (no shift by 32).
    sim::SimConfig wide;
    wide.service_bits = 32;
    CHECK(e.build(wide));
    // The goods table the entries are resolved against is the caller's.
    Entries herbs;
    EntrySpec spec;
    spec.ware = names({"WOLLE"});
    herbs.add(spec);
    auto custom = sim::TypeRegistry::build(
        herbs.entries, std::vector<sim::GoodsInfo>{{"UNUSED", 0}, {"OTHER", 0}}, sim::SimConfig{});
    REQUIRE_FALSE(custom);
    CHECK(custom.error().find("WOLLE") != std::string::npos);
}

TEST_CASE("registry refuses what it cannot run") {
    auto error_of = [](const EntrySpec& spec, const sim::SimConfig& cfg = sim::SimConfig{}) {
        Entries e;
        e.add(spec);
        auto r = e.build(cfg);
        REQUIRE_FALSE(r);
        return r.error();
    };
    auto builder_error = [](sim::TypeRegistry::Builder& b) {
        auto r = b.build();
        REQUIRE_FALSE(r);
        return r.error();
    };
    SUBCASE("a zero footprint") {
        EntrySpec flat;
        flat.id = 31;
        (*flat.head)[1] = 0; // height
        std::string err = error_of(flat);
        CHECK(err.find("type 31") != std::string::npos);
        CHECK(err.find("footprint") != std::string::npos);
        EntrySpec thin;
        thin.id = 32;
        (*thin.head)[0] = 0; // width
        err = error_of(thin);
        CHECK(err.find("type 32") != std::string::npos);
        CHECK(err.find("footprint") != std::string::npos);
        sim::BuildingType t;
        t.id = sim::TypeId{33};
        t.width = 0;
        sim::TypeRegistry::Builder by_hand;
        by_hand.add(t);
        err = builder_error(by_hand);
        CHECK(err.find("type 33") != std::string::npos);
        t.width = 1;
        t.height = 0;
        sim::TypeRegistry::Builder tall;
        tall.add(t);
        CHECK(builder_error(tall).find("footprint") != std::string::npos);
    }
    SUBCASE("a class that ticks needs production data of its kind") {
        // Every ticking class without a PROD3 record (the original reader leaves
        // the production fields at zero for it, and the class tick would run on
        // nothing).
        for (const char* name : {"HANDWERK", "HANDWERKWAFFEN", "PLANTAGE", "WEIDETIER", "FISCHER",
                                 "PELZJAEGER", "MINE", "UNIVERSAL"}) {
            EntrySpec spec;
            spec.id = 40;
            spec.prodtype = name;
            spec.ware = names({"EISENERZ", "UNUSED"});
            INFO(name);
            const std::string err = error_of(spec);
            CHECK(err.find("type 40") != std::string::npos);
            CHECK(err.find("production data") != std::string::npos);
        }
        // A PROD3 of another size than 68 bytes means "no production data".
        EntrySpec odd;
        odd.id = 41;
        odd.prodtype = "HANDWERK";
        odd.prod3 = weaver_prod3();
        odd.prod3->pop_back();
        CHECK(error_of(odd).find("type 41") != std::string::npos);
        // A class whose tick is not ported needs none: a dual workshop, a market,
        // a house or a hunter builds without PROD3.
        for (const char* name : {"HANDWERKDUAL", "KONTOR", "PIONIER", "JAGDHAUS", "STEINBRUCH"}) {
            EntrySpec spec;
            spec.prodtype = name;
            INFO(name);
            CHECK(build_one(spec)->find(sim::TypeId{1}) != nullptr);
        }
        // The helper add_plain makes no production data: a ticking class is refused.
        sim::TypeRegistry::Builder plain;
        plain.add_plain(sim::TypeId{42}, 1);
        const std::string err = builder_error(plain);
        CHECK(err.find("type 42") != std::string::npos);
        // The production kind must be the class's own.
        sim::BuildingType wrong;
        wrong.id = sim::TypeId{43};
        wrong.prodtype = 1;
        wrong.cls = sim::RuntimeClass::Workshop;
        wrong.has_prod = true;
        wrong.prod.kind = sim::BuildingClass::Farm;
        sim::TypeRegistry::Builder mismatch;
        mismatch.add(wrong);
        CHECK(builder_error(mismatch).find("type 43") != std::string::npos);
        wrong.prod.kind = sim::BuildingClass::Workshop;
        sim::TypeRegistry::Builder fine;
        fine.add(wrong);
        CHECK(fine.build());
        // A modded class table that makes the workshop inert accepts add_plain.
        std::vector<sim::ClassInfo> inert = sim::original_class_table();
        inert[1] = {sim::RuntimeClass::Workshop, sim::TickKind::None, sim::PayloadTag::Inert, 500};
        sim::TypeRegistry::Builder modded;
        modded.classes(inert).add_plain(sim::TypeId{44}, 1);
        CHECK(modded.build());
    }
    SUBCASE("a mine needs an output") {
        EntrySpec mine;
        mine.id = 50;
        mine.prodtype = "MINE";
        mine.prod3 = weaver_prod3();
        mine.ware = names({"UNUSED", "UNUSED"});
        std::string err = error_of(mine);
        CHECK(err.find("type 50") != std::string::npos);
        CHECK(err.find("Mine") != std::string::npos);
        mine.ware.reset();
        CHECK(error_of(mine).find("Mine") != std::string::npos);
        mine.ware = names({"EISENERZ", "UNUSED"});
        CHECK(build_one(mine)->find(sim::TypeId{50}) != nullptr);
    }
    SUBCASE("a build cost good outside the configured build goods") {
        // head_bytes costs: 0x29 (tools), 0x2a (wood), 0x2b (bricks), 0x2c (marble).
        EntrySpec priced;
        priced.id = 60;
        sim::SimConfig two;
        two.build_goods_count = 2; // 0x29 and 0x2a
        std::string err = error_of(priced, two);
        CHECK(err.find("type 60") != std::string::npos);
        CHECK(err.find("build cost goods 43") != std::string::npos);
        sim::SimConfig shifted;
        shifted.build_goods_first = 0x2a; // 0x29 is below the range
        err = error_of(priced, shifted);
        CHECK(err.find("build cost goods 41") != std::string::npos);
        sim::SimConfig four;
        four.build_goods_count = 4;
        Entries e;
        e.add(priced);
        auto fits = e.build(four);
        REQUIRE(fits);
        // The sealed registry is checked against a config again (a registry is shared
        // between worlds, and a loaded header has its own config).
        CHECK(fits.value()->validate_against(four).empty());
        CHECK(fits.value()->validate_against(sim::SimConfig{}).empty());
        CHECK_FALSE(fits.value()->validate_against(two).empty());
        CHECK(fits.value()->validate_against(two).find("type 60") != std::string::npos);
        CHECK_FALSE(fits.value()->validate_against(shifted).empty());
        // A cost good beyond the warehouse (the original has 56 goods) by hand.
        sim::BuildingType t;
        t.id = sim::TypeId{61};
        t.cost_goods = {{0x38, 256}};
        sim::TypeRegistry::Builder by_hand;
        by_hand.add(t);
        CHECK(builder_error(by_hand).find("type 61") != std::string::npos);
        t.cost_goods = {{0x2d, 256}}; // the fifth build good of the original: accepted
        sim::TypeRegistry::Builder fifth;
        fifth.add(t);
        CHECK(fifth.build());
        // The empty registry fits every valid config.
        CHECK(sim::TypeRegistry::empty()->validate_against(two).empty());
    }
    SUBCASE("a class table that disagrees with itself") {
        std::vector<sim::ClassInfo> swapped = sim::original_class_table();
        std::swap(swapped[1], swapped[2]); // the cls of row 1 is now DualWorkshop
        sim::TypeRegistry::Builder b1;
        b1.classes(swapped);
        std::string err = builder_error(b1);
        CHECK(err.find("class table row 1") != std::string::npos);
        // The last row too: a wrong cls anywhere is found.
        std::vector<sim::ClassInfo> last = sim::original_class_table();
        last.back().cls = sim::RuntimeClass::Plain;
        sim::TypeRegistry::Builder b2;
        b2.classes(last);
        CHECK(builder_error(b2).find("class table row 24") != std::string::npos);
        // A tick whose payload is another alternative, or Inert.
        std::vector<sim::ClassInfo> inert_payload = sim::original_class_table();
        inert_payload[1].payload = sim::PayloadTag::Inert;
        sim::TypeRegistry::Builder b3;
        b3.classes(inert_payload);
        err = builder_error(b3);
        CHECK(err.find("class table row 1") != std::string::npos);
        CHECK(err.find("disagree") != std::string::npos);
        std::vector<sim::ClassInfo> other_payload = sim::original_class_table();
        other_payload[4].payload = sim::PayloadTag::Mine; // Farm tick
        sim::TypeRegistry::Builder b4;
        b4.classes(other_payload);
        CHECK(builder_error(b4).find("class table row 4") != std::string::npos);
        std::vector<sim::ClassInfo> payload_without_tick = sim::original_class_table();
        payload_without_tick[8].payload = sim::PayloadTag::Farm; // Quarry: no tick
        sim::TypeRegistry::Builder b5;
        b5.classes(payload_without_tick);
        CHECK(builder_error(b5).find("class table row 8") != std::string::npos);
        std::vector<sim::ClassInfo> bad_tick = sim::original_class_table();
        bad_tick[8].tick = static_cast<sim::TickKind>(99);
        sim::TypeRegistry::Builder b6;
        b6.classes(bad_tick);
        CHECK(builder_error(b6).find("class table row 8") != std::string::npos);
        // FurTrapper shares the Farm payload and is fine.
        sim::TypeRegistry::Builder ok;
        ok.classes(sim::original_class_table());
        CHECK(ok.build());
        // A type whose class is outside the table.
        sim::BuildingType t;
        t.id = sim::TypeId{70};
        t.cls = static_cast<sim::RuntimeClass>(200);
        sim::TypeRegistry::Builder outside;
        outside.add(t);
        CHECK(builder_error(outside).find("type 70") != std::string::npos);
    }
}

TEST_CASE("ware names after the second are ignored as in the original") {
    // AnnoGame.dll reads exactly two names from WARE and from ROHSTOFF (0x1000a1f4,
    // 0x1000a2e2): a third name is never looked up, not even to report it unknown.
    CHECK(sim::kWareRecordNames == 2);
    EntrySpec three;
    three.ware = names({"STOFFE", "WOLLE", "NOSUCHGOODS"});
    three.rohstoff = names({"WOLLE", "SEILE", "ALSONOSUCH"});
    const auto r = build_one(three);
    CHECK(r->find(sim::TypeId{1})->output_goods == std::vector<sim::GoodsId>{33, 9});
    CHECK(r->find(sim::TypeId{1})->input_goods == std::vector<sim::GoodsId>{9, 19});
    // An UNUSED or empty second name is cut, a third name does not bring it back.
    EntrySpec cut;
    cut.ware = names({"STOFFE", "UNUSED", "WOLLE"});
    cut.rohstoff = names({"WOLLE", "", "SEILE"});
    const auto c = build_one(cut);
    CHECK(c->find(sim::TypeId{1})->output_goods == std::vector<sim::GoodsId>{33});
    CHECK(c->find(sim::TypeId{1})->input_goods == std::vector<sim::GoodsId>{9});
    // An empty trailing name (two NUL bytes) is dropped like UNUSED.
    EntrySpec empty_tail;
    empty_tail.ware = names({"STOFFE", ""});
    CHECK(build_one(empty_tail)->find(sim::TypeId{1})->output_goods ==
          std::vector<sim::GoodsId>{33});
    // An empty name inside the first two is not in the goods table: refused like any
    // unknown name (the original would read it as UNUSED, F25).
    Entries empty_head;
    EntrySpec head_spec;
    head_spec.ware = names({"", "STOFFE"});
    empty_head.add(head_spec);
    CHECK_FALSE(empty_head.build());
    // The production type reads the same two names.
    EntrySpec prod;
    prod.prodtype = "HANDWERK";
    prod.prod3 = weaver_prod3();
    prod.ware = names({"STOFFE", "KLEIDUNG", "NOSUCHGOODS"});
    const auto p = build_one(prod);
    REQUIRE(p->find(sim::TypeId{1})->has_prod);
    CHECK(p->find(sim::TypeId{1})->prod.outputs == std::vector<std::string>{"STOFFE", "KLEIDUNG"});
}

TEST_CASE("class for prodtype follows the layout") {
    using C = sim::RuntimeClass;
    // The class lookup and the list lookup agree on what a building is, for the
    // original layout and for layouts with more or fewer blocks.
    auto agrees = [](const sim::ListLayout& layout) {
        for (std::uint32_t p = 0; p <= 600; ++p) {
            INFO("prodtype " << p);
            const bool building = p != 0 && layout.list_for(p).has_value();
            CHECK(sim::class_for_prodtype(p, layout).has_value() == building);
        }
    };
    const sim::ListLayout original = sim::ListLayout::original();
    agrees(original);
    for (std::uint32_t p = 0; p <= 416; ++p) {
        INFO("prodtype " << p);
        CHECK(sim::class_for_prodtype(p, original) == sim::class_for_prodtype(p));
    }
    // A layout with a seventh block: the numbers 193..224 are buildings of the
    // default class (STRASSE is only a tile object in the original layout).
    sim::ListLayout wide = original;
    wide.block_to_list.push_back(4);
    agrees(wide);
    CHECK(sim::class_for_prodtype(193, wide) == C::Plain);
    CHECK(sim::class_for_prodtype(224, wide) == C::Plain);
    CHECK_FALSE(sim::class_for_prodtype(225, wide).has_value());
    CHECK_FALSE(sim::class_for_prodtype(193, original).has_value());
    // The named classes keep their numbers.
    CHECK(sim::class_for_prodtype(97, wide) == C::House);
    // A layout with five blocks: the stalls (161..192) are not buildings.
    sim::ListLayout narrow = original;
    narrow.block_to_list.resize(5);
    agrees(narrow);
    CHECK_FALSE(sim::class_for_prodtype(161, narrow).has_value());
    CHECK(sim::class_for_prodtype(160, narrow).has_value());
    // A shifted first number and a different block size move the extent.
    sim::ListLayout shifted = original;
    shifted.first_prodtype = 33;
    agrees(shifted);
    CHECK_FALSE(sim::class_for_prodtype(32, shifted).has_value());
    CHECK(sim::class_for_prodtype(33, shifted) == C::Market);
    sim::ListLayout small_blocks = original;
    small_blocks.block_size = 16;
    agrees(small_blocks);
    // The Builder derives the class with the layout of its config.
    sim::SimConfig cfg;
    cfg.layout = wide;
    sim::TypeRegistry::Builder b;
    b.config(cfg).add_plain(sim::TypeId{1}, 200);
    auto built = b.build();
    REQUIRE(built);
    CHECK(built.value()->find(sim::TypeId{1})->cls == C::Plain);
}

TEST_CASE("registry digest stable") {
    // The digest of the empty registry (the original goods and class tables):
    // an independent transcription is the FNV-1a 64 of u64 0 (types), the goods
    // (u64 143, then name as u64 length + bytes and u32 forsch), the classes
    // (u64 25, then u8 class, u8 tick, u8 payload, u32 gate period) and u64 0 (the
    // figure types, T24).
    sim::StateHasher h;
    h.u64(0);
    const std::vector<sim::GoodsInfo> goods = sim::original_goods();
    h.u64(goods.size());
    for (const sim::GoodsInfo& g : goods) {
        h.u64(g.name.size());
        for (const char c : g.name) {
            h.u8(static_cast<std::uint8_t>(c));
        }
        h.u32(g.forsch);
    }
    const std::vector<sim::ClassInfo> classes = sim::original_class_table();
    h.u64(classes.size());
    for (const sim::ClassInfo& c : classes) {
        h.u8(static_cast<std::uint8_t>(c.cls));
        h.u8(static_cast<std::uint8_t>(c.tick));
        h.u8(static_cast<std::uint8_t>(c.payload));
        h.u32(c.gate_period);
    }
    h.u64(0);
    CHECK(sim::TypeRegistry::empty()->digest() == h.value());
    // And the value of an independent Python transcription (tools/sim_stream.py:
    // goods names read from AnnoGame.dll, classes from Appendix C).
    CHECK(h.value() == 0x18432b5c366bf7b8ULL);
    // Rebuilding gives the same digest; the order types are added in does not matter.
    auto a = sim::synthetic::make_synthetic_types();
    auto b = sim::synthetic::make_synthetic_types();
    REQUIRE(a);
    REQUIRE(b);
    CHECK(a.value().registry->digest() == b.value().registry->digest());
    CHECK(a.value().registry->digest() != sim::TypeRegistry::empty()->digest());
    sim::TypeRegistry::Builder forward, backward;
    forward.add_plain(sim::TypeId{1}, 97).add_plain(sim::TypeId{2}, 35);
    backward.add_plain(sim::TypeId{2}, 35).add_plain(sim::TypeId{1}, 97);
    CHECK(forward.build().value()->digest() == backward.build().value()->digest());
    // The synthetic registry's digest, pinned (a change of the type data, the
    // class or goods tables, or the digest layout moves it).
    CHECK(a.value().registry->digest() == 0x13e65df34212c0c3ULL);
}

TEST_CASE("registry digest changes with every field") {
    static_assert(sim::field_count<sim::BuildingType>() == 21);
    static_assert(sim::field_count<sim::ProductionType>() == 18);
    sim::BuildingType base;
    base.id = sim::TypeId{10};
    base.prodtype = 1;
    base.cls = sim::RuntimeClass::Workshop;
    base.width = 2;
    base.height = 3;
    base.research_id = 4;
    base.cost_gold = 5;
    base.cost_goods = {{0x29, 256}};
    base.max_hp = 6;
    base.can_burn = 7;
    base.action_radius = 8;
    base.flags_word = 9;
    base.rubble = 10;
    base.service_mask = 11;
    base.running_cost = {12, 13};
    base.output_goods = {14};
    base.input_goods = {15};
    base.head_raw = {16, 17};
    base.entrances = {{1, 3}, {5, 9}};
    base.has_prod = true;
    base.prod.id = 10;
    base.prod.prodtype = "HANDWERK";
    base.prod.kind = sim::BuildingClass::Workshop;
    base.prod.outputs = {"STOFFE"};
    base.prod.inputs = {"WOLLE"};
    base.prod.capacity = {1, 2};
    base.prod.output_per_step = {3, 4};
    base.prod.input_per_step = {5, 6};
    base.prod.time0_128 = 7;
    base.prod.time1_128 = 8;
    base.prod.max_figures = {9, 10, 11};
    base.prod.figure_type_count = {12, 13, 14};
    base.prod.figure_ids = {15, 16, 17, 18, 19, 20, 21, 22, 23};
    base.prod.stages = 24;
    base.prod.stage_shift = 25;
    base.prod.growth_period = 26;
    base.prod.display_rate0 = 27;
    base.prod.display_rate1 = 28;
    // TypeRegistry::digest_of hashes the data as given, so perturbations that
    // build() refuses (a class that no longer matches its production data) still
    // move the digest.
    const std::vector<sim::GoodsInfo> goods_table = sim::original_goods();
    const std::vector<sim::ClassInfo> class_table = sim::original_class_table();
    auto digest_of = [&](const sim::BuildingType& t) {
        return sim::TypeRegistry::digest_of({&t, 1}, goods_table, class_table);
    };
    using Mut = std::function<void(sim::BuildingType&)>;
    std::vector<std::pair<std::string, Mut>> muts = {
        {"id", [](auto& t) { t.id = sim::TypeId{11}; }},
        {"prodtype", [](auto& t) { t.prodtype += 1; }},
        {"cls", [](auto& t) { t.cls = sim::RuntimeClass::Farm; }},
        {"width", [](auto& t) { t.width += 1; }},
        {"height", [](auto& t) { t.height += 1; }},
        {"research_id", [](auto& t) { t.research_id += 1; }},
        {"cost_gold", [](auto& t) { t.cost_gold += 1; }},
        {"cost_goods.goods", [](auto& t) { t.cost_goods[0].goods += 1; }},
        {"cost_goods.amount", [](auto& t) { t.cost_goods[0].amount_88 += 1; }},
        {"cost_goods.count", [](auto& t) { t.cost_goods.clear(); }},
        {"max_hp", [](auto& t) { t.max_hp += 1; }},
        {"can_burn", [](auto& t) { t.can_burn += 1; }},
        {"action_radius", [](auto& t) { t.action_radius += 1; }},
        {"flags_word", [](auto& t) { t.flags_word += 1; }},
        {"rubble", [](auto& t) { t.rubble += 1; }},
        {"service_mask", [](auto& t) { t.service_mask += 1; }},
        {"running_cost", [](auto& t) { t.running_cost[1] += 1; }},
        {"output_goods", [](auto& t) { t.output_goods[0] += 1; }},
        {"input_goods", [](auto& t) { t.input_goods[0] += 1; }},
        {"head_raw", [](auto& t) { t.head_raw[1] += 1; }},
        {"entrances.x6", [](auto& t) { t.entrances[1].x6 += 1; }},
        {"entrances.y6", [](auto& t) { t.entrances[0].y6 += 1; }},
        {"entrances.count", [](auto& t) { t.entrances.pop_back(); }},
        {"has_prod", [](auto& t) { t.has_prod = false; }},
        {"prod.id", [](auto& t) { t.prod.id += 1; }},
        {"prod.prodtype", [](auto& t) { t.prod.prodtype = "PLANTAGE"; }},
        {"prod.kind", [](auto& t) { t.prod.kind = sim::BuildingClass::Farm; }},
        {"prod.outputs", [](auto& t) { t.prod.outputs[0] = "GOLD"; }},
        {"prod.inputs", [](auto& t) { t.prod.inputs.push_back("GOLD"); }},
        {"prod.capacity", [](auto& t) { t.prod.capacity[1] += 1; }},
        {"prod.output_per_step", [](auto& t) { t.prod.output_per_step[1] += 1; }},
        {"prod.input_per_step", [](auto& t) { t.prod.input_per_step[1] += 1; }},
        {"prod.time0_128", [](auto& t) { t.prod.time0_128 += 1; }},
        {"prod.time1_128", [](auto& t) { t.prod.time1_128 += 1; }},
        {"prod.max_figures", [](auto& t) { t.prod.max_figures[2] += 1; }},
        {"prod.figure_type_count", [](auto& t) { t.prod.figure_type_count[2] += 1; }},
        {"prod.figure_ids", [](auto& t) { t.prod.figure_ids[8] += 1; }},
        {"prod.stages", [](auto& t) { t.prod.stages += 1; }},
        {"prod.stage_shift", [](auto& t) { t.prod.stage_shift += 1; }},
        {"prod.growth_period", [](auto& t) { t.prod.growth_period += 1; }},
        {"prod.display_rate0", [](auto& t) { t.prod.display_rate0 += 1; }},
        {"prod.display_rate1", [](auto& t) { t.prod.display_rate1 += 1; }},
    };
    const std::uint64_t base_digest = digest_of(base);
    CHECK(digest_of(base) == base_digest);
    std::map<std::uint64_t, std::string> seen;
    seen[base_digest] = "base";
    for (const auto& [name, mutate] : muts) {
        sim::BuildingType t = base;
        mutate(t);
        INFO("field " << name);
        CHECK(seen.emplace(digest_of(t), name).second);
    }
    // The goods and class tables are in the digest too.
    const std::uint64_t empty_digest = sim::TypeRegistry::empty()->digest();
    CHECK(sim::TypeRegistry::digest_of({}, goods_table, class_table) == empty_digest);
    auto goods = sim::original_goods();
    goods[9].forsch = 1;
    CHECK(sim::TypeRegistry::digest_of({}, goods, class_table) != empty_digest);
    goods = sim::original_goods();
    goods[9].name = "WOLLE2";
    CHECK(sim::TypeRegistry::digest_of({}, goods, class_table) != empty_digest);
    // The figure types (T24) too, field by field.
    static_assert(sim::field_count<sim::FigureType>() == 5);
    const sim::FigureType fig{220, 53687, 1280, 3, 500};
    std::map<std::uint64_t, std::string> fseen;
    fseen[sim::TypeRegistry::digest_of({}, goods_table, class_table, {&fig, 1})] = "base";
    using FMut = std::function<void(sim::FigureType&)>;
    const std::vector<std::pair<std::string, FMut>> fmuts = {
        {"id", [](auto& f) { f.id += 1; }},
        {"speed_q24", [](auto& f) { f.speed_q24 += 1; }},
        {"capacity_88", [](auto& f) { f.capacity_88 += 1; }},
        {"layer", [](auto& f) { f.layer += 1; }},
        {"load_ticks", [](auto& f) { f.load_ticks += 1; }},
    };
    for (const auto& [name, mutate] : fmuts) {
        sim::FigureType f = fig;
        mutate(f);
        INFO("figure field " << name);
        CHECK(
            fseen.emplace(sim::TypeRegistry::digest_of({}, goods_table, class_table, {&f, 1}), name)
                .second);
    }
    CHECK(fseen.size() == fmuts.size() + 1);
    CHECK(fseen.count(empty_digest) == 0);
    auto classes = sim::original_class_table();
    classes[1].gate_period = 501;
    CHECK(sim::TypeRegistry::digest_of({}, goods_table, classes) != empty_digest);
    classes = sim::original_class_table();
    classes[1].tick = sim::TickKind::None;
    CHECK(sim::TypeRegistry::digest_of({}, goods_table, classes) != empty_digest);
    classes = sim::original_class_table();
    classes[1].payload = sim::PayloadTag::Inert;
    CHECK(sim::TypeRegistry::digest_of({}, goods_table, classes) != empty_digest);
    classes = sim::original_class_table();
    classes[1].cls = sim::RuntimeClass::Plain;
    CHECK(sim::TypeRegistry::digest_of({}, goods_table, classes) != empty_digest);
    // The digest of a built registry is the digest of its data.
    auto built = sim::synthetic::make_synthetic_types();
    REQUIRE(built);
    CHECK(sim::TypeRegistry::digest_of(built.value().registry->types(), goods_table, class_table) ==
          built.value().registry->digest());
}

TEST_CASE("synthetic registry holds the fixture types") {
    auto made = sim::synthetic::make_synthetic_types();
    REQUIRE(made);
    const sim::synthetic::SyntheticTypes& s = made.value();
    const sim::TypeRegistry& r = *s.registry;
    CHECK(r.types().size() == 10);
    using C = sim::RuntimeClass;
    // F2 / F6: weaver 6 / 11 / 5 t, grain farm 37 / 80.
    const sim::BuildingType& weaver = *r.find(s.weaver);
    CHECK(weaver.cls == C::Workshop);
    CHECK(weaver.prod.output_per_step[0] == 6);
    CHECK(weaver.prod.input_per_step[0] == 11);
    CHECK(weaver.prod.capacity[0] == 1280);
    const sim::BuildingType& farm = *r.find(s.farm);
    CHECK(farm.cls == C::Farm);
    CHECK(farm.prod.output_per_step[0] == 37);
    CHECK(farm.prod.input_per_step[0] == 80);
    CHECK(r.find(s.fur_trapper)->cls == C::FurTrapper);
    CHECK(r.find(s.small_farm)->cls == C::SmallFarm);
    CHECK(r.find(s.bow_maker)->cls == C::WeaponsWorkshop);
    CHECK(r.find(s.iron_mine)->cls == C::Mine);
    CHECK(r.find(s.pioneer_house)->cls == C::House);
    CHECK(r.find(s.kontor)->cls == C::Market);
    CHECK(r.find(s.market_stall)->cls == C::MarketStall);
    CHECK(r.find(s.road)->prodtype == 193);
    // The ids are in a range Anlagen.dat does not use (1..3998, none above 9000).
    for (const sim::BuildingType& t : r.types()) {
        CHECK(t.id.v >= 9001);
    }
    // The lists the types go to.
    const sim::ListLayout layout = sim::ListLayout::original();
    CHECK(*layout.list_for(r.find(s.weaver)->prodtype) == layout.roles.production_list);
    CHECK(*layout.list_for(r.find(s.pioneer_house)->prodtype) == layout.roles.house_list);
    CHECK(*layout.list_for(r.find(s.kontor)->prodtype) == layout.roles.market_list);
    CHECK(*layout.list_for(r.find(s.market_stall)->prodtype) == layout.roles.stall_list);
    CHECK_FALSE(layout.list_for(r.find(s.road)->prodtype).has_value());
}

namespace {

std::vector<std::uint8_t> read_anlagen(const std::filesystem::path& dir) {
    std::ifstream in(dir / "Anlagen.dat", std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
}

} // namespace

TEST_CASE("registry builds all 921 entries of Anlagen.dat") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = read_anlagen(*dir);
    auto file = fmt::anlagen::AnlagenFile::parse(buf);
    REQUIRE(file);
    auto built =
        sim::TypeRegistry::build(file.value().entries, sim::original_goods(), sim::SimConfig{});
    REQUIRE(built);
    const sim::TypeRegistry& r = *built.value();
    REQUIRE(r.types().size() == 921);
    // Ascending ids 1..3998, each entry produced a type.
    for (std::size_t i = 0; i < r.types().size(); ++i) {
        CHECK(r.types()[i].id.v == file.value().entries[i].id);
    }
    // The class tally against the PRODTYPE strings of the entries.
    std::map<std::string, std::size_t> by_prodtype;
    std::map<sim::RuntimeClass, std::size_t> by_class;
    std::size_t not_buildings = 0;
    for (const sim::BuildingType& t : r.types()) {
        const fmt::figuren::Entry* e = file.value().entry(t.id.v);
        REQUIRE(e != nullptr);
        const std::string name = e->record_str("PRODTYPE").value_or("");
        ++by_prodtype[name];
        CHECK(t.prodtype == *sim::prodtype_by_name(name));
        const auto cls = sim::class_for_prodtype(t.prodtype);
        if (cls) {
            CHECK(t.cls == *cls);
            ++by_class[t.cls];
        } else {
            CHECK(t.cls == sim::RuntimeClass::Plain);
            ++not_buildings;
        }
        CHECK(t.has_prod); // PROD3 of 68 bytes in every entry
        CHECK(t.head_raw.size() == 56);
        CHECK(t.running_cost.size() == 2);
        CHECK(t.width >= 1);
        CHECK(t.height >= 1);
        // The footprint agrees with the format layer's.
        const auto fp = file.value().footprint(t.id.v);
        REQUIRE(fp.has_value());
        CHECK(t.width == fp->first);
        CHECK(t.height == fp->second);
    }
    // Five entries are UNUSED (Appendix C: 101, 102, 103, 1003, 1007).
    for (const std::uint32_t id : {101U, 102U, 103U, 1003U, 1007U}) {
        CHECK(r.find(sim::TypeId{id})->prodtype == 0);
    }
    CHECK(by_prodtype["UNUSED"] == 5);
    std::size_t total = not_buildings;
    for (const auto& [cls, n] : by_class) {
        total += n;
    }
    CHECK(total == 921);
    // Spot classes by entry id.
    CHECK(r.find(sim::TypeId{809})->cls == sim::RuntimeClass::SmallFarm);
    CHECK(r.find(sim::TypeId{824})->cls == sim::RuntimeClass::FurTrapper);
    CHECK(r.find(sim::TypeId{1001})->cls == sim::RuntimeClass::Workshop);
    CHECK(r.find(sim::TypeId{1006})->cls == sim::RuntimeClass::WeaponsWorkshop);
    CHECK(r.find(sim::TypeId{1205})->cls == sim::RuntimeClass::Mine);
    CHECK(r.find(sim::TypeId{811})->cls == sim::RuntimeClass::Farm);
    // Build costs (docs/re/production.md §2.4; the weaver: 300 gold, 3 tools,
    // 4 wood; the palace 1380: 50 bricks, 15 wood, 40 tools, 20 marble, 15 000 gold).
    auto cost = [&](std::uint32_t id) {
        std::map<sim::GoodsId, std::uint32_t> c;
        for (const sim::CostEntry& e : r.find(sim::TypeId{id})->cost_goods) {
            c[e.goods] = e.amount_88 >> 8;
        }
        return c;
    };
    const sim::BuildingType& weaver = *r.find(sim::TypeId{1001});
    CHECK(weaver.cost_gold == 300);
    CHECK(cost(1001) == std::map<sim::GoodsId, std::uint32_t>{{0x29, 3}, {0x2a, 4}});
    const sim::BuildingType& palace = *r.find(sim::TypeId{1380});
    CHECK(palace.cost_gold == 15000);
    CHECK(cost(1380) ==
          std::map<sim::GoodsId, std::uint32_t>{{0x29, 40}, {0x2a, 15}, {0x2b, 50}, {0x2c, 20}});
    CHECK(palace.width == 4);
    CHECK(palace.height == 7);
    // FORSCHTYPE ids of Appendix B.
    const std::pair<std::uint32_t, std::uint32_t> research[] = {
        {744, 54},  {745, 54},  {823, 54},  {1337, 54}, {1005, 50}, {1006, 4},  {1010, 2},
        {1012, 38}, {1036, 3},  {1037, 1},  {1038, 21}, {1216, 57}, {1217, 57}, {1339, 58},
        {1341, 49}, {1342, 52}, {1350, 51}, {1378, 55}, {2516, 53}, {2517, 53}};
    std::size_t with_research = 0;
    for (const sim::BuildingType& t : r.types()) {
        if (t.research_id != 0) {
            ++with_research;
        }
    }
    CHECK(with_research == 20); // the 20 FORSCHTYPE records of Anlagen.dat
    for (const auto& [id, want] : research) {
        INFO("entry " << id);
        CHECK(r.find(sim::TypeId{id})->research_id == want);
    }
    // Fire data: max hit points and can-burn (docs/re/fire-and-ships.md §2.1).
    std::map<std::uint32_t, std::size_t> burn;
    for (const sim::BuildingType& t : r.types()) {
        ++burn[t.can_burn];
    }
    CHECK(burn[0] == 661);
    CHECK(burn[1] == 14);
    CHECK(burn[2] == 185);
    CHECK(burn[3] == 61);
    // Every entry that names a public-need good in WARE[0] has its mask; the
    // masks are single or merged bits below the 15 public goods.
    for (const sim::BuildingType& t : r.types()) {
        if (t.service_mask != 0) {
            CHECK(!t.output_goods.empty());
            CHECK(t.service_mask == sim::service_mask_for_goods(t.output_goods[0]));
            CHECK(t.service_mask < (1U << 15));
        }
    }
    // The registry's digest is stable across builds (and platforms: it is a
    // function of the parsed file only).
    auto again =
        sim::TypeRegistry::build(file.value().entries, sim::original_goods(), sim::SimConfig{});
    REQUIRE(again);
    CHECK(again.value()->digest() == r.digest());
    // T24: the digest covers the ENTRANCE points and the figure types (none here).
    CHECK(r.digest() == 0x626a29feff392800ULL);
    // The growing items (ROHSTWACHS / ROHSTDUERR): grain field 712 and forest 104.
    CHECK(r.find_growing(712) == r.find(sim::TypeId{712}));
    CHECK(r.find_growing(712)->prod.growth_period == 15500);
    CHECK(r.find_growing(104) != nullptr);
    CHECK(r.find_growing(1001) == nullptr);
    CHECK(r.max_footprint() >= 8); // the large castle 2223 is 8 x 8
}

TEST_CASE("registry build is deterministic and independent of entry order") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = read_anlagen(*dir);
    auto file = fmt::anlagen::AnlagenFile::parse(buf);
    REQUIRE(file);
    auto forward =
        sim::TypeRegistry::build(file.value().entries, sim::original_goods(), sim::SimConfig{});
    std::vector<fmt::figuren::Entry> reversed(file.value().entries.rbegin(),
                                              file.value().entries.rend());
    auto backward = sim::TypeRegistry::build(reversed, sim::original_goods(), sim::SimConfig{});
    REQUIRE(forward);
    REQUIRE(backward);
    CHECK(forward.value()->digest() == backward.value()->digest());
}
