#include <algorithm>

#include <anno/formats/chunk.hpp>
#include <anno/formats/colors.hpp>

namespace anno::formats::colors {

namespace {
bool tag(Bytes p, std::size_t o, std::string_view t) {
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (p[o + i] != static_cast<std::uint8_t>(t[i])) {
            return false;
        }
    }
    return true;
}
} // namespace

ByteBuffer ColorTable::to_bytes() const {
    ByteBuffer out;
    put_u32_le(out, palette.flag);
    put_u32_le(out, palette.id);
    for (char c : std::string_view("BODY\0\0", 6)) {
        out.push_back(static_cast<std::uint8_t>(c));
    }
    put_u16_le(out, static_cast<std::uint16_t>(palette.entries.size()));
    put_bytes(out, palette.entries);
    for (char c : std::string_view("NAME\0\0", 6)) {
        out.push_back(static_cast<std::uint8_t>(c));
    }
    put_u16_le(out, static_cast<std::uint16_t>(name.size()));
    put_bytes(out, name);
    return out;
}

Result<ColorTable, std::string> parse_entry(Bytes p) {
    if (p.size() < kNameOffset + 1) {
        return std::string("entry too short");
    }
    if (!tag(p, 0x08, "BODY") || u16_le(p, 0x0C) != 0) {
        return std::string("no BODY record");
    }
    if (u16_le(p, 0x0E) != tex::kPaletteLen * 4) {
        return std::string("BODY length is not 1024");
    }
    if (!tag(p, 0x410, "NAME") || u16_le(p, 0x414) != 0) {
        return std::string("no NAME record");
    }
    const Bytes name = p.subspan(kNameOffset);
    if (u16_le(p, 0x416) != name.size()) {
        return std::string("NAME length does not end the entry");
    }
    const auto nul = std::find(name.begin(), name.end(), std::uint8_t{0});
    if (static_cast<std::size_t>(nul - name.begin()) != name.size() - 1) {
        return std::string("NAME is not one NUL-terminated string");
    }
    ColorTable t;
    t.palette.flag = u32_le(p, 0);
    t.palette.id = u32_le(p, 4);
    t.palette.entries = p.subspan(kPaletteOffset, tex::kPaletteLen * 4);
    t.name = name;
    return t;
}

Result<std::vector<ColorTable>, std::string> parse_anlagen(Bytes buf) {
    auto f = chunk::parse_file(buf);
    if (!f) {
        return std::string("no root chunk");
    }
    const chunk::Chunk* colors = nullptr;
    for (const auto& c : f.value().chunks.front().children) {
        if (c.name() == "COLORS") {
            colors = &c;
            break;
        }
    }
    if (colors == nullptr) {
        return std::string("no ANNO/COLORS");
    }
    std::vector<ColorTable> out;
    for (const auto& e : colors->children) {
        if (e.name() != "ENTRY") {
            return std::string("child is not ENTRY");
        }
        auto t = parse_entry(e.payload);
        if (!t) {
            return t.error();
        }
        out.push_back(t.value());
    }
    return out;
}

} // namespace anno::formats::colors
