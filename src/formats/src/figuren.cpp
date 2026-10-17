#include <algorithm>
#include <cstdio>

#include <anno/formats/figuren.hpp>
#include <anno/formats/tex.hpp>

namespace anno::formats::figuren {

namespace {

Bytes cstr(Bytes b) {
    const auto nul = std::find(b.begin(), b.end(), std::uint8_t{0});
    return b.first(static_cast<std::size_t>(nul - b.begin()));
}

bool equals(Bytes a, std::string_view s) {
    return a.size() == s.size() &&
           std::equal(a.begin(), a.end(), s.begin(),
                      [](std::uint8_t x, char y) { return x == static_cast<std::uint8_t>(y); });
}

bool starts_with(Bytes a, std::string_view s) {
    return a.size() >= s.size() && equals(a.first(s.size()), s);
}

/// Appends a chunk with `name_field` and `payload`.
void write_record(const chunk::NameField& name_field, Bytes payload, ByteBuffer& out) {
    out.insert(out.end(), name_field.begin(), name_field.end());
    put_u32_le(out, static_cast<std::uint32_t>(payload.size()));
    put_bytes(out, payload);
}

/// `payload` as a sequence of ENTRY chunks; empty payload = no entries.
std::optional<std::vector<chunk::Chunk>> sub_entries(Bytes payload) {
    if (payload.empty()) {
        return std::vector<chunk::Chunk>{};
    }
    auto kids = chunk::parse_chunks(payload);
    if (!kids) {
        return std::nullopt;
    }
    for (const auto& k : *kids) {
        if (k.name() != "ENTRY") {
            return std::nullopt;
        }
    }
    return kids;
}

ByteBuffer entries_bytes(const std::vector<ByteBuffer>& payloads) {
    const auto field = *chunk::name_field("ENTRY");
    ByteBuffer out;
    for (const auto& p : payloads) {
        write_record(field, p, out);
    }
    return out;
}

/// Records after a prefix: an exact chunk sequence, empty allowed.
std::optional<std::vector<chunk::Chunk>> records_of(Bytes rest) {
    if (rest.empty()) {
        return std::vector<chunk::Chunk>{};
    }
    return chunk::parse_chunks(rest);
}

} // namespace

std::optional<FrameFormat> frame_format(Bytes raw) {
    const Bytes n = cstr(raw);
    static constexpr std::pair<std::string_view, FrameFormat> kNames[] = {
        {"BSH_RGBV", FrameFormat::RgbV},   {"BSH_RGBH", FrameFormat::RgbH},
        {"BSH_RGBZ", FrameFormat::RgbZ},   {"BSH_RGBAV", FrameFormat::RgbAV},
        {"BSH_RGBAZ", FrameFormat::RgbAZ}, {"BSH_ALPHA", FrameFormat::Alpha},
        {"BSH_RGBWZ", FrameFormat::RgbWZ}, {"BSH_RGBZM", FrameFormat::RgbZM},
        {"BSH_RGBAH", FrameFormat::RgbAH}};
    for (const auto& [name, f] : kNames) {
        if (equals(n, name)) {
            return f;
        }
    }
    return std::nullopt;
}

std::size_t bytes_per_pixel(FrameFormat f) {
    switch (f) {
    case FrameFormat::Alpha:
        return 0;
    case FrameFormat::RgbV:
    case FrameFormat::RgbH:
    case FrameFormat::RgbAV:
    case FrameFormat::RgbAH:
        return 1;
    default:
        return 2;
    }
}

bool has_level(FrameFormat f) {
    return f == FrameFormat::RgbAV || f == FrameFormat::RgbAZ || f == FrameFormat::RgbAH;
}

std::optional<Frame> Frame::parse(Bytes raw) {
    if (raw.size() != kFrameLen) {
        return std::nullopt;
    }
    Frame f;
    f.width = u16_le(raw, 0x00);
    f.height = u16_le(raw, 0x02);
    f.palette = u16_le(raw, 0x04);
    f.unknown_06 = u16_le(raw, 0x06);
    std::copy_n(raw.begin() + 0x08, 12, f.format_raw.begin());
    f.anchor_x = i16_le(raw, 0x14);
    f.anchor_y = i16_le(raw, 0x16);
    f.offset = u32_le(raw, 0x18);
    f.length = u32_le(raw, 0x1C);
    f.unknown_20 = u32_le(raw, 0x20);
    return f;
}

std::array<std::uint8_t, kFrameLen> Frame::to_bytes() const {
    ByteBuffer b;
    put_u16_le(b, width);
    put_u16_le(b, height);
    put_u16_le(b, palette);
    put_u16_le(b, unknown_06);
    b.insert(b.end(), format_raw.begin(), format_raw.end());
    put_u16_le(b, static_cast<std::uint16_t>(anchor_x));
    put_u16_le(b, static_cast<std::uint16_t>(anchor_y));
    put_u32_le(b, offset);
    put_u32_le(b, length);
    put_u32_le(b, unknown_20);
    std::array<std::uint8_t, kFrameLen> out{};
    std::copy(b.begin(), b.end(), out.begin());
    return out;
}

std::string Frame::format_name() const {
    const Bytes n = cstr(format_raw);
    return std::string(n.begin(), n.end());
}

Result<FrameSet, std::string> FrameSet::parse(Bytes payload) {
    if (payload.size() < 8) {
        return std::string("TEXTURES shorter than 8");
    }
    const std::uint64_t n = u32_le(payload, 4);
    if (payload.size() != 8 + n * kFrameLen) {
        return std::string("TEXTURES size is not 8 + 36 n");
    }
    FrameSet s;
    s.group = u32_le(payload, 0);
    for (std::size_t k = 0; k < n; ++k) {
        s.frames.push_back(*Frame::parse(payload.subspan(8 + k * kFrameLen, kFrameLen)));
    }
    return s;
}

ByteBuffer FrameSet::to_bytes() const {
    ByteBuffer out;
    put_u32_le(out, group);
    put_u32_le(out, static_cast<std::uint32_t>(frames.size()));
    for (const auto& f : frames) {
        const auto b = f.to_bytes();
        out.insert(out.end(), b.begin(), b.end());
    }
    return out;
}

Result<GfxSet, std::string> GfxSet::parse(Bytes payload, std::size_t prefix_len) {
    if (payload.size() < prefix_len) {
        return std::string("sprite set prefix");
    }
    auto recs = records_of(payload.subspan(prefix_len));
    if (!recs) {
        return std::string("sprite set records");
    }
    GfxSet g;
    g.prefix = payload.first(prefix_len);
    g.records = std::move(*recs);
    for (const auto& r : g.records) {
        if (r.name() == "TEXTURES") {
            auto s = FrameSet::parse(r.payload);
            if (!s) {
                return s.error();
            }
            g.frame_sets.push_back(std::move(s).value());
        } else if (r.name() == "SHADOW") {
            if (g.shadow) {
                return std::string("two SHADOW records");
            }
            auto s = GfxSet::parse(r.payload, kShadowPrefixLen);
            if (!s) {
                return s.error();
            }
            g.shadow = std::make_shared<const GfxSet>(std::move(s).value());
        }
    }
    return g;
}

ByteBuffer GfxSet::to_bytes() const {
    ByteBuffer out(prefix.begin(), prefix.end());
    std::size_t next_set = 0;
    for (const auto& r : records) {
        if (r.name() == "TEXTURES" && next_set < frame_sets.size()) {
            write_record(r.name_field, frame_sets[next_set++].to_bytes(), out);
        } else if (r.name() == "SHADOW" && shadow) {
            write_record(r.name_field, shadow->to_bytes(), out);
        } else {
            write_record(r.name_field, r.payload, out);
        }
    }
    return out;
}

Result<Entry, std::string> Entry::parse(const chunk::Chunk& c) {
    if (c.name() != "ENTRY") {
        return std::string("FIGUREN child is not ENTRY");
    }
    if (c.payload.size() < kEntryPrefixLen) {
        return std::string("ENTRY prefix");
    }
    auto recs = records_of(c.payload.subspan(kEntryPrefixLen));
    if (!recs) {
        return std::string("ENTRY records");
    }
    Entry e;
    e.offset = c.offset;
    e.flag = u32_le(c.payload, 0);
    e.id = u32_le(c.payload, 4);
    e.records = std::move(*recs);
    for (const auto& r : e.records) {
        if (r.name() == "COLORS" && !r.payload.empty() && r.payload.size() < chunk::kHeaderLen) {
            // Shorter than one chunk header: an id list (ANLAGEN).
            if (r.payload.size() % 4 != 0) {
                return std::string("COLORS id list");
            }
            e.palette_ids.clear();
            for (std::size_t o = 0; o < r.payload.size(); o += 4) {
                e.palette_ids.push_back(u32_le(r.payload, o));
            }
        } else if (r.name() == "COLORS") {
            auto ts = sub_entries(r.payload);
            if (!ts) {
                return std::string("COLORS");
            }
            for (const auto& t : *ts) {
                auto table = colors::parse_entry(t.payload);
                if (!table) {
                    return std::string("COLORS/ENTRY");
                }
                e.palettes.push_back(table.value());
            }
        } else if (r.name() == "HAEUSER") {
            auto sets = sub_entries(r.payload);
            if (!sets) {
                return std::string("HAEUSER");
            }
            for (const auto& s : *sets) {
                auto g = GfxSet::parse(s.payload, kGfxPrefixLen);
                if (!g) {
                    return g.error();
                }
                e.gfx.push_back(std::move(g).value());
            }
        }
    }
    return e;
}

std::optional<Bytes> Entry::record(std::string_view name) const {
    for (const auto& r : records) {
        if (r.name() == name) {
            return r.payload;
        }
    }
    return std::nullopt;
}

std::optional<std::string> Entry::record_str(std::string_view name) const {
    auto raw = record(name);
    if (!raw) {
        return std::nullopt;
    }
    const Bytes s = cstr(*raw);
    return std::string(s.begin(), s.end());
}

ByteBuffer Entry::to_bytes() const {
    ByteBuffer out;
    put_u32_le(out, flag);
    put_u32_le(out, id);
    for (const auto& r : records) {
        if (r.name() == "COLORS" && !palette_ids.empty()) {
            ByteBuffer ids;
            for (auto i : palette_ids) {
                put_u32_le(ids, i);
            }
            write_record(r.name_field, ids, out);
        } else if (r.name() == "COLORS") {
            std::vector<ByteBuffer> tables;
            for (const auto& t : palettes) {
                tables.push_back(t.to_bytes());
            }
            write_record(r.name_field, entries_bytes(tables), out);
        } else if (r.name() == "HAEUSER") {
            std::vector<ByteBuffer> sets;
            for (const auto& g : gfx) {
                sets.push_back(g.to_bytes());
            }
            write_record(r.name_field, entries_bytes(sets), out);
        } else {
            write_record(r.name_field, r.payload, out);
        }
    }
    return out;
}

Result<FigurenHeader, std::string> FigurenHeader::parse(Bytes buf) {
    constexpr std::size_t kH = chunk::kHeaderLen;
    if (buf.size() < 2 * kH || !starts_with(buf, std::string_view("ANNO\0", 5)) ||
        !starts_with(buf.subspan(kH), std::string_view("FIGUREN\0", 8))) {
        return std::string("no ANNO/FIGUREN root");
    }
    const std::uint64_t size = u32_le(buf, kH + chunk::kNameLen);
    const std::uint64_t body_end = 2 * kH + size;
    if (body_end > buf.size()) {
        return std::string("bad FIGUREN size or volume headers");
    }
    FigurenHeader h;
    h.root_header = buf.first(kH);
    std::copy_n(buf.begin() + kH, chunk::kNameLen, h.figuren_name.begin());
    auto kids = records_of(buf.subspan(2 * kH, size));
    if (!kids) {
        return std::string("FIGUREN records");
    }
    for (auto& c : *kids) {
        c.offset += 2 * kH;
        auto e = Entry::parse(c);
        if (!e) {
            return e.error();
        }
        h.entries.push_back(std::move(e).value());
    }
    const Bytes tail = buf.subspan(body_end);
    if (tail.size() % kVolumeHeaderLen != 0) {
        return std::string("bad FIGUREN size or volume headers");
    }
    for (std::size_t o = 0; o < tail.size(); o += kVolumeHeaderLen) {
        const Bytes v = tail.subspan(o, kVolumeHeaderLen);
        if (!starts_with(v, std::string_view("TEXTURES\0", 9))) {
            return std::string("bad volume header");
        }
        VolumeHeader vh;
        std::copy_n(v.begin(), chunk::kNameLen, vh.name_field.begin());
        vh.size = u32_le(v, 12);
        vh.group = u32_le(v, 16);
        vh.frames = u32_le(v, 20);
        h.volumes.push_back(vh);
    }
    return h;
}

ByteBuffer FigurenHeader::to_bytes() const {
    const auto field = *chunk::name_field("ENTRY");
    ByteBuffer body;
    for (const auto& e : entries) {
        write_record(field, e.to_bytes(), body);
    }
    ByteBuffer out(root_header.begin(), root_header.end());
    write_record(figuren_name, body, out);
    for (const auto& v : volumes) {
        out.insert(out.end(), v.name_field.begin(), v.name_field.end());
        put_u32_le(out, v.size);
        put_u32_le(out, v.group);
        put_u32_le(out, v.frames);
    }
    return out;
}

const Entry* FigurenHeader::entry(std::uint32_t id) const {
    for (const auto& e : entries) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

std::string volume_file_name(std::uint32_t group, std::uint32_t part) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "Figuren%u%02u.dat", group, part);
    return buf;
}

std::optional<std::vector<Piece>> locate(const std::vector<std::uint64_t>& part_sizes,
                                         std::uint64_t offset, std::uint64_t length) {
    std::uint64_t pos = offset;
    const std::uint64_t end = offset + length;
    if (end < offset) {
        return std::nullopt;
    }
    std::uint64_t base = 0;
    std::vector<Piece> out;
    for (std::size_t part = 0; part < part_sizes.size(); ++part) {
        if (pos >= end) {
            break;
        }
        const std::uint64_t size = part_sizes[part];
        if (pos < base + size) {
            const std::uint64_t stop = std::min(end, base + size);
            out.push_back({part, pos - base, stop - base});
            pos = stop;
        }
        base += size;
    }
    if (pos < end) {
        return std::nullopt;
    }
    return out;
}

namespace {

struct Decoded {
    Pixels px;
    std::size_t dropped = 0;
    std::size_t extent = 0;
};

Result<Decoded, std::string> decode(FrameFormat f, Bytes data, std::size_t width,
                                    std::size_t height, bool clip) {
    Decoded d{Pixels(width * height), 0, width};
    if (data.empty()) {
        return d;
    }
    const std::size_t bpp = bytes_per_pixel(f);
    const bool levels = has_level(f);
    std::size_t i = 0, x = 0, y = 0;
    while (true) {
        if (i >= data.size()) {
            return std::string("missing end marker");
        }
        const std::uint8_t b = data[i++];
        if (b == tex::kRleEndOfRow || b == tex::kRleEndOfImage) {
            ++y;
            x = 0;
            if (b == tex::kRleEndOfImage) {
                break;
            }
            continue;
        }
        if (i >= data.size()) {
            return std::string("truncated run");
        }
        const std::uint8_t raw = data[i++];
        const std::uint8_t level = levels ? static_cast<std::uint8_t>(raw >> 6) : 0;
        const std::size_t count = levels ? (raw & 0x3Fu) : raw;
        if (i + count * bpp > data.size()) {
            return std::string("truncated run");
        }
        const Bytes px = data.subspan(i, count * bpp);
        i += count * bpp;
        x += b;
        if (y >= height) {
            return std::string("run outside image");
        }
        for (std::size_t k = 0; k < count; ++k) {
            // BSH_RGBWZ: stored pixel x goes to column 2x + (y & 1)
            // (D: AnnoDisplay.dll 0x100161c0).
            const std::size_t col = f == FrameFormat::RgbWZ ? 2 * (x + k) + (y & 1) : x + k;
            d.extent = std::max(d.extent, col + 1);
            if (col >= width) {
                if (!clip) {
                    return std::string("run outside image");
                }
                ++d.dropped;
                continue;
            }
            Pixel p;
            if (bpp == 1) {
                p = {px[k], 0, level};
            } else if (bpp == 2) {
                p = {px[2 * k + 1], px[2 * k], level};
            }
            d.px[y * width + col] = p;
        }
        x += count;
    }
    if (y != height) {
        return std::string("row count differs from height");
    }
    if (i != data.size()) {
        return std::string("bytes after end marker");
    }
    return d;
}

} // namespace

Result<Pixels, std::string> decode_frame(FrameFormat f, Bytes data, std::size_t width,
                                         std::size_t height) {
    auto d = decode(f, data, width, height, false);
    if (!d) {
        return d.error();
    }
    return std::move(d).value().px;
}

Result<std::pair<Pixels, std::size_t>, std::string>
decode_frame_clipped(FrameFormat f, Bytes data, std::size_t width, std::size_t height) {
    auto d = decode(f, data, width, height, true);
    if (!d) {
        return d.error();
    }
    return std::pair{std::move(d).value().px, d.value().dropped};
}

Result<std::pair<Pixels, std::size_t>, std::string>
decode_frame_extended(FrameFormat f, Bytes data, std::size_t width, std::size_t height) {
    auto d = decode(f, data, width, height, true);
    if (!d) {
        return d.error();
    }
    if (d.value().dropped == 0) {
        return std::pair{std::move(d).value().px, width};
    }
    const std::size_t extent = d.value().extent;
    auto wide = decode(f, data, extent, height, false);
    if (!wide) {
        return wide.error();
    }
    return std::pair{std::move(wide).value().px, extent};
}

} // namespace anno::formats::figuren
