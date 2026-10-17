#include <anno/formats/tex.hpp>
#include <anno/formats/zei.hpp>

namespace anno::formats::zei {

Result<GlyphImage, std::string> Glyph::decode() const {
    const std::string bad = "glyph " + std::to_string(code) + " is malformed";
    struct Run {
        std::size_t row, x;
        Bytes px;
    };
    std::vector<Run> runs;
    std::size_t i = 0, x = 0, row = 0, extent = width;
    while (true) {
        if (i >= data.size()) {
            return bad;
        }
        const std::uint8_t b = data[i++];
        if (b == tex::kRleEndOfRow || b == tex::kRleEndOfImage) {
            x = 0;
            if (b == tex::kRleEndOfImage) {
                break;
            }
            ++row;
            continue;
        }
        if (i >= data.size()) {
            return bad;
        }
        const std::size_t count = data[i++];
        if (i + count > data.size()) {
            return bad;
        }
        x += b;
        runs.push_back({row, x, data.subspan(i, count)});
        i += count;
        x += count;
        if (count > 0) {
            extent = std::max(extent, x);
        }
    }
    if (i != data.size()) {
        return bad;
    }
    GlyphImage img{extent, row + 1, std::vector<std::optional<std::uint8_t>>(extent * (row + 1))};
    for (const auto& r : runs) {
        for (std::size_t k = 0; k < r.px.size(); ++k) {
            img.levels[r.row * extent + r.x + k] = r.px[k];
        }
    }
    return img;
}

Result<CharSet, std::string> CharSet::parse(Bytes buf) {
    const std::string none = "no CHARSET with HEAD and BODY";
    auto f = chunk::parse_file(buf);
    if (!f) {
        return none;
    }
    const auto& root = f.value().chunks.front();
    if (root.name() != "CHARSET" || !root.is_container || root.children.size() != 2 ||
        root.children[0].name() != "HEAD" || root.children[1].name() != "BODY") {
        return none;
    }
    const Bytes head = root.children[0].payload, body = root.children[1].payload;
    if (head.size() != kHeadLen) {
        return std::string("HEAD is not 2064 bytes");
    }
    CharSet cs;
    cs.name_fields = {root.name_field, root.children[0].name_field, root.children[1].name_field};
    cs.size_a = u16_le(head, 0);
    cs.size_b = u16_le(head, 2);
    std::copy_n(head.begin() + 4, 12, cs.reserved.begin());
    std::size_t pos = 0;
    for (unsigned code = 0; code < 256; ++code) {
        const std::size_t o = 16 + 8 * code;
        const std::uint32_t width = u32_le(head, o), len = u32_le(head, o + 4);
        if (len > body.size() - pos) {
            return std::string("glyph lengths do not match BODY");
        }
        cs.glyphs.push_back({static_cast<std::uint8_t>(code), width, body.subspan(pos, len)});
        pos += len;
    }
    if (pos != body.size()) {
        return std::string("glyph lengths do not match BODY");
    }
    return cs;
}

ByteBuffer CharSet::head_bytes() const {
    ByteBuffer out;
    put_u16_le(out, size_a);
    put_u16_le(out, size_b);
    out.insert(out.end(), reserved.begin(), reserved.end());
    for (const auto& g : glyphs) {
        put_u32_le(out, g.width);
        put_u32_le(out, static_cast<std::uint32_t>(g.data.size()));
    }
    return out;
}

ByteBuffer CharSet::body_bytes() const {
    ByteBuffer out;
    for (const auto& g : glyphs) {
        put_bytes(out, g.data);
    }
    return out;
}

ByteBuffer CharSet::to_bytes() const {
    auto part = [](const chunk::NameField& field, const ByteBuffer& payload) {
        ByteBuffer out(field.begin(), field.end());
        put_u32_le(out, static_cast<std::uint32_t>(payload.size()));
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    };
    ByteBuffer inner = part(name_fields[1], head_bytes());
    const ByteBuffer b = part(name_fields[2], body_bytes());
    inner.insert(inner.end(), b.begin(), b.end());
    return part(name_fields[0], inner);
}

} // namespace anno::formats::zei
