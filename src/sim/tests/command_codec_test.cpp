// Stage S8: the wire codec of the commands (docs/sim/state.md §8.5): exact bytes,
// round trips, canonical decoding, hostile lengths and counts, fuzz.
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>

#include "alloc_probe.hpp"

namespace sim = anno::sim;

namespace {

using Bytes = std::vector<std::uint8_t>;

sim::CityRef city(std::uint32_t island, std::uint32_t no) {
    return sim::CityRef{sim::IslandId{island}, sim::CityNo{no}};
}

/// One body of every alternative, with values that use every byte of every field.
std::vector<sim::Body> samples() {
    std::vector<sim::Body> v;
    sim::BuildCmd build;
    build.city = city(3, 1);
    build.placements = {
        {sim::TypeId{9001}, 110, -7, 2, 0x11, 0x22},
        {sim::TypeId{0x01020304}, -2147483647 - 1, 2147483647, 3, 0, 255},
    };
    v.emplace_back(build);
    v.emplace_back(sim::BuildCmd{city(0, 0), {}});
    sim::DemolishCmd demolish;
    demolish.city = city(0xFFFF, 0x10000);
    demolish.tiles = {{1, 2}, {-3, 4}, {0x7FFFFFFF, -0x7FFFFFFF}};
    v.emplace_back(demolish);
    v.emplace_back(sim::DemolishCmd{city(1, 2), {}});
    v.emplace_back(sim::SetProductionLineCmd{{110, 110}, 0x2a, false});
    v.emplace_back(sim::SetProductionLineCmd{{-1, 65536}, 0xFFFFFFFFU, true});
    v.emplace_back(sim::SetTradeFlagCmd{city(0, 3), 9, 0x8e, 0xFFFFFFFFU});
    v.emplace_back(sim::SetTradeFlagCmd{city(7, 7), 0, 0, 0});
    v.emplace_back(sim::SetTradeSlotCmd{city(2, 2), 1, 5, 0x20, 0x12345678, 0x9abcdef0});
    v.emplace_back(sim::SetTradeSlotCmd{city(2, 2), 0, sim::kAppendIndex, 0, 0, 0});
    v.emplace_back(sim::SetSpeedCmd{200});
    v.emplace_back(sim::SetSpeedCmd{0xFFFFFFFFU});
    v.emplace_back(sim::LegacyRawCmd{0x6a, {1, 2, 3}});
    v.emplace_back(sim::LegacyRawCmd{0x8a, {}});
    v.emplace_back(sim::LegacyRawCmd{0, {0xff}});
    v.emplace_back(sim::LegacyRawCmd{0xFFFF, Bytes(300, 0xAB)});
    return v;
}

Bytes encoded(const sim::Body& b) {
    auto r = sim::encode_record(b);
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return r.value();
}

Bytes header(std::uint16_t type, std::uint16_t len) {
    return {static_cast<std::uint8_t>(type & 0xff), static_cast<std::uint8_t>(type >> 8),
            static_cast<std::uint8_t>(len & 0xff), static_cast<std::uint8_t>(len >> 8)};
}

Bytes record(std::uint16_t type, const Bytes& payload) {
    Bytes b = header(type, static_cast<std::uint16_t>(payload.size()));
    b.insert(b.end(), payload.begin(), payload.end());
    return b;
}

void le32(Bytes& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

bool decodes(const Bytes& b) {
    return sim::decode_record(b).ok();
}

std::string error_of(const Bytes& b) {
    auto r = sim::decode_record(b);
    return r.ok() ? std::string() : r.error();
}

} // namespace

TEST_CASE("command wire: the alternative order and the native types are the format") {
    // The variant's alternative order is the codec's tag order and part of the replay format
    // (append only): pin it.
    static_assert(std::is_same_v<std::variant_alternative_t<0, sim::Body>, sim::BuildCmd>);
    static_assert(std::is_same_v<std::variant_alternative_t<1, sim::Body>, sim::DemolishCmd>);
    static_assert(
        std::is_same_v<std::variant_alternative_t<2, sim::Body>, sim::SetProductionLineCmd>);
    static_assert(std::is_same_v<std::variant_alternative_t<3, sim::Body>, sim::SetTradeFlagCmd>);
    static_assert(std::is_same_v<std::variant_alternative_t<4, sim::Body>, sim::SetTradeSlotCmd>);
    static_assert(std::is_same_v<std::variant_alternative_t<5, sim::Body>, sim::SetSpeedCmd>);
    static_assert(std::is_same_v<std::variant_alternative_t<6, sim::Body>, sim::LegacyRawCmd>);
    static_assert(std::variant_size_v<sim::Body> == 7);
    CHECK(sim::kCommandWireVersion == 1);
    CHECK(sim::kWireBuild == 0x64);
    CHECK(sim::kWireDemolish == 0x68);
    CHECK(sim::kWireSetTradeSlot == 0x73);
    CHECK(sim::kWireSetProductionLine == 0x74);
    CHECK(sim::kWireSetSpeed == 0x7a);
    CHECK(sim::kWireSetTradeFlag == 0x7c);
    int natives = 0;
    for (std::uint32_t t = 0; t <= 0xFFFF; ++t) {
        natives += sim::is_native_wire_type(static_cast<std::uint16_t>(t)) ? 1 : 0;
    }
    CHECK(natives == 6);
    const auto all = samples();
    CHECK(sim::wire_type(all[0]) == 0x64);
    CHECK(sim::wire_type(all[2]) == 0x68);
    CHECK(sim::wire_type(all[4]) == 0x74);
    CHECK(sim::wire_type(all[6]) == 0x7c);
    CHECK(sim::wire_type(all[8]) == 0x73);
    CHECK(sim::wire_type(all[10]) == 0x7a);
    CHECK(sim::wire_type(all[12]) == 0x6a);
}

TEST_CASE("command wire: exact bytes of every native record") {
    // Little-endian, widened ids, a u16 type and a u16 length in front (§8.5).
    CHECK(encoded(sim::SetSpeedCmd{200}) == Bytes{0x7a, 0, 4, 0, 200, 0, 0, 0});
    CHECK(encoded(sim::SetProductionLineCmd{{110, -2}, 0x2a, true}) ==
          Bytes{0x74, 0, 13, 0, 110, 0, 0, 0, 0xfe, 0xff, 0xff, 0xff, 0x2a, 0, 0, 0, 1});
    CHECK(encoded(sim::SetTradeFlagCmd{city(1, 2), 9, 0x8e, 5}) ==
          Bytes{0x7c, 0, 18, 0, 1, 0, 0, 0, 2, 0, 0, 0, 9, 0, 0x8e, 0, 0, 0, 5, 0, 0, 0});
    CHECK(encoded(sim::SetTradeSlotCmd{city(1, 2), 3, 4, 5, 6, 7}) ==
          Bytes{0x73, 0, 28, 0, 1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0,
                4,    0, 0,  0, 5, 0, 0, 0, 6, 0, 0, 0, 7, 0, 0, 0});
    sim::DemolishCmd d;
    d.city = city(1, 2);
    d.tiles = {{3, 4}};
    CHECK(encoded(d) ==
          Bytes{0x68, 0, 20, 0, 1, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0});
    sim::BuildCmd b;
    b.city = city(1, 2);
    b.placements = {{sim::TypeId{0x0102}, 5, 6, 3, 7, 8}};
    CHECK(encoded(b) == Bytes{0x64, 0, 28, 0, 1, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0,
                              2,    1, 0,  0, 5, 0, 0, 0, 6, 0, 0, 0, 3, 7, 8, 0});
    CHECK(encoded(sim::LegacyRawCmd{0x6a, {9, 8}}) == Bytes{0x6a, 0, 2, 0, 9, 8});
}

TEST_CASE("command wire: encode then decode is the identity") {
    for (const sim::Body& body : samples()) {
        const Bytes wire = encoded(body);
        auto back = sim::decode_record(wire);
        REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
        CHECK(back.value() == body);
        CHECK(back.value().index() == body.index());
        // The decoding is canonical: encoding the decoded body gives the same bytes.
        CHECK(encoded(back.value()) == wire);
    }
    // A turn entry adds the host's issuer and sequence number.
    for (const sim::Body& body : samples()) {
        const sim::TurnEntry e{sim::Command{sim::PlayerId{5}, body}, 0xdeadbeefU};
        auto wire = sim::encode_turn_entry(e);
        REQUIRE(wire.ok());
        CHECK(wire.value().size() == 8 + encoded(body).size());
        CHECK(wire.value()[0] == 5);
        CHECK(wire.value()[4] == 0xef);
        auto back = sim::decode_turn_entry(wire.value());
        REQUIRE(back.ok());
        CHECK(back.value() == e);
    }
    // The issuer is data of the entry, not of the record: an invalid id survives too.
    const sim::TurnEntry odd{sim::Command{sim::PlayerId{}, sim::SetSpeedCmd{100}}, 0};
    auto wire = sim::encode_turn_entry(odd);
    REQUIRE(wire.ok());
    auto back = sim::decode_turn_entry(wire.value());
    REQUIRE(back.ok());
    CHECK(!back.value().command.issuer.valid());
    CHECK(sim::decode_turn_entry(Bytes{1, 2, 3}).ok() == false);
    CHECK(sim::decode_turn_entry(Bytes{}).ok() == false);
    // A bad record behind a good header is still an error.
    CHECK(sim::decode_turn_entry(Bytes{0, 0, 0, 0, 0, 0, 0, 0, 0x7a, 0, 4, 0, 1}).ok() == false);
}

TEST_CASE("command wire: decode rejects length and count lies") {
    // Shorter than the header.
    CHECK(!decodes({}));
    CHECK(!decodes({0x7a}));
    CHECK(!decodes({0x7a, 0, 4}));
    // A length beyond the input, and trailing bytes behind the record.
    CHECK(!decodes({0x7a, 0, 4, 0, 1, 0, 0}));
    CHECK(!decodes({0x7a, 0, 0xff, 0xff}));
    CHECK(!decodes({0x7a, 0, 4, 0, 1, 0, 0, 0, 0}));
    CHECK(!decodes({0x6a, 0, 0, 0, 0}));
    CHECK(decodes({0x6a, 0, 0, 0}));
    CHECK(error_of({0x7a, 0, 4, 0, 1, 0, 0, 0, 0}).find("trailing") != std::string::npos);
    CHECK(error_of({0x7a, 0, 9, 0, 1}).find("beyond") != std::string::npos);

    // Every native record with every wrong payload length up to a little past its size.
    // (The counted records have a 12-byte head: an all-zero payload of exactly that length
    // is the one valid empty record, any other all-zero length is wrong.)
    struct Native {
        std::uint16_t type;
        std::size_t size;
    };
    const Native natives[] = {{0x74, 13}, {0x7c, 18}, {0x73, 28},
                              {0x7a, 4},  {0x64, 12}, {0x68, 12}};
    for (const Native& n : natives) {
        for (std::size_t len = 0; len <= n.size + 20; ++len) {
            const Bytes rec = record(n.type, Bytes(len, 0));
            CHECK_MESSAGE(decodes(rec) == (len == n.size), "type " << n.type << " len " << len);
        }
    }

    // A count that lies: more elements than the payload holds, fewer, or one that would
    // wrap a 32-bit product. The error comes before any allocation: the decoder reserves
    // nothing for a count it has not checked against the bytes that are left.
    for (const std::uint16_t type : {std::uint16_t{0x64}, std::uint16_t{0x68}}) {
        const std::size_t element = type == 0x64 ? 16 : 8;
        for (const std::uint32_t n : {1U, 2U, 0xFFFFFFFFU, 0x10000000U, 0x20000001U, 0x08000001U,
                                      0x0FFFFFFFU, 0x7FFFFFFFU, 0x80000000U}) {
            Bytes payload;
            le32(payload, 0);
            le32(payload, 0);
            le32(payload, n);
            payload.resize(12 + element * 2, 0); // room for two elements
            const std::uint32_t want = 2;
            const bool exact = n == want;
            const anno::test::AllocScope probe;
            const bool ok = decodes(record(type, payload));
            CHECK(ok == exact);
            // 2 elements of at most 24 bytes each plus the container: far below 1 KiB; a
            // reserve of n elements would be gigabytes.
            CHECK(probe.bytes() < 1024);
        }
        // Fewer elements declared than present: rejected (trailing element bytes).
        Bytes payload;
        le32(payload, 0);
        le32(payload, 0);
        le32(payload, 1);
        payload.resize(12 + element * 2, 0);
        CHECK(!decodes(record(type, payload)));
    }

    // A flag byte that is not 0 or 1, and a reserved byte that is not 0.
    Bytes line;
    le32(line, 1);
    le32(line, 2);
    le32(line, 3);
    line.push_back(2);
    CHECK(!decodes(record(0x74, line)));
    line.back() = 1;
    CHECK(decodes(record(0x74, line)));
    line.back() = 0;
    CHECK(decodes(record(0x74, line)));
    line.back() = 0xff;
    CHECK(!decodes(record(0x74, line)));
    Bytes build;
    le32(build, 0);
    le32(build, 0);
    le32(build, 1);
    le32(build, 9001);
    le32(build, 1);
    le32(build, 1);
    build.insert(build.end(), {0, 0, 0, 0});
    CHECK(decodes(record(0x64, build)));
    build.back() = 1;
    CHECK(!decodes(record(0x64, build)));
}

TEST_CASE("command wire: legacy raw never carries a native tag") {
    // A record of a native type is always that native body or an error, never LegacyRaw;
    // every other type is LegacyRaw with its own type. Checked over all 65536 types.
    for (std::uint32_t t = 0; t <= 0xFFFF; ++t) {
        const auto type = static_cast<std::uint16_t>(t);
        auto r = sim::decode_record(record(type, {}));
        if (sim::is_native_wire_type(type)) {
            // An empty payload is the wrong size for every native record (the counted ones
            // need their 12-byte head).
            CHECK(!r.ok());
        } else {
            REQUIRE(r.ok());
            const auto* raw = std::get_if<sim::LegacyRawCmd>(&r.value());
            REQUIRE(raw != nullptr);
            CHECK(raw->type == type);
            CHECK(raw->payload.empty());
        }
    }
    // Every decoded body that is a LegacyRaw has a non-native type, whatever the bytes.
    std::uint32_t x = 99;
    for (int i = 0; i < 20000; ++i) {
        Bytes rec;
        const std::size_t len = (x >> 8) % 20;
        x = x * 1664525U + 1013904223U;
        rec = header(static_cast<std::uint16_t>(0x60 + ((x >> 16) % 0x30)),
                     static_cast<std::uint16_t>(len));
        for (std::size_t k = 0; k < len; ++k) {
            x = x * 1664525U + 1013904223U;
            rec.push_back(static_cast<std::uint8_t>(x >> 24));
        }
        auto r = sim::decode_record(rec);
        if (r.ok()) {
            if (const auto* raw = std::get_if<sim::LegacyRawCmd>(&r.value())) {
                CHECK(!sim::is_native_wire_type(raw->type));
            }
        }
    }
    // Encoding a LegacyRaw with a native type is refused (it could not be told from the
    // native record, and would change meaning on decode).
    for (const std::uint16_t t : {std::uint16_t{0x64}, std::uint16_t{0x68}, std::uint16_t{0x73},
                                  std::uint16_t{0x74}, std::uint16_t{0x7a}, std::uint16_t{0x7c}}) {
        auto r = sim::encode_record(sim::LegacyRawCmd{t, {1, 2, 3, 4}});
        CHECK(!r.ok());
        auto e = sim::encode_turn_entry(
            sim::TurnEntry{sim::Command{sim::PlayerId{0}, sim::LegacyRawCmd{t, {}}}, 0});
        CHECK(!e.ok());
    }
    CHECK(sim::encode_record(sim::LegacyRawCmd{0x75, {1}}).ok());
}

TEST_CASE("command wire: the u16 length bounds the record") {
    // 12 + 4093 * 16 = 65500 fits; 4096 placements (65548) do not.
    sim::BuildCmd b;
    b.city = city(0, 0);
    b.placements.assign(4093, sim::PlacementWire{sim::TypeId{1}, 1, 2, 0, 0, 0});
    auto ok = sim::encode_record(b);
    REQUIRE(ok.ok());
    CHECK(ok.value().size() == 4 + 12 + 4093 * 16);
    auto back = sim::decode_record(ok.value());
    REQUIRE(back.ok());
    CHECK(back.value() == sim::Body{b});
    b.placements.assign(4096, sim::PlacementWire{sim::TypeId{1}, 1, 2, 0, 0, 0});
    CHECK(!sim::encode_record(b).ok());
    // The largest legacy payload.
    CHECK(sim::encode_record(sim::LegacyRawCmd{0x70, Bytes(sim::kMaxRecordPayload, 1)}).ok());
    CHECK(!sim::encode_record(sim::LegacyRawCmd{0x70, Bytes(sim::kMaxRecordPayload + 1, 1)}).ok());
    // The exact limit of a counted record: the last count that fits.
    sim::DemolishCmd d;
    d.city = city(0, 0);
    d.tiles.assign((sim::kMaxRecordPayload - 12) / 8, sim::TileXY{1, 1});
    CHECK(sim::encode_record(d).ok());
    d.tiles.push_back(sim::TileXY{1, 1});
    CHECK(!sim::encode_record(d).ok());
}

TEST_CASE("command wire: fuzz over prefixes, suffixes and single-byte mutations") {
    // No input crashes, over-reads or changes meaning silently: whatever decodes encodes
    // back to exactly the same bytes (the format is canonical), and prefixes of a valid
    // record are always errors.
    std::size_t decoded = 0, rejected = 0;
    for (const sim::Body& body : samples()) {
        const Bytes wire = encoded(body);
        for (std::size_t n = 0; n < wire.size(); ++n) {
            CHECK(!decodes(Bytes(wire.begin(), wire.begin() + static_cast<std::ptrdiff_t>(n))));
        }
        for (const std::uint8_t extra : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{0xff}}) {
            Bytes longer = wire;
            longer.push_back(extra);
            CHECK(!decodes(longer));
        }
        // Mutating one byte: every value of every byte (the wire of a sample is small except
        // the 300-byte legacy one, still cheap).
        for (std::size_t i = 0; i < wire.size(); ++i) {
            for (int v = 0; v < 256; ++v) {
                Bytes m = wire;
                m[i] = static_cast<std::uint8_t>(v);
                auto r = sim::decode_record(m);
                if (r.ok()) {
                    ++decoded;
                    auto again = sim::encode_record(r.value());
                    REQUIRE(again.ok());
                    CHECK(again.value() == m);
                } else {
                    ++rejected;
                    CHECK(!r.error().empty());
                }
            }
        }
    }
    CHECK(decoded > 1000);
    CHECK(rejected > 1000);

    // Random bytes behind a native or near-native header, with the length field right
    // (otherwise the check is only the length test).
    std::uint32_t x = 4242;
    const std::uint16_t types[] = {0x64, 0x68, 0x73, 0x74, 0x7a, 0x7c, 0x6a, 0x8a, 0};
    std::size_t valid = 0;
    for (int i = 0; i < 100000; ++i) {
        x = x * 1664525U + 1013904223U;
        const std::size_t len = (x >> 10) % 70;
        x = x * 1664525U + 1013904223U;
        Bytes rec = header(types[(x >> 16) % 9], static_cast<std::uint16_t>(len));
        for (std::size_t k = 0; k < len; ++k) {
            x = x * 1664525U + 1013904223U;
            // Mostly small values so that flags and reserved bytes are right sometimes.
            rec.push_back(static_cast<std::uint8_t>((x >> 24) < 200 ? 0 : x >> 24));
        }
        auto r = sim::decode_record(rec);
        if (r.ok()) {
            ++valid;
            auto again = sim::encode_record(r.value());
            REQUIRE(again.ok());
            CHECK(again.value() == rec);
        }
    }
    CHECK(valid > 1000);
}
