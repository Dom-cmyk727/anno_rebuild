// Commands: the only way to change a world from outside besides advance (spec:
// docs/sim/state.md §8). A Command is {issuer, body}; the body is one alternative of
// a closed variant whose order is the codec's tag order and part of the replay
// format (append only). The wire format of §8.5 is explicit little-endian with
// widened ids and is bounds-checked; it is NOT byte-compatible with the original's.
//
// Stage S8 applies SetSpeed and SetProductionLine (apply.cpp), stage S10 Build,
// Demolish, SetTradeFlag and SetTradeSlot; a LegacyRawCmd is decoded, logged, replayed
// and answers Reject::Unsupported.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/freight_state.hpp>
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

/// 0xF070 (native, MAP-2): a ship founds its owner's first city on an island with a warehouse
/// (docs/sim/ship-founding.md section 16). The payer, the owner and the city are derived from the
/// authenticated issuer and the ship, never supplied. `placement.type` is the building type,
/// `placement.rotation` the cursor's initial rotation, `placement.x/y` the origin, which must equal
/// the origin derived from the click (centred on it, clamped at 0); `byte7/byte8` are ignored.
struct FoundCmd {
    FigureId ship;
    PlacementWire placement;
    std::int32_t click_x = 0, click_y = 0;
    friend bool operator==(const FoundCmd&, const FoundCmd&) = default;
};

/// 0xF071 (native, MAP-NAV-2): a ship sails to a point (docs/sim/ship-navigation.md 2, 12.1). The
/// target is a Q24.8 tile coordinate inside the world; the player is the authenticated issuer. An
/// accepted command is not a routed one: the route is planned at the pick-up point and may end in a
/// RouteFailed event. A target of (0, 0) is a valid corner, never a stop (StopCmd is separate).
struct MoveCmd {
    FigureId ship;
    std::int32_t target_x_q8 = 0, target_y_q8 = 0;
    friend bool operator==(const MoveCmd&, const MoveCmd&) = default;
};

/// 0xF072 (native, MAP-NAV-2): a ship stops (motion trim) and drops its order and ticket.
struct StopCmd {
    FigureId ship;
    friend bool operator==(const StopCmd&, const StopCmd&) = default;
};

/// 0xF073 (native, MAP-CARGO-1): a ship unloads one hold, or part of it, into the city of the named
/// warehouse (docs/sim/ship-cargo.md section 3). The issuer is the authenticated player; the
/// command carries no payer, owner, city or credit. `tons` is a whole-ton request, 0 is refused.
struct UnloadCmd {
    FigureId ship;
    BuildingId warehouse;   ///< stable (island << 32) + serial, never reused
    std::uint32_t hold = 0; ///< index into Figure::holds (the original's rec[1])
    GoodsId goods = 0;      ///< must equal the hold's goods (the original's rec[0])
    std::uint32_t tons = 0; ///< requested whole tons (the original's rec[2..3], widened)
    friend bool operator==(const UnloadCmd&, const UnloadCmd&) = default;
};

/// 0xF074 (native, MAP-CARGO-2): a ship takes goods aboard from the city of the named warehouse
/// (docs/sim/ship-cargo.md section 12). The issuer is the authenticated player; the command carries
/// no payer, owner, city, price or debit. `hold` is only the PREFERRED hold: the plan may choose
/// another (section 12.4 step 12). `goods` is the goods to take aboard (not an echo of the hold).
/// `tons` is a whole-ton request, 0 is refused.
struct LoadCmd {
    FigureId ship;
    BuildingId warehouse;   ///< stable (island << 32) + serial, never reused
    std::uint32_t hold = 0; ///< the preferred hold (the original's rec[1])
    GoodsId goods = 0;      ///< the goods to take aboard (the original's rec[0])
    std::uint32_t tons = 0; ///< requested whole tons (the original's rec[2..3], widened)
    friend bool operator==(const LoadCmd&, const LoadCmd&) = default;
};

struct AssignFreightRouteCmd {
    FigureId ship;
    std::vector<FreightStop> stops;
    friend bool operator==(const AssignFreightRouteCmd&, const AssignFreightRouteCmd&) = default;
};
struct ControlFreightRouteCmd {
    FigureId ship;
    FreightControl control = FreightControl::Suspend;
    friend bool operator==(const ControlFreightRouteCmd&, const ControlFreightRouteCmd&) = default;
};

/// Alternative order = the codec's tag order and part of the replay format.
using Body = std::variant<BuildCmd, DemolishCmd, SetProductionLineCmd, SetTradeFlagCmd,
                          SetTradeSlotCmd, SetSpeedCmd, LegacyRawCmd, FoundCmd, MoveCmd, StopCmd,
                          UnloadCmd, LoadCmd, AssignFreightRouteCmd, ControlFreightRouteCmd>;

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
    // Not an original value: the original's ship records carry the building type id in this word
    // (docs/re/ship-founding.md 8.8) and its group commands are 0x64..0x8a, so a tag far above both
    // cannot collide with a recorded original record.
    kWireFound = 0xF070,
    kWireMove = 0xF071, ///< MAP-NAV-2, same reasoning as kWireFound
    kWireStop = 0xF072,
    kWireUnload = 0xF073, ///< MAP-CARGO-1, same reasoning as kWireFound
    kWireAssignFreight = 0xF075,
    kWireControlFreight = 0xF076,
    kWireLoad = 0xF074, ///< MAP-CARGO-2, same reasoning as kWireFound
    kWireSetProductionLine = 0x74,
    kWireSetSpeed = 0x7a,
    kWireSetTradeFlag = 0x7c
};

/// Whether `type` is one of the native record types. (0xF070 became native with MAP-2; a log
/// that holds a LegacyRawCmd of that type from before is no longer representable; no original
/// record has that type and the replay fixtures hold none: docs/sim/ship-founding.md section 16.)
[[nodiscard]] constexpr bool is_native_wire_type(std::uint16_t type) {
    return type == kWireBuild || type == kWireDemolish || type == kWireSetTradeSlot ||
           type == kWireSetProductionLine || type == kWireSetSpeed || type == kWireSetTradeFlag ||
           type == kWireFound || type == kWireMove || type == kWireStop || type == kWireUnload ||
           type == kWireLoad || type == kWireAssignFreight || type == kWireControlFreight;
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
