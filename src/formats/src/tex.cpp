#include <algorithm>

#include <anno/formats/tex.hpp>

namespace anno::formats::tex {

namespace {

/// Bytes up to the first NUL.
Bytes cstr(Bytes b) {
    const auto nul = std::find(b.begin(), b.end(), std::uint8_t{0});
    return b.first(static_cast<std::size_t>(nul - b.begin()));
}

bool equals(Bytes a, std::string_view s) {
    return a.size() == s.size() &&
           std::equal(a.begin(), a.end(), s.begin(),
                      [](std::uint8_t x, char y) { return x == static_cast<std::uint8_t>(y); });
}

/// The first sub-chunk called `name`.
const chunk::Chunk* sub(const std::vector<chunk::Chunk>& chunks, std::string_view name) {
    for (const auto& c : chunks) {
        if (c.name() == name) {
            return &c;
        }
    }
    return nullptr;
}

} // namespace

std::optional<Head> Head::parse(Bytes raw) {
    if (raw.size() != kHeadLen) {
        return std::nullopt;
    }
    Head h;
    h.width = u16_le(raw, 0x00);
    h.height = u16_le(raw, 0x02);
    h.palette_id = u16_le(raw, 0x04);
    h.unknown_06 = u16_le(raw, 0x06);
    h.unknown_08 = u16_le(raw, 0x08);
    std::copy_n(raw.begin() + 0x0A, 10, h.format_raw.begin());
    std::copy_n(raw.begin() + 0x14, 4, h.color_key.begin());
    h.unknown_18 = u16_le(raw, 0x18);
    h.unknown_1a = u16_le(raw, 0x1A);
    std::copy_n(raw.begin() + 0x1C, 8, h.unknown_1c.begin());
    return h;
}

std::array<std::uint8_t, kHeadLen> Head::to_bytes() const {
    ByteBuffer b;
    put_u16_le(b, width);
    put_u16_le(b, height);
    put_u16_le(b, palette_id);
    put_u16_le(b, unknown_06);
    put_u16_le(b, unknown_08);
    b.insert(b.end(), format_raw.begin(), format_raw.end());
    b.insert(b.end(), color_key.begin(), color_key.end());
    put_u16_le(b, unknown_18);
    put_u16_le(b, unknown_1a);
    b.insert(b.end(), unknown_1c.begin(), unknown_1c.end());
    std::array<std::uint8_t, kHeadLen> out{};
    std::copy(b.begin(), b.end(), out.begin());
    return out;
}

std::optional<PixelFormat> Head::format() const {
    const Bytes f = cstr(format_raw);
    if (equals(f, "BSH_RGB")) {
        return PixelFormat::BshRgb;
    }
    if (equals(f, "BSH_RGBH")) {
        return PixelFormat::BshRgbH;
    }
    if (equals(f, "TEX_RGB")) {
        return PixelFormat::TexRgb;
    }
    return std::nullopt;
}

Result<Pixels, std::string> Texture::decode() const {
    const std::size_t w = head.width, h = head.height;
    const auto f = head.format();
    if (!f) {
        return std::string("unknown pixel format");
    }
    if (*f == PixelFormat::TexRgb) {
        if (body.size() != w * h) {
            return std::string("raw size is not width * height");
        }
        return Pixels(body.begin(), body.end());
    }
    return decode_rle(body, w, h);
}

Result<TexFile, std::string> TexFile::parse(Bytes buf) {
    auto f = chunk::parse_file(buf);
    if (!f) {
        return std::string("no root chunk");
    }
    const chunk::Chunk& root = f.value().chunks.front();
    TexFile out;
    if (root.payload.empty()) {
        return out;
    }
    if (!root.is_container) {
        return std::string("root is not a container");
    }
    for (const auto& e : root.children) {
        const std::string name(e.name());
        if (e.payload.size() < 8) {
            return name + " shorter than its prefix";
        }
        const std::uint32_t flag = u32_le(e.payload, 0), id = u32_le(e.payload, 4);
        auto subs = chunk::parse_chunks(e.payload.subspan(8));
        if (!subs) {
            return std::string("entry body is not a chunk sequence");
        }
        if (name == "COLOR") {
            const chunk::Chunk* body = sub(*subs, "BODY");
            if (subs->size() != 1 || body == nullptr || body->payload.size() != kPaletteLen * 4) {
                return std::string("bad COLOR/BODY");
            }
            out.order.emplace_back(true, out.palettes.size());
            out.palettes.push_back(Palette{id, flag, body->payload});
        } else if (name == "TEXTURE") {
            const chunk::Chunk* head = sub(*subs, "HEAD");
            const chunk::Chunk* body = sub(*subs, "BODY");
            if (head == nullptr || head->payload.size() != kHeadLen || body == nullptr) {
                return std::string("bad TEXTURE/HEAD or BODY");
            }
            Texture t;
            t.id = id;
            t.flag = flag;
            t.head = *Head::parse(head->payload);
            t.body = body->payload;
            if (const chunk::Chunk* n = sub(*subs, "NAME")) {
                t.name = n->payload;
            }
            out.order.emplace_back(false, out.textures.size());
            out.textures.push_back(t);
        } else {
            return "unexpected chunk " + name;
        }
    }
    return out;
}

const Palette* TexFile::palette(std::uint32_t id) const {
    for (const auto& p : palettes) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

Result<Pixels, std::string> decode_rle(Bytes data, std::size_t width, std::size_t height) {
    Pixels out(width * height);
    std::size_t i = 0, x = 0, y = 0;
    while (true) {
        if (i >= data.size()) {
            return std::string("missing end marker");
        }
        const std::uint8_t b = data[i++];
        if (b == kRleEndOfRow || b == kRleEndOfImage) {
            ++y;
            x = 0;
            if (b == kRleEndOfImage) {
                break;
            }
            continue;
        }
        if (i >= data.size()) {
            return std::string("truncated run");
        }
        const std::size_t count = data[i++];
        if (i + count > data.size()) {
            return std::string("truncated run");
        }
        x += b;
        if (y >= height || x + count > width) {
            return std::string("run outside image");
        }
        for (std::size_t k = 0; k < count; ++k) {
            out[y * width + x + k] = data[i + k];
        }
        i += count;
        x += count;
    }
    if (y != height) {
        return std::string("row count differs from height");
    }
    if (i != data.size()) {
        return std::string("bytes after end marker");
    }
    return out;
}

ByteBuffer encode_rle(const Pixels& px, std::size_t width, std::size_t height) {
    ByteBuffer out;
    auto byte = [](std::size_t n) { return static_cast<std::uint8_t>(n); }; // all ≤ kRleMaxRun
    for (std::size_t y = 0; y < height; ++y) {
        const std::size_t row = y * width;
        std::size_t x = 0;
        while (x < width) {
            std::size_t gap = 0;
            while (x + gap < width && !px[row + x + gap]) {
                ++gap;
            }
            x += gap;
            std::size_t n = 0;
            while (x + n < width && px[row + x + n]) {
                ++n;
            }
            while (gap > kRleMaxRun) {
                out.push_back(byte(kRleMaxRun));
                out.push_back(0);
                gap -= kRleMaxRun;
            }
            if (n == 0) {
                out.push_back(byte(gap));
                out.push_back(0);
                break;
            }
            std::size_t skip = gap;
            while (n > 0) {
                const std::size_t count = std::min(n, kRleMaxRun - skip);
                out.push_back(byte(skip));
                out.push_back(byte(count));
                for (std::size_t k = 0; k < count; ++k) {
                    out.push_back(*px[row + x + k]);
                }
                x += count;
                n -= count;
                skip = 0;
            }
        }
        out.push_back(y + 1 < height ? kRleEndOfRow : kRleEndOfImage);
    }
    return out;
}

} // namespace anno::formats::tex
