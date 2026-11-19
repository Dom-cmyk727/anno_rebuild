// Authored records and hand-derived facts, not original Anlagen bytes.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>

#include <anno/formats/bytes.hpp>

namespace anno::test::ground_facts_fixture {
struct Row {
    std::uint32_t id, kind;
    std::uint8_t value;
    std::string_view name;
};
inline constexpr std::array<Row, 5> rows{{{104, 225, 100, "ROHSTOFF"},
                                          {105, 225, 0, "ROHSTOFF"},
                                          {16384, 225, 20, "ROHSTOFF"},
                                          {16488, 225, 60, "ROHSTOFF"},
                                          {90001, 321, 90, "FELSEN"}}};
inline void append(formats::ByteBuffer& out, const formats::ByteBuffer& bytes) {
    out.insert(out.end(), bytes.begin(), bytes.end());
}
inline formats::ByteBuffer chunk(std::string_view name, const formats::ByteBuffer& bytes) {
    formats::ByteBuffer out(12, 0);
    for (std::size_t i = 0; i < name.size(); ++i)
        out.at(i) = static_cast<std::uint8_t>(name[i]);
    formats::put_u32_le(out, static_cast<std::uint32_t>(bytes.size()));
    append(out, bytes);
    return out;
}
inline formats::ByteBuffer catalogue() {
    formats::ByteBuffer entries;
    for (const auto& row : rows) {
        formats::ByteBuffer head(56, 0);
        head.at(0) = 1;
        head.at(1) = 1;
        head.at(0x18) = 4;
        head.at(0x19) = row.value;
        head.at(0x1a) = 77;
        head.at(0x1c) = 0x29;
        formats::ByteBuffer entry;
        formats::put_u32_le(entry, 0);
        formats::put_u32_le(entry, row.id);
        append(entry, chunk("HEAD", head));
        formats::ByteBuffer name(row.name.begin(), row.name.end());
        name.push_back(0);
        append(entry, chunk("PRODTYPE", name));
        append(entries, chunk("ENTRY", entry));
    }
    auto body = chunk("COLORS", {});
    append(body, chunk("ANLAGEN", entries));
    return chunk("ANNO", body);
}
} // namespace anno::test::ground_facts_fixture
