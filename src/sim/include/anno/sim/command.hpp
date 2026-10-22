// Commands: the only way to change a world from outside besides advance (spec:
// docs/sim/state.md §8). A Command is {issuer, body}; the body is one alternative of
// a closed variant whose order is the codec's tag order and part of the replay
// format (append only). The wire format of §8.5 is explicit little-endian with
// widened ids and is bounds-checked; it is NOT byte-compatible with the original's.
//
// Stage S8 applies SetSpeed and SetProductionLine (apply.cpp). The other bodies are
// declared, encoded and decoded now (S9's replay log needs them) and answer
// Reject::Unsupported until stage S10 gives them their rules.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/ids.hpp>

namespace anno::sim {

/// The version of the wire layout below. The replay log (S9) records it in its
/// header; a reader refuses a log written with another one.
inline constexpr std::uint32_t kCommandWireVersion = 1;

/// The original's record {u16 type, u16 x, u16 y, u8 rotation, u8 [7], u8 [8], u8 [9]
/// unused}, widened. `byte7` and `byte8` ride along for the legacy import only: the
/// original overwrites byte 8 from the terrain check and nothing reads byte 7 (§8.4).
struct PlacementWire {
    TypeId type;
    std::int32_t x = 0, y = 0;
    std::uint8_t rotation = 0;
    std::uint8_t byte7 = 0;
    std::uint8_t byte8 = 0;
    friend bool operator==(const PlacementWire&, const PlacementWire&) = default;
};

/// 0x64, the gameplay build (drag build: several placements in stream order).
struct BuildCmd {
    CityRef city;
    std::vector<PlacementWire> placements;
    friend bool operator==(const BuildCmd&, const BuildCmd&) = default;
};

/// 0x68.
struct DemolishCmd {
    CityRef city;
    std::vector<TileXY> tiles;
    friend bool operator==(const DemolishCmd&, const DemolishCmd&) = default;
};

/// 0x74: switch the production line of `goods` of the building on `tile` on or off.
struct SetProductionLineCmd {
    TileXY tile;
    GoodsId goods = 0;
    bool on = true;
    friend bool operator==(const SetProductionLineCmd&, const SetProductionLineCmd&) = default;
};

/// 0x7c.
struct SetTradeFlagCmd {
    CityRef city;
    std::uint16_t mode = 0;
    GoodsId goods = 0;
    std::uint32_t value = 0;
    friend bool operator==(const SetTradeFlagCmd&, const SetTradeFlagCmd&) = default;
};

/// `index` value that appends an entry (the original's command 0x8a).
inline constexpr std::uint32_t kAppendIndex = 0xffffffffU;

/// 0x73; index == kAppendIndex is the original's 0x8a (entry add). `list` is a u8 in
/// the original (F3).
struct SetTradeSlotCmd {
    CityRef city;
    std::uint32_t list = 0;
    std::uint32_t index = 0;
    GoodsId goods = 0;
    std::uint32_t amount_88 = 0;
    std::uint32_t price = 0;
    friend bool operator==(const SetTradeSlotCmd&, const SetTradeSlotCmd&) = default;
};

/// 0x7a, host only. The original sends an f32; the percent is an integer here.
struct SetSpeedCmd {
    std::uint32_t percent = 100;
    friend bool operator==(const SetSpeedCmd&, const SetSpeedCmd&) = default;
};

/// Decoded, logged, replayed and rejected: every record type this build does not
/// implement (ships, units, diplomacy, chat, editor operations, ...). Its type is never
/// one of the native wire types (a native type always decodes to its own body).
struct LegacyRawCmd {
    std::uint16_t type = 0;
    std::vector<std::uint8_t> payload;
    friend bool operator==(const LegacyRawCmd&, const LegacyRawCmd&) = default;
};

/// Alternative order = the codec's tag order and part of the replay format.
using Body = std::variant<BuildCmd, DemolishCmd, SetProductionLineCmd, SetTradeFlagCmd,
                          SetTradeSlotCmd, SetSpeedCmd, LegacyRawCmd>;

struct Command {
    /// Stamped by the host (single player: the local player), never read from a
    /// client's record.
    PlayerId issuer;
    Body body;
    friend bool operator==(const Command&, const Command&) = default;
};

/// The native record types of the wire format (the original's values where the meaning
/// matches, §8.5). An enumeration, not constants: they are format identifiers.
enum WireType : std::uint16_t {
    kWireBuild = 0x64,
    kWireDemolish = 0x68,
    kWireSetTradeSlot = 0x73,
    kWireSetProductionLine = 0x74,
    kWireSetSpeed = 0x7a,
    kWireSetTradeFlag = 0x7c
};

/// Whether `type` is one of the six native record types.
[[nodiscard]] constexpr bool is_native_wire_type(std::uint16_t type) {
    return type == kWireBuild || type == kWireDemolish || type == kWireSetTradeSlot ||
           type == kWireSetProductionLine || type == kWireSetSpeed || type == kWireSetTradeFlag;
}

/// The record type a body is written with (LegacyRawCmd: its own type field).
[[nodiscard]] std::uint16_t wire_type(const Body& body);

/// The longest payload a record can carry: its length field is a u16.
enum : std::size_t { kMaxRecordPayload = 0xffff };

/// ClientRecord = {u16 type, u16 len, payload[len]} (§8.5). An error (a message) for a
/// payload longer than kMaxRecordPayload, or a LegacyRawCmd whose type is native.
[[nodiscard]] formats::Result<std::vector<std::uint8_t>, std::string>
encode_record(const Body& body);

/// Whether encode_record accepts `body`, decided without allocating: false for a
/// LegacyRawCmd whose type is native and for a payload longer than kMaxRecordPayload
/// (a Build of more than 4 095 placements, a Demolish of more than 8 190 tiles). The
/// session drivers apply only such commands (replay.hpp), so a recorded and an
/// unrecorded session agree.
[[nodiscard]] bool has_wire_form(const Body& body);

/// Decodes exactly one ClientRecord. Rejects: input shorter than a header, a length
/// beyond the input, trailing bytes, a count larger than the payload can hold (checked
/// before any allocation), a native type with the wrong payload length, a flag byte
/// that is not 0 or 1 (or a reserved byte that is not 0). Total: no input throws,
/// over-reads or allocates more than a small multiple of its own size.
[[nodiscard]] formats::Result<Body, std::string> decode_record(std::span<const std::uint8_t> bytes);

/// TurnEntry = {u32 issuer, u32 seq, ClientRecord}: what the host sends to everyone
/// (§8.5). `seq` is the host's order index.
struct TurnEntry {
    Command command;
    std::uint32_t seq = 0;
    friend bool operator==(const TurnEntry&, const TurnEntry&) = default;
};
[[nodiscard]] formats::Result<std::vector<std::uint8_t>, std::string>
encode_turn_entry(const TurnEntry& entry);
[[nodiscard]] formats::Result<TurnEntry, std::string>
decode_turn_entry(std::span<const std::uint8_t> bytes);

/// The total size in bytes of the one ClientRecord at the front of `bytes`: its
/// header (4 bytes) plus the payload length its header announces. An error (a
/// message) when `bytes` is shorter than a header or than the record the header
/// claims. It only frames — it does not decode or validate the body — so that the
/// header read lives in one place (§8.5, §8.7): decode_turn and the replay-log
/// reader walk a back-to-back record stream through it instead of re-deriving the
/// framing at each call site. The wire version is NOT per record; it is carried
/// once in the replay-log header (kCommandWireVersion).
[[nodiscard]] formats::Result<std::size_t, std::string>
record_size(std::span<const std::uint8_t> bytes);

/// Decodes a back-to-back stream of TurnEntry records (`{u32 issuer, u32 seq,
/// ClientRecord}`, §8.5) that fills `bytes` exactly, appending each to `out`.
/// Framing is by record_size, so no per-record version or separator is needed.
/// Returns the number of entries appended, or an error (leaving `out` with the
/// entries decoded so far) for a truncated entry, a body that does not decode, or
/// a record that does not end on the span boundary. Total on hostile input.
[[nodiscard]] formats::Result<std::size_t, std::string>
decode_turn(std::span<const std::uint8_t> bytes, std::vector<TurnEntry>& out);

} // namespace anno::sim
