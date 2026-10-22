// The original's 0x64 group codec, decode only (docs/sim/state.md §8.5, D14).
#include <cstddef>
#include <string>

#include <anno/formats/bytes.hpp>
#include <anno/sim/legacy_codec.hpp>

namespace anno::sim::legacy {

namespace {

// Layout widths of the original record (format identifiers, not game counts).
enum : std::size_t {
    kHead = 6,      // u32 flags, u8 island, u8 city
    kGroupHead = 8, // u16 type | n << 12, u16 x, u16 y, u8 rot | rec7 << 2, u8 rec8
    kFollower = 3   // u8 rot, s8 dx, s8 dy
};

std::string at(const char* what, std::size_t offset) {
    return std::string("legacy 0x64: ") + what + " (byte " + std::to_string(offset) + ")";
}

std::int32_t signed_byte(std::uint8_t b) {
    return b < 0x80 ? std::int32_t{b} : std::int32_t{b} - 0x100;
}

} // namespace

formats::Result<BuildCmd, std::string> decode_0x64(std::span<const std::uint8_t> payload) {
    if (payload.size() < kHead) {
        return at("payload shorter than its head", payload.size());
    }
    // u32 flags at 0: dropped (F13, see the header).
    BuildCmd cmd;
    cmd.city.island = IslandId{payload[4]};
    cmd.city.city = CityNo{payload[5]};
    std::size_t pos = kHead;
    while (pos < payload.size()) {
        if (payload.size() - pos < kGroupHead) {
            return at("group head cut short", pos);
        }
        const std::uint16_t type_n = formats::u16_le(payload, pos);
        const std::uint16_t x = formats::u16_le(payload, pos + 2);
        const std::uint16_t y = formats::u16_le(payload, pos + 4);
        const std::uint8_t rot = payload[pos + 6];
        const std::uint8_t rec8 = payload[pos + 7];
        const std::size_t followers = type_n >> 12;
        PlacementWire head;
        head.type = TypeId{static_cast<std::uint32_t>(type_n & 0x0fffU)};
        head.x = x;
        head.y = y;
        head.rotation = static_cast<std::uint8_t>(rot & 3U);
        head.byte7 = static_cast<std::uint8_t>(rot >> 2);
        head.byte8 = rec8;
        pos += kGroupHead;
        if ((payload.size() - pos) / kFollower < followers) {
            return at("followers cut short", pos);
        }
        cmd.placements.push_back(head);
        for (std::size_t i = 0; i < followers; ++i) {
            PlacementWire f = head;
            f.rotation = payload[pos];
            f.x = head.x + signed_byte(payload[pos + 1]);
            f.y = head.y + signed_byte(payload[pos + 2]);
            cmd.placements.push_back(f);
            pos += kFollower;
        }
    }
    return cmd;
}

} // namespace anno::sim::legacy
