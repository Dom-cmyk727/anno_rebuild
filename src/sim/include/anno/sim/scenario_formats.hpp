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
    /// A record the adapter does not map (a SCENE child, a player record, an
    /// island record or a city record), once per scenario, scope and record name.
    UnknownRecord,
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
    // --- T26 (the city records into the state) ---
    /// A market KONTID (the market's number on its island, D AnnoWelt 0x1006be00,
    /// 0x1006bda0): the Market payload (production.hpp) has no number field and T26
    /// adds no state field, so the number is read but not kept (OQ-158). Once per
    /// scenario. The record is consumed, so it is not UndecodedBuildingRecord.
    MarketNumberNotKept,
    /// A BAUMAT entry whose goods number is not in the build-goods block
    /// (build_goods_first .. +build_goods_count-1, 0x29..0x2d): the original writes
    /// past the block (city +0x48c + 8g, no range check, D 0x1002e862); here the
    /// entry is dropped with this warning.
    BuildGoodsOutOfRange,
    /// A VK/EK trade entry whose loop index is at or beyond the list's slot count
    /// (HEAD2 [0x26]/[0x27], default 4/6; D AnnoWelt 0x1002e8e8): the original has no
    /// bound (the lists hold 8/12); here the entry is still stored and the list grown.
    TradeEntryBeyondSlots,
    /// A HEAD2 field the adapter does not map to a City field (the research
    /// sub-object [0x34]/[0x36], property [0x2c], a foreign bit in the player mask
    /// [0x30], a trade-list slot count [0x26]/[0x27] other than 4/6; D AnnoWelt
    /// 0x1002e399): no City field exists (O-40, OQ-157). Once per city that carries a
    /// non-default value there.
    Head2FieldNotMapped,
    /// A HEAD2/STAT2 city statistic (sales/trade counters, the ten tick counters):
    /// the City struct has no statistics fields (state.md §4.7 "Not modelled",
    /// O-9/O-23) and T26 adds none; read but not kept. Once per scope.
    StatisticsNotModelled,
    /// A STST2/STAT city statistics history: the rebuild has no sampled history
    /// (state.md §4.7); the original's STAT is skipped too. Once per scope.
    StatisticsHistoryNotModelled,
    /// A house record (PROD/ENTRY of a house type): its residents, stocks and the
    /// reset rule (D AnnoWelt 0x10076e60) are not applied, because the sim has no
    /// house population state (no resident/stock field on the house payload, which is
    /// Inert) and no BGruppen group data (Menge/Wohnmax; the population model is not
    /// ported (no stage in state.md §15 yet; PLAN M3 orders it after transport)), and
    /// T26 adds no state field (OQ-165). The record is read and
    /// consumed (so it is not UndecodedBuildingRecord) and its residents counted for
    /// the economy diagnostic; the s_j -> stock formula and the reset rule are
    /// implemented and unit-tested as pure functions but not applied here. Once per
    /// scenario. This subsumes the task's planned HouseResetNotModelled (the reset
    /// condition cannot be evaluated without the group's Wohnmax).
    HouseRecordNotModelled,
};

/// The stable name of a kind ("UnknownRecord", …): the corpus expectations file
/// and the tool print it.
[[nodiscard]] std::string_view warning_kind_name(ScenarioWarningKind kind);
/// Every kind in enum order (for tables).
[[nodiscard]] std::vector<ScenarioWarningKind> all_warning_kinds();

// --- The house record's population reading (T26). Pure functions, derived from
// docs/re/population.md §2/§5 and docs/re/annowelt.md "Scenario cities" (f). They
// are NOT applied to any state in T26: a house is the Inert payload with no
// population field and the sim has no BGruppen group data (Menge/Wohnmax), so the
// residents and stocks a loaded house carries have nowhere to go (HouseRecordNotModelled,
// OQ-165). They are kept and unit-tested so the formula is written down once (a spec
// oracle) and the mutation checks have a target, for the population stage to use (the
// population model is not ported (no stage in state.md §15 yet; PLAN M3 orders it after
// transport)).

/// The warehouse-stock value (8.8 units, city +0x30 + 2j) that gives satisfaction
/// byte `s` for a need of quantity `menge` at `residents` residents (D AnnoWelt
/// 0x1007702b–0x10077119: stock = trunc(Menge · s · residents · 128 / 1536); 128 =
/// 100 %, population.md §2). `menge` is the group need's Menge (stored × 40.96).
[[nodiscard]] std::uint32_t house_need_stock(std::uint32_t menge, std::uint32_t s,
                                             std::uint32_t residents);

/// The satisfaction bytes s_j of a house record's need block `need_bytes` (the bytes
/// after the 8-byte header), by `version` and need count `n` (D AnnoWelt 0x10076f34 /
/// 0x10076f69 / 0x1007702b): version 0 → none; version 1 → the SECOND byte of each
/// 2-byte pair (record byte 1 + 2j of the block); any other version → one byte per
/// need. A byte past the block end reads as 0.
[[nodiscard]] std::vector<std::uint32_t> house_satisfaction(std::uint8_t version, std::uint32_t n,
                                                            formats::Bytes need_bytes);

/// Whether a loaded house meets the reset rule (D AnnoWelt 0x10076e9e–0x10076ecb):
/// its group is above pioneers (group != 0) and its residents are below half the
/// lower group's Wohnmax. When it holds the original draws from the world sequence;
/// T26 does not draw (so the PRNG timeline cannot move, acceptance 4).
[[nodiscard]] bool house_reset_applies(std::uint32_t group, std::uint32_t residents,
                                       std::uint32_t lower_wohnmax);

/// The people name of index `i` in the player VOLK name table (D AnnoGame 0x10024c50,
/// kind 0x1c 0x10013d80; docs/re/annowelt.md "Scenario cities" (i)): GOTT 0 … BANDE
/// 13; empty for 14..19 (null in the table) and for an index out of range. The
/// inverse (name → index) is what the player reader does for a VOLK of a type that
/// takes its people from the name.
[[nodiscard]] std::string_view volk_name(std::uint32_t i);
/// The index of `name` in the table, or -1 when it is not one of the 14 names.
[[nodiscard]] std::int32_t volk_name_index(std::string_view name);

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
    /// T26 economy diagnostic (not state, not hashed): house records read, those
    /// whose [1] residents byte is non-zero, and the sum of those residents. The
    /// residents are read from the file but not applied to any state field (houses
    /// are Inert; HouseRecordNotModelled, OQ-165).
    std::uint64_t houses = 0, houses_with_residents = 0, resident_sum = 0;
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
