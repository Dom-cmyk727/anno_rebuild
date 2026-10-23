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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/island_links.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// What a warning reports. Data, not a limit: a new kind is added when the
/// adapter meets a new thing it does not decode.
enum class ScenarioWarningKind : std::uint8_t {
    /// A record the adapter does not map (a SCENE child, a player record or an
    /// island record), once per scenario, scope and record name.
    UnknownRecord,
    /// A STADT2 city: its live records (PROD, WARE, STAT, …) are undecoded
    /// (O-36), so the city starts default-constructed.
    UndecodedCity,
    /// A placed item (GELAENDE, FLUESSE, PROD2) whose ANLAGEN id the registry
    /// does not know.
    UnknownObject,
    /// A PROD2 item of a building type (a list in the layout) whose tile has no
    /// known city: not placed.
    BuildingWithoutCity,
    /// No decoded record gives the island's tile owners (OQ-141): every tile is
    /// neutral.
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
    /// An INSELLNK to a file that is not a *DUMMY.scp: the original loads that file
    /// as it is (D AnnoScene 0x10009fe9, docs/re/annoscene.md §3 item 3), which the
    /// adapter does not; none in the corpus.
    LinkNamesTemplate,
    /// A link that gets no island: no candidate was left (the original keeps its
    /// name empty and skips it, D 0x100084f4–0x100084fd), it comes after the ERZE
    /// record that runs the selection (D 0x1000a25c), or the scenario has no ERZE.
    LinkUnresolved,
    /// The ores assigned to a placed link: the original puts them on the island's
    /// mountain objects with one draw each (D 0x100085d5–0x10008784), which is not
    /// read far enough to map (O-37): no deposit is made, the draws are not made.
    LinkOresUnplaced,
    /// Every placed link: the original sets island property 11 = 0xF009 | the raw
    /// materials assigned to it (D 0x100085a2–0x100085cf); the sim has no field for it
    /// yet.
    LinkRawMaterials,
    /// A placed link that holds a treasure: the original creates a treasure object
    /// with up to two draws (D 0x10008b25–0x10008bb5, the spot scan 0x10008cc0 not
    /// read); not placed, not drawn.
    LinkTreasureUnplaced,
    /// SCHATZ is more than the links can hold: the original's treasure loop never
    /// ends (D 0x10009c5e–0x10009c9d); here it stops when every link holds one.
    LinkTreasuresCapped,
    /// A link's template holds SPIELER blocks (native settlements): they are not
    /// read, and the original's name draws for native cities (D 0x10008848–
    /// 0x10008b20) are not made.
    LinkTemplatePlayers,
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
    /// The INSELLNK records to *DUMMY.scp that take part in the selection (those
    /// before the ERZE record, in file order; docs/re/annoscene.md §3, §5).
    std::vector<IslandLinkSpec> links;
    /// The ROHSTOFF, SCHATZ and ERZE values as the ERZE record finds them.
    LinkInputs link_inputs;
    /// Every INSELLNK record of the file, and the islands placed from links.
    std::uint64_t links_total = 0, links_placed = 0;
};

/// What a loaded scenario holds, for the corpus table: counts only.
struct ScenarioCounts {
    std::uint64_t islands = 0, links = 0, links_resolved = 0, links_unresolved = 0, cities = 0,
                  buildings = 0, tile_objects = 0, deposits = 0, players = 0;
    std::map<ScenarioWarningKind, std::uint64_t> warnings;
};
[[nodiscard]] ScenarioCounts count(const LoadedScenario& s);

/// Reads an original scenario (root ANNO or ANNOADDON1 with one SCENE).
/// `types` resolves the ANLAGEN ids of the placed items and tells buildings (a
/// list in `config.layout`) from tile objects; `config` gives the table sizes the
/// file is checked against. Errors name the path ("players[2]: …").
formats::Result<LoadedScenario, std::string>
load_scenario(formats::Bytes file, const TypeRegistry& types, const SimConfig& config);

/// One island template the caller read (the bytes of an .scp below Scenes/Islands)
/// as a candidate of the link selection, under the caller's `name` (the upper-cased
/// path, TemplateSummary). What GetAnnoInselFileData (AnnoWelt 0x100b25a0) gives the
/// island cache: nullopt when the root is not ANNO or no INSEL4/INSEL5 child is
/// there; width and height from the head, INFO zero where the record is missing.
[[nodiscard]] std::optional<TemplateSummary> template_summary(formats::Bytes scp, std::string name);

/// Steps 2–9 of the link selection (resolve_island_links) on the scenario's own
/// generator: the draws start at the cursor World::create would start from (the
/// table, then spec.seed mod its length), and spec.seed becomes the cursor they leave,
/// so the world starts where the original's is after the ERZE record (D AnnoScene
/// 0x1000a25c: the selection draws on the world created at HEAD, docs/re/annoscene.md
/// §4). Adds the LinkTreasuresCapped warning. No link: no draw, spec.seed unchanged.
formats::Result<LinkResolution, std::string>
resolve_scenario_links(LoadedScenario& s, std::span<const TemplateSummary> candidates,
                       const SimConfig& config, const Rules& rules);

/// Step 10 (D AnnoScene 0x10008490) for every link of `s` in order: a link without a
/// template → LinkUnresolved; otherwise `templates[i]` (the .scp bytes of link i's
/// chosen candidate, read by the caller) becomes an island of slot `number & 0xFFFF`
/// at the link's position, built as an INSEL5 of the scenario is, with the link's
/// climate (island property 7, D 0x10008532–0x10008545), and the warnings for what
/// the placement does beyond that (LinkRawMaterials, LinkOresUnplaced,
/// LinkTreasureUnplaced, LinkTemplatePlayers). Errors name the link
/// ("links[3] (island 12): ...").
std::optional<std::string> place_island_links(LoadedScenario& s, const LinkResolution& res,
                                              std::span<const formats::Bytes> templates,
                                              const TypeRegistry& types, const SimConfig& config);

} // namespace anno::sim
