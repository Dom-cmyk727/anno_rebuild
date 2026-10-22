// Why the simulation refused something (spec: docs/sim/state.md §8.3, stage S6).
//
// Introduced with create_building, because creation and the placement seam
// return it; RecordResult and ApplyResult are declared here too, and the commands of
// stage S8 (command.hpp) produce them. A rejected creation or command changes nothing
// (state hash before == after).
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include <anno/sim/ids.hpp>

namespace anno::sim {

/// Alternative order is part of the replay format (S9): append only.
enum class Reject : std::uint16_t {
    None = 0,
    UnknownIssuer,
    UnknownIsland,
    UnknownCity,
    NotCityOwner,
    NotHost,
    BadSpeed,
    UnknownType,
    BadRotation,
    NotABuilding,
    SerialExhausted,
    /// Decoded but not implemented: tile objects (roads, walls: PRODTYPE > 192),
    /// legacy types.
    Unsupported,
    OutOfIsland,
    TileNotOwned,
    Occupied,
    TerrainRefused,
    CannotAfford,
    NotResearched,
    AtomicAbort,
    NoBuildingAtTile,
    LineNotFound,
    BadIndex,
    /// A numeric field outside what its state allows (a trade-list index that names
    /// no entry, a given payload that does not fit the type).
    BadValue,
    /// The building list is full and cannot grow: a list of 0xFFFFFFFF elements
    /// (the slot is a u32 and kNoSlot its maximum, SlotList::grown_size). Own
    /// design (S6): unreachable in a game, but create_building must be total.
    ListFull,
    /// The command has no wire form, so a replay log cannot hold it (encode_record
    /// refuses it: a LegacyRawCmd whose type is a native wire type, or a payload longer
    /// than kMaxRecordPayload). Recorder::submit answers it without logging or applying
    /// the command, so the log stays the ground truth of the session (S9).
    Unencodable
};

/// What one record of a command did (docs/sim/state.md §8.3): the status and, for
/// a placement that was applied, the building it created.
struct RecordResult {
    Reject status = Reject::None;
    BuildingId building;
};

/// The result of World::apply (stage S8): the status of the command as a whole
/// and one RecordResult per record, in stream order.
struct ApplyResult {
    Reject status = Reject::None;
    std::vector<RecordResult> records;
    [[nodiscard]] bool ok() const { return status == Reject::None; }
};

/// The enumerator's name, for messages ("OutOfIsland"); "?" for a value that is
/// not an enumerator.
[[nodiscard]] constexpr std::string_view reject_name(Reject r) {
    switch (r) {
    case Reject::None:
        return "None";
    case Reject::UnknownIssuer:
        return "UnknownIssuer";
    case Reject::UnknownIsland:
        return "UnknownIsland";
    case Reject::UnknownCity:
        return "UnknownCity";
    case Reject::NotCityOwner:
        return "NotCityOwner";
    case Reject::NotHost:
        return "NotHost";
    case Reject::BadSpeed:
        return "BadSpeed";
    case Reject::UnknownType:
        return "UnknownType";
    case Reject::BadRotation:
        return "BadRotation";
    case Reject::NotABuilding:
        return "NotABuilding";
    case Reject::SerialExhausted:
        return "SerialExhausted";
    case Reject::Unsupported:
        return "Unsupported";
    case Reject::OutOfIsland:
        return "OutOfIsland";
    case Reject::TileNotOwned:
        return "TileNotOwned";
    case Reject::Occupied:
        return "Occupied";
    case Reject::TerrainRefused:
        return "TerrainRefused";
    case Reject::CannotAfford:
        return "CannotAfford";
    case Reject::NotResearched:
        return "NotResearched";
    case Reject::AtomicAbort:
        return "AtomicAbort";
    case Reject::NoBuildingAtTile:
        return "NoBuildingAtTile";
    case Reject::LineNotFound:
        return "LineNotFound";
    case Reject::BadIndex:
        return "BadIndex";
    case Reject::BadValue:
        return "BadValue";
    case Reject::ListFull:
        return "ListFull";
    case Reject::Unencodable:
        return "Unencodable";
    }
    return "?";
}

} // namespace anno::sim
