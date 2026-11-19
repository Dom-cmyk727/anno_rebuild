#include <algorithm>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <tuple>
#include <utility>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/annogame.hpp>
#include <anno/formats/sha256.hpp>
#include <anno/formats/tagged.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/wildlife_create.hpp>
#include <anno/sim_load/wildlife_final_facts.hpp>
#include <anno/sim_load/wildlife_geometry.hpp>
#include <anno/sim_load/wildlife_habitat_load.hpp>
#include <anno/sim_load/wildlife_import_plan.hpp>
#include <anno/sim_load/wildlife_materialize.hpp>
#include <anno/sim_load/wildlife_release.hpp>
#include <anno/sim_load/wildlife_species.hpp>
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

namespace {
using SpeciesProfile = fmt::annowelt::WildlifeSpeciesProfile;
Buffer species_pe(std::vector<std::uint16_t> ids = {}) {
    if (ids.empty()) {
        ids.assign(32, 60001);
        ids[1] = 127;
        ids[31] = 503;
    }
    Buffer b(0x1200, 0);
    b[0] = 'M';
    b[1] = 'Z';
    put32(b, 0x3c, 0x40);
    b[0x40] = 'P';
    b[0x41] = 'E';
    put16(b, 0x44, 0x14c);
    put16(b, 0x46, 1);
    put32(b, 0x48, fmt::annowelt::kGogTimeDateStamp);
    put16(b, 0x54, 0xe0);
    put16(b, 0x58, 0x10b);
    put32(b, 0x74, 0x10000000);
    put32(b, 0x90, fmt::annowelt::kGogSizeOfImage);
    constexpr auto sh = 0x138;
    put32(b, sh + 8, 0x1000);
    put32(b, sh + 12, 0xca000);
    put32(b, sh + 16, 0x1000);
    put32(b, sh + 20, 0x200);
    put32(b, sh + 36, 0x40000040);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        put16(b, 0x200 + 0x780 + 2 * i, ids[i]);
        put16(b, 0x200 + 0x740 + 2 * i, 127); // Distinct adjacent row, not row 2.
    }
    return b;
}
void species_source(Source& s) {
    s.add(60001, "HASE", hp(17), "MEAT");
    s.add(127, "HIRSCH", {}, "FUR");
    s.add(503, "RAUBTIER", {}, "MOVED");
}
auto species_plan(const Source& source, const Buffer& welt, bool cave, sim::GoodsId saved,
                  std::span<const std::uint32_t> draws, std::size_t entrances = 1) {
    return load::prepare_wildlife_creation_plan(welt, source.image, source.entries,
                                                SpeciesProfile::GogClimate2, 2, cave, entrances,
                                                saved, draws);
}
} // namespace
TEST_CASE("wildlife species: raw order duplicate slots numeric facts and exact selection") {
    Source s;
    species_source(s);
    const auto welt = species_pe();
    const auto raw =
        fmt::annowelt::read_wildlife_species_ring(welt, SpeciesProfile::GogClimate2, 2);
    REQUIRE(raw);
    REQUIRE(raw.value().ids.size() == 32);
    CHECK(raw.value().ids[0] == 60001);
    CHECK(raw.value().ids[1] == 127);
    CHECK(raw.value().ids[2] == 60001);
    CHECK(raw.value().ids[31] == 503);
    CHECK(raw.value().rva == 0xca780);
    CHECK(raw.value().raw_offset == 0x980);
    const std::array<std::uint32_t, 3> ordinary{6, 33, 999};
    const auto before = s.image;
    const auto stored_before = s.storage;
    const auto welt_before = welt;
    const auto plan = species_plan(s, welt, false, 255, ordinary);
    REQUIRE(plan);
    CHECK(plan.value().ring.ids == raw.value().ids);
    REQUIRE(plan.value().slot_facts.size() == 32);
    CHECK(plan.value().slot_facts[0] == plan.value().slot_facts[2]);
    CHECK(plan.value().slot_facts[31].kind == 50);
    CHECK(plan.value().slot_facts[31].yield == 9);
    CHECK(plan.value().choice.value.position == 2);
    CHECK(plan.value().choice.value.index == 1);
    CHECK(plan.value().choice.value.species.v == 127);
    CHECK(plan.value().choice.value.inspected == 1);
    CHECK(plan.value().choice.consumed == 2);
    CHECK(plan.value().selected.yield == 138);
    REQUIRE(plan.value().creation);
    CHECK(plan.value().creation->yield() == 138);
    const std::array<std::uint32_t, 2> cave{31, 999};
    const auto wrap = species_plan(s, welt, true, 137, cave);
    REQUIRE(wrap);
    CHECK(wrap.value().choice.value.position == -1);
    CHECK(wrap.value().choice.value.index == 0);
    CHECK(wrap.value().choice.value.species.v == 60001);
    CHECK(wrap.value().choice.value.inspected == 2);
    CHECK(wrap.value().choice.consumed == 1);
    CHECK(wrap.value().selected.kind == 51);
    REQUIRE(wrap.value().creation);
    CHECK(wrap.value().creation->id().v == 60001);
    const auto unsupported = species_plan(s, welt, true, 9, cave);
    REQUIRE(unsupported);
    CHECK(unsupported.value().disposition ==
          load::WildlifePlanDisposition::UnsupportedCreationKind);
    CHECK_FALSE(unsupported.value().creation);
    CHECK(unsupported.value().selected.kind == 50);
    CHECK(unsupported.value().choice.value.index == 31);
    CHECK(unsupported.value().choice.value.species.v == 503);
    CHECK(unsupported.value().choice.value.inspected == 1);
    CHECK(unsupported.value().choice.consumed == 1);
    const auto failed_match = species_plan(s, welt, true, 142, cave);
    REQUIRE(failed_match);
    CHECK(failed_match.value().choice.value.index == 16);
    CHECK(failed_match.value().choice.value.inspected == 18);
    CHECK(failed_match.value().choice.consumed == 1);
    CHECK(failed_match.value().selected.yield == 137); // Still selected despite no goods match.
    CHECK(s.image == before);
    CHECK(s.storage == stored_before);
    CHECK(welt == welt_before);
    CHECK(ordinary == std::array<std::uint32_t, 3>{6, 33, 999});
    CHECK(cave == std::array<std::uint32_t, 2>{31, 999});
    const auto again = species_plan(s, welt, false, 255, ordinary);
    REQUIRE(again);
    CHECK(again.value().slot_facts == plan.value().slot_facts);
    CHECK(again.value().choice.value.index == plan.value().choice.value.index);
    CHECK(again.value().choice.consumed == plan.value().choice.consumed);
}
TEST_CASE("wildlife species: selector exhaustion stays distinct from source and creation failure") {
    Source s;
    species_source(s);
    const auto welt = species_pe();
    const std::array<std::uint32_t, 1> one{31};
    for (const auto cave : {false, true}) {
        const auto empty = species_plan(s, welt, cave, 255, {});
        REQUIRE_FALSE(empty);
        CHECK(empty.error().stage == load::WildlifePlanFault::Selector);
        CHECK(empty.error().selector.fault == sim::WildlifeFault::DrawsExhausted);
        CHECK(empty.error().selector.consumed == 0);
    }
    const auto short_ordinary = species_plan(s, welt, false, 255, one);
    REQUIRE_FALSE(short_ordinary);
    CHECK(short_ordinary.error().selector.fault == sim::WildlifeFault::DrawsExhausted);
    CHECK(short_ordinary.error().selector.consumed == 1);
    const auto no_entrance = species_plan(s, welt, true, 255, one, 0);
    REQUIRE_FALSE(no_entrance);
    CHECK(no_entrance.error().selector.fault == sim::WildlifeFault::Oq574CaveWithoutEntrance);
    CHECK(no_entrance.error().selector.consumed == 0);
    REQUIRE(species_plan(s, welt, true, 255, one));
    s.entries.pop_back(); // Missing unselected descriptor must still refuse source first.
    const auto missing = species_plan(s, welt, false, 255, {});
    REQUIRE_FALSE(missing);
    CHECK(missing.error().stage == load::WildlifePlanFault::Source);
    CHECK(missing.error().source == "creation descriptor: missing entry");
    CHECK(missing.error().selector.consumed == 0);
}
TEST_CASE("wildlife species: own row profile and safe source refusal controls") {
    Source s;
    species_source(s);
    const auto welt = species_pe();
    const auto read = [](const Buffer& b) {
        return fmt::annowelt::read_wildlife_species_ring(b, SpeciesProfile::GogClimate2, 2);
    };
    REQUIRE(read(welt));
    for (auto climate : {0U, 1U, 3U, 255U})
        REQUIRE_FALSE(
            fmt::annowelt::read_wildlife_species_ring(welt, SpeciesProfile::GogClimate2, climate));
    REQUIRE_FALSE(
        fmt::annowelt::read_wildlife_species_ring(welt, static_cast<SpeciesProfile>(99), 2));
    for (unsigned variant = 0; variant < 7; ++variant) {
        auto bad = welt;
        if (variant == 0)
            put32(bad, 0x48, 0);
        if (variant == 1)
            put32(bad, 0x138 + 8, 0x7bf); // One byte short initialized virtual range.
        if (variant == 2)
            put32(bad, 0x138 + 16, 0x7bf); // One byte short raw range.
        if (variant == 3) {
            put16(bad, 0x46, 2);
            std::copy_n(bad.begin() + 0x138, 40, bad.begin() + 0x160);
        }
        if (variant == 4)
            put32(bad, 0x138 + 36, 0xc0000040);
        if (variant == 5)
            put16(bad, 0x44, 0x8664);
        if (variant == 6)
            bad.pop_back();
        REQUIRE_FALSE(read(bad));
    }
    const std::array<std::uint32_t, 2> draws{0, 0};
    auto zero = welt;
    put16(zero, 0x980 + 31 * 2, 0);
    const auto zero_ring = read(zero);
    REQUIRE(zero_ring);
    CHECK(zero_ring.value().ids[31] == 0); // Reader retains; binder refuses absent descriptor 0.
    REQUIRE_FALSE(species_plan(s, zero, false, 255, draws));
    s.entries.push_back(s.entries[0]);
    const auto duplicate = species_plan(s, welt, false, 255, draws);
    REQUIRE_FALSE(duplicate);
    CHECK(duplicate.error().source == "creation descriptor: duplicate entry id");
}
TEST_CASE("wildlife species: fresh non-New unmatched default and source ownership are explicit") {
    std::optional<load::WildlifeCreationPlan> owned;
    {
        Source s;
        species_source(s);
        s.add(131, "RAUBTIER", {}, "SAFE_UNMATCHED_NAME");
        s.add(130, "RAUBTIER");
        const std::array<sim::TypeId, 3> requested{sim::TypeId{127}, sim::TypeId{130},
                                                   sim::TypeId{131}};
        const auto generic = load::bind_wildlife_species_facts(load::WildlifeDescriptorProfile::Gog,
                                                               requested, s.entries, s.image);
        REQUIRE(generic);
        CHECK(generic.value()[0].yield == 138);
        CHECK(generic.value()[0].ware_present);
        CHECK(generic.value()[0].ware_matched);
        CHECK(generic.value()[1].yield == 0);
        CHECK_FALSE(generic.value()[1].ware_present);
        CHECK_FALSE(generic.value()[1].ware_matched);
        CHECK(generic.value()[2].yield == 0);
        CHECK(generic.value()[2].ware_present);
        CHECK_FALSE(generic.value()[2].ware_matched);
        auto welt = species_pe();
        put16(welt, 0x980 + 31 * 2, 131);
        const std::array<std::uint32_t, 1> draw{31};
        const auto defaulted = species_plan(s, welt, true, 255, draw);
        REQUIRE(defaulted);
        CHECK(defaulted.value().selected.yield == 0);
        CHECK(defaulted.value().choice.value.index == 31);
        CHECK(defaulted.value().choice.consumed == 1);
        CHECK(defaulted.value().disposition ==
              load::WildlifePlanDisposition::UnsupportedCreationKind);
        CHECK_FALSE(defaulted.value().creation);
        const std::array<std::uint32_t, 1> supported_draw{0};
        const auto plan = species_plan(s, welt, true, 255, supported_draw);
        REQUIRE(plan);
        owned = plan.value();
        const auto safe_ware = s.entries[3].records.back();
        s.entries[3].records.back() = s.record("WARE", {'U', 'N', 'K'});
        const auto unsafe = species_plan(s, welt, true, 255, supported_draw);
        REQUIRE_FALSE(unsafe);
        CHECK(unsafe.error().source == "creation descriptor: unsafe WARE name");
        s.entries[3].records.back() = safe_ware;
        s.entries[3].records.push_back(s.record("WARE", {'M', 'E', 'A', 'T', 0}));
        const auto ambiguous = species_plan(s, welt, true, 255, supported_draw);
        REQUIRE_FALSE(ambiguous);
        CHECK(ambiguous.error().source == "creation descriptor: unique needed records required");
    }
    REQUIRE(owned);
    REQUIRE(owned->creation);
    CHECK(owned->creation->id().v == 60001);
    CHECK(owned->ring.ids.size() == 32);
    CHECK(owned->slot_facts.size() == 32);
    CHECK(owned->selected.source_sha256.size() == 64);
    // Authored geometry only: selection did not generate these position words.
    const auto projected = load::project_wildlife_creation(*owned->creation, end_creation_inputs());
    REQUIRE(projected);
    CHECK(projected.value().animal->type.v == 60001);
    CHECK(projected.value().animal->hp == 17);
    CHECK(projected.value().animal->yield == 137);
}
TEST_CASE(
    "wildlife species: original climate two ring facts and selections match independent oracle") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const auto welt = anno::test::read_file(*dir / "AnnoWelt.dll");
    const auto game = anno::test::read_file(*dir / "AnnoGame.dll");
    const auto bytes = anno::test::read_file(*dir / "FigurenHeader.dat");
    const auto header = fmt::figuren::FigurenHeader::parse(bytes);
    REQUIRE(header);
    const std::array<std::uint32_t, 3> ordinary{6, 33, 999};
    const auto prepare = [&](bool cave, sim::GoodsId saved, std::span<const std::uint32_t> draws) {
        return load::prepare_wildlife_creation_plan(welt, game, header.value().entries,
                                                    SpeciesProfile::GogClimate2, 2, cave, 2, saved,
                                                    draws);
    };
    const auto plan = prepare(false, 255, ordinary);
    REQUIRE_MESSAGE(plan, (plan ? "" : plan.error().source));
    REQUIRE(plan.value().ring.ids.size() == 32);
    REQUIRE(plan.value().slot_facts.size() == 32);
    std::ostringstream rows;
    for (std::size_t i = 0; i < 32; ++i) {
        const auto& f = plan.value().slot_facts[i];
        CHECK(f.id.v == plan.value().ring.ids[i]);
        rows << i << '\t' << f.id.v << '\t' << f.kind << '\t' << f.yield << '\n';
    }
    // Independent pefile + raw Figuren reference, not generated by this reader.
    const auto text = rows.str();
    CHECK(fmt::sha256_hex(
              fmt::Bytes(reinterpret_cast<const std::uint8_t*>(text.data()), text.size())) ==
          "fda833fca3e31245f1b9ce6088015ac6b0379a37fde6687ed2602b914617b0cc");
    if (const auto oracle = std::getenv("ANNO_WILDLIFE_SPECIES_ORACLE")) {
        std::ifstream input(oracle);
        REQUIRE(input);
        for (std::size_t i = 0; i < 32; ++i) {
            std::uint32_t index = 0, id = 0, kind = 0, yield = 0;
            REQUIRE(static_cast<bool>(input >> index >> id >> kind >> yield));
            CHECK(index == i);
            CHECK(id == plan.value().ring.ids[i]);
            CHECK(kind == plan.value().slot_facts[i].kind);
            CHECK(yield == plan.value().slot_facts[i].yield);
        }
        std::string extra;
        CHECK_FALSE(static_cast<bool>(input >> extra));
    }
    struct Expected {
        bool cave;
        sim::GoodsId saved;
        std::array<std::uint32_t, 3> draws;
        std::int32_t position;
        std::uint32_t index, inspected;
        std::size_t consumed;
    };
    // Hand/source-derived, cross-checked with unchanged wildlife_ref.py before C++.
    for (const auto& e : std::array<Expected, 5>{{{false, 255, {6, 33, 999}, 2, 1, 1, 2},
                                                  {true, 255, {31, 999, 888}, -1, 31, 1, 1},
                                                  {true, 137, {31, 999, 888}, -1, 0, 2, 1},
                                                  {false, 138, {1, 17, 999}, 1, 23, 7, 2},
                                                  {true, 142, {31, 999, 888}, -1, 16, 18, 1}}}) {
        const auto selected = prepare(e.cave, e.saved, e.draws);
        REQUIRE(selected);
        CHECK(selected.value().choice.value.position == e.position);
        CHECK(selected.value().choice.value.index == e.index);
        CHECK(selected.value().choice.value.inspected == e.inspected);
        CHECK(selected.value().choice.consumed == e.consumed);
        CHECK(selected.value().selected == plan.value().slot_facts[e.index]);
        if (e.index == 31) {
            CHECK(selected.value().selected.kind == 50);
            CHECK(selected.value().selected.yield == 0);
            CHECK(selected.value().selected.ware_present);
            CHECK_FALSE(selected.value().selected.ware_matched);
            CHECK_FALSE(selected.value().creation);
            CHECK(selected.value().disposition ==
                  load::WildlifePlanDisposition::UnsupportedCreationKind);
        } else {
            REQUIRE(selected.value().creation);
            CHECK(selected.value().creation->id() == selected.value().selected.id);
            CHECK(selected.value().creation->yield() == selected.value().selected.yield);
        }
    }
}
TEST_CASE("wildlife species: single record error order remains source before selector") {
    Source source;
    species_source(source);
    const auto welt = species_pe();
    auto broken_welt = welt;
    put32(broken_welt, 0x48, 0);
    auto broken_game = source.image;
    put32(broken_game, 0x48, 0);
    const auto ring_first = load::prepare_wildlife_creation_plan(
        broken_welt, broken_game, source.entries, SpeciesProfile::GogClimate2, 2, true, 0, 255, {});
    REQUIRE_FALSE(ring_first);
    CHECK(ring_first.error().stage == load::WildlifePlanFault::Source);
    CHECK(ring_first.error().source == "species ring: wrong build");
    const auto game_first = load::prepare_wildlife_creation_plan(
        welt, broken_game, source.entries, SpeciesProfile::GogClimate2, 2, true, 0, 255, {});
    REQUIRE_FALSE(game_first);
    CHECK(game_first.error().stage == load::WildlifePlanFault::Source);
    CHECK(game_first.error().source == "wildlife names: wrong build");
    const auto no_entrance = species_plan(source, welt, true, 255, {}, 0);
    REQUIRE_FALSE(no_entrance);
    CHECK(no_entrance.error().stage == load::WildlifePlanFault::Selector);
    CHECK(no_entrance.error().selector.fault == sim::WildlifeFault::Oq574CaveWithoutEntrance);
    CHECK(no_entrance.error().selector.consumed == 0);
    const std::array<std::uint32_t, 1> one{0};
    const auto short_draws = species_plan(source, welt, false, 255, one);
    REQUIRE_FALSE(short_draws);
    CHECK(short_draws.error().selector.fault == sim::WildlifeFault::DrawsExhausted);
    CHECK(short_draws.error().selector.consumed == 1);
    const auto supported = species_plan(source, welt, true, 255, one);
    REQUIRE(supported);
    REQUIRE(supported.value().creation);
    CHECK(supported.value().selected.id.v == 60001); // Unselected kind 50 never New-bound.
    const std::array<std::uint32_t, 1> predator{31};
    const auto unsupported = species_plan(source, welt, true, 255, predator);
    REQUIRE(unsupported);
    CHECK(unsupported.value().disposition ==
          load::WildlifePlanDisposition::UnsupportedCreationKind);
    CHECK(unsupported.value().choice.consumed == 1);
    source.entries[0].records.back() = source.record("WARE", {'B', 'A', 'D', 0});
    const auto needed_invalid = species_plan(source, welt, true, 255, predator);
    REQUIRE_FALSE(needed_invalid);
    CHECK(needed_invalid.error().stage == load::WildlifePlanFault::Source);
    CHECK(needed_invalid.error().source == "creation descriptor: unknown WARE");
}

namespace {
using MatD = load::WildlifeMaterializeDisposition;
using MatF = load::WildlifeMaterializeFault;
using MatS = load::WildlifeMaterializeStage;
using TileR = load::WildlifeTileResolution;
load::WildlifeChunkRecord ready_record(std::uint64_t token, std::uint8_t pos,
                                       std::uint16_t goods = 255) {
    load::WildlifeChunkRecord record;
    record.token = token;
    record.position = pos;
    record.occupied = false;
    record.timestamp = std::uint8_t{0};
    record.goods = goods;
    return record;
}
load::WildlifeChunkPlan authored_chunk(std::vector<load::WildlifeChunkRecord> records) {
    load::WildlifeChunkPlan out;
    out.completed.chunk.records = std::move(records);
    out.completed.retained = static_cast<std::uint64_t>(
        std::count_if(out.completed.chunk.records.begin(), out.completed.chunk.records.end(),
                      [](const auto& record) { return record.category == 0; }));
    return out; // Authored completed-snapshot facts, not maintenance execution.
}
load::WildlifeMaterializeFacts materialize_facts() {
    return {{{1, TileR::Present, 41},
             {2, TileR::Present, 10},
             {3, TileR::Present, 11},
             {4, TileR::Present, 11}},
            {{41, 7, 0xe2}, {42, 0x46, 0}, {10, 0x46, 0}, {11, 7, 0x145}},
            {{3, 2}, {4, 2}}};
}
load::WildlifeChunkPlan mixed_materialize_chunk() {
    load::WildlifeChunkRecord category;
    category.token = 100;
    category.category = 3;
    load::WildlifeChunkRecord occupied;
    occupied.token = 90;
    occupied.occupied = true;
    load::WildlifeChunkRecord delayed;
    delayed.token = 80;
    delayed.occupied = false;
    delayed.timestamp = std::uint8_t{1};
    return authored_chunk({category, occupied, delayed, ready_record(70, 1), ready_record(60, 2),
                           ready_record(50, 3, 137)});
}
load::WildlifeSpeciesSource source_view(const Source& source, const Buffer& welt) {
    return {welt, source.image, source.entries, SpeciesProfile::GogClimate2, 2};
}
std::string materialize_summary(const load::WildlifeMaterializeProgress& work) {
    std::ostringstream out;
    for (const auto& e : work.entries) {
        out << e.token << ':' << static_cast<int>(e.disposition) << ':' << e.selector_consumed
            << ':' << e.cumulative_selector_consumed;
        if (e.selection) {
            const auto& s = *e.selection;
            out << ':' << s.choice.value.position << ':' << s.choice.value.index << ':'
                << s.choice.value.species.v << ':' << s.choice.value.inspected << ':'
                << s.selected.kind << ':' << s.selected.yield << ':' << s.selected.source_sha256;
            if (s.creation)
                out << ':' << s.creation->hp() << ':' << s.creation->source_sha256();
        }
        out << '\n';
    }
    return out.str(); // Repeat check supplements, never replaces literal expected rows.
}
} // namespace
TEST_CASE("wildlife materialize: literal mixed order skips and ordinary cave selector prefix") {
    Source source;
    species_source(source);
    const auto welt = species_pe();
    const auto chunk = mixed_materialize_chunk();
    const auto facts = materialize_facts();
    const auto before_facts = facts;
    const auto source_bytes = source.storage;
    const auto image = source.image;
    const std::array<std::uint32_t, 4> raw{6, 33, 31, 999};
    const auto result =
        load::prepare_wildlife_materialization(chunk, 0, facts, source_view(source, welt), raw);
    REQUIRE(result);
    const auto& out = result.value().completed;
    CHECK(out.unchanged_chunk == chunk);
    CHECK(out.selector_consumed == 3);
    CHECK(out.unused_draws == std::vector<std::uint32_t>{999});
    REQUIRE(out.entries.size() == 6);
    const std::array<std::uint64_t, 6> tokens{100, 90, 80, 70, 60, 50};
    const std::array reasons{MatD::WrongCategory,     MatD::Occupied, MatD::NotReady,
                             MatD::NonqualifyingType, MatD::Selected, MatD::Selected};
    const std::array<std::size_t, 6> per{0, 0, 0, 0, 2, 1}, cumulative{0, 0, 0, 0, 2, 3};
    for (std::size_t i = 0; i < 6; ++i) {
        CHECK(out.entries[i].token == tokens[i]);
        CHECK(out.entries[i].disposition == reasons[i]);
        CHECK(out.entries[i].selector_consumed == per[i]);
        CHECK(out.entries[i].cumulative_selector_consumed == cumulative[i]);
        CHECK(out.entries[i].selection.has_value() == (i >= 4));
    }
    REQUIRE(out.entries[4].selection);
    REQUIRE(out.entries[5].selection);
    const auto& ordinary = *out.entries[4].selection;
    CHECK(ordinary.choice.value.position == 2);
    CHECK(ordinary.choice.value.index == 1);
    CHECK(ordinary.choice.value.species.v == 127);
    CHECK(ordinary.choice.value.inspected == 1);
    REQUIRE(ordinary.creation);
    CHECK(ordinary.creation->yield() == 138);
    CHECK(ordinary.creation->source_sha256() == ordinary.selected.source_sha256);
    const auto& cave = *out.entries[5].selection;
    CHECK(cave.choice.value.position == -1);
    CHECK(cave.choice.value.index == 0);
    CHECK(cave.choice.value.species.v == 60001);
    CHECK(cave.choice.value.inspected == 2);
    REQUIRE(cave.creation);
    CHECK(cave.creation->id().v == 60001);
    REQUIRE(out.catalogue);
    CHECK(out.catalogue->ring().ids.size() == 32);
    CHECK(out.catalogue->slots()[31].kind == 50); // Not eagerly New-bound or filtered.
    CHECK(facts == before_facts);
    CHECK(source.storage == source_bytes);
    CHECK(source.image == image);
    CHECK(raw == std::array<std::uint32_t, 4>{6, 33, 31, 999});
    const auto again =
        load::prepare_wildlife_materialization(chunk, 0, facts, source_view(source, welt), raw);
    REQUIRE(again);
    CHECK(materialize_summary(again.value().completed) == materialize_summary(out));
    CHECK(again.value().completed.unchanged_chunk == chunk);
}
TEST_CASE("wildlife materialize: signed byte gate boundaries ignore all unreached facts") {
    struct TimeCase {
        std::uint32_t time;
        std::uint8_t stamp;
        bool eligible;
    };
    for (const auto& t : std::array<TimeCase, 5>{{{256000, 0, true},
                                                  {256000, 129, true},
                                                  {256000, 128, false},
                                                  {256000, 1, false},
                                                  {260000, 250, true}}}) {
        auto record = ready_record(0, 1);
        record.timestamp = t.stamp;
        record.goods.reset();
        if (!t.eligible)
            record.position.reset();
        const auto chunk = authored_chunk({record});
        load::WildlifeMaterializeFacts facts;
        facts.tiles = {{1, TileR::Null, 0}}; // Known-null zero lookup, not a missing fact.
        const std::array<std::uint32_t, 1> raw{999};
        const auto result = load::prepare_wildlife_materialization(chunk, t.time, facts, {}, raw);
        REQUIRE(result);
        const auto& out = result.value().completed;
        REQUIRE(out.entries.size() == 1);
        CHECK(out.entries[0].disposition == (t.eligible ? MatD::NullType : MatD::NotReady));
        CHECK(out.selector_consumed == 0);
        CHECK(out.unused_draws == std::vector<std::uint32_t>{999});
        CHECK_FALSE(out.catalogue);
        CHECK(out.unchanged_chunk == chunk);
    }
    auto chunk = mixed_materialize_chunk();
    chunk.completed.chunk.records.resize(3); // Category/occupied/delayed, all later fields absent.
    load::WildlifeMaterializeFacts ambiguous_unused;
    ambiguous_unused.tiles = {{2, TileR::Unknown, {}}, {2, TileR::Present, 99}};
    const auto skipped = load::prepare_wildlife_materialization(chunk, 0, ambiguous_unused, {}, {});
    REQUIRE(skipped);
    REQUIRE(skipped.value().completed.entries.size() == 3);
    CHECK(skipped.value().completed.entries[0].token == 100);
    CHECK(skipped.value().completed.entries[1].token == 90);
    CHECK(skipped.value().completed.entries[2].token == 80);
    CHECK_FALSE(skipped.value().completed.catalogue);
    CHECK(skipped.value().completed.selector_consumed == 0);
}
TEST_CASE("wildlife materialize: actual whole type never substitutes growing id plus one") {
    Source source;
    species_source(source);
    const auto welt = species_pe();
    const auto facts = materialize_facts();
    const std::array<std::uint32_t, 2> raw{6, 33};
    const auto growing = authored_chunk({ready_record(1, 1)});
    const auto result =
        load::prepare_wildlife_materialization(growing, 0, facts, source_view(source, welt), raw);
    REQUIRE(result);
    CHECK(result.value().completed.entries[0].disposition == MatD::NonqualifyingType);
    CHECK(result.value().completed.entries[0].type->object_word == 41);
    CHECK(result.value().completed.selector_consumed == 0);
    CHECK_FALSE(result.value().completed.catalogue);
    auto control = facts;
    control.tiles[0].object_word =
        42; // Actual next type is a distinct legal control, not substitution.
    const auto legal =
        load::prepare_wildlife_materialization(growing, 0, control, source_view(source, welt), raw);
    REQUIRE(legal);
    CHECK(legal.value().completed.entries[0].disposition == MatD::Selected);
    CHECK(legal.value().completed.entries[0].type->object_word == 42);
    CHECK(legal.value().completed.selector_consumed == 2);
    auto whole = control;
    whole.tiles[0].object_word = 0x4001;
    whole.types.push_back({0x4001, 0, 0});
    whole.types.push_back({1, 0x46, 0});
    const auto unmasked = load::prepare_wildlife_materialization(growing, 0, whole, {}, raw);
    REQUIRE(unmasked);
    CHECK(unmasked.value().completed.entries[0].disposition == MatD::NonqualifyingType);
    CHECK(unmasked.value().completed.entries[0].type->object_word == 0x4001);
}
TEST_CASE("wildlife materialize: reached missing ambiguous and known zero facts stay distinct") {
    Source source;
    species_source(source);
    const auto welt = species_pe();
    const auto original = authored_chunk({ready_record(1, 2)});
    const auto original_facts = materialize_facts();
    const std::array<std::uint32_t, 2> raw{0, 0};
    REQUIRE(load::prepare_wildlife_materialization(original, 0, original_facts,
                                                   source_view(source, welt), raw));
    const std::array<MatS, 10> stages{
        MatS::Occupancy,  MatS::Timestamp, MatS::Position, MatS::Tile,       MatS::Tile,
        MatS::ActualType, MatS::Output0,   MatS::Kind,     MatS::SavedGoods, MatS::ActualType};
    for (unsigned variant = 0; variant < stages.size(); ++variant) {
        auto chunk = original;
        auto facts = original_facts;
        if (variant == 0)
            chunk.completed.chunk.records[0].occupied.reset();
        if (variant == 1)
            chunk.completed.chunk.records[0].timestamp.reset();
        if (variant == 2)
            chunk.completed.chunk.records[0].position.reset();
        if (variant == 3)
            facts.tiles.clear();
        if (variant == 4)
            facts.tiles.push_back(facts.tiles[1]);
        if (variant == 5)
            facts.tiles[1].object_word.reset();
        if (variant == 6)
            facts.types[2].output0.reset();
        if (variant == 7)
            facts.types[2].prodtype.reset();
        if (variant == 8)
            chunk.completed.chunk.records[0].goods.reset();
        if (variant == 9)
            facts.types.push_back(facts.types[2]);
        const auto before = facts;
        const auto result = load::prepare_wildlife_materialization(chunk, 0, facts, {}, raw);
        REQUIRE_FALSE(result);
        CHECK(result.error().stage == stages[variant]);
        CHECK(result.error().fault ==
              ((variant == 4 || variant == 9) ? MatF::AmbiguousFacts : MatF::MissingFacts));
        CHECK(result.error().diagnostic_only.selector_consumed == 0);
        CHECK_FALSE(result.error().diagnostic_only.catalogue);
        CHECK(result.error().diagnostic_only.unchanged_chunk == chunk);
        CHECK(facts == before);
    }
    auto zero = original_facts;
    zero.tiles[1].object_word = 0;
    zero.types.push_back({0, 0, 0}); // Known zero properties, not missing metadata.
    const auto known_zero = load::prepare_wildlife_materialization(original, 0, zero, {}, raw);
    REQUIRE(known_zero);
    CHECK(known_zero.value().completed.entries[0].disposition == MatD::NonqualifyingType);
    zero.tiles[1].resolution = TileR::Null;
    zero.types.clear();
    const auto known_null = load::prepare_wildlife_materialization(original, 0, zero, {}, raw);
    REQUIRE(known_null);
    CHECK(known_null.value().completed.entries[0].disposition == MatD::NullType);
    auto goods_zero = original;
    goods_zero.completed.chunk.records[0].goods = std::uint16_t{0};
    const auto no_match = load::prepare_wildlife_materialization(goods_zero, 0, original_facts,
                                                                 source_view(source, welt), raw);
    REQUIRE(no_match);
    CHECK(no_match.value().completed.entries[0].selection->choice.value.inspected == 18);
    CHECK(no_match.value().completed.selector_consumed == 2);
    auto cave_chunk = authored_chunk({ready_record(2, 3, 137)});
    auto cave_facts = original_facts;
    cave_facts.caves.clear();
    const auto absent = load::prepare_wildlife_materialization(cave_chunk, 0, cave_facts, {}, raw);
    REQUIRE_FALSE(absent);
    CHECK(absent.error().fault == MatF::MissingFacts);
    CHECK(absent.error().stage == MatS::Cave);
    cave_facts.caves = {{3, 0}};
    const auto zero_entrance = load::prepare_wildlife_materialization(
        cave_chunk, 0, cave_facts, source_view(source, welt), raw);
    REQUIRE_FALSE(zero_entrance);
    CHECK(zero_entrance.error().fault == MatF::Selector);
    REQUIRE(zero_entrance.error().selection_error);
    CHECK(zero_entrance.error().selection_error->selector.fault ==
          sim::WildlifeFault::Oq574CaveWithoutEntrance);
    CHECK(zero_entrance.error().diagnostic_only.selector_consumed == 0);
    REQUIRE(zero_entrance.error().diagnostic_only.catalogue);
    const auto bad_source_zero =
        load::prepare_wildlife_materialization(cave_chunk, 0, cave_facts, {}, raw);
    REQUIRE_FALSE(bad_source_zero);
    CHECK(bad_source_zero.error().fault == MatF::Source); // Same source-before-selector order.
}
TEST_CASE("wildlife materialize: supported prefix stops with actual unsupported choice or exact "
          "exhaustion") {
    Source source;
    species_source(source);
    const auto welt = species_pe();
    const auto chunk =
        authored_chunk({ready_record(60, 2), ready_record(50, 3, 137), ready_record(40, 4)});
    const auto facts = materialize_facts();
    const std::array<std::uint32_t, 5> raw{6, 33, 31, 31, 999};
    const auto unsupported =
        load::prepare_wildlife_materialization(chunk, 0, facts, source_view(source, welt), raw);
    REQUIRE_FALSE(unsupported);
    CHECK(unsupported.error().fault == MatF::UnsupportedCreationKind);
    CHECK(unsupported.error().stage == MatS::UnsupportedSelection);
    const auto& prefix = unsupported.error().diagnostic_only;
    REQUIRE(prefix.entries.size() == 2);
    CHECK(prefix.entries[0].token == 60);
    CHECK(prefix.entries[0].selection->choice.value.index == 1);
    CHECK(prefix.entries[0].cumulative_selector_consumed == 2);
    CHECK(prefix.entries[1].token == 50);
    CHECK(prefix.entries[1].selection->choice.value.index == 0);
    CHECK(prefix.entries[1].cumulative_selector_consumed == 3);
    CHECK(prefix.selector_consumed == 4);
    CHECK(prefix.unused_draws == std::vector<std::uint32_t>{999});
    CHECK(prefix.unchanged_chunk == chunk);
    const auto& stop = unsupported.error().failing_record;
    CHECK(stop.token == 40);
    CHECK(stop.disposition == MatD::UnsupportedSelection);
    CHECK(stop.selector_consumed == 1);
    CHECK(stop.cumulative_selector_consumed == 4);
    REQUIRE(stop.selection);
    CHECK(stop.selection->choice.value.index == 31);
    CHECK(stop.selection->choice.value.species.v == 503);
    CHECK(stop.selection->choice.value.inspected == 1);
    CHECK_FALSE(stop.selection->creation);
    auto ordinary_last = facts;
    ordinary_last.tiles[3].object_word = 10;
    const std::array<std::uint32_t, 4> short_raw{6, 33, 31, 0};
    const auto exhausted = load::prepare_wildlife_materialization(
        chunk, 0, ordinary_last, source_view(source, welt), short_raw);
    REQUIRE_FALSE(exhausted);
    CHECK(exhausted.error().fault == MatF::Selector);
    CHECK(exhausted.error().stage == MatS::Selector);
    REQUIRE(exhausted.error().selection_error);
    CHECK(exhausted.error().selection_error->selector.fault == sim::WildlifeFault::DrawsExhausted);
    CHECK(exhausted.error().selection_error->selector.consumed == 1);
    CHECK(exhausted.error().diagnostic_only.entries.size() == 2);
    CHECK(exhausted.error().diagnostic_only.selector_consumed == 4);
    CHECK(exhausted.error().diagnostic_only.unused_draws.empty());
    CHECK(exhausted.error().diagnostic_only.unchanged_chunk == chunk);
    CHECK(exhausted.error().failing_record.token == 40);
    CHECK(exhausted.error().failing_record.selector_consumed == 1);
    CHECK(exhausted.error().failing_record.cumulative_selector_consumed == 4);
    CHECK_FALSE(exhausted.error().failing_record.selection);
}
TEST_CASE("wildlife materialize: catalogue and batch own source and unchanged fresh chunk fields") {
    std::optional<load::WildlifeSpeciesCatalogue> catalogue;
    std::optional<load::WildlifeMaterializePlan> owned;
    load::WildlifeChunkPlan expected;
    {
        Source source;
        species_source(source);
        const auto welt = species_pe();
        auto prepared = load::prepare_wildlife_species_catalogue(welt, source.image, source.entries,
                                                                 SpeciesProfile::GogClimate2, 2);
        REQUIRE(prepared);
        catalogue = std::move(prepared.value());
        load::WildlifeChunkInputs input;
        input.habitat.fill(false);
        input.habitat[9] = true;
        input.fresh_tokens = {7};
        input.time = 259000;
        const std::array<std::uint32_t, 1> maintenance_draw{0};
        auto chunk = load::prepare_wildlife_chunk(input, maintenance_draw);
        REQUIRE(chunk);
        expected = chunk.value();
        REQUIRE_FALSE(expected.completed.chunk.records[0].saved_id);
        REQUIRE_FALSE(expected.completed.chunk.records[0].high_bit);
        const load::WildlifeMaterializeFacts facts{{{9, TileR::Present, 10}}, {{10, 0x46, 0}}, {}};
        const std::array<std::uint32_t, 3> raw{6, 33, 999};
        auto result = load::prepare_wildlife_materialization(chunk.value(), input.time, facts,
                                                             source_view(source, welt), raw);
        REQUIRE(result);
        owned = std::move(result.value());
    }
    REQUIRE(catalogue);
    const std::array<std::uint32_t, 3> raw{6, 33, 999};
    const auto first = catalogue->select(false, 0, 255, raw); // No source bytes/entries exist now.
    REQUIRE(first);
    CHECK(first.value().choice.value.index == 1);
    REQUIRE(first.value().creation);
    CHECK(first.value().creation->source_sha256() == first.value().selected.source_sha256);
    const std::array<std::uint32_t, 1> cave{31};
    const auto second = catalogue->select(true, 2, 137, cave);
    REQUIRE(second);
    CHECK(second.value().choice.value.index == 0);
    REQUIRE(second.value().creation);
    CHECK(second.value().creation->hp() == 17);
    CHECK(catalogue->slots().size() == 32);
    CHECK(catalogue->ring().image_sha256.size() == 64);
    REQUIRE(owned);
    CHECK(owned->completed.unchanged_chunk == expected);
    CHECK(owned->completed.selector_consumed == 2);
    CHECK(owned->completed.unused_draws == std::vector<std::uint32_t>{999});
    REQUIRE(owned->completed.entries.size() == 1);
    CHECK(owned->completed.entries[0].token == 7);
    REQUIRE(owned->completed.entries[0].selection);
    CHECK(owned->completed.entries[0].selection->creation->id().v == 127);
    CHECK(owned->completed.unchanged_chunk.completed.chunk.records[0].timestamp == 3);
    CHECK(owned->completed.unchanged_chunk.completed.chunk.records[0].goods == 255);
    CHECK(owned->completed.unchanged_chunk.completed.chunk.records[0].occupied == false);
    CHECK_FALSE(owned->completed.unchanged_chunk.completed.chunk.records[0].saved_id);
    CHECK_FALSE(owned->completed.unchanged_chunk.completed.chunk.records[0].high_bit);
    CHECK_FALSE(owned->completed.unchanged_chunk.completed.chunk.records[0].byte11);
}
TEST_CASE("wildlife materialize: original climate two batch matches independent source rows and "
          "selections") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const auto welt = anno::test::read_file(*dir / "AnnoWelt.dll");
    const auto game = anno::test::read_file(*dir / "AnnoGame.dll");
    const auto bytes = anno::test::read_file(*dir / "FigurenHeader.dat");
    const auto header = fmt::figuren::FigurenHeader::parse(bytes);
    REQUIRE(header);
    const auto chunk = authored_chunk({ready_record(60, 2), ready_record(50, 3, 137)});
    const auto facts =
        materialize_facts(); // Authored actual tile/cave outcomes, not terrain proof.
    const load::WildlifeSpeciesSource source{welt, game, header.value().entries,
                                             SpeciesProfile::GogClimate2, 2};
    const std::array<std::uint32_t, 4> raw{6, 33, 31, 999};
    const auto result = load::prepare_wildlife_materialization(chunk, 0, facts, source, raw);
    REQUIRE_MESSAGE(result, (result ? "" : result.error().reason));
    const auto& out = result.value().completed;
    REQUIRE(out.catalogue);
    REQUIRE(out.catalogue->slots().size() == 32);
    std::ostringstream rows;
    for (std::size_t i = 0; i < 32; ++i) {
        const auto& f = out.catalogue->slots()[i];
        rows << i << '\t' << f.id.v << '\t' << f.kind << '\t' << f.yield << '\n';
    }
    const auto text = rows.str();
    CHECK(fmt::sha256_hex(
              fmt::Bytes(reinterpret_cast<const std::uint8_t*>(text.data()), text.size())) ==
          "fda833fca3e31245f1b9ce6088015ac6b0379a37fde6687ed2602b914617b0cc");
    if (const auto path = std::getenv("ANNO_WILDLIFE_SPECIES_ORACLE")) {
        std::ifstream input(path);
        REQUIRE(input);
        for (std::size_t i = 0; i < 32; ++i) {
            std::uint32_t index = 0, id = 0, kind = 0, yield = 0;
            REQUIRE(static_cast<bool>(input >> index >> id >> kind >> yield));
            CHECK(index == i);
            CHECK(id == out.catalogue->slots()[i].id.v);
            CHECK(kind == out.catalogue->slots()[i].kind);
            CHECK(yield == out.catalogue->slots()[i].yield);
        }
        std::string extra;
        CHECK_FALSE(static_cast<bool>(input >> extra));
    }
    CHECK(out.unchanged_chunk == chunk);
    CHECK(out.selector_consumed == 3);
    CHECK(out.unused_draws == std::vector<std::uint32_t>{999});
    REQUIRE(out.entries.size() == 2);
    REQUIRE(out.entries[0].selection);
    REQUIRE(out.entries[1].selection);
    CHECK(out.entries[0].token == 60);
    CHECK(out.entries[0].selector_consumed == 2);
    CHECK(out.entries[0].cumulative_selector_consumed == 2);
    CHECK(out.entries[0].selection->choice.value.position == 2);
    CHECK(out.entries[0].selection->choice.value.index == 1);
    CHECK(out.entries[0].selection->choice.value.species.v == 501);
    CHECK(out.entries[0].selection->choice.value.inspected == 1);
    CHECK(out.entries[1].token == 50);
    CHECK(out.entries[1].selector_consumed == 1);
    CHECK(out.entries[1].cumulative_selector_consumed == 3);
    CHECK(out.entries[1].selection->choice.value.position == -1);
    CHECK(out.entries[1].selection->choice.value.index == 0);
    CHECK(out.entries[1].selection->choice.value.species.v == 501);
    CHECK(out.entries[1].selection->choice.value.inspected == 2);
    REQUIRE(out.entries[0].selection->creation);
    REQUIRE(out.entries[1].selection->creation);
    CHECK(out.entries[0].selection->creation->source_sha256() ==
          out.entries[0].selection->selected.source_sha256);
    CHECK(out.entries[1].selection->creation->source_sha256() ==
          out.entries[1].selection->selected.source_sha256);
}

namespace {
constexpr auto final_profile = load::GroundDescriptorProfile::GogAnlagen;
struct FinalRig {
    Source source;
    std::shared_ptr<const sim::TypeRegistry> types;
    FinalRig() {
        const auto name = [&](std::uint32_t number, std::uint32_t base, std::uint32_t rva,
                              std::string_view text) {
            put32(source.image, base + number * 4 - 0x24000 + 0x200, 0x10000000 + rva);
            std::copy(text.begin(), text.end(), source.image.begin() + rva - 0x24000 + 0x200);
        };
        name(70, 0x251f8, 0x262a0, "BAUM");
        name(42, 0x25178, 0x262c0, "HOLZ");
        sim::TypeRegistry::Builder builder;
        const auto add = [&](std::uint32_t id, std::string_view kind,
                             std::optional<std::string> ware, std::vector<sim::GoodsId> output) {
            fmt::figuren::Entry e;
            e.id = id;
            Buffer head(56, 0);
            head[0] = head[1] = 1;
            head[0x19] = 99; // NOT output 0; wrong-property mutant must differ.
            head[0x1c] = 0x29;
            e.records.push_back(source.record("HEAD", head));
            Buffer k(kind.begin(), kind.end());
            k.push_back(0);
            e.records.push_back(source.record("PRODTYPE", std::move(k)));
            if (ware)
                e.records.push_back(source.record("WARE", Buffer(ware->begin(), ware->end())));
            source.entries.push_back(e);
            sim::BuildingType t;
            t.id = sim::TypeId{id};
            t.width = t.height = 1;
            t.prodtype = *sim::prodtype_by_name(kind);
            t.flags_word = 0x29;
            t.head_raw = head;
            t.output_goods = std::move(output);
            builder.add(t);
        };
        add(104, "ROHSTWACHS", std::string("UNUSED\0BAUM\0", 12), {0, 70});
        add(105, "ROHSTOFF", std::string("BAUM\0HOLZ\0", 10), {70, 42});
        add(16384, "ROHSTOFF", {}, {});
        add(16488, "ROHSTOFF", std::string("BAUM\0", 5), {70});
        add(90001, "TIERHOEHLE", std::string{}, {});
        const auto built = builder.build();
        REQUIRE_MESSAGE(built, (built ? "" : built.error()));
        types = built.value();
    }
    auto bind(std::vector<std::uint32_t> words) const {
        return load::bind_wildlife_final_types(final_profile, words, source.entries, source.image,
                                               *types);
    }
    void edit_registry(std::uint32_t word, const auto& edit) {
        sim::TypeRegistry::Builder builder;
        for (const auto& old : types->types()) {
            auto t = old;
            if (t.id.v == word)
                edit(t);
            builder.add(t);
        }
        const auto built = builder.build();
        REQUIRE(built);
        types = built.value();
    }
    load::WildlifeFirstCallResult prepared() const {
        load::WildlifeFirstCallResult out;
        auto& island = out.fill.island;
        island.complete_local_records = true;
        island.resource.tiles = sim::TileLayer(24, 40, 24, 16, 8);
        // Authored completed-view fixture, not proof the original load ran.
        island.terrain = sim::make_terrain(Buffer(384, 32), std::vector<std::uint16_t>(384, 105),
                                           Buffer(384, 6));
        island.resource.tiles.set_object(9U * 24U + 9U, 104);     // local position 9.
        island.resource.tiles.set_object(9U * 24U + 10U, 105);    // position 10.
        island.resource.tiles.set_object(15U * 24U + 15U, 90001); // position 63.
        out.source_sha256 = "authored completed view";
        return out;
    }
};
} // namespace
TEST_CASE("wildlife final facts: actual source getter defaults whole words and owned values") {
    std::optional<load::WildlifeFinalTypes> owned;
    std::optional<load::WildlifeFinalChunkFacts> owned_chunk;
    {
        FinalRig rig;
        const auto before = rig.types->digest();
        const auto result = rig.bind({90001, 104, 105, 0, 16384, 16488, 104});
        REQUIRE_MESSAGE(result, (result ? "" : result.error()));
        CHECK(result.value().null_words == std::vector<std::uint32_t>{0});
        CHECK(result.value().types == std::vector<load::WildlifeFinalTypeFact>{
                                          {104, sim::GoodsId{0}, std::uint32_t{226}},
                                          {105, sim::GoodsId{70}, std::uint32_t{225}},
                                          {16384, sim::GoodsId{0}, std::uint32_t{225}},
                                          {16488, sim::GoodsId{70}, std::uint32_t{225}},
                                          {90001, sim::GoodsId{0}, std::uint32_t{325}}});
        REQUIRE(result.value().provenance.size() == 6);
        CHECK(result.value().provenance[0].null_descriptor);
        CHECK_FALSE(result.value().provenance[3].ware_present); // Absent WARE -> fresh 0.
        CHECK(result.value().provenance[5].ware_present);       // Present empty -> fresh 0.
        CHECK(result.value().game_sha256 == fmt::sha256_hex(rig.source.image));
        CHECK(rig.types->digest() == before);
        owned = result.value();
        const auto view = rig.prepared();
        const auto chunk = load::bind_wildlife_final_chunk(
            view, 1, 1, final_profile, rig.source.entries, rig.source.image, *rig.types);
        REQUIRE(chunk);
        owned_chunk = chunk.value();
    }
    REQUIRE(owned_chunk);
    CHECK(owned_chunk->facts.tiles[63].object_word == 90001);
    CHECK(owned_chunk->source.provenance.back().source_sha256.size() == 64);
    REQUIRE(owned);
    CHECK(owned->types[1].output0 == 70);
    CHECK(owned->types[4].prodtype == 325);
    CHECK(owned->provenance[1].source_sha256.size() == 64);
}
TEST_CASE(
    "wildlife final facts: source admission rejects unavailable ambiguous and mismatched inputs") {
    for (unsigned variant = 0; variant < 13; ++variant) {
        FinalRig rig;
        REQUIRE(rig.bind({105}));
        auto profile = final_profile;
        if (variant == 0)
            rig.source.entries.erase(rig.source.entries.begin() + 1);
        if (variant == 1)
            rig.source.entries.push_back(rig.source.entries[1]);
        if (variant == 2)
            rig.source.entries[1].records.push_back(rig.source.entries[1].records.back());
        if (variant == 3)
            rig.edit_registry(105, [](auto& t) { t.output_goods = {42, 70}; });
        if (variant == 4)
            rig.edit_registry(105, [](auto& t) { t.prodtype = 0; });
        if (variant == 5)
            rig.edit_registry(105, [](auto& t) { t.head_raw[0x19] = 8; });
        if (variant == 6)
            rig.source.entries[1].records.back() = rig.source.record("WARE", {'B', 'A', 'U', 'M'});
        if (variant == 7)
            rig.source.entries[1].records.back() = rig.source.record("WARE", {'X', 0});
        if (variant == 8)
            rig.source.entries[1].records.back() = rig.source.record("WARE", Buffer(1089, 0));
        if (variant == 9)
            profile = static_cast<load::GroundDescriptorProfile>(99);
        if (variant == 10) {
            sim::TypeRegistry::Builder builder;
            for (const auto& t : rig.types->types())
                if (t.id.v != 105)
                    builder.add(t);
            const auto built = builder.build();
            REQUIRE(built);
            rig.types = built.value();
        }
        if (variant == 11)
            rig.source.image[0] = 'X';
        if (variant == 12) {
            Buffer oversized(81, 0);
            const std::string known = "ROHSTOFF";
            std::copy(known.begin(), known.end(), oversized.begin());
            rig.source.entries[1].records[1] = rig.source.record("PRODTYPE", oversized);
        }
        const std::array<std::uint32_t, 1> word{105};
        const auto result = load::bind_wildlife_final_types(profile, word, rig.source.entries,
                                                            rig.source.image, *rig.types);
        CHECK_FALSE(result);
    }
    FinalRig rig;
    CHECK_FALSE(rig.bind({77777})); // Never a fabricated Null descriptor.
    auto zero = rig.source.entries[2];
    zero.id = 0;
    rig.source.entries.push_back(zero);
    sim::TypeRegistry::Builder builder;
    for (const auto& old : rig.types->types())
        builder.add(old);
    auto t = *rig.types->find(sim::TypeId{16384});
    t.id = sim::TypeId{0};
    builder.add(t);
    const auto types = builder.build();
    REQUIRE(types);
    const std::array<std::uint32_t, 1> word{0};
    const auto present = load::bind_wildlife_final_types(final_profile, word, rig.source.entries,
                                                         rig.source.image, *types.value());
    REQUIRE(present);
    CHECK(present.value().null_words.empty());
    CHECK(present.value().types[0].output0 == 0);
}
TEST_CASE(
    "wildlife final facts: checked final chunk addressing and ordered selection composition") {
    FinalRig rig;
    const auto prepared = rig.prepared();
    const auto before = prepared.fill.island.resource.tiles;
    const auto bound = load::bind_wildlife_final_chunk(
        prepared, 1, 1, final_profile, rig.source.entries, rig.source.image, *rig.types);
    REQUIRE_MESSAGE(bound, (bound ? "" : bound.error()));
    const auto& value = bound.value();
    CHECK(value.world_x == 32);
    CHECK(value.world_y == 48);
    REQUIRE(value.facts.tiles.size() == 64);
    for (std::size_t i = 0; i < 64; ++i) {
        const auto expected = i == 9 ? 104U : i == 10 ? 105U : i == 63 ? 90001U : 0U;
        CHECK(value.facts.tiles[i].object_word == expected);
        CHECK(value.facts.tiles[i].resolution == (expected == 0 ? TileR::Null : TileR::Present));
    }
    Source species;
    species_source(species);
    const auto welt = species_pe();
    auto wrong_category = ready_record(100, 0);
    wrong_category.category = 3;
    auto chunk = authored_chunk(
        {wrong_category, ready_record(20, 9), ready_record(30, 10), ready_record(40, 63)});
    const std::array<std::uint32_t, 3> draws{6, 33, 999};
    const auto result = load::prepare_wildlife_materialization(chunk, 0, value.facts,
                                                               source_view(species, welt), draws);
    REQUIRE_FALSE(result);
    CHECK(result.error().stage == MatS::Cave);
    const auto& partial = result.error().diagnostic_only;
    REQUIRE(partial.entries.size() == 3);
    CHECK(partial.entries[0].disposition == MatD::WrongCategory);
    CHECK(partial.entries[1].disposition == MatD::NonqualifyingType);
    REQUIRE(partial.entries[2].selection);
    CHECK(partial.entries[2].selection->selected.id.v == 127);
    CHECK(partial.entries[2].selection->choice.value.index == 1);
    CHECK(partial.selector_consumed == 2);
    CHECK(partial.unused_draws == std::vector<std::uint32_t>{999});
    CHECK(partial.unchanged_chunk == chunk);
    auto facts = value.facts;
    facts.caves = {{63, std::size_t{0}}}; // Supplied outcome, no entrance provider claim.
    const auto no_entrance =
        load::prepare_wildlife_materialization(chunk, 0, facts, source_view(species, welt), draws);
    REQUIRE_FALSE(no_entrance);
    REQUIRE(no_entrance.error().selection_error);
    CHECK(no_entrance.error().selection_error->selector.fault ==
          sim::WildlifeFault::Oq574CaveWithoutEntrance);
    CHECK(no_entrance.error().diagnostic_only.selector_consumed == 2);
    const auto again = load::prepare_wildlife_materialization(chunk, 0, value.facts,
                                                              source_view(species, welt), draws);
    REQUIRE_FALSE(again);
    CHECK(materialize_summary(again.error().diagnostic_only) == materialize_summary(partial));
    for (std::uint64_t i = 0; i < before.tile_count(); ++i)
        CHECK(prepared.fill.island.resource.tiles.object(i) == before.object(i));
    auto missing = prepared;
    missing.fill.island.resource.tiles.set_object(8U * 24U + 8U, 77777);
    CHECK_FALSE(load::bind_wildlife_final_chunk(missing, 1, 1, final_profile, rig.source.entries,
                                                rig.source.image, *rig.types));
    const auto skipped =
        load::prepare_wildlife_materialization(authored_chunk({wrong_category}), 0, {}, {}, draws);
    REQUIRE(skipped);
    CHECK(skipped.value().completed.entries[0].disposition == MatD::WrongCategory);
    for (unsigned variant = 0; variant < 5; ++variant) {
        auto bad = prepared;
        auto cx = 1U, cy = 1U;
        if (variant == 0)
            cx = 3;
        if (variant == 1)
            cy = 2;
        if (variant == 2)
            bad.fill.island.complete_local_records = false;
        if (variant == 3)
            bad.fill.island.terrain.reset();
        if (variant == 4)
            REQUIRE(bad.fill.island.resource.tiles.reset(25, 40, 24, 16, 8));
        CHECK_FALSE(load::bind_wildlife_final_chunk(bad, cx, cy, final_profile, rig.source.entries,
                                                    rig.source.image, *rig.types));
    }
}
TEST_CASE("wildlife final facts: original completed Tutorial chunk and metadata match independent "
          "oracle") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original final facts unavailable)\n";
        return;
    }
    const auto read = [](const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        REQUIRE(input);
        return Buffer(std::istreambuf_iterator<char>(input), {});
    };
    const auto scene = read(*dir / "Scenes/Tutorial/01Discover01.sze");
    CHECK(fmt::sha256_hex(scene) ==
          "225ed50b005743e00d8d3ffe18667d4565dc781ef45bd6d9b767fd2fe056c0f6");
    const auto bytes = read(*dir / "Anlagen.dat");
    const auto entries = fmt::anlagen::AnlagenFile::parse(bytes);
    REQUIRE(entries);
    const auto game = read(*dir / "AnnoGame.dll");
    const auto registry = load::load_original_registry(*dir, load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    load::PrefixInputs supplied;
    supplied.sampled_stamps = {500};
    supplied.stamp_provenance = "authored stamp 500, not original time";
    supplied.mode_provenance = "authored mode 0";
    const std::vector<std::uint32_t> prefix_draws(20000, 7);
    const auto prepared = load::prepare_wildlife_first_call(
        scene, {0, 11}, entries.value().entries, game, *registry.types, supplied, prefix_draws);
    REQUIRE_MESSAGE(prepared, (prepared ? "" : prepared.error().reason));
    const auto& tiles = prepared.value().fill.island.resource.tiles;
    std::vector<std::uint32_t> before;
    for (std::uint64_t i = 0; i < tiles.tile_count(); ++i)
        before.push_back(tiles.object(i));
    const auto bound = load::bind_wildlife_final_chunk(
        prepared.value(), 10, 2, final_profile, entries.value().entries, game, *registry.types);
    REQUIRE_MESSAGE(bound, (bound ? "" : bound.error()));
    CHECK(bound.value().world_x == 104);
    CHECK(bound.value().world_y == 40);
    REQUIRE(bound.value().facts.tiles.size() == 64);
    std::string rows;
    for (const auto& cell : bound.value().facts.tiles) {
        REQUIRE(cell.object_word);
        REQUIRE(cell.resolution == TileR::Present);
        const auto it =
            std::find_if(bound.value().facts.types.begin(), bound.value().facts.types.end(),
                         [&](const auto& t) { return t.object_word == *cell.object_word; });
        REQUIRE(it != bound.value().facts.types.end());
        REQUIRE(it->prodtype);
        REQUIRE(it->output0);
        rows += std::to_string(cell.position) + " " + std::to_string(*cell.object_word) + " " +
                std::to_string(*it->prodtype) + " " + std::to_string(*it->output0) + "\n";
    }
    // Independent raw GELAENDE footprint + HEIGHT/FRUCHTBAR fill and direct
    // Game field/name decoding, not recorded C++ output. No world/state pin.
    CHECK(fmt::sha256_hex(Buffer(rows.begin(), rows.end())) ==
          "b6533b5d020db1d48d0c9bb41b1a84e319d5f9031fd061806e50cb74da9a29ed");
    if (const char* path = std::getenv("ANNO_WILDLIFE_FINAL_ORACLE")) {
        std::ifstream input(path, std::ios::binary);
        REQUIRE(input);
        CHECK(rows == std::string(std::istreambuf_iterator<char>(input), {}));
    }
    const auto figure_bytes = read(*dir / "FigurenHeader.dat");
    const auto figures = fmt::figuren::FigurenHeader::parse(figure_bytes);
    REQUIRE(figures);
    const auto welt = read(*dir / "AnnoWelt.dll");
    const load::WildlifeSpeciesSource species{
        welt, game, figures.value().entries, fmt::annowelt::WildlifeSpeciesProfile::GogClimate2, 2};
    const auto chunk =
        authored_chunk({ready_record(10, 0), ready_record(20, 2), ready_record(30, 63)});
    const std::array<std::uint32_t, 3> raw{6, 33, 999};
    const auto plan =
        load::prepare_wildlife_materialization(chunk, 0, bound.value().facts, species, raw);
    REQUIRE(plan);
    REQUIRE(plan.value().completed.entries.size() == 3);
    const auto& out = plan.value().completed;
    CHECK(out.entries[0].disposition == MatD::NonqualifyingType);
    CHECK(out.entries[1].disposition == MatD::Selected);
    CHECK(out.entries[2].disposition == MatD::NonqualifyingType);
    REQUIRE(out.entries[1].selection);
    CHECK(out.entries[1].selection->selected.id.v == 501);
    CHECK(out.entries[1].selection->choice.value.index == 1);
    CHECK(out.entries[1].selector_consumed == 2);
    CHECK(out.selector_consumed == 2);
    CHECK(out.unused_draws == std::vector<std::uint32_t>{999});
    CHECK(out.unchanged_chunk == chunk);
    // Tutorial contains no cave in this independent raw census. Original cave
    // metadata is a labelled synthetic-tile companion, not a real cave provider.
    const std::array<std::uint32_t, 1> cave_word{1542};
    const auto cave = load::bind_wildlife_final_types(
        final_profile, cave_word, entries.value().entries, game, *registry.types);
    REQUIRE(cave);
    REQUIRE(cave.value().types.size() == 1);
    CHECK(cave.value().types[0].prodtype == 325);
    CHECK(cave.value().types[0].output0 == 0);
    load::WildlifeMaterializeFacts companion;
    companion.types = cave.value().types;
    companion.tiles = {{0, TileR::Present, std::uint32_t{1542}}};
    const auto unresolved = load::prepare_wildlife_materialization(
        authored_chunk({ready_record(40, 0)}), 0, companion, species, raw);
    REQUIRE_FALSE(unresolved);
    CHECK(unresolved.error().stage == MatS::Cave);
    CHECK(unresolved.error().diagnostic_only.selector_consumed == 0);
    for (std::uint64_t i = 0; i < tiles.tile_count(); ++i)
        REQUIRE(tiles.object(i) == before[i]);
}

namespace {
Buffer habitat_scene(std::uint8_t property, std::size_t info_size = 12) {
    const auto packed = [](std::string_view name, const Buffer& bytes) {
        fmt::chunk::Chunk c;
        c.name_field = *fmt::chunk::name_field(name);
        c.payload = bytes;
        Buffer out;
        REQUIRE(fmt::chunk::write_chunk(c, out));
        return out;
    };
    Buffer head;
    for (const auto value : {24U, 40U, 32U, 24U})
        fmt::put_u16_le(head, static_cast<std::uint16_t>(value));
    fmt::put_u32_le(head, 9);
    Buffer info(info_size, 0);
    if (!info.empty())
        info[0] = property;
    if (info.size() > 1)
        info[1] = 5; // Not property 7. Wrong INFO offset must differ.
    const auto tail = packed("INFO", info);
    head.insert(head.end(), tail.begin(), tail.end());
    return packed("ANNO", packed("SCENE", packed("INSEL5", head)));
}
struct HabitatRig {
    FinalRig metadata;
    load::WildlifeFirstCallResult prepared;
    Buffer scene;
    HabitatRig(std::uint8_t property = 2) {
        auto double_entry = metadata.source.entries[1];
        double_entry.id = 90002;
        const std::string cave = "TIERHOEHLE";
        Buffer kind(cave.begin(), cave.end());
        kind.push_back(0);
        double_entry.records[1] = metadata.source.record("PRODTYPE", kind);
        metadata.source.entries.push_back(double_entry);
        auto both = *metadata.types->find(sim::TypeId{105});
        both.id = sim::TypeId{90002};
        both.prodtype = 325;
        sim::TypeRegistry::Builder builder;
        for (const auto& t : metadata.types->types())
            builder.add(t);
        builder.add(both);
        const auto types = builder.build();
        REQUIRE(types);
        metadata.types = types.value();
        auto& island = prepared.fill.island;
        island.slot = 9;
        island.complete_local_records = true;
        island.resource.tiles = sim::TileLayer(24, 40, 32, 24, 8);
        island.terrain = sim::make_terrain(Buffer(768, 32), std::vector<std::uint16_t>(768, 104),
                                           Buffer(768, 6));
        property_source(property);
    }
    void property_source(std::uint8_t value) {
        scene = habitat_scene(value);
        prepared.source_sha256 = fmt::sha256_hex(scene);
        prepared.fill.island.climate = value;
    }
    void cell(std::int32_t x, std::int32_t y, std::uint32_t word) {
        auto& tiles = prepared.fill.island.resource.tiles;
        REQUIRE(tiles.contains(32 + x, 48 + y));
        tiles.set_object(tiles.index_of(32 + x, 48 + y), word);
    }
    auto bind(std::uint32_t cx = 1, std::uint32_t cy = 1) const {
        return load::bind_wildlife_habitat_chunk(prepared, scene, cx, cy, final_profile,
                                                 metadata.source.entries, metadata.source.image,
                                                 *metadata.types);
    }
};
constexpr std::array<std::pair<std::int32_t, std::int32_t>, 8> habitat_offsets{
    {{-1, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}, {-1, 1}, {0, 1}, {1, 1}}};
std::string habitat_region(const load::WildlifeHabitatChunk& out) {
    std::string rows;
    for (const auto& c : out.region)
        rows += std::to_string(c.x) + " " + std::to_string(c.y) + " " +
                std::to_string(c.original_word) + " " + std::to_string(c.resolved_word) + " " +
                std::to_string(c.fact.kind) + " " + std::to_string(c.fact.goods) + "\n";
    return rows;
}
} // namespace
TEST_CASE("wildlife habitat load: each halo offset growing resolution zero and exact threshold") {
    for (std::size_t n = 0; n < 8; ++n) {
        HabitatRig rig;
        rig.cell(0, 0, 105);
        for (const auto other : {(n + 1) % 8, (n + 2) % 8})
            rig.cell(habitat_offsets[other].first, habitat_offsets[other].second, 105);
        auto baseline = rig.bind();
        REQUIRE_MESSAGE(baseline, (baseline ? "" : baseline.error()));
        CHECK(baseline.value().habitat[0] == false); // 1+2 = 3, strictly below 4.
        rig.cell(habitat_offsets[n].first, habitat_offsets[n].second, 104);
        const auto result = rig.bind();
        REQUIRE(result);
        const auto& out = result.value();
        REQUIRE(out.cells.size() == 64);
        REQUIRE(out.region.size() == 100);
        CHECK(out.property7 == 2);
        CHECK(out.habitat[0] == true); // 1+3 = 4, inclusive.
        const auto& neighbor = out.region[out.cells[0].neighbors[n]];
        CHECK(neighbor.x == 32 + habitat_offsets[n].first);
        CHECK(neighbor.y == 48 + habitat_offsets[n].second);
        CHECK(neighbor.original_word == 104);
        CHECK(neighbor.resolved_word == 105);
        CHECK(neighbor.fact.object.v == 104);
        CHECK(neighbor.fact.kind == 225);
        CHECK(neighbor.fact.goods == 70);
        CHECK(rig.prepared.fill.island.resource.tiles.object(
                  rig.prepared.fill.island.resource.tiles.index_of(neighbor.x, neighbor.y)) == 104);
    }
    HabitatRig both;
    both.cell(0, 0, 105);
    both.cell(-1, 0, 90002);
    auto below = both.bind();
    REQUIRE(below);
    CHECK(below.value().habitat[0] == false); // 1+2 = 3.
    both.cell(1, 0, 105);
    const auto equal = both.bind();
    REQUIRE(equal);
    CHECK(equal.value().habitat[0] == true); // 1+2+1 = 4; OR collapse would give 3.
    HabitatRig easy(5);
    easy.cell(0, 0, 104);
    const auto no_neighbors = easy.bind();
    REQUIRE(no_neighbors);
    CHECK(no_neighbors.value().property7 == 5);
    CHECK(no_neighbors.value().habitat[0] == true); // Growing centre, score 1.
    CHECK(no_neighbors.value().region[11].resolved_word == 105);
    easy.property_source(2);
    const auto ordinary_property = easy.bind();
    REQUIRE(ordinary_property);
    CHECK(ordinary_property.value().habitat[0] == false);
    easy.cell(0, 0, 0);
    easy.property_source(5);
    const auto empty = easy.bind();
    REQUIRE(empty);
    CHECK(empty.value().habitat[0] == false);
}
TEST_CASE("wildlife habitat load: source admission geometry missing successor and source property "
          "refuse") {
    for (unsigned variant = 0; variant < 10; ++variant) {
        HabitatRig rig(5);
        rig.cell(0, 0, 104);
        REQUIRE(rig.bind());
        auto cx = 1U, cy = 1U;
        if (variant == 0)
            cx = 0;
        if (variant == 1)
            cy = 2; // Last chunk has no southern halo.
        if (variant == 2)
            rig.prepared.fill.island.climate = 2; // Numeric claim cannot replace INFO 5.
        if (variant == 3)
            rig.scene.back() ^= 1; // Exact source pairing required.
        if (variant == 4) {
            rig.scene = habitat_scene(5, 11);
            rig.prepared.source_sha256 = fmt::sha256_hex(rig.scene);
        }
        if (variant == 5)
            rig.cell(-1, -1, 77777); // Missing actual type, not false habitat.
        if (variant == 6)
            rig.metadata.source.entries.erase(rig.metadata.source.entries.begin() +
                                              1); // Successor.
        if (variant == 7)
            rig.prepared.fill.island.complete_local_records = false;
        if (variant == 8)
            rig.prepared.fill.island.terrain.reset();
        if (variant == 9) {
            auto t = *rig.metadata.types->find(sim::TypeId{104});
            t.id = sim::TypeId{std::numeric_limits<std::uint32_t>::max()};
            sim::TypeRegistry::Builder builder;
            for (const auto& old : rig.metadata.types->types())
                builder.add(old);
            builder.add(t);
            const auto types = builder.build();
            REQUIRE(types);
            rig.metadata.types = types.value();
            auto actual = rig.metadata.source.entries[0];
            actual.id = t.id.v;
            rig.metadata.source.entries.push_back(actual);
            rig.cell(0, 0, t.id.v); // Checked successor addition, never wraps to 0.
        }
        const auto refused = rig.bind(cx, cy);
        REQUIRE_FALSE(refused);
        if (variant == 9)
            CHECK(refused.error() == "habitat source: successor overflow");
    }
    HabitatRig zero(5);
    auto type0 = *zero.metadata.types->find(sim::TypeId{90002});
    type0.id = sim::TypeId{0};
    sim::TypeRegistry::Builder builder;
    for (const auto& old : zero.metadata.types->types())
        builder.add(old);
    builder.add(type0);
    const auto types = builder.build();
    REQUIRE(types);
    zero.metadata.types = types.value();
    auto entry0 = zero.metadata.source.entries.back();
    entry0.id = 0;
    zero.metadata.source.entries.push_back(entry0);
    const auto result = zero.bind();
    REQUIRE(result);
    CHECK(result.value().types.types.empty()); // Zero skipped BEFORE lookup, not resolved Null.
    for (const auto& value : result.value().habitat)
        CHECK(value == false);
}
TEST_CASE("wildlife habitat load: complete outcomes compose literal recycle collision and partial "
          "maintenance") {
    std::optional<load::WildlifeHabitatChunk> owned;
    {
        HabitatRig rig(5);
        const std::vector<std::uint8_t> candidates{9, 11, 13, 15, 25, 27, 29, 31};
        for (const auto p : candidates)
            rig.cell(p & 7, p >> 3, 105);
        // Decoys beyond the admitted halo, nonzero origin and non-square stride.
        rig.cell(23, -1, 90002);
        const auto before = rig.prepared.fill.island.resource.tiles;
        const auto result = rig.bind();
        REQUIRE(result);
        for (std::size_t p = 0; p < 64; ++p) {
            const auto expected =
                std::find(candidates.begin(), candidates.end(), p) != candidates.end();
            CHECK(result.value().habitat[p] == expected);
        }
        const auto repeated = rig.bind();
        REQUIRE(repeated);
        CHECK(habitat_region(repeated.value()) == habitat_region(result.value()));
        CHECK(repeated.value().habitat == result.value().habitat);
        for (std::uint64_t i = 0; i < before.tile_count(); ++i)
            REQUIRE(rig.prepared.fill.island.resource.tiles.object(i) == before.object(i));
        owned = result.value();
    }
    REQUIRE(owned);
    CHECK(owned->property7 == 5);
    CHECK(owned->region[0].x == 31);
    CHECK(owned->region[99].y == 56);
    auto occupied = ready_record(10, 2, 7);
    occupied.occupied = true;
    occupied.saved_id = std::uint16_t{101};
    auto rejected = ready_record(20, 4, 8);
    rejected.saved_id = std::uint16_t{202};
    rejected.high_bit = true;
    rejected.timestamp = std::uint8_t{12};
    const auto retained = ready_record(30, 9, 9);
    load::WildlifeChunkRecord unrelated;
    unrelated.token = 90;
    unrelated.category = 3;
    const load::WildlifeChunkSnapshot input{0x8101, {occupied, rejected, retained, unrelated}};
    const auto before = input;
    const std::array<std::uint32_t, 3> draws{0, 0, 999};
    const auto result = load::prepare_wildlife_habitat_maintenance(
        *owned, input, 259000, load::WildlifeChunkAllocation::NormalSuccess, {}, draws);
    REQUIRE(result);
    auto inserted = rejected;
    inserted.position = std::uint8_t{31};
    inserted.goods = std::uint16_t{255};
    inserted.timestamp = std::uint8_t{3};
    CHECK(result.value().completed.chunk ==
          load::WildlifeChunkSnapshot{0x8101, {inserted, occupied, retained, unrelated}});
    CHECK(result.value().completed.recycled_order ==
          std::vector<load::WildlifeChunkRecord>{rejected});
    REQUIRE(result.value().completed.attempts.size() == 2);
    CHECK(result.value().completed.attempts[0].position == 9);
    CHECK(result.value().completed.attempts[0].collision == std::uint64_t{30});
    CHECK(result.value().completed.attempts[1].position == 31);
    CHECK(result.value().completed.consumed == 2);
    CHECK(result.value().completed.retained_before_replenish == 2);
    CHECK(result.value().completed.retained == 3);
    CHECK(result.value().completed.desired == 3);
    CHECK(result.value().completed.candidates == std::vector<std::uint8_t>{29, 11, 13, 15, 25, 27});
    CHECK(input == before);
    CHECK(draws[result.value().completed.consumed] == 999);
    const auto partial = load::prepare_wildlife_habitat_maintenance(
        *owned, input, 259000, load::WildlifeChunkAllocation::NormalSuccess, {},
        std::span<const std::uint32_t>(draws).first(1));
    REQUIRE_FALSE(partial);
    CHECK(partial.error().fault == load::WildlifeChunkFault::DrawsExhausted);
    CHECK(partial.error().stage == load::WildlifeChunkStage::SelectCandidate);
    const auto& out = partial.error().diagnostic_only;
    REQUIRE(out.reserved);
    auto collision = rejected;
    collision.position = std::uint8_t{9};
    collision.goods = std::uint16_t{255};
    CHECK(*out.reserved == collision); // OLD timestamp 12/high bit/ID retained.
    CHECK(out.chunk == load::WildlifeChunkSnapshot{0x8101, {occupied, retained, unrelated}});
    CHECK(out.consumed == 1);
    CHECK(out.candidates == std::vector<std::uint8_t>{31, 11, 13, 15, 25, 27, 29});
    CHECK(out.discarded.empty());
}
TEST_CASE("wildlife habitat load: original Tutorial halo property and maintenance match "
          "independent oracle") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original habitat source unavailable)\n";
        return;
    }
    const auto read = [](const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in);
        return Buffer(std::istreambuf_iterator<char>(in), {});
    };
    const auto scene = read(*dir / "Scenes/Tutorial/01Discover01.sze");
    CHECK(fmt::sha256_hex(scene) ==
          "225ed50b005743e00d8d3ffe18667d4565dc781ef45bd6d9b767fd2fe056c0f6");
    const auto bytes = read(*dir / "Anlagen.dat");
    const auto entries = fmt::anlagen::AnlagenFile::parse(bytes);
    REQUIRE(entries);
    const auto game = read(*dir / "AnnoGame.dll");
    const auto registry = load::load_original_registry(*dir, load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    load::PrefixInputs supplied;
    supplied.sampled_stamps = {500};
    supplied.stamp_provenance = "authored stamp 500";
    supplied.mode_provenance = "authored mode 0";
    const std::vector<std::uint32_t> prefix_draws(20000, 7);
    const auto prepared = load::prepare_wildlife_first_call(
        scene, {0, 11}, entries.value().entries, game, *registry.types, supplied, prefix_draws);
    REQUIRE_MESSAGE(prepared, (prepared ? "" : prepared.error().reason));
    const auto& tiles = prepared.value().fill.island.resource.tiles;
    std::vector<std::uint32_t> before;
    for (std::uint64_t i = 0; i < tiles.tile_count(); ++i)
        before.push_back(tiles.object(i));
    const auto result =
        load::bind_wildlife_habitat_chunk(prepared.value(), scene, 10, 2, final_profile,
                                          entries.value().entries, game, *registry.types);
    REQUIRE_MESSAGE(result, (result ? "" : result.error()));
    const auto& out = result.value();
    CHECK(out.property7 == 2); // Independent raw INFO byte 0, not byte 1.
    CHECK(out.world_x == 104);
    CHECK(out.world_y == 40);
    REQUIRE(out.region.size() == 100);
    const auto rows = habitat_region(out);
    CHECK(fmt::sha256_hex(Buffer(rows.begin(), rows.end())) ==
          "5c9ea575bb934faba1443664f3bdc6af90bfea6891d96a30b03b3574784c4ee0");
    std::string outcomes;
    std::size_t count = 0;
    for (std::size_t i = 0; i < 64; ++i) {
        REQUIRE(out.habitat[i]);
        const auto value = *out.habitat[i];
        outcomes += std::to_string(i) + " " + (value ? "1\n" : "0\n");
        count += value ? 1U : 0U;
    }
    CHECK(count == 2);
    CHECK(fmt::sha256_hex(Buffer(outcomes.begin(), outcomes.end())) ==
          "22c133fe0767858fa6a42c437a06e10175b75a55f4075d31e9bdfd9acacc288f");
    for (const auto& [name, expected] : std::array<std::pair<const char*, std::string>, 2>{
             {{"ANNO_WILDLIFE_HABITAT_REGION_ORACLE", rows},
              {"ANNO_WILDLIFE_HABITAT_ORACLE", outcomes}}}) {
        if (const char* path = std::getenv(name)) {
            std::ifstream in(path, std::ios::binary);
            REQUIRE(in);
            CHECK(std::string(std::istreambuf_iterator<char>(in), {}) == expected);
        }
    }
    // Authored empty records, successful fresh token, time and draws. No original
    // manager outcome, global ID or full-world PRNG history is asserted.
    const std::array<std::uint32_t, 2> draws{17, 999};
    const auto plan = load::prepare_wildlife_habitat_maintenance(
        out, {0x8000, {}}, 259000, load::WildlifeChunkAllocation::NormalSuccess, {5000}, draws);
    REQUIRE(plan);
    auto fresh = ready_record(5000, 47);
    fresh.timestamp = std::uint8_t{3};
    CHECK(plan.value().completed.chunk == load::WildlifeChunkSnapshot{0x8001, {fresh}});
    CHECK(plan.value().completed.retained_before_replenish == 0);
    CHECK(plan.value().completed.desired == 1);
    CHECK(plan.value().completed.retained == 1);
    CHECK(plan.value().completed.consumed == 1);
    CHECK(plan.value().completed.fresh_used == 1);
    REQUIRE(plan.value().completed.attempts.size() == 1);
    CHECK(plan.value().completed.attempts[0].modulus == 1);
    CHECK(plan.value().completed.attempts[0].position == 47);
    CHECK(plan.value().completed.candidates.empty());
    CHECK(draws[plan.value().completed.consumed] == 999);
    const auto repeat = load::prepare_wildlife_habitat_maintenance(
        out, {0x8000, {}}, 259000, load::WildlifeChunkAllocation::NormalSuccess, {5000}, draws);
    REQUIRE(repeat);
    CHECK(repeat.value().completed.chunk == plan.value().completed.chunk);
    for (std::uint64_t i = 0; i < tiles.tile_count(); ++i)
        REQUIRE(tiles.object(i) == before[i]);
}

namespace {
struct ImportRig {
    FinalRig metadata;
    Buffer scene;
    load::WildlifeFirstCallResult prepared;
    ImportRig(std::uint8_t mode = 1, std::uint16_t stamp = 500,
              std::optional<std::array<std::uint8_t, 4>> geometry_patch = {}) {
        // Authored resource-table section, disjoint from the name-table section.
        auto& image = metadata.source.image;
        image.resize(0x4600, 0);
        put16(image, 0x46, 2);
        const auto section = 0x58U + 0xe0U + 40U;
        put32(image, section + 8, 0x400);
        put32(image, section + 12, 0x21000);
        put32(image, section + 16, 0x400);
        put32(image, section + 20, 0x4200);
        put32(image, section + 36, 0x40000040);
        for (unsigned climate = 0; climate < 7; ++climate) {
            for (unsigned i = 0; i < 16; ++i)
                put16(image, 0x4200 + 0x14c + climate * 32 + i * 2, 105);
            for (unsigned base : {0x22cU, 0x29cU, 0x30cU})
                for (unsigned i = 0; i < 8; ++i)
                    put16(image, 0x4200 + base + climate * 16 + i * 2, 105);
        }
        const auto name = [&](std::uint32_t ordinal, std::uint32_t rva, std::string_view text) {
            put32(image, 0x251f8 + ordinal * 4 - 0x24000 + 0x200, 0x10000000 + rva);
            std::copy(text.begin(), text.end(), image.begin() + rva - 0x24000 + 0x200);
        };
        name(69, 0x262e0, "GRAS");
        name(71, 0x26300, "FISCH");
        metadata.source.entries.erase(metadata.source.entries.begin() + 2,
                                      metadata.source.entries.end());
        sim::TypeRegistry::Builder builder;
        builder.add(*metadata.types->find(sim::TypeId{104}));
        builder.add(*metadata.types->find(sim::TypeId{105}));
        for (const auto id : {2U, 11U}) {
            auto entry = metadata.source.entries[1];
            entry.id = id;
            const std::string ware = id == 2 ? "GRAS" : "FISCH";
            Buffer bytes(ware.begin(), ware.end());
            bytes.push_back(0);
            entry.records[2] = metadata.source.record("WARE", bytes);
            metadata.source.entries.push_back(entry);
            auto type = *metadata.types->find(sim::TypeId{105});
            type.id = sim::TypeId{id};
            type.output_goods = {id == 2 ? 69U : 71U};
            builder.add(type);
        }
        sim::FigureType ore;
        ore.id = 501;
        ore.bewohntype = 145;
        ore.ware = "EISENERZ";
        builder.figure_type(ore);
        const auto types = builder.build();
        REQUIRE(types);
        metadata.types = types.value();
        const auto packed = [](std::string_view tag, const Buffer& bytes) {
            fmt::chunk::Chunk c;
            c.name_field = *fmt::chunk::name_field(tag);
            c.payload = bytes;
            Buffer out;
            REQUIRE(fmt::chunk::write_chunk(c, out));
            return out;
        };
        const auto append = [](Buffer& dst, const Buffer& src) {
            dst.insert(dst.end(), src.begin(), src.end());
        };
        const auto block = [&](Buffer bytes) {
            Buffer out;
            fmt::put_u16_le(out, 0);
            fmt::put_u16_le(out, 0);
            out.push_back(32);
            out.push_back(24);
            append(out, bytes);
            return out;
        };
        Buffer body;
        for (const auto value : {24U, 40U, 32U, 24U})
            fmt::put_u16_le(body, static_cast<std::uint16_t>(value));
        fmt::put_u32_le(body, 9);
        Buffer info(12, 0);
        info[0] = 2;
        append(body, packed("INFO", info));
        Buffer heights(768, 32);
        if (geometry_patch) {
            const std::array<std::size_t, 4> indices{9 * 32 + 9, 9 * 32 + 10, 10 * 32 + 9,
                                                     10 * 32 + 10};
            for (std::size_t i = 0; i < 4; ++i)
                heights[indices[i]] = (*geometry_patch)[i];
        }
        append(body, packed("HEIGHT", block(heights)));
        append(body, packed("GFXID", block(Buffer(1536, 0))));
        Buffer objects;
        for (std::uint8_t y = 7; y <= 16; ++y)
            for (std::uint8_t x = 7; x <= 16; ++x) {
                if (x == 13 && y == 8)
                    continue; // Nonqualifying final grass at spawn position 5.
                fmt::put_u16_le(objects, 105);
                fmt::put_u16_le(objects, 0);
                objects.push_back(x);
                objects.push_back(y);
            }
        append(body, packed("GELAENDE", block(objects)));
        append(body,
               packed("HIRSCH", fmt::insel::encode_hirsch(std::vector<fmt::insel::HirschRecord>{
                                    {9, 9, 0x80, 7, 0}, {11, 9, 1, 17, 137}})));
        Buffer berg;
        Buffer value;
        fmt::put_u32_le(value, 15U | (14U << 12) | (1U << 24));
        REQUIRE(fmt::tagged::write_record("ENTRY", false, value, berg));
        append(body, packed("BERGWERK2", berg));
        const auto ore_entry = [](std::uint16_t tons, std::uint32_t discovery) {
            Buffer v;
            fmt::put_u32_le(v, 501U << 14);
            fmt::put_u32_le(v, 0x41680000); // Authored local 14.5.
            fmt::put_u32_le(v, 0x41580000); // Authored local 13.5.
            fmt::put_u32_le(v, 0x3fc00000);
            fmt::put_u16_le(v, tons);
            fmt::put_u16_le(v, 0);
            fmt::put_u32_le(v, discovery);
            Buffer out;
            REQUIRE(fmt::tagged::write_record("ENTRY2", false, v, out));
            return out;
        };
        Buffer occur = ore_entry(400, 7);
        append(occur, ore_entry(1601, 99)); // Same target; no allocation OR state reload.
        append(body, packed("OCCUR", occur));
        append(body,
               packed("HIRSCH", fmt::insel::encode_hirsch(
                                    std::vector<fmt::insel::HirschRecord>{{13, 8, 1, 33, 0}})));
        append(body, packed("FRUCHTBAR", Buffer(768, 6)));
        scene = packed("ANNO", packed("SCENE", packed("INSEL5", body)));
        load::PrefixInputs inputs;
        inputs.mode_bit = mode;
        inputs.sampled_stamps = {stamp};
        inputs.mode_provenance = "authored import mode";
        inputs.stamp_provenance = "authored import stamp";
        const auto result = load::prepare_wildlife_first_call(
            scene, {0, 9}, metadata.source.entries, image, *metadata.types, inputs,
            std::vector<std::uint32_t>(200, 7));
        REQUIRE_MESSAGE(result, (result ? "" : result.error().reason));
        prepared = result.value();
    }
    auto reconstruct(load::WildlifeImportFlags flags = {std::uint16_t{0x8000},
                                                        "authored omitted flags"}) const {
        return load::reconstruct_wildlife_import(prepared, scene, 1, 1, flags);
    }
};
std::vector<std::uint64_t> import_tokens(const load::WildlifeChunkSnapshot& snapshot) {
    std::vector<std::uint64_t> out;
    for (const auto& r : snapshot.records)
        out.push_back(r.token);
    return out;
}
load::WildlifeImportRuntime import_runtime() {
    load::WildlifeImportRuntime runtime;
    runtime.time = 10000;
    runtime.fresh_tokens = {1000, 1001};
    runtime.flags = {std::uint16_t{0x8000}, "authored omitted native flags, not recovered"};
    runtime.provenance = "authored sweep time and successful allocation tokens";
    return runtime;
}
} // namespace
TEST_CASE("wildlife import plan: mixed prepend chronology projections flags and owned sidecars") {
    std::optional<load::WildlifeImportedChunk> owned;
    {
        ImportRig rig;
        const auto result = rig.reconstruct();
        REQUIRE_MESSAGE(result, (result ? "" : result.error().reason));
        const auto& out = result.value();
        CHECK(import_tokens(out.snapshot) == std::vector<std::uint64_t>{7, 6, 2, 1, 0});
        CHECK(out.source_token_count == 8);
        CHECK(out.prefix_draws == 100);
        CHECK(out.derived_presence == 0x43);
        CHECK(out.snapshot.flags == 0x8043);
        CHECK(out.supplied_flags.non_presence == 0x8000);
        REQUIRE(out.order.size() == 5);
        CHECK(out.order[0].source.header == 7);
        CHECK(out.order[1].source.header == 6);
        CHECK(out.order[2].source.header == 5);
        CHECK(out.order[3].source.header == 4);
        CHECK(out.order[4].source.header == 4);
        CHECK(out.order[4].source.record == 0);
        CHECK(out.snapshot.records[0].timestamp == 1);
        CHECK(out.snapshot.records[4].timestamp == 8);
        for (const auto& r : out.snapshot.records) {
            CHECK_FALSE(r.saved_id);
            CHECK_FALSE(r.high_bit);
            CHECK_FALSE(r.byte11);
            if (r.category == 0) {
                REQUIRE(r.position);
                CHECK(r.occupied == false);
                CHECK(r.goods == 255);
            } else {
                CHECK_FALSE(r.position);
                CHECK_FALSE(r.timestamp);
                CHECK_FALSE(r.occupied);
                CHECK_FALSE(r.goods);
            }
        }
        REQUIRE(out.sidecars.size() == 2);
        const auto& berg = std::get<load::WildlifeBergwerkSidecar>(out.sidecars.at(2));
        CHECK(berg.entry.local_x == 15);
        CHECK(berg.entry.local_y == 14);
        CHECK(berg.node_projection.offsets == 0x67);
        CHECK(berg.node_projection.rotation == 1);
        CHECK(berg.node_projection.mask == 0x11);
        CHECK(out.order[2].insertion_ordinal == 0);
        const auto& ore = std::get<load::WildlifeOccurrenceSidecar>(out.sidecars.at(6));
        CHECK(ore.occurrence.type == 501);
        CHECK(ore.occurrence.world_x_bits == 0x421a0000);
        CHECK(ore.occurrence.world_y_bits == 0x42560000);
        CHECK(ore.occurrence.quantity_88 == 512000);
        CHECK(ore.occurrence.discovery == 7);
        CHECK(ore.occurrence.file_tons == 400);
        REQUIRE(ore.loads.size() == 2);
        CHECK(ore.loads[0].created);
        CHECK_FALSE(ore.loads[1].created);
        CHECK(ore.loads[1].source.record == 1);
        const auto repeat = rig.reconstruct();
        REQUIRE(repeat);
        CHECK(repeat.value().snapshot == out.snapshot);
        CHECK(rig.prepared.fill.island.spawns.size() == 3);
        CHECK(rig.prepared.fill.island.resource_list_nodes.size() == 5);
        owned = out;
    }
    REQUIRE(owned);
    CHECK(import_tokens(owned->snapshot) == std::vector<std::uint64_t>{7, 6, 2, 1, 0});
    CHECK(std::get<load::WildlifeOccurrenceSidecar>(owned->sidecars.at(6)).loads[1].source.header ==
          6);
    ImportRig zero(0, 0);
    const auto result = zero.reconstruct({std::uint16_t{0}, "explicit zero omitted flags"});
    REQUIRE(result);
    CHECK(result.value().snapshot.flags == 0x43);
    REQUIRE(result.value().snapshot.records[0].goods);
    CHECK(result.value().snapshot.records[0].goods == 0);
    REQUIRE(result.value().snapshot.records[0].timestamp);
    CHECK(result.value().snapshot.records[0].timestamp == 0);
    CHECK_FALSE(result.value().snapshot.records[0].saved_id);
}
TEST_CASE(
    "wildlife import plan: staged maintenance selection and failure draw domains stay distinct") {
    ImportRig rig;
    Source animals;
    species_source(animals);
    const auto welt = species_pe();
    const load::WildlifeSpeciesSource species{welt, rig.metadata.source.image, animals.entries,
                                              fmt::annowelt::WildlifeSpeciesProfile::GogClimate2,
                                              2};
    const auto runtime = import_runtime();
    const std::array<std::uint32_t, 6> maintenance{0, 1, 0, 0, 0, 999};
    const std::array<std::uint32_t, 11> selectors{6, 0, 6, 0, 6, 0, 6, 0, 6, 0, 999};
    const auto run = [&](std::span<const std::uint32_t> m, std::span<const std::uint32_t> s,
                         const load::WildlifeImportRuntime& context) {
        return load::prepare_wildlife_import_plan(
            rig.prepared, rig.scene, 1, 1, context, final_profile, rig.metadata.source.entries,
            rig.metadata.source.image, *rig.metadata.types, species, m, s);
    };
    const auto result = run(maintenance, selectors, runtime);
    REQUIRE_MESSAGE(result, (result ? "" : result.error().reason));
    const auto& out = result.value();
    CHECK(import_tokens(out.imported.snapshot) == std::vector<std::uint64_t>{7, 6, 2, 1, 0});
    CHECK(import_tokens(out.maintenance.completed.chunk) ==
          std::vector<std::uint64_t>{1001, 1000, 7, 6, 2, 1, 0});
    CHECK(out.draws.imported_prefix == 100);
    CHECK(out.draws.maintenance == 5);
    CHECK(out.draws.selector_only == 10);
    CHECK(out.unused_maintenance == std::vector<std::uint32_t>{999});
    CHECK(out.materialization.completed.unused_draws == std::vector<std::uint32_t>{999});
    REQUIRE(out.materialization.completed.entries.size() == 7);
    const std::array<MatD, 7> expected{MatD::Selected,      MatD::Selected,      MatD::Selected,
                                       MatD::WrongCategory, MatD::WrongCategory, MatD::Selected,
                                       MatD::Selected};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto& step = out.materialization.completed.entries[i];
        CHECK(step.disposition == expected[i]);
        if (step.selection) {
            CHECK(step.selection->selected.id.v == 60001);
            CHECK(step.selection->choice.value.index == 0);
            CHECK(step.selector_consumed == 2);
        }
    }
    CHECK(out.imported.sidecars.size() == 2);
    CHECK(
        std::get<load::WildlifeBergwerkSidecar>(out.imported.sidecars.at(2)).node_projection.mask ==
        17);
    CHECK(std::get<load::WildlifeOccurrenceSidecar>(out.imported.sidecars.at(6))
              .occurrence.discovery == 7);
    const auto partial =
        run(std::span<const std::uint32_t>(maintenance).first(1), selectors, runtime);
    REQUIRE_FALSE(partial);
    CHECK(partial.error().stage == load::WildlifeImportPlanStage::Maintenance);
    CHECK(partial.error().draws.imported_prefix == 100);
    CHECK(partial.error().draws.maintenance == 1);
    CHECK(partial.error().draws.selector_only == 0);
    REQUIRE(partial.error().imported);
    REQUIRE(partial.error().maintenance_error);
    CHECK(partial.error().imported->snapshot == out.imported.snapshot);
    CHECK(partial.error().supplied.flags.provenance == runtime.flags.provenance);
    CHECK(partial.error().maintenance_error->diagnostic_only.reserved->token == 7);
    CHECK(partial.error().maintenance_error->diagnostic_only.reserved->timestamp == 1);
    const std::array<std::uint32_t, 3> unsupported{6, 31, 999};
    const auto stopped = run(maintenance, unsupported, runtime);
    REQUIRE_FALSE(stopped);
    CHECK(stopped.error().stage == load::WildlifeImportPlanStage::Materialization);
    CHECK(stopped.error().draws.maintenance == 5);
    CHECK(stopped.error().draws.selector_only == 2);
    REQUIRE(stopped.error().materialization_error);
    CHECK(stopped.error().materialization_error->failing_record.token == 1001);
    REQUIRE(stopped.error().materialization_error->failing_record.selection);
    CHECK(stopped.error().materialization_error->failing_record.selection->selected.id.v == 503);
    CHECK(stopped.error().materialization_error->diagnostic_only.unused_draws ==
          std::vector<std::uint32_t>{999});
    const auto again = run(maintenance, selectors, runtime);
    REQUIRE(again);
    CHECK(again.value().maintenance.completed.chunk == out.maintenance.completed.chunk);
    CHECK(materialize_summary(again.value().materialization.completed) ==
          materialize_summary(out.materialization.completed));
    auto collision = runtime;
    collision.fresh_tokens = {3}; // Off-target source node is still in source token namespace.
    const auto refused = run(maintenance, selectors, collision);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().stage == load::WildlifeImportPlanStage::Runtime);
    CHECK(refused.error().supplied.flags.provenance == runtime.flags.provenance);
    CHECK(rig.prepared.fill.prefix_consumed == 100);
    CHECK(rig.prepared.fill.island.spawns.size() == 3);
    CHECK(rig.prepared.fill.island.resource_list_nodes[0].mask == 17);
    CHECK(rig.prepared.fill.island.occurrences[0].discovery == 7);
    auto no_provenance = runtime;
    no_provenance.provenance.clear();
    const auto no_context = run(maintenance, selectors, no_provenance);
    REQUIRE_FALSE(no_context);
    CHECK(no_context.error().stage == load::WildlifeImportPlanStage::Runtime);
    CHECK(no_context.error().supplied.flags.provenance == runtime.flags.provenance);
    const auto short_selector =
        run(maintenance, std::span<const std::uint32_t>(selectors).first(1), runtime);
    REQUIRE_FALSE(short_selector);
    CHECK(short_selector.error().draws.selector_only == 1);
    REQUIRE(short_selector.error().materialization_error);
    CHECK(short_selector.error().materialization_error->failing_record.token == 1001);
    auto different = species;
    different.climate = 5;
    const auto mismatch = load::prepare_wildlife_import_plan(
        rig.prepared, rig.scene, 1, 1, runtime, final_profile, rig.metadata.source.entries,
        rig.metadata.source.image, *rig.metadata.types, different, maintenance, selectors);
    REQUIRE_FALSE(mismatch);
    CHECK(mismatch.error().stage == load::WildlifeImportPlanStage::Materialization);
    CHECK(mismatch.error().draws.selector_only == 0);
    CHECK(mismatch.error().imported->sidecars.size() == 2);
}
TEST_CASE(
    "wildlife import plan: malformed projections chronology and unknown flag context refuse") {
    ImportRig rig;
    REQUIRE(rig.reconstruct());
    const auto original = rig.prepared;
    for (unsigned variant = 0; variant < 17; ++variant) {
        rig.prepared = original;
        auto& is = rig.prepared.fill.island;
        if (variant == 0)
            is.chunk_heads[5] = std::uint64_t{0}; // Omits later spawns.
        if (variant == 1)
            is.spawns[2].next_in_chunk = std::uint64_t{2}; // Cycle, never recursively walked.
        if (variant == 2)
            is.spawns[0].source.header = 5; // Not a HIRSCH header.
        if (variant == 3)
            is.spawns[0].chunk = 6;
        if (variant == 4)
            is.spawns[1].source = is.spawns[0].source; // Duplicate token/source mapping.
        if (variant == 5)
            is.resource_list_nodes[4].next = std::uint64_t{4};
        if (variant == 6)
            is.resource_list_nodes[0].entry = 99;
        if (variant == 7)
            is.resource_list_nodes[4].kind = 2;
        if (variant == 8)
            is.occurrence_loads[1].created = true;
        if (variant == 9)
            is.resource_list_nodes[1].chunk = 5; // Wrong multi-chunk insertion ordinal.
        if (variant == 10)
            is.occurrence_loads[1].occurrence = 99;
        if (variant == 11)
            is.resource_list_chunks[5].resource_flags = 0;
        if (variant == 12)
            is.spawns[0].source.island = 1;
        if (variant == 13)
            ++is.spawns[0].source.header_offset;
        if (variant == 14)
            is.spawns[2].next_in_chunk = std::uint64_t{999};
        if (variant == 15)
            is.spawns.pop_back();
        if (variant == 16)
            is.spawns[0].source.record = 99;
        const auto result = rig.reconstruct();
        REQUIRE_FALSE(result);
        CHECK(result.error().supplied_flags.non_presence == 0x8000);
        CHECK(result.error().supplied_flags.provenance == "authored omitted flags");
        CHECK(result.error().prefix_draws == 100);
    }
    rig.prepared = original;
    for (const auto& flags :
         std::vector<load::WildlifeImportFlags>{{{}, "unknown omitted flags"},
                                                {std::uint16_t{0}, ""},
                                                {std::uint16_t{1}, "overlaps spawn bit"},
                                                {std::uint16_t{2}, "overlaps BERG bit"},
                                                {std::uint16_t{0x40}, "overlaps occurrence bit"}}) {
        const auto result = rig.reconstruct(flags);
        REQUIRE_FALSE(result);
        CHECK(result.error().stage == load::WildlifeImportStage::Flags);
        CHECK(result.error().supplied_flags.non_presence == flags.non_presence);
        CHECK(result.error().supplied_flags.provenance == flags.provenance);
    }
    const auto valid = rig.reconstruct({std::uint16_t{0}, "explicitly supplied zero"});
    REQUIRE(valid);
    CHECK(valid.value().snapshot.flags == 0x43);
    CHECK(rig.prepared.fill.island.spawns[2].next_in_chunk == std::uint64_t{1});
}
TEST_CASE(
    "wildlife import plan: original Tutorial nonempty spawn chunk matches source chronology") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original import plan unavailable)\n";
        return;
    }
    const auto read = [](const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in);
        return Buffer(std::istreambuf_iterator<char>(in), {});
    };
    const auto scene = read(*dir / "Scenes/Tutorial/01Discover01.sze");
    const auto bytes = read(*dir / "Anlagen.dat");
    const auto entries = fmt::anlagen::AnlagenFile::parse(bytes);
    REQUIRE(entries);
    const auto game = read(*dir / "AnnoGame.dll");
    const auto welt = read(*dir / "AnnoWelt.dll");
    const auto figure_bytes = read(*dir / "FigurenHeader.dat");
    const auto figures = fmt::figuren::FigurenHeader::parse(figure_bytes);
    REQUIRE(figures);
    const auto registry = load::load_original_registry(*dir, load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    load::PrefixInputs inputs;
    inputs.sampled_stamps = {500};
    inputs.stamp_provenance = "authored stamp 500";
    inputs.mode_provenance = "authored mode 0";
    const auto prepared = load::prepare_wildlife_first_call(scene, {0, 11}, entries.value().entries,
                                                            game, *registry.types, inputs,
                                                            std::vector<std::uint32_t>(20000, 7));
    REQUIRE_MESSAGE(prepared, (prepared ? "" : prepared.error().reason));
    const load::WildlifeImportFlags flags{std::uint16_t{0x8000},
                                          "authored non-presence flags, not recovered"};
    const auto imported = load::reconstruct_wildlife_import(prepared.value(), scene, 10, 2, flags);
    REQUIRE_MESSAGE(imported, (imported ? "" : imported.error().reason));
    const auto& out = imported.value();
    REQUIRE(out.snapshot.records.size() == 1); // ACTUAL imported spawn, never empty-chunk proof.
    CHECK(out.source_token_count == 177);
    CHECK(out.prefix_draws == 4745);
    CHECK(out.snapshot.flags == 0x8001);
    CHECK(out.derived_presence == 1);
    CHECK(out.sidecars.empty());
    const auto& record = out.snapshot.records[0];
    CHECK(record.token == 21); // 20 BERG nodes + one occurrence precede first source spawn.
    CHECK(record.position == 47);
    CHECK(record.timestamp == 8);
    CHECK(record.goods == 0);
    CHECK_FALSE(record.saved_id);
    CHECK_FALSE(record.high_bit);
    CHECK_FALSE(record.byte11);
    REQUIRE(out.order.size() == 1);
    CHECK(out.order[0].source.header == 71);
    CHECK(out.order[0].source.record == 0);
    std::string row = std::to_string(out.order[0].token) + " " +
                      std::to_string(out.order[0].category) + " " +
                      std::to_string(out.order[0].source.header) + " " +
                      std::to_string(out.order[0].source.record) + " " +
                      std::to_string(out.order[0].insertion_ordinal) + "\n";
    if (const char* path = std::getenv("ANNO_WILDLIFE_IMPORT_ORACLE")) {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in);
        CHECK(row == std::string(std::istreambuf_iterator<char>(in), {}));
    }
    load::WildlifeImportRuntime runtime;
    runtime.time = 10000;
    runtime.flags = flags;
    runtime.provenance = "authored sweep time, no new allocation expected";
    const load::WildlifeSpeciesSource species{
        welt, game, figures.value().entries, fmt::annowelt::WildlifeSpeciesProfile::GogClimate2, 2};
    const std::array<std::uint32_t, 1> maintenance{999};
    const std::array<std::uint32_t, 3> selectors{6, 0, 999};
    const auto plan = load::prepare_wildlife_import_plan(
        prepared.value(), scene, 10, 2, runtime, final_profile, entries.value().entries, game,
        *registry.types, species, maintenance, selectors);
    REQUIRE_MESSAGE(plan, (plan ? "" : plan.error().reason));
    CHECK(plan.value().draws.imported_prefix == 4745);
    CHECK(plan.value().draws.maintenance == 0);
    CHECK(plan.value().draws.selector_only == 2);
    CHECK(plan.value().maintenance.completed.chunk == out.snapshot);
    CHECK(plan.value().unused_maintenance == std::vector<std::uint32_t>{999});
    const auto& selected = plan.value().materialization.completed;
    REQUIRE(selected.entries.size() == 1);
    REQUIRE(selected.entries[0].selection);
    CHECK(selected.entries[0].token == 21);
    CHECK(selected.entries[0].selection->choice.value.index == 2);
    CHECK(selected.entries[0].selection->choice.value.inspected == 3);
    CHECK(selected.entries[0].selection->selected.id.v == 528);
    CHECK(selected.unused_draws == std::vector<std::uint32_t>{999});
    CHECK(selected.unchanged_chunk.completed.chunk == out.snapshot);
    CHECK(prepared.value().fill.island.spawns.size() == 156);
    CHECK(prepared.value().fill.island.resource_list_nodes.size() == 21);
}
namespace {
load::WildlifeNumericContext geometry_context() {
    const load::WildlifeFpProfile p{53, load::WildlifeRounding::Nearest};
    const auto context = load::prepare_wildlife_numeric_context(
        {p, p, "authored table53/nearest and callback53/nearest, not observed"});
    REQUIRE(context);
    return context.value();
}
load::WildlifeCreationEffects geometry_effects(const load::WildlifeCreationDescriptor& descriptor,
                                               const load::WildlifeOrdinaryGeometry& geometry,
                                               sim::TileRect area) {
    load::WildlifeCreationInputs input;
    input.manager = load::WildlifeManagerOutcome::Success;
    input.assigned_id = 123;
    input.creation_time = 10000;
    input.position_words = geometry.words;
    input.geometry.area = area;
    input.geometry.home_x = geometry.home[0];
    input.geometry.home_z = geometry.home[1];
    input.prior = {17, 8, 9, 0};
    input.sweep_time = 10000;
    input.helper_consumed = 2;
    const auto effects = load::project_wildlife_creation(descriptor, input);
    REQUIRE_MESSAGE(effects, (effects ? "" : effects.error()));
    CHECK(effects.value().helper_consumed == 2);
    REQUIRE(effects.value().animal);
    CHECK(effects.value().animal->position_words == geometry.words);
    CHECK(effects.value().animal->home == geometry.home);
    CHECK(effects.value().animal->engine_id == 123);
    return effects.value();
}
} // namespace
TEST_CASE("wildlife geometry: nonflat prepared source composition ownership and strict admission") {
    const auto numeric = geometry_context();
    const auto owned = [&] {
        ImportRig rig{1, 500, std::array<std::uint8_t, 4>{0, 25, 50, 100}};
        Source animals;
        species_source(animals);
        const auto welt = species_pe();
        const load::WildlifeSpeciesSource species{
            welt, rig.metadata.source.image, animals.entries,
            fmt::annowelt::WildlifeSpeciesProfile::GogClimate2, 2};
        const std::array<std::uint32_t, 6> maintenance{0, 1, 0, 0, 0, 999};
        const std::array<std::uint32_t, 11> selectors{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 999};
        const auto plan = load::prepare_wildlife_import_plan(
            rig.prepared, rig.scene, 1, 1, import_runtime(), final_profile,
            rig.metadata.source.entries, rig.metadata.source.image, *rig.metadata.types, species,
            maintenance, selectors);
        REQUIRE_MESSAGE(plan, (plan ? "" : plan.error().reason));
        const auto before = rig.prepared.fill.island.terrain->height;
        const auto batch = load::bind_wildlife_ordinary_source(
            rig.prepared, plan.value(), numeric,
            "fresh authored HEIGHT through completed first-call");
        REQUIRE_MESSAGE(batch, (batch ? "" : batch.error().reason));
        const auto geometry = batch.value().project(0);
        REQUIRE_MESSAGE(geometry, (geometry ? "" : geometry.error().reason));
        const auto& g = geometry.value();
        // Tile33/49 low offset rounds to43691/2^18; y=3*43691/2^18 =131073/2^18.
        CHECK(g.words == std::array<std::uint32_t, 3>{0x4204aaab, 0x3f000040, 0x4244aaab});
        CHECK(g.corners == std::array<std::uint8_t, 4>{0, 25, 50, 100});
        CHECK(g.home == std::array<std::uint32_t, 2>{33, 49});
        CHECK(g.triangle == 1);
        CHECK(g.corner_reads == 4);
        CHECK(g.actual_object == 105);
        // Four source resource origins now have means 24/22/30/28 below 32:
        // (8,8),(9,8),(10,8),(8,9). 99-4 resource draws +1 jitter =96.
        CHECK(g.unchanged_draws.imported_prefix == 96);
        CHECK(g.unchanged_draws.maintenance == 5);
        CHECK(g.unchanged_draws.selector_only == 10);
        CHECK(g.scene_sha256 == rig.prepared.source_sha256);
        CHECK(g.corner_provenance == "fresh authored HEIGHT through completed first-call");
        CHECK(rig.prepared.fill.island.terrain->height == before);
        auto selected = std::find_if(plan.value().materialization.completed.entries.begin(),
                                     plan.value().materialization.completed.entries.end(),
                                     [](const auto& e) { return e.token == 0; });
        REQUIRE(selected != plan.value().materialization.completed.entries.end());
        REQUIRE(selected->selection);
        REQUIRE(selected->selection->creation);
        const auto effects = geometry_effects(*selected->selection->creation, g, {24, 40, 32, 24});
        CHECK(effects.animal->type.v == 60001);
        CHECK(batch.value().project(0).value().words == g.words);
        CHECK_FALSE(batch.value().project(999999));
        CHECK_FALSE(load::bind_wildlife_ordinary_source(rig.prepared, plan.value(), numeric, ""));
        auto mismatch = rig.prepared;
        mismatch.source_sha256 = "wrong";
        CHECK_FALSE(load::bind_wildlife_ordinary_source(mismatch, plan.value(), numeric, "fresh"));
        mismatch = rig.prepared;
        mismatch.fill.island.terrain.reset();
        CHECK_FALSE(load::bind_wildlife_ordinary_source(mismatch, plan.value(), numeric, "fresh"));
        mismatch = rig.prepared;
        auto corrupt = std::make_shared<sim::Terrain>(*mismatch.fill.island.terrain);
        corrupt->height[0] ^= 1;
        mismatch.fill.island.terrain = corrupt;
        CHECK_FALSE(load::bind_wildlife_ordinary_source(mismatch, plan.value(), numeric, "fresh"));
        auto wrong = plan.value();
        for (auto& e : wrong.materialization.completed.entries)
            if (e.token == 0) {
                e.tile->object_word = 104;
                e.type->object_word = 104;
            }
        const auto wrong_source =
            load::bind_wildlife_ordinary_source(rig.prepared, wrong, numeric, "fresh");
        REQUIRE_FALSE(wrong_source);
        CHECK(wrong_source.error().stage == load::WildlifeGeometryStage::Source);
        mismatch = rig.prepared;
        auto short_heights = before;
        short_heights.pop_back();
        mismatch.fill.island.terrain =
            sim::make_terrain(short_heights, rig.prepared.fill.island.terrain->gfx, {});
        CHECK_FALSE(load::bind_wildlife_ordinary_source(mismatch, plan.value(), numeric, "fresh"));
        auto cave = plan.value();
        for (auto& e : cave.materialization.completed.entries)
            if (e.token == 0)
                e.selection->choice.value.position = -1;
        CHECK_FALSE(load::bind_wildlife_ordinary_source(rig.prepared, cave, numeric, "fresh"));
        return std::pair{batch.value(), g};
    }();
    // All source scene/metadata/plan owners destroyed; batch and output remain owned.
    const auto result = owned.first.project(0);
    REQUIRE(result);
    CHECK(result.value().words == owned.second.words);
    CHECK(result.value().scene_sha256 == owned.second.scene_sha256);
}
TEST_CASE(
    "wildlife geometry: original Tutorial source words compose into supplied creation effects") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout
            << "SKIPPED: ANNO1503_GAME_DIR not set (original ordinary geometry unavailable)\n";
        return;
    }
    const auto read = [](const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in);
        return Buffer(std::istreambuf_iterator<char>(in), {});
    };
    const auto scene = read(*dir / "Scenes/Tutorial/01Discover01.sze"),
               game = read(*dir / "AnnoGame.dll"), welt = read(*dir / "AnnoWelt.dll");
    const auto data = read(*dir / "Anlagen.dat"), figdata = read(*dir / "FigurenHeader.dat");
    const auto entries = fmt::anlagen::AnlagenFile::parse(data);
    const auto figures = fmt::figuren::FigurenHeader::parse(figdata);
    REQUIRE(entries);
    REQUIRE(figures);
    const auto registry = load::load_original_registry(*dir, load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    load::PrefixInputs input;
    input.sampled_stamps = {500};
    input.stamp_provenance = "authored stamp 500";
    input.mode_provenance = "authored mode 0";
    const auto prepared = load::prepare_wildlife_first_call(scene, {0, 11}, entries.value().entries,
                                                            game, *registry.types, input,
                                                            std::vector<std::uint32_t>(20000, 7));
    REQUIRE(prepared);
    load::WildlifeImportRuntime runtime;
    runtime.time = 10000;
    runtime.flags = {std::uint16_t{0x8000}, "authored non-presence context"};
    runtime.provenance = "authored time/no allocation";
    const load::WildlifeSpeciesSource species{
        welt, game, figures.value().entries, fmt::annowelt::WildlifeSpeciesProfile::GogClimate2, 2};
    const std::array<std::uint32_t, 1> maintenance{999};
    const std::array<std::uint32_t, 3> selectors{6, 0, 999};
    const auto plan = load::prepare_wildlife_import_plan(
        prepared.value(), scene, 10, 2, runtime, final_profile, entries.value().entries, game,
        *registry.types, species, maintenance, selectors);
    REQUIRE_MESSAGE(plan, (plan ? "" : plan.error().reason));
    const auto batch = load::bind_wildlife_ordinary_source(
        prepared.value(), plan.value(), geometry_context(),
        "fresh original completed HEIGHT, authored numeric context");
    REQUIRE_MESSAGE(batch, (batch ? "" : batch.error().reason));
    const auto geometry = batch.value().project(21);
    REQUIRE_MESSAGE(geometry, (geometry ? "" : geometry.error().reason));
    const auto& g = geometry.value();
    // Independent prior raw decode and hand words, not current C++/Python generated expected
    // values.
    CHECK(g.words == std::array<std::uint32_t, 3>{0x42dfaaab, 0x3ff5c28f, 0x42375555});
    CHECK(g.corners == std::array<std::uint8_t, 4>{48, 48, 48, 48});
    CHECK(g.home == std::array<std::uint32_t, 2>{111, 45});
    CHECK(g.actual_object == 419);
    CHECK(g.triangle == 2);
    CHECK(g.corner_reads == 4);
    CHECK(g.unchanged_draws.imported_prefix == 4745);
    CHECK(g.unchanged_draws.maintenance == 0);
    CHECK(g.unchanged_draws.selector_only == 2);
    const auto& selection = plan.value().materialization.completed.entries[0].selection;
    REQUIRE(selection);
    REQUIRE(selection->creation);
    const auto effects = geometry_effects(*selection->creation, g, {24, 24, 128, 128});
    CHECK(effects.animal->type.v == 528);
    CHECK(effects.animal->yield == 0);
    CHECK(effects.spawn.id == 123);
    CHECK(plan.value().materialization.completed.unused_draws == std::vector<std::uint32_t>{999});
    CHECK(plan.value().unused_maintenance == std::vector<std::uint32_t>{999});
}
TEST_CASE("wildlife geometry: source batch forwards independent table and callback profiles") {
    ImportRig rig{1, 500, std::array<std::uint8_t, 4>{248, 248, 248, 248}};
    Source animals;
    species_source(animals);
    const auto welt = species_pe();
    const load::WildlifeSpeciesSource species{welt, rig.metadata.source.image, animals.entries,
                                              fmt::annowelt::WildlifeSpeciesProfile::GogClimate2,
                                              2};
    const std::array<std::uint32_t, 6> maintenance{0, 1, 0, 0, 0, 999};
    const std::array<std::uint32_t, 11> selectors{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 999};
    const auto plan = load::prepare_wildlife_import_plan(
        rig.prepared, rig.scene, 1, 1, import_runtime(), final_profile, rig.metadata.source.entries,
        rig.metadata.source.image, *rig.metadata.types, species, maintenance, selectors);
    REQUIRE_MESSAGE(plan, (plan ? "" : plan.error().reason));
    using R = load::WildlifeRounding;
    struct Case {
        R table, callback;
        std::array<std::uint32_t, 3> expected;
    };
    // a*2^18=43690+171/256. Nearest takes43691, down43690 at both world33/49.
    // 248*c=166429979/2^24; stored table nearest chooses166429984, down166429968.
    // Flat positive H interpolates to H exactly, even under downward callback RC.
    const std::array<Case, 3> cases{{{R::Nearest, R::Nearest, {0x4204aaab, 0x411eb852, 0x4244aaab}},
                                     {R::Nearest, R::Down, {0x4204aaaa, 0x411eb852, 0x4244aaaa}},
                                     {R::Down, R::Nearest, {0x4204aaab, 0x411eb851, 0x4244aaab}}}};
    const auto before = rig.prepared.fill.island.terrain->height;
    for (const auto& c : cases) {
        INFO("table rounding=" << static_cast<unsigned>(c.table)
                               << " callback=" << static_cast<unsigned>(c.callback));
        const auto context = load::prepare_wildlife_numeric_context(
            {load::WildlifeFpProfile{53, c.table}, load::WildlifeFpProfile{53, c.callback},
             "authored independent source-batch profiles"});
        REQUIRE(context);
        const auto batch = load::bind_wildlife_ordinary_source(
            rig.prepared, plan.value(), context.value(), "fresh authored flat 248 HEIGHT patch");
        REQUIRE_MESSAGE(batch, (batch ? "" : batch.error().reason));
        const auto geometry = batch.value().project(0);
        REQUIRE_MESSAGE(geometry, (geometry ? "" : geometry.error().reason));
        INFO("actual XYZ=" << geometry.value().words[0] << "/" << geometry.value().words[1] << "/"
                           << geometry.value().words[2] << " expected=" << c.expected[0] << "/"
                           << c.expected[1] << "/" << c.expected[2]);
        CHECK(geometry.value().words == c.expected);
        CHECK(geometry.value().corners == std::array<std::uint8_t, 4>{248, 248, 248, 248});
        CHECK(geometry.value().home == std::array<std::uint32_t, 2>{33, 49});
        CHECK(geometry.value().corner_reads == 4);
        CHECK(geometry.value().unchanged_draws.imported_prefix == 100);
        CHECK(geometry.value().unchanged_draws.maintenance == 5);
        CHECK(geometry.value().unchanged_draws.selector_only == 10);
    }
    CHECK(rig.prepared.fill.island.terrain->height == before);
}
