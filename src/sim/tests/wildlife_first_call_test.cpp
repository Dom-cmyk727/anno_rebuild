#include <array>
#include <utility>

#include <doctest/doctest.h>

#include <anno/formats/chunk.hpp>
#include <anno/formats/sha256.hpp>
#include <anno/sim_load/wildlife_first_call.hpp>
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
Buffer packed(std::string_view name, const Buffer& bytes) {
    fmt::chunk::Chunk c;
    const auto field = fmt::chunk::name_field(name);
    REQUIRE(field);
    c.name_field = *field;
    c.payload = bytes;
    Buffer out;
    REQUIRE(fmt::chunk::write_chunk(c, out));
    return out;
}
void append(Buffer& out, const Buffer& bytes) {
    out.insert(out.end(), bytes.begin(), bytes.end());
}
Buffer fake_pe() {
    // Authored PE/data only; profile constants and RVA geometry, no original bytes/table.
    Buffer b(0x600, 0);
    b.at(0) = 'M';
    b.at(1) = 'Z';
    put32(b, 0x3c, 0x40);
    b.at(0x40) = 'P';
    b.at(0x41) = 'E';
    put16(b, 0x44, 0x14c);
    put16(b, 0x46, 1);
    put32(b, 0x48, 0x44c0c30f);
    put16(b, 0x54, 0xe0);
    put16(b, 0x58, 0x10b);
    put32(b, 0x58 + 0x1c, 0x10000000);
    put32(b, 0x58 + 0x38, 0x2e000);
    const auto sec = 0x58U + 0xe0U;
    put32(b, sec + 12, 0x21000);
    put32(b, sec + 16, 0x400);
    put32(b, sec + 20, 0x200);
    for (unsigned climate = 0; climate < 7; ++climate) {
        for (unsigned i = 0; i < 16; ++i)
            put16(b, 0x200 + 0x14c + climate * 32 + i * 2,
                  static_cast<std::uint16_t>(100 + climate * 16 + i));
        for (unsigned base : {0x22cU, 0x29cU, 0x30cU})
            for (unsigned i = 0; i < 8; ++i)
                put16(b, 0x200 + base + climate * 16 + i * 2,
                      static_cast<std::uint16_t>((base == 0x29c   ? 100U
                                                  : base == 0x30c ? 200U
                                                                  : 300U) +
                                                 climate * 16 + i));
    }
    return b;
}
struct Fixture {
    std::array<Buffer, 3> heads;
    Buffer ware{'B', 'A', 'U', 'M', 0};
    Buffer unused{'U', 'N', 'U', 'S', 'E', 'D', 0};
    std::vector<fmt::figuren::Entry> entries;
    std::shared_ptr<const sim::TypeRegistry> types;
    Buffer pe = fake_pe();
    std::vector<std::uint32_t> draws{4, 7, 91, 123};
    Fixture() {
        sim::TypeRegistry::Builder builder;
        const std::array<std::uint32_t, 3> ids{2, 11, 104};
        for (std::size_t i = 0; i < ids.size(); ++i) {
            auto& head = heads[i];
            head.assign(56, 0);
            head[0] = head[1] = 1;
            head[0x1c] = 0x29;
            head[0x2f] = 3; // Distinguishes a compiled constant grass code 2.
            if (ids[i] == 104)
                put16(head, 0x1a, 104);
            fmt::figuren::Entry e;
            e.id = ids[i];
            fmt::chunk::Chunk h, w;
            h.name_field = *fmt::chunk::name_field("HEAD");
            h.payload = head;
            w.name_field = *fmt::chunk::name_field("WARE");
            w.payload = ids[i] == 104 ? fmt::Bytes(ware) : fmt::Bytes(unused);
            e.records = {h, w};
            entries.push_back(e);
            sim::BuildingType t;
            t.id = sim::TypeId{ids[i]};
            t.width = t.height = 1;
            t.prodtype = 225;
            t.flags_word = 0x29;
            t.head_raw = head;
            builder.add(t);
        }
        const auto built = builder.build();
        REQUIRE(built);
        types = built.value();
    }
    load::PrefixInputs inputs() const {
        return {{500}, 0, "authored stamp 500", "authored mode 0", 8};
    }
};
Buffer block(std::uint16_t x, std::uint16_t y, std::uint8_t w, std::uint8_t h,
             const Buffer& payload) {
    Buffer b;
    fmt::put_u16_le(b, x);
    fmt::put_u16_le(b, y);
    b.push_back(w);
    b.push_back(h);
    append(b, payload);
    return b;
}
Buffer island(bool ground = true, Buffer fertility = Buffer(8, 6), std::string_view extra = "",
              std::uint16_t x = 8, std::uint16_t y = 16, bool duplicate = false,
              bool overlap = false) {
    Buffer b;
    for (auto v : {x, y, std::uint16_t{4}, std::uint16_t{2}})
        fmt::put_u16_le(b, v);
    fmt::put_u32_le(b, 9);
    append(b, packed("INFO", Buffer(12, 0)));
    append(b, packed("HEIGHT", block(0, 0, 4, 2, {0, 0, 64, 64, 0, 0, 64, 64})));
    append(b, packed("GFXID", block(0, 0, 4, 2, Buffer(16, 0))));
    if (overlap)
        append(b, packed("HEIGHT", block(0, 0, 1, 1, {0})));
    Buffer object;
    fmt::put_u16_le(object, 104);
    fmt::put_u16_le(object, 0x507); // signed dx -1: block x 3 becomes cell x 2.
    object.push_back(255);
    object.push_back(0);
    append(b, packed("GELAENDE", block(3, 0, 1, 1, object)));
    if (duplicate)
        append(b, packed("FRUCHTBAR", Buffer(8, 0)));
    if (ground)
        append(b, packed("FRUCHTBAR", fertility));
    append(b, packed("HIRSCH", fmt::insel::encode_hirsch(
                                   std::array{fmt::insel::HirschRecord{0, 0, 0x80, 7, 10}})));
    if (!extra.empty())
        append(b, packed(extra, {}));
    return packed("INSEL5", b);
}
Buffer scene(const Buffer& local) {
    return packed("ANNO", packed("SCENE", local));
}
using Cell = std::array<std::uint32_t, 10>;
Cell cell(const sim::TileLayer& t, std::uint64_t i) {
    return {t.object(i),    t.phase(i),   t.owner(i),   t.aux(i),           t.walk_code(i),
            t.walk_mask(i), t.foot_dx(i), t.foot_dy(i), t.foot_rotation(i), t.foot_anchor(i)};
}
} // namespace

TEST_CASE("wildlife first call: source-built hand fields draws lifetime and repeated ground") {
    std::optional<load::WildlifeFirstCallResult> owned;
    for (bool duplicate : {false, true}) {
        Fixture f;
        const auto file = scene(island(true, Buffer(8, 6), "", 8, 16, duplicate));
        const auto snapshot = file;
        const auto pe = f.pe;
        const auto draws = f.draws;
        auto result = load::prepare_wildlife_first_call(file, {0, 9}, f.entries, f.pe, *f.types,
                                                        f.inputs(), f.draws);
        REQUIRE_MESSAGE(result, (result ? "" : result.error().reason));
        CHECK(file == snapshot);
        CHECK(f.pe == pe);
        CHECK(f.draws == draws);
        CHECK(result.value().source_sha256 == fmt::sha256_hex(file));
        owned = std::move(result).value();
    } // All source/catalogue buffers and original registry owners destroyed.
    REQUIRE(owned);
    const auto& out = owned->fill;
    CHECK(out.fish == 2);
    CHECK(out.grass == 3);
    CHECK(out.prefix_consumed == 2);
    CHECK(out.consumed == 0);
    REQUIRE(out.island.resources.size() == 1);
    CHECK(out.island.resources[0].effect.variant == 4);
    CHECK(out.island.resources[0].draw_before == 0);
    REQUIRE(out.island.spawns.size() == 1);
    CHECK(out.island.spawns[0].timestamp == 8); // 500/500 + raw jitter 7.
    CHECK(out.island.spawns[0].draw_before == 1);
    CHECK(out.island.terrain->height == Buffer{0, 0, 64, 64, 0, 0, 64, 64});
    CHECK(out.island.terrain->fertility == Buffer(8, 6));
    // All channels: object, phase, owner, aux, code, mask, dx, dy, rotation, anchor.
    const std::array<Cell, 8> expected{{{11, 0, 0, 0, 2, 0, 0, 0, 0, 0},
                                        {2, 0, 0, 32, 3, 0, 0, 0, 0, 0},
                                        {104, 7, 0, 64, 3, 0, 0, 0, 0, 1},
                                        {2, 0, 0, 32, 3, 0, 0, 0, 0, 0},
                                        {11, 0, 0, 0, 2, 0, 0, 0, 0, 0},
                                        {0, 0, 0, 0, 2, 0, 0, 0, 0, 0},
                                        {2, 0, 0, 32, 3, 0, 0, 0, 0, 0},
                                        {0, 0, 0, 0, 2, 0, 0, 0, 0, 0}}};
    for (std::uint64_t i = 0; i < expected.size(); ++i) {
        const auto actual = cell(out.island.resource.tiles, i);
        for (std::size_t channel = 0; channel < actual.size(); ++channel) {
            INFO("cell " << i << " channel " << channel);
            CHECK(actual[channel] == expected[i][channel]);
        }
    }
    CHECK(out.island.resource.variation == Buffer{6, 6, 70, 6, 6, 6, 6, 6});
    CHECK(out.island.resource.tiles.cache_consistent());
}

TEST_CASE("wildlife first call: legal zero ground and intended refusal stages are transactional") {
    Fixture f;
    const auto legal = scene(island());
    const auto run = [&](const Buffer& file, load::WildlifeFirstCallSelection selection = {0, 9},
                         load::WildlifeFillRoles roles = {}) {
        return load::prepare_wildlife_first_call(file, selection, f.entries, f.pe, *f.types,
                                                 f.inputs(), f.draws, roles);
    };
    REQUIRE(run(legal));
    const auto zero = run(scene(island(true, Buffer(8, 0))));
    REQUIRE(zero);
    CHECK(zero.value().fill.fish == 2);
    CHECK(zero.value().fill.grass == 0);
    const auto expect = [&](const Buffer& file, load::WildlifeFirstCallStage stage,
                            std::string_view reason, load::WildlifeFirstCallSelection id = {0, 9},
                            load::WildlifeFillRoles roles = {}) {
        const auto saved = file;
        const auto draws = f.draws;
        const auto pe = f.pe;
        const auto result = run(file, id, roles);
        REQUIRE_FALSE(result);
        CHECK(result.error().stage == stage);
        CHECK(result.error().reason == reason);
        CHECK(file == saved);
        CHECK(f.draws == draws);
        CHECK(f.pe == pe);
    };
    using S = load::WildlifeFirstCallStage;
    expect(scene(island(false)), S::FillValidation,
           "wildlife fill: terrain/ground plane shape mismatch");
    expect(scene(island(true, Buffer(7, 6))), S::Prefix,
           "invalid fertility length or poisoned session");
    expect(scene(island(true, Buffer(8, 6), "MYSTERY")), S::Prefix,
           "unsupported intervening header or late terrain/setup callback");
    auto empty_geometry = island();
    put16(empty_geometry, 20, 0); // INSEL5 record header 16 + width offset 4.
    expect(scene(empty_geometry), S::Source,
           "wildlife first call: positive representable full geometry required");
    f.heads[0][0x1c] = 0;
    expect(legal, S::FillValidation, "wildlife fill: role HEAD/registry mismatch");
    f.heads[0][0x1c] = 0x29;
    auto extra = island();
    append(extra, island());
    expect(scene(extra), S::Source, "wildlife first call: single fresh embedded island required");
    expect(legal, S::Source, "wildlife first call: selected island identity mismatch", {1, 9});
    expect(legal, S::Source, "wildlife first call: selected island identity mismatch", {0, 8});
    expect(scene(island(true, Buffer(8, 6), "", 9, 16)), S::Source,
           "wildlife first call: source geometry requires original factory adjustment");
    expect(scene(island(true, Buffer(8, 6), "", 8, 17)), S::Source,
           "wildlife first call: source geometry requires original factory adjustment");
    expect(scene(island(true, Buffer(8, 6), "", 8, 16, false, true)), S::FillValidation,
           "wildlife fill: source terrain blocks incomplete or overlapping");
    expect(legal, S::FillValidation, "wildlife fill: distinct nonzero role ids required", {0, 9},
           {sim::TypeId{2}, sim::TypeId{2}});
    const auto partial = run(scene(island(true, Buffer(8, 6), "MYSTERY")));
    REQUIRE_FALSE(partial);
    REQUIRE(partial.error().diagnostic);
    CHECK(partial.error().diagnostic->consumed == 2); // Diagnostic, never committed consumption.
    CHECK_FALSE(partial.error().diagnostic->complete_local_records);
    const auto repeated = run(legal);
    REQUIRE(repeated);
    CHECK(repeated.value().fill.prefix_consumed == 2);
    CHECK(repeated.value().fill.fish == 2);
    CHECK(repeated.value().fill.grass == 3);
}
