// Authored bytes, not extracted records. Primary: AnnoWelt 0x100946c0–0x10094873.
#include <algorithm>
#include <array>
#include <iostream>
#include <map>

#include <doctest/doctest.h>

#include <anno/formats/insel.hpp>
#include <anno/formats/sha256.hpp>
#include <anno/test/game_dir.hpp>

namespace insel = anno::formats::insel;
namespace fmt = anno::formats;

namespace {
// Hand-derived LE values: 0x1234, 0xabcd, raw 0x82/0xf3, goods 0x5678.
const fmt::ByteBuffer distinct{0x34, 0x12, 0xcd, 0xab, 0x82, 0xf3, 0x78, 0x56};

fmt::ByteBuffer record(std::string_view name, const fmt::ByteBuffer& payload) {
    fmt::ByteBuffer out(12, 0);
    for (std::size_t i = 0; i < name.size(); ++i)
        out[i] = static_cast<std::uint8_t>(name[i]);
    fmt::put_u32_le(out, static_cast<std::uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

fmt::ByteBuffer island_payload(const std::vector<fmt::ByteBuffer>& records) {
    fmt::ByteBuffer out(12, 0); // Authored INSEL5 head; no geometry validation is requested.
    for (const auto& r : records)
        out.insert(out.end(), r.begin(), r.end());
    return out;
}

void exact_distinct(const insel::HirschRecord& r) {
    CHECK(r.x == 4660);
    CHECK(r.y == 43981);
    CHECK(r.flags == 130);
    CHECK(r.raw5 == 243);
    CHECK(r.goods == 22136);
}
} // namespace

TEST_CASE("HIRSCH raw records: distinct fields and exact encoding are independent expectations") {
    auto decoded = insel::decode_hirsch(distinct);
    REQUIRE(decoded);
    REQUIRE(decoded.value().size() == 1);
    exact_distinct(decoded.value()[0]);
    CHECK(insel::encode_hirsch(decoded.value()) == distinct);
    // Writer expectation is also authored independently of the decoder.
    const std::vector<insel::HirschRecord> values{{4660, 43981, 130, 243, 22136}};
    CHECK(insel::encode_hirsch(values) == distinct);
}

TEST_CASE("HIRSCH raw records: file order extremes and owned lifetime") {
    const fmt::ByteBuffer maximum(8, 255);
    const fmt::ByteBuffer zero(8, 0);
    auto decoded = [&] {
        auto temporary = distinct;
        temporary.insert(temporary.end(), maximum.begin(), maximum.end());
        temporary.insert(temporary.end(), zero.begin(), zero.end());
        return insel::decode_hirsch(temporary);
    }(); // Input buffer is gone; typed records own their field values.
    REQUIRE(decoded);
    REQUIRE(decoded.value().size() == 3);
    exact_distinct(decoded.value()[0]);
    const insel::HirschRecord max_record{65535, 65535, 255, 255, 65535};
    CHECK(decoded.value()[1] == max_record);
    CHECK(decoded.value()[2] == insel::HirschRecord{});
    auto expected = distinct;
    expected.insert(expected.end(), maximum.begin(), maximum.end());
    expected.insert(expected.end(), zero.begin(), zero.end());
    CHECK(insel::encode_hirsch(decoded.value()) == expected);
}

TEST_CASE("HIRSCH raw records: all raw flag and byte 5 values survive") {
    for (unsigned raw = 0; raw < 256; ++raw) { // Complete on-file u8 domain, not a population cap.
        std::array<std::uint8_t, 8> bytes{0x34, 0x12, 0xcd, 0xab, 0x82, 0xf3, 0x78, 0x56};
        bytes[4] = static_cast<std::uint8_t>(raw);
        bytes[5] = static_cast<std::uint8_t>(255 - raw);
        auto decoded = insel::decode_hirsch(bytes);
        REQUIRE(decoded);
        REQUIRE(decoded.value().size() == 1);
        CHECK(decoded.value()[0].flags == raw);
        CHECK(decoded.value()[0].raw5 == 255 - raw);
        CHECK(insel::encode_hirsch(decoded.value()) == fmt::ByteBuffer(bytes.begin(), bytes.end()));
    }
}

TEST_CASE("HIRSCH accessor: absence and present-empty remain different") {
    const auto absent_bytes = island_payload({record("INFO", {1, 2})});
    auto absent = insel::Island::from_insel5(absent_bytes);
    REQUIRE(absent);
    auto missing = absent.value().hirsch_records();
    REQUIRE(missing);
    CHECK_FALSE(missing.value().has_value());
    const auto empty_bytes = island_payload({record("HIRSCH", {})});
    auto empty = insel::Island::from_insel5(empty_bytes);
    REQUIRE(empty);
    auto present = empty.value().hirsch_records();
    REQUIRE(present);
    REQUIRE(present.value().has_value());
    CHECK(present.value()->empty());
    auto decoded_empty = insel::decode_hirsch({});
    REQUIRE(decoded_empty);
    CHECK(decoded_empty.value().empty());
    CHECK(insel::encode_hirsch(decoded_empty.value()).empty());
}

TEST_CASE("HIRSCH accessor: duplicates keep first-record lookup without changing any chunk") {
    const auto bytes = record(
        "ANNO", record("INSEL5", island_payload({record("HIRSCH", distinct), record("INFO", {7}),
                                                 record("HIRSCH", {0})})));
    auto islands = insel::islands(bytes);
    REQUIRE(islands);
    REQUIRE(islands.value().size() == 1);
    const auto& island = islands.value()[0];
    REQUIRE(island.records.size() == 3);
    auto first = island.hirsch_records();
    REQUIRE(first);
    REQUIRE(first.value().has_value());
    REQUIRE(first.value()->size() == 1);
    exact_distinct(first.value()->at(0));
    CHECK(island.records[2].payload.size() == 1); // The unused duplicate is not discarded.
    auto parsed = fmt::chunk::parse_file(bytes);
    REQUIRE(parsed);
    fmt::ByteBuffer written;
    REQUIRE(fmt::chunk::write_file(parsed.value(), written));
    CHECK(written == bytes);

    const auto malformed_first =
        island_payload({record("HIRSCH", {0}), record("HIRSCH", distinct)});
    auto other = insel::Island::from_insel5(malformed_first);
    REQUIRE(other); // Island parsing does not implicitly invoke the strict accessor.
    auto refused = other.value().hirsch_records();
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == "HIRSCH payload length is not a multiple of 8");

    const auto empty_first = island_payload({record("HIRSCH", {}), record("HIRSCH", distinct)});
    auto empty = insel::Island::from_insel5(empty_first);
    REQUIRE(empty);
    auto selected = empty.value().hirsch_records();
    REQUIRE(selected);
    REQUIRE(selected.value().has_value());
    CHECK(selected.value()->empty());
}

TEST_CASE("HIRSCH accessor: partial records refuse only when explicitly decoded") {
    for (std::size_t length = 1; length < 16; ++length) {
        if (length == 8)
            continue;
        INFO("payload length " << length);
        const fmt::ByteBuffer bad(length, 0x9a);
        auto raw = insel::decode_hirsch(bad);
        REQUIRE_FALSE(raw);
        CHECK(raw.error() == "HIRSCH payload length is not a multiple of 8");
        const auto bytes = island_payload({record("HIRSCH", bad)});
        auto island = insel::Island::from_insel5(bytes);
        REQUIRE(island);
        REQUIRE(island.value().record("HIRSCH").has_value());
        CHECK(island.value().record("HIRSCH")->size() == length);
        auto strict = island.value().hirsch_records();
        REQUIRE_FALSE(strict);
        CHECK(strict.error() == raw.error());
    }
}

// Independent reference inventory: tools/wildlife_load_ref.py --corpus GAME_DIR.
// Only counts and an aggregate hash are stored here, never original record listings.
TEST_CASE("HIRSCH corpus: typed accessor matches independent original SCP and SZE inventory") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipping HIRSCH corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto scenes = *game / "Scenes";
    auto files = anno::test::files_with_ext(scenes, "scp");
    const auto sze = anno::test::files_with_ext(scenes, "sze");
    files.insert(files.end(), sze.begin(), sze.end());
    std::sort(files.begin(), files.end());
    REQUIRE_FALSE(files.empty());
    using Counts = std::map<std::string, std::size_t>;
    const Counts zero{{"files", 0},
                      {"islands", 0},
                      {"absent", 0},
                      {"empty", 0},
                      {"headers", 0},
                      {"records", 0},
                      {"selected_records", 0},
                      {"malformed", 0},
                      {"duplicate_islands", 0},
                      {"jitter_entries", 0}};
    std::map<std::string, Counts> categories;
    auto totals = zero;
    fmt::ByteBuffer canonical;
    for (const auto& file : files) {
        const auto name = file.lexically_relative(scenes).generic_string();
        INFO(name);
        const auto bytes = anno::test::read_file(file);
        auto parsed = insel::islands(bytes);
        REQUIRE(parsed);
        auto counts = zero;
        counts["files"] = 1;
        counts["islands"] = parsed.value().size();
        fmt::put_u32_le(canonical, static_cast<std::uint32_t>(name.size()));
        canonical.insert(canonical.end(), name.begin(), name.end());
        fmt::put_u32_le(canonical, static_cast<std::uint32_t>(parsed.value().size()));
        for (std::size_t index = 0; index < parsed.value().size(); ++index) {
            const auto& island = parsed.value()[index];
            std::size_t headers = 0;
            for (const auto& r : island.records) {
                if (r.name() != "HIRSCH")
                    continue;
                ++headers;
                auto decoded = insel::decode_hirsch(r.payload);
                if (!decoded) {
                    ++counts["malformed"];
                    continue;
                }
                counts["records"] += decoded.value().size();
                CHECK(insel::encode_hirsch(decoded.value()) ==
                      fmt::ByteBuffer(r.payload.begin(), r.payload.end()));
            }
            counts["headers"] += headers;
            counts["absent"] += headers == 0 ? 1U : 0U;
            counts["duplicate_islands"] += headers > 1 ? 1U : 0U;
            auto selected = island.hirsch_records();
            REQUIRE_MESSAGE(selected.ok(), (selected ? std::string{} : selected.error()));
            CHECK(selected.value().has_value() == (headers > 0));
            fmt::put_u32_le(canonical, static_cast<std::uint32_t>(index));
            canonical.push_back(selected.value().has_value() ? 1 : 0);
            const std::size_t count = selected.value() ? selected.value()->size() : 0;
            fmt::put_u32_le(canonical, static_cast<std::uint32_t>(count));
            counts["selected_records"] += count;
            if (!selected.value())
                continue;
            counts["empty"] += selected.value()->empty() ? 1U : 0U;
            for (const auto& r : *selected.value()) {
                // Canonical comparison bytes from the typed fields, NOT encode_hirsch:
                // a mutually wrong decoder/encoder pair cannot hide a field swap here.
                fmt::put_u16_le(canonical, r.x);
                fmt::put_u16_le(canonical, r.y);
                canonical.push_back(r.flags);
                canonical.push_back(r.raw5);
                fmt::put_u16_le(canonical, r.goods);
                counts["jitter_entries"] += (r.flags & 1) == 0 ? 1U : 0U;
            }
        }
        const std::string kind = file.extension() == ".scp" ? "scp" : "sze";
        auto [category, inserted] = categories.try_emplace(kind, zero);
        static_cast<void>(inserted);
        for (const auto& [key, value] : counts) {
            totals[key] += value;
            category->second[key] += value;
        }
    }
    const Counts expected{{"files", 515},
                          {"islands", 1133},
                          {"absent", 465},
                          {"empty", 530},
                          {"headers", 668},
                          {"records", 13919},
                          {"selected_records", 13919},
                          {"malformed", 0},
                          {"duplicate_islands", 0},
                          {"jitter_entries", 13919}};
    CHECK(totals == expected);
    CHECK(categories.at("scp").at("files") == 465);
    CHECK(categories.at("scp").at("absent") == 465);
    CHECK(categories.at("sze").at("files") == 50);
    CHECK(categories.at("sze").at("islands") == 668);
    CHECK(categories.at("sze").at("empty") == 530);
    CHECK(categories.at("sze").at("records") == 13919);
    const auto digest = fmt::sha256_hex(canonical);
    CHECK(digest == "f4078805e58d909226279c20b7a2f2d38b6bc3d1aeb7578906511214780f9aa9");
    for (const auto& [kind, counts] : categories) {
        std::cout << "HIRSCH_CORPUS " << kind;
        for (const auto& [key, value] : counts)
            std::cout << ' ' << key << '=' << value;
        std::cout << '\n';
    }
    std::cout << "HIRSCH_CORPUS total";
    for (const auto& [key, value] : totals)
        std::cout << ' ' << key << '=' << value;
    std::cout << " digest=" << digest << '\n';
}
