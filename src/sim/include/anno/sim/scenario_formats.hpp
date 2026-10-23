// The scenario adapter (spec: docs/sim/state.md §10, stage S14): an original .sze
// scenario → ScenarioSpec, the only code in src/sim that reads the original
// scenario formats (formats::sze, formats::insel).
//
// Rule: what the file carries is decoded where the specs give evidence (D or P in
// docs/formats/sze.md, docs/formats/insel.md, docs/re/annoscene.md); everything
// else becomes a typed warning that names the record and its offset, never a
// default that pretends to be a reading. A refusal (the Result error) is only for a
// file that is not a scenario or whose decoded parts contradict each other.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// What a warning reports. Data, not a limit: a new kind is added when the
/// adapter meets a new thing it does not decode.
enum class ScenarioWarningKind : std::uint8_t {
    /// A record the adapter does not map (a SCENE child, a player record, an
    /// island record or a city record), once per scenario, scope and record name.
    UnknownRecord,
    /// An INSELLNK: resolved by the template choice of stage 2 (O-35); its slot
    /// stays empty.
    IslandLink,
    /// A placed item (GELAENDE, FLUESSE, PROD2) whose ANLAGEN id the registry
    /// does not know.
    UnknownObject,
    /// A PROD2 item of a building type (a list in the layout) that no city's
    /// PROD/ENTRY names: not placed. The buildings come from PROD/ENTRY
    /// (docs/re/annowelt.md "Scenario cities"); that the original keeps such an
    /// item as a tile item without a building is likely (OQ-144).
    BuildingItemWithoutEntry,
    /// A city's territory record that is not modelled: ID (radius claims through
    /// island +0xe8, D AnnoWelt 0x1002e5a5; the claim shape is not read); none in
    /// the corpus (every city has ID3).
    TileOwnersUndecoded,
    /// BERGWERK2 is undecoded (OQ-024, O-37): no deposit is made.
    UndecodedDeposits,
    /// A placed item on a tile that already holds an item: the first stays.
    ObjectTileTaken,
    /// A placed item equal to an earlier item of the same record (OQ-024).
    DuplicateObject,
    /// A STADT2 position on no INSEL5 island (the original skips such a record,
    /// D AnnoWelt 0x1001e294).
    CityOffIsland,
    /// The people number of a player whose type takes it from the VOLK name list
    /// (types 14 and 17, D AnnoWelt 0x1001d657–0x1001d753), which is not traced:
    /// the default stays.
    UndecodedPeople,
    /// A computer player (type 12) without a name: the original draws one from
    /// COMPSPIELER_NAME with the world generator (D AnnoScene 0x100025a0), a
    /// PRNG-timeline gap (docs/sim/state.md §6); not drawn here.
    ComputerNameNotDrawn,
    /// An island without INFO, or whose INFO[0] climate has no monopoly mask: the
    /// island is skipped.
    UnknownClimate,
    /// An INSEL5 whose x or y is not a multiple of 8: the original snaps the
    /// rectangle down (D AnnoWelt 0x100a253e–0x100a2586); here it stays where the head
    /// puts it (none in the corpus).
    UnalignedIsland,
    /// A PROD/ENTRY whose tile holds no PROD2 building item, skipped: no PROD2 item
    /// at all, or one whose PRODTYPE has no list in the layout (the corpus: DENKMAL
    /// 289). The original skips an entry only when the type lookup of the tile's
    /// object fails (D 0x10026599); what it creates for these is not read (OQ-144).
    EntryWithoutBuildingItem,
    /// A building create_building would refuse (outside the island, on another
    /// building, a payload that does not fit): the original creates it blindly
    /// (F32); here it is left out with the reason.
    BuildingRefused,
    /// The bytes of a building's own record that the adapter does not map (a class
    /// whose Load is not read, or a record longer than the class's buffer): the
    /// make_* default stays. Once per scenario and class.
    UndecodedBuildingRecord,
    /// A WARE entry whose goods number has no name in WARETYPE or whose name the
    /// registry's goods table does not know (the original maps by name, D AnnoWelt
    /// 0x100a18d0); the entry is skipped.
    UnknownGoods,
    /// An ID3 or ID2 entry that names tiles outside its island: the original writes
    /// them unclipped (D 0x10085620, past the island's tile array, F-fix); here
    /// they are dropped.
    OwnerTileOutsideIsland,
};

/// The stable name of a kind ("UnknownRecord", …): the corpus expectations file
/// and the tool print it.
[[nodiscard]] std::string_view warning_kind_name(ScenarioWarningKind kind);
/// Every kind in enum order (for tables).
[[nodiscard]] std::vector<ScenarioWarningKind> all_warning_kinds();

struct ScenarioWarning {
    ScenarioWarningKind kind = ScenarioWarningKind::UnknownRecord;
    /// Where, in the loader's path style: "scene.SKTIMER", "players[3].STADT2[0]",
    /// "islands[2].PROD2[17]" (the SCENE child's index for islands).
    std::string path;
    /// The record the warning is about.
    std::string record;
    /// The absolute file offset of that record's header (0 where none applies).
    std::uint64_t offset = 0;
    std::string message;
};

struct LoadedScenario {
    ScenarioSpec spec;
    std::vector<ScenarioWarning> warnings;
};

/// What a loaded scenario holds, for the corpus table: counts only.
struct ScenarioCounts {
    std::uint64_t islands = 0, links = 0, cities = 0, buildings = 0, tile_objects = 0, deposits = 0,
                  players = 0;
    std::map<ScenarioWarningKind, std::uint64_t> warnings;
};
[[nodiscard]] ScenarioCounts count(const LoadedScenario& s);

/// Reads an original scenario (root ANNO or ANNOADDON1 with one SCENE).
/// `types` resolves the ANLAGEN ids of the placed items and tells buildings (a
/// list in `config.layout`) from tile objects; `config` gives the table sizes the
/// file is checked against. Errors name the path ("players[2]: …").
formats::Result<LoadedScenario, std::string>
load_scenario(formats::Bytes file, const TypeRegistry& types, const SimConfig& config);

} // namespace anno::sim
