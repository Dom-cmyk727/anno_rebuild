#include <algorithm>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>
#include <tuple>
#include <utility>

#include <doctest/doctest.h>

#include <anno/formats/annogame.hpp>
#include <anno/formats/sha256.hpp>
#include <anno/sim_load/wildlife_create.hpp>
#include <anno/sim_load/wildlife_release.hpp>
#include <anno/test/game_dir.hpp>

namespace fmt = anno::formats;
namespace sim = anno::sim;
namespace load = anno::sim_load;
namespace {
using Buffer = fmt::ByteBuffer;
void put16(Buffer& b, std::size_t at, std::uint16_t v) {
    b.at(at) = static_cast<std::uint8_t>(v);
    b.at(at + 1) = static_cast<std::uint8_t>(v >> 8);
}
void put32(Buffer& b, std::size_t at, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        b.at(at + i) = static_cast<std::uint8_t>(v >> (8U * i));
}
Buffer named_pe() {
    // Entirely authored PE/strings at the original profile's addressed windows.
    Buffer b(0x4200, 0);
    b[0] = 'M';
    b[1] = 'Z';
    put32(b, 0x3c, 0x40);
    b[0x40] = 'P';
    b[0x41] = 'E';
    put16(b, 0x44, 0x14c);
    put16(b, 0x46, 1);
    put32(b, 0x48, 0x44c0c30f);
    put16(b, 0x54, 0xe0);
    put16(b, 0x58, 0x10b);
    put32(b, 0x58 + 0x1c, 0x10000000);
    put32(b, 0x58 + 0x38, 0x2e000);
    constexpr auto section = 0x58 + 0xe0;
    put32(b, section + 8, 0x4000);
    put32(b, section + 12, 0x24000);
    put32(b, section + 16, 0x4000);
    put32(b, section + 20, 0x200);
    put32(b, section + 36, 0x40000040);
    const auto string = [&](std::uint32_t rva, std::string_view text) {
        std::copy(text.begin(), text.end(), b.begin() + (rva - 0x24000 + 0x200));
    };
    string(0x261e8, "UNUSED");
    for (const auto& [kind, rva, text] :
         std::vector<std::tuple<std::uint32_t, std::uint32_t, std::string>>{
             {49, 0x26020, "HIRSCH"}, {50, 0x26040, "RAUBTIER"}, {51, 0x26060, "HASE"}}) {
        put32(b, 0x24cb8 + kind * 4 - 0x24000 + 0x200, 0x10000000 + rva);
        string(rva, text);
    }
    string(0x26220, "MEAT");
    string(0x26240, "FUR");
    string(0x26260, "MOVED");
    string(0x26280, "OTHER");
    put32(b, 0x251d0 + 137 * 4 - 0x24000 + 0x200, 0x10026220);
    put32(b, 0x251d0 + 138 * 4 - 0x24000 + 0x200, 0x10026240);
    put32(b, 0x25234 + 9 * 4 - 0x24000 + 0x200, 0x10026260);
    put32(b, 0x251e4 + 142 * 4 - 0x24000 + 0x200, 0x10026280);
    return b;
}
struct Source {
    Buffer image = named_pe();
    std::deque<Buffer> storage;
    std::vector<fmt::figuren::Entry> entries;
    fmt::chunk::Chunk record(std::string_view name, Buffer value) {
        storage.push_back(std::move(value));
        fmt::chunk::Chunk c;
        c.name_field = *fmt::chunk::name_field(name);
        c.payload = storage.back();
        return c;
    }
    void add(std::uint32_t id, std::string_view kind, std::optional<Buffer> logic = {},
             std::optional<std::string> ware = {}) {
        fmt::figuren::Entry e;
        e.id = id;
        Buffer name(kind.begin(), kind.end());
        name.push_back(0);
        e.records.push_back(record("BEWOHNTYPE", std::move(name)));
        if (logic)
            e.records.push_back(record("LOGIC", std::move(*logic)));
        if (ware) {
            Buffer bytes(ware->begin(), ware->end());
            bytes.push_back(0);
            e.records.push_back(record("WARE", std::move(bytes)));
        }
        entries.push_back(std::move(e));
    }
    auto bind(std::vector<sim::TypeId> requested) const {
        return load::bind_wildlife_creation_descriptors(load::WildlifeDescriptorProfile::Gog,
                                                        requested, entries, image);
    }
};
Buffer hp(std::uint16_t value) {
    Buffer b(44, 0);
    put16(b, 30, value);
    return b;
}
load::WildlifeCreationInputs inputs() {
    load::WildlifeCreationInputs in;
    in.assigned_id = 9;
    in.creation_time = 65538;
    in.position_words = {0x3fa00000, 0x40200000, 0x40700000}; // authored 1.25,2.5,3.75
    in.geometry = {{0, 0, 64, 64}, 1, 3, load::WildlifeCreationPlace::Ordinary, 0};
    in.prior = {17, 9, 0x83, 255};
    in.sweep_time = 250000;
    in.helper_consumed = 2;
    return in;
}
std::vector<load::WildlifeCreationStage> success_stages() {
    using S = load::WildlifeCreationStage;
    return {S::ConstructorQuantity,
            S::AttachType,
            S::AttachWorld,
            S::SuppliedRegistration,
            S::SetTime,
            S::CopyPosition,
            S::SuppliedSpatialRegistration,
            S::SetHome,
            S::SetCargoGoods,
            S::AttachIsland,
            S::QueryYield,
            S::SaveId,
            S::SaveTimestamp,
            S::MarkOccupied,
            S::SaveYield};
}
} // namespace

TEST_CASE("wildlife create: owned descriptor facts preserve defaults kinds and segmented yields") {
    std::vector<load::WildlifeCreationDescriptor> owned;
    {
        Source s;
        s.add(127, "HIRSCH");
        s.add(128, "HIRSCH", Buffer{}, "MEAT");
        s.add(90001, "HIRSCH", hp(37), "MOVED");
        s.add(90002, "HASE", hp(0x1234), "FUR");
        const auto saved = s.image;
        auto facts = s.bind({sim::TypeId{90002}, sim::TypeId{127}, sim::TypeId{128},
                             sim::TypeId{90001}, sim::TypeId{127}});
        REQUIRE_MESSAGE(facts, (facts ? "" : facts.error()));
        CHECK(s.image == saved);
        owned = std::move(facts).value();
    }
    REQUIRE(owned.size() == 4);
    CHECK(owned[0].id().v == 127);
    CHECK(owned[0].kind() == 49);
    CHECK(owned[0].hp() == 5);
    CHECK(owned[0].yield() == 0);
    CHECK_FALSE(owned[0].logic_present());
    CHECK_FALSE(owned[0].ware_present());
    CHECK(owned[1].hp() == 0);
    CHECK(owned[1].yield() == 137);
    CHECK(owned[1].logic_present());
    CHECK(owned[2].id().v == 90001);
    CHECK(owned[2].hp() == 37);
    CHECK(owned[2].yield() == 9);
    CHECK(owned[3].kind() == 51);
    CHECK(owned[3].hp() == 0x1234);
    CHECK(owned[3].yield() == 138);
    CHECK(owned[0].image_sha256().size() == 64);
    CHECK(owned[0].source_sha256().size() == 64);
    for (std::size_t i : {std::size_t{2}, std::size_t{3}}) {
        const auto projected = load::project_wildlife_creation(owned[i], inputs());
        REQUIRE(projected);
        CHECK(projected.value().animal->type == owned[i].id());
        CHECK(projected.value().animal->quantity == 512);
        CHECK(projected.value().animal->yield == owned[i].yield());
    }
    Source short_source;
    Buffer short_logic(31, 0);
    short_logic[30] = 0x7f;
    short_source.add(3, "HIRSCH", short_logic, "OTHER");
    auto short_facts = short_source.bind({sim::TypeId{3}});
    REQUIRE(short_facts);
    CHECK(short_facts.value()[0].hp() == 127);
    CHECK(short_facts.value()[0].yield() == 142);
}

TEST_CASE("wildlife create: source and PE refusal siblings cannot relabel predators") {
    Source source;
    source.add(127, "HIRSCH", hp(37), "MEAT");
    REQUIRE(source.bind({sim::TypeId{127}}));
    const auto expect = [&](const Source& s, std::string_view reason) {
        const auto image = s.image;
        const auto result = s.bind({sim::TypeId{127}});
        REQUIRE_FALSE(result);
        CHECK(result.error() == reason);
        CHECK(s.image == image);
    };
    Source predator;
    predator.add(127, "RAUBTIER");
    predator.entries[0].records.push_back(
        predator.record("ROUTETYPE", {'H', 'I', 'R', 'S', 'C', 'H', 0}));
    expect(predator, "creation descriptor: unsupported animal kind");
    Source unknown;
    unknown.add(127, "NEVER_KNOWN");
    expect(unknown, "creation descriptor: unknown BEWOHNTYPE");
    Source unknown_ware;
    unknown_ware.add(127, "HIRSCH", {}, "UNKNOWN_WARE");
    expect(unknown_ware, "creation descriptor: unknown WARE");
    Source duplicate;
    duplicate.add(127, "HIRSCH");
    duplicate.add(127, "HIRSCH");
    expect(duplicate, "creation descriptor: duplicate entry id");
    Source records;
    records.add(127, "HIRSCH");
    records.entries[0].records.push_back(records.entries[0].records[0]);
    expect(records, "creation descriptor: unique needed records required");
    Source long_logic;
    long_logic.add(127, "HIRSCH", Buffer(45, 0));
    expect(long_logic, "creation descriptor: unsafe LOGIC length");
    Source bad_name;
    bad_name.add(127, "HIRSCH");
    bad_name.entries[0].records[0] = bad_name.record("BEWOHNTYPE", {'H', 'I', 'R', 'S', 'C', 'H'});
    expect(bad_name, "creation descriptor: unsafe BEWOHNTYPE name");
    Source flag;
    flag.add(127, "HIRSCH");
    flag.entries[0].flag = 1;
    expect(flag, "creation descriptor: unsupported entry flag");
    Source bad_ware;
    bad_ware.add(127, "HIRSCH");
    bad_ware.entries[0].records.push_back(bad_ware.record("WARE", {'M', 'E', 'A', 'T'}));
    expect(bad_ware, "creation descriptor: unsafe WARE name");
    Source duplicate_logic;
    duplicate_logic.add(127, "HIRSCH", Buffer{});
    duplicate_logic.entries[0].records.push_back(duplicate_logic.record("LOGIC", Buffer{}));
    expect(duplicate_logic, "creation descriptor: unique needed records required");
    Source duplicate_ware;
    duplicate_ware.add(127, "HIRSCH", {}, "MEAT");
    duplicate_ware.entries[0].records.push_back(duplicate_ware.record("WARE", {'F', 'U', 'R', 0}));
    expect(duplicate_ware, "creation descriptor: unique needed records required");
    for (std::size_t length : {std::size_t{155}, std::size_t{156}}) {
        Source long_mapping;
        long_mapping.add(127, "HIRSCH", {}, std::string(length, 'L'));
        put32(long_mapping.image, 0x251d0 + 137 * 4 - 0x24000 + 0x200, 0x10027000);
        std::fill_n(long_mapping.image.begin() + 0x3200, length, std::uint8_t{'L'});
        if (length == 155) {
            const auto legal_long = long_mapping.bind({sim::TypeId{127}});
            REQUIRE(legal_long);
            CHECK(legal_long.value()[0].yield() == 137);
        } else {
            expect(long_mapping, "wildlife names: lookup name exceeds safe original buffer");
        }
    }
    Source wrong_map;
    wrong_map.add(127, "RAUBTIER");
    put32(wrong_map.image, 0x24cb8 + 49 * 4 - 0x24000 + 0x200, 0x10026040);
    expect(wrong_map, "wildlife names: unsupported class mapping");
    Source writable;
    writable.add(127, "HIRSCH");
    put32(writable.image, 0x58 + 0xe0 + 36, 0x80000040);
    expect(writable, "wildlife names: section must be readable");
    Source truncated;
    truncated.add(127, "HIRSCH");
    truncated.image.resize(0x300);
    expect(truncated, "wildlife names: truncated backing section");
    Source unsafe;
    unsafe.add(127, "HIRSCH");
    put32(unsafe.image, 0x24cb8 + 49 * 4 - 0x24000 + 0x200, 0x20000000);
    expect(unsafe, "wildlife names: invalid string pointer");
    CHECK_FALSE(source.bind({sim::TypeId{999}}));
    CHECK_FALSE(source.bind({}));
    CHECK_FALSE(load::bind_wildlife_creation_descriptors(
        static_cast<load::WildlifeDescriptorProfile>(99), std::array{sim::TypeId{127}},
        source.entries, source.image));
    CHECK(source.bind({sim::TypeId{127}}));
}

TEST_CASE("wildlife create: defined effects match hand fields stages and null bookkeeping") {
    Source source;
    auto with_capacity = hp(37);
    with_capacity[0x11] = 4; // Type capacity 1024, not initial quantity 512.
    source.add(127, "HIRSCH", with_capacity, "MEAT");
    source.add(128, "HIRSCH", hp(0), "FUR");
    auto facts = source.bind({sim::TypeId{127}, sim::TypeId{128}});
    REQUIRE(facts);
    auto in = inputs();
    const auto saved = in;
    auto result = load::project_wildlife_creation(facts.value()[0], in);
    REQUIRE(result);
    REQUIRE(result.value().animal);
    const auto& a = *result.value().animal;
    CHECK(a.type.v == 127);
    CHECK(a.engine_id == 9);
    CHECK(a.original_id == 9);
    CHECK(a.full_creation_time == 65538);
    CHECK(a.hp == 37);
    CHECK(a.original_times == std::array<std::uint16_t, 3>{2, 2, 2});
    CHECK(a.position_words == std::array<std::uint32_t, 3>{0x3fa00000, 0x40200000, 0x40700000});
    CHECK(a.second_position_words == a.position_words);
    CHECK(a.quantity == 512);
    CHECK(a.carried_goods == 69);
    CHECK(a.yield == 137);
    CHECK(a.home == std::array<std::uint32_t, 2>{1, 3});
    CHECK(a.original_home == std::array<std::uint16_t, 2>{1, 3});
    CHECK(a.original_current_tile == a.original_home);
    CHECK(a.flags == 8);
    CHECK(a.action == 19);
    CHECK(a.requested == 27);
    CHECK(a.current == 0);
    CHECK(a.behavior == 4);
    CHECK(a.configuration == 4);
    CHECK_FALSE(a.first_action_started);
    CHECK(result.value().spawn == load::WildlifeSpawnProjection{9, 54, 0xc3, 137});
    CHECK(result.value().stages == success_stages());
    CHECK(result.value().helper_consumed == 2);
    CHECK(in.position_words == saved.position_words);
    CHECK(in.prior == saved.prior);
    CHECK(load::project_wildlife_creation(facts.value()[0], in).value() == result.value());
    const auto special = load::project_wildlife_creation(facts.value()[1], in);
    REQUIRE(special);
    CHECK(special.value().animal->quantity == 2048);
    CHECK(special.value().animal->hp == 0);
    for (auto id : {0U, 65535U}) {
        in.assigned_id = id;
        auto edge = load::project_wildlife_creation(facts.value()[0], in);
        REQUIRE(edge);
        CHECK(edge.value().animal->engine_id == id);
        CHECK(edge.value().animal->original_id == id);
    }
    in = inputs();
    in.geometry.place = load::WildlifeCreationPlace::ResolvedCaveEntrance;
    in.geometry.entrance_count = 1;
    in.geometry.home_x = 5;
    in.geometry.home_z = 6;
    auto cave = load::project_wildlife_creation(facts.value()[0], in);
    REQUIRE(cave);
    CHECK(cave.value().animal->home == std::array<std::uint32_t, 2>{5, 6});
    CHECK(cave.value().animal->position_words == saved.position_words);
    in = inputs();
    in.manager = load::WildlifeManagerOutcome::Null;
    auto null = load::project_wildlife_creation(facts.value()[0], in);
    REQUIRE(null);
    CHECK_FALSE(null.value().animal);
    CHECK(null.value().spawn == in.prior);
    CHECK(null.value().helper_consumed == 2);
    CHECK(null.value().stages == std::vector{load::WildlifeCreationStage::ManagerNull});
}

TEST_CASE("wildlife create: geometry refuses inconsistent ordinary homes and unsafe projections") {
    Source s;
    s.add(127, "HIRSCH");
    auto facts = s.bind({sim::TypeId{127}});
    REQUIRE(facts);
    auto control = load::project_wildlife_creation(facts.value()[0], inputs());
    REQUIRE(control);
    const auto expect = [&](const load::WildlifeCreationInputs& in, std::string_view error) {
        const auto saved = in.position_words;
        auto result = load::project_wildlife_creation(facts.value()[0], in);
        REQUIRE_FALSE(result);
        CHECK(result.error() == error);
        CHECK(in.position_words == saved);
    };
    auto in = inputs();
    in.geometry.home_x = 10;
    in.geometry.home_z = 20;
    expect(in, "creation effects: inconsistent ordinary home");
    in = inputs();
    in.assigned_id = 65536;
    expect(in, "creation effects: id outside original projection");
    for (auto bits : {0x7f800000U, 0x7fc00000U}) {
        in = inputs();
        in.position_words[1] = bits;
        expect(in, "creation effects: nonfinite position");
    }
    in = inputs();
    in.position_words[0] = 0xbf800000;
    expect(in, "creation effects: unsupported coordinate");
    in = inputs();
    in.position_words[0] = 0x4f000000;
    expect(in, "creation effects: coordinate outside integer projection");
    in = inputs();
    in.geometry.area = {0, 0, 1, 1};
    expect(in, "creation effects: outside supplied geometry");
    in = inputs();
    in.geometry.place = load::WildlifeCreationPlace::ResolvedCaveEntrance;
    expect(in, "creation effects: OQ-574 cave without entrance");
    in = inputs();
    in.geometry.place = load::WildlifeCreationPlace::ResolvedCaveEntrance;
    in.geometry.entrance_count = 1;
    in.geometry.area = {0, 0, 20000000, 64};
    in.geometry.home_x = 16777216;
    auto exact_cave = load::project_wildlife_creation(facts.value()[0], in);
    REQUIRE(exact_cave);
    CHECK(exact_cave.value().animal->home[0] == 16777216);
    in.geometry.home_x = 16777217;
    expect(in, "creation effects: cave target needs unproved rounding");
    for (auto bits : {0U, 0x80000000U, 1U, 0x3f7fffffU}) {
        in = inputs();
        in.position_words[0] = bits;
        in.geometry.home_x = 0;
        auto small = load::project_wildlife_creation(facts.value()[0], in);
        REQUIRE(small);
        CHECK(small.value().animal->home[0] == 0);
    }
    in = inputs();
    in.position_words[0] = 0x47800080; // 65537 exactly: wide home retained, legacy word 1.
    in.geometry.home_x = 65537;
    in.geometry.area = {0, 0, 70000, 64};
    auto wide = load::project_wildlife_creation(facts.value()[0], in);
    REQUIRE(wide);
    CHECK(wide.value().animal->home[0] == 65537);
    CHECK(wide.value().animal->original_home[0] == 1);
}

TEST_CASE("wildlife create: complete owned result survives all descriptor owners") {
    std::optional<load::WildlifeCreationEffects> owned;
    std::string source_hash;
    {
        Source s;
        s.add(127, "HIRSCH", hp(37), "MEAT");
        const auto facts = s.bind({sim::TypeId{127}});
        REQUIRE(facts);
        source_hash = facts.value()[0].source_sha256();
        const auto projected = load::project_wildlife_creation(facts.value()[0], inputs());
        REQUIRE(projected);
        owned = projected.value();
    }
    REQUIRE(owned);
    REQUIRE(owned->animal);
    CHECK(owned->animal->descriptor_source_sha256 == source_hash);
    CHECK(owned->animal->quantity == 512);
    CHECK(owned->animal->hp == 37);
    CHECK(owned->animal->position_words == inputs().position_words);
    CHECK(owned->spawn == load::WildlifeSpawnProjection{9, 54, 0xc3, 137});
    CHECK(owned->stages == success_stages());
}

TEST_CASE("wildlife create: original descriptor projections agree with independent raw oracle") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const auto image = anno::test::read_file(*dir / "AnnoGame.dll");
    const auto data = anno::test::read_file(*dir / "FigurenHeader.dat");
    REQUIRE(fmt::sha256_hex(image) ==
            "ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc");
    REQUIRE(fmt::sha256_hex(data) ==
            "29fa1f0528fb337d9a61e8b73169281fd05bb27efa6df65638580a4c36c468a8");
    const auto parsed = fmt::figuren::FigurenHeader::parse(data);
    REQUIRE(parsed);
    REQUIRE(parsed.value().entries.size() == 368);
    const auto mapping = fmt::annogame::read_wildlife_names(image);
    REQUIRE_MESSAGE(mapping, (mapping ? "" : mapping.error()));
    std::vector<sim::TypeId> selected;
    for (const auto& e : parsed.value().entries) {
        const auto raw = e.record_str("BEWOHNTYPE");
        if (raw && (*raw == mapping.value().kinds[49] || *raw == mapping.value().kinds[51]))
            selected.push_back(sim::TypeId{e.id});
    }
    REQUIRE(selected.size() == 29);
    const auto facts = load::bind_wildlife_creation_descriptors(
        load::WildlifeDescriptorProfile::Gog, selected, parsed.value().entries, image);
    REQUIRE_MESSAGE(facts, (facts ? "" : facts.error()));
    std::ostringstream serial;
    std::uint32_t count49 = 0, count51 = 0, zero_hp = 0;
    for (const auto& d : facts.value()) {
        serial << d.id().v << '\t' << d.kind() << '\t' << d.hp() << '\t' << d.yield() << '\n';
        count49 += d.kind() == 49;
        count51 += d.kind() == 51;
        zero_hp += d.hp() == 0;
        auto in = inputs();
        auto projected = load::project_wildlife_creation(d, in);
        REQUIRE(projected);
        REQUIRE(projected.value().animal);
        const auto& a = *projected.value().animal;
        CHECK(a.type == d.id());
        CHECK(a.hp == d.hp());
        CHECK(a.yield == d.yield());
        CHECK(a.quantity ==
              ((d.id().v == 128 || d.id().v == 129 || d.id().v == 1206) ? 2048 : 512));
        CHECK(a.engine_id == 9);
        CHECK(a.original_times == std::array<std::uint16_t, 3>{2, 2, 2});
        CHECK(a.home == std::array<std::uint32_t, 2>{1, 3});
        CHECK(a.position_words == in.position_words);
        CHECK(a.carried_goods == 69);
        CHECK(a.current == 0);
        CHECK_FALSE(a.first_action_started);
        CHECK(projected.value().spawn ==
              load::WildlifeSpawnProjection{9, 54, 0xc3, static_cast<std::uint16_t>(d.yield())});
        CHECK(projected.value().stages == success_stages());
        CHECK(projected.value().helper_consumed == 2);
    }
    CHECK(count49 == 29);
    CHECK(count51 == 0);
    CHECK(zero_hp == 4);
    const auto text = serial.str();
    const Buffer bytes(text.begin(), text.end());
    // Pre-C++ independent pefile/figuren parse; source digest, not a state pin.
    CHECK(fmt::sha256_hex(bytes) ==
          "69bf63f7acb864820fcddff8eda640ab28c1a9d82ecd0abd6f6c2751d11cf073");
    if (const auto* path = std::getenv("ANNO_WILDLIFE_CREATE_ORACLE"); path && *path) {
        const auto oracle = anno::test::read_file(path);
        CHECK(text == std::string(oracle.begin(), oracle.end()));
    } else
        MESSAGE("private full-row oracle not supplied; independent digest comparison only");
    std::cout << "REAL_DESCRIPTOR_PROJECTION entries=368 selected=29 kind49=29 kind51=0 zero_hp=4 "
                 "helper_input_draws=2 new_draws=0 original_runtime=NOT_RUN\n";
}

namespace {
load::WildlifeCreationInputs end_creation_inputs() {
    auto in = inputs();
    in.position_words = {0x40400000, 0, 0}; // Authored x = 3, z = 0.
    in.geometry.home_x = 3;
    in.geometry.home_z = 0;
    return in;
}
load::WildlifeReleaseIsland release_island(const load::WildlifeSpawnProjection& home) {
    return {0, 0, 2, 2, {{0, 0, 1, {{0, home}}}, {1, 1, 1, {{0, {77, 13, 0xc0, 138}}}}}};
}
} // namespace
TEST_CASE("wildlife end: composed creation moves current position but releases original home") {
    Source source;
    source.add(90001, "HIRSCH", hp(12), "MEAT");
    auto facts = source.bind({sim::TypeId{90001}});
    REQUIRE(facts);
    auto created = load::project_wildlife_creation(facts.value()[0], end_creation_inputs());
    REQUIRE(created);
    REQUIRE(created.value().animal);
    CHECK(created.value().spawn.packed == 0xc3);
    load::WildlifeEndState in{*created.value().animal};
    // Authored movement only; no motion simulation. Widened home is deliberately
    // distinct too: the original query reads its saved low-word projection.
    in.animal.position_words = {0x41000000, 0, 0x41000000}; // 8,0,8
    in.animal.original_current_tile = {8, 8};
    in.animal.home = {65539, 65536};
    in.animal.flags = 0x8808;
    const auto before = in;
    const auto island = release_island(created.value().spawn);
    const auto saved = island;
    auto result = load::project_wildlife_end(facts.value()[0], in, island);
    REQUIRE(result);
    auto expected = island;
    expected.chunks[0].records[0].spawn.packed = 0x83;
    CHECK(result.value().island == expected);
    CHECK(result.value().island.chunks[1] == island.chunks[1]);
    CHECK(result.value().island.chunks[0].records[0].spawn.id == 9);
    CHECK(result.value().island.chunks[0].records[0].spawn.timestamp == 54);
    CHECK(result.value().island.chunks[0].records[0].spawn.goods == 137);
    CHECK(result.value().state.animal.flags == 0x8008);
    auto expected_state = in;
    expected_state.animal.flags = 0x8008;
    expected_state.animal.requested = 0;
    expected_state.animal.second_position_words = in.animal.position_words;
    expected_state.attached = false;
    expected_state.ended = true;
    CHECK(result.value().state == expected_state);
    using S = load::WildlifeEndStage;
    CHECK(result.value().stages == std::vector<S>{S::EmptyFollowers, S::ResetAction, S::EmptyMotion,
                                                  S::EmptyRoute, S::IslandRelease, S::HomeLookup,
                                                  S::ClearOccupancy, S::DetachIsland});
    CHECK(in == before);
    CHECK(island == saved);
    CHECK(load::project_wildlife_end(facts.value()[0], in, island).value() == result.value());
}
TEST_CASE("wildlife end: ordered category lookup clears only occupancy without saved ID guard") {
    Source source;
    source.add(90002, "HASE");
    auto facts = source.bind({sim::TypeId{90002}});
    REQUIRE(facts);
    const auto created = load::project_wildlife_creation(facts.value()[0], end_creation_inputs());
    REQUIRE(created);
    load::WildlifeEndState in{*created.value().animal};
    auto island = release_island({65535, 255, 0xc3, 65535}); // Not this animal's ID.
    // Wrong category first, wrong local position second, two matches after them.
    island.chunks[0].records = {{1, {1, 1, 0xc3, 1}},
                                {0, {2, 2, 0xc2, 2}},
                                {0, {65535, 255, 0xc3, 65535}},
                                {0, {4, 4, 0xc3, 4}}};
    auto expected = island;
    expected.chunks[0].records[2].spawn.packed = 0x83;
    auto ended = load::project_wildlife_end(facts.value()[0], in, island);
    REQUIRE(ended);
    CHECK(ended.value().island == expected);
    for (unsigned sibling = 0; sibling < 4; ++sibling) {
        auto other = island;
        if (sibling == 0)
            other.chunks[0].categories = 0; // Physical record exists, category gate absent.
        if (sibling == 1)
            other.chunks[0].records.clear();
        if (sibling == 2)
            for (auto& r : other.chunks[0].records)
                r.category = 1;
        if (sibling == 3) {
            other.chunks[0].records.resize(1);
            other.chunks[0].records[0] = {0, {65535, 255, 0x83, 65535}}; // Already clear.
        }
        const auto result = load::project_wildlife_end(facts.value()[0], in, other);
        REQUIRE(result);
        CHECK(result.value().island == other);
        CHECK_FALSE(result.value().state.attached);
    }
    // Unaligned origin: signed difference truncates toward zero; local remainder
    // uses absolute home (8 % 8), not relative home ((8 - 9) % 8).
    in.animal.original_home = {8, 8};
    auto unaligned = release_island({5, 7, 0xc0, 9});
    unaligned.origin_x = 9;
    unaligned.origin_z = 9;
    const auto result = load::project_wildlife_end(facts.value()[0], in, unaligned);
    REQUIRE(result);
    CHECK(result.value().island.chunks[0].records[0].spawn ==
          load::WildlifeSpawnProjection{5, 7, 0x80, 9});
}
TEST_CASE("wildlife end: repeat detached end and direct removal contrast do not release again") {
    Source source;
    source.add(128, "HIRSCH");
    auto facts = source.bind({sim::TypeId{128}});
    REQUIRE(facts);
    const auto created = load::project_wildlife_creation(facts.value()[0], end_creation_inputs());
    REQUIRE(created);
    const load::WildlifeEndState in{*created.value().animal};
    auto island = release_island(created.value().spawn);
    auto direct = load::project_wildlife_end(facts.value()[0], in, island,
                                             load::WildlifeEndOperation::DirectRemovalContrast);
    REQUIRE(direct);
    CHECK(direct.value().island == island);
    CHECK(direct.value().state == in); // Contrast does not actually destroy/detach.
    CHECK(direct.value().stages == std::vector{load::WildlifeEndStage::DirectRemovalNoRelease});
    auto first = load::project_wildlife_end(facts.value()[0], in, island);
    REQUIRE(first);
    island.chunks[0].records[0].spawn = {444, 88, 0xc3, 138}; // Replacement at same home.
    auto second = load::project_wildlife_end(facts.value()[0], first.value().state, island);
    REQUIRE(second);
    CHECK(second.value().island == island);
    CHECK(second.value().state == first.value().state);
    CHECK(second.value().stages.size() == 4); // Only empty local cleanup.
    auto stale = load::project_wildlife_end(facts.value()[0], in, island);
    REQUIRE(stale);
    CHECK(stale.value().island.chunks[0].records[0].spawn.packed == 0x83);
    auto after =
        load::project_wildlife_end(facts.value()[0], first.value().state, first.value().island,
                                   load::WildlifeEndOperation::DirectRemovalContrast);
    REQUIRE(after);
    CHECK(after.value().island == first.value().island);
}
TEST_CASE("wildlife end: unsupported live profiles refuse and returned effects own their inputs") {
    std::optional<load::WildlifeEndEffects> owned;
    {
        Source source;
        source.add(90001, "HIRSCH");
        auto facts = source.bind({sim::TypeId{90001}});
        REQUIRE(facts);
        const auto created =
            load::project_wildlife_creation(facts.value()[0], end_creation_inputs());
        REQUIRE(created);
        const load::WildlifeEndState legal{*created.value().animal};
        const auto island = release_island({19, 7, 0xc3, 138});
        auto good = load::project_wildlife_end(facts.value()[0], legal, island);
        REQUIRE(good);
        owned = good.value();
        for (unsigned variant = 0; variant < 10; ++variant) {
            auto bad = legal;
            if (variant == 0)
                bad.animal.configuration = 2;
            if (variant == 1)
                bad.followers = 1;
            if (variant == 2)
                bad.motion_entries = 1;
            if (variant == 3)
                bad.motion_cursor = true;
            if (variant == 4)
                bad.route = true;
            if (variant == 5)
                bad.animal.current = 25;
            if (variant == 6)
                bad.animal.current = 26;
            if (variant == 7)
                bad.animal.first_action_started = true;
            if (variant == 8)
                bad.animal.type = sim::TypeId{2};
            if (variant == 9)
                bad.ended = true;
            const auto copy = bad;
            REQUIRE_FALSE(load::project_wildlife_end(facts.value()[0], bad, island));
            CHECK(bad == copy);
        }
        auto duplicate = island;
        duplicate.chunks.push_back(duplicate.chunks[0]);
        REQUIRE_FALSE(load::project_wildlife_end(facts.value()[0], legal, duplicate));
        auto outside = legal;
        outside.animal.original_home = {65535, 65535};
        REQUIRE_FALSE(load::project_wildlife_end(facts.value()[0], outside, island));
        REQUIRE_FALSE(load::project_wildlife_end(facts.value()[0], legal, island,
                                                 static_cast<load::WildlifeEndOperation>(99)));
    }
    REQUIRE(owned);
    CHECK(owned->state.animal.type.v == 90001);
    CHECK(owned->state.animal.descriptor_source_sha256.size() == 64);
    CHECK(owned->island.chunks[0].records[0].spawn ==
          load::WildlifeSpawnProjection{19, 7, 0x83, 138});
    CHECK(owned->island.chunks[1].records[0].spawn ==
          load::WildlifeSpawnProjection{77, 13, 0xc0, 138});
}
