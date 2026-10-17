#include <algorithm>

#include <anno/formats/insel.hpp>

namespace anno::formats::insel {

Result<Block, std::string> Block::parse(Bytes payload) {
    if (payload.size() < kBlockHeaderLen) {
        return std::string("block header");
    }
    return Block{u16_le(payload, 0), u16_le(payload, 2), payload[4], payload[5],
                 payload.subspan(kBlockHeaderLen)};
}

std::array<std::uint8_t, kBlockHeaderLen> Block::header_bytes() const {
    return {static_cast<std::uint8_t>(x & 0xFF),
            static_cast<std::uint8_t>(x >> 8),
            static_cast<std::uint8_t>(y & 0xFF),
            static_cast<std::uint8_t>(y >> 8),
            width,
            height};
}

GroundCell GroundCell::from_gfx_id(std::uint16_t gfx) {
    const auto low = static_cast<std::uint8_t>(gfx & 0xFF);
    return {static_cast<std::uint8_t>(gfx >> 8), static_cast<std::uint8_t>(low >> 5),
            static_cast<std::uint8_t>((low >> 2) & 7), static_cast<std::uint8_t>(low & 3)};
}

std::uint16_t GroundCell::to_gfx_id() const {
    return static_cast<std::uint16_t>(texture << 8 | u << 5 | v << 2 | rotation);
}

std::pair<std::size_t, std::size_t> GroundCell::texel(std::size_t s, std::size_t t,
                                                      std::size_t n) const {
    std::size_t a = s, b = t;
    for (unsigned k = 0; k < rotation; ++k) {
        const std::size_t na = n - 1 - b;
        b = a;
        a = na;
    }
    auto px = [n](std::uint8_t cell, std::size_t f) {
        const std::size_t p =
            (std::size_t{cell} * n + f) * kGroundTexturePx / (kCellsPerTexture * n);
        return std::min(p, kGroundTexturePx - 1);
    };
    return {px(u, a), px(v, b)};
}

PlacedObject PlacedObject::parse(Bytes item, const Block& block) {
    const std::uint16_t id = u16_le(item, 0);
    PlacedObject o;
    o.anlage = id & 0x0FFF;
    o.rotation = static_cast<std::uint8_t>(id >> 14);
    o.reserved = static_cast<std::uint8_t>((id >> 12) & 3);
    o.state = u16_le(item, 2);
    o.x = static_cast<std::uint16_t>(block.x + item[4]);
    o.y = static_cast<std::uint16_t>(block.y + item[5]);
    return o;
}

Result<Island, std::string> Island::from_insel5(Bytes payload) {
    if (payload.size() < kInsel5HeadLen) {
        return std::string("INSEL5 head");
    }
    auto recs = chunk::parse_chunks(payload.subspan(kInsel5HeadLen));
    if (!recs) {
        return std::string("INSEL5 records");
    }
    Island i;
    i.version = Version::Insel5;
    i.head = payload.first(kInsel5HeadLen);
    i.x = u16_le(payload, 0);
    i.y = u16_le(payload, 2);
    i.width = u16_le(payload, 4);
    i.height = u16_le(payload, 6);
    i.number = u32_le(payload, 8);
    i.records = std::move(*recs);
    return i;
}

Result<Island, std::string> Island::from_insel4(const std::vector<chunk::Chunk>& kids) {
    const auto it =
        std::find_if(kids.begin(), kids.end(), [](const auto& c) { return c.name() == "INSEL4"; });
    if (it == kids.end()) {
        return std::string("no island");
    }
    if (it->payload.size() != kInsel4HeadLen) {
        return std::string("INSEL4 size");
    }
    Island i;
    i.version = Version::Insel4;
    i.head = it->payload;
    i.width = u16_le(it->payload, 0);
    i.height = u16_le(it->payload, 2);
    i.number = u32_le(it->payload, 4);
    i.records.assign(it + 1, kids.end());
    return i;
}

Result<std::vector<Block>, std::string> Island::blocks(std::string_view name) const {
    std::vector<Block> out;
    for (const auto& r : records) {
        if (r.name() != name) {
            continue;
        }
        auto b = Block::parse(r.payload);
        if (!b) {
            return b.error();
        }
        out.push_back(b.value());
    }
    return out;
}

std::optional<Bytes> Island::record(std::string_view name) const {
    for (const auto& r : records) {
        if (r.name() == name) {
            return r.payload;
        }
    }
    return std::nullopt;
}

namespace {

/// Assembles cells from blocks; every tile must be covered exactly once.
Result<std::vector<std::uint16_t>, std::string> grid(const Island& i, std::string_view name,
                                                     std::size_t cell) {
    const std::size_t w = i.width, h = i.height;
    std::vector<std::uint16_t> out(w * h, 0);
    std::vector<bool> covered(w * h, false);
    auto bs = i.blocks(name);
    if (!bs) {
        return bs.error();
    }
    for (const auto& b : bs.value()) {
        const std::size_t bx = b.x, by = b.y, bw = b.width, bh = b.height;
        if (b.data.size() != bw * bh * cell) {
            return std::string("block data size");
        }
        if (bx + bw > w || by + bh > h) {
            return std::string("block outside island");
        }
        for (std::size_t j = 0; j < bh; ++j) {
            for (std::size_t k = 0; k < bw; ++k) {
                const std::size_t t = (by + j) * w + bx + k;
                if (covered[t]) {
                    return std::string("tile covered twice");
                }
                covered[t] = true;
                const std::size_t o = (j * bw + k) * cell;
                out[t] = cell == 1 ? b.data[o] : u16_le(b.data, o);
            }
        }
    }
    if (std::find(covered.begin(), covered.end(), false) != covered.end()) {
        return std::string("tile not covered");
    }
    return out;
}

} // namespace

Result<std::vector<std::uint8_t>, std::string> Island::heights() const {
    auto g = grid(*this, "HEIGHT", 1);
    if (!g) {
        return g.error();
    }
    std::vector<std::uint8_t> out;
    out.reserve(g.value().size());
    for (auto v : g.value()) {
        out.push_back(static_cast<std::uint8_t>(v));
    }
    return out;
}

Result<std::vector<std::uint16_t>, std::string> Island::gfx_ids() const {
    return grid(*this, "GFXID", 2);
}

std::optional<Bytes> Island::fertility() const {
    auto f = record("FRUCHTBAR");
    if (!f || f->size() != std::size_t{width} * height) {
        return std::nullopt;
    }
    return f;
}

Result<std::vector<PlacedObject>, std::string> Island::objects(std::string_view name) const {
    auto bs = blocks(name);
    if (!bs) {
        return bs.error();
    }
    std::vector<PlacedObject> out;
    for (const auto& b : bs.value()) {
        if (b.data.size() % kItemLen != 0) {
            return std::string("item list length");
        }
        for (std::size_t o = 0; o < b.data.size(); o += kItemLen) {
            out.push_back(PlacedObject::parse(b.data.subspan(o, kItemLen), b));
        }
    }
    return out;
}

std::vector<ByteBuffer> encode_blocks(Bytes grid, std::size_t width, std::size_t height,
                                      std::size_t cell) {
    std::vector<ByteBuffer> out;
    for (std::size_t by = 0; by < height; by += kBlock) {
        for (std::size_t bx = 0; bx < width; bx += kBlock) {
            const std::size_t bw = std::min(kBlock, width - bx), bh = std::min(kBlock, height - by);
            const Block hdr{static_cast<std::uint16_t>(bx),
                            static_cast<std::uint16_t>(by),
                            static_cast<std::uint8_t>(bw),
                            static_cast<std::uint8_t>(bh),
                            {}};
            const auto h = hdr.header_bytes();
            ByteBuffer p(h.begin(), h.end());
            for (std::size_t j = 0; j < bh; ++j) {
                const std::size_t start = ((by + j) * width + bx) * cell;
                put_bytes(p, grid.subspan(start, bw * cell));
            }
            out.push_back(std::move(p));
        }
    }
    return out;
}

Result<std::vector<Island>, std::string> islands(Bytes buf) {
    auto f = chunk::parse_file(buf);
    if (!f || !f.value().chunks.front().is_container) {
        return std::string("no island");
    }
    const auto& kids = f.value().chunks.front().children;
    if (std::any_of(kids.begin(), kids.end(), [](const auto& c) { return c.name() == "INSEL4"; })) {
        auto i = Island::from_insel4(kids);
        if (!i) {
            return i.error();
        }
        return std::vector<Island>{std::move(i).value()};
    }
    const std::vector<chunk::Chunk>* parent = &kids;
    for (const auto& c : kids) {
        if (c.name() == "SCENE") {
            if (!c.is_container) {
                return std::string("no island");
            }
            parent = &c.children;
            break;
        }
    }
    std::vector<Island> out;
    for (const auto& c : *parent) {
        if (c.name() != "INSEL5") {
            continue;
        }
        auto i = Island::from_insel5(c.payload);
        if (!i) {
            return i.error();
        }
        out.push_back(std::move(i).value());
    }
    return out;
}

} // namespace anno::formats::insel
