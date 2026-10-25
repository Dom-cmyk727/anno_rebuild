// The wire codec of the commands (docs/sim/state.md §8.5): explicit little-endian,
// widened ids, every read bounds-checked, every count checked against the bytes that
// are left before anything is allocated. No exceptions, no UB on hostile input.
#include <string>
#include <utility>

#include <anno/formats/bytes.hpp>
#include <anno/sim/command.hpp>

namespace anno::sim {

namespace {

using formats::ByteBuffer;
using formats::Bytes;

// Element and payload sizes of the records (an enumeration: layout widths of the wire
// format, not counts of the game).
enum : std::size_t {
    kHeaderSize = 4,             // u16 type, u16 len
    kCountedHead = 12,           // u32 island, u32 city, u32 n
    kPlacementSize = 16,         // u32 type, i32 x, i32 y, u8 rot, u8 b7, u8 b8, u8 0
    kTileSize = 8,               // i32 x, i32 y
    kSetProductionLineSize = 13, // i32 x, i32 y, u32 goods, u8 on
    kSetTradeFlagSize = 18,      // u32 island, u32 city, u16 mode, u32 goods, u32 value
    kSetTradeSlotSize = 28,      // 7 x u32
    kSetSpeedSize = 4            // u32 percent
};

std::string at(const char* what, std::size_t offset) {
    return std::string(what) + " (byte " + std::to_string(offset) + ")";
}

/// A cursor over the payload. Every read is preceded by the caller's exact-length test;
/// the accessors still never read beyond the span (they return 0 and drain it), so a
/// logic slip cannot turn into an over-read.
class In {
  public:
    explicit In(Bytes b) : b_(b) {}
    [[nodiscard]] std::size_t left() const { return b_.size(); }
    std::uint8_t u8() {
        if (b_.empty()) {
            return 0;
        }
        const std::uint8_t v = b_[0];
        b_ = b_.subspan(1);
        return v;
    }
    std::uint16_t u16() {
        if (b_.size() < 2) {
            b_ = Bytes{};
            return 0;
        }
        const std::uint16_t v = formats::u16_le(b_, 0);
        b_ = b_.subspan(2);
        return v;
    }
    std::uint32_t u32() {
        if (b_.size() < 4) {
            b_ = Bytes{};
            return 0;
        }
        const std::uint32_t v = formats::u32_le(b_, 0);
        b_ = b_.subspan(4);
        return v;
    }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }

  private:
    Bytes b_;
};

void put_i32(ByteBuffer& out, std::int32_t v) {
    formats::put_u32_le(out, static_cast<std::uint32_t>(v));
}

void put_city(ByteBuffer& out, const CityRef& c) {
    formats::put_u32_le(out, c.island.v);
    formats::put_u32_le(out, c.city.v);
}

CityRef get_city(In& in) {
    CityRef c;
    c.island = IslandId{in.u32()};
    c.city = CityNo{in.u32()};
    return c;
}

/// Checks `count` elements of `element` bytes against the `remaining` payload bytes
/// without forming a product that could wrap: the count must be exactly what is left.
bool count_fits(std::uint32_t count, std::size_t element, std::size_t remaining) {
    return count <= remaining / element && std::size_t{count} * element == remaining;
}

// ---- encode ------------------------------------------------------------------------

struct PayloadWriter {
    ByteBuffer out;

    void operator()(const BuildCmd& c) {
        put_city(out, c.city);
        formats::put_u32_le(out, static_cast<std::uint32_t>(c.placements.size()));
        for (const PlacementWire& p : c.placements) {
            formats::put_u32_le(out, p.type.v);
            put_i32(out, p.x);
            put_i32(out, p.y);
            out.push_back(p.rotation);
            out.push_back(p.byte7);
            out.push_back(p.byte8);
            out.push_back(0);
        }
    }
    void operator()(const DemolishCmd& c) {
        put_city(out, c.city);
        formats::put_u32_le(out, static_cast<std::uint32_t>(c.tiles.size()));
        for (const TileXY& t : c.tiles) {
            put_i32(out, t.x);
            put_i32(out, t.y);
        }
    }
    void operator()(const SetProductionLineCmd& c) {
        put_i32(out, c.tile.x);
        put_i32(out, c.tile.y);
        formats::put_u32_le(out, c.goods);
        out.push_back(c.on ? 1 : 0);
    }
    void operator()(const SetTradeFlagCmd& c) {
        put_city(out, c.city);
        formats::put_u16_le(out, c.mode);
        formats::put_u32_le(out, c.goods);
        formats::put_u32_le(out, c.value);
    }
    void operator()(const SetTradeSlotCmd& c) {
        put_city(out, c.city);
        formats::put_u32_le(out, c.list);
        formats::put_u32_le(out, c.index);
        formats::put_u32_le(out, c.goods);
        formats::put_u32_le(out, c.amount_88);
        formats::put_u32_le(out, c.price);
    }
    void operator()(const SetSpeedCmd& c) { formats::put_u32_le(out, c.percent); }
    void operator()(const LegacyRawCmd& c) { formats::put_bytes(out, c.payload); }
};

/// The payload size a body will have, formed in u64 so a huge count cannot wrap.
std::uint64_t payload_size(const Body& body) {
    struct Size {
        std::uint64_t operator()(const BuildCmd& c) const {
            return kCountedHead + std::uint64_t{c.placements.size()} * kPlacementSize;
        }
        std::uint64_t operator()(const DemolishCmd& c) const {
            return kCountedHead + std::uint64_t{c.tiles.size()} * kTileSize;
        }
        std::uint64_t operator()(const SetProductionLineCmd&) const {
            return kSetProductionLineSize;
        }
        std::uint64_t operator()(const SetTradeFlagCmd&) const { return kSetTradeFlagSize; }
        std::uint64_t operator()(const SetTradeSlotCmd&) const { return kSetTradeSlotSize; }
        std::uint64_t operator()(const SetSpeedCmd&) const { return kSetSpeedSize; }
        std::uint64_t operator()(const LegacyRawCmd& c) const { return c.payload.size(); }
    };
    return std::visit(Size{}, body);
}

// ---- decode ------------------------------------------------------------------------

formats::Result<Body, std::string> decode_build(Bytes p) {
    if (p.size() < kCountedHead) {
        return at("Build: payload shorter than its head", p.size());
    }
    In in(p);
    BuildCmd c;
    c.city = get_city(in);
    const std::uint32_t n = in.u32();
    if (!count_fits(n, kPlacementSize, in.left())) {
        return at("Build: count does not match the payload length", kCountedHead);
    }
    c.placements.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        PlacementWire w;
        w.type = TypeId{in.u32()};
        w.x = in.i32();
        w.y = in.i32();
        w.rotation = in.u8();
        w.byte7 = in.u8();
        w.byte8 = in.u8();
        if (in.u8() != 0) {
            return at("Build: reserved byte is not 0", p.size() - in.left() - 1);
        }
        c.placements.push_back(w);
    }
    return Body{std::move(c)};
}

formats::Result<Body, std::string> decode_demolish(Bytes p) {
    if (p.size() < kCountedHead) {
        return at("Demolish: payload shorter than its head", p.size());
    }
    In in(p);
    DemolishCmd c;
    c.city = get_city(in);
    const std::uint32_t n = in.u32();
    if (!count_fits(n, kTileSize, in.left())) {
        return at("Demolish: count does not match the payload length", kCountedHead);
    }
    c.tiles.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        TileXY t;
        t.x = in.i32();
        t.y = in.i32();
        c.tiles.push_back(t);
    }
    return Body{std::move(c)};
}

formats::Result<Body, std::string> decode_production_line(Bytes p) {
    if (p.size() != kSetProductionLineSize) {
        return at("SetProductionLine: wrong payload length", p.size());
    }
    In in(p);
    SetProductionLineCmd c;
    c.tile.x = in.i32();
    c.tile.y = in.i32();
    c.goods = in.u32();
    const std::uint8_t on = in.u8();
    if (on > 1) {
        return at("SetProductionLine: flag byte is not 0 or 1", kSetProductionLineSize - 1);
    }
    c.on = on == 1;
    return Body{c};
}

formats::Result<Body, std::string> decode_trade_flag(Bytes p) {
    if (p.size() != kSetTradeFlagSize) {
        return at("SetTradeFlag: wrong payload length", p.size());
    }
    In in(p);
    SetTradeFlagCmd c;
    c.city = get_city(in);
    c.mode = in.u16();
    c.goods = in.u32();
    c.value = in.u32();
    return Body{c};
}

formats::Result<Body, std::string> decode_trade_slot(Bytes p) {
    if (p.size() != kSetTradeSlotSize) {
        return at("SetTradeSlot: wrong payload length", p.size());
    }
    In in(p);
    SetTradeSlotCmd c;
    c.city = get_city(in);
    c.list = in.u32();
    c.index = in.u32();
    c.goods = in.u32();
    c.amount_88 = in.u32();
    c.price = in.u32();
    return Body{c};
}

formats::Result<Body, std::string> decode_speed(Bytes p) {
    if (p.size() != kSetSpeedSize) {
        return at("SetSpeed: wrong payload length", p.size());
    }
    In in(p);
    SetSpeedCmd c;
    c.percent = in.u32();
    return Body{c};
}

} // namespace

std::uint16_t wire_type(const Body& body) {
    struct Type {
        std::uint16_t operator()(const BuildCmd&) const { return kWireBuild; }
        std::uint16_t operator()(const DemolishCmd&) const { return kWireDemolish; }
        std::uint16_t operator()(const SetProductionLineCmd&) const {
            return kWireSetProductionLine;
        }
        std::uint16_t operator()(const SetTradeFlagCmd&) const { return kWireSetTradeFlag; }
        std::uint16_t operator()(const SetTradeSlotCmd&) const { return kWireSetTradeSlot; }
        std::uint16_t operator()(const SetSpeedCmd&) const { return kWireSetSpeed; }
        std::uint16_t operator()(const LegacyRawCmd& c) const { return c.type; }
    };
    return std::visit(Type{}, body);
}

bool has_wire_form(const Body& body) {
    if (const auto* legacy = std::get_if<LegacyRawCmd>(&body);
        legacy != nullptr && is_native_wire_type(legacy->type)) {
        return false;
    }
    return payload_size(body) <= kMaxRecordPayload;
}

formats::Result<std::vector<std::uint8_t>, std::string> encode_record(const Body& body) {
    // One rule for the recorder, the session drivers and the encoder: has_wire_form;
    // the message names which of its two refusals applies.
    if (!has_wire_form(body)) {
        const auto* legacy = std::get_if<LegacyRawCmd>(&body);
        return std::string(legacy != nullptr && is_native_wire_type(legacy->type)
                               ? "LegacyRaw: the type is a native wire type"
                               : "record payload longer than 65535 bytes");
    }
    PayloadWriter w;
    std::visit(w, body);
    ByteBuffer out;
    out.reserve(kHeaderSize + w.out.size());
    formats::put_u16_le(out, wire_type(body));
    formats::put_u16_le(out, static_cast<std::uint16_t>(w.out.size()));
    formats::put_bytes(out, w.out);
    return out;
}

formats::Result<Body, std::string> decode_record(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < kHeaderSize) {
        return at("record shorter than its header", bytes.size());
    }
    const std::uint16_t type = formats::u16_le(bytes, 0);
    const std::uint16_t len = formats::u16_le(bytes, 2);
    if (len > bytes.size() - kHeaderSize) {
        return at("record length beyond the input", bytes.size());
    }
    if (len < bytes.size() - kHeaderSize) {
        return at("trailing bytes after the record", kHeaderSize + len);
    }
    const Bytes payload = bytes.subspan(kHeaderSize, len);
    switch (type) {
    case kWireBuild:
        return decode_build(payload);
    case kWireDemolish:
        return decode_demolish(payload);
    case kWireSetProductionLine:
        return decode_production_line(payload);
    case kWireSetTradeFlag:
        return decode_trade_flag(payload);
    case kWireSetTradeSlot:
        return decode_trade_slot(payload);
    case kWireSetSpeed:
        return decode_speed(payload);
    default:
        break;
    }
    LegacyRawCmd raw;
    raw.type = type;
    raw.payload.assign(payload.begin(), payload.end());
    return Body{std::move(raw)};
}

formats::Result<std::vector<std::uint8_t>, std::string> encode_turn_entry(const TurnEntry& entry) {
    auto record = encode_record(entry.command.body);
    if (!record.ok()) {
        return record.error();
    }
    ByteBuffer out;
    out.reserve(8 + record.value().size());
    formats::put_u32_le(out, entry.command.issuer.v);
    formats::put_u32_le(out, entry.seq);
    formats::put_bytes(out, record.value());
    return out;
}

formats::Result<TurnEntry, std::string> decode_turn_entry(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 8) {
        return at("turn entry shorter than its header", bytes.size());
    }
    auto body = decode_record(bytes.subspan(8));
    if (!body.ok()) {
        return body.error();
    }
    TurnEntry e;
    e.command.issuer = PlayerId{formats::u32_le(bytes, 0)};
    e.seq = formats::u32_le(bytes, 4);
    e.command.body = std::move(body).value();
    return e;
}

formats::Result<std::size_t, std::string> record_size(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < kHeaderSize) {
        return at("record shorter than its header", bytes.size());
    }
    const std::uint16_t len = formats::u16_le(bytes, 2);
    const std::size_t total = kHeaderSize + std::size_t{len};
    if (total > bytes.size()) {
        return at("record length beyond the input", bytes.size());
    }
    return total;
}

formats::Result<std::size_t, std::string> decode_turn(std::span<const std::uint8_t> bytes,
                                                      std::vector<TurnEntry>& out) {
    // A turn entry is {u32 issuer, u32 seq, ClientRecord}: the issuer/seq header
    // (8 bytes) in front of a length-framed record. record_size frames the record,
    // so the version read and the per-record framing live in one place.
    enum : std::size_t { kEntryHeader = 8 };
    std::size_t count = 0;
    std::span<const std::uint8_t> rest = bytes;
    while (!rest.empty()) {
        if (rest.size() < kEntryHeader) {
            return at("turn entry shorter than its header", rest.size());
        }
        const auto rs = record_size(rest.subspan(kEntryHeader));
        if (!rs.ok()) {
            return rs.error();
        }
        const std::size_t entry_size = kEntryHeader + rs.value();
        auto entry = decode_turn_entry(rest.first(entry_size));
        if (!entry.ok()) {
            return entry.error();
        }
        out.push_back(std::move(entry).value());
        rest = rest.subspan(entry_size);
        ++count;
    }
    return count;
}

} // namespace anno::sim
