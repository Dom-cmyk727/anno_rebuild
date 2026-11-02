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
    Unencodable,
    // MAP-2 (the founding command, docs/sim/ship-founding.md section 16), appended in order.
    /// FoundCmd: the ship figure does not exist.
    UnknownFigure,
    /// The figure's type is not the ship class.
    NotAShip,
    /// The ship belongs to another player.
    NotShipOwner,
    /// The ship's state word has the rejecting bit.
    ShipDisabled,
    /// The building type is not offered for founding (Rules::founding_types, PRODTYPE 33..64).
    NotAFoundingType,
    /// The click is beyond the founding distance of the ship.
    OutOfRange,
    /// A footprint tile is owned by a city (the takeover path is not part of founding).
    SiteOwned,
    /// The issuer already has a city on the island.
    OwnCityOnIsland,
    /// No rotation passes bounds, ownership, overlap and terrain.
    NoRotationFits,
    /// A nearby figure vetoes the site.
    FigureInTheWay,
    /// A fact the figure veto needs is missing (an unknown figure type, owner or relation).
    FigureFactsMissing,
    /// The island is locked.
    IslandLocked,
    /// The island's city ring has no usable slot.
    NoCitySlot,
    // MAP-NAV-2 (MoveCmd / StopCmd, docs/sim/ship-navigation.md 12.1), appended in order.
    /// The ship is a crew child (it has a parent) and takes no orders.
    ShipEmbarked,
    // MAP-CARGO-1 (UnloadCmd, docs/sim/ship-cargo.md section 3), appended in order.
    /// UnloadCmd: the named building's type is not an eligible warehouse type.
    NotAWarehouse,
    /// UnloadCmd: the named hold does not hold the named goods, or holds none of it (amount 0).
    HoldGoodsMismatch,
    /// UnloadCmd: the goods id is 0 or outside the city's stock table.
    UnsupportedGoods,
    /// UnloadCmd: the city's store has no room left for the goods (full or over capacity).
    StockFull,
    // MAP-CARGO-2 (LoadCmd, docs/sim/ship-cargo.md section 12.2), appended in order.
    /// LoadCmd: the ship's type has no navigation facts, so no cargo hold count (no guessed limit).
    NoCargoFacts,
    /// LoadCmd: no hold of the first hold_count_a can take the goods (no same-goods hold with room
    /// and no empty hold with capacity).
    HoldsFull,
    /// LoadCmd: the city holds less than one whole ton of the goods.
    StockShort
};

/// What one record of a command did (docs/sim/state.md §8.3): the status and, for
/// a placement that was applied, the building it created.
struct RecordResult {
    Reject status = Reject::None;
    BuildingId building;
    /// MAP-CARGO-1/2: the amount an accepted UnloadCmd moved from the hold into the city store, or
    /// an accepted LoadCmd moved from the city store into a hold, in 1/256 ton
    /// (docs/sim/ship-cargo.md sections 3 and 12). 0 for every other command and every refusal.
    std::uint64_t transferred_88 = 0;
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
    case Reject::UnknownFigure:
        return "UnknownFigure";
    case Reject::NotAShip:
        return "NotAShip";
    case Reject::NotShipOwner:
        return "NotShipOwner";
    case Reject::ShipDisabled:
        return "ShipDisabled";
    case Reject::NotAFoundingType:
        return "NotAFoundingType";
    case Reject::OutOfRange:
        return "OutOfRange";
    case Reject::SiteOwned:
        return "SiteOwned";
    case Reject::OwnCityOnIsland:
        return "OwnCityOnIsland";
    case Reject::NoRotationFits:
        return "NoRotationFits";
    case Reject::FigureInTheWay:
        return "FigureInTheWay";
    case Reject::FigureFactsMissing:
        return "FigureFactsMissing";
    case Reject::IslandLocked:
        return "IslandLocked";
    case Reject::NoCitySlot:
        return "NoCitySlot";
    case Reject::ShipEmbarked:
        return "ShipEmbarked";
    case Reject::NotAWarehouse:
        return "NotAWarehouse";
    case Reject::HoldGoodsMismatch:
        return "HoldGoodsMismatch";
    case Reject::UnsupportedGoods:
        return "UnsupportedGoods";
    case Reject::StockFull:
        return "StockFull";
    case Reject::NoCargoFacts:
        return "NoCargoFacts";
    case Reject::HoldsFull:
        return "HoldsFull";
    case Reject::StockShort:
        return "StockShort";
    }
    return "?";
}

} // namespace anno::sim
