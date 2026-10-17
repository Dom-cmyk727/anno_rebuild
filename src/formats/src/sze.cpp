#include <algorithm>
#include <bit>

#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>

namespace anno::formats::sze {

std::optional<std::size_t> nested_prefix(std::string_view parent, std::string_view name) {
    auto in = [](std::string_view v, std::initializer_list<std::string_view> l) {
        return std::find(l.begin(), l.end(), v) != l.end();
    };
    if (parent == "SPIELER" && in(name, {"SCHIFF", "SOLDAT"})) {
        return kUnitPrefixLen;
    }
    if (parent == "SPIELER" && name == "STADT2") {
        return 6;
    }
    if (parent == "SPIELER" && in(name, {"DIPL", "FORSCH"})) {
        return 0;
    }
    if (parent == "STADT2" && name == "PROD") {
        return 0;
    }
    if (in(parent, {"SCHIFF", "SOLDAT", "ENTRY"}) && name == "PRIVAT") {
        return 0;
    }
    if (parent == "PRIVAT" && in(name, {"CREW", "ROUTE"})) {
        return 0;
    }
    if (parent == "PRIVAT" && name == "ESEL") {
        return 8;
    }
    if (parent == "CREW" && name == "ENTRY") {
        return 6;
    }
    return std::nullopt;
}

Result<IslandLink, std::string> IslandLink::parse(Bytes payload) {
    if (payload.size() < insel::kInsel5HeadLen) {
        return std::string("INSELLNK shorter than its prefix");
    }
    const Bytes path = payload.subspan(insel::kInsel5HeadLen);
    // Exactly one NUL, at the end.
    if (path.empty() || path.back() != 0 ||
        std::find(path.begin(), path.end() - 1, std::uint8_t{0}) != path.end() - 1) {
        return std::string("INSELLNK: bad string");
    }
    return IslandLink{u16_le(payload, 0), u16_le(payload, 2), u16_le(payload, 4),
                      u16_le(payload, 6), u32_le(payload, 8), path.first(path.size() - 1)};
}

ByteBuffer IslandLink::to_bytes() const {
    ByteBuffer out;
    for (auto v : {x, y, width, height}) {
        put_u16_le(out, v);
    }
    put_u32_le(out, number);
    put_bytes(out, path);
    out.push_back(0);
    return out;
}

std::optional<PlayerHead> PlayerHead::parse(Bytes raw) {
    if (raw.size() != kPlayerHeadLen) {
        return std::nullopt;
    }
    PlayerHead h;
    h.kind = u32_le(raw, 0);
    h.money = u32_le(raw, 4);
    const Bytes name = raw.subspan(0x08, 0x30);
    h.name = name.first(static_cast<std::size_t>(
        std::find(name.begin(), name.end(), std::uint8_t{0}) - name.begin()));
    std::copy_n(raw.begin() + 0x38, 16, h.id.begin());
    h.colour = raw[0x4C];
    return h;
}

Result<Unit, std::string> Unit::parse(Bytes payload, std::size_t offset) {
    if (payload.size() < kUnitPrefixLen) {
        return "unit at " + std::to_string(offset) + " shorter than its prefix";
    }
    auto recs = tagged::parse_records(payload.subspan(kUnitPrefixLen), offset + kUnitPrefixLen);
    if (!recs) {
        return recs.error();
    }
    Unit u;
    u.id = u16_le(payload, 0);
    u.figure = u16_le(payload, 2);
    u.owner = payload[4];
    u.attr = payload[5];
    u.reserved = u16_le(payload, 6);
    u.x_bits = u32_le(payload, 8);
    u.y_bits = u32_le(payload, 12);
    u.records = std::move(recs).value();
    return u;
}

float Unit::x() const {
    return std::bit_cast<float>(x_bits);
}
float Unit::y() const {
    return std::bit_cast<float>(y_bits);
}

std::optional<ByteBuffer> Unit::to_bytes() const {
    ByteBuffer out;
    put_u16_le(out, id);
    put_u16_le(out, figure);
    out.push_back(owner);
    out.push_back(attr);
    put_u16_le(out, reserved);
    put_u32_le(out, x_bits);
    put_u32_le(out, y_bits);
    if (!tagged::write_records(records, out)) {
        return std::nullopt;
    }
    return out;
}

Result<Player, std::string> Player::parse(Bytes payload, std::size_t offset) {
    if (payload.size() < kPlayerPrefixLen) {
        return "SPIELER at " + std::to_string(offset) + " shorter than its prefix";
    }
    auto recs = tagged::parse_records(payload.subspan(kPlayerPrefixLen), offset + kPlayerPrefixLen);
    if (!recs) {
        return recs.error();
    }
    return Player{u32_le(payload, 0), u32_le(payload, 4), std::move(recs).value()};
}

std::optional<ByteBuffer> Player::to_bytes() const {
    ByteBuffer out;
    put_u32_le(out, flag);
    put_u32_le(out, index);
    if (!tagged::write_records(records, out)) {
        return std::nullopt;
    }
    return out;
}

std::optional<PlayerHead> Player::head() const {
    const tagged::Record* r = tagged::find(records, "HEAD");
    return r ? PlayerHead::parse(r->payload) : std::nullopt;
}

Result<std::vector<Unit>, std::string> Player::units(std::string_view name) const {
    std::vector<Unit> out;
    for (const auto& r : records) {
        if (r.name != name) {
            continue;
        }
        auto u = Unit::parse(r.payload, r.offset + r.encoded_len() - r.payload.size());
        if (!u) {
            return u.error();
        }
        out.push_back(std::move(u).value());
    }
    return out;
}

Result<Scene, std::string> Scene::parse(Bytes buf) {
    const std::string none = "no ANNO/SCENE or ANNOADDON1/SCENE container";
    auto f = chunk::parse_file(buf);
    if (!f) {
        return none;
    }
    const chunk::Chunk& root = f.value().chunks.front();
    Scene s;
    if (root.name() == "ANNO") {
        s.edition = Edition::Base;
    } else if (root.name() == "ANNOADDON1") {
        s.edition = Edition::Addon;
    } else {
        return none;
    }
    for (const auto& c : root.children) {
        if (c.name() == "SCENE") {
            if (!c.is_container) {
                return none;
            }
            s.children = c.children;
            return s;
        }
    }
    return none;
}

std::vector<const chunk::Chunk*> Scene::chunks(std::string_view name) const {
    std::vector<const chunk::Chunk*> out;
    for (const auto& c : children) {
        if (c.name() == name) {
            out.push_back(&c);
        }
    }
    return out;
}

Result<std::vector<IslandLink>, std::string> Scene::island_links() const {
    std::vector<IslandLink> out;
    for (const chunk::Chunk* c : chunks("INSELLNK")) {
        auto l = IslandLink::parse(c->payload);
        if (!l) {
            return l.error();
        }
        out.push_back(l.value());
    }
    return out;
}

Result<std::vector<Player>, std::string> Scene::players() const {
    std::vector<Player> out;
    for (const chunk::Chunk* c : chunks("SPIELER")) {
        auto p = Player::parse(c->payload, c->offset + chunk::kHeaderLen);
        if (!p) {
            return p.error();
        }
        out.push_back(std::move(p).value());
    }
    return out;
}

} // namespace anno::formats::sze
